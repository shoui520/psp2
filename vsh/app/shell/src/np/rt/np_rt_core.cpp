/*
 * np_rt_core.cpp: SceNpCore handler registry, SceNpSession lifecycle & state
 * machine, IQ transaction queue, SceNpCoreMain / SceNpCoreRecv worker threads,
 * and NP-TICKET-AUTH-COMPACT SASL authentication
 * (0x81310320..0x81312637, 0x81313DE4..0x81314523, 0x8131616C..0x8131696F).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sce_atomic.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <libnet/netinternal.h>
#include <bXCe/bxce_internal.h>
#include <np_common/np_common.h>
#include "np_rt_core.h"

extern "C" int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

/* Account ticket request helpers (0x813268BC..0x813269F0) */
extern int npAccountCreateRequest(void);
extern int npAccountDestroyRequest(int reqHandle);
extern int npAccountGetTicketForService(int reqHandle, void *ticketBuf, SceSize *ioSize,
					int serviceId);
extern int npAccountAbortRequest(int reqHandle);

namespace {

static const SceUInt32 kMaxHandlers = 8;
static const SceUInt32 kPipeBufferSize = 0x1800;		/* 128 * 48 bytes */
static const SceUInt32 kPipeSendTimeoutUsec = 10000000;		/* 10s */
static const SceUInt64 kDefaultTxTimerUsec = 10000000ULL;	/* 10s */
static const SceUInt64 kDefaultTxExpireUsec = 20000000ULL;	/* 20s */
static const SceUInt64 kMaxTxExpireUsec = 60000000ULL;		/* 60s */
static const SceUInt64 kConnectTimeoutUsec = 30000000ULL;	/* 30s */
static const SceSize kTicketBufSize = 4096;
static const int kRtTicketServiceId = 14;
static const SceSize kSessionSidMaxSize = 42;

static const int kKernelErrPipeTimeout = -2147319803;		/* 0x80028005 */
static const int kKernelErrPipeEmpty = -2147319261;		/* 0x80028223 */

enum NpRtSessionFlags {
	NP_RT_SFLAG_LINKED	= 0x01,
	NP_RT_SFLAG_CLOSING	= 0x10
};

enum NpRtPipeCmdId {
	NP_RT_CMD_NONE		= -1,
	NP_RT_CMD_ABORT		= 0,
	NP_RT_CMD_CONNECT	= 1,
	NP_RT_CMD_RECONNECT	= 2,
	NP_RT_CMD_SEND_IQ	= 4,
	NP_RT_CMD_SEND_MESSAGE	= 5,
	NP_RT_CMD_SEND_PRESENCE	= 6,
	NP_RT_CMD_SEND_RAW_XML	= 7,
	NP_RT_CMD_DISCONNECT	= 8,
	NP_RT_CMD_RECV_STANZA	= 9,
	NP_RT_CMD_RECV_CLOSED	= 10
};

enum NpRtTxListState {
	NP_RT_TX_UNLISTED	= 0,
	NP_RT_TX_PENDING	= 1,
	NP_RT_TX_EXPIRED	= 2
};

/* Registered handler node (32 bytes, 0x8131081A) */
struct NpRtHandlerNode {
	NpRtHandlerNode *next;		/* +0x00 */
	NpRtHandlerNode *prev;		/* +0x04 */
	int handlerId;			/* +0x08 */
	NpRtHandlerConfig config;	/* +0x0C (20 bytes) */
};

struct NpRtTxQueue;

/* Pending / expired IQ transaction node (128 bytes = 0x80, 0x813117B6) */
struct NpRtTxNode {
	NpRtTxQueue *ownerQueue;	/* +0x00 */
	int listState;			/* +0x04: NpRtTxListState */
	NpRtTxNode *next;		/* +0x08 */
	NpRtTxNode *prev;		/* +0x0C */
	char stanzaId[32];		/* +0x10: "psp2-<hex>" */
	int handlerId;			/* +0x30 */
	int transactionId;		/* +0x34 */
	NpRtIqResponseCallback callback; /* +0x38 */
	void *userArg;			/* +0x3C */
	SceUInt64 timeoutUsec;		/* +0x40 */
	NpPollingTimeoutTimer timer;	/* +0x48 (40 bytes) */
	SceUInt64 sentTimeUsec;		/* +0x70 */
	int errorCode;			/* +0x78 */
	SceUInt32 padTail;		/* +0x7C */
};

typedef int (*NpRtTxWakeCallback)(NpRtTxQueue *queue, void *arg);

/* IQ transaction queue embedded at NpRtSession +0x100 (32 bytes, 0x813116F4) */
struct NpRtTxQueue {
	SceKernelLwMutexWork *mutex;	/* +0x00 */
	NpRtTxWakeCallback onWake;	/* +0x04 */
	void *wakeArg;			/* +0x08 */
	int activeCount;		/* +0x0C */
	NpRtTxNode *pendingHead;	/* +0x10 */
	NpRtTxNode *pendingTail;	/* +0x14 */
	NpRtTxNode *expiredHead;	/* +0x18 */
	NpRtTxNode *expiredTail;	/* +0x1C */
};

/* Receive worker thread descriptor (8 bytes, 0x81313E58) */
struct NpRtRecvThread {
	SceUID threadId;		/* +0x00 */
	int stopRequested;		/* +0x04 */
};

/* 48-byte command packet passed through SceNpSession msgpipe */
struct NpRtPipeCommand {
	int commandId;			/* +0x00: NpRtPipeCmdId */
	union {
		char reconnectSid[42];
		struct {
			NpRtStanza *stanza;
			int handlerId;
			int transactionId;
		} sendIq;
		struct {
			void *stanza;
			int handlerId;
			int transactionId;
			NpRtSendDoneCallback callback;
			void *userArg;
		} sendStanza;
		struct {
			char *xmlStr;
			SceSize xmlLen;
		} sendRaw;
		NpRtAnyStanza recvStanza;
		int closeResult;
		SceUInt8 rawBody[44];
	} body;
};

/* Main XMPP session object (472 bytes = 0x1D8, 0x81310FC4) */
struct NpRtSession {
	SceKernelLwMutexWork mutex;	/* +0x000 (32 bytes) */
	SceUInt32 sessionFlags;		/* +0x020 */
	int refCount;			/* +0x024 */
	NpRtSession *next;		/* +0x028 */
	NpRtSession *prev;		/* +0x02C */
	NpRtSessionIdentity identity;	/* +0x030 (200 bytes) */
	int state;			/* +0x0F8: NpRtSessionState */
	SceUID msgPipeId;		/* +0x0FC */
	NpRtTxQueue txQueue;		/* +0x100 (32 bytes) */
	NpRtXmppClient xmppClient;	/* +0x120 (128 bytes) */
	NpPollingTimeoutTimer connectTimer; /* +0x1A0 (40 bytes) */
	SceUID *mainThreadPtr;		/* +0x1C8 */
	NpRtRecvThread *recvThreadPtr;	/* +0x1CC */
	int aborted;			/* +0x1D0 */
	int accountReqHandle;		/* +0x1D4 */
};

/* Global SceNpCore state (0x815463A8..0x815463E4, 0x815C53C8) */
static SceKernelLwMutexWork s_coreMutex;
static int s_coreMutexInit = 0;

static int s_handlerListInit = 0;
static NpRtHandlerNode *s_defaultHandler = NULL;
static NpRtHandlerNode *s_handlerHead = NULL;
static NpRtHandlerNode *s_handlerTail = NULL;
static int s_handlerCount = 0;
static int s_nextHandlerId = 0;

static volatile SceInt32 s_txIdCounter = 0;

static int s_sessionMgrInit = 0;
static NpRtSession *s_sessionHead = NULL;
static NpRtSession *s_sessionTail = NULL;
static int s_sessionCount = 0;
static int s_nextSessionId = 0;
static SceUID s_mainThreadId = -1;
static NpRtRecvThread s_recvThread = { -1, 0 };

/* Forward declarations */
static int runSessionStateMachine(NpRtSession *session);
static int pipeSendAbort(SceUID *pipeId);
static void resetPipeCommand(NpRtPipeCommand *cmd);

/* --------------------------------------------------------------------------
 * Global Core Mutex (0x81310384..0x81310427)
 * -------------------------------------------------------------------------- */
/* 0x81310384 */
static int coreMutexInit(void)
{
	if (s_coreMutexInit > 0)
		return NP_RT_CORE_ERR_ALREADY_INIT;
	int ret = sceKernelCreateLwMutex(&s_coreMutex, "SceNpCore", 0x2002u, 0, NULL);
	if (ret >= 0)
		s_coreMutexInit = 1;
	return ret;
}

/* 0x813103D4 */
static int coreMutexTerm(void)
{
	if (s_coreMutexInit >= 0)
		return sceKernelDeleteLwMutex(&s_coreMutex);
	return s_coreMutexInit;
}

/* 0x813103F2 */
static int coreMutexLock(void)
{
	int ret = sceKernelLockLwMutex(&s_coreMutex, 1, NULL);
	return (ret >= 0) ? 1 : ret;
}

/* 0x8131040E */
static void coreMutexUnlock(int lockState)
{
	if (lockState > 0)
		sceKernelUnlockLwMutex(&s_coreMutex, 1);
}

/* --------------------------------------------------------------------------
 * Handler Registry & Incoming Stanza Routing (0x8131042A..0x81310A93)
 * -------------------------------------------------------------------------- */
/* 0x8131042A */
static void dispatchStateChanged(const NpRtSessionIdentity *identity, int eventState,
				 int resultCode)
{
	if (s_defaultHandler != NULL && s_defaultHandler->config.onStateChanged != NULL)
		s_defaultHandler->config.onStateChanged(identity, eventState, resultCode);

	for (NpRtHandlerNode *cur = s_handlerHead; cur != NULL; cur = cur->next) {
		if (cur != s_defaultHandler && cur->config.onStateChanged != NULL)
			cur->config.onStateChanged(identity, eventState, resultCode);
	}
}

static NpRtHandlerNode *findHandlerForNamespaces(const char *const *nsList, SceUInt32 count)
{
	for (SceUInt32 idx = 0; idx < count && nsList[idx] != NULL; ++idx) {
		const char *stanzaNs = nsList[idx];
		SceSize nsLen = sceClibStrnlen(stanzaNs, 0x7FFFFFFFu);

		for (NpRtHandlerNode *cur = s_handlerHead; cur != NULL; cur = cur->next) {
			if (cur->config.namespaces == NULL)
				continue;
			for (const char *const *reg = cur->config.namespaces; *reg != NULL; ++reg) {
				if (sceClibStrncmp(stanzaNs, *reg, nsLen) == 0)
					return cur;
			}
		}
	}
	return NULL;
}

/* 0x81310472 */
static int dispatchIncomingIq(const NpRtSessionIdentity *identity, NpRtStanza *stanza)
{
	if (stanza->stanzaType == NP_RT_XMPP_TYPE_ERROR ||
	    stanza->stanzaType == NP_RT_XMPP_TYPE_RESULT) {
		return 0;
	}

	NpRtIqCallback targetCb = (s_defaultHandler != NULL)
					  ? s_defaultHandler->config.onIqRecv
					  : NULL;

	SceUInt32 nsCount = 0;
	int ret = npRtCollectExactXmlns(stanza->xmlDoc, NULL, &nsCount);
	if (ret < 0)
		return ret;

	const char **nsList = reinterpret_cast<const char **>(
		npRtHeapAlloc(nsCount * sizeof(const char *), NULL));
	if (nsList == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	ret = npRtCollectExactXmlns(stanza->xmlDoc, nsList, NULL);
	if (ret >= 0) {
		NpRtHandlerNode *matched = findHandlerForNamespaces(nsList, nsCount);
		if (matched != NULL)
			targetCb = matched->config.onIqRecv;
		if (targetCb != NULL)
			targetCb(identity, stanza);
		ret = 0;
	}

	npRtHeapFree(nsList, NULL);
	return ret;
}

/* 0x81310586 */
static int dispatchIncomingMessage(const NpRtSessionIdentity *identity, NpRtMessageStanza *stanza)
{
	NpRtMessageCallback targetCb = (s_defaultHandler != NULL)
					       ? s_defaultHandler->config.onMessageRecv
					       : NULL;

	SceUInt32 nsCount = 0;
	int ret = npRtCollectExactXmlns(stanza->xmlDoc, NULL, &nsCount);
	if (ret < 0)
		return ret;

	const char **nsList = reinterpret_cast<const char **>(
		npRtHeapAlloc(nsCount * sizeof(const char *), NULL));
	if (nsList == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	ret = npRtCollectExactXmlns(stanza->xmlDoc, nsList, NULL);
	if (ret >= 0) {
		NpRtHandlerNode *matched = findHandlerForNamespaces(nsList, nsCount);
		if (matched != NULL)
			targetCb = matched->config.onMessageRecv;
		if (targetCb != NULL)
			targetCb(identity, stanza);
		ret = 0;
	}

	npRtHeapFree(nsList, NULL);
	return ret;
}

/* 0x81310690 */
static int dispatchIncomingPresence(const NpRtSessionIdentity *identity,
				    NpRtPresenceStanza *stanza)
{
	NpRtPresenceCallback targetCb = (s_defaultHandler != NULL)
						? s_defaultHandler->config.onPresenceRecv
						: NULL;

	SceUInt32 nsCount = 0;
	int ret = npRtCollectExactXmlns(stanza->xmlDoc, NULL, &nsCount);
	if (ret < 0)
		return ret;

	const char **nsList = reinterpret_cast<const char **>(
		npRtHeapAlloc(nsCount * sizeof(const char *), NULL));
	if (nsList == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	ret = npRtCollectExactXmlns(stanza->xmlDoc, nsList, NULL);
	if (ret >= 0) {
		NpRtHandlerNode *matched = findHandlerForNamespaces(nsList, nsCount);
		if (matched != NULL)
			targetCb = matched->config.onPresenceRecv;
		if (targetCb != NULL)
			targetCb(identity, stanza);
		ret = 0;
	}

	npRtHeapFree(nsList, NULL);
	return ret;
}

/* 0x8131079A */
static int handlerRegistryInit(void)
{
	if (s_handlerListInit != 0)
		return NP_RT_CORE_ERR_ALREADY_INIT;
	s_handlerHead = NULL;
	s_handlerTail = NULL;
	s_handlerCount = 0;
	s_nextHandlerId = 0;
	s_handlerListInit = 1;
	return 0;
}

static void unlinkAndFreeHandler(NpRtHandlerNode *node)
{
	if (node->next != NULL)
		node->next->prev = node->prev;
	else
		s_handlerTail = node->prev;

	if (node->prev != NULL)
		node->prev->next = node->next;
	else
		s_handlerHead = node->next;

	node->next = NULL;
	node->prev = NULL;
	npRtHeapFree(node, NULL);
	--s_handlerCount;
}

/* 0x813107C4 */
static int handlerRegistryTerm(void)
{
	if (s_handlerListInit == 0)
		return NP_RT_CORE_ERR_NOT_INITIALIZED;

	while (s_handlerHead != NULL)
		unlinkAndFreeHandler(s_handlerHead);

	s_handlerCount = 0;
	s_handlerListInit = 0;
	return 0;
}

/* 0x8131081A */
static int registerHandlerInternal(const NpRtHandlerConfig *config, NpRtHandlerNode **outNode)
{
	if (static_cast<SceUInt32>(s_handlerCount) >= kMaxHandlers)
		return NP_RT_CORE_ERR_HANDLER_FULL;

	if (config->namespaces != NULL) {
		for (const char *const *ns = config->namespaces; *ns != NULL; ++ns) {
			SceSize nsLen = sceClibStrnlen(*ns, 0x7FFFFFFFu);
			for (NpRtHandlerNode *cur = s_handlerHead; cur != NULL; cur = cur->next) {
				if (cur->config.namespaces == NULL)
					continue;
				for (const char *const *existing = cur->config.namespaces;
				     *existing != NULL; ++existing) {
					if (sceClibStrncmp(*ns, *existing, nsLen) == 0)
						return NP_RT_CORE_ERR_NS_CONFLICT;
				}
			}
		}
	}

	NpRtHandlerNode *node = reinterpret_cast<NpRtHandlerNode *>(
		npRtHeapAlloc(sizeof(NpRtHandlerNode), NULL));
	if (node == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(node, 0, sizeof(*node));
	sceClibMemcpy_safe(&node->config, config, sizeof(*config));
	++s_handlerCount;

	++s_nextHandlerId;
	if (s_nextHandlerId == 0)
		s_nextHandlerId = 1;
	node->handlerId = s_nextHandlerId;

	node->next = NULL;
	node->prev = s_handlerTail;
	if (s_handlerTail != NULL)
		s_handlerTail->next = node;
	if (s_handlerHead == NULL)
		s_handlerHead = node;
	s_handlerTail = node;

	*outNode = node;
	return 0;
}

/* --------------------------------------------------------------------------
 * IQ Transaction Queue (0x813116E8..0x81311D23)
 * -------------------------------------------------------------------------- */
/* 0x813116E8 */
static int sessionTxWakeHandler(NpRtTxQueue *queue, void *arg)
{
	(void)queue;
	NpRtSession *session = reinterpret_cast<NpRtSession *>(arg);
	return pipeSendAbort(&session->msgPipeId);
}

/* 0x813116F4 */
static void txQueueInit(NpRtTxQueue *queue, SceKernelLwMutexWork *mutex,
			NpRtTxWakeCallback onWake, void *wakeArg)
{
	queue->mutex = mutex;
	queue->onWake = onWake;
	queue->wakeArg = wakeArg;
	queue->activeCount = 0;
	queue->pendingHead = NULL;
	queue->pendingTail = NULL;
	queue->expiredHead = NULL;
	queue->expiredTail = NULL;
}

/* 0x8131170A */
static void txQueueUnlinkNode(NpRtTxQueue *queue, NpRtTxNode *node)
{
	if (node->listState == NP_RT_TX_PENDING) {
		if (node->next != NULL)
			node->next->prev = node->prev;
		else
			queue->pendingTail = node->prev;

		if (node->prev != NULL)
			node->prev->next = node->next;
		else
			queue->pendingHead = node->next;

		node->listState = NP_RT_TX_UNLISTED;
		node->next = NULL;
		node->prev = NULL;
	} else if (node->listState == NP_RT_TX_EXPIRED) {
		if (node->next != NULL)
			node->next->prev = node->prev;
		else
			queue->expiredTail = node->prev;

		if (node->prev != NULL)
			node->prev->next = node->next;
		else
			queue->expiredHead = node->next;

		node->listState = NP_RT_TX_UNLISTED;
		node->next = NULL;
		node->prev = NULL;
	}
}

static void txQueueAppendExpiredLocked(NpRtTxQueue *queue, NpRtTxNode *node, int errCode)
{
	txQueueUnlinkNode(queue, node);
	node->next = NULL;
	node->prev = queue->expiredTail;
	if (queue->expiredTail != NULL)
		queue->expiredTail->next = node;
	if (queue->expiredHead == NULL)
		queue->expiredHead = node;
	queue->expiredTail = node;
	node->errorCode = errCode;
	node->listState = NP_RT_TX_EXPIRED;
}

/* 0x8131175E */
static void txQueueTerm(NpRtTxQueue *queue)
{
	while (queue->pendingHead != NULL) {
		NpRtTxNode *node = queue->pendingHead;
		txQueueUnlinkNode(queue, node);
		npRtCalloutCancel(&node->timer);
		npRtHeapFree(node, NULL);
		--queue->activeCount;
	}
	while (queue->expiredHead != NULL) {
		NpRtTxNode *node = queue->expiredHead;
		txQueueUnlinkNode(queue, node);
		npRtCalloutCancel(&node->timer);
		npRtHeapFree(node, NULL);
		--queue->activeCount;
	}
}

/* 0x81311CC0 */
static void txNodeTimeoutCallback(void *arg)
{
	NpRtTxNode *node = reinterpret_cast<NpRtTxNode *>(arg);
	NpRtTxQueue *queue = node->ownerQueue;

	if (sceKernelLockLwMutex(queue->mutex, 1, NULL) < 0)
		return;

	txQueueAppendExpiredLocked(queue, node, NP_RT_CORE_ERR_TIMEOUT);
	sceKernelUnlockLwMutex(queue->mutex, 1);

	if (queue->onWake != NULL)
		queue->onWake(queue, queue->wakeArg);
}

/* 0x813117B6 */
static int txQueueAllocNode(NpRtTxQueue *queue, int handlerId, int transactionId,
			    SceUInt64 timeoutUsec, NpRtIqResponseCallback callback,
			    void *userArg, NpRtTxNode **outNode)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpRtTxNode *node = reinterpret_cast<NpRtTxNode *>(
		npRtHeapAlloc(sizeof(NpRtTxNode), NULL));
	if (node == NULL) {
		sceKernelUnlockLwMutex(queue->mutex, 1);
		return NP_RT_CORE_ERR_NO_MEMORY;
	}

	sceClibMemset(node, 0, sizeof(*node));
	npRtCalloutInit(&node->timer);
	node->listState = NP_RT_TX_UNLISTED;
	node->next = NULL;
	node->prev = NULL;
	++queue->activeCount;

	node->ownerQueue = queue;
	node->handlerId = handlerId;
	node->transactionId = transactionId;
	node->callback = callback;
	node->userArg = userArg;
	node->timeoutUsec = timeoutUsec;
	sceClibSnprintf(node->stanzaId, sizeof(node->stanzaId), "%s%x", "psp2-", transactionId);

	*outNode = node;
	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x813118BC */
static int txQueueFreeNode(NpRtTxQueue *queue, NpRtTxNode *node)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	txQueueUnlinkNode(queue, node);
	npRtCalloutCancel(&node->timer);
	npRtHeapFree(node, NULL);
	--queue->activeCount;

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81311910 */
static int txQueueActivateNode(NpRtTxQueue *queue, NpRtTxNode *node)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	ret = SceNpCommon_55F7FCF7(&node->sentTimeUsec);
	if (ret >= 0) {
		node->next = NULL;
		node->prev = queue->pendingTail;
		if (queue->pendingTail != NULL)
			queue->pendingTail->next = node;
		if (queue->pendingHead == NULL)
			queue->pendingHead = node;
		queue->pendingTail = node;
		node->listState = NP_RT_TX_PENDING;

		SceUInt64 delay = (node->timeoutUsec != 0) ? node->timeoutUsec
							   : kDefaultTxTimerUsec;
		ret = npRtCalloutArm(&node->timer, delay, txNodeTimeoutCallback, node);
		if (ret >= 0)
			ret = 0;
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return ret;
}

/* 0x813119BC */
static int txQueueFailAllPending(NpRtTxQueue *queue, int errCode)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpRtTxNode *cur = queue->pendingHead;
	while (cur != NULL) {
		NpRtTxNode *next = cur->next;
		if (errCode < 0)
			txQueueAppendExpiredLocked(queue, cur, errCode);
		cur = next;
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81311A28 */
static int txQueueFailById(NpRtTxQueue *queue, int transactionId, int errCode)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (NpRtTxNode *cur = queue->pendingHead; cur != NULL; cur = cur->next) {
		if (cur->transactionId == transactionId) {
			if (errCode < 0)
				txQueueAppendExpiredLocked(queue, cur, errCode);
			break;
		}
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81311AA0 */
static int txQueuePeekExpired(NpRtTxQueue *queue, NpRtTxNode **outNode)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	*outNode = queue->expiredHead;
	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81311ADA */
static int txQueueFindByTxId(NpRtTxQueue *queue, int transactionId, NpRtTxNode **outNode)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpRtTxNode *found = NULL;
	for (NpRtTxNode *cur = queue->pendingHead; cur != NULL; cur = cur->next) {
		if (cur->transactionId == transactionId) {
			found = cur;
			break;
		}
	}

	*outNode = found;
	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81311B1E */
static int txQueueFindByStanzaId(NpRtTxQueue *queue, const char *stanzaId, NpRtTxNode **outNode)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpRtTxNode *found = NULL;
	for (NpRtTxNode *cur = queue->pendingHead; cur != NULL; cur = cur->next) {
		SceSize idLen = sceClibStrnlen(cur->stanzaId, 0x7FFFFFFFu);
		if (sceClibStrncmp(cur->stanzaId, stanzaId, idLen + 1) == 0) {
			found = cur;
			break;
		}
	}

	*outNode = found;
	sceKernelUnlockLwMutex(queue->mutex, 1);
	return 0;
}

/* 0x81311B8C */
static int txQueueSweepExpired(NpRtTxQueue *queue)
{
	int ret = sceKernelLockLwMutex(queue->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	SceUInt64 now = 0;
	ret = SceNpCommon_55F7FCF7(&now);
	if (ret >= 0) {
		NpRtTxNode *cur = queue->pendingHead;
		while (cur != NULL) {
			NpRtTxNode *next = cur->next;
			SceUInt64 limit = kDefaultTxExpireUsec;
			if (cur->timeoutUsec != 0) {
				limit = (cur->timeoutUsec > kMaxTxExpireUsec)
						? kMaxTxExpireUsec
						: cur->timeoutUsec;
			}
			if (now - cur->sentTimeUsec >= limit)
				txQueueAppendExpiredLocked(queue, cur, NP_RT_CORE_ERR_TIMEOUT);
			cur = next;
		}
		ret = 0;
	}

	sceKernelUnlockLwMutex(queue->mutex, 1);
	return ret;
}

/* --------------------------------------------------------------------------
 * Session MsgPipe & Command Dispatch Helpers (0x81311D24..0x81312637)
 * -------------------------------------------------------------------------- */
/* 0x81311D24 */
static int pipeCreate(SceUID *outPipeId)
{
	SceUID uid = sceKernelCreateMsgPipe("SceNpSession", 64, 12, kPipeBufferSize, NULL);
	if (uid < 0)
		return uid;
	*outPipeId = uid;
	return 0;
}

/* 0x81311D54 */
static int pipeSend(SceUID *pipeId, const NpRtPipeCommand *cmd, SceUInt32 *timeoutUsec)
{
	SceSize sentSize = 0;
	int ret = sceKernelSendMsgPipe(*pipeId, const_cast<NpRtPipeCommand *>(cmd),
				       sizeof(*cmd), 1, &sentSize, timeoutUsec);
	if (ret == kKernelErrPipeTimeout)
		return NP_RT_CORE_ERR_PIPE_TIMEOUT;
	if (ret >= 0)
		return (sentSize == sizeof(*cmd)) ? 0 : NP_RT_CORE_ERR_INTERNAL;
	return ret;
}

/* 0x81311DB4 */
static int pipeTryReceive(SceUID *pipeId, NpRtPipeCommand *outCmd)
{
	SceSize recvSize = 0;
	int ret = sceKernelTryReceiveMsgPipe(*pipeId, outCmd, sizeof(*outCmd), 1, &recvSize);
	if (ret == kKernelErrPipeEmpty) {
		outCmd->commandId = NP_RT_CMD_NONE;
		return 0;
	}
	if (ret >= 0) {
		if (recvSize == sizeof(*outCmd))
			return 0;
		ret = NP_RT_CORE_ERR_INTERNAL;
	}
	outCmd->commandId = NP_RT_CMD_NONE;
	return ret;
}

/* 0x81311E1C */
static int pipeReceive(SceUID *pipeId, NpRtPipeCommand *outCmd, SceUInt32 *timeoutUsec)
{
	SceSize recvSize = 0;
	int ret = sceKernelReceiveMsgPipe(*pipeId, outCmd, sizeof(*outCmd), 1,
					  &recvSize, timeoutUsec);
	if (ret == kKernelErrPipeTimeout) {
		ret = NP_RT_CORE_ERR_PIPE_TIMEOUT;
	} else if (ret >= 0) {
		if (recvSize == sizeof(*outCmd))
			return 0;
		ret = NP_RT_CORE_ERR_INTERNAL;
	}
	outCmd->commandId = NP_RT_CMD_NONE;
	return ret;
}

/* 0x81311E86 */
static int pipeSendAbort(SceUID *pipeId)
{
	NpRtPipeCommand cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.commandId = NP_RT_CMD_ABORT;
	SceUInt32 timeout = kPipeSendTimeoutUsec;
	return pipeSend(pipeId, &cmd, &timeout);
}

/* 0x813123E8 */
static int sessionPostCommand(NpRtSession *session, const SceUInt32 *allowedStateMask,
			      const NpRtPipeCommand *cmd)
{
	int ret = sceKernelLockLwMutex(&session->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (((1u << session->state) & *allowedStateMask) == 0) {
		sceKernelUnlockLwMutex(&session->mutex, 1);
		return NP_RT_CORE_ERR_BAD_STATE;
	}

	if ((session->sessionFlags & NP_RT_SFLAG_LINKED) == 0 ||
	    (session->sessionFlags & NP_RT_SFLAG_CLOSING) != 0) {
		sceKernelUnlockLwMutex(&session->mutex, 1);
		return NP_RT_CORE_ERR_SESSION_NOT_FOUND;
	}

	sceKernelUnlockLwMutex(&session->mutex, 1);
	SceUInt32 timeout = kPipeSendTimeoutUsec;
	ret = pipeSend(&session->msgPipeId, cmd, &timeout);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81311EF8 */
static int sessionQueueIq(NpRtSession *session, int handlerId, int transactionId,
			  NpRtStanza *stanza, SceUInt64 timeoutUsec,
			  NpRtIqResponseCallback callback, void *userArg)
{
	NpRtTxNode *txNode = NULL;
	int ret = txQueueAllocNode(&session->txQueue, handlerId, transactionId,
				   timeoutUsec, callback, userArg, &txNode);
	if (ret < 0)
		return ret;

	NpRtStanza *retained = reinterpret_cast<NpRtStanza *>(npRtAddRefStanza(stanza));
	NpRtStanzaInit tempHeader;
	tempHeader.stanzaId = retained->stanzaId;
	ret = npRtStanzaHeaderSetId(&tempHeader, txNode->stanzaId);
	if (ret >= 0) {
		retained->stanzaId = const_cast<char *>(tempHeader.stanzaId);

		NpRtPipeCommand cmd;
		sceClibMemset(&cmd, 0, sizeof(cmd));
		cmd.commandId = NP_RT_CMD_SEND_IQ;
		cmd.body.sendIq.stanza = retained;
		cmd.body.sendIq.handlerId = handlerId;
		cmd.body.sendIq.transactionId = txNode->transactionId;

		ret = txQueueActivateNode(&session->txQueue, txNode);
		if (ret >= 0) {
			SceUInt32 mask = 1u << NP_RT_STATE_RUNNING;
			ret = sessionPostCommand(session, &mask, &cmd);
			if (ret >= 0)
				return 0;
		}
	}

	npRtReleaseStanza(retained);
	txQueueFreeNode(&session->txQueue, txNode);
	return ret;
}

/* 0x81311FC4 */
static int sessionQueueMessage(NpRtSession *session, int handlerId, int transactionId,
			       NpRtMessageStanza *stanza, NpRtSendDoneCallback callback,
			       void *userArg)
{
	NpRtMessageStanza *retained =
		reinterpret_cast<NpRtMessageStanza *>(npRtAddRefStanza(stanza));

	NpRtPipeCommand cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.commandId = NP_RT_CMD_SEND_MESSAGE;
	cmd.body.sendStanza.stanza = retained;
	cmd.body.sendStanza.handlerId = handlerId;
	cmd.body.sendStanza.transactionId = transactionId;
	cmd.body.sendStanza.callback = callback;
	cmd.body.sendStanza.userArg = userArg;

	SceUInt32 mask = 1u << NP_RT_STATE_RUNNING;
	int ret = sessionPostCommand(session, &mask, &cmd);
	if (ret >= 0)
		return 0;

	npRtReleaseMessageStanza(retained);
	return ret;
}

/* 0x81312044 */
static int sessionQueuePresence(NpRtSession *session, int handlerId, int transactionId,
				NpRtPresenceStanza *stanza, NpRtSendDoneCallback callback,
				void *userArg)
{
	NpRtPresenceStanza *retained =
		reinterpret_cast<NpRtPresenceStanza *>(npRtAddRefStanza(stanza));

	NpRtPipeCommand cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.commandId = NP_RT_CMD_SEND_PRESENCE;
	cmd.body.sendStanza.stanza = retained;
	cmd.body.sendStanza.handlerId = handlerId;
	cmd.body.sendStanza.transactionId = transactionId;
	cmd.body.sendStanza.callback = callback;
	cmd.body.sendStanza.userArg = userArg;

	SceUInt32 mask = 1u << NP_RT_STATE_RUNNING;
	int ret = sessionPostCommand(session, &mask, &cmd);
	if (ret >= 0)
		return 0;

	npRtReleasePresenceStanza(retained);
	return ret;
}

/* 0x813120C4 */
static int sessionQueueIqError(NpRtSession *session, int handlerId,
			       const NpXmppErrorStanza *errStanza)
{
	(void)handlerId;
	char *xmlStr = NULL;
	int ret = npXmppSerializeIqError(errStanza, &xmlStr);
	if (ret >= 0) {
		NpRtPipeCommand cmd;
		sceClibMemset(&cmd, 0, sizeof(cmd));
		cmd.commandId = NP_RT_CMD_SEND_RAW_XML;
		cmd.body.sendRaw.xmlStr = xmlStr;
		cmd.body.sendRaw.xmlLen = sceClibStrnlen(xmlStr, 0x7FFFFFFFu);

		SceUInt32 mask = 1u << NP_RT_STATE_RUNNING;
		ret = sessionPostCommand(session, &mask, &cmd);
		if (ret >= 0)
			return 0;
	}
	if (xmlStr != NULL)
		ScebXCe_EC560647(xmlStr);
	return ret;
}

/* 0x8131213E */
static int sessionQueueIqResult(NpRtSession *session, int handlerId, NpRtStanza *stanza)
{
	(void)handlerId;
	char *xmlStr = NULL;
	int ret = npXmppSerializeIq(stanza, &xmlStr);
	if (ret >= 0) {
		NpRtPipeCommand cmd;
		sceClibMemset(&cmd, 0, sizeof(cmd));
		cmd.commandId = NP_RT_CMD_SEND_RAW_XML;
		cmd.body.sendRaw.xmlStr = xmlStr;
		cmd.body.sendRaw.xmlLen = sceClibStrnlen(xmlStr, 0x7FFFFFFFu);

		SceUInt32 mask = 1u << NP_RT_STATE_RUNNING;
		ret = sessionPostCommand(session, &mask, &cmd);
		if (ret >= 0)
			return 0;
	}
	if (xmlStr != NULL)
		ScebXCe_EC560647(xmlStr);
	return ret;
}

/* 0x813121B8 */
static int sessionDispatchExpiredTx(NpRtSession *session)
{
	int ret = txQueueSweepExpired(&session->txQueue);
	if (ret < 0)
		return ret;

	for (;;) {
		NpRtTxNode *expired = NULL;
		ret = txQueuePeekExpired(&session->txQueue, &expired);
		if (ret < 0)
			return ret;
		if (expired == NULL)
			return 0;

		if (expired->callback != NULL) {
			expired->callback(&session->identity, expired->transactionId,
					  expired->errorCode, NULL, expired->userArg);
		}
		txQueueFreeNode(&session->txQueue, expired);
	}
}

/* 0x81312224 */
static int sessionSendQueuedIq(NpRtSession *session, const NpRtPipeCommand *cmd)
{
	NpRtTxNode *txNode = NULL;
	int ret = txQueueFindByTxId(&session->txQueue, cmd->body.sendIq.transactionId, &txNode);
	if (ret < 0) {
		if (txNode != NULL)
			txQueueFailById(&session->txQueue, txNode->transactionId, ret);
		return ret;
	}
	if (txNode == NULL)
		return 0;

	char *xmlStr = NULL;
	ret = npXmppSerializeIq(cmd->body.sendIq.stanza, &xmlStr);
	if (ret >= 0) {
		SceSize xmlLen = sceClibStrnlen(xmlStr, 0x7FFFFFFFu);
		ret = npRtXmppClientSend(&session->xmppClient, xmlStr, xmlLen);
		if (ret >= 0)
			ret = 0;
	}

	if (ret < 0)
		txQueueFailById(&session->txQueue, txNode->transactionId, ret);

	if (xmlStr != NULL)
		ScebXCe_EC560647(xmlStr);
	return ret;
}

/* 0x813122C2 */
static int sessionSendQueuedMessage(NpRtSession *session, const NpRtPipeCommand *cmd)
{
	char *xmlStr = NULL;
	int ret = npXmppSerializeMessage(
		reinterpret_cast<const NpRtMessageStanza *>(cmd->body.sendStanza.stanza), &xmlStr);
	if (ret >= 0) {
		SceSize xmlLen = sceClibStrnlen(xmlStr, 0x7FFFFFFFu);
		ret = npRtXmppClientSend(&session->xmppClient, xmlStr, xmlLen);
		if (ret >= 0)
			ret = 0;
	}

	if (xmlStr != NULL)
		ScebXCe_EC560647(xmlStr);

	if (cmd->body.sendStanza.callback != NULL) {
		cmd->body.sendStanza.callback(&session->identity,
					      cmd->body.sendStanza.transactionId,
					      ret, cmd->body.sendStanza.userArg);
	}
	return ret;
}

/* 0x81312348 */
static int sessionSendQueuedPresence(NpRtSession *session, const NpRtPipeCommand *cmd)
{
	char *xmlStr = NULL;
	int ret = npXmppSerializePresence(
		reinterpret_cast<const NpRtPresenceStanza *>(cmd->body.sendStanza.stanza), &xmlStr);
	if (ret >= 0) {
		SceSize xmlLen = sceClibStrnlen(xmlStr, 0x7FFFFFFFu);
		ret = npRtXmppClientSend(&session->xmppClient, xmlStr, xmlLen);
		if (ret >= 0)
			ret = 0;
	}

	if (xmlStr != NULL)
		ScebXCe_EC560647(xmlStr);

	if (cmd->body.sendStanza.callback != NULL) {
		cmd->body.sendStanza.callback(&session->identity,
					      cmd->body.sendStanza.transactionId,
					      ret, cmd->body.sendStanza.userArg);
	}
	return ret;
}

/* 0x813123CE */
static int sessionSendQueuedRaw(NpRtSession *session, const NpRtPipeCommand *cmd)
{
	int ret = npRtXmppClientSend(&session->xmppClient, cmd->body.sendRaw.xmlStr,
				     cmd->body.sendRaw.xmlLen);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81312492 */
static int sessionRejectCommand(NpRtSession *session, const NpRtPipeCommand *cmd, int errCode)
{
	switch (cmd->commandId) {
	case NP_RT_CMD_SEND_IQ:
		txQueueFailById(&session->txQueue, cmd->body.sendIq.transactionId, errCode);
		break;
	case NP_RT_CMD_SEND_MESSAGE:
	case NP_RT_CMD_SEND_PRESENCE:
		if (cmd->body.sendStanza.callback != NULL) {
			cmd->body.sendStanza.callback(&session->identity,
						      cmd->body.sendStanza.transactionId,
						      errCode, cmd->body.sendStanza.userArg);
		}
		break;
	default:
		break;
	}
	return 0;
}

/* 0x813124E0 */
static int sessionDrainPipeWithError(NpRtSession *session, int errCode)
{
	NpRtPipeCommand cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.commandId = NP_RT_CMD_NONE;

	int ret = 0;
	for (;;) {
		ret = pipeTryReceive(&session->msgPipeId, &cmd);
		if (ret < 0)
			break;
		if (cmd.commandId == NP_RT_CMD_NONE) {
			ret = 0;
			break;
		}
		ret = sessionRejectCommand(session, &cmd, errCode);
		if (ret < 0)
			break;
		resetPipeCommand(&cmd);
	}

	resetPipeCommand(&cmd);
	return ret;
}

/* 0x8131256C */
static int sessionReceiveCommand(NpRtSession *session, NpRtPipeCommand *outCmd,
				 SceUInt32 *timeoutUsec)
{
	int ret = pipeReceive(&session->msgPipeId, outCmd, timeoutUsec);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8131258A */
static int sessionReceiveCommandWithKeepAlive(NpRtSession *session, NpRtPipeCommand *outCmd)
{
	for (;;) {
		SceUInt8 kaEnabled = 0;
		SceUInt32 kaRemainingUsec = 0;
		int ret = npRtXmppClientCheckKeepAlive(&session->xmppClient, &kaEnabled,
						       &kaRemainingUsec);
		if (ret < 0)
			return ret;

		if (kaEnabled != 0) {
			if (kaRemainingUsec != 0) {
				ret = pipeReceive(&session->msgPipeId, outCmd, &kaRemainingUsec);
				if (ret != NP_RT_CORE_ERR_PIPE_TIMEOUT && ret >= 0)
					ret = 0;
			} else {
				ret = npRtXmppClientSendKeepAlive(&session->xmppClient);
				if (ret < 0)
					return ret;
				continue;
			}
		} else {
			ret = pipeReceive(&session->msgPipeId, outCmd, NULL);
			if (ret != NP_RT_CORE_ERR_PIPE_TIMEOUT && ret >= 0)
				ret = 0;
		}

		if (ret != NP_RT_CORE_ERR_PIPE_TIMEOUT)
			return (ret >= 0) ? 0 : ret;
	}
}

/* --------------------------------------------------------------------------
 * Worker Threads: SceNpCoreMain & SceNpCoreRecv (0x81313DE4..0x8131403B)
 * -------------------------------------------------------------------------- */
/* 0x81313E32 */
static int coreMainThreadEntry(SceSize argSize, void *argBlock)
{
	(void)argSize;
	NpRtSession *session = *reinterpret_cast<NpRtSession **>(argBlock);
	SceUID tid = sceKernelGetThreadId();
	if (SceNetInternal_9DBD2F60(tid, 1, 0) >= 0)
		runSessionStateMachine(session);
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

/* 0x81313DE4 */
static int coreMainThreadCreate(SceUID *outThreadId, const NpPollingThreadParam *param)
{
	SceUID tid = sceKernelCreateThread("SceNpCoreMain", coreMainThreadEntry,
					   param->priority, param->stackSize, 0,
					   param->cpuAffinityMask, NULL);
	if (tid < 0) {
		if (tid != -1)
			sceKernelDeleteThread(tid);
		return tid;
	}
	*outThreadId = tid;
	return 0;
}

/* 0x81313F0E */
static int coreRecvThreadEntry(SceSize argSize, void *argBlock)
{
	(void)argSize;
	NpRtSession *session = *reinterpret_cast<NpRtSession **>(argBlock);
	NpRtAnyStanza anyStanza;
	anyStanza.kind = NP_RT_STANZA_KIND_NONE;
	anyStanza.stanza = NULL;

	SceUID tid = sceKernelGetThreadId();
	int status = SceNetInternal_9DBD2F60(tid, 1, 0);
	if (status >= 0) {
		for (;;) {
			if (session->recvThreadPtr->stopRequested != 0) {
				status = NP_RT_CORE_ERR_ABORTED;
				break;
			}
			status = npRtXmppClientRecvStanza(&session->xmppClient, &anyStanza);
			if (status < 0)
				break;
			if (anyStanza.kind == NP_RT_STANZA_KIND_STREAM_END) {
				status = 0;
				break;
			}

			NpRtPipeCommand recvCmd;
			sceClibMemset(&recvCmd, 0, sizeof(recvCmd));
			recvCmd.commandId = NP_RT_CMD_RECV_STANZA;
			recvCmd.body.recvStanza = anyStanza;
			status = pipeSend(&session->msgPipeId, &recvCmd, NULL);
			if (status < 0)
				break;

			anyStanza.kind = NP_RT_STANZA_KIND_NONE;
			anyStanza.stanza = NULL;
		}
	}

	npRtReleaseAnyStanza(&anyStanza);
	if (status == 0 && session->aborted != 0)
		status = NP_RT_CORE_ERR_ABORTED;

	NpRtPipeCommand closedCmd;
	sceClibMemset(&closedCmd, 0, sizeof(closedCmd));
	closedCmd.commandId = NP_RT_CMD_RECV_CLOSED;
	closedCmd.body.closeResult = status;
	pipeSend(&session->msgPipeId, &closedCmd, NULL);

	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

/* 0x81313E58 */
static int coreRecvThreadCreate(NpRtRecvThread *recvThread, const NpPollingThreadParam *param)
{
	SceUID tid = sceKernelCreateThread("SceNpCoreRecv", coreRecvThreadEntry,
					   param->priority, param->stackSize, 0,
					   param->cpuAffinityMask, NULL);
	if (tid < 0) {
		if (tid != -1)
			sceKernelDeleteThread(tid);
		return tid;
	}
	recvThread->threadId = tid;
	recvThread->stopRequested = 0;
	return 0;
}

/* 0x81313EBE */
static int coreRecvThreadStart(NpRtRecvThread *recvThread, NpRtSession *session)
{
	recvThread->stopRequested = 0;
	int ret = sceKernelStartThread(recvThread->threadId, sizeof(session), &session);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81313EFA */
static int coreRecvThreadRequestStop(NpRtRecvThread *recvThread, NpRtSession *session)
{
	recvThread->stopRequested = 1;
	npRtXmppClientAbort(&session->xmppClient, 5);
	return 0;
}

/* 0x81313FE4 / 0x81314034 */
static void resetPipeCommand(NpRtPipeCommand *cmd)
{
	switch (cmd->commandId) {
	case NP_RT_CMD_SEND_IQ:
		npRtReleaseStanza(cmd->body.sendIq.stanza);
		break;
	case NP_RT_CMD_SEND_MESSAGE:
		npRtReleaseMessageStanza(
			reinterpret_cast<NpRtMessageStanza *>(cmd->body.sendStanza.stanza));
		break;
	case NP_RT_CMD_SEND_PRESENCE:
		npRtReleasePresenceStanza(
			reinterpret_cast<NpRtPresenceStanza *>(cmd->body.sendStanza.stanza));
		break;
	case NP_RT_CMD_SEND_RAW_XML:
		if (cmd->body.sendRaw.xmlStr != NULL)
			ScebXCe_EC560647(cmd->body.sendRaw.xmlStr);
		break;
	case NP_RT_CMD_RECV_STANZA:
		npRtReleaseAnyStanza(&cmd->body.recvStanza);
		break;
	default:
		break;
	}
	sceClibMemset(cmd, 0, sizeof(*cmd));
	cmd->commandId = NP_RT_CMD_NONE;
}

/* --------------------------------------------------------------------------
 * SASL Authentication & Incoming Stanza Dispatcher (0x8131616C..0x8131696F)
 * -------------------------------------------------------------------------- */
/* 0x8131616C */
static int sendSaslAuthStanza(NpRtSession *session, const SceUInt8 *ticketBuf,
			      SceSize ticketSize, const char *reconnectSid)
{
	BxceSerializer serializer;
	ScebXCe_8EA5A077(&serializer);

	char *encodedTicket = NULL;
	char *jidBuf = NULL;
	char *authXml = NULL;
	int status = NP_RT_CORE_ERR_NO_MEMORY;

	BxceDocument *doc = reinterpret_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(sizeof(BxceDocument)));
	if (doc != NULL) {
		if (ScebXCe_FAB50608(doc, 1) == 0) {
			BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
			if (attrs != NULL) {
				if (ScebXCe_E7C8B20D(attrs) == 0) {
					status = NP_RT_CORE_ERR_SASL_BUILD;
					if (ScebXCe_3DD761CF(attrs, "mechanism",
							     "NP-TICKET-AUTH-COMPACT") >= 0 &&
					    ScebXCe_3DD761CF(
						    attrs, "xmlns",
						    "urn:ietf:params:xml:ns:xmpp-sasl") >= 0) {
						BxceNodeId root = ScebXCe_75E1B095(doc);
						BxceNodeId authNode = ScebXCe_AC0E4F67(
							doc, root, "auth", attrs, NULL, 0);
						if (authNode != 0) {
							SceSize b64Cap =
								4u * ((ticketSize + 2u) / 3u) + 1u;
							encodedTicket = reinterpret_cast<char *>(
								npRtHeapAlloc(b64Cap, NULL));
							if (encodedTicket == NULL) {
								status = NP_RT_CORE_ERR_NO_MEMORY;
							} else {
								int b64Len =
									sceNpCommonBase64Encode(
										ticketBuf,
										encodedTicket,
										ticketSize);
								if (b64Len < 0) {
									status = b64Len;
								} else {
									encodedTicket[b64Len] =
										'\0';
									SceSize encLen =
										sceClibStrnlen(
											encodedTicket,
											0x7FFFFFFFu);
									if (ScebXCe_AC0E4F67(
										    doc, authNode,
										    "ticket", NULL,
										    encodedTicket,
										    encLen) != 0) {
										npRtHeapFree(
											encodedTicket,
											NULL);
										encodedTicket =
											NULL;
										jidBuf = reinterpret_cast<
											char *>(
											npRtHeapAlloc(
												1024,
												NULL));
										if (jidBuf ==
										    NULL) {
											status =
												NP_RT_CORE_ERR_NO_MEMORY;
										} else {
											sceClibSnprintf(
												jidBuf,
												1024,
												"%s@%s",
												session->identity
													.onlineId
													.data,
												session->identity
													.serverConfig
													.domain);
											SceSize jidLen = sceClibStrnlen(
												jidBuf,
												0x7FFFFFFFu);
											SceSize resLen = sceClibStrnlen(
												"psp2",
												0x7FFFFFFFu);
											bool nodesOk =
												ScebXCe_AC0E4F67(
													doc,
													authNode,
													"jid",
													NULL,
													jidBuf,
													jidLen) !=
													0 &&
												ScebXCe_AC0E4F67(
													doc,
													authNode,
													"resource",
													NULL,
													"psp2",
													resLen) !=
													0;
											if (nodesOk &&
											    reconnectSid !=
												    NULL) {
												SceSize sidLen = sceClibStrnlen(
													reconnectSid,
													0x7FFFFFFFu);
												nodesOk =
													ScebXCe_AC0E4F67(
														doc,
														authNode,
														"sid",
														NULL,
														reconnectSid,
														sidLen) !=
													0;
											}
											if (nodesOk) {
												npRtHeapFree(
													jidBuf,
													NULL);
												jidBuf =
													NULL;
												if (ScebXCe_D7FE96A2(
													    &serializer,
													    doc,
													    &authXml,
													    NULL,
													    0) ==
												    0) {
													SceSize xmlLen = sceClibStrnlen(
														authXml,
														0x7FFFFFFFu);
													status = npRtXmppClientSend(
														&session->xmppClient,
														authXml,
														xmlLen);
													if (status >=
													    0)
														status = 0;
												}
											}
										}
									}
								}
							}
						}
					}
				}
				ScebXCe_9C2F9897(attrs);
				sceNpCommonFreeNpServerName(attrs);
			}
		}
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}

	if (encodedTicket != NULL)
		npRtHeapFree(encodedTicket, NULL);
	if (jidBuf != NULL)
		npRtHeapFree(jidBuf, NULL);
	if (authXml != NULL)
		ScebXCe_EC560647(authXml);
	ScebXCe_D24E80F2();
	return status;
}

/* 0x81316864 */
static void connectTimeoutCallback(void *arg)
{
	NpRtSession *session = reinterpret_cast<NpRtSession *>(arg);
	int lockState = npRtLockLwMutex(&session->mutex);
	npRtXmppClientAbort(&session->xmppClient, 7);
	if (session->accountReqHandle != -1)
		npAccountAbortRequest(session->accountReqHandle);
	npRtUnlockLwMutex(&session->mutex, lockState);
}

static int waitForSaslFeatures(NpRtSession *session)
{
	NpRtAnyStanza anyStanza;
	anyStanza.kind = NP_RT_STANZA_KIND_NONE;
	anyStanza.stanza = NULL;

	int status = 0;
	for (;;) {
		npRtReleaseAnyStanza(&anyStanza);
		status = npRtXmppClientRecvStanza(&session->xmppClient, &anyStanza);
		if (status < 0)
			break;
		if (anyStanza.kind == NP_RT_STANZA_KIND_STREAM_END) {
			status = NP_RT_CORE_ERR_DISCONNECTED;
			break;
		}
		if (anyStanza.kind != NP_RT_STANZA_KIND_STREAM)
			continue;

		BxceDocument *doc = reinterpret_cast<BxceDocument *>(anyStanza.stanza);
		BxceNodeId root = ScebXCe_CED6BECF(doc);
		int isFeatures = 0;
		status = npRtMatchElementName(doc, root, "stream:features", &isFeatures);
		if (status < 0)
			break;
		if (isFeatures == 0)
			continue;

		SceUInt8 mechFlags = 0;
		BxceNodeId child = ScebXCe_FEA6E4A1(doc, root);
		if (child != 0) {
			int isMechs = 0;
			status = npRtMatchElementName(doc, child, "mechanisms", &isMechs);
			if (status >= 0 && isMechs != 0) {
				BxceNodeList *children = ScebXCe_2B9B57AE(doc, child);
				for (SceUInt32 idx = 0;
				     idx < static_cast<SceUInt32>(children->length); ++idx) {
					BxceNodeId mechNode = ScebXCe_6C46D786(children, idx);
					int isMech = 0;
					status = npRtMatchElementName(doc, mechNode, "mechanism",
								      &isMech);
					if (status < 0)
						break;
					if (isMech != 0) {
						BxceNodeId textNode =
							ScebXCe_FEA6E4A1(doc, mechNode);
						const char *mechName =
							ScebXCe_33825EFF(doc, textNode);
						if (mechName != NULL &&
						    sceClibStrncmp(
							    mechName, "NP-TICKET-AUTH-COMPACT",
							    sceClibStrnlen(
								    "NP-TICKET-AUTH-COMPACT",
								    0x7FFFFFFFu)) == 0) {
							mechFlags |= 1u;
						}
					}
				}
				if (status >= 0 && children != NULL) {
					ScebXCe_1F59F0C4(children);
					ScebXCe_EC560647(children);
				}
			}
		}

		if (status >= 0 && (mechFlags & 1u) == 0)
			status = NP_RT_CORE_ERR_NO_SASL_MECH;
		break;
	}

	npRtReleaseAnyStanza(&anyStanza);
	return status;
}

static int fetchAuthTicket(NpRtSession *session, SceUInt8 *ticketBuf, SceSize *ioTicketSize)
{
	int lockState = npRtLockLwMutex(&session->mutex);
	if (lockState < 0)
		return lockState;

	int status = 0;
	if (session->aborted != 0) {
		status = NP_RT_CORE_ERR_ABORTED;
	} else {
		int reqHandle = npAccountCreateRequest();
		if (reqHandle < 0) {
			status = reqHandle;
		} else {
			session->accountReqHandle = reqHandle;
			npRtUnlockLwMutex(&session->mutex, lockState);
			lockState = 0;

			status = npAccountGetTicketForService(reqHandle, ticketBuf, ioTicketSize,
							      kRtTicketServiceId);
			if (status >= 0) {
				lockState = npRtLockLwMutex(&session->mutex);
				if (lockState >= 0) {
					npAccountDestroyRequest(reqHandle);
					session->accountReqHandle = -1;
					npRtUnlockLwMutex(&session->mutex, lockState);
					return 0;
				}
				status = lockState;
			}
			npAccountDestroyRequest(reqHandle);
		}
	}

	session->accountReqHandle = -1;
	npRtUnlockLwMutex(&session->mutex, lockState);
	return status;
}

/* 0x8131646C */
static int sessionConnectAndAuthenticate(NpRtSession *session, const char *reconnectSid)
{
	SceSize ticketSize = kTicketBufSize;
	int status = npRtXmppClientOpen(&session->xmppClient);
	if (status >= 0) {
		status = npRtXmppClientConnect(&session->xmppClient,
					       session->identity.serverConfig.host,
					       session->identity.serverConfig.port,
					       session->identity.serverConfig.domain);
		if (status >= 0) {
			status = npRtCalloutArm(&session->connectTimer, kConnectTimeoutUsec,
						connectTimeoutCallback, session);
			if (status >= 0)
				status = waitForSaslFeatures(session);
		}
	}

	if (status >= 0) {
		SceUInt8 *ticketBuf = reinterpret_cast<SceUInt8 *>(
			npRtHeapAlloc(kTicketBufSize, NULL));
		if (ticketBuf == NULL) {
			status = NP_RT_CORE_ERR_NO_MEMORY;
		} else {
			status = fetchAuthTicket(session, ticketBuf, &ticketSize);
			if (status >= 0) {
				NpRtAnyStanza anyStanza;
				anyStanza.kind = NP_RT_STANZA_KIND_NONE;
				anyStanza.stanza = NULL;

				status = sendSaslAuthStanza(session, ticketBuf, ticketSize,
							    reconnectSid);
				if (status >= 0) {
					for (;;) {
						npRtReleaseAnyStanza(&anyStanza);
						status = npRtXmppClientRecvStanza(
							&session->xmppClient, &anyStanza);
						if (status < 0)
							break;
						if (anyStanza.kind ==
						    NP_RT_STANZA_KIND_STREAM_END) {
							status = NP_RT_CORE_ERR_DISCONNECTED;
							break;
						}
						if (anyStanza.kind != NP_RT_STANZA_KIND_STREAM)
							continue;

						BxceDocument *doc =
							reinterpret_cast<BxceDocument *>(
								anyStanza.stanza);
						BxceNodeId root = ScebXCe_CED6BECF(doc);
						int matched = 0;
						status = npRtMatchElementName(doc, root, "failure",
									      &matched);
						if (status < 0)
							break;
						status = npRtMatchElementName(doc, root, "success",
									      &matched);
						if (status < 0)
							break;
						if (matched == 0)
							continue;

						const char *xmlns = ScebXCe_753D9382(doc, root,
										     "xmlns");
						if (xmlns == NULL ||
						    sceClibStrncmp(
							    xmlns,
							    "urn:ietf:params:xml:ns:xmpp-sasl",
							    sceClibStrnlen(
								    "urn:ietf:params:xml:ns:xmpp-sasl",
								    0x7FFFFFFFu)) != 0) {
							status = NP_RT_CORE_ERR_SASL_BAD_XMLNS;
						} else {
							const char *sid = ScebXCe_753D9382(
								doc, root, "sid");
							if (sid == NULL) {
								status =
									NP_RT_CORE_ERR_SASL_BAD_SID;
							} else {
								SceSize sidLen = sceClibStrnlen(
									sid, 0x7FFFFFFFu);
								if (sidLen < kSessionSidMaxSize) {
									sceClibMemset(
										session->identity
											.sessionSid,
										0,
										kSessionSidMaxSize);
									sceClibStrncpy(
										session->identity
											.sessionSid,
										sid, sidLen);
									status = 0;
								} else {
									status =
										NP_RT_CORE_ERR_SASL_BAD_SID;
								}
							}
						}
						break;
					}
					npRtReleaseAnyStanza(&anyStanza);
				}
			}
			npRtHeapFree(ticketBuf, NULL);
		}
	}

	if (status < 0 && npRtCalloutIsFired(&session->connectTimer) != 0)
		status = NP_RT_CORE_ERR_CONNECT_TIMEOUT;
	npRtCalloutCancel(&session->connectTimer);
	return status;
}

/* 0x81316890 */
static int sessionHandleIncomingStanza(NpRtSession *session, NpRtAnyStanza *anyStanza)
{
	int ret = 0;
	switch (anyStanza->kind) {
	case NP_RT_STANZA_KIND_MESSAGE:
		ret = dispatchIncomingMessage(
			&session->identity,
			reinterpret_cast<NpRtMessageStanza *>(anyStanza->stanza));
		break;
	case NP_RT_STANZA_KIND_PRESENCE:
		ret = dispatchIncomingPresence(
			&session->identity,
			reinterpret_cast<NpRtPresenceStanza *>(anyStanza->stanza));
		break;
	case NP_RT_STANZA_KIND_IQ: {
		NpRtStanza *iq = reinterpret_cast<NpRtStanza *>(anyStanza->stanza);
		if (iq->stanzaType == NP_RT_XMPP_TYPE_ERROR ||
		    iq->stanzaType == NP_RT_XMPP_TYPE_RESULT) {
			NpRtTxNode *txNode = NULL;
			ret = txQueueFindByStanzaId(&session->txQueue, iq->stanzaId, &txNode);
			if (ret < 0)
				return ret;
			if (txNode != NULL) {
				if (txNode->callback != NULL) {
					txNode->callback(&session->identity, txNode->transactionId,
							 0, iq, txNode->userArg);
				}
				txQueueFreeNode(&session->txQueue, txNode);
			}
		} else {
			ret = dispatchIncomingIq(&session->identity, iq);
			if (ret < 0)
				return ret;
		}
		break;
	}
	default:
		break;
	}

	if (ret < 0)
		return ret;
	npRtReleaseAnyStanza(anyStanza);
	return 0;
}

/* --------------------------------------------------------------------------
 * Session State Machine (0x8131403C..0x81314523)
 * -------------------------------------------------------------------------- */
/* 0x8131403C */
static int transitionSessionState(NpRtSession *session, int newState, int resultCode)
{
	int lockState = npRtLockLwMutex(&session->mutex);
	if (lockState < 0)
		return lockState;

	int prevState = session->state;
	session->state = newState;
	npRtUnlockLwMutex(&session->mutex, lockState);

	if (newState == NP_RT_STATE_ERROR || newState == NP_RT_STATE_STOPPED) {
		if (session->recvThreadPtr != NULL) {
			npRtWaitThreadEnd(&session->recvThreadPtr->threadId);
			session->recvThreadPtr = NULL;
		}
		if (npRtXmppClientIsOpen(&session->xmppClient) != 0)
			npRtXmppClientClose(&session->xmppClient);

		int drainErr = NP_RT_CORE_ERR_BAD_STATE;
		if (session->state == NP_RT_STATE_STOPPED)
			drainErr = (resultCode < 0) ? resultCode : NP_RT_CORE_ERR_DISCONNECTED;
		else if (session->state == NP_RT_STATE_ERROR)
			drainErr = resultCode;

		int ret = sessionDrainPipeWithError(session, drainErr);
		if (ret < 0)
			return ret;
		ret = txQueueFailAllPending(&session->txQueue, drainErr);
		if (ret < 0)
			return ret;
		ret = sessionDispatchExpiredTx(session);
		if (ret < 0)
			return ret;
	}

	int eventState = -1;
	switch (newState) {
	case NP_RT_STATE_ERROR:
		if (prevState != -1 && prevState != NP_RT_STATE_ERROR &&
		    prevState != NP_RT_STATE_STOPPED) {
			eventState = NP_RT_EVENT_ERROR;
		}
		break;
	case NP_RT_STATE_CONNECTING:
		eventState = NP_RT_EVENT_CONNECTING;
		break;
	case NP_RT_STATE_RUNNING:
		eventState = NP_RT_EVENT_CONNECTED;
		break;
	case NP_RT_STATE_CLOSING:
		eventState = NP_RT_EVENT_CLOSING;
		break;
	case NP_RT_STATE_STOPPED:
		if (prevState != -1 && prevState != NP_RT_STATE_ERROR &&
		    prevState != NP_RT_STATE_STOPPED) {
			eventState = NP_RT_EVENT_STOPPED;
		}
		break;
	default:
		break;
	}

	if (eventState >= 0)
		dispatchStateChanged(&session->identity, eventState, resultCode);
	return 0;
}

/* 0x81314230: State 1 (IDLE) */
static int stateIdleHandler(void *arg, int *nextState)
{
	NpRtSession *session = reinterpret_cast<NpRtSession *>(arg);
	NpRtPipeCommand cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.commandId = NP_RT_CMD_NONE;

	int ret = sessionReceiveCommand(session, &cmd, NULL);
	if (ret >= 0) {
		if (cmd.commandId == NP_RT_CMD_RECONNECT) {
			sceClibMemcpy_safe(session->identity.sessionSid, cmd.body.reconnectSid,
					   kSessionSidMaxSize);
			*nextState = NP_RT_STATE_CONNECTING;
		} else if (cmd.commandId == NP_RT_CMD_CONNECT) {
			*nextState = NP_RT_STATE_CONNECTING;
		}
		ret = 0;
	}

	resetPipeCommand(&cmd);
	return ret;
}

/* 0x813142C0: State 2 (CONNECTING) */
static int stateConnectingHandler(void *arg, int *nextState)
{
	NpRtSession *session = reinterpret_cast<NpRtSession *>(arg);
	const char *sid = (session->identity.sessionSid[0] != '\0')
				  ? session->identity.sessionSid
				  : NULL;
	int ret = sessionConnectAndAuthenticate(session, sid);
	if (ret < 0)
		return ret;

	ret = coreRecvThreadStart(session->recvThreadPtr, session);
	if (ret < 0)
		return ret;

	*nextState = NP_RT_STATE_RUNNING;
	return 0;
}

/* 0x81314300: State 3 (RUNNING) */
static int stateRunningHandler(void *arg, int *nextState)
{
	NpRtSession *session = reinterpret_cast<NpRtSession *>(arg);
	NpRtPipeCommand cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.commandId = NP_RT_CMD_NONE;

	int ret = sessionReceiveCommandWithKeepAlive(session, &cmd);
	if (ret >= 0) {
		bool checkExpired = false;
		switch (cmd.commandId) {
		case NP_RT_CMD_SEND_IQ:
			ret = sessionSendQueuedIq(session, &cmd);
			checkExpired = (ret >= 0);
			break;
		case NP_RT_CMD_SEND_MESSAGE:
			ret = sessionSendQueuedMessage(session, &cmd);
			checkExpired = (ret >= 0);
			break;
		case NP_RT_CMD_SEND_PRESENCE:
			ret = sessionSendQueuedPresence(session, &cmd);
			checkExpired = (ret >= 0);
			break;
		case NP_RT_CMD_SEND_RAW_XML:
			ret = sessionSendQueuedRaw(session, &cmd);
			checkExpired = (ret >= 0);
			break;
		case NP_RT_CMD_DISCONNECT:
			ret = npRtXmppClientSendStreamClose(&session->xmppClient);
			if (ret >= 0) {
				ret = coreRecvThreadRequestStop(session->recvThreadPtr, session);
				if (ret >= 0) {
					*nextState = NP_RT_STATE_CLOSING;
					checkExpired = true;
				}
			}
			break;
		case NP_RT_CMD_RECV_STANZA:
			if (cmd.body.recvStanza.kind == NP_RT_STANZA_KIND_STREAM_END) {
				ret = NP_RT_CORE_ERR_DISCONNECTED;
			} else {
				ret = sessionHandleIncomingStanza(session, &cmd.body.recvStanza);
				checkExpired = (ret >= 0);
			}
			break;
		case NP_RT_CMD_RECV_CLOSED:
			ret = (cmd.body.closeResult < 0) ? cmd.body.closeResult
							 : NP_RT_CORE_ERR_DISCONNECTED;
			break;
		default:
			ret = sessionRejectCommand(session, &cmd, NP_RT_CORE_ERR_BAD_STATE);
			checkExpired = (ret >= 0);
			break;
		}

		if (checkExpired) {
			ret = sessionDispatchExpiredTx(session);
			if (ret >= 0)
				ret = 0;
		}
	}

	resetPipeCommand(&cmd);
	return ret;
}

/* 0x81314436: State 4 (CLOSING) */
static int stateClosingHandler(void *arg, int *nextState)
{
	NpRtSession *session = reinterpret_cast<NpRtSession *>(arg);
	NpRtPipeCommand cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.commandId = NP_RT_CMD_NONE;

	int ret = sessionReceiveCommandWithKeepAlive(session, &cmd);
	if (ret >= 0) {
		if (cmd.commandId == NP_RT_CMD_RECV_CLOSED) {
			npRtCalloutCancel(&session->connectTimer);
			*nextState = NP_RT_STATE_STOPPED;
		} else if (cmd.commandId == NP_RT_CMD_RECV_STANZA) {
			ret = sessionHandleIncomingStanza(session, &cmd.body.recvStanza);
		} else {
			ret = sessionRejectCommand(session, &cmd, NP_RT_CORE_ERR_BAD_STATE);
		}
	}

	if (ret >= 0)
		ret = sessionDispatchExpiredTx(session);

	if (ret >= 0 && npRtCalloutIsFired(&session->connectTimer) != 0)
		ret = NP_RT_CORE_ERR_CLOSE_TIMEOUT;

	if (ret < 0)
		npRtCalloutCancel(&session->connectTimer);

	resetPipeCommand(&cmd);
	return (ret >= 0) ? 0 : ret;
}

typedef int (*NpRtStateHandler)(void *session, int *nextState);

/* 0x8154646C: Session state machine dispatch table [0..5] */
static const NpRtStateHandler kSessionStateHandlers[6] = {
	npRtStateInvalid,
	stateIdleHandler,
	stateConnectingHandler,
	stateRunningHandler,
	stateClosingHandler,
	npRtStateInvalid
};

/* 0x81314176 */
static int runSessionStateMachine(NpRtSession *session)
{
	int status = 0;
	for (;;) {
		int lockState = npRtLockLwMutex(&session->mutex);
		bool isStopped = false;
		if (lockState >= 0) {
			isStopped = (session->state == NP_RT_STATE_STOPPED);
			npRtUnlockLwMutex(&session->mutex, lockState);
		} else {
			status = lockState;
			break;
		}

		if (isStopped)
			return 0;

		int nextState = session->state;
		if (nextState != NP_RT_STATE_CLOSING && session->aborted != 0) {
			status = NP_RT_CORE_ERR_ABORTED;
			break;
		}

		status = kSessionStateHandlers[nextState](session, &nextState);
		if (status < 0)
			break;

		if (session->state != nextState) {
			status = transitionSessionState(session, nextState, status);
			if (status < 0)
				break;
		}
	}

	transitionSessionState(session, NP_RT_STATE_ERROR, status);
	return 0;
}

/* --------------------------------------------------------------------------
 * Session Lifecycle & Table Management (0x81310F74..0x813116E7)
 * -------------------------------------------------------------------------- */
/* 0x81310F74 */
static void sessionWaitMainThread(NpRtSession *session)
{
	if (session->mainThreadPtr != NULL) {
		npRtWaitThreadEnd(session->mainThreadPtr);
		session->mainThreadPtr = NULL;
	}
}

/* 0x81310F8A */
static void sessionAbort(NpRtSession *session)
{
	int lockState = npRtLockLwMutex(&session->mutex);
	npRtXmppClientAbort(&session->xmppClient, 7);
	if (session->accountReqHandle != -1)
		npAccountAbortRequest(session->accountReqHandle);
	pipeSendAbort(&session->msgPipeId);
	session->aborted = 1;
	npRtUnlockLwMutex(&session->mutex, lockState);
}

static void sessionDestroyFinal(NpRtSession *session)
{
	sessionAbort(session);
	sessionWaitMainThread(session);
	npRtCalloutCancel(&session->connectTimer);
	npRtXmppClientTerm(&session->xmppClient);
	txQueueTerm(&session->txQueue);
	npRtDeleteMsgPipe(&session->msgPipeId);
	sceKernelDeleteLwMutex(&session->mutex);
	npRtHeapFree(session, NULL);
}

static void unlinkSessionLocked(NpRtSession *session)
{
	if ((session->sessionFlags & NP_RT_SFLAG_LINKED) == 0)
		return;

	if (session->next != NULL)
		session->next->prev = session->prev;
	else
		s_sessionTail = session->prev;

	if (session->prev != NULL)
		session->prev->next = session->next;
	else
		s_sessionHead = session->next;

	session->next = NULL;
	session->prev = NULL;
	session->sessionFlags &= ~static_cast<SceUInt32>(NP_RT_SFLAG_LINKED);
}

/* 0x81310FC4 */
static int sessionCreate(const SceNpOnlineId *onlineId, const NpRtServerConfig *serverConfig,
			 NpRtSession **outSession)
{
	if (s_sessionMgrInit == 0)
		return NP_RT_CORE_ERR_NOT_INITIALIZED;

	NpRtSession *session = reinterpret_cast<NpRtSession *>(
		npRtHeapAlloc(sizeof(NpRtSession), NULL));
	if (session == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(session, 0, sizeof(*session));
	int ret = sceKernelCreateLwMutex(&session->mutex, "SceNpSession", 0x2002u, 0, NULL);
	if (ret < 0) {
		npRtHeapFree(session, NULL);
		return ret;
	}

	sceClibMemcpy_safe(&session->identity.onlineId, onlineId, sizeof(*onlineId));
	sceClibMemcpy_safe(&session->identity.serverConfig, serverConfig, sizeof(*serverConfig));
	session->state = NP_RT_STATE_IDLE;

	ret = pipeCreate(&session->msgPipeId);
	if (ret < 0) {
		sceKernelDeleteLwMutex(&session->mutex);
		npRtHeapFree(session, NULL);
		return ret;
	}

	txQueueInit(&session->txQueue, &session->mutex, sessionTxWakeHandler, session);
	npRtXmppClientInit(&session->xmppClient);
	npRtCalloutInit(&session->connectTimer);
	session->mainThreadPtr = NULL;
	session->recvThreadPtr = NULL;
	session->aborted = 0;
	session->accountReqHandle = -1;

	++s_nextSessionId;
	if (s_nextSessionId == 0)
		s_nextSessionId = 1;
	session->identity.sessionId = s_nextSessionId;
	++session->refCount;

	int coreLock = coreMutexLock();
	if (coreLock < 0) {
		sessionDestroyFinal(session);
		return coreLock;
	}

	++s_sessionCount;
	int sessLock = npRtLockLwMutex(&session->mutex);
	if (sessLock < 0) {
		coreMutexUnlock(coreLock);
		sessionDestroyFinal(session);
		return sessLock;
	}

	session->next = NULL;
	session->prev = s_sessionTail;
	if (s_sessionTail != NULL)
		s_sessionTail->next = session;
	if (s_sessionHead == NULL)
		s_sessionHead = session;
	s_sessionTail = session;
	session->sessionFlags |= NP_RT_SFLAG_LINKED;

	npRtUnlockLwMutex(&session->mutex, sessLock);
	coreMutexUnlock(coreLock);
	*outSession = session;
	return 0;
}

/* 0x81311242 */
static int sessionAcquireByOnlineId(const SceNpId *npId, NpRtSession **outSession)
{
	if (s_sessionMgrInit == 0)
		return NP_RT_CORE_ERR_NOT_INITIALIZED;

	int coreLock = coreMutexLock();
	if (coreLock < 0)
		return coreLock;

	NpRtSession *found = NULL;
	for (NpRtSession *cur = s_sessionHead; cur != NULL; cur = cur->next) {
		if (sceClibStrncasecmp(npId->handle.data, cur->identity.onlineId.data, 16u) == 0) {
			found = cur;
			break;
		}
	}

	int status = 0;
	int sessLock = 0;
	if (found != NULL) {
		sessLock = npRtLockLwMutex(&found->mutex);
		if (sessLock < 0) {
			status = sessLock;
		} else if ((found->sessionFlags & NP_RT_SFLAG_CLOSING) != 0) {
			status = NP_RT_CORE_ERR_SESSION_NOT_FOUND;
		} else {
			++found->refCount;
			*outSession = found;
		}
		npRtUnlockLwMutex(&found->mutex, sessLock);
	} else {
		status = NP_RT_CORE_ERR_SESSION_NOT_FOUND;
	}

	coreMutexUnlock(coreLock);
	return status;
}

/* 0x813112E4 */
static int sessionAcquireById(int sessionHandle, NpRtSession **outSession)
{
	if (s_sessionMgrInit == 0)
		return NP_RT_CORE_ERR_NOT_INITIALIZED;

	int coreLock = coreMutexLock();
	if (coreLock < 0)
		return coreLock;

	NpRtSession *found = NULL;
	for (NpRtSession *cur = s_sessionHead; cur != NULL; cur = cur->next) {
		if (cur->identity.sessionId == sessionHandle) {
			found = cur;
			break;
		}
	}

	int status = 0;
	int sessLock = 0;
	if (found != NULL) {
		sessLock = npRtLockLwMutex(&found->mutex);
		if (sessLock < 0) {
			status = sessLock;
		} else if ((found->sessionFlags & NP_RT_SFLAG_CLOSING) != 0) {
			status = NP_RT_CORE_ERR_SESSION_NOT_FOUND;
		} else {
			++found->refCount;
			*outSession = found;
		}
		npRtUnlockLwMutex(&found->mutex, sessLock);
	} else {
		status = NP_RT_CORE_ERR_SESSION_NOT_FOUND;
	}

	coreMutexUnlock(coreLock);
	return status;
}

/* 0x8131137E */
static int sessionRelease(NpRtSession *session)
{
	if (s_sessionMgrInit == 0)
		return NP_RT_CORE_ERR_NOT_INITIALIZED;

	NpRtSession *toDestroy = NULL;
	int coreLock = coreMutexLock();
	int sessLock = 0;
	int status = 0;

	if (coreLock >= 0) {
		sessLock = npRtLockLwMutex(&session->mutex);
		if (sessLock >= 0) {
			--session->refCount;
			if (session->refCount == 0 &&
			    (session->sessionFlags & NP_RT_SFLAG_CLOSING) != 0) {
				unlinkSessionLocked(session);
				toDestroy = session;
			}
		} else {
			status = sessLock;
		}
	} else {
		status = coreLock;
	}

	npRtUnlockLwMutex(&session->mutex, sessLock);
	coreMutexUnlock(coreLock);

	if (toDestroy != NULL)
		sessionDestroyFinal(toDestroy);
	return status;
}

/* 0x813114B0 */
static int sessionMarkClosingAndRelease(NpRtSession *session)
{
	if (s_sessionMgrInit == 0)
		return NP_RT_CORE_ERR_NOT_INITIALIZED;

	NpRtSession *toDestroy = NULL;
	int coreLock = coreMutexLock();
	int sessLock = 0;
	int status = 0;

	if (coreLock >= 0) {
		sessLock = npRtLockLwMutex(&session->mutex);
		if (sessLock >= 0) {
			session->sessionFlags |= NP_RT_SFLAG_CLOSING;
			--session->refCount;
			if (session->refCount == 0) {
				unlinkSessionLocked(session);
				toDestroy = session;
			}
		} else {
			status = sessLock;
		}
	} else {
		status = coreLock;
	}

	npRtUnlockLwMutex(&session->mutex, sessLock);
	coreMutexUnlock(coreLock);

	if (toDestroy != NULL)
		sessionDestroyFinal(toDestroy);
	return status;
}

/* 0x813115EA */
static int sessionStartWorker(NpRtSession *session)
{
	int ret = npRtStartThread(&s_mainThreadId, session);
	if (ret < 0)
		return ret;
	session->mainThreadPtr = &s_mainThreadId;
	session->recvThreadPtr = &s_recvThread;
	return 0;
}

/* 0x8131161E */
static int sessionManagerInit(const NpPollingThreadParam *mainParam,
			      const NpPollingThreadParam *recvParam)
{
	if (s_sessionMgrInit != 0)
		return NP_RT_CORE_ERR_ALREADY_INIT;

	s_sessionHead = NULL;
	s_sessionTail = NULL;
	s_sessionCount = 0;
	s_nextSessionId = 0;
	s_sessionMgrInit = 1;

	int ret = coreRecvThreadCreate(&s_recvThread, recvParam);
	if (ret < 0)
		return ret;

	ret = coreMainThreadCreate(&s_mainThreadId, mainParam);
	if (ret < 0) {
		npRtJoinAndDeleteThread(&s_recvThread.threadId);
		return ret;
	}
	return 0;
}

/* 0x813116AA */
static int sessionManagerTerm(void)
{
	if (s_sessionMgrInit == 0)
		return NP_RT_CORE_ERR_NOT_INITIALIZED;

	while (s_sessionHead != NULL)
		sessionMarkClosingAndRelease(s_sessionHead);

	npRtJoinAndDeleteThread(&s_mainThreadId);
	npRtJoinAndDeleteThread(&s_recvThread.threadId);
	s_sessionCount = 0;
	s_sessionMgrInit = 0;
	return 0;
}

} /* namespace */

/* --------------------------------------------------------------------------
 * Public SceNpCore API (0x81310320..0x81310F73, 0x81313EA8)
 * -------------------------------------------------------------------------- */
/* 0x81310320 */
int npRtCoreInit(const NpRtCoreInitParam *param)
{
	int ret = npRtHeapInit(NULL, 0);
	if (ret >= 0)
		ret = coreMutexInit();
	if (ret >= 0)
		ret = sessionManagerInit(&param->mainThread, &param->recvThread);
	if (ret >= 0)
		ret = handlerRegistryInit();
	if (ret >= 0)
		return 0;

	handlerRegistryTerm();
	sessionManagerTerm();
	coreMutexTerm();
	npRtHeapTerm();
	return ret;
}

/* 0x8131036E */
int npRtCoreTerm(void)
{
	handlerRegistryTerm();
	sessionManagerTerm();
	coreMutexTerm();
	npRtHeapTerm();
	return 0;
}

/* 0x81310980 */
int npRtRegisterDefaultHandler(const NpRtHandlerConfig *config, int *outHandlerId)
{
	if (s_defaultHandler != NULL)
		return NP_RT_CORE_ERR_DEFAULT_HANDLER_SET;

	NpRtHandlerNode *node = NULL;
	int ret = registerHandlerInternal(config, &node);
	if (ret < 0)
		return ret;

	s_defaultHandler = node;
	*outHandlerId = node->handlerId;
	return 0;
}

/* 0x813109DE */
int npRtRegisterHandler(const NpRtHandlerConfig *config, int *outHandlerId)
{
	NpRtHandlerNode *node = NULL;
	int ret = registerHandlerInternal(config, &node);
	if (ret < 0)
		return ret;

	*outHandlerId = node->handlerId;
	return 0;
}

/* 0x81310A14 */
int npRtUnregisterHandler(int handlerId)
{
	NpRtHandlerNode *found = NULL;
	for (NpRtHandlerNode *cur = s_handlerHead; cur != NULL; cur = cur->next) {
		if (cur->handlerId == handlerId) {
			found = cur;
			break;
		}
	}
	if (found == NULL)
		return NP_RT_CORE_ERR_HANDLER_NOT_FOUND;

	bool wasDefault = (found == s_defaultHandler);
	unlinkAndFreeHandler(found);
	if (wasDefault)
		s_defaultHandler = NULL;
	return 0;
}

/* 0x81310A94 */
int npRtCreateSession(const SceNpOnlineId *onlineId, const NpRtServerConfig *serverConfig,
		      int *outSessionHandle)
{
	NpRtSession *session = NULL;
	int ret = sessionCreate(onlineId, serverConfig, &session);
	if (ret >= 0)
		ret = sessionStartWorker(session);

	if (ret < 0) {
		if (session != NULL)
			sessionMarkClosingAndRelease(session);
		return ret;
	}

	*outSessionHandle = session->identity.sessionId;
	return 0;
}

/* 0x81310AE6 */
int npRtDestroySession(int sessionHandle)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireById(sessionHandle, &session);
	if (ret < 0)
		return ret;

	sessionMarkClosingAndRelease(session);
	sessionAbort(session);
	sessionWaitMainThread(session);
	sessionRelease(session);
	return 0;
}

/* 0x81310B30 */
int npRtConnectSession(int sessionHandle)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireById(sessionHandle, &session);
	if (ret >= 0) {
		NpRtPipeCommand cmd;
		sceClibMemset(&cmd, 0, sizeof(cmd));
		cmd.commandId = NP_RT_CMD_CONNECT;
		SceUInt32 mask = 1u << NP_RT_STATE_IDLE;
		ret = sessionPostCommand(session, &mask, &cmd);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x81310B92 */
int npRtReconnectSession(int sessionHandle, const char *sessionSid)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireById(sessionHandle, &session);
	if (ret >= 0) {
		NpRtPipeCommand cmd;
		sceClibMemset(&cmd, 0, sizeof(cmd));
		cmd.commandId = NP_RT_CMD_RECONNECT;
		sceClibMemcpy_safe(cmd.body.reconnectSid, sessionSid, kSessionSidMaxSize);
		SceUInt32 mask = 1u << NP_RT_STATE_IDLE;
		ret = sessionPostCommand(session, &mask, &cmd);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x81310BFE */
int npRtDisconnectSession(int sessionHandle)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireById(sessionHandle, &session);
	if (ret >= 0) {
		NpRtPipeCommand cmd;
		sceClibMemset(&cmd, 0, sizeof(cmd));
		cmd.commandId = NP_RT_CMD_DISCONNECT;
		SceUInt32 mask = 1u << NP_RT_STATE_RUNNING;
		ret = sessionPostCommand(session, &mask, &cmd);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x81310C5E */
int npRtGetSessionByNpId(const SceNpId *npId, int *outSessionHandle)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireByOnlineId(npId, &session);
	if (ret >= 0) {
		*outSessionHandle = session->identity.sessionId;
		ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x81310CA2 */
int npRtAllocTransactionId(int sessionHandle, int *outTransactionId)
{
	(void)sessionHandle;
	SceInt32 nextId = sceAtomicIncrement32AcqRel(&s_txIdCounter) + 1;
	if (nextId == 0)
		nextId = sceAtomicIncrement32AcqRel(&s_txIdCounter) + 1;
	*outTransactionId = nextId;
	return 0;
}

/* 0x81310CE6 */
int npRtSendIqRequest(int sessionHandle, int handlerId, int transactionId, NpRtStanza *stanza,
		      NpRtIqResponseCallback callback, void *userArg)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireById(sessionHandle, &session);
	if (ret >= 0) {
		ret = sessionQueueIq(session, handlerId, transactionId, stanza, 0ULL,
				     callback, userArg);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x81310D64 */
int npRtSendIqRequestEx(int sessionHandle, int handlerId, int transactionId, NpRtStanza *stanza,
			SceUInt64 timeoutUsec, NpRtIqResponseCallback callback, void *userArg)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireById(sessionHandle, &session);
	if (ret >= 0) {
		ret = sessionQueueIq(session, handlerId, transactionId, stanza, timeoutUsec,
				     callback, userArg);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x81310DE8 */
int npRtSendMessage(int sessionHandle, int handlerId, int transactionId,
		    NpRtMessageStanza *stanza, NpRtSendDoneCallback callback, void *userArg)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireById(sessionHandle, &session);
	if (ret >= 0) {
		ret = sessionQueueMessage(session, handlerId, transactionId, stanza,
					  callback, userArg);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x81310E5C */
int npRtSendPresence(int sessionHandle, int handlerId, int transactionId,
		     NpRtPresenceStanza *stanza, NpRtSendDoneCallback callback, void *userArg)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireById(sessionHandle, &session);
	if (ret >= 0) {
		ret = sessionQueuePresence(session, handlerId, transactionId, stanza,
					   callback, userArg);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x81310ED0 */
int npRtSendIqResult(int sessionHandle, int handlerId, NpRtStanza *stanza)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireById(sessionHandle, &session);
	if (ret >= 0) {
		ret = sessionQueueIqResult(session, handlerId, stanza);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x81310F22 */
int npRtSendIqError(int sessionHandle, int handlerId, const NpXmppErrorStanza *errStanza)
{
	NpRtSession *session = NULL;
	int ret = sessionAcquireById(sessionHandle, &session);
	if (ret >= 0) {
		ret = sessionQueueIqError(session, handlerId, errStanza);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x81313EA8 */
int npRtJoinAndDeleteThread(SceUID *threadId)
{
	sceKernelWaitThreadEnd(*threadId, NULL, NULL);
	return sceKernelDeleteThread(*threadId);
}
