/*
 * The collations and SQL functions over dbutil's collators: dbutil_CollationEN and
 * dbutil_CollationJP, SortKeyEN and SortKeyJP, and the collation, index and sort key of a
 * system language under a name the caller gives. Sony's names: the SQL names; the class
 * names are ours.
 */
#ifndef DBUTIL_COLLATION_H
#define DBUTIL_COLLATION_H

#include <scetypes.h>
#include <std/string>
#include "connection.h"
#include "function.h"
#include "collator.h"

namespace dbutil {

/* dbutil_CollationEN: the English collator's order */
class CollationEN : public Collation {
public:
	CollationEN();
	virtual ~CollationEN();

	Collator *m_collator;
};

/* dbutil_CollationJP: the Japanese collator's order */
class CollationJP : public Collation {
public:
	CollationJP();
	virtual ~CollationJP();

	Collator *m_collator;
};

/* a system language's collator's order under the caller's name */
class LanguageCollation : public Collation {
public:
	LanguageCollation(int language, const paf::string &name);
	void SetAttribute(int attribute, bool value);
	virtual ~LanguageCollation();

	int m_language;
	Collator *m_collator;
};

/* name(text): the index (the letter group) a system language's collator gives the text */
class IndexFunction : public Function {
public:
	IndexFunction(int language, const paf::string &name);
	virtual ~IndexFunction();
	void SetAttribute(int attribute, bool value);

	int m_language;
	Collator *m_collator;
};

/* name(text): a system language's collator's sort key of the text, as a blob */
class SortKeyFunction : public Function {
public:
	SortKeyFunction(int language, const paf::string &name);
	virtual ~SortKeyFunction();
	void SetAttribute(int attribute, bool value);

	int m_language;
	Collator *m_collator;
};

/* SortKeyEN(text) */
class SortKeyEN : public Function {
public:
	SortKeyEN();
	virtual ~SortKeyEN();

	Collator *m_collator;
};

/* SortKeyJP(text) */
class SortKeyJP : public Function {
public:
	SortKeyJP();
	virtual ~SortKeyJP();

	Collator *m_collator;
};

}

#endif
