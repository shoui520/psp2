#include "lowio_int.h"

/* =========================================================================
 * Part 3: sceDmac, sceDmacplus_driver & sceKermitMemory_driver
 * ========================================================================= */

typedef struct SceDmacplusLli {
	u32 src;
	u32 dst;
	u32 next;
	u32 attr;
} SceDmacplusLli;

typedef struct SceDmacplusAvcSrc {
	u32 mb_height;
	u32 mb_width;
	u32 interlace;
	u32 field_select;
	u32 y_plane[2];
	u32 cb_plane[2];
	u32 cr_plane[2];
	u32 aux_plane[2];
} SceDmacplusAvcSrc;

typedef struct SceDmacplusCscCoeff {
	u16 coeff_r_y;
	u16 coeff_r_cr;
	u16 coeff_r_cb;
	u16 coeff_g_y;
	u16 coeff_g_cr;
	u16 coeff_g_cb;
	u16 coeff_b_y;
	u16 coeff_b_cr;
	u16 coeff_b_cb;
	u8 y_offset;
	u8 pad_coeff;
	u16 clamp_mode;
} SceDmacplusCscCoeff;

typedef struct SceDmacplusAvcRegs {
	u32 plane_addr[8];
	u32 mode_word;
	u32 dst_primary;
	u32 dst_secondary;
	u32 dst_ctrl;
	u32 csc_word[4];
	u32 start_ctrl;
} SceDmacplusAvcRegs;

typedef struct SceDmacplusChanRegs {
	u32 src;
	u32 dst;
	u32 next;
	u32 attr;
	u32 ctrl;
} SceDmacplusChanRegs;

typedef struct SceKermitArgSlot {
	u32 lo;
	u32 hi;
} SceKermitArgSlot;

typedef struct SceKermitReqPacket {
	u32 cmd;
	u32 seq;
	u64 sema;
	SceKermitArgSlot args[14];
} SceKermitReqPacket;

typedef struct SceDmacplusAvcState {
	SceUID ev_flag_id;
	u32 err_dump[12];
	u32 saved_csc[4];
	u32 dst_stride;
	u32 dst_pixel_fmt;
} SceDmacplusAvcState;

#define DMAC_ALIGN64(buf) ((SceKermitReqPacket *)(((u32)(buf) + 63U) & ~63U))
#define DMAC_UNCACHED_PTR(ptr) \
	((SceKermitReqPacket *)((u32)(ptr) | (u32)((((s32)(ptr) >> 31) + 2) << 29)))

u32 g_dmacplusAttrYuv = 0x1048094B;
u32 g_dmacplusAttrCsc = 0x10480949;
u32 g_dmacplusAttrMemcpy = 0x10480900;
u32 g_dmacplusAttrSc2Me = 0x10482492;

static u16 g_dmacSubIntrMask;
static u16 g_dmacActiveFlags;

/* LCDC DMAC state */
static u32 g_lcdcDmacErrFlag;
static s32 g_lcdcDmacBufState;
static u32 g_lcdcDmacBaseAddr;
static u32 g_lcdcDmacDisplayWidth;
static u32 g_lcdcDmacFrameStride;
static u32 g_lcdcDmacPixelFormat;
static u8 g_lcdcDmacBlankFb[4096];

/* Kermit completion flags for ME DMA channels */
static u32 g_avcKermitDone;
static u32 g_sc2meKermitDone;
static u32 g_me2scKermitDone;

/* AVC DMAC (Channel 1) state */
static SceDmacplusAvcRegs g_avcRegs;
static SceDmacplusAvcState g_avcState;

/* SC2ME DMAC (Channel 2) state */
static SceDmacplusChanRegs g_sc2meRegs;
static SceUID g_sc2meEvFlagId;
static u32 g_sc2meErrDump[4];

/* ME2SC DMAC (Channel 3) state */
static SceDmacplusChanRegs g_me2scRegs;
static SceUID g_me2scEvFlagId;
static u32 g_me2scErrDump[4];

/* SC128 DMAC (Channel 4) state */
static SceUID g_sc128SemaId;
static SceUID g_sc128EvFlagId;
static u32 g_sc128ErrDump[4];

/* Kermit DMAC shared memory / AVC sync state */
static u32 g_kermitAvcSkipPull;
static u32 g_kermitAvcSavedDst;
static u32 g_kermitAvcForceLinePull;
static u32 g_kermitAvcFrameLimit;
static u32 g_kermitAvcFrameCount;
static SceUID g_kermitDmacMutexId;
static u32 g_kermitDmacSmBaseLo;
static u32 g_kermitDmacSmBaseHi;
static u32 g_kermitDmacSmLimit;

/* Forward declarations */
static s32 dmacplusSysEventHandler(int ev_id, char *ev_name, void *ev_param, int *result);
static s32 dmacplusSubIntrIsPending(s32 subintr);
static s32 dmacplusSubIntrClear(s32 subintr);
static s32 dmacplusCheckBusy(void);
static s32 dmacplusLcdcInit(void);
static s32 dmacplusLcdcResume(s32 is_cold);
static s32 dmacplusLcdcTerm(void);
static s32 dmacplusLcdcCheckBusy(void);
static s32 dmacplusLcdcHwStop(void);
static s32 dmacplusLcdcErrIntr(s32 subintr, void *common, s32 status);
static s32 dmacplusAvcInit(void);
static s32 dmacplusAvcTerm(void);
static s32 dmacplusAvcCheckBusy(void);
static s32 dmacplusAvcSuspend(void);
static s32 dmacplusAvcResume(void);
static s32 dmacplusAvcTcIntr(void);
static s32 dmacplusSc2MeInit(void);
static s32 dmacplusSc2MeTerm(void);
static s32 dmacplusSc2MeCheckBusy(void);
static s32 dmacplusSc2MeSuspend(void);
static s32 dmacplusSc2MeResume(void);
static s32 dmacplusSc2MeTcIntr(void);
static s32 dmacplusMe2ScInit(void);
static s32 dmacplusMe2ScTerm(void);
static s32 dmacplusMe2ScCheckBusy(void);
static s32 dmacplusMe2ScSuspend(void);
static s32 dmacplusMe2ScResume(void);
static s32 dmacplusMe2ScTcIntr(void);
static s32 dmacplusSc128Suspend(void);
static s32 dmacplusSc128DoneIntr(s32 subintr, void *common, s32 status);
static s32 dmacplusSc128ErrIntr(s32 subintr, void *common, s32 status);
static s32 dmacplusSc128Init(void);
static s32 dmacplusSc128Term(void);
static s32 dmacplusSc128CheckBusy(void);
static s32 dmacplusSc128Resume(void);
static s32 kermitDmacplusSendCmd(u32 y_step, u32 cb_step, u32 cr_step, u32 aux_step);
static void kermitDmacplusPullAvcOut(u32 plane_y_top, u32 plane_y_bot, u32 plane_cb_top, u32 plane_cb_bot,
                                     u32 plane_cr_top, u32 plane_cr_bot, u32 plane_aux_top, u32 plane_aux_bot,
                                     u32 mode_word, u32 dst_primary, u32 dst_secondary, u32 dst_ctrl);
static s32 kermitDmacplusSubmitAvc(u32 plane_y_top, u32 plane_y_bot, u32 plane_cb_top, u32 plane_cb_bot,
                                   u32 plane_cr_top, u32 plane_cr_bot, u32 plane_aux_top, u32 plane_aux_bot,
                                   u32 mode_word, u32 dst_primary, u32 dst_secondary, u32 dst_ctrl, u32 start_ctrl);
static void kermitDmacplusFinishSc2Me(u32 src, u32 dst, u32 next, u32 attr);
static s32 kermitDmacplusSubmitSc2Me(u32 src, u32 dst, u32 next, u32 attr, u32 ctrl);
static void kermitDmacplusPushMe2ScSrc(u32 src, u32 dst, u32 next, u32 attr);
static s32 kermitDmacplusSubmitMe2Sc(u32 src, u32 dst, u32 next, u32 attr, u32 ctrl);
static s32 kermitDmacplusInitSharedMem(void);
static void kermitDmacplusPullMe2ScDst(u32 src, u32 dst, u32 next, u32 attr);

s32 sceDmacplus_driver_A3AA8D00(u32 base_addr);
s32 sceDmacplus_driver_ED849260(void);
s32 sceDmacplus_driver_BF0DB45E(s32 poll_mode, u32 *err_out);

static SceSysEventHandler g_dmacplusEvent = {
	sizeof(SceSysEventHandler),
	"SceDmacplus",
	0x00FFFF00,
	dmacplusSysEventHandler,
	0, 0, NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static s32 dmacplusSubIntrEnableCb(s32 intr_code, s32 subintr)
{
	(void)intr_code;
	g_dmacSubIntrMask = (u16)(g_dmacSubIntrMask | (1U << subintr));
	return 0;
}

static s32 dmacplusSubIntrDisableCb(s32 intr_code, s32 subintr)
{
	(void)intr_code;
	g_dmacSubIntrMask = (u16)(g_dmacSubIntrMask & ~(1U << subintr));
	return 0;
}

static SceLowioIntrCb g_dmacSubIntrCb = {
	sizeof(SceLowioIntrCb),
	NULL, NULL, NULL, NULL,
	dmacplusSubIntrDisableCb,
	dmacplusSubIntrEnableCb,
	dmacplusSubIntrDisableCb,
	NULL, NULL, NULL
};

static SceLowioIntrOpt g_dmacIntrOpt = {
	sizeof(SceLowioIntrOpt),
	10,
	&g_dmacSubIntrCb
};

static inline u32 dmacplusReadK1(void)
{
	u32 reg_k1;
	__asm__ volatile("move %0, $k1" : "=r"(reg_k1));
	return reg_k1;
}

static inline void dmacplusWriteK1(u32 reg_k1)
{
	__asm__ volatile("move $k1, %0" :: "r"(reg_k1));
}

static s32 __attribute__((noinline)) dmacplusSysEventHandler(int ev_id, char *ev_name, void *ev_param, int *result)
{
	(void)ev_name;
	(void)result;
	const SceLowioSysEventParam *ev = (const SceLowioSysEventParam *)ev_param;
	if (ev_id == 0x402) {
		return dmacplusCheckBusy();
	}
	if (ev_id == 0x4004) {
		if (ev->body.suspend_kind != 2)
			dmacplusLcdcHwStop();
		dmacplusAvcSuspend();
		dmacplusSc2MeSuspend();
		dmacplusMe2ScSuspend();
		dmacplusSc128Suspend();
		return 0;
	}
	if (ev_id == 0x10004) {
		s32 is_cold = (ev->body.resume_flags[1] == 0x80000000U) ? 1 : 0;
		sysregDmacplusBusClockEnable();
		dmacplusLcdcResume(is_cold);
		dmacplusAvcResume();
		dmacplusSc2MeResume();
		dmacplusMe2ScResume();
		dmacplusSc128Resume();
		return 0;
	}
	return 0;
}

static s32 __attribute__((noinline)) dmacplusSubIntrIsPending(s32 subintr)
{
	if (subintr == 0)
		return 0;
	if (subintr >= 10)
		return (s32)0x80000102;
	s32 shift = subintr / 2;
	if ((subintr & 1) != 0)
		return (s32)((REG32(0xBC800018) >> shift) & 1U);
	return (s32)((REG32(0xBC800014) >> shift) & 1U);
}

static s32 __attribute__((noinline)) dmacplusSubIntrClear(s32 subintr)
{
	if (subintr == 0)
		return 0;
	if ((u32)subintr >= 10U)
		return (s32)0x80000102;
	s32 intr = sceKernelCpuSuspendIntr();
	s32 shift = subintr / 2;
	u32 pending;
	if ((subintr & 1) != 0) {
		pending = (REG32(0xBC80000C) >> shift) & 1U;
		REG32(0xBC800010) = pending << shift;
	} else {
		pending = (REG32(0xBC800004) >> shift) & 1U;
		REG32(0xBC800008) = pending << shift;
	}
	sceKernelCpuResumeIntrWithSync(intr);
	return (s32)pending;
}

static s32 dmacplusIntrHandler(s32 intr_code, void *common, void *frame)
{
	(void)common;
	u32 mask = g_dmacSubIntrMask;
	s32 ch = 0;
	s32 subintr = 0;
	do {
		u32 ch_bit = 1U << ch;
		if (subintr != 0 && (REG32(0xBC800004) & ch_bit) != 0) {
			REG32(0xBC800008) = ch_bit;
			if ((mask & (1U << subintr)) != 0)
				sceKernelCallSubIntrHandler(intr_code, subintr, (void *)subintr, frame);
		}
		s32 odd_sub = subintr + 1;
		if ((REG32(0xBC80000C) & ch_bit) != 0) {
			REG32(0xBC800010) = ch_bit;
			if ((mask & (1U << odd_sub)) != 0)
				sceKernelCallSubIntrHandler(intr_code, odd_sub, (void *)odd_sub, frame);
		}
		subintr += 2;
		ch++;
	} while (subintr < 10);
	return -1;
}

s32 dmacplusModuleStart(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	kermitDmacplusInitSharedMem();
	sysregDmacplusBusClockEnable();
	g_dmacSubIntrMask = 0;
	sceKernelRegisterIntrHandler(21, 2, dmacplusIntrHandler, NULL, &g_dmacIntrOpt);
	sceKernelEnableIntr(21);
	dmacplusLcdcInit();
	dmacplusAvcInit();
	dmacplusSc2MeInit();
	dmacplusMe2ScInit();
	dmacplusSc128Init();
	sceKernelRegisterSysEventHandler(&g_dmacplusEvent);
	return 0;
}

s32 dmacplusModuleStop(SceSize args)
{
	(void)args;
	sceKernelUnregisterSysEventHandler(&g_dmacplusEvent);
	dmacplusSc128Term();
	dmacplusMe2ScTerm();
	dmacplusSc2MeTerm();
	dmacplusAvcTerm();
	dmacplusLcdcTerm();
	sceKernelReleaseIntrHandler(21);
	return 0;
}

static s32 __attribute__((noinline)) dmacplusCheckBusy(void)
{
	s32 res = dmacplusLcdcCheckBusy();
	if (res < 0)
		return res;
	res = dmacplusAvcCheckBusy();
	if (res < 0)
		return res;
	res = dmacplusSc2MeCheckBusy();
	if (res < 0)
		return res;
	res = dmacplusMe2ScCheckBusy();
	if (res < 0)
		return res;
	res = dmacplusSc128CheckBusy();
	return (res < 0) ? res : 0;
}

/* --- LCDC DMAC (Channel 0) --- */
static s32 __attribute__((noinline)) dmacplusLcdcInit(void)
{
	memset(g_lcdcDmacBlankFb, 0, sizeof(g_lcdcDmacBlankFb));
	s32 intr = sceKernelCpuSuspendIntr();
	u32 base_addr = 0;
	g_lcdcDmacErrFlag = 0;
	g_lcdcDmacBufState = -1;
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~1U);
	if ((REG32(0xBC800110) & 1U) != 0) {
		g_dmacActiveFlags = (u16)(g_dmacActiveFlags | 1U);
		g_lcdcDmacFrameStride = REG32(0xBC80010C);
		g_lcdcDmacPixelFormat = REG32(0xBC800104);
		g_lcdcDmacDisplayWidth = REG32(0xBC800108);
		if (REG32(0xBC80010C) != 0)
			base_addr = REG32(0xBC800100);
	}
	sceDmacplus_driver_A3AA8D00(base_addr);
	sceKernelRegisterSubIntrHandler(21, 1, dmacplusLcdcErrIntr, NULL);
	sceKernelEnableSubIntr(21, 1);
	REG32(0xBC800110) |= 2U;
	sceKernelCpuResumeIntrWithSync(intr);
	sceDmacplus_driver_ED849260();
	return 0;
}

static s32 __attribute__((noinline)) dmacplusLcdcResume(s32 is_cold)
{
	if (is_cold != 0) {
		g_lcdcDmacBufState = 0;
		sceDmacplus_driver_A3AA8D00(0);
		return 0;
	}
	REG32(0xBC800110) = 2;
	REG32(0xBC800108) = g_lcdcDmacDisplayWidth;
	REG32(0xBC80010C) = g_lcdcDmacFrameStride;
	REG32(0xBC800104) = g_lcdcDmacPixelFormat;
	if (g_lcdcDmacBaseAddr != 0) {
		REG32(0xBC800100) = g_lcdcDmacBaseAddr & 0x1FFFFFFFU;
		REG32(0xBC80010C) = g_lcdcDmacFrameStride;
	} else {
		REG32(0xBC800100) = (u32)g_lcdcDmacBlankFb & 0x1FFFFFFFU;
		REG32(0xBC80010C) = 0;
	}
	if (g_lcdcDmacErrFlag == 0 && (g_dmacActiveFlags & 1U) != 0)
		REG32(0xBC800110) |= 1U;
	return 0;
}

s32 sceDmacplus_driver_A3AA8D00(u32 base_addr)
{
	if (base_addr != 0) {
		if ((base_addr & 0xFU) != 0)
			return (s32)0x80000103;
		u32 top_bits = base_addr >> 29;
		u32 is_vram = (((base_addr >> 23) & 0x3FU) == 8U) && (((0x35U >> top_bits) & 1U) != 0);
		u32 is_ram = ((0x220202U >> (base_addr >> 27)) & 1U) != 0;
		u32 is_spad = (((base_addr >> 14) & 0x7FFFU) == 4U) && (((0x35U >> top_bits) & 1U) != 0);
		if (!is_vram && !is_ram && !is_spad)
			return (s32)0x80000103;
	}
	s32 intr = sceKernelCpuSuspendIntr();
	if (base_addr != 0) {
		u32 is_spad = (((base_addr >> 14) & 0x7FFFU) == 4U) && (((0x35U >> (base_addr >> 29)) & 1U) != 0);
		if (!is_spad) {
			REG32(0xBC800100) = base_addr & 0x1FFFFFFFU;
			if (g_lcdcDmacBufState != 0) {
				REG32(0xBC80010C) = g_lcdcDmacFrameStride;
				g_lcdcDmacBufState = 0;
			}
		} else if (g_lcdcDmacBufState != 2) {
			u32 saved_ctrl = REG32(0xBC800110);
			g_lcdcDmacBufState = 2;
			REG32(0xBC800110) = saved_ctrl & ~1U;
			REG32(0xBC800100) = base_addr & 0x1FFFFFFFU;
			REG32(0xBC80010C) = 0;
			REG32(0xBC800110) = saved_ctrl;
		}
	} else if (g_lcdcDmacBufState != 1) {
		u32 saved_ctrl = REG32(0xBC800110);
		g_lcdcDmacBufState = 1;
		REG32(0xBC800110) = saved_ctrl & ~1U;
		REG32(0xBC800100) = (u32)g_lcdcDmacBlankFb & 0x1FFFFFFFU;
		REG32(0xBC80010C) = 0;
		REG32(0xBC800110) = saved_ctrl;
	}
	g_lcdcDmacBaseAddr = base_addr;
	if (g_lcdcDmacErrFlag != 0 && (g_dmacActiveFlags & 1U) != 0)
		REG32(0xBC800110) |= 1U;
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

s32 sceDmacplus_driver_88ACB6F1(u32 disp_width, u32 frame_stride, u32 pixel_format)
{
	if ((disp_width & 7U) != 0 || (frame_stride & 0x3FU) != 0)
		return (s32)0x80000104;
	if (pixel_format >= 4U)
		return (s32)0x80000108;
	if (g_lcdcDmacErrFlag == 0 && (g_dmacActiveFlags & 1U) != 0)
		return (s32)0x80000021;
	g_lcdcDmacPixelFormat = pixel_format;
	g_lcdcDmacDisplayWidth = disp_width;
	g_lcdcDmacFrameStride = frame_stride;
	REG32(0xBC800108) = disp_width;
	REG32(0xBC80010C) = frame_stride;
	REG32(0xBC800104) = pixel_format;
	return 0;
}

s32 sceDmacplus_driver_33CD36DD(u32 *out_width, u32 *out_stride, u32 *out_format)
{
	if (out_width != NULL)
		*out_width = g_lcdcDmacDisplayWidth;
	if (out_stride != NULL)
		*out_stride = g_lcdcDmacFrameStride;
	if (out_format != NULL)
		*out_format = g_lcdcDmacPixelFormat;
	return 0;
}

s32 sceDmacplus_driver_ED849260(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	s32 prev = (s32)(g_dmacActiveFlags & 1U);
	if (prev == 0 || g_lcdcDmacErrFlag != 0) {
		g_dmacActiveFlags = (u16)(g_dmacActiveFlags | 1U);
		g_lcdcDmacErrFlag = 0;
		u32 base = g_lcdcDmacBaseAddr;
		u32 is_spad = (((base >> 14) & 0x7FFFU) == 4U) && (((0x35U >> (base >> 29)) & 1U) != 0);
		if (base == 0) {
			REG32(0xBC800100) = (u32)g_lcdcDmacBlankFb & 0x1FFFFFFFU;
			REG32(0xBC80010C) = 0;
		} else if (is_spad) {
			REG32(0xBC800100) = base & 0x1FFFFFFFU;
			REG32(0xBC80010C) = 0;
		} else {
			REG32(0xBC800100) = base & 0x1FFFFFFFU;
			REG32(0xBC80010C) = g_lcdcDmacFrameStride;
		}
		REG32(0xBC800110) |= 1U;
	}
	sceKernelCpuResumeIntrWithSync(intr);
	return prev;
}

static s32 __attribute__((noinline)) dmacplusLcdcTerm(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	REG32(0xBC800110) &= ~2U;
	sceKernelReleaseSubIntrHandler(21, 1);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

static s32 __attribute__((noinline)) dmacplusLcdcCheckBusy(void)
{
	return 0;
}

static s32 __attribute__((noinline)) dmacplusLcdcHwStop(void)
{
	REG32(0xBC800110) = 0;
	return 0;
}

u32 sceDmacplus_driver_CCCF6069(void)
{
	return g_lcdcDmacBaseAddr;
}

s32 sceDmacplus_driver_E9B746F9(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	s32 prev = (s32)(g_dmacActiveFlags & 1U);
	if (prev != 0) {
		g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~1U);
		REG32(0xBC800110) &= ~1U;
	}
	sceKernelCpuResumeIntrWithSync(intr);
	return prev;
}

static s32 dmacplusLcdcErrIntr(s32 subintr, void *common, s32 status)
{
	(void)subintr;
	(void)common;
	(void)status;
	REG32(0xBC800110) &= ~1U;
	g_lcdcDmacBaseAddr = 0;
	g_lcdcDmacErrFlag = 1;
	return -1;
}

/* --- AVC DMAC (Channel 1) --- */
s32 sceDmacplus_driver_D126494B(u32 dst_primary, u32 dst_secondary, const SceDmacplusAvcSrc *src_cfg)
{
	if (dst_primary == 0)
		return (s32)0x80000103;
	u32 user_bit = (dmacplusReadK1() >> 31) << 30;
	s32 intr = sceKernelCpuSuspendIntr();
	u16 flags = (u16)(g_dmacActiveFlags | 2U);
	g_dmacActiveFlags = flags;
	if (g_avcState.dst_pixel_fmt != 0)
		g_dmacActiveFlags = (u16)(flags | 0x8000U);
	g_avcRegs.plane_addr[0] = user_bit | (src_cfg->y_plane[0] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[1] = user_bit | (src_cfg->y_plane[1] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[2] = user_bit | (src_cfg->cb_plane[0] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[3] = user_bit | (src_cfg->cb_plane[1] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[4] = user_bit | (src_cfg->cr_plane[0] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[5] = user_bit | (src_cfg->cr_plane[1] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[6] = user_bit | (src_cfg->aux_plane[0] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[7] = user_bit | (src_cfg->aux_plane[1] & 0x1FFFFFFFU);
	g_avcRegs.dst_primary = user_bit | (dst_primary & 0x1FFFFFFFU);
	g_avcRegs.dst_secondary = user_bit | (dst_secondary & 0x1FFFFFFFU);
	if (dst_secondary != 0)
		g_avcRegs.dst_ctrl = (g_avcState.dst_stride << 8) | (g_avcState.dst_pixel_fmt << 1) | 1U;
	else
		g_avcRegs.dst_ctrl = (g_avcState.dst_stride << 8) | (g_avcState.dst_pixel_fmt << 1);
	g_avcRegs.mode_word = ((src_cfg->mb_height & 0x3FU) << 16) |
	                      ((src_cfg->mb_width & 0x3FU) << 8) |
	                      ((src_cfg->interlace & 1U) << 2) |
	                      ((src_cfg->field_select & 1U) << 1);
	g_avcRegs.start_ctrl = 13;
	sceKernelClearEventFlag(g_avcState.ev_flag_id, ~3U);
	sceKernelCpuResumeIntrWithSync(intr);
	kermitDmacplusSubmitAvc(
		g_avcRegs.plane_addr[0], g_avcRegs.plane_addr[1],
		g_avcRegs.plane_addr[2], g_avcRegs.plane_addr[3],
		g_avcRegs.plane_addr[4], g_avcRegs.plane_addr[5],
		g_avcRegs.plane_addr[6], g_avcRegs.plane_addr[7],
		g_avcRegs.mode_word, g_avcRegs.dst_primary,
		g_avcRegs.dst_secondary, g_avcRegs.dst_ctrl, g_avcRegs.start_ctrl);
	return 0;
}

s32 sceDmacplus_driver_9C492B9B(u32 dst_primary, u32 dst_secondary, const SceDmacplusAvcSrc *src_cfg)
{
	if (dst_primary == 0)
		return (s32)0x80000103;
	u32 user_bit = (dmacplusReadK1() >> 31) << 30;
	s32 intr = sceKernelCpuSuspendIntr();
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags | 0x8002U);
	g_avcRegs.plane_addr[0] = user_bit | (src_cfg->y_plane[0] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[2] = user_bit | (src_cfg->y_plane[1] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[4] = user_bit | (src_cfg->cb_plane[0] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[5] = user_bit | (src_cfg->cb_plane[1] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[6] = user_bit | (src_cfg->cr_plane[0] & 0x1FFFFFFFU);
	g_avcRegs.plane_addr[7] = user_bit | (src_cfg->cr_plane[1] & 0x1FFFFFFFU);
	g_avcRegs.dst_primary = user_bit | (dst_primary & 0x1FFFFFFFU);
	g_avcRegs.dst_secondary = user_bit | (dst_secondary & 0x1FFFFFFFU);
	if (dst_secondary != 0)
		g_avcRegs.dst_ctrl = (g_avcState.dst_stride << 8) | (g_avcState.dst_pixel_fmt << 1) | 1U;
	else
		g_avcRegs.dst_ctrl = (g_avcState.dst_stride << 8) | (g_avcState.dst_pixel_fmt << 1);
	g_avcRegs.mode_word = ((src_cfg->mb_height & 0x3FU) << 16) |
	                      ((src_cfg->mb_width & 0x3FU) << 8) |
	                      ((src_cfg->interlace & 1U) << 2) |
	                      ((src_cfg->field_select & 1U) << 1) | 1U;
	g_avcRegs.start_ctrl = 13;
	sceKernelClearEventFlag(g_avcState.ev_flag_id, ~3U);
	sceKernelCpuResumeIntrWithSync(intr);
	kermitDmacplusSubmitAvc(
		g_avcRegs.plane_addr[0], g_avcRegs.plane_addr[1],
		g_avcRegs.plane_addr[2], g_avcRegs.plane_addr[3],
		g_avcRegs.plane_addr[4], g_avcRegs.plane_addr[5],
		g_avcRegs.plane_addr[6], g_avcRegs.plane_addr[7],
		g_avcRegs.mode_word, g_avcRegs.dst_primary,
		g_avcRegs.dst_secondary, g_avcRegs.dst_ctrl, g_avcRegs.start_ctrl);
	return 0;
}

s32 sceDmacplus_driver_BF0DB45E(s32 poll_mode, u32 *err_out)
{
	u32 out_bits;
	s32 res;
	if (poll_mode != 0) {
		if (poll_mode != 1)
			return (s32)0x80000107;
		res = sceKernelPollEventFlag(g_avcState.ev_flag_id, 3, 1, &out_bits);
		if (res == (s32)0x800201AF)
			return 1;
	} else {
		res = sceKernelWaitEventFlag(g_avcState.ev_flag_id, 3, 1, &out_bits, NULL);
	}
	if (res < 0)
		return res;
	if (g_avcKermitDone == 1) {
		kermitDmacplusPullAvcOut(
			g_avcRegs.plane_addr[0], g_avcRegs.plane_addr[1],
			g_avcRegs.plane_addr[2], g_avcRegs.plane_addr[3],
			g_avcRegs.plane_addr[4], g_avcRegs.plane_addr[5],
			g_avcRegs.plane_addr[6], g_avcRegs.plane_addr[7],
			g_avcRegs.mode_word, g_avcRegs.dst_primary,
			g_avcRegs.dst_secondary, g_avcRegs.dst_ctrl);
	}
	g_avcKermitDone = 0;
	if ((out_bits & 2U) != 0) {
		if (err_out != NULL) {
			s32 idx;
			for (idx = 0; idx < 12; idx++)
				err_out[idx] = g_avcState.err_dump[idx];
		}
		g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~0x8002U);
		return -1;
	}
	return res;
}

static s32 __attribute__((noinline)) dmacplusAvcInit(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	memset(&g_avcState, 0, sizeof(g_avcState));
	g_avcState.saved_csc[0] = g_avcRegs.csc_word[0];
	g_avcState.saved_csc[1] = g_avcRegs.csc_word[1];
	g_avcState.saved_csc[2] = g_avcRegs.csc_word[2];
	g_avcState.saved_csc[3] = g_avcRegs.csc_word[3];
	SceUID ev = sceKernelCreateEventFlag("SceDmacplusAvc", 0x201, 1, NULL);
	g_avcKermitDone = 0;
	g_avcState.ev_flag_id = ev;
	if (sceKermit_driver_5280B410(4, dmacplusAvcTcIntr) < 0) {
		Kprintf("fatal error : sceKermitRegisterVirtualIntrHandler(KERMIT_VIRTUAL_INTR_ME_DMA_CH1,_sceDmacplusAvc1TcInterrupt) \n");
		return (s32)0x80020001;
	}
	g_avcRegs.start_ctrl = 0;
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

static s32 __attribute__((noinline)) dmacplusAvcTerm(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_avcRegs.start_ctrl = 0;
	sceKernelReleaseSubIntrHandler(21, 3);
	sceKernelReleaseSubIntrHandler(21, 2);
	sceKernelDeleteEventFlag(g_avcState.ev_flag_id);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

static s32 __attribute__((noinline)) dmacplusAvcCheckBusy(void)
{
	return ((g_dmacActiveFlags & 2U) != 0) ? -1 : 0;
}

static s32 __attribute__((noinline)) dmacplusAvcSuspend(void)
{
	g_avcRegs.start_ctrl = 0;
	return 0;
}

static s32 __attribute__((noinline)) dmacplusAvcResume(void)
{
	g_avcRegs.csc_word[0] = g_avcState.saved_csc[0];
	g_avcRegs.csc_word[1] = g_avcState.saved_csc[1];
	g_avcRegs.csc_word[2] = g_avcState.saved_csc[2];
	g_avcRegs.csc_word[3] = g_avcState.saved_csc[3];
	return 0;
}

s32 sceDmacplus_driver_B269EAC9(u32 dst_fmt, u32 dst_stride, const SceDmacplusCscCoeff *coeff)
{
	s32 intr = sceKernelCpuSuspendIntr();
	if ((g_dmacActiveFlags & 2U) != 0) {
		sceKernelCpuResumeIntr(intr);
		return (s32)0x80000021;
	}
	u32 row_r = (((u32)coeff->coeff_r_cb & 0x3FFU) << 20) |
	            (((u32)coeff->coeff_r_cr & 0x3FFU) << 10) |
	            ((u32)coeff->coeff_r_y & 0x3FFU);
	u32 row_g = (((u32)coeff->coeff_g_cb & 0x3FFU) << 20) |
	            (((u32)coeff->coeff_g_cr & 0x3FFU) << 10) |
	            ((u32)coeff->coeff_g_y & 0x3FFU);
	u32 row_b = (((u32)coeff->coeff_b_cb & 0x3FFU) << 20) |
	            (((u32)coeff->coeff_b_cr & 0x3FFU) << 10) |
	            ((u32)coeff->coeff_b_y & 0x3FFU);
	u32 offset_clamp = (((u32)coeff->clamp_mode & 1U) << 8) | (u32)coeff->y_offset;
	g_avcRegs.csc_word[0] = row_r;
	g_avcState.dst_pixel_fmt = dst_fmt;
	g_avcRegs.csc_word[1] = row_g;
	g_avcState.dst_stride = dst_stride;
	g_avcRegs.csc_word[2] = row_b;
	g_avcState.saved_csc[0] = row_r;
	g_avcRegs.csc_word[3] = offset_clamp;
	g_avcState.saved_csc[1] = row_g;
	g_avcState.saved_csc[2] = row_b;
	g_avcState.saved_csc[3] = offset_clamp;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceDmacplus_driver_F652F09D(u32 y_step, u32 cb_step, u32 cr_step, u32 aux_step)
{
	return kermitDmacplusSendCmd(y_step << 4, cb_step << 4, cr_step << 4, aux_step << 4);
}

static s32 dmacplusAvcTcIntr(void)
{
	g_avcKermitDone = 1;
	sceKernelSetEventFlag(g_avcState.ev_flag_id, 1);
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~0x8002U);
	return 0x10000;
}

/* --- SC2ME DMAC (Channel 2) --- */
s32 sceDmacplus_driver_282CA0D7(const SceDmacplusLli *lli)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_sc2meRegs.src = lli->src;
	g_sc2meRegs.dst = lli->dst;
	g_sc2meRegs.next = lli->next;
	u32 attr = lli->attr;
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags | 4U);
	if ((attr & 0x08000000U) != 0)
		g_sc2meRegs.attr = attr | 0x0A909000U;
	else
		g_sc2meRegs.attr = attr | 0x02489000U;
	g_sc2meRegs.ctrl = 0xC001U;
	sceKernelClearEventFlag(g_sc2meEvFlagId, ~3U);
	sceKernelCpuResumeIntrWithSync(intr);
	kermitDmacplusSubmitSc2Me(g_sc2meRegs.src, g_sc2meRegs.dst,
	                          g_sc2meRegs.next, g_sc2meRegs.attr, g_sc2meRegs.ctrl);
	return 0;
}

s32 sceDmacplus_driver_47D400CB(s32 poll_mode, u32 *err_out)
{
	u32 out_bits;
	s32 res;
	if (poll_mode != 0) {
		if (poll_mode != 1)
			return (s32)0x80000107;
		res = sceKernelPollEventFlag(g_sc2meEvFlagId, 3, 1, &out_bits);
		if (res == (s32)0x800201AF)
			return 1;
	} else {
		res = sceKernelWaitEventFlag(g_sc2meEvFlagId, 3, 1, &out_bits, NULL);
	}
	if (res < 0)
		return res;
	if (g_sc2meKermitDone == 1)
		kermitDmacplusFinishSc2Me(g_sc2meRegs.src, g_sc2meRegs.dst, g_sc2meRegs.next, g_sc2meRegs.attr);
	g_sc2meKermitDone = 0;
	if ((out_bits & 2U) != 0) {
		if (err_out != NULL) {
			err_out[0] = g_sc2meErrDump[0];
			err_out[1] = g_sc2meErrDump[1];
			err_out[2] = g_sc2meErrDump[2];
			err_out[3] = g_sc2meErrDump[3];
		}
		g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~4U);
		return -1;
	}
	return res;
}

static s32 __attribute__((noinline)) dmacplusSc2MeInit(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~4U);
	SceUID ev = sceKernelCreateEventFlag("SceDmacplusSc2Me", 0x201, 1, NULL);
	g_sc2meKermitDone = 0;
	g_sc2meEvFlagId = ev;
	if (sceKermit_driver_5280B410(5, dmacplusSc2MeTcIntr) < 0) {
		Kprintf("fatal error : sceKermitRegisterVirtualIntrHandler(KERMIT_VIRTUAL_INTR_ME_DMA_CH2,_sceDmacplusCh2TcInterruptWrapper) \n");
		return (s32)0x80020001;
	}
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

static s32 __attribute__((noinline)) dmacplusSc2MeTerm(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_sc2meRegs.ctrl = 0;
	sceKernelReleaseSubIntrHandler(21, 5);
	sceKernelReleaseSubIntrHandler(21, 4);
	sceKernelDeleteEventFlag(g_sc2meEvFlagId);
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

static s32 __attribute__((noinline)) dmacplusSc2MeCheckBusy(void)
{
	return ((g_dmacActiveFlags & 4U) != 0) ? -1 : 0;
}

static s32 __attribute__((noinline)) dmacplusSc2MeSuspend(void)
{
	return 0;
}

static s32 __attribute__((noinline)) dmacplusSc2MeResume(void)
{
	return 0;
}

s32 sceDmacplus_driver_9EBF7D3E(u32 dst, u32 src, u32 count)
{
	if (count >= 0x1000U)
		return (s32)0x80000104;
	s32 intr = sceKernelCpuSuspendIntr();
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags | 4U);
	g_sc2meRegs.src = src & 0x1FFFFFFFU;
	g_sc2meRegs.dst = dst & 0x1FFFFFFFU;
	g_sc2meRegs.next = 0;
	g_sc2meRegs.attr = count | 0x82909000U;
	g_sc2meRegs.ctrl = 0xC001U;
	sceKernelClearEventFlag(g_sc2meEvFlagId, ~3U);
	sceKernelCpuResumeIntrWithSync(intr);
	kermitDmacplusSubmitSc2Me(g_sc2meRegs.src, g_sc2meRegs.dst,
	                          g_sc2meRegs.next, g_sc2meRegs.attr, g_sc2meRegs.ctrl);
	return 0;
}

static s32 dmacplusSc2MeTcIntr(void)
{
	g_sc2meKermitDone = 1;
	sceKernelSetEventFlag(g_sc2meEvFlagId, 1);
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~4U);
	return 0x10000;
}

/* --- ME2SC DMAC (Channel 3) --- */
s32 sceDmacplus_driver_5FCF43BD(s32 poll_mode, u32 *err_out)
{
	u32 out_bits;
	s32 res;
	if (poll_mode != 0) {
		if (poll_mode != 1)
			return (s32)0x80000107;
		res = sceKernelPollEventFlag(g_me2scEvFlagId, 3, 1, &out_bits);
		if (res == (s32)0x800201AF)
			return 1;
	} else {
		res = sceKernelWaitEventFlag(g_me2scEvFlagId, 3, 1, &out_bits, NULL);
	}
	if (res < 0)
		return res;
	if (g_me2scKermitDone == 1)
		kermitDmacplusPullMe2ScDst(g_me2scRegs.src, g_me2scRegs.dst, g_me2scRegs.next, g_me2scRegs.attr);
	g_me2scKermitDone = 0;
	if ((out_bits & 2U) != 0) {
		if (err_out != NULL) {
			err_out[0] = g_me2scErrDump[0];
			err_out[1] = g_me2scErrDump[1];
			err_out[2] = g_me2scErrDump[2];
			err_out[3] = g_me2scErrDump[3];
		}
		g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~8U);
		return -1;
	}
	return res;
}

static s32 __attribute__((noinline)) dmacplusMe2ScInit(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~8U);
	SceUID ev = sceKernelCreateEventFlag("SceDmacplusMe2Sc", 0x201, 1, NULL);
	g_me2scKermitDone = 0;
	g_me2scEvFlagId = ev;
	if (sceKermit_driver_5280B410(6, dmacplusMe2ScTcIntr) < 0) {
		Kprintf("fatal error : sceKermitRegisterVirtualIntrHandler(KERMIT_VIRTUAL_INTR_ME_DMA_CH3,_sceDmacplusCh2TcInterruptWrapper) \n");
		return (s32)0x80020001;
	}
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

static s32 __attribute__((noinline)) dmacplusMe2ScTerm(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_me2scRegs.ctrl = 0;
	sceKernelReleaseSubIntrHandler(21, 7);
	sceKernelReleaseSubIntrHandler(21, 6);
	sceKernelDeleteEventFlag(g_me2scEvFlagId);
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

static s32 __attribute__((noinline)) dmacplusMe2ScCheckBusy(void)
{
	return ((g_dmacActiveFlags & 8U) != 0) ? -1 : 0;
}

static s32 __attribute__((noinline)) dmacplusMe2ScSuspend(void)
{
	return 0;
}

static s32 __attribute__((noinline)) dmacplusMe2ScResume(void)
{
	return 0;
}

s32 sceDmacplus_driver_DBD8526F(u32 dst, u32 src, u32 count)
{
	if (count >= 0x1000U)
		return (s32)0x80000104;
	s32 intr = sceKernelCpuSuspendIntr();
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags | 8U);
	g_me2scRegs.src = src & 0x1FFFFFFFU;
	g_me2scRegs.dst = dst & 0x1FFFFFFFU;
	g_me2scRegs.next = 0;
	g_me2scRegs.attr = count | 0x81909000U;
	g_me2scRegs.ctrl = 0xC001U;
	sceKernelClearEventFlag(g_me2scEvFlagId, ~3U);
	sceKernelCpuResumeIntrWithSync(intr);
	kermitDmacplusSubmitMe2Sc(g_me2scRegs.src, g_me2scRegs.dst,
	                          g_me2scRegs.next, g_me2scRegs.attr, g_me2scRegs.ctrl);
	return 0;
}

s32 sceDmacplus_driver_6945F1D3(const SceDmacplusLli *lli)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_me2scRegs.src = lli->src;
	g_me2scRegs.dst = lli->dst;
	g_me2scRegs.next = lli->next;
	g_me2scRegs.attr = lli->attr;
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags | 8U);
	g_me2scRegs.ctrl = 0xC001U;
	sceKernelClearEventFlag(g_me2scEvFlagId, ~3U);
	sceKernelCpuResumeIntrWithSync(intr);
	kermitDmacplusSubmitMe2Sc(g_me2scRegs.src, g_me2scRegs.dst,
	                          g_me2scRegs.next, g_me2scRegs.attr, g_me2scRegs.ctrl);
	return 0;
}

static s32 dmacplusMe2ScTcIntr(void)
{
	g_me2scKermitDone = 1;
	sceKernelSetEventFlag(g_me2scEvFlagId, 1);
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~8U);
	return 0x10000;
}

/* --- SC128 DMAC (Channel 4 / sceDmacMemcpy) --- */
static s32 __attribute__((noinline)) dmacplusSc128Suspend(void)
{
	if ((g_dmacActiveFlags & 0x10U) != 0) {
		s32 subintr;
		for (;;) {
			if (dmacplusSubIntrIsPending(8) != 0) {
				dmacplusSc128DoneIntr(8, NULL, 0);
				subintr = 8;
				break;
			}
			if (dmacplusSubIntrIsPending(9) != 0) {
				dmacplusSc128ErrIntr(9, NULL, 0);
				subintr = 9;
				break;
			}
		}
		dmacplusSubIntrClear(subintr);
	}
	return 0;
}

static s32 dmacplusSc128DoneIntr(s32 subintr, void *common, s32 status)
{
	(void)subintr;
	(void)common;
	(void)status;
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~0x10U);
	sceKernelSetEventFlag(g_sc128EvFlagId, 1);
	sceKernelSignalSema(g_sc128SemaId, 1);
	return -1;
}

static s32 dmacplusSc128ErrIntr(s32 subintr, void *common, s32 status)
{
	(void)subintr;
	(void)common;
	(void)status;
	g_sc128ErrDump[0] = REG32(0xBC8001C0);
	g_sc128ErrDump[1] = REG32(0xBC8001C4);
	g_sc128ErrDump[2] = REG32(0xBC8001C8);
	g_dmacActiveFlags = (u16)(g_dmacActiveFlags & ~0x10U);
	g_sc128ErrDump[3] = REG32(0xBC8001CC);
	sceKernelSetEventFlag(g_sc128EvFlagId, 2);
	return -1;
}

static inline __attribute__((always_inline)) s32 dmacplusSc128CopyImpl(void *dst, const void *src, u32 size, s32 is_try)
{
	if (size == 0)
		return (s32)0x80000104;
	if (dst == NULL || src == NULL)
		return (s32)0x80000103;
	u32 old_k1 = dmacplusReadK1();
	u32 perm_mask = old_k1 << 11;
	dmacplusWriteK1(perm_mask);
	u32 udst = (u32)dst;
	u32 usrc = (u32)src;
	if (((perm_mask & ((usrc + size) | usrc | size)) & 0x80000000U) != 0 ||
	    ((perm_mask & ((udst + size) | udst | size)) & 0x80000000U) != 0) {
		dmacplusWriteK1(old_k1);
		return (s32)0x80000023;
	}
	u32 psrc = (((s32)usrc >= 0) ? 0x40000000U : 0U) | (usrc & 0x1FFFFFFFU);
	u32 pdst = (((s32)udst >= 0) ? 0x40000000U : 0U) | (udst & 0x1FFFFFFFU);
	u32 align_bits = ((usrc ^ udst) & 0xFU) | 0x10U;
	u32 shift = (u32)__builtin_ctz(align_bits);
	u32 burst = (0x12333U >> (shift * 4U)) & 7U;
	u32 attr_base = (shift << 18) | (burst << 12) | (shift << 21) | (burst << 15) | 0x0C000000U;
	u32 align_mask = (1U << shift) - 1U;
	u8 *uncached_dst = (u8 *)(udst | (u32)((((s32)udst >> 31) + 2) << 29));
	const u8 *uncached_src = (const u8 *)(usrc | (u32)((((s32)usrc >> 31) + 2) << 29));
	if ((psrc & align_mask) != 0) {
		s32 head = (s32)(psrc & align_mask) - (s32)(1U << shift);
		size += (u32)head;
		psrc -= (u32)head;
		pdst -= (u32)head;
		do {
			*uncached_dst++ = *uncached_src++;
			head++;
		} while (head != 0);
	}
	u32 tail = size & align_mask;
	if (tail != 0) {
		size -= tail;
		u8 *tail_dst = uncached_dst + size;
		const u8 *tail_src = uncached_src + size;
		do {
			*tail_dst++ = *tail_src++;
			tail--;
		} while (tail != 0);
	}
	if (size <= 0x100U) {
		memcpy(uncached_dst, uncached_src, size);
		dmacplusWriteK1(old_k1);
		return 0;
	}
	s32 res = 0;
	u8 lli_stack[832];
	u32 max_blk = 0xFFFU << shift;
	while (size != 0) {
		SceDmacplusLli *chain = (SceDmacplusLli *)(((u32)lli_stack | 0x3FU) + 1U);
		u32 head_src = psrc;
		u32 head_dst = pdst;
		u32 first_next = 0;
		u32 first_attr;
		s32 lli_count = 0;
		if (size > max_blk) {
			u32 full_attr = attr_base | 0xFFFU;
			u32 last_full_attr = attr_base | 0x80000FFFU;
			first_next = (u32)chain;
			first_attr = full_attr;
			psrc += max_blk;
			pdst += max_blk;
			size -= max_blk;
			SceDmacplusLli *cur = chain;
			for (lli_count = 0; lli_count < 48; ) {
				cur->src = psrc;
				cur->dst = pdst;
				if (size <= max_blk) {
					cur->next = 0;
					cur->attr = attr_base | (size >> shift) | 0x80000000U;
					size = 0;
					lli_count++;
					break;
				}
				if (lli_count == 47) {
					cur->next = 0;
					cur->attr = last_full_attr;
				} else {
					cur->next = (u32)(cur + 1) & 0x1FFFFFFFU;
					cur->attr = full_attr;
				}
				lli_count++;
				cur++;
				psrc += max_blk;
				pdst += max_blk;
				size -= max_blk;
			}
		} else {
			first_attr = attr_base | (size >> shift) | 0x80000000U;
			size = 0;
		}
		s32 sema_res = (is_try != 0)
			? sceKernelPollSema(g_sc128SemaId, 1)
			: sceKernelWaitSema(g_sc128SemaId, 1, NULL);
		if (sema_res < 0) {
			dmacplusWriteK1(old_k1);
			return (sema_res == (s32)0x800201AD) ? (s32)0x80000021 : sema_res;
		}
		if (first_next != 0) {
			s32 lines = (lli_count + 3) / 4;
			u8 *line_ptr = (u8 *)first_next;
			s32 line_i;
			for (line_i = lines; line_i > 0; line_i--) {
				__asm__ volatile("cache 0x1a, 0(%0)" :: "r"(line_ptr));
				line_ptr += 64;
			}
			u32 cop_cfg;
			__asm__ volatile("mfc0 %0, $24" : "=r"(cop_cfg));
			if ((cop_cfg & 1U) != 0) {
				__asm__ volatile("sync");
				REG32(0xA7F00000) = (first_next & 0x07FFFFC0U) | ((u32)(lines - 1) & 0x3FU) | 0xA0000000U;
			}
		}
		s32 intr = sceKernelCpuSuspendIntr();
		while ((s16)g_dmacActiveFlags < 0) {
			sceKernelCpuResumeIntr(intr);
			sceDmacplus_driver_BF0DB45E(0, NULL);
			intr = sceKernelCpuSuspendIntr();
		}
		g_dmacActiveFlags = (u16)(g_dmacActiveFlags | 0x10U);
		REG32(0xBC8001C0) = head_src;
		REG32(0xBC8001C4) = head_dst;
		REG32(0xBC8001C8) = first_next & 0x1FFFFFFFU;
		REG32(0xBC8001CC) = first_attr;
		REG32(0xBC8001D0) = 0xC001U;
		sceKernelCpuResumeIntrWithSync(intr);
		u32 out_bits;
		res = sceKernelWaitEventFlag(g_sc128EvFlagId, 3, 0x21, &out_bits, NULL);
		if (res == 0 && (out_bits & 2U) != 0) {
			res = (s32)0x80000103;
			sceKernelSignalSema(g_sc128SemaId, 1);
		}
	}
	dmacplusWriteK1(old_k1);
	return res;
}

s32 sceDmacMemcpy(void *dst, const void *src, u32 size)
{
	return dmacplusSc128CopyImpl(dst, src, size, 0);
}

s32 sceDmacplus_driver_D57D5BC2(void *dst, const void *src, u32 size)
{
	return dmacplusSc128CopyImpl(dst, src, size, 1);
}

s32 sceDmacplus_driver_B0EC610D(const SceDmacplusLli *lli)
{
	s32 res = sceKernelWaitSema(g_sc128SemaId, 1, NULL);
	if (res < 0)
		return res;
	const volatile SceDmacplusLli *uncached_lli =
		(const volatile SceDmacplusLli *)(((u32)lli & 0x1FFFFFFFU) | 0xA0000000U);
	REG32(0xBC8001C0) = uncached_lli->src;
	REG32(0xBC8001C4) = uncached_lli->dst;
	REG32(0xBC8001C8) = uncached_lli->next;
	REG32(0xBC8001CC) = uncached_lli->attr;
	REG32(0xBC8001D0) = 0xC001U;
	__asm__ volatile("sync");
	u32 out_bits;
	res = sceKernelWaitEventFlag(g_sc128EvFlagId, 3, 0x21, &out_bits, NULL);
	if (res != 0 || (out_bits & 2U) == 0)
		return res;
	sceKernelSignalSema(g_sc128SemaId, 1);
	return (s32)0x80000103;
}

static s32 __attribute__((noinline)) dmacplusSc128Init(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_sc128SemaId = sceKernelCreateSema("SceDmacplusSc128", 1, 1, 1, NULL);
	g_sc128EvFlagId = sceKernelCreateEventFlag("SceDmacplusSc128", 0x201, 0, NULL);
	sceKernelRegisterSubIntrHandler(21, 8, dmacplusSc128DoneIntr, NULL);
	sceKernelRegisterSubIntrHandler(21, 9, dmacplusSc128ErrIntr, NULL);
	sceKernelEnableSubIntr(21, 8);
	sceKernelEnableSubIntr(21, 9);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

static s32 __attribute__((noinline)) dmacplusSc128Term(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	REG32(0xBC8001D0) = 0;
	sceKernelReleaseSubIntrHandler(21, 8);
	sceKernelReleaseSubIntrHandler(21, 9);
	sceKernelDeleteSema(g_sc128SemaId);
	sceKernelDeleteEventFlag(g_sc128EvFlagId);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

static s32 __attribute__((noinline)) dmacplusSc128CheckBusy(void)
{
	return 0;
}

static s32 __attribute__((noinline)) dmacplusSc128Resume(void)
{
	return 0;
}

/* --- Kermit DMAC RPC helpers (0x56A, 0x56B, 0x56C, 0x569) --- */
static s32 __attribute__((noinline)) kermitDmacplusSendCmd(u32 y_step, u32 cb_step, u32 cr_step, u32 aux_step)
{
	u8 req_buf[128];
	SceKermitReqPacket *req_cached = DMAC_ALIGN64(req_buf);
	sceKernelDcacheInvalidateRange(req_cached, 64);
	SceKermitReqPacket *req = DMAC_UNCACHED_PTR(req_cached);
	u64 resp;
	req->cmd = 0x56A;
	req->args[0].lo = y_step;
	req->args[1].lo = cb_step;
	req->args[2].lo = cr_step;
	req->args[3].lo = aux_step;
	s32 res = sceKermit_driver_36666181((SceKermitRequest *)req, KERMIT_MODE_LOWIO, 0x56A, 4, 0, &resp);
	return (res != 0) ? res : (s32)resp;
}

static void __attribute__((noinline)) kermitDmacplusPullAvcOut(
	u32 plane_y_top, u32 plane_y_bot, u32 plane_cb_top, u32 plane_cb_bot,
	u32 plane_cr_top, u32 plane_cr_bot, u32 plane_aux_top, u32 plane_aux_bot,
	u32 mode_word, u32 dst_primary, u32 dst_secondary, u32 dst_ctrl)
{
	(void)plane_y_top;
	(void)plane_y_bot;
	(void)plane_cb_top;
	(void)plane_cb_bot;
	(void)plane_cr_top;
	(void)plane_cr_bot;
	(void)plane_aux_top;
	(void)plane_aux_bot;
	(void)dst_secondary;
	if (g_kermitAvcSkipPull != 0)
		return;
	u32 mb_h = (mode_word >> 16) & 0x3FU;
	u32 mb_w = (mode_word >> 8) & 0x3FU;
	u32 stride = (dst_ctrl >> 8) & 0x7FFU;
	u32 bpp = (((dst_ctrl >> 1) & 3U) != 0) ? 2U : 4U;
	s32 total_bytes = (s32)(16U * stride * mb_h * bpp);
	if (g_kermitAvcFrameLimit != 0) {
		if (g_kermitAvcFrameLimit < g_kermitAvcFrameCount)
			return;
		g_kermitAvcFrameCount++;
	}
	if ((dst_primary & 0x1FFFFFFFU) == 0 || total_bytes == 0)
		return;
	s32 per_line = 0;
	if (g_kermitAvcForceLinePull != 0) {
		if (16U * mb_w != 480U)
			per_line = 1;
	} else if (total_bytes >= (s32)((mb_w * mb_h * bpp) << 9)) {
		per_line = 1;
	}
	if (per_line != 0) {
		u32 line_bytes = 16U * mb_w * bpp;
		u32 row_step = stride * bpp;
		u32 rows = 16U * mb_h;
		u32 addr = dst_primary;
		while (rows != 0) {
			rows--;
			sceKermitMemory_driver_90B662D0((void *)addr, line_bytes);
			addr += row_step;
		}
		return;
	}
	sceKermitMemory_driver_90B662D0((void *)dst_primary, (u32)total_bytes);
}

static s32 __attribute__((noinline)) kermitDmacplusSubmitAvc(
	u32 plane_y_top, u32 plane_y_bot, u32 plane_cb_top, u32 plane_cb_bot,
	u32 plane_cr_top, u32 plane_cr_bot, u32 plane_aux_top, u32 plane_aux_bot,
	u32 mode_word, u32 dst_primary, u32 dst_secondary, u32 dst_ctrl, u32 start_ctrl)
{
	u8 req_buf[192];
	SceKermitReqPacket *req_cached = DMAC_ALIGN64(req_buf);
	sceKernelDcacheInvalidateRange(req_cached, 128);
	SceKermitReqPacket *req = DMAC_UNCACHED_PTR(req_cached);
	u64 resp;
	if (g_kermitAvcSkipPull != 0)
		g_kermitAvcSavedDst = dst_primary;
	u32 src_paddr = plane_y_top & 0x1FFFFFFFU;
	if (src_paddr - 0x04000000U <= 0x007FFFFFU) {
		u32 mb_w = (mode_word >> 8) & 0x3FU;
		u32 mb_h = (mode_word >> 16) & 0x3FU;
		sceKermitMemory_driver_80E1240A((const void *)src_paddr, 384U * mb_w * mb_h);
	}
	req->cmd = 0x56B;
	req->args[0].lo = plane_y_top;
	req->args[1].lo = plane_y_bot;
	req->args[2].lo = plane_cb_top;
	req->args[3].lo = plane_cb_bot;
	req->args[4].lo = plane_cr_top;
	req->args[5].lo = plane_cr_bot;
	req->args[6].lo = plane_aux_top;
	req->args[7].lo = plane_aux_bot;
	req->args[8].lo = mode_word;
	req->args[9].lo = dst_primary;
	req->args[10].lo = dst_secondary;
	req->args[11].lo = dst_ctrl;
	req->args[12].lo = start_ctrl;
	s32 res = sceKermit_driver_36666181((SceKermitRequest *)req, KERMIT_MODE_LOWIO, 0x56B, 13, 0, &resp);
	return (res != 0) ? res : (s32)resp;
}

static void __attribute__((noinline)) kermitDmacplusFinishSc2Me(u32 src, u32 dst, u32 next, u32 attr)
{
	SceDmacplusLli head;
	head.src = src;
	head.dst = dst;
	head.next = next;
	head.attr = attr;
	SceDmacplusLli *cur = &head;
	if (((u32)cur & 0x03FFFFFFU) == 0)
		return;
	s32 in_vram = ((src - 0x04000000U) <= 0x007FFFFFU) ? 1 : 0;
	u32 next_addr;
	do {
		if (in_vram != 0)
			sceKermitMemory_driver_90B662D0((void *)(cur->dst & 0x03FFFFFFU), cur->attr & 0xFFFU);
		next_addr = cur->next;
		cur = (SceDmacplusLli *)((next_addr & 0x1FFFFFFFU) | 0xA0000000U);
	} while ((next_addr & 0x03FFFFFFU) != 0);
}

static s32 __attribute__((noinline)) kermitDmacplusSubmitSc2Me(u32 src, u32 dst, u32 next, u32 attr, u32 ctrl)
{
	u8 req_buf[128];
	SceKermitReqPacket *req_cached = DMAC_ALIGN64(req_buf);
	sceKernelDcacheInvalidateRange(req_cached, 64);
	SceKermitReqPacket *req = DMAC_UNCACHED_PTR(req_cached);
	u64 resp;
	SceDmacplusLli head;
	head.src = src;
	head.dst = dst;
	head.next = next;
	head.attr = attr;
	SceDmacplusLli *cur = &head;
	if (((u32)cur & 0x03FFFFFFU) != 0) {
		u32 next_addr;
		do {
			sceKermitMemory_driver_80E1240A((const void *)cur->src, cur->attr & 0xFFFU);
			next_addr = cur->next;
			cur = (SceDmacplusLli *)((next_addr & 0x1FFFFFFFU) | 0xA0000000U);
		} while ((next_addr & 0x03FFFFFFU) != 0);
	}
	req->cmd = 0x56C;
	req->args[0].lo = src;
	req->args[1].lo = dst;
	req->args[2].lo = next;
	req->args[3].lo = attr;
	req->args[4].lo = ctrl;
	s32 res = sceKermit_driver_36666181((SceKermitRequest *)req, KERMIT_MODE_LOWIO, 0x56C, 5, 0, &resp);
	if (res == 0) {
		res = (s32)resp;
		if (g_kermitAvcFrameLimit != 0)
			g_kermitAvcFrameCount = 0;
	}
	return res;
}

static void __attribute__((noinline)) kermitDmacplusPushMe2ScSrc(u32 src, u32 dst, u32 next, u32 attr)
{
	SceDmacplusLli head;
	head.src = src;
	head.dst = dst;
	head.next = next;
	head.attr = attr;
	SceDmacplusLli *cur = &head;
	if (((u32)cur & 0x03FFFFFFU) == 0)
		return;
	u32 next_addr;
	do {
		u32 src_paddr = cur->src & 0x03FFFFFFU;
		u32 cur_attr = cur->attr;
		if (src_paddr - 0x04000000U <= 0x007FFFFFU) {
			u32 shift = (cur_attr >> 18) & 7U;
			u32 len = (cur_attr & 0xFFFU) << shift;
			sceKermitMemory_driver_80E1240A((const void *)src_paddr, len);
		}
		next_addr = cur->next;
		cur = (SceDmacplusLli *)((next_addr & 0x1FFFFFFFU) | 0xA0000000U);
	} while ((next_addr & 0x03FFFFFFU) != 0);
}

static s32 __attribute__((noinline)) kermitDmacplusSubmitMe2Sc(u32 src, u32 dst, u32 next, u32 attr, u32 ctrl)
{
	u8 req_buf[128];
	SceKermitReqPacket *req_cached = DMAC_ALIGN64(req_buf);
	sceKernelDcacheInvalidateRange(req_cached, 64);
	SceKermitReqPacket *req = DMAC_UNCACHED_PTR(req_cached);
	u64 resp;
	kermitDmacplusPushMe2ScSrc(src, dst, next, attr);
	req->cmd = 0x569;
	req->args[0].lo = src;
	req->args[1].lo = dst;
	req->args[2].lo = next;
	req->args[3].lo = attr;
	req->args[4].lo = ctrl;
	s32 res = sceKermit_driver_36666181((SceKermitRequest *)req, KERMIT_MODE_LOWIO, 0x569, 5, 0, &resp);
	return (res != 0) ? res : (s32)resp;
}

/* Push PSP memory into shared Kermit mirror before host read */
int sceKermitMemory_driver_80E1240A(const void *addr, u32 size)
{
	u32 uaddr = (u32)addr;
	u32 paddr = uaddr & 0x1FFFFFFFU;
	u32 base_off;
	if (paddr - 0x04000000U <= 0x007FFFFFU) {
		base_off = (uaddr & 0x039FFFFFU) + 0x01E00000U;
	} else if (paddr - 0x00010000U < 0x4000U) {
		base_off = (uaddr & 0x03FFFFFFU) + 0x01D00000U;
	} else {
		return 0;
	}
	u32 kaddr = (0xAA000000U + base_off) & 0x9FFFFFFFU;
	const u8 *src = (const u8 *)addr;
	u8 *dst = (u8 *)kaddr;
	if (size < 0x3000U) {
		sceKernelMemcpy(dst, src, size);
		sceKernelDcacheWritebackInvalidateRange(dst, size);
		return 0;
	}
	u32 misalign = kaddr & 0x3FU;
	if (misalign != 0) {
		u32 head = 0x40U - misalign;
		sceKernelMemcpy(dst, src, head);
		sceKernelDcacheWritebackInvalidateRange(dst, head);
		dst += head;
		src += head;
		size -= head;
	}
	u32 body = size - (size & 0x3FU);
	sceKernelDcacheWritebackRange(src, body);
	sceDmacMemcpy(dst, src, body);
	if ((size & 0x3FU) != 0) {
		sceKernelMemcpy(dst + body, src + body, size & 0x3FU);
		sceKernelDcacheWritebackInvalidateRange(dst + body, size & 0x3FU);
	}
	return 0;
}

/* Pull shared Kermit mirror back into PSP memory after host write */
int sceKermitMemory_driver_90B662D0(void *addr, u32 size)
{
	u32 uaddr = (u32)addr;
	u32 paddr = uaddr & 0x1FFFFFFFU;
	u32 base_off;
	if (paddr - 0x04000000U <= 0x007FFFFFU) {
		base_off = (uaddr & 0x039FFFFFU) + 0x01E00000U;
	} else if (paddr - 0x00010000U < 0x4000U) {
		base_off = (uaddr & 0x03FFFFFFU) + 0x01D00000U;
	} else {
		return 0;
	}
	u32 kaddr = (0xAA000000U + base_off) & 0x9FFFFFFFU;
	sceKernelDcacheInvalidateRange((const void *)kaddr, size);
	const u8 *src = (const u8 *)kaddr;
	u8 *dst = (u8 *)addr;
	if (size < 0x3000U) {
		sceKernelMemcpy(dst, src, size);
		sceKernelDcacheWritebackRange(dst, size);
		return 0;
	}
	u32 misalign = kaddr & 0x3FU;
	if (misalign != 0) {
		u32 head = 0x40U - misalign;
		sceKernelMemcpy(dst, src, head);
		sceKernelDcacheWritebackRange(dst, head);
		src += head;
		dst += head;
		size -= head;
	}
	u32 body = size - (size & 0x3FU);
	sceDmacMemcpy(dst, src, body);
	sceKernelDcacheInvalidateRange(dst, body);
	if ((size & 0x3FU) != 0) {
		sceKernelMemcpy(dst + body, src + body, size & 0x3FU);
		sceKernelDcacheWritebackRange(dst + body, size & 0x3FU);
	}
	return 0;
}

static s32 __attribute__((noinline)) kermitDmacplusInitSharedMem(void)
{
	sceKernelMemset((void *)0xABD38000, 0, 0x8000);
	g_kermitDmacSmBaseLo = 0xABD38000U;
	g_kermitDmacSmBaseHi = 0xABD3C000U;
	g_kermitDmacSmLimit = 0x4000U;
	SceUID mtx = sceKernelCreateMutex("SceKermitDmacplus", 0x100, 0, NULL);
	g_kermitDmacMutexId = mtx;
	return (mtx < 1) ? 1 : 0;
}

static void __attribute__((noinline)) kermitDmacplusPullMe2ScDst(u32 src, u32 dst, u32 next, u32 attr)
{
	SceDmacplusLli head;
	head.src = src;
	head.dst = dst;
	head.next = next;
	head.attr = attr;
	SceDmacplusLli *cur = &head;
	if (((u32)cur & 0x03FFFFFFU) == 0)
		return;
	u32 next_addr;
	do {
		u32 cur_attr = cur->attr;
		u32 shift = (cur_attr >> 18) & 7U;
		u32 len = (cur_attr & 0xFFFU) << shift;
		sceKermitMemory_driver_90B662D0((void *)cur->dst, len);
		next_addr = cur->next;
		cur = (SceDmacplusLli *)((next_addr & 0x1FFFFFFFU) | 0xA0000000U);
	} while ((next_addr & 0x03FFFFFFU) != 0);
}

/* Set Kermit request argument and optionally push buffer */
void sceKermitMemory_driver_AAF047AC(SceKermitRequest *req, int slot_idx, void *addr, u32 size, int copy_flag)
{
	SceKermitReqPacket *pkt = (SceKermitReqPacket *)req;
	if ((copy_flag & 1) != 0)
		sceKermitMemory_driver_80E1240A(addr, size);
	if (slot_idx <= 13)
		pkt->args[slot_idx].lo = (u32)addr;
}
