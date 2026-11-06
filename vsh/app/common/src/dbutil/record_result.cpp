/*
 * dbutil::RecordResult: a result of rowid columns, read row by row into rowid lists, and the
 * records those rows name.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include "connection.h"
#include "statement.h"
#include "record.h"
#include "result.h"
#include "record_result.h"

namespace dbutil {

/* the same rowids in the same order */
static inline bool SameRow(const RecordResult::Row &left, const RecordResult::Row &right)
{
	if (left.size() != right.size())
		return false;
	for (unsigned int i = 0; i < left.size(); i++) {
		if (left[i] != right[i])
			return false;
	}
	return true;
}

RecordResult::RecordResult() : m_columnCount(0)
{
}

/* forgets the rows and the statement and stops listening; the column names stay */
__attribute__((noinline)) void RecordResult::Clear()
{
	m_changed = false;
	m_columnCount = 0;
	m_rowCount = 0;
	if (m_statement.Get() != NULL) {
		m_statement->Finalize();
		m_statement.Clear();
	}
	if (m_connection != NULL)
		m_connection->RemoveListener(this);
}

RecordResult::~RecordResult()
{
	Clear();
}

int RecordResult::GetRow(int index, Row *row)
{
	row->clear();
	if (index < 0 || index >= m_rowCount)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	*row = m_rows[index];
	return 0;
}

/* each column's "database.table", or the table its rowid is found in without one */
int RecordResult::OpenRecord(const Row &rowids, const paf::string &columns,
			     RefPtr<Record> *record)
{
	paf::vector<paf::string> names;

	for (unsigned int i = 0; i < m_databases.size(); i++) {
		if (m_databases[i].size() != 0 && m_tables[i].size() != 0) {
			names.push_back(m_databases[i] + "." + m_tables[i]);
		} else {
			paf::string name;

			if (m_connection->FindTable(rowids[i], &name))
				names.push_back(name);
		}
	}
	if (m_connection != NULL)
		return m_connection->OpenRecord(names, rowids, columns, record);
	return DBUTIL_ERROR_INVALID_STATE;
}

int RecordResult::GetRecord(int index, const paf::string &columns, RefPtr<Record> *record)
{
	Row rowids;
	int ret;

	ret = GetRow(index, &rowids);
	if (ret != 0)
		return ret;
	return OpenRecord(rowids, columns, record);
}

/* steps the statement to its end, a row of the rowid columns at each step */
int RecordResult::LoadRows(RefPtr<Statement> statement, int *count, RowList *rows)
{
	int ret;

	do {
		ret = statement->Step(1);
		switch (ret) {
		case 1:
			return 0;
		case 0: {
			Row row;

			for (int i = 0; i < m_columnCount; i++) {
				Value value;

				ret = statement->GetValue(m_columns[i], &value);
				row.push_back(value.data.integer);
			}
			rows->push_back(row);
			(*count)++;
			break;
		}
		}
	} while (ret == 0);
	return ret;
}

/*
 * Reads the statement again and walks the new rows against the old ones, both in the
 * statement's order: a new row not among the old ones from the current one on was added, the
 * old rows passed over to reach a new row were removed, and so were the old rows left at the
 * end. The new rows replace the old unless the callback stops the walk.
 */
int RecordResult::Refresh(RowCallback callback, void *arg)
{
	int count = 0;
	RowList rows;
	int ret;
	int result;
	int old;

	ret = LoadRows(m_statement, &count, &rows);
	m_statement->Reset();
	if (ret != 0)
		return ret;

	old = 0;
	result = 0;
	for (int i = 0; i < count; i++) {
		if (SameRow(rows[i], m_rows[old])) {
			old++;
			continue;
		}

		int found;

		for (found = old; found < m_rowCount; found++) {
			if (SameRow(rows[i], m_rows[found]))
				goto removed;
		}
		result = callback(0, &rows[i], i, 0, arg);
		if (result != 0)
			goto done;
		continue;
removed:
		for (; old < found; old++) {
			result = callback(1, &m_rows[old], old, 0, arg);
			if (result != 0)
				goto done;
		}
		old++;
	}
	for (; old < m_rowCount; old++) {
		result = callback(1, &m_rows[old], old, 0, arg);
		if (result != 0)
			break;
	}
done:
	if (result == 0) {
		m_rows = rows;
		m_rowCount = count;
	}
	return ret;
}

/* takes every column as a rowid column, with its database and table, then reads the rows */
int RecordResult::Load(Connection *connection, RefPtr<Statement> statement)
{
	int ret;

	if (statement.Get() == NULL)
		return 0;
	Clear();
	m_connection = connection;
	m_statement = statement;

	int columnCount = m_statement->m_columnCount;

	for (int i = 0; i < columnCount; i++) {
		const char *database = m_statement->GetColumnDatabaseName(i);
		const char *table = m_statement->GetColumnTableName(i);

		m_databases.push_back(paf::string(database));
		m_tables.push_back(paf::string(table));
		m_columns.push_back(i);
		m_columnCount++;
	}
	ret = LoadRows(statement, &m_rowCount, &m_rows);
	if (ret != 0) {
		m_statement->Finalize();
		m_statement.Clear();
	} else {
		m_statement->Reset();
	}
	return ret;
}

}
