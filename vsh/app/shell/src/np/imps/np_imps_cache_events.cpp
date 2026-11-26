/*
 * np_imps_cache_events.cpp: SceNpImpsCache worker event classes
 * (0x81301AA4..0x81301B35, 0x813043B4..0x8130640B).
 */
#include <stddef.h>
#include <std/new>
#include <std/string.h>
#include <std/stdlib.h>
#include <scetypes.h>
#include <rtc.h>
#include "np_imps_cache.h"

namespace {

/* Helper to dispatch friend-sync diff sets to store sink in Sony's exact order */
static int notifyFriendSyncDiffs(NpImpsCacheStoreSink *sink,
				 bool firstLoad,
				 const NpImpsCacheNpIdSet *removedAccepted,
				 const NpImpsCacheNpIdSet *removedPending,
				 const NpImpsCacheNpIdSet *removedRequested,
				 const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *added,
				 const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *updated)
{
	if (sink == NULL)
		return 0;

	int ret = 0;
	if (firstLoad)
		ret = sink->NotifyFriendListLoaded(added);
	else if (removedAccepted->Size() > 0)
		ret = sink->NotifyFriendsRemoved(removedAccepted);
	else if (removedPending->Size() > 0)
		ret = sink->NotifyFriendRequestsRemoved(removedPending);
	else if (removedRequested->Size() > 0)
		ret = sink->NotifyFriendContextRemoved(removedRequested);
	else if (updated->Size() > 0)
		ret = sink->NotifyFriendListUpdated(updated);
	else if (added->Size() > 0)
		ret = sink->NotifyFriendListAdded(added);
	return ret;
}

static int notifyBlockSyncDiffs(NpImpsCacheStoreSink *sink,
				bool firstLoad,
				const NpImpsCacheNpIdSet *added,
				const NpImpsCacheNpIdSet *removed)
{
	if (sink == NULL)
		return 0;

	int ret = 0;
	if (firstLoad)
		ret = sink->NotifyBlockListLoaded(added);
	if (ret >= 0 && added->Size() > 0)
		ret = sink->NotifyBlockListAdded(added);
	if (ret >= 0 && removed->Size() > 0)
		ret = sink->NotifyBlockListRemoved(removed);
	return ret;
}

/* --------------------------------------------------------------------------
 * 1. ForceSyncEvent (0x81301AA4, 0x81301B36)
 * -------------------------------------------------------------------------- */
class ForceSyncEvent : public NpImpsCacheEvent {
public:
	ForceSyncEvent(int syncFlags, NpImpsCacheFriendStoreImpl *friendStore,
		       NpImpsCacheBlockStoreImpl *blockStore, NpImpsCacheStoreSink *sink)
		: m_syncFlags(syncFlags),
		  m_friendStore(friendStore),
		  m_blockStore(blockStore),
		  m_sink(sink)
	{
	}

	virtual int Execute()
	{
		NpImpsCacheNpIdSet removedAccepted;
		NpImpsCacheNpIdSet removedPending;
		NpImpsCacheNpIdSet removedRequested;
		NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> addedFriends;
		NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> updatedFriends;
		NpImpsCacheNpIdSet addedBlocks;
		NpImpsCacheNpIdSet removedBlocks;
		NpImpsCacheRefVector<NpImpsCacheRosterData> fetchedFriends;
		NpImpsCacheNpIdSet fetchedBlocks;

		bool friendFirstLoad = false;
		bool blockFirstLoad = false;
		int firstError = 0;

		if ((m_syncFlags & 1) != 0) {
			int ret = m_friendStore->FetchFromRoster(&fetchedFriends);
			if (ret < 0) {
				firstError = ret;
				NpImpsCacheCoreImpl *core = NpImpsCacheCoreImpl::GetInstance();
				if (core != NULL)
					core->MarkFriendSyncRetry();
			}
		}

		if ((m_syncFlags & 2) != 0) {
			int ret = m_blockStore->FetchFromRoster(&fetchedBlocks);
			if (ret < 0) {
				if (firstError == 0)
					firstError = ret;
				NpImpsCacheCoreImpl *core = NpImpsCacheCoreImpl::GetInstance();
				if (core != NULL)
					core->MarkBlockSyncRetry();
			}
		}

		m_friendStore->Lock();
		m_blockStore->Lock();

		if (fetchedFriends.Size() > 0) {
			int ret = m_friendStore->ApplyFullList(
				&fetchedFriends, &removedAccepted, &removedPending,
				&removedRequested, &addedFriends, &updatedFriends,
				&friendFirstLoad);
			if (ret < 0 && firstError == 0)
				firstError = ret;
		}

		if (fetchedBlocks.Size() > 0) {
			int ret = m_blockStore->ApplyFullList(
				fetchedBlocks.Data(), fetchedBlocks.Size(),
				&addedBlocks, &removedBlocks, &blockFirstLoad);
			if (ret < 0 && firstError == 0)
				firstError = ret;
		}

		int notifyRet = notifyFriendSyncDiffs(m_sink, friendFirstLoad,
						      &removedAccepted, &removedPending,
						      &removedRequested, &addedFriends,
						      &updatedFriends);
		if (notifyRet < 0 && firstError == 0)
			firstError = notifyRet;

		notifyRet = notifyBlockSyncDiffs(m_sink, blockFirstLoad,
						 &addedBlocks, &removedBlocks);
		if (notifyRet < 0 && firstError == 0)
			firstError = notifyRet;

		m_blockStore->Unlock();
		m_friendStore->Unlock();
		return firstError;
	}

private:
	int m_syncFlags;
	NpImpsCacheFriendStoreImpl *m_friendStore;
	NpImpsCacheBlockStoreImpl *m_blockStore;
	NpImpsCacheStoreSink *m_sink;
};

/* --------------------------------------------------------------------------
 * 2. RefreshBlockListEvent (0x813043B4, 0x81304534)
 * -------------------------------------------------------------------------- */
class RefreshBlockListEvent : public NpImpsCacheEvent {
public:
	RefreshBlockListEvent(NpImpsCacheBlockStoreImpl *blockStore)
		: m_blockStore(blockStore)
	{
	}

	virtual int Execute()
	{
		NpImpsCacheNpIdSet addedBlocks;
		NpImpsCacheNpIdSet removedBlocks;
		bool firstLoad = false;

		int ret = m_blockStore->FetchAndApply(&addedBlocks, &removedBlocks, &firstLoad);
		if (ret >= 0) {
			NpImpsCacheCoreImpl *core = NpImpsCacheCoreImpl::GetInstance();
			notifyBlockSyncDiffs(core, firstLoad, &addedBlocks, &removedBlocks);
			m_blockStore->Unlock();
		}
		return ret;
	}

private:
	NpImpsCacheBlockStoreImpl *m_blockStore;
};

/* --------------------------------------------------------------------------
 * 3. BlockAddEvent (0x81304594, 0x8130463E)
 * -------------------------------------------------------------------------- */
class BlockAddEvent : public NpImpsCacheEvent {
public:
	BlockAddEvent(NpImpsCacheFriendStoreImpl *friendStore,
		      NpImpsCacheBlockStoreImpl *blockStore)
		: m_friendStore(friendStore),
		  m_blockStore(blockStore)
	{
	}

	int AddTarget(const SceNpId *npId) { return m_targets.Insert(npId); }

	virtual int Execute()
	{
		int ret = m_blockStore->AddEntries(&m_targets);
		if (ret >= 0)
			ret = m_friendStore->RemoveList(&m_targets);
		return ret;
	}

private:
	NpImpsCacheFriendStoreImpl *m_friendStore;
	NpImpsCacheBlockStoreImpl *m_blockStore;
	NpImpsCacheNpIdSet m_targets;
};

/* --------------------------------------------------------------------------
 * 4. BlockRemoveEvent (0x8130471C, 0x813047C2)
 * -------------------------------------------------------------------------- */
class BlockRemoveEvent : public NpImpsCacheEvent {
public:
	BlockRemoveEvent(NpImpsCacheBlockStoreImpl *blockStore)
		: m_blockStore(blockStore)
	{
	}

	int AddTarget(const SceNpId *npId) { return m_targets.Insert(npId); }

	virtual int Execute()
	{
		return m_blockStore->RemoveEntries(&m_targets);
	}

private:
	NpImpsCacheBlockStoreImpl *m_blockStore;
	NpImpsCacheNpIdSet m_targets;
};

/* --------------------------------------------------------------------------
 * 5. FriendAddOrUpdateEvent (0x813049A8, 0x81304B10)
 * -------------------------------------------------------------------------- */
class FriendAddOrUpdateEvent : public NpImpsCacheEvent {
public:
	FriendAddOrUpdateEvent(NpImpsCacheFriendStoreImpl *friendStore)
		: m_friendStore(friendStore)
	{
	}

	int AddItem(NpImpsCacheRosterData *item) { return m_items.PushBack(item); }

	virtual int Execute()
	{
		return m_friendStore->AddOrUpdateList(&m_items);
	}

private:
	NpImpsCacheFriendStoreImpl *m_friendStore;
	NpImpsCacheRefVector<NpImpsCacheRosterData> m_items;
};

/* --------------------------------------------------------------------------
 * 6. FriendRemoveEvent (0x81304A84, 0x81304C8E)
 * -------------------------------------------------------------------------- */
class FriendRemoveEvent : public NpImpsCacheEvent {
public:
	FriendRemoveEvent(NpImpsCacheFriendStoreImpl *friendStore)
		: m_friendStore(friendStore)
	{
	}

	int AddTarget(const SceNpId *npId) { return m_targets.Insert(npId); }

	virtual int Execute()
	{
		return m_friendStore->RemoveList(&m_targets);
	}

private:
	NpImpsCacheFriendStoreImpl *m_friendStore;
	NpImpsCacheNpIdSet m_targets;
};

/* --------------------------------------------------------------------------
 * 7. FriendPresenceEvent (0x81304E8E, 0x81304F54)
 * -------------------------------------------------------------------------- */
class FriendPresenceEvent : public NpImpsCacheEvent {
public:
	FriendPresenceEvent(NpImpsCacheFriendStoreImpl *friendStore,
			    const SceNpId *npId, NpImpsSvcPresenceData *presence, int platform)
		: m_friendStore(friendStore),
		  m_presence(presence),
		  m_platform(platform)
	{
		m_npId = *npId;
		if (m_presence != NULL)
			m_presence->AddRef();
	}

	virtual ~FriendPresenceEvent()
	{
		if (m_presence != NULL)
			m_presence->Release();
	}

	virtual int Execute()
	{
		return m_friendStore->UpdatePresence(&m_npId, &m_presence, m_platform);
	}

private:
	NpImpsCacheFriendStoreImpl *m_friendStore;
	SceNpId m_npId;
	NpImpsSvcPresenceData *m_presence;
	int m_platform;
};

/* --------------------------------------------------------------------------
 * 8. InGameDataMessageEvent (0x81304FA0, 0x813050C8)
 * -------------------------------------------------------------------------- */
class InGameDataMessageEvent : public NpImpsCacheEvent {
public:
	InGameDataMessageEvent(NpImpsSvcInGameDataMessage *msg, NpImpsCacheStoreSink *sink)
		: m_message(msg),
		  m_sink(sink)
	{
		if (m_message != NULL)
			m_message->AddRef();
	}

	virtual ~InGameDataMessageEvent()
	{
		if (m_message != NULL)
			m_message->Release();
	}

	virtual int Execute()
	{
		if (m_sink == NULL)
			return 0;
		return m_sink->NotifyInGameDataMessage(&m_message);
	}

private:
	NpImpsSvcInGameDataMessage *m_message;
	NpImpsCacheStoreSink *m_sink;
};

/* --------------------------------------------------------------------------
 * 9. RetryFailedSyncEvent (0x81305110, 0x81305394)
 * -------------------------------------------------------------------------- */
class RetryFailedSyncEvent : public NpImpsCacheEvent {
public:
	RetryFailedSyncEvent(NpImpsCacheCoreImpl *core,
			     NpImpsCacheFriendStoreImpl *friendStore,
			     NpImpsCacheBlockStoreImpl *blockStore,
			     NpImpsCacheStoreSink *sink)
		: m_core(core),
		  m_friendStore(friendStore),
		  m_blockStore(blockStore),
		  m_sink(sink)
	{
	}

	virtual int Execute()
	{
		bool needFriends = m_core->NeedsFriendSyncRetry();
		bool needBlocks = m_core->NeedsBlockSyncRetry();
		if (!needFriends && !needBlocks)
			return 0;

		NpImpsCacheNpIdSet removedAccepted;
		NpImpsCacheNpIdSet removedPending;
		NpImpsCacheNpIdSet removedRequested;
		NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> addedFriends;
		NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> updatedFriends;
		NpImpsCacheNpIdSet addedBlocks;
		NpImpsCacheNpIdSet removedBlocks;
		NpImpsCacheRefVector<NpImpsCacheRosterData> fetchedFriends;
		NpImpsCacheNpIdSet fetchedBlocks;

		bool friendFirstLoad = false;
		bool blockFirstLoad = false;
		bool friendsOk = false;
		bool blocksOk = false;
		int firstError = 0;

		if (needFriends) {
			int ret = m_friendStore->FetchFromRoster(&fetchedFriends);
			if (ret >= 0) {
				friendsOk = true;
				m_core->ClearFriendSyncRetry();
			} else {
				firstError = ret;
			}
		}

		if (needBlocks) {
			int ret = m_blockStore->FetchFromRoster(&fetchedBlocks);
			if (ret >= 0) {
				blocksOk = true;
				m_core->ClearBlockSyncRetry();
			} else if (firstError == 0) {
				firstError = ret;
			}
		}

		m_friendStore->Lock();
		m_blockStore->Lock();

		if (friendsOk) {
			int ret = m_friendStore->ApplyFullList(
				&fetchedFriends, &removedAccepted, &removedPending,
				&removedRequested, &addedFriends, &updatedFriends,
				&friendFirstLoad);
			if (ret < 0) {
				if (firstError == 0)
					firstError = ret;
				m_core->MarkFriendSyncRetry();
			}
		}

		if (blocksOk) {
			int ret = m_blockStore->ApplyFullList(
				fetchedBlocks.Data(), fetchedBlocks.Size(),
				&addedBlocks, &removedBlocks, &blockFirstLoad);
			if (ret < 0) {
				if (firstError == 0)
					firstError = ret;
				m_core->MarkBlockSyncRetry();
			}
		}

		int notifyRet = notifyFriendSyncDiffs(m_sink, friendFirstLoad,
						      &removedAccepted, &removedPending,
						      &removedRequested, &addedFriends,
						      &updatedFriends);
		if (notifyRet < 0 && firstError == 0)
			firstError = notifyRet;

		notifyRet = notifyBlockSyncDiffs(m_sink, blockFirstLoad,
						 &addedBlocks, &removedBlocks);
		if (notifyRet < 0 && firstError == 0)
			firstError = notifyRet;

		m_blockStore->Unlock();
		m_friendStore->Unlock();
		return firstError;
	}

private:
	NpImpsCacheCoreImpl *m_core;
	NpImpsCacheFriendStoreImpl *m_friendStore;
	NpImpsCacheBlockStoreImpl *m_blockStore;
	NpImpsCacheStoreSink *m_sink;
};

/* --------------------------------------------------------------------------
 * 10. ImpsCoreNotificationEvent (0x813054D8, 0x8130575E)
 * -------------------------------------------------------------------------- */
class ImpsCoreNotificationEvent : public NpImpsCacheEvent {
public:
	ImpsCoreNotificationEvent(int notifyCode,
				  NpImpsCacheFriendStoreImpl *friendStore,
				  NpImpsCacheBlockStoreImpl *blockStore,
				  NpImpsCacheStoreSink *sink)
		: m_notifyCode(notifyCode),
		  m_friendStore(friendStore),
		  m_blockStore(blockStore),
		  m_sink(sink)
	{
	}

	virtual int Execute()
	{
		if (m_sink != NULL)
			return m_sink->NotifyNotificationEvent(m_notifyCode);
		return 0;
	}

private:
	int m_notifyCode;
	NpImpsCacheFriendStoreImpl *m_friendStore;
	NpImpsCacheBlockStoreImpl *m_blockStore;
	NpImpsCacheStoreSink *m_sink;
};

/* --------------------------------------------------------------------------
 * 11. ImpsCoreStateEvent (0x81305794, 0x81305814, 0x81305926, 0x81305D92)
 * -------------------------------------------------------------------------- */
class ImpsCoreStateEvent : public NpImpsCacheEvent {
public:
	ImpsCoreStateEvent(NpImpsCacheFriendStoreImpl *friendStore,
			   NpImpsCacheBlockStoreImpl *blockStore,
			   int coreState, int errorCode,
			   NpImpsCacheStoreSink *sink)
		: m_friendStore(friendStore),
		  m_blockStore(blockStore),
		  m_coreState(coreState),
		  m_errorCode(errorCode),
		  m_sink(sink)
	{
	}

	virtual int Execute()
	{
		NpImpsCacheCoreImpl *core = NpImpsCacheCoreImpl::GetInstance();
		if (m_coreState == NP_IMPS_CORE_STATE_ONLINE) {
			/* 0x81305926: full sync of friends + blocks on online transition */
			NpImpsCacheNpIdSet removedAccepted;
			NpImpsCacheNpIdSet removedPending;
			NpImpsCacheNpIdSet removedRequested;
			NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> addedFriends;
			NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> updatedFriends;
			NpImpsCacheNpIdSet addedBlocks;
			NpImpsCacheNpIdSet removedBlocks;
			NpImpsCacheRefVector<NpImpsCacheRosterData> fetchedFriends;
			NpImpsCacheNpIdSet fetchedBlocks;

			bool friendFirstLoad = false;
			bool blockFirstLoad = false;
			int firstError = 0;

			int ret = m_friendStore->FetchFromRoster(&fetchedFriends);
			if (ret < 0) {
				firstError = ret;
				if (core != NULL)
					core->MarkFriendSyncRetry();
			}

			ret = m_blockStore->FetchFromRoster(&fetchedBlocks);
			if (ret < 0) {
				if (firstError == 0)
					firstError = ret;
				if (core != NULL)
					core->MarkBlockSyncRetry();
			}

			m_friendStore->Lock();
			m_blockStore->Lock();

			if (fetchedFriends.Size() > 0) {
				ret = m_friendStore->ApplyFullList(
					&fetchedFriends, &removedAccepted, &removedPending,
					&removedRequested, &addedFriends, &updatedFriends,
					&friendFirstLoad);
				if (ret < 0 && firstError == 0)
					firstError = ret;
			}

			if (fetchedBlocks.Size() > 0) {
				ret = m_blockStore->ApplyFullList(
					fetchedBlocks.Data(), fetchedBlocks.Size(),
					&addedBlocks, &removedBlocks, &blockFirstLoad);
				if (ret < 0 && firstError == 0)
					firstError = ret;
			}

			ret = notifyFriendSyncDiffs(m_sink, friendFirstLoad,
						    &removedAccepted, &removedPending,
						    &removedRequested, &addedFriends,
						    &updatedFriends);
			if (ret < 0 && firstError == 0)
				firstError = ret;

			ret = notifyBlockSyncDiffs(m_sink, blockFirstLoad,
						   &addedBlocks, &removedBlocks);
			if (ret < 0 && firstError == 0)
				firstError = ret;

			if (core != NULL) {
				ret = core->ForwardImpsCoreState(m_coreState, m_errorCode);
				if (ret < 0 && firstError == 0)
					firstError = ret;
			}

			m_blockStore->Unlock();
			m_friendStore->Unlock();
			return firstError;
		}

		if (m_coreState == NP_IMPS_CORE_STATE_BLOCKLIST_READY) {
			/* 0x81305814: refresh blocklist then forward state */
			NpImpsCacheNpIdSet addedBlocks;
			NpImpsCacheNpIdSet removedBlocks;
			bool firstLoad = false;

			int ret = m_blockStore->FetchAndApply(&addedBlocks, &removedBlocks,
							      &firstLoad);
			if (ret >= 0) {
				notifyBlockSyncDiffs(m_sink, firstLoad,
						     &addedBlocks, &removedBlocks);
				if (core != NULL)
					ret = core->ForwardImpsCoreState(m_coreState, m_errorCode);
				m_blockStore->Unlock();
			} else if (core != NULL) {
				core->ForwardImpsCoreState(m_coreState, m_errorCode);
			}
			return ret;
		}

		if (m_coreState >= NP_IMPS_CORE_STATE_OFFLINE_BEGIN &&
		    m_coreState <= NP_IMPS_CORE_STATE_OFFLINE_END) {
			/* 0x81305D92: reset all friend presences offline */
			m_friendStore->Lock();
			m_blockStore->Lock();
			m_friendStore->ResetAllPresenceOffline();
			int ret = 0;
			if (core != NULL)
				ret = core->ForwardImpsCoreState(m_coreState, m_errorCode);
			m_blockStore->Unlock();
			m_friendStore->Unlock();
			return ret;
		}

		return 0;
	}

private:
	NpImpsCacheFriendStoreImpl *m_friendStore;
	NpImpsCacheBlockStoreImpl *m_blockStore;
	int m_coreState;
	int m_errorCode;
	NpImpsCacheStoreSink *m_sink;
};

/* --------------------------------------------------------------------------
 * 12. SignInSyncEvent (0x81305E28, 0x81305E9E)
 * -------------------------------------------------------------------------- */
class SignInSyncEvent : public NpImpsCacheEvent {
public:
	SignInSyncEvent(NpImpsCacheFriendStoreImpl *friendStore,
			NpImpsCacheBlockStoreImpl *blockStore,
			NpImpsCacheStoreSink *sink)
		: m_friendStore(friendStore),
		  m_blockStore(blockStore),
		  m_sink(sink)
	{
	}

	virtual int Execute()
	{
		NpImpsCacheCoreImpl *core = NpImpsCacheCoreImpl::GetInstance();
		NpImpsCacheNpIdSet removedAccepted;
		NpImpsCacheNpIdSet removedPending;
		NpImpsCacheNpIdSet removedRequested;
		NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> addedFriends;
		NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> updatedFriends;
		NpImpsCacheNpIdSet addedBlocks;
		NpImpsCacheNpIdSet removedBlocks;
		NpImpsCacheRefVector<NpImpsCacheRosterData> fetchedFriends;
		NpImpsCacheNpIdSet fetchedBlocks;

		bool friendFirstLoad = false;
		bool blockFirstLoad = false;
		bool friendsFetched = false;
		bool blocksFetched = false;
		int firstError = 0;

		int ret = m_friendStore->FetchFromRoster(&fetchedFriends);
		if (ret >= 0) {
			friendsFetched = true;
		} else {
			firstError = ret;
			if (core != NULL)
				core->MarkFriendSyncRetry();
		}

		ret = m_blockStore->FetchFromRoster(&fetchedBlocks);
		if (ret >= 0) {
			blocksFetched = true;
		} else {
			if (firstError == 0)
				firstError = ret;
			if (core != NULL)
				core->MarkBlockSyncRetry();
		}

		m_friendStore->Lock();
		m_blockStore->Lock();

		if (friendsFetched) {
			ret = m_friendStore->ApplyFullList(
				&fetchedFriends, &removedAccepted, &removedPending,
				&removedRequested, &addedFriends, &updatedFriends,
				&friendFirstLoad);
			if (ret < 0) {
				if (firstError == 0)
					firstError = ret;
				if (core != NULL)
					core->MarkFriendSyncRetry();
			}
		}

		if (blocksFetched) {
			ret = m_blockStore->ApplyFullList(
				fetchedBlocks.Data(), fetchedBlocks.Size(),
				&addedBlocks, &removedBlocks, &blockFirstLoad);
			if (ret < 0) {
				if (firstError == 0)
					firstError = ret;
				if (core != NULL)
					core->MarkBlockSyncRetry();
			}
		}

		if (m_sink != NULL) {
			ret = m_sink->NotifySignInComplete();
			if (ret < 0 && firstError == 0)
				firstError = ret;
		}

		ret = notifyFriendSyncDiffs(m_sink, friendFirstLoad,
					    &removedAccepted, &removedPending,
					    &removedRequested, &addedFriends,
					    &updatedFriends);
		if (ret < 0 && firstError == 0)
			firstError = ret;

		ret = notifyBlockSyncDiffs(m_sink, blockFirstLoad,
					   &addedBlocks, &removedBlocks);
		if (ret < 0 && firstError == 0)
			firstError = ret;

		m_blockStore->Unlock();
		m_friendStore->Unlock();
		return firstError;
	}

private:
	NpImpsCacheFriendStoreImpl *m_friendStore;
	NpImpsCacheBlockStoreImpl *m_blockStore;
	NpImpsCacheStoreSink *m_sink;
};

/* --------------------------------------------------------------------------
 * 13. SignOutClearEvent (0x8130636C, 0x813063E2)
 * -------------------------------------------------------------------------- */
class SignOutClearEvent : public NpImpsCacheEvent {
public:
	SignOutClearEvent(NpImpsCacheFriendStoreImpl *friendStore,
			  NpImpsCacheBlockStoreImpl *blockStore,
			  NpImpsCacheStoreSink *sink)
		: m_friendStore(friendStore),
		  m_blockStore(blockStore),
		  m_sink(sink)
	{
	}

	virtual int Execute()
	{
		int ret = m_friendStore->Clear();
		if (ret >= 0) {
			ret = m_blockStore->Clear();
			if (ret >= 0 && m_sink != NULL)
				ret = m_sink->NotifySignOutComplete();
		}
		return (ret >= 0) ? 0 : ret;
	}

private:
	NpImpsCacheFriendStoreImpl *m_friendStore;
	NpImpsCacheBlockStoreImpl *m_blockStore;
	NpImpsCacheStoreSink *m_sink;
};

static int assignEvent(NpImpsCacheEvent **outEvent, NpImpsCacheEvent *created)
{
	if (created == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	created->AddRef();
	if (*outEvent != NULL)
		(*outEvent)->Release();
	*outEvent = created;
	return 0;
}

} /* anonymous namespace */

/* --------------------------------------------------------------------------
 * Event factory functions
 * -------------------------------------------------------------------------- */
int npImpsCacheCreateForceSyncEvent(NpImpsCacheEvent **outEvent, int syncFlags,
				    NpImpsCacheFriendStoreImpl *friendStore,
				    NpImpsCacheBlockStoreImpl *blockStore,
				    NpImpsCacheStoreSink *sink)
{
	void *mem = npImpsCacheMalloc(sizeof(ForceSyncEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	return assignEvent(outEvent,
			   new (mem) ForceSyncEvent(syncFlags, friendStore, blockStore, sink));
}

int npImpsCacheCreateRefreshBlockListEvent(NpImpsCacheEvent **outEvent,
					   NpImpsCacheBlockStoreImpl *blockStore)
{
	void *mem = npImpsCacheMalloc(sizeof(RefreshBlockListEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	return assignEvent(outEvent, new (mem) RefreshBlockListEvent(blockStore));
}

int npImpsCacheCreateBlockAddEvent(NpImpsCacheEvent **outEvent,
				   NpImpsCacheFriendStoreImpl *friendStore,
				   NpImpsCacheBlockStoreImpl *blockStore,
				   SceUInt32 count, const SceNpId *npIds)
{
	void *mem = npImpsCacheMalloc(sizeof(BlockAddEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	BlockAddEvent *event = new (mem) BlockAddEvent(friendStore, blockStore);
	for (SceUInt32 pos = 0; pos < count; ++pos)
		event->AddTarget(&npIds[pos]);
	return assignEvent(outEvent, event);
}

int npImpsCacheCreateBlockRemoveEvent(NpImpsCacheEvent **outEvent,
				      NpImpsCacheBlockStoreImpl *blockStore,
				      SceUInt32 count, const SceNpId *npIds)
{
	void *mem = npImpsCacheMalloc(sizeof(BlockRemoveEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	BlockRemoveEvent *event = new (mem) BlockRemoveEvent(blockStore);
	for (SceUInt32 pos = 0; pos < count; ++pos)
		event->AddTarget(&npIds[pos]);
	return assignEvent(outEvent, event);
}

int npImpsCacheCreateFriendAddOrUpdateEvent(NpImpsCacheEvent **outEvent,
					    NpImpsCacheFriendStoreImpl *friendStore,
					    const SceNpId *npId, int relation)
{
	NpImpsCacheRosterData *rosterData = NULL;
	int ret = NpImpsCacheRosterData::CreateWithRelation(&rosterData, npId, relation);
	if (ret < 0)
		return ret;

	void *mem = npImpsCacheMalloc(sizeof(FriendAddOrUpdateEvent));
	if (mem == NULL) {
		rosterData->Release();
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	}
	FriendAddOrUpdateEvent *event = new (mem) FriendAddOrUpdateEvent(friendStore);
	event->AddItem(rosterData);
	rosterData->Release();
	return assignEvent(outEvent, event);
}

int npImpsCacheCreateFriendRosterBatchAddEvent(NpImpsCacheEvent **outEvent,
					       NpImpsCacheFriendStoreImpl *friendStore,
					       const NpImpsCoreRosterBatch *batch)
{
	void *mem = npImpsCacheMalloc(sizeof(FriendAddOrUpdateEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	FriendAddOrUpdateEvent *event = new (mem) FriendAddOrUpdateEvent(friendStore);

	paf::rtc::Tick nowTick = 0;
	sceRtcGetCurrentTick(reinterpret_cast<SceRtcTick *>(&nowTick));

	for (SceUInt32 pos = 0; pos < batch->count; ++pos) {
		NpImpsCacheRosterData *rosterData = NULL;
		int ret = NpImpsCacheRosterData::CreateFromPushItem(&rosterData, &nowTick,
								    batch->items[pos]);
		if (ret >= 0 && rosterData != NULL) {
			event->AddItem(rosterData);
			rosterData->Release();
		}
	}
	return assignEvent(outEvent, event);
}

int npImpsCacheCreateFriendRemoveEvent(NpImpsCacheEvent **outEvent,
				       NpImpsCacheFriendStoreImpl *friendStore,
				       const SceNpId *npId)
{
	void *mem = npImpsCacheMalloc(sizeof(FriendRemoveEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	FriendRemoveEvent *event = new (mem) FriendRemoveEvent(friendStore);
	event->AddTarget(npId);
	return assignEvent(outEvent, event);
}

int npImpsCacheCreateFriendRosterBatchRemoveEvent(NpImpsCacheEvent **outEvent,
						  NpImpsCacheFriendStoreImpl *friendStore,
						  const NpImpsCoreRosterBatch *batch)
{
	void *mem = npImpsCacheMalloc(sizeof(FriendRemoveEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	FriendRemoveEvent *event = new (mem) FriendRemoveEvent(friendStore);
	for (SceUInt32 pos = 0; pos < batch->count; ++pos) {
		if (batch->items[pos] != NULL)
			event->AddTarget(&batch->items[pos]->npId);
	}
	return assignEvent(outEvent, event);
}

int npImpsCacheCreatePresenceUpdateEvent(NpImpsCacheEvent **outEvent,
					 NpImpsCacheFriendStoreImpl *friendStore,
					 const SceNpId *npId,
					 const NpImpsCorePresenceUpdate *update)
{
	NpImpsSvcPresenceData *presence = NULL;
	int ret = NpImpsSvcPresenceData::CreateFromCoreUpdate(&presence, update);
	if (ret < 0)
		return ret;

	void *mem = npImpsCacheMalloc(sizeof(FriendPresenceEvent));
	if (mem == NULL) {
		presence->Release();
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	}
	FriendPresenceEvent *event =
		new (mem) FriendPresenceEvent(friendStore, npId, presence, update->platform);
	presence->Release();
	return assignEvent(outEvent, event);
}

int npImpsCacheCreateInGameMessageEvent(NpImpsCacheEvent **outEvent,
					const SceNpId *senderNpId,
					const NpImpsCoreInGameMessage *msg,
					NpImpsCacheStoreSink *sink)
{
	if (msg == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;

	NpImpsSvcInGameDataMessage *wrapped = NULL;
	int ret = NpImpsSvcInGameDataMessage::Create(&wrapped, senderNpId, &msg->commId,
						     msg->payload, msg->payloadSize);
	if (ret < 0)
		return ret;

	void *mem = npImpsCacheMalloc(sizeof(InGameDataMessageEvent));
	if (mem == NULL) {
		wrapped->Release();
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	}
	InGameDataMessageEvent *event = new (mem) InGameDataMessageEvent(wrapped, sink);
	wrapped->Release();
	return assignEvent(outEvent, event);
}

int npImpsCacheCreateRetrySyncEvent(NpImpsCacheEvent **outEvent,
				    NpImpsCacheCoreImpl *core,
				    NpImpsCacheFriendStoreImpl *friendStore,
				    NpImpsCacheBlockStoreImpl *blockStore,
				    NpImpsCacheStoreSink *sink)
{
	void *mem = npImpsCacheMalloc(sizeof(RetryFailedSyncEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	return assignEvent(outEvent,
			   new (mem) RetryFailedSyncEvent(core, friendStore, blockStore, sink));
}

int npImpsCacheCreateNotificationEvent(NpImpsCacheEvent **outEvent, int notifyCode,
				       NpImpsCacheFriendStoreImpl *friendStore,
				       NpImpsCacheBlockStoreImpl *blockStore,
				       NpImpsCacheStoreSink *sink)
{
	void *mem = npImpsCacheMalloc(sizeof(ImpsCoreNotificationEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	return assignEvent(outEvent,
			   new (mem) ImpsCoreNotificationEvent(notifyCode, friendStore,
							       blockStore, sink));
}

int npImpsCacheCreateCoreStateEvent(NpImpsCacheEvent **outEvent,
				    NpImpsCacheFriendStoreImpl *friendStore,
				    NpImpsCacheBlockStoreImpl *blockStore,
				    int coreState, int errorCode,
				    NpImpsCacheStoreSink *sink)
{
	void *mem = npImpsCacheMalloc(sizeof(ImpsCoreStateEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	return assignEvent(outEvent,
			   new (mem) ImpsCoreStateEvent(friendStore, blockStore,
							coreState, errorCode, sink));
}

int npImpsCacheCreateSignInSyncEvent(NpImpsCacheEvent **outEvent,
				     NpImpsCacheFriendStoreImpl *friendStore,
				     NpImpsCacheBlockStoreImpl *blockStore,
				     NpImpsCacheStoreSink *sink)
{
	void *mem = npImpsCacheMalloc(sizeof(SignInSyncEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	return assignEvent(outEvent,
			   new (mem) SignInSyncEvent(friendStore, blockStore, sink));
}

int npImpsCacheCreateSignOutClearEvent(NpImpsCacheEvent **outEvent,
				       NpImpsCacheFriendStoreImpl *friendStore,
				       NpImpsCacheBlockStoreImpl *blockStore,
				       NpImpsCacheStoreSink *sink)
{
	void *mem = npImpsCacheMalloc(sizeof(SignOutClearEvent));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	return assignEvent(outEvent,
			   new (mem) SignOutClearEvent(friendStore, blockStore, sink));
}
