/* Directory-entry date conversion; Sony source-file records are absent. */
#include "exfatfs_internal.h"

extern void adjustFatDateTimeMinutes(SceExfatfsDateTime *dateTime, int minutes);

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
