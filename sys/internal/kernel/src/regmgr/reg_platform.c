/*
 * reg_platform.c: what the reg library asks of the system it runs on: the
 * registry files under vd0:. All names unknown.
 */

#include <iofilemgr/iofilemgr.h>
#include "reg_internal.h"

#define REG_PATH_SIZE	0x108	/* "vd0:" and a registry file path */
#define REG_O_FLAGS	0x4010000	/* name unknown */

/* the reg library's errors (names unknown) */
#define REG_ERROR_WRITE		(-6)
#define REG_ERROR_READ		(-8)
#define REG_ERROR_SEEK		(-9)
#define REG_ERROR_STAT		(-10)
#define REG_ERROR_NO_MEMORY	(-11)
#define REG_ERROR_RENAME	(-14)
#define REG_ERROR_REMOVE	(-5)
#define REG_READ_EOF		1

SceIoStat g_regDirStat;		/* name unknown */

/* the platform is ready (nothing to do) */
int regPlatformInit(void)
{
	return 0;
}

/* create the registry folder when it is missing */
void regMakeDirectory(void)
{
	if (sceIoGetstat("vd0:/registry", &g_regDirStat) >= 0)
		return;

	sceIoMkdir("vd0:/registry", 6);
}

/* open a registry file for writing, created when missing */
SceUID regFileCreate(const char *path)
{
	char *fullPath;
	SceUID fd;

	fullPath = regMemAlloc(REG_PATH_SIZE);
	if (fullPath == NULL)
		return REG_ERROR_NO_MEMORY_HEAP;

	snprintf(fullPath, REG_PATH_SIZE, "vd0:%s", path);
	fd = sceIoOpen(fullPath, SCE_O_RDWR | SCE_O_CREAT | REG_O_FLAGS, 6);
	memset(fullPath, 0, REG_PATH_SIZE);
	regMemFree(fullPath);

	return fd;
}

/* open a file by its full path for writing, created when missing */
SceUID regFileCreateFullPath(const char *path)
{
	return sceIoOpen(path, SCE_O_RDWR | SCE_O_CREAT | REG_O_FLAGS, 6);
}

/* open a registry file for reading and writing */
SceUID regFileOpen(const char *path)
{
	char *fullPath;
	SceUID fd;

	fullPath = regMemAlloc(REG_PATH_SIZE);
	if (fullPath == NULL)
		return REG_ERROR_NO_MEMORY_HEAP;

	snprintf(fullPath, REG_PATH_SIZE, "vd0:%s", path);
	fd = sceIoOpen(fullPath, SCE_O_RDWR | REG_O_FLAGS, 0);
	memset(fullPath, 0, REG_PATH_SIZE);
	regMemFree(fullPath);

	return fd;
}

/* open a file by its full path for reading and writing */
SceUID regFileOpenFullPath(const char *path)
{
	return sceIoOpen(path, SCE_O_RDWR | REG_O_FLAGS, 0);
}

/* close a registry file: -1 on failure */
int regFileClose(SceUID fd)
{
	return sceIoClose(fd) >> 31;
}

/* remove a registry file */
int regFileRemove(const char *path)
{
	char *fullPath;
	int ret;

	fullPath = regMemAlloc(REG_PATH_SIZE);
	if (fullPath == NULL)
		return REG_ERROR_NO_MEMORY;

	snprintf(fullPath, REG_PATH_SIZE, "vd0:%s", path);
	ret = sceIoRemove(fullPath);
	memset(fullPath, 0, REG_PATH_SIZE);
	regMemFree(fullPath);

	return ret < 0 ? REG_ERROR_REMOVE : 0;
}

/* rename a registry file */
int regFileRename(const char *oldPath, const char *newPath)
{
	char *oldFullPath;
	char *newFullPath;
	int ret;

	oldFullPath = regMemAlloc(REG_PATH_SIZE);
	newFullPath = regMemAlloc(REG_PATH_SIZE);
	if (oldFullPath == NULL || newFullPath == NULL) {
		ret = REG_ERROR_NO_MEMORY_HEAP;
	} else {
		snprintf(oldFullPath, REG_PATH_SIZE, "vd0:%s", oldPath);
		snprintf(newFullPath, REG_PATH_SIZE, "vd0:%s", newPath);
		ret = sceIoRename(oldFullPath, newFullPath) < 0 ? REG_ERROR_RENAME : 0;
	}

	if (oldFullPath != NULL) {
		memset(oldFullPath, 0, REG_PATH_SIZE);
		regMemFree(oldFullPath);
	}
	if (newFullPath != NULL) {
		memset(newFullPath, 0, REG_PATH_SIZE);
		regMemFree(newFullPath);
	}

	return ret;
}

/* write num items of size bytes */
int regFileWrite(const void *data, int size, int num, SceUID fd, int *written)
{
	int ret;

	ret = sceIoWrite(fd, data, size * num);
	*written = ret;

	return ret < size * num ? REG_ERROR_WRITE : 0;
}

/* read num items of size bytes: REG_READ_EOF at the end of the file */
int regFileRead(void *data, int size, int num, SceUID fd, int *read)
{
	int ret;

	ret = sceIoRead(fd, data, size * num);
	*read = ret;
	if (ret >= size * num)
		return 0;

	return ret == 0 ? REG_READ_EOF : REG_ERROR_READ;
}

/* seek in a registry file */
int regFileSeek(SceUID fd, int offset, int whence)
{
	return sceIoLseek(fd, offset, whence) < 0 ? REG_ERROR_SEEK : 0;
}

/* the size of a registry file */
int regFileGetSize(const char *path, SceUInt32 *size)
{
	SceIoStat stat;
	char *fullPath;
	int ret;

	fullPath = regMemAlloc(REG_PATH_SIZE);
	if (fullPath == NULL)
		return REG_ERROR_NO_MEMORY;

	snprintf(fullPath, REG_PATH_SIZE, "vd0:%s", path);
	ret = sceIoGetstat(fullPath, &stat);
	memset(fullPath, 0, REG_PATH_SIZE);
	regMemFree(fullPath);
	if (ret < 0)
		return REG_ERROR_STAT;

	size[0] = (SceUInt32)stat.st_size;
	size[1] = 0;

	return 0;
}
