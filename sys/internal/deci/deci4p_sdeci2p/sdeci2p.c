/*
 * SceDeci4pSDeci2p: DECI2 for the PSP compatibility side. Packets cross a
 * shared memory window set up by the compat side (SceDeci2pForDriver_F6427E0F):
 *
 *   acquire  (+0x00000)  compat -> host; interrupt 0x4e says one is ready
 *   forward  (+0x10000, +0x20000)  host -> compat, double buffered;
 *                         interrupt 0x41 says the compat side took one
 *   control  (+0x30000)  break / continue requests for the compat side
 *   ctl      (+0x3f000)  handshake words
 *
 * and a DECI4P channel (protocol 0x1000e000, "SDECI2P") through SDfMgr. Host
 * packets arriving while the compat side is not running go to a local buffer.
 * Both interrupts also have break hooks for when the system is stopped.
 */

#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/barrier.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/qafmgr.h>
#include <deci4p_sdfmgr/sdfmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysroot.h>
#include <threadmgr/threadmgr.h>
#include <compat/compat_regs.h>

#define SDECI2P_PROTOCOL	0x1000e000
#define SDECI2P_INTR_FWD	0x41		/* compat side took a forwarded packet */
#define SDECI2P_INTR_ACQ	0x4e		/* compat side has a packet */
#define SDECI2P_CPU		3
#define SDECI2P_SUSPEND_IDX	0x14
#define SDECI2P_CTL_MAGIC	0xdec1dec1
#define SDECI2P_STOPPED_SIZE	0x11000

#define SDFMGR_ERROR_BUSY	0x80090007

#if FW_DEBUG
#define SDECI2P_PRINTF(_lvl, _line, _fmt, ...)				\
	SceDeci4pSDfMgrForKernel_501CE61D((_lvl), SCE_DBGINFO(_line), _fmt, ##__VA_ARGS__)
/* in code Sony wrote inside _func (shared by helpers here) */
#define SDECI2P_PRINTF_IN(_func, _lvl, _line, _fmt, ...)			\
	SceDeci4pSDfMgrForKernel_501CE61D((_lvl), SCE_DBGINFO_FUNC(_line, _func), _fmt, ##__VA_ARGS__)
#else
#define SDECI2P_PRINTF(_lvl, _line, _fmt, ...)	do { } while (0)
#define SDECI2P_PRINTF_IN(_func, _lvl, _line, _fmt, ...)	do { } while (0)
#endif

/* a packet buffer */
typedef struct Sdeci2pChannel {
	SceDeci4pHeader hdr;
	void *buf;
	SceSize size;
	SceSize count;				/* bytes received / sent */
	SceUInt32 flags;			/* SDECI2P_CH_* */
} Sdeci2pChannel;

#define SDECI2P_CH_FULL		0x1
#define SDECI2P_CH_HDR		0x2		/* header done */

typedef struct Sdeci2pContext {
	SceCompatMailboxRegisters *mailbox;	/* the compat side's doorbells */
	SceUInt32 *ctl;				/* handshake words (shared) */
	SceUInt32 flags;			/* SDECI2P_* */
	Sdeci2pChannel acq;
	Sdeci2pChannel fwd[2];
	SceUInt32 cur;				/* forward buffer being filled */
	SceUInt32 last;				/* forward buffer last handed over */
	Sdeci2pChannel ctrl;
	Sdeci2pChannel stopped;			/* host packets while not running */
	SceUID memblock;
	int handle;
} Sdeci2pContext;

_Static_assert(sizeof(Sdeci2pContext) == 0xbc, "Sdeci2pContext");

#define SDECI2P_RUNNING		0x1
#define SDECI2P_ACKED		0x2		/* the compat side reports which buffer it took */
#define SDECI2P_FORWARDING	0x4
#define SDECI2P_FWD_PENDING	0x8
#define SDECI2P_BREAKING	0x10
#define SDECI2P_CONNECTED	0x10000000

/* the shared window */
#define SHM_WINDOW_SIZE		0x10000
#define SHM_FORWARD(n)		(SHM_WINDOW_SIZE * (1 + (n)))
#define SHM_CONTROL		0x30000
#define SHM_CTL			0x3f000
#define SHM_CONTROL_SIZE	(SHM_CTL - SHM_CONTROL)
#define SHM_SIZE_MIN		0x40000

static Sdeci2pContext s_ctx;

/* name unknown: channel flags and context flags have separate Sony helpers. */
static void sdeci2pSetChannelFlags(SceUInt32 *flags, SceUInt32 bits, int set)
{
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_821FC0EE();
	if (!set)
		*flags &= ~bits;
	else
		*flags |= bits;
	sceKernelCpuResumeIntr(intr);
}

/* name unknown */
static void sdeci2pSetFlags(SceUInt32 *flags, SceUInt32 bits, int set)
{
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_821FC0EE();
	if (!set)
		*flags &= ~bits;
	else
		*flags |= bits;
	sceKernelCpuResumeIntr(intr);
}


static int notifyForwardStart(Sdeci2pContext *ctx, SceUInt32 n)
{
	SceKernelIntrStatus intr;
	SceUInt32 *ctl;

	if (ctx->mailbox == NULL) {
		SDECI2P_PRINTF(0xf, 1572, "not initialized!\n");
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	intr = SceCpuForDriver_821FC0EE();
	sdeci2pSetFlags(&ctx->flags, SDECI2P_FORWARDING, 1);
	ctl = ctx->ctl;
	ctl[11] = n;
	SceCpuForDriver_9CB9F0CE((void *)((SceUIntPtr)&ctl[11] & ~0x3f), 0x40);
	dsb();
	ctx->mailbox->enableInterrupt[1] = 0xffffffff;
	dsb();
	(void)ctx->mailbox->raiseInterrupt[1];
	dsb();
	ctx->mailbox->raiseInterrupt[1] = 1;
	dsb();
	sceKernelCpuResumeIntr(intr);
	return 0;
}

static int notifyAcquireEnd(Sdeci2pContext *ctx)
{
	SceKernelIntrStatus intr;

	if (ctx->mailbox == NULL) {
		SDECI2P_PRINTF(0xf, 1614, "not initialized!\n");
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	intr = SceCpuForDriver_821FC0EE();
	ctx->mailbox->enableInterrupt[14] = 0xffffffff;
	dsb();
	(void)ctx->mailbox->raiseInterrupt[14];
	dsb();
	ctx->mailbox->raiseInterrupt[14] = 1;
	dsb();
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* -- host packets -------------------------------------------------------- */

static int eventReadWhenStopped(Sdeci2pContext *ctx, SceSize size)
{
	Sdeci2pChannel *ch = &ctx->stopped;
	int ret;

	if (size <= sizeof(SceDeci4pHeader) || size > ch->size) {
		SDECI2P_PRINTF(0xf, 801, "packet size error 0x%x\n", size);
		return 0x80090006;
	}
	if (ch->flags & SDECI2P_CH_FULL) {
		SDECI2P_PRINTF(0, 808, "[SDECI2P] NOSPACE\n");
		return 0x8009000b;
	}
	if (!(ch->flags & SDECI2P_CH_HDR)) {
		ret = sceSDfMgrExRecv(ctx->handle, &ch->hdr, sizeof(ch->hdr));
		if (ret < 0) {
			SDECI2P_PRINTF(0, 820, "sceSDfMgrExRecv() failed (0x%08X)\n", ret);
			return ret;
		}
		size -= sizeof(ch->hdr);
		sdeci2pSetChannelFlags(&ch->flags, SDECI2P_CH_HDR, 1);
	}
	ret = sceSDfMgrExRecv(ctx->handle, (char *)ch->buf + ch->count, size);
	if (ret < 0) {
		SDECI2P_PRINTF(0, 837, "sceSDfMgrExRecv() failed (0x%08X)\n", ret);
		return ret;
	}
	ch->count += ret;
	return 0;
}

static inline int eventReadWhenRunning(Sdeci2pContext *ctx, SceSize size)
{
	Sdeci2pChannel *ch = &ctx->fwd[ctx->cur];
	int ret;

	if (size <= sizeof(SceDeci4pHeader) || size > ch->size) {
		SDECI2P_PRINTF(0xf, 745, "packet size error 0x%x\n", size);
		return 0x80090006;
	}
	if (ch->flags & SDECI2P_CH_FULL) {
		SDECI2P_PRINTF(0, 752, "[SDECI2P] NOSPACE\n");
		eventReadWhenStopped(ctx, size);
		return 0x8009000b;
	}
	if (!(ch->flags & SDECI2P_CH_HDR)) {
		ret = sceSDfMgrExRecv(ctx->handle, &ch->hdr, sizeof(ch->hdr));
		if (ret < 0) {
			SDECI2P_PRINTF(0, 765, "sceSDfMgrExRecv() failed (0x%08X)\n", ret);
			return ret;
		}
		size -= sizeof(ch->hdr);
		sdeci2pSetChannelFlags(&ch->flags, SDECI2P_CH_HDR, 1);
	}
	ret = sceSDfMgrExRecv(ctx->handle, (char *)ch->buf + ch->count, size);
	if (ret < 0) {
		SDECI2P_PRINTF(0, 782, "sceSDfMgrExRecv() failed (0x%08X)\n", ret);
		return ret;
	}
	ch->count += ret;
	return 0;
}

/* a whole host packet is in: hand it to the compat side */
static inline int eventReadDoneWhenRunning(Sdeci2pContext *ctx)
{
	Sdeci2pChannel *ch = &ctx->fwd[ctx->cur];
	SceKernelIntrStatus intr;
	int ret;

	sdeci2pSetChannelFlags(&ch->flags, SDECI2P_CH_FULL, 1);
	SceCpuForDriver_9CB9F0CE(ch->buf, (ch->count + 0x3f) & ~0x3f);
	ctx->last = ctx->cur;
	if (ctx->flags & SDECI2P_ACKED) {
		ctx->cur++;
		if (ctx->cur > 1)
			ctx->cur = 0;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (ctx->flags & SDECI2P_FORWARDING) {
		sdeci2pSetFlags(&ctx->flags, SDECI2P_FWD_PENDING, 1);
		sceKernelCpuResumeIntr(intr);
		return 0;
	}
	sceKernelCpuResumeIntr(intr);
	ret = notifyForwardStart(ctx, ctx->last);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 915, "notifyForwardStart() failed (0x%08X)\n", ret);
	return ret;
}

/* send the acquired packet: header, then the rest */
static inline int eventWrite(Sdeci2pContext *ctx)
{
	Sdeci2pChannel *ch = &ctx->acq;
	SceSize left;
	void *p;
	int ret;

	left = SceDeci4pSDfMgrForKernel_68903C1D(&ch->hdr) - ch->count;
	if (!(ch->flags & SDECI2P_CH_HDR)) {
		ret = sceSDfMgrExSend(ctx->handle, &ch->hdr, sizeof(ch->hdr));
		if (ret < 0) {
			SDECI2P_PRINTF(0, 970, "sceSDfMgrExSend() failed (0x%08X)\n", ret);
			return ret;
		}
		sdeci2pSetChannelFlags(&ch->flags, SDECI2P_CH_HDR, 1);
	}
	p = (char *)ch->buf + ch->count;
	SceCpuForDriver_8B4C26DF(p, (left + 0x2f) & ~0x3f);
	ret = sceSDfMgrExSend(ctx->handle, p, left - sizeof(ch->hdr));
	if (ret < 0) {
		SDECI2P_PRINTF(0, 988, "sceSDfMgrExSend() failed (0x%08X)\n", ret);
		return ret;
	}
	ch->count += ret;
	return 0;
}

static void sdeci2pHandler(int event, void *arg, void *data)
{
	Sdeci2pContext *ctx = data;
	SceDfMgrStatus *status = arg;
	int ret;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		if (ctx->flags & SDECI2P_RUNNING)
			ret = eventReadWhenRunning(ctx, (SceSize)arg);
		else
			ret = eventReadWhenStopped(ctx, (SceSize)arg);
		break;
	case SCE_DFMGR_EVENT_REQ_DONE:
		if (!(ctx->flags & SDECI2P_RUNNING)) {
			sdeci2pSetChannelFlags(&ctx->stopped.flags, SDECI2P_CH_FULL, 1);
			sdeci2pSetChannelFlags(&ctx->stopped.flags, SDECI2P_CH_FULL | SDECI2P_CH_HDR, 0);
			ctx->stopped.count = 0;
			return;
		}
		ret = eventReadDoneWhenRunning(ctx);
		break;
	case SCE_DFMGR_EVENT_SEND:
		ret = eventWrite(ctx);
		break;
	case SCE_DFMGR_EVENT_SENT:
		ret = notifyAcquireEnd(ctx);
		if (ret >= 0) {
			sdeci2pSetChannelFlags(&ctx->acq.flags, SDECI2P_CH_FULL | SDECI2P_CH_HDR, 0);
			ctx->acq.count = 0;
			return;
		}
		break;
	case SCE_DFMGR_EVENT_STATUS:
		if (status->kind == 0) {
			if (status->connected == 0) {
				sdeci2pSetFlags(&ctx->flags, SDECI2P_CONNECTED, 0);
				return;
			}
			if (status->connected == 1) {
				sdeci2pSetFlags(&ctx->flags, SDECI2P_CONNECTED, 1);
				return;
			}
		} else if (status->kind == 1) {
			if (status->enabled == 0) {
				sdeci2pSetFlags(&ctx->flags, SDECI2P_CONNECTED, 0);
				return;
			}
			if (status->enabled == 1) {
				sdeci2pSetFlags(&ctx->flags, SDECI2P_CONNECTED, 1);
				return;
			}
		}
		ret = 0x80090001;
		break;
	case 6:
		return;
	default:
		SDECI2P_PRINTF(0xf, 705, "unknown event (0x%08x)\n", event);
		ret = SCE_KERNEL_ERROR_UNSUP;
		break;
	}
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 713, "error (event:%d, 0x%08x)\n", event, ret);
}

/* -- the compat side took a forwarded packet ------------------------------ */

/* name unknown: the buffer the compat side reports it took */
static inline int sdeci2pAckedChannel(Sdeci2pContext *ctx, int line,
				     Sdeci2pChannel **channel)
{
	SceUInt32 n;

	if (!(ctx->flags & SDECI2P_ACKED)) {
		*channel = &ctx->fwd[ctx->cur];
		return 0;
	}
	SceCpuForDriver_8B4C26DF((void *)((SceUIntPtr)&ctx->ctl[3] & ~0x3f), 0x40);
	n = ctx->ctl[3];
	if (n < 2) {
		*channel = &ctx->fwd[n];
		return 0;
	}
	if (n == 2) {
		*channel = &ctx->ctrl;
		return 0;
	}
	if (line == 1117)
		SDECI2P_PRINTF_IN("forwardDonePollHandler", 0xf, 1117, "Unknown ack number (%d)\n", n);
	else
		SDECI2P_PRINTF_IN("forwardDoneIntrHandler", 0xf, 1291, "Unknown ack number (%d)\n", n);
	return -1;
}

/* name unknown */
static inline void sdeci2pForwardDone(Sdeci2pContext *ctx, Sdeci2pChannel *ch, int line)
{
	int ret;

	sdeci2pSetChannelFlags(&ch->flags, SDECI2P_CH_FULL | SDECI2P_CH_HDR, 0);
	ch->count = 0;
	sdeci2pSetFlags(&ctx->flags, SDECI2P_FORWARDING, 0);
	if (ctx->flags & SDECI2P_FWD_PENDING) {
		sdeci2pSetFlags(&ctx->flags, SDECI2P_FWD_PENDING, 0);
		ret = notifyForwardStart(ctx, ctx->last);
		if (ret < 0) {
			if (line == 1150)
				SDECI2P_PRINTF_IN("forwardDonePollHandler", 0xf, 1150, "notifyForwardStart() failed (0x%08X)\n", ret);
			else
				SDECI2P_PRINTF_IN("forwardDoneIntrHandler", 0xf, 1321, "notifyForwardStart() failed (0x%08X)\n", ret);
		}
	}
}

/* break hook */
static void forwardDonePollHandler(void)
{
	Sdeci2pChannel *ch;

	if (sceKernelCpuId() != SDECI2P_CPU ||
	    !sceKernelGetIntrPendingStatusForDebugger(SDECI2P_INTR_FWD))
		return;
	if (sdeci2pAckedChannel(&s_ctx, 1117, &ch) < 0)
		return;
	sceKernelClearIntrPendingStatusForDebugger(SDECI2P_INTR_FWD);
	sdeci2pForwardDone(&s_ctx, ch, 1150);
}

static int forwardDoneIntrHandler(int code, void *arg)
{
	Sdeci2pChannel *ch;

	if (code != SDECI2P_INTR_FWD) {
		SDECI2P_PRINTF(0xf, 1276, "Unknown intrcode (0x%08x)\n", code);
		return -1;
	}
	if (sdeci2pAckedChannel(&s_ctx, 1291, &ch) < 0)
		return -1;
	sdeci2pForwardDone(&s_ctx, ch, 1321);
	return -1;
}

/* -- the compat side has a packet ---------------------------------------- */

/*
 * name unknown: take the acquired packet; DBGP break / continue requests act
 * on the system right away.
 */
static inline __attribute__((always_inline)) void sdeci2pAcquire(Sdeci2pContext *ctx)
{
	SceUInt16 *pkt;

	sdeci2pSetChannelFlags(&ctx->acq.flags, SDECI2P_CH_FULL, 1);
	SceCpuForDriver_8B4C26DF(ctx->acq.buf, 0x40);
	pkt = ctx->acq.buf;
	ctx->acq.count = 0;
	SceDeci4pSDfMgrForKernel_FFC95B63(&ctx->acq.hdr, pkt[0] + sizeof(SceDeci4pHeader));
	if (pkt[2] == 0x340 && ((SceUInt8 *)pkt)[9] == 0) {
		if (((SceUInt8 *)pkt)[10] == 0x15) {
			sdeci2pSetFlags(&ctx->flags, SDECI2P_BREAKING, 1);
			sceSDfMgrBreakSystem();
			sdeci2pSetFlags(&ctx->flags, SDECI2P_BREAKING, 0);
		} else if (pkt[5] == 0x17) {
			sdeci2pSetFlags(&ctx->flags, SDECI2P_BREAKING, 1);
			sceSDfMgrContinueSystem();
			sdeci2pSetFlags(&ctx->flags, SDECI2P_BREAKING, 0);
		}
	}
}

static inline __attribute__((always_inline)) void sdeci2pFinishAcquire(Sdeci2pContext *ctx)
{
	notifyAcquireEnd(ctx);
	sdeci2pSetChannelFlags(&ctx->acq.flags, SDECI2P_CH_FULL | SDECI2P_CH_HDR, 0);
	ctx->acq.count = 0;
}

/* break hook */
static void acquireStartPollHandler(void)
{
	int ret;

	if (sceKernelCpuId() != SDECI2P_CPU ||
	    !sceKernelGetIntrPendingStatusForDebugger(SDECI2P_INTR_ACQ))
		return;
	sceKernelClearIntrPendingStatusForDebugger(SDECI2P_INTR_ACQ);
	sdeci2pAcquire(&s_ctx);
	if (!(s_ctx.flags & SDECI2P_CONNECTED)) {
		sdeci2pFinishAcquire(&s_ctx);
		return;
	}
	ret = sceSDfMgrExReqSend(s_ctx.handle);
	if (ret >= 0 || ret == (int)SDFMGR_ERROR_BUSY) {
		SceDeci4pSDfMgrForKernel_98AEC60E();
		return;
	}
	SDECI2P_PRINTF(0xf, 1254, "ReqSend failed (0x%08X)\n", ret);
}

static inline int forwardPacketForInitialize(Sdeci2pContext *ctx)
{
	Sdeci2pChannel *ch = &ctx->fwd[ctx->cur];
	SceUInt8 *pkt = ch->buf;
	int ret;

	sdeci2pSetChannelFlags(&ch->flags, SDECI2P_CH_FULL, 1);
	SceDeci4pSDfMgrForKernel_8A3B06F2(pkt, 0, 0x18);
	pkt[8] = 0;
	pkt[9] = 0;
	pkt[12] = 0;
	*(SceUInt16 *)&pkt[4] = 1;
	pkt[6] = 0x48;
	pkt[7] = 0x53;
	*(SceUInt16 *)&pkt[0] = 0x18;
	SceCpuForDriver_9CB9F0CE(pkt, 0x40);
	ret = sceKernelDisableIntr(SDECI2P_INTR_FWD);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 1657, "DisableIntr() failed (0x%08X)\n", ret);
	ret = notifyForwardStart(ctx, ctx->cur);
	if (ret < 0)
		return ret;
	while (ctx->fwd[ctx->cur].flags & SDECI2P_CH_FULL)
		forwardDonePollHandler();
	ret = sceKernelEnableIntr(SDECI2P_INTR_FWD);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 1676, "EnableIntr() failed (0x%08X)\n", ret);
	return 0;
}

static int acquireStartIntrHandler(int code, void *arg)
{
	Sdeci2pContext *ctx = &s_ctx;
	SceUInt32 *ctl;
	int ret;

	if (code != SDECI2P_INTR_ACQ) {
		SDECI2P_PRINTF(0xf, 1345, "Unknown intrcode (0x%08x)\n", code);
		return -1;
	}
	if (!(ctx->flags & SDECI2P_RUNNING) ||
	    ((ctx->flags & SDECI2P_ACKED) && ctx->ctl[3] == 0x10000000)) {
		/* the compat side (re)started */
		sdeci2pSetFlags(&ctx->flags, SDECI2P_RUNNING, 0);
		sdeci2pSetFlags(&ctx->flags, 0x1f, 0);
		sdeci2pSetChannelFlags(&ctx->acq.flags, 0xffffffff, 0);
		sdeci2pSetChannelFlags(&ctx->fwd[0].flags, 0xffffffff, 0);
		sdeci2pSetChannelFlags(&ctx->fwd[1].flags, 0xffffffff, 0);
		sdeci2pSetChannelFlags(&ctx->ctrl.flags, 0xffffffff, 0);
		ctx->cur = 0;
		notifyAcquireEnd(ctx);
		sdeci2pSetFlags(&ctx->flags, SDECI2P_RUNNING, 1);
		ctl = ctx->ctl;
		if (ctl != NULL && ctl[1] == SDECI2P_CTL_MAGIC && ctl[2] == 1 && ctl[0] == 0x40) {
			sdeci2pSetFlags(&ctx->flags, SDECI2P_ACKED, 1);
			ctl = ctx->ctl;
			ctl[8] = 0x40;
			ctl[10] = 1;
			ctl[9] = SDECI2P_CTL_MAGIC;
			ctl[11] = 0;
			ctl[12] = 0;
		}
		ret = forwardPacketForInitialize(ctx);
		if (ret < 0)
			SDECI2P_PRINTF(0xf, 1407, "forwardPacketForInitialize() failed (0x%08X)", ret);
		return -1;
	}
	sdeci2pAcquire(ctx);
	if (!(ctx->flags & SDECI2P_CONNECTED)) {
		sdeci2pFinishAcquire(ctx);
		return -1;
	}
	ret = sceSDfMgrExReqSend(ctx->handle);
	if (ret >= 0 || ret == (int)SDFMGR_ERROR_BUSY)
		SceDeci4pSDfMgrForKernel_98AEC60E();
	else
		SDECI2P_PRINTF(0xf, 1486, "ReqSend failed (0x%08X)\n", ret);
	return -1;
}

/* -- debugger control (break / continue the compat side) ---------------- */

/* name unknown */
static inline int sdeci2pForwardControl(Sdeci2pContext *ctx, SceUInt8 type)
{
	SceUInt8 *pkt = ctx->ctrl.buf;

	sdeci2pSetChannelFlags(&ctx->ctrl.flags, SDECI2P_CH_FULL, 1);
	SceDeci4pSDfMgrForKernel_8A3B06F2(pkt, 0, 0x14);
	*(SceUInt16 *)&pkt[0] = 0x14;
	pkt[10] = type;
	*(SceUInt16 *)&pkt[4] = 0x340;
	pkt[7] = 0x53;
	pkt[8] = 0;
	pkt[9] = 0;
	pkt[11] = 0;
	pkt[6] = 0x48;
	SceCpuForDriver_9CB9F0CE(pkt, 0x40);
	return notifyForwardStart(ctx, 2);
}

static inline void forwardDbgpSystemBreak(Sdeci2pContext *ctx)
{
	int ret = sdeci2pForwardControl(ctx, 0x14);

	if (ret < 0)
		SDECI2P_PRINTF(0xf, 1838, "notifyForwardStart() failed (0x%08X)\n", ret);
}

static inline void forwardDbgpSystemContinue(Sdeci2pContext *ctx)
{
	int ret = sdeci2pForwardControl(ctx, 0x16);

	if (ret < 0)
		SDECI2P_PRINTF(0xf, 1879, "notifyForwardStart() failed (0x%08X)\n", ret);
}

static void controlCompat(int id)
{
	if ((s_ctx.flags & (SDECI2P_RUNNING | SDECI2P_ACKED)) != (SDECI2P_RUNNING | SDECI2P_ACKED))
		return;
	if (s_ctx.flags & SDECI2P_BREAKING) {
		/* our own break / continue coming back */
		sdeci2pSetFlags(&s_ctx.flags, SDECI2P_BREAKING, 0);
		return;
	}
	if (id == 1)
		forwardDbgpSystemBreak(&s_ctx);
	else if (id == 0)
		forwardDbgpSystemContinue(&s_ctx);
	else
		SDECI2P_PRINTF(0xf, 1915, "unknown control ID (%d)\n", id);
}

static int suspendHandler(int value, void *args)
{
	int ret;

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == SDECI2P_CPU) {
		ret = sceKernelDisableIntr(SDECI2P_INTR_ACQ);
		if (ret < 0)
			SDECI2P_PRINTF(0xf, 1698, "DisableIntr() failed (0x%08X)\n", ret);
		ret = sceKernelDisableIntr(SDECI2P_INTR_FWD);
		if (ret < 0)
			SDECI2P_PRINTF(0xf, 1705, "DisableIntr() failed (0x%08X)\n", ret);
	}
	sceKernelSysrootCorelockUnlock();
	return 0;
}

/* -- start ------------------------------------------------------------- */

static int initThread(SceSize args, void *argp)
{
	Sdeci2pContext *ctx;
	SceSDfMgrProtocol proto;
	SceKernelIntrRegisterOpt opt;
	void *base;
	int ret;

	if (args != sizeof(ctx)) {
		SDECI2P_PRINTF(0xf, 401, "invalid args (%d)\n", args);
		sceKernelExitThread(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
	}
	ctx = *(Sdeci2pContext **)argp;
	ret = SceSysmemForDriver_C94850C9("SceDeci4pSDeci2p", SceDeci4pSDfMgrForKernel_35238B8A(),
				      SDECI2P_STOPPED_SIZE, NULL);
	if (ret < 0) {
		SDECI2P_PRINTF(0xf, 414, "AllocMemBlockForKernel failed (0x%08x)\n", ret);
		SCE_DBG_ASSERT_FAIL(415);
		return ret;
	}
	ctx->memblock = ret;
	ret = SceSysmemForDriver_A841EDDA(ret, &base);
	if (ret < 0) {
		SDECI2P_PRINTF(0xf, 423, "GetMemBlockBase failed (0x%08x)\n", ret);
		SCE_DBG_ASSERT_FAIL(424);
		return ret;
	}
	ctx->stopped.count = 0;
	ctx->stopped.buf = base;
	ctx->stopped.size = SDECI2P_STOPPED_SIZE;

	proto.size = sizeof(proto);
	proto.protocol = SDECI2P_PROTOCOL;
	proto.alias = 0;
	proto.version = 0x1000000;
	proto.subVersion = 0x1000000;
	proto.destination = 0;
	proto.flags = 1;
	proto.name = "SDECI2P";
	ret = SceDeci4pSDfMgrForKernel_A55F5F0D(sdeci2pHandler, &proto, ctx);
	if (ret < 0) {
		SDECI2P_PRINTF(0xf, 446, "sceSDfMgrOpen() failed (0x%08x)\n", ret);
		return ret;
	}
	ctx->handle = ret;
	sdeci2pSetFlags(&ctx->flags, SDECI2P_CONNECTED, 1);

	ret = SceDeci4pSDfMgrForKernel_B22B42FB(forwardDonePollHandler);
	if (ret < 0) {
		SDECI2P_PRINTF(0xf, 457, "RegisterBreakHook() failed (0x%08x)\n", ret);
		goto close;
	}
	ret = SceDeci4pSDfMgrForKernel_3A25594F(controlCompat);
	if (ret < 0) {
		SDECI2P_PRINTF(0xf, 465, "RegisterControlComponent() failed (0x%08x)\n", ret);
		goto unhook_fwd;
	}
	ret = SceDeci4pSDfMgrForKernel_B22B42FB(acquireStartPollHandler);
	if (ret < 0) {
		SDECI2P_PRINTF(0xf, 472, "RegisterBreakHook() failed (0x%08x)\n", ret);
		goto unregister_control;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(&opt, 0, sizeof(opt));
	opt.attr = SCE_KERNEL_INTR_OPT_HAS_CONFIG;
	opt.size = sizeof(opt);
	opt.config = SCE_KERNEL_INTR_CONFIG_EDGE;
	ret = sceKernelRegisterIntrHandler(SDECI2P_INTR_FWD, "SceDeci4pSDeci2pFwd", 0,
					    forwardDoneIntrHandler, NULL, 0x10, 0x80000, &opt);
	if (ret < 0) {
		SDECI2P_PRINTF(0xf, 492, "RegisterIntrHandler() failed (0x%08X)\n", ret);
		goto unhook_acq;
	}
	ret = sceKernelRegisterIntrHandler(SDECI2P_INTR_ACQ, "SceDeci4pSDeci2pAcq", 0,
					    acquireStartIntrHandler, NULL, 0x10, 0x80000, &opt);
	if (ret < 0) {
		SDECI2P_PRINTF(0xf, 506, "RegisterIntrHandler() failed (0x%08X)\n", ret);
		goto release_fwd;
	}
	ret = sceKernelEnableIntr(SDECI2P_INTR_FWD);
	if (ret < 0) {
		SDECI2P_PRINTF(0xf, 513, "EnableIntr() failed (0x%08X)\n", ret);
		goto release_acq;
	}
	ret = sceKernelEnableIntr(SDECI2P_INTR_ACQ);
	if (ret >= 0) {
		sceKernelExitThread(0);
		return 0;
	}
	SDECI2P_PRINTF(0xf, 520, "EnableIntr() failed (0x%08X)\n", ret);
	ret = sceKernelDisableIntr(SDECI2P_INTR_FWD);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 531, "DisableIntr() failed (0x%08X)\n", ret);
release_acq:
	ret = sceKernelReleaseIntrHandler(SDECI2P_INTR_ACQ);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 539, "ReleaseIntrHandler() failed (0x%08X)\n", ret);
release_fwd:
	ret = sceKernelReleaseIntrHandler(SDECI2P_INTR_FWD);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 547, "ReleaseIntrHandler() failed (0x%08X)\n", ret);
unhook_acq:
	ret = SceDeci4pSDfMgrForKernel_E53400AE(acquireStartPollHandler);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 555, "UnregisterBreakHook() failed (0x%08X)\n", ret);
unregister_control:
	ret = SceDeci4pSDfMgrForKernel_59C077B3(controlCompat);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 564, "UnregisterControlComponent() failed (0x%08X)\n", ret);
unhook_fwd:
	ret = SceDeci4pSDfMgrForKernel_E53400AE(forwardDonePollHandler);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 572, "UnregisterBreakHook() failed (0x%08X)\n", ret);
close:
	SceDeci4pSDfMgrForKernel_86CFC8C6(ctx->handle);
	sceKernelExitThread(ret);
	return ret;
}

int SDeci2pModuleStart(SceSize args, void *argp)
{
	SceKernelThreadOptParam opt;
	Sdeci2pContext *ctx;
	int status, ret;
	SceUID thid;

	if (!SceDipswForDriver_A98FC2FD(0xc1)) {
		SCE_DBG_PRINTF(0xb, 278, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (!SceQafMgrForDriver_382C71E8())
		return SCE_KERNEL_START_NO_RESIDENT;

	ctx = &s_ctx;
	SceDeci4pSDfMgrForKernel_8A3B06F2(&s_ctx, 0, sizeof(s_ctx));
	opt.size = sizeof(opt);
	opt.attr = 1;
	opt.kStackMemType = SceDeci4pSDfMgrForKernel_35238B8A();
	thid = sceKernelCreateThread("SceDeci4pSDeci2p", initThread, 0x1e, 0x2000, 0, 0x80000, &opt);
	if (thid < 0) {
		SDECI2P_PRINTF(0xf, 304, "CreateThread failed (0x%08X)\n", thid);
		return thid;
	}
	ret = sceKernelStartThread(thid, sizeof(ctx), &ctx);
	if (ret < 0) {
		SDECI2P_PRINTF(0xf, 312, "StartThread failed (0x%08X)\n", ret);
		return ret;
	}
	ret = sceKernelWaitThreadEnd(thid, &status, NULL);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 319, "WaitThreadEnd failed (0x%08X)\n", ret);
	ret = sceKernelDeleteThread(thid);
	if (ret < 0)
		SDECI2P_PRINTF(0xf, 326, "DeleteThread failed (0x%08X)\n", ret);
	if (status < 0) {
		SDECI2P_PRINTF(0xf, 332, "init failed (0x%08X)\n", status);
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	SceKernelSuspendForDriver_CE7A2207(SDECI2P_SUSPEND_IDX, suspendHandler, NULL);
	return SCE_KERNEL_START_SUCCESS;
}

/* -- SceDeci2pForDriver ---------------------------------------------------- */

static inline void sdeci2pReset(void)
{
	sdeci2pSetFlags(&s_ctx.flags, 0x1f, 0);
	sdeci2pSetChannelFlags(&s_ctx.acq.flags, 0xffffffff, 0);
	sdeci2pSetChannelFlags(&s_ctx.fwd[0].flags, 0xffffffff, 0);
	sdeci2pSetChannelFlags(&s_ctx.fwd[1].flags, 0xffffffff, 0);
	sdeci2pSetChannelFlags(&s_ctx.ctrl.flags, 0xffffffff, 0);
}

/* name unknown: the compat side's doorbells and shared window (>= 256 KiB) */
int SceDeci2pForDriver_F6427E0F(void *regs, void *shm, SceSize size)
{
	if (size < SHM_SIZE_MIN)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	sdeci2pReset();
	s_ctx.mailbox = regs;
	s_ctx.acq.buf = shm;
	s_ctx.fwd[0].buf = (char *)shm + SHM_FORWARD(0);
	s_ctx.fwd[1].buf = (char *)shm + SHM_FORWARD(1);
	s_ctx.ctrl.buf = (char *)shm + SHM_CONTROL;
	s_ctx.ctl = (SceUInt32 *)((char *)shm + SHM_CTL);
	s_ctx.ctrl.size = SHM_CONTROL_SIZE;
	s_ctx.acq.size = SHM_WINDOW_SIZE;
	s_ctx.fwd[0].size = SHM_WINDOW_SIZE;
	s_ctx.fwd[1].size = SHM_WINDOW_SIZE;
	s_ctx.cur = 0;
	s_ctx.last = 0;
	return 0;
}

int sceDeci2pTermForCompat(void)
{
	SDECI2P_PRINTF(0xf, 1554, "terminate SDECI2p\n");
	sdeci2pReset();
	return 0;
}
