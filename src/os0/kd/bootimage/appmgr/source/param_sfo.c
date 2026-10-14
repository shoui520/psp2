/* Parameter-file loading, serialization and parsed-context ownership. */

#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>
#include <string.h>
#include "param_sfo.h"

extern void *appmgrAllocMountHeapMemory(SceSize size);
extern int appmgrFreeMountHeapMemory(void *memory);
extern int appmgrPsfParse(const void *buffer, SceSize size, void **psf);
extern int appmgrPsfDestroy(void *psf);
extern int appmgrPsfCreate(void **psf);
extern int appmgrPsfSetEntry(void *psf, const char *key, const void *value,
			   SceUInt16 format, SceSize length, SceSize capacity);
extern int appmgrPsfGetSize(void *psf, SceSize *size);
extern int appmgrPsfWrite(void *buffer, SceSize capacity, void *psf);
extern int appmgrPsfFindEntry(void *psf, const char *key, void **entry);
extern int appmgrPsfGetEntryData(void *psf, void *entry, void **data);
extern int appmgrPsfGetEntryInfo(void *psf, void *entry, const char **key,
			       SceSize *length, SceSize *capacity);
extern int appmgrPsfSetEntryLength(void *psf, void *entry, SceSize length);

static const char emptySfoString[4] = "";
static const char savedataCategory[4] = "sd";

int appmgrBuildSfoFile(AppmgrSfoFile *file, const AppmgrSfoEntry *entries,
		      int serialize)
{
	void *psf = NULL;
	SceSize size = 0;
	const AppmgrSfoEntry *next;
	int result;

	result = appmgrPsfCreate(&psf);
	if (result < 0)
		goto fail;
	next = entries + 1;
	if (entries->key[0] != 0) {
		do {
			result = appmgrPsfSetEntry(psf, (const char *)entries->key,
				entries->value, entries->format, entries->length,
				entries->capacity);
			if (result != 0)
				goto fail;
			__builtin_prefetch(next + 7);
			entries = next++;
		} while (entries->key[0] != 0);
	}
	result = appmgrPsfGetSize(psf, &size);
	if (result != 0)
		goto fail;
	file->size = size;
	if (serialize == 0) {
		appmgrPsfDestroy(psf);
		return 0;
	}
	file->buffer = appmgrAllocMountHeapMemory((size + 15) & ~15U);
	if (file->buffer == NULL) {
		result = (int)0x80800032;
		goto fail;
	}
	result = appmgrPsfWrite(file->buffer, (file->size + 15) & ~15U, psf);
	if (result != 0)
		goto fail;
	appmgrPsfDestroy(psf);
	file->psf = NULL;
	return appmgrPsfParse(file->buffer, file->size, &file->psf);
fail:
	appmgrPsfDestroy(psf);
	return result;
}

int appmgrGetSfoEntry(AppmgrSfoFile *file, const char *key, void **data,
		      SceSize *length, SceSize *capacity)
{
	void *entry;
	const char *entryKey;
	int result;

	result = appmgrPsfFindEntry(file->psf, key, &entry);
	if (result == 0) {
		result = appmgrPsfGetEntryData(file->psf, entry, data);
		if (result == 0)
			result = appmgrPsfGetEntryInfo(file->psf, entry,
				&entryKey, length, capacity);
	}
	return result;
}

int appmgrLoadSfoFile(const char *path, AppmgrSfoFile *file)
{
	SceIoStat stat;
	SceUID fd = 0;
	int result, length;

	__builtin_memset(file, 0, sizeof(*file));
	file->buffer = NULL;
	result = ksceIoGetstat(path, &stat);
	if (result < 0)
		goto fail;
	if (stat.st_size > 0x8000) {
		result = (int)0x80800032;
		goto fail;
	}
	fd = ksceIoOpen(path, 0x10001, 0);
	if (fd < 0) {
		result = fd;
		goto fail;
	}
	file->buffer = appmgrAllocMountHeapMemory(stat.st_size);
	if (file->buffer == NULL) {
		result = (int)0x80800032;
		goto fail;
	}
	result = ksceIoRead(fd, file->buffer, stat.st_size);
	if (result < 0)
		goto fail;
	if ((SceOff)result != stat.st_size) {
		result = (int)0x80800033;
		goto fail;
	}
	length = result;
	result = ksceIoClose(fd);
	if (result != 0)
		goto fail;
	file->size = length;
	return appmgrPsfParse(file->buffer, file->size, &file->psf);
fail:
	if (file->buffer != NULL) {
		appmgrFreeMountHeapMemory(file->buffer);
		file->buffer = NULL;
	}
	if (fd > 0)
		ksceIoClose(fd);
	return result;
}

int appmgrReleaseSfoFile(AppmgrSfoFile *file)
{
	if (file->buffer != NULL) {
		appmgrFreeMountHeapMemory(file->buffer);
		file->buffer = NULL;
	}
	if (file->psf != NULL) {
		appmgrPsfDestroy(file->psf);
		file->psf = NULL;
	}
	file->size = 0;
	return 0;
}

int appmgrParseSfoVersion(const signed char *version)
{
	int second = version[1];

	if (second == '.')
		return 10 * (version[2] - '0') +
			100 * (version[0] - '0') + version[3] - '0';
	if (version[2] == '.')
		return version[4] - '0' + 100 * (second - '0') +
			1000 * (version[0] - '0') + 10 * (version[3] - '0');
	return 0;
}

int appmgrSetSfoAccountId(AppmgrSfoFile *file, SceUInt64 accountId)
{
	AppmgrSfoEntry value;
	void *entry, *data;
	const char *key;
	SceSize length, capacity;
	int result;

	__builtin_memcpy(value.key, "ACCOUNT_ID", 11);
	memset(value.key + 11, 0, 21);
	value.value = &accountId;
	value.format = 4;
	value.reserved = 0;
	value.length = 8;
	value.capacity = 8;
	result = appmgrPsfFindEntry(file->psf, (const char *)value.key, &entry);
	if (result != 0)
		return result;
	result = appmgrPsfGetEntryData(file->psf, entry, &data);
	if (result != 0)
		return result;
	result = appmgrPsfGetEntryInfo(file->psf, entry, &key, &length, &capacity);
	if (result != 0)
		return result;
	if (value.format == 0x0404) {
		*(SceInt32 *)data = *(const signed char *)value.value;
		return 0;
	}
	if (value.length > value.capacity)
		return (int)0x80800031;
	memset(data, 0, value.capacity);
	memcpy(data, value.value, value.length);
	result = appmgrPsfFindEntry(file->psf, (const char *)value.key, &entry);
	if (result == 0)
		result = appmgrPsfSetEntryLength(file->psf, entry, value.length);
	return result;
}

int appmgrSetSfoParams(AppmgrSfoFile *file, const void *params)
{
	AppmgrSfoEntry value;
	void *entry, *data;
	const char *key;
	SceSize length, capacity;
	int result;

	__builtin_memcpy(value.key, "PARAMS", 7);
	memset(value.key + 7, 0, 25);
	value.value = params;
	value.format = 4;
	value.reserved = 0;
	value.length = 1024;
	value.capacity = 1024;
	result = appmgrPsfFindEntry(file->psf, (const char *)value.key, &entry);
	if (result != 0)
		return result;
	result = appmgrPsfGetEntryData(file->psf, entry, &data);
	if (result != 0)
		return result;
	result = appmgrPsfGetEntryInfo(file->psf, entry, &key, &length, &capacity);
	if (result != 0)
		return result;
	if (value.format == 0x0404) {
		*(SceInt32 *)data = *(const signed char *)value.value;
		return 0;
	}
	if (value.length > value.capacity)
		return (int)0x80800031;
	memset(data, 0, value.capacity);
	memcpy(data, value.value, value.length);
	result = appmgrPsfFindEntry(file->psf, (const char *)value.key, &entry);
	if (result == 0)
		result = appmgrPsfSetEntryLength(file->psf, entry, value.length);
	return result;
}

int appmgrBuildSavedataSfo(AppmgrSfoFile *file,
			 const AppmgrSavedataSfoParam *param)
{
	const char *parent = param->parentDirectory;
	const char *title = param->title;
	const char *detail = param->detail;
	SceUInt32 attribute = 0;
	SceUInt64 accountId = 0;
	AppmgrSfoEntry entries[9];
	char version[8];
	SceSize length;
	int result;

	if (parent == NULL)
		parent = emptySfoString;
	if (param->attribute != NULL)
		attribute = *param->attribute;
	if (title == NULL)
		title = emptySfoString;
	if (detail == NULL)
		detail = emptySfoString;
	if (param->accountId != NULL)
		accountId = *param->accountId;
	if (param->params == NULL)
		return (int)0x80800031;
	version[0] = '0';
	version[1] = '.';
	version[2] = '0';
	version[3] = '0';
	version[4] = 0;
	memset(entries, 0, sizeof(entries));
	__builtin_memcpy(entries[0].key, "PARENT_DIRECTORY", 17);
	memset(entries[0].key + 17, 0, 15);
	entries[0].value = parent;
	entries[0].format = 0x0204;
	length = strnlen(parent, 64);
	if (length == 0)
		length = 1;
	else if (length < 64)
		++length;
	entries[0].length = length;
	entries[0].capacity = 64;
	__builtin_memcpy(entries[1].key, "CATEGORY", 9);
	memset(entries[1].key + 9, 0, 23);
	entries[1].value = savedataCategory;
	entries[1].format = 0x0204;
	length = strnlen(savedataCategory, 4);
	if (length == 0)
		length = 1;
	else if (length < 4)
		++length;
	entries[1].length = length;
	entries[1].capacity = 4;
	__builtin_memcpy(entries[2].key, "TITLE", 6);
	__builtin_memset(entries[2].key + 6, 0, 26);
	entries[2].value = title;
	entries[2].format = 0x0204;
	length = strnlen(title, 128);
	if (length == 0)
		length = 1;
	else if (length < 128)
		++length;
	entries[2].length = length;
	entries[2].capacity = 128;
	__builtin_memcpy(entries[3].key, "DETAIL", 7);
	memset(entries[3].key + 7, 0, 25);
	entries[3].value = detail;
	entries[3].format = 0x0204;
	length = strnlen(detail, 1024);
	if (length == 0)
		length = 1;
	else if (length < 1024)
		++length;
	entries[3].length = length;
	entries[3].capacity = 1024;
	__builtin_memcpy(entries[4].key, "ATTRIBUTE", 10);
	__builtin_memset(entries[4].key + 10, 0, 22);
	entries[4].value = &attribute;
	entries[4].format = 0x0404;
	entries[4].length = 4;
	entries[4].capacity = 4;
	__builtin_memcpy(entries[5].key, "SAVEDATA_VER", 13);
	memset(entries[5].key + 13, 0, 19);
	entries[5].value = version;
	entries[5].format = 0x0204;
	entries[5].length = 6;
	entries[5].capacity = 8;
	__builtin_memcpy(entries[6].key, "ACCOUNT_ID", 11);
	memset(entries[6].key + 11, 0, 21);
	entries[6].value = &accountId;
	entries[6].format = 4;
	entries[6].length = 8;
	entries[6].capacity = 8;
	__builtin_memcpy(entries[7].key, "PARAMS", 7);
	memset(entries[7].key + 7, 0, 25);
	entries[7].value = param->params;
	entries[7].format = 4;
	entries[7].length = 1024;
	entries[7].capacity = 1024;
	result = appmgrBuildSfoFile(file, entries, 1);
	return result < 0 ? result : 0;
}
