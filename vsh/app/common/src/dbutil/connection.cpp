/*
 * dbutil::Connection: opening and closing a database, its collations, and the databases attached
 * to it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <appmgr/appmgr.h>
#include "connection.h"
#include "dcn_client.h"
#include "statement.h"
#include "record.h"
#include "result.h"

namespace dbutil {

int Collation::Register(Connection *connection)
{
	int ret = 0;

	if (connection == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (connection->m_db == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	ret = sqlite3_create_collation(connection->m_db, m_name.c_str(), m_textRep, m_arg,
				       m_compare);
	if (ret != 0)
		ret |= DBUTIL_ERROR_SQLITE;
	return ret;
}

int Collation::Unregister(Connection *connection)
{
	int ret = 0;

	if (connection == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (connection->m_db == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	ret = sqlite3_create_collation(connection->m_db, m_name.c_str(), m_textRep, m_arg, NULL);
	if (ret != 0)
		ret |= DBUTIL_ERROR_SQLITE;
	return ret;
}

Connection::Connection()
	: m_borrowed(false), m_db(NULL), m_handler(NULL), m_committed(false), m_rolledBack(false),
	  m_changeCount(0), m_activeStatements(0), m_errorCallback(NULL), m_errorArg(NULL),
	  m_statementMutex("SceDbUtilConnection", 0),
	  m_statementCond(m_statementMutex, "SceDbUtilConnection", 0),
	  m_lock("SceDbUtilConnection", 0)
{
	sce_paf_memset(m_databaseCount, 0, sizeof(m_databaseCount));
	m_option[1] = false;
	m_option[0] = false;
	DCNClient::AddConnection(this);
}

Connection::Connection(sqlite3 *db)
	: m_borrowed(true), m_db(db), m_handler(NULL), m_committed(false), m_rolledBack(false),
	  m_changeCount(0), m_activeStatements(0), m_errorCallback(NULL), m_errorArg(NULL),
	  m_statementMutex("SceDbUtilConnection", 0),
	  m_statementCond(m_statementMutex, "SceDbUtilConnection", 0),
	  m_lock("SceDbUtilConnection", 0)
{
	sce_paf_memset(m_databaseCount, 0, sizeof(m_databaseCount));
	m_option[1] = false;
	m_option[0] = false;
	DCNClient::AddConnection(this);
}

/* an SQLite result as SceDbutil's, reported to the error callback */
int Connection::CheckResult(int result)
{
	if (result == SQLITE_OK)
		return result;
	if (m_errorCallback != NULL)
		m_errorCallback(m_errorArg, result);
	return result | DBUTIL_ERROR_SQLITE;
}

int Connection::Close()
{
	int ret = 0;

	m_lock.Lock();
	for (paf::list<EventListener *>::iterator it = m_listeners.begin();
	     it != m_listeners.end(); ++it) {
		EventListener *listener = *it;
		Event event(DBUTIL_EVENT_CLOSE);

		listener->OnEvent(&event);
	}
	m_listeners.clear();
	if (m_db != NULL) {
		sqlite3_update_hook(m_db, NULL, this);
		sqlite3_commit_hook(m_db, NULL, this);
		sqlite3_rollback_hook(m_db, NULL, this);
		ret = CheckResult(sqlite3_close(m_db));
		m_db = NULL;
	}
	sce_paf_memset(m_databaseCount, 0, sizeof(m_databaseCount));
	m_databases.clear();
	m_tables.clear();
	m_lock.Unlock();
	return ret;
}

Connection::~Connection()
{
	DCNClient::RemoveConnection(this);
	if (m_db != NULL && !m_borrowed)
		Close();
}

/* reads the rowid base of each table of a database from its sqlite_sequence */
int Connection::LoadSequences(const paf::string &database)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;
	char *sql;
	int step;

	sql = sqlite3_mprintf("SELECT name,seq FROM %s.%s", database.c_str(), "sqlite_sequence");
	if (sqlite3_prepare_v2(m_db, sql, sce_paf_strlen(sql) + 1, &stmt, NULL) == SQLITE_OK) {
		do {
			step = sqlite3_step(stmt);
			switch (step) {
			case SQLITE_ROW: {
				paf::string name = database + "." +
					(const char *)sqlite3_column_text(stmt, 0);
				SceInt64 base = sqlite3_column_int64(stmt, 1) & 0xFFFFFFFF00000000LL;

				m_tables.insert(TableMap::value_type(base, paf::string(name)));
				break;
			}
			case SQLITE_DONE:
				ret = 0;
				break;
			default:
				ret = CheckResult(step);
				break;
			}
		} while (step == SQLITE_ROW);
	}
	sqlite3_free(sql);
	sqlite3_finalize(stmt);
	return ret;
}

int Connection::GetRawPath(const char *path, char *rawPath, int size)
{
	return sceAppMgrGetRawPath(path, rawPath, size);
}

int Connection::GetDatabaseType(const paf::string &path)
{
	if (path.size() == 0)
		return DBUTIL_DATABASE_TEMPORARY;
	if (path == ":memory:")
		return DBUTIL_DATABASE_MEMORY;
	return DBUTIL_DATABASE_FILE;
}

int Connection::Open(const paf::string &path, int flags)
{
	int type = GetDatabaseType(path);
	paf::string rawPath(path);
	char resolved[256];
	int ret;

	if (type == DBUTIL_DATABASE_FILE) {
		ret = sceAppMgrGetRawPath(path.c_str(), resolved, sizeof(resolved));
		if (ret != 0)
			goto out;
		rawPath.assign(resolved, sce_paf_strlen(resolved));
	}
	m_lock.Lock();
	if (m_db != NULL) {
		ret = DBUTIL_ERROR_INVALID_STATE;
	} else {
		ret = CheckResult(sqlite3_open_v2(path.size() != 0 ? path.c_str() : NULL, &m_db,
						  flags, NULL));
		if (ret == 0) {
			sqlite3_busy_timeout(m_db, 30000);
			sqlite3_update_hook(m_db, UpdateHook, this);
			sqlite3_commit_hook(m_db, CommitHook, this);
			sqlite3_rollback_hook(m_db, RollbackHook, this);
			++m_databaseCount[type];
			m_databases.insert(DatabaseMap::value_type(paf::string("main"),
								    Database(type, rawPath)));
			ret = LoadSequences(paf::string("main"));
		}
	}
out:
	m_lock.Unlock();
	return ret;
}


int Connection::AddListener(EventListener *listener)
{
	if (listener == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	m_lock.Lock();
	for (paf::list<EventListener *>::iterator it = m_listeners.begin();
	     it != m_listeners.end(); ++it) {
		if (*it == listener) {
			m_lock.Unlock();
			return 0;
		}
	}
	m_listeners.push_back(listener);
	m_lock.Unlock();
	return 0;
}

int Connection::Prepare(const paf::string &sql, RefPtr<Statement> *statement)
{
	Statement *created;
	int ret;

	m_lock.Lock();
	if (m_db == NULL) {
		m_lock.Unlock();
		return DBUTIL_ERROR_INVALID_STATE;
	}
	created = new Statement(this, sql);
	if (created == NULL) {
		m_lock.Unlock();
		return DBUTIL_ERROR_NO_MEMORY;
	}
	ret = created->m_result;
	if (ret != 0) {
		delete created;
	} else {
		AddListener(created);
		*statement = RefPtr<Statement>(created);
	}
	m_lock.Unlock();
	return ret;
}

int Connection::Execute(const paf::string &sql, int count, const Value *values, Result *result,
			SceInt64 *lastRowid, int *changes)
{
	RefPtr<Statement> statement;
	int ret;

	m_lock.Lock();
	if (lastRowid != NULL)
		*lastRowid = 0;
	if (changes != NULL)
		*changes = 0;
	if (m_db == NULL) {
		ret = DBUTIL_ERROR_INVALID_STATE;
	} else {
		ret = Prepare(sql, &statement);
		if (ret == 0)
			ret = statement->BindAll(values, count);
		if (ret == 0) {
			if (result != NULL) {
				ret = result->Load(this, statement);
				if (ret != 0)
					statement.Clear();
				else
					AddListener(result);
			} else {
				do {
					ret = statement->Step(1);
				} while (ret == 0);
				if (ret == 1)
					ret = 0;
				statement->Reset();
			}
			if (lastRowid != NULL)
				*lastRowid = sqlite3_last_insert_rowid(m_db);
			if (changes != NULL)
				*changes = sqlite3_changes(m_db);
		}
	}
	if ((result == NULL || ret != 0) && statement.Get() != NULL)
		statement->Finalize();
	m_lock.Unlock();
	return ret;
}

/* the "database.table" whose rowids the rowid is among */
bool Connection::FindTable(SceInt64 rowid, paf::string *name)
{
	m_lock.Lock();
	TableMap::iterator it = m_tables.find(rowid & 0xFFFFFFFF00000000LL);

	if (it == m_tables.end()) {
		m_lock.Unlock();
		return false;
	}
	*name = it->second;
	m_lock.Unlock();
	return true;
}

int Connection::OpenRecord(const paf::string &database, const paf::string &table, SceInt64 rowid,
			   const paf::string &columns, RefPtr<Record> *record)
{
	paf::string name;

	if (rowid == 0)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (database.size() != 0 || table.size() != 0 || !FindTable(rowid, &name))
		name = database + "." + table;
	*record = RefPtr<Record>(new Record(this, name, columns, rowid, NULL));
	if (record->Get() == NULL)
		return DBUTIL_ERROR_NO_MEMORY;
	if (!(*record)->m_valid) {
		record->Clear();
		return DBUTIL_ERROR_NOT_FOUND;
	}
	AddListener(record->Get());
	return 0;
}


/* a record joining a row of each table; an empty table name is found from the row's rowid */
int Connection::OpenRecord(const paf::vector<paf::string> &tables,
			   const paf::vector<SceInt64> &rowids, const paf::string &columns,
			   RefPtr<Record> *record)
{
	paf::vector<paf::string> names;

	for (unsigned int i = 0; i < tables.size(); i++) {
		paf::string name;

		if (tables[i].size() != 0 || !FindTable(rowids[i], &name))
			names.push_back(tables[i]);
		else
			names.push_back(name);
	}
	*record = RefPtr<Record>(new Record(this, names, columns, rowids, NULL));
	if (record->Get() == NULL)
		return DBUTIL_ERROR_NO_MEMORY;
	if (!(*record)->m_valid) {
		record->Clear();
		return DBUTIL_ERROR_NOT_FOUND;
	}
	AddListener(record->Get());
	return 0;
}

/* the last error: SQLite's extended code, an I/O error or a lock as SceDbutil's */
int Connection::GetError()
{
	int code = 0;

	if (m_db != NULL) {
		code = sqlite3_extended_errcode(m_db);
		switch ((SceUInt8)code) {
		case SQLITE_LOCKED:
			return DBUTIL_ERROR_LOCKED;
		case SQLITE_IOERR:
			return DBUTIL_ERROR_IO | (SceUInt8)((unsigned int)code >> 8);
		}
	}
	return code;
}

const char *Connection::GetErrorMessage()
{
	return m_db != NULL ? sqlite3_errmsg(m_db) : NULL;
}


/* attaches the database at path under name, and reads its tables' rowid bases */
int Connection::Attach(const paf::string &path, const paf::string &name)
{
	Database database(0, path);
	char resolved[256];
	int ret;

	database.type = GetDatabaseType(path);
	ret = sceAppMgrGetRawPath(path.c_str(), resolved, sizeof(resolved));
	if (ret != 0)
		return ret;
	database.path.assign(resolved, sce_paf_strlen(resolved));
	ret = Execute("ATTACH '" + path + "' AS " + name, 0, NULL, NULL, NULL, NULL);
	if (ret == 0) {
		m_lock.Lock();
		++m_databaseCount[database.type];
		m_databases.insert(DatabaseMap::value_type(paf::string(name), Database(database)));
		m_lock.Unlock();
		ret = LoadSequences(name);
	}
	return ret;
}

int Connection::Detach(const paf::string &name)
{
	int ret;

	m_lock.Lock();
	ret = Execute("DETACH " + name, 0, NULL, NULL, NULL, NULL);
	if (ret != 0) {
		m_lock.Unlock();
		return ret;
	}
	DatabaseMap::iterator database = m_databases.find(name);
	if (database != m_databases.end()) {
		--m_databaseCount[database->second.type];
		m_databases.erase(database->first);
	}
	for (TableMap::iterator it = m_tables.begin(); it != m_tables.end();) {
		if (it->second == name)
			m_tables.erase(it++);
		else
			++it;
	}
	m_lock.Unlock();
	return 0;
}


void Connection::Interrupt()
{
	m_lock.Lock();
	if (m_db != NULL)
		sqlite3_interrupt(m_db);
	m_lock.Unlock();
}

SceInt64 Connection::GetLastInsertRowid()
{
	m_lock.Lock();
	if (m_db != NULL) {
		SceInt64 rowid = sqlite3_last_insert_rowid(m_db);

		m_lock.Unlock();
		return rowid;
	}
	m_lock.Unlock();
	return 0;
}

int Connection::GetChanges()
{
	int changes = 0;

	m_lock.Lock();
	if (m_db != NULL)
		changes = sqlite3_changes(m_db);
	m_lock.Unlock();
	return changes;
}

int Connection::GetTotalChanges()
{
	int changes = 0;

	m_lock.Lock();
	if (m_db != NULL)
		changes = sqlite3_total_changes(m_db);
	m_lock.Unlock();
	return changes;
}

int Connection::OpenBlob(const paf::string &database, const paf::string &table,
			 const paf::string &column, SceInt64 rowid, int flags, sqlite3_blob **blob)
{
	if (blob == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (m_db == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	return CheckResult(sqlite3_blob_open(m_db, database.c_str(), table.c_str(), column.c_str(),
					     rowid, flags, blob));
}

int Connection::CloseBlob(sqlite3_blob *blob)
{
	if (blob == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (m_db == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	return CheckResult(sqlite3_blob_close(blob));
}

int Connection::ReadBlob(sqlite3_blob *blob, void *buffer, int size, int offset)
{
	if (blob == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (m_db == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	return CheckResult(sqlite3_blob_read(blob, buffer, size, offset));
}

int Connection::WriteBlob(sqlite3_blob *blob, const void *buffer, int size, int offset)
{
	if (blob == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (m_db == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	return CheckResult(sqlite3_blob_write(blob, buffer, size, offset));
}

int Connection::GetBlobSize(sqlite3_blob *blob, int *size)
{
	if (blob == NULL || size == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (m_db == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	*size = sqlite3_blob_bytes(blob);
	return 0;
}


int Connection::Begin(int mode)
{
	switch (mode) {
	case DBUTIL_TRANSACTION_DEFERRED:
		return Execute("begin deferred transaction;", 0, NULL, NULL, NULL, NULL);
	case DBUTIL_TRANSACTION_IMMEDIATE:
		return Execute("begin immediate transaction;", 0, NULL, NULL, NULL, NULL);
	case DBUTIL_TRANSACTION_EXCLUSIVE:
		return Execute("begin exclusive transaction;", 0, NULL, NULL, NULL, NULL);
	}
	return DBUTIL_ERROR_INVALID_ARGUMENT;
}

int Connection::Commit()
{
	return Execute("commit;", 0, NULL, NULL, NULL, NULL);
}

/* with DBUTIL_OPTION_WAIT_STATEMENTS, waits for the running statements first */
int Connection::Rollback()
{
	int ret;

	m_lock.Lock();
	if (m_option[DBUTIL_OPTION_WAIT_STATEMENTS]) {
		m_statementMutex.Lock();
		if (m_activeStatements > 0) {
			m_lock.Unlock();
			m_statementCond.Wait(0);
			m_lock.Lock();
		}
		m_statementMutex.Unlock();
		ret = Execute("rollback;", 0, NULL, NULL, NULL, NULL);
	} else {
		ret = Execute("rollback;", 0, NULL, NULL, NULL, NULL);
	}
	m_lock.Unlock();
	return ret;
}


int Connection::SetBusyTimeout(int milliseconds)
{
	int ret;

	m_lock.Lock();
	if (milliseconds < 0) {
		m_lock.Unlock();
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	}
	if (m_db == NULL)
		ret = DBUTIL_ERROR_INVALID_STATE;
	else
		ret = CheckResult(sqlite3_busy_timeout(m_db, milliseconds));
	ret = CheckResult(ret);
	m_lock.Unlock();
	return ret;
}

int Connection::SetBusyHandler(int (*handler)(void *arg, int count), void *arg)
{
	int ret;

	m_lock.Lock();
	if (m_db != NULL)
		ret = CheckResult(sqlite3_busy_handler(m_db, handler, arg));
	else
		ret = DBUTIL_ERROR_INVALID_STATE;
	m_lock.Unlock();
	return ret;
}

int Connection::SetOption(int option, bool value)
{
	m_lock.Lock();
	m_option[option] = value;
	m_lock.Unlock();
	return 0;
}

/* the name a database at path is attached under */
bool Connection::FindDatabaseName(const paf::string &path, paf::string *name)
{
	m_lock.Lock();
	for (DatabaseMap::iterator it = m_databases.begin(); it != m_databases.end(); ++it) {
		if (it->second.path == path) {
			*name = it->first;
			m_lock.Unlock();
			return true;
		}
	}
	m_lock.Unlock();
	return false;
}

/* an event of another process names its database by path: give it this connection's name */
__attribute__((noinline)) int Connection::TranslateEvent(const Event *event, Event *translated)
{
	int ret = 0;

	switch (event->type) {
	case DBUTIL_EVENT_COMMIT:
	case DBUTIL_EVENT_ROLLBACK:
	case DBUTIL_EVENT_OVERFLOW:
		break;
	case DBUTIL_EVENT_UPDATE:
	case DBUTIL_EVENT_INSERT:
	case DBUTIL_EVENT_DELETE:
		if (!event->local)
			FindDatabaseName(event->database, &translated->database);
		break;
	default:
		ret = DBUTIL_ERROR_INVALID_TYPE;
		break;
	}
	return ret;
}

/* whether one of the connection's databases is among paths (raw paths joined by ",") */
bool Connection::UsesDatabase(const paf::string &paths)
{
	bool found = false;

	m_lock.Lock();
	for (DatabaseMap::iterator it = m_databases.begin(); it != m_databases.end(); ++it) {
		const paf::string &path = it->second.path;

		if (paths.find(path.c_str(), 0, path.size()) != paf::string::npos) {
			found = true;
			break;
		}
	}
	m_lock.Unlock();
	return found;
}


/* tells the listeners of an event that concerns this connection's databases */
int Connection::Deliver(const Event *event)
{
	Event translated(*event);
	int ret;

	m_lock.Lock();
	if (!event->local && event->type != DBUTIL_EVENT_OVERFLOW &&
	    !UsesDatabase(event->database)) {
		m_lock.Unlock();
		return 0;
	}
	ret = TranslateEvent(event, &translated);
	if (ret == 0) {
		/* a listener may remove itself or another: tell only those still listening */
		paf::list<EventListener *> listeners(m_listeners);

		for (paf::list<EventListener *>::iterator it = listeners.begin();
		     it != listeners.end(); ++it) {
			EventListener *listener = *it;

			for (paf::list<EventListener *>::iterator current = m_listeners.begin();
			     current != m_listeners.end(); ++current) {
				if (*current == listener) {
					listener->OnEvent(&translated);
					break;
				}
			}
		}
	}
	m_lock.Unlock();
	return ret;
}

int Connection::RemoveListener(EventListener *listener)
{
	m_lock.Lock();
	for (paf::list<EventListener *>::iterator it = m_listeners.begin();
	     it != m_listeners.end(); ++it) {
		if (*it == listener) {
			m_listeners.erase(it);
			break;
		}
	}
	m_lock.Unlock();
	return 0;
}

/* the listener told of this connection's own commits, rollbacks and changes */
int Connection::SetHandler(EventListener *handler)
{
	m_lock.Lock();
	m_handler = handler;
	m_lock.Unlock();
	return 0;
}

/* the raw path of the database attached under name; empty when there is none */
paf::string Connection::GetDatabasePath(const paf::string &name)
{
	paf::string path;

	m_lock.Lock();
	DatabaseMap::iterator it = m_databases.find(name);

	if (it != m_databases.end())
		path = it->second.path;
	m_lock.Unlock();
	return path;
}

paf::string Connection::GetDatabaseName(const paf::string &path)
{
	paf::string name;

	FindDatabaseName(path, &name);
	return name;
}


/* the raw paths of the attached databases */
int Connection::CollectDatabasePaths(paf::vector<paf::string> *paths)
{
	m_lock.Lock();
	paths->clear();
	for (DatabaseMap::iterator it = m_databases.begin(); it != m_databases.end(); ++it)
		paths->push_back(it->second.path);
	m_lock.Unlock();
	return 0;
}

int Connection::GetDatabasePaths(paf::vector<paf::string> *paths)
{
	return CollectDatabasePaths(paths);
}

/* the handler can be set once */
int Connection::SetErrorHandler(const ErrorHandler *handler)
{
	if (m_errorCallback != NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	if (handler == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	m_errorCallback = handler->callback;
	m_errorArg = handler->arg;
	return 0;
}


/* the raw paths of the file databases, each followed by separator, the last one's taken off */
int Connection::JoinFileDatabasePaths(const paf::string &separator, paf::string *paths)
{
	m_lock.Lock();
	paths->assign(NULL, 0);
	for (DatabaseMap::iterator it = m_databases.begin(); it != m_databases.end(); ++it) {
		if (it->second.type == DBUTIL_DATABASE_FILE) {
			paths->append(it->second.path);
			paths->append(separator);
		}
	}
	if (paths->size() != 0)
		paths->erase(paths->size() - 1, 1);
	m_lock.Unlock();
	return 0;
}

/*
 * Posts an event of this connection to the DCN server, naming its database by the raw path, so
 * other processes' connections can find it; a commit or rollback names every file database
 */
int Connection::Dispatch(Event *event)
{
	event->source = this;
	event->local = m_databaseCount[DBUTIL_DATABASE_FILE] <= 0;
	if (event->type >= DBUTIL_EVENT_COMMIT && event->type <= DBUTIL_EVENT_ROLLBACK) {
		JoinFileDatabasePaths(",", &event->database);
	} else {
		m_lock.Lock();
		DatabaseMap::iterator it = m_databases.find(event->database);

		if (it == m_databases.end() || it->second.type == DBUTIL_DATABASE_FILE) {
			paf::string path;

			if (FindDatabasePath(event->database, &path))
				event->database = path;
		} else {
			event->local = 1;
			event->source = this;
		}
		m_lock.Unlock();
	}
	return DCNClient::Post(event);
}

/* sqlite3_update_hook: tells the handler, then the other connections */
void Connection::UpdateHook(void *arg, int operation, const char *database, const char *table,
			    sqlite3_int64 rowid)
{
	Connection *connection = (Connection *)arg;
	Event event(0);

	event.database = database;
	event.table = table;
	event.rowid = rowid;

	EventListener *handler = connection->m_handler;

	switch (operation) {
	case SQLITE_UPDATE:
		event.type = DBUTIL_EVENT_UPDATE;
		break;
	case SQLITE_INSERT:
		event.type = DBUTIL_EVENT_INSERT;
		break;
	case SQLITE_DELETE:
		event.type = DBUTIL_EVENT_DELETE;
		break;
	}
	if (handler != NULL)
		handler->OnEvent(&event);
	connection->m_changeCount++;
	connection->Dispatch(&event);
}

/* sqlite3_commit_hook */
int Connection::CommitHook(void *arg)
{
	Connection *connection = (Connection *)arg;

	if (connection != NULL) {
		EventListener *handler = connection->m_handler;

		connection->m_committed = true;
		if (handler != NULL) {
			Event event(DBUTIL_EVENT_COMMIT);

			handler->OnEvent(&event);
		}
	}
	return 0;
}

/* sqlite3_rollback_hook */
void Connection::RollbackHook(void *arg)
{
	Connection *connection = (Connection *)arg;

	if (connection != NULL) {
		EventListener *handler = connection->m_handler;

		connection->m_rolledBack = true;
		if (handler != NULL) {
			Event event(DBUTIL_EVENT_ROLLBACK);

			handler->OnEvent(&event);
		}
	}
}

}
