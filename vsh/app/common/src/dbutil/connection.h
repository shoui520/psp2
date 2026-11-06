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

/* Connection::SetOption */
#define DBUTIL_OPTION_WAIT_STATEMENTS	0	/* a rollback waits for the running statements */
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
		if (other.m_ptr != m_ptr) {
			if (m_ptr != NULL)
				m_ptr->Release();
			m_ptr = other.m_ptr;
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
		: reserved(0), local(0), source(NULL), type(type), database((const char *)NULL, 0),
		  table((const char *)NULL, 0), rowid(0)
	{
	}

	SceUInt32 reserved;
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

/* the DCN client keeps every connection, to tell them of other processes' changes */
class DCNClient {
public:
	static int AddConnection(Connection *connection);
	static int RemoveConnection(Connection *connection);
};

}

#endif
