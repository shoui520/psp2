/*
 * dbutil::Statement: a prepared SQL statement of a connection. Each step posts the transaction
 * the step ended, and a statement that has stepped counts as running on its connection until it
 * is reset or finalized.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <sqlite3.h>
#include "connection.h"
#include "statement.h"

namespace dbutil {

/*
 * A statement that has stepped runs until it is reset or finalized; with
 * DBUTIL_OPTION_WAIT_STATEMENTS the connection counts it, for a rollback to wait for.
 */

/* SQLite's error, reported to the connection's handler */
static inline int SqliteError(Connection *connection, int ret)
{
	if (connection != NULL && connection->m_errorCallback != NULL)
		connection->CheckResult(ret);
	return ret | DBUTIL_ERROR_SQLITE;
}

Statement::Statement(Connection *connection, const paf::string &sql)
	: m_connection(connection), m_db(NULL), m_stmt(NULL), m_row(-1), m_columnCount(0),
	  m_result(0), m_refCount(0), m_lock("SceDbUtilStatement", 0), m_changed(false),
	  m_active(false)
{
	if (m_connection != NULL) {
		m_db = m_connection->m_db;
		m_result = 0;

		int ret = sqlite3_prepare_v2(m_db, sql.c_str(), sql.size() + 1, &m_stmt, NULL);

		if (ret != SQLITE_OK)
			m_result = SqliteError(m_connection, ret);
		else
			m_columnCount = sqlite3_column_count(m_stmt);
	} else {
		m_result = DBUTIL_ERROR_INVALID_ARGUMENT;
	}
}

int Statement::Finalize()
{
	int ret;

	m_lock.Lock();
	if (m_stmt == NULL) {
		m_lock.Unlock();
		return DBUTIL_ERROR_INVALID_STATE;
	}
	ret = sqlite3_finalize(m_stmt);
	if (ret != SQLITE_OK)
		ret |= DBUTIL_ERROR_SQLITE;
	m_stmt = NULL;
	if (ret == 0 && m_active) {
		m_active = false;
		if (m_connection->m_option[DBUTIL_OPTION_WAIT_STATEMENTS]) {
			m_connection->m_statementMutex.Lock();
			if (--m_connection->m_activeStatements == 0)
				m_connection->m_statementCond.Notify();
			m_connection->m_statementMutex.Unlock();
		}
	}
	m_lock.Unlock();
	return ret;
}

Statement::~Statement()
{
	if (m_connection != NULL)
		m_connection->RemoveListener(this);
	Finalize();
}

void Statement::OnCommit(Event *event)
{
	m_lock.Lock();
	m_changed = true;
	m_lock.Unlock();
}

/* events may have been lost: the rows may have changed */
void Statement::OnOverflow(Event *event)
{
	m_lock.Lock();
	m_changed = true;
	m_lock.Unlock();
}

void Statement::OnClose(Event *event)
{
	Finalize();
	m_connection = NULL;
}

int Statement::AddRef()
{
	int count;

	m_lock.Lock();
	count = ++m_refCount;
	m_lock.Unlock();
	return count;
}

int Statement::Release()
{
	int count;

	m_lock.Lock();
	count = --m_refCount;
	if (count != 0) {
		m_lock.Unlock();
		return count;
	}
	m_lock.Unlock();
	delete this;
	return 0;
}

/*
 * Steps count rows at most, posting each transaction a step ends; 0 at a row, 1 past the last
 */
int Statement::Step(int count)
{
	int ret = 0;
	int result = SQLITE_OK;

	if (m_stmt == NULL || m_connection == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	m_connection->m_lock.Lock();
	m_lock.Lock();
	if (!m_active) {
		m_active = true;
		if (m_connection->m_option[DBUTIL_OPTION_WAIT_STATEMENTS]) {
			m_connection->m_statementMutex.Lock();
			m_connection->m_activeStatements++;
			m_connection->m_statementMutex.Unlock();
		}
	}
	for (int i = 0; i < count; i++, m_row++) {
		result = sqlite3_step(m_stmt);
		/* the end of the transaction the step ran in, unless it changed nothing and only
		   changes are posted */
		if (m_connection->m_committed) {
			Event event(0);

			event.type = DBUTIL_EVENT_COMMIT;
			if (!m_connection->m_option[DBUTIL_OPTION_CHANGES_ONLY] ||
			    m_connection->m_changeCount > 0)
				m_connection->Dispatch(&event);
			m_connection->m_committed = false;
			m_connection->m_changeCount = 0;
		}
		if (m_connection->m_rolledBack) {
			Event event(0);

			event.type = DBUTIL_EVENT_ROLLBACK;
			if (!m_connection->m_option[DBUTIL_OPTION_CHANGES_ONLY] ||
			    m_connection->m_changeCount > 0)
				m_connection->Dispatch(&event);
			m_connection->m_rolledBack = false;
			m_connection->m_changeCount = 0;
		}
		if (result != SQLITE_ROW)
			break;
	}
	m_lock.Unlock();
	m_connection->m_lock.Unlock();
	switch (result) {
	case SQLITE_ROW:
		return ret;
	case SQLITE_DONE:
		return 1;
	}
	if (result != SQLITE_OK)
		return SqliteError(m_connection, result);
	return 0;
}

int Statement::Reset()
{
	int ret;

	m_lock.Lock();
	m_row = -1;
	if (m_stmt == NULL) {
		m_lock.Unlock();
		return DBUTIL_ERROR_INVALID_STATE;
	}
	ret = sqlite3_reset(m_stmt);
	if (ret != SQLITE_OK)
		ret = SqliteError(m_connection, ret);
	if (ret == 0 && m_active) {
		m_active = false;
		if (m_connection->m_option[DBUTIL_OPTION_WAIT_STATEMENTS]) {
			m_connection->m_statementMutex.Lock();
			if (--m_connection->m_activeStatements == 0)
				m_connection->m_statementCond.Notify();
			m_connection->m_statementMutex.Unlock();
		}
	}
	m_lock.Unlock();
	return ret;
}

const char *Statement::GetColumnDatabaseName(int column)
{
	return sqlite3_column_database_name(m_stmt, column);
}

const char *Statement::GetColumnTableName(int column)
{
	return sqlite3_column_table_name(m_stmt, column);
}

const char *Statement::GetColumnOriginName(int column)
{
	return sqlite3_column_origin_name(m_stmt, column);
}

/* the column's declaration: its type by the declared type's first letter */
int Statement::GetColumnMetadata(int column, int *type, const char **collation, bool *notNull,
				 bool *primaryKey, bool *autoIncrement)
{
	const char *declType;
	const char *collSeq;
	int isNotNull;
	int isPrimaryKey;
	int isAutoIncrement;
	int ret;

	ret = sqlite3_table_column_metadata(m_db, sqlite3_column_database_name(m_stmt, column),
					    sqlite3_column_table_name(m_stmt, column),
					    sqlite3_column_origin_name(m_stmt, column), &declType,
					    &collSeq, &isNotNull, &isPrimaryKey, &isAutoIncrement);
	if (ret != SQLITE_OK)
		return SqliteError(m_connection, ret);
	if (type != NULL) {
		switch (declType[0]) {
		case 'I':
		case 'i':
			*type = SQLITE_INTEGER;
			break;
		case 'F':
		case 'f':
		case 'R':
		case 'r':
			*type = SQLITE_FLOAT;
			break;
		case 'T':
		case 't':
			*type = SQLITE_TEXT;
			break;
		case 'B':
		case 'b':
			*type = SQLITE_BLOB;
			break;
		case 'N':
		case 'n':
			*type = SQLITE_NULL;
			break;
		default:
			*type = 0;
			break;
		}
	}
	if (collation != NULL)
		*collation = collSeq;
	if (notNull != NULL)
		*notNull = isNotNull != 0;
	if (primaryKey != NULL)
		*primaryKey = isPrimaryKey != 0;
	if (autoIncrement != NULL)
		*autoIncrement = isAutoIncrement != 0;
	return 0;
}

/* the current row's column; a text or a blob stays the statement's */
int Statement::GetValue(int column, Value *value)
{
	if (value == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (m_stmt == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	value->name = sqlite3_column_name(m_stmt, column);
	value->type = sqlite3_column_type(m_stmt, column);
	value->size = sqlite3_column_bytes(m_stmt, column);
	switch (value->type) {
	case SQLITE_INTEGER:
		value->data.integer = sqlite3_column_int64(m_stmt, column);
		break;
	case SQLITE_FLOAT:
		value->data.real = sqlite3_column_double(m_stmt, column);
		break;
	case SQLITE_TEXT:
		value->data.text = (const char *)sqlite3_column_text(m_stmt, column);
		break;
	case SQLITE_BLOB:
		value->data.blob = sqlite3_column_blob(m_stmt, column);
		break;
	}
	return 0;
}

/* binds a value to the parameter at index; a kept text or blob is not copied */
int Statement::Bind(int index, const Value *value, bool keep)
{
	int ret = 0;

	if (m_stmt == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	switch (value->type) {
	case SQLITE_INTEGER:
		ret = sqlite3_bind_int64(m_stmt, index, value->data.integer);
		break;
	case SQLITE_FLOAT:
		ret = sqlite3_bind_double(m_stmt, index, value->data.real);
		break;
	case SQLITE_TEXT:
		ret = sqlite3_bind_text(m_stmt, index, value->data.text, value->size,
					keep ? SQLITE_STATIC : SQLITE_TRANSIENT);
		break;
	case SQLITE_BLOB:
		ret = sqlite3_bind_blob(m_stmt, index, value->data.blob, value->size,
					keep ? SQLITE_STATIC : SQLITE_TRANSIENT);
		break;
	case SQLITE_NULL:
		ret = sqlite3_bind_null(m_stmt, index);
		break;
	default:
		return ret;
	}
	if (ret != SQLITE_OK)
		return SqliteError(m_connection, ret);
	return ret;
}

/* binds the values to the parameters from the first on, each copied */
int Statement::BindAll(const Value *values, int count)
{
	int ret = 0;

	for (int i = 0; i < count; i++) {
		ret = Bind(i + 1, &values[i], false);
		if (ret != 0)
			break;
	}
	return ret;
}

int Statement::BindZeroblob(int index, int size)
{
	int ret;

	if (m_stmt == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	ret = sqlite3_bind_zeroblob(m_stmt, index, size);
	if (ret != SQLITE_OK)
		return SqliteError(m_connection, ret);
	return ret;
}

int Statement::GetParameterCount(int *count)
{
	if (count == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (m_stmt == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	*count = sqlite3_bind_parameter_count(m_stmt);
	return 0;
}

int Statement::GetParameterIndex(const paf::string &name, int *index)
{
	int ret = 0;

	if (index == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (m_stmt == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	*index = sqlite3_bind_parameter_index(m_stmt, name.c_str());
	if (*index == 0)
		return DBUTIL_ERROR_NOT_FOUND;
	return ret;
}

int Statement::GetParameterName(int index, const char **name)
{
	if (name == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (m_stmt == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	*name = sqlite3_bind_parameter_name(m_stmt, index);
	if (*name == NULL)
		return DBUTIL_ERROR_NOT_FOUND;
	return 0;
}

int Statement::ClearBindings()
{
	int ret;

	if (m_stmt == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	ret = sqlite3_clear_bindings(m_stmt);
	if (ret != SQLITE_OK)
		return SqliteError(m_connection, ret);
	return ret;
}

int Statement::ClearChanged()
{
	if (m_stmt == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	m_changed = false;
	return 0;
}

/*
 * The column holding the rowid: an INTEGER PRIMARY KEY column, else the first column whose
 * value in the first row is an integer; -1 for none
 */
int Statement::FindRowidColumn()
{
	int count = sqlite3_column_count(m_stmt);
	int first = 0;
	int found = -1;

	for (int column = 0; column < count; column++) {
		const char *database = sqlite3_column_database_name(m_stmt, column);
		const char *table = sqlite3_column_table_name(m_stmt, column);
		const char *name = sqlite3_column_name(m_stmt, column);

		if (database == NULL || table == NULL || name == NULL)
			break;

		const char *declType = NULL;
		int primaryKey = 0;

		sqlite3_table_column_metadata(sqlite3_db_handle(m_stmt), database, table, name,
					      &declType, NULL, NULL, &primaryKey, NULL);
		if (primaryKey != 0 && declType != NULL &&
		    (declType[0] == 'i' || declType[0] == 'I'))
			return column;
	}
	if (Step(1) != 0) {
		Reset();
		return found;
	}
	for (int column = first; column < count; column++) {
		if (sqlite3_column_type(m_stmt, column) == SQLITE_INTEGER) {
			found = column;
			break;
		}
	}
	Reset();
	return found;
}

}
