/*
 * dbutil::Result: the base of every result a statement's rows are loaded into.
 */
#include <stddef.h>
#include <scetypes.h>
#include "connection.h"
#include "statement.h"
#include "result.h"

namespace dbutil {

Result::Result()
	: m_connection(NULL), m_lock("SceDbUtilResult", 0), m_changed(false), m_rowCount(0)
{
}

void Result::OnCommit(Event *event)
{
	m_lock.Lock();
	m_changed = true;
	m_lock.Unlock();
}

/* events may have been lost: the rows may have changed */
void Result::OnOverflow(Event *event)
{
	m_lock.Lock();
	m_changed = true;
	m_lock.Unlock();
}

/* the connection closed: the statement goes with it */
void Result::OnClose(Event *event)
{
	m_lock.Lock();
	if (m_statement.Get() != NULL) {
		m_statement->Finalize();
		m_statement.Clear();
	}
	m_connection = NULL;
	m_lock.Unlock();
}

}
