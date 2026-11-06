/*
 * dbutil::Result: what Connection::Execute loads a statement's rows into; each kind of result
 * is a subclass with its own Load. Sony's names: the object name "SceDbUtilResult"; the class
 * and member names are ours.
 */
#ifndef DBUTIL_RESULT_H
#define DBUTIL_RESULT_H

#include "connection.h"

namespace dbutil {

class Result : public EventListener {
public:
	virtual int Load(Connection *connection, RefPtr<Statement> statement) = 0;
};

}

#endif
