/*
 * Binding a statement's parameters and reading the columns of the row it stepped to: the
 * helpers every query of the databases uses (appdb.cpp, newevent_accessor.cpp, ...).
 */
#ifndef LSDB_STATEMENT_H
#define LSDB_STATEMENT_H

#include <string.h>
#include <std/string.h>
#include <dbutil/statement.h>

/* the value to bind: SQLite's integer, text, blob or NULL */
static inline int AppDbBindInt(dbutil::RefPtr<dbutil::Statement> *statement, int index,
	SceInt64 integer)
{
	dbutil::Value value;

	memset(&value, 0, sizeof(value));
	value.type = SQLITE_INTEGER;
	value.data.integer = integer;
	return (*statement)->Bind(index, &value, false);
}

static inline int AppDbBindNull(dbutil::RefPtr<dbutil::Statement> *statement, int index)
{
	dbutil::Value value;

	memset(&value, 0, sizeof(value));
	value.type = SQLITE_NULL;
	return (*statement)->Bind(index, &value, false);
}

/* text of size bytes (to its NUL with a negative size); NULL for none */
static inline int AppDbBindText(dbutil::RefPtr<dbutil::Statement> *statement, int index,
	const char *text, int size)
{
	if (text != NULL && *text != '\0' && size != 0) {
		dbutil::Value value;

		memset(&value, 0, sizeof(value));
		value.type = SQLITE_TEXT;
		if (size < 0)
			size = sce_paf_strlen(text);
		value.size = size;
		value.data.text = text;
		return (*statement)->Bind(index, &value, false);
	}
	return AppDbBindNull(statement, index);
}

static inline int AppDbBindString(dbutil::RefPtr<dbutil::Statement> *statement, int index,
	const paf::string *text)
{
	return AppDbBindText(statement, index, text->c_str(), text->size());
}

/* a column of the row a statement stepped to: its integer (0 for another type), its text */
static inline SceInt64 AppDbColumnInt(dbutil::RefPtr<dbutil::Statement> *statement, int column)
{
	dbutil::Value value;

	memset(&value, 0, sizeof(value));
	(*statement)->GetValue(column, &value);
	return value.type == SQLITE_INTEGER ? value.data.integer : 0;
}

static inline paf::string AppDbColumnString(dbutil::RefPtr<dbutil::Statement> *statement,
	int column)
{
	dbutil::Value value;

	memset(&value, 0, sizeof(value));
	(*statement)->GetValue(column, &value);
	paf::string text;

	if (value.type == SQLITE_TEXT)
		text.assign(value.data.text, value.size);
	return text;
}

#endif
