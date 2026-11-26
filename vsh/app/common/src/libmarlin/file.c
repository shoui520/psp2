/*
 * The file layer: paths joined with the platform's separator, files read through an
 * 8 KiB block cache, directories made when missing. A path without a device name is
 * taken from the host (host0:). The file's name is ours.
 */
#include <kernel/iofilemgr.h>
#include <sceerror.h>
#include "mln_internal.h"

#define MLN_FILE_PATH_MAX	1024
#define MLN_FILE_DEVICE		"host0:"
#define MLN_FILE_DEVICE_LENGTH	6
#define MLN_FILE_BLOCK_SIZE	8192
#define MLN_FILE_CACHE_ALIGN	64

/* the open modes: the low bit reads, 8 is an alias of each */
#define MLN_FILE_MODE_READ	1
#define MLN_FILE_MODE_WRITE	6

#define MLN_FILE_ERROR_READ	0x11060003
#define MLN_FILE_ERROR_WRITE	0x11060004
#define MLN_FILE_ERROR_SEEK	0x11060005
#define MLN_FILE_ERROR_TELL	0x11060006
#define MLN_FILE_ERROR_STAT	0x11060007
#define MLN_FILE_ERROR_MODE	0x11060008

typedef struct MlnFile {
	int mode;
	SceUID fd;
	int position;		/* -1 until a file opened for writing is read */
	int block;		/* the block the cache holds, -1 for none */
	int length;		/* the bytes of that block read */
	unsigned char *cache;	/* data, aligned */
	unsigned char data[MLN_FILE_BLOCK_SIZE + MLN_FILE_CACHE_ALIGN];
} MlnFile;

/* the platform path of path (length bytes): host0: unless it names a device */
static inline __attribute__((always_inline)) void MlnFilePath(char *out, const char *path, int length)
{
	int i;

	for (i = 0; i < length; i++)
		if (((const signed char *)path)[i] == ':')
			break;
	if (i == length) {
		MlnMemcpy(out, MLN_FILE_DEVICE, MLN_FILE_DEVICE_LENGTH);
		MlnMemcpy(out + MLN_FILE_DEVICE_LENGTH, path, i + 1);
	} else {
		MlnMemcpy(out, path, length + 1);
	}
}

/* append name to the directory path; 1 with the size needed when *size is too small */
int MlnImpl_810256EC(unsigned int *size, char *path, const char *name)
{
	unsigned int length, nameLength, need;

	if (path == NULL || size == NULL || name == NULL)
		return 3;
	length = MlnStrlen(path);
	nameLength = MlnStrlen(name);
	if (length != 0 && (signed char)path[length - 1] != MlnImpl_81026430())
		length++;
	need = length + nameLength + 1;
	if (*size < need)
		goto small;
	if (length != 0)
		path[length - 1] = MlnImpl_81026430();
	MlnMemcpy(path + length, name, nameLength + 1);
	*size = need;
	return 0;

small:
	*size = need;
	return 1;
}

/* free a directory's file list */
int MlnImpl_810257DC(int heap, MlnFileList *files)
{
	MlnFileList *next;

	for (; files != NULL; files = next) {
		next = files->next;
		MlnFree(heap, files->path);
		MlnFree(heap, files);
	}
	return 0;
}

/* make the directory unless it is there; -1 on failure */
int MlnImpl_8102581C(const char *directory)
{
	SceIoStat stat;
	char path[MLN_FILE_PATH_MAX];
	int length;

	length = MlnStrlen(directory);
	if (length > MLN_FILE_PATH_MAX - MLN_FILE_DEVICE_LENGTH)
		return -1;
	MlnFilePath(path, directory, length);
	if (sceIoGetstat(path, &stat) != 0) {
		if (sceIoMkdir(path, SCE_STM_RWU | 0006) < 0)
			return -1;
	} else if (!(stat.st_mode & SCE_STM_FDIR)) {
		return -1;
	}
	return 0;
}

int MlnImpl_8102590C(int area, const char *name)
{
	char path[MLN_FILE_PATH_MAX];
	int length, ret;

	length = MlnStrlen(name);
	if (length > MLN_FILE_PATH_MAX - MLN_FILE_DEVICE_LENGTH)
		return MLN_FILE_ERROR_PLATFORM;
	MlnFilePath(path, name, length);
	ret = sceIoRemove(path);
	if (ret != 0) {
		if (ret == SCE_ERROR_ERRNO_ENOENT)
			return MLN_FILE_ERROR_NOT_FOUND;
		return MLN_FILE_ERROR_PLATFORM;
	}
	return ret;
}

int MlnImpl_810259F8(int area, const char *name, int mode, void **handle)
{
	char path[MLN_FILE_PATH_MAX];
	MlnFile *file;
	SceUID fd;
	int length, ret;

	if (handle == NULL)
		return 3;
	file = MlnAllocate(area, sizeof(*file));
	if (file == NULL)
		return 2;
	length = MlnStrlen(name);
	if (length > MLN_FILE_PATH_MAX - MLN_FILE_DEVICE_LENGTH) {
		ret = MLN_FILE_ERROR_PLATFORM;
		goto error;
	}
	MlnFilePath(path, name, length);
	switch (mode) {
	case 1:
	case 9:
		fd = sceIoOpen(path, SCE_O_RDONLY, 0666);
		break;
	case 2:
	case 10:
		fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
		break;
	case 3:
	case 11:
		fd = sceIoOpen(path, SCE_O_RDWR, 0777);
		break;
	case 4:
	case 12:
		fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_APPEND | SCE_O_CREAT, 0777);
		break;
	default:
		ret = MLN_FILE_ERROR_MODE;
		goto error;
	}
	if (fd < 0) {
		if (fd == SCE_ERROR_ERRNO_ENOENT)
			ret = MLN_FILE_ERROR_NOT_FOUND;
		else
			ret = MLN_FILE_ERROR_PLATFORM;
		goto error;
	}
	file->cache = (unsigned char *)(((unsigned int)file->data + MLN_FILE_CACHE_ALIGN - 1) &
				       ~(MLN_FILE_CACHE_ALIGN - 1));
	file->position = (mode & MLN_FILE_MODE_READ) ? 0 : -1;
	file->block = -1;
	file->length = 0;
	file->mode = mode;
	file->fd = fd;
	*handle = file;
	return 0;

error:
	MlnFree(area, file);
	return ret;
}

int MlnImpl_81025BE8(int area, void *handle)
{
	MlnFile *file = handle;

	if (file == NULL)
		return 4;
	sceIoClose(file->fd);
	MlnFree(area, file);
	return 0;
}

/* read size bytes into data through the block cache; *read says how many were there */
int MlnImpl_81025C1C(int area, void *handle, unsigned int size, unsigned int *read, void *data)
{
	MlnFile *file = handle;
	unsigned char *out = data;
	unsigned int done, left;
	int block, offset, ready, n;
	SceUID fd;

	if (data == NULL || read == NULL || size == 0)
		return 3;
	if (file == NULL || (fd = file->fd) == 0 || !(file->mode & MLN_FILE_MODE_READ))
		return 4;
	block = file->position / MLN_FILE_BLOCK_SIZE;
	if (file->block != block) {
		sceIoLseek(fd, block * MLN_FILE_BLOCK_SIZE, SCE_SEEK_SET);
		n = sceIoRead(fd, file->cache, MLN_FILE_BLOCK_SIZE);
		if (n < 0)
			return MLN_FILE_ERROR_READ;
		file->block = block;
		file->length = n;
	}
	offset = file->position % MLN_FILE_BLOCK_SIZE;
	ready = file->length - offset;
	if (ready < 0)
		return MLN_FILE_ERROR_READ;
	if (size <= (unsigned int)ready) {
		MlnMemcpy(out, file->cache + offset, size);
		file->position += size;
		*read = size;
		return 0;
	}
	MlnMemcpy(out, file->cache + offset, ready);
	file->position += ready;
	done = ready;
	if (file->position >= sceIoLseek(fd, 0, SCE_SEEK_END)) {
		*read = done;
		return 0;
	}
	sceIoLseek(fd, file->position, SCE_SEEK_SET);
	left = size - ready;
	while (left > MLN_FILE_BLOCK_SIZE) {
		n = sceIoRead(fd, out + done, MLN_FILE_BLOCK_SIZE);
		if (n < 0)
			return MLN_FILE_ERROR_READ;
		if (n == 0) {
			*read = done;
			return 0;
		}
		left -= n;
		done += n;
		file->position += n;
	}
	if (left != 0) {
		n = sceIoRead(fd, file->cache, MLN_FILE_BLOCK_SIZE);
		if (n < 0)
			return MLN_FILE_ERROR_READ;
		file->length = n;
		file->block = file->position / MLN_FILE_BLOCK_SIZE;
		if ((unsigned int)n >= left)
			n = left;
		MlnMemcpy(out + done, file->cache, n);
		done += n;
		file->position += n;
	}
	*read = done;
	return 0;
}

/* read up to and including the first delimiter, leaving the file just past it */
#ifndef MLN_PLAYBACK
int MlnImpl_81025E74(int area, void *handle, int delimiter, unsigned int size,
		     unsigned int *read, unsigned char *data)
{
	MlnFile *file = handle;
	unsigned int got, i;

	if (data == NULL || read == NULL || size == 0)
		return 3;
	if (file == NULL || file->fd == 0 || !(file->mode & MLN_FILE_MODE_READ))
		return 4;
	if (MlnImpl_81025C1C(area, file, size, &got, data) == 0 && got != 0) {
		for (i = 0; i < got; i++) {
			if (data[i] == (unsigned char)delimiter) {
				if (got > i + 1) {
					file->position = file->position - got + i + 1;
					got = i + 1;
				}
				break;
			}
		}
	}
	*read = got;
	return 0;
}
#endif

int MlnImpl_81025F5C(int area, void *handle, unsigned int size, const void *data)
{
	MlnFile *file = handle;
	SceUID fd;

	if (data == NULL || size == 0)
		return 3;
	if (file == NULL || (fd = file->fd) == 0 || !(file->mode & MLN_FILE_MODE_WRITE))
		return 4;
	file->length = 0;
	file->block = -1;
	if (sceIoWrite(fd, data, size) < size)
		return MLN_FILE_ERROR_WRITE;
	return 0;
}

int MlnImpl_81025FD4(int area, void *handle, int offset, int whence)
{
	MlnFile *file = handle;
	SceUID fd;
	int ret;

	if (file == NULL || (fd = file->fd) == 0)
		return 4;
	if (whence == SCE_SEEK_CUR) {
		/* the descriptor is ahead of a reader by its cache */
		if (file->position >= 0)
			sceIoLseek(fd, file->position, SCE_SEEK_SET);
	} else if (whence != SCE_SEEK_END && whence != SCE_SEEK_SET) {
		return 3;
	}
	ret = sceIoLseek(fd, offset, whence);
	if (ret < 0)
		return MLN_FILE_ERROR_SEEK;
	if (file->position >= 0)
		file->position = ret;
	return 0;
}

int MlnImpl_81026084(int area, void *handle, int *position)
{
	MlnFile *file = handle;
	SceUID fd;
	int ret;

	if (position == NULL)
		return 3;
	if (file == NULL || (fd = file->fd) == 0)
		return 4;
	ret = sceIoLseek(fd, 0, SCE_SEEK_CUR);
	if (ret < 0)
		return MLN_FILE_ERROR_TELL;
	*position = ret;
	return 0;
}

/* the file's size, the descriptor left where it was */
int MlnImpl_810260E4(int area, void *handle, unsigned int *size)
{
	MlnFile *file = handle;
	SceUID fd;
	int position, end;

	if (size == NULL)
		return 3;
	if (file == NULL || (fd = file->fd) == 0)
		return 4;
	position = sceIoLseek(fd, 0, SCE_SEEK_CUR);
	if (position < 0)
		return MLN_FILE_ERROR_TELL;
	end = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (end < 0)
		return MLN_FILE_ERROR_SEEK;
	if (sceIoLseek(fd, position, SCE_SEEK_SET) < 0)
		return MLN_FILE_ERROR_SEEK;
	*size = end;
	return 0;
}

/* make every directory of path */
int MlnImpl_8102619C(int area, const char *path)
{
	const char *start, *slash;
	char *copy, *p;
	unsigned int length;
	int ret;

	if (path == NULL || *(const signed char *)path == '\0')
		return 3;
	length = MlnStrlen(path);
	start = MlnStrchr(path, ':');
	start = start != NULL ? start + 1 : path;
	if (*(const signed char *)start == '/')
		start++;
	slash = MlnStrchr(start, '/');
	if (slash == NULL) {
		copy = NULL;
	} else {
		copy = MlnAllocate(area, length + 1);
		if (copy == NULL)
			return 2;
		MlnMemcpy(copy, path, length + 1);
		p = copy + (slash - path);
		while (1) {
			*p = '\0';
			if (MlnImpl_8102581C(copy) != 0) {
				ret = MLN_FILE_ERROR_PLATFORM;
				goto out;
			}
			*p = '/';
			p = MlnStrchr(p + 1, '/');
			if (p == NULL)
				break;
		}
	}
	ret = MLN_FILE_ERROR_PLATFORM;
	if (MlnImpl_8102581C(path) == 0)
		ret = 0;
	if (copy == NULL)
		return ret;
out:
	MlnFree(area, copy);
	return ret;
}

/* the mount point of the client's data; 5 with its length when *size is too small */
int MlnImpl_810262B4(int area, unsigned int *size, char *path)
{
	unsigned int length;

	if (*size < MlnStrlen(s_mountPoint.drive_id) + 1) {
		*size = MlnStrlen(s_mountPoint.drive_id);
		return MLN_FILE_ERROR_INVALID;
	}
	length = MlnStrlen(s_mountPoint.drive_id);
	*size = length;
	MlnMemcpy(path, s_mountPoint.drive_id, length + 1);
	return 0;
}

int MlnImpl_81026324(int area, const char *path, MlnFileInfo *info)
{
	SceIoStat stat;
	int ret;

	if (path == NULL || info == NULL)
		return 3;
	ret = sceIoGetstat(path, &stat);
	if (ret < 0) {
		if (ret == SCE_ERROR_ERRNO_ENOENT || ret == SCE_ERROR_ERRNO_ENOTDIR)
			return MLN_FILE_ERROR_NOT_FOUND;
		return MLN_FILE_ERROR_STAT;
	}
	switch (stat.st_mode & SCE_STM_FMT) {
	case SCE_STM_FREG:
		info->type = 1;
		break;
	case SCE_STM_FDIR:
		info->type = 0;
		break;
	default:
		return MLN_FILE_ERROR_STAT;
	}
	info->size = stat.st_size;
	return 0;
}

/* *isDirectory 0 for a directory, -1 otherwise */
int MlnImpl_810263D4(const char *path, int *isDirectory)
{
	SceIoStat stat;

	if (path == NULL || isDirectory == NULL)
		return 3;
	*isDirectory = -1;
	if (sceIoGetstat(path, &stat) != 0)
		return 0;
	if (SCE_STM_ISDIR(stat.st_mode))
		*isDirectory = 0;
	return 0;
}

