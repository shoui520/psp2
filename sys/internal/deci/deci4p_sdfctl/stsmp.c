/*
 * STSMP: target status to the host. The kernel reports boot parameters and the
 * target status through the senders registered with sysroot; the packets queue
 * (3 deep) for the worker thread, which asks SDfCtl to send them. Received
 * packets are discarded.
 */

#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysevent.h>
#include <sysmem/sysroot.h>
#include "sdfctl.h"

#define STSMP_QUEUE		3
#define STSMP_PACKET_MAX	0x100
#define STSMP_PROTOCOL		0x10002000

#define STSMP_EVF_SEND		0x1

/* message types / codes */
#define STSMP_BOOT_PARAM	5
#define STSMP_BOOT_PARAM_SET	4
#define STSMP_TARGET_STATUS	6
#define STSMP_TARGET_STATUS_SET	2

/* boot parameter: a name and an integer value */
typedef struct StsmpBootParam {
	DcmpHeader m;
	SceUInt32 id[2];			/* TSMP's request id */
	SceUInt16 nameTag;			/* 1 */
	SceUInt16 reserved;
	SceUInt32 nameSize;			/* the padded name + 0x10 */
	SceUInt32 nameLen;			/* with the NUL */
	char data[0xd8];			/* the padded name, then StsmpValue */
} StsmpBootParam;

typedef struct StsmpValue {
	SceUInt16 tag;				/* 4 */
	SceUInt16 reserved;
	SceUInt32 value;
} StsmpValue;

typedef struct StsmpTargetStatus {
	DcmpHeader m;
	SceUInt32 id[2];			/* TSMP's request id */
	SceUInt32 status;
} StsmpTargetStatus;

typedef struct StsmpQueue {
	int write;
	int read;
	int count;
	void *buf[STSMP_QUEUE];
	SceSize size[STSMP_QUEUE];
} StsmpQueue;

static struct {
	SceUID thread;
	SceUID evf;
	SceUID sock;
	DcmpHeader *rx;
	StsmpQueue tx;
} s_stsmp;

static SceKernelSpinlock s_stsmpLock;
static SceUInt8 s_rxBuf[STSMP_PACKET_MAX];
static SceUInt8 s_txBuf[STSMP_QUEUE][STSMP_PACKET_MAX];

/* the rest of a received packet */
static inline __attribute__((always_inline)) void _discardPacket(DcmpHeader *rx,
							      const void *lr)
{
	SceSize rest, n;
	int ret;

	rest = sdfctlGetSize(rx);
	SCE_DBG_ASSERT_CHECK_LR(rest < 0x11000, 339, lr);
	rest -= 0x1c;
	while (rest != 0) {
		n = rest > 0xfff ? 0x1000 : rest;
		ret = sceSDfCtlExRecv(s_stsmp.sock, (char *)rx + sizeof(SceDeci4pHeader), n);
		if (ret < 0) {
			SDFCTL_PRINTF(349, "error: 0x%08x\n", ret);
			return;
		}
		rest -= ret;
	}
}

static inline __attribute__((always_inline)) void _stsmpRecv(const void *lr)
{
	int ret;

	ret = sceSDfCtlExRecv(s_stsmp.sock, s_stsmp.rx, 0x1c);
	if (ret < 0) {
		SDFCTL_PRINTF(311, "error: 0x%08x\n", ret);
		return;
	}
	_discardPacket(s_stsmp.rx, lr);
}

static inline __attribute__((always_inline)) void _stsmpSend(void)
{
	DcmpHeader *pkt;
	SceSize size;
	int intr, ret, read, next, count;

	if (s_stsmp.tx.count == 0)
		return;
	for (;;) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_stsmpLock);
		if (s_stsmp.tx.count < 1)
			break;
		pkt = s_stsmp.tx.buf[s_stsmp.tx.read];
		size = s_stsmp.tx.size[s_stsmp.tx.read];
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_stsmpLock, intr);
		if (pkt->hdr.dst == SDFCTL_ROUTE_LOCAL || sdfctlTestStatus(SDFCTL_STATUS_CONNECTED)) {
			ret = sceSDfCtlExSend(s_stsmp.sock, pkt, size);
			if (ret < 0) {
				SDFCTL_PRINTF(286, "error: 0x%08x\n", ret);
				return;
			}
		} else {
			SDFCTL_PRINTF(280, "no route (0x%04x)\n", pkt->hdr.dst);
		}
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_stsmpLock);
		read = s_stsmp.tx.read;
		count = s_stsmp.tx.count - 1;
		next = read + 1;
		if (next > STSMP_QUEUE - 1)
			next = 0;
		s_stsmp.tx.read = next;
		s_stsmp.tx.size[read] = 0;
		s_stsmp.tx.count = count;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_stsmpLock, intr);
		if (s_stsmp.tx.count == 0)
			return;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_stsmpLock, intr);
	SDFCTL_PRINTF(270, "error: 0x%08x\n", SDFCTL_ERROR_QUEUE_EMPTY);
}

static void _stsmpHandler(int event, SceUIntPtr arg, void *ctx)
{
	const void *lr = __builtin_return_address(0);

	switch (event) {
	case SDFCTL_EVENT_RECV:
		_stsmpRecv(lr);
		break;
	case SDFCTL_EVENT_RECV_DONE:
	case SDFCTL_EVENT_SEND_DONE:
	case SDFCTL_EVENT_STATUS:
	case SDFCTL_EVENT_6:
		break;
	case SDFCTL_EVENT_SEND:
		_stsmpSend();
		break;
	default:
		SDFCTL_PRINTF(253, "unknown event (0x%08x)\n", event);
		break;
	}
}

static int _workerThread(SceSize args, void *argp)
{
	int ret;

	for (;;) {
		ret = sceKernelWaitEventFlag(s_stsmp.evf, STSMP_EVF_SEND,
					    SCE_KERNEL_EVF_WAITMODE_OR |
					    SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
		if (ret < 0)
			SDFCTL_PRINTF(377, "WaitEventFlag failed (0x%08X)\n", ret);
		else
			sdfctlReqSend(s_stsmp.sock);
	}
	return 0;
}

/* name unknown: queue a packet for the worker */
static __attribute__((noinline, noclone)) int stsmpQueue(const void *pkt, SceSize size)
{
	int intr, next, count;

	if (size > STSMP_PACKET_MAX)
		return SDFCTL_ERROR_INVALID_ARG;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_stsmpLock);
	if (s_stsmp.tx.count > STSMP_QUEUE - 1) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_stsmpLock, intr);
		return SDFCTL_ERROR_QUEUE_FULL;
	}
	sdfctlMemcpy(s_stsmp.tx.buf[s_stsmp.tx.write], pkt, size);
	next = s_stsmp.tx.write + 1;
	count = s_stsmp.tx.count + 1;
	if (next < STSMP_QUEUE)
		s_stsmp.tx.write = next;
	s_stsmp.tx.size[next - 1] = size;
	s_stsmp.tx.count = count;
	if (next > STSMP_QUEUE - 1)
		s_stsmp.tx.write = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_stsmpLock, intr);
	return sceKernelSetEventFlag(s_stsmp.evf, STSMP_EVF_SEND);
}

/* (SceSysrootForDriver_E541959B) a boot parameter (0 / 1) to the host */
static int setBootParameter(const char *key, SceUInt32 value)
{
	StsmpBootParam pkt;
	StsmpValue *v;
	SceSize len, padded, size;
	int ret;

	if (key == NULL) {
		SDFCTL_PRINTF(420, "invalid key\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (value > 1) {
		SDFCTL_PRINTF(425, "invalid value (%d)\n", value);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	len = strnlen(key, 0x7f);
	padded = (len + 4) & ~3;
	size = padded + 0x30;
	sdfctlMemset(&pkt, 0, size);
	pkt.nameSize = padded + 0x10;
	pkt.nameLen = len + 1;
	pkt.nameTag = 1;
	sdfctlMemcpy(pkt.data, key, len);
	v = (StsmpValue *)&pkt.data[padded];
	v->tag = 4;
	v->value = value;
	v->reserved = 0;
	pkt.m.code = STSMP_BOOT_PARAM_SET;
	pkt.m.type = STSMP_BOOT_PARAM;
	sdfctlSetSize(&pkt, size);
	pkt.m.hdr.dst = SDFCTL_ROUTE_TARGET;
	ret = stsmpQueue(&pkt, size);
	if (ret < 0) {
		SDFCTL_PRINTF(515, "error: 0x%08x\n", ret);
		return ret;
	}
	return 0;
}

/* (SceSysrootForDriver_D56A2F3E) the target status (0 only) to the host */
static int sendTargetStatus(SceUInt32 status)
{
	StsmpTargetStatus pkt;
	int ret;

	if (status != 0) {
		SDFCTL_PRINTF(532, "invalid status (0x%08x)\n", status);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	sdfctlMemset(&pkt, 0, sizeof(pkt));
	pkt.m.type = STSMP_TARGET_STATUS;
	pkt.m.code = STSMP_TARGET_STATUS_SET;
	pkt.status = status;
	sdfctlSetSize(&pkt, sizeof(pkt));
	pkt.m.hdr.dst = SDFCTL_ROUTE_TARGET;
	ret = stsmpQueue(&pkt, sizeof(pkt));
	if (ret < 0) {
		SDFCTL_PRINTF(564, "error: 0x%08x\n", ret);
		return ret;
	}
	return 0;
}

/* name unknown: sys event handler (event 0x102 before a cold reset: report the target status) */
static int stsmpSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	const struct PowerSuspendEvent *suspend = args;

	if (resume != 0)
		return 0;
	if (eventid == SYSEVENT_SUSPEND_START && suspend->sleepMode == POWER_SLEEP_COLD_RESET)
		return sendTargetStatus(0);
	return 0;
}

int stsmpInit(SDfCtlPd *pd)
{
	const void *lr = __builtin_return_address(0);
	SceDfMgrProtocol desc;
	SceUID uid;
	int ret;

	sdfctlMemset(&s_stsmp, 0, sizeof(s_stsmp));
	s_stsmpLock = 0;
	s_stsmp.sock = 0xffe;
	desc.size = sizeof(desc);
	desc.protocol = STSMP_PROTOCOL;
	desc.alias = 0;
	desc.version = 0x1000000;
	desc.subVersion = 0;
	desc.destination = SDFCTL_ROUTE_TARGET;
	desc.flags = DFMGR_PROTOCOL_ENABLE;
	strncpy(desc.name, "STSMP", sizeof(desc.name));
	pd->handler = _stsmpHandler;
	pd->desc = desc;
	s_stsmp.rx = (DcmpHeader *)s_rxBuf;
	pd->ctx = NULL;
	pd->sock = 0xffe;
	pd->flags = 0;
	/* (Sony's: the buffers are STSMP_QUEUE bytes apart, not a packet) */
	s_stsmp.tx.buf[0] = (SceUInt8 *)s_txBuf;
	s_stsmp.tx.buf[1] = (SceUInt8 *)s_txBuf + STSMP_QUEUE;
	s_stsmp.tx.buf[2] = (SceUInt8 *)s_txBuf + 2 * STSMP_QUEUE;

	uid = sceKernelCreateThread("SceDeci4pSDfCtl", _workerThread, 0x1e, 0x2000, 0, 0x80000, NULL);
	if (uid < 0)
		return uid;
	s_stsmp.thread = uid;
	uid = sceKernelCreateEventFlag("SceDeci4pSDfCtl", SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (uid < 0) {
		SDFCTL_PRINTF(139, "CreateEventFlag failed (0x%08X)\n", uid);
		return uid;
	}
	s_stsmp.evf = uid;
	SceSysrootForDriver_E541959B(setBootParameter);
	/* sysroot passes a second word, which the sender ignores */
	SceSysrootForDriver_D56A2F3E((SysrootSendTargetStatus)sendTargetStatus);
	ret = sdfctlSetProtocolStatus(pd, SDFCTL_PD_REGISTERED, 1);
	if (ret < 0) {
		SDFCTL_PRINTF(151, "SetProtocolStatus failed (0x%08x)\n", ret);
		SCE_DBG_ASSERT_CHECK_LR(0, 152, lr);
		return ret;
	}
	ret = sdfctlSetProtocolStatus(pd, SDFCTL_PD_STARTED, 1);
	if (ret < 0) {
		SDFCTL_PRINTF(157, "SetProtocolStatus failed (0x%08x)\n", ret);
		SCE_DBG_ASSERT_CHECK_LR(0, 158, lr);
		return ret;
	}
	sceKernelRegisterSysEventHandler("SceDeci4pSDfCtl", stsmpSysEventHandler, NULL);
	ret = sceKernelStartThread(s_stsmp.thread, 0, NULL);
	return ret < 0 ? ret : 0;
}
