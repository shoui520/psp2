/*
 * SceMusicExport (vs0:sys/external/libSceMusicExport.suprx): exports a music file to the system's
 * music content through the media service (SceAvMediaSrv), a client of its IPMI service. No Sony
 * record names the library's files; the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <string.h>
#include <music_export.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <kernel/sysmem/memblock.h>
#include <kernel/threadmgr.h>
#include "../../../../../vsh/lowlayer/src/ipmi/ipmi_error.h"
#include "music_export_internal.h"

/* the heap's memory block: user read-write, 4 KiB pages (the SDK's name for the type) */
#define MUSIC_EXPORT_HEAP_MEMBLOCK_TYPE		0x0C20D060

/* the registry settings that stop an export (the system's own names for them are not known) */
#define MUSIC_EXPORT_PARAM_MEDIA_FULL		1395244
#define MUSIC_EXPORT_PARAM_CONTENT_FULL		8196804
#define MUSIC_EXPORT_PARAM_MEDIA_NOT_EXIST	9839568

/* how often and how long the library waits for the media service to start */
#define MUSIC_EXPORT_CONNECT_RETRY_COUNT	20
#define MUSIC_EXPORT_CONNECT_RETRY_USEC		1000000
#define MUSIC_EXPORT_POLL_USEC			16000

/* the size of a piece of the file sent to the media service */
#define MUSIC_EXPORT_PIECE_SIZE			0x4000

/* the state the callbacks set while the media service starts */
#define MUSIC_EXPORT_LAUNCH_STATE_NONE		0
#define MUSIC_EXPORT_LAUNCH_STATE_LAUNCHED	1
#define MUSIC_EXPORT_LAUNCH_STATE_READY		2

/* the application's launch by URI (SceAppMgr): the callbacks it reports to */
typedef struct MusicExportLaunchParam {
	SceSize size;
	SceInt32 reserved04;
	SceInt32 reserved08;
	SceUID appLaunchCallback;
	SceUID getParamCallback;
	SceInt32 reserved14;
	SceUInt8 reserved18[40];
} MusicExportLaunchParam;

extern "C" int sceAppMgrLaunchAppByUri2(const char *uri, const MusicExportLaunchParam *param);

extern "C" int sceAppMgrSendNotificationRequest(const void *request, int flags);
extern "C" int sceRegMgrSystemParamGetInt(int id, int *pValue);

/* the notification request the library sends (SceAppMgr): the text the system shows */
typedef struct MusicExportNotification {
	SceUInt32 type;
	SceUInt8 payload[60];
} MusicExportNotification;

/* the paths the library refuses */
static const char *musicExportForbiddenPrefixes[3] = { "music0:", "vs0:", "vd0:" };

static __attribute__((noinline)) void musicExportFree(MusicExportSession *session, void *ptr)
{
	sceClibMspaceFree(session->mspace, ptr);
}

static __attribute__((noinline)) void *musicExportAlloc(MusicExportSession *session, SceSize size)
{
	return sceClibMspaceMalloc(session->mspace, size);
}

/* releases the heap */
static void musicExportDestroyHeap(MusicExportSession *session)
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

static SceInt32 musicExportAppLaunchCallback(SceUID notifyId, SceInt32 notifyCount,
					     SceInt32 notifyArg, void *pCommon)
{
	if (notifyArg > 0)
		*(SceInt32 *)pCommon = MUSIC_EXPORT_LAUNCH_STATE_LAUNCHED;
	return 0;
}

static SceInt32 musicExportGetParamCallback(SceUID notifyId, SceInt32 notifyCount,
					    SceInt32 notifyArg, void *pCommon)
{
	if (notifyArg != 0) {
		if (notifyArg == (SceInt32)0x8080201B)
			*(SceInt32 *)pCommon = MUSIC_EXPORT_LAUNCH_STATE_NONE;
	} else {
		*(SceInt32 *)pCommon = MUSIC_EXPORT_LAUNCH_STATE_READY;
	}
	return 0;
}

/* gives the session its heap and connects it to the media service, starting the service if need be */
static int musicExportOpenSession(MusicExportSession *session, SceMusicExportCancelFunc cancelFunc,
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
	MusicExportLaunchParam launchParam;
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
		memblockId = sceKernelAllocMemBlock("SceMusicExportHeap",
						    MUSIC_EXPORT_HEAP_MEMBLOCK_TYPE,
						    MUSIC_EXPORT_HEAP_SIZE, NULL);
		if (memblockId < 0)
			return SCE_MUSIC_EXPORT_ERROR_NO_MEMORY;
		sceKernelGetMemBlockBase(memblockId, &pBase);
	}
	mspace = sceClibMspaceCreate(pBase, MUSIC_EXPORT_HEAP_SIZE);
	if (mspace == NULL) {
		if (memblockId != 0)
			sceKernelFreeMemBlock(memblockId);
		return SCE_MUSIC_EXPORT_ERROR_NO_MEMORY;
	}
	session->mspace = mspace;
	session->memblockId = memblockId;
	IPMI::Client::Config config;

	sceClibStrncpy(config.serviceName, "SceAvMediaSrv", sizeof(config.serviceName));
	config.maxAsyncCallInSize = 11944;
	config.maxAsyncCallOutSize = 11344;
	config.maxSyncCallInSize = 18432;
	config.maxSyncCallOutSize = 4096;
	config.budget = IPMI::USER_BUDGET;
	pBuffer = sceClibMspaceMalloc(session->mspace, IPMI::Client::getBufferSize(&config));
	if (pBuffer == NULL) {
		musicExportDestroyHeap(session);
		return SCE_MUSIC_EXPORT_ERROR_NO_MEMORY;
	}
	session->pClientBuffer = sceClibMspaceMalloc(session->mspace, 11944);
	if (session->pClientBuffer == NULL) {
		sceClibMspaceFree(session->mspace, pBuffer);
		musicExportDestroyHeap(session);
		return SCE_MUSIC_EXPORT_ERROR_NO_MEMORY;
	}
	ret = IPMI::Client::create(&session->pClient, &config, session, pBuffer);
	if (ret != 0) {
		sceClibMspaceFree(session->mspace, pBuffer);
		sceClibMspaceFree(session->mspace, session->pClientBuffer);
		session->pClientBuffer = NULL;
		session->pClient = NULL;
		musicExportDestroyHeap(session);
		return ret;
	}
	launchState = MUSIC_EXPORT_LAUNCH_STATE_NONE;
	appLaunchCallback = sceKernelCreateCallback("SceMusicExportCallbackAppLaunch", 0,
						    musicExportAppLaunchCallback, &launchState);
	getParamCallback = sceKernelCreateCallback("SceMusicExportCallbackGetParam", 0,
						   musicExportGetParamCallback, &launchState);
	error = SCE_MUSIC_EXPORT_ERROR_INTERNAL;
	connectResult = SCE_MUSIC_EXPORT_ERROR_INTERNAL;
	if (appLaunchCallback > 0) {
		if (getParamCallback > 0) {
			retry = 0;
			for (;;) {
				if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
					error = SCE_MUSIC_EXPORT_ERROR_CANCELED;
					break;
				}
				error = session->pClient->connect(NULL, 0, &connectResult);
				if (error != IPMI_ERROR_SERVER_NOT_FOUND ||
				    retry >= MUSIC_EXPORT_CONNECT_RETRY_COUNT)
					break;
				retry++;
				if (launchState == MUSIC_EXPORT_LAUNCH_STATE_NONE) {
					sceClibMemset(&launchParam, 0, sizeof(launchParam));
					launchParam.reserved08 = -1;
					launchParam.appLaunchCallback = appLaunchCallback;
					launchParam.size = sizeof(launchParam);
					launchParam.reserved04 = 0;
					launchParam.getParamCallback = getParamCallback;
					launchParam.reserved14 = -1;
					sceAppMgrLaunchAppByUri2("avmedia_service:", &launchParam);
				}
				sceKernelDelayThread(MUSIC_EXPORT_CONNECT_RETRY_USEC);
				if (launchState == MUSIC_EXPORT_LAUNCH_STATE_READY)
					continue;
				sceKernelCheckCallback();
			}
		}
		sceKernelDeleteCallback(appLaunchCallback);
	}
	if (getParamCallback > 0)
		sceKernelDeleteCallback(getParamCallback);
	if (error != 0) {
		ret = SCE_MUSIC_EXPORT_ERROR_INTERNAL;
		switch (error) {
		case SCE_MUSIC_EXPORT_ERROR_CANCELED:
			ret = SCE_MUSIC_EXPORT_ERROR_CANCELED;
			break;
		case IPMI_ERROR_SERVER_NOT_FOUND:
			ret = SCE_MUSIC_EXPORT_ERROR_SERVER_DOWN;
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
		musicExportDestroyHeap(session);
		return ret;
	}
	session->requestId = -1;
	return 0;
}

/* disconnects and frees what the session took */
static int musicExportCloseSession(MusicExportSession *session)
{
	void *pClientBuffer;

	if (session->pClient == NULL)
		return SCE_MUSIC_EXPORT_ERROR_INTERNAL;
	session->pClient->disconnect();
	session->pClient->destroy();
	sceClibMspaceFree(session->mspace, session->pClient);
	pClientBuffer = session->pClientBuffer;
	session->pClient = NULL;
	if (pClientBuffer != NULL) {
		sceClibMspaceFree(session->mspace, pClientBuffer);
		session->pClientBuffer = NULL;
	}
	musicExportDestroyHeap(session);
	return 0;
}

static __attribute__((noinline)) int musicExportSendData(MusicExportSession *session, const void *pData, SceUInt32 size,
			       SceUInt32 index, SceUInt8 last, SceOff total,
			       const char *pExtension, SceUInt32 reserved)
{
	return musicExportIpcSendData(session->pClient, pData, size, index, last, total, pExtension,
				      reserved, session->pPath);
}

static __attribute__((noinline)) int musicExportStart(MusicExportSession *session, SceUInt32 count,
			    const MusicExportItem *pItems, SceUInt32 mode)
{
	return musicExportIpcStart(session->pClient, &session->requestId, NULL, count, pItems, mode);
}

static __attribute__((noinline)) int musicExportGetResult(MusicExportSession *session, SceUInt32 *pCount,
				MusicExportResult *pResults)
{
	if (session->requestId == -1)
		return IPMI_ERROR_NOT_INITIALIZED;
	return musicExportIpcGetResult(session->pClient, session->requestId, pCount, pResults);
}

static __attribute__((noinline)) int musicExportCancel(MusicExportSession *session)
{
	return musicExportIpcCancel(session->pClient);
}

static __attribute__((noinline)) int musicExportGetProgress(MusicExportSession *session, SceInt32 *pProgress)
{
	return musicExportIpcGetProgress(session->pClient, pProgress);
}

/* the path the session sends along with the data (the empty string where there is none) */
static __attribute__((noinline)) int musicExportSetPath(MusicExportSession *session, const SceMusicExportParam *param,
			      const char *pPath)
{
	if (param != NULL)
		session->pPath = pPath != NULL ? pPath : "";
	else
		session->pPath = pPath != NULL ? pPath : "";
	return 0;
}

/* starts the export of the items and waits for its end, reporting its progress */
static int musicExportRun(MusicExportSession *session, SceInt32 count,
			  const MusicExportItem *pItems, SceUInt32 mode,
			  SceMusicExportCancelFunc cancelFunc,
			  SceMusicExportProgressInfoFunc progressFunc, void *userdata,
			  MusicExportResult *pOut)
{
	MusicExportResult *pResults;
	SceUInt32 resultCount;
	SceInt32 progress;
	SceInt32 lastProgress;
	int canceled;
	int ret;

	pResults = (MusicExportResult *)musicExportAlloc(session,
						     sizeof(MusicExportResult) * MUSIC_EXPORT_MAX_ITEMS);
	if (pResults == NULL)
		return SCE_MUSIC_EXPORT_ERROR_NO_MEMORY;
	ret = musicExportStart(session, count, pItems, mode);
	if (ret == 0) {
		canceled = 0;
		lastProgress = 0;
		do {
			if (cancelFunc != NULL && canceled == 0 && cancelFunc(userdata) != 0) {
				musicExportCancel(session);
				canceled = 1;
			}
			if (pItems->mode != 1 && progressFunc != NULL) {
				progress = 0;
				if (musicExportGetProgress(session, &progress) == 0 &&
				    lastProgress != progress) {
					progressFunc(userdata,
						     (SceInt32)((float)progress / (float)count * 100.0f));
					lastProgress = progress;
				}
			}
			sceKernelDelayThread(MUSIC_EXPORT_POLL_USEC);
			resultCount = count;
			ret = musicExportGetResult(session, &resultCount, pResults);
		} while (ret == IPMI_ERROR_WOULD_BLOCK);
		if (ret == 0)
			sceClibMemcpy_safe(pOut, pResults, sizeof(MusicExportResult) * resultCount);
	}
	if (pResults != NULL)
		musicExportFree(session, pResults);
	return ret;
}

/* not in the SDK: the items to export in one call (at most 10), each answered in pResults */
extern "C" SceInt32 SceMusicExport_D1011570(const MusicExportItem *pItems, SceUInt32 count,
					    void *workMemory,
					    SceMusicExportCancelFunc cancelFunc,
					    SceMusicExportProgressInfoFunc progressFunc,
					    void *userdata, SceInt32 mode,
					    MusicExportResult *pResults)
{
	MusicExportSession session;
	SceUInt32 i;
	MusicExportResult *pResult;
	int ret;

	if (pItems == NULL)
		return SCE_MUSIC_EXPORT_ERROR_PARAM;
	if (count == 0 || count > MUSIC_EXPORT_MAX_ITEMS)
		return SCE_MUSIC_EXPORT_ERROR_PARAM;
	if (mode <= 1) {
		if (mode < 0)
			return SCE_MUSIC_EXPORT_ERROR_PARAM;
	} else if (mode > 2) {
		return SCE_MUSIC_EXPORT_ERROR_PARAM;
	}
	if (pResults != NULL) {
		pResult = pResults;
		for (i = count; i != 0; i--) {
			pResult->reserved0 = -1;
			pResult->reserved1 = -1;
			sceClibMemset(pResult, 0, SCE_MUSIC_EXPORT_MAX_FS_PATH);
			pResult++;
		}
	}
	session.pClient = NULL;
	session.requestId = -1;
	session.mspace = NULL;
	session.memblockId = 0;
	session.pClientBuffer = NULL;
	session.pWorkMemory = workMemory;
	ret = musicExportOpenSession(&session, cancelFunc, userdata);
	if (ret == 0) {
		int result = musicExportRun(&session, count, pItems, mode, cancelFunc, progressFunc,
					    userdata, pResults);

		musicExportCloseSession(&session);
		ret = result;
	}
	return ret;
}

extern "C" SceInt32 sceMusicExportFromFile(const SceChar8 *musicfilePath,
					   const SceMusicExportParam *param, void *workMemory,
					   SceMusicExportCancelFunc cancelFunc,
					   SceMusicExportProgressInfoFunc progressFunc,
					   void *userdata, SceChar8 *exportedPath,
					   SceInt32 exportedPathLength)
{
	const char *path = (const char *)musicfilePath;
	char *outPath = (char *)exportedPath;
	MusicExportNotification notice = { 0 };
	MusicExportSession session;
	MusicExportItem item;
	SceIoStat stat;
	char extension[8];
	SceUInt32 i;
	SceSize length;
	SceSize prefixLength;
	int value;
	int ret;

	sceClibStrncpy((char *)notice.payload,
		       "Music:\nRequire TRC R3146 test\nsceMusicExportFromFile()",
		       sizeof(notice.payload));
	notice.payload[sizeof(notice.payload) - 1] = '\0';
	sceAppMgrSendNotificationRequest(&notice, 1);
	value = 0;
	if (sceRegMgrSystemParamGetInt(MUSIC_EXPORT_PARAM_MEDIA_FULL, &value) == 0 && value == 1)
		return SCE_MUSIC_EXPORT_ERROR_MEDIA_FULL;
	value = 0;
	if (sceRegMgrSystemParamGetInt(MUSIC_EXPORT_PARAM_CONTENT_FULL, &value) == 0 && value == 1)
		return SCE_MUSIC_EXPORT_ERROR_CONTENT_FULL;
	value = 0;
	if (sceRegMgrSystemParamGetInt(MUSIC_EXPORT_PARAM_MEDIA_NOT_EXIST, &value) == 0 && value == 1)
		return SCE_MUSIC_EXPORT_ERROR_MEDIA_NOT_EXIST;
	if (path == NULL)
		return SCE_MUSIC_EXPORT_ERROR_PARAM;
	if (exportedPathLength < 0)
		return SCE_MUSIC_EXPORT_ERROR_PARAM;
	if (outPath == NULL)
		return SCE_MUSIC_EXPORT_ERROR_PARAM;
	if (sceClibStrnlen(path, SCE_MUSIC_EXPORT_MAX_FS_PATH) >= SCE_MUSIC_EXPORT_MAX_FS_PATH)
		return SCE_MUSIC_EXPORT_ERROR_INVALID_PATH;
	for (i = 0; i < 3; i++) {
		prefixLength = sceClibStrnlen(musicExportForbiddenPrefixes[i], 7);
		if (sceClibStrncmp(path, musicExportForbiddenPrefixes[i], prefixLength) == 0)
			return SCE_MUSIC_EXPORT_ERROR_INVALID_PATH;
	}
	if (SceLibKernel_9F793F84() >= 0x2000000 && param != NULL) {
		for (i = 0; i < sizeof(param->reserved); i++) {
			if (param->reserved[i] != 0)
				return SCE_MUSIC_EXPORT_ERROR_PARAM;
		}
	}
	stat.st_size = 0;
	ret = SCE_MUSIC_EXPORT_ERROR_FILE_NOT_FOUND;
	if (sceIoGetstat(path, &stat) == 0) {
		int sizeError = 0;

		if (stat.st_size >= 0x80000000LL)
			sizeError = SCE_MUSIC_EXPORT_ERROR_NOT_SUPPORTED_FORMAT;
		ret = sizeError;
	}
	if (ret != 0)
		return ret;
	sceClibMemset(extension, 0, 6);
	length = sceClibStrnlen(path, SCE_MUSIC_EXPORT_MAX_FS_PATH);
	{
		SceInt32 index = length - 1;

		if (index > (SceInt32)(length - 6)) {
			const char *p = &path[index];
			SceInt32 n = 0;

			while (*p != '.') {
				index--;
				p--;
				n++;
				if (index <= (SceInt32)(length - 6))
					goto noExtension;
			}
			sceClibStrncpy(extension, &path[index], n + 1);
		}
	}
noExtension:
	if (extension[0] == '\0' ||
	    (sceClibStrncasecmp(extension, ".mp3", 4) != 0 &&
	     sceClibStrncasecmp(extension, ".m4a", 4) != 0 &&
	     sceClibStrncasecmp(extension, ".3gp", 4) != 0 &&
	     sceClibStrncasecmp(extension, ".wav", 4) != 0))
		return SCE_MUSIC_EXPORT_ERROR_NOT_SUPPORTED_FORMAT;
	for (;;) {
		MusicExportResult *pResults;
		SceUID fd;
		SceOff size;
		void *buf;
		SceInt32 pieces, piece, lastPiece, lastPercent;

		session.pClient = NULL;
		session.requestId = -1;
		session.mspace = NULL;
	session.memblockId = 0;
	session.pClientBuffer = NULL;
		session.pWorkMemory = workMemory;
		ret = musicExportOpenSession(&session, cancelFunc, userdata);
		if (ret != 0)
			return ret;
		if (musicExportSetPath(&session, param, path) != 0) {
			musicExportCloseSession(&session);
			return SCE_MUSIC_EXPORT_ERROR_PARAM;
		}
		pResults = (MusicExportResult *)musicExportAlloc(&session, 0x2850);
		if (pResults == NULL) {
			musicExportCloseSession(&session);
			return SCE_MUSIC_EXPORT_ERROR_NO_MEMORY;
		}
		fd = sceIoOpen(path, SCE_O_RDONLY, 0);
		if (fd >= 0) {
			size = sceIoLseek(fd, 0, SCE_SEEK_END);
			if (size >= 0) {
				sceIoLseek(fd, 0, SCE_SEEK_SET);
				buf = musicExportAlloc(&session, MUSIC_EXPORT_PIECE_SIZE);
				if (buf != NULL) {
					pieces = (SceInt32)(size / MUSIC_EXPORT_PIECE_SIZE);
					if (size % MUSIC_EXPORT_PIECE_SIZE != 0)
						pieces++;
					lastPiece = pieces - 1;
					piece = 0;
					ret = 0;
					if (lastPiece > 0) {
						lastPercent = 0;
						do {
							ret = sceIoRead(fd, buf, MUSIC_EXPORT_PIECE_SIZE);
							if (ret < 0)
								break;
							ret = musicExportSendData(&session, buf,
										  MUSIC_EXPORT_PIECE_SIZE,
										  piece, 0, size,
										  extension, 0);
							if (ret != 0)
								break;
							if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
								ret = SCE_MUSIC_EXPORT_ERROR_CANCELED;
								break;
							}
							piece++;
							if (progressFunc != NULL &&
							    lastPercent != (SceInt32)((float)piece / (float)pieces * 90.0f)) {
								progressFunc(userdata,
									     (SceInt32)((float)piece / (float)pieces * 90.0f));
								lastPercent = (SceInt32)((float)piece / (float)pieces * 90.0f);
							}
						} while (piece < lastPiece);
					}
					if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
						ret = SCE_MUSIC_EXPORT_ERROR_CANCELED;
					} else {
						if (ret == 0) {
							ret = sceIoRead(fd, buf, size - ((SceOff)piece << 14));
							if (ret >= 0)
								ret = musicExportSendData(&session, buf,
											  size - ((SceOff)piece << 14),
											  piece, 1, size,
											  extension, 0);
						}
						if (progressFunc != NULL)
							progressFunc(userdata, 90);
					}
					sceIoClose(fd);
					musicExportFree(&session, buf);
				} else {
					sceIoClose(fd);
					ret = SCE_MUSIC_EXPORT_ERROR_NO_MEMORY;
				}
			} else {
				ret = (int)size;
				sceIoClose(fd);
			}
		} else {
			ret = SCE_MUSIC_EXPORT_ERROR_FILE_NOT_FOUND;
		}
		if (ret == 0) {
			if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
				ret = SCE_MUSIC_EXPORT_ERROR_CANCELED;
				goto clearPath;
			}
			item.mode = 1;
			ret = musicExportRun(&session, 1, &item, 0, cancelFunc, NULL, userdata,
					     pResults);
			if (progressFunc != NULL)
				progressFunc(userdata, 100);
			if (ret == 0) {
				if (outPath != NULL && exportedPathLength > 0)
					sceClibStrncpy(outPath, pResults->path, exportedPathLength);
				goto done;
			}
		}
		if (ret != IPMI_ERROR_TERMINATED) {
clearPath:
			if (outPath != NULL && exportedPathLength > 0)
				*outPath = '\0';
		}
done:
		musicExportFree(&session, pResults);
		musicExportCloseSession(&session);
		if (ret != IPMI_ERROR_TERMINATED)
			return ret;
	}
}
