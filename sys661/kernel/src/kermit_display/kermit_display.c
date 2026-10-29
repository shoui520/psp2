/*
 * ePSP 6.61 sceDisplay_Service (kermit_display.prx)
 *
 * Reimplemented from the 6.61 kermit_display.prx binary, which manages
 * ePSP framebuffer scanout via DMACPLUS/LCDC, VBLANK sub-interrupts,
 * Hcount/Vcount tracking, boot logo fade-out via GE, and Kermit sync.
 */

#include <kermit.h>

/* Error codes */
#define SCE_ERROR_NOT_SUPPORTED                 0x80000004
#define SCE_ERROR_PRIV_REQUIRED                 0x80000023
#define SCE_ERROR_INVALID_ID                    0x80000102
#define SCE_ERROR_INVALID_POINTER               0x80000103
#define SCE_ERROR_INVALID_SIZE                  0x80000104
#define SCE_ERROR_INVALID_MODE                  0x80000107
#define SCE_ERROR_INVALID_FORMAT                0x80000108
#define SCE_ERROR_INVALID_VALUE                 0x800001FE
#define SCE_ERROR_OUT_OF_RANGE                  0x800001FF

#define SCE_KERNEL_ERROR_ERROR                  0x80020001
#define SCE_KERNEL_ERROR_ILLEGAL_CONTEXT        0x80020064

/* Hardware timer registers */
#define APB_TIMER1_BASE                         0xBE740000u
#define APB_TIMER2_BASE                         0xBE780000u

/* Additional kernel & driver imports */
typedef struct SceGameInfo {
	u8 pad[0x44];
	char disc_id[16];
} SceGameInfo;

typedef struct SceSm1Operations {
	u32 unk[4];
	int (*checkMagic)(void);
	u32 unk14[4];
	void (*notifyMode)(int mode);
} SceSm1Operations;

int sceKernelApplicationType(void);
int sceKernelInitApitype(void);
int sceKernelSetInitCallback(int (*cb)(void *, int, void *), int flag, int *res);
int sceKernelCallSubIntrHandler(int intr_code, int sub_intr_code, void *arg1, void *arg2);
void sceKernelCpuResumeIntrWithSync(int state);
int sceKernelSetUserModeIntrHanlerAcceptable(int intr_code, int sub_intr_code, int acceptable);
SceSm1Operations *sceKernelSm1ReferOperations(void);
int sceKernelGetModel(void);
int sceKernelMemset32(void *dst, int val, SceSize size);
SceGameInfo *sceKernelGetGameInfo(void);
void *memset(void *s, int c, SceSize n);
void *memcpy(void *dst, const void *src, SceSize n);
int strcmp(const char *s1, const char *s2);
int sceKernelAllocateKTLS(SceSize size, int (*cb)(unsigned int *size, void *arg), void *arg);
int sceKernelFreeKTLS(int id);
void *sceKernelGetKTLS(int id);
void sceKernelDcacheWritebackAll(void);
int sceDmacplus_driver_33CD36DD(void **topaddr, int *bufferwidth, int *pixelformat);
int sceDmacplus_driver_88ACB6F1(int width, int bufferwidth, int pixelformat);
int sceDmacplus_driver_A3AA8D00(void *topaddr);
void *sceDmacplus_driver_CCCF6069(void);
int sceDmacplus_driver_E9B746F9(void);
int sceDmacplus_driver_ED849260(void);
int sceGeDrawSync(int syncType);
int sceGeEdramSetAddrTranslation(int width);
int sceGeListSync(int qid, int syncType);
unsigned int sceGeEdramGetHwSize(void);
void *sceGeEdramGetAddr(void);
int sceGeListEnQueue(const void *list, void *stall, int cbid, void *arg);
int sceGeEdramSetSize(int size);
int sceIdStorageLookup(u16 key, u32 offset, void *buf, u32 len);
int sceKermitPeripheral_driver_D0A69002(void);
int sceLcdc_driver_2D80BB2E(int *hsync, int *vsync, int *hblank, int *vblank, int *hperiod);
float sceLcdc_driver_9D4FB8B1(void);
int sceLcdc_driver_BAF165CE(int mode, int width, int height, int unk);
int sceLcdc_driver_E9DBD35F(int mode, int width, int height, int pixelformat);
int sceSysreg_driver_4841B2D2(int timer);
int sceSysreg_driver_4E5C86AA(void);
int sceSysregApbTimerClkEnable(int timer);
int sceSysregApbTimerClkSelect(int timer, int clk);
int sceSysregApbTimerClkDisable(int timer);

static inline u32 pspShiftK1(void)
{
	u32 old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);
	return old_k1;
}

/* Structures */
typedef struct SceDisplayFrameBufEntry {
	int state;          /* 0x00 */
	void *nextAddr;     /* 0x04 */
	void *topAddr;      /* 0x08 */
	int bufferWidth;    /* 0x0C */
	int pixelFormat;    /* 0x10 */
} SceDisplayFrameBufEntry;

typedef struct SceDisplayContext {
	SceDisplayFrameBufEntry fb[4]; /* 0x000..0x04F */
	int hwWidth;                   /* 0x050 */
	int hwHeight;                  /* 0x054 */
	int hwBufferWidth;             /* 0x058 */
	int hwPixelFormat;             /* 0x05C */
	int fadeState;                 /* 0x060 */
	s8 activeFbIdx;                /* 0x064 */
	u8 forceImmediateAddr;         /* 0x065 */
	s8 displayEnabled;             /* 0x066 */
	s8 targetEnabled;              /* 0x067 */
	s8 curEnabled;                 /* 0x068 */
	s8 hasLcd;                     /* 0x069 */
	s8 unk6A;                      /* 0x06A */
	s8 backlightPending;           /* 0x06B */
	s8 hasCustomLevels;            /* 0x06C */
	u8 pad6D;                      /* 0x06D */
	u16 lcdConfig;                 /* 0x06E */
	u8 backlightSel;               /* 0x070 */
	u8 brightnessLevels[5];        /* 0x071..0x075 */
	u8 pad76[0x62];                /* 0x076..0x0D7 */
	int targetBrightness;          /* 0x0D8 */
	int currentBrightness;         /* 0x0DC */
	int curLcdcMode;               /* 0x0E0 */
	int nextLcdcMode;              /* 0x0E4 */
	int curMode;                   /* 0x0E8 */
	int nextMode;                  /* 0x0EC */
	int width;                     /* 0x0F0 */
	int height;                    /* 0x0F4 */
	int holdMode;                  /* 0x0F8 */
	int resumeMode;                /* 0x0FC */
	int suspendCountdown;          /* 0x100 */
	int disableCountdown;          /* 0x104 */
	int unk108;                    /* 0x108 */
	int brightnessHoldCount;       /* 0x10C */
	int modeChangeCountdown;       /* 0x110 */
	int modeChangeFlags;           /* 0x114 */
	int unk118;                    /* 0x118 */
	int initDone;                  /* 0x11C */
	int vshModeInfo;               /* 0x120 */
	SceSm1Operations *sm1Ops;      /* 0x124 */
} SceDisplayContext;

typedef struct SceDisplayVblankContext {
	u32 subIntrMask;               /* 0x00 */
	int vcount;                    /* 0x04 */
	int hcountOffset;              /* 0x08 */
	u32 baseTimerVal;              /* 0x0C */
	int accumHcount;               /* 0x10 */
	int hsync;                     /* 0x14 */
	int vsync;                     /* 0x18 */
	u32 hperiod;                   /* 0x1C */
	int vblank;                    /* 0x20 */
	int isPseudoVsyncEnabled;      /* 0x24 */
	int pseudoVsyncInterval;       /* 0x28 */
	int isPseudoField;             /* 0x2C */
	int hcountPerVblank;           /* 0x30 */
	int ktlsId;                    /* 0x34 */
	SceUID evFlagId;               /* 0x38 */
} SceDisplayVblankContext;

typedef struct SceDisplayFadeContext {
	int pixelFormat;               /* 0x00 */
	int bufferWidth;               /* 0x04 */
	int holdFrames;                /* 0x08 */
	int stepAlpha;                 /* 0x0C */
	int curAlpha;                  /* 0x10 */
	int bufIdx;                    /* 0x14 */
	void *fbAddr[2];               /* 0x18..0x1F */
	u32 *geListAddr;               /* 0x20 */
} SceDisplayFadeContext;

typedef struct SceDisplaySubIntrOps {
	void *unk0;
	void *unk4;
	void *unk8;
	int (*disable1)(int intr_code, int sub_intr_code);
	int (*enable)(int intr_code, int sub_intr_code);
	int (*disable2)(int intr_code, int sub_intr_code);
	int (*suspend)(int intr_code, int sub_intr_code, int *state);
	int (*resume)(int intr_code, int sub_intr_code, int state);
	void *unk20;
} SceDisplaySubIntrOps;

typedef struct SceDisplayIntrOpt {
	int size;
	int numSubIntrs;
	const SceDisplaySubIntrOps *ops;
} SceDisplayIntrOpt;

/* Forward declarations */
int sceDisplayInit(void);
int sceDisplayEnd(void);
int sceDisplayWaitVblankStart(void);
int sceDisplayIsVblank(void);
int sceDisplayIsVsync(void);
int sceDisplayGetCurrentHcount(void);
int sceDisplaySetFrameBuf(void *topaddr, int bufferwidth, int pixelformat, int sync);
int sceDisplayGetFrameBuf(void **topaddr, int *bufferwidth, int *pixelformat, int sync);
int sceDisplay_driver_3E17FE8D(u32 pri, void *topaddr, int bufferwidth, int pixelformat, int sync);
int sceDisplay_driver_F338AAE0(u32 pri, void **topaddr, int *bufferwidth, int *pixelformat, int sync);
int sceDisplay_driver_8475E040(void);

static int displaySysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static int displayInitCallback(void *arg1, int arg2, void *arg3);
static int displayVblankSubIntrHandler(int intr_code, void *arg);
static void displayUpdateEnableState(void);
static int displayApplyBrightness(int brightness);
static int displayConfigureHw(int lcdcMode, int width, int height, int bufferwidth, int pixelformat);
static int displayResetTimerAndIntr(int onlyEnableIntr);
static int displayMasterVblankIntrHandler(int intr_code);
static void displayInitApbTimer1(void);
static int displayWaitVblankInternal(int vblankCount, int isCB);
static int displayGetRawHcount(void);
static int displayInitVblank(void);
static int displayEndVblank(void);
static int displayDisableVblank(void);
static int displayUpdateTimingParams(int mode);
static int displaySubIntrEnable(int intr_code, int sub_intr_code);
static int displaySubIntrDisable(int intr_code, int sub_intr_code);
static int displaySubIntrSuspend(int intr_code, int sub_intr_code, int *state);
static int displaySubIntrResume(int intr_code, int sub_intr_code, int state);
static int displayPseudoVsyncIntrHandler(void);
static int displayWaitVblankImpl(int isCB);
static int displaySetupPseudoVsyncTimer(int interval);
static int displaySetupBootFade(int pixelFormat, int bufferWidth, int holdFrames, int stepAlpha);
static int displayStepBootFade(void);
static int displayEnqueueFadeGeList(void *srcAddr, void *dstAddr, int bufferWidth, int pixelFormat,
				    int width, int height, u32 color);
static int displayFinishBootFade(void);

/* Rodata tables */
static const SceDisplaySubIntrOps g_displaySubIntrOps = {
	NULL,
	NULL,
	NULL,
	displaySubIntrDisable,
	displaySubIntrEnable,
	displaySubIntrDisable,
	displaySubIntrSuspend,
	displaySubIntrResume,
	NULL
};

static const SceDisplayIntrOpt g_displayIntrOpt = {
	sizeof(SceDisplayIntrOpt),
	32,
	&g_displaySubIntrOps
};

static const u8 g_subIntrCallOrder[32] = {
	16, 17, 18, 19,  0,  1,  2,  3,
	20, 21, 22, 23,  4,  5,  6,  7,
	 8,  9, 10, 11, 24, 25, 26, 27,
	12, 13, 14, 15, 28, 29, 30, 31
};

static const u32 g_fadeStepGeCmds[24] = {
	0x9C000000u, 0x9D000000u, 0xD2000003u, 0xD4000000u,
	0xD5000000u, 0x15000000u, 0x16000000u, 0x4C000000u,
	0x4D000000u, 0xC2000000u, 0xA0000000u, 0xA8000000u,
	0xB8000909u, 0xC3000003u, 0xC7000101u, 0xC9000000u,
	0x1E000001u, 0x13000000u, 0x1280011Eu, 0x10000000u,
	0x01000000u, 0x04060010u, 0x0F000000u, 0x0C000000u
};

static const u32 g_fadeEndGeCmds[20] = {
	0x9C000000u, 0x9D000000u, 0xD2000000u, 0xD5000000u,
	0x16000000u, 0xC2000000u, 0xA0000000u, 0xA8000000u,
	0xB8000000u, 0xC3000000u, 0xC7000000u, 0xC9000000u,
	0x1E000000u, 0x13000000u, 0x12000000u, 0x10000000u,
	0x01000000u, 0x04000000u, 0x0F000000u, 0x0C000000u
};

/* Data & BSS globals */
static SceSysEventHandler g_displaySysEvent = {
	sizeof(SceSysEventHandler),
	"SceDisplay",
	0x00FFFF00,
	displaySysEventHandler,
	0,
	0,
	NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static SceDisplayContext g_display;             /* 0x3960 */
static SceDisplayVblankContext g_vblank;        /* 0x3A88 */
static SceDisplayFadeContext g_fade;            /* 0x3AC4 */

/* 0x0000 */
int module_reboot_before(SceSize args, void *argp)
{
	int clearAction = 0;
	int fbAction = 0;
	int intr = sceKernelCpuSuspendIntr();

	if (g_display.holdMode != 0) {
		clearAction = 1;
		fbAction = 1;
	}

	int curMode = g_display.curMode;
	if (curMode == 0 || curMode == 0x6D1 || curMode == 0x7D1 ||
	    curMode == 0x2D2 || curMode == 0x3D2 || curMode == 0x2E2) {
		void *hwFb = sceDmacplus_driver_CCCF6069();
		void *fb1 = g_display.fb[1].topAddr;
		if (fb1 == hwFb) {
			if ((((u32)fb1 >> 23) & 0x3Fu) == 8 &&
			    ((0x35 >> (((u32)fb1 >> 29) & 7)) & 1) != 0) {
				/* Keep current clearAction / fbAction */
			} else {
				clearAction = 0;
				fbAction = 0;
			}
		} else if (hwFb == NULL) {
			clearAction = 0;
			fbAction = 0;
		} else if (g_display.holdMode == 2) {
			clearAction = 0;
			if ((((u32)hwFb >> 23) & 0x3Fu) == 8) {
				if (((0x35 >> (((u32)hwFb >> 29) & 7)) & 1) == 0)
					fbAction = 2;
			} else {
				fbAction = 2;
			}
			if (sceKernelGetModel() == 0) {
				int m = g_display.curMode;
				if (m == 0x2D2 || m == 0x3D2 || m == 0x2E2)
					fbAction = 0;
			}
		} else {
			fbAction = 0;
		}
	} else {
		if (sceKernelApplicationType() == 0x100 &&
		    (u32)(g_display.curMode - 0x1D1) < 2u) {
			clearAction = 2;
			fbAction = 1;
		} else if (sceKernelApplicationType() == 0x300) {
			if (sceDmacplus_driver_CCCF6069() == NULL) {
				clearAction = 2;
				fbAction = 1;
			} else {
				u32 hwFb = (u32)sceDmacplus_driver_CCCF6069();
				if (((hwFb >> 23) & 0x3Fu) == 8) {
					if (((0x35 >> ((hwFb >> 29) & 7)) & 1) != 0) {
						clearAction = 2;
						fbAction = 1;
					} else {
						clearAction = 0;
						fbAction = 0;
					}
				} else {
					clearAction = 0;
					fbAction = 0;
				}
			}
		} else {
			clearAction = 0;
			fbAction = 0;
		}
	}

	if (fbAction == 0 || fbAction == 3) {
		int i;
		for (i = 0; i < 4; i++)
			sceDisplay_driver_3E17FE8D(i, NULL, 0, 3, 1);
		sceKernelCpuResumeIntr(intr);
		sceDisplayWaitVblankStart();
		intr = sceKernelCpuSuspendIntr();
		sceDmacplus_driver_A3AA8D00(NULL);
	}

	if (fbAction == 0) {
		int lcdcMode = g_display.nextLcdcMode;
		if (g_display.initDone != 0) {
			int m = g_display.curMode;
			if (m != 0 && m != 0x6D1 && m != 0x7D1 &&
			    m != 0x2D2 && m != 0x3D2 && m != 0x2E2)
				lcdcMode = 0;
		}
		if (sceKernelGetModel() == 0 && g_display.holdMode == 2)
			lcdcMode = 0;
		displayConfigureHw(lcdcMode, 480, 272, 0, 3);
		if (lcdcMode == 0 && g_display.sm1Ops != NULL)
			g_display.sm1Ops->notifyMode(3);
	}

	void *edram = sceGeEdramGetAddr();
	if (clearAction == 0) {
		int hwSize = sceGeEdramGetHwSize();
		int oldSize = sceGeEdramSetSize(hwSize);
		sceGeEdramSetAddrTranslation(0x400);
		sceKernelMemset32(edram, 0, oldSize);
		sceKernelDcacheWritebackAll();
		sceGeEdramSetSize(0x200000);
	} else if (clearAction == 1) {
		sceKernelMemset32((u8 *)edram + 0x1E0000, 0, 0x20000);
		sceKernelDcacheWritebackAll();
	}

	if (fbAction == 0 || fbAction == 3) {
		sceKernelDisableSubIntr(30, 18);
		sceDmacplus_driver_A3AA8D00(edram);
	} else if (fbAction == 2) {
		int bw = 0;
		int pf = 0;
		sceDmacplus_driver_33CD36DD(NULL, &bw, &pf);
		void *src = sceDmacplus_driver_CCCF6069();
		void *dst = sceGeEdramGetAddr();
		int dstStep, copyLen, srcStep;
		if (pf == 0) {
			dstStep = 0x800;
			copyLen = 0x780;
			srcStep = bw << 2;
		} else {
			dstStep = 0x400;
			copyLen = 0x3C0;
			srcStep = bw << 1;
		}
		int y;
		for (y = 271; y >= 0; y--) {
			memcpy(dst, src, copyLen);
			src = (u8 *)src + srcStep;
			dst = (u8 *)dst + dstStep;
		}
		sceKernelDcacheWritebackAll();
		void *base = sceGeEdramGetAddr();
		int fmt = SCE_ERROR_INVALID_FORMAT;
		if ((u32)pf < 4u)
			fmt = (0x93 >> (pf << 1)) & 3;
		sceDisplay_driver_3E17FE8D(0, base, 512, fmt, 1);
		sceKernelCpuResumeIntr(intr);
		sceDisplayWaitVblankStart();
		intr = sceKernelCpuSuspendIntr();
	}

	sceKernelCpuResumeIntr(intr);
	sceDisplayEnd();
	return 0;
}

/* 0x04EC */
int sceDisplayInit(void)
{
	u8 idbuf[512];
	int i;

	memset(&g_display, 0, sizeof(g_display));
	g_display.hasLcd = -1;
	if (sceKernelGetModel() == 0)
		g_display.hasLcd = 0;
	if (g_display.hasLcd < 0)
		return SCE_ERROR_NOT_SUPPORTED;

	if (sceIdStorageLookup(8, 0, idbuf, sizeof(idbuf)) < 0)
		memset(idbuf, 0, sizeof(idbuf));

	u32 magic = *(u32 *)idbuf;
	g_display.brightnessLevels[1] = 24;
	g_display.brightnessLevels[2] = 40;
	g_display.brightnessLevels[3] = 72;
	g_display.brightnessLevels[4] = 100;
	g_display.lcdConfig = 24;
	g_display.brightnessLevels[0] = 0;

	if (magic == 0x4C434470u) {
		u16 flags = *(u16 *)(idbuf + 0x10);
		u16 maxVal = *(u16 *)(idbuf + 0x12);
		u16 cfg = ((flags & 1) ? 0 : 8) | ((flags << 3) & 0x30);
		g_display.lcdConfig = cfg;
		if ((flags & 8) != 0) {
			g_display.hasCustomLevels = 1;
			g_display.brightnessLevels[1] = (u8)(((maxVal - *(u16 *)(idbuf + 0x14)) * 100) / maxVal);
			g_display.brightnessLevels[2] = (u8)(((maxVal - *(u16 *)(idbuf + 0x16)) * 100) / maxVal);
			g_display.brightnessLevels[3] = (u8)(((maxVal - *(u16 *)(idbuf + 0x18)) * 100) / maxVal);
			g_display.brightnessLevels[4] = (u8)(((maxVal - *(u16 *)(idbuf + 0x1A)) * 100) / maxVal);
		} else {
			g_display.brightnessLevels[1] = (u8)((*(u16 *)(idbuf + 0x14) * 100) / maxVal);
			g_display.brightnessLevels[2] = (u8)((*(u16 *)(idbuf + 0x16) * 100) / maxVal);
			g_display.brightnessLevels[3] = (u8)((*(u16 *)(idbuf + 0x18) * 100) / maxVal);
			g_display.brightnessLevels[4] = (u8)((*(u16 *)(idbuf + 0x1A) * 100) / maxVal);
		}
	}

	displayInitVblank();

	for (i = 0; i < 4; i++) {
		g_display.fb[i].state = 0;
		g_display.fb[i].nextAddr = NULL;
		g_display.fb[i].topAddr = NULL;
		g_display.fb[i].bufferWidth = 0;
		g_display.fb[i].pixelFormat = 3;
	}

	g_display.width = 480;
	g_display.height = 272;
	g_display.modeChangeCountdown = 0;
	g_display.curLcdcMode = 0;
	g_display.nextLcdcMode = 0;
	g_display.curMode = 0;
	g_display.nextMode = 0;

	int isEnabled = 0;
	if (g_display.hasLcd == 0)
		isEnabled = sceKermitPeripheral_driver_D0A69002();
	if (isEnabled == 1) {
		g_display.curEnabled = 1;
		g_display.displayEnabled = 1;
		g_display.targetEnabled = 1;
	}

	g_display.backlightPending = 0;
	SceSm1Operations *ops = sceKernelSm1ReferOperations();
	g_display.sm1Ops = ops;
	if (ops != NULL) {
		if (ops->checkMagic() != 0x534D)
			g_display.sm1Ops = NULL;
	}

	if (g_display.sm1Ops != NULL && sceKernelApplicationType() == 0x200)
		g_display.vshModeInfo = 0x0200000A;

	void *topaddr = NULL;
	int bw = 0;
	int pf = 0;
	sceDmacplus_driver_33CD36DD(&topaddr, &bw, &pf);
	if (bw != 0) {
		void *hwFb = sceDmacplus_driver_CCCF6069();
		int fmt = SCE_ERROR_INVALID_FORMAT;
		if ((u32)pf < 4u)
			fmt = (0x93 >> (pf << 1)) & 3;
		g_display.fb[2].pixelFormat = fmt;
		g_display.fb[2].topAddr = hwFb;
		g_display.fb[2].nextAddr = hwFb;
		g_display.fb[2].bufferWidth = bw;
		pf = fmt;
	}

	g_display.activeFbIdx = -2;
	g_display.hwPixelFormat = -1;
	g_display.hwWidth = -1;
	g_display.hwHeight = -1;
	g_display.hwBufferWidth = -1;

	if (bw == 512) {
		int apitype = sceKernelInitApitype();
		u32 idx = (u32)(apitype - 0x120);
		if (idx < 0x42u) {
			/* Boot fade-out for disc/game apitypes */
			static const u8 fadeMask[66] = {
				1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				1, 1
			};
			if (fadeMask[idx]) {
				g_display.fadeState = 1;
				displaySetupBootFade(pf, bw, 240, 4);
			}
		}
	}

	displayUpdateTimingParams(g_display.nextMode);
	sceKernelRegisterSubIntrHandler(30, 18, displayVblankSubIntrHandler, NULL);
	sceKernelEnableSubIntr(30, 18);
	sceKernelRegisterSysEventHandler(&g_displaySysEvent);

	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo != NULL && strcmp(gameInfo->disc_id, "ULJM05007") == 0)
		g_display.forceImmediateAddr = 1;

	if (g_display.fb[2].topAddr == NULL) {
		if (g_display.displayEnabled == 0) {
			sceDmacplus_driver_A3AA8D00(NULL);
			displayConfigureHw(0, 480, 272, 512, 3);
			sceKermit_driver_D69C50BB();
			if (g_display.sm1Ops != NULL)
				g_display.sm1Ops->notifyMode(3);
			g_display.displayEnabled = 1;
			g_display.targetEnabled = 1;
		} else {
			sceDisplayWaitVblankStart();
			sceDmacplus_driver_A3AA8D00(NULL);
		}
		sceDisplayWaitVblankStart();
	}

	sceKernelSetInitCallback(displayInitCallback, 2, NULL);
	return 0;
}

/* 0x09CC */
int sceDisplayEnd(void)
{
	sceKernelUnregisterSysEventHandler(&g_displaySysEvent);
	sceKernelReleaseSubIntrHandler(30, 18);
	displayEndVblank();
	return 0;
}

/* 0x0A04 */
static int displayInitCallback(void *arg1, int arg2, void *arg3)
{
	int fadeState = g_display.fadeState;
	g_display.initDone = 1;
	if (fadeState != 0) {
		while (g_display.fadeState == 1)
			sceDisplayWaitVblankStart();
		displayFinishBootFade();
	}

	int apitype = sceKernelInitApitype();
	if ((u32)(apitype - 0x110) >= 10u) {
		sceDisplay_driver_3E17FE8D(2, NULL, 0, 3, 1);
		sceDisplayWaitVblankStart();
		sceGeEdramSetAddrTranslation(0x400);
		int hwSize = sceGeEdramGetHwSize();
		int oldSize = sceGeEdramSetSize(hwSize);
		void *edram = sceGeEdramGetAddr();
		sceKernelMemset32(edram, 0, oldSize);
		sceKernelDcacheWritebackAll();
		sceGeEdramSetSize(0x200000);
	}
	return 0;
}

/* 0x0AF8 */
static int displaySysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	switch (ev_id) {
	case 0x400: {
		u32 *p = *(u32 **)((u8 *)param + 0x14);
		g_display.suspendCountdown = 10;
		p[0x50 / 4] = g_display.currentBrightness;
		break;
	}
	case 0x402:
		if (g_display.curEnabled != g_display.targetEnabled ||
		    g_display.suspendCountdown > 0 ||
		    g_display.modeChangeFlags != 0 ||
		    g_display.unk118 != 0)
			return -1;
		break;
	case 0x400A: {
		u32 *p = *(u32 **)((u8 *)param + 0x14);
		p[0x5C / 4] = g_display.nextMode;
		sceDisplayGetFrameBuf((void **)&p[0x38 / 4], (int *)&p[0x3C / 4], (int *)&p[0x40 / 4], 1);
		if (((u32 *)param)[1] != 2)
			displayDisableVblank();
		break;
	}
	case 0x1000A:
		if (g_display.resumeMode != 0) {
			g_display.fb[2].state = 1;
			g_display.fb[2].nextAddr = NULL;
			g_display.fb[2].topAddr = NULL;
			if (g_display.activeFbIdx == 2)
				g_display.activeFbIdx = -1;
		}
		if (g_display.hasLcd == 0)
			g_display.curEnabled = -1;
		displayResetTimerAndIntr(0);
		displayUpdateTimingParams(g_display.nextMode);
		break;
	case 0x1000F:
		while (sceDisplayIsVblank() == 1)
			;
		while (sceDisplayIsVblank() == 0)
			;
		if (g_display.hasLcd == 0) {
			g_display.disableCountdown = 0;
			if (g_display.targetEnabled != 0)
				sceDmacplus_driver_ED849260();
			else
				sceDmacplus_driver_E9B746F9();
			g_display.curEnabled = g_display.targetEnabled;
		}
		g_display.currentBrightness = 0;
		break;
	case 0x40000:
		displayResetTimerAndIntr(1);
		break;
	default:
		break;
	}
	return 0;
}

/* 0x0D04 */
static int displayVblankSubIntrHandler(int intr_code, void *arg)
{
	int i;

	if (g_display.fadeState == 1) {
		if (displayStepBootFade() != 0)
			g_display.fadeState = 2;
	}

	int intr = sceKernelCpuSuspendIntr();
	if (g_display.curLcdcMode != g_display.nextLcdcMode) {
		int cnt = g_display.modeChangeCountdown;
		if (cnt < 2) {
			if (cnt == 1) {
				g_display.modeChangeCountdown = 0;
				displayConfigureHw(g_display.nextLcdcMode, g_display.width, g_display.height,
						   g_display.hwBufferWidth, g_display.hwPixelFormat);
				displayUpdateTimingParams(g_display.nextMode);
				if (g_display.sm1Ops != NULL) {
					int m = g_display.nextLcdcMode;
					g_display.sm1Ops->notifyMode((m == 0 || m == 0xB) ? 3 : 0);
				}
			} else {
				g_display.curLcdcMode = g_display.nextLcdcMode;
			}
		} else {
			g_display.modeChangeCountdown = cnt - 1;
			sceDmacplus_driver_A3AA8D00(NULL);
		}
	} else {
		int bestIdx = -1;
		void *bestAddr = NULL;

		for (i = 0; i < 4; i++) {
			if (g_display.fb[i].state != 0) {
				g_display.fb[i].state = 0;
				g_display.fb[i].topAddr = g_display.fb[i].nextAddr;
			}
			if (bestIdx == -1 && g_display.fb[i].topAddr != NULL) {
				bestAddr = g_display.fb[i].topAddr;
				bestIdx = i;
			}
		}

		int bw = 0;
		int pf = g_display.hwPixelFormat;
		if (bestIdx >= 0) {
			bw = g_display.fb[bestIdx].bufferWidth;
			pf = g_display.fb[bestIdx].pixelFormat;
		}

		if (g_display.hwPixelFormat == pf && g_display.hwBufferWidth == bw) {
			g_display.activeFbIdx = (s8)bestIdx;
			sceDmacplus_driver_A3AA8D00(bestAddr);
		} else if (sceDisplay_driver_8475E040() >= 2) {
			g_display.activeFbIdx = (s8)bestIdx;
			sceDmacplus_driver_A3AA8D00(bestAddr);
			displayConfigureHw(g_display.nextLcdcMode, g_display.width, g_display.height, bw, pf);
		} else {
			if (bestIdx >= 0) {
				g_display.fb[bestIdx].state = 2;
				g_display.fb[bestIdx].nextAddr = g_display.fb[bestIdx].topAddr;
			}
			s8 curIdx = g_display.activeFbIdx;
			if (curIdx != -2) {
				void *addr = (curIdx >= 0) ? g_display.fb[curIdx].topAddr : NULL;
				sceDmacplus_driver_A3AA8D00(addr);
			}
		}
	}
	sceKernelCpuResumeIntr(intr);

	if (g_display.hasLcd == 0)
		displayUpdateEnableState();

	if (g_display.suspendCountdown > 0)
		g_display.suspendCountdown--;

	if (g_display.disableCountdown > 0) {
		g_display.disableCountdown--;
		if (g_display.disableCountdown == 0 && g_display.displayEnabled == 0)
			sceDmacplus_driver_E9B746F9();
	}

	if (g_display.brightnessHoldCount > 0)
		g_display.brightnessHoldCount--;

	if (g_display.curEnabled == 1 && g_display.brightnessHoldCount == 0 && g_display.hasLcd == 0) {
		if (g_display.currentBrightness != g_display.targetBrightness)
			displayApplyBrightness(g_display.targetBrightness);
		if (g_display.backlightPending == 2)
			g_display.backlightPending = 1;
		else if (g_display.backlightPending == 1)
			g_display.backlightPending = 0;
	}
	return -1;
}

/* 0x1050 */
static void displayUpdateEnableState(void)
{
	u8 target = (u8)g_display.targetEnabled;
	if (g_display.curEnabled == (s8)target)
		return;

	if ((s8)target == 0) {
		g_display.disableCountdown = 2;
		g_display.curEnabled = 0;
		g_display.suspendCountdown = 11;
	} else {
		int intr = sceKernelCpuSuspendIntr();
		if (sceDisplayIsVsync() == 0) {
			g_display.disableCountdown = 0;
			g_display.curEnabled = (s8)target;
			sceDmacplus_driver_ED849260();
		}
		sceKernelCpuResumeIntr(intr);
	}
}

/* 0x10E0 */
static int displayApplyBrightness(int brightness)
{
	int old = g_display.currentBrightness;
	g_display.currentBrightness = brightness;
	if (old == 0 && brightness != 0)
		g_display.backlightPending = 2;
	else if (old != 0 && brightness == 0)
		g_display.backlightPending = 0;
	return 0;
}

/* 0x112C */
static int displayConfigureHw(int lcdcMode, int width, int height, int bufferwidth, int pixelformat)
{
	int hwFmt = SCE_ERROR_INVALID_FORMAT;
	if ((u32)pixelformat < 4u)
		hwFmt = (0x39 >> (pixelformat << 1)) & 3;

	g_display.hwWidth = width;
	g_display.hwHeight = height;
	g_display.hwBufferWidth = bufferwidth;
	g_display.hwPixelFormat = pixelformat;

	sceLcdc_driver_E9DBD35F(lcdcMode, width, height, hwFmt);
	sceDmacplus_driver_E9B746F9();
	sceDmacplus_driver_88ACB6F1(width, bufferwidth, hwFmt);
	sceDmacplus_driver_ED849260();
	return 0;
}

/* 0x11CC */
int sceDisplaySetMode(int mode, int width, int height)
{
	u32 oldK1 = pspShiftK1();
	int lcdcMode = 0;

	if (mode == 0) {
		if ((s32)pspGetK1() < 0) {
			int cur = g_display.curMode;
			if (cur == 0x6D1 || cur == 0x7D1 || cur == 0x2D2 || cur == 0x3D2) {
				mode = cur;
				lcdcMode = 1;
				if (sceLcdc_driver_BAF165CE(0, width, height, 0) < 0) {
					pspSetK1(oldK1);
					return SCE_ERROR_INVALID_SIZE;
				}
				if ((width & 7) != 0) {
					pspSetK1(oldK1);
					return SCE_ERROR_INVALID_SIZE;
				}
			} else if (cur == 0x2E2) {
				mode = 0x2E2;
				lcdcMode = 12;
				if (sceLcdc_driver_BAF165CE(0, width, height, 0) < 0) {
					pspSetK1(oldK1);
					return SCE_ERROR_INVALID_SIZE;
				}
				if ((width & 7) != 0) {
					pspSetK1(oldK1);
					return SCE_ERROR_INVALID_SIZE;
				}
			}
		}
	} else {
		if ((s32)pspGetK1() < 0 && g_display.vshModeInfo == 0) {
			pspSetK1(oldK1);
			return SCE_ERROR_INVALID_MODE;
		}
		if (mode == 0x1A) {
			lcdcMode = 2;
		} else if (mode == 0x60) {
			lcdcMode = 3;
		} else if (mode == 0xD2 || mode == 0x2D2) {
			lcdcMode = 1;
		} else if (mode == 0xE0) {
			lcdcMode = 11;
		} else {
			pspSetK1(oldK1);
			return SCE_ERROR_INVALID_MODE;
		}
	}

	int ret = sceLcdc_driver_BAF165CE(lcdcMode, width, height, 0);
	if (ret < 0) {
		pspSetK1(oldK1);
		return ret;
	}
	if ((width & 7) != 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_SIZE;
	}

	int intr = sceKernelCpuSuspendIntr();
	if ((s32)pspGetK1() < 0) {
		if ((g_display.modeChangeFlags & 2) != 0) {
			sceKernelCpuResumeIntr(intr);
			pspSetK1(oldK1);
			return 0;
		}
		g_display.modeChangeFlags |= 1;
	} else {
		g_display.modeChangeFlags |= 2;
	}
	sceKernelCpuResumeIntr(intr);

	if (g_display.curLcdcMode != lcdcMode && g_display.sm1Ops != NULL)
		g_display.sm1Ops->notifyMode(1);

	intr = sceKernelCpuSuspendIntr();
	int prevLcdcMode = g_display.curLcdcMode;
	g_display.width = width;
	g_display.height = height;
	g_display.nextMode = mode;
	g_display.nextLcdcMode = lcdcMode;

	if (prevLcdcMode == lcdcMode) {
		displayUpdateTimingParams(mode);
	} else {
		g_display.fb[2].state = 1;
		if ((prevLcdcMode == 0 && lcdcMode == 11) || (prevLcdcMode == 11 && lcdcMode == 0))
			g_display.modeChangeCountdown = 1;
		else
			g_display.modeChangeCountdown = 10;
	}

	while (g_display.curLcdcMode != lcdcMode) {
		sceKernelCpuResumeIntr(intr);
		sceDisplayWaitVblankStart();
		intr = sceKernelCpuSuspendIntr();
	}

	g_display.curMode = mode;
	if ((s32)pspGetK1() < 0)
		g_display.modeChangeFlags &= ~1;
	else
		g_display.modeChangeFlags &= ~2;

	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

/* 0x14B8 */
int sceDisplayGetMode(int *pmode, int *pwidth, int *pheight)
{
	u32 oldK1 = pspShiftK1();
	u32 k1 = pspGetK1();

	if ((s32)(k1 & (u32)pmode) < 0 || (s32)(k1 & (u32)pwidth) < 0 || (s32)(k1 & (u32)pheight) < 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	int intr = sceKernelCpuSuspendIntr();
	if (pmode != NULL) {
		int mode = g_display.curMode;
		if ((s32)k1 < 0) {
			if (mode == 0x6D1 || mode == 0x7D1 || mode == 0x2D2 || mode == 0x3D2 || mode == 0x2E2)
				mode = 0;
		}
		*pmode = mode;
	}
	if (pwidth != NULL)
		*pwidth = g_display.width;
	if (pheight != NULL)
		*pheight = g_display.height;
	sceKernelCpuResumeIntr(intr);

	pspSetK1(oldK1);
	return 0;
}

/* 0x15C8 */
int sceDisplay_driver_3E17FE8D(u32 pri, void *topaddr, int bufferwidth, int pixelformat, int sync)
{
	if (pri >= 4)
		return SCE_ERROR_INVALID_ID;
	if ((u32)sync >= 2u)
		return SCE_ERROR_INVALID_MODE;

	if (topaddr != NULL) {
		if (((u32)topaddr & 0xFu) != 0)
			return SCE_ERROR_INVALID_POINTER;
		u32 seg = ((u32)topaddr >> 23) & 0x3Fu;
		if (seg == 8) {
			if (((0x35 >> (((u32)topaddr >> 29) & 7)) & 1) == 0) {
				if (((0x220202 >> (((u32)topaddr >> 27) & 0x1F)) & 1) == 0)
					return SCE_ERROR_INVALID_POINTER;
			}
		} else {
			if (((0x220202 >> (((u32)topaddr >> 27) & 0x1F)) & 1) == 0)
				return SCE_ERROR_INVALID_POINTER;
		}
		if (bufferwidth <= 0 || (bufferwidth & 0x3F) != 0)
			return SCE_ERROR_INVALID_SIZE;
	} else {
		if ((bufferwidth & 0x3F) != 0)
			return SCE_ERROR_INVALID_SIZE;
	}

	if ((u32)pixelformat >= 4u)
		return SCE_ERROR_INVALID_FORMAT;

	u32 oldK1 = pspShiftK1();
	int intr = sceKernelCpuSuspendIntr();
	SceDisplayFrameBufEntry *entry = &g_display.fb[pri];

	if (sync == 0) {
		if (entry->pixelFormat != pixelformat || entry->bufferWidth != bufferwidth) {
			sceKernelCpuResumeIntr(intr);
			pspSetK1(oldK1);
			return SCE_ERROR_INVALID_MODE;
		}
		entry->topAddr = topaddr;
		if ((entry->state & 2) != 0) {
			entry->nextAddr = topaddr;
		} else if (g_display.activeFbIdx == (s8)pri) {
			sceDmacplus_driver_A3AA8D00(topaddr);
		}
	} else {
		entry->nextAddr = topaddr;
		if (entry->bufferWidth == bufferwidth && entry->pixelFormat == pixelformat)
			entry->state |= 1;
		else
			entry->state |= 2;
		entry->pixelFormat = pixelformat;
		entry->bufferWidth = bufferwidth;
	}

	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

/* 0x17D8 */
int sceDisplay_driver_F338AAE0(u32 pri, void **topaddr, int *bufferwidth, int *pixelformat, int sync)
{
	if (pri >= 4)
		return SCE_ERROR_INVALID_ID;

	u32 oldK1 = pspShiftK1();
	u32 k1 = pspGetK1();

	if ((s32)(k1 & (u32)topaddr) < 0 || (s32)(k1 & (u32)bufferwidth) < 0 || (s32)(k1 & (u32)pixelformat) < 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	if (topaddr == NULL) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_POINTER;
	}

	int intr = sceKernelCpuSuspendIntr();
	SceDisplayFrameBufEntry *entry = &g_display.fb[pri];

	if (sync == 1 && entry->state != 0) {
		*topaddr = entry->nextAddr;
	} else {
		*topaddr = entry->topAddr;
		if (g_display.forceImmediateAddr != 0)
			*topaddr = entry->nextAddr;
	}
	if (bufferwidth != NULL)
		*bufferwidth = entry->bufferWidth;
	if (pixelformat != NULL)
		*pixelformat = entry->pixelFormat;

	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

/* 0x1948 */
int sceDisplayGetFrameBuf(void **topaddr, int *bufferwidth, int *pixelformat, int sync)
{
	return sceDisplay_driver_F338AAE0(2, topaddr, bufferwidth, pixelformat, sync);
}

/* 0x197C */
int sceDisplayGetBacklightSel(int *psel, int *punk)
{
	u32 oldK1 = pspShiftK1();
	u32 k1 = pspGetK1();

	if ((s32)(k1 & (u32)psel) < 0 || (s32)(k1 & (u32)punk) < 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	if (psel != NULL)
		*psel = g_display.backlightSel;
	if (punk != NULL)
		*punk = 0;

	pspSetK1(oldK1);
	return 0;
}

/* 0x19CC */
int sceDisplay_driver_60112E07(u32 brightness, u32 mode)
{
	if (brightness >= 101u)
		return SCE_ERROR_OUT_OF_RANGE;
	if (mode >= 2u)
		return SCE_ERROR_INVALID_MODE;

	g_display.targetBrightness = (int)brightness;
	if (g_display.brightnessHoldCount == 0)
		displayApplyBrightness((int)brightness);
	return 0;
}

/* 0x1A2C */
int sceDisplayGetBrightness(int *pbrightness, int *pmode)
{
	u32 oldK1 = pspShiftK1();
	u32 k1 = pspGetK1();

	if ((s32)(k1 & (u32)pbrightness) < 0 || (s32)(k1 & (u32)pmode) < 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	if (pbrightness != NULL)
		*pbrightness = g_display.targetBrightness;
	if (pmode != NULL)
		*pmode = 0;

	pspSetK1(oldK1);
	return 0;
}

/* 0x1A7C */
int module_start(SceSize args, void *argp)
{
	return (sceDisplayInit() < 0) ? 1 : 0;
}

/* 0x1A9C */
int sceDisplayEnable(void)
{
	int prev = g_display.displayEnabled;
	g_display.displayEnabled = 1;
	g_display.targetEnabled = 1;
	return prev;
}

/* 0x1AB8 */
int sceDisplayDisable(void)
{
	int prev = g_display.displayEnabled;
	g_display.targetEnabled = 0;
	g_display.displayEnabled = 0;
	return prev;
}

/* 0x1AD0..0x1B24: Unsupported hardware functions on ePSP */
int sceDisplay_driver_BA3ADBF0(void) { return SCE_ERROR_NOT_SUPPORTED; }
int sceDisplay_driver_78AF29C6(void) { return SCE_ERROR_NOT_SUPPORTED; }
int sceDisplay_driver_9B18DDDD(void) { return SCE_ERROR_NOT_SUPPORTED; }
int sceDisplay_driver_AF2E6CDC(void) { return SCE_ERROR_NOT_SUPPORTED; }
int sceDisplay_driver_F455917F(void) { return SCE_ERROR_NOT_SUPPORTED; }
int sceDisplay_driver_401450A6(void) { return SCE_ERROR_NOT_SUPPORTED; }
int sceDisplay_driver_D94EB277(void) { return SCE_ERROR_NOT_SUPPORTED; }
int sceDisplay_driver_7DF044BA(void) { return SCE_ERROR_NOT_SUPPORTED; }

/* 0x1B30 */
int sceDisplaySetHoldMode(int mode)
{
	u32 k1 = pspGetK1();
	if (((k1 >> 20) & 1) != 0 && mode != 0)
		mode = 1;
	pspSetK1(k1);
	g_display.holdMode = mode;
	return 0;
}

/* 0x1B58 */
int sceDisplaySetResumeMode(int mode)
{
	g_display.resumeMode = mode;
	return 0;
}

/* 0x1B68 */
int sceDisplayGetResumeMode(int *pmode)
{
	if ((s32)((pspGetK1() << 11) & (u32)pmode) < 0)
		return SCE_ERROR_PRIV_REQUIRED;
	*pmode = g_display.resumeMode;
	return 0;
}

/* 0x1B94 */
int sceDisplay_driver_996881D2(void)
{
	return g_display.vshModeInfo;
}

/* 0x1BA0 */
float sceDisplayGetFramePerSec(void)
{
	return sceLcdc_driver_9D4FB8B1();
}

/* 0x1BBC */
int sceDisplaySetFrameBuf(void *topaddr, int bufferwidth, int pixelformat, int sync)
{
	return sceDisplay_driver_3E17FE8D(2, topaddr, bufferwidth, pixelformat, sync);
}

/* 0x1BF0 */
int sceDisplayIsForeground(void)
{
	return (g_display.activeFbIdx == 2) ? 1 : 0;
}

/* 0x1C04 */
int sceDisplayGetForegroundLevel(void)
{
	return g_display.activeFbIdx;
}

/* 0x1C10 */
int sceDisplaySetBacklightSel(u32 sel, u32 mode)
{
	if (sel >= 5u)
		return SCE_ERROR_OUT_OF_RANGE;
	g_display.backlightSel = (u8)sel;
	return sceDisplay_driver_60112E07(g_display.brightnessLevels[sel], mode);
}

/* 0x1C58 */
static int displayResetTimerAndIntr(int onlyEnableIntr)
{
	if (onlyEnableIntr == 0) {
		sceSysreg_driver_4841B2D2(1);
		displayInitApbTimer1();
		u32 v = HW(APB_TIMER1_BASE + 0x08);
		while (HW(APB_TIMER1_BASE + 0x08) == v)
			;
	} else {
		sceKernelEnableIntr(30);
	}
	return 0;
}

/* 0x1CA8 */
static int displayMasterVblankIntrHandler(int intr_code)
{
	int i;

	g_vblank.vcount++;
	if (g_vblank.isPseudoVsyncEnabled == 0 || g_vblank.pseudoVsyncInterval < 0) {
		g_vblank.isPseudoField = 0;
	} else {
		if (displayGetRawHcount() < g_vblank.vblank) {
			g_vblank.isPseudoField = 0;
		} else if (g_vblank.isPseudoField != 0) {
			g_vblank.isPseudoField--;
		}
	}

	for (i = 0; i < 32; i++) {
		u8 sub = g_subIntrCallOrder[i];
		if ((g_vblank.subIntrMask & (1u << sub)) != 0)
			sceKernelCallSubIntrHandler(intr_code, sub, (void *)(u32)sub, NULL);
	}

	sceKernelSetEventFlag(g_vblank.evFlagId, (u32)g_vblank.vcount | 0x80000000u);
	sceKernelClearEventFlag(g_vblank.evFlagId, 0);
	return -1;
}

/* 0x1DCC */
static void displayInitApbTimer1(void)
{
	int intr = sceKernelCpuSuspendIntr();
	sceSysregApbTimerClkDisable(1);
	sceSysregApbTimerClkSelect(1, 4);
	sceSysregApbTimerClkEnable(1);

	while ((HW(APB_TIMER1_BASE + 0x20) & 4) != 0)
		;
	HW(APB_TIMER1_BASE + 0x00) = 0;
	while ((HW(APB_TIMER1_BASE + 0x20) & 4) != 0)
		;
	while ((HW(APB_TIMER1_BASE + 0x20) & 2) != 0)
		;
	HW(APB_TIMER1_BASE + 0x04) = 0;
	while ((HW(APB_TIMER1_BASE + 0x20) & 8) != 0)
		;
	HW(APB_TIMER1_BASE + 0x0C) = 0xFFFFFFFFu;
	while ((HW(APB_TIMER1_BASE + 0x20) & 0x10) != 0)
		;
	HW(APB_TIMER1_BASE + 0x10) = 0;
	while ((HW(APB_TIMER1_BASE + 0x20) & 0x20) != 0)
		;
	HW(APB_TIMER1_BASE + 0x14) = 0;
	while ((HW(APB_TIMER1_BASE + 0x20) & 0x3A) != 0)
		;
	HW(APB_TIMER1_BASE + 0x00) = 0x00020201u;
	while ((HW(APB_TIMER1_BASE + 0x20) & 4) != 0)
		;

	sceSysregApbTimerClkDisable(1);
	sceSysregApbTimerClkSelect(1, 7);
	sceSysregApbTimerClkEnable(1);
	HW(APB_TIMER1_BASE + 0x24) = 1;
	sceKernelCpuResumeIntrWithSync(intr);
}

/* 0x1F04 */
static int displayWaitVblankInternal(int vblankCount, int isCB)
{
	if (vblankCount <= 0)
		return SCE_ERROR_INVALID_VALUE;

	u32 oldK1 = pspShiftK1();
	if (sceKernelIsIntrContext() != 0) {
		pspSetK1(oldK1);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	u32 *ktls = (u32 *)sceKernelGetKTLS(g_vblank.ktlsId);
	if (ktls == NULL) {
		pspSetK1(oldK1);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	u32 depth = ktls[0];
	u32 curVcount = (u32)g_vblank.vcount & 0x7FFFFFFFu;
	u32 startVcount = ktls[depth + 1];
	ktls[0] = depth + 1;
	if (depth + 1 < 7u)
		ktls[depth + 2] = startVcount;

	int ret;
	u32 outBits = 0;
	do {
		if (isCB != 0)
			ret = sceKernelWaitEventFlagCB(g_vblank.evFlagId, 0xFFFFFFFFu, 1, &outBits, NULL);
		else
			ret = sceKernelWaitEventFlag(g_vblank.evFlagId, 0xFFFFFFFFu, 1, &outBits, NULL);
		if (ret < 0)
			break;
		curVcount = outBits & 0x7FFFFFFFu;
	} while ((s32)((curVcount - startVcount) & 0x7FFFFFFFu) < vblankCount);

	u32 newDepth = ktls[0] - 1;
	ktls[newDepth + 1] = curVcount;
	ktls[0] = newDepth;
	pspSetK1(oldK1);
	return ret;
}

/* 0x2050 */
int sceDisplayAdjustAccumulatedHcount(int value)
{
	if (value < 0)
		return SCE_ERROR_INVALID_VALUE;

	int intr = sceKernelCpuSuspendIntr();
	int tries = 1;
	u32 t1, t2;
	for (;;) {
		if (tries >= 4) {
			sceKernelCpuResumeIntr(intr);
			return SCE_KERNEL_ERROR_ERROR;
		}
		t1 = HW(APB_TIMER1_BASE + 0x04);
		t2 = HW(APB_TIMER1_BASE + 0x04);
		tries++;
		if (t1 == t2)
			break;
	}

	g_vblank.accumHcount = value;
	g_vblank.hcountOffset = value - (int)t1;
	g_vblank.baseTimerVal = t1;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* 0x20EC */
int sceDisplayGetAccumulatedHcount(void)
{
	int intr = sceKernelCpuSuspendIntr();
	int tries = 1;
	u32 t1, t2;
	for (;;) {
		if (tries >= 4) {
			sceKernelCpuResumeIntr(intr);
			return SCE_KERNEL_ERROR_ERROR;
		}
		t1 = HW(APB_TIMER1_BASE + 0x04);
		t2 = HW(APB_TIMER1_BASE + 0x04);
		tries++;
		if (t1 == t2)
			break;
	}

	int res;
	if (g_vblank.hcountPerVblank == 0) {
		res = (int)t1 + g_vblank.hcountOffset;
	} else {
		u32 delta = t1 - g_vblank.baseTimerVal;
		u32 hperiod = g_vblank.hperiod;
		if (delta >= hperiod) {
			u32 vblanks = 1;
			u32 sub = hperiod;
			if (delta >= (hperiod << 1)) {
				vblanks = delta / hperiod;
				sub = vblanks * hperiod;
			}
			delta -= sub;
			g_vblank.baseTimerVal += sub;
			g_vblank.accumHcount += (int)(vblanks * 286);
		}
		res = g_vblank.accumHcount + (int)((delta * 286u) / g_vblank.hperiod);
	}

	sceKernelCpuResumeIntr(intr);
	return res & 0x7FFFFFFF;
}

/* 0x221C */
static int displayGetRawHcount(void)
{
	int intr = sceKernelCpuSuspendIntr();
	int tries = 1;
	u32 t0, t1, t2, t3;
	for (;;) {
		if (tries >= 4) {
			sceKernelCpuResumeIntr(intr);
			return SCE_KERNEL_ERROR_ERROR;
		}
		t0 = HW(APB_TIMER1_BASE + 0x08);
		t1 = HW(APB_TIMER1_BASE + 0x04);
		t2 = HW(APB_TIMER1_BASE + 0x04);
		tries++;
		if (t2 != t1)
			continue;
		t3 = HW(APB_TIMER1_BASE + 0x08);
		tries++;
		if (t0 == t3)
			break;
	}
	sceKernelCpuResumeIntr(intr);

	u32 diff = ((t1 - t0 - 1) & 0x07FFFFFFu) + 4;
	u32 hperiod = g_vblank.hperiod;
	if (g_vblank.isPseudoVsyncEnabled != 0) {
		if (diff >= hperiod)
			diff -= hperiod;
		if (g_vblank.pseudoVsyncInterval > 0) {
			u32 half = ((s32)hperiod / 2);
			if (diff >= half + 1)
				diff = diff - half - 1;
		}
	}
	return (int)diff;
}

/* 0x2308 */
int sceDisplayGetCurrentHcount(void)
{
	int raw = displayGetRawHcount();
	if (g_vblank.hcountPerVblank != 0)
		raw = (raw * 286) / (int)g_vblank.hperiod;
	return raw;
}

/* 0x2360 */
static int displayInitVblank(void)
{
	int i;

	memset(&g_vblank, 0, sizeof(g_vblank));
	g_vblank.pseudoVsyncInterval = -1;
	g_vblank.isPseudoVsyncEnabled = 0;
	g_vblank.ktlsId = sceKernelAllocateKTLS(0x20, NULL, NULL);
	g_vblank.evFlagId = sceKernelCreateEventFlag("SceDisplayVblank", 0x201, 0, NULL);
	sceSysreg_driver_4841B2D2(1);
	displayInitApbTimer1();
	sceKernelRegisterIntrHandler(30, 1, displayMasterVblankIntrHandler, NULL, (void *)&g_displayIntrOpt);
	sceKernelRegisterIntrHandler(50, 2, displayPseudoVsyncIntrHandler, NULL, NULL);
	for (i = 0; i < 16; i++)
		sceKernelSetUserModeIntrHanlerAcceptable(30, i, 1);
	sceKernelEnableIntr(30);
	return 0;
}

/* 0x2454 */
static int displayEndVblank(void)
{
	sceSysregApbTimerClkDisable(1);
	sceKernelReleaseIntrHandler(30);
	sceKernelReleaseIntrHandler(50);
	sceKernelDeleteEventFlag(g_vblank.evFlagId);
	sceKernelFreeKTLS(g_vblank.ktlsId);
	return 0;
}

/* 0x24A4 */
static int displayDisableVblank(void)
{
	sceSysregApbTimerClkDisable(1);
	sceKernelDisableIntr(30);
	return 0;
}

/* 0x24CC */
static int displayUpdateTimingParams(int mode)
{
	int hsync = 0, vsync = 0, vblank = 0, hperiod = 0;
	int intr = sceKernelCpuSuspendIntr();
	sceLcdc_driver_2D80BB2E(&hsync, &vsync, NULL, &vblank, &hperiod);
	g_vblank.vblank = vblank;
	g_vblank.hsync = hsync;
	g_vblank.vsync = vsync;
	g_vblank.hperiod = (u32)hperiod;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* 0x253C */
int sceDisplayGetVcount(void)
{
	return g_vblank.vcount & 0x7FFFFFFF;
}

/* 0x254C */
int sceDisplayIsVblank(void)
{
	return (sceDisplayGetCurrentHcount() < 14) ? 1 : 0;
}

/* 0x256C */
int sceDisplayGetVblankRest(void)
{
	int rest = 14 - sceDisplayGetCurrentHcount();
	return (rest > -1) ? rest : -1;
}

/* 0x2598 */
int sceDisplay_driver_8475E040(void)
{
	int raw = displayGetRawHcount();
	int rest = g_vblank.hsync - raw;
	return (rest > -1) ? rest : -1;
}

/* 0x25D0 */
int sceDisplayIsVsync(void)
{
	int raw = displayGetRawHcount();
	if (raw < g_vblank.hsync)
		return 0;
	return (raw < g_vblank.hsync + g_vblank.vsync) ? 1 : 0;
}

/* 0x2614 */
int sceDisplayWaitVblank(void)
{
	return displayWaitVblankImpl(0);
}

/* 0x2630 */
int sceDisplayWaitVblankCB(void)
{
	return displayWaitVblankImpl(1);
}

/* 0x264C */
int sceDisplayWaitVblankStart(void)
{
	return displayWaitVblankInternal(1, 0);
}

/* 0x266C */
int sceDisplayWaitVblankStartCB(void)
{
	return displayWaitVblankInternal(1, 1);
}

/* 0x268C */
int sceDisplay_driver_A70066A1(int vblankCount)
{
	return displayWaitVblankInternal(vblankCount, 0);
}

int sceDisplayWaitVblankStartMulti(int vblankCount)
	__attribute__((alias("sceDisplay_driver_A70066A1")));

/* 0x26A8 */
int sceDisplay_driver_113958AE(int vblankCount)
{
	return displayWaitVblankInternal(vblankCount, 1);
}

int sceDisplayWaitVblankStartMultiCB(int vblankCount)
	__attribute__((alias("sceDisplay_driver_113958AE")));

/* 0x26C4 */
int sceDisplay_driver_CE8A328E(void)
{
	int raw = displayGetRawHcount();
	int hp = (int)g_vblank.hperiod;
	if (hp != 286)
		raw = (raw * 286) / hp;
	return raw;
}

/* 0x2710 */
int sceDisplaySetPseudoVsync(int interval)
{
	int enabled = g_vblank.isPseudoVsyncEnabled;
	g_vblank.pseudoVsyncInterval = interval;
	if (enabled != 0)
		displaySetupPseudoVsyncTimer(interval);
	return 0;
}

/* 0x274C */
int sceDisplayIsPseudoField(void)
{
	return (g_vblank.isPseudoField != 0) ? 1 : 0;
}

/* 0x275C..0x27D8: Sub-interrupt controller callbacks */
static int displaySubIntrEnable(int intr_code, int sub_intr_code)
{
	g_vblank.subIntrMask |= 1u << sub_intr_code;
	return 0;
}

static int displaySubIntrDisable(int intr_code, int sub_intr_code)
{
	g_vblank.subIntrMask &= ~(1u << sub_intr_code);
	return 0;
}

static int displaySubIntrSuspend(int intr_code, int sub_intr_code, int *state)
{
	if (state != NULL)
		*state = (g_vblank.subIntrMask >> sub_intr_code) & 1;
	g_vblank.subIntrMask &= ~(1u << sub_intr_code);
	return 0;
}

static int displaySubIntrResume(int intr_code, int sub_intr_code, int state)
{
	g_vblank.subIntrMask |= (u32)state << sub_intr_code;
	return 0;
}

/* 0x27F4 */
static int displayPseudoVsyncIntrHandler(void)
{
	u32 v = HW(APB_TIMER2_BASE + 0x18);
	HW(APB_TIMER2_BASE + 0x1C) = v;
	g_vblank.isPseudoField = 2;
	sceSysreg_driver_4E5C86AA();
	return -1;
}

/* 0x2828 */
static int displayWaitVblankImpl(int isCB)
{
	u32 oldK1 = pspShiftK1();
	int ret = 1;

	if (sceDisplayGetCurrentHcount() < 14) {
		u32 *ktls = (u32 *)sceKernelGetKTLS(g_vblank.ktlsId);
		if (ktls != NULL) {
			u32 depth = ktls[0];
			if (depth < 7u)
				ktls[depth + 1] = (u32)g_vblank.vcount & 0x7FFFFFFFu;
		}
	} else {
		ret = displayWaitVblankInternal(1, isCB);
	}

	pspSetK1(oldK1);
	return ret;
}

/* 0x28C8 */
static int displaySetupPseudoVsyncTimer(int interval)
{
	int intr = sceKernelCpuSuspendIntr();
	if (interval >= 0) {
		sceSysregApbTimerClkDisable(2);
		sceSysregApbTimerClkSelect(2, 4);
		sceSysregApbTimerClkEnable(2);
		HW(APB_TIMER2_BASE + 0x00) = 0x8A4;
		HW(APB_TIMER2_BASE + 0x1C) = 1;
		HW(APB_TIMER2_BASE + 0x04) = 0;
		HW(APB_TIMER2_BASE + 0x10) = (u32)interval;
		HW(APB_TIMER2_BASE + 0x24) = 1;
		sceKernelEnableIntr(50);
	} else {
		HW(APB_TIMER2_BASE + 0x00) = 0;
		HW(APB_TIMER2_BASE + 0x1C) = 1;
		sceKernelDisableIntr(50);
		sceSysregApbTimerClkDisable(2);
	}
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* 0x2980 */
static int displaySetupBootFade(int pixelFormat, int bufferWidth, int holdFrames, int stepAlpha)
{
	u8 *edram = (u8 *)sceGeEdramGetAddr();
	g_fade.pixelFormat = pixelFormat;
	g_fade.bufferWidth = bufferWidth;
	g_fade.holdFrames = holdFrames;
	g_fade.stepAlpha = stepAlpha;
	g_fade.curAlpha = 0xFF;
	g_fade.bufIdx = 0;

	if (pixelFormat >= 0) {
		if (pixelFormat < 3) {
			int stride = bufferWidth * 17;
			g_fade.fbAddr[0] = edram + (stride << 5);
			g_fade.fbAddr[1] = edram + (stride << 6);
		} else if (pixelFormat == 3) {
			int stride = bufferWidth * 17;
			g_fade.fbAddr[0] = edram + (stride << 6);
			g_fade.fbAddr[1] = edram + (stride << 7);
		}
	}

	g_fade.geListAddr = (u32 *)(edram + bufferWidth * 0xCC0);
	void *base = sceGeEdramGetAddr();
	int qid = displayEnqueueFadeGeList(base, g_fade.fbAddr[0], bufferWidth, pixelFormat, 480, 272, 0xFFFFFFu);
	sceGeListSync(qid, 0);
	qid = displayEnqueueFadeGeList(base, g_fade.fbAddr[1], bufferWidth, pixelFormat, 480, 272, 0xFFFFFFu);
	sceGeListSync(qid, 0);
	return 0;
}

/* 0x2ABC */
static int displayStepBootFade(void)
{
	if (g_fade.curAlpha == 0)
		return 1;

	int idx = g_fade.bufIdx;
	int nextIdx = idx ^ 1;
	void *curFb = g_fade.fbAddr[idx];
	void *nextFb = g_fade.fbAddr[nextIdx];
	g_fade.bufIdx = nextIdx;

	sceDisplaySetFrameBuf(curFb, g_fade.bufferWidth, g_fade.pixelFormat, 0);
	void *base = sceGeEdramGetAddr();

	if (g_fade.holdFrames > 0) {
		g_fade.holdFrames--;
	} else {
		int a = g_fade.curAlpha - g_fade.stepAlpha;
		g_fade.curAlpha = (a >= 0) ? a : 0;
	}

	u32 a = (u32)g_fade.curAlpha;
	u32 color = (a << 16) | (a << 8) | a;
	if (sceGeDrawSync(1) != 0)
		return 0;

	displayEnqueueFadeGeList(base, nextFb, g_fade.bufferWidth, g_fade.pixelFormat, 480, 272, color);
	return 0;
}

/* 0x2BD0 */
static int displayEnqueueFadeGeList(void *srcAddr, void *dstAddr, int bufferWidth, int pixelFormat,
				    int width, int height, u32 color)
{
	u32 *list = g_fade.geListAddr;
	u8 *verts = (u8 *)list + 0x60;
	int i;

	for (i = 0; i < 24; i++)
		list[i] = g_fadeStepGeCmds[i];

	u32 sizeCmd = ((u32)(height - 1) << 10) | (u32)(width - 1);
	list[0] = 0x9C000000u | ((u32)dstAddr & 0x00FFFFFFu);
	list[1] = 0x9D000000u | (((u32)dstAddr >> 8) & 0x000F0000u) | (u32)bufferWidth;
	list[2] = 0xD2000000u | (u32)pixelFormat;
	list[4] = 0xD5000000u | sizeCmd;
	list[6] = 0x16000000u | sizeCmd;
	list[10] = 0xA0000000u | ((u32)srcAddr & 0x00FFFFFFu);
	list[11] = 0xA8000000u | (((u32)srcAddr >> 8) & 0x000F0000u) | (u32)bufferWidth;
	list[13] = 0xC3000000u | (u32)pixelFormat;
	list[19] = 0x10000000u | (((u32)verts >> 8) & 0x000F0000u);
	list[20] = 0x01000000u | ((u32)verts & 0x00FFFFFFu);

	u16 x = 0;
	for (i = 0; i < 8; i++) {
		u16 nextX = x + 64;
		u16 *v = (u16 *)(verts + i * 32);
		v[0] = x;
		v[1] = 0;
		*(u32 *)&v[2] = color;
		v[4] = x;
		v[5] = 0;
		v[6] = 0;
		v[8] = nextX;
		v[9] = (u16)height;
		*(u32 *)&v[10] = color;
		v[12] = nextX;
		v[13] = (u16)height;
		v[14] = 0;
		x = nextX;
	}

	sceKernelDcacheWritebackRange(g_fade.geListAddr, 0x160);
	return sceGeListEnQueue(g_fade.geListAddr, NULL, -1, NULL);
}

/* 0x2D94 */
static int displayFinishBootFade(void)
{
	int i;

	sceGeDrawSync(0);
	u32 *list = g_fade.geListAddr;
	for (i = 0; i < 20; i++)
		list[i] = g_fadeEndGeCmds[i];

	sceKernelDcacheWritebackRange(g_fade.geListAddr, 0x50);
	sceGeListEnQueue(g_fade.geListAddr, NULL, -1, NULL);
	sceGeDrawSync(0);
	return 0;
}
