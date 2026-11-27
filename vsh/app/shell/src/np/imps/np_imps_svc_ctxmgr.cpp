/*
 * np_imps_svc_ctxmgr.cpp: SceNpImpsSvc heap allocators, async roster request
 * contexts (request/accept/delete friend, add/delete block), context queue,
 * and worker thread ("SceNpImpsSvcCtxMgr") (0x812FDC94..0x812FE0AB,
 * 0x812FF2E8..0x812FFE9D).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/new>
#include <std/string.h>
#include "np_imps_svc.h"

static paf::memory::HeapAllocator *s_svcAllocator = NULL;	/* 0x815C386C */

/* 0x812FF2E8..0x812FF337: SceNpImpsSvc heap helpers */
void npImpsSvcHeapSet(paf::memory::HeapAllocator *heap)
{
	s_svcAllocator = heap;
}

void npImpsSvcHeapClear(void)
{
	s_svcAllocator = NULL;
}

void *npImpsSvcMalloc(SceSize size)
{
	if (s_svcAllocator != NULL)
		return s_svcAllocator->sce_paf_malloc(size);
	return NULL;
}

void npImpsSvcFree(void *ptr)
{
	if (s_svcAllocator != NULL)
		s_svcAllocator->sce_paf_free(ptr);
}

/* 0x812FF4BC: trigger roster cache sync if not yet fully online */
static int npImpsTriggerRosterSync(const SceNpId *npId, int syncReason)
{
	int serviceState = 0;
	int ret = npImpsSvcGetServiceState(&serviceState);
	if (ret >= 0 && serviceState != NP_IMPS_SVC_STATE_ONLINE) {
		NpImpsCacheCore *core = npImpsCacheGetCore();
		if (core == NULL)
			return NP_IMPS_SVC_ERROR_INTERNAL;
		NpImpsCacheRosterSync *rosterSync = core->GetRosterSync();
		if (rosterSync == NULL)
			return NP_IMPS_SVC_ERROR_INTERNAL;
		return rosterSync->TriggerRosterUpdate(npId, syncReason);
	}
	return ret;
}

/* =========================================================================
 * NpImpsSvcContext base class (0x812FF338..0x812FF4BB)
 * ========================================================================= */

/* 0x812FF338 */
NpImpsSvcContext::NpImpsSvcContext(SceUInt32 contextId)
	: m_rmutex("imps_svc::Context::m_rmutex", 0),
	  m_rosterReqId(-1),
	  m_contextId(contextId),
	  m_aborted(0),
	  m_mutexToWait("imps_svc::Context::m_mutexToWait", 0),
	  m_condToWait(m_mutexToWait, "imps_svc::Context::m_condToWait", 0)
{
}

/* 0x812FF39C, 0x812FF3DE */
NpImpsSvcContext::~NpImpsSvcContext()
{
}

/* 0x812FF428 */
int NpImpsSvcContext::RunIfNotAborted()
{
	if (m_aborted != 0)
		return NP_IMPS_SVC_ERROR_ABORTED;
	return Execute();
}

/* 0x812FF444 */
int NpImpsSvcContext::Abort()
{
	if (m_aborted != 0)
		return NP_IMPS_SVC_ERROR_ALREADY_ABORTED;

	m_aborted = 1;
	m_rmutex.Lock();
	int ret = 0;
	if (m_rosterReqId != -1)
		ret = npRosterAbortRequest(m_rosterReqId);
	m_rmutex.Unlock();
	return ret;
}

/* 0x812FF492 */
void NpImpsSvcContext::WaitComplete()
{
	m_mutexToWait.Lock();
	m_condToWait.Wait(0);
	m_mutexToWait.Unlock();
}

/* 0x812FF4B2 */
void NpImpsSvcContext::SignalComplete()
{
	m_condToWait.NotifyAll();
}

/* =========================================================================
 * Concrete roster async contexts (0x812FDE94..0x812FE0AB, 0x812FFB3C..0x812FFE4B)
 * ========================================================================= */

class NpImpsSvcSimpleRosterContext : public NpImpsSvcContext {
public:
	NpImpsSvcSimpleRosterContext(SceUInt32 contextId, const SceNpId *npId,
				     NpImpsSvcRosterCallback callback)
		: NpImpsSvcContext(contextId),
		  m_callback(callback)
	{
		sce_paf_memcpy(&m_targetNpId, npId, sizeof(SceNpId));
	}

	virtual ~NpImpsSvcSimpleRosterContext() {}

	/* 0x812FDEE8 */
	virtual void NotifyComplete(int result)
	{
		if (m_callback != NULL)
			m_callback(result, m_contextId, &m_targetNpId);
	}

protected:
	SceNpId m_targetNpId;			/* +0x6C */
	NpImpsSvcRosterCallback m_callback;	/* +0x90 */
};

/* Delete friend context (0x812FDE94..0x812FDF83) */
class NpImpsSvcDeleteFriendContext : public NpImpsSvcSimpleRosterContext {
public:
	NpImpsSvcDeleteFriendContext(SceUInt32 contextId, const SceNpId *npId,
				     NpImpsSvcRosterCallback callback)
		: NpImpsSvcSimpleRosterContext(contextId, npId, callback)
	{
	}

	/* 0x812FDF02, 0x812FDF6A */
	virtual int Execute()
	{
		if (IsAborted())
			return NP_IMPS_SVC_ERROR_ABORTED;

		m_rmutex.Lock();
		m_rosterReqId = npRosterCreateRequest();
		m_rmutex.Unlock();

		int ret = m_rosterReqId;
		if (ret >= 0) {
			ret = npRosterDeleteFriend(m_rosterReqId, &m_targetNpId);
			m_rmutex.Lock();
			int destroyRet = npRosterDestroyRequest(m_rosterReqId);
			m_rosterReqId = -1;
			m_rmutex.Unlock();
			if (ret >= 0)
				ret = destroyRet;
		}
		if (ret >= 0)
			ret = npImpsTriggerRosterSync(&m_targetNpId, NP_IMPS_ROSTER_SYNC_DELETE_FRIEND);
		return ret;
	}
};

/* Delete block context (0x812FDF84..0x812FE0AB) */
class NpImpsSvcDeleteBlockContext : public NpImpsSvcSimpleRosterContext {
public:
	NpImpsSvcDeleteBlockContext(SceUInt32 contextId, const SceNpId *npId,
				    NpImpsSvcRosterCallback callback)
		: NpImpsSvcSimpleRosterContext(contextId, npId, callback)
	{
	}

	/* 0x812FDFD6, 0x812FE094 */
	virtual int Execute()
	{
		if (IsAborted())
			return NP_IMPS_SVC_ERROR_ABORTED;

		m_rmutex.Lock();
		m_rosterReqId = npRosterCreateRequest();
		m_rmutex.Unlock();

		int ret = m_rosterReqId;
		if (ret >= 0) {
			NpRosterBlockListEntry entry;
			sce_paf_memcpy(&entry.npId, &m_targetNpId, sizeof(SceNpId));
			entry.action = 1;
			ret = npRosterUpdateBlockList(m_rosterReqId, &entry, 1);
			m_rmutex.Lock();
			int destroyRet = npRosterDestroyRequest(m_rosterReqId);
			m_rosterReqId = -1;
			m_rmutex.Unlock();
			if (ret >= 0)
				ret = destroyRet;
		}
		if (ret >= 0)
			ret = npImpsTriggerRosterSync(&m_targetNpId, NP_IMPS_ROSTER_SYNC_DELETE_BLOCK);
		return ret;
	}
};

/* Request friend context (2200 bytes, 0x812FFB3C..0x812FFC53) */
class NpImpsSvcRequestFriendContext : public NpImpsSvcSimpleRosterContext {
public:
	NpImpsSvcRequestFriendContext(SceUInt32 contextId, const SceNpId *npId,
				      const char *comment, NpImpsSvcRosterCallback callback)
		: NpImpsSvcSimpleRosterContext(contextId, npId, callback)
	{
		sce_paf_memset(m_comment, 0, sizeof(m_comment));
		if (comment != NULL) {
			sce_paf_strncpy(m_comment, comment, sizeof(m_comment));
			m_comment[sizeof(m_comment) - 1] = '\0';
		}
	}

	/* 0x812FFBBE, 0x812FFC3C */
	virtual int Execute()
	{
		if (IsAborted())
			return NP_IMPS_SVC_ERROR_ABORTED;

		m_rmutex.Lock();
		m_rosterReqId = npRosterCreateRequest();
		m_rmutex.Unlock();

		int ret = m_rosterReqId;
		if (ret >= 0) {
			const char *commentArg = (sce_paf_strlen(m_comment) != 0) ? m_comment : NULL;
			ret = npRosterRequestFriend(m_rosterReqId, &m_targetNpId, commentArg);
			m_rmutex.Lock();
			int destroyRet = npRosterDestroyRequest(m_rosterReqId);
			m_rosterReqId = -1;
			m_rmutex.Unlock();
			if (ret >= 0)
				ret = destroyRet;
		}
		if (ret >= 0)
			ret = npImpsTriggerRosterSync(&m_targetNpId, NP_IMPS_ROSTER_SYNC_REQUEST_FRIEND);
		return ret;
	}

private:
	char m_comment[2049];			/* +0x94 */
};

/* Add block context (0x812FFC54..0x812FFD7F) */
class NpImpsSvcAddBlockContext : public NpImpsSvcSimpleRosterContext {
public:
	NpImpsSvcAddBlockContext(SceUInt32 contextId, const SceNpId *npId,
				 NpImpsSvcRosterCallback callback)
		: NpImpsSvcSimpleRosterContext(contextId, npId, callback)
	{
	}

	/* 0x812FFCA6, 0x812FFD66 */
	virtual int Execute()
	{
		if (IsAborted())
			return NP_IMPS_SVC_ERROR_ABORTED;

		m_rmutex.Lock();
		m_rosterReqId = npRosterCreateRequest();
		m_rmutex.Unlock();

		int ret = m_rosterReqId;
		if (ret >= 0) {
			NpRosterBlockListEntry entry;
			sce_paf_memcpy(&entry.npId, &m_targetNpId, sizeof(SceNpId));
			entry.action = 0;
			ret = npRosterUpdateBlockList(m_rosterReqId, &entry, 1);
			m_rmutex.Lock();
			int destroyRet = npRosterDestroyRequest(m_rosterReqId);
			m_rosterReqId = -1;
			m_rmutex.Unlock();
			if (ret >= 0)
				ret = destroyRet;
		}
		if (ret >= 0)
			ret = npImpsTriggerRosterSync(&m_targetNpId, NP_IMPS_ROSTER_SYNC_ADD_BLOCK);
		return ret;
	}
};

/* Accept friend context (0x812FFD80..0x812FFE4B) */
class NpImpsSvcAcceptFriendContext : public NpImpsSvcSimpleRosterContext {
public:
	NpImpsSvcAcceptFriendContext(SceUInt32 contextId, const SceNpId *npId,
				     NpImpsSvcRosterCallback callback)
		: NpImpsSvcSimpleRosterContext(contextId, npId, callback)
	{
	}

	/* 0x812FFDD4 */
	virtual int Execute()
	{
		if (IsAborted())
			return NP_IMPS_SVC_ERROR_ABORTED;

		m_rmutex.Lock();
		m_rosterReqId = npRosterCreateRequest();
		m_rmutex.Unlock();

		int ret = m_rosterReqId;
		if (ret >= 0) {
			ret = npRosterAcceptFriend(m_rosterReqId, &m_targetNpId);
			m_rmutex.Lock();
			int destroyRet = npRosterDestroyRequest(m_rosterReqId);
			m_rosterReqId = -1;
			m_rmutex.Unlock();
			if (ret >= 0)
				ret = destroyRet;
		}
		if (ret >= 0)
			ret = npImpsTriggerRosterSync(&m_targetNpId, NP_IMPS_ROSTER_SYNC_ACCEPT_FRIEND);
		return ret;
	}
};

/* =========================================================================
 * NpImpsSvcContextFactory (0x812FDC94..0x812FDE93)
 * ========================================================================= */

static int npImpsAssignContext(NpImpsSvcContext **outCtx, NpImpsSvcContext *created)
{
	if (created != NULL)
		created->AddRef();

	if (*outCtx != created) {
		if (*outCtx != NULL)
			(*outCtx)->Release();
		*outCtx = created;
		if (created != NULL)
			created->AddRef();
	}
	if (created != NULL)
		created->Release();

	return (*outCtx != NULL) ? 0 : NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
}

/* 0x812FDC94 */
int NpImpsSvcContextFactory::CreateRequestFriend(NpImpsSvcContext **outCtx, SceUInt32 ctxId,
						 const SceNpId *npId, const char *comment,
						 NpImpsSvcRosterCallback callback)
{
	void *mem = npImpsSvcMalloc(sizeof(NpImpsSvcRequestFriendContext));
	NpImpsSvcContext *created = (mem != NULL)
		? new (mem) NpImpsSvcRequestFriendContext(ctxId, npId, comment, callback)
		: NULL;
	return npImpsAssignContext(outCtx, created);
}

/* 0x812FDD04 */
int NpImpsSvcContextFactory::CreateDeleteFriend(NpImpsSvcContext **outCtx, SceUInt32 ctxId,
						const SceNpId *npId,
						NpImpsSvcRosterCallback callback)
{
	void *mem = npImpsSvcMalloc(sizeof(NpImpsSvcDeleteFriendContext));
	NpImpsSvcContext *created = (mem != NULL)
		? new (mem) NpImpsSvcDeleteFriendContext(ctxId, npId, callback)
		: NULL;
	return npImpsAssignContext(outCtx, created);
}

/* 0x812FDD68 */
int NpImpsSvcContextFactory::CreateAcceptFriend(NpImpsSvcContext **outCtx, SceUInt32 ctxId,
						const SceNpId *npId,
						NpImpsSvcRosterCallback callback)
{
	void *mem = npImpsSvcMalloc(sizeof(NpImpsSvcAcceptFriendContext));
	NpImpsSvcContext *created = (mem != NULL)
		? new (mem) NpImpsSvcAcceptFriendContext(ctxId, npId, callback)
		: NULL;
	return npImpsAssignContext(outCtx, created);
}

/* 0x812FDDCC */
int NpImpsSvcContextFactory::CreateAddBlock(NpImpsSvcContext **outCtx, SceUInt32 ctxId,
					    const SceNpId *npId,
					    NpImpsSvcRosterCallback callback)
{
	void *mem = npImpsSvcMalloc(sizeof(NpImpsSvcAddBlockContext));
	NpImpsSvcContext *created = (mem != NULL)
		? new (mem) NpImpsSvcAddBlockContext(ctxId, npId, callback)
		: NULL;
	return npImpsAssignContext(outCtx, created);
}

/* 0x812FDE30 */
int NpImpsSvcContextFactory::CreateDeleteBlock(NpImpsSvcContext **outCtx, SceUInt32 ctxId,
					       const SceNpId *npId,
					       NpImpsSvcRosterCallback callback)
{
	void *mem = npImpsSvcMalloc(sizeof(NpImpsSvcDeleteBlockContext));
	NpImpsSvcContext *created = (mem != NULL)
		? new (mem) NpImpsSvcDeleteBlockContext(ctxId, npId, callback)
		: NULL;
	return npImpsAssignContext(outCtx, created);
}

/* =========================================================================
 * NpImpsSvcContextQueue & NpImpsSvcContextManager (0x812FF528..0x812FFB3B)
 * ========================================================================= */

NpImpsSvcContextQueue::NpImpsSvcContextQueue()
	: m_capacity(16),
	  m_mutex("SceNpImpsSvcContextQueueRMutex", 0),
	  m_head(NULL),
	  m_tail(NULL),
	  m_count(0)
{
}

NpImpsSvcContextQueue::~NpImpsSvcContextQueue()
{
	m_mutex.Lock();
	QueueNode *cur = m_head;
	while (cur != NULL) {
		QueueNode *next = cur->next;
		if (cur->ctx != NULL)
			cur->ctx->Release();
		npImpsSvcFree(cur);
		cur = next;
	}
	m_head = NULL;
	m_tail = NULL;
	m_count = 0;
	m_mutex.Unlock();
}

/* 0x812FF8D0 */
int NpImpsSvcContextQueue::Enqueue(NpImpsSvcContext *ctx)
{
	m_mutex.Lock();
	if (m_count >= m_capacity) {
		m_mutex.Unlock();
		return NP_IMPS_SVC_ERROR_QUEUE_FULL;
	}

	QueueNode *node = reinterpret_cast<QueueNode *>(npImpsSvcMalloc(sizeof(QueueNode)));
	if (node == NULL) {
		m_mutex.Unlock();
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	}

	node->prev = m_tail;
	node->next = NULL;
	node->ctx = ctx;
	if (ctx != NULL)
		ctx->AddRef();

	if (m_tail != NULL)
		m_tail->next = node;
	else
		m_head = node;
	m_tail = node;
	m_count++;

	m_mutex.Unlock();
	return 0;
}

/* 0x812FF97C */
void NpImpsSvcContextQueue::ProcessAll()
{
	m_mutex.Lock();
	while (m_count != 0) {
		NpImpsSvcContext *ctx = m_head->ctx;
		if (ctx != NULL)
			ctx->AddRef();
		m_mutex.Unlock();

		int result = ctx->RunIfNotAborted();
		ctx->NotifyComplete(result);

		m_mutex.Lock();
		if (m_head != NULL) {
			QueueNode *front = m_head;
			m_head = front->next;
			if (m_head != NULL)
				m_head->prev = NULL;
			else
				m_tail = NULL;
			if (front->ctx != NULL)
				front->ctx->Release();
			npImpsSvcFree(front);
			m_count--;
		}
		ctx->SignalComplete();
		if (ctx != NULL)
			ctx->Release();
	}
	m_mutex.Unlock();
}

/* 0x812FFA5E */
int NpImpsSvcContextQueue::AbortContext(SceUInt32 contextId)
{
	m_mutex.Lock();
	int ret = 0;
	for (QueueNode *cur = m_head; cur != NULL; cur = cur->next) {
		if (cur->ctx != NULL && cur->ctx->GetContextId() == contextId) {
			ret = cur->ctx->Abort();
			break;
		}
	}
	m_mutex.Unlock();
	return ret;
}

/* 0x812FFABE */
void NpImpsSvcContextQueue::WaitContext(SceUInt32 contextId)
{
	m_mutex.Lock();
	NpImpsSvcContext *target = NULL;
	for (QueueNode *cur = m_head; cur != NULL; cur = cur->next) {
		if (cur->ctx != NULL && cur->ctx->GetContextId() == contextId) {
			target = cur->ctx;
			target->AddRef();
			break;
		}
	}
	if (target != NULL) {
		m_mutex.Unlock();
		target->WaitComplete();
		m_mutex.Lock();
		target->Release();
	}
	m_mutex.Unlock();
}

/* 0x812FF528 */
NpImpsSvcContextManager::NpImpsSvcContextManager(const NpImpsSvcInitParam *param)
	: paf::thread::Thread(param->ctxMgrThreadPriority, param->ctxMgrThreadStackSize,
			      "SceNpImpsSvcCtxMgr", NULL),
	  m_waitMutex("SceNpImpsSvcContextManagerMutex", 0),
	  m_waitCond(m_waitMutex, "SceNpImpsSvcContextManagerCond", 0),
	  m_queue(),
	  m_nextContextId(1),
	  m_setMutex("SceNpImpsSvcCtxMgrRMutex4CtxSet", 0),
	  m_activeCount(0)
{
	sce_paf_memset(m_activeContextIds, 0, sizeof(m_activeContextIds));
}

/* 0x812FF610 */
NpImpsSvcContextManager::~NpImpsSvcContextManager()
{
}

/* 0x812FF6D8 */
int NpImpsSvcContextManager::StartManager()
{
	return Start();
}

/* 0x812FF6E0 */
int NpImpsSvcContextManager::StopManager()
{
	m_waitCond.Notify();
	Cancel();
	Join();
	return 0;
}

/* 0x812FF6FC */
bool NpImpsSvcContextManager::HasContextIdLocked(SceUInt32 contextId) const
{
	for (SceUInt32 idx = 0; idx < m_activeCount; ++idx) {
		if (m_activeContextIds[idx] == contextId)
			return true;
	}
	return false;
}

bool NpImpsSvcContextManager::InsertContextIdLocked(SceUInt32 contextId)
{
	if (m_activeCount >= 16)
		return false;
	m_activeContextIds[m_activeCount++] = contextId;
	return true;
}

void NpImpsSvcContextManager::EraseContextIdLocked(SceUInt32 contextId)
{
	for (SceUInt32 idx = 0; idx < m_activeCount; ++idx) {
		if (m_activeContextIds[idx] == contextId) {
			m_activeContextIds[idx] = m_activeContextIds[m_activeCount - 1];
			m_activeCount--;
			return;
		}
	}
}

/* 0x812FF75E */
int NpImpsSvcContextManager::CreateContextId(SceUInt32 *outContextId)
{
	m_setMutex.Lock();
	if (m_activeCount >= 16) {
		m_setMutex.Unlock();
		return NP_IMPS_SVC_ERROR_CONTEXT_LIMIT;
	}

	SceUInt32 assigned = m_nextContextId;
	*outContextId = assigned;
	InsertContextIdLocked(assigned);

	do {
		if (m_nextContextId == 0xFFFFFFFFU)
			m_nextContextId = 1;
		else
			m_nextContextId++;
	} while (HasContextIdLocked(m_nextContextId));

	m_setMutex.Unlock();
	return 0;
}

/* 0x812FF7BE */
int NpImpsSvcContextManager::DestroyContextId(SceUInt32 contextId)
{
	m_setMutex.Lock();
	m_queue.AbortContext(contextId);
	EraseContextIdLocked(contextId);
	m_setMutex.Unlock();
	return 0;
}

/* 0x812FF826 */
int NpImpsSvcContextManager::AbortContext(SceUInt32 contextId)
{
	return m_queue.AbortContext(contextId);
}

/* 0x812FF830 */
int NpImpsSvcContextManager::WaitContext(SceUInt32 contextId)
{
	m_queue.WaitContext(contextId);
	return 0;
}

/* 0x812FF83C */
int NpImpsSvcContextManager::EnqueueContext(NpImpsSvcContext *ctx)
{
	if (ctx != NULL)
		ctx->AddRef();
	int ret = m_queue.Enqueue(ctx);
	if (ctx != NULL)
		ctx->Release();
	m_waitCond.Notify();
	return ret;
}

/* 0x812FF888 */
void NpImpsSvcContextManager::EntryFunction()
{
	m_waitMutex.Lock();
	while (!IsCanceled()) {
		m_waitCond.Wait(0);
		m_waitMutex.Unlock();
		m_queue.ProcessAll();
		m_waitMutex.Lock();
	}
	m_waitMutex.Unlock();
}
