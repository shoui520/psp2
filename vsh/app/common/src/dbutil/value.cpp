/*
 * dbutil's owned values, column names and shared column values.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <std/stdio.h>
#include <std/list>
#include <std/memory>
#include <common/string_util.h>
#include <common/shared_string.h>
#include <sqlite3.h>
#include "statement.h"
#include "value.h"

namespace dbutil {

/* a copy of size bytes, NULL for none or when the heap has no room */
static inline void *Duplicate(void *heap, const void *src, SceSize size)
{
	void *copy = NULL;

	if (size != 0) {
		copy = Allocate(heap, size);
		if (copy != NULL)
			sce_paf_memcpy(copy, src, size);
	}
	return copy;
}

/* the set's string str, added to the set when it is not there yet */
static inline ColumnValue::Name Intern(paf::common::SharedStringSet *names, const char *str)
{
	paf::common::SharedStringSet::iterator it = names->find(str);

	if (it == names->end())
		return ScePafCommon_BD163724(names, str).first->m_body;
	return it->m_body;
}

/* the same shared string, or both without one */
static inline bool SameName(const ColumnValue::Name &left, const ColumnValue::Name &right)
{
	if (left.get() != NULL && right.get() != NULL)
		return *left == *right;
	return left.get() == NULL && right.get() == NULL;
}

ColumnValue::~ColumnValue()
{
	if (m_type == SQLITE_TEXT || m_type == SQLITE_BLOB)
		Free(m_heap, m_data.blob);
}

int ColumnValue::Release()
{
	int count = sceAtomicDecrement32AcqRel(&m_refCount) - 1;

	if (count == 0)
		delete this;
	return count;
}

int ColumnValue::AddRef()
{
	return sceAtomicIncrement32AcqRel(&m_refCount) + 1;
}

/* frees what the value owned, then copies value's name, text and blob onto heap */
void OwnedValue::Assign(const Value &value, void *heap)
{
	if ((type == SQLITE_TEXT || type == SQLITE_BLOB) && data.blob != NULL)
		Free(m_heap, (void *)data.blob);
	if (name != NULL)
		Free(m_heap, (void *)name);

	m_heap = heap;
	type = value.type;

	const char *source = value.name;

	size = value.size;
	name = source != NULL ?
		(const char *)Duplicate(heap, source, sce_paf_strlen(source) + 1) : NULL;

	switch (value.type) {
	case SQLITE_INTEGER:
		data.integer = value.data.integer;
		break;
	case SQLITE_FLOAT:
		data.real = value.data.real;
		break;
	case SQLITE_TEXT:
		data.text = value.data.text != NULL ?
			(const char *)Duplicate(m_heap, value.data.text, value.size + 1) : NULL;
		break;
	case SQLITE_BLOB:
		data.blob = Duplicate(m_heap, value.data.blob, value.size);
		break;
	}
}

OwnedValue::~OwnedValue()
{
	Value none;

	none.type = 0;
	none.name = NULL;
	Assign(none, NULL);
}

/* Sony's clears 48 bytes, more than the value's 32 */
OwnedValue::OwnedValue()
{
	sce_paf_memset(this, 0, 48);
}

OwnedValue::OwnedValue(const OwnedValue &other)
{
	sce_paf_memset(this, 0, sizeof(OwnedValue));
	Assign(other, other.m_heap);
}

OwnedValue::OwnedValue(const Value &value, void *heap)
{
	sce_paf_memset(this, 0, sizeof(OwnedValue));
	Assign(value, heap);
}

OwnedValue &OwnedValue::operator=(const OwnedValue &other)
{
	Assign(other, other.m_heap);
	return *this;
}

void ColumnName::Parse(const paf::string &name)
{
	m_database.clear();
	m_table.clear();
	m_column.clear();
	if (name.size() != 0) {
		paf::list<paf::string> parts;

		paf::common::string_util::tokenize(parts, name, 0, '.');

		paf::list<paf::string>::iterator it = parts.begin();

		if (parts.size() == 1) {
			m_column = *it;
		} else if (parts.size() == 2) {
			m_table = *it;
			++it;
			m_column = *it;
		} else if (parts.size() >= 3) {
			m_database = *it;
			++it;
			m_table = *it;
			++it;
			m_column = *it;
		}
	}
}

ColumnName::ColumnName(const paf::string &name)
{
	Parse(name);
}

ColumnName::ColumnName(const paf::string &database, const paf::string &table,
		       const paf::string &column)
	: m_database(database), m_table(table), m_column(column)
{
}

bool ColumnName::IsEmpty()
{
	return m_database.size() == 0 && m_table.size() == 0 && m_column.size() == 0;
}

ColumnValue::ColumnValue() : m_heap(NULL), m_type(0), m_refCount(0)
{
	m_marked = false;
}

/* a copy of value, its text and blob on heap, its names from the set */
ColumnValue::ColumnValue(const Value &value, void *heap, paf::common::SharedStringSet *names,
			 const char *database, const char *table)
	: m_heap(heap), m_type(value.type), m_size(value.size), m_refCount(0)
{
	m_marked = false;
	if (value.name != NULL)
		m_name = Intern(names, value.name);
	if (database != NULL)
		m_database = Intern(names, database);
	if (table != NULL)
		m_table = Intern(names, table);

	switch (value.type) {
	case SQLITE_INTEGER:
	case SQLITE_FLOAT:
		m_data.integer = value.data.integer;
		break;
	case SQLITE_TEXT: {
		char *text = (char *)Allocate(heap, value.size + 1);

		if (text != NULL) {
			sce_paf_memcpy(text, value.data.text, value.size);
			text[value.size] = '\0';
			m_data.text = text;
		} else {
			m_data.text = NULL;
		}
		break;
	}
	case SQLITE_BLOB: {
		void *blob = Allocate(heap, value.size);

		if (blob != NULL) {
			sce_paf_memcpy(blob, value.data.blob, value.size);
			m_data.blob = blob;
		} else {
			m_data.blob = NULL;
		}
		break;
	}
	}
}

bool ColumnValue::operator==(const ColumnValue &other)
{
	if (m_type == other.m_type && m_size == other.m_size && m_marked == other.m_marked &&
	    SameName(m_name, other.m_name) && SameName(m_table, other.m_table) &&
	    SameName(m_database, other.m_database)) {
		switch (m_type) {
		case SQLITE_INTEGER:
			return m_data.integer == other.m_data.integer;
		case SQLITE_FLOAT:
			return m_data.real == other.m_data.real;
		default:
			return sce_paf_memcmp(m_data.blob, other.m_data.blob, m_size) == 0;
		}
	}
	return false;
}

bool ColumnValue::operator!=(const ColumnValue &other)
{
	return !(*this == other);
}

/* the value's names into name; a part the value has no name for is left empty */
void ColumnValue::GetColumnName(ColumnName *name)
{
	name->Parse(paf::string());
	if (m_database.get() != NULL)
		name->m_database = *m_database;
	if (m_table.get() != NULL)
		name->m_table = *m_table;
	if (m_name.get() != NULL)
		name->m_column = *m_name;
}

paf::string ColumnValue::ToString()
{
	switch (m_type) {
	case SQLITE_INTEGER: {
		char text[64];

		sce_paf_snprintf(text, sizeof(text), "%lld", m_data.integer);
		return paf::string(text);
	}
	case SQLITE_FLOAT:
		return paf::common::ToString((float)m_data.real, false, true, true);
	case SQLITE_TEXT:
		if (m_data.text != NULL)
			return paf::string(m_data.text);
		break;
	case SQLITE_BLOB:
		return paf::common::string_util::ToString(m_data.blob, m_size);
	}
	return paf::string();
}

}
