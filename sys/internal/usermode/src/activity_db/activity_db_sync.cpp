/*
 * The exported handle on the network: its requests (posting statuses and comments, good
 * jobs, syncing the feed) go to the network daemon under the handle's ID, and wait for it
 * where the caller needs the result. Statuses of the local titles never reach the network:
 * the activity daemon keeps them.
 */
#include "activity_db_internal.h"

/* what the network answers when a request changed (or already found) the status: its detail
   is read again (ALREADY_VOTED, and three answers nothing names) */
static inline bool ActivityDbNetReached(int result)
{
	switch (result) {
	case 0:
	case NP_ACTIVITY_ERROR_ALREADY_VOTED:
	case NP_ACTIVITY_ERROR_CONFLICT_1:
	case NP_ACTIVITY_ERROR_CONFLICT_2:
	case NP_ACTIVITY_ERROR_CONFLICT_3:
		return true;
	}
	return false;
}

ActivityDbSyncHandle::ActivityDbSyncHandle()
{
	m_sync = NULL;
	m_sync = new ActivityDbSync;
}

ActivityDbSyncHandle::~ActivityDbSyncHandle()
{
	if (m_sync != NULL)
		delete m_sync;
	m_sync = NULL;
}

void ActivityDbSyncHandle::MakeStatus(ActivityDbStatus *status, const paf::string &titleId,
	int type, const paf::string &message)
{
	new (status) ActivityDbStatus;
	status->titleId = titleId;
	status->type = type;
	status->message = message;
}

void ActivityDbSyncHandle::MakeComment(ActivityDbComment *comment, const paf::string &message)
{
	new (comment) ActivityDbComment;
	comment->message = message;
}

int ActivityDbSyncHandle::PostStatus(const ActivityDbStatus *status)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->PostStatus(status);
}

int ActivityDbSyncHandle::DeleteStatus(const SceInt64 *rowid)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->DeleteStatus(rowid);
}

int ActivityDbSyncHandle::PostComment(const SceInt64 *mainRowid, const ActivityDbComment *comment)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->PostComment(mainRowid, comment, true);
}

int ActivityDbSyncHandle::DeleteComment(const SceInt64 *rowid)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->DeleteComment(rowid, true);
}

int ActivityDbSyncHandle::VoteGoodJob(const SceInt64 *mainRowid)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->VoteGoodJob(mainRowid, true);
}

int ActivityDbSyncHandle::CancelGoodJob(const SceInt64 *mainRowid)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->CancelGoodJob(mainRowid, true);
}

int ActivityDbSyncHandle::SyncActivity(const ActivityDbSyncParam *param)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->SyncActivity(param);
}

int ActivityDbSyncHandle::SyncDetail(const SceInt64 *mainRowid, const paf::string *titleId,
	const ActivityDbNetCallback *callback)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->SyncDetail(mainRowid, titleId, callback);
}

int ActivityDbSyncHandle::GetAvatarInfo(const ActivityDbQueryCondition *condition,
	const ActivityDbNetCallback *callback)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->GetAvatarInfo(condition, callback);
}

int ActivityDbSyncHandle::Cancel()
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->Cancel();
}

int ActivityDbSyncHandle::RemoveAllMyStatuses()
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->RemoveAllMyStatuses();
}

int ActivityDbSyncHandle::RemoveTitles(paf::vector<paf::string> *titleIds)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->RemoveTitles(titleIds);
}

int ActivityDbSyncHandle::GetAnnounceInfo(ActivityDbAnnounceInfo *info, const char *ownerId)
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->GetAnnounceInfo(info, ownerId);
}

int ActivityDbSyncHandle::ClearWaitStatus()
{
	if (m_sync == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_sync->ClearWaitStatus();
}

__attribute__((noinline)) ActivityDbSync::ActivityDbSync()
{
	do
		m_id = ScePafCommon_AC2F3710();
	while (m_id == ACTIVITY_DB_NET_ID_NONE);
}

__attribute__((noinline)) int ActivityDbSync::SyncActivity(const ActivityDbSyncParam *param)
{
	paf::string titleId;
	int database;
	int result;

	if (param->condition.type == ACTIVITY_DB_QUERY_TITLE ||
	    param->condition.type == ACTIVITY_DB_QUERY_ALL) {
		if (param->condition.titleId.size() == 0)
			return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
		titleId = param->condition.titleId;
	}
	database = ActivityDbGetQueryDatabase(param->condition.type);
	if (ActivityDbIsLocalTitle(&titleId) || database == ACTIVITY_DB_DATABASE_NEWS ||
	    (result = SceActivityDb_7B68F69D()->SendWaitStatuses()) == 0)
		result = SceActivityDb_7B68F69D()->SyncActivity(m_id, param, &titleId);
	return result;
}

__attribute__((noinline))
int ActivityDbSync::GetAvatarInfo(const ActivityDbQueryCondition *condition,
	const ActivityDbNetCallback *callback)
{
	paf::vector<ActivityDbStatusKey> keys;
	int result;

	if (condition->type != ACTIVITY_DB_QUERY_TITLE || condition->titleId.size() != 0)
		result = SceActivityDb_7B68F69D()->GetAvatarInfo(m_id,
			ActivityDbGetQueryDatabase(condition->type), 0, condition, callback);
	else
		result = ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	return result;
}

__attribute__((noinline))
int ActivityDbSync::SyncDetail(const SceInt64 *mainRowid, const paf::string *titleId,
	const ActivityDbNetCallback *callback)
{
	return SceActivityDb_7B68F69D()->SyncDetail(m_id, titleId, mainRowid, callback, NULL);
}

__attribute__((noinline)) int ActivityDbSync::PostStatus(const ActivityDbStatus *status)
{
	int result;

	result = SceActivityDb_7B68F69D()->SendWaitStatuses();
	if (result != 0)
		return result;
	return ActivityDbPostStatus(status->type, status->titleId.c_str(), status->contentId.c_str(),
		status->message.c_str(), status->actionUrl.c_str(), status->iconUrl.c_str());
}

__attribute__((noinline)) int ActivityDbSync::DeleteStatus(const SceInt64 *rowid)
{
	ActivityDbStatusKey key;
	paf::string titleId;
	int result;

	{
		ActivityDbQuery query;
		ActivityDbQueryCondition condition;
		int database = ActivityDbGetRowidDatabase(*rowid);

		result = query.Setup(&condition, &database);
		if (result != 0)
			return result;
		result = query.GetStatusKey(rowid, &key, &titleId);
		if (result != 0)
			return result;
	}
	if (!ActivityDbIsLocalTitle(&titleId)) {
		ActivityDbJobWait wait("SceActDbSyncObjDeleteStatus", NULL);

		result = SceActivityDb_7B68F69D()->DeleteStatus(&key.statusId, ACTIVITY_DB_NET_ID_NONE,
			&wait);
		if (result == 0)
			result = wait.Wait();
		if (result != 0)
			return result;
	}
	return SceActivityDb_FBB82B37()->DeleteStatus(rowid);
}

__attribute__((noinline))
int ActivityDbSync::PostComment(const SceInt64 *mainRowid, const ActivityDbComment *comment,
	bool syncDetail)
{
	paf::string titleId;
	ActivityDbStatusKey key;
	int posted = 0;
	int result;

	{
		ActivityDbQuery query;
		int database = ActivityDbGetRowidDatabase(*mainRowid);

		{
			ActivityDbQueryCondition condition;

			result = query.Setup(&condition, &database);
		}
		if (result != 0)
			goto end;
		result = query.GetStatusKey(mainRowid, &key, &titleId);
		if (result != 0)
			goto end;
	}
	if (ActivityDbIsLocalTitle(&titleId)) {
		ActivityDbComment local(*comment);

		local.commentId = ScePafCommon_AC2F3710();
		result = SceActivityDb_FBB82B37()->PostComment(mainRowid, &local);
	} else {
		ActivityDbJobWait wait("SceActDbSyncObjPostComment", NULL);

		result = SceActivityDb_7B68F69D()->PostComment(m_id, mainRowid, comment, &wait);
		if (result == 0) {
			posted = wait.Wait();
			if (ActivityDbNetReached(posted) && syncDetail) {
				ActivityDbNetCallback callback = { NULL, NULL };
				ActivityDbJobWait detail("SceActDbSyncObjSyncDetail", NULL);

				result = SceActivityDb_7B68F69D()->SyncDetail(m_id, &titleId, mainRowid,
					&callback, &detail);
				if (result == 0) {
					result = detail.Wait();
					if (result != 0)
						result = ACTIVITY_DB_ERROR_SYNC_DETAIL;
				}
			}
		}
	}
end:
	return posted != 0 ? posted : result;
}

__attribute__((noinline))
int ActivityDbSync::DeleteComment(const SceInt64 *rowid, bool syncDetail)
{
	paf::string titleId;
	ActivityDbCommentKey key;
	int result;

	{
		ActivityDbQuery query;
		ActivityDbQueryCondition condition;
		int database = ActivityDbGetCommentRowidDatabase(*rowid);

		result = query.Setup(&condition, &database);
		if (result != 0)
			return result;
		result = query.GetCommentKey(rowid, &key);
		if (result != 0)
			return result;

		ActivityDbStatusKey status;

		result = query.GetStatusKey(&key.status.rowid, &status, &titleId);
		if (result != 0)
			return result;
	}
	if (ActivityDbIsLocalTitle(&titleId)) {
		ActivityDbDaemon *daemon = SceActivityDb_FBB82B37();
		paf::vector<ActivityDbCommentKey> keys(1, key);

		return daemon->DeleteComments(&keys);
	}

	ActivityDbJobWait wait("SceActDbSyncObjDeleteComment", NULL);

	result = SceActivityDb_7B68F69D()->DeleteComment(m_id, &key, &wait);
	if (result != 0)
		return result;

	int deleted = wait.Wait();

	if (!ActivityDbNetReached(deleted))
		return deleted;
	if (syncDetail) {
		ActivityDbNetCallback callback = { NULL, NULL };
		ActivityDbJobWait detail("SceActDbSyncObjSyncDetail", NULL);

		result = SceActivityDb_7B68F69D()->SyncDetail(m_id, &titleId, &key.status.rowid,
			&callback, &detail);
		if (result == 0)
			result = detail.Wait();
	}
	return result;
}

__attribute__((noinline))
int ActivityDbSync::VoteGoodJob(const SceInt64 *mainRowid, bool syncDetail)
{
	ActivityDbStatusKey key;
	paf::string titleId;
	int voted = 0;
	int result;

	{
		ActivityDbQuery query;
		ActivityDbQueryCondition condition;
		int database = ActivityDbGetRowidDatabase(*mainRowid);

		result = query.Setup(&condition, &database);
		if (result != 0)
			goto end;
		result = query.GetStatusKey(mainRowid, &key, &titleId);
		if (result != 0)
			goto end;
	}
	if (ActivityDbIsLocalTitle(&titleId)) {
		ActivityDbGoodJobKey goodJob;

		goodJob.status = key;
		{
			ActivityDbOwner user = SceActivityDb_7B68F69D()->GetUser();

			goodJob.owners.push_back(user);
		}
		result = SceActivityDb_FBB82B37()->VoteGoodJob(&goodJob);
	} else {
		ActivityDbJobWait wait("SceActDbSyncObjVoteGoodJob", NULL);

		result = SceActivityDb_7B68F69D()->VoteGoodJob(m_id, &key, &wait);
		if (result == 0) {
			voted = wait.Wait();
			if (ActivityDbNetReached(voted) && syncDetail) {
				ActivityDbNetCallback callback = { NULL, NULL };
				ActivityDbJobWait detail("SceActDbSyncObjSyncDetail", NULL);

				result = SceActivityDb_7B68F69D()->SyncDetail(m_id, &titleId, &key.rowid,
					&callback, &detail);
				if (result == 0)
					result = detail.Wait();
			}
		}
	}
end:
	return voted != 0 ? voted : result;
}

__attribute__((noinline))
int ActivityDbSync::CancelGoodJob(const SceInt64 *mainRowid, bool syncDetail)
{
	ActivityDbStatusKey key;
	paf::string titleId;
	ActivityDbGoodJobKey goodJob;
	int result;

	{
		ActivityDbQuery query;
		ActivityDbQueryCondition condition;
		int database = ActivityDbGetRowidDatabase(*mainRowid);

		result = query.Setup(&condition, &database);
		if (result != 0)
			return result;
		result = query.GetStatusKey(mainRowid, &key, &titleId);
		if (result != 0)
			return result;
		goodJob.status = key;

		ActivityDbOwner user = SceActivityDb_7B68F69D()->GetUser();

		goodJob.owners.push_back(user);
	}
	if (ActivityDbIsLocalTitle(&titleId))
		return SceActivityDb_FBB82B37()->CancelGoodJob(&goodJob);

	ActivityDbJobWait wait("SceActDbSyncObjCancelGoodJob", NULL);

	result = SceActivityDb_7B68F69D()->CancelGoodJob(m_id, &key, &wait);
	if (result != 0)
		return result;

	int cancelled = wait.Wait();

	if (!ActivityDbNetReached(cancelled))
		return cancelled;
	if (syncDetail) {
		ActivityDbNetCallback callback = { NULL, NULL };
		ActivityDbJobWait detail("SceActDbSyncObjSyncDetail", NULL);

		result = SceActivityDb_7B68F69D()->SyncDetail(m_id, &titleId, &key.rowid, &callback,
			&detail);
		if (result == 0)
			result = detail.Wait();
	}
	return result;
}

__attribute__((noinline)) int ActivityDbSync::RemoveTitles(paf::vector<paf::string> *titleIds)
{
	ActivityDbJobWait wait("SceActDbSyncObjRmvStByTitle", NULL);
	int result;

	result = SceActivityDb_FBB82B37()->DeleteTitles(ACTIVITY_DB_DATABASE_ACTIVITY, titleIds,
		&wait);
	if (result == 0)
		result = wait.Wait();
	return result;
}

__attribute__((noinline)) int ActivityDbSync::Cancel()
{
	return SceActivityDb_7B68F69D()->Cancel(m_id);
}

__attribute__((noinline))
int ActivityDbSync::GetAnnounceInfo(ActivityDbAnnounceInfo *info, const char *ownerId)
{
	ActivityDbOwner owner;
	ActivityDbOwner user = SceActivityDb_7B68F69D()->GetUser();
	int result;

	if (ownerId != NULL) {
		owner.ownerId.assign(ownerId, sce_paf_strlen(ownerId));
		owner.ownerType = ActivityDbStrEqual(user.ownerId.c_str(), owner.ownerId.c_str()) ?
			ACTIVITY_DB_OWNER_SELF : ACTIVITY_DB_OWNER_USER;
	} else {
		owner = user;
	}

	ActivityDbJobWait wait("SceActDbSyncObjGetAvatorInfo", NULL);

	result = SceActivityDb_7B68F69D()->GetAnnounceInfo(m_id, &owner, info, &wait);
	if (result == 0)
		result = wait.Wait();
	return result;
}

__attribute__((noinline)) int ActivityDbSync::ClearWaitStatus()
{
	return SceActivityDb_FBB82B37()->ClearWaitStatus();
}

__attribute__((noinline)) int ActivityDbSync::RemoveAllMyStatuses()
{
	ActivityDbJobWait wait("SceActDbSyncObjRmvAllMyStatus", NULL);
	int result;

	result = SceActivityDb_7B68F69D()->RemoveAllMyStatuses(m_id, &wait);
	if (result == 0)
		result = wait.Wait();
	return result;
}
