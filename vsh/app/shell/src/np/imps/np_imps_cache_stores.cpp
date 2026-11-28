/*
 * np_imps_cache_stores.cpp: SceNpImpsCache roster data, friend entry, friend
 * store, and block store implementations (0x81301CA0..0x813043B3,
 * 0x8130640C..0x81308393).
 */
#include <stddef.h>
#include <std/new>
#include <std/string.h>
#include <std/stdlib.h>
#include <scetypes.h>
#include <kernel.h>
#include <libkernel/libkernel.h>
#include "np_imps_cache.h"

/* --------------------------------------------------------------------------
 * SceNpId comparison & sorted set helpers
 * -------------------------------------------------------------------------- */
int npImpsCacheCompareNpId(const SceNpId *lhs, const SceNpId *rhs)
{
	return sceClibStrncasecmp(lhs->handle.data, rhs->handle.data, 16);
}

void NpImpsCacheNpIdSet::Clear()
{
	if (m_items != NULL) {
		npImpsCacheFree(m_items);
		m_items = NULL;
	}
	m_count = 0;
	m_capacity = 0;
}

SceUInt32 NpImpsCacheNpIdSet::LowerBound(const SceNpId *npId) const
{
	SceUInt32 low = 0;
	SceUInt32 high = m_count;
	while (low < high) {
		SceUInt32 mid = low + ((high - low) >> 1);
		if (npImpsCacheCompareNpId(&m_items[mid], npId) < 0)
			low = mid + 1;
		else
			high = mid;
	}
	return low;
}

bool NpImpsCacheNpIdSet::Contains(const SceNpId *npId) const
{
	if (npId == NULL || m_count == 0)
		return false;
	SceUInt32 pos = LowerBound(npId);
	return (pos < m_count && npImpsCacheCompareNpId(&m_items[pos], npId) == 0);
}

int NpImpsCacheNpIdSet::Insert(const SceNpId *npId)
{
	if (npId == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;

	SceUInt32 pos = LowerBound(npId);
	if (pos < m_count && npImpsCacheCompareNpId(&m_items[pos], npId) == 0)
		return 0;

	if (m_count >= m_capacity) {
		SceUInt32 newCap = (m_capacity == 0) ? 8 : (m_capacity * 2);
		SceNpId *grown = reinterpret_cast<SceNpId *>(
			npImpsCacheMalloc(sizeof(SceNpId) * newCap));
		if (grown == NULL)
			return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		if (m_items != NULL) {
			sce_paf_memcpy(grown, m_items, sizeof(SceNpId) * m_count);
			npImpsCacheFree(m_items);
		}
		m_items = grown;
		m_capacity = newCap;
	}

	for (SceUInt32 move = m_count; move > pos; --move)
		m_items[move] = m_items[move - 1];
	m_items[pos] = *npId;
	++m_count;
	return 1;
}

bool NpImpsCacheNpIdSet::Erase(const SceNpId *npId)
{
	if (npId == NULL || m_count == 0)
		return false;

	SceUInt32 pos = LowerBound(npId);
	if (pos >= m_count || npImpsCacheCompareNpId(&m_items[pos], npId) != 0)
		return false;

	for (SceUInt32 move = pos + 1; move < m_count; ++move)
		m_items[move - 1] = m_items[move];
	--m_count;
	return true;
}

/* 0x81308280 */
bool npImpsCacheRosterListContains(const SceNpId *npId,
				   const NpImpsCacheRefVector<NpImpsCacheRosterData> *list)
{
	if (npId == NULL || list == NULL)
		return false;
	for (SceUInt32 pos = 0; pos < list->Size(); ++pos) {
		NpImpsCacheRosterData *item = list->At(pos);
		if (item != NULL && npImpsCacheCompareNpId(npId, item->GetNpId()) == 0)
			return true;
	}
	return false;
}

/* 0x813082C4 */
bool npImpsCacheArrayContains(const SceNpId *npId, const SceNpId *array, SceUInt32 count)
{
	if (npId == NULL || array == NULL)
		return false;
	for (SceUInt32 pos = 0; pos < count; ++pos) {
		if (npImpsCacheCompareNpId(npId, &array[pos]) == 0)
			return true;
	}
	return false;
}

/* 0x813082F8 */
int npImpsCacheAllocFriendHandleArray(SceUInt32 count, NpImpsCacheFriendPtr **outArray)
{
	SceSize byteCount = sizeof(NpImpsCacheFriendPtr) * count;
	void *mem = npImpsCacheMalloc(byteCount);
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	NpImpsCacheFriendPtr *arr = reinterpret_cast<NpImpsCacheFriendPtr *>(mem);
	for (SceUInt32 pos = 0; pos < count; ++pos)
		arr[pos] = NULL;
	*outArray = arr;
	return 0;
}

/* 0x8130833E */
void npImpsCacheFreeFriendHandleArray(SceUInt32 count, NpImpsCacheFriendPtr *array)
{
	for (SceUInt32 pos = 0; pos < count; ++pos) {
		if (array[pos] != NULL)
			array[pos]->Release();
	}
	npImpsCacheFree(array);
}

/* 0x81308366 */
int npImpsCacheAllocNpIdArray(SceUInt32 count, SceNpId **outArray)
{
	SceSize byteCount = sizeof(SceNpId) * count;
	void *mem = npImpsCacheMalloc(byteCount);
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	*outArray = reinterpret_cast<SceNpId *>(mem);
	return 0;
}

/* 0x81308394 */
NpImpsCacheEvent::NpImpsCacheEvent()
{
}

/* --------------------------------------------------------------------------
 * NpImpsCacheRosterData (0x813076BE..0x8130795B)
 * -------------------------------------------------------------------------- */
NpImpsCacheRosterData::NpImpsCacheRosterData()
	: m_relation(0),
	  m_requestDirection(-1)
{
	sce_paf_memset(&m_npId, 0, sizeof(m_npId));
	m_updateDate = 0;
}

/* 0x813076BE */
NpImpsCacheRosterData::~NpImpsCacheRosterData()
{
}

/* 0x813076D4 */
void NpImpsCacheRosterData::SetRelationAndDate(int relation, const paf::rtc::Tick *date)
{
	m_relation = relation;
	m_updateDate = *date;
}

/* 0x8130771C */
int NpImpsCacheRosterData::CreateFromRosterEntry(NpImpsCacheRosterData **outData,
						 const NpRosterFriendEntry *entry)
{
	void *mem = npImpsCacheMalloc(sizeof(NpImpsCacheRosterData));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	NpImpsCacheRosterData *obj = new (mem) NpImpsCacheRosterData();
	obj->AddRef();
	if (*outData != NULL)
		(*outData)->Release();
	*outData = obj;

	if (entry == NULL)
		return 0;

	obj->m_npId = entry->npId;

	int mappedRelation;
	switch (entry->subscription) {
	case 0:
		mappedRelation = NP_IMPS_CACHE_REL_NONE;
		break;
	case 1:
		mappedRelation = NP_IMPS_CACHE_REL_PENDING_OUT;
		break;
	case 2:
		mappedRelation = NP_IMPS_CACHE_REL_REQUESTED_IN;
		break;
	case 3:
		mappedRelation = NP_IMPS_CACHE_REL_ACCEPTED;
		break;
	default:
		return NP_IMPS_SVC_ERROR_INTERNAL;
	}

	paf::rtc::Tick tick = entry->updateDate.tick;
	obj->SetRelationAndDate(mappedRelation, &tick);
	obj->SetRequestDirection((entry->msgFlag != 0) ? 1 : 0);
	return 0;
}

/* 0x81307804 */
int NpImpsCacheRosterData::CreateFromPushItem(NpImpsCacheRosterData **outData,
					      const paf::rtc::Tick *timestamp,
					      const NpImpsCoreRosterItem *item)
{
	void *mem = npImpsCacheMalloc(sizeof(NpImpsCacheRosterData));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	NpImpsCacheRosterData *obj = new (mem) NpImpsCacheRosterData();
	obj->AddRef();
	if (*outData != NULL)
		(*outData)->Release();
	*outData = obj;

	if (item == NULL)
		return 0;

	obj->m_npId = item->npId;

	/* Note: SceNpImpsCore push encodes 1=requested-in, 2=pending-out (swapped vs roster) */
	int mappedRelation;
	switch (item->subscription) {
	case 0:
		mappedRelation = NP_IMPS_CACHE_REL_NONE;
		break;
	case 1:
		mappedRelation = NP_IMPS_CACHE_REL_REQUESTED_IN;
		break;
	case 2:
		mappedRelation = NP_IMPS_CACHE_REL_PENDING_OUT;
		break;
	case 3:
		mappedRelation = NP_IMPS_CACHE_REL_ACCEPTED;
		break;
	default:
		return NP_IMPS_SVC_ERROR_INTERNAL;
	}

	obj->SetRelationAndDate(mappedRelation, timestamp);
	return 0;
}

/* 0x813078CE */
int NpImpsCacheRosterData::CreateWithRelation(NpImpsCacheRosterData **outData,
					      const SceNpId *npId, int relation)
{
	void *mem = npImpsCacheMalloc(sizeof(NpImpsCacheRosterData));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	NpImpsCacheRosterData *obj = new (mem) NpImpsCacheRosterData();
	obj->AddRef();
	if (*outData != NULL)
		(*outData)->Release();
	*outData = obj;

	obj->m_npId = *npId;
	obj->m_relation = relation;
	obj->m_requestDirection = 0;
	return 0;
}

/* --------------------------------------------------------------------------
 * NpImpsCacheFriendEntryImpl (0x8130640C..0x813076D3)
 * -------------------------------------------------------------------------- */
NpImpsCacheFriendEntryImpl::NpImpsCacheFriendEntryImpl(const SceNpId *npId,
						       paf::thread::RMutex *dataMutex)
	: m_mutex(dataMutex),
	  m_rosterData(NULL),
	  m_presences(NULL),
	  m_presenceCount(0),
	  m_presenceCapacity(0),
	  m_isNew(0)
{
	m_npId = *npId;
}

/* 0x813064F8 */
NpImpsCacheFriendEntryImpl::~NpImpsCacheFriendEntryImpl()
{
	if (m_presences != NULL) {
		for (SceUInt32 pos = 0; pos < m_presenceCount; ++pos) {
			if (m_presences[pos].presence != NULL)
				m_presences[pos].presence->Release();
		}
		npImpsCacheFree(m_presences);
		m_presences = NULL;
	}
	m_presenceCount = 0;
	m_presenceCapacity = 0;

	if (m_rosterData != NULL) {
		m_rosterData->Release();
		m_rosterData = NULL;
	}
}

/* 0x81306480 */
int NpImpsCacheFriendEntryImpl::Create(NpImpsCacheFriendEntryImpl **outEntry,
				       NpImpsCacheRosterData **rosterData,
				       paf::thread::RMutex *dataMutex)
{
	void *mem = npImpsCacheMalloc(sizeof(NpImpsCacheFriendEntryImpl));
	if (mem == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	NpImpsCacheFriendEntryImpl *entry =
		new (mem) NpImpsCacheFriendEntryImpl((*rosterData)->GetNpId(), dataMutex);
	entry->AddRef();

	if (*outEntry != NULL)
		(*outEntry)->Release();
	*outEntry = entry;

	entry->m_rosterData = *rosterData;
	if (entry->m_rosterData != NULL)
		entry->m_rosterData->AddRef();
	return 0;
}

NpImpsSvcPresenceData *NpImpsCacheFriendEntryImpl::FindPlatformPresenceLocked(int platform) const
{
	for (SceUInt32 pos = 0; pos < m_presenceCount; ++pos) {
		if (m_presences[pos].platform == platform)
			return m_presences[pos].presence;
	}
	return NULL;
}

int NpImpsCacheFriendEntryImpl::SetPlatformPresenceLocked(int platform,
							  NpImpsSvcPresenceData *presence)
{
	for (SceUInt32 pos = 0; pos < m_presenceCount; ++pos) {
		if (m_presences[pos].platform == platform) {
			if (m_presences[pos].presence != presence) {
				if (m_presences[pos].presence != NULL)
					m_presences[pos].presence->Release();
				m_presences[pos].presence = presence;
				if (presence != NULL)
					presence->AddRef();
			}
			return 0;
		}
	}

	if (m_presenceCount >= m_presenceCapacity) {
		SceUInt32 newCap = (m_presenceCapacity == 0) ? 4 : (m_presenceCapacity * 2);
		PlatformPresenceSlot *grown = reinterpret_cast<PlatformPresenceSlot *>(
			npImpsCacheMalloc(sizeof(PlatformPresenceSlot) * newCap));
		if (grown == NULL)
			return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		if (m_presences != NULL) {
			sce_paf_memcpy(grown, m_presences,
				       sizeof(PlatformPresenceSlot) * m_presenceCount);
			npImpsCacheFree(m_presences);
		}
		m_presences = grown;
		m_presenceCapacity = newCap;
	}

	m_presences[m_presenceCount].platform = platform;
	m_presences[m_presenceCount].presence = presence;
	if (presence != NULL)
		presence->AddRef();
	++m_presenceCount;
	return 0;
}

/* 0x813065AC */
bool NpImpsCacheFriendEntryImpl::IsHighestEffectiveOnlineStatus(NpImpsSvcPresenceData *candidate,
								int platform)
{
	NpImpsSvcPresenceData *existing = FindPlatformPresenceLocked(platform);
	if (existing == NULL)
		return GetFriendContext() < candidate->GetEffectiveOnlineStatus();

	int candidateEff = candidate->GetEffectiveOnlineStatus();
	for (SceUInt32 pos = 0; pos < m_presenceCount; ++pos) {
		if (m_presences[pos].platform == platform)
			continue;
		if (m_presences[pos].presence != NULL &&
		    m_presences[pos].presence->GetEffectiveOnlineStatus() >= candidateEff)
			return false;
	}
	return true;
}

/* 0x8130654A */
void NpImpsCacheFriendEntryImpl::UpdateRosterData(int relation, const paf::rtc::Tick *date)
{
	if (m_rosterData != NULL)
		m_rosterData->SetRelationAndDate(relation, date);
}

/* 0x81306554 */
int NpImpsCacheFriendEntryImpl::UpdatePresenceDefault(NpImpsSvcPresenceData **newPresence,
						      NpImpsSvcPresenceDiffFlags *outDiff,
						      NpImpsSvcPresenceData **outPrev)
{
	NpImpsSvcPresenceData *held = (newPresence != NULL) ? *newPresence : NULL;
	if (held != NULL)
		held->AddRef();
	int ret = UpdatePresenceForPlatform(&held, outDiff, outPrev, NP_IMPS_SVC_PLATFORM_PSP2);
	if (held != NULL)
		held->Release();
	return ret;
}

/* 0x8130669A */
int NpImpsCacheFriendEntryImpl::UpdatePresenceForPlatform(NpImpsSvcPresenceData **newPresence,
							  NpImpsSvcPresenceDiffFlags *outDiff,
							  NpImpsSvcPresenceData **outPrev,
							  int platform)
{
	if (newPresence == NULL || *newPresence == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;

	NpImpsSvcPresenceData *incoming = *newPresence;
	incoming->FormatLastOnlineTickForLog();
	NpImpsSvcPresenceData *existing = FindPlatformPresenceLocked(platform);
	if (existing == NULL) {
		NpImpsSvcPresenceData emptyPresence;
		int ret = NpImpsSvcPresenceData::Clone(outPrev, &emptyPresence);
		if (ret < 0)
			return ret;

		sce_paf_memset(outDiff, 1, sizeof(*outDiff));
		if (!IsHighestEffectiveOnlineStatus(incoming, platform))
			outDiff->effectiveStatusChanged = 0;

		return SetPlatformPresenceLocked(platform, incoming);
	}

	int ret = NpImpsSvcPresenceData::Clone(outPrev, existing);
	if (ret < 0)
		return ret;

	ret = existing->Compare(incoming, outDiff);
	if (ret < 0)
		return ret;

	if (outDiff->effectiveStatusChanged != 0) {
		if (!IsHighestEffectiveOnlineStatus(incoming, platform))
			outDiff->effectiveStatusChanged = 0;
	}

	return SetPlatformPresenceLocked(platform, incoming);
}

/* 0x81306938 */
void NpImpsCacheFriendEntryImpl::GetNpId(SceNpId *outNpId)
{
	m_mutex->Lock();
	SceNpId copy = m_npId;
	m_mutex->Unlock();
	*outNpId = copy;
}

/* 0x813069EC */
int NpImpsCacheFriendEntryImpl::GetRelation()
{
	m_mutex->Lock();
	int rel = (m_rosterData != NULL) ? m_rosterData->GetRelation() : 0;
	m_mutex->Unlock();
	return rel;
}

/* 0x81306A0A */
void NpImpsCacheFriendEntryImpl::GetDate(paf::rtc::Tick *outTick)
{
	m_mutex->Lock();
	paf::rtc::Tick tick = (m_rosterData != NULL) ? *m_rosterData->GetDate() : 0;
	m_mutex->Unlock();
	*outTick = tick;
}

/* 0x81306A32 */
int NpImpsCacheFriendEntryImpl::GetRequestDirection()
{
	m_mutex->Lock();
	int dir = (m_rosterData != NULL) ? m_rosterData->GetRequestDirection() : -1;
	m_mutex->Unlock();
	return dir;
}

/* 0x81306A50 */
SceUInt8 NpImpsCacheFriendEntryImpl::GetIsNew()
{
	m_mutex->Lock();
	SceUInt8 val = m_isNew;
	m_mutex->Unlock();
	return val;
}

/* 0x81306A6A */
void NpImpsCacheFriendEntryImpl::SetIsNew(SceUInt8 isNew)
{
	m_mutex->Lock();
	m_isNew = isNew;
	m_mutex->Unlock();
}

/* 0x81306A84 */
int NpImpsCacheFriendEntryImpl::GetFriendContext()
{
	m_mutex->Lock();
	int bestStatus = 0;
	for (SceUInt32 pos = 0; pos < m_presenceCount; ++pos) {
		if (m_presences[pos].presence != NULL) {
			int eff = m_presences[pos].presence->GetEffectiveOnlineStatus();
			if (bestStatus < eff)
				bestStatus = eff;
		}
	}
	m_mutex->Unlock();
	return bestStatus;
}

/* 0x81306B16 */
int NpImpsCacheFriendEntryImpl::GetOnlineStatus(int platform)
{
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	int status = (presence != NULL) ? presence->GetOnlineStatus() : 0;
	m_mutex->Unlock();
	return status;
}

/* 0x81306B74 */
int NpImpsCacheFriendEntryImpl::GetPlatform(int platform)
{
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	int plat = (presence != NULL) ? presence->GetPlatform() : 0;
	m_mutex->Unlock();
	return plat;
}

/* 0x81306BD2 */
int NpImpsCacheFriendEntryImpl::CopyComment(char **outBuf, void *(*allocFn)(SceSize), int platform)
{
	m_mutex->Lock();
	if (outBuf == NULL || allocFn == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	}

	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	int ret;
	if (presence == NULL || presence->GetComment() == NULL) {
		char *empty = reinterpret_cast<char *>(allocFn(1));
		*outBuf = empty;
		if (empty != NULL) {
			empty[0] = '\0';
			ret = 0;
		} else {
			ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		}
	} else {
		const char *comment = presence->GetComment();
		SceSize len = sce_paf_strlen(comment);
		char *copy = reinterpret_cast<char *>(allocFn(len + 1));
		*outBuf = copy;
		if (copy != NULL) {
			sce_paf_memset(copy, 0, len + 1);
			sce_paf_strncpy(copy, comment, len);
			ret = 0;
		} else {
			ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		}
	}
	m_mutex->Unlock();
	return ret;
}

/* 0x81306CAC */
int NpImpsCacheFriendEntryImpl::GetTitlePlatform(int platform)
{
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	int titlePlat = (presence != NULL) ? presence->GetTitlePlatform() : 0;
	m_mutex->Unlock();
	return titlePlat;
}

/* 0x81306D0A */
void NpImpsCacheFriendEntryImpl::GetLastOnlineTick(paf::rtc::Tick *outTick, int platform)
{
	paf::rtc::Tick tick = 0;
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL)
		tick = *presence->GetLastOnlineTick();
	m_mutex->Unlock();
	*outTick = tick;
}

/* 0x81306D9E */
void NpImpsCacheFriendEntryImpl::GetTitleId(SceNpCommunicationId *outId, int platform)
{
	SceNpCommunicationId result;
	sce_paf_memset(&result, 0, sizeof(result));
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL)
		result = *presence->GetTitleId();
	m_mutex->Unlock();
	*outId = result;
}

/* 0x81306EF4 */
void NpImpsCacheFriendEntryImpl::GetTitleIdStr(char *outBuf, int platform)
{
	sce_paf_memset(outBuf, 0, 128);
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL && presence->GetTitleIdStr() != NULL)
		sce_paf_strncpy(outBuf, presence->GetTitleIdStr(), 128);
	m_mutex->Unlock();
}

/* 0x81306F6C */
void NpImpsCacheFriendEntryImpl::GetTitleName(char *outBuf, int platform)
{
	sce_paf_memset(outBuf, 0, 128);
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL && presence->GetTitleName() != NULL)
		sce_paf_strncpy(outBuf, presence->GetTitleName(), 128);
	m_mutex->Unlock();
}

/* 0x81306FE4 */
SceUInt8 NpImpsCacheFriendEntryImpl::IsSystemTitle(int platform)
{
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	SceUInt8 isSys = (presence != NULL) ? presence->IsSystemTitle() : 0;
	m_mutex->Unlock();
	return isSys;
}

/* 0x81307042 */
void NpImpsCacheFriendEntryImpl::GetJoinableCommId(SceNpCommunicationId *outId, int platform)
{
	SceNpCommunicationId result;
	sce_paf_memset(&result, 0, sizeof(result));
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL)
		result = *presence->GetJoinableCommId();
	m_mutex->Unlock();
	*outId = result;
}

/* 0x81307138 */
void NpImpsCacheFriendEntryImpl::GetContentId(char *outBuf, int platform)
{
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL && outBuf != NULL && presence->GetContentId() != NULL)
		sce_paf_strncpy(outBuf, presence->GetContentId(), 48);
	m_mutex->Unlock();
}

/* 0x813071B2 */
int NpImpsCacheFriendEntryImpl::GetGamePresenceType(int platform)
{
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	int type = (presence != NULL) ? presence->GetGamePresenceType() : 0;
	m_mutex->Unlock();
	return type;
}

/* 0x81307210 */
void NpImpsCacheFriendEntryImpl::GetCommId(SceNpCommunicationId *outId, int platform)
{
	SceNpCommunicationId result;
	sce_paf_memset(&result, 0, sizeof(result));
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL)
		result = *presence->GetCommId();
	m_mutex->Unlock();
	*outId = result;
}

/* 0x81307306 */
void NpImpsCacheFriendEntryImpl::GetGameStatus(char *outBuf, int platform)
{
	sce_paf_memset(outBuf, 0, 201);
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL && presence->GetGameStatus() != NULL)
		sce_paf_strncpy(outBuf, presence->GetGameStatus(), 201);
	m_mutex->Unlock();
}

/* 0x8130737E */
SceSize NpImpsCacheFriendEntryImpl::GetGameDataSize(int platform)
{
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	SceSize len = (presence != NULL) ? presence->GetGameDataSize() : 0;
	m_mutex->Unlock();
	return len;
}

/* 0x813073DC */
int NpImpsCacheFriendEntryImpl::CopyGameData(void *dst, SceSize maxLen, int platform)
{
	int ret = 0;
	sce_paf_memset(dst, 0, maxLen);
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL)
		ret = presence->CopyGameData(dst, maxLen);
	m_mutex->Unlock();
	return ret;
}

/* 0x8130745C */
int NpImpsCacheFriendEntryImpl::GetPartyPresenceType(int platform)
{
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	int type = (presence != NULL) ? presence->GetPartyPresenceType() : 0;
	m_mutex->Unlock();
	return type;
}

/* 0x813074BA */
void NpImpsCacheFriendEntryImpl::GetPartyTitle(char *outBuf, int platform)
{
	sce_paf_strncpy(outBuf, "", 128);
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL && presence->GetPartyTitle() != NULL)
		sce_paf_strncpy(outBuf, presence->GetPartyTitle(), 128);
	m_mutex->Unlock();
}

/* 0x81307544 */
void NpImpsCacheFriendEntryImpl::GetPartyStatus(char *outBuf, int platform)
{
	sce_paf_strncpy(outBuf, "", 192);
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL && presence->GetPartyStatus() != NULL)
		sce_paf_strncpy(outBuf, presence->GetPartyStatus(), 192);
	m_mutex->Unlock();
}

/* 0x813075CE */
SceSize NpImpsCacheFriendEntryImpl::GetPartyDataSize(int platform)
{
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	SceSize len = (presence != NULL) ? presence->GetPartyDataSize() : 0;
	m_mutex->Unlock();
	return len;
}

/* 0x8130762C */
int NpImpsCacheFriendEntryImpl::CopyPartyData(void *dst, SceSize maxLen, int platform)
{
	int ret = 0;
	m_mutex->Lock();
	NpImpsSvcPresenceData *presence = FindPlatformPresenceLocked(platform);
	if (presence != NULL)
		ret = presence->CopyPartyData(dst, maxLen);
	m_mutex->Unlock();
	return ret;
}

/* 0x813076A4 */
int NpImpsCacheFriendEntryImpl::FreeAllocatedString(void *ptr, void (*freeFn)(void *))
{
	if (ptr == NULL || freeFn == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	freeFn(ptr);
	return 0;
}

/* --------------------------------------------------------------------------
 * NpImpsCacheFriendStoreImpl (0x81301C5C..0x813043B3)
 * -------------------------------------------------------------------------- */
/* 0x81301C5C */
NpImpsCacheFriendStoreImpl::NpImpsCacheFriendStoreImpl(NpImpsCacheStoreSink *sink,
						       paf::thread::RMutex *dataMutex)
	: m_sink(sink),
	  m_mutex(dataMutex),
	  m_entries(NULL),
	  m_count(0),
	  m_capacity(0),
	  m_loaded(0)
{
}

NpImpsCacheFriendStoreImpl::~NpImpsCacheFriendStoreImpl()
{
	Clear();
	if (m_entries != NULL) {
		npImpsCacheFree(m_entries);
		m_entries = NULL;
	}
	m_capacity = 0;
}

/* 0x81301CA0, 0x81301CAA */
void NpImpsCacheFriendStoreImpl::Lock()
{
	m_mutex->Lock();
}

void NpImpsCacheFriendStoreImpl::Unlock()
{
	m_mutex->Unlock();
}

SceUInt32 NpImpsCacheFriendStoreImpl::LowerBoundLocked(const SceNpId *npId) const
{
	SceUInt32 low = 0;
	SceUInt32 high = m_count;
	while (low < high) {
		SceUInt32 mid = low + ((high - low) >> 1);
		if (npImpsCacheCompareNpId(&m_entries[mid].npId, npId) < 0)
			low = mid + 1;
		else
			high = mid;
	}
	return low;
}

NpImpsCacheFriendEntryImpl *NpImpsCacheFriendStoreImpl::FindLocked(const SceNpId *npId) const
{
	if (npId == NULL || m_count == 0)
		return NULL;
	SceUInt32 pos = LowerBoundLocked(npId);
	if (pos < m_count && npImpsCacheCompareNpId(&m_entries[pos].npId, npId) == 0)
		return m_entries[pos].entry;
	return NULL;
}

int NpImpsCacheFriendStoreImpl::InsertLocked(const SceNpId *npId,
					     NpImpsCacheFriendEntryImpl *entry)
{
	SceUInt32 pos = LowerBoundLocked(npId);
	if (pos < m_count && npImpsCacheCompareNpId(&m_entries[pos].npId, npId) == 0)
		return 0;

	if (m_count >= m_capacity) {
		SceUInt32 newCap = (m_capacity == 0) ? 16 : (m_capacity * 2);
		FriendMapSlot *grown = reinterpret_cast<FriendMapSlot *>(
			npImpsCacheMalloc(sizeof(FriendMapSlot) * newCap));
		if (grown == NULL)
			return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		if (m_entries != NULL) {
			sce_paf_memcpy(grown, m_entries, sizeof(FriendMapSlot) * m_count);
			npImpsCacheFree(m_entries);
		}
		m_entries = grown;
		m_capacity = newCap;
	}

	for (SceUInt32 move = m_count; move > pos; --move)
		m_entries[move] = m_entries[move - 1];
	m_entries[pos].npId = *npId;
	m_entries[pos].entry = entry;
	if (entry != NULL)
		entry->AddRef();
	++m_count;
	return 1;
}

bool NpImpsCacheFriendStoreImpl::EraseLocked(const SceNpId *npId)
{
	if (npId == NULL || m_count == 0)
		return false;
	SceUInt32 pos = LowerBoundLocked(npId);
	if (pos >= m_count || npImpsCacheCompareNpId(&m_entries[pos].npId, npId) != 0)
		return false;

	if (m_entries[pos].entry != NULL)
		m_entries[pos].entry->Release();
	for (SceUInt32 move = pos + 1; move < m_count; ++move)
		m_entries[move - 1] = m_entries[move];
	--m_count;
	return true;
}

/* 0x81302FF8 */
SceUInt32 NpImpsCacheFriendStoreImpl::GetCount()
{
	m_mutex->Lock();
	SceUInt32 count = (m_loaded != 0) ? m_count : 0;
	m_mutex->Unlock();
	return count;
}

/* 0x8130353A */
int NpImpsCacheFriendStoreImpl::CopyHandles(NpImpsCacheFriendPtr *outHandles, SceUInt32 count)
{
	m_mutex->Lock();
	SceUInt32 limit = (count < m_count) ? count : m_count;
	for (SceUInt32 pos = 0; pos < limit; ++pos) {
		NpImpsCacheFriendEntry *entry = m_entries[pos].entry;
		if (outHandles[pos] != entry) {
			if (outHandles[pos] != NULL)
				outHandles[pos]->Release();
			outHandles[pos] = entry;
			if (entry != NULL)
				entry->AddRef();
		}
	}
	m_mutex->Unlock();
	return 0;
}

SceUInt32 NpImpsCacheFriendStoreImpl::CountByRelationLocked(int relation) const
{
	if (m_loaded == 0)
		return 0;
	SceUInt32 total = 0;
	for (SceUInt32 pos = 0; pos < m_count; ++pos) {
		if (m_entries[pos].entry->GetRelation() == relation)
			++total;
	}
	return total;
}

void NpImpsCacheFriendStoreImpl::CopyByRelationLocked(SceUInt32 startIndex, SceNpId *outEntries,
						      SceUInt32 maxCount, SceUInt32 *outCount,
						      SceUInt8 *outLoaded, int relation)
{
	SceUInt32 copied = 0;
	if (m_loaded != 0) {
		SceUInt32 seen = 0;
		for (SceUInt32 pos = 0; pos < m_count; ++pos) {
			if (m_entries[pos].entry->GetRelation() != relation)
				continue;
			if (seen >= startIndex) {
				if (copied >= maxCount)
					break;
				m_entries[pos].entry->GetNpId(&outEntries[copied]);
				++copied;
			}
			++seen;
		}
		if (outCount != NULL)
			*outCount = copied;
	}
	if (outLoaded != NULL)
		*outLoaded = m_loaded;
}

/* 0x81303010 */
SceUInt32 NpImpsCacheFriendStoreImpl::GetAcceptedCount(SceUInt8 *outLoaded)
{
	m_mutex->Lock();
	SceUInt32 count = CountByRelationLocked(NP_IMPS_CACHE_REL_ACCEPTED);
	if (outLoaded != NULL)
		*outLoaded = m_loaded;
	m_mutex->Unlock();
	return count;
}

/* 0x813030AE */
void NpImpsCacheFriendStoreImpl::CopyAcceptedNpIds(SceUInt32 startIndex, SceNpId *outEntries,
						   SceUInt32 maxCount, SceUInt32 *outCount,
						   SceUInt8 *outLoaded)
{
	m_mutex->Lock();
	CopyByRelationLocked(startIndex, outEntries, maxCount, outCount, outLoaded,
			     NP_IMPS_CACHE_REL_ACCEPTED);
	m_mutex->Unlock();
}

/* 0x813031D8 */
SceUInt32 NpImpsCacheFriendStoreImpl::CountByRelation(SceUInt8 *outLoaded, int relation)
{
	m_mutex->Lock();
	SceUInt32 total = 0;
	if (m_loaded != 0) {
		for (SceUInt32 pos = 0; pos < m_count; ++pos) {
			if (m_entries[pos].entry->GetRelation() == relation)
				++total;
		}
	}
	if (outLoaded != NULL)
		*outLoaded = m_loaded;
	m_mutex->Unlock();
	return total;
}

/* 0x81303282 */
SceUInt32 NpImpsCacheFriendStoreImpl::GetPendingCount(SceUInt8 *outLoaded)
{
	return CountByRelation(outLoaded, NP_IMPS_CACHE_REL_PENDING_OUT);
}

/* 0x813033FC */
void NpImpsCacheFriendStoreImpl::CopyPendingNpIds(SceUInt32 startIndex, SceNpId *outEntries,
						  SceUInt32 maxCount, SceUInt32 *outCount,
						  SceUInt8 *outLoaded)
{
	m_mutex->Lock();
	CopyByRelationLocked(startIndex, outEntries, maxCount, outCount, outLoaded,
			     NP_IMPS_CACHE_REL_PENDING_OUT);
	m_mutex->Unlock();
}

/* 0x81303414 */
SceUInt32 NpImpsCacheFriendStoreImpl::GetRequestedCount(SceUInt8 *outLoaded)
{
	return CountByRelation(outLoaded, NP_IMPS_CACHE_REL_REQUESTED_IN);
}

/* 0x8130341E */
void NpImpsCacheFriendStoreImpl::CopyRequestedNpIds(SceUInt32 startIndex, SceNpId *outEntries,
						    SceUInt32 maxCount, SceUInt32 *outCount,
						    SceUInt8 *outLoaded)
{
	m_mutex->Lock();
	CopyByRelationLocked(startIndex, outEntries, maxCount, outCount, outLoaded,
			     NP_IMPS_CACHE_REL_REQUESTED_IN);
	m_mutex->Unlock();
}

/* 0x813035F2 */
int NpImpsCacheFriendStoreImpl::CopyAllNpIds(SceUInt32 startIndex, SceNpId *outEntries,
					     SceUInt32 maxCount, SceUInt32 *outCount)
{
	m_mutex->Lock();
	*outCount = 0;
	if (outEntries != NULL && maxCount != 0 && startIndex < m_count) {
		SceUInt32 copied = 0;
		for (SceUInt32 pos = startIndex; pos < m_count && copied < maxCount; ++pos) {
			outEntries[copied] = m_entries[pos].npId;
			++copied;
		}
		*outCount = copied;
	}
	m_mutex->Unlock();
	return 0;
}

/* 0x81303754 */
int NpImpsCacheFriendStoreImpl::GetRelation(const SceNpId *npId, int *outRelation)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	entry->AddRef();
	m_mutex->Unlock();
	*outRelation = entry->GetRelation();
	entry->Release();
	return 0;
}

/* 0x813037E2 */
int NpImpsCacheFriendStoreImpl::GetDate(const SceNpId *npId, paf::rtc::Tick *outTick)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	entry->GetDate(outTick);
	m_mutex->Unlock();
	return 0;
}

/* 0x8130385C */
int NpImpsCacheFriendStoreImpl::GetFriendContext(const SceNpId *npId, int *outContext)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	entry->AddRef();
	m_mutex->Unlock();
	*outContext = entry->GetFriendContext();
	entry->Release();
	return 0;
}

/* 0x813038DA */
int NpImpsCacheFriendStoreImpl::GetOnlineStatus(const SceNpId *npId, int *outStatus)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	entry->AddRef();
	m_mutex->Unlock();
	*outStatus = entry->GetOnlineStatus(NP_IMPS_SVC_PLATFORM_PSP2);
	entry->Release();
	return 0;
}

/* 0x8130395A */
int NpImpsCacheFriendStoreImpl::GetTitlePlatform(const SceNpId *npId, int *outPlatform)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	*outPlatform = entry->GetTitlePlatform(NP_IMPS_SVC_PLATFORM_PSP2);
	m_mutex->Unlock();
	return 0;
}

/* 0x813039CE */
int NpImpsCacheFriendStoreImpl::GetLastOnlineTick(const SceNpId *npId, paf::rtc::Tick *outTick,
						  bool *outHasTick)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	paf::rtc::Tick tick;
	entry->GetLastOnlineTick(&tick, NP_IMPS_SVC_PLATFORM_PSP2);
	*outTick = tick;
	if (outHasTick != NULL)
		*outHasTick = (tick != 0);
	m_mutex->Unlock();
	return 0;
}

/* 0x81303A68, 0x81303A76 */
int NpImpsCacheFriendStoreImpl::GetTitleId(const SceNpId *npId, SceNpCommunicationId *outId)
{
	return GetTitleIdForPlatform(npId, outId, NP_IMPS_SVC_PLATFORM_PSP2);
}

int NpImpsCacheFriendStoreImpl::GetTitleName(const SceNpId *npId, char *outTitleName)
{
	return GetTitleNameForPlatform(npId, outTitleName, NP_IMPS_SVC_PLATFORM_PSP2);
}

/* 0x81303A84 */
int NpImpsCacheFriendStoreImpl::IsSystemTitle(const SceNpId *npId, SceUInt8 *outIsSystem)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	*outIsSystem = entry->IsSystemTitle(NP_IMPS_SVC_PLATFORM_PSP2);
	m_mutex->Unlock();
	return 0;
}

/* 0x81303AF8 */
int NpImpsCacheFriendStoreImpl::GetContentId(const SceNpId *npId, char *outContentId)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	entry->GetContentId(outContentId, NP_IMPS_SVC_PLATFORM_PSP2);
	m_mutex->Unlock();
	return 0;
}

/* 0x81303B6C..0x81303BA4 */
int NpImpsCacheFriendStoreImpl::GetGamePresenceType(const SceNpId *npId, int *outType)
{
	return GetGamePresenceTypeForPlatform(npId, outType, NP_IMPS_SVC_PLATFORM_PSP2);
}

int NpImpsCacheFriendStoreImpl::GetCommId(const SceNpId *npId, SceNpCommunicationId *outId)
{
	return GetCommIdForPlatform(npId, outId, NP_IMPS_SVC_PLATFORM_PSP2);
}

int NpImpsCacheFriendStoreImpl::GetGameStatus(const SceNpId *npId, char *outStatus)
{
	return GetGameStatusForPlatform(npId, outStatus, NP_IMPS_SVC_PLATFORM_PSP2);
}

int NpImpsCacheFriendStoreImpl::GetGameData(const SceNpId *npId, void *outData)
{
	return GetGameDataForPlatform(npId, outData, NP_IMPS_SVC_PLATFORM_PSP2);
}

int NpImpsCacheFriendStoreImpl::GetGameDataSize(const SceNpId *npId, SceSize *outSize)
{
	return GetGameDataSizeForPlatform(npId, outSize, NP_IMPS_SVC_PLATFORM_PSP2);
}

/* 0x81303BB2..0x81303E0D */
int NpImpsCacheFriendStoreImpl::GetPartyPresenceType(const SceNpId *npId, int *outType)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	*outType = entry->GetPartyPresenceType(NP_IMPS_SVC_PLATFORM_PSP2);
	m_mutex->Unlock();
	return 0;
}

int NpImpsCacheFriendStoreImpl::GetPartyTitle(const SceNpId *npId, char *outTitle)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	entry->GetPartyTitle(outTitle, NP_IMPS_SVC_PLATFORM_PSP2);
	m_mutex->Unlock();
	return 0;
}

int NpImpsCacheFriendStoreImpl::GetPartyStatus(const SceNpId *npId, char *outStatus)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	entry->GetPartyStatus(outStatus, NP_IMPS_SVC_PLATFORM_PSP2);
	m_mutex->Unlock();
	return 0;
}

int NpImpsCacheFriendStoreImpl::GetPartyData(const SceNpId *npId, void *outData)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	int ret = entry->CopyPartyData(outData, 128, NP_IMPS_SVC_PLATFORM_PSP2);
	m_mutex->Unlock();
	return (ret >= 0) ? 0 : ret;
}

int NpImpsCacheFriendStoreImpl::GetPartyDataSize(const SceNpId *npId, SceSize *outSize)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	*outSize = entry->GetPartyDataSize(NP_IMPS_SVC_PLATFORM_PSP2);
	m_mutex->Unlock();
	return 0;
}

/* 0x81303E0E */
bool NpImpsCacheFriendStoreImpl::Contains(const SceNpId *npId)
{
	m_mutex->Lock();
	bool found = (FindLocked(npId) != NULL);
	m_mutex->Unlock();
	return found;
}

/* 0x81303E64..0x8130424F */
int NpImpsCacheFriendStoreImpl::GetTitleIdForPlatform(const SceNpId *npId,
						      SceNpCommunicationId *outId, int platform)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	entry->GetTitleId(outId, platform);
	m_mutex->Unlock();
	return 0;
}

int NpImpsCacheFriendStoreImpl::GetTitleNameForPlatform(const SceNpId *npId, char *outTitleName,
							int platform)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	char nameBuf[128];
	entry->GetTitleName(nameBuf, platform);
	sce_paf_strncpy(outTitleName, nameBuf, sizeof(nameBuf));
	m_mutex->Unlock();
	return 0;
}

int NpImpsCacheFriendStoreImpl::GetGamePresenceTypeForPlatform(const SceNpId *npId, int *outType,
							       int platform)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	*outType = entry->GetGamePresenceType(platform);
	m_mutex->Unlock();
	return 0;
}

int NpImpsCacheFriendStoreImpl::GetCommIdForPlatform(const SceNpId *npId,
						     SceNpCommunicationId *outId, int platform)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	entry->GetCommId(outId, platform);
	m_mutex->Unlock();
	return 0;
}

int NpImpsCacheFriendStoreImpl::GetGameStatusForPlatform(const SceNpId *npId, char *outStatus,
							 int platform)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	char statusBuf[204];
	entry->GetGameStatus(statusBuf, platform);
	sce_paf_strncpy(outStatus, statusBuf, 192);
	m_mutex->Unlock();
	return 0;
}

int NpImpsCacheFriendStoreImpl::GetGameDataForPlatform(const SceNpId *npId, void *outData,
						       int platform)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	int ret = entry->CopyGameData(outData, 128, platform);
	m_mutex->Unlock();
	return (ret >= 0) ? 0 : ret;
}

int NpImpsCacheFriendStoreImpl::GetGameDataSizeForPlatform(const SceNpId *npId, SceSize *outSize,
							   int platform)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	*outSize = entry->GetGameDataSize(platform);
	m_mutex->Unlock();
	return 0;
}

/* 0x81303436 */
int NpImpsCacheFriendStoreImpl::GetNpIdAt(SceUInt32 index, SceNpId *outNpId)
{
	m_mutex->Lock();
	if (index >= m_count) {
		m_mutex->Unlock();
		return NP_IMPS_CACHE_ERROR_INDEX_OUT_OF_RANGE;
	}
	m_entries[index].entry->GetNpId(outNpId);
	m_mutex->Unlock();
	return 0;
}

/* 0x81301CB4 */
int NpImpsCacheFriendStoreImpl::AddEntryLocked(
	NpImpsCacheRosterData **rosterData,
	NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outAdded)
{
	NpImpsCacheFriendEntryImpl *created = NULL;
	NpImpsCacheRosterData *heldRoster = *rosterData;
	if (heldRoster != NULL)
		heldRoster->AddRef();
	int ret = NpImpsCacheFriendEntryImpl::Create(&created, &heldRoster, m_mutex);
	if (heldRoster != NULL)
		heldRoster->Release();

	if (ret >= 0) {
		if (m_count < 100u) {
			SceUInt32 prevCount = m_count;
			const SceNpId *npId = (*rosterData)->GetNpId();
			int insRet = InsertLocked(npId, created);
			if (insRet < 0 || prevCount == m_count) {
				ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			} else {
				created->SetIsNew(1);
			}
		}
		if (ret >= 0 && outAdded != NULL) {
			SceUInt32 prevOut = outAdded->Size();
			outAdded->PushBack(created);
			if (prevOut == outAdded->Size())
				ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		}
	}

	if (created != NULL)
		created->Release();
	return ret;
}

/* 0x81301DD8 */
int NpImpsCacheFriendStoreImpl::UpdateEntryLocked(
	SceUInt32 slotIndex, NpImpsCacheRosterData **rosterData,
	NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outUpdated)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *existing = m_entries[slotIndex].entry;
	SceNpId existingId;
	existing->GetNpId(&existingId);
	const SceNpId *incomingId = (*rosterData)->GetNpId();

	int ret = 0;
	if (sce_paf_memcmp(&existingId, incomingId, 20u) == 0 &&
	    existing->GetRelation() != (*rosterData)->GetRelation()) {
		NpImpsCacheFriendEntryImpl *replacement = NULL;
		NpImpsCacheRosterData *heldRoster = *rosterData;
		if (heldRoster != NULL)
			heldRoster->AddRef();
		ret = NpImpsCacheFriendEntryImpl::Create(&replacement, &heldRoster, m_mutex);
		if (heldRoster != NULL)
			heldRoster->Release();

		if (ret >= 0) {
			replacement->SetIsNew(1);
			EraseLocked(&existingId);
			SceUInt32 prevCount = m_count;
			InsertLocked(incomingId, replacement);
			if (prevCount == m_count) {
				ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			} else if (outUpdated != NULL) {
				SceUInt32 prevOut = outUpdated->Size();
				outUpdated->PushBack(replacement);
				if (prevOut == outUpdated->Size())
					ret = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			}
		}
		if (replacement != NULL)
			replacement->Release();
	}

	m_mutex->Unlock();
	return ret;
}

/* 0x8130201E */
int NpImpsCacheFriendStoreImpl::ApplyAddedAndUpdated(
	const NpImpsCacheRefVector<NpImpsCacheRosterData> *fetched,
	NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outAdded,
	NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outUpdated)
{
	m_mutex->Lock();
	int ret = 0;
	for (SceUInt32 pos = 0; pos < fetched->Size(); ++pos) {
		NpImpsCacheRosterData *item = fetched->At(pos);
		if (item == NULL)
			continue;
		item->AddRef();
		const SceNpId *npId = item->GetNpId();
		SceUInt32 slotIdx = LowerBoundLocked(npId);
		bool exists = (slotIdx < m_count &&
			       npImpsCacheCompareNpId(&m_entries[slotIdx].npId, npId) == 0);
		item->Release();

		if (exists) {
			slotIdx = LowerBoundLocked(npId);
			if (slotIdx >= m_count ||
			    npImpsCacheCompareNpId(&m_entries[slotIdx].npId, npId) != 0) {
				m_mutex->Unlock();
				return NP_IMPS_SVC_ERROR_INTERNAL;
			}
			NpImpsCacheRosterData *held = item;
			held->AddRef();
			ret = UpdateEntryLocked(slotIdx, &held, outUpdated);
			held->Release();
			if (ret < 0)
				break;
		} else {
			NpImpsCacheRosterData *held = item;
			held->AddRef();
			ret = AddEntryLocked(&held, outAdded);
			held->Release();
			if (ret < 0)
				break;
		}
	}
	m_mutex->Unlock();
	return ret;
}

/* 0x8130215A */
int NpImpsCacheFriendStoreImpl::CollectAndRemoveMissing(
	const NpImpsCacheRefVector<NpImpsCacheRosterData> *fetched,
	NpImpsCacheNpIdSet *outRemovedAll,
	NpImpsCacheNpIdSet *outRemovedAccepted,
	NpImpsCacheNpIdSet *outRemovedPendingOrReq)
{
	m_mutex->Lock();
	for (SceUInt32 pos = 0; pos < m_count; ++pos) {
		const SceNpId *npId = &m_entries[pos].npId;
		if (!npImpsCacheRosterListContains(npId, fetched)) {
			if (outRemovedAll != NULL && outRemovedAll->Insert(npId) <= 0) {
				m_mutex->Unlock();
				return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			}
			int rel = m_entries[pos].entry->GetRelation();
			if (rel == NP_IMPS_CACHE_REL_ACCEPTED) {
				if (outRemovedAccepted != NULL &&
				    outRemovedAccepted->Insert(npId) <= 0) {
					m_mutex->Unlock();
					return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
				}
			} else if (rel == NP_IMPS_CACHE_REL_PENDING_OUT ||
				   rel == NP_IMPS_CACHE_REL_REQUESTED_IN) {
				if (outRemovedPendingOrReq != NULL &&
				    outRemovedPendingOrReq->Insert(npId) <= 0) {
					m_mutex->Unlock();
					return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
				}
			}
		}
	}

	if (outRemovedAll != NULL) {
		for (SceUInt32 pos = 0; pos < outRemovedAll->Size(); ++pos)
			EraseLocked(outRemovedAll->At(pos));
	}

	m_mutex->Unlock();
	return 0;
}

/* 0x8130232A: diff and apply full roster friend list */
int NpImpsCacheFriendStoreImpl::ApplyFullList(
	const NpImpsCacheRefVector<NpImpsCacheRosterData> *fetched,
	NpImpsCacheNpIdSet *outRemovedAll,
	NpImpsCacheNpIdSet *outRemovedAccepted,
	NpImpsCacheNpIdSet *outRemovedPendingOrReq,
	NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outAdded,
	NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outUpdated,
	bool *outFirstLoad)
{
	m_mutex->Lock();
	m_mutex->Lock();
	SceUInt8 wasLoaded = m_loaded;
	m_mutex->Unlock();

	int ret = CollectAndRemoveMissing(fetched, outRemovedAll, outRemovedAccepted,
					  outRemovedPendingOrReq);
	if (ret >= 0) {
		ret = ApplyAddedAndUpdated(fetched, outAdded, outUpdated);
		if (ret >= 0) {
			m_loaded = 1;
			if (outFirstLoad != NULL)
				*outFirstLoad = (wasLoaded == 0);
		}
	}

	m_mutex->Unlock();
	return ret;
}

/* 0x813023B8 */
void NpImpsCacheFriendStoreImpl::CollectPendingAndRequested(const NpImpsCacheNpIdSet *npIds,
							    NpImpsCacheNpIdSet *outSet)
{
	for (SceUInt32 pos = 0; pos < npIds->Size(); ++pos) {
		const SceNpId *npId = npIds->At(pos);
		NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
		if (entry != NULL) {
			int rel = entry->GetRelation();
			if (rel == NP_IMPS_CACHE_REL_PENDING_OUT ||
			    rel == NP_IMPS_CACHE_REL_REQUESTED_IN) {
				outSet->Insert(npId);
			}
		}
	}
}

/* 0x8130247C */
void NpImpsCacheFriendStoreImpl::CollectAccepted(const NpImpsCacheNpIdSet *npIds,
						 NpImpsCacheNpIdSet *outSet)
{
	for (SceUInt32 pos = 0; pos < npIds->Size(); ++pos) {
		const SceNpId *npId = npIds->At(pos);
		NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
		if (entry != NULL && entry->GetRelation() == NP_IMPS_CACHE_REL_ACCEPTED)
			outSet->Insert(npId);
	}
}

/* 0x813028CE: incremental add/update from push or local action */
int NpImpsCacheFriendStoreImpl::AddOrUpdateList(
	const NpImpsCacheRefVector<NpImpsCacheRosterData> *items)
{
	m_mutex->Lock();
	NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> added;
	NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> updated;
	int ret = 0;

	for (SceUInt32 pos = 0; pos < items->Size(); ++pos) {
		NpImpsCacheRosterData *rosterItem = items->At(pos);
		if (rosterItem == NULL)
			continue;
		const SceNpId *npId = rosterItem->GetNpId();
		SceUInt32 slotIdx = LowerBoundLocked(npId);
		bool exists = (slotIdx < m_count &&
			       npImpsCacheCompareNpId(&m_entries[slotIdx].npId, npId) == 0);
		NpImpsCacheRosterData *held = rosterItem;
		held->AddRef();
		if (exists)
			ret = UpdateEntryLocked(slotIdx, &held, &updated);
		else
			ret = AddEntryLocked(&held, &added);
		held->Release();
		if (ret < 0)
			break;
	}

	if (ret >= 0 && m_sink != NULL) {
		if (updated.Size() != 0)
			ret = m_sink->NotifyFriendListUpdated(&updated);
		if (ret >= 0 && added.Size() != 0)
			ret = m_sink->NotifyFriendListAdded(&added);
	}

	m_mutex->Unlock();
	return ret;
}

/* 0x8130253C: incremental remove */
int NpImpsCacheFriendStoreImpl::RemoveList(const NpImpsCacheNpIdSet *npIds)
{
	m_mutex->Lock();
	NpImpsCacheNpIdSet removedAccepted;
	NpImpsCacheNpIdSet removedPendingOrReq;

	CollectAccepted(npIds, &removedAccepted);
	CollectPendingAndRequested(npIds, &removedPendingOrReq);

	for (SceUInt32 pos = 0; pos < npIds->Size(); ++pos)
		EraseLocked(npIds->At(pos));

	int ret = 0;
	if (m_sink != NULL) {
		if (removedAccepted.Size() > 0)
			ret = m_sink->NotifyFriendsRemoved(&removedAccepted);
		if (ret >= 0 && removedPendingOrReq.Size() > 0)
			ret = m_sink->NotifyFriendRequestsRemoved(&removedPendingOrReq);
	}

	m_mutex->Unlock();
	return ret;
}

/* 0x81302EC6: fetch full friend list from roster and apply */
int NpImpsCacheFriendStoreImpl::FetchAndApply(
	NpImpsCacheNpIdSet *outRemovedAll,
	NpImpsCacheNpIdSet *outRemovedAccepted,
	NpImpsCacheNpIdSet *outRemovedPendingOrReq,
	NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outAdded,
	NpImpsCacheRefVector<NpImpsCacheFriendEntryImpl> *outUpdated,
	bool *outFirstLoad)
{
	NpImpsCacheRefVector<NpImpsCacheRosterData> fetched;
	int ret = npImpsCacheFetchFriendListFromRoster(&fetched);
	if (ret >= 0) {
		Lock();
		m_mutex->Lock();
		m_mutex->Lock();
		SceUInt8 wasLoaded = m_loaded;
		m_mutex->Unlock();

		ret = CollectAndRemoveMissing(&fetched, outRemovedAll, outRemovedAccepted,
					      outRemovedPendingOrReq);
		if (ret >= 0) {
			ret = ApplyAddedAndUpdated(&fetched, outAdded, outUpdated);
			if (ret >= 0) {
				m_loaded = 1;
				if (outFirstLoad != NULL)
					*outFirstLoad = (wasLoaded == 0);
			}
		}
		m_mutex->Unlock();
		if (ret < 0)
			Unlock();
	}
	return ret;
}

/* 0x81302A34: update presence for a friend (single platform or all platforms) */
int NpImpsCacheFriendStoreImpl::UpdatePresence(const SceNpId *npId,
					       NpImpsSvcPresenceData **newPresence,
					       int platform)
{
	m_mutex->Lock();
	NpImpsCacheFriendEntryImpl *entry = FindLocked(npId);
	if (entry == NULL) {
		m_mutex->Unlock();
		return NP_IMPS_SVC_ERROR_NO_PRESENCE;
	}
	entry->AddRef();

	int ret = 0;
	int firstPlatform = (platform == NP_IMPS_SVC_PLATFORM_UNSPECIFIED)
				    ? NP_IMPS_SVC_PLATFORM_PSP2
				    : platform;
	int lastPlatform = (platform == NP_IMPS_SVC_PLATFORM_UNSPECIFIED)
				   ? NP_IMPS_SVC_PLATFORM_PS3
				   : platform;

	for (int curPlat = firstPlatform; curPlat <= lastPlatform; ++curPlat) {
		NpImpsSvcPresenceDiffFlags diffFlags;
		sce_paf_memset(&diffFlags, 0, sizeof(diffFlags));
		NpImpsSvcPresenceData *prevPresence = NULL;

		ret = entry->UpdatePresenceForPlatform(newPresence, &diffFlags,
						       &prevPresence, curPlat);
		if (ret >= 0 && m_sink != NULL) {
			if (diffFlags.effectiveStatusChanged != 0) {
				m_sink->NotifyFriendContextChanged(&entry,
								   entry->GetFriendContext(),
								   NULL);
			}
			if (curPlat == NP_IMPS_SVC_PLATFORM_PSP2) {
				m_sink->NotifyFriendPresenceChanged(&entry, newPresence,
								    &prevPresence, &diffFlags);
			}
			m_sink->NotifyPlatformPresenceChanged(&entry, newPresence,
							      &prevPresence, &diffFlags, curPlat);
		}
		if (prevPresence != NULL)
			prevPresence->Release();
		if (ret < 0)
			break;
	}

	entry->Release();
	m_mutex->Unlock();
	return ret;
}

/* 0x81302D6C: mark all cached friends offline across platforms */
int NpImpsCacheFriendStoreImpl::ResetAllPresenceOffline()
{
	m_mutex->Lock();

	NpImpsSvcPresenceData *offlinePresence = NULL;
	NpImpsSvcPresenceData templateOffline;
	templateOffline.SetOnlineStatus(NP_IMPS_SVC_ONLINE_OFFLINE);
	int ret = NpImpsSvcPresenceData::Clone(&offlinePresence, &templateOffline);
	if (ret < 0) {
		m_mutex->Unlock();
		return ret;
	}

	for (SceUInt32 pos = 0; pos < m_count; ++pos) {
		SceNpId npId = m_entries[pos].npId;
		UpdatePresence(&npId, &offlinePresence, NP_IMPS_SVC_PLATFORM_UNSPECIFIED);
	}

	offlinePresence->Release();
	m_mutex->Unlock();
	return 0;
}

/* 0x81302FCC -> 0x81304250 */
int NpImpsCacheFriendStoreImpl::FetchFromRoster(
	NpImpsCacheRefVector<NpImpsCacheRosterData> *outList)
{
	return npImpsCacheFetchFriendListFromRoster(outList);
}

/* 0x81302FD6 */
int NpImpsCacheFriendStoreImpl::Clear()
{
	m_mutex->Lock();
	for (SceUInt32 pos = 0; pos < m_count; ++pos) {
		if (m_entries[pos].entry != NULL)
			m_entries[pos].entry->Release();
	}
	m_count = 0;
	m_loaded = 0;
	m_mutex->Unlock();
	return 0;
}

/* 0x81304250 */
int npImpsCacheFetchFriendListFromRoster(NpImpsCacheRefVector<NpImpsCacheRosterData> *outList)
{
	NpRosterFriendEntry entries[100];
	sce_paf_memset(entries, 0, sizeof(entries));

	SceNpId zeroId;
	sce_paf_memset(&zeroId, 0, sizeof(zeroId));

	SceUInt32 totalCount = 0;
	int reqId = npRosterCreateRequest();
	if (reqId < 0)
		return reqId;

	int ret = npRosterGetFriendList(reqId,
					NP_ROSTER_FRIEND_LIST_OPT_UPD_DATE |
						NP_ROSTER_FRIEND_LIST_OPT_MSG_FLAG,
					entries, 100, &totalCount);
	if (ret < 0) {
		npRosterDestroyRequest(reqId);
		return ret;
	}

	for (SceUInt32 pos = 0; pos < 100; ++pos) {
		if (sce_paf_memcmp(&zeroId, &entries[pos].npId, sizeof(SceNpId)) == 0)
			continue;

		NpImpsCacheRosterData *rosterData = NULL;
		ret = NpImpsCacheRosterData::CreateFromRosterEntry(&rosterData, &entries[pos]);
		if (ret < 0) {
			if (rosterData != NULL)
				rosterData->Release();
			npRosterDestroyRequest(reqId);
			return ret;
		}
		outList->PushBack(rosterData);
		rosterData->Release();
	}

	npRosterDestroyRequest(reqId);
	return 0;
}

/* 0x81304324 */
int npImpsCacheFetchBlockListFromRoster(NpImpsCacheNpIdSet *outSet)
{
	SceNpId entries[50];
	sce_paf_memset(entries, 0, sizeof(entries));

	SceUInt32 totalCount = 0;
	int reqId = npRosterCreateRequest();
	if (reqId < 0)
		return reqId;

	int ret = npRosterGetBlockList(reqId, 0, entries, 50, &totalCount);
	if (ret >= 0) {
		SceUInt32 limit = (totalCount < 50u) ? totalCount : 50u;
		for (SceUInt32 pos = 0; pos < limit; ++pos)
			outSet->Insert(&entries[pos]);
	}

	npRosterDestroyRequest(reqId);
	return ret;
}

/* --------------------------------------------------------------------------
 * NpImpsCacheBlockStoreImpl (0x8130795C..0x8130827F)
 * -------------------------------------------------------------------------- */
NpImpsCacheBlockStoreImpl::NpImpsCacheBlockStoreImpl(NpImpsCacheStoreSink *sink,
						     paf::thread::RMutex *dataMutex)
	: m_sink(sink),
	  m_mutex(dataMutex),
	  m_loaded(0)
{
}

NpImpsCacheBlockStoreImpl::~NpImpsCacheBlockStoreImpl()
{
	Clear();
}

void NpImpsCacheBlockStoreImpl::Lock()
{
	m_mutex->Lock();
}

void NpImpsCacheBlockStoreImpl::Unlock()
{
	m_mutex->Unlock();
}

/* 0x813079A0 */
SceUInt32 NpImpsCacheBlockStoreImpl::GetCount(SceUInt8 *outLoaded)
{
	m_mutex->Lock();
	SceUInt32 count = (m_loaded != 0) ? m_blockSet.Size() : 0;
	if (outLoaded != NULL)
		*outLoaded = m_loaded;
	m_mutex->Unlock();
	return count;
}

/* 0x813079CA */
void NpImpsCacheBlockStoreImpl::CopyEntries(SceUInt32 startIndex, SceNpId *outEntries,
					    SceUInt32 maxCount, SceUInt32 *outCount,
					    SceUInt8 *outLoaded)
{
	m_mutex->Lock();
	if (outLoaded != NULL)
		*outLoaded = m_loaded;
	if (m_loaded != 0) {
		*outCount = 0;
		if (outEntries != NULL && maxCount != 0 && startIndex < m_blockSet.Size()) {
			SceUInt32 copied = 0;
			for (SceUInt32 pos = startIndex;
			     pos < m_blockSet.Size() && copied < maxCount; ++pos) {
				outEntries[copied] = *m_blockSet.At(pos);
				++copied;
			}
			*outCount = copied;
		}
	}
	m_mutex->Unlock();
}

/* 0x81307B48 */
bool NpImpsCacheBlockStoreImpl::Contains(const SceNpId *npId, SceUInt8 *outLoaded)
{
	m_mutex->Lock();
	if (outLoaded != NULL)
		*outLoaded = m_loaded;
	bool found = m_blockSet.Contains(npId);
	m_mutex->Unlock();
	return found;
}

/* 0x81307BB0 */
int NpImpsCacheBlockStoreImpl::ApplyFullList(const SceNpId *entries, SceUInt32 count,
					     NpImpsCacheNpIdSet *outAdded,
					     NpImpsCacheNpIdSet *outRemoved,
					     bool *outFirstLoad)
{
	m_mutex->Lock();
	bool wasLoaded = (m_loaded != 0);

	NpImpsCacheNpIdSet toRemove;
	for (SceUInt32 pos = 0; pos < m_blockSet.Size(); ++pos) {
		const SceNpId *existing = m_blockSet.At(pos);
		if (!npImpsCacheArrayContains(existing, entries, count)) {
			int ins = toRemove.Insert(existing);
			if (ins < 0) {
				m_mutex->Unlock();
				return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			}
		}
	}

	for (SceUInt32 pos = 0; pos < toRemove.Size(); ++pos) {
		const SceNpId *npId = toRemove.At(pos);
		if (!m_blockSet.Erase(npId)) {
			m_mutex->Unlock();
			return NP_IMPS_SVC_ERROR_INTERNAL;
		}
		if (outRemoved != NULL)
			outRemoved->Insert(npId);
	}

	for (SceUInt32 pos = 0; pos < count; ++pos) {
		if (!m_blockSet.Contains(&entries[pos])) {
			int ins = m_blockSet.Insert(&entries[pos]);
			if (ins <= 0) {
				m_mutex->Unlock();
				return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			}
			if (outAdded != NULL)
				outAdded->Insert(&entries[pos]);
		}
	}

	m_loaded = 1;
	if (outFirstLoad != NULL)
		*outFirstLoad = !wasLoaded;

	m_mutex->Unlock();
	return 0;
}

/* 0x81307E82 */
int NpImpsCacheBlockStoreImpl::AddEntries(const NpImpsCacheNpIdSet *npIds)
{
	m_mutex->Lock();
	NpImpsCacheNpIdSet added;
	int firstError = 0;

	for (SceUInt32 pos = 0; pos < npIds->Size(); ++pos) {
		const SceNpId *npId = npIds->At(pos);
		if (!m_blockSet.Contains(npId)) {
			int ins = m_blockSet.Insert(npId);
			if (ins <= 0) {
				if (firstError == 0)
					firstError = NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			} else {
				added.Insert(npId);
			}
		}
	}

	if (m_sink != NULL) {
		int notifyRet = m_sink->NotifyBlockListAdded(&added);
		if (notifyRet < 0 && firstError == 0)
			firstError = notifyRet;
	}

	m_mutex->Unlock();
	return firstError;
}

/* 0x81307FDA */
int NpImpsCacheBlockStoreImpl::RemoveEntries(const NpImpsCacheNpIdSet *npIds)
{
	m_mutex->Lock();
	NpImpsCacheNpIdSet removed;

	for (SceUInt32 pos = 0; pos < npIds->Size(); ++pos) {
		const SceNpId *npId = npIds->At(pos);
		m_blockSet.Erase(npId);
		removed.Insert(npId);
	}

	int ret = 0;
	if (m_sink != NULL)
		ret = m_sink->NotifyBlockListRemoved(&removed);

	m_mutex->Unlock();
	return (ret >= 0) ? 0 : ret;
}

/* 0x8130810A */
int NpImpsCacheBlockStoreImpl::FetchAndApply(NpImpsCacheNpIdSet *outAdded,
					     NpImpsCacheNpIdSet *outRemoved,
					     bool *outFirstLoad)
{
	NpImpsCacheNpIdSet fetched;
	int ret = npImpsCacheFetchBlockListFromRoster(&fetched);
	if (ret < 0)
		return ret;

	Lock();
	ret = ApplyFullList(fetched.Data(), fetched.Size(), outAdded, outRemoved, outFirstLoad);
	if (ret < 0)
		Unlock();
	return ret;
}

/* 0x81308252 */
int NpImpsCacheBlockStoreImpl::FetchFromRoster(NpImpsCacheNpIdSet *outSet)
{
	return npImpsCacheFetchBlockListFromRoster(outSet);
}

/* 0x8130825C */
int NpImpsCacheBlockStoreImpl::Clear()
{
	m_mutex->Lock();
	m_blockSet.Clear();
	m_loaded = 0;
	m_mutex->Unlock();
	return 0;
}
