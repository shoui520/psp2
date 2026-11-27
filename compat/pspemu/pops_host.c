/*
 * pops_host.c: ScePspemu PS1 (POPS) host display scaler, audio output thread,
 * menuinfo persistence, button remapper, and VMC/PocketStation bridge
 * (0x8102f0d8..0x81030703).
 */
#include "host_common.h"

#include <stdio.h>
#include <string.h>

#define POPS_SHARED_STATE_PADDR     0x09FE0000U
#define POPS_VRAM_BASE_PADDR        0x090C0000U
#define POPS_VRAM_STRIDE_BYTES      1310720U
#define POPS_VMC_SIZE_BYTES         0x20080U
#define POPS_MENUINFO_SIZE          0x400U
#define POPS_MENUINFO_MAGIC_LO      0x53504F50U /* "POPS" */
#define POPS_MENUINFO_MAGIC_HI      0x4746432EU /* ".CFG" */
#define POPS_MENUINFO_VERSION       5U
#define POPS_ERR_IO                 ((int)0x80010005)
#define POPS_ERR_NO_GUEST           ((int)0x80000001)
#define POPS_APP_STATE_SDK_VERSION  PSPEMU_SDK_VERSION

typedef struct PopsScaleEntry {
	uint32_t scaleX;
	uint32_t scaleY;
	int32_t offsetX;
	int32_t offsetY;
} PopsScaleEntry;

typedef struct PopsDisplaySlot {
	int16_t startX;
	int16_t startY;
	int16_t width;
	int16_t height;
	uint8_t modeFlags;
	uint8_t frameIndex;
} PopsDisplaySlot;

typedef struct PopsDisplayState {
	PopsDisplaySlot slots[2];
	uint8_t activeSlot;
	uint8_t padSlot[11];
} PopsDisplayState;

_Static_assert(sizeof(PopsDisplaySlot) == 10U, "PopsDisplaySlot size");
_Static_assert(sizeof(PopsDisplayState) == 32U, "PopsDisplayState size");

typedef struct PopsDisplayFrameBuf {
	uint32_t size;
	void *base;
	uint32_t pitch;
	uint32_t pixelformat;
	uint32_t width;
	uint32_t height;
	uint32_t flags;
} PopsDisplayFrameBuf;

_Static_assert(sizeof(PopsDisplayFrameBuf) == 28U, "PopsDisplayFrameBuf size");

typedef struct PopsCaptureFrameBuf {
	uint32_t size;
	void *base;
	uint32_t pitch;
	uint32_t pixelformat;
	uint32_t width;
	uint32_t height;
} PopsCaptureFrameBuf;

_Static_assert(sizeof(PopsCaptureFrameBuf) == 24U, "PopsCaptureFrameBuf size");

typedef struct PopsGuestControlBlock {
	PopsDisplayState display;
	uint32_t padButtons[4][4];
	uint8_t reservedArea[64];
	uint8_t audioPaused;
	uint8_t discLoadSpeedFlag;
	uint8_t hostPaused;
	uint8_t singleDiscFlag;
	uint8_t currentDisc;
	uint8_t discCount;
	uint8_t resetAction;
	uint8_t multiTapMode;
	uint8_t analogMode;
	uint8_t padExtra;
	uint8_t screenMode;
	uint8_t analogDisabled;
	uint16_t titleWide[120];
	uint8_t titleSplitPos;
	uint8_t titleValidFlag;
	uint8_t vmcSyncFlag[2];
	uint8_t customScreenFlag;
	uint8_t padAlign[3];
	uint32_t vmcGuestAddr[2];
	int32_t pendingErrorCode;
} PopsGuestControlBlock;

_Static_assert(offsetof(PopsGuestControlBlock, audioPaused) == 160U, "audioPaused offset");
_Static_assert(offsetof(PopsGuestControlBlock, vmcSyncFlag) == 414U, "vmcSyncFlag offset");
_Static_assert(offsetof(PopsGuestControlBlock, vmcGuestAddr) == 420U, "vmcGuestAddr offset");
_Static_assert(offsetof(PopsGuestControlBlock, pendingErrorCode) == 428U, "pendingErrorCode offset");

typedef struct PopsMenuInfoFile {
	uint32_t magicLo;
	uint32_t magicHi;
	uint32_t version;
	uint32_t controllerMode;
	uint32_t screenMode;
	uint32_t volumeLevel;
	uint32_t multiTapMode;
	uint16_t analogMode;
	uint16_t customAssignEnabled;
	uint32_t bilinearFilter;
	uint32_t displayPreset;
	int32_t customOffsetX;
	int32_t customOffsetY;
	uint32_t customWidth;
	uint32_t customHeight;
	uint8_t buttonMap[18];
	uint8_t screenLeftUp;
	uint8_t screenRightUp;
	uint8_t screenLeftDown;
	uint8_t screenRightDown;
	uint8_t rearTouchLeftUp;
	uint8_t rearTouchRightUp;
	uint8_t rearTouchLeftDown;
	uint8_t rearTouchRightDown;
	uint8_t reservedMap[6];
	uint8_t dpadAssign;
	uint8_t pocsMounted;
	uint8_t padPocs;
	uint8_t pocsBgType;
	uint8_t pocsFirstBootSeen;
	uint8_t pocsDisplayChannelDefault;
	uint8_t pocsDisplayChannelAlt;
	uint8_t pocsDisplaySlot;
	uint8_t pocsAutoBoot;
	uint8_t trailingPad[927];
} PopsMenuInfoFile;

_Static_assert(sizeof(PopsMenuInfoFile) == POPS_MENUINFO_SIZE, "PopsMenuInfoFile size");
_Static_assert(offsetof(PopsMenuInfoFile, buttonMap) == 56U, "buttonMap offset");
_Static_assert(offsetof(PopsMenuInfoFile, screenLeftUp) == 74U, "screenLeftUp offset");
_Static_assert(offsetof(PopsMenuInfoFile, rearTouchLeftUp) == 78U, "rearTouchLeftUp offset");
_Static_assert(offsetof(PopsMenuInfoFile, dpadAssign) == 88U, "dpadAssign offset");
_Static_assert(offsetof(PopsMenuInfoFile, pocsAutoBoot) == 96U, "pocsAutoBoot offset");

static const uint32_t g_popsHorizontalDivisors[4] = { 28U, 35U, 56U, 70U };

static const uint16_t g_popsButtonMasks[19] = {
	0x0010U, 0x0040U, 0x0080U, 0x0020U,
	0x1000U, 0x2000U, 0x4000U, 0x8000U,
	0x0400U, 0x0800U, 0x0100U, 0x0200U,
	0x0002U, 0x0004U, 0x0001U, 0x0008U,
	0x0000U, 0x0C00U, 0x0300U
};

static PopsScaleEntry g_popsScalePresets[2][5] = {
	{
		{ 30678338U, 32768U, 0, 0 },
		{ 29776034U, 28913U, 0, 0 },
		{ 20452225U, 19859U, 0, 0 },
		{ 20452225U, 28913U, 0, 0 },
		{ 30678338U, 27525U, 0, 0 },
	},
	{
		{ 30678338U, 32768U, 0, 0 },
		{ 26467586U, 30840U, 0, 0 },
		{ 20452225U, 23831U, 0, 0 },
		{ 20452225U, 30840U, 0, 0 },
		{ 30678338U, 30684U, 0, 0 },
	},
};

uint8_t g_popsMenuConfig[240];
int g_popsSpuAudioFlags;
static int g_popsAudioPort;

static uint32_t g_popsAudioRingBuf[2][512];
static volatile int g_popsDisplayPaused;
static int g_popsVibrationEnabled;
static int g_popsCdramOffset;
static char g_popsMenuInfoPath[128];
static int g_popsBilinearFilter;
static PopsScaleEntry g_popsCustomScale;
static int g_popsDisplayPreset;
static PopsMenuInfoFile g_popsMenuInfo;
static int g_popsAudioVolumeGain;

/* 0x8102f0d8 */
static PopsScaleEntry *__attribute__((noinline, noclone))
popsCalcDisplayOffset(PopsScaleEntry *entry, int frameHeight)
{
	entry->offsetX = (int32_t)((960.0 - 640.0 / (double)entry->scaleX * 30678337.828571428) * 0.5);
	entry->offsetY = (int32_t)((544.0 - (double)frameHeight / (double)entry->scaleY * 32768.0) * 0.5);
	return entry;
}

/* 0x8102f160 */
static int __attribute__((noinline)) popsResetTouchDefaults(void)
{
	int idx;

	for (idx = 10; idx < 18; idx++)
		g_popsMenuInfo.buttonMap[idx] = 16U;

	g_popsMenuInfo.rearTouchLeftUp = 13U;
	g_popsMenuInfo.rearTouchRightUp = 14U;
	g_popsMenuInfo.rearTouchLeftDown = 15U;
	g_popsMenuInfo.rearTouchRightDown = 16U;
	g_popsMenuInfo.screenLeftUp = 0U;
	g_popsMenuInfo.screenRightUp = 0U;
	g_popsMenuInfo.screenLeftDown = 0U;
	g_popsMenuInfo.screenRightDown = 0U;
	g_popsMenuInfo.dpadAssign = 0U;
	return 15;
}

/* 0x8102f1b4 */
static uint8_t *__attribute__((noinline, noclone))
popsSetVmcSyncFlag(int slot, uint8_t value)
{
	uint8_t *flags = (uint8_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 414U, 1, 2);
	if (flags[slot] != 1U)
		flags[slot] = value;
	return flags;
}

/* 0x8102f1d8 */
static int __attribute__((noinline, noclone))
popsUpdateDisplayFrameBuf(const PopsDisplayState *state, void *dstFrameBuf, int preset)
{
	const PopsDisplaySlot *slot = &state->slots[state->activeSlot & 1U];
	int isPal = (slot->modeFlags & 8U) != 0U ? 1 : 0;
	int useBilinear = g_popsBilinearFilter;
	uint8_t *vramBase = (uint8_t *)remotefsTranslateGuestAddr(POPS_VRAM_BASE_PADDR, 0, 0);
	PopsScaleEntry *scaleEntry;
	PopsDisplayFrameBuf srcFb;
	uint32_t horizDiv;
	uint32_t scaledWidth;
	uint32_t scaleX;
	uint32_t scaleY;
	int32_t posX;
	int32_t posY;
	uint32_t interlaceShift;
	uint32_t yShift;

	if (preset < 0) {
		scaleEntry = &g_popsScalePresets[isPal][0];
	} else if (sceCompatGetPrimaryHead() != 0) {
		scaleEntry = &g_popsScalePresets[isPal][4];
		scaleEntry->scaleX = (dstFrameBuf != NULL) ? 28633115U : 30678338U;
		useBilinear = 1;
		popsCalcDisplayOffset(scaleEntry, isPal ? 512 : 480);
	} else if (preset > 3) {
		int baseHeight = isPal ? 512 : 480;
		scaleEntry = &g_popsCustomScale;
		scaleEntry->scaleX = (uint32_t)(int32_t)((640.0f / (float)(g_popsMenuInfo.customWidth + 640U)) * 30678337.828571428 + 0.5);
		scaleEntry->scaleY = (uint32_t)(int32_t)(((float)baseHeight / (float)(g_popsMenuInfo.customHeight + (uint32_t)baseHeight)) * 32768.0 + 0.5);
		popsCalcDisplayOffset(scaleEntry, baseHeight);
		scaleEntry->offsetX += g_popsMenuInfo.customOffsetX;
		scaleEntry->offsetY += g_popsMenuInfo.customOffsetY;
	} else {
		scaleEntry = &g_popsScalePresets[isPal][preset];
	}

	sceClibMemset(&srcFb, 0, sizeof(srcFb));
	scaleX = scaleEntry->scaleX;
	scaleY = scaleEntry->scaleY;

	if ((slot->modeFlags & 0x40U) != 0U)
		horizDiv = 40U;
	else
		horizDiv = g_popsHorizontalDivisors[slot->modeFlags & 3U];

	srcFb.size = sizeof(srcFb);
	srcFb.base = &vramBase[(uint32_t)slot->frameIndex * POPS_VRAM_STRIDE_BYTES];
	srcFb.height = (uint32_t)slot->height;
	scaledWidth = (uint32_t)((uint32_t)slot->width * horizDiv) / 140U;
	srcFb.width = scaledWidth;

	if ((slot->modeFlags & 0x10U) != 0U) {
		srcFb.pitch = 640U;
		srcFb.pixelformat = 0U;
		if (scaledWidth > 640U)
			srcFb.width = 640U;
	} else {
		srcFb.pitch = 1280U;
		srcFb.pixelformat = 0x50000000U;
	}

	srcFb.flags = (useBilinear != 0) ? 0x100000U : 0U;

	interlaceShift = ((slot->modeFlags & 4U) != 0U && (slot->modeFlags & 0x20U) != 0U) ? 1U : 0U;
	yShift = 16U - interlaceShift;

	posX = scaleEntry->offsetX +
	       (int32_t)((((float)(slot->startX - 512) * 7669584.5f) / (float)scaleX) + 0.5f);
	posY = scaleEntry->offsetY + (int32_t)(((uint32_t)slot->startY << yShift) / scaleY);

	if (preset < 0) {
		posX -= 160;
		posY -= isPal ? 16 : 32;
		if (slot->width > 2560) {
			float ratio = (float)slot->width / 2560.0f;
			scaleX = (uint32_t)(int32_t)((float)scaleX * ratio);
			scaleY = (uint32_t)(int32_t)((float)scaleY * ratio);
		}
	}

	if (posY < 0) {
		uint32_t skipLines = (scaleY * (uint32_t)(-posY)) >> yShift;
		uint8_t *fbBytes = (uint8_t *)srcFb.base;
		srcFb.height -= skipLines;
		srcFb.base = &fbBytes[skipLines * 2560U];
		posY = 0;
	}

	if (posX < 0)
		posX = 0;

	return sceDisplaySetFrameBufForCompat(
		posX,
		posY,
		(int)scaleX,
		(int)(scaleY << interlaceShift),
		((slot->modeFlags & 0x80U) == 0U && srcFb.height != 0U) ? &srcFb : NULL,
		dstFrameBuf);
}

/* 0x8102f5a0 */
static int __attribute__((noinline)) popsSyncMenuToGuest(void)
{
	PopsGuestControlBlock *guest =
		(PopsGuestControlBlock *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR, 0, 0);

	guest->currentDisc = (uint8_t)g_popsMenuInfo.controllerMode;
	guest->screenMode = (uint8_t)g_popsMenuInfo.screenMode;
	popsSetAudioVolume((int)g_popsMenuInfo.volumeLevel);
	if (pspemuHasRearTouchCapability() == 0)
		guest->multiTapMode = (uint8_t)g_popsMenuInfo.multiTapMode;
	guest->analogMode = (uint8_t)g_popsMenuInfo.analogMode;
	g_popsMenuInfo.version = POPS_MENUINFO_VERSION;
	g_popsBilinearFilter = (int)g_popsMenuInfo.bilinearFilter;
	g_popsDisplayPreset = (int)g_popsMenuInfo.displayPreset;
	return g_popsBilinearFilter;
}

/* 0x8102f5fc */
static int __attribute__((noinline)) popsSaveMenuInfo(void)
{
	SceUID fd = sceIoOpen(g_popsMenuInfoPath, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0x186);
	SceSSize written;

	if (fd < 0) {
		int32_t *errSlot = (int32_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 428U, 1, 4);
		*errSlot = POPS_ERR_IO;
		pspemuTriggerFatalError(fd);
	}

	written = sceIoWrite(fd, &g_popsMenuInfo, sizeof(g_popsMenuInfo));
	sceIoClose(fd);
	if (written != (SceSSize)sizeof(g_popsMenuInfo)) {
		int32_t *errSlot = (int32_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 428U, 1, 4);
		*errSlot = POPS_ERR_IO;
		pspemuTriggerFatalError((written < 0) ? (int)written : -1);
	}
	return 0;
}

/* 0x8102f664 */
SceBool popsHostIsOverlayActive(void)
{
	SceAppMgrAppState appState;

	if (_sceAppMgrGetAppState(&appState, sizeof(appState), POPS_APP_STATE_SDK_VERSION) >= 0)
		return appState.isSystemUiOverlaid != 0 ? SCE_TRUE : SCE_FALSE;
	return SCE_FALSE;
}

/* 0x8102f6a4 */
static int popsDisplayThread(SceSize args, void *argp)
{
	PopsGuestControlBlock *guest =
		(PopsGuestControlBlock *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR, 0, 0);
	PopsDisplayState displayCopy;
	SceCtrlData2 ctrlData;
	SceCtrlWirelessControllerInfo wirelessInfo;
	int prevSlot = -1;
	int isNtscAudio = 0;
	int idx;

	(void)args;
	(void)argp;

	for (idx = 0; idx < 5; idx++)
		popsCalcDisplayOffset(&g_popsScalePresets[0][idx], 480);
	for (idx = 0; idx < 5; idx++)
		popsCalcDisplayOffset(&g_popsScalePresets[1][idx], 512);

	pspemuSettingsWaitMenuEnabled();

	for (;;) {
		for (;;) {
			if (g_popsDisplayPaused != 0) {
				do {
					guest->hostPaused = 1U;
					do {
						sceDisplayWaitVblankStart();
					} while (g_popsDisplayPaused > 0);
					prevSlot = (int)guest->display.activeSlot;
				} while (g_popsDisplayPaused != 0);
			}
			if (remotePocsIsActive() == 0 && popsHostIsOverlayActive() == SCE_FALSE)
				break;
		}

		if (pspemuHasRearTouchCapability() != 0) {
			sceCtrlGetWirelessControllerInfo(&wirelessInfo);
			if (wirelessInfo.connected[2] != 0U || wirelessInfo.connected[3] != 0U)
				guest->multiTapMode = 0xFFU;
			else
				guest->multiTapMode = 0x80U;
		}

		sceDisplayWaitVblankStart();
		sceClibMemcpy_safe(&displayCopy,
		                   remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR, 0, 0),
		                   sizeof(displayCopy));
		guest->hostPaused = 0U;

		{
			PopsGuestControlBlock *ctrlDst =
				(PopsGuestControlBlock *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR, 0, 0);
			int port;
			for (port = 1; port < 5; port++) {
				if (sceCtrlPeekBufferNegative2(port, &ctrlData, 1) > 0) {
					pspemuSettingsApplyStickFilter((SceCtrlData *)&ctrlData);
					ctrlData.buttons &= ~(uint32_t)pspemuSettingsReadDs4TouchPad(port - 1);
					ctrlDst->padButtons[port - 1][0] = ctrlData.buttons;
					ctrlDst->padButtons[port - 1][1] =
						(uint32_t)ctrlData.lx |
						((uint32_t)ctrlData.ly << 8) |
						((uint32_t)ctrlData.rx << 16) |
						((uint32_t)ctrlData.ry << 24);
				}
			}
		}

		if (prevSlot != (int)displayCopy.activeSlot) {
			int curNtsc = ((displayCopy.slots[displayCopy.activeSlot & 1U].modeFlags & 0x20U) == 0U) ? 1 : 0;
			if (isNtscAudio != curNtsc) {
				isNtscAudio = curNtsc;
				sceAudioOutSetConfig(g_popsAudioPort, -1,
				                     curNtsc ? 44184 : 44100,
				                     -1);
			}
			if (remotePocsConsumeDirtyFlag() != 0)
				popsSetVmcSyncFlag(0, 2U);
			prevSlot = (int)displayCopy.activeSlot;
			if (g_popsMenuInfo.pocsAutoBoot != 1U)
				popsUpdateDisplayFrameBuf(&displayCopy, NULL, g_popsDisplayPreset);
		}
	}
	return 0;
}

/* 0x8102f878 */
int popsHostGetCurrentDisc(void)
{
	return (int)*(const uint8_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 164U, 2, 0);
}

/* 0x8102f88c */
int popsHostIsSingleDisc(void)
{
	return (int)*(const uint8_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 163U, 2, 0);
}

/* 0x8102f8a0 */
int popsHostIsAnalogDisabled(void)
{
	if (pspemuIsPocketStationEnabled() != 0 && remotePocsIsSavePending() != 0)
		return 0;
	return (int)*(const uint8_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 171U, 2, 0);
}

/* 0x8102f8c4 */
void *popsHostGetVmcBuffer(unsigned int slot)
{
	uint32_t guestAddr;

	if (slot > 1U)
		return NULL;
	guestAddr = *(const uint32_t *)remotefsTranslateGuestAddr(
		POPS_SHARED_STATE_PADDR + 420U + slot * 4U, 2, 0);
	if (guestAddr - 0x08800000U > 0x017DFF7FU)
		return NULL;
	return (uint8_t *)remotefsTranslateGuestAddr(guestAddr, 2, 0) + g_popsCdramOffset;
}

/* 0x8102f90c */
int popsHostGetDiscLoadSpeed(const void *popsConfig)
{
	uint16_t *dstTitle = (uint16_t *)(void *)popsConfig;
	const uint16_t *srcTitle =
		(const uint16_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 172U, 2, 0);
	uint8_t validFlag;

	sceClibMemcpy_safe(dstTitle, srcTitle, 0xF0U);
	if (dstTitle[0] == 0U)
		return 2;

	validFlag = *(const uint8_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 413U, 2, 0);
	if (validFlag == 2U) {
		uint32_t splitPos = *(const uint8_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 412U, 2, 0);
		if (splitPos >= 2U && splitPos <= 120U) {
			uint32_t insertIdx = splitPos - 1U;
			dstTitle[insertIdx] = 10U;
			sceClibMemcpy_safe(&dstTitle[insertIdx + 1U],
			                   &srcTitle[insertIdx + 1U],
			                   (120U - splitPos) * 2U);
		}
	} else if (validFlag == 0U) {
		dstTitle[0] = 0U;
	}

	return *(const uint8_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 161U, 2, 0) == 0U ? 1 : 0;
}

/* 0x8102f99c */
int popsHostReloadVmc(unsigned int slot)
{
	if (slot == 0U) {
		if (remotePocsHasMountedMedia() != 0)
			remotePocsNotifyVmcDirty();
	} else if (slot != 1U) {
		return (int)slot;
	}

	sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK, popsHostGetVmcBuffer(slot), POPS_VMC_SIZE_BYTES);
	popsSetVmcSyncFlag((int)slot, 1U);
	return 0;
}

/* 0x8102f9d4 */
int popsHostFinishPocsTransition(void)
{
	if (remotePocsHasMountedMedia() != 0) {
		popsSetVmcSyncFlag(0, 1U);
		pocsSyncRtcFromHost();
		g_popsDisplayPaused = 0;
		remotePocsSetEmulationActive(0);
		pocsSyncRtcFromHost();
		g_popsDisplayPaused = 1;
	}
	return 0;
}

/* 0x8102fa04 */
int popsHostSetScreenScale(int mode, int scale, uint16_t *outWidth, int16_t *outHeight)
{
	g_popsDisplayPreset = mode;
	g_popsBilinearFilter = scale;

	if (mode == 4 && outWidth != NULL && outHeight != NULL) {
		uint32_t width = outWidth[1];
		uint32_t height = outWidth[0];
		int32_t maxOffX;
		int32_t offX = outHeight[1];
		int32_t offY = -outHeight[0];

		if (width > 320U) {
			width = 320U;
			maxOffX = 0;
		} else {
			maxOffX = (int32_t)((320U - width) >> 1);
		}
		g_popsMenuInfo.customWidth = width;

		if (height > 480U)
			height = 480U;
		g_popsMenuInfo.customHeight = height;

		if (offX > maxOffX)
			offX = maxOffX;
		else if (offX < -maxOffX)
			offX = -maxOffX;
		g_popsMenuInfo.customOffsetX = offX;

		if (offY > 272)
			offY = 272;
		else if (offY < -272)
			offY = -272;
		g_popsMenuInfo.customOffsetY = offY;

		outWidth[0] = (uint16_t)height;
		outWidth[1] = (uint16_t)width;
		outHeight[0] = (int16_t)(-offY);
		outHeight[1] = (int16_t)offX;
	}
	return 0;
}

/* 0x8102fac4 */
int popsHostApplyMenuSettings(PspemuMenuSettings settings)
{
	PopsGuestControlBlock *guest =
		(PopsGuestControlBlock *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR, 0, 0);
	int idx;

	if (settings.resetRequest == 2U) {
		while (guest->audioPaused == 0U) {
			g_popsDisplayPaused = 1;
			sceDisplayWaitVblankStart();
		}
		popsSpuReset();
		pocsSpiSyncHost();
		guest->resetAction = 2U;
	} else if (settings.resetRequest == 1U) {
		guest->resetAction = 1U;
	}

	g_popsMenuInfo.controllerMode = (settings.controllerMode != 0U) ? (uint32_t)(settings.controllerMode - 1U) : 0U;
	g_popsMenuInfo.analogMode = (uint16_t)(settings.padModeFlags & 1U);
	g_popsMenuInfo.screenMode = settings.screenMode;
	g_popsMenuInfo.volumeLevel = settings.volumeLevel;
	g_popsMenuInfo.multiTapMode = settings.controllerPort;
	g_popsMenuInfo.bilinearFilter = settings.dispFilter;
	g_popsMenuInfo.displayPreset = settings.multiTapEnabled;
	g_popsMenuInfo.customAssignEnabled = settings.analogStickAssign;

	for (idx = 0; idx < 18; idx++)
		g_popsMenuInfo.buttonMap[idx] = settings.customButtonMap[idx];

	g_popsMenuInfo.screenLeftUp = settings.screenLeftUp;
	g_popsMenuInfo.screenRightUp = settings.screenRightUp;
	g_popsMenuInfo.screenLeftDown = settings.screenLeftDown;
	g_popsMenuInfo.screenRightDown = settings.screenRightDown;
	g_popsMenuInfo.rearTouchLeftUp = settings.rearTouchLeftUp;
	g_popsMenuInfo.rearTouchRightUp = settings.rearTouchRightUp;
	g_popsMenuInfo.rearTouchLeftDown = settings.rearTouchLeftDown;
	g_popsMenuInfo.rearTouchRightDown = settings.rearTouchRightDown;
	g_popsMenuInfo.dpadAssign = settings.dpadAssign;

	popsSyncMenuToGuest();
	g_popsMenuInfo.pocsMounted = (uint8_t)remotePocsHasMountedMedia();
	popsSaveMenuInfo();
	return 0;
}

/* 0x8102fbe4 */
int popsHostSetPocsBgTypeAndSave(uint8_t bgType)
{
	g_popsMenuInfo.pocsBgType = bgType;
	g_popsMenuInfo.pocsMounted = (uint8_t)remotePocsHasMountedMedia();
	g_popsMenuInfo.version = POPS_MENUINFO_VERSION;
	popsSaveMenuInfo();
	return 0;
}

/* 0x8102fc08 */
int popsHostInitForTitle(const char *titleId)
{
	int len = snprintf(g_popsMenuInfoPath, sizeof(g_popsMenuInfoPath),
	                   "ms0:PSP/GAME/%s/__sce_menuinfo", titleId);
	return (len == 37) ? 0 : -1;
}

/* 0x8102fc2c */
int popsHostHandlePocsFirstBoot(void)
{
	if (pspemuIsPocketStationLaunch() == 0 &&
	    g_popsMenuInfo.pocsFirstBootSeen == 0U &&
	    g_popsMenuInfo.pocsMounted == 0U) {
		uint8_t confirmCheck = 0U;
		uint8_t importCheck = 0U;
		int confirmRes = 0;
		int importRes = 0;

		pocsSyncRtcFromHost();
		g_popsDisplayPaused = 1;
		pspemuSettingsSetMenuEnabled(0);

		if (pspemuSettingsShowFirstBootConfirm(&confirmCheck, &confirmRes) >= 0 &&
		    confirmRes != 2) {
			if (confirmCheck == 1U) {
				g_popsMenuInfo.pocsMounted = 1U;
				remotePocsMountMediaFromMenu();
				if (pspemuMenuLoadVmcPreview(3U) <= 0) {
					int entryCount = pspemuMenuLoadVmcPreview(0U);
					if (entryCount > 0 &&
					    pspemuSettingsShowPocsConfirmDialog(&importCheck, &importRes) >= 0 &&
					    importRes != 2 &&
					    importCheck == 1U) {
						pspemuMenuImportVmcPreviewEntries(entryCount);
					}
					popsHostReloadVmc(0U);
				}
			}
			if (importRes != 2) {
				g_popsMenuInfo.pocsFirstBootSeen = 1U;
				popsSaveMenuInfo();
			}
		}

		pspemuSettingsSetMenuEnabled(1);
		pocsSyncRtcFromHost();
		g_popsDisplayPaused = 0;
	}
	return 0;
}

/* 0x8102fd10 */
int popsHostGetDiscCount(void)
{
	const uint8_t *guest =
		(const uint8_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR, 0, 0);
	int count = 3 * ((guest[29] & 1U) + ((guest[29] & 2U) != 0U ? 1 : 0)) + 2;
	int flags = (int)guest[168];
	int idx;

	for (idx = 0; idx < count; idx++)
		flags |= (int)guest[21 + idx] << 7;
	return flags;
}

/* 0x8102fd50 */
int popsHostInitSettings(PspemuMenuSettings *settings)
{
	const PopsGuestControlBlock *guest =
		(const PopsGuestControlBlock *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR, 0, 0);
	SceUID fd = sceIoOpen(g_popsMenuInfoPath, SCE_O_RDONLY, 0);
	uint32_t maxDiscs = (guest->discCount >= 1U && guest->discCount <= 5U) ? guest->discCount : 1U;
	int valid = 0;
	int idx;

	pspemuMenuSetScreenshotEnabled((uint8_t)maxDiscs);

	if (fd >= 0) {
		SceSSize bytesRead = sceIoRead(fd, &g_popsMenuInfo, sizeof(g_popsMenuInfo));
		sceIoClose(fd);

		if (g_popsMenuInfo.version <= 1U)
			popsResetTouchDefaults();
		if (g_popsMenuInfo.version <= 3U) {
			g_popsMenuInfo.pocsMounted = 0U;
			g_popsMenuInfo.pocsBgType = 0U;
			g_popsMenuInfo.pocsFirstBootSeen = 0U;
			g_popsMenuInfo.pocsDisplayChannelDefault = 0U;
			g_popsMenuInfo.pocsDisplayChannelAlt = 0U;
			g_popsMenuInfo.pocsDisplaySlot = 0U;
			g_popsMenuInfo.pocsAutoBoot = 0U;
		} else if (g_popsMenuInfo.version == 4U) {
			g_popsMenuInfo.pocsAutoBoot = 0U;
		}

		if (bytesRead == (SceSSize)sizeof(g_popsMenuInfo) &&
		    g_popsMenuInfo.magicLo == POPS_MENUINFO_MAGIC_LO &&
		    g_popsMenuInfo.magicHi == POPS_MENUINFO_MAGIC_HI &&
		    g_popsMenuInfo.version <= POPS_MENUINFO_VERSION &&
		    g_popsMenuInfo.controllerMode < maxDiscs &&
		    g_popsMenuInfo.screenMode <= 1U &&
		    g_popsMenuInfo.volumeLevel <= 2U &&
		    g_popsMenuInfo.multiTapMode <= 1U &&
		    g_popsMenuInfo.analogMode <= 1U &&
		    g_popsMenuInfo.bilinearFilter <= 1U &&
		    g_popsMenuInfo.displayPreset <= 4U) {
			int32_t maxOffX = (int32_t)((320U - g_popsMenuInfo.customWidth) >> 1);
			if (g_popsMenuInfo.customOffsetX >= -maxOffX &&
			    g_popsMenuInfo.customOffsetX <= maxOffX &&
			    g_popsMenuInfo.customOffsetY >= -272 &&
			    g_popsMenuInfo.customOffsetY <= 272 &&
			    g_popsMenuInfo.customWidth <= 320U &&
			    g_popsMenuInfo.customHeight <= 480U) {
				valid = 1;
				for (idx = 0; idx < 18; idx++) {
					if (g_popsMenuInfo.buttonMap[idx] > 18U) {
						valid = 0;
						break;
					}
				}
				if (valid) {
					if (g_popsMenuInfo.screenLeftUp > 16U ||
					    g_popsMenuInfo.screenRightUp > 16U ||
					    g_popsMenuInfo.screenLeftDown > 16U ||
					    g_popsMenuInfo.screenRightDown > 16U) {
						valid = 0;
					}
				}
				if (valid) {
					uint8_t rlu = g_popsMenuInfo.rearTouchLeftUp;
					uint8_t rru = g_popsMenuInfo.rearTouchRightUp;
					uint8_t rld = g_popsMenuInfo.rearTouchLeftDown;
					uint8_t rrd = g_popsMenuInfo.rearTouchRightDown;

					if (((rlu & 0xFBU) != 0x0BU && rlu != 13U && rlu != 0U) ||
					    ((rru & 0xFDU) != 0x0CU && (rru & 0xEFU) != 0U) ||
					    ((rld & 0xFBU) != 0x0BU && rld != 13U && rld != 0U) ||
					    ((rrd & 0xFDU) != 0x0CU && (rrd & 0xEFU) != 0U) ||
					    g_popsMenuInfo.pocsMounted > 1U ||
					    g_popsMenuInfo.pocsBgType > 2U ||
					    g_popsMenuInfo.pocsFirstBootSeen > 1U ||
					    g_popsMenuInfo.pocsDisplayChannelDefault > 3U ||
					    g_popsMenuInfo.pocsDisplayChannelAlt != 0U ||
					    g_popsMenuInfo.pocsDisplaySlot > 16U ||
					    g_popsMenuInfo.pocsAutoBoot > 1U) {
						valid = 0;
					}
				}
			}
		}
	}

	if (!valid) {
		sceClibMemset(&g_popsMenuInfo, 0, sizeof(g_popsMenuInfo));
		g_popsMenuInfo.magicLo = POPS_MENUINFO_MAGIC_LO;
		g_popsMenuInfo.magicHi = POPS_MENUINFO_MAGIC_HI;
		g_popsMenuInfo.version = POPS_MENUINFO_VERSION;
		g_popsMenuInfo.bilinearFilter = 1U;
		g_popsMenuInfo.customWidth = 80U;
		g_popsMenuInfo.customHeight = 120U;
		for (idx = 0; idx < 10; idx++)
			g_popsMenuInfo.buttonMap[idx] = (uint8_t)idx;
		popsResetTouchDefaults();
		g_popsMenuInfo.pocsMounted = 0U;
		g_popsMenuInfo.pocsBgType = 0U;
		g_popsMenuInfo.pocsFirstBootSeen = 0U;
		g_popsMenuInfo.pocsDisplayChannelDefault = 0U;
		g_popsMenuInfo.pocsDisplayChannelAlt = 0U;
		g_popsMenuInfo.pocsDisplaySlot = 0U;
		g_popsMenuInfo.pocsAutoBoot = 0U;
		popsSaveMenuInfo();
	}

	popsSyncMenuToGuest();

	settings->screenMode = (uint8_t)g_popsMenuInfo.screenMode;
	settings->controllerMode = (uint8_t)(g_popsMenuInfo.controllerMode + 1U);
	settings->volumeLevel = (uint8_t)g_popsMenuInfo.volumeLevel;
	settings->controllerPort = (uint8_t)g_popsMenuInfo.multiTapMode;
	settings->padModeFlags = (uint8_t)g_popsMenuInfo.analogMode;
	settings->dispFilter = (uint8_t)g_popsMenuInfo.bilinearFilter;
	settings->analogStickAssign = (uint8_t)g_popsMenuInfo.customAssignEnabled;

	for (idx = 0; idx < 18; idx++)
		settings->customButtonMap[idx] = g_popsMenuInfo.buttonMap[idx];
	settings->customButtonMap[18] = g_popsMenuInfo.screenLeftUp;
	settings->customButtonMap[19] = g_popsMenuInfo.screenRightUp;
	settings->customButtonMap[20] = g_popsMenuInfo.screenLeftDown;
	settings->customButtonMap[21] = g_popsMenuInfo.screenRightDown;
	settings->customButtonMap[22] = g_popsMenuInfo.rearTouchLeftUp;
	settings->customButtonMap[23] = g_popsMenuInfo.rearTouchRightUp;

	settings->screenLeftUp = g_popsMenuInfo.screenLeftUp;
	settings->screenRightUp = g_popsMenuInfo.screenRightUp;
	settings->screenLeftDown = g_popsMenuInfo.screenLeftDown;
	settings->screenRightDown = g_popsMenuInfo.screenRightDown;
	settings->rearTouchLeftUp = g_popsMenuInfo.rearTouchLeftUp;
	settings->rearTouchRightUp = g_popsMenuInfo.rearTouchRightUp;
	settings->rearTouchLeftDown = g_popsMenuInfo.rearTouchLeftDown;
	settings->rearTouchRightDown = g_popsMenuInfo.rearTouchRightDown;
	settings->dpadAssign = g_popsMenuInfo.dpadAssign;

	{
		uint16_t customDims[2];
		int16_t customPos[2];
		customDims[0] = (uint16_t)g_popsMenuInfo.customHeight;
		customDims[1] = (uint16_t)g_popsMenuInfo.customWidth;
		customPos[0] = (int16_t)(-g_popsMenuInfo.customOffsetY);
		customPos[1] = (int16_t)g_popsMenuInfo.customOffsetX;
		memcpy(&settings->trailingPad[3], customDims, sizeof(customDims));
		memcpy(&settings->trailingPad[7], customPos, sizeof(customPos));
	}

	if (pspemuIsPocketStationEnabled() == 0)
		return 0;

	remotePocsWaitSaveIdle();
	if (g_popsMenuInfo.pocsMounted == 1U) {
		remotePocsMountMediaFromMenu();
		popsHostReloadVmc(0U);
	}
	if (pspemuIsPocketStationLaunch() != 0) {
		remotePocsEnterMode();
		pocsSyncRtcFromHost();
		g_popsDisplayPaused = 0;
	} else {
		remotePocsStartCore();
		remotePocsNotifyDisplayReset();
	}
	remotePocsSetEmulationActive(1);
	return 0;
}

/* 0x810300d4 */
int popsHostSetPauseState(int paused)
{
	pocsSyncRtcFromHost();
	g_popsDisplayPaused = paused;
	return paused;
}

/* 0x810300ec */
int popsHostCaptureScreen(void *dstBuf, int forExport)
{
	PopsCaptureFrameBuf captureFb;
	PopsDisplayState displayCopy;
	int frameHeight;
	int preset;

	if (remotefsGetCdramBase() == NULL)
		return 0;

	sceClibMemset(&captureFb, 0, sizeof(captureFb));
	sceClibMemcpy_safe(&displayCopy,
	                   remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR, 0, 0),
	                   sizeof(displayCopy));

	frameHeight = ((displayCopy.slots[displayCopy.activeSlot & 1U].modeFlags & 8U) != 0U) ? 512 : 480;
	captureFb.size = sizeof(captureFb);
	captureFb.base = dstBuf;
	captureFb.pixelformat = 0U;

	if (forExport != 0) {
		captureFb.pitch = 640U;
		captureFb.width = 640U;
		captureFb.height = (uint32_t)frameHeight;
		sceDmacMemset(dstBuf, 0, (SceSize)(2560 * frameHeight));
		preset = -1;
	} else {
		captureFb.pitch = 1024U;
		captureFb.width = 960U;
		captureFb.height = 544U;
		sceDmacMemset(dstBuf, 0, 0x220000U);
		preset = g_popsDisplayPreset;
	}

	popsUpdateDisplayFrameBuf(&displayCopy, &captureFb, preset);
	return frameHeight;
}

/* 0x810301c4 */
uint32_t popsHostGetAnalogButtonMask(int index)
{
	uint8_t mapIdx;

	if ((uint32_t)index > 25U)
		return 0U;
	mapIdx = g_popsMenuInfo.buttonMap[index];
	if (mapIdx > 18U)
		return 0U;
	return g_popsButtonMasks[mapIdx];
}

/* 0x810301ec */
uint32_t popsHostMapButtons(uint32_t buttons)
{
	static const uint32_t srcBits[10] = {
		0x0010U, 0x0040U, 0x0080U, 0x0020U,
		0x1000U, 0x2000U, 0x4000U, 0x8000U,
		0x0400U, 0x0800U
	};
	uint32_t swapped;
	uint32_t mapped;
	int idx;

	if (pspemuHasRearTouchCapability() != 0)
		return buttons;

	swapped = ((buttons >> 2) & 0x0300U) | ((buttons << 2) & 0x0C00U) | (buttons & 0xF0FFU);
	if (g_popsMenuInfo.customAssignEnabled == 0U)
		return swapped;

	mapped = swapped & 9U;
	for (idx = 0; idx < 10; idx++) {
		uint8_t target = g_popsMenuInfo.buttonMap[idx];
		if ((swapped & srcBits[idx]) != 0U && target <= 18U)
			mapped |= g_popsButtonMasks[target];
	}
	return mapped;
}

/* 0x810303bc */
int popsHostClearPendingError(void)
{
	if (pspemuIsPocketStationEnabled() != 0) {
		remotePocsStopBackgroundTasks();
		remotePocsDisconnectAdhoc(1);
	}
	if (!pspemuSettingsIsMenuEnabled())
		return 0;

	if (pspemuIsPocketStationEnabled() != 0) {
		while (remotePocsIsSavePending() != 0)
			sceDisplayWaitVblankStart();
	}

	pocsSyncRtcFromHost();
	g_popsDisplayPaused = 1;
	return 0;
}

/* 0x81030404 */
int popsHostGetPendingError(void)
{
	if (remotefsGetCdramBase() == NULL)
		return POPS_ERR_NO_GUEST;
	return *(const int32_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 428U, 2, 0);
}

/* 0x81030428 */
int popsHostIsCustomScreenActive(void)
{
	return (int)*(const uint8_t *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR + 416U, 2, 0);
}

/* 0x81030440 */
int popsHostSetVibrationEnabled(int enabled)
{
	g_popsVibrationEnabled = enabled;
	return 0;
}

/* 0x81030450 */
int popsHostHandlePeripheralCmd(uint32_t port, uint32_t motorLevel)
{
	SceCtrlActuator actuator;

	sceClibMemset(&actuator, 0, sizeof(actuator));
	if (g_popsVibrationEnabled != 0) {
		actuator.small = (uint8_t)(motorLevel & 0xFFU);
		actuator.large = (uint8_t)((motorLevel >> 8) & 0xFFU);
	}
	return sceCtrlSetActuator((int)(port + 1U), &actuator);
}

/* 0x8103049c */
int popsHostIsMultiTapEnabled(void)
{
	return (int)g_popsMenuInfo.displayPreset;
}

/* 0x810304a8 */
int popsHostGetPocsBgType(void)
{
	return (int)g_popsMenuInfo.pocsBgType;
}

/* 0x810304b8 */
int popsHostSetPocsBgType(uint8_t bgType)
{
	g_popsMenuInfo.pocsBgType = bgType;
	return 0;
}

/* 0x810304c8 */
int popsHostGetPocsDisplayChannel(void)
{
	if (g_popsMenuInfo.pocsBgType == 0U)
		return (int)g_popsMenuInfo.pocsDisplayChannelDefault;
	if (g_popsMenuInfo.pocsBgType == 1U)
		return (int)g_popsMenuInfo.pocsDisplayChannelAlt;
	return 0;
}

/* 0x810304e8 */
int popsHostSetPocsDisplayChannel(uint8_t channel)
{
	if (g_popsMenuInfo.pocsBgType == 0U) {
		g_popsMenuInfo.pocsDisplayChannelDefault = channel;
	} else if (g_popsMenuInfo.pocsBgType == 1U) {
		g_popsMenuInfo.pocsDisplayChannelAlt = channel;
	} else {
		return 0;
	}

	if (g_popsMenuInfo.pocsDisplaySlot <= 15U)
		g_popsMenuInfo.pocsDisplaySlot++;
	popsSaveMenuInfo();
	return 0;
}

/* 0x81030524 */
int popsHostGetPocsDisplaySlot(void)
{
	return (int)g_popsMenuInfo.pocsDisplaySlot;
}

/* 0x81030534 */
int popsHostSetCdramOffset(int offset)
{
	g_popsCdramOffset = offset;
	return offset;
}

/* 0x81030540 */
int popsHostGetPocsAutoBoot(void)
{
	return (int)g_popsMenuInfo.pocsAutoBoot;
}

/* 0x81030550 */
uint8_t popsHostSetPocsAutoBoot(uint8_t enabled)
{
	g_popsMenuInfo.pocsAutoBoot = enabled;
	return enabled;
}

static inline int32_t popsSaturateHalfSample(int32_t sample)
{
	int32_t result;
	__asm__("ssat %0, #16, %1, asr #1" : "=r"(result) : "r"(sample));
	return result;
}

/* 0x81030560 */
static int popsAudioOutThread(SceSize args, void *argp)
{
	const PopsGuestControlBlock *guest =
		(const PopsGuestControlBlock *)remotefsTranslateGuestAddr(POPS_SHARED_STATE_PADDR, 0, 0);
	int port;
	int bufToggle = 0;

	(void)args;
	(void)argp;

	port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, 512, 44100, SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
	if (port < 0)
		return 0;

	g_popsAudioPort = port;
	for (;;) {
		int gain = g_popsAudioVolumeGain;
		uint32_t *outBuf = g_popsAudioRingBuf[bufToggle];
		uint32_t startUs = sceKernelGetProcessTimeLow();
		int spuFlags = g_popsSpuAudioFlags;
		int sampleIdx = 0;
		uint32_t *dst = outBuf;
		uint32_t *end = outBuf + 512;

		do {
			uint32_t packed = 0U;

			if (guest->audioPaused == 0U) {
				uint32_t rawSample = popsSpuMixSample();
				int32_t left = popsSaturateHalfSample(gain * (int16_t)(rawSample & 0xFFFFU));
				int32_t right = popsSaturateHalfSample(gain * (int16_t)((int32_t)rawSample >> 16));
				packed = (uint32_t)(uint16_t)left | ((uint32_t)right << 16);
			}
			*dst++ = packed;

			if (spuFlags != 0 && (sampleIdx == 256 || spuFlags <= 2)) {
				int32_t waitUs = (int32_t)(startUs + (uint32_t)(10000 * sampleIdx) / 441U -
				                           sceKernelGetProcessTimeLow());
				if (waitUs > 1100)
					sceKernelDelayThread((SceUInt)(waitUs - 1000));
				spuFlags = g_popsSpuAudioFlags;
			}
			sampleIdx++;
		} while (dst != end);

		bufToggle ^= 1;
		sceAudioOutOutput(port, outBuf);
	}
}

/* 0x8103067c */
int popsSetAudioVolume(int volumeLevel)
{
	g_popsAudioVolumeGain = volumeLevel + 2;
	return 0;
}

/* 0x8103068c */
int popsHostInit(void)
{
	SceUID audioThid;
	SceUID dispThid;
	int ret;

	popsSpuReset();

	audioThid = sceKernelCreateThread("ScePopsAudioOut",
	                                  popsAudioOutThread,
	                                  64,
	                                  0x8000U,
	                                  0U,
	                                  SCE_KERNEL_CPU_MASK_USER_1,
	                                  NULL);
	if (audioThid < 0)
		return audioThid;

	ret = sceKernelStartThread(audioThid, 0U, NULL);
	if (ret < 0)
		return ret;

	dispThid = sceKernelCreateThread("ScePopsDisplay",
	                                 popsDisplayThread,
	                                 125,
	                                 0x8000U,
	                                 0U,
	                                 SCE_KERNEL_CPU_MASK_USER_2,
	                                 NULL);
	ret = sceKernelStartThread(dispThid, 0U, NULL);
	return (ret < 0) ? ret : 0;
}
