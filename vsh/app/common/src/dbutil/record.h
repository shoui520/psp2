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
#include "statement.h"

namespace dbutil {

class Record : public EventListener {
public:
	/* a row the record reads: its table and rowid */
	struct Row {
		SceInt64 rowid;
		paf::string table;
	};

	Record();
	Record(Connection *connection, const paf::string &table, const paf::string &columns,
	       SceInt64 rowid, void *heap);
	Record(Connection *connection, const paf::vector<paf::string> &tables,
	       const paf::string &columns, const paf::vector<SceInt64> &rowids, void *heap);
	virtual ~Record();

	int ReadValues(RefPtr<Statement> &statement);
	int FreeValues();
	int AddRef();
	int Release();
	const char *GetTableName(int index);
	int GetValue(int column, Value *value);
	int GetValue(const paf::string &name, Value *value);
	bool IsChanged();
	int Update();
	virtual void OnEvent(Event *event);

	Connection *m_connection;
	RefPtr<Statement> m_statement;
	int m_columnCount;
	Value *m_values;		/* the row's, one per column */
	paf::vector<Row> m_rows;
	bool m_valid;			/* the row was read */
	SceUID m_changeProcess;		/* the uncommitted change to a row: its event's */
	Connection *m_changeSource;	/* process and source connection */
	bool m_changed;			/* to be read again */
	void *m_heap;			/* the values' heap context; NULL for paf's heap */
	int m_refCount;
	paf::thread::RMutex m_lock;
	paf::thread::RMutex m_flagLock;
};

}

#endif
