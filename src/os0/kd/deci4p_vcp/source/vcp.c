/*
 * SceDeci4pVcp: DECI4P protocol 0x2B000 ("VCP"), the host's view of the
 * screen (dipsw 0xC2 enables it).
 *
 * A worker thread serves the host's requests one at a time:
 *
 *	0x10	capture the shown frame (DMA into our frame buffer) and describe it
 *	0x12	describe the captured frame again
 *	0x14	send rows of the captured frame
 *	0x16	send the whole captured frame, deflated, 64 KiB per packet
 *
 * Replies carry the request code + 1; an unknown request gets error 8.
 */

#include "sce/cp15.h"
#include <psp2kern/display.h>
#include <psp2kern/kernel/aimgr.h>
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include "sce/sysmem.h"
#include <psp2kern/kernel/sysmem/address_space.h>
#include <psp2kern/kernel/threadmgr.h>
#include "sce/debug.h"
#include "sce/deci4p.h"
#include "sce/display.h"
#include "sce/error.h"
#include "sce/threadmgr.h"
#include "sce/zlib.h"

#define VCP_PROTOCOL		0x2b000
#define VCP_RECV_SIZE		0x200
#define VCP_ZBUF_SIZE		0x10000

/* requests; the reply code is the request code + 1 */
#define VCP_CMD_FRAME		0x10
#define VCP_CMD_INFO		0x12
#define VCP_CMD_LINES		0x14
#define VCP_CMD_DEFLATE		0x16
#define VCP_ERROR_COMMAND	8

/* reply results */
#define VCP_RESULT_RANGE	0x2b001
#define VCP_RESULT_NO_FRAME	0x2b002
#define VCP_RESULT_BUSY		0x2b011

/* event flag bits */
#define EVF_REQUEST		0x4
#define EVF_SENT		0x8
#define EVF_STATUS		0xc

/* VcpCtx.flags */
#define VCP_LINK_UP		0x1
#define VCP_FLAG_2		0x2

typedef struct VcpHeader {
	SceUInt16 size;
	SceUInt8 sizeHi;
	SceUInt8 version;
	SceUInt16 src;
	SceUInt16 dst;
	SceUInt32 protocol;
	SceUInt8 reserved0;
	SceUInt8 reserved1;
	SceUInt16 reserved2;
	SceUInt32 code;
} VcpHeader;

typedef struct VcpRequest {
	VcpHeader hdr;
	union {
		struct {				/* VCP_CMD_FRAME */
			SceUInt16 enable;
			SceUInt16 flags;		/* bit 0: index, bit 1: head */
			SceUID pid;
		};
		struct {				/* VCP_CMD_LINES */
			SceUInt32 start;
			SceUInt32 count;
		};
	};
} VcpRequest;

typedef struct VcpFrameReply {
	VcpHeader hdr;
	SceUInt32 result;
	SceUID pid;
	SceUInt32 vblankcount;
	SceUInt32 width;
	SceUInt32 height;
	SceUInt32 pixelformat;
} VcpFrameReply;

typedef struct VcpLinesReply {
	VcpHeader hdr;
	SceUInt32 result;
	SceUInt32 count;
} VcpLinesReply;

typedef struct VcpDeflateReply {
	VcpHeader hdr;
	SceUInt32 result;
	SceUInt32 remaining;			/* input bytes not deflated yet */
	SceUInt32 total;			/* deflated size, in the last packet */
} VcpDeflateReply;

typedef struct VcpErrorReply {
	VcpHeader hdr;
	SceUInt32 error;
} VcpErrorReply;

typedef struct VcpCtx {
	int handle;				/* sceDfMgrOpen */
	SceUID thread;
	SceUID heap;				/* zlib state */
	SceUID evf;
	SceSize fbSize;
	SceUID fbMemblock;
	void *fbBase;
	uintptr_t fbPaddr;
	SceSize recvLen;
	VcpRequest *req;
	SceUInt32 flags;
	SceUInt32 reserved;
	SceUInt32 lines;
	const void *sendHdr;
	SceSize sendHdrLen;
	const void *sendData;
	SceSize sendDataLen;
	SceUInt32 haveFrame;
	SceDisplayFrameBufInfo info;
	VcpFrameReply frameReply;
	VcpFrameReply infoReply;
	VcpLinesReply linesReply;
	VcpDeflateReply deflateReply;
	VcpErrorReply errorReply;
} VcpCtx;

static SceDfMgrProtocol s_protocol;
static VcpCtx s_vcp;
static SceUInt8 s_recvBuf[VCP_RECV_SIZE];
static SceUInt8 s_zbuf[VCP_ZBUF_SIZE];

/* name unknown: DfMgr event handler (protocol API version 2) */
static int VcpHandler(int event, void *arg, void *ctx)
{
	VcpCtx *vcp = ctx;
	const SceDfMgrStatus *st;
	SceSize len;
	int ret;

	switch (event) {
	case 1:
	case 2:					/* data; 2: the request is complete */
		len = vcp->recvLen;
		if (len < VCP_RECV_SIZE) {
			ret = sceDfMgrRecv(vcp->handle, (SceUInt8 *)vcp->req + len, VCP_RECV_SIZE - len);
			if (ret >= 0)
				vcp->recvLen += ret;
		}
		if (event == 2) {
			vcp->recvLen = 0;
			ksceKernelSetEventFlag(vcp->evf, EVF_REQUEST);
		}
		break;
	case 3:					/* ready to send: the header, then the data */
		if (vcp->sendHdr != NULL) {
			sceDfMgrSend(vcp->handle, vcp->sendHdr, vcp->sendHdrLen);
			vcp->sendHdr = NULL;
			return 0;
		}
		sceDfMgrSend(vcp->handle, vcp->sendData, vcp->sendDataLen);
		vcp->sendData = NULL;
		break;
	case 4:					/* sent */
		if (vcp->sendData == NULL || sceDfMgrReqSend(vcp->handle) < 0)
			ksceKernelSetEventFlag(vcp->evf, EVF_SENT);
		break;
	case 5:
		break;
	case 6:					/* status */
		st = arg;
		if (st->kind == 1) {
			vcp->flags |= VCP_LINK_UP;
			ksceKernelSetEventFlag(vcp->evf, EVF_STATUS);
		} else if (st->kind == 2) {
			vcp->flags &= ~VCP_FLAG_2;
			ksceKernelSetEventFlag(vcp->evf, EVF_STATUS);
		}
		break;
	default:
		SCE_PRINTF_WITH_INFO(0xf, 1082, "unknown event 0x%08X.\n", event);
		return 0;
	}
	return 0;
}

/* name unknown: zlib allocator on our heap */
static void *VcpZAlloc(void *opaque, SceUInt32 items, SceUInt32 size)
{
	return ksceKernelAllocHeapMemory(((VcpCtx *)opaque)->heap, items * size);
}

static void VcpZFree(void *opaque, void *address)
{
	ksceKernelFreeHeapMemory(((VcpCtx *)opaque)->heap, address);
}

static void VcpReplyHeader(VcpHeader *r, const VcpRequest *req, SceSize size, SceUInt32 code)
{
	r->size = size;
	r->sizeHi = size >> 16;
	r->version = req->hdr.version;
	r->src = req->hdr.dst;
	r->dst = req->hdr.src;
	r->protocol = req->hdr.protocol;
	r->reserved0 = 0;
	r->reserved1 = 0;
	r->reserved2 = 0;
	r->code = code;
}

/* send a reply (header, then data) and wait until it is out */
static int VcpSend(const void *hdr, SceSize hdrLen, const void *data, SceSize dataLen)
{
	SceUInt32 bits;
	int ret;

	s_vcp.sendHdr = hdr;
	s_vcp.sendHdrLen = hdrLen;
	s_vcp.sendData = data;
	s_vcp.sendDataLen = dataLen;
	ret = sceDfMgrReqSend(s_vcp.handle);
	if (ret < 0)
		return ret;
	return ksceKernelWaitEventFlag(s_vcp.evf, EVF_SENT, SCE_EVENT_WAITCLEAR_PAT, &bits, NULL);
}

static void VcpClearInfo(void)
{
	SceDisplayFrameBufInfo *info = &s_vcp.info;

	info->resolution = 0;
	info->size = 0;
	info->pid = 0;
	info->vblankcount = 0;
	info->paddr = 0;
	info->framebuf.size = 0;
	info->framebuf.base = NULL;
	info->framebuf.pitch = 0;
	info->framebuf.pixelformat = 0;
	info->framebuf.width = 0;
	info->framebuf.height = 0;
}

/* VCP_CMD_FRAME */
static void VcpCaptureFrame(const VcpRequest *req)
{
	SceDisplayFrameBufInfo *info = &s_vcp.info;
	SceDisplayFrameBuf *fb = &info->framebuf;
	VcpFrameReply *r = &s_vcp.frameReply;
	SceUInt32 result, vblank, width, height, pixfmt;
	SceUID pid;
	SceSize len;
	int head, index, ret;

	if (req->enable == 0) {
		result = 0;
		s_vcp.haveFrame = 0;
		goto clear;
	}

	head = (req->flags >> 1) & 1;
	index = req->flags & 1;
	if (req->hdr.size < 0x1c)
		pid = -1;
	else if ((pid = req->pid) <= 0)
		pid = -1;

	if (ksceDisplayWaitVblankStartInternal(head) < 0)
		goto no_frame;
	info->size = sizeof(*info);
	ret = ksceDisplayGetProcFrameBufInternal(pid, head, index, info);
	if (ret < 0)
		goto failed;
	if (info->paddr == 0 || fb->pitch == 0 || fb->width == 0 || fb->height == 0)
		goto no_frame;

	/* capture it into our frame buffer */
	s_vcp.haveFrame = 0;
	info->size = sizeof(*info);
	info->paddr = s_vcp.fbPaddr;
	fb->size = sizeof(*fb);
	fb->base = s_vcp.fbBase;
	pid = info->pid;
	if (ksceSblAimgrIsDolce()) {
		fb->pitch = 1280;
		fb->width = 1280;
		fb->height = 736;
	} else {
		fb->pitch = 960;
		fb->width = 960;
		fb->height = 544;
	}
	ret = ksceDisplayCaptureFrameBufDMACInternal(pid, head, index, fb);
	if (ret < 0)
		goto failed;
	if (fb->pitch == 0 || fb->width == 0 || fb->height == 0)
		goto no_frame;

	s_vcp.haveFrame = 1;
	result = 0;
	len = sizeof(*r);
	goto describe;

failed:
	if (ret == SCE_KERNEL_ERROR_INVALID_PID && pid != -1) {
		result = VCP_RESULT_BUSY;
		goto old_frame;
	}
no_frame:
	result = VCP_RESULT_NO_FRAME;
old_frame:
	if (!s_vcp.haveFrame) {
clear:
		VcpClearInfo();
		len = result == 0 ? sizeof(*r) : sizeof(VcpHeader) + 4;
		pid = vblank = width = height = pixfmt = 0;
		goto reply;
	}
	len = sizeof(VcpHeader) + 4;
describe:
	pid = info->pid;
	vblank = info->vblankcount;
	width = fb->width;
	height = fb->height;
	pixfmt = fb->pixelformat;
reply:
	VcpReplyHeader(&r->hdr, req, len, VCP_CMD_FRAME + 1);
	r->result = result;
	r->pid = pid;
	r->vblankcount = vblank;
	r->width = width;
	r->height = height;
	r->pixelformat = pixfmt;
	VcpSend(r, len, NULL, 0);
}

/* VCP_CMD_INFO */
static void VcpDescribeFrame(const VcpRequest *req)
{
	VcpFrameReply *r = &s_vcp.infoReply;

	VcpReplyHeader(&r->hdr, req, sizeof(*r), VCP_CMD_INFO + 1);
	r->result = 0;
	r->pid = s_vcp.info.pid;
	r->vblankcount = s_vcp.info.vblankcount;
	r->width = s_vcp.info.framebuf.width;
	r->height = s_vcp.info.framebuf.height;
	r->pixelformat = s_vcp.info.framebuf.pixelformat;
	VcpSend(r, sizeof(*r), NULL, 0);
}

/* VCP_CMD_LINES: rows start.. of the captured frame, as many as fit a packet */
static void VcpSendLines(const VcpRequest *req)
{
	SceDisplayFrameBuf *fb = &s_vcp.info.framebuf;
	VcpLinesReply *r = &s_vcp.linesReply;
	SceUInt32 start, count, result;
	const void *data = NULL;
	SceSize dataLen = 0, size;

	if (!s_vcp.haveFrame) {
		result = VCP_RESULT_NO_FRAME;
		goto error;
	}
	start = req->start;
	count = req->count;
	if (start >= fb->height) {
		result = VCP_RESULT_RANGE;
		goto error;
	}

	if (fb->width != fb->pitch) {
		count = 1;
	} else if (fb->width <= 960) {
		if (count >= 16)
			count = 16;
	} else {
		if (count >= 12)
			count = 12;
		if (fb->width > 1280)
			count = 1;
	}
	if (count >= fb->height - start)
		count = fb->height - start;

	s_vcp.lines = count;
	data = (SceUInt8 *)s_vcp.fbBase + start * fb->pitch * 4;
	dataLen = count * fb->width * 4;
	size = (SceUInt16)((count * fb->width + 7) * 4);
	result = 0;
	goto reply;

error:
	count = 0;
	size = sizeof(VcpHeader) + 4;
reply:
	VcpReplyHeader(&r->hdr, req, size, VCP_CMD_LINES + 1);
	r->result = result;
	r->count = count;
	VcpSend(r, result == 0 ? sizeof(*r) : sizeof(VcpHeader) + 4, data, dataLen);
}

static void VcpDeflateError(const VcpRequest *req)
{
	VcpDeflateReply *r = &s_vcp.deflateReply;

	VcpReplyHeader(&r->hdr, req, sizeof(VcpHeader) + 4, VCP_CMD_DEFLATE + 1);
	r->result = VCP_RESULT_NO_FRAME;
	r->remaining = 0;
	r->total = 0;
	VcpSend(r, sizeof(VcpHeader) + 4, NULL, 0);
}

/* VCP_CMD_DEFLATE: the captured frame, deflated, in 64 KiB packets */
static void pdVcpGetCompPictCmd(const VcpRequest *req)
{
	SceDisplayFrameBuf *fb = &s_vcp.info.framebuf;
	VcpDeflateReply *r = &s_vcp.deflateReply;
	SceZlibStream z;
	SceUInt32 row, remaining, out;
	int init, ret, flush;

	if (!s_vcp.haveFrame) {
		VcpDeflateError(req);
		return;
	}

	z.next_in = NULL;
	z.avail_in = 0;
	z.total_in = 0;
	z.next_out = NULL;
	z.avail_out = 0;
	z.total_out = 0;
	z.msg = NULL;
	z.state = NULL;
	z.data_type = 0;
	z.adler = 0;
	z.reserved = 0;
	z.zalloc = VcpZAlloc;
	z.zfree = VcpZFree;
	z.opaque = &s_vcp;
	init = SceZlibForDriver_25F28DA7(&z, 1, SCE_ZLIB_VERSION, sizeof(z));
	if (init != Z_OK) {
		VcpDeflateError(req);
		return;
	}

	remaining = fb->height * fb->width * 4;
	z.next_out = s_zbuf;
	z.avail_out = VCP_ZBUF_SIZE;
	row = 0;
	do {
		do {
			flush = Z_NO_FLUSH;
			if (z.avail_in == 0) {
				flush = Z_FINISH;
				if (row < fb->height) {
					z.next_in = (SceUInt8 *)s_vcp.fbBase + row * fb->pitch * 4;
					z.avail_in = fb->width * 4;
					row++;
					remaining -= fb->width * 4;
					if (z.avail_in != 0)
						flush = Z_NO_FLUSH;
				}
			}
			ret = SceZlibForDriver_5B718E55(&z, flush);
			if ((unsigned int)ret > Z_STREAM_END) {
				VcpDeflateError(req);
				return;
			}
		} while (ret != Z_STREAM_END && z.avail_out != 0);

		out = VCP_ZBUF_SIZE - z.avail_out;
		VcpReplyHeader(&r->hdr, req, out + sizeof(*r), VCP_CMD_DEFLATE + 1);
		r->result = init;
		r->remaining = z.avail_in + remaining;
		r->total = ret == Z_STREAM_END ? z.total_out : 0;
		if (VcpSend(r, sizeof(*r), s_zbuf, out) < 0)
			break;
		z.next_out = s_zbuf;
		z.avail_out = VCP_ZBUF_SIZE;
	} while (ret != Z_STREAM_END);

	SceZlibForDriver_5492B3F2(&z);
}

static int VcpThread(SceSize args, void *argp)
{
	SceKernelAllocMemBlockKernelOpt opt;
	VcpErrorReply *r;
	VcpRequest *req;
	SceUInt32 bits;
	SceUID uid;
	int ret;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 0x600000;
	uid = ksceKernelAllocMemBlock("SceDeci4pVcpFrameBuf", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_NC_RW,
				      s_vcp.fbSize, &opt);
	if (uid < 0)
		return uid;
	s_vcp.fbMemblock = uid;
	if ((ret = ksceKernelGetMemBlockBase(uid, &s_vcp.fbBase)) < 0 ||
	    (ret = ksceKernelVAtoPA(s_vcp.fbBase, &s_vcp.fbPaddr)) < 0) {
		ksceKernelFreeMemBlock(s_vcp.fbMemblock);
		return ret;
	}
	ksceKernelChangeThreadCpuAffinityMask(0, 0xf);

	for (;;) {
		bits = 0;
		ret = ksceKernelWaitEventFlag(s_vcp.evf, EVF_REQUEST, SCE_EVENT_WAITCLEAR, &bits, NULL);
		if (ret < 0)
			return ret;

		req = s_vcp.req;
		switch (req->hdr.code) {
		case VCP_CMD_FRAME:
			VcpCaptureFrame(req);
			break;
		case VCP_CMD_INFO:
			VcpDescribeFrame(req);
			break;
		case VCP_CMD_LINES:
			VcpSendLines(req);
			break;
		case VCP_CMD_DEFLATE:
			pdVcpGetCompPictCmd(req);
			break;
		default:
			SCE_PRINTF_WITH_INFO(0, 400, "VcpThread : invalid command (0x%08x)\n",
					     req->hdr.code);
			r = &s_vcp.errorReply;
			VcpReplyHeader(&r->hdr, req, sizeof(*r), req->hdr.code);
			r->error = VCP_ERROR_COMMAND;
			VcpSend(r, sizeof(*r), NULL, 0);
			break;
		}
	}
}

static inline int GetCpuId(void)
{
	return readMpidr() & 3;
}

int VcpModuleStart(SceSize args, void *argp)
{
	SceKernelHeapCreateOpt hopt;
	SceKernelThreadOptParam topt;
	SceUID uid;

	if (!ksceKernelCheckDipsw(0xc2)) {
		SCE_DBG_PRINTF(0xb, 127, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}

	memset(&s_vcp, 0, sizeof(s_vcp));
	s_vcp.fbSize = ksceSblAimgrIsDolce() ? 0x3c0000 : 0x200000;
	s_vcp.req = (VcpRequest *)s_recvBuf;

	uid = ksceKernelCreateEventFlag("SceDeci4pVcp", SCE_EVENT_WAITMULTIPLE, 0, NULL);
	if (uid < 0)
		return SCE_KERNEL_START_FAILED;
	s_vcp.evf = uid;

	hopt.size = sizeof(hopt);
	hopt.attr = HEAP_AUTO_EXTEND | HEAP_HAS_MEMTYPE;
	hopt.maxSize = 0;
	hopt.memBlock = 0;
	hopt.memtype = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW;
	hopt.pid = 0;
	hopt.heap = NULL;
	uid = ksceKernelCreateHeap("SceDeci4pVcp", 0x2000, &hopt);
	if (uid < 0)
		goto err_heap;
	s_vcp.heap = uid;

	topt.size = sizeof(topt);
	topt.attr = 1;
	topt.kStackMemType = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW;
	topt.uStackMemType = 0;
	topt.uTLSMemType = 0;
	topt.uStackMemid = 0;
	topt.data_0x18 = 0;
	uid = ksceKernelCreateThread("SceDeci4pVcp", VcpThread, 0x40, 0x2000, 0,
				     (1 << GetCpuId()) ^ 0xf, &topt);
	if (uid < 0)
		goto err_thread;
	s_vcp.thread = uid;
	if (ksceKernelStartThread(uid, 0, NULL) < 0) {
		ksceKernelDeleteThread(s_vcp.thread);
		goto err_thread;
	}

	s_protocol.version = 0x01000000;
	s_protocol.subVersion = 0x20001;
	s_protocol.destination = 0;
	s_protocol.flags = DFMGR_PROTOCOL_ENABLE;
	s_protocol.size = sizeof(s_protocol);
	s_protocol.protocol = VCP_PROTOCOL;
	strncpy(s_protocol.name, "VCP", sizeof(s_protocol.name));
	if (!ksceKernelCheckDipsw(0xc3))
		s_protocol.flags |= DFMGR_PROTOCOL_USB;
	s_vcp.flags = VCP_FLAG_2;
	s_vcp.handle = sceDfMgrOpen(VcpHandler, &s_protocol, &s_vcp);
	return SCE_KERNEL_START_SUCCESS;

err_thread:
	ksceKernelDeleteHeap(s_vcp.heap);
err_heap:
	ksceKernelDeleteEventFlag(s_vcp.evf);
	return SCE_KERNEL_START_FAILED;
}
