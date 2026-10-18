/* uvfat_rawio.c */
#include <sysmem/debug.h>
#include <string.h>
#include "exfatfs_internal.h"
#include <psp2kern/types.h>
#include <psp2kern/kernel/threadmgr/misc.h>

extern void *allocateFatBuffer(SceSize size);
extern void freeFatBuffer(void *buffer, SceSize size);
extern void touchFnodeModificationTime(SceExfatfsFnode *fnode);
extern SceUInt32 getInitializedFileCount(SceExfatfsSharedResource *sharedResource);
extern void freeUpcaseStorage(SceExfatfsFileSystem *fileSystem);
extern int updateallocbitmaps(SceExfatfsFileSystem *fileSystem);
extern int flushFileSystemMetadata(SceExfatfsFileSystem *fileSystem);
extern int _check_allocation_bitmap(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem);
extern int UVFAT_readupcasetable(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem);
extern void _exfatfs_notify_invalid_boot_signature(SceExfatfsDrive *drive);
extern void _exfatfs_notify_invalid_sector_shift(SceExfatfsDrive *drive,
	SceUInt32 sectorSize);
extern void _exfatfs_notify_unsupported_sector_size(SceExfatfsDrive *drive);

int hasCachedFreeClusterCount(SceExfatfsFileSystem *fileSystem)
{
	return fileSystem->freeClusterCountValid == 1;
}

void cacheFreeClusterCount(SceExfatfsFileSystem *fileSystem, SceUInt32 count)
{
	fileSystem->freeClusterCount = count;
	fileSystem->freeClusterCountValid = 1;
}

void accountAllocatedCluster(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster)
{
	if (fileSystem->fileSystemType == 2)
		fileSystem->bitmapFreeClusterCount--;
	if (fileSystem->freeClusterCountValid == 1)
		fileSystem->freeClusterCount--;
	if (fileSystem->nextFreeCluster == cluster)
		fileSystem->nextFreeCluster = cluster + 1;
}

void accountFreedCluster(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster)
{
	if (fileSystem->fileSystemType == 2) {
		fileSystem->bitmapFreeClusterCount++;
		if (fileSystem->freeClusterCountValid != 1)
			goto update_next_free_cluster;
	} else if (fileSystem->freeClusterCountValid != 1) {
		goto update_next_free_cluster;
	}
	fileSystem->freeClusterCount++;

update_next_free_cluster:
	if (fileSystem->nextFreeCluster > cluster)
		fileSystem->nextFreeCluster = cluster;
}

SceUInt64 clusterToDeviceSector(SceUInt32 cluster,
	SceExfatfsFileSystem *fileSystem)
{
	const SceExfatfsBootSector *boot = fileSystem->PBRBuf;
	SceUInt32 clusterSectors, rootCluster;

	if (fileSystem->fileSystemType != 2) {
		if (fileSystem->FATBits == 32) {
			rootCluster = fileSystem->firstClusterOfRoot - 2;
			clusterSectors = boot->fat32.parameters.sectorsPerCluster;
		} else {
			clusterSectors = boot->fat32.parameters.sectorsPerCluster;
			rootCluster = 0;
		}
	} else {
		clusterSectors = fileSystem->clusterSectorCount;
		rootCluster = fileSystem->firstClusterOfRoot - 2;
	}
	if (cluster > 1)
		return (SceUInt64)(cluster - 2) * clusterSectors +
			(SceUInt32)(fileSystem->rootDirectorySectors +
				fileSystem->dataAreaStartSector);
	return (SceUInt64)(rootCluster + cluster) * clusterSectors +
		fileSystem->dataAreaStartSector;
}

int (*g_currentClockCallback)(SceDateTime *time, int timeZone);

int getCurrentFatDateTime(SceExfatfsDateTime *dateTime)
{
	SceDateTime time;

	if (g_currentClockCallback && !g_currentClockCallback(&time, 0)) {
		SceUInt16 year = time.year;
		SceUInt16 month = time.month;
		SceUInt16 day = time.day;
		SceUInt16 hour = time.hour;
		SceUInt16 minute = time.minute;
		SceUInt16 second = time.second;

		dateTime->year = year;
		dateTime->month = month;
		dateTime->day = day;
		dateTime->hour = hour;
		dateTime->minute = minute;
		dateTime->second = second;
	} else {
		dateTime->month = 1;
		dateTime->year = 2000;
		dateTime->day = 1;
		dateTime->hour = 0;
		dateTime->minute = 0;
		dateTime->second = 0;
	}
	return 0;
}

int initializeFatDateTime(SceExfatfsDateTime *dateTime)
{
	dateTime->year = 2000;
	dateTime->month = 1;
	dateTime->day = 1;
	dateTime->hour = 0;
	dateTime->minute = 0;
	dateTime->second = 0;
	return 0;
}

unsigned int UVFAT_TimeToFATDate(SceExfatfsDateTime *dateTime)
{
	SCE_DBG_ASSERT(dateTime->year >= 1980, 1514);
	SCE_DBG_ASSERT(dateTime->year <= 2107, 1515);
	return (SceUInt16)(dateTime->day | (dateTime->month << 5) |
		((dateTime->year - 1980) << 9));
}

#if FW_DEBUG
int _exfatfs_cleanup_device_file_buffer(SceExfatfsDrive *drive)
#else
static inline int _exfatfs_cleanup_device_file_buffer(SceExfatfsDrive *drive)
#endif
{
	SceVfsVnode *vp = drive->deviceInfo.device_vnode;
	SceVfsFile *file = drive->deviceInfo.device_file;
	int result = sceVopSync(vp, file, 2);

	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 135,
			"%s: drive[%p], assignName(%s), vp[%p], file[%p], sceVopSync result[%#x]\n",
			"_exfatfs_cleanup_device_file_buffer", drive,
			drive->deviceInfo.mount_misc->assign_name, vp, file, result);
		vp = drive->deviceInfo.device_vnode;
		file = drive->deviceInfo.device_file;
		result = sceVopCleanup(vp, file, 0, 0);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 115, "_exfatfs_force_invalidate_device_file_buffer",
				"%s: drive[%p], assignName(%s), vp[%p], file[%p], sceVopCleanup failed[%#x]\n",
				"_exfatfs_force_invalidate_device_file_buffer", drive,
				drive->deviceInfo.mount_misc->assign_name, vp, file, result);
			if (result == (int)0x80010030)
				result = 0;
		}
	}
	return result;
}

#if FW_DEBUG
int _exfatfs_write_device_file(SceExfatfsDrive *drive, const void *buffer,
	SceSize size, SceOff offset, SceSize *writeSize)
#else
static inline int _exfatfs_write_device_file(SceExfatfsDrive *drive, const void *buffer,
	SceSize size, SceOff offset, SceSize *writeSize)
#endif
{
	SceVfsVnode *vp = drive->deviceInfo.device_vnode;
	SceVfsFile *file = drive->deviceInfo.device_file;
	SceOff newoff;
	int result;

	SCE_DBG_ASSERT(vp->core.ops != 0, 268);
	if (vp->core.ops->vop_pwrite) {
		result = sceVopPwrite(vp, file, buffer, size, offset, writeSize);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0, 308, "%s: vop_pwrite failed[%#x]\n",
				"_exfatfs_write_device_file", result);
	} else if (vp->core.ops->vop_lseek && vp->core.ops->vop_write) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 312, "%s: use vop_lseek and vop_write\n", "_exfatfs_write_device_file");
		newoff = sceVopLseek(vp, file, offset, 0);
		result = newoff;
		if (newoff < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 325, "%s: vop_lseek failed[%#llx]\n", "_exfatfs_write_device_file", newoff);
		} else if (newoff == offset) {
			result = sceVopWrite(vp, file, buffer, size, writeSize);
			if (result < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0, 342, "%s: vop_write failed[%#x]\n", "_exfatfs_write_device_file", result);
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 328, "%s: newoff[%#llx] != offset[%#llx]\n",
				"_exfatfs_write_device_file", newoff, offset);
			result = (int)0x80010030;
		}
	} else {
		SCE_DBG_PRINTF_LEVEL(1, 0, 346, "%s: no vop\n", "_exfatfs_write_device_file");
		result = (int)0x80010030;
	}
	return result;
}

#if FW_DEBUG
int _exfatfs_read_device_file(SceExfatfsDrive *drive, void *buffer,
	SceSize size, SceOff offset, SceSize *readSize)
#else
static inline int _exfatfs_read_device_file(SceExfatfsDrive *drive, void *buffer,
	SceSize size, SceOff offset, SceSize *readSize)
#endif
{
	SceVfsVnode *vp = drive->deviceInfo.device_vnode;
	SceVfsFile *file = drive->deviceInfo.device_file;
	SceOff newoff;
	int result;

	SCE_DBG_ASSERT(vp->core.ops != 0, 178);
	if (vp->core.ops->vop_pread) {
		result = sceVopPread(vp, file, buffer, size, offset, readSize);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0, 218, "%s: vop_pread failed[%#x]\n", "_exfatfs_read_device_file", result);
	} else if (vp->core.ops->vop_lseek && vp->core.ops->vop_read) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 222, "%s: use vop_lseek and vop_read\n", "_exfatfs_read_device_file");
		newoff = sceVopLseek(vp, file, offset, 0);
		result = newoff;
		if (newoff < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 235, "%s: vop_lseek failed[%#llx]\n", "_exfatfs_read_device_file", newoff);
		} else if (newoff == offset) {
			result = sceVopRead(vp, file, buffer, size, readSize);
			if (result < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0, 253, "%s: vop_read failed[%#x]\n", "_exfatfs_read_device_file", result);
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 238, "%s: newoff[%#llx] != offset[%#llx]\n",
				"_exfatfs_read_device_file", newoff, offset);
			result = (int)0x80010005;
		}
	} else {
		SCE_DBG_PRINTF_LEVEL(1, 0, 257, "%s: no vop\n", "_exfatfs_read_device_file");
		result = (int)0x80010030;
	}
	return result;
}

int exfatfs_cleanup_device_file_buffer(SceExfatfsDrive *drive)
{
	SceVfsVnode *vp;
	SceVfsFile *file;
	int result, unlockResult;

	if (!drive)
		return 0;
	vp = drive->deviceInfo.device_vnode;
	file = drive->deviceInfo.device_file;
	result = 0;
	if (vp && file) {
		if (drive->deviceInfo.device_fd == -1)
			return 0;
		SCE_DBG_ASSERT(vp->core.ops != 0, 153);
		result = vfsLockVnode(vp);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 156, "%s: _lock_device_vnode failed[%#x]\n",
				"exfatfs_cleanup_device_file_buffer", result);
		} else {
			result = _exfatfs_cleanup_device_file_buffer(drive);
			unlockResult = vfsUnlockVnode(vp);
			if (unlockResult < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0, 162, "%s: _unlock_device_vnode failed[%#x]\n",
					"exfatfs_cleanup_device_file_buffer", unlockResult);
		}
	}
	return result;
}

int UVFAT_ReadDevice(SceExfatfsFileSystem *fileSystem, SceUInt64 sector,
	unsigned int size, void *buffer)
{
	SceExfatfsDrive *drive;
	SceVfsVnode *vp;
	SceSize readSize;
	int result, unlockResult;

	if (size & 0x1ff) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 1847, "Illegal read size(%d)\n", size);
		return -32;
	}
	drive = fileSystem->drive;
	if (!drive)
		return -13;
	vp = drive->deviceInfo.device_vnode;
	if (!drive->deviceInfo.device_file || !vp || drive->deviceInfo.device_fd == -1)
		return -13;
	SCE_DBG_ASSERT(vp->core.ops != 0, 1862);
	result = vfsLockVnode(vp);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1865, "%s: _lock_device_vnode failed[%#x]\n", "UVFAT_ReadDevice", result);
		return -13;
	}
	readSize = 0;
	result = _exfatfs_read_device_file(fileSystem->drive, buffer, size,
		sector << fileSystem->BytePerSectorShift, &readSize);
	if (result < 0 && !_exfatfs_cleanup_device_file_buffer(fileSystem->drive)) {
		readSize = 0;
		result = _exfatfs_read_device_file(fileSystem->drive, buffer, size,
			sector << fileSystem->BytePerSectorShift, &readSize);
		SCE_DBG_PRINTF_LEVEL(1, 0, 1877, "%s: retry after cleanup result[%#x]\n", "UVFAT_ReadDevice", result);
	}
	unlockResult = vfsUnlockVnode(vp);
	if (unlockResult < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1882, "%s: _unlock_device_vnode failed[%#x]\n", "UVFAT_ReadDevice", unlockResult);
	if (result < 0) {
		if (result != (int)0x8032001a) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1890, "%s: convert error[%#x] -> internal error(%d)\n",
				"UVFAT_ReadDevice", result, -13);
			result = -13;
		}
	} else {
		result = 0;
		if (readSize != size) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1894, "%s: rsz[%#x] != size[%#x]\n", "UVFAT_ReadDevice", readSize, size);
			result = -13;
		}
	}
	return result;
}

int UVFAT_ReadCblock(SceUInt32 cluster, SceUInt32 block,
	SceExfatfsFileSystem *fileSystem, void *buffer)
{
	SceUInt32 clusterSectorCount, rootClusterOffset;
	SceUInt64 sector;

	if (fileSystem->fileSystemType != 2) {
		if (fileSystem->FATBits == 32) {
			clusterSectorCount = ((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat16.parameters.sectorsPerCluster;
			rootClusterOffset = fileSystem->firstClusterOfRoot - 2;
		} else {
			rootClusterOffset = 0;
			clusterSectorCount = ((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat16.parameters.sectorsPerCluster;
		}
		if (cluster > 1)
			goto dataCluster;
	} else {
		clusterSectorCount = fileSystem->clusterSectorCount;
		rootClusterOffset = fileSystem->firstClusterOfRoot - 2;
		if (cluster > 1)
			goto dataCluster;
	}
	sector = (SceUInt64)(cluster + rootClusterOffset) * clusterSectorCount +
		fileSystem->dataAreaStartSector;
	goto readBlock;
dataCluster:
	sector = (SceUInt64)(cluster - 2) * clusterSectorCount +
		(SceUInt32)(fileSystem->dataAreaStartSector + fileSystem->rootDirectorySectors);
readBlock:
	sector += (fileSystem->fileBufferSize >> fileSystem->BytePerSectorShift) * block;
	return UVFAT_ReadDevice(fileSystem, sector, fileSystem->fileBufferSize, buffer);
}

unsigned int fatDateTimeToTime(SceExfatfsDateTime *dateTime)
{
	return (SceUInt16)((dateTime->minute << 5) |
		(dateTime->hour << 11) | (dateTime->second >> 1));
}

extern int _sceFatfsWriteModeFromStat(SceExfatfsDrive *drive, int attribute);

int copyFatEntryAttributeToStat(const SceExfatfsFatDirectoryEntry *entry,
	SceExfatfsStat *stat, SceExfatfsFileSystem *fileSystem)
{
	const SceExfatfsBootSector *boot;
	SceUInt16 mode;
	int attribute;

	stat->st_attr = entry->attribute;
	stat->st_mode = _sceFatfsWriteModeFromStat(fileSystem->drive, entry->attribute);
	if (!(entry->attribute & 0x20))
		stat->st_mode |= 0x40;
	boot = fileSystem->PBRBuf;
	stat->st_type = entry->attribute & 0x10 ? 6 : 0;
	if ((signed char)entry->attribute < 0)
		stat->st_mode |= 0x200;
	if (strncmp((const char *)boot->fat16.fileSystemName, "UVFAT", 5)) {
		attribute = entry->attribute;
		stat->st_attr = attribute;
		mode = _sceFatfsWriteModeFromStat(fileSystem->drive, attribute);
		if (!(attribute & 0x20))
			mode |= 0x40;
		stat->st_mode = mode;
		stat->st_dev = 0;
		stat->st_type = attribute & 0x10 ? 6 : 0;
		stat->st_uid = 0;
	}
	return 0;
}

int applyFatAttributeToStat(SceExfatfsDrive *drive, int attribute,
	SceExfatfsStat *stat)
{
	SceUInt16 mode;

	stat->st_attr = attribute;
	mode = _sceFatfsWriteModeFromStat(drive, attribute);
	if (!(attribute & 0x20))
		mode |= 0x40;
	stat->st_mode = mode;
	stat->st_dev = 0;
	stat->st_type = attribute & 0x10 ? 6 : 0;
	stat->st_uid = 0;
	return 0;
}

int copyExfatEntryAttributeToStat(const SceExfatfsFileDirectoryEntry *entry,
	SceExfatfsStat *stat, SceExfatfsFileSystem *fileSystem)
{
	int attribute = entry->fileAttributes[0];
	SceUInt16 mode;

	stat->st_attr = attribute;
	mode = _sceFatfsWriteModeFromStat(fileSystem->drive, attribute);
	if (!(attribute & 0x20))
		mode |= 0x40;
	stat->st_mode = mode;
	stat->st_dev = 0;
	stat->st_type = attribute & 0x10 ? 6 : 0;
	stat->st_uid = 0;
	return 0;
}

int copyStatAttributeToFatEntry(const SceExfatfsStat *stat,
	SceExfatfsFatDirectoryEntry *entry, SceExfatfsFileSystem *fileSystem)
{
	(void)fileSystem;
	entry->attribute = stat->st_attr;
	return 0;
}

SceExfatfsDateTime *decodeFatDateTime(SceExfatfsDateTime *dateTime,
	unsigned int date, unsigned int time)
{
	dateTime->hour = time >> 11;
	dateTime->year = (date >> 9) + 1980;
	dateTime->month = (date >> 5) & 15;
	dateTime->day = date & 31;
	dateTime->minute = (time >> 5) & 63;
	dateTime->second = (time & 31) * 2;
	return dateTime;
}

int decodeExfatUtcOffset(unsigned int offset)
{
	int quarters, minutes;

	if ((signed char)offset >= 0)
		return 0;
	quarters = offset & ~0x80U;
	if (quarters > 63)
		minutes = quarters * 15 - 1920;
	else
		minutes = quarters * 15;
	return minutes;
}

/* Gregorian month lengths, January through December. */
const SceUInt16 g_daysInLeapMonth[12] = {
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};

const SceUInt16 g_daysInCommonMonth[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};

__attribute__((noinline)) void adjustFatDateTimeMinutes(SceExfatfsDateTime *dateTime, int minutes)
{
	const SceUInt16 *monthDays;
	int originalDay, day, hour, minute, hours, year;
	SceUInt16 month;

	if (!minutes)
		return;
	originalDay = dateTime->day;
	hour = dateTime->hour;
	hours = minutes / 60;
	minute = dateTime->minute + minutes % 60;
	if (minute > 59) {
		hour += 1 + hours;
		dateTime->minute = minute - 60;
	} else {
		if (minute < 0) {
			minute += 60;
			hour--;
		}
		hour += hours;
		dateTime->minute = minute;
	}
	if (hour > 23) {
		hour -= 24;
		day = (SceUInt16)(originalDay + 1);
		dateTime->day = day;
	} else if (hour < 0) {
		hour += 24;
		day = (SceUInt16)(originalDay - 1);
		dateTime->day = day;
	} else {
		dateTime->hour = hour;
		return;
	}
	dateTime->hour = hour;
	if (originalDay == day)
		return;
	year = dateTime->year;
	if ((year & 3) || ((SceUInt16)(year % 100U) == 0 &&
		(SceUInt16)(year % 400U) != 0))
		monthDays = g_daysInCommonMonth;
	else
		monthDays = g_daysInLeapMonth;
	month = dateTime->month;
	if (monthDays[month - 1] < day) {
		month++;
		dateTime->day = 1;
		dateTime->month = month;
	} else if (!day) {
		month--;
		dateTime->month = month;
		dateTime->day = monthDays[month ? month - 1 : 11];
	}
	if (month > 12) {
		dateTime->year = year + 1;
		dateTime->month = 1;
	} else if (!month) {
		dateTime->year = year - 1;
		dateTime->month = 12;
	}
}

void decodeExfatEntryDateTimes(SceExfatfsStat *stat,
	const SceExfatfsFileDirectoryEntry *entry)
{
	SceUInt32 stamp;
	SceUInt16 date, time;
	int offset, minutes;

	stamp = ((SceUInt32)entry->modificationTime[2] << 16) |
		((SceUInt32)entry->modificationTime[3] << 24) |
		entry->modificationTime[0] | (entry->modificationTime[1] << 8);
	date = stamp >> 16;
	time = stamp;
	offset = entry->modificationUtcOffset;
	stat->mtime.month = (date >> 5) & 15;
	stat->mtime.second = entry->modificationTenths / 100U + (time & 31) * 2;
	stat->mtime.day = date & 31;
	stat->mtime.year = (date >> 9) + 1980;
	stat->mtime.hour = time >> 11;
	stat->mtime.minute = (time >> 5) & 63;
	minutes = 0;
	if ((signed char)offset < 0) {
		offset &= ~0x80;
		if (offset > 63)
			minutes = 1920 - 15 * offset;
		else
			minutes = -15 * offset;
	}
	adjustFatDateTimeMinutes(&stat->mtime, minutes);
	stamp = ((SceUInt32)entry->creationTime[2] << 16) |
		((SceUInt32)entry->creationTime[3] << 24) |
		entry->creationTime[0] | (entry->creationTime[1] << 8);
	date = stamp >> 16;
	time = stamp;
	offset = entry->creationUtcOffset;
	stat->ctime.day = date & 31;
	stat->ctime.month = (date >> 5) & 15;
	stat->ctime.second = entry->creationTenths / 100U + (time & 31) * 2;
	stat->ctime.year = (date >> 9) + 1980;
	stat->ctime.hour = time >> 11;
	stat->ctime.minute = (time >> 5) & 63;
	minutes = 0;
	if ((signed char)offset < 0) {
		offset &= ~0x80;
		if (offset > 63)
			minutes = 1920 - 15 * offset;
		else
			minutes = -15 * offset;
	}
	adjustFatDateTimeMinutes(&stat->ctime, minutes);
	stamp = ((SceUInt32)entry->accessTime[2] << 16) |
		((SceUInt32)entry->accessTime[3] << 24) |
		entry->accessTime[0] | (entry->accessTime[1] << 8);
	date = stamp >> 16;
	time = stamp;
	offset = entry->accessUtcOffset;
	stat->atime.hour = time >> 11;
	stat->atime.month = (date >> 5) & 15;
	stat->atime.day = date & 31;
	stat->atime.year = (date >> 9) + 1980;
	stat->atime.minute = (time >> 5) & 63;
	stat->atime.second = (time & 31) * 2;
	minutes = 0;
	if ((signed char)offset < 0) {
		offset &= ~0x80;
		if (offset > 63)
			minutes = 1920 - 15 * offset;
		else
			minutes = -15 * offset;
	}
	adjustFatDateTimeMinutes(&stat->atime, minutes);
}

int UVFAT_WriteDevice(SceExfatfsFileSystem *fileSystem, SceUInt64 sector,
	SceSize size, const void *buffer)
{
	SceExfatfsDrive *drive;
	SceVfsVnode *vp;
	SceSize writeSize;
	int result, unlockResult;

	if (size & 0x1ff) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 2032, "Illegal write size(%lld)\n", (SceUInt64)size);
		return -32;
	}
	drive = fileSystem->drive;
	if (!drive)
		return -12;
	vp = drive->deviceInfo.device_vnode;
	if (!drive->deviceInfo.device_file || !vp || drive->deviceInfo.device_fd == -1)
		return -12;
	SCE_DBG_ASSERT(vp->core.ops != 0, 2057);
	result = vfsLockVnode(vp);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2060, "%s: _lock_device_vnode failed[%#x]\n", "UVFAT_WriteDevice", result);
		return -12;
	}
	writeSize = 0;
	result = _exfatfs_write_device_file(fileSystem->drive, buffer, size,
		sector << fileSystem->BytePerSectorShift, &writeSize);
	if (result < 0 && !_exfatfs_cleanup_device_file_buffer(fileSystem->drive)) {
		writeSize = 0;
		result = _exfatfs_write_device_file(fileSystem->drive, buffer, size,
			sector << fileSystem->BytePerSectorShift, &writeSize);
		SCE_DBG_PRINTF_LEVEL(1, 0, 2072, "%s: retry after cleanup result[%#x]\n", "UVFAT_WriteDevice", result);
	}
	unlockResult = vfsUnlockVnode(vp);
	if (unlockResult < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2077, "%s: _unlock_device_vnode failed[%#x]\n", "UVFAT_WriteDevice", unlockResult);
	if (result < 0) {
		if (result != (int)0x8032001a) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2085, "%s: convert error[%#x] -> internal error(%d)\n",
				"UVFAT_WriteDevice", result, -12);
			result = -12;
		}
	} else {
		result = 0;
		if (writeSize != size) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2089, "%s: written[%#x] != size[%#x]\n", "UVFAT_WriteDevice", writeSize, size);
			result = -12;
		}
	}
	return result;
}

int flushFatCache(SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsBootSector *boot;
	SceExfatfsFatCache *cache;
	unsigned int entrySize, fatSectors, fatCount, firstFat, copies;
	unsigned int size, sector, i;
	int activeFat, result;

	if (!fileSystem->fatDirty)
		return 0;
	boot = fileSystem->PBRBuf;
	if (fileSystem->FATBits == 12) {
		unsigned int fatSize = fileSystem->sectorSize * boot->fat16.parameters.fatSectors16;
		unsigned int allocationSize = fatSize + 512;
		void *allocation = allocateFatBuffer(allocationSize);
		unsigned char *buffer, *packed;

		if (!allocation)
			return -15;
		buffer = (void *)(((SceUIntPtr)allocation + 511) >> 9 << 9);
		memset(buffer, 0, fatSize);
		packed = buffer;
		for (i = 0; i != 4; i++) {
			SceUInt16 *entries;
			unsigned int count, pairs, j;

			cache = fileSystem->fatCacheHead;
			while (cache && cache->deviceSector != i)
				cache = cache->next;
			if (!cache)
				break;
			entries = cache->buffer;
			count = cache->clusterCount;
			pairs = count >> 1;
			for (j = 0; j < pairs; j++) {
				packed[3 * j] = entries[2 * j];
				packed[3 * j + 1] = ((entries[2 * j] >> 8) & 15) |
					(entries[2 * j + 1] << 4);
				packed[3 * j + 2] = entries[2 * j + 1] >> 4;
			}
			packed += 3 * pairs;
			if (count & 1) {
				packed[0] = entries[2 * pairs];
				packed[1] = (entries[2 * pairs] >> 8) & 15;
				packed++;
			}
			cache->dirty = 0;
		}
		sector = boot->fat16.parameters.reservedSectorCount;
		for (i = 0; i < boot->fat16.parameters.numberOfFats; i++) {
			result = UVFAT_WriteDevice(fileSystem, sector, fatSize, buffer);
			if (result) {
				freeFatBuffer(allocation, allocationSize);
				return result;
			}
			sector += boot->fat16.parameters.fatSectors16;
		}
		freeFatBuffer(allocation, allocationSize);
	} else {
		if (fileSystem->FATBits == 16) {
			entrySize = 2;
			activeFat = -1;
			fatSectors = boot->fat16.parameters.fatSectors16;
			fatCount = boot->fat16.parameters.numberOfFats;
		} else {
			entrySize = 4;
			if (fileSystem->fileSystemType == 1) {
				activeFat = -1;
				fatCount = boot->fat32.parameters.numberOfFats;
				fatSectors = boot->fat32.fatSectors32;
			} else {
				activeFat = boot->exfat.volumeFlags & 1;
				fatCount = boot->exfat.numberOfFats;
				fatSectors = boot->exfat.fatLength;
			}
		}
		copies = fatCount - 1;
		if (activeFat == -1)
			firstFat = 0;
		else {
			firstFat = activeFat;
			copies = 0;
		}
		firstFat *= fatSectors;
		for (cache = fileSystem->fatCacheHead; cache; cache = cache->next) {
			if (!cache->dirty)
				continue;
			size = cache->clusterCount * entrySize;
			if (fileSystem->fatBufSize != size)
				size = fileSystem->sectorSize *
					((fileSystem->sectorSize - 1 + size) / fileSystem->sectorSize);
			if (activeFat != -1 || fatCount) {
				sector = firstFat;
				i = copies;
				for (;;) {
					result = UVFAT_WriteDevice(fileSystem,
						(SceUInt32)(sector + cache->deviceSector), size, cache->buffer);
					if (result)
						return result;
					sector += fatSectors;
					if (!i)
						break;
					i--;
				}
			}
			cache->dirty = 0;
		}
	}
	fileSystem->fatDirty = 0;
	return 0;
}

int UVFAT_ReadFATBuf(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster)
{
	SceUInt32 totalClusters = fileSystem->clusterCount + 2;
	SceExfatfsBootSector *boot = fileSystem->PBRBuf;
	SceExfatfsFatCache *cache, *previous;
	SceSize fatBufSize;
	unsigned int fatSector, i;
	int result;

	if (cluster >= totalClusters) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 2304, "%s: clusterNo over range(cn = %d)\n", "UVFAT_ReadFATBuf", cluster);
		return -32;
	}
	if (fileSystem->fileSystemType == 2) {
		const unsigned char *fatOffset = (const void *)&boot->exfat.fatOffset;
		const unsigned char *flags = (const void *)&boot->exfat.volumeFlags;

		fatSector = ((SceUInt32)fatOffset[2] << 16) |
			((SceUInt32)fatOffset[3] << 24) | fatOffset[0] |
			(fatOffset[1] << 8);
		if (flags[0] & 1) {
			const unsigned char *fatLength = (const void *)&boot->exfat.fatLength;

			fatSector += ((SceUInt32)fatLength[2] << 16) |
				((SceUInt32)fatLength[3] << 24) | fatLength[0] |
				(fatLength[1] << 8);
		}
	} else {
		fatSector = boot->fat16.parameters.reservedSectorCount;
	}
	fatBufSize = fileSystem->fatBufSize;
	for (cache = fileSystem->fatCacheHead; cache; cache = cache->next) {
		if (cluster < cache->firstCluster || cluster >= cache->firstCluster + cache->clusterCount)
			continue;
		if (cache != fileSystem->fatCacheHead) {
			for (previous = fileSystem->fatCacheHead; previous->next != cache; previous = previous->next)
				;
			previous->next = cache->next;
			cache->next = fileSystem->fatCacheHead;
			fileSystem->fatCacheHead = cache;
		}
		return 0;
	}
	if (fileSystem->FATBits == 12) {
		unsigned int readSize, allocationSize, nextCluster, cacheIndex;
		void *allocation;
		unsigned char *packed;

		SCE_DBG_PRINTF_LEVEL(2, 0, 2356, "%s: FAT12, fileSystem[%p], cn[%#lx], assignName(%s)\n",
			"UVFAT_ReadFATBuf", fileSystem, cluster, fileSystem->drive->assignName);
		if (2 * totalClusters > 4 * fatBufSize) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 2358, "buffer empty for FAT12\n");
			return -15;
		}
		readSize = fatBufSize * ((fatBufSize - 1 + ((3 * totalClusters) >> 1) +
			(totalClusters & 1)) / fatBufSize);
		allocationSize = readSize + 512;
		allocation = allocateFatBuffer(allocationSize);
		if (!allocation)
			return -15;
		packed = (void *)(((SceUIntPtr)allocation + 511) >> 9 << 9);
		result = UVFAT_ReadDevice(fileSystem, fatSector, readSize, packed);
		if (result)
			goto freePacked;
		nextCluster = 0;
		for (cacheIndex = 0; cacheIndex != 4; cacheIndex++) {
			SceUInt16 *entries;
			unsigned int count, pairs;

			for (i = 0; i != 4 && fileSystem->fatCache[i].buffer; i++)
				;
			if (i == 4) {
				SCE_DBG_PRINTF_LEVEL(2, 15, 2388, "No empty buffer for FAT12\n");
				result = -15;
				goto freePacked;
			}
			cache = &fileSystem->fatCache[i];
			if (cache->buffer) {
				SCE_DBG_PRINTF_LEVEL(2, 15, 2396, "FAT buf is already allocated\n");
				result = -15;
				goto freePacked;
			}
			cache->allocation = allocateFatBuffer(fatBufSize + 512);
			if (!cache->allocation) {
				result = -15;
				goto freePacked;
			}
			cache->firstCluster = nextCluster;
			cache->deviceSector = cacheIndex;
			cache->buffer = (void *)(((SceUIntPtr)cache->allocation + 511) >> 9 << 9);
			entries = cache->buffer;
			count = fatBufSize >> 1;
			if (count > totalClusters - nextCluster) {
				cache->clusterCount = totalClusters - nextCluster;
				while (nextCluster < totalClusters) {
					*entries++ = packed[0] | ((packed[1] & 15) << 8);
					nextCluster++;
					if (nextCluster == totalClusters) {
						packed++;
						break;
					}
					*entries++ = (packed[1] >> 4) | (packed[2] << 4);
					packed += 3;
					nextCluster++;
				}
			} else {
				cache->clusterCount = count;
				pairs = fatBufSize >> 2;
				for (i = 0; i < pairs; i++) {
					entries[2 * i] = packed[3 * i] | ((packed[3 * i + 1] & 15) << 8);
					entries[2 * i + 1] = (packed[3 * i + 1] >> 4) | (packed[3 * i + 2] << 4);
				}
				packed += pairs * 3;
				nextCluster += count;
			}
			cache->next = 0;
			if (fileSystem->fatCacheHead) {
				previous = fileSystem->fatCacheHead;
				while (previous->next)
					previous = previous->next;
				previous->next = cache;
			} else {
				fileSystem->fatCacheHead = cache;
			}
			if (nextCluster >= totalClusters)
				break;
		}
		result = 0;
freePacked:
		freeFatBuffer(allocation, allocationSize);
		return result;
	} else {
		unsigned int count, cacheNumber, firstCluster, sectorOffset, remainder;

		SCE_DBG_PRINTF_LEVEL(2, 0, 2501, "%s: fileSystem[%p], cn[%#lx], type(%d), assignName(%s)\n",
			"UVFAT_ReadFATBuf", fileSystem, cluster, fileSystem->fileSystemType, fileSystem->drive->assignName);
		count = fatBufSize / (fileSystem->FATBits == 32 ? 4 : 2);
		cacheNumber = cluster / count;
		firstCluster = count * cacheNumber;
		sectorOffset = (fatBufSize >> fileSystem->BytePerSectorShift) * cacheNumber;
		remainder = totalClusters % count;
		if (remainder && cluster >= count * (totalClusters / count))
			count = remainder;
		for (i = 0; i != 4 && fileSystem->fatCache[i].buffer; i++)
			;
		if (i == 4) {
			cache = fileSystem->fatCacheHead;
			if (!cache)
				return -15;
			while (cache->next)
				cache = cache->next;
			if (fileSystem->fatDirty && fileSystem->FATBits != 12 && cache->dirty) {
				result = flushFatCache(fileSystem);
				if (result) {
					SCE_DBG_PRINTF_LEVEL(2, 15, 2541, "FAT flush fail\n");
					return result;
				}
			}
			if (cache == fileSystem->fatCacheHead)
				fileSystem->fatCacheHead = cache->next;
			else {
				for (previous = fileSystem->fatCacheHead; previous->next != cache; previous = previous->next)
					;
				previous->next = cache->next;
			}
			memset(cache->buffer, 0, fatBufSize);
		} else {
			cache = &fileSystem->fatCache[i];
		}
		if (!cache->allocation) {
			cache->allocation = allocateFatBuffer(fatBufSize + 512);
			if (!cache->allocation)
				return -15;
			cache->buffer = (void *)(((SceUIntPtr)cache->allocation + 511) >> 9 << 9);
		}
		cache->deviceSector = sectorOffset + fatSector;
		cache->firstCluster = firstCluster;
		cache->clusterCount = count;
		result = UVFAT_ReadDevice(fileSystem, (SceUInt32)(sectorOffset + fatSector), fatBufSize, cache->buffer);
		if (!result) {
			cache->next = fileSystem->fatCacheHead;
			fileSystem->fatCacheHead = cache;
		}
		return result;
	}
}

int UVFAT_WriteCblock(SceUInt32 cluster, SceUInt32 block,
	SceExfatfsFileSystem *fileSystem, const void *buffer)
{
	SceUInt32 clusterSectorCount, rootClusterOffset;
	SceUInt64 sector;
	int result;

	if (fileSystem->fileSystemType != 2) {
		if (fileSystem->FATBits == 32) {
			clusterSectorCount = ((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat16.parameters.sectorsPerCluster;
			rootClusterOffset = fileSystem->firstClusterOfRoot - 2;
		} else {
			rootClusterOffset = 0;
			clusterSectorCount = ((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat16.parameters.sectorsPerCluster;
		}
		if (cluster > 1)
			goto dataCluster;
	} else {
		clusterSectorCount = fileSystem->clusterSectorCount;
		rootClusterOffset = fileSystem->firstClusterOfRoot - 2;
		if (cluster > 1)
			goto dataCluster;
	}
	sector = (SceUInt64)(cluster + rootClusterOffset) * clusterSectorCount +
		fileSystem->dataAreaStartSector;
	goto writeBlock;
dataCluster:
	sector = (SceUInt64)(cluster - 2) * clusterSectorCount +
		(SceUInt32)(fileSystem->dataAreaStartSector + fileSystem->rootDirectorySectors);
writeBlock:
	sector += (fileSystem->fileBufferSize >> fileSystem->BytePerSectorShift) * block;
	result = UVFAT_WriteDevice(fileSystem, sector, fileSystem->fileBufferSize, buffer);
	if (result)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1010,
			"%s: UVFAT_WriteDevice failed[%#x], sector[%#llx], cluster[%#lx], index_in_cluster[%#x]\n",
			"UVFAT_WriteCblock", result, sector, cluster, block);
	return result;
}

int UVFAT_FlushFdBuf(SceExfatfsFile *file, SceExfatfsFileSystem *fileSystem)
{
	unsigned int block;
	int result = file->cacheFlags;

	if (result) {
		SceUInt32 cachedCluster = file->cacheCluster;
		SceUInt64 cacheIndex = file->cacheIndex;

		if (cachedCluster)
			block = cacheIndex %
				((fileSystem->clusterSectorCount * fileSystem->sectorSize) / fileSystem->fileBufferSize);
		else
			block = cacheIndex;
		SCE_DBG_PRINTF_LEVEL(2, 0, 1041,
			"%s: Cluster[%#lx], CachedCluster[%#lx], clustBlockNo[%#x], blockIndex[%#llx]\n",
			"UVFAT_FlushFdBuf", file->Cluster, cachedCluster, block, cacheIndex);
#if FW_DEBUG
		result = UVFAT_WriteCblock(file->cacheCluster, block, fileSystem, file->Buf);
#else
		{
			SceUInt32 clusterSectorCount, rootClusterOffset, cluster, dataSector;
			SceUInt64 sector;

			if (fileSystem->fileSystemType == 2) {
				clusterSectorCount = fileSystem->clusterSectorCount;
				rootClusterOffset = fileSystem->firstClusterOfRoot - 2;
			} else if (fileSystem->FATBits == 32) {
				clusterSectorCount = ((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat16.parameters.sectorsPerCluster;
				rootClusterOffset = fileSystem->firstClusterOfRoot - 2;
			} else {
				rootClusterOffset = 0;
				clusterSectorCount = ((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat16.parameters.sectorsPerCluster;
			}
			if (cachedCluster <= 1) {
				cluster = cachedCluster + rootClusterOffset;
				dataSector = fileSystem->dataAreaStartSector;
			} else {
				cluster = cachedCluster - 2;
				dataSector = fileSystem->dataAreaStartSector + fileSystem->rootDirectorySectors;
			}
			sector = (SceUInt64)cluster * clusterSectorCount + dataSector +
				block * (fileSystem->fileBufferSize >> fileSystem->BytePerSectorShift);
			result = UVFAT_WriteDevice(fileSystem, sector, fileSystem->fileBufferSize, file->Buf);
		}
#endif
		if (result) {
			file->cacheFlags |= 2;
			SCE_DBG_PRINTF_LEVEL(1, 0, 1050, "%s: UVFAT_WriteCblock() failed[%#x]\n", "UVFAT_FlushFdBuf", result);
		} else {
			file->cacheFlags = 0;
			touchFnodeModificationTime(file->fnode);
		}
	}
	return result;
}

int releaseMediaStorage(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, int mountMode)
{
	unsigned int fileCount = getInitializedFileCount(sharedResource);
	SceExfatfsFile *file;
	SceExfatfsFatCache *cache;
	unsigned int i;
	int inUse = 0, result;

	if (!fileSystem->devname)
		return 0;
	if (fileCount) {
		for (i = 0; i != fileCount; i++) {
			file = &sharedResource->files[i];
			if (file->mode && file->fnode && file->fnode->fileSystem == fileSystem)
				inUse = 1;
		}
	}
#define CHECK_MOUNT_ACTIVITY(index) \
	if (sharedResource->mountActivity[index].active && \
	    sharedResource->mountActivity[index].fileSystem == fileSystem) \
		inUse = 1
	CHECK_MOUNT_ACTIVITY(0);
	CHECK_MOUNT_ACTIVITY(1);
	CHECK_MOUNT_ACTIVITY(2);
	CHECK_MOUNT_ACTIVITY(3);
	CHECK_MOUNT_ACTIVITY(4);
	CHECK_MOUNT_ACTIVITY(5);
	CHECK_MOUNT_ACTIVITY(6);
	CHECK_MOUNT_ACTIVITY(7);
	CHECK_MOUNT_ACTIVITY(8);
	CHECK_MOUNT_ACTIVITY(9);
	CHECK_MOUNT_ACTIVITY(10);
	CHECK_MOUNT_ACTIVITY(11);
	CHECK_MOUNT_ACTIVITY(12);
	CHECK_MOUNT_ACTIVITY(13);
	CHECK_MOUNT_ACTIVITY(14);
#undef CHECK_MOUNT_ACTIVITY
	if (fileSystem->state > 0) {
		if ((unsigned char)mountMode == 2) {
#if FW_DEBUG
			if (fileSystem->fileSystemType == 2) {
				SceExfatfsBootSector *boot = fileSystem->PBRBuf;

				result = updateallocbitmaps(fileSystem);
				if (result) {
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1416, "UVFAT_WriteManagementAreaExfat",
						"Cannot flush allocationbitmap(%d)\n", result);
					return result;
				}
				if (boot->exfat.percentInUse != 255) {
					boot->exfat.percentInUse = 255;
					fileSystem->PBRDirty = 1;
				}
			}
			if (fileSystem->PBRDirty) {
				result = UVFAT_WriteDevice(fileSystem, 0, 512, fileSystem->PBRBuf);
				if (result)
					return result;
				fileSystem->PBRDirty = 0;
			}
			result = flushFatCache(fileSystem);
			if (result)
				return result;
			fileSystem->fatDirty = 0;
#else
			result = flushFileSystemMetadata(fileSystem);
			if (result)
				return result;
#endif
		}
		if (!inUse) {
			freeFatBuffer(fileSystem->file.BufTop, fileSystem->fileBufferSize + 512);
			fileSystem->file.Buf = 0;
			fileSystem->file.BufTop = 0;
		}
	}
	if (fileSystem->bitmapAllocation)
		freeFatBuffer(fileSystem->bitmapAllocation, fileSystem->sectorSize + 512);
	fileSystem->bitmapBuffer = 0;
	fileSystem->bitmapAllocation = 0;
	freeUpcaseStorage(fileSystem);
#define FREE_FAT_CACHE() do { \
	SceSize fatBufSize; \
	void *allocation; \
	\
	cache = fileSystem->fatCacheHead; \
	if (!cache) \
		return 0; \
	fatBufSize = fileSystem->fatBufSize; \
	allocation = cache->allocation; \
	fileSystem->fatCacheHead = cache->next; \
	freeFatBuffer(allocation, fatBufSize + 512); \
	memset(cache, 0, sizeof(*cache)); \
} while (0)
	FREE_FAT_CACHE();
	FREE_FAT_CACHE();
	FREE_FAT_CACHE();
	FREE_FAT_CACHE();
#undef FREE_FAT_CACHE
	return 0;
}

int UVFAT_MediaInitialize(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, int mountMode)
{
	unsigned int fileCount = getInitializedFileCount(sharedResource);
	unsigned int mode = mountMode & 0xffU;
	SceExfatfsBootSector *boot, *currentBoot;
	SceExfatfsFile *file;
	SceExfatfsFatCache *cache;
	SceUInt32 fatSectors, totalSectors, maxEntries, bufferSize;
	SceUInt64 fatAreaSize;
	const unsigned char *bytesPerSector, *rootEntryCount, *totalSectors16;
	const unsigned char *volumeLength, *clusterHeapOffset, *rootCluster, *fatOffset;
	SceSize allocationSize;
	unsigned int i;
	int inUse = 0, result;

	if (mode != 1) {
		if (mode != 2) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 690, "%s: unexpected mount_mode[%#x]\n",
				"UVFAT_MediaInitialize", mountMode);
			return -32;
		}
		mode = 6;
	}
	if (!fileSystem->devname) {
		if ((mode & ~2U) != 1)
			return -8;
		fileSystem->state = 2;
		return 0;
	}
	if (fileCount) {
		for (i = 0; i != fileCount; i++) {
			file = &sharedResource->files[i];
			if (file->mode && file->fnode && file->fnode->fileSystem == fileSystem)
				inUse = 1;
		}
	}
#define CHECK_MOUNT_ACTIVITY(index) \
	if (sharedResource->mountActivity[index].active && \
	    sharedResource->mountActivity[index].fileSystem == fileSystem) \
		inUse = 1
	CHECK_MOUNT_ACTIVITY(0);
	CHECK_MOUNT_ACTIVITY(1);
	CHECK_MOUNT_ACTIVITY(2);
	CHECK_MOUNT_ACTIVITY(3);
	CHECK_MOUNT_ACTIVITY(4);
	CHECK_MOUNT_ACTIVITY(5);
	CHECK_MOUNT_ACTIVITY(6);
	CHECK_MOUNT_ACTIVITY(7);
	CHECK_MOUNT_ACTIVITY(8);
	CHECK_MOUNT_ACTIVITY(9);
	CHECK_MOUNT_ACTIVITY(10);
	CHECK_MOUNT_ACTIVITY(11);
	CHECK_MOUNT_ACTIVITY(12);
	CHECK_MOUNT_ACTIVITY(13);
	CHECK_MOUNT_ACTIVITY(14);
#undef CHECK_MOUNT_ACTIVITY
	if (inUse)
		return 0;
	if (fileSystem->mode != 1 && fileSystem->state != 0)
		return 0;
	boot = fileSystem->PBRBuf;
	result = UVFAT_ReadDevice(fileSystem, 0, 512, boot);
	if (result) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1189, "UVFAT_ReadManagementArea",
			"%s: UVFAT_ReadDevice failed[%#x]\n", "UVFAT_ReadManagementArea", result);
		goto fail;
	}
	if (boot->fat16.signature != 0xaa55) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1198, "UVFAT_ReadManagementArea",
			"Boot signature is wrong[0x%x, 0x%x]\n",
			boot->fat16.signature & 255, boot->fat16.signature >> 8);
		_exfatfs_notify_invalid_boot_signature(fileSystem->drive);
		result = -17;
		goto fail;
	}
	fileSystem->PBRDirty = 0;
	memset(fileSystem->fatCache, 0, sizeof(fileSystem->fatCache));
	fileSystem->freeClusterCountValid = 0;
	fileSystem->fatCacheHead = NULL;
	fileSystem->freeClusterCount = 0;
	fileSystem->nextFreeCluster = 2;
	fileSystem->fakeFreeClusterCount = 0;
	fileSystem->fatBufSize = 2048;
	bytesPerSector = (const void *)&boot->fat16.parameters.bytesPerSector;
	if (bytesPerSector[0] || bytesPerSector[1]) {
		fileSystem->fileSystemType = 1;
		if ((bytesPerSector[0] | (bytesPerSector[1] << 8)) != 512) {
			_exfatfs_notify_unsupported_sector_size(fileSystem->drive);
			result = -17;
			goto fail;
		}
		fileSystem->BytePerSectorShift = 9;
		fileSystem->sectorSize = 512;
		fileSystem->clusterSectorCount = boot->fat16.parameters.sectorsPerCluster;
		bufferSize = (SceUInt32)boot->fat16.parameters.sectorsPerCluster << 9;
		if (bufferSize >= 512)
			bufferSize = 512;
		allocationSize = bufferSize + 512;
		fileSystem->fileBufferSize = bufferSize;
		fileSystem->file.BufTop = allocateFatBuffer(allocationSize);
		if (!fileSystem->file.BufTop) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1291, "UVFAT_ReadManagementArea", "No memory\n");
			result = -15;
			goto fail;
		}
		currentBoot = fileSystem->PBRBuf;
		fileSystem->file.Buf = (void *)(((uintptr_t)fileSystem->file.BufTop + 511) & ~511U);
		if (!currentBoot) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2243, "UVFAT_GetDevSectors",
				"%s(): NULL pointer found\n", "UVFAT_GetDevSectors");
			goto badFatDevice;
		}
		totalSectors16 = (const void *)&currentBoot->fat16.parameters.totalSectors16;
		if (!currentBoot->fat16.parameters.totalSectors32 &&
		    !(totalSectors16[0] | (totalSectors16[1] << 8)))
			goto badFatDevice;
		if (boot->fat16.parameters.rootEntryCount) {
			rootEntryCount = (const void *)&boot->fat16.parameters.rootEntryCount;
			fatSectors = boot->fat16.parameters.fatSectors16;
			fileSystem->FATBits = 0;
			fileSystem->dataAreaStartSector = boot->fat16.parameters.reservedSectorCount +
				fatSectors * boot->fat16.parameters.numberOfFats;
			fileSystem->rootDirectorySectors =
				(32U * (rootEntryCount[0] + 16 + (rootEntryCount[1] << 8)) - 1) >> 9;
		} else {
			fileSystem->drive->state |= 0x1000;
			fileSystem->FATBits = 32;
			fileSystem->ClusterMAX = 0x0ffffff7;
			fatSectors = boot->fat32.fatSectors32;
			fileSystem->rootDirectorySectors = 0;
			fileSystem->dataAreaStartSector = boot->fat16.parameters.reservedSectorCount +
				fatSectors * boot->fat16.parameters.numberOfFats;
			fileSystem->firstClusterOfRoot = boot->fat32.rootCluster;
		}
		totalSectors = currentBoot->fat16.parameters.totalSectors32;
		if (!totalSectors)
			totalSectors = totalSectors16[0] | (totalSectors16[1] << 8);
		fileSystem->clusterCount = (totalSectors - fileSystem->dataAreaStartSector -
			fileSystem->rootDirectorySectors) / boot->fat16.parameters.sectorsPerCluster;
		if (fileSystem->FATBits != 32) {
			if (strncmp((const char *)boot->fat16.fileSystemName, "FAT12   ", 8) &&
			    strncmp((const char *)boot->fat16.fileSystemName, "UVFAT12 ", 8)) {
				fileSystem->drive->state |= 0x8000;
				fileSystem->FATBits = 16;
				fileSystem->ClusterMAX = 0xfff7;
			} else {
				fileSystem->FATBits = 12;
				fileSystem->ClusterMAX = 0xff7;
			}
		}
		maxEntries = fatSectors * 8 * fileSystem->sectorSize / fileSystem->FATBits;
		if (fileSystem->clusterCount + 2 > maxEntries)
			fileSystem->clusterCount = maxEntries - 2;
		totalSectors = currentBoot->fat16.parameters.totalSectors32;
		if (!totalSectors)
			totalSectors = totalSectors16[0] | (totalSectors16[1] << 8);
		fileSystem->devSectors = totalSectors;
		fileSystem->fatDirty = 0;
		goto managementReady;

badFatDevice:
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1298, "UVFAT_ReadManagementArea",
			"ERROR: Cannot get device size.\n");
	} else {
		fileSystem->fileSystemType = 2;
		if ((unsigned int)(boot->exfat.bytesPerSectorShift - 9) > 3) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1104, "UVFAT_ReadManagementArea_ex",
				"Over range : BytePerSectorShift\n");
			_exfatfs_notify_invalid_sector_shift(fileSystem->drive,
				1U << boot->exfat.bytesPerSectorShift);
			result = -17;
			goto fail;
		}
		if (boot->exfat.bytesPerSectorShift + boot->exfat.sectorsPerClusterShift > 25) {
			result = -17;
			goto fail;
		}
		fileSystem->BytePerSectorShift = boot->exfat.bytesPerSectorShift;
		fileSystem->sectorSize = 1U << boot->exfat.bytesPerSectorShift;
		if (fileSystem->sectorSize > 2048) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1116, "UVFAT_ReadManagementArea_ex",
				"fatbufSize need sectorSize or more\n");
			result = -32;
			goto fail;
		}
		fileSystem->clusterSectorCount = 1U << boot->exfat.sectorsPerClusterShift;
		bufferSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
		if (bufferSize >= 512)
			bufferSize = 512;
		fileSystem->fileBufferSize = bufferSize;
		allocationSize = bufferSize + 512;
		volumeLength = (const void *)&boot->exfat.volumeLength;
		fileSystem->devSectors = ((SceUInt64)((SceUInt32)volumeLength[6] << 16 |
			(SceUInt32)volumeLength[7] << 24 | volumeLength[4] | volumeLength[5] << 8) << 32) |
			((SceUInt32)volumeLength[2] << 16 | (SceUInt32)volumeLength[3] << 24 |
			volumeLength[0] | volumeLength[1] << 8);
		fileSystem->file.BufTop = allocateFatBuffer(allocationSize);
		if (!fileSystem->file.BufTop) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1131, "UVFAT_ReadManagementArea_ex", "No memory\n");
			result = -15;
			goto fail;
		}
		currentBoot = fileSystem->PBRBuf;
		fileSystem->file.Buf = (void *)(((uintptr_t)fileSystem->file.BufTop + 511) & ~511U);
		if (!currentBoot) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2229, "UVFAT_GetDevSectors_ex",
				"%s(): NULL pointer found\n", "UVFAT_GetDevSectors_ex");
			goto badExfatDevice;
		}
		volumeLength = (const void *)&currentBoot->exfat.volumeLength;
		if (!((SceUInt32)volumeLength[6] << 16 | (SceUInt32)volumeLength[7] << 24 |
		      volumeLength[4] | volumeLength[5] << 8 | (SceUInt32)volumeLength[2] << 16 |
		      (SceUInt32)volumeLength[3] << 24 | volumeLength[0] | volumeLength[1] << 8))
			goto badExfatDevice;
		fileSystem->drive->state |= 0x1000;
		fileSystem->FATBits = 32;
		fileSystem->ClusterMAX = 0xfffffff7;
		fileSystem->rootDirectorySectors = 0;
		clusterHeapOffset = (const void *)&boot->exfat.clusterHeapOffset;
		fileSystem->dataAreaStartSector = (SceUInt32)clusterHeapOffset[2] << 16 |
			(SceUInt32)clusterHeapOffset[3] << 24 | clusterHeapOffset[0] | clusterHeapOffset[1] << 8;
		rootCluster = (const void *)&boot->exfat.rootCluster;
		fileSystem->firstClusterOfRoot = (SceUInt32)rootCluster[2] << 16 |
			(SceUInt32)rootCluster[3] << 24 | rootCluster[0] | rootCluster[1] << 8;
		fileSystem->clusterCount = ((((SceUInt64)((SceUInt32)volumeLength[6] << 16 |
			(SceUInt32)volumeLength[7] << 24 | volumeLength[4] | volumeLength[5] << 8) << 32) |
			((SceUInt32)volumeLength[2] << 16 | (SceUInt32)volumeLength[3] << 24 |
			volumeLength[0] | volumeLength[1] << 8)) -
			fileSystem->dataAreaStartSector) / fileSystem->clusterSectorCount;
		fileSystem->fatDirty = 0;
		goto managementReady;

badExfatDevice:
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1138, "UVFAT_ReadManagementArea_ex",
			"ERROR: Cannot get device size.\n");
	}
	freeFatBuffer(fileSystem->file.BufTop, allocationSize);
	fileSystem->file.Buf = NULL;
	fileSystem->file.BufTop = NULL;
	result = -34;
	goto fail;

managementReady:
	if (fileSystem->fileSystemType == 2) {
		result = _check_allocation_bitmap(sharedResource, fileSystem);
		if (result)
			goto fail;
		result = UVFAT_readupcasetable(sharedResource, fileSystem);
		if (result)
			goto fail;
	} else if (mountMode & 0x200) {
		currentBoot = fileSystem->PBRBuf;
		fatAreaSize = (SceUInt64)(fileSystem->FATBits == 32 ?
			currentBoot->fat32.fatSectors32 : currentBoot->fat16.parameters.fatSectors16)
			<< fileSystem->BytePerSectorShift;
		SCE_DBG_PRINTF_LEVEL(2, 0, 768,
			"%s: mode[%#x], clusterCount[%#lx], fatBufSize[%#x], ClusterMAX[%#lx], FATBits(%d), _fat_area_size[%#llx]\n",
			"UVFAT_MediaInitialize", mountMode, fileSystem->clusterCount,
			fileSystem->fatBufSize, fileSystem->ClusterMAX, fileSystem->FATBits, fatAreaSize);
		if (fatAreaSize > 0x20000) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 771, "%s: too large FAT area[%#llx]\n",
				"UVFAT_MediaInitialize", fatAreaSize);
			result = -15;
			goto freeFileBuffer;
		}
		cache = NULL;
		if (!fileSystem->fatCache[0].buffer)
			cache = &fileSystem->fatCache[0];
		else if (!fileSystem->fatCache[1].buffer)
			cache = &fileSystem->fatCache[1];
		else if (!fileSystem->fatCache[2].buffer)
			cache = &fileSystem->fatCache[2];
		else if (!fileSystem->fatCache[3].buffer)
			cache = &fileSystem->fatCache[3];
		cache->allocation = allocateFatBuffer((SceSize)fatAreaSize + 512);
		if (!cache->allocation) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 777, "%s: No memory\n", "UVFAT_MediaInitialize");
			result = -15;
			goto freeFileBuffer;
		}
		cache->firstCluster = 0;
		cache->clusterCount = 0;
		cache->buffer = (void *)(((uintptr_t)cache->allocation + 511) & ~511U);
		currentBoot = fileSystem->PBRBuf;
		fatOffset = (const void *)&currentBoot->exfat.fatOffset;
		totalSectors = fileSystem->fileSystemType == 2 ?
			((SceUInt32)fatOffset[2] << 16 | (SceUInt32)fatOffset[3] << 24 |
			 fatOffset[0] | fatOffset[1] << 8) : currentBoot->fat16.parameters.reservedSectorCount;
		cache->deviceSector = totalSectors;
		result = UVFAT_ReadDevice(fileSystem, totalSectors, (SceSize)fatAreaSize, cache->buffer);
		if (result) {
			freeFatBuffer(cache->allocation, (SceSize)fatAreaSize + 512);
			cache->buffer = NULL;
			cache->allocation = NULL;
			goto freeFileBuffer;
		}
		cache->next = fileSystem->fatCacheHead;
		fileSystem->fatCacheHead = cache;
		fileSystem->fatBufSize = fatAreaSize;
		cache->clusterCount = fileSystem->clusterCount + 2;
	}
	fileSystem->state = 2;
	return 0;

freeFileBuffer:
	freeFatBuffer(fileSystem->file.BufTop, fileSystem->fileBufferSize + 512);
	fileSystem->file.Buf = NULL;
	fileSystem->file.BufTop = NULL;
fail:
	fileSystem->state = 0;
	releaseMediaStorage(sharedResource, fileSystem, mountMode);
	return result;
}

int flushFileSystemMetadata(SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsBootSector *boot;
	int result;

	if (fileSystem->fileSystemType == 2) {
		boot = fileSystem->PBRBuf;
		result = updateallocbitmaps(fileSystem);
		if (result) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1416, "UVFAT_WriteManagementAreaExfat",
				"Cannot flush allocationbitmap(%d)\n", result);
			return result;
		}
		if (boot->exfat.percentInUse != 255) {
			boot->exfat.percentInUse = 255;
			fileSystem->PBRDirty = 1;
		} else if (!fileSystem->PBRDirty) {
			goto flushFat;
		}
		result = UVFAT_WriteDevice(fileSystem, 0, 512, fileSystem->PBRBuf);
	} else {
		if (!fileSystem->PBRDirty)
			goto flushFat;
		result = UVFAT_WriteDevice(fileSystem, 0, 512, fileSystem->PBRBuf);
	}
	if (result)
		return result;
	fileSystem->PBRDirty = 0;
flushFat:
	result = flushFatCache(fileSystem);
	if (!result)
		fileSystem->fatDirty = 0;
	return result;
}

SceUInt32 fileSystemClusterSize(SceExfatfsFileSystem *fileSystem)
{
	return fileSystem->clusterSectorCount * fileSystem->sectorSize;
}

int UVFAT_GetFreeClusterCount(SceExfatfsFileSystem *fileSystem, SceUInt32 *count)
{
	SceUInt32 maxClusters, freeClusters = 0, cluster = 0;
	SceUInt32 scanCluster, clusterCount;
	SceExfatfsFatCache *cache;
	int result;

	if (fileSystem->freeClusterCountValid == 1) {
		*count = fileSystem->freeClusterCount;
		return 0;
	}
	maxClusters = fileSystem->clusterCount + 2;
	while (cluster < maxClusters) {
		result = UVFAT_ReadFATBuf(fileSystem, cluster);
		if (result)
			return result;
		cache = fileSystem->fatCacheHead;
		while (cache) {
			if (cache->firstCluster <= cluster) {
				clusterCount = cache->clusterCount;
				if (clusterCount + cache->firstCluster > cluster)
					break;
			}
			cache = cache->next;
		}
		if (!cache)
			return -15;
		scanCluster = cluster;
		if (fileSystem->FATBits == 12 || fileSystem->FATBits == 16) {
			SceUInt16 *entries = cache->buffer;

			if (!cluster) {
				if (!entries[0])
					SCE_DBG_PRINTF_LEVEL(1, 0, 2630, "%s: unexpected [%#hx] for FAT[0]\n",
						"UVFAT_GetFreeClusterCount", entries[0]);
				if (!entries[1])
					SCE_DBG_PRINTF_LEVEL(1, 0, 2633, "%s: unexpected [%#hx] for FAT[1]\n",
						"UVFAT_GetFreeClusterCount", entries[1]);
				clusterCount -= 2;
				entries += 2;
				scanCluster = 2;
			}
			cluster = clusterCount + scanCluster;
			if (clusterCount) {
				entries--;
				do {
					if (!*++entries) {
						if (!freeClusters)
							fileSystem->nextFreeCluster = scanCluster;
						freeClusters++;
					}
					scanCluster++;
				} while (--clusterCount);
			}
		} else {
			SceUInt32 *entries = cache->buffer;

			if (!cluster) {
				if (!entries[0])
					SCE_DBG_PRINTF_LEVEL(1, 0, 2659, "%s: unexpected [%#x] for FAT[0]\n",
						"UVFAT_GetFreeClusterCount", entries[0]);
				if (!entries[1])
					SCE_DBG_PRINTF_LEVEL(1, 0, 2662, "%s: unexpected [%#x] for FAT[1]\n",
						"UVFAT_GetFreeClusterCount", entries[1]);
				clusterCount -= 2;
				entries += 2;
				scanCluster = 2;
			}
			cluster = clusterCount + scanCluster;
			if (clusterCount) {
				entries--;
				do {
					if (!*++entries) {
						if (!freeClusters)
							fileSystem->nextFreeCluster = scanCluster;
						freeClusters++;
					}
					scanCluster++;
				} while (--clusterCount);
			}
		}
	}
	fileSystem->freeClusterCount = freeClusters;
	fileSystem->freeClusterCountValid = 1;
	*count = freeClusters;
	return 0;
}

int UVFAT_SetGetFAT(SceExfatfsFileSystem *fileSystem, int mode,
	SceUInt32 cluster, SceUInt32 *next)
{
	SceExfatfsFatCache *cache;
	SceUInt32 index, oldValue, newValue;
	int result;

	if (fileSystem->nextFreeCluster < fileSystem->firstClusterOfRoot)
		fileSystem->nextFreeCluster = fileSystem->firstClusterOfRoot;
	result = UVFAT_ReadFATBuf(fileSystem, cluster);
	if (result)
		return result;
	cache = fileSystem->fatCacheHead;
	while (cache) {
		if (cluster >= cache->firstCluster &&
		    cluster < cache->firstCluster + cache->clusterCount)
			break;
		cache = cache->next;
	}
	if (!cache)
		return -15;
	index = cluster - cache->firstCluster;
	if (mode != 1) {
		if (fileSystem->FATBits == 32)
			oldValue = ((SceUInt32 *)cache->buffer)[index];
		else
			oldValue = ((SceUInt16 *)cache->buffer)[index];
		*next = oldValue;
		if (oldValue) {
			if (fileSystem->nextFreeCluster == cluster)
				fileSystem->nextFreeCluster = cluster + 1;
		} else if (cluster < fileSystem->nextFreeCluster) {
			fileSystem->nextFreeCluster = cluster;
		}
		return result;
	}
	newValue = *next;
	if (fileSystem->FATBits == 32) {
		oldValue = ((SceUInt32 *)cache->buffer)[index];
		((SceUInt32 *)cache->buffer)[index] = newValue;
	} else {
		oldValue = ((SceUInt16 *)cache->buffer)[index];
		((SceUInt16 *)cache->buffer)[index] = newValue;
	}
	if (oldValue == newValue) {
		if (fileSystem->fileSystemType == 2)
			SCE_DBG_PRINTF_LEVEL(1, 0, 2953,
				"%s: cn[%#lx], next[%#x]->[%#lx], exFAT mountpoint(%s)\n",
				"UV", cluster, oldValue, oldValue, fileSystem->drive->assignName);
		else
			SCE_DBG_PRINTF_LEVEL(1, 0, 2955,
				"%s: cn[%#lx], unexpected next[%#x]->[%#lx], FAT mountpoint(%s)\n",
				"UV", cluster, oldValue, oldValue, fileSystem->drive->assignName);
		return result;
	}
	if (newValue) {
		if (oldValue || fileSystem->fileSystemType == 2)
			goto dirty;
		if (fileSystem->freeClusterCountValid == 1)
			fileSystem->freeClusterCount--;
		if (cluster != fileSystem->nextFreeCluster)
			goto dirty;
		cluster++;
	} else {
		if (fileSystem->fileSystemType == 2)
			goto dirty;
		if (fileSystem->freeClusterCountValid == 1)
			fileSystem->freeClusterCount++;
		if (cluster >= fileSystem->nextFreeCluster)
			goto dirty;
	}
	fileSystem->nextFreeCluster = cluster;
dirty:
	cache->dirty = 1;
	fileSystem->fatDirty = 1;
	return result;
}

int UVFAT_SetEOF(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster)
{
	SceUInt32 next;
	int result;

	if (fileSystem->fileSystemType == 2)
		next = 0xffffffff;
	else if (fileSystem->FATBits == 32)
		next = 0x0fffffff;
	else
		next = 0xffff;
	result = UVFAT_SetGetFAT(fileSystem, 1, cluster, &next);
	if (result)
		SCE_DBG_PRINTF_LEVEL(2, 15, 3044, "Cannot set EOF, FAT[0x%lx]=EOF\n", cluster);
	return result;
}

SceUInt32 UVFAT_GetFatEmptyClust(SceExfatfsFileSystem *fileSystem,
	SceUInt32 startCluster)
{
	SceUInt32 cluster = fileSystem->nextFreeCluster;
	SceUInt32 limit = fileSystem->clusterCount + 2;
	SceUInt32 firstCluster, clusterCount, index, nextFreeCluster, emptyCluster;
	SceExfatfsFatCache *cache;
	int wrapped = 0, updateSearchStart, result;

	if (startCluster >= cluster)
		cluster = startCluster;
	for (;;) {
		if (cluster >= limit && !wrapped) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 3204,
				"%s: startCn[%#lx], scn[%#lx] >= limit[%#lx], check from search_start[%#lx]\n",
				"UVFAT_GetFatEmptyClust", startCluster, cluster, limit,
				fileSystem->nextFreeCluster);
			wrapped = 1;
			cluster = fileSystem->nextFreeCluster;
		}
		if (cluster >= startCluster && wrapped) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 3214, "%s: no empty cluster\n",
				"UVFAT_GetFatEmptyClust");
			fileSystem->nextFreeCluster = limit;
			emptyCluster = 0x0fffffff;
			break;
		}
		result = UVFAT_ReadFATBuf(fileSystem, cluster);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 3227,
				"%s: UVFAT_ReadFATBuf failed[%#x], scn[%#lx]\n",
				"UVFAT_GetFatEmptyClust", result, cluster);
			emptyCluster = 0x0fffffff;
			break;
		}
		cache = fileSystem->fatCacheHead;
		if (!cache) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 3238, "%s: Not found FAT buffer data\n",
				"UVFAT_GetFatEmptyClust");
			emptyCluster = 0x0fffffff;
			break;
		}
		firstCluster = cache->firstCluster;
		clusterCount = cache->clusterCount;
		if (startCluster >= firstCluster) {
			index = startCluster - firstCluster;
			if (index >= clusterCount)
				index = 0;
		} else {
			index = 0;
		}
		nextFreeCluster = fileSystem->nextFreeCluster;
		emptyCluster = index + firstCluster;
		updateSearchStart = nextFreeCluster >= emptyCluster &&
			nextFreeCluster - firstCluster < clusterCount;
		if (fileSystem->FATBits == 32) {
			SceUInt32 *entries = cache->buffer;

			while (index < clusterCount) {
				if (!entries[index])
					break;
				index++;
			}
		} else {
			SceUInt16 *entries = cache->buffer;

			while (index < clusterCount) {
				if (!entries[index])
					break;
				index++;
			}
		}
		if (index >= clusterCount) {
			if (updateSearchStart)
				fileSystem->nextFreeCluster = clusterCount + firstCluster;
			emptyCluster = 0x0fffffff;
		} else {
			emptyCluster = index + firstCluster;
			if (updateSearchStart && nextFreeCluster < emptyCluster)
				fileSystem->nextFreeCluster = emptyCluster;
		}
		if (fileSystem->ClusterMAX > emptyCluster)
			break;
		cluster = clusterCount + firstCluster;
	}
	if (emptyCluster < fileSystem->nextFreeCluster)
		fileSystem->nextFreeCluster = emptyCluster;
	return emptyCluster;
}

int UVFAT_SetFatChain_ex_NFC(SceExfatfsFileSystem *fileSystem, int mode,
	SceUInt32 startCluster, SceUInt32 lastCluster)
{
	SceExfatfsFatCache *cache;
	SceUInt32 *entries;
	SceUInt32 firstCluster, clusterCount, index;
	int complete = 0, result;

	if (fileSystem->fileSystemType != 2) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 3284, "exFAT only!!");
		return -15;
	}
	result = UVFAT_ReadFATBuf(fileSystem, startCluster);
	while (!result) {
		cache = fileSystem->fatCacheHead;
		if (!cache)
			return -15;
		firstCluster = cache->firstCluster;
		if (startCluster < firstCluster)
			goto invalidRange;
		clusterCount = cache->clusterCount;
		if (startCluster >= clusterCount + firstCluster) {
invalidRange:
			SCE_DBG_PRINTF_LEVEL(2, 15, 3306, "Cannot read FAT buffer\n");
			return -15;
		}
		entries = cache->buffer;
		if (clusterCount + firstCluster >= lastCluster) {
			clusterCount = lastCluster - firstCluster;
			complete = 1;
		}
		index = startCluster - firstCluster;
		if (mode == 1) {
			for (; index < clusterCount; index++)
				entries[index] = firstCluster + 1 + index;
		} else if (mode == 2) {
			for (; index < clusterCount; index++)
				entries[index] = 0;
		} else {
			return -15;
		}
		cache->dirty = 1;
		fileSystem->fatDirty = 1;
		if (complete)
			return result;
		startCluster = firstCluster + cache->clusterCount;
		result = UVFAT_ReadFATBuf(fileSystem, startCluster);
	}
	return result;
}
