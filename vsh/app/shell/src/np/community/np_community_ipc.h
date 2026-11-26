/*
 * np_community_ipc.h: shared internal declarations for SceNpScore, SceNpTus,
 * and SceNpCommunityIpcServer (0x8131F65C..0x8132475B).
 */
#ifndef SHELL_NP_COMMUNITY_IPC_H
#define SHELL_NP_COMMUNITY_IPC_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <rtc.h>
#include <np/np_common.h>
#include <np/np_error.h>
#include <np_common/np_common.h>

/* Community IPC constants */
#define NP_COMMUNITY_WORK_QUEUE_DEPTH		32
#define NP_COMMUNITY_WORK_PAYLOAD_COUNT		3
#define NP_COMMUNITY_HOST_NAME_LEN		128
#define NP_COMMUNITY_KEEPALIVE_TIMEOUT_USEC	7000000u
#define NP_COMMUNITY_SERVER_CACHE_TTL_USEC	60000000ULL
#define NP_COMMUNITY_RATELIMIT_DELAY_USEC	2000000u

/* Community IPC service kinds */
enum NpCommunityServiceKind {
	NP_COMMUNITY_SERVICE_TUS	= 0,
	NP_COMMUNITY_SERVICE_SCORE	= 1,
	NP_COMMUNITY_LIST_REQUEST	= 2
};

/* Community backend transaction types (passed to npCommunityCreateTx) */
enum NpCommunityTxServiceType {
	NP_COMMUNITY_TX_TUS_SERVER_INFO		= 21,
	NP_COMMUNITY_TX_SCORE			= 22,
	NP_COMMUNITY_TX_TUS			= 25
};

/* Savedata bind-mode permission bits (SCE_NP_SCORE_BINDMODE_* / SCE_NP_TUS_BINDMODE_*) */
#define NP_COMMUNITY_BINDMODE_RDONLY		0x0001
#define NP_COMMUNITY_BINDMODE_WRONLY		0x0002

/* BlockingIpc invalid parameter size error (0x80101740) */
#define NP_BLOCKING_IPC_ERR_INVALID_SIZE	-2146429120
#define NP_IPMI_ERR_UNSUPPORTED_METHOD		-2147351103

/* Score IPC sync method IDs (0x10010000..0x10010007) */
#define NP_SCORE_IPC_PING_A			0x10010000u
#define NP_SCORE_IPC_PING_B			0x10010001u
#define NP_SCORE_IPC_NOTIFY_TITLE		0x10010002u
#define NP_SCORE_IPC_SET_TIMEOUT		0x10010003u
#define NP_SCORE_IPC_CHANGE_BIND_MODE		0x10010004u
#define NP_SCORE_IPC_ENQUEUE_WORK		0x10010005u
#define NP_SCORE_IPC_GET_RESULT			0x10010006u
#define NP_SCORE_IPC_GET_RESULT_RANGE		0x10010007u

/* TUS IPC sync method IDs (0x10020000..0x10020006) */
#define NP_TUS_IPC_PING				0x10020000u
#define NP_TUS_IPC_CLOSE_CLIENT			0x10020001u
#define NP_TUS_IPC_NOTIFY_TITLE			0x10020002u
#define NP_TUS_IPC_SET_TIMEOUT			0x10020003u
#define NP_TUS_IPC_CHANGE_BIND_MODE		0x10020004u
#define NP_TUS_IPC_ABORT_BY_ID			0x10020005u
#define NP_TUS_IPC_WORK_OR_RESULT		0x10020006u

/* Score worker opcodes */
enum NpScoreWorkOpCode {
	NP_SCORE_WORK_GET_BOARD_INFO		= 0,
	NP_SCORE_WORK_RECORD_SCORE		= 1,
	NP_SCORE_WORK_RECORD_GAME_DATA		= 2,
	NP_SCORE_WORK_GET_GAME_DATA		= 3,
	NP_SCORE_WORK_GET_RANKING_BY_NPID	= 4,
	NP_SCORE_WORK_GET_RANKING_BY_RANGE	= 5,
	NP_SCORE_WORK_FILTER_COMMENT		= 6,
	NP_SCORE_WORK_SHUTDOWN			= 9
};

/* TUS/TSS worker opcodes */
enum NpTusWorkOpCode {
	NP_TUS_WORK_SHUTDOWN			= 0,
	NP_TUS_WORK_GET_MULTI_USER_VARIABLE	= 1,
	NP_TUS_WORK_GET_MULTI_SLOT_VARIABLE	= 2,
	NP_TUS_WORK_GET_MULTI_USER_DATA_STATUS	= 3,
	NP_TUS_WORK_GET_MULTI_SLOT_DATA_STATUS	= 4,
	NP_TUS_WORK_DELETE_MULTI_SLOT_VARIABLE	= 5,
	NP_TUS_WORK_DELETE_MULTI_SLOT_DATA	= 6,
	NP_TUS_WORK_SET_MULTI_SLOT_VARIABLE	= 7,
	NP_TUS_WORK_ADD_AND_GET_VARIABLE	= 8,
	NP_TUS_WORK_TRY_AND_SET_VARIABLE	= 9,
	NP_TUS_WORK_SET_DATA			= 10,
	NP_TUS_WORK_GET_DATA			= 11,
	NP_TUS_WORK_TSS_GET_DATA		= 12
};

/* HTTP timeout tuple (20 bytes) */
struct NpCommunityTimeoutConfig {
	SceInt32 resolveRetry;			/* +0x00 */
	SceInt32 resolveTimeout;		/* +0x04 */
	SceInt32 connTimeout;			/* +0x08 */
	SceInt32 sendTimeout;			/* +0x0C */
	SceInt32 recvTimeout;			/* +0x10 */
};

/* Worker thread creation descriptor (28 bytes) */
struct NpCommunityWorkerThreadConfig {
	const char *threadName;			/* +0x00 */
	SceKernelThreadEntry entry;		/* +0x04 */
	SceInt32 initPriority;			/* +0x08 */
	SceSize stackSize;			/* +0x0C */
	SceUInt32 attr;				/* +0x10 */
	SceInt32 cpuAffinityMask;		/* +0x14 */
	SceKernelThreadOptParam *optParam;	/* +0x18 */
};

/* Single payload buffer slice inside a queued work item (8 bytes) */
struct NpCommunityWorkSlice {
	void *buf;				/* +0x00 */
	SceSize size;				/* +0x04 */
};

/* 24-byte payload array passed to npCommunityEnqueueWork */
struct NpCommunityWorkPayloads {
	NpCommunityWorkSlice slices[NP_COMMUNITY_WORK_PAYLOAD_COUNT];
};

/* Ring-buffer work item inside NpCommunityClientContext (32 bytes) */
struct NpCommunityWorkItem {
	SceInt32 opCode;			/* +0x00 */
	SceInt32 requestId;			/* +0x04 */
	NpCommunityWorkPayloads payloads;	/* +0x08 (24 bytes) */
};

/* Tracked heap allocation node owned by a request (8 bytes) */
struct NpCommunityAllocNode {
	void *ptr;				/* +0x00 */
	NpCommunityAllocNode *next;		/* +0x04 */
};

/* Result buffer pair attached to a completed request (16 bytes) */
struct NpCommunityResultPacket {
	void *headBuf;				/* +0x00 */
	SceSize headSize;			/* +0x04 */
	void *bodyBuf;				/* +0x08 */
	SceSize bodySize;			/* +0x0C */
};

/* Per-request state node (256 bytes = 0x100) */
struct NpCommunityRequestNode {
	SceInt32 transactionId;			/* +0x00 */
	SceInt32 refCount;			/* +0x04 */
	SceInt32 markedForRemoval;		/* +0x08 */
	NpCommunityRequestNode *next;		/* +0x0C */
	NpCommunityRequestNode *prev;		/* +0x10 */
	SceInt32 serverLookupTxId;		/* +0x14 */
	SceInt64 webApiReqId;			/* +0x18 */
	SceInt32 aborted;			/* +0x20 */
	NpCommunityAllocNode *allocListHead;	/* +0x24 */
	SceUInt32 padBeforeResult;		/* +0x28 */
	NpCommunityResultPacket *resultPacket;	/* +0x2C */
	SceNpId selfNpId;			/* +0x30 (36 bytes) */
	SceNpCommunicationId commId;		/* +0x54 (12 bytes) */
	SceNpCommunicationPassphrase passphrase;/* +0x60 (128 bytes) */
	SceInt32 resultCode;			/* +0xE0 */
	SceInt32 resultReady;			/* +0xE4 */
	NpCommunityTimeoutConfig timeoutConfig;	/* +0xE8 (20 bytes) */
	SceInt32 savedataBindMode;		/* +0xFC */
};

/* Per-client IPC session context (1104 bytes = 0x450) */
struct NpCommunityClientContext {
	SceUID processId;			/* +0x000 */
	SceInt32 refCount;			/* +0x004 */
	SceInt32 markedForRemoval;		/* +0x008 */
	NpCommunityClientContext *next;		/* +0x00C */
	NpCommunityClientContext *prev;		/* +0x010 */
	SceUID eventFlagId;			/* +0x014 */
	SceUID workerThreadId;			/* +0x018 */
	SceUID workSemaId;			/* +0x01C */
	SceKernelLwMutexWork delayMutex;	/* +0x020 (32 bytes) */
	SceUInt32 queueWriteIndex;		/* +0x040 */
	SceUInt32 queueReadIndex;		/* +0x044 */
	NpCommunityWorkItem workQueue[NP_COMMUNITY_WORK_QUEUE_DEPTH]; /* +0x048 (1024 bytes) */
	NpCommunityRequestNode *requestListHead;/* +0x448 */
	SceUInt32 padTail;			/* +0x44C */
};

/* Score server hostname pair (256 bytes = 0x100) */
struct NpScoreServerHosts {
	char viewHost[NP_COMMUNITY_HOST_NAME_LEN];	/* +0x00 */
	char recordHost[NP_COMMUNITY_HOST_NAME_LEN];	/* +0x80 */
};

/* Score HTTP transaction environment (20 bytes) */
struct NpScoreRequestEnv {
	const SceNpCommunicationId *commId;	/* +0x00 */
	const char *hostName;			/* +0x04 */
	SceUInt32 boardId;			/* +0x08 */
	SceUID clientPid;			/* +0x0C */
	SceInt32 httpConnId;			/* +0x10 */
};

/* TUS HTTP transaction environment (32 bytes) */
struct NpTusRequestEnv {
	const SceNpCommunicationId *commId;	/* +0x00 */
	const char *hostName;			/* +0x04 */
	const SceNpId *selfNpId;		/* +0x08 */
	const void *ticketBuf;			/* +0x0C */
	SceSize ticketSize;			/* +0x10 */
	const SceNpCommunicationPassphrase *passphrase; /* +0x14 */
	SceUID clientPid;			/* +0x18 */
	SceInt32 httpConnId;			/* +0x1C */
};

/* TUS server host list node returned by npCommunityGetServerList (16 bytes) */
struct NpCommunityServerNode {
	const char *hostName;			/* +0x00 */
	SceUInt32 padWord;			/* +0x04 */
	SceUInt32 statusCode;			/* +0x08 */
	NpCommunityServerNode *next;		/* +0x0C */
};

/* Score SDK-compatible public data layouts (matching Sony np/np_ranking.h) */
struct NpScoreBoardInfo {
	SceUInt32 rankLimit;
	SceUInt32 updateMode;
	SceUInt32 sortMode;
	SceUInt32 uploadNumLimit;
	SceSize uploadSizeLimit;
};

struct NpScoreComment {
	char utf8Comment[64];
};

struct NpScoreGameInfo {
	SceSize infoSize;
	SceUInt8 padAfterSize[4];
	SceUInt8 bytes[189];
	SceUInt8 padTail[3];
};

struct NpScoreRankData {
	SceNpId npId;				/* +0x00 (36 bytes) */
	SceUInt8 onlineNameBuf[49];		/* +0x24 */
	SceUInt8 padBeforePcId[3];		/* +0x55 */
	SceInt32 pcId;				/* +0x58 */
	SceUInt32 serialRank;			/* +0x5C */
	SceUInt32 rank;				/* +0x60 */
	SceUInt32 highestRank;			/* +0x64 */
	SceInt32 hasGameData;			/* +0x68 */
	SceUInt8 padBeforeScore[4];		/* +0x6C */
	SceInt64 scoreValue;			/* +0x70 */
	SceRtcTick recordDate;			/* +0x78 */
};

struct NpScorePlayerRankData {
	SceInt32 hasData;			/* +0x00 */
	SceUInt8 padBeforeRank[4];		/* +0x04 */
	NpScoreRankData rankData;		/* +0x08 (128 bytes) */
};

struct NpScoreNpIdPcId {
	SceNpId npId;				/* +0x00 (36 bytes) */
	SceInt32 pcId;				/* +0x24 */
	SceUInt8 padTail[4];			/* +0x28 */
};

/* TUS/TSS SDK-compatible public data layouts (matching Sony np/np_tus.h & np_tss.h) */
struct NpTusVariable {
	SceNpId ownerId;			/* +0x00 (36 bytes) */
	SceInt32 hasData;			/* +0x24 */
	SceRtcTick lastChangedDate;		/* +0x28 (8 bytes) */
	SceUInt8 padBeforeAuthor[4];		/* +0x30 */
	SceNpId lastChangedAuthorId;		/* +0x34 (36 bytes) */
	SceInt64 variable;			/* +0x58 (8 bytes) */
	SceInt64 oldVariable;			/* +0x60 (8 bytes) */
	SceUInt8 tailPad[16];			/* +0x68 (16 bytes -> 120 bytes total) */
};

struct NpTusDataInfo {
	SceSize infoSize;			/* +0x00 */
	SceUInt8 padAfterSize[4];		/* +0x04 */
	SceUInt8 bytes[384];			/* +0x08 (total 392 bytes) */
};

struct NpTusDataStatus {
	SceNpId ownerId;			/* +0x00 (36 bytes) */
	SceInt32 hasData;			/* +0x24 */
	SceRtcTick lastChangedDate;		/* +0x28 (8 bytes) */
	SceNpId lastChangedAuthorId;		/* +0x30 (36 bytes) */
	void *dataPtr;				/* +0x54 */
	SceSize dataSize;			/* +0x58 */
	SceUInt8 padBeforeInfo[4];		/* +0x5C */
	NpTusDataInfo info;			/* +0x60 (392 bytes -> 488 bytes total) */
};

struct NpTssIfModifiedSinceParam {
	SceInt32 ifType;			/* +0x00 */
	SceUInt8 padAfterType[4];		/* +0x04 */
	SceRtcTick lastModified;		/* +0x08 */
};

struct NpTssGetDataOptParam {
	SceSize size;				/* +0x00 */
	SceOff *offset;				/* +0x04 */
	SceOff *lastByte;			/* +0x08 */
	NpTssIfModifiedSinceParam *ifParam;	/* +0x0C */
};

struct NpTssDataStatus {
	SceRtcTick lastModified;		/* +0x00 */
	SceInt32 statusCodeType;		/* +0x08 */
	SceSize contentLength;			/* +0x0C */
};

/* NP online display name (52 bytes: 48 UTF-8 chars + NUL + 3 alignment bytes) */
struct SceNpOnlineName {
	char data[48];
	char term;
	char padding[3];
};

/* Backend community client (168 bytes = 0xA8, SceNpCommunityClient) */
struct NpCommunityBackendClient {
	SceInt32 clientId;			/* +0x00 */
	SceInt32 refCount;			/* +0x04 */
	SceInt32 markedForRemoval;		/* +0x08 */
	SceNpOnlineId onlineId;			/* +0x0C (20 bytes) */
	SceNpId npId;				/* +0x20 (36 bytes) */
	SceNpOnlineName onlineName;		/* +0x44 (52 bytes) */
	SceInt32 httpTemplateId;		/* +0x78 */
	SceUID clientMutexId;			/* +0x7C */
	char envName[16];			/* +0x80 ("np" for product env) */
	char countryCode[4];			/* +0x90 (2-letter ISO code + NUL) */
	SceUInt32 reservedWords[2];		/* +0x94..+0x9B */
	NpCommunityAllocNode *allocListHead;	/* +0x9C */
	NpCommunityBackendClient *next;		/* +0xA0 */
	NpCommunityBackendClient *prev;		/* +0xA4 */
};

/* Backend community HTTP transaction (84 bytes = 0x54, SceNpCommunityTransaction) */
struct NpCommunityBackendTx {
	SceInt32 txId;				/* +0x00 */
	SceInt32 refCount;			/* +0x04 */
	SceInt32 markedForRemoval;		/* +0x08 */
	SceUInt32 reservedState;		/* +0x0C */
	SceInt32 httpConnId;			/* +0x10 */
	SceInt32 httpReqId;			/* +0x14 */
	SceInt32 ownerKind;			/* +0x18 (1 = profile, 2 = community) */
	SceInt32 aborted;			/* +0x1C */
	NpCommunityAllocNode *allocListHead;	/* +0x20 */
	NpCommunityBackendClient *client;	/* +0x24 */
	NpCommunityBackendTx *next;		/* +0x28 */
	NpCommunityBackendTx *prev;		/* +0x2C */
	SceUInt32 reservedFlags;		/* +0x30 */
	SceInt32 serviceType;			/* +0x34 */
	void *serviceParam;			/* +0x38 */
	NpCommunityTimeoutConfig timeoutConfig;	/* +0x3C (20 bytes) */
	SceUInt32 tailReserved;			/* +0x50 */
};

/* Shared community IPC manager (np_community_ipc_common.cpp, 0x81323B34..0x8132475B) */
int npCommunityIpcInit(int serviceKind);
int npCommunityReleaseRequest(NpCommunityRequestNode *req);
int npCommunityFindEntry(void **outEntry, SceInt32 key, int listKind,
			 NpCommunityClientContext *clientCtx, int removeFlag);
int npCommunityReleaseClient(NpCommunityClientContext *client);
int npCommunityLinkEntry(void *entry, int listKind, NpCommunityClientContext *clientCtx);
int npCommunityLookupClient(SceUID processId, int serviceKind,
			    NpCommunityClientContext **outClient);
int npCommunityLookupRequest(NpCommunityRequestNode **outReq,
			     NpCommunityClientContext *clientCtx, SceInt32 reqId);
int npCommunityTrackRequestAlloc(NpCommunityRequestNode *req, void *allocPtr);
int npCommunityCreateClientSession(int serviceKind, SceUID processId,
				   const NpCommunityWorkerThreadConfig *threadCfg,
				   NpCommunityClientContext **outClient);
int npCommunityEnqueueWork(SceInt32 opCode, NpCommunityClientContext *clientCtx,
			   SceInt32 requestId, const NpCommunityWorkPayloads *payloads);
int npCommunityCloseClientSession(SceUID processId, int serviceKind);
int npCommunityCreateRequest(NpCommunityClientContext *clientCtx, int txServiceType,
			     SceInt32 bindMode, const SceNpId *selfNpId,
			     const SceNpCommunicationId *commId,
			     const SceNpCommunicationPassphrase *passphrase,
			     const NpCommunityTimeoutConfig *timeoutCfg);
int npCommunityDestroyRequest(NpCommunityClientContext *clientCtx, SceInt32 requestId);
int npCommunityCheckNotAborted(NpCommunityRequestNode *req);
int npCommunityAbortRequest(NpCommunityClientContext *clientCtx, SceInt32 requestId);
int npCommunityChangeBindMode(NpCommunityClientContext *clientCtx, SceInt32 requestId,
			      SceInt32 bindMode);
int npCommunityReadResult(NpCommunityClientContext *clientCtx, SceInt32 requestId,
			  SceSize headMaxSize, SceSize *outHeadSize, void *headDst,
			  SceUInt32 headOffset, SceSize bodyMaxSize, SceSize *outBodySize,
			  void *bodyDst, SceUInt32 bodyOffset, int clearOnRead);
int npCommunityFetchTicket(void **outTicketBuf, SceSize *outTicketSize);
void npCommunityLogServiceNotConfigured(const char *tag, const SceNpCommunicationId *commId);

/* Score & TUS server entrypoints (0x8131F65C, 0x8132117C) */
int npScoreIpcServerStart(void);
int npTusIpcServerStart(void);
int npCommunitySetTimeout(void *session, SceInt32 requestId,
			  const NpCommunityTimeoutConfig *timeoutCfg);

/* External NP community/account/shell bridges */
extern int npCommunityNoop(void);							/* 0x81409E2C */
extern void *npCommunityAlloc(SceSize size);						/* 0x8132DA00 */
extern void npCommunityFree(void *ptr);							/* 0x8132DA08 */
extern int npCommunityCreateTx(const SceNpId *selfNpId, int serviceType,
			       const SceNpCommunicationId *commId);			/* 0x8132A2BA */
extern int npCommunityAbortTx(SceInt32 txId);						/* 0x8132A322 */
extern int npCommunityDestroyTx(SceInt32 txId);						/* 0x8132A382 */
extern int npCommunitySetTxTimeout(SceInt32 txId,
				   const NpCommunityTimeoutConfig *timeoutCfg);		/* 0x8132A38C */
extern int npCommunityGetTxSessionKey(SceInt32 txId, void *outKeyBuf);			/* 0x8132A3E2 */
extern int npCommunityReleaseBackendTx(NpCommunityBackendTx *tx);			/* 0x8132A9BC */
extern int npCommunityAcquireBackendTx(SceInt32 txId, NpCommunityBackendTx **outTx);	/* 0x8132AA32 */
extern int npCommunityGetTxHttpTemplate(SceInt32 txId);					/* 0x8132AE28 */

/* Score HTTP backend operations (0x813304C4..0x81331FF2, 0x81338898) */
extern int npScoreBackendGetServerInfo(SceInt32 txId, const SceNpCommunicationId *commId,
				       NpScoreServerHosts *outHosts);			/* 0x81338898 */
extern int npScoreBackendGetBoardInfo(SceInt32 txId, const NpScoreRequestEnv *env,
				      const SceNpCommunicationPassphrase *passphrase,
				      NpScoreBoardInfo *outBoardInfo, void *option);	/* 0x813304C4 */
extern int npScoreBackendRecordScore(SceInt32 txId, const NpScoreRequestEnv *env,
				     const SceNpId *selfNpId, SceInt32 pcId,
				     const void *sessionKey,
				     const SceNpCommunicationPassphrase *passphrase,
				     const void *ticketBuf, SceSize ticketSize,
				     SceInt64 scoreValue,
				     const NpScoreComment *comment,
				     const NpScoreGameInfo *gameInfo,
				     SceUInt32 *outTmpRank,
				     const SceRtcTick *compareDate, void *option);	/* 0x813306DE */
extern int npScoreBackendRecordGameData(SceInt32 txId, const NpScoreRequestEnv *env,
					const SceNpId *selfNpId, SceInt32 pcId,
					const void *ticketBuf, SceSize ticketSize,
					SceInt64 scoreValue, SceSize totalSize,
					SceSize sendSize, const void *chunkData,
					void *option);					/* 0x8133090A */
extern int npScoreBackendGetGameData(SceInt32 txId, const NpScoreRequestEnv *env,
				     SceInt32 pcId,
				     const SceNpCommunicationPassphrase *passphrase,
				     const SceNpId *targetNpId, SceSize *outTotalSize,
				     SceSize recvSize, void *outData,
				     SceSize offsetHint, void *option);			/* 0x81330BFE */
extern int npScoreBackendGetRankingByNpId(SceInt32 txId, const NpScoreRequestEnv *env,
					  const SceNpCommunicationPassphrase *passphrase,
					  const NpScoreNpIdPcId *targets,
					  NpScorePlayerRankData *outRanks,
					  void *unusedFilter, void *unusedExtra,
					  NpScoreComment *outComments,
					  NpScoreGameInfo *outGameInfos,
					  SceSize arrayNum, SceRtcTick *outSortDate,
					  SceUInt32 *outTotalRecord, void *option);	/* 0x81330EAC */
extern int npScoreBackendGetFriendsRanking(SceInt32 txId, const NpScoreRequestEnv *env,
					   const SceNpId *selfNpId, SceInt32 pcId,
					   const SceNpCommunicationPassphrase *passphrase,
					   const void *ticketBuf, SceSize ticketSize,
					   SceUInt32 startSerialRank,
					   NpScoreRankData *outRanks,
					   NpScoreComment *outComments,
					   NpScoreGameInfo *outGameInfos,
					   SceSize arrayNum, SceRtcTick *outSortDate,
					   SceUInt32 *outTotalRecord, SceUInt32 *outHits,
					   SceInt32 includeSelf, void *option);		/* 0x813314B2 */
extern int npScoreBackendGetRankingByRange(SceInt32 txId, const NpScoreRequestEnv *env,
					   const SceNpCommunicationPassphrase *passphrase,
					   SceUInt32 startSerialRank,
					   NpScoreRankData *outRanks, void *unusedArg,
					   SceInt32 friendsScope,
					   NpScoreComment *outComments,
					   NpScoreGameInfo *outGameInfos,
					   SceSize arrayNum, SceRtcTick *outSortDate,
					   SceUInt32 *outTotalRecord, void *option);	/* 0x81331A7C */
extern int npScoreBackendFilterComment(SceInt32 txId, const NpScoreRequestEnv *env,
				       const SceNpId *selfNpId,
				       const SceNpCommunicationPassphrase *passphrase,
				       const char *inComment, char *outComment,
				       SceInt32 isSanitize);				/* 0x81331FF2 */

/* TUS/TSS HTTP backend operations (0x81333A74..0x8133888E) */
extern int npTusBackendGetServerList(SceInt32 txId, NpCommunityServerNode **outList);	/* 0x8133888E */
extern int npTusBackendSetMultiSlotVariable(SceInt32 txId, const NpTusRequestEnv *env,
					    const SceNpId *targetNpId, SceInt32 isVirtualUser,
					    const SceInt32 *slotIds, const SceInt64 *variables,
					    SceInt32 arrayNum, void *option);		/* 0x81333A74 */
extern int npTusBackendAddAndGetVariable(SceInt32 txId, const NpTusRequestEnv *env,
					 const SceNpId *targetNpId, SceInt32 isVirtualUser,
					 SceInt32 slotId, SceInt64 inVariable,
					 const SceNpId *isLastChangedAuthor,
					 const SceRtcTick *isLastChangedDate,
					 NpTusVariable *outVariable, void *option);	/* 0x81333BDC */
extern int npTusBackendGetMultiUserVariable(SceInt32 txId, const NpTusRequestEnv *env,
					    const SceNpId *targetNpIds, SceInt32 userQueryMode,
					    SceInt32 slotId, NpTusVariable *outVars,
					    SceInt32 arrayNum, void *option);		/* 0x81333ECE */
extern int npTusBackendGetFriendsVariable(SceInt32 txId, const NpTusRequestEnv *env,
					  SceInt32 slotId, SceInt32 includeSelf,
					  SceUInt32 startSerialRank, SceInt32 sortType,
					  NpTusVariable *outVars, SceInt32 arrayNum,
					  SceUInt32 *outHits, void *option);		/* 0x813342F6 */
extern int npTusBackendGetMultiSlotVariable(SceInt32 txId, const NpTusRequestEnv *env,
					    const SceNpId *targetNpId, SceInt32 isVirtualUser,
					    const SceInt32 *slotIds, NpTusVariable *outVars,
					    SceInt32 arrayNum, void *option);		/* 0x813346D8 */
extern int npTusBackendGetMultiUserDataStatus(SceInt32 txId, const NpTusRequestEnv *env,
					      const SceNpId *targetNpIds, SceInt32 userQueryMode,
					      SceInt32 slotId, NpTusDataStatus *outStatuses,
					      SceInt32 arrayNum, void *option);		/* 0x81334B16 */
extern int npTusBackendGetFriendsDataStatus(SceInt32 txId, const NpTusRequestEnv *env,
					    SceInt32 slotId, SceInt32 includeSelf,
					    SceUInt32 startSerialRank, SceInt32 sortType,
					    NpTusDataStatus *outStatuses, SceInt32 arrayNum,
					    SceUInt32 *outHits, void *option);		/* 0x81334F6A */
extern int npTusBackendGetMultiSlotDataStatus(SceInt32 txId, const NpTusRequestEnv *env,
					      const SceNpId *targetNpId, SceInt32 isVirtualUser,
					      const SceInt32 *slotIds,
					      NpTusDataStatus *outStatuses,
					      SceInt32 arrayNum, void *option);		/* 0x8133535C */
extern int npTusBackendTryAndSetVariable(SceInt32 txId, const NpTusRequestEnv *env,
					 const SceNpId *targetNpId, SceInt32 isVirtualUser,
					 SceInt32 slotId, SceInt64 inVariable,
					 const SceInt64 *compareValue,
					 const SceNpId *isLastChangedAuthor,
					 const SceRtcTick *isLastChangedDate,
					 NpTusVariable *outVariable, SceInt32 opeType,
					 void *option);					/* 0x81335790 */
extern int npTusBackendSetData(SceInt32 txId, const NpTusRequestEnv *env,
			       const SceNpId *targetNpId, SceInt32 isVirtualUser,
			       SceInt32 slotId, SceSize totalSize, const void *chunkData,
			       SceSize sendSize, const NpTusDataInfo *dataInfo,
			       const SceNpId *isLastChangedAuthor,
			       const SceRtcTick *isLastChangedDate, void *option);	/* 0x81335A9C */
extern int npTusBackendGetData(SceInt32 txId, const NpTusRequestEnv *env,
			       const SceNpId *targetNpId, SceInt32 isVirtualUser,
			       SceInt32 slotId, NpTusDataStatus *outStatus,
			       void *outData, SceSize recvSize, void *option);		/* 0x81335D5E */
extern int npTusBackendDeleteMultiSlotVariable(SceInt32 txId, const NpTusRequestEnv *env,
					       const SceNpId *targetNpId, SceInt32 isVirtualUser,
					       const SceInt32 *slotIds, SceInt32 arrayNum,
					       void *option);				/* 0x813361F6 */
extern int npTusBackendDeleteMultiSlotData(SceInt32 txId, const NpTusRequestEnv *env,
					   const SceNpId *targetNpId, SceInt32 isVirtualUser,
					   const SceInt32 *slotIds, SceInt32 arrayNum,
					   void *option);				/* 0x8133635E */
extern int npTssBackendGetData(SceInt32 txId, const SceNpCommunicationId *commId,
			       SceInt32 slotId, NpTssDataStatus *outStatus, void *outData,
			       SceSize maxSize, SceSize recvSize, SceInt32 isSmallStorage,
			       const NpTssGetDataOptParam *option);			/* 0x81337E78 */

/* Account ticket fetch & shell netctl/http/appmgr imports */
extern int npAccountCreateRequest(void);						/* 0x813268BC */
extern int npAccountDestroyRequest(int reqHandle);					/* 0x813268DC */
extern int npAccountGetTicket(int reqHandle, void *ticketBuf, SceSize *ioSize);		/* 0x81326906 */

extern "C" {
extern int SceShell_6B8772A2(SceUID pid);
extern int SceShell_794BF5BE(unsigned int token);
extern int sceAppMgrAppParamGetInt(SceUID pid, int paramId, int *outVal);
extern int sceHttpCreateConnection(SceInt32 tmplId, const char *serverName, const char *scheme,
				   SceUInt16 port, SceBool enableKeepalive);
extern int sceHttpDeleteConnection(SceInt32 connId);
extern int sceNpWebApiAbortRequest(SceInt64 requestId);
extern void SceNpManager_9554802B(const void *tag, const void *detail, const void *noticeName);
}

#endif /* SHELL_NP_COMMUNITY_IPC_H */
