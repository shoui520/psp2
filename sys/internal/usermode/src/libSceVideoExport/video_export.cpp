/*
 * SceVideoExport (vs0:sys/external/libSceVideoExport.suprx): exports a video file to the system's
 * video content through the media service (SceAvMediaSrv), a client of its IPMI service: the same
 * session code as SceMusicExport's. No Sony record names the library's files; the file's name is
 * ours. SceVideoExportEmpr_8493BF31 takes the parameters in one structure.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <kernel/sysmem/memblock.h>
#include <kernel/threadmgr.h>
#include "../../../../../vsh/lowlayer/src/ipmi/ipmi_error.h"
#include "video_export_internal.h"

/* the heap's memory block: user read-write, 4 KiB pages */
#define VIDEO_EXPORT_HEAP_MEMBLOCK_TYPE		0x0C20D060

/* how often and how long the library waits for the media service to start */
#define VIDEO_EXPORT_CONNECT_RETRY_COUNT	20
#define VIDEO_EXPORT_CONNECT_RETRY_USEC		1000000
#define VIDEO_EXPORT_POLL_USEC			16000

/* the state the callbacks set while the media service starts */
#define VIDEO_EXPORT_LAUNCH_STATE_NONE		0
#define VIDEO_EXPORT_LAUNCH_STATE_LAUNCHED	1
#define VIDEO_EXPORT_LAUNCH_STATE_READY		2

/* the application's launch by URI (SceAppMgr): the callbacks it reports to */
typedef struct VideoExportLaunchParam {
	SceSize size;
	SceInt32 reserved04;
	SceInt32 reserved08;
	SceUID appLaunchCallback;
	SceUID getParamCallback;
	SceInt32 reserved14;
	SceUInt8 reserved18[40];
} VideoExportLaunchParam;

extern "C" int sceAppMgrLaunchAppByUri2(const char *uri, const VideoExportLaunchParam *param);

static __attribute__((noinline)) void videoExportFree(VideoExportSession *session, void *ptr)
{
	sceClibMspaceFree(session->mspace, ptr);
}

static __attribute__((noinline)) void *videoExportAlloc(VideoExportSession *session, SceSize size)
{
	return sceClibMspaceMalloc(session->mspace, size);
}

/* releases the heap */
static void videoExportDestroyHeap(VideoExportSession *session)
{
	if (session->mspace != NULL) {
		sceClibMspaceDestroy(session->mspace);
		session->mspace = NULL;
	}
	if (session->memblockId != 0) {
		sceKernelFreeMemBlock(session->memblockId);
		session->memblockId = 0;
	}
}

static SceInt32 videoExportAppLaunchCallback(SceUID notifyId, SceInt32 notifyCount,
					     SceInt32 notifyArg, void *pCommon)
{
	if (notifyArg > 0)
		*(SceInt32 *)pCommon = VIDEO_EXPORT_LAUNCH_STATE_LAUNCHED;
	return 0;
}

static SceInt32 videoExportGetParamCallback(SceUID notifyId, SceInt32 notifyCount,
					    SceInt32 notifyArg, void *pCommon)
{
	if (notifyArg != 0) {
		if (notifyArg == (SceInt32)0x8080201B)
			*(SceInt32 *)pCommon = VIDEO_EXPORT_LAUNCH_STATE_NONE;
	} else {
		*(SceInt32 *)pCommon = VIDEO_EXPORT_LAUNCH_STATE_READY;
	}
	return 0;
}

/* gives the session its heap and connects it to the media service, starting the service if need be */
static int videoExportOpenSession(VideoExportSession *session, VideoExportCancelFunc cancelFunc,
				  void *userdata)
{
	void *pBase;
	SceUID memblockId;
	void *mspace;
	void *pBuffer;
	SceInt32 launchState;
	SceInt32 connectResult;
	SceUID appLaunchCallback;
	SceUID getParamCallback;
	VideoExportLaunchParam launchParam;
	int retry;
	int ret;
	int error;

	if (session->pClient != NULL)
		return 0;
	memblockId = 0;
	if (session->pWorkMemory != NULL) {
		pBase = session->pWorkMemory;
	} else {
		pBase = NULL;
		memblockId = sceKernelAllocMemBlock("SceVideoExportHeap",
						    VIDEO_EXPORT_HEAP_MEMBLOCK_TYPE,
						    VIDEO_EXPORT_HEAP_SIZE, NULL);
		if (memblockId < 0)
			return VIDEO_EXPORT_ERROR_NO_MEMORY;
		sceKernelGetMemBlockBase(memblockId, &pBase);
	}
	mspace = sceClibMspaceCreate(pBase, VIDEO_EXPORT_HEAP_SIZE);
	if (mspace == NULL) {
		if (memblockId != 0)
			sceKernelFreeMemBlock(memblockId);
		return VIDEO_EXPORT_ERROR_NO_MEMORY;
	}
	session->mspace = mspace;
	session->memblockId = memblockId;
	IPMI::Client::Config config;

	sceClibStrncpy(config.serviceName, "SceAvMediaSrv", sizeof(config.serviceName));
	config.maxAsyncCallInSize = 11904;
	config.maxAsyncCallOutSize = 11344;
	config.budget = IPMI::USER_BUDGET;
	pBuffer = sceClibMspaceMalloc(session->mspace, IPMI::Client::getBufferSize(&config));
	if (pBuffer == NULL) {
		videoExportDestroyHeap(session);
		return VIDEO_EXPORT_ERROR_NO_MEMORY;
	}
	session->pClientBuffer = sceClibMspaceMalloc(session->mspace, 11904);
	if (session->pClientBuffer == NULL) {
		sceClibMspaceFree(session->mspace, pBuffer);
		videoExportDestroyHeap(session);
		return VIDEO_EXPORT_ERROR_NO_MEMORY;
	}
	ret = IPMI::Client::create(&session->pClient, &config, session, pBuffer);
	if (ret != 0) {
		sceClibMspaceFree(session->mspace, pBuffer);
		sceClibMspaceFree(session->mspace, session->pClientBuffer);
		session->pClientBuffer = NULL;
		session->pClient = NULL;
		videoExportDestroyHeap(session);
		return ret;
	}
	launchState = VIDEO_EXPORT_LAUNCH_STATE_NONE;
	appLaunchCallback = sceKernelCreateCallback("SceVideoExportCallbackAppLaunch", 0,
						    videoExportAppLaunchCallback, &launchState);
	getParamCallback = sceKernelCreateCallback("SceVideoExportCallbackGetParam", 0,
						   videoExportGetParamCallback, &launchState);
	error = VIDEO_EXPORT_ERROR_INTERNAL;
	connectResult = VIDEO_EXPORT_ERROR_INTERNAL;
	if (appLaunchCallback > 0) {
		if (getParamCallback > 0) {
			retry = 0;
			for (;;) {
				if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
					error = VIDEO_EXPORT_ERROR_CANCELED;
					break;
				}
				error = session->pClient->connect(NULL, 0, &connectResult);
				if (error != IPMI_ERROR_SERVER_NOT_FOUND ||
				    retry >= VIDEO_EXPORT_CONNECT_RETRY_COUNT)
					break;
				retry++;
				if (launchState == VIDEO_EXPORT_LAUNCH_STATE_NONE) {
					sceClibMemset(&launchParam, 0, sizeof(launchParam));
					launchParam.reserved08 = -1;
					launchParam.appLaunchCallback = appLaunchCallback;
					launchParam.size = sizeof(launchParam);
					launchParam.reserved04 = 0;
					launchParam.getParamCallback = getParamCallback;
					launchParam.reserved14 = -1;
					sceAppMgrLaunchAppByUri2("avmedia_service:", &launchParam);
				}
				sceKernelDelayThread(VIDEO_EXPORT_CONNECT_RETRY_USEC);
				if (launchState == VIDEO_EXPORT_LAUNCH_STATE_READY)
					continue;
				sceKernelCheckCallback();
			}
		}
		sceKernelDeleteCallback(appLaunchCallback);
	}
	if (getParamCallback > 0)
		sceKernelDeleteCallback(getParamCallback);
	if (error != 0) {
		ret = VIDEO_EXPORT_ERROR_INTERNAL;
		switch (error) {
		case VIDEO_EXPORT_ERROR_CANCELED:
			ret = VIDEO_EXPORT_ERROR_CANCELED;
			break;
		case IPMI_ERROR_SERVER_NOT_FOUND:
			ret = VIDEO_EXPORT_ERROR_SERVER_DOWN;
			break;
		case IPMI_ERROR_CONNECTION_REFUSED:
			ret = connectResult;
			break;
		default:
			break;
		}
		session->pClient->destroy();
		sceClibMspaceFree(session->mspace, pBuffer);
		sceClibMspaceFree(session->mspace, session->pClientBuffer);
		session->pClientBuffer = NULL;
		session->pClient = NULL;
		videoExportDestroyHeap(session);
		return ret;
	}
	session->requestId = -1;
	return 0;
}

/* disconnects and frees what the session took */
static int videoExportCloseSession(VideoExportSession *session)
{
	void *pClientBuffer;

	if (session->pClient == NULL)
		return VIDEO_EXPORT_ERROR_INTERNAL;
	session->pClient->disconnect();
	session->pClient->destroy();
	sceClibMspaceFree(session->mspace, session->pClient);
	pClientBuffer = session->pClientBuffer;
	session->pClient = NULL;
	if (pClientBuffer != NULL) {
		sceClibMspaceFree(session->mspace, pClientBuffer);
		session->pClientBuffer = NULL;
	}
	videoExportDestroyHeap(session);
	return 0;
}

static __attribute__((noinline)) int videoExportStart(VideoExportSession *session, SceUInt32 count,
			    const VideoExportInputParam *pItems, SceUInt32 mode)
{
	return videoExportIpcStart(session->pClient, &session->requestId, NULL, count, pItems, mode);
}

static __attribute__((noinline)) int videoExportGetResult(VideoExportSession *session, SceUInt32 *pCount,
				VideoExportOutputParam *pResults)
{
	if (session->requestId == -1)
		return IPMI_ERROR_NOT_INITIALIZED;
	return videoExportIpcGetResult(session->pClient, session->requestId, pCount, pResults);
}

static __attribute__((noinline)) int videoExportCancel(VideoExportSession *session)
{
	return videoExportIpcCancel(session->pClient);
}

static __attribute__((noinline)) int videoExportGetProgress(VideoExportSession *session, SceInt32 *pProgress)
{
	return videoExportIpcGetProgress(session->pClient, pProgress);
}

/* starts the export of the items and waits for its end, reporting its progress */
static int videoExportRun(VideoExportSession *session, SceInt32 count,
			  const VideoExportInputParam *pItems, SceUInt32 mode,
			  VideoExportCancelFunc cancelFunc,
			  VideoExportProgressFunc progressFunc, void *userdata,
			  VideoExportOutputParam *pOut)
{
	VideoExportOutputParam *pResults;
	SceUInt32 resultCount;
	SceInt32 progress;
	SceInt32 lastProgress;
	int canceled;
	int ret;

	pResults = (VideoExportOutputParam *)videoExportAlloc(session,
						     sizeof(VideoExportOutputParam) * VIDEO_EXPORT_MAX_ITEMS);
	if (pResults == NULL)
		return VIDEO_EXPORT_ERROR_NO_MEMORY;
	ret = videoExportStart(session, count, pItems, mode);
	if (ret == 0) {
		canceled = 0;
		lastProgress = 0;
		do {
			if (cancelFunc != NULL && canceled == 0 && cancelFunc(userdata) != 0) {
				videoExportCancel(session);
				canceled = 1;
			}
			if (progressFunc != NULL) {
				progress = 0;
				if (videoExportGetProgress(session, &progress) == 0 &&
				    lastProgress != progress) {
					if (session->progressMode != 0) {
						if (session->progressMode == 1)
							progressFunc(userdata, progress);
					} else {
						progressFunc(userdata,
							     (SceInt32)((float)progress / (float)count * 100.0f),
							     progress);
					}
					lastProgress = progress;
				}
			}
			sceKernelDelayThread(VIDEO_EXPORT_POLL_USEC);
			resultCount = count;
			ret = videoExportGetResult(session, &resultCount, pResults);
		} while (ret == IPMI_ERROR_WOULD_BLOCK);
		if (ret == 0)
			sceClibMemcpy_safe(pOut, pResults, sizeof(VideoExportOutputParam) * resultCount);
	}
	if (pResults != NULL)
		videoExportFree(session, pResults);
	return ret;
}

extern "C" SceInt32 sceVideoExportFromFile(const VideoExportInputParam *pItems, SceInt32 count,
					   void *workMemory, VideoExportCancelFunc cancelFunc,
					   VideoExportProgressFunc progressFunc, void *userdata,
					   SceInt32 mode, VideoExportOutputParam *pResults)
{
	VideoExportSession session;
	const VideoExportInputParam *pItem;
	const char *pByte;
	VideoExportOutputParam *pResult;
	SceUInt32 i;
	int j;
	int ret;

	if (pItems == NULL) {
		ret = VIDEO_EXPORT_ERROR_PARAM;
		goto end;
	}
	if (count == 0 || (SceUInt32)count > VIDEO_EXPORT_MAX_ITEMS) {
		ret = VIDEO_EXPORT_ERROR_PARAM;
		goto end;
	}
	if (mode <= 1) {
		if (mode < 0) {
			ret = VIDEO_EXPORT_ERROR_PARAM;
			goto end;
		}
	} else if (mode > 2) {
		ret = VIDEO_EXPORT_ERROR_PARAM;
		goto end;
	}
	pItem = pItems;
	for (i = 0; i < (SceUInt32)count; i++) {
		if (pItem->mode != 0 && pItem->mode != 1) {
			ret = VIDEO_EXPORT_ERROR_PARAM;
			goto end;
		}
		pByte = (const char *)pItem;
		for (j = 0; j < 60; j++) {
			if (pByte[offsetof(VideoExportInputParam, reserved)] != 0) {
				ret = VIDEO_EXPORT_ERROR_PARAM;
				goto end;
			}
			pByte++;
		}
		pItem++;
	}
	if (pResults != NULL) {
		pResult = pResults;
		for (i = count; i != 0; i--) {
			pResult->contentIdLow = -1;
			pResult->contentIdHigh = -1;
			sceClibMemset(pResult, 0, VIDEO_EXPORT_MAX_FS_PATH);
			pResult++;
		}
	}
	session.pClient = NULL;
	session.requestId = -1;
	session.mspace = NULL;
	session.memblockId = 0;
	session.pClientBuffer = NULL;
	session.pWorkMemory = workMemory;
	session.progressMode = 0;
	ret = videoExportOpenSession(&session, cancelFunc, userdata);
	if (ret == 0) {
		int result = videoExportRun(&session, count, pItems, mode, cancelFunc, progressFunc,
					    userdata, pResults);

		videoExportCloseSession(&session);
		ret = result;
	}
end:
	return ret;
}

extern "C" SceInt32 SceVideoExportEmpr_8493BF31(const VideoExportEmprInput *pInput,
						VideoExportEmprOutput *pOutput)
{
	VideoExportSession session;
	VideoExportInputParam *pItem;
	VideoExportOutputParam *pResult;
	const char *pByte;
	int i;
	int ret;
	int result;

	if (pInput == NULL || pOutput == NULL)
		return VIDEO_EXPORT_ERROR_PARAM;
	if (pInput->path[0] == '\0')
		return VIDEO_EXPORT_ERROR_PARAM;
	if (sceClibStrnlen(pInput->path, VIDEO_EXPORT_MAX_FS_PATH) >= VIDEO_EXPORT_MAX_FS_PATH)
		return VIDEO_EXPORT_ERROR_PARAM;
	pByte = (const char *)pInput;
	for (i = 0; i < 64; i++) {
		if (pByte[offsetof(VideoExportEmprInput, reserved0)] != 0)
			return VIDEO_EXPORT_ERROR_PARAM;
		pByte++;
	}
	if (pInput->mode > 1 || pInput->workMemory == NULL)
		return VIDEO_EXPORT_ERROR_PARAM;
	if (pInput->workMemorySize < VIDEO_EXPORT_HEAP_SIZE)
		return VIDEO_EXPORT_ERROR_PARAM;
	pByte = (const char *)pInput;
	for (i = 0; i < 64; i++) {
		if (pByte[offsetof(VideoExportEmprInput, reserved1)] != 0)
			return VIDEO_EXPORT_ERROR_PARAM;
		pByte++;
	}
	sceClibMemset(pOutput, 0, sizeof(*pOutput));
	session.pClient = NULL;
	pOutput->reserved0 = -1;
	pOutput->reserved1 = -1;
	session.requestId = -1;
	session.mspace = NULL;
	session.memblockId = 0;
	session.pClientBuffer = NULL;
	session.pWorkMemory = pInput->workMemory;
	session.progressMode = 1;
	ret = videoExportOpenSession(&session, pInput->cancelFunc, pInput->userdata);
	if (ret != 0)
		return ret;
	pItem = (VideoExportInputParam *)videoExportAlloc(&session, sizeof(VideoExportInputParam));
	if (pItem != NULL) {
		sceClibMemset(pItem, 0, sizeof(*pItem));
		sceClibStrncpy(pItem->path, pInput->path, VIDEO_EXPORT_MAX_FS_PATH);
	}
	pResult = (VideoExportOutputParam *)videoExportAlloc(&session, sizeof(VideoExportOutputParam));
	if (pResult != NULL)
		sceClibMemset(pResult, 0, sizeof(*pResult));
	result = VIDEO_EXPORT_ERROR_NO_MEMORY;
	if (pItem != NULL && pResult != NULL)
		result = videoExportRun(&session, 1, pItem, 0, pInput->cancelFunc, pInput->progressFunc,
					pInput->userdata, pResult);
	videoExportCloseSession(&session);
	if (result == 0) {
		sceClibStrncpy(pOutput->path, pResult->path, VIDEO_EXPORT_MAX_FS_PATH);
		pOutput->reserved0 = pResult->contentIdLow;
		pOutput->reserved1 = pResult->contentIdHigh;
	}
	return result;
}
