/*
 * np_imps_cache.h: SceNpImpsCache internal declarations (0x812FFE4C..0x813083AB).
 *
 * SceNpImpsCache maintains the in-memory friend list, presence, and block list
 * caches on a dedicated worker thread ("SceNpImpsCacheCore"):
 *   - receives asynchronous push notifications from SceNpImpsCore and account
 *     session events, enqueuing work items onto a bounded event queue (512 max),
 *   - synchronizes full friend and block lists via SceNpRoster when signing in,
 *     reconnecting, or retrying after a transient failure,
 *   - exposes thread-safe friend/block stores and per-friend ref-counted entries
 *     to SceNpImpsSvc via NpImpsCacheCore / NpImpsCacheObserver.
 */
#ifndef SHELL_NP_IMPS_CACHE_H
#define SHELL_NP_IMPS_CACHE_H

#include <stddef.h>
#include <scetypes.h>
#include <np/np_common.h>
#include <common/ref_obj.h>
#include <memory/heap_allocator.h>
#include <datetime/datetime.h>
#include <thread/thread.h>
#include "np_imps_svc.h"
#include "np_imps_core.h"
#include "../roster/np_roster.h"

/* SceNpImpsCache-specific error codes (0x80551400 range) */
#define NP_IMPS_CACHE_ERROR_QUEUE_FULL		-2141906419	/* 0x8055140D */
#define NP_IMPS_CACHE_ERROR_INDEX_OUT_OF_RANGE	-2141906416	/* 0x80551410 */

/* Friend relation values stored in NpImpsCacheRosterData */
enum NpImpsCacheFriendRelation {
	NP_IMPS_CACHE_REL_NONE		= 0,
	NP_IMPS_CACHE_REL_PENDING_OUT	= 1,
	NP_IMPS_CACHE_REL_REQUESTED_IN	= 2,
	NP_IMPS_CACHE_REL_ACCEPTED	= 3
};

/* SceNpImpsCore state notification values handled by ImpsCoreStateEvent */
enum NpImpsCoreStateCode {
	NP_IMPS_CORE_STATE_ONLINE		= 1,
	NP_IMPS_CORE_STATE_BLOCKLIST_READY	= 6,
	NP_IMPS_CORE_STATE_OFFLINE_BEGIN	= 7,
	NP_IMPS_CORE_STATE_OFFLINE_END		= 11
};

/* SceNpImpsCore friend/roster push notification types */
enum NpImpsCoreNotifyAction {
	NP_IMPS_CORE_NOTIFY_FRIEND_ADDED	= 0,
	NP_IMPS_CORE_NOTIFY_FRIEND_REMOVED	= 1
};

/* SceNpImpsCore roster push aliases matching SceNpBasic ref-counted types */
typedef NpBasicRosterItem NpImpsCoreRosterItem;
typedef NpBasicRosterList NpImpsCoreRosterBatch;

/* Dynamic array of ref-counted pointers (friend handles or roster data) */
template <typename T>
class NpImpsCacheRefVector {
public:
	NpImpsCacheRefVector() : m_items(NULL), m_count(0), m_capacity(0) {}
	~NpImpsCacheRefVector() { Clear(); }

	void Clear()
	{
		if (m_items != NULL) {
			for (SceUInt32 pos = m_count; pos > 0; --pos) {
				if (m_items[pos - 1] != NULL)
					m_items[pos - 1]->Release();
			}
			npImpsCacheFree(m_items);
			m_items = NULL;
		}
		m_count = 0;
		m_capacity = 0;
	}

	int PushBack(T *item)
	{
		if (m_count >= m_capacity) {
			SceUInt32 newCap = (m_capacity == 0) ? 8 : (m_capacity * 2);
			T **grown = reinterpret_cast<T **>(
				npImpsCacheMalloc(sizeof(T *) * newCap));
			if (grown == NULL)
				return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			for (SceUInt32 pos = 0; pos < m_count; ++pos)
				grown[pos] = m_items[pos];
			if (m_items != NULL)
				npImpsCacheFree(m_items);
			m_items = grown;
			m_capacity = newCap;
		}
		if (item != NULL)
			item->AddRef();
		m_items[m_count++] = item;
		return 0;
	}

	SceUInt32 Size() const { return m_count; }
	T *At(SceUInt32 index) const { return m_items[index]; }
	T **Data() const { return m_items; }

private:
	T **m_items;
	SceUInt32 m_count;
	SceUInt32 m_capacity;
};

/* Case-insensitive SceNpId handle comparator (first 16 bytes of onlineId.data) */
extern int npImpsCacheCompareNpId(const SceNpId *lhs, const SceNpId *rhs);

/* Sorted unique set of SceNpId values (replaces RB-tree set over SceNpId) */
class NpImpsCacheNpIdSet {
public:
	NpImpsCacheNpIdSet() : m_items(NULL), m_count(0), m_capacity(0) {}
	~NpImpsCacheNpIdSet() { Clear(); }

	void Clear();
	bool Contains(const SceNpId *npId) const;
	int Insert(const SceNpId *npId);
	bool Erase(const SceNpId *npId);

	SceUInt32 Size() const { return m_count; }
	const SceNpId *At(SceUInt32 index) const { return &m_items[index]; }
	const SceNpId *Data() const { return m_items; }

private:
	SceUInt32 LowerBound(const SceNpId *npId) const;

	SceNpId *m_items;
	SceUInt32 m_count;
	SceUInt32 m_capacity;
};

/* Ref-counted roster metadata for a friend entry (64 bytes, 0x813076DE..0x8130795B) */
class NpImpsCacheRosterData : public paf::common::RefObj {
public:
	NpImpsCacheRosterData();
	virtual ~NpImpsCacheRosterData();

	static int CreateFromRosterEntry(NpImpsCacheRosterData **outData,
					 const NpRosterFriendEntry *entry);
	static int CreateFromPushItem(NpImpsCacheRosterData **outData,
				      const paf::rtc::Tick *timestamp,
				      const NpImpsCoreRosterItem *item);
	static int CreateWithRelation(NpImpsCacheRosterData **outData,
				      const SceNpId *npId, int relation);

	void SetRelationAndDate(int relation, const paf::rtc::Tick *date);
	void SetRequestDirection(int direction) { m_requestDirection = direction; }

	const SceNpId *GetNpId() const { return &m_npId; }
	int GetRelation() const { return m_relation; }
	const paf::rtc::Tick *GetDate() const { return &m_updateDate; }
	int GetRequestDirection() const { return m_requestDirection; }

private:
	SceNpId m_npId;				/* +0x08 (36 bytes) */
	int m_relation;				/* +0x2C */
	paf::rtc::Tick m_updateDate;		/* +0x30 (8 bytes) */
	int m_requestDirection;			/* +0x38 */
};

/* Concrete cached friend entry (68 bytes, 0x8130640C..0x813076D3) */
class NpImpsCacheFriendEntryImpl : public NpImpsCacheFriendEntry {
public:
	NpImpsCacheFriendEntryImpl(const SceNpId *npId, paf::thread::RMutex *dataMutex);
	virtual ~NpImpsCacheFriendEntryImpl();

	static int Create(NpImpsCacheFriendEntryImpl **outEntry,
			  NpImpsCacheRosterData **rosterData,
			  paf::thread::RMutex *dataMutex);

	/* NpImpsCacheFriendEntry overrides */
	virtual void UpdateRosterData(int relation, const paf::rtc::Tick *date);
	virtual int UpdatePresenceDefault(NpImpsSvcPresenceData **newPresence,
					  NpImpsSvcPresenceDiffFlags *outDiff,
					  NpImpsSvcPresenceData **outPrev);
	virtual int UpdatePresenceForPlatform(NpImpsSvcPresenceData **newPresence,
					      NpImpsSvcPresenceDiffFlags *outDiff,
					      NpImpsSvcPresenceData **outPrev,
					      int platform);
	virtual void GetNpId(SceNpId *outNpId);
	virtual int GetRelation();
	virtual void GetDate(paf::rtc::Tick *outTick);
	virtual int GetRequestDirection();
	virtual SceUInt8 GetIsNew();
	virtual void SetIsNew(SceUInt8 isNew);
	virtual int GetFriendContext();
	virtual int GetOnlineStatus(int platform);
	virtual int GetPlatform(int platform);
	virtual int CopyComment(char **outBuf, void *(*allocFn)(SceSize), int platform);
	virtual int GetTitlePlatform(int platform);
	virtual void GetLastOnlineTick(paf::rtc::Tick *outTick, int platform);
	virtual void GetTitleId(SceNpCommunicationId *outId, int platform);
	virtual void GetTitleIdStr(char *outBuf, int platform);
	virtual void GetTitleName(char *outBuf, int platform);
	virtual SceUInt8 IsSystemTitle(int platform);
	virtual void GetJoinableCommId(SceNpCommunicationId *outId, int platform);
	virtual void GetContentId(char *outBuf, int platform);
	virtual int GetGamePresenceType(int platform);
	virtual void GetCommId(SceNpCommunicationId *outId, int platform);
	virtual void GetGameStatus(char *outBuf, int platform);
	virtual SceSize GetGameDataSize(int platform);
	virtual int CopyGameData(void *dst, SceSize maxLen, int platform);
	virtual int GetPartyPresenceType(int platform);
	virtual void GetPartyTitle(char *outBuf, int platform);
	virtual void GetPartyStatus(char *outBuf, int platform);
	virtual SceSize GetPartyDataSize(int platform);
	virtual int CopyPartyData(void *dst, SceSize maxLen, int platform);
	virtual int FreeAllocatedString(void *ptr, void (*freeFn)(void *));

private:
	struct PlatformPresenceSlot {
		int platform;
		NpImpsSvcPresenceData *presence;
	};

	NpImpsSvcPresenceData *FindPlatformPresenceLocked(int platform) const;
	int SetPlatformPresenceLocked(int platform, NpImpsSvcPresenceData *presence);
	bool IsHighestEffectiveOnlineStatus(NpImpsSvcPresenceData *candidate, int platform);

	paf::thread::RMutex *m_mutex;
	SceNpId m_npId;
	NpImpsCacheRosterData *m_rosterData;
	PlatformPresenceSlot *m_presences;
	SceUInt32 m_presenceCount;
	SceUInt32 m_presenceCapacity;
	SceUInt8 m_isNew;
};

/* Base class for queued cache worker events (0x81308394) */
class NpImpsCacheEvent : public paf::common::RefObj {
public:
	NpImpsCacheEvent();
	virtual ~NpImpsCacheEvent() {}
	virtual int Execute() = 0;
};

/* Bounded FIFO event queue (max 512 items, "SceNpImpsCacheEventQueueMutex", 0x81301B36..0x81301C9F) */
class NpImpsCacheEventQueue {
public:
	NpImpsCacheEventQueue();
	~NpImpsCacheEventQueue();

	int Push(NpImpsCacheEvent **event);
	void DrainAll();

private:
	struct QueueNode {
		QueueNode *prev;
		QueueNode *next;
		NpImpsCacheEvent *event;
	};

	SceUInt32 m_maxCapacity;
	paf::thread::RMutex m_mutex;
	QueueNode *m_head;
	QueueNode *m_tail;
	SceUInt32 m_count;
};

/* Internal store-to-core notification sink (implemented by NpImpsCacheCoreImpl) */
class NpImpsCacheStoreSink {
public:
	virtual ~NpImpsCacheStoreSink() {}
	virtual int NotifySignInComplete() = 0;
	virtual int NotifySignOutComplete() = 0;
	virtual int NotifyNotificationEvent(int notifyCode) = 0;
	virtual int NotifyFriendListLoaded(const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *entries) = 0;
	virtual int NotifyFriendListAdded(const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *entries) = 0;
	virtual int NotifyFriendListUpdated(const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *entries) = 0;
	virtual int NotifyFriendsRemoved(const NpImpsCacheNpIdSet *npIds) = 0;
	virtual int NotifyFriendRequestsRemoved(const NpImpsCacheNpIdSet *npIds) = 0;
	virtual int NotifyFriendContextRemoved(const NpImpsCacheNpIdSet *npIds) = 0;
	virtual int NotifyBlockListLoaded(const NpImpsCacheNpIdSet *npIds) = 0;
	virtual int NotifyBlockListAdded(const NpImpsCacheNpIdSet *npIds) = 0;
	virtual int NotifyBlockListRemoved(const NpImpsCacheNpIdSet *npIds) = 0;
	virtual int NotifyFriendContextChanged(NpImpsCacheFriendEntryImpl **entry,
					       int contextState, void *extra) = 0;
	virtual int NotifyFriendPresenceChanged(NpImpsCacheFriendEntryImpl **entry,
						NpImpsSvcPresenceData **curPresence,
						NpImpsSvcPresenceData **prevPresence,
						const NpImpsSvcPresenceDiffFlags *diffFlags) = 0;
	virtual int NotifyPlatformPresenceChanged(NpImpsCacheFriendEntryImpl **entry,
						  NpImpsSvcPresenceData **curPresence,
						  NpImpsSvcPresenceData **prevPresence,
						  const NpImpsSvcPresenceDiffFlags *diffFlags,
						  int platform) = 0;
	virtual int NotifyInGameDataMessage(NpImpsSvcInGameDataMessage **msg) = 0;
};

/* Concrete friend list cache store (0x81301CA0..0x813043B3) */
class NpImpsCacheFriendStoreImpl : public NpImpsCacheFriendStore {
public:
	NpImpsCacheFriendStoreImpl(NpImpsCacheStoreSink *sink, paf::thread::RMutex *dataMutex);
	virtual ~NpImpsCacheFriendStoreImpl();

	/* NpImpsCacheFriendStore overrides */
	virtual void Lock();
	virtual void Unlock();
	virtual SceUInt32 GetCount();
	virtual int CopyHandles(NpImpsCacheFriendPtr *outHandles, SceUInt32 count);
	virtual SceUInt32 GetAcceptedCount(SceUInt8 *outLoaded);
	virtual void CopyAcceptedNpIds(SceUInt32 startIndex, SceNpId *outEntries,
				       SceUInt32 maxCount, SceUInt32 *outCount,
				       SceUInt8 *outLoaded);
	virtual SceUInt32 GetPendingCount(SceUInt8 *outLoaded);
	virtual void CopyPendingNpIds(SceUInt32 startIndex, SceNpId *outEntries,
				      SceUInt32 maxCount, SceUInt32 *outCount,
				      SceUInt8 *outLoaded);
	virtual SceUInt32 GetRequestedCount(SceUInt8 *outLoaded);
	virtual void CopyRequestedNpIds(SceUInt32 startIndex, SceNpId *outEntries,
					SceUInt32 maxCount, SceUInt32 *outCount,
					SceUInt8 *outLoaded);
	virtual int CopyAllNpIds(SceUInt32 startIndex, SceNpId *outEntries,
				 SceUInt32 maxCount, SceUInt32 *outCount);
	virtual int GetRelation(const SceNpId *npId, int *outRelation);
	virtual int GetDate(const SceNpId *npId, paf::rtc::Tick *outTick);
	virtual int GetFriendContext(const SceNpId *npId, int *outContext);
	virtual int GetOnlineStatus(const SceNpId *npId, int *outStatus);
	virtual int GetTitlePlatform(const SceNpId *npId, int *outPlatform);
	virtual int GetLastOnlineTick(const SceNpId *npId, paf::rtc::Tick *outTick,
				      bool *outHasTick);
	virtual int GetTitleId(const SceNpId *npId, SceNpCommunicationId *outId);
	virtual int GetTitleName(const SceNpId *npId, char *outTitleName);
	virtual int IsSystemTitle(const SceNpId *npId, SceUInt8 *outIsSystem);
	virtual int GetContentId(const SceNpId *npId, char *outContentId);
	virtual int GetGamePresenceType(const SceNpId *npId, int *outType);
	virtual int GetCommId(const SceNpId *npId, SceNpCommunicationId *outId);
	virtual int GetGameStatus(const SceNpId *npId, char *outStatus);
	virtual int GetGameData(const SceNpId *npId, void *outData);
	virtual int GetGameDataSize(const SceNpId *npId, SceSize *outSize);
	virtual int GetPartyPresenceType(const SceNpId *npId, int *outType);
	virtual int GetPartyTitle(const SceNpId *npId, char *outTitle);
	virtual int GetPartyStatus(const SceNpId *npId, char *outStatus);
	virtual int GetPartyData(const SceNpId *npId, void *outData);
	virtual int GetPartyDataSize(const SceNpId *npId, SceSize *outSize);
	virtual bool Contains(const SceNpId *npId);
	virtual int GetTitleIdForPlatform(const SceNpId *npId, SceNpCommunicationId *outId,
					  int platform);
	virtual int GetTitleNameForPlatform(const SceNpId *npId, char *outTitleName,
					    int platform);
	virtual int GetGamePresenceTypeForPlatform(const SceNpId *npId, int *outType,
						   int platform);
	virtual int GetCommIdForPlatform(const SceNpId *npId, SceNpCommunicationId *outId,
					 int platform);
	virtual int GetGameStatusForPlatform(const SceNpId *npId, char *outStatus,
					     int platform);
	virtual int GetGameDataForPlatform(const SceNpId *npId, void *outData,
					   int platform);
	virtual int GetGameDataSizeForPlatform(const SceNpId *npId, SceSize *outSize,
					       int platform);
	virtual int GetNpIdAt(SceUInt32 index, SceNpId *outNpId);

	/* Mutators invoked from cache worker events */
	int ApplyFullList(const NpImpsCacheRefVector<NpImpsCacheRosterData> *fetched,
			  NpImpsCacheNpIdSet *outRemovedAll,
			  NpImpsCacheNpIdSet *outRemovedAccepted,
			  NpImpsCacheNpIdSet *outRemovedPendingOrReq,
			  NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outAdded,
			  NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outUpdated,
			  bool *outFirstLoad);
	int FetchAndApply(NpImpsCacheNpIdSet *outRemovedAll,
			  NpImpsCacheNpIdSet *outRemovedAccepted,
			  NpImpsCacheNpIdSet *outRemovedPendingOrReq,
			  NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outAdded,
			  NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outUpdated,
			  bool *outFirstLoad);
	int AddOrUpdateList(const NpImpsCacheRefVector<NpImpsCacheRosterData> *items);
	int RemoveList(const NpImpsCacheNpIdSet *npIds);
	int UpdatePresence(const SceNpId *npId, NpImpsSvcPresenceData **newPresence,
			   int platform);
	int ResetAllPresenceOffline();
	int FetchFromRoster(NpImpsCacheRefVector<NpImpsCacheRosterData> *outList);
	int Clear();

private:
	struct FriendMapSlot {
		SceNpId npId;
		NpImpsCacheFriendEntryImpl *entry;
	};

	SceUInt32 LowerBoundLocked(const SceNpId *npId) const;
	NpImpsCacheFriendEntryImpl *FindLocked(const SceNpId *npId) const;
	int InsertLocked(const SceNpId *npId, NpImpsCacheFriendEntryImpl *entry);
	bool EraseLocked(const SceNpId *npId);
	int AddEntryLocked(NpImpsCacheRosterData **rosterData,
			   NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outAdded);
	int UpdateEntryLocked(SceUInt32 slotIndex, NpImpsCacheRosterData **rosterData,
			      NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outUpdated);
	int ApplyAddedAndUpdated(const NpImpsCacheRefVector<NpImpsCacheRosterData> *fetched,
				 NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outAdded,
				 NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outUpdated);
	int CollectAndRemoveMissing(const NpImpsCacheRefVector<NpImpsCacheRosterData> *fetched,
				    NpImpsCacheNpIdSet *outRemovedAll,
				    NpImpsCacheNpIdSet *outRemovedAccepted,
				    NpImpsCacheNpIdSet *outRemovedPendingOrReq);
	void CollectPendingAndRequested(const NpImpsCacheNpIdSet *npIds,
					NpImpsCacheNpIdSet *outSet);
	void CollectAccepted(const NpImpsCacheNpIdSet *npIds, NpImpsCacheNpIdSet *outSet);
	SceUInt32 CountByRelation(SceUInt8 *outLoaded, int relation);
	SceUInt32 CountByRelationLocked(int relation) const;
	void CopyByRelationLocked(SceUInt32 startIndex, SceNpId *outEntries,
				  SceUInt32 maxCount, SceUInt32 *outCount,
				  SceUInt8 *outLoaded, int relation);

	NpImpsCacheStoreSink *m_sink;
	paf::thread::RMutex *m_mutex;
	FriendMapSlot *m_entries;
	SceUInt32 m_count;
	SceUInt32 m_capacity;
	SceUInt8 m_loaded;
};

/* Concrete block list cache store (0x8130795C..0x8130827F) */
class NpImpsCacheBlockStoreImpl : public NpImpsCacheBlockStore {
public:
	NpImpsCacheBlockStoreImpl(NpImpsCacheStoreSink *sink, paf::thread::RMutex *dataMutex);
	virtual ~NpImpsCacheBlockStoreImpl();

	/* NpImpsCacheBlockStore overrides */
	virtual void Lock();
	virtual void Unlock();
	virtual SceUInt32 GetCount(SceUInt8 *outLoaded);
	virtual void CopyEntries(SceUInt32 startIndex, SceNpId *outEntries,
				 SceUInt32 maxCount, SceUInt32 *outCount,
				 SceUInt8 *outLoaded);
	virtual bool Contains(const SceNpId *npId, SceUInt8 *outLoaded);

	/* Mutators invoked from cache worker events */
	int ApplyFullList(const SceNpId *entries, SceUInt32 count,
			  NpImpsCacheNpIdSet *outAdded,
			  NpImpsCacheNpIdSet *outRemoved,
			  bool *outFirstLoad);
	int AddEntries(const NpImpsCacheNpIdSet *npIds);
	int RemoveEntries(const NpImpsCacheNpIdSet *npIds);
	int FetchAndApply(NpImpsCacheNpIdSet *outAdded, NpImpsCacheNpIdSet *outRemoved,
			  bool *outFirstLoad);
	int FetchFromRoster(NpImpsCacheNpIdSet *outSet);
	int Clear();

private:
	NpImpsCacheStoreSink *m_sink;
	paf::thread::RMutex *m_mutex;
	NpImpsCacheNpIdSet m_blockSet;
	SceUInt8 m_loaded;
};

/* Main SceNpImpsCache worker core (0x812FFE9E..0x81301B35) */
class NpImpsCacheCoreImpl : public paf::thread::Thread,
			    public NpImpsCacheCore,
			    public NpImpsCacheStoreSink,
			    public NpImpsCacheRosterSync {
public:
	NpImpsCacheCoreImpl(int threadPriority, SceSize stackSize,
			    paf::memory::HeapAllocator *heap);
	virtual ~NpImpsCacheCoreImpl();

	static int Init(int threadPriority, SceSize stackSize,
			paf::memory::HeapAllocator *heap);
	static int Term();
	static NpImpsCacheCoreImpl *GetInstance();

	/* paf::thread::Thread override */
	virtual void EntryFunction();

	/* NpImpsCacheCore overrides */
	virtual int RegisterObserver(NpImpsCacheObserver *observer);
	virtual int UnregisterObserver();
	virtual const SceNpCommunicationId *GetPartyTitleId();
	virtual paf::thread::RMutex *GetStateMutex();
	virtual void Lock();
	virtual void Unlock();

	/* NpImpsCacheStoreSink overrides */
	virtual int NotifySignInComplete();
	virtual int NotifySignOutComplete();
	virtual int NotifyNotificationEvent(int notifyCode);
	virtual int NotifyFriendListLoaded(const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *entries);
	virtual int NotifyFriendListAdded(const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *entries);
	virtual int NotifyFriendListUpdated(const NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *entries);
	virtual int NotifyFriendsRemoved(const NpImpsCacheNpIdSet *npIds);
	virtual int NotifyFriendRequestsRemoved(const NpImpsCacheNpIdSet *npIds);
	virtual int NotifyFriendContextRemoved(const NpImpsCacheNpIdSet *npIds);
	virtual int NotifyBlockListLoaded(const NpImpsCacheNpIdSet *npIds);
	virtual int NotifyBlockListAdded(const NpImpsCacheNpIdSet *npIds);
	virtual int NotifyBlockListRemoved(const NpImpsCacheNpIdSet *npIds);
	virtual int NotifyFriendContextChanged(NpImpsCacheFriendEntryImpl **entry,
					       int contextState, void *extra);
	virtual int NotifyFriendPresenceChanged(NpImpsCacheFriendEntryImpl **entry,
						NpImpsSvcPresenceData **curPresence,
						NpImpsSvcPresenceData **prevPresence,
						const NpImpsSvcPresenceDiffFlags *diffFlags);
	virtual int NotifyPlatformPresenceChanged(NpImpsCacheFriendEntryImpl **entry,
						  NpImpsSvcPresenceData **curPresence,
						  NpImpsSvcPresenceData **prevPresence,
						  const NpImpsSvcPresenceDiffFlags *diffFlags,
						  int platform);
	virtual int NotifyInGameDataMessage(NpImpsSvcInGameDataMessage **msg);

	virtual NpImpsCacheFriendStore *GetFriendStore();
	virtual NpImpsCacheBlockStore *GetBlockStore();
	virtual NpImpsCacheRosterSync *GetRosterSync();

	/* NpImpsCacheRosterSync overrides */
	virtual int TriggerRosterUpdate(const SceNpId *npId, int syncReason);
	virtual int ForceFullSync(int syncFlags);

	/* Core state forwarding used by ImpsCoreStateEvent */
	int ForwardImpsCoreState(int coreState, int errorCode);

	/* Retry-sync flags set when roster HTTP fetch fails transiently */
	void MarkFriendSyncRetry();
	void ClearFriendSyncRetry();
	bool NeedsFriendSyncRetry();
	void MarkBlockSyncRetry();
	void ClearBlockSyncRetry();
	bool NeedsBlockSyncRetry();

	/* Enqueue helpers invoked by static callback trampolines */
	int EnqueueImpsCoreState(int coreState, int errorCode);
	int EnqueueFriendRosterAdded(const NpImpsCoreRosterBatch *batch);
	int EnqueueFriendRosterRemoved(const NpImpsCoreRosterBatch *batch);
	int EnqueueBlockListRefresh();
	int EnqueuePresenceUpdate(const SceNpId *npId, const NpImpsCorePresenceUpdate *update);
	int EnqueueInGameDataMessage(const SceNpId *senderNpId,
				     const NpImpsCoreMessage *msg);
	int EnqueueNotification(int notifyCode);
	int EnqueueRetrySync();
	int EnqueueSignInSync();
	int EnqueueSignOutClear();

	void SignalWorker() { m_waitCond.Notify(); }

private:
	int EnqueueBlockAdd(const SceNpId *npId);
	int EnqueueBlockRemove(const SceNpId *npId);
	int EnqueueFriendAddOrUpdate(const SceNpId *npId, int relation);
	int EnqueueFriendRemove(const SceNpId *npId);

	static void OnCoreCtlEvent(int eventType);
	static void OnCoreSessionState(const NpImpsSessionIdentity *session, int coreState,
				       int errorCode);
	static void OnCoreFriendRosterPush(const NpImpsSessionIdentity *session, int action,
					   const NpBasicRosterList *batch);
	static void OnCoreBlockListPush(const NpImpsSessionIdentity *session, int action,
					const NpBasicPrivacyList *privacyList);
	static void OnCorePresencePush(const NpImpsSessionIdentity *session, const SceNpId *npId,
				       const NpImpsCorePresenceUpdate *update);
	static void OnCoreInGameDataMessage(const NpImpsSessionIdentity *session,
					    const SceNpId *senderNpId,
					    const NpImpsCoreMessage *msg);
	static void OnCoreNotification(const NpImpsSessionIdentity *session, int notifyCode,
				       const SceRtcTick *timestamp);
	static void OnAccountSessionCallback(int eventId, int stateGroup, int stateCode,
					     int detailA, int detailB, void *userArg);

	paf::thread::Mutex m_waitMutex;
	paf::thread::Cond m_waitCond;
	paf::memory::HeapAllocator *m_heap;
	paf::thread::RMutex m_dataMutex;
	NpImpsCacheFriendStoreImpl m_friendStore;
	NpImpsCacheBlockStoreImpl m_blockStore;
	NpImpsCacheObserver *m_observer;
	NpImpsCacheEventQueue *m_eventQueue;
	int m_accountCallbackId;
	int m_impsCoreCallbackId;
	SceNpCommunicationId m_partyTitleId;
	SceUInt8 m_queueOverflowed;
	SceUInt8 m_retryFriendSync;
	SceUInt8 m_retryBlockSync;
};

/* Event factory functions (implemented in np_imps_cache_events.cpp) */
extern int npImpsCacheCreateForceSyncEvent(NpImpsCacheEvent **outEvent, int syncFlags,
					   NpImpsCacheFriendStoreImpl *friendStore,
					   NpImpsCacheBlockStoreImpl *blockStore,
					   NpImpsCacheStoreSink *sink);
extern int npImpsCacheCreateRefreshBlockListEvent(NpImpsCacheEvent **outEvent,
						  NpImpsCacheBlockStoreImpl *blockStore);
extern int npImpsCacheCreateBlockAddEvent(NpImpsCacheEvent **outEvent,
					  NpImpsCacheFriendStoreImpl *friendStore,
					  NpImpsCacheBlockStoreImpl *blockStore,
					  SceUInt32 count, const SceNpId *npIds);
extern int npImpsCacheCreateBlockRemoveEvent(NpImpsCacheEvent **outEvent,
					     NpImpsCacheBlockStoreImpl *blockStore,
					     SceUInt32 count, const SceNpId *npIds);
extern int npImpsCacheCreateFriendAddOrUpdateEvent(NpImpsCacheEvent **outEvent,
						   NpImpsCacheFriendStoreImpl *friendStore,
						   const SceNpId *npId, int relation);
extern int npImpsCacheCreateFriendRosterBatchAddEvent(NpImpsCacheEvent **outEvent,
						      NpImpsCacheFriendStoreImpl *friendStore,
						      const NpImpsCoreRosterBatch *batch);
extern int npImpsCacheCreateFriendRemoveEvent(NpImpsCacheEvent **outEvent,
					      NpImpsCacheFriendStoreImpl *friendStore,
					      const SceNpId *npId);
extern int npImpsCacheCreateFriendRosterBatchRemoveEvent(NpImpsCacheEvent **outEvent,
							 NpImpsCacheFriendStoreImpl *friendStore,
							 const NpImpsCoreRosterBatch *batch);
extern int npImpsCacheCreatePresenceUpdateEvent(NpImpsCacheEvent **outEvent,
						NpImpsCacheFriendStoreImpl *friendStore,
						const SceNpId *npId,
						const NpImpsCorePresenceUpdate *update);
extern int npImpsCacheCreateInGameMessageEvent(NpImpsCacheEvent **outEvent,
					       const SceNpId *senderNpId,
					       const NpImpsCoreMessage *msg,
					       NpImpsCacheStoreSink *sink);
extern int npImpsCacheCreateRetrySyncEvent(NpImpsCacheEvent **outEvent,
					   NpImpsCacheCoreImpl *core,
					   NpImpsCacheFriendStoreImpl *friendStore,
					   NpImpsCacheBlockStoreImpl *blockStore,
					   NpImpsCacheStoreSink *sink);
extern int npImpsCacheCreateNotificationEvent(NpImpsCacheEvent **outEvent, int notifyCode,
					      NpImpsCacheFriendStoreImpl *friendStore,
					      NpImpsCacheBlockStoreImpl *blockStore,
					      NpImpsCacheStoreSink *sink);
extern int npImpsCacheCreateCoreStateEvent(NpImpsCacheEvent **outEvent,
					   NpImpsCacheFriendStoreImpl *friendStore,
					   NpImpsCacheBlockStoreImpl *blockStore,
					   int coreState, int errorCode,
					   NpImpsCacheStoreSink *sink);
extern int npImpsCacheCreateSignInSyncEvent(NpImpsCacheEvent **outEvent,
					    NpImpsCacheFriendStoreImpl *friendStore,
					    NpImpsCacheBlockStoreImpl *blockStore,
					    NpImpsCacheStoreSink *sink);
extern int npImpsCacheCreateSignOutClearEvent(NpImpsCacheEvent **outEvent,
					      NpImpsCacheFriendStoreImpl *friendStore,
					      NpImpsCacheBlockStoreImpl *blockStore,
					      NpImpsCacheStoreSink *sink);

/* Roster fetch & buffer allocation helpers (0x81304250, 0x81304324, 0x81308280..0x81308393) */
extern int npImpsCacheFetchFriendListFromRoster(NpImpsCacheRefVector<NpImpsCacheRosterData> *outList);
extern int npImpsCacheFetchBlockListFromRoster(NpImpsCacheNpIdSet *outSet);
extern bool npImpsCacheRosterListContains(const SceNpId *npId,
					  const NpImpsCacheRefVector<NpImpsCacheRosterData> *list);
extern bool npImpsCacheArrayContains(const SceNpId *npId, const SceNpId *array, SceUInt32 count);
extern int npImpsCacheAllocFriendHandleArray(SceUInt32 count, NpImpsCacheFriendPtr **outArray);
extern void npImpsCacheFreeFriendHandleArray(SceUInt32 count, NpImpsCacheFriendPtr *array);
extern int npImpsCacheAllocNpIdArray(SceUInt32 count, SceNpId **outArray);

/* External bridge to Shell account event service */
typedef void (*ShellAccountEventCallback)(int eventId, int stateGroup, int stateCode,
					  int detailA, int detailB, void *userArg);
extern "C" {
int SceShell_2E0B6FA6(ShellAccountEventCallback callback, void *userArg, int *outId);
int SceShell_B4D888D5(int callbackId);
}

#endif
