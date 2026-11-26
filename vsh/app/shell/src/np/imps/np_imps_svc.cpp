/*
 * np_imps_svc.cpp: SceNpImpsSvc core service singleton, listener registry,
 * event fanout, friend/blocklist queries, and state trackers
 * (0x812F899C..0x812FC13F, 0x812FDB20..0x812FDC93).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/new>
#include <std/string.h>
#include <memory/heap_allocator.h>
#include "np_imps_svc.h"

static paf::memory::HeapAllocator *s_svcHeap = NULL;		/* 0x81546104 */
static NpImpsSvc *s_svcInstance = NULL;				/* 0x81546108 */

/* 0x812FDB20 */
int NpImpsSvcOnlineStatusTracker::GetStatus()
{
	m_mutex->Lock();
	int status = m_status;
	m_mutex->Unlock();
	return status;
}

/* 0x812FDB38 */
void NpImpsSvcOnlineStatusTracker::OnImpsCoreState(int coreState, SceUInt8 *outChanged)
{
	m_mutex->Lock();
	int cur = m_status;
	switch (coreState) {
	case 6:
	case 7:
		if (cur == NP_IMPS_SVC_ONLINE_ONLINE) {
			*outChanged = 0;
		} else {
			m_status = NP_IMPS_SVC_ONLINE_ONLINE;
			*outChanged = 1;
		}
		break;
	case 8:
		if (cur == NP_IMPS_SVC_ONLINE_AWAY) {
			*outChanged = 0;
		} else {
			m_status = NP_IMPS_SVC_ONLINE_AWAY;
			*outChanged = 1;
		}
		break;
	case 9:
	case 10:
	case 11:
		if (cur == NP_IMPS_SVC_ONLINE_OFFLINE) {
			*outChanged = 0;
		} else {
			m_status = NP_IMPS_SVC_ONLINE_OFFLINE;
			*outChanged = 1;
		}
		break;
	default:
		break;
	}
	m_mutex->Unlock();
}

/* 0x812FDBB4 */
bool NpImpsSvcSessionStateTracker::IsSignedIn()
{
	m_mutex->Lock();
	bool signedIn = (m_level > 0);
	m_mutex->Unlock();
	return signedIn;
}

/* 0x812FDBDA */
bool NpImpsSvcSessionStateTracker::IsOnline()
{
	m_mutex->Lock();
	bool online = (m_level >= 2);
	m_mutex->Unlock();
	return online;
}

/* 0x812FDC00 */
void NpImpsSvcSessionStateTracker::SetSignedIn()
{
	m_mutex->Lock();
	m_level = 1;
	m_mutex->Unlock();
}

/* 0x812FDC18 */
void NpImpsSvcSessionStateTracker::SetSignedOut()
{
	m_mutex->Lock();
	m_level = 0;
	m_mutex->Unlock();
}

/* 0x812FDC30 */
void NpImpsSvcSessionStateTracker::OnImpsCoreState(int coreState, SceUInt8 *outChanged)
{
	m_mutex->Lock();
	switch (coreState) {
	case 1:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
		if (m_level == 2) {
			SetSignedIn();
			*outChanged = 1;
		}
		break;
	case 6:
		if (m_level != 2) {
			m_level = 2;
			*outChanged = 1;
		}
		break;
	default:
		break;
	}
	m_mutex->Unlock();
}

/* 0x812F899C */
NpImpsSvc::NpImpsSvc(const NpImpsSvcInitParam *param, paf::thread::RMutex *cacheStateMutex)
	: m_cacheCore(NULL),
	  m_listenerMutex("SceNpImpsSvcRMutex", 0),
	  m_listenersHead(NULL),
	  m_listenerCount(0),
	  m_nextListenerId(1),
	  m_sessionState(cacheStateMutex),
	  m_onlineStatus(cacheStateMutex),
	  m_ctxMgr(param),
	  m_ctxFactory(),
	  m_myPresence(this)
{
}

/* 0x812F8C2A */
NpImpsSvc::~NpImpsSvc()
{
	m_listenerMutex.Lock();
	ListenerNode *cur = m_listenersHead;
	while (cur != NULL) {
		ListenerNode *next = cur->next;
		npImpsSvcFree(cur);
		cur = next;
	}
	m_listenersHead = NULL;
	m_listenerCount = 0;
	m_listenerMutex.Unlock();
}

/* 0x812F8A5A */
int NpImpsSvc::Init(void *heapMem, SceSize heapSize, const NpImpsSvcInitParam *param)
{
	if (s_svcInstance != NULL)
		return NP_IMPS_SVC_ERROR_ALREADY_INITIALIZED;
	if (heapMem == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;

	paf::memory::HeapAllocator *heap =
		new paf::memory::HeapAllocator(heapMem, heapSize, "SceNpImpsSvc", NULL);
	s_svcHeap = heap;
	if (heap == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	npImpsSvcHeapSet(heap);
	int ret = npImpsCacheInit(param->cacheThreadPriority, param->cacheThreadStackSize, heap);
	if (ret >= 0) {
		void *mem = npImpsSvcMalloc(sizeof(NpImpsSvc));
		if (mem != NULL) {
			NpImpsCacheCore *cacheCore = npImpsCacheGetCore();
			paf::thread::RMutex *stateMutex = cacheCore->GetStateMutex();
			NpImpsSvc *svc = new (mem) NpImpsSvc(param, stateMutex);
			svc->m_cacheCore = cacheCore;
			ret = cacheCore->RegisterObserver(svc);
			if (ret >= 0) {
				ret = svc->m_ctxMgr.StartManager();
				if (ret >= 0) {
					s_svcInstance = svc;
					return 0;
				}
			}
			svc->~NpImpsSvc();
			npImpsSvcFree(svc);
			npImpsSvcHeapClear();
		} else {
			ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		}
		npImpsCacheTerm();
	}

	if (s_svcHeap != NULL) {
		delete s_svcHeap;
		s_svcHeap = NULL;
	}
	return ret;
}

/* 0x812F8B8E */
int NpImpsSvc::Term()
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;

	s_svcInstance->m_ctxMgr.StopManager();
	s_svcInstance->m_cacheCore->UnregisterObserver();
	npImpsCacheTerm();
	s_svcInstance->m_cacheCore = NULL;

	s_svcInstance->~NpImpsSvc();
	npImpsSvcFree(s_svcInstance);
	npImpsSvcHeapClear();

	if (s_svcHeap != NULL) {
		delete s_svcHeap;
		s_svcHeap = NULL;
	}
	s_svcInstance = NULL;
	return 0;
}

/* 0x812F8C0C */
int NpImpsSvc::GetInstance(NpImpsSvc **outInstance)
{
	if (s_svcInstance == NULL)
		return NP_IMPS_SVC_ERROR_NOT_INITIALIZED;
	*outInstance = s_svcInstance;
	return 0;
}

/* 0x812F8CD8 */
void NpImpsSvc::FillPresenceSnapshotFromFriend(NpImpsSvcPresenceSnapshot *outSnapshot,
					       NpImpsCacheFriendEntry *friendEntry,
					       int platform)
{
	char titleNameBuf[128];
	char statusBuf[204];

	outSnapshot->onlineStatus = friendEntry->GetOnlineStatus(platform);
	outSnapshot->titlePlatform = friendEntry->GetTitlePlatform(platform);

	sce_paf_memset(titleNameBuf, 0, sizeof(titleNameBuf));
	friendEntry->GetTitleName(titleNameBuf, platform);
	sce_paf_strncpy(outSnapshot->titleName, titleNameBuf, sizeof(outSnapshot->titleName));

	friendEntry->GetTitleId(&outSnapshot->titleId, platform);
	friendEntry->GetCommId(&outSnapshot->commId, platform);

	outSnapshot->gamePresence.size = 0;
	outSnapshot->gamePresence.presenceType = friendEntry->GetGamePresenceType(platform);

	sce_paf_memset(statusBuf, 0, 201);
	friendEntry->GetGameStatus(statusBuf, platform);
	sce_paf_strncpy(outSnapshot->gamePresence.status, statusBuf,
			sizeof(outSnapshot->gamePresence.status));

	SceSize gameDataLen = friendEntry->GetGameDataSize(platform);
	if (gameDataLen > sizeof(outSnapshot->gamePresence.gameData))
		gameDataLen = sizeof(outSnapshot->gamePresence.gameData);
	outSnapshot->gamePresence.gameDataSize = gameDataLen;
	friendEntry->CopyGameData(outSnapshot->gamePresence.gameData,
				  sizeof(outSnapshot->gamePresence.gameData), platform);

	outSnapshot->partyPresence.size = sizeof(NpImpsPartyPresence);
	outSnapshot->partyPresence.presenceType = friendEntry->GetPartyPresenceType(platform);
	friendEntry->GetPartyTitle(outSnapshot->partyPresence.title, platform);
	friendEntry->GetPartyStatus(outSnapshot->partyPresence.status, platform);
	outSnapshot->partyPresence.partyDataSize = friendEntry->GetPartyDataSize(platform);
	friendEntry->CopyPartyData(outSnapshot->partyPresence.partyData,
				   sizeof(outSnapshot->partyPresence.partyData), platform);

	friendEntry->GetJoinableCommId(&outSnapshot->joinableCommId, platform);
	friendEntry->GetContentId(outSnapshot->contentId, platform);
}

/* 0x812F8F54 */
int NpImpsSvc::ReplayInitialPlatformPresence(const NpImpsSvcListenerCallbacks *cb)
{
	NpImpsCacheFriendStore *friendStore = m_cacheCore->GetFriendStore();
	SceUInt32 count = friendStore->GetCount();
	NpImpsCacheFriendPtr *handles =
		reinterpret_cast<NpImpsCacheFriendPtr *>(npImpsSvcMalloc(sizeof(NpImpsCacheFriendPtr) * count));
	if (handles == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	for (SceUInt32 idx = 0; idx < count; ++idx)
		handles[idx] = NULL;

	int ret = friendStore->CopyHandles(handles, count);
	if (ret >= 0) {
		for (SceUInt32 idx = 0; idx < count; ++idx) {
			for (int platform = NP_IMPS_SVC_PLATFORM_PS3;
			     platform >= NP_IMPS_SVC_PLATFORM_PSP2; --platform) {
				NpImpsSvcPresenceSnapshot snapshot;
				sce_paf_memset(&snapshot, 0, sizeof(snapshot));

				NpImpsCacheFriendEntry *entry = handles[idx];
				if (entry != NULL)
					entry->AddRef();
				FillPresenceSnapshotFromFriend(&snapshot, entry, platform);
				if (entry != NULL)
					entry->Release();

				NpImpsCacheFriendPtr held = handles[idx];
				if (held != NULL)
					held->AddRef();

				SceNpId npId;
				held->GetNpId(&npId);
				if (held->GetRelation() == 3 && cb->onPlatformFriendPresence != NULL) {
					cb->onPlatformFriendPresence(1, &held, &npId,
								     &snapshot, NULL, 0,
								     platform, cb->userArg);
				}
				if (held != NULL)
					held->Release();
			}
		}
		for (SceUInt32 idx = 0; idx < count; ++idx) {
			if (handles[idx] != NULL)
				handles[idx]->Release();
		}
		npImpsSvcFree(handles);
	}
	return ret;
}

/* 0x812F9154 */
int NpImpsSvc::ReplayInitialFriendPresence(const NpImpsSvcListenerCallbacks *cb)
{
	NpImpsCacheFriendStore *friendStore = m_cacheCore->GetFriendStore();
	SceUInt32 count = friendStore->GetCount();
	NpImpsCacheFriendPtr *handles =
		reinterpret_cast<NpImpsCacheFriendPtr *>(npImpsSvcMalloc(sizeof(NpImpsCacheFriendPtr) * count));
	if (handles == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	for (SceUInt32 idx = 0; idx < count; ++idx)
		handles[idx] = NULL;

	int ret = friendStore->CopyHandles(handles, count);
	if (ret >= 0) {
		for (SceUInt32 idx = 0; idx < count; ++idx) {
			NpImpsSvcPresenceSnapshot snapshot;
			sce_paf_memset(&snapshot, 0, sizeof(snapshot));

			NpImpsCacheFriendEntry *entry = handles[idx];
			if (entry != NULL)
				entry->AddRef();
			FillPresenceSnapshotFromFriend(&snapshot, entry, NP_IMPS_SVC_PLATFORM_PSP2);
			if (entry != NULL)
				entry->Release();

			NpImpsCacheFriendPtr held = handles[idx];
			if (held != NULL)
				held->AddRef();

			SceNpId npId;
			held->GetNpId(&npId);
			if (held->GetRelation() == 3 && cb->onFriendPresence != NULL) {
				cb->onFriendPresence(1, &held, &npId, &snapshot, NULL, 0, cb->userArg);
			}
			if (held != NULL)
				held->Release();
		}
		for (SceUInt32 idx = 0; idx < count; ++idx) {
			if (handles[idx] != NULL)
				handles[idx]->Release();
		}
		npImpsSvcFree(handles);
	}
	return ret;
}

/* 0x812F9332 */
int NpImpsSvc::ReplayInitialFriendContext(const NpImpsSvcListenerCallbacks *cb)
{
	NpImpsCacheFriendStore *friendStore = m_cacheCore->GetFriendStore();
	SceUInt32 count = friendStore->GetCount();
	NpImpsCacheFriendPtr *handles =
		reinterpret_cast<NpImpsCacheFriendPtr *>(npImpsSvcMalloc(sizeof(NpImpsCacheFriendPtr) * count));
	if (handles == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	for (SceUInt32 idx = 0; idx < count; ++idx)
		handles[idx] = NULL;

	int ret = friendStore->CopyHandles(handles, count);
	if (ret >= 0) {
		for (SceUInt32 idx = 0; idx < count; ++idx) {
			int friendCtx = handles[idx]->GetFriendContext();
			NpImpsCacheFriendPtr held = handles[idx];
			if (held != NULL)
				held->AddRef();

			SceNpId npId;
			held->GetNpId(&npId);
			if (held->GetRelation() == 3 && cb->onFriendContext != NULL) {
				cb->onFriendContext(1, &held, &npId, friendCtx, NULL, cb->userArg);
			}
			if (held != NULL)
				held->Release();
		}
		for (SceUInt32 idx = 0; idx < count; ++idx) {
			if (handles[idx] != NULL)
				handles[idx]->Release();
		}
		npImpsSvcFree(handles);
	}
	return ret;
}

/* 0x812F94B8 */
int NpImpsSvc::ReplayInitialFriendList(const NpImpsSvcListenerCallbacks *cb)
{
	NpImpsCacheFriendStore *friendStore = m_cacheCore->GetFriendStore();
	SceUInt32 count = friendStore->GetCount();
	NpImpsCacheFriendPtr *handles =
		reinterpret_cast<NpImpsCacheFriendPtr *>(npImpsSvcMalloc(sizeof(NpImpsCacheFriendPtr) * count));
	if (handles == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	for (SceUInt32 idx = 0; idx < count; ++idx)
		handles[idx] = NULL;

	int ret = friendStore->CopyHandles(handles, count);
	if (ret >= 0) {
		SceNpId *npIds =
			reinterpret_cast<SceNpId *>(npImpsSvcMalloc(sizeof(SceNpId) * count));
		if (npIds != NULL) {
			sce_paf_memset(npIds, 0, sizeof(SceNpId) * count);
			for (SceUInt32 idx = 0; idx < count; ++idx)
				handles[idx]->GetNpId(&npIds[idx]);

			NpImpsCacheFriendPtr *ownedRefs = reinterpret_cast<NpImpsCacheFriendPtr *>(
				npImpsSvcMalloc(sizeof(NpImpsCacheFriendPtr) * count));
			if (ownedRefs != NULL) {
				for (SceUInt32 idx = 0; idx < count; ++idx)
					ownedRefs[idx] = NULL;

				NpImpsCacheFriendPtr **handlePtrs =
					reinterpret_cast<NpImpsCacheFriendPtr **>(
						npImpsSvcMalloc(sizeof(NpImpsCacheFriendPtr *) * count));
				if (handlePtrs != NULL) {
					sce_paf_memset(handlePtrs, 0,
						       sizeof(NpImpsCacheFriendPtr *) * count);
					for (SceUInt32 idx = 0; idx < count; ++idx) {
						handlePtrs[idx] = &ownedRefs[idx];
						ownedRefs[idx] = handles[idx];
						if (ownedRefs[idx] != NULL)
							ownedRefs[idx]->AddRef();
					}
					if (cb->onFriendList != NULL)
						cb->onFriendList(1, npIds, handlePtrs, count, cb->userArg);
					npImpsSvcFree(handlePtrs);
				} else {
					ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
				}

				for (SceUInt32 idx = 0; idx < count; ++idx) {
					if (ownedRefs[idx] != NULL)
						ownedRefs[idx]->Release();
				}
				npImpsSvcFree(ownedRefs);
			} else {
				ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			}
			npImpsSvcFree(npIds);
		} else {
			ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		}
	}

	for (SceUInt32 idx = 0; idx < count; ++idx) {
		if (handles[idx] != NULL)
			handles[idx]->Release();
	}
	npImpsSvcFree(handles);
	return ret;
}

/* 0x812F96F4 */
int NpImpsSvc::RegisterListener(const NpImpsSvcListenerCallbacks *callbacks, SceUInt32 *outListenerId)
{
	NpImpsCacheFriendStore *friendStore = m_cacheCore->GetFriendStore();
	friendStore->Lock();
	NpImpsCacheBlockStore *blockStore = m_cacheCore->GetBlockStore();
	blockStore->Lock();
	m_onlineStatus.Lock();
	m_myPresence.Lock();
	m_listenerMutex.Lock();

	int ret = 0;
	if (callbacks == NULL) {
		ret = NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	} else if (callbacks->onServiceState == NULL &&
		   callbacks->onMyOnlineStatus == NULL &&
		   callbacks->onOnlineTransition == NULL &&
		   callbacks->onFriendList == NULL &&
		   callbacks->onFriendPresence == NULL &&
		   callbacks->onBlockList == NULL &&
		   callbacks->onInGameDataMessage == NULL &&
		   callbacks->onFriendRemoved == NULL &&
		   callbacks->onMyPresence == NULL &&
		   callbacks->onPlatformFriendPresence == NULL &&
		   callbacks->onFriendContext == NULL &&
		   callbacks->onFriendRequestRemoved == NULL) {
		ret = NP_IMPS_SVC_ERROR_NO_CALLBACK;
	} else {
		if (callbacks->onMyOnlineStatus != NULL)
			callbacks->onMyOnlineStatus(m_onlineStatus.GetStatus(), callbacks->userArg);

		if (callbacks->onMyPresence != NULL) {
			NpImpsSvcPresenceSnapshot mySnapshot;
			sce_paf_memset(&mySnapshot, 0, sizeof(mySnapshot));
			ret = m_myPresence.GetSnapshot(&mySnapshot);
			if (ret >= 0)
				callbacks->onMyPresence(1, &mySnapshot, &mySnapshot, callbacks->userArg);
		}

		if (ret >= 0 && callbacks->onFriendList != NULL)
			ret = ReplayInitialFriendList(callbacks);

		if (ret >= 0 && callbacks->onBlockList != NULL) {
			SceUInt32 blockCount = blockStore->GetCount(0);
			SceNpId *blockNpIds = reinterpret_cast<SceNpId *>(
				npImpsSvcMalloc(sizeof(SceNpId) * blockCount));
			if (blockNpIds != NULL) {
				sce_paf_memset(blockNpIds, 0, sizeof(SceNpId) * blockCount);
				SceUInt32 copied = 0;
				blockStore->CopyEntries(0, blockNpIds, blockCount, &copied, 0);
				callbacks->onBlockList(1, blockNpIds, blockCount, callbacks->userArg);
				npImpsSvcFree(blockNpIds);
			}
		}

		if (ret >= 0 && callbacks->onFriendContext != NULL)
			ret = ReplayInitialFriendContext(callbacks);

		if (ret >= 0 && callbacks->onFriendPresence != NULL)
			ret = ReplayInitialFriendPresence(callbacks);

		if (ret >= 0 && callbacks->onPlatformFriendPresence != NULL)
			ret = ReplayInitialPlatformPresence(callbacks);

		if (ret >= 0) {
			ListenerNode *node =
				reinterpret_cast<ListenerNode *>(npImpsSvcMalloc(sizeof(ListenerNode)));
			if (node == NULL) {
				ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			} else {
				node->id = m_nextListenerId++;
				node->callbacks = *callbacks;
				node->next = NULL;

				if (m_listenersHead == NULL) {
					m_listenersHead = node;
				} else {
					ListenerNode *tail = m_listenersHead;
					while (tail->next != NULL)
						tail = tail->next;
					tail->next = node;
				}
				m_listenerCount++;
				*outListenerId = node->id;
			}
		}
	}

	m_listenerMutex.Unlock();
	m_myPresence.Unlock();
	m_onlineStatus.Unlock();
	blockStore->Unlock();
	friendStore->Unlock();
	return ret;
}

/* 0x812F9988 */
int NpImpsSvc::UnregisterListener(SceUInt32 listenerId)
{
	if (listenerId == 0)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;

	NpImpsCacheFriendStore *friendStore = m_cacheCore->GetFriendStore();
	friendStore->Lock();
	NpImpsCacheBlockStore *blockStore = m_cacheCore->GetBlockStore();
	blockStore->Lock();
	m_onlineStatus.Lock();
	m_myPresence.Lock();
	m_listenerMutex.Lock();

	int ret = NP_IMPS_SVC_ERROR_LISTENER_NOT_FOUND;
	ListenerNode **link = &m_listenersHead;
	while (*link != NULL) {
		ListenerNode *cur = *link;
		if (cur->id == listenerId) {
			*link = cur->next;
			npImpsSvcFree(cur);
			m_listenerCount--;
			ret = 0;
			break;
		}
		link = &cur->next;
	}

	m_listenerMutex.Unlock();
	m_myPresence.Unlock();
	m_onlineStatus.Unlock();
	blockStore->Unlock();
	friendStore->Unlock();
	return ret;
}

/* 0x812F9B1A */
int NpImpsSvc::IsOnline(bool *outIsOnline)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	*outIsOnline = m_sessionState.IsOnline();
	return 0;
}

/* 0x812F9B42 */
void NpImpsSvc::DispatchServiceState(int state, int errorCode)
{
	m_listenerMutex.Lock();
	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onServiceState != NULL)
			cur->callbacks.onServiceState(state, errorCode, cur->callbacks.userArg);
	}
	m_listenerMutex.Unlock();
}

/* 0x812F9BDE */
void NpImpsSvc::OnAccountSessionEvent(int eventType)
{
	if (eventType == 0) {
		m_sessionState.SetSignedOut();
		DispatchServiceState(NP_IMPS_SVC_STATE_SIGNED_OUT, 0);
	} else if (eventType == 1) {
		m_myPresence.PublishCurrentPresence();
		m_sessionState.SetSignedIn();
		DispatchServiceState(NP_IMPS_SVC_STATE_SIGNED_IN, 0);
	}
}

/* 0x812F9C20 */
void NpImpsSvc::DispatchMyOnlineStatus(int status)
{
	m_listenerMutex.Lock();
	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onMyOnlineStatus != NULL)
			cur->callbacks.onMyOnlineStatus(status, cur->callbacks.userArg);
	}
	m_listenerMutex.Unlock();
}

/* 0x812F9CA8 */
void NpImpsSvc::DispatchOnlineTransition(int isOnline)
{
	m_listenerMutex.Lock();
	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onOnlineTransition != NULL)
			cur->callbacks.onOnlineTransition(isOnline, cur->callbacks.userArg);
	}
	m_listenerMutex.Unlock();
}

/* 0x812F9D30 */
void NpImpsSvc::OnImpsCoreStateEvent(int coreState, int errorCode)
{
	SceUInt8 changed = 0;
	m_sessionState.OnImpsCoreState(coreState, &changed);
	if (changed != 0) {
		if (m_sessionState.IsOnline()) {
			m_myPresence.PublishCurrentPresence();
			DispatchOnlineTransition(1);
			DispatchServiceState(NP_IMPS_SVC_STATE_ONLINE, errorCode);
		} else {
			DispatchOnlineTransition(0);
			DispatchServiceState(NP_IMPS_SVC_STATE_SIGNED_IN, errorCode);
		}
	}

	m_onlineStatus.Lock();
	changed = 0;
	m_onlineStatus.OnImpsCoreState(coreState, &changed);
	if (changed != 0)
		DispatchMyOnlineStatus(m_onlineStatus.GetStatus());
	m_onlineStatus.Unlock();
}

/* 0x812F9DE4 */
void NpImpsSvc::DispatchFriendList(int eventType, NpImpsCacheFriendPtr *handles, SceSize count)
{
	m_listenerMutex.Lock();

	SceNpId *npIds = NULL;
	NpImpsCacheFriendPtr *ownedRefs = NULL;
	NpImpsCacheFriendPtr **handlePtrs = NULL;
	bool ready = true;

	if (count != 0) {
		npIds = reinterpret_cast<SceNpId *>(npImpsSvcMalloc(sizeof(SceNpId) * count));
		if (npIds == NULL) {
			ready = false;
		} else {
			sce_paf_memset(npIds, 0, sizeof(SceNpId) * count);
			ownedRefs = reinterpret_cast<NpImpsCacheFriendPtr *>(
				npImpsSvcMalloc(sizeof(NpImpsCacheFriendPtr) * count));
			if (ownedRefs == NULL) {
				ready = false;
			} else {
				handlePtrs = reinterpret_cast<NpImpsCacheFriendPtr **>(
					npImpsSvcMalloc(sizeof(NpImpsCacheFriendPtr *) * count));
				if (handles == NULL || handlePtrs == NULL) {
					ready = false;
				} else {
					for (SceSize idx = 0; idx < count; ++idx) {
						handles[idx]->GetNpId(&npIds[idx]);
						ownedRefs[idx] = handles[idx];
						if (ownedRefs[idx] != NULL)
							ownedRefs[idx]->AddRef();
						handlePtrs[idx] = &ownedRefs[idx];
					}
				}
			}
		}
	}

	if (ready) {
		for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
			if (cur->callbacks.onFriendList != NULL)
				cur->callbacks.onFriendList(eventType, npIds, handlePtrs,
							    count, cur->callbacks.userArg);
		}
		if (ownedRefs != NULL) {
			for (SceSize idx = 0; idx < count; ++idx) {
				if (ownedRefs[idx] != NULL)
					ownedRefs[idx]->Release();
			}
		}
	}

	if (handlePtrs != NULL)
		npImpsSvcFree(handlePtrs);
	if (ownedRefs != NULL)
		npImpsSvcFree(ownedRefs);
	if (npIds != NULL)
		npImpsSvcFree(npIds);

	m_listenerMutex.Unlock();
}

/* 0x812FA004 */
void NpImpsSvc::OnFriendListEvent(int eventType, SceSize count, NpImpsCacheFriendPtr *handles)
{
	switch (eventType) {
	case 1:
		DispatchFriendList(4, handles, count);
		break;
	case 2:
		DispatchFriendList(2, handles, count);
		break;
	case 3:
		DispatchFriendList(3, handles, count);
		break;
	case 4:
		DispatchFriendList(5, handles, count);
		break;
	default:
		break;
	}
}

/* 0x812FA044, 0x812FA0E0 */
void NpImpsSvc::DispatchFriendRemoved(const SceNpId *npIds, SceSize count)
{
	m_listenerMutex.Lock();
	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onFriendRemoved != NULL)
			cur->callbacks.onFriendRemoved(npIds, count, cur->callbacks.userArg);
	}
	m_listenerMutex.Unlock();
}

void NpImpsSvc::OnFriendRemovedEvent(SceSize count, const SceNpId *npIds)
{
	DispatchFriendRemoved(npIds, count);
}

/* 0x812FA0E8, 0x812FA184 */
void NpImpsSvc::DispatchFriendRequestRemoved(const SceNpId *npIds, SceSize count)
{
	m_listenerMutex.Lock();
	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onFriendRequestRemoved != NULL)
			cur->callbacks.onFriendRequestRemoved(npIds, count, cur->callbacks.userArg);
	}
	m_listenerMutex.Unlock();
}

void NpImpsSvc::OnFriendRequestRemovedEvent(SceSize count, const SceNpId *npIds)
{
	DispatchFriendRequestRemoved(npIds, count);
}

/* 0x812FA18C */
void NpImpsSvc::DispatchFriendContext(int eventType, NpImpsCacheFriendPtr *handle,
				      int contextState, void *extra)
{
	m_listenerMutex.Lock();
	NpImpsCacheFriendPtr held = *handle;
	if (held != NULL)
		held->AddRef();

	SceNpId npId;
	held->GetNpId(&npId);
	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onFriendContext != NULL)
			cur->callbacks.onFriendContext(eventType, &held, &npId,
						       contextState, extra, cur->callbacks.userArg);
	}

	if (held != NULL)
		held->Release();
	m_listenerMutex.Unlock();
}

/* 0x812FA2BC */
void NpImpsSvc::OnFriendContextEvent(NpImpsCacheFriendPtr *handle, int contextState, void *extra)
{
	NpImpsCacheFriendPtr held = *handle;
	if (held != NULL)
		held->AddRef();
	DispatchFriendContext(2, &held, contextState, extra);
	if (held != NULL)
		held->Release();
}

/* 0x812FA304 */
void NpImpsSvc::FillPresenceSnapshotFromData(NpImpsSvcPresenceSnapshot *outSnapshot,
					     const NpImpsSvcPresenceData *data)
{
	outSnapshot->onlineStatus = data->GetOnlineStatus();
	outSnapshot->platform = data->GetPlatform();
	outSnapshot->titlePlatform = data->GetTitlePlatform();

	if (data->HasTitle()) {
		if (data->GetTitleName() != NULL)
			sce_paf_strncpy(outSnapshot->titleName, data->GetTitleName(),
					sizeof(outSnapshot->titleName));
		sce_paf_memcpy(&outSnapshot->titleId, data->GetTitleId(), sizeof(SceNpCommunicationId));
		if (data->GetContentId() != NULL)
			sce_paf_memcpy(outSnapshot->contentId, data->GetContentId(),
				       sizeof(outSnapshot->contentId));
		sce_paf_memcpy(&outSnapshot->joinableCommId, data->GetJoinableCommId(),
			       sizeof(SceNpCommunicationId));

		if (data->HasCommId()) {
			sce_paf_memcpy(&outSnapshot->commId, data->GetCommId(),
				       sizeof(SceNpCommunicationId));
		} else {
			sce_paf_memset(&outSnapshot->commId, 0, sizeof(SceNpCommunicationId));
		}

		if (data->HasGamePresence()) {
			outSnapshot->gamePresence.presenceType = data->GetGamePresenceType();
			if (data->GetGameStatus() != NULL)
				sce_paf_strncpy(outSnapshot->gamePresence.status,
						data->GetGameStatus(),
						sizeof(outSnapshot->gamePresence.status));
			SceSize dataSize = data->GetGameDataSize();
			outSnapshot->gamePresence.gameDataSize = dataSize;
			if (dataSize != 0)
				data->CopyGameData(outSnapshot->gamePresence.gameData,
						   sizeof(outSnapshot->gamePresence.gameData));
		} else {
			sce_paf_memset(&outSnapshot->gamePresence, 0,
				       sizeof(outSnapshot->gamePresence));
		}
	} else {
		sce_paf_memset(outSnapshot->titleName, 0,
			       offsetof(NpImpsSvcPresenceSnapshot, partyPresence) -
			       offsetof(NpImpsSvcPresenceSnapshot, titleName));
	}

	if (!data->HasPartyPresence()) {
		sce_paf_memset(&outSnapshot->partyPresence, 0, sizeof(outSnapshot->partyPresence));
		return;
	}

	outSnapshot->partyPresence.presenceType = data->GetPartyPresenceType();
	if (data->GetPartyTitle() != NULL)
		sce_paf_strncpy(outSnapshot->partyPresence.title, data->GetPartyTitle(),
				sizeof(outSnapshot->partyPresence.title));
	if (data->GetPartyStatus() != NULL)
		sce_paf_strncpy(outSnapshot->partyPresence.status, data->GetPartyStatus(),
				sizeof(outSnapshot->partyPresence.status));
	SceSize partyLen = data->GetPartyDataSize();
	outSnapshot->partyPresence.partyDataSize = partyLen;
	if (partyLen != 0)
		data->CopyPartyData(outSnapshot->partyPresence.partyData,
				    sizeof(outSnapshot->partyPresence.partyData));
}

/* 0x812FA4A0 */
void NpImpsSvc::DispatchFriendPresence(int eventType, NpImpsCacheFriendPtr *handle,
				       NpImpsSvcPresenceData *curPresence,
				       NpImpsSvcPresenceData *prevPresence, SceUInt32 diffMask)
{
	m_listenerMutex.Lock();
	NpImpsCacheFriendPtr held = *handle;
	if (held != NULL)
		held->AddRef();

	NpImpsSvcPresenceSnapshot curSnap;
	NpImpsSvcPresenceSnapshot prevSnap;
	sce_paf_memset(&curSnap, 0, sizeof(curSnap));
	sce_paf_memset(&prevSnap, 0, sizeof(prevSnap));
	FillPresenceSnapshotFromData(&curSnap, curPresence);
	FillPresenceSnapshotFromData(&prevSnap, prevPresence);

	SceNpId npId;
	(*handle)->GetNpId(&npId);
	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onFriendPresence != NULL)
			cur->callbacks.onFriendPresence(eventType, &held, &npId,
							&curSnap, &prevSnap, diffMask,
							cur->callbacks.userArg);
	}

	if (held != NULL)
		held->Release();
	m_listenerMutex.Unlock();
}

/* 0x812FA652 */
void NpImpsSvc::BuildDiffMask(SceUInt32 *outMask, const NpImpsSvcPresenceDiffFlags *diffFlags)
{
	if (diffFlags->onlineStatusChanged != 0)
		*outMask |= NP_IMPS_DIFF_ONLINE_STATUS;
	if (diffFlags->platformChanged != 0)
		*outMask |= NP_IMPS_DIFF_PLATFORM;
	if (diffFlags->commentChanged != 0)
		*outMask |= NP_IMPS_DIFF_COMMENT;
	if (diffFlags->titlePlatformChanged != 0)
		*outMask |= NP_IMPS_DIFF_TITLE_PLATFORM;
	if (diffFlags->lastOnlineChanged != 0)
		*outMask |= NP_IMPS_DIFF_LAST_ONLINE;
	if (diffFlags->titleChanged != 0)
		*outMask |= NP_IMPS_DIFF_TITLE;
	if (diffFlags->commIdChanged != 0)
		*outMask |= NP_IMPS_DIFF_COMM_ID;
	if (diffFlags->gamePresenceChanged != 0)
		*outMask |= NP_IMPS_DIFF_GAME_PRESENCE;
	if (diffFlags->partyPresenceChanged != 0)
		*outMask |= NP_IMPS_DIFF_PARTY_PRESENCE;
	if (diffFlags->contentInfoChanged != 0)
		*outMask |= NP_IMPS_DIFF_CONTENT_INFO;
}

/* 0x812FA6E8 */
void NpImpsSvc::OnFriendPresenceEvent(NpImpsCacheFriendPtr *handle,
				      NpImpsSvcPresenceData **curPresence,
				      NpImpsSvcPresenceData **prevPresence,
				      const NpImpsSvcPresenceDiffFlags *diffFlags)
{
	SceUInt32 mask = 0;
	BuildDiffMask(&mask, diffFlags);

	NpImpsCacheFriendPtr held = *handle;
	NpImpsSvcPresenceData *curRef = *curPresence;
	NpImpsSvcPresenceData *prevRef = *prevPresence;
	if (held != NULL)
		held->AddRef();
	if (curRef != NULL)
		curRef->AddRef();
	if (prevRef != NULL)
		prevRef->AddRef();

	DispatchFriendPresence(2, &held, curRef, prevRef, mask);

	if (prevRef != NULL)
		prevRef->Release();
	if (curRef != NULL)
		curRef->Release();
	if (held != NULL)
		held->Release();
}

/* 0x812FA770 */
void NpImpsSvc::DispatchPlatformFriendPresence(int eventType, NpImpsCacheFriendPtr *handle,
					       NpImpsSvcPresenceData *curPresence,
					       NpImpsSvcPresenceData *prevPresence,
					       SceUInt32 diffMask, int platform)
{
	m_listenerMutex.Lock();
	NpImpsCacheFriendPtr held = *handle;
	if (held != NULL)
		held->AddRef();

	NpImpsSvcPresenceSnapshot curSnap;
	NpImpsSvcPresenceSnapshot prevSnap;
	sce_paf_memset(&curSnap, 0, sizeof(curSnap));
	sce_paf_memset(&prevSnap, 0, sizeof(prevSnap));
	FillPresenceSnapshotFromData(&curSnap, curPresence);
	FillPresenceSnapshotFromData(&prevSnap, prevPresence);

	SceNpId npId;
	(*handle)->GetNpId(&npId);
	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onPlatformFriendPresence != NULL)
			cur->callbacks.onPlatformFriendPresence(eventType, &held, &npId,
								&curSnap, &prevSnap, diffMask,
								platform, cur->callbacks.userArg);
	}

	if (held != NULL)
		held->Release();
	m_listenerMutex.Unlock();
}

/* 0x812FA930 */
void NpImpsSvc::OnPlatformFriendPresenceEvent(NpImpsCacheFriendPtr *handle,
					      NpImpsSvcPresenceData **curPresence,
					      NpImpsSvcPresenceData **prevPresence,
					      const NpImpsSvcPresenceDiffFlags *diffFlags,
					      int platform)
{
	SceUInt32 mask = 0;
	BuildDiffMask(&mask, diffFlags);

	NpImpsCacheFriendPtr held = *handle;
	NpImpsSvcPresenceData *curRef = *curPresence;
	NpImpsSvcPresenceData *prevRef = *prevPresence;
	if (held != NULL)
		held->AddRef();
	if (curRef != NULL)
		curRef->AddRef();
	if (prevRef != NULL)
		prevRef->AddRef();

	DispatchPlatformFriendPresence(2, &held, curRef, prevRef, mask, platform);

	if (prevRef != NULL)
		prevRef->Release();
	if (curRef != NULL)
		curRef->Release();
	if (held != NULL)
		held->Release();
}

/* 0x812FA9C0 */
void NpImpsSvc::DispatchBlockList(int eventType, const SceNpId *npIds, SceSize count)
{
	m_listenerMutex.Lock();
	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onBlockList != NULL)
			cur->callbacks.onBlockList(eventType, npIds, count, cur->callbacks.userArg);
	}
	m_listenerMutex.Unlock();
}

/* 0x812FAA6A */
void NpImpsSvc::OnBlockListEvent(int eventType, SceSize count, const SceNpId *npIds)
{
	if (eventType == 1)
		DispatchBlockList(2, npIds, count);
	else if (eventType == 2)
		DispatchBlockList(3, npIds, count);
	else if (eventType == 3)
		DispatchBlockList(4, npIds, count);
}

/* 0x812FAA9E */
void NpImpsSvc::DispatchInGameDataMessage(int eventType, const SceNpId *senderNpId,
					  const NpImpsSvcInGameDataPayload *payload)
{
	m_listenerMutex.Lock();
	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onInGameDataMessage != NULL)
			cur->callbacks.onInGameDataMessage(eventType, senderNpId, payload,
							   cur->callbacks.userArg);
	}
	m_listenerMutex.Unlock();
}

/* 0x812FAB48 */
void NpImpsSvc::OnInGameDataMessageEvent(NpImpsSvcInGameDataMessage **msg)
{
	NpImpsSvcInGameDataPayload payload;
	sce_paf_memset(&payload, 0, sizeof(payload));
	sce_paf_memcpy(&payload.commId, (*msg)->GetCommId(), sizeof(SceNpCommunicationId));
	SceSize copyLen = (*msg)->GetPayloadSize();
	if (copyLen > sizeof(payload.data))
		copyLen = sizeof(payload.data);
	sce_paf_memcpy(payload.data, (*msg)->GetPayload(), copyLen);
	payload.dataSize = (*msg)->GetPayloadSize();
	DispatchInGameDataMessage(1, (*msg)->GetSenderNpId(), &payload);
}

/* 0x812FAC04 */
void NpImpsSvc::DispatchMyPresence(int eventType, NpImpsSvcPresenceData *curPresence,
				   NpImpsSvcPresenceData *prevPresence)
{
	m_listenerMutex.Lock();

	NpImpsSvcPresenceSnapshot curSnap;
	NpImpsSvcPresenceSnapshot prevSnap;
	sce_paf_memset(&curSnap, 0, sizeof(curSnap));
	sce_paf_memset(&prevSnap, 0, sizeof(prevSnap));
	FillPresenceSnapshotFromData(&curSnap, curPresence);
	FillPresenceSnapshotFromData(&prevSnap, prevPresence);

	for (ListenerNode *cur = m_listenersHead; cur != NULL; cur = cur->next) {
		if (cur->callbacks.onMyPresence != NULL)
			cur->callbacks.onMyPresence(eventType, &curSnap, &prevSnap,
						    cur->callbacks.userArg);
	}
	m_listenerMutex.Unlock();
}

/* 0x812FAD14 */
void NpImpsSvc::OnMyPresenceUpdated(NpImpsSvcPresenceData **curPresence,
				    NpImpsSvcPresenceData **prevPresence)
{
	NpImpsSvcPresenceData *curRef = *curPresence;
	NpImpsSvcPresenceData *prevRef = *prevPresence;
	if (curRef != NULL)
		curRef->AddRef();
	if (prevRef != NULL)
		prevRef->AddRef();

	DispatchMyPresence(2, curRef, prevRef);

	if (prevRef != NULL)
		prevRef->Release();
	if (curRef != NULL)
		curRef->Release();
}

/* 0x812FAD6A..0x812FAE05: lock helpers */
int NpImpsSvc::LockServiceState() { m_sessionState.Lock(); return 0; }
int NpImpsSvc::UnlockServiceState() { m_sessionState.Unlock(); return 0; }
int NpImpsSvc::LockOnlineStatus() { m_onlineStatus.Lock(); return 0; }
int NpImpsSvc::UnlockOnlineStatus() { m_onlineStatus.Unlock(); return 0; }
int NpImpsSvc::LockMyPresence() { m_myPresence.Lock(); return 0; }
int NpImpsSvc::UnlockMyPresence() { m_myPresence.Unlock(); return 0; }
int NpImpsSvc::LockFriendStore() { m_cacheCore->GetFriendStore()->Lock(); return 0; }
int NpImpsSvc::UnlockFriendStore() { m_cacheCore->GetFriendStore()->Unlock(); return 0; }
int NpImpsSvc::LockBlockStore() { m_cacheCore->GetBlockStore()->Lock(); return 0; }
int NpImpsSvc::UnlockBlockStore() { m_cacheCore->GetBlockStore()->Unlock(); return 0; }

/* 0x812FAE06..0x812FAF8B: local presence delegations */
int NpImpsSvc::SetActiveProcess(SceUID processId, int appId)
{
	m_myPresence.SetActiveProcess(processId, appId);
	return 0;
}

int NpImpsSvc::SetGameTitle(int appId)
{
	return m_myPresence.SetGameTitle(appId);
}

int NpImpsSvc::ClearGameTitle()
{
	return m_myPresence.ClearGameTitle();
}

int NpImpsSvc::SetCommId(SceUID processId, const SceNpCommunicationId *commId)
{
	if (commId == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	return m_myPresence.SetCommId(processId, commId);
}

int NpImpsSvc::ClearCommId()
{
	return m_myPresence.ClearCommId();
}

int NpImpsSvc::HasCommId(bool *outHasCommId)
{
	if (outHasCommId == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	*outHasCommId = m_myPresence.HasCommId();
	return 0;
}

int NpImpsSvc::PublishMyPresence()
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	return m_myPresence.PublishCurrentPresence();
}

int NpImpsSvc::RepublishMyPresenceWithoutCommId()
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	return m_myPresence.RepublishWithoutCommId();
}

int NpImpsSvc::SetGamePresence(SceUID processId, const NpImpsGamePresenceInput *input)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (input == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	return m_myPresence.SetGamePresence(processId, input);
}

int NpImpsSvc::ClearGamePresence()
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	return m_myPresence.ClearGamePresence();
}

int NpImpsSvc::SetPartyPresence(const NpImpsPartyPresence *party)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (party == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	return m_myPresence.SetPartyPresence(party);
}

int NpImpsSvc::ClearPartyPresence()
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	return m_myPresence.ClearPartyPresence();
}

int NpImpsSvc::GetAppTitleInfo(int appId, NpImpsAppTitleInfo *outInfo)
{
	return m_myPresence.GetAppTitleInfo(appId, outInfo);
}

int NpImpsSvc::IsBigAppGame(int appId)
{
	return m_myPresence.IsBigAppGame(appId);
}

/* 0x812FAFD4 */
int NpImpsSvc::GetServiceState(int *outState)
{
	if (m_sessionState.IsOnline())
		*outState = NP_IMPS_SVC_STATE_ONLINE;
	else if (m_sessionState.IsSignedIn())
		*outState = NP_IMPS_SVC_STATE_SIGNED_IN;
	else
		*outState = NP_IMPS_SVC_STATE_SIGNED_OUT;
	return 0;
}

/* 0x812FB000 */
int NpImpsSvc::GetMyOnlineStatus(int *outStatus)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	*outStatus = m_onlineStatus.GetStatus();
	return 0;
}

/* 0x812FB02A */
int NpImpsSvc::GetMyPresence(NpImpsSvcPresenceSnapshot *outSnapshot)
{
	return m_myPresence.GetSnapshot(outSnapshot);
}

/* 0x812FB036..0x812FB11B: blocklist queries */
int NpImpsSvc::GetBlockListCount(SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (outCount == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	*outCount = m_cacheCore->GetBlockStore()->GetCount(outLoaded);
	return 0;
}

int NpImpsSvc::GetBlockListEntries(SceUInt32 startIndex, SceNpId *outEntries, SceUInt32 maxCount,
				   SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	m_cacheCore->GetBlockStore()->CopyEntries(startIndex, outEntries, maxCount, outCount, outLoaded);
	return 0;
}

int NpImpsSvc::IsBlocked(const SceNpId *npId, bool *outBlocked, SceUInt8 *outLoaded)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (npId == NULL || outBlocked == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	*outBlocked = m_cacheCore->GetBlockStore()->Contains(npId, outLoaded);
	return 0;
}

/* 0x812FB11C..0x812FB703: friend list & handle queries */
int NpImpsSvc::GetFriendCount(SceUInt32 *outCount)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	*outCount = m_cacheCore->GetFriendStore()->GetCount();
	return 0;
}

int NpImpsSvc::GetFriendEntries(SceUInt32 startIndex, SceNpId *outEntries, SceUInt32 maxCount,
				SceUInt32 *outCount)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	return m_cacheCore->GetFriendStore()->CopyAllNpIds(startIndex, outEntries, maxCount, outCount);
}

int NpImpsSvc::GetFriendEntryNpId(NpImpsCacheFriendPtr *handle, SceNpId *outNpId)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (handle == NULL || outNpId == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	sce_paf_memset(outNpId, 0, sizeof(SceNpId));
	(*handle)->GetNpId(outNpId);
	return 0;
}

int NpImpsSvc::GetFriendEntryRelation(NpImpsCacheFriendPtr *handle, int *outRelation)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (handle == NULL || outRelation == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	*outRelation = (*handle)->GetRelation();
	return 0;
}

int NpImpsSvc::GetFriendEntryDate(NpImpsCacheFriendPtr *handle, paf::rtc::Tick *outTick)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (handle == NULL || outTick == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	(*handle)->GetDate(outTick);
	return 0;
}

int NpImpsSvc::GetFriendEntryRequestDirection(NpImpsCacheFriendPtr *handle, int *outDirection)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (handle == NULL || outDirection == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	*outDirection = (*handle)->GetRequestDirection();
	return 0;
}

int NpImpsSvc::GetFriendEntryIsNew(NpImpsCacheFriendPtr *handle, SceUInt8 *outIsNew)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (handle == NULL || outIsNew == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	*outIsNew = (*handle)->GetIsNew();
	return 0;
}

/* 0x812FB326 */
int NpImpsSvc::SnapshotFriendHandles(NpImpsCacheFriendPtr ***outHandles, SceUInt32 *outCount,
				     void *(*allocFn)(SceSize))
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;

	NpImpsCacheFriendStore *friendStore = m_cacheCore->GetFriendStore();
	friendStore->Lock();

	SceUInt32 count = friendStore->GetCount();
	*outCount = count;

	NpImpsCacheFriendPtr **handleArray =
		reinterpret_cast<NpImpsCacheFriendPtr **>(allocFn(sizeof(NpImpsCacheFriendPtr *) * count));
	*outHandles = handleArray;
	if (handleArray == NULL) {
		friendStore->Unlock();
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	}

	for (SceUInt32 idx = 0; idx < count; ++idx) {
		handleArray[idx] =
			reinterpret_cast<NpImpsCacheFriendPtr *>(allocFn(sizeof(NpImpsCacheFriendPtr)));
		if (handleArray[idx] == NULL) {
			friendStore->Unlock();
			return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		}
		*handleArray[idx] = NULL;
	}

	NpImpsCacheFriendPtr *rawHandles = reinterpret_cast<NpImpsCacheFriendPtr *>(
		npImpsSvcMalloc(sizeof(NpImpsCacheFriendPtr) * count));
	if (rawHandles == NULL) {
		friendStore->Unlock();
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	}

	for (SceUInt32 idx = 0; idx < count; ++idx)
		rawHandles[idx] = NULL;

	int ret = friendStore->CopyHandles(rawHandles, count);
	if (ret >= 0) {
		for (SceUInt32 idx = 0; idx < count; ++idx) {
			*handleArray[idx] = rawHandles[idx];
			if (rawHandles[idx] != NULL)
				rawHandles[idx]->AddRef();
		}
		for (SceUInt32 idx = 0; idx < count; ++idx) {
			if (rawHandles[idx] != NULL)
				rawHandles[idx]->Release();
		}
		npImpsSvcFree(rawHandles);
		friendStore->Unlock();
	}
	return ret;
}

/* 0x812FB4CE */
int NpImpsSvc::ReleaseFriendHandles(NpImpsCacheFriendPtr **handles, SceUInt32 count,
				    void (*freeFn)(void *))
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (handles == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;

	for (SceUInt32 idx = 0; idx < count; ++idx) {
		if (*handles[idx] != NULL)
			(*handles[idx])->Release();
		freeFn(handles[idx]);
	}
	freeFn(handles);
	return 0;
}

int NpImpsSvc::GetAcceptedFriendCount(SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (outCount == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	*outCount = m_cacheCore->GetFriendStore()->GetAcceptedCount(outLoaded);
	return 0;
}

int NpImpsSvc::GetAcceptedFriendEntries(SceUInt32 startIndex, SceNpId *outEntries, SceUInt32 maxCount,
					SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (outEntries == NULL || outCount == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	m_cacheCore->GetFriendStore()->CopyAcceptedNpIds(startIndex, outEntries, maxCount,
							 outCount, outLoaded);
	return 0;
}

int NpImpsSvc::GetPendingFriendCount(SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (outCount == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	*outCount = m_cacheCore->GetFriendStore()->GetPendingCount(outLoaded);
	return 0;
}

int NpImpsSvc::GetPendingFriendEntries(SceUInt32 startIndex, SceNpId *outEntries, SceUInt32 maxCount,
				       SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (outEntries == NULL || outCount == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	m_cacheCore->GetFriendStore()->CopyPendingNpIds(startIndex, outEntries, maxCount,
							outCount, outLoaded);
	return 0;
}

int NpImpsSvc::GetRequestedFriendCount(SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (outCount == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	*outCount = m_cacheCore->GetFriendStore()->GetRequestedCount(outLoaded);
	return 0;
}

int NpImpsSvc::GetRequestedFriendEntries(SceUInt32 startIndex, SceNpId *outEntries, SceUInt32 maxCount,
					 SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	if (outEntries == NULL || outCount == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	m_cacheCore->GetFriendStore()->CopyRequestedNpIds(startIndex, outEntries, maxCount,
							  outCount, outLoaded);
	return 0;
}

/* 0x812FB704 */
int NpImpsSvc::GetFriendPresenceByNpId(const SceNpId *npId, NpImpsSvcPresenceSnapshot *outSnapshot)
{
	if (!m_sessionState.IsOnline())
		return NP_IMPS_SVC_ERROR_OFFLINE;

	NpImpsCacheFriendStore *friendStore = m_cacheCore->GetFriendStore();
	friendStore->Lock();

	int ret = friendStore->GetOnlineStatus(npId, &outSnapshot->onlineStatus);
	if (ret >= 0)
		ret = friendStore->GetTitlePlatform(npId, &outSnapshot->titlePlatform);
	if (ret >= 0)
		ret = friendStore->GetTitleName(npId, outSnapshot->titleName);
	if (ret >= 0)
		ret = friendStore->GetTitleId(npId, &outSnapshot->titleId);
	if (ret >= 0)
		ret = friendStore->GetCommId(npId, &outSnapshot->commId);
	if (ret >= 0)
		ret = friendStore->GetContentId(npId, outSnapshot->contentId);
	if (ret >= 0) {
		outSnapshot->gamePresence.size = 0;
		ret = friendStore->GetGamePresenceType(npId, &outSnapshot->gamePresence.presenceType);
	}
	if (ret >= 0)
		ret = friendStore->GetGameStatus(npId, outSnapshot->gamePresence.status);
	if (ret >= 0)
		ret = friendStore->GetGameData(npId, outSnapshot->gamePresence.gameData);
	if (ret >= 0)
		ret = friendStore->GetGameDataSize(npId, &outSnapshot->gamePresence.gameDataSize);
	if (ret >= 0) {
		outSnapshot->partyPresence.size = sizeof(NpImpsPartyPresence);
		ret = friendStore->GetPartyPresenceType(npId, &outSnapshot->partyPresence.presenceType);
	}
	if (ret >= 0)
		ret = friendStore->GetPartyTitle(npId, outSnapshot->partyPresence.title);
	if (ret >= 0)
		ret = friendStore->GetPartyStatus(npId, outSnapshot->partyPresence.status);
	if (ret >= 0)
		ret = friendStore->GetPartyData(npId, outSnapshot->partyPresence.partyData);
	if (ret >= 0)
		ret = friendStore->GetPartyDataSize(npId, &outSnapshot->partyPresence.partyDataSize);

	friendStore->Unlock();
	return ret;
}

/* 0x812FB8E8 */
int NpImpsSvc::GetFriendPresenceByHandle(NpImpsCacheFriendPtr *handle,
					 NpImpsSvcPresenceSnapshot *outSnapshot, int platform)
{
	if (!m_sessionState.IsOnline())
		return NP_IMPS_SVC_ERROR_OFFLINE;

	NpImpsCacheFriendStore *friendStore = m_cacheCore->GetFriendStore();
	friendStore->Lock();

	int ret = NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	if (handle != NULL) {
		NpImpsCacheFriendEntry *entry = *handle;
		char statusBuf[204];

		outSnapshot->onlineStatus = entry->GetOnlineStatus(platform);
		outSnapshot->platform = entry->GetPlatform(platform);
		outSnapshot->titlePlatform = entry->GetTitlePlatform(platform);
		entry->GetTitleName(outSnapshot->titleName, platform);
		entry->GetTitleId(&outSnapshot->titleId, platform);
		entry->GetCommId(&outSnapshot->commId, platform);

		outSnapshot->gamePresence.size = 0;
		outSnapshot->gamePresence.presenceType = entry->GetGamePresenceType(platform);
		entry->GetGameStatus(statusBuf, platform);
		sce_paf_strncpy(outSnapshot->gamePresence.status, statusBuf,
				sizeof(outSnapshot->gamePresence.status));
		outSnapshot->gamePresence.status[sizeof(outSnapshot->gamePresence.status) - 1] = '\0';

		SceSize gameDataLen = entry->GetGameDataSize(platform);
		if (gameDataLen > sizeof(outSnapshot->gamePresence.gameData))
			gameDataLen = sizeof(outSnapshot->gamePresence.gameData);
		outSnapshot->gamePresence.gameDataSize = gameDataLen;
		ret = entry->CopyGameData(outSnapshot->gamePresence.gameData,
					  sizeof(outSnapshot->gamePresence.gameData), platform);
		if (ret >= 0) {
			entry->GetJoinableCommId(&outSnapshot->joinableCommId, platform);
			outSnapshot->partyPresence.size = sizeof(NpImpsPartyPresence);
			outSnapshot->partyPresence.presenceType = entry->GetPartyPresenceType(platform);
			entry->GetPartyTitle(outSnapshot->partyPresence.title, platform);
			entry->GetPartyStatus(outSnapshot->partyPresence.status, platform);
			outSnapshot->partyPresence.partyDataSize = entry->GetPartyDataSize(platform);
			ret = entry->CopyPartyData(outSnapshot->partyPresence.partyData,
						   sizeof(outSnapshot->partyPresence.partyData), platform);
		}
	}

	friendStore->Unlock();
	return ret;
}

/* 0x812FBBA0 */
int NpImpsSvc::GetFriendContextByNpId(const SceNpId *npId, int *outContext)
{
	if (!m_sessionState.IsOnline())
		return NP_IMPS_SVC_ERROR_OFFLINE;
	return m_cacheCore->GetFriendStore()->GetFriendContext(npId, outContext);
}

/* 0x812FBBD2 */
int NpImpsSvc::GetFriendGamePresence(const SceNpId *npId, NpImpsGamePresenceWithTitle *outPresence)
{
	NpImpsSvcPresenceSnapshot snapshot;
	sce_paf_memset(&snapshot, 0, sizeof(snapshot));

	int partyErr = 0;
	int ret = npPartyGetMemberGamePresence(npId, outPresence);
	if (ret >= 0) {
		int ptype = outPresence->body.presenceType;
		if (ptype == 2 || ptype == 3 || ptype == 4)
			return ret;
	} else {
		partyErr = ret;
	}

	ret = npImpsSvcGetFriendPresenceByNpId(npId, &snapshot);
	if (ret < 0)
		return ret;

	int gameType = snapshot.gamePresence.presenceType;
	if (gameType == 2 || gameType == 4) {
		outPresence->size = sizeof(NpImpsGamePresenceWithTitle);
		sce_paf_strncpy(outPresence->title, snapshot.titleName, sizeof(outPresence->title));
		outPresence->title[sizeof(outPresence->title) - 1] = '\0';
		outPresence->body = snapshot.gamePresence;
		return ret;
	}
	if (gameType == 3) {
		if (partyErr == NP_PARTY_ERROR_NOT_IN_PARTY)
			return NP_IMPS_SVC_ERROR_NO_PRESENCE;
		return ret;
	}
	return NP_IMPS_SVC_ERROR_NO_PRESENCE;
}

/* 0x812FBCC0..0x812FC13F: NpImpsSvcAsyncInterface methods */
int NpImpsSvc::CreateContext(SceUInt32 *outContextId)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	return m_ctxMgr.CreateContextId(outContextId);
}

int NpImpsSvc::DestroyContext(SceUInt32 contextId)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	return m_ctxMgr.DestroyContextId(contextId);
}

int NpImpsSvc::AbortContext(SceUInt32 contextId)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	return m_ctxMgr.AbortContext(contextId);
}

int NpImpsSvc::WaitContext(SceUInt32 contextId)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;
	return m_ctxMgr.WaitContext(contextId);
}

int NpImpsSvc::RequestFriend(SceUInt32 contextId, const SceNpId *npId,
			     const char *comment, NpImpsSvcRosterCallback callback)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;

	NpImpsSvcContext *ctx = NULL;
	int ret = m_ctxFactory.CreateRequestFriend(&ctx, contextId, npId, comment, callback);
	if (ret >= 0) {
		ret = m_ctxMgr.EnqueueContext(ctx);
		if (ctx != NULL)
			ctx->Release();
	}
	return ret;
}

int NpImpsSvc::AcceptFriend(SceUInt32 contextId, const SceNpId *npId,
			    NpImpsSvcRosterCallback callback)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;

	NpImpsSvcContext *ctx = NULL;
	int ret = m_ctxFactory.CreateAcceptFriend(&ctx, contextId, npId, callback);
	if (ret >= 0) {
		ret = m_ctxMgr.EnqueueContext(ctx);
		if (ctx != NULL)
			ctx->Release();
	}
	return ret;
}

int NpImpsSvc::DeleteFriend(SceUInt32 contextId, const SceNpId *npId,
			    NpImpsSvcRosterCallback callback)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;

	NpImpsSvcContext *ctx = NULL;
	int ret = m_ctxFactory.CreateDeleteFriend(&ctx, contextId, npId, callback);
	if (ret >= 0) {
		ret = m_ctxMgr.EnqueueContext(ctx);
		if (ctx != NULL)
			ctx->Release();
	}
	return ret;
}

int NpImpsSvc::AddBlock(SceUInt32 contextId, const SceNpId *npId,
			NpImpsSvcRosterCallback callback)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;

	NpImpsSvcContext *ctx = NULL;
	int ret = m_ctxFactory.CreateAddBlock(&ctx, contextId, npId, callback);
	if (ret >= 0) {
		ret = m_ctxMgr.EnqueueContext(ctx);
		if (ctx != NULL)
			ctx->Release();
	}
	return ret;
}

int NpImpsSvc::DeleteBlock(SceUInt32 contextId, const SceNpId *npId,
			   NpImpsSvcRosterCallback callback)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;

	NpImpsSvcContext *ctx = NULL;
	int ret = m_ctxFactory.CreateDeleteBlock(&ctx, contextId, npId, callback);
	if (ret >= 0) {
		ret = m_ctxMgr.EnqueueContext(ctx);
		if (ctx != NULL)
			ctx->Release();
	}
	return ret;
}

int NpImpsSvc::ForceRosterSync(int syncFlags)
{
	if (!m_sessionState.IsSignedIn())
		return NP_IMPS_SVC_ERROR_SIGNED_OUT;

	NpImpsCacheCore *core = npImpsCacheGetCore();
	if (core == NULL)
		return NP_IMPS_SVC_ERROR_INTERNAL;
	NpImpsCacheRosterSync *rosterSync = core->GetRosterSync();
	if (rosterSync == NULL)
		return NP_IMPS_SVC_ERROR_INTERNAL;
	return rosterSync->ForceFullSync(syncFlags);
}
