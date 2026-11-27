/*
 * remote_low_priority.c: asynchronous media export (photo/music/video
 * promotion) queue and PS-button shell lock management for ScePspemu.
 */
#include "host_common.h"

#include <libScePhotoExport/photo_export.h>
#include <libSceMusicExport/music_export.h>
#include <libSceVideoExport/video_export.h>
#include <stdio.h>
#include <string.h>

#define LOWPRIO_QUEUE_CAPACITY     32
#define LOWPRIO_PATH_STRIDE        4096
#define LOWPRIO_EXPORT_WORK_SIZE   65444

#define LOWPRIO_MEDIA_NONE         (-1)
#define LOWPRIO_MEDIA_PHOTO        0
#define LOWPRIO_MEDIA_MUSIC        1
#define LOWPRIO_MEDIA_VIDEO        2

int                  g_lowPrioQueueHead;
SceUID               g_lowPrioThreadId;
int                (*g_lowPrioPhotoDoneCb)(int result);
SceUID               g_lowPrioSemaId;
char                 g_lowPrioPrefixBuf[256];
SceKernelLwMutexWork g_lowPrioMutex;
char                 g_lowPrioQueue[LOWPRIO_QUEUE_CAPACITY][LOWPRIO_PATH_STRIDE];
int                  g_lowPrioPsLockCount;
int                  g_lowPrioQueueTail;

static const char g_lowPrioEmptyStr[] = "";

static int __attribute__((noinline, noclone)) lowPrioCompareRootSubdir(const char *path, const char *subdir)
{
	int len = snprintf(g_lowPrioPrefixBuf, sizeof(g_lowPrioPrefixBuf), "%s%s", g_msfsRootPrefix, subdir);
	return memcmp(path, g_lowPrioPrefixBuf, (size_t)len);
}

static int __attribute__((noinline, noclone)) lowPrioEndsWithIgnoreCase(
	const char *str, int strLen, const char *suffix, int suffixLen)
{
	if (suffixLen <= 0)
		return 1;

	const char *tail = str + strLen - suffixLen;
	for (int i = 0; i < suffixLen; i++) {
		unsigned char sCh = (unsigned char)suffix[i];
		unsigned char tCh = (unsigned char)tail[i];
		unsigned char upper = (sCh >= 'a' && sCh <= 'z') ? (unsigned char)(sCh - 32U) : sCh;
		if (tCh != upper && tCh != sCh)
			return 0;
	}
	return 1;
}

static int __attribute__((noinline, noclone)) lowPrioClassifyMediaPath(const char *path)
{
	int len = (int)strlen(path);
	if (len <= 3)
		return LOWPRIO_MEDIA_NONE;

	if (lowPrioEndsWithIgnoreCase(path, len, ".jpg", 4) != 0)
		return LOWPRIO_MEDIA_PHOTO;
	if ((len > 4 && lowPrioEndsWithIgnoreCase(path, len, ".jpeg", 5) != 0) ||
	    lowPrioEndsWithIgnoreCase(path, len, ".gif", 4) != 0) {
		return LOWPRIO_MEDIA_PHOTO;
	}
	if (lowPrioEndsWithIgnoreCase(path, len, ".png", 4) != 0)
		return LOWPRIO_MEDIA_PHOTO;
	if (lowPrioEndsWithIgnoreCase(path, len, ".bmp", 4) != 0 ||
	    (len > 4 && lowPrioEndsWithIgnoreCase(path, len, ".tiff", 5) != 0)) {
		return LOWPRIO_MEDIA_PHOTO;
	}

	if (lowPrioEndsWithIgnoreCase(path, len, ".mp3", 4) != 0 ||
	    lowPrioEndsWithIgnoreCase(path, len, ".mp4", 4) != 0 ||
	    lowPrioEndsWithIgnoreCase(path, len, ".wav", 4) != 0 ||
	    lowPrioEndsWithIgnoreCase(path, len, ".wma", 4) != 0 ||
	    lowPrioEndsWithIgnoreCase(path, len, ".aa3", 4) != 0 ||
	    lowPrioEndsWithIgnoreCase(path, len, ".m4a", 4) != 0 ||
	    lowPrioEndsWithIgnoreCase(path, len, ".3gp", 4) != 0) {
		return LOWPRIO_MEDIA_MUSIC;
	}
	if (lowPrioEndsWithIgnoreCase(path, len, ".m4v", 4) != 0)
		return LOWPRIO_MEDIA_VIDEO;
	return LOWPRIO_MEDIA_NONE;
}

static int lowPrioShellEventHandler(int result, int mode, int type, void *userArg)
{
	(void)type;

	if (mode == 1) {
		*(int *)userArg = (result == 0) ? 1 : 0;
		return sceShellUtilRegisterEventHandler(NULL, NULL);
	}
	return result;
}

static int __attribute__((noinline, noclone)) lowPrioUnlock(void)
{
	int ret = sceKernelUnlockLwMutex(&g_lowPrioMutex, 1);
	if (ret < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

static int __attribute__((noinline, noclone)) lowPrioLock(void)
{
	int ret = sceKernelLockLwMutex(&g_lowPrioMutex, 1, NULL);
	if (ret < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

static int __attribute__((noinline, noclone)) lowPrioReleasePsButtonLock(void)
{
	lowPrioLock();
	if (--g_lowPrioPsLockCount == 0)
		sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
	lowPrioUnlock();
	return 0;
}

static int __attribute__((noinline, noclone)) lowPrioEnqueuePath(const char *path)
{
	int nextTail = (g_lowPrioQueueTail + 1) % LOWPRIO_QUEUE_CAPACITY;

	lowPrioLock();
	int head = g_lowPrioQueueHead;
	lowPrioUnlock();

	if (nextTail == head)
		return -1;

	strncpy(g_lowPrioQueue[g_lowPrioQueueTail], path, LOWPRIO_PATH_STRIDE);
	int ret = sceKernelSignalSema(g_lowPrioSemaId, 1);
	if (ret < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	g_lowPrioQueueTail = nextTail;
	return ret;
}

int remoteLowPriorityLockPsButton(void)
{
	int locked = 0;

	sceShellUtilRegisterEventHandler(lowPrioShellEventHandler, &locked);
	sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);

	for (int retries = 100; retries > 0; retries--) {
		sceKernelDelayThread(10000U);
		if (locked != 0)
			break;
	}
	return locked;
}

static int __attribute__((noinline, noclone)) lowPrioAcquirePsButtonLock(void)
{
	lowPrioLock();
	if (g_lowPrioPsLockCount == 0)
		remoteLowPriorityLockPsButton();
	g_lowPrioPsLockCount++;
	lowPrioUnlock();
	return 0;
}

static int __attribute__((noinline, noclone)) lowPrioExportPhoto(const char *path)
{
	SceInt32 contentId[2] = { 0, 0 };
	ScePhotoExportParam param;
	uint8_t workMemory[65460];

	param.version = PSPEMU_SDK_VERSION;
	param.photoTitle = (const SceChar8 *)g_lowPrioEmptyStr;
	param.gameTitle = (const SceChar8 *)g_lowPrioEmptyStr;
	param.gameComment = (const SceChar8 *)g_lowPrioEmptyStr;
	memset(param.reserved, 0, sizeof(param.reserved));

	lowPrioAcquirePsButtonLock();
	int ret = ScePhotoExport_76640642((const SceChar8 *)path, &param, 0, workMemory, NULL, NULL, contentId);
	lowPrioReleasePsButtonLock();
	return ret;
}

static int __attribute__((noinline, noclone)) lowPrioExportMusic(const char *path)
{
	MusicExportResult outResult;
	MusicExportItem inItem;
	uint8_t workMemory[LOWPRIO_EXPORT_WORK_SIZE];

	memset(&inItem, 0, sizeof(inItem));
	strncpy(inItem.path, path, 256U);

	lowPrioAcquirePsButtonLock();
	int ret = SceMusicExport_D1011570(&inItem, 1U, workMemory, NULL, NULL, NULL, 0, &outResult);
	sceIoRemove(path);
	lowPrioReleasePsButtonLock();
	return ret;
}

static int __attribute__((noinline, noclone)) lowPrioExportVideo(const char *path)
{
	VideoExportOutputParam outResult;
	VideoExportInputParam inItem;
	uint8_t workMemory[LOWPRIO_EXPORT_WORK_SIZE];

	memset(&inItem, 0, sizeof(inItem));
	strncpy(inItem.path, path, 256U);

	lowPrioAcquirePsButtonLock();
	int ret = sceVideoExportFromFile(&inItem, 1, workMemory, NULL, NULL, NULL, 0, &outResult);
	lowPrioReleasePsButtonLock();
	return ret;
}

static int lowPrioPromoteThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	while (sceKernelWaitSema(g_lowPrioSemaId, 1, NULL) >= 0) {
		const char *path = g_lowPrioQueue[g_lowPrioQueueHead];
		int kind = lowPrioClassifyMediaPath(path);

		switch (kind) {
		case LOWPRIO_MEDIA_MUSIC:
			if (memcmp(path, "ux0:temp/pspemu/", 16U) == 0)
				lowPrioExportMusic(path);
			break;
		case LOWPRIO_MEDIA_VIDEO:
			if (lowPrioCompareRootSubdir(path, "VIDEO/") == 0)
				lowPrioExportVideo(path);
			break;
		case LOWPRIO_MEDIA_PHOTO:
			if (lowPrioCompareRootSubdir(path, "PSP/SCREENSHOT/") == 0 ||
			    lowPrioCompareRootSubdir(path, "PICTURE/") == 0 ||
			    lowPrioCompareRootSubdir(path, "PSP/PHOTO/") == 0) {
				lowPrioExportPhoto(path);
			} else if (memcmp(path, "ux0:temp/pspemu/", 16U) == 0 && g_lowPrioPhotoDoneCb != NULL) {
				sceKernelDelayThread(500000U);
				int ret = pspemuMenuPreparePhotoExport();
				if (ret >= 0)
					ret = lowPrioExportPhoto(path);
				g_lowPrioPhotoDoneCb(ret);
				g_lowPrioPhotoDoneCb = NULL;
			}
			break;
		default:
			break;
		}

		lowPrioLock();
		g_lowPrioQueueHead = (g_lowPrioQueueHead + 1) % LOWPRIO_QUEUE_CAPACITY;
		lowPrioUnlock();
	}

	pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
}

int remoteLowPriorityUnlockPsButton(void)
{
	sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
	return 0;
}

int remoteLowPriorityIsPromotablePath(const char *path)
{
	return (lowPrioClassifyMediaPath(path) != LOWPRIO_MEDIA_NONE) ? 1 : 0;
}

int remoteLowPriorityInit(void)
{
	g_lowPrioPsLockCount = 0;
	g_lowPrioPhotoDoneCb = NULL;

	int ret = sceKernelCreateLwMutex(&g_lowPrioMutex, "ScePspemuLockForPromote", 0, 0, NULL);
	if (ret < 0)
		return ret;

	for (int i = 0; i < LOWPRIO_QUEUE_CAPACITY; i++)
		memset(g_lowPrioQueue[i], 0, LOWPRIO_PATH_STRIDE);

	g_lowPrioQueueHead = 0;
	g_lowPrioQueueTail = 0;
	g_lowPrioSemaId = sceKernelCreateSema("ScePspemuPromoteRequest", 0, 0, LOWPRIO_QUEUE_CAPACITY, NULL);
	if (g_lowPrioSemaId <= 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	g_lowPrioThreadId = sceKernelCreateThread(
		"ScePspemuPromoteRequest",
		lowPrioPromoteThread,
		95,
		0x11000U,
		0,
		0x10000,
		NULL);
	if (g_lowPrioThreadId < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	ret = sceKernelStartThread(g_lowPrioThreadId, 0, NULL);
	if (ret != 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

int remoteLowPriorityEnqueuePromote(const char *path)
{
	return lowPrioEnqueuePath(path);
}

int remoteLowPriorityEnqueuePromoteWithCallback(const char *path, int (*doneCb)(int result))
{
	g_lowPrioPhotoDoneCb = doneCb;
	int ret = lowPrioEnqueuePath(path);
	if (ret < 0) {
		g_lowPrioPhotoDoneCb(ret);
		g_lowPrioPhotoDoneCb = NULL;
	}
	return ret;
}

int remoteLowPriorityWaitIdle(void)
{
	int ret = 0;

	while (g_lowPrioPhotoDoneCb != NULL)
		ret = sceDisplayWaitVblankStartCB();
	return ret;
}
