/*
 * dbutil::Function: registering an SQL function on a connection and taking it away again.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sqlite3.h>
#include "connection.h"
#include "function.h"

namespace dbutil {

const char *Function::GetName()
{
	return m_name.c_str();
}

int Function::Register(Connection *connection)
{
	int ret;

	if (connection == NULL || m_name.size() == 0)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (connection->m_db == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	ret = sqlite3_create_function(connection->m_db, m_name.c_str(), m_argCount, m_textRep,
				      m_arg, m_func, m_step, m_final);
	if (ret != 0)
		ret |= DBUTIL_ERROR_SQLITE;
	return ret;
}

/* the same name and arity with no callbacks */
int Function::Unregister(Connection *connection)
{
	int ret;

	if (connection == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	if (connection->m_db == NULL)
		return DBUTIL_ERROR_INVALID_STATE;
	ret = sqlite3_create_function(connection->m_db, m_name.c_str(), m_argCount, m_textRep,
				      m_arg, NULL, NULL, NULL);
	if (ret != 0)
		ret |= DBUTIL_ERROR_SQLITE;
	return ret;
}

}
