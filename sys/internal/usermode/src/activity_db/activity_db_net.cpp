/*
 * The network daemon: it queues the requests to the NP activity service (posting statuses
 * and comments, good jobs, syncing the feed) on its thread, and brings what the network
 * answers into the activity daemon's databases.
 */
/* Sony's build of this file inlines vector's fill constructor */
#define PAF_VECTOR_FILL_INLINE
#include "activity_db_internal.h"

const char g_activityDbLocalTitle0[] = ACTIVITY_DB_LOCAL_TITLE_0;
const char g_activityDbLocalTitle1[] = ACTIVITY_DB_LOCAL_TITLE_1;

ActivityDbNpAccess ActivityDbNetDaemon::s_noNpAccess;

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
	if (result == 0)
		result = m_queue->Push(new ActivityDbNetPostStatusJob(&copy), id);
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
	result = m_queue->Push(new ActivityDbNetDeleteStatusJob(statusId, wait), id);
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
		int database = ActivityDbDatabaseOfRowid(*mainRowid);

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
	if (result == 0)
		result = m_queue->Push(new ActivityDbNetPostCommentJob(mainRowid, comment, wait),
			id);
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
	result = m_queue->Push(new ActivityDbNetDeleteCommentJob(key, wait), id);
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
	else
		result = m_queue->Push(new ActivityDbNetVoteGoodJobJob(key, wait), id);
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
	else
		result = m_queue->Push(new ActivityDbNetCancelGoodJobJob(key, wait), id);
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
	result = m_queue->Push(new ActivityDbNetSyncActivityJob(param, titleId), id);
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
	result = m_queue->Push(new ActivityDbNetSyncDetailJob(titleId, mainRowid, callback, wait),
		id);
	m_lock.Unlock();
	return result;
}

int ActivityDbNetDaemon::GetAvatarInfo(SceInt64 id, int database, int mode,
	const ActivityDbQueryCondition *condition, const ActivityDbNetCallback *callback)
{
	int result;

	m_lock.Lock();
	result = m_queue->Push(new ActivityDbNetGetAvatarInfoJob(database, mode, condition,
		callback), id);
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

int ActivityDbNetDaemon::RemoveTitleStatuses(SceInt64 id, const ActivityDbOwner *owner,
	int scope, ActivityDbJobWait *wait)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	result = m_queue->Push(new ActivityDbNetRemoveTitleStatusesJob(owner, scope, wait), id);
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
	result = m_queue->Push(new ActivityDbNetSendWaitStatusesJob, ACTIVITY_DB_NET_ID_NONE);
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
