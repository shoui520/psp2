/*
 * The activity queries: a condition's statuses, their comments and good jobs, read from the
 * database a query of its type reads, and told by the daemon when that database changes.
 */
#include <string.h>
#include <sce_atomic.h>
#include "activity_db_internal.h"

/* the rows a query reaches by default */
#define ACTIVITY_DB_QUERY_LIMIT		100
#define ACTIVITY_DB_QUERY_NEWS_LIMIT	3000

/* the owner type a query of every title's statuses leaves out */
#define ACTIVITY_DB_QUERY_OWNER_TYPE_HIDDEN	0x100000000LL

/* the statuses a read of the recent ones reaches */
#define ACTIVITY_DB_RECENT_LIMIT	20

/* which recent statuses are read */
#define ACTIVITY_DB_RECENT_TITLE	0	/* a title's */
#define ACTIVITY_DB_RECENT_ALL		1	/* every title's */

/* the statuses a query of every title's keeps; past the most, the oldest beyond the kept go */
#define ACTIVITY_DB_SYNC_MAX		600
#define ACTIVITY_DB_SYNC_KEEP		400

/* the Friends application, whose statuses a title's query reads with the title's */
#define ACTIVITY_DB_FRIENDS_TITLE_ID	"NPXS10006"

static void ActivityDbQueryOnChange(void *arg);
static int ActivityDbGetRecentStatuses(int database, paf::vector<ActivityDbStatus> *statuses,
	bool *hasNew, const paf::string *titleId, const SceUInt64 *since, int max, int scope);

/* the user's own entries take the user's avatar and announce panel */
template<class _Ty>
static inline void ActivityDbSetSelfPanel(paf::vector<_Ty> *entries,
	const NpManagerAccountInfo *info)
{
	typename paf::vector<_Ty>::iterator entry;

	for (entry = entries->begin(); entry != entries->end(); ++entry) {
		if (entry->ownerType != ACTIVITY_DB_OWNER_SELF)
			continue;
		entry->announceAvatarUrl = info->avatarUrl.data;
		switch (info->panelType) {
		case NP_MANAGER_PANEL_COLOR:
			entry->announcePanelBgColor = info->panelBgc;
			break;
		case NP_MANAGER_PANEL_IMAGE:
			entry->announcePanelBgColor = info->panelBgc;
			entry->announcePanelUrl = info->panelUrl;
			break;
		default:
			entry->announcePanelBgColor = ACTIVITY_DB_PANEL_BG_COLOR_DEFAULT;
			entry->announcePanelUrl.clear();
			break;
		}
	}
}

ActivityDbQueryHandle::ActivityDbQueryHandle()
	: m_query(NULL)
{
}

int ActivityDbQueryHandle::Destroy()
{
	if (m_query == NULL)
		return 0;
	m_query->Unregister();
	delete m_query;
	m_query = NULL;
	return 0;
}

ActivityDbQueryHandle::~ActivityDbQueryHandle()
{
	if (m_query != NULL) {
		m_query->Unregister();
		delete m_query;
		m_query = NULL;
	}
}

int ActivityDbQueryHandle::Create(const ActivityDbQueryCondition *condition)
{
	if (m_query != NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	m_query = new ActivityDbQuery;
	return m_query->Setup(condition, NULL);
}

int ActivityDbQueryHandle::GetInfoCount(int *count)
{
	if (m_query == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_query->GetCount(count, true);
}

int ActivityDbQueryHandle::GetCountAndSyncTimes(int *count, SceInt64 *baseTime,
	SceInt64 *lastSyncedTime, SceInt64 *lastModifiedTime)
{
	int result;

	if (m_query == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = m_query->GetCount(count, false);
	if (result == 0 && baseTime != NULL && lastSyncedTime != NULL &&
	    lastModifiedTime != NULL)
		return m_query->GetSyncTimes(baseTime, lastSyncedTime, lastModifiedTime);
	return result;
}

int ActivityDbQueryHandle::GetCount(int *count)
{
	if (m_query == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_query->GetCount(count, false);
}

int ActivityDbQueryHandle::GetRowids(int offset, int count, paf::vector<SceInt64> *rowids)
{
	if (m_query == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_query->GetRowids(offset, count, rowids);
}

int ActivityDbQueryHandle::GetStatuses(paf::vector<SceInt64> *rowids,
	paf::vector<ActivityDbStatus> *statuses)
{
	if (m_query == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_query->GetStatuses(rowids, statuses);
}

int ActivityDbQueryHandle::GetCommentRowids(const SceInt64 *mainRowid, int offset, int count,
	paf::vector<SceInt64> *rowids)
{
	if (m_query == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_query->GetCommentRowids(mainRowid, offset, count, rowids);
}

int ActivityDbQueryHandle::GetComments(paf::vector<SceInt64> *rowids,
	paf::vector<ActivityDbComment> *comments)
{
	if (m_query == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_query->GetComments(rowids, comments);
}

int ActivityDbQueryHandle::GetGoodJob(const SceInt64 *mainRowid, ActivityDbGoodJob *goodJob)
{
	if (m_query == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_query->GetGoodJob(mainRowid, goodJob);
}

bool ActivityDbQueryHandle::IsChanged()
{
	if (m_query == NULL)
		return false;
	return m_query->IsChanged();
}

int ActivityDbQueryHandle::ClearChanged()
{
	if (m_query == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	return m_query->ClearChanged();
}

int SceActivityDb_6268A990(paf::vector<ActivityDbStatus> *statuses, bool *hasNew,
	const paf::string *titleId, const SceUInt64 *since, int max, int scope)
{
	return ActivityDbGetRecentStatuses(0, statuses, hasNew, titleId, since, max, scope);
}

int SceActivityDb_C37F81FE(const paf::string *titleId, const SceInt64 *baseTime, int scope)
{
	return ActivityDbSetBaseTime(0, titleId, baseTime, scope);
}

__attribute__((noinline)) ActivityDbQuery::ActivityDbQuery()
	: m_where((const char *)NULL, 0), m_changed(0), m_registered(false), m_database(2)
{
}

__attribute__((noinline)) int ActivityDbQuery::Unregister()
{
	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	SceActivityDb_FBB82B37()->RemoveListener(this);
	m_registered = false;
	return 0;
}

ActivityDbQuery::~ActivityDbQuery()
{
	if (m_registered) {
		SceActivityDb_FBB82B37()->RemoveListener(this);
		m_registered = false;
	}
}

/* the condition's WHERE clause */
static int ActivityDbQueryBuildWhere(paf::string *where,
	const ActivityDbQueryCondition *condition)
{
	bool joined = false;

	where->clear();
	if (condition->ownerId.size() != 0) {
		*where = "WHERE ";
		*where += "(ownerID='";
		where->append(condition->ownerId);
		*where += "') ";
		joined = true;
	}
	if (condition->titleId.size() != 0 && condition->type != ACTIVITY_DB_QUERY_ALL) {
		if (joined)
			*where += "AND ";
		else
			*where = "WHERE ";
		*where += "(titleID='";
		where->append(condition->titleId);
		*where += "') ";
		joined = true;
	}
	if (condition->type == ACTIVITY_DB_QUERY_ALL) {
		char text[64];

		if (joined)
			*where += "AND ";
		else
			*where = "WHERE ";
		sce_paf_snprintf(text, sizeof(text), "%lld", ACTIVITY_DB_QUERY_OWNER_TYPE_HIDDEN);

		paf::string ownerType(text);

		where->append("(ownerType!=" + ownerType + ") ");
	}
	return 0;
}

int ActivityDbQuery::Setup(const ActivityDbQueryCondition *condition, const int *database)
{
	int limit;
	int result;

	if (m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	limit = condition->limit;
	if (limit <= 0) {
		switch (condition->type) {
		case ACTIVITY_DB_QUERY_TITLE:
		case ACTIVITY_DB_QUERY_ALL:
		case ACTIVITY_DB_QUERY_TITLE_ONLY:
			if (condition->titleId.size() != 0)
				m_titleId = condition->titleId;
			limit = ACTIVITY_DB_QUERY_LIMIT;
			break;
		case ACTIVITY_DB_QUERY_NEWS:
			limit = ACTIVITY_DB_QUERY_NEWS_LIMIT;
			break;
		default:
			limit = ACTIVITY_DB_QUERY_LIMIT;
			break;
		}
	}
	m_condition.type = condition->type;
	m_condition.ownerId = condition->ownerId;
	m_condition.titleId = condition->titleId;
	m_condition.limit = limit;
	ActivityDbQueryBuildWhere(&m_where, &m_condition);
	if (database != NULL)
		m_database = *database;
	else
		m_database = ActivityDbQueryDatabase(condition->type);
	result = SceActivityDb_FBB82B37()->AddListener(ActivityDbQueryOnChange, this);
	if (result == 0)
		m_registered = true;
	return result;
}

int ActivityDbQuery::GetCount(int *count, bool infoOnly)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (count == NULL)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	*count = 0;
	{
		paf::string sql = "SELECT count(rowid) FROM tbl_statusMainTable " + m_where;

		if (infoOnly) {
			sql += m_where.size() != 0 ? " AND " : " WHERE ";
			sql += "isGetInfo=?";
		}

		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result == 0 && infoOnly)
			result = ActivityDbBindInt64(&statement, 1, 1);
		if (result == 0) {
			while ((result = statement->Step(1)) == 0) {
				dbutil::Value value;

				result = statement->GetValue(0, &value);
				if (result != 0)
					break;
				*count = ActivityDbValueInt(&value);
			}
			if (result == 1)
				result = 0;
		}
	}
	if (*count > m_condition.limit)
		*count = m_condition.limit;
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

int ActivityDbQuery::GetSyncTimes(SceInt64 *baseTime, SceInt64 *lastSyncedTime,
	SceInt64 *lastModifiedTime)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	{
		paf::string sql;

		switch (m_condition.type) {
		case ACTIVITY_DB_QUERY_TITLE:
			sql = "SELECT title.lastSyncedTime, MAX(read_max.baseTime), "
			      "MAX(read_max.lastModifiedTime) FROM tbl_lastSyncedTimeTable title, "
			      "(SELECT baseTime ,lastModifiedTime FROM tbl_lastSyncedTimeTable "
			      "WHERE titleID=?1 OR titleID=?2) read_max WHERE titleID=?3";
			break;
		case ACTIVITY_DB_QUERY_ALL:
			sql = "SELECT title.lastSyncedTime, title.baseTime, "
			      "MAX(read_max.lastModifiedTime) FROM tbl_lastSyncedTimeTable title, "
			      "(SELECT lastModifiedTime FROM tbl_lastSyncedTimeTable) read_max "
			      "WHERE titleID=?1";
			break;
		default:
			sql = "SELECT lastSyncedTime, baseTime, lastModifiedTime "
			      "FROM tbl_lastSyncedTimeTable WHERE titleID=?1";
			break;
		}

		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result == 0) {
			if (m_condition.type == ACTIVITY_DB_QUERY_TITLE) {
				result = ActivityDbBindText(&statement, 1, ACTIVITY_DB_FRIENDS_TITLE_ID,
							    -1);
				if (result == 0)
					result = ActivityDbBindString(&statement, 2, m_titleId);
				if (result == 0)
					result = ActivityDbBindString(&statement, 3, m_titleId);
			} else {
				result = ActivityDbBindString(&statement, 1, m_titleId);
			}
		}
		if (result == 0) {
			while ((result = statement->Step(1)) == 0) {
				dbutil::Value value;

				result = statement->GetValue(0, &value);
				if (result != 0)
					break;
				*lastSyncedTime = ActivityDbValueTime(&value);
				result = statement->GetValue(1, &value);
				if (result != 0)
					break;
				*baseTime = ActivityDbValueInt64(&value);
				result = statement->GetValue(2, &value);
				if (result != 0)
					break;
				*lastModifiedTime = ActivityDbValueTime(&value);
				if (*lastModifiedTime == 0)
					*lastModifiedTime = *lastSyncedTime;
			}
			if (result == (int)DBUTIL_ERROR_NOT_FOUND)
				result = ACTIVITY_DB_ERROR_NOT_FOUND;
			else if (result == 1)
				result = 0;
		}
	}
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

int ActivityDbQuery::GetRowids(int offset, int count, paf::vector<SceInt64> *rowids)
{
	dbutil::Connection *connection;
	int limit;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	limit = -1;
	if (count > 0)
		limit = count;
	rowids->clear();
	{
		paf::string sql = "SELECT rowid FROM tbl_statusMainTable " + m_where;

		sql += m_where.size() != 0 ? " AND " : " WHERE ";
		sql += "isGetInfo=?1 ORDER BY creationTime DESC, rowid DESC LIMIT ?2 OFFSET ?3 ";

		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result == 0)
			result = ActivityDbBindInt64(&statement, 1, 1);
		if (result == 0)
			result = ActivityDbBindInt64(&statement, 2, limit);
		if (result == 0)
			result = ActivityDbBindInt64(&statement, 3, offset);
		if (result == 0) {
			while ((result = statement->Step(1)) == 0) {
				dbutil::Value value;

				result = statement->GetValue(0, &value);
				if (result != 0)
					break;
				rowids->push_back(ActivityDbValueInt64(&value));
			}
			if (result == 1)
				result = 0;
		}
	}
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/* reads the statuses of rowids into statuses, one for each */
static int ActivityDbReadStatuses(dbutil::Connection *connection,
	paf::vector<SceInt64> *rowids, paf::vector<ActivityDbStatus> *statuses)
{
	paf::vector<SceInt64>::iterator rowid = rowids->begin();
	paf::vector<ActivityDbStatus>::iterator status = statuses->begin();
	int result;

	for (; rowid != rowids->end(); ++rowid, ++status) {
		if (status == statuses->end())
			break;

		paf::string sql("SELECT statusID, ownerID, ownerType, titleID, titleName, type, "
				"creationTime, updateTime, isGetInfo, isGetDetail, message, "
				"numOfComments, numOfNewComments, numOfVotes, actionURL, iconURL, "
				"contentID, announceAvatarURL, announcePanelBgColor, announcePanelURL "
				"FROM tbl_statusMainTable WHERE rowid=?");
		dbutil::RefPtr<dbutil::Statement> statement;
		bool found;

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 1, *rowid);
		if (result != 0)
			return result;
		found = false;
		while ((result = statement->Step(1)) == 0) {
			int count;
			int i;

			found = true;
			status->rowid = *rowid;
			count = statement->m_columnCount;
			for (i = 0; i < count; i++) {
				dbutil::Value value;

				result = statement->GetValue(i, &value);
				if (result != 0)
					return result;
				if (sce_paf_strcmp("statusID", value.name) == 0)
					status->statusId = ActivityDbValueInt(&value);
				else if (sce_paf_strcmp("ownerID", value.name) == 0)
					status->ownerId = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("ownerType", value.name) == 0)
					status->ownerType = ActivityDbValueInt(&value);
				else if (sce_paf_strcmp("titleID", value.name) == 0)
					status->titleId = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("titleName", value.name) == 0)
					status->titleName = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("type", value.name) == 0)
					status->type = ActivityDbValueInt(&value);
				else if (sce_paf_strcmp("creationTime", value.name) == 0)
					status->creationTime = ActivityDbValueTime(&value);
				else if (sce_paf_strcmp("updateTime", value.name) == 0)
					status->updateTime = ActivityDbValueTime(&value);
				else if (sce_paf_strcmp("isGetInfo", value.name) == 0)
					status->isGetInfo = ActivityDbValueInt(&value) != 0;
				else if (sce_paf_strcmp("isGetDetail", value.name) == 0)
					status->isGetDetail = ActivityDbValueInt(&value) != 0;
				else if (sce_paf_strcmp("message", value.name) == 0)
					status->message = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("numOfComments", value.name) == 0)
					status->numOfComments = ActivityDbValueInt(&value);
				else if (sce_paf_strcmp("numOfNewComments", value.name) == 0)
					status->numOfNewComments = ActivityDbValueInt(&value);
				else if (sce_paf_strcmp("numOfVotes", value.name) == 0)
					status->numOfVotes = ActivityDbValueInt(&value);
				else if (sce_paf_strcmp("actionURL", value.name) == 0)
					status->actionUrl = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("iconURL", value.name) == 0)
					status->iconUrl = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("contentID", value.name) == 0)
					status->contentId = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("announceAvatarURL", value.name) == 0)
					status->announceAvatarUrl = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("announcePanelBgColor", value.name) == 0)
					status->announcePanelBgColor = ActivityDbValueInt(&value);
				else if (sce_paf_strcmp("announcePanelURL", value.name) == 0)
					status->announcePanelUrl = ActivityDbValueText(&value);
			}
		}
		if (result != 1)
			return result;
		if (!found)
			return ACTIVITY_DB_ERROR_NOT_FOUND;
	}
	return 0;
}

int ActivityDbQuery::GetStatuses(paf::vector<SceInt64> *rowids,
	paf::vector<ActivityDbStatus> *statuses)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	statuses->clear();
	if (rowids->size() == 0)
		return 0;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	statuses->resize(rowids->size());
	result = ActivityDbReadStatuses(connection, rowids, statuses);
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	if (result == 0) {
		NpManagerAccountInfo info;

		result = SceNpManager_E45E4003(&info);
		if (result == 0) {
			ActivityDbSetSelfPanel(statuses, &info);
			result = 0;
		}
	}
	return result;
}

int ActivityDbQuery::GetCommentRowids(const SceInt64 *mainRowid, int offset, int count,
	paf::vector<SceInt64> *rowids)
{
	dbutil::Connection *connection;
	int limit;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (*mainRowid == 0)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	limit = -1;
	if (count > 0)
		limit = count;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	rowids->clear();
	{
		paf::string sql("SELECT rowid FROM tbl_commentTable WHERE mainRowID=? "
				"ORDER BY creationTime ASC, rowid ASC LIMIT ? OFFSET ?");
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result == 0)
			result = ActivityDbBindInt64(&statement, 1, *mainRowid);
		if (result == 0)
			result = ActivityDbBindInt64(&statement, 2, limit);
		if (result == 0)
			result = ActivityDbBindInt64(&statement, 3, offset);
		if (result == 0) {
			while ((result = statement->Step(1)) == 0) {
				dbutil::Value value;

				result = statement->GetValue(0, &value);
				if (result != 0)
					break;
				rowids->push_back(ActivityDbValueInt64(&value));
			}
			if (result == 1)
				result = 0;
		}
	}
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/* reads the comment at rowid */
static int ActivityDbReadComment(dbutil::Connection *connection, const SceInt64 *rowid,
	ActivityDbComment *comment)
{
	paf::string sql("SELECT commentID, ownerID, ownerType, creationTime, message, "
			"announceAvatarURL, announcePanelBgColor, announcePanelURL "
			"FROM tbl_commentTable WHERE rowid=?");
	dbutil::RefPtr<dbutil::Statement> statement;
	bool found;
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, *rowid);
	if (result != 0)
		return result;
	found = false;
	while ((result = statement->Step(1)) == 0) {
		int count;
		int i;

		found = true;
		comment->rowid = *rowid;
		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("commentID", value.name) == 0)
				comment->commentId = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("ownerID", value.name) == 0)
				comment->ownerId = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("ownerType", value.name) == 0)
				comment->ownerType = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("creationTime", value.name) == 0)
				comment->creationTime = ActivityDbValueTime(&value);
			else if (sce_paf_strcmp("message", value.name) == 0)
				comment->message = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("announceAvatarURL", value.name) == 0)
				comment->announceAvatarUrl = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("announcePanelBgColor", value.name) == 0)
				comment->announcePanelBgColor = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("announcePanelURL", value.name) == 0)
				comment->announcePanelUrl = ActivityDbValueText(&value);
		}
	}
	if (result == 1) {
		result = 0;
		if (!found)
			result = ACTIVITY_DB_ERROR_NOT_FOUND;
	}
	return result;
}

int ActivityDbQuery::GetComments(paf::vector<SceInt64> *rowids,
	paf::vector<ActivityDbComment> *comments)
{
	dbutil::Connection *connection;
	int result;
	unsigned int i;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (rowids->size() == 0)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	comments->clear();
	result = 0;
	for (i = 0; i < rowids->size(); i++) {
		ActivityDbComment comment;

		result = ActivityDbReadComment(connection, &(*rowids)[i], &comment);
		if (result == ACTIVITY_DB_ERROR_NOT_FOUND)
			result = 0;
		else if (result != 0)
			break;
		comments->push_back(comment);
	}
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	if (result == 0) {
		NpManagerAccountInfo info;

		result = SceNpManager_E45E4003(&info);
		if (result == 0) {
			ActivityDbSetSelfPanel(comments, &info);
			result = 0;
		}
	}
	return result;
}

/* reads who gave the status at mainRowid good jobs */
static int ActivityDbReadGoodJobOwners(dbutil::Connection *connection, SceInt64 mainRowid,
	ActivityDbGoodJob *goodJob)
{
	paf::string sql("SELECT ownerID, ownerType FROM tbl_goodJobTable WHERE mainRowID=? "
			"ORDER BY rowid DESC");
	dbutil::RefPtr<dbutil::Statement> statement;
	bool found;
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, mainRowid);
	if (result != 0)
		return result;
	goodJob->mainRowid = mainRowid;
	found = false;
	while ((result = statement->Step(1)) == 0) {
		ActivityDbGoodJobOwner owner;
		int count;
		int i;

		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("ownerID", value.name) == 0)
				owner.ownerId = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("ownerType", value.name) == 0)
				owner.ownerType = ActivityDbValueInt(&value);
		}
		goodJob->ownerIds.push_back(owner.ownerId);
		found = true;
	}
	if (result == 1) {
		result = 0;
		if (!found)
			result = ACTIVITY_DB_ERROR_NOT_FOUND;
	}
	return result;
}

/* whether the user gave the status at mainRowid a good job */
static int ActivityDbReadIsVoted(dbutil::Connection *connection, SceInt64 mainRowid,
	bool *isVoted)
{
	paf::string sql("SELECT isVoted FROM tbl_goodJobVotedTable WHERE mainRowID=?");
	dbutil::RefPtr<dbutil::Statement> statement;
	bool found;
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, mainRowid);
	if (result != 0)
		return result;
	*isVoted = false;
	found = false;
	while ((result = statement->Step(1)) == 0) {
		int count;
		int i;

		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("isVoted", value.name) == 0)
				*isVoted = ActivityDbValueInt(&value) != 0;
		}
		found = true;
	}
	if (result == 1) {
		result = 0;
		if (!found)
			result = ACTIVITY_DB_ERROR_NOT_FOUND;
	}
	return result;
}

int ActivityDbQuery::GetGoodJob(const SceInt64 *mainRowid, ActivityDbGoodJob *goodJob)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (*mainRowid == 0)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;

	paf::vector<ActivityDbStatus> statuses;
	paf::vector<SceInt64> rowids;

	statuses.push_back(ActivityDbStatus());
	rowids.push_back(*mainRowid);
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbReadGoodJobOwners(connection, *mainRowid, goodJob);
	if (result == ACTIVITY_DB_ERROR_NOT_FOUND || result == 0) {
		bool isVoted = false;

		result = ActivityDbReadIsVoted(connection, *mainRowid, &isVoted);
		goodJob->isVoted = isVoted;
		if (result == 0) {
			result = ActivityDbReadStatuses(connection, &rowids, &statuses);
			if (result == 0)
				goodJob->numOfVotes = statuses[0].numOfVotes;
		}
	}
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

__attribute__((noinline)) bool ActivityDbQuery::IsChanged()
{
	return m_changed != 0;
}

__attribute__((noinline)) int ActivityDbQuery::ClearChanged()
{
	sceAtomicExchange32AcqRel((volatile int32_t *)&m_changed, 0);
	return 0;
}

/* the base time of titleId's last sync, 0 when it never synced */
int ActivityDbQuery::GetBaseTime(const paf::string *titleId, SceInt64 *baseTime)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (baseTime == NULL)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	{
		paf::string sql("SELECT baseTime FROM tbl_lastSyncedTimeTable WHERE titleID=?");
		dbutil::RefPtr<dbutil::Statement> statement;
		bool found = false;

		result = connection->Prepare(sql, &statement);
		if (result == 0)
			result = ActivityDbBindString(&statement, 1, *titleId);
		if (result == 0) {
			while ((result = statement->Step(1)) == 0) {
				dbutil::Value value;

				found = true;
				result = statement->GetValue(0, &value);
				if (result != 0)
					break;
				*baseTime = ActivityDbValueInt64(&value);
			}
			if (result == 1) {
				if (!found)
					*baseTime = 0;
				result = 0;
			}
		}
	}
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/*
 * reads the database's counts of statuses' comments and good jobs, and sets what each status
 * adds to them; its new comments are the added ones, with the database's unless
 * keepNewComments
 */
int ActivityDbQuery::UpdateCounts(int keepNewComments, paf::vector<ActivityDbNetStatus> *statuses)
{
	paf::vector<ActivityDbNetStatus>::iterator status;
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (statuses->size() == 0)
		return 0;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = 0;
	for (status = statuses->begin(); status != statuses->end(); ++status) {
		paf::string sql("SELECT numOfComments, numOfNewComments, numOfVotes  "
				"FROM tbl_statusMainTable WHERE ");
		dbutil::RefPtr<dbutil::Statement> statement;
		int numOfComments;
		int numOfNewComments;
		int numOfVotes;
		bool found;

		if (status->status.ownerType == ACTIVITY_DB_OWNER_TITLE)
			sql += "titleID=?1 AND statusID=?2 AND ownerType=?3";
		else
			sql += "ownerID=?1 AND statusID=?2";
		result = connection->Prepare(sql, &statement);
		if (result != 0)
			break;
		if (status->status.ownerType == ACTIVITY_DB_OWNER_TITLE) {
			result = ActivityDbBindString(&statement, 1, status->status.titleId);
			if (result != 0)
				break;
			result = ActivityDbBindInt64(&statement, 2, status->status.statusId);
			if (result != 0)
				break;
			result = ActivityDbBindInt64(&statement, 3, status->status.ownerType);
			if (result != 0)
				break;
		} else {
			result = ActivityDbBindString(&statement, 1, status->status.ownerId);
			if (result != 0)
				break;
			result = ActivityDbBindInt64(&statement, 2, status->status.statusId);
			if (result != 0)
				break;
		}
		found = false;
		numOfComments = 0;
		numOfNewComments = 0;
		numOfVotes = 0;
		while ((result = statement->Step(1)) == 0) {
			int count;
			int i;

			found = true;
			count = statement->m_columnCount;
			for (i = 0; i < count; i++) {
				dbutil::Value value;

				result = statement->GetValue(i, &value);
				if (result != 0)
					break;
				if (sce_paf_strcmp("numOfComments", value.name) == 0)
					numOfComments = ActivityDbValueInt(&value);
				else if (sce_paf_strcmp("numOfNewComments", value.name) == 0)
					numOfNewComments = ActivityDbValueInt(&value);
				else if (sce_paf_strcmp("numOfVotes", value.name) == 0)
					numOfVotes = ActivityDbValueInt(&value);
			}
			if (result != 0)
				break;
		}
		if (result != 1)
			break;
		if (found) {
			int added;

			status->addedVotes = numOfVotes < status->status.numOfVotes ?
					     status->status.numOfVotes - numOfVotes : 0;
			added = status->status.numOfComments > numOfComments ?
				status->status.numOfComments - numOfComments : 0;
			status->addedComments = added;
			if (!keepNewComments)
				added += numOfNewComments;
			status->status.numOfNewComments = added;
		} else {
			status->addedComments = status->status.numOfComments;
			status->addedVotes = status->status.numOfVotes;
			status->status.numOfNewComments = status->status.numOfComments;
		}
		result = 0;
	}
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/* reads what names the status at rowid; titleId, when given, takes its title too */
static __attribute__((noinline)) int ActivityDbReadStatusKey(dbutil::Connection *connection,
	const SceInt64 *rowid, ActivityDbStatusKey *key, paf::string *titleId)
{
	paf::string sql("SELECT ownerID, ownerType, statusID, titleID FROM tbl_statusMainTable "
			"WHERE rowid=?");
	dbutil::RefPtr<dbutil::Statement> statement;
	bool found;
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, *rowid);
	if (result != 0)
		return result;
	found = false;
	while ((result = statement->Step(1)) == 0) {
		int count;
		int i;

		found = true;
		key->rowid = *rowid;
		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("ownerID", value.name) == 0) {
				key->ownerId = ActivityDbValueText(&value);
			} else if (sce_paf_strcmp("ownerType", value.name) == 0) {
				key->ownerType = ActivityDbValueInt(&value);
			} else if (sce_paf_strcmp("statusID", value.name) == 0) {
				key->statusId = ActivityDbValueInt(&value);
			} else if (sce_paf_strcmp("titleID", value.name) == 0) {
				key->titleId = ActivityDbValueText(&value);
				if (titleId != NULL)
					*titleId = ActivityDbValueText(&value);
			}
		}
	}
	if (result != 1)
		return result;
	if (!found)
		return ACTIVITY_DB_ERROR_NOT_FOUND;
	return 0;
}

int ActivityDbQuery::GetStatusKey(const SceInt64 *rowid, ActivityDbStatusKey *key,
	paf::string *titleId)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbReadStatusKey(connection, rowid, key, titleId);
	if (result == (int)(DBUTIL_ERROR_SQLITE | SQLITE_CONSTRAINT))
		result = ACTIVITY_DB_ERROR_CONFLICT;
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/* reads what names the comment at rowid and its status */
static __attribute__((noinline)) int ActivityDbReadCommentKey(dbutil::Connection *connection,
	const SceInt64 *rowid, ActivityDbCommentKey *key)
{
	paf::string sql("SELECT mainRowID, commentID, ownerID, ownerType FROM tbl_commentTable "
			"WHERE rowid=?");
	dbutil::RefPtr<dbutil::Statement> statement;
	SceInt64 mainRowid;
	bool found;
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, *rowid);
	if (result != 0)
		return result;
	found = false;
	while ((result = statement->Step(1)) == 0) {
		int count;
		int i;

		found = true;
		key->rowid = *rowid;
		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("mainRowID", value.name) == 0)
				mainRowid = ActivityDbValueInt64(&value);
			else if (sce_paf_strcmp("commentID", value.name) == 0)
				key->commentId = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("ownerID", value.name) == 0)
				key->ownerId = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("ownerType", value.name) == 0)
				key->ownerType = ActivityDbValueInt(&value);
		}
	}
	if (result != 1)
		return result;
	if (!found)
		return ACTIVITY_DB_ERROR_NOT_FOUND;

	ActivityDbStatusKey status;

	result = ActivityDbReadStatusKey(connection, &mainRowid, &status, NULL);
	if (result == 0)
		key->status = status;
	return result;
}

int ActivityDbQuery::GetCommentKey(const SceInt64 *rowid, ActivityDbCommentKey *key)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbReadCommentKey(connection, rowid, key);
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/* the time titleId last synced, 0 when it never synced */
int ActivityDbQuery::GetLastSyncedTime(const paf::string *titleId, SceInt64 *lastSyncedTime)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	*lastSyncedTime = 0;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	{
		paf::string sql("SELECT lastSyncedTime FROM tbl_lastSyncedTimeTable WHERE titleID=?");
		dbutil::RefPtr<dbutil::Statement> statement;
		bool found = false;

		result = connection->Prepare(sql, &statement);
		if (result == 0)
			result = ActivityDbBindString(&statement, 1, *titleId);
		if (result == 0) {
			while ((result = statement->Step(1)) == 0) {
				dbutil::Value value;

				found = true;
				result = statement->GetValue(0, &value);
				if (result != 0)
					break;
				*lastSyncedTime = ActivityDbValueTime(&value);
			}
			if (result == 1) {
				if (!found)
					*lastSyncedTime = 0;
				result = 0;
			}
		}
	}
	if (result == (int)DBUTIL_ERROR_NOT_FOUND)
		result = 0;
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/*
 * reads the latest statuses (of titleId's, or of every title) created after since, when it is
 * not 0, into statuses: one for each owner, until statuses holds max
 */
static int ActivityDbReadRecentStatuses(dbutil::Connection *connection,
	paf::vector<ActivityDbStatus> *statuses, const paf::string *titleId,
	const SceUInt64 *since, int max, int scope)
{
	dbutil::RefPtr<dbutil::Statement> statement;
	paf::string sql("SELECT * FROM tbl_statusMainTable ");
	int result;

	if (*since != 0)
		sql += "WHERE creationTime>?2 ";
	switch (scope) {
	case ACTIVITY_DB_RECENT_TITLE:
		if (*since != 0)
			sql += "AND titleID=?3 ORDER BY creationTime DESC";
		else
			sql += "WHERE titleID=?3 ORDER BY creationTime DESC";
		break;
	case ACTIVITY_DB_RECENT_ALL:
		sql += "ORDER BY creationTime DESC";
		break;
	default:
		return ACTIVITY_DB_ERROR_INVALID_TYPE;
	}
	sql += " LIMIT ?4";
	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, 1);
	if (result != 0)
		return result;
	if (*since != 0) {
		char text[30] = { 0 };

		SceActivityDb_9A8944FC(since, text, sizeof(text));
		if (text[0] != '\0') {
			result = ActivityDbBindText(&statement, 2, text, -1);
		} else {
			dbutil::Value value = { SQLITE_NULL };

			result = statement->Bind(2, &value, false);
		}
		if (result != 0)
			return result;
	}
	if (scope == ACTIVITY_DB_RECENT_TITLE) {
		result = ActivityDbBindString(&statement, 3, *titleId);
		if (result != 0)
			return result;
	}
	result = ActivityDbBindInt64(&statement, 4, ACTIVITY_DB_RECENT_LIMIT);
	if (result != 0)
		return result;
	while ((result = statement->Step(1)) == 0) {
		paf::vector<ActivityDbStatus>::iterator entry;
		ActivityDbStatus status;
		int count;
		int i;

		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("rowid", value.name) == 0)
				status.rowid = ActivityDbValueInt64(&value);
			else if (sce_paf_strcmp("ownerID", value.name) == 0)
				status.ownerId = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("ownerType", value.name) == 0)
				status.ownerType = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("titleID", value.name) == 0)
				status.titleId = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("titleName", value.name) == 0)
				status.titleName = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("type", value.name) == 0)
				status.type = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("creationTime", value.name) == 0)
				status.creationTime = ActivityDbValueTime(&value);
			else if (sce_paf_strcmp("updateTime", value.name) == 0)
				status.updateTime = ActivityDbValueTime(&value);
			else if (sce_paf_strcmp("isGetInfo", value.name) == 0)
				status.isGetInfo = ActivityDbValueInt(&value) != 0;
			else if (sce_paf_strcmp("isGetDetail", value.name) == 0)
				status.isGetDetail = ActivityDbValueInt(&value) != 0;
			else if (sce_paf_strcmp("message", value.name) == 0)
				status.message = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("numOfComments", value.name) == 0)
				status.numOfComments = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("numOfNewComments", value.name) == 0)
				status.numOfNewComments = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("numOfVotes", value.name) == 0)
				status.numOfVotes = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("actionURL", value.name) == 0)
				status.actionUrl = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("iconURL", value.name) == 0)
				status.iconUrl = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("contentID", value.name) == 0)
				status.contentId = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("announceAvatarURL", value.name) == 0)
				status.announceAvatarUrl = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("announcePanelBgColor", value.name) == 0)
				status.announcePanelBgColor = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("announcePanelURL", value.name) == 0)
				status.announcePanelUrl = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("statusID", value.name) == 0)
				status.statusId = ActivityDbValueInt(&value);
		}
		for (entry = statuses->begin(); entry != statuses->end(); ++entry) {
			if (status.ownerId == entry->ownerId)
				break;
		}
		if (entry == statuses->end()) {
			statuses->push_back(status);
			if (statuses->size() == (SceUInt32)max)
				return 0;
		}
	}
	if (result == 1)
		result = 0;
	return result;
}

/*
 * whether a status (of titleId's, or of every title's) updated after since, when it is not 0,
 * has new comments
 */
static int ActivityDbReadHasNewComments(dbutil::Connection *connection, bool *hasNew,
	const paf::string *titleId, const SceUInt64 *since, int scope)
{
	dbutil::RefPtr<dbutil::Statement> statement;
	paf::string sql;
	SceUInt64 latest = 0;
	int result;

	*hasNew = false;
	switch (scope) {
	case ACTIVITY_DB_RECENT_TITLE:
		sql = "SELECT MAX(updateTime) FROM tbl_statusMainTable WHERE titleID=?1 AND ";
		break;
	case ACTIVITY_DB_RECENT_ALL:
		sql = "SELECT MAX(updateTime) FROM tbl_statusMainTable WHERE ";
		break;
	default:
		return ACTIVITY_DB_ERROR_INVALID_TYPE;
	}
	if (*since != 0)
		sql += "updateTime>?2 AND ";
	sql += "numOfNewComments>0";
	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	if (scope == ACTIVITY_DB_RECENT_TITLE) {
		result = ActivityDbBindString(&statement, 1, *titleId);
		if (result != 0)
			return result;
	}
	if (*since != 0) {
		char text[30] = { 0 };

		SceActivityDb_9A8944FC(since, text, sizeof(text));
		if (text[0] != '\0') {
			result = ActivityDbBindText(&statement, 2, text, -1);
		} else {
			dbutil::Value value = { SQLITE_NULL };

			result = statement->Bind(2, &value, false);
		}
		if (result != 0)
			return result;
	}
	while ((result = statement->Step(1)) == 0) {
		int count;
		int i;

		latest = 0;
		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("MAX(updateTime)", value.name) == 0)
				latest = ActivityDbValueTime(&value);
		}
	}
	if (result != 1)
		return result;
	if (latest != 0 && (*since < latest || *since == 0))
		*hasNew = true;
	return 0;
}

/* reads how far the status at rowid was read; found tells whether it is there */
static __attribute__((noinline)) int ActivityDbReadStatusState(dbutil::Connection *connection,
	const SceInt64 *rowid, bool *found, ActivityDbStatusState *state)
{
	paf::string sql("SELECT titleID, updateTime, isGetInfo, isGetDetail FROM "
			"tbl_statusMainTable WHERE rowid=? ");
	dbutil::RefPtr<dbutil::Statement> statement;
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, *rowid);
	if (result != 0)
		return result;
	*found = false;
	while ((result = statement->Step(1)) == 0) {
		int count;
		int i;

		*found = true;
		state->rowid = *rowid;
		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("titleID", value.name) == 0)
				state->titleId = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("updateTime", value.name) == 0)
				state->updateTime = ActivityDbValueTime(&value);
			else if (sce_paf_strcmp("isGetInfo", value.name) == 0)
				state->isGetInfo = ActivityDbValueInt64(&value) != 0;
			else if (sce_paf_strcmp("isGetDetail", value.name) == 0)
				state->isGetDetail = ActivityDbValueInt64(&value) != 0;
		}
	}
	if (result != 1)
		return result;
	return 0;
}

int ActivityDbQuery::GetStatusState(const SceInt64 *rowid, bool *found,
	ActivityDbStatusState *state)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = ActivityDbReadStatusState(connection, rowid, found, state);
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/* the rows of tbl_statusMainTable */
static inline int ActivityDbCountStatuses(dbutil::Connection *connection, int *count)
{
	dbutil::RefPtr<dbutil::Statement> statement;
	paf::string sql("SELECT count(rowid) FROM tbl_statusMainTable");
	int result;

	*count = 0;
	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	while ((result = statement->Step(1)) == 0) {
		dbutil::Value value;

		result = statement->GetValue(0, &value);
		if (result != 0)
			return result;
		*count = ActivityDbValueInt(&value);
	}
	if (result != 1)
		return result;
	return 0;
}

/* the rows past the newest ACTIVITY_DB_SYNC_KEEP statuses */
static inline int ActivityDbReadOldRowids(dbutil::Connection *connection,
	paf::vector<SceInt64> *rowids)
{
	dbutil::RefPtr<dbutil::Statement> statement;
	paf::string sql("SELECT rowid FROM tbl_statusMainTable ORDER BY creationTime DESC "
			"LIMIT -1 OFFSET ?");
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, ACTIVITY_DB_SYNC_KEEP);
	if (result != 0)
		return result;
	while ((result = statement->Step(1)) == 0) {
		dbutil::Value value = {};

		statement->GetValue(0, &value);
		rowids->push_back(ActivityDbValueInt64(&value));
	}
	if (result != 1)
		return result;
	return 0;
}

/* the status's key on the network: its owner, "@" and its ID */
static inline paf::string ActivityDbStatusIdText(int statusId)
{
	char text[64];

	sce_paf_snprintf(text, sizeof(text), "%lld", (SceInt64)statusId);
	return paf::string(text);
}

/* reads the statuses a query of the type reaches and pairs each with the network's keys */
static inline int ActivityDbMatchStatuses(dbutil::Connection *connection,
	const ActivityDbQueryCondition *condition, const paf::string *titleId,
	const paf::string *ownerId, const SceUInt64 *since,
	paf::vector<paf::string> *keys, paf::vector<ActivityDbSyncStatus> *matched,
	paf::vector<SceInt64> *unmatched)
{
	paf::string sql("SELECT rowid, ownerID, ownerType, statusID, type, creationTime, "
			"updateTime, isGetInfo FROM tbl_statusMainTable WHERE ");
	int result;

	switch (condition->type) {
	case ACTIVITY_DB_QUERY_TITLE:
		sql += "titleID=?1 OR isGetInfo=0 ORDER BY creationTime DESC, rowid DESC";
		break;
	case ACTIVITY_DB_QUERY_ALL:
		sql += "ownerType!=?1 AND creationTime>=?2 ORDER BY creationTime DESC, rowid DESC";
		break;
	case ACTIVITY_DB_QUERY_NEWS:
		sql += "ownerID=?1 ORDER BY creationTime DESC, rowid DESC";
		break;
	default:
		return ACTIVITY_DB_ERROR_INVALID_TYPE;
	}

	dbutil::RefPtr<dbutil::Statement> statement;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	switch (condition->type) {
	case ACTIVITY_DB_QUERY_TITLE:
		result = ActivityDbBindString(&statement, 1, *titleId);
		break;
	case ACTIVITY_DB_QUERY_ALL: {
		char text[30] = { 0 };

		result = ActivityDbBindInt64(&statement, 1, ACTIVITY_DB_OWNER_TITLE);
		if (result != 0)
			return result;
		SceActivityDb_9A8944FC(since, text, sizeof(text));
		if (text[0] != '\0') {
			result = ActivityDbBindText(&statement, 2, text, -1);
		} else {
			dbutil::Value value = { SQLITE_NULL };

			result = statement->Bind(2, &value, false);
		}
		break;
	}
	case ACTIVITY_DB_QUERY_NEWS:
		result = ActivityDbBindString(&statement, 1, *ownerId);
		break;
	default:
		return ACTIVITY_DB_ERROR_INVALID_TYPE;
	}
	if (result != 0)
		return result;
	while ((result = statement->Step(1)) == 0) {
		paf::vector<paf::string>::iterator key;
		ActivityDbStatusHeader status;
		int count;
		int i;

		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("rowid", value.name) == 0)
				status.rowid = ActivityDbValueInt64(&value);
			else if (sce_paf_strcmp("ownerID", value.name) == 0)
				status.ownerId = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("ownerType", value.name) == 0)
				status.ownerType = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("statusID", value.name) == 0)
				status.statusId = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("type", value.name) == 0)
				status.type = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("creationTime", value.name) == 0)
				status.creationTime = ActivityDbValueTime(&value);
			else if (sce_paf_strcmp("updateTime", value.name) == 0)
				status.updateTime = ActivityDbValueTime(&value);
			else if (sce_paf_strcmp("isGetInfo", value.name) == 0)
				status.isGetInfo = ActivityDbValueInt64(&value) != 0;
		}

		paf::string owner(status.ownerType == ACTIVITY_DB_OWNER_TITLE ?
			*titleId : status.ownerId);
		paf::string syncKey = owner + "@" + ActivityDbStatusIdText(status.statusId);

		for (key = keys->begin(); key != keys->end(); ++key) {
			if (sce_paf_strcmp(key->c_str(), syncKey.c_str()) == 0)
				break;
		}
		if (key == keys->end()) {
			unmatched->push_back(status.rowid);
		} else {
			ActivityDbSyncStatus entry;

			entry.syncKey = *key;
			entry.status = status;
			matched->push_back(entry);
		}
	}
	if (result != 1)
		return result;
	return 0;
}

/*
 * pairs the query's statuses with the network's keys: those the network sent go to matched,
 * the others (and, for every title's, the oldest past the most kept) to unmatched
 */
int ActivityDbQuery::MatchStatuses(const paf::string *titleId, const paf::string *ownerId,
	const SceUInt64 *since, paf::vector<paf::string> *keys,
	paf::vector<ActivityDbSyncStatus> *matched, paf::vector<SceInt64> *unmatched)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (m_condition.type == ACTIVITY_DB_QUERY_ALL) {
		int count;

		result = ActivityDbCountStatuses(connection, &count);
		if (result != 0)
			goto end;
		if (count > ACTIVITY_DB_SYNC_MAX) {
			result = ActivityDbReadOldRowids(connection, unmatched);
			if (result != 0)
				goto end;
		}
	}
	result = ActivityDbMatchStatuses(connection, &m_condition, titleId, ownerId, since,
		keys, matched, unmatched);
end:
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/* reads what names each comment on the status: its key, and the comment's own */
static inline int ActivityDbReadCommentKeys(dbutil::Connection *connection,
	const SceInt64 *mainRowid, const ActivityDbStatusKey *status,
	paf::vector<ActivityDbCommentKey> *keys)
{
	paf::vector<ActivityDbCommentKey>::iterator key;
	int count = 0;
	int result;

	{
		paf::string sql("SELECT count(rowid) FROM tbl_commentTable WHERE mainRowID=?");
		dbutil::RefPtr<dbutil::Statement> statement;

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 1, *mainRowid);
		if (result != 0)
			return result;
		while ((result = statement->Step(1)) == 0) {
			dbutil::Value value;
			int error;

			error = statement->GetValue(0, &value);
			count = ActivityDbValueInt(&value);
			if (error != 0)
				break;
		}
		if (result != 0 && result != 1)
			return result;
	}
	if (count == 0)
		return 0;
	keys->clear();
	keys->resize(count);
	key = keys->begin();

	paf::string sql("SELECT rowid, commentID, ownerID, ownerType FROM tbl_commentTable "
			"WHERE mainRowID=? ");
	dbutil::RefPtr<dbutil::Statement> statement;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, *mainRowid);
	if (result != 0)
		return result;
	while ((result = statement->Step(1)) == 0) {
		int i;

		if (key == keys->end())
			return ACTIVITY_DB_ERROR_OUT_OF_RANGE;
		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("rowid", value.name) == 0)
				key->rowid = ActivityDbValueInt64(&value);
			else if (sce_paf_strcmp("commentID", value.name) == 0)
				key->commentId = ActivityDbValueInt(&value);
			else if (sce_paf_strcmp("ownerID", value.name) == 0)
				key->ownerId = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("ownerType", value.name) == 0)
				key->ownerType = ActivityDbValueInt(&value);
		}
		key->status = *status;
		++key;
	}
	if (result != 1)
		return result;
	return 0;
}

int ActivityDbQuery::GetCommentKeys(const SceInt64 *mainRowid,
	paf::vector<ActivityDbCommentKey> *keys)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;

	ActivityDbStatusKey status;

	result = ActivityDbReadStatusKey(connection, mainRowid, &status, NULL);
	if (result == 0)
		result = ActivityDbReadCommentKeys(connection, mainRowid, &status, keys);
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/* reads who gave the status at mainRowid good jobs; found tells whether anyone did */
static inline int ActivityDbReadGoodJobOwnerList(dbutil::Connection *connection,
	SceInt64 mainRowid, paf::vector<ActivityDbGoodJobOwner> *owners, bool *found)
{
	paf::string sql("SELECT ownerID, ownerType FROM tbl_goodJobTable WHERE mainRowID=?");
	dbutil::RefPtr<dbutil::Statement> statement;
	int result;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, mainRowid);
	if (result != 0)
		return result;
	while ((result = statement->Step(1)) == 0) {
		ActivityDbGoodJobOwner owner;
		int count;
		int i;

		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			result = statement->GetValue(i, &value);
			if (result != 0)
				return result;
			if (sce_paf_strcmp("ownerID", value.name) == 0)
				owner.ownerId = ActivityDbValueText(&value);
			else if (sce_paf_strcmp("ownerType", value.name) == 0)
				owner.ownerType = ActivityDbValueInt(&value);
		}
		owners->push_back(owner);
		*found = true;
	}
	if (result != 1)
		return result;
	return 0;
}

int ActivityDbQuery::GetGoodJobKey(const ActivityDbStatusKey *status, ActivityDbGoodJobKey *key)
{
	dbutil::Connection *connection;
	bool found = false;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	key->status = *status;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	key->owners.clear();
	result = ActivityDbReadGoodJobOwnerList(connection, status->rowid, &key->owners, &found);
	if (result == 0 && found) {
		bool isVoted;

		result = ActivityDbReadIsVoted(connection, status->rowid, &isVoted);
		if (result == 0)
			key->isVoted = isVoted;
	}
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/* the rows of the statuses waiting to be sent */
int ActivityDbQuery::GetWaitStatusRowids(paf::vector<SceInt64> *rowids)
{
	dbutil::Connection *connection;
	int count;
	int i;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	rowids->clear();

	paf::string sql("SELECT rowid FROM tbl_waitStatusTable");
	dbutil::RefPtr<dbutil::Statement> statement;

	if (connection->Prepare(sql, &statement) != 0)
		goto end;
	while (statement->Step(1) == 0) {
		count = statement->m_columnCount;
		for (i = 0; i < count; i++) {
			dbutil::Value value;

			if (statement->GetValue(i, &value) != 0)
				goto end;
			if (sce_paf_strcmp("rowid", value.name) == 0)
				rowids->push_back(ActivityDbValueInt64(&value));
		}
	}
end:
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return 0;
}

/* reads the waiting statuses of rowids into statuses, one for each */
static __attribute__((noinline)) int ActivityDbReadWaitStatuses(dbutil::Connection *connection,
	paf::vector<SceInt64> *rowids, paf::vector<ActivityDbStatus> *statuses)
{
	paf::vector<SceInt64>::iterator rowid = rowids->begin();
	paf::vector<ActivityDbStatus>::iterator status = statuses->begin();
	int result;

	for (; rowid != rowids->end(); ++rowid, ++status) {
		if (status == statuses->end())
			break;

		paf::string sql("SELECT rowid, titleID, titleName, type, message, creationTime, "
				"updateTime, actionURL, iconURL, contentID FROM tbl_waitStatusTable "
				"WHERE rowid=?");
		dbutil::RefPtr<dbutil::Statement> statement;
		bool found;

		result = connection->Prepare(sql, &statement);
		if (result != 0)
			return result;
		result = ActivityDbBindInt64(&statement, 1, *rowid);
		if (result != 0)
			return result;
		found = false;
		while ((result = statement->Step(1)) == 0) {
			int count;
			int i;

			found = true;
			status->rowid = *rowid;
			count = statement->m_columnCount;
			for (i = 0; i < count; i++) {
				dbutil::Value value;

				result = statement->GetValue(i, &value);
				if (result != 0)
					return result;
				if (sce_paf_strcmp("rowid", value.name) == 0)
					status->rowid = ActivityDbValueInt64(&value);
				else if (sce_paf_strcmp("titleID", value.name) == 0)
					status->titleId = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("titleName", value.name) == 0)
					status->titleName = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("type", value.name) == 0)
					status->type = ActivityDbValueInt(&value);
				else if (sce_paf_strcmp("message", value.name) == 0)
					status->message = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("creationTime", value.name) == 0)
					status->creationTime = ActivityDbValueTime(&value);
				else if (sce_paf_strcmp("updateTime", value.name) == 0)
					status->updateTime = ActivityDbValueTime(&value);
				else if (sce_paf_strcmp("actionURL", value.name) == 0)
					status->actionUrl = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("iconURL", value.name) == 0)
					status->iconUrl = ActivityDbValueText(&value);
				else if (sce_paf_strcmp("contentID", value.name) == 0)
					status->contentId = ActivityDbValueText(&value);
			}
		}
		if (result != 1)
			return result;
		if (!found)
			return ACTIVITY_DB_ERROR_NOT_FOUND;
	}
	return 0;
}

int ActivityDbQuery::GetWaitStatuses(paf::vector<SceInt64> *rowids,
	paf::vector<ActivityDbStatus> *statuses)
{
	dbutil::Connection *connection;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	statuses->clear();
	if (rowids->size() == 0)
		return 0;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	statuses->resize(rowids->size());
	result = ActivityDbReadWaitStatuses(connection, rowids, statuses);
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/* reads the keys of the statuses where reaches, up to limit, not read from the network yet */
static inline int ActivityDbReadUnreadStatusKeys(dbutil::Connection *connection,
	const paf::string *where, int limit, paf::vector<ActivityDbStatusKey> *keys)
{
	int result;

	keys->clear();

	paf::string sql = "SELECT rowid, ownerID, ownerType, statusID, titleID "
			  "FROM tbl_statusMainTable " + *where;

	sql += where->size() != 0 ? " AND " : " WHERE ";
	sql += "isGetInfo=? ORDER BY creationTime DESC, rowid DESC LIMIT ?";

	dbutil::RefPtr<dbutil::Statement> statement;

	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 1, 0);
	if (result != 0)
		return result;
	result = ActivityDbBindInt64(&statement, 2, limit);
	if (result != 0)
		return result;
	while ((result = statement->Step(1)) == 0) {
		dbutil::Value rowid;
		dbutil::Value ownerId;
		dbutil::Value ownerType;
		dbutil::Value statusId;
		dbutil::Value titleId;

		result = statement->GetValue(0, &rowid);
		if (result != 0)
			return result;
		result = statement->GetValue(1, &ownerId);
		if (result != 0)
			return result;
		result = statement->GetValue(2, &ownerType);
		if (result != 0)
			return result;
		result = statement->GetValue(3, &statusId);
		if (result != 0)
			return result;
		result = statement->GetValue(4, &titleId);
		if (result != 0)
			return result;

		ActivityDbStatusKey key;

		key.rowid = ActivityDbValueInt64(&rowid);
		key.ownerId = ActivityDbValueText(&ownerId);
		key.ownerType = ActivityDbValueInt(&ownerType);
		key.statusId = ActivityDbValueInt(&statusId);
		key.titleId = ActivityDbValueText(&titleId);
		keys->push_back(key);
	}
	if (result != 1)
		return result;
	return 0;
}

/* the keys of the statuses the condition reaches that were not read from the network yet */
int ActivityDbQuery::GetUnreadStatusKeys(const ActivityDbQueryCondition *condition,
	paf::vector<ActivityDbStatusKey> *keys)
{
	dbutil::Connection *connection;
	int limit;
	int result;

	if (!m_registered)
		return ACTIVITY_DB_ERROR_NOT_READY;
	connection = SceActivityDb_FBB82B37()->BeginTransaction(m_database);
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	limit = -1;
	if (condition->limit > 0)
		limit = condition->limit;

	paf::string where;

	result = ActivityDbQueryBuildWhere(&where, condition);
	if (result == 0)
		result = ActivityDbReadUnreadStatusKeys(connection, &where, limit, keys);
	SceActivityDb_FBB82B37()->EndTransaction(m_database, -1);
	return result;
}

/*
 * the recent statuses of titleId (or of every title) since since, one per owner, up to max,
 * and whether one of them has new comments
 */
static __attribute__((noinline)) int ActivityDbGetRecentStatuses(int database,
	paf::vector<ActivityDbStatus> *statuses, bool *hasNew, const paf::string *titleId,
	const SceUInt64 *since, int max, int scope)
{
	ActivityDbQuery query;
	ActivityDbQueryCondition condition;

	condition.titleId = *titleId;
	if (query.Setup(&condition, &database) == 0 && query.m_registered &&
	    titleId->size() != 0) {
		dbutil::Connection *connection;

		statuses->clear();
		connection = SceActivityDb_FBB82B37()->BeginTransaction(query.m_database);
		if (connection != NULL) {
			if (ActivityDbReadRecentStatuses(connection, statuses, titleId, since, max,
							 scope) == 0)
				ActivityDbReadHasNewComments(connection, hasNew, titleId, since, scope);
			SceActivityDb_FBB82B37()->EndTransaction(query.m_database, -1);
		}
	}
	return 0;
}

/* sets the time titleId's statuses were last read from the network */
__attribute__((noinline)) int ActivityDbSetBaseTime(int database, const paf::string *titleId,
	const SceInt64 *baseTime, int scope)
{
	return SceActivityDb_FBB82B37()->SetBaseTime(database, titleId, baseTime, scope);
}

/* updates the database's comment and good job counts of the network's statuses */
int ActivityDbUpdateCounts(int database, int keepNewComments,
	paf::vector<ActivityDbNetStatus> *statuses)
{
	ActivityDbQuery query;
	ActivityDbQueryCondition condition;
	int result;

	result = query.Setup(&condition, &database);
	if (result != 0)
		return result;
	return query.UpdateCounts(keepNewComments, statuses);
}

/* the daemon's listener: the query's database changed */
static void ActivityDbQueryOnChange(void *arg)
{
	if (arg != NULL)
		sceAtomicExchange32AcqRel((volatile int32_t *)&((ActivityDbQuery *)arg)->m_changed, 1);
}
