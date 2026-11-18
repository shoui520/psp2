/*
 * The files ("FILE:"): the objects' files and folders on the console's storages, their
 * dates and sizes, and how much a storage holds.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include "mtp_internal.h"

#define MTP_TAG_FILE			"FILE:"

/* the modes Sony's code creates them with */
#define MTP_FILE_MODE			6
#define MTP_FOLDER_MODE			7

int mtpFileExists(const char *path)
{
	SceIoStat stat;
	int exists = 1;

	if (sceIoGetstat(path, &stat) < 0)
		exists = 0;
	return exists;
}

int mtpFileGetDates(const char *path, SceDateTime *ctime, SceDateTime *mtime)
{
	SceIoStat stat;
	int ret;

	ret = sceIoGetstat(path, &stat);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_FILE, 60, "unable to get file i/o stat=0x%x\n", ret);
		return ret;
	}
	mtpUtilMemcpy(ctime, &stat.st_ctime, sizeof(*ctime));
	mtpUtilMemcpy(mtime, &stat.st_mtime, sizeof(*mtime));
	return ret;
}

int mtpFileSetDates(const char *path, const SceDateTime *ctime, const SceDateTime *mtime)
{
	SceIoStat stat;
	int ret = 0;

	if (path == NULL || (ctime == NULL && mtime == NULL))
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (mtime != NULL) {
		mtpUtilMemset(&stat, 0, sizeof(stat));
		mtpUtilMemcpy(&stat.st_mtime, mtime, sizeof(*mtime));
		ret = sceIoChstat(path, &stat, SCE_CST_MT);
		if (ret < 0)
			MTP_ERR(MTP_TAG_FILE, 96, "sceIoChstat for %s, mtime failed: 0x%x\n", path,
				ret);
	}
	if (ctime != NULL) {
		mtpUtilMemset(&stat, 0, sizeof(stat));
		mtpUtilMemcpy(&stat.st_ctime, ctime, sizeof(*ctime));
		ret = sceIoChstat(path, &stat, SCE_CST_CT);
		if (ret < 0)
			MTP_ERR(MTP_TAG_FILE, 107, "sceIoChstat for %s, ctime failed: 0x%x\n", path,
				ret);
	}
	return ret;
}

int mtpFileGetSize(const char *path, SceOff *size)
{
	SceIoStat stat;
	int ret;

	ret = sceIoGetstat(path, &stat);
	if (ret < 0)
		MTP_ERR(MTP_TAG_FILE, 127, "unable to get file i/o stat=0x%x, %s\n", ret, path);
	else
		*size = stat.st_size;
	return ret;
}

/* create the file or folder unless it is there */
int mtpFileCreate(const char *path, SceUInt16 format)
{
	SceIoStat stat;
	SceUID fd;
	int ret = 0;

	if (path == NULL) {
		MTP_ERR(MTP_TAG_FILE, 151, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (sceIoGetstat(path, &stat) < 0) {
		if (format == MTP_FORMAT_ASSOCIATION) {
			ret = sceIoMkdir(path, MTP_FOLDER_MODE);
		} else {
			fd = sceIoOpen(path, SCE_O_RDWR | SCE_O_CREAT | SCE_O_EXCL, MTP_FILE_MODE);
			if (fd < 0)
				ret = fd;
			else
				sceIoClose(fd);
		}
	}
	return ret;
}

int mtpFileDelete(const char *path, SceUInt16 format)
{
	if (path == NULL) {
		MTP_ERR(MTP_TAG_FILE, 188, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (format == MTP_FORMAT_ASSOCIATION)
		return sceIoRmdir(path);
	return sceIoRemove(path);
}

int mtpFileGetData(const char *path, void *buf, SceSize size)
{
	SceUID fd;
	int ret;

	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0)
		return fd;
	ret = sceIoRead(fd, buf, size);
	if (ret < 0)
		MTP_ERR(MTP_TAG_FILE, 294, "unable to read file=0x%x, %s\n", ret, path);
	sceIoClose(fd);
	return ret;
}

SceUID mtpFileOpen(const char *path, int flags, SceMode mode)
{
	if (path == NULL) {
		MTP_ERR(MTP_TAG_FILE, 315, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	return sceIoOpen(path, flags, mode);
}

int mtpFileClose(SceUID fd)
{
	return sceIoClose(fd);
}

SceOff mtpFileSeek(SceUID fd, SceOff offset, int whence)
{
	return sceIoLseek(fd, offset, whence);
}

/* a short read or write is an error */
int mtpFileRead(SceUID fd, void *buf, SceSize size)
{
	SceSize ret;

	ret = sceIoRead(fd, buf, size);
	if (ret < size) {
		MTP_ERR(MTP_TAG_FILE, 363, "sceIoRead read small size %d, expected %d\n", ret,
			size);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	return ret;
}

int mtpFileWrite(SceUID fd, const void *buf, SceSize size)
{
	SceSize ret;

	ret = sceIoWrite(fd, buf, size);
	if (ret < size) {
		MTP_ERR(MTP_TAG_FILE, 382, "sceIoWrite wrote small size %d, expected %d\n", ret,
			size);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	return ret;
}

int mtpFileGetStorageInfo(const char *path, SceUInt64 *maxCapacity, SceUInt64 *freeSpace)
{
	int ret;

	if (path == NULL || maxCapacity == NULL || freeSpace == NULL) {
		MTP_ERR(MTP_TAG_FILE, 394, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = sceFsUtilGetDevInfo(path, maxCapacity, freeSpace);
	if (ret < 0)
		MTP_ERR(MTP_TAG_FILE, 400, "sceFsUtilGetDevInfo %s failed: 0x%x\n", path, ret);
	return ret;
}
