/*
 * SceActivityDb (vs0:sys/external/activity_db.suprx): the shell's activity database (the
 * statuses, comments and "good jobs" of the NP activity feed, ur0:shell/db/app.db's
 * tbl_statusMainTable and its tables) and the daemons that sync it with the network.
 *
 * Built by Sony with SNC 3.57 in C++ on paf and dbutil. No debug record or string names its
 * files or classes; Sony's object names name its parts: SceLsdbDbAcc (the database access
 * object), SceLsdbActDaemon, SceLsdbActNetDaemon, SceLsdbActNpAcc, the SceActMngObj* and
 * SceActDbSyncObj* jobs. Class, member and file names are ours, after those. Its exports are
 * C++ members whose NIDs reproduce with no known name: CMakeLists.txt links each to its
 * SceActivityDb_<NID> name.
 */
#ifndef ACTIVITY_DB_INTERNAL_H
#define ACTIVITY_DB_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/stdio.h>
#include <std/string>
#include <std/vector>
#include <std/list>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <rtc/rtc.h>
#include <system/system.h>
#include <thread/thread.h>
#include <dbutil/connection.h>
#include <dbutil/statement.h>
#include <dbutil/function.h>
#include <np/np_manager/np_manager.h>
#include "activity_db.h"

/* SceActivityDb's errors (no SDK header names them) */
#define ACTIVITY_DB_ERROR_DATABASE		((int)0x80101000)
#define ACTIVITY_DB_ERROR_INVALID_ARGUMENT	((int)0x80101001)
#define ACTIVITY_DB_ERROR_NO_MEMORY		((int)0x80101002)
#define ACTIVITY_DB_ERROR_NOT_READY		((int)0x80101003)
#define ACTIVITY_DB_ERROR_CONFLICT		((int)0x80101004)	/* SQLite's constraint */
#define ACTIVITY_DB_ERROR_NOT_FOUND		((int)0x80101005)
#define ACTIVITY_DB_ERROR_NO_TITLE_ID		((int)0x80101009)	/* a status's info without its title */
#define ACTIVITY_DB_ERROR_INVALID_TYPE		((int)0x8010100A)
#define ACTIVITY_DB_ERROR_NO_ROWID		((int)0x8010100D)
#define ACTIVITY_DB_ERROR_NO_TIME		((int)0x80101010)	/* a status without its times */
#define ACTIVITY_DB_ERROR_OUT_OF_RANGE		((int)0x80101040)	/* more rows than counted */
#define ACTIVITY_DB_ERROR_WAIT			((int)0x80101045)	/* a job's result never came */

/* SQLite's constraint error, as dbutil reports it: a row the database already has */
#define ACTIVITY_DB_SQLITE_CONSTRAINT	((int)(DBUTIL_ERROR_SQLITE | SQLITE_CONSTRAINT))

/* the database's user_version: what it was built for */
#define ACTIVITY_DB_VERSION_FORMAT	1	/* the layout of user_version itself */

/* Sony inlines the end callbacks' vector destructor */
PAF_VECTOR_INLINE_DTOR(ActivityDbEndCallback)

/* the database's access object (Sony's object name SceLsdbDbAcc) */
struct ActivityDbAccess {
	ActivityDbAccess();
	~ActivityDbAccess();

	void Close();
	dbutil::Connection *BeginTransaction();
	int EndTransaction(int result);
	int CheckParam(dbutil::Connection *connection, const ActivityDbRebuildParam *param,
		       ActivityDbRebuildParam *current, int *rebuilt);
	int Upgrade(const ActivityDbRebuildParam *param, ActivityDbUpgradeCallback upgrade);
	int OpenConnection(const char *path, int flags);
	int Open(const char *path, int flags, const ActivityDbRebuildParam *param,
		 bool keepOld, ActivityDbUpgradeCallback upgrade);
	void AddEndCallback(ActivityDbEndCallback callback);
	void RegisterFunction(dbutil::Function *function);
	void UnregisterFunction(dbutil::Function *function);
	int GetParam(ActivityDbRebuildParam *param);
	int SetParam(const ActivityDbRebuildParam *param);
	int UpdateParam(ActivityDbRebuildParam *param);

	dbutil::Connection *m_connection;
	paf::thread::RMutex m_lock;
	int m_depth;				/* the transactions taken and not yet ended */
	paf::vector<ActivityDbEndCallback> m_endCallbacks;
};

/* steps statement to its last row: 0, or the error */
int ActivityDbStepAll(dbutil::RefPtr<dbutil::Statement> *statement);

/* a value's integer, 0 for one that is not */
static inline SceInt64 ActivityDbValueInt64(const dbutil::Value *value)
{
	return value->type == SQLITE_INTEGER ? value->data.integer : 0;
}

static inline int ActivityDbValueInt(const dbutil::Value *value)
{
	return value->type == SQLITE_INTEGER ? (int)value->data.integer : 0;
}

/* a value's text, empty for one that is not */
static inline paf::string ActivityDbValueText(const dbutil::Value *value)
{
	paf::string text;

	if (value->type == SQLITE_TEXT)
		text.assign(value->data.text, value->size);
	return text;
}

/* a value's time (a text the database keeps), 0 for one that is not */
static inline SceInt64 ActivityDbValueTime(const dbutil::Value *value)
{
	return value->type == SQLITE_TEXT ? SceActivityDb_ADFC2C4C(value->data.text) : 0;
}

/* binds an integer to the statement's parameter index */
static inline int ActivityDbBindInt64(dbutil::RefPtr<dbutil::Statement> *statement, int index,
	SceInt64 integer)
{
	dbutil::Value value = { SQLITE_INTEGER };

	value.data.integer = integer;
	return (*statement)->Bind(index, &value, false);
}

/* binds a text of size bytes (a negative size: up to its NUL) */
static inline int ActivityDbBindText(dbutil::RefPtr<dbutil::Statement> *statement, int index,
	const char *text, int size)
{
	dbutil::Value value = { SQLITE_TEXT };

	if (size < 0)
		size = sce_paf_strlen(text);
	value.size = size;
	value.data.text = text;
	return (*statement)->Bind(index, &value, false);
}

/* binds a string's text, or NULL for an empty one */
static inline int ActivityDbBindString(dbutil::RefPtr<dbutil::Statement> *statement, int index,
	const paf::string &string)
{
	if (string.c_str() != NULL && *string.c_str() != '\0' && string.size() != 0)
		return ActivityDbBindText(statement, index, string.c_str(), string.size());

	dbutil::Value value = { SQLITE_NULL };

	return (*statement)->Bind(index, &value, false);
}

/* told that a database changed */
typedef void (*ActivityDbListener)(void *arg);

/* what a query selects (ActivityDbQueryCondition::type) */
#define ACTIVITY_DB_QUERY_TITLE		0	/* a title's statuses, with the Friends application's */
#define ACTIVITY_DB_QUERY_ALL		1	/* every title's statuses */
#define ACTIVITY_DB_QUERY_NEWS		2	/* the second database's */
#define ACTIVITY_DB_QUERY_TITLE_ONLY	3	/* a title's statuses alone */

/* a status's owner (ActivityDbStatus::ownerType) */
#define ACTIVITY_DB_OWNER_TITLE		1	/* a title, named by titleID */
#define ACTIVITY_DB_OWNER_SELF		2	/* the signed-in user */
#define ACTIVITY_DB_OWNER_UNKNOWN	3

/* the announce panel's colour without one */
#define ACTIVITY_DB_PANEL_BG_COLOR_DEFAULT	0x989898ffU

/* what a query selects */
struct ActivityDbQueryCondition {
	ActivityDbQueryCondition() : type(ACTIVITY_DB_QUERY_TITLE_ONLY), limit(0)
	{
	}

	int type;			/* ACTIVITY_DB_QUERY_* */
	paf::string ownerId;
	paf::string titleId;
	int limit;			/* the rows it reaches; 0: the type's default */
};

/* a status (a row of tbl_statusMainTable) */
struct ActivityDbStatus {
	ActivityDbStatus()
		: ownerType(ACTIVITY_DB_OWNER_UNKNOWN), rowid(0), statusId(-1), type(100),
		  creationTime(0), updateTime(0), isGetInfo(false), isGetDetail(false),
		  numOfComments(0), numOfNewComments(0), numOfVotes(0),
		  announcePanelBgColor(ACTIVITY_DB_PANEL_BG_COLOR_DEFAULT)
	{
	}

	paf::string ownerId;
	int ownerType;			/* ACTIVITY_DB_OWNER_* */
	SceInt64 rowid;
	int statusId;
	int type;
	SceInt64 creationTime;
	SceInt64 updateTime;
	bool isGetInfo;
	bool isGetDetail;
	paf::string titleId;
	paf::string titleName;
	paf::string contentId;
	paf::string message;
	int numOfComments;
	int numOfNewComments;
	int numOfVotes;
	paf::string actionUrl;
	paf::string iconUrl;
	paf::string announceAvatarUrl;
	SceUInt32 announcePanelBgColor;
	paf::string announcePanelUrl;
};

/* Sony calls these vectors' clear and push_back, and inlines the rows' vector's destructor */
PAF_VECTOR_NOINLINE_CLEAR(ActivityDbStatus, paf::allocator<ActivityDbStatus>)
PAF_VECTOR_NOINLINE_PUSH_BACK(ActivityDbStatus, paf::allocator<ActivityDbStatus>)
PAF_VECTOR_INLINE_DTOR(SceInt64)

/* vector<SceInt64>'s copy constructor out of line: Sony calls it where it inlines the rest */
namespace paf {
template<> inline __attribute__((noinline))
vector<SceInt64, allocator<SceInt64> >::vector(const vector<SceInt64, allocator<SceInt64> > &src)
	: alloc(src.alloc)
{
	count = 0;
	limit = 0;

	SceInt64 *data = alloc.allocate(src.limit);

	if (data == NULL)
		alloc.report_failure();
	ptr = data;
	if (data != NULL) {
		count = src.count;
		limit = src.limit;
		for (size_type i = 0; i < count; ++i)
			new (&ptr[i]) SceInt64(src.ptr[i]);
	}
}
}

/* a status as the network gives it, with what it added since the database's copy */
struct ActivityDbNetStatus {
	ActivityDbNetStatus() : syncKey(), isSynced(false), addedComments(0), addedVotes(0)
	{
	}

	ActivityDbStatus status;
	paf::string syncKey;
	bool isSynced;
	int addedComments;		/* the comments the database does not have yet */
	int addedVotes;			/* the good jobs the database does not have yet */
};
PAF_VECTOR_NOINLINE_PUSH_BACK(ActivityDbNetStatus, paf::allocator<ActivityDbNetStatus>)

/* Sony calls vector<ActivityDbNetStatus>'s destructor out of line */
namespace paf {
template<> inline __attribute__((noinline))
vector<ActivityDbNetStatus, allocator<ActivityDbNetStatus> >::~vector()
{
	typedef ActivityDbNetStatus _Elem;
	PAF_VECTOR_DESTROY_ALL
}
}

/* a comment on a status (a row of tbl_commentTable) */
struct ActivityDbComment {
	ActivityDbComment()
		: rowid(0), commentId(-1), ownerType(ACTIVITY_DB_OWNER_UNKNOWN), creationTime(0),
		  reserved(false), announcePanelBgColor(0)
	{
	}

	SceInt64 rowid;
	int commentId;
	paf::string ownerId;
	int ownerType;			/* ACTIVITY_DB_OWNER_* */
	SceInt64 creationTime;
	paf::string message;
	bool reserved;
	paf::string announceAvatarUrl;
	SceUInt32 announcePanelBgColor;
	paf::string announcePanelUrl;
};

PAF_VECTOR_NOINLINE_CLEAR(ActivityDbComment, paf::allocator<ActivityDbComment>)
PAF_VECTOR_NOINLINE_PUSH_BACK(ActivityDbComment, paf::allocator<ActivityDbComment>)

/* what names a status on the network: its owner, its title and its ID */
struct ActivityDbStatusKey {
	ActivityDbStatusKey() : rowid(0), ownerType(ACTIVITY_DB_OWNER_UNKNOWN), statusId(-1)
	{
	}

	SceInt64 rowid;
	paf::string ownerId;
	int ownerType;			/* ACTIVITY_DB_OWNER_* */
	paf::string titleId;
	int statusId;
};

/* Sony calls this vector's clear and push_back */
PAF_VECTOR_NOINLINE_CLEAR(ActivityDbStatusKey, paf::allocator<ActivityDbStatusKey>)
PAF_VECTOR_NOINLINE_PUSH_BACK(ActivityDbStatusKey, paf::allocator<ActivityDbStatusKey>)

/* what names a comment on the network: its status's key, and its own */
struct ActivityDbCommentKey {
	ActivityDbCommentKey()
		: rowid(0), commentId(-1), ownerType(ACTIVITY_DB_OWNER_UNKNOWN)
	{
	}

	ActivityDbStatusKey status;
	SceInt64 rowid;
	int commentId;
	paf::string ownerId;
	int ownerType;			/* ACTIVITY_DB_OWNER_* */
	paf::string reserved;		/* constructed and destroyed, never read or written */
};

/* Sony calls this vector's clear */
PAF_VECTOR_NOINLINE_CLEAR(ActivityDbCommentKey, paf::allocator<ActivityDbCommentKey>)

/* how far a status was read from the network */
struct ActivityDbStatusState {
	SceInt64 rowid;
	paf::string titleId;
	SceInt64 updateTime;
	bool isGetInfo;
	bool isGetDetail;
};

/* the columns of a status that tell whether the network's copy is newer */
struct ActivityDbStatusHeader {
	ActivityDbStatusHeader()
		: rowid(0), ownerType(ACTIVITY_DB_OWNER_UNKNOWN), statusId(-1), type(100),
		  creationTime(0), updateTime(0), isGetInfo(false)
	{
	}

	SceInt64 rowid;
	paf::string ownerId;
	int ownerType;			/* ACTIVITY_DB_OWNER_* */
	paf::string titleId;
	int statusId;
	int type;
	SceInt64 creationTime;
	SceInt64 updateTime;
	bool isGetInfo;
};

/* a status the database has and the network sent, under the network's key for it */
struct ActivityDbSyncStatus {
	ActivityDbStatusHeader status;
	paf::string syncKey;		/* "<owner>@<statusID>" */
};

PAF_VECTOR_NOINLINE_PUSH_BACK(ActivityDbSyncStatus, paf::allocator<ActivityDbSyncStatus>)

/* a status's good jobs (tbl_goodJobTable and tbl_goodJobVotedTable) */
struct ActivityDbGoodJob {
	SceInt64 mainRowid;		/* the status's */
	bool isVoted;			/* the user gave one */
	paf::vector<paf::string> ownerIds;	/* who gave them, the latest first */
	int reserved;
	int numOfVotes;
};

/* a good job's giver */
struct ActivityDbGoodJobOwner {
	ActivityDbGoodJobOwner() : ownerType(ACTIVITY_DB_OWNER_UNKNOWN)
	{
	}

	paf::string ownerId;
	int ownerType;
	paf::string reserved;
};

PAF_VECTOR_NOINLINE_CLEAR(ActivityDbGoodJobOwner, paf::allocator<ActivityDbGoodJobOwner>)
PAF_VECTOR_NOINLINE_PUSH_BACK(ActivityDbGoodJobOwner, paf::allocator<ActivityDbGoodJobOwner>)

/* what names a status's good jobs on the network: its key, who gave them, and the user's */
struct ActivityDbGoodJobKey {
	ActivityDbStatusKey status;
	paf::vector<ActivityDbGoodJobOwner> owners;
	bool isVoted;			/* the user gave one */
};

/* the daemon's databases: the activity feed's and the news's (ActivityDbQueryDatabase) */
#define ACTIVITY_DB_DATABASE_ACTIVITY	0
#define ACTIVITY_DB_DATABASE_NEWS	1
#define ACTIVITY_DB_DATABASE_COUNT	2

/* the first rowid of the activity feed's database; the news's are below it */
#define ACTIVITY_DB_ACTIVITY_ROWID_BASE	0x4000000000000000LL

/* the databases the daemon opened */
struct ActivityDbDatabases {
	ActivityDbDatabases();
	~ActivityDbDatabases()
	{
	}

	int Open(int database, const char *path, int flags, const ActivityDbRebuildParam *param,
		 bool keepOld, ActivityDbUpgradeCallback upgrade);
	int Close(int database);
	bool IsEmpty();
	ActivityDbAccess *Get(int database);
	ActivityDbAccess *GetByRowid(SceInt64 rowid);
	ActivityDbAccess *GetByType(int type);

	ActivityDbAccess *m_access[ACTIVITY_DB_DATABASE_COUNT];
	int m_count;
};

/* a job of the daemon's queue, run on a database's access object */
class ActivityDbJob {
public:
	virtual ~ActivityDbJob()
	{
	}

	virtual int Run(ActivityDbAccess *access) = 0;

	/* told the result of Run, or of the queue's refusal */
	virtual void Done(int result)
	{
	}
};

/* a job's caller, waiting for its result (Sony's mutexes "SceActMngObj<job>") */
struct ActivityDbJobWait {
	ActivityDbJobWait(const char *name, SceInt64 *syncedTime)
		: mutex(name, 0), cond(mutex, name, 0), result(0), syncedTime(syncedTime)
	{
		mutex.Lock();
	}

	~ActivityDbJobWait()
	{
		mutex.Unlock();
	}

	int Wait()
	{
		int result = ACTIVITY_DB_ERROR_WAIT;

		if (cond.Wait(0) == 0)
			result = this->result;
		return result;
	}

	void Done(int result)
	{
		mutex.Lock();
		cond.Notify();
		this->result = result;
		mutex.Unlock();
	}

	paf::thread::Mutex mutex;
	paf::thread::Cond cond;
	int result;
	SceInt64 *syncedTime;		/* where a sync's job hands back its time */
};

/* the jobs' queue and the thread that runs them (Sony's object name SceLsdbActQueue) */
class ActivityDbQueue : public paf::thread::Thread {
public:
	struct Entry {
		ActivityDbAccess *access;
		ActivityDbJob *job;
		bool notify;		/* the daemon's listeners are told when it succeeds */
	};

	ActivityDbQueue(int priority);
	virtual ~ActivityDbQueue();
	virtual void EntryFunction();

	int Push(ActivityDbAccess *access, ActivityDbJob *job, bool notify);
	void Stop();

	paf::list<Entry *> m_entries;
	paf::thread::Mutex m_lock;
	paf::thread::Cond m_emptyCond;		/* "SceLsdbActQueueEmpty": the thread waits on it */
	paf::thread::Cond m_notEmptyCond;	/* "SceLsdbActQueueNotEmpty" */
};

/* a listener of the daemon */
struct ActivityDbListenerEntry {
	ActivityDbListener listener;
	void *arg;
};

/* the activity daemon (Sony's object name SceLsdbActDaemon) */
struct ActivityDbDaemon {
	ActivityDbDaemon()
		: m_queue(NULL), m_databases(NULL), m_lock("SceLsdbActDaemon", 0),
		  m_listenerLock("SceLsdbActDaemonUpdate", 0), m_started(false)
	{
	}

	int Start(ActivityDbDatabases *databases, int priority);
	int OpenDatabase(const char *path, int database);
	int CloseDatabase(int database);
	void Stop();
	dbutil::Connection *BeginTransaction(int database);
	int EndTransaction(int database, int result);
	int AddListener(ActivityDbListener listener, void *arg);
	int RemoveListener(void *arg);
	int StoreStatuses(paf::vector<ActivityDbNetStatus> *statuses);
	int DeleteStatus(const SceInt64 *rowid);
	int UpdateStatuses(paf::vector<ActivityDbStatusKey> *keys,
		paf::vector<ActivityDbNetStatus> *statuses);
	int PostComment(const SceInt64 *mainRowid, const ActivityDbComment *comment);
	int DeleteComments(paf::vector<ActivityDbCommentKey> *keys);
	int VoteGoodJob(const ActivityDbGoodJobKey *key);
	int CancelGoodJob(const ActivityDbGoodJobKey *key);
	int AddWaitStatus(const ActivityDbStatus *status);
	int ClearWaitStatus();
	int ClearWaitStatuses(paf::vector<SceInt64> *rowids);
	int SyncActivity(int type, const paf::string *titleId, const SceInt64 *syncedTime,
		paf::vector<ActivityDbNetStatus> *added, paf::vector<ActivityDbNetStatus> *updated,
		paf::vector<SceInt64> *deleted, SceInt64 *waitTime);
	int SyncStatusDetail(const ActivityDbNetStatus *status,
		const paf::vector<ActivityDbComment> *comments,
		const paf::vector<ActivityDbCommentKey> *deletedComments,
		const ActivityDbGoodJobKey *goodJobs, const ActivityDbGoodJobKey *cancelledGoodJobs,
		bool isVoted);
	int NotifyListeners();
	int Notify();
	int DeleteTitles(int database, paf::vector<paf::string> *titleIds,
		ActivityDbJobWait *wait);
	int TrimStatuses(int database, const char *titleId, int keep);
	int UpdateBaseTime(ActivityDbAccess *access, const paf::string *titleId,
		const SceInt64 *baseTime, int scope);
	int SetBaseTime(int database, const paf::string *titleId, const SceInt64 *baseTime,
		int scope);

	/* the jobs' work, on the queue's thread */
	int RunStoreStatuses(ActivityDbAccess *access, paf::vector<ActivityDbNetStatus> *statuses);
	int RunDeleteStatuses(ActivityDbAccess *access, paf::vector<SceInt64> *rowids);
	int RunUpdateStatuses(ActivityDbAccess *access, paf::vector<ActivityDbNetStatus> *statuses);
	int RunPostComments(ActivityDbAccess *access, const SceInt64 *mainRowid,
		paf::vector<ActivityDbComment> *comments);
	int RunDeleteComments(ActivityDbAccess *access, paf::vector<ActivityDbCommentKey> *keys);
	int RunVoteGoodJob(ActivityDbAccess *access, ActivityDbGoodJobKey *key);
	int RunCancelGoodJob(ActivityDbAccess *access, ActivityDbGoodJobKey *key);
	int RunAddWaitStatus(ActivityDbAccess *access, const ActivityDbStatus *status);
	int RunClearWaitStatus(ActivityDbAccess *access);
	int RunClearWaitStatuses(ActivityDbAccess *access, paf::vector<SceInt64> *rowids);
	int RunSyncActivity(ActivityDbAccess *access, paf::vector<SceInt64> *deleted,
		paf::vector<ActivityDbNetStatus> *added, paf::vector<ActivityDbNetStatus> *updated,
		const paf::string *titleId, const SceInt64 *syncedTime);
	int RunSyncStatusDetail(ActivityDbAccess *access, ActivityDbNetStatus *status,
		paf::vector<ActivityDbComment> *comments,
		paf::vector<ActivityDbCommentKey> *deletedComments, ActivityDbGoodJobKey *goodJobs,
		ActivityDbGoodJobKey *cancelledGoodJobs, bool isVoted);
	int RunDeleteTitles(ActivityDbAccess *access, paf::vector<paf::string> *titleIds);

	ActivityDbQueue *m_queue;
	ActivityDbDatabases *m_databases;
	paf::thread::Mutex m_lock;		/* "SceLsdbActDaemon" */
	paf::thread::Mutex m_listenerLock;	/* "SceLsdbActDaemonUpdate" */
	bool m_started;
	paf::list<ActivityDbListenerEntry> m_listeners;
};

/* the jobs the daemon queues (Sony's mutexes name them "SceActMngObj<job>") */
class ActivityDbSyncActivityJob : public ActivityDbJob {
public:
	ActivityDbSyncActivityJob(const paf::string *titleId, const SceInt64 *syncedTime,
		paf::vector<SceInt64> *deleted, paf::vector<ActivityDbNetStatus> *added,
		paf::vector<ActivityDbNetStatus> *updated, ActivityDbJobWait *wait)
		: m_titleId(*titleId), m_syncedTime(*syncedTime), m_deleted(*deleted),
		  m_added(added), m_updated(*updated), m_wait(wait)
	{
	}

	virtual int Run(ActivityDbAccess *access);
	virtual void Done(int result);

	paf::string m_titleId;
	char reserved1[16];
	SceInt64 m_syncedTime;		/* the time the network's statuses were read */
	int reserved2;
	paf::vector<SceInt64> m_deleted;	/* the statuses the network dropped */
	paf::vector<ActivityDbNetStatus> *m_added;	/* the caller's: the ones it added */
	paf::vector<ActivityDbNetStatus> m_updated;	/* the ones it changed */
	ActivityDbJobWait *m_wait;
};

class ActivityDbSyncStatusDetailJob : public ActivityDbJob {
public:
	ActivityDbSyncStatusDetailJob(const ActivityDbNetStatus *status,
		const paf::vector<ActivityDbComment> *comments,
		const paf::vector<ActivityDbCommentKey> *deletedComments,
		const ActivityDbGoodJobKey *goodJobs, const ActivityDbGoodJobKey *cancelledGoodJobs,
		bool isVoted, ActivityDbJobWait *wait)
		: m_status(*status), m_comments(*comments), m_deletedComments(*deletedComments),
		  m_goodJobs(*goodJobs), m_cancelledGoodJobs(*cancelledGoodJobs), m_isVoted(isVoted),
		  m_wait(wait)
	{
	}

	virtual int Run(ActivityDbAccess *access);
	virtual void Done(int result);

	ActivityDbNetStatus m_status;
	paf::vector<ActivityDbComment> m_comments;
	paf::vector<ActivityDbCommentKey> m_deletedComments;
	ActivityDbGoodJobKey m_goodJobs;
	ActivityDbGoodJobKey m_cancelledGoodJobs;
	bool m_isVoted;
	ActivityDbJobWait *m_wait;
};

class ActivityDbStoreStatusesJob : public ActivityDbJob {
public:
	ActivityDbStoreStatusesJob(paf::vector<ActivityDbNetStatus> *statuses)
		: m_statuses(*statuses)
	{
	}

	virtual int Run(ActivityDbAccess *access);

	paf::vector<ActivityDbNetStatus> m_statuses;
};

class ActivityDbDeleteStatusJob : public ActivityDbJob {
public:
	ActivityDbDeleteStatusJob(const SceInt64 *rowid) : m_rowid(*rowid)
	{
	}

	virtual int Run(ActivityDbAccess *access);

	SceInt64 m_rowid;
};

class ActivityDbUpdateStatusJob : public ActivityDbJob {
public:
	ActivityDbUpdateStatusJob(paf::vector<ActivityDbNetStatus> *statuses,
		ActivityDbJobWait *wait)
		: m_statuses(*statuses), m_wait(wait)
	{
	}

	virtual int Run(ActivityDbAccess *access);
	virtual void Done(int result);

	paf::vector<ActivityDbNetStatus> m_statuses;
	ActivityDbJobWait *m_wait;
};

class ActivityDbPostCommentJob : public ActivityDbJob {
public:
	ActivityDbPostCommentJob(const SceInt64 *mainRowid,
		const paf::vector<ActivityDbComment> &comments, ActivityDbJobWait *wait)
		: m_mainRowid(*mainRowid), m_comments(comments), m_wait(wait)
	{
	}

	virtual int Run(ActivityDbAccess *access);
	virtual void Done(int result);

	SceInt64 m_mainRowid;
	paf::vector<ActivityDbComment> m_comments;
	ActivityDbJobWait *m_wait;
};

class ActivityDbDeleteCommentJob : public ActivityDbJob {
public:
	ActivityDbDeleteCommentJob(paf::vector<ActivityDbCommentKey> *keys, ActivityDbJobWait *wait)
		: m_keys(*keys), m_wait(wait)
	{
	}

	virtual int Run(ActivityDbAccess *access);
	virtual void Done(int result);

	paf::vector<ActivityDbCommentKey> m_keys;
	ActivityDbJobWait *m_wait;
};

class ActivityDbVoteGoodJobJob : public ActivityDbJob {
public:
	ActivityDbVoteGoodJobJob(const ActivityDbGoodJobKey *key, ActivityDbJobWait *wait)
		: m_key(*key), m_wait(wait)
	{
	}

	virtual int Run(ActivityDbAccess *access);
	virtual void Done(int result);

	ActivityDbGoodJobKey m_key;
	ActivityDbJobWait *m_wait;
};

class ActivityDbCancelGoodJobJob : public ActivityDbJob {
public:
	ActivityDbCancelGoodJobJob(const ActivityDbGoodJobKey *key, ActivityDbJobWait *wait)
		: m_key(*key), m_wait(wait)
	{
	}

	virtual int Run(ActivityDbAccess *access);
	virtual void Done(int result);

	ActivityDbGoodJobKey m_key;
	ActivityDbJobWait *m_wait;
};

class ActivityDbAddWaitStatusJob : public ActivityDbJob {
public:
	ActivityDbAddWaitStatusJob(const ActivityDbStatus *status) : m_status(*status)
	{
	}

	virtual int Run(ActivityDbAccess *access);

	ActivityDbStatus m_status;
};

class ActivityDbClearWaitStatusJob : public ActivityDbJob {
public:
	ActivityDbClearWaitStatusJob(ActivityDbJobWait *wait) : m_wait(wait)
	{
	}

	virtual int Run(ActivityDbAccess *access);
	virtual void Done(int result);

	ActivityDbJobWait *m_wait;
};

class ActivityDbClearWaitStatusesJob : public ActivityDbJob {
public:
	ActivityDbClearWaitStatusesJob(paf::vector<SceInt64> *rowids, ActivityDbJobWait *wait)
		: m_rowids(*rowids), m_wait(wait)
	{
	}

	virtual int Run(ActivityDbAccess *access);
	virtual void Done(int result);

	paf::vector<SceInt64> m_rowids;
	ActivityDbJobWait *m_wait;
};

class ActivityDbDeleteTitlesJob : public ActivityDbJob {
public:
	ActivityDbDeleteTitlesJob(paf::vector<paf::string> *titleIds, ActivityDbJobWait *wait)
		: m_titleIds(*titleIds), m_wait(wait)
	{
	}

	virtual int Run(ActivityDbAccess *access);
	virtual void Done(int result);

	paf::vector<paf::string> m_titleIds;
	ActivityDbJobWait *m_wait;
};

/* a query of the statuses, kept told of the database's changes */
struct ActivityDbQuery {
	ActivityDbQuery();
	~ActivityDbQuery();

	int Unregister();
	int Setup(const ActivityDbQueryCondition *condition, const int *database);
	int GetCount(int *count, bool infoOnly);
	int GetSyncTimes(SceInt64 *baseTime, SceInt64 *lastSyncedTime,
			 SceInt64 *lastModifiedTime);
	int GetBaseTime(const paf::string *titleId, SceInt64 *baseTime);
	int UpdateCounts(int keepNewComments, paf::vector<ActivityDbNetStatus> *statuses);
	int GetStatusKey(const SceInt64 *rowid, ActivityDbStatusKey *key, paf::string *titleId);
	int GetCommentKey(const SceInt64 *rowid, ActivityDbCommentKey *key);
	int GetLastSyncedTime(const paf::string *titleId, SceInt64 *lastSyncedTime);
	int GetStatusState(const SceInt64 *rowid, bool *found, ActivityDbStatusState *state);
	int GetCommentKeys(const SceInt64 *mainRowid, paf::vector<ActivityDbCommentKey> *keys);
	int GetGoodJobKey(const ActivityDbStatusKey *status, ActivityDbGoodJobKey *key);
	int GetWaitStatusRowids(paf::vector<SceInt64> *rowids);
	int GetWaitStatuses(paf::vector<SceInt64> *rowids, paf::vector<ActivityDbStatus> *statuses);
	int GetUnreadStatusKeys(const ActivityDbQueryCondition *condition,
		paf::vector<ActivityDbStatusKey> *keys);
	int MatchStatuses(const paf::string *titleId, const paf::string *ownerId,
		const SceUInt64 *since, paf::vector<paf::string> *keys,
		paf::vector<ActivityDbSyncStatus> *matched, paf::vector<SceInt64> *unmatched);
	int GetRowids(int offset, int count, paf::vector<SceInt64> *rowids);
	int GetStatuses(paf::vector<SceInt64> *rowids, paf::vector<ActivityDbStatus> *statuses);
	int GetCommentRowids(const SceInt64 *mainRowid, int offset, int count,
			     paf::vector<SceInt64> *rowids);
	int GetComments(paf::vector<SceInt64> *rowids, paf::vector<ActivityDbComment> *comments);
	int GetGoodJob(const SceInt64 *mainRowid, ActivityDbGoodJob *goodJob);
	bool IsChanged();
	int ClearChanged();

	paf::string m_name;
	ActivityDbQueryCondition m_condition;
	paf::string m_where;		/* the condition's WHERE clause */
	volatile int m_changed;		/* the database changed since the last read */
	bool m_registered;		/* a listener of the daemon */
	int m_database;
	paf::string m_titleId;		/* the title whose sync times it reads */
};

/* the exported handle of a query */
struct ActivityDbQueryHandle {
	ActivityDbQueryHandle();
	int Destroy();
	~ActivityDbQueryHandle();
	int Create(const ActivityDbQueryCondition *condition);
	int GetInfoCount(int *count);
	int GetCountAndSyncTimes(int *count, SceInt64 *baseTime, SceInt64 *lastSyncedTime,
				 SceInt64 *lastModifiedTime);
	int GetCount(int *count);
	int GetRowids(int offset, int count, paf::vector<SceInt64> *rowids);
	int GetStatuses(paf::vector<SceInt64> *rowids, paf::vector<ActivityDbStatus> *statuses);
	int GetCommentRowids(const SceInt64 *mainRowid, int offset, int count,
			     paf::vector<SceInt64> *rowids);
	int GetComments(paf::vector<SceInt64> *rowids, paf::vector<ActivityDbComment> *comments);
	int GetGoodJob(const SceInt64 *mainRowid, ActivityDbGoodJob *goodJob);
	bool IsChanged();
	int ClearChanged();

	ActivityDbQuery *m_query;
};

/* the database a query of type reads */
int ActivityDbQueryDatabase(int type);

/*
 * the first database's recent statuses of titleId (or of every title, by scope) since since,
 * one per owner, up to max, and whether one of them has new comments; and the time its
 * statuses of titleId were last read from the network
 */
extern "C" int SceActivityDb_6268A990(paf::vector<ActivityDbStatus> *statuses, bool *hasNew,
	const paf::string *titleId, const SceUInt64 *since, int max, int scope);
extern "C" int SceActivityDb_C37F81FE(const paf::string *titleId, const SceInt64 *baseTime,
	int scope);

/* activity_db_query.cpp's functions on a database by its ID */
int ActivityDbSetBaseTime(int database, const paf::string *titleId, const SceInt64 *baseTime,
	int scope);
int ActivityDbUpdateCounts(int database, int keepNewComments,
	paf::vector<ActivityDbNetStatus> *statuses);


/* the network daemon (Sony's object name SceLsdbActNetDaemon), made at its first use */
struct ActivityDbNetDaemon {
	int Start(int priority, SceUID *threadId);
	int Stop();
};

extern "C" ActivityDbNetDaemon *SceActivityDb_7B68F69D(void);

/* the schema a database is brought up to: the activity feed's, and the news's */
int ActivityDbUpgradeActivity(dbutil::Connection *connection, int version);
int ActivityDbUpgradeNews(dbutil::Connection *connection, int version);

/*
 * The library's exports on the daemon's databases: open the activity feed's (at path) and
 * the news's, close them (the daemon stops with the last), and tell the listeners
 */
extern "C" int SceActivityDb_7B0C84A3(const char *path);
extern "C" int SceActivityDb_AD7BFF8F(void);
extern "C" int SceActivityDb_36B8046C(void);
extern "C" int SceActivityDb_738D4B49(void);

#endif
