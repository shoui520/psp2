/*
 * np_imps_core_session.cpp: SceNpImpsCore and SceNpImpsSession engine
 * (0x813176AC..0x8131B357).
 *
 * Manages the per-account IMPS session state machine ("SceNpImpsSession"),
 * coordinating SceNpPolling UDP status/interval polling and SceNpCore/SceNpBasic
 * XMPP real-time presence, roster, privacy, and messaging on "SceNpImpsCoreMain".
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <sce_atomic.h>
#include <libnet/netinternal.h>
#include <np/np_common.h>
#include <std/string.h>
#include "np_imps_core.h"

extern "C" int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

/* Msgpipe sizes and timeouts */
static const SceSize NP_IMPS_SESSION_PIPE_BUF_SIZE	= 0x4000u;
static const SceUInt32 NP_IMPS_PIPE_SEND_TIMEOUT_USEC	= 10000000u;
static const SceUInt64 NP_IMPS_CONFIG_TIMEOUT_USEC	= 30000000ULL;
static const SceUInt16 NP_IMPS_POLLING_PORT		= 3478u;
static const SceUInt16 NP_IMPS_XMPP_PORT		= 5223u;
static const int NP_IMPS_MAX_LISTENERS			= 8;
static const int NP_IMPS_DEFAULT_AUTO_INTERVAL_SEC	= 900;

/* Session flag bits */
static const SceUInt32 NP_IMPS_SESSION_FLAG_LINKED	= 0x01u;
static const SceUInt32 NP_IMPS_SESSION_FLAG_CLOSING	= 0x10u;

/* Session worker internal states (0..16, table at 0x81546540) */
enum NpImpsSessionState {
	NP_IMPS_SESS_ERROR			= 0,
	NP_IMPS_SESS_IDLE			= 1,
	NP_IMPS_SESS_FETCH_CONFIG		= 2,
	NP_IMPS_SESS_WAIT_POLL_CONNECT		= 3,
	NP_IMPS_SESS_WAIT_RT_CONNECT		= 4,
	NP_IMPS_SESS_RT_CONNECTED_NEW_SID	= 5,
	NP_IMPS_SESS_RT_CONNECTED_SAME_SID	= 6,
	NP_IMPS_SESS_ENTERING_ONLINE		= 7,
	NP_IMPS_SESS_ENTERING_INTERVAL_POLL	= 8,
	NP_IMPS_SESS_ENTERING_CRON_POLL		= 9,
	NP_IMPS_SESS_DISCONNECTING		= 10,
	NP_IMPS_SESS_ONLINE			= 11,
	NP_IMPS_SESS_POLL_INTERVAL		= 12,
	NP_IMPS_SESS_POLL_CRON			= 13,
	NP_IMPS_SESS_SUSPENDED			= 14,
	NP_IMPS_SESS_STOPPING			= 15,
	NP_IMPS_SESS_STOPPED			= 16
};

/* RT sub-controller states (0..6) */
enum NpImpsRtState {
	NP_IMPS_RT_ERROR		= 0,
	NP_IMPS_RT_IDLE			= 1,
	NP_IMPS_RT_CONNECTING		= 2,
	NP_IMPS_RT_CONNECTED		= 3,
	NP_IMPS_RT_SETTING_STANDBY	= 4,
	NP_IMPS_RT_DISCONNECTING	= 5,
	NP_IMPS_RT_DISCONNECTED		= 6
};

/* Polling sub-controller states (0..4) */
enum NpImpsPollState {
	NP_IMPS_POLL_ERROR		= 0,
	NP_IMPS_POLL_IDLE		= 1,
	NP_IMPS_POLL_CONNECTING		= 2,
	NP_IMPS_POLL_CONNECTED		= 3,
	NP_IMPS_POLL_DISCONNECTED	= 4
};

/* 128-byte msgpipe command/event kinds */
enum NpImpsSessionMsgKind {
	NP_IMPS_MSG_INVALID		= -1,
	NP_IMPS_MSG_WAKEUP		= 0,
	NP_IMPS_MSG_START		= 1,
	NP_IMPS_MSG_SET_MODE		= 2,
	NP_IMPS_MSG_SEND_PRESENCE	= 3,
	NP_IMPS_MSG_SEND_MESSAGE	= 4,
	NP_IMPS_MSG_DELETE_STORED	= 5,
	NP_IMPS_MSG_RT_EVENT		= 6,
	NP_IMPS_MSG_POLL_EVENT		= 7
};

/* RT sub-event kinds inside NP_IMPS_MSG_RT_EVENT */
enum NpImpsRtEventKind {
	NP_IMPS_RT_EVT_STATE		= 0,
	NP_IMPS_RT_EVT_STANDBY_DONE	= 1,
	NP_IMPS_RT_EVT_ROSTER_PUSH	= 2,
	NP_IMPS_RT_EVT_ROSTER_GET_DONE	= 3,
	NP_IMPS_RT_EVT_PRIVACY_PUSH	= 4,
	NP_IMPS_RT_EVT_PRESENCE_RECV	= 5,
	NP_IMPS_RT_EVT_MESSAGE_RECV	= 6,
	NP_IMPS_RT_EVT_PRESENCE_SENT	= 7,
	NP_IMPS_RT_EVT_MESSAGE_SENT	= 8,
	NP_IMPS_RT_EVT_STORED_DELETED	= 9
};

/* Polling sub-event kinds inside NP_IMPS_MSG_POLL_EVENT */
enum NpImpsPollEventKind {
	NP_IMPS_POLL_EVT_STATE		= 0,
	NP_IMPS_POLL_EVT_UPDATE		= 1
};

/* Polling update bitmask computed from timestamp diffs */
enum NpImpsPollDiffBits {
	NP_IMPS_POLL_DIFF_FRIEND_LIST	= 0x01,
	NP_IMPS_POLL_DIFF_BLOCK_LIST	= 0x02,
	NP_IMPS_POLL_DIFF_MESSAGE	= 0x04
};

/* Outgoing RT transaction node (36 bytes) */
struct NpImpsRtTxNode {
	NpImpsTxNodeHeader header;		/* +0x00 (20 bytes) */
	NpImpsSessionSendDoneCallback callback;	/* +0x14 */
	void *userArg;				/* +0x18 */
	int resultCode;				/* +0x1C */
	int rtTransactionId;			/* +0x20 */
};

/* Embedded RT sub-controller (244 bytes = 0xF4, at NpImpsSession +0x98) */
struct NpImpsRtBridge {
	NpRtServerConfig serverConfig;		/* +0x00 (130 bytes) */
	SceUInt8 padAfterServer[2];		/* +0x82 */
	int rtSessionHandle;			/* +0x84 */
	SceNpOnlineId onlineId;			/* +0x88 (20 bytes) */
	char sessionSid[42];			/* +0x9C */
	SceUInt8 padAfterSid[2];		/* +0xC6 */
	int rtState;				/* +0xC8 */
	int pendingStandbyTxId;			/* +0xCC */
	int pendingRosterTxId;			/* +0xD0 */
	NpImpsTxQueue txQueue;			/* +0xD4 (32 bytes) */
};

/* Embedded Polling sub-controller (200 bytes = 0xC8, at NpImpsSession +0x198) */
struct NpImpsPollBridge {
	int pollState;				/* +0x00 */
	NpPollingEndpoint endpoint;		/* +0x04 (66 bytes) */
	SceUInt8 padAfterEndpoint[2];		/* +0x46 */
	int pollSessionId;			/* +0x48 */
	SceNpId npId;				/* +0x4C (36 bytes) */
	SceUInt8 padBeforeTicks[48];		/* +0x70 */
	NpPollingTimestampSet timestamps;	/* +0xA0 (24 bytes) */
	NpPollingStatusConfig statusConfig;	/* +0xB8 (12 bytes) */
	SceUInt32 padTail;			/* +0xC4 */
};

/* RT event payload carried inside NpImpsSessionMsg (120 bytes) */
struct NpImpsRtEventPayload {
	int rtSessionId;			/* +0x00 */
	SceNpOnlineId onlineId;			/* +0x04 (20 bytes) */
	char sessionSid[42];			/* +0x18 */
	SceUInt8 padAfterSid[2];		/* +0x42 */
	int eventKind;				/* +0x44 (word 17) */
	union {
		struct {
			int stateOrTxId;	/* +0x48 (word 18) */
			int errorOrResult;	/* +0x4C (word 19) */
			NpBasicRosterList *rosterList; /* +0x50 (word 20) */
		} status;
		NpBasicRosterList *rosterPush;
		NpBasicPrivacyList *privacyPush;
		NpRtPresenceStanza *presenceStanza;
		NpRtMessageStanza *messageStanza;
	} detail;
};

/* Polling event payload carried inside NpImpsSessionMsg (120 bytes) */
struct NpImpsPollEventPayload {
	NpPollingSessionIdentity identity;	/* +0x00 (84 bytes) */
	int eventKind;				/* +0x54 (word 21) */
	union {
		struct {
			int stateCode;		/* +0x58 (word 22) */
			int resultCode;		/* +0x5C (word 23) */
		} state;
		struct {
			int updateKind;		/* +0x58 (word 22) */
			SceUInt32 padAlign;	/* +0x5C */
			NpPollingTimestampSet timestamps; /* +0x60 (24 bytes) */
		} update;
	} detail;
};

/* 128-byte session msgpipe message */
struct NpImpsSessionMsg {
	int kind;				/* +0x00 */
	int padHeader;				/* +0x04 */
	union {
		struct {
			int mode;		/* +0x08 (word 2) */
			NpRtPresenceStanza *presence; /* +0x0C (word 3) */
		} modeReq;
		struct {
			int transactionId;	/* +0x08 (word 2) */
			NpRtPresenceStanza *stanza; /* +0x0C (word 3) */
		} sendPresence;
		struct {
			int transactionId;	/* +0x08 (word 2) */
			NpRtMessageStanza *stanza; /* +0x0C (word 3) */
		} sendMessage;
		struct {
			int transactionId;	/* +0x08 (word 2) */
			SceUInt32 padAlign;	/* +0x0C */
			SceUInt64 storedId;	/* +0x10 (words 4..5) */
		} deleteStored;
		NpImpsRtEventPayload rtEvent;	/* +0x08 */
		NpImpsPollEventPayload pollEvent; /* +0x08 */
		SceUInt8 raw[120];
	} body;
};

/* SceNpImpsSession object (616 bytes = 0x268) */
struct NpImpsSession {
	SceKernelLwMutexWork mutex;		/* +0x000 (32 bytes) */
	SceUInt32 flags;			/* +0x020 (word 8) */
	int refCount;				/* +0x024 (word 9) */
	NpImpsSession *next;			/* +0x028 (word 10) */
	NpImpsSession *prev;			/* +0x02C (word 11) */
	NpImpsSessionIdentity identity;		/* +0x030 (40 bytes, word 12) */
	int state;				/* +0x058 (word 22) */
	SceUID msgPipeId;			/* +0x05C (word 23) */
	NpPollingTimeoutTimer timeoutTimer;	/* +0x060 (40 bytes, word 24) */
	SceUID *workerThreadPtr;		/* +0x088 (word 34) */
	int desiredMode;			/* +0x08C (word 35) */
	NpRtPresenceStanza *desiredPresence;	/* +0x090 (word 36) */
	NpRtPresenceStanza *pendingPresence;	/* +0x094 (word 37) */
	NpImpsRtBridge rt;			/* +0x098 (244 bytes, word 38) */
	int aborted;				/* +0x18C (word 99) */
	int accountReqHandle;			/* +0x190 (word 100) */
	SceUInt32 padBeforePoll;		/* +0x194 */
	NpImpsPollBridge poll;			/* +0x198 (200 bytes, word 102) */
	int lastError;				/* +0x260 (word 152) */
	int fallbackError;			/* +0x264 (word 153) */
};

/* Registered listener node (36 bytes) */
struct NpImpsSessionListenerNode {
	NpImpsSessionListenerNode *next;	/* +0x00 */
	NpImpsSessionListenerNode *prev;	/* +0x04 */
	int listenerId;				/* +0x08 */
	NpImpsCoreSessionCallbacks callbacks;	/* +0x0C (24 bytes) */
};

/* Global SceNpImpsCore state */
static SceKernelLwMutexWork s_impsCoreMutex;
static int s_impsCoreMutexInit = 0;

static int s_sessionMgrInit = 0;
static NpImpsSession *s_sessionListHead = NULL;
static NpImpsSession *s_sessionListTail = NULL;
static int s_sessionCount = 0;
static int s_nextSessionId = 0;
static SceUID s_coreMainThreadId = -1;

static int s_pollingCallbackId = 0;
static int s_rtHandlerId = 0;

static NpImpsSessionListenerNode *s_listenerHead = NULL;
static NpImpsSessionListenerNode *s_listenerTail = NULL;
static int s_listenerCount = 0;
static int s_nextListenerId = 0;

/* Forward declarations */
static int npImpsSessionMainLoop(NpImpsSession *session);
static void npImpsSessionMsgCleanup(NpImpsSessionMsg *msg);
static int npImpsSessionSendWakeup(SceUID *pipeId);
static void npImpsSessionNotifyState(const NpImpsSessionIdentity *identity, int eventState,
				     int errorCode);
static void npImpsSessionNotifyRoster(const NpImpsSessionIdentity *identity, int isPush,
				      NpBasicRosterList *rosterList);
static void npImpsSessionNotifyPrivacy(const NpImpsSessionIdentity *identity, int action,
				       NpBasicPrivacyList *privacyList);
static void npImpsSessionNotifyMessage(const NpImpsSessionIdentity *identity,
				       NpRtMessageStanza *stanza);
static void npImpsSessionNotifyPresence(const NpImpsSessionIdentity *identity,
					NpRtPresenceStanza *stanza);
static void npImpsSessionNotifyPolling(const NpImpsSessionIdentity *identity,
				       const NpPollingTimestampSet *timestamps, int diffBits);

/* ============================================================================
 * Global SceNpImpsCore mutex (0x813192E8..0x8131938B)
 * ============================================================================ */

static int npImpsCoreMutexCreate(void)
{
	if (s_impsCoreMutexInit > 0)
		return NP_IMPS_CORE_ERR_ALREADY_INIT;
	int ret = sceKernelCreateLwMutex(&s_impsCoreMutex, "SceNpImpsCore", 0x2002u, 0, NULL);
	if (ret >= 0)
		s_impsCoreMutexInit = 1;
	return ret;
}

static int npImpsCoreMutexDelete(void)
{
	if (s_impsCoreMutexInit >= 0)
		return sceKernelDeleteLwMutex(&s_impsCoreMutex);
	return s_impsCoreMutexInit;
}

static int npImpsCoreMutexLock(void)
{
	int ret = sceKernelLockLwMutex(&s_impsCoreMutex, 1, NULL);
	if (ret >= 0)
		return 1;
	return ret;
}

static int npImpsCoreMutexUnlock(int lockState)
{
	if (lockState > 0)
		sceKernelUnlockLwMutex(&s_impsCoreMutex, 1);
	return 0;
}

/* ============================================================================
 * Shared 3-list transaction queue (0x8131938C..0x81319817)
 * ============================================================================ */

void npImpsTxQueueInit(NpImpsTxQueue *queue, SceKernelLwMutexWork *mutex)
{
	queue->mutex = mutex;
	queue->totalCount = 0;
	queue->pendingHead = NULL;
	queue->pendingTail = NULL;
	queue->inFlightHead = NULL;
	queue->inFlightTail = NULL;
	queue->completedHead = NULL;
	queue->completedTail = NULL;
}

void npImpsTxQueueUnlink(NpImpsTxQueue *queue, NpImpsTxNodeHeader *node)
{
	NpImpsTxNodeHeader **headPtr = NULL;
	NpImpsTxNodeHeader **tailPtr = NULL;

	if (node->listState == 1) {
		headPtr = &queue->pendingHead;
		tailPtr = &queue->pendingTail;
	} else if (node->listState == 2) {
		headPtr = &queue->inFlightHead;
		tailPtr = &queue->inFlightTail;
	} else if (node->listState == 3) {
		headPtr = &queue->completedHead;
		tailPtr = &queue->completedTail;
	} else {
		return;
	}

	if (node->next != NULL)
		node->next->prev = node->prev;
	else
		*tailPtr = node->prev;

	if (node->prev != NULL)
		node->prev->next = node->next;
	else
		*headPtr = node->next;

	node->listState = 0;
	node->next = NULL;
	node->prev = NULL;
}

static void npImpsTxQueueAppendLocked(NpImpsTxNodeHeader **headPtr, NpImpsTxNodeHeader **tailPtr,
				      NpImpsTxNodeHeader *node, int listState)
{
	node->next = NULL;
	node->prev = *tailPtr;
	if (*tailPtr != NULL)
		(*tailPtr)->next = node;
	if (*headPtr == NULL)
		*headPtr = node;
	*tailPtr = node;
	node->listState = listState;
}

/* 0x81319422 */
static int npImpsRtTxCreate(NpImpsTxQueue *queue, int requestId,
			    NpImpsSessionSendDoneCallback callback, void *userArg,
			    NpImpsRtTxNode **outNode)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpImpsRtTxNode *node = reinterpret_cast<NpImpsRtTxNode *>(
		npRtHeapAlloc(sizeof(NpImpsRtTxNode), NULL));
	if (node == NULL) {
		sceKernelUnlockLwMutex(queue->mutex, 1);
		return NP_IMPS_CORE_ERR_NO_MEMORY;
	}

	sceClibMemset(node, 0, sizeof(*node));
	queue->totalCount++;
	node->header.owner = queue;
	node->header.requestId = requestId;
	node->callback = callback;
	node->userArg = userArg;
	node->rtTransactionId = 0;
	npImpsTxQueueAppendLocked(&queue->pendingHead, &queue->pendingTail, &node->header, 1);
	*outNode = node;

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x813194FC */
static int npImpsRtTxFree(NpImpsTxQueue *queue, NpImpsRtTxNode *node)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	npImpsTxQueueUnlink(queue, &node->header);
	npRtHeapFree(node, NULL);
	queue->totalCount--;

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81319548 */
static int npImpsRtTxRemoveById(NpImpsTxQueue *queue, int requestId)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	NpImpsTxNodeHeader *found = NULL;
	NpImpsTxNodeHeader *lists[3] = {
		queue->pendingHead, queue->inFlightHead, queue->completedHead
	};
	for (int listIdx = 0; listIdx < 3 && found == NULL; ++listIdx) {
		for (NpImpsTxNodeHeader *cur = lists[listIdx]; cur != NULL; cur = cur->next) {
			if (cur->requestId == requestId) {
				found = cur;
				break;
			}
		}
	}

	if (found != NULL) {
		npImpsTxQueueUnlink(queue, found);
		npRtHeapFree(found, NULL);
		queue->totalCount--;
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x813195CE */
int npImpsTxQueueFindPending(NpImpsTxQueue *queue, int requestId, NpImpsTxNodeHeader **outNode)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	NpImpsTxNodeHeader *cur = queue->pendingHead;
	while (cur != NULL) {
		if (cur->requestId == requestId)
			break;
		cur = cur->next;
	}
	*outNode = cur;

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81311AA0 equivalent for NpImpsTxQueue: pops head of completed list */
int npImpsTxQueuePopCompleted(NpImpsTxQueue *queue, NpImpsTxNodeHeader **outNode)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	NpImpsTxNodeHeader *head = queue->completedHead;
	if (head != NULL)
		npImpsTxQueueUnlink(queue, head);
	*outNode = head;

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81319626 */
static int npImpsRtTxMarkInFlight(NpImpsTxQueue *queue, NpImpsRtTxNode *node, int rtTxId)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	npImpsTxQueueUnlink(queue, &node->header);
	npImpsTxQueueAppendLocked(&queue->inFlightHead, &queue->inFlightTail, &node->header, 2);
	node->rtTransactionId = rtTxId;

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x8131968E */
static int npImpsRtTxCompleteByRtId(NpImpsTxQueue *queue, int rtTxId, int resultCode)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (NpImpsTxNodeHeader *cur = queue->inFlightHead; cur != NULL; cur = cur->next) {
		NpImpsRtTxNode *node = reinterpret_cast<NpImpsRtTxNode *>(cur);
		if (node->rtTransactionId == rtTxId) {
			npImpsTxQueueUnlink(queue, &node->header);
			npImpsTxQueueAppendLocked(&queue->completedHead, &queue->completedTail,
						  &node->header, 3);
			node->resultCode = resultCode;
			break;
		}
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81319700 */
static int npImpsRtTxFailPendingById(NpImpsTxQueue *queue, int requestId, int resultCode)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (NpImpsTxNodeHeader *cur = queue->pendingHead; cur != NULL; cur = cur->next) {
		if (cur->requestId == requestId) {
			NpImpsRtTxNode *node = reinterpret_cast<NpImpsRtTxNode *>(cur);
			npImpsTxQueueUnlink(queue, &node->header);
			npImpsTxQueueAppendLocked(&queue->completedHead, &queue->completedTail,
						  &node->header, 3);
			node->resultCode = resultCode;
			break;
		}
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81319772 */
static int npImpsRtTxFailAllActive(NpImpsTxQueue *queue, int resultCode)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	while (queue->pendingHead != NULL) {
		NpImpsRtTxNode *node = reinterpret_cast<NpImpsRtTxNode *>(queue->pendingHead);
		npImpsTxQueueUnlink(queue, &node->header);
		npImpsTxQueueAppendLocked(&queue->completedHead, &queue->completedTail,
					  &node->header, 3);
		node->resultCode = resultCode;
	}
	while (queue->inFlightHead != NULL) {
		NpImpsRtTxNode *node = reinterpret_cast<NpImpsRtTxNode *>(queue->inFlightHead);
		npImpsTxQueueUnlink(queue, &node->header);
		npImpsTxQueueAppendLocked(&queue->completedHead, &queue->completedTail,
					  &node->header, 3);
		node->resultCode = resultCode;
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* ============================================================================
 * Session msgpipe helpers (0x81317DF0..0x813181DF)
 * ============================================================================ */

static int npImpsSessionPipeCreate(SceUID *pipeId)
{
	SceUID uid = sceKernelCreateMsgPipe("SceNpImpsSession", 64, 12,
					    NP_IMPS_SESSION_PIPE_BUF_SIZE, NULL);
	if (uid < 0)
		return uid;
	*pipeId = uid;
	return 0;
}

static void npImpsSessionPipeDestroy(SceUID *pipeId)
{
	NpImpsSessionMsg msg;
	SceSize received = 0;
	while (sceKernelTryReceiveMsgPipe(*pipeId, &msg, sizeof(msg), 1, &received) >= 0 &&
	       received == sizeof(msg)) {
		npImpsSessionMsgCleanup(&msg);
	}
	sceKernelDeleteMsgPipe(*pipeId);
}

static int npImpsSessionPipeSend(SceUID *pipeId, const NpImpsSessionMsg *msg, SceUInt32 *timeout)
{
	SceSize sent = 0;
	int ret = sceKernelSendMsgPipe(*pipeId, const_cast<NpImpsSessionMsg *>(msg),
				       sizeof(*msg), 1, &sent, timeout);
	if (ret == static_cast<int>(0x80028005u))
		return NP_IMPS_CORE_ERR_PIPE_TIMEOUT;
	if (ret >= 0)
		return (sent == sizeof(*msg)) ? 0 : NP_IMPS_CORE_ERR_INTERNAL;
	return ret;
}

static int npImpsSessionPipeTryRecv(SceUID *pipeId, NpImpsSessionMsg *msg)
{
	SceSize received = 0;
	int ret = sceKernelTryReceiveMsgPipe(*pipeId, msg, sizeof(*msg), 1, &received);
	if (ret == static_cast<int>(0x80028223u)) {
		msg->kind = NP_IMPS_MSG_INVALID;
		return 0;
	}
	if (ret >= 0) {
		if (received == sizeof(*msg))
			return 0;
		ret = NP_IMPS_CORE_ERR_INTERNAL;
	}
	msg->kind = NP_IMPS_MSG_INVALID;
	return ret;
}

static int npImpsSessionPipeRecv(SceUID *pipeId, NpImpsSessionMsg *msg, SceUInt32 *timeout)
{
	SceSize received = 0;
	int ret = sceKernelReceiveMsgPipe(*pipeId, msg, sizeof(*msg), 1, &received, timeout);
	if (ret == static_cast<int>(0x80028005u)) {
		ret = NP_IMPS_CORE_ERR_PIPE_TIMEOUT;
	} else if (ret >= 0) {
		if (received == sizeof(*msg))
			return 0;
		ret = NP_IMPS_CORE_ERR_INTERNAL;
	}
	msg->kind = NP_IMPS_MSG_INVALID;
	return ret;
}

/* 0x81317FAA */
static int npImpsSessionSendWakeup(SceUID *pipeId)
{
	NpImpsSessionMsg msg;
	msg.kind = NP_IMPS_MSG_WAKEUP;
	SceUInt32 timeout = NP_IMPS_PIPE_SEND_TIMEOUT_USEC;
	SceSize sent = 0;
	int ret = sceKernelSendMsgPipe(*pipeId, &msg, sizeof(msg), 1, &sent, &timeout);
	if (ret == -2147319803)
		return NP_IMPS_CORE_ERR_PIPE_TIMEOUT;
	if (ret >= 0)
		return (sent == sizeof(msg)) ? 0 : NP_IMPS_CORE_ERR_INTERNAL;
	return ret;
}

/* 0x8131B2EC */
static void npImpsRtEventCleanup(NpImpsRtEventPayload *ev)
{
	switch (ev->eventKind) {
	case NP_IMPS_RT_EVT_ROSTER_PUSH:
		if (ev->detail.rosterPush != NULL) {
			npBasicReleaseRosterList(ev->detail.rosterPush);
			ev->detail.rosterPush = NULL;
		}
		break;
	case NP_IMPS_RT_EVT_ROSTER_GET_DONE:
		if (ev->detail.status.rosterList != NULL) {
			npBasicReleaseRosterList(ev->detail.status.rosterList);
			ev->detail.status.rosterList = NULL;
		}
		break;
	case NP_IMPS_RT_EVT_PRIVACY_PUSH:
		if (ev->detail.privacyPush != NULL) {
			npBasicReleasePrivacyList(ev->detail.privacyPush);
			ev->detail.privacyPush = NULL;
		}
		break;
	case NP_IMPS_RT_EVT_PRESENCE_RECV:
		if (ev->detail.presenceStanza != NULL) {
			npRtReleasePresenceStanza(ev->detail.presenceStanza);
			ev->detail.presenceStanza = NULL;
		}
		break;
	case NP_IMPS_RT_EVT_MESSAGE_RECV:
		if (ev->detail.messageStanza != NULL) {
			npRtReleaseMessageStanza(ev->detail.messageStanza);
			ev->detail.messageStanza = NULL;
		}
		break;
	default:
		break;
	}
	ev->eventKind = -1;
}

/* 0x81319818 */
static void npImpsSessionMsgCleanupCore(NpImpsSessionMsg *msg)
{
	switch (msg->kind) {
	case NP_IMPS_MSG_START:
	case NP_IMPS_MSG_SET_MODE:
	case NP_IMPS_MSG_SEND_PRESENCE:
		if (msg->body.sendPresence.stanza != NULL) {
			npRtReleasePresenceStanza(msg->body.sendPresence.stanza);
			msg->body.sendPresence.stanza = NULL;
		}
		break;
	case NP_IMPS_MSG_SEND_MESSAGE:
		if (msg->body.sendMessage.stanza != NULL) {
			npRtReleaseMessageStanza(msg->body.sendMessage.stanza);
			msg->body.sendMessage.stanza = NULL;
		}
		break;
	case NP_IMPS_MSG_RT_EVENT:
		npImpsRtEventCleanup(&msg->body.rtEvent);
		break;
	case NP_IMPS_MSG_POLL_EVENT:
		msg->body.pollEvent.eventKind = -1;
		break;
	default:
		break;
	}
	msg->kind = NP_IMPS_MSG_INVALID;
}

/* 0x8131987A */
static void npImpsSessionMsgCleanup(NpImpsSessionMsg *msg)
{
	npImpsSessionMsgCleanupCore(msg);
}

/* 0x8131801C */
static int npImpsSessionRejectUnexpectedMsg(NpImpsSession *session, const NpImpsSessionMsg *msg,
					    int errorCode)
{
	if (msg->kind == NP_IMPS_MSG_SEND_PRESENCE || msg->kind == NP_IMPS_MSG_SEND_MESSAGE)
		npImpsRtTxFailPendingById(&session->rt.txQueue, msg->body.sendPresence.transactionId,
					  errorCode);
	return 0;
}

/* 0x8131804C */
static int npImpsSessionRecvFilteredMsg(NpImpsSession *session, NpImpsSessionMsg *msg,
					SceUInt32 *timeout)
{
	while (true) {
		msg->kind = NP_IMPS_MSG_INVALID;
		int ret = npImpsSessionPipeRecv(&session->msgPipeId, msg, timeout);
		if (ret < 0)
			return ret;

		if (msg->kind == NP_IMPS_MSG_RT_EVENT) {
			if (msg->body.rtEvent.rtSessionId == session->rt.rtSessionHandle)
				return 0;
		} else if (msg->kind == NP_IMPS_MSG_POLL_EVENT) {
			if (msg->body.pollEvent.identity.sessionId == session->poll.pollSessionId)
				return 0;
		} else {
			return 0;
		}
		npImpsSessionMsgCleanup(msg);
	}
}

/* 0x813180B8 */
static int npImpsSessionPostCommand(NpImpsSession *session, const SceUInt32 *allowedStateMask,
				    const NpImpsSessionMsg *msg)
{
	int ret = sceKernelLockLwMutex(&session->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (allowedStateMask != NULL && ((1u << session->state) & *allowedStateMask) == 0) {
		sceKernelUnlockLwMutex(&session->mutex, 1);
		return NP_IMPS_CORE_ERR_BAD_STATE;
	}

	if ((session->flags & NP_IMPS_SESSION_FLAG_LINKED) == 0 ||
	    (session->flags & NP_IMPS_SESSION_FLAG_CLOSING) != 0) {
		sceKernelUnlockLwMutex(&session->mutex, 1);
		return NP_IMPS_CORE_ERR_SESSION_NOT_FOUND;
	}

	sceKernelUnlockLwMutex(&session->mutex, 1);
	SceUInt32 timeout = NP_IMPS_PIPE_SEND_TIMEOUT_USEC;
	return npImpsSessionPipeSend(&session->msgPipeId, msg, &timeout);
}

/* 0x81318162 */
static int npImpsSessionDrainPendingMsgs(NpImpsSession *session, int errorCode)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = 0;
	while (true) {
		ret = npImpsSessionPipeTryRecv(&session->msgPipeId, &msg);
		if (ret < 0)
			break;
		if (msg.kind == NP_IMPS_MSG_INVALID) {
			ret = 0;
			break;
		}
		ret = npImpsSessionRejectUnexpectedMsg(session, &msg, errorCode);
		if (ret < 0)
			break;
		npImpsSessionMsgCleanup(&msg);
	}
	npImpsSessionMsgCleanup(&msg);
	return ret;
}

/* ============================================================================
 * Embedded RT sub-controller (0x813186C4..0x81318FFF)
 * ============================================================================ */

static void npImpsRtInit(NpImpsRtBridge *rt, SceKernelLwMutexWork *mutex)
{
	sceClibMemset(rt, 0, sizeof(*rt));
	rt->rtSessionHandle = 0;
	rt->rtState = NP_IMPS_RT_IDLE;
	rt->pendingStandbyTxId = 0;
	rt->pendingRosterTxId = 0;
	npImpsTxQueueInit(&rt->txQueue, mutex);
}

static void npImpsRtDestroyHandle(NpImpsRtBridge *rt)
{
	if (rt->rtSessionHandle != 0) {
		npRtDestroySession(rt->rtSessionHandle);
		rt->rtSessionHandle = 0;
	}
}

static void npImpsRtSetState(NpImpsRtBridge *rt, int newState)
{
	rt->rtState = newState;
	if (newState == NP_IMPS_RT_ERROR) {
		npImpsRtSetState(rt, NP_IMPS_RT_DISCONNECTED);
		return;
	}
	if (newState == NP_IMPS_RT_DISCONNECTED)
		npImpsRtDestroyHandle(rt);
}

static void npImpsRtForceClose(NpImpsRtBridge *rt)
{
	if (rt->rtState != NP_IMPS_RT_DISCONNECTED)
		npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
}

static bool npImpsRtHasSessionSid(const NpImpsRtBridge *rt)
{
	return rt->sessionSid[0] != '\0';
}

static void npImpsRtConfigure(NpImpsRtBridge *rt, const SceNpOnlineId *onlineId,
			      const NpRtServerConfig *serverConfig)
{
	sceClibMemcpy_safe(&rt->onlineId, onlineId, sizeof(rt->onlineId));
	sceClibMemcpy_safe(&rt->serverConfig, serverConfig, sizeof(rt->serverConfig));
}

/* 0x8131876E */
static int npImpsRtConnect(NpImpsRtBridge *rt)
{
	int handle = 0;
	int ret = npRtCreateSession(&rt->onlineId, &rt->serverConfig, &handle);
	if (ret >= 0)
		ret = npRtConnectSession(handle);
	if (ret < 0) {
		if (handle != 0)
			npRtDestroySession(handle);
		npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
		return ret;
	}
	npImpsRtSetState(rt, NP_IMPS_RT_CONNECTING);
	rt->rtSessionHandle = handle;
	return 0;
}

/* 0x813187D8 */
static int npImpsRtReconnect(NpImpsRtBridge *rt)
{
	int handle = 0;
	int ret = npRtCreateSession(&rt->onlineId, &rt->serverConfig, &handle);
	if (ret >= 0)
		ret = npRtReconnectSession(handle, rt->sessionSid);
	if (ret < 0) {
		if (handle != 0)
			npRtDestroySession(handle);
		npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
		return ret;
	}
	npImpsRtSetState(rt, NP_IMPS_RT_CONNECTING);
	rt->rtSessionHandle = handle;
	return 0;
}

/* 0x81318848 */
static int npImpsRtEnterStandby(NpImpsRtBridge *rt, int standbyMode,
				NpRtSendDoneCallback callback)
{
	int txId = 0;
	int ret = npRtAllocTransactionId(rt->rtSessionHandle, &txId);
	if (ret >= 0)
		ret = npRtImpsSendStandbyIq(rt->rtSessionHandle, txId, standbyMode, callback);
	if (ret < 0) {
		npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
		return ret;
	}
	rt->pendingStandbyTxId = txId;
	npImpsRtSetState(rt, NP_IMPS_RT_SETTING_STANDBY);
	return 0;
}

/* 0x813188B8 */
static int npImpsRtSyncInitialOnline(NpImpsRtBridge *rt, NpRtPresenceStanza *initialPresence,
				     NpBasicRosterGetCallback rosterCallback)
{
	int txId = 0;
	int ret = npRtAllocTransactionId(rt->rtSessionHandle, &txId);
	if (ret >= 0) {
		ret = npBasicRequestRoster(rt->rtSessionHandle, txId, rosterCallback);
		if (ret >= 0) {
			rt->pendingRosterTxId = txId;
			ret = npRtAllocTransactionId(rt->rtSessionHandle, &txId);
			if (ret >= 0) {
				ret = npRtImpsSendPresence(rt->rtSessionHandle, txId,
							   initialPresence, NULL);
				if (ret >= 0) {
					ret = npRtAllocTransactionId(rt->rtSessionHandle, &txId);
					if (ret >= 0) {
						ret = npRtImpsFetchStored(rt->rtSessionHandle,
									  txId, NULL);
						if (ret >= 0)
							return 0;
					}
				}
			}
		}
	}
	npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
	return ret;
}

/* 0x81318964 */
static int npImpsRtDisconnect(NpImpsRtBridge *rt)
{
	int ret = npRtImpsSendOfflinePresence(rt->rtSessionHandle);
	if (ret >= 0)
		ret = npRtDisconnectSession(rt->rtSessionHandle);
	if (ret < 0) {
		npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
		return ret;
	}
	npImpsRtSetState(rt, NP_IMPS_RT_DISCONNECTING);
	return 0;
}

/* 0x8131899C */
static int npImpsRtDispatchPresence(NpImpsRtBridge *rt, int requestId,
				    NpRtPresenceStanza *stanza, NpRtSendDoneCallback callback)
{
	NpImpsTxNodeHeader *txHdr = NULL;
	int rtTxId = 0;
	int ret = npImpsTxQueueFindPending(&rt->txQueue, requestId, &txHdr);
	if (ret >= 0) {
		ret = npRtAllocTransactionId(rt->rtSessionHandle, &rtTxId);
		if (ret >= 0) {
			ret = npImpsRtTxMarkInFlight(&rt->txQueue,
						     reinterpret_cast<NpImpsRtTxNode *>(txHdr),
						     rtTxId);
			if (ret >= 0) {
				ret = npRtImpsSendPresence(rt->rtSessionHandle, rtTxId,
							   stanza, callback);
				if (ret >= 0)
					return 0;
			}
		}
	}
	if (txHdr != NULL)
		npImpsRtTxFailPendingById(&rt->txQueue, requestId, ret);
	npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
	return ret;
}

/* 0x81318A3E */
static int npImpsRtDispatchMessage(NpImpsRtBridge *rt, int requestId,
				   NpRtMessageStanza *stanza, NpRtSendDoneCallback callback)
{
	NpImpsTxNodeHeader *txHdr = NULL;
	int rtTxId = 0;
	int ret = npImpsTxQueueFindPending(&rt->txQueue, requestId, &txHdr);
	if (ret >= 0) {
		ret = npRtAllocTransactionId(rt->rtSessionHandle, &rtTxId);
		if (ret >= 0) {
			ret = npImpsRtTxMarkInFlight(&rt->txQueue,
						     reinterpret_cast<NpImpsRtTxNode *>(txHdr),
						     rtTxId);
			if (ret >= 0) {
				ret = npRtImpsSendMessage(rt->rtSessionHandle, rtTxId,
							  stanza, callback);
				if (ret >= 0)
					return 0;
			}
		}
	}
	if (txHdr != NULL)
		npImpsRtTxFailPendingById(&rt->txQueue, requestId, ret);
	npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
	return ret;
}

/* 0x81318AE0 */
static int npImpsRtDispatchDeleteStored(NpImpsRtBridge *rt, int requestId,
					SceUInt64 storedId, NpRtSendDoneCallback callback)
{
	NpImpsTxNodeHeader *txHdr = NULL;
	int rtTxId = 0;
	int ret = npImpsTxQueueFindPending(&rt->txQueue, requestId, &txHdr);
	if (ret >= 0) {
		ret = npRtAllocTransactionId(rt->rtSessionHandle, &rtTxId);
		if (ret >= 0) {
			ret = npImpsRtTxMarkInFlight(&rt->txQueue,
						     reinterpret_cast<NpImpsRtTxNode *>(txHdr),
						     rtTxId);
			if (ret >= 0) {
				ret = npRtImpsDeleteStored(rt->rtSessionHandle, rtTxId,
							   storedId, callback);
				if (ret >= 0)
					return 0;
			}
		}
	}
	if (txHdr != NULL)
		npImpsRtTxFailPendingById(&rt->txQueue, requestId, ret);
	npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
	return ret;
}

/* 0x81318B80 */
static int npImpsRtFlushCompletedTx(NpImpsRtBridge *rt, const NpImpsSessionIdentity *identity)
{
	while (true) {
		NpImpsTxNodeHeader *hdr = NULL;
		int ret = npImpsTxQueuePopCompleted(&rt->txQueue, &hdr);
		if (ret < 0)
			return ret;
		if (hdr == NULL)
			return 0;
		NpImpsRtTxNode *node = reinterpret_cast<NpImpsRtTxNode *>(hdr);
		if (node->callback != NULL)
			node->callback(identity, node->header.requestId, node->resultCode,
				       node->userArg);
		npImpsRtTxFree(&rt->txQueue, node);
	}
}

/* 0x81318BDA */
static int npImpsRtHandleConnectEvent(NpImpsRtBridge *rt, const NpImpsRtEventPayload *ev,
				      const NpImpsSessionIdentity *identity,
				      SceUInt8 *outSidChanged)
{
	*outSidChanged = 0;
	int ret = 0;

	if (ev->eventKind == NP_IMPS_RT_EVT_STATE) {
		int rtEventState = ev->detail.status.stateOrTxId;
		if (rtEventState == NP_RT_EVENT_CONNECTED) {
			npImpsRtSetState(rt, NP_IMPS_RT_CONNECTED);
			if (ev->sessionSid[0] != '\0') {
				if (rt->sessionSid[0] == '\0' ||
				    sceClibMemcmp(rt->sessionSid, ev->sessionSid,
						  sizeof(rt->sessionSid)) != 0) {
					sceClibMemcpy_safe(rt->sessionSid, ev->sessionSid,
							   sizeof(rt->sessionSid));
					*outSidChanged = 1;
				}
			} else {
				ret = NP_IMPS_CORE_ERR_NO_SESSION_SID;
			}
		} else if (rtEventState == NP_RT_EVENT_CONNECTING) {
			ret = 0;
		} else if (rtEventState == NP_RT_EVENT_STOPPED) {
			ret = (ev->detail.status.errorOrResult < 0)
				? ev->detail.status.errorOrResult
				: NP_IMPS_CORE_ERR_STOPPED;
		} else if (rtEventState == NP_RT_EVENT_ERROR) {
			ret = ev->detail.status.errorOrResult;
		} else {
			ret = NP_IMPS_CORE_ERR_PROTOCOL;
		}
	}

	if (ret >= 0)
		ret = npImpsRtFlushCompletedTx(rt, identity);
	if (ret < 0) {
		npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
		return ret;
	}
	return 0;
}

/* 0x81318D38 */
static int npImpsRtHandleActiveEvent(NpImpsRtBridge *rt, const NpImpsRtEventPayload *ev,
				     const NpImpsSessionIdentity *identity)
{
	int ret = 0;

	switch (ev->eventKind) {
	case NP_IMPS_RT_EVT_STATE:
	case NP_IMPS_RT_EVT_STANDBY_DONE:
		if (rt->rtState == NP_IMPS_RT_CONNECTED) {
			if (ev->eventKind == NP_IMPS_RT_EVT_STATE) {
				int st = ev->detail.status.stateOrTxId;
				if (st == NP_RT_EVENT_ERROR || st == NP_RT_EVENT_STOPPED) {
					ret = (ev->detail.status.errorOrResult < 0)
						? ev->detail.status.errorOrResult
						: NP_IMPS_CORE_ERR_STOPPED;
				} else {
					ret = NP_IMPS_CORE_ERR_PROTOCOL;
				}
			}
		} else if (rt->rtState == NP_IMPS_RT_SETTING_STANDBY) {
			if (ev->eventKind == NP_IMPS_RT_EVT_STATE) {
				int st = ev->detail.status.stateOrTxId;
				if (st == NP_RT_EVENT_ERROR || st == NP_RT_EVENT_STOPPED) {
					ret = (ev->detail.status.errorOrResult < 0)
						? ev->detail.status.errorOrResult
						: NP_IMPS_CORE_ERR_STOPPED;
				} else {
					ret = NP_IMPS_CORE_ERR_PROTOCOL;
				}
			} else if (rt->pendingStandbyTxId == ev->detail.status.stateOrTxId) {
				rt->pendingStandbyTxId = 0;
				ret = ev->detail.status.errorOrResult;
				if (ret >= 0)
					ret = npRtDisconnectSession(rt->rtSessionHandle);
				if (ret >= 0)
					npImpsRtSetState(rt, NP_IMPS_RT_DISCONNECTING);
			}
		} else if (rt->rtState == NP_IMPS_RT_DISCONNECTING) {
			if (ev->eventKind == NP_IMPS_RT_EVT_STATE) {
				int st = ev->detail.status.stateOrTxId;
				if (st == NP_RT_EVENT_CLOSING) {
					ret = 0;
				} else if (st == NP_RT_EVENT_ERROR || st == NP_RT_EVENT_STOPPED) {
					ret = ev->detail.status.errorOrResult;
					if (ret >= 0)
						npImpsRtSetState(rt, NP_IMPS_RT_DISCONNECTED);
				} else {
					ret = NP_IMPS_CORE_ERR_PROTOCOL;
				}
			}
		} else {
			ret = NP_IMPS_CORE_ERR_PROTOCOL;
		}
		break;

	case NP_IMPS_RT_EVT_ROSTER_PUSH:
		npImpsSessionNotifyRoster(identity, 1, ev->detail.rosterPush);
		break;

	case NP_IMPS_RT_EVT_ROSTER_GET_DONE:
		if (ev->detail.status.stateOrTxId == rt->pendingRosterTxId) {
			ret = ev->detail.status.errorOrResult;
			if (ret >= 0)
				npImpsSessionNotifyRoster(identity, 0,
							  ev->detail.status.rosterList);
		}
		break;

	case NP_IMPS_RT_EVT_PRIVACY_PUSH:
		npImpsSessionNotifyPrivacy(identity, 0, ev->detail.privacyPush);
		break;

	case NP_IMPS_RT_EVT_PRESENCE_RECV:
		npImpsSessionNotifyPresence(identity, ev->detail.presenceStanza);
		break;

	case NP_IMPS_RT_EVT_MESSAGE_RECV:
		npImpsSessionNotifyMessage(identity, ev->detail.messageStanza);
		break;

	case NP_IMPS_RT_EVT_PRESENCE_SENT:
	case NP_IMPS_RT_EVT_MESSAGE_SENT:
	case NP_IMPS_RT_EVT_STORED_DELETED:
		ret = npImpsRtTxCompleteByRtId(&rt->txQueue, ev->detail.status.stateOrTxId,
					       ev->detail.status.errorOrResult);
		break;

	default:
		break;
	}

	if (ret >= 0)
		ret = npImpsRtFlushCompletedTx(rt, identity);
	if (ret < 0) {
		npImpsRtSetState(rt, NP_IMPS_RT_ERROR);
		return ret;
	}
	return 0;
}

/* ============================================================================
 * Embedded Polling sub-controller (0x81319000..0x813192E7)
 * ============================================================================ */

static void npImpsPollInit(NpImpsPollBridge *poll)
{
	sceClibMemset(poll, 0, sizeof(*poll));
	poll->pollSessionId = 0;
	poll->pollState = NP_IMPS_POLL_IDLE;
	poll->statusConfig.intervalMs = 0;
	poll->statusConfig.autoIntervalMs = 0;
	poll->statusConfig.autoIntervalPolicy = 0;
}

static void npImpsPollDestroyHandle(NpImpsPollBridge *poll)
{
	if (poll->pollSessionId != 0) {
		npPollingDestroySession(poll->pollSessionId);
		poll->pollSessionId = 0;
	}
}

static void npImpsPollSetState(NpImpsPollBridge *poll, int newState)
{
	poll->pollState = newState;
	if (newState == NP_IMPS_POLL_ERROR) {
		npImpsPollSetState(poll, NP_IMPS_POLL_DISCONNECTED);
		return;
	}
	if (newState == NP_IMPS_POLL_DISCONNECTED)
		npImpsPollDestroyHandle(poll);
}

static void npImpsPollForceClose(NpImpsPollBridge *poll)
{
	if (poll->pollState != NP_IMPS_POLL_DISCONNECTED)
		npImpsPollSetState(poll, NP_IMPS_POLL_ERROR);
}

static void npImpsPollResetState(NpImpsPollBridge *poll)
{
	sceClibMemset(&poll->timestamps, 0, sizeof(poll->timestamps));
	poll->statusConfig.intervalMs = 0;
	poll->statusConfig.autoIntervalMs = 0;
	poll->statusConfig.autoIntervalPolicy = 0;
}

static void npImpsPollConfigure(NpImpsPollBridge *poll, const SceNpId *npId,
				const NpPollingEndpoint *endpoint)
{
	sceClibMemcpy_safe(&poll->npId, npId, sizeof(poll->npId));
	sceClibMemcpy_safe(&poll->endpoint, endpoint, sizeof(poll->endpoint));
}

/* 0x813190A6 */
static int npImpsPollConnect(NpImpsPollBridge *poll, int checkInitialStatus, int pollingMode,
			     const char *sessionSid)
{
	int pollId = 0;
	int ret = npPollingCreateSession(&poll->npId, &poll->endpoint, &poll->timestamps,
					 &poll->statusConfig, &pollId);
	if (ret >= 0) {
		poll->pollSessionId = pollId;
		pollId = 0;
		ret = npPollingStartSession(poll->pollSessionId, checkInitialStatus,
					    pollingMode, sessionSid);
	}
	if (ret < 0) {
		if (pollId != 0)
			npPollingDestroySession(pollId);
		npImpsPollSetState(poll, NP_IMPS_POLL_ERROR);
		return ret;
	}
	npImpsPollSetState(poll, NP_IMPS_POLL_CONNECTING);
	return 0;
}

/* 0x8131912E */
static int npImpsPollGetServerStatus(NpImpsPollBridge *poll, int *outServerStatus)
{
	int ret = npPollingGetServerStatus(poll->pollSessionId, outServerStatus);
	if (ret < 0) {
		npImpsPollSetState(poll, NP_IMPS_POLL_ERROR);
		return ret;
	}
	return 0;
}

/* 0x8131914E */
static int npImpsPollUpdateMode(NpImpsPollBridge *poll, int pollingMode, const char *sessionSid)
{
	int ret = npPollingUpdateMode(poll->pollSessionId, pollingMode, sessionSid);
	if (ret < 0) {
		npImpsPollSetState(poll, NP_IMPS_POLL_ERROR);
		return ret;
	}
	return 0;
}

/* 0x8131916E */
static int npImpsPollDisconnect(NpImpsPollBridge *poll)
{
	int ret = npPollingDestroySession(poll->pollSessionId);
	if (ret < 0) {
		npImpsPollSetState(poll, NP_IMPS_POLL_ERROR);
		return ret;
	}
	poll->pollSessionId = 0;
	npImpsPollSetState(poll, NP_IMPS_POLL_DISCONNECTED);
	return 0;
}

/* 0x8131919A */
static int npImpsPollProcessStateEvent(NpImpsPollBridge *poll, const NpImpsPollEventPayload *ev)
{
	if (ev->eventKind != NP_IMPS_POLL_EVT_STATE)
		return NP_IMPS_CORE_ERR_PROTOCOL;

	int stateCode = ev->detail.state.stateCode;
	if (stateCode == NP_POLLING_EVENT_STOPPED) {
		return (ev->detail.state.resultCode < 0)
			? ev->detail.state.resultCode
			: NP_IMPS_CORE_ERR_STOPPED;
	}
	if (stateCode == NP_POLLING_EVENT_CONNECTED) {
		int ret = npPollingGetStatusConfig(poll->pollSessionId, &poll->statusConfig);
		if (ret < 0)
			return ret;
		npImpsPollSetState(poll, NP_IMPS_POLL_CONNECTED);
		return 0;
	}
	if (stateCode == NP_POLLING_EVENT_CONNECTING)
		return 0;
	if (stateCode == NP_POLLING_EVENT_ERROR)
		return ev->detail.state.resultCode;
	return NP_IMPS_CORE_ERR_PROTOCOL;
}

/* 0x81319200 */
static int npImpsPollHandleConnectEvent(NpImpsPollBridge *poll, const NpImpsPollEventPayload *ev)
{
	if (ev->eventKind != NP_IMPS_POLL_EVT_STATE)
		return 0;
	int ret = npImpsPollProcessStateEvent(poll, ev);
	if (ret < 0) {
		npImpsPollSetState(poll, NP_IMPS_POLL_ERROR);
		return ret;
	}
	return 0;
}

/* 0x81319224 */
static int npImpsPollHandleActiveEvent(NpImpsPollBridge *poll, const NpImpsPollEventPayload *ev,
				       int *outDiffBits)
{
	int diffBits = 0;
	*outDiffBits = 0;

	if (ev->eventKind == NP_IMPS_POLL_EVT_UPDATE) {
		if (ev->detail.update.updateKind == 0) {
			if (poll->timestamps.friendListTick.tick !=
			    ev->detail.update.timestamps.friendListTick.tick) {
				diffBits |= NP_IMPS_POLL_DIFF_FRIEND_LIST;
			}
			if (poll->timestamps.blockListTick.tick !=
			    ev->detail.update.timestamps.blockListTick.tick) {
				diffBits |= NP_IMPS_POLL_DIFF_BLOCK_LIST;
			}
			if (poll->timestamps.messageTick.tick !=
			    ev->detail.update.timestamps.messageTick.tick) {
				diffBits |= NP_IMPS_POLL_DIFF_MESSAGE;
			}
			*outDiffBits = diffBits;
			poll->timestamps = ev->detail.update.timestamps;
		}
		return 0;
	}

	if (ev->eventKind == NP_IMPS_POLL_EVT_STATE) {
		if (poll->pollState == NP_IMPS_POLL_DISCONNECTED)
			return 0;
		if (poll->pollState == NP_IMPS_POLL_CONNECTING) {
			int ret = npImpsPollProcessStateEvent(poll, ev);
			if (ret < 0) {
				npImpsPollSetState(poll, NP_IMPS_POLL_ERROR);
				return ret;
			}
			return 0;
		}
		if (ev->detail.state.stateCode == NP_POLLING_EVENT_ERROR) {
			int err = ev->detail.state.resultCode;
			npImpsPollSetState(poll, NP_IMPS_POLL_ERROR);
			return err;
		}
	}
	return 0;
}

/* ============================================================================
 * Session account config & lifecycle (0x81317704..0x8131848B)
 * ============================================================================ */

static void npImpsSessionAbortAccountTimeout(void *arg)
{
	NpImpsSession *session = reinterpret_cast<NpImpsSession *>(arg);
	int lockState = npRtLockLwMutex(&session->mutex);
	if (session->accountReqHandle != -1)
		npAccountAbortRequest(session->accountReqHandle);
	npRtUnlockLwMutex(&session->mutex, lockState);
}

/* 0x813181E0 */
static int npImpsSessionFetchNpId(NpImpsSession *session, SceNpId *outNpId)
{
	int reqHandle = -1;
	int lockState = npRtLockLwMutex(&session->mutex);
	if (lockState < 0)
		return lockState;

	int ret = 0;
	if (session->aborted != 0) {
		ret = NP_IMPS_CORE_ERR_ABORTED;
	} else {
		reqHandle = npAccountCreateRequest();
		if (reqHandle < 0) {
			ret = reqHandle;
			reqHandle = -1;
		} else {
			session->accountReqHandle = reqHandle;
			npRtUnlockLwMutex(&session->mutex, lockState);
			lockState = 0;

			ret = npAccountGetNpIdForce(reqHandle, outNpId);
			if (ret >= 0) {
				lockState = npRtLockLwMutex(&session->mutex);
				if (lockState >= 0) {
					npAccountDestroyRequest(reqHandle);
					reqHandle = -1;
					session->accountReqHandle = -1;
					npRtUnlockLwMutex(&session->mutex, lockState);
					lockState = 0;
					ret = 0;
				} else {
					ret = lockState;
				}
			}
			if (lockState <= 0)
				lockState = npRtLockLwMutex(&session->mutex);
		}
	}

	if (reqHandle != -1)
		npAccountDestroyRequest(reqHandle);
	npRtUnlockLwMutex(&session->mutex, lockState);
	return ret;
}

/* 0x813182AE */
static int npImpsSessionFetchConfig(NpImpsSession *session)
{
	SceNpId npId;
	char npEnv[18];
	NpPollingEndpoint pollEndpoint;
	NpRtServerConfig xmppConfig;

	int ret = npRtCalloutArm(&session->timeoutTimer, NP_IMPS_CONFIG_TIMEOUT_USEC,
				 npImpsSessionAbortAccountTimeout, session);
	if (ret >= 0)
		ret = npImpsSessionFetchNpId(session, &npId);
	if (ret >= 0)
		ret = sceNpCommonGetNpEnviroment(npEnv);
	if (ret >= 0) {
		const SceUInt8 *opt = npId.opt;
		sceClibMemset(&pollEndpoint, 0, sizeof(pollEndpoint));
		sceClibSnprintf(pollEndpoint.host, sizeof(pollEndpoint.host),
				"communication-polling.%c%c.%s.communication.playstation.net",
				opt[2], opt[3], npEnv);
		pollEndpoint.port = NP_IMPS_POLLING_PORT;
		npImpsPollConfigure(&session->poll, &npId, &pollEndpoint);

		sceClibMemset(&xmppConfig, 0, sizeof(xmppConfig));
		sceClibSnprintf(xmppConfig.host, sizeof(xmppConfig.host),
				"%c%c.%c%c.%s.communication.playstation.net",
				opt[0], opt[1], opt[2], opt[3], npEnv);
		sceClibSnprintf(xmppConfig.domain, sizeof(xmppConfig.domain),
				"%c%c.%c%c.%s.playstation.net",
				opt[0], opt[1], opt[2], opt[3], npEnv);
		xmppConfig.port = NP_IMPS_XMPP_PORT;
		npImpsRtConfigure(&session->rt, &npId.handle, &xmppConfig);
		ret = 0;
	}

	if (ret < 0 && npRtCalloutIsFired(&session->timeoutTimer) != 0)
		ret = NP_IMPS_CORE_ERR_CONFIG_TIMEOUT;

	npRtCalloutCancel(&session->timeoutTimer);
	return ret;
}

/* 0x8131841A */
static int npImpsSessionGetAutoIntervalSec(const NpImpsSession *session)
{
	const NpPollingStatusConfig *cfg = &session->poll.statusConfig;
	if (cfg->autoIntervalMs == 0 && cfg->intervalMs == 0 && cfg->autoIntervalPolicy == 0)
		return NP_IMPS_DEFAULT_AUTO_INTERVAL_SEC;
	if (cfg->autoIntervalPolicy <= 0)
		return -1;
	return cfg->autoIntervalPolicy / 1000;
}

/* 0x81318458 */
static void npImpsSessionAbort(NpImpsSession *session)
{
	int lockState = npRtLockLwMutex(&session->mutex);
	session->aborted = 1;
	if (session->accountReqHandle != -1)
		npAccountAbortRequest(session->accountReqHandle);
	npImpsSessionSendWakeup(&session->msgPipeId);
	npRtUnlockLwMutex(&session->mutex, lockState);
}

static void npImpsSessionJoinWorker(NpImpsSession *session)
{
	if (session->workerThreadPtr != NULL) {
		npRtWaitThreadEnd(session->workerThreadPtr);
		session->workerThreadPtr = NULL;
	}
}

/* 0x8131774A */
static void npImpsSessionFree(NpImpsSession *session)
{
	npImpsSessionAbort(session);
	npImpsSessionJoinWorker(session);
	npImpsRtDestroyHandle(&session->rt);
	npImpsPollDestroyHandle(&session->poll);
	npRtCalloutCancel(&session->timeoutTimer);
	npImpsSessionPipeDestroy(&session->msgPipeId);
	sceKernelDeleteLwMutex(&session->mutex);
	if (session->desiredPresence != NULL)
		npRtReleasePresenceStanza(session->desiredPresence);
	if (session->pendingPresence != NULL)
		npRtReleasePresenceStanza(session->pendingPresence);
	npRtHeapFree(session, NULL);
}

/* 0x813177CC */
static int npImpsSessionCreate(NpImpsSession **outSession)
{
	if (s_sessionMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	NpImpsSession *session = reinterpret_cast<NpImpsSession *>(
		npRtHeapAlloc(sizeof(NpImpsSession), NULL));
	if (session == NULL)
		return NP_IMPS_CORE_ERR_NO_MEMORY;

	sceClibMemset(session, 0, sizeof(*session));
	int ret = sceKernelCreateLwMutex(&session->mutex, "SceNpImpsSession", 0x2002u, 0, NULL);
	if (ret < 0) {
		npRtHeapFree(session, NULL);
		return ret;
	}

	session->state = NP_IMPS_SESS_IDLE;
	ret = npImpsSessionPipeCreate(&session->msgPipeId);
	if (ret < 0) {
		sceKernelDeleteLwMutex(&session->mutex);
		npRtHeapFree(session, NULL);
		return ret;
	}

	npRtCalloutInit(&session->timeoutTimer);
	session->accountReqHandle = -1;
	npImpsRtInit(&session->rt, &session->mutex);
	s_nextSessionId++;
	if (s_nextSessionId == 0)
		s_nextSessionId++;
	session->identity.sessionId = s_nextSessionId;
	npImpsPollInit(&session->poll);

	session->refCount++;

	int globalLock = npImpsCoreMutexLock();
	if (globalLock < 0) {
		npImpsSessionFree(session);
		return globalLock;
	}

	for (NpImpsSession *cur = s_sessionListHead; cur != NULL; cur = cur->next) {
		if (sceClibStrncasecmp(session->identity.npId.handle.data,
				       cur->identity.npId.handle.data, 16u) == 0) {
			npImpsSessionFree(session);
			npImpsCoreMutexUnlock(globalLock);
			return NP_IMPS_CORE_ERR_SESSION_EXISTS;
		}
	}

	s_sessionCount++;
	int sessLock = npRtLockLwMutex(&session->mutex);
	if (sessLock < 0) {
		npImpsSessionFree(session);
		npImpsCoreMutexUnlock(globalLock);
		return sessLock;
	}

	session->prev = NULL;
	session->next = s_sessionListHead;
	if (s_sessionListHead != NULL)
		s_sessionListHead->prev = session;
	if (s_sessionListTail == NULL)
		s_sessionListTail = session;
	s_sessionListHead = session;
	session->flags |= NP_IMPS_SESSION_FLAG_LINKED;

	npRtUnlockLwMutex(&session->mutex, sessLock);
	npImpsCoreMutexUnlock(globalLock);
	*outSession = session;
	return 0;
}

static inline int npImpsSessionAcquireFound(NpImpsSession *found, NpImpsSession **outSession,
					    SceUInt8 allowClosing)
{
	if (found == NULL)
		return NP_IMPS_CORE_ERR_SESSION_NOT_FOUND;

	int ret = 0;
	int sessLock = npRtLockLwMutex(&found->mutex);
	if (sessLock >= 0) {
		if (allowClosing != 0 || (found->flags & NP_IMPS_SESSION_FLAG_CLOSING) == 0) {
			found->refCount++;
			*outSession = found;
		} else {
			ret = NP_IMPS_CORE_ERR_SESSION_NOT_FOUND;
		}
		npRtUnlockLwMutex(&found->mutex, sessLock);
	} else {
		ret = sessLock;
	}
	return ret;
}

/* 0x813179CE */
static int npImpsSessionAcquireById(int sessionId, NpImpsSession **outSession,
				    SceUInt8 allowClosing)
{
	if (s_sessionMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	int globalLock = npImpsCoreMutexLock();
	if (globalLock < 0)
		return globalLock;

	NpImpsSession *found = NULL;
	for (NpImpsSession *cur = s_sessionListHead; cur != NULL; cur = cur->next) {
		if (cur->identity.sessionId == sessionId) {
			found = cur;
			break;
		}
	}

	int ret = npImpsSessionAcquireFound(found, outSession, allowClosing);
	npImpsCoreMutexUnlock(globalLock);
	return ret;
}

/* 0x81317A72 */
static int npImpsSessionAcquireByRtHandle(int rtSessionHandle, NpImpsSession **outSession,
					  SceUInt8 allowClosing)
{
	if (s_sessionMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	int globalLock = npImpsCoreMutexLock();
	if (globalLock < 0)
		return globalLock;

	NpImpsSession *found = NULL;
	for (NpImpsSession *cur = s_sessionListHead; cur != NULL; cur = cur->next) {
		if (cur->rt.rtSessionHandle == rtSessionHandle) {
			found = cur;
			break;
		}
	}

	int ret = npImpsSessionAcquireFound(found, outSession, allowClosing);
	npImpsCoreMutexUnlock(globalLock);
	return ret;
}

/* 0x81317B18 */
static int npImpsSessionAcquireByPollId(int pollSessionId, NpImpsSession **outSession,
					SceUInt8 allowClosing)
{
	if (s_sessionMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	int globalLock = npImpsCoreMutexLock();
	if (globalLock < 0)
		return globalLock;

	NpImpsSession *found = NULL;
	for (NpImpsSession *cur = s_sessionListHead; cur != NULL; cur = cur->next) {
		if (cur->poll.pollSessionId == pollSessionId) {
			found = cur;
			break;
		}
	}

	int ret = npImpsSessionAcquireFound(found, outSession, allowClosing);
	npImpsCoreMutexUnlock(globalLock);
	return ret;
}

static inline int npImpsSessionReleaseCore(NpImpsSession *session, bool markClosing)
{
	if (s_sessionMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	int globalLock = npImpsCoreMutexLock();
	if (globalLock < 0)
		return globalLock;

	NpImpsSession *toFree = NULL;
	int ret = 0;
	int sessLock = npRtLockLwMutex(&session->mutex);
	if (sessLock >= 0) {
		if (markClosing)
			session->flags |= NP_IMPS_SESSION_FLAG_CLOSING;
		session->refCount--;
		if (session->refCount == 0 && (session->flags & NP_IMPS_SESSION_FLAG_CLOSING) != 0) {
			if ((session->flags & NP_IMPS_SESSION_FLAG_LINKED) != 0) {
				if (session->next != NULL)
					session->next->prev = session->prev;
				else
					s_sessionListTail = session->prev;
				if (session->prev != NULL)
					session->prev->next = session->next;
				else
					s_sessionListHead = session->next;
				session->next = NULL;
				session->prev = NULL;
				session->flags &= ~NP_IMPS_SESSION_FLAG_LINKED;
			}
			toFree = session;
		}
		npRtUnlockLwMutex(&session->mutex, sessLock);
	} else {
		ret = sessLock;
	}

	npImpsCoreMutexUnlock(globalLock);
	if (toFree != NULL)
		npImpsSessionFree(toFree);
	return ret;
}

/* 0x81317BBE */
static int npImpsSessionRelease(NpImpsSession *session)
{
	return npImpsSessionReleaseCore(session, false);
}

/* 0x81317C82 */
static int npImpsSessionClose(NpImpsSession *session)
{
	return npImpsSessionReleaseCore(session, true);
}

/* 0x81317D4E */
static int npImpsSessionStartWorker(NpImpsSession *session)
{
	session->aborted = 0;
	int ret = npRtStartThread(&s_coreMainThreadId, session);
	if (ret >= 0) {
		session->workerThreadPtr = &s_coreMainThreadId;
		return 0;
	}
	return ret;
}

/* ============================================================================
 * SceNpImpsCoreMain worker thread & callbacks (0x8131848C..0x813186C3)
 * ============================================================================ */

/* 0x813184FA */
static int npImpsCoreMainThreadEntry(SceSize args, void *argp)
{
	(void)args;
	NpImpsSession *session = *reinterpret_cast<NpImpsSession **>(argp);
	SceUID threadId = sceKernelGetThreadId();
	if (SceNetInternal_9DBD2F60(threadId, 1, 0) >= 0)
		npImpsSessionMainLoop(session);
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

/* 0x8131848C */
static int npImpsCoreCreateMainThread(SceUID *outThreadId, const NpPollingThreadParam *threadParam)
{
	SceUID tid = sceKernelCreateThread("SceNpImpsCoreMain", npImpsCoreMainThreadEntry,
					   threadParam->priority, threadParam->stackSize,
					   0, threadParam->cpuAffinityMask, NULL);
	if (tid >= 0) {
		*outThreadId = tid;
		return 0;
	}
	if (tid != -1)
		sceKernelDeleteThread(tid);
	return tid;
}

/* 0x81317D7A */
static int npImpsSessionMgrInit(const NpPollingThreadParam *threadParam)
{
	if (s_sessionMgrInit != 0)
		return NP_IMPS_CORE_ERR_ALREADY_INIT;

	s_sessionMgrInit = 1;
	s_sessionListHead = NULL;
	s_sessionListTail = NULL;
	s_sessionCount = 0;
	s_nextSessionId = 0;

	int ret = npImpsCoreCreateMainThread(&s_coreMainThreadId, threadParam);
	return (ret >= 0) ? 0 : ret;
}

static int npImpsSessionMgrTerm(void)
{
	if (s_sessionMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	while (s_sessionListHead != NULL)
		npImpsSessionClose(s_sessionListHead);

	npRtJoinAndDeleteThread(&s_coreMainThreadId);
	s_sessionCount = 0;
	s_sessionMgrInit = 0;
	return 0;
}

/* Polling callbacks -> post NP_IMPS_MSG_POLL_EVENT (0x81319884, 0x81319908) */
/* 0x81319884 */
static void npImpsPostPollingState(const NpPollingSessionIdentity *identity, int eventState,
				   int resultCode)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByPollId(identity->sessionId, &session, 1) >= 0) {
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_POLL_EVENT;
		msg.body.pollEvent.identity = *identity;
		msg.body.pollEvent.eventKind = NP_IMPS_POLL_EVT_STATE;
		msg.body.pollEvent.detail.state.stateCode = eventState;
		msg.body.pollEvent.detail.state.resultCode = resultCode;
		npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x813185BE */
static void npImpsOnPollingState(const NpPollingSessionIdentity *identity, int eventState,
				 int resultCode)
{
	npImpsPostPollingState(identity, eventState, resultCode);
}

/* 0x81319908 */
static void npImpsPostPollingUpdate(const NpPollingSessionIdentity *identity, int updateKind,
				    const NpPollingTimestampSet *timestamps)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByPollId(identity->sessionId, &session, 0) >= 0) {
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_POLL_EVENT;
		msg.body.pollEvent.identity = *identity;
		msg.body.pollEvent.eventKind = NP_IMPS_POLL_EVT_UPDATE;
		msg.body.pollEvent.detail.update.updateKind = updateKind;
		if (timestamps != NULL)
			msg.body.pollEvent.detail.update.timestamps = *timestamps;
		npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x813185C6 */
static void npImpsOnPollingUpdate(const NpPollingSessionIdentity *identity, int updateKind,
				  const NpPollingTimestampSet *timestamps)
{
	npImpsPostPollingUpdate(identity, updateKind, timestamps);
}

/* RT / Basic callbacks -> post NP_IMPS_MSG_RT_EVENT (0x8131AA80..0x8131B00F) */
static void npImpsCopyRtHeader(NpImpsRtEventPayload *dst, const NpRtSessionIdentity *rtSession)
{
	dst->rtSessionId = rtSession->sessionId;
	dst->onlineId = rtSession->onlineId;
	sceClibMemcpy_safe(dst->sessionSid, rtSession->sessionSid, sizeof(dst->sessionSid));
}

/* 0x8131AA80 */
static void npImpsPostRtStateEvent(const NpRtSessionIdentity *rtSession, int eventState,
				   int resultCode)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByRtHandle(rtSession->sessionId, &session, 1) >= 0) {
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_RT_EVENT;
		npImpsCopyRtHeader(&msg.body.rtEvent, rtSession);
		msg.body.rtEvent.eventKind = NP_IMPS_RT_EVT_STATE;
		msg.body.rtEvent.detail.status.stateOrTxId = eventState;
		msg.body.rtEvent.detail.status.errorOrResult = resultCode;
		npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x8131869C */
static void npImpsOnRtState(const NpRtSessionIdentity *rtSession, int eventState, int resultCode)
{
	npImpsPostRtStateEvent(rtSession, eventState, resultCode);
}

/* 0x8131AD56 */
static void npImpsPostRtStandbyDone(const NpRtSessionIdentity *rtSession, int txId, int result)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByRtHandle(rtSession->sessionId, &session, 1) >= 0) {
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_RT_EVENT;
		npImpsCopyRtHeader(&msg.body.rtEvent, rtSession);
		msg.body.rtEvent.eventKind = NP_IMPS_RT_EVT_STANDBY_DONE;
		msg.body.rtEvent.detail.status.stateOrTxId = txId;
		msg.body.rtEvent.detail.status.errorOrResult = result;
		npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x81319B6A */
static void npImpsOnRtStandbyDone(const NpRtSessionIdentity *rtSession, int txId, int result,
				  void *userArg)
{
	(void)userArg;
	npImpsPostRtStandbyDone(rtSession, txId, result);
}

/* 0x8131AE82 */
static void npImpsPostRtPresenceSent(const NpRtSessionIdentity *rtSession, int txId, int result)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByRtHandle(rtSession->sessionId, &session, 1) >= 0) {
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_RT_EVENT;
		npImpsCopyRtHeader(&msg.body.rtEvent, rtSession);
		msg.body.rtEvent.eventKind = NP_IMPS_RT_EVT_PRESENCE_SENT;
		msg.body.rtEvent.detail.status.stateOrTxId = txId;
		msg.body.rtEvent.detail.status.errorOrResult = result;
		npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x81319B7A */
static void npImpsOnRtPresenceSent(const NpRtSessionIdentity *rtSession, int txId, int result,
				   void *userArg)
{
	(void)userArg;
	npImpsPostRtPresenceSent(rtSession, txId, result);
}

/* 0x8131AF06 */
static void npImpsPostRtMessageSent(const NpRtSessionIdentity *rtSession, int txId, int result)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByRtHandle(rtSession->sessionId, &session, 1) >= 0) {
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_RT_EVENT;
		npImpsCopyRtHeader(&msg.body.rtEvent, rtSession);
		msg.body.rtEvent.eventKind = NP_IMPS_RT_EVT_MESSAGE_SENT;
		msg.body.rtEvent.detail.status.stateOrTxId = txId;
		msg.body.rtEvent.detail.status.errorOrResult = result;
		npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x81319B82 */
static void npImpsOnRtMessageSent(const NpRtSessionIdentity *rtSession, int txId, int result,
				  void *userArg)
{
	(void)userArg;
	npImpsPostRtMessageSent(rtSession, txId, result);
}

/* 0x8131AF8A */
static void npImpsPostRtStoredDeleted(const NpRtSessionIdentity *rtSession, int txId, int result)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByRtHandle(rtSession->sessionId, &session, 1) >= 0) {
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_RT_EVENT;
		npImpsCopyRtHeader(&msg.body.rtEvent, rtSession);
		msg.body.rtEvent.eventKind = NP_IMPS_RT_EVT_STORED_DELETED;
		msg.body.rtEvent.detail.status.stateOrTxId = txId;
		msg.body.rtEvent.detail.status.errorOrResult = result;
		npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x81319B8A */
static void npImpsOnRtStoredDeleted(const NpRtSessionIdentity *rtSession, int txId, int result,
				    void *userArg)
{
	(void)userArg;
	npImpsPostRtStoredDeleted(rtSession, txId, result);
}

/* 0x8131ADDA */
static void npImpsPostRtRosterGetDone(const NpRtSessionIdentity *rtSession, int txId, int result,
				      NpBasicRosterList *rosterList)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByRtHandle(rtSession->sessionId, &session, 1) >= 0) {
		NpBasicRosterList *retained = (rosterList != NULL)
			? reinterpret_cast<NpBasicRosterList *>(npRtAddRefStanza(rosterList))
			: NULL;
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_RT_EVENT;
		npImpsCopyRtHeader(&msg.body.rtEvent, rtSession);
		msg.body.rtEvent.eventKind = NP_IMPS_RT_EVT_ROSTER_GET_DONE;
		msg.body.rtEvent.detail.status.stateOrTxId = txId;
		msg.body.rtEvent.detail.status.errorOrResult = result;
		msg.body.rtEvent.detail.status.rosterList = retained;
		if (npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL) < 0 && retained != NULL)
			npBasicReleaseRosterList(retained);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x81319B72 */
static void npImpsOnRtRosterGetDone(const NpRtSessionIdentity *rtSession, int txId, int result,
				    NpBasicRosterList *rosterList)
{
	npImpsPostRtRosterGetDone(rtSession, txId, result, rosterList);
}

/* 0x8131AB04 */
static void npImpsPostBasicRosterPush(const NpRtSessionIdentity *rtSession,
				      NpBasicRosterList *rosterList)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByRtHandle(rtSession->sessionId, &session, 1) >= 0) {
		NpBasicRosterList *retained = reinterpret_cast<NpBasicRosterList *>(
			npRtAddRefStanza(rosterList));
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_RT_EVENT;
		npImpsCopyRtHeader(&msg.body.rtEvent, rtSession);
		msg.body.rtEvent.eventKind = NP_IMPS_RT_EVT_ROSTER_PUSH;
		msg.body.rtEvent.detail.rosterPush = retained;
		if (npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL) < 0 && retained != NULL)
			npBasicReleaseRosterList(rosterList);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x813186A4 */
static void npImpsOnBasicRosterPush(const NpRtSessionIdentity *rtSession,
				    NpBasicRosterList *rosterList)
{
	npImpsPostBasicRosterPush(rtSession, rosterList);
}

/* 0x8131AB98 */
static void npImpsPostBasicPrivacyPush(const NpRtSessionIdentity *rtSession,
				       NpBasicPrivacyList *privacyList)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByRtHandle(rtSession->sessionId, &session, 1) >= 0) {
		NpBasicPrivacyList *retained = reinterpret_cast<NpBasicPrivacyList *>(
			npRtAddRefStanza(privacyList));
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_RT_EVENT;
		npImpsCopyRtHeader(&msg.body.rtEvent, rtSession);
		msg.body.rtEvent.eventKind = NP_IMPS_RT_EVT_PRIVACY_PUSH;
		msg.body.rtEvent.detail.privacyPush = retained;
		if (npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL) < 0 && retained != NULL)
			npBasicReleasePrivacyList(privacyList);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x813186AC */
static void npImpsOnBasicPrivacyPush(const NpRtSessionIdentity *rtSession,
				     NpBasicPrivacyList *privacyList)
{
	npImpsPostBasicPrivacyPush(rtSession, privacyList);
}

/* 0x8131AC2C */
static void npImpsPostBasicMessageRecv(const NpRtSessionIdentity *rtSession,
				       NpRtMessageStanza *stanza)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByRtHandle(rtSession->sessionId, &session, 1) >= 0) {
		NpRtMessageStanza *retained = reinterpret_cast<NpRtMessageStanza *>(
			npRtAddRefStanza(stanza));
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_RT_EVENT;
		npImpsCopyRtHeader(&msg.body.rtEvent, rtSession);
		msg.body.rtEvent.eventKind = NP_IMPS_RT_EVT_MESSAGE_RECV;
		msg.body.rtEvent.detail.messageStanza = retained;
		if (npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL) < 0 && retained != NULL)
			npRtReleaseMessageStanza(stanza);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x813186B4 */
static void npImpsOnBasicMessageRecv(const NpRtSessionIdentity *rtSession,
				     NpRtMessageStanza *stanza)
{
	npImpsPostBasicMessageRecv(rtSession, stanza);
}

/* 0x8131ACC2 */
static void npImpsPostBasicPresenceRecv(const NpRtSessionIdentity *rtSession,
					NpRtPresenceStanza *stanza)
{
	NpImpsSession *session = NULL;
	if (npImpsSessionAcquireByRtHandle(rtSession->sessionId, &session, 1) >= 0) {
		NpRtPresenceStanza *retained = reinterpret_cast<NpRtPresenceStanza *>(
			npRtAddRefStanza(stanza));
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_MSG_RT_EVENT;
		npImpsCopyRtHeader(&msg.body.rtEvent, rtSession);
		msg.body.rtEvent.eventKind = NP_IMPS_RT_EVT_PRESENCE_RECV;
		msg.body.rtEvent.detail.presenceStanza = retained;
		if (npImpsSessionPipeSend(&session->msgPipeId, &msg, NULL) < 0 && retained != NULL)
			npRtReleasePresenceStanza(stanza);
	}
	if (session != NULL)
		npImpsSessionRelease(session);
}

/* 0x813186BC */
static void npImpsOnBasicPresenceRecv(const NpRtSessionIdentity *rtSession,
				      NpRtPresenceStanza *stanza)
{
	npImpsPostBasicPresenceRecv(rtSession, stanza);
}

/* ============================================================================
 * Session 17-state machine (0x81319998..0x8131AA7F)
 * ============================================================================ */

static void npImpsSessionUpdateDesiredMode(NpImpsSession *session, int mode,
					   NpRtPresenceStanza *presence)
{
	if (session->desiredPresence != NULL) {
		npRtReleasePresenceStanza(session->desiredPresence);
		session->desiredPresence = NULL;
	}
	session->desiredMode = mode;
	if (presence != NULL) {
		session->desiredPresence = reinterpret_cast<NpRtPresenceStanza *>(
			npRtAddRefStanza(presence));
	}
}

static void npImpsSessionLatchPendingPresence(NpImpsSession *session)
{
	if (session->pendingPresence != NULL) {
		npRtReleasePresenceStanza(session->pendingPresence);
		session->pendingPresence = NULL;
	}
	if (session->desiredPresence != NULL) {
		session->pendingPresence = reinterpret_cast<NpRtPresenceStanza *>(
			npRtAddRefStanza(session->desiredPresence));
	}
}

static void npImpsSessionEnterSuspended(NpImpsSession *session, int errorCode, int *nextState)
{
	npImpsRtForceClose(&session->rt);
	npImpsPollForceClose(&session->poll);
	if (session->desiredPresence != NULL) {
		npRtReleasePresenceStanza(session->desiredPresence);
		session->desiredPresence = NULL;
	}
	session->lastError = errorCode;
	session->desiredMode = NP_IMPS_MODE_NONE;
	*nextState = NP_IMPS_SESS_SUSPENDED;
}

/* 0x81319B92: states 0 and 16 */
static int npImpsStateInvalid(NpImpsSession *session, int *nextState)
{
	(void)session;
	(void)nextState;
	return NP_IMPS_CORE_ERR_BAD_STATE;
}

/* 0x81319B9C: state 1 (IDLE) */
static int npImpsStateIdle(NpImpsSession *session, int *nextState)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = npImpsSessionRecvFilteredMsg(session, &msg, NULL);
	if (ret >= 0) {
		if (msg.kind == NP_IMPS_MSG_START) {
			npImpsSessionUpdateDesiredMode(session, msg.body.modeReq.mode,
						       msg.body.modeReq.presence);
			*nextState = NP_IMPS_SESS_FETCH_CONFIG;
			ret = 0;
		} else {
			ret = npImpsSessionRejectUnexpectedMsg(session, &msg,
							       NP_IMPS_CORE_ERR_BAD_STATE);
		}
	}
	npImpsSessionMsgCleanup(&msg);
	return ret;
}

/* 0x81319C3C: state 2 (FETCH_CONFIG) */
static int npImpsStateFetchConfig(NpImpsSession *session, int *nextState)
{
	int ret = npImpsSessionFetchConfig(session);
	if (ret < 0) {
		npImpsSessionEnterSuspended(session, ret, nextState);
		return 0;
	}

	if (session->desiredMode == NP_IMPS_MODE_ONLINE) {
		npImpsSessionLatchPendingPresence(session);
		ret = npImpsPollConnect(&session->poll, 0, 0, session->rt.sessionSid);
	} else {
		ret = npImpsPollConnect(&session->poll, 1, 0, session->rt.sessionSid);
	}

	if (ret < 0)
		return ret;
	*nextState = NP_IMPS_SESS_WAIT_POLL_CONNECT;
	return 0;
}

/* 0x81319CD4: state 3 (WAIT_POLL_CONNECT) */
static int npImpsStateWaitPollConnect(NpImpsSession *session, int *nextState)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = npImpsSessionRecvFilteredMsg(session, &msg, NULL);
	if (ret < 0)
		return ret;

	if (msg.kind == NP_IMPS_MSG_SET_MODE) {
		npImpsSessionUpdateDesiredMode(session, msg.body.modeReq.mode,
					       msg.body.modeReq.presence);
		npImpsSessionMsgCleanup(&msg);
		return 0;
	}

	if (msg.kind != NP_IMPS_MSG_POLL_EVENT) {
		ret = npImpsSessionRejectUnexpectedMsg(session, &msg, NP_IMPS_CORE_ERR_BAD_STATE);
		npImpsSessionMsgCleanup(&msg);
		return ret;
	}

	int pollRet = npImpsPollHandleConnectEvent(&session->poll, &msg.body.pollEvent);
	npImpsSessionMsgCleanup(&msg);

	if (pollRet == NP_POLLING_ERR_SERVER_BUSY && session->desiredMode == NP_IMPS_MODE_ONLINE) {
		npImpsSessionLatchPendingPresence(session);
		ret = npImpsPollConnect(&session->poll, 0, 0, session->rt.sessionSid);
		if (ret >= 0) {
			*nextState = NP_IMPS_SESS_WAIT_POLL_CONNECT;
			return 0;
		}
		return ret;
	}

	if (pollRet < 0) {
		npImpsSessionEnterSuspended(session, pollRet, nextState);
		return 0;
	}

	if (session->poll.pollState != NP_IMPS_POLL_CONNECTED)
		return 0;

	if (session->desiredMode == NP_IMPS_MODE_INTERVAL_POLL ||
	    session->desiredMode == NP_IMPS_MODE_CRON_POLL) {
		int serverStatus = NP_POLLING_SERVER_UNCHECKED;
		ret = npImpsPollGetServerStatus(&session->poll, &serverStatus);
		if (ret < 0)
			return ret;
		if (serverStatus == NP_POLLING_SERVER_OK && npImpsRtHasSessionSid(&session->rt)) {
			*nextState = (session->desiredMode == NP_IMPS_MODE_INTERVAL_POLL)
				? NP_IMPS_SESS_ENTERING_INTERVAL_POLL
				: NP_IMPS_SESS_ENTERING_CRON_POLL;
			return 0;
		}
		sceClibMemset(session->rt.sessionSid, 0, sizeof(session->rt.sessionSid));
	}

	if (npImpsRtHasSessionSid(&session->rt))
		ret = npImpsRtReconnect(&session->rt);
	else
		ret = npImpsRtConnect(&session->rt);
	if (ret < 0)
		return ret;

	*nextState = NP_IMPS_SESS_WAIT_RT_CONNECT;
	return 0;
}

/* 0x81319E74: state 4 (WAIT_RT_CONNECT) */
static int npImpsStateWaitRtConnect(NpImpsSession *session, int *nextState)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = npImpsSessionRecvFilteredMsg(session, &msg, NULL);
	if (ret >= 0) {
		if (msg.kind == NP_IMPS_MSG_SET_MODE) {
			npImpsSessionUpdateDesiredMode(session, msg.body.modeReq.mode,
						       msg.body.modeReq.presence);
			ret = 0;
		} else if (msg.kind == NP_IMPS_MSG_RT_EVENT) {
			SceUInt8 sidChanged = 0;
			int rtRet = npImpsRtHandleConnectEvent(&session->rt, &msg.body.rtEvent,
							       &session->identity, &sidChanged);
			if (rtRet < 0) {
				npImpsSessionEnterSuspended(session, rtRet, nextState);
				ret = 0;
			} else {
				if (session->rt.rtState == NP_IMPS_RT_CONNECTED) {
					if (sidChanged != 0) {
						npImpsPollResetState(&session->poll);
						*nextState = NP_IMPS_SESS_RT_CONNECTED_NEW_SID;
					} else {
						*nextState = NP_IMPS_SESS_RT_CONNECTED_SAME_SID;
					}
				}
				ret = 0;
			}
		} else {
			ret = npImpsSessionRejectUnexpectedMsg(session, &msg,
							       NP_IMPS_CORE_ERR_BAD_STATE);
		}
	}

	npImpsSessionMsgCleanup(&msg);
	return ret;
}

/* 0x81319F80 / 0x8131A034: states 5 and 6 (RT_CONNECTED) */
static int npImpsStateRtConnected(NpImpsSession *session, int *nextState)
{
	switch (session->desiredMode) {
	case NP_IMPS_MODE_DISCONNECT: {
		int ret = npImpsPollDisconnect(&session->poll);
		if (ret >= 0)
			ret = npImpsRtDisconnect(&session->rt);
		if (ret < 0)
			return ret;
		*nextState = NP_IMPS_SESS_DISCONNECTING;
		return 0;
	}
	case NP_IMPS_MODE_CRON_POLL: {
		int ret = npImpsRtEnterStandby(&session->rt, 2, npImpsOnRtStandbyDone);
		if (ret < 0)
			return ret;
		*nextState = NP_IMPS_SESS_ENTERING_CRON_POLL;
		return 0;
	}
	case NP_IMPS_MODE_INTERVAL_POLL: {
		int ret = npImpsRtEnterStandby(&session->rt, 1, npImpsOnRtStandbyDone);
		if (ret < 0)
			return ret;
		*nextState = NP_IMPS_SESS_ENTERING_INTERVAL_POLL;
		return 0;
	}
	case NP_IMPS_MODE_ONLINE: {
		npImpsSessionLatchPendingPresence(session);
		int ret = npImpsPollDisconnect(&session->poll);
		if (ret < 0)
			return ret;
		*nextState = NP_IMPS_SESS_ENTERING_ONLINE;
		return 0;
	}
	default:
		return NP_IMPS_CORE_ERR_INVALID_MODE;
	}
}

/* 0x8131A034: state 6 (RT_CONNECTED_SAME_SID) */
static int npImpsStateRtConnectedSameSid(NpImpsSession *session, int *nextState)
{
	int ret = npImpsStateRtConnected(session, nextState);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131A044: state 7 (ENTERING_ONLINE) */
static int npImpsStateEnteringOnline(NpImpsSession *session, int *nextState)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = 0;
	if (session->rt.rtState == NP_IMPS_RT_CONNECTED) {
		ret = npImpsRtSyncInitialOnline(&session->rt, session->pendingPresence,
						npImpsOnRtRosterGetDone);
		if (ret >= 0) {
			if (session->pendingPresence != NULL) {
				npRtReleasePresenceStanza(session->pendingPresence);
				session->pendingPresence = NULL;
			}
			*nextState = NP_IMPS_SESS_ONLINE;
			ret = 0;
		}
	} else {
		ret = npImpsSessionRecvFilteredMsg(session, &msg, NULL);
		if (ret >= 0) {
			if (msg.kind == NP_IMPS_MSG_SET_MODE) {
				npImpsSessionUpdateDesiredMode(session, msg.body.modeReq.mode,
							       msg.body.modeReq.presence);
				ret = 0;
			} else if (msg.kind == NP_IMPS_MSG_RT_EVENT) {
				SceUInt8 sidChanged = 0;
				int rtRet = npImpsRtHandleConnectEvent(&session->rt,
								       &msg.body.rtEvent,
								       &session->identity,
								       &sidChanged);
				if (rtRet < 0) {
					npImpsSessionEnterSuspended(session, rtRet, nextState);
					ret = 0;
				} else {
					if (sidChanged != 0) {
						npImpsPollResetState(&session->poll);
						*nextState = NP_IMPS_SESS_RT_CONNECTED_NEW_SID;
					}
					ret = 0;
				}
			} else {
				ret = npImpsSessionRejectUnexpectedMsg(session, &msg,
								       NP_IMPS_CORE_ERR_BAD_STATE);
			}
		}
	}

	npImpsSessionMsgCleanup(&msg);
	return ret;
}

/* 0x8131A17A / 0x8131A326: states 8 and 9 (ENTERING_POLL) */
static int npImpsStateEnteringPoll(NpImpsSession *session, int *nextState)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = 0;
	if (session->rt.rtState == NP_IMPS_RT_DISCONNECTED) {
		if (session->poll.pollState == NP_IMPS_POLL_CONNECTED) {
			if (session->state == NP_IMPS_SESS_ENTERING_INTERVAL_POLL) {
				ret = npImpsPollUpdateMode(&session->poll,
							   NP_POLLING_MODE_INTERVAL,
							   session->rt.sessionSid);
				if (ret >= 0)
					*nextState = NP_IMPS_SESS_POLL_INTERVAL;
			} else if (session->state == NP_IMPS_SESS_ENTERING_CRON_POLL) {
				ret = npImpsPollUpdateMode(&session->poll,
							   NP_POLLING_MODE_CRON,
							   session->rt.sessionSid);
				if (ret >= 0)
					*nextState = NP_IMPS_SESS_POLL_CRON;
			} else {
				ret = NP_IMPS_CORE_ERR_BAD_STATE;
			}
			npImpsSessionMsgCleanup(&msg);
			return ret;
		}
		if (session->poll.pollState == NP_IMPS_POLL_DISCONNECTED) {
			ret = npImpsPollConnect(&session->poll, 0, 0, session->rt.sessionSid);
			if (ret < 0) {
				npImpsSessionMsgCleanup(&msg);
				return ret;
			}
		}
	}

	ret = npImpsSessionRecvFilteredMsg(session, &msg, NULL);
	if (ret >= 0) {
		if (msg.kind == NP_IMPS_MSG_SET_MODE) {
			npImpsSessionUpdateDesiredMode(session, msg.body.modeReq.mode,
						       msg.body.modeReq.presence);
			ret = 0;
		} else if (msg.kind == NP_IMPS_MSG_RT_EVENT) {
			int rtRet = npImpsRtHandleActiveEvent(&session->rt, &msg.body.rtEvent,
							      &session->identity);
			if (rtRet < 0)
				npImpsSessionEnterSuspended(session, rtRet, nextState);
			ret = 0;
		} else if (msg.kind == NP_IMPS_MSG_POLL_EVENT) {
			int diffBits = 0;
			int pollRet = npImpsPollHandleActiveEvent(&session->poll,
								  &msg.body.pollEvent, &diffBits);
			if (pollRet < 0)
				npImpsSessionEnterSuspended(session, pollRet, nextState);
			ret = 0;
		} else {
			ret = npImpsSessionRejectUnexpectedMsg(session, &msg,
							       NP_IMPS_CORE_ERR_BAD_STATE);
		}
	}

	npImpsSessionMsgCleanup(&msg);
	return ret;
}

/* 0x8131A326: state 9 (ENTERING_CRON_POLL) */
static int npImpsStateEnteringCronPoll(NpImpsSession *session, int *nextState)
{
	int ret = npImpsStateEnteringPoll(session, nextState);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131A336: state 10 (DISCONNECTING) */
static int npImpsStateDisconnecting(NpImpsSession *session, int *nextState)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = npImpsSessionRecvFilteredMsg(session, &msg, NULL);
	if (ret >= 0) {
		if (msg.kind != NP_IMPS_MSG_RT_EVENT) {
			ret = npImpsSessionRejectUnexpectedMsg(session, &msg,
							       NP_IMPS_CORE_ERR_BAD_STATE);
		} else {
			if (session->rt.rtState == NP_IMPS_RT_CONNECTING) {
				SceUInt8 sidChanged = 0;
				ret = npImpsRtHandleConnectEvent(&session->rt, &msg.body.rtEvent,
								 &session->identity, &sidChanged);
				if (ret >= 0 && sidChanged != 0)
					npImpsPollResetState(&session->poll);
			} else {
				ret = npImpsRtHandleActiveEvent(&session->rt, &msg.body.rtEvent,
								&session->identity);
			}

			if (ret >= 0) {
				if (session->rt.rtState == NP_IMPS_RT_CONNECTED) {
					ret = npImpsRtDisconnect(&session->rt);
				} else if (session->rt.rtState == NP_IMPS_RT_DISCONNECTED) {
					*nextState = NP_IMPS_SESS_STOPPING;
				} else if (session->rt.rtState != NP_IMPS_RT_CONNECTING &&
					   session->rt.rtState != NP_IMPS_RT_DISCONNECTING) {
					ret = NP_IMPS_CORE_ERR_BAD_STATE;
				}
			}
		}
	}

	npImpsSessionMsgCleanup(&msg);
	return ret;
}

/* 0x8131A418: state 11 (ONLINE) */
static int npImpsStateOnline(NpImpsSession *session, int *nextState)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = 0;
	if (session->desiredMode == NP_IMPS_MODE_DISCONNECT) {
		ret = npImpsRtDisconnect(&session->rt);
		if (ret >= 0)
			*nextState = NP_IMPS_SESS_DISCONNECTING;
	} else if (session->desiredMode == NP_IMPS_MODE_CRON_POLL) {
		ret = npImpsRtEnterStandby(&session->rt, 2, npImpsOnRtStandbyDone);
		if (ret >= 0)
			*nextState = NP_IMPS_SESS_ENTERING_CRON_POLL;
	} else if (session->desiredMode == NP_IMPS_MODE_INTERVAL_POLL) {
		ret = npImpsRtEnterStandby(&session->rt, 1, npImpsOnRtStandbyDone);
		if (ret >= 0)
			*nextState = NP_IMPS_SESS_ENTERING_INTERVAL_POLL;
	} else if (session->desiredMode == NP_IMPS_MODE_ONLINE) {
		ret = npImpsSessionRecvFilteredMsg(session, &msg, NULL);
		if (ret >= 0) {
			switch (msg.kind) {
			case NP_IMPS_MSG_SET_MODE:
				npImpsSessionUpdateDesiredMode(session, msg.body.modeReq.mode,
							       msg.body.modeReq.presence);
				ret = 0;
				break;
			case NP_IMPS_MSG_SEND_PRESENCE: {
				int sendRet = npImpsRtDispatchPresence(
					&session->rt, msg.body.sendPresence.transactionId,
					msg.body.sendPresence.stanza, npImpsOnRtPresenceSent);
				if (sendRet < 0)
					npImpsSessionEnterSuspended(session, sendRet, nextState);
				ret = 0;
				break;
			}
			case NP_IMPS_MSG_SEND_MESSAGE: {
				int sendRet = npImpsRtDispatchMessage(
					&session->rt, msg.body.sendMessage.transactionId,
					msg.body.sendMessage.stanza, npImpsOnRtMessageSent);
				if (sendRet < 0)
					npImpsSessionEnterSuspended(session, sendRet, nextState);
				ret = 0;
				break;
			}
			case NP_IMPS_MSG_DELETE_STORED: {
				int sendRet = npImpsRtDispatchDeleteStored(
					&session->rt, msg.body.deleteStored.transactionId,
					msg.body.deleteStored.storedId, npImpsOnRtStoredDeleted);
				if (sendRet < 0)
					npImpsSessionEnterSuspended(session, sendRet, nextState);
				ret = 0;
				break;
			}
			case NP_IMPS_MSG_RT_EVENT: {
				int rtRet = npImpsRtHandleActiveEvent(&session->rt,
								      &msg.body.rtEvent,
								      &session->identity);
				if (rtRet < 0)
					npImpsSessionEnterSuspended(session, rtRet, nextState);
				ret = 0;
				break;
			}
			default:
				ret = npImpsSessionRejectUnexpectedMsg(session, &msg,
								       NP_IMPS_CORE_ERR_BAD_STATE);
				break;
			}
		}
	} else {
		ret = NP_IMPS_CORE_ERR_INVALID_MODE;
	}

	npImpsSessionMsgCleanup(&msg);
	return ret;
}

/* 0x8131A672: state 12 (POLL_INTERVAL) */
static int npImpsStatePollInterval(NpImpsSession *session, int *nextState)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = 0;
	bool waitEvent = false;

	if (session->desiredMode == NP_IMPS_MODE_DISCONNECT) {
		ret = npImpsPollDisconnect(&session->poll);
		if (ret >= 0)
			ret = npImpsRtReconnect(&session->rt);
		if (ret >= 0)
			*nextState = NP_IMPS_SESS_DISCONNECTING;
	} else if (session->desiredMode == NP_IMPS_MODE_ONLINE) {
		npImpsSessionLatchPendingPresence(session);
		ret = npImpsPollDisconnect(&session->poll);
		if (ret >= 0)
			ret = npImpsRtReconnect(&session->rt);
		if (ret >= 0)
			*nextState = NP_IMPS_SESS_ENTERING_ONLINE;
	} else if (session->desiredMode == NP_IMPS_MODE_CRON_POLL) {
		ret = npImpsPollUpdateMode(&session->poll, NP_POLLING_MODE_CRON,
					   session->rt.sessionSid);
		if (ret >= 0) {
			*nextState = NP_IMPS_SESS_POLL_INTERVAL;
			waitEvent = true;
		}
	} else {
		waitEvent = true;
	}

	if (waitEvent) {
		ret = npImpsSessionRecvFilteredMsg(session, &msg, NULL);
		if (ret >= 0) {
			if (msg.kind == NP_IMPS_MSG_SET_MODE) {
				npImpsSessionUpdateDesiredMode(session, msg.body.modeReq.mode,
							       msg.body.modeReq.presence);
				ret = 0;
			} else if (msg.kind == NP_IMPS_MSG_POLL_EVENT) {
				int diffBits = 0;
				int pollRet = npImpsPollHandleActiveEvent(
					&session->poll, &msg.body.pollEvent, &diffBits);
				if (pollRet < 0) {
					npImpsSessionEnterSuspended(session, pollRet, nextState);
				} else if (diffBits != 0) {
					npImpsSessionNotifyPolling(&session->identity,
								   &session->poll.timestamps,
								   diffBits);
				}
				ret = 0;
			} else {
				ret = npImpsSessionRejectUnexpectedMsg(session, &msg,
								       NP_IMPS_CORE_ERR_BAD_STATE);
			}
		}
	}

	npImpsSessionMsgCleanup(&msg);
	return ret;
}

/* 0x8131A808: state 13 (POLL_CRON) */
static int npImpsStatePollCron(NpImpsSession *session, int *nextState)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = 0;
	bool waitEvent = false;

	if (session->desiredMode == NP_IMPS_MODE_DISCONNECT) {
		ret = npImpsPollDisconnect(&session->poll);
		if (ret >= 0)
			ret = npImpsRtReconnect(&session->rt);
		if (ret >= 0)
			*nextState = NP_IMPS_SESS_DISCONNECTING;
	} else if (session->desiredMode == NP_IMPS_MODE_ONLINE) {
		npImpsSessionLatchPendingPresence(session);
		ret = npImpsPollDisconnect(&session->poll);
		if (ret >= 0)
			ret = npImpsRtReconnect(&session->rt);
		if (ret >= 0)
			*nextState = NP_IMPS_SESS_ENTERING_ONLINE;
	} else if (session->desiredMode == NP_IMPS_MODE_INTERVAL_POLL) {
		ret = npImpsPollUpdateMode(&session->poll, NP_POLLING_MODE_INTERVAL,
					   session->rt.sessionSid);
		if (ret >= 0) {
			*nextState = NP_IMPS_SESS_POLL_INTERVAL;
			waitEvent = true;
		}
	} else {
		waitEvent = true;
	}

	if (waitEvent) {
		ret = npImpsSessionRecvFilteredMsg(session, &msg, NULL);
		if (ret >= 0) {
			if (msg.kind == NP_IMPS_MSG_SET_MODE) {
				npImpsSessionUpdateDesiredMode(session, msg.body.modeReq.mode,
							       msg.body.modeReq.presence);
				ret = 0;
			} else if (msg.kind == NP_IMPS_MSG_POLL_EVENT) {
				int diffBits = 0;
				int pollRet = npImpsPollHandleActiveEvent(
					&session->poll, &msg.body.pollEvent, &diffBits);
				if (pollRet < 0) {
					npImpsSessionEnterSuspended(session, pollRet, nextState);
				} else if (diffBits != 0) {
					npImpsSessionNotifyPolling(&session->identity,
								   &session->poll.timestamps,
								   diffBits);
				}
				ret = 0;
			} else {
				ret = npImpsSessionRejectUnexpectedMsg(session, &msg,
								       NP_IMPS_CORE_ERR_BAD_STATE);
			}
		}
	}

	npImpsSessionMsgCleanup(&msg);
	return ret;
}

/* 0x8131A99C: state 14 (SUSPENDED) */
static int npImpsStateSuspended(NpImpsSession *session, int *nextState)
{
	NpImpsSessionMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_MSG_INVALID;

	int ret = 0;
	switch (session->desiredMode) {
	case NP_IMPS_MODE_NONE:
		ret = npImpsSessionRecvFilteredMsg(session, &msg, NULL);
		if (ret >= 0) {
			if (msg.kind == NP_IMPS_MSG_SET_MODE) {
				npImpsSessionUpdateDesiredMode(session, msg.body.modeReq.mode,
							       msg.body.modeReq.presence);
				ret = 0;
			} else {
				ret = npImpsSessionRejectUnexpectedMsg(session, &msg,
								       NP_IMPS_CORE_ERR_BAD_STATE);
			}
		}
		break;

	case NP_IMPS_MODE_DISCONNECT:
		if (npImpsRtHasSessionSid(&session->rt)) {
			ret = npImpsRtReconnect(&session->rt);
			if (ret >= 0)
				*nextState = NP_IMPS_SESS_DISCONNECTING;
		} else {
			*nextState = NP_IMPS_SESS_STOPPING;
		}
		break;

	case NP_IMPS_MODE_CRON_POLL:
	case NP_IMPS_MODE_INTERVAL_POLL:
	case NP_IMPS_MODE_ONLINE:
		*nextState = NP_IMPS_SESS_FETCH_CONFIG;
		break;

	default:
		ret = NP_IMPS_CORE_ERR_INVALID_MODE;
		break;
	}

	npImpsSessionMsgCleanup(&msg);
	return ret;
}

/* 0x8131AA76: state 15 (STOPPING) */
static int npImpsStateStopping(NpImpsSession *session, int *nextState)
{
	(void)session;
	*nextState = NP_IMPS_SESS_STOPPED;
	return 0;
}

typedef int (*NpImpsSessionStateFn)(NpImpsSession *session, int *nextState);

static const NpImpsSessionStateFn s_sessionStateTable[17] = {
	npImpsStateInvalid,		/* 0: ERROR */
	npImpsStateIdle,		/* 1: IDLE */
	npImpsStateFetchConfig,		/* 2: FETCH_CONFIG */
	npImpsStateWaitPollConnect,	/* 3: WAIT_POLL_CONNECT */
	npImpsStateWaitRtConnect,	/* 4: WAIT_RT_CONNECT */
	npImpsStateRtConnected,		/* 5: RT_CONNECTED_NEW_SID */
	npImpsStateRtConnectedSameSid,	/* 6: RT_CONNECTED_SAME_SID */
	npImpsStateEnteringOnline,	/* 7: ENTERING_ONLINE */
	npImpsStateEnteringPoll,	/* 8: ENTERING_INTERVAL_POLL */
	npImpsStateEnteringCronPoll,	/* 9: ENTERING_CRON_POLL */
	npImpsStateDisconnecting,	/* 10: DISCONNECTING */
	npImpsStateOnline,		/* 11: ONLINE */
	npImpsStatePollInterval,	/* 12: POLL_INTERVAL */
	npImpsStatePollCron,		/* 13: POLL_CRON */
	npImpsStateSuspended,		/* 14: SUSPENDED */
	npImpsStateStopping,		/* 15: STOPPING */
	npImpsStateInvalid		/* 16: STOPPED */
};

/* 0x81319998 */
static int npImpsSessionTransitionState(NpImpsSession *session, int newState)
{
	int reportError = 0;
	if (newState == NP_IMPS_SESS_SUSPENDED || newState == NP_IMPS_SESS_ERROR)
		reportError = session->lastError;

	int lockState = npRtLockLwMutex(&session->mutex);
	if (lockState < 0)
		return lockState;

	int prevState = session->state;
	session->state = newState;
	npRtUnlockLwMutex(&session->mutex, lockState);

	if (prevState == NP_IMPS_SESS_SUSPENDED)
		session->lastError = 0;

	if (newState == NP_IMPS_SESS_ERROR || newState == NP_IMPS_SESS_STOPPED) {
		int drainError = reportError;
		if (newState == NP_IMPS_SESS_STOPPED) {
			if (drainError >= 0)
				drainError = NP_IMPS_CORE_ERR_STOPPED;
			sceClibMemset(session->rt.sessionSid, 0, sizeof(session->rt.sessionSid));
		} else {
			if (drainError >= 0)
				drainError = NP_IMPS_CORE_ERR_BAD_STATE;
		}
		int ret = npImpsSessionDrainPendingMsgs(session, drainError);
		if (ret < 0)
			return ret;
		ret = npImpsRtTxFailAllActive(&session->rt.txQueue, drainError);
		if (ret < 0)
			return ret;
		ret = npImpsRtFlushCompletedTx(&session->rt, &session->identity);
		if (ret < 0)
			return ret;
	}

	if (newState == NP_IMPS_SESS_ONLINE || newState == NP_IMPS_SESS_POLL_INTERVAL ||
	    newState == NP_IMPS_SESS_POLL_CRON || newState == NP_IMPS_SESS_SUSPENDED ||
	    newState == NP_IMPS_SESS_STOPPING) {
		if (session->pendingPresence != NULL) {
			npRtReleasePresenceStanza(session->pendingPresence);
			session->pendingPresence = NULL;
		}
	}

	int notifyEvent = -1;
	switch (newState) {
	case NP_IMPS_SESS_ERROR:
		if (prevState != -1 && prevState != NP_IMPS_SESS_ERROR &&
		    prevState != NP_IMPS_SESS_STOPPED) {
			notifyEvent = NP_IMPS_EVT_STOPPED;
		}
		break;
	case NP_IMPS_SESS_WAIT_POLL_CONNECT:
		notifyEvent = NP_IMPS_EVT_CONNECTING;
		break;
	case NP_IMPS_SESS_RT_CONNECTED_NEW_SID:
		notifyEvent = NP_IMPS_EVT_CONNECTED;
		break;
	case NP_IMPS_SESS_ENTERING_ONLINE:
		notifyEvent = NP_IMPS_EVT_ENTERING_ONLINE;
		break;
	case NP_IMPS_SESS_ENTERING_INTERVAL_POLL:
		notifyEvent = NP_IMPS_EVT_ENTERING_INTERVAL;
		break;
	case NP_IMPS_SESS_ENTERING_CRON_POLL:
		notifyEvent = NP_IMPS_EVT_ENTERING_CRON;
		break;
	case NP_IMPS_SESS_DISCONNECTING:
		notifyEvent = NP_IMPS_EVT_DISCONNECTING;
		break;
	case NP_IMPS_SESS_ONLINE:
		notifyEvent = NP_IMPS_EVT_ONLINE;
		break;
	case NP_IMPS_SESS_POLL_INTERVAL:
		notifyEvent = NP_IMPS_EVT_POLL_INTERVAL;
		break;
	case NP_IMPS_SESS_POLL_CRON:
		notifyEvent = NP_IMPS_EVT_POLL_CRON;
		break;
	case NP_IMPS_SESS_SUSPENDED:
		notifyEvent = NP_IMPS_EVT_SUSPENDED;
		break;
	case NP_IMPS_SESS_STOPPING:
		notifyEvent = NP_IMPS_EVT_STOPPING;
		break;
	case NP_IMPS_SESS_STOPPED:
		notifyEvent = NP_IMPS_EVT_STOPPED;
		break;
	default:
		break;
	}

	if (notifyEvent >= 0)
		npImpsSessionNotifyState(&session->identity, notifyEvent, reportError);
	return 0;
}

/* 0x81319AD8 */
static int npImpsSessionMainLoop(NpImpsSession *session)
{
	int err = 0;
	while (session->state != NP_IMPS_SESS_STOPPED) {
		if (session->aborted != 0) {
			err = NP_IMPS_CORE_ERR_ABORTED;
			break;
		}
		int nextState = session->state;
		err = s_sessionStateTable[session->state](session, &nextState);
		if (err < 0)
			break;
		if (session->state != nextState) {
			err = npImpsSessionTransitionState(session, nextState);
			if (err < 0)
				break;
		}
	}

	if (session->state == NP_IMPS_SESS_STOPPED)
		return 0;

	if (session->lastError == 0 && session->fallbackError < 0)
		session->lastError = session->fallbackError;
	else
		session->lastError = err;

	npImpsSessionTransitionState(session, NP_IMPS_SESS_ERROR);
	return err;
}

/* ============================================================================
 * Internal session listener registry (0x8131B010..0x8131B2EB)
 * ============================================================================ */

static void npImpsSessionNotifyState(const NpImpsSessionIdentity *identity, int eventState,
				     int errorCode)
{
	for (NpImpsSessionListenerNode *cur = s_listenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onSessionState != NULL)
			cur->callbacks.onSessionState(identity, eventState, errorCode);
	}
}

static void npImpsSessionNotifyRoster(const NpImpsSessionIdentity *identity, int isPush,
				      NpBasicRosterList *rosterList)
{
	for (NpImpsSessionListenerNode *cur = s_listenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onRosterUpdate != NULL)
			cur->callbacks.onRosterUpdate(identity, isPush, rosterList);
	}
}

static void npImpsSessionNotifyPrivacy(const NpImpsSessionIdentity *identity, int action,
				       NpBasicPrivacyList *privacyList)
{
	for (NpImpsSessionListenerNode *cur = s_listenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onPrivacyUpdate != NULL)
			cur->callbacks.onPrivacyUpdate(identity, action, privacyList);
	}
}

static void npImpsSessionNotifyMessage(const NpImpsSessionIdentity *identity,
				       NpRtMessageStanza *stanza)
{
	for (NpImpsSessionListenerNode *cur = s_listenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onMessageStanza != NULL)
			cur->callbacks.onMessageStanza(identity, stanza);
	}
}

static void npImpsSessionNotifyPresence(const NpImpsSessionIdentity *identity,
					NpRtPresenceStanza *stanza)
{
	for (NpImpsSessionListenerNode *cur = s_listenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onPresenceStanza != NULL)
			cur->callbacks.onPresenceStanza(identity, stanza);
	}
}

static void npImpsSessionNotifyPolling(const NpImpsSessionIdentity *identity,
				       const NpPollingTimestampSet *timestamps, int diffBits)
{
	for (NpImpsSessionListenerNode *cur = s_listenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onPollingUpdate != NULL) {
			if ((diffBits & NP_IMPS_POLL_DIFF_FRIEND_LIST) != 0)
				cur->callbacks.onPollingUpdate(identity, 0,
							       &timestamps->friendListTick);
			if ((diffBits & NP_IMPS_POLL_DIFF_BLOCK_LIST) != 0)
				cur->callbacks.onPollingUpdate(identity, 1,
							       &timestamps->blockListTick);
		}
	}
}

/* 0x8131B010 */
static int npImpsSessionListenerAlloc(const NpImpsCoreSessionCallbacks *callbacks,
				      NpImpsSessionListenerNode **outNode)
{
	if (s_listenerCount >= NP_IMPS_MAX_LISTENERS)
		return NP_IMPS_CORE_ERR_CB_FULL;

	NpImpsSessionListenerNode *node = reinterpret_cast<NpImpsSessionListenerNode *>(
		npRtHeapAlloc(sizeof(NpImpsSessionListenerNode), NULL));
	if (node == NULL)
		return NP_IMPS_CORE_ERR_NO_MEMORY;

	sceClibMemset(node, 0, sizeof(*node));
	sceClibMemcpy_safe(&node->callbacks, callbacks, sizeof(node->callbacks));
	s_listenerCount++;

	s_nextListenerId++;
	if (s_nextListenerId == 0)
		s_nextListenerId++;
	node->listenerId = s_nextListenerId;

	node->next = NULL;
	node->prev = s_listenerTail;
	if (s_listenerTail != NULL)
		s_listenerTail->next = node;
	if (s_listenerHead == NULL)
		s_listenerHead = node;
	s_listenerTail = node;

	*outNode = node;
	return 0;
}

/* 0x8131B246 */
int npImpsCoreRegisterSessionCallbacks(const NpImpsCoreSessionCallbacks *callbacks,
				       int *outCallbackId)
{
	NpImpsSessionListenerNode *node = NULL;
	int ret = npImpsSessionListenerAlloc(callbacks, &node);
	if (ret >= 0) {
		*outCallbackId = node->listenerId;
		return 0;
	}
	return ret;
}

/* 0x8131B27C */
int npImpsCoreUnregisterSessionCallbacks(int callbackId)
{
	NpImpsSessionListenerNode *found = NULL;
	for (NpImpsSessionListenerNode *cur = s_listenerHead; cur != NULL; cur = cur->next) {
		if (cur->listenerId == callbackId) {
			found = cur;
			break;
		}
	}
	if (found == NULL)
		return NP_IMPS_CORE_ERR_CB_NOT_FOUND;

	if (found->next != NULL)
		found->next->prev = found->prev;
	else
		s_listenerTail = found->prev;

	if (found->prev != NULL)
		found->prev->next = found->next;
	else
		s_listenerHead = found->next;

	found->next = NULL;
	found->prev = NULL;
	npRtHeapFree(found, NULL);
	s_listenerCount--;
	return 0;
}

/* ============================================================================
 * Module Init / Term & Public Session Facade (0x813176AC, 0x813176F2,
 *                                             0x813F4D74..0x813F5193)
 * ============================================================================ */

static int npImpsRegisterPollingCallbacks(void)
{
	if (s_pollingCallbackId != 0)
		return NP_IMPS_CORE_ERR_ALREADY_INIT;

	NpPollingCallbackTable cbTable;
	sceClibMemset(&cbTable, 0, sizeof(cbTable));
	cbTable.onStateChanged = npImpsOnPollingState;
	cbTable.onUpdateReceived = npImpsOnPollingUpdate;

	int cbId = 0;
	int ret = npPollingRegisterCallbacks(&cbTable, &cbId);
	if (ret >= 0) {
		s_pollingCallbackId = cbId;
		return 0;
	}
	return ret;
}

static int npImpsUnregisterPollingCallbacks(void)
{
	if (s_pollingCallbackId == 0)
		return 0;
	int ret = npPollingUnregisterCallbacks(s_pollingCallbackId);
	if (ret >= 0) {
		s_pollingCallbackId = 0;
		return 0;
	}
	return ret;
}

static int npImpsRegisterRtHandlers(void)
{
	if (s_rtHandlerId != 0)
		return NP_IMPS_CORE_ERR_ALREADY_INIT;

	NpRtHandlerConfig config;
	sceClibMemset(&config, 0, sizeof(config));
	config.onStateChanged = npImpsOnRtState;

	int handlerId = 0;
	int ret = npRtRegisterHandler(&config, &handlerId);
	if (ret >= 0) {
		s_rtHandlerId = handlerId;
		NpBasicCallbacks basicCb;
		basicCb.onRosterPush = npImpsOnBasicRosterPush;
		basicCb.onPrivacyPush = npImpsOnBasicPrivacyPush;
		basicCb.onMessageRecv = npImpsOnBasicMessageRecv;
		basicCb.onPresenceRecv = npImpsOnBasicPresenceRecv;
		npBasicSetCallbacks(&basicCb);
		return 0;
	}
	return ret;
}

static int npImpsUnregisterRtHandlers(void)
{
	if (s_rtHandlerId == 0)
		return 0;
	int ret = npRtUnregisterHandler(s_rtHandlerId);
	if (ret >= 0) {
		s_rtHandlerId = 0;
		return 0;
	}
	return ret;
}

/* 0x813176AC */
int npImpsCoreInit(const NpPollingThreadParam *threadParam)
{
	int ret = npImpsCoreMutexCreate();
	if (ret >= 0) {
		ret = npImpsRegisterPollingCallbacks();
		if (ret >= 0) {
			ret = npImpsRegisterRtHandlers();
			if (ret >= 0) {
				ret = npImpsSessionMgrInit(threadParam);
				if (ret >= 0)
					return 0;
			}
		}
	}
	npImpsSessionMgrTerm();
	npImpsUnregisterRtHandlers();
	npImpsUnregisterPollingCallbacks();
	npImpsCoreMutexDelete();
	return ret;
}

/* 0x813176F2 */
int npImpsCoreTerm(void)
{
	npImpsSessionMgrTerm();
	npImpsUnregisterRtHandlers();
	return npImpsCoreMutexDelete();
}

/* 0x813F4D74 */
int npImpsCoreCreateSession(int *outSessionId)
{
	NpImpsSession *session = NULL;
	int ret = npImpsSessionCreate(&session);
	if (ret >= 0)
		ret = npImpsSessionStartWorker(session);
	if (ret < 0) {
		if (session != NULL)
			npImpsSessionClose(session);
		return ret;
	}
	*outSessionId = session->identity.sessionId;
	return 0;
}

/* 0x813F4DC6 */
int npImpsCoreDestroySession(int sessionId, int reasonError)
{
	NpImpsSession *session = NULL;
	int ret = npImpsSessionAcquireById(sessionId, &session, 0);
	if (ret < 0) {
		if (session != NULL)
			npImpsSessionRelease(session);
		return ret;
	}

	if (reasonError < 0)
		session->fallbackError = reasonError;
	npImpsSessionClose(session);
	npImpsSessionAbort(session);
	npImpsSessionJoinWorker(session);
	npImpsSessionRelease(session);
	return 0;
}

static inline int npImpsCoreSendModeRequestInline(int sessionId, int msgKind,
						  SceUInt32 allowedStateMask, int mode,
						  NpRtPresenceStanza *presence)
{
	if (mode != NP_IMPS_MODE_DISCONNECT && mode != NP_IMPS_MODE_CRON_POLL &&
	    mode != NP_IMPS_MODE_INTERVAL_POLL && mode != NP_IMPS_MODE_ONLINE) {
		return NP_IMPS_CORE_ERR_INVALID_ARG;
	}
	if (mode == NP_IMPS_MODE_ONLINE && presence == NULL)
		return NP_IMPS_CORE_ERR_INVALID_ARG;

	NpRtPresenceStanza *retained = (presence != NULL)
		? reinterpret_cast<NpRtPresenceStanza *>(npRtAddRefStanza(presence))
		: NULL;

	NpImpsSession *session = NULL;
	int ret = npImpsSessionAcquireById(sessionId, &session, 0);
	if (ret >= 0) {
		NpImpsSessionMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = msgKind;
		msg.body.modeReq.mode = mode;
		msg.body.modeReq.presence = retained;
		ret = npImpsSessionPostCommand(session, &allowedStateMask, &msg);
		if (ret >= 0) {
			ret = 0;
			retained = NULL;
		}
	}

	if (session != NULL)
		npImpsSessionRelease(session);
	if (retained != NULL)
		npRtReleasePresenceStanza(retained);
	return ret;
}

/* 0x813F4E28 */
int npImpsCoreStartSession(int sessionId, int mode, NpRtPresenceStanza *initialPresence)
{
	return npImpsCoreSendModeRequestInline(sessionId, NP_IMPS_MSG_START, 0x0002u,
					       mode, initialPresence);
}

/* 0x813F4ED6 */
int npImpsCoreSetSessionMode(int sessionId, int mode, NpRtPresenceStanza *presence)
{
	return npImpsCoreSendModeRequestInline(sessionId, NP_IMPS_MSG_SET_MODE, 0xFFFEu,
					       mode, presence);
}

/* 0x813F4F88 */
int npImpsCoreGetSessionAutoInterval(int sessionId, int *outIntervalSec)
{
	NpImpsSession *session = NULL;
	int ret = npImpsSessionAcquireById(sessionId, &session, 0);
	if (ret >= 0) {
		*outIntervalSec = npImpsSessionGetAutoIntervalSec(session);
		ret = 0;
	}
	if (session != NULL)
		npImpsSessionRelease(session);
	return ret;
}

/* 0x813F4FD4 */
static volatile SceInt32 s_nextTransactionId = 0;

int npImpsCoreAllocTransactionId(int *outTransactionId)
{
	SceInt32 nextId = sceAtomicIncrement32AcqRel(&s_nextTransactionId) + 1;
	if (nextId == 0)
		nextId = sceAtomicIncrement32AcqRel(&s_nextTransactionId) + 1;
	*outTransactionId = static_cast<int>(nextId);
	return 0;
}

/* 0x813F5018 */
int npImpsCoreSendPresence(int sessionId, int transactionId, NpRtPresenceStanza *stanza,
			   NpImpsSessionSendDoneCallback callback, void *userArg)
{
	NpImpsSession *session = NULL;
	NpImpsRtTxNode *txNode = NULL;
	int ret = npImpsSessionAcquireById(sessionId, &session, 0);
	if (ret >= 0) {
		ret = npImpsRtTxCreate(&session->rt.txQueue, transactionId, callback,
				       userArg, &txNode);
		if (ret >= 0) {
			NpRtPresenceStanza *retained = reinterpret_cast<NpRtPresenceStanza *>(
				npRtAddRefStanza(stanza));
			NpImpsSessionMsg msg;
			sceClibMemset(&msg, 0, sizeof(msg));
			msg.kind = NP_IMPS_MSG_SEND_PRESENCE;
			msg.body.sendPresence.transactionId = transactionId;
			msg.body.sendPresence.stanza = retained;
			SceUInt32 allowedMask = 0xFFFEu;
			ret = npImpsSessionPostCommand(session, &allowedMask, &msg);
			if (ret >= 0) {
				npImpsSessionRelease(session);
				return 0;
			}
			if (retained != NULL)
				npRtReleasePresenceStanza(retained);
		}
	}

	if (txNode != NULL)
		npImpsRtTxRemoveById(&session->rt.txQueue, txNode->header.requestId);
	if (session != NULL)
		npImpsSessionRelease(session);
	return ret;
}

/* 0x813F50D6 */
int npImpsCoreSendMessage(int sessionId, int transactionId, NpRtMessageStanza *stanza,
			  NpImpsSessionSendDoneCallback callback, void *userArg)
{
	NpImpsSession *session = NULL;
	NpImpsRtTxNode *txNode = NULL;
	int ret = npImpsSessionAcquireById(sessionId, &session, 0);
	if (ret >= 0) {
		ret = npImpsRtTxCreate(&session->rt.txQueue, transactionId, callback,
				       userArg, &txNode);
		if (ret >= 0) {
			NpRtMessageStanza *retained = reinterpret_cast<NpRtMessageStanza *>(
				npRtAddRefStanza(stanza));
			NpImpsSessionMsg msg;
			sceClibMemset(&msg, 0, sizeof(msg));
			msg.kind = NP_IMPS_MSG_SEND_MESSAGE;
			msg.body.sendMessage.transactionId = transactionId;
			msg.body.sendMessage.stanza = retained;
			SceUInt32 allowedMask = 0xFFFEu;
			ret = npImpsSessionPostCommand(session, &allowedMask, &msg);
			if (ret >= 0) {
				npImpsSessionRelease(session);
				return 0;
			}
			if (retained != NULL)
				npRtReleaseMessageStanza(retained);
		}
	}

	if (txNode != NULL)
		npImpsRtTxRemoveById(&session->rt.txQueue, txNode->header.requestId);
	if (session != NULL)
		npImpsSessionRelease(session);
	return ret;
}
