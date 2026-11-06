/*
 * dbutil::RowidResult: a result of one rowid column, read into a rowid list on its own heap.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/stdlib.h>
#include "connection.h"
#include "statement.h"
#include "record.h"
#include "result.h"
#include "value.h"
#include "rowid_result.h"

namespace dbutil {

RowidResult::RowidResult() : m_column(-1)
{
}

RowidResult::RowidResult(const RowidAllocator &alloc) : m_column(-1), m_rowids(alloc)
{
}

/* forgets the rows, the column and the statement and stops listening */
void RowidResult::Clear()
{
	m_changed = false;
	m_rowids.resize(0);
	m_rowCount = 0;
	m_column = -1;
	m_database.clear();
	m_table.clear();
	if (m_connection != NULL) {
		m_connection->RemoveListener(this);

		paf::thread::RMutex *lock = &m_connection->m_lock;

		lock->Lock();
		if (m_statement.Get() != NULL) {
			m_statement->Finalize();
			m_statement.Clear();
		}
		lock->Unlock();
	}
}

RowidResult::~RowidResult()
{
	Clear();
}

/* Sony's test lets the index one past the last row through */
SceInt64 RowidResult::GetRowid(int index)
{
	SceInt64 rowid;

	m_lock.Lock();
	if (index >= 0 && index <= (int)m_rowids.size())
		rowid = m_rowids[index];
	else
		rowid = 0;
	m_lock.Unlock();
	return rowid;
}

int RowidResult::OpenRecord(int index, const paf::string &columns, RefPtr<Record> *record)
{
	SceInt64 rowid = GetRowid(index);

	if (m_connection != NULL)
		return m_connection->OpenRecord(m_database, m_table, rowid, columns, record);
	return DBUTIL_ERROR_INVALID_STATE;
}

int RowidResult::OpenRecord(SceInt64 rowid, const paf::string &columns, RefPtr<Record> *record)
{
	if (m_connection != NULL)
		return m_connection->OpenRecord(m_database, m_table, rowid, columns, record);
	return DBUTIL_ERROR_INVALID_STATE;
}

/* steps the statement to its end, the rowid of each row into rows when given */
int RowidResult::LoadRows(RefPtr<Statement> statement, int column, int *count, RowidList *rows)
{
	int ret;

	do {
		ret = statement->Step(1);
		if (ret != 0)
			continue;
		if (rows != NULL) {
			Value value;

			ret = statement->GetValue(column, &value);
			if (ret != 0)
				continue;
			rows->push_back(value.data.integer);
		}
		(*count)++;
	} while (ret == 0);
	if (ret == 1)
		ret = 0;
	return ret;
}

/*
 * Reads the statement again and walks the new rowids against the old ones, as
 * RecordResult::Refresh does with its rows. The new rowids replace the old unless the
 * callback stops the walk.
 */
int RowidResult::Refresh(RowCallback callback, void *arg)
{
	RowidList rows(m_rowids.alloc);
	int count = 0;
	paf::thread::RMutex *lock = &m_connection->m_lock;
	int ret;

	lock->Lock();
	m_lock.Lock();
	ret = LoadRows(m_statement, m_column, &count, &rows);
	m_statement->Reset();
	if (ret != 0) {
		m_lock.Unlock();
		lock->Unlock();
		return ret;
	}
	if (callback == NULL) {
		m_rowids = rows;
		m_rowCount = count;
		m_changed = false;
		m_lock.Unlock();
		lock->Unlock();
		return ret;
	}

	int old = 0;

	for (int i = 0; i < count; i++) {
		if (rows[i] == m_rowids[old]) {
			old++;
			continue;
		}

		int found;

		for (found = old; found < m_rowCount; found++) {
			if (rows[i] == m_rowids[found])
				goto removed;
		}
		if (callback(0, rows[i], i, 0, arg) != 0) {
			m_lock.Unlock();
			lock->Unlock();
			return DBUTIL_ERROR_CANCELED;
		}
		continue;
removed:
		for (; old < found; old++) {
			if (callback(1, m_rowids[old], old, 0, arg) != 0) {
				m_lock.Unlock();
				lock->Unlock();
				return DBUTIL_ERROR_CANCELED;
			}
		}
		old++;
	}
	for (; old < m_rowCount; old++) {
		if (callback(1, m_rowids[old], old, 0, arg) != 0) {
			m_lock.Unlock();
			lock->Unlock();
			return DBUTIL_ERROR_CANCELED;
		}
	}
	m_lock.Unlock();
	lock->Unlock();
	m_rowids = rows;
	m_rowCount = count;
	m_changed = false;
	return ret;
}

/* prepares sql with values bound as the result's statement; the rows are read by Refresh */
int RowidResult::Prepare(const paf::string &sql, int count, const Value *values)
{
	int ret;

	if (m_connection == NULL)
		return DBUTIL_ERROR_INVALID_STATE;

	RefPtr<Statement> statement;
	paf::thread::RMutex *lock = &m_connection->m_lock;

	lock->Lock();
	m_lock.Lock();
	ret = m_connection->Prepare(sql, &statement);
	if (ret != 0) {
		m_lock.Unlock();
		lock->Unlock();
		return ret;
	}
	ret = statement->BindAll(values, count);
	if (ret != 0) {
		m_lock.Unlock();
		lock->Unlock();
		return ret;
	}

	int column = statement->FindRowidColumn();

	if (column == -1) {
		m_lock.Unlock();
		lock->Unlock();
		return DBUTIL_ERROR_NO_ROWID_COLUMN;
	}

	const char *database = statement->GetColumnDatabaseName(column);
	const char *table = statement->GetColumnTableName(column);

	statement->Reset();
	m_statement->Finalize();
	m_statement = statement;
	m_column = column;
	m_database = database;
	m_table = table;
	m_changed = true;
	m_lock.Unlock();
	lock->Unlock();
	return ret;
}

/* Sony's loop stops before the second rowid: the first two are never swapped */
int RowidResult::Shuffle()
{
	m_lock.Lock();
	for (unsigned int i = m_rowids.size() - 1; i > 1; i--) {
		unsigned int j = sce_paf_rand() % (i + 1);
		SceInt64 rowid = m_rowids[i];

		m_rowids[i] = m_rowids[j];
		m_rowids[j] = rowid;
	}
	m_lock.Unlock();
	return 0;
}

/* takes the statement's rowid column, then reads the rows */
int RowidResult::Load(Connection *connection, RefPtr<Statement> statement)
{
	int ret;

	if (statement.Get() == NULL)
		return 0;
	Clear();
	m_connection = connection;
	m_statement = statement;

	paf::thread::RMutex *lock = &m_connection->m_lock;

	lock->Lock();
	m_lock.Lock();
	m_column = m_statement->FindRowidColumn();
	if (m_column == -1) {
		m_statement->Finalize();
		m_statement.Clear();
		m_lock.Unlock();
		lock->Unlock();
		return DBUTIL_ERROR_NO_ROWID_COLUMN;
	}
	m_database = m_statement->GetColumnDatabaseName(m_column);
	m_table = m_statement->GetColumnTableName(m_column);
	ret = LoadRows(statement, m_column, &m_rowCount, &m_rowids);
	if (ret != 0) {
		m_statement->Finalize();
		m_statement.Clear();
		m_lock.Unlock();
		lock->Unlock();
		return ret;
	}
	m_statement->Reset();
	m_lock.Unlock();
	lock->Unlock();
	return 0;
}

}
