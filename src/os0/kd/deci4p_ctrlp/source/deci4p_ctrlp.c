/*
 * SceDeci4pCtrlp: DECI4P protocol 0x2c000 ("CTRLP"). The host records the
 * controller and touch input of the target, plays recorded input back into
 * SceCtrl and SceTouch, emulates buttons and sticks, sets rapid fire, and
 * pairs Bluetooth controllers.
 *
 * Recorded and played-back input goes through one ring of records. Each
 * controller port and each touch panel reads the ring with its own read index;
 * a playback record is consumed when its port's sample callback reaches its
 * frame or time.
 */
#include <stdint.h>
#include <stddef.h>
#include <psp2/touch.h>
#include <psp2kern/bt.h>
#include <psp2kern/ctrl.h>
#include <psp2kern/kernel/aimgr.h>
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/cpu/spinlock.h>
#include "sce/error.h"
#include "sce/ctrl_sample.h"
#include "sce/deci4p.h"
#include "sce/deci4p_ctrlp.h"

#if FW_VERSION >= 0x04000000
#define error(line, fmt, ...) ksceKernelPrintf("[CTRLP E2]::%d " fmt, line, ##__VA_ARGS__)
#else
#define error(line, fmt, ...) ((void)0)
#endif

#define CTRLP_PROTOCOL		0x2c000

#define CTRLP_DIPSW		0xc2	/* the protocol is available */
#define CTRLP_DIPSW_AUTO_ENABLE	0xc3

/* commands from the host; the response code is the command code + 1 */
#define CTRLP_CMD_START_RECORD		0x02
#define CTRLP_CMD_STOP_RECORD		0x04
#define CTRLP_CMD_START_PLAYBACK	0x08
#define CTRLP_CMD_STOP_PLAYBACK		0x0a
#define CTRLP_CMD_PUT_RECORDS		0x0c
#define CTRLP_CMD_EMULATE		0x10
#define CTRLP_CMD_SET_RAPID_FIRE	0x12
#define CTRLP_CMD_CLEAR_RAPID_FIRE	0x14
#define CTRLP_CMD_GET_PANEL_INFO	0x16
#define CTRLP_CMD_BT_REGISTER		0x20
#define CTRLP_CMD_BT_DELETE		0x22
#define CTRLP_CMD_GET_PORT_INFO		0x24
#define CTRLP_CMD_PUT_RECORDS2		0x28	/* records carry a CtrlpPortType */
#define CTRLP_CMD_EMULATE2		0x2a

/* packets the target sends on its own */
#define CTRLP_NOTIFY_RECORDS		0x07
#define CTRLP_NOTIFY_SPACE		0x0f	/* playback: room for more records */
#define CTRLP_NOTIFY_RECORDS2		0x27

/* command results */
#define CTRLP_RESULT_OK			0
#define CTRLP_RESULT_NO_SPACE		1
#define CTRLP_RESULT_BUSY		4
#define CTRLP_RESULT_INVALID		7
#define CTRLP_RESULT_UNKNOWN_COMMAND	8
#define CTRLP_RESULT_ILLEGAL_STATE	(CTRLP_PROTOCOL | 0x00)
#define CTRLP_RESULT_BT_ERROR_1801	(CTRLP_PROTOCOL | 0x02)
#define CTRLP_RESULT_BT_ERROR_1802	(CTRLP_PROTOCOL | 0x03)
#define CTRLP_RESULT_FAILED		(CTRLP_PROTOCOL | 0x04)
#define CTRLP_RESULT_BT_NOT_REGISTERED	(CTRLP_PROTOCOL | 0x05)
#define CTRLP_RESULT_BT_CONNECTING	(CTRLP_PROTOCOL | 0x07)
#define CTRLP_RESULT_BT_DELETE_FAILED	(CTRLP_PROTOCOL | 0x08)
#define CTRLP_RESULT_TYPE_MISMATCH	(CTRLP_PROTOCOL | 0x0b)

/* SceBtForDriver_10D68FAE errors (names unknown) */
#define BT_ERROR_REGISTER_1801		0x802f1801
#define BT_ERROR_REGISTER_1802		0x802f1802
#define BT_ERROR_REGISTER_INVALID	0x802f1803

/* CtrlpState.mode */
#define CTRLP_MODE_IDLE		0
#define CTRLP_MODE_PLAYBACK	1
#define CTRLP_MODE_RECORD	2

/* CtrlpState.streams */
#define CTRLP_STREAM_CTRL	0x1
#define CTRLP_STREAM_TOUCH	0x2

/* CtrlpState.flags (playback): records are due by frame, not by time */
#define CTRLP_SYNC_CTRL_FRAME	0x1
#define CTRLP_SYNC_TOUCH_FRAME	0x2

/* CtrlpState.pending and .sending: what the next send event sends */
#define CTRLP_SEND_RECORDS	0x1
#define CTRLP_SEND_SPACE	0x2
#define CTRLP_SEND_RESPONSE	0x4

/* CtrlpRecordHeader.kind */
#define CTRLP_RECORD_CTRL	0
#define CTRLP_RECORD_TOUCH	1

/* CtrlpPortType.type: the controller as the host names it */
#define CTRLP_DEVICE_BUILTIN	1
#define CTRLP_DEVICE_DS3	2
#define CTRLP_DEVICE_DS4	3
#define CTRLP_DEVICE_NONE	0xff

#define CTRLP_PORT_NUM		5	/* SceCtrl ports */
#define CTRLP_READER_TOUCH	CTRLP_PORT_NUM	/* the touch panels read after the ports */
#define CTRLP_READER_NUM	(CTRLP_PORT_NUM + SCE_TOUCH_PORT_MAX_NUM)
#define CTRLP_RING_SIZE		256
#define CTRLP_TOUCH_REPORT_NUM	6

#define CTRLP_RECV_SIZE		0xda0
#define CTRLP_SEND_SIZE		0xd9c

#define CTRLP_EMULATION_TIME	3600

/* a controller port and what is connected to it */
typedef struct CtrlpPortType {
	SceUInt16 size;				/* sizeof(CtrlpPortType) */
	SceUInt8 port;
	SceUInt8 type;				/* CTRLP_DEVICE_* */
} CtrlpPortType;

typedef struct CtrlpCtrlData {
	SceUInt32 size;				/* sizeof(CtrlpCtrlData) */
	SceUInt32 buttons;
	SceUInt8 lx;
	SceUInt8 ly;
	SceUInt8 rx;
	SceUInt8 ry;
	SceUInt8 up;
	SceUInt8 right;
	SceUInt8 down;
	SceUInt8 left;
} CtrlpCtrlData;

typedef struct CtrlpTouchReport {
	SceUInt32 size;				/* sizeof(CtrlpTouchReport) */
	SceUInt8 id;
	SceUInt8 force;
	SceUInt8 reserved[2];
	SceUInt16 x;
	SceUInt16 y;
} CtrlpTouchReport;

typedef struct CtrlpTouchData {
	SceUInt32 size;				/* sizeof(CtrlpTouchData) */
	SceUInt32 status;
	SceUInt32 reportNum;
	CtrlpTouchReport report[CTRLP_TOUCH_REPORT_NUM];
} CtrlpTouchData;

/* records follow each other in packets at their own size, so 4-byte aligned */
typedef struct CtrlpRecordHeader {
	SceUInt32 size;				/* the header and its data */
	SceUInt32 frame;
	SceUInt64 time;				/* microseconds since the start */
	SceUInt8 kind;				/* CTRLP_RECORD_* */
	SceUInt8 port;				/* controller port or touch panel */
	SceUInt8 reserved[2];
	CtrlpPortType portType;			/* CTRLP_CMD_PUT_RECORDS2, CTRLP_NOTIFY_RECORDS2 */
} __attribute__((packed, aligned(4))) CtrlpRecordHeader;

typedef struct CtrlpRecord {
	CtrlpRecordHeader header;
	union {
		CtrlpCtrlData ctrl;
		CtrlpTouchData touch;
	};
} __attribute__((packed, aligned(4))) CtrlpRecord;

/* the header of every CTRLP packet, after the DECI4P header */
typedef struct CtrlpHeader {
	SceUInt16 length;			/* this header and the body */
	SceUInt16 code;				/* CTRLP_CMD_*, CTRLP_NOTIFY_* */
	SceUInt32 sequence;
} CtrlpHeader;

typedef struct CtrlpStartRecord {
	SceUInt32 streams;			/* CTRLP_STREAM_* */
	SceUInt32 version;			/* optional: 1 sends CtrlpPortType */
} CtrlpStartRecord;

typedef struct CtrlpStartPlayback {
	SceUInt32 streams;			/* CTRLP_STREAM_* */
	SceUInt32 flags;			/* CTRLP_SYNC_* */
} CtrlpStartPlayback;

typedef struct CtrlpPutRecords {
	SceUInt32 count;
	SceUInt32 spaceThreshold;		/* free records worth a CTRLP_NOTIFY_SPACE */
	SceUInt8 records[];			/* CtrlpRecord, each of its own size */
} CtrlpPutRecords;

typedef struct CtrlpRecords {
	SceUInt32 count;
	SceUInt8 records[];			/* CtrlpRecord, each of its own size */
} CtrlpRecords;

typedef struct CtrlpEmulate {
	SceUInt32 port;
	CtrlpCtrlData data;
	CtrlpPortType portType;			/* CTRLP_CMD_EMULATE2 */
} CtrlpEmulate;

typedef struct CtrlpSetRapidFire {
	SceUInt32 port;
	SceUInt32 index;
	SceCtrlRapidFireRule rule;
	CtrlpPortType portType;			/* optional */
} CtrlpSetRapidFire;

typedef struct CtrlpClearRapidFire {
	SceUInt32 port;
	SceUInt32 index;
	CtrlpPortType portType;			/* optional */
} CtrlpClearRapidFire;

typedef struct CtrlpGetPanelInfo {
	SceUInt32 panel;
} CtrlpGetPanelInfo;

/* passed through to SceBtForDriver_10D68FAE, whose parameters are not named */
typedef struct CtrlpBtRegister {
	SceUInt8 address[8];
	SceUInt32 deviceClass;
	SceUInt8 pinCode[8];
	SceUInt16 vendorId;
	SceUInt16 productId;
	SceUInt16 deviceVersion;
	SceUInt16 reserved;
	SceUInt32 nameLength;
	char name[];				/* nameLength bytes and a NUL */
} CtrlpBtRegister;

/* the address as ksceBtDeleteRegisteredInfo takes it */
typedef struct CtrlpBtDelete {
	SceUInt32 mac0;
	SceUInt16 mac1;
} __attribute__((packed)) CtrlpBtDelete;

typedef struct CtrlpPutRecordsResult {
	SceUInt32 result;
	SceUInt32 space;
} CtrlpPutRecordsResult;

typedef struct CtrlpPanelInfoResult {
	SceUInt32 result;
	SceTouchPanelInfo info;
} CtrlpPanelInfoResult;

typedef struct CtrlpBtRegisterResult {
	SceUInt32 result;
	SceUInt8 address[6];
	SceUInt8 reserved[2];
	SceUInt8 linkKey[16];
} CtrlpBtRegisterResult;

typedef struct CtrlpPortInfoResult {
	SceUInt32 result;
	SceUInt32 count;
	CtrlpPortType port[CTRLP_PORT_NUM];
} CtrlpPortInfoResult;

/* the body of a response */
typedef union CtrlpResult {
	SceUInt32 result;
	CtrlpPutRecordsResult putRecords;
	CtrlpPanelInfoResult panelInfo;
	CtrlpBtRegisterResult btRegister;
	CtrlpPortInfoResult portInfo;
	SceUInt8 buffer[64];
} CtrlpResult;

typedef struct CtrlpPacket {
	SceDeci4pHeader deci4p;
	CtrlpHeader header;
	union {
		CtrlpStartRecord startRecord;
		CtrlpStartPlayback startPlayback;
		CtrlpPutRecords putRecords;
		CtrlpEmulate emulate;
		CtrlpSetRapidFire setRapidFire;
		CtrlpClearRapidFire clearRapidFire;
		CtrlpGetPanelInfo getPanelInfo;
		CtrlpBtRegister btRegister;
		CtrlpBtDelete btDelete;
		CtrlpRecords records;
		SceUInt32 space;		/* CTRLP_NOTIFY_SPACE */
		CtrlpResult response;
	};
} CtrlpPacket;

typedef struct CtrlpRing {
	SceUInt32 writeIndex;
	SceUInt32 readIndex[CTRLP_READER_NUM];
	CtrlpRecord record[CTRLP_RING_SIZE];
} CtrlpRing;

typedef struct CtrlpState {
	SceKernelSpinlock lock;
	SceUInt32 mode;				/* CTRLP_MODE_* */
	SceUInt32 flags;			/* CTRLP_SYNC_* */
	SceUInt32 streams;			/* CTRLP_STREAM_* */
	SceUInt64 baseTime;
	SceUInt32 ctrlFrame;
	SceUInt32 touchFrame;
	SceUInt32 recordThreshold;		/* records worth a CTRLP_NOTIFY_RECORDS */
	SceUInt32 recordReady;			/* the last records were sent */
	SceUInt32 sequence;
	SceUInt32 spaceWanted;			/* the host waits for room */
	SceUInt32 spaceThreshold;
	SceTouchPanelInfo panelInfo[SCE_TOUCH_PORT_MAX_NUM];
	SceUInt32 buttonMask;
	SceUInt32 dolce;
	int handle;				/* sceDfMgrOpen */
	SceUInt32 rapidFire[CTRLP_PORT_NUM];	/* rapid fire rules set, one bit each */
	SceUInt32 sending;			/* CTRLP_SEND_* */
	SceUInt32 pending;			/* CTRLP_SEND_* */
	CtrlpHeader responseHeader;
	CtrlpResult response;
	SceUInt32 space;			/* free records for CTRLP_NOTIFY_SPACE */
	SceUInt32 sendCount;
	SceUInt32 sendSize;
	CtrlpPacket *sendBuffer;
	SceUInt32 recvCount;
	SceUInt32 recvSize;
	CtrlpPacket *recvBuffer;
	SceCtrlPortInfo portInfo;
	SceUInt32 primaryPort;			/* its samples count the frames */
	SceUInt32 version;			/* the host uses CtrlpPortType */
} CtrlpState;

_Static_assert(sizeof(CtrlpRecord) == 108, "CtrlpRecord");
_Static_assert(sizeof(CtrlpState) == 0x138, "CtrlpState");

static CtrlpRing ring;
static SceUInt8 recvBuffer[CTRLP_RECV_SIZE];
static CtrlpState state;
static SceDfMgrProtocol protocol;
static SceUInt8 sendBuffer[CTRLP_SEND_SIZE];
static CtrlpRecord saved[CTRLP_READER_NUM];	/* the last sample played back */

static inline SceUInt32 nextIndex(SceUInt32 index)
{
	return (SceUInt8)(index + 1);
}

/* records written and not yet read by the reader */
static inline SceUInt32 ringCount(SceUInt32 reader)
{
	SceUInt32 read = ring.readIndex[reader];

	if (read <= ring.writeIndex)
		return ring.writeIndex - read;
	return ring.writeIndex + CTRLP_RING_SIZE - read;
}

/* records that can be written before the reader's next record */
static inline SceUInt32 ringSpace(SceUInt32 reader)
{
	SceUInt32 read = ring.readIndex[reader];
	SceUInt32 write = ring.writeIndex;

	if (write == read)
		write = nextIndex(write);
	if (read >= write)
		return read - write;
	return CTRLP_RING_SIZE - write + read;
}

static inline void ringAdvanceWrite(void)
{
	ring.writeIndex = nextIndex(ring.writeIndex);
	for (int i = 0; i < CTRLP_READER_NUM; i++) {
		if (ring.writeIndex == ring.readIndex[i])
			ring.readIndex[i] = nextIndex(ring.writeIndex);
	}
}

static inline void ringAdvanceRead(SceUInt32 reader)
{
	if (ring.readIndex[reader] != ring.writeIndex)
		ring.readIndex[reader] = nextIndex(ring.readIndex[reader]);
}

static inline int isController(SceUInt8 type)
{
	return type == SCE_CTRL_TYPE_PHY || type == SCE_CTRL_TYPE_DS3 ||
		type == SCE_CTRL_TYPE_DS4;
}

static inline SceUInt32 controllerType(SceUInt32 port)
{
	if (port > 5)
		return 0;

	switch (state.portInfo.port[port]) {
	case SCE_CTRL_TYPE_PHY:
	case SCE_CTRL_TYPE_DS3:
		return CTRLP_DEVICE_DS3;
	case SCE_CTRL_TYPE_DS4:
		return CTRLP_DEVICE_DS4;
	default:
		return CTRLP_DEVICE_NONE;
	}
}

static inline void getPortInfo(void)
{
	memset(&state.portInfo, 0, sizeof(state.portInfo));
	ksceCtrlGetControllerPortInfo(&state.portInfo);
}

/* the highest port with a controller */
static inline __attribute__((always_inline)) void findPrimaryPort(void)
{
	state.primaryPort = 0;
	for (int port = CTRLP_PORT_NUM - 1; port > 0; port--) {
		if (isController(state.portInfo.port[port])) {
			state.primaryPort = port;
			break;
		}
	}
}

/* L2/R2 fold into L1/R1 for the host's handheld-only view */
static inline SceUInt32 normalizeButtons(SceUInt32 buttons, SceUInt32 version)
{
	if (!state.dolce && !version) {
		if (buttons & (SCE_CTRL_L2 | SCE_CTRL_L1))
			buttons = (buttons & ~SCE_CTRL_L2) | SCE_CTRL_L1;
		if (buttons & (SCE_CTRL_R2 | SCE_CTRL_R1))
			buttons = (buttons & ~SCE_CTRL_R2) | SCE_CTRL_R1;
	}
	return buttons;
}

/* asks SceDeci4pDfMgr for a send event; called with the lock held */
static inline void requestSend(SceUInt32 what)
{
	int ret;

	if (state.pending & what)
		return;
	if (state.pending || state.sending) {
		state.pending |= what;
		return;
	}
	state.pending = what;
	ret = sceDfMgrReqSend(state.handle);
	if (ret < 0)
		error(901, "sceDfMgrReqSend error %d\n", ret);
}

/* records the ring can take before a playback reader would be overrun */
static inline SceUInt32 freeRecords(void)
{
	SceUInt32 space = CTRLP_RING_SIZE, n;

	if (state.streams & CTRLP_STREAM_CTRL) {
		for (int port = 0; port < CTRLP_PORT_NUM; port++) {
			n = ringSpace(port);
			if (space >= n)
				space = n;
		}
		if (space)
			space--;
	}
	if (state.streams & CTRLP_STREAM_TOUCH) {
		SceUInt32 front = ringSpace(CTRLP_READER_TOUCH + SCE_TOUCH_PORT_FRONT);
		SceUInt32 back = ringSpace(CTRLP_READER_TOUCH + SCE_TOUCH_PORT_BACK);

		n = back >= front ? front : back;
		if (space >= n)
			space = n;
		if (space)
			space--;
	}
	return space;
}

/* playback: tells the host when there is room for the records it waits to send */
static inline void checkSpace(void)
{
	SceUInt32 space;

	if (!state.spaceWanted || !state.spaceThreshold)
		return;
	space = state.mode == CTRLP_MODE_PLAYBACK ? freeRecords() : 0;
	if (space >= state.spaceThreshold) {
		state.space = space;
		requestSend(CTRLP_SEND_SPACE);
		state.spaceWanted = 0;
	}
}

/* recording: sends the records once enough are collected */
static inline void checkRecords(void)
{
	if (ringCount(0) >= state.recordThreshold && state.recordReady) {
		state.recordReady = 0;
		requestSend(CTRLP_SEND_RECORDS);
	}
}

/* the last sample of every port: nothing pressed, sticks centred */
static inline __attribute__((always_inline)) void resetSaved(void)
{
	memset(saved, 0, sizeof(saved));
	for (int port = 0; port < CTRLP_PORT_NUM; port++) {
		CtrlpCtrlData *data = &saved[port].ctrl;

		data->lx = data->ly = data->rx = data->ry = 0x80;
		data->up = data->right = data->down = data->left = 0x80;
	}
}

static void applyTouch(SceTouchData *sample, const CtrlpTouchData *data, SceUInt32 panel)
{
	const SceTouchPanelInfo *info = &state.panelInfo[panel];
	const CtrlpTouchReport *in = data->report;

	sample->status = data->status;
	sample->reportNum = data->reportNum >= CTRLP_TOUCH_REPORT_NUM ?
		CTRLP_TOUCH_REPORT_NUM : data->reportNum;
	for (SceUInt32 i = 0; i < sample->reportNum; i++, in++) {
		SceTouchReport *report = &sample->report[i];

		report->id = (SceInt8)in->id < 0 ? 0x7f : in->id;
		if (in->force > info->maxForce)
			report->force = info->maxForce;
		else if (in->force < info->minForce)
			report->force = info->minForce;
		else
			report->force = in->force;
		if (in->x > info->maxAaX)
			report->x = info->maxAaX;
		else if (in->x < info->minAaX)
			report->x = info->minAaX;
		else
			report->x = in->x;
		if (in->y > info->maxAaY)
			report->y = info->maxAaY;
		else if (in->y < info->minAaY)
			report->y = info->minAaY;
		else
			report->y = in->y;
		for (int j = 0; j < 8; j++)
			report->reserved[j] = 0;
	}
}

int emulateController(CtrlpEmulate *emulate, SceUInt32 version)
{
	SceUInt32 intr, port, buttons;

	state.version = version;
	getPortInfo();
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	buttons = normalizeButtons(emulate->data.buttons, version);
	if (state.version) {
		port = emulate->portType.port;
		emulate->portType.size = 0;
		emulate->portType.port = 0;
		emulate->port = port;
		if (emulate->portType.type &&
		    (port > 5 || emulate->portType.type != controllerType(port))) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
			return CTRLP_RESULT_TYPE_MISMATCH;
		}
	} else {
		port = emulate->port;
	}
	buttons &= state.buttonMask;
	if (ksceCtrlSetButtonEmulation(port, 0, buttons, buttons, CTRLP_EMULATION_TIME) < 0 ||
	    ksceCtrlSetAnalogEmulation(emulate->port, 0,
			emulate->data.lx, emulate->data.ly,
			emulate->data.rx, emulate->data.ry,
			emulate->data.up, emulate->data.right,
			emulate->data.down, emulate->data.left,
			CTRLP_EMULATION_TIME) < 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
		return CTRLP_RESULT_INVALID;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return CTRLP_RESULT_OK;
}

static inline int stopMode(SceUInt32 mode)
{
	SceUInt32 intr;
	int result;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (state.mode == mode) {
		state.mode = CTRLP_MODE_IDLE;
		result = CTRLP_RESULT_OK;
	} else {
		result = CTRLP_RESULT_ILLEGAL_STATE;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return result;
}

/* the host is gone: clear the rapid fire rules and stop recording or playback */
static int stopPlayback(void)
{
	for (int port = 0; port < CTRLP_PORT_NUM; port++) {
		for (int index = 0; index < 16; index++) {
			if (state.rapidFire[port] & (1 << index))
				ksceCtrlClearRapidFire(port, index);
		}
		state.rapidFire[port] = 0;
	}

	if (state.mode == CTRLP_MODE_RECORD)
		return stopMode(CTRLP_MODE_RECORD);
	if (state.mode == CTRLP_MODE_PLAYBACK)
		return stopMode(CTRLP_MODE_PLAYBACK);
	return CTRLP_RESULT_OK;
}

static inline void playController(struct ctrlNativeSample *sample, const CtrlpCtrlData *data)
{
	sample->sample.buttons = normalizeButtons(data->buttons, state.version) & state.buttonMask;
	sample->sample.lx = data->lx;
	sample->sample.ly = data->ly;
	sample->sample.rx = data->rx;
	sample->sample.ry = data->ry;
	sample->sample.up = data->up;
	sample->sample.right = data->right;
	sample->sample.down = data->down;
	sample->sample.left = data->left;
	for (int i = 0; i < 4; i++)
		sample->reserved[i] = 0;
}

static inline void saveController(CtrlpCtrlData *data, const struct ctrlNativeSample *sample)
{
	data->size = sizeof(*data);
	data->buttons = sample->sample.buttons & state.buttonMask;
	data->lx = sample->sample.lx;
	data->ly = sample->sample.ly;
	data->rx = sample->sample.rx;
	data->ry = sample->sample.ry;
	data->up = sample->sample.up;
	data->right = sample->sample.right;
	data->down = sample->sample.down;
	data->left = sample->sample.left;
}

/* SceCtrl's ctrlp callback: records a sample, or replaces it with the recording */
static int controllerCallback(SceUInt32 port, struct ctrlNativeSample *sample)
{
	CtrlpRecord *record;
	SceUInt32 intr, count, frame;
	SceUInt64 time;
	int played = 0;

	if (port != 0) {
		if (!state.version)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		if ((SceUInt8)port <= 5 && !isController(state.portInfo.port[(SceUInt8)port]))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (sample == NULL) {
		error(2510, "ILLEGAL_PARAM\n");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}

	if (state.mode == CTRLP_MODE_RECORD) {
		if (port >= CTRLP_PORT_NUM)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
		if (state.mode != CTRLP_MODE_RECORD || !(state.streams & CTRLP_STREAM_CTRL))
			goto out;
		record = &ring.record[ring.writeIndex];
		memset(record, 0, sizeof(*record));
		time = sceKernelGetSystemTimeWide() - state.baseTime;
		if (port == state.primaryPort)
			state.ctrlFrame++;
		record->header.port = port;
		record->header.kind = CTRLP_RECORD_CTRL;
		record->header.time = time;
		record->header.frame = state.ctrlFrame;
		record->header.size = sizeof(record->header);
		saveController(&record->ctrl, sample);
		record->header.size += record->ctrl.size;
		ringAdvanceWrite();
		checkRecords();
		goto out;
	}
	if (state.mode != CTRLP_MODE_PLAYBACK)
		return 0;

	if (port >= CTRLP_PORT_NUM) {
		error(2274, "ILLEGAL PARAM\n");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (state.mode != CTRLP_MODE_PLAYBACK || !(state.streams & CTRLP_STREAM_CTRL))
		goto out;
	time = sceKernelGetSystemTimeWide() - state.baseTime;
	if (port == state.primaryPort)
		state.ctrlFrame++;
	frame = state.ctrlFrame;
	for (count = ringCount(port); count != 0; count--) {
		record = &ring.record[ring.readIndex[port]];
		if (record->header.kind == CTRLP_RECORD_CTRL && record->header.port == port) {
			if (state.flags & CTRLP_SYNC_CTRL_FRAME) {
				if (record->header.frame > frame)
					break;
			} else if (record->header.time != 0 && record->header.time > time) {
				break;
			}
			played = 1;
			playController(sample, &record->ctrl);
		}
		ringAdvanceRead(port);
		/* the first port drives the ports without a controller */
		if (port == (state.dolce == 1)) {
			for (int i = 0; i < CTRLP_PORT_NUM; i++) {
				if (!isController(state.portInfo.port[i]))
					ring.readIndex[i] = ring.readIndex[port];
			}
			if (state.dolce == 1) {
				ring.readIndex[CTRLP_READER_TOUCH + SCE_TOUCH_PORT_FRONT] = ring.readIndex[port];
				ring.readIndex[CTRLP_READER_TOUCH + SCE_TOUCH_PORT_BACK] = ring.readIndex[port];
			}
		}
	}
	if (played) {
		saveController(&saved[port].ctrl, sample);
		if (port == 1)
			saveController(&saved[0].ctrl, sample);
	} else {
		playController(sample, &saved[port].ctrl);
	}
	checkSpace();
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return 0;
}

/* playback: takes records from the host into the ring */
int appendRecords(CtrlpPutRecords *put, SceUInt32 length, SceUInt32 *space,
		  SceUInt32 version)
{
	CtrlpRecord *record, *entry;
	SceUInt32 count, free, intr, port, inserted;

	if (length < sizeof(*put))
		return CTRLP_RESULT_INVALID;
	count = put->count;
	state.version = version;
	if (count == 0)
		return CTRLP_RESULT_OK;
	if (count > (version ? 32 : 8))
		return CTRLP_RESULT_INVALID;
	state.spaceThreshold = put->spaceThreshold;
	*space = 0;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (state.mode != CTRLP_MODE_PLAYBACK) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
		return CTRLP_RESULT_ILLEGAL_STATE;
	}
	free = freeRecords();
	if (free < count) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
		*space = free;
		state.spaceWanted = 1;
		return CTRLP_RESULT_NO_SPACE;
	}

	length -= sizeof(*put);
	record = (CtrlpRecord *)put->records;
	inserted = 0;
	while (length > sizeof(record->header) && record->header.size >= sizeof(record->header)) {
		if (record->header.kind == CTRLP_RECORD_CTRL) {
			port = record->header.port;
			if (version) {
				port = record->header.portType.port;
				record->header.portType.size = 0;
				record->header.portType.port = 0;
				record->header.port = port;
				if (record->header.portType.type &&
				    (port > 5 || record->header.portType.type != controllerType(port))) {
					ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
					return CTRLP_RESULT_TYPE_MISMATCH;
				}
				record->header.portType.type = 0;
			}
			if (port >= CTRLP_PORT_NUM)
				break;
		} else if (record->header.kind != CTRLP_RECORD_TOUCH ||
			   record->header.port >= SCE_TOUCH_PORT_MAX_NUM) {
			break;
		}

		entry = &ring.record[ring.writeIndex];
		entry->header = record->header;
		length -= sizeof(record->header);
		if (record->header.kind == CTRLP_RECORD_CTRL) {
			if (length < sizeof(record->ctrl) ||
			    record->header.size != record->ctrl.size + sizeof(record->header) ||
			    record->ctrl.size != sizeof(record->ctrl))
				break;
			entry->ctrl = record->ctrl;
			length -= sizeof(record->ctrl);
			record = (CtrlpRecord *)(&record->ctrl + 1);
		} else {
			if (length < sizeof(record->touch) ||
			    record->header.size != record->touch.size + sizeof(record->header) ||
			    record->touch.size != sizeof(record->touch))
				break;
			memcpy(&entry->touch, &record->touch, sizeof(record->touch));
			length -= sizeof(record->touch);
			record++;
		}
		inserted++;
		ringAdvanceWrite();
		if (inserted == count) {
			*space = free - inserted;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
			return CTRLP_RESULT_OK;
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return CTRLP_RESULT_INVALID;
}

static inline int startRecord(const CtrlpStartRecord *start, SceUInt32 length)
{
	SceUInt32 intr;

	if (length < sizeof(start->streams))
		return CTRLP_RESULT_INVALID;
	state.version = 0;
	if (length < sizeof(*start) || start->version == 0) {
		state.recordThreshold = 8;
	} else if (start->version == 1) {
		state.version = 1;
		state.recordThreshold = 32;
	} else {
		return CTRLP_RESULT_INVALID;
	}
	getPortInfo();
	findPrimaryPort();

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (state.mode != CTRLP_MODE_IDLE) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
		return CTRLP_RESULT_BUSY;
	}
	ring.writeIndex = 0;
	state.ctrlFrame = 0;
	state.touchFrame = 0;
	state.streams = start->streams;
	state.mode = CTRLP_MODE_RECORD;
	state.recordReady = 1;
	state.flags = 0;
	memset(ring.readIndex, 0, sizeof(ring.readIndex));
	state.baseTime = sceKernelGetSystemTimeWide();
	resetSaved();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return CTRLP_RESULT_OK;
}

static inline int startPlayback(const CtrlpStartPlayback *start, SceUInt32 length)
{
	SceUInt32 intr;

	if (length < sizeof(*start))
		return CTRLP_RESULT_INVALID;
	getPortInfo();
	findPrimaryPort();

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (state.mode != CTRLP_MODE_IDLE) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
		return CTRLP_RESULT_BUSY;
	}
	ring.writeIndex = 0;
	state.spaceWanted = 0;
	state.spaceThreshold = 0;
	state.mode = CTRLP_MODE_PLAYBACK;
	state.flags = start->flags;
	state.streams = start->streams;
	state.ctrlFrame = 0;
	state.touchFrame = 0;
	memset(ring.readIndex, 0, sizeof(ring.readIndex));
	resetSaved();
	state.baseTime = sceKernelGetSystemTimeWide();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return CTRLP_RESULT_OK;
}

static inline int setRapidFire(const CtrlpSetRapidFire *set, SceUInt32 length)
{
	SceCtrlRapidFireRule rule;
	SceUInt32 intr;
	int result;

	if (length < offsetof(CtrlpSetRapidFire, portType))
		return CTRLP_RESULT_INVALID;
	state.version = length >= sizeof(*set);
	if (set->port >= CTRLP_PORT_NUM)
		return CTRLP_RESULT_INVALID;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	rule = set->rule;
	if (ksceCtrlSetRapidFire(state.version ? set->portType.port : 0, set->index, &rule) < 0) {
		result = CTRLP_RESULT_INVALID;
	} else {
		state.rapidFire[set->port] |= 1 << set->index;
		result = CTRLP_RESULT_OK;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return result;
}

static inline int clearRapidFire(const CtrlpClearRapidFire *clear, SceUInt32 length)
{
	SceUInt32 intr;
	int result;

	if (length < offsetof(CtrlpClearRapidFire, portType))
		return CTRLP_RESULT_INVALID;
	state.version = length >= sizeof(*clear);
	if (clear->port >= CTRLP_PORT_NUM)
		return CTRLP_RESULT_INVALID;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (ksceCtrlClearRapidFire(state.version ? clear->portType.port : 0, clear->index) < 0) {
		result = CTRLP_RESULT_INVALID;
	} else {
		state.rapidFire[clear->port] &= ~(1 << clear->index);
		result = CTRLP_RESULT_OK;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return result;
}

static inline int getPanelInfo(const CtrlpGetPanelInfo *get, SceUInt32 length,
			       SceTouchPanelInfo *out)
{
	const SceTouchPanelInfo *info;

	if (length < sizeof(*get))
		return CTRLP_RESULT_INVALID;
	state.version = length >= 8;
	if (get->panel >= SCE_TOUCH_PORT_MAX_NUM)
		return CTRLP_RESULT_INVALID;

	info = &state.panelInfo[get->panel];
	memset(out, 0, sizeof(*out));
	out->minAaX = info->minAaX;
	out->minAaY = info->minAaY;
	out->maxAaX = info->maxAaX;
	out->maxAaY = info->maxAaY;
	out->minDispX = info->minDispX;
	out->minDispY = info->minDispY;
	out->maxDispX = info->maxDispX;
	out->maxDispY = info->maxDispY;
	out->minForce = info->minForce;
	out->maxForce = info->maxForce;
	return CTRLP_RESULT_OK;
}

static inline int btRegister(const CtrlpBtRegister *reg, SceUInt32 length,
			     CtrlpBtRegisterResult *out)
{
	int ret;

	memset(out, 0, sizeof(*out));
	if (length < sizeof(*reg) || length < reg->nameLength + sizeof(*reg) ||
	    reg->name[reg->nameLength] != '\0')
		return CTRLP_RESULT_INVALID;

	ret = SceBtForDriver_10D68FAE(reg->address, &reg->deviceClass, reg->pinCode,
		reg->vendorId, reg->productId, reg->deviceVersion, reg->name,
		out->address, out->linkKey);
	switch (ret) {
	case 1:
		return CTRLP_RESULT_OK;
	case BT_ERROR_REGISTER_1801:
		return CTRLP_RESULT_BT_ERROR_1801;
	case BT_ERROR_REGISTER_1802:
		return CTRLP_RESULT_BT_ERROR_1802;
	case BT_ERROR_REGISTER_INVALID:
		return CTRLP_RESULT_INVALID;
	case 0:
		return CTRLP_RESULT_BT_NOT_REGISTERED;
	default:
		return CTRLP_RESULT_FAILED;
	}
}

static inline int btDelete(const CtrlpBtDelete *del, SceUInt32 length)
{
	int ret;

	if (length < 8)
		return CTRLP_RESULT_INVALID;
	ret = ksceBtDeleteRegisteredInfo(del->mac0, del->mac1);
	if (ret == 0)
		return CTRLP_RESULT_OK;
	if (ret == SCE_BT_ERROR_REG_DELETE_CONNECTING)
		return CTRLP_RESULT_BT_CONNECTING;
	return CTRLP_RESULT_BT_DELETE_FAILED;
}

static inline int getControllerPortInfo(CtrlpPortInfoResult *out)
{
	SceCtrlPortInfo info;

	out->count = CTRLP_PORT_NUM;
	if (ksceCtrlGetControllerPortInfo(&info) != 0)
		return CTRLP_RESULT_FAILED;
	for (int port = 0; port < CTRLP_PORT_NUM; port++) {
		CtrlpPortType *entry = &out->port[port];

		entry->size = sizeof(*entry);
		entry->port = port;
		switch (info.port[port]) {
		case SCE_CTRL_TYPE_DS3:
			entry->type = CTRLP_DEVICE_DS3;
			break;
		case SCE_CTRL_TYPE_DS4:
			entry->type = CTRLP_DEVICE_DS4;
			break;
		case SCE_CTRL_TYPE_PHY:
			entry->type = CTRLP_DEVICE_BUILTIN;
			break;
		default:
			entry->type = CTRLP_DEVICE_NONE;
			break;
		}
	}
	return CTRLP_RESULT_OK;
}

/* runs the received command and queues its response */
static inline int processRequest(void)
{
	CtrlpPacket *packet = state.recvBuffer;
	SceUInt32 length = packet->header.length - sizeof(packet->header);
	SceUInt32 code = packet->header.code;
	CtrlpResult out;
	SceUInt32 space = 0, intr;
	int result;

	switch (code) {
	case CTRLP_CMD_START_RECORD:
		result = startRecord(&packet->startRecord, length);
		break;
	case CTRLP_CMD_STOP_RECORD:
		result = stopMode(CTRLP_MODE_RECORD);
		break;
	case CTRLP_CMD_START_PLAYBACK:
		result = startPlayback(&packet->startPlayback, length);
		break;
	case CTRLP_CMD_STOP_PLAYBACK:
		result = stopMode(CTRLP_MODE_PLAYBACK);
		break;
	case CTRLP_CMD_PUT_RECORDS:
	case CTRLP_CMD_PUT_RECORDS2:
		result = appendRecords(&packet->putRecords, length, &space,
			code == CTRLP_CMD_PUT_RECORDS2);
		break;
	case CTRLP_CMD_EMULATE:
	case CTRLP_CMD_EMULATE2:
		if (length < offsetof(CtrlpEmulate, portType) ||
		    packet->emulate.data.size != sizeof(packet->emulate.data)) {
			result = CTRLP_RESULT_INVALID;
			break;
		}
		result = emulateController(&packet->emulate, code == CTRLP_CMD_EMULATE2);
		break;
	case CTRLP_CMD_SET_RAPID_FIRE:
		result = setRapidFire(&packet->setRapidFire, length);
		break;
	case CTRLP_CMD_CLEAR_RAPID_FIRE:
		result = clearRapidFire(&packet->clearRapidFire, length);
		break;
	case CTRLP_CMD_GET_PANEL_INFO:
		result = getPanelInfo(&packet->getPanelInfo, length, &out.panelInfo.info);
		break;
	case CTRLP_CMD_BT_REGISTER:
		result = btRegister(&packet->btRegister, length, &out.btRegister);
		break;
	case CTRLP_CMD_BT_DELETE:
		result = btDelete(&packet->btDelete, length);
		break;
	case CTRLP_CMD_GET_PORT_INFO:
		result = getControllerPortInfo(&out.portInfo);
		break;
	default:
		result = CTRLP_RESULT_UNKNOWN_COMMAND;
		break;
	}

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (state.pending & CTRLP_SEND_RESPONSE) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
		error(934, "dup resp req 0x%02X 0x%02X\n", code, state.responseHeader.code);
		return DFMGR_ERROR_QUEUE_FULL;
	}
	state.responseHeader.sequence = packet->header.sequence;
	state.responseHeader.code = code + 1;
	state.responseHeader.length = sizeof(state.responseHeader) + sizeof(out.result);
	state.response.result = result;
	switch (code) {
	case CTRLP_CMD_PUT_RECORDS:
	case CTRLP_CMD_PUT_RECORDS2:
		state.responseHeader.length = sizeof(state.responseHeader) + sizeof(out.putRecords);
		state.response.putRecords.space = space;
		break;
	case CTRLP_CMD_GET_PANEL_INFO:
		state.responseHeader.length = sizeof(state.responseHeader) + sizeof(out.panelInfo);
		state.response.panelInfo.info = out.panelInfo.info;
		break;
	case CTRLP_CMD_BT_REGISTER:
		state.responseHeader.length = sizeof(state.responseHeader) + sizeof(out.btRegister);
		memcpy(state.response.btRegister.address, out.btRegister.address,
			sizeof(out.btRegister.address));
		memcpy(state.response.btRegister.linkKey, out.btRegister.linkKey,
			sizeof(out.btRegister.linkKey));
		break;
	case CTRLP_CMD_GET_PORT_INFO:
		state.responseHeader.length = sizeof(state.responseHeader) + sizeof(out.portInfo);
		state.response.portInfo.count = CTRLP_PORT_NUM;
		memcpy(state.response.portInfo.port, out.portInfo.port, sizeof(out.portInfo.port));
		break;
	default:
		break;
	}
	requestSend(CTRLP_SEND_RESPONSE);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return 0;
}

static inline void finishPacket(CtrlpPacket *packet)
{
	SceDeci4pDfMgrForDebugger_6D26CC56(&packet->deci4p,
		sizeof(packet->deci4p) + packet->header.length);
}

/* recording: fills the send buffer with the records collected so far */
static inline SceUInt32 buildRecords(void)
{
	CtrlpPacket *packet = state.sendBuffer;
	SceUInt8 *out = packet->records.records;
	SceUInt32 count, size = 0;

	count = ringCount(0);
	if (count > (state.version ? 32 : 8))
		count = state.version ? 32 : 8;
	for (SceUInt32 i = 0; i < count; i++) {
		const CtrlpRecord *record = &ring.record[ring.readIndex[0]];
		CtrlpRecord *dest = (CtrlpRecord *)out;
		SceUInt32 n = sizeof(dest->header);

		memset(&dest->header, 0, sizeof(dest->header));
		dest->header = record->header;
		if (record->header.kind == CTRLP_RECORD_CTRL) {
			dest->ctrl = record->ctrl;
			if (state.version) {
				dest->header.portType.size = sizeof(dest->header.portType);
				dest->header.portType.port = record->header.port;
				dest->header.portType.type = controllerType(record->header.port);
				dest->header.port = 0;
			}
			n += sizeof(dest->ctrl);
		} else if (record->header.kind == CTRLP_RECORD_TOUCH) {
			memcpy(&dest->touch, &record->touch, sizeof(dest->touch));
			if (state.version) {
				dest->header.portType.size = sizeof(dest->header.portType);
				dest->header.portType.port = 0;
				dest->header.portType.type = CTRLP_DEVICE_BUILTIN;
			}
			n += sizeof(dest->touch);
		}
		ringAdvanceRead(0);
		/* the ports without a controller follow port 0 */
		if (dest->header.kind == CTRLP_RECORD_CTRL && state.dolce != 1) {
			for (int port = 1; port < CTRLP_PORT_NUM; port++) {
				if (!isController(state.portInfo.port[port]))
					ring.readIndex[port] = ring.readIndex[0];
			}
		}
		size += n;
		out += n;
	}
	packet->header.code = state.version ? CTRLP_NOTIFY_RECORDS2 : CTRLP_NOTIFY_RECORDS;
	packet->records.count = count;
	packet->header.length = sizeof(packet->header) + sizeof(packet->records) + size;
	packet->header.sequence = state.sequence++;
	finishPacket(packet);
	return sizeof(packet->deci4p) + packet->header.length;
}

static inline int recvPacket(SceSize size)
{
	SceUInt8 *buffer = (SceUInt8 *)state.recvBuffer;
	SceUInt32 intr;
	int ret, left;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (state.pending & CTRLP_SEND_RESPONSE) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
		state.recvCount = 0;
		return DFMGR_ERROR_QUEUE_FULL;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);

	if ((int)size > (int)state.recvSize) {
		error(1078, "data size error[%d]\n", size);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	for (left = size; left > 0; left -= ret) {
		ret = sceDfMgrRecv(state.handle, buffer, left);
		if (ret < 0) {
			error(1085, "sceDfMgrRecv err %d\n", ret);
			return ret;
		}
		if (ret > left)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		buffer += ret;
	}
	state.recvCount = size;
	return 0;
}

static inline int sendPacket(void)
{
	CtrlpPacket *packet = state.sendBuffer;
	SceUInt8 *buffer;
	SceUInt32 intr, what, size;
	int ret;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (state.sending) {
		error(1212, "Already sending\n");
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
		return SCE_KERNEL_ERROR_ILLEGAL_MODE;
	}
	if (state.pending & CTRLP_SEND_RECORDS)
		what = CTRLP_SEND_RECORDS;
	else if (state.pending & CTRLP_SEND_SPACE)
		what = CTRLP_SEND_SPACE;
	else
		what = state.pending & CTRLP_SEND_RESPONSE;
	state.sendCount = 0;

	switch (what) {
	case CTRLP_SEND_RECORDS:
		size = buildRecords();
		break;
	case CTRLP_SEND_SPACE:
		packet->header.code = CTRLP_NOTIFY_SPACE;
		packet->header.length = sizeof(packet->header) + sizeof(packet->space);
		packet->header.sequence = state.sequence++;
		packet->space = state.space;
		finishPacket(packet);
		size = sizeof(packet->deci4p) + packet->header.length;
		break;
	case CTRLP_SEND_RESPONSE:
		packet->header = state.responseHeader;
		memcpy(&packet->response, &state.response,
			state.responseHeader.length - sizeof(state.responseHeader));
		finishPacket(packet);
		size = sizeof(packet->deci4p) + packet->header.length;
		break;
	default:
		error(1234, "Unknown send event %08X\n", what);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	state.pending &= ~what;
	state.sending = what;
	state.sendCount = size;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);

	buffer = (SceUInt8 *)packet;
	do {
		ret = sceDfMgrSend(state.handle, buffer, size);
		if (ret < 0) {
			error(1260, "sceDfMgrSend error %d\n", ret);
			return ret;
		}
		size -= ret;
		buffer += ret;
	} while (size != 0);
	return 0;
}

static inline int sentPacket(void)
{
	SceUInt32 intr;
	int ret;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (state.sending == CTRLP_SEND_RECORDS)
		state.recordReady = 1;
	state.sending = 0;
	if (state.pending) {
		ret = sceDfMgrReqSend(state.handle);
		if (ret < 0)
			error(1292, "sceDfMgrReqSend error %d\n", ret);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return 0;
}

static int protocolHandler(int event, void *arg, void *ctx)
{
	const SceDfMgrStatus *status = arg;

	(void)ctx;
	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		return recvPacket((SceSize)arg);
	case SCE_DFMGR_EVENT_REQ_DONE:
		if (state.recvCount < offsetof(CtrlpPacket, startRecord))
			return 0;
		return processRequest();
	case SCE_DFMGR_EVENT_SEND:
		return sendPacket();
	case SCE_DFMGR_EVENT_SENT:
		return sentPacket();
	case SCE_DFMGR_EVENT_STATUS:
		if (status->kind == 0) {
			if (!status->connected)
				stopPlayback();
		} else if (status->kind != 1) {
			return 0;
		}
		if (!status->enabled)
			stopPlayback();
		return 0;
	case 6:		/* meaning unknown; it carries a status packet */
		if (status->kind == 1 || status->kind == 2)
			stopPlayback();
		return 0;
	default:
		return 0;
	}
}

static inline void saveTouch(CtrlpTouchData *data, const SceTouchData *sample)
{
	data->size = sizeof(*data);
	data->status = sample->status;
	data->reportNum = sample->reportNum >= CTRLP_TOUCH_REPORT_NUM ?
		CTRLP_TOUCH_REPORT_NUM : sample->reportNum;
	for (int i = 0; i < CTRLP_TOUCH_REPORT_NUM; i++) {
		CtrlpTouchReport *report = &data->report[i];

		report->size = sizeof(*report);
		report->id = sample->report[i].id;
		report->force = sample->report[i].force;
		report->x = sample->report[i].x;
		report->y = sample->report[i].y;
		report->reserved[0] = 0;
		report->reserved[1] = 0;
	}
}

/* SceTouch's ctrlp callback: records a sample, or replaces it with the recording */
static int touchCallback(SceUInt32 panel, SceTouchData *sample)
{
	CtrlpRecord *record;
	SceUInt32 intr, reader, count, frame;
	SceUInt64 time;
	int played = 0;

	if (state.dolce == 1)
		return 0;
	if (panel >= SCE_TOUCH_PORT_MAX_NUM) {
		error(2524, "ILLEGAL_PARAM\n");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (sample == NULL) {
		error(2528, "ILLEGAL_PARAM\n");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}

	if (state.mode == CTRLP_MODE_RECORD) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
		if (state.mode != CTRLP_MODE_RECORD || !(state.streams & CTRLP_STREAM_TOUCH))
			goto out;
		record = &ring.record[ring.writeIndex];
		memset(record, 0, sizeof(*record));
		time = sceKernelGetSystemTimeWide() - state.baseTime;
		if (panel == SCE_TOUCH_PORT_FRONT)
			state.touchFrame++;
		record->header.frame = state.touchFrame;
		record->header.time = time;
		record->header.size = sizeof(record->header);
		record->header.kind = CTRLP_RECORD_TOUCH;
		record->header.port = panel;
		saveTouch(&record->touch, sample);
		record->header.size += record->touch.size;
		ringAdvanceWrite();
		checkRecords();
		goto out;
	}
	if (state.mode != CTRLP_MODE_PLAYBACK)
		return 0;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state.lock);
	if (state.mode != CTRLP_MODE_PLAYBACK || !(state.streams & CTRLP_STREAM_TOUCH))
		goto out;
	time = sceKernelGetSystemTimeWide() - state.baseTime;
	if (panel == SCE_TOUCH_PORT_FRONT)
		state.touchFrame++;
	frame = state.touchFrame;
	reader = CTRLP_READER_TOUCH + panel;
	for (count = ringCount(reader); count != 0; count--) {
		record = &ring.record[ring.readIndex[reader]];
		if (record->header.kind == CTRLP_RECORD_TOUCH && record->header.port == panel) {
			if (state.flags & CTRLP_SYNC_TOUCH_FRAME) {
				if (record->header.frame > frame)
					break;
			} else if (record->header.time != 0 && record->header.time > time) {
				break;
			}
			played = 1;
			applyTouch(sample, &record->touch, panel);
		}
		ringAdvanceRead(reader);
	}
	if (played)
		saveTouch(&saved[reader].touch, sample);
	else
		applyTouch(sample, &saved[reader].touch, panel);
	checkSpace();
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state.lock, intr);
	return 0;
}

int module_start(SceSize args, void *argp)
{
	int ret;

	(void)args;
	(void)argp;
	if (!ksceKernelCheckDipsw(CTRLP_DIPSW))
		return SCE_KERNEL_START_NO_RESIDENT;

	memset(&state, 0, sizeof(state));
	ring.writeIndex = 0;
	memset(ring.readIndex, 0, sizeof(ring.readIndex));
	state.recvSize = sizeof(recvBuffer);
	state.recvBuffer = (CtrlpPacket *)recvBuffer;
	state.sendSize = sizeof(sendBuffer);
	state.sendBuffer = (CtrlpPacket *)sendBuffer;
	state.recordThreshold = 32;
	ksceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &state.panelInfo[SCE_TOUCH_PORT_FRONT]);
	ksceTouchGetPanelInfo(SCE_TOUCH_PORT_BACK, &state.panelInfo[SCE_TOUCH_PORT_BACK]);
	state.buttonMask = 0x31ffff;
	state.dolce = ksceSblAimgrIsDolce() != 0;

	ret = ksceCtrlSetCtrlpCallback(controllerCallback);
	if (ret != 0) {
		error(1428, "sceCtrlSetCtrlpCallback error %d\n", ret);
		return ret < 0 ? SCE_KERNEL_START_FAILED : SCE_KERNEL_START_SUCCESS;
	}
	ret = ksceTouchSetCtrlpCallback(touchCallback);
	if (ret != 0) {
		error(1433, "sceTouchSetCtrlpCallback error %d\n", ret);
		return ret < 0 ? SCE_KERNEL_START_FAILED : SCE_KERNEL_START_SUCCESS;
	}

	protocol.size = sizeof(protocol);
	protocol.protocol = CTRLP_PROTOCOL;
	protocol.alias = 0;
	protocol.version = 0x01020000;
	protocol.subVersion = 0x00010000;
	protocol.destination = 0;
	protocol.flags = DFMGR_PROTOCOL_ENABLE;
	memset(protocol.name, 0, sizeof(protocol.name));
	strncpy(protocol.name, "CTRLP", sizeof(protocol.name));
	protocol.flags |= ksceKernelCheckDipsw(CTRLP_DIPSW_AUTO_ENABLE) ?
			  DFMGR_PROTOCOL_SDIO : DFMGR_PROTOCOL_USB;
	ret = sceDfMgrOpen(protocolHandler, &protocol, NULL);
	if (ret < 0) {
		error(1456, "Cannot Open socket %d\n", ret);
		return SCE_KERNEL_START_FAILED;
	}
	state.lock = 0;
	state.handle = ret;
	return SCE_KERNEL_START_SUCCESS;
}
