/*
 * np_game_presence.cpp: SceNpGamePresence background WebAPI presence updater and
 * NpIpc2 service (0x20000..0x20002) posting/deleting version 1 presence/gameTitleInfo.
 * 3.65-CEX: 0x813B9D04..0x813BA81B
 */
#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <ipmi/session.h>
#include <job/job.h>
#include <std/stdio.h>
#include <std/string.h>
#include "../ipc/np_ipc_internal.h"
#include "../np_ipc2/np_ipc2_internal.h"

#define NP_GAME_PRESENCE_ERR_OUT_OF_MEMORY	(-2141871101)	/* 0x8055A403 */
#define NP_GAME_PRESENCE_ERR_INVALID_ACTION	(-2141871102)	/* 0x8055A402 */
#define NP_GAME_PRESENCE_ERR_NO_TITLE		(-2141871096)	/* 0x8055A408 */
#define NP_GAME_PRESENCE_ERR_RETRY_NEEDED	(-2141871095)	/* 0x8055A409 */
#define NP_GAME_PRESENCE_ERR_RETRY_EXHAUSTED	(-2141871094)	/* 0x8055A40A */
#define NP_GAME_PRESENCE_ERR_ALREADY_ONLINE	(-2141871093)	/* 0x8055A40B */
#define NP_GAME_PRESENCE_ERR_IGNORED_TITLE	(-2141871091)	/* 0x8055A40D */
#define NP_GAME_PRESENCE_ERR_WEBAPI_CONFLICT	(-2111822848)	/* 0x82202000 */
#define NP_GAME_PRESENCE_ERR_ALLOC_FAILED	(-2141911804)	/* 0x80550004 */
#define NP_GAME_PRESENCE_IPMI_UNSUPPORTED	(-2147351103)	/* 0x800205C1 */

#define NP_GAME_PRESENCE_IPC_SET_ONLINE		0x20000U
#define NP_GAME_PRESENCE_IPC_SET_TITLE_ID	0x20001U
#define NP_GAME_PRESENCE_IPC_SET_TITLE_ASYNC	0x20002U

enum GamePresenceAction {
	GAME_PRESENCE_ACTION_NONE	= 0,
	GAME_PRESENCE_ACTION_SET_ONLINE	= 1,
	GAME_PRESENCE_ACTION_SET_EXIT	= 2,
	GAME_PRESENCE_ACTION_SIGNED_OUT	= 3
};

struct GamePresenceWebApiContentParam {
	SceSize contentLength;
	const char *contentType;
	SceUInt32 reserved[4];
};

struct NpImpsAppTitleInfo {
	char titleId[10];
	char titleName[144];
	SceUInt8 valid;
	SceUInt8 padding;
};

extern "C" {
extern NpIpc2Allocator *SceNpCommonPs4_53484B79(void);
extern void *SceNpCommonPs4_4A93F656(void *pointer, SceSize size);
extern void SceNpCommonPs4_6106C031(void *pointer);
extern int SceNpCommonPs4_9AD6061C(char *dst, SceSize dstSize, const char *src, SceSize srcLen);
extern int SceNpCommonPs4_4858EFB6(SceKernelLwMutexWork *work, const char *name, char recursive);

extern int SceNpWebApi_0D910413(int libCtxId, const char *apiGroup, const char *path,
				int method, const void *contentParam, SceSize contentLength,
				SceInt64 *outRequestId);
extern int sceNpWebApiSendRequest(SceInt64 requestId, const void *data, SceSize dataSize);
extern int sceNpWebApiGetHttpStatusCode(SceInt64 requestId, int *outStatusCode);
extern int sceNpWebApiReadData(SceInt64 requestId, void *buffer, SceSize bufferSize);
extern int sceNpWebApiGetErrorCode(int httpStatusCode, const void *body, SceSize bodySize);
extern int sceNpWebApiDeleteRequest(SceInt64 requestId);
}

extern int ipmiAsyncNotSupported(void);
extern int shellNpGetWebApiLibCtx(int *outLibCtx);
extern paf::job::JobQueue *shellGetNpBackgroundJobQueue(void);
extern int npImpsSvcGetAppTitleInfo(SceUID processId, NpImpsAppTitleInfo *outInfo);
extern int npImpsSvcSetGameTitle(int appId);
extern int npImpsSvcClearGameTitle(void);

typedef int (*GamePresenceFinishCallback)(IPMI::Session *session, int requestId);

struct GamePresenceTask {
	GamePresenceAction action;
	int appId;
	int requestId;
	IPMI::Session *session;
	GamePresenceFinishCallback onFinish;
	bool completed;
	char customTitleId[16];
};

static SceUInt32 s_presenceOnlineFlag = 0;		/* 0x815CB31C */
static SceUInt32 s_presenceNextReqId = 0;		/* 0x8154920C */
static SceUInt8 s_presenceLockInit = 0;			/* 0x81549210 */
static SceKernelLwMutexWork s_presenceLock;		/* 0x815CB320 */

/* 0x813B9E92 */
static void MarkGamePresenceOnline(void)
{
	s_presenceOnlineFlag = 1;
}

/* 0x813B9EA0 */
static void MarkGamePresenceOffline(void)
{
	s_presenceOnlineFlag = 0;
}

/* 0x813B9EAE */
static bool IsGamePresenceOnline(void)
{
	return s_presenceOnlineFlag != 0;
}

/* 0x813B9EC0 */
static int DelayMilliseconds(int msec)
{
	return sceKernelDelayThread(1000 * msec);
}

/* 0x813BA7E0 */
static int InitGamePresenceLock(void)
{
	if (s_presenceLockInit != 0)
		return 0;

	int ret = SceNpCommonPs4_4858EFB6(&s_presenceLock, "SceNpGamePresenceLock", 1);
	if (ret >= 0)
		s_presenceLockInit = 1;
	return ret;
}

/* 0x813BA26C: Read WebAPI error response body and translate server error code */
static int ReadWebApiErrorResponse(SceInt64 requestId, int httpStatusCode, bool mapConflictToRetry)
{
	unsigned char *bodyBuf = NULL;
	SceSize totalSize = 0;
	char chunk[513];
	int result = 0;

	for (;;) {
		sce_paf_memset(chunk, 0, sizeof(chunk));
		int readBytes = sceNpWebApiReadData(requestId, chunk, 512);
		if (readBytes < 0) {
			result = readBytes;
			break;
		}
		if (readBytes > 0) {
			SceSize nextTotal = totalSize + static_cast<SceSize>(readBytes);
			unsigned char *grown = static_cast<unsigned char *>(
				SceNpCommonPs4_4A93F656(bodyBuf, nextTotal));
			if (grown == NULL) {
				result = NP_GAME_PRESENCE_ERR_OUT_OF_MEMORY;
				break;
			}
			bodyBuf = grown;
			sce_paf_memcpy(bodyBuf + totalSize, chunk, static_cast<SceSize>(readBytes));
			totalSize = nextTotal;
			if (nextTotal < 0x800U)
				continue;
		}
		int errCode = sceNpWebApiGetErrorCode(httpStatusCode, bodyBuf, totalSize);
		if (mapConflictToRetry && errCode == NP_GAME_PRESENCE_ERR_WEBAPI_CONFLICT)
			errCode = NP_GAME_PRESENCE_ERR_RETRY_NEEDED;
		result = errCode;
		break;
	}

	if (bodyBuf != NULL)
		SceNpCommonPs4_6106C031(bodyBuf);
	return result;
}

/* 0x813BA336: PUT version 1 users/{onlineId}/presence/gameTitleInfo */
static int PutGamePresenceTitleInfo(int appId, const char *overrideTitleId)
{
	if (IsGamePresenceOnline())
		return NP_GAME_PRESENCE_ERR_ALREADY_ONLINE;

	SceNpId npId;
	sceClibMemset(&npId, 0, sizeof(npId));
	int ret = npAccountGetCachedNpId(&npId);
	if (ret < 0)
		return ret;

	char onlineId[20];
	sce_paf_memset(onlineId, 0, sizeof(onlineId));
	sce_paf_memcpy(onlineId, npId.handle.data, 16);

	NpImpsAppTitleInfo appInfo;
	sce_paf_memset(&appInfo, 0, sizeof(appInfo));
	ret = npImpsSvcGetAppTitleInfo(appId, &appInfo);
	if (ret < 0)
		return ret;
	if (appInfo.valid == 0)
		return NP_GAME_PRESENCE_ERR_NO_TITLE;

	char npTitleId[16];
	char titleName[132];
	sce_paf_memset(npTitleId, 0, sizeof(npTitleId));
	sce_paf_memset(titleName, 0, sizeof(titleName));
	sce_paf_snprintf(npTitleId, 13, "%s" "_00", appInfo.titleId);
	sce_paf_strncpy(titleName, appInfo.titleName, 129);

	if (overrideTitleId != NULL) {
		sceClibMemcpy_safe(npTitleId, overrideTitleId, 13);
	} else {
		SceSize ignoredLen = sceClibStrnlen("NPXS10999", (SceSize)-1);
		if (sceClibStrncmp("NPXS10999", npTitleId, ignoredLen) == 0)
			return NP_GAME_PRESENCE_ERR_IGNORED_TITLE;
	}

	int retriesRemaining = 1;
	int status = 0;
	for (;;) {
		int libCtxId = 0;
		SceInt64 requestId = 0;
		int httpStatus = 0;

		status = shellNpGetWebApiLibCtx(&libCtxId);
		if (status >= 0) {
			char apiPath[128];
			char escapedName[512];
			char jsonBody[516];
			sce_paf_memset(apiPath, 0, sizeof(apiPath));
			sce_paf_snprintf(apiPath, sizeof(apiPath),
					 "/%s/users/%s/presence/gameTitleInfo", "v" "1", onlineId);

			sce_paf_memset(escapedName, 0, sizeof(escapedName));
			SceSize nameLen = sceClibStrnlen(titleName, (SceSize)-1);
			status = SceNpCommonPs4_9AD6061C(escapedName, sizeof(escapedName),
							 titleName, nameLen);
			if (status >= 0) {
				sce_paf_memset(jsonBody, 0, sizeof(jsonBody));
				sce_paf_snprintf(jsonBody, 512,
						 "{\"npTitleId\":\"%s\",\"titleName\":\"%s\"}",
						 npTitleId, escapedName);

				GamePresenceWebApiContentParam contentParam;
				sce_paf_memset(&contentParam, 0, sizeof(contentParam));
				contentParam.contentLength = sce_paf_strlen(jsonBody);
				contentParam.contentType = "application/json; charset=utf-8";

				status = SceNpWebApi_0D910413(libCtxId, "userProfile", apiPath,
							      2, &contentParam, 0, &requestId);
				if (status >= 0) {
					status = sceNpWebApiSendRequest(
						requestId, jsonBody, sce_paf_strlen(jsonBody));
					if (status >= 0) {
						status = sceNpWebApiGetHttpStatusCode(
							requestId, &httpStatus);
						if (status >= 0) {
							if (httpStatus < 200 || httpStatus >= 300) {
								status = ReadWebApiErrorResponse(
									requestId, httpStatus, true);
							} else {
								status = 0;
							}
						}
					}
				}
			}
		}

		if (requestId > 0)
			sceNpWebApiDeleteRequest(requestId);

		if (status != NP_GAME_PRESENCE_ERR_RETRY_NEEDED)
			break;

		DelayMilliseconds(100);
		if (retriesRemaining-- == 0) {
			status = NP_GAME_PRESENCE_ERR_RETRY_EXHAUSTED;
			break;
		}
	}

	if (status == NP_GAME_PRESENCE_ERR_RETRY_EXHAUSTED) {
		DelayMilliseconds(1000);
		int clearRet = npImpsSvcClearGameTitle();
		if (clearRet >= 0) {
			DelayMilliseconds(1000);
			clearRet = npImpsSvcSetGameTitle(appId);
		}
		return (clearRet >= 0) ? 0 : clearRet;
	}

	if (status >= 0)
		MarkGamePresenceOnline();
	return status;
}

/* 0x813BA686: DELETE version 1 users/{onlineId}/presence/gameTitleInfo */
static int DeleteGamePresenceTitleInfo(void)
{
	SceNpId npId;
	sceClibMemset(&npId, 0, sizeof(npId));
	int status = npAccountGetCachedNpId(&npId);
	if (status >= 0) {
		char onlineId[20];
		sce_paf_memset(onlineId, 0, sizeof(onlineId));
		sce_paf_memcpy(onlineId, npId.handle.data, 16);

		int libCtxId = 0;
		SceInt64 requestId = 0;
		int httpStatus = 0;
		status = shellNpGetWebApiLibCtx(&libCtxId);
		if (status >= 0) {
			char apiPath[128];
			sce_paf_memset(apiPath, 0, sizeof(apiPath));
			sce_paf_snprintf(apiPath, sizeof(apiPath),
					 "/%s/users/%s/presence/gameTitleInfo", "v" "1", onlineId);
			status = SceNpWebApi_0D910413(libCtxId, "userProfile", apiPath,
						      3, NULL, 0, &requestId);
			if (status >= 0) {
				status = sceNpWebApiSendRequest(requestId, NULL, 0);
				if (status >= 0) {
					status = sceNpWebApiGetHttpStatusCode(requestId,
									      &httpStatus);
					if (status >= 0) {
						if (httpStatus < 200 || httpStatus >= 300) {
							status = ReadWebApiErrorResponse(
								requestId, httpStatus, false);
						} else {
							status = 0;
						}
					}
				}
			}
		}
		if (requestId > 0)
			sceNpWebApiDeleteRequest(requestId);
	}

	MarkGamePresenceOffline();
	return status;
}

/* 0x813BA110..0x813BA254: Execute queued GamePresenceTask */
static int ExecuteGamePresenceTask(GamePresenceTask *task)
{
	int ret = 0;
	switch (task->action) {
	case GAME_PRESENCE_ACTION_SET_ONLINE: {
		char emptyTitle[16];
		sceClibMemset(emptyTitle, 0, sizeof(emptyTitle));
		if (sceClibMemcmp(task->customTitleId, emptyTitle, sizeof(emptyTitle)) == 0)
			ret = PutGamePresenceTitleInfo(task->appId, NULL);
		else
			ret = PutGamePresenceTitleInfo(task->appId, task->customTitleId);
		task->completed = true;
		break;
	}
	case GAME_PRESENCE_ACTION_SET_EXIT:
		ret = DeleteGamePresenceTitleInfo();
		task->completed = true;
		break;
	case GAME_PRESENCE_ACTION_SIGNED_OUT:
		MarkGamePresenceOffline();
		task->completed = true;
		ret = 0;
		break;
	default:
		return NP_GAME_PRESENCE_ERR_INVALID_ACTION;
	}
	return ret;
}

class NpGamePresenceJobItem : public paf::job::JobItem {
public:
	explicit NpGamePresenceJobItem(GamePresenceTask *task)
		: paf::job::JobItem("SceNpGamePresenceJob", NULL), m_task(task)
	{
	}
	virtual ~NpGamePresenceJobItem()
	{
		if (m_task != NULL) {
			SceNpCommonPs4_BB5282C8(m_task);
			m_task = NULL;
		}
	}
	virtual int Run(void)
	{
		if (m_task != NULL)
			return ExecuteGamePresenceTask(m_task);
		return 0;
	}
	virtual void Finish(int result)
	{
		(void)result;
		if (m_task != NULL && m_task->session != NULL &&
		    m_task->requestId != 0 && m_task->onFinish != NULL) {
			m_task->onFinish(m_task->session, m_task->requestId);
		}
	}

private:
	GamePresenceTask *m_task;
};

/* 0x813B9FD0 */
static int EnqueueGamePresenceTask(GamePresenceTask *task)
{
	NpIpc2Allocator *alloc = SceNpCommonPs4_53484B79();
	void *rawJob = SceNpCommonPs4_0C922423(sizeof(NpGamePresenceJobItem), alloc);
	if (rawJob == NULL) {
		SceNpCommonPs4_BB5282C8(task);
		return NP_GAME_PRESENCE_ERR_ALLOC_FAILED;
	}

	NpGamePresenceJobItem *jobItem = new (rawJob) NpGamePresenceJobItem(task);
	paf::common::SharedPtr<paf::job::JobItem> jobPtr(jobItem);
	paf::job::JobQueue *queue = shellGetNpBackgroundJobQueue();
	return queue->Enqueue(jobPtr);
}

static int PostGamePresenceAction(GamePresenceAction action, int appId, int requestId,
				  IPMI::Session *session, GamePresenceFinishCallback onFinish,
				  const char *customTitleId)
{
	NpIpc2Allocator *alloc = SceNpCommonPs4_53484B79();
	GamePresenceTask *task = static_cast<GamePresenceTask *>(
		SceNpCommonPs4_0C922423(sizeof(GamePresenceTask), alloc));
	if (task == NULL)
		return NP_GAME_PRESENCE_ERR_OUT_OF_MEMORY;

	sceClibMemset(task, 0, sizeof(*task));
	task->action = action;
	task->appId = appId;
	task->requestId = requestId;
	task->session = session;
	task->onFinish = onFinish;
	task->completed = false;
	if (customTitleId != NULL)
		sceClibMemcpy_safe(task->customTitleId, customTitleId, sizeof(task->customTitleId));

	return EnqueueGamePresenceTask(task);
}

/* 0x813B9DF8 */
static int OnGamePresenceIpcFinished(IPMI::Session *session, int requestId)
{
	return npIpc2ServerFinishRequest(session, 1, requestId);
}

/* ============================================================================
 * Public Entry Points & NpIpc2 Handlers (0x813B9D04..0x813B9E8F)
 * ============================================================================ */

/* 0x813B9D04 */
int npGamePresenceInit(void)
{
	int ret = InitGamePresenceLock();
	if (ret >= 0) {
		s_presenceOnlineFlag = 0;
		return 0;
	}
	return ret;
}

/* 0x813B9D0C: Declared in np_imps_util_mpstate.cpp */
int npPushNotifyAppOnline(int appId)
{
	return PostGamePresenceAction(GAME_PRESENCE_ACTION_SET_ONLINE, appId, 0, NULL, NULL, NULL);
}

/* 0x813B9D1A: Declared in np_imps_util_mpstate.cpp */
void npPushNotifyAppExit(void)
{
	PostGamePresenceAction(GAME_PRESENCE_ACTION_SET_EXIT, 0, 0, NULL, NULL, NULL);
}

/* 0x813B9D22: Declared in np_imps_util_mpstate.cpp */
int npPushNotifySignedOut(void)
{
	return PostGamePresenceAction(GAME_PRESENCE_ACTION_SIGNED_OUT, 0, 0, NULL, NULL, NULL);
}

/* 0x813B9DBC */
int npGamePresenceIpcCreateRequest(IPMI::Session *session, const void *optionBuf,
				   SceSize optionSize, int *outRequestId)
{
	(void)session;
	(void)optionBuf;
	(void)optionSize;

	SceUInt32 nextId = ++s_presenceNextReqId;
	if (nextId >= 0xEFFFFFFFU) {
		nextId = 1;
		s_presenceNextReqId = 1;
	}
	if (outRequestId != NULL)
		*outRequestId = static_cast<int>(nextId);
	return static_cast<int>(nextId);
}

int npGamePresenceIpcDestroyRequest(IPMI::Session *session, int requestId)
{
	(void)session;
	(void)requestId;
	return 0;
}

/* 0x813B9DF8 */
int npGamePresenceIpcAbortRequest(IPMI::Session *session, int requestId)
{
	return OnGamePresenceIpcFinished(session, requestId);
}

/* 0x813B9D2C / 0x813B9E04 */
int npGamePresenceIpcOnSyncMethod(IPMI::Session *session, SceUInt32 methodId,
				  const IPMI::DataInfo *inList, SceUInt32 inCount,
				  IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	switch (methodId) {
	case NP_GAME_PRESENCE_IPC_SET_ONLINE: {
		int requestId = *static_cast<const int *>(inList[0].pData);
		int ret = PostGamePresenceAction(GAME_PRESENCE_ACTION_SET_ONLINE,
						 session->getAppId(), requestId, session,
						 OnGamePresenceIpcFinished, NULL);
		if (ret < 0)
			npIpc2ServerFinishRequest(session, 1, requestId);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case NP_GAME_PRESENCE_IPC_SET_TITLE_ID: {
		int requestId = *static_cast<const int *>(inList[0].pData);
		const char *customTitleId = static_cast<const char *>(inList[1].pData);
		int ret = PostGamePresenceAction(GAME_PRESENCE_ACTION_SET_ONLINE,
						 session->getAppId(), requestId, session,
						 OnGamePresenceIpcFinished, customTitleId);
		if (ret < 0)
			npIpc2ServerFinishRequest(session, 1, requestId);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case NP_GAME_PRESENCE_IPC_SET_TITLE_ASYNC: {
		const char *customTitleId = static_cast<const char *>(inList[0].pData);
		int ret = PostGamePresenceAction(GAME_PRESENCE_ACTION_SET_ONLINE,
						 session->getAppId(), 0, NULL, NULL, customTitleId);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	default:
		return NP_GAME_PRESENCE_IPMI_UNSUPPORTED;
	}
}

/* 0x813B9D40 */
int npGamePresenceIpcOnAsyncMethod(IPMI::Session *session, SceUInt32 requestId,
				   SceUInt32 methodId, const IPMI::DataInfo *inList,
				   SceUInt32 inCount)
{
	(void)session;
	(void)requestId;
	(void)methodId;
	(void)inList;
	(void)inCount;
	return ipmiAsyncNotSupported();
}
