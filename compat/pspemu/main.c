/*
 * main.c: ScePspemu entry point, lifecycle, suspend/resume handshake,
 * STARTDAT splash loader, compatibility memory patches, title list sync,
 * and main event loop.
 */
#include "host_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Module state */
int g_bootLockState = -1;

uint8_t *g_activeCdram;
SceUID g_errDlgSema;
int g_isPocketStationLaunch;
uint8_t *g_uncachedCdram;
uint8_t *g_frameBuf;
int g_isPs1Mode;
int g_powerLockCount;
int g_useTempFrameBuf;
int g_isPocketStationTitle;
uint8_t *g_frameTempBuf;

SceKernelLwMutexWork g_lockWork[3];
int64_t g_heartbeatStallStart;
uint32_t g_splashShownTime;
uint8_t *g_cachedCdram;
volatile SceInt8 g_fatalTriggered;
int g_fatalErrorCode;
int g_rebootReady;

char g_popsDiscName[64];
char g_titleListBuf[PSPEMU_TITLE_LIST_SIZE];
int g_lastHeartbeat;
char g_syncTitleListBuf[PSPEMU_TITLE_LIST_SIZE];
uint8_t *g_mcWorkBuf;
volatile SceInt8 g_dialogInited;
int g_lastPowerTickSeq;
char g_popsTitleName[64];
int g_psButtonIntercepted;
int g_hasPocketStationApp;

static const char *const g_lockNames[3] = {
	"ScePspemuPowerLock",
	"ScePspemuBootLock",
	"ScePspemuRebootLock",
};

enum {
	PSPEMU_LOCK_POWER = 0,
	PSPEMU_LOCK_BOOT = 1,
	PSPEMU_LOCK_REBOOT = 2,
};

static inline uint32_t *pspemuRamWords(uint32_t offset)
{
	return (uint32_t *)(void *)(g_uncachedCdram + offset);
}

static inline __attribute__((always_inline)) void pspemuLockOrDie(int lockKind)
{
	int res = sceKernelLockLwMutex(&g_lockWork[lockKind], 1, NULL);
	if (res < 0)
		pspemuTriggerFatalError(res);
}

static inline __attribute__((always_inline)) void pspemuUnlockOrDie(int lockKind)
{
	int res = sceKernelUnlockLwMutex(&g_lockWork[lockKind], 1);
	if (res < 0)
		pspemuTriggerFatalError(res);
}

/* 0x81003a2c: writes a 10-word MIPS thread-delay compatibility stub into PSP RAM */
static __attribute__((noinline)) void pspemuWriteThreadDelayPatchStub(
	uint32_t pspOffset, uint32_t firstJal, uint32_t secondJal)
{
	uint32_t *code = pspemuRamWords(pspOffset);

	code[0] = 0xAFA8000CU;
	code[1] = 0x1540000AU;
	code[2] = 0x8E04FA84U;
	code[3] = 0x00803025U;
	code[4] = 0x00A02025U;
	code[5] = 0x00002825U;
	code[6] = firstJal;
	code[7] = 0x03A03825U;
	code[8] = secondJal;
	code[9] = 0x24042710U;
}

/* 0x81003a98: patches a 6-word MIPS load sequence in-place */
static __attribute__((noinline)) void pspemuPatchMipsLoadPairStub(uint32_t *patchSite)
{
	const uint16_t *halfWords = (const uint16_t *)patchSite;
	uint32_t luiImm = halfWords[0];
	uint32_t lwFirstImm = halfWords[2];
	uint32_t shiftedOp = patchSite[2];
	uint32_t movedOp = patchSite[3];
	uint32_t lwSecondImm = halfWords[10];

	patchSite[0] = 0x3C100000U | luiImm;
	patchSite[1] = 0x8E020000U | lwFirstImm;
	patchSite[2] = 0x2406002BU;
	patchSite[3] = shiftedOp + 0x222U;
	patchSite[4] = movedOp;
	patchSite[5] = 0x8E030000U | lwSecondImm;
}

/* 0x81003ad8: writes a 13-word MIPS relocated-call stub into PSP RAM */
static __attribute__((noinline)) void pspemuWriteRelocatedCallStub(uint32_t pspOffset)
{
	uint32_t *code = pspemuRamWords(pspOffset);
	uint32_t firstTarget = (pspOffset + 0x08018B78U) >> 2;
	uint32_t secondTarget = (pspOffset + 0x08018BB0U) >> 2;

	code[0]  = 0x0A2001EEU;
	code[1]  = 0x00000000U;
	code[2]  = 0x0C000000U | firstTarget;
	code[3]  = 0x001D2821U;
	code[4]  = 0x10400006U;
	code[5]  = 0x00000000U;
	code[6]  = 0x0C000000U | secondTarget;
	code[7]  = 0x24040100U;
	code[8]  = 0x27A60004U;
	code[9]  = 0x1000FFF8U;
	code[10] = 0x00002021U;
	code[12] = 0x00000000U;
}

typedef struct PspemuTitleIdChars {
	char chars[PSPEMU_TITLE_ID_LEN];
} PspemuTitleIdChars;

/* 0x81003b64: upgrades 10-byte legacy title_list.bin entries to 12-byte format */
static __attribute__((noinline, noclone)) void pspemuUpgradeTitleListFormat(char *titleList)
{
	char legacyCopy[PSPEMU_TITLE_LIST_SIZE];
	PspemuTitleRecord *dst = (PspemuTitleRecord *)titleList;
	const PspemuLegacyTitleRecord *src = (const PspemuLegacyTitleRecord *)legacyCopy;
	const PspemuLegacyTitleRecord *srcEnd = src + PSPEMU_TITLE_LIST_COUNT;

	memcpy(legacyCopy, titleList, sizeof(legacyCopy));
	memset(titleList, 0, PSPEMU_TITLE_LIST_SIZE);

	*(PspemuTitleIdChars *)dst->titleId = *(const PspemuTitleIdChars *)src->titleId;
	dst->flagsChar = '0';
	dst->statusChar = '0';
	src++;
	dst++;

	while (src != srcEnd) {
		dst[-1].separator = ',';
		*(PspemuTitleIdChars *)dst->titleId = *(const PspemuTitleIdChars *)src->titleId;
		dst->flagsChar = '0';
		dst->statusChar = '0';
		src++;
		dst++;
	}
}

/* 0x81003bf8: opens the current boot PBP and validates its 40-byte header */
static SceUID pspemuOpenBootPbp(PspPbpHeader *outHeader)
{
	SceUID fd;

	if (g_originalPath[0] == '\0')
		return PSPEMU_ERROR_INVALID_ARG;

	fd = sceIoOpen(g_originalPath, SCE_O_RDONLY, 0);
	if (fd < 0)
		return fd;

	if (sceIoRead(fd, outHeader, sizeof(*outHeader)) != (SceSSize)sizeof(*outHeader)) {
		sceIoClose(fd);
		return PSPEMU_ERROR_CORRUPTED;
	}

	if (outHeader->magic != 0x50425000U) {
		sceIoClose(fd);
		return PSPEMU_ERROR_CORRUPTED;
	}

	return fd;
}

/* 0x81003c5c: initializes settings and menu dialogs exactly once */
static __attribute__((noinline)) int pspemuInitDialogOnce(void)
{
	int res;

	if (sceKernelAtomicGetAndSet8(&g_dialogInited, 1) != 0)
		return 0;

	res = pspemuSettingsDialogInit();
	if (res == 0) {
		res = pspemuMenuDialogInit();
		if (res == 0)
			return pspemuMenuDialogSetup();
	}
	return res;
}

/* 0x81003c88: reads a splash image from the PBP's STARTDAT container */
static __attribute__((noinline, noclone)) int pspemuReadStartDatImage(
	int imageIndex, void *outBuf, SceSize *outSize, void *outDiscMeta)
{
	PspPbpHeader pbp;
	PspStartDatHeader startDat;
	PsImgHeader psImg;
	SceOff startDatOff;
	SceUID fd;
	int err = PSPEMU_ERROR_CORRUPTED;

	fd = pspemuOpenBootPbp(&pbp);
	if (fd < 0)
		return fd;

	do {
		SceOff imageOff;
		SceSize imageSize;

		if (g_isPs1Mode == 0) {
			startDatOff = (SceOff)(pbp.dataPspOffset + 0x5A0U);
		} else if (g_isPs1Mode == 1) {
			if (sceIoPread(fd, &psImg, sizeof(psImg), (SceOff)pbp.dataPsarOffset) != (SceSSize)sizeof(psImg))
				break;
			if (outDiscMeta == NULL) {
				err = PSPEMU_ERROR_FAULT;
				break;
			}
			if (psImg.singleDisc.magicWords[0] == 0x53495350U) {
				if (psImg.singleDisc.magicWords[1] != 0x474D494FU ||
				    psImg.singleDisc.magicWords[2] != 0x30303030U)
					break;
				startDatOff = (((SceOff)psImg.singleDisc.startDatOffsetHi) << 32)
					+ (SceOff)psImg.singleDisc.startDatOffsetLo
					+ (SceOff)pbp.dataPsarOffset;
				memset(outDiscMeta, 0, 16);
			} else if (psImg.multiDisc.magicWords[0] == 0x49545350U &&
			           psImg.multiDisc.magicWords[1] == 0x49454C54U &&
			           psImg.multiDisc.magicWords[2] == 0x3030474DU &&
			           psImg.multiDisc.magicWords[3] == 0x30303030U) {
				startDatOff = (((SceOff)psImg.multiDisc.startDatOffsetHi) << 32)
					+ (SceOff)psImg.multiDisc.startDatOffsetLo
					+ (SceOff)pbp.dataPsarOffset;
				memcpy(outDiscMeta, psImg.multiDisc.discMeta, 16);
			} else {
				break;
			}
		} else {
			err = PSPEMU_ERROR_INVALID_ARG;
			break;
		}

		if (sceIoPread(fd, &startDat, sizeof(startDat), startDatOff) != (SceSSize)sizeof(startDat) ||
		    memcmp(startDat.magic, "STARTDAT", 8) != 0 ||
		    startDat.imageCount == 0 ||
		    startDat.imageCount < (uint32_t)(imageIndex + 1)) {
			break;
		}

		imageOff = startDatOff + (SceOff)startDat.images[imageIndex].offset;
		imageSize = startDat.images[imageIndex].size;
		if (imageOff < 1 || imageSize == 0 || imageSize > 0x80000U)
			break;

		if (sceIoPread(fd, outBuf, imageSize, imageOff) != (SceSSize)imageSize)
			break;

		sceIoClose(fd);
		if ((int)imageSize < 0)
			return (int)imageSize;

		*outSize = imageSize;
		return (int)startDat.imageCount;
	} while (0);

	sceIoClose(fd);
	return err;
}

/* 0x81003ea0 */
int pspemuIsPocketStationLaunch(void)
{
	return g_isPocketStationLaunch;
}

/* 0x81003eac */
int pspemuFinishSplashAndLockModules(void)
{
	SceCtrlData padData;

	scePowerSetArmClockFrequency(333);
	if (g_isPs1Mode == 1) {
		do {
			if (sceKernelGetProcessTimeLow() - g_splashShownTime >= 6000000U)
				break;
			sceDisplayWaitVblankStart();
			sceCtrlPeekBufferPositive(0, &padData, 1);
		} while (padData.buttons == 0);
	}

	sceDmacMemset(g_frameTempBuf, 0, PSPEMU_FRAMEBUF_CLEAR_SIZE);
	return sceKernelInhibitLoadingModule(32);
}

/* 0x81003f30 */
int pspemuDrawDebugColorSquare(uint32_t color, int columnOffset)
{
	uint32_t *frameWords = (uint32_t *)(void *)g_frameTempBuf;
	uint32_t *colStart = frameWords + columnOffset;
	uint32_t *colEnd = colStart + 32;
	uint32_t *colPtr;

	for (colPtr = colStart; colPtr != colEnd; colPtr++) {
		uint32_t rowOffset;

		for (rowOffset = 0; rowOffset != 0x4000; rowOffset += PSPEMU_SCREEN_PITCH)
			colPtr[rowOffset] = color;
	}
	return sceKernelDelayThread(1000000);
}

/* 0x81003f68 */
int pspemuIsCachedCdramAddress(uintptr_t addr)
{
	uintptr_t base = (uintptr_t)g_cachedCdram;

	if (base == 0)
		return 0;
	if (addr < base)
		return 0;
	return addr < base + PSPEMU_RAM_CDRAM_SIZE;
}

/* 0x81003f90 */
void *pspemuGetActiveFrameBuffer(void)
{
	if (g_useTempFrameBuf != 0)
		return g_frameTempBuf;
	return g_frameBuf;
}

/* 0x81003fb4 */
int pspemuSetUseTempFrameBuffer(int useTemp)
{
	if (g_isPs1Mode == 1)
		useTemp = 1;
	g_useTempFrameBuf = useTemp;
	return useTemp;
}

/* 0x81003fd0 */
int pspemuConfirmPocketStationFirstBoot(void)
{
	int confirmed = 0;

	if (g_isPocketStationLaunch != 0 && pspemuSettingsGetPocsFirstBootDone() == 0) {
		int buttonResult = 0;

		pspemuSettingsSetMenuEnabled(0);
		pspemuSettingsShowPocsFirstBootDialog(&buttonResult);
		if (buttonResult == 2)
			confirmed = 1;
		else
			pspemuSettingsMarkPocsFirstBootDone();
		pspemuSettingsSetMenuEnabled(1);
	}
	return confirmed;
}

/* 0x81004030 */
int pspemuIsPocketStationEnabled(void)
{
	return g_isPocketStationTitle;
}

/* 0x8100403c */
int pspemuIsPocketStationInstalled(void)
{
	return g_hasPocketStationApp;
}

/* 0x81004048 */
int pspemuIsPs1Mode(void)
{
	return g_isPs1Mode;
}

/* 0x81004054 */
int pspemuLoadSysmodules(void)
{
	int res;

	res = sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);
	if (res != 0)
		return res;
	res = sceSysmoduleLoadModule(SCE_SYSMODULE_SCREEN_SHOT);
	if (res != 0)
		return res;
	res = sceSysmoduleLoadModule(SCE_SYSMODULE_PHOTO_EXPORT);
	if (res != 0)
		return res;

	if (g_isPs1Mode == 1) {
		res = sceSysmoduleLoadModule(SCE_SYSMODULE_NET_ADHOC_MATCHING);
		return res < 0 ? res : 0;
	}

	res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_MUSIC_EXPORT);
	if (res != 0)
		return res;
	res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_VIDEO_EXPORT);
	if (res != 0)
		return res;
	res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_AUDIOCODEC);
	if (res != 0)
		return res;
	res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_G729);
	if (res != 0)
		return res;
	res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_JPEG_ARM);
	if (res != 0)
		return res;
	res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM);
	if (res != 0)
		return res;
	res = sceSysmoduleLoadModule(SCE_SYSMODULE_LOCATION);
	if (res != 0)
		return res;
	res = sceSysmoduleLoadModule(SCE_SYSMODULE_LOCATION_EXTENSION);
	if (res != 0)
		return res;
	return sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_LOCATION_INTERNAL);
}

/* 0x81004104 */
void pspemuTriggerFatalError(int errorCode)
{
	if (sceKernelAtomicGetAndSet8(&g_fatalTriggered, 1) == 0) {
		g_fatalErrorCode = errorCode;
		sceKernelSignalSema(g_errDlgSema, 1);
	}
	for (;;)
		sceKernelDelayThread(1000000);
}

/* 0x81004140: periodic MD5 check on PSP guest memory */
static int pspemuMemoryCheckThread(SceSize args, void *argp)
{
	uint32_t checkSize;
	uint32_t expectedPrefix;
	uint8_t digest[16];

	(void)args;
	(void)argp;

	checkSize = (uint32_t)pspemuCompatGetParam(16);
	expectedPrefix = (uint32_t)pspemuCompatGetParam(17);
	if (checkSize <= PSPEMU_MEMCHECK_MAX_SIZE) {
		uint32_t expectedWord;

		srand(sceKernelGetProcessTimeLow());
		do {
			uint32_t delayUsec = (uint32_t)(rand() % 240000000) + 180000000U;
			void *guestMem;

			sceKernelDelayThread(delayUsec);
			expectedWord = expectedPrefix;
			guestMem = remotefsTranslateGuestAddr(PSPEMU_GUEST_USER_BASE, 1, checkSize);
			sceMd5Digest(guestMem, checkSize, digest);
		} while (memcmp(digest, &expectedWord, sizeof(expectedWord)) == 0);

		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	}
	return 0;
}

/* 0x810041e4 */
int pspemuIsRebootReady(void)
{
	int res = sceKernelLockLwMutex(&g_lockWork[PSPEMU_LOCK_REBOOT], 1, NULL);
	if (res >= 0) {
		int ready = g_rebootReady;
		res = sceKernelUnlockLwMutex(&g_lockWork[PSPEMU_LOCK_REBOOT], 1);
		if (res >= 0)
			return ready;
	}
	pspemuTriggerFatalError(res);
}

/* 0x81004220 */
int pspemuSetRebootReady(void)
{
	int res = sceKernelLockLwMutex(&g_lockWork[PSPEMU_LOCK_REBOOT], 1, NULL);
	if (res >= 0) {
		g_rebootReady = 1;
		res = sceKernelUnlockLwMutex(&g_lockWork[PSPEMU_LOCK_REBOOT], 1);
		if (res >= 0)
			return res;
	}
	pspemuTriggerFatalError(res);
}

/* 0x8100425c: suspends Vita-side services before guest suspend (0.990: suspendSC) */
static int suspendSC(void)
{
	int attempts;
	int res;

	while (remotePeripheralIsBusy() != 0)
		sceKernelDelayThread(1000);

	for (attempts = 10000; attempts != 0; attempts--) {
		int lockCount;

		res = sceKernelLockLwMutex(&g_lockWork[PSPEMU_LOCK_POWER], 1, NULL);
		if (res < 0)
			pspemuTriggerFatalError(res);
		lockCount = g_powerLockCount;
		res = sceKernelUnlockLwMutex(&g_lockWork[PSPEMU_LOCK_POWER], 1);
		if (res < 0)
			pspemuTriggerFatalError(res);
		if (lockCount <= 0)
			break;
		sceKernelDelayThread(1000);
	}

	res = sceKernelLockLwMutex(&g_lockWork[PSPEMU_LOCK_REBOOT], 1, NULL);
	if (res < 0)
		pspemuTriggerFatalError(res);
	g_rebootReady = 0;
	res = sceKernelUnlockLwMutex(&g_lockWork[PSPEMU_LOCK_REBOOT], 1);
	if (res < 0)
		pspemuTriggerFatalError(res);

	if (g_isPs1Mode == 1) {
		popsHostSuspend();
	} else {
		remoteAudioOutSuspend(1);
		remoteNetSuspend();
		remoteMsfsSuspendReset();
		remoteUsbPspcmSuspend();
	}
	return 0;
}

/* 0x81004318 */
int pspemuPowerUnlock(void)
{
	int res = sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
	if (res == 0) {
		pspemuLockOrDie(PSPEMU_LOCK_POWER);
		g_powerLockCount--;
		pspemuUnlockOrDie(PSPEMU_LOCK_POWER);
	}
	return res;
}

/* 0x81004360 */
int pspemuPowerLock(void)
{
	int res = sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
	if (res == 0) {
		pspemuLockOrDie(PSPEMU_LOCK_POWER);
		g_powerLockCount++;
		pspemuUnlockOrDie(PSPEMU_LOCK_POWER);
	}
	return res;
}

/* 0x810043a8: ScePspemuErrDlg thread (and direct early-exit helper) */
static int pspemuErrDlgThread(SceSize immediateErr, void *argp)
{
	int lockCount;
	int uninitRes;

	(void)argp;

	if (immediateErr != 0)
		g_fatalErrorCode = (int)immediateErr;
	else
		sceKernelWaitSema(g_errDlgSema, 1, NULL);

	remoteNetSuspend();
	sceCompatStop();
	uninitRes = sceCompatUninit();
	if (g_fatalErrorCode == 0)
		g_fatalErrorCode = uninitRes;

	if (immediateErr == 0)
		pspemuLockOrDie(PSPEMU_LOCK_POWER);

	for (lockCount = g_powerLockCount; lockCount > 0; lockCount--)
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
	g_powerLockCount = lockCount;

	if (immediateErr == 0)
		pspemuUnlockOrDie(PSPEMU_LOCK_POWER);

	if (g_fatalErrorCode != 0) {
		remoteLowPriorityUnlockPsButton();
		remoteLowPriorityUnlockPsButton();
		pspemuInitDialogOnce();
		pspemuSettingsShowErrorDialog(g_fatalErrorCode);
	}

	sceAppMgrExitToLiveboardForGameApp();
	return 0;
}

/* 0x81004460: application entry point */
int main(int argc, char **argv)
{
	SceUID errThreadId;
	SceUID mainThreadId;
	SceUID titleInfoThreadId;
	struct SceAppMgrAppEvent appEvent;
	PspemuMainThreadArg mainArg;
	int eventCount;
	int eventIndex;
	int lockIndex;
	int exitStatus;
	int res;

	(void)argc;
	(void)argv;

	scePowerSetArmClockFrequency(444);

	res = SceShellUtil_EC5881A5();
	if (res < 0)
		pspemuErrDlgThread((SceSize)res, NULL);

	res = SceShellUtil_CE35B2B8(NULL, NULL);
	if (res < 0)
		pspemuErrDlgThread((SceSize)res, NULL);

	res = sceAppMgrAcquireBgmPort();
	if (res < 0)
		pspemuErrDlgThread((SceSize)res, NULL);

	g_errDlgSema = sceKernelCreateSema("ScePspemuErrDlg", 0, 0, 1, NULL);
	if (g_errDlgSema < 0)
		pspemuErrDlgThread((SceSize)g_errDlgSema, NULL);

	errThreadId = sceKernelCreateThread(
		"ScePspemuErrDlg",
		pspemuErrDlgThread,
		64,
		0x1000,
		0,
		SCE_KERNEL_CPU_MASK_USER_0,
		NULL);
	if (errThreadId < 0)
		pspemuErrDlgThread((SceSize)errThreadId, NULL);

	res = sceKernelStartThread(errThreadId, 0, NULL);
	if (res != 0)
		pspemuErrDlgThread((SceSize)res, NULL);

	for (lockIndex = 0; lockIndex < 3; lockIndex++) {
		res = sceKernelCreateLwMutex(&g_lockWork[lockIndex], g_lockNames[lockIndex], 0, 0, NULL);
		if (res < 0)
			pspemuTriggerFatalError(res);
	}

	res = remoteMsfsPreInit();
	if (res != 0)
		pspemuTriggerFatalError(res);

	memset(&mainArg, 0, sizeof(mainArg));

	for (;;) {
		do {
			while (sceAppMgrReceiveEventNum(&eventCount) != 0)
				;
		} while (eventCount <= 0);

		for (eventIndex = 0; eventIndex < eventCount; eventIndex++) {
			if (sceAppMgrReceiveEvent(&appEvent) != 0)
				continue;

			if (appEvent.eventId == PSPEMU_APPMGR_EVENT_QUIT)
				pspemuTriggerFatalError(PSPEMU_ERROR_QUIT_REQUEST);

			if (appEvent.eventId == PSPEMU_APPMGR_EVENT_APP_PARAM) {
				res = remoteMsfsParseAppParam(
					mainArg.gameInfo.sfoProductString,
					&mainArg.gameInfo.sfoParentalLevel,
					mainArg.gameInfo.sfoGameDataId,
					mainArg.gameInfo.sfoAppVer,
					&mainArg.gameInfo.sfoBootable,
					&g_isPs1Mode,
					&g_isPocketStationLaunch);
				if (res != 0)
					pspemuTriggerFatalError(res);

				titleInfoThreadId = sceKernelCreateThread(
					"ScePspemuTitleInfo",
					pspemuTitleInfoThread,
					96,
					0x5000,
					0,
					SCE_KERNEL_CPU_MASK_USER_2,
					NULL);
				mainArg.titleInfoThreadId = titleInfoThreadId;
				if (titleInfoThreadId < 0)
					pspemuTriggerFatalError(titleInfoThreadId);

				res = sceKernelStartThread(titleInfoThreadId, 0, NULL);
				if (res != 0)
					pspemuTriggerFatalError(res);

				mainThreadId = sceKernelCreateThread(
					"ScePspemuMain",
					sceCompatMain,
					97,
					0x5000,
					0,
					SCE_KERNEL_CPU_MASK_USER_0,
					NULL);
				if (mainThreadId < 0)
					pspemuTriggerFatalError(mainThreadId);

				res = sceKernelStartThread(mainThreadId, sizeof(mainArg), &mainArg);
				if (res != 0)
					pspemuTriggerFatalError(res);

				res = sceKernelWaitThreadEnd(mainThreadId, &exitStatus, NULL);
				if (res < 0)
					exitStatus = res;
				pspemuTriggerFatalError(exitStatus);
			}
		}
	}
}

/* 0x8100465c: ScePspemuPrepareSuspend thread */
static int pspemuPrepareSuspendThread(SceSize args, void *argp)
{
	SceUID prepareSema;
	SceUID preparedSema;
	int res;

	(void)args;
	(void)argp;

	prepareSema = sceKernelCreateSema("ScePspemuPrepareSuspend", 0, 0, 1, NULL);
	if (prepareSema < 0)
		return prepareSema;

	preparedSema = sceKernelCreateSema("ScePspemuPreparedSuspend", 0, 0, 1, NULL);
	if (preparedSema < 0)
		return preparedSema;

	sceCompatSetSuspendSema(prepareSema, preparedSema);

	for (;;) {
		int status;

		res = sceKernelWaitSema(prepareSema, 1, NULL);
		if (res != 0)
			break;

		pspemuSettingsSetMenuEnabled(0);

		if (sceCompatGetStatus() == PSPEMU_COMPAT_STATUS_SUSPENDED) {
			int lockCount;

			pspemuLockOrDie(PSPEMU_LOCK_POWER);
			lockCount = g_powerLockCount;
			pspemuUnlockOrDie(PSPEMU_LOCK_POWER);

			if (lockCount > 0) {
				int attempts;

				for (attempts = 10000; attempts != 0; attempts--) {
					if (sceCompatGetStatus() == PSPEMU_COMPAT_STATUS_RUNNING)
						break;
					sceKernelDelayThread(1000);
				}
			}
		}

		status = sceCompatGetStatus();
		if (status == PSPEMU_COMPAT_STATUS_RUNNING) {
			res = suspendSC();
			if (res != 0)
				break;

			if (g_frameTempBuf != NULL && g_isPs1Mode != 1) {
				sceCompatLCDCSync();
				memcpy(g_frameTempBuf, g_frameBuf, PSPEMU_FRAMEBUF_CLEAR_SIZE);
				res = sceKernelSignalSema(preparedSema, 1);
				if (res != 0)
					break;
				continue;
			}
		} else if (status == PSPEMU_COMPAT_STATUS_STOPPED) {
			pspemuLockOrDie(PSPEMU_LOCK_BOOT);
			if (g_bootLockState == 0) {
				g_bootLockState = 1;
				pspemuUnlockOrDie(PSPEMU_LOCK_BOOT);
			} else if (g_bootLockState == 1) {
				pspemuUnlockOrDie(PSPEMU_LOCK_BOOT);
				res = suspendSC();
				if (res != 0)
					break;
			} else {
				pspemuUnlockOrDie(PSPEMU_LOCK_BOOT);
			}
		}

		res = sceKernelSignalSema(preparedSema, 1);
		if (res != 0)
			break;
	}

	sceKernelCancelSema(preparedSema, 1, NULL);
	sceKernelDeleteSema(prepareSema);
	sceKernelDeleteSema(preparedSema);
	return res;
}

/* 0x81004814 */
int pspemuRequestSuspend(void)
{
	remoteNetSuspend();
	return scePowerRequestSuspend();
}

/* 0x81004820..0x8100501c: per-title MIPS compatibility patches */
void pspemuApplyTitlePatchAudioDelayAlt(void)
{
	pspemuWriteThreadDelayPatchStub(0x00AC02ECU, 0x0E30BB37U, 0x0E30BB6FU);
}

void pspemuApplyTitlePatchAudioDelay(void)
{
	pspemuWriteThreadDelayPatchStub(0x00AC0410U, 0x0E30BBBFU, 0x0E30BBF7U);
}

void pspemuApplyTitlePatchReturnZero(void)
{
	uint32_t *code = pspemuRamWords(0x00924B74U);

	code[0] = 0x24080001U;
	code[1] = 0xA0880004U;
	code[2] = 0x24040000U;
}

void pspemuApplyTitlePatchReturnZeroAlt(void)
{
	uint32_t *code = pspemuRamWords(0x00925544U);

	code[0] = 0x24080001U;
	code[1] = 0xA0880004U;
	code[2] = 0x24040000U;
}

void pspemuApplyTitlePatchClearCallbacks(void)
{
	uint32_t *ram = pspemuRamWords(0);

	ram[0x0089E0E4U >> 2] = 0x24040100U;
	ram[0x0089F578U >> 2] = 0x24040100U;
	ram[0x0089FED4U >> 2] = 0x24040100U;
	ram[0x0089FF34U >> 2] = 0x24040100U;
	ram[0x0089FF90U >> 2] = 0x24040100U;
	ram[0x008A00D0U >> 2] = 0x24040100U;
	ram[0x008A16D0U >> 2] = 0x24040100U;
	ram[0x008A17A8U >> 2] = 0x24040100U;
	ram[0x008A1818U >> 2] = 0x24040100U;
	ram[0x008A1860U >> 2] = 0x24040100U;
	ram[0x008A3680U >> 2] = 0x24040100U;
}

void pspemuApplyTitlePatchMemoryHook(void)
{
	uint32_t *code = pspemuRamWords(0x008281D4U);

	code[0]  = 0x24060080U;
	code[2]  = 0x2E030080U;
	code[3]  = 0x0003100AU;
	code[4]  = 0xA0400020U;
	code[5]  = 0x3C0208A6U;
	code[6]  = 0x8C44E278U;
	code[7]  = 0x00000000U;
	code[8]  = 0x0E22428CU;
	code[9]  = 0x27A50020U;
	code[10] = 0x00520018U;
	code[11] = 0x00001812U;
	code[12] = 0x00031083U;
	pspemuPatchMipsLoadPairStub(pspemuRamWords(0x0087F128U));
}

void pspemuApplyTitlePatchFrameSync(void)
{
	uint32_t *code = pspemuRamWords(0x008E7B08U);

	code[0] = 0x0E203A97U;
	code[1] = 0x24060080U;
	code[2] = 0x02701821U;
	code[3] = 0x2E020080U;
	code[4] = 0x0002180AU;
	code[5] = 0xA0600000U;
	code[6] = 0x2402000AU;
	code[9] = 0x26240001U;
}

void pspemuApplyTitlePatchShiftPrologue(void)
{
	uint32_t *ram = pspemuRamWords(0);
	uint32_t *cursor = pspemuRamWords(0x008279D8U);
	uint32_t offset = 0x008279D4U;

	ram[0x008279CCU >> 2] = 0;
	do {
		uint32_t nextWord = cursor[1];
		cursor++;
		offset += 4;
		cursor[-2] = nextWord;
	} while (offset != 0x00827A0CU);

	ram[0x00827A04U >> 2] = 0x24060080U;
	ram[0x008279F0U >> 2] = 0x54620016U;
	ram[0x00827A0CU >> 2] = 0x2E230080U;
	ram[0x00827A10U >> 2] = 0x0003100AU;
	ram[0x00827A44U >> 2] = 0x1000FFE6U;
	ram[0x00827A6CU >> 2] = 0x1000FFDCU;
}

void pspemuApplyTitlePatchShiftPrologueWithHook(void)
{
	uint32_t *ram = pspemuRamWords(0);
	uint32_t *cursor = pspemuRamWords(0x00827A08U);
	uint32_t offset = 0x00827A04U;

	ram[0x008279FCU >> 2] = 0;
	do {
		uint32_t nextWord = cursor[1];
		cursor++;
		offset += 4;
		cursor[-2] = nextWord;
	} while (offset != 0x00827A3CU);

	ram[0x00827A34U >> 2] = 0x24060080U;
	ram[0x00827A20U >> 2] = 0x54620016U;
	ram[0x00827A3CU >> 2] = 0x2E230080U;
	ram[0x00827A40U >> 2] = 0x0003100AU;
	ram[0x00827A74U >> 2] = 0x1000FFE6U;
	ram[0x00827A9CU >> 2] = 0x1000FFDCU;
	pspemuPatchMipsLoadPairStub(pspemuRamWords(0x0087F424U));
}

void pspemuApplyTitlePatchBranchTrampoline(void)
{
	uint32_t *code = pspemuRamWords(0x00843F78U);

	code[0] = 0x3C0608E7U;
	code[1] = 0x34C87654U;
	code[2] = 0x13E80005U;
	code[3] = 0x34C875D4U;
	code[4] = 0x13E80003U;
	code[5] = 0x00000000U;
	code[6] = 0x0A2B10DEU;
	code[7] = 0x00000000U;
	code[8] = 0x0A2B112AU;
	code[9] = 0x2406001FU;
}

void pspemuApplyTitlePatchBranchTrampolineAlt(void)
{
	uint32_t *code = pspemuRamWords(0x008F56C0U);

	code[0] = 0x3C0608E7U;
	code[1] = 0x34C85E9CU;
	code[2] = 0x13E80005U;
	code[3] = 0x34C85E1CU;
	code[4] = 0x13E80003U;
	code[5] = 0x00000000U;
	code[6] = 0x0A2AF19BU;
	code[7] = 0x00000000U;
	code[8] = 0x0A2AF1E7U;
	code[9] = 0x2406001FU;
}

void pspemuApplyAvcPatchReturnZero(void)
{
	uint32_t *slot = pspemuRamWords(0x01E660C4U);

	if (*slot == 0x0E7997AAU)
		*slot = 0x24020000U;
}

void pspemuApplyAvcPatchReturnZeroAlt(void)
{
	uint32_t *slot = pspemuRamWords(0x01F380C4U);

	if (*slot == 0x0E7CDFAAU)
		*slot = 0x24020000U;
}

void pspemuApplyTitlePatchRelocStub(void)
{
	uint32_t *ram = pspemuRamWords(0);

	pspemuWriteRelocatedCallStub(0x00BF8BC0U);
	ram[0x00BF8BECU >> 2] = 0x0A2E1847U;
	ram[0x00B86114U >> 2] = 0x0A2FE2F2U;
}

void pspemuApplyTitlePatchRelocStubAlt(void)
{
	uint32_t *ram = pspemuRamWords(0);

	pspemuWriteRelocatedCallStub(0x00BF8E14U);
	ram[0x00BF8E40U >> 2] = 0x0A2E18DAU;
	ram[0x00B86360U >> 2] = 0x0A2FE387U;
}

void pspemuApplyTitlePatchStackCallStub(void)
{
	uint32_t *ram = pspemuRamWords(0);
	uint32_t *code = pspemuRamWords(0x00A3BB08U);

	code[0]  = 0x0A2001EEU;
	code[1]  = 0x00000000U;
	code[2]  = 0x0E2998A6U;
	code[3]  = 0x00000000U;
	code[4]  = 0x10400005U;
	code[5]  = 0x00000000U;
	code[6]  = 0x0E2998CCU;
	code[7]  = 0x24040100U;
	code[8]  = 0x0A26F873U;
	code[9]  = 0x00000000U;
	code[10] = 0x0A26F879U;
	code[11] = 0x00000000U;
	ram[0x009BE1DCU >> 2] = 0x0A28EEC4U;
}

void pspemuApplyTitlePatchInlineJump(void)
{
	uint32_t *ram = pspemuRamWords(0);
	uint32_t *code = pspemuRamWords(0x00A1A700U);

	code[0] = 0x0E226618U;
	code[1] = 0x2406001FU;
	code[2] = 0x0A203E34U;
	code[3] = 0x00000000U;
	ram[0x0080F8C8U >> 2] = 0x0A2869C0U;
}

void pspemuApplyTitlePatchInlineJumpAlt(void)
{
	uint32_t *ram = pspemuRamWords(0);
	uint32_t *code = pspemuRamWords(0x009F1F00U);

	code[0] = 0x0E2444EDU;
	code[1] = 0x2406001FU;
	code[2] = 0x0A21426BU;
	code[3] = 0x00000000U;
	ram[0x008509A4U >> 2] = 0x0A27C7C0U;
}

/* 0x81005084 */
int pspemuMountPs1VmcSlots(void)
{
	char vmcPath[52];
	int res;
	unsigned int slot;

	res = vmcManagerRegisterSlot(2, g_mcWorkBuf + PSPEMU_VMC_WORK_SIZE, PSPEMU_VMC_SLOT_SIZE);
	if (res < 0)
		return res;

	for (slot = 0; slot < 2; slot++) {
		void *slotBuf;

		snprintf(vmcPath, 50, "ms0:PSP/SAVEDATA/%s/SCEVMC%d.VMP", pspemuCompatGetTitleId(), slot);
		slotBuf = popsHostGetVmcBuffer(slot);
		if (slotBuf == NULL)
			return PSPEMU_ERROR_INVALID_ARG;

		res = vmcManagerRegisterSlot(slot, slotBuf, PSPEMU_VMC_SLOT_SIZE);
		if (res < 0)
			return res;

		res = vmcManagerMountFile(slot, -1, vmcPath);
		if (res < 0)
			return res;
	}

	return 0;
}

/* 0x81005118 */
int pspemuCopyPopsTitleStrings(void)
{
	uint8_t *cdram = g_uncachedCdram;

	strncpy(g_popsTitleName, (const char *)(cdram + PSPEMU_RAM_POPS_TITLE_OFFSET), sizeof(g_popsTitleName));
	g_popsTitleName[sizeof(g_popsTitleName) - 1] = '\0';
	strncpy(g_popsDiscName, (const char *)(cdram + PSPEMU_RAM_POPS_DISC_OFFSET), sizeof(g_popsDiscName));
	g_popsDiscName[sizeof(g_popsDiscName) - 1] = '\0';
	return 0;
}

/* 0x81005168 */
int pspemuGetPopsTitleStrings(const char **outTitle, const char **outDisc)
{
	*outTitle = g_popsTitleName;
	*outDisc = g_popsDiscName;
	return 0;
}

static inline int pspemuDecodeTitleFlagChar(unsigned char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'Z')
		return ch - 'A' + 36;
	if (ch == '_')
		return 62;
	if (ch == '-')
		return 63;
	return -1;
}

static inline char pspemuEncodeTitleFlagChar(unsigned int bits)
{
	if (bits <= 9)
		return (char)(bits + '0');
	if (bits <= 35)
		return (char)(bits - 10 + 'a');
	if (bits <= 61)
		return (char)(bits - 36 + 'A');
	if (bits == 62)
		return '_';
	if (bits == 63)
		return '-';
	return (char)-1;
}

static inline void pspemuUpgradeIfLegacyTitleList(char *titleList)
{
	char *cursor;

	for (cursor = titleList; cursor != titleList + 90; cursor += 10) {
		if (cursor[9] != ',')
			return;
	}
	pspemuUpgradeTitleListFormat(titleList);
}

/* 0x81005180 */
SceSSize pspemuRecordTitleListFlags(unsigned int flagBits)
{
	const char *titleId = pspemuCompatGetTitleId();
	SceUID fd;

	if (strlen(titleId) == PSPEMU_TITLE_ID_LEN) {
		PspemuTitleRecord *records = (PspemuTitleRecord *)g_titleListBuf;
		int index;
		int found = 0;

		if (pspemuCompatEnsureListDir() != 0)
			return 0;

		if (sceCompatGetTitleList(g_titleListBuf, sizeof(g_titleListBuf)) != 0) {
			static const PspemuTitleIdChars defaultTitleId = { "ULJS00000" };

			memset(g_titleListBuf, 0, sizeof(g_titleListBuf));
			*(PspemuTitleIdChars *)records[0].titleId = defaultTitleId;
			records[0].flagsChar = '0';
			records[0].statusChar = '0';
			for (index = 1; index < PSPEMU_TITLE_LIST_COUNT; index++) {
				records[index - 1].separator = ',';
				*(PspemuTitleIdChars *)records[index].titleId = defaultTitleId;
				records[index].flagsChar = '0';
				records[index].statusChar = '0';
			}
		}

		pspemuUpgradeIfLegacyTitleList(g_titleListBuf);

		for (index = 0; index < PSPEMU_TITLE_LIST_COUNT; index++) {
			if (memcmp(records[index].titleId, titleId, PSPEMU_TITLE_ID_LEN) == 0) {
				int prevBits = pspemuDecodeTitleFlagChar((unsigned char)records[index].flagsChar);
				char nextChar = (prevBits < 0)
					? (char)-1
					: pspemuEncodeTitleFlagChar(flagBits | (unsigned int)prevBits);

				records[index].flagsChar = nextChar;
				records[index].statusChar = 'd';
				found = 1;
				break;
			}
		}

		if (!found) {
			memmove(&records[1], &records[0], 0x53);
			*(PspemuTitleIdChars *)records[0].titleId = *(const PspemuTitleIdChars *)titleId;
			records[0].flagsChar = pspemuEncodeTitleFlagChar(flagBits);
			records[0].statusChar = 'd';
			records[0].separator = ',';
		}
	}

	fd = sceIoOpen(PSPEMU_TITLE_LIST_PATH, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 06);
	if (fd >= 0) {
		SceSSize written = sceIoWrite(fd, g_titleListBuf, sizeof(g_titleListBuf));
		sceIoClose(fd);
		return written;
	}
	return 0;
}

/* 0x8100538c */
SceSSize pspemuSyncTitleList(void)
{
	SceUID fd;
	SceSSize written;

	if (sceCompatGetTitleList(g_syncTitleListBuf, sizeof(g_syncTitleListBuf)) != 0)
		return 0;

	pspemuUpgradeIfLegacyTitleList(g_syncTitleListBuf);

	fd = sceIoOpen(PSPEMU_TITLE_LIST_PATH, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 06);
	if (fd < 0)
		return 0;

	written = sceIoWrite(fd, g_syncTitleListBuf, sizeof(g_syncTitleListBuf));
	sceIoClose(fd);
	return written;
}

/* 0x810053f8: ScePspemuMain thread entry (0.990: sceCompatMain) */
int sceCompatMain(SceSize argLen, void *argp)
{
	SceCompatCdram cdram;
	SceDisplayFrameBuf frameBufParam;
	SceKernelAllocMemBlockOpt memOpt;
	SceAppUtilInitParam appUtilInitParam;
	SceAppUtilBootParam appUtilBootParam;
	PspPbpHeader pbp;
	PspNpUmdHeader npUmdHeader;
	char rifContentId[36];
	static const char defaultDiscBoot[33] = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN";
	PspemuBootBlock *bootBlock;
	PspemuGameInfo *guestGameInfo;
	PspemuSystemSettings *sysSettings;
	SceUID tempBlockId;
	SceUID frameBlockId;
	SceUID suspendThreadId;
	SceUID pbpFd;
	int splashIndex;
	int splashCount;
	int guideDialogId;
	int res;

	if (argLen != sizeof(PspemuMainThreadArg))
		return PSPEMU_ERROR_INVALID_ARG;

	res = sceShellUtilInitEvents(0);
	if (res < 0)
		return res;

	sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_USB_CONNECTION);

	res = sceCompatAllocCdramWithHole(&cdram);
	if (res < 0)
		return res;

	g_cachedCdram = (uint8_t *)cdram.cachedCdram;
	g_uncachedCdram = (uint8_t *)cdram.uncachedCdram;

	tempBlockId = sceKernelAllocMemBlock("ScePspemuFrameTempWork", PSPEMU_MEMBLOCK_CDRAM_RW, 0x100000U, NULL);
	if (tempBlockId < 0)
		return tempBlockId;

	res = sceKernelGetMemBlockBase(tempBlockId, (void **)&g_frameTempBuf);
	if (res != 0)
		return res;

	frameBufParam.size = sizeof(frameBufParam);
	frameBufParam.base = g_frameTempBuf;
	frameBufParam.pitch = PSPEMU_SCREEN_PITCH;
	frameBufParam.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
	frameBufParam.width = PSPEMU_SCREEN_WIDTH;
	frameBufParam.height = PSPEMU_SCREEN_HEIGHT;

	for (splashIndex = 0; ; splashIndex++) {
		SceSize pngSize = 0;
		int32_t pngWidth = 0;
		int32_t pngHeight = 0;
		int32_t pngFormat = 0;
		uint8_t *pngScratch = g_cachedCdram;
		uint8_t *decodeScratch = g_cachedCdram + 0x100000U;

		splashCount = pspemuReadStartDatImage(splashIndex, pngScratch, &pngSize, rifContentId);
		if (splashCount == 1 || splashCount == 2) {
			int decRes = scePngDec(
				decodeScratch, 0x100000U, pngScratch, 0x100000U,
				&pngWidth, &pngHeight, &pngFormat);
			if (decRes < 0) {
				splashCount = decRes;
			} else if (pngWidth != (int32_t)PSPEMU_SCREEN_WIDTH || pngHeight != (int32_t)PSPEMU_SCREEN_HEIGHT) {
				splashCount = PSPEMU_ERROR_CORRUPTED;
			} else if (pngFormat == 0x420) {
				uint32_t row;

				for (row = 0; row < PSPEMU_SCREEN_HEIGHT; row++) {
					memcpy(
						g_frameTempBuf + row * PSPEMU_SCREEN_STRIDE_BYTES,
						decodeScratch + row * PSPEMU_SCREEN_ROW_BYTES,
						PSPEMU_SCREEN_ROW_BYTES);
				}
				g_splashShownTime = sceKernelGetProcessTimeLow();
			} else {
				decRes = scePngConvertToRGBA(g_frameTempBuf, decodeScratch, pngWidth, pngHeight, pngFormat);
				if (decRes < 0) {
					splashCount = decRes;
				} else {
					int row;

					for (row = (int)PSPEMU_SCREEN_HEIGHT - 1; row >= 0; row--) {
						memcpy(
							g_frameTempBuf + (uint32_t)row * PSPEMU_SCREEN_STRIDE_BYTES,
							g_frameTempBuf + (uint32_t)row * PSPEMU_SCREEN_ROW_BYTES,
							PSPEMU_SCREEN_ROW_BYTES);
					}
					g_splashShownTime = sceKernelGetProcessTimeLow();
				}
			}
		} else {
			g_splashShownTime = sceKernelGetProcessTimeLow();
		}

		res = sceDisplaySetFrameBuf(&frameBufParam, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);
		if (res != 0)
			return res;

		if (splashIndex + 1 >= splashCount)
			break;

		sceKernelDelayThread(3000000);
		sceDisplaySetFrameBuf(NULL, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);
	}

	if (g_isPs1Mode != 1) {
		void *frameBase = NULL;

		memOpt.size = sizeof(SceKernelAllocMemBlockOpt);
		memOpt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
		memOpt.alignment = 0x100000U;

		frameBlockId = sceKernelAllocMemBlock(
			"ScePspemuFrameBuffer", PSPEMU_MEMBLOCK_PHYCONT_NC_RW, 0x100000U, &memOpt);
		if (frameBlockId < 0)
			return frameBlockId;

		res = sceKernelGetMemBlockBase(frameBlockId, &frameBase);
		if (res < 0)
			return res;
		if (res == 0)
			g_frameBuf = (uint8_t *)frameBase;

		res = sceCompatFrameBufferInit((void *)(uintptr_t)frameBlockId);
		if (res != 0)
			return res;
	}

	sceDmacMemset(g_cachedCdram, 0, 0x02000000U);
	sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK, g_cachedCdram, 0x02000000U);
	sceDmacMemset(g_cachedCdram + 0x03000000U, 0, 0x01000000U);
	sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK, g_cachedCdram + 0x03000000U, 0x01000000U);

	bootBlock = (PspemuBootBlock *)(void *)(g_uncachedCdram + PSPEMU_RAM_BOOT_BLOCK_OFFSET);
	guestGameInfo = &bootBlock->gameInfo;
	memcpy(guestGameInfo, argp, sizeof(PspemuGameInfo));

	memset(&appUtilInitParam, 0, sizeof(appUtilInitParam));
	memset(&appUtilBootParam, 0, sizeof(appUtilBootParam));
	res = sceAppUtilInit(&appUtilInitParam, &appUtilBootParam);
	if (res != 0)
		return res;

	sysSettings = (PspemuSystemSettings *)(void *)(g_uncachedCdram + PSPEMU_RAM_SYS_SETTINGS_OFFSET);
	res = pspemuSettingsLoadAll(sysSettings);
	if (res != 0)
		return res;

	res = pspemuInitDialogOnce();
	if (res != 0)
		return res;

	g_useTempFrameBuf = 1;

	suspendThreadId = sceKernelCreateThread(
		"ScePspemuPrepareSuspend",
		pspemuPrepareSuspendThread,
		95,
		0x1000,
		0,
		SCE_KERNEL_CPU_MASK_USER_0,
		NULL);
	if (suspendThreadId < 0)
		pspemuTriggerFatalError(suspendThreadId);

	res = sceKernelStartThread(suspendThreadId, 0, NULL);
	if (res != 0)
		pspemuTriggerFatalError(res);

	if (g_isPs1Mode == 0)
		guideDialogId = 4;
	else if (g_isPs1Mode == 1)
		guideDialogId = 5;
	else
		return PSPEMU_ERROR_NOT_SUPPORTED;

	if ((unsigned int)pspemuSettingsGetFirstBootDone() <= 1U) {
		pspemuSettingsShowGuideDialog(guideDialogId);
		pspemuSettingsMarkFirstBootDone();
	}

	if (g_isPs1Mode == 0)
		pspemuSettingsSetMenuEnabled(1);

	pspemuLockOrDie(PSPEMU_LOCK_BOOT);
	g_bootLockState = 0;
	pspemuUnlockOrDie(PSPEMU_LOCK_BOOT);

	res = pspemuCompatLoadTitleEntry(guestGameInfo->sfoProductString,
	                                 ((const PspemuMainThreadArg *)argp)->titleInfoThreadId);
	if (res != 0)
		return res;

	if (pspemuCompatHasFlag(0x200000U) != 0)
		sysSettings->buttonAssign = 1;

	if (pspemuCompatHasFlag(0x10U) != 0)
		sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_USB_CONNECTION);
	else
		sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_USB_CONNECTION);

	if (g_isPs1Mode != 0 || pspemuCompatHasFlag(0x08U) != 0) {
		g_activeCdram = g_uncachedCdram;
		popsHostSetCdramOffset((int)(g_cachedCdram - g_uncachedCdram));
		g_cachedCdram = NULL;
	} else {
		g_activeCdram = g_cachedCdram;
	}
	remotefsSetCdramBase(g_activeCdram, g_cachedCdram, g_uncachedCdram);

	res = sceScreenShotDisable();
	if (res != 0)
		return res;

	res = sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITALANALOG);
	if (res != 0)
		return res;

	if (vshKernelCheckModelCapability(2) != 0) {
		res = SceCamera_99B5FB62(1);
		if (res != 0)
			return res;
	}

	res = remoteLowPriorityInit();
	if (res < 0)
		return res;

	{
		int needWaitResume = 0;

		pspemuLockOrDie(PSPEMU_LOCK_BOOT);
		if (g_bootLockState == 0)
			g_bootLockState = 1;
		else
			needWaitResume = 1;
		pspemuUnlockOrDie(PSPEMU_LOCK_BOOT);

		if (needWaitResume != 0) {
			int attempts = 10000;
			int resumed = 0;

			while (!resumed) {
				int eventCount = 0;

				if (sceAppMgrReceiveEventNum(&eventCount) == 0 && eventCount > 0) {
					int index;

					for (index = 0; index < eventCount; index++) {
						struct SceAppMgrAppEvent event;

						if (sceAppMgrReceiveEvent(&event) == 0 &&
						    event.eventId == PSPEMU_APPMGR_EVENT_RESUME_FINISH) {
							resumed = 1;
							break;
						}
					}
					if (resumed)
						break;
				}
				sceKernelDelayThread(1000);
				if (--attempts == 0)
					return PSPEMU_ERROR_FATAL;
			}
		}
	}

	remoteLowPriorityLockPsButton();
	pspemuPowerLock();

	res = sceCompatInitEx(pspemuCompatHasFlag(0x04U) == 0);
	if (res != 0)
		return res;

	scePowerSetGpuClockFrequency(41);

	res = remoteMsfsInit();
	if (res != 0)
		return res;

	res = remoteFlashfsInit(g_uncachedCdram + PSPEMU_RAM_FLASH_WORK_OFFSET);
	if (res != 0)
		return res;

	if (g_isPs1Mode != 1) {
		res = remoteAudioOutInit();
		if (res != 0)
			return res;
		res = remoteAudioInInit();
		if (res != 0)
			return res;
		res = meEdramInit(0x400000U);
		if (res != 0)
			return res;
		res = remoteMeInit();
		if (res != 0)
			return res;
		res = remoteDmacplusInit();
		if (res != 0)
			return res;
		res = remoteSas2Init();
		if (res != 0)
			return res;
		res = remotePeripheralInit();
		if (res != 0)
			return res;
		res = remoteNetInit();
		if (res != 0)
			return res;
		res = remoteOskInit();
		if (res != 0)
			return res;
		res = remoteUsbInit(g_uncachedCdram + PSPEMU_RAM_USB_WORK_OFFSET);
		if (res != 0)
			return res;
		res = remoteUsbPspcmInit();
		if (res != 0)
			return res;
		sceJpegWorkInit(meEdramGetBase(0));
		if (sceJpegWorkReset() != NULL) {
			res = meWrapperApiInit();
			if (res != 0)
				return res;
		}
	} else {
		SceUID mcWorkId;
		int pocsAvailable;

		res = popsHostInit();
		if (res != 0)
			return res;
		res = remotePeripheralInit();
		if (res != 0)
			return res;

		mcWorkId = sceKernelAllocMemBlock("ScePspemuMcWork", PSPEMU_MEMBLOCK_PHYCONT_RW, 0x40000U, NULL);
		if (mcWorkId < 0)
			return mcWorkId;
		res = sceKernelGetMemBlockBase(mcWorkId, (void **)&g_mcWorkBuf);
		if (res != 0)
			return res;
		res = vmcManagerInitWork(g_mcWorkBuf, PSPEMU_VMC_WORK_SIZE);
		if (res < 0)
			return res;

		g_hasPocketStationApp = (sceCompatCheckPocketStation() == 1);
		pocsAvailable = g_hasPocketStationApp;
		if ((sysSettings->npCountry[0] == 'j' && sysSettings->npCountry[1] == 'p' && sysSettings->npCountry[2] == '\0') ||
		    pocsAvailable) {
			pocsAvailable = (sceCompatIsPocketStationTitle(pspemuCompatGetTitleId()) == 1);
		}
		g_isPocketStationTitle = pocsAvailable;

		res = remotePocsInit();
		if (res != 0)
			return res;
	}

	if (g_isPs1Mode == 0) {
		size_t selfLen;

		bootBlock->bootMode = PSPEMU_BOOT_MODE_PSP;

		selfLen = strlen(g_selfPath);
		if (g_selfPath[selfLen - 9] == 'P') {
			bootBlock->apiType = PSPEMU_API_TYPE_MS_PBP;
			strncpy(bootBlock->bootPath, g_selfPath, strlen(g_selfPath) + 1);
			strncpy(bootBlock->execPath, g_selfPath, strlen(g_selfPath) + 1);
		} else {
			bootBlock->apiType = PSPEMU_API_TYPE_UMD_ISO;
			memcpy(bootBlock->bootPath, defaultDiscBoot, sizeof(defaultDiscBoot));
			memcpy(bootBlock->execPath, defaultDiscBoot, sizeof(defaultDiscBoot));
		}
		strncpy(bootBlock->umdImagePath, g_originalPath, strlen(g_originalPath) + 1);
	} else if (g_isPs1Mode == 1) {
		bootBlock->bootMode = PSPEMU_BOOT_MODE_POPS;
		bootBlock->apiType = PSPEMU_API_TYPE_POPS;
		strncpy(bootBlock->bootPath, g_selfPath, strlen(g_selfPath) + 1);
		strncpy(bootBlock->execPath, g_selfPath, strlen(g_selfPath) + 1);
	} else {
		return PSPEMU_ERROR_INVALID_ARG;
	}

	pbpFd = pspemuOpenBootPbp(&pbp);
	if (pbpFd < 0)
		return pbpFd;

	{
		uint32_t sfoSize = pbp.iconPngOffset - pbp.paramSfoOffset;
		SceSSize sfoRead;

		if (sfoSize >= sizeof(bootBlock->paramSfo)) {
			sceIoClose(pbpFd);
			return PSPEMU_ERROR_CORRUPTED;
		}
		sfoRead = sceIoPread(pbpFd, bootBlock->paramSfo, sfoSize, (SceOff)pbp.paramSfoOffset);
		sceIoClose(pbpFd);
		if (sfoRead != (SceSSize)sfoSize)
			return PSPEMU_ERROR_CORRUPTED;
		bootBlock->paramSfoSize = sfoSize;
	}

	guestGameInfo->size = sizeof(PspemuGameInfo);
	guestGameInfo->flags |= 0x100U;
	strncpy(guestGameInfo->umdDataString, guestGameInfo->sfoGameDataId, sizeof(guestGameInfo->umdDataString));
	guestGameInfo->flags |= 0x01U;
	strncpy(guestGameInfo->expectUmdData, guestGameInfo->umdDataString, sizeof(guestGameInfo->expectUmdData));
	guestGameInfo->allowReplaceUmd = 0;
	guestGameInfo->umdCacheOn = 0;
	guestGameInfo->utilityLocation = 0;
	guestGameInfo->flags |= 0x20212U;
	memcpy(guestGameInfo->vshBootfilename, defaultDiscBoot, sizeof(defaultDiscBoot));
	guestGameInfo->flags |= 0x40000U;
	memset(guestGameInfo->subscriptionValidity, 0, sizeof(guestGameInfo->subscriptionValidity));

	pspemuSyncTitleList();

	if (pspemuCompatGetParam(16) != -1 && pspemuCompatGetParam(17) != -1) {
		SceUID memCheckId = sceKernelCreateThread(
			"ScePspemuMemoryCheck",
			pspemuMemoryCheckThread,
			98,
			0x5000,
			0,
			SCE_KERNEL_CPU_MASK_USER_0,
			NULL);
		if (memCheckId < 0)
			return memCheckId;
		res = sceKernelStartThread(memCheckId, 0, NULL);
		if (res != 0)
			return res;
	}

	pbpFd = pspemuOpenBootPbp(&pbp);
	if (pbpFd < 0)
		return pbpFd;

	if (g_isPs1Mode == 0) {
		if (sceIoPread(pbpFd, &npUmdHeader, sizeof(npUmdHeader), (SceOff)pbp.dataPsarOffset) !=
		    (SceSSize)sizeof(npUmdHeader)) {
			sceIoClose(pbpFd);
			return PSPEMU_ERROR_CORRUPTED;
		}
		sceIoClose(pbpFd);
		if ((npUmdHeader.licenseFlags & 0x80000000U) != 0 ||
		    (npUmdHeader.licenseFlags & 0x01000000U) != 0)
			return PSPEMU_ERROR_NOT_SUPPORTED;
		memcpy(rifContentId, npUmdHeader.contentId, sizeof(rifContentId));
	} else if (g_isPs1Mode == 1) {
		SceSSize rifRead = sceIoPread(
			pbpFd, rifContentId, sizeof(rifContentId), (SceOff)(pbp.dataPspOffset + 0x560U));
		sceIoClose(pbpFd);
		if (rifRead != (SceSSize)sizeof(rifContentId))
			return PSPEMU_ERROR_CORRUPTED;
	} else {
		return PSPEMU_ERROR_INVALID_ARG;
	}

	sceCompatSetRif(rifContentId);
	if (g_isPs1Mode == 0)
		pspemuSettingsApplyDisplay();

	g_heartbeatStallStart = 0;
	res = sceCompatStart();
	if (res != 0) {
		fflush(NULL);
		return res;
	}

	if (g_isPocketStationTitle != 0) {
		res = pspemuBgImageInit();
		if (res != 0)
			return res;
		res = remotePocsMountSaveData(pspemuCompatGetTitleId());
		if (res != 0)
			return res;
	}

	res = sceCompatWaitSpecialRequest(1);
	if (res != 0)
		return res;

	for (;;) {
		int powerTickKind = SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND;
		int eventCount = 0;

		sceDisplayWaitVblankStartCB();

		if (sceCompatGetStatus() == PSPEMU_COMPAT_STATUS_RUNNING) {
			SceCtrlData padData;
			SceCtrlDataPsp pspPad;
			int seqB = 0;
			int seqA = 0;
			int tickReq = 0;
			int heartbeat = 0;

			sceCtrlPeekBufferPositive(0, &padData, 1);
			pspemuSettingsUpdateController(&padData);

			pspPad.TimeStamp = (uint32_t)padData.timeStamp;
			pspPad.Buttons = padData.buttons;
			pspPad.Lx = padData.lx;
			pspPad.Ly = padData.ly;
			pspPad.Rx = padData.rx;
			pspPad.Ry = padData.ry;
			memcpy(pspPad.Rsrv, padData.rsrv, sizeof(pspPad.Rsrv));
			sceCompatWriteSharedCtrl(&pspPad);

			g_psButtonIntercepted = (padData.buttons >> 16) & 1U;
			remotePeripheralUpdateState();

			if (sceCompatReadShared32(PSPEMU_SHARED_POWER_TICK_SEQ_B, &seqB) == 0 &&
			    g_lastPowerTickSeq != seqB &&
			    g_psButtonIntercepted == 0) {
				g_lastPowerTickSeq = seqB;
				if (sceCompatReadShared32(PSPEMU_SHARED_POWER_TICK_SEQ_A, &seqA) == 0 &&
				    seqB == seqA &&
				    sceCompatReadShared32(PSPEMU_SHARED_POWER_TICK_KIND, &tickReq) == 0) {
					if (tickReq == 1)
						powerTickKind = SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND;
					else if (tickReq == 6)
						powerTickKind = SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF |
							SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING |
							SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND;
					else
						powerTickKind = SCE_KERNEL_POWER_TICK_DEFAULT;
				}
			}

			if (sceCompatReadShared32(PSPEMU_SHARED_HEARTBEAT_COUNTER, &heartbeat) == 0) {
				if (g_lastHeartbeat != heartbeat) {
					g_heartbeatStallStart = 0;
				} else if (g_heartbeatStallStart != 0) {
					if (sceKernelGetProcessTimeWide() - g_heartbeatStallStart > 30000000LL)
						return PSPEMU_ERROR_FATAL;
				} else {
					g_heartbeatStallStart = sceKernelGetProcessTimeWide();
				}
				g_lastHeartbeat = heartbeat;
			}
		}

		if (remotePocsIsActive() == 1) {
			if (remotePocsPollPowerKeepAlive() == 0)
				sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
		} else if (g_isPs1Mode != 1 || popsHostIsOverlayActive() == SCE_FALSE) {
			sceKernelPowerTick(powerTickKind);
		}

		if (sceAppMgrReceiveEventNum(&eventCount) != 0 || eventCount <= 0)
			continue;

		{
			int eventIndex;

			for (eventIndex = 0; eventIndex < eventCount; eventIndex++) {
				struct SceAppMgrAppEvent appEvent;

				res = sceAppMgrReceiveEvent(&appEvent);
				if (res != 0)
					continue;

				if (appEvent.eventId == PSPEMU_APPMGR_EVENT_QUIT)
					return 0;

				if (appEvent.eventId == PSPEMU_APPMGR_EVENT_APP_PARAM) {
					g_isPocketStationLaunch = remoteMsfsCheckPocketStationParam();
					if (remotePocsIsActive() != g_isPocketStationLaunch) {
						if (SceCommonDialog_FA26EB30() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
							remoteLowPriorityWaitIdle();
							SceCommonDialog_A68A44EA();
							while (SceCommonDialog_FA26EB30() == SCE_COMMON_DIALOG_STATUS_RUNNING)
								sceDisplayWaitVblankStart();
						} else if (SceCommonDialog_500229F0() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
							remoteLowPriorityWaitIdle();
							SceCommonDialog_7030E4A6();
							while (SceCommonDialog_500229F0() == SCE_COMMON_DIALOG_STATUS_RUNNING)
								sceDisplayWaitVblankStart();
						} else if (scePhotoImportDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
							scePhotoImportDialogAbort();
							while (scePhotoImportDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
								sceDisplayWaitVblankStart();
						}

						if (g_isPocketStationLaunch != 0) {
							remotePocsEnterMode();
							popsHostSetDisplayMode(1);
						} else {
							remotePocsExitMode();
							if (remotePocsHasMountedMedia() != 0)
								popsHostReloadVmc(0);
							remotePocsNotifyDisplayReset();
							popsHostSetDisplayMode(0);
						}
					}
					continue;
				}

				if (appEvent.eventId != PSPEMU_APPMGR_EVENT_RESUME_START &&
				    appEvent.eventId != PSPEMU_APPMGR_EVENT_RESUME_FINISH)
					continue;

				if (g_isPocketStationTitle != 0) {
					if (g_hasPocketStationApp == 0 && sceCompatCheckPocketStation() == 1) {
						res = remotePocsInstallAppSupport();
						if (res < 0)
							return res;
					}
					g_hasPocketStationApp = (sceCompatCheckPocketStation() == 1);
				}

				if (sceCompatGetStatus() == PSPEMU_COMPAT_STATUS_SUSPENDED) {
					int refreshRes;

					if (pspemuCompatHasFlag(0x10U) != 0)
						sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_USB_CONNECTION);
					remoteNetResume();

					if (g_isPs1Mode == 0 &&
					    sceImeDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
						sceKernelDelayThread(500000);
					}

					if (SceCommonDialog_FA26EB30() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
						if (g_isPs1Mode == 1)
							SceCommonDialog_ED2F1495();
					} else if (g_isPs1Mode == 1) {
						popsHostSetDisplayMode(0);
					} else {
						frameBufParam.size = sizeof(frameBufParam);
						frameBufParam.base = g_frameTempBuf;
						frameBufParam.pitch = PSPEMU_SCREEN_PITCH;
						frameBufParam.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
						frameBufParam.width = PSPEMU_SCREEN_WIDTH;
						frameBufParam.height = PSPEMU_SCREEN_HEIGHT;
						sceDisplaySetFrameBuf(&frameBufParam, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);
					}

					g_useTempFrameBuf = 1;
					pspemuPowerLock();
					remoteLowPriorityLockPsButton();
					g_heartbeatStallStart = 0;
					remotePocsNotifyDisplayReset();

					refreshRes = pspemuSettingsRefreshDynamic(sysSettings);
					if (refreshRes < 0) {
						if (g_isPocketStationTitle != 0)
							remotePocsSetEmulationActive(0);
						pspemuSettingsShowFatalNotice(refreshRes);
						if (g_isPocketStationTitle != 0)
							remotePocsSetEmulationActive(1);
					}

					res = sceCompatSuspendResume(1);
					if (res != 0)
						return res;
				}

				remoteMeAvcdecNotifyResume();
				if ((g_isPs1Mode == 1 && pspemuSettingsGetDialogActive() != 0) ||
				    remoteNetIsAdhocActive() == 0) {
					res = pspemuSettingsSetMenuEnabled(1);
					if (res != 0)
						return res;
				}
			}
		}
	}
}

/* 0x81006288: verifies path length, NUL-termination, prefix match, and no "/../" traversal */
int pspemuValidatePathPrefix(const char *path, unsigned int pathLen, unsigned int maxLen, const char *prefix)
{
	int prefixLen = (int)strlen(prefix);
	int index;

	if (prefixLen + 1 >= (int)pathLen)
		return 0;
	if (pathLen > maxLen)
		return 0;
	if (memcmp(path, prefix, (size_t)prefixLen) != 0)
		return 0;
	if (path[pathLen - 1] != '\0')
		return 0;

	for (index = prefixLen; index < (int)(pathLen - 4); index++) {
		const char *cursor = &path[index];
		if ((cursor[0] == '/' || cursor[0] == '\\') &&
		    cursor[1] == '.' &&
		    cursor[2] == '.' &&
		    (cursor[3] == '/' || cursor[3] == '\\')) {
			return 0;
		}
	}

	return 1;
}

/* 0x81006304 */
int pspemuHasCameraCapability(void)
{
	return (int)vshKernelCheckModelCapability(2);
}

/* 0x81006310 */
int pspemuIsVitaModel(void)
{
	return (int)vshSblAimgrIsVITA();
}

/* 0x81006318 */
int pspemuHasRearTouchCapability(void)
{
	return (int)vshKernelCheckModelCapability(1);
}
