/*
 * SceDeci4pRDrfp protocol: module start, the SceDeci4pDfMgr handler, and the
 * request / reply queues (Sony's rdrfp.c).
 */
#include <sysmem/dipsw.h>
#include <sysmem/sysmem.h>
#include <string.h>
#include <kernel/modulemgr.h>
#include "rdrfp.h"

#define SCE_ERROR_ERRNO_ENOMEM			0x8001000c
#define RDRFP_DFMGR_ERROR_REQUESTED		0x80080007	/* name unknown: already requested */

#define RDRFP_DIPSW			0xc2
#define RDRFP_DIPSW_NO_AUTO_ENABLE	0xc3

static RdrfpQueue rdrfpSendQueue;
RdrfpContext rdrfp;
static SceUID rdrfpHeap;
static RdrfpDesc *rdrfpCurrentSending;
static RdrfpQueue rdrfpRecvQueue;

/* ===== requests ===== */

/* the oldest collected write, flushed for room (Sony's build inlines the request) */
void rdrfpFlushWritebuf(void)
{
	RdrfpWritebuf *wb = rdrfpGetWritebufFromList();

	if (wb == NULL)
		return;
	rdrfpRequestWritebuf(wb, 2);
}

int rdrfpIsEnabled(void)
{
	return rdrfp.enabled != 0;
}

/* the file system thread's next request */
RdrfpRequest *rdrfpDequeueRequest(void)
{
	return rdrfpDequeue(&rdrfpRecvQueue, 1);
}

/* Outlined, and also inlined into the write-buffer paths in Sony's build. */
RdrfpRequest *rdrfpPrepareRequest(SceUInt32 id, int bufType, int bufIndex, void *packet)
{
	RdrfpDesc *desc;
	RdrfpRequest *req;

	desc = rdrfpCreateDesc(id, bufType, bufIndex, packet);
	if (desc == NULL)
		SCE_PANIC(163);
	req = sceKernelAllocHeapMemory(rdrfpHeap, sizeof(*req));
	if (req == NULL)
		SCE_PANIC(168);
	memset(req, 0, sizeof(*req));
	req->link.next = &req->link;
	req->link.prev = &req->link;
	req->id = id;
	req->desc = desc;
	return req;
}

void rdrfpTerminateRequest(RdrfpRequest *req)
{
	RdrfpDesc *desc;

	SCE_DBG_ASSERT_LEVEL(0, req && &req->link == req->link.next, 184);
	desc = req->desc;
	sceKernelFreeHeapMemory(rdrfpHeap, req);
	rdrfpReleaseBuffer(desc->bufType, desc->bufIndex);
	rdrfpDeleteDesc(desc);
}

void rdrfpSetPacketSize(void *packet, SceSize size)
{
	SceDeci4pDfMgrForDebugger_6D26CC56(packet, size);
}

void rdrfpSetPacketType(RdrfpHeader *packet, SceUInt32 type)
{
	packet->type = type;
}

void rdrfpSetPacketId(RdrfpHeader *packet, SceUInt16 id)
{
	packet->id16 = id;
}

int rdrfpSendPacket(RdrfpRequest *req)
{
	RdrfpDesc *desc;
	int ret;

	rdrfpEnqueue(&rdrfpSendQueue, req);
	ret = sceDfMgrReqSend(rdrfp.handle);
	if (ret < 0 && ret != (int)RDRFP_DFMGR_ERROR_REQUESTED) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 222, "ReqSend failed (0x%08x)\n", ret);
		rdrfpRemoveFromQueue(&rdrfpSendQueue, req);
		desc = req->desc;
		rdrfpReleaseBuffer(desc->bufType, desc->bufIndex);
		rdrfpDeleteDesc(desc);
	}
	return ret;
}

static int sceDeci4pRDrfpHandler(int event, void *arg, void *ctx);

/* ===== start ===== */

static inline __attribute__((always_inline)) int sceDeci4pRDrfpInit(void)
{
	SceKernelHeapCreateOpt opt;
	SceDfMgrProtocol protocol;
	int ret;

	rdrfp.handle = 0;
	rdrfp.connected = 0;
	rdrfp.enabled = 0;
	opt = (SceKernelHeapCreateOpt) {
		.size = sizeof(opt),
		.attr = 0x401,
		.memtype = 0x10f0d006,
	};
	ret = sceKernelCreateHeap("SceDeci4pRDrfp", 0x2000, &opt);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 80, "CreateHeap failed (0x%08X)\n", ret);
		return SCE_KERNEL_START_FAILED;
	}
	rdrfpHeap = ret;

	protocol.size = sizeof(protocol);
	protocol.protocol = 0x12000;
	protocol.alias = 0;
	protocol.version = 0x1000001;
	protocol.subVersion = 0x10000;
	protocol.destination = 0;
	protocol.flags = 1;
	strncpy(protocol.name, "RDRFP", sizeof(protocol.name));
	if (!sceKernelCheckDipsw(RDRFP_DIPSW_NO_AUTO_ENABLE))
		protocol.flags |= 0x200;
	ret = sceDfMgrOpen(sceDeci4pRDrfpHandler, &protocol, &rdrfp);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 102, "sceDfMgrOpen failed (0x%08x)\n", ret);
		return SCE_KERNEL_START_FAILED;
	}
	rdrfp.handle = ret;
	rdrfp.connected = 1;
	rdrfp.enabled = 1;
	rdrfpInitBufferTable();
	rdrfpInitDesc();
	rdrfpInitQueue(&rdrfpRecvQueue);
	rdrfpInitQueue(&rdrfpSendQueue);
	rdrfpInitWritebufList();
	rdrfpInitFileCache();
	rdrfpInitFS();
	return SCE_KERNEL_START_SUCCESS;
}

int RDrfpModuleStart(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	if (!sceKernelCheckDipsw(RDRFP_DIPSW)) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0xb, SCE_DBGINFO_FUNC(639, "RDrfpModuleStart"),
					      "disabled by dipsw\n");
#endif
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	return sceDeci4pRDrfpInit();
}

int rdrfpIsRequestQueueEmpty(void)
{
	return rdrfpIsQueueEmpty(&rdrfpRecvQueue);
}

void rdrfpQueueRequest(RdrfpRequest *req)
{
	rdrfpEnqueue(&rdrfpRecvQueue, req);
}

void *rdrfpAlloc(SceSize size)
{
	return sceKernelAllocHeapMemory(rdrfpHeap, size);
}

void rdrfpFree(void *p)
{
	sceKernelFreeHeapMemory(rdrfpHeap, p);
}

/* queue a collected write: its last piece's ID answers for all of them */
void rdrfpRequestWritebuf(RdrfpWritebuf *wb, SceUInt8 kind)
{
	RdrfpWritePacket *pkt;
	RdrfpRequest *req;
	SceUInt32 id;
	int count = wb->count;

	SCE_DBG_ASSERT(count > 0, 691);
	pkt = (RdrfpWritePacket *)(wb->buf + RDRFP_BUF_HEADER);
	id = wb->ids[count - 1];
	pkt->head.type = RDRFP_CMD_WRITE;
	pkt->head.id16 = id;
	pkt->write.fd = wb->fd;
	pkt->write.kind = kind;
	pkt->write.size = wb->size;
	pkt->write.offset = wb->offset;
	req = rdrfpPrepareRequest(id, wb->bufType, wb->bufIndex, pkt);
	rdrfpSetDescExtra(req->desc, wb);
	rdrfpEnqueue(&rdrfpRecvQueue, req);
}

/* ===== SceDeci4pDfMgr ===== */

/* a write buffer for a new file write */
static inline __attribute__((always_inline)) RdrfpWritebuf *createWritebuf(const RdrfpWriteHeader *wh)
{
	int index;

	index = rdrfpReserveBuffer(RDRFP_BUF_WRITE, 0);
	if (index < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 343, "couldn't reserve write buffer\n");
		return NULL;
	}
	return rdrfpCreateWritebuf(wh->fd, wh->offset, RDRFP_BUF_WRITE, index);
}

/*
 * Where a write piece goes: appended to the write buffer collecting its file
 * (*pwb), or on its own in a receive buffer, its header copied in front.
 * Returns the packet the piece's data follows.
 */
static inline __attribute__((always_inline)) void *getWritebuf(RdrfpWriteHeader *wh, RdrfpWritebuf **pwb,
							      int *bufType, int *index)
{
	RdrfpWritebuf *wb = NULL;
	char *buf;

	if (wh->size > RDRFP_PACKET_MAX) {
		wh->size = RDRFP_WRITE_MAX + 1;
	} else {
		switch ((wh->kind >> 1) & 3) {
		case 1:
			wb = rdrfpFindWritebuf(wh->fd, wh->offset, wh->size);
			if (wb == NULL) {
				wb = createWritebuf(wh);
				if (wb == NULL)
					rdrfpFlushWritebuf();
			}
			break;
		case 3:
			wb = createWritebuf(wh);
			if (wb == NULL)
				rdrfpFlushWritebuf();
			break;
		case 0:
			wb = rdrfpFindWritebuf(wh->fd, wh->offset, wh->size);
			break;
		}
		if (wb) {
			*pwb = wb;
			*bufType = wb->bufType;
			*index = wb->bufIndex;
			return wb->buf + RDRFP_BUF_HEADER;
		}
	}

	*index = rdrfpReserveBuffer(RDRFP_BUF_RECV, 1);
	if (*index < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 437, "couldn't reserve buffer\n");
		return NULL;
	}
	buf = rdrfpBufferAddr(RDRFP_BUF_RECV, *index);
	((RdrfpWritePacket *)(buf + RDRFP_BUF_HEADER))->write = *wh;
	*pwb = NULL;
	*bufType = RDRFP_BUF_RECV;
	return buf + RDRFP_BUF_HEADER;
}

static inline __attribute__((always_inline)) int ReadHandler(SceSize size, RdrfpContext *ctx)
{
	RdrfpHeader head;
	RdrfpWriteHeader wh;
	RdrfpWritebuf *wb;
	RdrfpRequest *req;
	RdrfpHeader *packet;
	RdrfpReadState *state;
	int bufType, index, ret;

	if ((int)size < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 479, "param is minus\n");
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceDfMgrRecv(ctx->handle, &head.hdr, sizeof(head.hdr));
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 485, "(0x%08x)\n", ret);
		return ret;
	}
	ret = sceDfMgrRecv(ctx->handle, &head.type, 8);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 490, "(0x%08x)\n", ret);
		return ret;
	}

	if (head.type == RDRFP_CMD_WRITE) {
		ret = sceDfMgrRecv(ctx->handle, &wh, sizeof(wh));
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 497, "(0x%08x)\n", ret);
			return ret;
		}
		packet = getWritebuf(&wh, &wb, &bufType, &index);
		if (packet == NULL) {
			SCE_PRINTF_WITH_INFO(0xf, 502, "(0x%08x)\n", SCE_ERROR_ERRNO_ENOMEM);
			return SCE_ERROR_ERRNO_ENOMEM;
		}
		ret = sceDfMgrRecv(ctx->handle,
				   (char *)packet + sizeof(*packet) + sizeof(wh) + (wb ? wb->size : 0),
				   size - (sizeof(*packet) + sizeof(wh)));
		if (ret < 0)
			goto recv_failed;
		if (wb) {
			rdrfpWritebufAddSize(wb, wh.size);
			rdrfpWritebufAddId(wb, head.id & 0xffff);
			if (wb->size == RDRFP_WRITE_MAX || !(wh.kind & 2) || wb->count > 7)
				rdrfpRequestWritebuf(wb, wh.kind);
			else
				rdrfpPutWritebufToList(wb);
			return ret;
		}
	} else {
		index = rdrfpReserveBuffer(RDRFP_BUF_RECV, 1);
		if (index < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 519, "(0x%08x)\n", SCE_ERROR_ERRNO_ENOMEM);
			return SCE_ERROR_ERRNO_ENOMEM;
		}
		packet = rdrfpBufferAddr(RDRFP_BUF_RECV, index);
		wb = NULL;
		bufType = RDRFP_BUF_RECV;
		ret = sceDfMgrRecv(ctx->handle, packet + 1, size - sizeof(*packet));
		if (ret < 0)
			goto recv_failed;
	}

	*packet = head;
	req = rdrfpPrepareRequest(head.id & 0xffff, bufType, index, packet);
	if (head.type == RDRFP_CMD_READ) {
		state = sceKernelAllocHeapMemory(rdrfpHeap, sizeof(*state));
		if (state == NULL)
			SCE_PANIC(553);
		state->continued = 0;
		state->reserved[0] = 0;
		state->reserved[1] = 0;
		state->reserved[2] = 0;
		rdrfpSetDescExtra(req->desc, state);
	}
	rdrfpEnqueue(&rdrfpRecvQueue, req);
	return ret;

recv_failed:
	SCE_PRINTF_WITH_INFO(0xf, 529, "(0x%08x)\n", ret);
	if (wb == NULL) {
		rdrfpReleaseBuffer(bufType, index);
	} else if (wb->count == 0) {
		rdrfpReleaseBuffer(wb->bufType, wb->bufIndex);
		rdrfpDeleteWritebuf(wb);
	} else {
		rdrfpPutWritebufToList(wb);
	}
	return ret;
}

/* send the next queued reply */
static inline __attribute__((always_inline)) int WriteHandler(RdrfpContext *ctx)
{
	RdrfpRequest *req;
	RdrfpDesc *desc;
	void *packet;
	SceSize total;
	int sent;

	req = rdrfpDequeue(&rdrfpSendQueue, 0);
	if (req == NULL)
		return 0;
	desc = req->desc;
	rdrfpCurrentSending = desc;
	sceKernelFreeHeapMemory(rdrfpHeap, req);
	packet = desc->packet;
	total = SceDeci4pDfMgrForDebugger_CACAB5F9(packet);
	sent = sceDfMgrSend(ctx->handle, packet,
			    desc->dataSize ? total - desc->dataSize : total);
	if (desc->dataSize)
		sent += sceDfMgrSend(ctx->handle, desc->data, desc->dataSize);
	if (total != (SceSize)sent) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 304, "sceDfMgrSend size mismatch 0x%08x <=> 0x%08x\n",
				     total, sent);
		return 0;
	}
	return 0;
}

static inline __attribute__((always_inline)) int WriteDoneHandler(RdrfpContext *ctx)
{
	int ret;

	if (rdrfpCurrentSending == NULL) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 316, "currentSendingRequest is NULL\n");
		return 0;
	}
	if (rdrfpCurrentSending->releaseBuffer)
		rdrfpReleaseBuffer(rdrfpCurrentSending->bufType, rdrfpCurrentSending->bufIndex);
	rdrfpDeleteDesc(rdrfpCurrentSending);
	rdrfpCurrentSending = NULL;
	if (!rdrfpIsQueueEmpty(&rdrfpSendQueue)) {
		ret = sceDfMgrReqSend(ctx->handle);
		if (ret < 0 && ret != (int)RDRFP_DFMGR_ERROR_REQUESTED) {
			SCE_PRINTF_WITH_INFO(0xf, 330, "(0x%08x)\n", ret);
			return 0;
		}
	}
	return 0;
}

static inline __attribute__((always_inline)) int ChangeStatusHandler(const SceDfMgrStatus *status,
								      RdrfpContext *ctx)
{
	if (status->kind == 0) {
		if (status->connected == 0)
			ctx->connected = 0;
		else if (status->connected == 1)
			ctx->connected = 1;
		else
			SCE_DBG_PRINTF_WITH_INFO(0xf, 602, "unknown connection status\n");
	} else if (status->kind == 1) {
		if (status->enabled == 0)
			ctx->enabled = 0;
		else if (status->enabled == 1)
			ctx->enabled = 1;
		else
			SCE_DBG_PRINTF_WITH_INFO(0xf, 620, "unknown protocol status\n");
	} else {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 626, "unknown code\n");
	}
	return 0;
}

static int sceDeci4pRDrfpHandler(int event, void *arg, void *ctx)
{
	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		return ReadHandler((SceSize)arg, ctx);
	case SCE_DFMGR_EVENT_REQ_DONE:
	case 6:
		return 0;
	case SCE_DFMGR_EVENT_SEND:
		return WriteHandler(ctx);
	case SCE_DFMGR_EVENT_SENT:
		return WriteDoneHandler(ctx);
	case SCE_DFMGR_EVENT_STATUS:
		return ChangeStatusHandler(arg, ctx);
	default:
		SCE_DBG_PRINTF_WITH_INFO(0xf, 257, "unknown event (0x%08x)\n", event);
		return 0;
	}
}
