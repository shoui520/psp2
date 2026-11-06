/*
 * dbutil::RecordResult: a result whose columns are rowids, each row the rowids of one record
 * (one row of each column's table); it opens the record of a row and tells its caller which
 * rows a fresh read of the statement adds and removes. Sony's names: none; the class and
 * member names are ours.
 */
#ifndef DBUTIL_RECORD_RESULT_H
#define DBUTIL_RECORD_RESULT_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include "connection.h"
#include "statement.h"
#include "result.h"

namespace dbutil {

class RecordResult : public Result {
public:
	typedef paf::vector<SceInt64> Row;
	typedef paf::vector<Row> RowList;

	/* told of each row Refresh finds added (removed 0) or removed (removed 1) */
	typedef int (*RowCallback)(int removed, const Row *row, int index, int reserved,
				   void *arg);

	RecordResult();
	virtual ~RecordResult();

	void Clear();
	int GetRow(int index, Row *row);
	int OpenRecord(const Row &rowids, const paf::string &columns, RefPtr<Record> *record);
	int GetRecord(int index, const paf::string &columns, RefPtr<Record> *record);
	int LoadRows(RefPtr<Statement> statement, int *count, RowList *rows);
	int Refresh(RowCallback callback, void *arg);
	virtual int Load(Connection *connection, RefPtr<Statement> statement);

	paf::vector<paf::string> m_databases;	/* each column's */
	paf::vector<paf::string> m_tables;	/* each column's */
	RowList m_rows;
	paf::vector<int> m_columns;		/* the rowid columns */
	int m_columnCount;
};

}

#endif
