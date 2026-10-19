/*
 * SceHdmi CEC (Sony's cec.c): the CEC main thread, logical address
 * allocation, the opcode table with what PS TV sends and answers, the
 * remote control keys and the one touch play / standby requests.
 *
 * Internal names are ours unless a comment says otherwise; Sony's come from
 * the debug records (cec.c) and the thread, timer and event flag names.
 */
#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <sysmem/cpu.h>
#include <fw.h>
#include <sysmem/debug.h>
#include <dtrace/sce/dtrace.h>
#include "hdmi_internal.h"
#include <threadmgr/threadmgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>

/* CEC logical addresses */
#define CEC_ADDR_TV			0x0
#define CEC_ADDR_PLAYBACK1		0x4
#define CEC_ADDR_PLAYBACK2		0x8
#define CEC_ADDR_PLAYBACK3		0xb
#define CEC_ADDR_BROADCAST		0xf
#define CEC_ADDR_COUNT			15	/* addresses a message can come from */

/* CEC opcodes */
#define CEC_FEATURE_ABORT		0x00
#define CEC_TEXT_VIEW_ON		0x0d
#define CEC_SET_OSD_NAME		0x47
#define CEC_ACTIVE_SOURCE		0x82
#define CEC_REPORT_PHYSICAL_ADDRESS	0x84
#define CEC_MENU_STATUS			0x8e
#define CEC_REPORT_POWER_STATUS		0x90
#define CEC_INACTIVE_SOURCE		0x9d
#define CEC_CEC_VERSION			0x9e

#define CEC_ABORT_UNRECOGNIZED_OPCODE	0
#define CEC_DEVICE_TYPE_PLAYBACK	4
#define CEC_VERSION_1_4			5
#define CEC_MENU_ACTIVATED		0
#define CEC_MENU_DEACTIVATED		1
#define CEC_POWER_ON			0

#define CEC_FRAME_SIZE			16
#define CEC_OPERAND_MAX			14

/* The main thread's event flag. */
#define CEC_EVENT_ALLOC			0x00001	/* allocate a logical address */
#define CEC_EVENT_TX_DONE		0x00002
#define CEC_EVENT_RX			0x00004
#define CEC_EVENT_ONE_TOUCH_PLAY	0x10000
#define CEC_EVENT_STANDBY		0x20000
#define CEC_EVENT_ALL			0x3000f


#define CEC_PASS_THROUGH_USEC		500000	/* a key is released when nothing follows */
#define CEC_ONE_TOUCH_PLAY_USEC		5000000
#define CEC_ONE_TOUCH_PLAY_WINDOW	8000000	/* after SceHdmiForDriver_EA486A37(1) */

/* A received message, as the opcode table's receive handlers get it. */
typedef struct CecMessage {
	SceUInt8 initiator;		/* 0x00 */
	SceUInt8 destination;		/* 0x01 */
	const SceUInt8 *frame;		/* 0x04: header, opcode, operands */
	SceUInt32 length;		/* 0x08 */
} CecMessage;

/* What a send handler sends: [0] the destination (directed opcodes), then operands. */
typedef struct CecParam {
	SceUInt8 data[5];
} CecParam;

/* One opcode SceHdmi knows. */
typedef struct CecOpcode {
	SceUInt8 opcode;		/* 0x00 */
	signed char directed;		/* 0x01: may be sent to one address */
	signed char broadcast;		/* 0x02: may be broadcast */
	SceUInt8 minLength;		/* 0x03: header and opcode included */
	int (*send)(const CecParam *param);		/* 0x04 */
	int (*receive)(const CecMessage *msg);		/* 0x08 */
} CecOpcode;

/* CecWork.status */
#define CEC_STATUS_ENABLED		0x0001	/* sceHdmiCecEnableCec */
#define CEC_STATUS_ALLOCATED		0x0002	/* a logical address is allocated */
#define CEC_STATUS_CONNECTED		0x0008	/* the sink gave a physical address */
#define CEC_STATUS_CLEARED		0x0010	/* only ever cleared */
#define CEC_STATUS_STANDBY_REQUEST	0x0020	/* the device asked for standby */
#define CEC_STATUS_ACTIVE_SOURCE	0x0040
#define CEC_STATUS_REPORTED		0x0100	/* our physical address was reported */
#define CEC_STATUS_ONE_TOUCH_COUNTED	0x1000
#define CEC_STATUS_USER_CONTROL_COUNTED	0x2000
#define CEC_STATUS_ONE_TOUCH_PENDING	0x4000

typedef struct CecWork {
	SceUID eventFlag;		/* 0x000: "SceHdmiCEC" */
	SceUInt32 status;		/* 0x004: CEC_STATUS_* */
	SceKernelSpinlock lock;		/* 0x008: guards status */
	SceUInt32 reserved;		/* 0x00c */
	SceUInt8 unrecognized[CEC_ADDR_COUNT][256];	/* 0x010: opcodes a device aborted */
	void (*addressCallback)(int event);	/* 0x f10: event 2: address allocated */
	SceUID oneTouchPlayAlarm;	/* 0xf14 */
	SceUInt64 oneTouchPlayTime;	/* 0xf18: SceHdmiForDriver_EA486A37 */
	SceUID passThroughTimer;	/* 0xf20 */
	int passThroughMode;		/* 0xf24: 1: circle enters */
	void (*standbyCallback)(int event);	/* 0xf28: event 1: standby */
	CecKeyCallback keyCallback;		/* 0xf2c */
} CecWork;

/* .data */
static int cecSupported = -1;		/* -1: not checked yet */
static SceUInt32 lastKey = CEC_KEY_NONE;
static SceUInt8 logicalAddress = CEC_ADDR_BROADCAST;

/* .bss */
#if FW_SINCE(0x04000000)
void (*txProbeHook)(const SceDtraceProbe *probe, ...);
#endif
static CecWork work;
static CecStatistics statistics;
#if FW_SINCE(0x04000000)
int txProbeEnabled;
int rxProbeEnabled;
#endif
static int handlersReady;
#if FW_SINCE(0x04000000)
void (*rxProbeHook)(const SceDtraceProbe *probe, ...);
#endif
static const CecOpcode *handlers[256];

static inline void _setStatus(SceUInt32 flag)
{
	SceKernelIntrStatus intr = SceCpuForDriver_D32ACE9E(&work.lock);

	work.status |= flag;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&work.lock, intr);
}

static inline void _clearStatus(SceUInt32 flag)
{
	SceKernelIntrStatus intr = SceCpuForDriver_D32ACE9E(&work.lock);

	work.status &= ~flag;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&work.lock, intr);
}

/* Counts an event; 0 when the counter is saturated. */
static inline int _countEvent(SceUInt32 *count)
{
	if (statistics.time == 0)
		statistics.time = sceKernelGetSystemTimeWide();
	if (*count == 0xffffffff)
		return 0;
	(*count)++;
	return 1;
}

/* Hands a key to the key callback when it changed. */
static inline void _notifyKey(CecKey *key)
{
	if (lastKey != (key->code | key->flags) && work.keyCallback != NULL)
		work.keyCallback(key);
	lastKey = key->code | key->flags;
}

/* CEC is there on a PS TV (Dolce) outside safe and update mode. */
static inline int _checkCecSupported(void)
{
	if (cecSupported == -1) {
		if (SceSysrootForDriver_834439A7() != 0 || SceSysrootForDriver_B0E1FC67() != 0) {
			cecSupported = 0;
			return SCE_HDMI_ERROR_CEC_UNSUPPORTED;
		}
		if (SceSblAIMgrForDriver_71608CA3() != 0 &&
		    (SceQafMgrForDriver_382C71E8() != 0 || SceSblAIMgrForDriver_C6E83F34() != 0)) {
			cecSupported = 1;
		} else {
			cecSupported = 0;
			return SCE_HDMI_ERROR_CEC_UNSUPPORTED;
		}
	} else if (cecSupported == 0) {
		return SCE_HDMI_ERROR_CEC_UNSUPPORTED;
	}
	return 0;
}

/* <Report Power Status> received. */
int cecRecvReportPowerStatus(const CecMessage *msg)
{
	return 0;
}

int cecDevCallbackNop1(void)
{
	return 0;
}

int cecDevCallbackNop2(void)
{
	return 0;
}

int cecDevCallbackNop3(void)
{
	return 0;
}

int cecDevCallbackIsEnabled(void)
{
	return -(work.status & CEC_STATUS_ENABLED);
}

int cecDevCallbackRx(void)
{
	sceKernelSetEventFlag(work.eventFlag, CEC_EVENT_RX);
	return 0;
}

int cecDevCallbackTxDone(void)
{
	sceKernelSetEventFlag(work.eventFlag, CEC_EVENT_TX_DONE);
	return 0;
}

/* <Standby> received. */
static int cecRecvStandby(const CecMessage *msg)
{
	if (work.standbyCallback != NULL)
		work.standbyCallback(1);
	_countEvent(&statistics.standby);
	return 0;
}

/* <Feature Abort> received: remember the opcodes a device does not know. Sony's name. */
static int _setRecvUnrecognizedOpcode(const CecMessage *msg)
{
	SceUInt8 reason = msg->frame[3];
	SceUInt8 opcode = msg->frame[2];

	if (reason == CEC_ABORT_UNRECOGNIZED_OPCODE) {
		if (msg->initiator >= CEC_ADDR_COUNT)
			SCE_PANIC(2477);
		work.unrecognized[msg->initiator][opcode] = 1;
	}
	return 0;
}

/* Sony's name. */
static inline int _isAlreadyRecvUnrecognizedOpcode(SceUInt8 dest, SceUInt8 opcode)
{
	if (dest >= CEC_ADDR_COUNT)
		SCE_PANIC_FUNC(2469, "_isAlreadyRecvUnrecognizedOpcode");
	return work.unrecognized[dest][opcode];
}

#if FW_SINCE(0x04000000)
static const SceDtraceProbe txProbe;
#endif

/* Sends one message. Sony's name. */
int _txOpecode(SceUInt8 dest, SceUInt8 opcode, const SceUInt8 *operand, SceUInt32 length)
{
	SceUInt8 address[8];
	SceUInt8 frame[CEC_FRAME_SIZE] = { 0 };
	int ret;

	if (length > CEC_OPERAND_MAX) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 2314);
		return -1;
	}
	ret = cecDevGetLogicalAddress(address);
	if (ret < 0)
		return ret;
	if (address[0] == CEC_ADDR_BROADCAST)
		return -1;
	if (dest != CEC_ADDR_BROADCAST && _isAlreadyRecvUnrecognizedOpcode(dest, opcode))
		return -1;
	frame[0] = (address[0] << 4) | dest;
	frame[1] = opcode;
	if (length != 0 && operand != NULL)
		memcpy(&frame[2], operand, length != 0 ? length : 1);
	ret = cecDevTx(frame, length + 2);
	if (ret < 0)
		return ret;
	if (ret == 1 || ret == 2)
		return -1;
#if FW_SINCE(0x04000000)
	if (txProbeHook != NULL)
		txProbeHook(&txProbe, address[0], dest, opcode, frame[2], frame[3], frame[4],
			    frame[5], frame[6], frame[7], frame[8]);
#endif
	return 0;
}

static inline int cecSendReportPhysicalAddress(const CecParam *param)
{
	SceUInt8 operand[3];

	operand[0] = param->data[0];
	operand[1] = param->data[1];
	operand[2] = param->data[2];
	return _txOpecode(CEC_ADDR_BROADCAST, CEC_REPORT_PHYSICAL_ADDRESS, operand, 3);
}

static inline int cecSendFeatureAbort(const CecParam *param)
{
	SceUInt8 operand[2];

	operand[0] = param->data[1];
	operand[1] = param->data[2];
	return _txOpecode(param->data[0], CEC_FEATURE_ABORT, operand, 2);
}

static inline int cecSendReportPowerStatus(const CecParam *param)
{
	SceUInt8 operand[1];

	operand[0] = param->data[1];
	return _txOpecode(param->data[0], CEC_REPORT_POWER_STATUS, operand, 1);
}

static inline int cecSendGiveDevicePowerStatus(const CecParam *param)
{
	return _txOpecode(param->data[0], 0x8f, NULL, 0);
}

static inline int cecSendMenuStatus(const CecParam *param)
{
	SceUInt8 operand[1];

	operand[0] = param->data[1];
	return _txOpecode(param->data[0], CEC_MENU_STATUS, operand, 1);
}

static inline int cecSendSetOsdName(const CecParam *param)
{
	SceUInt8 name[5] = "PS TV";

	return _txOpecode(param->data[0], CEC_SET_OSD_NAME, name, sizeof(name));
}

static inline int cecSendCecVersion(const CecParam *param)
{
	SceUInt8 operand[1];

	operand[0] = param->data[1];
	return _txOpecode(param->data[0], CEC_CEC_VERSION, operand, 1);
}

static inline int cecSendInactiveSource(const CecParam *param)
{
	SceUInt8 physical[2];
	int ret;

	ret = cecDevGetPhysicalAddress(physical);
	if (ret < 0)
		return ret;
	if (physical[0] == 0xff && physical[1] == 0xff)
		return -1;
	return _txOpecode(param->data[0], CEC_INACTIVE_SOURCE, physical, 2);
}

static inline int cecSendTextViewOn(const CecParam *param)
{
	return _txOpecode(param->data[0], CEC_TEXT_VIEW_ON, NULL, 0);
}

static inline int cecSendActiveSource(const CecParam *param)
{
	SceUInt8 physical[2];
	int ret;

	ret = cecDevGetPhysicalAddress(physical);
	if (ret < 0)
		return ret;
	if (physical[0] == 0xff && physical[1] == 0xff)
		return -1;
	return _txOpecode(CEC_ADDR_BROADCAST, CEC_ACTIVE_SOURCE, physical, 2);
}

static const CecOpcode opcodes[];

/* The opcode table's entries this file sends itself. */
#define OP_ACTIVE_SOURCE		0
#define OP_TEXT_VIEW_ON			2
#define OP_INACTIVE_SOURCE		3
#define OP_REPORT_PHYSICAL_ADDRESS	26
#define OP_CEC_VERSION			22
#define OP_SET_OSD_NAME			45
#define OP_MENU_STATUS			48
#define OP_REPORT_POWER_STATUS		52
#define OP_FEATURE_ABORT		53

static inline int _send(int op, const CecParam *param)
{
	return opcodes[op].send(param);
}

/* <Give Device Power Status> received. */
static int cecRecvGiveDevicePowerStatus(const CecMessage *msg)
{
	CecParam param = { { 0 } };
	int ret;

	param.data[0] = msg->initiator;
	param.data[1] = CEC_POWER_ON;
	ret = _send(OP_REPORT_POWER_STATUS, &param);
	if (ret < 0)
		return ret;
	return 0;
}

/* <Menu Request> received. */
static int cecRecvMenuRequest(const CecMessage *msg)
{
	CecParam param = { { 0 } };
	int ret;

	param.data[0] = msg->initiator;
	param.data[1] = CEC_MENU_ACTIVATED;
	ret = _send(OP_MENU_STATUS, &param);
	if (ret < 0)
		return ret;
	return 0;
}

/* <Get CEC Version> received. */
static int cecRecvGetCecVersion(const CecMessage *msg)
{
	CecParam param = { { 0 } };

	param.data[0] = msg->initiator;
	param.data[1] = CEC_VERSION_1_4;
	_send(OP_CEC_VERSION, &param);
	return 0;
}

/* <Give OSD Name> received. */
static int cecRecvGiveOsdName(const CecMessage *msg)
{
	CecParam param = { { 0 } };
	int ret;

	param.data[0] = msg->initiator;
	ret = _send(OP_SET_OSD_NAME, &param);
	if (ret < 0)
		return ret;
	return 0;
}

/* <Abort> received: answer with <Feature Abort>. */
static int cecRecvAbort(const CecMessage *msg)
{
	CecParam param = { { 0 } };

	if (msg->destination == CEC_ADDR_BROADCAST)
		return 0;
	param.data[0] = msg->initiator;
	param.data[1] = 0xff;
	param.data[2] = CEC_ABORT_UNRECOGNIZED_OPCODE;
	return _send(OP_FEATURE_ABORT, &param);
}

/* No key for CEC_PASS_THROUGH_USEC: the key is released. */
static SceUInt32 passThroughTimerHandler(void *arg)
{
	CecKey key;

	work.passThroughTimer = 0;
	key.buttons = 0;
	key.code = CEC_KEY_NONE;
	key.flags = 0;
	_notifyKey(&key);
	return 0;
}

/* <User Control Released> received. */
static int cecRecvUserControlReleased(const CecMessage *msg)
{
	CecKey key;

	if (work.passThroughTimer != 0) {
		sceKernelCancelAlarm(work.passThroughTimer);
		work.passThroughTimer = 0;
	}
	key.buttons = 0;
	key.code = CEC_KEY_NONE;
	key.flags = 0;
	_notifyKey(&key);
	return 0;
}

/* <Give Physical Address> received. */
static int cecRecvGivePhysicalAddress(const CecMessage *msg)
{
	SceUInt8 address[8];
	SceUInt8 physical[8];
	CecParam param = { { 0 } };
	int ret;

	ret = cecDevGetLogicalAddress(address);
	if (ret < 0)
		return ret;
	if (address[0] == CEC_ADDR_BROADCAST)
		return -1;
	ret = cecDevGetPhysicalAddress(physical);
	if (ret < 0)
		return ret;
	if (physical[0] == 0xff && physical[1] == 0xff)
		return -1;
	param.data[0] = physical[0];
	param.data[1] = physical[1];
	param.data[2] = CEC_DEVICE_TYPE_PLAYBACK;
	ret = _send(OP_REPORT_PHYSICAL_ADDRESS, &param);
	if (ret < 0)
		return ret;
	return 0;
}

/* The sink gave a physical address: allocate a logical address. */
int cecDevCallbackConnected(void)
{
	_setStatus(CEC_STATUS_CONNECTED);
	if ((work.status & (CEC_STATUS_ENABLED | CEC_STATUS_CONNECTED)) ==
	    (CEC_STATUS_ENABLED | CEC_STATUS_CONNECTED)) {
		if (!(work.status & CEC_STATUS_REPORTED))
			sceKernelSetEventFlag(work.eventFlag, CEC_EVENT_ALLOC);
	}
	return 0;
}

int cecDevCallbackStandby(void)
{
	if (work.status & CEC_STATUS_STANDBY_REQUEST)
		return 0;
	_setStatus(CEC_STATUS_STANDBY_REQUEST);
	if (work.status & CEC_STATUS_ENABLED)
		sceKernelSetEventFlag(work.eventFlag, CEC_EVENT_STANDBY);
	return 0;
}

int cecDevCallbackStandbyDone(void)
{
	_clearStatus(CEC_STATUS_STANDBY_REQUEST);
	return 0;
}

static SceUInt32 oneTouchPlayAlarmHandler(void *arg)
{
	work.oneTouchPlayAlarm = 0;
	_clearStatus(CEC_STATUS_ONE_TOUCH_PENDING);
	return 0;
}

/* <Request Active Source> received. */
static int cecRecvRequestActiveSource(const CecMessage *msg)
{
	CecParam param;

	if (!(work.status & CEC_STATUS_ACTIVE_SOURCE))
		return 0;
	return _send(OP_ACTIVE_SOURCE, &param);
}

/* <Active Source> received: another device took over. */
static int cecRecvActiveSource(const CecMessage *msg)
{
	if (work.status & CEC_STATUS_ACTIVE_SOURCE)
		_clearStatus(CEC_STATUS_ACTIVE_SOURCE);
	return 0;
}

/* The sink is gone. */
int cecDevCallbackDisconnected(void)
{
	_clearStatus(CEC_STATUS_CONNECTED);
	_clearStatus(CEC_STATUS_REPORTED);
	return 0;
}

/* <User Control Pressed> received. Sony's name. */
static int _recvUserControlPressed(const CecMessage *msg)
{
	CecKey key;
	SceUInt32 enter, back;
	SceUID timer;

	if (work.passThroughTimer != 0) {
		sceKernelCancelAlarm(work.passThroughTimer);
		work.passThroughTimer = 0;
	}
	timer = SceThreadmgrForDriver_C58DF384("SceHdmiCecPassThrough", CEC_PASS_THROUGH_USEC,
					passThroughTimerHandler, NULL);
	if (timer < 0) {
		SCE_DBG_ASSERT(0, 1565);
		return timer;
	}
	work.passThroughTimer = timer;
	key.buttons = 0;
	key.code = CEC_KEY_NONE;
	key.flags = 0;
	if (work.passThroughMode == 1) {
		enter = 0x4000;
		back = 0x2000;
	} else {
		enter = 0x2000;
		back = 0x4000;
	}
	switch (msg->frame[2]) {
	case 0x00: case 0x2b:		/* select, enter */
		key.buttons = enter;
		key.code = 0x0b;
		break;
	case 0x01:			/* up */
		key.buttons = 0x10;
		key.code = 0x54;
		break;
	case 0x02:			/* down */
		key.buttons = 0x40;
		key.code = 0x56;
		break;
	case 0x03:			/* left */
		key.buttons = 0x80;
		key.code = 0x57;
		break;
	case 0x04:			/* right */
		key.buttons = 0x20;
		key.code = 0x55;
		break;
	case 0x09:			/* root menu */
		key.buttons = 0x10000;
		key.code = 0x43;
		break;
	case 0x0a:			/* setup menu */
		key.buttons = 0x1000;
		key.code = 0x5c;
		break;
	case 0x0b:			/* contents menu */
		key.buttons = 0x8000;
		key.code = 0x5f;
		break;
	case 0x0d: case 0x2c:		/* exit, clear */
		key.buttons = back;
		key.code = 0x0e;
		break;
	case 0x10: key.code = 0x1a; break;
	case 0x11: key.code = 0x40; break;
	case 0x1e: key.code = 0x11e; break;
	case 0x1f: key.code = 0x11f; break;
	case 0x20: key.code = 0x09; break;
	case 0x21: key.code = 0x00; break;
	case 0x22: key.code = 0x01; break;
	case 0x23: key.code = 0x02; break;
	case 0x24: key.code = 0x03; break;
	case 0x25: key.code = 0x04; break;
	case 0x26: key.code = 0x05; break;
	case 0x27: key.code = 0x06; break;
	case 0x28: key.code = 0x07; break;
	case 0x29: key.code = 0x08; break;
	case 0x2a: key.code = 0x12a; break;
	case 0x30: key.code = 0x10; break;
	case 0x31: key.code = 0x11; break;
	case 0x32: key.code = 0x132; break;
	case 0x33: key.code = 0x64; break;
	case 0x35: key.code = 0x70; break;
	case 0x44: key.code = 0x32; break;
	case 0x45: key.code = 0x38; break;
	case 0x46: key.code = 0x39; break;
	case 0x48: key.code = 0x33; break;
	case 0x49: key.code = 0x34; break;
	case 0x4a: key.code = 0x16; break;
	case 0x4b: key.code = 0x31; break;
	case 0x4c: key.code = 0x30; break;
	case 0x50: key.code = 0x65; break;
	case 0x51: key.code = 0x63; break;
	case 0x53: key.code = 0x153; break;
	case 0x71: key.code = 0x80; break;
	case 0x72: key.code = 0x81; break;
	case 0x73: key.code = 0x82; break;
	case 0x74: key.code = 0x83; break;
	case 0x40: case 0x6b: case 0x6c:	/* power */
		if (work.standbyCallback != NULL)
			work.standbyCallback(1);
		/* fall through */
	default:
		key.buttons = 0;
		key.code = CEC_KEY_NONE;
		key.flags = 0;
		goto count;
	}
	key.flags = CEC_KEY_PRESSED;
	_notifyKey(&key);
count:
	if (!(work.status & CEC_STATUS_USER_CONTROL_COUNTED) && _countEvent(&statistics.userControl))
		_setStatus(CEC_STATUS_USER_CONTROL_COUNTED);
	return 0;
}

/* <Set Stream Path> received: PS TV becomes the active source when it is the path. */
static int cecRecvSetStreamPath(const CecMessage *msg)
{
	SceUInt8 physical[8];
	CecParam param;
	int ret;

	ret = cecDevGetPhysicalAddress(physical);
	if (ret < 0)
		return ret;
	if (physical[0] == 0xff && physical[1] == 0xff)
		return -1;
	if (msg->frame[2] != physical[0] || msg->frame[3] != physical[1]) {
		_clearStatus(CEC_STATUS_ACTIVE_SOURCE);
		return 0;
	}
	ret = _send(OP_ACTIVE_SOURCE, &param);
	if (ret < 0)
		return ret;
	_setStatus(CEC_STATUS_ACTIVE_SOURCE);
	param = (CecParam){ { 0 } };
	param.data[0] = msg->initiator;
	param.data[1] = CEC_MENU_ACTIVATED;
	return _send(OP_MENU_STATUS, &param);
}

#if FW_SINCE(0x04000000)
static const SceDtraceProbe rxProbe;
#endif

/* Handles received message id. Sony's name. */
static int _execRecvAction(SceUInt8 id)
{
	SceUInt32 length = 0;
	SceUInt8 frame[CEC_FRAME_SIZE] = { 0 };
	SceUInt8 initiator, destination, opcode;
	const CecOpcode *entry;
	CecMessage msg;
	CecParam param;
	int ret;

	ret = cecDevGetRxLength(id, &length);
	if (ret < 0 || length - 1 > CEC_FRAME_SIZE - 1)
		return ret;
	ret = cecDevGetRxFrame(id, frame, length);
	if (ret < 0 || length < 2)
		return ret;
	destination = frame[0] & 0xf;
	initiator = frame[0] >> 4;
	opcode = frame[1];
#if FW_SINCE(0x04000000)
	if (rxProbeHook != NULL)
		rxProbeHook(&rxProbe, destination, initiator, opcode, frame[2], frame[3], frame[4],
			    frame[5], frame[6], frame[7], frame[8]);
#endif
	if (!handlersReady) {
		for (entry = opcodes; entry != &opcodes[70]; entry++)
			handlers[entry->opcode] = entry;
		handlersReady = 1;
	}
	entry = handlers[opcode];
	if (entry == NULL) {
		if (destination == CEC_ADDR_BROADCAST)
			return 0;
		param.data[0] = initiator;
		param.data[1] = opcode;
		param.data[2] = CEC_ABORT_UNRECOGNIZED_OPCODE;
		return _send(OP_FEATURE_ABORT, &param);
	}
	if (length < entry->minLength)
		return 0;
	if (entry->directed && !entry->broadcast && destination == CEC_ADDR_BROADCAST)
		return 0;
	if (!handlers[opcode]->directed && handlers[opcode]->broadcast &&
	    destination != CEC_ADDR_BROADCAST)
		return 0;
	if (initiator == CEC_ADDR_BROADCAST) {
		switch (opcode) {
		case 0x36: case 0x70: case 0x80: case 0x81: case 0x83: case 0x84:
		case 0x85: case 0x86: case 0x8c: case 0x91: case 0x9d:
			break;
		default:
			return 0;
		}
	}
	msg.initiator = initiator;
	msg.destination = destination;
	msg.frame = frame;
	msg.length = length;
	if (handlers[opcode] == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 1736);
		return -1;
	}
	if (handlers[opcode]->receive == NULL) {
		if (destination == CEC_ADDR_BROADCAST)
			return 0;
		param.data[0] = initiator;
		param.data[1] = opcode;
		param.data[2] = CEC_ABORT_UNRECOGNIZED_OPCODE;
		return _send(OP_FEATURE_ABORT, &param);
	}
	return handlers[opcode]->receive(&msg);
}

/* Polls the playback addresses for a free one. Sony's name. */
static inline void _allocLogicalAddr(const SceUInt8 *physical)
{
	SceUInt8 candidates[3];
	SceUInt8 header, source, address;
	CecParam param;
	int i, ret;

	switch (logicalAddress) {
	case CEC_ADDR_PLAYBACK1:
	case CEC_ADDR_BROADCAST:
		candidates[0] = CEC_ADDR_PLAYBACK1;
		candidates[1] = CEC_ADDR_PLAYBACK2;
		candidates[2] = CEC_ADDR_PLAYBACK3;
		break;
	case CEC_ADDR_PLAYBACK2:
		candidates[0] = CEC_ADDR_PLAYBACK2;
		candidates[1] = CEC_ADDR_PLAYBACK3;
		candidates[2] = CEC_ADDR_PLAYBACK1;
		break;
	case CEC_ADDR_PLAYBACK3:
		candidates[0] = CEC_ADDR_PLAYBACK3;
		candidates[1] = CEC_ADDR_PLAYBACK1;
		candidates[2] = CEC_ADDR_PLAYBACK2;
		break;
	default:
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2416, "_allocLogicalAddr");
		cecDevSetLogicalAddress(CEC_ADDR_BROADCAST);
		return;
	}
	for (i = 0; i < 3; i++) {
		address = candidates[i];
		if (work.status & CEC_STATUS_ALLOCATED)
			cecDevGetLogicalAddress(&source);
		else
			source = address;
		header = (source << 4) | address;
		ret = cecDevTx(&header, 1);
		if (ret < 0 || ret == 1 || ret == 0)
			continue;
		if (ret != 2)
			continue;
		logicalAddress = address;
		_setStatus(CEC_STATUS_ALLOCATED);
		ret = cecDevSetLogicalAddress(address);
		if (ret < 0 || address == CEC_ADDR_BROADCAST)
			return;
		param = (CecParam){ { 0 } };
		param.data[0] = physical[0];
		param.data[1] = physical[1];
		param.data[2] = CEC_DEVICE_TYPE_PLAYBACK;
		if (_send(OP_REPORT_PHYSICAL_ADDRESS, &param) < 0)
			return;
		if (work.addressCallback != NULL)
			work.addressCallback(2);
		_setStatus(CEC_STATUS_REPORTED);
		return;
	}
	logicalAddress = CEC_ADDR_BROADCAST;
	_clearStatus(CEC_STATUS_ALLOCATED);
	cecDevSetLogicalAddress(CEC_ADDR_BROADCAST);
}

/* "SceHdmiCecMain". Sony's name. */
static int _mainThread(SceSize args, void *argp)
{
	SceUInt32 bits;
	SceUInt32 count;
	SceUInt8 physical[8];
	CecParam param;
	SceUInt32 i;
	int ret, i15;

	for (;;) {
		ret = sceKernelWaitEventFlag(work.eventFlag, CEC_EVENT_ALL,
					      SCE_KERNEL_EVF_WAITMODE_OR,
					      &bits, NULL);
		if (ret < 0) {
			SCE_DBG_ASSERT_LEVEL(0, 0, 2253);
			return ret;
		}
		if (bits & CEC_EVENT_TX_DONE)
			sceKernelClearEventFlag(work.eventFlag, ~CEC_EVENT_TX_DONE);
		if (bits & CEC_EVENT_ALLOC) {
			if ((work.status & CEC_STATUS_ENABLED) && !(work.status & CEC_STATUS_REPORTED) &&
			    cecDevGetPhysicalAddress(physical) >= 0 &&
			    (physical[0] != 0xff || physical[1] != 0xff)) {
				for (i15 = 0; i15 < CEC_ADDR_COUNT; i15++)
					work.unrecognized[i15][0] = 0;
				_allocLogicalAddr(physical);
			}
			sceKernelClearEventFlag(work.eventFlag, ~CEC_EVENT_ALLOC);
		}
		if (bits & CEC_EVENT_RX) {
			SceUInt8 ids[3] = { 0 };

			ret = cecDevRxGetPending(ids, &count);
			if (ret < 0) {
				for (i = 0; i < count; i++)
					cecDevRxDone(ids[i]);
			} else {
				for (i = 0; i < count; i++) {
					ret = _execRecvAction(ids[i]);
					cecDevRxDone(ids[i]);
					if (ret < 0)
						break;
				}
			}
			sceKernelClearEventFlag(work.eventFlag, ~CEC_EVENT_RX);
		}
		if (bits & CEC_EVENT_ONE_TOUCH_PLAY) {
			if (work.status & CEC_STATUS_ALLOCATED) {
				param = (CecParam){ { 0 } };
				param.data[0] = CEC_ADDR_TV;
				if (_send(OP_TEXT_VIEW_ON, &param) >= 0 &&
				    _send(OP_ACTIVE_SOURCE, &param) >= 0) {
					_setStatus(CEC_STATUS_ACTIVE_SOURCE);
					param = (CecParam){ { 0 } };
					param.data[0] = CEC_ADDR_TV;
					param.data[1] = CEC_MENU_ACTIVATED;
					_send(OP_MENU_STATUS, &param);
				}
			}
			sceKernelClearEventFlag(work.eventFlag, ~CEC_EVENT_ONE_TOUCH_PLAY);
		}
		if (bits & CEC_EVENT_STANDBY) {
			param = (CecParam){ { 0 } };
			if (work.status & CEC_STATUS_ENABLED) {
				param.data[0] = CEC_ADDR_TV;
				param.data[1] = CEC_MENU_DEACTIVATED;
				_send(OP_MENU_STATUS, &param);
				if (work.status & CEC_STATUS_ACTIVE_SOURCE) {
					param.data[0] = CEC_ADDR_TV;
					_send(OP_INACTIVE_SOURCE, &param);
					_clearStatus(CEC_STATUS_ACTIVE_SOURCE);
				}
				cecDevDisable();
				_clearStatus(CEC_STATUS_ENABLED);
				_clearStatus(CEC_STATUS_REPORTED);
				_clearStatus(CEC_STATUS_CLEARED);
			}
			sceKernelClearEventFlag(work.eventFlag, ~CEC_EVENT_STANDBY);
		}
	}
}

int cecInitialize(void)
{
	CecDevCallbacks callbacks;
	SceUID thread;
	int ret;

	ret = _checkCecSupported();
	if (ret != 0)
		return ret;
	_clearStatus(CEC_STATUS_ENABLED);
	_clearStatus(CEC_STATUS_ALLOCATED);
	_clearStatus(CEC_STATUS_CONNECTED);
	_clearStatus(CEC_STATUS_CLEARED);
	_clearStatus(CEC_STATUS_STANDBY_REQUEST);
	_clearStatus(CEC_STATUS_ACTIVE_SOURCE);
	_clearStatus(CEC_STATUS_REPORTED);
	_clearStatus(CEC_STATUS_ONE_TOUCH_COUNTED);
	_clearStatus(CEC_STATUS_USER_CONTROL_COUNTED);
	thread = sceKernelCreateThread("SceHdmiCecMain", _mainThread, 0x1f, 0x2000, 0,
					0x80000, NULL);
	if (thread < 0) {
		SCE_DBG_ASSERT_FAIL(2762);
		return thread;
	}
	ret = sceKernelCreateEventFlag("SceHdmiCEC", SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(2774);
		return ret;
	}
	work.eventFlag = ret;
	ret = sceKernelStartThread(thread, 0, NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(2781);
		return ret;
	}
	callbacks.txDone = cecDevCallbackTxDone;
	callbacks.rx = cecDevCallbackRx;
	callbacks.nop1 = cecDevCallbackNop1;
	callbacks.disconnected = cecDevCallbackDisconnected;
	callbacks.nop2 = cecDevCallbackNop2;
	callbacks.nop3 = cecDevCallbackNop3;
	callbacks.connected = cecDevCallbackConnected;
	callbacks.standby = cecDevCallbackStandby;
	callbacks.isEnabled = cecDevCallbackIsEnabled;
	callbacks.standbyDone = cecDevCallbackStandbyDone;
	return cecDevInitialize(&callbacks);
}

/* SceHdmiForDriver_3126A070. Sony's name. */
int sceHdmiCecRegisterEventCallback(int event, void (*callback)(int event))
{
	if (event == 1) {
		SCE_DBG_ASSERT_CHECK(work.standbyCallback == NULL, 2829);
		work.standbyCallback = callback;
	} else if (event == 2) {
		SCE_DBG_ASSERT_CHECK(work.addressCallback == NULL, 2832);
		work.addressCallback = callback;
	}
	return 0;
}

int SceHdmiForDriver_47894820(int mode)
{
	if ((SceUInt32)mode < 2)
		work.passThroughMode = mode;
	return 0;
}

/* SceHdmiForDriver_FA30ACFC. Sony's name. */
int sceHdmiCecEnableCec(void)
{
	int ret;

	ret = _checkCecSupported();
	if (ret != 0)
		return ret;
	if (_sceHdmiLockMutex() != 0) {
		SCE_DBG_ASSERT_PRINTF(0, 2861, "err _sceHdmiLockMutex()\n");
		return SCE_HDMI_ERROR_CEC_FAILED;
	}
	if (work.status & CEC_STATUS_ENABLED) {
		if (_sceHdmiUnlockMutex() != 0) {
			SCE_DBG_ASSERT_PRINTF(0, 2870, "err _sceHdmiUnlockMutex()\n");
			return SCE_HDMI_ERROR_CEC_FAILED;
		}
		return 0;
	}
	if (cecDevEnable() < 0) {
		if (_sceHdmiUnlockMutex() != 0) {
			SCE_DBG_ASSERT_PRINTF(0, 2881, "err _sceHdmiUnlockMutex()\n");
			return SCE_HDMI_ERROR_CEC_FAILED;
		}
		return 0;
	}
	_setStatus(CEC_STATUS_ENABLED);
	if (_sceHdmiUnlockMutex() != 0) {
		SCE_DBG_ASSERT_PRINTF(0, 2891, "err _sceHdmiUnlockMutex()\n");
		return SCE_HDMI_ERROR_CEC_FAILED;
	}
	if ((work.status & CEC_STATUS_ENABLED) && !(work.status & CEC_STATUS_REPORTED) &&
	    (work.status & (CEC_STATUS_ALLOCATED | CEC_STATUS_CONNECTED)))
		sceKernelSetEventFlag(work.eventFlag, CEC_EVENT_ALLOC);
	return 0;
}

int SceHdmiForDriver_291F000B(void)
{
	int ret;

	ret = _checkCecSupported();
	if (ret != 0)
		return ret;
	if (work.status & CEC_STATUS_ENABLED) {
		cecDevDisable();
		_clearStatus(CEC_STATUS_ENABLED);
		_clearStatus(CEC_STATUS_REPORTED);
	}
	return 0;
}

int SceHdmiForDriver_D929E4E5(void)
{
	int ret;

	ret = _checkCecSupported();
	if (ret != 0)
		return ret;
	if (work.status & CEC_STATUS_ENABLED)
		sceKernelSetEventFlag(work.eventFlag, CEC_EVENT_STANDBY);
	return 0;
}

int SceHdmiForDriver_05174BC2(CecKeyCallback callback)
{
	work.keyCallback = callback;
	return 0;
}

/* Sony's name. */
static inline int _oneTouchPlayAlarmStart(void)
{
	SceUID alarm;

	_setStatus(CEC_STATUS_ONE_TOUCH_PENDING);
	if (work.oneTouchPlayAlarm != 0) {
		sceKernelCancelAlarm(work.oneTouchPlayAlarm);
		work.oneTouchPlayAlarm = 0;
	}
	alarm = SceThreadmgrForDriver_C58DF384("SceHdmiCecOneTouchPlay", CEC_ONE_TOUCH_PLAY_USEC,
					oneTouchPlayAlarmHandler, NULL);
	if (alarm < 0) {
		SCE_DBG_ASSERT_FUNC(0, 362, "_oneTouchPlayAlarmStart");
		return alarm;
	}
	work.oneTouchPlayAlarm = alarm;
	sceKernelSetEventFlag(work.eventFlag, CEC_EVENT_ONE_TOUCH_PLAY);
	return 0;
}

/* SceHdmiForDriver_4C31F5E8. Sony's name. */
int sceHdmiCecCmdOneTouchPlay(int mode)
{
	int ret;

	if (mode == 1) {
		if (!(work.status & CEC_STATUS_ENABLED))
			return SCE_HDMI_ERROR_CEC_DISABLED;
	} else if (mode == 0) {
		if (sceKernelGetSystemTimeWide() - work.oneTouchPlayTime > CEC_ONE_TOUCH_PLAY_WINDOW)
			return 0;
		if (!(work.status & CEC_STATUS_ENABLED))
			return SCE_HDMI_ERROR_CEC_DISABLED;
		if (!(work.status & CEC_STATUS_ONE_TOUCH_COUNTED) && _countEvent(&statistics.oneTouchPlay))
			_setStatus(CEC_STATUS_ONE_TOUCH_COUNTED);
	} else if (mode == 2) {
		if (work.status & CEC_STATUS_ONE_TOUCH_PENDING)
			return 0;
		if (!(work.status & CEC_STATUS_ENABLED))
			return SCE_HDMI_ERROR_CEC_DISABLED;
		_countEvent(&statistics.oneTouchPlay);
	} else {
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	}
	ret = _oneTouchPlayAlarmStart();
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(3001);
	return ret;
}

int SceHdmiForDriver_EA486A37(int start)
{
	if (start != 0) {
		work.oneTouchPlayTime = sceKernelGetSystemTimeWide();
		_clearStatus(CEC_STATUS_ONE_TOUCH_COUNTED);
		_clearStatus(CEC_STATUS_USER_CONTROL_COUNTED);
	}
	return 0;
}

int SceHdmiForDriver_93AA48AC(CecStatistics *stats)
{
	if (stats == NULL || stats->size > sizeof(CecStatistics))
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	__builtin_memcpy(stats, &statistics, sizeof(statistics));
	return 0;
}

int SceHdmiForDriver_47B753EB(void)
{
	statistics = (CecStatistics){ .size = sizeof(CecStatistics) };
	statistics.time = sceKernelGetSystemTimeWide();
	return 0;
}

/* Polls address 4 with the transmitter set to mode 0. */
int SceHdmiForDriver_6D2D10D8(void)
{
	SceUInt8 header;
	int ret;

	if (!(work.status & CEC_STATUS_ENABLED))
		return -1;
	if (cecDevSetMode(0) < 0)
		return SCE_HDMI_ERROR_CEC_FAILED;
	header = (CEC_ADDR_PLAYBACK1 << 4) | CEC_ADDR_PLAYBACK1;
	ret = cecDevTx(&header, 1);
	if (ret < 0)
		return ret;
	if (ret == 1)
		return -1;
	return 0;
}

static const CecOpcode opcodes[] = {
	{ 0x82, 0, 1, 4, cecSendActiveSource, cecRecvActiveSource },
	{ 0x04, 1, 0, 2, NULL, NULL },
	{ 0x0d, 1, 0, 2, cecSendTextViewOn, NULL },
	{ 0x9d, 1, 0, 4, cecSendInactiveSource, NULL },
	{ 0x85, 0, 1, 2, NULL, cecRecvRequestActiveSource },
	{ 0x80, 0, 1, 4, NULL, NULL },
	{ 0x81, 0, 1, 4, NULL, NULL },
	{ 0x86, 0, 1, 4, NULL, cecRecvSetStreamPath },
	{ 0x36, 1, 1, 2, NULL, cecRecvStandby },
	{ 0x0b, 1, 0, 2, NULL, NULL },
	{ 0x09, 1, 0, 3, NULL, NULL },
	{ 0x0a, 1, 0, 3, NULL, NULL },
	{ 0x0f, 1, 0, 2, NULL, NULL },
	{ 0x33, 1, 0, 13, NULL, NULL },
	{ 0x99, 1, 0, 16, NULL, NULL },
	{ 0xa1, 1, 0, 11, NULL, NULL },
	{ 0x34, 1, 0, 13, NULL, NULL },
	{ 0x97, 1, 0, 16, NULL, NULL },
	{ 0xa2, 1, 0, 11, NULL, NULL },
	{ 0x67, 1, 0, 3, NULL, NULL },
	{ 0x43, 1, 0, 3, NULL, NULL },
	{ 0x35, 1, 0, 3, NULL, NULL },
	{ 0x9e, 1, 0, 3, cecSendCecVersion, NULL },
	{ 0x9f, 1, 0, 2, NULL, cecRecvGetCecVersion },
	{ 0x83, 1, 0, 2, NULL, cecRecvGivePhysicalAddress },
	{ 0x91, 1, 0, 2, NULL, NULL },
	{ 0x84, 0, 1, 5, cecSendReportPhysicalAddress, NULL },
	{ 0x32, 0, 1, 5, NULL, NULL },
	{ 0x42, 1, 0, 3, NULL, NULL },
	{ 0x1b, 1, 0, 3, NULL, NULL },
	{ 0x1a, 1, 0, 3, NULL, NULL },
	{ 0x41, 1, 0, 3, NULL, NULL },
	{ 0x08, 1, 0, 3, NULL, NULL },
	{ 0x92, 1, 0, 6, NULL, NULL },
	{ 0x93, 1, 0, 9, NULL, NULL },
	{ 0x07, 1, 0, 3, NULL, NULL },
	{ 0x06, 1, 0, 3, NULL, NULL },
	{ 0x05, 1, 0, 2, NULL, NULL },
	{ 0x87, 0, 1, 5, NULL, NULL },
	{ 0x8c, 1, 0, 2, NULL, NULL },
	{ 0x89, 1, 0, 2, NULL, NULL },
	{ 0xa0, 1, 1, 5, NULL, NULL },
	{ 0x8a, 1, 1, 2, NULL, NULL },
	{ 0x8b, 1, 1, 2, NULL, NULL },
	{ 0x64, 1, 0, 4, NULL, NULL },
	{ 0x47, 1, 0, 3, cecSendSetOsdName, NULL },
	{ 0x46, 1, 0, 2, NULL, cecRecvGiveOsdName },
	{ 0x8d, 1, 0, 3, NULL, cecRecvMenuRequest },
	{ 0x8e, 1, 0, 3, cecSendMenuStatus, NULL },
	{ 0x44, 1, 0, 3, NULL, _recvUserControlPressed },
	{ 0x45, 1, 0, 2, NULL, cecRecvUserControlReleased },
	{ 0x8f, 1, 0, 2, cecSendGiveDevicePowerStatus, cecRecvGiveDevicePowerStatus },
	{ 0x90, 1, 0, 3, cecSendReportPowerStatus, cecRecvReportPowerStatus },
	{ 0x00, 1, 0, 4, cecSendFeatureAbort, _setRecvUnrecognizedOpcode },
	{ 0xff, 1, 0, 2, NULL, cecRecvAbort },
	{ 0x71, 1, 0, 2, NULL, NULL },
	{ 0x7d, 1, 0, 2, NULL, NULL },
	{ 0x7a, 1, 0, 3, NULL, NULL },
	{ 0xa3, 1, 0, 5, NULL, NULL },
	{ 0xa4, 1, 0, 3, NULL, NULL },
	{ 0x72, 1, 1, 3, NULL, NULL },
	{ 0x70, 1, 0, 2, NULL, NULL },
	{ 0x7e, 1, 0, 3, NULL, NULL },
	{ 0x9a, 1, 0, 3, NULL, NULL },
	{ 0xc0, 1, 0, 2, NULL, NULL },
	{ 0xc1, 1, 0, 2, NULL, NULL },
	{ 0xc2, 1, 0, 2, NULL, NULL },
	{ 0xc3, 1, 0, 2, NULL, NULL },
	{ 0xc4, 1, 0, 2, NULL, NULL },
	{ 0xc5, 1, 0, 2, NULL, NULL },
};

#if FW_SINCE(0x04000000)
/* DTrace probes __hdmi__hdmi_cec_tx and __hdmi__hdmi_cec_rx. */
static const SceDtraceProbe txProbe = {
	.enabled = &txProbeEnabled,
	.provider = "__hdmi__",
	.name = "hdmi_cec_tx",
	.address = _txOpecode,		/* PLACEHOLDER: Sony's is the probe site (0x81002cda) */
	.hook = &txProbeHook,
};

static const SceDtraceProbe rxProbe = {
	.enabled = &rxProbeEnabled,
	.provider = "__hdmi__",
	.name = "hdmi_cec_rx",
	.address = _execRecvAction,	/* PLACEHOLDER: Sony's is the probe site (0x81003850) */
	.hook = &rxProbeHook,
};

static const SceDtraceProbe *const cecProbes[] __attribute__((used)) = { &txProbe, &rxProbe };
#endif
