/*
 * np_imps_cache_core.cpp: SceNpImpsCache heap, event queue, and worker core
 * (0x812FFE4C..0x81301C9F).
 */
#include <stddef.h>
#include <std/new>
#include <std/string.h>
#include <std/stdlib.h>
#include <scetypes.h>
#include <kernel.h>
#include <libkernel/libkernel.h>
#include "np_imps_cache.h"

static paf::memory::HeapAllocator *s_cacheHeap = NULL;
static NpImpsCacheCoreImpl *s_cacheCore = NULL;

/* 0x812FFE4C..0x812FFE9D: SceNpImpsCache heap helpers */
void npImpsCacheHeapSet(paf::memory::HeapAllocator *heap)
{
	s_cacheHeap = heap;
}

void npImpsCacheHeapClear(void)
{
	s_cacheHeap = NULL;
}

void *npImpsCacheMalloc(SceSize size)
{
	if (s_cacheHeap != NULL)
		return s_cacheHeap->sce_paf_malloc(size);
	return sce_paf_malloc(size);
}

void npImpsCacheFree(void *ptr)
{
	if (ptr == NULL)
		return;
	if (s_cacheHeap != NULL)
		s_cacheHeap->sce_paf_free(ptr);
	else
		sce_paf_free(ptr);
}

/* 0x812FFF56, 0x813000FC, 0x812FE0BC: public lifecycle bridges */
int npImpsCacheInit(int threadPriority, SceSize stackSize, paf::memory::HeapAllocator *heap)
{
	return NpImpsCacheCoreImpl::Init(threadPriority, stackSize, heap);
}

int npImpsCacheTerm(void)
{
	return NpImpsCacheCoreImpl::Term();
}

NpImpsCacheCore *npImpsCacheGetCore(void)
{
	return NpImpsCacheCoreImpl::GetInstance();
}

/* --------------------------------------------------------------------------
 * NpImpsCacheEventQueue (0x81301B36..0x81301C9F)
 * -------------------------------------------------------------------------- */
NpImpsCacheEventQueue::NpImpsCacheEventQueue()
	: m_maxCapacity(512),
	  m_mutex("SceNpImpsCacheEventQueueMutex", 0),
	  m_head(NULL),
	  m_tail(NULL),
	  m_count(0)
{
}

NpImpsCacheEventQueue::~NpImpsCacheEventQueue()
{
	m_mutex.Lock();
	QueueNode *cursor = m_head;
	while (cursor != NULL) {
		QueueNode *nextNode = cursor->next;
		if (cursor->event != NULL)
			cursor->event->Release();
		npImpsCacheFree(cursor);
		cursor = nextNode;
	}
	m_head = NULL;
	m_tail = NULL;
	m_count = 0;
	m_mutex.Unlock();
}

/* 0x81301B90 */
int NpImpsCacheEventQueue::Push(NpImpsCacheEvent **event)
{
	if (event == NULL || *event == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;

	m_mutex.Lock();
	if (m_count >= m_maxCapacity) {
		m_mutex.Unlock();
		return NP_IMPS_CACHE_ERROR_QUEUE_FULL;
	}

	QueueNode *node = reinterpret_cast<QueueNode *>(npImpsCacheMalloc(sizeof(QueueNode)));
	if (node == NULL) {
		m_mutex.Unlock();
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	}

	node->prev = m_tail;
	node->next = NULL;
	node->event = *event;
	node->event->AddRef();

	if (m_tail != NULL)
		m_tail->next = node;
	else
		m_head = node;
	m_tail = node;
	++m_count;

	m_mutex.Unlock();
	return 0;
}

/* 0x81301BDA */
void NpImpsCacheEventQueue::DrainAll()
{
	for (;;) {
		m_mutex.Lock();
		QueueNode *front = m_head;
		if (front == NULL) {
			m_mutex.Unlock();
			return;
		}
		m_head = front->next;
		if (m_head != NULL)
			m_head->prev = NULL;
		else
			m_tail = NULL;
		--m_count;

		NpImpsCacheEvent *event = front->event;
		npImpsCacheFree(front);
		m_mutex.Unlock();

		if (event != NULL) {
			event->Execute();
			event->Release();
		}
	}
}

/* --------------------------------------------------------------------------
 * NpImpsCacheCoreImpl (0x812FFE9E..0x81301A9F)
 * -------------------------------------------------------------------------- */
NpImpsCacheCoreImpl::NpImpsCacheCoreImpl(int threadPriority, SceSize stackSize,
					 paf::memory::HeapAllocator *heap)
	: paf::thread::Thread(threadPriority, stackSize, "SceNpImpsCacheCore", NULL),
	  m_waitMutex("SceNpImpsCacheCoreMutex", 0),
	  m_waitCond(m_waitMutex, "SceNpImpsCacheCoreCond", 0),
	  m_heap(heap),
	  m_dataMutex("SceNpImpsCacheDataRMtx", 0),
	  m_friendStore(this, &m_dataMutex),
	  m_blockStore(this, &m_dataMutex),
	  m_observer(NULL),
	  m_eventQueue(NULL),
	  m_accountCallbackId(-1),
	  m_impsCoreCallbackId(-1),
	  m_queueOverflowed(0),
	  m_retryFriendSync(0),
	  m_retryBlockSync(0)
{
	sce_paf_memset(&m_partyTitleId, 0, sizeof(m_partyTitleId));
	sce_paf_strncpy(m_partyTitleId.data, "NPXS10001", sizeof(m_partyTitleId.data));
}

/* 0x813001FE */
NpImpsCacheCoreImpl::~NpImpsCacheCoreImpl()
{
	if (m_impsCoreCallbackId >= 0) {
		npImpsCoreUnregisterCallbacks(m_impsCoreCallbackId);
		m_impsCoreCallbackId = -1;
	}
	if (m_accountCallbackId >= 0) {
		SceShell_B4D888D5(m_accountCallbackId);
		m_accountCallbackId = -1;
	}
	if (m_eventQueue != NULL) {
		m_eventQueue->~NpImpsCacheEventQueue();
		npImpsCacheFree(m_eventQueue);
		m_eventQueue = NULL;
	}
}

/* 0x812FFF56 */
int NpImpsCacheCoreImpl::Init(int threadPriority, SceSize stackSize,
			      paf::memory::HeapAllocator *heap)
{
	if (s_cacheCore != NULL)
		return NP_IMPS_SVC_ERROR_ALREADY_INITIALIZED;

	npImpsCacheHeapSet(heap);

	void *coreMem = npImpsCacheMalloc(sizeof(NpImpsCacheCoreImpl));
	if (coreMem == NULL) {
		npImpsCacheHeapClear();
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	}

	NpImpsCacheCoreImpl *core = new (coreMem) NpImpsCacheCoreImpl(threadPriority, stackSize, heap);
	s_cacheCore = core;

	void *queueMem = npImpsCacheMalloc(sizeof(NpImpsCacheEventQueue));
	if (queueMem == NULL) {
		Term();
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	}
	core->m_eventQueue = new (queueMem) NpImpsCacheEventQueue();

	NpImpsCoreCallbacks coreCallbacks;
	sce_paf_memset(&coreCallbacks, 0, sizeof(coreCallbacks));
	coreCallbacks.onCtlEvent = OnCoreCtlEvent;
	coreCallbacks.onSessionState = OnCoreSessionState;
	coreCallbacks.onRosterUpdate = OnCoreFriendRosterPush;
	coreCallbacks.onPrivacyUpdate = OnCoreBlockListPush;
	coreCallbacks.onPresenceUpdate = OnCorePresencePush;
	coreCallbacks.onMessageRecv = OnCoreInGameDataMessage;
	coreCallbacks.onPollingNotify = OnCoreNotification;

	int ret = npImpsCoreRegisterCallbacks(&coreCallbacks, &core->m_impsCoreCallbackId);
	if (ret < 0) {
		Term();
		return ret;
	}

	ret = SceShell_2E0B6FA6(OnAccountSessionCallback, core, &core->m_accountCallbackId);
	if (ret < 0) {
		Term();
		return ret;
	}

	ret = core->Start();
	if (ret < 0) {
		Term();
		return ret;
	}

	return 0;
}

/* 0x813000FC */
int NpImpsCacheCoreImpl::Term()
{
	if (s_cacheCore == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;

	NpImpsCacheCoreImpl *core = s_cacheCore;
	core->Cancel();
	core->m_waitMutex.Lock();
	core->m_waitCond.Notify();
	core->m_waitMutex.Unlock();
	core->Join();

	core->~NpImpsCacheCoreImpl();
	npImpsCacheFree(core);
	s_cacheCore = NULL;
	npImpsCacheHeapClear();
	return 0;
}

/* 0x813001F0 */
NpImpsCacheCoreImpl *NpImpsCacheCoreImpl::GetInstance()
{
	return s_cacheCore;
}

/* 0x8130187E */
void NpImpsCacheCoreImpl::EntryFunction()
{
	m_waitMutex.Lock();
	while (!IsCanceled()) {
		m_waitCond.Wait(0);
		m_waitMutex.Unlock();

		if (m_eventQueue != NULL)
			m_eventQueue->DrainAll();

		if (m_queueOverflowed != 0) {
			npImpsCoreResyncSession();
			m_queueOverflowed = 0;
		}

		m_waitMutex.Lock();
	}
	m_waitMutex.Unlock();
}

/* 0x813002EE..0x81300327, 0x81300D3E..0x81300D49: NpImpsCacheCore interface */
int NpImpsCacheCoreImpl::RegisterObserver(NpImpsCacheObserver *observer)
{
	if (observer == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	m_dataMutex.Lock();
	m_observer = observer;
	m_dataMutex.Unlock();
	return 0;
}

int NpImpsCacheCoreImpl::UnregisterObserver()
{
	m_dataMutex.Lock();
	m_observer = NULL;
	m_dataMutex.Unlock();
	return 0;
}

NpImpsCacheFriendStore *NpImpsCacheCoreImpl::GetFriendStore()
{
	return &m_friendStore;
}

NpImpsCacheBlockStore *NpImpsCacheCoreImpl::GetBlockStore()
{
	return &m_blockStore;
}

NpImpsCacheRosterSync *NpImpsCacheCoreImpl::GetRosterSync()
{
	return this;
}

const SceNpCommunicationId *NpImpsCacheCoreImpl::GetPartyTitleId()
{
	return &m_partyTitleId;
}

paf::thread::RMutex *NpImpsCacheCoreImpl::GetStateMutex()
{
	return &m_dataMutex;
}

void NpImpsCacheCoreImpl::Lock()
{
	m_dataMutex.Lock();
}

void NpImpsCacheCoreImpl::Unlock()
{
	m_dataMutex.Unlock();
}

/* 0x81300328..0x81300D3D: NpImpsCacheStoreSink forwarding to NpImpsCacheObserver */
int NpImpsCacheCoreImpl::NotifySignInComplete()
{
	if (m_observer != NULL)
		m_observer->OnAccountSessionEvent(1);
	return 0;
}

int NpImpsCacheCoreImpl::NotifySignOutComplete()
{
	if (m_observer != NULL)
		m_observer->OnAccountSessionEvent(0);
	return 0;
}

int NpImpsCacheCoreImpl::NotifyNotificationEvent(int notifyCode)
{
	if (m_observer != NULL)
		m_observer->OnAccountSessionEvent(notifyCode);
	return 0;
}

static int dispatchFriendVectorEvent(NpImpsCacheObserver *observer, int eventType,
				     const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *entries)
{
	if (observer == NULL || entries == NULL)
		return 0;

	SceUInt32 count = entries->Size();
	if (count == 0) {
		observer->OnFriendListEvent(eventType, 0, NULL);
		return 0;
	}

	NpImpsCacheFriendPtr *handles = NULL;
	int allocRet = npImpsCacheAllocFriendHandleArray(count, &handles);
	if (allocRet < 0)
		return allocRet;

	for (SceUInt32 pos = 0; pos < count; ++pos) {
		NpImpsCacheFriendEntryImpl *entry = entries->At(pos);
		handles[pos] = entry;
		if (entry != NULL)
			entry->AddRef();
	}

	observer->OnFriendListEvent(eventType, count, handles);
	npImpsCacheFreeFriendHandleArray(count, handles);
	return 0;
}

int NpImpsCacheCoreImpl::NotifyFriendListLoaded(
	const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *entries)
{
	return dispatchFriendVectorEvent(m_observer, 1, entries);
}

int NpImpsCacheCoreImpl::NotifyFriendListAdded(
	const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *entries)
{
	return dispatchFriendVectorEvent(m_observer, 2, entries);
}

int NpImpsCacheCoreImpl::NotifyFriendListUpdated(
	const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *entries)
{
	return dispatchFriendVectorEvent(m_observer, 3, entries);
}

/* 0x813006DE */
int NpImpsCacheCoreImpl::NotifyFriendsRemoved(const NpImpsCacheNpIdSet *npIds)
{
	if (m_observer == NULL || npIds == NULL || npIds->Size() == 0)
		return 0;
	SceUInt32 count = npIds->Size();
	SceNpId *copy = NULL;
	int ret = npImpsCacheAllocNpIdArray(count, &copy);
	if (ret < 0)
		return ret;
	for (SceUInt32 pos = 0; pos < count; ++pos)
		copy[pos] = *npIds->At(pos);
	m_observer->OnFriendRemovedEvent(count, copy);
	npImpsCacheFree(copy);
	return 0;
}

int NpImpsCacheCoreImpl::NotifyFriendRequestsRemoved(const NpImpsCacheNpIdSet *npIds)
{
	if (m_observer == NULL || npIds == NULL || npIds->Size() == 0)
		return 0;
	m_observer->OnFriendRequestRemovedEvent(npIds->Size(), npIds->Data());
	return 0;
}

int NpImpsCacheCoreImpl::NotifyFriendContextRemoved(const NpImpsCacheNpIdSet *npIds)
{
	if (m_observer == NULL || npIds == NULL || npIds->Size() == 0)
		return 0;
	m_observer->OnFriendRemovedEvent(npIds->Size(), npIds->Data());
	return 0;
}

int NpImpsCacheCoreImpl::NotifyBlockListLoaded(const NpImpsCacheNpIdSet *npIds)
{
	if (m_observer == NULL || npIds == NULL)
		return 0;
	m_observer->OnBlockListEvent(1, npIds->Size(), npIds->Data());
	return 0;
}

int NpImpsCacheCoreImpl::NotifyBlockListAdded(const NpImpsCacheNpIdSet *npIds)
{
	if (m_observer == NULL || npIds == NULL || npIds->Size() == 0)
		return 0;
	m_observer->OnBlockListEvent(2, npIds->Size(), npIds->Data());
	return 0;
}

int NpImpsCacheCoreImpl::NotifyBlockListRemoved(const NpImpsCacheNpIdSet *npIds)
{
	if (m_observer == NULL || npIds == NULL || npIds->Size() == 0)
		return 0;
	m_observer->OnBlockListEvent(3, npIds->Size(), npIds->Data());
	return 0;
}

int NpImpsCacheCoreImpl::NotifyFriendContextChanged(NpImpsCacheFriendEntryImpl **entry,
						    int contextState, void *extra)
{
	if (m_observer == NULL || entry == NULL)
		return 0;
	NpImpsCacheFriendPtr handle = *entry;
	if (handle != NULL)
		handle->AddRef();
	m_observer->OnFriendContextEvent(&handle, contextState, extra);
	if (handle != NULL)
		handle->Release();
	return 0;
}

int NpImpsCacheCoreImpl::NotifyFriendPresenceChanged(NpImpsCacheFriendEntryImpl **entry,
						     NpImpsSvcPresenceData **curPresence,
						     NpImpsSvcPresenceData **prevPresence,
						     const NpImpsSvcPresenceDiffFlags *diffFlags)
{
	if (m_observer == NULL || entry == NULL)
		return 0;
	NpImpsCacheFriendPtr handle = *entry;
	if (handle != NULL)
		handle->AddRef();
	m_observer->OnFriendPresenceEvent(&handle, curPresence, prevPresence, diffFlags);
	if (handle != NULL)
		handle->Release();
	return 0;
}

int NpImpsCacheCoreImpl::NotifyPlatformPresenceChanged(NpImpsCacheFriendEntryImpl **entry,
						       NpImpsSvcPresenceData **curPresence,
						       NpImpsSvcPresenceData **prevPresence,
						       const NpImpsSvcPresenceDiffFlags *diffFlags,
						       int platform)
{
	if (m_observer == NULL || entry == NULL)
		return 0;
	NpImpsCacheFriendPtr handle = *entry;
	if (handle != NULL)
		handle->AddRef();
	m_observer->OnPlatformFriendPresenceEvent(&handle, curPresence, prevPresence,
						  diffFlags, platform);
	if (handle != NULL)
		handle->Release();
	return 0;
}

int NpImpsCacheCoreImpl::NotifyInGameDataMessage(NpImpsSvcInGameDataMessage **msg)
{
	if (m_observer == NULL || msg == NULL)
		return 0;
	m_observer->OnInGameDataMessageEvent(msg);
	return 0;
}

/* Forward SceNpImpsCore state change to observer (called from ImpsCoreStateEvent) */
int NpImpsCacheCoreImpl::ForwardImpsCoreState(int coreState, int errorCode)
{
	if (m_observer != NULL)
		m_observer->OnImpsCoreStateEvent(coreState, errorCode);
	return 0;
}

/* 0x813017D6..0x8130187D: retry-sync flag helpers */
void NpImpsCacheCoreImpl::MarkFriendSyncRetry()
{
	m_dataMutex.Lock();
	m_retryFriendSync = 1;
	m_dataMutex.Unlock();
}

void NpImpsCacheCoreImpl::ClearFriendSyncRetry()
{
	m_dataMutex.Lock();
	m_retryFriendSync = 0;
	m_dataMutex.Unlock();
}

/* 0x8130180E */
bool NpImpsCacheCoreImpl::NeedsFriendSyncRetry()
{
	m_dataMutex.Lock();
	bool needed = (m_retryFriendSync != 0);
	m_dataMutex.Unlock();
	return needed;
}

void NpImpsCacheCoreImpl::MarkBlockSyncRetry()
{
	m_dataMutex.Lock();
	m_retryBlockSync = 1;
	m_dataMutex.Unlock();
}

void NpImpsCacheCoreImpl::ClearBlockSyncRetry()
{
	m_dataMutex.Lock();
	m_retryBlockSync = 0;
	m_dataMutex.Unlock();
}

/* 0x81301862 */
bool NpImpsCacheCoreImpl::NeedsBlockSyncRetry()
{
	m_dataMutex.Lock();
	bool needed = (m_retryBlockSync != 0);
	m_dataMutex.Unlock();
	return needed;
}

/* 0x81300EC8: event enqueue wrappers (0x81300D46..0x813017B5) */
int NpImpsCacheCoreImpl::EnqueueImpsCoreState(int coreState, int errorCode)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateCoreStateEvent(&event, &m_friendStore, &m_blockStore,
						  coreState, errorCode, this);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x81300FEE */
int NpImpsCacheCoreImpl::EnqueueFriendRosterAdded(const NpImpsCoreRosterBatch *batch)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateFriendRosterBatchAddEvent(&event, &m_friendStore, batch);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x81300F66 */
int NpImpsCacheCoreImpl::EnqueueFriendRosterRemoved(const NpImpsCoreRosterBatch *batch)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateFriendRosterBatchRemoveEvent(&event, &m_friendStore, batch);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x8130113A */
int NpImpsCacheCoreImpl::EnqueueBlockListRefresh()
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateRefreshBlockListEvent(&event, &m_blockStore);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x813011E8 */
int NpImpsCacheCoreImpl::EnqueuePresenceUpdate(const SceNpId *npId,
					       const NpImpsCorePresenceUpdate *update)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreatePresenceUpdateEvent(&event, &m_friendStore, npId, update);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x8130129E */
int NpImpsCacheCoreImpl::EnqueueInGameDataMessage(const SceNpId *senderNpId,
						  const NpImpsCoreMessage *msg)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateInGameMessageEvent(&event, senderNpId, msg, this);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x8130134A */
int NpImpsCacheCoreImpl::EnqueueNotification(int notifyCode)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateNotificationEvent(&event, notifyCode, &m_friendStore,
						     &m_blockStore, this);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x81301724 */
int NpImpsCacheCoreImpl::EnqueueRetrySync()
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateRetrySyncEvent(&event, this, &m_friendStore,
						  &m_blockStore, this);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x81300DD4 */
int NpImpsCacheCoreImpl::EnqueueSignInSync()
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateSignInSyncEvent(&event, &m_friendStore, &m_blockStore, this);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x81300D46 */
int NpImpsCacheCoreImpl::EnqueueSignOutClear()
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateSignOutClearEvent(&event, &m_friendStore, &m_blockStore, this);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x8130148A */
int NpImpsCacheCoreImpl::EnqueueBlockAdd(const SceNpId *npId)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateBlockAddEvent(&event, &m_friendStore, &m_blockStore, 1, npId);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x81301400 */
int NpImpsCacheCoreImpl::EnqueueBlockRemove(const SceNpId *npId)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateBlockRemoveEvent(&event, &m_blockStore, 1, npId);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x813015A0 */
int NpImpsCacheCoreImpl::EnqueueFriendAddOrUpdate(const SceNpId *npId, int relation)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateFriendAddOrUpdateEvent(&event, &m_friendStore, npId, relation);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x81301518 */
int NpImpsCacheCoreImpl::EnqueueFriendRemove(const SceNpId *npId)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateFriendRemoveEvent(&event, &m_friendStore, npId);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x8130162C, 0x81301688: NpImpsCacheRosterSync overrides */
int NpImpsCacheCoreImpl::TriggerRosterUpdate(const SceNpId *npId, int syncReason)
{
	int ret;
	switch (syncReason) {
	case NP_IMPS_ROSTER_SYNC_REQUEST_FRIEND:
		ret = EnqueueFriendAddOrUpdate(npId, NP_IMPS_CACHE_REL_PENDING_OUT);
		break;
	case NP_IMPS_ROSTER_SYNC_ACCEPT_FRIEND:
		ret = EnqueueFriendAddOrUpdate(npId, NP_IMPS_CACHE_REL_ACCEPTED);
		break;
	case NP_IMPS_ROSTER_SYNC_DELETE_FRIEND:
		ret = EnqueueFriendRemove(npId);
		break;
	case NP_IMPS_ROSTER_SYNC_ADD_BLOCK:
		ret = EnqueueBlockAdd(npId);
		break;
	case NP_IMPS_ROSTER_SYNC_DELETE_BLOCK:
		ret = EnqueueBlockRemove(npId);
		break;
	default:
		return NP_IMPS_SVC_ERROR_INTERNAL;
	}

	if (ret >= 0) {
		m_waitCond.Notify();
		return 0;
	}
	return ret;
}

int NpImpsCacheCoreImpl::ForceFullSync(int syncFlags)
{
	NpImpsCacheEvent *event = NULL;
	int ret = npImpsCacheCreateForceSyncEvent(&event, syncFlags, &m_friendStore,
						  &m_blockStore, this);
	if (ret >= 0) {
		NpImpsCacheEvent *held = event;
		if (held != NULL)
			held->AddRef();
		NpImpsCacheEvent *queued = held;
		if (queued != NULL)
			queued->AddRef();
		ret = m_eventQueue->Push(&queued);
		if (queued != NULL)
			queued->Release();
		if (ret == NP_IMPS_CACHE_ERROR_QUEUE_FULL)
			m_queueOverflowed = 1;
		if (held != NULL)
			held->Release();
		if (ret >= 0)
			m_waitCond.Notify();
	}
	if (event != NULL)
		event->Release();
	return ret;
}

/* 0x81300E60..0x813017D5: static callback trampolines from SceNpImpsCore & Account */
void NpImpsCacheCoreImpl::OnCoreCtlEvent(int eventType)
{
	if (s_cacheCore == NULL)
		return;
	if (eventType == 0) {
		if (s_cacheCore->EnqueueSignInSync() >= 0)
			s_cacheCore->SignalWorker();
	} else if (eventType == 1) {
		if (s_cacheCore->EnqueueSignOutClear() >= 0)
			s_cacheCore->SignalWorker();
	}
}

void NpImpsCacheCoreImpl::OnCoreSessionState(const NpImpsSessionIdentity *session, int coreState,
					     int errorCode)
{
	(void)session;
	if (s_cacheCore == NULL)
		return;
	if (s_cacheCore->EnqueueImpsCoreState(coreState, errorCode) >= 0)
		s_cacheCore->SignalWorker();
}

void NpImpsCacheCoreImpl::OnCoreFriendRosterPush(const NpImpsSessionIdentity *session, int action,
						 const NpBasicRosterList *batch)
{
	(void)session;
	if (s_cacheCore == NULL || batch == NULL)
		return;
	if (action == NP_IMPS_CORE_NOTIFY_FRIEND_ADDED) {
		if (s_cacheCore->EnqueueFriendRosterAdded(batch) >= 0)
			s_cacheCore->SignalWorker();
	} else if (action == NP_IMPS_CORE_NOTIFY_FRIEND_REMOVED) {
		if (s_cacheCore->EnqueueFriendRosterRemoved(batch) >= 0)
			s_cacheCore->SignalWorker();
	}
}

void NpImpsCacheCoreImpl::OnCoreBlockListPush(const NpImpsSessionIdentity *session, int action,
					      const NpBasicPrivacyList *privacyList)
{
	(void)session;
	(void)privacyList;
	if (s_cacheCore == NULL)
		return;
	if (action == 0 && s_cacheCore->EnqueueBlockListRefresh() >= 0)
		s_cacheCore->SignalWorker();
}

void NpImpsCacheCoreImpl::OnCorePresencePush(const NpImpsSessionIdentity *session,
					     const SceNpId *npId,
					     const NpImpsCorePresenceUpdate *update)
{
	(void)session;
	if (s_cacheCore == NULL || update == NULL)
		return;
	int status = update->onlineStatus;
	if (status != 4 && status != 5 && status != 6 && status != 7) {
		s_cacheCore->EnqueuePresenceUpdate(npId, update);
		s_cacheCore->SignalWorker();
	}
}

void NpImpsCacheCoreImpl::OnCoreInGameDataMessage(const NpImpsSessionIdentity *session,
						  const SceNpId *senderNpId,
						  const NpImpsCoreMessage *msg)
{
	(void)session;
	if (s_cacheCore == NULL)
		return;
	if (s_cacheCore->EnqueueInGameDataMessage(senderNpId, msg) >= 0)
		s_cacheCore->SignalWorker();
}

void NpImpsCacheCoreImpl::OnCoreNotification(const NpImpsSessionIdentity *session, int notifyCode,
					     const SceRtcTick *timestamp)
{
	(void)session;
	(void)timestamp;
	if (s_cacheCore == NULL)
		return;
	if (s_cacheCore->EnqueueNotification(notifyCode) >= 0)
		s_cacheCore->SignalWorker();
}

/* 0x813017B6 */
void NpImpsCacheCoreImpl::OnAccountSessionCallback(int eventId, int stateGroup, int stateCode,
						   int detailA, int detailB, void *userArg)
{
	(void)detailA;
	(void)detailB;
	NpImpsCacheCoreImpl *core = reinterpret_cast<NpImpsCacheCoreImpl *>(userArg);
	if (core == NULL)
		return;
	if (stateGroup == 4 && (stateCode == 6 || eventId == 5)) {
		core->EnqueueRetrySync();
		core->SignalWorker();
	}
}
