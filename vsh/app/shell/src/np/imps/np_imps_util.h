/*
 * np_imps_util.h: SceNpImpsUtil internal declarations (0x812F0510..0x812F899B,
 *                 0x81438ECC..0x8143D91F).
 *
 * SceNpImpsUtil exposes the "SceNpImpsUtil" IPMI server used by games and system
 * apps to interact with SceNpImpsSvc, SceNpImpsCore, and NpFriendStorage:
 *   - manages game and system IPMI client session holders with per-event subscription
 *     masks and queue overflow recovery strategies,
 *   - tracks per-PID process lifecycle and online state via a hierarchical state
 *     machine thread ("SceNpImpsUtilMPState"),
 *   - relays async roster operations and connect requests through AsyncCallRelayMgr
 *     while holding per-PID network lock tokens,
 *   - provides raw-message channel multiplexing for SceNpParty / SceNpIpc,
 *   - exposes the C facade over SceNpImpsSvc (0x812F7FD0..0x812F899B).
 */
#ifndef SHELL_NP_IMPS_UTIL_H
#define SHELL_NP_IMPS_UTIL_H

#include <stddef.h>
#include <scetypes.h>
#include <np/np_common.h>
#include <np/np_basic.h>
#include <ipmi_wrapper/ipmi_wrapper_server.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include <datetime/datetime.h>
#include <thread/thread.h>
#include "np_imps_svc.h"
#include "../ipc/np_ipc_internal.h"

/* SceNpImpsUtil error codes (0x80551600 / 0x80551700 range) */
#define NP_IMPS_UTIL_ERROR_INTERNAL		-2141905919	/* 0x80551601 */
#define NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY	-2141905918	/* 0x80551602 */
#define NP_IMPS_UTIL_ERROR_NOT_INITIALIZED	-2141905917	/* 0x80551603 */
#define NP_IMPS_UTIL_ERROR_ALREADY_INITIALIZED	-2141905916	/* 0x80551604 */
#define NP_IMPS_UTIL_ERROR_ALREADY_IN_PROGRESS	-2141905915	/* 0x80551605 */
#define NP_IMPS_UTIL_ERROR_CHANNEL_REGISTERED	-2141905913	/* 0x80551607 */
#define NP_IMPS_UTIL_ERROR_CHANNEL_NOT_FOUND	-2141905912	/* 0x80551608 */
#define NP_IMPS_UTIL_ERROR_NOT_ONLINE		-2141905911	/* 0x80551609 */
#define NP_IMPS_UTIL_ERROR_INVALID_SIZE		-2141905910	/* 0x8055160A */

#define NP_IMPS_UTIL_ERROR_STATE_INVALID	-2141905663	/* 0x80551701 */
#define NP_IMPS_UTIL_ERROR_SIGNED_OUT		-2141905658	/* 0x80551706 */
#define NP_IMPS_UTIL_ERROR_OFFLINE		-2141905657	/* 0x80551707 */
#define NP_IMPS_UTIL_ERROR_QUEUE_OVERFLOW	-2141905652	/* 0x8055170C */

#define NP_IMPS_IPMI_ERROR_QUEUE_FULL		-2147351295	/* 0x80020501 */
#define NP_IMPS_IPMI_ERROR_INVALID_METHOD	-2147351103	/* 0x800205C1 */

/* Number of event subscription channels per client session */
#define NP_IMPS_UTIL_EVENT_COUNT		14

/* Event subscription kind indices (matches NpImpsUtilityIpcMessage::kind) */
enum NpImpsUtilEventKind {
	NP_IMPS_EVT_SERVICE_STATE		= 0,
	NP_IMPS_EVT_MY_ONLINE_STATUS		= 1,
	NP_IMPS_EVT_FRIEND_LIST			= 2,
	NP_IMPS_EVT_FRIEND_ONLINE_STATUS	= 3,
	NP_IMPS_EVT_FRIEND_GAME_PRESENCE	= 4,
	NP_IMPS_EVT_FRIEND_PARTY_PRESENCE	= 5,
	NP_IMPS_EVT_IN_GAME_DATA_MESSAGE	= 6,
	NP_IMPS_EVT_FRIEND_GAME_PRESENCE_DETAIL	= 7,
	NP_IMPS_EVT_BLOCK_LIST			= 8,
	NP_IMPS_EVT_MY_GAME_PRESENCE_DETAIL	= 9,
	NP_IMPS_EVT_FRIEND_COMM_ID		= 10,
	NP_IMPS_EVT_PLATFORM_FRIEND_PRESENCE	= 11,
	NP_IMPS_EVT_MY_PARTY_PRESENCE		= 12,
	NP_IMPS_EVT_FRIEND_REQUEST		= 13
};

/* IPMI method IDs (0x90000..0x9003E) */
enum NpImpsUtilIpmiMethod {
	NP_IMPS_METHOD_REGISTER_GAME_EVENT	= 0x90000,
	NP_IMPS_METHOD_UNREGISTER_GAME_EVENT	= 0x90001,
	NP_IMPS_METHOD_GET_SERVICE_STATE	= 0x90002,
	NP_IMPS_METHOD_ENTER_ONLINE_ASYNC	= 0x90003,
	NP_IMPS_METHOD_ENTER_ONLINE_RESULT	= 0x90004,
	NP_IMPS_METHOD_LEAVE_ONLINE		= 0x90005,
	NP_IMPS_METHOD_CONNECT_CORE		= 0x90006,
	NP_IMPS_METHOD_GET_ACCEPTED_FRIEND_CNT	= 0x90007,
	NP_IMPS_METHOD_GET_ACCEPTED_FRIEND_LIST	= 0x90008,
	NP_IMPS_METHOD_GET_FRIEND_PRESENCE_GAME	= 0x90009,
	NP_IMPS_METHOD_GET_BLOCK_LIST_COUNT	= 0x9000A,
	NP_IMPS_METHOD_GET_BLOCK_LIST_ENTRIES	= 0x9000B,
	NP_IMPS_METHOD_IS_BLOCKED		= 0x9000C,
	NP_IMPS_METHOD_SET_PARTY_PRESENCE	= 0x9000D,
	NP_IMPS_METHOD_CLEAR_PARTY_PRESENCE	= 0x9000E,
	NP_IMPS_METHOD_SET_COMM_ID		= 0x9000F,
	NP_IMPS_METHOD_CLEAR_COMM_ID		= 0x90010,
	NP_IMPS_METHOD_SET_GAME_PRESENCE	= 0x90011,
	NP_IMPS_METHOD_CLEAR_GAME_PRESENCE	= 0x90012,
	NP_IMPS_METHOD_SEND_IN_GAME_DATA	= 0x90013,
	NP_IMPS_METHOD_ADD_PLAY_LOG		= 0x90014,
	NP_IMPS_METHOD_GET_PLAY_LOG_COUNT	= 0x90015,
	NP_IMPS_METHOD_GET_PLAY_LOG_LIST	= 0x90016,
	NP_IMPS_METHOD_CREATE_ROSTER_CTX	= 0x90017,
	NP_IMPS_METHOD_DESTROY_ROSTER_CTX	= 0x90018,
	NP_IMPS_METHOD_ABORT_ROSTER_CTX		= 0x90019,
	NP_IMPS_METHOD_WAIT_ROSTER_CTX		= 0x9001A,
	NP_IMPS_METHOD_REQUEST_FRIEND_ASYNC	= 0x9001B,
	NP_IMPS_METHOD_REQUEST_FRIEND_RESULT	= 0x9001C,
	NP_IMPS_METHOD_ACCEPT_FRIEND_ASYNC	= 0x9001D,
	NP_IMPS_METHOD_ACCEPT_FRIEND_RESULT	= 0x9001E,
	NP_IMPS_METHOD_DELETE_FRIEND_ASYNC	= 0x9001F,
	NP_IMPS_METHOD_DELETE_FRIEND_RESULT	= 0x90020,
	NP_IMPS_METHOD_ADD_BLOCK_ASYNC		= 0x90021,
	NP_IMPS_METHOD_ADD_BLOCK_RESULT		= 0x90022,
	NP_IMPS_METHOD_DELETE_BLOCK_ASYNC	= 0x90023,
	NP_IMPS_METHOD_DELETE_BLOCK_RESULT	= 0x90024,
	NP_IMPS_METHOD_GET_ALL_FRIEND_COUNT	= 0x90025,
	NP_IMPS_METHOD_GET_ALL_FRIEND_LIST	= 0x90026,
	NP_IMPS_METHOD_IS_ONLINE		= 0x90027,
	NP_IMPS_METHOD_GET_FRIEND_CONTEXT_SYS	= 0x90028,
	NP_IMPS_METHOD_GET_FRIEND_PRESENCE_SYS	= 0x90029,
	NP_IMPS_METHOD_DELETE_PLAY_LOG		= 0x9002A,
	NP_IMPS_METHOD_DELETE_ALL_PLAY_LOG	= 0x9002B,
	NP_IMPS_METHOD_GET_FRIEND_CONTEXT_GAME	= 0x9002C,
	NP_IMPS_METHOD_GET_FRIEND_GAME_PRESENCE	= 0x9002D,
	NP_IMPS_METHOD_REGISTER_SYS_EVENT	= 0x9002E,
	NP_IMPS_METHOD_UNREGISTER_SYS_EVENT	= 0x9002F,
	NP_IMPS_METHOD_DISCONNECT_CORE		= 0x90030,
	NP_IMPS_METHOD_GET_PENDING_FRIEND_CNT	= 0x90031,
	NP_IMPS_METHOD_GET_PENDING_FRIEND_LIST	= 0x90032,
	NP_IMPS_METHOD_GET_REQ_FRIEND_CNT	= 0x90033,
	NP_IMPS_METHOD_GET_REQ_FRIEND_LIST	= 0x90034,
	NP_IMPS_METHOD_RAW_MSG_OPEN		= 0x90035,
	NP_IMPS_METHOD_RAW_MSG_CLOSE		= 0x90036,
	NP_IMPS_METHOD_RAW_MSG_SEND		= 0x90037,
	NP_IMPS_METHOD_GET_RAW_SERVICE_STATE	= 0x90038,
	NP_IMPS_METHOD_IS_GLOBAL_ONLINE		= 0x90039,
	NP_IMPS_METHOD_GET_FRIEND_PRESENCE_ALT	= 0x9003A,
	NP_IMPS_METHOD_FORCE_ROSTER_SYNC	= 0x9003B,
	NP_IMPS_METHOD_GET_MY_STATUS_PRESENCE	= 0x9003C,
	NP_IMPS_METHOD_GET_PLAYER_PLAY_LOG_CNT	= 0x9003D,
	NP_IMPS_METHOD_GET_PLAYER_PLAY_LOG	= 0x9003E
};

/* MPState HSM event IDs */
enum NpImpsMpStateEventId {
	NP_IMPS_MP_EVT_PROCESS_TERMINATE	= 1,
	NP_IMPS_MP_EVT_SIGN_OUT			= 2,
	NP_IMPS_MP_EVT_SIGN_IN			= 3,
	NP_IMPS_MP_EVT_CORE_ONLINE		= 4,
	NP_IMPS_MP_EVT_ENTER_ONLINE_REQ		= 5,
	NP_IMPS_MP_EVT_ENTER_ONLINE_FAIL	= 6,
	NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW	= 7,
	NP_IMPS_MP_EVT_IPC_QUEUE_RECOVERED	= 8,
	NP_IMPS_MP_EVT_SET_COMM_ID		= 9,
	NP_IMPS_MP_EVT_CLEAR_COMM_ID		= 10,
	NP_IMPS_MP_EVT_LEAVE_ONLINE_REQ		= 11
};

/* Detailed game presence wire payload (552 bytes = 0x228) */
struct NpImpsGamePresenceDetail {
	SceSize size;					/* +0x000 (552) */
	SceNpCommunicationId titleId;			/* +0x004 */
	char contentId[48];				/* +0x010 */
	SceNpCommunicationId commId;			/* +0x040 */
	SceNpCommunicationId joinableCommId;		/* +0x04C */
	NpImpsGamePresenceWithTitle gamePresence;	/* +0x058 (464 bytes) */
};

/* Async request relay record (12 bytes) */
struct NpImpsAsyncRelayRecord {
	IPMI::Session *session;
	SceUInt32 requestId;
	int netLockToken;
};

/* Heap helpers for SceNpImpsUtil (0x812F4BF4..0x812F4C43) */
extern void npImpsUtilHeapInit(paf::memory::HeapAllocator *heap = NULL);
extern void npImpsUtilHeapTerm(void);
extern void *npImpsUtilMalloc(SceSize size);
extern void npImpsUtilFree(void *ptr);

/* Queue overflow recovery strategy interface */
class NpImpsQueueOverflowStrategy {
public:
	virtual ~NpImpsQueueOverflowStrategy() {}
	virtual int HandleOverflow(IPMI::Session *session, SceUInt32 queueIndex,
				   const void *msgBuf, SceSize msgSize) = 0;
};

/* Per-session event subscription and message dispatch manager (0x812F1810..0x812F3737) */
class NpImpsUtilSessionHolder {
public:
	NpImpsUtilSessionHolder(NpImpsQueueOverflowStrategy *strategy);
	virtual ~NpImpsUtilSessionHolder();

	int RegisterSession(IPMI::Session *session, const SceUInt8 mask[NP_IMPS_UTIL_EVENT_COUNT]);
	int UnregisterSession(IPMI::Session *session);
	int ClearQueuesForProcess(SceUID processId);
	bool IsSubscribed(IPMI::Session *session, SceUInt32 eventKind);

	void SendServiceStateToSession(IPMI::Session *session, int state, int errorCode);
	void SendServiceStateToProcess(SceUID processId, int state, int errorCode);

	void SendMyOnlineStatusToSession(IPMI::Session *session, int status);
	void BroadcastMyOnlineStatus(int status);
	void SendMyOnlineStatusToProcess(SceUID processId, int status);

	void SendFriendListToSession(IPMI::Session *session, int action, const SceNpId *npId);
	void BroadcastFriendList(int action, const SceNpId *npId);
	void SendFriendListToProcess(SceUID processId, int action, const SceNpId *npId);

	void SendFriendContextToSession(IPMI::Session *session, int context, const SceNpId *npId);
	void BroadcastFriendContext(int context, const SceNpId *npId);
	void SendFriendContextToProcess(SceUID processId, int context, const SceNpId *npId);

	void SendFriendCommIdToSession(IPMI::Session *session, const SceNpId *npId,
				       const SceNpCommunicationId *commId);
	void SendFriendCommIdToProcess(SceUID processId, const SceNpId *npId,
				       const SceNpCommunicationId *commId);

	void SendBlockListToSession(IPMI::Session *session, int action, const SceNpId *npId);
	void SendBlockListToProcess(SceUID processId, int action, const SceNpId *npId);

	void SendInGameDataToProcess(SceUID processId, const SceNpId *senderNpId,
				     const NpImpsSvcInGameDataPayload *payload);

	void SendFriendGamePresenceToSession(IPMI::Session *session, const SceNpId *npId,
					     const SceNpCommunicationId *curCommId,
					     const SceNpCommunicationId *prevCommId,
					     int isInitial,
					     const NpImpsGamePresenceWithTitle *presence);
	void SendFriendGamePresenceToProcess(SceUID processId, const SceNpId *npId,
					     const SceNpCommunicationId *curCommId,
					     const SceNpCommunicationId *prevCommId,
					     int isInitial,
					     const NpImpsGamePresenceWithTitle *presence);

	void SendFriendPartyPresenceToSession(IPMI::Session *session, const SceNpId *npId,
					      const NpImpsPartyPresence *presence);
	void BroadcastFriendPartyPresence(const SceNpId *npId,
					  const NpImpsPartyPresence *presence);
	void SendFriendPartyPresenceToProcess(SceUID processId, const SceNpId *npId,
					      const NpImpsPartyPresence *presence);

	void SendFriendPresenceDetailToSession(IPMI::Session *session, const SceNpId *npId,
					       const NpImpsGamePresenceDetail *detail);
	void SendFriendPresenceDetailToProcess(SceUID processId, const SceNpId *npId,
					       const NpImpsGamePresenceDetail *detail);

	void SendMyPresenceDetailToSession(IPMI::Session *session,
					   const NpImpsGamePresenceDetail *detail);
	void BroadcastMyPresenceDetail(const NpImpsGamePresenceDetail *detail);
	void SendMyPresenceDetailToProcess(SceUID processId,
					   const NpImpsGamePresenceDetail *detail);

	void SendPlatformFriendPresenceToSession(IPMI::Session *session, const SceNpId *npId,
						 const NpImpsGamePresenceDetail *detail,
						 int platform);
	void BroadcastPlatformFriendPresence(const SceNpId *npId,
					     const NpImpsGamePresenceDetail *detail, int platform);
	void SendPlatformFriendPresenceToProcess(SceUID processId, const SceNpId *npId,
						 const NpImpsGamePresenceDetail *detail,
						 int platform);

	void SendMyPartyPresenceToSession(IPMI::Session *session,
					  const NpImpsPartyPresence *presence);
	void BroadcastMyPartyPresence(const NpImpsPartyPresence *presence);
	void SendMyPartyPresenceToProcess(SceUID processId, const NpImpsPartyPresence *presence);

	void SendFriendRequestToSession(IPMI::Session *session, int action, const SceNpId *npId);
	void SendFriendRequestToProcess(SceUID processId, int action, const SceNpId *npId);

private:
	struct SessionMaskNode {
		SessionMaskNode *next;
		IPMI::Session *session;
		SceUInt8 mask[NP_IMPS_UTIL_EVENT_COUNT];
	};

	void TrySendToSessionLocked(IPMI::Session *session, SceUInt32 queueIndex,
				    SceUInt32 eventKind, const void *msgBuf, SceSize msgSize);
	void BroadcastLocked(SceUInt32 queueIndex, SceUInt32 eventKind,
			     const void *msgBuf, SceSize msgSize);
	void SendToProcessLocked(SceUID processId, SceUInt32 queueIndex, SceUInt32 eventKind,
				 const void *msgBuf, SceSize msgSize);

	paf::thread::RMutex m_mutex;
	SessionMaskNode *m_head;
	NpImpsQueueOverflowStrategy *m_overflowStrategy;
};

/* Async call relay manager (0x812F4124..0x812F4BF3) */
class NpImpsUtilAsyncRelayMgr {
public:
	NpImpsUtilAsyncRelayMgr();
	virtual ~NpImpsUtilAsyncRelayMgr();

	static int Create();
	static int Destroy();
	static NpImpsUtilAsyncRelayMgr *GetInstance();

	void Lock();
	void Unlock();

	void CleanupProcessContexts(SceUID processId);
	int AddProcessContext(SceUID processId, SceUInt32 contextId);
	int RemoveProcessContext(SceUID processId, SceUInt32 contextId);

	int RegisterContextRelay(SceUInt32 contextId, const NpImpsAsyncRelayRecord *record);
	int UnregisterContextRelay(SceUInt32 contextId);
	int LookupContextRelay(SceUInt32 contextId, NpImpsAsyncRelayRecord *outRecord);

	int RegisterProcessOpRelay(SceUID processId, SceUIntPtr opKey,
				   const NpImpsAsyncRelayRecord *record);
	int UnregisterProcessOpRelay(SceUID processId, SceUIntPtr opKey);
	int LookupProcessOpRelay(SceUID processId, SceUIntPtr opKey,
				 NpImpsAsyncRelayRecord *outRecord);

private:
	struct ProcessContextNode {
		ProcessContextNode *next;
		SceUID processId;
		SceUInt32 contextId;
	};

	struct ContextRelayNode {
		ContextRelayNode *next;
		SceUInt32 contextId;
		NpImpsAsyncRelayRecord record;
	};

	struct ProcessOpRelayNode {
		ProcessOpRelayNode *next;
		SceUID processId;
		SceUIntPtr opKey;
		NpImpsAsyncRelayRecord record;
	};

	paf::thread::RMutex m_mutex;
	ProcessContextNode *m_procContexts;
	ContextRelayNode *m_ctxRelays;
	ProcessOpRelayNode *m_procOpRelays;
};

/* Forward declaration for MPState process tracker */
class NpImpsUtilMpState;

/* Main SceNpImpsUtil server context (0x812F0510..0x812F1121) */
struct NpImpsUtilServerContext {
	NpImpsUtilSessionHolder *gameSessions;
	NpImpsUtilSessionHolder *systemSessions;
	SceUInt32 svcListenerId;
	IpmiWrapper::Server *ipmiServer;
	NpImpsUtilMpState *mpState;
};

/* Global accessors used between np_imps_util_server.cpp and np_imps_util_mpstate.cpp */
extern NpImpsUtilServerContext *npImpsUtilGetServerContext(void);
extern NpImpsUtilSessionHolder *npImpsUtilGetGameSessionHolder(void);
extern NpImpsUtilSessionHolder *npImpsUtilGetSystemSessionHolder(void);
extern IpmiWrapper::Server *npImpsUtilGetIpmiServer(void);
extern NpImpsUtilMpState *npImpsUtilGetMpState(void);

/* Presence snapshot conversion helpers (0x812F0ACA..0x812F0CCF) */
extern void npImpsUtilDiffMyPresence(int eventType, const NpImpsSvcPresenceSnapshot *curSnap,
				     const NpImpsSvcPresenceSnapshot *prevSnap,
				     SceUInt8 *outGameChanged, SceUInt8 *outPartyChanged);
extern void npImpsUtilFillGamePresenceDetail(NpImpsGamePresenceDetail *outDetail,
					     const NpImpsSvcPresenceSnapshot *snapshot);
extern void npImpsUtilFillPartyPresence(NpImpsPartyPresence *outParty,
					const NpImpsSvcPresenceSnapshot *snapshot);

/* Async reply writers (0x812F37B2..0x812F3983) */
extern int npImpsUtilSendEnterOnlineReply(IPMI::Session *session, SceUInt32 requestId,
					  int status, const int *resultCode);
extern int npImpsUtilSendRequestFriendReply(IPMI::Session *session, SceUInt32 requestId,
					    int status, const SceUInt32 *contextId,
					    const SceNpId *npId, const int *resultCode);
extern int npImpsUtilSendAcceptFriendReply(IPMI::Session *session, SceUInt32 requestId,
					   int status, const SceUInt32 *contextId,
					   const SceNpId *npId, const int *resultCode);
extern int npImpsUtilSendDeleteFriendReply(IPMI::Session *session, SceUInt32 requestId,
					   int status, const SceUInt32 *contextId,
					   const SceNpId *npId, const int *resultCode);
extern int npImpsUtilSendAddBlockReply(IPMI::Session *session, SceUInt32 requestId,
				       int status, const SceUInt32 *contextId,
				       const SceNpId *npId, const int *resultCode);
extern int npImpsUtilSendDeleteBlockReply(IPMI::Session *session, SceUInt32 requestId,
					  int status, const SceUInt32 *contextId,
					  const SceNpId *npId, const int *resultCode);

/* Server lifecycle and process event hooks (0x812F0794..0x812F1121) */
extern int npImpsUtilInit(void);
extern int npImpsUtilTerm(void);
extern int npImpsUtilShutdown(void);
extern int npImpsUtilOnProcessEvent(NpImpsUtilServerContext *ctx, int eventType,
				    SceUID processId, int appId, int eventAttr);
extern int npImpsUtilGetProcessServiceState(NpImpsUtilServerContext *ctx, SceUID processId);
extern int npImpsUtilIsInitialized(void);

/* MPState lifecycle and IPC method handlers (implemented in np_imps_util_mpstate.cpp) */
extern int npImpsUtilMpStateStart(void);
extern int npImpsUtilMpStateStop(void);
extern int npImpsUtilMpStateOnProcessCreated(NpImpsUtilMpState *mpState, SceUID processId, int appId);
extern int npImpsUtilMpStatePostEvent(NpImpsUtilMpState *mpState, int eventId,
				      SceUID processId, int errorCode);
extern int npImpsUtilMpStatePostProcessEvent(NpImpsUtilMpState *mpState, int eventId,
					     SceUID processId);
extern int npImpsUtilMpStatePostGlobalEvent(NpImpsUtilMpState *mpState, int eventId,
					    int errorCode);
extern int npImpsUtilMpStateGetServiceState(NpImpsUtilMpState *mpState, SceUID processId,
					    int *outState, SceUInt8 rawMode);

extern int npImpsUtilHandleRegisterGameEvents(IPMI::Session *session,
					      const SceUInt8 mask[NP_IMPS_UTIL_EVENT_COUNT]);
extern int npImpsUtilHandleUnregisterGameEvents(IPMI::Session *session);
extern int npImpsUtilHandleRegisterSystemEvents(IPMI::Session *session,
						const SceUInt8 mask[NP_IMPS_UTIL_EVENT_COUNT]);
extern int npImpsUtilHandleUnregisterSystemEvents(IPMI::Session *session);
extern int npImpsUtilHandleResyncSystemEvents(IPMI::Session *session);

extern int npImpsUtilHandleGetServiceState(IPMI::Session *session, int *outState);
extern int npImpsUtilHandleGetRawServiceState(IPMI::Session *session, int *outState);
extern void npImpsUtilHandleEnterOnlineAsync(IPMI::Session *session, SceUInt32 requestId,
					     SceUInt8 requireAlreadyOnline);
extern int npImpsUtilHandleLeaveOnline(IPMI::Session *session);
extern int npImpsUtilHandleConnectCore(IPMI::Session *session);
extern int npImpsUtilHandleDisconnectCore(IPMI::Session *session);
extern int npImpsUtilHandleIsGlobalOnline(IPMI::Session *session, SceUInt8 *outOnline);
extern void npImpsUtilHandleSessionClosedCleanup(SceUID processId);

extern int npImpsUtilHandleCreateRosterCtx(IPMI::Session *session, SceUInt32 *outCtxId);
extern int npImpsUtilHandleDestroyRosterCtx(IPMI::Session *session, const SceUInt32 *ctxId);
extern int npImpsUtilHandleAbortRosterCtx(IPMI::Session *session, const SceUInt32 *ctxId);
extern int npImpsUtilHandleWaitRosterCtx(IPMI::Session *session, const SceUInt32 *ctxId);
extern int npImpsUtilHandleRequestFriendAsync(IPMI::Session *session, SceUInt32 requestId,
					      const SceUInt32 *ctxId, const SceNpId *npId,
					      const char *comment);
extern int npImpsUtilHandleAcceptFriendAsync(IPMI::Session *session, SceUInt32 requestId,
					     const SceUInt32 *ctxId, const SceNpId *npId);
extern int npImpsUtilHandleDeleteFriendAsync(IPMI::Session *session, SceUInt32 requestId,
					     const SceUInt32 *ctxId, const SceNpId *npId);
extern int npImpsUtilHandleAddBlockAsync(IPMI::Session *session, SceUInt32 requestId,
					 const SceUInt32 *ctxId, const SceNpId *npId);
extern int npImpsUtilHandleDeleteBlockAsync(IPMI::Session *session, SceUInt32 requestId,
					    const SceUInt32 *ctxId, const SceNpId *npId);

extern int npImpsUtilHandleGetAcceptedFriendCount(IPMI::Session *session, SceUInt32 *outCount,
						  SceUInt8 *outLoaded);
extern int npImpsUtilHandleGetAcceptedFriendList(IPMI::Session *session, SceUInt32 startIndex,
						 SceNpId *outEntries, SceUInt32 maxCount,
						 SceUInt32 *outCount, SceUInt8 *outLoaded);
extern int npImpsUtilHandleGetPendingFriendCount(IPMI::Session *session, SceUInt32 *outCount,
						 SceUInt8 *outLoaded);
extern int npImpsUtilHandleGetPendingFriendList(IPMI::Session *session, SceUInt32 startIndex,
						SceNpId *outEntries, SceUInt32 maxCount,
						SceUInt32 *outCount, SceUInt8 *outLoaded);
extern int npImpsUtilHandleGetRequestedFriendCount(IPMI::Session *session, SceUInt32 *outCount,
						   SceUInt8 *outLoaded);
extern int npImpsUtilHandleGetRequestedFriendList(IPMI::Session *session, SceUInt32 startIndex,
						  SceNpId *outEntries, SceUInt32 maxCount,
						  SceUInt32 *outCount, SceUInt8 *outLoaded);
extern int npImpsUtilHandleGetAllFriendCount(IPMI::Session *session, SceUInt32 *outCount);
extern int npImpsUtilHandleGetAllFriendList(IPMI::Session *session, SceUInt32 startIndex,
					    SceNpId *outEntries, SceUInt32 maxCount,
					    SceUInt32 *outCount);
extern int npImpsUtilHandleIsOnline(IPMI::Session *session, SceUInt8 *outOnline);
extern int npImpsUtilHandleGetFriendContextGame(IPMI::Session *session, const SceNpId *npId,
						int *outContext);
extern int npImpsUtilHandleGetFriendContextSys(IPMI::Session *session, const SceNpId *npId,
					       int defaultVal, int *outContext);
extern int npImpsUtilHandleGetFriendPresenceGame(IPMI::Session *session, const SceNpId *npId,
						 NpImpsSvcPresenceSnapshot *outSnap);
extern int npImpsUtilHandleGetFriendPresenceSys(IPMI::Session *session, const SceNpId *npId,
						NpImpsSvcPresenceSnapshot *outSnap);
extern int npImpsUtilHandleGetFriendGamePresence(IPMI::Session *session, const SceNpId *npId,
						 NpImpsGamePresenceWithTitle *outPresence);
extern int npImpsUtilHandleGetBlockListCount(IPMI::Session *session, SceUInt32 *outCount,
					     SceUInt8 *outLoaded);
extern int npImpsUtilHandleGetBlockListEntries(IPMI::Session *session, SceUInt32 startIndex,
					       SceNpId *outEntries, SceUInt32 maxCount,
					       SceUInt32 *outCount, SceUInt8 *outLoaded);
extern int npImpsUtilHandleIsBlocked(IPMI::Session *session, const SceNpId *npId,
				     bool *outBlocked, SceUInt8 *outLoaded);
extern int npImpsUtilHandleSetPartyPresence(IPMI::Session *session,
					    const NpImpsPartyPresence *party);
extern int npImpsUtilHandleClearPartyPresence(IPMI::Session *session);
extern int npImpsUtilHandleSetCommId(IPMI::Session *session, const SceNpCommunicationId *commId);
extern int npImpsUtilHandleClearCommId(IPMI::Session *session);
extern int npImpsUtilHandleSetGamePresence(IPMI::Session *session,
					   const NpImpsGamePresenceInput *input);
extern int npImpsUtilHandleClearGamePresence(IPMI::Session *session);
extern int npImpsUtilHandleSendInGameData(IPMI::Session *session, const SceNpId *targetNpId,
					  const void *msgData, const SceNpCommunicationId *commId);
extern int npImpsUtilHandleAddPlayLog(IPMI::Session *session, const paf::rtc::Tick *date,
				      const SceNpId *npId, const SceNpCommunicationId *commId,
				      const char *description);
extern int npImpsUtilHandleGetPlayLogCount(IPMI::Session *session, int queryKind,
					   const SceNpCommunicationId *commId, SceUInt32 *outCount);
extern int npImpsUtilHandleGetPlayLogList(IPMI::Session *session, int queryKind,
					  const SceNpCommunicationId *commId, SceUInt32 startIndex,
					  void *outEntries, SceUInt32 maxCount, SceUInt32 *outCount);
extern int npImpsUtilHandleDeletePlayLog(IPMI::Session *session, SceUInt32 dateLow,
					 SceUInt32 dateHigh);
extern int npImpsUtilHandleDeleteAllPlayLog(IPMI::Session *session);
extern int npImpsUtilHandleForceRosterSync(IPMI::Session *session, int syncFlags);
extern int npImpsUtilHandleGetMyStatusAndPresence(IPMI::Session *session, int *outStatus,
						  NpImpsSvcPresenceSnapshot *outSnap);
extern int npImpsUtilHandleGetPlayerPlayLogCount(IPMI::Session *session, const SceNpId *npId,
						 SceUInt32 *outCount);
extern int npImpsUtilHandleGetPlayerPlayLog(IPMI::Session *session, const SceNpId *npId,
					    SceUInt32 index, void *outEntry);

/* SceNpImpsSvc C facade (0x812F7FD0..0x812F899B) */
extern int npImpsSvcIsOnline(int *outOnline);
extern int npImpsSvcGetFriendEntryNpId(NpImpsCacheFriendPtr *handle, SceNpId *outNpId);
extern int npImpsSvcGetFriendEntryRelation(NpImpsCacheFriendPtr *handle, int *outRelation);
extern int npImpsSvcGetFriendEntryDate(NpImpsCacheFriendPtr *handle, paf::rtc::Tick *outTick);
extern int npImpsSvcGetFriendEntryRequestDirection(NpImpsCacheFriendPtr *handle, int *outDir);
extern int npImpsSvcGetFriendEntryIsNew(NpImpsCacheFriendPtr *handle, SceUInt8 *outIsNew);
extern int npImpsSvcGetFriendCount(SceUInt32 *outCount);
extern int npImpsSvcGetFriendEntries(SceUInt32 startIndex, SceNpId *outEntries,
				     SceUInt32 maxCount, SceUInt32 *outCount);
extern int npImpsSvcSnapshotFriendHandles(NpImpsCacheFriendPtr ***outHandles, SceUInt32 *outCount,
					  void *(*allocFn)(SceSize));
extern int npImpsSvcReleaseFriendHandles(NpImpsCacheFriendPtr **handles, SceUInt32 count,
					 void (*freeFn)(void *));
extern int npImpsSvcGetAcceptedFriendCount(SceUInt32 *outCount, SceUInt8 *outLoaded);
extern int npImpsSvcGetAcceptedFriendEntries(SceUInt32 startIndex, SceNpId *outEntries,
					     SceUInt32 maxCount, SceUInt32 *outCount,
					     SceUInt8 *outLoaded);
extern int npImpsSvcGetPendingFriendCount(SceUInt32 *outCount, SceUInt8 *outLoaded);
extern int npImpsSvcGetPendingFriendEntries(SceUInt32 startIndex, SceNpId *outEntries,
					    SceUInt32 maxCount, SceUInt32 *outCount,
					    SceUInt8 *outLoaded);
extern int npImpsSvcGetRequestedFriendCount(SceUInt32 *outCount, SceUInt8 *outLoaded);
extern int npImpsSvcGetRequestedFriendEntries(SceUInt32 startIndex, SceNpId *outEntries,
					      SceUInt32 maxCount, SceUInt32 *outCount,
					      SceUInt8 *outLoaded);
extern int npImpsSvcGetFriendPresenceByHandle(NpImpsCacheFriendPtr *handle,
					      NpImpsSvcPresenceSnapshot *outSnap);
extern int npImpsSvcGetFriendPresenceByPlatform(NpImpsCacheFriendPtr *handle,
						NpImpsSvcPresenceSnapshot *outSnap, int platform);
extern int npImpsSvcGetFriendContextByNpId(const SceNpId *npId, int *outContext);
extern int npImpsSvcGetMyOnlineStatus(int *outStatus);
extern int npImpsSvcGetBlockListCount(SceUInt32 *outCount, SceUInt8 *outLoaded);
extern int npImpsSvcGetBlockListEntries(SceUInt32 startIndex, SceNpId *outEntries,
					SceUInt32 maxCount, SceUInt32 *outCount,
					SceUInt8 *outLoaded);
extern int npImpsSvcIsBlocked(const SceNpId *npId, bool *outBlocked, SceUInt8 *outLoaded);
extern int npImpsSvcGetMyPresence(NpImpsSvcPresenceSnapshot *outSnap);
extern int npImpsSvcGetFriendGamePresence(const SceNpId *npId,
					  NpImpsGamePresenceWithTitle *outPresence);
extern int npImpsSvcLockServiceState(void);
extern int npImpsSvcUnlockServiceState(void);
extern int npImpsSvcLockOnlineStatus(void);
extern int npImpsSvcUnlockOnlineStatus(void);
extern int npImpsSvcLockMyPresence(void);
extern int npImpsSvcUnlockMyPresence(void);
extern int npImpsSvcLockFriendStore(void);
extern int npImpsSvcUnlockFriendStore(void);
extern int npImpsSvcLockBlockStore(void);
extern int npImpsSvcUnlockBlockStore(void);
extern int npImpsSvcSetActiveProcess(SceUID processId, int appId);
extern int npImpsSvcSetGameTitle(int appId);
extern int npImpsSvcClearGameTitle(void);
extern int npImpsSvcSetCommId(SceUID processId, const SceNpCommunicationId *commId);
extern int npImpsSvcClearCommId(void);
extern int npImpsSvcHasCommId(bool *outHasCommId);
extern int npImpsSvcPublishMyPresence(void);
extern int npImpsSvcRepublishMyPresenceWithoutCommId(void);
extern int npImpsSvcSetGamePresence(SceUID processId, const NpImpsGamePresenceInput *input);
extern int npImpsSvcClearGamePresence(void);
extern int npImpsSvcSetPartyPresence(const NpImpsPartyPresence *party);
extern int npImpsSvcClearPartyPresence(void);
extern int npImpsSvcIsBigAppGame(int appId, SceUInt8 *outIsBigApp);
extern int npImpsSvcRegisterListener(const NpImpsSvcListenerCallbacks *cb, SceUInt32 *outId);
extern int npImpsSvcUnregisterListener(SceUInt32 listenerId);
extern int npImpsSvcCreateContext(SceUInt32 *outCtxId);
extern int npImpsSvcDestroyContext(SceUInt32 ctxId);
extern int npImpsSvcAbortContext(SceUInt32 ctxId);
extern int npImpsSvcWaitContext(SceUInt32 ctxId);
extern int npImpsSvcRequestFriend(SceUInt32 ctxId, const SceNpId *npId, const char *comment,
				  NpImpsSvcRosterCallback callback);
extern int npImpsSvcAcceptFriend(SceUInt32 ctxId, const SceNpId *npId,
				 NpImpsSvcRosterCallback callback);
extern int npImpsSvcDeleteFriend(SceUInt32 ctxId, const SceNpId *npId,
				 NpImpsSvcRosterCallback callback);
extern int npImpsSvcAddBlock(SceUInt32 ctxId, const SceNpId *npId,
			     NpImpsSvcRosterCallback callback);
extern int npImpsSvcDeleteBlock(SceUInt32 ctxId, const SceNpId *npId,
				NpImpsSvcRosterCallback callback);
extern int npImpsSvcForceRosterSync(int syncFlags);

#endif
