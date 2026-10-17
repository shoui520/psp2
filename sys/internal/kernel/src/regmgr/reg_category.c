/*
 * reg_category.c: the reg library's categories and keys, in the open
 * registries. All names unknown.
 */

#include <psp2kern/io/fcntl.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/utils.h>
#include "reg_internal.h"

SceUInt8 g_regBlockCheck[4];	/* name unknown */
int g_regCategoryNum;
RegCategory *g_regCategoryList;
RegCategory *g_regCategoryTail;

/* the entry of the category name under parent, searched from its hash slot */
int regFindCategoryEntry(int registry, RegIndexImage *image, SceUInt32 parent, SceUInt32 slot,
			 const char *name, RegCategoryEntry **entry)
{
	RegCategoryEntry *table;
	RegCategoryEntry *e;
	SceUInt32 num;
	SceUInt32 i;
	int ret;

	num = image->categoryNum;
	table = (RegCategoryEntry *)((char *)image + image->categoryOffset);
	if (parent != (SceUInt32)-1 && table[parent].slot == REG_CATEGORY_NONE) {
		ret = REG_ERROR_PARENT_NOT_FOUND;
		goto fail;
	}

	i = slot;
	do {
		e = &table[i];
		if (e->slot == (SceUInt16)slot && e->parent == (SceUInt16)parent) {
			if (strncmp(e->name, name, REG_CATEGORY_NAME_LEN) == 0) {
				e->registry = regGetRegistry(registry);
				if (e->registry == NULL) {
					ret = REG_ERROR_NOT_FOUND;
					goto fail;
				}
				*entry = e;
				return 0;
			}
		}
		i++;
		if (i >= num)
			i = 0;
	} while (i != slot);
	ret = REG_ERROR_CATEGORY_NOT_FOUND;

fail:
	*entry = NULL;

	return ret;
}

/* read a category's data blocks into a new cache */
int regReadCategoryBlocks(RegCategoryEntry *entry, SceUID fd, RegCategoryCache **cache)
{
	RegCategoryCache *c;
	SceUInt8 *data;
	int read;
	int ret;
	int i;

	data = regMemAlloc(entry->blockNum * REG_BLOCK_SIZE);
	if (data == NULL) {
		ret = REG_ERROR_NO_MEMORY_HEAP;
		goto fail;
	}

	memset(data, 0, entry->blockNum * REG_BLOCK_SIZE);
	for (i = 0; i < entry->blockNum; i++) {
		if (entry->block[i] == REG_CATEGORY_NONE)
			continue;
		if (regFileSeek(fd, entry->block[i] * REG_BLOCK_SIZE, SCE_SEEK_SET) != 0) {
			ret = REG_ERROR_BLOCK_SEEK;
			goto free;
		}
		if (regFileRead(data + i * REG_BLOCK_SIZE, 1, REG_BLOCK_SIZE, fd, &read) != 0) {
			ret = REG_ERROR_COPY_READ;
			goto free;
		}
	}

	c = regMemAlloc(sizeof(*c));
	if (c != NULL) {
		memset(c, 0, sizeof(*c));
		c->openCount = 0;
		c->info = 1;
		c->data = (RegDataBlock *)data;
		*cache = c;
		return 0;
	}
	ret = REG_ERROR_NO_MEMORY_HEAP;

free:
	memset(data, 0, entry->blockNum * REG_BLOCK_SIZE);
	regMemFree(data);
fail:
	*cache = NULL;

	return ret;
}

/* the 4-byte check of a data block: its SHA-1 folded, the check field zeroed first */
int regComputeBlockCheck(RegDataBlock *block, SceSize size, SceUInt8 *check)
{
	SceUInt8 digest[0x14];
	int ret;
	int i;
	int j;

	block->check[0] = 0;
	block->check[1] = 0;
	block->check[2] = 0;
	block->check[3] = 0;
	ret = SceKernelUtilsForDriver_87DC7F2F(block, size, digest);
	if (ret >= 0) {
		ret = 0;
		for (i = 0; i < 4; i++) {
			check[i] = 0;
			for (j = 0; j < 5; j++)
				check[i] ^= digest[i * 5 + j];
		}
	}

	return ret;
}

/* free a category entry: its data blocks zeroed and released, its cache freed */
void regFreeCategoryEntry(RegCategoryEntry *entry, RegIndexImage *image)
{
	RegRegistry *reg;
	SceUInt8 *bitmap;
	SceUInt16 block;
	void *zero;
	int written;
	SceUID fd;
	int i;

	for (i = 0; i < entry->blockNum; i++) {
		block = entry->block[i];
		bitmap = (SceUInt8 *)image + image->bitmapOffset;
		bitmap[block >> 3] &= ~(1 << (block & 7));
		image->freeBlockNum++;

		reg = NULL;
		fd = -1;
		zero = regMemAlloc(REG_BLOCK_SIZE);
		if (zero != NULL) {
			memset(zero, 0, REG_BLOCK_SIZE);
			reg = entry->registry;
			fd = regOpenRegistryFile(reg, REG_FILE_DATA);
			if (fd != -1 && regFileSeek(fd, block * REG_BLOCK_SIZE, SCE_SEEK_SET) == 0)
				regFileWrite(zero, 1, REG_BLOCK_SIZE, fd, &written);
		}
		regCloseRegistryFile(reg, fd, REG_FILE_DATA);
		if (zero != NULL) {
			memset(zero, 0, REG_BLOCK_SIZE);
			regMemFree(zero);
		}
	}

	if (entry->cache != NULL) {
		if (entry->cache->data != NULL) {
			memset(entry->cache->data, 0, entry->blockNum * REG_BLOCK_SIZE);
			regMemFree(entry->cache->data);
		}
		memset(entry->cache, 0, sizeof(*entry->cache));
		regMemFree(entry->cache);
		entry->cache = NULL;
	}

	memset(entry, 0, sizeof(*entry));
	entry->slot = REG_CATEGORY_NONE;
	for (i = 0; i < REG_CATEGORY_BLOCK_MAX; i++)
		entry->block[i] = REG_CATEGORY_NONE;
}

/* give an entry its cache: read and checked, or made empty with create */
int regLoadCategoryCache(int create, RegRegistry *reg, RegCategoryEntry *entry)
{
	RegCategoryCache *cache = NULL;
	RegDataBlock *data;
	SceUInt8 stored[4];
	SceUInt8 check[4];
	SceUInt16 n;
	SceUID fd;
	int ret;

	if (entry->cache != NULL) {
		entry->cache->openCount++;
		return 0;
	}

	n = entry->blockNum;
	if (n == 0) {
		ret = REG_ERROR_NO_BLOCKS;
		goto fail;
	}

	if (create == 0) {
		fd = regOpenRegistryFile(reg, REG_FILE_DATA);
		if (fd == -1) {
			ret = REG_ERROR_NOT_FOUND;
			goto fail;
		}
		ret = regReadCategoryBlocks(entry, fd, &cache);
		regCloseRegistryFile(reg, fd, REG_FILE_DATA);
		if (ret != 0)
			goto fail;
	} else {
		data = regMemAlloc(n * REG_BLOCK_SIZE);
		if (data == NULL) {
			ret = REG_ERROR_NO_MEMORY_HEAP;
			goto fail;
		}
		memset(data, 0, n * REG_BLOCK_SIZE);
		data->recordMax = n * 16 - 1;
		data->blockNum = n;
		data->info = n * 32;
		data->freeRecordNum = n * 16 - 1;
		data->dataRecord = n * 16 - 1;
		data->keyNum = 0;
		memset(data->recordBitmap + 1, 0, 7);
		data->recordNum = 1;
		data->recordBitmap[0] = 0x80;

		cache = regMemAlloc(sizeof(*cache));
		if (cache == NULL) {
			memset(data, 0, n * REG_BLOCK_SIZE);
			regMemFree(data);
			ret = REG_ERROR_NO_MEMORY_HEAP;
			goto fail;
		}
		memset(cache, 0, sizeof(*cache));
		cache->info = 1;
		cache->data = data;
	}

	memset(check, 0, sizeof(check));
	data = cache != NULL ? cache->data : NULL;
	memcpy(stored, check, sizeof(stored));
	if (data != NULL)
		memcpy(stored, data->check, sizeof(stored));
	ret = regComputeBlockCheck(data, entry->blockNum * REG_BLOCK_SIZE, check);
	if (ret != 0)
		goto free;
	if (create == 0 && memcmp(stored, check, sizeof(check)) != 0) {
		ret = REG_ERROR_BAD_CHECK;
		goto free;
	}
	if (data != NULL)
		memcpy(data->check, stored, sizeof(stored));

	entry->cache = cache;
	cache->openCount++;

	return 0;

free:
	if (cache != NULL) {
		if (cache->data != NULL) {
			memset(cache->data, 0, entry->blockNum * REG_BLOCK_SIZE);
			regMemFree(cache->data);
		}
		memset(cache, 0, sizeof(*cache));
		regMemFree(cache);
	}
fail:
	entry->cache = NULL;

	return ret;
}

/* add a category to the open list, unless it is there */
void regAddOpenCategory(int registry, int handle)
{
	RegCategory *cat;

	if (regFindCategory(handle) != 0)
		return;

	cat = regMemAlloc(sizeof(*cat));
	cat->modified = 0;
	cat->next = NULL;
	cat->registry = registry;
	cat->handle = handle;
	if (g_regCategoryList != NULL)
		g_regCategoryTail->next = cat;
	else
		g_regCategoryList = cat;
	g_regCategoryTail = cat;
	g_regCategoryNum++;
}

/* the entry of the category path names, from *index on: its index in *index */
int regFindCategoryByPath(const char *path, RegIndexImage *image, int *index, const SceUInt32 *id)
{
	RegCategoryEntry *entry;
	const char *sep;
	char *parent;
	char *name;
	SceUInt32 len;
	int start;
	int ret;
	int i;

	parent = regMemAlloc(REG_CATEGORY_PATH_LEN + 1);
	if (parent == NULL)
		return REG_ERROR_NO_MEMORY_HEAP;

	name = regMemAlloc(REG_CATEGORY_NAME_LEN + 1);
	if (name == NULL) {
		memset(parent, 0, REG_CATEGORY_PATH_LEN + 1);
		regMemFree(parent);
		return REG_ERROR_NO_MEMORY_HEAP;
	}

	memset(parent, 0, REG_CATEGORY_PATH_LEN + 1);
	memset(name, 0, REG_CATEGORY_NAME_LEN + 1);
	ret = REG_ERROR_BAD_CATEGORY_PATH;
	if (path[0] != '/')
		goto out;
	len = strnlen(path, REG_CATEGORY_PATH_LEN);
	if (len > REG_CATEGORY_PATH_LEN)
		goto out;

	sep = NULL;
	for (i = 0; i != len + 1; i++) {
		if (path[len - i] == '/') {
			sep = &path[len - i + 1];
			break;
		}
	}
	if (sep == NULL || *sep == '\0')
		goto out;

	if (sep - path > 1)
		memcpy(parent, path, sep - path - 1);
	if (i != 1)
		memcpy(name, sep, i - 1);

	start = *index;
	i = start;
	do {
		entry = (RegCategoryEntry *)((char *)image + image->categoryOffset) + i;
		if (id != NULL && strncmp(name, entry->name, REG_CATEGORY_NAME_LEN) == 0 &&
		    entry->id == *id) {
			*index = i;
			ret = 0;
			goto out;
		}
		if (++i > image->categoryNum)
			i = 0;
	} while (i != start);
	ret = REG_ERROR_PARENT_NOT_FOUND;
	*index = -1;

out:
	memset(parent, 0, REG_CATEGORY_PATH_LEN + 1);
	regMemFree(parent);
	memset(name, 0, REG_CATEGORY_NAME_LEN + 1);
	regMemFree(name);

	return ret;
}

/* give a category the blocks for size more bytes of records, when it lacks them */
int regReserveCategorySpace(RegCategoryEntry *entry, int size)
{
	RegIndexImage *image;
	RegRegistry *reg;
	RegCategory *cat;
	SceUInt8 *bitmap;
	SceUInt16 oldNum;
	SceUInt16 block;
	int blocks;
	int need;
	int free;
	int got;
	int bit;
	int b;
	int i;

	need = (size + 31) / 32;
	free = entry->cache->data->freeRecordNum;
	if (need <= free)
		return REG_ERROR_NO_SPACE;

	oldNum = entry->blockNum;
	blocks = (need + 15 - free) / 16;
	if (oldNum + blocks > REG_CATEGORY_BLOCK_MAX - 1)
		return REG_ERROR_TOO_MANY_BLOCKS;

	for (got = 0; got < blocks; got++) {
		image = entry->registry->image;
		if (image->freeBlockNum == 0)
			goto undo;

		bitmap = (SceUInt8 *)image + image->bitmapOffset;
		for (b = 0; b < image->bitmapSize; b++) {
			if (bitmap[b] == 0xff)
				continue;
			for (bit = 0; bit < 8; bit++) {
				if ((bitmap[b] & (1 << bit)) == 0)
					break;
			}
			break;
		}
		if (b == image->bitmapSize)
			goto undo;

		bitmap[b] |= 1 << bit;
		image->freeBlockNum--;
		entry->block[entry->blockNum++] = b * 8 + bit;
	}

	cat = g_regCategoryList;
	for (i = 0; i < g_regCategoryNum; i++) {
		if (cat->handle == (int)entry) {
			reg = regGetRegistry(cat->registry);
			if (reg != NULL)
				reg->modified = 1;
			return 0;
		}
		cat = cat->next;
	}

	return 0;

undo:
	for (i = 0; i < got; i++) {
		image = entry->registry->image;
		bitmap = (SceUInt8 *)image + image->bitmapOffset;
		block = entry->block[oldNum + i];
		bitmap[block >> 3] &= ~(1 << (block & 7));
		image->freeBlockNum++;
		entry->block[oldNum + i] = REG_CATEGORY_NONE;
	}
	entry->blockNum = oldNum;

	return REG_ERROR_NO_FREE_BLOCK;
}

/* take a category off the open list */
static inline __attribute__((always_inline)) void regRemoveOpenCategory(int handle)
{
	RegCategory *prev;
	RegCategory *cat;
	int i;

	prev = NULL;
	cat = g_regCategoryList;
	for (i = 0; i < g_regCategoryNum; i++) {
		if (cat->handle == handle)
			break;
		prev = cat;
		cat = cat->next;
	}
	if (cat == NULL)
		return;

	if (g_regCategoryList == cat) {
		g_regCategoryList = cat->next;
	} else {
		if (prev != NULL)
			prev->next = cat->next;
		if (cat == g_regCategoryTail)
			g_regCategoryTail = prev;
	}
	memset(cat, 0, sizeof(*cat));
	regMemFree(cat);
	g_regCategoryNum--;
}

/* close an open category: flushed, its cache freed with its last open */
int regCloseCategoryUnlocked(int handle)
{
	RegCategoryEntry *entry;
	RegCategoryCache *cache;
	int ret;

	if (regFindCategory(handle) == 0)
		return REG_ERROR_CATEGORY_NOT_OPEN;

	ret = regFlushCategory(handle);
	if (ret != 0)
		return ret;

	entry = (RegCategoryEntry *)handle;
	cache = entry->cache;
	if (cache == NULL)
		return REG_ERROR_NO_CACHE;
	if (--cache->openCount != 0)
		return 0;

	memset(cache->data, 0, entry->blockNum * REG_BLOCK_SIZE);
	regMemFree(entry->cache->data);
	memset(entry->cache, 0, sizeof(*entry->cache));
	regMemFree(entry->cache);
	entry->cache = NULL;

	regRemoveOpenCategory(handle);

	return 0;
}

/* close an open category */
int regCloseCategory(int handle)
{
	int state;
	int ret;

	state = regPlatformLock();
	if (ksceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	ret = regCloseCategoryUnlocked(handle);
	if (ksceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* the number of top-level categories of a registry */
int regGetCategoryNum(int registry, int *num)
{
	RegCategoryEntry *entry;
	RegIndexImage *image;
	RegRegistry *reg;
	int count;
	int state;
	int ret;
	int i;

	state = regPlatformLock();
	if (regPlatformCheckPointer(num) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}

	if (ksceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	reg = regGetRegistry(registry);
	if (reg == NULL) {
		ret = REG_ERROR_NOT_FOUND;
	} else {
		image = reg->image;
		entry = (RegCategoryEntry *)((char *)image + image->categoryOffset);
		count = 0;
		for (i = 0; i < image->categoryNum; i++, entry++) {
			if (entry->parent == REG_CATEGORY_NONE)
				count++;
		}
		*num = count;
		ret = 0;
	}

	if (ksceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* the names of the top-level categories of a registry */
int regGetCategoryNames(int registry, char (*names)[REG_CATEGORY_NAME_LEN + 1], int num)
{
	RegCategoryEntry *entry;
	RegIndexImage *image;
	RegRegistry *reg;
	int count;
	int state;
	int ret;
	int i;

	state = regPlatformLock();
	if (regPlatformCheckBuffer(names, num * (REG_CATEGORY_NAME_LEN + 1)) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}

	if (ksceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	reg = regGetRegistry(registry);
	if (reg == NULL) {
		ret = REG_ERROR_NOT_FOUND;
	} else {
		image = reg->image;
		entry = (RegCategoryEntry *)((char *)image + image->categoryOffset);
		count = 0;
		for (i = 0; i < image->categoryNum; i++, entry++) {
			if (entry->parent != REG_CATEGORY_NONE)
				continue;
			memset(names[count], 0, REG_CATEGORY_NAME_LEN + 1);
			memcpy(names[count], entry->name, REG_CATEGORY_NAME_LEN);
			count++;
		}
		ret = 0;
	}

	if (ksceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* handle, when it is an open category; 0 otherwise */
int regFindCategory(int handle)
{
	RegCategory *cat;
	int i;

	cat = g_regCategoryList;
	for (i = 0; i < g_regCategoryNum; i++) {
		if (cat->handle == handle)
			return handle;
		cat = cat->next;
	}

	return 0;
}

int regSetRegistryModified(int registry)
{
	RegRegistry *reg;

	reg = regGetRegistry(registry);
	if (reg == NULL)
		return REG_ERROR_NOT_FOUND;

	reg->modified = 1;

	return 0;
}

/* mark an open category modified and renew the check of its data */
int regUpdateCategoryCheck(int handle)
{
	RegCategoryEntry *entry;
	RegDataBlock *data;
	RegCategory *cat;
	int ret;
	int i;

	entry = (RegCategoryEntry *)handle;
	data = entry->cache->data;
	cat = g_regCategoryList;
	for (i = 0; i < g_regCategoryNum; i++) {
		if (cat->handle == handle)
			break;
		cat = cat->next;
	}
	if (i == g_regCategoryNum)
		return REG_ERROR_CATEGORY_NOT_OPEN;

	cat->modified = 1;
	ret = regComputeBlockCheck(data, entry->blockNum << 9, g_regBlockCheck);
	if (ret != 0)
		return ret;

	data->check[0] = g_regBlockCheck[0];
	data->check[1] = g_regBlockCheck[1];
	data->check[2] = g_regBlockCheck[2];
	data->check[3] = g_regBlockCheck[3];

	return 0;
}

/* open the category path names, made first when missing with REG_OPEN_CREATE */
int regOpenCategory(int registry, const char *path, int mode, int *handle)
{
	RegCategoryEntry *parentEntry;
	RegCategoryEntry *entry = NULL;
	RegCategoryEntry *e = NULL;
	RegCategoryEntry *table;
	RegIndexImage *image;
	RegRegistry *reg;
	SceUInt8 *bitmap;
	const char *sep;
	char *parent;
	char *name;
	SceUInt32 len;
	SceUInt32 id;
	int parentIndex;
	int create;
	int block;
	int state;
	int hash;
	int slot;
	int ret;
	int bit;
	int b;
	int i;

	state = regPlatformLock();
	if (path == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_NO_PATH;
	}
	if (regPlatformCheckPointer(path) == NULL || regPlatformCheckPointer(handle) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}
	if (ksceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	parent = regMemAlloc(REG_CATEGORY_PATH_LEN + 1);
	if (parent == NULL) {
		ret = REG_ERROR_NO_MEMORY_HEAP;
		goto unlock;
	}
	name = regMemAlloc(REG_CATEGORY_NAME_LEN + 1);
	if (name == NULL) {
		memset(parent, 0, REG_CATEGORY_PATH_LEN + 1);
		ret = REG_ERROR_NO_MEMORY_HEAP;
		regMemFree(parent);
		goto unlock;
	}
	memset(parent, 0, REG_CATEGORY_PATH_LEN + 1);
	memset(name, 0, REG_CATEGORY_NAME_LEN + 1);

	reg = regGetRegistry(registry);
	if (reg == NULL) {
		ret = REG_ERROR_NOT_FOUND;
		goto free;
	}
	image = reg->image;
	table = (RegCategoryEntry *)((char *)image + image->categoryOffset);

	ret = REG_ERROR_BAD_CATEGORY_PATH;
	if (path[0] != '/')
		goto free;
	len = strnlen(path, REG_CATEGORY_PATH_LEN);
	if (len > REG_CATEGORY_PATH_LEN)
		goto free;
	sep = NULL;
	for (i = 0; i != len + 1; i++) {
		if (path[len - i] == '/') {
			sep = &path[len - i + 1];
			break;
		}
	}
	if (sep == NULL || *sep == '\0')
		goto free;
	if (sep - path > 1)
		memcpy(parent, path, sep - path - 1);
	if (i != 1)
		memcpy(name, sep, i - 1);

	parentIndex = regHashName((SceChar8 *)parent, image->categoryNum, (int *)&id);
	if (parent[0] == '\0') {
		parentIndex = -1;
	} else {
		ret = regFindCategoryByPath(parent, image, &parentIndex, &id);
		if (ret != 0)
			goto free;
	}

	slot = regHashName((const SceChar8 *)path, image->categoryNum, &hash);
	ret = regFindCategoryEntry(registry, image, parentIndex, slot, name, &entry);
	if (ret == REG_ERROR_PARENT_NOT_FOUND)
		goto free;

	create = 0;
	if (mode == REG_OPEN_CREATE) {
		if (ret != REG_ERROR_CATEGORY_NOT_FOUND)
			goto load;

		/* a free data block for it */
		bitmap = (SceUInt8 *)image + image->bitmapOffset;
		block = -1;
		if (image->freeBlockNum != 0) {
			for (b = 0; b < image->bitmapSize; b++) {
				if (bitmap[b] == 0xff)
					continue;
				for (bit = 0; bit < 8; bit++) {
					if ((bitmap[b] & (1 << bit)) == 0)
						break;
				}
				block = b * 8 + bit;
				break;
			}
		}
		if (block < 0) {
			ret = REG_ERROR_NO_FREE_BLOCK;
			entry = NULL;
			goto free;
		}
		bitmap[block >> 3] |= 1 << (block & 7);
		image->freeBlockNum--;

		/* a free entry from its hash slot on */
		i = slot;
		do {
			if (table[i].slot == REG_CATEGORY_NONE) {
				e = &table[i];
				break;
			}
			if (++i >= image->categoryNum)
				i = 0;
		} while (i != slot);
		if (e == NULL)
			goto undo;

		e->slot = slot;
		e->parent = parentIndex;
		e->id = hash;
		e->registry = NULL;
		e->keyNum = 0;
		e->blockNum = 1;
		memcpy(e->name, name, REG_CATEGORY_NAME_LEN + 1);
		memset(e->block, 0xff, sizeof(e->block));
		e->cache = NULL;
		e->block[0] = block;
		e->registry = regGetRegistry(registry);
		if (e->registry == NULL) {
			ret = REG_ERROR_NOT_FOUND;
			goto undo;
		}
		parentEntry = NULL;
		if (e->parent != REG_CATEGORY_NONE) {
			parentEntry = &table[e->parent];
			parentEntry->registry = e->registry;
		}
		entry = e;

		if (parentIndex != -1) {
			ret = regLoadCategoryCache(0, reg, parentEntry);
			if (ret != 0) {
				regCloseCategoryUnlocked((int)parentEntry);
				regFreeCategoryEntry(entry, image);
				goto free;
			}
			regAddOpenCategory(registry, (int)parentEntry);
			ret = regAddCategoryKey(parentEntry, name);
			if (ret != 0) {
				regCloseCategoryUnlocked((int)parentEntry);
				regFreeCategoryEntry(entry, image);
				goto free;
			}
			regUpdateCategoryCheck((int)parentEntry);
			regSetRegistryModified(registry);
			regCloseCategoryUnlocked((int)parentEntry);
		}

		create = 1;
		if (g_regRegistryOpened == 0)
			regFlushIndex(registry);
	} else if (mode != REG_OPEN_EXISTING) {
		ret = REG_ERROR_BAD_OPEN_MODE;
		goto free;
	} else if (ret != 0) {
		goto free;
	}

load:
	ret = regLoadCategoryCache(create, reg, entry);
	if (ret == 0) {
		regAddOpenCategory(registry, (int)entry);
		if (create) {
			regUpdateCategoryCheck((int)entry);
			regSetRegistryModified(registry);
		}
		*handle = (int)entry;
	} else {
		regCloseCategoryUnlocked((int)entry);
	}
	goto free;

undo:
	bitmap[block >> 3] &= ~(1 << (block & 7));
	image->freeBlockNum++;
	if (e != NULL) {
		e->registry = NULL;
		e->parent = 0;
		e->slot = REG_CATEGORY_NONE;
		e->id = REG_CATEGORY_NONE;
		memset(e->name, 0, REG_CATEGORY_NAME_LEN + 1);
		memset(e->block, 0xff, 8);
		e->cache = NULL;
	}
	entry = NULL;
free:
	memset(parent, 0, REG_CATEGORY_PATH_LEN + 1);
	regMemFree(parent);
	memset(name, 0, REG_CATEGORY_NAME_LEN + 1);
	regMemFree(name);
unlock:
	if (ksceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* remove the category path names, with its key in its parent */
int regRemoveCategory(int registry, const char *path, int flags)
{
	char name[REG_CATEGORY_NAME_LEN + 1] = { 0 };
	char parent[REG_CATEGORY_PATH_LEN + 1];
	RegCategoryEntry *parentEntry;
	RegCategoryEntry *entry = NULL;
	RegIndexImage *image;
	RegDataBlock *data;
	RegRegistry *reg;
	const char *sep;
	SceUInt32 len;
	SceUInt32 id;
	int parentIndex;
	int state;
	int slot;
	int ret;
	int i;

	state = regPlatformLock();
	if (path == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_NO_PATH;
	}
	if (regPlatformCheckPointer(path) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}
	if (ksceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	memset(parent, 0, sizeof(parent));
	reg = regGetRegistry(registry);
	if (reg == NULL) {
		ret = REG_ERROR_NOT_FOUND;
		goto unlock;
	}
	image = reg->image;

	ret = REG_ERROR_BAD_CATEGORY_PATH;
	if (path[0] != '/')
		goto unlock;
	len = strnlen(path, REG_CATEGORY_PATH_LEN);
	if (len > REG_CATEGORY_PATH_LEN)
		goto unlock;
	sep = NULL;
	for (i = 0; i != len + 1; i++) {
		if (path[len - i] == '/') {
			sep = &path[len - i + 1];
			break;
		}
	}
	if (sep == NULL || *sep == '\0')
		goto unlock;
	if (sep - path > 1)
		memcpy(parent, path, sep - path - 1);
	if (i != 1)
		memcpy(name, sep, i - 1);

	parentIndex = regHashName((SceChar8 *)parent, image->categoryNum, (int *)&id);
	if (parent[0] == '\0') {
		parentIndex = -1;
	} else {
		ret = regFindCategoryByPath(parent, image, &parentIndex, &id);
		if (ret != 0)
			goto unlock;
	}

	slot = regHashName((const SceChar8 *)path, image->categoryNum, NULL);
	ret = regFindCategoryEntry(registry, image, parentIndex, slot, name, &entry);
	if (ret != 0)
		goto unlock;
	if (entry->cache != NULL && entry->cache->openCount != 0) {
		ret = REG_ERROR_CATEGORY_IN_USE;
		goto unlock;
	}

	ret = regLoadCategoryCache(0, reg, entry);
	if (ret != 0) {
		regCloseCategoryUnlocked((int)entry);
		goto unlock;
	}
	regAddOpenCategory(registry, (int)entry);

	data = entry->cache->data;
	for (i = 0; i < data->keyNum; i++) {
		if (data->key[i].type == REG_TYPE_CATEGORY) {
			ret = REG_ERROR_HAS_CATEGORIES;
			goto unlock;
		}
	}

	if (parentIndex != -1) {
		parentEntry = (RegCategoryEntry *)((char *)image + image->categoryOffset) + parentIndex;
		parentEntry->registry = reg;
		regAddOpenCategory(registry, (int)parentEntry);
		ret = regLoadCategoryCache(0, reg, parentEntry);
		if (ret != 0) {
			regCloseCategoryUnlocked((int)parentEntry);
			goto unlock;
		}
		regDeleteKeyUnlocked((int)parentEntry, name);
		regUpdateCategoryCheck((int)parentEntry);
		regSetRegistryModified(registry);
		regCloseCategoryUnlocked((int)parentEntry);
	}

	regFreeCategoryEntry(entry, image);
	regSetRegistryModified(registry);
	if (g_regRegistryOpened == 0)
		regFlushIndex(registry);
	regRemoveOpenCategory((int)entry);

unlock:
	if (ksceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

int regClearRegistryModified(int registry)
{
	RegRegistry *reg;

	reg = regGetRegistry(registry);
	if (reg == NULL)
		return REG_ERROR_NOT_FOUND;

	reg->modified = 0;

	return 0;
}

int regClearCategoryModified(int handle)
{
	RegCategory *cat;
	int i;

	cat = g_regCategoryList;
	for (i = 0; i < g_regCategoryNum; i++) {
		if (cat->handle == handle) {
			cat->modified = 0;
			return 0;
		}
		cat = cat->next;
	}

	return REG_ERROR_CATEGORY_NOT_OPEN;
}

/* 0 when the registry was modified */
int regIsRegistryModified(int registry)
{
	RegRegistry *reg;

	reg = regGetRegistry(registry);
	if (reg == NULL)
		return REG_ERROR_NOT_FOUND;

	return reg->modified == 1 ? 0 : REG_ERROR_REGISTRY_CLEAN;
}

/* 0 when the category was modified */
int regIsCategoryModified(int handle)
{
	RegCategory *cat;
	int i;

	cat = g_regCategoryList;
	for (i = 0; i < g_regCategoryNum; i++) {
		if (cat->handle == handle)
			return cat->modified == 1 ? 0 : REG_ERROR_CATEGORY_CLEAN;
		cat = cat->next;
	}

	return REG_ERROR_CATEGORY_NOT_OPEN;
}
