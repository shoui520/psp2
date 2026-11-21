/*
 * Directories: the path separator, a tree removed, a directory's files listed. Its own
 * file: the file layer calls the separator out of line. The file's name is ours.
 */
#include <kernel/iofilemgr.h>
#include "mln_internal.h"

/* the platform's path separator */
signed char MlnImpl_81026430(void)
{
	return '/';
}

#define MLN_DIR_PATH_MAX	256
#define MLN_DIR_SEPARATOR	"/"

/* what the last entry whose status could be read was */
#define MLN_DIR_ENTRY_DIRECTORY	0
#define MLN_DIR_ENTRY_FILE	1

/*
 * Remove the directory and everything in it; 1 on failure. The entries are read
 * through a NULL buffer and their names taken from where that buffer's would be, as
 * Sony's code does.
 */
int MlnImpl_81026438(const char *directory)
{
	SceIoStat stat;
	char path[MLN_DIR_PATH_MAX];
	SceIoDirent *entry = NULL;
	SceUID fd;
	int length, type;

	length = MlnStrlen(directory);
	if (length > MLN_DIR_PATH_MAX)
		return 1;
	MlnMemcpy(path, directory, length);
	if (((const signed char *)directory)[length - 1] != '/') {
		if (length + 1 + MlnStrlen(MLN_DIR_SEPARATOR) > MLN_DIR_PATH_MAX)
			return 1;
		MlnStrncpy(path + length, MLN_DIR_SEPARATOR, MlnStrlen(MLN_DIR_SEPARATOR) + 1);
		length++;
	}
	fd = sceIoDopen(directory);
	if (fd == 0)
		return 0;
	type = MLN_DIR_ENTRY_DIRECTORY;
	while (sceIoDread(fd, entry) > 0) {
		if (MlnStrcmp(entry->d_name, ".") == 0 || MlnStrcmp(entry->d_name, "..") == 0)
			continue;
		if (length + 1 + MlnStrlen(entry->d_name) > MLN_DIR_PATH_MAX)
			goto error;
		MlnStrncpy(path + length, entry->d_name, MlnStrlen(entry->d_name) + 1);
		if (sceIoGetstat(path, &stat) >= 0) {
			if (SCE_STM_ISREG(stat.st_mode))
				type = MLN_DIR_ENTRY_FILE;
			else if (SCE_STM_ISDIR(stat.st_mode))
				type = MLN_DIR_ENTRY_DIRECTORY;
		}
		if (type == MLN_DIR_ENTRY_FILE) {
			if (sceIoRemove(path) != 0)
				goto error;
		} else if (type == MLN_DIR_ENTRY_DIRECTORY) {
			if (MlnImpl_81026438(path) != 0)
				goto error;
		}
	}
	sceIoDclose(fd);
	return sceIoRmdir(directory) != 0;

error:
	sceIoDclose(fd);
	return 1;
}

/* list the files of a directory (not its directories), newest entry first */
int MlnImpl_810265E8(int heap, const char *directory, MlnFileList **files)
{
	SceIoStat stat;
	char path[MLN_DIR_PATH_MAX];
	SceIoDirent *entry = NULL;
	MlnFileList *list = NULL, *node;
	SceUID fd;
	int length, type, ret;

	if (files == NULL || directory == NULL) {
		ret = 3;
		goto out;
	}
	length = MlnStrlen(directory);
	if (length > MLN_DIR_PATH_MAX) {
		ret = 1;
		goto out;
	}
	MlnMemcpy(path, directory, length);
	if (((const signed char *)directory)[length - 1] != '/') {
		if (length + 1 + MlnStrlen(MLN_DIR_SEPARATOR) > MLN_DIR_PATH_MAX) {
			ret = 1;
			goto out;
		}
		MlnStrncpy(path + length, MLN_DIR_SEPARATOR, MlnStrlen(MLN_DIR_SEPARATOR) + 1);
		length++;
	}
	fd = sceIoDopen(directory);
	if (fd == 0) {
		ret = MLN_FILE_ERROR_NOT_FOUND;
		goto out;
	}
	type = MLN_DIR_ENTRY_DIRECTORY;
	while (sceIoDread(fd, entry) > 0) {
		if (MlnStrcmp(entry->d_name, ".") == 0 || MlnStrcmp(entry->d_name, "..") == 0)
			continue;
		if (length + 1 + MlnStrlen(entry->d_name) > MLN_DIR_PATH_MAX) {
			ret = 1;
			goto close;
		}
		MlnStrncpy(path + length, entry->d_name, MlnStrlen(entry->d_name) + 1);
		if (sceIoGetstat(path, &stat) >= 0) {
			if (SCE_STM_ISREG(stat.st_mode))
				type = MLN_DIR_ENTRY_FILE;
			else if (SCE_STM_ISDIR(stat.st_mode))
				type = MLN_DIR_ENTRY_DIRECTORY;
		}
		if (type != MLN_DIR_ENTRY_FILE)
			continue;
		node = MlnAllocate(heap, sizeof(*node));
		if (node == NULL) {
			ret = 2;
			goto close;
		}
		MlnMemset(node, 0, sizeof(*node));
		node->path = MlnStrdup(heap, path);
		if (node->path == NULL) {
			ret = 2;
			MlnFree(heap, node->path);
			MlnFree(heap, node);
			goto close;
		}
		node->next = list;
		list = node;
	}
	*files = list;
	list = NULL;
	ret = 0;
close:
	sceIoDclose(fd);
out:
	MlnImpl_810257DC(heap, list);
	return ret;
}
