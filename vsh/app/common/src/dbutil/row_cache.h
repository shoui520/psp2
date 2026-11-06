/*
 * dbutil::RowCache: the rows of a query read a page at a time ("<sql> LIMIT start,count")
 * around the row asked for, each row a list of counted column values whose names are shared
 * from the cache's string set. A count query, when given, sizes the result and centres the
 * page; without one the whole result is read as one page. It listens to its connection: a
 * commit or an overflow marks it changed. Exported by NID only: the class and member names
 * are ours.
 */
#ifndef DBUTIL_ROW_CACHE_H
#define DBUTIL_ROW_CACHE_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <common/shared_string.h>
#include "connection.h"
#include "statement.h"
#include "value.h"

namespace dbutil {

/* a row's column values */
class ColumnValueList
	: public paf::vector<RefPtr<ColumnValue>, Allocator<RefPtr<ColumnValue> > > {
public:
	typedef Allocator<RefPtr<ColumnValue> > ValueAllocator;

	ColumnValueList(const ValueAllocator &alloc);

	RefPtr<ColumnValue> Find(const ColumnName &name);
	RefPtr<ColumnValue> GetPrimaryKey();
};

/* a row of the result: its index in the result and its values */
struct CachedRow {
	int index;
	paf::common::SharedPtr<ColumnValueList> values;
};

/* the page of rows read last, in index order */
class CachedRowList
	: public paf::vector<paf::common::SharedPtr<CachedRow>,
			     Allocator<paf::common::SharedPtr<CachedRow> > > {
public:
	typedef Allocator<paf::common::SharedPtr<CachedRow> > RowAllocator;

	CachedRowList(const RowAllocator &alloc);
};

/* objects the caller has the cache keep; nothing here reads them */
struct RowCacheObject;

/* what a cache reads */
struct RowCacheParam {
	RowCacheParam() : offset(-1), pageSize(-1), heap(NULL)
	{
	}

	paf::string sql;		/* the query, without a LIMIT */
	int offset;			/* added to each page's start when positive */
	int pageSize;			/* rows a page holds; below 0 for one */
	paf::string countSql;		/* the query that counts the rows; empty for none */
	void *heap;			/* the values' and lists' heap; NULL for the default */
	paf::common::SharedPtr<RowCacheObject> owner;
	paf::common::SharedPtr<RowCacheObject> context;
};

class RowCache : public EventListener {
public:
	RowCache(Connection *connection, const RowCacheParam &param);
	virtual ~RowCache();

	void ClearChanged();
	void Reset();
	void Set(Connection *connection, const RowCacheParam &param);
	paf::common::SharedPtr<CachedRow> ReadRow(int index, RefPtr<Statement> &statement);
	void Load(int index);
	void Count();
	int GetRowCount();
	paf::common::SharedPtr<CachedRow> GetRow(int index);
	bool IsChanged();
	virtual void OnCommit(Event *event);
	virtual void OnOverflow(Event *event);

	virtual void OnRollback(Event *event)
	{
	}

	virtual void OnChange(int type, paf::string *database, paf::string *table, SceInt64 rowid)
	{
	}

	virtual void OnClose(Event *event)
	{
	}

	Connection *m_connection;
	RowCacheParam m_param;
	paf::common::SharedPtr<CachedRowList> m_rows;
	int m_rowCount;			/* -1 until counted */
	int32_t m_changed;		/* set by a commit or an overflow */
	paf::common::SharedStringSet m_names;	/* the values' database, table and column names */
};

}

#endif
