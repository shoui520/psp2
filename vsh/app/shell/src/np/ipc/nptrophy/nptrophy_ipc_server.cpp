/*
 * nptrophy_ipc_server.cpp: SceNpTrophyServer BlockingIpc lifecycle, job queue
 * manager, sync/async dispatchers, response senders, and sync handlers
 * (0x81226C10..0x81227F94, 0x8122845C, 0x81228492, 0x812286FE).
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <blocking_ipc/blocking_ipc_server.h>
#include <ipmi/session.h>
#include "nptrophy_internal.h"

#define NP_TROPHY_MEMBLOCK_TYPE		0x0C20D060U
#define NP_TROPHY_JOB_THREAD_ATTR	0x10000100U
#define NP_TROPHY_JOB_STACK_SIZE	0x4000U

#define NP_TROPHY_INIT_HEAP		0x01U
#define NP_TROPHY_INIT_CORE		0x02U
#define NP_TROPHY_INIT_IPC		0x04U

/* Synchronous request parameter structs */
struct NpTrophyCreateContextIn {
	int titleIdMode;
	int hasCommSign;
	SceUInt64 options;
};

struct NpTrophyCreateTitleSessionIn {
	int sessionFlags;
	int limit;
	int reserved;
	int alignPad;
	SceUInt64 options;
};

struct NpTrophyUpdateContextDataIn {
	int ctxId;
	SceSize dataSize;
};

/* Asynchronous request parameter structs */
struct NpTrophyHandleCtxIn {
	int handleId;
	int ctxId;
};

struct NpTrophyHandleCtxOptIn {
	int handleId;
	int ctxId;
	SceUInt64 options;
};

struct NpTrophyHandleCtxArgIn {
	int handleId;
	int ctxId;
	int itemId;
};

struct NpTrophyHandleCtxSubArgIn {
	int handleId;
	int ctxId;
	int titleHandle;
	int targetId;
};

struct NpTrophyGetGameInfoIn {
	int handleId;
	int ctxId;
	int mode;
	int alignPad;
	SceUInt64 options;
};

struct NpTrophyGetSubInfoIn {
	int handleId;
	int ctxId;
	int mode;
	int targetId;
	SceUInt64 options;
};

struct NpTrophyOpenStreamIn {
	int handleId;
	int ctxId;
	int mode;
	int alignPad;
	SceUInt64 options;
	int wantSize;
};

struct NpTrophyOpenSubStreamIn {
	int handleId;
	int ctxId;
	int mode;
	int targetId;
	SceUInt64 options;
	int wantSize;
};

struct NpTrophyUnlockIn {
	int handleId;
	int ctxId;
	int trophyId;
	int flags;
};

/* Global SceNpTrophyServer state (0x81545524..0x8154552C, 0x815AA40C) */
static BlockingIpc::Server *s_trophyIpcServer = NULL;
static SceUInt32 s_trophyInitFlags = 0;
static SceUID s_trophyMemBlockId = -1;
static NpTrophyQueue *s_trophyJobQueue = NULL;

/* Forward declarations */
static int npTrophyQueueCreate(void);
static void npTrophyQueueDestroy(void);
static int npTrophyOnCreateRequest(IPMI::Session *session, const void *arg, SceSize argSize);
static int npTrophyOnDestroyRequest(IPMI::Session *session, int handleId);
static int npTrophyOnAbortRequest(IPMI::Session *session, int handleId);
static void npTrophyOnSessionCreated(IPMI::Session *session);
static void npTrophyOnSessionClosed(IPMI::Session *session);
static void npTrophyOnSessionKilled(IPMI::Session *session);
static int npTrophySyncDispatch(IPMI::Session *session, SceUInt32 methodId,
				const IPMI::DataInfo *inBufs, SceUInt32 inCount,
				IPMI::BufferInfo *outBufs, SceUInt32 outCount);
static int npTrophyAsyncDispatch(IPMI::Session *session, SceUInt32 ipcReqId,
				 SceUInt32 methodId, const IPMI::DataInfo *inBufs,
				 SceUInt32 inCount);

/* 0x81226C10: start BlockingIpc server "SceNpTrophy" and worker job queue */
static int npTrophyIpcStart(void)
{
	if (s_trophyIpcServer != NULL)
		return NP_TROPHY_ERR_ALREADY_INIT;

	int ret = npTrophyQueueCreate();
	if (ret < 0)
		return ret;

	BlockingIpc::Server::Config cfg;
	sceClibMemset(&cfg, 0, sizeof(cfg));
	cfg.threadPriority = 80;
	cfg.threadStackSize = 0x4000;
	cfg.maxSessions = 16;
	cfg.syncCallCount = 4;
	cfg.maxSyncCallInSize = 4096;
	cfg.maxSyncCallOutSize = 4096;
	cfg.asyncCallCount = 4;
	cfg.maxAsyncCallInSize = 4096;
	cfg.maxAsyncCallOutSize = 4096;
	cfg.blockingIpcCount = 4;
	cfg.maxBlockingInSize = 4096;
	cfg.maxBlockingOutSize = 4096;
	cfg.eventFlagCount = 0;
	cfg.msgQueueCount = 1;
	for (int i = 0; i < 32; ++i) {
		cfg.msgQueue[i].msgSize = 256;
		cfg.msgQueue[i].msgCount = 8;
	}
	cfg.msgQueue[0].msgSize = 256;
	cfg.msgQueue[0].msgCount = 32;
	cfg.slotCount = 32;
	cfg.budget = 2;
	sceClibStrncpy(cfg.name, "SceNpTrophy", sizeof(cfg.name));
	cfg.callbacks.createRequest = npTrophyOnCreateRequest;
	cfg.callbacks.destroyRequest = npTrophyOnDestroyRequest;
	cfg.callbacks.abortRequest = npTrophyOnAbortRequest;
	cfg.callbacks.onSessionCreated = npTrophyOnSessionCreated;
	cfg.callbacks.onSessionClosedByClient = npTrophyOnSessionClosed;
	cfg.callbacks.onSyncMethod = npTrophySyncDispatch;
	cfg.callbacks.onAsyncMethod = npTrophyAsyncDispatch;
	cfg.callbacks.onSessionKilled = npTrophyOnSessionKilled;

	SceSize bufSize = static_cast<SceSize>(BlockingIpc::Server::getBufferSize(&cfg));
	void *serverBuf = npTrophyHeapAlloc(bufSize);
	BlockingIpc::Server *server = NULL;

	if (serverBuf != NULL) {
		ret = BlockingIpc::Server::create(&server, &cfg, serverBuf);
		if (ret != 0) {
			npTrophyHeapFree(serverBuf);
			npTrophyQueueDestroy();
			return ret;
		}
		ret = server->start();
		if (ret >= 0) {
			s_trophyIpcServer = server;
			return 0;
		}
	} else {
		ret = NP_TROPHY_ERR_NO_MEMORY;
	}

	if (server != NULL) {
		server->destroy();
		npTrophyHeapFree(serverBuf);
	}
	npTrophyQueueDestroy();
	return ret;
}

/* 0x81226DD6: initialize SceNpTrophyServer heap, core service, and IPC server */
int npTrophyServerInit(SceSize poolSize)
{
	if (s_trophyInitFlags != 0)
		return NP_TROPHY_ERR_ALREADY_INIT;

	void *baseAddr = NULL;
	int ret = sceKernelAllocMemBlock("SceNpTrophyServer", NP_TROPHY_MEMBLOCK_TYPE,
					 poolSize, NULL);
	if (ret >= 0) {
		SceUID blockId = ret;
		ret = sceKernelGetMemBlockBase(blockId, &baseAddr);
		if (ret >= 0) {
			s_trophyMemBlockId = blockId;
			ret = npTrophyHeapInit(baseAddr, poolSize);
			if (ret >= 0) {
				s_trophyInitFlags |= NP_TROPHY_INIT_HEAP;
				ret = npTrophyCoreInit();
				if (ret >= 0) {
					s_trophyInitFlags |= NP_TROPHY_INIT_CORE;
					ret = npTrophyIpcStart();
					if (ret >= 0) {
						s_trophyInitFlags |= NP_TROPHY_INIT_IPC;
						return 0;
					}
				}
			}
		}
	}

	if ((s_trophyInitFlags & NP_TROPHY_INIT_IPC) != 0 && s_trophyIpcServer != NULL) {
		s_trophyIpcServer->stop();
		if (s_trophyIpcServer->destroy() < 0) {
			s_trophyIpcServer->start();
		} else {
			npTrophyHeapFree(s_trophyIpcServer);
			s_trophyIpcServer = NULL;
			npTrophyQueueDestroy();
		}
	}
	if ((s_trophyInitFlags & NP_TROPHY_INIT_CORE) != 0)
		npTrophyCoreTerm();
	if ((s_trophyInitFlags & NP_TROPHY_INIT_HEAP) != 0)
		npTrophyHeapTerm();
	if (s_trophyMemBlockId != -1) {
		sceKernelFreeMemBlock(s_trophyMemBlockId);
		s_trophyMemBlockId = -1;
	}
	s_trophyInitFlags = 0;
	return ret;
}

/* 0x81226F54 */
int npTrophyServerSuspend(SceUID pid)
{
	int ret = npTrophyCoreSuspend(pid);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81226F64 */
int npTrophyServerResume(SceUID pid)
{
	int ret = npTrophyCoreResume(pid);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81226F74: BlockingIpc createRequest */
static int npTrophyOnCreateRequest(IPMI::Session *session, const void *arg, SceSize argSize)
{
	(void)argSize;
	int handleId = 0;
	int handleType = *static_cast<const int *>(arg);
	int ret = npTrophyCoreCreateHandle(session->getProcessId(), session, handleType, &handleId);
	return (ret >= 0) ? handleId : ret;
}

/* 0x81226FB2: BlockingIpc destroyRequest */
static int npTrophyOnDestroyRequest(IPMI::Session *session, int handleId)
{
	int ret = npTrophyCoreDestroyHandle(session->getProcessId(), session, handleId);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81226FD0: BlockingIpc abortRequest */
static int npTrophyOnAbortRequest(IPMI::Session *session, int handleId)
{
	int ret = npTrophyCoreAbortHandle(session->getProcessId(), session, handleId);
	if (ret >= 0) {
		npTrophyQueueCancelByHandle(handleId);
		return 0;
	}
	return ret;
}

/* 0x81226FF4: BlockingIpc onSessionCreated */
static void npTrophyOnSessionCreated(IPMI::Session *session)
{
	npTrophyCoreOnClientConnect(session->getProcessId(), session);
}

/* 0x81227006: BlockingIpc onSessionClosedByClient */
static void npTrophyOnSessionClosed(IPMI::Session *session)
{
	npTrophyCoreOnClientDisconnect(session->getProcessId(), session,
				       NP_TROPHY_ERR_CLIENT_CLOSED);
}

/* 0x81227020: BlockingIpc onSessionKilled */
static void npTrophyOnSessionKilled(IPMI::Session *session)
{
	npTrophyCoreOnClientDisconnect(session->getProcessId(), session,
				       NP_TROPHY_ERR_CLIENT_KILLED);
}

/* SceNpTrophyJobBase methods (0x8122703C..0x812270B8) */
SceNpTrophyJobBase::SceNpTrophyJobBase(const char *name, IPMI::Session *session,
				       SceUInt32 ipcReqId, int handleId)
	: NpTrophyRuntimeJob(name),
	  m_session(session),
	  m_ipcReqId(ipcReqId),
	  m_clientPid(session->getProcessId()),
	  m_handleId(handleId)
{
}

SceNpTrophyJobBase::~SceNpTrophyJobBase()
{
}

int SceNpTrophyJobBase::Cancel(int cancelError)
{
	npTrophyCoreAbortHandle(m_clientPid, m_session, m_handleId);
	return NpTrophyRuntimeJob::Cancel(cancelError);
}

void *SceNpTrophyJobBase::operator new(size_t size)
{
	return npTrophyHeapAllocObj(size, npTrophyHeapGetContext());
}

void SceNpTrophyJobBase::operator delete(void *ptr)
{
	npTrophyHeapFreeObj(ptr);
}

/* Trophy job queue lifecycle (0x812270D2..0x812271C4) */
static int npTrophyQueueCreate(void)
{
	if (s_trophyJobQueue != NULL)
		return NP_TROPHY_ERR_ALREADY_INIT;

	void *mem = npTrophyHeapAllocObj(88, npTrophyHeapGetContext());
	NpTrophyQueue *queue = (mem != NULL) ? npTrophyQueueConstruct(mem) : NULL;
	int ret = NP_TROPHY_ERR_NO_MEMORY;

	if (queue != NULL) {
		ret = npTrophyQueueInit(queue, "SceNpTrophyServerJob",
					NP_TROPHY_JOB_THREAD_ATTR, NP_TROPHY_JOB_STACK_SIZE);
		if (ret >= 0) {
			ret = npTrophyQueueStart(queue);
			if (ret >= 0) {
				s_trophyJobQueue = queue;
				return 0;
			}
		}
		npTrophyHeapFreeObj(queue);
	}
	return ret;
}

static void npTrophyQueueDestroy(void)
{
	if (s_trophyJobQueue != NULL) {
		npTrophyQueueCancelAll(s_trophyJobQueue, NP_TROPHY_ERR_SERVER_STOPPED);
		npTrophyQueueTerm(s_trophyJobQueue);
		npTrophyHeapFreeObj(s_trophyJobQueue);
		s_trophyJobQueue = NULL;
	}
}

int npTrophyQueueSubmitJob(SceNpTrophyJobBase *job)
{
	if (s_trophyJobQueue == NULL)
		return NP_TROPHY_ERR_NOT_INIT;
	int ret = npTrophyQueueEnqueue(s_trophyJobQueue, 0, job);
	return (ret >= 0) ? 0 : ret;
}

int npTrophyQueueCancelByHandle(int handleId)
{
	if (s_trophyJobQueue == NULL)
		return NP_TROPHY_ERR_NOT_INIT;
	int ret = npTrophyQueueCancelHandle(s_trophyJobQueue, handleId, NP_TROPHY_ERR_ABORTED);
	return (ret >= 0) ? 0 : ret;
}

/* Async response senders (0x812271F4..0x812279DC) */
int npTrophySendRemoveAllTitlesResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result)
{
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_REMOVE_ALL_TITLES_RES,
						    ipcReqId, result, NULL, 0);
}

int npTrophySendCheckRecoveryResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				      int result, const int *outRequired)
{
	IPMI::DataInfo out = { outRequired, sizeof(int) };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_CHECK_RECOVERY_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendPerformRecoveryResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result)
{
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_PERFORM_RECOVERY_RES,
						    ipcReqId, result, NULL, 0);
}

int npTrophySendGetUserFileStatusResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					  int result, const void *outStatus)
{
	IPMI::DataInfo out = { outStatus, 16 };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_USER_FILE_STATUS_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendGetUserTitleInfoResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					 int result, const void *outInfo)
{
	IPMI::DataInfo out = { outInfo, 32 };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_USER_TITLE_INFO_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendGetNextTitleStatusResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					   int result, const int *outNextCursor,
					   const void *outEntry)
{
	IPMI::DataInfo out[2] = {
		{ outNextCursor, sizeof(int) },
		{ outEntry, 104 }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_NEXT_TITLE_STATUS_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendGetTitleForRemovalResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					   int result, const void *outInfo,
					   const void *outExtra,
					   const NpTrophyGameDetails *outDetails)
{
	IPMI::DataInfo out[3] = {
		{ outInfo, 104 },
		{ outExtra, 8 },
		{ outDetails, sizeof(NpTrophyGameDetails) }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_TITLE_FOR_REMOVAL_RES,
						    ipcReqId, result, out, 3);
}

int npTrophySendCheckNetSyncResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				     int result, const int *outNeedSync)
{
	IPMI::DataInfo out = { outNeedSync, sizeof(int) };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_CHECK_NET_SYNC_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendNetSyncTitlesResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result)
{
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_NET_SYNC_TITLES_RES,
						    ipcReqId, result, NULL, 0);
}

int npTrophySendGetTitleStatusForGameResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					      int result, const int *outStatus)
{
	IPMI::DataInfo out = { outStatus, sizeof(int) };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_TITLE_STATUS_GAME_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendInstallTitleResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				     int result, const int *outResult)
{
	IPMI::DataInfo out = { outResult, sizeof(int) };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_INSTALL_TITLE_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendUpdateTitleResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				    int result, const int *outResult)
{
	IPMI::DataInfo out = { outResult, sizeof(int) };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_UPDATE_TITLE_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendSetupTitleResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				   int result, const int *outResult)
{
	IPMI::DataInfo out = { outResult, sizeof(int) };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_SETUP_TITLE_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendRemoveTitleResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result)
{
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_REMOVE_TITLE_RES,
						    ipcReqId, result, NULL, 0);
}

int npTrophySendGetTitleFileStatusResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					   int result, const void *outStatus)
{
	IPMI::DataInfo out = { outStatus, 104 };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_TITLE_FILE_STATUS_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendGetTitleSyncStatusResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					   int result, const void *outStatus)
{
	IPMI::DataInfo out = { outStatus, 8 };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_TITLE_SYNC_STATUS_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendGetUnlockedFlagArrayResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					     int result, const NpTrophyFlagArray *outFlags,
					     const SceUInt32 *outCount)
{
	IPMI::DataInfo out[2] = {
		{ outFlags, sizeof(NpTrophyFlagArray) },
		{ outCount, sizeof(SceUInt32) }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_UNLOCKED_FLAG_ARR_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendGetTrophyTitleDataResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					   int result, const void *outInfo, const void *outStats)
{
	IPMI::DataInfo out[2] = {
		{ outInfo, 108 },
		{ outStats, 136 }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_TROPHY_TITLE_DATA_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendGetTrophyGroupDataResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					   int result, const void *outInfo, const void *outStats)
{
	IPMI::DataInfo out[2] = {
		{ outInfo, 68 },
		{ outStats, 56 }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_TROPHY_GROUP_DATA_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendGetTrophyDataResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				      int result, const void *outInfo, const void *outStats)
{
	IPMI::DataInfo out[2] = {
		{ outInfo, 36 },
		{ outStats, 40 }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_TROPHY_DATA_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendGetGameInfoResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				    int result, const NpTrophyGameDetails *outDetails,
				    const NpTrophyGameData *outData)
{
	IPMI::DataInfo out[2] = {
		{ outDetails, sizeof(NpTrophyGameDetails) },
		{ outData, sizeof(NpTrophyGameData) }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_GAME_INFO_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendGetGroupInfoResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				     int result, const NpTrophyGroupDetails *outDetails,
				     const NpTrophyGroupData *outData)
{
	IPMI::DataInfo out[2] = {
		{ outDetails, sizeof(NpTrophyGroupDetails) },
		{ outData, sizeof(NpTrophyGroupData) }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_GROUP_INFO_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendGetTrophyInfoResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				      int result, const NpTrophyDetails *outDetails,
				      const NpTrophyData *outData)
{
	IPMI::DataInfo out[2] = {
		{ outDetails, sizeof(NpTrophyDetails) },
		{ outData, sizeof(NpTrophyData) }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_GET_TROPHY_INFO_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendDebugUnlockTrophyResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					  int result, const int *outPlatinumId)
{
	IPMI::DataInfo out = { outPlatinumId, sizeof(int) };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_DEBUG_UNLOCK_TROPHY_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendDebugLockTrophyResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					int result, const int *outPlatinumId)
{
	IPMI::DataInfo out = { outPlatinumId, sizeof(int) };
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_DEBUG_LOCK_TROPHY_RES,
						    ipcReqId, result, &out, 1);
}

int npTrophySendUnlockTrophyResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				     int result, const int *outPlatinumId, const int *outFlag)
{
	IPMI::DataInfo out[2] = {
		{ outPlatinumId, sizeof(int) },
		{ outFlag, sizeof(int) }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_UNLOCK_TROPHY_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendOpenConfStreamResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				       int result, const int *outStreamId,
				       const SceUInt64 *outSize)
{
	IPMI::DataInfo out[2] = {
		{ outStreamId, sizeof(int) },
		{ outSize, sizeof(SceUInt64) }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_OPEN_TROPHY_CONF_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendOpenTitleIconStreamResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					    int result, const int *outStreamId,
					    const SceUInt64 *outSize)
{
	IPMI::DataInfo out[2] = {
		{ outStreamId, sizeof(int) },
		{ outSize, sizeof(SceUInt64) }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_OPEN_TITLE_ICON_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendOpenGroupIconStreamResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					    int result, const int *outStreamId,
					    const SceUInt64 *outSize)
{
	IPMI::DataInfo out[2] = {
		{ outStreamId, sizeof(int) },
		{ outSize, sizeof(SceUInt64) }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_OPEN_GROUP_ICON_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendOpenTrophyIconStreamResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					     int result, const int *outStreamId,
					     const SceUInt64 *outSize)
{
	IPMI::DataInfo out[2] = {
		{ outStreamId, sizeof(int) },
		{ outSize, sizeof(SceUInt64) }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_OPEN_TROPHY_ICON_RES,
						    ipcReqId, result, out, 2);
}

int npTrophySendReadStreamResponse(IPMI::Session *session, SceUInt32 ipcReqId,
				   int result, const SceSize *outReadSize, const void *outBuf)
{
	IPMI::DataInfo out[2] = {
		{ outReadSize, sizeof(SceSize) },
		{ outBuf, *outReadSize }
	};
	return session->respondToAsyncMethodRequest(NP_TROPHY_CMD_READ_STREAM_RES,
						    ipcReqId, result, out, 2);
}

/* Synchronous IPC handlers (0x81227EB0..0x81227F94, 0x8122845C, 0x81228492, 0x812286FE) */
static int npTrophyHandleCreateContext(IPMI::Session *session, int *outCtxId, int titleIdMode,
				       const void *commId, int hasCommSign, const void *commSign,
				       SceUInt64 options)
{
	return npTrophyCoreCreateContext(session->getProcessId(), session, outCtxId, titleIdMode,
					 commId, hasCommSign != 0 ? commSign : NULL, options);
}

static int npTrophyHandleDestroyContext(IPMI::Session *session, int ctxId)
{
	return npTrophyCoreDestroyContext(session->getProcessId(), session, ctxId);
}

static int npTrophyHandleRegisterContextData(IPMI::Session *session, const void *commId,
					     const void *buf, SceSize bufSize)
{
	return npTrophyCoreRegisterContextData(session->getProcessId(), session, commId,
					       buf, bufSize);
}

static int npTrophyHandleReadContextData(IPMI::Session *session, const void *commId,
					 void *outBuf, SceSize bufSize)
{
	return npTrophyCoreReadContextData(session->getProcessId(), session, commId,
					   outBuf, bufSize);
}

static int npTrophyHandleUpdateContextData(IPMI::Session *session, int ctxId,
					   const void *buf, SceSize bufSize)
{
	return npTrophyCoreUpdateContextData(session->getProcessId(), session, ctxId,
					     buf, bufSize);
}

static int npTrophyHandleGetProgress(IPMI::Session *session, int handleId, int *outProgress)
{
	int ret = npTrophyCoreGetHandleProgress(session->getProcessId(), session, handleId,
						outProgress);
	return (ret >= 0) ? 0 : ret;
}

static int npTrophyHandleCreateTitleSession(IPMI::Session *session, int sessionFlags, int limit,
					    int reserved, SceUInt64 options, int *outTitleSession)
{
	return npTrophyCoreCreateTitleSession(session->getProcessId(), session, sessionFlags,
					      limit, reserved, options, outTitleSession);
}

static int npTrophyHandleDestroyTitleSession(IPMI::Session *session, int titleSession)
{
	return npTrophyCoreDestroyTitleSession(session->getProcessId(), session, titleSession);
}

static int npTrophyHandleCloseStream(IPMI::Session *session, int streamId)
{
	return npTrophyCoreCloseStream(session->getProcessId(), session, streamId);
}

/* 0x81227A26: synchronous method dispatcher */
static int npTrophySyncDispatch(IPMI::Session *session, SceUInt32 methodId,
				const IPMI::DataInfo *inBufs, SceUInt32 inCount,
				IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	(void)inCount;
	int ret;

	if (methodId == NP_TROPHY_CMD_CLOSE_STREAM) {
		ret = npTrophyHandleCloseStream(session,
						*static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	switch (methodId) {
	case NP_TROPHY_CMD_CREATE_CONTEXT: {
		const NpTrophyCreateContextIn *in =
			static_cast<const NpTrophyCreateContextIn *>(inBufs[0].pData);
		ret = npTrophyHandleCreateContext(session,
						  static_cast<int *>(outBufs[0].pBuffer),
						  in->titleIdMode, inBufs[1].pData,
						  in->hasCommSign, inBufs[2].pData, in->options);
		outBufs[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	case NP_TROPHY_CMD_DESTROY_CONTEXT:
		ret = npTrophyHandleDestroyContext(session,
						   *static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_TROPHY_CMD_CREATE_TITLE_SESSION: {
		const NpTrophyCreateTitleSessionIn *in =
			static_cast<const NpTrophyCreateTitleSessionIn *>(inBufs[0].pData);
		ret = npTrophyHandleCreateTitleSession(session, in->sessionFlags, in->limit,
						       in->reserved, in->options,
						       static_cast<int *>(outBufs[0].pBuffer));
		outBufs[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	case NP_TROPHY_CMD_DESTROY_TITLE_SESSION:
		ret = npTrophyHandleDestroyTitleSession(
			session, *static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_TROPHY_CMD_GET_HANDLE_PROGRESS:
		ret = npTrophyHandleGetProgress(session,
						*static_cast<const int *>(inBufs[0].pData),
						static_cast<int *>(outBufs[0].pBuffer));
		outBufs[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_TROPHY_CMD_REGISTER_CONTEXT_DATA:
		ret = npTrophyHandleRegisterContextData(
			session, inBufs[1].pData, inBufs[2].pData,
			*static_cast<const SceSize *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_TROPHY_CMD_READ_CONTEXT_DATA: {
		SceSize readSize = *static_cast<const SceSize *>(inBufs[0].pData);
		ret = npTrophyHandleReadContextData(session, inBufs[1].pData,
						    outBufs[0].pBuffer, readSize);
		outBufs[0].outSize = readSize;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	case NP_TROPHY_CMD_UPDATE_CONTEXT_DATA: {
		const NpTrophyUpdateContextDataIn *in =
			static_cast<const NpTrophyUpdateContextDataIn *>(inBufs[0].pData);
		ret = npTrophyHandleUpdateContextData(session, in->ctxId,
						      inBufs[1].pData, in->dataSize);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	default:
		return NP_TROPHY_ERR_UNSUPPORTED_METHOD;
	}
}

/* 0x81227BAA: asynchronous method dispatcher */
static int npTrophyAsyncDispatch(IPMI::Session *session, SceUInt32 ipcReqId,
				 SceUInt32 methodId, const IPMI::DataInfo *inBufs,
				 SceUInt32 inCount)
{
	(void)inCount;

	switch (methodId) {
	case NP_TROPHY_CMD_REMOVE_ALL_TITLES: {
		const NpTrophyHandleCtxIn *in =
			static_cast<const NpTrophyHandleCtxIn *>(inBufs[0].pData);
		return npTrophyEnqueueRemoveAllTitlesJob(session, ipcReqId,
							 in->handleId, in->ctxId);
	}

	case NP_TROPHY_CMD_CHECK_RECOVERY: {
		const NpTrophyHandleCtxIn *in =
			static_cast<const NpTrophyHandleCtxIn *>(inBufs[0].pData);
		return npTrophyEnqueueCheckRecoveryJob(session, ipcReqId,
						       in->handleId, in->ctxId);
	}

	case NP_TROPHY_CMD_PERFORM_RECOVERY: {
		const NpTrophyHandleCtxIn *in =
			static_cast<const NpTrophyHandleCtxIn *>(inBufs[0].pData);
		return npTrophyEnqueuePerformRecoveryJob(session, ipcReqId,
							 in->handleId, in->ctxId);
	}

	case NP_TROPHY_CMD_GET_USER_FILE_STATUS: {
		const NpTrophyHandleCtxIn *in =
			static_cast<const NpTrophyHandleCtxIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetUserFileStatusJob(session, ipcReqId,
							   in->handleId, in->ctxId);
	}

	case NP_TROPHY_CMD_GET_USER_TITLE_INFO: {
		const NpTrophyHandleCtxIn *in =
			static_cast<const NpTrophyHandleCtxIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetUserTitleInfoJob(session, ipcReqId,
							  in->handleId, in->ctxId);
	}

	case NP_TROPHY_CMD_GET_NEXT_TITLE_STATUS: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetNextTitleStatusJob(session, ipcReqId,
							    in->handleId, in->ctxId, in->itemId);
	}

	case NP_TROPHY_CMD_GET_TITLE_FOR_REMOVAL: {
		const NpTrophyHandleCtxIn *in =
			static_cast<const NpTrophyHandleCtxIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetTitleForRemovalJob(session, ipcReqId,
							    in->handleId, in->ctxId);
	}

	case NP_TROPHY_CMD_CHECK_NET_SYNC: {
		const NpTrophyHandleCtxOptIn *in =
			static_cast<const NpTrophyHandleCtxOptIn *>(inBufs[0].pData);
		return npTrophyEnqueueCheckNetSyncJob(session, ipcReqId,
						      in->handleId, in->ctxId, in->options);
	}

	case NP_TROPHY_CMD_NET_SYNC_TITLES: {
		const NpTrophyHandleCtxOptIn *in =
			static_cast<const NpTrophyHandleCtxOptIn *>(inBufs[0].pData);
		return npTrophyEnqueueNetSyncTitlesJob(session, ipcReqId,
						       in->handleId, in->ctxId, in->options);
	}

	case NP_TROPHY_CMD_GET_TITLE_STATUS_GAME: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetTitleStatusForGameJob(session, ipcReqId, in->handleId,
							       in->ctxId, in->itemId);
	}

	case NP_TROPHY_CMD_INSTALL_TITLE: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueInstallTitleJob(session, ipcReqId, in->handleId,
						      in->ctxId, in->itemId);
	}

	case NP_TROPHY_CMD_UPDATE_TITLE: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueUpdateTitleJob(session, ipcReqId, in->handleId,
						     in->ctxId, in->itemId);
	}

	case NP_TROPHY_CMD_SETUP_TITLE: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueSetupTitleJob(session, ipcReqId, in->handleId,
						    in->ctxId, in->itemId);
	}

	case NP_TROPHY_CMD_REMOVE_TITLE: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueRemoveTitleJob(session, ipcReqId, in->handleId,
						     in->ctxId, in->itemId);
	}

	case NP_TROPHY_CMD_GET_TITLE_FILE_STATUS: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetTitleFileStatusJob(session, ipcReqId, in->handleId,
							    in->ctxId, in->itemId);
	}

	case NP_TROPHY_CMD_GET_TITLE_SYNC_STATUS: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetTitleSyncStatusJob(session, ipcReqId, in->handleId,
							    in->ctxId, in->itemId);
	}

	case NP_TROPHY_CMD_GET_UNLOCKED_FLAG_ARRAY: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetUnlockedFlagArrayJob(session, ipcReqId, in->handleId,
							      in->ctxId, in->itemId);
	}

	case NP_TROPHY_CMD_GET_TROPHY_TITLE_DATA: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetTrophyTitleDataJob(session, ipcReqId, in->handleId,
							    in->ctxId, in->itemId);
	}

	case NP_TROPHY_CMD_GET_TROPHY_GROUP_DATA: {
		const NpTrophyHandleCtxSubArgIn *in =
			static_cast<const NpTrophyHandleCtxSubArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetTrophyGroupDataJob(session, ipcReqId, in->handleId,
							    in->ctxId, in->titleHandle,
							    in->targetId);
	}

	case NP_TROPHY_CMD_GET_TROPHY_DATA: {
		const NpTrophyHandleCtxSubArgIn *in =
			static_cast<const NpTrophyHandleCtxSubArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetTrophyDataJob(session, ipcReqId, in->handleId,
						       in->ctxId, in->titleHandle, in->targetId);
	}

	case NP_TROPHY_CMD_GET_GAME_INFO: {
		const NpTrophyGetGameInfoIn *in =
			static_cast<const NpTrophyGetGameInfoIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetGameInfoJob(session, ipcReqId, in->handleId,
						     in->ctxId, in->mode, in->options);
	}

	case NP_TROPHY_CMD_GET_GROUP_INFO: {
		const NpTrophyGetSubInfoIn *in =
			static_cast<const NpTrophyGetSubInfoIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetGroupInfoJob(session, ipcReqId, in->handleId, in->ctxId,
						      in->mode, in->targetId, in->options);
	}

	case NP_TROPHY_CMD_GET_TROPHY_INFO: {
		const NpTrophyGetSubInfoIn *in =
			static_cast<const NpTrophyGetSubInfoIn *>(inBufs[0].pData);
		return npTrophyEnqueueGetTrophyInfoJob(session, ipcReqId, in->handleId, in->ctxId,
						       in->mode, in->targetId, in->options);
	}

	case NP_TROPHY_CMD_DEBUG_UNLOCK_TROPHY: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueDebugUnlockTrophyJob(
			session, ipcReqId, in->handleId, in->ctxId, in->itemId,
			static_cast<const SceUInt64 *>(inBufs[1].pData));
	}

	case NP_TROPHY_CMD_DEBUG_LOCK_TROPHY: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueDebugLockTrophyJob(
			session, ipcReqId, in->handleId, in->ctxId, in->itemId,
			static_cast<const SceUInt64 *>(inBufs[1].pData));
	}

	case NP_TROPHY_CMD_UNLOCK_TROPHY: {
		const NpTrophyUnlockIn *in =
			static_cast<const NpTrophyUnlockIn *>(inBufs[0].pData);
		return npTrophyEnqueueUnlockTrophyJob(session, ipcReqId, in->handleId, in->ctxId,
						      in->trophyId, in->flags,
						      static_cast<const int *>(inBufs[1].pData));
	}

	case NP_TROPHY_CMD_OPEN_TROPHY_CONF_STREAM: {
		const NpTrophyOpenStreamIn *in =
			static_cast<const NpTrophyOpenStreamIn *>(inBufs[0].pData);
		return npTrophyEnqueueOpenConfStreamJob(session, ipcReqId, in->handleId, in->ctxId,
							in->mode, in->options, in->wantSize);
	}

	case NP_TROPHY_CMD_OPEN_TITLE_ICON_STREAM: {
		const NpTrophyOpenStreamIn *in =
			static_cast<const NpTrophyOpenStreamIn *>(inBufs[0].pData);
		return npTrophyEnqueueOpenTitleIconStreamJob(session, ipcReqId, in->handleId,
							     in->ctxId, in->mode, in->options,
							     in->wantSize);
	}

	case NP_TROPHY_CMD_OPEN_GROUP_ICON_STREAM: {
		const NpTrophyOpenSubStreamIn *in =
			static_cast<const NpTrophyOpenSubStreamIn *>(inBufs[0].pData);
		return npTrophyEnqueueOpenGroupIconStreamJob(session, ipcReqId, in->handleId,
							     in->ctxId, in->mode, in->targetId,
							     in->options, in->wantSize);
	}

	case NP_TROPHY_CMD_OPEN_TROPHY_ICON_STREAM: {
		const NpTrophyOpenSubStreamIn *in =
			static_cast<const NpTrophyOpenSubStreamIn *>(inBufs[0].pData);
		return npTrophyEnqueueOpenTrophyIconStreamJob(session, ipcReqId, in->handleId,
							      in->ctxId, in->mode, in->targetId,
							      in->options, in->wantSize);
	}

	case NP_TROPHY_CMD_READ_STREAM: {
		const NpTrophyHandleCtxArgIn *in =
			static_cast<const NpTrophyHandleCtxArgIn *>(inBufs[0].pData);
		return npTrophyEnqueueReadStreamJob(session, ipcReqId, in->handleId, in->ctxId,
						    static_cast<SceSize>(in->itemId));
	}

	default:
		return NP_TROPHY_ERR_UNSUPPORTED_METHOD;
	}
}
