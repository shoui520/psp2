/*
 * SceDeci4pDrfp: the DRFP protocol on SceDeci4pDfMgr. sceDeci4pDrfpSend
 * queues a request packet and waits until the protocol handler has sent it
 * and received the host's response (Sony's drfp.c).
 */

#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/modulemgr.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr.h>
#include <kernel/error.h>
#include <sysmem/sysevent.h>
#include "drfp_internal.h"

#define DRFP_NAME		"SceDeci4pDrfp"
#define DRFP_MEMTYPE		SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW
#define DRFP_RESPONSE_TIMEOUT	60000000
#define DRFP_SEND_RETRIES	5
#define DRFP_SEND_RETRIES_SUSPENDED 15
#define DRFP_SEND_RETRY_DELAY	500000

static DrfpBufferPool s_drfpBuffer[DRFP_BUFFER_COUNT] = {
	{ .size = 0x11000, .count = 1 },
	{ .size = 0x11000, .count = 1 },
	{ .size = 0x11000, .count = 1 },
	{ .size = 0x11000, .count = 1 },
};

static DrfpRequest s_drfpRequest[DRFP_REQUEST_COUNT];
static DrfpProtocol s_drfpProtocol;
static SceUID s_drfpRequestSema;
static SceUInt16 s_drfpSequence;
static DrfpQueue s_drfpRecvQueue;		/* sent, waiting for the response */
static SceKernelSpinlock s_drfpLock;
static DrfpQueue s_drfpSendQueue;		/* waiting to be sent */
static SceUID s_drfpHeap;
static int s_drfpSuspended;

static inline int drfpQueueEmpty(DrfpQueue *queue)
{
	return queue->next == queue;
}

static inline void drfpRequestUnlink(DrfpRequest *req)
{
	ListNode *next = req->link.next;

	next->prev = req->link.prev;
	req->link.prev->next = next;
	req->link.next = &req->link;
	req->link.prev = &req->link;
}

static inline void drfpRequestAppend(DrfpQueue *queue, DrfpRequest *req)
{
	req->link.next = queue;
	req->link.prev = queue->prev;
	queue->prev = &req->link;
	req->link.prev->next = &req->link;
}

/* the first request of a queue, NULL when it is empty */
static inline DrfpRequest *drfpQueueFirst(DrfpQueue *queue)
{
	SceKernelIntrStatus s;
	DrfpRequest *req;

	s = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_drfpLock);
	if (drfpQueueEmpty(queue)) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_drfpLock, s);
		return NULL;
	}
	req = (DrfpRequest *)queue->next;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_drfpLock, s);
	return req;
}

static inline void drfpRemoveRequest(DrfpRequest *req)
{
	SceKernelIntrStatus s;

	s = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_drfpLock);
	drfpRequestUnlink(req);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_drfpLock, s);
}

static inline void drfpAddRequest(DrfpQueue *queue, DrfpRequest *req)
{
	SceKernelIntrStatus s;

	s = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_drfpLock);
	drfpRequestAppend(queue, req);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_drfpLock, s);
}

/* name unknown: a suspend makes later sends wait longer for the link */
static int drfpSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	if (resume == 0)
		s_drfpSuspended = 1;
	return 0;
}

static inline int drfpCancelQueue(DrfpQueue *queue)
{
	DrfpRequest *req;

	for (;;) {
		req = drfpQueueFirst(queue);
		if (req == NULL)
			return 0;
		drfpRemoveRequest(req);
		ksceKernelSetEventFlag(req->evf, DRFP_EVF_CANCEL);
	}
}

/* name unknown: the link or the host went away, fail every request */
static void drfpCancelAllRequests(void)
{
	drfpCancelQueue(&s_drfpSendQueue);
	drfpCancelQueue(&s_drfpRecvQueue);
}

static inline __attribute__((always_inline))
int ReadResHandler(DrfpProtocol *ctx, DrfpRequest *req)
{
	DrfpReadResult res;
	char *base, *end, *dst;
	SceUInt32 nfrag;
	int ret;

	ret = sceDfMgrRecv(ctx->handle, &res, sizeof(res));
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0, 710, "(0x%08x)\n", ret);
		req->result = ret;
		req->flags |= DRFP_REQ_ERROR;
		return ret;
	}
	if (res.result != 0) {
		DRFP_PRINTF(0xf, 718, "DRFP error, result = 0x%08x\n", res.result);
		req->flags |= DRFP_REQ_ERROR;
		req->read.hostResult = res.result;
		return 0;
	}
	base = req->read.buf;
	end = base + req->read.size;
	dst = base + (res.frag - 1u) * DRFP_FRAG_DATA_MAX;
	if (dst < base || dst >= end) {
		DRFP_PRINTF(0xf, 738, "illegal fragment number %d (0x%08x, 0x%08x)\n",
			    res.frag, dst, end);
		goto invalid;
	}
	if (res.nbytes > DRFP_FRAG_DATA_MAX || res.nbytes > (SceSize)(end - dst)) {
		DRFP_PRINTF(0xf, 749, "illegal number of bytes: 0x%08x\n", res.nbytes);
		goto invalid;
	}
	ret = 0;
	if (res.nbytes != 0) {
		ret = sceDfMgrRecv(ctx->handle, dst, res.nbytes);
		if (ret != (int)res.nbytes) {
			SCE_PRINTF_WITH_INFO(0, 761, "(0x%08x)\n", ret);
			req->result = ret;
			req->flags |= DRFP_REQ_ERROR;
			return ret;
		}
	}
	nfrag = req->read.nfrag;
	if (nfrag == 0) {
		nfrag = res.nfrag;
		req->read.nfrag = nfrag;
	} else if (nfrag != res.nfrag) {
		DRFP_PRINTF(0xf, 776, "illegal Number Of Fragments: %d %d\n", nfrag, res.nfrag);
		goto invalid;
	}
	req->read.received += ret;
	if (res.frag >= nfrag)
		req->flags |= DRFP_REQ_DONE;
	if ((SceSize)ret < DRFP_FRAG_DATA_MAX)
		req->flags |= DRFP_REQ_DONE;
	return 0;

invalid:
	req->read.hostResult = DRFP_HOST_EINVAL;
	req->flags |= DRFP_REQ_ERROR;
	return SCE_ERROR_ERRNO_EINVAL;
}

static inline __attribute__((always_inline))
int ReadHandler(DrfpProtocol *ctx, SceSize size)
{
	SceKernelIntrStatus s;
	DrfpHeader h, *pkt;
	DrfpRequest *req;
	SceUInt32 seq, code;
	int ret;

	ret = sceDfMgrRecv(ctx->handle, &h.hdr, sizeof(h.hdr));
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0, 820, "(0x%08x)\n", ret);
		return ret;
	}
	ret = sceDfMgrRecv(ctx->handle, &h.code, sizeof(h) - sizeof(h.hdr));
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0, 828, "(0x%08x)\n", ret);
		return ret;
	}
	seq = h.seq;
	s = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_drfpLock);
	for (req = (DrfpRequest *)s_drfpRecvQueue.next;
	     &req->link != &s_drfpRecvQueue; req = (DrfpRequest *)req->link.next) {
		if (req->seq != seq)
			continue;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_drfpLock, s);
		code = req->code;
		ctx->readRequest = req;
		if (code == DRFP_CODE_READ)
			return ReadResHandler(ctx, req);
		pkt = req->packet;
		*pkt = h;
		ret = sceDfMgrRecv(ctx->handle, pkt + 1, size - sizeof(*pkt));
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0, 868, "(0x%08x)\n", ret);
			req->result = ret;
			req->flags |= DRFP_REQ_ERROR;
		} else {
			req->flags |= DRFP_REQ_DONE;
		}
		return ret;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_drfpLock, s);
	DRFP_PRINTF(0xf, 838, "couldn't find request 0x%08x\n", seq);
	return SCE_ERROR_ERRNO_ENOENT;
}

static inline __attribute__((always_inline))
int ReadDoneHandler(DrfpProtocol *ctx)
{
	DrfpRequest *req = ctx->readRequest;

	if (req == NULL) {
		DRFP_PRINTF(0xf, 885, "readdone: currentRequest = 0\n");
		return 0;
	}
	if (req->flags & (DRFP_REQ_DONE | DRFP_REQ_ERROR))
		ksceKernelSetEventFlag(req->evf, DRFP_EVF_RESPONSE);
	ctx->readRequest = NULL;
	return 0;
}

static inline __attribute__((always_inline))
int WriteHandler(DrfpProtocol *ctx)
{
	DrfpWritePacket *wpkt;
	DrfpRequest *req;
	int ret;

	req = drfpQueueFirst(&s_drfpSendQueue);
	if (req == NULL)
		return 0;
	if (req->code != DRFP_CODE_WRITE) {
		drfpRemoveRequest(req);
		drfpAddRequest(&s_drfpRecvQueue, req);
		ret = sceDfMgrSend(ctx->handle, req->packet, req->size);
		if ((SceSize)ret != req->size)
			DRFP_PRINTF(0xf, 677, "sceDfmgrSend() size mismatch (0x%08x <=> 0x%08x)\n",
				    ret, req->size);
		return 0;
	}
	/* a write goes out in two parts: the request, then the data */
	wpkt = req->packet;
	if (req->write.state == 0) {
		ret = sceDfMgrSend(ctx->handle, wpkt, sizeof(*wpkt));
		if (ret != sizeof(*wpkt))
			DRFP_PRINTF(0xf, 634, "sceDfmgrSend() size mismatch (0x%08x <=> 0x%08x)\n",
				    ret, sizeof(*wpkt));
		req->write.state = 1;
		return 0;
	}
	if (req->write.state != 1) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 661);
		return 0;
	}
	drfpRemoveRequest(req);
	if (wpkt->flags & DRFP_WRITE_MORE)
		ctx->writeRequest = req;
	else
		drfpAddRequest(&s_drfpRecvQueue, req);
	ret = sceDfMgrSend(ctx->handle, req->write.buf, req->write.size);
	if ((SceSize)ret != req->write.size)
		DRFP_PRINTF(0xf, 656, "sceDfmgrSend() size mismatch (0x%08x <=> 0x%08x)\n",
			    ret, req->write.size);
	return 0;
}

/* name unknown */
static inline __attribute__((always_inline))
int WriteDoneHandler(DrfpProtocol *ctx)
{
	if (ctx->writeRequest != NULL) {
		ksceKernelSetEventFlag(ctx->writeRequest->evf, DRFP_EVF_SENT);
		ctx->writeRequest = NULL;
	}
	if (drfpQueueFirst(&s_drfpSendQueue) != NULL)
		sceDfMgrReqSend(ctx->handle);
	return 0;
}

static inline __attribute__((always_inline))
int ChangeStatusHandler(DrfpProtocol *ctx, const SceDfMgrStatus *status)
{
	if (status->kind == 0) {
		if (status->connected == 0) {
			ctx->connected = 0;
			ctx->retry = 1;
			drfpCancelAllRequests();
		} else if (status->connected == 1) {
			ctx->connected = 1;
		} else {
			DRFP_PRINTF(0xf, 919, "unknown connection status (0x%x)\n", status->connected);
		}
	} else if (status->kind == 1) {
		if (status->enabled == 0) {
			ctx->registered = 0;
			drfpCancelAllRequests();
		} else if (status->enabled == 1) {
			ctx->registered = 1;
		} else {
			DRFP_PRINTF(0xf, 939, "unknown protocol status (0x%x)\n", status->enabled);
		}
	} else {
		DRFP_PRINTF(0xf, 945, "unknown code (%08x)\n", status->kind);
	}
	return 0;
}

/* name unknown: the SceDeci4pDfMgr protocol handler */
static int drfpHandler(int event, void *arg, void *argp)
{
	DrfpProtocol *ctx = argp;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		return ReadHandler(ctx, (SceSize)arg);
	case SCE_DFMGR_EVENT_REQ_DONE:
		return ReadDoneHandler(ctx);
	case SCE_DFMGR_EVENT_SEND:
		return WriteHandler(ctx);
	case SCE_DFMGR_EVENT_SENT:
		return WriteDoneHandler(ctx);
	case SCE_DFMGR_EVENT_STATUS:
		return ChangeStatusHandler(ctx, arg);
	}
	return 0;
}

int sceDeci4pDrfpInit(void)
{
	SceKernelHeapCreateOpt opt;
	SceDfMgrProtocol proto;
	DrfpRequest *req;
	DrfpBufferPool *pool;
	char name[32];
	char memBlockName[32];
	SceUID ret;
	int i;

	opt.size = sizeof(opt);
	opt.attr = HEAP_AUTO_EXTEND | HEAP_HAS_MEMTYPE;
	opt.maxSize = 0;
	opt.memBlock = 0;
	opt.memtype = DRFP_MEMTYPE;
	opt.pid = 0;
	opt.heap = NULL;
	ret = ksceKernelCreateHeap(DRFP_NAME, 0x1000, &opt);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 305, "CreateHeap failed (0x%08x)\n", ret);
		return ret;
	}
	s_drfpHeap = ret;
	s_drfpRequestSema = ksceKernelCreateSema(DRFP_NAME, SCE_KERNEL_ATTR_THREAD_PRIO,
						 DRFP_REQUEST_COUNT, DRFP_REQUEST_COUNT, NULL);
	if (s_drfpRequestSema < 0)
		return -1;

	s_drfpProtocol.handle = 0;
	s_drfpProtocol.connected = 0;
	s_drfpProtocol.registered = 0;
	s_drfpProtocol.retry = 0;
	s_drfpProtocol.readRequest = NULL;
	s_drfpProtocol.writeRequest = NULL;
	memset(s_drfpRequest, 0, sizeof(s_drfpRequest));
	for (i = 0; i < DRFP_REQUEST_COUNT; i++) {
		req = &s_drfpRequest[i];
		req->link.next = &req->link;
		req->link.prev = &req->link;
		req->flags = 0;
		req->code = 0;
		req->seq = 0;
		req->packet = NULL;
		req->size = 0;
		req->result = 0;
		snprintf(name, sizeof(name), DRFP_NAME "%d", i);
		req->evf = ksceKernelCreateEventFlag(name, 0, 0, NULL);
	}
	s_drfpSendQueue.next = &s_drfpSendQueue;
	s_drfpSendQueue.prev = &s_drfpSendQueue;
	s_drfpRecvQueue.next = &s_drfpRecvQueue;
	s_drfpRecvQueue.prev = &s_drfpRecvQueue;

	for (i = 0; i < DRFP_BUFFER_COUNT; i++) {
		pool = &s_drfpBuffer[i];
		snprintf(memBlockName, sizeof(memBlockName), DRFP_NAME "MemBlock%d", i);
		pool->memBlock = ksceKernelAllocMemBlock(memBlockName, DRFP_MEMTYPE,
						     pool->size, NULL);
		ksceKernelGetMemBlockBase(pool->memBlock, &pool->base);
		snprintf(name, sizeof(name), DRFP_NAME "%d", i);
		pool->sema = ksceKernelCreateSema(name, SCE_KERNEL_ATTR_THREAD_PRIO, pool->count,
						  pool->count, NULL);
	}

	proto.size = sizeof(proto);
	proto.protocol = DRFP_PROTOCOL;
	proto.alias = DRFP_PROTOCOL_ALIAS;
	proto.version = DRFP_PROTOCOL_VERSION;
	proto.subVersion = DRFP_PROTOCOL_SUBVERSION;
	proto.destination = 0;
	proto.flags = DFMGR_PROTOCOL_ALIAS;
	strncpy(proto.name, "DRFP", sizeof(proto.name));
	if (ksceKernelCheckDipsw(0xc3) == 0)
		proto.flags |= DFMGR_PROTOCOL_USB;
	s_drfpProtocol.handle = sceDfMgrOpen(drfpHandler, &proto, &s_drfpProtocol);
	s_drfpSuspended = 0;
	s_drfpProtocol.retry = 1;
	s_drfpProtocol.connected = 0;
	s_drfpProtocol.registered = 0;
	s_drfpProtocol.readRequest = NULL;
	s_drfpProtocol.writeRequest = NULL;
	ksceKernelRegisterSysEventHandler(DRFP_NAME, drfpSysEventHandler, NULL);
	return 0;
}

/* name unknown */
SceUID drfpGetHeap(void)
{
	return s_drfpHeap;
}

/* name unknown: take a packet buffer, NULL when the wait fails */
DrfpBufferPool *drfpGetBuffer(int index)
{
	DrfpBufferPool *pool = &s_drfpBuffer[index];

	if (ksceKernelWaitSema(pool->sema, 1, NULL) != 0)
		return NULL;
	return pool;
}

/* name unknown */
void drfpReleaseBuffer(int index)
{
	ksceKernelSignalSema(s_drfpBuffer[index].sema, 1);
}

int sceDeci4pDrfpSend(void *packet, SceSize size, void *info)
{
	DrfpHeader *pkt = packet;
	SceKernelIntrStatus s;
	DrfpRequest *req;
	SceUInt32 timeout, bits, *pTimeout;
	int ret, retries, i, index;

	if (ksceKernelWaitSema(s_drfpRequestSema, 1, NULL) != 0)
		SCE_PANIC(460);

	s = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_drfpLock);
	index = -1;
	for (i = 0; i < DRFP_REQUEST_COUNT; i++) {
		if (!(s_drfpRequest[i].flags & DRFP_REQ_USED)) {
			index = i;
			break;
		}
	}
	req = &s_drfpRequest[index];
	req->flags |= DRFP_REQ_USED;
	req->seq = ++s_drfpSequence;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_drfpLock, s);

	req->packet = pkt;
	req->size = size;
	req->code = pkt->code;
	if (req->code == DRFP_CODE_READ)
		req->read = *(DrfpReadInfo *)info;
	if (req->code == DRFP_CODE_WRITE)
		req->write = *(DrfpWriteInfo *)info;
	SceDeci4pDfMgrForDebugger_6D26CC56(&pkt->hdr, size);
	pkt->seq = req->seq;
	drfpAddRequest(&s_drfpSendQueue, req);

	retries = s_drfpSuspended ? DRFP_SEND_RETRIES_SUSPENDED : DRFP_SEND_RETRIES;
	for (;;) {
		ret = sceDfMgrReqSend(s_drfpProtocol.handle);
		if (ret != DFMGR_ERROR_NOT_CONNECTED)
			break;
		if (s_drfpProtocol.retry == 0) {
			retries = 0;
			break;
		}
		sceKernelDelayThread(DRFP_SEND_RETRY_DELAY);
		if (--retries <= 0) {
			retries = 0;
			break;
		}
	}
	if (retries == 0) {
		s_drfpProtocol.retry = 0;
		ret = SCE_ERROR_ERRNO_ENODEV;
		goto done;
	}
	if (ret < 0 && ret != DFMGR_ERROR_BUSY)
		goto done;

	timeout = DRFP_RESPONSE_TIMEOUT;
	pTimeout = &timeout;
	for (;;) {
		bits = 0;
		ret = ksceKernelWaitEventFlag(req->evf,
					      DRFP_EVF_RESPONSE | DRFP_EVF_SENT | DRFP_EVF_CANCEL,
					      SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, &bits,
					      pTimeout);
		if (ret >= 0)
			break;
		if (ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			DRFP_PRINTF(0xf, 545, "WaitEventFlag failed (0x%08x)\n", ret);
			goto done;
		}
		if (s_drfpProtocol.connected == 0 || s_drfpProtocol.registered == 0) {
			DRFP_PRINTF(0xf, 534,
				    "WARNING: request cancellation doesn't work correctly "
				    "(req = 0x%x, code = 0x%x, conn = %d, reg = %d)\n",
				    req->seq, req->code, s_drfpProtocol.connected,
				    s_drfpProtocol.registered);
			ret = SCE_ERROR_ERRNO_ENODEV;
			goto done;
		}
		SCE_PRINTF_WITH_INFO(0, 541, "DRFP: no response from file server (0x%08x, 0x%08x)\n",
				     req->seq, req->code);
		pTimeout = NULL;
	}
	if (bits & DRFP_EVF_CANCEL) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		DRFP_PRINTF(0xf, 559,
			    "NOTICE: request has been canceled "
			    "(req = 0x%x, code = 0x%x, conn = %d, reg = %d)\n",
			    req->seq, req->code, s_drfpProtocol.connected, s_drfpProtocol.registered);
	} else {
		if (req->code == DRFP_CODE_READ)
			*(DrfpReadInfo *)info = req->read;
		ret = req->result;
	}

done:
	drfpRemoveRequest(req);
	ksceKernelClearEventFlag(req->evf, 0);
	s = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_drfpLock);
	req->flags = 0;
	req->code = 0;
	req->seq = 0;
	req->packet = NULL;
	req->size = 0;
	req->result = 0;
	req->link.next = &req->link;
	req->link.prev = &req->link;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_drfpLock, s);
	ksceKernelSignalSema(s_drfpRequestSema, 1);
	return ret;
}

int DrfpModuleStart(SceSize args, void *argp)
{
	SceUID handle;

	if (ksceKernelCheckDipsw(0xc2) == 0) {
		SCE_DBG_PRINTF(0xb, 980, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (sceDeci4pDrfpFsInit() != 0 || sceDeci4pDrfpInit() != 0)
		return SCE_KERNEL_START_FAILED;
	if (sceDeci4pDrfpFsStart() >= 0)
		return SCE_KERNEL_START_SUCCESS;
	handle = s_drfpProtocol.handle;
	s_drfpProtocol.handle = -1;
	if (handle > 0)
		sceDfMgrClose(handle);
	ksceKernelDeleteSema(s_drfpRequestSema);
	return SCE_KERNEL_START_FAILED;
}
