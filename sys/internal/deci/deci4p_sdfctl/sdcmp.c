/*
 * SDCMP: the SDfCtl manager protocol. The host connects / disconnects, starts and
 * stops the other protocols, echoes, and controls the power (suspend, standby,
 * cold reset); the target reports its protocols and power state. Outgoing packets
 * wait in a ring of 15 until the interface can send.
 */

#include <sysmem/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/debug.h>
#include <sysmem/suspend.h>
#include "sdfctl.h"
#include <deci4p_sdfmgr/deci4p_memory.h>
#include <deci4p_dfmgr/deci4p_packet.h>

#define SDCMP_QUEUE		15
#define SDCMP_PACKET_MAX	0x100

/* message types / codes */
#define SDCMP_CONNECT		0
#define SDCMP_CONNECT_UP	0
#define SDCMP_CONNECT_DOWN	1
#define SDCMP_CONNECT_START	4
#define SDCMP_CONNECT_STOP	5
#define SDCMP_ECHO		1
#define SDCMP_STATUS		2
#define SDCMP_STATUS_CONNECTION	0
#define SDCMP_STATUS_PROTOCOL	1
#define SDCMP_STATUS_ALIAS	2
#define SDCMP_STATUS_POWER	4
#define SDCMP_CONTROL		4
#define SDCMP_CONTROL_POWER	2
#define SDCMP_CONTROL_RESULT	3

/* power control operations */
#define SDCMP_POWER_STANDBY	0
#define SDCMP_POWER_SUSPEND	0x200
#define SDCMP_POWER_COLD_RESET	0x8000

typedef struct SdcmpProtocolStatus {
	DcmpHeader m;
	SceUInt32 protocol;
	SceUInt32 version;			/* the descriptor's */
	SceUInt32 subVersion;
	SceUInt16 cpu;				/* 0xa200 + (1 << cpu) */
	SceUInt8 reserved0[2];
	SceUInt8 status;
	SceUInt8 reserved1[3];
} SdcmpProtocolStatus;

typedef struct SdcmpAliasStatus {
	DcmpHeader m;
	SceUInt32 protocol;
	SceUInt32 alias;			/* the descriptor's */
} SdcmpAliasStatus;

typedef struct SdcmpPowerStatus {
	DcmpHeader m;
	SceUInt8 kind;
	SceUInt8 state;
	SceUInt8 arg;				/* kind 0x80 / 0x81, state 3 */
	SceUInt8 arg2;				/* kind 0, state 2 */
} SdcmpPowerStatus;

typedef struct SdcmpConnectionStatus {
	DcmpHeader m;
	SceUInt8 status;
	SceUInt8 padding[3];
} SdcmpConnectionStatus;

typedef struct SdcmpEcho {
	DcmpHeader m;
	SceUInt16 id;
	SceUInt16 seq;				/* the reply's is one more */
	SceUInt8 data[0x20];
} SdcmpEcho;

typedef struct SdcmpControl {
	DcmpHeader m;
	SceUInt32 value;			/* operation / result */
} SdcmpControl;

typedef struct SdcmpQueue {
	int write;
	int read;
	int count;
	void *buf[SDCMP_QUEUE];
	SceSize size[SDCMP_QUEUE];
} SdcmpQueue;

static SceUInt8 s_rxBuf[SDCMP_PACKET_MAX];
static SceUInt8 s_txBuf[SDCMP_QUEUE][SDCMP_PACKET_MAX];

static struct {
	SceUID sock;
	DcmpHeader *rx;
	SdcmpQueue tx;
} s_sdcmp;

static SceKernelSpinlock s_sdcmpLock;

/* name unknown: memcpy */
__attribute__((noinline)) void sdfctlMemcpy(void *dst, const void *src, SceSize n)
{
	deci4pCopyBytes(dst, src, n);
}

/* name unknown: the packet's 24-bit size */
__attribute__((noinline)) SceSize sdfctlGetSize(const void *hdr)
{
	return deci4pGetSize(hdr);
}

/* name unknown */
__attribute__((noinline)) void sdfctlSetSize(void *hdr, SceSize size)
{
	deci4pSetSize(hdr, size);
}

/* name unknown: queue a packet for sending */
static __attribute__((noinline, noclone)) int sdcmpQueue(SdcmpQueue *q, const void *pkt, SceSize size)
{
	int intr, next, count, ret;

	if (size > SDCMP_PACKET_MAX)
		return SDFCTL_ERROR_INVALID_ARG;
	intr = SceCpuForDriver_D32ACE9E(&s_sdcmpLock);
	if (q->count < SDCMP_QUEUE) {
		sdfctlMemcpy(q->buf[q->write], pkt, size);
		next = q->write + 1;
		count = q->count + 1;
		q->size[q->write] = size;
		q->count = count;
		q->write = next > SDCMP_QUEUE - 1 ? 0 : next;
		ret = 0;
	} else {
		ret = SDFCTL_ERROR_QUEUE_FULL;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdcmpLock, intr);
	return ret;
}

/* the rest of an unknown packet */
static void _discardPacket(DcmpHeader *rx)
{
	const void *lr = __builtin_return_address(0);
	SceSize rest, n;
	int ret;

	rest = sdfctlGetSize(rx);
	SCE_DBG_ASSERT_CHECK_LR(rest < 0x11000, 586, lr);
	rest -= sizeof(DcmpHeader);
	while (rest != 0) {
		n = rest < 0x1000 ? rest : 0x1000;
		ret = sceSDfCtlExRecv(s_sdcmp.sock, (char *)rx + sizeof(SceDeci4pHeader), n);
		if (ret < 0) {
			SDFCTL_PRINTF(596, "error: 0x%08x\n", ret);
			return;
		}
		rest -= ret;
	}
}

static inline __attribute__((always_inline))
void _sdcmpEcho(DcmpHeader *rx, SceUInt8 type, SdcmpEcho *reply)
{
	SdcmpEcho *req = (SdcmpEcho *)rx;
	SceSize size;
	int ret;

	size = sdfctlGetSize(rx);
	if (size > sizeof(SdcmpEcho))
		return;
	ret = sceSDfCtlExRecv(s_sdcmp.sock, &req->id, size - sizeof(DcmpHeader));
	if (ret < 0) {
		SDFCTL_PRINTF(338, "error: 0x%08x\n", ret);
		return;
	}
	sdfctlMemset(reply, 0, sizeof(*reply));
	sdfctlSetSize(reply, size);
	reply->id = req->id;
	reply->m.hdr.dst = SDFCTL_ROUTE_LOCAL;
	reply->seq = req->seq + 1;
	reply->m.reserved = 0;
	reply->m.type = type;
	reply->m.code = type;
	/* (copies 4 bytes more than the payload) */
	sdfctlMemcpy(reply->data, req->data, size - sizeof(DcmpHeader));
	ret = sdcmpQueue(&s_sdcmp.tx, reply, size);
	if (ret < 0) {
		SDFCTL_PRINTF(376, "error: 0x%08x\n", ret);
		return;
	}
	sceSDfCtlExReqSend(s_sdcmp.sock);
}

static inline __attribute__((always_inline))
void _sdcmpControlPowerControl(DcmpHeader *rx, SdcmpControl *reply)
{
	SdcmpControl *req = (SdcmpControl *)rx;
	SceUInt32 result;
	SceSize size;
	int ret;

	size = sdfctlGetSize(rx);
	if (size > sizeof(SdcmpControl))
		return;
	ret = sceSDfCtlExRecv(s_sdcmp.sock, &req->value, size - sizeof(DcmpHeader));
	if (ret < 0) {
		SDFCTL_PRINTF(406, "error: 0x%08x\n", ret);
		return;
	}
	switch (req->value) {
	case SDCMP_POWER_SUSPEND:
		result = 0;
		SceDebugForDriver_391B74B7("Suspend from Host\n");
		SceKernelSuspendForDriver_0106C0F0();
		break;
	case SDCMP_POWER_COLD_RESET:
		result = 0;
		SceDebugForDriver_391B74B7("Cold Reset from Host\n");
		SceKernelSuspendForDriver_AEA9440D();
		break;
	case SDCMP_POWER_STANDBY:
		SceDebugForDriver_391B74B7("Standby from Host\n");
		SceKernelSuspendForDriver_0A6CA124();
		result = 0;
		break;
	default:
		result = 7;
		SDFCTL_PRINTF(427, "unknown operation (0x%08x)\n", req->value);
		break;
	}
	sdfctlMemset(reply, 0, sizeof(*reply));
	sdfctlSetSize(reply, size);
	reply->m.reserved = 0;
	reply->m.hdr.dst = SDFCTL_ROUTE_LOCAL;
	reply->m.type = SDCMP_CONTROL;
	reply->m.code = SDCMP_CONTROL_RESULT;
	reply->value = result;
	ret = sdcmpQueue(&s_sdcmp.tx, reply, size);
	if (ret < 0) {
		SDFCTL_PRINTF(449, "error: 0x%08x\n", ret);
		return;
	}
	sceSDfCtlExReqSend(s_sdcmp.sock);
}

static inline __attribute__((always_inline)) void _sdcmpLinkStart(DcmpHeader *rx)
{
	int ret;

	if (sdfctlGetSize(rx) != sizeof(DcmpHeader) + 4)
		return;
	ret = sceSDfCtlExRecv(s_sdcmp.sock, rx + 1, 4);
	if (ret < 0) {
		SDFCTL_PRINTF(533, "error: 0x%08x\n", ret);
		return;
	}
	sdfctlStartProtocol(*(SceUInt32 *)(rx + 1));
}

static inline __attribute__((always_inline)) void _sdcmpLinkStop(DcmpHeader *rx)
{
	int ret;

	if (sdfctlGetSize(rx) != sizeof(DcmpHeader) + 4)
		return;
	ret = sceSDfCtlExRecv(s_sdcmp.sock, rx + 1, 4);
	if (ret < 0) {
		SDFCTL_PRINTF(564, "error: 0x%08x\n", ret);
		return;
	}
	sdfctlStopProtocol(*(SceUInt32 *)(rx + 1));
}

static inline __attribute__((always_inline))
void _sdcmpRecv(SdcmpControl *control, SdcmpEcho *echo)
{
	DcmpHeader *rx;
	int ret;

	ret = sceSDfCtlExRecv(s_sdcmp.sock, s_sdcmp.rx, sizeof(DcmpHeader));
	rx = s_sdcmp.rx;
	if (ret < 0) {
		SDFCTL_PRINTF(252, "error: 0x%08x\n", ret);
		return;
	}
	switch (rx->type) {
	case SDCMP_ECHO:
		if (rx->code == 0) {
			_sdcmpEcho(rx, rx->type, echo);
		} else {
			SDFCTL_PRINTF(291, "unknown packet (type:ECHO, code:%02x)\n", s_sdcmp.rx->code);
			_discardPacket(s_sdcmp.rx);
		}
		break;
	case SDCMP_CONNECT:
		switch (s_sdcmp.rx->code) {
		case SDCMP_CONNECT_UP:
			if (sdfctlGetSize(s_sdcmp.rx) == sizeof(DcmpHeader)) {
				sdfctlSetStatus(SDFCTL_STATUS_CONNECTED, 1);
				sdfctlConnect();
			}
			break;
		case SDCMP_CONNECT_DOWN:
			if (sdfctlGetSize(s_sdcmp.rx) == sizeof(DcmpHeader)) {
				sdfctlSetStatus(SDFCTL_STATUS_CONNECTED, 0);
				sdfctlDisconnect();
			}
			break;
		case SDCMP_CONNECT_START:
			_sdcmpLinkStart(rx);
			break;
		case SDCMP_CONNECT_STOP:
			_sdcmpLinkStop(rx);
			break;
		default:
			SDFCTL_PRINTF(279, "unknown packet (type:CONNECT, code:%02x)\n", s_sdcmp.rx->code);
			_discardPacket(s_sdcmp.rx);
			break;
		}
		break;
	case SDCMP_CONTROL:
		if (s_sdcmp.rx->code == SDCMP_CONTROL_POWER) {
			_sdcmpControlPowerControl(rx, control);
		} else {
			SDFCTL_PRINTF(304, "unknown packet (type:CONTROL, code:%02x)\n", s_sdcmp.rx->code);
			_discardPacket(s_sdcmp.rx);
		}
		break;
	default:
		SDFCTL_PRINTF(311, "unknown packet (type:%02x, code:%02x)\n", rx->type, s_sdcmp.rx->code);
		_discardPacket(s_sdcmp.rx);
		break;
	}
}

static inline __attribute__((always_inline)) void _sdcmpSend(void)
{
	DcmpHeader *pkt;
	SceSize size;
	int intr, ret, read, next, count;

	while (s_sdcmp.tx.count != 0) {
		intr = SceCpuForDriver_D32ACE9E(&s_sdcmpLock);
		if (s_sdcmp.tx.count < 1) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdcmpLock, intr);
			SDFCTL_PRINTF(213, "error: 0x%08x\n", SDFCTL_ERROR_QUEUE_EMPTY);
			return;
		}
		pkt = s_sdcmp.tx.buf[s_sdcmp.tx.read];
		size = s_sdcmp.tx.size[s_sdcmp.tx.read];
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdcmpLock, intr);
		if (pkt->hdr.dst == SDFCTL_ROUTE_LOCAL || sdfctlTestStatus(SDFCTL_STATUS_CONNECTED)) {
			ret = sceSDfCtlExSend(s_sdcmp.sock, pkt, size);
			if (ret < 0) {
				SDFCTL_PRINTF(229, "error: 0x%08x\n", ret);
				return;
			}
		} else {
			SDFCTL_PRINTF(223, "no route (0x%04x)\n", pkt->hdr.dst);
		}
		intr = SceCpuForDriver_D32ACE9E(&s_sdcmpLock);
		read = s_sdcmp.tx.read;
		count = s_sdcmp.tx.count - 1;
		next = read + 1;
		if (next > SDCMP_QUEUE - 1)
			next = 0;
		s_sdcmp.tx.read = next;
		s_sdcmp.tx.size[read] = 0;
		s_sdcmp.tx.count = count;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdcmpLock, intr);
	}
}

static void _sdcmpHandler(int event, SceUIntPtr arg, void *ctx)
{
	struct {
		SdcmpControl control;
		SdcmpEcho echo;
	} reply;

	switch (event) {
	case SDFCTL_EVENT_RECV:
		_sdcmpRecv(&reply.control, &reply.echo);
		break;
	case SDFCTL_EVENT_RECV_DONE:
	case SDFCTL_EVENT_SEND_DONE:
	case SDFCTL_EVENT_STATUS:
	case SDFCTL_EVENT_6:
		break;
	case SDFCTL_EVENT_SEND:
		_sdcmpSend();
		break;
	default:
		SDFCTL_PRINTF(191, "unknown event (0x%08x)\n", event);
		break;
	}
}

int sdcmpInit(SDfCtlPd *pd)
{
	const void *lr = __builtin_return_address(0);
	SceDfMgrProtocol desc;
	int i, ret;

	sdfctlMemset(&s_sdcmp, 0, sizeof(s_sdcmp));
	s_sdcmp.sock = 0xfff;
	s_sdcmpLock = 0;
	desc.size = sizeof(desc);
	desc.protocol = SDFCTL_PROTOCOL_MGR;
	desc.alias = 0;
	desc.version = 0x1000000;
	desc.subVersion = 0;
	desc.destination = SDFCTL_ROUTE_TARGET;
	desc.flags = DFMGR_PROTOCOL_ENABLE;
	strncpy(desc.name, "SDCMP", sizeof(desc.name));
	pd->handler = _sdcmpHandler;
	pd->desc = desc;
	s_sdcmp.rx = (DcmpHeader *)s_rxBuf;
	pd->ctx = NULL;
	pd->sock = 0xfff;
	pd->flags = 0;
	/* (Sony's: the buffers are SDCMP_QUEUE bytes apart, not a packet) */
	for (i = 0; i != SDCMP_QUEUE; i++)
		s_sdcmp.tx.buf[i] = (SceUInt8 *)s_txBuf + i * SDCMP_QUEUE;

	ret = sdfctlSetProtocolStatus(pd, SDFCTL_PD_REGISTERED, 1);
	if (ret < 0) {
		SDFCTL_PRINTF(121, "SetProtocolStatus failed (0x%08x)\n", ret);
		SCE_DBG_ASSERT_CHECK_LR(0, 122, lr);
		return ret;
	}
	ret = sdfctlSetProtocolStatus(pd, SDFCTL_PD_STARTED, 1);
	if (ret < 0) {
		SDFCTL_PRINTF(127, "SetProtocolStatus failed (0x%08x)\n", ret);
		SCE_DBG_ASSERT_CHECK_LR(0, 128, lr);
		return ret;
	}
	return 0;
}

/* report a protocol's status (SDFCTL_PD_* bits, 4: aliased) */
void sdcmpSendStatusProtocol(SceUInt32 status, const SceDfMgrProtocol *desc)
{
	SdcmpProtocolStatus pkt;
	SceUInt32 protocol, version, subVersion;
	int cpu, ret;

	if (status & ~7) {
		SDFCTL_PRINTF(618, "unknown status (0x%08x)\n", status);
		return;
	}
	sdfctlMemset(&pkt, 0, sizeof(pkt));
	sdfctlSetSize(&pkt, sizeof(pkt));
	protocol = desc->protocol;
	version = desc->version;
	subVersion = desc->subVersion;
	pkt.subVersion = subVersion;
	pkt.m.hdr.dst = SDFCTL_ROUTE_TARGET;
	pkt.protocol = protocol;
	pkt.version = version;
	pkt.m.code = SDCMP_STATUS_PROTOCOL;
	pkt.m.reserved = 0;
	pkt.m.type = SDCMP_STATUS;
	cpu = sceKernelCpuId();
	pkt.status = status;
	pkt.cpu = (1 << cpu) + 0xa200;
	ret = sdcmpQueue(&s_sdcmp.tx, &pkt, sizeof(pkt));
	if (ret < 0) {
		SDFCTL_PRINTF(655, "error: 0x%08x\n", ret);
		return;
	}
	sceSDfCtlExReqSend(s_sdcmp.sock);
	sdfctlPoll();
}

void sdcmpSendStatusProtocolAlias(const SceDfMgrProtocol *desc)
{
	SdcmpAliasStatus pkt;
	int ret;

	sdfctlMemset(&pkt, 0, sizeof(pkt));
	sdfctlSetSize(&pkt, sizeof(pkt));
	pkt.protocol = desc->protocol;
	pkt.alias = desc->alias;
	pkt.m.type = SDCMP_STATUS;
	pkt.m.code = SDCMP_STATUS_ALIAS;
	pkt.m.hdr.dst = SDFCTL_ROUTE_TARGET;
	pkt.m.reserved = 0;
	ret = sdcmpQueue(&s_sdcmp.tx, &pkt, sizeof(pkt));
	if (ret < 0) {
		SDFCTL_PRINTF(699, "error: 0x%08x\n", ret);
		return;
	}
	sceSDfCtlExReqSend(s_sdcmp.sock);
	sdfctlPoll();
}

void sdcmpSendStatusPower(int kind, int state, int arg2)
{
	SdcmpPowerStatus pkt;
	int ret;

	sdfctlMemset(&pkt, 0, sizeof(pkt));
	sdfctlSetSize(&pkt, sizeof(pkt));
	if (kind != 0 || state != 2)
		arg2 = 0;
	pkt.m.reserved = 0;
	pkt.kind = kind;
	pkt.state = state;
	pkt.m.hdr.dst = SDFCTL_ROUTE_LOCAL;
	pkt.m.type = SDCMP_STATUS;
	pkt.m.code = SDCMP_STATUS_POWER;
	pkt.arg2 = arg2;
	ret = sdcmpQueue(&s_sdcmp.tx, &pkt, sizeof(pkt));
	if (ret < 0) {
		SDFCTL_PRINTF(750, "error: 0x%08x\n", ret);
		return;
	}
	sceSDfCtlExReqSend(s_sdcmp.sock);
	sdfctlPoll();
}

int sdcmpMakeStatusConnection(SceUInt32 status, void *buf)
{
	SdcmpConnectionStatus *pkt = buf;

	if (pkt == NULL) {
		SDFCTL_PRINTF(806, "invalid arg\n");
		return SDFCTL_ERROR_INVALID_ARG;
	}
	if (status > 1) {
		SDFCTL_PRINTF(812, "unknown status (0x%08x)\n", status);
		return SDFCTL_ERROR_INVALID_ARG;
	}
	sdfctlMemset(pkt, 0, sizeof(*pkt));
	sdfctlSetSize(pkt, sizeof(*pkt));
	pkt->status = status;
	pkt->m.hdr.protocol = SDFCTL_PROTOCOL_MGR;
	pkt->m.code = SDCMP_STATUS_CONNECTION;
	pkt->m.reserved = 0;
	pkt->m.hdr.version = SCE_DECI4P_VERSION;
	pkt->m.hdr.src = SDFCTL_PORT;
	pkt->m.hdr.dst = SDFCTL_ROUTE_HOST;
	pkt->m.type = SDCMP_STATUS;
	return 0;
}

int sdcmpMakeStatusProtocol(SceUInt32 status, const SceDfMgrProtocol *desc, void *buf)
{
	SdcmpProtocolStatus *pkt = buf;
	SceUInt32 protocol, version, subVersion;
	int cpu;

	if (desc == NULL || pkt == NULL) {
		SDFCTL_PRINTF(845, "invalid arg\n");
		return SDFCTL_ERROR_INVALID_ARG;
	}
	if (status > 1) {
		SDFCTL_PRINTF(851, "unknown status (0x%08x)\n", status);
		return SDFCTL_ERROR_INVALID_ARG;
	}
	sdfctlMemset(pkt, 0, sizeof(*pkt));
	sdfctlSetSize(pkt, sizeof(*pkt));
	pkt->m.hdr.version = SCE_DECI4P_VERSION;
	pkt->m.code = SDCMP_STATUS_PROTOCOL;
	pkt->m.type = SDCMP_STATUS;
	protocol = desc->protocol;
	version = desc->version;
	subVersion = desc->subVersion;
	pkt->m.reserved = 0;
	pkt->m.hdr.protocol = SDFCTL_PROTOCOL_MGR;
	pkt->protocol = protocol;
	pkt->version = version;
	pkt->subVersion = subVersion;
	pkt->m.hdr.src = SDFCTL_PORT;
	pkt->m.hdr.dst = SDFCTL_ROUTE_HOST;
	cpu = sceKernelCpuId();
	pkt->status = status;
	pkt->cpu = (1 << cpu) + 0xa200;
	return 0;
}
