/*
 * ScebXCe's name tables: a table of the bX form grows by at least its initial size, and an
 * element or attribute name is interned once, as a length byte, the name and a NUL, its offset
 * found again through a hash table of 211 buckets.
 */
#include "bxce_internal.h"

/* grows the table to hold needed more bytes; NULL when there is no table */
char *bxceGrowTable(char *table, SceUInt32 needed, SceUInt32 *capacity)
{
	SceUInt32 extra;

	if (table == NULL)
		return NULL;

	extra = *capacity + BXCE_TABLE_INITIAL_SIZE;
	if (extra <= needed)
		extra = needed * 2;

	*capacity += extra;
	return (char *)bxceHeapExtend(g_bxceHeap, table, extra);
}

void *bxceNameHashInitialize(BxceNameHash *hash)
{
	return bxceMemset(hash, 0, sizeof(*hash));
}

/* the offset of the name in the table, adding it when it is new; -1 when out of memory */
int bxceNameHashIntern(BxceNameHash *hash, const char *name, int length, char *table,
		       SceUInt32 *size)
{
	BxceNameHashEntry *entry;
	int key = 0;
	int i;

	for (i = 0; i < length; i++)
		key = (name[i] + (key << 8)) % BXCE_NAME_HASH_SIZE;

	for (entry = hash->bucket[key]; entry != NULL; entry = entry->next) {
		if (bxceStrncmp(name, table + entry->offset, length) == 0)
			break;
	}
	if (entry == NULL) {
		entry = (BxceNameHashEntry *)ScebXCe_B8DDE741(sizeof(*entry));
		if (entry == NULL)
			return -1;

		entry->offset = *size + 1;
		bxceMemcpy(table + *size + 1, name, length);
		table[*size] = length;
		*size += length;
		table[++*size] = 0;
		*size += 1;

		entry->next = hash->bucket[key];
		hash->bucket[key] = entry;
		return entry->offset;
	}

	return entry->offset;
}

/* frees every entry; the buckets keep their stale pointers */
void bxceNameHashClear(BxceNameHash *hash)
{
	BxceNameHashEntry *entry, *freed;
	int i;

	for (i = 0; i < BXCE_NAME_HASH_SIZE; i++) {
		entry = hash->bucket[i];
		if (entry == NULL)
			continue;
		while (entry != NULL) {
			freed = entry;
			entry = freed->next;
			if (freed != NULL)
				ScebXCe_EC560647(freed);
		}
	}
}
