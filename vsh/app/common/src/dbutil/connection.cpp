/*
 * dbutil::Connection: opening and closing a database, its collations, and the databases attached
 * to it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <appmgr/appmgr.h>
#include "connection.h"
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

}
