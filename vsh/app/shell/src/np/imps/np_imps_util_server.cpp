/*
 * np_imps_util_server.cpp: SceNpImpsUtil IPMI server, raw-message channel manager,
 *                          SessionHolder, AsyncCallRelayMgr, and SceNpImpsSvc C facade
 *                          (0x812F0510..0x812F4C43, 0x812F7FD0..0x812F899B).
 */
#include <stddef.h>
#include <std/new>
#include <std/string.h>
#include <scetypes.h>
#include <kernel.h>
#include <libkernel/libkernel.h>
#include "np_imps_util.h"

/* External shell / NP service bridges */
extern "C" {
extern void *sce_paf_malloc(SceSize size);
extern void sce_paf_free(void *ptr);
extern int SceNpImpsUtil_NotifyShellProcess(void);
}

/* Global SceNpImpsUtil state (0x815460DC..0x81546100, 0x815C3840) */
static NpImpsUtilServerContext *s_serverCtx = NULL;		/* 0x815460DC */
static NpImpsUtilSessionHolder *s_gameSessionHolder = NULL;	/* 0x815460E0 */
static NpImpsUtilSessionHolder *s_systemSessionHolder = NULL;	/* 0x815460E4 */
static IpmiWrapper::Server *s_ipmiServer = NULL;		/* 0x815460E8 */
static NpImpsUtilMpState *s_mpState = NULL;			/* 0x815460EC */
static SceUInt32 s_svcListenerId = 0;				/* 0x815C3840 */

/* SceNpImpsSvc C facade globals (0x815460FC, 0x81546100) */
static NpImpsSvc *s_svcInstance = NULL;
static NpImpsSvcAsyncInterface *s_svcAsyncInterface = NULL;

/* ============================================================================
 * Heap helpers (0x812F4BF4..0x812F4C43)
 * ============================================================================ */

void npImpsUtilHeapInit(void)
{
}

void npImpsUtilHeapTerm(void)
{
}

void *npImpsUtilMalloc(SceSize size)
{
	return sce_paf_malloc(size);
}

void npImpsUtilFree(void *ptr)
{
	if (ptr != NULL)
		sce_paf_free(ptr);
}

NpImpsUtilServerContext *npImpsUtilGetServerContext(void)
{
	return s_serverCtx;
}

NpImpsUtilSessionHolder *npImpsUtilGetGameSessionHolder(void)
{
	return s_gameSessionHolder;
}

NpImpsUtilSessionHolder *npImpsUtilGetSystemSessionHolder(void)
{
	return s_systemSessionHolder;
}

IpmiWrapper::Server *npImpsUtilGetIpmiServer(void)
{
	return s_ipmiServer;
}

NpImpsUtilMpState *npImpsUtilGetMpState(void)
{
	return s_mpState;
}

/* ============================================================================
 * Queue overflow strategies (0x812F3738..0x812F37B1)
 * ============================================================================ */

class NpImpsSystemQueueOverflowStrategy : public NpImpsQueueOverflowStrategy {
public:
	virtual int HandleOverflow(IPMI::Session *session, SceUInt32 queueIndex,
				   const void *msgBuf, SceSize msgSize)
	{
		(void)msgBuf;
		(void)msgSize;
		if (queueIndex == 0) {
			session->emptyMsgQueue(0);
			return npImpsUtilHandleResyncSystemEvents(session);
		}
		return 0;
	}
};

class NpImpsGameQueueOverflowStrategy : public NpImpsQueueOverflowStrategy {
public:
	virtual int HandleOverflow(IPMI::Session *session, SceUInt32 queueIndex,
				   const void *msgBuf, SceSize msgSize)
	{
		if (queueIndex == 0) {
			SceUID processId = session->getProcessId();
			return npImpsUtilMpStatePostEvent(s_mpState,
							  NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW,
							  processId,
							  NP_IMPS_UTIL_ERROR_QUEUE_OVERFLOW);
		}
		if (queueIndex == 1) {
			session->emptyMsgQueue(queueIndex);
			return session->trySendMsg(queueIndex, msgBuf, msgSize);
		}
		return 0;
	}
};

/* ============================================================================
 * Presence snapshot diffing and wire-format conversion (0x812F0ACA..0x812F0CCF)
 * ============================================================================ */

void npImpsUtilDiffMyPresence(int eventType, const NpImpsSvcPresenceSnapshot *curSnap,
			      const NpImpsSvcPresenceSnapshot *prevSnap,
			      SceUInt8 *outGameChanged, SceUInt8 *outPartyChanged)
{
	*outGameChanged = 0;
	*outPartyChanged = 0;

	if (eventType == 1) {
		*outGameChanged = 1;
		*outPartyChanged = 1;
		return;
	}

	if (eventType == 2) {
		/* Compare titleName + titleId + commId + contentId + joinableCommId + gamePresence (0x220 bytes) */
		if (sce_paf_memcmp(curSnap->titleName, prevSnap->titleName, 0x220u) != 0)
			*outGameChanged = 1;
		if (sce_paf_memcmp(&curSnap->partyPresence, &prevSnap->partyPresence,
				   sizeof(NpImpsPartyPresence)) != 0) {
			*outPartyChanged = 1;
		}
	}
}

void npImpsUtilFillGamePresenceDetail(NpImpsGamePresenceDetail *outDetail,
				     const NpImpsSvcPresenceSnapshot *snapshot)
{
	sceClibMemset(outDetail, 0, sizeof(*outDetail));
	outDetail->size = sizeof(*outDetail);
	outDetail->titleId = snapshot->titleId;
	outDetail->commId = snapshot->commId;
	outDetail->joinableCommId = snapshot->joinableCommId;
	sceClibMemcpy_safe(outDetail->contentId, snapshot->contentId, sizeof(outDetail->contentId));

	outDetail->gamePresence.size = sizeof(outDetail->gamePresence);
	sceClibStrncpy(outDetail->gamePresence.title, snapshot->titleName,
		       sizeof(outDetail->gamePresence.title));
	outDetail->gamePresence.title[sizeof(outDetail->gamePresence.title) - 1] = '\0';
	outDetail->gamePresence.body = snapshot->gamePresence;
}

void npImpsUtilFillPartyPresence(NpImpsPartyPresence *outParty,
				 const NpImpsSvcPresenceSnapshot *snapshot)
{
	sceClibMemset(outParty, 0, sizeof(*outParty));
	*outParty = snapshot->partyPresence;
}

/* ============================================================================
 * Raw-message channel manager ("SceNpImpsUtilRawMsgCbMgr", 0x812F11E4..0x812F180F)
 * ============================================================================ */

class NpImpsUtilRawMsgMgr {
public:
	NpImpsUtilRawMsgMgr()
		: m_mutex("SceNpImpsUtilRawMsgCbMgr", 0),
		  m_channels(NULL),
		  m_sessionChannels(NULL)
	{
	}

	~NpImpsUtilRawMsgMgr()
	{
		m_mutex.Lock();
		while (m_channels != NULL) {
			ChannelNode *next = m_channels->next;
			npImpsUtilFree(m_channels);
			m_channels = next;
		}
		while (m_sessionChannels != NULL) {
			SessionChannelNode *next = m_sessionChannels->next;
			npImpsUtilFree(m_sessionChannels);
			m_sessionChannels = next;
		}
		m_mutex.Unlock();
	}

	int RegisterChannel(int channelId, const NpImpsRawMsgServerCallbacks *callbacks)
	{
		m_mutex.Lock();
		for (ChannelNode *cur = m_channels; cur != NULL; cur = cur->next) {
			if (cur->channelId == channelId) {
				m_mutex.Unlock();
				return NP_IMPS_UTIL_ERROR_CHANNEL_REGISTERED;
			}
		}

		ChannelNode *node = static_cast<ChannelNode *>(npImpsUtilMalloc(sizeof(ChannelNode)));
		if (node == NULL) {
			m_mutex.Unlock();
			return NP_IMPS_UTIL_ERROR_INTERNAL;
		}
		node->channelId = channelId;
		node->callbacks = *callbacks;
		node->next = m_channels;
		m_channels = node;
		m_mutex.Unlock();
		return 0;
	}

	int UnregisterChannel(int channelId)
	{
		m_mutex.Lock();
		ChannelNode **link = &m_channels;
		while (*link != NULL) {
			if ((*link)->channelId == channelId) {
				ChannelNode *victim = *link;
				*link = victim->next;
				npImpsUtilFree(victim);
				m_mutex.Unlock();
				return 0;
			}
			link = &(*link)->next;
		}
		m_mutex.Unlock();
		return NP_IMPS_UTIL_ERROR_CHANNEL_NOT_FOUND;
	}

	const NpImpsRawMsgServerCallbacks *FindChannel(int channelId)
	{
		m_mutex.Lock();
		for (ChannelNode *cur = m_channels; cur != NULL; cur = cur->next) {
			if (cur->channelId == channelId) {
				const NpImpsRawMsgServerCallbacks *cb = &cur->callbacks;
				m_mutex.Unlock();
				return cb;
			}
		}
		m_mutex.Unlock();
		return NULL;
	}

	int BindSession(IPMI::Session *session, int channelId)
	{
		m_mutex.Lock();
		for (SessionChannelNode *cur = m_sessionChannels; cur != NULL; cur = cur->next) {
			if (cur->session == session) {
				m_mutex.Unlock();
				return NP_IMPS_UTIL_ERROR_INTERNAL;
			}
		}

		SessionChannelNode *node =
			static_cast<SessionChannelNode *>(npImpsUtilMalloc(sizeof(SessionChannelNode)));
		if (node == NULL) {
			m_mutex.Unlock();
			return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
		}
		node->session = session;
		node->channelId = channelId;
		node->next = m_sessionChannels;
		m_sessionChannels = node;
		m_mutex.Unlock();
		return 0;
	}

	int UnbindSession(IPMI::Session *session)
	{
		m_mutex.Lock();
		SessionChannelNode **link = &m_sessionChannels;
		while (*link != NULL) {
			if ((*link)->session == session) {
				SessionChannelNode *victim = *link;
				*link = victim->next;
				npImpsUtilFree(victim);
				m_mutex.Unlock();
				return 0;
			}
			link = &(*link)->next;
		}
		m_mutex.Unlock();
		return NP_IMPS_UTIL_ERROR_INTERNAL;
	}

	int GetSessionChannel(IPMI::Session *session, int *outChannelId)
	{
		m_mutex.Lock();
		for (SessionChannelNode *cur = m_sessionChannels; cur != NULL; cur = cur->next) {
			if (cur->session == session) {
				*outChannelId = cur->channelId;
				m_mutex.Unlock();
				return 0;
			}
		}
		m_mutex.Unlock();
		return NP_IMPS_UTIL_ERROR_INTERNAL;
	}

	bool HasSession(IPMI::Session *session)
	{
		int channelId = 0;
		return GetSessionChannel(session, &channelId) >= 0;
	}

private:
	struct ChannelNode {
		ChannelNode *next;
		int channelId;
		NpImpsRawMsgServerCallbacks callbacks;
	};

	struct SessionChannelNode {
		SessionChannelNode *next;
		IPMI::Session *session;
		int channelId;
	};

	paf::thread::RMutex m_mutex;
	ChannelNode *m_channels;
	SessionChannelNode *m_sessionChannels;
};

static NpImpsUtilRawMsgMgr *s_rawMsgMgr = NULL;		/* 0x815460F0 */

static int rawMsgMgrInit(void)
{
	if (s_rawMsgMgr != NULL)
		return NP_IMPS_UTIL_ERROR_ALREADY_INITIALIZED;

	void *mem = npImpsUtilMalloc(sizeof(NpImpsUtilRawMsgMgr));
	if (mem == NULL)
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;

	s_rawMsgMgr = new (mem) NpImpsUtilRawMsgMgr();
	return 0;
}

static int rawMsgMgrTerm(void)
{
	if (s_rawMsgMgr == NULL)
		return NP_IMPS_UTIL_ERROR_NOT_INITIALIZED;

	s_rawMsgMgr->~NpImpsUtilRawMsgMgr();
	npImpsUtilFree(s_rawMsgMgr);
	s_rawMsgMgr = NULL;
	return 0;
}

/* 0x812F11E4 */
int npImpsRawMsgRegisterChannel(int channelId, const NpImpsRawMsgServerCallbacks *callbacks)
{
	if (s_rawMsgMgr == NULL)
		return NP_IMPS_UTIL_ERROR_INTERNAL;
	return s_rawMsgMgr->RegisterChannel(channelId, callbacks);
}

/* 0x812F1208 */
int npImpsRawMsgUnregisterChannel(int channelId)
{
	if (s_rawMsgMgr == NULL)
		return NP_IMPS_UTIL_ERROR_INTERNAL;
	return s_rawMsgMgr->UnregisterChannel(channelId);
}

/* 0x812F122A */
int npImpsRawMsgSend(IPMI::Session *session, const void *msgBuf, SceSize msgSize, int mode)
{
	int ret = session->trySendMsg(2, msgBuf, msgSize);
	if (ret == NP_IMPS_IPMI_ERROR_QUEUE_FULL) {
		if (mode == 1) {
			NpImpsGameQueueOverflowStrategy gameStrategy;
			gameStrategy.HandleOverflow(session, 2, msgBuf, msgSize);
		} else if (mode == 2) {
			session->emptyMsgQueue(2);
		} else if (mode == 3) {
			if (session->emptyMsgQueue(2) >= 0)
				session->trySendMsg(2, msgBuf, msgSize);
		}
		return 0;
	}
	return ret;
}

/* 0x812F12D6 */
static int handleRawMsgOpen(IPMI::Session *session, int channelId)
{
	if (s_rawMsgMgr == NULL)
		return NP_IMPS_UTIL_ERROR_INTERNAL;

	int ret = s_rawMsgMgr->BindSession(session, channelId);
	if (ret < 0)
		return ret;

	const NpImpsRawMsgServerCallbacks *cb = s_rawMsgMgr->FindChannel(channelId);
	if (cb == NULL)
		return NP_IMPS_UTIL_ERROR_INTERNAL;

	if (cb->onClientConnected != NULL)
		cb->onClientConnected(session);
	return 0;
}

/* 0x812F1328 */
static int handleRawMsgClose(IPMI::Session *session, int channelId)
{
	if (s_rawMsgMgr == NULL)
		return NP_IMPS_UTIL_ERROR_INTERNAL;

	const NpImpsRawMsgServerCallbacks *cb = s_rawMsgMgr->FindChannel(channelId);
	if (cb != NULL && cb->onClientDisconnected != NULL)
		cb->onClientDisconnected(session);

	return s_rawMsgMgr->UnbindSession(session);
}

/* 0x812F1364 */
static int handleRawMsgSend(IPMI::Session *session, int channelId, const void *msgBuf,
			    SceSize msgSize)
{
	(void)msgSize;
	if (s_rawMsgMgr == NULL)
		return NP_IMPS_UTIL_ERROR_INTERNAL;

	const NpImpsRawMsgServerCallbacks *cb = s_rawMsgMgr->FindChannel(channelId);
	if (cb == NULL || cb->onClientMessage == NULL)
		return NP_IMPS_UTIL_ERROR_INTERNAL;

	return cb->onClientMessage(session, msgBuf);
}

/* ============================================================================
 * Wire message structs for SessionHolder (0x812F1810..0x812F3737)
 * ============================================================================ */

struct NpImpsWireServiceState {
	int kind;
	int state;
	int errorCode;
};

struct NpImpsWireMyOnlineStatus {
	int kind;
	int status;
};

struct NpImpsWireNpIdAction {
	int kind;
	int action;
	SceNpId npId;
};

struct NpImpsWireFriendContext {
	int kind;
	SceNpId npId;
	int context;
};

struct NpImpsWireFriendCommId {
	int kind;
	SceNpId npId;
	SceNpCommunicationId commId;
};

struct NpImpsWireInGameData {
	int kind;
	SceNpId senderNpId;
	NpImpsSvcInGameDataPayload payload;
};

struct NpImpsWireFriendGamePresence {
	int kind;
	SceNpId npId;
	SceNpCommunicationId curCommId;
	SceNpCommunicationId prevCommId;
	int isInitial;
	NpImpsGamePresenceWithTitle presence;
};

struct NpImpsWireFriendPartyPresence {
	int kind;
	SceNpId npId;
	NpImpsPartyPresence presence;
};

struct NpImpsWireFriendPresenceDetail {
	int kind;
	SceNpId npId;
	NpImpsGamePresenceDetail detail;
};

struct NpImpsWireMyPresenceDetail {
	int kind;
	NpImpsGamePresenceDetail detail;
};

struct NpImpsWirePlatformFriendPresence {
	int kind;
	SceNpId npId;
	NpImpsGamePresenceDetail detail;
	int platform;
};

struct NpImpsWireMyPartyPresence {
	int kind;
	NpImpsPartyPresence presence;
};

NpImpsUtilSessionHolder::NpImpsUtilSessionHolder(NpImpsQueueOverflowStrategy *strategy)
	: m_mutex("SceNpImpsUtilSessionHolderRMtx", 0),
	  m_head(NULL),
	  m_overflowStrategy(strategy)
{
}

NpImpsUtilSessionHolder::~NpImpsUtilSessionHolder()
{
	m_mutex.Lock();
	while (m_head != NULL) {
		SessionMaskNode *next = m_head->next;
		npImpsUtilFree(m_head);
		m_head = next;
	}
	m_mutex.Unlock();

	if (m_overflowStrategy != NULL) {
		m_overflowStrategy->~NpImpsQueueOverflowStrategy();
		npImpsUtilFree(m_overflowStrategy);
		m_overflowStrategy = NULL;
	}
}

/* 0x812F1810 */
int NpImpsUtilSessionHolder::RegisterSession(IPMI::Session *session,
					     const SceUInt8 mask[NP_IMPS_UTIL_EVENT_COUNT])
{
	m_mutex.Lock();
	for (SessionMaskNode *cur = m_head; cur != NULL; cur = cur->next) {
		if (cur->session == session) {
			sce_paf_memcpy(cur->mask, mask, NP_IMPS_UTIL_EVENT_COUNT);
			m_mutex.Unlock();
			return 0;
		}
	}

	SessionMaskNode *node =
		static_cast<SessionMaskNode *>(npImpsUtilMalloc(sizeof(SessionMaskNode)));
	if (node == NULL) {
		m_mutex.Unlock();
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	node->session = session;
	sce_paf_memcpy(node->mask, mask, NP_IMPS_UTIL_EVENT_COUNT);
	node->next = m_head;
	m_head = node;
	m_mutex.Unlock();
	return 0;
}

/* 0x812F1994 */
int NpImpsUtilSessionHolder::UnregisterSession(IPMI::Session *session)
{
	m_mutex.Lock();
	SessionMaskNode **link = &m_head;
	while (*link != NULL) {
		if ((*link)->session == session) {
			SessionMaskNode *victim = *link;
			*link = victim->next;
			npImpsUtilFree(victim);
			break;
		}
		link = &(*link)->next;
	}
	m_mutex.Unlock();
	return 0;
}

/* 0x812F1A0E */
int NpImpsUtilSessionHolder::ClearQueuesForProcess(SceUID processId)
{
	m_mutex.Lock();
	for (SessionMaskNode *cur = m_head; cur != NULL; cur = cur->next) {
		if (cur->session->getProcessId() == processId)
			cur->session->emptyMsgQueue(0);
	}
	m_mutex.Unlock();
	return 0;
}

/* 0x812F1A9C */
bool NpImpsUtilSessionHolder::IsSubscribed(IPMI::Session *session, SceUInt32 eventKind)
{
	if (eventKind >= NP_IMPS_UTIL_EVENT_COUNT)
		return false;

	for (SessionMaskNode *cur = m_head; cur != NULL; cur = cur->next) {
		if (cur->session == session)
			return cur->mask[eventKind] != 0;
	}
	return false;
}

void NpImpsUtilSessionHolder::TrySendToSessionLocked(IPMI::Session *session, SceUInt32 queueIndex,
						     SceUInt32 eventKind, const void *msgBuf,
						     SceSize msgSize)
{
	if (!IsSubscribed(session, eventKind))
		return;

	if (session->trySendMsg(queueIndex, msgBuf, msgSize) == NP_IMPS_IPMI_ERROR_QUEUE_FULL &&
	    m_overflowStrategy != NULL) {
		m_overflowStrategy->HandleOverflow(session, queueIndex, msgBuf, msgSize);
	}
}

void NpImpsUtilSessionHolder::BroadcastLocked(SceUInt32 queueIndex, SceUInt32 eventKind,
					      const void *msgBuf, SceSize msgSize)
{
	for (SessionMaskNode *cur = m_head; cur != NULL; cur = cur->next)
		TrySendToSessionLocked(cur->session, queueIndex, eventKind, msgBuf, msgSize);
}

void NpImpsUtilSessionHolder::SendToProcessLocked(SceUID processId, SceUInt32 queueIndex,
						  SceUInt32 eventKind, const void *msgBuf,
						  SceSize msgSize)
{
	for (SessionMaskNode *cur = m_head; cur != NULL; cur = cur->next) {
		if (cur->session->getProcessId() == processId)
			TrySendToSessionLocked(cur->session, queueIndex, eventKind, msgBuf, msgSize);
	}
}

/* 0x812F1AF4 */
void NpImpsUtilSessionHolder::SendServiceStateToSession(IPMI::Session *session, int state,
							int errorCode)
{
	NpImpsWireServiceState msg;
	msg.kind = NP_IMPS_EVT_SERVICE_STATE;
	msg.state = state;
	msg.errorCode = errorCode;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 1, NP_IMPS_EVT_SERVICE_STATE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F1D1C */
void NpImpsUtilSessionHolder::SendServiceStateToProcess(SceUID processId, int state, int errorCode)
{
	NpImpsWireServiceState msg;
	msg.kind = NP_IMPS_EVT_SERVICE_STATE;
	msg.state = state;
	msg.errorCode = errorCode;

	m_mutex.Lock();
	SendToProcessLocked(processId, 1, NP_IMPS_EVT_SERVICE_STATE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F1D7A */
void NpImpsUtilSessionHolder::SendMyOnlineStatusToSession(IPMI::Session *session, int status)
{
	NpImpsWireMyOnlineStatus msg;
	msg.kind = NP_IMPS_EVT_MY_ONLINE_STATUS;
	msg.status = status;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_MY_ONLINE_STATUS, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F1DFE */
void NpImpsUtilSessionHolder::BroadcastMyOnlineStatus(int status)
{
	NpImpsWireMyOnlineStatus msg;
	msg.kind = NP_IMPS_EVT_MY_ONLINE_STATUS;
	msg.status = status;

	m_mutex.Lock();
	BroadcastLocked(0, NP_IMPS_EVT_MY_ONLINE_STATUS, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F1E4A */
void NpImpsUtilSessionHolder::SendMyOnlineStatusToProcess(SceUID processId, int status)
{
	NpImpsWireMyOnlineStatus msg;
	msg.kind = NP_IMPS_EVT_MY_ONLINE_STATUS;
	msg.status = status;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_MY_ONLINE_STATUS, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F1EA4 */
void NpImpsUtilSessionHolder::SendFriendListToSession(IPMI::Session *session, int action,
						      const SceNpId *npId)
{
	NpImpsWireNpIdAction msg;
	msg.kind = NP_IMPS_EVT_FRIEND_LIST;
	msg.action = action;
	msg.npId = *npId;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_FRIEND_LIST, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F1F6A */
void NpImpsUtilSessionHolder::BroadcastFriendList(int action, const SceNpId *npId)
{
	NpImpsWireNpIdAction msg;
	msg.kind = NP_IMPS_EVT_FRIEND_LIST;
	msg.action = action;
	msg.npId = *npId;

	m_mutex.Lock();
	BroadcastLocked(0, NP_IMPS_EVT_FRIEND_LIST, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F2000 */
void NpImpsUtilSessionHolder::SendFriendListToProcess(SceUID processId, int action,
						      const SceNpId *npId)
{
	NpImpsWireNpIdAction msg;
	msg.kind = NP_IMPS_EVT_FRIEND_LIST;
	msg.action = action;
	msg.npId = *npId;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_FRIEND_LIST, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F209E */
void NpImpsUtilSessionHolder::SendFriendContextToSession(IPMI::Session *session, int context,
							 const SceNpId *npId)
{
	NpImpsWireFriendContext msg;
	msg.kind = NP_IMPS_EVT_FRIEND_ONLINE_STATUS;
	msg.npId = *npId;
	msg.context = context;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_FRIEND_ONLINE_STATUS, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F216C */
void NpImpsUtilSessionHolder::BroadcastFriendContext(int context, const SceNpId *npId)
{
	NpImpsWireFriendContext msg;
	msg.kind = NP_IMPS_EVT_FRIEND_ONLINE_STATUS;
	msg.npId = *npId;
	msg.context = context;

	m_mutex.Lock();
	BroadcastLocked(0, NP_IMPS_EVT_FRIEND_ONLINE_STATUS, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F2206 */
void NpImpsUtilSessionHolder::SendFriendContextToProcess(SceUID processId, int context,
							 const SceNpId *npId)
{
	NpImpsWireFriendContext msg;
	msg.kind = NP_IMPS_EVT_FRIEND_ONLINE_STATUS;
	msg.npId = *npId;
	msg.context = context;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_FRIEND_ONLINE_STATUS, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F22AA */
void NpImpsUtilSessionHolder::SendFriendCommIdToSession(IPMI::Session *session, const SceNpId *npId,
							const SceNpCommunicationId *commId)
{
	NpImpsWireFriendCommId msg;
	msg.kind = NP_IMPS_EVT_FRIEND_COMM_ID;
	msg.npId = *npId;
	msg.commId = *commId;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_FRIEND_COMM_ID, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F2404 */
void NpImpsUtilSessionHolder::SendFriendCommIdToProcess(SceUID processId, const SceNpId *npId,
							const SceNpCommunicationId *commId)
{
	NpImpsWireFriendCommId msg;
	msg.kind = NP_IMPS_EVT_FRIEND_COMM_ID;
	msg.npId = *npId;
	msg.commId = *commId;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_FRIEND_COMM_ID, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F252C */
void NpImpsUtilSessionHolder::SendBlockListToSession(IPMI::Session *session, int action,
						     const SceNpId *npId)
{
	NpImpsWireNpIdAction msg;
	msg.kind = NP_IMPS_EVT_BLOCK_LIST;
	msg.action = action;
	msg.npId = *npId;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_BLOCK_LIST, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F25F4 */
void NpImpsUtilSessionHolder::SendBlockListToProcess(SceUID processId, int action,
						     const SceNpId *npId)
{
	NpImpsWireNpIdAction msg;
	msg.kind = NP_IMPS_EVT_BLOCK_LIST;
	msg.action = action;
	msg.npId = *npId;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_BLOCK_LIST, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F2692 */
void NpImpsUtilSessionHolder::SendInGameDataToProcess(SceUID processId, const SceNpId *senderNpId,
						      const NpImpsSvcInGameDataPayload *payload)
{
	NpImpsWireInGameData msg;
	msg.kind = NP_IMPS_EVT_IN_GAME_DATA_MESSAGE;
	msg.senderNpId = *senderNpId;
	msg.payload = *payload;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_IN_GAME_DATA_MESSAGE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F2806 */
void NpImpsUtilSessionHolder::SendFriendGamePresenceToSession(
	IPMI::Session *session, const SceNpId *npId,
	const SceNpCommunicationId *curCommId, const SceNpCommunicationId *prevCommId,
	int isInitial, const NpImpsGamePresenceWithTitle *presence)
{
	NpImpsWireFriendGamePresence msg;
	msg.kind = NP_IMPS_EVT_FRIEND_GAME_PRESENCE;
	msg.npId = *npId;
	msg.curCommId = *curCommId;
	msg.prevCommId = *prevCommId;
	msg.isInitial = isInitial;
	msg.presence = *presence;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_FRIEND_GAME_PRESENCE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F2A14 */
void NpImpsUtilSessionHolder::SendFriendGamePresenceToProcess(
	SceUID processId, const SceNpId *npId,
	const SceNpCommunicationId *curCommId, const SceNpCommunicationId *prevCommId,
	int isInitial, const NpImpsGamePresenceWithTitle *presence)
{
	NpImpsWireFriendGamePresence msg;
	msg.kind = NP_IMPS_EVT_FRIEND_GAME_PRESENCE;
	msg.npId = *npId;
	msg.curCommId = *curCommId;
	msg.prevCommId = *prevCommId;
	msg.isInitial = isInitial;
	msg.presence = *presence;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_FRIEND_GAME_PRESENCE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F2BF4 */
void NpImpsUtilSessionHolder::SendFriendPartyPresenceToSession(IPMI::Session *session,
							       const SceNpId *npId,
							       const NpImpsPartyPresence *presence)
{
	NpImpsWireFriendPartyPresence msg;
	msg.kind = NP_IMPS_EVT_FRIEND_PARTY_PRESENCE;
	msg.npId = *npId;
	msg.presence = *presence;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_FRIEND_PARTY_PRESENCE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F2DAA */
void NpImpsUtilSessionHolder::SendFriendPartyPresenceToProcess(SceUID processId,
							       const SceNpId *npId,
							       const NpImpsPartyPresence *presence)
{
	NpImpsWireFriendPartyPresence msg;
	msg.kind = NP_IMPS_EVT_FRIEND_PARTY_PRESENCE;
	msg.npId = *npId;
	msg.presence = *presence;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_FRIEND_PARTY_PRESENCE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F2E70 */
void NpImpsUtilSessionHolder::SendFriendPresenceDetailToSession(
	IPMI::Session *session, const SceNpId *npId, const NpImpsGamePresenceDetail *detail)
{
	NpImpsWireFriendPresenceDetail msg;
	msg.kind = NP_IMPS_EVT_FRIEND_GAME_PRESENCE_DETAIL;
	msg.npId = *npId;
	msg.detail = *detail;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_FRIEND_GAME_PRESENCE_DETAIL,
			       &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F2F62 */
void NpImpsUtilSessionHolder::SendFriendPresenceDetailToProcess(
	SceUID processId, const SceNpId *npId, const NpImpsGamePresenceDetail *detail)
{
	NpImpsWireFriendPresenceDetail msg;
	msg.kind = NP_IMPS_EVT_FRIEND_GAME_PRESENCE_DETAIL;
	msg.npId = *npId;
	msg.detail = *detail;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_FRIEND_GAME_PRESENCE_DETAIL,
			    &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F3026 */
void NpImpsUtilSessionHolder::SendMyPresenceDetailToSession(IPMI::Session *session,
							    const NpImpsGamePresenceDetail *detail)
{
	NpImpsWireMyPresenceDetail msg;
	msg.kind = NP_IMPS_EVT_MY_GAME_PRESENCE_DETAIL;
	msg.detail = *detail;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_MY_GAME_PRESENCE_DETAIL, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F30CC */
void NpImpsUtilSessionHolder::BroadcastMyPresenceDetail(const NpImpsGamePresenceDetail *detail)
{
	NpImpsWireMyPresenceDetail msg;
	msg.kind = NP_IMPS_EVT_MY_GAME_PRESENCE_DETAIL;
	msg.detail = *detail;

	m_mutex.Lock();
	BroadcastLocked(0, NP_IMPS_EVT_MY_GAME_PRESENCE_DETAIL, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F3134 */
void NpImpsUtilSessionHolder::SendMyPresenceDetailToProcess(SceUID processId,
							    const NpImpsGamePresenceDetail *detail)
{
	NpImpsWireMyPresenceDetail msg;
	msg.kind = NP_IMPS_EVT_MY_GAME_PRESENCE_DETAIL;
	msg.detail = *detail;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_MY_GAME_PRESENCE_DETAIL, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F31AE */
void NpImpsUtilSessionHolder::SendPlatformFriendPresenceToSession(
	IPMI::Session *session, const SceNpId *npId,
	const NpImpsGamePresenceDetail *detail, int platform)
{
	NpImpsWirePlatformFriendPresence msg;
	msg.kind = NP_IMPS_EVT_PLATFORM_FRIEND_PRESENCE;
	msg.npId = *npId;
	msg.detail = *detail;
	msg.platform = platform;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_PLATFORM_FRIEND_PRESENCE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F32A6 */
void NpImpsUtilSessionHolder::BroadcastPlatformFriendPresence(
	const SceNpId *npId, const NpImpsGamePresenceDetail *detail, int platform)
{
	NpImpsWirePlatformFriendPresence msg;
	msg.kind = NP_IMPS_EVT_PLATFORM_FRIEND_PRESENCE;
	msg.npId = *npId;
	msg.detail = *detail;
	msg.platform = platform;

	m_mutex.Lock();
	BroadcastLocked(0, NP_IMPS_EVT_PLATFORM_FRIEND_PRESENCE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F3368 */
void NpImpsUtilSessionHolder::SendPlatformFriendPresenceToProcess(
	SceUID processId, const SceNpId *npId,
	const NpImpsGamePresenceDetail *detail, int platform)
{
	NpImpsWirePlatformFriendPresence msg;
	msg.kind = NP_IMPS_EVT_PLATFORM_FRIEND_PRESENCE;
	msg.npId = *npId;
	msg.detail = *detail;
	msg.platform = platform;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_PLATFORM_FRIEND_PRESENCE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F342E */
void NpImpsUtilSessionHolder::SendMyPartyPresenceToSession(IPMI::Session *session,
							   const NpImpsPartyPresence *presence)
{
	NpImpsWireMyPartyPresence msg;
	msg.kind = NP_IMPS_EVT_MY_PARTY_PRESENCE;
	msg.presence = *presence;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_MY_PARTY_PRESENCE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F34DC */
void NpImpsUtilSessionHolder::BroadcastMyPartyPresence(const NpImpsPartyPresence *presence)
{
	NpImpsWireMyPartyPresence msg;
	msg.kind = NP_IMPS_EVT_MY_PARTY_PRESENCE;
	msg.presence = *presence;

	m_mutex.Lock();
	BroadcastLocked(0, NP_IMPS_EVT_MY_PARTY_PRESENCE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F3550 */
void NpImpsUtilSessionHolder::SendMyPartyPresenceToProcess(SceUID processId,
							   const NpImpsPartyPresence *presence)
{
	NpImpsWireMyPartyPresence msg;
	msg.kind = NP_IMPS_EVT_MY_PARTY_PRESENCE;
	msg.presence = *presence;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_MY_PARTY_PRESENCE, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F35D2 */
void NpImpsUtilSessionHolder::SendFriendRequestToSession(IPMI::Session *session, int action,
							 const SceNpId *npId)
{
	NpImpsWireNpIdAction msg;
	msg.kind = NP_IMPS_EVT_FRIEND_REQUEST;
	msg.action = action;
	msg.npId = *npId;

	m_mutex.Lock();
	TrySendToSessionLocked(session, 0, NP_IMPS_EVT_FRIEND_REQUEST, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* 0x812F369A */
void NpImpsUtilSessionHolder::SendFriendRequestToProcess(SceUID processId, int action,
							 const SceNpId *npId)
{
	NpImpsWireNpIdAction msg;
	msg.kind = NP_IMPS_EVT_FRIEND_REQUEST;
	msg.action = action;
	msg.npId = *npId;

	m_mutex.Lock();
	SendToProcessLocked(processId, 0, NP_IMPS_EVT_FRIEND_REQUEST, &msg, sizeof(msg));
	m_mutex.Unlock();
}

/* ============================================================================
 * Async call reply writers (0x812F37B2..0x812F3983)
 * ============================================================================ */

int npImpsUtilSendEnterOnlineReply(IPMI::Session *session, SceUInt32 requestId,
				   int status, const int *resultCode)
{
	IPMI::DataInfo outInfo;
	outInfo.pData = resultCode;
	outInfo.dataSize = sizeof(*resultCode);
	return session->respondToAsyncMethodRequest(NP_IMPS_METHOD_ENTER_ONLINE_RESULT, requestId,
						    status, &outInfo, 1);
}

int npImpsUtilSendRosterReply(IPMI::Session *session, SceUInt32 methodId,
			      SceUInt32 requestId, int status, const SceUInt32 *contextId,
			      const SceNpId *npId, const int *resultCode)
{
	IPMI::DataInfo outInfos[3];
	outInfos[0].pData = contextId;
	outInfos[0].dataSize = sizeof(*contextId);
	outInfos[1].pData = npId;
	outInfos[1].dataSize = sizeof(*npId);
	outInfos[2].pData = resultCode;
	outInfos[2].dataSize = sizeof(*resultCode);
	return session->respondToAsyncMethodRequest(methodId, requestId, status, outInfos, 3);
}

/* ============================================================================
 * AsyncCallRelayMgr ("SceNpImpsUtilAsyncCallRelayMgr", 0x812F4124..0x812F4BF3)
 * ============================================================================ */

static NpImpsUtilAsyncRelayMgr *s_asyncRelayMgr = NULL;		/* 0x815460F4 */

NpImpsUtilAsyncRelayMgr::NpImpsUtilAsyncRelayMgr()
	: m_mutex("SceNpImpsUtilAsyncCallRelayMgr", 0),
	  m_procContexts(NULL),
	  m_ctxRelays(NULL),
	  m_procOpRelays(NULL)
{
}

NpImpsUtilAsyncRelayMgr::~NpImpsUtilAsyncRelayMgr()
{
	m_mutex.Lock();
	while (m_procOpRelays != NULL) {
		ProcessOpRelayNode *next = m_procOpRelays->next;
		npImpsUtilFree(m_procOpRelays);
		m_procOpRelays = next;
	}
	while (m_ctxRelays != NULL) {
		ContextRelayNode *next = m_ctxRelays->next;
		npImpsUtilFree(m_ctxRelays);
		m_ctxRelays = next;
	}
	while (m_procContexts != NULL) {
		ProcessContextNode *next = m_procContexts->next;
		npImpsUtilFree(m_procContexts);
		m_procContexts = next;
	}
	m_mutex.Unlock();
}

int NpImpsUtilAsyncRelayMgr::Create()
{
	if (s_asyncRelayMgr != NULL)
		return NP_IMPS_UTIL_ERROR_ALREADY_INITIALIZED;

	void *mem = npImpsUtilMalloc(sizeof(NpImpsUtilAsyncRelayMgr));
	if (mem == NULL)
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;

	s_asyncRelayMgr = new (mem) NpImpsUtilAsyncRelayMgr();
	return 0;
}

int NpImpsUtilAsyncRelayMgr::Destroy()
{
	if (s_asyncRelayMgr == NULL)
		return NP_IMPS_UTIL_ERROR_NOT_INITIALIZED;

	s_asyncRelayMgr->~NpImpsUtilAsyncRelayMgr();
	npImpsUtilFree(s_asyncRelayMgr);
	s_asyncRelayMgr = NULL;
	return 0;
}

NpImpsUtilAsyncRelayMgr *NpImpsUtilAsyncRelayMgr::GetInstance()
{
	return s_asyncRelayMgr;
}

/* 0x812F42A4 */
void NpImpsUtilAsyncRelayMgr::CleanupProcessContexts(SceUID processId)
{
	m_mutex.Lock();
	ProcessContextNode **link = &m_procContexts;
	while (*link != NULL) {
		if ((*link)->processId == processId) {
			ProcessContextNode *victim = *link;
			*link = victim->next;
			npImpsSvcAbortContext(victim->contextId);
			npImpsSvcDestroyContext(victim->contextId);
			npImpsUtilFree(victim);
		} else {
			link = &(*link)->next;
		}
	}
	m_mutex.Unlock();
}

/* 0x812F4392 */
int NpImpsUtilAsyncRelayMgr::AddProcessContext(SceUID processId, SceUInt32 contextId)
{
	m_mutex.Lock();
	for (ProcessContextNode *cur = m_procContexts; cur != NULL; cur = cur->next) {
		if (cur->processId == processId && cur->contextId == contextId) {
			m_mutex.Unlock();
			return NP_IMPS_UTIL_ERROR_INTERNAL;
		}
	}

	ProcessContextNode *node =
		static_cast<ProcessContextNode *>(npImpsUtilMalloc(sizeof(ProcessContextNode)));
	if (node == NULL) {
		m_mutex.Unlock();
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	node->processId = processId;
	node->contextId = contextId;
	node->next = m_procContexts;
	m_procContexts = node;
	m_mutex.Unlock();
	return 0;
}

/* 0x812F4562 */
int NpImpsUtilAsyncRelayMgr::RemoveProcessContext(SceUID processId, SceUInt32 contextId)
{
	m_mutex.Lock();
	ProcessContextNode **link = &m_procContexts;
	while (*link != NULL) {
		if ((*link)->processId == processId && (*link)->contextId == contextId) {
			ProcessContextNode *victim = *link;
			*link = victim->next;
			npImpsUtilFree(victim);
			m_mutex.Unlock();
			return 0;
		}
		link = &(*link)->next;
	}
	m_mutex.Unlock();
	return NP_IMPS_UTIL_ERROR_INTERNAL;
}

/* 0x812F4692 */
int NpImpsUtilAsyncRelayMgr::RegisterContextRelay(SceUInt32 contextId,
						  const NpImpsAsyncRelayRecord *record)
{
	m_mutex.Lock();
	ContextRelayNode *node =
		static_cast<ContextRelayNode *>(npImpsUtilMalloc(sizeof(ContextRelayNode)));
	if (node == NULL) {
		m_mutex.Unlock();
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	node->contextId = contextId;
	node->record = *record;
	node->next = m_ctxRelays;
	m_ctxRelays = node;
	m_mutex.Unlock();
	return 0;
}

/* 0x812F4706 */
int NpImpsUtilAsyncRelayMgr::UnregisterContextRelay(SceUInt32 contextId)
{
	m_mutex.Lock();
	ContextRelayNode **link = &m_ctxRelays;
	while (*link != NULL) {
		if ((*link)->contextId == contextId) {
			ContextRelayNode *victim = *link;
			*link = victim->next;
			npImpsUtilFree(victim);
			m_mutex.Unlock();
			return 0;
		}
		link = &(*link)->next;
	}
	m_mutex.Unlock();
	return NP_IMPS_UTIL_ERROR_INTERNAL;
}

/* 0x812F4790 */
int NpImpsUtilAsyncRelayMgr::LookupContextRelay(SceUInt32 contextId,
						NpImpsAsyncRelayRecord *outRecord)
{
	m_mutex.Lock();
	for (ContextRelayNode *cur = m_ctxRelays; cur != NULL; cur = cur->next) {
		if (cur->contextId == contextId) {
			*outRecord = cur->record;
			m_mutex.Unlock();
			return 0;
		}
	}
	m_mutex.Unlock();
	return NP_IMPS_UTIL_ERROR_INTERNAL;
}

/* 0x812F4814 */
int NpImpsUtilAsyncRelayMgr::RegisterProcessOpRelay(SceUID processId, SceUIntPtr opKey,
						    const NpImpsAsyncRelayRecord *record)
{
	m_mutex.Lock();
	for (ProcessOpRelayNode *cur = m_procOpRelays; cur != NULL; cur = cur->next) {
		if (cur->processId == processId && cur->opKey == opKey) {
			m_mutex.Unlock();
			return NP_IMPS_UTIL_ERROR_ALREADY_IN_PROGRESS;
		}
	}

	ProcessOpRelayNode *node =
		static_cast<ProcessOpRelayNode *>(npImpsUtilMalloc(sizeof(ProcessOpRelayNode)));
	if (node == NULL) {
		m_mutex.Unlock();
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	node->processId = processId;
	node->opKey = opKey;
	node->record = *record;
	node->next = m_procOpRelays;
	m_procOpRelays = node;
	m_mutex.Unlock();
	return 0;
}

/* 0x812F4A14 */
int NpImpsUtilAsyncRelayMgr::UnregisterProcessOpRelay(SceUID processId, SceUIntPtr opKey)
{
	m_mutex.Lock();
	ProcessOpRelayNode **link = &m_procOpRelays;
	while (*link != NULL) {
		if ((*link)->processId == processId && (*link)->opKey == opKey) {
			ProcessOpRelayNode *victim = *link;
			*link = victim->next;
			npImpsUtilFree(victim);
			m_mutex.Unlock();
			return 0;
		}
		link = &(*link)->next;
	}
	m_mutex.Unlock();
	return NP_IMPS_UTIL_ERROR_INTERNAL;
}

/* 0x812F4B0C */
int NpImpsUtilAsyncRelayMgr::LookupProcessOpRelay(SceUID processId, SceUIntPtr opKey,
						  NpImpsAsyncRelayRecord *outRecord)
{
	m_mutex.Lock();
	for (ProcessOpRelayNode *cur = m_procOpRelays; cur != NULL; cur = cur->next) {
		if (cur->processId == processId && cur->opKey == opKey) {
			*outRecord = cur->record;
			m_mutex.Unlock();
			return 0;
		}
	}
	m_mutex.Unlock();
	return NP_IMPS_UTIL_ERROR_INTERNAL;
}

/* ============================================================================
 * SceNpImpsSvc global listener callbacks for system-app sessions (0x812F0CD0..0x812F1107)
 * ============================================================================ */

/* 0x812F0CD0 */
static void onGlobalServiceState(int state, int errorCode, void *userArg)
{
	NpImpsUtilServerContext *ctx = static_cast<NpImpsUtilServerContext *>(userArg);
	if (ctx == NULL || ctx->mpState == NULL)
		return;

	int mpEvent = 0;
	if (state == NP_IMPS_SVC_STATE_SIGNED_OUT)
		mpEvent = NP_IMPS_MP_EVT_SIGN_OUT;
	else if (state == NP_IMPS_SVC_STATE_SIGNED_IN)
		mpEvent = NP_IMPS_MP_EVT_SIGN_IN;
	else if (state == NP_IMPS_SVC_STATE_ONLINE)
		mpEvent = NP_IMPS_MP_EVT_CORE_ONLINE;

	if (mpEvent != 0)
		npImpsUtilMpStatePostEvent(ctx->mpState, mpEvent, 0, errorCode);
}

/* 0x812F0CFC */
static void onGlobalFriendList(int eventType, const SceNpId *npIds,
			       NpImpsCacheFriendPtr **handles, SceSize count, void *userArg)
{
	(void)npIds;
	(void)userArg;
	if (s_systemSessionHolder == NULL || handles == NULL)
		return;

	if (eventType == 1 || eventType == 2 || eventType == 4) {
		for (SceSize idx = 0; idx < count; ++idx) {
			int relation = 0;
			SceUInt8 isNew = 0;
			SceNpId friendNpId;
			if (npImpsSvcGetFriendEntryRelation(handles[idx], &relation) >= 0 &&
			    npImpsSvcGetFriendEntryIsNew(handles[idx], &isNew) >= 0 &&
			    npImpsSvcGetFriendEntryNpId(handles[idx], &friendNpId) >= 0 &&
			    isNew != 0 && relation == 3) {
				s_systemSessionHolder->BroadcastFriendList(3, &friendNpId);
			}
		}
	}
}

/* 0x812F0D72 */
static void onGlobalFriendRemoved(const SceNpId *npIds, SceSize count, void *userArg)
{
	(void)userArg;
	if (s_systemSessionHolder == NULL)
		return;
	for (SceSize idx = 0; idx < count; ++idx)
		s_systemSessionHolder->BroadcastFriendList(4, &npIds[idx]);
}

/* 0x812F0D90 */
static void onGlobalMyOnlineStatus(int status, void *userArg)
{
	(void)userArg;
	if (s_systemSessionHolder != NULL)
		s_systemSessionHolder->BroadcastMyOnlineStatus(status);
}

/* 0x812F0D9E */
static void onGlobalMyPresence(int eventType, const NpImpsSvcPresenceSnapshot *curSnap,
			       const NpImpsSvcPresenceSnapshot *prevSnap, void *userArg)
{
	(void)userArg;
	if (s_systemSessionHolder == NULL)
		return;

	SceUInt8 gameChanged = 0;
	SceUInt8 partyChanged = 0;
	npImpsUtilDiffMyPresence(eventType, curSnap, prevSnap, &gameChanged, &partyChanged);

	if (gameChanged != 0) {
		NpImpsGamePresenceDetail detail;
		npImpsUtilFillGamePresenceDetail(&detail, curSnap);
		s_systemSessionHolder->BroadcastMyPresenceDetail(&detail);
	}
	if (partyChanged != 0) {
		NpImpsPartyPresence party;
		npImpsUtilFillPartyPresence(&party, curSnap);
		s_systemSessionHolder->BroadcastMyPartyPresence(&party);
	}
}

/* 0x812F0FEE */
static void onGlobalFriendPresence(int eventType, NpImpsCacheFriendPtr *handle, const SceNpId *npId,
				   const NpImpsSvcPresenceSnapshot *curSnap,
				   const NpImpsSvcPresenceSnapshot *prevSnap,
				   SceUInt32 diffMask, void *userArg)
{
	(void)eventType;
	(void)handle;
	(void)npId;
	(void)curSnap;
	(void)prevSnap;
	(void)diffMask;
	(void)userArg;
}

/* 0x812F1010 */
static void onGlobalPlatformFriendPresence(int eventType, NpImpsCacheFriendPtr *handle,
					   const SceNpId *npId,
					   const NpImpsSvcPresenceSnapshot *curSnap,
					   const NpImpsSvcPresenceSnapshot *prevSnap,
					   SceUInt32 diffMask, int platform, void *userArg)
{
	(void)handle;
	(void)prevSnap;
	(void)userArg;
	if (s_systemSessionHolder == NULL)
		return;

	bool shouldSend = false;
	if (eventType == 1) {
		shouldSend = true;
	} else if (eventType == 2 &&
		   ((diffMask & (NP_IMPS_DIFF_TITLE | NP_IMPS_DIFF_GAME_PRESENCE |
				 NP_IMPS_DIFF_CONTENT_INFO)) != 0)) {
		shouldSend = true;
	}

	if (shouldSend) {
		NpImpsGamePresenceDetail detail;
		npImpsUtilFillGamePresenceDetail(&detail, curSnap);
		s_systemSessionHolder->BroadcastPlatformFriendPresence(npId, &detail, platform);
	}
}

/* 0x812F10D0 */
static void onGlobalFriendContext(int eventType, NpImpsCacheFriendPtr *handle, const SceNpId *npId,
				  int contextState, void *extra, void *userArg)
{
	(void)eventType;
	(void)handle;
	(void)extra;
	(void)userArg;
	if (s_systemSessionHolder != NULL)
		s_systemSessionHolder->BroadcastFriendContext(contextState, npId);
}

/* ============================================================================
 * IPMI Server callbacks (0x812F1122, 0x812F3984, 0x812F407C)
 * ============================================================================ */

/* 0x812F1122 */
static void onIpmiSessionClosed(IPMI::Session *session)
{
	SceUID processId = session->getProcessId();
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	if (relayMgr != NULL)
		relayMgr->CleanupProcessContexts(processId);

	if (s_serverCtx != NULL) {
		if (s_serverCtx->gameSessions != NULL)
			s_serverCtx->gameSessions->UnregisterSession(session);
		if (s_serverCtx->systemSessions != NULL)
			s_serverCtx->systemSessions->UnregisterSession(session);
	}

	npImpsUtilHandleSessionClosedCleanup(processId);

	if (s_rawMsgMgr != NULL && s_rawMsgMgr->HasSession(session)) {
		int channelId = 0;
		if (s_rawMsgMgr->GetSessionChannel(session, &channelId) >= 0) {
			const NpImpsRawMsgServerCallbacks *cb = s_rawMsgMgr->FindChannel(channelId);
			if (cb != NULL && cb->onClientDisconnected != NULL)
				cb->onClientDisconnected(session);
			s_rawMsgMgr->UnbindSession(session);
		}
	}
}

/* 0x812F3984 */
static int onIpmiSyncMethod(IPMI::Session *session, SceUInt32 methodId,
			    const IPMI::DataInfo *inList, SceUInt32 inCount,
			    IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	int ret = 0;

	switch (methodId) {
	case NP_IMPS_METHOD_REGISTER_GAME_EVENT:
		ret = npImpsUtilHandleRegisterGameEvents(
			session, static_cast<const SceUInt8 *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_UNREGISTER_GAME_EVENT:
		ret = npImpsUtilHandleUnregisterGameEvents(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_SERVICE_STATE:
		ret = npImpsUtilHandleGetServiceState(session, static_cast<int *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_LEAVE_ONLINE:
		ret = npImpsUtilHandleLeaveOnline(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_CONNECT_CORE:
		ret = npImpsUtilHandleConnectCore(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_ACCEPTED_FRIEND_CNT:
		ret = npImpsUtilHandleGetAcceptedFriendCount(
			session, static_cast<SceUInt32 *>(outList[0].pBuffer),
			*static_cast<const SceUInt8 *>(outList[1].pBuffer));
		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_ACCEPTED_FRIEND_LIST: {
		const SceUInt32 *rangeReq = static_cast<const SceUInt32 *>(inList[0].pData);
		SceUInt32 maxCount = rangeReq[1];
		ret = npImpsUtilHandleGetAcceptedFriendList(
			session, rangeReq[0], static_cast<SceNpId *>(outList[0].pBuffer),
			maxCount, static_cast<SceUInt32 *>(outList[1].pBuffer),
			*static_cast<const SceUInt8 *>(outList[2].pBuffer));
		outList[0].outSize = sizeof(SceNpId) * maxCount;
		outList[1].outSize = sizeof(SceUInt32);
		outList[2].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_IMPS_METHOD_GET_FRIEND_PRESENCE_GAME:
		ret = npImpsUtilHandleGetFriendPresenceGame(
			session, static_cast<const SceNpId *>(inList[0].pData),
			static_cast<NpImpsSvcPresenceSnapshot *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(NpImpsSvcPresenceSnapshot);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_BLOCK_LIST_COUNT:
		ret = npImpsUtilHandleGetBlockListCount(
			session, static_cast<SceUInt32 *>(outList[0].pBuffer),
			*static_cast<const SceUInt8 *>(outList[1].pBuffer));
		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_BLOCK_LIST_ENTRIES: {
		const SceUInt32 *rangeReq = static_cast<const SceUInt32 *>(inList[0].pData);
		SceUInt32 maxCount = rangeReq[1];
		ret = npImpsUtilHandleGetBlockListEntries(
			session, rangeReq[0], static_cast<SceNpId *>(outList[0].pBuffer),
			maxCount, static_cast<SceUInt32 *>(outList[1].pBuffer),
			*static_cast<const SceUInt8 *>(outList[2].pBuffer));
		outList[0].outSize = sizeof(SceNpId) * maxCount;
		outList[1].outSize = sizeof(SceUInt32);
		outList[2].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_IMPS_METHOD_IS_BLOCKED:
		ret = npImpsUtilHandleIsBlocked(
			session, static_cast<const SceNpId *>(inList[0].pData),
			static_cast<bool *>(outList[0].pBuffer),
			*static_cast<const SceUInt8 *>(outList[1].pBuffer));
		outList[0].outSize = sizeof(SceUInt8);
		outList[1].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_SET_PARTY_PRESENCE:
		ret = npImpsUtilHandleSetPartyPresence(
			session, static_cast<const NpImpsPartyPresence *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_CLEAR_PARTY_PRESENCE:
		ret = npImpsUtilHandleClearPartyPresence(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_SET_COMM_ID:
		ret = npImpsUtilHandleSetCommId(
			session, static_cast<const SceNpCommunicationId *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_CLEAR_COMM_ID:
		ret = npImpsUtilHandleClearCommId(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_SET_GAME_PRESENCE:
		ret = npImpsUtilHandleSetGamePresence(
			session, static_cast<const NpImpsGamePresenceInput *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_CLEAR_GAME_PRESENCE:
		ret = npImpsUtilHandleClearGamePresence(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_SEND_IN_GAME_DATA:
		ret = npImpsUtilHandleSendInGameData(
			session, static_cast<const SceNpId *>(inList[1].pData),
			inList[2].pData,
			static_cast<const SceNpCommunicationId *>(inList[3].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_ADD_PLAY_LOG:
		ret = npImpsUtilHandleAddPlayLog(
			session, static_cast<const paf::rtc::Tick *>(inList[0].pData),
			static_cast<const SceNpId *>(inList[1].pData),
			static_cast<const SceNpCommunicationId *>(inList[2].pData),
			static_cast<const char *>(inList[3].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_PLAY_LOG_COUNT:
		ret = npImpsUtilHandleGetPlayLogCount(
			session, *static_cast<const int *>(inList[0].pData),
			static_cast<const SceNpCommunicationId *>(inList[1].pData),
			static_cast<SceUInt32 *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceUInt32);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_PLAY_LOG_LIST: {
		const SceUInt32 *logReq = static_cast<const SceUInt32 *>(inList[0].pData);
		SceUInt32 maxCount = logReq[2];
		ret = npImpsUtilHandleGetPlayLogList(
			session, static_cast<int>(logReq[0]),
			static_cast<const SceNpCommunicationId *>(inList[1].pData),
			logReq[1], outList[0].pBuffer, maxCount,
			static_cast<SceUInt32 *>(outList[1].pBuffer));
		outList[0].outSize = 696u * maxCount;
		outList[1].outSize = sizeof(SceUInt32);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_IMPS_METHOD_CREATE_ROSTER_CTX:
		ret = npImpsUtilHandleCreateRosterCtx(
			session, static_cast<SceUInt32 *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceUInt32);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_DESTROY_ROSTER_CTX:
		ret = npImpsUtilHandleDestroyRosterCtx(
			session, static_cast<const SceUInt32 *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_ABORT_ROSTER_CTX:
		ret = npImpsUtilHandleAbortRosterCtx(
			session, static_cast<const SceUInt32 *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_WAIT_ROSTER_CTX:
		ret = npImpsUtilHandleWaitRosterCtx(
			session, static_cast<const SceUInt32 *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_ALL_FRIEND_COUNT:
		ret = npImpsUtilHandleGetAllFriendCount(
			session, static_cast<SceUInt32 *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceUInt32);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_ALL_FRIEND_LIST: {
		const SceUInt32 *rangeReq = static_cast<const SceUInt32 *>(inList[0].pData);
		SceUInt32 maxCount = rangeReq[1];
		ret = npImpsUtilHandleGetAllFriendList(
			session, rangeReq[0], static_cast<SceNpId *>(outList[0].pBuffer),
			maxCount, static_cast<SceUInt32 *>(outList[1].pBuffer));
		outList[0].outSize = sizeof(SceNpId) * maxCount;
		outList[1].outSize = sizeof(SceUInt32);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_IMPS_METHOD_IS_ONLINE:
		ret = npImpsUtilHandleIsOnline(session, static_cast<SceUInt8 *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_FRIEND_CONTEXT_SYS:
		ret = npImpsUtilHandleGetFriendContextSys(
			session, static_cast<const SceNpId *>(inList[1].pData),
			*static_cast<const int *>(inList[0].pData),
			static_cast<int *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_FRIEND_PRESENCE_SYS:
	case NP_IMPS_METHOD_GET_FRIEND_PRESENCE_ALT:
		ret = npImpsUtilHandleGetFriendPresenceSys(
			session, static_cast<const SceNpId *>(inList[0].pData),
			static_cast<NpImpsSvcPresenceSnapshot *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(NpImpsSvcPresenceSnapshot);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_DELETE_PLAY_LOG: {
		const SceUInt32 *tickParts = static_cast<const SceUInt32 *>(inList[0].pData);
		ret = npImpsUtilHandleDeletePlayLog(session, tickParts[0], tickParts[1]);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_IMPS_METHOD_DELETE_ALL_PLAY_LOG:
		ret = npImpsUtilHandleDeleteAllPlayLog(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_FRIEND_CONTEXT_GAME:
		ret = npImpsUtilHandleGetFriendContextGame(
			session, static_cast<const SceNpId *>(inList[0].pData),
			static_cast<int *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_FRIEND_GAME_PRESENCE:
		ret = npImpsUtilHandleGetFriendGamePresence(
			session, static_cast<const SceNpId *>(inList[0].pData),
			static_cast<NpImpsGamePresenceWithTitle *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(NpImpsGamePresenceWithTitle);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_REGISTER_SYS_EVENT:
		ret = npImpsUtilHandleRegisterSystemEvents(
			session, static_cast<const SceUInt8 *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_UNREGISTER_SYS_EVENT:
		ret = npImpsUtilHandleUnregisterSystemEvents(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_DISCONNECT_CORE:
		ret = npImpsUtilHandleDisconnectCore(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_PENDING_FRIEND_CNT:
		ret = npImpsUtilHandleGetPendingFriendCount(
			session, static_cast<SceUInt32 *>(outList[0].pBuffer),
			*static_cast<const SceUInt8 *>(outList[1].pBuffer));
		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_PENDING_FRIEND_LIST: {
		const SceUInt32 *rangeReq = static_cast<const SceUInt32 *>(inList[0].pData);
		SceUInt32 maxCount = rangeReq[1];
		ret = npImpsUtilHandleGetPendingFriendList(
			session, rangeReq[0], static_cast<SceNpId *>(outList[0].pBuffer),
			maxCount, static_cast<SceUInt32 *>(outList[1].pBuffer),
			*static_cast<const SceUInt8 *>(outList[2].pBuffer));
		outList[0].outSize = sizeof(SceNpId) * maxCount;
		outList[1].outSize = sizeof(SceUInt32);
		outList[2].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_IMPS_METHOD_GET_REQ_FRIEND_CNT:
		ret = npImpsUtilHandleGetRequestedFriendCount(
			session, static_cast<SceUInt32 *>(outList[0].pBuffer),
			*static_cast<const SceUInt8 *>(outList[1].pBuffer));
		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_REQ_FRIEND_LIST: {
		const SceUInt32 *rangeReq = static_cast<const SceUInt32 *>(inList[0].pData);
		SceUInt32 maxCount = rangeReq[1];
		ret = npImpsUtilHandleGetRequestedFriendList(
			session, rangeReq[0], static_cast<SceNpId *>(outList[0].pBuffer),
			maxCount, static_cast<SceUInt32 *>(outList[1].pBuffer),
			*static_cast<const SceUInt8 *>(outList[2].pBuffer));
		outList[0].outSize = sizeof(SceNpId) * maxCount;
		outList[1].outSize = sizeof(SceUInt32);
		outList[2].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_IMPS_METHOD_RAW_MSG_OPEN:
		ret = handleRawMsgOpen(session, *static_cast<const int *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_RAW_MSG_CLOSE:
		ret = handleRawMsgClose(session, *static_cast<const int *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_RAW_MSG_SEND: {
		const SceUInt32 *hdr = static_cast<const SceUInt32 *>(inList[0].pData);
		ret = handleRawMsgSend(session, static_cast<int>(hdr[0]), inList[1].pData, hdr[1]);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case NP_IMPS_METHOD_GET_RAW_SERVICE_STATE:
		ret = npImpsUtilHandleGetRawServiceState(
			session, static_cast<int *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_IS_GLOBAL_ONLINE:
		ret = npImpsUtilHandleIsGlobalOnline(
			session, static_cast<SceUInt8 *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceUInt8);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_FORCE_ROSTER_SYNC:
		ret = npImpsUtilHandleForceRosterSync(
			session, *static_cast<const int *>(inList[0].pData));
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_MY_STATUS_PRESENCE:
		ret = npImpsUtilHandleGetMyStatusAndPresence(
			session, static_cast<int *>(outList[0].pBuffer),
			static_cast<NpImpsSvcPresenceSnapshot *>(outList[1].pBuffer));
		outList[0].outSize = sizeof(int);
		outList[1].outSize = sizeof(NpImpsSvcPresenceSnapshot);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_PLAYER_PLAY_LOG_CNT:
		ret = npImpsUtilHandleGetPlayerPlayLogCount(
			session, static_cast<const SceNpId *>(inList[0].pData),
			static_cast<SceUInt32 *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceUInt32);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case NP_IMPS_METHOD_GET_PLAYER_PLAY_LOG:
		ret = npImpsUtilHandleGetPlayerPlayLog(
			session, static_cast<const SceNpId *>(inList[1].pData),
			*static_cast<const SceUInt32 *>(inList[0].pData),
			outList[0].pBuffer);
		outList[0].outSize = 712u;
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	default:
		return NP_IMPS_IPMI_ERROR_INVALID_METHOD;
	}
}

/* 0x812F407C */
static int onIpmiAsyncMethod(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
			     const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)inCount;

	switch (methodId) {
	case NP_IMPS_METHOD_ENTER_ONLINE_ASYNC:
		npImpsUtilHandleEnterOnlineAsync(
			session, requestId, *static_cast<const SceUInt8 *>(inList[0].pData));
		return 0;

	case NP_IMPS_METHOD_REQUEST_FRIEND_ASYNC:
		return npImpsUtilHandleRequestFriendAsync(
			session, requestId,
			static_cast<const SceUInt32 *>(inList[1].pData),
			static_cast<const SceNpId *>(inList[2].pData),
			static_cast<const char *>(inList[3].pData));

	case NP_IMPS_METHOD_ACCEPT_FRIEND_ASYNC:
		return npImpsUtilHandleAcceptFriendAsync(
			session, requestId,
			static_cast<const SceUInt32 *>(inList[0].pData),
			static_cast<const SceNpId *>(inList[1].pData));

	case NP_IMPS_METHOD_DELETE_FRIEND_ASYNC:
		return npImpsUtilHandleDeleteFriendAsync(
			session, requestId,
			static_cast<const SceUInt32 *>(inList[0].pData),
			static_cast<const SceNpId *>(inList[1].pData));

	case NP_IMPS_METHOD_ADD_BLOCK_ASYNC:
		return npImpsUtilHandleAddBlockAsync(
			session, requestId,
			static_cast<const SceUInt32 *>(inList[0].pData),
			static_cast<const SceNpId *>(inList[1].pData));

	case NP_IMPS_METHOD_DELETE_BLOCK_ASYNC:
		return npImpsUtilHandleDeleteBlockAsync(
			session, requestId,
			static_cast<const SceUInt32 *>(inList[0].pData),
			static_cast<const SceNpId *>(inList[1].pData));

	default:
		return NP_IMPS_IPMI_ERROR_INVALID_METHOD;
	}
}

/* ============================================================================
 * Server creation, lifecycle, and process hooks (0x812F0510..0x812F1121)
 * ============================================================================ */

/* 0x812F0510 */
static int createSessionHolders(NpImpsUtilServerContext *ctx)
{
	void *gameStratMem = npImpsUtilMalloc(sizeof(NpImpsGameQueueOverflowStrategy));
	if (gameStratMem == NULL)
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	NpImpsGameQueueOverflowStrategy *gameStrat =
		new (gameStratMem) NpImpsGameQueueOverflowStrategy();

	void *gameHolderMem = npImpsUtilMalloc(sizeof(NpImpsUtilSessionHolder));
	if (gameHolderMem == NULL) {
		gameStrat->~NpImpsGameQueueOverflowStrategy();
		npImpsUtilFree(gameStrat);
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	ctx->gameSessions = new (gameHolderMem) NpImpsUtilSessionHolder(gameStrat);

	void *sysStratMem = npImpsUtilMalloc(sizeof(NpImpsSystemQueueOverflowStrategy));
	if (sysStratMem == NULL) {
		ctx->gameSessions->~NpImpsUtilSessionHolder();
		npImpsUtilFree(ctx->gameSessions);
		ctx->gameSessions = NULL;
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	NpImpsSystemQueueOverflowStrategy *sysStrat =
		new (sysStratMem) NpImpsSystemQueueOverflowStrategy();

	void *sysHolderMem = npImpsUtilMalloc(sizeof(NpImpsUtilSessionHolder));
	if (sysHolderMem == NULL) {
		sysStrat->~NpImpsSystemQueueOverflowStrategy();
		npImpsUtilFree(sysStrat);
		ctx->gameSessions->~NpImpsUtilSessionHolder();
		npImpsUtilFree(ctx->gameSessions);
		ctx->gameSessions = NULL;
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	ctx->systemSessions = new (sysHolderMem) NpImpsUtilSessionHolder(sysStrat);
	return 0;
}

/* 0x812F0638 */
static int startIpmiServer(NpImpsUtilServerContext *ctx)
{
	IpmiWrapper::Server::Config cfg;
	sce_paf_memset(&cfg, 0, sizeof(cfg));
	sce_paf_strncpy(cfg.name, "SceNpImpsUtil", sizeof(cfg.name));
	cfg.threadPriority = 80;
	cfg.threadStackSize = 0x2000;
	cfg.maxSessions = 16;
	cfg.syncCallCount = 1;
	cfg.maxSyncCallInSize = 3840;
	cfg.maxSyncCallOutSize = 3840;
	cfg.asyncCallCount = 1;
	cfg.maxAsyncCallInSize = 3840;
	cfg.maxAsyncCallOutSize = 3840;
	cfg.callbackCount = 5;
	cfg.msgQueueCount = 3;
	cfg.msgQueue[0].msgSize = 1024;
	cfg.msgQueue[0].msgCount = 256;
	cfg.msgQueue[1].msgSize = 64;
	cfg.msgQueue[1].msgCount = 16;
	cfg.msgQueue[2].msgSize = 1;
	cfg.msgQueue[2].msgCount = 1;
	cfg.callbacks.onSyncMethod = onIpmiSyncMethod;
	cfg.callbacks.onAsyncMethod = onIpmiAsyncMethod;
	cfg.callbacks.onSessionKilled = onIpmiSessionClosed;
	cfg.budget = 2;

	SceSize bufSize = IpmiWrapper::Server::getBufferSize(&cfg);
	void *serverMem = npImpsUtilMalloc(bufSize);
	if (serverMem == NULL)
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;

	int ret = IpmiWrapper::Server::create(&ctx->ipmiServer, &cfg, serverMem);
	if (ret >= 0) {
		if (ctx->ipmiServer == serverMem) {
			ret = ctx->ipmiServer->start();
			if (ret >= 0)
				return 0;
			ctx->ipmiServer->destroy();
		} else {
			ret = NP_IMPS_UTIL_ERROR_INTERNAL;
		}
	}

	npImpsUtilFree(serverMem);
	ctx->ipmiServer = NULL;
	return ret;
}

/* 0x812F0794 */
int npImpsUtilInit(void)
{
	if (s_serverCtx != NULL)
		return NP_IMPS_UTIL_ERROR_ALREADY_INITIALIZED;

	npImpsUtilHeapInit();

	NpImpsUtilServerContext *ctx =
		static_cast<NpImpsUtilServerContext *>(npImpsUtilMalloc(sizeof(*ctx)));
	if (ctx == NULL) {
		npImpsUtilHeapTerm();
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	sce_paf_memset(ctx, 0, sizeof(*ctx));

	int ret = startIpmiServer(ctx);
	if (ret < 0) {
		npImpsUtilFree(ctx);
		npImpsUtilHeapTerm();
		return ret;
	}

	ret = createSessionHolders(ctx);
	if (ret >= 0) {
		ret = NpImpsUtilAsyncRelayMgr::Create();
		if (ret >= 0) {
			NpImpsSvcListenerCallbacks cb;
			sce_paf_memset(&cb, 0, sizeof(cb));
			cb.onServiceState = onGlobalServiceState;
			cb.onMyOnlineStatus = onGlobalMyOnlineStatus;
			cb.onFriendList = onGlobalFriendList;
			cb.onFriendPresence = onGlobalFriendPresence;
			cb.onFriendRemoved = onGlobalFriendRemoved;
			cb.onMyPresence = onGlobalMyPresence;
			cb.onPlatformFriendPresence = onGlobalPlatformFriendPresence;
			cb.onFriendContext = onGlobalFriendContext;
			cb.userArg = ctx;

			ret = npImpsSvcRegisterListener(&cb, &ctx->svcListenerId);
			if (ret >= 0) {
				ret = npImpsUtilMpStateStart();
				if (ret >= 0) {
					ret = rawMsgMgrInit();
					if (ret >= 0) {
						ctx->mpState = npImpsUtilGetMpState();
						s_gameSessionHolder = ctx->gameSessions;
						s_systemSessionHolder = ctx->systemSessions;
						s_ipmiServer = ctx->ipmiServer;
						s_svcListenerId = ctx->svcListenerId;
						s_mpState = ctx->mpState;
						s_serverCtx = ctx;
						return 0;
					}
					npImpsUtilMpStateStop();
				}
				npImpsSvcUnregisterListener(ctx->svcListenerId);
			}
			NpImpsUtilAsyncRelayMgr::Destroy();
		}

		if (ctx->gameSessions != NULL) {
			ctx->gameSessions->~NpImpsUtilSessionHolder();
			npImpsUtilFree(ctx->gameSessions);
			ctx->gameSessions = NULL;
		}
		if (ctx->systemSessions != NULL) {
			ctx->systemSessions->~NpImpsUtilSessionHolder();
			npImpsUtilFree(ctx->systemSessions);
			ctx->systemSessions = NULL;
		}
	}

	ctx->ipmiServer->stop();
	ctx->ipmiServer->destroy();
	npImpsUtilFree(ctx->ipmiServer);
	ctx->ipmiServer = NULL;
	npImpsUtilFree(ctx);
	npImpsUtilHeapTerm();
	return ret;
}

/* 0x812F0964 */
int npImpsUtilTerm(void)
{
	if (s_serverCtx == NULL)
		return NP_IMPS_UTIL_ERROR_NOT_INITIALIZED;

	rawMsgMgrTerm();
	npImpsUtilMpStateStop();
	s_serverCtx->mpState = NULL;
	npImpsSvcUnregisterListener(s_serverCtx->svcListenerId);
	NpImpsUtilAsyncRelayMgr::Destroy();

	if (s_serverCtx->gameSessions != NULL) {
		s_serverCtx->gameSessions->~NpImpsUtilSessionHolder();
		npImpsUtilFree(s_serverCtx->gameSessions);
		s_serverCtx->gameSessions = NULL;
	}
	if (s_serverCtx->systemSessions != NULL) {
		s_serverCtx->systemSessions->~NpImpsUtilSessionHolder();
		npImpsUtilFree(s_serverCtx->systemSessions);
		s_serverCtx->systemSessions = NULL;
	}

	s_serverCtx->ipmiServer->stop();
	s_serverCtx->ipmiServer->destroy();
	npImpsUtilFree(s_serverCtx->ipmiServer);
	s_serverCtx->ipmiServer = NULL;

	npImpsUtilFree(s_serverCtx);
	s_serverCtx = NULL;
	s_gameSessionHolder = NULL;
	s_systemSessionHolder = NULL;
	s_svcListenerId = 0;
	s_ipmiServer = NULL;
	s_mpState = NULL;
	npImpsUtilHeapTerm();
	return 0;
}

/* 0x812F0A48 */
int npImpsUtilOnProcessEvent(NpImpsUtilServerContext *ctx, int eventType,
			     SceUID processId, int appId, int eventAttr)
{
	if (eventType == 1) {
		if (eventAttr != 0x1002)
			npImpsUtilMpStateOnProcessCreated(ctx->mpState, processId, appId);
		return 0;
	}
	if (eventType == 2) {
		if (eventAttr != 0x1002) {
			int ret = npImpsUtilMpStatePostEvent(ctx->mpState,
							     NP_IMPS_MP_EVT_PROCESS_TERMINATE,
							     processId, 0);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

/* 0x812F0A92 */
int npImpsUtilGetProcessServiceState(NpImpsUtilServerContext *ctx, SceUID processId)
{
	if (ctx == NULL || ctx->mpState == NULL)
		return 0;

	int state = 0;
	npImpsUtilMpStateGetServiceState(ctx->mpState, processId, &state, 0);
	return state;
}

/* 0x812F1108 */
int npImpsUtilIsInitialized(void)
{
	return (s_serverCtx != NULL) ? 0 : NP_IMPS_UTIL_ERROR_NOT_INITIALIZED;
}

/* ============================================================================
 * SceNpImpsSvc C facade (0x812F7FD0..0x812F899B)
 * ============================================================================ */

/* 0x812F7FD0 */
int npImpsSvcIsOnline(int *outOnline)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;

	bool online = false;
	int ret = s_svcInstance->IsOnline(&online);
	if (ret >= 0)
		*outOnline = online ? 1 : 0;
	return ret;
}

/* 0x812F8024 */
int npImpsSvcGetFriendEntryNpId(NpImpsCacheFriendPtr *handle, SceNpId *outNpId)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetFriendEntryNpId(handle, outNpId);
}

/* 0x812F8046 */
int npImpsSvcGetFriendEntryRelation(NpImpsCacheFriendPtr *handle, int *outRelation)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetFriendEntryRelation(handle, outRelation);
}

/* 0x812F8068 */
int npImpsSvcGetFriendEntryDate(NpImpsCacheFriendPtr *handle, paf::rtc::Tick *outTick)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetFriendEntryDate(handle, outTick);
}

/* 0x812F808A */
int npImpsSvcGetFriendEntryRequestDirection(NpImpsCacheFriendPtr *handle, int *outDir)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetFriendEntryRequestDirection(handle, outDir);
}

/* 0x812F80AC */
int npImpsSvcGetFriendEntryIsNew(NpImpsCacheFriendPtr *handle, SceUInt8 *outIsNew)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetFriendEntryIsNew(handle, outIsNew);
}

/* 0x812F80CE */
int npImpsSvcGetFriendCount(SceUInt32 *outCount)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetFriendCount(outCount);
}

/* 0x812F80EE */
int npImpsSvcGetFriendEntries(SceUInt32 startIndex, SceNpId *outEntries,
			      SceUInt32 maxCount, SceUInt32 *outCount)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetFriendEntries(startIndex, outEntries, maxCount, outCount);
}

/* 0x812F811C */
int npImpsSvcSnapshotFriendHandles(NpImpsCacheFriendPtr ***outHandles, SceUInt32 *outCount,
				   void *(*allocFn)(SceSize))
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->SnapshotFriendHandles(outHandles, outCount, allocFn);
}

/* 0x812F8142 */
int npImpsSvcReleaseFriendHandles(NpImpsCacheFriendPtr **handles, SceUInt32 count,
				  void (*freeFn)(void *))
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->ReleaseFriendHandles(handles, count, freeFn);
}

/* 0x812F8168 */
int npImpsSvcGetAcceptedFriendCount(SceUInt32 *outCount, int filterMode)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetAcceptedFriendCount(outCount, filterMode);
}

/* 0x812F818A */
int npImpsSvcGetAcceptedFriendEntries(SceUInt32 startIndex, SceNpId *outEntries,
				      SceUInt32 maxCount, SceUInt32 *outCount, int filterMode)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetAcceptedFriendEntries(startIndex, outEntries, maxCount,
						       outCount, filterMode);
}

/* 0x812F81BE */
int npImpsSvcGetPendingFriendCount(SceUInt32 *outCount, int filterMode)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetPendingFriendCount(outCount, filterMode);
}

/* 0x812F81E0 */
int npImpsSvcGetPendingFriendEntries(SceUInt32 startIndex, SceNpId *outEntries,
				     SceUInt32 maxCount, SceUInt32 *outCount, int filterMode)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetPendingFriendEntries(startIndex, outEntries, maxCount,
						      outCount, filterMode);
}

/* 0x812F8214 */
int npImpsSvcGetRequestedFriendCount(SceUInt32 *outCount, int filterMode)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetRequestedFriendCount(outCount, filterMode);
}

/* 0x812F8236 */
int npImpsSvcGetRequestedFriendEntries(SceUInt32 startIndex, SceNpId *outEntries,
				       SceUInt32 maxCount, SceUInt32 *outCount, int filterMode)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetRequestedFriendEntries(startIndex, outEntries, maxCount,
							outCount, filterMode);
}

/* 0x812F826A */
int npImpsSvcGetFriendPresenceByNpId(const SceNpId *npId, NpImpsSvcPresenceSnapshot *outSnapshot)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	if (npId == NULL || outSnapshot == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	return s_svcInstance->GetFriendPresenceByNpId(npId, outSnapshot);
}

/* 0x812F829A */
int npImpsSvcGetFriendPresenceByHandle(NpImpsCacheFriendPtr *handle,
				       NpImpsSvcPresenceSnapshot *outSnap)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	if (outSnap == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	return s_svcInstance->GetFriendPresenceByHandle(handle, outSnap,
							NP_IMPS_SVC_PLATFORM_UNSPECIFIED);
}

/* 0x812F82CA */
int npImpsSvcGetFriendPresenceByPlatform(NpImpsCacheFriendPtr *handle,
					 NpImpsSvcPresenceSnapshot *outSnap, int platform)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	if (outSnap == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	return s_svcInstance->GetFriendPresenceByHandle(handle, outSnap, platform);
}

/* 0x812F82FC */
int npImpsSvcGetFriendContextByNpId(const SceNpId *npId, int *outContext)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	if (npId == NULL || outContext == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	return s_svcInstance->GetFriendContextByNpId(npId, outContext);
}

/* 0x812F832C */
int npImpsSvcGetServiceState(int *outState)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetServiceState(outState);
}

/* 0x812F834E */
int npImpsSvcGetMyOnlineStatus(int *outStatus)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetMyOnlineStatus(outStatus);
}

/* 0x812F836E */
int npImpsSvcGetBlockListCount(SceUInt32 *outCount, int listMode)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetBlockListCount(outCount, listMode);
}

/* 0x812F8390 */
int npImpsSvcGetBlockListEntries(SceUInt32 startIndex, SceNpId *outEntries,
				 SceUInt32 maxCount, SceUInt32 *outCount, int listMode)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetBlockListEntries(startIndex, outEntries, maxCount,
						  outCount, listMode);
}

/* 0x812F83C4 */
int npImpsSvcIsBlocked(const SceNpId *npId, bool *outBlocked, int listMode)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->IsBlocked(npId, outBlocked, listMode);
}

/* 0x812F83EA */
int npImpsSvcGetMyPresence(NpImpsSvcPresenceSnapshot *outSnap)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetMyPresence(outSnap);
}

/* 0x812F840A */
int npImpsSvcGetFriendGamePresence(const SceNpId *npId,
				   NpImpsGamePresenceWithTitle *outPresence)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	if (npId == NULL || outPresence == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	return s_svcInstance->GetFriendGamePresence(npId, outPresence);
}

/* 0x812F843C */
int npImpsSvcLockServiceState(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->LockServiceState();
}

/* 0x812F845A */
int npImpsSvcUnlockServiceState(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->UnlockServiceState();
}

/* 0x812F8478 */
int npImpsSvcLockOnlineStatus(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->LockOnlineStatus();
}

/* 0x812F8496 */
int npImpsSvcUnlockOnlineStatus(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->UnlockOnlineStatus();
}

/* 0x812F84B4 */
int npImpsSvcLockMyPresence(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->LockMyPresence();
}

/* 0x812F84D2 */
int npImpsSvcUnlockMyPresence(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->UnlockMyPresence();
}

/* 0x812F84F0 */
int npImpsSvcLockFriendStore(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->LockFriendStore();
}

/* 0x812F850E */
int npImpsSvcUnlockFriendStore(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->UnlockFriendStore();
}

/* 0x812F852C */
int npImpsSvcLockBlockStore(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->LockBlockStore();
}

/* 0x812F854A */
int npImpsSvcUnlockBlockStore(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->UnlockBlockStore();
}

/* 0x812F8568 */
int npImpsSvcSetActiveProcess(SceUID processId, int appId)
{
	if (s_svcInstance == NULL)
		return 0;
	return s_svcInstance->SetActiveProcess(processId, appId);
}

/* 0x812F8580 */
int npImpsSvcSetGameTitle(int appId)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->SetGameTitle(appId);
}

/* 0x812F85A0 */
int npImpsSvcClearGameTitle(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->ClearGameTitle();
}

/* 0x812F85BE */
int npImpsSvcSetCommId(SceUID processId, const SceNpCommunicationId *commId)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->SetCommId(processId, commId);
}

/* 0x812F85E0 */
int npImpsSvcClearCommId(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->ClearCommId();
}

/* 0x812F85FE */
int npImpsSvcHasCommId(bool *outHasCommId)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->HasCommId(outHasCommId);
}

/* 0x812F861E */
int npImpsSvcPublishMyPresence(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->PublishMyPresence();
}

/* 0x812F863C */
int npImpsSvcRepublishMyPresenceWithoutCommId(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->RepublishMyPresenceWithoutCommId();
}

/* 0x812F865A */
int npImpsSvcSetGamePresence(SceUID processId, const NpImpsGamePresenceInput *input)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->SetGamePresence(processId, input);
}

/* 0x812F867C */
int npImpsSvcClearGamePresence(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->ClearGamePresence();
}

/* 0x812F869A */
int npImpsSvcSetPartyPresence(const NpImpsPartyPresence *party)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->SetPartyPresence(party);
}

/* 0x812F86BA */
int npImpsSvcClearPartyPresence(void)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->ClearPartyPresence();
}

/* 0x812F86D8 */
int npImpsSvcGetAppTitleInfo(SceUID appId, NpImpsAppTitleInfo *outInfo)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->GetAppTitleInfo(appId, outInfo);
}

/* 0x812F86FA */
int npImpsSvcIsBigAppGame(int appId, SceUInt8 *outIsBigApp)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	*outIsBigApp = static_cast<SceUInt8>(s_svcInstance->IsBigAppGame(appId));
	return 0;
}

/* 0x812F8720 */
int npImpsSvcRegisterListener(const NpImpsSvcListenerCallbacks *cb, SceUInt32 *outId)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->RegisterListener(cb, outId);
}

/* 0x812F8742 */
int npImpsSvcUnregisterListener(SceUInt32 listenerId)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcInstance->UnregisterListener(listenerId);
}

/* 0x812F8764 */
int npImpsSvcCreateContext(SceUInt32 *outCtxId)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcAsyncInterface->CreateContext(outCtxId);
}

/* 0x812F8790 */
int npImpsSvcDestroyContext(SceUInt32 ctxId)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcAsyncInterface->DestroyContext(ctxId);
}

/* 0x812F87BC */
int npImpsSvcAbortContext(SceUInt32 ctxId)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcAsyncInterface->AbortContext(ctxId);
}

/* 0x812F87E8 */
int npImpsSvcWaitContext(SceUInt32 ctxId)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcAsyncInterface->WaitContext(ctxId);
}

/* 0x812F8814 */
int npImpsSvcRequestFriend(SceUInt32 ctxId, const SceNpId *npId, const char *comment,
			   NpImpsSvcRosterCallback callback)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcAsyncInterface->RequestFriend(ctxId, npId, comment, callback);
}

/* 0x812F8850 */
int npImpsSvcAcceptFriend(SceUInt32 ctxId, const SceNpId *npId, NpImpsSvcRosterCallback callback)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcAsyncInterface->AcceptFriend(ctxId, npId, callback);
}

/* 0x812F8880 */
int npImpsSvcDeleteFriend(SceUInt32 ctxId, const SceNpId *npId, NpImpsSvcRosterCallback callback)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcAsyncInterface->DeleteFriend(ctxId, npId, callback);
}

/* 0x812F88B0 */
int npImpsSvcAddBlock(SceUInt32 ctxId, const SceNpId *npId, NpImpsSvcRosterCallback callback)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcAsyncInterface->AddBlock(ctxId, npId, callback);
}

/* 0x812F88E0 */
int npImpsSvcDeleteBlock(SceUInt32 ctxId, const SceNpId *npId, NpImpsSvcRosterCallback callback)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcAsyncInterface->DeleteBlock(ctxId, npId, callback);
}

/* 0x812F8910 */
int npImpsSvcForceRosterSync(int syncFlags)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	return s_svcAsyncInterface->ForceRosterSync(syncFlags);
}

/* 0x812F893C */
int npImpsSvcInit(void *heapMem, SceSize heapSize, const NpImpsSvcInitParam *param)
{
	int ret = NpImpsSvc::Init(heapMem, heapSize, param);
	if (ret < 0)
		return ret;

	ret = NpImpsSvc::GetInstance(&s_svcInstance);
	if (ret < 0)
		return ret;

	s_svcAsyncInterface = static_cast<NpImpsSvcAsyncInterface *>(s_svcInstance);
	return 0;
}

/* 0x812F8976 */
int npImpsSvcTerm(void)
{
	int ret = NpImpsSvc::Term();
	if (ret >= 0) {
		s_svcInstance = NULL;
		s_svcAsyncInterface = NULL;
		return 0;
	}
	return ret;
}
