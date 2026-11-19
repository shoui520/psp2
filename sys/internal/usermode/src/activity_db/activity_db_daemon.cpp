/*
 * The activity daemon: it opens the databases, runs the jobs that change them on its queue's
 * thread, and tells its listeners (the queries) when they changed.
 */
#include "activity_db_internal.h"

/* the schema version the daemon's databases are brought up to */
#define ACTIVITY_DB_SCHEMA_VERSION	5

ActivityDbDaemon *SceActivityDb_FBB82B37(void)
{
	static ActivityDbDaemon daemon;

	return &daemon;
}

int ActivityDbDaemon::Start(ActivityDbDatabases *databases, int priority)
{
	int result;

	m_lock.Lock();
	if (databases == NULL) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	}
	if (m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	m_queue = new ActivityDbQueue(priority);
	if (m_queue == NULL) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NO_MEMORY;
	}
	result = m_queue->Start();
	if (result != 0) {
		if (m_queue != NULL) {
			delete m_queue;
			m_queue = NULL;
		}
	} else {
		m_databases = databases;
		m_started = true;
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::OpenDatabase(const char *path, int database)
{
	ActivityDbRebuildParam param = { 0, ACTIVITY_DB_SCHEMA_VERSION, 0, 0 };

	return m_databases->Open(database, path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
		&param, true,
		database == ACTIVITY_DB_DATABASE_ACTIVITY ? ActivityDbUpgradeActivity :
							    ActivityDbUpgradeNews);
}

int ActivityDbDaemon::CloseDatabase(int database)
{
	if (m_databases == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (database >= ACTIVITY_DB_DATABASE_COUNT)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	return m_databases->Close(database);
}

void ActivityDbDaemon::Stop()
{
	int database;

	if (m_databases == NULL || !m_started)
		return;
	m_lock.Lock();
	if (m_queue != NULL) {
		m_lock.Unlock();
		m_queue->Stop();
		m_lock.Lock();
	}
	if (!m_databases->IsEmpty()) {
		for (database = 0; database < ACTIVITY_DB_DATABASE_COUNT; database++)
			m_databases->Close(database);
	}
	if (m_queue != NULL)
		delete m_queue;
	m_queue = NULL;
	m_databases = NULL;
	m_listeners.clear();
	m_started = false;
	m_lock.Unlock();
}

dbutil::Connection *ActivityDbDaemon::BeginTransaction(int database)
{
	ActivityDbAccess *access;

	if (m_databases == NULL)
		return NULL;
	access = m_databases->Get(database);
	if (access == NULL)
		return NULL;
	return access->BeginTransaction();
}

int ActivityDbDaemon::EndTransaction(int database, int result)
{
	ActivityDbAccess *access;

	if (m_databases == NULL)
		return 0;
	access = m_databases->Get(database);
	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return access->EndTransaction(result);
}

int ActivityDbDaemon::AddListener(ActivityDbListener listener, void *arg)
{
	m_listenerLock.Lock();

	ActivityDbListenerEntry entry = { listener, arg };

	m_listeners.push_back(entry);
	m_listenerLock.Unlock();
	return 0;
}

int ActivityDbDaemon::RemoveListener(void *arg)
{
	paf::list<ActivityDbListenerEntry>::iterator entry;

	m_listenerLock.Lock();
	for (entry = m_listeners.begin(); entry != m_listeners.end(); ++entry) {
		if (entry->arg == arg) {
			m_listeners.erase(entry);
			m_listenerLock.Unlock();
			return 0;
		}
	}
	m_listenerLock.Unlock();
	return ACTIVITY_DB_ERROR_NOT_FOUND;
}

/* whether the network's status is whole enough to store */
static inline int ActivityDbCheckNetStatus(const ActivityDbNetStatus *status)
{
	if (status->status.ownerId.size() == 0)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	if (status->status.ownerType == ACTIVITY_DB_OWNER_UNKNOWN)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	if (status->status.statusId == -1)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	if (status->status.type == 100)
		return ACTIVITY_DB_ERROR_INVALID_TYPE;
	if (status->status.creationTime == 0 || status->status.updateTime == 0)
		return ACTIVITY_DB_ERROR_NO_TIME;
	if (!status->status.isGetInfo)
		return 0;
	if (status->status.titleId.size() == 0)
		return ACTIVITY_DB_ERROR_NO_TITLE_ID;
	return status->status.message.size() != 0 ? 0 : ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
}

int ActivityDbDaemon::StoreStatuses(paf::vector<ActivityDbNetStatus> *statuses)
{
	paf::vector<ActivityDbNetStatus>::iterator status;
	ActivityDbJob *job;
	int result;

	if (statuses->size() == 0)
		return 0;
	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	for (status = statuses->begin(); status != statuses->end(); ++status) {
		result = ActivityDbCheckNetStatus(&*status);
		if (result != 0)
			goto out;
	}
	job = new ActivityDbStoreStatusesJob(statuses);
	if (job == NULL)
		result = ACTIVITY_DB_ERROR_NO_MEMORY;
	else
		result = m_queue->Push(m_databases->GetByRowid(statuses->at(0).status.rowid), job,
			true);
out:
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::DeleteStatus(const SceInt64 *rowid)
{
	ActivityDbJob *job;
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	if (*rowid == 0) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NO_ROWID;
	}
	job = new ActivityDbDeleteStatusJob(rowid);
	if (job == NULL)
		result = ACTIVITY_DB_ERROR_NO_MEMORY;
	else
		result = m_queue->Push(m_databases->GetByRowid(*rowid), job, true);
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::UpdateStatuses(paf::vector<ActivityDbStatusKey> *keys,
	paf::vector<ActivityDbNetStatus> *statuses)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	if (keys->size() == 0 || statuses->size() == 0) {
		m_lock.Unlock();
		return 0;
	}
	{
		ActivityDbJobWait wait("SceActMngObjUpdateStatus", NULL);
		ActivityDbJob *job = new ActivityDbUpdateStatusJob(statuses, &wait);

		if (job == NULL) {
			result = ACTIVITY_DB_ERROR_NO_MEMORY;
		} else {
			result = m_queue->Push(m_databases->GetByRowid(keys->at(0).rowid), job, true);
			if (result == 0)
				result = wait.Wait();
		}
	}
	m_lock.Unlock();
	return result;
}

/* whether a comment is whole enough to post */
static inline int ActivityDbCheckComment(const ActivityDbComment *comment)
{
	if (comment->ownerId.size() == 0)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	if (comment->ownerType == ACTIVITY_DB_OWNER_UNKNOWN)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	if (comment->rowid == 0)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	if (comment->creationTime == 0)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	if (comment->message.size() == 0)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	return 0;
}

int ActivityDbDaemon::PostComment(const SceInt64 *mainRowid, const ActivityDbComment *comment)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	{
		ActivityDbJobWait wait("SceActMngObjPostComment", NULL);

		result = ActivityDbCheckComment(comment);
		if (result == 0) {
			ActivityDbJob *job = new ActivityDbPostCommentJob(mainRowid,
				paf::vector<ActivityDbComment>(1, *comment), &wait);

			if (job == NULL) {
				result = ACTIVITY_DB_ERROR_NO_MEMORY;
			} else {
				result = m_queue->Push(m_databases->GetByRowid(*mainRowid), job,
					true);
				if (result == 0)
					result = wait.Wait();
			}
		}
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::DeleteComments(paf::vector<ActivityDbCommentKey> *keys)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	if (keys->size() == 0) {
		m_lock.Unlock();
		return 0;
	}
	{
		ActivityDbJobWait wait("SceActMngObjDeleteComment", NULL);
		ActivityDbJob *job = new ActivityDbDeleteCommentJob(keys, &wait);

		if (job == NULL) {
			result = ACTIVITY_DB_ERROR_NO_MEMORY;
		} else {
			result = m_queue->Push(m_databases->GetByRowid(keys->at(0).status.rowid), job,
				true);
			if (result == 0)
				result = wait.Wait();
		}
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::VoteGoodJob(const ActivityDbGoodJobKey *key)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	if (key->owners.size() == 0) {
		m_lock.Unlock();
		return 0;
	}
	{
		ActivityDbJobWait wait("SceActMngObjVoteGoodJob", NULL);
		ActivityDbJob *job = new ActivityDbVoteGoodJobJob(key, &wait);

		if (job == NULL) {
			result = ACTIVITY_DB_ERROR_NO_MEMORY;
		} else {
			result = m_queue->Push(m_databases->GetByRowid(key->status.rowid), job, true);
			if (result == 0)
				result = wait.Wait();
		}
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::CancelGoodJob(const ActivityDbGoodJobKey *key)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	if (key->owners.size() == 0) {
		m_lock.Unlock();
		return 0;
	}
	{
		ActivityDbJobWait wait("SceActMngObjCancelGoodJob", NULL);
		ActivityDbJob *job = new ActivityDbCancelGoodJobJob(key, &wait);

		if (job == NULL) {
			result = ACTIVITY_DB_ERROR_NO_MEMORY;
		} else {
			result = m_queue->Push(m_databases->GetByRowid(key->status.rowid), job, true);
			if (result == 0)
				result = wait.Wait();
		}
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::AddWaitStatus(const ActivityDbStatus *status)
{
	ActivityDbJob *job;
	int result = 0;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	job = new ActivityDbAddWaitStatusJob(status);
	if (job == NULL)
		result = ACTIVITY_DB_ERROR_NO_MEMORY;
	else
		m_queue->Push(m_databases->Get(ACTIVITY_DB_DATABASE_ACTIVITY), job, true);
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::ClearWaitStatus()
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	{
		ActivityDbJobWait wait("SceActMngObjClearWaitStatus", NULL);
		ActivityDbJob *job = new ActivityDbClearWaitStatusJob(&wait);

		if (job == NULL) {
			result = ACTIVITY_DB_ERROR_NO_MEMORY;
		} else {
			m_queue->Push(m_databases->Get(ACTIVITY_DB_DATABASE_ACTIVITY), job, true);
			result = wait.Wait();
		}
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::ClearWaitStatuses(paf::vector<SceInt64> *rowids)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	if (rowids->size() == 0) {
		m_lock.Unlock();
		return 0;
	}
	{
		ActivityDbJobWait wait("SceActMngObjClearWaitStatus", NULL);
		ActivityDbJob *job = new ActivityDbClearWaitStatusesJob(rowids, &wait);

		if (job == NULL) {
			result = ACTIVITY_DB_ERROR_NO_MEMORY;
		} else {
			m_queue->Push(m_databases->Get(ACTIVITY_DB_DATABASE_ACTIVITY), job, true);
			result = wait.Wait();
		}
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::SyncActivity(int type, const paf::string *titleId,
	const SceInt64 *syncedTime, paf::vector<ActivityDbNetStatus> *added,
	paf::vector<ActivityDbNetStatus> *updated, paf::vector<SceInt64> *deleted,
	SceInt64 *waitTime, int database)
{
	int result;

	if (!m_started)
		return ACTIVITY_DB_ERROR_NOT_READY;
	m_lock.Lock();
	{
		ActivityDbJobWait wait("SceActMngObjSyncActivity", waitTime);
		ActivityDbJob *job = new ActivityDbSyncActivityJob(titleId, syncedTime, deleted,
			added, updated, &wait);

		if (job == NULL) {
			result = ACTIVITY_DB_ERROR_NO_MEMORY;
		} else {
			result = m_queue->Push(m_databases->GetByType(type), job, true);
			if (result == 0)
				result = wait.Wait();
		}
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::SyncStatusDetail(const ActivityDbNetStatus *status,
	const paf::vector<ActivityDbComment> *comments,
	const paf::vector<ActivityDbCommentKey> *deletedComments,
	const ActivityDbGoodJobKey *goodJobs, const ActivityDbGoodJobKey *cancelledGoodJobs,
	bool isVoted)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	{
		ActivityDbJobWait wait("SceActMngObjSyncStatusDetail", NULL);
		ActivityDbJob *job = new ActivityDbSyncStatusDetailJob(status, comments,
			deletedComments, goodJobs, cancelledGoodJobs, isVoted, &wait);

		if (job == NULL) {
			result = ACTIVITY_DB_ERROR_NO_MEMORY;
		} else {
			result = m_queue->Push(m_databases->GetByRowid(status->status.rowid), job,
				true);
			if (result == 0)
				result = wait.Wait();
		}
	}
	m_lock.Unlock();
	return result;
}

int ActivityDbDaemon::NotifyListeners()
{
	paf::list<ActivityDbListenerEntry>::iterator entry;

	m_listenerLock.Lock();
	for (entry = m_listeners.begin(); entry != m_listeners.end(); ++entry)
		entry->listener(entry->arg);
	m_listenerLock.Unlock();
	return 0;
}

int ActivityDbDaemon::Notify()
{
	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	NotifyListeners();
	m_lock.Unlock();
	return 0;
}

int ActivityDbDaemon::DeleteTitles(int database, paf::vector<paf::string> *titleIds,
	ActivityDbJobWait *wait)
{
	ActivityDbJob *job;
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	job = new ActivityDbDeleteTitlesJob(titleIds, wait);
	if (job == NULL)
		result = ACTIVITY_DB_ERROR_NO_MEMORY;
	else
		result = m_queue->Push(m_databases->Get(database), job, true);
	if (result != 0 && wait != NULL)
		wait->Done(result);
	m_lock.Unlock();
	return result;
}

#if FW_DEVKIT
/* devkit builds only: the retail build posts no local titles to trim */
int ActivityDbDaemon::TrimStatuses(int database, const char *titleId, int keep)
{
	int count;
	int result;

	if (!m_started)
		return ACTIVITY_DB_ERROR_NOT_READY;
	count = 0;

	ActivityDbQuery query;
	ActivityDbQueryCondition condition;

	condition.type = ACTIVITY_DB_QUERY_TITLE;
	condition.titleId = titleId;
	result = query.Setup(&condition, &database);
	if (result == 0) {
		result = query.GetCount(&count, false);
		if (result == 0 && keep < count) {
			paf::vector<SceInt64> rowids;

			result = query.GetRowids(keep + 1, 0, &rowids);
			if (result == 0) {
				paf::vector<SceInt64>::iterator rowid;

				for (rowid = rowids.begin(); rowid != rowids.end(); ++rowid) {
					result = DeleteStatus(&*rowid);
					if (result != 0)
						break;
				}
			}
		}
	}
	return result;
}
#endif

/* clears the new comments' count of titleId's statuses (or of every title's, by scope) */
static __attribute__((noinline)) int ActivityDbClearNewComments(dbutil::Connection *connection,
	const paf::string *titleId, int scope)
{
	paf::string sql("UPDATE tbl_statusMainTable SET numOfNewComments=0 ");
	int result;

	switch (scope) {
	case 0:
		sql += "WHERE titleID=?1 AND numOfNewComments!=0";
		break;
	case 1:
		sql += "WHERE numOfNewComments!=0";
		break;
	default:
		return ACTIVITY_DB_ERROR_INVALID_TYPE;
	}

	dbutil::RefPtr<dbutil::Statement> statement;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	if (scope == 0) {
		result = ActivityDbBindString(&statement, 1, *titleId);
		if (result != 0)
			return result;
	}
	do
		result = statement->Step(1);
	while (result == 0);
	if (result != 1)
		return result;
	return 0;
}

int ActivityDbDaemon::UpdateBaseTime(ActivityDbAccess *access, const paf::string *titleId,
	const SceInt64 *baseTime, int scope)
{
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	{
		paf::string sql;

		if ((unsigned int)scope > 1) {
			result = ACTIVITY_DB_ERROR_INVALID_TYPE;
		} else {
			sql = "UPDATE tbl_lastSyncedTimeTable SET baseTime=? WHERE titleID=? ";

			dbutil::RefPtr<dbutil::Statement> statement;

			result = connection->Prepare(sql, &statement);
			if (result == 0) {
				result = ActivityDbBindInt64(&statement, 1, *baseTime);
				if (result == 0) {
					result = ActivityDbBindString(&statement, 2, *titleId);
					if (result == 0) {
						result = statement->Step(1);
						if (result == 1)
							result = 0;
					}
				}
			}
		}
	}
	if (result == 0)
		result = ActivityDbClearNewComments(connection, titleId, scope);
	access->EndTransaction(result);
	return result;
}

int ActivityDbDaemon::SetBaseTime(int database, const paf::string *titleId,
	const SceInt64 *baseTime, int scope)
{
	int result;

	m_lock.Lock();
	if (!m_started) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	result = UpdateBaseTime(m_databases->Get(database), titleId, baseTime, scope);
	if (result == 0)
		NotifyListeners();
	m_lock.Unlock();
	return result;
}

/* binds a flag as the integer 1 or 0 */
static inline int ActivityDbBindBool(dbutil::RefPtr<dbutil::Statement> *statement, int index,
	bool flag)
{
	if (flag)
		return ActivityDbBindInt64(statement, index, 1);
	return ActivityDbBindInt64(statement, index, 0);
}

/* binds a time as the text the database keeps, or NULL for one that has none */
static inline int ActivityDbBindTime(dbutil::RefPtr<dbutil::Statement> *statement, int index,
	const SceInt64 *time)
{
	char text[30] = "";

	SceActivityDb_9A8944FC((const SceUInt64 *)time, text, sizeof(text));
	if (text[0] != '\0') {
		dbutil::Value value = { SQLITE_TEXT };

		value.size = sce_paf_strlen(text);
		value.data.text = text;
		return (*statement)->Bind(index, &value, false);
	}

	dbutil::Value value = { SQLITE_NULL };

	return (*statement)->Bind(index, &value, false);
}

/* finds the rowid of the status the network names by its owner and ID */
static int ActivityDbFindStatusRowid(dbutil::Connection *connection, ActivityDbStatus *status)
{
	paf::string sql("SELECT rowid FROM tbl_statusMainTable WHERE ownerID=?1 AND statusID=?2");
	dbutil::RefPtr<dbutil::Statement> statement;
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 1, status->ownerId);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 2, status->statusId);
	if (result != 0)
		return result;
	while ((result = statement->Step(1)) == 0) {
		dbutil::Value value;

		result = statement->GetValue(0, &value);
		if (result != 0)
			return result;
		status->rowid = ActivityDbValueInt64(&value);
	}
	if (result == 1)
		return 0;
	return result;
}

/* writes the network's copy of a status over the database's */
static int ActivityDbUpdateStatusRow(dbutil::Connection *connection,
	const ActivityDbNetStatus *status)
{
	paf::string sql("UPDATE tbl_statusMainTable SET ownerID=?1, updateTime=?2, message=?3, "
		"isGetInfo=?4, isGetDetail=?5, numOfComments=?6, numOfNewComments=?7, "
		"numOfVotes=?8, actionURL=?9 ");
	int result;

	if (status->status.iconUrl.size() != 0)
		sql += ", iconURL=?10 ";
	sql += ", contentID=?11 ";
	if (status->isGetAnnounce)
		sql += ", announceAvatarURL=?12, announcePanelBgColor=?13, announcePanelURL=?14 ";
	sql += ", titleID=?15, titleName=?16 ";
	sql += "WHERE rowid=?17";

	dbutil::RefPtr<dbutil::Statement> statement;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 1, status->status.ownerId);
	if (result != 0)
		return result;
	result = ActivityDbBindTime(&statement, 2, &status->status.updateTime);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 3, status->status.message);
	if (result != 0)
		return result;
	result = ActivityDbBindBool(&statement, 4, status->status.isGetInfo);
	if (result != 0)
		return result;
	result = ActivityDbBindBool(&statement, 5, status->status.isGetDetail);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 6, (SceUInt32)status->status.numOfComments);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 7, (SceUInt32)status->status.numOfNewComments);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 8, (SceUInt32)status->status.numOfVotes);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 9, status->status.actionUrl);
	if (result != 0)
		return result;
	if (status->status.iconUrl.size() != 0) {
		result = ActivityDbBindString(&statement, 10, status->status.iconUrl);
		if (result != 0)
			return result;
	}
	result = ActivityDbBindString(&statement, 11, status->status.contentId);
	if (result != 0)
		return result;
	if (status->isGetAnnounce) {
		result = ActivityDbBindString(&statement, 12, status->status.announceAvatarUrl);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 13, status->status.announcePanelBgColor);
		if (result != 0)
			return result;
		result = ActivityDbBindString(&statement, 14, status->status.announcePanelUrl);
		if (result != 0)
			return result;
	}
	result = ActivityDbBindString(&statement, 15, status->status.titleId);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 16, status->status.titleName);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 17, status->status.rowid);
	if (result != 0)
		return result;
	do
		result = statement->Step(1);
	while (result == 0);
	if (result == 1)
		return 0;
	return result;
}

/* gives the statuses (the network's, which have no info) their first status's title */
static int ActivityDbSetStatusesTitle(dbutil::Connection *connection,
	paf::vector<ActivityDbNetStatus> *statuses)
{
	paf::vector<ActivityDbNetStatus>::iterator status;
	paf::string sql("UPDATE tbl_statusMainTable SET titleID=? WHERE rowid IN ( ");
	int index;
	int result;

	for (status = statuses->begin(); status != statuses->end();) {
		sql.append(1, '?');
		++status;
		if (status != statuses->end())
			sql.append(1, ',');
	}
	sql += " )";

	dbutil::RefPtr<dbutil::Statement> statement;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 1, statuses->at(0).status.titleId);
	if (result != 0)
		return result;
	index = 2;
	for (status = statuses->begin(); status != statuses->end(); ++status) {
		result = ActivityDbBindInt64(&statement, index, status->status.rowid);
		if (result != 0)
			return result;
		index++;
	}
	do
		result = statement->Step(1);
	while (result == 0);
	if (result == 1)
		return 0;
	return result;
}

/* inserts the network's statuses, the last first; the ones the database has are updated */
static int ActivityDbInsertStatuses(dbutil::Connection *connection,
	paf::vector<ActivityDbNetStatus> *statuses)
{
	paf::vector<ActivityDbNetStatus> updated;	/* stored before, with their info */
	paf::vector<ActivityDbNetStatus> retitled;	/* stored before, without it */
	paf::vector<ActivityDbNetStatus>::iterator status;
	int result;
	int i;

	{
		paf::string sql("INSERT INTO tbl_statusMainTable(ownerID, ownerType, statusID, "
			"titleID, titleName, type, creationTime, updateTime, isGetInfo, isGetDetail, "
			"message, numOfComments, numOfNewComments, numOfVotes, actionURL, iconURL, "
			"contentID, announceAvatarURL, announcePanelBgColor, announcePanelURL) "
			"VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			goto end;
		for (i = statuses->size() - 1; i >= 0; i--) {
			ActivityDbNetStatus *net = &(*statuses)[i];

			result = ActivityDbBindString(&statement, 1, net->status.ownerId);
			if (result != 0)
				goto end;
			result = ActivityDbBindInt64(&statement, 2, net->status.ownerType);
			if (result != 0)
				goto end;
			result = ActivityDbBindInt64(&statement, 3, net->status.statusId);
			if (result != 0)
				goto end;
			result = ActivityDbBindString(&statement, 4, net->status.titleId);
			if (result != 0)
				goto end;
			result = ActivityDbBindString(&statement, 5, net->status.titleName);
			if (result != 0)
				goto end;
			result = ActivityDbBindInt64(&statement, 6, net->status.type);
			if (result != 0)
				goto end;
			result = ActivityDbBindTime(&statement, 7, &net->status.creationTime);
			if (result != 0)
				goto end;
			result = ActivityDbBindTime(&statement, 8, &net->status.updateTime);
			if (result != 0)
				goto end;
			result = ActivityDbBindBool(&statement, 9, net->status.isGetInfo);
			if (result != 0)
				goto end;
			result = ActivityDbBindBool(&statement, 10, net->status.isGetDetail);
			if (result != 0)
				goto end;
			result = ActivityDbBindString(&statement, 11, net->status.message);
			if (result != 0)
				goto end;
			result = ActivityDbBindInt64(&statement, 12,
				(SceUInt32)net->status.numOfComments);
			if (result != 0)
				goto end;
			result = ActivityDbBindInt64(&statement, 13,
				(SceUInt32)net->status.numOfNewComments);
			if (result != 0)
				goto end;
			result = ActivityDbBindInt64(&statement, 14,
				(SceUInt32)net->status.numOfVotes);
			if (result != 0)
				goto end;
			result = ActivityDbBindString(&statement, 15, net->status.actionUrl);
			if (result != 0)
				goto end;
			result = ActivityDbBindString(&statement, 16, net->status.iconUrl);
			if (result != 0)
				goto end;
			result = ActivityDbBindString(&statement, 17, net->status.contentId);
			if (result != 0)
				goto end;
			result = ActivityDbBindString(&statement, 18, net->status.announceAvatarUrl);
			if (result != 0)
				goto end;
			result = ActivityDbBindInt64(&statement, 19,
				net->status.announcePanelBgColor);
			if (result != 0)
				goto end;
			result = ActivityDbBindString(&statement, 20, net->status.announcePanelUrl);
			if (result != 0)
				goto end;
			do {
				result = statement->Step(1);
				if (result == ACTIVITY_DB_SQLITE_CONSTRAINT) {
					if (net->status.isGetInfo)
						updated.push_back(*net);
					else
						retitled.push_back(*net);
					goto stored;
				}
			} while (result == 0);
			if (result != 1)
				goto end;
stored:
			(*statuses)[i].status.rowid = connection->GetLastInsertRowid();
			result = statement->ClearBindings();
			if (result != 0)
				goto end;
			result = statement->Reset();
			if (result != 0)
				goto end;
		}
	}
	for (status = updated.begin(); status != updated.end(); ++status) {
		result = ActivityDbFindStatusRowid(connection, &status->status);
		if (result != 0)
			goto end;
		result = ActivityDbUpdateStatusRow(connection, &*status);
		if (result != 0)
			goto end;
	}
	if (retitled.size() != 0) {
		for (status = retitled.begin(); status != retitled.end(); ++status) {
			result = ActivityDbFindStatusRowid(connection, &status->status);
			if (result != 0)
				goto end;
		}
		result = ActivityDbSetStatusesTitle(connection, &retitled);
	}
end:
	return result;
}

int ActivityDbDaemon::RunStoreStatuses(ActivityDbAccess *access,
	paf::vector<ActivityDbNetStatus> *statuses)
{
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbInsertStatuses(connection, statuses);
	if (result != 0 && result == ACTIVITY_DB_SQLITE_CONSTRAINT)
		result = ACTIVITY_DB_ERROR_CONFLICT;
	access->EndTransaction(result);
	return result;
}

/* deletes the statuses of rowids */
static int ActivityDbDeleteStatusRows(dbutil::Connection *connection,
	paf::vector<SceInt64> *rowids)
{
	paf::vector<SceInt64>::iterator rowid;
	paf::string sql("DELETE FROM tbl_statusMainTable WHERE rowid IN ( ");
	int index;
	int result;

	for (rowid = rowids->begin(); rowid != rowids->end();) {
		sql.append(1, '?');
		++rowid;
		if (rowid != rowids->end())
			sql.append(1, ',');
	}
	sql += " )";

	dbutil::RefPtr<dbutil::Statement> statement;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	index = 1;
	for (rowid = rowids->begin(); rowid != rowids->end(); ++rowid) {
		result = ActivityDbBindInt64(&statement, index, *rowid);
		if (result != 0)
			return result;
		index++;
	}
	do
		result = statement->Step(1);
	while (result == 0);
	if (result != 1)
		return result;
	statement->Finalize();
	return 0;
}

int ActivityDbDaemon::RunDeleteStatuses(ActivityDbAccess *access, paf::vector<SceInt64> *rowids)
{
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbDeleteStatusRows(connection, rowids);
	if (result != 0 && result == ACTIVITY_DB_SQLITE_CONSTRAINT)
		result = ACTIVITY_DB_ERROR_CONFLICT;
	access->EndTransaction(result);
	return result;
}

int ActivityDbDaemon::RunUpdateStatuses(ActivityDbAccess *access,
	paf::vector<ActivityDbNetStatus> *statuses)
{
	paf::vector<ActivityDbNetStatus>::iterator status;
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = 0;
	for (status = statuses->begin(); status != statuses->end(); ++status) {
		result = ActivityDbUpdateStatusRow(connection, &*status);
		if (result != 0)
			break;
	}
	access->EndTransaction(result);
	return result;
}

/* inserts a status's comments, the last first */
static int ActivityDbInsertComments(dbutil::Connection *connection, const SceInt64 *mainRowid,
	paf::vector<ActivityDbComment> *comments)
{
	paf::string sql("INSERT INTO tbl_commentTable(mainRowID, commentID, ownerID, ownerType, "
		"creationTime, message, announceAvatarURL, announcePanelBgColor, announcePanelURL ) "
		"VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)");
	int result;
	int i;

	for (i = comments->size() - 1; i >= 0; i--) {
		ActivityDbComment *comment = &comments->at(i);
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 1, *mainRowid);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 2, comment->commentId);
		if (result != 0)
			return result;
		result = ActivityDbBindString(&statement, 3, comment->ownerId);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 4, comment->ownerType);
		if (result != 0)
			return result;
		result = ActivityDbBindTime(&statement, 5, &comment->creationTime);
		if (result != 0)
			return result;
		result = ActivityDbBindString(&statement, 6, comment->message);
		if (result != 0)
			return result;
		result = ActivityDbBindString(&statement, 7, comment->announceAvatarUrl);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 8, comment->announcePanelBgColor);
		if (result != 0)
			return result;
		result = ActivityDbBindString(&statement, 9, comment->announcePanelUrl);
		if (result != 0)
			return result;
		do
			result = statement->Step(1);
		while (result == 0);
		if (result != 1)
			return result;
	}
	return 0;
}

int ActivityDbDaemon::RunPostComments(ActivityDbAccess *access, const SceInt64 *mainRowid,
	paf::vector<ActivityDbComment> *comments)
{
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbInsertComments(connection, mainRowid, comments);
	if (result != 0 && result == ACTIVITY_DB_SQLITE_CONSTRAINT)
		result = ACTIVITY_DB_ERROR_CONFLICT;
	access->EndTransaction(result);
	return result;
}

/* deletes the comments of keys */
static int ActivityDbDeleteCommentRows(dbutil::Connection *connection,
	paf::vector<ActivityDbCommentKey> *keys)
{
	paf::vector<ActivityDbCommentKey>::iterator key;
	paf::string sql("DELETE FROM tbl_commentTable WHERE rowid=? ");
	int result;

	for (key = keys->begin(); key != keys->end(); ++key) {
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 1, key->rowid);
		if (result != 0)
			return result;
		do
			result = statement->Step(1);
		while (result == 0);
		if (result != 1)
			return result;
	}
	return 0;
}

int ActivityDbDaemon::RunDeleteComments(ActivityDbAccess *access,
	paf::vector<ActivityDbCommentKey> *keys)
{
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbDeleteCommentRows(connection, keys);
	if (result != 0 && result == ACTIVITY_DB_SQLITE_CONSTRAINT)
		result = ACTIVITY_DB_ERROR_CONFLICT;
	access->EndTransaction(result);
	return result;
}

/* adds a status's good jobs from owners */
static int ActivityDbInsertGoodJobs(dbutil::Connection *connection, const SceInt64 *mainRowid,
	paf::vector<ActivityDbOwner> *owners)
{
	paf::vector<ActivityDbOwner>::iterator owner;

	if (owners->size() == 0)
		return 0;

	paf::string sql("INSERT INTO tbl_goodJobTable(mainRowID, ownerID, ownerType ) "
		"VALUES(?, ?, ?)");
	int result;

	for (owner = owners->begin(); owner != owners->end(); ++owner) {
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 1, *mainRowid);
		if (result != 0)
			return result;
		result = ActivityDbBindString(&statement, 2, owner->ownerId);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 3, owner->ownerType);
		if (result != 0)
			return result;
		do
			result = statement->Step(1);
		while (result == 0);
		if (result != 1)
			return result;
	}
	return 0;
}

int ActivityDbDaemon::RunVoteGoodJob(ActivityDbAccess *access, ActivityDbGoodJobKey *key)
{
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbInsertGoodJobs(connection, &key->status.rowid, &key->owners);
	access->EndTransaction(result);
	return result;
}

/* removes a status's good jobs from owners */
static int ActivityDbDeleteGoodJobs(dbutil::Connection *connection, const SceInt64 *mainRowid,
	paf::vector<ActivityDbOwner> *owners)
{
	paf::vector<ActivityDbOwner>::iterator owner;

	if (owners->size() == 0)
		return 0;

	paf::string sql("DELETE FROM tbl_goodJobTable WHERE mainRowID=? AND ownerID=?");
	int result;

	for (owner = owners->begin(); owner != owners->end(); ++owner) {
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 1, *mainRowid);
		if (result != 0)
			return result;
		result = ActivityDbBindString(&statement, 2, owner->ownerId);
		if (result != 0)
			return result;
		do
			result = statement->Step(1);
		while (result == 0);
		if (result != 1)
			return result;
	}
	return 0;
}

/* records whether the user gave a status a good job */
static int ActivityDbSetVoted(dbutil::Connection *connection, const SceInt64 *mainRowid,
	bool isVoted)
{
	paf::string sql("REPLACE INTO tbl_goodJobVotedTable(mainRowID, isVoted) VALUES(?, ?)");
	dbutil::RefPtr<dbutil::Statement> statement;
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, *mainRowid);
	if (result != 0)
		return result;
	result = ActivityDbBindBool(&statement, 2, isVoted);
	if (result != 0)
		return result;
	do
		result = statement->Step(1);
	while (result == 0);
	if (result == 1)
		return 0;
	return result;
}

int ActivityDbDaemon::RunCancelGoodJob(ActivityDbAccess *access, ActivityDbGoodJobKey *key)
{
	paf::vector<ActivityDbOwner>::iterator owner;
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbDeleteGoodJobs(connection, &key->status.rowid, &key->owners);
	if (result == 0) {
		for (owner = key->owners.begin(); owner != key->owners.end(); ++owner) {
			if (owner->ownerType == ACTIVITY_DB_OWNER_SELF) {
				result = ActivityDbSetVoted(connection, &key->status.rowid, false);
				if (result != 0)
					break;
			}
		}
	}
	access->EndTransaction(result);
	return result;
}

/* the wait statuses the database keeps, the newest */
#define ACTIVITY_DB_WAIT_STATUS_MAX	30

/* the title whose statuses' sync times follow every other title's */
#define ACTIVITY_DB_TITLE_FRIENDS	"NPXS10006"

/* stores a status the user posted that the network has not sent back yet */
static int ActivityDbInsertWaitStatus(dbutil::Connection *connection,
	const ActivityDbStatus *status)
{
	paf::string sql("INSERT INTO tbl_waitStatusTable(titleID, titleName, type, creationTime, "
		"updateTime, message, actionURL, iconURL, contentID) "
		"VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)");
	dbutil::RefPtr<dbutil::Statement> statement;
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 1, status->titleId);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 2, status->titleName);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 3, status->type);
	if (result != 0)
		return result;
	result = ActivityDbBindTime(&statement, 4, &status->creationTime);
	if (result != 0)
		return result;
	result = ActivityDbBindTime(&statement, 5, &status->updateTime);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 6, status->message);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 7, status->actionUrl);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 8, status->iconUrl);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 9, status->contentId);
	if (result != 0)
		return result;
	do
		result = statement->Step(1);
	while (result == 0);
	if (result == 1)
		return 0;
	return result;
}

int ActivityDbDaemon::RunAddWaitStatus(ActivityDbAccess *access, const ActivityDbStatus *status)
{
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbInsertWaitStatus(connection, status);
	if (result != 0) {
		if (result == ACTIVITY_DB_SQLITE_CONSTRAINT)
			result = ACTIVITY_DB_ERROR_CONFLICT;
	} else {
		paf::string sql("DELETE FROM tbl_waitStatusTable WHERE rowid IN (SELECT rowid FROM "
			"tbl_waitStatusTable ORDER BY creationTime DESC LIMIT -1 OFFSET ?)");
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result == 0) {
			result = ActivityDbBindInt64(&statement, 1, ACTIVITY_DB_WAIT_STATUS_MAX);
			if (result == 0) {
				do
					result = statement->Step(1);
				while (result == 0);
				if (result == 1)
					result = 0;
			}
		}
	}
	access->EndTransaction(result);
	return result;
}

int ActivityDbDaemon::RunClearWaitStatus(ActivityDbAccess *access)
{
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	{
		paf::string sql("DELETE FROM tbl_waitStatusTable");
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result == 0) {
			do
				result = statement->Step(1);
			while (result == 0);
			if (result == 1)
				result = 0;
		}
	}
	access->EndTransaction(result);
	return result;
}

int ActivityDbDaemon::RunClearWaitStatuses(ActivityDbAccess *access,
	paf::vector<SceInt64> *rowids)
{
	paf::vector<SceInt64>::iterator rowid;
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;

	dbutil::RefPtr<dbutil::Statement> statement;

	for (rowid = rowids->begin(); rowid != rowids->end(); ++rowid) {
		paf::string sql("DELETE FROM tbl_waitStatusTable WHERE rowid=?");

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			goto end;
		result = ActivityDbBindInt64(&statement, 1, *rowid);
		if (result != 0)
			goto end;
		do
			result = statement->Step(1);
		while (result == 0);
		if (result != 1)
			goto end;
	}
	statement->Finalize();
	result = 0;
end:
	access->EndTransaction(result);
	return result;
}

/* records the time a title's statuses were synced to, and the Friends application's with it */
static int ActivityDbUpdateSyncTimes(dbutil::Connection *connection, const paf::string *titleId,
	const SceInt64 *syncedTime)
{
	int changes;
	int result;

	{
		paf::string sql("UPDATE tbl_lastSyncedTimeTable SET lastSyncedTime=?, "
			"lastModifiedTime=? WHERE titleID=? ");
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			return result;
		result = ActivityDbBindTime(&statement, 1, syncedTime);
		if (result != 0)
			return result;
		result = ActivityDbBindTime(&statement, 2, syncedTime);
		if (result != 0)
			return result;
		result = ActivityDbBindString(&statement, 3, *titleId);
		if (result != 0)
			return result;
		result = statement->Step(1);
		if (result == 1)
			result = 0;
		changes = connection->GetChanges();
	}
	if (changes == 0) {
		paf::string sql("REPLACE INTO tbl_lastSyncedTimeTable(titleID, lastSyncedTime, "
			"lastModifiedTime) VALUES(?, ?, ?) ");
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			return result;
		result = ActivityDbBindString(&statement, 1, *titleId);
		if (result != 0)
			return result;
		result = ActivityDbBindTime(&statement, 2, syncedTime);
		if (result != 0)
			return result;
		result = ActivityDbBindTime(&statement, 3, syncedTime);
		if (result != 0)
			return result;
		do
			result = statement->Step(1);
		while (result == 0);
		if (result != 1)
			return result;
		result = 0;
	}
	if (sce_paf_strcmp(ACTIVITY_DB_TITLE_FRIENDS, titleId->c_str()) == 0)
		return result;

	paf::string sql("UPDATE tbl_lastSyncedTimeTable SET lastSyncedTime=?, lastModifiedTime=? "
		"WHERE titleID=?");
	dbutil::RefPtr<dbutil::Statement> statement;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindTime(&statement, 1, syncedTime);
	if (result != 0)
		return result;
	result = ActivityDbBindTime(&statement, 2, syncedTime);
	if (result != 0)
		return result;
	result = ActivityDbBindText(&statement, 3, ACTIVITY_DB_TITLE_FRIENDS, -1);
	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result == 1)
		result = 0;
	return result;
}

int ActivityDbDaemon::RunSyncActivity(ActivityDbAccess *access, paf::vector<SceInt64> *deleted,
	paf::vector<ActivityDbNetStatus> *added, paf::vector<ActivityDbNetStatus> *updated,
	const paf::string *titleId, const SceInt64 *syncedTime)
{
	paf::vector<ActivityDbNetStatus>::iterator status;
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (deleted->size() != 0 &&
	    (result = ActivityDbDeleteStatusRows(connection, deleted)) != 0) {
		if (result == ACTIVITY_DB_SQLITE_CONSTRAINT)
			result = ACTIVITY_DB_ERROR_CONFLICT;
	} else if (added->size() != 0 &&
		   (result = ActivityDbInsertStatuses(connection, added)) != 0) {
		if (result == ACTIVITY_DB_SQLITE_CONSTRAINT)
			result = ACTIVITY_DB_ERROR_CONFLICT;
	} else {
		if (updated->size() != 0) {
			for (status = updated->begin(); status != updated->end(); ++status) {
				result = ActivityDbUpdateStatusRow(connection, &*status);
				if (result != 0)
					goto end;
			}
		}
		result = ActivityDbUpdateSyncTimes(connection, titleId, syncedTime);
	}
end:
	access->EndTransaction(result);
	return result;
}

/* marks a title's statuses, and the Friends application's, modified at the network's time */
static int ActivityDbUpdateModifiedTime(dbutil::Connection *connection,
	const paf::string *titleId)
{
	paf::rtc::Tick now = 0;
	int result;

	result = paf::rtc::GetCurrentNetworkTick(&now);
	if (result != 0)
		return result;

	paf::string sql("UPDATE tbl_lastSyncedTimeTable SET lastModifiedTime=? "
		"WHERE titleID IN ( ?, ? )");
	dbutil::RefPtr<dbutil::Statement> statement;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindTime(&statement, 1, (const SceInt64 *)&now);
	if (result != 0)
		return result;
	result = ActivityDbBindString(&statement, 2, *titleId);
	if (result != 0)
		return result;
	result = ActivityDbBindText(&statement, 3, ACTIVITY_DB_TITLE_FRIENDS, -1);
	if (result != 0)
		return result;
	result = statement->Step(1);
	if (result == 1)
		result = 0;
	return result;
}

int ActivityDbDaemon::RunSyncStatusDetail(ActivityDbAccess *access, ActivityDbNetStatus *status,
	paf::vector<ActivityDbComment> *comments,
	paf::vector<ActivityDbCommentKey> *deletedComments, ActivityDbGoodJobKey *goodJobs,
	ActivityDbGoodJobKey *cancelledGoodJobs, bool isVoted)
{
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbDeleteCommentRows(connection, deletedComments);
	if (result == 0)
		result = ActivityDbDeleteGoodJobs(connection, &cancelledGoodJobs->status.rowid,
			&cancelledGoodJobs->owners);
	if (result == 0)
		result = ActivityDbInsertComments(connection, &status->status.rowid, comments);
	if (result == 0)
		result = ActivityDbInsertGoodJobs(connection, &goodJobs->status.rowid,
			&goodJobs->owners);
	if (result == 0)
		result = ActivityDbSetVoted(connection, &status->status.rowid, isVoted);
	if (result == 0)
		result = ActivityDbUpdateStatusRow(connection, status);
	if (result == 0)
		result = ActivityDbUpdateModifiedTime(connection, &status->status.titleId);
	access->EndTransaction(result);
	return result;
}

/* removes a table's rows of titles: the statuses' only those the titles posted */
static int ActivityDbDeleteTitleRows(dbutil::Connection *connection, const char *table,
	paf::vector<paf::string> *titleIds)
{
	paf::vector<paf::string>::iterator titleId;
	int index;
	int result;

	if (titleIds->size() == 0)
		return 0;

	dbutil::RefPtr<dbutil::Statement> statement;
	paf::string sql = paf::string("DELETE FROM ") + table + " WHERE ( ";

	titleId = titleIds->begin();
	while (titleId != titleIds->end()) {
		sql += "titleID=?";
		++titleId;
		if (titleId == titleIds->end())
			break;
		sql += " OR ";
	}
	if (sce_paf_strcmp(table, "tbl_statusMainTable") == 0) {
		char ownerType[64];

		sce_paf_snprintf(ownerType, sizeof(ownerType), "%lld",
			(SceInt64)ACTIVITY_DB_OWNER_TITLE);
		sql.append(paf::string(" ) AND ownerType=") + paf::string(ownerType));
	} else {
		sql += " )";
	}

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	index = 1;
	for (titleId = titleIds->begin(); titleId != titleIds->end(); ++titleId) {
		result = ActivityDbBindString(&statement, index, *titleId);
		if (result != 0)
			return result;
		index++;
	}
	do
		result = statement->Step(1);
	while (result == 0);
	if (result == 1) {
		statement->Finalize();
		result = 0;
	}
	return result;
}

int ActivityDbDaemon::RunDeleteTitles(ActivityDbAccess *access,
	paf::vector<paf::string> *titleIds)
{
	dbutil::Connection *connection;
	int result;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (titleIds->size() == 0)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	connection = access->BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbDeleteTitleRows(connection, "tbl_statusMainTable", titleIds);
	if (result == 0)
		result = ActivityDbDeleteTitleRows(connection, "tbl_lastSyncedTimeTable", titleIds);
	access->EndTransaction(result);
	return result;
}

/* binds the statuses' sequence */
static int ActivityDbBindStatusSequence(dbutil::RefPtr<dbutil::Statement> *statement,
	SceInt64 sequence)
{
	return ActivityDbBindInt64(statement, 1, sequence);
}

/* binds the comments' sequence */
static int ActivityDbBindCommentSequence(dbutil::RefPtr<dbutil::Statement> *statement,
	SceInt64 sequence)
{
	return ActivityDbBindInt64(statement, 1, sequence);
}

/* adds the columns the schema's versions 4 and 5 brought */
static int ActivityDbAddColumns(dbutil::Connection *connection, int version)
{
	int result = 0;

	if (version < 4) {
		const char *sql[] = {
			"ALTER TABLE tbl_statusMainTable ADD COLUMN titleName TEXT",
			"ALTER TABLE tbl_statusMainTable ADD COLUMN reserve1",
			"ALTER TABLE tbl_statusMainTable ADD COLUMN reserve2",
			"ALTER TABLE tbl_statusMainTable ADD COLUMN reserve3",
			"ALTER TABLE tbl_waitStatusTable ADD COLUMN titleName TEXT",
			"ALTER TABLE tbl_waitStatusTable ADD COLUMN reserve1",
			"ALTER TABLE tbl_waitStatusTable ADD COLUMN reserve2",
			"ALTER TABLE tbl_waitStatusTable ADD COLUMN reserve3",
		};

		result = SceActivityDb_D3277649(connection, sql, 8);
		if (result != 0)
			return result;
	}
	if (version < 5) {
		const char *sql[] = {
			"ALTER TABLE tbl_lastSyncedTimeTable ADD COLUMN lastModifiedTime TEXT",
		};

		result = SceActivityDb_D3277649(connection, sql, 1);
	}
	return result;
}

#define ACTIVITY_DB_CREATE_TABLES \
	"CREATE TABLE IF NOT EXISTS tbl_statusMainTable(rowid INTEGER PRIMARY KEY " \
		"AUTOINCREMENT, ownerID TEXT, ownerType INT, statusID INT, titleID TEXT, " \
		"type INT, creationTime TEXT, updateTime TEXT, isGetInfo INT DEFAULT 0, " \
		"isGetDetail INT DEFAULT 0, message TEXT, numOfComments INT DEFAULT 0, " \
		"numOfNewComments INT DEFAULT 0, numOfVotes INT DEFAULT 0, actionURL TEXT, " \
		"iconURL TEXT, contentID TEXT, announceAvatarURL TEXT, " \
		"announcePanelBgColor INT, announcePanelURL TEXT, UNIQUE(ownerID, statusID)); ", \
	"CREATE TABLE IF NOT EXISTS tbl_commentTable(rowid INTEGER PRIMARY KEY " \
		"AUTOINCREMENT, mainRowID INT, commentID INT, ownerID TEXT, ownerType INT, " \
		"creationTime TEXT, message TEXT, announceAvatarURL TEXT, " \
		"announcePanelBgColor INT, announcePanelURL TEXT, " \
		"UNIQUE(mainRowID, commentID)); ", \
	"CREATE TABLE IF NOT EXISTS tbl_goodJobTable(mainRowID INT, ownerID TEXT, " \
		"ownerType INT, UNIQUE(mainRowID, ownerID)); ", \
	"CREATE TABLE IF NOT EXISTS tbl_goodJobVotedTable(mainRowID INTEGER PRIMARY KEY, " \
		"isVoted INT ); ", \
	"CREATE TABLE IF NOT EXISTS tbl_lastSyncedTimeTable(titleID TEXT, " \
		"lastSyncedTime TEXT, baseTime INT, UNIQUE(titleID)) ", \
	"CREATE TABLE IF NOT EXISTS tbl_waitStatusTable(titleID TEXT, type INT, " \
		"creationTime TEXT, updateTime TEXT, message TEXT, actionURL TEXT, " \
		"iconURL TEXT, contentID TEXT ) ", \
	"CREATE TRIGGER IF NOT EXISTS tgr_removeStatusDetail AFTER DELETE ON " \
		"tbl_statusMainTable BEGIN DELETE FROM tbl_commentTable WHERE " \
		"mainRowID=old.rowid; DELETE FROM tbl_goodJobTable WHERE mainRowID=old.rowid; " \
		"DELETE FROM tbl_goodJobVotedTable WHERE mainRowID=old.rowid; END "

#define ACTIVITY_DB_CREATE_INDEXES \
	"CREATE INDEX IF NOT EXISTS idx_statusMainTable1 on " \
		"tbl_statusMainTable(titleID, creationTime DESC, rowid DESC);", \
	"CREATE INDEX IF NOT EXISTS idx_statusMainTable2 on " \
		"tbl_statusMainTable(titleID, isGetInfo, creationTime DESC, rowid DESC);", \
	"CREATE INDEX IF NOT EXISTS idx_statusMainTable3 on " \
		"tbl_statusMainTable(ownerID, statusID);", \
	"CREATE INDEX IF NOT EXISTS idx_statusMainTable4 on " \
		"tbl_statusMainTable(titleID, statusID, ownerType);", \
	"CREATE INDEX IF NOT EXISTS idx_statusMainTable5 on " \
		"tbl_statusMainTable(creationTime DESC, rowid DESC);", \
	"CREATE INDEX IF NOT EXISTS idx_commentTable on " \
		"tbl_commentTable(creationTime ASC, rowid ASC);", \
	"CREATE INDEX IF NOT EXISTS idx_goodJobTable on tbl_goodJobTable(mainRowID);", \
	"CREATE INDEX IF NOT EXISTS idx_goodJobVotedTable on " \
		"tbl_goodJobVotedTable(mainRowID);", \
	"CREATE INDEX IF NOT EXISTS idx_waitStatusTable on " \
		"tbl_waitStatusTable(creationTime);", \
	"CREATE INDEX IF NOT EXISTS idx_lastSyncedTimeTable on " \
		"tbl_lastSyncedTimeTable(titleID);"

int ActivityDbUpgradeActivity(dbutil::Connection *connection, int version)
{
	int result;
	int i;

	if (version < 3) {
		const char *tables[] = { ACTIVITY_DB_CREATE_TABLES };

		result = SceActivityDb_D3277649(connection, tables, 7);
		if (result != 0)
			return result;

		const char *indexes[] = { ACTIVITY_DB_CREATE_INDEXES };

		result = SceActivityDb_D3277649(connection, indexes, 10);
		if (result != 0)
			return result;

		const char *sequences[] = {
			"INSERT INTO sqlite_sequence VALUES('tbl_statusMainTable', ?)",
			"INSERT INTO sqlite_sequence VALUES('tbl_commentTable', ?)",
		};
		int (*binders[])(dbutil::RefPtr<dbutil::Statement> *, SceInt64) = {
			ActivityDbBindStatusSequence,
			ActivityDbBindCommentSequence,
		};

		for (i = 0; i < 2; i++) {
			dbutil::RefPtr<dbutil::Statement> statement;

			result = connection->Prepare(paf::string(sequences[i]), &statement);
			if (result != 0)
				return result;
			result = binders[i](&statement, ACTIVITY_DB_ACTIVITY_ROWID_BASE);
			if (result != 0)
				return result;
			result = ActivityDbStepAll(&statement);
			if (result != 0)
				return result;
		}
	}
	return ActivityDbAddColumns(connection, version);
}

int ActivityDbUpgradeNews(dbutil::Connection *connection, int version)
{
	int result;

	if (version < 3) {
		const char *tables[] = { ACTIVITY_DB_CREATE_TABLES };

		result = SceActivityDb_D3277649(connection, tables, 7);
		if (result != 0)
			return result;

		const char *indexes[] = { ACTIVITY_DB_CREATE_INDEXES };

		result = SceActivityDb_D3277649(connection, indexes, 10);
		if (result != 0)
			return result;
	}
	return ActivityDbAddColumns(connection, version);
}
