/*
 * SceDeci4pSDrfp: the DRFP channel. A request is built in a resource's packet
 * buffer and handed to SceDeci4pSDfMgr; the event handler sends it, receives
 * the response fragments into the same buffer (one DRFP_FRAG_SIZE slot per
 * fragment) and releases the waiting requester.
 *
 * Resource 0 (one slot) carries open / close / getstat, resource 1 reads.
 */

#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/qafmgr.h>
#include <deci4p_sdfmgr/sdfmgr.h>
#include "sdrfp.h"

#define SDRFP_RESOURCES		2

#if FW_DEBUG
#define SDRFP_PRINTF(_line, _fmt, ...)					\
	SceDeci4pSDfMgrForKernel_501CE61D(0xf, SCE_DBGINFO(_line), _fmt, ##__VA_ARGS__)
#else
#define SDRFP_PRINTF(_line, _fmt, ...)	do { } while (0)
#endif

typedef struct SdrfpResource {
	SceUID uid;
	void *base;
	SceUInt32 nfrag;			/* DRFP_FRAG_SIZE slots */
	SceSize size;
	SceUInt32 reserved;			/* 1, never read */
} SdrfpResource;

/* request in flight */
typedef struct SdrfpRequest {
	int multi;				/* 1: fragmented response (read) */
	SceUInt32 reserved;
	SceUInt32 seq;
	void *buf;
	SceSize size;
	SceUInt32 nfrag;			/* fragments of the response */
	SceUInt32 received;
} SdrfpRequest;

/* handler context */
typedef struct SdrfpChannel {
	int handle;
	SceUInt32 connected;
	SceUInt32 enabled;
	SdrfpRequest *recv;			/* receiving the response of */
} SdrfpChannel;

static SceKernelSpinlock s_seqLock;
static SceSDfMgrProtocol s_protocol = {
	.size = sizeof(SceSDfMgrProtocol),
	.protocol = DRFP_PROTOCOL,
	.alias = 0x11000,
	.version = 0x1000000,
	.subVersion = 0x20001,
	.destinationAndFlags = 0x20000,
	.name = "SDRFP",
};
static SdrfpResource s_resource[SDRFP_RESOURCES] = {
	{ .nfrag = 1, .reserved = 1 },
	{ .nfrag = 1, .reserved = 1 },
};
static SceKernelSpinlock s_requestLock;
static SceUInt16 s_seq;
static SdrfpRequest *s_response;	/* sent, waiting for its response */
static SdrfpChannel s_channel;
static SdrfpRequest s_request;
static SceKernelSpinlock s_doneLock;
static SceKernelSpinlock s_sendLock;
static SdrfpRequest *s_send;		/* waiting to be sent */

/* name unknown: send a request in buf and wait for the response in buf */
static int sdrfpTransact(int multi, void *buf, SceSize size)
{
	SceDeci4pHeader *hdr = buf;
	SceKernelIntrStatus intr;
	SceUInt16 seq;
	SceUInt32 cpu;
	int ret;

	ksceKernelSpinlockLowLock(&s_sendLock);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_seqLock);
	seq = ++s_seq;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_seqLock, intr);

	s_request.seq = seq;
	s_request.buf = buf;
	s_request.multi = multi;
	s_request.size = size;
	SceDeci4pSDfMgrForKernel_FFC95B63(hdr, size);
	hdr->version = SCE_DECI4P_VERSION;
	cpu = SceCpuId();
	((DrfpHeader *)(hdr + 1))->seq = s_request.seq;
	hdr->protocol = DRFP_PROTOCOL;
	hdr->src = 0xa200 + (1 << cpu);
	hdr->dst = 0;

	s_send = &s_request;
	ret = _threadSignal(s_channel.handle);
	if (ret >= 0)
		ksceKernelSpinlockLowLock(&s_doneLock);	/* released by the handler */
	ksceKernelSpinlockLowUnlock(&s_sendLock);

	s_request.multi = 0;
	s_request.reserved = 0;
	s_request.seq = 0;
	s_request.buf = NULL;
	s_request.size = 0;
	s_request.nfrag = 0;
	s_request.received = 0;
	return ret;
}

/* name unknown: DRFP result to an SCE error */
static int sdrfpError(int result)
{
	switch (result) {
	case 0:
		return 0;
	case 0x11002:
		return SCE_ERROR_ERRNO_ENOENT;
	case 0x11005:
		return SCE_ERROR_ERRNO_EIO;
	case 0x11009:
		return SCE_ERROR_ERRNO_EBADF;
	case 0x1100c:
		return SCE_ERROR_ERRNO_ENOMEM;
	case 0x1100d:
		return SCE_ERROR_ERRNO_EACCES;
	case 0x11010:
		return SCE_ERROR_ERRNO_EBUSY;
	case 0x11011:
		return SCE_ERROR_ERRNO_EEXIST;
	case 0x11014:
		return SCE_ERROR_ERRNO_ENOTDIR;
	case 0x11015:
		return SCE_ERROR_ERRNO_EISDIR;
	case 0x11016:
		return SCE_ERROR_ERRNO_EINVAL;
	case 0x11018:
		return SCE_ERROR_ERRNO_EMFILE;
	case 0x1101c:
		return SCE_ERROR_ERRNO_ENOSPC;
	case 0x1101e:
		return SCE_ERROR_ERRNO_EROFS;
	case 0x11024:
		return SCE_ERROR_ERRNO_ENAMETOOLONG;
	case 0x11027:
		return SCE_ERROR_ERRNO_ENOTEMPTY;
	case 0x110c0:
		return SCE_ERROR_ERRNO_ENOTSUP;
	}
	ksceKernelPrintf("DRFP: unknown error code 0x%08x\n", result);
	return SCE_ERROR_ERRNO_ENOTSUP;
}

static int sdrfpReadHandler(SceSize size, SdrfpChannel *ch)
{
	SceDeci4pHeader hdr;
	DrfpHeader drfp;
	SdrfpRequest *req;
	SceSize hdrOff, drfpOff, slot;
	char *frag;
	int ret;

	ret = sceSDfMgrExRecv(ch->handle, &hdr, sizeof(hdr));
	if (ret < 0) {
		SDRFP_PRINTF(922, "failed to recv Deci4pHeader (0x%08x)", ret);
		return ret;
	}
	ret = sceSDfMgrExRecv(ch->handle, &drfp, sizeof(drfp));
	if (ret < 0) {
		SDRFP_PRINTF(929, "failed to recv DrfpHeader (0x%08x)", ret);
		return ret;
	}

	req = s_response;
	s_response = NULL;
	if (req == NULL) {
		SDRFP_PRINTF(937, "sprious DRFP packet (req=0x%08x, seq=0x%04x)\n",
			     drfp.code, drfp.seq);
		return 0;
	}

	ch->recv = req;
	if (req->multi == 1) {
		if (req->received >= s_resource[1].nfrag) {
			SDRFP_PRINTF(949, "illegal fragment number %d (%d)\n",
				     req->received, s_resource[1].nfrag);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		slot = req->received * DRFP_FRAG_SIZE;
		hdrOff = slot + sizeof(SceDeci4pHeader);
		drfpOff = slot + sizeof(SceDeci4pHeader) + sizeof(DrfpHeader);
	} else {
		slot = 0;
		hdrOff = sizeof(SceDeci4pHeader);
		drfpOff = sizeof(SceDeci4pHeader) + sizeof(DrfpHeader);
	}

	frag = req->buf;
	*(SceDeci4pHeader *)(frag + slot) = hdr;
	*(DrfpHeader *)(frag + hdrOff) = drfp;
	ret = sceSDfMgrExRecv(ch->handle, frag + drfpOff,
			      size - sizeof(SceDeci4pHeader) - sizeof(DrfpHeader));
	if (ret < 0) {
		SDRFP_PRINTF(973, "failed to recv DRFP payload (0x%08x)", ret);
		return ret;
	}

	if (req->nfrag == 0)
		req->nfrag = req->multi == 1 ? ((DrfpReadPacket *)(frag + slot))->nfrag : 1;
	req->received++;
	return 0;
}

static int sdrfpWriteHandler(SdrfpChannel *ch)
{
	int ret;

	ret = sceSDfMgrExSend(ch->handle, s_send->buf, s_send->size);
	if (ret < 0) {
		SDRFP_PRINTF(882, "ExSend failed (0x%08x)", ret);
		return ret;
	}
	return 0;
}

static void sdrfpEventHandler(int event, void *arg, void *ctx)
{
	SdrfpChannel *ch = ctx;
	SceDfMgrStatus *status = arg;
	SdrfpRequest *req;
	int ret;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		ret = sdrfpReadHandler((SceSize)arg, ch);
		break;
	case SCE_DFMGR_EVENT_REQ_DONE:
		req = ch->recv;
		ch->recv = NULL;
		if (req != NULL && req->nfrag <= req->received)
			ksceKernelSpinlockLowUnlock(&s_doneLock);
		return;
	case SCE_DFMGR_EVENT_SEND:
		ret = sdrfpWriteHandler(ch);
		break;
	case SCE_DFMGR_EVENT_SENT:
		s_response = s_send;
		s_send = NULL;
		return;
	case SCE_DFMGR_EVENT_STATUS:
		if (status->kind == 0) {
			if (status->connected == 0)
				ch->connected = 0;
			else if (status->connected == 1)
				ch->connected = 1;
		} else if (status->kind == 1) {
			if (status->enabled == 0)
				ch->enabled = 0;
			else if (status->enabled == 1)
				ch->enabled = 1;
		}
		return;
	case 6:
		return;
	default:
		ret = SCE_KERNEL_ERROR_UNSUP;
		SDRFP_PRINTF(860, "unknown event (0x%08x)\n", event);
		break;
	}

	if (ret != 0)
		SDRFP_PRINTF(865, "event error (0x%08x)\n", ret);
}

int sdrfpOpen(const char *name, int flags, int mode, Deci4pFsStat *stat)
{
	DrfpOpenPacket *pkt;
	SceUInt32 *src, *dst;
	SceSize len, size;
	int ret;

	len = strnlen(name, 0x400);
	ksceKernelSpinlockLowLock(&s_requestLock);
	pkt = s_resource[0].base;
	size = ((len + 4) & ~3) + offsetof(DrfpOpenPacket, name);
	if (pkt == NULL) {
		/* (returns with the request lock held) */
		SDRFP_PRINTF(435, "bufferAddr failed 0x%08x\n", pkt);
		return DRFP_ERROR_NO_BUFFER;
	}

	memset(pkt, 0, size);
	pkt->drfp.code = DRFP_CMD_OPEN;
	pkt->flags = flags;
	pkt->mode = mode;
	pkt->nameLen = len + 1;
	memcpy(pkt->name, name, len + 1);
	ret = sdrfpTransact(0, pkt, size);
	if (ret >= 0) {
		ret = sdrfpError(pkt->result);
		if (ret == 0) {
			src = (SceUInt32 *)&pkt->stat;
			dst = (SceUInt32 *)stat;
			do {
				SceUInt32 first = src[0];
				SceUInt32 second = src[1];
				SceUInt32 third = src[2];
				SceUInt32 fourth = src[3];

				src += 4;
				dst += 4;
				dst[-4] = first;
				dst[-3] = second;
				dst[-2] = third;
				dst[-1] = fourth;
			} while (src != (SceUInt32 *)(&pkt->stat + 1));
			ret = pkt->fd;
		}
	}
	ksceKernelSpinlockLowUnlock(&s_requestLock);
	return ret;
}

int sdrfpRead(int fd, void *buf, SceOff offset, SceSize size)
{
	DrfpReadPacket *pkt, *frag, *next;
	SceSize max, total, n;
	SceUInt16 nfrag;
	int ret, i;

	ksceKernelSpinlockLowLock(&s_requestLock);
	pkt = s_resource[1].base;
	if (pkt == NULL) {
		/* (returns with the request lock held) */
		SDRFP_PRINTF(521, "bufferAddr failed 0x%08x\n", pkt);
		return DRFP_ERROR_NO_BUFFER;
	}
	max = s_resource[1].nfrag << 16;

	for (total = 0; total < size; ) {
		memset(pkt, 0, offsetof(DrfpReadPacket, size) + sizeof(pkt->size));
		pkt->offset = offset;
		pkt->drfp.code = DRFP_CMD_READ;
		pkt->size = size - total < max ? size - total : max;
		pkt->fd = fd;
		pkt->padding = 0;
		ret = sdrfpTransact(1, pkt, offsetof(DrfpReadPacket, size) + sizeof(pkt->size));
		if (ret != 0) {
			SDRFP_PRINTF(554, "READ_CMD: send error - 0x%x\n", ret);
			goto out;
		}
		if (pkt->result != 0) {
			SDRFP_PRINTF(569, "READ_RES: error - 0x%x\n", pkt->result);
			ksceKernelSpinlockLowUnlock(&s_requestLock);
			return DRFP_ERROR_READ;
		}
		if (pkt->nfrag == 0)
			continue;

		frag = pkt;
		next = (DrfpReadPacket *)((char *)pkt + DRFP_FRAG_SIZE);
		n = frag->nbytes;
		for (i = 0; ; ) {
			if (n > DRFP_FRAG_DATA_MAX) {
				SDRFP_PRINTF(612, "READ_RES: error - invalid numberOfBytes\n");
				ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
				goto out;
			}
			if (n == 0)
				goto out;
			memcpy((char *)buf + total, frag->data, n);
			n = frag->nbytes;
			i++;
			nfrag = frag->nfrag;
			frag = next;
			offset += n;
			total += n;
			if (i >= nfrag)
				break;
			if (frag->result != 0) {
				SDRFP_PRINTF(603, "READ_RES: error - 0x%x\n", frag->result);
				ret = -1;
				goto out;
			}
			n = frag->nbytes;
			next = (DrfpReadPacket *)((char *)next + DRFP_FRAG_SIZE);
		}
	}
	ksceKernelSpinlockLowUnlock(&s_requestLock);
	return total;

out:
	ksceKernelSpinlockLowUnlock(&s_requestLock);
	return ret < 0 ? ret : (int)total;
}

int sdrfpClose(int fd)
{
	DrfpClosePacket *pkt;
	int ret;

	ksceKernelSpinlockLowLock(&s_requestLock);
	pkt = s_resource[0].base;
	if (pkt == NULL) {
		/* (returns with the request lock held) */
		SDRFP_PRINTF(664, "bufferAddr failed 0x%08x\n", pkt);
		return DRFP_ERROR_NO_BUFFER;
	}

	memset(pkt, 0, sizeof(*pkt));
	pkt->fd = fd;
	pkt->drfp.code = DRFP_CMD_CLOSE;
	ret = sdrfpTransact(0, pkt, sizeof(*pkt));
	if (ret >= 0)
		ret = sdrfpError(pkt->result);
	ksceKernelSpinlockLowUnlock(&s_requestLock);
	return ret;
}

/* name unknown */
int sdrfpGetstat(const char *name, Deci4pFsStat *stat)
{
	DrfpGetstatPacket *pkt;
	SceUInt32 *src, *dst;
	SceSize len, size;
	int ret;

	len = strnlen(name, 0x400);
	ksceKernelSpinlockLowLock(&s_requestLock);
	pkt = s_resource[0].base;
	size = ((len + 4) & ~3) + offsetof(DrfpGetstatPacket, name);
	if (pkt == NULL)
		return -1;		/* (with the request lock held) */

	memset(pkt, 0, size);
	pkt->drfp.code = DRFP_CMD_GETSTAT;
	pkt->nameLen = len + 1;
	memcpy(pkt->name, name, len + 1);
	ret = sdrfpTransact(0, pkt, size);
	if (ret >= 0) {
		ret = sdrfpError(pkt->result);
		if (ret == 0) {
			src = (SceUInt32 *)&pkt->stat;
			dst = (SceUInt32 *)stat;
			do {
				SceUInt32 first = src[0];
				SceUInt32 second = src[1];
				SceUInt32 third = src[2];
				SceUInt32 fourth = src[3];

				src += 4;
				dst += 4;
				dst[-4] = first;
				dst[-3] = second;
				dst[-2] = third;
				dst[-1] = fourth;
			} while (src != (SceUInt32 *)(&pkt->stat + 1));
		}
	}
	ksceKernelSpinlockLowUnlock(&s_requestLock);
	return ret;
}

static void initResourceTable(void)
{
	char name[32];
	SdrfpResource *res;
	SceUID uid;
	int i;

	for (i = 0, res = s_resource; i != SDRFP_RESOURCES; i++, res++) {
		res->size = res->nfrag * DRFP_FRAG_SIZE;
		snprintf(name, sizeof(name), "SceDeci4pSDrfpMemBlock%d", i);
		uid = ksceKernelAllocMemBlock(name, SceDeci4pSDfMgrForKernel_35238B8A(), res->size, NULL);
		if (uid < 0) {
			SDRFP_PRINTF(1116, "AllocMemBlockForKernel failed (0x%08x)\n", uid);
			SCE_DBG_ASSERT_FAIL(1117);
		}
		res->uid = uid;
		ksceKernelGetMemBlockBase(uid, &res->base);
	}
}

int SDrfpModuleStart(SceSize args, void *argp)
{
	int ret;

	SCE_DBG_PRINTF_LEVEL(2, 0xf, 216, "\n");
	if (!ksceKernelCheckDipsw(0xc1)) {
		SCE_DBG_PRINTF(0xb, 220, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (!SceQafMgrForDriver_382C71E8())
		return SCE_KERNEL_START_NO_RESIDENT;

	s_request.multi = 0;
	s_request.reserved = 0;
	s_request.seq = 0;
	s_request.buf = NULL;
	s_request.size = 0;
	s_request.nfrag = 0;
	s_channel.enabled = 0;
	s_channel.recv = NULL;
	s_request.received = 0;
	s_channel.handle = 0;
	s_channel.connected = 0;
	ksceKernelSpinlockLowLock(&s_doneLock);
	initResourceTable();

	ret = SceDeci4pSDfMgrForKernel_A55F5F0D(sdrfpEventHandler, &s_protocol, &s_channel);
	if (ret < 0) {
		SDRFP_PRINTF(239, "SDfMgrOpen failed (0x%08x)\n", ret);
		return ret;
	}
	s_channel.connected = 0;
	s_channel.enabled = 0;
	s_channel.handle = ret;

	ret = sdrfpVfsMount();
	if (ret < 0) {
		SDRFP_PRINTF(249, "vfs mount failed (0x%08X)\n", ret);
		return ret;
	}
	return SCE_KERNEL_START_SUCCESS;
}
