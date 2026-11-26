/*
 * np_tus_ipc_server.cpp: SceNpTus BlockingIpc server, keep-alive HTTPS
 * connection cache, savedata owner validation, and TUS/TSS worker thread
 * (0x8132117C..0x8132384B, 0x8132399C..0x81323B33).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <blocking_ipc/blocking_ipc_server.h>
#include <ipmi/session.h>
#include <np/np_common.h>
#include <np/np_error.h>
#include <np_common/np_common.h>
#include "np_community_ipc.h"

/* TUS IPC create-request parameter (204 bytes = 0xCC) */
struct NpTusCreateRequestParam {
	SceUID processId;				/* +0x00 */
	SceNpId selfNpId;				/* +0x04 (36 bytes) */
	SceNpCommunicationId commId;			/* +0x28 (12 bytes) */
	SceNpCommunicationPassphrase passphrase;	/* +0x34 (128 bytes) */
	NpCommunityTimeoutConfig timeoutConfig;		/* +0xB4 (20 bytes) */
	SceInt32 savedataBindMode;			/* +0xC8 */
};

/* TUS sync IPC headers */
struct NpTusChangeBindModeIn {
	SceInt32 requestId;
	SceInt32 bindMode;
};

struct NpTusAbortByIdIn {
	SceUID unusedPid;
	SceInt32 requestId;
};

struct NpTusWorkOrResultIn {
	SceUID processId;				/* +0x00 */
	SceInt32 requestId;				/* +0x04 */
	SceInt32 actionKind;				/* +0x08 (0 = enqueue, 1 = read result) */
	SceInt32 opCode;				/* +0x0C */
	SceInt32 unusedHeadTag;				/* +0x10 */
	SceSize headInSize;				/* +0x14 */
	SceInt32 unusedMidTag;				/* +0x18 */
	SceSize midInSize;				/* +0x1C */
	SceInt32 unusedTailTag;				/* +0x20 */
	SceSize tailInSize;				/* +0x24 */
	SceSize headOutMaxSize;				/* +0x28 */
	SceSize bodyOutMaxSize;				/* +0x2C */
};

/* TUS worker request payload layouts */
struct NpTusMultiUserPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 (36 bytes) */
	SceUInt32 padBeforeMode;			/* +0x28 */
	SceInt32 userQueryMode;				/* +0x2C */
	SceInt32 sortType;				/* +0x30 */
	SceNpCommunicationId commId;			/* +0x34 (12 bytes) */
	SceInt32 slotId;				/* +0x40 */
	SceInt32 arrayNum;				/* +0x44 */
	SceUInt32 startSerialRank;			/* +0x48 */
};

struct NpTusMultiSlotPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 (36 bytes) */
	SceNpId targetNpId;				/* +0x28 (36 bytes) */
	SceInt32 isVirtualUser;				/* +0x4C */
	SceNpCommunicationId commId;			/* +0x50 (12 bytes) */
	SceUInt32 padBeforeCount;			/* +0x5C */
	SceInt32 slotCount;				/* +0x60 */
	SceInt32 setVarCount;				/* +0x64 */
};

struct NpTusAddAndGetPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 (36 bytes) */
	SceNpId targetNpId;				/* +0x28 (36 bytes) */
	SceInt32 isVirtualUser;				/* +0x4C */
	SceNpCommunicationId commId;			/* +0x50 (12 bytes) */
	SceInt32 slotId;				/* +0x5C */
	SceInt64 inVariable;				/* +0x60 */
	SceNpId lastChangedAuthor;			/* +0x68 (36 bytes) */
	SceUInt32 padBeforeDate;			/* +0x8C */
	SceRtcTick lastChangedDate;			/* +0x90 (8 bytes) */
	SceInt32 hasLastChangedAuthor;			/* +0x98 */
	SceInt32 hasLastChangedDate;			/* +0x9C */
};

struct NpTusTryAndSetPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 (36 bytes) */
	SceNpId targetNpId;				/* +0x28 (36 bytes) */
	SceInt32 isVirtualUser;				/* +0x4C */
	SceNpCommunicationId commId;			/* +0x50 (12 bytes) */
	SceInt32 slotId;				/* +0x5C */
	SceInt32 opeType;				/* +0x60 */
	SceUInt32 padBeforeVar;				/* +0x64 */
	SceInt64 inVariable;				/* +0x68 */
	SceInt64 compareValue;				/* +0x70 */
	SceNpId lastChangedAuthor;			/* +0x78 (36 bytes) */
	SceUInt32 padBeforeDate;			/* +0x9C */
	SceRtcTick lastChangedDate;			/* +0xA0 (8 bytes) */
	SceInt32 hasCompareValue;			/* +0xA8 */
	SceInt32 hasLastChangedAuthor;			/* +0xAC */
	SceInt32 hasLastChangedDate;			/* +0xB0 */
};

struct NpTusSetDataPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 (36 bytes) */
	SceNpId targetNpId;				/* +0x28 (36 bytes) */
	SceInt32 isVirtualUser;				/* +0x4C */
	SceNpCommunicationId commId;			/* +0x50 (12 bytes) */
	SceInt32 slotId;				/* +0x5C */
	SceNpId lastChangedAuthor;			/* +0x60 (36 bytes) */
	SceUInt32 padBeforeDate;			/* +0x84 */
	SceRtcTick lastChangedDate;			/* +0x88 (8 bytes) */
	SceInt32 hasLastChangedAuthor;			/* +0x90 */
	SceInt32 hasLastChangedDate;			/* +0x94 */
	NpTusDataInfo dataInfo;				/* +0x98 (392 bytes) */
	SceSize totalSize;				/* +0x220 */
};

struct NpTusGetDataPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 (36 bytes) */
	SceNpId targetNpId;				/* +0x28 (36 bytes) */
	SceInt32 isVirtualUser;				/* +0x4C */
	SceNpCommunicationId commId;			/* +0x50 (12 bytes) */
	SceInt32 slotId;				/* +0x5C */
	SceSize recvSize;				/* +0x60 */
};

struct NpTssGetDataPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 (36 bytes) */
	SceNpCommunicationId commId;			/* +0x28 (12 bytes) */
	SceInt32 slotId;				/* +0x34 */
	SceSize maxSize;				/* +0x38 */
	SceSize recvSize;				/* +0x3C */
	NpTssIfModifiedSinceParam ifParam;		/* +0x40 (16 bytes) */
	SceInt32 hasIfParam;				/* +0x50 */
	SceUInt32 padBeforeOffset;			/* +0x54 */
	SceOff offset;					/* +0x58 (8 bytes) */
	SceInt32 hasOffset;				/* +0x60 */
	SceUInt32 padBeforeLastByte;			/* +0x64 */
	SceOff lastByte;				/* +0x68 (8 bytes) */
	SceInt32 hasLastByte;				/* +0x70 */
	SceInt32 isSmallStorage;			/* +0x74 */
};

/* Cached TUS server hostname and timestamp (148 bytes = 0x94, 0x815C5600..0x815C5693) */
struct NpTusServerCache {
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];	/* +0x00 */
	SceNpCommunicationId commId;			/* +0x80 */
	SceUInt32 padBeforeTime;			/* +0x8C */
	SceUInt32 cachedTimeLow;			/* +0x90 */
};

/* Global SceNpTus server state (0x81546688..0x81546697, 0x815C55E0..0x815C56B7) */
static BlockingIpc::Server *s_tusIpcServer = NULL;	/* 0x81546688 */
static SceInt32 s_tusWorkerInit = 0;			/* 0x8154668C */
static SceBool s_tusRateLimitEnabled = SCE_FALSE;	/* 0x81546690 */
static SceInt32 s_tusHttpsConn = 0;			/* 0x81546694 */
static SceKernelLwMutexWork s_tusKeepAliveMutex;	/* 0x815C55E0 */
static NpTusServerCache s_tusServerCache;		/* 0x815C5600 */
static SceKernelLwMutexWork s_tusWorkQueueMutex;	/* 0x815C5698 */

/* Forward declarations */
static int npTusOnCreateRequest(IPMI::Session *session, const void *param, SceSize paramSize);
static int npTusOnDestroyRequest(IPMI::Session *session, int requestId);
static int npTusOnAbortRequest(IPMI::Session *session, int requestId);
static void npTusOnSessionCreated(IPMI::Session *session);
static void npTusOnSessionClosed(IPMI::Session *session);
static void npTusOnSessionKilled(IPMI::Session *session);
static int npTusSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
			     const IPMI::DataInfo *inList, SceUInt32 inCount,
			     IPMI::BufferInfo *outList, SceUInt32 outCount);
static int npTusAsyncUnsupported(IPMI::Session *session, SceUInt32 requestId,
				 SceUInt32 methodId, const IPMI::DataInfo *inList,
				 SceUInt32 inCount);
static int npTusWorkerThread(SceSize args, void *argp);
static int npTusWorkerInit(SceBool initialRateLimit);

extern "C" void sceNpCommonFreeNpServerName(void *ptr);
extern int ipmiAsyncNotSupported(void);

/* 0x8132117C */
int npTusIpcServerStart(void)
{
	if (s_tusIpcServer != NULL)
		return SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED;

	BlockingIpc::Server::Config cfg;
	sceClibMemset(&cfg, 0, sizeof(cfg));
	cfg.threadPriority = 80;
	cfg.threadStackSize = 0x4000u;
	cfg.maxSessions = 4u;
	cfg.syncCallCount = 1u;
	cfg.maxSyncCallInSize = 55296u;
	cfg.maxSyncCallOutSize = 55296u;
	cfg.asyncCallCount = 0u;
	cfg.maxAsyncCallInSize = 0u;
	cfg.maxAsyncCallOutSize = 0u;
	cfg.blockingIpcCount = 1u;
	cfg.maxBlockingInSize = 55296u;
	cfg.maxBlockingOutSize = 55296u;
	cfg.eventFlagCount = 0u;
	cfg.msgQueueCount = 1u;
	for (int idx = 0; idx < 32; idx++) {
		cfg.msgQueue[idx].msgSize = 256u;
		cfg.msgQueue[idx].msgCount = 8u;
	}
	cfg.msgQueue[0].msgSize = 256u;
	cfg.msgQueue[0].msgCount = 8u;
	cfg.slotCount = 32u;
	cfg.budget = 0;
	sceClibStrlcpy(cfg.name, "SceNpTus", sizeof(cfg.name));
	cfg.callbacks.createRequest = npTusOnCreateRequest;
	cfg.callbacks.destroyRequest = npTusOnDestroyRequest;
	cfg.callbacks.abortRequest = npTusOnAbortRequest;
	cfg.callbacks.onSessionCreated = npTusOnSessionCreated;
	cfg.callbacks.onSessionClosedByClient = npTusOnSessionClosed;
	cfg.callbacks.onSyncMethod = npTusSyncDispatch;
	cfg.callbacks.onAsyncMethod = npTusAsyncUnsupported;
	cfg.callbacks.onSessionKilled = npTusOnSessionKilled;

	SceSize bufSize = BlockingIpc::Server::getBufferSize(&cfg);
	void *serverBuf = npCommunityAlloc(bufSize);
	if (serverBuf == NULL)
		return SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;

	int ret = BlockingIpc::Server::create(&s_tusIpcServer, &cfg, serverBuf);
	if (ret != 0) {
		sceNpCommonFreeNpServerName(serverBuf);
		return ret;
	}

	s_tusIpcServer->start();
	npTusWorkerInit(SCE_FALSE);
	return 0;
}

static int npTusAsyncUnsupported(IPMI::Session *session, SceUInt32 requestId,
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

/* 0x8132131E */
static int npTusOnCreateRequest(IPMI::Session *session, const void *param, SceSize paramSize)
{
	(void)session;
	if (paramSize != sizeof(NpTusCreateRequestParam))
		return NP_BLOCKING_IPC_ERR_INVALID_SIZE;

	const NpTusCreateRequestParam *reqParam =
		static_cast<const NpTusCreateRequestParam *>(param);
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(reqParam->processId, NP_COMMUNITY_SERVICE_TUS, &client);
	if (ret >= 0) {
		ret = npCommunityCreateRequest(client, NP_COMMUNITY_TX_TUS,
					       reqParam->savedataBindMode, &reqParam->selfNpId,
					       &reqParam->commId, &reqParam->passphrase,
					       &reqParam->timeoutConfig);
	}
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x8132139E */
static int npTusOnDestroyRequest(IPMI::Session *session, int requestId)
{
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(session->getProcessId(), NP_COMMUNITY_SERVICE_TUS,
					  &client);
	if (ret >= 0)
		ret = npCommunityDestroyRequest(client, requestId);
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x813213EE */
static int npTusOnAbortRequest(IPMI::Session *session, int requestId)
{
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(session->getProcessId(), NP_COMMUNITY_SERVICE_TUS,
					  &client);
	if (ret >= 0)
		ret = npCommunityAbortRequest(client, requestId);
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x8132143E */
static void npTusOnSessionCreated(IPMI::Session *session)
{
	char threadName[32];
	sceClibMemset(threadName, 0, sizeof(threadName));
	SceUID pid = session->getProcessId();
	sceClibSnprintf(threadName, sizeof(threadName) - 1, "%s_%08x",
			"SceNpTusIpcServerWork", pid);

	NpCommunityWorkerThreadConfig cfg;
	sceClibMemset(&cfg, 0, sizeof(cfg));
	cfg.threadName = threadName;
	cfg.entry = npTusWorkerThread;
	cfg.initPriority = 96;
	cfg.stackSize = 0x8000u;

	NpCommunityClientContext *client = NULL;
	if (npCommunityCreateClientSession(NP_COMMUNITY_SERVICE_TUS, pid, &cfg, &client) >= 0)
		sceKernelStartThread(client->workerThreadId, sizeof(client), &client);
	if (client != NULL)
		npCommunityReleaseClient(client);
}

/* 0x813214EA */
static void npTusOnSessionClosed(IPMI::Session *session)
{
	npCommunityCloseClientSession(session->getProcessId(), NP_COMMUNITY_SERVICE_TUS);
}

/* 0x813214FA */
static int npTusCloseClientByPid(IPMI::Session *session, const SceUID *processIdPtr)
{
	(void)session;
	npCommunityCloseClientSession(*processIdPtr, NP_COMMUNITY_SERVICE_TUS);
	return 0;
}

/* 0x81321508 */
static int npTusNotifyTitleUsed(IPMI::Session *session, const SceNpCommunicationId *commId)
{
	(void)session;
	char commIdText[16];
	sceClibMemset(commIdText, 0, 13u);
	if (SceNpCommon_E426FEE8(commId, commIdText, 13u) >= 0)
		SceNpManager_9554802B("NpTus/Tss", commIdText, NULL);
	return 0;
}

/* 0x81321558 */
static int npTusAbortRequestById(IPMI::Session *session, SceUID unusedPid, SceInt32 requestId)
{
	(void)session;
	(void)unusedPid;
	return npCommunityAbortRequest(NULL, requestId);
}

/* 0x81321564: shared by both Score (0x10010003) and TUS (0x10020003) */
int npCommunitySetTimeout(void *session, SceInt32 requestId,
			  const NpCommunityTimeoutConfig *timeoutCfg)
{
	(void)session;
	NpCommunityTimeoutConfig localCfg = *timeoutCfg;
	return npCommunitySetTxTimeout(requestId, &localCfg);
}

/* 0x813215A2 */
static int npTusChangeBindMode(IPMI::Session *session, SceInt32 requestId, SceInt32 bindMode)
{
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(session->getProcessId(), NP_COMMUNITY_SERVICE_TUS,
					  &client);
	if (ret >= 0)
		ret = npCommunityChangeBindMode(client, requestId, bindMode);
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x813215F6 */
static int npTusDispatchWorkOrResult(IPMI::Session *session, SceUID processId, SceInt32 requestId,
				     SceInt32 actionKind, SceInt32 opCode, SceInt32 unusedHeadTag,
				     SceSize headInSize, const void *headInBuf,
				     SceInt32 unusedMidTag, SceSize midInSize,
				     const void *midInBuf, SceInt32 unusedTailTag,
				     SceSize tailInSize, const void *tailInBuf,
				     SceSize headOutMaxSize, SceSize *outHeadSize,
				     void *headOutBuf, SceSize bodyOutMaxSize,
				     SceSize *outBodySize, void *bodyOutBuf)
{
	(void)session;
	(void)unusedHeadTag;
	(void)unusedMidTag;
	(void)unusedTailTag;

	*outHeadSize = 0;
	*outBodySize = 0;

	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(processId, NP_COMMUNITY_SERVICE_TUS, &client);
	if (ret >= 0) {
		NpCommunityWorkPayloads payloads;
		sceClibMemset(&payloads, 0, sizeof(payloads));
		if (headInSize != 0) {
			payloads.slices[0].buf = const_cast<void *>(headInBuf);
			payloads.slices[0].size = headInSize;
		}
		if (midInSize != 0) {
			payloads.slices[1].buf = const_cast<void *>(midInBuf);
			payloads.slices[1].size = midInSize;
		}
		if (tailInSize != 0) {
			payloads.slices[2].buf = const_cast<void *>(tailInBuf);
			payloads.slices[2].size = tailInSize;
		}

		if (actionKind == 0) {
			npCommunityEnqueueWork(opCode, client, requestId, &payloads);
			*outHeadSize = 0;
			*outBodySize = 0;
			ret = 0;
		} else if (actionKind == 1) {
			ret = npCommunityReadResult(client, requestId, headOutMaxSize, outHeadSize,
						    headOutBuf, 0u, bodyOutMaxSize, outBodySize,
						    bodyOutBuf, 0u, 1);
			if (ret < 0)
				*outHeadSize = 0;
		} else {
			ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
			*outHeadSize = 0;
		}
	}

	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x8132172C */
static void npTusOnSessionKilled(IPMI::Session *session)
{
	npCommunityCloseClientSession(session->getProcessId(), NP_COMMUNITY_SERVICE_TUS);
}

/* 0x81321740 */
static int npTusResolveServerHost(char *outHost, NpCommunityRequestNode *req,
				  const SceNpCommunicationId *commId, const SceNpId *selfNpId)
{
	int ret = npCommunityCheckNotAborted(req);
	if (ret < 0)
		return ret;

	ret = sceKernelLockLwMutex(&s_tusKeepAliveMutex, 1, NULL);
	if (ret < 0)
		return ret;

	SceUInt32 nowLow = sceKernelGetProcessTimeLow();
	SceUInt32 expireLow = s_tusServerCache.cachedTimeLow +
			      static_cast<SceUInt32>(NP_COMMUNITY_SERVER_CACHE_TTL_USEC);
	int lookupTxId = 0;
	bool cacheHit = false;

	if (sceClibMemcmp(&s_tusServerCache.commId, commId, sizeof(SceNpCommunicationId)) != 0) {
		if (s_tusHttpsConn != 0) {
			sceHttpDeleteConnection(s_tusHttpsConn);
			s_tusHttpsConn = 0;
		}
	} else if (nowLow < expireLow) {
		sceClibMemcpy_safe(outHost, s_tusServerCache.hostName, NP_COMMUNITY_HOST_NAME_LEN);
		cacheHit = true;
	}

	if (!cacheHit) {
		ret = npCommunityCreateTx(selfNpId, NP_COMMUNITY_TX_TUS_SERVER_INFO, commId);
		if (ret >= 0) {
			lookupTxId = ret;
			req->serverLookupTxId = lookupTxId;
			ret = npCommunitySetTxTimeout(lookupTxId, &req->timeoutConfig);
			if (ret >= 0)
				ret = npCommunityCheckNotAborted(req);
			if (ret >= 0) {
				NpCommunityServerNode *serverList = NULL;
				ret = npTusBackendGetServerList(lookupTxId, &serverList);
				req->serverLookupTxId = 0;
				if (ret >= 0) {
					sceClibStrncpy(s_tusServerCache.hostName,
						       serverList->hostName,
						       NP_COMMUNITY_HOST_NAME_LEN - 1);
					sceClibMemcpy_safe(outHost, s_tusServerCache.hostName,
							   NP_COMMUNITY_HOST_NAME_LEN);
					s_tusServerCache.cachedTimeLow = nowLow;
					s_tusServerCache.commId = *commId;
					ret = 0;
				} else if (ret == SCE_NP_COMMUNITY_SERVER_ERROR_NO_SUCH_TITLE) {
					npCommunityLogServiceNotConfigured("NpTus", commId);
				}
			}
		}
	}

	sceKernelUnlockLwMutex(&s_tusKeepAliveMutex, 1);
	if (lookupTxId != 0)
		npCommunityDestroyTx(lookupTxId);
	return ret;
}

/* 0x8132196A */
static int npTusAcquireKeepAliveConn(SceInt32 txId, const char *hostName, SceInt32 *outConn)
{
	SceInt32 conn = 0;
	int ret = sceKernelLockLwMutex(&s_tusKeepAliveMutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (outConn != NULL) {
		if (s_tusHttpsConn != 0) {
			if (sceClibStrncasecmp(s_tusServerCache.hostName, hostName,
					       NP_COMMUNITY_HOST_NAME_LEN) == 0) {
				conn = s_tusHttpsConn;
				s_tusHttpsConn = 0;
			} else {
				sceHttpDeleteConnection(s_tusHttpsConn);
				s_tusHttpsConn = 0;
			}
		}
		if (conn == 0) {
			int tmplId = npCommunityGetTxHttpTemplate(txId);
			ret = tmplId;
			if (tmplId >= 0) {
				ret = sceHttpCreateConnection(tmplId, hostName, "https", 443u,
							      SCE_TRUE);
				if (ret >= 0)
					conn = ret;
			}
		}
	}

	sceKernelUnlockLwMutex(&s_tusKeepAliveMutex, 1);

	if (ret >= 0) {
		if (outConn != NULL)
			*outConn = conn;
	} else if (conn != 0) {
		sceHttpDeleteConnection(conn);
	}
	return ret;
}

static void npTusReturnKeepAliveConn(const char *hostName, SceInt32 conn)
{
	if (conn == 0)
		return;
	if (sceKernelLockLwMutex(&s_tusKeepAliveMutex, 1, NULL) >= 0) {
		if (sceClibStrncasecmp(s_tusServerCache.hostName, hostName,
				       NP_COMMUNITY_HOST_NAME_LEN) != 0) {
			sceHttpDeleteConnection(conn);
		} else {
			if (s_tusHttpsConn != 0)
				sceHttpDeleteConnection(s_tusHttpsConn);
			s_tusHttpsConn = conn;
		}
		sceKernelUnlockLwMutex(&s_tusKeepAliveMutex, 1);
	}
}

static void npTusCloseKeepAliveConn(void)
{
	if (sceKernelLockLwMutex(&s_tusKeepAliveMutex, 1, NULL) >= 0) {
		if (s_tusHttpsConn != 0) {
			sceHttpDeleteConnection(s_tusHttpsConn);
			s_tusHttpsConn = 0;
		}
		sceKernelUnlockLwMutex(&s_tusKeepAliveMutex, 1);
	}
}

/* 0x81321A44 */
static int npTusHandleGetMultiUserVariable(NpCommunityClientContext *client, SceInt32 requestId,
					   const NpCommunityWorkPayloads *payloads)
{
	const NpTusMultiUserPayload *in =
		static_cast<const NpTusMultiUserPayload *>(payloads->slices[0].buf);
	const SceNpId *targetList =
		static_cast<const SceNpId *>(payloads->slices[1].buf);

	NpCommunityRequestNode *req = NULL;
	SceInt32 httpsConn = 0;
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];
	NpCommunityResultPacket *pkt = NULL;
	NpTusVariable *varsBuf = NULL;
	SceUInt32 *hitsBuf = NULL;
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;
	bool completed = false;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npTusResolveServerHost(hostName, req, &in->commId, &in->selfNpId);
	if (ret >= 0) {
		npTusAcquireKeepAliveConn(requestId, hostName, &httpsConn);
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			SceSize varsSize = sizeof(NpTusVariable) *
					   static_cast<SceSize>(in->arrayNum);
			varsBuf = static_cast<NpTusVariable *>(npCommunityAlloc(varsSize));
			if (varsBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(varsBuf, 0, varsSize);
				pkt->headBuf = varsBuf;
				pkt->headSize = varsSize;

				hitsBuf = static_cast<SceUInt32 *>(
					npCommunityAlloc(sizeof(SceUInt32)));
				if (hitsBuf == NULL) {
					ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
				} else {
					*hitsBuf = 0;
					pkt->bodyBuf = hitsBuf;
					pkt->bodySize = sizeof(SceUInt32);

					ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
					if (ret >= 0) {
						NpTusRequestEnv env;
						sceClibMemset(&env, 0, sizeof(env));
						env.commId = &in->commId;
						env.hostName = hostName;
						env.selfNpId = &in->selfNpId;
						env.ticketBuf = ticketBuf;
						env.ticketSize = ticketSize;
						env.passphrase = &req->passphrase;
						env.clientPid = client->processId;
						env.httpConnId = httpsConn;

						if (in->userQueryMode == 2 ||
						    in->userQueryMode == 3) {
							ret = npTusBackendGetFriendsVariable(
								in->txId, &env, in->slotId,
								in->userQueryMode == 3 ? 1 : 0,
								in->startSerialRank, in->sortType,
								varsBuf, in->arrayNum, hitsBuf,
								NULL);
						} else {
							ret = npTusBackendGetMultiUserVariable(
								in->txId, &env, targetList,
								in->userQueryMode, in->slotId,
								varsBuf, in->arrayNum, NULL);
						}
						if (ret >= 0) {
							int opResult = ret;
							ret = npCommunityTrackRequestAlloc(
								req, hitsBuf);
							if (ret >= 0) {
								hitsBuf = NULL;
								ret = npCommunityTrackRequestAlloc(
									req, varsBuf);
								if (ret >= 0) {
									varsBuf = NULL;
									ret = npCommunityTrackRequestAlloc(
										req, pkt);
									if (ret >= 0) {
										npTusReturnKeepAliveConn(
											hostName,
											httpsConn);
										req->resultPacket =
											pkt;
										req->resultCode =
											opResult;
										req->resultReady =
											1;
										npCommunityFree(
											ticketBuf);
										npCommunityReleaseRequest(
											req);
										return opResult;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (!completed) {
		if (ticketBuf != NULL)
			npCommunityFree(ticketBuf);
		if (pkt != NULL)
			npCommunityFree(pkt);
		if (varsBuf != NULL)
			npCommunityFree(varsBuf);
		if (hitsBuf != NULL)
			npCommunityFree(hitsBuf);
		if (req != NULL) {
			req->resultCode = ret;
			npCommunityReleaseRequest(req);
		}
		npTusReturnKeepAliveConn(hostName, httpsConn);
	}
	return ret;
}

/* 0x81321D2A */
static int npTusHandleGetMultiUserDataStatus(NpCommunityClientContext *client, SceInt32 requestId,
					     const NpCommunityWorkPayloads *payloads)
{
	const NpTusMultiUserPayload *in =
		static_cast<const NpTusMultiUserPayload *>(payloads->slices[0].buf);
	const SceNpId *targetList =
		static_cast<const SceNpId *>(payloads->slices[1].buf);

	NpCommunityRequestNode *req = NULL;
	SceInt32 httpsConn = 0;
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];
	NpCommunityResultPacket *pkt = NULL;
	NpTusDataStatus *statusBuf = NULL;
	SceUInt32 *hitsBuf = NULL;
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npTusResolveServerHost(hostName, req, &in->commId, &in->selfNpId);
	if (ret >= 0) {
		npTusAcquireKeepAliveConn(requestId, hostName, &httpsConn);
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			SceSize statusSize = sizeof(NpTusDataStatus) *
					     static_cast<SceSize>(in->arrayNum);
			statusBuf = static_cast<NpTusDataStatus *>(npCommunityAlloc(statusSize));
			if (statusBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(statusBuf, 0, statusSize);
				pkt->headBuf = statusBuf;
				pkt->headSize = statusSize;

				hitsBuf = static_cast<SceUInt32 *>(
					npCommunityAlloc(sizeof(SceUInt32)));
				if (hitsBuf == NULL) {
					ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
				} else {
					*hitsBuf = 0;
					pkt->bodyBuf = hitsBuf;
					pkt->bodySize = sizeof(SceUInt32);

					ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
					if (ret >= 0) {
						NpTusRequestEnv env;
						sceClibMemset(&env, 0, sizeof(env));
						env.commId = &in->commId;
						env.hostName = hostName;
						env.selfNpId = &in->selfNpId;
						env.ticketBuf = ticketBuf;
						env.ticketSize = ticketSize;
						env.passphrase = &req->passphrase;
						env.clientPid = client->processId;
						env.httpConnId = httpsConn;

						if (in->userQueryMode == 2 ||
						    in->userQueryMode == 3) {
							ret = npTusBackendGetFriendsDataStatus(
								in->txId, &env, in->slotId,
								in->userQueryMode == 3 ? 1 : 0,
								in->startSerialRank, in->sortType,
								statusBuf, in->arrayNum, hitsBuf,
								NULL);
						} else {
							ret = npTusBackendGetMultiUserDataStatus(
								in->txId, &env, targetList,
								in->userQueryMode, in->slotId,
								statusBuf, in->arrayNum, NULL);
						}
						if (ret >= 0) {
							int opResult = ret;
							ret = npCommunityTrackRequestAlloc(
								req, hitsBuf);
							if (ret >= 0) {
								hitsBuf = NULL;
								ret = npCommunityTrackRequestAlloc(
									req, statusBuf);
								if (ret >= 0) {
									statusBuf = NULL;
									ret = npCommunityTrackRequestAlloc(
										req, pkt);
									if (ret >= 0) {
										npTusReturnKeepAliveConn(
											hostName,
											httpsConn);
										req->resultPacket =
											pkt;
										req->resultCode =
											opResult;
										req->resultReady =
											1;
										npCommunityFree(
											ticketBuf);
										npCommunityReleaseRequest(
											req);
										return opResult;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (ticketBuf != NULL)
		npCommunityFree(ticketBuf);
	if (pkt != NULL)
		npCommunityFree(pkt);
	if (statusBuf != NULL)
		npCommunityFree(statusBuf);
	if (hitsBuf != NULL)
		npCommunityFree(hitsBuf);
	if (req != NULL) {
		req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	npTusReturnKeepAliveConn(hostName, httpsConn);
	return ret;
}

/* 0x81322012 */
static int npTusHandleTryAndSetVariable(NpCommunityClientContext *client, SceInt32 requestId,
					const NpCommunityWorkPayloads *payloads)
{
	const NpTusTryAndSetPayload *in =
		static_cast<const NpTusTryAndSetPayload *>(payloads->slices[0].buf);

	NpCommunityRequestNode *req = NULL;
	SceInt32 httpsConn = 0;
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];
	NpCommunityResultPacket *pkt = NULL;
	NpTusVariable *outVar = NULL;
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npTusResolveServerHost(hostName, req, &in->commId, &in->selfNpId);
	if (ret >= 0) {
		npTusAcquireKeepAliveConn(requestId, hostName, &httpsConn);
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			outVar = static_cast<NpTusVariable *>(
				npCommunityAlloc(sizeof(NpTusVariable)));
			if (outVar == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(outVar, 0, sizeof(NpTusVariable));
				pkt->headBuf = outVar;
				pkt->headSize = sizeof(NpTusVariable);

				ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
				if (ret >= 0) {
					NpTusRequestEnv env;
					sceClibMemset(&env, 0, sizeof(env));
					env.commId = &in->commId;
					env.hostName = hostName;
					env.selfNpId = &in->selfNpId;
					env.ticketBuf = ticketBuf;
					env.ticketSize = ticketSize;
					env.passphrase = &req->passphrase;
					env.clientPid = client->processId;
					env.httpConnId = httpsConn;

					const SceInt64 *cmpPtr = (in->hasCompareValue != 0)
									 ? &in->compareValue
									 : NULL;
					const SceNpId *authorPtr =
						(in->hasLastChangedAuthor != 0)
							? &in->lastChangedAuthor
							: NULL;
					const SceRtcTick *datePtr =
						(in->hasLastChangedDate != 0)
							? &in->lastChangedDate
							: NULL;

					ret = npTusBackendTryAndSetVariable(
						in->txId, &env, &in->targetNpId,
						in->isVirtualUser, in->slotId, in->inVariable,
						cmpPtr, authorPtr, datePtr, outVar, in->opeType,
						NULL);
					if (ret >= 0) {
						int opResult = ret;
						ret = npCommunityTrackRequestAlloc(req, outVar);
						if (ret >= 0) {
							outVar = NULL;
							ret = npCommunityTrackRequestAlloc(req,
											   pkt);
							if (ret >= 0) {
								npTusReturnKeepAliveConn(
									hostName, httpsConn);
								req->resultPacket = pkt;
								req->resultCode = opResult;
								req->resultReady = 1;
								npCommunityFree(ticketBuf);
								npCommunityReleaseRequest(req);
								return opResult;
							}
						}
					}
				}
			}
		}
	}

	if (ticketBuf != NULL)
		npCommunityFree(ticketBuf);
	if (pkt != NULL)
		npCommunityFree(pkt);
	if (outVar != NULL)
		npCommunityFree(outVar);
	if (req != NULL) {
		req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	npTusReturnKeepAliveConn(hostName, httpsConn);
	return ret;
}

static int npTusCheckSavedataOwner(NpCommunityClientContext *client, SceInt32 requestId,
				   SceInt32 requiredBit)
{
	NpCommunityRequestNode *req = NULL;
	NpCommunityBackendTx *backendTx = NULL;
	int ret = npCommunityLookupRequest(&req, client, requestId);

	if (ret >= 0) {
		if ((requiredBit & req->savedataBindMode) != 0) {
			ret = 0;
		} else {
			int ownerAttr = 0;
			int fakeAttr = 0;
			ret = sceAppMgrAppParamGetInt(client->processId, 5, &ownerAttr);
			if (ret >= 0) {
				bool isRealOwner = ((ownerAttr & 1) == 0);
				ret = sceAppMgrAppParamGetInt(client->processId, 13, &fakeAttr);
				if (ret >= 0) {
					bool isFakeOwner = ((fakeAttr & 8) != 0);
					ret = npCommunityAcquireBackendTx(req->transactionId,
									  &backendTx);
					if (ret >= 0) {
						SceSize envLen = sceClibStrnlen("np", 0x7FFFFFFFu);
						bool isProdEnv =
							(sceClibMemcmp(backendTx->client->envName,
								       "np", envLen + 1) == 0);
						if (!isRealOwner) {
							SceNpManager_9554802B(
								"NpTus",
								"invalid savedata owner", NULL);
							ret = SCE_NP_COMMUNITY_ERROR_TUS_INVALID_SAVEDATA_OWNER;
						} else if (isFakeOwner && isProdEnv) {
							SceNpManager_9554802B(
								"NpTus",
								"fakeOwner not allowed in product env",
								NULL);
							ret = SCE_NP_COMMUNITY_ERROR_TUS_INVALID_SAVEDATA_OWNER;
						} else {
							ret = 0;
						}
					}
				}
			}
		}
	}

	if (req != NULL) {
		if (ret < 0)
			req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	if (backendTx != NULL)
		npCommunityReleaseBackendTx(backendTx);
	return ret;
}

static int npTusHandleGetMultiSlotVariable(NpCommunityClientContext *client, SceInt32 requestId,
					   const NpTusMultiSlotPayload *in,
					   const SceInt32 *slotIds)
{
	NpCommunityRequestNode *req = NULL;
	SceInt32 httpsConn = 0;
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];
	NpCommunityResultPacket *pkt = NULL;
	NpTusVariable *varsBuf = NULL;
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npTusResolveServerHost(hostName, req, &in->commId, &in->selfNpId);
	if (ret >= 0) {
		npTusAcquireKeepAliveConn(requestId, hostName, &httpsConn);
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			SceSize varsSize = sizeof(NpTusVariable) *
					   static_cast<SceSize>(in->slotCount);
			varsBuf = static_cast<NpTusVariable *>(npCommunityAlloc(varsSize));
			if (varsBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(varsBuf, 0, varsSize);
				pkt->headBuf = varsBuf;
				pkt->headSize = varsSize;

				ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
				if (ret >= 0) {
					NpTusRequestEnv env;
					sceClibMemset(&env, 0, sizeof(env));
					env.commId = &in->commId;
					env.hostName = hostName;
					env.selfNpId = &in->selfNpId;
					env.ticketBuf = ticketBuf;
					env.ticketSize = ticketSize;
					env.passphrase = &req->passphrase;
					env.clientPid = client->processId;
					env.httpConnId = httpsConn;

					ret = npTusBackendGetMultiSlotVariable(
						in->txId, &env, &in->targetNpId,
						in->isVirtualUser, slotIds, varsBuf,
						in->slotCount, NULL);
					if (ret >= 0) {
						int opResult = ret;
						ret = npCommunityTrackRequestAlloc(req, varsBuf);
						if (ret >= 0) {
							varsBuf = NULL;
							ret = npCommunityTrackRequestAlloc(req,
											   pkt);
							if (ret >= 0) {
								npTusReturnKeepAliveConn(
									hostName, httpsConn);
								req->resultPacket = pkt;
								req->resultCode = opResult;
								req->resultReady = 1;
								npCommunityFree(ticketBuf);
								npCommunityReleaseRequest(req);
								return opResult;
							}
						}
					}
				}
			}
		}
	}

	if (ticketBuf != NULL)
		npCommunityFree(ticketBuf);
	if (pkt != NULL)
		npCommunityFree(pkt);
	if (varsBuf != NULL)
		npCommunityFree(varsBuf);
	if (req != NULL) {
		req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	npTusReturnKeepAliveConn(hostName, httpsConn);
	return ret;
}

static int npTusHandleGetMultiSlotDataStatus(NpCommunityClientContext *client, SceInt32 requestId,
					     const NpTusMultiSlotPayload *in,
					     const SceInt32 *slotIds)
{
	NpCommunityRequestNode *req = NULL;
	SceInt32 httpsConn = 0;
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];
	NpCommunityResultPacket *pkt = NULL;
	NpTusDataStatus *statusBuf = NULL;
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npTusResolveServerHost(hostName, req, &in->commId, &in->selfNpId);
	if (ret >= 0) {
		npTusAcquireKeepAliveConn(requestId, hostName, &httpsConn);
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			SceSize statusSize = sizeof(NpTusDataStatus) *
					     static_cast<SceSize>(in->slotCount);
			statusBuf = static_cast<NpTusDataStatus *>(npCommunityAlloc(statusSize));
			if (statusBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(statusBuf, 0, statusSize);
				pkt->headBuf = statusBuf;
				pkt->headSize = statusSize;

				ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
				if (ret >= 0) {
					NpTusRequestEnv env;
					sceClibMemset(&env, 0, sizeof(env));
					env.commId = &in->commId;
					env.hostName = hostName;
					env.selfNpId = &in->selfNpId;
					env.ticketBuf = ticketBuf;
					env.ticketSize = ticketSize;
					env.passphrase = &req->passphrase;
					env.clientPid = client->processId;
					env.httpConnId = httpsConn;

					ret = npTusBackendGetMultiSlotDataStatus(
						in->txId, &env, &in->targetNpId,
						in->isVirtualUser, slotIds, statusBuf,
						in->slotCount, NULL);
					if (ret >= 0) {
						int opResult = ret;
						ret = npCommunityTrackRequestAlloc(req, statusBuf);
						if (ret >= 0) {
							statusBuf = NULL;
							ret = npCommunityTrackRequestAlloc(req,
											   pkt);
							if (ret >= 0) {
								npTusReturnKeepAliveConn(
									hostName, httpsConn);
								req->resultPacket = pkt;
								req->resultCode = opResult;
								req->resultReady = 1;
								npCommunityFree(ticketBuf);
								npCommunityReleaseRequest(req);
								return opResult;
							}
						}
					}
				}
			}
		}
	}

	if (ticketBuf != NULL)
		npCommunityFree(ticketBuf);
	if (pkt != NULL)
		npCommunityFree(pkt);
	if (statusBuf != NULL)
		npCommunityFree(statusBuf);
	if (req != NULL) {
		req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	npTusReturnKeepAliveConn(hostName, httpsConn);
	return ret;
}

static int npTusHandleDeleteMultiSlot(NpCommunityClientContext *client, SceInt32 requestId,
				      const NpTusMultiSlotPayload *in, const SceInt32 *slotIds,
				      bool isData)
{
	NpCommunityRequestNode *req = NULL;
	SceInt32 httpsConn = 0;
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npTusResolveServerHost(hostName, req, &in->commId, &in->selfNpId);
	if (ret >= 0) {
		npTusAcquireKeepAliveConn(requestId, hostName, &httpsConn);
		ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
		if (ret >= 0) {
			NpTusRequestEnv env;
			sceClibMemset(&env, 0, sizeof(env));
			env.commId = &in->commId;
			env.hostName = hostName;
			env.selfNpId = &in->selfNpId;
			env.ticketBuf = ticketBuf;
			env.ticketSize = ticketSize;
			env.passphrase = &req->passphrase;
			env.clientPid = client->processId;
			env.httpConnId = httpsConn;

			if (isData) {
				ret = npTusBackendDeleteMultiSlotData(
					in->txId, &env, &in->targetNpId, in->isVirtualUser,
					slotIds, in->slotCount, NULL);
			} else {
				ret = npTusBackendDeleteMultiSlotVariable(
					in->txId, &env, &in->targetNpId, in->isVirtualUser,
					slotIds, in->slotCount, NULL);
			}
			if (ret >= 0) {
				npTusReturnKeepAliveConn(hostName, httpsConn);
				req->resultCode = ret;
				req->resultReady = 1;
				npCommunityFree(ticketBuf);
				npCommunityReleaseRequest(req);
				return 0;
			}
		}
	}

	if (ticketBuf != NULL)
		npCommunityFree(ticketBuf);
	if (req != NULL) {
		req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	npTusReturnKeepAliveConn(hostName, httpsConn);
	return ret;
}

static int npTusHandleSetMultiSlotVariable(NpCommunityClientContext *client, SceInt32 requestId,
					   const NpTusMultiSlotPayload *in,
					   const SceInt32 *slotIds, const SceInt64 *variables)
{
	NpCommunityRequestNode *req = NULL;
	SceInt32 httpsConn = 0;
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npTusResolveServerHost(hostName, req, &in->commId, &in->selfNpId);
	if (ret >= 0) {
		npTusAcquireKeepAliveConn(requestId, hostName, &httpsConn);
		ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
		if (ret >= 0) {
			NpTusRequestEnv env;
			sceClibMemset(&env, 0, sizeof(env));
			env.commId = &in->commId;
			env.hostName = hostName;
			env.selfNpId = &in->selfNpId;
			env.ticketBuf = ticketBuf;
			env.ticketSize = ticketSize;
			env.passphrase = &req->passphrase;
			env.clientPid = client->processId;
			env.httpConnId = httpsConn;

			ret = npTusBackendSetMultiSlotVariable(
				in->txId, &env, &in->targetNpId, in->isVirtualUser, slotIds,
				variables, in->setVarCount, NULL);
			if (ret >= 0) {
				npTusReturnKeepAliveConn(hostName, httpsConn);
				npCommunityFree(ticketBuf);
				req->resultCode = ret;
				req->resultReady = 1;
				npCommunityReleaseRequest(req);
				return 0;
			}
		}
	}

	if (ticketBuf != NULL)
		npCommunityFree(ticketBuf);
	if (req != NULL) {
		req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	npTusReturnKeepAliveConn(hostName, httpsConn);
	return ret;
}

static int npTusHandleAddAndGetVariable(NpCommunityClientContext *client, SceInt32 requestId,
					const NpTusAddAndGetPayload *in)
{
	NpCommunityRequestNode *req = NULL;
	SceInt32 httpsConn = 0;
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];
	NpCommunityResultPacket *pkt = NULL;
	NpTusVariable *outVar = NULL;
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npTusResolveServerHost(hostName, req, &in->commId, &in->selfNpId);
	if (ret >= 0) {
		npTusAcquireKeepAliveConn(requestId, hostName, &httpsConn);
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			outVar = static_cast<NpTusVariable *>(
				npCommunityAlloc(sizeof(NpTusVariable)));
			if (outVar == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(outVar, 0, sizeof(NpTusVariable));
				pkt->headBuf = outVar;
				pkt->headSize = sizeof(NpTusVariable);

				ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
				if (ret >= 0) {
					NpTusRequestEnv env;
					sceClibMemset(&env, 0, sizeof(env));
					env.commId = &in->commId;
					env.hostName = hostName;
					env.selfNpId = &in->selfNpId;
					env.ticketBuf = ticketBuf;
					env.ticketSize = ticketSize;
					env.passphrase = &req->passphrase;
					env.clientPid = client->processId;
					env.httpConnId = httpsConn;

					const SceNpId *authorPtr =
						(in->hasLastChangedAuthor != 0)
							? &in->lastChangedAuthor
							: NULL;
					const SceRtcTick *datePtr =
						(in->hasLastChangedDate != 0)
							? &in->lastChangedDate
							: NULL;

					ret = npTusBackendAddAndGetVariable(
						in->txId, &env, &in->targetNpId,
						in->isVirtualUser, in->slotId, in->inVariable,
						authorPtr, datePtr, outVar, NULL);
					if (ret >= 0) {
						int opResult = ret;
						ret = npCommunityTrackRequestAlloc(req, outVar);
						if (ret >= 0) {
							outVar = NULL;
							ret = npCommunityTrackRequestAlloc(req,
											   pkt);
							if (ret >= 0) {
								npTusReturnKeepAliveConn(
									hostName, httpsConn);
								req->resultPacket = pkt;
								req->resultCode = opResult;
								req->resultReady = 1;
								npCommunityFree(ticketBuf);
								npCommunityReleaseRequest(req);
								return opResult;
							}
						}
					}
				}
			}
		}
	}

	if (ticketBuf != NULL)
		npCommunityFree(ticketBuf);
	if (pkt != NULL)
		npCommunityFree(pkt);
	if (outVar != NULL)
		npCommunityFree(outVar);
	if (req != NULL) {
		req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	npTusReturnKeepAliveConn(hostName, httpsConn);
	return ret;
}

static int npTusHandleSetData(NpCommunityClientContext *client, SceInt32 requestId,
			      const NpTusSetDataPayload *in, const void *chunkData,
			      SceSize sendSize)
{
	NpCommunityRequestNode *req = NULL;
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npTusResolveServerHost(hostName, req, &in->commId, &in->selfNpId);
	if (ret >= 0)
		ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
	if (ret >= 0) {
		NpTusRequestEnv env;
		sceClibMemset(&env, 0, sizeof(env));
		env.commId = &in->commId;
		env.hostName = hostName;
		env.selfNpId = &in->selfNpId;
		env.ticketBuf = ticketBuf;
		env.ticketSize = ticketSize;
		env.passphrase = &req->passphrase;
		env.clientPid = client->processId;
		env.httpConnId = 0;

		const SceNpId *authorPtr =
			(in->hasLastChangedAuthor != 0) ? &in->lastChangedAuthor : NULL;
		const SceRtcTick *datePtr =
			(in->hasLastChangedDate != 0) ? &in->lastChangedDate : NULL;

		ret = npTusBackendSetData(in->txId, &env, &in->targetNpId, in->isVirtualUser,
					  in->slotId, in->totalSize, chunkData, sendSize,
					  &in->dataInfo, authorPtr, datePtr, NULL);
		if (ret >= 0) {
			req->resultCode = ret;
			req->resultReady = 1;
			npCommunityFree(ticketBuf);
			npCommunityReleaseRequest(req);
			return ret;
		}
	}

	if (ticketBuf != NULL)
		npCommunityFree(ticketBuf);
	if (req != NULL) {
		req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	return ret;
}

static int npTusHandleGetData(NpCommunityClientContext *client, SceInt32 requestId,
			      const NpTusGetDataPayload *in)
{
	NpCommunityRequestNode *req = NULL;
	char hostName[NP_COMMUNITY_HOST_NAME_LEN];
	NpCommunityResultPacket *pkt = NULL;
	NpTusDataStatus *statusBuf = NULL;
	void *bodyBuf = NULL;
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npTusResolveServerHost(hostName, req, &in->commId, &in->selfNpId);
	if (ret >= 0) {
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			statusBuf = static_cast<NpTusDataStatus *>(
				npCommunityAlloc(sizeof(NpTusDataStatus)));
			if (statusBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(statusBuf, 0, sizeof(NpTusDataStatus));
				pkt->headBuf = statusBuf;
				pkt->headSize = sizeof(NpTusDataStatus);

				if (in->recvSize != 0) {
					bodyBuf = npCommunityAlloc(in->recvSize);
					if (bodyBuf == NULL) {
						ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
					} else {
						sceClibMemset(bodyBuf, 0, in->recvSize);
						pkt->bodyBuf = bodyBuf;
						pkt->bodySize = in->recvSize;
					}
				}
				if (ret >= 0)
					ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
				if (ret >= 0) {
					NpTusRequestEnv env;
					sceClibMemset(&env, 0, sizeof(env));
					env.commId = &in->commId;
					env.hostName = hostName;
					env.selfNpId = &in->selfNpId;
					env.ticketBuf = ticketBuf;
					env.ticketSize = ticketSize;
					env.passphrase = &req->passphrase;
					env.clientPid = client->processId;
					env.httpConnId = 0;

					ret = npTusBackendGetData(
						in->txId, &env, &in->targetNpId, in->isVirtualUser,
						in->slotId, statusBuf, bodyBuf, in->recvSize, NULL);
					if (ret >= 0) {
						int opResult = ret;
						if (bodyBuf != NULL)
							ret = npCommunityTrackRequestAlloc(
								req, bodyBuf);
						if (ret >= 0) {
							bodyBuf = NULL;
							ret = npCommunityTrackRequestAlloc(
								req, statusBuf);
							if (ret >= 0) {
								statusBuf = NULL;
								ret = npCommunityTrackRequestAlloc(
									req, pkt);
								if (ret >= 0) {
									pkt->bodySize =
										static_cast<SceSize>(
											opResult);
									req->resultPacket = pkt;
									req->resultCode = opResult;
									req->resultReady = 1;
									npCommunityFree(ticketBuf);
									npCommunityReleaseRequest(
										req);
									return opResult;
								}
							}
						}
					}
				}
			}
		}
	}

	if (ticketBuf != NULL)
		npCommunityFree(ticketBuf);
	if (pkt != NULL)
		npCommunityFree(pkt);
	if (bodyBuf != NULL)
		npCommunityFree(bodyBuf);
	if (statusBuf != NULL)
		npCommunityFree(statusBuf);
	if (req != NULL) {
		req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	return ret;
}

static int npTusHandleTssGetData(NpCommunityClientContext *client, SceInt32 requestId,
				 const NpTssGetDataPayload *in)
{
	NpCommunityRequestNode *req = NULL;
	NpCommunityResultPacket *pkt = NULL;
	NpTssDataStatus *statusBuf = NULL;
	void *bodyBuf = NULL;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0) {
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			statusBuf = static_cast<NpTssDataStatus *>(
				npCommunityAlloc(sizeof(NpTssDataStatus)));
			if (statusBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(statusBuf, 0, sizeof(NpTssDataStatus));
				pkt->headBuf = statusBuf;
				pkt->headSize = sizeof(NpTssDataStatus);

				NpTssGetDataOptParam optParam;
				NpTssGetDataOptParam *optPtr = NULL;
				SceOff localOffset = 0;
				SceOff localLastByte = 0;
				NpTssIfModifiedSinceParam localIfParam;

				if (in->hasOffset != 0 || in->hasLastByte != 0 ||
				    in->hasIfParam != 0) {
					sceClibMemset(&optParam, 0, sizeof(optParam));
					if (in->hasOffset != 0) {
						localOffset = in->offset;
						optParam.offset = &localOffset;
					}
					if (in->hasLastByte != 0) {
						localLastByte = in->lastByte;
						optParam.lastByte = &localLastByte;
					}
					if (in->hasIfParam != 0) {
						localIfParam = in->ifParam;
						optParam.ifParam = &localIfParam;
					}
					optPtr = &optParam;
				}

				if (in->recvSize != 0) {
					bodyBuf = npCommunityAlloc(in->recvSize);
					if (bodyBuf == NULL) {
						ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
					} else {
						sceClibMemset(bodyBuf, 0, in->recvSize);
						pkt->bodyBuf = bodyBuf;
						pkt->bodySize = in->recvSize;
					}
				}

				if (ret >= 0) {
					ret = npTssBackendGetData(
						in->txId, &in->commId, in->slotId, statusBuf,
						bodyBuf, in->maxSize, in->recvSize,
						in->isSmallStorage, optPtr);
					if (ret >= 0) {
						int opResult = ret;
						if (bodyBuf != NULL)
							ret = npCommunityTrackRequestAlloc(
								req, bodyBuf);
						if (ret >= 0) {
							bodyBuf = NULL;
							ret = npCommunityTrackRequestAlloc(
								req, statusBuf);
							if (ret >= 0) {
								statusBuf = NULL;
								ret = npCommunityTrackRequestAlloc(
									req, pkt);
								if (ret >= 0) {
									pkt->bodySize =
										static_cast<SceSize>(
											opResult);
									req->resultCode = opResult;
									req->resultPacket = pkt;
									req->resultReady = 1;
									npCommunityReleaseRequest(
										req);
									return opResult;
								}
							}
						}
					}
				}
			}
		}
	}

	if (pkt != NULL)
		npCommunityFree(pkt);
	if (bodyBuf != NULL)
		npCommunityFree(bodyBuf);
	if (statusBuf != NULL)
		npCommunityFree(statusBuf);
	if (req != NULL) {
		req->resultCode = ret;
		npCommunityReleaseRequest(req);
	}
	return ret;
}

/* 0x813222B8 */
static int npTusWorkerThread(SceSize args, void *argp)
{
	(void)args;
	NpCommunityClientContext *client = *static_cast<NpCommunityClientContext **>(argp);
	s_tusRateLimitEnabled = SceNpCommon_BBC13621();

	bool running = true;
	while (running) {
		int hasKeepAlive = 0;
		if (sceKernelLockLwMutex(&s_tusKeepAliveMutex, 1, NULL) >= 0) {
			if (s_tusHttpsConn != 0)
				hasKeepAlive = 1;
			sceKernelUnlockLwMutex(&s_tusKeepAliveMutex, 1);
		}

		SceUInt32 waitTimeout = NP_COMMUNITY_KEEPALIVE_TIMEOUT_USEC;
		SceUInt32 *timeoutPtr = (hasKeepAlive != 0) ? &waitTimeout : NULL;
		int waitRet = sceKernelWaitSema(client->workSemaId, 1, timeoutPtr);
		if (waitRet < 0) {
			if (waitRet == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
				npTusCloseKeepAliveConn();
				continue;
			}
			break;
		}

		int lockRet = sceKernelLockLwMutex(&s_tusWorkQueueMutex, 1, NULL);
		if (lockRet < 0)
			return lockRet;

		NpCommunityWorkItem work = client->workQueue[client->queueReadIndex];
		client->queueReadIndex++;
		if (client->queueReadIndex == NP_COMMUNITY_WORK_QUEUE_DEPTH)
			client->queueReadIndex = 0;
		sceKernelUnlockLwMutex(&s_tusWorkQueueMutex, 1);

		if (work.opCode == NP_TUS_WORK_SHUTDOWN) {
			running = false;
			break;
		}

		SceInt32 requiredBit = 0;
		switch (work.opCode) {
		case NP_TUS_WORK_GET_MULTI_USER_VARIABLE:
		case NP_TUS_WORK_GET_MULTI_SLOT_VARIABLE:
		case NP_TUS_WORK_GET_MULTI_USER_DATA_STATUS:
		case NP_TUS_WORK_GET_MULTI_SLOT_DATA_STATUS:
		case NP_TUS_WORK_GET_DATA:
			requiredBit = NP_COMMUNITY_BINDMODE_RDONLY;
			break;
		case NP_TUS_WORK_DELETE_MULTI_SLOT_VARIABLE:
		case NP_TUS_WORK_DELETE_MULTI_SLOT_DATA:
		case NP_TUS_WORK_SET_MULTI_SLOT_VARIABLE:
		case NP_TUS_WORK_ADD_AND_GET_VARIABLE:
		case NP_TUS_WORK_TRY_AND_SET_VARIABLE:
		case NP_TUS_WORK_SET_DATA:
			requiredBit = NP_COMMUNITY_BINDMODE_WRONLY;
			break;
		default:
			break;
		}

		int opRet = 0;
		if (requiredBit != 0)
			opRet = npTusCheckSavedataOwner(client, work.requestId, requiredBit);

		if (opRet >= 0) {
			int netToken = 0;
			switch (work.opCode) {
			case NP_TUS_WORK_GET_MULTI_USER_VARIABLE:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleGetMultiUserVariable(client, work.requestId,
									&work.payloads);
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_GET_MULTI_SLOT_VARIABLE:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleGetMultiSlotVariable(
					client, work.requestId,
					static_cast<const NpTusMultiSlotPayload *>(
						work.payloads.slices[0].buf),
					static_cast<const SceInt32 *>(work.payloads.slices[1].buf));
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_GET_MULTI_USER_DATA_STATUS:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleGetMultiUserDataStatus(client, work.requestId,
									  &work.payloads);
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_GET_MULTI_SLOT_DATA_STATUS:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleGetMultiSlotDataStatus(
					client, work.requestId,
					static_cast<const NpTusMultiSlotPayload *>(
						work.payloads.slices[0].buf),
					static_cast<const SceInt32 *>(work.payloads.slices[1].buf));
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_DELETE_MULTI_SLOT_VARIABLE:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleDeleteMultiSlot(
					client, work.requestId,
					static_cast<const NpTusMultiSlotPayload *>(
						work.payloads.slices[0].buf),
					static_cast<const SceInt32 *>(work.payloads.slices[1].buf),
					false);
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_DELETE_MULTI_SLOT_DATA:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleDeleteMultiSlot(
					client, work.requestId,
					static_cast<const NpTusMultiSlotPayload *>(
						work.payloads.slices[0].buf),
					static_cast<const SceInt32 *>(work.payloads.slices[1].buf),
					true);
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_SET_MULTI_SLOT_VARIABLE:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleSetMultiSlotVariable(
					client, work.requestId,
					static_cast<const NpTusMultiSlotPayload *>(
						work.payloads.slices[0].buf),
					static_cast<const SceInt32 *>(work.payloads.slices[1].buf),
					static_cast<const SceInt64 *>(work.payloads.slices[2].buf));
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_ADD_AND_GET_VARIABLE:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleAddAndGetVariable(
					client, work.requestId,
					static_cast<const NpTusAddAndGetPayload *>(
						work.payloads.slices[0].buf));
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_TRY_AND_SET_VARIABLE:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleTryAndSetVariable(client, work.requestId,
								     &work.payloads);
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_SET_DATA:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleSetData(
					client, work.requestId,
					static_cast<const NpTusSetDataPayload *>(
						work.payloads.slices[0].buf),
					work.payloads.slices[1].buf, work.payloads.slices[1].size);
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_GET_DATA:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleGetData(
					client, work.requestId,
					static_cast<const NpTusGetDataPayload *>(
						work.payloads.slices[0].buf));
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			case NP_TUS_WORK_TSS_GET_DATA:
				netToken = SceShell_6B8772A2(client->processId);
				opRet = npTusHandleTssGetData(
					client, work.requestId,
					static_cast<const NpTssGetDataPayload *>(
						work.payloads.slices[0].buf));
				SceShell_794BF5BE(static_cast<unsigned int>(netToken));
				break;
			default:
				opRet = SCE_NP_COMMUNITY_ERROR_INVALID_ID;
				break;
			}
		}

		sceKernelSetEventFlag(client->eventFlagId, 1u);
		for (int idx = 0; idx < NP_COMMUNITY_WORK_PAYLOAD_COUNT; idx++) {
			if (work.payloads.slices[idx].buf != NULL)
				npCommunityFree(work.payloads.slices[idx].buf);
		}

		if (s_tusRateLimitEnabled == SCE_TRUE &&
		    work.opCode != NP_TUS_WORK_GET_DATA &&
		    work.opCode != NP_TUS_WORK_SET_DATA &&
		    work.opCode != 2007 &&
		    work.opCode != NP_TUS_WORK_TSS_GET_DATA &&
		    work.opCode != 2008 &&
		    opRet != SCE_NP_COMMUNITY_ERROR_INVALID_ID &&
		    opRet != SCE_NP_COMMUNITY_ERROR_ABORTED) {
			SceUInt32 delayTimeout = NP_COMMUNITY_RATELIMIT_DELAY_USEC;
			sceKernelLockLwMutex(&client->delayMutex, 1, &delayTimeout);
		}
	}

	npTusCloseKeepAliveConn();
	return 0;
}

/* 0x813237BA */
static int npTusWorkerInit(SceBool initialRateLimit)
{
	if (s_tusWorkerInit == 1)
		return SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED;

	s_tusRateLimitEnabled = initialRateLimit;
	sceClibMemset(&s_tusServerCache, 0, sizeof(s_tusServerCache));
	int ret = sceKernelCreateLwMutex(&s_tusWorkQueueMutex, "SceNpTusWorkQueue", 2u, 0, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelCreateLwMutex(&s_tusKeepAliveMutex, "SceNpTusKeepAlive", 2u, 0, NULL);
	if (ret < 0)
		return ret;
	ret = npCommunityIpcInit(NP_COMMUNITY_SERVICE_TUS);
	if (ret < 0)
		return ret;

	s_tusWorkerInit = 1;
	return 0;
}

/* 0x813239A0 */
static int npTusHandleWorkOrResultMethod(IPMI::Session *session, const IPMI::DataInfo *inList,
					 SceUInt32 inCount, IPMI::BufferInfo *outList,
					 SceUInt32 outCount)
{
	(void)inCount;
	const NpTusWorkOrResultIn *in =
		static_cast<const NpTusWorkOrResultIn *>(inList[0].pData);
	SceSize *outHeadSize = static_cast<SceSize *>(outList[0].pBuffer);
	SceSize *outBodySize = static_cast<SceSize *>(outList[2].pBuffer);

	int ret = npTusDispatchWorkOrResult(
		session, in->processId, in->requestId, in->actionKind, in->opCode,
		in->unusedHeadTag, in->headInSize, inList[1].pData, in->unusedMidTag,
		in->midInSize, inList[2].pData, in->unusedTailTag, in->tailInSize,
		inList[3].pData, in->headOutMaxSize, outHeadSize, outList[1].pBuffer,
		in->bodyOutMaxSize, outBodySize, outList[3].pBuffer);

	outList[0].outSize = sizeof(SceSize);
	outList[1].outSize = *outHeadSize;
	outList[2].outSize = sizeof(SceSize);
	outList[3].outSize = *outBodySize;
	return session->respondToSyncMethodRequest(ret, outList, outCount);
}

/* 0x81323A46 */
static int npTusSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
			     const IPMI::DataInfo *inList, SceUInt32 inCount,
			     IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	switch (methodId) {
	case NP_TUS_IPC_PING:
		return session->respondToSyncMethodRequest(npCommunityNoop(), outList, outCount);

	case NP_TUS_IPC_CLOSE_CLIENT: {
		const SceUID *pidPtr = static_cast<const SceUID *>(inList[0].pData);
		int ret = npTusCloseClientByPid(session, pidPtr);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_TUS_IPC_NOTIFY_TITLE: {
		const SceNpCommunicationId *commId =
			static_cast<const SceNpCommunicationId *>(inList[0].pData);
		int ret = npTusNotifyTitleUsed(session, commId);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_TUS_IPC_SET_TIMEOUT: {
		SceInt32 reqId = *static_cast<const SceInt32 *>(inList[0].pData);
		const NpCommunityTimeoutConfig *cfg =
			static_cast<const NpCommunityTimeoutConfig *>(inList[1].pData);
		int ret = npCommunitySetTimeout(session, reqId, cfg);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_TUS_IPC_CHANGE_BIND_MODE: {
		const NpTusChangeBindModeIn *in =
			static_cast<const NpTusChangeBindModeIn *>(inList[0].pData);
		int ret = npTusChangeBindMode(session, in->requestId, in->bindMode);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_TUS_IPC_ABORT_BY_ID: {
		const NpTusAbortByIdIn *in =
			static_cast<const NpTusAbortByIdIn *>(inList[0].pData);
		int ret = npTusAbortRequestById(session, in->unusedPid, in->requestId);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_TUS_IPC_WORK_OR_RESULT:
		return npTusHandleWorkOrResultMethod(session, inList, inCount, outList, outCount);

	default:
		return NP_IPMI_ERR_UNSUPPORTED_METHOD;
	}
}
