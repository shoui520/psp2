/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"
#include "path.h"
#include <kernel/error.h>
#include <psp2kern/io/stat.h>

unsigned int MarlinStringLength(const char *string);
char *MarlinDuplicateString(void *allocator, const char *string);
signed char *MarlinStringFindCharacter(signed char *string, int character);
signed char MarlinGetPathSeparator(void);

int MarlinEnsureDirectory(const char *path)
{
	int index;
	int length;
	char devicePath[1024];
	SceIoStat status;

	length = MarlinStringLength(path);
	if (length > 1018)
		return -1;
	for (index = 0; index < length; index++) {
		if (((const signed char *)path)[index] == ':')
			break;
	}
	if (index == length) {
		MarlinCopyMemory(devicePath, "host0:", 6);
		MarlinCopyMemory(devicePath + 6, path, length + 1);
	} else {
		MarlinCopyMemory(devicePath, path, length + 1);
	}
	if (ksceIoGetstat(devicePath, &status) == 0) {
		if ((((int)status.st_mode >> 12) & 1) == 0)
			return -1;
	} else {
		index = ksceIoMkdir(devicePath, 0606);
		if (index < 0)
			return -1;
	}
	return 0;
}

int MarlinStringListPrepend(void *allocator, const char *string,
			    MarlinStringList **list)
{
	int result = 0;
	MarlinStringList *node = 0;

	if (string == 0 || list == 0) {
		result = 3;
		goto out;
	}
	node = MarlinAllocateMemory(allocator, sizeof(*node));
	if (node == 0) {
		result = 2;
		goto out;
	}
	MarlinFillMemory(node, 0, sizeof(*node));
	node->string = MarlinDuplicateString(allocator, string);
	if (node->string == 0) {
		result = 2;
		goto out;
	}
	node->next = *list;
	*list = node;
	node = 0;
out:
	if (node != 0) {
		MarlinFreeMemory(allocator, node->string);
		MarlinFreeMemory(allocator, node);
	}
	return result;
}

int MarlinCreateDirectories(void *allocator, const char *path)
{
	int result;
	char *copy = 0;
	signed char *cursor;
	unsigned int length;

	if (path == 0 || *path == 0) {
		result = 3;
		goto out;
	}
	length = MarlinStringLength(path);
	cursor = MarlinStringFindCharacter((signed char *)path, ':');
	cursor = cursor != 0 ? cursor + 1 : (signed char *)path;
	cursor = MarlinStringFindCharacter(
		*cursor == MarlinGetPathSeparator() ? cursor + 1 : cursor,
		MarlinGetPathSeparator());
	if (cursor != 0) {
		copy = MarlinAllocateMemory(allocator, length + 1);
		if (copy == 0) {
			result = 2;
			goto out;
		}
		MarlinCopyMemory(copy, path, length + 1);
		cursor = (signed char *)copy + (cursor - (const signed char *)path);
		do {
			*cursor = 0;
			if (MarlinEnsureDirectory(copy) != 0) {
				result = 0x11060001;
				goto out;
			}
			*cursor = MarlinGetPathSeparator();
			cursor = MarlinStringFindCharacter(cursor + 1,
						   MarlinGetPathSeparator());
		} while (cursor != 0);
	}
	if (MarlinEnsureDirectory(path) != 0)
		result = 0x11060001;
	else
		result = 0;
out:
	if (copy != 0)
		MarlinFreeMemory(allocator, copy);
	return result;
}

int MarlinGetStorageRoot(void *allocator, unsigned int *size, char *path)
{
	int result;
	unsigned int suffixSize = 0;
	const signed char *root = (const signed char *)"tm0:nphome";
	unsigned int rootSize;
	char suffix[32];

	rootSize = MarlinStringLength((const char *)root);
	if (root[rootSize - 1] != '/') {
		suffix[suffixSize] = '/';
		suffixSize++;
	}
	suffix[suffixSize] = 0;
	suffixSize++;
	if (*size < rootSize + suffixSize) {
		*size = rootSize + suffixSize;
		result = 5;
		goto out;
	}
	MarlinCopyMemory(path, root, rootSize);
	MarlinCopyMemory(path + rootSize, suffix, suffixSize);
	*size = rootSize + suffixSize;
	result = 0;
out:
	return result;
}

int MarlinGetPathInfo(void *allocator, const char *path, MarlinPathInfo *info)
{
	int result;
	int status;
	SceIoStat stat;

	if (path == 0 || info == 0) {
		result = 3;
		goto out;
	}
	status = ksceIoGetstat(path, &stat);
	if (status < 0) {
		if (status == SCE_ERROR_ERRNO_ENOENT ||
		    status == SCE_ERROR_ERRNO_ENOTDIR)
			result = 0x11060002;
		else
			result = 0x11060007;
		goto out;
	}
	if (SCE_S_ISREG(stat.st_mode)) {
		info->isFile = 1;
	} else if (SCE_S_ISDIR(stat.st_mode)) {
		info->isFile = 0;
	} else {
		result = 0x11060007;
		goto out;
	}
	info->size = (unsigned int)stat.st_size;
	result = 0;
out:
	return result;
}

signed char MarlinGetPathSeparator(void)
{
	return (signed char)'/';
}
