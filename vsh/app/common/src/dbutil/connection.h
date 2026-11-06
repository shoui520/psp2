/*
 * dbutil::Connection: SceDbutil's connection to an SQLite database, with the databases attached to
 * it, its tables by rowid base, and the listeners told of its changes.
 *
 * Sony's names: the namespace and the class (the shell's "new dbutil::Connection returned NULL"),
 * DCNClient and DCNServer (their "dbutil::DCNClient::Initialize() failed"), and the object names
 * "SceDbUtilConnection", "SceDbUtilDcnClient", ... Every member and method name is ours.
 */
#ifndef DBUTIL_CONNECTION_H
#define DBUTIL_CONNECTION_H

#include <stddef.h>
#include <scetypes.h>
#include <sqlite3.h>
#include <std/string>
#include <std/map>
#include <std/list>
#include <std/vector>
#include <thread/thread.h>

/* SceDbutil's errors; an SQLite error is its result code ORed into DBUTIL_ERROR_SQLITE */
#define DBUTIL_ERROR_INVALID_ARGUMENT	0x80103101
#define DBUTIL_ERROR_NO_MEMORY		0x80103102
#define DBUTIL_ERROR_INVALID_STATE	0x80103103	/* not open, already open, no DCN client */
#define DBUTIL_ERROR_NOT_FOUND		0x80103104
#define DBUTIL_ERROR_NO_ROWID_COLUMN	0x80103105	/* a statement selects no rowid */
#define DBUTIL_ERROR_CANCELED		0x80103107	/* a refresh's callback stopped it */
#define DBUTIL_ERROR_INVALID_TYPE	0x80103108	/* an event's or a value's */
#define DBUTIL_ERROR_SQLITE		0x80103200
#define DBUTIL_ERROR_IO			0x80103D00	/* ORed with SQLITE_IOERR's extended code */
#define DBUTIL_ERROR_LOCKED		0x80103D12

/* where a database lives (Connection::GetDatabaseType) */
#define DBUTIL_DATABASE_FILE		1
#define DBUTIL_DATABASE_TEMPORARY	2	/* an empty name: SQLite's temporary database */
#define DBUTIL_DATABASE_MEMORY		3	/* ":memory:" */
#define DBUTIL_DATABASE_TYPE_COUNT	4

/* Event::type */
#define DBUTIL_EVENT_COMMIT		1
#define DBUTIL_EVENT_ROLLBACK		2
#define DBUTIL_EVENT_UPDATE		3
#define DBUTIL_EVENT_INSERT		4
#define DBUTIL_EVENT_DELETE		5
#define DBUTIL_EVENT_OVERFLOW		6	/* the DCN server's queue filled and was flushed */
#define DBUTIL_EVENT_CLOSE		7

/* Connection::Begin */
#define DBUTIL_TRANSACTION_DEFERRED	0
#define DBUTIL_TRANSACTION_IMMEDIATE	1
#define DBUTIL_TRANSACTION_EXCLUSIVE	2

/* Connection::SetOption */
#define DBUTIL_OPTION_WAIT_STATEMENTS	0	/* a rollback waits for the running statements */
#define DBUTIL_OPTION_CHANGES_ONLY	1	/* a commit or rollback that changed no row is not posted */
#define DBUTIL_OPTION_COUNT		2

namespace dbutil {

class Connection;
class Statement;
class Record;
class Result;
struct Value;

/* a reference to an object counted by its AddRef and Release */
template <class T>
class RefPtr {
public:
	RefPtr() : m_ptr(NULL)
	{
	}

	RefPtr(T *ptr) : m_ptr(ptr)
	{
		if (m_ptr != NULL)
			m_ptr->AddRef();
	}

	RefPtr(const RefPtr &other) : m_ptr(other.m_ptr)
	{
		if (m_ptr != NULL)
			m_ptr->AddRef();
	}

	~RefPtr()
	{
		if (m_ptr != NULL)
			m_ptr->Release();
	}

	RefPtr &operator=(const RefPtr &other)
	{
		T *ptr = other.m_ptr;

		if (ptr != m_ptr) {
			if (m_ptr != NULL)
				m_ptr->Release();
			m_ptr = ptr;
			if (m_ptr != NULL)
				m_ptr->AddRef();
		}
		return *this;
	}

	/* drops the reference */
	void Clear()
	{
		if (m_ptr != NULL) {
			m_ptr->Release();
			m_ptr = NULL;
		}
	}

	T *operator->() const
	{
		return m_ptr;
	}

	T *Get() const
	{
		return m_ptr;
	}

	T *m_ptr;
};

/* a change to a connection's databases */
struct Event {
	Event(int type)
		: processId(0), local(0), source(NULL), type(type), database((const char *)NULL, 0),
		  table((const char *)NULL, 0), rowid(0)
	{
	}

	Event(SceUID processId, SceBool local, Connection *source, int type, const char *database,
	      const char *table, SceInt64 rowid)
		: processId(processId), local(local), source(source), type(type), database(database),
		  table(table), rowid(rowid)
	{
	}

	SceUID processId;		/* the process that posted it (the DCN client's) */
	SceBool local;			/* no file database: only the source connection is told */
	Connection *source;
	int type;			/* DBUTIL_EVENT_* */
	paf::string database;
	paf::string table;
	SceInt64 rowid;
};

/* told of a connection's events; OnEvent hands each to the handler for its type */
class EventListener {
public:
	virtual void OnEvent(Event *event);
	virtual ~EventListener()
	{
	}
	virtual void OnCommit(Event *event)
	{
	}
	virtual void OnRollback(Event *event)
	{
	}
	virtual void OnChange(int type, paf::string *database, paf::string *table, SceInt64 rowid)
	{
	}
	virtual void OnOverflow(Event *event)
	{
	}
	virtual void OnClose(Event *event)
	{
	}
};

/* a collation the caller registers on a connection (sqlite3_create_collation) */
class Collation {
public:
	Collation() : m_textRep(SQLITE_UTF8), m_arg(NULL), m_compare(NULL)
	{
	}

	virtual ~Collation()
	{
	}

	int Register(Connection *connection);
	int Unregister(Connection *connection);

	paf::string m_name;
	int m_textRep;
	void *m_arg;
	int (*m_compare)(void *arg, int leftLength, const void *left, int rightLength,
			 const void *right);
};

class Connection {
public:
	/* told of every SQLite error (Connection::SetErrorHandler) */
	struct ErrorHandler {
		void (*callback)(void *arg, int result);
		void *arg;
	};

	/* an attached database: its type and the raw path it was opened from */
	struct Database {
		Database(int type, const paf::string &path) : type(type), path(path)
		{
		}

		int type;
		paf::string path;
	};

	typedef paf::map<paf::string, Database> DatabaseMap;
	/* a table's rowid base (the high word of its AUTOINCREMENT sequence) to "database.table" */
	typedef paf::map<SceInt64, paf::string> TableMap;

	Connection();
	Connection(sqlite3 *db);
	virtual ~Connection();

	int CheckResult(int result);
	int Close();
	int GetRawPath(const char *path, char *rawPath, int size);
	int GetDatabaseType(const paf::string &path);
	int Open(const paf::string &path, int flags);
	int LoadSequences(const paf::string &database);
	int AddListener(EventListener *listener);
	int Prepare(const paf::string &sql, RefPtr<Statement> *statement);
	int Execute(const paf::string &sql, int count, const Value *values, Result *result,
		    SceInt64 *lastRowid, int *changes);
	bool FindTable(SceInt64 rowid, paf::string *name);
	int OpenRecord(const paf::string &database, const paf::string &table, SceInt64 rowid,
		       const paf::string &columns, RefPtr<Record> *record);
	int OpenRecord(const paf::vector<paf::string> &tables, const paf::vector<SceInt64> &rowids,
		       const paf::string &columns, RefPtr<Record> *record);
	int GetError();
	const char *GetErrorMessage();
	int Attach(const paf::string &path, const paf::string &name);
	int Detach(const paf::string &name);
	void Interrupt();
	SceInt64 GetLastInsertRowid();
	int GetChanges();
	int GetTotalChanges();
	int OpenBlob(const paf::string &database, const paf::string &table,
		     const paf::string &column, SceInt64 rowid, int flags, sqlite3_blob **blob);
	int CloseBlob(sqlite3_blob *blob);
	int ReadBlob(sqlite3_blob *blob, void *buffer, int size, int offset);
	int WriteBlob(sqlite3_blob *blob, const void *buffer, int size, int offset);
	int GetBlobSize(sqlite3_blob *blob, int *size);
	int Begin(int mode);
	int Commit();
	int Rollback();
	int SetBusyTimeout(int milliseconds);
	int SetBusyHandler(int (*handler)(void *arg, int count), void *arg);
	int SetOption(int option, bool value);
	bool FindDatabaseName(const paf::string &path, paf::string *name);

	/* the raw path of the database attached under name, when there is one */
	bool FindDatabasePath(const paf::string &name, paf::string *path)
	{
		bool found;

		m_lock.Lock();
		DatabaseMap::iterator it = m_databases.find(name);

		found = it != m_databases.end();
		if (found)
			*path = it->second.path;
		m_lock.Unlock();
		return found;
	}

	int TranslateEvent(const Event *event, Event *translated);
	bool UsesDatabase(const paf::string &paths);
	int Deliver(const Event *event);
	int RemoveListener(EventListener *listener);
	int SetHandler(EventListener *handler);
	paf::string GetDatabasePath(const paf::string &name);
	paf::string GetDatabaseName(const paf::string &path);
	int CollectDatabasePaths(paf::vector<paf::string> *paths);
	int GetDatabasePaths(paf::vector<paf::string> *paths);
	int SetErrorHandler(const ErrorHandler *handler);
	int JoinFileDatabasePaths(const paf::string &separator, paf::string *paths);
	int Dispatch(Event *event);

	static void UpdateHook(void *arg, int operation, const char *database, const char *table,
			       sqlite3_int64 rowid);
	static int CommitHook(void *arg);
	static void RollbackHook(void *arg);

	bool m_borrowed;		/* m_db is the caller's: not closed with the connection */
	sqlite3 *m_db;
	int m_databaseCount[DBUTIL_DATABASE_TYPE_COUNT];
	bool m_option[DBUTIL_OPTION_COUNT];
	DatabaseMap m_databases;
	TableMap m_tables;
	paf::list<EventListener *> m_listeners;
	EventListener *m_handler;
	bool m_committed;
	bool m_rolledBack;
	int m_changeCount;
	int m_activeStatements;
	void (*m_errorCallback)(void *arg, int result);
	void *m_errorArg;
	paf::thread::Mutex m_statementMutex;
	paf::thread::Cond m_statementCond;
	paf::thread::RMutex m_lock;
};

}

#endif
