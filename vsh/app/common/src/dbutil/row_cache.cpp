/*
 * dbutil::RowCache: a query's rows read a page at a time, and the lists that hold them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sqlite3.h>
#include <sce_atomic.h>
#include <std/string>
#include <std/vector>
#include <std/algorithm>
#include <std/stdio.h>
#include <common/shared_ptr.h>
#include <common/shared_string.h>
#include "connection.h"
#include "statement.h"
#include "value.h"
#include "row_cache.h"

using paf::common::SharedPtr;

namespace dbutil {

ColumnValueList::ColumnValueList(const ValueAllocator &alloc) : vector(alloc)
{
}

/* a value of the column name names: the name, then the table and the database when given */
struct ColumnNameMatch {
	ColumnNameMatch(const ColumnName &name) : m_name(name)
	{
	}

	bool operator()(const RefPtr<ColumnValue> &value) const
	{
		const ColumnValue::Name &column = value->m_name;

		if (column.get() == NULL || column->ptr[0] != m_name.m_column.ptr[0] ||
		    sce_paf_strcmp(column->ptr, m_name.m_column.ptr) != 0)
			return false;
		if (m_name.m_table.size() != 0) {
			const ColumnValue::Name &table = value->m_table;

			if (table.get() == NULL || table->ptr[0] != m_name.m_table.ptr[0] ||
			    sce_paf_strcmp(table->ptr, m_name.m_table.ptr) != 0)
				return false;
		}
		if (m_name.m_database.size() == 0)
			return true;

		const ColumnValue::Name &database = value->m_database;

		return database.get() != NULL && database->ptr[0] == m_name.m_database.ptr[0] &&
		       sce_paf_strcmp(database->ptr, m_name.m_database.ptr) == 0;
	}

	const ColumnName &m_name;
};

RefPtr<ColumnValue> ColumnValueList::Find(const ColumnName &name)
{
	iterator found = paf::find_if(begin(), end(), ColumnNameMatch(name));

	if (found == end())
		return RefPtr<ColumnValue>();
	return *found;
}

/* the first value of a primary key column */
RefPtr<ColumnValue> ColumnValueList::GetPrimaryKey()
{
	iterator it;

	for (it = begin(); it != end(); ++it) {
		if ((*it)->m_marked)
			break;
	}
	if (it == end())
		return RefPtr<ColumnValue>();
	return *it;
}

CachedRowList::CachedRowList(const RowAllocator &alloc) : vector(alloc)
{
}

void RowCache::ClearChanged()
{
	sceAtomicExchange32AcqRel(&m_changed, 0);
}

/* drops the rows and the count */
void RowCache::Reset()
{
	m_rowCount = -1;
	m_rows = SharedPtr<CachedRowList>(
		new CachedRowList(CachedRowList::RowAllocator(m_param.heap)));
}

void RowCache::Set(Connection *connection, const RowCacheParam &param)
{
	if (m_connection != NULL)
		m_connection->RemoveListener(this);
	Reset();
	ClearChanged();
	m_names.clear();
	m_connection = connection;
	m_param = param;
	if (m_connection != NULL)
		m_connection->AddListener(this);
}

RowCache::RowCache(Connection *connection, const RowCacheParam &param) : m_connection(NULL)
{
	Set(connection, param);
}

RowCache::~RowCache()
{
	Set(NULL, RowCacheParam());
}

/* the statement's current row as the row at index */
SharedPtr<CachedRow> RowCache::ReadRow(int index, RefPtr<Statement> &statement)
{
	SharedPtr<ColumnValueList> values(
		new ColumnValueList(ColumnValueList::ValueAllocator(m_param.heap)));
	int count = statement->m_columnCount;

	for (int i = 0; i < count; i++) {
		Value value;

		if (statement->GetValue(i, &value) != 0)
			continue;

		const char *database = statement->GetColumnDatabaseName(i);
		const char *table = statement->GetColumnTableName(i);
		bool primaryKey = false;

		statement->GetColumnMetadata(i, NULL, NULL, NULL, &primaryKey, NULL);

		RefPtr<ColumnValue> columnValue(
			new ColumnValue(value, m_param.heap, &m_names, database, table));

		columnValue->m_marked = primaryKey;
		values->push_back(columnValue);
	}

	SharedPtr<CachedRow> row(new CachedRow);

	row->index = index;
	row->values = values;
	return row;
}

/* reads the page around index unless it holds index already */
void RowCache::Load(int index)
{
	CachedRowList *rows = m_rows.get();

	if (rows->size() != 0 && index >= (*rows)[0]->index &&
	    (*rows)[rows->size() - 1]->index >= index)
		return;
	rows->clear();
	if (m_connection == NULL || m_param.sql.size() == 0)
		return;

	int limit = m_param.pageSize;
	int start = 0;

	if (m_param.countSql.size() != 0) {
		Count();
		if (m_rowCount <= 0)
			return;
		if (limit < 0)
			limit = 1;
		if (limit < m_rowCount) {
			int first = index - limit / 2;
			int last = m_rowCount - limit;

			if (first < 0)
				first = 0;
			if (last > first)
				last = first;
			start = last;
		} else {
			limit = m_rowCount;
		}
	}

	RefPtr<Statement> statement;
	int offset = start;
	char clause[128];

	if (m_param.offset > 0)
		offset += m_param.offset;
	sce_paf_snprintf(clause, sizeof(clause), " LIMIT %d,%d", offset, limit);
	if (m_connection->Prepare(m_param.sql + clause, &statement) < 0)
		return;
	for (;;) {
		int ret = statement->Step(1);

		if (ret == 1 || ret < 0)
			break;
		m_rows->push_back(ReadRow(start, statement));
		start++;
	}
}

/* counts the rows once: the count query's sum, else the rows of one read */
void RowCache::Count()
{
	if (m_rowCount != -1)
		return;
	m_rowCount = 0;
	if (m_connection == NULL)
		return;
	if (m_param.countSql.size() == 0) {
		Load(0);
		m_rowCount = m_rows->size();
		return;
	}

	RefPtr<Statement> statement;

	if (m_connection->Prepare(m_param.countSql, &statement) < 0)
		return;
	for (;;) {
		int ret = statement->Step(1);

		if (ret == 1 || ret < 0)
			break;
		if (statement->m_columnCount <= 0)
			break;

		Value value;

		if (statement->GetValue(0, &value) < 0)
			break;
		if (value.type != SQLITE_INTEGER)
			break;
		m_rowCount += (int)value.data.integer;
	}
}

int RowCache::GetRowCount()
{
	Count();
	return m_rowCount;
}

/* the row at index, read with its page; none past the result */
SharedPtr<CachedRow> RowCache::GetRow(int index)
{
	Load(index);

	CachedRowList *rows = m_rows.get();

	if (rows->size() != 0 && index >= (*rows)[0]->index &&
	    (*rows)[rows->size() - 1]->index >= index)
		return rows->at(index - (*rows)[0]->index);
	return SharedPtr<CachedRow>();
}

bool RowCache::IsChanged()
{
	return m_changed != 0;
}

void RowCache::OnCommit(Event *event)
{
	sceAtomicExchange32AcqRel(&m_changed, 1);
}

void RowCache::OnOverflow(Event *event)
{
	sceAtomicExchange32AcqRel(&m_changed, 1);
}

}
