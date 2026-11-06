/*
 * dbutil::RowidResult: a result of the rowids of one column, with the database and table they
 * are rowids of; it opens the record of a row, reshuffles the rows, and tells its caller which
 * rows a fresh read of the statement adds and removes. Sony's names: none; the class and member
 * names are ours.
 */
#ifndef DBUTIL_ROWID_RESULT_H
#define DBUTIL_ROWID_RESULT_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include "connection.h"
#include "statement.h"
#include "record.h"
#include "result.h"
#include "value.h"

namespace dbutil {

class RowidResult : public Result {
public:
	typedef Allocator<SceInt64> RowidAllocator;
	typedef paf::vector<SceInt64, RowidAllocator> RowidList;

	/* told of each rowid Refresh finds added (removed 0) or removed (removed 1) */
	typedef int (*RowCallback)(int removed, SceInt64 rowid, int index, int reserved,
				   void *arg);

	RowidResult();
	RowidResult(const RowidAllocator &alloc);

	void Clear();
	virtual ~RowidResult();

	SceInt64 GetRowid(int index);
	int OpenRecord(int index, const paf::string &columns, RefPtr<Record> *record);
	int OpenRecord(SceInt64 rowid, const paf::string &columns, RefPtr<Record> *record);
	int LoadRows(RefPtr<Statement> statement, int column, int *count, RowidList *rows);
	int Refresh(RowCallback callback, void *arg);
	int Prepare(const paf::string &sql, int count, const Value *values);
	int Shuffle();
	virtual int Load(Connection *connection, RefPtr<Statement> statement);

	int m_column;			/* the rowid column; -1 for none */
	RowidList m_rowids;
	paf::string m_database;		/* the rowids' */
	paf::string m_table;
};

}

#endif
