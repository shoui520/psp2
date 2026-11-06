/*
 * The collations and SQL functions over dbutil's collators.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sqlite3.h>
#include <std/string>
#include "connection.h"
#include "function.h"
#include "collator.h"
#include "collation.h"

namespace dbutil {

/* SQLite's collation callback: the collator is the argument */
static int CompareEN(void *arg, int leftLength, const void *left, int rightLength,
		     const void *right)
{
	return ((Collator *)arg)->Compare((const char *)left, leftLength, (const char *)right,
					  rightLength);
}

CollationEN::CollationEN()
{
	m_name = "dbutil_CollationEN";
	m_compare = CompareEN;
	m_collator = new CollatorEnglish;
	m_arg = m_collator;
}

CollationEN::~CollationEN()
{
	if (m_collator != NULL)
		delete m_collator;
}

static int CompareJP(void *arg, int leftLength, const void *left, int rightLength,
		     const void *right)
{
	return ((Collator *)arg)->Compare((const char *)left, leftLength, (const char *)right,
					  rightLength);
}

CollationJP::CollationJP()
{
	m_name = "dbutil_CollationJP";
	m_compare = CompareJP;
	m_collator = new CollatorJapanese;
	m_arg = m_collator;
}

CollationJP::~CollationJP()
{
	if (m_collator != NULL)
		delete m_collator;
}

/* a text of four bytes that starts with a NUL is taken as empty */
static int CompareLanguage(void *arg, int leftLength, const void *left, int rightLength,
			   const void *right)
{
	if (leftLength == 4 && *(const SceUInt8 *)left == 0)
		leftLength = 0;
	if (rightLength == 4 && *(const SceUInt8 *)right == 0)
		rightLength = 0;
	return ((Collator *)arg)->Compare((const char *)left, leftLength, (const char *)right,
					  rightLength);
}

LanguageCollation::LanguageCollation(int language, const paf::string &name)
	: m_language(language)
{
	m_compare = CompareLanguage;
	m_name = name;
	m_arg = m_collator = CreateCollator(language);
}

void LanguageCollation::SetAttribute(int attribute, bool value)
{
	if (m_collator != NULL)
		m_collator->SetAttribute(attribute, value);
}

LanguageCollation::~LanguageCollation()
{
	DestroyCollator(m_collator);
	m_collator = NULL;
}

/* SQLite's function callback: the collator is the user data */
static void GetIndex(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	Collator *collator = (Collator *)sqlite3_user_data(context);
	const char *text = (const char *)sqlite3_value_blob(argv[0]);
	int length = sqlite3_value_bytes(argv[0]);

	sqlite3_result_int(context, collator->GetIndex(text, length));
}

IndexFunction::IndexFunction(int language, const paf::string &name) : m_language(language)
{
	m_argCount = 1;
	m_name = name;
	m_func = GetIndex;
	m_arg = m_collator = CreateCollator(m_language);
}

IndexFunction::~IndexFunction()
{
	DestroyCollator(m_collator);
	m_collator = NULL;
}

void IndexFunction::SetAttribute(int attribute, bool value)
{
	if (m_collator != NULL)
		m_collator->SetAttribute(attribute, value);
}

/* the text's sort key as a blob from sqlite3_malloc, which SQLite frees */
static void GetSortKey(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	Collator *collator = (Collator *)sqlite3_user_data(context);
	const char *text = (const char *)sqlite3_value_text(argv[0]);
	int length = sqlite3_value_bytes(argv[0]);
	int keyLength = 0;
	static const char error[] = "failed to get sortkey.";

	collator->GetSortKey(text, length, NULL, 0, &keyLength);

	SceUInt8 *key = (SceUInt8 *)sqlite3_malloc(keyLength);

	if (key == NULL) {
		sqlite3_result_error_nomem(context);
		return;
	}
	if (collator->GetSortKey(text, length, key, keyLength, &keyLength) != 0) {
		sqlite3_result_error(context, error, -1);
		return;
	}
	sqlite3_result_blob(context, key, keyLength, sqlite3_free);
}

SortKeyFunction::SortKeyFunction(int language, const paf::string &name) : m_language(language)
{
	m_argCount = 1;
	m_func = GetSortKey;
	m_name = name;
	m_arg = m_collator = CreateCollator(m_language);
}

SortKeyFunction::~SortKeyFunction()
{
	DestroyCollator(m_collator);
	m_collator = NULL;
}

void SortKeyFunction::SetAttribute(int attribute, bool value)
{
	if (m_collator != NULL)
		m_collator->SetAttribute(attribute, value);
}

static void GetSortKeyEN(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	Collator *collator = (Collator *)sqlite3_user_data(context);
	const char *text = (const char *)sqlite3_value_text(argv[0]);
	int length = sqlite3_value_bytes(argv[0]);
	int keyLength = 0;
	static const char error[] = "failed to get sortkey.";

	collator->GetSortKey(text, length, NULL, 0, &keyLength);

	SceUInt8 *key = (SceUInt8 *)sqlite3_malloc(keyLength);

	if (key == NULL) {
		sqlite3_result_error_nomem(context);
		return;
	}
	if (collator->GetSortKey(text, length, key, keyLength, &keyLength) != 0) {
		sqlite3_result_error(context, error, -1);
		return;
	}
	sqlite3_result_blob(context, key, keyLength, sqlite3_free);
}

SortKeyEN::SortKeyEN()
{
	m_name = "SortKeyEN";
	m_argCount = 1;
	m_func = GetSortKeyEN;
	m_collator = new CollatorEnglish;
	m_arg = m_collator;
}

SortKeyEN::~SortKeyEN()
{
	if (m_collator != NULL)
		delete m_collator;
}

static void GetSortKeyJP(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	Collator *collator = (Collator *)sqlite3_user_data(context);
	const char *text = (const char *)sqlite3_value_text(argv[0]);
	int length = sqlite3_value_bytes(argv[0]);
	int keyLength = 0;
	static const char error[] = "failed to get sortkey.";

	collator->GetSortKey(text, length, NULL, 0, &keyLength);

	SceUInt8 *key = (SceUInt8 *)sqlite3_malloc(keyLength);

	if (key == NULL) {
		sqlite3_result_error_nomem(context);
		return;
	}
	if (collator->GetSortKey(text, length, key, keyLength, &keyLength) != 0) {
		sqlite3_result_error(context, error, -1);
		return;
	}
	sqlite3_result_blob(context, key, keyLength, sqlite3_free);
}

SortKeyJP::SortKeyJP()
{
	m_name = "SortKeyJP";
	m_argCount = 1;
	m_func = GetSortKeyJP;
	m_collator = new CollatorJapanese;
	m_arg = m_collator;
}

SortKeyJP::~SortKeyJP()
{
	if (m_collator != NULL)
		delete m_collator;
}

}
