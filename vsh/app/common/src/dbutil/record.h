/*
 * dbutil::Record: one row (or a join of rows) of a connection's tables, read into Values and
 * kept current by the connection's events. Sony's names: the object names "SceDbUtilRecord" and
 * "SceDbUtilRecordFlag"; the class and member names are ours.
 */
#ifndef DBUTIL_RECORD_H
#define DBUTIL_RECORD_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <thread/thread.h>
#include "connection.h"

namespace dbutil {

class Record : public EventListener {
public:
	/* a row the record reads: its table and rowid */
	struct Row {
		SceInt64 rowid;
		paf::string table;
	};

	Record(Connection *connection, const paf::string &table, const paf::string &columns,
	       SceInt64 rowid, void *heap);
	Record(Connection *connection, const paf::vector<paf::string> &tables,
	       const paf::string &columns, const paf::vector<SceInt64> &rowids, void *heap);
	virtual ~Record();

	void AddRef();
	void Release();

	Connection *m_connection;
	RefPtr<Statement> m_statement;
	int m_columnCount;
	Value *m_values;
	paf::vector<Row> m_rows;
	bool m_valid;			/* the row was read */
	SceUInt32 m_changeTag;		/* the uncommitted change to the row: its event's */
	Connection *m_changeSource;	/* reserved word and source */
	bool m_changed;			/* to be read again */
	void *m_heap;			/* the values' heap context; NULL for paf's heap */
	int reserved;
	paf::thread::RMutex m_lock;
	paf::thread::RMutex m_flagLock;
};

}

#endif
