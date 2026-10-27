/*
 * files.db: the header sector, with the hmac over it, and the tree of the
 * mount's entries in the pages after it.
 */

#include <sysmem/sysclib.h>
#include "pfsmgr_internal.h"

#define PFS_DB_V1_BASE			0x160	/* name unknown: page 0 of types 1 and 2 */

#define SCE_PFS_ERROR_DB_HASH		0x80140902	/* name unknown */
#define SCE_PFS_ERROR_DB_EMPTY		0x80140904	/* name unknown */
#define SCE_PFS_ERROR_DB_PAGE_SIZE	0x80140909	/* name unknown */
#define SCE_PFS_ERROR_DB_BROKEN		0x8014090a	/* name unknown */

typedef union ScePfsDbSector {			/* name unknown */
	ScePfsDbHeader header;
	char bytes[PFS_DB_HEADER_SECTOR];
} ScePfsDbSector;

static inline SceOff pfsDbTableBase(ScePfsDbTable *table)	/* name unknown */
{
	return table->header.type - 1 < 2 ? PFS_DB_V1_BASE : table->header.pageSize;
}

/* the key of an entry as the tree keeps it */
static inline int pfsDbTableRecordKey(ScePfsDbRecordKey *rkey, ScePfsDbKey *key)	/* name unknown */
{
	char name[PFS_DB_NAME_MAX];
	const char *path2, *path3;
	SceSize len;
	int ret;

	ret = sceVfsOpDecodePathElem(key->mnt, key->name, &path2, &path3, name, sizeof(name),
				      &len);
	if (ret != 0)
		return ret;
	if (len >= sizeof(name))
		return SCE_PFS_ERROR_DB_NAME_TOO_LONG;
	memset(rkey, 0, sizeof(*rkey));
	rkey->id.parentIndex = key->parentIndex;
	pfsMemcpyChecked(rkey->id.name, name, len, sizeof(*rkey) - offsetof(ScePfsDbRecordKey, id.name));
	rkey->id.name[len] = '\0';
	return 0;
}

int pfsDbTableWriteHeader(ScePfsDbTable *table)
{
	char buf[sizeof(ScePfsDbSector) + PFS_DMA_ALIGN];
	ScePfsDbSector *sector = PFS_DMA_ALIGNED(buf);
	SceSize n;
	int ret;

	ret = pfsLowerPread(table->lf, sector, sizeof(*sector), 0, &n);
	if (ret != 0)
		return ret;
	if (n != sizeof(*sector))
		return SCE_PFS_ERROR_DB_BROKEN;
	memcpy(sector, &table->header, sizeof(table->header));
	return pfsLowerPwrite(table->lf, sector, sizeof(*sector), 0, &n);
}

int pfsDbTableWriteDirty(ScePfsDbTable *table)
{
	char buf[sizeof(ScePfsDbSector) + PFS_DMA_ALIGN];
	ScePfsDbSector *sector = PFS_DMA_ALIGNED(buf);
	SceSize n;
	int ret;

	ret = pfsLowerPread(table->lf, sector, sizeof(*sector), 0, &n);
	if (ret != 0)
		return ret;
	if (n != sizeof(*sector))
		return SCE_PFS_ERROR_DB_BROKEN;
	sector->header.dirty = table->dirty;
	return pfsLowerPwrite(table->lf, sector, sizeof(*sector), 0, &n);
}

int pfsDbTableReadInfo(ScePfsLowerFile *lf, SceUInt32 *type, SceUInt32 *keyParam,
		       SceUInt16 *a6)
{
	char buf[sizeof(ScePfsDbSector) + PFS_DMA_ALIGN];
	ScePfsDbSector *sector = PFS_DMA_ALIGNED(buf);
#if FW_VERSION < 0x04000000
	ScePfsDbHeader header;
#endif
	SceSize n;
	int ret;

	ret = pfsLowerPread(lf, sector, sizeof(*sector), 0, &n);
	if (ret != 0)
		return ret;
	if (n != sizeof(*sector))
		return SCE_PFS_ERROR_DB_BROKEN;
#if FW_VERSION < 0x04000000
	memcpy(&header, sector, sizeof(header));
	*type = header.type;
	*keyParam = header.keyParam;
	*a6 = header.a6;
	return 0;
#else
	*type = sector->header.type;
	*keyParam = sector->header.keyParam;
	*a6 = sector->header.a6;
	return 0;
#endif
}

void pfsDbTableInit(ScePfsDbTable *table, ScePfsLru *lru)
{
	table->lru = lru;
	table->dirty = 0;
}

void pfsDbTableTerm(ScePfsDbTable *table)
{
	pfsPageAllocatorTerm(&table->pages);
}

int pfsDbTableOpen(ScePfsDbTable *table, ScePfsLowerFile *lf, const ScePfsHash *key)
{
	char buf[sizeof(ScePfsDbSector) + PFS_DMA_ALIGN];
	ScePfsDbSector *sector = PFS_DMA_ALIGNED(buf);
	SceUInt8 signature[sizeof(table->header.signature)];
	ScePfsHash hmac;
	SceUInt64 size;
	SceSize n;
	int ret;

	table->lf = lf;
	ret = pfsLowerGetSize(lf, &size);
	if (ret != 0)
		return ret;
	if (size == 0)
		return SCE_PFS_ERROR_DB_EMPTY;
	ret = pfsLowerPread(table->lf, sector, sizeof(*sector), 0, &n);
	if (ret != 0)
		return ret;
	if (n != sizeof(*sector))
		return SCE_PFS_ERROR_DB_BROKEN;
	/* left dirty: a change was cut short */
	table->dirty = sector->header.dirty;
	if (table->dirty != 0)
		return SCE_PFS_ERROR_DB_BROKEN;
	ret = pfsLowerPread(table->lf, sector, sizeof(*sector), 0, &n);
	if (ret != 0)
		return ret;
	if (n != sizeof(*sector))
		return SCE_PFS_ERROR_DB_BROKEN;
	memcpy(&table->header, sector, sizeof(table->header));
	if (table->header.magic != PFS_DB_MAGIC)
		return SCE_PFS_ERROR_DB_BROKEN;
	table->key = *key;
	if (table->header.pageSize != table->lru->pageSize)
		return SCE_PFS_ERROR_DB_PAGE_SIZE;
	pfsPageAllocatorInit(&table->pages, table->lru, table->lf, pfsDbTableBase(table));
	memcpy(signature, table->header.signature, sizeof(signature));
	pfsHashCopy(&hmac, &table->header.hmac);
	memset(&table->header.hmac, 0, sizeof(table->header.hmac));
	memset(table->header.signature, 0, sizeof(table->header.signature));
	pfsHmacSha1Dma(&table->header.hmac, &table->key, &table->header, sizeof(table->header));
	if (!pfsHashEqual(&hmac, &table->header.hmac))
		return SCE_PFS_ERROR_DB_HASH;
	pfsPageAllocatorSet(&table->pages, table->header.freeHead, table->header.end);
	ret = pfsDbTreeInit(&table->tree, &table->key, &table->pages, table->header.root,
			    table->header.type);
	if (ret != 0)
		return ret;
	if (!pfsHashEqual(&table->header.rootHash, &table->tree.hash))
		return SCE_PFS_ERROR_DB_HASH;
	memcpy(table->header.signature, signature, sizeof(signature));
	table->header.hmac = hmac;
	return 0;
}

int pfsDbTableCreate(ScePfsDbTable *table, ScePfsLowerFile *lf, const ScePfsHash *key,
		     SceUInt16 fsType, SceUInt16 a6, SceUInt32 type, SceUInt32 keyParam)
{
	int ret;

	memset(&table->header, 0, sizeof(table->header));
	table->header.keyParam = keyParam;
	table->header.type = type;
	table->header.fsType = fsType;
	table->header.a6 = a6;
	table->header.magic = PFS_DB_MAGIC;
	table->header.pageSize = table->lru->pageSize;
	table->lf = lf;
	table->header.usage = 0;
	ret = pfsLowerSetSize(lf, table->header.pageSize);
	if (ret != 0)
		return ret;
	ret = pfsLowerZero(table->lf, table->header.pageSize);
	if (ret != 0)
		return ret;
	pfsPageAllocatorInit(&table->pages, table->lru, table->lf, pfsDbTableBase(table));
	table->key = *key;
	ret = pfsDbTreeInit(&table->tree, &table->key, &table->pages, -1, table->header.type);
	if (ret == 0)
		table->dirty = 1;
	return ret;
}

int pfsDbTableFlush(ScePfsDbTable *table)
{
	int ret = 0;

	if (table->dirty != 0 && (ret = pfsDbTableWriteDirty(table)) == 0 &&
	    (ret = pfsLruFlush(table->lru, table->lf, 0)) == 0) {
		pfsPageAllocatorGet(&table->pages, &table->header.freeHead, &table->header.end);
		table->header.order = table->tree.order;
		table->header.root = table->tree.root;
		pfsHashCopy(&table->header.rootHash, &table->tree.hash);
		memset(&table->header.hmac, 0, sizeof(table->header.hmac));
		pfsHmacSha1Dma(&table->header.hmac, &table->key, &table->header,
			       sizeof(table->header));
		ret = pfsDbTableWriteHeader(table);
		if (ret == 0) {
			table->dirty = 0;
			ret = pfsDbTableWriteDirty(table);
			if (ret == 0)
				ret = pfsLowerSync(table->lf);
		}
	}
	return ret;
}

int pfsDbTableFind(ScePfsDbTable *table, ScePfsDbKey *key, void *hash, SceUInt32 *index,
		   SceUInt16 *attr, SceUInt64 *size)
{
	ScePfsDbRecordKey rkey;
	ScePfsDbRecord record;
	ScePfsHash found;
	int ret;

	ret = pfsDbTableRecordKey(&rkey, key);
	if (ret != 0)
		return ret;
	ret = pfsDbTreeFind(&table->tree, &table->key, &rkey, &record, &found);
	if (ret != 0)
		return ret;
	if (index != NULL)
		*index = record.index;
	if (hash != NULL)
		pfsHashCopy(hash, &found);
	if (attr != NULL)
		*attr = record.attr;
	if (size != NULL)
		*size = record.size;
	return 0;
}

int pfsDbTableInsert(ScePfsDbTable *table, ScePfsDbKey *key, const void *hash, SceUInt32 index,
		     SceUInt16 attr, SceUInt64 size)
{
	ScePfsDbRecordKey rkey;
	ScePfsDbRecord record;
	int ret;

	ret = pfsDbTableRecordKey(&rkey, key);
	if (ret != 0)
		return ret;
	record.size = size;
	record.attr = attr;
	record.index = index;
	ret = pfsDbTreeSet(&table->tree, &table->key, &rkey, &record, hash, PFS_DB_TREE_INSERT);
	if (ret == 0)
		table->dirty = 1;
	return ret;
}

int pfsDbTableUpdate(ScePfsDbTable *table, ScePfsDbKey *key, const void *hash, SceUInt32 *index,
		     SceUInt16 *attr, SceUInt64 *size)
{
	ScePfsDbRecordKey rkey;
	ScePfsDbRecord record;
	ScePfsHash found;
	int ret;

	ret = pfsDbTableRecordKey(&rkey, key);
	if (ret != 0)
		return ret;
	ret = pfsDbTreeFind(&table->tree, &table->key, &rkey, &record, &found);
	if (ret != 0)
		return ret;
	if (index != NULL)
		record.index = *index;
	if (attr != NULL)
		record.attr = *attr;
	if (size != NULL)
		record.size = *size;
	if (hash != NULL)
		pfsHashCopy(&found, hash);
	pfsDbTreeSet(&table->tree, &table->key, &rkey, &record, &found, PFS_DB_TREE_UPDATE);
	table->dirty = 1;
	return 0;
}

int pfsDbTableRemove(ScePfsDbTable *table, ScePfsDbKey *key)
{
	ScePfsDbRecordKey rkey;
	int ret;

	ret = pfsDbTableRecordKey(&rkey, key);
	if (ret != 0)
		return ret;
	ret = pfsDbTreeRemove(&table->tree, &table->key, &rkey);
	if (ret == 0)
		table->dirty = 1;
	return ret;
}

int pfsDbTableGetSize(ScePfsDbTable *table, SceUInt64 *size)
{
	SceUInt32 height;
	int ret;

	ret = pfsDbTreeHeight(&table->tree, &height);
	if (ret == 0)
		*size = (SceUInt64)height * table->header.pageSize;
	return ret;
}

typedef struct ScePfsDbName {			/* name unknown */
	char name[0x3f];
} ScePfsDbName;

int pfsDbTableFirst(ScePfsDbTable *table, SceUInt32 *parentIndex, char *name, SceUInt32 *index,
		    SceUInt16 *attr, SceUInt64 *size)
{
	ScePfsDbRecordKey rkey;
	ScePfsDbRecord record;
	int ret;

	if (parentIndex == NULL || name == NULL || index == NULL)
		return SCE_PFS_ERROR_DB_INVALID_ARG;
	ret = pfsDbTreeNext(&table->tree, NULL, &rkey, &record);
	if (ret != 0)
		return ret;
	*parentIndex = rkey.id.parentIndex;
	*(ScePfsDbName *)name = *(ScePfsDbName *)rkey.id.name;
	*index = record.index;
	if (attr != NULL)
		*attr = record.attr;
	if (size != NULL)
		*size = record.size;
	return 0;
}

int pfsDbTableNext(ScePfsDbTable *table, SceUInt32 *parentIndex, char *name, SceUInt32 *index,
		   SceUInt16 *attr, SceUInt64 *size)
{
	ScePfsDbRecordKey after, rkey;
	ScePfsDbRecord record;
	int ret;

	if (parentIndex == NULL || name == NULL || index == NULL)
		return SCE_PFS_ERROR_DB_INVALID_ARG;
	memset(&after, 0, sizeof(after));
	*(ScePfsDbName *)after.id.name = *(ScePfsDbName *)name;
	after.id.parentIndex = *parentIndex;
	ret = pfsDbTreeNext(&table->tree, &after, &rkey, &record);
	if (ret != 0)
		return ret;
	*parentIndex = rkey.id.parentIndex;
	*(ScePfsDbName *)name = *(ScePfsDbName *)rkey.id.name;
	*index = record.index;
	if (attr != NULL)
		*attr = record.attr;
	if (size != NULL)
		*size = record.size;
	return 0;
}
