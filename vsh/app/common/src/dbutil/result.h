/*
 * dbutil::Result: what Connection::Execute loads a statement's rows into; each kind of result
 * is a subclass with its own Load. It listens to its connection: a commit or an overflow marks
 * it changed, a close drops its statement. Sony's names: the object name "SceDbUtilResult"; the
 * class and member names are ours.
 */
#ifndef DBUTIL_RESULT_H
#define DBUTIL_RESULT_H

#include <scetypes.h>
#include <thread/thread.h>
#include "connection.h"
#include "statement.h"

namespace dbutil {

class Result : public EventListener {
public:
	Result();
	virtual ~Result()
	{
	}

	virtual void OnCommit(Event *event);
	virtual void OnOverflow(Event *event);
	virtual void OnClose(Event *event);
	virtual int Load(Connection *connection, RefPtr<Statement> statement) = 0;

	Connection *m_connection;
	RefPtr<Statement> m_statement;
	paf::thread::RMutex m_lock;
	bool m_changed;			/* the rows may differ from the database's */
	int m_rowCount;
};

}

#endif
