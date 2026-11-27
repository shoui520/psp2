/*
 * menu_dlg.c: ScePspemu in-game settings / PS1 / PocketStation common dialog
 * host interface and GXM dialog presentation (0x81017924..0x810185ab).
 */
#include "host_common.h"

#include <stdio.h>
#include <string.h>

#define MENU_COMMON_DIALOG_SDK_VERSION  0x03650000U
#define MENU_COMMON_DIALOG_MAGIC_BASE   0xC0D1A109U
#define MENU_ERR_BUSY_DIALOG            ((int)0x80020401)
#define MENU_ERR_DIALOG_ABORTED         ((int)0x80010089)
#define MENU_ERR_IO                     ((int)0x80010005)
#define MENU_ERR_NOT_FOUND              ((int)0x80010021)
#define MENU_ERR_INVALID_STATE          ((int)0x80010032)

#define MENU_DISPLAY_WIDTH              960
#define MENU_DISPLAY_HEIGHT             544
#define MENU_DISPLAY_STRIDE             1024
#define MENU_GXM_FB_BYTES               0x300000U
#define MENU_GXM_TOTAL_FB_BYTES         0x900000U
#define MENU_GXM_PARAM_BUF_SIZE         0x1000000U
#define MENU_WORK_BUF_SIZE              0x400000U
#define MENU_PNG_SCRATCH_OFFSET         0x200000U

#define MENU_SFO_READ_SIZE              0x1330U
#define MENU_MAX_SAVEDATA_TITLES        1024U
#define MENU_MAX_VMC_SLOTS              2048U
#define MENU_VMC_ENTRY_STRIDE           928U
#define MENU_VMC_LIST_BYTES             0x3660U
#define MENU_TITLE_RECORD_STRIDE        152U

typedef struct MenuSavedataTitleRecord {
	char titleId[16];
	char titleName[136];
} MenuSavedataTitleRecord;

_Static_assert(sizeof(MenuSavedataTitleRecord) == MENU_TITLE_RECORD_STRIDE,
               "MenuSavedataTitleRecord size");

typedef struct MenuVmcEntryRecord {
	uint8_t iconPixels[768];
	uint8_t header[128];
	char entryName[32];
} MenuVmcEntryRecord;

_Static_assert(sizeof(MenuVmcEntryRecord) == MENU_VMC_ENTRY_STRIDE,
               "MenuVmcEntryRecord size");

typedef struct MenuPocsStatusFlags {
	uint8_t pocsEnabled;
	uint8_t pocsStandalone;
	uint8_t pocsMounted;
	uint8_t pocsAutoBoot;
} MenuPocsStatusFlags;

typedef struct MenuCompatDialogParam {
	uint32_t sdkVersion;
	uint8_t commonParamHeader[72];
	uint32_t commonMagic;
	PspemuMenuSettings settings;
	const char *compatBranch;
	const char *compatRev;
	const char *vshBranch;
	const char *vshRev;
	const char *titleName;
	const char *discId;
	char *titleBanner;
	uint8_t isPs1Mode;
	uint8_t screenshotEnabled;
	uint8_t currentDiscNumber;
	uint8_t modePad;
	uint32_t reservedWord;
	void *popsConfig;
	uint32_t reservedPtrPad;
	int (*onTriggerScreenshot)(void (*doneCb)(void));
	void *(*onCapturePs1Screen)(int *outHeight);
	int (*onSetPs1ScreenScale)(int mode, int scale, uint16_t *outWidth, int16_t *outHeight);
	SceUID (*onScanSavedata)(void);
	int (*onGetVmcInfo)(unsigned int index, char *outTitleId, char *outTitleName, unsigned int *outSlot);
	int (*onMountVmc)(const void *titleId, unsigned int slot);
	int (*onListVmc)(unsigned int slot, void *outEntries);
	int (*onImportVmcEntry)(unsigned int dstSlot, const void *entryName);
	int (*onDeleteVmcEntry)(unsigned int slot, const void *entryName);
	SceBool (*onCheckPocsEntry)(int slot, const char *entryName);
	int (*onGetPocsStatus)(MenuPocsStatusFlags *outFlags);
	int (*onSetPocsMedia)(int mount);
	int (*onSetPocsAutoBoot)(uint8_t enabled);
	uint8_t trailingPad[32];
} MenuCompatDialogParam;

_Static_assert(sizeof(MenuCompatDialogParam) == 0x110, "MenuCompatDialogParam size");

typedef struct MenuCompatDialogResult {
	int32_t resultCode;
	PspemuMenuSettings settings;
	uint8_t trailingPad[32];
} MenuCompatDialogResult;

_Static_assert(sizeof(MenuCompatDialogResult) == 0x64, "MenuCompatDialogResult size");

static unsigned int g_menuVmcCount;
static MenuVmcEntryRecord g_menuVmcPreview[15];
static uint32_t g_menuVmcIndexMap[MENU_MAX_VMC_SLOTS];
static uint8_t *g_menuWorkBase;
static char g_menuTitleBanner[256];
static uint8_t g_menuSfoBuf[MENU_SFO_READ_SIZE];
static int g_menuCaptureWidth;
static uint8_t g_menuScreenshotEnabled;
static char g_menuScreenshotPath[256];
static int g_menuCaptureHeight;
static void *g_menuGxmFrameBufs[3];
static unsigned int g_menuGxmBackIndex;
static SceGxmSyncObject *g_menuGxmSyncObjs[3];
static int g_menuActiveDialogOwner;
static SceUID g_menuGxmFbMemBlock;
static uint64_t g_menuImeDeadline;
static unsigned int g_menuGxmFrontIndex;

int menuDlgTriggerScreenshot(void (*doneCb)(void))
{
	SceDateTime localTime;
	int ret = sceRtcGetCurrentClockLocalTime(&localTime);

	if (ret != 0) {
		doneCb();
		return ret;
	}

	snprintf(g_menuScreenshotPath, sizeof(g_menuScreenshotPath),
	         "ux0:temp/pspemu/%04d-%02d-%02d_%02d-%02d-%02d.png",
	         localTime.year, localTime.month, localTime.day,
	         localTime.hour, localTime.minute, localTime.second);
	return remoteLowPriorityEnqueuePromoteWithCallback(
		g_menuScreenshotPath, (int (*)(int))doneCb);
}

static int menuDlgMountVmc(const void *titleId, unsigned int slot)
{
	char vmcPath[52];
	const char *curTitle = pspemuCompatGetTitleId();
	int isPocsMcx;

	if (memcmp(curTitle, titleId, 10U) == 0 && slot == 3U) {
		strncpy(vmcPath, remotePocsGetMcxPath(), 50U);
		isPocsMcx = 1;
	} else if (slot <= 1U) {
		snprintf(vmcPath, 50U, "ms0:PSP/SAVEDATA/%s/SCEVMC%d.VMP",
		         (const char *)titleId, slot);
		isPocsMcx = 0;
	} else {
		return PSPEMU_ERROR_INVALID_ARG;
	}

	return vmcManagerMountFile(2U, isPocsMcx, vmcPath);
}

static int menuDlgListVmc(unsigned int slot, void *outEntries)
{
	if (slot == 2U)
		return vmcManagerListEntries(2U, outEntries);
	return PSPEMU_ERROR_INVALID_ARG;
}

static int menuDlgImportVmcEntry(unsigned int dstSlot, const void *entryName)
{
	int ret;

	if (dstSlot > 1U)
		return PSPEMU_ERROR_INVALID_ARG;
	if (vmcManagerFlushAndVerify(dstSlot) < 0)
		return MENU_ERR_IO;

	ret = vmcManagerCopyEntry(dstSlot, 2U, entryName);
	if (ret == 0) {
		popsHostReloadVmc(dstSlot);
		return 0;
	}
	return ret;
}

static int menuDlgGetVmcInfo(unsigned int index, char *outTitleId,
                             char *outTitleName, unsigned int *outSlot)
{
	unsigned int mappedIndex = index;
	uint32_t packed;
	uint32_t titleIdx;
	const MenuSavedataTitleRecord *rec;

	if (mappedIndex >= g_menuVmcCount)
		return MENU_ERR_NOT_FOUND;

	if (remotePocsHasMountedMedia() != 0 && (mappedIndex & ~2U) == 0U)
		mappedIndex ^= 2U;

	packed = g_menuVmcIndexMap[mappedIndex];
	*outSlot = packed >> 16;
	titleIdx = packed & 0xFFFFU;
	rec = ((const MenuSavedataTitleRecord *)g_menuWorkBase) + titleIdx;
	strncpy(outTitleId, rec->titleId, 10U);
	strncpy(outTitleName, rec->titleName, 128U);
	return 0;
}

static int menuDlgDeleteVmcEntry(unsigned int slot, const void *entryName)
{
	int ret;

	if (slot > 1U)
		return PSPEMU_ERROR_INVALID_ARG;
	if (vmcManagerFlushAndVerify(slot) < 0)
		return MENU_ERR_IO;

	ret = vmcManagerDeleteEntry(slot, entryName);
	if (ret == 0) {
		popsHostReloadVmc(slot);
		return 0;
	}
	return ret;
}

static int menuDlgSetPocsAutoBoot(uint8_t enabled)
{
	popsHostSetPocsAutoBoot(enabled);
	return 0;
}

static int menuDlgSetPs1ScreenScale(int mode, int scale,
                                    uint16_t *outWidth, int16_t *outHeight)
{
	if (pspemuIsPs1Mode() == 1)
		return popsHostSetScreenScale(mode, scale, outWidth, outHeight);
	return 0;
}

static void *menuDlgCapturePs1Screen(int *outHeight)
{
	int height = popsHostCaptureScreen(g_menuWorkBase, 1);

	*outHeight = height;
	g_menuCaptureHeight = height;
	g_menuCaptureWidth = 640;
	return g_menuWorkBase;
}

static int menuDlgGetPocsStatus(MenuPocsStatusFlags *outFlags)
{
	outFlags->pocsEnabled = (uint8_t)pspemuIsPocketStationEnabled();
	outFlags->pocsStandalone = (uint8_t)pspemuIsPocketStationInstalled();
	outFlags->pocsMounted = (uint8_t)remotePocsHasMountedMedia();
	outFlags->pocsAutoBoot = (uint8_t)popsHostGetPocsAutoBoot();
	return 0;
}

static SceBool menuDlgCheckPocsEntry(int slot, const char *entryName);
static int menuDlgSetPocsMedia(int mount);
static SceUID menuDlgScanSavedata(void);

static int __attribute__((noinline, noclone)) menuDlgOpenCompatDialog(const PspemuMenuSettings *settings)
{
	MenuCompatDialogParam param;
	const char *titleId;
	const char *listVer;
	int ret;

	sceClibMemset(&param, 0, sizeof(param));
	param.sdkVersion = MENU_COMMON_DIALOG_SDK_VERSION;
	param.commonMagic = (uint32_t)(uintptr_t)&param.commonParamHeader[0] + MENU_COMMON_DIALOG_MAGIC_BASE;
	param.settings = *settings;

	if (pspemuIsPs1Mode() == 1) {
		uint8_t *cfgBytes = (uint8_t *)&param.settings;
		const char *popsTitle = NULL;
		const char *popsDiscId = NULL;

		param.isPs1Mode = 1U;
		param.onGetPocsStatus = menuDlgGetPocsStatus;
		param.onSetPocsMedia = menuDlgSetPocsMedia;
		param.screenshotEnabled = g_menuScreenshotEnabled;
		param.currentDiscNumber = (uint8_t)(popsHostGetCurrentDisc() + 1);
		param.popsConfig = g_popsMenuConfig;

		pspemuGetPopsTitleStrings(&popsTitle, &popsDiscId);
		param.titleName = popsTitle;
		param.discId = popsDiscId;

		param.onSetPocsAutoBoot = menuDlgSetPocsAutoBoot;
		param.onImportVmcEntry = menuDlgImportVmcEntry;
		param.onDeleteVmcEntry = menuDlgDeleteVmcEntry;
		param.onCheckPocsEntry = menuDlgCheckPocsEntry;
		param.onTriggerScreenshot = menuDlgTriggerScreenshot;
		param.onCapturePs1Screen = menuDlgCapturePs1Screen;
		param.onSetPs1ScreenScale = menuDlgSetPs1ScreenScale;
		param.onScanSavedata = menuDlgScanSavedata;
		param.onGetVmcInfo = menuDlgGetVmcInfo;
		param.onMountVmc = menuDlgMountVmc;
		param.onListVmc = menuDlgListVmc;

		cfgBytes[6] = (uint8_t)(popsHostIsSingleDisc() == 0);
		cfgBytes[18] = (uint8_t)(popsHostIsAnalogDisabled() == 0);
		cfgBytes[7] = (uint8_t)popsHostGetDiscCount();
	} else {
		uint8_t *cfgBytes = (uint8_t *)&param.settings;

		param.isPs1Mode = 0U;
		cfgBytes[28] = (uint8_t)sceCompatAvailableColorSpaceSetting();
	}

	param.vshBranch = "vsh/release_branches/release_03.650";
	param.vshRev = "142676";
	param.compatBranch = "compat/release_branches/release_03.650";
	param.compatRev = "6665";

	titleId = pspemuCompatGetTitleId();
	listVer = pspemuCompatGetListVersion();
	snprintf(g_menuTitleBanner, sizeof(g_menuTitleBanner), "%s -%s-", titleId, listVer);
	param.titleBanner = g_menuTitleBanner;

	ret = SceCommonDialog_9FA21E09(&param);
	if (ret >= 0) {
		SceUInt64 now = sceKernelGetProcessTimeWide();
		ret = pspemuMenuSetImeDeadline(now);
	}
	return ret;
}

static SceBool menuDlgCheckPocsEntry(int slot, const char *entryName)
{
	int entryIdx;

	if (slot != 0)
		return SCE_FALSE;
	if (remotePocsHasMountedMedia() == 0)
		return SCE_FALSE;
	if (vmcManagerFlushAndVerify(0U) < 0)
		return SCE_FALSE;

	entryIdx = vmcManagerFindEntryIndex(0U, entryName);
	return (entryIdx > 0 && entryIdx == remotePocsGetInsertedSlot()) ? SCE_TRUE : SCE_FALSE;
}

static int menuDlgSetPocsMedia(int mount)
{
	int ret;

	if (remotePocsHasMountedMedia() == 0 && mount == 1) {
		ret = remotePocsMountMediaFromMenu();
		if (ret != 0)
			return ret;
	} else if (remotePocsHasMountedMedia() == 1 && mount == 0) {
		ret = remotePocsUnmountMediaFromMenu();
		if (ret != 0)
			return ret;
	} else {
		return MENU_ERR_INVALID_STATE;
	}

	popsHostReloadVmc(0U);
	if (popsHostGetPocsAutoBoot() != 0)
		popsHostSetPocsAutoBoot(1U);
	return ret;
}

static SceBool menuIsValidPs1SavedataDir(const char *name)
{
	int i;

	if (name[9] != '\0')
		return SCE_FALSE;
	for (i = 0; i < 4; i++) {
		if ((unsigned int)((uint8_t)name[i] - 'A') > 25U)
			return SCE_FALSE;
	}
	for (i = 4; i < 9; i++) {
		if ((unsigned int)((uint8_t)name[i] - '0') > 9U)
			return SCE_FALSE;
	}
	if (name[0] != 'S' && name[0] != 'P') {
		uint8_t regionChar = (uint8_t)name[3];

		if (name[0] != 'N' || name[1] != 'P')
			return SCE_FALSE;
		if ((unsigned int)(regionChar - 'E') > 1U &&
		    (unsigned int)(regionChar - 'I') > 1U)
			return SCE_FALSE;
	}
	return SCE_TRUE;
}

static SceUID menuDlgScanSavedata(void)
{
	SceUID dfd;
	unsigned int titleCount = 0U;
	unsigned int slotCount;
	SceIoDirent dirent;
	SceIoStat statBuf;
	char pathBuf[52];

	memset(g_menuVmcIndexMap, 0, sizeof(g_menuVmcIndexMap));
	g_menuVmcCount = 0U;

	dfd = sceIoDopen("ms0:PSP/SAVEDATA");
	if (dfd < 0)
		return dfd;

	slotCount = (pspemuIsPocketStationEnabled() != 0) ? 3U : 2U;

	while (sceIoDread(dfd, &dirent) > 0) {
		MenuSavedataTitleRecord *rec;
		SceUID sfoFd;
		SceSSize readBytes;
		SceBool isCurrentTitle;
		unsigned int writeIdx;

		if ((dirent.d_stat.st_mode & 0xF000) != 0x1000)
			continue;
		if (!menuIsValidPs1SavedataDir(dirent.d_name))
			continue;

		rec = ((MenuSavedataTitleRecord *)g_menuWorkBase) + titleCount;
		memcpy(rec->titleId, dirent.d_name, 10U);

		snprintf(pathBuf, 50U, "ms0:PSP/SAVEDATA/%s/PARAM.SFO", rec->titleId);
		sfoFd = sceIoOpen(pathBuf, SCE_O_RDONLY, 0);
		if (sfoFd >= 0) {
			readBytes = sceIoRead(sfoFd, g_menuSfoBuf, MENU_SFO_READ_SIZE);
			sceIoClose(sfoFd);

			if (readBytes == (SceSSize)MENU_SFO_READ_SIZE) {
				void *sfoHandle = NULL;
				int titleKeyIdx = 0;
				const char *keyName = "TITLE";
				char *titleVal = NULL;
				size_t usedSize = 0U;
				size_t maxSize = 0U;

				sfoParseBuffer(g_menuSfoBuf, MENU_SFO_READ_SIZE, &sfoHandle);
				if (sfoFindKey(sfoHandle, "TITLE", &titleKeyIdx) == 0 &&
				    sfoGetEntryMeta(sfoHandle, titleKeyIdx, &keyName, &usedSize, &maxSize) == 0 &&
				    maxSize >= usedSize && usedSize != 0U &&
				    sfoGetEntryValue(sfoHandle, titleKeyIdx, &titleVal) == 0) {
					strncpy(rec->titleName, titleVal, maxSize);
				}
				sfoDestroy(sfoHandle);
			}

			if (readBytes >= 0) {
				isCurrentTitle = (memcmp(pspemuCompatGetTitleId(), rec->titleId, 10U) == 0);
				writeIdx = isCurrentTitle ? 0U : slotCount;

				snprintf(pathBuf, 50U, "ms0:PSP/SAVEDATA/%s/SCEVMC%d.VMP", rec->titleId, 0);
				if (sceIoGetstat(pathBuf, &statBuf) == 0)
					g_menuVmcIndexMap[writeIdx++] = titleCount;

				snprintf(pathBuf, 50U, "ms0:PSP/SAVEDATA/%s/SCEVMC%d.VMP", rec->titleId, 1);
				if (sceIoGetstat(pathBuf, &statBuf) == 0)
					g_menuVmcIndexMap[writeIdx++] = titleCount + 0x10000U;

				if (isCurrentTitle && pspemuIsPocketStationEnabled() != 0)
					g_menuVmcIndexMap[writeIdx++] = titleCount + 0x30000U;

				if (isCurrentTitle || writeIdx != slotCount) {
					titleCount++;
					if (!isCurrentTitle)
						slotCount = writeIdx;
				} else {
					slotCount = writeIdx;
				}
			}
		}

		if (titleCount >= MENU_MAX_SAVEDATA_TITLES || slotCount >= MENU_MAX_VMC_SLOTS)
			break;
	}

	sceIoDclose(dfd);
	g_menuVmcCount = slotCount;
	return (SceUID)slotCount;
}

void *pspemuMenuCapturePocsScreen(void)
{
	g_menuCaptureWidth = MENU_DISPLAY_WIDTH;
	g_menuCaptureHeight = MENU_DISPLAY_HEIGHT;
	return g_menuWorkBase;
}

int pspemuMenuPreparePhotoExport(void)
{
	uint8_t *pngOut = g_menuWorkBase + MENU_PNG_SCRATCH_OFFSET;
	int pngLen;
	SceUID fd;
	SceSSize written;

	pngLen = popsPngEncodeRgba(pngOut, g_menuWorkBase,
	                           g_menuCaptureWidth, g_menuCaptureHeight,
	                           g_menuCaptureWidth, 0);
	if (pngLen < 0)
		return pngLen;

	pspemuPowerLock();
	fd = sceIoOpen(g_menuScreenshotPath,
	               SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0606);
	if (fd < 0) {
		pspemuPowerUnlock();
		return fd;
	}

	written = sceIoWrite(fd, pngOut, (SceSize)pngLen);
	sceIoClose(fd);
	pspemuPowerUnlock();
	return (int)written;
}

int pspemuMenuLoadVmcPreview(unsigned int slot)
{
	int ret = menuDlgMountVmc(pspemuCompatGetTitleId(), slot);

	if (ret >= 0)
		return vmcManagerListEntries(2U, g_menuVmcPreview);
	return ret;
}

int pspemuMenuImportVmcPreviewEntries(int entryCount)
{
	int i;

	for (i = 0; i < entryCount; i++) {
		if (g_menuVmcPreview[i].header[82] != 'M' &&
		    vmcManagerFlushAndVerify(0U) >= 0) {
			vmcManagerCopyEntry(0U, 2U, g_menuVmcPreview[i].entryName);
		}
	}
	popsHostReloadVmc(0U);
	return 0;
}

int pspemuMenuRunDialog(PspemuMenuSettings *settings)
{
	MenuCompatDialogResult dlgResult;
	int status;
	int ret;

	for (;;) {
		status = SceCommonDialog_FA26EB30();
		if (status == 0) {
			if (pspemuMenuLockDialog(2) != 0)
				return -1;
			ret = menuDlgOpenCompatDialog(settings);
			if (ret < 0) {
				if (ret == MENU_ERR_BUSY_DIALOG)
					return ret;
				pspemuTriggerFatalError(MENU_ERR_DIALOG_ABORTED);
				pspemuMenuUnlockDialog(status);
				return 0;
			}
		} else if (status == 1) {
			if (pspemuMenuPollDialog(0) < 0) {
				pspemuTriggerFatalError(MENU_ERR_DIALOG_ABORTED);
				pspemuMenuUnlockDialog(status);
				return 0;
			}
		} else if (status == 2) {
			memset(&dlgResult, 0, sizeof(dlgResult));
			remoteLowPriorityWaitIdle();
			if (SceCommonDialog_64F65B8D(&dlgResult) < 0) {
				pspemuTriggerFatalError(MENU_ERR_DIALOG_ABORTED);
				pspemuMenuUnlockDialog(status);
				return 0;
			}
			*settings = dlgResult.settings;
			SceCommonDialog_C2D6F7F2();
			if (pspemuMenuClearImeDeadline() < 0)
				pspemuTriggerFatalError(MENU_ERR_DIALOG_ABORTED);
			pspemuMenuUnlockDialog(status);
			return 0;
		}
		sceDisplayWaitVblankStart();
	}
}

int pspemuMenuDialogSetup(void)
{
	SceUID uid;

	if (pspemuIsPs1Mode() != 1)
		return 0;

	uid = sceKernelAllocMemBlock("ScePspemuMenuWork",
	                             0x0C208060U,
	                             MENU_WORK_BUF_SIZE, NULL);
	if (uid >= 0)
		return sceKernelGetMemBlockBase(uid, (void **)&g_menuWorkBase);
	return uid;
}

int pspemuMenuSetScreenshotEnabled(uint8_t enabled)
{
	g_menuScreenshotEnabled = enabled;
	return 0;
}

static void menuGxmDisplayCallback(const void *callbackData)
{
	SceDisplayFrameBuf frameBuf;
	void *const *fbPtr = (void *const *)callbackData;

	frameBuf.size = sizeof(frameBuf);
	frameBuf.base = *fbPtr;
	frameBuf.pitch = MENU_DISPLAY_STRIDE;
	frameBuf.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
	frameBuf.width = MENU_DISPLAY_WIDTH;
	frameBuf.height = MENU_DISPLAY_HEIGHT;
	sceDisplaySetFrameBuf(&frameBuf, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);
	sceDisplayWaitVblankStart();
}

int pspemuMenuSetImeDeadline(SceUInt64 deadline)
{
	g_menuImeDeadline = deadline;
	return 0;
}

int pspemuMenuClearImeDeadline(void)
{
	g_menuImeDeadline = UINT64_MAX;
	return 0;
}

int pspemuMenuPollDialog(int mode)
{
	SceCommonDialogUpdateParam updateParam;
	void *displayBuf;
	int ret;

	if (sceKernelGetProcessTimeWide() < g_menuImeDeadline)
		return 0;

	updateParam.renderTarget.colorSurfaceData = g_menuGxmFrameBufs[g_menuGxmFrontIndex];
	updateParam.renderTarget.depthSurfaceData = NULL;
	updateParam.renderTarget.width = MENU_DISPLAY_WIDTH;
	updateParam.renderTarget.height = MENU_DISPLAY_HEIGHT;
	updateParam.renderTarget.strideInPixels = MENU_DISPLAY_STRIDE;
	updateParam.renderTarget.colorFormat = PSPEMU_GXM_COLOR_FORMAT_U8U8U8U8_ABGR;
	updateParam.renderTarget.surfaceType = PSPEMU_GXM_COLOR_SURFACE_LINEAR;
	updateParam.displaySyncObject = g_menuGxmSyncObjs[g_menuGxmFrontIndex];

	if (pspemuIsPs1Mode() == 1) {
		if (mode == 0) {
			if (remotePocsIsActive() != 0 ||
			    (pspemuIsPocketStationInstalled() != 0 && remotePocsHasActiveSession() != 0)) {
				int slot = (remotePocsIsActive() != 0) ? popsHostGetPocsDisplaySlot() : 3;
				int channel = popsHostGetPocsDisplayChannel();
				int renderRes;

				updateParam.renderTarget.colorSurfaceData = remotePocsGetRenderBuffer();
				renderRes = remotePocsRenderFrame(slot, channel, -1);
				remotePocsWaitRenderComplete(renderRes);
			} else {
				popsHostCaptureScreen(g_menuGxmFrameBufs[g_menuGxmFrontIndex], 0);
			}
		} else {
			pspemuCompatHasFlag(1U);
			remoteMeAvcdecUpscale(g_menuGxmFrameBufs[g_menuGxmFrontIndex],
			                      pspemuGetActiveFrameBuffer());
		}
	} else {
		if (pspemuCompatHasFlag(1U) == 0 && mode == 0)
			sceCompatLCDCSync();
		remoteMeAvcdecUpscale(g_menuGxmFrameBufs[g_menuGxmFrontIndex],
		                      pspemuGetActiveFrameBuffer());
	}

	ret = sceCommonDialogUpdate(&updateParam);
	if (ret >= 0) {
		displayBuf = updateParam.renderTarget.colorSurfaceData;
		sceGxmDisplayQueueAddEntry(g_menuGxmSyncObjs[g_menuGxmBackIndex],
		                           g_menuGxmSyncObjs[g_menuGxmFrontIndex],
		                           &displayBuf);
		g_menuGxmBackIndex = g_menuGxmFrontIndex;
		g_menuGxmFrontIndex = (g_menuGxmFrontIndex + 1U) % 3U;
	}
	return ret;
}

int pspemuMenuDialogInit(void)
{
	SceGxmInitializeParams initParams;
	void *fbBase = NULL;
	SceUID uid;
	int ret;
	int i;

	g_menuGxmSyncObjs[0] = NULL;
	g_menuGxmSyncObjs[1] = NULL;
	g_menuGxmSyncObjs[2] = NULL;
	g_menuGxmFbMemBlock = 0;

	initParams.flags = 0U;
	initParams.displayQueueMaxPendingCount = 2U;
	initParams.displayQueueCallback = menuGxmDisplayCallback;
	initParams.displayQueueCallbackDataSize = sizeof(void *);
	initParams.parameterBufferSize = MENU_GXM_PARAM_BUF_SIZE;

	ret = sceGxmInitialize(&initParams);
	if (ret < 0)
		return ret;

	uid = sceKernelAllocMemBlock("ScePspemuDlgFrameBuffer",
	                             SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
	                             MENU_GXM_TOTAL_FB_BYTES, NULL);
	if (uid < 0) {
		g_menuGxmFrameBufs[0] = NULL;
		return -1;
	}

	g_menuGxmFbMemBlock = uid;
	if (sceKernelGetMemBlockBase(uid, &fbBase) != 0 ||
	    sceGxmMapMemory(fbBase, MENU_GXM_TOTAL_FB_BYTES, PSPEMU_GXM_MEMORY_ATTRIB_RW) != 0) {
		sceKernelFreeMemBlock(g_menuGxmFbMemBlock);
		g_menuGxmFrameBufs[0] = NULL;
		return -1;
	}

	g_menuGxmFrameBufs[0] = fbBase;
	if (fbBase == NULL)
		return -1;

	for (i = 0; i < 3; i++) {
		ret = sceGxmSyncObjectCreate(&g_menuGxmSyncObjs[i]);
		if (ret < 0)
			return ret;
		g_menuGxmFrameBufs[i] = (uint8_t *)fbBase + (uint32_t)i * MENU_GXM_FB_BYTES;
	}

	return 0;
}

int pspemuMenuLockDialog(int id)
{
	if (g_menuActiveDialogOwner == 0) {
		g_menuActiveDialogOwner = id;
		return 0;
	}
	return (g_menuActiveDialogOwner == id) ? 0 : -1;
}

int pspemuMenuUnlockDialog(int id)
{
	if (id != g_menuActiveDialogOwner)
		return -1;
	g_menuActiveDialogOwner = 0;
	return 0;
}

SceBool pspemuMenuIsOskDialogLocked(void)
{
	return (g_menuActiveDialogOwner == 1) ? SCE_TRUE : SCE_FALSE;
}
