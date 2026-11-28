/*
 * np_imps_core_ctl.cpp: SceNpImpsContext and SceNpImpsCtl controller
 * (0x8131B358..0x8131F389).
 *
 * Manages per-process online requests (up to 17 slots), automatic background
 * sync via TriggerMgr ("SHELL0006") and ShellCron, account sign-in/sign-out
 * transitions, outgoing presence/message queues, and the public 7-callback
 * SceNpImpsCore listener registry.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>
#include <libnet/netinternal.h>
#include <np/np_common.h>
#include <common/main_thread_call_list.h>
#include <std/string.h>
#include "np_imps_core.h"
#include "shell_cron.h"
#include "src/trigger_mgr/trigger_mgr.h"

extern "C" int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

/* Shell account event service imports (0x8131B8E0, 0x8131ED4A, 0x8131E840) */
typedef void (*NpImpsShellAccountCallback)(int eventId, int stateGroup, int stateCode,
					   int detailA, int detailB, void *userArg);
extern "C" {
int SceShell_2E0B6FA6(NpImpsShellAccountCallback callback, void *userArg, int *outId);
int SceShell_B4D888D5(int callbackId);
}

/* Constants */
static const SceSize NP_IMPS_CTL_PIPE_BUF_SIZE		= 0x1A00u;
static const SceUInt32 NP_IMPS_CTL_PIPE_TIMEOUT_USEC	= 10000000u;
static const SceInt64 NP_IMPS_POWER_TICK_WINDOW_USEC	= 120000000LL;
static const int NP_IMPS_CTL_MAX_CLIENTS		= 17;
static const int NP_IMPS_CTL_MAX_LISTENERS		= 8;

static const char NP_IMPS_TRIGGER_SERVICE_ID[]		= "SHELL0006";

/* Context flags */
static const SceUInt32 NP_IMPS_CTX_FLAG_LINKED		= 0x01u;
static const SceUInt32 NP_IMPS_CTX_FLAG_CLOSING		= 0x10u;

/* Ctl state machine states (0..15, table at 0x815465D8) */
enum NpImpsCtlState {
	NP_IMPS_CTL_STATE_SIGNED_OUT		= 0,
	NP_IMPS_CTL_STATE_SIGNING_IN		= 1,
	NP_IMPS_CTL_STATE_SIGN_IN_OK		= 2,
	NP_IMPS_CTL_STATE_SIGN_IN_FAIL		= 3,
	NP_IMPS_CTL_STATE_ACTIVE		= 4,
	NP_IMPS_CTL_STATE_WAIT_CRON_SETUP	= 5,
	NP_IMPS_CTL_STATE_WAIT_CRON_WAKE	= 6,
	NP_IMPS_CTL_STATE_RECOVERY		= 7,
	NP_IMPS_CTL_STATE_SUSPENDED		= 8,
	NP_IMPS_CTL_STATE_SIGN_OUT_CLEAN	= 9,
	NP_IMPS_CTL_STATE_SIGNING_OUT		= 10,
	NP_IMPS_CTL_STATE_SIGNED_OUT_RESET	= 11,
	NP_IMPS_CTL_STATE_SIGN_OUT_REENTER	= 12,
	NP_IMPS_CTL_STATE_CLOSING		= 13,
	NP_IMPS_CTL_STATE_CLOSED		= 14,
	NP_IMPS_CTL_STATE_FATAL			= 15
};

/* 52-byte SceNpImpsContext msgpipe message kinds */
enum NpImpsCtlMsgKind {
	NP_IMPS_CTL_MSG_INVALID			= -1,
	NP_IMPS_CTL_MSG_WAKEUP			= 0,
	NP_IMPS_CTL_MSG_CLOSE			= 1,
	NP_IMPS_CTL_MSG_SIGN_IN			= 2,
	NP_IMPS_CTL_MSG_SIGN_OUT		= 3,
	NP_IMPS_CTL_MSG_CLIENT_UPDATE		= 4,
	NP_IMPS_CTL_MSG_PRESENCE_UPDATE		= 5,
	NP_IMPS_CTL_MSG_SEND_MESSAGE		= 6,
	NP_IMPS_CTL_MSG_CRON_FIRED		= 7,
	NP_IMPS_CTL_MSG_ACCOUNT_SYNC		= 8,
	NP_IMPS_CTL_MSG_AUTO_SYNC_RESET		= 9,
	NP_IMPS_CTL_MSG_CORE_SESSION_EVENT	= 10,
	NP_IMPS_CTL_MSG_TX_DONE			= 11
};

/* Client slot allocation kind & lifecycle state */
enum NpImpsClientAllocKind {
	NP_IMPS_CLIENT_FREE		= 0,
	NP_IMPS_CLIENT_INTERNAL		= 1,
	NP_IMPS_CLIENT_PROCESS		= 2
};

enum NpImpsClientSlotState {
	NP_IMPS_CSLOT_NONE		= 0,
	NP_IMPS_CSLOT_QUEUED		= 1,
	NP_IMPS_CSLOT_CONNECTING	= 2,
	NP_IMPS_CSLOT_WAITING		= 3,
	NP_IMPS_CSLOT_DONE		= 4,
	NP_IMPS_CSLOT_ACTIVE		= 5
};

/* Result descriptor passed to client connect callbacks (16 bytes) */
struct NpImpsCtlClientResult {
	SceUID processId;			/* +0x00 */
	int requestId;				/* +0x04 */
	int requestedLevel;			/* +0x08 */
	int errorCode;				/* +0x0C */
};

typedef void (*NpImpsCtlClientCallback)(const int *contextIdPtr,
					const NpImpsCtlClientResult *result,
					void *userArg);

/* Per-client request slot (48 bytes = 0x30) */
struct NpImpsCtlClientSlot {
	struct NpImpsCtlClientMgr *owner;	/* +0x00 */
	int allocKind;				/* +0x04 */
	NpImpsCtlClientCallback callback;	/* +0x08 */
	void *userArg;				/* +0x0C */
	int slotState;				/* +0x10 */
	SceUInt8 canceled;			/* +0x14 */
	SceUInt8 powerTickHeld;			/* +0x15 */
	SceUInt8 padFlags[2];			/* +0x16 */
	NpImpsCtlClientSlot *next;		/* +0x18 */
	NpImpsCtlClientSlot *prev;		/* +0x1C */
	SceUID processId;			/* +0x20 */
	int requestId;				/* +0x24 */
	int requestedLevel;			/* +0x28 */
	int resultCode;				/* +0x2C */
};

/* Client manager embedded at NpImpsCtlContext +0x68 (888 bytes = 0x378) */
struct NpImpsCtlClientMgr {
	SceKernelLwMutexWork *mutex;		/* +0x000 */
	int nextRequestId;			/* +0x004 */
	NpImpsCtlClientSlot slots[NP_IMPS_CTL_MAX_CLIENTS]; /* +0x008 (816 bytes) */
	NpImpsCtlClientSlot *queuedHead;	/* +0x338 (824) */
	NpImpsCtlClientSlot *queuedTail;	/* +0x33C (828) */
	NpImpsCtlClientSlot *connectingHead;	/* +0x340 (832) */
	NpImpsCtlClientSlot *connectingTail;	/* +0x344 (836) */
	NpImpsCtlClientSlot *waitingHead;	/* +0x348 (840) */
	NpImpsCtlClientSlot *waitingTail;	/* +0x34C (844) */
	NpImpsCtlClientSlot *doneHead;		/* +0x350 (848) */
	NpImpsCtlClientSlot *doneTail;		/* +0x354 (852) */
	int desiredLevel;			/* +0x358 (856) */
	int coreSessionState;			/* +0x35C (860) */
	int coreSessionError;			/* +0x360 (864) */
	int autoSyncLevel;			/* +0x364 (868) */
	SceUInt8 dirty;				/* +0x368 (872) */
	SceUInt8 padDirty[3];			/* +0x369 */
	int powerTickRefCount;			/* +0x36C (876) */
	SceInt64 powerTickStartUsec;		/* +0x370 (880) */
};

/* Outgoing ctl message transaction node (80 bytes = 0x50) */
struct NpImpsCtlTxNode {
	NpImpsTxNodeHeader header;		/* +0x00 (20 bytes) */
	NpImpsSessionSendDoneCallback callback;	/* +0x14 */
	void *userArg;				/* +0x18 */
	int txKind;				/* +0x1C (0 = message) */
	SceNpId targetNpId;			/* +0x20 (36 bytes) */
	NpImpsCoreMessage *message;		/* +0x44 (word 17) */
	int resultCode;				/* +0x48 (word 18) */
	int coreTransactionId;			/* +0x4C (word 19) */
};

/* 52-byte context msgpipe message */
struct NpImpsCtlMsg {
	int kind;				/* +0x00 */
	union {
		int requestId;			/* +0x04 (CLIENT_UPDATE, SEND_MESSAGE) */
		NpImpsCorePresenceUpdate *presence; /* +0x04 (PRESENCE_UPDATE) */
		struct {
			NpImpsSessionIdentity session; /* +0x04 (40 bytes) */
			int eventOrTxId;	/* +0x2C (word 11) */
			int errorOrResult;	/* +0x30 (word 12) */
		} coreEvent;
		SceUInt8 raw[48];
	} body;
};

/* SceNpImpsContext object (1096 bytes = 0x448) */
struct NpImpsCtlContext {
	SceUInt32 flags;			/* +0x000 */
	int refCount;				/* +0x004 */
	NpImpsCtlContext *next;			/* +0x008 */
	NpImpsCtlContext *prev;			/* +0x00C */
	SceKernelLwMutexWork mutex;		/* +0x010 (32 bytes) */
	int contextId;				/* +0x030 */
	int state;				/* +0x034 */
	SceUID msgPipeId;			/* +0x038 */
	SceUInt32 padBeforeTimer;		/* +0x03C */
	NpPollingTimeoutTimer timeoutTimer;	/* +0x040 (40 bytes) */
	NpImpsCtlClientMgr clientMgr;		/* +0x068 (888 bytes) */
	NpImpsSessionIdentity sessionIdentity;	/* +0x3E0 (40 bytes) */
	NpImpsTxQueue txQueue;			/* +0x408 (32 bytes) */
	int sessionConnected;			/* +0x428 */
	int sessionError;			/* +0x42C */
	SceUID *workerThreadPtr;		/* +0x430 */
	int aborted;				/* +0x434 */
	NpImpsCorePresenceUpdate *cachedPresence; /* +0x438 */
	int lastError;				/* +0x43C */
	SceUID cronHandle;			/* +0x440 */
	int accountEventCbId;			/* +0x444 */
};

/* Registered public listener node (40 bytes) */
struct NpImpsCtlListenerNode {
	NpImpsCtlListenerNode *next;		/* +0x00 */
	NpImpsCtlListenerNode *prev;		/* +0x04 */
	int listenerId;				/* +0x08 */
	NpImpsCoreCallbacks callbacks;		/* +0x0C (28 bytes) */
};

/* Global SceNpImpsCtl state */
static SceKernelLwMutexWork s_ctlMutex;
static int s_ctlMutexInit = 0;

static int s_netAccountCbId = -1;
static int s_defaultCtlCtxId = 0;
static SceUInt8 s_defaultCtlStarted = 0;

static int s_ctxMgrInit = 0;
static NpImpsCtlContext *s_ctxListHead = NULL;
static NpImpsCtlContext *s_ctxListTail = NULL;
static int s_ctxCount = 0;
static int s_nextCtxId = 0;
static SceUID s_ctlThreadId = -1;

static NpImpsCtlListenerNode *s_ctlListenerHead = NULL;
static NpImpsCtlListenerNode *s_ctlListenerTail = NULL;
static int s_ctlListenerCount = 0;
static int s_ctlNextListenerId = 0;
static int s_coreSessionCbId = 0;

/* Forward declarations */
static int npImpsCtlMainLoop(NpImpsCtlContext *ctx);
static void npImpsCtlMsgCleanup(NpImpsCtlMsg *msg);
static int npImpsCtlSendWakeup(SceUID *pipeId);
static int npImpsCtlNotifyAutoSyncChanged(void);
static int npImpsCtlTriggerSignIn(void);
static int npImpsCtlTriggerSignOut(void);

/* ============================================================================
 * Global SceNpImpsCtl mutex (0x8131E274..0x8131E317)
 * ============================================================================ */

static int npImpsCtlMutexCreate(void)
{
	if (s_ctlMutexInit > 0)
		return NP_IMPS_CORE_ERR_ALREADY_INIT;
	int ret = sceKernelCreateLwMutex(&s_ctlMutex, "SceNpImpsCtl", 0x2002u, 0, NULL);
	if (ret >= 0)
		s_ctlMutexInit = 1;
	return ret;
}

static int npImpsCtlMutexDelete(void)
{
	if (s_ctlMutexInit >= 0)
		return sceKernelDeleteLwMutex(&s_ctlMutex);
	return s_ctlMutexInit;
}

static int npImpsCtlMutexLock(void)
{
	int ret = sceKernelLockLwMutex(&s_ctlMutex, 1, NULL);
	if (ret >= 0)
		return 1;
	return ret;
}

static int npImpsCtlMutexUnlock(int lockState)
{
	if (lockState > 0)
		sceKernelUnlockLwMutex(&s_ctlMutex, 1);
	return 0;
}

static int npImpsCtxLock(NpImpsCtlContext *ctx)
{
	int ret = sceKernelLockLwMutex(&ctx->mutex, 1, NULL);
	if (ret >= 0)
		return 1;
	return ret;
}

static int npImpsCtxUnlock(NpImpsCtlContext *ctx, int lockState)
{
	if (lockState > 0)
		sceKernelUnlockLwMutex(&ctx->mutex, 1);
	return 0;
}

/* ============================================================================
 * Context msgpipe helpers (0x8131BEC8..0x8131C0F7, 0x8131CBFA..0x8131CDAF)
 * ============================================================================ */

static int npImpsCtlPipeCreate(SceUID *pipeId)
{
	SceUID uid = sceKernelCreateMsgPipe("SceNpImpsContext", 64, 12,
					    NP_IMPS_CTL_PIPE_BUF_SIZE, NULL);
	if (uid < 0)
		return uid;
	*pipeId = uid;
	return 0;
}

static void npImpsCtlMsgCleanup(NpImpsCtlMsg *msg)
{
	if (msg->kind == NP_IMPS_CTL_MSG_PRESENCE_UPDATE && msg->body.presence != NULL)
		npRtImpsFreePresenceUpdate(msg->body.presence, NULL);
	msg->kind = NP_IMPS_CTL_MSG_INVALID;
}

static void npImpsCtlPipeDestroy(SceUID *pipeId)
{
	NpImpsCtlMsg msg;
	SceSize received = 0;
	while (sceKernelTryReceiveMsgPipe(*pipeId, &msg, sizeof(msg), 1, &received) >= 0 &&
	       received == sizeof(msg)) {
		npImpsCtlMsgCleanup(&msg);
	}
	sceKernelDeleteMsgPipe(*pipeId);
}

static int npImpsCtlPipeSend(SceUID *pipeId, const NpImpsCtlMsg *msg, SceUInt32 *timeout)
{
	SceSize sent = 0;
	int ret = sceKernelSendMsgPipe(*pipeId, const_cast<NpImpsCtlMsg *>(msg),
				       sizeof(*msg), 1, &sent, timeout);
	if (ret == static_cast<int>(0x80028005u))
		return NP_IMPS_CTL_ERR_PIPE_TIMEOUT;
	if (ret >= 0)
		return (sent == sizeof(*msg)) ? 0 : NP_IMPS_CTL_ERR_INTERNAL;
	return ret;
}

static int npImpsCtlPipeRecv(SceUID *pipeId, NpImpsCtlMsg *msg, SceUInt32 *timeout)
{
	SceSize received = 0;
	int ret = sceKernelReceiveMsgPipeCB(*pipeId, msg, sizeof(*msg), 1, &received, timeout);
	if (ret == static_cast<int>(0x80028005u)) {
		ret = NP_IMPS_CTL_ERR_PIPE_TIMEOUT;
	} else if (ret >= 0) {
		if (received == sizeof(*msg))
			return 0;
		ret = NP_IMPS_CTL_ERR_INTERNAL;
	}
	msg->kind = NP_IMPS_CTL_MSG_INVALID;
	return ret;
}

static int npImpsCtlPipeTryRecv(SceUID *pipeId, NpImpsCtlMsg *msg)
{
	SceSize received = 0;
	int ret = sceKernelTryReceiveMsgPipe(*pipeId, msg, sizeof(*msg), 1, &received);
	if (ret == static_cast<int>(0x80028223u)) {
		msg->kind = NP_IMPS_CTL_MSG_INVALID;
		return 0;
	}
	if (ret >= 0) {
		if (received == sizeof(*msg))
			return 0;
		ret = NP_IMPS_CTL_ERR_INTERNAL;
	}
	msg->kind = NP_IMPS_CTL_MSG_INVALID;
	return ret;
}

static int npImpsCtlSendWakeup(SceUID *pipeId)
{
	NpImpsCtlMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_CTL_MSG_WAKEUP;
	SceUInt32 timeout = NP_IMPS_CTL_PIPE_TIMEOUT_USEC;
	return npImpsCtlPipeSend(pipeId, &msg, &timeout);
}

/* 0x8131CBFA */
static int npImpsCtlRecvFilteredMsg(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, SceUInt32 *timeout)
{
	while (true) {
		int ret = npImpsCtlPipeRecv(&ctx->msgPipeId, msg, timeout);
		if (ret < 0)
			return ret;
		if (msg->kind == NP_IMPS_CTL_MSG_CORE_SESSION_EVENT ||
		    msg->kind == NP_IMPS_CTL_MSG_TX_DONE) {
			if (msg->body.coreEvent.session.sessionId == ctx->sessionIdentity.sessionId)
				return 0;
		} else {
			return 0;
		}
		npImpsCtlMsgCleanup(msg);
	}
}

/* 0x8131CC54 */
static int npImpsCtlPostCommand(NpImpsCtlContext *ctx, const SceUInt32 *allowedStateMask,
				const NpImpsCtlMsg *msg, SceUInt8 allowClosing)
{
	int ret = sceKernelLockLwMutex(&ctx->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (allowedStateMask != NULL && ((1u << ctx->state) & *allowedStateMask) == 0) {
		sceKernelUnlockLwMutex(&ctx->mutex, 1);
		return NP_IMPS_CTL_ERR_BAD_STATE;
	}

	if ((ctx->flags & NP_IMPS_CTX_FLAG_LINKED) == 0 ||
	    (allowClosing == 0 && (ctx->flags & NP_IMPS_CTX_FLAG_CLOSING) != 0)) {
		sceKernelUnlockLwMutex(&ctx->mutex, 1);
		return NP_IMPS_CTL_ERR_CTX_NOT_FOUND;
	}

	sceKernelUnlockLwMutex(&ctx->mutex, 1);
	SceUInt32 timeout = NP_IMPS_CTL_PIPE_TIMEOUT_USEC;
	return npImpsCtlPipeSend(&ctx->msgPipeId, msg, &timeout);
}

/* ============================================================================
 * Outgoing Ctl message transaction queue (0x8131CDB0..0x8131D267)
 * ============================================================================ */

static void npImpsCtlTxFreePayload(NpImpsCtlTxNode *node)
{
	if (node->txKind == 0 && node->message != NULL)
		npRtImpsFreeMessage(node->message, NULL);
}

/* 0x8131CDB0 */
static void npImpsCtlTxClearAll(NpImpsTxQueue *queue)
{
	while (queue->pendingHead != NULL) {
		NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(queue->pendingHead);
		npImpsTxQueueUnlink(queue, &node->header);
		npImpsCtlTxFreePayload(node);
		npRtHeapFree(node, NULL);
		queue->totalCount--;
	}
	while (queue->inFlightHead != NULL) {
		NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(queue->inFlightHead);
		npImpsTxQueueUnlink(queue, &node->header);
		npImpsCtlTxFreePayload(node);
		npRtHeapFree(node, NULL);
		queue->totalCount--;
	}
	while (queue->completedHead != NULL) {
		NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(queue->completedHead);
		npImpsTxQueueUnlink(queue, &node->header);
		npImpsCtlTxFreePayload(node);
		npRtHeapFree(node, NULL);
		queue->totalCount--;
	}
}

static void npImpsCtlTxAppendLocked(NpImpsTxNodeHeader **headPtr, NpImpsTxNodeHeader **tailPtr,
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

/* 0x8131CE3A */
static int npImpsCtlTxCreate(NpImpsTxQueue *queue, int requestId,
			     NpImpsSessionSendDoneCallback callback, void *userArg,
			     NpImpsCtlTxNode **outNode)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(
		npRtHeapAlloc(sizeof(NpImpsCtlTxNode), NULL));
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
	node->coreTransactionId = 0;
	npImpsCtlTxAppendLocked(&queue->pendingHead, &queue->pendingTail, &node->header, 1);
	*outNode = node;

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x8131CF20 */
static int npImpsCtlTxFree(NpImpsTxQueue *queue, NpImpsCtlTxNode *node)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	npImpsTxQueueUnlink(queue, &node->header);
	npImpsCtlTxFreePayload(node);
	npRtHeapFree(node, NULL);
	queue->totalCount--;

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x8131CF78 */
static int npImpsCtlTxRemoveById(NpImpsTxQueue *queue, int requestId)
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
		NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(found);
		npImpsTxQueueUnlink(queue, found);
		npImpsCtlTxFreePayload(node);
		npRtHeapFree(node, NULL);
		queue->totalCount--;
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x8131D00A */
static int npImpsCtlTxMarkInFlight(NpImpsTxQueue *queue, NpImpsCtlTxNode *node, int coreTxId)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	npImpsTxQueueUnlink(queue, &node->header);
	npImpsCtlTxAppendLocked(&queue->inFlightHead, &queue->inFlightTail, &node->header, 2);
	node->coreTransactionId = coreTxId;

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x8131D072 */
static int npImpsCtlTxCompleteByCoreId(NpImpsTxQueue *queue, int coreTxId, int resultCode)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (NpImpsTxNodeHeader *cur = queue->inFlightHead; cur != NULL; cur = cur->next) {
		NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(cur);
		if (node->coreTransactionId == coreTxId) {
			npImpsTxQueueUnlink(queue, &node->header);
			npImpsCtlTxAppendLocked(&queue->completedHead, &queue->completedTail,
						&node->header, 3);
			node->resultCode = resultCode;
			break;
		}
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x8131D0E4 */
static int npImpsCtlTxFailPendingById(NpImpsTxQueue *queue, int requestId, int resultCode)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (NpImpsTxNodeHeader *cur = queue->pendingHead; cur != NULL; cur = cur->next) {
		if (cur->requestId == requestId) {
			NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(cur);
			npImpsTxQueueUnlink(queue, &node->header);
			npImpsCtlTxAppendLocked(&queue->completedHead, &queue->completedTail,
						&node->header, 3);
			node->resultCode = resultCode;
			break;
		}
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x8131D156 */
static int npImpsCtlTxFailAll(NpImpsTxQueue *queue, int resultCode)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	while (queue->pendingHead != NULL) {
		NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(queue->pendingHead);
		npImpsTxQueueUnlink(queue, &node->header);
		npImpsCtlTxAppendLocked(&queue->completedHead, &queue->completedTail,
					&node->header, 3);
		node->resultCode = resultCode;
	}
	while (queue->inFlightHead != NULL) {
		NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(queue->inFlightHead);
		npImpsTxQueueUnlink(queue, &node->header);
		npImpsCtlTxAppendLocked(&queue->completedHead, &queue->completedTail,
					&node->header, 3);
		node->resultCode = resultCode;
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x8131D1F8 */
static int npImpsCtlTxFailInFlight(NpImpsTxQueue *queue, int resultCode)
{
	int lockRet = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	while (queue->inFlightHead != NULL) {
		NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(queue->inFlightHead);
		npImpsTxQueueUnlink(queue, &node->header);
		npImpsCtlTxAppendLocked(&queue->completedHead, &queue->completedTail,
					&node->header, 3);
		node->resultCode = resultCode;
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* ============================================================================
 * Per-process Client Manager (0x8131D268..0x8131E273)
 * ============================================================================ */

/* 0x8131E22C */
static void npImpsCtlPowerTickCallback(void *userArg)
{
	NpImpsCtlClientMgr *mgr = reinterpret_cast<NpImpsCtlClientMgr *>(userArg);
	SceInt64 nowUsec = static_cast<SceInt64>(sceKernelGetProcessTimeWide());
	if (nowUsec > mgr->powerTickStartUsec &&
	    (nowUsec - mgr->powerTickStartUsec) < NP_IMPS_POWER_TICK_WINDOW_USEC) {
		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
	}
}

static void npImpsCtlClientAcquirePowerTick(NpImpsCtlClientMgr *mgr, NpImpsCtlClientSlot *slot)
{
	if (slot->powerTickHeld == 0) {
		slot->powerTickHeld = 1;
		if (mgr->powerTickRefCount == 0) {
			mgr->powerTickStartUsec = static_cast<SceInt64>(sceKernelGetProcessTimeWide());
			paf::common::MainThreadCallList::Register(npImpsCtlPowerTickCallback, mgr);
		}
		mgr->powerTickRefCount++;
	}
}

/* 0x8131D268 */
static void npImpsCtlClientUnlink(NpImpsCtlClientMgr *mgr, NpImpsCtlClientSlot *slot)
{
	if (slot->powerTickHeld != 0) {
		slot->powerTickHeld = 0;
		mgr->powerTickRefCount--;
		if (mgr->powerTickRefCount == 0)
			paf::common::MainThreadCallList::Unregister(npImpsCtlPowerTickCallback, mgr);
	}

	NpImpsCtlClientSlot **headPtr = NULL;
	NpImpsCtlClientSlot **tailPtr = NULL;
	if (slot->slotState == NP_IMPS_CSLOT_QUEUED) {
		headPtr = &mgr->queuedHead;
		tailPtr = &mgr->queuedTail;
	} else if (slot->slotState == NP_IMPS_CSLOT_CONNECTING) {
		headPtr = &mgr->connectingHead;
		tailPtr = &mgr->connectingTail;
	} else if (slot->slotState == NP_IMPS_CSLOT_WAITING) {
		headPtr = &mgr->waitingHead;
		tailPtr = &mgr->waitingTail;
	} else if (slot->slotState == NP_IMPS_CSLOT_DONE) {
		headPtr = &mgr->doneHead;
		tailPtr = &mgr->doneTail;
	} else {
		return;
	}

	if (slot->next != NULL)
		slot->next->prev = slot->prev;
	else
		*tailPtr = slot->prev;

	if (slot->prev != NULL)
		slot->prev->next = slot->next;
	else
		*headPtr = slot->next;

	slot->next = NULL;
	slot->prev = NULL;
}

static void npImpsCtlClientMoveToList(NpImpsCtlClientMgr *mgr, NpImpsCtlClientSlot *slot,
				      NpImpsCtlClientSlot **headPtr,
				      NpImpsCtlClientSlot **tailPtr, int newState)
{
	npImpsCtlClientUnlink(mgr, slot);
	slot->slotState = newState;
	slot->next = NULL;
	slot->prev = *tailPtr;
	if (*tailPtr != NULL)
		(*tailPtr)->next = slot;
	if (*headPtr == NULL)
		*headPtr = slot;
	*tailPtr = slot;
}

/* 0x8131D64E */
static void npImpsCtlClientMgrInit(NpImpsCtlClientMgr *mgr, SceKernelLwMutexWork *mutex)
{
	mgr->mutex = mutex;
	mgr->nextRequestId = 0;
	mgr->queuedHead = NULL;
	mgr->queuedTail = NULL;
	mgr->connectingHead = NULL;
	mgr->connectingTail = NULL;
	mgr->waitingHead = NULL;
	mgr->waitingTail = NULL;
	mgr->doneHead = NULL;
	mgr->doneTail = NULL;
	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		mgr->slots[slotIdx].processId = -1;
		mgr->slots[slotIdx].requestId = 0;
	}
	mgr->desiredLevel = NP_IMPS_MODE_NONE;
	mgr->coreSessionState = -1;
	mgr->coreSessionError = 0;
	mgr->autoSyncLevel = NP_IMPS_MODE_CRON_POLL;
	mgr->dirty = 1;
	mgr->powerTickRefCount = 0;
}

/* 0x8131D6AE */
static void npImpsCtlClientMgrTerm(NpImpsCtlClientMgr *mgr)
{
	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
		if (slot->allocKind != NP_IMPS_CLIENT_FREE && slot->powerTickHeld != 0) {
			slot->powerTickHeld = 0;
			mgr->powerTickRefCount--;
			if (mgr->powerTickRefCount == 0) {
				paf::common::MainThreadCallList::Unregister(
					npImpsCtlPowerTickCallback, mgr);
			}
		}
	}
}

/* 0x8131D33C */
static int npImpsCtlClientMgrEvaluateLocked(NpImpsCtlClientMgr *mgr, int *outTargetLevel)
{
	int coreState = mgr->coreSessionState;
	bool autoSyncEnabled = false;
	shellTriggerMgrIsServiceEnabled(NP_IMPS_TRIGGER_SERVICE_ID, &autoSyncEnabled);

	int targetLevel = autoSyncEnabled ? mgr->autoSyncLevel : NP_IMPS_MODE_DISCONNECT;

	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
		if (slot->allocKind == NP_IMPS_CLIENT_FREE || slot->processId == -1 ||
		    slot->canceled != 0) {
			continue;
		}

		if (slot->slotState == NP_IMPS_CSLOT_CONNECTING) {
			bool satisfied = false;
			if (coreState == NP_IMPS_EVT_ONLINE &&
			    (slot->requestedLevel == NP_IMPS_MODE_ONLINE ||
			     slot->requestedLevel == NP_IMPS_MODE_INTERVAL_POLL ||
			     slot->requestedLevel == NP_IMPS_MODE_CRON_POLL)) {
				satisfied = true;
			} else if (coreState == NP_IMPS_EVT_POLL_INTERVAL &&
				   (slot->requestedLevel == NP_IMPS_MODE_INTERVAL_POLL ||
				    slot->requestedLevel == NP_IMPS_MODE_CRON_POLL)) {
				satisfied = true;
			} else if (coreState == NP_IMPS_EVT_POLL_CRON &&
				   slot->requestedLevel == NP_IMPS_MODE_CRON_POLL) {
				satisfied = true;
			}
			if (satisfied) {
				npImpsCtlClientMoveToList(mgr, slot, &mgr->doneHead, &mgr->doneTail,
							  NP_IMPS_CSLOT_DONE);
				slot->resultCode = 0;
			}
		}

		if ((slot->slotState == NP_IMPS_CSLOT_CONNECTING ||
		     slot->slotState == NP_IMPS_CSLOT_DONE ||
		     slot->slotState == NP_IMPS_CSLOT_ACTIVE) &&
		    targetLevel < slot->requestedLevel) {
			targetLevel = slot->requestedLevel;
		}

		if (slot->slotState == NP_IMPS_CSLOT_CONNECTING) {
			npImpsCtlClientMoveToList(mgr, slot, &mgr->waitingHead, &mgr->waitingTail,
						  NP_IMPS_CSLOT_WAITING);
			npImpsCtlClientAcquirePowerTick(mgr, slot);
		}
	}

	if ((targetLevel == NP_IMPS_MODE_ONLINE && coreState == NP_IMPS_EVT_ONLINE) ||
	    (targetLevel == NP_IMPS_MODE_INTERVAL_POLL && coreState == NP_IMPS_EVT_POLL_INTERVAL) ||
	    (targetLevel == NP_IMPS_MODE_CRON_POLL && coreState == NP_IMPS_EVT_POLL_CRON) ||
	    (targetLevel == NP_IMPS_MODE_DISCONNECT &&
	     (coreState == -1 || coreState == NP_IMPS_EVT_STOPPING ||
	      coreState == NP_IMPS_EVT_STOPPED))) {
		targetLevel = NP_IMPS_MODE_NONE;
	}

	if (mgr->waitingHead == NULL) {
		if (mgr->dirty != 0 && targetLevel != NP_IMPS_MODE_NONE) {
			NpImpsCtlClientSlot *internalSlot = NULL;
			for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
				if (mgr->slots[slotIdx].allocKind == NP_IMPS_CLIENT_FREE) {
					internalSlot = &mgr->slots[slotIdx];
					break;
				}
			}
			if (internalSlot == NULL)
				return NP_IMPS_CTL_ERR_NO_SLOT;

			sceClibMemset(internalSlot, 0, sizeof(*internalSlot));
			internalSlot->allocKind = NP_IMPS_CLIENT_INTERNAL;
			internalSlot->slotState = NP_IMPS_CSLOT_NONE;
			internalSlot->processId = -1;
			internalSlot->requestId = 0;
			internalSlot->requestedLevel = targetLevel;
			npImpsCtlClientMoveToList(mgr, internalSlot, &mgr->waitingHead,
						  &mgr->waitingTail, NP_IMPS_CSLOT_WAITING);
			npImpsCtlClientAcquirePowerTick(mgr, internalSlot);
		} else {
			targetLevel = NP_IMPS_MODE_NONE;
		}
		if (mgr->dirty != 0)
			mgr->dirty = 0;
	}

	*outTargetLevel = targetLevel;
	return 0;
}

/* 0x8131D6F0 */
static int npImpsCtlClientMgrAdd(NpImpsCtlClientMgr *mgr, SceUID processId, int requestedLevel,
				 NpImpsCtlClientCallback callback, void *userArg,
				 int *outRequestId)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	NpImpsCtlClientSlot *slot = NULL;
	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *cur = &mgr->slots[slotIdx];
		if (cur->allocKind != NP_IMPS_CLIENT_FREE && cur->canceled == 0 &&
		    cur->processId == processId) {
			slot = cur;
			break;
		}
	}

	if (slot != NULL) {
		if (slot->slotState != NP_IMPS_CSLOT_ACTIVE) {
			sceKernelUnlockLwMutex(mgr->mutex, 1);
			return NP_IMPS_CTL_ERR_CLIENT_LIMIT;
		}
	} else {
		for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
			if (mgr->slots[slotIdx].allocKind == NP_IMPS_CLIENT_FREE) {
				slot = &mgr->slots[slotIdx];
				break;
			}
		}
		if (slot == NULL) {
			sceKernelUnlockLwMutex(mgr->mutex, 1);
			return NP_IMPS_CTL_ERR_NO_SLOT;
		}
		sceClibMemset(slot, 0, sizeof(*slot));
		slot->allocKind = NP_IMPS_CLIENT_PROCESS;
		slot->slotState = NP_IMPS_CSLOT_NONE;
	}

	slot->next = NULL;
	slot->prev = NULL;
	slot->canceled = 0;
	slot->owner = mgr;
	slot->callback = callback;
	slot->userArg = userArg;

	mgr->nextRequestId++;
	if (mgr->nextRequestId == 0)
		mgr->nextRequestId = 1;
	slot->processId = processId;
	slot->requestId = mgr->nextRequestId;
	slot->requestedLevel = requestedLevel;

	npImpsCtlClientMoveToList(mgr, slot, &mgr->queuedHead, &mgr->queuedTail,
				  NP_IMPS_CSLOT_QUEUED);
	mgr->dirty = 1;
	*outRequestId = slot->requestId;

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131D842 */
static int npImpsCtlClientMgrRemoveByReqId(NpImpsCtlClientMgr *mgr, int requestId,
					   int *outActiveCount)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
		if (slot->allocKind != NP_IMPS_CLIENT_FREE && slot->requestId == requestId) {
			npImpsCtlClientUnlink(mgr, slot);
			slot->slotState = NP_IMPS_CSLOT_NONE;
			slot->allocKind = NP_IMPS_CLIENT_FREE;
			break;
		}
	}

	if (outActiveCount != NULL) {
		int count = 0;
		for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
			if (mgr->slots[slotIdx].allocKind != NP_IMPS_CLIENT_FREE &&
			    mgr->slots[slotIdx].canceled == 0) {
				count++;
			}
		}
		*outActiveCount = count;
	}

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131D8D6 */
static int npImpsCtlClientMgrMarkDirty(NpImpsCtlClientMgr *mgr)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;
	mgr->dirty = 1;
	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131D912 */
static int npImpsCtlClientCancelSlotLocked(NpImpsCtlClientMgr *mgr, NpImpsCtlClientSlot *slot)
{
	int st = slot->slotState;
	if (st == NP_IMPS_CSLOT_QUEUED || st == NP_IMPS_CSLOT_CONNECTING ||
	    st == NP_IMPS_CSLOT_WAITING) {
		npImpsCtlClientMoveToList(mgr, slot, &mgr->doneHead, &mgr->doneTail,
					  NP_IMPS_CSLOT_DONE);
		slot->resultCode = NP_IMPS_CTL_ERR_CANCELED;
		slot->canceled = 1;
		return 0;
	}
	if (st == NP_IMPS_CSLOT_DONE) {
		slot->canceled = 1;
		return 0;
	}
	npImpsCtlClientUnlink(mgr, slot);
	slot->slotState = NP_IMPS_CSLOT_NONE;
	slot->allocKind = NP_IMPS_CLIENT_FREE;
	return 0;
}

/* 0x8131D982 */
static int npImpsCtlClientMgrCancelByPid(NpImpsCtlClientMgr *mgr, SceUID processId,
					 int *outActiveCount)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
		if (slot->allocKind != NP_IMPS_CLIENT_FREE && slot->canceled == 0 &&
		    slot->processId == processId) {
			npImpsCtlClientCancelSlotLocked(mgr, slot);
			break;
		}
	}

	mgr->dirty = 1;
	if (outActiveCount != NULL) {
		int count = 0;
		for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
			if (mgr->slots[slotIdx].allocKind != NP_IMPS_CLIENT_FREE &&
			    mgr->slots[slotIdx].canceled == 0) {
				count++;
			}
		}
		*outActiveCount = count;
	}

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131DA18 */
static int npImpsCtlClientMgrCancelAllProcesses(NpImpsCtlClientMgr *mgr)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
		if (slot->allocKind != NP_IMPS_CLIENT_FREE && slot->processId != -1)
			npImpsCtlClientCancelSlotLocked(mgr, slot);
	}
	mgr->dirty = 1;

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131DA88 */
static int npImpsCtlClientMgrPromoteQueued(NpImpsCtlClientMgr *mgr, int requestId,
					   NpImpsCtlClientResult *outInfo)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	NpImpsCtlClientSlot *found = NULL;
	for (NpImpsCtlClientSlot *cur = mgr->queuedHead; cur != NULL; cur = cur->next) {
		if (cur->requestId == requestId) {
			found = cur;
			break;
		}
	}

	if (found != NULL) {
		outInfo->processId = found->processId;
		outInfo->requestId = found->requestId;
		outInfo->requestedLevel = found->requestedLevel;
		outInfo->errorCode = found->resultCode;
		npImpsCtlClientMoveToList(mgr, found, &mgr->connectingHead, &mgr->connectingTail,
					  NP_IMPS_CSLOT_CONNECTING);
	} else {
		sceClibMemset(outInfo, 0, sizeof(*outInfo));
		outInfo->processId = -1;
		outInfo->errorCode = NP_IMPS_CORE_ERR_INTERNAL;
	}

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131DB56 */
static int npImpsCtlClientMgrFailByReqId(NpImpsCtlClientMgr *mgr, int requestId, int errorCode)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
		if (slot->allocKind != NP_IMPS_CLIENT_FREE && slot->requestId == requestId) {
			int st = slot->slotState;
			if (st == NP_IMPS_CSLOT_QUEUED || st == NP_IMPS_CSLOT_CONNECTING ||
			    st == NP_IMPS_CSLOT_WAITING) {
				npImpsCtlClientMoveToList(mgr, slot, &mgr->doneHead, &mgr->doneTail,
							  NP_IMPS_CSLOT_DONE);
				slot->resultCode = errorCode;
			}
			break;
		}
	}

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131DBF0 */
static int npImpsCtlClientMgrFailAllPending(NpImpsCtlClientMgr *mgr, int errorCode)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
		if (slot->allocKind != NP_IMPS_CLIENT_FREE) {
			int st = slot->slotState;
			if (st == NP_IMPS_CSLOT_QUEUED || st == NP_IMPS_CSLOT_CONNECTING ||
			    st == NP_IMPS_CSLOT_WAITING) {
				npImpsCtlClientMoveToList(mgr, slot, &mgr->doneHead, &mgr->doneTail,
							  NP_IMPS_CSLOT_DONE);
				slot->resultCode = errorCode;
			}
		}
	}

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131DC7C */
static int npImpsCtlClientMgrFlushDone(NpImpsCtlClientMgr *mgr, const int *contextIdPtr)
{
	while (true) {
		int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
		if (lockRet < 0)
			return lockRet;

		NpImpsCtlClientResult resultInfo;
		NpImpsCtlClientCallback callback = NULL;
		void *userArg = NULL;
		sceClibMemset(&resultInfo, 0, sizeof(resultInfo));
		resultInfo.processId = -1;
		resultInfo.errorCode = NP_IMPS_CORE_ERR_INTERNAL;

		NpImpsCtlClientSlot *slot = mgr->doneHead;
		if (slot != NULL) {
			if (slot->allocKind == NP_IMPS_CLIENT_PROCESS) {
				npImpsCtlClientUnlink(mgr, slot);
				slot->slotState = NP_IMPS_CSLOT_ACTIVE;
				resultInfo.processId = slot->processId;
				resultInfo.requestId = slot->requestId;
				resultInfo.requestedLevel = slot->requestedLevel;
				resultInfo.errorCode = slot->resultCode;
				callback = slot->callback;
				userArg = slot->userArg;
				if (slot->canceled != 0) {
					npImpsCtlClientUnlink(mgr, slot);
					slot->slotState = NP_IMPS_CSLOT_NONE;
					slot->allocKind = NP_IMPS_CLIENT_FREE;
				}
			} else if (slot->allocKind == NP_IMPS_CLIENT_INTERNAL) {
				npImpsCtlClientUnlink(mgr, slot);
				slot->slotState = NP_IMPS_CSLOT_NONE;
				slot->allocKind = NP_IMPS_CLIENT_FREE;
			}
		}

		sceKernelUnlockLwMutex(mgr->mutex, 1);

		if (resultInfo.requestId != 0 && callback != NULL)
			callback(contextIdPtr, &resultInfo, userArg);

		int relock = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
		int coreState = mgr->coreSessionState;
		if (coreState == NP_IMPS_EVT_SUSPENDED || coreState == NP_IMPS_EVT_STOPPING ||
		    coreState == NP_IMPS_EVT_STOPPED) {
			for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
				NpImpsCtlClientSlot *cur = &mgr->slots[slotIdx];
				if (cur->allocKind != NP_IMPS_CLIENT_FREE &&
				    cur->processId != -1 && cur->canceled == 0 &&
				    cur->slotState == NP_IMPS_CSLOT_ACTIVE) {
					npImpsCtlClientUnlink(mgr, cur);
					cur->slotState = NP_IMPS_CSLOT_NONE;
					cur->allocKind = NP_IMPS_CLIENT_FREE;
				}
			}
		}
		if (relock >= 0)
			sceKernelUnlockLwMutex(mgr->mutex, 1);

		if (resultInfo.requestId == 0)
			return 0;
	}
}

/* 0x8131DE02 */
static int npImpsCtlClientMgrOnSessionState(NpImpsCtlClientMgr *mgr, int coreState, int errorCode)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	if (coreState >= NP_IMPS_EVT_ONLINE && coreState <= NP_IMPS_EVT_STOPPED) {
		NpImpsCtlClientSlot *cur = mgr->waitingHead;
		while (cur != NULL) {
			NpImpsCtlClientSlot *next = cur->next;
			if (coreState == NP_IMPS_EVT_ONLINE) {
				if (cur->requestedLevel == NP_IMPS_MODE_ONLINE ||
				    cur->requestedLevel == NP_IMPS_MODE_INTERVAL_POLL ||
				    cur->requestedLevel == NP_IMPS_MODE_CRON_POLL) {
					npImpsCtlClientMoveToList(mgr, cur, &mgr->doneHead,
								  &mgr->doneTail,
								  NP_IMPS_CSLOT_DONE);
					cur->resultCode = 0;
				}
			} else if (coreState == NP_IMPS_EVT_POLL_INTERVAL) {
				if (cur->requestedLevel == NP_IMPS_MODE_INTERVAL_POLL ||
				    cur->requestedLevel == NP_IMPS_MODE_CRON_POLL) {
					npImpsCtlClientMoveToList(mgr, cur, &mgr->doneHead,
								  &mgr->doneTail,
								  NP_IMPS_CSLOT_DONE);
					cur->resultCode = 0;
				}
			} else if (coreState == NP_IMPS_EVT_POLL_CRON) {
				if (cur->requestedLevel == NP_IMPS_MODE_CRON_POLL) {
					npImpsCtlClientMoveToList(mgr, cur, &mgr->doneHead,
								  &mgr->doneTail,
								  NP_IMPS_CSLOT_DONE);
					cur->resultCode = 0;
				}
			} else {
				npImpsCtlClientMoveToList(mgr, cur, &mgr->doneHead,
							  &mgr->doneTail, NP_IMPS_CSLOT_DONE);
				cur->requestedLevel = NP_IMPS_MODE_NONE;
				cur->resultCode = (errorCode < 0)
					? errorCode
					: NP_IMPS_CTL_ERR_DISCONNECTED;
			}
			cur = next;
		}
		mgr->coreSessionState = coreState;
		mgr->coreSessionError = errorCode;
	}

	if (coreState == NP_IMPS_EVT_SUSPENDED || coreState == NP_IMPS_EVT_STOPPED) {
		for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
			NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
			if (slot->allocKind != NP_IMPS_CLIENT_FREE && slot->processId != -1 &&
			    slot->canceled == 0 &&
			    (slot->slotState == NP_IMPS_CSLOT_DONE ||
			     slot->slotState == NP_IMPS_CSLOT_ACTIVE)) {
				slot->requestedLevel = NP_IMPS_MODE_NONE;
			}
		}
		if (mgr->desiredLevel != NP_IMPS_MODE_DISCONNECT &&
		    (errorCode == NP_ACCOUNT_ERR_AUTH_EXPIRED ||
		     errorCode == NP_ACCOUNT_ERR_TOKEN_REJECTED ||
		     errorCode == NP_ACCOUNT_ERR_TICKET_INVALID)) {
			npAccountClearSession();
		}
	}

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131E008 */
static int npImpsCtlClientMgrSetAutoSyncLevel(NpImpsCtlClientMgr *mgr, int level)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;
	mgr->autoSyncLevel = level;
	mgr->dirty = 1;
	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131E04A */
static int npImpsCtlClientMgrResetWaiting(NpImpsCtlClientMgr *mgr, int errorCode)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
		if (slot->allocKind != NP_IMPS_CLIENT_FREE &&
		    slot->slotState == NP_IMPS_CSLOT_WAITING) {
			npImpsCtlClientMoveToList(mgr, slot, &mgr->doneHead, &mgr->doneTail,
						  NP_IMPS_CSLOT_DONE);
			slot->resultCode = errorCode;
		}
	}

	mgr->desiredLevel = NP_IMPS_MODE_NONE;
	mgr->coreSessionError = 0;
	mgr->coreSessionState = -1;

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* 0x8131E0EC */
static int npImpsCtlClientMgrResolveTarget(NpImpsCtlClientMgr *mgr, SceUInt8 forceDisconnect,
					   int *outTargetLevel)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	int ret = 0;
	if (mgr->waitingHead != NULL) {
		*outTargetLevel = NP_IMPS_MODE_NONE;
	} else {
		int target = NP_IMPS_MODE_NONE;
		ret = npImpsCtlClientMgrEvaluateLocked(mgr, &target);
		if (ret >= 0) {
			if (forceDisconnect != 0 && target == NP_IMPS_MODE_NONE)
				target = NP_IMPS_MODE_DISCONNECT;
			if (target == NP_IMPS_MODE_NONE) {
				*outTargetLevel = NP_IMPS_MODE_NONE;
			} else {
				*outTargetLevel = target;
				mgr->desiredLevel = target;
			}
		}
	}

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return ret;
}

/* 0x8131E184 */
static int npImpsCtlClientMgrPrepareSignOut(NpImpsCtlClientMgr *mgr)
{
	int lockRet = sceKernelLockLwMutex(mgr->mutex, 1, NULL);
	if (lockRet < 0)
		return lockRet;

	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
		if (slot->allocKind != NP_IMPS_CLIENT_FREE && slot->processId != -1)
			npImpsCtlClientCancelSlotLocked(mgr, slot);
	}
	for (int slotIdx = 0; slotIdx < NP_IMPS_CTL_MAX_CLIENTS; ++slotIdx) {
		NpImpsCtlClientSlot *slot = &mgr->slots[slotIdx];
		if (slot->allocKind != NP_IMPS_CLIENT_FREE && slot->processId != -1 &&
		    slot->canceled == 0 &&
		    (slot->slotState == NP_IMPS_CSLOT_DONE ||
		     slot->slotState == NP_IMPS_CSLOT_ACTIVE)) {
			slot->requestedLevel = NP_IMPS_MODE_NONE;
		}
	}
	mgr->autoSyncLevel = NP_IMPS_MODE_DISCONNECT;
	mgr->dirty = 1;

	sceKernelUnlockLwMutex(mgr->mutex, 1);
	return 0;
}

/* ============================================================================
 * Context-level session, presence, and message helpers (0x8131C5AC..0x8131CDAC)
 * ============================================================================ */

static void npImpsCtlSendMsgAdapter(const NpImpsSessionIdentity *session, int txId, int result,
				    void *userArg)
{
	NpImpsCoreSendMsgCallback userCb = reinterpret_cast<NpImpsCoreSendMsgCallback>(userArg);
	if (userCb != NULL)
		userCb(session, txId, result, NULL);
}

static int npImpsCtlGetAutoIntervalSec(NpImpsCtlContext *ctx, int *outIntervalSec)
{
	int ret = npImpsCoreGetSessionAutoInterval(ctx->sessionIdentity.sessionId, outIntervalSec);
	return (ret >= 0) ? 0 : ret;
}

static int npImpsCtlBuildCachedPresenceStanza(NpImpsCtlContext *ctx, NpRtPresenceStanza **outStanza)
{
	if (ctx->cachedPresence != NULL)
		return npRtImpsBuildPresenceStanza(ctx->cachedPresence, outStanza);

	NpImpsCorePresenceUpdate defaultPresence;
	sceClibMemset(&defaultPresence, 0, sizeof(defaultPresence));
	defaultPresence.onlineStatus = 0;
	return npRtImpsBuildPresenceStanza(&defaultPresence, outStanza);
}

/* 0x8131C66A */
static int npImpsCtlStartCoreSession(NpImpsCtlContext *ctx, int targetMode)
{
	int sessionId = 0;
	NpRtPresenceStanza *stanza = NULL;
	ctx->sessionConnected = 0;
	ctx->sessionError = 0;

	int ret = npImpsCoreCreateSession(&sessionId);
	if (ret >= 0 && targetMode == NP_IMPS_MODE_ONLINE)
		ret = npImpsCtlBuildCachedPresenceStanza(ctx, &stanza);
	if (ret >= 0)
		ret = npImpsCoreStartSession(sessionId, targetMode, stanza);

	if (stanza != NULL)
		npRtReleasePresenceStanza(stanza);
	if (ret < 0) {
		if (sessionId != 0)
			npImpsCoreDestroySession(sessionId, NP_IMPS_CORE_ERR_INTERNAL);
		return ret;
	}

	ctx->sessionIdentity.sessionId = sessionId;
	return 0;
}

/* 0x8131C720 */
static int npImpsCtlHandleCoreSessionEvent(NpImpsCtlContext *ctx, const NpImpsCtlMsg *msg,
					   SceUInt8 *outSuspended)
{
	*outSuspended = 0;
	int eventState = msg->body.coreEvent.eventOrTxId;
	int eventError = msg->body.coreEvent.errorOrResult;

	if (eventState == NP_IMPS_EVT_STOPPED) {
		int err = (eventError < 0) ? eventError : NP_IMPS_CTL_ERR_SESSION_FAILED;
		ctx->sessionConnected = 0;
		ctx->sessionError = err;
		return err;
	}
	if (eventState == NP_IMPS_EVT_SUSPENDED) {
		int err = (eventError < 0) ? eventError : NP_IMPS_CTL_ERR_SESSION_FAILED;
		ctx->sessionConnected = 0;
		ctx->sessionError = err;
		*outSuspended = 1;
		return 0;
	}
	if (eventState == NP_IMPS_EVT_CONNECTING) {
		ctx->sessionIdentity = msg->body.coreEvent.session;
		ctx->sessionConnected = 1;
	}
	return 0;
}

/* 0x8131C7AE */
static int npImpsCtlHandleSessionStopEvent(NpImpsCtlContext *ctx, const NpImpsCtlMsg *msg,
					   int *outStopped)
{
	if (msg->body.coreEvent.eventOrTxId == NP_IMPS_EVT_STOPPED) {
		*outStopped = 1;
		npImpsCoreDestroySession(ctx->sessionIdentity.sessionId, NP_IMPS_CORE_ERR_INTERNAL);
		sceClibMemset(&ctx->sessionIdentity, 0, sizeof(ctx->sessionIdentity));
		ctx->sessionIdentity.sessionId = 0;
	}
	return 0;
}

/* 0x8131C7EE */
static void npImpsCtlReplaceCachedPresence(NpImpsCtlContext *ctx,
					   NpImpsCorePresenceUpdate *newPresence)
{
	if (ctx->cachedPresence != NULL) {
		npRtImpsFreePresenceUpdate(ctx->cachedPresence, NULL);
		ctx->cachedPresence = NULL;
	}
	if (newPresence != NULL)
		ctx->cachedPresence = newPresence;
}

/* 0x8131C80E */
static int npImpsCtlClearPresenceAndSignOut(NpImpsCtlContext *ctx)
{
	npImpsCtlReplaceCachedPresence(ctx, NULL);
	return npImpsCtlClientMgrPrepareSignOut(&ctx->clientMgr);
}

/* 0x8131C836 */
static int npImpsCtlApplySessionMode(NpImpsCtlContext *ctx, int targetMode)
{
	NpRtPresenceStanza *stanza = NULL;
	int ret = 0;
	if (targetMode == NP_IMPS_MODE_ONLINE)
		ret = npImpsCtlBuildCachedPresenceStanza(ctx, &stanza);
	if (ret >= 0)
		ret = npImpsCoreSetSessionMode(ctx->sessionIdentity.sessionId, targetMode, stanza);
	if (stanza != NULL)
		npRtReleasePresenceStanza(stanza);
	return ret;
}

/* 0x8131C8C2 */
static int npImpsCtlSendCurrentPresence(NpImpsCtlContext *ctx)
{
	NpRtPresenceStanza *stanza = NULL;
	int txId = 0;
	int ret = npImpsCtlBuildCachedPresenceStanza(ctx, &stanza);
	if (ret >= 0)
		ret = npImpsCoreAllocTransactionId(&txId);
	if (ret >= 0)
		ret = npImpsCoreSendPresence(ctx->sessionIdentity.sessionId, txId, stanza,
					     NULL, NULL);
	if (stanza != NULL)
		npRtReleasePresenceStanza(stanza);
	return ret;
}

/* 0x8131C98A */
static int npImpsCtlTearDownCoreSession(NpImpsCtlContext *ctx, int errorCode)
{
	int ret = npImpsCtlTxFailInFlight(&ctx->txQueue, errorCode);
	if (ret < 0)
		return ret;
	if (ctx->sessionIdentity.sessionId != 0)
		npImpsCoreDestroySession(ctx->sessionIdentity.sessionId, errorCode);
	sceClibMemset(&ctx->sessionIdentity, 0, sizeof(ctx->sessionIdentity));
	ctx->sessionIdentity.sessionId = 0;
	return 0;
}

/* 0x8131C9CA */
static int npImpsCtlFlushCompletedTx(NpImpsCtlContext *ctx)
{
	while (true) {
		NpImpsTxNodeHeader *hdr = NULL;
		int ret = npImpsTxQueuePopCompleted(&ctx->txQueue, &hdr);
		if (ret < 0)
			return ret;
		if (hdr == NULL)
			return 0;
		NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(hdr);
		if (node->callback != NULL) {
			node->callback(&ctx->sessionIdentity, node->header.requestId,
				       node->resultCode, node->userArg);
		}
		npImpsCtlTxFree(&ctx->txQueue, node);
	}
}

/* 0x8131C2EE */
static int npImpsCtlFlushAllDone(NpImpsCtlContext *ctx)
{
	int ret = npImpsCtlClientMgrFlushDone(&ctx->clientMgr, &ctx->contextId);
	if (ret >= 0)
		ret = npImpsCtlFlushCompletedTx(ctx);
	return (ret >= 0) ? 0 : ret;
}

static int npImpsCtxFindBySessionId(int sessionId, NpImpsCtlContext **outCtx, SceUInt8 allowClosing);
static int npImpsCtxRelease(NpImpsCtlContext *ctx);
static int npImpsCtxClose(NpImpsCtlContext *ctx);

/* 0x8131CA26 */
static int npImpsCtlInitSessionTx(NpImpsCtlContext *ctx)
{
	sceClibMemset(&ctx->sessionIdentity, 0, sizeof(ctx->sessionIdentity));
	npImpsTxQueueInit(&ctx->txQueue, &ctx->mutex);
	ctx->sessionIdentity.sessionId = 0;
	return 0;
}

/* 0x8131CA4A */
static int npImpsCtlTermSessionTx(NpImpsCtlContext *ctx)
{
	if (ctx->sessionIdentity.sessionId != 0)
		npImpsCoreDestroySession(ctx->sessionIdentity.sessionId, NP_IMPS_CORE_ERR_INTERNAL);
	npImpsCtlTxClearAll(&ctx->txQueue);
	return 0;
}

/* 0x8131C95C */
static int npImpsCtlCreateMessageTx(NpImpsCtlContext *ctx, int requestId,
				    NpImpsSessionSendDoneCallback callback, void *userArg,
				    NpImpsCtlTxNode **outNode)
{
	return npImpsCtlTxCreate(&ctx->txQueue, requestId, callback, userArg, outNode);
}

/* 0x8131C970 */
static int npImpsCtlRemoveMessageTx(NpImpsCtlContext *ctx, NpImpsCtlTxNode *node)
{
	return npImpsCtlTxRemoveById(&ctx->txQueue, node->header.requestId);
}

/* 0x8131C9BE */
static int npImpsCtlFailPendingMessageTx(NpImpsCtlContext *ctx, int requestId, int errorCode)
{
	return npImpsCtlTxFailPendingById(&ctx->txQueue, requestId, errorCode);
}

/* 0x8131CB14 */
static void npImpsCtlOnCoreSendMsgDone(const NpImpsSessionIdentity *session, int txId, int result,
				       void *userArg)
{
	(void)userArg;
	NpImpsCtlContext *ctx = NULL;
	if (npImpsCtxFindBySessionId(session->sessionId, &ctx, 1) >= 0) {
		NpImpsCtlMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_CTL_MSG_TX_DONE;
		msg.body.coreEvent.session = *session;
		msg.body.coreEvent.eventOrTxId = txId;
		msg.body.coreEvent.errorOrResult = result;
		npImpsCtlPipeSend(&ctx->msgPipeId, &msg, NULL);
	}
	if (ctx != NULL)
		npImpsCtxRelease(ctx);
}

/* 0x8131CA6A */
static int npImpsCtlDispatchQueuedMessage(NpImpsCtlContext *ctx, int requestId)
{
	NpImpsTxNodeHeader *hdr = NULL;
	NpRtMessageStanza *stanza = NULL;

	int ret = npImpsTxQueueFindPending(&ctx->txQueue, requestId, &hdr);
	if (ret >= 0 && hdr != NULL) {
		NpImpsCtlTxNode *node = reinterpret_cast<NpImpsCtlTxNode *>(hdr);
		ret = npRtImpsBuildMessageStanza(&node->targetNpId, node->message, &stanza);
		if (ret >= 0 && node->txKind == 0) {
			ret = npImpsCoreSendMessage(ctx->sessionIdentity.sessionId, requestId,
						    stanza, npImpsCtlOnCoreSendMsgDone, NULL);
			if (ret >= 0)
				ret = npImpsCtlTxMarkInFlight(&ctx->txQueue, node, requestId);
		}
	}
	if (ret < 0 && hdr != NULL)
		npImpsCtlTxFailPendingById(&ctx->txQueue, hdr->requestId, ret);
	if (stanza != NULL)
		npRtReleaseMessageStanza(stanza);
	return 0;
}

/* 0x8131CBD0 */
static int npImpsCtlRejectUnexpectedMsg(NpImpsCtlContext *ctx, const NpImpsCtlMsg *msg,
					int errorCode)
{
	if (msg->kind == NP_IMPS_CTL_MSG_CLIENT_UPDATE)
		npImpsCtlClientMgrFailByReqId(&ctx->clientMgr, msg->body.requestId, errorCode);
	else if (msg->kind == NP_IMPS_CTL_MSG_SEND_MESSAGE)
		npImpsCtlFailPendingMessageTx(ctx, msg->body.requestId, errorCode);
	return 0;
}

/* 0x8131CD20 */
static int npImpsCtlDrainPendingMsgs(NpImpsCtlContext *ctx, int errorCode)
{
	NpImpsCtlMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_CTL_MSG_INVALID;

	int ret = 0;
	while (true) {
		ret = npImpsCtlPipeTryRecv(&ctx->msgPipeId, &msg);
		if (ret < 0)
			break;
		if (msg.kind == NP_IMPS_CTL_MSG_INVALID) {
			ret = 0;
			break;
		}
		ret = npImpsCtlRejectUnexpectedMsg(ctx, &msg, errorCode);
		if (ret < 0)
			break;
		npImpsCtlMsgCleanup(&msg);
	}
	npImpsCtlMsgCleanup(&msg);
	return ret;
}

/* ============================================================================
 * Public 7-callback listener registry & Core bridge (0x8131E318..0x8131E7AF)
 * ============================================================================ */

static void npImpsCtlNotifyCtlEvent(int eventType)
{
	for (NpImpsCtlListenerNode *cur = s_ctlListenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onCtlEvent != NULL)
			cur->callbacks.onCtlEvent(eventType);
	}
}

static void npImpsCtlNotifySessionState(const NpImpsSessionIdentity *session, int sessionState,
					int errorCode)
{
	for (NpImpsCtlListenerNode *cur = s_ctlListenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onSessionState != NULL)
			cur->callbacks.onSessionState(session, sessionState, errorCode);
	}
}

static void npImpsCtlNotifyRoster(const NpImpsSessionIdentity *session, int action,
				  const NpBasicRosterList *rosterList)
{
	for (NpImpsCtlListenerNode *cur = s_ctlListenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onRosterUpdate != NULL)
			cur->callbacks.onRosterUpdate(session, action, rosterList);
	}
}

static void npImpsCtlNotifyPrivacy(const NpImpsSessionIdentity *session, int action,
				   const NpBasicPrivacyList *privacyList)
{
	for (NpImpsCtlListenerNode *cur = s_ctlListenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onPrivacyUpdate != NULL)
			cur->callbacks.onPrivacyUpdate(session, action, privacyList);
	}
}

/* 0x8131E4D2 */
static void npImpsCtlNotifyMessageStanza(const NpImpsSessionIdentity *session,
					 NpRtMessageStanza *stanza)
{
	NpImpsCoreMessage *parsed = NULL;
	SceNpId senderNpId;
	if (npRtImpsParseMessageStanza(&parsed, stanza, NULL) >= 0 &&
	    SceNpCommon_9B7CAA67(&senderNpId, &stanza->fromJid) >= 0) {
		for (NpImpsCtlListenerNode *cur = s_ctlListenerHead; cur != NULL; cur = cur->next) {
			if (cur->callbacks.onMessageRecv != NULL)
				cur->callbacks.onMessageRecv(session, &senderNpId, parsed);
		}
	}
	if (parsed != NULL)
		npRtImpsFreeMessage(parsed, NULL);
}

/* 0x8131E540 */
static void npImpsCtlNotifyPresenceStanza(const NpImpsSessionIdentity *session,
					  NpRtPresenceStanza *stanza)
{
	NpImpsCorePresenceUpdate *parsed = NULL;
	SceNpId senderNpId;
	if (npRtImpsParsePresenceStanza(&parsed, stanza, NULL) >= 0 &&
	    SceNpCommon_9B7CAA67(&senderNpId, &stanza->fromJid) >= 0) {
		for (NpImpsCtlListenerNode *cur = s_ctlListenerHead; cur != NULL; cur = cur->next) {
			if (cur->callbacks.onPresenceUpdate != NULL)
				cur->callbacks.onPresenceUpdate(session, &senderNpId, parsed);
		}
	}
	if (parsed != NULL)
		npRtImpsFreePresenceUpdate(parsed, NULL);
}

static void npImpsCtlNotifyPolling(const NpImpsSessionIdentity *session, int notifyType,
				   const SceRtcTick *timestamp)
{
	for (NpImpsCtlListenerNode *cur = s_ctlListenerHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onPollingNotify != NULL)
			cur->callbacks.onPollingNotify(session, notifyType, timestamp);
	}
}

/* 0x8131C5CA */
static int npImpsCtlPostCoreSessionEvent(const NpImpsSessionIdentity *session, int eventState,
					 int errorCode)
{
	NpImpsCtlContext *ctx = NULL;
	int ret = npImpsCtxFindBySessionId(session->sessionId, &ctx, 1);
	if (ret >= 0) {
		NpImpsCtlMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_CTL_MSG_CORE_SESSION_EVENT;
		msg.body.coreEvent.session = *session;
		msg.body.coreEvent.eventOrTxId = eventState;
		msg.body.coreEvent.errorOrResult = errorCode;
		ret = npImpsCtlPipeSend(&ctx->msgPipeId, &msg, NULL);
		if (ret >= 0)
			ret = 0;
	}
	if (ctx != NULL)
		npImpsCtxRelease(ctx);
	return ret;
}

static void npImpsCtlBridgeOnSessionState(const NpImpsSessionIdentity *session, int eventState,
					  int errorCode)
{
	npImpsCtlPostCoreSessionEvent(session, eventState, errorCode);
	npImpsCtlNotifySessionState(session, eventState, errorCode);
}

static void npImpsCtlBridgeOnRoster(const NpImpsSessionIdentity *session, int isPush,
				    NpBasicRosterList *rosterList)
{
	npImpsCtlNotifyRoster(session, isPush, rosterList);
}

static void npImpsCtlBridgeOnPrivacy(const NpImpsSessionIdentity *session, int action,
				     NpBasicPrivacyList *privacyList)
{
	npImpsCtlNotifyPrivacy(session, action, privacyList);
}

/* 0x8131E796 */
static void npImpsCtlBridgeOnMessage(const NpImpsSessionIdentity *session,
				     NpRtMessageStanza *stanza)
{
	npImpsCtlNotifyMessageStanza(session, stanza);
}

/* 0x8131E79E */
static void npImpsCtlBridgeOnPresence(const NpImpsSessionIdentity *session,
				      NpRtPresenceStanza *stanza)
{
	npImpsCtlNotifyPresenceStanza(session, stanza);
}

/* 0x8131E7A6 */
static void npImpsCtlBridgeOnPolling(const NpImpsSessionIdentity *session, int updateKind,
				     const SceRtcTick *timestamp)
{
	npImpsCtlNotifyPolling(session, updateKind, timestamp);
}

/* 0x8131E6A8 / 0x8131E744 */
static int npImpsCtlRegisterCoreBridge(void)
{
	if (s_coreSessionCbId != 0)
		return NP_IMPS_CORE_ERR_ALREADY_INIT;

	NpImpsCoreSessionCallbacks cb;
	sceClibMemset(&cb, 0, sizeof(cb));
	cb.onSessionState = npImpsCtlBridgeOnSessionState;
	cb.onRosterUpdate = npImpsCtlBridgeOnRoster;
	cb.onPrivacyUpdate = npImpsCtlBridgeOnPrivacy;
	cb.onMessageStanza = npImpsCtlBridgeOnMessage;
	cb.onPresenceStanza = npImpsCtlBridgeOnPresence;
	cb.onPollingUpdate = npImpsCtlBridgeOnPolling;

	int cbId = 0;
	int ret = npImpsCoreRegisterSessionCallbacks(&cb, &cbId);
	if (ret >= 0) {
		s_coreSessionCbId = cbId;
		return 0;
	}
	return ret;
}

static int npImpsCtlUnregisterCoreBridge(void)
{
	if (s_coreSessionCbId == 0)
		return 0;
	int ret = npImpsCoreUnregisterSessionCallbacks(s_coreSessionCbId);
	if (ret >= 0) {
		s_coreSessionCbId = 0;
		return 0;
	}
	return ret;
}

/* 0x8131E318 */
static int npImpsCtlListenerAlloc(const NpImpsCoreCallbacks *callbacks,
				  NpImpsCtlListenerNode **outNode)
{
	if (s_ctlListenerCount >= NP_IMPS_CTL_MAX_LISTENERS)
		return NP_IMPS_CORE_ERR_CB_FULL;

	NpImpsCtlListenerNode *node = reinterpret_cast<NpImpsCtlListenerNode *>(
		npRtHeapAlloc(sizeof(NpImpsCtlListenerNode), NULL));
	if (node == NULL)
		return NP_IMPS_CORE_ERR_NO_MEMORY;

	sceClibMemset(node, 0, sizeof(*node));
	sceClibMemcpy_safe(&node->callbacks, callbacks, sizeof(node->callbacks));
	s_ctlListenerCount++;

	s_ctlNextListenerId++;
	if (s_ctlNextListenerId == 0)
		s_ctlNextListenerId++;
	node->listenerId = s_ctlNextListenerId;

	node->next = NULL;
	node->prev = s_ctlListenerTail;
	if (s_ctlListenerTail != NULL)
		s_ctlListenerTail->next = node;
	if (s_ctlListenerHead == NULL)
		s_ctlListenerHead = node;
	s_ctlListenerTail = node;

	*outNode = node;
	return 0;
}

/* 0x8131E5DA */
int npImpsCoreRegisterCallbacks(const NpImpsCoreCallbacks *callbacks, int *outCallbackId)
{
	if (npImpsCtlIsStarted() != 0)
		return NP_IMPS_CTL_ERR_ALREADY_STARTED;

	NpImpsCtlListenerNode *node = NULL;
	int ret = npImpsCtlListenerAlloc(callbacks, &node);
	if (ret >= 0) {
		*outCallbackId = node->listenerId;
		return 0;
	}
	return ret;
}

/* 0x8131E626 */
int npImpsCoreUnregisterCallbacks(int callbackId)
{
	if (npImpsCtlIsStarted() != 0)
		return NP_IMPS_CTL_ERR_ALREADY_STARTED;

	NpImpsCtlListenerNode *found = NULL;
	for (NpImpsCtlListenerNode *cur = s_ctlListenerHead; cur != NULL; cur = cur->next) {
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
		s_ctlListenerTail = found->prev;

	if (found->prev != NULL)
		found->prev->next = found->next;
	else
		s_ctlListenerHead = found->next;

	found->next = NULL;
	found->prev = NULL;
	npRtHeapFree(found, NULL);
	s_ctlListenerCount--;
	return 0;
}

/* ============================================================================
 * Context Lifecycle & Worker Thread (0x8131B8E0..0x8131BEC7, 0x8131E7B0..0x8131E823)
 * ============================================================================ */

static void npImpsCtxFree(NpImpsCtlContext *ctx)
{
	if (ctx->accountEventCbId != -1)
		SceShell_B4D888D5(ctx->accountEventCbId);
	if (ctx->cronHandle != 0) {
		ShellCron *cron = shellCronGetInstance();
		if (cron != NULL)
			cron->Remove(ctx->cronHandle);
	}
	npImpsCtlClientMgrTerm(&ctx->clientMgr);

	int lockState = npImpsCtxLock(ctx);
	if (lockState >= 0) {
		ctx->aborted = 1;
		npImpsCtlSendWakeup(&ctx->msgPipeId);
	}
	npImpsCtxUnlock(ctx, lockState);

	if (ctx->workerThreadPtr != NULL) {
		npRtWaitThreadEnd(ctx->workerThreadPtr);
		ctx->workerThreadPtr = NULL;
	}

	npImpsCtlTermSessionTx(ctx);
	npRtCalloutCancel(&ctx->timeoutTimer);
	npImpsCtlPipeDestroy(&ctx->msgPipeId);
	npRtImpsFreePresenceUpdate(ctx->cachedPresence, NULL);
	sceKernelDeleteLwMutex(&ctx->mutex);
	npRtHeapFree(ctx, NULL);
}

/* 0x8131B974 */
static int npImpsCtxCreate(NpImpsCtlContext **outCtx)
{
	if (s_ctxMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	NpImpsCtlContext *ctx = reinterpret_cast<NpImpsCtlContext *>(
		npRtHeapAlloc(sizeof(NpImpsCtlContext), NULL));
	if (ctx == NULL)
		return NP_IMPS_CORE_ERR_NO_MEMORY;

	sceClibMemset(ctx, 0, sizeof(*ctx));
	int ret = sceKernelCreateLwMutex(&ctx->mutex, "SceNpImpsContext", 0x2002u, 0, NULL);
	if (ret < 0) {
		npRtHeapFree(ctx, NULL);
		return ret;
	}

	ctx->state = NP_IMPS_CTL_STATE_SIGNED_OUT;
	ret = npImpsCtlPipeCreate(&ctx->msgPipeId);
	if (ret < 0) {
		sceKernelDeleteLwMutex(&ctx->mutex);
		npRtHeapFree(ctx, NULL);
		return ret;
	}

	npRtCalloutInit(&ctx->timeoutTimer);
	npImpsCtlInitSessionTx(ctx);

	s_nextCtxId++;
	if (s_nextCtxId == 0)
		s_nextCtxId++;
	ctx->contextId = s_nextCtxId;
	ctx->sessionConnected = 0;
	npImpsCtlClientMgrInit(&ctx->clientMgr, &ctx->mutex);
	ctx->cronHandle = 0;
	ctx->accountEventCbId = -1;

	ctx->refCount++;

	int globalLock = npImpsCtlMutexLock();
	if (globalLock < 0) {
		npImpsCtxFree(ctx);
		return globalLock;
	}

	s_ctxCount++;
	int ctxLock = npImpsCtxLock(ctx);
	if (ctxLock < 0) {
		npImpsCtxFree(ctx);
		npImpsCtlMutexUnlock(globalLock);
		return ctxLock;
	}

	ctx->next = NULL;
	ctx->prev = s_ctxListTail;
	if (s_ctxListTail != NULL)
		s_ctxListTail->next = ctx;
	if (s_ctxListHead == NULL)
		s_ctxListHead = ctx;
	s_ctxListTail = ctx;
	ctx->flags |= NP_IMPS_CTX_FLAG_LINKED;

	npImpsCtxUnlock(ctx, ctxLock);
	npImpsCtlMutexUnlock(globalLock);
	*outCtx = ctx;
	return 0;
}

/* 0x8131BB28 / 0x8131BBCC */
static int npImpsCtxFindInternal(bool bySessionId, int matchId, NpImpsCtlContext **outCtx,
				 SceUInt8 allowClosing)
{
	if (s_ctxMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	int globalLock = npImpsCtlMutexLock();
	if (globalLock < 0)
		return globalLock;

	NpImpsCtlContext *found = NULL;
	for (NpImpsCtlContext *cur = s_ctxListHead; cur != NULL; cur = cur->next) {
		int curId = bySessionId ? cur->sessionIdentity.sessionId : cur->contextId;
		if (curId == matchId) {
			found = cur;
			break;
		}
	}

	int ret = 0;
	if (found != NULL) {
		int ctxLock = npImpsCtxLock(found);
		if (ctxLock >= 0) {
			if (allowClosing != 0 || (found->flags & NP_IMPS_CTX_FLAG_CLOSING) == 0) {
				found->refCount++;
				*outCtx = found;
			} else {
				ret = NP_IMPS_CTL_ERR_CTX_NOT_FOUND;
			}
			npImpsCtxUnlock(found, ctxLock);
		} else {
			ret = ctxLock;
		}
	} else {
		ret = NP_IMPS_CTL_ERR_CTX_NOT_FOUND;
	}

	npImpsCtlMutexUnlock(globalLock);
	return ret;
}

static int npImpsCtxFindById(int contextId, NpImpsCtlContext **outCtx, SceUInt8 allowClosing)
{
	return npImpsCtxFindInternal(false, contextId, outCtx, allowClosing);
}

static int npImpsCtxFindBySessionId(int sessionId, NpImpsCtlContext **outCtx, SceUInt8 allowClosing)
{
	return npImpsCtxFindInternal(true, sessionId, outCtx, allowClosing);
}

/* 0x8131BC72 */
static int npImpsCtxRelease(NpImpsCtlContext *ctx)
{
	if (s_ctxMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	int globalLock = npImpsCtlMutexLock();
	if (globalLock < 0)
		return globalLock;

	NpImpsCtlContext *toFree = NULL;
	int ret = 0;
	int ctxLock = npImpsCtxLock(ctx);
	if (ctxLock >= 0) {
		ctx->refCount--;
		if (ctx->refCount == 0 && (ctx->flags & NP_IMPS_CTX_FLAG_CLOSING) != 0) {
			if ((ctx->flags & NP_IMPS_CTX_FLAG_LINKED) != 0) {
				if (ctx->next != NULL)
					ctx->next->prev = ctx->prev;
				else
					s_ctxListTail = ctx->prev;
				if (ctx->prev != NULL)
					ctx->prev->next = ctx->next;
				else
					s_ctxListHead = ctx->next;
				ctx->next = NULL;
				ctx->prev = NULL;
				ctx->flags &= ~NP_IMPS_CTX_FLAG_LINKED;
			}
			toFree = ctx;
		}
		npImpsCtxUnlock(ctx, ctxLock);
	} else {
		ret = ctxLock;
	}

	npImpsCtlMutexUnlock(globalLock);
	if (toFree != NULL)
		npImpsCtxFree(toFree);
	return ret;
}

/* 0x8131BD36 */
static int npImpsCtxClose(NpImpsCtlContext *ctx)
{
	if (s_ctxMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	int globalLock = npImpsCtlMutexLock();
	if (globalLock < 0)
		return globalLock;

	NpImpsCtlContext *toFree = NULL;
	int ret = 0;
	int ctxLock = npImpsCtxLock(ctx);
	if (ctxLock >= 0) {
		ctx->flags |= NP_IMPS_CTX_FLAG_CLOSING;
		ctx->refCount--;
		if (ctx->refCount == 0 && (ctx->flags & NP_IMPS_CTX_FLAG_CLOSING) != 0) {
			if ((ctx->flags & NP_IMPS_CTX_FLAG_LINKED) != 0) {
				if (ctx->next != NULL)
					ctx->next->prev = ctx->prev;
				else
					s_ctxListTail = ctx->prev;
				if (ctx->prev != NULL)
					ctx->prev->next = ctx->next;
				else
					s_ctxListHead = ctx->next;
				ctx->next = NULL;
				ctx->prev = NULL;
				ctx->flags &= ~NP_IMPS_CTX_FLAG_LINKED;
			}
			toFree = ctx;
		}
		npImpsCtxUnlock(ctx, ctxLock);
	} else {
		ret = ctxLock;
	}

	npImpsCtlMutexUnlock(globalLock);
	if (toFree != NULL)
		npImpsCtxFree(toFree);
	return ret;
}

static int npImpsCtxStartWorker(NpImpsCtlContext *ctx)
{
	ctx->aborted = 0;
	int ret = npRtStartThread(&s_ctlThreadId, ctx);
	if (ret >= 0) {
		ctx->workerThreadPtr = &s_ctlThreadId;
		return 0;
	}
	return ret;
}

static bool npImpsCtxIsAborted(NpImpsCtlContext *ctx)
{
	bool aborted = false;
	int lockState = npImpsCtxLock(ctx);
	if (lockState >= 0)
		aborted = (ctx->aborted != 0);
	npImpsCtxUnlock(ctx, lockState);
	return aborted;
}

static int npImpsCtlThreadEntry(SceSize args, void *argp)
{
	(void)args;
	NpImpsCtlContext *ctx = *reinterpret_cast<NpImpsCtlContext **>(argp);
	SceUID tid = sceKernelGetThreadId();
	if (SceNetInternal_9DBD2F60(tid, 1, 0) >= 0)
		npImpsCtlMainLoop(ctx);
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

/* 0x8131E7B0 */
static int npImpsCtlCreateThread(SceUID *outThreadId, const NpPollingThreadParam *threadParam)
{
	SceUID tid = sceKernelCreateThread("SceNpImpsCtl", npImpsCtlThreadEntry,
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

static int npImpsCtxMgrInit(const NpPollingThreadParam *threadParam)
{
	if (s_ctxMgrInit != 0)
		return NP_IMPS_CORE_ERR_ALREADY_INIT;

	s_ctxMgrInit = 1;
	s_ctxListHead = NULL;
	s_ctxListTail = NULL;
	s_ctxCount = 0;
	s_nextCtxId = 0;

	int ret = npImpsCtlCreateThread(&s_ctlThreadId, threadParam);
	return (ret >= 0) ? 0 : ret;
}

static int npImpsCtxMgrTerm(void)
{
	if (s_ctxMgrInit == 0)
		return NP_IMPS_CORE_ERR_NOT_INITIALIZED;

	while (s_ctxListHead != NULL)
		npImpsCtxClose(s_ctxListHead);

	npRtJoinAndDeleteThread(&s_ctlThreadId);
	s_ctxCount = 0;
	s_ctxMgrInit = 0;
	return 0;
}

/* ============================================================================
 * Ctl 16-state machine (0x8131E840..0x8131F389)
 * ============================================================================ */

/* 0x8131EB52 */
static void npImpsCtlOnAccountEvent(int eventId, int stateGroup, int stateCode,
				    int detailA, int detailB, void *userArg)
{
	(void)detailA;
	(void)detailB;
	if (stateGroup == 4 && (stateCode == 6 || eventId == 5)) {
		NpImpsCtlContext *ctx = reinterpret_cast<NpImpsCtlContext *>(userArg);
		NpImpsCtlMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_CTL_MSG_ACCOUNT_SYNC;
		SceUInt32 allowedMask = (1u << NP_IMPS_CTL_STATE_WAIT_CRON_WAKE);
		npImpsCtlPostCommand(ctx, &allowedMask, &msg, 0);
	}
}

/* 0x8131EFFA */
static int npImpsCtlOnCronWakeup(SceUID handle, void *userArg)
{
	(void)handle;
	NpImpsCtlContext *ctx = reinterpret_cast<NpImpsCtlContext *>(userArg);
	NpImpsCtlMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_CTL_MSG_CRON_FIRED;
	SceUInt32 allowedMask = (1u << NP_IMPS_CTL_STATE_WAIT_CRON_WAKE);
	npImpsCtlPostCommand(ctx, &allowedMask, &msg, 0);
	ctx->cronHandle = 0;
	return 0;
}

/* 0x8131EBA2 */
static int npImpsCtlTryStartSession(NpImpsCtlContext *ctx, SceUInt8 *outStarted)
{
	if (outStarted != NULL)
		*outStarted = 0;
	int targetLevel = NP_IMPS_MODE_NONE;
	int ret = npImpsCtlClientMgrResolveTarget(&ctx->clientMgr, 0, &targetLevel);
	if (ret < 0)
		return ret;
	if (targetLevel > NP_IMPS_MODE_DISCONNECT) {
		ret = npImpsCtlStartCoreSession(ctx, targetLevel);
		if (ret < 0)
			return ret;
		if (outStarted != NULL)
			*outStarted = 1;
	}
	return 0;
}

/* 0x8131EC02 */
static int npImpsCtlBeginDisconnect(NpImpsCtlContext *ctx)
{
	int ret = npImpsCtlClearPresenceAndSignOut(ctx);
	if (ret < 0)
		return ret;
	int targetLevel = NP_IMPS_MODE_NONE;
	ret = npImpsCtlClientMgrResolveTarget(&ctx->clientMgr, 0, &targetLevel);
	if (ret >= 0 && targetLevel != NP_IMPS_MODE_NONE)
		ret = npImpsCtlApplySessionMode(ctx, targetLevel);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131EC5A */
static int npImpsCtlHandleDisconnectingSessionEvent(NpImpsCtlContext *ctx, const NpImpsCtlMsg *msg,
						    int *outDone)
{
	*outDone = 0;
	if (ctx->clientMgr.desiredLevel == NP_IMPS_MODE_DISCONNECT)
		return npImpsCtlHandleSessionStopEvent(ctx, msg, outDone);

	SceUInt8 suspended = 0;
	int ret = npImpsCtlHandleCoreSessionEvent(ctx, msg, &suspended);
	if (ret < 0)
		return ret;

	ret = npImpsCtlClientMgrOnSessionState(&ctx->clientMgr, msg->body.coreEvent.eventOrTxId,
					       msg->body.coreEvent.errorOrResult);
	if (ret < 0)
		return ret;

	if (ctx->sessionConnected == 1)
		return 0;

	npImpsCtlClientMgrResetWaiting(&ctx->clientMgr, ctx->sessionError);
	int targetLevel = NP_IMPS_MODE_NONE;
	ret = npImpsCtlClientMgrResolveTarget(&ctx->clientMgr, 0, &targetLevel);
	if (ret < 0)
		return ret;
	if (targetLevel == NP_IMPS_MODE_NONE)
		return NP_IMPS_CORE_ERR_BAD_STATE;
	return npImpsCtlApplySessionMode(ctx, targetLevel);
}

/* State 0 (SIGNED_OUT): 0x8131ED0C */
static int npImpsCtlStateSignedOut(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	if (msg->kind == NP_IMPS_CTL_MSG_CLOSE)
		*nextState = NP_IMPS_CTL_STATE_CLOSED;
	else if (msg->kind == NP_IMPS_CTL_MSG_SIGN_IN)
		*nextState = NP_IMPS_CTL_STATE_SIGNING_IN;
	else
		npImpsCtlRejectUnexpectedMsg(ctx, msg, NP_IMPS_CTL_ERR_BAD_STATE);

	return npImpsCtlFlushAllDone(ctx);
}

/* State 1 (SIGNING_IN): 0x8131ED4A */
static int npImpsCtlStateSigningIn(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	(void)msg;
	int cbId = -1;
	int targetLevel = NP_IMPS_MODE_NONE;
	if (SceShell_2E0B6FA6(npImpsCtlOnAccountEvent, ctx, &cbId) >= 0) {
		ctx->accountEventCbId = cbId;
		if (npImpsCtlClientMgrSetAutoSyncLevel(&ctx->clientMgr, NP_IMPS_MODE_CRON_POLL) >= 0 &&
		    npImpsCtlClientMgrResolveTarget(&ctx->clientMgr, 1, &targetLevel) >= 0 &&
		    targetLevel != NP_IMPS_MODE_NONE &&
		    npImpsCtlStartCoreSession(ctx, targetLevel) >= 0) {
			*nextState = NP_IMPS_CTL_STATE_SIGN_IN_OK;
			return 0;
		}
	}
	*nextState = NP_IMPS_CTL_STATE_SIGN_IN_FAIL;
	return 0;
}

/* State 2 (SIGN_IN_OK): 0x8131EDD4 */
static int npImpsCtlStateSignInOk(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	(void)ctx;
	(void)msg;
	*nextState = NP_IMPS_CTL_STATE_ACTIVE;
	return 0;
}

/* State 3 (SIGN_IN_FAIL): 0x8131EDDC */
static int npImpsCtlStateSignInFail(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	(void)ctx;
	(void)msg;
	*nextState = NP_IMPS_CTL_STATE_RECOVERY;
	return 0;
}

/* States 4 (ACTIVE) & 6 (WAIT_CRON_WAKE): 0x8131EDE4 / 0x8131EFE8 / 0x8131F140 */
static int npImpsCtlStateActiveCommon(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState,
				      bool isCronWakeState)
{
	int ret = 0;
	switch (msg->kind) {
	case NP_IMPS_CTL_MSG_CLOSE:
		ret = npImpsCtlBeginDisconnect(ctx);
		if (ret < 0)
			return ret;
		*nextState = NP_IMPS_CTL_STATE_CLOSING;
		break;

	case NP_IMPS_CTL_MSG_SIGN_OUT:
		if (npImpsCtlBeginDisconnect(ctx) >= 0)
			*nextState = NP_IMPS_CTL_STATE_SIGNING_OUT;
		else
			*nextState = NP_IMPS_CTL_STATE_SIGN_OUT_CLEAN;
		break;

	case NP_IMPS_CTL_MSG_CLIENT_UPDATE:
		if (msg->body.requestId != 0) {
			NpImpsCtlClientResult info;
			ret = npImpsCtlClientMgrPromoteQueued(&ctx->clientMgr, msg->body.requestId,
							      &info);
			if (ret < 0)
				return ret;
		}
		break;

	case NP_IMPS_CTL_MSG_PRESENCE_UPDATE:
		npImpsCtlReplaceCachedPresence(ctx, msg->body.presence);
		msg->body.presence = NULL;
		ret = npImpsCtlSendCurrentPresence(ctx);
		if (ret < 0)
			return ret;
		break;

	case NP_IMPS_CTL_MSG_SEND_MESSAGE:
		ret = npImpsCtlDispatchQueuedMessage(ctx, msg->body.requestId);
		if (ret < 0)
			return ret;
		break;

	case NP_IMPS_CTL_MSG_CRON_FIRED:
	case NP_IMPS_CTL_MSG_ACCOUNT_SYNC:
		if (isCronWakeState) {
			ret = npImpsCtlClientMgrSetAutoSyncLevel(&ctx->clientMgr,
								 NP_IMPS_MODE_CRON_POLL);
			if (ret < 0)
				return ret;
		} else {
			npImpsCtlRejectUnexpectedMsg(ctx, msg, NP_IMPS_CTL_ERR_BAD_STATE);
		}
		break;

	case NP_IMPS_CTL_MSG_AUTO_SYNC_RESET:
		ret = npImpsCtlClientMgrSetAutoSyncLevel(&ctx->clientMgr, NP_IMPS_MODE_CRON_POLL);
		if (ret < 0)
			return ret;
		break;

	case NP_IMPS_CTL_MSG_CORE_SESSION_EVENT: {
		SceUInt8 suspended = 0;
		ret = npImpsCtlHandleCoreSessionEvent(ctx, msg, &suspended);
		if (ret < 0)
			return ret;
		ret = npImpsCtlClientMgrOnSessionState(&ctx->clientMgr,
						       msg->body.coreEvent.eventOrTxId,
						       msg->body.coreEvent.errorOrResult);
		if (ret < 0)
			return ret;
		if (ctx->sessionConnected != 1) {
			npImpsCtlClientMgrResetWaiting(&ctx->clientMgr, ctx->sessionError);
			if (suspended == 0)
				return ctx->sessionError;
			ctx->lastError = ctx->sessionError;
			*nextState = NP_IMPS_CTL_STATE_WAIT_CRON_SETUP;
		}
		break;
	}

	case NP_IMPS_CTL_MSG_TX_DONE:
		ret = npImpsCtlTxCompleteByCoreId(&ctx->txQueue, msg->body.coreEvent.eventOrTxId,
						  msg->body.coreEvent.errorOrResult);
		if (ret < 0)
			return ret;
		break;

	default:
		npImpsCtlRejectUnexpectedMsg(ctx, msg, NP_IMPS_CTL_ERR_BAD_STATE);
		break;
	}

	if (*nextState == NP_IMPS_CTL_STATE_ACTIVE ||
	    *nextState == NP_IMPS_CTL_STATE_WAIT_CRON_WAKE) {
		int targetLevel = NP_IMPS_MODE_NONE;
		ret = npImpsCtlClientMgrResolveTarget(&ctx->clientMgr, 0, &targetLevel);
		if (ret < 0)
			return ret;
		if (targetLevel != NP_IMPS_MODE_NONE) {
			ret = npImpsCtlApplySessionMode(ctx, targetLevel);
			if (ret < 0)
				return ret;
			*nextState = NP_IMPS_CTL_STATE_ACTIVE;
		}
	}

	return npImpsCtlFlushAllDone(ctx);
}

static int npImpsCtlStateActive(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	return npImpsCtlStateActiveCommon(ctx, msg, nextState, false);
}

/* State 5 (WAIT_CRON_SETUP): 0x8131F044 */
static int npImpsCtlStateWaitCronSetup(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	(void)msg;
	int targetLevel = NP_IMPS_MODE_NONE;
	int ret = npImpsCtlClientMgrResolveTarget(&ctx->clientMgr, 0, &targetLevel);
	if (ret < 0)
		return ret;
	if (targetLevel != NP_IMPS_MODE_NONE) {
		ret = npImpsCtlApplySessionMode(ctx, targetLevel);
		if (ret < 0)
			return ret;
		*nextState = NP_IMPS_CTL_STATE_ACTIVE;
		return 0;
	}

	int intervalSec = -1;
	ret = npImpsCtlGetAutoIntervalSec(ctx, &intervalSec);
	if (ret < 0)
		return ret;
	if (intervalSec < 0) {
		*nextState = NP_IMPS_CTL_STATE_WAIT_CRON_WAKE;
		return 0;
	}

	if (ctx->cronHandle != 0) {
		ShellCron *cron = shellCronGetInstance();
		if (cron != NULL)
			cron->Remove(ctx->cronHandle);
		ctx->cronHandle = 0;
	}

	ShellCronOption cronOpt;
	cronOpt.wakeUp = true;
	cronOpt.persist = false;
	SceUID cronHandle = 0;
	ShellCron *cron = shellCronGetInstance();
	ret = cron->AddInterval(&cronHandle, npImpsCtlOnCronWakeup, ctx,
				static_cast<unsigned int>(intervalSec), 0, &cronOpt);
	if (ret < 0) {
		if (cronHandle != 0)
			cron->Remove(cronHandle);
		return ret;
	}
	ctx->cronHandle = cronHandle;
	*nextState = NP_IMPS_CTL_STATE_WAIT_CRON_WAKE;
	return 0;
}

static int npImpsCtlStateWaitCronWake(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	return npImpsCtlStateActiveCommon(ctx, msg, nextState, true);
}

/* State 7 (RECOVERY): 0x8131F152 */
static int npImpsCtlStateRecovery(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	(void)msg;
	SceUInt8 started = 0;
	int ret = npImpsCtlTryStartSession(ctx, &started);
	if (ret < 0)
		return ret;
	*nextState = (started != 0) ? NP_IMPS_CTL_STATE_ACTIVE : NP_IMPS_CTL_STATE_SUSPENDED;
	return npImpsCtlFlushAllDone(ctx);
}

/* State 8 (SUSPENDED): 0x8131F19A */
static int npImpsCtlStateSuspended(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	int ret = 0;
	switch (msg->kind) {
	case NP_IMPS_CTL_MSG_CLOSE:
		*nextState = NP_IMPS_CTL_STATE_CLOSED;
		break;
	case NP_IMPS_CTL_MSG_SIGN_OUT:
		*nextState = NP_IMPS_CTL_STATE_SIGN_OUT_CLEAN;
		break;
	case NP_IMPS_CTL_MSG_CLIENT_UPDATE:
		if (msg->body.requestId != 0) {
			NpImpsCtlClientResult info;
			ret = npImpsCtlClientMgrPromoteQueued(&ctx->clientMgr, msg->body.requestId,
							      &info);
			if (ret < 0)
				return ret;
		}
		break;
	case NP_IMPS_CTL_MSG_PRESENCE_UPDATE:
		npImpsCtlReplaceCachedPresence(ctx, msg->body.presence);
		msg->body.presence = NULL;
		break;
	case NP_IMPS_CTL_MSG_AUTO_SYNC_RESET:
		ret = npImpsCtlClientMgrSetAutoSyncLevel(&ctx->clientMgr, NP_IMPS_MODE_CRON_POLL);
		if (ret < 0)
			return ret;
		break;
	default:
		npImpsCtlRejectUnexpectedMsg(ctx, msg, NP_IMPS_CTL_ERR_BAD_STATE);
		break;
	}

	if (*nextState == NP_IMPS_CTL_STATE_SUSPENDED) {
		SceUInt8 started = 0;
		ret = npImpsCtlTryStartSession(ctx, &started);
		if (ret < 0)
			return ret;
		if (started != 0)
			*nextState = NP_IMPS_CTL_STATE_ACTIVE;
	}
	return npImpsCtlFlushAllDone(ctx);
}

/* State 9 (SIGN_OUT_CLEAN): 0x8131F262 */
static int npImpsCtlStateSignOutClean(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	(void)ctx;
	(void)msg;
	*nextState = NP_IMPS_CTL_STATE_SIGNED_OUT_RESET;
	return 0;
}

/* State 10 (SIGNING_OUT): 0x8131F26A */
static int npImpsCtlStateSigningOut(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	int ret = 0;
	switch (msg->kind) {
	case NP_IMPS_CTL_MSG_TX_DONE:
		ret = npImpsCtlTxCompleteByCoreId(&ctx->txQueue, msg->body.coreEvent.eventOrTxId,
						  msg->body.coreEvent.errorOrResult);
		if (ret < 0)
			return ret;
		break;
	case NP_IMPS_CTL_MSG_CORE_SESSION_EVENT: {
		int stopped = 0;
		ret = npImpsCtlHandleDisconnectingSessionEvent(ctx, msg, &stopped);
		if (ret < 0)
			return ret;
		if (stopped != 0)
			*nextState = NP_IMPS_CTL_STATE_SIGNED_OUT_RESET;
		break;
	}
	case NP_IMPS_CTL_MSG_SIGN_IN:
		*nextState = NP_IMPS_CTL_STATE_SIGN_OUT_REENTER;
		break;
	case NP_IMPS_CTL_MSG_CLOSE:
		*nextState = NP_IMPS_CTL_STATE_CLOSING;
		break;
	default:
		npImpsCtlRejectUnexpectedMsg(ctx, msg, NP_IMPS_CTL_ERR_BAD_STATE);
		break;
	}
	return npImpsCtlFlushAllDone(ctx);
}

/* State 11 (SIGNED_OUT_RESET): 0x8131F2F0 */
static int npImpsCtlStateSignedOutReset(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	(void)ctx;
	(void)msg;
	*nextState = NP_IMPS_CTL_STATE_SIGNED_OUT;
	return 0;
}

/* State 12 (SIGN_OUT_REENTER): 0x8131F2F6 */
static int npImpsCtlStateSignOutReenter(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	(void)ctx;
	(void)msg;
	*nextState = NP_IMPS_CTL_STATE_SIGNING_IN;
	return 0;
}

/* State 13 (CLOSING): 0x8131F2FE */
static int npImpsCtlStateClosing(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	int ret = 0;
	if (msg->kind == NP_IMPS_CTL_MSG_TX_DONE) {
		ret = npImpsCtlTxCompleteByCoreId(&ctx->txQueue, msg->body.coreEvent.eventOrTxId,
						  msg->body.coreEvent.errorOrResult);
		if (ret < 0)
			return ret;
	} else if (msg->kind == NP_IMPS_CTL_MSG_CORE_SESSION_EVENT) {
		int stopped = 0;
		ret = npImpsCtlHandleDisconnectingSessionEvent(ctx, msg, &stopped);
		if (ret < 0)
			return ret;
		if (stopped != 0)
			*nextState = NP_IMPS_CTL_STATE_CLOSED;
	} else {
		npImpsCtlRejectUnexpectedMsg(ctx, msg, NP_IMPS_CTL_ERR_BAD_STATE);
	}
	return npImpsCtlFlushAllDone(ctx);
}

/* State 14 (CLOSED): 0x8131F378 */
static int npImpsCtlStateClosed(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	(void)ctx;
	(void)msg;
	(void)nextState;
	return NP_IMPS_CTL_ERR_BAD_STATE;
}

/* State 15 (FATAL): 0x8131F382 */
static int npImpsCtlStateFatal(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState)
{
	(void)msg;
	(void)nextState;
	return ctx->lastError;
}

typedef int (*NpImpsCtlStateFn)(NpImpsCtlContext *ctx, NpImpsCtlMsg *msg, int *nextState);

struct NpImpsCtlStateDesc {
	NpImpsCtlStateFn handler;
	SceUInt8 waitsForMsg;
};

static const NpImpsCtlStateDesc s_ctlStateTable[16] = {
	{ npImpsCtlStateSignedOut,	1 },	/* 0 */
	{ npImpsCtlStateSigningIn,	0 },	/* 1 */
	{ npImpsCtlStateSignInOk,	0 },	/* 2 */
	{ npImpsCtlStateSignInFail,	0 },	/* 3 */
	{ npImpsCtlStateActive,		1 },	/* 4 */
	{ npImpsCtlStateWaitCronSetup,	0 },	/* 5 */
	{ npImpsCtlStateWaitCronWake,	1 },	/* 6 */
	{ npImpsCtlStateRecovery,	0 },	/* 7 */
	{ npImpsCtlStateSuspended,	1 },	/* 8 */
	{ npImpsCtlStateSignOutClean,	0 },	/* 9 */
	{ npImpsCtlStateSigningOut,	1 },	/* 10 */
	{ npImpsCtlStateSignedOutReset,	0 },	/* 11 */
	{ npImpsCtlStateSignOutReenter,	0 },	/* 12 */
	{ npImpsCtlStateClosing,	1 },	/* 13 */
	{ npImpsCtlStateClosed,		0 },	/* 14 */
	{ npImpsCtlStateFatal,		0 }	/* 15 */
};

/* 0x8131E840 */
static int npImpsCtlTransitionState(NpImpsCtlContext *ctx, int newState)
{
	int err = ctx->lastError;
	int lockState = npImpsCtxLock(ctx);
	if (lockState < 0)
		return lockState;

	int prevState = ctx->state;
	ctx->state = newState;
	npImpsCtxUnlock(ctx, lockState);

	if (newState == NP_IMPS_CTL_STATE_RECOVERY ||
	    newState == NP_IMPS_CTL_STATE_SIGNED_OUT_RESET ||
	    newState == NP_IMPS_CTL_STATE_SIGN_OUT_REENTER) {
		if (newState == NP_IMPS_CTL_STATE_SIGNED_OUT_RESET ||
		    newState == NP_IMPS_CTL_STATE_SIGN_OUT_REENTER) {
			if (err >= 0)
				err = NP_IMPS_CTL_ERR_NOT_READY;
		} else if (err >= 0) {
			err = NP_IMPS_CTL_ERR_INTERNAL;
		}

		if (ctx->accountEventCbId != -1) {
			SceShell_B4D888D5(ctx->accountEventCbId);
			ctx->accountEventCbId = -1;
		}
		int ret = npImpsCtlClientMgrResetWaiting(&ctx->clientMgr, err);
		if (ret >= 0)
			ret = npImpsCtlTearDownCoreSession(ctx, err);
		if (ret >= 0)
			ret = npImpsCtlFlushAllDone(ctx);
		if (ret < 0)
			return ret;
	}

	if (newState == NP_IMPS_CTL_STATE_FATAL)
		npAccountClearSession();

	if (newState == NP_IMPS_CTL_STATE_CLOSED || newState == NP_IMPS_CTL_STATE_FATAL) {
		if (newState == NP_IMPS_CTL_STATE_CLOSED) {
			if (err >= 0)
				err = NP_IMPS_CTL_ERR_CLOSED;
		}

		if (ctx->accountEventCbId != -1) {
			SceShell_B4D888D5(ctx->accountEventCbId);
			ctx->accountEventCbId = -1;
		}
		int ret = npImpsCtlDrainPendingMsgs(ctx, err);
		if (ret >= 0)
			ret = npImpsCtlClientMgrFailAllPending(&ctx->clientMgr, err);
		if (ret >= 0)
			ret = npImpsCtlTxFailAll(&ctx->txQueue, err);
		if (ret >= 0)
			ret = npImpsCtlFlushAllDone(ctx);
		if (ret < 0)
			return ret;
	}

	if (prevState == NP_IMPS_CTL_STATE_WAIT_CRON_WAKE && ctx->cronHandle != 0) {
		ShellCron *cron = shellCronGetInstance();
		if (cron != NULL)
			cron->Remove(ctx->cronHandle);
		ctx->cronHandle = 0;
	}

	switch (newState) {
	case NP_IMPS_CTL_STATE_SIGNING_IN:
		npImpsCtlNotifyCtlEvent(0);
		break;
	case NP_IMPS_CTL_STATE_SIGN_OUT_CLEAN:
	case NP_IMPS_CTL_STATE_SIGNING_OUT:
		npImpsCtlNotifyCtlEvent(1);
		break;
	case NP_IMPS_CTL_STATE_CLOSED:
	case NP_IMPS_CTL_STATE_FATAL:
		if (prevState != -1 && prevState != NP_IMPS_CTL_STATE_SIGNED_OUT &&
		    prevState != NP_IMPS_CTL_STATE_SIGN_OUT_CLEAN &&
		    prevState != NP_IMPS_CTL_STATE_SIGNING_OUT &&
		    prevState != NP_IMPS_CTL_STATE_SIGNED_OUT_RESET &&
		    prevState != NP_IMPS_CTL_STATE_SIGN_OUT_REENTER &&
		    prevState != NP_IMPS_CTL_STATE_FATAL) {
			npImpsCtlNotifyCtlEvent(1);
		}
		break;
	default:
		break;
	}
	return 0;
}

/* 0x8131E9D2 */
static int npImpsCtlStepState(const NpImpsCtlStateDesc *desc, NpImpsCtlContext *ctx,
			      NpImpsCtlMsg *msg)
{
	int nextState = ctx->state;
	int ret = desc->handler(ctx, (desc->waitsForMsg != 0) ? msg : NULL, &nextState);
	if (ret < 0) {
		int curState = ctx->state;
		if (curState == NP_IMPS_CTL_STATE_CLOSING) {
			nextState = NP_IMPS_CTL_STATE_CLOSED;
		} else if (curState == NP_IMPS_CTL_STATE_SIGN_OUT_CLEAN ||
			   curState == NP_IMPS_CTL_STATE_SIGNING_OUT) {
			nextState = NP_IMPS_CTL_STATE_SIGNED_OUT_RESET;
		} else if (curState >= NP_IMPS_CTL_STATE_SIGNING_IN &&
			   curState <= NP_IMPS_CTL_STATE_SIGN_OUT_REENTER) {
			ctx->lastError = ret;
			nextState = NP_IMPS_CTL_STATE_RECOVERY;
			if (curState == NP_IMPS_CTL_STATE_RECOVERY) {
				int transRet = npImpsCtlTransitionState(ctx, NP_IMPS_CTL_STATE_RECOVERY);
				if (transRet < 0)
					return transRet;
			}
		} else {
			npAccountClearSession();
			ctx->lastError = ret;
			nextState = NP_IMPS_CTL_STATE_RECOVERY;
		}
	}

	if (ctx->state == nextState)
		return 0;
	return npImpsCtlTransitionState(ctx, nextState);
}

/* 0x8131EA98 */
static int npImpsCtlMainLoop(NpImpsCtlContext *ctx)
{
	while (ctx->state != NP_IMPS_CTL_STATE_CLOSED) {
		NpImpsCtlMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_CTL_MSG_INVALID;

		const NpImpsCtlStateDesc *desc = &s_ctlStateTable[ctx->state];
		int ret = 0;
		if (npImpsCtxIsAborted(ctx)) {
			ret = NP_IMPS_CTL_ERR_ABORTED;
		} else if (desc->waitsForMsg != 0) {
			ret = npImpsCtlRecvFilteredMsg(ctx, &msg, NULL);
			if (ret >= 0)
				ret = npImpsCtlStepState(desc, ctx, &msg);
		} else {
			ret = npImpsCtlStepState(desc, ctx, NULL);
		}

		if (ret < 0) {
			ctx->lastError = ret;
			npImpsCtlTransitionState(ctx, NP_IMPS_CTL_STATE_FATAL);
			return ret;
		}
	}
	return 0;
}

/* ============================================================================
 * Context command senders (0x8131C0F8..0x8131C5AB)
 * ============================================================================ */

/* 0x8131C0F8 */
static int npImpsCtlAddClient(NpImpsCtlContext *ctx, SceUID processId, int requestedLevel,
			      NpImpsCtlClientCallback callback, void *userArg, int *outReqId)
{
	int reqId = 0;
	int ret = npImpsCtlClientMgrAdd(&ctx->clientMgr, processId, requestedLevel,
					callback, userArg, &reqId);
	if (ret >= 0) {
		NpImpsCtlMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_CTL_MSG_CLIENT_UPDATE;
		msg.body.requestId = reqId;
		SceUInt32 allowedMask = 0x01FEu;
		ret = npImpsCtlPostCommand(ctx, &allowedMask, &msg, 0);
		if (ret == NP_IMPS_CTL_ERR_BAD_STATE)
			ret = NP_IMPS_CTL_ERR_NOT_READY;
		if (ret >= 0) {
			if (outReqId != NULL)
				*outReqId = reqId;
			return 0;
		}
	}
	if (reqId != 0)
		npImpsCtlClientMgrRemoveByReqId(&ctx->clientMgr, reqId, NULL);
	return ret;
}

/* 0x8131C19C */
static int npImpsCtlMarkAutoSyncDirty(NpImpsCtlContext *ctx)
{
	int ret = npImpsCtlClientMgrMarkDirty(&ctx->clientMgr);
	if (ret < 0)
		return ret;
	NpImpsCtlMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_CTL_MSG_CLIENT_UPDATE;
	msg.body.requestId = 0;
	SceUInt32 allowedMask = 0x01FEu;
	ret = npImpsCtlPostCommand(ctx, &allowedMask, &msg, 0);
	if (ret == NP_IMPS_CTL_ERR_BAD_STATE || ret >= 0)
		return 0;
	return ret;
}

/* 0x8131C206 */
static int npImpsCtlRemoveClient(NpImpsCtlContext *ctx, SceUID processId, int *outActiveCount)
{
	int ret = npImpsCtlClientMgrCancelByPid(&ctx->clientMgr, processId, outActiveCount);
	if (ret < 0)
		return ret;
	NpImpsCtlMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_CTL_MSG_CLIENT_UPDATE;
	msg.body.requestId = 0;
	SceUInt32 allowedMask = 0x01FEu;
	ret = npImpsCtlPostCommand(ctx, &allowedMask, &msg, 0);
	if (ret == NP_IMPS_CTL_ERR_BAD_STATE)
		return NP_IMPS_CTL_ERR_NOT_READY;
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131C27A */
static int npImpsCtlCancelAllClients(NpImpsCtlContext *ctx)
{
	int ret = npImpsCtlClientMgrCancelAllProcesses(&ctx->clientMgr);
	if (ret < 0)
		return ret;
	NpImpsCtlMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_CTL_MSG_CLIENT_UPDATE;
	msg.body.requestId = 0;
	SceUInt32 allowedMask = 0x01FEu;
	ret = npImpsCtlPostCommand(ctx, &allowedMask, &msg, 0);
	if (ret == NP_IMPS_CTL_ERR_BAD_STATE)
		return NP_IMPS_CTL_ERR_NOT_READY;
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131C312 */
static int npImpsCtlEnqueuePresence(NpImpsCtlContext *ctx, const NpImpsCorePresenceUpdate *update)
{
	NpImpsCorePresenceUpdate *cloned = NULL;
	int ret = 0;

	if (update != NULL) {
		NpRtPresenceStanza *probeStanza = NULL;
		ret = npRtImpsBuildPresenceStanza(update, &probeStanza);
		if (ret < 0) {
			if (ret != NP_RT_CORE_ERR_NO_MEMORY)
				ret = NP_RT_CORE_ERR_INVALID_ARG;
			return ret;
		}
		npRtReleasePresenceStanza(probeStanza);
		ret = npRtImpsClonePresenceUpdate(&cloned, update);
		if (ret < 0)
			return ret;
	}

	NpImpsCtlMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_CTL_MSG_PRESENCE_UPDATE;
	msg.body.presence = cloned;
	SceUInt32 allowedMask = 0x01FEu;
	ret = npImpsCtlPostCommand(ctx, &allowedMask, &msg, 0);
	if (ret == NP_IMPS_CTL_ERR_BAD_STATE)
		ret = NP_IMPS_CTL_ERR_NOT_READY;
	if (ret >= 0)
		return 0;

	if (cloned != NULL)
		npRtImpsFreePresenceUpdate(cloned, NULL);
	return ret;
}

/* 0x8131C3D0 */
static int npImpsCtlEnqueueMessage(NpImpsCtlContext *ctx, const SceNpId *targetNpId,
				   const NpImpsCoreMessage *message,
				   NpImpsCoreSendMsgCallback callback, int *outTxId)
{
	if (message == NULL)
		return NP_IMPS_CTL_ERR_INVALID_ARG;

	NpImpsCoreMessage *cloned = NULL;
	NpImpsCtlTxNode *txNode = NULL;
	int txId = 0;

	int ret = npRtImpsCloneMessage(&cloned, message);
	if (ret >= 0)
		ret = npImpsCoreAllocTransactionId(&txId);
	if (ret >= 0) {
		ret = npImpsCtlCreateMessageTx(ctx, txId, npImpsCtlSendMsgAdapter,
					       reinterpret_cast<void *>(callback), &txNode);
	}
	if (ret >= 0) {
		txNode->txKind = 0;
		txNode->targetNpId = *targetNpId;
		txNode->message = cloned;
		cloned = NULL;

		NpImpsCtlMsg msg;
		sceClibMemset(&msg, 0, sizeof(msg));
		msg.kind = NP_IMPS_CTL_MSG_SEND_MESSAGE;
		msg.body.requestId = txNode->header.requestId;
		SceUInt32 allowedMask = 0x01FEu;
		ret = npImpsCtlPostCommand(ctx, &allowedMask, &msg, 0);
		if (ret == NP_IMPS_CTL_ERR_BAD_STATE)
			ret = NP_IMPS_CTL_ERR_NOT_READY;
		if (ret >= 0) {
			if (outTxId != NULL)
				*outTxId = txId;
			return 0;
		}
	}

	if (txNode != NULL)
		npImpsCtlRemoveMessageTx(ctx, txNode);
	if (cloned != NULL)
		npRtImpsFreeMessage(cloned, NULL);
	return ret;
}

/* 0x8131C4E8 */
static int npImpsCtlSendSignOutCmd(NpImpsCtlContext *ctx)
{
	NpImpsCtlMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_CTL_MSG_SIGN_OUT;
	SceUInt32 allowedMask = 0x1FFFu;
	int ret = npImpsCtlPostCommand(ctx, &allowedMask, &msg, 0);
	if (ret == NP_IMPS_CTL_ERR_BAD_STATE)
		return NP_IMPS_CTL_ERR_SIGNED_OUT;
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131C54A */
static int npImpsCtlSendSignInCmd(NpImpsCtlContext *ctx)
{
	NpImpsCtlMsg msg;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.kind = NP_IMPS_CTL_MSG_SIGN_IN;
	SceUInt32 allowedMask = 0x1FFFu;
	int ret = npImpsCtlPostCommand(ctx, &allowedMask, &msg, 0);
	if (ret == NP_IMPS_CTL_ERR_BAD_STATE)
		return NP_IMPS_CTL_ERR_SIGNED_OUT;
	return (ret >= 0) ? 0 : ret;
}

/* ============================================================================
 * TriggerMgr & Account hooks + Public SceNpImpsCtl API (0x8131B358..0x8131B8DF)
 * ============================================================================ */

/* 0x8131B474 */
static void npImpsCtlOnNetAccountEvent(int eventType, void *userArg)
{
	(void)userArg;
	if (eventType == 0)
		npImpsCtlTriggerSignIn();
	else if (eventType == 1)
		npImpsCtlTriggerSignOut();
}

/* 0x8131B498 */
static void npImpsCtlOnAutoSyncChanged(bool enabled, void *userArg)
{
	(void)enabled;
	(void)userArg;
	npImpsCtlNotifyAutoSyncChanged();
}

/* 0x8131B8D4 */
static void npImpsCtlClientConnectAdapter(const int *contextIdPtr,
					  const NpImpsCtlClientResult *result,
					  void *userArg)
{
	(void)contextIdPtr;
	NpImpsCoreConnectCallback userCb = reinterpret_cast<NpImpsCoreConnectCallback>(userArg);
	if (userCb != NULL)
		userCb(result->processId, result->errorCode);
}

/* 0x8131B358 */
int npImpsCtlInit(const NpPollingThreadParam *threadParam)
{
	int ret = npImpsCtlMutexCreate();
	if (ret >= 0) {
		ret = npImpsCtlRegisterCoreBridge();
		if (ret >= 0) {
			ret = npImpsCtxMgrInit(threadParam);
			if (ret >= 0) {
				ret = shellTriggerMgrSetServiceCallback(
					NP_IMPS_TRIGGER_SERVICE_ID,
					npImpsCtlOnAutoSyncChanged, NULL);
				if (ret >= 0)
					ret = shellTriggerMgrRegisterService(NP_IMPS_TRIGGER_SERVICE_ID);
				if (ret >= 0) {
					if (s_netAccountCbId < 0) {
						int cbRet = npAccountRegisterNetCallback(
							npImpsCtlOnNetAccountEvent, NULL);
						if (cbRet < 0) {
							ret = cbRet;
						} else {
							s_netAccountCbId = cbRet;
							return 0;
						}
					} else {
						ret = NP_IMPS_CTL_ERR_ALREADY_INIT;
					}
				}
			}
		}
	}

	if (s_netAccountCbId >= 0) {
		npAccountUnregisterNetCallback(s_netAccountCbId);
		s_netAccountCbId = -1;
	}
	shellTriggerMgrUnregisterService(NP_IMPS_TRIGGER_SERVICE_ID);
	shellTriggerMgrClearServiceCallback(NP_IMPS_TRIGGER_SERVICE_ID);
	npImpsCtxMgrTerm();
	npImpsCtlUnregisterCoreBridge();
	npImpsCtlMutexDelete();
	return ret;
}

/* 0x8131B430 */
int npImpsCtlTerm(void)
{
	if (s_netAccountCbId >= 0) {
		npAccountUnregisterNetCallback(s_netAccountCbId);
		s_netAccountCbId = -1;
	}
	shellTriggerMgrUnregisterService(NP_IMPS_TRIGGER_SERVICE_ID);
	shellTriggerMgrClearServiceCallback(NP_IMPS_TRIGGER_SERVICE_ID);
	npImpsCtxMgrTerm();
	npImpsCtlUnregisterCoreBridge();
	return npImpsCtlMutexDelete();
}

/* 0x8131B4A0 */
int npImpsCtlStartDefault(void)
{
	if (s_defaultCtlStarted != 0)
		return NP_IMPS_CTL_ERR_ALREADY_STARTED;

	NpImpsCtlContext *ctx = NULL;
	int ret = npImpsCtxCreate(&ctx);
	if (ret >= 0)
		ret = npImpsCtxStartWorker(ctx);
	if (ret < 0) {
		if (ctx != NULL)
			npImpsCtxClose(ctx);
		return ret;
	}

	s_defaultCtlCtxId = ctx->contextId;
	s_defaultCtlStarted = 1;
	return 0;
}

/* 0x8131B51A */
int npImpsCtlIsStarted(void)
{
	return static_cast<int>(s_defaultCtlStarted);
}

/* 0x8131B526 */
int npImpsCoreConnectProcess(SceUID processId, NpImpsCoreConnectCallback callback)
{
	if (s_defaultCtlCtxId == 0)
		return NP_IMPS_CTL_ERR_NOT_STARTED;
	if (processId == -1)
		return NP_IMPS_CTL_ERR_INVALID_ARG;

	NpImpsCtlContext *ctx = NULL;
	int ret = npImpsCtxFindById(s_defaultCtlCtxId, &ctx, 0);
	if (ret >= 0)
		ret = npImpsCtlAddClient(ctx, processId, NP_IMPS_MODE_ONLINE,
					 npImpsCtlClientConnectAdapter,
					 reinterpret_cast<void *>(callback), NULL);
	if (ctx != NULL)
		npImpsCtxRelease(ctx);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131B526 bridge for power_manage_plugin */
int shellNpPushResumeNotify(SceUID processId, int flags)
{
	(void)flags;
	return npImpsCoreConnectProcess(processId, NULL);
}

/* 0x8131B5B4 */
int npImpsCoreDisconnectProcess(SceUID processId)
{
	if (s_defaultCtlCtxId == 0)
		return NP_IMPS_CTL_ERR_NOT_STARTED;
	if (processId == -1)
		return NP_IMPS_CTL_ERR_INVALID_ARG;

	NpImpsCtlContext *ctx = NULL;
	int remaining = 0;
	int ret = npImpsCtxFindById(s_defaultCtlCtxId, &ctx, 0);
	if (ret >= 0)
		ret = npImpsCtlRemoveClient(ctx, processId, &remaining);
	if (ctx != NULL)
		npImpsCtxRelease(ctx);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131B632 */
int npImpsCoreResyncSession(void)
{
	if (s_defaultCtlCtxId == 0)
		return NP_IMPS_CTL_ERR_NOT_STARTED;

	NpImpsCtlContext *ctx = NULL;
	int ret = npImpsCtxFindById(s_defaultCtlCtxId, &ctx, 0);
	if (ret >= 0)
		ret = npImpsCtlCancelAllClients(ctx);
	if (ctx != NULL)
		npImpsCtxRelease(ctx);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131B69A */
int npImpsCoreUpdatePresence(const NpImpsCorePresenceUpdate *update)
{
	if (s_defaultCtlCtxId == 0)
		return NP_IMPS_CTL_ERR_NOT_STARTED;

	NpImpsCtlContext *ctx = NULL;
	int ret = npImpsCtxFindById(s_defaultCtlCtxId, &ctx, 0);
	if (ret >= 0)
		ret = npImpsCtlEnqueuePresence(ctx, update);
	if (ctx != NULL)
		npImpsCtxRelease(ctx);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131B706 */
int npImpsCoreSendInGameMessage(const SceNpId *targetNpId, const NpImpsCoreMessage *msg,
				NpImpsCoreSendMsgCallback callback, int *outTransactionId)
{
	if (s_defaultCtlCtxId == 0)
		return NP_IMPS_CTL_ERR_NOT_STARTED;
	if (msg->type == 0 && msg->storedExt != NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;

	NpImpsCtlContext *ctx = NULL;
	int ret = npImpsCtxFindById(s_defaultCtlCtxId, &ctx, 0);
	if (ret >= 0)
		ret = npImpsCtlEnqueueMessage(ctx, targetNpId, msg, callback, outTransactionId);
	if (ctx != NULL)
		npImpsCtxRelease(ctx);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131B79C */
static int npImpsCtlNotifyAutoSyncChanged(void)
{
	if (s_defaultCtlCtxId == 0)
		return NP_IMPS_CTL_ERR_NOT_STARTED;

	NpImpsCtlContext *ctx = NULL;
	int ret = npImpsCtxFindById(s_defaultCtlCtxId, &ctx, 0);
	if (ret >= 0)
		ret = npImpsCtlMarkAutoSyncDirty(ctx);
	if (ctx != NULL)
		npImpsCtxRelease(ctx);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131B804 */
static int npImpsCtlTriggerSignIn(void)
{
	if (s_defaultCtlCtxId == 0)
		return NP_IMPS_CTL_ERR_NOT_STARTED;

	NpImpsCtlContext *ctx = NULL;
	int ret = npImpsCtxFindById(s_defaultCtlCtxId, &ctx, 0);
	if (ret >= 0)
		ret = npImpsCtlSendSignInCmd(ctx);
	if (ctx != NULL)
		npImpsCtxRelease(ctx);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131B86C */
static int npImpsCtlTriggerSignOut(void)
{
	if (s_defaultCtlCtxId == 0)
		return NP_IMPS_CTL_ERR_NOT_STARTED;

	NpImpsCtlContext *ctx = NULL;
	int ret = npImpsCtxFindById(s_defaultCtlCtxId, &ctx, 0);
	if (ret >= 0)
		ret = npImpsCtlSendSignOutCmd(ctx);
	if (ctx != NULL)
		npImpsCtxRelease(ctx);
	return (ret >= 0) ? 0 : ret;
}
