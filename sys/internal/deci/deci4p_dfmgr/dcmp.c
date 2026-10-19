/*
 * DCMP, the DECI4P manager protocol: one instance on every link, talking to
 * the host about connections, protocols, power and tool progress (Sony's
 * dcmp.c).
 */

#include <string.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include "dfmgr_internal.h"
#include <kernel/error.h>

static DcmpCB s_dcmp[DFMGR_LINK_COUNT];

static const SceDfMgrProtocol s_dcmpProtocol[DFMGR_LINK_COUNT] = {
	{
		.size = sizeof(SceDfMgrProtocol),
		.protocol = DFMGR_PROTOCOL_DCMP,
		.version = 0x01000400,
		.subVersion = 0x00020004,
		.flags = DFMGR_PROTOCOL_SDIO | DFMGR_PROTOCOL_ENABLE,
		.name = "DCMP",
	},
	{
		.size = sizeof(SceDfMgrProtocol),
		.protocol = DFMGR_PROTOCOL_DCMP,
		.version = 0x01000400,
		.subVersion = 0x00020004,
		.flags = DFMGR_PROTOCOL_USB | DFMGR_PROTOCOL_ENABLE,
		.name = "DCMP",
	},
};

/* 4.00 is built with _FORTIFY_SOURCE: the copies into stack buffers are checked */
#if FW_SINCE(0x04000000)
#define dcmpMemsetChk(dst, c, n) \
	__builtin___memset_chk(dst, c, n, __builtin_object_size(dst, 0))
#define dcmpMemcpyChk(dst, src, n) \
	__builtin___memcpy_chk(dst, src, n, __builtin_object_size(dst, 0))
#define dcmpStrncpyChk(dst, src, n) \
	__builtin___strncpy_chk(dst, src, n, __builtin_object_size(dst, 0))
#else
#define dcmpMemsetChk(dst, c, n)	memset(dst, c, n)
#define dcmpMemcpyChk(dst, src, n)	memcpy(dst, src, n)
#define dcmpStrncpyChk(dst, src, n)	strncpy(dst, src, n)
#endif

static inline DcmpCB *_getDcmpCB(SceUInt32 linkIndex)
{
	if (linkIndex < DFMGR_LINK_COUNT)
		return &s_dcmp[linkIndex];
	SCE_PRINTF_WITH_INFO(15, 2296, "%d\n", linkIndex);
	SCE_DBG_ASSERT_FAIL(2297);
	return NULL;
}

/* the send queue: lock it unless it is full, fill dcmp->buf[dcmp->write], commit */
static inline int dcmpQueueLock(DcmpCB *dcmp, SceKernelIntrStatus *s)
{
	*s = sceKernelSpinlockLowLockCpuSuspendIntr(&dcmp->lock);
	if (dcmp->count >= DCMP_SEND_QUEUE) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&dcmp->lock, *s);
		return DFMGR_ERROR_QUEUE_FULL;
	}
	return 0;
}

static inline void dcmpQueueCommit(DcmpCB *dcmp, SceSize size, int errPending,
				   SceKernelIntrStatus s)
{
	int next = dcmp->write + 1;

	if (next >= DCMP_SEND_QUEUE)
		next = 0;
	dcmp->size[dcmp->write] = size;
	dcmp->write = next;
	dcmp->count++;
	dcmp->errPending = errPending;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&dcmp->lock, s);
	sceDfMgrReqSend(dcmp->handle);
}

/* read and drop the rest of a packet */
static inline void _discardPacket(DcmpCB *dcmp, DcmpHeader *msg)
{
	SceSize size, n;
	int ret;

	size = SceDeci4pDfMgrForDebugger_CACAB5F9(&msg->hdr);
	SCE_DBG_ASSERT_CHECK(size < DFMGR_PACKET_MAX, 825);
	for (size -= sizeof(DcmpHeader); size != 0; size -= ret) {
		n = size > 0x1000 ? 0x1000 : size;
		ret = sceDfMgrRecv(dcmp->handle, &msg->type, n);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(15, 834, "0x%08X\n", ret);
			return;
		}
	}
}

static inline int _dcmpEcho(DcmpCB *dcmp)
{
	DcmpEcho *req = (DcmpEcho *)dcmp->recvBuf;
	DcmpEcho reply;
	SceKernelIntrStatus s;
	SceSize size, n;
	int ret;

	size = SceDeci4pDfMgrForDebugger_CACAB5F9(&req->dcmp.hdr);
	if (size > sizeof(DcmpEcho))
		return 0;
	n = size - sizeof(DcmpHeader);
	ret = sceDfMgrRecv(dcmp->handle, &req->id, n);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 632, "0x%08X\n", ret);
		return 0;
	}
	memset(&reply, 0, sizeof(reply));
	SceDeci4pDfMgrForDebugger_6D26CC56(&reply.dcmp.hdr, size);
	if (n > sizeof(reply.data))
		n = sizeof(reply.data);
	reply.dcmp.hdr.dst = req->dcmp.hdr.src;
	reply.dcmp.type = DCMP_TYPE_ECHO;
	reply.dcmp.code = 1;
	reply.dcmp.reserved = 0;
	reply.id = req->id;
	reply.seq = req->seq + 1;
	dcmpMemcpyChk(reply.data, req->data, n);
	if (dcmpQueueLock(dcmp, &s) < 0) {
		SCE_PRINTF_WITH_INFO(15, 677, "0x%08X\n", DFMGR_ERROR_QUEUE_FULL);
		return 0;
	}
	memcpy(dcmp->buf[dcmp->write], &reply, size);
	dcmpQueueCommit(dcmp, size, 0, s);
	return 0;
}

static inline int _dcmpLinkStart(DcmpCB *dcmp)
{
	SceUInt32 *protocol = (SceUInt32 *)(dcmp->recvBuf + 1);
	int ret;

	ret = sceDfMgrRecv(dcmp->handle, protocol, sizeof(*protocol));
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 772, "0x%08X\n", ret);
		return 0;
	}
	dfmgrStartProtocol(*protocol);
	return 0;
}

static inline int _dcmpLinkStop(DcmpCB *dcmp)
{
	SceUInt32 *protocol = (SceUInt32 *)(dcmp->recvBuf + 1);
	int ret;

	ret = sceDfMgrRecv(dcmp->handle, protocol, sizeof(*protocol));
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 803, "0x%08X\n", ret);
		return 0;
	}
	dfmgrStopProtocol(*protocol);
	return 0;
}

static inline int _dcmpRecv(DcmpCB *dcmp)
{
	DcmpHeader *msg = dcmp->recvBuf;
	int ret;

	ret = sceDfMgrRecv(dcmp->handle, msg, sizeof(*msg));
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 503, "0x%08X\n", ret);
		return ret;
	}
	switch (msg->type) {
	case DCMP_TYPE_CONTROL:
		switch (msg->code) {
		case DCMP_CONTROL_CONNECT:
		case DCMP_CONTROL_CONNECT2:
			if (SceDeci4pDfMgrForDebugger_CACAB5F9(&msg->hdr) == sizeof(DcmpHeader))
				dfmgrConnectLink(dcmp->link, 1);
			break;
		case DCMP_CONTROL_DISCONNECT:
		case DCMP_CONTROL_DISCONNECT2:
			if (SceDeci4pDfMgrForDebugger_CACAB5F9(&msg->hdr) == sizeof(DcmpHeader))
				dfmgrDisconnectLink(dcmp->link, 1);
			break;
		case DCMP_CONTROL_START:
			if (SceDeci4pDfMgrForDebugger_CACAB5F9(&msg->hdr) == sizeof(DcmpHeader) + 4)
				_dcmpLinkStart(dcmp);
			break;
		case DCMP_CONTROL_STOP:
			if (SceDeci4pDfMgrForDebugger_CACAB5F9(&msg->hdr) == sizeof(DcmpHeader) + 4)
				_dcmpLinkStop(dcmp);
			break;
		case DCMP_CONTROL_DISCARD:
			_discardPacket(dcmp, msg);
			break;
		default:
			SCE_PRINTF_WITH_INFO(15, 541, "0x%02x\n", msg->code);
			_discardPacket(dcmp, msg);
			break;
		}
		break;
	case DCMP_TYPE_ECHO:
		if (msg->code != 0) {
			SCE_PRINTF_WITH_INFO(15, 554, "0x%02x\n", msg->code);
			_discardPacket(dcmp, msg);
			break;
		}
		_dcmpEcho(dcmp);
		break;
	case DCMP_TYPE_STATUS:
		if (msg->code < 3 || msg->code > 6) {
			SCE_PRINTF_WITH_INFO(15, 574, "0x%02x\n", msg->code);
			_discardPacket(dcmp, msg);
		}
		break;
	default:
		SCE_PRINTF_WITH_INFO(15, 602, "0x%02x:0x%02x\n", msg->type, msg->code);
		_discardPacket(dcmp, msg);
		break;
	}
	return 0;
}

/* send the packet at the head of the queue (and the rejected one after an error) */
static inline int _dcmpSend(DcmpCB *dcmp)
{
	SceKernelIntrStatus s;
	DcmpHeader *msg;
	SceSize size;
	int count, read, next, errPending, ret;

	s = sceKernelSpinlockLowLockCpuSuspendIntr(&dcmp->lock);
	if (dcmp->count <= 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&dcmp->lock, s);
		return DFMGR_ERROR_NOTHING_TO_SEND;
	}
	errPending = dcmp->errPending;
	msg = dcmp->buf[dcmp->read];
	size = dcmp->size[dcmp->read];
	sceKernelSpinlockLowUnlockCpuResumeIntr(&dcmp->lock, s);

	if (msg->hdr.dst == DCMP_PORT_POWER ||
	    (dfmgrTestLinkStatus(dcmp->link, DFMGR_LINK_READY) &&
	     dfmgrTestLinkStatus(dcmp->link, DFMGR_LINK_ATTACHED))) {
		ret = sceDfMgrSend(dcmp->handle, msg, size);
		if (ret < 0 && ret != (int)0x8024fff2) {
			SCE_PRINTF_WITH_INFO(15, 453, "0x%08X\n", ret);
			return ret;
		}
		if (errPending) {
			sceDfMgrSend(dcmp->handle, dcmp->errBuf, dcmp->errSize);
			dcmp->errSize = 0;
		}
	}

	s = sceKernelSpinlockLowLockCpuSuspendIntr(&dcmp->lock);
	count = dcmp->count;
	read = dcmp->read;
	next = read + 1;
	if (next >= DCMP_SEND_QUEUE)
		next = 0;
	dcmp->errPending = 0;
	dcmp->read = next;
	dcmp->size[read] = 0;
	dcmp->count = count - 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&dcmp->lock, s);
	return 0;
}

static inline int _dcmpHandler(SceUInt32 linkIndex, int event)
{
	DcmpCB *dcmp = _getDcmpCB(linkIndex);

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		return _dcmpRecv(dcmp);
	case SCE_DFMGR_EVENT_REQ_DONE:
	case SCE_DFMGR_EVENT_STATUS:
	case 6:
		return 0;
	case SCE_DFMGR_EVENT_SEND:
		return _dcmpSend(dcmp);
	case SCE_DFMGR_EVENT_SENT:
		if (dcmp->count > 0)
			sceDfMgrReqSend(dcmp->handle);
		return 0;
	default:
		SCE_PRINTF_WITH_INFO(15, 329, "0x%08x\n", event);
		return 0;
	}
}

/* names unknown: the handler of each link's DCMP slot */
static int _dcmpHandlerSdio(int event, void *arg, void *ctx)
{
	return _dcmpHandler(0, event);
}

static int _dcmpHandlerUsb(int event, void *arg, void *ctx)
{
	return _dcmpHandler(1, event);
}

int dcmpInit(DfMgrProtocolCB *pd, SceUInt32 linkIndex, DfMgrLinkCB *link)
{
	DcmpCB *dcmp;
	SceUInt8 *base;
	SceUInt32 i;
	int ret;

	dcmp = _getDcmpCB(linkIndex);
	memset(dcmp, 0, sizeof(*dcmp));
	dcmp->link = link;
	if (linkIndex == 1) {
		dcmp->handle = 0xffe;
		pd->handler = _dcmpHandlerUsb;
		pd->linkIndex = 1;
	} else {
		dcmp->handle = 0xfff;
		pd->handler = _dcmpHandlerSdio;
		pd->linkIndex = 0;
	}
	pd->info = s_dcmpProtocol[linkIndex];
	pd->link = link;
	pd->arg = NULL;
	pd->handle = dcmp->handle;
	pd->flags = 0;
	dcmp->lock = 0;

	ret = sceKernelAllocMemBlockForKernel("SceDeci4pDfMgrDcmp", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW, 0x15000, NULL);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 200, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(201);
		return ret;
	}
	dcmp->memBlock = ret;
	ret = sceKernelGetMemBlockBase(dcmp->memBlock, (void **)&base);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 209, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(210);
		return ret;
	}
	dcmp->recvBuf = (DcmpHeader *)base;
	base += 0x2000;
	for (i = 0; i < DCMP_SEND_QUEUE; i++)
		dcmp->buf[i] = base + i * DCMP_SEND_BUF_SIZE;
	base += 0x2000;
	dcmp->errBuf = base;

	ret = dfmgrSetProtocolStatus(pd, DFMGR_PD_OPEN, 1);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 230, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(231);
		return ret;
	}
	ret = dfmgrSetProtocolStatus(pd, DFMGR_PD_STARTED, 1);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 236, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(237);
		return ret;
	}
	return 0;
}

/* answer a packet nobody takes: an error packet with the rejected one behind it */
void dcmpSendError(DfMgrProtocolCB *pd, SceUInt32 linkIndex, SceUInt8 code,
		   const SceDeci4pHeader *hdr)
{
	DcmpHeader msg;
	SceKernelIntrStatus s;
	DcmpCB *dcmp;
	SceSize size;

	dcmp = _getDcmpCB(linkIndex);
	while (dcmp->errSize != 0)
		sceKernelDelayThread(300);
	memset(&msg, 0, sizeof(msg));
	size = SceDeci4pDfMgrForDebugger_CACAB5F9(hdr);
	*(SceDeci4pHeader *)dcmp->errBuf = *hdr;
	sceDfMgrRecv(pd->handle, (SceDeci4pHeader *)dcmp->errBuf + 1, size - sizeof(*hdr));
	if (size > DFMGR_PACKET_MAX - sizeof(DcmpHeader))
		size = DFMGR_PACKET_MAX - sizeof(DcmpHeader);
	dcmp->errSize = size;
	msg.hdr = *hdr;
	msg.hdr.src = hdr->dst;
	msg.hdr.dst = hdr->src;
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg.hdr, size + sizeof(msg));
	msg.type = DCMP_TYPE_ERROR;
	msg.reserved = 0;
	msg.code = code;
	if (dcmpQueueLock(dcmp, &s) < 0) {
		SCE_PRINTF_WITH_INFO(15, 401, "0x%08X\n", DFMGR_ERROR_QUEUE_FULL);
		dcmp->errSize = 0;
		return;
	}
	memcpy(dcmp->buf[dcmp->write], &msg, sizeof(msg));
	dcmpQueueCommit(dcmp, sizeof(msg), 1, s);
}

void dcmpSendStatusProtocol(SceUInt32 linkIndex, SceUInt32 status,
			    const SceDfMgrProtocol *protocol)
{
	DcmpStatusProtocol msg;
	SceKernelIntrStatus s;
	DcmpCB *dcmp;
	SceUInt16 flags;
	SceUInt32 protocolId, version, subVersion;

	if (status & ~7) {
		SCE_PRINTF_WITH_INFO(15, 854, "0x%08X\n", status);
		return;
	}
	dcmp = _getDcmpCB(linkIndex);
	memset(&msg, 0, sizeof(msg));
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg.dcmp.hdr, sizeof(msg));
	flags = protocol->flags;
	version = protocol->version;
	protocolId = protocol->protocol;
	subVersion = protocol->subVersion;
	msg.dcmp.reserved = 0;
	msg.protocol = protocolId;
	msg.version = version;
	msg.subVersion = subVersion;
	msg.status = status;
	msg.port = (flags & DFMGR_PROTOCOL_USB) ? DCMP_PORT_USB : DCMP_PORT_SDIO;
	msg.dcmp.hdr.dst = DCMP_PORT_HOST;
	msg.dcmp.type = DCMP_TYPE_STATUS;
	msg.dcmp.code = DCMP_STATUS_PROTOCOL;
	if (dcmpQueueLock(dcmp, &s) < 0) {
		SCE_PRINTF_WITH_INFO(15, 896, "0x%08X\n", DFMGR_ERROR_QUEUE_FULL);
		return;
	}
	memcpy(dcmp->buf[dcmp->write], &msg, sizeof(msg));
	dcmpQueueCommit(dcmp, sizeof(msg), 0, s);
}

void dcmpSendStatusProtocolAlias(SceUInt32 linkIndex, const SceDfMgrProtocol *protocol)
{
	DcmpStatusAlias msg;
	SceKernelIntrStatus s;
	DcmpCB *dcmp;

	dcmp = _getDcmpCB(linkIndex);
	memset(&msg, 0, sizeof(msg));
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg.dcmp.hdr, sizeof(msg));
	msg.dcmp.type = DCMP_TYPE_STATUS;
	msg.dcmp.code = DCMP_STATUS_ALIAS;
	msg.dcmp.reserved = 0;
	msg.protocol = protocol->protocol;
	msg.alias = protocol->alias;
	msg.dcmp.hdr.dst = DCMP_PORT_HOST;
	if (dcmpQueueLock(dcmp, &s) < 0) {
		SCE_PRINTF_WITH_INFO(15, 938, "0x%08X\n", DFMGR_ERROR_QUEUE_FULL);
		return;
	}
	memcpy(dcmp->buf[dcmp->write], &msg, sizeof(msg));
	dcmpQueueCommit(dcmp, sizeof(msg), 0, s);
}

void dcmpSendStatusPower(SceUInt32 linkIndex, SceUInt32 kind, SceUInt32 state, SceUInt32 arg)
{
	DcmpStatusPower msg;
	SceKernelIntrStatus s;
	DcmpCB *dcmp;
	SceUInt8 arg2;

	dcmp = _getDcmpCB(linkIndex);
	memset(&msg, 0, sizeof(msg));
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg.dcmp.hdr, sizeof(msg));
	arg2 = (kind == 0 && state == 2) ? arg : 0;
	msg.kind = kind;
	msg.dcmp.code = DCMP_STATUS_POWER;
	msg.dcmp.reserved = 0;
	msg.state = state;
	msg.dcmp.hdr.dst = DCMP_PORT_POWER;
	msg.dcmp.type = DCMP_TYPE_STATUS;
	msg.arg2 = arg2;
	if ((kind == 0x80 || kind == 0x81) && state == 3)
		msg.arg = arg;
	if (dcmpQueueLock(dcmp, &s) < 0) {
		SCE_PRINTF_WITH_INFO(15, 992, "0x%08X\n", DFMGR_ERROR_QUEUE_FULL);
		return;
	}
	memcpy(dcmp->buf[dcmp->write], &msg, sizeof(msg));
	dcmpQueueCommit(dcmp, sizeof(msg), 0, s);
}

void dcmpSendStatusSpace(SceUInt32 linkIndex, SceUInt32 protocol)
{
	DcmpStatusSpace msg;
	SceKernelIntrStatus s;
	DcmpCB *dcmp;

	dcmp = _getDcmpCB(linkIndex);
	memset(&msg, 0, sizeof(msg));
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg.dcmp.hdr, sizeof(msg));
	msg.dcmp.code = DCMP_STATUS_SPACE;
	msg.dcmp.hdr.dst = 0;
	msg.dcmp.reserved = 0;
	msg.protocol = protocol;
	msg.dcmp.type = DCMP_TYPE_STATUS;
	if (dcmpQueueLock(dcmp, &s) < 0) {
		SCE_PRINTF_WITH_INFO(15, 1034, "0x%08X\n", DFMGR_ERROR_QUEUE_FULL);
		return;
	}
	memcpy(dcmp->buf[dcmp->write], &msg, sizeof(msg));
	dcmpQueueCommit(dcmp, sizeof(msg), 0, s);
}

static inline SceSize dcmpProgressSize(SceSize len)
{
	return ((len + 4) & ~3) + offsetof(DcmpProgress, msg);
}

/* the progress reports: started, ongoing and finished, one set per tool */
static inline __attribute__((always_inline))
int dcmpSendProgressStarted(DcmpCB *dcmp, SceUInt8 code, SceUInt8 kind, SceUInt32 total,
			    const char *str, SceSize len, SceUInt16 flag)
{
	SceSize size = dcmpProgressSize(len);
	SceKernelIntrStatus s;
	DcmpProgress msg;

	dcmpMemsetChk(&msg, 0, size);
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg.dcmp.hdr, size);
	msg.value = total;
	msg.kind = kind;
	msg.flag = flag;
	msg.dcmp.type = DCMP_TYPE_STATUS;
	msg.dcmp.code = code;
	msg.msgLen = len + 1;
	msg.dcmp.hdr.dst = 0;
	msg.dcmp.reserved = 0;
	msg.percent = 0;
	dcmpStrncpyChk(msg.msg, str, len);
	if (size > sizeof(msg))
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (dcmpQueueLock(dcmp, &s) < 0)
		return DFMGR_ERROR_QUEUE_FULL;
	memcpy(dcmp->buf[dcmp->write], &msg, size);
	dcmpQueueCommit(dcmp, size, 0, s);
	return 0;
}

static inline __attribute__((always_inline))
int dcmpSendProgressOngoing(DcmpCB *dcmp, SceUInt8 code, SceUInt8 kind, SceUInt8 percent)
{
	DcmpProgress msg;
	SceSize size = dcmpProgressSize(0);
	SceKernelIntrStatus s;

	memset(&msg, 0, size);
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg.dcmp.hdr, size);
	msg.msgLen = 1;
	msg.dcmp.hdr.dst = 0;
	msg.dcmp.reserved = 0;
	msg.kind = kind;
	msg.percent = percent;
	msg.flag = 0;
	msg.value = 0;
	msg.msg[0] = '\0';
	msg.dcmp.type = DCMP_TYPE_STATUS;
	msg.dcmp.code = code;
	if (dcmpQueueLock(dcmp, &s) < 0)
		return DFMGR_ERROR_QUEUE_FULL;
	memcpy(dcmp->buf[dcmp->write], &msg, size);
	dcmpQueueCommit(dcmp, size, 0, s);
	return 0;
}

/* the finished report goes out with sceDfMgrDcmpReqSendForLoadp */
static inline __attribute__((always_inline))
void dcmpSetFinished(DcmpCB *dcmp, SceUInt8 code, SceUInt32 result, const char *str,
		     SceSize len)
{
	memset(&dcmp->pending, 0, sizeof(dcmp->pending));
	dcmp->pending.len = len + 1;
	dcmp->pending.type = DCMP_TYPE_STATUS;
	dcmp->pending.result = result;
	dcmp->pending.code = code;
	strncpy(dcmp->pending.msg, str, len);
}

void dcmpSendStatusUpdateProgressStarted(SceUInt32 linkIndex, SceInt32 kind, SceUInt32 total,
					 const char *msg, SceSize len, SceUInt32 flag)
{
	int ret;

	ret = dcmpSendProgressStarted(_getDcmpCB(linkIndex), DCMP_STATUS_UPDATE_PROGRESS, kind,
				      total, msg, len, flag);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1095, "0x%08X\n", ret);
}

void dcmpSendStatusUpdateProgressOngoing(SceUInt32 linkIndex, SceInt32 kind, SceUInt32 percent)
{
	int ret;

	ret = dcmpSendProgressOngoing(_getDcmpCB(linkIndex), DCMP_STATUS_UPDATE_PROGRESS, kind,
				      percent);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1153, "0x%08X\n", ret);
}

void dcmpSendStatusUpdateFinished(SceUInt32 linkIndex, SceUInt32 result, const char *str,
				  SceSize len)
{
	SceSize size = dcmpProgressSize(len);
	SceKernelIntrStatus s;
	SceUInt32 status;
	DcmpProgress msg;
	DcmpCB *dcmp;
	int ret;

	switch (result) {
	case 0:
		status = 0;
		break;
	case 0x80182803:
		status = 0x103;
		break;
	case 0x80182804:
		status = 0x101;
		break;
	case 0x80182805:
		status = 0x102;
		break;
	case 0x80182806:
		status = 0x100;
		break;
	default:
		status = 0x104;
		break;
	}
	dcmp = _getDcmpCB(linkIndex);
	dcmpMemsetChk(&msg, 0, size);
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg.dcmp.hdr, size);
	msg.status = status;
	msg.value = result;
	msg.dcmp.type = DCMP_TYPE_STATUS;
	msg.dcmp.code = DCMP_STATUS_UPDATE_FINISHED;
	msg.msgLen = len + 1;
	msg.dcmp.hdr.dst = 0;
	msg.dcmp.reserved = 0;
	dcmpStrncpyChk(msg.msg, str, len);
	if (size > sizeof(msg)) {
		ret = DFMGR_ERROR_INVALID_ARGUMENT;
	} else {
		if (dcmpQueueLock(dcmp, &s) >= 0) {
			memcpy(dcmp->buf[dcmp->write], &msg, size);
			dcmpQueueCommit(dcmp, size, 0, s);
			return;
		}
		ret = DFMGR_ERROR_QUEUE_FULL;
	}
	SCE_PRINTF_WITH_INFO(15, 1233, "0x%08X\n", ret);
}

/* report the result of a tool (or of a load) to the host on the SDIO link */
void sceDfMgrDcmpReqSendForLoadp(SceUInt32 result)
{
	DcmpCB *dcmp = &s_dcmp[0];
	SceKernelIntrStatus s;
	SceUInt32 status;
	DcmpProgress msg;
	SceSize size;
	SceUInt16 pendingHeader;
	int ret;

	size = offsetof(DcmpProgress, msg) + 4;
	memcpy(&pendingHeader, &dcmp->pending.type, sizeof(pendingHeader));
	if (pendingHeader != 0)
		size = ((dcmp->pending.len + 3) & ~3) + offsetof(DcmpProgress, msg);

	switch (dcmp->pending.result) {
	case 0x80106B23:
		status = 0x10a;
		break;
	case 0x80101005:
	case SCE_ERROR_ERRNO_ENOENT:
	case 0x80103A07:
		status = 0x103;
		break;
	case SCE_ERROR_ERRNO_EPERM:
		status = 0x102;
		break;
	case 0x80106B21:
		status = 0x108;
		break;
	case 0x80106B22:
		status = 0x109;
		break;
	case 0x80106B24:
		status = 0x10b;
		break;
	case 0x80106B25:
		status = 0x10c;
		break;
	case 0x80106B26:
		status = 0x10d;
		break;
	case 0:
	case 4:
		status = dcmp->pending.result;
		break;
	case 0x80871002:
		status = 0x100;
		break;
	default:
		status = 0x104;
		break;
	}

	dcmpMemsetChk(&msg, 0, size);
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg.dcmp.hdr, size);
	msg.dcmp.hdr.dst = 0;
	memcpy(&pendingHeader, &dcmp->pending.type, sizeof(pendingHeader));
	if (pendingHeader == 0) {
		msg.status = result;
		msg.value = result == 4 ? 0x82 : result;
		msg.dcmp.reserved = 0;
		msg.msg[0] = '\0';
		msg.dcmp.type = DCMP_TYPE_STATUS;
		msg.dcmp.code = DCMP_STATUS_LOADP;
		msg.msgLen = 1;
	} else {
		msg.dcmp.reserved = 0;
		msg.status = status;
		msg.value = dcmp->pending.result == 4 ? 0x82 : dcmp->pending.result;
		msg.dcmp.type = dcmp->pending.type;
		msg.dcmp.code = dcmp->pending.code;
		msg.msgLen = dcmp->pending.len;
		dcmpStrncpyChk(msg.msg, dcmp->pending.msg, dcmp->pending.len);
	}
	if (size > sizeof(msg)) {
		ret = DFMGR_ERROR_INVALID_ARGUMENT;
	} else {
		if (dcmpQueueLock(dcmp, &s) >= 0) {
			memcpy(dcmp->buf[dcmp->write], &msg, size);
			dcmpQueueCommit(dcmp, size, 0, s);
			memset(&dcmp->pending, 0, sizeof(dcmp->pending));
			return;
		}
		ret = DFMGR_ERROR_QUEUE_FULL;
	}
	SCE_PRINTF_WITH_INFO(15, 1365, "0x%08X\n", ret);
}

void dcmpSendStatusInstallProgressStarted(SceUInt32 linkIndex, SceInt32 kind, SceUInt32 total,
					  const char *msg, SceSize len, SceUInt32 flag)
{
	int ret;

	ret = dcmpSendProgressStarted(_getDcmpCB(linkIndex), DCMP_STATUS_INSTALL_PROGRESS, kind,
				      total, msg, len, flag);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1429, "0x%08X\n", ret);
}

void dcmpSendStatusInstallProgressOngoing(SceUInt32 linkIndex, SceInt32 kind, SceUInt32 percent)
{
	int ret;

	ret = dcmpSendProgressOngoing(_getDcmpCB(linkIndex), DCMP_STATUS_INSTALL_PROGRESS, kind,
				      percent);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1487, "0x%08X\n", ret);
}

void dcmpSetStatusInstallFinished(SceUInt32 linkIndex, SceUInt32 result, const char *msg,
				  SceSize len)
{
	dcmpSetFinished(_getDcmpCB(linkIndex), DCMP_STATUS_INSTALL_FINISHED, result, msg, len);
}

void dcmpSendStatusUninstallProgressStarted(SceUInt32 linkIndex, SceInt32 kind, SceUInt32 total,
					    const char *msg, SceSize len, SceUInt32 flag)
{
	int ret;

	ret = dcmpSendProgressStarted(_getDcmpCB(linkIndex), DCMP_STATUS_UNINSTALL_PROGRESS, kind,
				      total, msg, len, flag);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1568, "0x%08X\n", ret);
}

void dcmpSendStatusUninstallProgressOngoing(SceUInt32 linkIndex, SceInt32 kind,
					    SceUInt32 percent)
{
	int ret;

	ret = dcmpSendProgressOngoing(_getDcmpCB(linkIndex), DCMP_STATUS_UNINSTALL_PROGRESS, kind,
				      percent);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1626, "0x%08X\n", ret);
}

void dcmpSetStatusUninstallFinished(SceUInt32 linkIndex, SceUInt32 result, const char *msg,
				    SceSize len)
{
	dcmpSetFinished(_getDcmpCB(linkIndex), DCMP_STATUS_UNINSTALL_FINISHED, result, msg, len);
}

void dcmpSendStatusSaveDataInstallProgressStarted(SceUInt32 linkIndex, SceInt32 kind,
						  SceUInt32 total, const char *msg, SceSize len,
						  SceUInt32 flag)
{
	int ret;

	ret = dcmpSendProgressStarted(_getDcmpCB(linkIndex), DCMP_STATUS_SD_INSTALL_PROGRESS,
				      kind, total, msg, len, flag);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1707, "0x%08X\n", ret);
}

void dcmpSendStatusSaveDataInstallProgressOngoing(SceUInt32 linkIndex, SceInt32 kind,
						  SceUInt32 percent)
{
	int ret;

	ret = dcmpSendProgressOngoing(_getDcmpCB(linkIndex), DCMP_STATUS_SD_INSTALL_PROGRESS,
				      kind, percent);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1765, "0x%08X\n", ret);
}

void dcmpSetStatusSaveDataInstallFinished(SceUInt32 linkIndex, SceUInt32 result,
					  const char *msg, SceSize len)
{
	dcmpSetFinished(_getDcmpCB(linkIndex), DCMP_STATUS_SD_INSTALL_FINISHED, result, msg,
			len);
}

void dcmpSendStatusSaveDataFormatProgressStarted(SceUInt32 linkIndex, SceInt32 kind,
						 SceUInt32 total, const char *msg, SceSize len,
						 SceUInt32 flag)
{
	int ret;

	ret = dcmpSendProgressStarted(_getDcmpCB(linkIndex), DCMP_STATUS_SD_FORMAT_PROGRESS, kind,
				      total, msg, len, flag);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1846, "0x%08X\n", ret);
}

void dcmpSendStatusSaveDataFormatProgressOngoing(SceUInt32 linkIndex, SceInt32 kind,
						 SceUInt32 percent)
{
	int ret;

	ret = dcmpSendProgressOngoing(_getDcmpCB(linkIndex), DCMP_STATUS_SD_FORMAT_PROGRESS, kind,
				      percent);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1904, "0x%08X\n", ret);
}

void dcmpSetStatusSaveDataFormatFinished(SceUInt32 linkIndex, SceUInt32 result,
					 const char *msg, SceSize len)
{
	dcmpSetFinished(_getDcmpCB(linkIndex), DCMP_STATUS_SD_FORMAT_FINISHED, result, msg, len);
}

/* the SCE_DFMGR_EVENT_STATUS packets dfmgr hands to the protocols */
int dcmpMakeStatusConnection(SceUInt32 connected, DcmpStatusConnection *status)
{
	DcmpStatusConnection *msg = status;

	if (status == NULL) {
		SCE_PRINTF_WITH_INFO(15, 2075, "\n");
		return DFMGR_ERROR_INVALID_ARGUMENT;
	}
	if (connected > 1) {
		SCE_PRINTF_WITH_INFO(15, 2081, "0x%08x\n", connected);
		return DFMGR_ERROR_INVALID_ARGUMENT;
	}
	memset(msg, 0, sizeof(*msg));
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg->dcmp.hdr, sizeof(*msg));
	msg->connected = connected;
	msg->dcmp.code = DCMP_STATUS_CONNECTION;
	msg->dcmp.reserved = 0;
	msg->dcmp.hdr.version = SCE_DECI4P_VERSION;
	msg->dcmp.hdr.src = DCMP_PORT_TARGET;
	msg->dcmp.hdr.dst = DCMP_PORT_TARGET;
	msg->dcmp.hdr.protocol = DFMGR_PROTOCOL_DCMP;
	msg->dcmp.type = DCMP_TYPE_STATUS;
	return 0;
}

int dcmpMakeStatusProtocol(SceUInt32 enabled, const SceDfMgrProtocol *protocol,
			   DcmpStatus *status)
{
	DcmpStatusProtocol *msg = &status->protocol;

	if (protocol == NULL || status == NULL) {
		SCE_PRINTF_WITH_INFO(15, 2112, "\n");
		return DFMGR_ERROR_INVALID_ARGUMENT;
	}
	if (enabled > 1) {
		SCE_PRINTF_WITH_INFO(15, 2118, "0x%08x\n", enabled);
		return DFMGR_ERROR_INVALID_ARGUMENT;
	}
	memset(msg, 0, sizeof(*msg));
	SceDeci4pDfMgrForDebugger_6D26CC56(&msg->dcmp.hdr, sizeof(*msg));
	msg->dcmp.type = DCMP_TYPE_STATUS;
	msg->dcmp.code = DCMP_STATUS_PROTOCOL;
	msg->dcmp.hdr.version = SCE_DECI4P_VERSION;
	msg->protocol = protocol->protocol;
	msg->dcmp.reserved = 0;
	msg->version = protocol->version;
	msg->port = (protocol->flags & DFMGR_PROTOCOL_USB) ? DCMP_PORT_USB : DCMP_PORT_SDIO;
	msg->subVersion = protocol->subVersion;
	msg->status = enabled;
	msg->dcmp.hdr.src = DCMP_PORT_TARGET;
	msg->dcmp.hdr.dst = DCMP_PORT_TARGET;
	msg->dcmp.hdr.protocol = DFMGR_PROTOCOL_DCMP;
	return 0;
}

/* name unknown */
int dcmpIsSendQueueEmpty(SceUInt32 linkIndex)
{
	return _getDcmpCB(linkIndex)->count <= 0;
}
