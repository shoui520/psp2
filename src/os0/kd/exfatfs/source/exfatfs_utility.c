/* Utility routines whose Sony source-file records are absent. */
#include <psp2kern/types.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <string.h>
#include "exfatfs_internal.h"

extern void *allocateHeapBuffer(SceExfatfsDrive *unusedDrive);
extern void *allocateCommonHeapMemory(int unused, SceSize size);
extern void freeHeapBuffer(int unused, void *buffer);

void *allocateFatBuffer(SceSize size)
{
	if (size > 0x800)
		return allocateCommonHeapMemory(0, size);
	return allocateHeapBuffer(NULL);
}

void freeFatBuffer(void *buffer, SceSize size)
{
	(void)size;
	freeHeapBuffer(0, buffer);
}

unsigned int checksumUtf16Name(const SceUInt16 *name, unsigned int length)
{
	unsigned int checksum;
	SceUInt16 index;
	unsigned int character;
	SceUInt16 partial;

	if (!length)
		return length;
	character = *name;
	if (!character)
		return character;
	checksum = 0;
	index = 0;
	do {
		index++;
		partial = (character & 0xff) +
			(SceUInt16)((checksum << 15) | (checksum >> 1));
		checksum = (SceUInt16)((character >> 8) +
			(SceUInt16)((partial << 15) | (partial >> 1)));
		if (index >= length)
			break;
		character = name[index];
	} while (character);
	return checksum;
}

extern int utf16Length(const SceUInt16 *string);
extern SceUInt16 *utf16CopyBounded(SceUInt16 *destination,
	const SceUInt16 *source, unsigned int count);
extern int utf16Compare(const SceUInt16 *left, const SceUInt16 *right);
extern int codepageToUtf16(SceUInt16 *destination, int capacity,
	const unsigned char *source);

int fatNameLength(const SceUInt16 *name)
{
	return utf16Length(name);
}

SceUInt16 *fatNameCopyBounded(SceUInt16 *destination,
	const SceUInt16 *source, unsigned int count)
{
	return utf16CopyBounded(destination, source, count);
}

int fatNameCompare(const SceUInt16 *left, const SceUInt16 *right)
{
	return utf16Compare(left, right);
}

SceUInt16 *fatNameAppendBounded(SceUInt16 *destination,
	const SceUInt16 *source, unsigned int count)
{
	SceUInt16 *end = destination;
	SceUInt16 *next;
	unsigned int copied;
	SceUInt16 character;

	for (;;) {
		copied = *end;
		next = end + 1;
		if (!copied)
			break;
		end = next;
	}
	if (count) {
		character = *source;
		if (character) {
			do {
				copied++;
				*end = character;
				end = next++;
				if (copied == count)
					break;
				character = *++source;
			} while (character);
		}
	}
	*end = 0;
	return destination;
}

int fatCodepageToUtf16(SceUInt16 *destination, int capacity,
	const unsigned char *source)
{
	return codepageToUtf16(destination, capacity, source);
}

int isShortNameCharacter(unsigned int character, int allowExtended)
{
	SceUInt16 allowed[] = {
		'$', '%', '\'', '-', '_', '@', '~', '`', '!', '(', ')', '{', '}', '^', '#', '&', 0,
	};
	SceUInt16 extended[] = { '+', ',', ';', '=', '[', ']', 0 };
	const SceUInt16 *entry;
	int result;

	if (character > '~')
		return 0;
	if (((character & ~0x20U) - 'A') <= 25 || character - '0' <= 9)
		return 1;
	entry = allowed;
	if (*entry == character)
		return 1;
	if (*entry) {
		do {
			entry++;
			if (*entry == character)
				return 1;
		} while (*entry);
	}
	if (allowExtended) {
		entry = extended;
		result = *entry;
		if (result == character)
			return 1;
		if (result) {
			do {
				entry++;
				result = *entry;
				if (result == character)
					return 1;
			} while (result);
		}
		return result;
	}
	return 0;
}

int shortNameTimeCharacter(void)
{
	unsigned char value = ksceKernelGetSystemTimeLow() % 36;

	if (value > 9)
		return (unsigned char)(value + 'A' - 10);
	return (unsigned char)(value + '0');
}

extern unsigned int lookupUpcaseCharacter(unsigned int unicode,
	SceExfatfsUpcaseTable *table);

int UVFAT_ToUpperBuf(SceUInt16 *destination, const SceUInt16 *source,
	int count, SceExfatfsFileSystem *fileSystem)
{
	unsigned int character;
	int index;
	int destinationIndex;

	if (fileSystem->fileSystemType == 2) {
		if (!count)
			return 0;
		character = *source;
		destinationIndex = 0;
		if (character) {
			index = 0;
			do {
				index++;
				destination[destinationIndex] = lookupUpcaseCharacter(character,
					&fileSystem->upcase);
				destinationIndex = index;
				if (index == count)
					return 0;
				character = *++source;
			} while (character);
		}
	} else {
		if (!count)
			return 0;
		character = *source;
		destinationIndex = 0;
		if (character) {
			index = 0;
			do {
				index++;
				if (character - 'a' <= 25)
					character = (SceUInt16)(character - 'a' + 'A');
				destination[destinationIndex] = character;
				destinationIndex = index;
				if (index == count)
					return 0;
				character = *++source;
			} while (character);
		}
	}
	destination[destinationIndex] = 0;
	return 0;
}

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

void adjustFatDateTimeMinutes(SceExfatfsDateTime *dateTime, int minutes)
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

