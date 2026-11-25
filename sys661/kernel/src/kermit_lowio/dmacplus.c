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

u32 sceDmacplus_driver_var_AA2E7E54 = 0x1048094B;
u32 sceDmacplus_driver_var_C10B947A = 0x10480949;
u32 sceDmacplus_driver_var_374689B0 = 0x10480900;
u32 sceDmacplus_driver_var_9D3DDDF0 = 0x10482492;

static u16 g_dmacSubIntrMask;
static u16 g_dmacActiveFlags;

static u32 g_lcdcDmacFrameBuf;
static u32 g_lcdcDmacPixelFormat;
static u32 g_lcdcDmacDisplayWidth;
static u32 g_lcdcDmacFrameWidth;
static u32 g_lcdcDmacCtrl;
static void (*g_lcdcDmacCb)(s32, void *);
static void *g_lcdcDmacCbArg;

static SceUID g_avcEvFlagId;
static void (*g_avcDmacCb)(s32, void *);
static void *g_avcDmacCbArg;
static u32 g_avcDmacBusy;
static void *g_avcDmacSavedArg;

static SceUID g_sc2meEvFlagId;
static void (*g_sc2meDmacCb)(s32, s32, void *, void *);
static void *g_sc2meDmacCbArg;
static void *g_sc2meDmacCurLli;
static u32 g_sc2meDmacBusy;

static SceUID g_me2scEvFlagId;
static void (*g_me2scDmacCb)(s32, s32, void *, void *);
static void *g_me2scDmacCbArg;
static void *g_me2scDmacCurLli;
static u32 g_me2scDmacBusy;

static SceUID g_sc128SemaId;
static SceUID g_sc128EvFlagId;

static u32 g_kermitDmacSmBase1;
static u32 g_kermitDmacSmBase2;
static u32 g_kermitDmacSmLimit;
static SceUID g_kermitDmacMutexId;

/* Forward declarations for internal DMAC functions */
static s32 dmacplusSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static s32 dmacplusSubIntrAck(s32 sub_intr);
static s32 dmacplusSubIntrClear(s32 sub_intr);
static s32 dmacplusCheckBusy(void);
static s32 dmacplusLcdcSuspend(void);
static s32 dmacplusLcdcResume(void);
static s32 dmacplusLcdcTerm(void);
static s32 dmacplusLcdcResetBusy(void);
static s32 dmacplusLcdcIntrCb(s32 sub_intr, void *arg, s32 status);
static s32 dmacplusAvcInit(void);
static s32 dmacplusAvcTerm(void);
static s32 dmacplusAvcSuspend(void);
static s32 dmacplusAvcResume(void);
static s32 dmacplusAvcComplete(void);
static s32 dmacplusSc2MeInit(void);
static s32 dmacplusSc2MeTerm(void);
static s32 dmacplusSc2MeSuspend(void);
static s32 dmacplusSc2MeResume(void);
static s32 dmacplusSc2MeComplete(void);
static s32 dmacplusMe2ScInit(void);
static s32 dmacplusMe2ScTerm(void);
static s32 dmacplusMe2ScSuspend(void);
static s32 dmacplusMe2ScResume(void);
static s32 dmacplusMe2ScComplete(void);
static s32 dmacplusSc128TryLock(void);
static s32 dmacplusSc128Init(void);
static s32 dmacplusSc128Term(void);
static s32 dmacplusSc128DoneIntr(s32 sub_intr, void *arg, s32 status);
static s32 dmacplusSc128ErrIntr(s32 sub_intr, void *arg, s32 status);
static s32 dmacplusSc128Resume(void);
static s32 kermitDmacplusSendCmd(u32 a0, u32 a1, u32 a2, u32 a3);
static s32 kermitDmacplusInvalidateAvcOut(const u32 *p);
static s32 kermitDmacplusSubmitAvc(const u32 *p);
static s32 kermitDmacplusFinishSc2Me(SceDmacplusLli *lli);
static s32 kermitDmacplusSubmitSc2Me(SceDmacplusLli *lli);
static s32 kermitDmacplusFlushLli(SceDmacplusLli *lli);
static s32 kermitDmacplusSubmitMe2Sc(SceDmacplusLli *lli);
static s32 kermitDmacplusInitSharedMem(void);
static s32 kermitDmacplusInvalidateMe2Sc(SceDmacplusLli *lli);

static SceSysEventHandler g_dmacplusEvent = {
	sizeof(SceSysEventHandler),
	"SceDmacplus",
	0x00FFFF00,
	dmacplusSysEventHandler,
	0, 0, NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static s32 dmacplusSubIntrClearCb(s32 intr_code, s32 sub_intr)
{
	(void)intr_code;
	dmacplusSubIntrClear(sub_intr);
	return 0;
}

static s32 dmacplusSubIntrDisableCb(s32 intr_code, s32 sub_intr)
{
	(void)intr_code;
	g_dmacSubIntrMask &= ~(1U << sub_intr);
	return 0;
}

static s32 dmacplusSubIntrEnableCb(s32 intr_code, s32 sub_intr)
{
	(void)intr_code;
	g_dmacSubIntrMask |= (1U << sub_intr);
	return 0;
}

static s32 dmacplusSubIntrSuspendCb(s32 intr_code, s32 sub_intr, u32 *prev)
{
	(void)intr_code;
	if (prev != NULL)
		*prev = (g_dmacSubIntrMask >> sub_intr) & 1;
	g_dmacSubIntrMask &= ~(1U << sub_intr);
	return 0;
}

static s32 dmacplusSubIntrResumeCb(s32 intr_code, s32 sub_intr, u32 prev)
{
	(void)intr_code;
	g_dmacSubIntrMask |= ((prev & 1) << sub_intr);
	return 0;
}

__attribute__((noipa)) static s32 dmacplusSubIntrAck(s32 sub_intr)
{
	if (sub_intr == 0)
		return 0;
	if ((u32)sub_intr >= 10)
		return 0x80000102;
	s32 shift = sub_intr / 2;
	if ((sub_intr & 1) == 0)
		return (REG32(0xBC800014) >> shift) & 1;
	return (REG32(0xBC800018) >> shift) & 1;
}

__attribute__((noipa)) static s32 dmacplusSubIntrClear(s32 sub_intr)
{
	if (sub_intr == 0)
		return 0;
	if ((u32)sub_intr >= 10)
		return 0x80000102;
	s32 intr = sceKernelCpuSuspendIntr();
	s32 shift = sub_intr / 2;
	u32 v;
	if ((sub_intr & 1) == 0) {
		v = (REG32(0xBC800004) >> shift) & 1;
		REG32(0xBC800008) = v << shift;
	} else {
		v = (REG32(0xBC80000C) >> shift) & 1;
		REG32(0xBC800010) = v << shift;
	}
	sceKernelCpuResumeIntrWithSync(intr);
	return (s32)v;
}

static s32 dmacplusIntrHandler(s32 intr_code, void *arg, void *frame)
{
	(void)arg;
	u32 mask = g_dmacSubIntrMask;
	s32 ch = 0;
	s32 sub_intr;
	for (sub_intr = 0; sub_intr < 10; sub_intr += 2, ch++) {
		u32 ch_bit = 1U << ch;
		if (sub_intr != 0) {
			if ((REG32(0xBC800004) & ch_bit) != 0) {
				REG32(0xBC800008) = ch_bit;
				if ((mask & (1U << sub_intr)) != 0)
					sceKernelCallSubIntrHandler(intr_code, sub_intr, (void *)sub_intr, frame);
			}
		}
		s32 odd_sub = sub_intr + 1;
		if ((REG32(0xBC80000C) & ch_bit) != 0) {
			REG32(0xBC800010) = ch_bit;
			if ((mask & (1U << odd_sub)) != 0)
				sceKernelCallSubIntrHandler(intr_code, odd_sub, (void *)odd_sub, frame);
		}
	}
	return -1;
}

static u32 g_dmacSubIntrCb[7] = {
	0, (u32)dmacplusSubIntrClearCb, (u32)dmacplusSubIntrEnableCb, (u32)dmacplusSubIntrDisableCb,
	(u32)dmacplusSubIntrSuspendCb, (u32)dmacplusSubIntrResumeCb, (u32)dmacplusSubIntrAck
};
static u32 g_dmacIntrOpt[3] = { 0x0C, 0x0A, (u32)g_dmacSubIntrCb };

static s32 dmacplusSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	(void)ev_name;
	(void)param;
	(void)result;
	if (ev_id == 0x402) {
		return dmacplusCheckBusy();
	} else if (ev_id == 0x4004) {
		dmacplusLcdcResetBusy();
		dmacplusAvcSuspend();
		dmacplusSc2MeSuspend();
		dmacplusMe2ScSuspend();
		return dmacplusSc128TryLock();
	} else if (ev_id == 0x10004) {
		sysregDmacplusBusClockEnable();
		dmacplusLcdcResume();
		dmacplusAvcResume();
		dmacplusSc2MeResume();
		dmacplusMe2ScResume();
		dmacplusSc128Resume();
	}
	return 0;
}

static s32 dmacplusCheckBusy(void)
{
	if ((g_dmacActiveFlags & 0x1E) != 0)
		return 0x80000021;
	return 0;
}

s32 dmacplusModuleStart(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	sysregDmacplusBusClockEnable();
	sceKernelRegisterIntrHandler(21, 2, dmacplusIntrHandler, NULL, g_dmacIntrOpt);
	dmacplusLcdcSuspend();
	dmacplusAvcInit();
	dmacplusSc2MeInit();
	dmacplusMe2ScInit();
	dmacplusSc128Init();
	sceKernelEnableIntr(21);
	sceKernelUnregisterSysEventHandler(&g_dmacplusEvent);
	sceKernelRegisterSysEventHandler(&g_dmacplusEvent);
	kermitDmacplusInitSharedMem();
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
	sysregDmacplusBusClockDisable();
	return 0;
}

/* --- LCDC DMAC (Channel 0) --- */
static s32 dmacplusLcdcSuspend(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_lcdcDmacFrameBuf = REG32(0xBC800100);
	g_lcdcDmacPixelFormat = REG32(0xBC800104);
	g_lcdcDmacDisplayWidth = REG32(0xBC800108);
	g_lcdcDmacFrameWidth = REG32(0xBC80010C);
	u32 ctrl = REG32(0xBC800110);
	g_lcdcDmacCtrl = ctrl;
	if ((ctrl & 1) != 0)
		g_dmacActiveFlags |= 1;
	else
		g_dmacActiveFlags &= ~1U;
	sceKernelCpuResumeIntrWithSync(intr);
	sceKernelRegisterSubIntrHandler(21, 1, dmacplusLcdcIntrCb, NULL);
	sceKernelEnableSubIntr(21, 1);
	return 0;
}

static s32 dmacplusLcdcResume(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	REG32(0xBC800100) = g_lcdcDmacFrameBuf;
	REG32(0xBC800104) = g_lcdcDmacPixelFormat;
	REG32(0xBC800108) = g_lcdcDmacDisplayWidth;
	REG32(0xBC80010C) = g_lcdcDmacFrameWidth;
	u32 ctrl = g_lcdcDmacCtrl;
	REG32(0xBC800110) = ctrl;
	if ((ctrl & 1) != 0)
		g_dmacActiveFlags |= 1;
	else
		g_dmacActiveFlags &= ~1U;
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

s32 sceDmacplus_driver_A3AA8D00(void *cb, void *arg)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_lcdcDmacCb = (void (*)(s32, void *))cb;
	g_lcdcDmacCbArg = arg;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceDmacplus_driver_88ACB6F1(u32 pformat, u32 disp_width, u32 frame_width)
{
	if (pformat >= 4)
		return 0x80000108;
	if ((disp_width - 1) >= 2048 || (frame_width - 1) >= 2048)
		return 0x80000104;
	s32 intr = sceKernelCpuSuspendIntr();
	g_lcdcDmacPixelFormat = pformat;
	g_lcdcDmacDisplayWidth = disp_width;
	g_lcdcDmacFrameWidth = frame_width;
	REG32(0xBC800104) = pformat;
	REG32(0xBC800108) = disp_width;
	REG32(0xBC80010C) = frame_width;
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

s32 sceDmacplus_driver_33CD36DD(u32 *pformat, u32 *disp_width, u32 *frame_width)
{
	if (pformat != NULL) *pformat = g_lcdcDmacPixelFormat;
	if (disp_width != NULL) *disp_width = g_lcdcDmacDisplayWidth;
	if (frame_width != NULL) *frame_width = g_lcdcDmacFrameWidth;
	return 0;
}

s32 sceDmacplus_driver_ED849260(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	if ((REG32(0xBC800110) & 1) == 0) {
		u32 ctrl = g_lcdcDmacCtrl | 1;
		g_lcdcDmacCtrl = ctrl;
		REG32(0xBC800110) = ctrl;
	}
	g_dmacActiveFlags |= 1;
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

static s32 dmacplusLcdcTerm(void)
{
	sceKernelReleaseSubIntrHandler(21, 0);
	sceKernelReleaseSubIntrHandler(21, 1);
	return 0;
}

static s32 dmacplusLcdcResetBusy(void)
{
	g_dmacActiveFlags &= ~1U;
	return 0;
}

s32 sceDmacplus_driver_CCCF6069(u32 *fb)
{
	if (fb != NULL)
		*fb = g_lcdcDmacFrameBuf;
	return 0;
}

s32 sceDmacplus_driver_E9B746F9(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	if ((REG32(0xBC800110) & 1) != 0) {
		u32 ctrl = g_lcdcDmacCtrl & ~1U;
		g_lcdcDmacCtrl = ctrl;
		REG32(0xBC800110) = ctrl;
	}
	g_dmacActiveFlags &= ~1U;
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

static s32 dmacplusLcdcIntrCb(s32 sub_intr, void *arg, s32 status)
{
	(void)sub_intr;
	(void)arg;
	if (g_lcdcDmacCb != NULL)
		g_lcdcDmacCb(0, g_lcdcDmacCbArg);
	return status;
}

s32 sceDmacplus_driver_D126494B(u32 fb)
{
	if (((fb >> 29) & 1) != 0 || (fb & 0xFF800000) == 0)
		return 0x80000103;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 paddr = fb & 0x1FFFFFFF;
	g_lcdcDmacFrameBuf = paddr;
	REG32(0xBC800100) = paddr;
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

/* --- AVC DMAC (Channel 1) --- */
s32 sceDmacplus_driver_9C492B9B(void *cb, void *arg)
{
	s32 intr = sceKernelCpuSuspendIntr();
	if ((g_dmacActiveFlags & 2) != 0) {
		sceKernelCpuResumeIntr(intr);
		return 0x80000021;
	}
	g_avcDmacCb = (void (*)(s32, void *))cb;
	g_avcDmacCbArg = arg;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceDmacplus_driver_BF0DB45E(u32 mode)
{
	if (mode >= 2)
		return 0x80000107;
	u32 out_bits = 0;
	s32 res;
	if (mode != 0)
		res = sceKernelWaitEventFlag(g_avcEvFlagId, 3, 0x21, &out_bits, NULL);
	else
		res = sceKernelPollEventFlag(g_avcEvFlagId, 3, 0x21, &out_bits);
	if (res == (s32)0x800201A8)
		return 0x80000021;
	if (res < 0)
		return res;
	kermitDmacplusInvalidateAvcOut((const u32 *)g_avcDmacSavedArg);
	if ((out_bits & 2) != 0)
		return 0x80000024;
	return 0;
}

static s32 dmacplusAvcInit(void)
{
	g_avcDmacBusy = 0;
	g_dmacActiveFlags &= ~2U;
	g_avcDmacCb = NULL;
	g_avcDmacCbArg = NULL;
	SceUID ev = sceKernelCreateEventFlag("SceDmacplusAvc", 0x201, 0, NULL);
	g_avcEvFlagId = ev;
	if (ev < 0)
		return ev;
	s32 r = sceKermit_driver_5280B410(4, dmacplusAvcComplete);
	if (r != 0)
		Kprintf("error: %s lines %d, result = 0x%08X\n", "dmacplus.c", 1110, r);
	return 0;
}

static s32 dmacplusAvcTerm(void)
{
	sceKernelReleaseSubIntrHandler(21, 2);
	sceKernelReleaseSubIntrHandler(21, 3);
	if (g_avcEvFlagId > 0) {
		sceKernelDeleteEventFlag(g_avcEvFlagId);
		g_avcEvFlagId = 0;
	}
	return 0;
}

static s32 dmacplusAvcSuspend(void)
{
	return 0;
}

static s32 dmacplusAvcResume(void)
{
	g_avcDmacBusy = 0;
	g_dmacActiveFlags &= ~2U;
	return 0;
}

s32 sceDmacplus_driver_B269EAC9(void *arg)
{
	s32 intr = sceKernelCpuSuspendIntr();
	if (g_avcDmacBusy != 0) {
		sceKernelCpuResumeIntr(intr);
		return 0x80000021;
	}
	sceKernelClearEventFlag(g_avcEvFlagId, 0);
	g_avcDmacBusy = 1;
	g_dmacActiveFlags |= 2;
	g_avcDmacSavedArg = arg;
	sceKernelCpuResumeIntrWithSync(intr);
	kermitDmacplusSubmitAvc((const u32 *)arg);
	return 0;
}

s32 sceDmacplus_driver_F652F09D(u32 a0, u32 a1, u32 a2, u32 a3)
{
	return kermitDmacplusSendCmd(a0 << 4, a1 << 4, a2 << 4, a3 << 4);
}

static s32 dmacplusAvcComplete(void)
{
	g_avcDmacBusy = 0;
	g_dmacActiveFlags &= ~2U;
	if (g_avcDmacCb != NULL) {
		kermitDmacplusInvalidateAvcOut((const u32 *)g_avcDmacSavedArg);
		g_avcDmacCb(0, g_avcDmacCbArg);
		return 0;
	}
	sceKernelSetEventFlag(g_avcEvFlagId, 1);
	return 0;
}

/* --- SC2ME DMAC (Channel 2) --- */
s32 sceDmacplus_driver_282CA0D7(void *cb, void *arg)
{
	s32 intr = sceKernelCpuSuspendIntr();
	if ((g_dmacActiveFlags & 4) != 0) {
		sceKernelCpuResumeIntr(intr);
		return 0x80000021;
	}
	g_sc2meDmacCb = (void (*)(s32, s32, void *, void *))cb;
	g_sc2meDmacCbArg = arg;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceDmacplus_driver_47D400CB(u32 mode, void **out_lli)
{
	if (mode >= 2)
		return 0x80000107;
	u32 out_bits = 0;
	s32 res;
	if (mode != 0)
		res = sceKernelWaitEventFlag(g_sc2meEvFlagId, 3, 0x21, &out_bits, NULL);
	else
		res = sceKernelPollEventFlag(g_sc2meEvFlagId, 3, 0x21, &out_bits);
	if (res == (s32)0x800201A8)
		return 0x80000021;
	if (res < 0)
		return res;
	if ((out_bits & 2) != 0) {
		if (out_lli != NULL)
			*out_lli = g_sc2meDmacCurLli;
		return 0x80000024;
	}
	kermitDmacplusFinishSc2Me((SceDmacplusLli *)g_sc2meDmacCurLli);
	return 0;
}

static s32 dmacplusSc2MeInit(void)
{
	g_sc2meDmacBusy = 0;
	g_dmacActiveFlags &= ~4U;
	g_sc2meDmacCb = NULL;
	g_sc2meDmacCbArg = NULL;
	g_sc2meDmacCurLli = NULL;
	SceUID ev = sceKernelCreateEventFlag("SceDmacplusSc2Me", 0x201, 0, NULL);
	g_sc2meEvFlagId = ev;
	if (ev < 0)
		return ev;
	sceKermit_driver_5280B410(5, dmacplusSc2MeComplete);
	return 0;
}

static s32 dmacplusSc2MeTerm(void)
{
	sceKernelReleaseSubIntrHandler(21, 4);
	sceKernelReleaseSubIntrHandler(21, 5);
	if (g_sc2meEvFlagId > 0) {
		sceKernelDeleteEventFlag(g_sc2meEvFlagId);
		g_sc2meEvFlagId = 0;
	}
	return 0;
}

static s32 dmacplusSc2MeSuspend(void)
{
	return 0;
}

static s32 dmacplusSc2MeResume(void)
{
	g_sc2meDmacBusy = 0;
	g_dmacActiveFlags &= ~4U;
	return 0;
}

s32 sceDmacplus_driver_9EBF7D3E(SceDmacplusLli *lli)
{
	s32 intr = sceKernelCpuSuspendIntr();
	if (g_sc2meDmacBusy != 0) {
		sceKernelCpuResumeIntr(intr);
		return 0x80000021;
	}
	sceKernelClearEventFlag(g_sc2meEvFlagId, 0);
	g_sc2meDmacCurLli = lli;
	g_sc2meDmacBusy = 1;
	g_dmacActiveFlags |= 4;
	sceKernelCpuResumeIntrWithSync(intr);
	kermitDmacplusSubmitSc2Me(lli);
	return 0;
}

static s32 dmacplusSc2MeComplete(void)
{
	g_sc2meDmacBusy = 0;
	g_dmacActiveFlags &= ~4U;
	if (g_sc2meDmacCb != NULL) {
		kermitDmacplusFinishSc2Me((SceDmacplusLli *)g_sc2meDmacCurLli);
		g_sc2meDmacCb(0, 0, NULL, g_sc2meDmacCbArg);
		return 0;
	}
	sceKernelSetEventFlag(g_sc2meEvFlagId, 1);
	return 0;
}

/* --- ME2SC DMAC (Channel 3) --- */
s32 sceDmacplus_driver_5FCF43BD(u32 mode, void **out_lli)
{
	if (mode >= 2)
		return 0x80000107;
	u32 out_bits = 0;
	s32 res;
	if (mode != 0)
		res = sceKernelWaitEventFlag(g_me2scEvFlagId, 3, 0x21, &out_bits, NULL);
	else
		res = sceKernelPollEventFlag(g_me2scEvFlagId, 3, 0x21, &out_bits);
	if (res == (s32)0x800201A8)
		return 0x80000021;
	if (res < 0)
		return res;
	if ((out_bits & 2) != 0) {
		if (out_lli != NULL)
			*out_lli = g_me2scDmacCurLli;
		return 0x80000024;
	}
	kermitDmacplusInvalidateMe2Sc((SceDmacplusLli *)g_me2scDmacCurLli);
	return 0;
}

static s32 dmacplusMe2ScInit(void)
{
	g_me2scDmacBusy = 0;
	g_dmacActiveFlags &= ~8U;
	g_me2scDmacCb = NULL;
	g_me2scDmacCbArg = NULL;
	g_me2scDmacCurLli = NULL;
	SceUID ev = sceKernelCreateEventFlag("SceDmacplusMe2Sc", 0x201, 0, NULL);
	g_me2scEvFlagId = ev;
	if (ev < 0)
		return ev;
	sceKermit_driver_5280B410(6, dmacplusMe2ScComplete);
	return 0;
}

static s32 dmacplusMe2ScTerm(void)
{
	sceKernelReleaseSubIntrHandler(21, 6);
	sceKernelReleaseSubIntrHandler(21, 7);
	if (g_me2scEvFlagId > 0) {
		sceKernelDeleteEventFlag(g_me2scEvFlagId);
		g_me2scEvFlagId = 0;
	}
	return 0;
}

static s32 dmacplusMe2ScSuspend(void)
{
	return 0;
}

static s32 dmacplusMe2ScResume(void)
{
	g_me2scDmacBusy = 0;
	g_dmacActiveFlags &= ~8U;
	return 0;
}

s32 sceDmacplus_driver_DBD8526F(void *cb, void *arg)
{
	s32 intr = sceKernelCpuSuspendIntr();
	if ((g_dmacActiveFlags & 8) != 0) {
		sceKernelCpuResumeIntr(intr);
		return 0x80000021;
	}
	g_me2scDmacCb = (void (*)(s32, s32, void *, void *))cb;
	g_me2scDmacCbArg = arg;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceDmacplus_driver_6945F1D3(SceDmacplusLli *lli)
{
	s32 intr = sceKernelCpuSuspendIntr();
	if (g_me2scDmacBusy != 0) {
		sceKernelCpuResumeIntr(intr);
		return 0x80000021;
	}
	sceKernelClearEventFlag(g_me2scEvFlagId, 0);
	g_me2scDmacCurLli = lli;
	g_me2scDmacBusy = 1;
	g_dmacActiveFlags |= 8;
	sceKernelCpuResumeIntrWithSync(intr);
	kermitDmacplusSubmitMe2Sc(lli);
	return 0;
}

static s32 dmacplusMe2ScComplete(void)
{
	g_me2scDmacBusy = 0;
	g_dmacActiveFlags &= ~8U;
	if (g_me2scDmacCb != NULL) {
		kermitDmacplusInvalidateMe2Sc((SceDmacplusLli *)g_me2scDmacCurLli);
		g_me2scDmacCb(0, 0, NULL, g_me2scDmacCbArg);
		return 0;
	}
	sceKernelSetEventFlag(g_me2scEvFlagId, 1);
	return 0;
}

/* --- SC128 DMAC (Channel 4 / sceDmacMemcpy) --- */
static s32 dmacplusSc128TryLock(void)
{
	s32 res = sceKernelPollSema(g_sc128SemaId, 1);
	if (res == (s32)0x800201A5)
		return 0x80000021;
	return (res < 0) ? res : 0;
}

static s32 dmacplusSc128Init(void)
{
	g_dmacActiveFlags &= ~0x10U;
	SceUID sema = sceKernelCreateSema("SceDmacplusSc128", 1, 1, 1, NULL);
	g_sc128SemaId = sema;
	if (sema < 0)
		return sema;
	SceUID ev = sceKernelCreateEventFlag("SceDmacplusSc128", 0x201, 0, NULL);
	g_sc128EvFlagId = ev;
	if (ev < 0) {
		sceKernelDeleteSema(g_sc128SemaId);
		g_sc128SemaId = 0;
		return ev;
	}
	sceKernelRegisterSubIntrHandler(21, 8, dmacplusSc128DoneIntr, NULL);
	sceKernelRegisterSubIntrHandler(21, 9, dmacplusSc128ErrIntr, NULL);
	sceKernelEnableSubIntr(21, 8);
	sceKernelEnableSubIntr(21, 9);
	return 0;
}

static s32 dmacplusSc128Term(void)
{
	sceKernelReleaseSubIntrHandler(21, 8);
	sceKernelReleaseSubIntrHandler(21, 9);
	if (g_sc128SemaId > 0) {
		sceKernelDeleteSema(g_sc128SemaId);
		g_sc128SemaId = 0;
	}
	if (g_sc128EvFlagId > 0) {
		sceKernelDeleteEventFlag(g_sc128EvFlagId);
		g_sc128EvFlagId = 0;
	}
	return 0;
}

static s32 dmacplusSc128DoneIntr(s32 sub_intr, void *arg, s32 status)
{
	(void)sub_intr;
	(void)arg;
	g_dmacActiveFlags &= ~0x10U;
	if ((status & 1) != 0)
		sceKernelSetEventFlag(g_sc128EvFlagId, 1);
	if ((status & 2) != 0)
		sceKernelSetEventFlag(g_sc128EvFlagId, 2);
	return 0;
}

static s32 dmacplusSc128ErrIntr(s32 sub_intr, void *arg, s32 status)
{
	(void)sub_intr;
	(void)arg;
	(void)status;
	g_dmacActiveFlags &= ~0x10U;
	sceKernelSetEventFlag(g_sc128EvFlagId, 2);
	return 0;
}

static s32 dmacplusSc128Copy(void *dst, const void *src, u32 size, s32 is_try)
{
	if (size == 0)
		return 0x80000104;
	u32 udst = (u32)dst;
	u32 usrc = (u32)src;
	if (((udst ^ usrc) & 0x1FFFFFFF) < size && ((usrc ^ udst) & 0x1FFFFFFF) < size) {
		/* check overlap */
	}
	u32 kmode;
	__asm__ volatile("mfc0 %0, $24" : "=r"(kmode));
	s32 intr = sceKernelCpuSuspendIntr();
	sceKernelCpuResumeIntr(intr);
	s32 res;
	if (is_try != 0) {
		res = sceKernelPollSema(g_sc128SemaId, 1);
		if (res == (s32)0x800201A5)
			return 0x80000021;
	} else {
		res = sceKernelWaitSema(g_sc128SemaId, 1, NULL);
	}
	if (res < 0)
		return res;
	if ((kmode & 0xFF) == 0 && size <= 0x1A0) {
		memcpy(dst, src, size);
		sceKernelSignalSema(g_sc128SemaId, 1);
		return 0;
	}
	SceDmacplusLli lli_chain[16];
	u32 pdst = udst & 0x1FFFFFFF;
	u32 psrc = usrc & 0x1FFFFFFF;
	if ((pdst >= 0x08000000 && pdst < 0x0C000000) || (psrc >= 0x08000000 && psrc < 0x0C000000))
		(void)REG32(0xA7F00000);
	sceDdrFlush(4);
	intr = sceKernelCpuSuspendIntr();
	if ((REG32(0xBC8001D0) & 1) != 0) {
		sceKernelCpuResumeIntr(intr);
		sceKernelSignalSema(g_sc128SemaId, 1);
		return 0x80000021;
	}
	sceKernelClearEventFlag(g_sc128EvFlagId, 0);
	u32 shift = __builtin_ctz((udst | usrc | size) | 0x10);
	if (shift > 4) shift = 4;
	u32 attr_base = sceDmacplus_driver_var_374689B0 | (shift << 18) | (shift << 21);
	u32 max_blk = 0xFFFCU << shift;
	u32 rem = size;
	s32 idx = 0;
	while (rem > max_blk && idx < 15) {
		lli_chain[idx].src = psrc;
		lli_chain[idx].dst = pdst;
		lli_chain[idx].next = ((u32)&lli_chain[idx + 1]) & 0x1FFFFFFF;
		lli_chain[idx].attr = attr_base | 0xFFF;
		psrc += max_blk;
		pdst += max_blk;
		rem -= max_blk;
		idx++;
	}
	lli_chain[idx].src = psrc;
	lli_chain[idx].dst = pdst;
	lli_chain[idx].next = 0;
	lli_chain[idx].attr = attr_base | (rem >> shift) | 0x80000000U;
	sceKernelDcacheWritebackRange(lli_chain, (idx + 1) * sizeof(SceDmacplusLli));
	REG32(0xBC8001C0) = lli_chain[0].src;
	REG32(0xBC8001C4) = lli_chain[0].dst;
	REG32(0xBC8001C8) = lli_chain[0].next;
	REG32(0xBC8001CC) = lli_chain[0].attr;
	g_dmacActiveFlags |= 0x10;
	REG32(0xBC8001D0) = 1;
	sceKernelCpuResumeIntrWithSync(intr);
	u32 out_bits = 0;
	res = sceKernelWaitEventFlag(g_sc128EvFlagId, 3, 0x21, &out_bits, NULL);
	sceKernelSignalSema(g_sc128SemaId, 1);
	if (res < 0)
		return res;
	return ((out_bits & 2) != 0) ? 0x80000024 : 0;
}

s32 sceDmacplus_driver_2C5FC1E3(void *dst, const void *src, u32 size)
{
	return dmacplusSc128Copy(dst, src, size, 0);
}

s32 sceDmacMemcpy(void *dst, const void *src, u32 size)
{
	return dmacplusSc128Copy(dst, src, size, 0);
}

s32 sceDmacplus_driver_D57D5BC2(void *dst, const void *src, u32 size)
{
	return dmacplusSc128Copy(dst, src, size, 1);
}

s32 sceDmacTryMemcpy(void *dst, const void *src, u32 size)
{
	return dmacplusSc128Copy(dst, src, size, 1);
}

s32 sceDmacplus_driver_B0EC610D(SceDmacplusLli *lli)
{
	if (lli == NULL)
		return 0x80000103;
	s32 res = sceKernelWaitSema(g_sc128SemaId, 1, NULL);
	if (res < 0)
		return res;
	(void)REG32(0xA7F00000);
	sceDdrFlush(4);
	s32 intr = sceKernelCpuSuspendIntr();
	if ((REG32(0xBC8001D0) & 1) != 0) {
		sceKernelCpuResumeIntr(intr);
		sceKernelSignalSema(g_sc128SemaId, 1);
		return 0x80000021;
	}
	sceKernelClearEventFlag(g_sc128EvFlagId, 0);
	REG32(0xBC8001C0) = lli->src & 0x1FFFFFFF;
	REG32(0xBC8001C4) = lli->dst & 0x1FFFFFFF;
	REG32(0xBC8001C8) = lli->next & 0x1FFFFFFF;
	REG32(0xBC8001CC) = lli->attr;
	g_dmacActiveFlags |= 0x10;
	REG32(0xBC8001D0) = 1;
	sceKernelCpuResumeIntrWithSync(intr);
	u32 out_bits = 0;
	res = sceKernelWaitEventFlag(g_sc128EvFlagId, 3, 0x21, &out_bits, NULL);
	sceKernelSignalSema(g_sc128SemaId, 1);
	if (res < 0)
		return res;
	return ((out_bits & 2) != 0) ? 0x80000024 : 0;
}

static s32 dmacplusSc128Resume(void)
{
	g_dmacActiveFlags &= ~0x10U;
	return 0;
}

/* --- Kermit DMAC RPC helpers (0x56A, 0x56B, 0x56C, 0x569) --- */
static s32 kermitDmacplusSendCmd(u32 a0, u32 a1, u32 a2, u32 a3)
{
	u8 buf[176];
	SceKermitRequest *req_cached = (SceKermitRequest *)(((u32)buf + 63) & ~63U);
	sceKernelDcacheInvalidateRange(req_cached, 64);
	SceKermitRequest *req = (SceKermitRequest *)KERMIT_UNCACHED(req_cached);
	u64 resp = 0;
	req->args[0] = a0;
	req->args[1] = a1;
	req->args[2] = a2;
	req->args[3] = a3;
	s32 res = sceKermit_driver_36666181(req, KERMIT_MODE_LOWIO, 0x56A, 4, 0, &resp);
	return (res != 0) ? res : (s32)resp;
}

static s32 kermitDmacplusInvalidateAvcOut(const u32 *p)
{
	if (p == NULL)
		return 0;
	u32 size0 = ((p[1] & 0xFFF) + ((p[1] >> 16) & 0xFFF)) << 4;
	if (size0 != 0)
		sceKermitMemory_driver_90B662D0((void *)p[3], size0);
	u32 size1 = (((p[4] & 0xFFF) << 2) + ((p[4] >> 16) & 0xFFF)) << 2;
	if (size1 != 0) {
		sceKermitMemory_driver_90B662D0((void *)p[5], size1);
		sceKermitMemory_driver_90B662D0((void *)p[6], size1);
		sceKermitMemory_driver_90B662D0((void *)p[7], size1);
		sceKermitMemory_driver_90B662D0((void *)p[8], size1);
		sceKermitMemory_driver_90B662D0((void *)p[9], size1);
		sceKermitMemory_driver_90B662D0((void *)p[10], size1);
		sceKermitMemory_driver_90B662D0((void *)p[11], size1);
		sceKermitMemory_driver_90B662D0((void *)p[12], size1);
	}
	return 0;
}

static s32 kermitDmacplusSubmitAvc(const u32 *p)
{
	u8 buf[176];
	SceKermitRequest *req_cached = (SceKermitRequest *)(((u32)buf + 63) & ~63U);
	sceKernelDcacheInvalidateRange(req_cached, 64);
	SceKermitRequest *req = (SceKermitRequest *)KERMIT_UNCACHED(req_cached);
	u64 resp = 0;
	u32 in_size = ((p[1] & 0xFFF) + ((p[1] >> 16) & 0xFFF)) << 4;
	if (in_size != 0)
		sceKermitMemory_driver_80E1240A((const void *)p[2], in_size);
	s32 i;
	for (i = 0; i < 13; i++)
		req->args[i] = p[i];
	s32 res = sceKermit_driver_36666181(req, KERMIT_MODE_LOWIO, 0x56B, 13, 1, &resp);
	return (res != 0) ? res : (s32)resp;
}

static s32 kermitDmacplusFinishSc2Me(SceDmacplusLli *lli)
{
	(void)lli;
	return 0;
}

static s32 kermitDmacplusSubmitSc2Me(SceDmacplusLli *lli)
{
	u8 buf[176];
	SceKermitRequest *req_cached = (SceKermitRequest *)(((u32)buf + 63) & ~63U);
	sceKernelDcacheInvalidateRange(req_cached, 64);
	SceKermitRequest *req = (SceKermitRequest *)KERMIT_UNCACHED(req_cached);
	u64 resp = 0;
	SceDmacplusLli *cur = lli;
	while (cur != NULL) {
		u32 shift = (cur->attr >> 18) & 7;
		u32 len = (cur->attr & 0xFFF) << shift;
		sceKermitMemory_driver_80E1240A((const void *)cur->src, len);
		sceKernelDcacheWritebackRange(cur, sizeof(SceDmacplusLli));
		sceKermitMemory_driver_80E1240A(cur, sizeof(SceDmacplusLli));
		cur = (SceDmacplusLli *)cur->next;
	}
	req->args[0] = lli->src;
	req->args[1] = lli->dst;
	req->args[2] = lli->next;
	req->args[3] = lli->attr;
	req->args[4] = (u32)lli;
	s32 res = sceKermit_driver_36666181(req, KERMIT_MODE_LOWIO, 0x56C, 5, 1, &resp);
	return (res != 0) ? res : (s32)resp;
}

static s32 kermitDmacplusFlushLli(SceDmacplusLli *lli)
{
	SceDmacplusLli *cur = lli;
	while (cur != NULL) {
		sceKernelDcacheWritebackRange(cur, sizeof(SceDmacplusLli));
		sceKermitMemory_driver_80E1240A(cur, sizeof(SceDmacplusLli));
		cur = (SceDmacplusLli *)cur->next;
	}
	return 0;
}

static s32 kermitDmacplusSubmitMe2Sc(SceDmacplusLli *lli)
{
	u8 buf[176];
	SceKermitRequest *req_cached = (SceKermitRequest *)(((u32)buf + 63) & ~63U);
	sceKernelDcacheInvalidateRange(req_cached, 64);
	SceKermitRequest *req = (SceKermitRequest *)KERMIT_UNCACHED(req_cached);
	u64 resp = 0;
	kermitDmacplusFlushLli(lli);
	req->args[0] = lli->src;
	req->args[1] = lli->dst;
	req->args[2] = lli->next;
	req->args[3] = lli->attr;
	req->args[4] = (u32)lli;
	s32 res = sceKermit_driver_36666181(req, KERMIT_MODE_LOWIO, 0x569, 5, 1, &resp);
	return (res != 0) ? res : (s32)resp;
}

/* Push PSP memory into shared Kermit mirror before host read */
int sceKermitMemory_driver_80E1240A(const void *addr, u32 size)
{
	u32 uaddr = (u32)addr;
	u32 paddr = uaddr & 0x1FFFFFFF;
	u32 base_off;
	if (paddr >= 0x04000000 && paddr <= 0x0BFFFFFF) {
		u32 masked = uaddr & ~((3U << 21) | (0x3FU << 26));
		base_off = 0x01E00000 + masked;
	} else if ((paddr - 0x00010000) < 0x4000) {
		base_off = 0x01D00000 + (uaddr & 0x03FFFFFF);
	} else {
		return 0;
	}
	u32 kaddr = (0xAA000000 + base_off) & ~(3U << 29);
	const u8 *src = (const u8 *)addr;
	u8 *dst = (u8 *)kaddr;
	if (size < 0x3000) {
		sceKernelMemcpy(dst, src, size);
		sceKernelDcacheWritebackInvalidateRange(dst, size);
		return 0;
	}
	u32 misalign = kaddr & 0x3F;
	if (misalign != 0) {
		u32 head = 0x40 - misalign;
		sceKernelMemcpy(dst, src, head);
		sceKernelDcacheWritebackInvalidateRange(dst, head);
		dst += head;
		src += head;
		size -= head;
	}
	u32 body = size & ~0x3FU;
	u32 tail = size - body;
	sceKernelDcacheWritebackRange(src, body);
	sceDmacplus_driver_2C5FC1E3(dst, src, body);
	if (tail != 0) {
		sceKernelMemcpy(dst + body, src + body, tail);
		sceKernelDcacheWritebackInvalidateRange(dst + body, tail);
	}
	return 0;
}

/* Pull shared Kermit mirror back into PSP memory after host write */
int sceKermitMemory_driver_90B662D0(void *addr, u32 size)
{
	u32 uaddr = (u32)addr;
	u32 paddr = uaddr & 0x1FFFFFFF;
	u32 base_off;
	if (paddr >= 0x04000000 && paddr <= 0x0BFFFFFF) {
		u32 masked = uaddr & ~((3U << 21) | (0x3FU << 26));
		base_off = 0x01E00000 + masked;
	} else if ((paddr - 0x00010000) < 0x4000) {
		base_off = 0x01D00000 + (uaddr & 0x03FFFFFF);
	} else {
		return 0;
	}
	u32 kaddr = (0xAA000000 + base_off) & ~(3U << 29);
	sceKernelDcacheInvalidateRange((const void *)kaddr, size);
	const u8 *src = (const u8 *)kaddr;
	u8 *dst = (u8 *)addr;
	if (size < 0x3000) {
		sceKernelMemcpy(dst, src, size);
		sceKernelDcacheWritebackRange(dst, size);
		return 0;
	}
	u32 misalign = kaddr & 0x3F;
	if (misalign != 0) {
		u32 head = 0x40 - misalign;
		sceKernelMemcpy(dst, src, head);
		sceKernelDcacheWritebackRange(dst, head);
		src += head;
		dst += head;
		size -= head;
	}
	u32 body = size & ~0x3FU;
	u32 tail = size - body;
	sceDmacplus_driver_2C5FC1E3(dst, src, body);
	sceKernelDcacheInvalidateRange(dst, body);
	if (tail != 0) {
		sceKernelMemcpy(dst + body, src + body, tail);
		sceKernelDcacheWritebackRange(dst + body, tail);
	}
	return 0;
}

static s32 kermitDmacplusInitSharedMem(void)
{
	sceKernelMemset((void *)0xABD38000, 0, 0x8000);
	g_kermitDmacSmBase1 = 0xABD38000;
	g_kermitDmacSmBase2 = 0xABD3C000;
	g_kermitDmacSmLimit = 0x4000;
	SceUID mtx = sceKernelCreateMutex("SceKermitDmacplus", 0x100, 0, NULL);
	g_kermitDmacMutexId = mtx;
	return (mtx < 0) ? mtx : 0;
}

static s32 kermitDmacplusInvalidateMe2Sc(SceDmacplusLli *lli)
{
	SceDmacplusLli *cur = lli;
	while (cur != NULL) {
		u32 shift = (cur->attr >> 18) & 7;
		u32 len = (cur->attr & 0xFFF) << shift;
		sceKermitMemory_driver_90B662D0((void *)cur->dst, len);
		cur = (SceDmacplusLli *)cur->next;
	}
	return 0;
}

/* Set Kermit request argument and optionally push buffer */
void sceKermitMemory_driver_AAF047AC(SceKermitRequest *req, int arg_idx, void *addr, u32 size, int copy_flag)
{
	if ((copy_flag & 1) != 0)
		sceKermitMemory_driver_80E1240A(addr, size);
	if (arg_idx < 14)
		*(u32 *)((u8 *)req + 0x10 + (arg_idx << 3)) = (u32)addr;
}

