/*
 * SceLibMtp: the object property cache (CACH).
 *
 * A logical storage keeps its most used database rows in memory: a property is read from and
 * written to the cached row, and a written row goes back to the database when it leaves the
 * cache (the least used row leaves first, folders before files) or when the storage writes
 * its cache back.
 */

#include <kernel/error.h>

#include "mtp_internal.h"

#define MTP_TAG_CACH	"CACH:"

/* a cached row */
typedef struct MtpCacheData MtpCacheData;
struct MtpCacheData {
	SceUInt32 useCount;
	int dirty;			/* written since it was read from the database */
	ObjectData object;
	MtpCacheData *next;
};

struct MtpCache {
	MtpLogicalStorage *storage;
	SceUInt32 maxEntries;
	SceUInt32 numEntries;
	MtpCacheData *head;
	SceUID mutex;
};

extern "C" {

static inline void mtpCacheLock(MtpCache *cache)
{
	int ret;

	ret = sceKernelLockMutex(cache->mutex, 1, NULL);
	if (ret != SCE_OK && ret != SCE_KERNEL_ERROR_MUTEX_RECURSIVE)
		MTP_ERR(MTP_TAG_CACH, 885, "unable to lock mutex for cache %x\n", ret);
}

static inline void mtpCacheUnlock(MtpCache *cache)
{
	int ret;

	if (cache == NULL) {
		MTP_ERR(MTP_TAG_CACH, 903, "invalid parameter\n");
		return;
	}
	ret = sceKernelUnlockMutex(cache->mutex, 1);
	if (ret != SCE_OK && ret != SCE_KERNEL_ERROR_MUTEX_NOT_OWNED)
		MTP_ERR(MTP_TAG_CACH, 909, "unable to unlock mutex for cache %x\n", ret);
}

/* appends a row */
static inline void _cacheAddTail(MtpCache *cache, MtpCacheData *data)
{
	MtpCacheData *last;

	if (cache == NULL || data == NULL) {
		MTP_ERR(MTP_TAG_CACH, 68, "invalid parameter\n");
		return;
	}
	last = cache->head;
	if (last == NULL) {
		cache->head = data;
	} else {
		while (last->next != NULL)
			last = last->next;
		last->next = data;
	}
	cache->numEntries++;
}

/* removes the least used row (a folder's before a file's), writing it back first */
static inline int _cacheDelLFU(MtpCache *cache)
{
	MtpCacheData *victim;
	MtpCacheData *data;
	MtpCacheData *prev;
	MtpCacheData *next;
	int ret;

	victim = cache->head;
	for (data = cache->head; data != NULL; data = data->next) {
		if (victim->useCount > data->useCount)
			victim = data;
		else if (victim->object.basic.objectFormat == MTP_FORMAT_ASSOCIATION)
			victim = data;
	}
	if (victim->dirty == 1) {
		ret = mtpDBSetObjectProperty(cache->storage->db, victim->object.basic.uid,
			MTP_DB_QUERY_OBJECT_DATA, &victim->object, sizeof(victim->object));
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CACH, 137, "unable to set data to DB=0x%x\n", ret);
			return ret;
		}
		victim->dirty = 0;
	}
	prev = cache->head;
	if (victim == prev) {
		cache->head = prev->next;
		mtpUtilFree(prev);
	} else if (prev != NULL) {
		for (;;) {
			next = prev->next;
			if (victim == next)
				break;
			prev = next;
			if (next == NULL)
				goto out;
		}
		prev->next = victim->next;
		mtpUtilFree(victim);
	}
out:
	if (prev == NULL) {
		MTP_ERR(MTP_TAG_CACH, 161, "not deleted ?");
		return 0;
	}
	cache->numEntries--;
	return 0;
}

/* adds a row, making room for it */
static __attribute__((noinline)) int _cacheAdd(MtpCache *cache, MtpCacheData *data)
{
	if (cache == NULL || data == NULL) {
		MTP_ERR(MTP_TAG_CACH, 186, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (cache->numEntries >= cache->maxEntries && cache->head != NULL)
		_cacheDelLFU(cache);
	_cacheAddTail(cache, data);
	return 0;
}

/* copies a property out of a cached row */
static inline int _cacheCopyObjectPropertyData(MtpCacheData *data, int prop, void *buf,
	SceSize size)
{
	SceUInt32 offset;
	SceUInt32 propSize;
	int ret;

	ret = mtpDBGetObjectPropertyOffset(prop, &offset, &propSize);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CACH, 373, "unable to survey offset and size=0x%x\n", ret);
		return ret;
	}
	if (size < propSize) {
		MTP_ERR(MTP_TAG_CACH, 379, "bufferSize too small\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	mtpUtilMemcpy(buf, (SceUInt8 *)&data->object + offset, size);
	return ret;
}

static __attribute__((noinline)) int _cacheLookupAndGet(MtpCache *cache, SceUInt32 uniqueId,
	int prop, void *buf, SceSize size)
{
	MtpCacheData *data;
	int ret;

	if (cache == NULL || buf == NULL) {
		MTP_ERR(MTP_TAG_CACH, 278, "invalid parameter");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	for (data = cache->head; data != NULL; data = data->next) {
		if (data->object.basic.uid == uniqueId)
			break;
	}
	if (data == NULL)
		return SCE_MTP_ERROR_NOT_FOUND;
	data->useCount++;
	ret = _cacheCopyObjectPropertyData(data, prop, buf, size);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CACH, 294, "unable to set object property data=0x%x\n", ret);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	return ret;
}

/* writes a property into a cached row */
static inline int _cacheSetObjectPropertyData(MtpCacheData *data, int prop, const void *buf)
{
	SceUInt32 offset;
	SceUInt32 propSize;
	int ret;

	ret = mtpDBGetObjectPropertyOffset(prop, &offset, &propSize);
	if (ret < 0)
		MTP_ERR(MTP_TAG_CACH, 420, "unable to survey offset and size=0x%x\n", ret);
	else
		mtpUtilMemcpy((SceUInt8 *)&data->object + offset, buf, propSize);
	return ret;
}

static __attribute__((noinline)) int _cacheLookupAndSet(MtpCache *cache, SceUInt32 uniqueId,
	int prop, const void *buf)
{
	MtpCacheData *data;
	int ret;

	if (cache == NULL || buf == NULL) {
		MTP_ERR(MTP_TAG_CACH, 322, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	for (data = cache->head; data != NULL; data = data->next) {
		if (data->object.basic.uid == uniqueId)
			break;
	}
	if (data == NULL)
		return SCE_MTP_ERROR_NOT_FOUND;
	data->dirty = 1;
	data->useCount++;
	ret = _cacheSetObjectPropertyData(data, prop, buf);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CACH, 337, "unable to set object property to cachedata=0x%x\n",
			ret);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	return ret;
}

int mtpCacheInitialize(MtpLogicalStorage *storage, MtpCache **cache, SceUInt32 maxEntries)
{
	MtpCache *c;
	SceUID mutex;

	if (storage == NULL || cache == NULL) {
		MTP_ERR(MTP_TAG_CACH, 454, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	c = (MtpCache *)mtpUtilMalloc(sizeof(*c));
	if (c == NULL) {
		MTP_ERR(MTP_TAG_CACH, 462, "unable to alloc memory for cache\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mutex = sceKernelCreateMutex("SceMtpCache", 0, 0, NULL);
	if (mutex < 0) {
		MTP_ERR(MTP_TAG_CACH, 470, "unable to create mutex fo cache=0x%x\n", mutex);
		mtpUtilFree(c);
		*cache = NULL;
		return mutex;
	}
	c->mutex = mutex;
	c->head = NULL;
	c->storage = storage;
	c->maxEntries = maxEntries;
	c->numEntries = 0;
	*cache = c;
	return mutex;
}

int mtpCacheUninitialize(MtpCache **cache)
{
	MtpCache *c;
	MtpCacheData *data;
	MtpCacheData *next;

	if (cache == NULL || (c = *cache) == NULL) {
		MTP_ERR(MTP_TAG_CACH, 506, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpCacheLock(c);
	for (data = c->head; data != NULL; data = next) {
		next = data->next;
		mtpUtilFree(data);
	}
	mtpCacheUnlock(c);
	sceKernelDeleteMutex(c->mutex);
	mtpUtilFree(*cache);
	*cache = NULL;
	return 0;
}

int mtpCacheGetObjectProperty(MtpCache *cache, SceUInt32 uniqueId, int prop, void *buf,
	SceSize size)
{
	MtpLogicalStorage *storage;
	MtpCacheData *data;
	int found = 1;
	int ret = 0;

	if (cache == NULL || buf == NULL) {
		MTP_ERR(MTP_TAG_CACH, 553, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	storage = cache->storage;
	if (storage == NULL || storage->db == NULL) {
		MTP_ERR(MTP_TAG_CACH, 560, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpCacheLock(cache);
	if (cache->numEntries != 0) {
		ret = _cacheLookupAndGet(cache, uniqueId, prop, buf, size);
		if (ret < 0) {
			if (ret != SCE_MTP_ERROR_NOT_FOUND) {
				MTP_ERR(MTP_TAG_CACH, 580,
					"unable to look up object property in cache=0x%x\n", ret);
				mtpCacheUnlock(cache);
				return ret;
			}
			found = 0;
		}
	} else {
		found = 0;
	}
	mtpCacheUnlock(cache);
	if (found)
		return ret;
	data = (MtpCacheData *)mtpUtilMalloc(sizeof(*data));
	if (data == NULL) {
		ret = mtpDBGetObjectProperty(storage->db, uniqueId, prop, buf, size);
		if (ret >= 0)
			return ret;
		MTP_ERR(MTP_TAG_CACH, 599, "unable to get object property from DB=0x%x\n", ret);
		goto free;
	}
	mtpUtilMemset(data, 0, sizeof(*data));
	ret = mtpDBGetObjectProperty(storage->db, uniqueId, MTP_DB_QUERY_OBJECT_DATA,
		&data->object, sizeof(data->object));
	if (ret < 0) {
		if (ret != SCE_MTP_ERROR_NO_SUCH_OBJECT)
			MTP_ERR(MTP_TAG_CACH, 609, "unable to get object property from DB=0x%x\n",
				ret);
		goto free;
	}
	mtpCacheLock(cache);
	ret = _cacheAdd(cache, data);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CACH, 618, "unable to add cache=0x%x\n", ret);
		mtpCacheUnlock(cache);
		goto free;
	}
	ret = _cacheLookupAndGet(cache, uniqueId, prop, buf, size);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CACH, 622,
			"unable to lookup data in cache uniqueId(%08x)=0x%x\n", uniqueId, ret);
		mtpCacheUnlock(cache);
		goto free;
	}
	mtpCacheUnlock(cache);
	return ret;
free:
	if (data != NULL)
		mtpUtilFree(data);
	return ret;
}

int mtpCacheSetObjectProperty(MtpCache *cache, SceUInt32 uniqueId, int prop, const void *buf,
	SceSize size)
{
	MtpLogicalStorage *storage;
	MtpCacheData *data;
	int found = 1;
	int ret = 0;

	if (cache == NULL || buf == NULL) {
		MTP_ERR(MTP_TAG_CACH, 661, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	storage = cache->storage;
	if (storage == NULL || storage->db == NULL) {
		MTP_ERR(MTP_TAG_CACH, 668, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpCacheLock(cache);
	if (cache->numEntries != 0) {
		ret = _cacheLookupAndSet(cache, uniqueId, prop, buf);
		if (ret < 0) {
			if (ret != SCE_MTP_ERROR_NOT_FOUND) {
				MTP_ERR(MTP_TAG_CACH, 688,
					"unable to look up object property in cache=0x%x\n", ret);
				mtpCacheUnlock(cache);
				return ret;
			}
			found = 0;
		}
	} else {
		found = 0;
	}
	mtpCacheUnlock(cache);
	if (found)
		return ret;
	data = (MtpCacheData *)mtpUtilMalloc(sizeof(*data));
	if (data == NULL) {
		ret = mtpDBSetObjectProperty(storage->db, uniqueId, prop, buf, size);
		if (ret >= 0)
			return ret;
		MTP_ERR(MTP_TAG_CACH, 708, "unable to get object property from DB=0x%x\n", ret);
		goto free;
	}
	mtpUtilMemset(data, 0, sizeof(*data));
	ret = mtpDBGetObjectProperty(storage->db, uniqueId, MTP_DB_QUERY_OBJECT_DATA,
		&data->object, sizeof(data->object));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CACH, 716, "unable to get object property from DB=0x%x\n", ret);
		goto free;
	}
	mtpCacheLock(cache);
	ret = _cacheAdd(cache, data);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CACH, 724, "unable to add cache=0x%x\n", ret);
		mtpCacheUnlock(cache);
		goto free;
	}
	ret = _cacheLookupAndSet(cache, uniqueId, prop, buf);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CACH, 728, "unable to lookup data in cache=0x%x", ret);
		mtpCacheUnlock(cache);
		goto free;
	}
	mtpCacheUnlock(cache);
	return ret;
free:
	if (data != NULL)
		mtpUtilFree(data);
	return ret;
}

int mtpCacheInvalidate(MtpCache *cache, SceUInt32 uniqueId)
{
	MtpCacheData *data;
	MtpCacheData *prev;
	MtpCacheData *next;

	if (cache == NULL) {
		MTP_ERR(MTP_TAG_CACH, 765, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	for (data = cache->head; data != NULL; data = data->next) {
		if (data->object.basic.uid == uniqueId)
			break;
	}
	mtpCacheLock(cache);
	if (data != NULL) {
		prev = cache->head;
		if (data == prev) {
			cache->head = prev->next;
			mtpUtilFree(prev);
		} else if (prev != NULL) {
			for (;;) {
				next = prev->next;
				if (data == next)
					break;
				prev = next;
				if (next == NULL)
					goto out;
			}
			prev->next = data->next;
			mtpUtilFree(data);
		}
out:
		if (prev != NULL)
			cache->numEntries--;
		else
			MTP_ERR(MTP_TAG_CACH, 804, "not deleted ?");
	}
	mtpCacheUnlock(cache);
	return 0;
}

int mtpCacheWriteBackDirtyCache(MtpCache *cache)
{
	MtpCacheData *data;
	int ret = 0;

	if (cache == NULL || cache->storage == NULL) {
		MTP_ERR(MTP_TAG_CACH, 832, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpCacheLock(cache);
	for (data = cache->head; data != NULL; data = data->next) {
		if (data->dirty != 1)
			continue;
		ret = mtpDBSetObjectProperty(cache->storage->db, data->object.basic.uid,
			MTP_DB_QUERY_OBJECT_DATA, &data->object, sizeof(data->object));
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CACH, 853, "unable to set data to DB=0x%x\n", ret);
			break;
		}
		data->dirty = 0;
	}
	mtpCacheUnlock(cache);
	return ret;
}

} /* extern "C" */
