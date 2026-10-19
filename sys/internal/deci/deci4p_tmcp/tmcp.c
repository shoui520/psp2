/*
 * SceDeci4pTmcp: DECI4P protocol 0x2E000 ("TMCP"), commands from the target
 * to the host's target manager (dipsw 0xC2 enables it).
 *
 * The command handler is installed in sysroot (SceSysrootForDriver_1834D482).
 * Each calling thread gets one of 32 slots. A command is one packet (code
 * 0x10, sequence number, an argument and three strings); the host answers
 * with an acknowledgement (0x11: status, value, id) and later a completion
 * (0x81: id, error, value). Synchronous callers wait for both. Asynchronous
 * callers return after the acknowledgement; the completion is copied to the
 * caller's process and a user callback is notified.
 */

#include <sysmem/dipsw.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/procevent.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <deci4p_dfmgr/deci4p.h>
#include <kernel/error.h>
#include <sysmem/sysroot.h>
#include <threadmgr/threadmgr.h>

#define TMCP_PROTOCOL		0x2e000
#define TMCP_SLOTS		32
#define TMCP_PKT_SIZE		0xc24
#define TMCP_STR_MAX		0x3ff
#define TMCP_SEQ_MAX		0x7ffe

/* packet codes */
#define TMCP_CMD_REQUEST	0x10
#define TMCP_CMD_ACK		0x11
#define TMCP_CMD_DONE		0x81

/* acknowledgement status */
#define TMCP_STATUS_OK		0
#define TMCP_STATUS_7		7
#define TMCP_STATUS_2E001	0x2e001
#define TMCP_STATUS_2E002	0x2e002

/* event flag bits */
#define EVF_ACK			0x1
#define EVF_DONE		0x2
#define EVF_LINK_DOWN		0x80000000

#define EVF_ATTR_UNK8000	0x8000		/* meaning unknown */

typedef SceUInt32 __attribute__((may_alias)) TmcpWord;

typedef struct TmcpSlot {
	SceUInt8 used;
	SceUInt8 reserved;
	SceUInt8 abandoned;			/* the caller is gone */
	SceUInt8 notified;			/* async completion delivered */
	SceUID pid;
	SceUID tid;
	SceUInt32 acked;
	SceUInt32 ack[3];			/* status, value, id */
	SceUInt32 done;
	SceUInt32 result[3];			/* id, error, value */
	SceUID callback;			/* async */
	void *user;				/* async: completion record; sync: result */
	SceUInt32 async;
	SceUID evf;
} TmcpSlot;

typedef struct TmcpPacket {
	SceDeci4pHeader hdr;
	SceUInt8 code;
	SceUInt8 reserved;			/* sent as 0 */
	SceUInt16 seq;
	SceUInt32 arg;
	/* then up to three strings: SceUInt32 length (with NUL), bytes padded to 4 */
} TmcpPacket;

typedef struct TmcpLink {
	int handle;				/* sceDfMgrOpen */
	SceUInt32 connected;
	SceUID mutex;
	SceUInt16 seq;
	SceUID evf;
	SceUInt32 busy;				/* a command is being sent */
	TmcpSlot *cur;				/* its slot */
	union {
		TmcpPacket pkt;
		SceUInt8 raw[TMCP_PKT_SIZE];
	};
} TmcpLink;

typedef struct TmcpReply {
	SceUInt8 code;
	SceUInt8 reserved;
	SceUInt16 seq;
} TmcpReply;

static TmcpSlot s_slot[TMCP_SLOTS];
static TmcpLink s_link;

static int TmcpProcStop(SceUID pid, int type, ProcEventParam *param, void *common)
{
	return 0;
}

static int TmcpProcStart(SceUID pid, int type, ProcEventParam *param, void *common)
{
	return 0;
}

/* name unknown: deliver an async completion to the caller's process */
static inline __attribute__((always_inline)) void TmcpDeliverCompletion(TmcpSlot *slot,
		SceUInt32 error, SceUInt32 id, SceUInt32 value)
{
	SceUInt32 rec[8];

	rec[0] = error;
	rec[1] = id;
	rec[2] = value;
	rec[3] = 0;
	rec[4] = 0;
	rec[5] = 0;
	rec[6] = 0;
	rec[7] = 0;

	sceKernelCopyToUserProc(slot->pid, slot->user, rec, sizeof(rec));
	sceKernelNotifyCallback(slot->callback, (int)slot->user);
	slot->notified = 1;
	if (slot->abandoned) {
		if (slot == s_link.cur)
			s_link.cur = NULL;
		sceKernelDeleteEventFlag(slot->evf);
		memset(slot, 0, sizeof(*slot));
		slot->pid = -1;
		slot->callback = -1;
		slot->evf = -1;
	}
}

static inline __attribute__((always_inline)) void TmcpFreeSlot(TmcpSlot *slot)
{
	sceKernelDeleteEventFlag(slot->evf);
	memset(slot, 0, sizeof(*slot));
	slot->pid = -1;
	slot->callback = -1;
	slot->evf = -1;
}

/* name unknown: process exit / kill; drop the process' slots */
static inline __attribute__((always_inline)) int TmcpReleaseProcessSlots(SceUID pid, TmcpLink *link)
{
	TmcpSlot *slot;
	int i = 0;

	sceKernelLockMutex(link->mutex, 1, NULL);
	while (i < TMCP_SLOTS) {
		slot = &s_slot[i++];
		if (!slot->used || pid != slot->pid)
			continue;
		if (slot == s_link.cur)
			s_link.cur = NULL;
		TmcpFreeSlot(slot);
		i = 0;
	}
	sceKernelUnlockMutex(link->mutex, 1);
	return 0;
}

static int TmcpProcExit(SceUID pid, ProcEventParam *param, void *common)
{
	return TmcpReleaseProcessSlots(pid, common);
}

static void __attribute__((noinline)) TmcpNotify(TmcpSlot *slot, SceUInt32 error,
					       SceUInt32 id, SceUInt32 value)
{
	TmcpDeliverCompletion(slot, error, id, value);
}

static int TmcpProcKill(SceUID pid, ProcEventParam *param, void *common)
{
	return TmcpReleaseProcessSlots(pid, common);
}

static const SceKernelProcEventHandler s_procHandler = {
	.size = sizeof(SceKernelProcEventHandler),
	.exit = TmcpProcExit,
	.kill = TmcpProcKill,
	.suspend = TmcpProcStop,
	.resume = TmcpProcStart,
};

/* name unknown: an acknowledgement or completion from the host */
static int TmcpRecv(TmcpLink *link)
{
	SceDeci4pHeader hdr;
	TmcpReply reply;
	SceUInt32 w[3];
	TmcpSlot *slot, *cur;
	SceUInt32 busy;
	int ret;

	if ((ret = sceDfMgrRecv(link->handle, &hdr, sizeof(hdr))) < 0)
		return ret;
	if ((ret = sceDfMgrRecv(link->handle, &reply, sizeof(reply))) < 0)
		return ret;

	if (reply.code == TMCP_CMD_ACK) {
		busy = link->busy;
		cur = link->cur;
		if ((ret = sceDfMgrRecv(link->handle, w, sizeof(w))) < 0)
			return ret;
		sceKernelLockMutex(link->mutex, 1, NULL);
		link->busy = 0;
		link->cur = NULL;
		sceKernelSetEventFlag(link->evf, EVF_ACK);
		if (busy && cur != NULL && link->pkt.seq == reply.seq) {
			cur->acked = 1;
			cur->ack[0] = w[0];
			cur->ack[1] = w[1];
			cur->ack[2] = w[2];
			sceKernelSetEventFlag(cur->evf, EVF_ACK);
		}
	} else if (reply.code == TMCP_CMD_DONE) {
		if ((ret = sceDfMgrRecv(link->handle, w, sizeof(w))) < 0)
			return ret;
		sceKernelLockMutex(link->mutex, 1, NULL);
		for (slot = s_slot; slot != &s_slot[TMCP_SLOTS]; slot++) {
			if (!slot->used || !slot->acked || slot->ack[0] != TMCP_STATUS_OK ||
			    w[0] != slot->ack[2] || slot->done)
				continue;
			slot->done = 1;
			slot->result[0] = w[0];
			slot->result[1] = w[1];
			slot->result[2] = w[2];
			sceKernelSetEventFlag(slot->evf, EVF_DONE);
			if (slot->used && slot->async && slot->acked && slot->ack[0] == TMCP_STATUS_OK) {
				if (w[1] == 0) {
					if (!slot->notified)
						TmcpNotify(slot, 0, w[0], w[2]);
				} else if (!slot->notified) {
					TmcpNotify(slot, SCE_KERNEL_ERROR_DECI4P_UNKNOWN, w[0], -1);
				}
			}
			break;
		}
	} else {
		return 0;
	}

	sceKernelUnlockMutex(link->mutex, 1);
	return 0;
}

/* name unknown: protocol status from DfMgr */
static int TmcpStatus(TmcpLink *link, const SceDfMgrStatus *st)
{
	TmcpSlot *slot;

	sceKernelLockMutex(link->mutex, 1, NULL);
	if (st->kind == 1) {
		if (st->enabled == 0) {
			link->connected = 0;
			link->busy = 0;
			link->cur = NULL;
			sceKernelSetEventFlag(link->evf, EVF_LINK_DOWN);
			for (slot = s_slot; slot != &s_slot[TMCP_SLOTS]; slot++) {
				if (!slot->used)
					continue;
				sceKernelSetEventFlag(slot->evf, EVF_LINK_DOWN);
				if (slot->used && slot->async && slot->acked &&
				    slot->ack[0] == TMCP_STATUS_OK && !slot->notified)
					TmcpDeliverCompletion(slot, SCE_KERNEL_ERROR_DECI4P_INTERRUPTED,
							      slot->ack[2], -1);
			}
		} else if (st->enabled == 1) {
			link->connected = 1;
			sceKernelClearEventFlag(link->evf, 0);
			sceKernelSetEventFlag(link->evf, EVF_ACK);
		}
	}
	sceKernelUnlockMutex(link->mutex, 1);
	return 0;
}

/* name unknown: DfMgr event handler */
static int TmcpHandler(int event, void *arg, void *ctx)
{
	TmcpLink *link = ctx;
	int ret;

	switch (event) {
	case SCE_DFMGR_EVENT_SEND:
		ret = sceDfMgrSend(link->handle, &link->pkt,
				   SceDeci4pDfMgrForDebugger_CACAB5F9(&link->pkt.hdr));
		return ret < 0 ? ret : 0;
	case SCE_DFMGR_EVENT_STATUS:
		return TmcpStatus(link, arg);
	case SCE_DFMGR_EVENT_RECV:
		return TmcpRecv(link);
	}
	return 0;
}

/* append a string: length word, then the bytes padded to 4 */
static inline __attribute__((always_inline)) void TmcpPutString(const char *s)
{
	SceSize off, n, pad, len, i;
	SceUInt8 *dst;
	SceUInt8 c;

	off = SceDeci4pDfMgrForDebugger_CACAB5F9(&s_link.pkt.hdr);
	n = strnlen(s, TMCP_STR_MAX);
	pad = (n + 4) & ~3;
	len = n + 1;
	if (off + 4 + pad > TMCP_PKT_SIZE)
		return;

	SceDeci4pDfMgrForDebugger_6D26CC56(&s_link.pkt.hdr, off + 4 + pad);
	*(TmcpWord *)&s_link.raw[off] = len;
	dst = &s_link.raw[off + 4];
	i = 0;
	c = *s;
	if (c && pad) {
		do {
			dst[i++] = c;
			c = *++s;
		} while (c && i < pad);
	}
	if (i < pad)
		memset(&dst[i], 0, pad - i);
}

/* name unknown: the command handler installed in sysroot */
static int TmcpCommand(SceUInt32 arg, const char *s1, const char *s2, const char *s3,
		       SceUInt32 *id, SceUInt32 *value, SceUID callback, void *user, int async)
{
	SceUID pid, tid, evf;
	SceUInt32 bits;
	TmcpSlot *slot;
	int ret, seq;

	pid = sceKernelGetProcessId();
	tid = sceKernelGetThreadId();
	if (sceKernelLockMutex(s_link.mutex, 1, NULL) < 0)
		return SCE_KERNEL_ERROR_DECI4P_UNKNOWN;

	/* a command of this thread is still pending: wait for it */
	for (slot = s_slot; slot != &s_slot[TMCP_SLOTS]; slot++)
		if (slot->used && tid == slot->tid)
			goto wait_ack;

	for (slot = s_slot; slot->used; )
		if (++slot == &s_slot[TMCP_SLOTS]) {
			ret = SCE_KERNEL_ERROR_DECI4P_HOST_TOOMANY_PROCESS;
			goto out;
		}
	evf = sceKernelCreateEventFlag("SceDeci4pTmcpRcv",
					SCE_KERNEL_ATTR_TH_PRIO | EVF_ATTR_UNK8000, 0, NULL);
	if (evf < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
		goto out;
	}
	slot->evf = evf;
	if (async) {
		slot->pid = pid;
		slot->tid = tid;
		slot->callback = callback;
		slot->user = user;
		slot->async = async;
	} else {
		slot->user = NULL;
		slot->async = 0;
		slot->pid = pid;
		slot->tid = tid;
		slot->callback = -1;
	}
	slot->used = 1;

	ret = 0;
	if (!s_link.connected)
		goto no_link;
	while (s_link.busy && s_link.connected) {
		sceKernelUnlockMutex(s_link.mutex, 1);
		ret = sceKernelWaitEventFlag(s_link.evf, EVF_LINK_DOWN | EVF_ACK,
					      SCE_KERNEL_EVF_WAITMODE_OR,
					      &bits, NULL);
		sceKernelLockMutex(s_link.mutex, 1, NULL);
		if (ret < 0)
			break;
	}
	if (ret == SCE_KERNEL_ERROR_THREAD_SUSPENDED)
		goto abandon;
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
		goto abandon;
	}
	if (!s_link.connected)
		goto no_link;

	sceKernelClearEventFlag(s_link.evf, ~EVF_ACK);
	s_link.busy = 1;
	s_link.cur = slot;
	SceDeci4pDfMgrForDebugger_6D26CC56(&s_link.pkt.hdr, 0x18);
	seq = s_link.seq;
	s_link.pkt.code = TMCP_CMD_REQUEST;
	s_link.pkt.reserved = 0;
	s_link.pkt.seq = seq;
	s_link.seq = (SceUInt16)(seq + 1) > TMCP_SEQ_MAX ? 0 : seq + 1;
	s_link.pkt.arg = arg;
	TmcpPutString(s1 != NULL ? s1 : "");
	TmcpPutString(s2 != NULL ? s2 : "");
	TmcpPutString(s3 != NULL ? s3 : "");
	ret = sceDfMgrReqSend(s_link.handle);
	if (ret == DFMGR_ERROR_NOT_CONNECTED)
		goto no_link;
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
		goto abandon;
	}

wait_ack:
	sceKernelUnlockMutex(s_link.mutex, 1);
	ret = sceKernelWaitEventFlag(slot->evf, EVF_LINK_DOWN | EVF_ACK,
				      SCE_KERNEL_EVF_WAITMODE_OR, &bits, NULL);
	sceKernelLockMutex(s_link.mutex, 1, NULL);
	if (ret == SCE_KERNEL_ERROR_THREAD_SUSPENDED)
		goto out;
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
		goto abandon;
	}
	if ((int)bits < 0 && !slot->acked)
		goto no_link;

	switch (slot->ack[0]) {
	case TMCP_STATUS_OK:
		*id = slot->ack[2];
		ret = 0;
		break;
	case TMCP_STATUS_7:
		ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PARAM;
		break;
	case TMCP_STATUS_2E001:
		ret = SCE_KERNEL_ERROR_DECI4P_HOST_PROCESS_DISABLED;
		break;
	case TMCP_STATUS_2E002:
		ret = SCE_KERNEL_ERROR_DECI4P_HOST_CREATE_PROCESS;
		*value = slot->ack[1];
		break;
	default:
		ret = SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
		break;
	}

	if (ret == 0 && !async) {
		sceKernelUnlockMutex(s_link.mutex, 1);
		ret = sceKernelWaitEventFlag(slot->evf, EVF_LINK_DOWN | EVF_DONE,
					      SCE_KERNEL_EVF_WAITMODE_OR,
					      &bits, NULL);
		sceKernelLockMutex(s_link.mutex, 1, NULL);
		if (ret == SCE_KERNEL_ERROR_THREAD_SUSPENDED)
			goto out;
		if (ret < 0) {
			ret = SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
			goto abandon;
		}
		if ((int)bits < 0) {
			ret = SCE_KERNEL_ERROR_DECI4P_INTERRUPTED;
			goto abandon;
		}
		if (slot->result[1] != 0) {
			ret = SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
			goto abandon;
		}
		*(SceUInt32 *)user = slot->result[2];
	}
	if (ret == 0 && async)
		slot->tid = -1;			/* the slot now waits for the completion */
	goto abandon;

no_link:
	ret = SCE_KERNEL_ERROR_DECI4P_NO_CONNECT;
abandon:
	slot->abandoned = 1;
	if (slot->used && slot->async && slot->acked && slot->ack[0] == TMCP_STATUS_OK) {
		/* the completion is still to be delivered */
		if (!slot->notified)
			goto out;
	}
	if (slot == s_link.cur)
		s_link.cur = NULL;
	TmcpFreeSlot(slot);
out:
	sceKernelUnlockMutex(s_link.mutex, 1);
	return ret;
}

int module_start(SceSize args, void *argp)
{
	SceDfMgrProtocol proto;
	SceUID mutex;
	TmcpSlot *slot;
	int handle;

	if (!sceKernelCheckDipsw(0xc2))
		return SCE_KERNEL_START_NO_RESIDENT;

	mutex = sceKernelCreateMutex("SceDeci4pTmcpComm", SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
	if (mutex < 0)
		return SCE_KERNEL_START_FAILED;
	s_link.mutex = mutex;
	s_link.seq = 0;
	s_link.evf = sceKernelCreateEventFlag("SceDeci4pTmcpCmd",
					       SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_EVF_ATTR_MULTI |
					       EVF_ATTR_UNK8000, EVF_ACK, NULL);
	for (slot = s_slot; slot != &s_slot[TMCP_SLOTS]; slot++) {
		slot->pid = -1;
		slot->tid = -1;
		slot->callback = -1;
		slot->evf = -1;
	}
	s_link.handle = -1;
	s_link.connected = 0;

	proto.size = sizeof(proto);
	proto.protocol = TMCP_PROTOCOL;
	proto.alias = 0;
	proto.version = 0;
	proto.subVersion = 0;
	proto.destination = 0;
	proto.flags = 0;
	strncpy(proto.name, "TMCP", sizeof(proto.name));
	handle = sceDfMgrOpen(TmcpHandler, &proto, &s_link);
	if (handle < 0)
		return SCE_KERNEL_START_FAILED;
	s_link.handle = handle;

	SceSysrootForDriver_1834D482(TmcpCommand);
	sceKernelRegisterProcEventHandler("SceDeci4pTmcp", &s_procHandler, &s_link);
	return SCE_KERNEL_START_SUCCESS;
}
