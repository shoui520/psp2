/*
 * dbutil::Statement: a prepared SQL statement of a connection, counted by its references, and
 * dbutil::Value, a value bound to it or read from a row. Sony's names: the object name
 * "SceDbUtilStatement"; the class and member names are ours.
 */
#ifndef DBUTIL_STATEMENT_H
#define DBUTIL_STATEMENT_H

#include <scetypes.h>
#include <sqlite3.h>
#include <thread/thread.h>
#include "connection.h"

namespace dbutil {

/* a column's or a parameter's value; type is SQLite's (SQLITE_INTEGER ... SQLITE_NULL) */
struct Value {
	int type;
	const char *name;		/* the column's name */
	int size;			/* the text's or the blob's bytes */
	union {
		SceInt64 integer;
		double real;
		const char *text;
		const void *blob;
	} data;
};

class Statement : public EventListener {
public:
	Statement(Connection *connection, const paf::string &sql);
	virtual ~Statement();

	void AddRef();
	void Release();
	int Step(int count);
	int Reset();
	int Finalize();
	int BindAll(const Value *values, int count);

	Connection *m_connection;
	sqlite3 *m_db;
	sqlite3_stmt *m_stmt;
	SceInt64 m_row;			/* the row the last Step reached; -1 before the first */
	int m_columnCount;
	int m_result;			/* the constructor's: nonzero when preparing failed */
	int m_refCount;
	paf::thread::RMutex m_lock;
	bool m_changed;
	bool m_active;			/* counted in the connection's m_activeStatements */
};

}

#endif
