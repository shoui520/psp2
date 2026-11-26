#include <stddef.h>
#include <scetypes.h>
#include <string.h>
#include <libkernel/libkernel.h>
#include <kernel/sysmem/memblock.h>
#include <kernel/threadmgr.h>
#include "ipmi/client.h"
#include "ipmi/ipmi_error.h"

#define PHOTO_EXPORT_CONVERT_HEAP_TYPE      0x0C20D060U
#define PHOTO_EXPORT_CONVERT_HEAP_SIZE      0x10000U
#define PHOTO_EXPORT_CONVERT_CLIENT_BUFSIZE 0x4400U
#define PHOTO_EXPORT_CONVERT_RETRY_LIMIT    10
#define PHOTO_EXPORT_CONVERT_RETRY_USEC     1000000U
#define PHOTO_EXPORT_CONVERT_POLL_USEC      16000U

#define PHOTO_EXPORT_CONVERT_METHOD_CANCEL  0x23460000U
#define PHOTO_EXPORT_CONVERT_METHOD_START   0x23460001U
#define PHOTO_EXPORT_CONVERT_METHOD_RESULT  0x23460002U

#define PHOTO_EXPORT_CONVERT_ERR_PARAM      ((int)0x80101A01)
#define PHOTO_EXPORT_CONVERT_ERR_NOMEM      ((int)0x80101A04)
#define PHOTO_EXPORT_CONVERT_ERR_SERVER     ((int)0x80101A05)
#define PHOTO_EXPORT_CONVERT_ERR_CANCELED   ((int)0x80101A0B)
#define PHOTO_EXPORT_CONVERT_ERR_INTERNAL   ((int)0x80101A0C)
#define PHOTO_EXPORT_CONVERT_ERR_NOT_INIT   ((int)0x80105907)
#define PHOTO_EXPORT_CONVERT_ERR_NO_REQ     ((int)0x8002000F)
#define PHOTO_EXPORT_CONVERT_ERR_BUSY       ((int)0x80020001)
#define PHOTO_EXPORT_CONVERT_ERR_APP_STOP   ((int)0x8080201B)

struct PhotoExportConvertParam {
	const char *srcPath;
	const char *dstPath;
	int reserved;
	int type;
};

struct PhotoExportConvertResult {
	int imageWidth;
	int imageHeight;
	int texWidth;
	int texHeight;
};

struct PhotoExportConvertLaunchParam {
	SceSize size;
	SceInt32 flags;
	SceInt32 initialState;
	SceUID appLaunchCallback;
	SceUID getParamCallback;
	SceInt32 timeoutMode;
	SceUInt8 padding[40];
};

struct PhotoExportConvertSession {
	IPMI::Client *client;
	SceInt32 requestId;
	void *mspace;
	SceUID memblockId;
	void *clientBuffer;
	void *workMemory;
};

struct PhotoExportConvertStartHeader {
	int reserved;
	int type;
};

extern "C" int sceAppMgrLaunchAppByUri2(const char *uri,
	const PhotoExportConvertLaunchParam *param);

namespace {

static SceSize stringLengthWithNul(const char *text)
{
	SceSize len = 0;

	while (text[len] != '\0') {
		++len;
	}
	return len + 1U;
}

static int photoExportConvertLaunchCb(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	if (notifyArg > 0) {
		*(int *)common = 1;
	}
	return 0;
}

static int photoExportConvertGetParamCb(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	if (notifyArg == 0) {
		*(int *)common = 2;
	} else if (notifyArg == PHOTO_EXPORT_CONVERT_ERR_APP_STOP) {
		*(int *)common = 0;
	}
	return 0;
}

static int photoExportConvertIpcCancel(IPMI::Client *client)
{
	int srvResult = 0;
	int err = client->invokeSyncMethod(PHOTO_EXPORT_CONVERT_METHOD_CANCEL,
		NULL, 0, &srvResult, NULL, 0);

	if (err == 0) {
		return srvResult;
	}
	return err;
}

static int photoExportConvertIpcStart(IPMI::Client *client, SceInt32 *outRequestId,
	const IPMI::EventFlagInfo *evfInfo, const char *srcPath, const char *dstPath,
	int reserved, int convertType)
{
	PhotoExportConvertStartHeader header;
	IPMI::DataInfo dataList[3];

	header.reserved = reserved;
	header.type = convertType;

	dataList[0].pData = &header;
	dataList[0].dataSize = sizeof(header);
	dataList[1].pData = srcPath;
	dataList[1].dataSize = stringLengthWithNul(srcPath);
	dataList[2].pData = dstPath;
	dataList[2].dataSize = stringLengthWithNul(dstPath);

	return client->invokeAsyncMethod(PHOTO_EXPORT_CONVERT_METHOD_START,
		dataList, 3, outRequestId, evfInfo);
}

static int photoExportConvertIpcGetResult(IPMI::Client *client, SceInt32 requestId,
	PhotoExportConvertResult *outResult)
{
	int srvResult = 0;
	IPMI::BufferInfo outBuf;

	outBuf.pBuffer = outResult;
	outBuf.bufferSize = sizeof(*outResult);
	outBuf.outSize = 0;

	int err = client->tryGetResult(PHOTO_EXPORT_CONVERT_METHOD_RESULT,
		requestId, &srvResult, &outBuf, 1);
	if (err == 0) {
		return srvResult;
	}
	return err;
}

static void releaseSessionMemory(PhotoExportConvertSession *session)
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

static int photoExportConvertOpenSession(PhotoExportConvertSession *session,
	SceBool (*cancelFunc)(void *), void *userdata)
{
	if (session->client != NULL) {
		return 0;
	}

	void *heapBase = session->workMemory;
	SceUID memblockId = 0;

	if (heapBase == NULL) {
		memblockId = sceKernelAllocMemBlock("ScePhotoExportHeap",
			PHOTO_EXPORT_CONVERT_HEAP_TYPE, PHOTO_EXPORT_CONVERT_HEAP_SIZE, NULL);
		if (memblockId < 0) {
			return PHOTO_EXPORT_CONVERT_ERR_NOMEM;
		}
		sceKernelGetMemBlockBase(memblockId, &heapBase);
	}

	void *mspace = sceClibMspaceCreate(heapBase, PHOTO_EXPORT_CONVERT_HEAP_SIZE);
	if (mspace == NULL) {
		if (memblockId != 0) {
			sceKernelFreeMemBlock(memblockId);
		}
		return PHOTO_EXPORT_CONVERT_ERR_NOMEM;
	}
	session->mspace = mspace;
	session->memblockId = memblockId;

	IPMI::Client::Config config;
	sceClibStrncpy(config.serviceName, "PhotoExportSrv", sizeof(config.serviceName));
	config.maxSyncCallInSize = 17408U;
	config.msgQueueCount = 1U;
	config.msgQueueBufferSize[0] = 16400U;
	config.budget = IPMI::USER_BUDGET;

	SceSize clientBufSize = IPMI::Client::getBufferSize(&config);
	void *clientMem = sceClibMspaceMalloc(session->mspace, clientBufSize);
	if (clientMem == NULL) {
		releaseSessionMemory(session);
		return PHOTO_EXPORT_CONVERT_ERR_NOMEM;
	}

	void *scratchMem = sceClibMspaceMalloc(session->mspace, PHOTO_EXPORT_CONVERT_CLIENT_BUFSIZE);
	session->clientBuffer = scratchMem;
	if (scratchMem == NULL) {
		sceClibMspaceFree(session->mspace, clientMem);
		releaseSessionMemory(session);
		return PHOTO_EXPORT_CONVERT_ERR_NOMEM;
	}

	int err = IPMI::Client::create(&session->client, &config, session, clientMem);
	if (err != 0) {
		sceClibMspaceFree(session->mspace, clientMem);
		sceClibMspaceFree(session->mspace, session->clientBuffer);
		session->clientBuffer = NULL;
		session->client = NULL;
		releaseSessionMemory(session);
		return err;
	}

	int launchState = 0;
	SceUID appLaunchCb = sceKernelCreateCallback("ScePhotoExportCallbackAppLaunch", 0,
		photoExportConvertLaunchCb, &launchState);
	SceUID getParamCb = sceKernelCreateCallback("ScePhotoExportCallbackGetParam", 0,
		photoExportConvertGetParamCb, &launchState);
	int connectErr = PHOTO_EXPORT_CONVERT_ERR_INTERNAL;
	int detailErr = PHOTO_EXPORT_CONVERT_ERR_INTERNAL;

	if (appLaunchCb > 0 && getParamCb > 0) {
		int attempt = 0;

		for (;;) {
			if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
				connectErr = PHOTO_EXPORT_CONVERT_ERR_CANCELED;
				break;
			}
			connectErr = session->client->connect(NULL, 0, &detailErr);
			if (connectErr != IPMI_ERROR_SERVER_NOT_FOUND || attempt >= PHOTO_EXPORT_CONVERT_RETRY_LIMIT) {
				break;
			}
			++attempt;
			if (launchState == 0) {
				PhotoExportConvertLaunchParam launchParam;

				sceClibMemset(&launchParam, 0, sizeof(launchParam));
				launchParam.size = sizeof(launchParam);
				launchParam.flags = 0;
				launchParam.initialState = -1;
				launchParam.appLaunchCallback = appLaunchCb;
				launchParam.getParamCallback = getParamCb;
				launchParam.timeoutMode = -1;
				sceAppMgrLaunchAppByUri2("photoexport:", &launchParam);
			}
			sceKernelDelayThread(PHOTO_EXPORT_CONVERT_RETRY_USEC);
			if (launchState != 2) {
				sceKernelCheckCallback();
			}
		}
	}

	if (appLaunchCb > 0) {
		sceKernelDeleteCallback(appLaunchCb);
	}
	if (getParamCb > 0) {
		sceKernelDeleteCallback(getParamCb);
	}

	if (connectErr != 0) {
		int mappedErr = PHOTO_EXPORT_CONVERT_ERR_INTERNAL;

		if (connectErr == PHOTO_EXPORT_CONVERT_ERR_CANCELED) {
			mappedErr = PHOTO_EXPORT_CONVERT_ERR_CANCELED;
		} else if (connectErr == IPMI_ERROR_SERVER_NOT_FOUND) {
			mappedErr = PHOTO_EXPORT_CONVERT_ERR_SERVER;
		} else if (connectErr == IPMI_ERROR_CONNECTION_REFUSED) {
			mappedErr = detailErr;
		}
		session->client->destroy();
		sceClibMspaceFree(session->mspace, clientMem);
		sceClibMspaceFree(session->mspace, session->clientBuffer);
		session->clientBuffer = NULL;
		session->client = NULL;
		releaseSessionMemory(session);
		return mappedErr;
	}
	return 0;
}

static int photoExportConvertCloseSession(PhotoExportConvertSession *session)
{
	if (session->client == NULL) {
		return PHOTO_EXPORT_CONVERT_ERR_NOT_INIT;
	}
	int err = session->client->disconnect();
	if (err != 0) {
		return err;
	}
	err = session->client->destroy();
	if (err != 0) {
		return err;
	}
	sceClibMspaceFree(session->mspace, session->client);
	session->client = NULL;
	if (session->clientBuffer != NULL) {
		sceClibMspaceFree(session->mspace, session->clientBuffer);
		session->clientBuffer = NULL;
	}
	releaseSessionMemory(session);
	return 0;
}

static int photoExportConvertBegin(PhotoExportConvertSession *session,
	const PhotoExportConvertParam *param)
{
	return photoExportConvertIpcStart(session->client, &session->requestId, NULL,
		param->srcPath, param->dstPath, param->reserved, param->type);
}

static int photoExportConvertPoll(PhotoExportConvertSession *session,
	PhotoExportConvertResult *outResult)
{
	if (session->requestId == -1) {
		return PHOTO_EXPORT_CONVERT_ERR_NO_REQ;
	}
	return photoExportConvertIpcGetResult(session->client, session->requestId, outResult);
}

static int photoExportConvertCancel(PhotoExportConvertSession *session)
{
	return photoExportConvertIpcCancel(session->client);
}

} /* namespace */

int photoExportConvertFile(const PhotoExportConvertParam *param, void *workMemory,
	SceBool (*cancelFunc)(void *), void *userdata, PhotoExportConvertResult *result)
{
	if (param == NULL || param->srcPath == NULL || param->dstPath == NULL) {
		return PHOTO_EXPORT_CONVERT_ERR_PARAM;
	}

	PhotoExportConvertSession session;
	PhotoExportConvertResult localResult;

	session.client = NULL;
	session.requestId = -1;
	session.mspace = NULL;
	session.memblockId = 0;
	session.clientBuffer = NULL;
	session.workMemory = workMemory;
	sceClibMemset(&localResult, 0, sizeof(localResult));

	int err = photoExportConvertOpenSession(&session, cancelFunc, userdata);
	if (err != 0) {
		return err;
	}

	int runErr = photoExportConvertBegin(&session, param);
	if (runErr == 0) {
		int cancelSent = 0;

		do {
			if (cancelFunc != NULL && cancelSent == 0 && cancelFunc(userdata) != 0) {
				photoExportConvertCancel(&session);
				cancelSent = 1;
			}
			sceKernelDelayThread(PHOTO_EXPORT_CONVERT_POLL_USEC);
			runErr = photoExportConvertPoll(&session, &localResult);
		} while (runErr == PHOTO_EXPORT_CONVERT_ERR_BUSY);

		if (result != NULL) {
			*result = localResult;
		}
	}

	photoExportConvertCloseSession(&session);
	return runErr;
}
