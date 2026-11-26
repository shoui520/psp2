/*
 * np_score_ipc_server.cpp: SceNpScore BlockingIpc server, keep-alive HTTP
 * connection cache, savedata owner validation, and worker thread
 * (0x8131F65C..0x8132117B, 0x8132384C).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include <blocking_ipc/blocking_ipc_server.h>
#include <ipmi/session.h>
#include <np/np_common.h>
#include <np/np_error.h>
#include <np_common/np_common.h>
#include "np_community_ipc.h"

/* Score IPC create-request parameter (200 bytes = 0xC8) */
struct NpScoreCreateRequestParam {
	SceNpId selfNpId;				/* +0x00 (36 bytes) */
	SceNpCommunicationId commId;			/* +0x24 (12 bytes) */
	SceNpCommunicationPassphrase passphrase;	/* +0x30 (128 bytes) */
	NpCommunityTimeoutConfig timeoutConfig;		/* +0xB0 (20 bytes) */
	SceInt32 savedataBindMode;			/* +0xC4 */
};

/* Score sync IPC request headers */
struct NpScoreChangeBindModeIn {
	SceInt32 requestId;
	SceInt32 bindMode;
};

struct NpScoreEnqueueWorkIn {
	SceInt32 requestId;
	SceInt32 opCode;
	SceInt32 reservedOpWord;
	SceSize payloadSize;
};

struct NpScoreGetResultIn {
	SceInt32 requestId;
	SceSize maxReadSize;
};

struct NpScoreGetResultRangeIn {
	SceInt32 requestId;
	SceUInt32 readOffset;
	SceInt32 clearAfterRead;
	SceSize maxReadSize;
};

/* Score worker request payload layouts */
struct NpScoreBoardInfoPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 */
	SceNpCommunicationId commId;			/* +0x28 */
	SceUInt32 boardId;				/* +0x34 */
};

struct NpScoreRecordScorePayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 */
	SceNpCommunicationId commId;			/* +0x28 */
	SceUInt32 boardId;				/* +0x34 */
	SceInt32 pcId;					/* +0x38 */
	SceUInt32 padBeforeScore;			/* +0x3C */
	SceInt64 scoreValue;				/* +0x40 */
	NpScoreComment comment;				/* +0x48 (64 bytes) */
	NpScoreGameInfo gameInfo;			/* +0x88 (200 bytes) */
	SceInt32 hasTmpRank;				/* +0x150 */
	SceUInt32 padBeforeCompareDate;			/* +0x154 */
	SceRtcTick compareDate;				/* +0x158 */
	SceInt32 hasCompareDate;			/* +0x160 */
};

struct NpScoreRecordScoreResult {
	SceInt32 requestId;
	SceUInt32 tmpRank;
};

struct NpScoreRecordGameDataPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 */
	SceNpCommunicationId commId;			/* +0x28 */
	SceUInt32 boardId;				/* +0x34 */
	SceInt32 pcId;					/* +0x38 */
	SceUInt32 padBeforeScore;			/* +0x3C */
	SceInt64 scoreValue;				/* +0x40 */
	SceSize totalSize;				/* +0x48 */
	SceSize sendSize;				/* +0x4C */
	SceUInt8 chunkBytes[1];				/* +0x50 */
};

struct NpScoreGetGameDataPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 */
	SceNpCommunicationId commId;			/* +0x28 */
	SceNpId targetNpId;				/* +0x34 */
	SceUInt32 boardId;				/* +0x58 */
	SceInt32 pcId;					/* +0x5C */
	SceSize recvSize;				/* +0x60 */
};

struct NpScoreGetGameDataResult {
	SceInt32 requestId;				/* +0x000 */
	SceUInt8 chunkBytes[2048];			/* +0x004 */
	SceSize totalSize;				/* +0x804 */
	SceSize recvSize;				/* +0x808 */
};

struct NpScoreGetRankingByNpIdPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 */
	SceNpCommunicationId commId;			/* +0x28 */
	SceUInt32 boardId;				/* +0x34 */
	NpScoreNpIdPcId targets[101];			/* +0x38 (4444 bytes) */
	SceInt32 wantComment;				/* +0x1194 */
	SceInt32 wantGameInfo;				/* +0x1198 */
	SceSize arrayNum;				/* +0x119C */
};

struct NpScoreGetRankingByNpIdResult {
	SceInt32 requestId;				/* +0x0000 */
	SceUInt32 alignPad;				/* +0x0004 */
	NpScorePlayerRankData rankArray[101];		/* +0x0008 (13736 bytes) */
	NpScoreComment commentArray[101];		/* +0x35B0 (6464 bytes) */
	NpScoreGameInfo gameInfoArray[101];		/* +0x4EF0 (20200 bytes) */
	SceUInt32 padBeforeSortDate[2];			/* +0x9DD8 */
	SceRtcTick lastSortDate;			/* +0x9DE0 */
	SceUInt32 totalRecord;				/* +0x9DE8 */
	SceUInt32 padTail;				/* +0x9DEC */
};

struct NpScoreGetRankingByRangePayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 */
	SceNpCommunicationId commId;			/* +0x28 */
	SceUInt32 boardId;				/* +0x34 */
	SceInt32 pcId;					/* +0x38 */
	SceUInt32 startSerialRank;			/* +0x3C */
	SceInt32 wantComment;				/* +0x40 */
	SceInt32 wantGameInfo;				/* +0x44 */
	SceUInt32 unusedRangeWord;			/* +0x48 */
	SceInt32 friendsScope;				/* +0x4C */
	SceSize arrayNum;				/* +0x50 */
};

struct NpScoreGetRankingByRangeResult {
	SceInt32 requestId;				/* +0x0000 */
	SceUInt32 alignPad;				/* +0x0004 */
	NpScoreRankData rankArray[100];			/* +0x0008 (12800 bytes) */
	NpScoreComment commentArray[100];		/* +0x3208 (6400 bytes) */
	NpScoreGameInfo gameInfoArray[100];		/* +0x4B08 (20000 bytes) */
	SceUInt32 padBeforeSortDate[2];			/* +0x9928 */
	SceRtcTick lastSortDate;			/* +0x9930 */
	SceUInt32 totalRecord;				/* +0x9938 */
	SceUInt32 hits;					/* +0x993C */
};

struct NpScoreFilterCommentPayload {
	SceInt32 txId;					/* +0x00 */
	SceNpId selfNpId;				/* +0x04 */
	SceNpCommunicationId commId;			/* +0x28 */
	char comment[256];				/* +0x34 */
	SceInt32 isSanitize;				/* +0x134 */
};

struct NpScoreFilterCommentResult {
	SceInt32 requestId;				/* +0x00 */
	char comment[256];				/* +0x04 */
};

/* Cached server hostnames and timestamp (280 bytes = 0x118, 0x815C54A8..0x815C55BF) */
struct NpScoreServerCache {
	NpScoreServerHosts hosts;			/* +0x000 (256 bytes) */
	SceNpCommunicationId commId;			/* +0x100 (12 bytes) */
	SceUInt32 padBeforeTick;			/* +0x10C */
	SceRtcTick cachedAt;				/* +0x110 (8 bytes) */
};

/* Global SceNpScore server state (0x81546670..0x81546687, 0x815C5488..0x815C55DF) */
static BlockingIpc::Server *s_scoreIpcServer = NULL;	/* 0x81546670 */
static SceInt32 s_scoreWorkerInit = 0;			/* 0x81546674 */
static SceBool s_scoreRateLimitEnabled = SCE_FALSE;	/* 0x81546678 */
static SceInt32 s_scoreRecordHttpsConn = 0;		/* 0x8154667C */
static SceInt32 s_scoreViewHttpConn = 0;		/* 0x81546680 */
static SceInt32 s_scoreViewHttpsConn = 0;		/* 0x81546684 */
static SceKernelLwMutexWork s_scoreKeepAliveMutex;	/* 0x815C5488 */
static NpScoreServerCache s_scoreServerCache;		/* 0x815C54A8 */
static SceKernelLwMutexWork s_scoreWorkQueueMutex;	/* 0x815C55C0 */

/* Forward declarations */
static int npScoreOnCreateRequest(IPMI::Session *session, const void *param, SceSize paramSize);
static int npScoreOnDestroyRequest(IPMI::Session *session, int requestId);
static int npScoreOnAbortRequest(IPMI::Session *session, int requestId);
static void npScoreOnSessionCreated(IPMI::Session *session);
static void npScoreOnSessionClosed(IPMI::Session *session);
static void npScoreOnSessionKilled(IPMI::Session *session);
static int npScoreSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
			       const IPMI::DataInfo *inList, SceUInt32 inCount,
			       IPMI::BufferInfo *outList, SceUInt32 outCount);
static int npScoreAsyncUnsupported(IPMI::Session *session, SceUInt32 requestId,
				   SceUInt32 methodId, const IPMI::DataInfo *inList,
				   SceUInt32 inCount);
static int npScoreWorkerThread(SceSize args, void *argp);
static int npScoreWorkerInit(void);

extern "C" void sceNpCommonFreeNpServerName(void *ptr);
extern int ipmiAsyncNotSupported(void);

/* 0x8131F65C */
int npScoreIpcServerStart(void)
{
	if (s_scoreIpcServer != NULL)
		return SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED;

	BlockingIpc::Server::Config cfg;
	sceClibMemset(&cfg, 0, sizeof(cfg));
	cfg.threadPriority = 80;
	cfg.threadStackSize = 0x4000u;
	cfg.maxSessions = 4u;
	cfg.syncCallCount = 1u;
	cfg.maxSyncCallInSize = 49152u;
	cfg.maxSyncCallOutSize = 49152u;
	cfg.asyncCallCount = 0u;
	cfg.maxAsyncCallInSize = 0u;
	cfg.maxAsyncCallOutSize = 0u;
	cfg.blockingIpcCount = 1u;
	cfg.maxBlockingInSize = 49152u;
	cfg.maxBlockingOutSize = 49152u;
	cfg.eventFlagCount = 0u;
	cfg.msgQueueCount = 1u;
	for (int idx = 0; idx < 32; idx++) {
		cfg.msgQueue[idx].msgSize = 256u;
		cfg.msgQueue[idx].msgCount = 8u;
	}
	cfg.msgQueue[0].msgSize = 256u;
	cfg.msgQueue[0].msgCount = 2u;
	cfg.slotCount = 32u;
	cfg.budget = 0;
	sceClibStrlcpy(cfg.name, "SceNpScore", sizeof(cfg.name));
	cfg.callbacks.createRequest = npScoreOnCreateRequest;
	cfg.callbacks.destroyRequest = npScoreOnDestroyRequest;
	cfg.callbacks.abortRequest = npScoreOnAbortRequest;
	cfg.callbacks.onSessionCreated = npScoreOnSessionCreated;
	cfg.callbacks.onSessionClosedByClient = npScoreOnSessionClosed;
	cfg.callbacks.onSyncMethod = npScoreSyncDispatch;
	cfg.callbacks.onAsyncMethod = npScoreAsyncUnsupported;
	cfg.callbacks.onSessionKilled = npScoreOnSessionKilled;

	SceSize bufSize = BlockingIpc::Server::getBufferSize(&cfg);
	void *serverBuf = npCommunityAlloc(bufSize);
	if (serverBuf == NULL)
		return SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;

	int ret = BlockingIpc::Server::create(&s_scoreIpcServer, &cfg, serverBuf);
	if (ret != 0) {
		sceNpCommonFreeNpServerName(serverBuf);
		return ret;
	}

	s_scoreIpcServer->start();
	npScoreWorkerInit();
	return 0;
}

static int npScoreAsyncUnsupported(IPMI::Session *session, SceUInt32 requestId,
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

/* 0x8131F7FC */
static int npScoreOnCreateRequest(IPMI::Session *session, const void *param, SceSize paramSize)
{
	if (paramSize != sizeof(NpScoreCreateRequestParam))
		return NP_BLOCKING_IPC_ERR_INVALID_SIZE;

	const NpScoreCreateRequestParam *reqParam =
		static_cast<const NpScoreCreateRequestParam *>(param);
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(session->getProcessId(), NP_COMMUNITY_SERVICE_SCORE,
					  &client);
	if (ret >= 0) {
		ret = npCommunityCreateRequest(client, NP_COMMUNITY_TX_SCORE,
					       reqParam->savedataBindMode, &reqParam->selfNpId,
					       &reqParam->commId, &reqParam->passphrase,
					       &reqParam->timeoutConfig);
	}
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x8131F87E */
static int npScoreOnDestroyRequest(IPMI::Session *session, int requestId)
{
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(session->getProcessId(), NP_COMMUNITY_SERVICE_SCORE,
					  &client);
	if (ret >= 0)
		ret = npCommunityDestroyRequest(client, requestId);
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x8131F8CE */
static int npScoreOnAbortRequest(IPMI::Session *session, int requestId)
{
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(session->getProcessId(), NP_COMMUNITY_SERVICE_SCORE,
					  &client);
	if (ret >= 0)
		ret = npCommunityAbortRequest(client, requestId);
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x8131F91E */
static void npScoreOnSessionCreated(IPMI::Session *session)
{
	char threadName[32];
	sceClibMemset(threadName, 0, sizeof(threadName));
	SceUID pid = session->getProcessId();
	sceClibSnprintf(threadName, sizeof(threadName) - 1, "%s_%08x",
			"SceNpScoreIpcServerWork", pid);

	NpCommunityWorkerThreadConfig cfg;
	sceClibMemset(&cfg, 0, sizeof(cfg));
	cfg.threadName = threadName;
	cfg.entry = npScoreWorkerThread;
	cfg.initPriority = 96;
	cfg.stackSize = 0x8000u;

	NpCommunityClientContext *client = NULL;
	if (npCommunityCreateClientSession(NP_COMMUNITY_SERVICE_SCORE, pid, &cfg, &client) >= 0)
		sceKernelStartThread(client->workerThreadId, sizeof(client), &client);

	while (client != NULL && npCommunityReleaseClient(client) < 0)
		;
}

/* 0x8131F9CE */
static void npScoreOnSessionClosed(IPMI::Session *session)
{
	npCommunityCloseClientSession(session->getProcessId(), NP_COMMUNITY_SERVICE_SCORE);
}

/* 0x8131F9DE */
static int npScoreNotifyTitleUsed(IPMI::Session *session, const SceNpCommunicationId *commId)
{
	(void)session;
	char commIdText[16];
	sceClibMemset(commIdText, 0, 13u);
	if (SceNpCommon_E426FEE8(commId, commIdText, 13u) >= 0)
		SceNpManager_9554802B("NpScore", commIdText, NULL);
	return 0;
}

/* 0x8131FA30 */
static int npScoreChangeBindMode(IPMI::Session *session, SceInt32 requestId, SceInt32 bindMode)
{
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(session->getProcessId(), NP_COMMUNITY_SERVICE_SCORE,
					  &client);
	if (ret >= 0)
		ret = npCommunityChangeBindMode(client, requestId, bindMode);
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x8131FA84 */
static int npScoreEnqueueWork(IPMI::Session *session, SceInt32 requestId, SceInt32 opCode,
			      SceInt32 reservedOpWord, SceSize payloadSize, const void *payloadBuf)
{
	(void)reservedOpWord;
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(session->getProcessId(), NP_COMMUNITY_SERVICE_SCORE,
					  &client);
	if (ret >= 0) {
		NpCommunityWorkPayloads payloads;
		sceClibMemset(&payloads, 0, sizeof(payloads));
		if (payloadSize != 0) {
			payloads.slices[0].buf = const_cast<void *>(payloadBuf);
			payloads.slices[0].size = payloadSize;
		}
		ret = npCommunityEnqueueWork(opCode, client, requestId, &payloads);
	}
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x8131FB06 */
static int npScoreGetResult(IPMI::Session *session, SceInt32 requestId, SceSize maxReadSize,
			    SceSize *outBytesRead, void *outBuf)
{
	*outBytesRead = 0;
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(session->getProcessId(), NP_COMMUNITY_SERVICE_SCORE,
					  &client);
	if (ret >= 0) {
		ret = npCommunityReadResult(client, requestId, maxReadSize, outBytesRead, outBuf,
					    0u, 0u, NULL, NULL, 0u, 1);
		if (ret < 0)
			*outBytesRead = 0;
	}
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x8131FB8A */
static int npScoreGetResultRange(IPMI::Session *session, SceInt32 requestId, SceUInt32 readOffset,
				 SceInt32 clearAfterRead, SceSize maxReadSize,
				 SceSize *outBytesRead, void *outBuf)
{
	*outBytesRead = 0;
	NpCommunityClientContext *client = NULL;
	int ret = npCommunityLookupClient(session->getProcessId(), NP_COMMUNITY_SERVICE_SCORE,
					  &client);
	if (ret >= 0) {
		ret = npCommunityReadResult(client, requestId, maxReadSize, outBytesRead, outBuf,
					    readOffset, 0u, NULL, NULL, 0u, clearAfterRead);
		if (ret < 0)
			*outBytesRead = 0;
	}
	if (client != NULL)
		npCommunityReleaseClient(client);
	return ret;
}

/* 0x8131FC1A */
static void npScoreOnSessionKilled(IPMI::Session *session)
{
	npCommunityCloseClientSession(session->getProcessId(), NP_COMMUNITY_SERVICE_SCORE);
}

/* 0x8131FC2C */
static int npScoreResolveServerHosts(NpScoreServerHosts *outHosts, NpCommunityRequestNode *req,
				     const SceNpCommunicationId *commId, const SceNpId *selfNpId)
{
	int ret = npCommunityCheckNotAborted(req);
	if (ret < 0)
		return ret;

	ret = sceKernelLockLwMutex(&s_scoreKeepAliveMutex, 1, NULL);
	if (ret < 0)
		return ret;

	SceRtcTick nowTick;
	sceRtcGetCurrentTick(&nowTick);
	SceUInt64 expireTick = s_scoreServerCache.cachedAt.tick +
			       NP_COMMUNITY_SERVER_CACHE_TTL_USEC;
	int lookupTxId = 0;
	bool cacheHit = false;

	if (sceClibMemcmp(&s_scoreServerCache.commId, commId, sizeof(SceNpCommunicationId)) != 0) {
		if (s_scoreViewHttpConn != 0) {
			sceHttpDeleteConnection(s_scoreViewHttpConn);
			s_scoreViewHttpConn = 0;
		}
		if (s_scoreViewHttpsConn != 0) {
			sceHttpDeleteConnection(s_scoreViewHttpsConn);
			s_scoreViewHttpsConn = 0;
		}
		if (s_scoreRecordHttpsConn != 0) {
			sceHttpDeleteConnection(s_scoreRecordHttpsConn);
			s_scoreRecordHttpsConn = 0;
		}
	} else if (expireTick > nowTick.tick) {
		sceClibMemcpy_safe(outHosts, &s_scoreServerCache.hosts, sizeof(NpScoreServerHosts));
		cacheHit = true;
	}

	if (!cacheHit) {
		ret = npCommunityCreateTx(selfNpId, NP_COMMUNITY_TX_SCORE, NULL);
		if (ret >= 0) {
			lookupTxId = ret;
			req->serverLookupTxId = lookupTxId;
			ret = npCommunitySetTxTimeout(lookupTxId, &req->timeoutConfig);
			if (ret >= 0)
				ret = npCommunityCheckNotAborted(req);
			if (ret >= 0) {
				ret = npScoreBackendGetServerInfo(lookupTxId, commId,
								  &s_scoreServerCache.hosts);
				req->serverLookupTxId = 0;
				if (ret >= 0) {
					sceClibMemcpy_safe(outHosts, &s_scoreServerCache.hosts,
							   sizeof(NpScoreServerHosts));
					s_scoreServerCache.cachedAt = nowTick;
					s_scoreServerCache.commId = *commId;
					ret = 0;
				} else if (ret == SCE_NP_COMMUNITY_SERVER_ERROR_NO_SUCH_TITLE) {
					npCommunityLogServiceNotConfigured("NpScore", commId);
				}
			}
		}
	}

	sceKernelUnlockLwMutex(&s_scoreKeepAliveMutex, 1);
	if (lookupTxId != 0)
		npCommunityDestroyTx(lookupTxId);
	return ret;
}

/* 0x8131FEAA */
static int npScoreAcquireKeepAliveConns(SceInt32 txId, const NpScoreServerHosts *hosts,
					SceInt32 *outViewHttp, SceInt32 *outViewHttps,
					SceInt32 *outRecordHttps)
{
	SceInt32 viewHttp = 0;
	SceInt32 viewHttps = 0;
	SceInt32 recordHttps = 0;

	int ret = sceKernelLockLwMutex(&s_scoreKeepAliveMutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (outViewHttp != NULL) {
		if (s_scoreViewHttpConn != 0) {
			if (sceClibStrncasecmp(s_scoreServerCache.hosts.viewHost, hosts->viewHost,
					       NP_COMMUNITY_HOST_NAME_LEN) == 0) {
				viewHttp = s_scoreViewHttpConn;
				s_scoreViewHttpConn = 0;
			} else {
				sceHttpDeleteConnection(s_scoreViewHttpConn);
				s_scoreViewHttpConn = 0;
			}
		}
		if (viewHttp == 0) {
			int tmplId = npCommunityGetTxHttpTemplate(txId);
			ret = tmplId;
			if (tmplId >= 0)
				ret = sceHttpCreateConnection(tmplId, hosts->viewHost, "http",
							      80u, SCE_TRUE);
			if (ret >= 0)
				viewHttp = ret;
		}
	}

	if (ret >= 0 && outViewHttps != NULL) {
		if (s_scoreViewHttpsConn != 0) {
			if (sceClibStrncasecmp(s_scoreServerCache.hosts.viewHost, hosts->viewHost,
					       NP_COMMUNITY_HOST_NAME_LEN) == 0) {
				viewHttps = s_scoreViewHttpsConn;
				s_scoreViewHttpsConn = 0;
			} else {
				sceHttpDeleteConnection(s_scoreViewHttpsConn);
				s_scoreViewHttpsConn = 0;
			}
		}
		if (viewHttps == 0) {
			int tmplId = npCommunityGetTxHttpTemplate(txId);
			ret = tmplId;
			if (tmplId >= 0)
				ret = sceHttpCreateConnection(tmplId, hosts->viewHost, "https",
							      443u, SCE_TRUE);
			if (ret >= 0)
				viewHttps = ret;
		}
	}

	if (ret >= 0 && outRecordHttps != NULL) {
		if (s_scoreRecordHttpsConn != 0) {
			if (sceClibStrncasecmp(s_scoreServerCache.hosts.recordHost,
					       hosts->recordHost,
					       NP_COMMUNITY_HOST_NAME_LEN) == 0) {
				recordHttps = s_scoreRecordHttpsConn;
				s_scoreRecordHttpsConn = 0;
			} else {
				sceHttpDeleteConnection(s_scoreRecordHttpsConn);
				s_scoreRecordHttpsConn = 0;
			}
		}
		if (recordHttps == 0) {
			int tmplId = npCommunityGetTxHttpTemplate(txId);
			ret = tmplId;
			if (tmplId >= 0)
				ret = sceHttpCreateConnection(tmplId, hosts->recordHost, "https",
							      443u, SCE_TRUE);
			if (ret >= 0)
				recordHttps = ret;
		}
	}

	sceKernelUnlockLwMutex(&s_scoreKeepAliveMutex, 1);

	if (ret >= 0) {
		if (outRecordHttps != NULL)
			*outRecordHttps = recordHttps;
		if (outViewHttp != NULL)
			*outViewHttp = viewHttp;
		if (outViewHttps != NULL)
			*outViewHttps = viewHttps;
	} else {
		if (recordHttps != 0)
			sceHttpDeleteConnection(recordHttps);
		if (viewHttp != 0)
			sceHttpDeleteConnection(viewHttp);
		if (viewHttps != 0)
			sceHttpDeleteConnection(viewHttps);
	}
	return ret;
}

/* 0x813200A6 */
static int npScoreReturnKeepAliveConns(const NpScoreServerHosts *hosts, SceInt32 viewHttp,
				       SceInt32 viewHttps, SceInt32 recordHttps)
{
	int ret = sceKernelLockLwMutex(&s_scoreKeepAliveMutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (viewHttp != 0) {
		if (sceClibStrncasecmp(s_scoreServerCache.hosts.viewHost, hosts->viewHost,
				       NP_COMMUNITY_HOST_NAME_LEN) != 0) {
			sceHttpDeleteConnection(viewHttp);
		} else {
			if (s_scoreViewHttpConn != 0)
				sceHttpDeleteConnection(s_scoreViewHttpConn);
			s_scoreViewHttpConn = viewHttp;
		}
	}
	if (viewHttps != 0) {
		if (sceClibStrncasecmp(s_scoreServerCache.hosts.viewHost, hosts->viewHost,
				       NP_COMMUNITY_HOST_NAME_LEN) != 0) {
			sceHttpDeleteConnection(viewHttps);
		} else {
			if (s_scoreViewHttpsConn != 0)
				sceHttpDeleteConnection(s_scoreViewHttpsConn);
			s_scoreViewHttpsConn = viewHttps;
		}
	}
	if (recordHttps != 0) {
		if (sceClibStrncasecmp(s_scoreServerCache.hosts.recordHost, hosts->recordHost,
				       NP_COMMUNITY_HOST_NAME_LEN) != 0) {
			sceHttpDeleteConnection(recordHttps);
		} else {
			if (s_scoreRecordHttpsConn != 0)
				sceHttpDeleteConnection(s_scoreRecordHttpsConn);
			s_scoreRecordHttpsConn = recordHttps;
		}
	}

	sceKernelUnlockLwMutex(&s_scoreKeepAliveMutex, 1);
	return 0;
}

static void npScoreCloseAllKeepAliveConns(void)
{
	if (sceKernelLockLwMutex(&s_scoreKeepAliveMutex, 1, NULL) >= 0) {
		if (s_scoreRecordHttpsConn != 0) {
			sceHttpDeleteConnection(s_scoreRecordHttpsConn);
			s_scoreRecordHttpsConn = 0;
		}
		if (s_scoreViewHttpConn != 0) {
			sceHttpDeleteConnection(s_scoreViewHttpConn);
			s_scoreViewHttpConn = 0;
		}
		if (s_scoreViewHttpsConn != 0) {
			sceHttpDeleteConnection(s_scoreViewHttpsConn);
			s_scoreViewHttpsConn = 0;
		}
		sceKernelUnlockLwMutex(&s_scoreKeepAliveMutex, 1);
	}
}

static int npScoreCheckSavedataOwner(NpCommunityClientContext *client, SceInt32 requestId,
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
								"NpScore",
								"invalid savedata owner", NULL);
							ret = SCE_NP_COMMUNITY_ERROR_SCORE_INVALID_SAVEDATA_OWNER;
						} else if (isFakeOwner && isProdEnv) {
							SceNpManager_9554802B(
								"NpScore",
								"fakeOwner not allowed in product env",
								NULL);
							ret = SCE_NP_COMMUNITY_ERROR_SCORE_INVALID_SAVEDATA_OWNER;
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

static void npScoreHandleGetBoardInfo(NpCommunityClientContext *client, SceInt32 requestId,
				      const NpScoreBoardInfoPayload *payload)
{
	int netToken = SceShell_6B8772A2(client->processId);
	NpCommunityRequestNode *req = NULL;
	SceInt32 viewHttp = 0;
	NpScoreServerHosts hosts;
	NpCommunityResultPacket *pkt = NULL;
	NpScoreBoardInfo *boardInfo = NULL;
	bool completed = false;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npScoreResolveServerHosts(&hosts, req, &payload->commId, &payload->selfNpId);
	if (ret >= 0) {
		npScoreAcquireKeepAliveConns(requestId, &hosts, &viewHttp, NULL, NULL);
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			boardInfo = static_cast<NpScoreBoardInfo *>(
				npCommunityAlloc(sizeof(NpScoreBoardInfo)));
			if (boardInfo == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(boardInfo, 0, sizeof(NpScoreBoardInfo));
				pkt->headBuf = boardInfo;
				pkt->headSize = sizeof(NpScoreBoardInfo);

				NpScoreRequestEnv env;
				sceClibMemset(&env, 0, sizeof(env));
				env.commId = &payload->commId;
				env.hostName = hosts.viewHost;
				env.boardId = payload->boardId;
				env.httpConnId = viewHttp;

				ret = npScoreBackendGetBoardInfo(requestId, &env,
								 &req->passphrase, boardInfo, NULL);
				if (ret >= 0) {
					int opResult = ret;
					ret = npCommunityTrackRequestAlloc(req, boardInfo);
					if (ret >= 0) {
						boardInfo = NULL;
						ret = npCommunityTrackRequestAlloc(req, pkt);
						if (ret >= 0) {
							npScoreReturnKeepAliveConns(&hosts,
										    viewHttp, 0, 0);
							req->resultPacket = pkt;
							req->resultCode = opResult;
							req->resultReady = 1;
							npCommunityReleaseRequest(req);
							pkt = NULL;
							req = NULL;
							viewHttp = 0;
							completed = true;
						}
					}
				}
			}
		}
	}

	if (!completed) {
		if (boardInfo != NULL)
			npCommunityFree(boardInfo);
		if (pkt != NULL)
			npCommunityFree(pkt);
		if (req != NULL) {
			req->resultCode = ret;
			npCommunityReleaseRequest(req);
		}
		if (viewHttp != 0)
			npScoreReturnKeepAliveConns(&hosts, viewHttp, 0, 0);
	}
	SceShell_794BF5BE(static_cast<unsigned int>(netToken));
}

static void npScoreHandleRecordScore(NpCommunityClientContext *client, SceInt32 requestId,
				     const NpScoreRecordScorePayload *payload)
{
	int netToken = SceShell_6B8772A2(client->processId);
	NpCommunityRequestNode *req = NULL;
	SceInt32 recordHttps = 0;
	NpScoreServerHosts hosts;
	NpCommunityResultPacket *pkt = NULL;
	NpScoreRecordScoreResult *resultBuf = NULL;
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;
	bool completed = false;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npScoreResolveServerHosts(&hosts, req, &payload->commId, &payload->selfNpId);
	if (ret >= 0) {
		npScoreAcquireKeepAliveConns(requestId, &hosts, NULL, NULL, &recordHttps);
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			resultBuf = static_cast<NpScoreRecordScoreResult *>(
				npCommunityAlloc(sizeof(NpScoreRecordScoreResult)));
			if (resultBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(resultBuf, 0, sizeof(NpScoreRecordScoreResult));
				resultBuf->requestId = requestId;
				pkt->headBuf = resultBuf;
				pkt->headSize = sizeof(NpScoreRecordScoreResult);

				SceUInt32 *tmpRankPtr = (payload->hasTmpRank != 0)
								? &resultBuf->tmpRank
								: NULL;
				const SceRtcTick *comparePtr = (payload->hasCompareDate != 0)
								       ? &payload->compareDate
								       : NULL;
				SceUInt8 sessionKey[52];
				ret = npCommunityGetTxSessionKey(payload->txId, sessionKey);
				if (ret >= 0)
					ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
				if (ret >= 0) {
					NpScoreRequestEnv env;
					sceClibMemset(&env, 0, sizeof(env));
					env.commId = &payload->commId;
					env.hostName = hosts.recordHost;
					env.boardId = payload->boardId;
					env.clientPid = client->processId;
					env.httpConnId = recordHttps;

					ret = npScoreBackendRecordScore(
						payload->txId, &env, &payload->selfNpId,
						payload->pcId, sessionKey, &req->passphrase,
						ticketBuf, ticketSize, payload->scoreValue,
						&payload->comment, &payload->gameInfo,
						tmpRankPtr, comparePtr, NULL);
					if (ret >= 0) {
						int opResult = ret;
						npCommunityFree(ticketBuf);
						ticketBuf = NULL;
						ret = npCommunityTrackRequestAlloc(req, resultBuf);
						if (ret >= 0) {
							resultBuf = NULL;
							ret = npCommunityTrackRequestAlloc(req,
											   pkt);
							if (ret >= 0) {
								npScoreReturnKeepAliveConns(
									&hosts, 0, 0, recordHttps);
								req->resultPacket = pkt;
								req->resultCode = opResult;
								req->resultReady = 1;
								npCommunityReleaseRequest(req);
								pkt = NULL;
								req = NULL;
								recordHttps = 0;
								completed = true;
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
		if (resultBuf != NULL)
			npCommunityFree(resultBuf);
		if (pkt != NULL)
			npCommunityFree(pkt);
		if (req != NULL) {
			req->resultCode = ret;
			npCommunityReleaseRequest(req);
		}
		if (recordHttps != 0)
			npScoreReturnKeepAliveConns(&hosts, 0, 0, recordHttps);
	}
	SceShell_794BF5BE(static_cast<unsigned int>(netToken));
}

static void npScoreHandleRecordGameData(NpCommunityClientContext *client, SceInt32 requestId,
					const NpScoreRecordGameDataPayload *payload)
{
	int netToken = SceShell_6B8772A2(client->processId);
	NpCommunityRequestNode *req = NULL;
	NpScoreServerHosts hosts;
	NpCommunityResultPacket *pkt = NULL;
	SceInt32 *reqIdBuf = NULL;
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;
	bool completed = false;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npScoreResolveServerHosts(&hosts, req, &payload->commId, &payload->selfNpId);
	if (ret >= 0) {
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			reqIdBuf = static_cast<SceInt32 *>(npCommunityAlloc(sizeof(SceInt32)));
			if (reqIdBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				*reqIdBuf = requestId;
				pkt->headBuf = reqIdBuf;
				pkt->headSize = sizeof(SceInt32);
				ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
				if (ret >= 0) {
					NpScoreRequestEnv env;
					sceClibMemset(&env, 0, sizeof(env));
					env.commId = &payload->commId;
					env.hostName = hosts.recordHost;
					env.boardId = payload->boardId;
					env.httpConnId = 0;

					ret = npScoreBackendRecordGameData(
						payload->txId, &env, &payload->selfNpId,
						payload->pcId, ticketBuf, ticketSize,
						payload->scoreValue, payload->totalSize,
						payload->sendSize, payload->chunkBytes, NULL);
					if (ret >= 0) {
						npCommunityFree(ticketBuf);
						ticketBuf = NULL;
						ret = npCommunityTrackRequestAlloc(req, reqIdBuf);
						if (ret >= 0) {
							reqIdBuf = NULL;
							ret = npCommunityTrackRequestAlloc(req,
											   pkt);
							if (ret >= 0) {
								req->resultPacket = pkt;
								req->resultCode = 0;
								req->resultReady = 1;
								npCommunityReleaseRequest(req);
								pkt = NULL;
								req = NULL;
								completed = true;
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
		if (reqIdBuf != NULL)
			npCommunityFree(reqIdBuf);
		if (pkt != NULL)
			npCommunityFree(pkt);
		if (req != NULL) {
			req->resultCode = ret;
			npCommunityReleaseRequest(req);
		}
	}
	SceShell_794BF5BE(static_cast<unsigned int>(netToken));
}

static void npScoreHandleGetGameData(NpCommunityClientContext *client, SceInt32 requestId,
				     const NpScoreGetGameDataPayload *payload)
{
	int netToken = SceShell_6B8772A2(client->processId);
	NpCommunityRequestNode *req = NULL;
	NpScoreServerHosts hosts;
	NpCommunityResultPacket *pkt = NULL;
	NpScoreGetGameDataResult *resultBuf = NULL;
	bool completed = false;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npScoreResolveServerHosts(&hosts, req, &payload->commId, &payload->selfNpId);
	if (ret >= 0) {
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			resultBuf = static_cast<NpScoreGetGameDataResult *>(
				npCommunityAlloc(sizeof(NpScoreGetGameDataResult)));
			if (resultBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(resultBuf, 0, sizeof(NpScoreGetGameDataResult));
				resultBuf->requestId = requestId;
				pkt->headBuf = resultBuf;
				pkt->headSize = sizeof(NpScoreGetGameDataResult);

				NpScoreRequestEnv env;
				sceClibMemset(&env, 0, sizeof(env));
				env.commId = &payload->commId;
				env.hostName = hosts.viewHost;
				env.boardId = payload->boardId;
				env.httpConnId = 0;

				ret = npScoreBackendGetGameData(
					payload->txId, &env, payload->pcId, &req->passphrase,
					&payload->targetNpId, &resultBuf->totalSize,
					payload->recvSize, resultBuf->chunkBytes, 0u, NULL);
				if (ret >= 0) {
					int opResult = ret;
					resultBuf->recvSize = static_cast<SceSize>(opResult);
					ret = npCommunityTrackRequestAlloc(req, resultBuf);
					if (ret >= 0) {
						resultBuf = NULL;
						ret = npCommunityTrackRequestAlloc(req, pkt);
						if (ret >= 0) {
							req->resultPacket = pkt;
							req->resultCode = opResult;
							req->resultReady = 1;
							npCommunityReleaseRequest(req);
							pkt = NULL;
							req = NULL;
							completed = true;
						}
					}
				}
			}
		}
	}

	if (!completed) {
		if (resultBuf != NULL)
			npCommunityFree(resultBuf);
		if (pkt != NULL)
			npCommunityFree(pkt);
		if (req != NULL) {
			req->resultCode = ret;
			npCommunityReleaseRequest(req);
		}
	}
	SceShell_794BF5BE(static_cast<unsigned int>(netToken));
}

static void npScoreHandleGetRankingByNpId(NpCommunityClientContext *client, SceInt32 requestId,
					  const NpScoreGetRankingByNpIdPayload *payload)
{
	int netToken = SceShell_6B8772A2(client->processId);
	NpCommunityRequestNode *req = NULL;
	SceInt32 viewHttp = 0;
	NpScoreServerHosts hosts;
	NpCommunityResultPacket *pkt = NULL;
	NpScoreGetRankingByNpIdResult *resultBuf = NULL;
	bool completed = false;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npScoreResolveServerHosts(&hosts, req, &payload->commId, &payload->selfNpId);
	if (ret >= 0) {
		npScoreAcquireKeepAliveConns(requestId, &hosts, &viewHttp, NULL, NULL);
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			resultBuf = static_cast<NpScoreGetRankingByNpIdResult *>(
				npCommunityAlloc(sizeof(NpScoreGetRankingByNpIdResult)));
			if (resultBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(resultBuf, 0, sizeof(NpScoreGetRankingByNpIdResult));
				resultBuf->requestId = requestId;
				pkt->headBuf = resultBuf;
				pkt->headSize = sizeof(NpScoreGetRankingByNpIdResult);

				NpScoreRequestEnv env;
				sceClibMemset(&env, 0, sizeof(env));
				env.commId = &payload->commId;
				env.hostName = hosts.viewHost;
				env.boardId = payload->boardId;
				env.httpConnId = viewHttp;

				NpScoreComment *comments = (payload->wantComment == 1)
								   ? resultBuf->commentArray
								   : NULL;
				NpScoreGameInfo *gameInfos = (payload->wantGameInfo == 1)
								     ? resultBuf->gameInfoArray
								     : NULL;

				ret = npScoreBackendGetRankingByNpId(
					requestId, &env, &req->passphrase, payload->targets,
					resultBuf->rankArray, NULL, NULL, comments, gameInfos,
					payload->arrayNum, &resultBuf->lastSortDate,
					&resultBuf->totalRecord, NULL);
				if (ret >= 0) {
					int opResult = ret;
					ret = npCommunityTrackRequestAlloc(req, resultBuf);
					if (ret >= 0) {
						resultBuf = NULL;
						ret = npCommunityTrackRequestAlloc(req, pkt);
						if (ret >= 0) {
							npScoreReturnKeepAliveConns(&hosts,
										    viewHttp, 0, 0);
							req->resultPacket = pkt;
							req->resultCode = opResult;
							req->resultReady = 1;
							npCommunityReleaseRequest(req);
							pkt = NULL;
							req = NULL;
							viewHttp = 0;
							completed = true;
						}
					}
				}
			}
		}
	}

	if (!completed) {
		if (resultBuf != NULL)
			npCommunityFree(resultBuf);
		if (pkt != NULL)
			npCommunityFree(pkt);
		if (req != NULL) {
			req->resultCode = ret;
			npCommunityReleaseRequest(req);
		}
		if (viewHttp != 0)
			npScoreReturnKeepAliveConns(&hosts, viewHttp, 0, 0);
	}
	SceShell_794BF5BE(static_cast<unsigned int>(netToken));
}

static void npScoreHandleGetRankingByRange(NpCommunityClientContext *client, SceInt32 requestId,
					   const NpScoreGetRankingByRangePayload *payload)
{
	int netToken = SceShell_6B8772A2(client->processId);
	NpCommunityRequestNode *req = NULL;
	NpScoreServerHosts hosts;
	NpCommunityResultPacket *pkt = NULL;
	NpScoreGetRankingByRangeResult *resultBuf = NULL;
	bool completed = false;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npScoreResolveServerHosts(&hosts, req, &payload->commId, &payload->selfNpId);
	if (ret >= 0) {
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			resultBuf = static_cast<NpScoreGetRankingByRangeResult *>(
				npCommunityAlloc(sizeof(NpScoreGetRankingByRangeResult)));
			if (resultBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(resultBuf, 0, sizeof(NpScoreGetRankingByRangeResult));
				resultBuf->requestId = requestId;
				pkt->headBuf = resultBuf;
				pkt->headSize = sizeof(NpScoreGetRankingByRangeResult);

				NpScoreRequestEnv env;
				sceClibMemset(&env, 0, sizeof(env));
				env.commId = &payload->commId;
				env.hostName = hosts.viewHost;
				env.boardId = payload->boardId;

				NpScoreComment *comments = (payload->wantComment == 1)
								   ? resultBuf->commentArray
								   : NULL;
				NpScoreGameInfo *gameInfos = (payload->wantGameInfo == 1)
								     ? resultBuf->gameInfoArray
								     : NULL;

				if (payload->friendsScope == 1 || payload->friendsScope == 2) {
					SceInt32 viewHttps = 0;
					void *ticketBuf = NULL;
					SceSize ticketSize = 0;
					npScoreAcquireKeepAliveConns(requestId, &hosts, NULL,
								     &viewHttps, NULL);
					env.httpConnId = viewHttps;
					ret = npCommunityFetchTicket(&ticketBuf, &ticketSize);
					if (ret >= 0) {
						SceInt32 includeSelf =
							(payload->friendsScope == 2) ? 1 : 0;
						ret = npScoreBackendGetFriendsRanking(
							requestId, &env, &payload->selfNpId,
							payload->pcId, &req->passphrase,
							ticketBuf, ticketSize,
							payload->startSerialRank,
							resultBuf->rankArray, comments, gameInfos,
							payload->arrayNum,
							&resultBuf->lastSortDate,
							&resultBuf->totalRecord,
							&resultBuf->hits, includeSelf, NULL);
					}
					if (ticketBuf != NULL)
						npCommunityFree(ticketBuf);
					if (viewHttps != 0)
						npScoreReturnKeepAliveConns(&hosts, 0, viewHttps,
									    0);
				} else {
					SceInt32 viewHttp = 0;
					npScoreAcquireKeepAliveConns(requestId, &hosts, &viewHttp,
								     NULL, NULL);
					env.httpConnId = viewHttp;
					ret = npScoreBackendGetRankingByRange(
						requestId, &env, &req->passphrase,
						payload->startSerialRank, resultBuf->rankArray,
						NULL, payload->friendsScope, comments, gameInfos,
						payload->arrayNum, &resultBuf->lastSortDate,
						&resultBuf->totalRecord, NULL);
					if (viewHttp != 0)
						npScoreReturnKeepAliveConns(&hosts, viewHttp, 0, 0);
				}

				if (ret >= 0) {
					int opResult = ret;
					ret = npCommunityTrackRequestAlloc(req, resultBuf);
					if (ret >= 0) {
						resultBuf = NULL;
						ret = npCommunityTrackRequestAlloc(req, pkt);
						if (ret >= 0) {
							req->resultPacket = pkt;
							req->resultCode = opResult;
							req->resultReady = 1;
							npCommunityReleaseRequest(req);
							pkt = NULL;
							req = NULL;
							completed = true;
						}
					}
				}
			}
		}
	}

	if (!completed) {
		if (resultBuf != NULL)
			npCommunityFree(resultBuf);
		if (pkt != NULL)
			npCommunityFree(pkt);
		if (req != NULL) {
			req->resultCode = ret;
			npCommunityReleaseRequest(req);
		}
	}
	SceShell_794BF5BE(static_cast<unsigned int>(netToken));
}

static void npScoreHandleFilterComment(NpCommunityClientContext *client, SceInt32 requestId,
				       const NpScoreFilterCommentPayload *payload)
{
	int netToken = SceShell_6B8772A2(client->processId);
	NpCommunityRequestNode *req = NULL;
	SceInt32 viewHttp = 0;
	NpScoreServerHosts hosts;
	NpCommunityResultPacket *pkt = NULL;
	NpScoreFilterCommentResult *resultBuf = NULL;
	bool completed = false;

	int ret = npCommunityLookupRequest(&req, client, requestId);
	if (ret >= 0)
		ret = npScoreResolveServerHosts(&hosts, req, &payload->commId, &payload->selfNpId);
	if (ret >= 0) {
		npScoreAcquireKeepAliveConns(requestId, &hosts, &viewHttp, NULL, NULL);
		pkt = static_cast<NpCommunityResultPacket *>(
			npCommunityAlloc(sizeof(NpCommunityResultPacket)));
		if (pkt == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(pkt, 0, sizeof(NpCommunityResultPacket));
			resultBuf = static_cast<NpScoreFilterCommentResult *>(
				npCommunityAlloc(sizeof(NpScoreFilterCommentResult)));
			if (resultBuf == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(resultBuf, 0, sizeof(NpScoreFilterCommentResult));
				resultBuf->requestId = requestId;
				pkt->headBuf = resultBuf;
				pkt->headSize = sizeof(NpScoreFilterCommentResult);

				NpScoreRequestEnv env;
				sceClibMemset(&env, 0, sizeof(env));
				env.commId = &payload->commId;
				env.hostName = hosts.viewHost;
				env.boardId = 0u;
				env.httpConnId = viewHttp;

				ret = npScoreBackendFilterComment(
					requestId, &env, &payload->selfNpId, &req->passphrase,
					payload->comment, resultBuf->comment, payload->isSanitize);
				if (ret >= 0) {
					int opResult = ret;
					ret = npCommunityTrackRequestAlloc(req, resultBuf);
					if (ret >= 0) {
						resultBuf = NULL;
						ret = npCommunityTrackRequestAlloc(req, pkt);
						if (ret >= 0) {
							npScoreReturnKeepAliveConns(&hosts,
										    viewHttp, 0, 0);
							req->resultPacket = pkt;
							req->resultCode = opResult;
							req->resultReady = 1;
							npCommunityReleaseRequest(req);
							pkt = NULL;
							req = NULL;
							viewHttp = 0;
							completed = true;
						}
					}
				}
			}
		}
	}

	if (!completed) {
		if (resultBuf != NULL)
			npCommunityFree(resultBuf);
		if (pkt != NULL)
			npCommunityFree(pkt);
		if (req != NULL) {
			req->resultCode = ret;
			npCommunityReleaseRequest(req);
		}
		if (viewHttp != 0)
			npScoreReturnKeepAliveConns(&hosts, viewHttp, 0, 0);
	}
	SceShell_794BF5BE(static_cast<unsigned int>(netToken));
}

/* 0x81320188 */
static int npScoreWorkerThread(SceSize args, void *argp)
{
	(void)args;
	NpCommunityClientContext *client = *static_cast<NpCommunityClientContext **>(argp);
	s_scoreRateLimitEnabled = SceNpCommon_BBC13621();

	bool running = true;
	while (running) {
		int hasKeepAlive = 0;
		if (sceKernelLockLwMutex(&s_scoreKeepAliveMutex, 1, NULL) >= 0) {
			if ((s_scoreRecordHttpsConn | s_scoreViewHttpConn |
			     s_scoreViewHttpsConn) != 0)
				hasKeepAlive = 1;
			sceKernelUnlockLwMutex(&s_scoreKeepAliveMutex, 1);
		}

		SceUInt32 waitTimeout = NP_COMMUNITY_KEEPALIVE_TIMEOUT_USEC;
		SceUInt32 *timeoutPtr = (hasKeepAlive != 0) ? &waitTimeout : NULL;
		int waitRet = sceKernelWaitSema(client->workSemaId, 1, timeoutPtr);
		if (waitRet < 0) {
			if (waitRet == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
				npScoreCloseAllKeepAliveConns();
				continue;
			}
			break;
		}

		int lockRet = sceKernelLockLwMutex(&s_scoreWorkQueueMutex, 1, NULL);
		if (lockRet < 0)
			return lockRet;

		NpCommunityWorkItem work = client->workQueue[client->queueReadIndex];
		client->queueReadIndex++;
		if (client->queueReadIndex == NP_COMMUNITY_WORK_QUEUE_DEPTH)
			client->queueReadIndex = 0;
		sceKernelUnlockLwMutex(&s_scoreWorkQueueMutex, 1);

		if (work.opCode == NP_SCORE_WORK_SHUTDOWN) {
			running = false;
			break;
		}

		SceInt32 requiredBit = 0;
		switch (work.opCode) {
		case NP_SCORE_WORK_GET_BOARD_INFO:
		case NP_SCORE_WORK_GET_GAME_DATA:
		case NP_SCORE_WORK_GET_RANKING_BY_NPID:
		case NP_SCORE_WORK_GET_RANKING_BY_RANGE:
		case NP_SCORE_WORK_FILTER_COMMENT:
			requiredBit = NP_COMMUNITY_BINDMODE_RDONLY;
			break;
		case NP_SCORE_WORK_RECORD_SCORE:
		case NP_SCORE_WORK_RECORD_GAME_DATA:
			requiredBit = NP_COMMUNITY_BINDMODE_WRONLY;
			break;
		default:
			break;
		}

		int ownerRet = 0;
		if (requiredBit != 0)
			ownerRet = npScoreCheckSavedataOwner(client, work.requestId, requiredBit);

		void *payloadBuf = work.payloads.slices[0].buf;
		if (ownerRet >= 0) {
			switch (work.opCode) {
			case NP_SCORE_WORK_GET_BOARD_INFO:
				npScoreHandleGetBoardInfo(
					client, work.requestId,
					static_cast<const NpScoreBoardInfoPayload *>(payloadBuf));
				break;
			case NP_SCORE_WORK_RECORD_SCORE:
				npScoreHandleRecordScore(
					client, work.requestId,
					static_cast<const NpScoreRecordScorePayload *>(payloadBuf));
				break;
			case NP_SCORE_WORK_RECORD_GAME_DATA:
				npScoreHandleRecordGameData(
					client, work.requestId,
					static_cast<const NpScoreRecordGameDataPayload *>(
						payloadBuf));
				break;
			case NP_SCORE_WORK_GET_GAME_DATA:
				npScoreHandleGetGameData(
					client, work.requestId,
					static_cast<const NpScoreGetGameDataPayload *>(payloadBuf));
				break;
			case NP_SCORE_WORK_GET_RANKING_BY_NPID:
				npScoreHandleGetRankingByNpId(
					client, work.requestId,
					static_cast<const NpScoreGetRankingByNpIdPayload *>(
						payloadBuf));
				break;
			case NP_SCORE_WORK_GET_RANKING_BY_RANGE:
				npScoreHandleGetRankingByRange(
					client, work.requestId,
					static_cast<const NpScoreGetRankingByRangePayload *>(
						payloadBuf));
				break;
			case NP_SCORE_WORK_FILTER_COMMENT:
				npScoreHandleFilterComment(
					client, work.requestId,
					static_cast<const NpScoreFilterCommentPayload *>(
						payloadBuf));
				break;
			default:
				break;
			}
		}

		int evRet = sceKernelSetEventFlag(client->eventFlagId, 1u);
		if (payloadBuf != NULL)
			npCommunityFree(payloadBuf);

		if (s_scoreRateLimitEnabled == SCE_TRUE &&
		    work.opCode != NP_SCORE_WORK_RECORD_GAME_DATA &&
		    work.opCode != NP_SCORE_WORK_GET_GAME_DATA &&
		    evRet != SCE_NP_COMMUNITY_ERROR_INVALID_ID &&
		    evRet != SCE_NP_COMMUNITY_ERROR_ABORTED) {
			SceUInt32 delayTimeout = NP_COMMUNITY_RATELIMIT_DELAY_USEC;
			sceKernelLockLwMutex(&client->delayMutex, 1, &delayTimeout);
		}
	}

	npScoreCloseAllKeepAliveConns();
	return 0;
}

/* 0x813210F2 */
static int npScoreWorkerInit(void)
{
	if (s_scoreWorkerInit == 1)
		return SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED;

	sceClibMemset(&s_scoreServerCache, 0, sizeof(s_scoreServerCache));
	int ret = sceKernelCreateLwMutex(&s_scoreWorkQueueMutex, "SceNpScoreWorkQueue", 2u, 0,
					 NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelCreateLwMutex(&s_scoreKeepAliveMutex, "SceNpScoreKeepAlive", 2u, 0, NULL);
	if (ret < 0)
		return ret;
	ret = npCommunityIpcInit(NP_COMMUNITY_SERVICE_SCORE);
	if (ret < 0)
		return ret;

	s_scoreWorkerInit = 1;
	return 0;
}

/* 0x8132384C */
static int npScoreSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
			       const IPMI::DataInfo *inList, SceUInt32 inCount,
			       IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	switch (methodId) {
	case NP_SCORE_IPC_PING_A:
	case NP_SCORE_IPC_PING_B:
		return session->respondToSyncMethodRequest(npCommunityNoop(), outList, outCount);

	case NP_SCORE_IPC_NOTIFY_TITLE: {
		const SceNpCommunicationId *commId =
			static_cast<const SceNpCommunicationId *>(inList[0].pData);
		int ret = npScoreNotifyTitleUsed(session, commId);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_SCORE_IPC_SET_TIMEOUT: {
		SceInt32 reqId = *static_cast<const SceInt32 *>(inList[0].pData);
		const NpCommunityTimeoutConfig *cfg =
			static_cast<const NpCommunityTimeoutConfig *>(inList[1].pData);
		int ret = npCommunitySetTimeout(session, reqId, cfg);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_SCORE_IPC_CHANGE_BIND_MODE: {
		const NpScoreChangeBindModeIn *in =
			static_cast<const NpScoreChangeBindModeIn *>(inList[0].pData);
		int ret = npScoreChangeBindMode(session, in->requestId, in->bindMode);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_SCORE_IPC_ENQUEUE_WORK: {
		const NpScoreEnqueueWorkIn *in =
			static_cast<const NpScoreEnqueueWorkIn *>(inList[0].pData);
		int ret = npScoreEnqueueWork(session, in->requestId, in->opCode, in->reservedOpWord,
					     in->payloadSize, inList[1].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_SCORE_IPC_GET_RESULT: {
		const NpScoreGetResultIn *in =
			static_cast<const NpScoreGetResultIn *>(inList[0].pData);
		SceSize *outLenPtr = static_cast<SceSize *>(outList[0].pBuffer);
		int ret = npScoreGetResult(session, in->requestId, in->maxReadSize, outLenPtr,
					   outList[1].pBuffer);
		outList[0].outSize = sizeof(SceSize);
		outList[1].outSize = *outLenPtr;
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_SCORE_IPC_GET_RESULT_RANGE: {
		const NpScoreGetResultRangeIn *in =
			static_cast<const NpScoreGetResultRangeIn *>(inList[0].pData);
		SceSize *outLenPtr = static_cast<SceSize *>(outList[0].pBuffer);
		int ret = npScoreGetResultRange(session, in->requestId, in->readOffset,
						in->clearAfterRead, in->maxReadSize, outLenPtr,
						outList[1].pBuffer);
		outList[0].outSize = sizeof(SceSize);
		outList[1].outSize = *outLenPtr;
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	default:
		return NP_IPMI_ERR_UNSUPPORTED_METHOD;
	}
}
