/*
 * reg_key.c: the reg library's keys, in the open categories. All names
 * unknown.
 */

#include <psp2kern/kernel/threadmgr.h>
#include <stddef.h>
#include "reg_internal.h"

/* copy the key src of old to dst in data, its data records packed */
static inline __attribute__((always_inline)) int regMoveKey(RegDataBlock *data, RegKeyRecord *dst,
				      const RegKeyRecord *src, const RegDataBlock *old)
{
	SceUInt32 count;
	SceUInt16 first;
	SceUInt16 top;
	int record;
	int j;

	*dst = *src;
	switch (src->type) {
	case REG_TYPE_INT:
		dst->value = src->value;
		/* fall through */
	case REG_TYPE_CATEGORY:
		record = data->recordNum;
		data->recordBitmap[record >> 3] |= 1 << (7 - (record & 7));
		data->recordNum = record + 1;
		data->freeRecordNum--;
		break;
	case REG_TYPE_STR:
	case REG_TYPE_BIN:
		count = REG_KEY_RECORDS(src->value);
		if (data->freeRecordNum < count + 1)
			return REG_ERROR_NO_RECORDS;

		top = data->dataRecord;
		first = 0;
		for (j = 0; j < count; j++) {
			record = top - j;
			data->recordBitmap[record / 8] |= 1 << (7 - record % 8);
		}
		if (count != 0)
			first = top + 1 - count;
		data->dataRecord = top - count;

		dst->value = (src->value & 0xffffff) | first << 24;
		memcpy((SceUInt8 *)data + first * sizeof(RegKeyRecord),
		       (const SceUInt8 *)old + REG_KEY_OFFSET(src->value) * sizeof(RegKeyRecord),
		       count * sizeof(RegKeyRecord));

		record = data->recordNum;
		data->recordBitmap[record >> 3] |= 1 << (7 - (record & 7));
		data->freeRecordNum -= count + 1;
		data->recordNum = record + 1;
		break;
	}

	return 0;
}

/* lay a category's keys out again in *blockNum blocks, its data records packed */
int regResizeCategoryCache(SceUInt16 *blockNum, RegCategoryCache **cache, RegDataBlock *old)
{
	RegDataBlock *data;
	SceUInt16 n;
	int i;

	n = *blockNum;
	data = regMemAlloc(n * REG_BLOCK_SIZE);
	if (data == NULL)
		return REG_ERROR_NO_MEMORY_HEAP;

	memset(data, 0, n * REG_BLOCK_SIZE);
	memcpy(data, old, offsetof(RegDataBlock, recordBitmap) + 8);
	data->keyNum = 0;
	memset(data->recordBitmap + 1, 0, 7);
	data->blockNum = n;
	data->recordMax = n * 16 - 1;
	data->freeRecordNum = n * 16 - 1;
	data->dataRecord = n * 16 - 1;
	data->recordNum = 1;
	data->recordBitmap[0] = 0x80;

	for (i = 0; i < old->keyNum; i++) {
		if (regMoveKey(data, &data->key[i], &old->key[i], old) != 0) {
			memset(data, 0, *blockNum * REG_BLOCK_SIZE);
			regMemFree(data);
			return REG_ERROR_NO_RECORDS;
		}
		data->keyNum++;
	}

	memset((*cache)->data, 0, old->blockNum * REG_BLOCK_SIZE);
	regMemFree((*cache)->data);
	(*cache)->data = data;

	return 0;
}

/* create a key in an open category */
int regCreateKey(int handle, const char *name, int type, int size)
{
	RegCategoryEntry *entry;
	RegDataBlock *data;
	RegKeyRecord *key;
	SceUInt32 count;
	SceUInt16 first;
	SceUInt16 top;
	int record;
	int state;
	int free;
	int ret;
	int i;

	state = regPlatformLock();
	if (name == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_NO_NAME;
	}
	if (regPlatformCheckPointer(name) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}
	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	entry = (RegCategoryEntry *)handle;
	if (regFindCategory(handle) == 0) {
		ret = REG_ERROR_CATEGORY_NOT_OPEN;
		goto unlock;
	}
	if (strnlen(name, REG_CATEGORY_NAME_LEN + 1) > REG_CATEGORY_NAME_LEN) {
		ret = REG_ERROR_NAME_TOO_LONG;
		goto unlock;
	}

	data = entry->cache->data;
	for (i = 0; i < data->keyNum; i++) {
		key = &data->key[i];
		if (key->name[0] == name[0] && strncmp(key->name, name, REG_CATEGORY_NAME_LEN) == 0) {
			ret = REG_ERROR_KEY_EXISTS;
			goto unlock;
		}
	}
	if (entry->blockNum == REG_CATEGORY_BLOCK_MAX - 1 && data->freeRecordNum == 0) {
		ret = REG_ERROR_CATEGORY_FULL;
		goto unlock;
	}

	if (type == REG_TYPE_INT)
		ret = regReserveCategorySpace(entry, size);
	else if (type > REG_TYPE_INT && type <= REG_TYPE_BIN)
		ret = regReserveCategorySpace(entry, size + sizeof(RegKeyRecord));
	else {
		ret = REG_ERROR_BAD_KEY_TYPE;
		goto unlock;
	}
	if (ret == 0) {
		ret = regResizeCategoryCache(&entry->blockNum, &entry->cache, data);
		if (ret != 0)
			goto unlock;
		data = entry->cache->data;
	} else if (ret != REG_ERROR_NO_SPACE) {
		goto unlock;
	}

	key = &data->key[data->recordNum - 1];
	if (type == REG_TYPE_INT) {
		memset(key, 0, sizeof(*key));
		key->type = REG_TYPE_INT;
		memcpy(key->name, name, strnlen(name, REG_CATEGORY_NAME_LEN));
		record = data->recordNum;
		data->recordBitmap[record >> 3] |= 1 << (7 - (record & 7));
		if (--data->freeRecordNum != 0)
			data->recordNum = record + 1;
	} else {
		count = (size + 31) / 32;
		if (count + 1 > data->freeRecordNum) {
			ret = REG_ERROR_NO_RECORDS;
			goto unlock;
		}

		top = data->dataRecord;
		first = 0;
		for (i = 0; i < count; i++) {
			record = top - i;
			data->recordBitmap[record / 8] |= 1 << (7 - record % 8);
		}
		if (count != 0)
			first = top + 1 - count;
		data->dataRecord = top - count;

		memset(key, 0, sizeof(*key));
		key->type = type;
		memcpy(key->name, name, strnlen(name, REG_CATEGORY_NAME_LEN));
		key->value = (size & 0xffff) | (SceInt8)count << 16 | first << 24;
		free = data->freeRecordNum - 1 - count;
		record = data->recordNum;
		data->recordBitmap[record >> 3] |= 1 << (7 - (record & 7));
		data->freeRecordNum = free;
		if ((SceUInt16)free != 0)
			data->recordNum = record + 1;
	}
	data->keyNum++;
	entry->keyNum++;
	regUpdateCategoryCheck(handle);
	ret = 0;

unlock:
	if (sceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* write a key of an open category */
int regSetKey(int handle, const char *name, const void *buf, SceSize size)
{
	RegCategoryEntry *entry;
	RegDataBlock *data;
	RegKeyRecord *key;
	SceUInt8 *dest;
	int state;
	int ret;
	int i;

	state = regPlatformLock();
	if (regPlatformCheckPointer(name) == NULL || regPlatformCheckBuffer(buf, size) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}
	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	entry = (RegCategoryEntry *)handle;
	if (regFindCategory(handle) == 0) {
		ret = REG_ERROR_CATEGORY_NOT_OPEN;
		goto unlock;
	}

	data = entry->cache->data;
	ret = REG_ERROR_KEY_NOT_FOUND;
	for (i = 0; i < data->recordNum; i++) {
		key = &data->key[i];
		if (key->name[0] != name[0] || strncmp(key->name, name, REG_CATEGORY_NAME_LEN) != 0)
			continue;

		dest = NULL;
		if (key->type == REG_TYPE_STR || key->type == REG_TYPE_BIN)
			dest = (SceUInt8 *)data + REG_KEY_OFFSET(key->value) * sizeof(RegKeyRecord);
		if (key->type == REG_TYPE_INT) {
			key->value = *(const SceUInt32 *)buf;
		} else {
			if (key->type < REG_TYPE_INT || key->type > REG_TYPE_BIN) {
				ret = REG_ERROR_BAD_KEY_TYPE;
				break;
			}
			if ((int)(REG_KEY_RECORDS(key->value) * sizeof(RegKeyRecord)) < (int)size) {
				ret = REG_ERROR_VALUE_TOO_BIG;
				break;
			}
			key->value = (size & 0xffff) | (key->value & 0xffff0000);
			memcpy(dest, buf, size);
		}
		ret = 0;
		regUpdateCategoryCheck(handle);
		break;
	}

unlock:
	if (sceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* the type and size of a key of an open category */
int regGetKeyInfo(int handle, const char *name, int *type, int *size)
{
	RegCategoryEntry *entry;
	RegDataBlock *data;
	RegKeyRecord *key;
	int state;
	int ret;
	int i;

	state = regPlatformLock();
	if (regPlatformCheckPointer(name) == NULL || regPlatformCheckPointer(type) == NULL ||
	    regPlatformCheckPointer(size) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}

	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	entry = (RegCategoryEntry *)handle;
	if (name == NULL) {
		ret = REG_ERROR_NO_NAME;
	} else if (regFindCategory(handle) == 0) {
		ret = REG_ERROR_CATEGORY_NOT_OPEN;
	} else {
		data = entry->cache->data;
		ret = REG_ERROR_KEY_NOT_FOUND;
		for (i = 0; i < data->recordNum; i++) {
			key = &data->key[i];
			if (key->name[0] != name[0] ||
			    strncmp(key->name, name, REG_CATEGORY_NAME_LEN) != 0)
				continue;

			*type = key->type;
			switch (key->type) {
			case REG_TYPE_CATEGORY:
				*size = 0;
				ret = 0;
				goto unlock;
			case REG_TYPE_INT:
				*size = sizeof(int);
				ret = 0;
				goto unlock;
			case REG_TYPE_STR:
			case REG_TYPE_BIN:
				*size = REG_KEY_SIZE(key->value);
				ret = 0;
				goto unlock;
			default:
				ret = REG_ERROR_BAD_KEY_TYPE;
				break;
			}
			break;
		}
	}
	*type = 0;
	*size = 0;

unlock:
	if (sceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* read a key of an open category */
int regGetKey(int handle, const char *name, void *buf, SceSize size)
{
	RegCategoryEntry *entry;
	RegDataBlock *data;
	RegKeyRecord *key;
	int state;
	int ret;
	int i;

	state = regPlatformLock();
	if (regPlatformCheckPointer(name) == NULL || regPlatformCheckBuffer(buf, size) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}

	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	entry = (RegCategoryEntry *)handle;
	if (name == NULL) {
		ret = REG_ERROR_NO_NAME;
		goto unlock;
	}
	if (regFindCategory(handle) == 0) {
		ret = REG_ERROR_CATEGORY_NOT_OPEN;
		goto unlock;
	}

	data = entry->cache->data;
	ret = REG_ERROR_KEY_NOT_FOUND;
	for (i = 0; i < data->recordNum; i++) {
		key = &data->key[i];
		if (key->name[0] != name[0] || strncmp(key->name, name, REG_CATEGORY_NAME_LEN) != 0)
			continue;

		if (key->type < REG_TYPE_CATEGORY || key->type > REG_TYPE_BIN) {
			ret = REG_ERROR_BAD_KEY_TYPE;
			break;
		}
		if (regFindCategory(handle) == 0) {
			ret = REG_ERROR_CATEGORY_NOT_OPEN;
			break;
		}

		switch (key->type) {
		case REG_TYPE_CATEGORY:
			ret = REG_ERROR_KEY_IS_CATEGORY;
			break;
		case REG_TYPE_INT:
			if (size < sizeof(int)) {
				ret = REG_ERROR_KEY_BUFFER_SIZE;
				break;
			}
			*(SceUInt32 *)buf = key->value;
			ret = 0;
			break;
		case REG_TYPE_STR:
		case REG_TYPE_BIN:
			if (REG_KEY_SIZE(key->value) > size) {
				ret = REG_ERROR_KEY_BUFFER_SIZE;
				break;
			}
			memcpy(buf, (SceUInt8 *)entry->cache->data + REG_KEY_OFFSET(key->value) * 0x20,
			       REG_KEY_SIZE(key->value));
			ret = 0;
			break;
		}
		break;
	}

unlock:
	if (sceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* delete a key of an open category: the other keys laid out again without it */
int regDeleteKeyUnlocked(int handle, const char *name)
{
	RegCategoryEntry *entry;
	RegKeyRecord *victim;
	RegKeyRecord *dst;
	RegDataBlock *data;
	RegDataBlock *old;
	int i;

	if (name == NULL)
		return REG_ERROR_NO_NAME;
	if (regFindCategory(handle) == 0)
		return REG_ERROR_CATEGORY_NOT_OPEN;

	entry = (RegCategoryEntry *)handle;
	old = entry->cache->data;
	victim = NULL;
	for (i = 0; i < old->recordNum; i++) {
		if (old->key[i].name[0] == name[0] &&
		    strncmp(old->key[i].name, name, REG_CATEGORY_NAME_LEN) == 0) {
			victim = &old->key[i];
			break;
		}
	}
	if (victim == NULL)
		return REG_ERROR_KEY_NOT_FOUND;

	data = regMemAlloc(old->blockNum * REG_BLOCK_SIZE);
	if (data == NULL)
		return REG_ERROR_NO_MEMORY_HEAP;

	memset(data, 0, old->blockNum * REG_BLOCK_SIZE);
	if ((old->recordMax & 0xff) == (old->blockNum & 0xf) * 16)
		old->recordMax--;
	memcpy(data, old, offsetof(RegDataBlock, recordBitmap) + 8);
	data->keyNum = 0;
	memset(data->recordBitmap + 1, 0, 7);
	data->recordBitmap[0] = 0x80;
	data->freeRecordNum = old->recordMax;
	data->dataRecord = old->recordMax;
	data->recordNum = 1;

	dst = data->key;
	for (i = 0; i < old->keyNum; i++) {
		if (&old->key[i] == victim)
			continue;
		if (regMoveKey(data, dst, &old->key[i], old) != 0) {
			memset(data, 0, old->blockNum * REG_BLOCK_SIZE);
			regMemFree(data);
			return REG_ERROR_NO_RECORDS;
		}
		dst++;
		data->keyNum++;
	}

	memcpy(old, data, old->blockNum * REG_BLOCK_SIZE);
	memset(data, 0, old->blockNum * REG_BLOCK_SIZE);
	regMemFree(data);
	entry->keyNum = old->keyNum;
	regUpdateCategoryCheck(handle);

	return 0;
}

/* delete a key of an open category */
int regDeleteKey(int handle, const char *name)
{
	int state;
	int ret;

	state = regPlatformLock();
	if (regPlatformCheckPointer(name) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}

	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	ret = regDeleteKeyUnlocked(handle, name);
	if (sceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* the number of keys of an open category */
int regGetKeyNum(int handle, int *num)
{
	RegCategoryEntry *entry;
	int state;
	int ret;

	state = regPlatformLock();
	if (regPlatformCheckPointer(num) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}

	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	entry = (RegCategoryEntry *)handle;
	if (regFindCategory(handle) == 0) {
		ret = REG_ERROR_CATEGORY_NOT_OPEN;
	} else if (entry->cache == NULL && entry->cache->data == NULL) {
		/* as the original: only a missing cache is caught */
		ret = REG_ERROR_NO_DATA;
	} else {
		ret = 0;
		*num = entry->cache->data->keyNum;
	}

	if (sceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* the names of the first num keys of an open category */
int regGetKeyNames(int handle, char (*names)[REG_CATEGORY_NAME_LEN + 1], int num)
{
	RegCategoryEntry *entry;
	RegKeyRecord *key;
	int state;
	int ret;
	int i;

	state = regPlatformLock();
	if (regPlatformCheckBuffer(names, num * (REG_CATEGORY_NAME_LEN + 1)) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}

	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	entry = (RegCategoryEntry *)handle;
	if (regFindCategory(handle) == 0) {
		ret = REG_ERROR_CATEGORY_NOT_OPEN;
	} else {
		key = entry->cache->data->key;
		for (i = 0; i < num; i++, key++) {
			memset(names[i], 0, REG_CATEGORY_NAME_LEN + 1);
			memcpy(names[i], key->name, REG_CATEGORY_NAME_LEN);
		}
		ret = 0;
	}

	if (sceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* add a category key name to a category */
int regAddCategoryKey(RegCategoryEntry *entry, const char *name)
{
	RegDataBlock *data;
	RegKeyRecord *key;
	SceUInt16 n;
	int ret;
	int i;

	data = entry->cache->data;
	for (i = 0; i < data->keyNum; i++) {
		key = &data->key[i];
		if (key->name[0] == name[0] && strncmp(key->name, name, REG_CATEGORY_NAME_LEN) == 0)
			return REG_ERROR_KEY_EXISTS;
	}

	if (data->freeRecordNum == 0) {
		ret = regReserveCategorySpace(entry, sizeof(RegKeyRecord));
		if (ret == 0) {
			ret = regResizeCategoryCache(&entry->blockNum, &entry->cache, data);
			if (ret != 0)
				return ret;
			data = entry->cache->data;
		} else if (ret != REG_ERROR_NO_SPACE) {
			return ret;
		}
	}

	key = &data->key[data->recordNum - 1];
	memset(key, 0, sizeof(*key));
	key->type = REG_TYPE_CATEGORY;
	memcpy(key->name, name, REG_CATEGORY_NAME_LEN);
	n = data->recordNum;
	data->recordBitmap[n >> 3] |= 1 << (7 - (n & 7));
	if (--data->freeRecordNum != 0)
		data->recordNum = n + 1;
	data->keyNum++;
	entry->keyNum++;

	return 0;
}
