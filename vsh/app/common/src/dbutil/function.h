/*
 * dbutil::Function: an SQL function the caller registers on a connection
 * (sqlite3_create_function). Sony's names: none; the class and member names are ours.
 */
#ifndef DBUTIL_FUNCTION_H
#define DBUTIL_FUNCTION_H

#include <scetypes.h>
#include <sqlite3.h>
#include <std/string>
#include "connection.h"

namespace dbutil {

class Function {
public:
	virtual ~Function()
	{
	}

	const char *GetName();
	int Register(Connection *connection);
	int Unregister(Connection *connection);

	paf::string m_name;
	int m_argCount;			/* -1 for any */
	void (*m_func)(sqlite3_context *context, int argc, sqlite3_value **argv);
	void (*m_step)(sqlite3_context *context, int argc, sqlite3_value **argv);
	void (*m_final)(sqlite3_context *context);
	void *m_arg;
	int m_textRep;
};

}

#endif
