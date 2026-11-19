/*
 * ScePhotoExport (vs0:sys/external/libScePhotoExport.suprx): exports a photo to the system's
 * photo content. The library opens a session with the media service (SceAvMediaSrv), sends the
 * data in pieces or the file's path, polls the service until it is done and returns where the
 * content is. Built by Sony with SNC 3.57 in C. No debug record names its files; the files are
 * named by what they hold (ours).
 */

#include <stddef.h>
#include <scetypes.h>
#include <string.h>
#include <photo_export.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <kernel/sysmem/memblock.h>
#include <kernel/threadmgr.h>
#include "../../../../../vsh/lowlayer/src/ipmi/ipmi_error.h"
#include "photo_export_internal.h"

/* the heap's memory block: user read-write, 4 KiB pages */
#define PHOTO_EXPORT_HEAP_MEMBLOCK_TYPE		0x0C20D060
#define PHOTO_EXPORT_HEAP_SIZE			0x10000

/* how often and how long the library waits for the media service to start */
#define PHOTO_EXPORT_CONNECT_RETRY_COUNT	20
#define PHOTO_EXPORT_CONNECT_RETRY_USEC		1000000
#define PHOTO_EXPORT_POLL_USEC			16000

/* the size of a piece of the file sent to the media service */
#define PHOTO_EXPORT_PIECE_SIZE			0x4000

/* the SDK version from which the reserved range of the parameters is checked */
#define PHOTO_EXPORT_CHECK_RESERVED_VERSION	0x1500000

/* the state the callbacks set while the media service starts */
#define PHOTO_EXPORT_LAUNCH_STATE_NONE		0
#define PHOTO_EXPORT_LAUNCH_STATE_LAUNCHED	1
#define PHOTO_EXPORT_LAUNCH_STATE_READY		2

/* the formats the library tells apart */
#define PHOTO_EXPORT_FORMAT_UNKNOWN		0
#define PHOTO_EXPORT_FORMAT_PNG			1
#define PHOTO_EXPORT_FORMAT_JPEG		2
#define PHOTO_EXPORT_FORMAT_TIFF		3
#define PHOTO_EXPORT_FORMAT_BMP			4
#define PHOTO_EXPORT_FORMAT_GIF			5

/* the errors the library answers with */
#define PHOTO_ERROR_PARAM		((SceInt32)SCE_PHOTO_EXPORT_ERROR_PARAM)
#define PHOTO_ERROR_FILE_NOT_FOUND	((SceInt32)SCE_PHOTO_EXPORT_ERROR_FILE_NOT_FOUND)
#define PHOTO_ERROR_NO_MEMORY		((SceInt32)SCE_PHOTO_EXPORT_ERROR_NO_MEMORY)
#define PHOTO_ERROR_SERVER_DOWN		((SceInt32)SCE_PHOTO_EXPORT_ERROR_SERVER_DOWN)
#define PHOTO_ERROR_NOT_SUPPORTED	((SceInt32)SCE_PHOTO_EXPORT_ERROR_NOT_SUPPORTED_FORMAT)
#define PHOTO_ERROR_CANCELED		((SceInt32)SCE_PHOTO_EXPORT_ERROR_CANCELED)
#define PHOTO_ERROR_INTERNAL		((SceInt32)SCE_PHOTO_EXPORT_ERROR_INTERNAL)
#define PHOTO_ERROR_INVALID_PATH	((SceInt32)SCE_PHOTO_EXPORT_ERROR_INVALID_PATH)
#define PHOTO_ERROR_INVALID_DATA	((SceInt32)SCE_PHOTO_EXPORT_ERROR_INVALID_DATA)

/* the application's launch by URI (SceAppMgr): the callbacks it reports to */
typedef struct PhotoExportLaunchParam {
	SceSize size;
	SceInt32 reserved04;
	SceInt32 reserved08;
	SceUID appLaunchCallback;
	SceUID getParamCallback;
	SceInt32 reserved14;
	SceUInt8 reserved18[40];
} PhotoExportLaunchParam;

extern "C" SceUInt32 SceLibKernel_9F793F84(void);
extern "C" SceUInt64 sceKernelGetProcessTimeWide(void);
extern "C" int sceAppMgrLaunchAppByUri2(const char *uri, const PhotoExportLaunchParam *param);
extern "C" int sceAppMgrSendNotificationRequest(const void *request, int flags);


/* the paths the media service does not take (photo0:, vs0:, vd0:; the batch checks the last) */
static const char *const photoExportForbiddenPrefixes[4] = { "photo0:", "vs0:", "vd0:", "photo0:" };


static __attribute__((noinline)) SceInt32 photoExportIpcSendData(IPMI::Client *pClient, void *pData, SceInt32 size,
				       SceInt32 index, SceUInt8 last, SceInt64 total, SceInt32 reserved,
				       const char *pPath, SceInt32 reserved2, const char *pPhotoTitle,
				       const char *pGameTitle, const char *pGameComment,
				       const char *pFilePath);
static __attribute__((noinline)) SceInt32 photoExportIpcStart(IPMI::Client *pClient, SceInt32 *pRequestId,
				    const IPMI::EventFlagInfo *pEventFlag, SceInt32 count, void *pData);
static __attribute__((noinline)) SceInt32 photoExportIpcGetResult(IPMI::Client *pClient, SceInt32 requestId,
					SceInt32 *pCount, void *pData);
static __attribute__((noinline)) SceInt32 photoExportIpcCancel(IPMI::Client *pClient);
static __attribute__((noinline)) SceInt32 photoExportIpcAbort(IPMI::Client *pClient);
static __attribute__((noinline)) SceInt32 photoExportIpcGetProgress(IPMI::Client *pClient, SceInt32 *pProgress);
static __attribute__((noinline)) SceInt32 photoExportIpcSetThumbnail(IPMI::Client *pClient, SceInt32 index,
					   const char *pPath);
static __attribute__((noinline)) SceInt32 photoExportIpcStartVideo(IPMI::Client *pClient, SceInt32 *pRequestId,
					 const IPMI::EventFlagInfo *pEventFlag, SceInt32 kind, SceInt32 count,
					 void *pData);
static __attribute__((noinline)) SceInt32 photoExportIpcGetVideoResult(IPMI::Client *pClient, SceInt32 requestId,
					     SceInt32 *pCount, void *pData);
#define PHOTO_EXPORT_SESSION_INIT(session, memory, sessionMode) \
	do { \
		SceInt32 i_; \
		(session).pClient = NULL; \
		(session).requestId = -1; \
		memset(&(session).mspace, 0, 12); \
		(session).pWorkMemory = (memory); \
		(session).progress = 0; \
		(session).pFilePath = ""; \
		(session).mode = (sessionMode); \
		for (i_ = 0; i_ < 10; i_++) \
			(session).weights[i_] = 0; \
	} while (0)

typedef struct PhotoExportNotification {
	SceUInt32 type;
	char text[60];
} PhotoExportNotification;
#define PHOTO_EXPORT_TRC_FROM_DATA	"Photo:\nRequire TRC R3146 test\nscePhotoExportFromData()"
#define PHOTO_EXPORT_TRC_FROM_FILE	"Photo:\nRequire TRC R3146 test\nscePhotoExportFromFile()"
typedef struct PhotoExportImageInfo {
	SceInt32 width;
	SceInt32 height;
	SceInt32 bitCount;		/* BMP */
	SceInt32 compression;		/* BMP */
	SceInt32 subsampling;		/* JPEG: 1 4:4:4, 2 4:2:2, 3 4:2:0, 4 4:1:1 */
	SceInt32 pngInterlace;
	SceInt32 pngColourType;
	SceInt32 tiffPhotometric;
	SceInt32 progressive;		/* JPEG */
} PhotoExportImageInfo;
typedef struct PhotoExportSendHeader {
	SceInt32 size;
	SceInt32 index;
	SceUInt8 last;
	SceInt64 total;
	SceInt32 reserved;
	SceInt32 reserved2;
} PhotoExportSendHeader;

/* what the image parsers read from a header */

static inline SceUInt32 photoExportSwap32(SceUInt32 value)
{
	return (((value >> 8) & 0xFF) << 16) | (value << 24) | (value >> 24) | (((value >> 16) & 0xFF) << 8);
}

static inline SceUInt32 photoExportSwap16(SceUInt32 value)
{
	return ((value >> 8) & 0xFF) | ((value & 0xFF) << 8);
}

/* the length of a string with its NUL terminator, as the requests send it */
static inline SceInt32 photoExportSize(const char *pString)
{
	SceInt32 length = 0;

	if (*pString != 0) {
		length = 0;
		do {
			pString++;
			length++;
		} while (*pString != 0);
	}
	return length + 1;
}

/* the path checks the file exports share: length, the prefixes, the file's size */
static inline SceInt32 photoExportCheckFile(const char *pPath)
{
	SceInt32 i;

	if (sceClibStrnlen(pPath, SCE_PHOTO_EXPORT_MAX_FS_PATH) >= SCE_PHOTO_EXPORT_MAX_FS_PATH)
		return PHOTO_ERROR_INVALID_PATH;
	for (i = 0; i < 3; i++) {
		const char *pPrefix = photoExportForbiddenPrefixes[i];

		if (sceClibStrncmp(pPath, pPrefix, sceClibStrnlen(pPrefix, 7)) == 0)
			return PHOTO_ERROR_INVALID_PATH;
	}
	return 0;
}

static __attribute__((noinline)) SceInt32 photoExportCheckExtension(const char *pPath, char *pExtension);
static __attribute__((noinline)) SceInt32 photoExportOpenSession(PhotoExportSession *pSession, SceBool (*cancelFunc)(void *), void *userdata);
static __attribute__((noinline)) int photoExportAppLaunchCallback(int notifyId, int notifyCount, int notifyArg, void *pCommon);
static __attribute__((noinline)) int photoExportGetParamCallback(int notifyId, int notifyCount, int notifyArg, void *pCommon);
static __attribute__((noinline)) SceInt32 photoExportSetParam(PhotoExportSession *pSession, const ScePhotoExportParam *param, const char *pFilePath);
static __attribute__((noinline)) SceInt32 photoExportSendFile(SceInt32 index, PhotoExportSession *pSession, const char *pFile, SceBool (*cancelFunc)(void *), void (*progressFunc)(void *, SceInt32), void *userdata, SceInt32 reserved, const char *pExtension, SceInt32 reserved2);
static __attribute__((noinline)) SceInt32 photoExportWait(PhotoExportSession *pSession, SceInt32 count, void *pData, SceBool (*cancelFunc)(void *), void (*progressFunc)(void *, SceInt32), void *userdata, void *pResult);
static __attribute__((noinline)) void *photoExportAlloc(PhotoExportSession *pSession, SceSize size);
static __attribute__((noinline)) void photoExportFree(PhotoExportSession *pSession, void *pMemory);
static __attribute__((noinline)) SceInt32 photoExportCloseSession(PhotoExportSession *pSession);

static inline SceInt32 photoExportCheckSize(const char *pPath)
{
	SceIoStat stat;
	SceInt32 result;

	stat.st_size = 0;
	result = PHOTO_ERROR_FILE_NOT_FOUND;
	if (sceIoGetstat(pPath, &stat) == 0) {
		result = 0;
		if (stat.st_size >= 0x80000000LL)
			result = PHOTO_ERROR_NOT_SUPPORTED;
	}
	return result;
}

static inline SceInt32 photoExportCheckReserved(const ScePhotoExportParam *param)
{
	SceInt32 i;

	for (i = 0; i < 32; i++) {
		if (param->reserved[i] != 0)
			return PHOTO_ERROR_PARAM;
	}
	return 0;
}

/* FromFile and its variants: send the file, wait and return where it went */
static inline SceInt32 photoExportFromFile(const char *pPath, const ScePhotoExportParam *param,
				    SceInt32 reserved, void *workMemory,
				    ScePhotoExportCancelFunc cancelFunc, void *userdata,
				    SceInt32 *pContentId, char *pExportedPath, SceInt32 exportedPathLength)
{
	PhotoExportSession session;
	char extension[6];
	SceInt32 result;
	PhotoExportResult *pResult;

	result = photoExportCheckSize(pPath);
	if (result != 0)
		return result;
	sceClibMemset(extension, 0, 6);
	if (photoExportCheckExtension(pPath, extension) == PHOTO_ERROR_NOT_SUPPORTED)
		return PHOTO_ERROR_NOT_SUPPORTED;
	do {
		PHOTO_EXPORT_SESSION_INIT(session, workMemory, 0);
		result = photoExportOpenSession(&session, cancelFunc, userdata);
		if (result != 0)
			break;
		if (photoExportSetParam(&session, param, pPath) != 0) {
			photoExportCloseSession(&session);
			result = PHOTO_ERROR_PARAM;
			break;
		}
		pResult = (PhotoExportResult *)photoExportAlloc(&session, sizeof(PhotoExportResult) * PHOTO_EXPORT_MAX_ITEMS);
		if (pResult == NULL) {
			photoExportCloseSession(&session);
			result = PHOTO_ERROR_NO_MEMORY;
			break;
		}
		result = photoExportSendFile(1, &session, pPath, cancelFunc, NULL, userdata, reserved,
					     extension, 0);
		if (result == 0) {
			if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
				result = PHOTO_ERROR_CANCELED;
				goto failed;
			}
			result = photoExportWait(&session, 0, NULL, cancelFunc, NULL, userdata, pResult);
			if (result == 0) {
				if (pContentId != NULL) {
					pContentId[0] = pResult->contentId[0];
					pContentId[1] = pResult->contentId[1];
				} else if (pExportedPath != NULL && exportedPathLength > 0) {
					sceClibStrncpy(pExportedPath, pResult->path, exportedPathLength);
				}
				goto release;
			}
		}
		if (result != IPMI_ERROR_TERMINATED) {
failed:
			if (pContentId != NULL) {
				pContentId[0] = -1;
				pContentId[1] = -1;
			} else if (pExportedPath != NULL && exportedPathLength > 0) {
				*pExportedPath = 0;
			}
		}
release:
		photoExportFree(&session, pResult);
		photoExportCloseSession(&session);
	} while (result == IPMI_ERROR_TERMINATED);
	return result;
}

/* releases the heap */
static inline void photoExportDestroyHeap(PhotoExportSession *pSession)
{
	if (pSession->mspace != NULL) {
		sceClibMspaceDestroy(pSession->mspace);
		pSession->mspace = NULL;
	}
	if (pSession->memblockId != 0) {
		sceKernelFreeMemBlock(pSession->memblockId);
		pSession->memblockId = 0;
	}
}

static __attribute__((noinline)) void *photoExportAlloc(PhotoExportSession *pSession, SceSize size)
{
	return sceClibMspaceMalloc(pSession->mspace, size);
}

static __attribute__((noinline)) void photoExportFree(PhotoExportSession *pSession, void *pMemory)
{
	if (pMemory != NULL)
		sceClibMspaceFree(pSession->mspace, pMemory);
}

static __attribute__((noinline)) int photoExportLaunchMediaService(SceUID appLaunchCallback, SceUID getParamCallback)
{
	PhotoExportLaunchParam param;

	sceClibMemset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.reserved04 = 0;
	param.reserved08 = -1;
	param.appLaunchCallback = appLaunchCallback;
	param.getParamCallback = getParamCallback;
	param.reserved14 = -1;
	return sceAppMgrLaunchAppByUri2("avmedia_service:", &param);
}



static __attribute__((noinline)) SceInt32 photoExportOpenSession(PhotoExportSession *pSession,
				       SceBool (*cancelFunc)(void *), void *userdata)
{
	void *pBase;
	SceUID memblockId;
	void *mspace;
	void *pBuffer;
	SceInt32 launchState;
	SceInt32 connectResult;
	SceUID appLaunchCallback;
	SceUID getParamCallback;
	PhotoExportLaunchParam launchParam;
	int retry;
	int ret;
	int error;

	if (pSession->pClient != NULL)
		return 0;
	memblockId = 0;
	if (pSession->pWorkMemory != NULL) {
		pBase = pSession->pWorkMemory;
	} else {
		pBase = NULL;
		memblockId = sceKernelAllocMemBlock("ScePhotoExportHeap", PHOTO_EXPORT_HEAP_MEMBLOCK_TYPE,
						    PHOTO_EXPORT_HEAP_SIZE, NULL);
		if (memblockId < 0)
			return SCE_PHOTO_EXPORT_ERROR_NO_MEMORY;
		sceKernelGetMemBlockBase(memblockId, &pBase);
	}
	mspace = sceClibMspaceCreate(pBase, PHOTO_EXPORT_HEAP_SIZE);
	if (mspace == NULL) {
		if (memblockId != 0)
			sceKernelFreeMemBlock(memblockId);
		return SCE_PHOTO_EXPORT_ERROR_NO_MEMORY;
	}
	pSession->mspace = mspace;
	pSession->memblockId = memblockId;
	IPMI::Client::Config config;

	sceClibStrncpy(config.serviceName, "SceAvMediaSrv", sizeof(config.serviceName));
	config.budget = IPMI::USER_BUDGET;
	if (pSession->mode == 2) {
		config.maxSyncCallInSize = 4096;
		config.maxAsyncCallInSize = 24576;
		config.maxAsyncCallOutSize = 12288;
	} else {
		config.maxSyncCallInSize = 18432;
		config.maxAsyncCallInSize = 4096;
		config.maxAsyncCallOutSize = 4096;
	}
	config.maxSyncCallOutSize = 4096;
	pBuffer = sceClibMspaceMalloc(pSession->mspace, IPMI::Client::getBufferSize(&config));
	if (pBuffer == NULL) {
		photoExportDestroyHeap(pSession);
		return SCE_PHOTO_EXPORT_ERROR_NO_MEMORY;
	}
	pSession->pClientBuffer = sceClibMspaceMalloc(pSession->mspace, 0x4800);
	if (pSession->pClientBuffer == NULL) {
		sceClibMspaceFree(pSession->mspace, pBuffer);
		photoExportDestroyHeap(pSession);
		return SCE_PHOTO_EXPORT_ERROR_NO_MEMORY;
	}
	ret = IPMI::Client::create(&pSession->pClient, &config, pSession, pBuffer);
	if (ret != 0) {
		sceClibMspaceFree(pSession->mspace, pBuffer);
		if (pSession->pClientBuffer != NULL)
			sceClibMspaceFree(pSession->mspace, pSession->pClientBuffer);
		pSession->pClientBuffer = NULL;
		pSession->pClient = NULL;
		photoExportDestroyHeap(pSession);
		return ret;
	}
	launchState = PHOTO_EXPORT_LAUNCH_STATE_NONE;
	appLaunchCallback = sceKernelCreateCallback("ScePhotoExportCallbackAppLaunch", 0,
						    photoExportAppLaunchCallback, &launchState);
	getParamCallback = sceKernelCreateCallback("ScePhotoExportCallbackGetParam", 0,
						   photoExportGetParamCallback, &launchState);
	error = SCE_PHOTO_EXPORT_ERROR_INTERNAL;
	connectResult = SCE_PHOTO_EXPORT_ERROR_INTERNAL;
	if (appLaunchCallback > 0) {
		if (getParamCallback > 0) {
			retry = 0;
			for (;;) {
				if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
					error = SCE_PHOTO_EXPORT_ERROR_CANCELED;
					break;
				}
				error = pSession->pClient->connect(NULL, 0, &connectResult);
				if (error != IPMI_ERROR_SERVER_NOT_FOUND ||
				    retry >= PHOTO_EXPORT_CONNECT_RETRY_COUNT)
					break;
				retry++;
				if (launchState == PHOTO_EXPORT_LAUNCH_STATE_NONE) {
					sceClibMemset(&launchParam, 0, sizeof(launchParam));
					launchParam.reserved08 = -1;
					launchParam.appLaunchCallback = appLaunchCallback;
					launchParam.size = sizeof(launchParam);
					launchParam.reserved04 = 0;
					launchParam.getParamCallback = getParamCallback;
					launchParam.reserved14 = -1;
					sceAppMgrLaunchAppByUri2("avmedia_service:", &launchParam);
				}
				sceKernelDelayThread(PHOTO_EXPORT_CONNECT_RETRY_USEC);
				if (launchState == PHOTO_EXPORT_LAUNCH_STATE_READY)
					continue;
				sceKernelCheckCallback();
			}
		}
		sceKernelDeleteCallback(appLaunchCallback);
	}
	if (getParamCallback > 0)
		sceKernelDeleteCallback(getParamCallback);
	if (error != 0) {
		ret = SCE_PHOTO_EXPORT_ERROR_INTERNAL;
		switch (error) {
		case SCE_PHOTO_EXPORT_ERROR_CANCELED:
			ret = SCE_PHOTO_EXPORT_ERROR_CANCELED;
			break;
		case IPMI_ERROR_SERVER_NOT_FOUND:
			ret = SCE_PHOTO_EXPORT_ERROR_SERVER_DOWN;
			break;
		case IPMI_ERROR_CONNECTION_REFUSED:
			ret = connectResult;
			break;
		default:
			break;
		}
		pSession->pClient->destroy();
		sceClibMspaceFree(pSession->mspace, pBuffer);
		if (pSession->pClientBuffer != NULL)
			sceClibMspaceFree(pSession->mspace, pSession->pClientBuffer);
		pSession->pClientBuffer = NULL;
		pSession->pClient = NULL;
		photoExportDestroyHeap(pSession);
		return ret;
	}
	pSession->requestId = -1;
	pSession->pPhotoTitle = (char *)sceClibMspaceMalloc(pSession->mspace, 0x100);
	if (pSession->pPhotoTitle != NULL)
		sceClibMemset(pSession->pPhotoTitle, 0, 0x100);
	pSession->pGameTitle = (char *)sceClibMspaceMalloc(pSession->mspace, 0x100);
	if (pSession->pGameTitle != NULL)
		sceClibMemset(pSession->pGameTitle, 0, 0x100);
	pSession->pGameComment = (char *)sceClibMspaceMalloc(pSession->mspace, 0x200);
	if (pSession->pGameComment != NULL)
		sceClibMemset(pSession->pGameComment, 0, 0x200);
	return 0;
}

static __attribute__((noinline)) SceInt32 photoExportCloseSession(PhotoExportSession *pSession)
{
	if (pSession->pGameComment != NULL) {
		sceClibMspaceFree(pSession->mspace, pSession->pGameComment);
		pSession->pGameComment = NULL;
	}
	if (pSession->pGameTitle != NULL) {
		sceClibMspaceFree(pSession->mspace, pSession->pGameTitle);
		pSession->pGameTitle = NULL;
	}
	if (pSession->pPhotoTitle != NULL) {
		sceClibMspaceFree(pSession->mspace, pSession->pPhotoTitle);
		pSession->pPhotoTitle = NULL;
	}
	if (pSession->pClient != NULL) {
		pSession->pClient->disconnect();
		pSession->pClient->destroy();
		if (pSession->pClient != NULL)
			sceClibMspaceFree(pSession->mspace, pSession->pClient);
		pSession->pClient = NULL;
	}
	if (pSession->pClientBuffer != NULL) {
		sceClibMspaceFree(pSession->mspace, pSession->pClientBuffer);
		pSession->pClientBuffer = NULL;
	}
	if (pSession->mspace != NULL) {
		sceClibMspaceDestroy(pSession->mspace);
		pSession->mspace = NULL;
	}
	if (pSession->memblockId != 0) {
		sceKernelFreeMemBlock(pSession->memblockId);
		pSession->memblockId = 0;
	}
	return 0;
}

/* the requests to the media service (photoExportIpc*, below) */

static __attribute__((noinline)) SceInt32 photoExportSendData(PhotoExportSession *pSession, void *pData, SceInt32 size,
				    SceInt32 index, SceUInt8 last, SceInt64 total, SceInt32 reserved,
				    const char *pPath, SceInt32 reserved2)
{
	return photoExportIpcSendData(pSession->pClient, pData, size, index, last, total, reserved,
				      pPath, reserved2, pSession->pPhotoTitle, pSession->pGameTitle,
				      pSession->pGameComment, pSession->pFilePath);
}

static __attribute__((noinline)) SceInt32 photoExportStart(PhotoExportSession *pSession, SceInt32 count, void *pData)
{
	return photoExportIpcStart(pSession->pClient, &pSession->requestId, NULL, count, pData);
}

static __attribute__((noinline)) SceInt32 photoExportGetResult(PhotoExportSession *pSession, SceInt32 *pCount, void *pData)
{
	if (pSession->requestId == -1)
		return IPMI_ERROR_NOT_INITIALIZED;
	return photoExportIpcGetResult(pSession->pClient, pSession->requestId, pCount, pData);
}

static __attribute__((noinline)) SceInt32 photoExportCancel(PhotoExportSession *pSession)
{
	return photoExportIpcCancel(pSession->pClient);
}

static __attribute__((noinline)) SceInt32 photoExportAbort(PhotoExportSession *pSession)
{
	return photoExportIpcAbort(pSession->pClient);
}

static __attribute__((noinline)) SceInt32 photoExportStartVideo(PhotoExportSession *pSession, SceInt32 kind,
				      SceInt32 count, void *pData)
{
	return photoExportIpcStartVideo(pSession->pClient, &pSession->requestId, NULL, kind, count,
					pData);
}

static __attribute__((noinline)) SceInt32 photoExportGetVideoResult(PhotoExportSession *pSession, SceInt32 *pCount,
					  void *pData)
{
	if (pSession->requestId == -1)
		return IPMI_ERROR_NOT_INITIALIZED;
	return photoExportIpcGetVideoResult(pSession->pClient, pSession->requestId, pCount, pData);
}

static __attribute__((noinline)) SceInt32 photoExportGetProgress(PhotoExportSession *pSession, SceInt32 *pProgress)
{
	return photoExportIpcGetProgress(pSession->pClient, pProgress);
}

static __attribute__((noinline)) SceInt32 photoExportSetParam(PhotoExportSession *pSession, const ScePhotoExportParam *param,
				    const char *pFilePath)
{
	if (param != NULL) {
		if (param->photoTitle != NULL) {
			if (sceClibStrnlen((const char *)param->photoTitle, 0x100) >= 0x100)
				return PHOTO_ERROR_PARAM;
			sceClibStrncpy(pSession->pPhotoTitle, (const char *)param->photoTitle, 0xFF);
		} else {
			sceClibMemset(pSession->pPhotoTitle, 0, 0x100);
		}
		if (param->gameTitle != NULL) {
			if (sceClibStrnlen((const char *)param->gameTitle, 0x100) >= 0x100)
				return PHOTO_ERROR_PARAM;
			sceClibStrncpy(pSession->pGameTitle, (const char *)param->gameTitle, 0xFF);
		} else {
			sceClibMemset(pSession->pGameTitle, 0, 0x100);
		}
		if (param->gameComment != NULL) {
			if (sceClibStrnlen((const char *)param->gameComment, 0x200) >= 0x200)
				return PHOTO_ERROR_PARAM;
			sceClibStrncpy(pSession->pGameComment, (const char *)param->gameComment, 0x1FF);
		} else {
			sceClibMemset(pSession->pGameComment, 0, 0x200);
		}
		pSession->pFilePath = pFilePath != NULL ? pFilePath : "";
		return 0;
	}
	sceClibMemset(pSession->pPhotoTitle, 0, 0x100);
	sceClibMemset(pSession->pGameTitle, 0, 0x100);
	sceClibMemset(pSession->pGameComment, 0, 0x200);
	pSession->pFilePath = pFilePath != NULL ? pFilePath : "";
	return 0;
}

static __attribute__((noinline)) SceInt32 photoExportGetWeight(PhotoExportSession *pSession, SceInt32 index)
{
	if (index > 0 && index <= PHOTO_EXPORT_MAX_ITEMS)
		return pSession->weights[index - 1];
	return 0;
}

static __attribute__((noinline)) SceInt32 photoExportSetThumbnail(PhotoExportSession *pSession, SceInt32 index,
					const char *pPath)
{
	return photoExportIpcSetThumbnail(pSession->pClient, index, pPath != NULL ? pPath : "");
}

static __attribute__((noinline)) int photoExportAppLaunchCallback(int notifyId, int notifyCount, int notifyArg, void *pCommon)
{
	if (notifyArg > 0)
		*(SceInt32 *)pCommon = PHOTO_EXPORT_LAUNCH_STATE_LAUNCHED;
	return 0;
}

static __attribute__((noinline)) int photoExportGetParamCallback(int notifyId, int notifyCount, int notifyArg, void *pCommon)
{
	int result = 0;

	if (notifyArg != 0) {
		if (notifyArg == (SceInt32)0x8080201B)
			*(SceInt32 *)pCommon = PHOTO_EXPORT_LAUNCH_STATE_NONE;
	} else {
		*(SceInt32 *)pCommon = PHOTO_EXPORT_LAUNCH_STATE_READY;
	}
	return result;
}

/* start the export of count items and wait for the service, reporting the progress */
static __attribute__((noinline)) SceInt32 photoExportWait(PhotoExportSession *pSession, SceInt32 count, void *pData,
				SceBool (*cancelFunc)(void *),
				void (*progressFunc)(void *, SceInt32), void *userdata,
				void *pResult)
{
	SceInt32 result, cancelled, shown, percent;
	SceInt32 progress, total;
	float scale;

	total = count;
	result = photoExportStart(pSession, count, pData);
	if (result == 0) {
		cancelled = 0;
		shown = 0;
		scale = (float)total;
		do {
			if (cancelFunc != NULL && cancelled == 0 && cancelFunc(userdata) != 0) {
				photoExportCancel(pSession);
				cancelled = 1;
			}
			if (progressFunc != NULL) {
				progress = 0;
				if (photoExportGetProgress(pSession, &progress) == 0) {
					percent = (int)((float)((float)progress / scale) * 100.0f);
					if (shown != percent) {
						progressFunc(userdata, (int)((float)((float)progress / scale) * 100.0f));
						shown = percent;
					}
				}
			}
			sceKernelDelayThread(PHOTO_EXPORT_POLL_USEC);
			total = count;
			if (count == 0)
				total = 1;
			result = photoExportGetResult(pSession, &total, pResult);
		} while (result == IPMI_ERROR_WOULD_BLOCK);
		if (progressFunc != NULL && shown != 100)
			progressFunc(userdata, 100);
	}
	return result;
}

/* the notification the library sends (SceAppMgr): the text the system shows */


extern "C" SceInt32 scePhotoExportFromData(const void *photodata, SceInt32 photodataSize,
				const ScePhotoExportParam *param, void *workMemory,
				ScePhotoExportCancelFunc cancelFunc, void *userdata,
				SceChar8 *exportedPath, SceInt32 exportedPathLength)
{
	PhotoExportSession session;
	PhotoExportNotification notification;
	SceUInt8 *pClear;
	SceInt32 result, i, pieces, tail;
	const char *pName;
	const char *pData;

	for (pClear = (SceUInt8 *)&notification; pClear < (SceUInt8 *)(&notification + 1); pClear++)
		*pClear = 0;
	sceClibStrncpy((char *)notification.text, PHOTO_EXPORT_TRC_FROM_DATA, 0x3C);
	notification.text[59] = 0;
	sceAppMgrSendNotificationRequest(&notification, 1);

	if (photodata == NULL || photodataSize <= 0)
		return PHOTO_ERROR_PARAM;
	if (exportedPathLength < 0)
		return PHOTO_ERROR_PARAM;
	if (exportedPath != NULL && exportedPathLength <= 0)
		return PHOTO_ERROR_PARAM;
	if ((SceUInt32)SceLibKernel_9F793F84() >= PHOTO_EXPORT_CHECK_RESERVED_VERSION && param != NULL) {
		for (i = 0; i < 32; i++) {
			if (param->reserved[i] != 0)
				return PHOTO_ERROR_PARAM;
		}
	}
	tail = photodataSize % PHOTO_EXPORT_PIECE_SIZE;
	pieces = (tail != 0) + ((photodataSize + ((SceUInt32)(photodataSize >> 14) >> 18)) >> 14) - 1;
	do {
		SceInt32 piece;

		PHOTO_EXPORT_SESSION_INIT(session, workMemory, 0);
		result = photoExportOpenSession(&session, cancelFunc, userdata);
		if (result != 0)
			break;
		if (photoExportSetParam(&session, param, NULL) != 0) {
			photoExportCloseSession(&session);
			result = PHOTO_ERROR_PARAM;
			break;
		}
		pName = (const char *)photoExportAlloc(&session, sizeof(PhotoExportResult) * PHOTO_EXPORT_MAX_ITEMS);
		if (pName == NULL) {
			photoExportCloseSession(&session);
			result = PHOTO_ERROR_NO_MEMORY;
			break;
		}
		pData = (const char *)photodata;
		for (piece = 0; piece < pieces; piece++) {
			result = photoExportSendData(&session, (void *)pData, PHOTO_EXPORT_PIECE_SIZE, piece,
						     0, photodataSize, 0, "", 0);
			if (result != 0)
				break;
			pData += PHOTO_EXPORT_PIECE_SIZE;
			if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
				result = PHOTO_ERROR_CANCELED;
				goto cancelled;
			}
		}
		if (result == 0) {
			result = photoExportSendData(&session, (void *)pData,
						     photodataSize - (piece << 14), piece, 1,
						     photodataSize, 0, "", 0);
			if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
				result = PHOTO_ERROR_CANCELED;
				goto cancelled;
			}
			if (result == 0) {
				result = photoExportWait(&session, 0, NULL, cancelFunc, NULL, userdata,
							 (void *)pName);
				if (result == 0) {
					if (exportedPath != NULL && exportedPathLength > 0)
						sceClibStrncpy((char *)exportedPath, pName, exportedPathLength);
					goto release;
				}
			}
		}
		if (result != IPMI_ERROR_TERMINATED) {
cancelled:
			if (exportedPath != NULL && exportedPathLength > 0)
				*exportedPath = 0;
		}
release:
		photoExportFree(&session, (void *)pName);
		photoExportCloseSession(&session);
	} while (result == IPMI_ERROR_TERMINATED);
	return result;
}


/* the file's extension, if the library takes it */
static __attribute__((noinline)) SceInt32 photoExportCheckExtension(const char *pPath, char *pExtension)
{
	SceInt32 length, pos, count;

	length = sceClibStrnlen(pPath, SCE_PHOTO_EXPORT_MAX_FS_PATH);
	count = 0;
	for (pos = length - 1; pos > length - 6; pos--) {
		if (pPath[pos] == '.') {
			sceClibStrncpy(pExtension, &pPath[pos], count + 1);
			break;
		}
		count++;
	}
	if (*pExtension == 0)
		return PHOTO_ERROR_NOT_SUPPORTED;
	if (sceClibStrncasecmp(pExtension, ".bmp", 4) != 0
	    && sceClibStrncasecmp(pExtension, ".gif", 4) != 0
	    && sceClibStrncasecmp(pExtension, ".jpg", 4) != 0
	    && sceClibStrncasecmp(pExtension, ".jpeg", 5) != 0
	    && sceClibStrncasecmp(pExtension, ".png", 4) != 0
	    && sceClibStrncasecmp(pExtension, ".tif", 4) != 0
	    && sceClibStrncasecmp(pExtension, ".tiff", 5) != 0
	    && sceClibStrncasecmp(pExtension, ".mpo", 4) != 0)
		return PHOTO_ERROR_NOT_SUPPORTED;
	return 0;
}

/* send the file in pieces; progress as the share of the item's weight */
static __attribute__((noinline)) SceInt32 photoExportSendFile(SceInt32 index, PhotoExportSession *pSession, const char *pFile,
				    SceBool (*cancelFunc)(void *),
				    void (*progressFunc)(void *, SceInt32), void *userdata,
				    SceInt32 reserved, const char *pExtension, SceInt32 reserved2)
{
	SceUID fd;
	SceInt64 size;
	SceOff offset;
	void *pBuffer;
	SceInt32 result, total, pieces, sent, shown, percent;
	float scale;

	fd = sceIoOpen(pFile, SCE_O_RDONLY, 0);
	if (fd < 0)
		return PHOTO_ERROR_FILE_NOT_FOUND;
	offset = sceIoLseek(fd, 0, SCE_SEEK_END);
	size = offset;
	if (offset < 0) {
		sceIoClose(fd);
		return (SceInt32)offset;
	}
	sceIoLseek(fd, 0, SCE_SEEK_SET);
	pBuffer = photoExportAlloc(pSession, PHOTO_EXPORT_PIECE_SIZE);
	if (pBuffer == NULL) {
		sceIoClose(fd);
		return PHOTO_ERROR_NO_MEMORY;
	}
	total = size / PHOTO_EXPORT_PIECE_SIZE + (size % PHOTO_EXPORT_PIECE_SIZE != 0);
	pieces = total - 1;
	sent = 0;
	shown = 0;
	result = 0;
	if (pieces > 0) {
		scale = (float)total;
		do {
			result = sceIoRead(fd, pBuffer, PHOTO_EXPORT_PIECE_SIZE);
			if (result < 0)
				break;
			result = photoExportSendData(pSession, pBuffer, PHOTO_EXPORT_PIECE_SIZE, sent, 0,
						     size, reserved, pExtension, 0);
			if (result != 0)
				break;
			if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
				result = PHOTO_ERROR_CANCELED;
				break;
			}
			sent++;
			if (progressFunc != NULL) {
				percent = (int)((float)photoExportGetWeight(pSession, index)
						* (float)((float)sent / scale));
				if (shown != percent) {
					progressFunc(userdata, percent + pSession->progress);
					shown = percent;
				}
			}
		} while (sent < pieces);
	}
	if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
		result = PHOTO_ERROR_CANCELED;
	} else {
		if (result == 0) {
			result = sceIoRead(fd, pBuffer, size - (sent << 14));
			if (result >= 0)
				result = photoExportSendData(pSession, pBuffer, size - (sent << 14), sent, 1,
							     size, reserved, pExtension, 0);
		}
		if (progressFunc != NULL) {
			percent = photoExportGetWeight(pSession, index);
			progressFunc(userdata, percent + pSession->progress);
			pSession->progress += percent;
		}
	}
	sceIoClose(fd);
	photoExportFree(pSession, pBuffer);
	return result;
}

extern "C" SceInt32 scePhotoExportFromFile(const SceChar8 *photodataPath, const ScePhotoExportParam *param,
				void *workMemory, ScePhotoExportCancelFunc cancelFunc,
				void *userdata, SceChar8 *exportedPath, SceInt32 exportedPathLength)
{
	PhotoExportNotification notification;
	SceUInt8 *pClear;
	SceInt32 result;

	for (pClear = (SceUInt8 *)&notification; pClear < (SceUInt8 *)(&notification + 1); pClear++)
		*pClear = 0;
	sceClibStrncpy(notification.text, PHOTO_EXPORT_TRC_FROM_FILE, 0x3C);
	notification.text[59] = 0;
	sceAppMgrSendNotificationRequest(&notification, 1);

	if (photodataPath == NULL)
		return PHOTO_ERROR_PARAM;
	if (exportedPathLength < 0)
		return PHOTO_ERROR_PARAM;
	if (exportedPath != NULL && exportedPathLength <= 0)
		return PHOTO_ERROR_PARAM;
	result = photoExportCheckFile((const char *)photodataPath);
	if (result != 0)
		return result;
	if (SceLibKernel_9F793F84() >= PHOTO_EXPORT_CHECK_RESERVED_VERSION && param != NULL
	    && photoExportCheckReserved(param) != 0)
		return PHOTO_ERROR_PARAM;
	return photoExportFromFile((const char *)photodataPath, param, 0, workMemory, cancelFunc,
				   userdata, NULL, (char *)exportedPath, exportedPathLength);
}

/* the file export of the two variants: the checks, then the export */
static __attribute__((noinline)) SceInt32 photoExportFromFileEx(const char *pPath, const ScePhotoExportParam *param,
				      SceInt32 reserved, void *workMemory,
				      ScePhotoExportCancelFunc cancelFunc, void *userdata,
				      SceInt32 *pContentId, char *pExportedPath,
				      SceInt32 exportedPathLength)
{
	SceInt32 result;

	if (pPath == NULL)
		return PHOTO_ERROR_PARAM;
	result = photoExportCheckFile(pPath);
	if (result != 0)
		return result;
	if (param != NULL && photoExportCheckReserved(param) != 0)
		return PHOTO_ERROR_PARAM;
	return photoExportFromFile(pPath, param, reserved, workMemory, cancelFunc, userdata,
				   pContentId, pExportedPath, exportedPathLength);
}

extern "C" SceInt32 ScePhotoExport_76640642(const SceChar8 *photodataPath, const ScePhotoExportParam *param,
				 SceInt32 reserved, void *workMemory,
				 ScePhotoExportCancelFunc cancelFunc, void *userdata,
				 SceInt32 *pContentId)
{
	return photoExportFromFileEx((const char *)photodataPath, param, reserved, workMemory,
				     cancelFunc, userdata, pContentId, NULL, 0);
}

extern "C" SceInt32 ScePhotoExport_719ACE85(const SceChar8 *photodataPath, const ScePhotoExportParam *param,
				 SceInt32 reserved, void *workMemory,
				 ScePhotoExportCancelFunc cancelFunc, void *userdata,
				 SceChar8 *exportedPath, SceInt32 exportedPathLength)
{
	return photoExportFromFileEx((const char *)photodataPath, param, reserved, workMemory,
				     cancelFunc, userdata, NULL, (char *)exportedPath,
				     exportedPathLength);
}

/* the export's abort: open a session, tell the service and close it */
extern "C" SceInt32 ScePhotoExport_16B1028E(void *workMemory)
{
	PhotoExportSession session;
	SceInt32 result;

	PHOTO_EXPORT_SESSION_INIT(session, workMemory, 3);
	result = photoExportOpenSession(&session, NULL, NULL);
	if (result == 0) {
		photoExportAbort(&session);
		photoExportCloseSession(&session);
		result = 0;
	}
	return result;
}

/* the batch export: up to ten items, their thumbnails and the share of the work each takes */
extern "C" SceInt32 ScePhotoExport_2EFB7250(PhotoExportBatchItem *pItems, SceUInt32 count, void *workMemory,
				 ScePhotoExportCancelFunc cancelFunc,
				 void (*progressFunc)(void *, SceInt32), void *userdata,
				 SceInt32 reserved, PhotoExportResult *pResults)
{
	SceIoStat stat;
	PhotoExportSession session;
	PhotoExportResult *pResult, *pOut;
	PhotoExportStartItem *pStart;
	SceInt32 result;
	SceUInt32 i;
	SceInt32 j;
	char extension[6];

	if (pItems == NULL)
		return PHOTO_ERROR_PARAM;
	if (count == 0 || count > PHOTO_EXPORT_MAX_ITEMS)
		return PHOTO_ERROR_PARAM;
	for (i = 0; i < count; i++) {
		SceInt32 length;

		if (pItems[i].path[0] == 0)
			return PHOTO_ERROR_PARAM;
		result = photoExportCheckSize(pItems[i].path);
		if (result != 0)
			return result;
		length = sceClibStrnlen(pItems[i].path, SCE_PHOTO_EXPORT_MAX_FS_PATH);
		if (length >= SCE_PHOTO_EXPORT_MAX_FS_PATH)
			return PHOTO_ERROR_INVALID_PATH;
		for (j = 3; j < 4; j++) {
			const char *pPrefix = photoExportForbiddenPrefixes[j];

			if (sceClibStrncmp(pItems[i].path, pPrefix, sceClibStrnlen(pPrefix, 7)) != 0)
				return PHOTO_ERROR_INVALID_PATH;
		}
	}
	if (pResults == NULL)
		return PHOTO_ERROR_PARAM;
	pOut = pResults;
	for (i = 0; i < count; i++) {
		sceClibMemset(extension, 0, 6);
		if (photoExportCheckExtension(pItems[i].path, extension) == PHOTO_ERROR_NOT_SUPPORTED
		    && sceClibStrncasecmp(extension, ".mp4", 4) != 0)
			return PHOTO_ERROR_NOT_SUPPORTED;
		pOut->path[0] = 0;
		pOut->contentId[0] = -1;
		pOut->contentId[1] = -1;
		pOut++;
	}
	do {
		PHOTO_EXPORT_SESSION_INIT(session, workMemory, 2);
		result = photoExportOpenSession(&session, cancelFunc, userdata);
		if (result != 0)
			break;
		pResult = (PhotoExportResult *)photoExportAlloc(&session, sizeof(PhotoExportResult) * count);
		if (pResult == NULL) {
			photoExportCloseSession(&session);
			result = PHOTO_ERROR_NO_MEMORY;
			break;
		}
		pStart = (PhotoExportStartItem *)photoExportAlloc(&session, sizeof(PhotoExportStartItem) * count);
		if (pStart == NULL) {
			photoExportFree(&session, pResult);
			photoExportCloseSession(&session);
			result = PHOTO_ERROR_NO_MEMORY;
			break;
		}
		sceClibMemset(pStart, 0, sizeof(PhotoExportStartItem) * count);
		for (i = 0; i < count; i++) {
			PhotoExportBatchItem *pItem = &pItems[i];
			PhotoExportStartItem *pDest = &pStart[i];

			sceClibMemset(extension, 0, 6);
			photoExportCheckExtension(pItem->path, extension);
			result = photoExportSetParam(&session, (const ScePhotoExportParam *)&pItem->version,
						     pItem->path);
			if (result != 0)
				break;
			sceClibStrncpy(pDest->path, pItem->path, 0x400);
			if (pItem->photoTitle != NULL)
				sceClibStrncpy(pDest->photoTitle, pItem->photoTitle, 0x100);
			else
				sceClibMemset(pDest->photoTitle, 0, 0x100);
			if (pItem->gameTitle != NULL)
				sceClibStrncpy(pDest->gameTitle, pItem->gameTitle, 0x100);
			else
				sceClibMemset(pDest->gameTitle, 0, 0x100);
			if (pItem->gameComment != NULL)
				sceClibStrncpy(pDest->gameComment, pItem->gameComment, 0x200);
			else
				sceClibMemset(pDest->gameComment, 0, 0x200);
			pDest->attribute = pItem->attribute;
			pDest->duration[0] = pItem->duration[0];
			pDest->duration[1] = pItem->duration[1];
			if (pItem->pThumbnailPath == NULL
			    || (stat.st_size = 0, sceIoGetstat(pItem->pThumbnailPath, &stat) != 0)
			    || stat.st_size >= 0x80000000LL) {
				if (pItem->pThumbnail != NULL && pItem->thumbnailSize != 0) {
					char *pPath = (char *)photoExportAlloc(&session, SCE_PHOTO_EXPORT_MAX_FS_PATH);

					if (pPath != NULL) {
						SceUInt64 time = sceKernelGetProcessTimeWide();
						SceUID fd;

						sceClibSnprintf(pPath, SCE_PHOTO_EXPORT_MAX_FS_PATH,
								"photo0:/temp/thumb/%016llx_%02d.bin", time, i);
						fd = sceIoOpen(pPath, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6);
						if (fd >= 0) {
							SceInt32 written = sceIoWrite(fd, pItem->pThumbnail,
										      pItem->thumbnailSize);

							sceIoClose(fd);
							if (written == pItem->thumbnailSize)
								photoExportSetThumbnail(&session, i, pPath);
						}
						photoExportFree(&session, pPath);
					}
				}
			} else {
				photoExportSetThumbnail(&session, i, pItem->pThumbnailPath);
			}
			if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
				result = PHOTO_ERROR_CANCELED;
				break;
			}
		}
		if (result == 0) {
			result = photoExportWait(&session, count, pStart, cancelFunc, progressFunc, userdata,
						 pResult);
			if (result == 0)
				sceClibMemcpy_safe(pResults, pResult, sizeof(PhotoExportResult) * count);
		}
		if (pResult != NULL)
			photoExportFree(&session, pResult);
		if (pStart != NULL)
			photoExportFree(&session, pStart);
		photoExportCloseSession(&session);
	} while (result == IPMI_ERROR_TERMINATED);
	return result;
}

/* start the media service without a request */
extern "C" SceInt32 ScePhotoExport_F4E0F7FF(void)
{
	return photoExportLaunchMediaService(-1, -1);
}

/* the export of a video: the file's path and where the content goes */
extern "C" SceInt32 ScePhotoExport_05D5A482(const char *pPath, void *workMemory, SceUInt32 size,
				 ScePhotoExportCancelFunc cancelFunc, void *userdata,
				 SceInt32 *pContentId)
{
	SceIoStat stat;
	PhotoExportSession session;
	char extension[6];
	SceInt32 result, i;
	SceInt32 request[2], contentId[2];
	char *pItem;

	if (pPath == NULL)
		return PHOTO_ERROR_PARAM;
	if (workMemory == NULL)
		return PHOTO_ERROR_PARAM;
	if (size < 0x10000)
		return PHOTO_ERROR_PARAM;
	if (sceClibStrnlen(pPath, SCE_PHOTO_EXPORT_MAX_FS_PATH) >= SCE_PHOTO_EXPORT_MAX_FS_PATH)
		return PHOTO_ERROR_INVALID_PATH;
	for (i = 3; i < 4; i++) {
		const char *pPrefix = photoExportForbiddenPrefixes[i];

		if (sceClibStrncmp(pPath, pPrefix, sceClibStrnlen(pPrefix, 7)) != 0)
			return PHOTO_ERROR_INVALID_PATH;
	}
	sceClibMemset(extension, 0, 6);
	photoExportCheckExtension(pPath, extension);
	if (sceClibStrncasecmp(extension, ".mp4", 4) != 0)
		return PHOTO_ERROR_NOT_SUPPORTED;
	stat.st_size = 0;
	if (sceIoGetstat(pPath, &stat) != 0)
		return PHOTO_ERROR_FILE_NOT_FOUND;
	do {
		PHOTO_EXPORT_SESSION_INIT(session, workMemory, 2);
		result = photoExportOpenSession(&session, cancelFunc, userdata);
		if (result != 0)
			break;
		request[0] = 1;
		pItem = (char *)photoExportAlloc(&session, 0x804);
		if (pItem == NULL) {
			result = PHOTO_ERROR_NO_MEMORY;
			break;
		}
		sceClibMemset(pItem, 0, 0x804);
		sceClibStrncpy(pItem, pPath, 0x3FF);
		*(SceInt32 *)(pItem + 0x800) = 1;
		result = photoExportStartVideo(&session, 4, request[0], pItem);
		if (result == 0) {
			i = 0;
			do {
				if (cancelFunc != NULL && i == 0 && cancelFunc(userdata) != 0) {
					i = 1;
					photoExportCancel(&session);
				}
				sceKernelDelayThread(PHOTO_EXPORT_POLL_USEC);
				result = photoExportGetVideoResult(&session, request, contentId);
			} while (result == IPMI_ERROR_WOULD_BLOCK);
		}
		if (result != 0) {
			if (result != IPMI_ERROR_TERMINATED && pContentId != NULL) {
				pContentId[0] = -1;
				pContentId[1] = -1;
			}
		} else if (pContentId != NULL) {
			pContentId[0] = contentId[0];
			pContentId[1] = contentId[1];
		}
		photoExportFree(&session, pItem);
		photoExportCloseSession(&session);
	} while (result == IPMI_ERROR_TERMINATED);
	return result;
}

/* the TIFF header: the byte order and the offset of the first directory */
static __attribute__((noinline)) SceInt32 photoExportTiffHeader(const SceUInt8 *pData, SceInt32 size, SceInt32 *pLittleEndian,
				      SceInt64 *pOffset)
{
	SceUInt32 offset;

	if (size < 8)
		return PHOTO_ERROR_INVALID_DATA;
	*pLittleEndian = 0;
	offset = *(const SceUInt32 *)(pData + 4);
	if (pData[0] == 'I' && pData[1] == 'I') {
		*pLittleEndian = 1;
		*pOffset = offset;
	} else {
		*pOffset = photoExportSwap32(offset);
	}
	return 0;
}

/* the format of the data, from its first bytes */
static __attribute__((noinline)) SceInt32 photoExportGetFormat(const void *pData)
{
	static const SceUInt8 pngSignature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
	static const SceUInt8 jpegSignature[2] = { 0xFF, 0xD8 };
	static const SceUInt8 tiffLittle[4] = { 'I', 'I', '*', 0 };
	static const SceUInt8 tiffBig[4] = { 'M', 'M', 0, '*' };

	if (sceClibMemcmp(pngSignature, pData, 8) == 0)
		return PHOTO_EXPORT_FORMAT_PNG;
	if (sceClibMemcmp(jpegSignature, pData, 2) == 0)
		return PHOTO_EXPORT_FORMAT_JPEG;
	if (sceClibMemcmp(tiffLittle, pData, 4) == 0 || sceClibMemcmp(tiffBig, pData, 4) == 0)
		return PHOTO_EXPORT_FORMAT_TIFF;
	if (sceClibMemcmp("GIF87a", pData, 6) == 0 || sceClibMemcmp("GIF89a", pData, 6) == 0)
		return PHOTO_EXPORT_FORMAT_GIF;
	if (sceClibMemcmp("BM", pData, 2) == 0)
		return PHOTO_EXPORT_FORMAT_BMP;
	return PHOTO_EXPORT_FORMAT_UNKNOWN;
}

/* the PNG header chunk */
static __attribute__((noinline)) SceInt32 photoExportPngInfo(const SceUInt8 *pData, SceInt32 size, PhotoExportImageInfo *pInfo)
{
	SceUInt32 width, height;

	if (size < 33)
		return PHOTO_ERROR_INVALID_DATA;
	width = photoExportSwap32(*(const SceUInt32 *)(pData + 16));
	height = photoExportSwap32(*(const SceUInt32 *)(pData + 20));
	if (width == 0 || height == 0 || width >= 0x80000000 || height >= 0x80000000)
		return PHOTO_ERROR_INVALID_DATA;
	pInfo->width = width;
	pInfo->height = height;
	pInfo->pngColourType = pData[25];
	pInfo->pngInterlace = pData[28];
	return 0;
}

/* one marker of a JPEG: its length, the frame header and the start of the scan */
static __attribute__((noinline)) SceInt32 photoExportJpegMarker(const SceUInt16 *pData, SceInt32 size, SceInt64 *pPosition,
				      SceUInt32 *pLength, SceInt32 *pScan, PhotoExportImageInfo *pInfo)
{
	SceInt64 position, next;
	SceInt32 marker;
	SceInt32 result = 0;
	SceUInt32 length;
	const SceUInt16 *pNext;

	position = *pPosition;
	next = position + 2;
	if (size < next)
		return PHOTO_ERROR_INVALID_DATA;
	marker = photoExportSwap16(*pData);
	*pPosition = next;
	pNext = pData + 1;
	if (marker != 0xFFD8 && marker != 0xFFD9) {
		if (size < position + 4)
			return PHOTO_ERROR_INVALID_DATA;
		length = photoExportSwap16(pData[1]);
		*pLength = length;
		pNext = pData + 2;
		if (length < 2)
			return PHOTO_ERROR_INVALID_DATA;
	}
	{
		switch (marker) {
		case 0xFFC0:
		case 0xFFC2:
			if (size >= position + 24) {
				SceUInt32 frameHeight = *(const SceUInt16 *)((const SceUInt8 *)pNext + 1);
				SceUInt32 frameWidth = *(const SceUInt16 *)((const SceUInt8 *)pNext + 3);

				pInfo->progressive = 0;
				pInfo->width = photoExportSwap16(frameWidth);
				pInfo->height = photoExportSwap16(frameHeight);
				if (marker == 0xFFC2) {
					pInfo->progressive = 1;
					if (((const SceUInt8 *)pNext)[5] == 3) {
						SceInt32 sampling = ((const SceUInt8 *)pNext)[7];
						SceInt32 sampling2 = ((const SceUInt8 *)pNext)[13];

						if (((const SceUInt8 *)pNext)[10] == 0x11 && sampling2 == 0x11) {
							switch (sampling) {
							case 0x11:
								pInfo->subsampling = 1;
								break;
							case 0x21:
								pInfo->subsampling = 2;
								break;
							case 0x22:
								pInfo->subsampling = 3;
								break;
							case 0x41:
								pInfo->subsampling = 4;
								break;
							default:
								break;
							}
						}
					}
				}
			} else {
				return PHOTO_ERROR_INVALID_DATA;
			}
			break;
		case 0xFFDA:
		case 0xFFD9:
			*pScan = 1;
			break;
		default:
			break;
		}
	}
	return result;
}

/* the entries of a TIFF directory: the width, the height and the photometric interpretation */
static __attribute__((noinline)) SceInt32 photoExportTiffEntries(const SceUInt16 *pEntry, SceInt32 littleEndian, SceInt32 count,
				       SceInt32 *pInfo)
{
	SceInt32 result = 0;
	SceInt32 i;

	for (i = 0; count > i; i++) {
		SceInt32 tag = pEntry[0];
		SceInt32 type = pEntry[1];
		SceUInt32 number = *(const SceUInt32 *)(pEntry + 2);
		SceUInt32 value = 0, short_ = 0;

		if (littleEndian == 0) {
			tag = photoExportSwap16(tag);
			type = photoExportSwap16(type);
			number = photoExportSwap32(number);
		}
		if (type == 0 || type > 12)
			return PHOTO_ERROR_INVALID_DATA;
		if (type == 3) {
			value = pEntry[4];
			if (littleEndian == 0)
				value = photoExportSwap16(value);
			short_ = value;
		} else if (type == 4) {
			value = *(const SceUInt32 *)(pEntry + 4);
			if (littleEndian == 0)
				value = photoExportSwap32(value);
		}
		pEntry += 6;
		switch (tag) {
		case 256:
			if (number == 1)
				pInfo[0] = value;
			else
				result = PHOTO_ERROR_INVALID_DATA;
			break;
		case 257:
			if (number == 1)
				pInfo[1] = value;
			else
				result = PHOTO_ERROR_INVALID_DATA;
			break;
		case 262:
			if (type == 3 && number == 1)
				pInfo[7] = short_;
			else
				result = PHOTO_ERROR_INVALID_DATA;
			break;
		default:
			break;
		}
	}
	return result;
}

/* the BMP header: the sizes and the colour depth */
static __attribute__((noinline)) SceInt32 photoExportBmpInfo(const SceUInt8 *pData, SceInt32 size, PhotoExportImageInfo *pInfo)
{
	SceUInt32 headerSize;
	SceInt32 height;

	if (size < 140)
		return PHOTO_ERROR_INVALID_DATA;
	headerSize = *(const SceUInt32 *)(pData + 14);
	if (headerSize == 124 || headerSize == 108 || headerSize == 64 || headerSize == 56
	    || headerSize == 40) {
		pInfo->width = *(const SceInt32 *)(pData + 18);
		height = *(const SceInt32 *)(pData + 22);
		pInfo->height = height;
		pInfo->bitCount = *(const SceInt16 *)(pData + 28);
		pInfo->compression = *(const SceInt32 *)(pData + 30);
	} else if (headerSize == 12) {
		pInfo->width = *(const SceInt16 *)(pData + 18);
		height = *(const SceInt16 *)(pData + 20);
		pInfo->height = height;
		pInfo->bitCount = *(const SceInt16 *)(pData + 24);
		pInfo->compression = 0;
	} else {
		height = pInfo->height;
		pInfo->height = height < 0 ? -height : height;
		return PHOTO_ERROR_INVALID_DATA;
	}
	if (height < 0)
		height = -height;
	pInfo->height = height;
	return 0;
}

/* whether the photo application shows an image of this size and kind */
static __attribute__((noinline)) SceBool photoExportCheckLimits(SceInt32 format, const PhotoExportImageInfo *pInfo)
{
	SceInt64 pixels = (SceInt64)pInfo->width * pInfo->height;
	SceInt64 limit;

	switch (format) {
	case PHOTO_EXPORT_FORMAT_PNG:
		if (pInfo->pngInterlace == 1)
			limit = 522240;
		else
			limit = pInfo->pngColourType == 3 ? 522240 : 1966080;
		return pixels > 0 && limit >= pixels;
	case PHOTO_EXPORT_FORMAT_JPEG:
		limit = 40000000;
		if ((pInfo->progressive & 1) != 0)
			limit = 522240;
		return pixels > 0 && limit >= pixels;
	case PHOTO_EXPORT_FORMAT_TIFF:
		limit = 1966080;
		if (pInfo->tiffPhotometric == 3)
			limit = 522240;
		return pixels > 0 && limit >= pixels;
	case PHOTO_EXPORT_FORMAT_BMP: {
		SceInt32 bits = pInfo->bitCount;

		limit = 0;
		switch (bits) {
		case 1:
		case 4:
		case 8:
		case 16:
			limit = 522240;
			break;
		case 24:
		case 32:
			limit = 1966080;
			break;
		default:
			break;
		}
		if (pixels > 0 && limit >= pixels) {
			SceInt32 compression = pInfo->compression;

			if (compression == 0 || ((bits == 16 || bits == 32) && compression == 3))
				return 1;
		}
		return 0;
	}
	case PHOTO_EXPORT_FORMAT_GIF:
		return pixels > 0 && pixels <= 522240;
	default:
		return 0;
	}
}

extern "C" SceInt32 scePhotoExportIsAvailableFromData(const void *photodata, SceInt32 photodataSize,
					   SceBool *isAvailable)
{
	PhotoExportImageInfo info;
	SceInt32 format, result, littleEndian, scan;
	SceInt64 position;
	SceUInt32 length;
	const SceUInt8 *pData = (const SceUInt8 *)photodata;
	const SceUInt16 *pMarker;

	*isAvailable = 0;
	if (photodata == NULL || photodataSize <= 0)
		return PHOTO_ERROR_PARAM;
	format = photoExportGetFormat(photodata);
	switch (format) {
	case PHOTO_EXPORT_FORMAT_PNG:
		result = photoExportPngInfo(pData, photodataSize, &info);
		break;
	case PHOTO_EXPORT_FORMAT_JPEG:
		scan = 0;
		position = 2;
		pMarker = (const SceUInt16 *)pData + 1;
		do {
			length = 0;
			result = photoExportJpegMarker(pMarker, photodataSize, &position, &length, &scan,
						       &info);
			if (photodataSize < position + length)
				return PHOTO_ERROR_INVALID_DATA;
			position += length;
			pMarker = (const SceUInt16 *)((const SceUInt8 *)pMarker + length + 2);
		} while ((scan | result) == 0);
		break;
	case PHOTO_EXPORT_FORMAT_TIFF:
		result = photoExportTiffHeader(pData, photodataSize, &littleEndian, &position);
		if (photodataSize >= position + 2) {
			SceUInt32 entries = *(const SceUInt16 *)(pData + position);

			if (littleEndian == 0)
				entries = photoExportSwap16(entries);
			if (photodataSize >= position + 2 + 12 * entries + 4)
				result = photoExportTiffEntries((const SceUInt16 *)(pData + position + 2),
								littleEndian, entries, (SceInt32 *)&info);
			else
				result = PHOTO_ERROR_INVALID_DATA;
		} else {
			result = PHOTO_ERROR_INVALID_DATA;
		}
		break;
	case PHOTO_EXPORT_FORMAT_BMP:
		result = photoExportBmpInfo(pData, photodataSize, &info);
		break;
	case PHOTO_EXPORT_FORMAT_GIF:
		result = PHOTO_ERROR_INVALID_DATA;
		if (photodataSize >= 16) {
			result = 0;
			info.width = ((const SceUInt16 *)pData)[3];
			info.height = ((const SceUInt16 *)pData)[4];
		}
		break;
	default:
		return PHOTO_ERROR_NOT_SUPPORTED;
	}
	if (result == 0)
		*isAvailable = photoExportCheckLimits(format, &info);
	return result;
}

/* the TIFF directory of a file */
static __attribute__((noinline)) SceInt32 photoExportTiffInfoFromFile(SceUID fd, SceInt32 *pInfo)
{
	SceUInt8 header[8];
	SceInt32 littleEndian;
	SceInt64 position;
	SceUInt16 entries;

	photoExportTiffHeader(header, sceIoRead(fd, header, 8), &littleEndian, &position);
	if ((SceInt32)sceIoLseek(fd, position, SCE_SEEK_SET) >= 0) {
		entries = 0;
		if (sceIoRead(fd, &entries, 2) > 0) {
			SceUInt32 count = entries;

			if (littleEndian == 0) {
				count = photoExportSwap16(count);
				entries = count;
			}
			SceUInt8 buffer[12 * count + 4];

			if (12 * count + 4 == sceIoRead(fd, buffer, 12 * count + 4))
				return photoExportTiffEntries((const SceUInt16 *)buffer, littleEndian, entries,
							      pInfo);
			return PHOTO_ERROR_INTERNAL;
		}
	}
	return PHOTO_ERROR_INTERNAL;
}

extern "C" SceInt32 scePhotoExportIsAvailableFromFile(const SceChar8 *filePath, SceBool *isAvailable)
{
	PhotoExportImageInfo info;
	SceUInt8 header[16];
	SceUInt8 buffer[140];
	SceInt32 format, result, jpegEnd, jpegSize;
	SceInt64 position;
	SceUInt32 length;
	SceUID fd;

	*isAvailable = 0;
	fd = sceIoOpen((const char *)filePath, SCE_O_RDONLY, 0400);
	if (fd < 0)
		return PHOTO_ERROR_FILE_NOT_FOUND;
	sceIoLseek(fd, 0, SCE_SEEK_SET);
	sceIoRead(fd, header, 16);
	format = photoExportGetFormat(header);
	sceIoLseek(fd, 0, SCE_SEEK_SET);
	switch (format) {
	case PHOTO_EXPORT_FORMAT_PNG:
		result = photoExportPngInfo(buffer, sceIoRead(fd, buffer, 33), &info);
		break;
	case PHOTO_EXPORT_FORMAT_JPEG:
		jpegSize = sceIoLseek(fd, 0, SCE_SEEK_SET);
		sceIoLseek(fd, 2, SCE_SEEK_SET);
		length = 0;
		jpegEnd = 0;
		position = 2;
		for (;;) {
			SceUInt8 marker[20];

			if (sceIoRead(fd, marker, 20) < 0) {
				result = PHOTO_ERROR_INTERNAL;
				break;
			}
			length = 0;
			result = photoExportJpegMarker((const SceUInt16 *)marker, jpegSize, &position, &length,
						       &jpegEnd, &info);
			position = sceIoLseek(fd, position + length, SCE_SEEK_SET);
			if (position < 0) {
				result = PHOTO_ERROR_INVALID_DATA;
				break;
			}
			if ((jpegEnd | result) != 0)
				break;
		}
		break;
	case PHOTO_EXPORT_FORMAT_TIFF:
		result = photoExportTiffInfoFromFile(fd, (SceInt32 *)&info);
		break;
	case PHOTO_EXPORT_FORMAT_BMP:
		result = photoExportBmpInfo(buffer, sceIoRead(fd, buffer, 140), &info);
		break;
	case PHOTO_EXPORT_FORMAT_GIF: {
		SceInt32 read = sceIoRead(fd, buffer, 16);

		result = PHOTO_ERROR_INVALID_DATA;
		if (read >= 16) {
			result = 0;
			info.width = ((const SceUInt16 *)buffer)[3];
			info.height = ((const SceUInt16 *)buffer)[4];
		}
		break;
	}
	default:
		result = PHOTO_ERROR_NOT_SUPPORTED;
		goto out;
	}
	if (result == 0)
		*isAvailable = photoExportCheckLimits(format, &info);
out:
	sceIoClose(fd);
	return result;
}

static __attribute__((noinline)) SceInt32 photoExportIpcSendData(IPMI::Client *pClient, void *pData, SceInt32 size,
				       SceInt32 index, SceUInt8 last, SceInt64 total, SceInt32 reserved,
				       const char *pName, SceInt32 reserved2, const char *pPhotoTitle,
				       const char *pGameTitle, const char *pGameComment,
				       const char *pFilePath)
{
	SceInt32 result;
	SceInt32 value = 0;
	PhotoExportSendHeader header;
	IPMI::DataInfo buffers[7];

	buffers[0].pData = &header;
	buffers[0].dataSize = 32;
	buffers[1].pData = pData;
	buffers[1].dataSize = size;
	header.size = size;
	header.index = index;
	header.last = last;
	header.total = total;
	header.reserved = reserved;
	header.reserved2 = reserved2;
	buffers[2].pData = (void *)pName;
	buffers[2].dataSize = photoExportSize(pName);
	buffers[3].pData = (void *)pPhotoTitle;
	buffers[3].dataSize = photoExportSize(pPhotoTitle);
	buffers[4].pData = (void *)pGameTitle;
	buffers[4].dataSize = photoExportSize(pGameTitle);
	buffers[5].pData = (void *)pGameComment;
	buffers[5].dataSize = photoExportSize(pGameComment);
	buffers[6].pData = (void *)pFilePath;
	buffers[6].dataSize = photoExportSize(pFilePath);
	result = pClient->invokeSyncMethod(PHOTO_EXPORT_METHOD_SEND_DATA, buffers, 7,
						 &value, NULL, 0);
	if (result == 0)
		result = value;
	return result;
}

static __attribute__((noinline)) SceInt32 photoExportIpcStart(IPMI::Client *pClient, SceInt32 *pRequestId,
				    const IPMI::EventFlagInfo *pEventFlag, SceInt32 count, void *pData)
{
	SceInt32 result;
	SceInt32 number;
	IPMI::DataInfo buffers[2];

	number = count;
	buffers[0].pData = &number;
	buffers[0].dataSize = 4;
	buffers[1].pData = pData;
	buffers[1].dataSize = 2064 * count;
	result = pClient->invokeAsyncMethod(PHOTO_EXPORT_METHOD_START, buffers, 2,
						  pRequestId, pEventFlag);
	if (result == 0)
		result = 0;
	return result;
}

static __attribute__((noinline)) SceInt32 photoExportIpcGetResult(IPMI::Client *pClient, SceInt32 requestId,
					SceInt32 *pCount, void *pData)
{
	SceInt32 result;
	SceInt32 value = 0;
	SceInt32 count;
	IPMI::BufferInfo buffers[2];

	count = *pCount;
	buffers[0].pBuffer = pCount;
	buffers[0].bufferSize = 4;
	buffers[1].pBuffer = pData;
	buffers[1].bufferSize = 1032 * count;
	result = pClient->tryGetResult(PHOTO_EXPORT_METHOD_GET_RESULT, requestId, &value,
					     buffers, 2);
	if (result == 0)
		result = value;
	return result;
}

static __attribute__((noinline)) SceInt32 photoExportIpcCancel(IPMI::Client *pClient)
{
	SceInt32 result;
	SceInt32 value = 0;

	result = pClient->invokeSyncMethod(PHOTO_EXPORT_METHOD_CANCEL, NULL, 0, &value,
						 NULL, 0);
	if (result == 0)
		result = value;
	return result;
}

static __attribute__((noinline)) SceInt32 photoExportIpcAbort(IPMI::Client *pClient)
{
	SceInt32 result;
	SceInt32 value = 0;

	result = pClient->invokeSyncMethod(PHOTO_EXPORT_METHOD_ABORT, NULL, 0, &value,
						 NULL, 0);
	if (result == 0)
		result = value;
	return result;
}

static __attribute__((noinline)) SceInt32 photoExportIpcGetProgress(IPMI::Client *pClient, SceInt32 *pProgress)
{
	SceInt32 result;
	SceInt32 value = 0;
	IPMI::BufferInfo buffer;

	buffer.pBuffer = pProgress;
	buffer.bufferSize = 4;
	result = pClient->invokeSyncMethod(PHOTO_EXPORT_METHOD_GET_PROGRESS, NULL, 0,
						 &value, &buffer, 1);
	if (result == 0)
		result = value;
	return result;
}

static __attribute__((noinline)) SceInt32 photoExportIpcSetThumbnail(IPMI::Client *pClient, SceInt32 index,
					   const char *pPath)
{
	SceInt32 value = 0;
	SceInt32 number;
	IPMI::DataInfo buffers[2];
	SceInt32 result;

	number = index;
	buffers[0].pData = &number;
	buffers[0].dataSize = 4;
	buffers[1].pData = (void *)pPath;
	buffers[1].dataSize = photoExportSize(pPath);
	result = pClient->invokeSyncMethod(PHOTO_EXPORT_METHOD_SET_THUMBNAIL, buffers,
						 2, &value, NULL, 0);
	if (result == 0)
		result = value;
	return result;
}

static __attribute__((noinline)) SceInt32 photoExportIpcStartVideo(IPMI::Client *pClient, SceInt32 *pRequestId,
					 const IPMI::EventFlagInfo *pEventFlag, SceInt32 kind, SceInt32 count,
					 void *pData)
{
	SceInt32 result;
	SceInt32 header[2];
	IPMI::DataInfo buffers[2];

	header[0] = kind;
	header[1] = count;
	buffers[0].pData = header;
	buffers[0].dataSize = 8;
	buffers[1].pData = pData;
	buffers[1].dataSize = 2052 * count;
	result = pClient->invokeAsyncMethod(PHOTO_EXPORT_METHOD_START_VIDEO, buffers, 2,
						  pRequestId, pEventFlag);
	if (result == 0)
		result = 0;
	return result;
}

static __attribute__((noinline)) SceInt32 photoExportIpcGetVideoResult(IPMI::Client *pClient, SceInt32 requestId,
					     SceInt32 *pCount, void *pData)
{
	SceInt32 result;
	SceInt32 value = 0;
	SceInt32 count;
	IPMI::BufferInfo buffers[2];

	count = *pCount;
	buffers[0].pBuffer = pCount;
	buffers[0].bufferSize = 4;
	buffers[1].pBuffer = pData;
	buffers[1].bufferSize = 8 * count;
	result = pClient->tryGetResult(PHOTO_EXPORT_METHOD_GET_VIDEO_RESULT, requestId,
					     &value, buffers, 2);
	if (result == 0)
		result = value;
	return result;
}

