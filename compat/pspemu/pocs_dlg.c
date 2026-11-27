/*
 * pocs_dlg.c: ScePspemu PocketStation common dialog, background PNG loader,
 * and photo import integration (0x81021efc..0x8102272f).
 */
#include "host_common.h"

#include <stdio.h>
#include <string.h>

#define POCS_COMMON_DIALOG_SDK_VERSION  0x03650000U
#define POCS_COMMON_DIALOG_MAGIC_BASE   0xC0D1A109U
#define POCS_BG_WORK_SIZE               0x400000U
#define POCS_BG_DECODE_OFFSET           0x200000U
#define POCS_BG_MAX_PNG_SIZE            0x200000
#define POCS_BG_WIDTH                   960U
#define POCS_BG_HEIGHT                  544U
#define POCS_BG_RGBA_BYTES              0x1FE000U
#define POCS_BG_STRIDE_BYTES            4096U
#define POCS_BG_ROW_BYTES               3840U
#define POCS_BG_TEXT_MAX_SIZE           1024

#define POCS_ERR_FILE_TOO_LARGE         ((int)0x8001001B)
#define POCS_ERR_IO                     ((int)0x80010005)
#define POCS_ERR_BAD_DIMENSIONS         ((int)0x8001002F)
#define POCS_ERR_DIALOG_BUSY            ((int)0x80020401)
#define POCS_ERR_DIALOG_FATAL           ((int)0x80010089)

typedef struct PocsDialogParam {
	uint32_t sdkVersion;
	uint8_t commonParamHeader[72];
	uint32_t commonMagic;
	int32_t dialogMode;
	uint32_t reservedA;
	void *popsMenuConfig;
	uint32_t reservedB;
	int32_t subMode;
	int32_t hasMedia;
	int32_t extraArg;
	uint8_t screenshotCompat;
	uint8_t padCompat[3];
	const uint16_t *serialWideStr;
	const char *bgPath;
	uint32_t reservedC;
	int32_t selectedBgType;
	uint32_t reservedD;
	int (*onTriggerScreenshot)(void (*doneCb)(void));
	void *(*onCapturePreview)(void);
	int (*onSelectBgType)(int bgType);
	int (*onGetPeerList)(void *outPeers, unsigned int *inOutCount);
	int (*onPocsCommand)(int cmd, int arg);
	int (*onModeChanged)(int mode);
	int (*onGetPeerInfo)(void *outPeer);
	int (*onGetConnectedPeerInfo)(void *outPeer);
	int (*onSetExitCallback)(int (*cb)(void));
	int (*onSetPeerRequestCallback)(int (*cb)(void));
	int (*onGetAdhocError)(int index);
} PocsDialogParam;

_Static_assert(sizeof(PocsDialogParam) == 0xB0U, "PocsDialogParam size");

typedef struct PocsDialogResult {
	int32_t resultCode;
	int32_t selectedBgType;
	int32_t actionCode;
} PocsDialogResult;

typedef struct PocsPhotoImportItem {
	char filePath[1024];
	uint8_t metadataHead[288];
	uint32_t imageWidth;
	uint32_t imageHeight;
	uint32_t imageFormat;
	uint8_t metadataTail[68];
} PocsPhotoImportItem;

_Static_assert(offsetof(PocsPhotoImportItem, imageWidth) == 0x520U, "PocsPhotoImportItem width offset");

typedef struct PocsPhotoImportParam {
	uint32_t sdkVersion;
	uint8_t commonParamHeader[72];
	uint32_t commonMagic;
	uint32_t mode;
	uint32_t visibleCategory;
	uint32_t itemCount;
	PocsPhotoImportItem *items;
	uint8_t reservedTail[32];
} PocsPhotoImportParam;

_Static_assert(sizeof(PocsPhotoImportParam) == 0x80U, "PocsPhotoImportParam size");

typedef struct PocsPhotoImportResult {
	int32_t result;
	int32_t importedCount;
	uint8_t reserved[32];
} PocsPhotoImportResult;

_Static_assert(sizeof(PocsPhotoImportResult) == 0x28U, "PocsPhotoImportResult size");

static PocsPhotoImportItem g_photoImportItem;
static char g_pocsBgPath[1024];
static uint8_t *g_pocsBgWorkBuf;
static SceKernelLwMutexWork g_pocsDialogNotStopMutex;
static int g_pocsPreviewState;
static int g_pocsBgLoaded;
static int g_pocsDialogNotStopFlag;
static int (*g_pocsDialogExitCallback)(void);

static int bgImageLoaderThread(SceSize args, void *argp);

/* 0x81021efc */
static int bgDialogSetExitCallback(int (*callback)(void))
{
	g_pocsDialogExitCallback = callback;
	return 0;
}

/* 0x81021f0c */
static int __attribute__((noinline)) bgImageSpawnLoaderThread(void)
{
	SceUID thid;
	int thStat = 0;
	int ret;

	thid = sceKernelCreateThread("ScePspemuReadBgImage",
	                             bgImageLoaderThread,
	                             96,
	                             0x5000U,
	                             0U,
	                             SCE_KERNEL_CPU_MASK_USER_0,
	                             NULL);
	if (thid < 0)
		return thid;

	ret = sceKernelStartThread(thid, 0U, NULL);
	if (ret == 0) {
		ret = sceKernelWaitThreadEnd(thid, &thStat, NULL);
		if (ret >= 0)
			ret = thStat;
	}
	return ret;
}

/* 0x81021f78 */
static int bgImageLoaderThread(SceSize args, void *argp)
{
	SceIoStat stat;
	int32_t imgWidth = 0;
	int32_t imgHeight = 0;
	int imgFormat = 0;
	int ret;

	(void)args;
	(void)argp;

	sceAppUtilPhotoMount();

	ret = sceIoGetstat(g_pocsBgPath, &stat);
	if (ret >= 0) {
		if (stat.st_size > POCS_BG_MAX_PNG_SIZE) {
			ret = POCS_ERR_FILE_TOO_LARGE;
		} else {
			SceUID fd = sceIoOpen(g_pocsBgPath, SCE_O_RDONLY, 0);
			if (fd < 0) {
				ret = fd;
			} else {
				SceSSize bytesRead = sceIoRead(fd, g_pocsBgWorkBuf, (SceSize)stat.st_size);
				sceIoClose(fd);
				if (bytesRead != stat.st_size) {
					ret = POCS_ERR_IO;
				} else {
					uint8_t *decodeBuf = &g_pocsBgWorkBuf[POCS_BG_DECODE_OFFSET];
					ret = scePngDec(decodeBuf,
					                POCS_BG_DECODE_OFFSET,
					                g_pocsBgWorkBuf,
					                POCS_BG_DECODE_OFFSET,
					                &imgWidth,
					                &imgHeight,
					                &imgFormat);
					if (ret >= 0) {
						if (imgWidth == (int32_t)POCS_BG_WIDTH && imgHeight == (int32_t)POCS_BG_HEIGHT) {
							void *bgDest = remotePocsGetBgTextureBuffer();
							if (imgFormat == 1056) {
								memcpy(bgDest, decodeBuf, POCS_BG_RGBA_BYTES);
								ret = 0;
							} else {
								ret = scePngConvertToRGBA(bgDest,
								                          decodeBuf,
								                          POCS_BG_WIDTH,
								                          POCS_BG_HEIGHT,
								                          imgFormat);
							}
							if (ret >= 0) {
								remotePocsCommitBgTexture();
								g_pocsBgLoaded = 1;
								ret = 0;
							}
						} else {
							ret = POCS_ERR_BAD_DIMENSIONS;
						}
					}
				}
			}
		}
	}

	sceAppUtilPhotoUmount();
	return ret;
}

/* 0x81022098 */
static int bgDialogOnModeChanged(int mode)
{
	if (mode != 0)
		return 0;
	return remotePocsGetAdhocStatus();
}

/* 0x810220a8 */
static int bgDialogOnSelectBgType(int bgType)
{
	if (g_pocsPreviewState != 2)
		g_pocsPreviewState = 0;

	if (bgType == 5) {
		if (g_pocsBgLoaded != 0)
			return popsHostSetPocsBgType(2U);
		return 0;
	}
	return popsHostSetPocsBgType((uint8_t)(bgType == 3));
}

/* 0x810220e0 */
static void *bgDialogCapturePreview(void)
{
	uint8_t *dstBuf = (uint8_t *)pspemuMenuCapturePocsScreen();

	if (g_pocsPreviewState == 0) {
		const uint8_t *srcBuf;
		uint32_t row;

		g_pocsPreviewState = 1;
		if (popsHostGetPocsBgType() == 2) {
			pocsRenderCurrentDisplay();
			srcBuf = (const uint8_t *)remotePocsGetRenderBuffer();
		} else {
			srcBuf = (const uint8_t *)remotePocsGetDefaultFrameBuffer();
		}

		for (row = 0; row < POCS_BG_HEIGHT; row++) {
			memcpy(&dstBuf[row * POCS_BG_ROW_BYTES],
			       &srcBuf[row * POCS_BG_STRIDE_BYTES],
			       POCS_BG_ROW_BYTES);
		}
	}
	return dstBuf;
}

/* 0x81022138 */
static int bgDialogOnPocsCommand(int cmd, int arg)
{
	switch (cmd) {
	case 0:
		sceKernelLockLwMutex(&g_pocsDialogNotStopMutex, 1, NULL);
		g_pocsDialogNotStopFlag = 1;
		sceKernelUnlockLwMutex(&g_pocsDialogNotStopMutex, 1);
		return 0;
	case 1:
		sceKernelLockLwMutex(&g_pocsDialogNotStopMutex, 1, NULL);
		g_pocsDialogNotStopFlag = 0;
		sceKernelUnlockLwMutex(&g_pocsDialogNotStopMutex, 1);
		return remotePocsStopAdhocIfIdle();
	case 2:
		return remotePocsSelectAdhocTarget((uint32_t)arg);
	case 3:
	case 5:
		return remotePocsCancelAdhocTarget();
	case 4:
		return remotePocsAcceptAdhocTarget();
	case 6:
		return remotePocsDisconnectAdhoc(0);
	default:
		return 0;
	}
}

/* 0x810221c8 */
int __attribute__((noinline)) bgImageEnsureLoaded(void)
{
	SceIoStat stat;
	char cfgPath[256];
	int ret;

	if (g_pocsBgLoaded != 0) {
		if (g_pocsPreviewState == 1) {
			g_pocsPreviewState = 0;
			return 1;
		}
		return 0;
	}

	snprintf(cfgPath, sizeof(cfgPath), "ux0:pspemu/PSP/SAVEDATA/%s/bg.txt",
	         pspemuCompatGetTitleId());
	ret = sceIoGetstat(cfgPath, &stat);
	if (ret < 0)
		return ret;
	if (stat.st_size > POCS_BG_TEXT_MAX_SIZE)
		return POCS_ERR_FILE_TOO_LARGE;

	{
		SceUID fd = sceIoOpen(cfgPath, SCE_O_RDONLY, 0);
		if (fd < 0)
			return fd;
		{
			SceSSize bytesRead = sceIoRead(fd, g_pocsBgPath, (SceSize)stat.st_size);
			sceIoClose(fd);
			if (pspemuValidatePathPrefix(g_pocsBgPath, bytesRead, POCS_BG_TEXT_MAX_SIZE, "photo0:") == 0)
				return POCS_ERR_IO;
			ret = bgImageSpawnLoaderThread();
			return (ret < 0) ? ret : 1;
		}
	}
}

/* 0x810222a4 */
static int __attribute__((noinline)) bgDialogRunModal(int mode, int subMode, int hasMedia,
                                                      int *inOutBgType, int *outAction,
                                                      int extraArg, int *outResult)
{
	for (;;) {
		int status = SceCommonDialog_500229F0();
		if (status == 0) {
			PocsDialogParam param;
			int ret;

			if (pspemuMenuLockDialog(2) != 0)
				return -1;

			sceClibMemset(&param, 0, sizeof(param));
			param.sdkVersion = POCS_COMMON_DIALOG_SDK_VERSION;
			param.commonMagic = POCS_COMMON_DIALOG_MAGIC_BASE +
			                    (uint32_t)(uintptr_t)&param.commonParamHeader;
			param.dialogMode = mode;
			param.subMode = subMode;
			param.hasMedia = hasMedia;
			param.extraArg = extraArg;
			param.popsMenuConfig = g_popsMenuConfig;
			param.selectedBgType = (inOutBgType != NULL) ? *inOutBgType : 0;

			bgImageEnsureLoaded();
			param.bgPath = (g_pocsBgLoaded != 0) ? g_pocsBgPath : NULL;
			param.onTriggerScreenshot = menuDlgTriggerScreenshot;
			param.onCapturePreview = bgDialogCapturePreview;
			param.onSelectBgType = bgDialogOnSelectBgType;
			param.screenshotCompat = (uint8_t)pspemuCompatHasFlag(0x80000000U);
			param.serialWideStr = remotePocsGetSerialWideStr();
			param.onGetPeerList = remotePocsGetAdhocPeerList;
			param.onPocsCommand = bgDialogOnPocsCommand;
			param.onModeChanged = bgDialogOnModeChanged;
			param.onGetPeerInfo = remotePocsGetPeerInfo;
			param.onGetConnectedPeerInfo = remotePocsGetConnectedPeerInfo;
			param.onSetExitCallback = bgDialogSetExitCallback;
			param.onSetPeerRequestCallback = remotePocsSetPeerRequestCallback;
			param.onGetAdhocError = remotePocsGetAdhocError;

			ret = SceCommonDialog_631DC45C(&param);
			if (ret >= 0) {
				SceUInt64 now = sceKernelGetProcessTimeWide();
				ret = pspemuMenuSetImeDeadline(now);
			}
			if (ret < 0) {
				if (ret == POCS_ERR_DIALOG_BUSY)
					return ret;
				pspemuTriggerFatalError(POCS_ERR_DIALOG_FATAL);
			}
		} else if (status == 1) {
			if (pspemuMenuPollDialog(0) < 0)
				pspemuTriggerFatalError(POCS_ERR_DIALOG_FATAL);
		} else if (status == 2) {
			PocsDialogResult dlgResult;

			dlgResult.resultCode = 0;
			dlgResult.selectedBgType = 0;
			dlgResult.actionCode = 0;

			remoteLowPriorityWaitIdle();
			if (SceCommonDialog_396D7B68(&dlgResult) < 0) {
				pspemuTriggerFatalError(POCS_ERR_DIALOG_FATAL);
			} else {
				SceCommonDialog_675248CA();
				if (pspemuMenuClearImeDeadline() < 0)
					pspemuTriggerFatalError(POCS_ERR_DIALOG_FATAL);
			}

			if (inOutBgType != NULL)
				*inOutBgType = dlgResult.selectedBgType;
			if (outAction != NULL)
				*outAction = dlgResult.actionCode;
			if (outResult != NULL)
				*outResult = dlgResult.resultCode;

			pspemuMenuUnlockDialog(2);
			return 0;
		}

		sceDisplayWaitVblankStart();
	}
}

/* 0x8102247c */
int bgDialogMapBgTypeToMode(int bgType)
{
	if (bgType == 3)
		return 1;
	if (bgType == 5)
		return 2;
	return 0;
}

/* 0x81022490 */
int pspemuBgDialogShowMenu(int mode, int hasMedia, int *inOutBgType, int *outAction)
{
	g_pocsPreviewState = (hasMedia != 0) ? 2 : 0;
	return bgDialogRunModal(mode, 0, hasMedia, inOutBgType, outAction, 0, NULL);
}

/* 0x810224bc */
int pspemuSettingsShowPocsFirstBootDialog(int *outButton)
{
	return bgDialogRunModal(4, 0, 0, NULL, NULL, 0, outButton);
}

/* 0x810224d8 */
int pspemuBgDialogShowAlert(int *outAction)
{
	return bgDialogRunModal(5, 0, 0, NULL, outAction, 0, NULL);
}

/* 0x810224f4 */
int pspemuBgDialogNotifyPocsEvent(int eventArg)
{
	int notStop;

	sceKernelLockLwMutex(&g_pocsDialogNotStopMutex, 1, NULL);
	notStop = g_pocsDialogNotStopFlag;
	sceKernelUnlockLwMutex(&g_pocsDialogNotStopMutex, 1);

	if (notStop == 0)
		remotePocsStopAdhocIfIdle();

	if (SceCommonDialog_500229F0() != 1)
		return bgDialogRunModal(5, 1, 0, NULL, NULL, eventArg, NULL);

	if (g_pocsDialogExitCallback != NULL)
		return g_pocsDialogExitCallback();
	return 0;
}

/* 0x8102255c */
int pspemuBgImageImportPhotoDialog(int32_t *outResult)
{
	PocsPhotoImportParam param;
	PocsPhotoImportResult dlgResult;
	char cfgPath[256];
	int ret;

	pspemuMenuSetImeDeadline(sceKernelGetProcessTimeWide());
	g_pocsBgPath[0] = '\0';

	sceClibMemset(&param, 0, sizeof(param));
	param.sdkVersion = POCS_COMMON_DIALOG_SDK_VERSION;
	param.commonMagic = POCS_COMMON_DIALOG_MAGIC_BASE +
	                    (uint32_t)(uintptr_t)&param.commonParamHeader;
	param.mode = 0U;
	param.visibleCategory = 1U;
	param.itemCount = 1U;
	param.items = &g_photoImportItem;

	ret = scePhotoImportDialogInit(&param);
	if (ret < 0)
		return ret;

	for (;;) {
		ret = scePhotoImportDialogGetStatus();
		if (ret == 0)
			break;

		if (ret == 2) {
			sceClibMemset(&dlgResult, 0, sizeof(dlgResult));
			ret = scePhotoImportDialogGetResult(&dlgResult);
			if (ret < 0)
				break;
			ret = scePhotoImportDialogTerm();
			if (ret < 0)
				break;

			*outResult = dlgResult.result;
			if (dlgResult.result == 0 && dlgResult.importedCount == 1) {
				memcpy(g_pocsBgPath, g_photoImportItem.filePath, sizeof(g_pocsBgPath));
				if (g_photoImportItem.imageWidth == POCS_BG_WIDTH &&
				    g_photoImportItem.imageHeight == POCS_BG_HEIGHT &&
				    g_photoImportItem.imageFormat == 2U) {
					SceUID fd;
					snprintf(cfgPath, sizeof(cfgPath),
					         "ux0:pspemu/PSP/SAVEDATA/%s/bg.txt",
					         pspemuCompatGetTitleId());
					pspemuPowerLock();
					fd = sceIoOpen(cfgPath,
					               SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
					               0x186);
					if (fd < 0) {
						pspemuPowerUnlock();
					} else {
						SceSSize written = sceIoWrite(fd, g_pocsBgPath,
						                              strlen(g_pocsBgPath) + 1U);
						sceIoClose(fd);
						pspemuPowerUnlock();
						if (written >= 0 && bgImageSpawnLoaderThread() >= 0) {
							remotePocsClearBgPreviewCache();
							popsHostSetPocsBgType(2U);
						}
					}
				}
			}
		}

		pspemuMenuPollDialog(1);
		sceDisplayWaitVblankStart();
	}

	return ret;
}

/* 0x810226cc */
int pspemuBgImageInit(void)
{
	SceUID uid;
	int ret;

	uid = sceKernelAllocMemBlock("ScePspemuPocketStationBgWork",
	                             SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
	                             POCS_BG_WORK_SIZE,
	                             NULL);
	if (uid < 0)
		return uid;

	ret = sceKernelGetMemBlockBase(uid, (void **)&g_pocsBgWorkBuf);
	if (ret != 0) {
		sceKernelFreeMemBlock(uid);
		return ret;
	}

	ret = sceKernelCreateLwMutex(&g_pocsDialogNotStopMutex,
	                             "ScePocketStationDialogNotStopFlag",
	                             0U, 0, NULL);
	return (ret < 0) ? ret : 0;
}
