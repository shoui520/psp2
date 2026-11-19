/*
 * The network daemon: it queues the requests to the NP activity service (posting statuses
 * and comments, good jobs, syncing the feed) on its thread, and brings what the network
 * answers into the activity daemon's databases.
 */
/* Sony's build of this file inlines vector's fill constructor */
#define PAF_VECTOR_FILL_INLINE
#include "activity_db_internal.h"
#include <framework.h>

/* except for the statuses' vector, whose fill constructor Sony calls */
namespace paf {
template<> inline __attribute__((noinline))
vector<ActivityDbNetStatus, allocator<ActivityDbNetStatus> >::vector(size_type size,
	const ActivityDbNetStatus &val, const allocator<ActivityDbNetStatus> &al)
	: alloc(al)
{
	count = 0;
	limit = 0;

	ActivityDbNetStatus *buffer = alloc.allocate(size);

	if (buffer == NULL)
		alloc.report_failure();
	ptr = buffer;
	if (buffer != NULL) {
		count = size;
		limit = size;
		for (size_type i = 0; i < count; ++i)
			new (&ptr[i]) ActivityDbNetStatus(val);
	}
}
}

const char g_activityDbLocalTitle0[] = ACTIVITY_DB_LOCAL_TITLE_0;
const char g_activityDbLocalTitle1[] = ACTIVITY_DB_LOCAL_TITLE_1;

ActivityDbNoNpAccess ActivityDbNetDaemon::s_noNpAccess;

extern "C" ActivityDbNetDaemon *SceActivityDb_7B68F69D(void)
{
	static ActivityDbNetDaemon daemon;

	return &daemon;
}

int ActivityDbNetDaemon::Start(int priority, SceUID *threadId)
{
	int result;

	m_lock.Lock();
	if (m_started || m_npAccess != &s_noNpAccess) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	m_npAccess = new ActivityDbNpAccess;
	result = m_npAccess->Open();
	if (result == 0) {
		m_queue = new ActivityDbNetQueue(priority);
		if (m_queue == NULL) {
			m_lock.Unlock();
			return ACTIVITY_DB_ERROR_NO_THREAD;
		}
		if (threadId != NULL)
			*threadId = m_queue->GetID();
		result = m_queue->Start();
		if (result != 0) {
			if (m_queue != NULL) {
				delete m_queue;
				m_queue = NULL;
			}
		} else {
			m_started = true;
		}
	}
	m_lock.Unlock();
	return result;
}

void ActivityDbNetDaemon::Stop()
{
	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return;
	}
	if (m_queue != NULL) {
		m_queue->Stop();
		if (m_queue != NULL)
			delete m_queue;
		m_queue = NULL;
	}
	m_npAccess->Close();
	if (m_npAccess != &s_noNpAccess) {
		if (m_npAccess != NULL)
			delete m_npAccess;
		m_npAccess = &s_noNpAccess;
	}
	m_user = ActivityDbOwner();
	m_started = false;
	m_lock.Unlock();
}

/* whether a status can be posted: a type, a title, a time and a message */
static inline int ActivityDbCheckPostStatus(const ActivityDbStatus *status)
{
	if (status->type == 100)
		return ACTIVITY_DB_ERROR_INVALID_TYPE;
	if (status->titleId.size() == 0)
		return ACTIVITY_DB_ERROR_NO_TITLE_ID;
	if (status->updateTime == 0)
		return ACTIVITY_DB_ERROR_NO_TIME;
	if (status->message.size() == 0)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	return 0;
}

int ActivityDbNetDaemon::PostStatus(const ActivityDbStatus *status, SceInt64 id)
{
	paf::rtc::Tick now;
	int result;

	if (!m_started)
		return ACTIVITY_DB_ERROR_NOT_READY;
	m_lock.Lock();

	ActivityDbStatus copy = *status;

	now = 0;
	if (paf::rtc::GetCurrentNetworkTick(&now) != 0) {
		paf::rtc::Tick tick = 0;

		paf::rtc::GetCurrentTick(&tick);
		copy.creationTime = tick;
		copy.updateTime = tick;
	} else {
		copy.creationTime = now;
		copy.updateTime = now;
	}
	result = ActivityDbCheckPostStatus(&copy);
	if (result == 0) {
		ActivityDbNetJob *job = new ActivityDbNetPostStatusJob(&copy);

		result = m_queue->Push(job, id);
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::DeleteStatus(const int *statusId, SceInt64 id, ActivityDbJobWait *wait)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	ActivityDbNetJob *job = new ActivityDbNetDeleteStatusJob(statusId, wait);

	result = m_queue->Push(job, id);
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::PostComment(SceInt64 id, const SceInt64 *mainRowid,
	const ActivityDbComment *comment, ActivityDbJobWait *wait)
{
	paf::rtc::Tick now;
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}

	ActivityDbComment copy = *comment;

	now = 0;
	if (paf::rtc::GetCurrentNetworkTick(&now) != 0) {
		/* offline: only a comment on a status that stays on the system */
		ActivityDbQuery query;
		int database = ActivityDbGetRowidDatabase(*mainRowid);

		{
			ActivityDbQueryCondition condition;

			result = query.Setup(&condition, &database);
		}
		if (result != 0)
			goto end;

		paf::vector<ActivityDbStatus> statuses;

		{
			paf::vector<SceInt64> rowids(1, *mainRowid);

			result = query.GetStatuses(&rowids, &statuses);
		}
		if (result != 0)
			goto end;
		if (!ActivityDbIsLocalTitle(&statuses.at(0).titleId)) {
			result = ACTIVITY_DB_ERROR_NOT_LOCAL;
			goto end;
		}
	}
	copy.creationTime = now;
	result = *mainRowid == 0 || copy.message.size() == 0 ? ACTIVITY_DB_ERROR_INVALID_ARGUMENT :
							      0;
	if (result == 0) {
		ActivityDbNetJob *job = new ActivityDbNetPostCommentJob(mainRowid, comment, wait);

		result = m_queue->Push(job, id);
	}
end:
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::DeleteComment(SceInt64 id, const ActivityDbCommentKey *key,
	ActivityDbJobWait *wait)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	ActivityDbNetJob *job = new ActivityDbNetDeleteCommentJob(key, wait);

	result = m_queue->Push(job, id);
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::VoteGoodJob(SceInt64 id, const ActivityDbStatusKey *key,
	ActivityDbJobWait *wait)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	if (key->owner.ownerType == ACTIVITY_DB_OWNER_SELF)
		result = ACTIVITY_DB_ERROR_OUT_OF_RANGE;
	else {
		ActivityDbNetJob *job = new ActivityDbNetVoteGoodJobJob(key, wait);

		result = m_queue->Push(job, id);
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::CancelGoodJob(SceInt64 id, const ActivityDbStatusKey *key,
	ActivityDbJobWait *wait)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	if (key->owner.ownerType == ACTIVITY_DB_OWNER_SELF)
		result = ACTIVITY_DB_ERROR_OUT_OF_RANGE;
	else {
		ActivityDbNetJob *job = new ActivityDbNetCancelGoodJobJob(key, wait);

		result = m_queue->Push(job, id);
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::SyncActivity(SceInt64 id, const ActivityDbSyncParam *param,
	const paf::string *titleId)
{
	int result;

	if (!m_started)
		return ACTIVITY_DB_ERROR_NOT_READY;
	m_lock.Lock();
	ActivityDbNetJob *job = new ActivityDbNetSyncActivityJob(param, titleId);

	result = m_queue->Push(job, id);
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::SyncDetail(SceInt64 id, const paf::string *titleId,
	const SceInt64 *mainRowid, const ActivityDbNetCallback *callback, ActivityDbJobWait *wait)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	ActivityDbNetJob *job = new ActivityDbNetSyncDetailJob(titleId, mainRowid, callback, wait);

	result = m_queue->Push(job, id);
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::GetAvatarInfo(SceInt64 id, int database, int mode,
	const ActivityDbQueryCondition *condition, const ActivityDbNetCallback *callback)
{
	int result;

	m_lock.Lock();
	ActivityDbNetJob *job = new ActivityDbNetGetAvatarInfoJob(database, mode, condition,
		callback);

	result = m_queue->Push(job, id);
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::Cancel(SceInt64 id)
{
	int result;

	if (!m_started)
		return ACTIVITY_DB_ERROR_NOT_READY;
	m_lock.Lock();
	result = m_queue->Cancel(id);
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::GetAnnounceInfo(SceInt64 id, const ActivityDbOwner *owner,
	ActivityDbAnnounceInfo *info, ActivityDbJobWait *wait)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	ActivityDbNetJob *job = new ActivityDbNetGetAnnounceInfoJob(owner, info, wait);

	result = m_queue->Push(job, id);
	m_lock.Unlock();
	return result;
}

/* the signed-in user, as the network names them */
static __attribute__((noinline)) ActivityDbOwner ActivityDbGetSignedInUser(void)
{
	ActivityDbOwner user;
	SceNpId npId;
	int ticket;
	int result;

	ticket = SceNpManagerTicket_37F7F258();
	if (ticket >= 0) {
		result = SceNpManagerTicket_877251F3(ticket, &npId);
		SceNpManagerTicket_2B813EC7(ticket);
		if (result == 0) {
			char onlineId[65] = "";

			if (SceNpActivityNet_319CB4F4(&npId, onlineId, sizeof(onlineId)) == 0) {
				user.ownerId.assign(onlineId, sce_paf_strlen(onlineId));
				user.ownerType = ACTIVITY_DB_OWNER_SELF;
			}
		}
	}
	return user;
}

ActivityDbOwner ActivityDbNetDaemon::GetUser()
{
	if (m_user.ownerType == ACTIVITY_DB_OWNER_UNKNOWN) {
		ActivityDbOwner user;

		user = ActivityDbGetSignedInUser();
		m_user = user;
	}
	return m_user;
}

int ActivityDbNetDaemon::RemoveAllMyStatuses(SceInt64 id, ActivityDbJobWait *wait)
{
	ActivityDbNetJob *job;
	int result;

	m_lock.Lock();
	job = new ActivityDbNetRemoveAllMyStatusesJob(GetUser(), wait);
	result = m_queue->Push(job, id);
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::SendWaitStatuses()
{
	int result;

	if (!m_started)
		return ACTIVITY_DB_ERROR_NOT_READY;
	m_lock.Lock();
	ActivityDbNetJob *job = new ActivityDbNetSendWaitStatusesJob;

	result = m_queue->Push(job, ACTIVITY_DB_NET_ID_NONE);
	m_lock.Unlock();
	return result;
}

/* an owner the network gave: its ID and type (a user: the signed-in one is SELF) */
__attribute__((noinline)) int ActivityDbFromNpOwner(const NpActivityOwner *owner,
	paf::string *ownerId, int *ownerType)
{
	int result = 0;

	if (owner->type > NP_ACTIVITY_OWNER_USER) {
		if (owner->type <= NP_ACTIVITY_OWNER_TITLE) {
			const char *titleId = SceActivityDb_F138D4E5(owner->titleId);

			ownerId->assign(titleId, titleId != NULL ? sce_paf_strlen(titleId) : 0);
			*ownerType = ACTIVITY_DB_OWNER_TITLE;
		} else {
			result = ACTIVITY_DB_ERROR_OUT_OF_RANGE;
		}
	} else if (owner->type < NP_ACTIVITY_OWNER_USER) {
		result = ACTIVITY_DB_ERROR_OUT_OF_RANGE;
	} else {
		ActivityDbOwner user = SceActivityDb_7B68F69D()->GetUser();
		char onlineId[65];

		result = SceNpActivityNet_319CB4F4(&owner->npId, onlineId, sizeof(onlineId));
		if (result == 0) {
			ownerId->assign(onlineId, sce_paf_strlen(onlineId));
			*ownerType = ActivityDbStrEqual(user.ownerId.c_str(), onlineId) ?
				ACTIVITY_DB_OWNER_SELF : ACTIVITY_DB_OWNER_USER;
		}
	}
	return result;
}

/* the network's key for a status: its owner, "@" and its ID */
static inline paf::string ActivityDbNetStatusKey(const paf::string *ownerId, int statusId)
{
	char text[64];

	sce_paf_snprintf(text, sizeof(text), "%lld", (SceInt64)statusId);
	return *ownerId + "@" + paf::string(text);
}

/* fills a status the network sent */
static inline void ActivityDbSetNetStatus(ActivityDbNetStatus *entry,
	const ActivityDbStatusHeader *header)
{
	entry->status.ownerId = header->ownerId;
	entry->status.ownerType = header->ownerType;
	entry->status.rowid = header->rowid;
	entry->status.statusId = header->statusId;
	entry->status.type = header->type;
	entry->status.creationTime = header->creationTime;
	entry->status.updateTime = header->updateTime;
}

/*
 * Sorts the statuses the network listed for a title against the database's: the ones it does
 * not have go to added, the ones it has an older copy of (or has not read) to updated, and the
 * rowids of its statuses the network no longer lists to deleted.
 */
__attribute__((noinline)) int ActivityDbSortNetStatuses(
	const ActivityDbQueryCondition *condition, const paf::string &titleId,
	const paf::string &ownerId, const NpActivityStatus *statuses, int count,
	paf::vector<ActivityDbNetStatus> *added, paf::vector<ActivityDbNetStatus> *updated,
	paf::vector<SceInt64> *deleted)
{
	paf::string owner;
	paf::string key;
	paf::vector<paf::string> keys;
	SceUInt64 since;
	int ownerType;
	int result;

	since = (SceUInt64)-1;
	for (int i = 0; i < count; i++) {
		if (ActivityDbFromNpOwner(&statuses[i].owner, &owner, &ownerType) == 0) {
			key = ActivityDbNetStatusKey(&owner, statuses[i].statusId);
			keys.push_back(key);
			if (statuses[i].creationTime < since)
				since = statuses[i].creationTime;
		}
	}

	paf::vector<ActivityDbSyncStatus> matched;

	{
		ActivityDbQuery query;
		ActivityDbQueryCondition copy = *condition;

		result = query.Setup(&copy, NULL);
		if (result == 0)
			result = query.MatchStatuses(&titleId, &ownerId, &since, &keys, &matched,
				deleted);
	}
	if (result != 0)
		return result;

	paf::string statusKey;

	for (int i = 0; i < count; i++) {
		ActivityDbStatusHeader header;

		{
			ActivityDbOwner from;

			result = ActivityDbFromNpOwner(&statuses[i].owner, &from.ownerId,
				&from.ownerType);
			if (result == 0) {
				header.ownerId = from.ownerId;
				header.ownerType = from.ownerType;
				header.statusId = statuses[i].statusId;
				header.type = statuses[i].type;
				header.creationTime = statuses[i].creationTime;
				header.updateTime = statuses[i].updateTime;
			}
		}
		if (result != 0)
			continue;
		statusKey = ActivityDbNetStatusKey(&header.ownerId, header.statusId);

		paf::vector<ActivityDbSyncStatus>::iterator it;

		for (it = matched.begin(); it != matched.end(); ++it) {
			if (sce_paf_strcmp(it->syncKey.c_str(), statusKey.c_str()) == 0)
				break;
		}
		if (it == matched.end()) {
			added->push_back(ActivityDbNetStatus());

			ActivityDbNetStatus *entry = &(*added)[added->size() - 1];

			ActivityDbSetNetStatus(entry, &header);
			entry->status.titleId = titleId;
		} else if (it->status.updateTime < header.updateTime || !it->status.isGetInfo) {
			updated->push_back(ActivityDbNetStatus());

			ActivityDbNetStatus *entry = &(*updated)[updated->size() - 1];

			ActivityDbSetNetStatus(entry, &header);
			entry->status.rowid = it->status.rowid;
			entry->status.titleId = titleId;
		}
	}
	return result;
}

/* the most statuses the network lists for one owner */
#define ACTIVITY_DB_NET_STATUS_MAX	30

/* an owner as the network names it */
__attribute__((noinline)) int ActivityDbToNpOwner(const ActivityDbOwner *owner,
	NpActivityOwner *npOwner)
{
	int result = 0;

	switch (owner->ownerType) {
	case ACTIVITY_DB_OWNER_USER:
	case ACTIVITY_DB_OWNER_SELF:
		npOwner->type = NP_ACTIVITY_OWNER_USER;
		result = SceNpActivityNet_C35984F3(owner->ownerId.c_str(), &npOwner->npId);
		break;
	case ACTIVITY_DB_OWNER_TITLE:
		npOwner->type = NP_ACTIVITY_OWNER_TITLE;
		sce_paf_strncpy(npOwner->titleId, SceActivityDb_575DA6A6(owner->titleId.c_str()),
			NP_ACTIVITY_TITLE_ID_SIZE);
		break;
	default:
		result = ACTIVITY_DB_ERROR_OUT_OF_RANGE;
		break;
	}
	return result;
}

/* room for count owners' lists of statuses */
static inline NpActivityStatusList *ActivityDbNewStatusLists(SceSize count, int max)
{
	NpActivityStatusList *lists = new NpActivityStatusList[count];

	for (SceSize i = 0; i < count; i++) {
		lists[i].max = max;
		lists[i].statuses = new NpActivityStatus[max];
	}
	return lists;
}

/*
 * Reads the statuses of the condition's owner (the signed-in user's when it names none) from
 * the network and sorts them against the database's.
 */
__attribute__((noinline)) int ActivityDbGetNetStatuses(const int *connection,
	paf::vector<ActivityDbNetStatus> *added, paf::vector<ActivityDbNetStatus> *updated,
	paf::vector<SceInt64> *deleted, const ActivityDbQueryCondition *condition)
{
	SceSize count = condition->ownerId.size() != 0;
	NpActivityOwner *owners;
	NpActivityStatusList *lists;
	int max = ACTIVITY_DB_NET_STATUS_MAX;
	int result;

	if (count == 0) {
		ActivityDbOwner user;

		user = SceActivityDb_7B68F69D()->GetUser();
		max = ACTIVITY_DB_NET_STATUS_MAX;
		count = 1;
		owners = new NpActivityOwner[count];
		lists = ActivityDbNewStatusLists(count, max);
		result = ActivityDbToNpOwner(&user, owners);
		if (result != 0)
			goto end;
	} else {
		owners = new NpActivityOwner[count];
		lists = ActivityDbNewStatusLists(count, max);

		ActivityDbOwner user;

		user = SceActivityDb_7B68F69D()->GetUser();
		for (SceSize i = 0; i < count; i++) {
			ActivityDbOwner owner;

			owner.ownerId = condition->ownerId;
			owner.ownerType = ActivityDbStrEqual(user.ownerId.c_str(),
				owner.ownerId.c_str()) ? ACTIVITY_DB_OWNER_SELF : ACTIVITY_DB_OWNER_USER;
			result = ActivityDbToNpOwner(&owner, &owners[i]);
			if (result != 0)
				goto end;
		}
	}
	result = SceNpActivityNet_2000C248(*connection, 0, owners, count, &max, lists);
	if (result == 0) {
		for (SceSize i = 0; i < count; i++) {
			ActivityDbOwner owner;

			result = ActivityDbFromNpOwner(&owners[i], &owner.ownerId, &owner.ownerType);
			if (result != 0)
				break;
			result = ActivityDbSortNetStatuses(condition, "", owner.ownerId,
				lists[i].statuses, lists[i].count, added, updated, deleted);
			if (result != 0)
				break;
		}
	}
end:
	delete[] owners;
	for (SceSize i = 0; i < count; i++)
		delete[] lists[i].statuses;
	delete[] lists;
	return result;
}

/* the most statuses the network lists for a title or for every title */
#define ACTIVITY_DB_NET_TITLE_STATUS_MAX	100

/*
 * Reads the statuses the condition reaches from the network (a title's, every title's or an
 * owner's) and sorts them against the database's.
 */
__attribute__((noinline)) int ActivityDbFetchNetStatuses(const int *connection,
	const ActivityDbQueryCondition *condition, paf::vector<ActivityDbNetStatus> *added,
	paf::vector<ActivityDbNetStatus> *updated, paf::vector<SceInt64> *deleted)
{
	int result;

	switch (condition->type) {
	case ACTIVITY_DB_QUERY_TITLE: {
		char titleId[NP_ACTIVITY_TITLE_ID_SIZE];
		NpActivityRange range;
		NpActivityStatusList list;

		if (condition->titleId.size() != 0) {
			sce_paf_strncpy(titleId, SceActivityDb_575DA6A6(condition->titleId.c_str()),
				sizeof(titleId));
			range.start = 1;
			range.count = ACTIVITY_DB_NET_TITLE_STATUS_MAX;
			list.statuses = new NpActivityStatus[ACTIVITY_DB_NET_TITLE_STATUS_MAX];
			list.max = ACTIVITY_DB_NET_TITLE_STATUS_MAX;
			result = SceNpActivityNet_40475264(*connection, 0, &titleId, 1, &range,
				&list);
			if (result == 0) {
				paf::string ownerId;

				result = ActivityDbSortNetStatuses(condition,
					condition->titleId.c_str(), ownerId,
					list.statuses, list.count, added, updated, deleted);
				if (result != 0)
					goto titleEnd;
			}
		} else {
			result = ACTIVITY_DB_ERROR_NO_ID;
		}
titleEnd:
		delete[] list.statuses;
		return result;
	}
	case ACTIVITY_DB_QUERY_ALL: {
		NpActivityRange range;
		NpActivityStatus *statuses;
		int count;

		range.start = 1;
		range.count = ACTIVITY_DB_NET_TITLE_STATUS_MAX;
		statuses = new NpActivityStatus[ACTIVITY_DB_NET_TITLE_STATUS_MAX];
		count = 0;
		result = SceNpActivityNet_99C86ADA(*connection, 0, &range, statuses,
			ACTIVITY_DB_NET_TITLE_STATUS_MAX, &count);
		if (result == 0) {
			paf::string ownerId;

			result = ActivityDbSortNetStatuses(condition, "", ownerId, statuses,
				count, added, updated, deleted);
			if (result != 0)
				goto allEnd;
		}
allEnd:
		delete[] statuses;
		return result;
	}
	case ACTIVITY_DB_QUERY_NEWS:
		return ActivityDbGetNetStatuses(connection, added, updated, deleted, condition);
	default:
		return ACTIVITY_DB_ERROR_OUT_OF_RANGE;
	}
}

/* the avatar and panel sizes the daemon looks a status's owners up in */
#define ACTIVITY_DB_NET_AVATAR_SIZE	3
#define ACTIVITY_DB_NET_PANEL_SIZE	2

/* the profiles of count users, from the NP community as the signed-in user */
__attribute__((noinline)) int ActivityDbLookupNpProfiles(const SceNpId *npIds, SceUInt32 count,
	NpLookupProfile *profiles, SceInt32 *results)
{
	int titleCtxId;
	int reqId;
	int result;

	{
		ActivityDbOwner user = SceActivityDb_7B68F69D()->GetUser();
		NpActivityOwner self;

		result = ActivityDbToNpOwner(&user, &self);
		if (result != 0)
			return result;
		titleCtxId = npCommunityCreateTitleCtx(NULL, &self.npId);
		if (titleCtxId < 0) {
			result = titleCtxId;
			goto end;
		}
		reqId = npCommunityCreateRequest(titleCtxId);
		if (reqId < 0) {
			result = reqId;
			goto end;
		}
	}
	result = npCommunityGetUserProfiles(reqId, ACTIVITY_DB_NET_AVATAR_SIZE,
		ACTIVITY_DB_NET_PANEL_SIZE, npIds, count, profiles, results);
	npCommunityDeleteRequest(reqId);
end:
	npCommunityDeleteTitleCtx(titleCtxId);
	return result;
}

typedef paf::map<paf::string, paf::string> ActivityDbUserMap;
typedef paf::map<paf::string, NpLookupProfile *> ActivityDbProfileMap;

/* Sony calls the user map's operator[] out of line */
namespace paf {
template<> inline __attribute__((noinline))
string &map<string, string, less<string>, allocator<string> >::operator[](const string &key)
{
	return this->_Insert(value_type(key, mapped_type()), NULL)->value.second;
}
}

/*
 * Looks the users up (by online ID) into profiles, one each, and maps each online ID to its
 * profile; a user the server does not answer for is left out.
 */
__attribute__((noinline)) int ActivityDbLookupUserProfiles(ActivityDbUserMap *users,
	ActivityDbProfileMap *profileMap, NpLookupProfile *profiles)
{
	SceUInt32 count = users->size();
	SceNpId *npIds;
	SceInt32 *results;
	int result;

	if (count == 0)
		return 0;
	npIds = new SceNpId[count];
	results = new SceInt32[count];

	SceNpId *npId = npIds;

	for (ActivityDbUserMap::iterator it = users->begin(); it != users->end(); ++it) {
		result = SceNpActivityNet_C35984F3(it->second.c_str(), npId++);
		if (result != 0)
			goto end;
	}
	result = ActivityDbLookupNpProfiles(npIds, count, profiles, results);
	if (result == 0) {
		profileMap->clear();
		for (SceUInt32 i = 0; i < count; i++) {
			if (results[i] == SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE)
				continue;
			if (results[i] != 0) {
				result = results[i];
				break;
			}

			char onlineId[65];

			result = SceNpActivityNet_319CB4F4(&profiles[i].npId, onlineId,
				sizeof(onlineId));

			paf::string id(onlineId);

			if (profileMap->find(id) == profileMap->end())
				profileMap->insert(paf::pair<paf::string, NpLookupProfile *>(id,
					&profiles[i]));
		}
	}
end:
	delete[] npIds;
	delete[] results;
	return result;
}

/* a status's panel from its owner's profile: the colour, and the panel's URL when it has one */
__attribute__((noinline)) void ActivityDbSetAnnouncePanel(SceUInt32 *bgColor,
	paf::string *panelUrl, const NpLookupPanel *panel)
{
	switch (panel->color.panel.type) {
	case 1:
		*bgColor = panel->color.panel.bgColor;
		break;
	case 2:
		*bgColor = panel->color.panel.bgColor;
		panelUrl->assign(panel->panelUrl);
		break;
	default:
		*bgColor = ACTIVITY_DB_PANEL_BG_COLOR_DEFAULT;
		break;
	}
}

/* the status type whose action URL names the user it is about */
#define ACTIVITY_DB_STATUS_TYPE_USER	5
/* the status type that is a game's: its icon is the title's */
#define ACTIVITY_DB_STATUS_TYPE_TITLE	4

static inline bool ActivityDbIsUserStatus(const ActivityDbNetStatus *status)
{
	return status->status.ownerType == ACTIVITY_DB_OWNER_USER ||
		status->status.ownerType == ACTIVITY_DB_OWNER_SELF;
}

/* looks up the users the statuses name, and fills in their avatars and panels */
__attribute__((noinline)) int ActivityDbGetNetStatusProfiles(
	paf::vector<ActivityDbNetStatus> *statuses)
{
	NpLookupProfile *profiles;
	int result;

	if (statuses->size() == 0)
		return 0;
	{
		ActivityDbUserMap users;
		paf::vector<ActivityDbNetStatus>::iterator it;

		for (it = statuses->begin(); it != statuses->end(); ++it) {
			if (!ActivityDbIsUserStatus(&*it))
				continue;
			if (users[it->status.ownerId].size() == 0)
				users[it->status.ownerId] = it->status.ownerId;

			char onlineId[65];

			if (it->status.type == ACTIVITY_DB_STATUS_TYPE_USER &&
			    SceNpActivityNet_C27E99D3(it->status.actionUrl.c_str(), onlineId,
				sizeof(onlineId)) == 0) {
				paf::string subjectId(onlineId);

				if (users[subjectId].size() == 0)
					users[subjectId] = subjectId;
				it->subjectId = subjectId;
			}
		}

		ActivityDbProfileMap profileMap;

		profiles = new NpLookupProfile[users.size()];
		result = ActivityDbLookupUserProfiles(&users, &profileMap, profiles);
		if (result == 0) {
			for (it = statuses->begin(); it != statuses->end(); ++it) {
				if (!ActivityDbIsUserStatus(&*it))
					continue;

				ActivityDbProfileMap::iterator found = profileMap.find(it->status.ownerId);

				if (found != profileMap.end()) {
					it->status.announceAvatarUrl = found->second->icon.data;
					ActivityDbSetAnnouncePanel(&it->status.announcePanelBgColor,
						&it->status.announcePanelUrl, &found->second->panel);
					it->isGetAnnounce = true;
				}
				if (it->status.type == ACTIVITY_DB_STATUS_TYPE_USER) {
					found = profileMap.find(it->subjectId);
					if (found != profileMap.end())
						it->status.iconUrl = found->second->icon.data;
				}
			}
		}
	}
	delete[] profiles;
	return result;
}

/* the most statuses one detail request reads */
#define ACTIVITY_DB_NET_DETAIL_MAX	50

__attribute__((noinline)) void ActivityDbAddNetStatusDetail(
	const NpActivityStatusDetail *detail, void *arg);

/* reads the details of count statuses into statuses, then their owners' profiles */
__attribute__((noinline)) int ActivityDbGetNetStatusDetails(const int *connection,
	const NpActivityStatusKey *keys, int count, paf::vector<ActivityDbNetStatus> *statuses)
{
	int locale;
	NpActivityDetailState state;
	char work[NP_ACTIVITY_DETAIL_WORK_SIZE];
	int result;

	if (count == 0)
		return 0;
	statuses->clear();

	paf::Framework *framework = paf::Framework::Instance();

	locale = 1;
	if (framework != NULL)
		locale = framework->GetLocale();
	for (int done = 0, rest = count; rest > 0 && done < count;
	     rest -= ACTIVITY_DB_NET_DETAIL_MAX, done += ACTIVITY_DB_NET_DETAIL_MAX,
	     keys += ACTIVITY_DB_NET_DETAIL_MAX) {
		state.state = 0;
		result = SceNpActivityNet_7E58E9B9(*connection, 0, keys,
			rest > ACTIVITY_DB_NET_DETAIL_MAX ? ACTIVITY_DB_NET_DETAIL_MAX : rest, &locale,
			work, &state, ActivityDbAddNetStatusDetail, statuses);
		if (result != 0)
			return result;
	}
	return ActivityDbGetNetStatusProfiles(statuses);
}

static inline bool ActivityDbIsSameNetStatus(const ActivityDbNetStatus *status,
	const ActivityDbNetStatus *detail)
{
	if (detail->status.ownerType == ACTIVITY_DB_OWNER_TITLE &&
	    status->status.ownerType == ACTIVITY_DB_OWNER_TITLE)
		return ActivityDbStrEqual(status->status.titleId.c_str(),
			detail->status.titleId.c_str());
	return ActivityDbStrEqual(status->status.ownerId.c_str(), detail->status.ownerId.c_str());
}

/* reads the details of the first count statuses from the network into them */
__attribute__((noinline)) int ActivityDbUpdateNetStatusDetails(const int *connection,
	paf::vector<ActivityDbNetStatus> *statuses, int count)
{
	NpActivityStatusKey *keys = NULL;
	int result = 0;

	if (count != 0) {
		keys = new NpActivityStatusKey[count];

		NpActivityStatusKey *key = keys;

		for (int i = 0; i < count; i++, key++) {
			key->statusId = (*statuses)[i].status.statusId;

			ActivityDbOwner owner;
			NpActivityOwner npOwner;

			owner.ownerId = (*statuses)[i].status.ownerId;
			owner.ownerType = (*statuses)[i].status.ownerType;
			owner.titleId = (*statuses)[i].status.titleId;
			result = ActivityDbToNpOwner(&owner, &npOwner);
			if (result != 0)
				goto end;
			key->owner = npOwner;
		}
		{
			paf::vector<ActivityDbNetStatus> details;

			result = ActivityDbGetNetStatusDetails(connection, keys, count, &details);
			if (result != 0)
				goto end;

			SceUInt32 j = 0;

			for (int i = 0; i < count && j < details.size(); i++) {
				if ((*statuses)[i].status.statusId != details[j].status.statusId ||
				    !ActivityDbIsSameNetStatus(&(*statuses)[i], &details[j]))
					continue;
				(*statuses)[i].status.ownerId = details[j].status.ownerId;
				(*statuses)[i].status.type = details[j].status.type;
				(*statuses)[i].status.titleId = details[j].status.titleId;
				(*statuses)[i].status.titleName = details[j].status.titleName;
				(*statuses)[i].status.message = details[j].status.message;
				(*statuses)[i].status.numOfComments =
					details[j].status.numOfComments;
				(*statuses)[i].status.isGetInfo = details[j].status.isGetInfo;
				(*statuses)[i].status.numOfVotes = details[j].status.numOfVotes;
				(*statuses)[i].isGetAnnounce = details[j].isGetAnnounce;
				(*statuses)[i].status.contentId = details[j].status.contentId;
				(*statuses)[i].status.iconUrl = details[j].status.iconUrl;
				(*statuses)[i].status.actionUrl = details[j].status.actionUrl;
				(*statuses)[i].status.announceAvatarUrl =
					details[j].status.announceAvatarUrl;
				(*statuses)[i].status.announcePanelBgColor =
					details[j].status.announcePanelBgColor;
				(*statuses)[i].status.announcePanelUrl =
					details[j].status.announcePanelUrl;
				j++;
			}
		}
	}
end:
	delete[] keys;
	return result;
}

/* the most statuses of the user's news, and of the others, the daemon reads the details of */
#define ACTIVITY_DB_NET_NEWS_DETAIL_MAX		30
#define ACTIVITY_DB_NET_DETAIL_READ_MAX		20

/*
 * Reads what the network has for a sync: the statuses it added and updated since the last
 * sync and the ones it removed, the details of the newest of them, and the network's time.
 */
int ActivityDbNetDaemon::FetchActivity(const ActivityDbSyncParam *param,
	const paf::string *titleId, paf::vector<ActivityDbNetStatus> *added,
	paf::vector<ActivityDbNetStatus> *updated, paf::vector<SceInt64> *deleted,
	SceUInt64 *netTime)
{
	int connection;
	int database;
	int count;
	int rest;
	int result;

	result = m_npAccess->Begin(0, &connection);
	if (result != 0)
		goto done;
	*netTime = 0;
	paf::rtc::GetCurrentNetworkTick(netTime);
	if (param->since > 0 && (param->condition.type == ACTIVITY_DB_QUERY_NEWS ||
				 param->condition.type == ACTIVITY_DB_QUERY_ALL))
		goto fetch;
	{
		bool changed;

		if (param->since != 0) {
			SceUInt64 since = param->since;
			char titleIds[1][NP_ACTIVITY_TITLE_ID_SIZE];
			SceSize titleCount = 0;
			NpActivityUpdates updates;

			changed = false;
			if (param->condition.titleId.size() != 0) {
				titleCount = 1;
				sce_paf_strncpy(titleIds[0],
					SceActivityDb_575DA6A6(param->condition.titleId.c_str()),
					sizeof(titleIds[0]));
			}
			result = SceNpActivityNet_C55B9299(connection, 0, &since, titleIds,
				titleCount, &updates);
			if (result == 0 &&
			    (updates.counts[0] | updates.counts[1] | updates.counts[2]) != 0)
				changed = true;
		} else {
			changed = true;
			result = 0;
		}
		if (result != 0)
			goto end;
		if (!changed)
			goto done;
	}
fetch:
	result = ActivityDbFetchNetStatuses(&connection, &param->condition, added, updated,
		deleted);
	if (result != 0)
		goto end;
	database = ActivityDbGetQueryDatabase(param->condition.type);
	switch (param->condition.type) {
	case ACTIVITY_DB_QUERY_ALL:
		count = ACTIVITY_DB_NET_DETAIL_READ_MAX;
		if (added->size() < ACTIVITY_DB_NET_DETAIL_READ_MAX)
			count = added->size();
		rest = ACTIVITY_DB_NET_DETAIL_READ_MAX - count;
		if ((SceUInt32)rest > updated->size())
			rest = updated->size();
		break;
	case ACTIVITY_DB_QUERY_NEWS:
		count = ACTIVITY_DB_NET_NEWS_DETAIL_MAX;
		if (added->size() < ACTIVITY_DB_NET_NEWS_DETAIL_MAX)
			count = added->size();
		rest = ACTIVITY_DB_NET_NEWS_DETAIL_MAX - count;
		if ((SceUInt32)rest > updated->size())
			rest = updated->size();
		break;
	default:
		count = ACTIVITY_DB_NET_DETAIL_READ_MAX;
		if (added->size() < ACTIVITY_DB_NET_DETAIL_READ_MAX)
			count = added->size();
		rest = ACTIVITY_DB_NET_DETAIL_READ_MAX - count;
		if ((SceUInt32)rest > updated->size())
			rest = updated->size();
		break;
	}
	result = ActivityDbUpdateNetStatusDetails(&connection, added, count);
	if (result != 0)
		goto end;
	result = ActivityDbUpdateNetStatusDetails(&connection, updated, rest);
	if (result != 0)
		goto end;
	for (paf::vector<ActivityDbNetStatus>::iterator it = added->begin(); it != added->end();
	     ++it) {
		it->addedVotes = it->status.numOfVotes;
		it->addedComments = it->status.numOfComments;
		it->status.numOfNewComments = it->status.numOfComments;
	}
	result = updated->size() != 0 ?
		ActivityDbUpdateCounts(database, param->keepNewComments, updated) : 0;
done:
	if (result == 0 && *netTime == 0)
		result = paf::rtc::GetCurrentNetworkTick(netTime);
end:
	m_npAccess->End();
	return result;
}

typedef paf::map<SceInt64, const NpActivityComment *> ActivityDbNetCommentMap;
typedef paf::map<SceInt64, const ActivityDbCommentKey *> ActivityDbCommentKeyMap;

/*
 * Sorts the comments the network listed for a status against the database's: the ones the
 * database does not have go to added, the database's the network no longer lists to deleted.
 */
__attribute__((noinline)) int ActivityDbSortNetComments(const SceInt64 *mainRowid,
	const NpActivityComment *comments, int count, paf::vector<ActivityDbComment> *added,
	paf::vector<ActivityDbCommentKey> *deleted)
{
	int result;

	if (count == 0)
		return 0;

	ActivityDbNetCommentMap netComments;
	ActivityDbCommentKeyMap dbComments;
	paf::vector<ActivityDbCommentKey> keys;

	for (; count != 0; count--, comments++)
		netComments[(SceInt64)comments->commentId] = comments;
	{
		ActivityDbQuery query;
		ActivityDbQueryCondition condition;
		int database = ActivityDbGetRowidDatabase(*mainRowid);

		result = query.Setup(&condition, &database);
		if (result != 0)
			return result;
		result = query.GetCommentKeys(mainRowid, &keys);
		if (result != 0)
			return result;
	}
	for (SceUInt32 i = 0; i < keys.size(); i++)
		dbComments[(SceInt64)keys[i].commentId] = &keys[i];
	for (ActivityDbCommentKeyMap::iterator it = dbComments.begin(); it != dbComments.end();) {
		if (netComments.find(it->first) == netComments.end()) {
			deleted->push_back(*it->second);
			dbComments.erase(it++);
		} else {
			++it;
		}
	}
	for (ActivityDbNetCommentMap::iterator it = netComments.begin();
	     it != netComments.end(); ++it) {
		if (dbComments.find(it->first) != dbComments.end())
			continue;

		ActivityDbComment comment;
		const NpActivityComment *net = it->second;

		result = ActivityDbFromNpOwner(&net->owner, &comment.ownerId, &comment.ownerType);
		if (result == 0) {
			comment.creationTime = net->creationTime;
			comment.message = net->message;
		}
		if (result != 0)
			return result;

		ActivityDbComment entry;

		entry = comment;
		entry.commentId = it->second->commentId;
		added->push_back(entry);
	}
	return result;
}

/* who gave a good job, by online ID: the signed-in user is SELF */
static inline int ActivityDbVoterType(const ActivityDbOwner *user, const paf::string *ownerId)
{
	return ActivityDbStrEqual(user->ownerId.c_str(), ownerId->c_str()) ?
		ACTIVITY_DB_OWNER_SELF : ACTIVITY_DB_OWNER_USER;
}

/*
 * Sorts the users the network lists as having given a status a good job against the
 * database's: the database's the network no longer lists go to removed, the network's the
 * database does not have to added (the oldest first).
 */
__attribute__((noinline)) int ActivityDbSortNetGoodJobs(const ActivityDbStatusKey *key,
	const SceNpId *npIds, SceUInt32 count, ActivityDbGoodJobKey *added,
	ActivityDbGoodJobKey *removed)
{
	ActivityDbUserMap netVoters;
	ActivityDbUserMap dbVoters;
	ActivityDbGoodJobKey goodJob;
	char onlineId[65] = "";
	int result;

	for (SceUInt32 i = 0; i < count; i++) {
		result = SceNpActivityNet_319CB4F4(&npIds[i], onlineId, sizeof(onlineId));
		if (result != 0)
			goto end;

		paf::string id(onlineId);

		netVoters[id] = onlineId;
	}
	{
		ActivityDbQuery query;
		ActivityDbQueryCondition condition;
		int database = ActivityDbGetRowidDatabase(key->rowid);

		result = query.Setup(&condition, &database);
		if (result != 0)
			goto end;
		result = query.GetGoodJobKey(key, &goodJob);
		if (result != 0)
			goto end;
	}
	for (paf::vector<ActivityDbOwner>::iterator it = goodJob.owners.begin();
	     it != goodJob.owners.end(); ++it) {
		paf::string id(it->ownerId);

		dbVoters[id] = id;
	}
	removed->status = *key;
	for (ActivityDbUserMap::iterator it = dbVoters.begin(); it != dbVoters.end();) {
		paf::string id(it->first);

		if (netVoters.find(id) == netVoters.end()) {
			ActivityDbOwner owner;

			owner.ownerId = id;

			ActivityDbOwner user = SceActivityDb_7B68F69D()->GetUser();

			owner.ownerType = ActivityDbVoterType(&user, &id);
			removed->owners.push_back(owner);
			dbVoters.erase((it++)->first);
		} else {
			++it;
		}
	}
	added->status = *key;
	for (int i = count - 1; i >= 0; i--) {
		result = SceNpActivityNet_319CB4F4(&npIds[i], onlineId, sizeof(onlineId));
		if (result != 0)
			break;

		paf::string id(onlineId);

		if (dbVoters.find(id) == dbVoters.end()) {
			ActivityDbOwner owner;

			owner.ownerId = id;

			ActivityDbOwner user = SceActivityDb_7B68F69D()->GetUser();

			owner.ownerType = ActivityDbVoterType(&user, &id);
			added->owners.push_back(owner);
		}
	}
end:
	return result;
}

/*
 * A status's details as the network gave them into status: a title's status is owned by
 * "<from>@<title>", and its announcement is to be read.
 */
__attribute__((noinline)) int ActivityDbFromNpStatusInfo(const NpActivityStatusKey *key,
	const NpActivityStatusInfo *info, ActivityDbNetStatus *status)
{
	ActivityDbOwner owner;
	int result;

	result = ActivityDbFromNpOwner(&key->owner, &owner.ownerId, &owner.ownerType);
	if (result == 0) {
		const char *iconUrl = info->iconUrl;
		const char *actionUrl = info->actionUrl;
		const char *contentId = info->contentId;
		const char *avatarUrl = info->avatarUrl;
		const char *panelUrl = info->panelUrl;
		const char *titleId;

		status->status.ownerId = key->owner.type == NP_ACTIVITY_OWNER_TITLE ?
			paf::string(info->fromName) + '@' + owner.ownerId : owner.ownerId;
		status->status.ownerType = owner.ownerType;
		status->status.statusId = key->statusId;
		status->status.type = info->type;
		status->status.creationTime = info->creationTime;
		status->status.updateTime = info->updateTime;
		status->status.isGetInfo = true;
		status->status.isGetDetail = false;
		status->isGetAnnounce = key->owner.type == NP_ACTIVITY_OWNER_TITLE;
		titleId = SceActivityDb_F138D4E5(info->titleId);
		status->status.titleId = titleId;
		status->status.titleName = info->titleName;
		status->status.message = info->message;
		status->status.numOfComments = info->numOfComments;
		status->status.numOfNewComments = 0;
		status->status.numOfVotes = info->numOfVotes;
		status->status.iconUrl = iconUrl;
		status->status.actionUrl = actionUrl;
		status->status.contentId = contentId;
		status->status.announceAvatarUrl = avatarUrl;
		status->status.announcePanelBgColor = info->panelBgColor != 0 ?
			info->panelBgColor : ACTIVITY_DB_PANEL_BG_COLOR_DEFAULT;
		status->status.announcePanelUrl = panelUrl;
	}
	return result;
}

/* the most comments, and voters, one status's detail request reads */
#define ACTIVITY_DB_NET_DETAIL_COMMENT_MAX	20
#define ACTIVITY_DB_NET_DETAIL_VOTER_MAX	20

/* a status's key as the network names it */
static inline int ActivityDbToNpStatusKey(const ActivityDbStatusKey *key,
	NpActivityStatusKey *npKey)
{
	int result;

	if (key->owner.ownerId.size() == 0) {
		result = ACTIVITY_DB_ERROR_NO_ID;
	} else if (key->owner.ownerType == ACTIVITY_DB_OWNER_UNKNOWN) {
		result = ACTIVITY_DB_ERROR_NO_ID;
	} else if (key->statusId == -1) {
		result = ACTIVITY_DB_ERROR_NO_ID;
	} else {
		result = ActivityDbToNpOwner(&key->owner, &npKey->owner);
		if (result == 0)
			npKey->statusId = key->statusId;
	}
	return result;
}

/*
 * Reads a status's details from the network, with its newest comments and good jobs. When
 * the network's copy is newer than the database's (or the database has no details yet), the
 * comments and good jobs are sorted against the database's, *updated is set, status filled
 * and *isVoted tells whether the signed-in user gave it a good job.
 */
__attribute__((noinline)) int ActivityDbGetNetStatusDetail(const int *connection,
	const ActivityDbStatusKey *key, bool *updated, ActivityDbNetStatus *status,
	paf::vector<ActivityDbComment> *addedComments,
	paf::vector<ActivityDbCommentKey> *deletedComments, ActivityDbGoodJobKey *addedGoodJobs,
	ActivityDbGoodJobKey *removedGoodJobs, bool *isVoted)
{
	NpActivityStatusDetail detail;
	NpActivityDetailParam param;
	NpActivityComment *comments = NULL;
	SceNpId *voters = NULL;
	int numComments = 0;
	int voted = 0;
	SceUInt32 numVoters = 0;
	int result;

	*updated = false;
	result = ActivityDbToNpStatusKey(key, &detail.key);
	if (result != 0)
		goto end;
	param.comments.start = 1;
	param.comments.count = ACTIVITY_DB_NET_DETAIL_COMMENT_MAX;
	param.voters.start = 1;
	param.voters.count = ACTIVITY_DB_NET_DETAIL_VOTER_MAX;
	{
		paf::Framework *framework = paf::Framework::Instance();

		param.locale = 1;
		if (framework != NULL)
			param.locale = framework->GetLocale();
	}
	comments = new NpActivityComment[ACTIVITY_DB_NET_DETAIL_COMMENT_MAX];
	voters = new SceNpId[ACTIVITY_DB_NET_DETAIL_VOTER_MAX];
	result = SceNpActivityNet_7E3116FB(*connection, 0, &detail.key, &param, &detail.info,
		comments, ACTIVITY_DB_NET_DETAIL_COMMENT_MAX, &numComments, &voted, voters,
		ACTIVITY_DB_NET_DETAIL_VOTER_MAX, &numVoters);
	if (result != 0)
		goto end;
	{
		bool found = false;
		ActivityDbStatusState state;

		{
			ActivityDbQuery query;
			ActivityDbQueryCondition condition;
			int database = ActivityDbGetRowidDatabase(key->rowid);

			result = query.Setup(&condition, &database);
			if (result != 0)
				goto end;
			result = query.GetStatusState(&key->rowid, &found, &state);
			if (result != 0)
				goto end;
		}
		if (!found) {
			result = ACTIVITY_DB_ERROR_OUT_OF_RANGE;
			goto end;
		}
		if (state.updateTime < detail.info.updateTime || !state.isGetDetail) {
			result = ActivityDbSortNetComments(&key->rowid, comments, numComments,
				addedComments, deletedComments);
			if (result != 0)
				goto end;
			result = ActivityDbSortNetGoodJobs(key, voters, numVoters, addedGoodJobs,
				removedGoodJobs);
			if (result != 0)
				goto end;
			*updated = true;
			result = ActivityDbToNpStatusKey(key, &detail.key);
			if (result != 0)
				goto end;
			result = ActivityDbFromNpStatusInfo(&detail.key, &detail.info, status);
			if (result != 0)
				goto end;
			*isVoted = voted == 1;
			status->status.isGetDetail = true;
		}
	}
end:
	delete[] comments;
	delete[] voters;
	return result;
}

/* a comment by a user (not a title) */
static inline bool ActivityDbIsUserComment(const ActivityDbComment *comment)
{
	return comment->ownerType == ACTIVITY_DB_OWNER_USER ||
		comment->ownerType == ACTIVITY_DB_OWNER_SELF;
}

/*
 * Looks up the users who wrote the comments and fills in their avatars and panels; for a
 * user's status, also the icon of the user it is about.
 */
__attribute__((noinline)) int ActivityDbGetNetCommentProfiles(
	paf::vector<ActivityDbComment> *comments, ActivityDbNetStatus *status)
{
	if (comments->size() == 0)
		return 0;

	ActivityDbProfileMap profileMap;
	NpLookupProfile *profiles;
	int result;

	{
		ActivityDbUserMap users;
		paf::vector<ActivityDbComment>::iterator it;

		for (it = comments->begin(); it != comments->end(); ++it) {
			if (!ActivityDbIsUserComment(&*it))
				continue;
			if (users[it->ownerId].size() == 0)
				users[it->ownerId] = it->ownerId;

			char onlineId[65];

			if (status->status.type == ACTIVITY_DB_STATUS_TYPE_USER &&
			    SceNpActivityNet_C27E99D3(status->status.actionUrl.c_str(), onlineId,
				sizeof(onlineId)) == 0) {
				paf::string subjectId(onlineId);

				if (users[subjectId].size() == 0)
					users[subjectId] = subjectId;
				status->subjectId = subjectId;
			}
		}
		profiles = new NpLookupProfile[users.size()];
		result = ActivityDbLookupUserProfiles(&users, &profileMap, profiles);
		if (result == 0) {
			for (it = comments->begin(); it != comments->end(); ++it) {
				if (!ActivityDbIsUserComment(&*it))
					continue;

				ActivityDbProfileMap::iterator found = profileMap.find(it->ownerId);

				if (found != profileMap.end()) {
					it->announceAvatarUrl = found->second->icon.data;
					ActivityDbSetAnnouncePanel(&it->announcePanelBgColor,
						&it->announcePanelUrl, &found->second->panel);
					it->isGetAnnounce = true;
				}
				if (status->status.type == ACTIVITY_DB_STATUS_TYPE_USER) {
					found = profileMap.find(status->subjectId);
					if (found != profileMap.end())
						status->status.iconUrl = found->second->icon.data;
				}
			}
		}
	}
	delete[] profiles;
	return result;
}

/*
 * Reads a status's details, comments and good jobs from the network and, when the network's
 * copy is newer, stores them (*updated). A title's statuses have no details to read.
 */
int ActivityDbNetDaemon::RunSyncDetail(SceInt64 mainRowid,
	const ActivityDbNetCallback *callback, bool *updated)
{
	ActivityDbStatusKey key;
	int connection;
	int result;

	*updated = false;
	{
		ActivityDbQuery query;
		int database = ActivityDbGetRowidDatabase(mainRowid);

		{
			ActivityDbQueryCondition condition;

			result = query.Setup(&condition, &database);
		}
		if (result != 0)
			return ACTIVITY_DB_ERROR_DATABASE;
		if (query.GetStatusKey(&mainRowid, &key, NULL) != 0)
			return ACTIVITY_DB_ERROR_CONFLICT;
	}
	if (key.owner.ownerType == ACTIVITY_DB_OWNER_TITLE)
		return 0;
	result = m_npAccess->Begin(0, &connection);
	if (result == 0) {
		ActivityDbNetStatus status;

		status.status.rowid = key.rowid;
		status.status.ownerId = key.owner.ownerId;

		paf::vector<ActivityDbComment> comments;
		paf::vector<ActivityDbCommentKey> deletedComments;
		ActivityDbGoodJobKey goodJobs;

		status.status.ownerType = key.owner.ownerType;
		status.status.statusId = key.statusId;

		ActivityDbGoodJobKey cancelledGoodJobs;
		bool isVoted = false;

		result = ActivityDbGetNetStatusDetail(&connection, &key, updated, &status, &comments,
			&deletedComments, &goodJobs, &cancelledGoodJobs, &isVoted);
		if (result != 0)
			goto end;
		if (*updated) {
			paf::vector<ActivityDbNetStatus> statuses(1, status);
			int database = ActivityDbGetRowidDatabase(key.rowid);

			if (statuses.size() != 0)
				ActivityDbUpdateCounts(database, 0, &statuses);
			if (ActivityDbGetNetCommentProfiles(&comments, &statuses.at(0)) != 0) {
				*updated = false;
				result = 0;
				goto end;
			}
			result = SceActivityDb_FBB82B37()->SyncStatusDetail(&statuses.at(0),
				&comments, &deletedComments, &goodJobs, &cancelledGoodJobs, isVoted);
		}
	}
end:
	m_npAccess->End();
	return result;
}

/* the action URL of a status that starts a game */
#define ACTIVITY_DB_PLAY_URL		"psgm:play"
#define ACTIVITY_DB_PLAY_URL_LENGTH	9

/*
 * A game's status, or one whose action starts a game, takes the title's icon from its
 * metadata on the TMDB server (none when the server has none).
 */
__attribute__((noinline)) int ActivityDbGetNetTitleIcon(ActivityDbStatus *status)
{
	int titleCtx;
	int request;
	int result;

	if (status->type != ACTIVITY_DB_STATUS_TYPE_TITLE) {
		bool other;
		{
			paf::string play(ACTIVITY_DB_PLAY_URL);

			other = sce_paf_strcmp(status->actionUrl.substr(0,
				ACTIVITY_DB_PLAY_URL_LENGTH).c_str(), play.c_str()) != 0;
		}
		if (other)
			return 0;
	}
	{
		ActivityDbOwner user = SceActivityDb_7B68F69D()->GetUser();
		NpActivityOwner npOwner;

		result = ActivityDbToNpOwner(&user, &npOwner);
		if (result != 0)
			return result;
		titleCtx = npCommunityCreateTitleCtx(NULL, &npOwner.npId);
		if (titleCtx < 0) {
			result = titleCtx;
			goto deleteTitleCtx;
		}
		request = npCommunityCreateRequest(titleCtx);
		if (request < 0) {
			result = request;
			goto deleteTitleCtx;
		}
	}
	{
		paf::Framework *framework = paf::Framework::Instance();
		int locale = 1;
		NpCommunityTitleInfo info;

		if (framework != NULL)
			locale = framework->GetLocale();

		paf::string titleId = status->titleId + "_00";

		result = npCommunityGetTitleInfo(request, titleId.c_str(), locale, &info);
		if (result == NP_LOOKUP_ERROR_NOT_FOUND) {
			status->iconUrl = "";
			result = 0;
		} else if (result == 0) {
			status->iconUrl = info.iconUrl;
		}
	}
	npCommunityDeleteRequest(request);
deleteTitleCtx:
	npCommunityDeleteTitleCtx(titleCtx);
	return result;
}

/* a status as the network takes it when it is posted */
__attribute__((noinline)) int ActivityDbToNpStatusInfo(const ActivityDbStatus *status,
	NpActivityStatusInfo *info)
{
	sce_paf_memset(info, 0, sizeof(*info));
	info->type = status->type;
	sce_paf_strncpy(info->titleId, status->titleId.c_str(), NP_ACTIVITY_TITLE_ID_SIZE);
	sce_paf_strncpy(info->titleName, status->titleName.c_str(), sizeof(info->titleName));
	sce_paf_strncpy(info->contentId, status->contentId.c_str(), sizeof(info->contentId));
	info->creationTime = status->creationTime;
	info->updateTime = status->updateTime;
	sce_paf_strncpy(info->message, status->message.c_str(), sizeof(info->message));
	info->numOfComments = status->numOfComments;
	info->numOfVotes = status->numOfVotes;
	sce_paf_memset(info->iconUrl, 0, sizeof(info->iconUrl));
	sce_paf_memset(info->actionUrl, 0, sizeof(info->actionUrl));
	sce_paf_memset(info->reserved2, 0, sizeof(info->reserved2));
	sce_paf_strncpy(info->iconUrl, status->iconUrl.c_str(), sizeof(info->iconUrl));
	sce_paf_strncpy(info->actionUrl, status->actionUrl.c_str(), sizeof(info->actionUrl));
	info->reserved3 = 0;
	return 0;
}

#if FW_DEVKIT
/* the owner of a status of a local title (one that stays on the system) */
static const char s_activityDbLocalOwner[] = "\xe2\x98\x85" "debug_user";
#endif

/*
 * Posts a status to the network: one the network cannot take yet waits in the database to
 * be sent again. A local title's status is stored as the signed-in user's, keeping at most
 * 99 statuses of each local title; the retail build refuses local titles.
 */
int ActivityDbNetDaemon::RunPostStatus(const ActivityDbStatus *src)
{
	ActivityDbStatus status(*src);
	int connection;
	int result;

	if (ActivityDbIsLocalTitle(&src->titleId)) {
#if FW_DEVKIT
		result = SceActivityDb_FBB82B37()->TrimStatuses(0, "NPXS10998", 99);
		if (result == 0)
			result = SceActivityDb_FBB82B37()->TrimStatuses(0, "NPXS10999", 99);
		if (result == 0) {
			ActivityDbNetStatus local;

			local.status = *src;
			local.status.statusId = (int)ScePafCommon_AC2F3710();
			local.status.ownerId = s_activityDbLocalOwner;
			local.status.ownerType = ACTIVITY_DB_OWNER_SELF;
			local.status.rowid = ACTIVITY_DB_ACTIVITY_ROWID_BASE;
			if (local.status.creationTime == 0 || local.status.updateTime == 0) {
				paf::rtc::Tick now;

				paf::rtc::GetCurrentTick(&now);
				local.status.creationTime = now;
				local.status.updateTime = now;
			}
			local.status.isGetDetail = true;
			local.status.isGetInfo = true;
			local.status.announceAvatarUrl = "";
			local.status.announcePanelBgColor = ACTIVITY_DB_PANEL_BG_COLOR_DEFAULT;

			paf::vector<ActivityDbNetStatus> statuses(1, local);

			result = SceActivityDb_FBB82B37()->StoreStatuses(&statuses);
		}
#else
		result = ACTIVITY_DB_ERROR_OUT_OF_RANGE;
#endif
	} else {
		result = ActivityDbGetNetTitleIcon(&status);
		if (result == 0)
			result = m_npAccess->Begin(1, &connection);
		if (result == 0) {
			NpActivityStatusInfo *info = new NpActivityStatusInfo[1];
			NpActivityPostResult post;

			result = ActivityDbToNpStatusInfo(&status, info);
			if (result == 0) {
				result = SceNpActivityNet_5A0524AD(connection, 0, info, 1, &post);
				if (result == 0 && post.result == NP_ACTIVITY_ERROR_POST_REJECTED)
					result = NP_ACTIVITY_ERROR_POST_REJECTED;
			}
			delete[] info;
		}
		if (result != 0) {
			int kind = SceNpActivityNet_447AC73E(result);

			if (kind == NP_ACTIVITY_ERROR_KIND_RETRY) {
				result = SceActivityDb_FBB82B37()->AddWaitStatus(&status);
			} else {
				if (kind == NP_ACTIVITY_ERROR_KIND_DISCARD)
					SceActivityDb_FBB82B37()->ClearWaitStatus();
				result = ACTIVITY_DB_ERROR_POST_FAILED;
			}
		}
	}
	m_npAccess->End();
	return result;
}

/* the statuses one post request sends */
#define ACTIVITY_DB_NET_POST_MAX	5

/*
 * Sends the statuses waiting in the database to the network, five a request, and takes the
 * ones the network took out of the waiting list.
 */
int ActivityDbNetDaemon::RunSendWaitStatuses()
{
	ActivityDbQuery query;
	ActivityDbQueryCondition condition;
	int database = ACTIVITY_DB_DATABASE_ACTIVITY;
	paf::vector<SceInt64> rowids;
	paf::vector<SceInt64> batch;
	paf::vector<SceInt64> sent;
	paf::vector<ActivityDbStatus> statuses;
	int result;

	result = query.Setup(&condition, &database);
	if (result == 0) {
		sent.clear();
		result = query.GetWaitStatusRowids(&rowids);

		SceSize count = rowids.size();

		if (count != 0) {
			int batches = (int)(count - 1) / ACTIVITY_DB_NET_POST_MAX + 1;
			NpActivityStatusInfo *infos;
			NpActivityPostResult *results;
			int connection;

			for (int b = 0; b < batches; b++) {
				batch.clear();
				for (SceSize i = b * ACTIVITY_DB_NET_POST_MAX, n = 0;
				     i < count && n < ACTIVITY_DB_NET_POST_MAX; i++) {
					if (i < count) {
						batch.push_back(rowids[i]);
						n++;
					}
				}
				query.GetWaitStatuses(&batch, &statuses);

				result = 0;
				for (paf::vector<ActivityDbStatus>::iterator it = statuses.begin();
				     it != statuses.end(); ++it) {
					result = ActivityDbGetNetTitleIcon(&*it);
					if (result != 0)
						break;
				}

				int posted = statuses.size();

				results = NULL;
				infos = NULL;
				if (result != 0)
					goto fail;
				infos = new NpActivityStatusInfo[posted];
				results = new NpActivityPostResult[posted];
				for (int i = 0; i < posted; i++) {
					result = ActivityDbToNpStatusInfo(&statuses[i], &infos[i]);
					if (result != 0)
						goto fail;
				}
				result = m_npAccess->Begin(1, &connection);
				if (result != 0)
					goto fail;
				result = SceNpActivityNet_5A0524AD(connection, 0, infos, posted, results);
				if (result != 0)
					goto fail;
				m_npAccess->End();
				if (results[0].result == NP_ACTIVITY_ERROR_POST_REJECTED) {
					result = NP_ACTIVITY_ERROR_POST_REJECTED;
					goto fail;
				}
				for (int i = 0; i < posted; i++) {
					if (results[i].result == 0)
						sent.push_back(statuses[i].rowid);
					else if (SceNpActivityNet_447AC73E(result) ==
						 NP_ACTIVITY_ERROR_KIND_DISCARD)
						goto fail;
				}
				delete[] infos;
				delete[] results;
			}
			goto store;
fail:
			delete[] infos;
			delete[] results;
			m_npAccess->End();
			if (SceNpActivityNet_447AC73E(result) == NP_ACTIVITY_ERROR_KIND_DISCARD) {
				SceActivityDb_FBB82B37()->ClearWaitStatus();
				result = ACTIVITY_DB_ERROR_POST_FAILED;
			} else {
store:
				result = SceActivityDb_FBB82B37()->ClearWaitStatuses(&sent);
			}
		}
	}
	m_npAccess->End();
	return result;
}

/* deletes one of the signed-in user's statuses from the network */
int ActivityDbNetDaemon::RunDeleteStatus(const int *statusId)
{
	int connection;
	int result;

	result = m_npAccess->Begin(1, &connection);
	if (result == 0)
		result = SceNpActivityNet_DA59D2CF(connection, 0, *statusId);
	m_npAccess->End();
	return result;
}

/* posts a comment on the status at mainRowid to the network */
int ActivityDbNetDaemon::RunPostComment(const SceInt64 *mainRowid,
	const ActivityDbComment *comment)
{
	int connection;
	int result;

	result = m_npAccess->Begin(1, &connection);
	if (result == 0) {
		ActivityDbStatusKey key;
		{
			ActivityDbQuery query;
			ActivityDbQueryCondition condition;
			int database = ActivityDbGetRowidDatabase(*mainRowid);

			result = query.Setup(&condition, &database);
			if (result != 0)
				goto end;
			query.GetStatusKey(mainRowid, &key, NULL);
		}

		NpActivityStatusKey npKey;
		NpActivityCommentPost post;
		NpActivityCommentRequest request;

		result = ActivityDbToNpStatusKey(&key, &npKey);
		if (result == 0) {
			sce_paf_memset(&post, 0, sizeof(post));
			post.creationTime = comment->creationTime;
			sce_paf_strncpy(post.message, comment->message.c_str(),
				NP_ACTIVITY_COMMENT_POST_MESSAGE_SIZE);
			post.reserved = 0;
			result = SceNpActivityNet_747FBB53(connection, 0, &npKey, &post, &request);
		}
		if (result != 0)
			goto end;
	}
end:
	m_npAccess->End();
	return result;
}

/* a comment's key as the network names it */
static inline int ActivityDbToNpCommentKey(const ActivityDbCommentKey *key,
	NpActivityCommentKey *npKey)
{
	int result = ActivityDbToNpOwner(&key->status.owner, &npKey->owner);

	if (result == 0) {
		npKey->statusId = key->status.statusId;
		npKey->commentId = key->commentId;
	}
	return result;
}

/* deletes one of the signed-in user's comments from the network */
int ActivityDbNetDaemon::RunDeleteComment(const ActivityDbCommentKey *key)
{
	NpActivityCommentKey npKey;
	NpActivityRequest request;
	int connection;
	int result;

	result = m_npAccess->Begin(1, &connection);
	if (result == 0) {
		result = ActivityDbToNpCommentKey(key, &npKey);
		if (result == 0)
			result = SceNpActivityNet_92CF4044(connection, 0, &npKey, &request);
	}
	m_npAccess->End();
	return result;
}

/* gives a status a good job on the network (one already given counts as given) */
int ActivityDbNetDaemon::RunVoteGoodJob(const ActivityDbStatusKey *key)
{
	NpActivityStatusKey npKey;
	NpActivityRequest request;
	int connection;
	int result;

	result = m_npAccess->Begin(1, &connection);
	if (result == 0) {
		result = ActivityDbToNpStatusKey(key, &npKey);
		if (result == 0)
			result = SceNpActivityNet_75F6CD31(connection, 0, &npKey, &request);
		if (result == NP_ACTIVITY_ERROR_ALREADY_VOTED)
			result = 0;
	}
	m_npAccess->End();
	return result;
}

/* takes a status's good job back on the network */
int ActivityDbNetDaemon::RunCancelGoodJob(const ActivityDbStatusKey *key)
{
	NpActivityStatusKey npKey;
	NpActivityRequest request;
	int connection;
	int result;

	result = m_npAccess->Begin(1, &connection);
	if (result == 0) {
		result = ActivityDbToNpStatusKey(key, &npKey);
		if (result == 0)
			result = SceNpActivityNet_690808DA(connection, 0, &npKey, &request);
	}
	m_npAccess->End();
	return result;
}

/* the most of the signed-in user's statuses one removal lists */
#define ACTIVITY_DB_NET_REMOVE_MAX	30

/* deletes the signed-in user's newest statuses from the network */
int ActivityDbNetDaemon::RunRemoveAllMyStatuses(const ActivityDbOwner *user)
{
	NpActivityOwner *owners = NULL;
	NpActivityStatusList list;
	int connection;
	int result;

	result = m_npAccess->Begin(0, &connection);
	if (result == 0) {
		owners = new NpActivityOwner[1];
		result = ActivityDbToNpOwner(user, owners);
		if (result == 0) {
			int max = ACTIVITY_DB_NET_REMOVE_MAX;

			list.max = ACTIVITY_DB_NET_REMOVE_MAX;
			list.statuses = new NpActivityStatus[ACTIVITY_DB_NET_REMOVE_MAX];
			result = SceNpActivityNet_2000C248(connection, 0, owners, 1, &max, &list);
			if (result == 0) {
				m_npAccess->End();
				result = m_npAccess->Begin(1, &connection);
				if (result == 0) {
					for (SceSize i = 0; i < (SceSize)list.count; i++) {
						result = SceNpActivityNet_DA59D2CF(connection, 0,
							list.statuses[i].statusId);
						if (result != 0)
							break;
					}
				}
			}
		}
	}
	m_npAccess->End();
	delete[] list.statuses;
	delete[] owners;
	return result;
}

/*
 * Reads the details of the statuses the condition gives that the user has not seen, and
 * stores them (*found).
 */
int ActivityDbNetDaemon::RunGetAvatarInfo(const ActivityDbQueryCondition *condition,
	bool *found, const ActivityDbNetCallback *callback)
{
	paf::vector<ActivityDbStatusKey> keys;
	NpActivityStatusKey *npKeys;
	int connection;
	int result;

	*found = false;
	{
		ActivityDbQuery query;
		ActivityDbQueryCondition copy = *condition;

		if (query.Setup(&copy, NULL) != 0)
			return ACTIVITY_DB_ERROR_DATABASE;
		if (query.GetUnreadStatusKeys(condition, &keys) != 0)
			return ACTIVITY_DB_ERROR_NOT_FOUND;
	}

	SceSize count = keys.size();

	result = m_npAccess->Begin(0, &connection);
	npKeys = NULL;
	if (result == 0 && count != 0) {
		npKeys = new NpActivityStatusKey[count];

		paf::vector<ActivityDbNetStatus> statuses;

		for (SceSize i = 0; i < count; i++) {
			result = ActivityDbToNpStatusKey(&keys.at(i), &npKeys[i]);
			if (result != 0)
				goto failed;
		}
		result = ActivityDbGetNetStatusDetails(&connection, npKeys, count, &statuses);
		if (result == 0) {
			paf::vector<ActivityDbNetStatus>::iterator status = statuses.begin();

			for (paf::vector<ActivityDbStatusKey>::iterator key = keys.begin();
			     key != keys.end(); ++key) {
				if (status == statuses.end())
					break;
				if (key->owner.ownerType == ACTIVITY_DB_OWNER_TITLE) {
					if (ActivityDbStrEqual(status->status.titleId.c_str(),
						key->owner.titleId.c_str()) &&
					    status->status.statusId == key->statusId) {
						status->status.rowid = key->rowid;
						++status;
					}
				} else {
					if (ActivityDbStrEqual(status->status.ownerId.c_str(),
						key->owner.ownerId.c_str()) &&
					    status->status.statusId == key->statusId) {
						status->status.rowid = key->rowid;
						++status;
					}
				}
			}
			result = SceActivityDb_FBB82B37()->UpdateStatuses(&keys, &statuses);
			if (result == 0)
				*found = true;
		}
failed:
		;
	}
	m_npAccess->End();
	delete[] npKeys;
	return result;
}

/* a user's avatar and announce panel, from their profile */
int ActivityDbNetDaemon::RunGetAnnounceInfo(const ActivityDbOwner *owner,
	ActivityDbAnnounceInfo *info)
{
	SceNpId npId;
	NpLookupProfile profile;
	SceInt32 lookupResult;
	int result;

	result = SceNpActivityNet_C35984F3(owner->ownerId.c_str(), &npId);
	if (result == 0) {
		result = ActivityDbLookupNpProfiles(&npId, 1, &profile, &lookupResult);
		if (result == 0 && lookupResult == 0) {
			info->ownerId = owner->ownerId;
			info->ownerType = owner->ownerType;
			info->announceAvatarUrl = profile.icon.data;
			ActivityDbSetAnnouncePanel(&info->announcePanelBgColor,
				&info->announcePanelUrl, &profile.panel);
		}
	}
	return result;
}

void ActivityDbNetDaemon::Abort()
{
	m_npAccess->Abort();
}

/* the detail request's callback: one status's details, added to the vector at arg */
__attribute__((noinline)) void ActivityDbAddNetStatusDetail(
	const NpActivityStatusDetail *detail, void *arg)
{
	paf::vector<ActivityDbNetStatus> *statuses = (paf::vector<ActivityDbNetStatus> *)arg;

	if (detail != NULL && statuses != NULL) {
		statuses->push_back(ActivityDbNetStatus());
		if (ActivityDbFromNpStatusInfo(&detail->key, &detail->info,
			&(*statuses)[statuses->size() - 1]) != 0)
			statuses->pop_back();
	}
}

