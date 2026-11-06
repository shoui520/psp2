/*
 * dbutil's values outside a statement: a Value that owns copies of its name, text and blob
 * (OwnedValue), a column's "database.table.column" name in its parts (ColumnName), and a
 * counted column value whose names are shared strings of a set (ColumnValue). Exported by NID
 * only: the class and member names are ours.
 */
#ifndef DBUTIL_VALUE_H
#define DBUTIL_VALUE_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/memory>
#include <common/shared_string.h>
#include "statement.h"

/* libpaf's heaps (ScePafMisc) */
extern "C" {
void *scePafMallocWithContext(void *context, SceSize size);
void scePafFreeWithContext(void *context, void *ptr);
}

namespace dbutil {

/* from the given heap, or paf's without one */
static inline void *Allocate(void *heap, SceSize size)
{
	if (heap != NULL)
		return scePafMallocWithContext(heap, size);
	return sce_paf_malloc(size);
}

static inline void Free(void *heap, void *ptr)
{
	if (heap != NULL)
		scePafFreeWithContext(heap, ptr);
	else
		sce_paf_free(ptr);
}

/* a Value whose name, text and blob are its own copies, on its heap */
class OwnedValue : public Value {
public:
	OwnedValue();
	OwnedValue(const OwnedValue &other);
	OwnedValue(const Value &value, void *heap);
	~OwnedValue();

	void Assign(const Value &value, void *heap);
	OwnedValue &operator=(const OwnedValue &other);

	void *m_heap;		/* NULL for paf's heap */
};

/* a column's name: "column", "table.column" or "database.table.column" */
class ColumnName {
public:
	ColumnName(const paf::string &name);
	ColumnName(const paf::string &database, const paf::string &table,
		   const paf::string &column);

	void Parse(const paf::string &name);
	bool IsEmpty();

	paf::string m_database;
	paf::string m_table;
	paf::string m_column;
};

/* a column's value with its names shared from a set; counts its references */
class ColumnValue {
public:
	typedef paf::intrusive_ptr<paf::common::SharedStringBody> Name;

	ColumnValue();
	ColumnValue(const Value &value, void *heap, paf::common::SharedStringSet *names,
		    const char *database, const char *table);
	~ColumnValue();

	int Release();
	int AddRef();
	bool operator==(const ColumnValue &other);
	bool operator!=(const ColumnValue &other);
	void GetColumnName(ColumnName *name);
	paf::string ToString();

	void *m_heap;		/* the text's and the blob's; NULL for paf's heap */
	Name m_database;
	Name m_table;
	Name m_name;
	bool m_marked : 1;
	int m_type;
	int m_size;
	union {
		SceInt64 integer;
		double real;
		char *text;
		void *blob;
	} m_data;
	int m_refCount;
};

}

#endif
