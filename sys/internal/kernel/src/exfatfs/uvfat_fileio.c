#include <sysmem/debug.h>
#include <string.h>
#include "exfatfs_internal.h"

extern int isShortNameCharacter(unsigned int character, int allowExtended);
extern int shortNameTimeCharacter(void);
extern SceUInt32 fileSystemClusterSize(SceExfatfsFileSystem *fileSystem);
extern int UVFAT_ReadCblock(SceUInt32 cluster, SceUInt32 block,
	SceExfatfsFileSystem *fileSystem, void *buffer);
extern int UVFAT_SetGetFAT(SceExfatfsFileSystem *fileSystem, int mode,
	SceUInt32 cluster, SceUInt32 *value);
extern void _exfatfs_notify_broken_cluster_chain(SceExfatfsDrive *drive,
	SceUInt32 cluster, SceUInt32 next);
extern int sceFatfsACGetRootDirAttribute(SceExfatfsDrive *drive, unsigned char *attribute);
extern int _sceFatfsWriteModeFromStat(SceExfatfsDrive *drive, int attribute);
extern int initializeFatDateTime(SceExfatfsDateTime *dateTime);
extern void _initialize_cluster_cache_data(SceExfatfsFnode *fnode);

extern int hasCachedFreeClusterCount(SceExfatfsFileSystem *fileSystem);
extern SceUInt32 UVFAT_GetFatEmptyClust(SceExfatfsFileSystem *fileSystem,
	SceUInt32 FatEmptyClust);
extern int UVFAT_allocationbitmapentry(SceExfatfsSharedResource *sharedResource,
	int mode, SceExfatfsFileSystem *fileSystem, SceUInt32 cluster, SceUInt32 *value);

extern int UVFAT_SetEOF(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster);

extern int updateallocbitmaps(SceExfatfsFileSystem *fileSystem);

extern int getCurrentFatDateTime(SceExfatfsDateTime *dateTime);

extern void _invalidate_cluster_cache_data(SceExfatfsFnode *fnode);
extern void resetFilePosition(SceExfatfsFile *file);
extern void _print_fnode(SceExfatfsFnode *fnode, const char *function);
extern int flushFileSystemMetadata(SceExfatfsFileSystem *fileSystem);
extern SceUInt64 clusterToDeviceSector(SceUInt32 cluster,
	SceExfatfsFileSystem *fileSystem);
extern int UVFAT_ReadDevice(SceExfatfsFileSystem *fileSystem,
	SceUInt64 sector, unsigned int size, void *buffer);
extern unsigned int UVFAT_TimeToFATDate(SceExfatfsDateTime *dateTime);
extern unsigned int fatDateTimeToTime(SceExfatfsDateTime *dateTime);
extern int UVFAT_WriteStatDir_ex(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceExfatfsStat *stat);
extern int readTemporalDirectoryEntry(SceExfatfsSharedResource *sharedResource,
	void *primary, void *secondary, SceExfatfsFileSystem *fileSystem);
extern int UVFAT_WriteDirEntry_ex(SceExfatfsSharedResource *sharedResource,
	int fd, const void *primary, const void *secondary, int *writeResult,
	SceExfatfsFileSystem *fileSystem);
extern int readDirectoryEntryBytes(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFile *file, void *entry);
extern int setDirectoryPosition(SceExfatfsFile *file, SceUInt64 offset);
extern SceUInt16 *fatNameAppendBounded(SceUInt16 *destination,
	const SceUInt16 *source, unsigned int capacity);
extern int copyFatEntryAttributeToStat(const SceExfatfsFatDirectoryEntry *entry,
	SceExfatfsStat *stat, SceExfatfsFileSystem *fileSystem);
extern SceExfatfsDateTime *decodeFatDateTime(SceExfatfsDateTime *dateTime,
	unsigned int date, unsigned int time);
extern void _exfatfs_notify_invalid_directory_entry(SceExfatfsDrive *drive,
	const void *entry);
extern void _exfatfs_notify_directory_loop(SceExfatfsDrive *drive,
	SceUInt32 startCluster, SceOff offset, unsigned char attributes, SceOff fileSize);
extern int writeDirectoryEntryBytes(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFile *file,
	const void *entry, int *writeResult);
extern int fatNameLength(const SceUInt16 *name);
extern int fatNameCompare(const SceUInt16 *left, const SceUInt16 *right);
extern SceUInt16 *fatNameCopyBounded(SceUInt16 *destination,
	const SceUInt16 *source, unsigned int capacity);
extern unsigned int checksumUtf16Name(const SceUInt16 *name, unsigned int length);
extern void *allocateFatBuffer(SceSize size);
extern void freeFatBuffer(void *buffer, SceSize size);
extern int UVFAT_ToUpperBuf(SceUInt16 *destination, const SceUInt16 *source,
	int count, SceExfatfsFileSystem *fileSystem);
extern int copyExfatEntryAttributeToStat(const SceExfatfsFileDirectoryEntry *entry,
	SceExfatfsStat *stat, SceExfatfsFileSystem *fileSystem);
extern void decodeExfatEntryDateTimes(SceExfatfsStat *stat,
	const SceExfatfsFileDirectoryEntry *entry);
extern int fatCodepageToUtf16(SceUInt16 *destination, int capacity,
	const unsigned char *source);
extern int sceFatfsACCheckRDirEntry(SceExfatfsDrive *drive, unsigned char attribute);

unsigned int _setShortNameString(const SceUInt16 *source, unsigned char *destination,
	unsigned int capacity, int *sourceIndex, unsigned int *flags)
{
	int index = *sourceIndex;
	unsigned int written = 0;

	while (written < capacity) {
		unsigned int character = source[index];
		unsigned char output;

		if (character == ' ') {
			index++;
			*flags |= 1;
			continue;
		}
		if ((character & 0xfc00) == 0xd800) {
			index++;
			*flags |= 1;
#if FW_DEBUG
			if ((source[index] & 0xfc00) != 0xdc00) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 5313,
					"%s: invalid code[%#x] after [%#x]\n",
					"_setShortNameString", source[index], character);
			}
#endif
			output = '_';
		} else if ((character & 0xfc00) == 0xdc00) {
			*flags |= 1;
			SCE_DBG_PRINTF_LEVEL(1, 0, 5320, "%s: invalid code[%#x]\n",
				"_setShortNameString", character);
			output = '_';
		} else if (character == '.' || !character) {
			unsigned int padIndex;

			*sourceIndex = index;
			for (padIndex = written; padIndex < capacity; padIndex++)
				destination[padIndex] = ' ';
			return written;
		} else if (isShortNameCharacter(character, 0)) {
			output = character;
			if ((unsigned int)output - 'a' <= 'z' - 'a') {
				output -= 32;
				*flags |= 2;
			}
		} else {
			*flags |= 1;
			output = shortNameTimeCharacter();
		}
		destination[written++] = output;
		index++;
	}
	*sourceIndex = index;
	return written;
}

extern int decodeExfatUtcOffset(unsigned int offset);
extern void adjustFatDateTimeMinutes(SceExfatfsDateTime *dateTime, int minutes);
extern int copyStatAttributeToFatEntry(const SceExfatfsStat *stat,
	SceExfatfsFatDirectoryEntry *entry, SceExfatfsFileSystem *fileSystem);

int UVFAT_ChangeStatFileentry(SceExfatfsFileSystem *fileSystem,
	SceExfatfsFileDirectoryEntry *entry, const SceExfatfsStat *stat)
{
	SceExfatfsDateTime time = stat->mtime;
	SceExfatfsFatDirectoryEntry fatEntry;
	SceUInt32 encoded;

	if (entry->modificationUtcOffset)
		adjustFatDateTimeMinutes(&time, decodeExfatUtcOffset(entry->modificationUtcOffset));
	encoded = (time.month << 21) | (time.day << 16) | (time.hour << 11) |
		(time.minute << 5) | (time.second >> 1) | ((time.year - 1980) << 25);
	entry->modificationTenths = (time.second & 1) ? 100 : 0;
	entry->modificationTime[0] = encoded;
	entry->modificationTime[2] = encoded >> 16;
	entry->modificationTime[1] = encoded >> 8;
	entry->modificationTime[3] = encoded >> 24;
	time = stat->atime;
	if (entry->accessUtcOffset)
		adjustFatDateTimeMinutes(&time, decodeExfatUtcOffset(entry->accessUtcOffset));
	encoded = (time.month << 21) | (time.day << 16) | (time.hour << 11) |
		(time.minute << 5) | (time.second >> 1) | ((time.year - 1980) << 25);
	entry->accessTime[0] = encoded;
	entry->accessTime[2] = encoded >> 16;
	entry->accessTime[1] = encoded >> 8;
	entry->accessTime[3] = encoded >> 24;
	time = stat->ctime;
	if (entry->creationUtcOffset)
		adjustFatDateTimeMinutes(&time, decodeExfatUtcOffset(entry->creationUtcOffset));
	encoded = (time.month << 21) | (time.day << 16) | (time.hour << 11) |
		(time.minute << 5) | (time.second >> 1) | ((time.year - 1980) << 25);
	entry->creationTenths = (time.second & 1) ? 100 : 0;
	entry->creationTime[0] = encoded;
	entry->creationTime[1] = encoded >> 8;
	entry->creationTime[2] = encoded >> 16;
	entry->creationTime[3] = encoded >> 24;
	if (stat->st_type == 6 && !(entry->fileAttributes[0] & 0x10)) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 4356, "%s: attribute[%#x], set directory flag\n",
			"UVFAT_ChangeStatFileentry", entry->fileAttributes[0]);
		entry->fileAttributes[0] |= 0x10;
	}
	if (entry->fileAttributes[0] & 0x10) {
		if (stat->st_type != 6)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4362, "%s: stat->st_type(%d) != expected(%d)\n",
				"UVFAT_ChangeStatFileentry", stat->st_type, 6);
	} else if (stat->st_type) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4372, "%s: stat->st_type(%d) != expected(%d)\n",
			"UVFAT_ChangeStatFileentry", stat->st_type, 0);
	}
	copyStatAttributeToFatEntry(stat, &fatEntry, fileSystem);
	entry->fileAttributes[0] = fatEntry.attribute;
	return 0;
}

extern void _invalidate_cluster_cache_element(SceExfatfsClusterCacheElement *element);

static inline unsigned int _cluster_cache_interval(SceExfatfsFnode *fnode)
{
	if (fnode->tailCluster == (SceUInt32)-2) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 200, "_cluster_cache_interval",
			"%s: fnode[%p], tailIndex[%#lx], tailCluster[%#lx]\n",
			"_cluster_cache_interval", fnode, fnode->tailIndex, fnode->tailCluster);
		return 1;
	}
	if (fnode->clusterCount < fnode->tailIndex) {
		if (fnode->Stat.st_type == 6)
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 209, "_cluster_cache_interval",
				"%s: fnode[%p], directory tailIndex[%#lx], tailCluster[%#lx], clusterCount[%#lx]\n",
				"_cluster_cache_interval", fnode, fnode->tailIndex,
				fnode->tailCluster, fnode->clusterCount);
		else
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 212, "_cluster_cache_interval",
				"%s: fnode[%p], unexpected tailIndex[%#lx], tailCluster[%#lx], clusterCount[%#lx]\n",
				"_cluster_cache_interval", fnode, fnode->tailIndex,
				fnode->tailCluster, fnode->clusterCount);
	}
	return fnode->tailIndex / fnode->cache_element_count + 1;
}

static inline void _change_cluster_cache_interval(SceExfatfsFnode *fnode,
	unsigned int interval)
{
	SceExfatfsClusterCacheElement *element = fnode->cluster_cache_array;
	SceExfatfsClusterCacheElement *end = element + fnode->cache_element_count;

	for (; element < end; element++) {
		SceExfatfsClusterCacheElement *target;
		unsigned int index, count, targetIndex, targetEnd, endIndex;

		count = element->count;
		if (!count)
			continue;
		index = element->index;
		__builtin_prefetch((const unsigned char *)element + 40);
		target = fnode->cluster_cache_array + index / interval;
		if (element == target)
			continue;
		if (target->count && target->index <= index + count) {
			targetIndex = target->index;
			targetEnd = targetIndex + target->count;
			endIndex = index + count;
			if (index <= targetEnd) {
				if (target->cluster + index == element->cluster + targetIndex) {
					if (index < targetIndex) {
						target->cluster = element->cluster;
						target->index = index;
						target->count = targetEnd - index;
						targetIndex = index;
						endIndex = element->index + element->count;
					}
					if (endIndex > targetEnd)
						target->count = endIndex - targetIndex;
				} else {
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 264,
						"_change_cluster_cache_interval",
						"%s: can't connect cluster[%lx] from index[%#lx] and count[%#lx], temp->cluster[%#lx] for index[%#lx]\n",
						"_change_cluster_cache_interval", target->cluster,
						targetIndex, target->count, element->cluster, index);
				}
			}
		} else {
			*target = *element;
		}
		_invalidate_cluster_cache_element(element);
	}
}

void _update_cached_index(SceExfatfsFnode *fnode, unsigned int clusterIndex,
	unsigned int start, SceUInt64 size, unsigned int flags)
{
	unsigned int clusterSize = fnode->fileSystem->sectorSize *
		fnode->fileSystem->clusterSectorCount;
	unsigned int count, checkedIndex;
	SceExfatfsClusterCacheElement *element;

	SCE_DBG_ASSERT_FUNC(size != 0, 187, "_last_cluster_index");
	count = (size - 1) / clusterSize;
	checkedIndex = count + clusterIndex;
	if (fnode->cluster_cache_array) {
		unsigned int interval = _cluster_cache_interval(fnode);

		if (checkedIndex + 1 > interval * fnode->cache_element_count) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 289,
				"%s: clusterCount[%#lx], checkedTailIndex[%#lx]->[%#lx], fnode[%p]\n",
				"_update_cached_index", fnode->clusterCount,
				fnode->tailIndex, checkedIndex, fnode);
			_change_cluster_cache_interval(fnode,
				(checkedIndex + 1) / fnode->cache_element_count + 1);
		}
	}
#if FW_DEBUG
	if (flags & 2) {
		if ((SceUInt64)fnode->clusterCount * clusterSize < fnode->dirSize)
			SCE_DBG_PRINTF_LEVEL(1, 0, 297,
				"%s: unexpected dirSize[%#x], clusterCount[%#lx], clusterSize[%#lx], fnode[%p]\n",
				"_update_cached_index", fnode->dirSize, fnode->clusterCount,
				clusterSize, fnode);
	} else if ((SceUInt64)fnode->clusterCount * clusterSize < (SceUInt64)fnode->Stat.Size) {
		if (fnode->tailIndex + 1 == fnode->clusterCount)
			SCE_DBG_PRINTF_LEVEL(2, 0, 304,
				"%s: Size[%#llx], clusterCount[%#lx], clusterSize[%#lx], fnode[%p], checkedTailIndex[%#lx]\n",
				"_update_cached_index", fnode->Stat.Size, fnode->clusterCount,
				clusterSize, fnode, fnode->tailIndex);
		else
			SCE_DBG_PRINTF_LEVEL(1, 0, 306,
				"%s: unexpected Size[%#llx], clusterCount[%#lx], clusterSize[%#lx], fnode[%p], checkedTailIndex[%#lx], checkedTailCluster[%#lx], clusterIndex[%#lx], start[%#lx], n[%#lx]\n",
				"_update_cached_index", fnode->Stat.Size, fnode->clusterCount,
				clusterSize, fnode, fnode->tailIndex, fnode->tailCluster,
				clusterIndex, start, count);
	}
#endif
	if (fnode->clusterCount <= checkedIndex) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 311,
			"%s: clusterCount[%#lx], checkedTailIndex[%#lx]->[%#lx], fnode[%p]\n",
			"_update_cached_index", fnode->clusterCount, fnode->tailIndex,
			checkedIndex, fnode);
		fnode->clusterCount = checkedIndex + 1;
	}
	if (fnode->tailIndex == (SceUInt32)-2) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 315,
			"%s: tailIndex[%#lx]->[%#lx], tailCluster[%#lx]->[%#lx]\n",
			"_update_cached_index", fnode->tailIndex, checkedIndex,
			fnode->tailCluster, count + start);
		fnode->tailCluster = count + start;
		fnode->tailIndex = checkedIndex;
	} else if (fnode->tailIndex < checkedIndex) {
		if (clusterIndex <= fnode->tailIndex + 1) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 323,
				"%s: tailIndex[%#lx]->[%#lx], tailCluster[%#lx]->[%#lx]\n",
				"_update_cached_index", fnode->tailIndex, checkedIndex,
				fnode->tailCluster, count + start);
			if (clusterIndex <= fnode->tailIndex) {
				if (fnode->tailCluster == start - clusterIndex + fnode->tailIndex) {
					SCE_DBG_PRINTF_LEVEL(2, 0, 329,
						"%s: increase chunk size, fnode[%p], checkedIndex[%#lx], checkedTail[%#lx], index[%#lx], start[%#lx], n[%#lx], chunkCount[%#lx]\n",
						"_update_cached_index", fnode, fnode->tailIndex,
						fnode->tailCluster, clusterIndex, start, count, fnode->chunkCount);
				} else {
					SCE_DBG_PRINTF_LEVEL(2, 0, 326,
						"%s: new chunk, fnode[%p], checkedIndex[%#lx], checkedTail[%#lx], index[%#lx], start[%#lx], n[%#lx], chunkCount[%#lx]\n",
						"_update_cached_index", fnode, fnode->tailIndex,
						fnode->tailCluster, clusterIndex, start, count, fnode->chunkCount);
					fnode->chunkCount++;
				}
			} else if (fnode->tailCluster == start - 1) {
				unsigned int oldIndex = clusterIndex;
				unsigned int oldStart = start;
				unsigned int oldCount = count;

				clusterIndex--;
				count++;
				start = fnode->tailCluster;
				SCE_DBG_PRINTF_LEVEL(2, 0, 336,
					"%s: increase chunk size, fnode[%p], checkedIndex[%#lx], checkedTail[%#lx], index[%#lx], start[%#lx], n[%#lx], chunkCount[%#lx]\n",
					"_update_cached_index", fnode, fnode->tailIndex,
					fnode->tailCluster, oldIndex, oldStart, oldCount, fnode->chunkCount);
			} else {
				SCE_DBG_PRINTF_LEVEL(2, 0, 333,
					"%s: new chunk, fnode[%p], checkedIndex[%#lx], checkedTail[%#lx], index[%#lx], start[%#lx], n[%#lx], chunkCount[%#lx]\n",
					"_update_cached_index", fnode, fnode->tailIndex,
					fnode->tailCluster, clusterIndex, start, count, fnode->chunkCount);
				fnode->chunkCount++;
			}
		} else {
			SCE_DBG_PRINTF_LEVEL(2, 0, 321,
				"%s: unexpected index[%#lx], fnode[%p], checkedIndex[%#lx], checkedTail[%#lx], start[%#lx], n[%#lx], chunkCount[%#lx]\n",
				"_update_cached_index", clusterIndex, fnode, fnode->tailIndex,
				fnode->tailCluster, start, count, fnode->chunkCount);
		}
		fnode->tailCluster = count + start;
		fnode->tailIndex = checkedIndex;
	} else {
		SCE_DBG_PRINTF_LEVEL(2, 0, 347,
			"%s: tailIndex[%#lx] >= index[%#lx], tailCluster[%#lx], cluster[%#lx]\n",
			"_update_cached_index", fnode->tailIndex, checkedIndex,
			fnode->tailCluster, count + start);
	}
	if (fnode->cluster_cache_array) {
		unsigned int interval, remainder;

		SCE_DBG_PRINTF_LEVEL(2, 0, 350, "%s: update cluster cache\n",
			"_update_cached_index");
		if (!fnode->clusterCount)
			return;
		element = fnode->cluster_cache_array;
		interval = _cluster_cache_interval(fnode);
		remainder = fnode->tailCluster == (SceUInt32)-2 ? 0 : clusterIndex % interval;
		element += clusterIndex / interval;
		do {
			if (element->cluster == (SceUInt32)-2) {
				element->cluster = start;
				element->index = clusterIndex;
				element->count = count + 1;
			} else {
				unsigned int index = element->index;

				__builtin_prefetch((const unsigned char *)(element + 1) + 40);
				if (clusterIndex >= index) {
					unsigned int endIndex = index + element->count;

					if (clusterIndex <= endIndex && start == element->cluster + clusterIndex - index) {
						if (endIndex < count + clusterIndex + 1)
							element->count = count + clusterIndex + 1 - index;
					}
				} else {
					if (index == clusterIndex + count + 1 && element->cluster == start + count + 1)
						element->count += count + 1;
					else
						element->count = count + 1;
					element->cluster = start;
					element->index = clusterIndex;
				}
			}
			if (count <= interval - remainder)
				break;
			count = count + remainder - interval;
			clusterIndex += interval - remainder;
			start += interval - remainder;
			remainder = 0;
			element++;
		} while (count);
	}
}

extern int g_codepageTablesEnabled;
extern SceUInt32 getInitializedFileCount(SceExfatfsSharedResource *sharedResource);
extern int UVFAT_FlushFdBuf(SceExfatfsFile *file, SceExfatfsFileSystem *fileSystem);

void sync_multi_open(SceExfatfsSharedResource *sharedResource, SceExfatfsFile *file)
{
	unsigned int fileCount = getInitializedFileCount(sharedResource);
	unsigned int index;
	int otherFd = 0;

	if (file->fnode->refCount <= 1)
		return;
	for (index = 0; index != fileCount; index++) {
		SceExfatfsFile *other = &sharedResource->files[index];

		if (other == file || other->fnode != file->fnode)
			continue;
		otherFd++;
		if (other->cacheIndex != file->cacheIndex)
			continue;
		if (other->cacheFlags & 1) {
			SCE_DBG_ASSERT(other->cacheFlags == 3, 5531);
			SCE_DBG_PRINTF_LEVEL(1, 0, 5533,
				"%s: cache is still dirty because of write error\n", "sync_multi_open");
			SCE_DBG_ASSERT(other->Buf != NULL, 5534);
			if (file->cacheFlags & 1) {
				SCE_DBG_ASSERT(file->Buf != NULL, 5536);
				memcpy(other->Buf, file->Buf, file->fnode->fileSystem->fileBufferSize);
			}
		} else {
			int cluster;

			SCE_DBG_ASSERT(other->cacheFlags == 0, 5541);
			cluster = g_codepageTablesEnabled;
			other->cacheIndex = (SceUInt64)-1;
			if (cluster)
				cluster = -2;
			other->cacheCluster = cluster;
		}
	}
	if (file->fnode->fdCount != otherFd + 1)
		SCE_DBG_PRINTF_LEVEL(1, 0, 5548, "%s: other_fd(%d) != expected(%d)\n",
			"sync_multi_open", otherFd, file->fnode->fdCount - 1);
	if (otherFd && (file->cacheFlags & 1)) {
		int result = UVFAT_FlushFdBuf(file, file->fnode->fileSystem);

		if (result)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5498, "_flush_fd_cache",
				"%s: UVFAT_FlushFdBuf() Fail[%#x]\n", "_flush_fd_cache", result);
	}
}

SceUInt32 _root_cluster(SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsBootSector *boot;
	SceUInt32 root;
	const unsigned char *rootEntries;

	SCE_DBG_PRINTF_LEVEL(2, 0, 98, "%s: fileSystem[%p], PBRBuf[%p]\n",
		"_root_cluster", fileSystem, fileSystem->PBRBuf);
	if (fileSystem->fileSystemType == 2) {
		root = fileSystem->firstClusterOfRoot;
		if (!root)
			SCE_DBG_PRINTF_LEVEL(1, 0, 102,
				"%s(exFAT): unexpected firstClusterOfRoot[%#lx]\n", "_root_cluster", root);
	} else {
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		if (rootEntries[0] | ((unsigned int)rootEntries[1] << 8)) {
			root = fileSystem->firstClusterOfRoot;
			if (root)
				SCE_DBG_PRINTF_LEVEL(1, 0, 108,
					"%s(FAT12/FAT16): unexpected firstClusterOfRoot[%#lx]\n",
					"_root_cluster", root);
			root = 0;
		} else {
			root = boot->fat32.rootCluster;
			if (!fileSystem->firstClusterOfRoot)
				SCE_DBG_PRINTF_LEVEL(1, 0, 114,
					"%s(FAT32): firstClusterOfRoot[%#lx] != root[%#lx]\n",
					"_root_cluster", fileSystem->firstClusterOfRoot, root);
		}
	}
	return root;
}

#if FW_VERSION >= 0x04000000
SceUInt32 _start_cluster(SceExfatfsFnode *fnode, int forDirectorySearch)
{
	SceUInt16 type = fnode->Stat.st_type;
	SceInt16 rootEntryCount;
	SceUInt32 cluster = fnode->DirCluster;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	SceExfatfsClusterCacheElement *element;

	if (forDirectorySearch) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 921,
			"%s: for_directory_search[%#x], DirCluster[%#lx], buffer StartCluster[%#lx]\n",
			"_start_cluster", forDirectorySearch, cluster, fnode->Stat.StartCluster);
		if (!cluster) {
			boot = fnode->fileSystem->PBRBuf;
			rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
			rootEntryCount = rootEntries[0] | ((unsigned int)rootEntries[1] << 8);
			if (!rootEntryCount) {
				cluster = _root_cluster(fnode->fileSystem);
				SCE_DBG_PRINTF_LEVEL(1, 0, 925,
					"%s: unexpected cluster[%#x]->root_cluster[%#lx]\n",
					"_start_cluster", rootEntryCount, cluster);
			}
		}
	} else {
		SCE_DBG_PRINTF_LEVEL(2, 0, 931,
			"%s: for_directory_search[%#x], DirCluster[%#lx], StartCluster[%#lx], type(%d)\n",
			"_start_cluster", 0, cluster, fnode->Stat.StartCluster, type);
		cluster = fnode->Stat.StartCluster;
		if (!cluster) {
			if (type == 6) {
				boot = fnode->fileSystem->PBRBuf;
				rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
				rootEntryCount = rootEntries[0] | ((unsigned int)rootEntries[1] << 8);
				if (!rootEntryCount) {
					cluster = _root_cluster(fnode->fileSystem);
					SCE_DBG_PRINTF_LEVEL(1, 0, 937,
						"%s: unexpected cluster[%#x]->root_cluster[%#lx]\n",
						"_start_cluster", rootEntryCount, cluster);
				}
			} else {
				if (fnode->tailIndex == (SceUInt32)-2) {
					if (fnode->tailCluster != (SceUInt32)-2)
						SCE_DBG_PRINTF_LEVEL(1, 0, 944,
							"%s: unexpected tailIndex[%#lx], tailCluster[%#lx], fnode[%p]\n",
							"_start_cluster", fnode->tailIndex, fnode->tailCluster, fnode);
				} else if (fnode->tailIndex) {
					SCE_DBG_PRINTF_LEVEL(1, 0, 947,
						"%s: unexpected tailIndex[%#lx], StartCluster[%#lx], fnode[%p]\n",
						"_start_cluster", fnode->tailIndex, 0, fnode);
				}
				if (fnode->clusterCount)
					SCE_DBG_PRINTF_LEVEL(1, 0, 950,
						"%s: unexpected clusterCount[%#lx], StartCluster[%#lx], fnode[%p]\n",
						"_start_cluster", fnode->clusterCount, 0, fnode);
				element = fnode->cluster_cache_array;
				if (element && (element->cluster != (SceUInt32)-2 || element->index || element->count))
					SCE_DBG_PRINTF_LEVEL(2, 0, 959,
						"%s: unexpected temp->cluster[%#lx], temp->clusterIndex[%#lx], contiguous[%#lx], cluster[%#lx], fnode[%p]\n",
						"_start_cluster", element->cluster, element->index, element->count, 0, fnode);
				fnode->tailCluster = (SceUInt32)-1;
				fnode->tailIndex = 0;
				return (SceUInt32)-1;
			}
		} else if (cluster == (SceUInt32)-1 && type == 6) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 971,
				"%s: StartCluster[%#lx] for deleted directory or uninitialized StartCluster\n",
				"_start_cluster", cluster);
			SCE_DBG_ASSERT(0, 972);
		}
	}
	if (fnode->tailIndex == (SceUInt32)-2) {
		if (fnode->tailCluster != (SceUInt32)-2)
			SCE_DBG_PRINTF_LEVEL(1, 0, 979,
				"%s: unexpected tailIndex[%#lx], tailCluster[%#lx], fnode[%p]\n",
				"_start_cluster", fnode->tailIndex, fnode->tailCluster, fnode);
	} else if (!fnode->tailIndex) {
		if (fnode->tailCluster == (SceUInt32)-2) {
			if (forDirectorySearch || fnode->Stat.st_type == 6)
				SCE_DBG_PRINTF_LEVEL(1, 0, 984,
					"%s: unexpected tailCluster[%#lx], StartCluster[%#lx], fnode[%p]\n",
					"_start_cluster", fnode->tailCluster, cluster, fnode);
			else
				SCE_DBG_PRINTF_LEVEL(1, 0, 986,
					"%s: can be 0 size file, tailCluster[%#lx], StartCluster[%#lx], fnode[%p], type(%d)\n",
					"_start_cluster", fnode->tailCluster, cluster, fnode, fnode->Stat.st_type);
		} else if (cluster != fnode->tailCluster) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 990,
				"%s: unexpected tailCluster[%#lx], StartCluster[%#lx], fnode[%p]\n",
				"_start_cluster", fnode->tailCluster, cluster, fnode);
		}
	}
	if (forDirectorySearch) {
		if (cluster) {
			if (fnode->clusterCount) {
				if (fnode->clusterCount <= fnode->tailIndex)
					SCE_DBG_PRINTF_LEVEL(1, 0, 1004,
						"%s: unexpected tailIndex[%#lx], clusterCount[%#lx], DirCluster[%#lx], StartCluster[%#lx], fnode[%p]\n",
						"_start_cluster", fnode->tailIndex, fnode->clusterCount,
						cluster, fnode->Stat.StartCluster, fnode);
			} else if (fnode->dirSize) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 999,
					"%s: dirSize[%#x], clusterCount[%#lx], DirCluster[%#lx], StartCluster[%#lx], fnode[%p]\n",
					"_start_cluster", fnode->dirSize, fnode->clusterCount,
					cluster, fnode->Stat.StartCluster, fnode);
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0, 1001,
					"%s: clusterCount[%#lx], DirCluster[%#lx], StartCluster[%#lx], fnode[%p]\n",
					"_start_cluster", fnode->clusterCount, cluster, fnode->Stat.StartCluster, fnode);
			}
		} else {
			boot = fnode->fileSystem->PBRBuf;
			rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
			if (rootEntries[0] | ((unsigned int)rootEntries[1] << 8))
				SCE_DBG_PRINTF_LEVEL(2, 0, 1008,
					"%s: DirCluster[%#lx], StartCluster[%#lx], clusterCount[%#lx], fnode[%p]\n",
					"_start_cluster", cluster, fnode->Stat.StartCluster, fnode->clusterCount, fnode);
			else
				SCE_DBG_PRINTF_LEVEL(1, 0, 1010,
					"%s: unexpected DirCluster[%#lx], StartCluster[%#lx], clusterCount[%#lx], fnode[%p]\n",
					"_start_cluster", cluster, fnode->Stat.StartCluster, fnode->clusterCount, fnode);
		}
	} else if (cluster) {
		if (!fnode->clusterCount)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1016,
				"%s: unexpected clusterCount[%#lx], StartCluster[%#lx], fnode[%p]\n",
				"_start_cluster", fnode->clusterCount, cluster, fnode);
		else if (fnode->clusterCount <= fnode->tailIndex)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1018,
				"%s: unexpected tailIndex[%#lx], clusterCount[%#lx], StartCluster[%#lx], tailCluster[%#lx], fnode[%p]\n",
				"_start_cluster", fnode->tailIndex, fnode->clusterCount, cluster, fnode->tailCluster, fnode);
	} else if (fnode->clusterCount) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1022,
			"%s: unexpected clusterCount[%#lx], StartCluster[%#lx], fnode[%p]\n",
			"_start_cluster", fnode->clusterCount, cluster, fnode);
	}
	element = fnode->cluster_cache_array;
	if (element && (element->index || element->cluster != cluster || !element->count))
		SCE_DBG_PRINTF_LEVEL(1, 0, 1029,
			"%s: unexpected temp->cluster[%#lx], temp->clusterIndex[%#lx], contiguous[%#lx], cluster[%#lx], fnode[%p]\n",
			"_start_cluster", element->cluster, element->index, element->count, cluster, fnode);
	if (!fnode->tailCluster) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 1033,
			"%s: checkedTailCluster[%#lx], checkedTailIndex[%#lx], clusterCount[%#lx], chunkCount[%#lx]\n",
			"_start_cluster", fnode->tailCluster, fnode->tailIndex, fnode->clusterCount, fnode->chunkCount);
		SCE_DBG_ASSERT(!fnode->clusterCount && fnode->chunkCount == 1, 1034);
	}
	return cluster;
}
#else
static inline SceUInt32 _start_cluster(SceExfatfsFnode *fnode, int forDirectorySearch)
{
	SceUInt32 cluster;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;

	cluster = forDirectorySearch ? fnode->DirCluster : fnode->Stat.StartCluster;
	if (!cluster) {
		if (!forDirectorySearch && fnode->Stat.st_type != 6) {
			fnode->tailCluster = (SceUInt32)-1;
			fnode->tailIndex = 0;
			return (SceUInt32)-1;
		}
		boot = fnode->fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8)))
			cluster = _root_cluster(fnode->fileSystem);
	}
	return cluster;
}
#endif

void _set_start_cluster(SceExfatfsFnode *fnode, SceUInt32 cluster, int type)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsClusterCacheElement *element;
	SceUInt32 maximum, first;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;

	if (fnode->Stat.StartCluster != cluster)
		SCE_DBG_PRINTF_LEVEL(1, 0, 406,
			"%s: cluster[%#lx] != StartCluster[%#lx], Size[%#llx], type(%d)\n",
			"_set_start_cluster", cluster, fnode->Stat.StartCluster, fnode->Stat.Size, type);
	else
		SCE_DBG_PRINTF_LEVEL(2, 0, 408,
			"%s: StartCluster[%#lx], Size[%#llx], type(%d)\n",
			"_set_start_cluster", cluster, fnode->Stat.Size, type);
	if (fnode->Stat.st_type != type)
		SCE_DBG_PRINTF_LEVEL(1, 0, 411, "%s: type(%d) != stat.st_type(%d)\n",
			"_set_start_cluster", type, fnode->Stat.st_type);
	fileSystem = fnode->fileSystem;
	fnode->tailCluster = cluster;
	fnode->tailIndex = 0;
	maximum = fileSystem->clusterCount + 2;
	if (cluster >= maximum) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 430, "%s: cluster[%#lx] >= [%#lx]\n",
			"_set_start_cluster", cluster, maximum);
		SCE_DBG_ASSERT(cluster < fnode->fileSystem->clusterCount + 2, 431);
		fnode->tailCluster = (SceUInt32)-2;
	} else if (!cluster) {
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		if (type == 6 && (rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
			fnode->clusterCount = 0;
			fnode->chunkCount = 1;
		} else {
			if (type)
				SCE_DBG_PRINTF_LEVEL(1, 0, 445,
					"%s: unexpected cluster[%#lx], type(%d), fnode[%p]\n",
					"_set_start_cluster", cluster, type, fnode);
			fnode->tailCluster = (SceUInt32)-2;
			fnode->clusterCount = 0;
			fnode->chunkCount = 0;
		}
	} else if (cluster < _root_cluster(fileSystem)) {
		fileSystem = fnode->fileSystem;
		first = fileSystem->fileSystemType == 2 ? 2 : _root_cluster(fileSystem);
		if (cluster < first) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 464, "%s: before root directory\n", "_set_start_cluster");
			fnode->clusterCount = 0;
			fnode->tailCluster = (SceUInt32)-2;
			fnode->chunkCount = 0;
		} else {
			SCE_DBG_PRINTF_LEVEL(2, 0, 460,
				"%s: before root directory of exFAT\n", "_set_start_cluster");
			fnode->clusterCount = 1;
			fnode->chunkCount = 1;
		}
	} else {
		fnode->clusterCount = 1;
		fnode->chunkCount = 1;
	}
	element = fnode->cluster_cache_array;
	if (element) {
		element->cluster = fnode->tailCluster;
		element->index = fnode->tailIndex;
		element->count = cluster != 0;
	}
}

#if FW_VERSION >= 0x04000000
void _append_cluster_chunk(SceExfatfsFnode *fnode, unsigned int appendIndex,
	SceUInt32 start, unsigned int count, unsigned int flags)
{
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceUInt32 sectorSize = fileSystem->sectorSize;
	SceUInt32 clusterSectorCount = fileSystem->clusterSectorCount;

	SCE_DBG_ASSERT(count != 0, 1149);
	if (fnode->clusterCount) {
		if (fnode->tailIndex + 1 != appendIndex)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1155,
				"%s: unexpected checkedTailIndex[%#lx], checkedTailCluster[%#lx], clusterCount[%#lx], appendIndex[%#lx]\n",
				"_append_cluster_chunk", fnode->tailIndex, fnode->tailCluster,
				fnode->clusterCount, appendIndex);
		return;
	}
	if (appendIndex) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1165,
			"%s: clusterCount[%#lx], clusterIndex[%#lx], clusterSize[%#lx]\n",
			"_append_cluster_chunk", fnode->clusterCount, appendIndex,
			sectorSize * clusterSectorCount);
		SCE_DBG_ASSERT(0, 1166);
	}
	if (fnode->tailIndex != (SceUInt32)-2) {
		if (fnode->tailIndex)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1172,
				"%s: unexpected checkedTailIndex[%#lx], appendIndex[%#lx]\n",
				"_append_cluster_chunk", fnode->tailIndex, appendIndex);
		else if (fnode->tailCluster != (SceUInt32)-1)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1178,
				"%s: unexpected checkedTailCluster[%#lx], appendIndex[%#lx]\n",
				"_append_cluster_chunk", fnode->tailCluster, appendIndex);
	}
	SCE_DBG_PRINTF_LEVEL(2, 0, 1181,
		"%s: before _set_start_cluster, fnode[%p], clusterIndex[%#lx], count[%#lx], start[%#lx]\n",
		"_append_cluster_chunk", fnode, appendIndex, count, start);
	_set_start_cluster(fnode, start, flags & 2 ? 6 : fnode->Stat.st_type);
	SCE_DBG_PRINTF_LEVEL(2, 0, 1183,
		"%s: after _set_start_cluster, fnode[%p], clusterIndex[%#lx], count[%#lx], start[%#lx]\n",
		"_append_cluster_chunk", fnode, appendIndex, count, start);
}
#endif

int _read_into_fd_cache(SceExfatfsFile *file, SceUInt32 readCluster)
{
	SceUInt64 position = file->Position, cacheIndex;
	SceExfatfsFileSystem *fileSystem = file->fnode->fileSystem;
	SceUInt32 clusterSize, bufferSize, block;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	int result;

	if (!readCluster && !_root_cluster(fileSystem)) {
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		clusterSize = 32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
	} else {
		clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	}
	block = (position % clusterSize) / file->fnode->fileSystem->fileBufferSize;
	if (file->Cluster != readCluster) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5602, "%s: readCluster[%#x] != expected[%#x]\n",
			"_read_into_fd_cache", readCluster, file->Cluster);
		SCE_DBG_ASSERT(file->Cluster == readCluster, 5603);
	}
	result = UVFAT_ReadCblock(readCluster, block, fileSystem, file->Buf);
	SCE_DBG_PRINTF_LEVEL(2, 0, 5606,
		"%s: readCluster[%#lx], clustBlockNo[%#x], result[%#x]\n",
		"_read_into_fd_cache", readCluster, block, result);
	if (!result) {
		position = file->Position;
		fileSystem = file->fnode->fileSystem;
		bufferSize = fileSystem->fileBufferSize;
		clusterSize = fileSystemClusterSize(fileSystem);
		cacheIndex = position / bufferSize;
		file->cacheIndex = cacheIndex;
		block = cacheIndex % (clusterSize / bufferSize);
		file->cacheCluster = readCluster;
		file->clustBlockNo = block;
	}
	return result;
}

#if FW_VERSION >= 0x04000000
int _valid_cluster(SceExfatfsFnode *fnode, SceUInt32 cluster)
{
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	SceUInt32 first;

	if (!cluster) {
		if (!_root_cluster(fileSystem)) {
			boot = fnode->fileSystem->PBRBuf;
			rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
			if (rootEntries[0] | ((unsigned int)rootEntries[1] << 8))
				SCE_DBG_PRINTF_LEVEL(2, 0, 515,
					"%s: root directory of FAT12/FAT16\n", "_valid_cluster");
			else
				SCE_DBG_PRINTF_LEVEL(1, 0, 517,
					"%s: unexpected root_cluster[0]\n", "_valid_cluster");
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 520,
				"%s: can be root directory\n", "_valid_cluster");
			SCE_DBG_ASSERT(0, 521);
		}
	} else if (cluster < _root_cluster(fileSystem)) {
		fileSystem = fnode->fileSystem;
		first = fileSystem->fileSystemType == 2 ? 2 : _root_cluster(fileSystem);
		if (cluster >= first)
			SCE_DBG_PRINTF_LEVEL(2, 0, 529,
				"%s: before root directory of exFAT\n", "_valid_cluster");
		else
			SCE_DBG_PRINTF_LEVEL(1, 0, 531,
				"%s: before root directory\n", "_valid_cluster");
	}
	return cluster < fnode->fileSystem->clusterCount + 2;
}
#endif

static inline void _set_tail_cluster(SceExfatfsFnode *fnode, SceUInt32 cluster)
{
	if (fnode->tailCluster == cluster && cluster) {
		if (fnode->tailIndex == (SceUInt32)-2) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 484,
				"%s: clusterCount[%#lx]=>0, tailIndex[%#lx]\n",
				"_set_tail_cluster", fnode->clusterCount, fnode->tailIndex);
			fnode->clusterCount = 0;
		} else if (fnode->clusterCount != fnode->tailIndex + 1) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 488,
				"%s: clusterCount[%#lx] <= tailIndex[%#lx] + 1\n",
				"_set_tail_cluster", fnode->clusterCount, fnode->tailIndex);
			fnode->clusterCount = fnode->tailIndex + 1;
		}
	}
}

static inline void _update_current_tail(SceExfatfsFnode *fnode,
	SceUInt32 cluster, SceUInt32 next)
{
	SceUInt32 sectorSize, clusterSectorCount;

	if (cluster != fnode->tailCluster)
		return;
	sectorSize = fnode->fileSystem->sectorSize;
	clusterSectorCount = fnode->fileSystem->clusterSectorCount;
	SCE_DBG_PRINTF_LEVEL(2, 0, 502,
		"%s: tailIndex[%#lx], clusterCount[%#lx], cluster[%#lx], next[%#lx]\n",
		"_update_current_tail", fnode->tailIndex, fnode->clusterCount, cluster, next);
	if (next)
		_update_cached_index(fnode, fnode->tailIndex + 1, next,
			sectorSize * clusterSectorCount,
#if FW_VERSION >= 0x04000000
			0x101);
#else
			0);
#endif
}

extern int UVFAT_WriteDevice(SceExfatfsFileSystem *fileSystem,
	SceUInt64 sector, unsigned int size, const void *buffer);

#if FW_VERSION >= 0x04000000
static inline
#endif
int _write_sectors(SceExfatfsFile *file, unsigned int flags,
	SceExfatfsSharedResource *sharedResource, unsigned int contiguousCount,
	unsigned int size, const void *buffer)
{
	SceExfatfsFnode *fnode = file->fnode;
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceUInt64 position = file->Position;
	SceUInt64 bufferSize = fileSystem->fileBufferSize;
	unsigned int clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	SceUInt32 cluster;
	SceUInt64 areaSize, offset, amount;
	int result;

	if (position % bufferSize) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6083,
			"%s: unaligned Position[%#llx], cache_size[%#lx]\n",
			"_write_sectors", position, (unsigned int)bufferSize);
		return -32;
	}
	cluster = file->Cluster;
	if (cluster) {
		areaSize = (SceUInt64)contiguousCount * clusterSize;
		offset = position % clusterSize;
	} else {
		SceExfatfsBootSector *boot = fileSystem->PBRBuf;
		const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

		SCE_DBG_PRINTF_LEVEL(1, 0, 6088,
			"%s: Cluster[%#lx], clusterIndex[%#lx], Position[%#llx]\n",
			"_write_sectors", 0, file->clusterIndex, position);
		position = file->Position;
		areaSize = (rootEntries[0] | ((unsigned int)rootEntries[1] << 8)) * 32;
		offset = position;
	}
	amount = areaSize - offset;
	while (amount > size)
		amount -= bufferSize;
	if (!amount) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6154,
			"%s: shouldn't reach here for performance, size(%ld), update_size(%lld), area_size[%#llx], Position[%#llx], offset_in_block[%#llx], clusterSize[%#lx], contCount[%#lx]\n",
			"_write_sectors", size, amount, areaSize, position, offset,
			clusterSize, contiguousCount);
		return 0;
	}
	if (!(flags & 0x100)) {
		SceUInt64 sector = clusterToDeviceSector(cluster, fileSystem) +
			offset / fileSystem->sectorSize;
		SceUInt64 cacheIndex = file->cacheIndex;
#if FW_VERSION >= 0x04000000
		unsigned int expectedCacheSize = file->fnode->fileSystem->fileBufferSize;

		if (bufferSize != expectedCacheSize)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5637, "_write_direct_from_current",
				"%s: cblockSize[%#lx] != expected[%#lx]\n",
				"_write_direct_from_current", (unsigned int)bufferSize, expectedCacheSize);
#endif

		position = file->Position;
		if (cacheIndex != (SceUInt64)-1) {
			SceExfatfsFileSystem *cacheFileSystem = file->fnode->fileSystem;
			unsigned int cacheSize = cacheFileSystem->fileBufferSize;
			SceUInt64 cacheStart = cacheIndex * cacheSize;

			if (position < cacheStart + cacheSize && cacheStart < position + amount) {
				unsigned int overlap = position - cacheStart + amount;

				if (overlap >= cacheSize)
					overlap = cacheSize;
				if (overlap) {
					if (file->cacheFlags & 1) {
						if (overlap < cacheSize) {
							SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5643, "_write_direct_from_current",
								"%s: call _flush_fd_cache\n", "_write_direct_from_current");
#if FW_VERSION >= 0x04000000
							if (file->cacheFlags & 1) {
								int error = UVFAT_FlushFdBuf(file, file->fnode->fileSystem);

								if (error)
									SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5498, "_flush_fd_cache",
										"%s: UVFAT_FlushFdBuf() Fail[%#x]\n", "_flush_fd_cache", error);
							}
#else
							UVFAT_FlushFdBuf(file, cacheFileSystem);
#endif
						} else {
							SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5649, "_write_direct_from_current",
								"%s: direct write before flush\n", "_write_direct_from_current");
							file->cacheFlags = 0;
						}
					}
					file->cacheIndex = (SceUInt64)-1;
					file->cacheCluster = g_codepageTablesEnabled ? (SceUInt32)-2 : 0;
				}
			}
		}
		sync_multi_open(sharedResource, file);
		result = UVFAT_WriteDevice(fileSystem, sector, amount, buffer);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 6150,
				"%s: update_size[%#llx], buf[%p], error(%d), writtenSize[%#lx]\n",
				"_write_sectors", amount, buffer, result, 0);
			return result;
		}
		fnode = file->fnode;
		if (!fnode)
			goto advancePosition;
	}
#if FW_VERSION >= 0x04000000
	if (fnode) {
#endif
		getCurrentFatDateTime(&fnode->Stat.mtime);
		fnode->flags |= 4;
#if FW_VERSION >= 0x04000000
		fnode = file->fnode;
	}
#endif
advancePosition:
	file->Position += amount;
	if ((flags & 1) && file->fnode->Stat.Size < (SceUInt64)file->Position) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 6127,
			"%s: stat.Size[%#llx] < Position[%#llx]\n",
			"_write_sectors", file->fnode->Stat.Size, file->Position);
		file->fnode->Stat.Size = file->Position;
	} else if (!(flags & 1) && file->fnode->dirSize && file->fnode->dirSize < (SceUInt64)file->Position) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6133,
			"%s: unexpected dirSize[%#x] < Position[%#llx]\n",
			"_write_sectors", file->fnode->dirSize, file->Position);
	}
	cluster = file->Cluster;
	if (cluster) {
		unsigned int advance = (offset - 1 + amount) / clusterSize;

		SCE_DBG_ASSERT_FUNC(offset + amount != 0, 187, "_last_cluster_index");
		file->Cluster = cluster + advance;
		file->clusterIndex += advance;
	}
	return amount;
}

SceUInt32 _next_cluster(SceExfatfsFnode *fnode, SceUInt32 cluster)
{
	SceUInt32 next = (SceUInt32)-1;
	SceUInt32 result = (SceUInt32)-1;
	int error;

	error = UVFAT_SetGetFAT(fnode->fileSystem, 0, cluster, &next);
	if (error) {
		next = (SceUInt32)-2;
		SCE_DBG_PRINTF_LEVEL(1, 0, 546,
			"%s: cluster[%#lx], next[%#lx], error(%d)\n",
			"_next_cluster", cluster, next, error);
		if (fnode->tailCluster == cluster)
			_set_tail_cluster(fnode, cluster);
		else
			SCE_DBG_PRINTF_LEVEL(1, 0, 551,
				"%s: invalid cluster[%#lx], fnode[%p], tailCluster[%#lx], tailIndex[%#lx], clusterCount[%#lx]\n",
				"_next_cluster", cluster, fnode, fnode->tailCluster,
				fnode->tailIndex, fnode->clusterCount);
		result = next;
	} else if (!next) {
		_exfatfs_notify_broken_cluster_chain(fnode->fileSystem->drive, cluster, next);
		_set_tail_cluster(fnode, cluster);
	} else {
#if FW_VERSION >= 0x04000000
		if (_valid_cluster(fnode, next)) {
#else
		if (next < fnode->fileSystem->clusterCount + 2) {
#endif
			SCE_DBG_PRINTF_LEVEL(2, 0, 575,
				"%s: cluster[%#lx]->next[%#lx], fnode[%p]\n",
				"_next_cluster", cluster, next, fnode);
			_update_current_tail(fnode, cluster, next);
		} else if (fnode->tailCluster == cluster) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 579, "%s: tail cluster[%#lx], fnode[%p]\n",
				"_next_cluster", fnode->tailCluster, fnode);
			_set_tail_cluster(fnode, cluster);
		} else {
			SCE_DBG_PRINTF_LEVEL(2, 0, 583,
				"%s: unexpected cluster[%#lx]->[%lx], fnode[%p], tailCluster[%#lx], tailIndex[%#lx], clusterCount[%#lx]\n",
				"_next_cluster", cluster, next, fnode, fnode->tailCluster,
				fnode->tailIndex, fnode->clusterCount);
		}
		result = next;
	}
	return result;
}

#if FW_VERSION >= 0x04000000
static inline
#endif
unsigned int _check_contiguous_clusters(SceExfatfsFile *file,
	SceUInt64 position, SceUInt64 size, unsigned int flags)
{
	SceExfatfsFnode *fnode = file->fnode;
	SceUInt32 cluster = file->Cluster;
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	unsigned int sectorSize = fileSystem->sectorSize;
	unsigned int clusterSectorCount = fileSystem->clusterSectorCount;
	unsigned int clusterIndex, clusterSize, contiguous;
	SceUInt64 end, lastIndex;
	int noFatChain;
#if FW_VERSION >= 0x04000000
	unsigned int generalSecondaryFlags;
#endif

	if (!cluster) {
		SceExfatfsBootSector *boot = fileSystem->PBRBuf;
		const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

#if FW_VERSION >= 0x04000000
		if (position < (rootEntries[0] | ((unsigned int)rootEntries[1] << 8)) * 32) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 1507,
				"%s: root_directory of FAT12/FAT16\n", "_check_contiguous_clusters");
			return 1;
		}
		return 0;
#else
		return position < (rootEntries[0] | ((unsigned int)rootEntries[1] << 8)) * 32;
#endif
	}
	clusterIndex = file->clusterIndex;
	end = position + size;
	if (flags & 1) {
#if FW_VERSION >= 0x04000000
		generalSecondaryFlags = fnode->Stat.generalSecondaryFlags;
		noFatChain = generalSecondaryFlags & 2;
#else
		noFatChain = fnode->Stat.generalSecondaryFlags & 2;
#endif
		if (noFatChain && fnode->Stat.Size < end)
			end = fnode->Stat.Size;
	} else {
#if FW_VERSION >= 0x04000000
		generalSecondaryFlags = fnode->dirGeneralSecondaryFlags;
		noFatChain = generalSecondaryFlags & 2;
#else
		noFatChain = fnode->dirGeneralSecondaryFlags & 2;
#endif
		if (noFatChain && fnode->dirSize < end)
			end = fnode->dirSize;
	}
	if (cluster == (SceUInt32)-2) {
		if (end)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1460,
				"%s: unexpected size[%#llx]\n", "_check_contiguous_clusters", end);
		return 0;
	}
	clusterSize = sectorSize * clusterSectorCount;
#if FW_VERSION >= 0x04000000
	if (!end) {
		if (flags & 1)
			SCE_DBG_PRINTF_LEVEL(2, 0, 1467,
				"%s: Cluster[%#lx] for size 0, dirFlag[%#x], nfcFlag[%#x]\n",
				"_check_contiguous_clusters", cluster, flags, generalSecondaryFlags);
		else
			SCE_DBG_PRINTF_LEVEL(1, 0, 1469,
				"%s: Cluster[%#lx] for size 0, dirFlag[%#x], nfcFlag[%#x]\n",
				"_check_contiguous_clusters", cluster, flags, generalSecondaryFlags);
	}
#endif
	if (noFatChain) {
#if FW_VERSION >= 0x04000000
		if (end)
			SCE_DBG_ASSERT_FUNC(end != 0, 187, "_last_cluster_index");
#endif
		contiguous = end ? (end - 1) / clusterSize : 0;
	} else {
		lastIndex = (end - 1) / clusterSize;
		if (clusterIndex < lastIndex) {
			for (;;) {
				SceUInt32 next;

#if FW_VERSION >= 0x04000000
				SCE_DBG_PRINTF_LEVEL(2, 0, 1485,
					"%s: fnode[%p], index[%#lx], temp[%#lx], checkedTailIndex[%#lx], checkedTail[%#lx], filep->index[%#lx], filep->Cluster[%#lx]\n",
					"_check_contiguous_clusters", file->fnode, clusterIndex, cluster,
					file->fnode->tailIndex, file->fnode->tailCluster, file->clusterIndex, file->Cluster);
				fnode = file->fnode;
#endif
				if (fnode->tailIndex < clusterIndex && cluster) {
					_update_cached_index(fnode, clusterIndex, cluster, clusterSize,
#if FW_VERSION >= 0x04000000
						flags);
#else
						0);
#endif
					fnode = file->fnode;
				}
				next = _next_cluster(fnode, cluster);
				if (next != cluster + 1)
					break;
				cluster = next;
				if (++clusterIndex >= lastIndex)
					break;
				fnode = file->fnode;
			}
			contiguous = clusterIndex;
			clusterIndex = file->clusterIndex;
		} else {
			contiguous = clusterIndex;
		}
	}
#if FW_VERSION >= 0x04000000
	if (contiguous >= clusterIndex) {
		unsigned int count = contiguous + 1 - clusterIndex;

		SCE_DBG_PRINTF_LEVEL(2, 0, 1499,
			"%s: clusterIndex[%#lx], tailIndex[%#lx], count[%#lx]\n",
			"_check_contiguous_clusters", clusterIndex, contiguous, count);
		return count;
	}
	SCE_DBG_PRINTF_LEVEL(1, 0, 1502,
		"%s: clusterIndex[%#lx] > tailIndex[%#lx], count[%#lx]\n",
		"_check_contiguous_clusters", clusterIndex, contiguous, 0);
	return 0;
#else
	return contiguous >= clusterIndex ? contiguous + 1 - clusterIndex : 0;
#endif
}

SceUInt32 _contiguous_cluster_count(SceExfatfsFile *file,
	unsigned int requestSize, unsigned int clusterSize)
{
	SceUInt64 position = file->Position;
	unsigned int offset = position % clusterSize;
	SceExfatfsFnode *fnode = file->fnode;
	SceUInt64 size = fnode->Stat.Size;
	SceUInt32 cluster = file->Cluster, lastIndex;

	if (size - position < requestSize) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5671,
			"%s: unexpected request_size[%#lx] > Size[%#llx] - Position[%#llx]\n",
			"_contiguous_cluster_count", requestSize, size, position);
		fnode = file->fnode;
		if (fnode->Stat.generalSecondaryFlags & 2) {
			position = file->Position;
			requestSize = fnode->Stat.Size - position;
			goto contiguous;
		}
	} else if (fnode->Stat.generalSecondaryFlags & 2) {
contiguous:
		SCE_DBG_ASSERT_FUNC(position + requestSize != 0, 187, "_last_cluster_index");
		lastIndex = (position + requestSize - 1) / clusterSize;
		SCE_DBG_ASSERT_FUNC(requestSize + offset != 0, 187, "_last_cluster_index");
		cluster += ((SceUInt64)(requestSize + offset) - 1) / clusterSize;
		fnode = file->fnode;
		if (fnode->tailIndex == (SceUInt32)-2 || fnode->tailIndex < lastIndex) {
			fnode->tailCluster = cluster;
			fnode->tailIndex = lastIndex;
		}
		return 1 - file->Cluster + cluster;
	}
	while (clusterSize < offset + requestSize) {
		requestSize += offset - clusterSize;
		offset = 0;
		if (cluster + 1 != _next_cluster(fnode, cluster))
			break;
		cluster++;
		if (clusterSize >= requestSize)
			break;
		fnode = file->fnode;
	}
	return 1 - file->Cluster + cluster;
}

static inline void _get_cluster_from_array(SceExfatfsFile *file,
	SceExfatfsFnode *fnode, unsigned int searchIndex)
{
	SceExfatfsClusterCacheElement *array = fnode->cluster_cache_array;
	unsigned int interval = _cluster_cache_interval(fnode);
	SceExfatfsClusterCacheElement *element;

	SCE_DBG_PRINTF_LEVEL(2, 0, 1192,
		"%s: search_index[%#lx], interval[%#lx], fnode[%p], clusterCount[%#lx]\n",
		"_get_cluster_from_array", searchIndex, interval, fnode, fnode->clusterCount);
	if (fnode != file->fnode)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1194, "%s: unexpected fnode[%p], filep[%p]\n",
			"_get_cluster_from_array", fnode, file);
	if (searchIndex >= fnode->tailIndex) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 1197,
			"%s: clusterCount[%#lx], checkedTailIndex[%#lx], search_index[%#lx], fnode[%p], checkedTailCluster[%#lx]\n",
			"_get_cluster_from_array", fnode->clusterCount, fnode->tailIndex,
			searchIndex, fnode, fnode->tailCluster);
		file->Cluster = fnode->tailCluster;
		file->clusterIndex = fnode->tailIndex;
		return;
	}
	file->Cluster = (SceUInt32)-2;
	file->clusterIndex = 0;
	if (searchIndex >= fnode->clusterCount) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 1205,
			"%s: check beyond EOF, interval[%#lx], search_index[%#lx], clusterCount[%#lx], cache_element_count[%#x]\n",
			"_get_cluster_from_array", interval, searchIndex,
			fnode->clusterCount, fnode->cache_element_count);
		return;
	}
	element = array + searchIndex / interval;
	while (element >= fnode->cluster_cache_array) {
#if FW_VERSION >= 0x04000000
		__builtin_prefetch((const unsigned char *)element - 36);
#endif
		if (!element->cluster) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1211,
				"%s: search_index[%#lx], interval[%#lx], fnode[%p], clusterCount[%#lx]\n",
				"_get_cluster_from_array", searchIndex, interval, fnode, fnode->clusterCount);
			SCE_DBG_PRINTF_LEVEL(1, 0, 1212,
				"%s: index(%d), cluster_index[%#lx], count[%#lx], unexpected start[%#lx], search_index[%#lx]\n",
				"_get_cluster_from_array", element - fnode->cluster_cache_array,
				element->index, element->count, element->cluster, searchIndex);
		} else {
			SCE_DBG_PRINTF_LEVEL(2, 0, 1214,
				"%s: index(%d), cluster_index[%#lx], count[%#lx], start[%#lx], search_index[%#lx]\n",
				"_get_cluster_from_array", element - fnode->cluster_cache_array,
				element->index, element->count, element->cluster, searchIndex);
		}
		if (element->count && element->cluster != (SceUInt32)-2 && searchIndex >= element->index) {
			if (searchIndex < element->index + element->count) {
				file->clusterIndex = searchIndex;
				file->Cluster = element->cluster + searchIndex - element->index;
			} else {
				file->Cluster = element->cluster + element->count - 1;
				file->clusterIndex = element->index + element->count - 1;
			}
			break;
		}
		element--;
	}
}

#if FW_VERSION >= 0x04000000
static inline
#endif
int _get_cached_index(SceExfatfsFile *file, SceUInt64 offset, int forDirectorySearch)
{
	SceExfatfsFnode *fnode = file->fnode;
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceUInt32 clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	unsigned int searchIndex = offset / clusterSize;
	unsigned int currentIndex, rootEntryCount;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;

	SCE_DBG_PRINTF_LEVEL(2, 0, 1244,
		"%s: filep[%p], fnode[%p], offset[%#llx], search_index[%#lx], current_index[%#lx], cluster_cache_array[%p]\n",
		"_get_cached_index", file, fnode, offset, searchIndex,
		file->clusterIndex, fnode->cluster_cache_array);
	if (file->fnode->cluster_cache_array) {
		_get_cluster_from_array(file, file->fnode, searchIndex);
		SCE_DBG_PRINTF_LEVEL(2, 0, 1248,
			"%s: after _get_cluster_from_array, fnode[%p], search_index[%#lx], index[%#lx], cluster[%#lx], clusterCount[%#lx]\n",
			"_get_cached_index", file->fnode, searchIndex, file->clusterIndex,
			file->Cluster, file->fnode->clusterCount);
	}
	currentIndex = file->clusterIndex;
	if (searchIndex < currentIndex || file->Cluster == (SceUInt32)-2) {
		currentIndex = 0;
		file->clusterIndex = 0;
		file->Cluster = _start_cluster(file->fnode, forDirectorySearch);
	}
	if (!file->Cluster) {
		fnode = file->fnode;
		if (!forDirectorySearch && !fnode->Stat.st_type) {
			file->Position = 0;
			return -7;
		}
		boot = fnode->fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		rootEntryCount = rootEntries[0] | ((unsigned int)rootEntries[1] << 8);
		SCE_DBG_PRINTF_LEVEL(2, 0, 1265,
			"%s: fnode[%p], checkedTailCluster[%#lx], checkedTailIndex[%#lx], clusterCount[%#lx], chunkCount[%#lx], current_index[%#lx]\n",
			"_get_cached_index", fnode, fnode->tailCluster, fnode->tailIndex,
			fnode->clusterCount, fnode->chunkCount, currentIndex);
		if (rootEntryCount) {
			SCE_DBG_ASSERT(file->fnode->clusterCount == 0 && file->fnode->chunkCount == 1, 1268);
			if (currentIndex) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 1270,
					"%s: wrong current_index[%#lx]\n", "_get_cached_index", currentIndex);
				file->clusterIndex = 0;
			}
			if ((offset >> 5) >= (SceUInt16)rootEntryCount) {
				SCE_DBG_PRINTF_LEVEL(2, 0, 1274,
					"%s: offset[%#llx], index[%#llx] >= entry_count[%#x]\n",
					"_get_cached_index", offset, offset >> 5, rootEntryCount);
				file->Position = (SceUInt64)32 * rootEntryCount;
				return -7;
			}
			SCE_DBG_PRINTF_LEVEL(2, 0, 1278,
				"%s: offset[%#llx], index[%#llx] < entry_count[%#x]\n",
				"_get_cached_index", offset, offset >> 5, rootEntryCount);
			return 0;
		}
		SCE_DBG_PRINTF_LEVEL(1, 0, 1282,
			"%s: unexpected cluster[%#lx] for directory\n", "_get_cached_index", file->Cluster);
		SCE_DBG_ASSERT(file->Cluster != 0, 1283);
	}
	if (searchIndex < currentIndex) {
		if (currentIndex == (SceUInt32)-2)
			SCE_DBG_PRINTF_LEVEL(2, 0, 1290,
				"%s: search from start, search_index[%#lx], current_index[%#lx], for_directory_search[%#x]\n",
				"_get_cached_index", searchIndex, currentIndex, forDirectorySearch);
		else
			SCE_DBG_PRINTF_LEVEL(1, 0, 1293,
				"%s: search from start, search_index[%#lx], current_index[%#lx], for_directory_search[%#x]\n",
				"_get_cached_index", searchIndex, currentIndex, forDirectorySearch);
		file->clusterIndex = 0;
		SCE_DBG_PRINTF_LEVEL(2, 0, 1297,
			"%s: call _start_cluster, for_directory_search[%#x]\n", "_get_cached_index", forDirectorySearch);
		file->Cluster = _start_cluster(file->fnode, forDirectorySearch);
	} else if (searchIndex != currentIndex) {
		if (searchIndex >= file->fnode->tailIndex && currentIndex < file->fnode->tailIndex) {
			file->clusterIndex = file->fnode->tailIndex;
			file->Cluster = file->fnode->tailCluster;
			SCE_DBG_PRINTF_LEVEL(2, 0, 1307,
				"%s: search from tail, search_index[%#lx], tail_index[%#lx]\n",
				"_get_cached_index", searchIndex, file->clusterIndex);
		} else {
			SCE_DBG_PRINTF_LEVEL(2, 0, 1310,
				"%s: search from current, search_index[%#lx], current_index[%#lx]\n",
				"_get_cached_index", searchIndex, currentIndex);
		}
	}
	SCE_DBG_PRINTF_LEVEL(2, 0, 1313,
		"%s(exit): filep[%p], offset[%#llx], search_index[%#lx], current_index[%#lx], cluster_cache_array[%p], resultIndex[%#lx], resultCluster[%#lx]\n",
		"_get_cached_index", file, offset, searchIndex, currentIndex,
		file->fnode->cluster_cache_array, file->clusterIndex, file->Cluster);
	if (searchIndex == file->clusterIndex) {
#if FW_VERSION >= 0x04000000
		if (_valid_cluster(file->fnode, file->Cluster))
#else
		if (file->Cluster < fileSystem->clusterCount + 2)
#endif
			return 0;
	}
	return -37;
}

static inline unsigned int _cluster_chain_length(SceExfatfsFnode *fnode,
	SceUInt32 cluster)
{
	unsigned int index = 0;

	SCE_DBG_ASSERT(cluster != 0, 597);
#if FW_VERSION >= 0x04000000
	while (_valid_cluster(fnode, cluster)) {
#else
	while (cluster < fnode->fileSystem->clusterCount + 2) {
#endif
		SCE_DBG_PRINTF_LEVEL(2, 0, 599, "%s: fnode[%p], index(%ld), cluster[%#lx]\n",
			"_cluster_chain_length", fnode, index, cluster);
		index++;
		cluster = _next_cluster(fnode, cluster);
	}
	return index;
}

static inline int _is_directory(SceExfatfsFnode *fnode)
{
	unsigned int type = fnode->Stat.st_type;
	unsigned int mode = fnode->Stat.st_mode & 0xf000;

	if (mode) {
		if (mode == (type == 6 ? 0x1000 : 0x2000)) {
			if (type == 6)
				SCE_DBG_PRINTF_LEVEL(1, 0, 860, "%s: st_mode[%#o] for st_type(%d)\n",
					"_is_directory", fnode->Stat.st_mode, type);
			else
				SCE_DBG_PRINTF_LEVEL(2, 0, 860, "%s: st_mode[%#o] for st_type(%d)\n",
					"_is_directory", fnode->Stat.st_mode, type);
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 863, "%s: unexpected st_mode[%#o] for st_type(%d)\n",
				"_is_directory", fnode->Stat.st_mode, type);
		}
	}
	return type == 6;
}

#if FW_VERSION < 0x04000000
SceUInt64 clusterChainByteSize(SceExfatfsFnode *fnode, SceUInt32 cluster)
{
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	unsigned int blockSize, count = 0;

	if (cluster || _root_cluster(fileSystem)) {
		blockSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	} else {
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		blockSize = 32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
	}
	if (cluster >= fileSystem->clusterCount + 2)
		return 0;
	do {
		count++;
		cluster = _next_cluster(fnode, cluster);
	} while (cluster < fnode->fileSystem->clusterCount + 2);
	return (SceUInt64)count * blockSize;
}
#endif

static inline SceUInt64 _allocated_size(SceExfatfsFnode *fnode, int forDirectorySearch)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	SceUInt32 cluster, blockSize;
	SceUInt64 size;

	SCE_DBG_PRINTF_LEVEL(2, 0, 873, "%s: fnode[%p], for_directory_search[%#x]\n",
		"_allocated_size", fnode, forDirectorySearch);
	fileSystem = fnode->fileSystem;
	if (forDirectorySearch) {
		cluster = fnode->DirCluster;
		if (!cluster) {
			if (_root_cluster(fileSystem))
				return fileSystem->sectorSize * fileSystem->clusterSectorCount;
			boot = fileSystem->PBRBuf;
			rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
			return 32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
		}
		blockSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
		size = fnode->dirSize;
		if (!size)
			size = (SceUInt64)_cluster_chain_length(fnode, cluster) * blockSize;
	} else {
		cluster = fnode->Stat.StartCluster;
		if (cluster || (fnode->Stat.st_type == 6 && _root_cluster(fileSystem))) {
			blockSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
		} else if (fnode->Stat.st_type == 6) {
			boot = fileSystem->PBRBuf;
			rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
			blockSize = 32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
		} else {
			blockSize = 0;
		}
		if (_is_directory(fnode)) {
			if (!cluster)
				return blockSize;
			size = fnode->Stat.Size;
			if (!size) {
				unsigned int clusterSize = fnode->fileSystem->sectorSize *
					fnode->fileSystem->clusterSectorCount;
				size = (SceUInt64)_cluster_chain_length(fnode, cluster) * clusterSize;
			}
		} else {
			size = fnode->Stat.Size;
		}
	}
	return (SceUInt64)(unsigned int)((size + blockSize - 1) / blockSize) * blockSize;
}

static inline SceUInt64 _contiguous_block_size(SceExfatfsFnode *fnode,
	SceUInt32 cluster, SceUInt64 requestSize, int forDirectorySearch)
{
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	SceUInt32 clusterSize, start, current = cluster, next, clusterIndex;
	SceUInt64 size;

	SCE_DBG_PRINTF_LEVEL(2, 0, 1051,
		"%s: fnode[%p], cluster[%#lx], for_directory_search[%#x]\n",
		"_contiguous_block_size", fnode, cluster, forDirectorySearch);
	if (!cluster)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1053, "%s: unexpected cluster[%#lx]\n",
			"_contiguous_block_size", cluster);
	SCE_DBG_ASSERT(cluster != 0, 1055);
#if FW_VERSION >= 0x04000000
	SCE_DBG_ASSERT(_valid_cluster(fnode, cluster), 1056);
#endif
	if (!cluster) {
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		size = 32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
		SCE_DBG_PRINTF_LEVEL(2, 0, 1108, "%s: fnode[%p], root_entry[%#lx], size[%#llx]\n",
			"_contiguous_block_size", fnode, cluster, size);
		return size;
	}
	clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	if (fnode->fileSystem->fileSystemType == 2 &&
		((forDirectorySearch ? fnode->dirGeneralSecondaryFlags : fnode->Stat.generalSecondaryFlags) & 2)) {
		start = _start_cluster(fnode, forDirectorySearch);
		size = _allocated_size(fnode, forDirectorySearch);
		if (cluster != start) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 1072, "%s: cluster[%#lx] != start[%#lx]\n",
				"_contiguous_block_size", cluster, start);
			if (cluster < start) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 1074, "%s: cluster[%#lx] < start[%#lx]\n",
					"_contiguous_block_size", cluster, start);
				SCE_DBG_ASSERT(0, 1075);
			} else {
				clusterIndex = cluster - start;
				if (clusterIndex >= size / clusterSize) {
					SCE_DBG_PRINTF_LEVEL(1, 0, 1080,
						"%s: cluster_index[%#lx] > allocation_size[%#llx]\n",
						"_contiguous_block_size", clusterIndex, size);
					size = 0;
				} else {
					size -= (SceUInt64)clusterIndex * clusterSize;
				}
			}
		}
	} else {
		size = clusterSize;
		while (size < requestSize) {
			next = _next_cluster(fnode, current);
			if (next != current + 1)
				break;
			size += clusterSize;
			current = next;
		}
	}
	SCE_DBG_PRINTF_LEVEL(2, 0, 1105, "%s: fnode[%p], cluster[%#lx], size[%#llx]\n",
		"_contiguous_block_size", fnode, current, size);
	return size;
}

static inline SceUInt32 _next_block(SceExfatfsFnode *fnode,
	SceUInt32 cluster, int forDirectorySearch)
{
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;

	if (!cluster) {
		if (!forDirectorySearch) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 700, "%s: unexpected cluster[%#lx], flags[%#x]\n",
				"_next_block", cluster, forDirectorySearch);
		} else {
			boot = fnode->fileSystem->PBRBuf;
			rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
			if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8)))
				SCE_DBG_PRINTF_LEVEL(1, 0, 703, "%s: unexpected cluster[%#lx], flags[%#x]\n",
					"_next_block", cluster, forDirectorySearch);
		}
		return (SceUInt32)-1;
	}
	if (fnode->fileSystem->fileSystemType == 2 &&
		((forDirectorySearch ? fnode->dirGeneralSecondaryFlags : fnode->Stat.generalSecondaryFlags) & 2))
		return (SceUInt32)-1;
	return _next_cluster(fnode, cluster);
}

int _set_position_and_cluster_index(SceExfatfsFile *file, SceUInt64 offset,
	unsigned int flags)
{
	SceExfatfsFileSystem *fileSystem = file->fnode->fileSystem;
	SceUInt32 clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	int forDirectorySearch = flags & 2;
	SceUInt32 cluster, index, count, next;
	SceUInt64 current, remaining, size;
	int result;

	result = _get_cached_index(file, offset, forDirectorySearch);
	if (result != -37) {
		if (result == -7)
			goto end_of_file;
		if (!result)
			file->Position = offset;
		return result;
	}
	index = file->clusterIndex;
	cluster = file->Cluster;
	SCE_DBG_PRINTF_LEVEL(2, 0, 1332,
		"%s(before loop): filep[%p], fs[%p], offset[%#llx], cluster[%#lx], index[%#lx]\n",
		"_set_position_and_cluster_index", file, fileSystem, offset, cluster, index);
	if (!cluster)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1334, "%s: unexpected cluster[%#lx]\n",
			"_set_position_and_cluster_index", cluster);
	SCE_DBG_ASSERT(cluster != 0, 1336);
	for (;;) {
#if FW_VERSION >= 0x04000000
		if (!_valid_cluster(file->fnode, cluster)) {
#else
		if (cluster >= file->fnode->fileSystem->clusterCount + 2) {
#endif
			SCE_DBG_PRINTF_LEVEL(2, 0, 1389,
				"%s: set EOF, filep[%p], offset[%#llx], for_directory_search[%#x]\n",
				"_set_position_and_cluster_index", file, offset, forDirectorySearch);
			goto end_of_file;
		}
		current = (SceUInt64)index * clusterSize;
		remaining = offset - current;
		size = _contiguous_block_size(file->fnode, cluster, remaining + 1, forDirectorySearch);
		SCE_DBG_PRINTF_LEVEL(2, 0, 1340,
			"%s(after _contiguous_block_size): filep[%p], fs[%p], offset[%#llx], current[%#llx], cluster[%#lx], index[%#lx], chunk_size[%#llx]\n",
			"_set_position_and_cluster_index", file, fileSystem, offset, current, cluster, index, size);
		if (cluster)
			_update_cached_index(file->fnode, index, cluster, size, flags);
		SCE_DBG_PRINTF_LEVEL(2, 0, 1342,
			"%s(after _update_cached_index): filep[%p], fs[%p], offset[%#llx], current[%#llx], cluster[%#lx], index[%#lx], chunk_size[%#llx]\n",
			"_set_position_and_cluster_index", file, fileSystem, offset, current, cluster, index, size);
		if (remaining < size) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 1376,
				"%s(found): filep[%p], fs[%p], offset[%#llx], current[%#llx], cluster[%#lx], index[%#lx], chunk_size[%#llx]\n",
				"_set_position_and_cluster_index", file, fileSystem, offset, current, cluster, index, size);
			if (cluster) {
				count = remaining / clusterSize;
				cluster += count;
				index += count;
			}
			file->Cluster = cluster;
			file->clusterIndex = index;
			file->Position = offset;
			return 0;
		}
		if (cluster) {
			SCE_DBG_ASSERT_FUNC(size != 0, 187, "_last_cluster_index");
			count = (size - 1) / clusterSize;
			cluster += count;
			index += count;
		}
		file->Cluster = cluster;
		file->clusterIndex = index;
		next = _next_block(file->fnode, cluster, forDirectorySearch);
		if (!next)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1358, "%s: unexpected cluster[%#lx]\n",
				"_set_position_and_cluster_index", next);
		SCE_DBG_ASSERT(next != 0, 1360);
		if (next == (SceUInt32)-1) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 1371,
				"%s(after _next_block): filep[%p], fs[%p], offset[%#llx], cluster[%#lx], index[%#lx]\n",
				"_set_position_and_cluster_index", file, fileSystem, offset, next, index);
			goto end_of_file;
		}
		if (file->Cluster == next) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1363,
				"%s: found cluster chain loop, Cluster[%#lx]->next[%#lx], offset[%#llx], index[%#lx]\n",
				"_set_position_and_cluster_index", file->Cluster, next, offset, index);
			goto end_of_file;
		}
		cluster = next;
		index++;
		SCE_DBG_PRINTF_LEVEL(2, 0, 1369,
			"%s(after _next_block): filep[%p], fs[%p], offset[%#llx], cluster[%#lx], index[%#lx]\n",
			"_set_position_and_cluster_index", file, fileSystem, offset, cluster, index);
	}

end_of_file:
	if (file->Cluster)
		file->Position = (SceUInt64)clusterSize * (file->clusterIndex + 1);
	return -7;
}

void _set_root_cluster_and_cluster_count(SceExfatfsFnode *fnode,
	SceUInt32 cluster, SceUInt64 size)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	SceUInt32 clusterSize;

	if (fnode->Stat.st_type != 6) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 761, "%s: type(%d) != stat.st_type(%d)\n",
			"_set_root_cluster_and_cluster_count", 6, fnode->Stat.st_type);
		fnode->Stat.st_type = 6;
	}
	_set_start_cluster(fnode, cluster, 6);
	if (fnode->clusterCount) {
		if (!size) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 768, "%s: check directory size\n",
				"_set_root_cluster_and_cluster_count");
			fileSystem = fnode->fileSystem;
			if (cluster) {
				clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
				size = (SceUInt64)_cluster_chain_length(fnode, cluster) * clusterSize;
			} else if (_root_cluster(fileSystem)) {
				size = fileSystem->sectorSize * fileSystem->clusterSectorCount;
			} else {
				boot = fileSystem->PBRBuf;
				rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
				size = 32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
			}
			fnode->Stat.Size = size;
		}
		clusterSize = fnode->fileSystem->sectorSize * fnode->fileSystem->clusterSectorCount;
		SCE_DBG_ASSERT_FUNC(size != 0, 187, "_last_cluster_index");
		fnode->clusterCount = (size - 1) / clusterSize + 1;
	} else if (!cluster) {
		boot = fnode->fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
#if FW_VERSION >= 0x04000000
		size = 32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
#else
		size = (SceUInt64)32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
#endif
		fnode->Stat.Size = size;
	}
	SCE_DBG_PRINTF_LEVEL(2, 0, 781,
		"%s: fnode[%p], cluster[%#lx], size[%#llx], clusterCount[%#lx]\n",
		"_set_root_cluster_and_cluster_count", fnode, cluster, size, fnode->clusterCount);
}

int _get_root_fnode(SceExfatfsFnode *fnode, SceExfatfsFileSystem *fileSystem)
{
	unsigned char attribute[4];
	int result;

	fnode->Stat.st_type = 6;
	attribute[0] = 0;
	result = sceFatfsACGetRootDirAttribute(fileSystem->drive, attribute);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2846,
			"%s: sceFatfsACGetRootDirAttribute failed[%#x]\n", "_get_root_fnode", result);
		return -5;
	}
	fnode->Stat.st_mode = _sceFatfsWriteModeFromStat(fileSystem->drive, attribute[0]);
	if (attribute[0] == 15)
		SCE_DBG_PRINTF_LEVEL(2, 0, 2851, "%s: special root_dir, st_mode[%#o]\n",
			"_get_root_fnode", fnode->Stat.st_mode);
	fnode->Stat.st_attr = attribute[0];
	fnode->Stat.st_uid = 0;
	initializeFatDateTime(&fnode->Stat.ctime);
	initializeFatDateTime(&fnode->Stat.atime);
	initializeFatDateTime(&fnode->Stat.mtime);
	fnode->Stat.Size = 0;
	fnode->Stat.StartCluster = _root_cluster(fileSystem);
	_initialize_cluster_cache_data(fnode);
	_set_root_cluster_and_cluster_count(fnode, fnode->Stat.StartCluster, fnode->Stat.Size);
	return result;
}

void _set_start_cluster_and_cluster_count(SceExfatfsFnode *fnode,
	SceUInt32 cluster, SceUInt64 size)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	SceUInt32 clusterSize;
	SceUInt64 lastByte;

	_set_start_cluster(fnode, cluster, fnode->Stat.st_type);
	if (fnode->clusterCount) {
		if (!size && fnode->Stat.st_type == 6) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 792, "%s: check directory size\n",
				"_set_start_cluster_and_cluster_count");
			fileSystem = fnode->fileSystem;
			if (cluster) {
				clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
				size = (SceUInt64)_cluster_chain_length(fnode, cluster) * clusterSize;
			} else if (_root_cluster(fileSystem)) {
				size = fileSystem->sectorSize * fileSystem->clusterSectorCount;
			} else {
				boot = fileSystem->PBRBuf;
				rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
				size = 32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
			}
			fnode->Stat.Size = size;
		}
		lastByte = size - 1;
		clusterSize = fnode->fileSystem->sectorSize * fnode->fileSystem->clusterSectorCount;
		SCE_DBG_ASSERT_FUNC(size != 0, 187, "_last_cluster_index");
		fnode->clusterCount = lastByte / clusterSize + 1;
	}
	SCE_DBG_PRINTF_LEVEL(2, 0, 802,
		"%s: fnode[%p], cluster[%#lx], type(%d), size[%#llx], clusterCount[%#lx]\n",
		"_set_start_cluster_and_cluster_count", fnode, cluster,
		fnode->Stat.st_type, size, fnode->clusterCount);
}

SceExfatfsFile *invalidateFileCachePosition(SceExfatfsFile *file)
{
	int enabled = g_codepageTablesEnabled;

	file->cacheIndex = (SceUInt64)-1;
	file->cacheCluster = enabled ? (SceUInt32)-2 : 0;
	return file;
}

SceUInt32 UVFAT_NextCluster(SceExfatfsSharedResource *sharedResource,
	SceUInt32 cluster, SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	SceUInt32 first, next;
	int rootEntryCount;

	SCE_DBG_PRINTF_LEVEL(2, 0, 4859, "%s: fileSystem[%p], current[%#lx]\n",
		"UVFAT_NextCluster", fileSystem, cluster);
	if (!cluster) {
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		rootEntryCount = rootEntries[0] | ((unsigned int)rootEntries[1] << 8);
#if FW_VERSION >= 0x04000000
		rootEntryCount = (short)rootEntryCount;
#endif
		if (rootEntryCount)
			goto invalid;
		cluster = _root_cluster(fileSystem);
		SCE_DBG_PRINTF_LEVEL(1, 0, 4881,
			"%s: unexpected current[%#x]->root_cluster[%#lx]\n",
			"UVFAT_NextCluster", rootEntryCount, cluster);
		if (!cluster)
			goto invalid;
	}
	if (fileSystem->fileSystemType == 2) {
		first = 2;
	} else {
		first = _root_cluster(fileSystem);
		if (!first)
			first = 2;
	}
	if (cluster < first) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4890, "%s: invalid cluster[%lx]\n",
			"UVFAT_NextCluster", cluster);
		goto invalid;
	}
	if (cluster >= fileSystem->clusterCount + 2)
		goto invalid;
	if (UVFAT_SetGetFAT(fileSystem, 0, cluster, &next)) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 4900, "Cannot get FAT value\n");
		goto invalid;
	}
	return next;
invalid:
	return fileSystem->fileSystemType == 2 ? (SceUInt32)-1 : 0x0fffffff;
}

static inline SceUInt32 _last_cluster_index(SceUInt64 size, SceUInt32 clusterSize)
{
	SCE_DBG_ASSERT_FUNC(size != 0, 187, "_last_cluster_index");
	return (size - 1) / clusterSize;
}

SceUInt32 nextContiguousCluster(SceExfatfsFileSystem *fileSystem,
	SceExfatfsStat *stat, SceUInt32 cluster)
{
	SceUInt64 size;
	SceUInt32 clusterSize;

	if (cluster <= 1)
		return (SceUInt32)-1;
	size = stat->Size;
	if (!size)
		return (SceUInt32)-1;
	clusterSize = fileSystem->clusterSectorCount * fileSystem->sectorSize;
	if (cluster < stat->StartCluster + _last_cluster_index(size, clusterSize))
		return cluster + 1;
	return (SceUInt32)-1;
}

SceUInt32 contiguousClusterAtIndex(SceExfatfsFileSystem *fileSystem,
	SceUInt32 start, SceUInt32 index, SceUInt64 size)
{
	SceUInt32 cluster, clusterSize;

	if ((size == 0) | (start <= 1))
		return (SceUInt32)-1;
	cluster = start + index;
	clusterSize = fileSystem->clusterSectorCount * fileSystem->sectorSize;
	if (start + _last_cluster_index(size, clusterSize) < cluster)
		return (SceUInt32)-1;
	return cluster;
}

SceUInt32 UVFAT_SearchEmptyCluster(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceUInt32 FatEmptyClust)
{
	SceUInt32 end = fileSystem->clusterCount + 2;
	SceUInt32 bitmapNextCluster, nextFreeCluster, first, value;
	int result;
	_Bool wrapped;

	if (fileSystem->fakeFreeClusterCount >= fileSystem->freeClusterCount &&
	    hasCachedFreeClusterCount(fileSystem)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4981,
			"%s: reserved_cluster_count[%#lx] >= freeClusterCount[%#lx]\n",
			"UVFAT_SearchEmptyCluster", fileSystem->fakeFreeClusterCount,
			fileSystem->freeClusterCount);
		goto notFound;
	}
	if (FatEmptyClust) {
		if ((FatEmptyClust <= 1) | (FatEmptyClust >= end))
			FatEmptyClust = 0;
	}
	if (fileSystem->fileSystemType != 2)
		return UVFAT_GetFatEmptyClust(fileSystem, FatEmptyClust);
	bitmapNextCluster = fileSystem->bitmapNextCluster;
	value = 1;
	nextFreeCluster = fileSystem->nextFreeCluster;
	if (FatEmptyClust && FatEmptyClust >= nextFreeCluster) {
		result = UVFAT_allocationbitmapentry(sharedResource, 5, fileSystem,
			FatEmptyClust, &value);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5041,
				"%s: (GET | ALLOC) for cluster[%#lx] failed[%#x]\n",
				"UVFAT_SearchEmptyCluster", FatEmptyClust, result);
		} else if (!value) {
			return FatEmptyClust;
		}
		nextFreeCluster = fileSystem->nextFreeCluster;
	}
	if ((bitmapNextCluster <= 1) | (bitmapNextCluster >= end))
		bitmapNextCluster = nextFreeCluster;
	first = bitmapNextCluster < nextFreeCluster ? nextFreeCluster : bitmapNextCluster;
	wrapped = 0;
	for (FatEmptyClust = first; ; FatEmptyClust++) {
		if ((FatEmptyClust >= end) & !wrapped) {
			FatEmptyClust = fileSystem->nextFreeCluster;
			wrapped = 1;
		}
		if ((FatEmptyClust >= first) & wrapped) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5113, "%s: not found empty\n",
				"UVFAT_SearchEmptyCluster");
			goto notFound;
		}
		result = UVFAT_allocationbitmapentry(sharedResource, 5, fileSystem,
			FatEmptyClust, &value);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 5120,
				"Cannot read allocationbitmap(%d)\n", result);
			SCE_DBG_PRINTF_LEVEL(1, 0, 5122, "%s: (get | alloc) failed[%#x]\n",
				"UVFAT_SearchEmptyCluster", result);
			goto notFound;
		}
		if (!value)
			return FatEmptyClust;
	}
notFound:
	return fileSystem->fileSystemType == 2 ? (SceUInt32)-1 : 0x0fffffff;
}

extern int UVFAT_SetFatChain_ex_NFC(SceExfatfsFileSystem *fileSystem,
	int mode, SceUInt32 startCluster, SceUInt32 lastCluster);

#if FW_VERSION >= 0x04000000
int _assign_first_cluster_of_file(SceExfatfsFile *file,
	SceExfatfsSharedResource *sharedResource);
static inline
#endif
int _append_contiguous_clusters(SceExfatfsFile *file, SceUInt64 size,
	unsigned int flags, SceExfatfsSharedResource *sharedResource,
	unsigned int *contiguousCount)
{
	SceUInt64 position = file->Position;
	SceExfatfsFnode *fnode = file->fnode;
	SceUInt64 end = position + size;
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	unsigned int clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	SceUInt32 oldCluster, firstCluster = (SceUInt32)-2, next, value;
	unsigned int count = 0, clusterIndex, generalSecondaryFlags;
	SceUInt64 lastIndex;
	int result = 0;

	SCE_DBG_PRINTF_LEVEL(2, 0, 1737,
		"%s: Position[%#llx], size[%#llx], clusterCount[%#lx], tailIndex[%#lx], tailCluster[%#lx], currentIndex[%#lx], currentCluster[%#lx]\n",
		"_append_contiguous_clusters", position, size, fnode->clusterCount,
		fnode->tailIndex, fnode->tailCluster, file->clusterIndex, file->Cluster);
#if FW_VERSION >= 0x04000000
	if (position % clusterSize || !position) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1740,
			"%s: unexpected Position[%#llx], clusterSize[%#lx]\n",
			"_append_contiguous_clusters", position, clusterSize);
		position = file->Position;
	} else {
		SceUInt32 cluster;

		SCE_DBG_PRINTF_LEVEL(2, 0, 1752,
			"%s: before _tail_check, fnode[%p], Cluster[%#lx]\n",
			"_append_contiguous_clusters", file->fnode, file->Cluster);
		cluster = file->Cluster;
		value = (SceUInt32)-1;
		fnode = file->fnode;
		if (cluster) {
			SceExfatfsFileSystem *tailFileSystem = fnode->fileSystem;

			if (tailFileSystem->fileSystemType != 2 ||
				!((flags & 2 ? fnode->dirGeneralSecondaryFlags : fnode->Stat.generalSecondaryFlags) & 2)) {
				int error = UVFAT_SetGetFAT(tailFileSystem, 0, cluster, &value);

				if (error) {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 662, "_tail_check",
						"%s: cluster[%#lx], next[%#lx], error(%d)\n", "_tail_check", cluster, value, error);
					SCE_DBG_ASSERT_FUNC(0, 663, "_tail_check");
				} else if (!value) {
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 666, "_tail_check",
						"%s: assigning cluster[%#lx], fnode[%p]\n", "_tail_check", cluster, fnode);
				} else if (_valid_cluster(fnode, value)) {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 668, "_tail_check",
						"%s: unexpected cluster[%#lx]->next[%#lx], fnode[%p]\n", "_tail_check", cluster, value, fnode);
				} else {
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 670, "_tail_check",
						"%s: tail cluster[%#lx], fnode[%p]\n", "_tail_check", cluster, fnode);
				}
			}
		} else if (!(flags & 2)) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 677, "_tail_check",
				"%s: unexpected cluster[%#lx], flags[%#x]\n", "_tail_check", 0, 0);
		} else {
			SceExfatfsBootSector *boot = fnode->fileSystem->PBRBuf;
			const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

			if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8)))
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 680, "_tail_check",
					"%s: unexpected cluster[%#lx], flags[%#x]\n", "_tail_check", 0, flags & 2);
		}
		SCE_DBG_PRINTF_LEVEL(2, 0, 1754,
			"%s: after _tail_check, fnode[%p], Cluster[%#lx]\n",
			"_append_contiguous_clusters", file->fnode, file->Cluster);
		fnode = file->fnode;
		if (!fnode->clusterCount && fnode->chunkCount == 1) {
			SceExfatfsBootSector *boot = fileSystem->PBRBuf;
			const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

			if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8)))
				SCE_DBG_PRINTF_LEVEL(1, 0, 1759,
					"%s: unexpected Position[%#llx], clusterSize[%#lx], clusterCount[%#lx], chunkCount[%#lx]\n",
					"_append_contiguous_clusters", file->Position, clusterSize, fnode->clusterCount, fnode->chunkCount);
		} else if ((SceUInt64)file->Position / clusterSize != fnode->clusterCount) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1762,
				"%s: unexpected Position[%#llx], clusterSize[%#lx], clusterCount[%#lx]\n",
				"_append_contiguous_clusters", file->Position, clusterSize, fnode->clusterCount);
		}
		position = file->Position;
		if ((position - 1) / clusterSize != file->clusterIndex)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1765,
				"%s: unexpected Position[%#llx], clusterSize[%#lx], clusterIndex[%#lx]\n",
				"_append_contiguous_clusters", position, clusterSize, file->clusterIndex);
		position = file->Position;
	}
#else
	if (!(position % clusterSize) && position) {
		SceUInt32 cluster = file->Cluster;

		value = (SceUInt32)-1;
		if (cluster && (fileSystem->fileSystemType != 2 ||
			!((flags & 2 ? fnode->dirGeneralSecondaryFlags :
			fnode->Stat.generalSecondaryFlags) & 2))) {
			UVFAT_SetGetFAT(fileSystem, 0, cluster, &value);
			position = file->Position;
		}
	}
#endif
	if (position >= end)
		goto done;
	oldCluster = file->Cluster;
	if (flags & 1) {
		if (!position) {
#if FW_VERSION >= 0x04000000
			if (oldCluster != (SceUInt32)-2)
				SCE_DBG_PRINTF_LEVEL(1, 0, 1776,
					"%s: unexpected[%#lx] for size 0 file\n", "_append_contiguous_clusters", oldCluster);
			result = _assign_first_cluster_of_file(file, sharedResource);
			if (result < 0)
				return result;
			firstCluster = file->Cluster;
			count = 1;
			fnode = file->fnode;
			clusterIndex = file->clusterIndex;
#else
			firstCluster = UVFAT_SearchEmptyCluster(sharedResource,
				file->fnode->fileSystem, 0);
			fnode = file->fnode;
			if (firstCluster >= fnode->fileSystem->clusterCount + 2)
				return -10;
			if (fnode->fileSystem->fileSystemType == 2) {
				fnode->Stat.generalSecondaryFlags |= 3;
			} else {
				if (fnode->Stat.generalSecondaryFlags) {
					fnode->Stat.generalSecondaryFlags = 0;
					fnode = file->fnode;
				}
				result = UVFAT_SetEOF(fnode->fileSystem, firstCluster);
				if (result < 0)
					return result;
				if (result) {
					count = 1;
					fnode = file->fnode;
					firstCluster = file->Cluster;
					clusterIndex = file->clusterIndex;
					goto haveFirstCluster;
				}
			}
			fnode = file->fnode;
			fnode->Stat.StartCluster = firstCluster;
			if (!fnode->clusterCount) {
				_set_start_cluster(fnode, firstCluster, fnode->Stat.st_type);
				fnode = file->fnode;
			}
			file->clusterIndex = 0;
			file->Cluster = firstCluster;
			count = 1;
			clusterIndex = 0;
#endif
		} else {
			fnode = file->fnode;
			clusterIndex = file->clusterIndex;
		}
#if FW_VERSION < 0x04000000
haveFirstCluster:
#endif
		generalSecondaryFlags = fnode->Stat.generalSecondaryFlags;
	} else {
		if (!position) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1791, "%s: unexpected 0 size directory\n", "_append_contiguous_clusters");
			return -15;
		}
		generalSecondaryFlags = file->fnode->dirGeneralSecondaryFlags;
		if (fileSystem->fileSystemType == 2) {
			if (end >= 0x10000000) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 1802,
					"%s: directory size is too large, Position[%#llx] + entry_size[%#x] >= limit[%#x]\n",
					"_append_contiguous_clusters", position, 32, 0x10000000);
				return -10;
			}
		} else if (!oldCluster) {
			SceExfatfsBootSector *boot = fileSystem->PBRBuf;
			const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

			if (rootEntries[0] | ((unsigned int)rootEntries[1] << 8))
				return -10;
		}
		clusterIndex = file->clusterIndex;
	}
	lastIndex = (end - 1) / clusterSize;
	if (clusterIndex < lastIndex) {
		next = file->Cluster;
		for (;;) {
			next = UVFAT_SearchEmptyCluster(sharedResource, fileSystem, next + 1);
			if (next >= fileSystem->ClusterMAX) {
				result = -10;
				SCE_DBG_PRINTF_LEVEL(1, 0, 1840, "%s: no empty cluster(2)\n", "_append_contiguous_clusters");
				break;
			}
			SCE_DBG_PRINTF_LEVEL(2, 0, 1866,
				"%s: Position[%#llx], offset[%#llx], contCnt(%ld), nfcFlag[%#x], filep->Cluster[%#lx], nextCn[%#lx]\n",
				"_append_contiguous_clusters", file->Position, end, count, generalSecondaryFlags, file->Cluster, next);
			if (next != file->Cluster + 1) {
				if (count) {
					if (fileSystem->fileSystemType == 2) {
						int error = UVFAT_allocationbitmapentry(sharedResource, 2, fileSystem, next, NULL);

						if (error)
							SCE_DBG_PRINTF_LEVEL(1, 0, 1928,
								"%s: BITMAP_FREE failed(%d), nextCn[%#lx]\n", "_append_contiguous_clusters", error, next);
					}
					break;
				}
				if (generalSecondaryFlags & 2) {
					SceUInt32 startCluster = flags & 1 ?
						file->fnode->Stat.StartCluster : file->fnode->DirCluster;

					if (file->Cluster - file->clusterIndex != startCluster)
						SCE_DBG_PRINTF_LEVEL(1, 0, 1886,
							"%s: updateStartCluster[%#lx] != expected[%#lx], clusterIndex[%#lx]\n",
							"_append_contiguous_clusters", startCluster, file->Cluster - file->clusterIndex, file->clusterIndex);
					result = UVFAT_SetEOF(fileSystem, file->Cluster);
					if (result)
						SCE_DBG_PRINTF_LEVEL(1, 0, 1891,
							"%s: UVFAT_SetEOF failed(%d), FAT[%#lx]->[EOF]\n",
							"_append_contiguous_clusters", result, file->Cluster);
					if (!result) {
						SceUInt32 cluster = file->Cluster;

						if (startCluster > cluster || cluster > fileSystem->clusterCount + 2)
							result = -15;
						else
							result = UVFAT_SetFatChain_ex_NFC(fileSystem,
								1, startCluster, cluster);
						if (result)
							SCE_DBG_PRINTF_LEVEL(2, 15, 1898, "%s: Bad Cluster found\n", "_append_contiguous_clusters");
					}
					if (result) {
						int error = UVFAT_allocationbitmapentry(sharedResource, 2, fileSystem, next, NULL);

						if (error)
							SCE_DBG_PRINTF_LEVEL(1, 0, 1906,
								"%s: BITMAP_FREE failed(%d), nextCn[%#lx]\n", "_append_contiguous_clusters", error, next);
						break;
					}
					if (flags & 1)
						file->fnode->Stat.generalSecondaryFlags &= ~2;
					else {
						SCE_DBG_PRINTF_LEVEL(2, 0, 1915,
							"%s: contCnt(%ld), nfcFlag[%#x], dirGeneralSecondaryFlags[%#x], generalSecondaryFlags[%#x]\n",
							"_append_contiguous_clusters", count, 0, file->fnode->dirGeneralSecondaryFlags, file->fnode->Stat.generalSecondaryFlags);
						file->fnode->dirGeneralSecondaryFlags &= ~2;
					}
					generalSecondaryFlags = 0;
				}
			} else if (generalSecondaryFlags & 2) {
				goto appended;
			}
			SCE_DBG_PRINTF_LEVEL(2, 0, 1935,
				"%s: fnode[%p], cluster[%#lx] will be connected to end of chain\n", "_append_contiguous_clusters", file->fnode, next);
			result = UVFAT_SetEOF(fileSystem, next);
			if (result) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 1938,
					"%s: UVFAT_SetEOF failed(%d), FAT[%#lx]->[EOF]\n", "_append_contiguous_clusters", result, next);
				break;
			}
			result = UVFAT_SetGetFAT(fileSystem, 1, file->Cluster, &next);
			if (result) {
				int error;

				value = 0;
				error = UVFAT_SetGetFAT(fileSystem, 1, next, &value);
				SCE_DBG_PRINTF_LEVEL(1, 0, 1946,
					"%s: UVFAT_SetGetFAT failed(%d), FAT[%#lx]->[%#lx]\n", "_append_contiguous_clusters", result, file->Cluster, next);
				if (error)
					SCE_DBG_PRINTF_LEVEL(1, 0, 1949,
						"%s: UVFAT_SetGetFAT failed(%d), FAT[%#lx]->[%#lx]\n", "_append_contiguous_clusters", error, next, value);
				break;
			}
			SCE_DBG_PRINTF_LEVEL(2, 0, 1952,
				"%s: updated cluster chain[%#lx]->[%#lx]\n", "_append_contiguous_clusters", file->Cluster, next);
			SCE_DBG_PRINTF_LEVEL(2, 0, 1958,
				"%s: fnode[%p], cluster[%#lx] has been connected to end of chain\n", "_append_contiguous_clusters", file->fnode, next);
appended:
			if (count)
				count++;
			else {
				firstCluster = next;
				count = 1;
			}
			file->Cluster = next;
			if (++file->clusterIndex >= lastIndex)
				break;
		}
	}
	file->Cluster = oldCluster;
	SCE_DBG_ASSERT_FUNC(file->Position != 0, 1979, "_append_contiguous_clusters");
	SCE_DBG_ASSERT_FUNC(file->Position != 0, 187, "_last_cluster_index");
	file->clusterIndex = ((SceUInt64)file->Position - 1) / clusterSize;
	if (firstCluster != (SceUInt32)-2) {
#if FW_VERSION >= 0x04000000
		if (result)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1989,
				"%s: lastCluster[%#lx], newCluster[%#lx], count(%ld), error(%d)\n", "_append_contiguous_clusters", oldCluster, firstCluster, count, result);
		else
			SCE_DBG_PRINTF_LEVEL(2, 0, 1991,
				"%s: lastCluster[%#lx], newCluster[%#lx], count(%ld)\n", "_append_contiguous_clusters", oldCluster, firstCluster, count);
		if (file->fnode->clusterCount != file->clusterIndex + 1)
			SCE_DBG_PRINTF_LEVEL(1, 0, 2001,
				"%s: Position[%#llx], unexpected clusterCount[%#lx], clusterIndex[%#lx], clusterSize[%#lx], contCnt(%ld)\n",
				"_append_contiguous_clusters", file->Position, file->fnode->clusterCount, file->clusterIndex, clusterSize, count);
		SCE_DBG_PRINTF_LEVEL(2, 0, 2004,
			"%s: before _append_cluster_chunk, fnode[%p], index[%#lx], contCnt[%#lx], start[%#lx]\n",
			"_append_contiguous_clusters", file->fnode, file->clusterIndex + 1, count, firstCluster);
		_append_cluster_chunk(file->fnode, file->clusterIndex + 1, firstCluster, count, flags);
		SCE_DBG_PRINTF_LEVEL(2, 0, 2006,
			"%s: clusterCount[%#lx], Start[%#lx], index[%#lx], contCnt[%#lx]\n", "_append_contiguous_clusters",
			file->fnode->clusterCount, file->Cluster, file->clusterIndex, count);
		if (file->clusterIndex < lastIndex)
			SCE_DBG_PRINTF_LEVEL(2, 0, 2008,
				"%s: fragmented index[%#lx] before Position[%#llx], clusterSize[%#lx]\n", "_append_contiguous_clusters", file->clusterIndex, end, clusterSize);
#else
		if (!file->fnode->clusterCount)
			_set_start_cluster(file->fnode, firstCluster,
				flags & 2 ? 6 : file->fnode->Stat.st_type);
#endif
		clusterIndex = file->clusterIndex + 1;
		if (!(flags & 1)) {
			SceUInt64 directorySize = (SceUInt64)(clusterIndex + count) * clusterSize;

			if (file->fnode->dirSize < directorySize)
				file->fnode->dirSize = directorySize;
		}
		file->Cluster = firstCluster;
		file->clusterIndex = clusterIndex;
#if FW_VERSION >= 0x04000000
		end = (SceUInt64)(clusterIndex + count) * clusterSize;
#endif
	}
done:
	SCE_DBG_PRINTF_LEVEL(2, 0, 2025,
		"%s: error(%d), offset[%#llx], contCnt(%ld), n[%p]\n", "_append_contiguous_clusters", result, end, count, contiguousCount);
	if (contiguousCount)
		*contiguousCount = count;
	return result;
}

#if FW_VERSION >= 0x04000000
int _assign_first_cluster_of_file(SceExfatfsFile *file,
	SceExfatfsSharedResource *sharedResource)
{
	SceUInt32 cluster;
	SceExfatfsFnode *fnode;
	int result;

	cluster = UVFAT_SearchEmptyCluster(sharedResource, file->fnode->fileSystem, 0);
	if (!cluster)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1644, "%s: unexpected cluster[%#lx]\n",
			"_assign_first_cluster_of_file", cluster);
	SCE_DBG_ASSERT(cluster != 0, 1646);
	if (!_valid_cluster(file->fnode, cluster)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1676, "%s: no empty cluster\n",
			"_assign_first_cluster_of_file");
		return -10;
	}
	fnode = file->fnode;
	if (fnode->fileSystem->fileSystemType == 2) {
		fnode->Stat.generalSecondaryFlags |= 3;
	} else {
		if (fnode->Stat.generalSecondaryFlags) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 1656,
				"%s: unexpected flags[%#x], fnode[%p]\n",
				"_assign_first_cluster_of_file", fnode->Stat.generalSecondaryFlags, fnode);
			file->fnode->Stat.generalSecondaryFlags = 0;
		}
		result = UVFAT_SetEOF(file->fnode->fileSystem, cluster);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1661, "%s: UVFAT_SetEOF failed(%d)\n",
				"_assign_first_cluster_of_file", result);
			return result;
		}
		SCE_DBG_PRINTF_LEVEL(2, 0, 1664,
			"%s: fnode[%p], first cluster[%#lx] is assigned\n",
			"_assign_first_cluster_of_file", file->fnode, cluster);
	}
	fnode = file->fnode;
	fnode->Stat.StartCluster = cluster;
	if (fnode->clusterCount)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1668,
			"%s: Position[%#llx], clusterCount[%#lx], clusterIndex[%#lx], clusterSize[%#lx]\n",
			"_assign_first_cluster_of_file", file->Position, fnode->clusterCount, 0,
			fnode->fileSystem->sectorSize * fnode->fileSystem->clusterSectorCount);
	_append_cluster_chunk(file->fnode, 0, cluster, 1, 0x101);
	file->Cluster = cluster;
	file->clusterIndex = 0;
	SCE_DBG_PRINTF_LEVEL(2, 0, 1673,
		"%s: clusterCount[%#lx], Start[%#lx], index[%#lx]\n",
		"_assign_first_cluster_of_file", file->fnode->clusterCount, cluster, 0);
	return 0;
}
#endif

int UVFAT_FreeClusterLink(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceUInt32 cluster)
{
	SceUInt32 next, value = 0;
	int result;

	while (cluster > 1 && cluster < fileSystem->clusterCount + 2) {
		next = UVFAT_NextCluster(sharedResource, cluster, fileSystem);
		result = UVFAT_SetGetFAT(fileSystem, 1, cluster, &value);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 5168, "Cannot set FAT value\n");
			return result;
		}
		if (fileSystem->fileSystemType == 2) {
			result = UVFAT_allocationbitmapentry(sharedResource, 2,
				fileSystem, cluster, NULL);
			if (result) {
				SCE_DBG_PRINTF_LEVEL(2, 15, 5175,
					"Cannot free allocationbitmap(%d)\n", result);
				return result;
			}
		}
		if (next == cluster) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5180,
				"%s: found cluster chain loop cn[%#lx]->next[%#lx]\n",
				"UVFAT_FreeClusterLink", next, next);
			break;
		}
		cluster = next;
	}
	fileSystem->fatDirty = 1;
	if (fileSystem->fileSystemType == 2) {
		result = updateallocbitmaps(fileSystem);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 5194,
				"Cannot free & flush allocationbitmap(%d)\n", result);
			return result;
		}
	}
	return 0;
}

int UVFAT_FreeClusterLink_ex_NFC(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsStat *stat, SceUInt32 cluster)
{
	SceUInt32 start = stat->StartCluster;
	SceUInt32 last, clusterSize;
	SceUInt64 size;
	int result;

	if (!start || !(size = stat->Size)) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 5212, "No need to free cluster\n");
		return 0;
	}
	clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	last = start + _last_cluster_index(size, clusterSize);
	if (last > fileSystem->clusterCount + 2) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 5224, "filesize is over range\n");
		return -15;
	}
	if (start > cluster || cluster > last) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 5229,
			"%s: cluster is not in this file(%ld)[%ld - %ld]\n",
			"UVFAT_FreeClusterLink_ex_NFC", cluster, start, last);
		return -32;
	}
	do {
		result = UVFAT_allocationbitmapentry(sharedResource, 2, fileSystem, cluster++, NULL);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 5237, "%s: Cannot free allocationbitmap(%d)\n",
				"UVFAT_FreeClusterLink_ex_NFC", result);
			return result;
		}
	} while (last >= cluster);
	result = updateallocbitmaps(fileSystem);
	if (result)
		SCE_DBG_PRINTF_LEVEL(2, 15, 5254, "%s: Cannot free & flush allocationbitmap(%d)\n",
			"UVFAT_FreeClusterLink_ex_NFC", result);
	return result;
}

void _flush_fd_cache(SceExfatfsSharedResource *sharedResource, SceExfatfsFnode *fnode)
{
	unsigned int fileCount = getInitializedFileCount(sharedResource);
	unsigned int index = 0;

	if (fnode->refCount <= 1 || !fileCount)
		return;
	do {
		SceExfatfsFile *file = &sharedResource->files[index];
		int result;

		index++;
		if (file->fnode == fnode && (file->cacheFlags & 1)) {
			result = UVFAT_FlushFdBuf(file, fnode->fileSystem);
			if (result)
				SCE_DBG_PRINTF_LEVEL(1, 0, 5498, "%s: UVFAT_FlushFdBuf() Fail[%#x]\n",
					"_flush_fd_cache", result);
		}
	} while (index != fileCount);
}

int _has_parent_directory(SceExfatfsFnode *fnode)
{
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceUInt32 root;

	if (!fileSystem)
		return 0;
	if (!fnode->FilePath[0]) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5745,
			"%s: deleted entry or unused, fnode[%p], StartCluster[%#lx], DirCluster[%#lx], refCount(%hd), fdCount(%hd)\n",
			"_has_parent_directory", fnode, fnode->Stat.StartCluster, fnode->DirCluster,
			fnode->refCount, fnode->fdCount);
		SCE_DBG_ASSERT(fnode->DirCluster == (SceUInt32)-2, 5746);
		return 0;
	}
	if (!fnode->FileName) {
#if FW_DEBUG
		SceInt16 refCount = fnode->refCount;
		SceInt16 fdCount = fnode->fdCount;
		SceUInt32 startCluster = fnode->Stat.StartCluster;
		SceUInt32 dirCluster = fnode->DirCluster;

		root = _root_cluster(fileSystem);
		SCE_DBG_PRINTF_LEVEL(1, 0, 5749,
			"%s: renamed entry, fnode[%p], StartCluster[%#lx], DirCluster[%#lx], refCount(%hd), fdCount(%hd), root_cluster[%#lx]\n",
			"_has_parent_directory", fnode, startCluster, dirCluster,
			refCount, fdCount, root);
#endif
		SCE_DBG_ASSERT(fnode->DirCluster == (SceUInt32)-2, 5750);
		return 0;
	}
	if (fnode->DirCluster == (SceUInt32)-2) {
#if FW_DEBUG
		if (!fnode->Stat.StartCluster) {
			if (!fnode->Stat.st_type) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 5765, "%s: renamed empty file, fnode[%p]\n",
					"_has_parent_directory", fnode);
			} else {
				SceExfatfsBootSector *boot = fileSystem->PBRBuf;
				const unsigned char *rootEntries =
					(const unsigned char *)&boot->fat32.parameters.rootEntryCount;

				if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
					SCE_DBG_PRINTF_LEVEL(1, 0, 5772,
						"%s: DirCluster[%#lx], StartCluster[%#lx] for FAT32 directory, fnode[%p]\n",
						"_has_parent_directory", fnode->DirCluster, fnode->Stat.StartCluster, fnode);
					SCE_DBG_ASSERT(fnode->Stat.StartCluster != 0, 5773);
				}
			}
		} else if (_root_cluster(fileSystem) != fnode->Stat.StartCluster) {
			if (!fnode->FilePath[0])
				SCE_DBG_PRINTF_LEVEL(1, 0, 5780,
					"%s: deleted entry, StartCluster[%#lx], fnode[%p]\n",
					"_has_parent_directory", fnode->Stat.StartCluster, fnode);
			else
				SCE_DBG_PRINTF_LEVEL(1, 0, 5782,
					"%s: renamed entry, StartCluster[%#lx], fnode[%p]\n",
					"_has_parent_directory", fnode->Stat.StartCluster, fnode);
		}
#endif
		return 0;
	}
	if (!fnode->Stat.StartCluster) {
		if (!g_codepageTablesEnabled)
			return fnode->Stat.st_type == 0;
		SCE_DBG_ASSERT(fnode->Stat.st_type == 0, 5791);
		return 1;
	}
	root = _root_cluster(fileSystem);
	if (root != fnode->Stat.StartCluster)
		return 1;
	SCE_DBG_PRINTF_LEVEL(1, 0, 5797,
		"%s: unexpected DirCluster[%#lx] of root directory fnode[%p], _root_cluster[%#lx]\n",
		"_has_parent_directory", fnode->DirCluster, fnode, root);
	SCE_DBG_ASSERT(0, 5798);
	return 0;
}

void touchFnodeModificationTime(SceExfatfsFnode *fnode)
{
	if (fnode) {
		getCurrentFatDateTime(&fnode->Stat.mtime);
		fnode->flags |= 4;
	}
}

int _exfatfs_release_temporal_file(SceExfatfsFileSystem *fileSystem)
{
	if (fileSystem->file.mode != 3) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6728,
			"%s: unexpected mode(%d), assignName(%s)\n",
			"_exfatfs_release_temporal_file", fileSystem->file.mode,
			fileSystem->drive->assignName);
		return -15;
	}
	fileSystem->file.mode = 0;
	return 0;
}

static inline int _exfatfs_reserve_temporal_file_and_fnode(SceExfatfsFileSystem *fileSystem)
{
	if (fileSystem->file.mode) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6704, "_exfatfs_reserve_temporal_file_and_fnode",
			"%s: unexpected mode(%d), assignName(%s)\n",
			"_exfatfs_reserve_temporal_file_and_fnode", fileSystem->file.mode,
			fileSystem->drive->assignName);
		return -15;
	}
	fileSystem->file.mode = 3;
	_invalidate_cluster_cache_data(fileSystem->file.fnode);
	return 0;
}

int UVFAT_InitDirEntry_ex(SceExfatfsSharedResource *sharedResource,
	int fd, SceUInt32 dirCluster, SceExfatfsFileSystem *fileSystem,
	unsigned char flags, SceUInt32 size)
{
	SceExfatfsFile *file;
	SceExfatfsFnode *fnode;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	int result;

	if (fd) {
		file = &sharedResource->files[fd];
	} else {
		result = _exfatfs_reserve_temporal_file_and_fnode(fileSystem);
		if (result)
			return result;
		fnode = fileSystem->file.fnode;
		fnode->Stat.st_type = 6;
		fnode->Stat.StartCluster = dirCluster;
#if FW_DEBUG
		if (dirCluster + 2 <= 1)
			SCE_DBG_PRINTF_LEVEL(1, 0, 6751, "%s: unexpected dirCluster[%#lx]\n",
				"UVFAT_InitDirEntry_ex", dirCluster);
		else if (dirCluster >= fnode->fileSystem->clusterCount + 2)
			SCE_DBG_PRINTF_LEVEL(1, 0, 6753, "%s: dirCluster[%#lx] >= [%#lx]\n",
				"UVFAT_InitDirEntry_ex", dirCluster, fnode->fileSystem->clusterCount + 2);
		SCE_DBG_ASSERT(fileSystem->file.fnode->fileSystem->clusterCount + 2 > dirCluster, 6755);
#endif
		file = &fileSystem->file;
#if FW_VERSION >= 0x04000000
		_set_start_cluster_and_cluster_count(fileSystem->file.fnode, dirCluster, size);
#else
		/* The release initializer inlines this setup, retaining the byte-size helper. */
		_set_start_cluster(fnode, dirCluster, 6);
		if (fnode->clusterCount) {
			SceUInt64 allocationSize = size;
			SceUInt32 clusterSize;

			if (!allocationSize && fnode->Stat.st_type == 6) {
				SceExfatfsFileSystem *directoryFileSystem = fnode->fileSystem;

				if (dirCluster) {
					allocationSize = clusterChainByteSize(fnode, dirCluster);
				} else if (_root_cluster(directoryFileSystem)) {
					allocationSize = directoryFileSystem->sectorSize *
						directoryFileSystem->clusterSectorCount;
				} else {
					boot = directoryFileSystem->PBRBuf;
					rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
					allocationSize = 32 * (rootEntries[0] |
						((unsigned int)rootEntries[1] << 8));
				}
				fnode->Stat.Size = allocationSize;
			}
			clusterSize = fnode->fileSystem->sectorSize * fnode->fileSystem->clusterSectorCount;
			fnode->clusterCount = (allocationSize - 1) / clusterSize + 1;
		}
#endif
	}
	if (!dirCluster) {
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 6766, "%s: unexpected dirCluster[%#lx]\n",
				"UVFAT_InitDirEntry_ex", dirCluster);
			SCE_DBG_ASSERT(dirCluster != 0, 6767);
			dirCluster = _root_cluster(fileSystem);
		}
	}
	resetFilePosition(file);
	fnode = file->fnode;
	fnode->dirGeneralSecondaryFlags = flags;
	fnode->fileSystem = fileSystem;
	fnode->DirCluster = dirCluster;
	file->fnode->dirSize = size;
	file->cacheFlags = 0;
	file->cacheIndex = (SceUInt64)-1;
	if (g_codepageTablesEnabled) {
		file->cacheCluster = (SceUInt32)-2;
		return 0;
	}
	file->cacheCluster = 0;
	return 0;
}

static inline int UVFAT_WriteStatDir_fat(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsStat *stat)
{
	SceExfatfsFatDirectoryEntry entry;
	int result;

#if FW_DEBUG
	result = readTemporalDirectoryEntry(sharedResource, &entry, 0, fileSystem);
#else
	if (!fileSystem->file.fnode->DirCluster &&
		!((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat32.parameters.rootEntryCount)
		fileSystem->file.fnode->DirCluster = _root_cluster(fileSystem);
	result = readDirectoryEntryBytes(sharedResource, fileSystem,
		&fileSystem->file, &entry);
	if (result > 0)
		result = 0;
	else if (!result)
		result = -7;
#endif
	if (result) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 4424, "UVFAT_WriteStatDir_fat",
			"Cannot read direntry\n");
		return result;
	}
	if (!entry.name[0]) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 4429, "UVFAT_WriteStatDir_fat", "slot empty\n");
		return -7;
	}
	if (entry.attribute == 15 || (entry.attribute & 8))
		return -32;
	if ((entry.name[0] <= 0x20 && entry.name[0] != 5) || entry.name[0] == 0xe5) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 4440, "UVFAT_WriteStatDir_fat",
			"Not start point of direntry set\n");
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 4451, "UVFAT_WriteStatDir_fat",
			"scode=0x%x, clust=%ld, offset=%lld, startClust=%ld\n",
			entry.name[0], fileSystem->file.Cluster, fileSystem->file.Position,
			fileSystem->file.fnode->DirCluster);
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4454, "UVFAT_WriteStatDir_fat",
			"%s: fd(%d), dirEntry.FileName[0]:[%#x], please report to Bug 36531\n",
			"UVFAT_WriteStatDir_fat", 0, entry.name[0]);
		return -32;
	}
	if ((SceUInt64)fileSystem->file.Position >= 32)
		fileSystem->file.Position -= 32;
	entry.creationDate = UVFAT_TimeToFATDate(&stat->ctime);
	entry.creationTime = fatDateTimeToTime(&stat->ctime);
	entry.accessDate = UVFAT_TimeToFATDate(&stat->atime);
	entry.modificationDate = UVFAT_TimeToFATDate(&stat->mtime);
	entry.modificationTime = fatDateTimeToTime(&stat->mtime);
	entry.firstClusterHigh = 0;
	entry.size = stat->Size;
	entry.firstClusterLow = stat->StartCluster;
	if (fileSystem->FATBits == 32)
		entry.firstClusterHigh = stat->StartCluster >> 16;
	copyStatAttributeToFatEntry(stat, &entry, fileSystem);
#if FW_DEBUG
	result = UVFAT_WriteDirEntry_ex(sharedResource, 0, &entry, 0, 0, fileSystem);
	if (result)
		return result;
#else
	result = writeDirectoryEntryBytes(sharedResource, fileSystem,
		&fileSystem->file, &entry, 0);
	if (result < 0)
		return result;
#endif
	return UVFAT_FlushFdBuf(&fileSystem->file, fileSystem);
}

int _exfatfs_flush_metadata(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *fnode)
{
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	int result;

	if (!fileSystem || !_has_parent_directory(fnode))
		return 0;
#if FW_DEBUG
	if (!fnode->DirCluster && !((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat32.parameters.rootEntryCount) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5879, "%s: unexpected DirCluster[%#lx]\n",
			"_exfatfs_flush_metadata", fnode->DirCluster);
		SCE_DBG_ASSERT(fnode->DirCluster != 0, 5880);
	}
	if (fnode->DirCluster == (SceUInt32)-2)
		SCE_DBG_PRINTF_LEVEL(1, 0, 5893,
			"%s: unexpected DirCluster[%#lx], fnode[%p]\n",
			"_exfatfs_flush_metadata", fnode->DirCluster, fnode);
#endif
	if (!(fnode->flags & 4))
		return 0;
	if (fileSystem->fileSystemType == 2) {
		if (!fnode->DirCluster || !fnode->DirOffClust)
			SCE_DBG_PRINTF_LEVEL(1, 0, 5907,
				"%s: exFAT fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], dirSize[%#x]\n",
				"_exfatfs_flush_metadata", fnode, fnode->DirCluster,
				fnode->DirOffset, fnode->DirOffClust, fnode->dirSize);
		result = UVFAT_InitDirEntry_ex(sharedResource, 0, fnode->DirCluster,
			fileSystem, fnode->dirGeneralSecondaryFlags, fnode->dirSize);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5912, "%s: UVFAT_InitDirEntry_ex failed(%d)\n",
				"_exfatfs_flush_metadata", result);
			return result;
		}
		fileSystem->file.Position = fnode->DirOffset;
		fileSystem->file.Cluster = fnode->DirOffClust;
		if (g_codepageTablesEnabled ? fnode->DirOffset == 0xfffffffeULL : !fnode->DirOffset) {
			fileSystem->file.clusterIndex = 0x0fffffff;
			if (g_codepageTablesEnabled)
				fileSystem->file.Cluster = fnode->DirCluster;
		} else {
			fileSystem->file.clusterIndex = (SceUInt64)fnode->DirOffset / fileSystemClusterSize(fileSystem);
		}
		result = UVFAT_WriteStatDir_ex(sharedResource, fileSystem, &fnode->Stat);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5936,
				"%s: set stat failed(%d), exFAT, DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], StartCluster[%#lx]\n",
				"_exfatfs_flush_metadata", result, fnode->DirCluster,
				fnode->DirOffset, fnode->DirOffClust, fnode->Stat.StartCluster);
			_print_fnode(fnode, "_exfatfs_flush_metadata");
		}
	} else {
#if FW_DEBUG
		if (!fnode->DirCluster && !fnode->DirOffClust)
			SCE_DBG_PRINTF_LEVEL(2, 0, 5943,
				"%s: FAT fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], dirSize[%#x]\n",
				"_exfatfs_flush_metadata", fnode, fnode->DirCluster,
				fnode->DirOffset, fnode->DirOffClust, fnode->dirSize);
		else if (!fnode->DirCluster || !fnode->DirOffClust)
			SCE_DBG_PRINTF_LEVEL(1, 0, 5945,
				"%s: FAT fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], dirSize[%#x]\n",
				"_exfatfs_flush_metadata", fnode, fnode->DirCluster,
				fnode->DirOffset, fnode->DirOffClust, fnode->dirSize);
#endif
		result = UVFAT_InitDirEntry_ex(sharedResource, 0, fnode->DirCluster, fileSystem, 0, 0);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5950, "%s: UVFAT_InitDirEntry_ex failed(%d)\n",
				"_exfatfs_flush_metadata", result);
			return result;
		}
		fileSystem->file.Position = fnode->DirOffset;
		fileSystem->file.Cluster = fnode->DirOffClust;
		if (g_codepageTablesEnabled ? fnode->DirOffset == 0xfffffffeULL : !fnode->DirOffset) {
			fileSystem->file.clusterIndex = 0x0fffffff;
			if (g_codepageTablesEnabled)
				fileSystem->file.Cluster = fnode->DirCluster;
		} else if (fnode->DirCluster) {
			fileSystem->file.clusterIndex = (SceUInt64)fnode->DirOffset / fileSystemClusterSize(fileSystem);
		} else {
			fileSystem->file.clusterIndex = 0;
		}
		result = UVFAT_WriteStatDir_fat(sharedResource, fileSystem, &fnode->Stat);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5989,
				"%s: set stat failed(%d), FAT, DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], StartCluster[%#lx]\n",
				"_exfatfs_flush_metadata", result, fnode->DirCluster,
				fnode->DirOffset, fnode->DirOffClust, fnode->Stat.StartCluster);
			_print_fnode(fnode, "_exfatfs_flush_metadata");
		}
	}
	_exfatfs_release_temporal_file(fileSystem);
	return result;
}

#define READ_DATA_TRACE(level, line, action) \
	SCE_DBG_PRINTF_LEVEL_FUNC(level, 0, line, "UVFAT_ReadData", \
		"%s: " action ", buf[%p], size[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n", \
		"UVFAT_ReadData", buffer, size, file, file->Position, \
		file->Cluster, file->clusterIndex, file->cacheIndex)

#define READ_DATA_CACHE_TRACE(line, action) \
	SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, line, "UVFAT_ReadData", \
		"%s: " action ", buf[%p], size[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx], readCluster[%#lx]\n", \
		"UVFAT_ReadData", buffer, size, file, file->Position, \
		file->Cluster, file->clusterIndex, file->cacheIndex, readCluster)

int UVFAT_ReadData(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFile *file,
	void *buffer, unsigned int size, unsigned char flags)
{
	unsigned int clusterSize = fileSystemClusterSize(fileSystem);
	unsigned int blockSize = file->fnode->fileSystem->fileBufferSize;
	int transferred = 0;
	char *cachedBuffer;
	SceUInt64 position, offset, cacheStart, readSize, blockOffset;
	SceUInt32 readCluster, block;
	unsigned int available, copySize, clusters;
	int result;

	while (size) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 6190,
			"%s: buf[%p], size[%#lx], filep[%p], clusterSize[%#lx], blockSize[%#lx], Position[%#llx], cacheIndex[%#llx]\n",
			"UVFAT_ReadData", buffer, size, file, clusterSize, blockSize,
			file->Position, file->cacheIndex);
		cachedBuffer = file->Buf;
		position = file->Position;
		if ((flags & 1) && position >= file->fnode->Stat.Size)
			return transferred;
		offset = position % blockSize;
		if (file->cacheIndex != (SceUInt64)-1) {
			available = file->fnode->fileSystem->fileBufferSize;
			cacheStart = file->cacheIndex * available;
			if (position < cacheStart + available && cacheStart < position + 1) {
				if ((unsigned int)(position + 1 - cacheStart) < available)
					available = position + 1 - cacheStart;
				if (available)
					goto copyFromCache;
			}
		}
		if (file->cacheFlags & 1) {
			result = UVFAT_FlushFdBuf(file, file->fnode->fileSystem);
			if (result) {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5498, "_flush_fd_cache",
					"%s: UVFAT_FlushFdBuf() Fail[%#x]\n", "_flush_fd_cache", result);
				SCE_DBG_PRINTF_LEVEL(1, 0, 6213, "%s: _flush_fd_cache() failed[%#x]\n",
					"UVFAT_ReadData", result);
			}
			if (flags & 1) {
				SceExfatfsFnode *fnode = file->fnode;

				if (fnode->fileSystem) {
					flushFileSystemMetadata(fnode->fileSystem);
					if (_has_parent_directory(fnode)) {
						getCurrentFatDateTime(&fnode->Stat.mtime);
						fnode->flags |= 4;
						result = _exfatfs_flush_metadata(sharedResource, fnode);
						if (result) {
							SCE_DBG_PRINTF_LEVEL(1, 0, 6220,
								"%s: _update_metadata() failed[%#x]\n", "UVFAT_ReadData", result);
							return result;
						}
						fnode->flags &= ~4;
					}
				}
			}
		}
		if (!file->Cluster && file->clusterIndex)
			SCE_DBG_PRINTF_LEVEL(1, 0, 6226,
				"%s: before _set_position_and_cluster_index, filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
				"UVFAT_ReadData", file, file->Position, file->Cluster,
				file->clusterIndex, file->cacheIndex);
		else
			SCE_DBG_PRINTF_LEVEL(2, 0, 6228,
				"%s: before _set_position_and_cluster_index, filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
				"UVFAT_ReadData", file, file->Position, file->Cluster,
				file->clusterIndex, file->cacheIndex);
		if (_set_position_and_cluster_index(file, file->Position, flags) == -7)
			return transferred;
		if (flags & 1)
			SCE_DBG_PRINTF_LEVEL(2, 0, 6236,
				"%s: buf[%p], size[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
				"UVFAT_ReadData", buffer, size, file, file->Position,
				file->Cluster, file->clusterIndex, file->cacheIndex);
		else
			SCE_DBG_PRINTF_LEVEL(2, 0, 6238,
				"%s: buf[%p], size[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
				"UVFAT_ReadData", buffer, size, file, file->Position,
				file->Cluster, file->clusterIndex, file->cacheIndex);
		readCluster = file->Cluster;
		position = file->Position;
		block = (readCluster ? position % clusterSize : position) / blockSize;
		if ((flags & 1) && ((size >= blockSize) & (offset == 0)) &&
			file->fnode->Stat.Size - position >= size) {
			clusters = _contiguous_cluster_count(file, size, clusterSize);
			blockOffset = blockSize * block;
			if (readCluster) {
				readSize = (SceUInt64)clusters * clusterSize - blockOffset;
			} else {
				SceExfatfsBootSector *boot = fileSystem->PBRBuf;
				const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

				readSize = (SceUInt64)(32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) - blockOffset;
			}
			while (size < readSize)
				readSize -= blockSize;
			if (readSize) {
				SceUInt64 sector = clusterToDeviceSector(readCluster, fileSystem) +
					(unsigned int)(blockSize * block / fileSystem->sectorSize);

				if ((file->cacheFlags & 1) && file->cacheIndex != (SceUInt64)-1) {
					available = file->fnode->fileSystem->fileBufferSize;
					cacheStart = file->cacheIndex * available;
					if ((SceUInt64)file->Position < cacheStart + available &&
						cacheStart < file->Position + readSize) {
						copySize = file->Position - cacheStart + readSize;
						if (copySize < available)
							available = copySize;
						if (available) {
							READ_DATA_TRACE(2, 6281, "before _flush_fd_cache");
							if (file->cacheFlags & 1) {
								result = UVFAT_FlushFdBuf(file, file->fnode->fileSystem);
								if (result) {
									SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5498, "_flush_fd_cache",
										"%s: UVFAT_FlushFdBuf() Fail[%#x]\n", "_flush_fd_cache", result);
									SCE_DBG_PRINTF_LEVEL(1, 0, 6284, "%s: _flush_fd_cache() failed[%#x]\n", "UVFAT_ReadData", result);
									return result;
								}
							}
						}
					}
				}
				READ_DATA_TRACE(2, 6288, "before UVFAT_ReadDevice");
				result = UVFAT_ReadDevice(fileSystem, sector, readSize, buffer);
				if (result) {
					SCE_DBG_PRINTF_LEVEL(2, 0, 6295,
						"%s: UVFAT_ReadDevice failed(%d), buf[%p], size[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
						"UVFAT_ReadData", result, buffer, size, file, file->Position,
						file->Cluster, file->clusterIndex, file->cacheIndex);
					return result;
				}
				READ_DATA_TRACE(2, 6297, "after UVFAT_ReadDevice");
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0, 6302, "%s: shouldn't reach here\n", "UVFAT_ReadData");
				if ((file->cacheFlags & 1) && file->cacheIndex != (SceUInt64)-1) {
					available = file->fnode->fileSystem->fileBufferSize;
					cacheStart = file->cacheIndex * available;
					if ((SceUInt64)file->Position < cacheStart + available &&
						cacheStart < blockSize + (SceUInt64)file->Position) {
						copySize = blockSize + file->Position - cacheStart;
						if (copySize < available)
							available = copySize;
						if (available) {
							result = UVFAT_FlushFdBuf(file, file->fnode->fileSystem);
							if (result) {
								SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5498, "_flush_fd_cache",
									"%s: UVFAT_FlushFdBuf() Fail[%#x]\n", "_flush_fd_cache", result);
								SCE_DBG_PRINTF_LEVEL(1, 0, 6306, "%s: _flush_fd_cache() failed[%#x]\n", "UVFAT_ReadData", result);
								return result;
							}
						}
					}
				}
				READ_DATA_TRACE(2, 6310, "before UVFAT_ReadCblock");
				result = UVFAT_ReadCblock(readCluster, block, fileSystem, buffer);
				if (result) {
					SCE_DBG_PRINTF_LEVEL(1, 0, 6314,
						"%s: UVFAT_ReadCblock failed(%d), buf[%p], size[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
						"UVFAT_ReadData", result, buffer, size, file, file->Position,
						file->Cluster, file->clusterIndex, file->cacheIndex);
					return result;
				}
				READ_DATA_TRACE(2, 6316, "after UVFAT_ReadCblock");
				readSize = blockSize;
			}
			size -= readSize;
			transferred += readSize;
			file->Position += readSize;
			clusters = _last_cluster_index(blockOffset + readSize, clusterSize);
			file->clusterIndex += clusters;
			file->Cluster += clusters;
			buffer = (char *)buffer + readSize;
			continue;
		}
		if (flags & 1) {
			READ_DATA_CACHE_TRACE(6344, "before _read_into_fd_cache");
			result = _read_into_fd_cache(file, readCluster);
			if (result)
				return result;
			READ_DATA_CACHE_TRACE(6354, "after _read_into_fd_cache");
			SCE_DBG_PRINTF_LEVEL(2, 0, 6355, "%s: %#x %#x %#x %#x\n", "UVFAT_ReadData",
				((unsigned char *)file->Buf)[0], ((unsigned char *)file->Buf)[1],
				((unsigned char *)file->Buf)[2], ((unsigned char *)file->Buf)[3]);
		} else {
			READ_DATA_CACHE_TRACE(6346, "before _read_into_fd_cache");
			result = _read_into_fd_cache(file, readCluster);
			if (result)
				return result;
		}
		if (cachedBuffer == buffer) {
			file->Position += blockSize;
			size -= blockSize;
			transferred += blockSize;
			buffer = (char *)buffer + blockSize;
			continue;
		}
copyFromCache:
		copySize = (SceUInt64)blockSize - offset;
		if (size < (SceUInt64)blockSize - offset)
			copySize = size;
		if (flags & 1) {
			if (file->fnode->Stat.Size < (SceUInt64)file->Position + copySize)
				copySize = file->fnode->Stat.Size - file->Position;
			SCE_DBG_PRINTF_LEVEL(2, 0, 6378,
				"%s: before _read_from_fd_cache, buf[%p], size[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx], offset[%#llx]\n",
				"UVFAT_ReadData", buffer, size, file, file->Position, file->Cluster,
				file->clusterIndex, file->cacheIndex, offset);
			SCE_DBG_PRINTF_LEVEL(2, 0, 6379, "%s: %#x %#x %#x %#x\n", "UVFAT_ReadData",
				((unsigned char *)file->Buf)[0], ((unsigned char *)file->Buf)[1],
				((unsigned char *)file->Buf)[2], ((unsigned char *)file->Buf)[3]);
		} else {
			READ_DATA_TRACE(2, 6381, "before _read_from_fd_cache");
		}
		if (copySize) {
			void *destination = buffer;

			transferred += copySize;
			buffer = (char *)buffer + copySize;
			memcpy(destination, (char *)file->Buf + offset, copySize);
			file->Position += copySize;
			size -= copySize;
		}
	}
	return transferred;
}

int readDirectoryEntryBytes(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFile *file, void *buffer)
{
	unsigned int clusterSize = fileSystemClusterSize(fileSystem);
	unsigned int blockSize = file->fnode->fileSystem->fileBufferSize;
	unsigned int size = 32;
	int transferred = 0, result;
	SceUInt64 position, offset, cacheStart, copySize;
	unsigned int available;
	SceUInt32 readCluster;
	char *cachedBuffer;

	(void)sharedResource;
	while (size) {
		cachedBuffer = file->Buf;
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 6190, "UVFAT_ReadData",
			"%s: buf[%p], size[%#lx], filep[%p], clusterSize[%#lx], blockSize[%#lx], Position[%#llx], cacheIndex[%#llx]\n",
			"UVFAT_ReadData", buffer, size, file, clusterSize, blockSize,
			file->Position, file->cacheIndex);
		position = file->Position;
		offset = position % blockSize;
		if (file->cacheIndex != (SceUInt64)-1) {
			available = file->fnode->fileSystem->fileBufferSize;
			cacheStart = file->cacheIndex * available;
			if (position < cacheStart + available && cacheStart < position + 1) {
				if ((unsigned int)(position + 1 - cacheStart) < available)
					available = position + 1 - cacheStart;
				if (available)
					goto copyEntryBytes;
			}
		}
		if (file->cacheFlags & 1) {
			result = UVFAT_FlushFdBuf(file, file->fnode->fileSystem);
			if (result) {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5498, "_flush_fd_cache",
					"%s: UVFAT_FlushFdBuf() Fail[%#x]\n", "_flush_fd_cache", result);
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6213, "UVFAT_ReadData",
					"%s: _flush_fd_cache() failed[%#x]\n", "UVFAT_ReadData", result);
			}
		}
		if (!file->Cluster && file->clusterIndex)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6226, "UVFAT_ReadData",
				"%s: before _set_position_and_cluster_index, filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
				"UVFAT_ReadData", file, file->Position, file->Cluster,
				file->clusterIndex, file->cacheIndex);
		else
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 6228, "UVFAT_ReadData",
				"%s: before _set_position_and_cluster_index, filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
				"UVFAT_ReadData", file, file->Position, file->Cluster,
				file->clusterIndex, file->cacheIndex);
#if FW_DEBUG
		result = _set_position_and_cluster_index(file, file->Position, 2);
#else
		result = setDirectoryPosition(file, file->Position);
#endif
		if (result == -7)
			return transferred;
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 6238, "UVFAT_ReadData",
			"%s: buf[%p], size[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
			"UVFAT_ReadData", buffer, size, file, file->Position,
			file->Cluster, file->clusterIndex, file->cacheIndex);
		readCluster = file->Cluster;
		READ_DATA_CACHE_TRACE(6346, "before _read_into_fd_cache");
		result = _read_into_fd_cache(file, readCluster);
		if (result)
			return result;
		if (cachedBuffer == buffer) {
			file->Position += blockSize;
			size -= blockSize;
			transferred += blockSize;
			buffer = (char *)buffer + blockSize;
			continue;
		}
copyEntryBytes:
#if FW_DEBUG
		copySize = size;
		if ((SceUInt64)blockSize - offset < size)
			copySize = (SceUInt64)blockSize - offset;
#else
		copySize = (SceUInt64)blockSize - offset;
		if (size < copySize)
			copySize = size;
#endif
		READ_DATA_TRACE(2, 6381, "before _read_from_fd_cache");
		if ((unsigned int)copySize) {
#if !FW_DEBUG
			void *destination = buffer;

			buffer = (char *)buffer + copySize;
#endif
			size -= copySize;
			transferred += copySize;
#if FW_DEBUG
			memcpy(buffer, (char *)file->Buf + offset, copySize);
			buffer = (char *)buffer + copySize;
#else
			memcpy(destination, (char *)file->Buf + offset, copySize);
#endif
			file->Position += copySize;
		}
	}
	return transferred;
}

#undef READ_DATA_TRACE
#undef READ_DATA_CACHE_TRACE

int readSingleDirectoryEntry(SceExfatfsSharedResource *sharedResource,
	int fd, void *entry, SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsFile *file;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	int result;

	if (!fd) {
		file = &fileSystem->file;
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		if (!file->fnode->DirCluster && !(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6870, "UVFAT_ReadDirEntry",
				"%s: unexpected StartCluster[%#lx]\n", "UVFAT_ReadDirEntry", file->fnode->Stat.StartCluster);
			file->fnode->DirCluster = _root_cluster(fileSystem);
		}
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6881, "UVFAT_ReadDirEntry",
			"%s: unexpected fd(%d)\n", "UVFAT_ReadDirEntry", fd);
		file = &sharedResource->files[fd];
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		if (!file->fnode->Stat.StartCluster && !(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6887, "UVFAT_ReadDirEntry",
				"%s: unexpected StartCluster[%#lx]\n", "UVFAT_ReadDirEntry", 0);
			file->fnode->Stat.StartCluster = _root_cluster(fileSystem);
		}
	}
	if (!entry) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6908, "UVFAT_ReadDirEntry", "%s: NULL buffer\n", "UVFAT_ReadDirEntry");
		return -32;
	}
	SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 6919, "UVFAT_ReadDirEntry",
		"%s: fd(%d), fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], StartCluster[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], _clusterIndex[%#lx]\n",
		"UVFAT_ReadDirEntry", fd, file->fnode, file->fnode->DirCluster,
		file->fnode->DirOffset, file->fnode->DirOffClust, file->fnode->Stat.StartCluster,
		file, file->Position, file->Cluster, file->clusterIndex);
	result = readDirectoryEntryBytes(sharedResource, fileSystem, file, entry);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 6923, "UVFAT_ReadDirEntry", "UVFAT_ReadData() Fail, er=%d\n", result);
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6924, "UVFAT_ReadDirEntry",
			"%s: UVFAT_ReadData failed[%#lx]\n", "UVFAT_ReadDirEntry", result);
		return result;
	}
	return result ? 0 : -7;
}

int _exfatfs_search_next_short_name(SceExfatfsSharedResource *sharedResource,
	int fd, SceExfatfsFileSystem *fileSystem, SceExfatfsFatDirectoryEntry *entry)
{
	int result;
	unsigned int firstCharacter, secondCharacter;
	char spaces[10];

	for (;;) {
		result = readSingleDirectoryEntry(sharedResource, fd, entry, fileSystem);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2240, "%s: UVFAT_ReadDirEntry failed(%d)\n",
				"_exfatfs_search_next_short_name", result);
			return result;
		}
		firstCharacter = entry->name[0];
		if (!firstCharacter)
			return -7;
		if (firstCharacter == 0xe5 || entry->attribute == 15 || (entry->attribute & 8))
			continue;
		if (firstCharacter <= 0x20 && firstCharacter != 5) {
			_exfatfs_notify_invalid_directory_entry(fileSystem->drive, entry);
			continue;
		}
		if (firstCharacter != '.')
			return 0;
		secondCharacter = entry->name[1];
		__builtin_memcpy(spaces, "         ", sizeof(spaces));
		if (secondCharacter != ' ' && secondCharacter != '.')
			return 0;
		if (memcmp(entry->name + 2, spaces, 9))
			return 0;
	}
}

int _ReadDir_fat(SceExfatfsSharedResource *sharedResource, SceExfatfsFile *file,
	SceUInt16 *longName, SceExfatfsStat *stat, unsigned char *shortName, int fd)
{
	SceExfatfsFatDirectoryRecord record;
	SceExfatfsFatDirectoryEntry *entry = &record.shortEntry;
	SceExfatfsFatLongDirectoryEntry *longEntry = &record.longEntry;
	SceUInt16 nameFragments[20][14];
	char spaces[10];
	int fragmentCount = 0, checksum = 0, result, index;
	unsigned int firstCharacter, sum;
	unsigned char *destination;
	SceExfatfsFnode *fnode;
	SceExfatfsFileSystem *fileSystem;
	SceUInt32 cluster;

	for (;;) {
		result = readSingleDirectoryEntry(sharedResource, fd, &record, file->fnode->fileSystem);
		if (result) {
			if (result != -7)
				SCE_DBG_PRINTF_LEVEL(1, 0, 2321, "%s: UVFAT_ReadDirEntry failed[%#x]\n", "_ReadDir_fat", result);
			return result;
		}
		firstCharacter = entry->name[0];
		if (!firstCharacter)
			return -7;
		if (firstCharacter == 0xe5) {
			fragmentCount = 0;
			continue;
		}
		if (entry->attribute == 15) {
			SceUInt16 *fragment;

			if (longEntry->sequence & 0x40) {
				fragmentCount = longEntry->sequence & 0x1f;
				checksum = longEntry->checksum;
			} else if (!fragmentCount) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 2343, "%s: skip LFN entry without lastEntry\n", "_ReadDir_fat");
				continue;
			} else if (longEntry->checksum != checksum) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 2347, "%s: skip sum[%#x] != sum of lastEntry[%#x]\n",
					"_ReadDir_fat", longEntry->checksum, checksum);
				fragmentCount = longEntry->sequence & 0x40;
				continue;
			}
			fragment = nameFragments[(longEntry->sequence & 0x1f) - 1];
#define LFN_CHARACTER(pair) ((pair)[0] | ((unsigned int)(pair)[1] << 8))
			fragment[0] = LFN_CHARACTER(longEntry->namePrefix[0]);
			fragment[1] = LFN_CHARACTER(longEntry->namePrefix[1]);
			fragment[2] = LFN_CHARACTER(longEntry->namePrefix[2]);
			fragment[3] = LFN_CHARACTER(longEntry->namePrefix[3]);
			fragment[4] = LFN_CHARACTER(longEntry->namePrefix[4]);
			fragment[5] = LFN_CHARACTER(longEntry->nameMiddle[0]);
			fragment[6] = LFN_CHARACTER(longEntry->nameMiddle[1]);
			fragment[7] = LFN_CHARACTER(longEntry->nameMiddle[2]);
			fragment[8] = LFN_CHARACTER(longEntry->nameMiddle[3]);
			fragment[9] = LFN_CHARACTER(longEntry->nameMiddle[4]);
			fragment[10] = LFN_CHARACTER(longEntry->nameMiddle[5]);
#undef LFN_CHARACTER
			fragment[11] = longEntry->nameSuffix[0][0] | ((unsigned int)longEntry->nameSuffix[0][1] << 8);
			fragment[12] = longEntry->nameSuffix[1][0] | ((unsigned int)longEntry->nameSuffix[1][1] << 8);
			fragment[13] = 0;
			continue;
		}
		if (entry->attribute & 8) {
			fragmentCount = 0;
			continue;
		}
		if (firstCharacter <= 0x20 && firstCharacter != 5) {
			fragmentCount = entry->attribute & 8;
			_exfatfs_notify_invalid_directory_entry(file->fnode->fileSystem->drive, entry);
			continue;
		}
		if (fragmentCount) {
			sum = firstCharacter;
#define LFN_CHECKSUM(character) (unsigned char)(((sum >> 1) | (sum << 7)) + (character))
			sum = LFN_CHECKSUM(entry->name[1]);
			sum = LFN_CHECKSUM(entry->name[2]);
			sum = LFN_CHECKSUM(entry->name[3]);
			sum = LFN_CHECKSUM(entry->name[4]);
			sum = LFN_CHECKSUM(entry->name[5]);
			sum = LFN_CHECKSUM(entry->name[6]);
			sum = LFN_CHECKSUM(entry->name[7]);
			sum = LFN_CHECKSUM(entry->extension[0]);
			sum = LFN_CHECKSUM(entry->extension[1]);
			sum = LFN_CHECKSUM(entry->extension[2]);
#undef LFN_CHECKSUM
			if (checksum != sum) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 2387, "%s: short entry, sum[%#x] != sum of LFN[%#x]\n",
					"_ReadDir_fat", sum, checksum);
				fragmentCount = 0;
			}
		}
		if (firstCharacter != '.')
			break;
		__builtin_memcpy(spaces, "         ", sizeof(spaces));
		if (entry->name[1] != ' ' && entry->name[1] != '.')
			break;
		if (memcmp(entry->name + 2, spaces, 9))
			break;
	}
	*longName = 0;
	destination = shortName;
	if (entry->name[0] != ' ')
		*destination++ = entry->name[0];
	if (entry->name[1] != ' ')
		*destination++ = entry->name[1];
	if (entry->name[2] != ' ')
		*destination++ = entry->name[2];
	if (entry->name[3] != ' ')
		*destination++ = entry->name[3];
	if (entry->name[4] != ' ')
		*destination++ = entry->name[4];
	if (entry->name[5] != ' ')
		*destination++ = entry->name[5];
	if (entry->name[6] != ' ')
		*destination++ = entry->name[6];
	if (entry->name[7] != ' ')
		*destination++ = entry->name[7];
	if (entry->extension[0] != ' ') {
		*destination++ = '.';
		if (entry->extension[0] != ' ')
			*destination++ = entry->extension[0];
	}
	if (entry->extension[1] != ' ')
		*destination++ = entry->extension[1];
	if (entry->extension[2] != ' ')
		*destination++ = entry->extension[2];
	*destination = 0;
	if (*shortName == 5)
		*shortName = 0xe5;
	for (index = 0; index < fragmentCount; index++)
		fatNameAppendBounded(longName, nameFragments[index], 256);
	copyFatEntryAttributeToStat(entry, stat, file->fnode->fileSystem);
	decodeFatDateTime(&stat->ctime, entry->creationDate, entry->creationTime);
	decodeFatDateTime(&stat->atime, entry->accessDate, 0);
	decodeFatDateTime(&stat->mtime, entry->modificationDate, entry->modificationTime);
	fnode = file->fnode;
	fileSystem = fnode->fileSystem;
	stat->Size = entry->size;
	cluster = entry->firstClusterLow;
	stat->StartCluster = cluster;
	if (fileSystem->FATBits == 32) {
		cluster |= (SceUInt32)entry->firstClusterHigh << 16;
		stat->StartCluster = cluster;
	}
	if (stat->st_type == 6) {
		if (cluster) {
			unsigned int clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;

			stat->Size = (SceUInt64)_cluster_chain_length(fnode, cluster) * clusterSize;
		} else if (_root_cluster(fileSystem)) {
			stat->Size = fileSystem->sectorSize * fileSystem->clusterSectorCount;
		} else {
			SceExfatfsBootSector *boot = fileSystem->PBRBuf;
			const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

			stat->Size = 32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
		}
	}
	return result;
}

int UVFAT_SearchGetFile_ex(int mode, SceExfatfsSharedResource *sharedResource,
	SceUInt16 *filename, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, int fd)
{
	SceExfatfsDirectoryRecord primary, stream, secondary;
	SceExfatfsFile *file;
	SceUInt16 *name;
	SceUInt32 directoryCluster;
	unsigned int nameHash, checksum, secondaryCount, nameEntryCount, entryType;
	int result, nameIndex, entryIndex, vendorCount;
	int search = mode == 0;

	if (fd && search) {
#if FW_DEBUG
		SCE_DBG_PRINTF_LEVEL(2, 15, 3100, "%s: Illegal args\n", "UVFAT_SearchGetFile_ex");
#endif
		return -32;
	}
	if (search && (unsigned int)fatNameLength(filename) > 256) {
#if FW_DEBUG
		SCE_DBG_PRINTF_LEVEL(2, 15, 3105, "%s: File length is over\n", "UVFAT_SearchGetFile_ex");
#endif
		return -32;
	}
	if (mode == 1)
		nameHash = 0;
	else
		nameHash = checksumUtf16Name(filename, fatNameLength(filename));
	name = allocateFatBuffer(512);
	if (!name) {
#if FW_DEBUG
		SCE_DBG_PRINTF_LEVEL(2, 15, 3118, "%s: No memory\n", "UVFAT_SearchGetFile_ex");
		SCE_DBG_PRINTF_LEVEL(1, 0, 3120, "%s: salloc failed\n", "UVFAT_SearchGetFile_ex");
#endif
		return -10;
	}
	file = fd ? &sharedResource->files[fd] : &fileSystem->file;

/* The set checksum covers the wire representation, except its own two bytes. */
#define ACCUMULATE_ENTRY(record) do { \
	const unsigned char *bytes = (const unsigned char *)&(record); \
	unsigned int byteIndex; \
	if (bytes[0] == 0x85) { \
		for (byteIndex = 0; byteIndex != sizeof(record); byteIndex++) { \
			if ((SceUInt16)(byteIndex - offsetof(SceExfatfsFileDirectoryEntry, setChecksum)) <= 1) \
				continue; \
			checksum = (SceUInt16)((checksum >> 1) + \
				((checksum & 1) ? 0x8000 : 0) + bytes[byteIndex]); \
		} \
	} else { \
		for (byteIndex = 0; byteIndex != sizeof(record); byteIndex++) \
			checksum = (SceUInt16)((checksum >> 1) + \
				((checksum & 1) ? 0x8000 : 0) + bytes[byteIndex]); \
	} \
} while (0)

	for (;;) {
		result = readSingleDirectoryEntry(sharedResource, fd, &primary, fileSystem);
		if (result)
			break;
		if (!primary.file.entryType) {
			result = -7;
			break;
		}
		if (primary.file.entryType != 0x85)
			continue;
readStream:
		directoryCluster = file->Cluster;
		result = readSingleDirectoryEntry(sharedResource, fd, &stream, fileSystem);
		if (result)
			break;
		entryType = stream.stream.entryType;
		if (entryType != 0xc0) {
#if FW_DEBUG
			SCE_DBG_PRINTF_LEVEL(2, 15, 3186, "%s: Not stream entry type found\n", "UVFAT_SearchGetFile_ex");
#endif
			if (entryType == 0x85) {
				primary = stream;
				goto readStream;
			}
			continue;
		}
		if (mode != 1 && nameHash != (stream.stream.nameHash[0] |
			((unsigned int)stream.stream.nameHash[1] << 8)))
			continue;
		checksum = 0;
		ACCUMULATE_ENTRY(primary);
		ACCUMULATE_ENTRY(stream);
		secondaryCount = primary.file.secondaryCount;
		nameEntryCount = ((unsigned int)stream.stream.nameLength + 14) / 15;
		if (secondaryCount < nameEntryCount + 1) {
#if FW_DEBUG
			SCE_DBG_PRINTF_LEVEL(2, 15, 3215, "Invalid vendor entry count\n");
#endif
			continue;
		}
		memset(name, 0, 512);
		nameIndex = 0;
		for (entryIndex = 0; entryIndex != nameEntryCount; entryIndex++) {
			result = readSingleDirectoryEntry(sharedResource, fd, &secondary, fileSystem);
			if (result)
				goto finished;
			entryType = secondary.filename.entryType;
			if (entryType == 0xc1) {
#define COPY_NAME_CHARACTER(index) \
	name[nameIndex + (index)] = secondary.filename.name[index][0] | \
		((unsigned int)secondary.filename.name[index][1] << 8)
				COPY_NAME_CHARACTER(0);
				COPY_NAME_CHARACTER(1);
				COPY_NAME_CHARACTER(2);
				COPY_NAME_CHARACTER(3);
				COPY_NAME_CHARACTER(4);
				COPY_NAME_CHARACTER(5);
				COPY_NAME_CHARACTER(6);
				COPY_NAME_CHARACTER(7);
				COPY_NAME_CHARACTER(8);
				COPY_NAME_CHARACTER(9);
				COPY_NAME_CHARACTER(10);
				COPY_NAME_CHARACTER(11);
				COPY_NAME_CHARACTER(12);
				COPY_NAME_CHARACTER(13);
				COPY_NAME_CHARACTER(14);
#undef COPY_NAME_CHARACTER
				nameIndex += 15;
				checksum = (SceUInt16)checksum;
				ACCUMULATE_ENTRY(secondary);
			} else {
#if FW_DEBUG
				SCE_DBG_PRINTF_LEVEL(2, 15, 3230, "Invalid entry type found(0x%x), filename\n", entryType);
#endif
				if (entryType == 0x85) {
					primary = secondary;
					goto readStream;
				}
				checksum = -1;
			}
		}
		name[nameIndex] = 0;
		vendorCount = secondaryCount - 1 - entryIndex;
		for (; vendorCount; vendorCount--) {
			result = readSingleDirectoryEntry(sharedResource, fd, &secondary, fileSystem);
			if (result)
				goto finished;
			if (secondary.filename.entryType & 0x40) {
				checksum = (SceUInt16)checksum;
				ACCUMULATE_ENTRY(secondary);
			} else {
				checksum = -1;
#if FW_DEBUG
				SCE_DBG_PRINTF_LEVEL(2, 15, 3256, "Invalid entry type found(0x%x), vendor\n", secondary.filename.entryType);
#endif
			}
		}
		result = 0;
		if (checksum != (primary.file.setChecksum[0] |
			((unsigned int)primary.file.setChecksum[1] << 8)))
			continue;
		if (search) {
			result = UVFAT_ToUpperBuf(name, name, 256, fileSystem);
			if (result) {
#if FW_DEBUG
				SCE_DBG_PRINTF_LEVEL(2, 15, 3276, "%s: toupper fail\n", "UVFAT_SearchGetFile_ex");
#endif
				break;
			}
			if (fatNameCompare(name, filename))
				continue;
			for (entryIndex = 0; entryIndex != secondaryCount + 1; entryIndex++) {
				SceExfatfsFile *positionFile = fd ? &sharedResource->files[fd] : &fileSystem->file;

				if ((SceUInt64)positionFile->Position > 31)
					positionFile->Position -= 32;
			}
		} else {
			fatNameCopyBounded(filename, name, 256);
			directoryCluster = fd ? sharedResource->files[fd].Cluster : fileSystem->file.Cluster;
		}
		if (fnode) {
			const unsigned char *cluster = stream.stream.firstCluster;
			const unsigned char *length = stream.stream.dataLength;
			SceExfatfsFile *positionFile;

			memset(&fnode->Stat, 0, sizeof(fnode->Stat));
			copyExfatEntryAttributeToStat(&primary.file, &fnode->Stat, fileSystem);
			decodeExfatEntryDateTimes(&fnode->Stat, &primary.file);
			fnode->Stat.StartCluster = cluster[0] | ((SceUInt32)cluster[2] << 16) |
				((SceUInt32)cluster[3] << 24) | ((SceUInt32)cluster[1] << 8);
			fnode->Stat.Size = (SceUInt64)(length[0] | ((SceUInt32)length[2] << 16) |
				((SceUInt32)length[3] << 24) | ((SceUInt32)length[1] << 8)) |
				((SceUInt64)(length[4] | ((SceUInt32)length[6] << 16) |
				((SceUInt32)length[7] << 24) | ((SceUInt32)length[5] << 8)) << 32);
			_initialize_cluster_cache_data(fnode);
			_set_start_cluster_and_cluster_count(fnode, fnode->Stat.StartCluster, fnode->Stat.Size);
#if FW_DEBUG
			SCE_DBG_PRINTF_LEVEL(2, 0, 3325, "%s: StartCluster[%#lx], Size[%#llx]\n",
				"UVFAT_SearchGetFile_ex", fnode->Stat.StartCluster, fnode->Stat.Size);
#endif
			fnode->DirOffClust = directoryCluster;
			positionFile = fd ? &sharedResource->files[fd] : &fileSystem->file;
			fnode->DirOffset = positionFile->Position;
			fnode->DirCluster = positionFile->fnode->DirCluster;
			fnode->Stat.generalSecondaryFlags = stream.stream.generalSecondaryFlags;
		}
		break;
	}
finished:
	freeFatBuffer(name, 512);
	return result;
#undef ACCUMULATE_ENTRY
}

#if FW_VERSION >= 0x04000000
int readTemporalDirectoryEntry(SceExfatfsSharedResource *sharedResource,
	void *primary, void *secondary, SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsFile *file = &fileSystem->file;
	SceExfatfsBootSector *boot = fileSystem->PBRBuf;
	const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
	void *entry;
	int result;

	if (!file->fnode->DirCluster && !(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6870, "UVFAT_ReadDirEntry",
			"%s: unexpected StartCluster[%#lx]\n", "UVFAT_ReadDirEntry", file->fnode->Stat.StartCluster);
		file->fnode->DirCluster = _root_cluster(fileSystem);
	}
	entry = primary;
	if (!entry) {
		entry = secondary;
		if (!entry) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6908, "UVFAT_ReadDirEntry",
				"%s: NULL buffer\n", "UVFAT_ReadDirEntry");
			return -32;
		}
	}
	SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 6919, "UVFAT_ReadDirEntry",
		"%s: fd(%d), fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], StartCluster[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], _clusterIndex[%#lx]\n",
		"UVFAT_ReadDirEntry", 0, file->fnode, file->fnode->DirCluster,
		file->fnode->DirOffset, file->fnode->DirOffClust, file->fnode->Stat.StartCluster,
		file, file->Position, file->Cluster, file->clusterIndex);
	result = readDirectoryEntryBytes(sharedResource, fileSystem, file, entry);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 6923, "UVFAT_ReadDirEntry", "UVFAT_ReadData() Fail, er=%d\n", result);
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6924, "UVFAT_ReadDirEntry",
			"%s: UVFAT_ReadData failed[%#lx]\n", "UVFAT_ReadDirEntry", result);
		return result;
	}
	if (!result)
		return -7;
	if (primary && secondary)
		*(SceExfatfsDirectoryRecord *)secondary = *(const SceExfatfsDirectoryRecord *)primary;
	return 0;
}
#endif

int _SearchFileFromDir(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFatDirectoryEntry *entry,
	const SceUInt16 *filename)
{
	SceUInt16 shortNameUnicode[13];
	SceExfatfsFatDirectoryRecord record;
	SceExfatfsFatLongDirectoryEntry *longEntry = &record.longEntry;
	SceUInt16 fragments[20][14];
	unsigned char shortName[13];
	SceUInt16 *name;
	unsigned int fragmentCount = 0, checksum = 0;
	unsigned int firstCharacter, sum, index;
	int result;

	if (!*filename)
		return -5;
	name = allocateFatBuffer(512);
	if (!name) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 2471, "%s: memory alloc fail\n", "_SearchFileFromDir");
		SCE_DBG_PRINTF_LEVEL(1, 0, 2473, "%s: salloc failed\n", "_SearchFileFromDir");
		return -10;
	}
	for (;;) {
#if FW_VERSION >= 0x04000000
		result = readTemporalDirectoryEntry(sharedResource, entry, &record, fileSystem);
#else
		SceExfatfsFile *file = &fileSystem->file;
		SceExfatfsBootSector *boot = fileSystem->PBRBuf;
		const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

		if (!file->fnode->DirCluster && !(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
			if (fileSystem->fileSystemType == 2)
				file->fnode->DirCluster = fileSystem->firstClusterOfRoot;
			else
				file->fnode->DirCluster = boot->fat32.rootCluster;
		}
		result = readDirectoryEntryBytes(sharedResource, fileSystem, file, entry ? (void *)entry : &record);
		if (result > 0) {
			if (entry)
				record.shortEntry = *entry;
			result = 0;
		} else if (!result) {
			result = -7;
		}
#endif
		if (result) {
			if (result == -7)
				result = -4;
			break;
		}
		firstCharacter = entry->name[0];
		if (!firstCharacter) {
			result = -4;
			break;
		}
		if (firstCharacter == 0xe5) {
			fragmentCount = 0;
			continue;
		}
		if (entry->attribute == 15) {
			if (longEntry->sequence & 0x40) {
				checksum = longEntry->checksum;
				fragmentCount = longEntry->sequence & 0x1f;
			} else if (!fragmentCount) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 2515, "%s: skip LFN entry without lastEntry\n", "_SearchFileFromDir");
				continue;
			} else if (longEntry->checksum != checksum) {
				unsigned int lastEntry = longEntry->sequence & 0x40;

				SCE_DBG_PRINTF_LEVEL(1, 0, 2519, "%s: skip sum[%#x] != sum of lastEntry[%#x]\n",
					"_SearchFileFromDir", longEntry->checksum, checksum);
				fragmentCount = lastEntry;
				continue;
			}
			{
				SceUInt16 *fragment = fragments[(longEntry->sequence & 0x1f) - 1];

#define LFN_CHARACTER(pair) ((pair)[0] | ((unsigned int)(pair)[1] << 8))
				fragment[0] = LFN_CHARACTER(longEntry->namePrefix[0]);
				fragment[1] = LFN_CHARACTER(longEntry->namePrefix[1]);
				fragment[2] = LFN_CHARACTER(longEntry->namePrefix[2]);
				fragment[3] = LFN_CHARACTER(longEntry->namePrefix[3]);
				fragment[4] = LFN_CHARACTER(longEntry->namePrefix[4]);
				fragment[5] = LFN_CHARACTER(longEntry->nameMiddle[0]);
				fragment[6] = LFN_CHARACTER(longEntry->nameMiddle[1]);
				fragment[7] = LFN_CHARACTER(longEntry->nameMiddle[2]);
				fragment[8] = LFN_CHARACTER(longEntry->nameMiddle[3]);
				fragment[9] = LFN_CHARACTER(longEntry->nameMiddle[4]);
				fragment[10] = LFN_CHARACTER(longEntry->nameMiddle[5]);
				fragment[11] = LFN_CHARACTER(longEntry->nameSuffix[0]);
				fragment[12] = LFN_CHARACTER(longEntry->nameSuffix[1]);
				fragment[13] = 0;
#undef LFN_CHARACTER
			}
			continue;
		}
		if (entry->attribute & 8) {
			fragmentCount = 0;
			continue;
		}
		if (firstCharacter <= 0x20 && firstCharacter != 5) {
			_exfatfs_notify_invalid_directory_entry(fileSystem->drive, &record);
			fragmentCount = 0;
			continue;
		}
		if (fragmentCount) {
			sum = firstCharacter;
#define LFN_CHECKSUM(character) \
	sum = (unsigned char)(((sum >> 1) | (sum << 7)) + (character))
			LFN_CHECKSUM(entry->name[1]);
			LFN_CHECKSUM(entry->name[2]);
			LFN_CHECKSUM(entry->name[3]);
			LFN_CHECKSUM(entry->name[4]);
			LFN_CHECKSUM(entry->name[5]);
			LFN_CHECKSUM(entry->name[6]);
			LFN_CHECKSUM(entry->name[7]);
			LFN_CHECKSUM(entry->extension[0]);
			LFN_CHECKSUM(entry->extension[1]);
			LFN_CHECKSUM(entry->extension[2]);
#undef LFN_CHECKSUM
			if (sum == checksum) {
				*name = 0;
				for (index = 0; index != fragmentCount; index++)
					fatNameAppendBounded(name, fragments[index], 13);
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0, 2555, "%s: short entry, sum[%#x] != sum of LFN[%#x]\n",
					"_SearchFileFromDir", sum, checksum);
				*name = 0;
			}
		} else {
			*name = 0;
		}
		result = UVFAT_ToUpperBuf(name, name, fatNameLength(name), fileSystem);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 2569, "%s: toupper fail\n", "_SearchFileFromDir");
			break;
		}
		if (!fatNameCompare(filename, name))
			goto found;
		{
			unsigned char *destination = shortName;

#define COPY_SHORT_CHARACTER(index) \
	if (entry->name[index] != ' ') *destination++ = entry->name[index]
			COPY_SHORT_CHARACTER(0);
			COPY_SHORT_CHARACTER(1);
			COPY_SHORT_CHARACTER(2);
			COPY_SHORT_CHARACTER(3);
			COPY_SHORT_CHARACTER(4);
			COPY_SHORT_CHARACTER(5);
			COPY_SHORT_CHARACTER(6);
			COPY_SHORT_CHARACTER(7);
#undef COPY_SHORT_CHARACTER
			if (entry->extension[0] != ' ') {
				destination[1] = entry->extension[0];
				destination[0] = '.';
				destination += 2;
			}
			if (entry->extension[1] != ' ')
				*destination++ = entry->extension[1];
			if (entry->extension[2] != ' ')
				*destination++ = entry->extension[2];
			*destination = 0;
			if (*shortName == 5)
				*shortName = 0xe5;
		}
		/* Sony's inlined _CompShortName checks twelve converted characters. */
		if ((unsigned int)fatNameLength(filename) > 12) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2117, "_CompShortName", "search file is over length\n");
		} else {
			int converted = fatCodepageToUtf16(shortNameUnicode, 26, shortName);

			if (converted < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2125, "_CompShortName", "short name convert errot\n");
#define UPPER_SHORT_CHARACTER(index) \
	if ((unsigned int)shortNameUnicode[index] - 'a' <= 'z' - 'a') \
		shortNameUnicode[index] -= 'a' - 'A'
			UPPER_SHORT_CHARACTER(0);
			UPPER_SHORT_CHARACTER(1);
			UPPER_SHORT_CHARACTER(2);
			UPPER_SHORT_CHARACTER(3);
			UPPER_SHORT_CHARACTER(4);
			UPPER_SHORT_CHARACTER(5);
			UPPER_SHORT_CHARACTER(6);
			UPPER_SHORT_CHARACTER(7);
			UPPER_SHORT_CHARACTER(8);
			UPPER_SHORT_CHARACTER(9);
			UPPER_SHORT_CHARACTER(10);
			UPPER_SHORT_CHARACTER(11);
#undef UPPER_SHORT_CHARACTER
			if (!fatNameCompare(filename, shortNameUnicode)) {
found:
				if (!g_codepageTablesEnabled && sceFatfsACCheckRDirEntry(fileSystem->drive, entry->attribute) < 0)
					result = -5;
				break;
			}
		}
		fragmentCount = 0;
	}
	freeFatBuffer(name, 512);
	return result;
}

int _update_metadata(SceExfatfsSharedResource *sharedResource, int fd)
{
	SceExfatfsFnode *fnode = sharedResource->files[fd].fnode;
	int result;

	if (!fnode->fileSystem)
		return 0;
	flushFileSystemMetadata(fnode->fileSystem);
#if FW_VERSION >= 0x04000000
	if (!_has_parent_directory(fnode))
		return 0;
#else
	{
		SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
		SceUInt32 startCluster;

		if (!fileSystem || !fnode->FilePath[0] || !fnode->FileName ||
			fnode->DirCluster == (SceUInt32)-2)
			return 0;
		startCluster = fnode->Stat.StartCluster;
		if (startCluster) {
			SceUInt32 rootCluster;

			if (fileSystem->fileSystemType == 2) {
				rootCluster = fileSystem->firstClusterOfRoot;
			} else {
				SceExfatfsBootSector *boot = fileSystem->PBRBuf;
				const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

				if (rootEntries[0] | ((unsigned int)rootEntries[1] << 8))
					goto updateModificationTime;
				rootCluster = boot->fat32.rootCluster;
			}
			if (startCluster == rootCluster)
				return 0;
		} else if (!g_codepageTablesEnabled && fnode->Stat.st_type) {
			return 0;
		}
	}
updateModificationTime:
#endif
	getCurrentFatDateTime(&fnode->Stat.mtime);
	fnode->flags |= 4;
	result = _exfatfs_flush_metadata(sharedResource, fnode);
	if (!result)
		fnode->flags &= ~4;
	return result;
}

int UVFAT_WriteData(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFile *file,
	const void *buffer, unsigned int size, unsigned int flags,
	unsigned int *writeResult)
{
	unsigned int contiguousCount = 0;
	unsigned int remaining = size, transferred = 0;
	unsigned int clusterSize, bufferSize;
	SceExfatfsFnode *fnode;
	int result;

	if ((flags & 1) && (file->mode > 8 ||
		(int)(0x22800000U << file->mode) >= 0))
		return -2;
	clusterSize = fileSystemClusterSize(fileSystem);
	fnode = file->fnode;
	bufferSize = fnode->fileSystem->fileBufferSize;
	if (!size)
		return 0;
	for (;;) {
		SceUInt64 position = file->Position;
		SceUInt64 offset = position % bufferSize;
		SceUInt64 cacheIndex = file->cacheIndex;
		unsigned int amount;
		int allocated = 0;
#if FW_VERSION < 0x04000000
		SceUInt32 startCluster;
#endif

		if (cacheIndex != (SceUInt64)-1) {
			unsigned int cacheSize = fnode->fileSystem->fileBufferSize;
			SceUInt64 cacheStart = cacheIndex * cacheSize;

			if (position < cacheStart + cacheSize && cacheStart < position + 1) {
				unsigned int overlap = position + 1 - cacheStart;

				if (overlap >= cacheSize)
					overlap = cacheSize;
				if (overlap)
					goto copyCached;
			}
		}
		if (file->cacheFlags & 1) {
			result = UVFAT_FlushFdBuf(file, fnode->fileSystem);
			if (result) {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5498, "_flush_fd_cache",
					"%s: UVFAT_FlushFdBuf() Fail[%#x]\n", "_flush_fd_cache", result);
				SCE_DBG_PRINTF_LEVEL(1, 0, 6447,
					"%s: _flush_fd_cache() failed[%#x]\n", "UVFAT_WriteData", result);
				if (flags & 1)
					flushFileSystemMetadata(fileSystem);
				return result;
			}
			fnode = file->fnode;
			if ((flags & 1) && fnode->fileSystem) {
#if FW_VERSION >= 0x04000000
				flushFileSystemMetadata(fnode->fileSystem);
				if (!_has_parent_directory(fnode))
					goto metadataDone;
#else
				SceExfatfsFileSystem *metadataFileSystem;
				SceUInt32 rootCluster;

				flushFileSystemMetadata(fnode->fileSystem);
				metadataFileSystem = fnode->fileSystem;
				if (!metadataFileSystem || !fnode->FilePath[0] ||
					!fnode->FileName || fnode->DirCluster == (SceUInt32)-2)
					goto metadataDone;
				startCluster = fnode->Stat.StartCluster;
				if (!startCluster) {
					if (g_codepageTablesEnabled || !fnode->Stat.st_type)
						goto updateTime;
					goto metadataDone;
				}
				if (metadataFileSystem->fileSystemType == 2) {
					rootCluster = metadataFileSystem->firstClusterOfRoot;
				} else {
					SceExfatfsBootSector *boot = metadataFileSystem->PBRBuf;
					const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

					if (rootEntries[0] | ((unsigned int)rootEntries[1] << 8))
						goto updateTime;
					rootCluster = boot->fat32.rootCluster;
				}
				if (startCluster == rootCluster)
					goto metadataDone;
updateTime:
#endif
				getCurrentFatDateTime(&fnode->Stat.mtime);
				fnode->flags |= 4;
				result = _exfatfs_flush_metadata(sharedResource, fnode);
				if (result) {
					SCE_DBG_PRINTF_LEVEL(1, 0, 6457,
						"%s: _update_metadata() failed[%#x]\n", "UVFAT_WriteData", result);
					return result;
				}
				fnode->flags &= ~4;
metadataDone:
				fnode = file->fnode;
			}
		}
		SCE_DBG_PRINTF_LEVEL(2, 0, 6462,
			"%s: call _start_cluster, dirFlag[%#x]\n", "UVFAT_WriteData", flags);
#if FW_VERSION >= 0x04000000
		if (_start_cluster(file->fnode, flags & 2) == (SceUInt32)-1) {
			result = _assign_first_cluster_of_file(file, sharedResource);
			if (result) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 6468,
					"%s: _assign_first_cluster_of_file failed(%d)\n", "UVFAT_WriteData", result);
				return result;
			}
			contiguousCount = 1;
			allocated = 1;
		} else {
			SCE_DBG_PRINTF_LEVEL(2, 0, 6479,
				"%s: fnode[%p], index[%#lx], cluster[%#lx], Position[%#llx]\n",
				"UVFAT_WriteData", file->fnode, file->clusterIndex, file->Cluster, file->Position);
		}
#else
		startCluster = flags & 2 ? fnode->DirCluster : fnode->Stat.StartCluster;
		if (!startCluster) {
			if (!(flags & 2) && fnode->Stat.st_type != 6) {
				fnode->tailCluster = (SceUInt32)-1;
				fnode->tailIndex = 0;
				startCluster = (SceUInt32)-1;
			} else {
				SceExfatfsFileSystem *startFileSystem = fnode->fileSystem;
				SceExfatfsBootSector *boot = startFileSystem->PBRBuf;
				const unsigned char *rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;

				if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8)))
					startCluster = startFileSystem->fileSystemType == 2 ?
						startFileSystem->firstClusterOfRoot : boot->fat32.rootCluster;
			}
		}
		if (startCluster == (SceUInt32)-1) {
			SceUInt32 cluster = UVFAT_SearchEmptyCluster(sharedResource,
				fnode->fileSystem, 0);

			fnode = file->fnode;
			if (cluster >= fnode->fileSystem->clusterCount + 2)
				return -10;
			if (fnode->fileSystem->fileSystemType == 2) {
				fnode->Stat.generalSecondaryFlags |= 3;
			} else {
				if (fnode->Stat.generalSecondaryFlags) {
					fnode->Stat.generalSecondaryFlags = 0;
					fnode = file->fnode;
				}
				result = UVFAT_SetEOF(fnode->fileSystem, cluster);
				if (result)
					return result;
			}
			fnode = file->fnode;
			fnode->Stat.StartCluster = cluster;
			if (!fnode->clusterCount)
				_set_start_cluster(fnode, cluster, fnode->Stat.st_type);
			file->Cluster = cluster;
			file->clusterIndex = 0;
			contiguousCount = 1;
			allocated = 1;
		}
#endif
		result = 0;
		if (!contiguousCount) {
#if FW_VERSION >= 0x04000000
			if (!file->Cluster && file->clusterIndex)
				SCE_DBG_PRINTF_LEVEL(1, 0, 6490,
					"%s: before _set_position_and_cluster_index, filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
					"UVFAT_WriteData", file, file->Position, file->Cluster, file->clusterIndex, file->cacheIndex);
			else
				SCE_DBG_PRINTF_LEVEL(2, 0, 6492,
					"%s: before _set_position_and_cluster_index for Position[%#llx], filep[%p], fnode[%p]\n",
					"UVFAT_WriteData", file->Position, file, file->fnode);
#endif
			result = _set_position_and_cluster_index(file, file->Position, flags);
			if (result == -7) {
				SCE_DBG_PRINTF_LEVEL(2, 0, 6496,
					"%s: _set_position_and_cluster_index finished(EOF), Position[%#llx], offset[%#llx], size[%#lx]\n",
					"UVFAT_WriteData", file->Position, offset, remaining);
#if FW_VERSION >= 0x04000000
				if ((SceUInt64)file->Position % clusterSize)
					SCE_DBG_PRINTF_LEVEL(1, 0, 6499,
						"%s: unexpected Position[%#llx]\n", "UVFAT_WriteData", file->Position);
				SCE_DBG_PRINTF_LEVEL(2, 0, 6526,
					"%s: before _append_contiguous_clusters, Position[%#llx], index[%#lx], offset[%#llx], size[%#lx], dirFlag[%#x]\n",
					"UVFAT_WriteData", file->Position, file->clusterIndex, offset, remaining, flags);
				if (offset)
					SCE_DBG_PRINTF_LEVEL(1, 0, 6528,
						"%s: unexpected offset[%#llx], Position[%#llx]\n", "UVFAT_WriteData", offset, file->Position);
#endif
				result = _append_contiguous_clusters(file, remaining, flags,
					sharedResource, &contiguousCount);
				if (!result) {
					if ((flags & 2) && writeResult)
						*writeResult += contiguousCount;
					allocated = 1;
				} else {
					SCE_DBG_PRINTF_LEVEL(1, 0, 6537,
						"%s: _append_contiguous_clusters failed(%d), Position[%#llx], offset[%#llx], size[%#lx], current_cluster[%#lx]\n",
						"UVFAT_WriteData", result, file->Position, offset, remaining, file->Cluster);
				}
				goto haveContiguousCount;
			}
			if (result) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 6506,
					"%s: _set_position_and_cluster_index failed(%d)\n", "UVFAT_WriteData", result);
				return result;
			}
#if FW_VERSION >= 0x04000000
			{
				SceUInt64 newOffset = (SceUInt64)file->Position % bufferSize;

				if (newOffset != offset)
					SCE_DBG_PRINTF_LEVEL(1, 0, 6510,
						"%s: after _set_position_and_cluster_index, Size[%#llx], Position[%#llx], clusterIndex[%#lx], Cluster[%#lx], offset_in_block[%#llx]->[%#llx], filep[%p], fnode[%p], flags[%#x]\n",
						"UVFAT_WriteData", file->fnode->Stat.Size, file->Position, file->clusterIndex, file->Cluster, offset, newOffset, file, file->fnode, flags);
				else
					SCE_DBG_PRINTF_LEVEL(2, 0, 6512,
						"%s: _set_position_and_cluster_index finished, Position[%#llx], index[%#lx], offset[%#llx], size[%#lx]\n",
						"UVFAT_WriteData", file->Position, file->clusterIndex, offset, remaining);
			}
#endif
		}
		SCE_DBG_PRINTF_LEVEL(2, 0, 6541,
			"%s: before _check_contiguous_clusters, Position[%#llx], index[%#lx], offset[%#llx], size[%#lx]\n",
			"UVFAT_WriteData", file->Position, file->clusterIndex, offset, remaining);
		contiguousCount = _check_contiguous_clusters(file, file->Position, remaining, flags);
haveContiguousCount:
		if (contiguousCount)
			SCE_DBG_PRINTF_LEVEL(2, 0, 6559,
				"%s: Position[%#llx], clusterIndex[%#lx], contiguous_count[%#lx], offset[%#llx], size[%#lx]\n",
				"UVFAT_WriteData", file->Position, file->clusterIndex, contiguousCount, offset, remaining);
		if (!contiguousCount) {
			if (result)
				return result;
		} else if (offset || remaining < bufferSize) {
			if (((flags & 1) && (SceUInt64)(file->Position - offset) >= file->fnode->Stat.Size) || allocated) {
				SceExfatfsFileSystem *cacheFileSystem;
				unsigned int cacheSize, cacheClusterSize;
				SceUInt32 cluster;

				memset(file->Buf, 0, bufferSize);
				position = file->Position;
				cluster = file->Cluster;
				cacheFileSystem = file->fnode->fileSystem;
				cacheSize = cacheFileSystem->fileBufferSize;
				cacheClusterSize = fileSystemClusterSize(cacheFileSystem);
				cacheIndex = position / cacheSize;
				file->cacheIndex = cacheIndex;
				file->cacheCluster = cluster;
				file->clustBlockNo = cacheIndex % (cacheClusterSize / cacheSize);
			} else {
				SCE_DBG_PRINTF_LEVEL(2, 0, 6571,
					"%s: before _read_into_fd_cache, buf[%p], size[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
					"UVFAT_WriteData", buffer, remaining, file, file->Position, file->Cluster, file->clusterIndex, file->cacheIndex);
				result = _read_into_fd_cache(file, file->Cluster);
				if (result) {
					SCE_DBG_PRINTF_LEVEL(1, 0, 6574,
						"%s: _read_into_fd_cache failed(%d), buf[%p], size[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], clusterIndex[%#lx], cacheIndex[%#llx]\n",
						"UVFAT_WriteData", result, buffer, remaining, file, file->Position, file->Cluster, file->clusterIndex, file->cacheIndex);
					return result;
				}
			}
		} else {
			result = _write_sectors(file, flags, sharedResource,
				contiguousCount, remaining, buffer);
			if (result < 0)
				return result;
			if (result) {
				remaining -= result;
				transferred += result;
				if (!remaining)
					return transferred;
				if (!(flags & 0x100))
					buffer = (const unsigned char *)buffer + result;
				contiguousCount = 0;
				fnode = file->fnode;
				continue;
			}
		}
copyCached:
		amount = remaining;
		if ((SceUInt64)bufferSize - offset < remaining)
			amount = bufferSize - offset;
		if (!(flags & 0x100)) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 6617,
				"%s: filep[%p], offset[%#llx], copysize[%#lx], buf[%p]\n", "UVFAT_WriteData", file, offset, amount, buffer);
			if (amount) {
				const void *source = buffer;

				file->cacheFlags |= 1;
				buffer = (const unsigned char *)buffer + amount;
				memcpy((unsigned char *)file->Buf + offset, source, amount);
				sync_multi_open(sharedResource, file);
			}
		}
		remaining -= amount;
		transferred += amount;
		position = file->Position + amount;
		file->Position = position;
		if ((flags & 1) && file->fnode->Stat.Size < position) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 6629,
				"%s: Size[%#llx]->[%#llx]\n", "UVFAT_WriteData", file->fnode->Stat.Size, position);
			file->fnode->Stat.Size = file->Position;
		} else if (!(flags & 1) && file->fnode->dirSize && file->fnode->dirSize < position) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 6635,
				"%s: unexpected dirSize[%#x] < Position[%#llx]\n", "UVFAT_WriteData", file->fnode->dirSize, position);
		}
		if (contiguousCount) {
#if FW_VERSION >= 0x04000000
			position = file->Position;
#endif
			if (!(position % clusterSize)) {
#if FW_VERSION >= 0x04000000
				if (position != clusterSize + file->clusterIndex * clusterSize)
					SCE_DBG_PRINTF_LEVEL(1, 0, 6648,
						"%s: unexpected Position[%#llx], clusterIndex[%#lx], clusterSize[%#lx], contCnt(%ld)\n",
						"UVFAT_WriteData", position, file->clusterIndex, clusterSize, contiguousCount);
#endif
				if (--contiguousCount) {
					file->Cluster++;
					file->clusterIndex++;
				}
			}
		}
		if (!remaining)
			return transferred;
		fnode = file->fnode;
	}
}

#if FW_VERSION >= 0x04000000
int UVFAT_WriteDirEntry_ex(SceExfatfsSharedResource *sharedResource, int fd,
	const void *primary, const void *secondary, int *writeResult,
	SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsFile *file;
	SceExfatfsDirectoryRecord empty;
	int result;

	if (fd) {
		file = &sharedResource->files[fd];
		SCE_DBG_PRINTF_LEVEL(1, 0, 6969,
			"%s: not safe, fd(%d)\n", "UVFAT_WriteDirEntry_ex", fd);
	} else {
		file = &fileSystem->file;
	}
	if (file->fnode->Stat.st_type != 6)
		SCE_DBG_PRINTF_LEVEL(1, 0, 6972,
			"%s: unexpected st_type(%d), fnode[%p], st_mode[%#o]\n",
			"UVFAT_WriteDirEntry_ex", file->fnode->Stat.st_type,
			file->fnode, file->fnode->Stat.st_mode);
	if (!primary) {
		primary = secondary;
		if (!secondary) {
			empty = (SceExfatfsDirectoryRecord){ 0 };
			primary = &empty;
		}
	}
	SCE_DBG_PRINTF_LEVEL(2, 0, 6989,
		"%s: fd(%d), fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], StartCluster[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], _clusterIndex[%#lx]\n",
		"UVFAT_WriteDirEntry_ex", fd, file->fnode, file->fnode->DirCluster,
		file->fnode->DirOffset, file->fnode->DirOffClust,
		file->fnode->Stat.StartCluster, file, file->Position,
		file->Cluster, file->clusterIndex);
	result = UVFAT_WriteData(sharedResource, fileSystem, file,
		primary, 32, 2, (unsigned int *)writeResult);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 15, 6993, "UVFAT_WriteData() Fail, er=%d\n", result);
	else
		result = 0;
	return result;
}
#endif

int UVFAT_WriteStatDir_ex(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceExfatfsStat *stat)
{
	SceExfatfsDirectoryRecord primary, stream, vendor;
	unsigned int checksum = 0;
	int result, secondaryCount, remaining, index;
#if FW_DEBUG
	SceUInt64 position = fileSystem->file.Position;
#endif
	SceUInt32 clusterSize = fileSystemClusterSize(fileSystem);

#if FW_DEBUG
	if (!(position % clusterSize))
		SCE_DBG_PRINTF_LEVEL(2, 15, 4537,
			"clust=%ld, offset=%lld, startClust=%ld\n",
			fileSystem->file.Cluster, fileSystem->file.Position,
			fileSystem->file.fnode->DirCluster);
#else
	(void)clusterSize;
#endif

#if FW_DEBUG
#define READ_STAT_ENTRY(entry) \
	(result = readTemporalDirectoryEntry(sharedResource, &(entry), 0, fileSystem))
#else
#define READ_STAT_ENTRY(entry) do { \
	if (!fileSystem->file.fnode->DirCluster && \
		!((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat32.parameters.rootEntryCount) \
		fileSystem->file.fnode->DirCluster = _root_cluster(fileSystem); \
	result = readDirectoryEntryBytes(sharedResource, fileSystem, &fileSystem->file, &(entry)); \
	if (result > 0) result = 0; \
	else if (!result) result = -7; \
} while (0)
#endif
#define CHECKSUM_STAT_ENTRY(entry, skipChecksum) do { \
	const unsigned char *bytes = (const unsigned char *)&(entry); \
	unsigned int byteIndex; \
	for (byteIndex = 0; byteIndex != sizeof(entry); byteIndex++) { \
		if ((skipChecksum) && \
			(SceUInt16)(byteIndex - offsetof(SceExfatfsFileDirectoryEntry, setChecksum)) <= 1) \
			continue; \
		checksum = (SceUInt16)((checksum >> 1) + \
			((checksum & 1) ? 0x8000 : 0) + bytes[byteIndex]); \
	} \
} while (0)
	READ_STAT_ENTRY(primary);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4546, "%s: UVFAT_ReadDirEntry(1) failed(%d)\n", "UVFAT_WriteStatDir_ex", result);
		SCE_DBG_PRINTF_LEVEL(2, 15, 4547, "Cannot read direntry\n");
		return result;
	}
	if (!primary.file.entryType) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 4552, "slot empty\n");
		return -7;
	}
	if (primary.file.entryType != 0x85) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4555, "%s: entryType[%#x] != expected(1)\n", "UVFAT_WriteStatDir_ex", primary.file.entryType);
		SCE_DBG_PRINTF_LEVEL(2, 15, 4556, "Not start point of direntry set\n");
		SCE_DBG_PRINTF_LEVEL(2, 15, 4567, "scode=0x%x, clust=%ld, offset=%lld, startClust=%ld\n",
			primary.file.entryType, fileSystem->file.Cluster,
			fileSystem->file.Position, fileSystem->file.fnode->DirCluster);
		return -32;
	}
	result = UVFAT_ChangeStatFileentry(fileSystem, &primary.file, stat);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 4573, "UVFAT_ChangeStatFileentry() Fail\n");
		SCE_DBG_PRINTF_LEVEL(1, 0, 4574, "%s: UVFAT_ChangeStatFileentry failed(%d)\n", "UVFAT_WriteStatDir_ex", result);
		return result;
	}
	CHECKSUM_STAT_ENTRY(primary, primary.file.entryType == 0x85);
	READ_STAT_ENTRY(stream);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4584, "%s: UVFAT_ReadDirEntry(2) failed(%d)\n", "UVFAT_WriteStatDir_ex", result);
		SCE_DBG_PRINTF_LEVEL(2, 15, 4585, "Cannot read direntry s\n");
		return result;
	}
	if (stream.stream.entryType != 0xc0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4590, "%s: entryType[%#x] != expected(2)\n", "UVFAT_WriteStatDir_ex", stream.stream.entryType);
		SCE_DBG_PRINTF_LEVEL(2, 15, 4591, "%s: Not stream entry type found\n", "UVFAT_WriteStatDir_ex");
		return -32;
	}
#define STORE_STAT_SIZE(byteIndex) \
	stream.stream.validDataLength[byteIndex] = stat->Size >> ((byteIndex) * 8); \
	stream.stream.dataLength[byteIndex] = stat->Size >> ((byteIndex) * 8)
	STORE_STAT_SIZE(0); STORE_STAT_SIZE(1); STORE_STAT_SIZE(2); STORE_STAT_SIZE(3);
	STORE_STAT_SIZE(4); STORE_STAT_SIZE(5); STORE_STAT_SIZE(6); STORE_STAT_SIZE(7);
#undef STORE_STAT_SIZE
	stream.stream.firstCluster[0] = stat->StartCluster;
	stream.stream.firstCluster[1] = stat->StartCluster >> 8;
	stream.stream.firstCluster[2] = stat->StartCluster >> 16;
	stream.stream.firstCluster[3] = stat->StartCluster >> 24;
	stream.stream.generalSecondaryFlags = stat->generalSecondaryFlags;
	CHECKSUM_STAT_ENTRY(stream, 0);
	secondaryCount = primary.file.secondaryCount;
	remaining = secondaryCount - 2;
	if (secondaryCount != 1) {
		do {
			READ_STAT_ENTRY(vendor);
			if (result) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 4612, "%s: UVFAT_ReadDirEntry(3) failed(%d)\n", "UVFAT_WriteStatDir_ex", result);
				SCE_DBG_PRINTF_LEVEL(2, 15, 4613, "Cannot read vendor entry (%d)\n", remaining);
				return result;
			}
			if (vendor.file.entryType == 0x85 || !(vendor.file.entryType & 0x80)) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 4618, "%s: entryType[%#x] != expected(3)\n", "UVFAT_WriteStatDir_ex", vendor.file.entryType);
				SCE_DBG_PRINTF_LEVEL(2, 15, 4619, "Invalid entry type found(0x%x), filename and vendor\n", vendor.file.entryType);
				return -32;
			}
			CHECKSUM_STAT_ENTRY(vendor, 0);
		} while (remaining-- != 0);
	}
	primary.file.setChecksum[0] = checksum;
	primary.file.setChecksum[1] = checksum >> 8;
	SCE_DBG_PRINTF_LEVEL(2, 0, 4630,
		"%s: File.Position[%#llx], secondCount(%d), File.Cluster[%#lx], File.ClusterCount[%#lx]\n",
		"UVFAT_WriteStatDir_ex", fileSystem->file.Position, secondaryCount,
		fileSystem->file.Cluster, fileSystem->file.clusterIndex);
	index = 0;
	do {
		if ((SceUInt64)fileSystem->file.Position > 31)
			fileSystem->file.Position -= 32;
		index++;
	} while (secondaryCount >= index);
#if FW_DEBUG
	result = UVFAT_WriteDirEntry_ex(sharedResource, 0, &primary, 0, 0, fileSystem);
	if (result) {
#else
	result = writeDirectoryEntryBytes(sharedResource, fileSystem, &fileSystem->file, &primary, 0);
	if (result < 0) {
#endif
		SCE_DBG_PRINTF_LEVEL(1, 0, 4644, "%s: UVFAT_WriteDirEntry(1) failed(%d)\n", "UVFAT_WriteStatDir_ex", result);
		SCE_DBG_PRINTF_LEVEL(2, 15, 4645, "UVFAT_WriteDirEntry() fe Fail\n");
		return result;
	}
#if FW_DEBUG
	result = UVFAT_WriteDirEntry_ex(sharedResource, 0, &stream, 0, 0, fileSystem);
	if (result) {
#else
	result = writeDirectoryEntryBytes(sharedResource, fileSystem, &fileSystem->file, &stream, 0);
	if (result < 0) {
#endif
		SCE_DBG_PRINTF_LEVEL(1, 0, 4650, "%s: UVFAT_WriteDirEntry(2) failed(%d)\n", "UVFAT_WriteStatDir_ex", result);
		SCE_DBG_PRINTF_LEVEL(2, 15, 4651, "UVFAT_WriteDirEntry() se Fail\n");
		return result;
	}
	result = UVFAT_FlushFdBuf(&fileSystem->file, fileSystem);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4656, "%s: UVFAT_WriteDirFlush failed(%d)\n", "UVFAT_WriteStatDir_ex", result);
		SCE_DBG_PRINTF_LEVEL(2, 15, 4657, "UVFAT_WriteDirFlush() Fail\n");
	}
#undef CHECKSUM_STAT_ENTRY
#undef READ_STAT_ENTRY
	return result;
}

#if FW_VERSION >= 0x04000000
int writeTemporalDirectoryEntry(SceExfatfsSharedResource *sharedResource,
	const void *entry, unsigned int *writeResult, SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsDirectoryRecord empty;
	SceExfatfsFile *file = &fileSystem->file;
	int result;

	if (file->fnode->Stat.st_type != 6)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6972, "UVFAT_WriteDirEntry_ex",
			"%s: unexpected st_type(%d), fnode[%p], st_mode[%#o]\n",
			"UVFAT_WriteDirEntry_ex", file->fnode->Stat.st_type,
			file->fnode, file->fnode->Stat.st_mode);
	if (!entry) {
		empty = (SceExfatfsDirectoryRecord){ 0 };
		entry = &empty;
	}
	SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 6989, "UVFAT_WriteDirEntry_ex",
		"%s: fd(%d), fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], StartCluster[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], _clusterIndex[%#lx]\n",
		"UVFAT_WriteDirEntry_ex", 0, file->fnode, file->fnode->DirCluster,
		file->fnode->DirOffset, file->fnode->DirOffClust,
		file->fnode->Stat.StartCluster, file, file->Position,
		file->Cluster, file->clusterIndex);
	result = UVFAT_WriteData(sharedResource, fileSystem, file, entry, 32, 2, writeResult);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 6993, "UVFAT_WriteDirEntry_ex",
			"UVFAT_WriteData() Fail, er=%d\n", result);
	else
		result = 0;
	return result;
}
#endif

#if FW_DEBUG
int UVFAT_SearchFileFromDir(SceExfatfsSharedResource *sharedResource,
#else
int searchFileFromDirUppercase(SceExfatfsSharedResource *sharedResource,
#endif
	SceUInt16 *filename, SceExfatfsFileSystem *fileSystem, SceExfatfsFnode *fnode)
{
	SceExfatfsFatDirectoryEntry entry;
	SceExfatfsBootSector *boot;
	const unsigned char *rootEntries;
	SceUInt32 cluster;
	int result;

#if FW_DEBUG
	result = UVFAT_ToUpperBuf(filename, filename, 256, fileSystem);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 2644, "%s: toupper fail\n", "UVFAT_SearchFileFromDir");
		return result;
	}
	if (fnode->DirCluster == (SceUInt32)-2)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2648,
			"%s: unexpected DirCluster[%#lx], fnode[%p]\n",
			"UVFAT_SearchFileFromDir", fnode->DirCluster, fnode);
	SCE_DBG_PRINTF_LEVEL(2, 0, 2650,
		"%s(before search): StartCluster[%#lx], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx]\n",
		"UVFAT_SearchFileFromDir", fnode->Stat.StartCluster,
		fnode->DirCluster, fnode->DirOffset, fnode->DirOffClust);
#endif
	if (!fnode->Stat.StartCluster && fnode->Stat.st_type == 6) {
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
			fnode->Stat.StartCluster = _root_cluster(fileSystem);
			SCE_DBG_PRINTF_LEVEL(1, 0, 2655, "%s: cluster[%#x]->root_cluster[%#lx]\n",
				"UVFAT_SearchFileFromDir", 0, fnode->DirCluster);
		}
	}
	if (!fnode->DirCluster) {
		boot = fileSystem->PBRBuf;
		rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
		if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
			fnode->DirCluster = _root_cluster(fileSystem);
			SCE_DBG_PRINTF_LEVEL(1, 0, 2661, "%s: unexpected cluster[%#x]->root_cluster[%#lx]\n",
				"UVFAT_SearchFileFromDir", 0, fnode->DirCluster);
		}
	}
	if (fnode->Stat.StartCluster != fnode->DirCluster)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2671,
			"%s: StartCluster[%#lx], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx]\n",
			"UVFAT_SearchFileFromDir", fnode->Stat.StartCluster,
			fnode->DirCluster, fnode->DirOffset, fnode->DirOffClust);

/* Sony inlines release of the temporal file at each search exit. */
#define RELEASE_SEARCH_FILE() do { \
	if (fileSystem->file.mode == 3) fileSystem->file.mode = 0; \
	else SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6728, "_exfatfs_release_temporal_file", \
		"%s: unexpected mode(%d), assignName(%s)\n", \
		"_exfatfs_release_temporal_file", fileSystem->file.mode, fileSystem->drive->assignName); \
} while (0)
	if (fileSystem->fileSystemType == 2) {
		result = UVFAT_InitDirEntry_ex(sharedResource, 0, fnode->DirCluster,
			fileSystem, fnode->dirGeneralSecondaryFlags, fnode->dirSize);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2686, "%s: UVFAT_InitDirEntry_ex failed(%d)\n", "UVFAT_SearchFileFromDir", result);
			return result;
		}
		result = UVFAT_SearchGetFile_ex(0, sharedResource, filename, fileSystem, fnode, 0);
		SCE_DBG_PRINTF_LEVEL(2, 0, 2698,
			"%s(after search): result(%d), StartCluster[%#lx], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], File->DirCluster[%#lx], fnode[%p], File.fnode[%p], fnode->Size[%#llx], File.fnode->Size[%#llx]\n",
			"UVFAT_SearchFileFromDir", result, fnode->Stat.StartCluster,
			fnode->DirCluster, fnode->DirOffset, fnode->DirOffClust,
			fileSystem->file.fnode->DirCluster, fnode, fileSystem->file.fnode,
			fnode->Stat.Size, fileSystem->file.fnode->Stat.Size);
		if (result == -7)
			result = -4;
		else if (!result && fnode->DirCluster == fnode->Stat.StartCluster)
			_exfatfs_notify_directory_loop(fnode->fileSystem->drive,
				fnode->DirCluster, fnode->DirOffset, fnode->Stat.st_attr, fnode->Stat.Size);
		RELEASE_SEARCH_FILE();
		return result;
	}
	result = UVFAT_InitDirEntry_ex(sharedResource, 0, fnode->DirCluster, fileSystem, 0, 0);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2712, "%s: UVFAT_InitDirEntry failed(%d)\n", "UVFAT_SearchFileFromDir", result);
		return result;
	}
	result = _SearchFileFromDir(sharedResource, fileSystem, &entry, filename);
	if (result) {
		RELEASE_SEARCH_FILE();
		return result;
	}
	{
		SceUInt32 directoryCluster = fileSystem->file.Cluster;
		SceUInt64 directoryOffset = fileSystem->file.Position - 32;

#if FW_DEBUG
		fnode->DirOffClust = directoryCluster;
		fnode->DirOffset = directoryOffset;
#else
		fnode->DirOffset = directoryOffset;
		fnode->DirOffClust = directoryCluster;
#endif
	}
	if (fileSystem->file.fnode == fnode)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2733,
			"%s(after search): StartCluster[%#lx], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], File->DirCluster[%#lx], fnode[%p], File.fnode[%p]\n",
			"UVFAT_SearchFileFromDir", fnode->Stat.StartCluster, fnode->DirCluster,
			fnode->DirOffset, fnode->DirOffClust, fileSystem->file.fnode->DirCluster,
			fnode, fileSystem->file.fnode);
	fnode->DirCluster = fileSystem->file.fnode->DirCluster;
	RELEASE_SEARCH_FILE();
#undef RELEASE_SEARCH_FILE
	memset(&fnode->Stat, 0, sizeof(fnode->Stat));
	copyFatEntryAttributeToStat(&entry, &fnode->Stat, fileSystem);
	decodeFatDateTime(&fnode->Stat.ctime, entry.creationDate, entry.creationTime);
	decodeFatDateTime(&fnode->Stat.atime, entry.accessDate, 0);
	decodeFatDateTime(&fnode->Stat.mtime, entry.modificationDate, entry.modificationTime);
	fnode->Stat.Size = entry.size;
	cluster = entry.firstClusterLow;
	fnode->Stat.StartCluster = cluster;
	if (fileSystem->FATBits == 32) {
		cluster |= (unsigned int)entry.firstClusterHigh << 16;
		fnode->Stat.StartCluster = cluster;
	}
	if (fnode->DirCluster == cluster)
		_exfatfs_notify_directory_loop(fnode->fileSystem->drive, cluster,
			fnode->DirOffset, entry.attribute, fnode->Stat.Size);
	_initialize_cluster_cache_data(fnode);
#if FW_DEBUG
	_set_start_cluster_and_cluster_count(fnode, fnode->Stat.StartCluster, fnode->Stat.Size);
#else
	{
		SceUInt64 size = fnode->Stat.Size;
		SceUInt32 clusterSize;

		cluster = fnode->Stat.StartCluster;
		_set_start_cluster(fnode, cluster, fnode->Stat.st_type);
		if (fnode->clusterCount) {
			if (!size && fnode->Stat.st_type == 6) {
				SceExfatfsFileSystem *owner = fnode->fileSystem;
				if (cluster)
					size = clusterChainByteSize(fnode, cluster);
				else if (_root_cluster(owner))
					size = owner->sectorSize * owner->clusterSectorCount;
				else {
					boot = owner->PBRBuf;
					rootEntries = (const unsigned char *)&boot->fat32.parameters.rootEntryCount;
					size = 32 * (rootEntries[0] | ((unsigned int)rootEntries[1] << 8));
				}
				fnode->Stat.Size = size;
			}
			clusterSize = fnode->fileSystem->sectorSize * fnode->fileSystem->clusterSectorCount;
			fnode->clusterCount = (size - 1) / clusterSize + 1;
		}
	}
#endif
	return result;
}

#if !FW_DEBUG
int UVFAT_SearchFileFromDir(SceExfatfsSharedResource *sharedResource,
	SceUInt16 *filename, SceExfatfsFileSystem *fileSystem, SceExfatfsFnode *fnode)
{
	int result = UVFAT_ToUpperBuf(filename, filename, 256, fileSystem);

	if (!result)
		return searchFileFromDirUppercase(sharedResource, filename, fileSystem, fnode);
	return result;
}
#endif

int UVFAT_SearchFile(SceExfatfsSharedResource *sharedResource,
	SceFatfsNameset *nameset, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, const SceExfatfsFnode *parentFnode)
{
#if FW_DEBUG
	int isRoot = nameset->isRoot;
#endif

	if (parentFnode) {
#if FW_DEBUG
		if (isRoot)
			SCE_DBG_PRINTF_LEVEL(1, 0, 3039,
				"%s: unexpected is_root_dir(%d), parentFnode[%p]\n",
				"UVFAT_SearchFile", isRoot, parentFnode);
#endif
		{
			SceUInt32 directoryCluster = parentFnode->Stat.StartCluster;
			SceUInt32 directorySize = parentFnode->Stat.Size;
			unsigned int directoryFlags;

#if FW_DEBUG
			fnode->DirCluster = directoryCluster;
			fnode->DirOffClust = directoryCluster;
			directoryFlags = parentFnode->Stat.generalSecondaryFlags;
			fnode->Stat = parentFnode->Stat;
			fnode->DirOffset = 0;
			fnode->dirGeneralSecondaryFlags = directoryFlags;
			fnode->dirSize = directorySize;
#else
			directoryFlags = parentFnode->Stat.generalSecondaryFlags;
			fnode->Stat = parentFnode->Stat;
			fnode->dirSize = directorySize;
			fnode->DirCluster = directoryCluster;
			fnode->dirGeneralSecondaryFlags = directoryFlags;
			fnode->DirOffset = 0;
			fnode->DirOffClust = directoryCluster;
#endif
		}
#if FW_DEBUG
		SCE_DBG_PRINTF_LEVEL(1, 0, 3055, "%s: UVFAT_SearchFileFromDir\n", "UVFAT_SearchFile");
		return UVFAT_SearchFileFromDir(sharedResource, nameset->fileName, fileSystem, fnode);
#else
		{
			SceUInt16 *filename = nameset->fileName;
			int result = UVFAT_ToUpperBuf(filename, filename, 256, fileSystem);

			if (!result)
				return searchFileFromDirUppercase(sharedResource, filename, fileSystem, fnode);
			return result;
		}
#endif
	}
#if FW_DEBUG
	if (!isRoot)
		SCE_DBG_PRINTF_LEVEL(1, 0, 3059,
			"%s: unexpected is_root_dir(%d), parentFnode[%p]\n",
			"UVFAT_SearchFile", 0, parentFnode);
#endif
	return _get_root_fnode(fnode, fileSystem);
}

int _UVFAT_SearchFile_from_root(SceExfatfsSharedResource *sharedResource,
	SceFatfsNameset *nameset, SceExfatfsFileSystem **fileSystem,
	SceExfatfsFnode *fnode, SceExfatfsFnode *parentFnode)
{
	SceExfatfsFileSystem *current = *fileSystem;
	SceUInt16 *name, *destination;
	const SceUInt16 *path = nameset->filePath + 1;
	SceUInt32 cluster;
	int result, separator;

	if (!current || !current->state || !current->devname) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 2893, "%s:mountpoint is not set\n", "_UVFAT_SearchFile_from_root");
		return -15;
	}
	if (parentFnode) {
		if (parentFnode->fileSystem) {
			if (parentFnode->fileSystem->fileSystemType == 2)
				parentFnode->DirCluster = (SceUInt32)-2;
		} else {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2806, "_setup_parent_fnode_before_search",
				"%s: NULL fileSystem\n", "_setup_parent_fnode_before_search");
			current = *fileSystem;
		}
	}
	fnode->DirCluster = (SceUInt32)-2;
	cluster = _root_cluster(current);
	if (nameset->isRoot == 1)
		return _get_root_fnode(fnode, *fileSystem);
	name = allocateFatBuffer(512);
	if (!name) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2925, "No memory\n");
		SCE_DBG_PRINTF_LEVEL(1, 0, 2927, "%s: salloc failed\n", "_UVFAT_SearchFile_from_root");
		return -10;
	}
	fnode->DirCluster = cluster;
	fnode->dirSize = 0;
	fnode->dirGeneralSecondaryFlags = 0;
	for (;;) {
		destination = name;
#if FW_DEBUG
		for (;;) {
			unsigned int character = *path++;

			if (character == '/' || !character)
				break;
			*destination++ = character;
		}
		*destination = 0;
		separator = path[-1] == '/';
#else
		while (*path != '/' && *path)
			*destination++ = *path++;
		*destination = 0;
		separator = *path == '/';
		if (separator)
			path++;
#endif
#if FW_DEBUG
		result = UVFAT_SearchFileFromDir(sharedResource, name, *fileSystem, fnode);
#else
		current = *fileSystem;
		result = UVFAT_ToUpperBuf(name, name, 256, current);
		if (!result)
			result = searchFileFromDirUppercase(sharedResource, name, current, fnode);
#endif
		if (separator && result == -4) {
			result = -5;
			break;
		}
		if (result || !separator)
			break;
		if (fnode->Stat.st_type != 6) {
			result = -6;
			break;
		}
		if (parentFnode) {
			parentFnode->Stat = fnode->Stat;
			parentFnode->DirOffset = fnode->DirOffset;
			parentFnode->DirCluster = fnode->DirCluster;
			parentFnode->DirOffClust = fnode->DirOffClust;
			parentFnode->dirSize = fnode->dirSize;
			parentFnode->dirGeneralSecondaryFlags = fnode->dirGeneralSecondaryFlags;
		}
		fnode->DirCluster = fnode->Stat.StartCluster;
		fnode->dirSize = fnode->Stat.Size;
		fnode->dirGeneralSecondaryFlags = fnode->Stat.generalSecondaryFlags;
	}
	freeFatBuffer(name, 512);
	return result;
}

int UVFAT_LinkFromDir_ex(SceExfatfsSharedResource *sharedResource,
	const SceUInt16 *filename, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, SceExfatfsFnode *parentFnode)
{
	int result;

#define RELEASE_LINK_FILE() do { \
	if (fileSystem->file.mode == 3) \
		fileSystem->file.mode = 0; \
	else \
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6728, "_exfatfs_release_temporal_file", \
			"%s: unexpected mode(%d), assignName(%s)\n", \
			"_exfatfs_release_temporal_file", fileSystem->file.mode, \
			fileSystem->drive->assignName); \
} while (0)
#if FW_DEBUG
#define READ_LINK_ENTRY(entry) \
	(result = readTemporalDirectoryEntry(sharedResource, &(entry), 0, fileSystem))
#else
#define READ_LINK_ENTRY(entry) do { \
	if (!fileSystem->file.fnode->DirCluster && \
		!((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat32.parameters.rootEntryCount) \
		fileSystem->file.fnode->DirCluster = _root_cluster(fileSystem); \
	result = readDirectoryEntryBytes(sharedResource, fileSystem, &fileSystem->file, &(entry)); \
	if (result > 0) result = 0; \
	else if (!result) result = -7; \
} while (0)
#endif
	if (fileSystem->fileSystemType == 2) {
		unsigned char nameLength = fatNameLength(filename);
		unsigned int nameEntryCount = (nameLength + 14) / 15;
		unsigned int totalEntryCount = nameEntryCount + 2;
		unsigned int allocationSize = totalEntryCount * 32;
		SceExfatfsDirectoryRecord *records = allocateFatBuffer(allocationSize);
		SceExfatfsDirectoryRecord entry;
		unsigned int nameHash, entryCount, index, writeResult;
		unsigned int checksum = 0;
		int writeTerminator;

		if (!records) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 3594, "memory allocate fail\n");
			SCE_DBG_PRINTF_LEVEL(1, 0, 3596, "%s: salloc failed\n", "UVFAT_LinkFromDir_ex");
			return -10;
		}
		result = UVFAT_ToUpperBuf((SceUInt16 *)records, filename, nameLength, fileSystem);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 3606, "%s: toupper fail\n", "UVFAT_LinkFromDir_ex");
			goto free_records;
		}
		nameHash = checksumUtf16Name((const SceUInt16 *)records, nameLength);
		if (!fnode->DirCluster &&
			!((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat32.parameters.rootEntryCount) {
			fnode->DirCluster = _root_cluster(fileSystem);
			SCE_DBG_PRINTF_LEVEL(1, 0, 3617,
				"%s: unexpected cluster[%#x]->root_cluster[%#lx]\n",
				"UVFAT_LinkFromDir_ex", 0, fnode->DirCluster);
		}
		if (fnode->DirCluster != parentFnode->Stat.StartCluster) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 3627,
				"%s: DirCluster[%#lx] != StartCluster[%#lx]\n",
				"UVFAT_LinkFromDir_ex", fnode->DirCluster, parentFnode->Stat.StartCluster);
			fnode->DirCluster = parentFnode->Stat.StartCluster;
		}
		if (fnode->DirCluster == (SceUInt32)-2)
			SCE_DBG_PRINTF_LEVEL(1, 0, 3631, "%s: unexpected DirCluster[%#lx], fnode[%p]\n",
				"UVFAT_LinkFromDir_ex", fnode->DirCluster, fnode);
		result = UVFAT_InitDirEntry_ex(sharedResource, 0,
#if FW_DEBUG
			fnode->DirCluster,
#else
			parentFnode->Stat.StartCluster,
#endif
			fileSystem, parentFnode->Stat.generalSecondaryFlags, parentFnode->Stat.Size);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 3648, "%s: UVFAT_InitDirEntry_ex failed(%d)\n",
				"UVFAT_LinkFromDir_ex", result);
			return result;
		}
		entryCount = 0;
		for (;;) {
			if (!entryCount) {
				fnode->DirOffset = fileSystem->file.Position;
				fnode->DirCluster = fileSystem->file.fnode->DirCluster;
				fnode->DirOffClust = fileSystem->file.Cluster;
			}
#if FW_DEBUG
			result = readTemporalDirectoryEntry(sharedResource, 0, &entry, fileSystem);
#else
			READ_LINK_ENTRY(entry);
#endif
			if (result) {
				if (result != -7)
					goto free_records;
				writeTerminator = 0;
				break;
			}
			if (!entryCount && fnode->DirOffClust != fileSystem->file.Cluster)
				fnode->DirOffClust = fileSystem->file.Cluster;
			if (!entry.file.entryType) {
				entryCount++;
				writeTerminator = 1;
				break;
			}
			if (entry.file.entryType & 0x80) {
				entryCount = 0;
				continue;
			}
			if (++entryCount >= totalEntryCount) {
				writeTerminator = 0;
				break;
			}
		}
		SCE_DBG_PRINTF_LEVEL(2, 0, 3704,
			"%s: fnode->DirOffset[%#llx], File.Position[%#llx], entryCount(%d)\n",
			"UVFAT_LinkFromDir_ex", fnode->DirOffset, fileSystem->file.Position, entryCount);
		for (index = 0; index < entryCount; index++) {
			if ((SceUInt64)fileSystem->file.Position > 31)
				fileSystem->file.Position -= 32;
		}
		memset(records, 0, allocationSize);
		records[0].file.entryType = 0x85;
		records[0].file.secondaryCount = nameEntryCount + 1;
		records[0].file.creationUtcOffset = 0x80;
		records[0].file.modificationUtcOffset = 0x80;
		records[0].file.accessUtcOffset = 0x80;
		result = UVFAT_ChangeStatFileentry(fileSystem, &records[0].file, &fnode->Stat);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 3716, "MakeDirEntrySet Fail\n");
			goto free_records;
		}
#define CHECKSUM_LINK_ENTRY(record, skipChecksum) do { \
	const unsigned char *bytes = (const unsigned char *)&(record); \
	unsigned int byteIndex; \
	for (byteIndex = 0; byteIndex != sizeof(record); byteIndex++) { \
		if ((skipChecksum) && \
			(SceUInt16)(byteIndex - offsetof(SceExfatfsFileDirectoryEntry, setChecksum)) <= 1) \
			continue; \
		checksum = (SceUInt16)((checksum >> 1) + \
			((checksum & 1) ? 0x8000 : 0) + bytes[byteIndex]); \
	} \
} while (0)
		if (records[0].file.entryType == 0x85) {
			CHECKSUM_LINK_ENTRY(records[0], 1);
		} else {
			CHECKSUM_LINK_ENTRY(records[0], 0);
		}
		records[1].stream.entryType = 0xc0;
		records[1].stream.generalSecondaryFlags = fnode->Stat.generalSecondaryFlags;
		records[1].stream.nameLength = nameLength;
		records[1].stream.nameHash[0] = nameHash;
		records[1].stream.nameHash[1] = nameHash >> 8;
#define STORE_LINK_SIZE(byteIndex) \
	records[1].stream.dataLength[byteIndex] = fnode->Stat.Size >> ((byteIndex) * 8)
		STORE_LINK_SIZE(0); STORE_LINK_SIZE(1); STORE_LINK_SIZE(2); STORE_LINK_SIZE(3);
		STORE_LINK_SIZE(4); STORE_LINK_SIZE(5); STORE_LINK_SIZE(6); STORE_LINK_SIZE(7);
#undef STORE_LINK_SIZE
#define STORE_LINK_SIZE(byteIndex) \
	records[1].stream.validDataLength[byteIndex] = fnode->Stat.Size >> ((byteIndex) * 8)
		STORE_LINK_SIZE(0); STORE_LINK_SIZE(1); STORE_LINK_SIZE(2); STORE_LINK_SIZE(3);
		STORE_LINK_SIZE(4); STORE_LINK_SIZE(5); STORE_LINK_SIZE(6); STORE_LINK_SIZE(7);
#undef STORE_LINK_SIZE
		records[1].stream.firstCluster[0] = fnode->Stat.StartCluster;
		records[1].stream.firstCluster[1] = fnode->Stat.StartCluster >> 8;
		records[1].stream.firstCluster[2] = fnode->Stat.StartCluster >> 16;
		records[1].stream.firstCluster[3] = fnode->Stat.StartCluster >> 24;
		records[1].stream.generalSecondaryFlags = fnode->Stat.generalSecondaryFlags;
		CHECKSUM_LINK_ENTRY(records[1], 0);
		for (index = 0; index < nameEntryCount; index++) {
			SceExfatfsNameDirectoryEntry *nameEntry = &records[index + 2].filename;
			unsigned int nameIndex = index * 15;

			nameEntry->entryType = 0xc1;
			nameEntry->generalSecondaryFlags = 0;
#define STORE_LINK_NAME(character) do { \
	if (nameIndex + (character) >= nameLength) break; \
	nameEntry->name[character][0] = filename[nameIndex + (character)]; \
	nameEntry->name[character][1] = filename[nameIndex + (character)] >> 8; \
} while (0)
			STORE_LINK_NAME(0); STORE_LINK_NAME(1); STORE_LINK_NAME(2);
			STORE_LINK_NAME(3); STORE_LINK_NAME(4); STORE_LINK_NAME(5);
			STORE_LINK_NAME(6); STORE_LINK_NAME(7); STORE_LINK_NAME(8);
			STORE_LINK_NAME(9); STORE_LINK_NAME(10); STORE_LINK_NAME(11);
			STORE_LINK_NAME(12); STORE_LINK_NAME(13); STORE_LINK_NAME(14);
#undef STORE_LINK_NAME
			CHECKSUM_LINK_ENTRY(records[index + 2], 0);
		}
#undef CHECKSUM_LINK_ENTRY
		records[0].file.setChecksum[0] = checksum;
		records[0].file.setChecksum[1] = checksum >> 8;
		writeResult = 0;
		for (index = 0; index < totalEntryCount; index++) {
#if FW_DEBUG
			result = writeTemporalDirectoryEntry(sharedResource, &records[index], &writeResult, fileSystem);
			if (result)
#else
			result = writeDirectoryEntryBytes(sharedResource, fileSystem,
				&fileSystem->file, &records[index], (int *)&writeResult);
			if (result < 0)
#endif
				goto free_records;
		}
		if (writeTerminator) {
#if FW_DEBUG
			result = writeTemporalDirectoryEntry(sharedResource, 0, &writeResult, fileSystem);
			if (result)
#else
			entry = (SceExfatfsDirectoryRecord){ 0 };
			result = writeDirectoryEntryBytes(sharedResource, fileSystem,
				&fileSystem->file, &entry, (int *)&writeResult);
			if (result < 0)
#endif
				SCE_DBG_PRINTF_LEVEL(1, 0, 3737,
					"%s: UVFAT_WriteDirEntry(to check next unused entry) failed(%d)\n",
					"UVFAT_LinkFromDir_ex", result);
		}
		result = UVFAT_FlushFdBuf(&fileSystem->file, fileSystem);
		if (result)
			goto free_records;
		RELEASE_LINK_FILE();
		fnode->flags &= ~4;
		freeFatBuffer(records, allocationSize);
		if (writeResult) {
			parentFnode->flags |= 4;
			parentFnode->Stat.Size += (SceUInt64)writeResult *
				(fileSystem->sectorSize * fileSystem->clusterSectorCount);
			parentFnode->Stat.generalSecondaryFlags = fileSystem->file.fnode->dirGeneralSecondaryFlags;
		}
		if ((SceUInt64)fnode->dirSize != parentFnode->Stat.Size)
			fnode->dirSize = parentFnode->Stat.Size;
		if (fnode->dirGeneralSecondaryFlags != parentFnode->Stat.generalSecondaryFlags)
			fnode->dirGeneralSecondaryFlags = parentFnode->Stat.generalSecondaryFlags;
		if (writeResult && parentFnode->DirCluster != (SceUInt32)-2) {
			result = UVFAT_InitDirEntry_ex(sharedResource, 0, parentFnode->DirCluster,
				fileSystem, parentFnode->dirGeneralSecondaryFlags, parentFnode->dirSize);
			if (result < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 3801, "%s: UVFAT_InitDirEntry_ex failed(%d)\n",
					"UVFAT_LinkFromDir_ex", result);
				return result;
			}
			fileSystem->file.Position = parentFnode->DirOffset;
			result = UVFAT_WriteStatDir_ex(sharedResource, fileSystem, &parentFnode->Stat);
			RELEASE_LINK_FILE();
			if (result) {
				SCE_DBG_PRINTF_LEVEL(2, 15, 3814, "UVFAT_WriteStatDir_ex() Fail\n");
				return result;
			}
			parentFnode->flags &= ~4;
			result = flushFileSystemMetadata(fileSystem);
		}
		return result;

free_records:
		RELEASE_LINK_FILE();
		freeFatBuffer(records, allocationSize);
		return result;
	} else {
		unsigned int nameLength = fatNameLength(filename);
		unsigned char baseName[8] = { 0 };
		unsigned char extension[24] = { 0 };
		unsigned int flags = 0, baseLength, baseStart, firstDotIndex;
		unsigned int extensionStart, index, totalEntryCount;
		unsigned int numericTail, passStart;
		int sourceIndex = 0, checksum, writeTerminator;
		unsigned char entryCount;
		SceExfatfsFatDirectoryEntry entry;
		SceExfatfsFatLongDirectoryEntry longEntry;
#if !FW_DEBUG
		SceExfatfsDirectoryRecord empty;
#endif

		while (filename[sourceIndex] == '.' || filename[sourceIndex] == ' ') {
			sourceIndex++;
			flags = 1;
		}
		baseStart = sourceIndex;
		while (filename[sourceIndex] && filename[sourceIndex] != '.')
			sourceIndex++;
		firstDotIndex = sourceIndex;
		extensionStart = 0;
		while (filename[sourceIndex]) {
			while (filename[sourceIndex] == '.' || filename[sourceIndex] == ' ')
				sourceIndex++;
			if (!filename[sourceIndex])
				break;
			extensionStart = sourceIndex;
			while (filename[sourceIndex] && filename[sourceIndex] != '.')
				sourceIndex++;
		}
		if (extensionStart) {
			sourceIndex = extensionStart;
			if (extensionStart != firstDotIndex + 1)
				flags |= 1;
		}
		_setShortNameString(filename, extension, 3, &sourceIndex, &flags);
		if (filename[sourceIndex])
			flags |= 1;
		sourceIndex = baseStart;
		baseLength = _setShortNameString(filename, baseName, 8, &sourceIndex, &flags);
		if (baseName[0] == 0xe5)
			baseName[0] = 5;
		if (filename[sourceIndex] && filename[sourceIndex] != '.')
			flags |= 1;
		if (!flags && (!filename[sourceIndex] || filename[sourceIndex] == '.')) {
			checksum = -1;
		} else {
			if (flags & 1) {
				if (baseLength > 6)
					baseLength = 6;
				baseName[baseLength] = '~';
				baseName[baseLength + 1] = '1';
			}
#define SHORT_LINK_CHECKSUM() do { \
	checksum = baseName[0]; \
	SHORT_LINK_CHECKSUM_BYTE(baseName[1]); SHORT_LINK_CHECKSUM_BYTE(baseName[2]); \
	SHORT_LINK_CHECKSUM_BYTE(baseName[3]); SHORT_LINK_CHECKSUM_BYTE(baseName[4]); \
	SHORT_LINK_CHECKSUM_BYTE(baseName[5]); SHORT_LINK_CHECKSUM_BYTE(baseName[6]); \
	SHORT_LINK_CHECKSUM_BYTE(baseName[7]); SHORT_LINK_CHECKSUM_BYTE(extension[0]); \
	SHORT_LINK_CHECKSUM_BYTE(extension[1]); SHORT_LINK_CHECKSUM_BYTE(extension[2]); \
} while (0)
#define SHORT_LINK_CHECKSUM_BYTE(character) \
	checksum = (unsigned char)(((checksum >> 1) | (checksum << 7)) + (character))
			SHORT_LINK_CHECKSUM();
		}
		if (baseName[0] <= 0x20 && baseName[0] != 5)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3858, "UVFAT_LinkFromDir",
				"%s: invalid entry created by UVFAT_ConvShortName\n", "UVFAT_LinkFromDir");
		totalEntryCount = checksum == -1 ? 1 : (nameLength + 12) / 13 + 1;
		if (!fnode->DirCluster &&
			!((SceExfatfsBootSector *)fileSystem->PBRBuf)->fat32.parameters.rootEntryCount) {
			fnode->DirCluster = _root_cluster(fileSystem);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3876, "UVFAT_LinkFromDir",
				"%s: unexpected cluster[%#x]->root_cluster[%#lx]\n",
				"UVFAT_LinkFromDir", 0, fnode->DirCluster);
		}
		if (fnode->DirCluster != parentFnode->Stat.StartCluster) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3886, "UVFAT_LinkFromDir",
				"%s: DirCluster[%#lx] != StartCluster[%#lx]\n",
				"UVFAT_LinkFromDir", fnode->DirCluster, parentFnode->Stat.StartCluster);
			fnode->DirCluster = parentFnode->Stat.StartCluster;
		}
		if (fnode->DirCluster == (SceUInt32)-2)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3890, "UVFAT_LinkFromDir",
				"%s: unexpected DirCluster[%#lx], fnode[%p]\n", "UVFAT_LinkFromDir", fnode->DirCluster, fnode);
		if (checksum != -1) {
			numericTail = 1;
			for (;;) {
				result = UVFAT_InitDirEntry_ex(sharedResource, 0, fnode->DirCluster, fileSystem, 0, 0);
				if (result < 0) {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2289, "_check_numeric_tail",
						"%s: UVFAT_InitDirEntry failed(%d)\n", "_check_numeric_tail", result);
					if (result == -7)
						break;
					return result;
				}
				passStart = numericTail;
				for (;;) {
					unsigned char candidate[16], currentName[16];
					unsigned int length, digitCount, prefixLength, divisor;
					unsigned char digits[6];
					char spaces[10];

					for (;;) {
						READ_LINK_ENTRY(entry);
						if (result) {
							SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2240, "_exfatfs_search_next_short_name",
								"%s: UVFAT_ReadDirEntry failed(%d)\n", "_exfatfs_search_next_short_name", result);
							break;
						}
						if (!entry.name[0]) {
							result = -7;
							break;
						}
						if (entry.name[0] == 0xe5 || entry.attribute == 15 || (entry.attribute & 8))
							continue;
						if (entry.name[0] <= 0x20 && entry.name[0] != 5) {
							_exfatfs_notify_invalid_directory_entry(fileSystem->drive, &entry);
							continue;
						}
						if (entry.name[0] != '.')
							break;
						__builtin_memcpy(spaces, "         ", sizeof(spaces));
						if (entry.name[1] != ' ' && entry.name[1] != '.')
							break;
						if (memcmp(entry.name + 2, spaces, 9))
							break;
					}
					if (result)
						break;
					length = 0;
					for (index = 0; index < 8; index++) {
						if (entry.name[index] != ' ')
							candidate[length++] = entry.name[index];
					}
					if (entry.extension[0] != ' ') {
						candidate[length++] = '.';
						candidate[length++] = entry.extension[0];
					}
					if (entry.extension[1] != ' ')
						candidate[length++] = entry.extension[1];
					if (entry.extension[2] != ' ')
						candidate[length++] = entry.extension[2];
					candidate[length] = 0;
					length = 0;
					for (index = 0; index < 8; index++) {
						if (baseName[index] != ' ')
							currentName[length++] = baseName[index];
					}
					if (extension[0] != ' ') {
						currentName[length++] = '.';
						currentName[length++] = extension[0];
					}
					if (extension[1] != ' ')
						currentName[length++] = extension[1];
					if (extension[2] != ' ')
						currentName[length++] = extension[2];
					currentName[length] = 0;
					if (strncmp((const char *)candidate, (const char *)currentName, 16))
						continue;
					numericTail++;
					digitCount = numericTail < 10 ? 1 : numericTail < 100 ? 2 :
						numericTail < 1000 ? 3 : numericTail < 10000 ? 4 :
						numericTail < 100000 ? 5 : 6;
					divisor = numericTail;
					for (index = 0; index < 6; index++) {
						digits[index] = divisor % 10 + '0';
						divisor /= 10;
					}
					prefixLength = 7 - digitCount;
					for (index = 0; index < 8; index++) {
						if (baseName[index] == ' ' || baseName[index] == '~') {
							if (prefixLength > index)
								prefixLength = index;
							break;
						}
					}
					baseName[prefixLength] = '~';
					for (index = 0; index < digitCount; index++)
						baseName[prefixLength + index + 1] = digits[index];
					SHORT_LINK_CHECKSUM();
				}
				RELEASE_LINK_FILE();
				if (result != -7)
					return result;
				if (numericTail == passStart)
					break;
			}
		}
#undef SHORT_LINK_CHECKSUM
#undef SHORT_LINK_CHECKSUM_BYTE
		result = UVFAT_InitDirEntry_ex(sharedResource, 0, fnode->DirCluster, fileSystem, 0, 0);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3905, "UVFAT_LinkFromDir",
				"%s: UVFAT_InitDirEntry failed(%d)\n", "UVFAT_LinkFromDir", result);
			return result;
		}
		entryCount = 0;
		for (;;) {
#if FW_DEBUG
			result = readTemporalDirectoryEntry(sharedResource, 0, &entry, fileSystem);
#else
			READ_LINK_ENTRY(entry);
#endif
			if (result) {
				if (result != -7)
					goto release_fat_file;
				writeTerminator = 0;
				break;
			}
			if (!entry.name[0]) {
				entryCount++;
				writeTerminator = 1;
				break;
			}
			if (entry.name[0] == 0xe5) {
				if (++entryCount >= totalEntryCount) {
					writeTerminator = 0;
					break;
				}
			} else {
				entryCount = 0;
			}
		}
		for (index = 0; index < entryCount; index++) {
			if ((SceUInt64)fileSystem->file.Position > 31)
				fileSystem->file.Position -= 32;
		}
		if (totalEntryCount > 1) {
			unsigned int ordinal = totalEntryCount - 1;
			unsigned char first = 0x40;

			do {
				unsigned int nameIndex = (ordinal - 1) * 13;

				longEntry.sequence = first | ordinal;
				longEntry.attribute = 15;
				longEntry.type = 0;
				longEntry.checksum = checksum;
				longEntry.firstClusterLow = 0;
#define STORE_FAT_LINK_NAME(member, element, character) do { \
	SceUInt16 code = nameLength >= nameIndex + (character) ? filename[nameIndex + (character)] : 0xffff; \
	longEntry.member[element][0] = code; \
	longEntry.member[element][1] = code >> 8; \
} while (0)
				STORE_FAT_LINK_NAME(namePrefix, 0, 0); STORE_FAT_LINK_NAME(namePrefix, 1, 1);
				STORE_FAT_LINK_NAME(namePrefix, 2, 2); STORE_FAT_LINK_NAME(namePrefix, 3, 3);
				STORE_FAT_LINK_NAME(namePrefix, 4, 4); STORE_FAT_LINK_NAME(nameMiddle, 0, 5);
				STORE_FAT_LINK_NAME(nameMiddle, 1, 6); STORE_FAT_LINK_NAME(nameMiddle, 2, 7);
				STORE_FAT_LINK_NAME(nameMiddle, 3, 8); STORE_FAT_LINK_NAME(nameMiddle, 4, 9);
				STORE_FAT_LINK_NAME(nameMiddle, 5, 10); STORE_FAT_LINK_NAME(nameSuffix, 0, 11);
				STORE_FAT_LINK_NAME(nameSuffix, 1, 12);
#undef STORE_FAT_LINK_NAME
#if FW_DEBUG
				result = UVFAT_WriteDirEntry_ex(sharedResource, 0, 0, &longEntry, 0, fileSystem);
				if (result) {
#else
				result = writeDirectoryEntryBytes(sharedResource, fileSystem, &fileSystem->file, &longEntry, 0);
				if (result < 0) {
#endif
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3984, "UVFAT_LinkFromDir",
						"%s: UVFAT_WriteDirEntry failed(%d)\n", "UVFAT_LinkFromDir", result);
					goto release_fat_file;
				}
				first = 0;
			} while (--ordinal);
		}
		fnode->DirCluster = fileSystem->file.fnode->DirCluster;
		fnode->DirOffClust = fileSystem->file.Cluster;
		fnode->DirOffset = fileSystem->file.Position;
		__builtin_memcpy(entry.name, baseName, sizeof(baseName));
		__builtin_memcpy(entry.extension, extension, sizeof(entry.extension));
		entry.nameCase = 0;
		entry.creationDate = UVFAT_TimeToFATDate(&fnode->Stat.ctime);
		entry.creationTime = fatDateTimeToTime(&fnode->Stat.ctime);
		entry.accessDate = UVFAT_TimeToFATDate(&fnode->Stat.atime);
		entry.firstClusterHigh = 0;
		copyStatAttributeToFatEntry(&fnode->Stat, &entry, fileSystem);
		entry.modificationDate = UVFAT_TimeToFATDate(&fnode->Stat.mtime);
		entry.modificationTime = fatDateTimeToTime(&fnode->Stat.mtime);
		if (fnode->Stat.st_type == 6) {
			if (fnode->Stat.StartCluster == fileSystem->firstClusterOfRoot) {
				entry.firstClusterLow = 0;
			} else {
				entry.firstClusterLow = fnode->Stat.StartCluster;
				if (fileSystem->FATBits == 32)
					entry.firstClusterHigh = fnode->Stat.StartCluster >> 16;
			}
			entry.size = 0;
		} else {
			entry.firstClusterLow = fnode->Stat.StartCluster;
			if (fileSystem->FATBits == 32)
				entry.firstClusterHigh = fnode->Stat.StartCluster >> 16;
			entry.size = fnode->Stat.Size;
		}
#if FW_DEBUG
		result = UVFAT_WriteDirEntry_ex(sharedResource, 0, &entry, 0, 0, fileSystem);
		if (result) {
#else
		result = writeDirectoryEntryBytes(sharedResource, fileSystem, &fileSystem->file, &entry, 0);
		if (result < 0) {
#endif
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4054, "UVFAT_LinkFromDir",
				"%s: UVFAT_WriteDirEntry(2) failed(%d)\n", "UVFAT_LinkFromDir", result);
			goto release_fat_file;
		}
		if (fnode->DirOffClust != fileSystem->file.Cluster)
			fnode->DirOffClust = fileSystem->file.Cluster;
		if (writeTerminator == 1) {
#if FW_DEBUG
			result = UVFAT_WriteDirEntry_ex(sharedResource, 0, 0, 0, 0, fileSystem);
			if (result)
#else
			empty = (SceExfatfsDirectoryRecord){ 0 };
			result = writeDirectoryEntryBytes(sharedResource, fileSystem, &fileSystem->file, &empty, 0);
			if (result < 0)
#endif
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4075, "UVFAT_LinkFromDir",
					"%s: UVFAT_WriteDirEntry(to check next unused entry) failed(%d)\n",
					"UVFAT_LinkFromDir", result);
		}
		result = UVFAT_FlushFdBuf(&fileSystem->file, fileSystem);
		RELEASE_LINK_FILE();
		if (!result)
			fnode->flags &= ~4;
		return result;

release_fat_file:
		RELEASE_LINK_FILE();
		return result;
	}
#undef READ_LINK_ENTRY
#undef RELEASE_LINK_FILE
}

int UVFAT_UnLinkFromDir(SceExfatfsSharedResource *sharedResource,
	const SceUInt16 *filename, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode)
{
	SceUInt16 *name = allocateFatBuffer(512);
	SceExfatfsFatDirectoryEntry entry;
	SceExfatfsFatLongDirectoryEntry longEntry;
	SceExfatfsDirectoryRecord primary;
	int result;
	unsigned int secondaryCount;
	unsigned char sequence;

#define RELEASE_UNLINK_FILE() do { \
	if (fileSystem->file.mode == 3) \
		fileSystem->file.mode = 0; \
	else \
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6728, "_exfatfs_release_temporal_file", \
			"%s: unexpected mode(%d), assignName(%s)\n", \
			"_exfatfs_release_temporal_file", fileSystem->file.mode, \
			fileSystem->drive->assignName); \
} while (0)
#if FW_DEBUG
#define READ_UNLINK_ENTRY(entry, secondary) \
	(result = readTemporalDirectoryEntry(sharedResource, (secondary) ? 0 : &(entry), \
		(secondary) ? &(entry) : 0, fileSystem))
#define WRITE_UNLINK_ENTRY(entry, secondary) \
	(result = UVFAT_WriteDirEntry_ex(sharedResource, 0, (secondary) ? 0 : &(entry), \
		(secondary) ? &(entry) : 0, 0, fileSystem))
#else
#define READ_UNLINK_ENTRY(entry, secondary) do { \
	if (!fileSystem->file.fnode->DirCluster) { \
		SceExfatfsBootSector *boot = fileSystem->PBRBuf; \
		const unsigned char *rootEntries = \
			(const unsigned char *)&boot->fat32.parameters.rootEntryCount; \
		if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) \
			fileSystem->file.fnode->DirCluster = _root_cluster(fileSystem); \
	} \
	result = readDirectoryEntryBytes(sharedResource, fileSystem, &fileSystem->file, &(entry)); \
	if (result > 0) result = 0; \
	else if (!result) result = -7; \
} while (0)
#define WRITE_UNLINK_ENTRY(entry, secondary) do { \
	result = writeDirectoryEntryBytes(sharedResource, fileSystem, &fileSystem->file, &(entry), 0); \
	if (result >= 0) result = 0; \
} while (0)
#endif
	if (!name) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 4168, "%s: No memory\n", "UVFAT_UnLinkFromDir");
		SCE_DBG_PRINTF_LEVEL(1, 0, 4170, "%s: salloc failed\n", "UVFAT_UnLinkFromDir");
		return -10;
	}
	fatNameCopyBounded(name, filename, 256);
	result = UVFAT_ToUpperBuf(name, name, 256, fileSystem);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 4179, "%s: toupper fail\n", "UVFAT_UnLinkFromDir");
		goto free_name;
	}
	if (!fnode->DirCluster) {
		SceExfatfsBootSector *boot = fileSystem->PBRBuf;
		const unsigned char *rootEntries =
			(const unsigned char *)&boot->fat32.parameters.rootEntryCount;

		if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
			fnode->DirCluster = _root_cluster(fileSystem);
			SCE_DBG_PRINTF_LEVEL(1, 0, 4188,
				"%s: unexpected cluster[%#x]->root_cluster[%#lx]\n",
				"UVFAT_UnLinkFromDir", 0, fnode->DirCluster);
		}
	}
	if (fnode->DirCluster == (SceUInt32)-2)
		SCE_DBG_PRINTF_LEVEL(1, 0, 4198,
			"%s: unexpected DirCluster[%#lx], fnode[%p]\n",
			"UVFAT_UnLinkFromDir", fnode->DirCluster, fnode);
	if (fileSystem->fileSystemType == 2) {
		result = UVFAT_InitDirEntry_ex(sharedResource, 0, fnode->DirCluster,
			fileSystem, fnode->dirGeneralSecondaryFlags, fnode->dirSize);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 4205, "%s: UVFAT_InitDirEntry_ex failed(%d)\n",
				"UVFAT_UnLinkFromDir", result);
			goto free_name;
		}
		result = UVFAT_SearchGetFile_ex(0, sharedResource, name, fileSystem, 0, 0);
		if (result) {
			if (result == -7)
				result = -4;
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 4108, "UVFAT_RemoveEntry_ex",
				"%s: UVFAT_SearchGetFile_ex() Fail(%ls)\n", "UVFAT_RemoveEntry_ex", name);
			goto release_and_free;
		}
		READ_UNLINK_ENTRY(primary, 0);
		if (result)
			goto release_and_free;
		if (!primary.file.entryType) {
			result = -7;
			goto release_and_free;
		}
		if (primary.file.entryType != 0x85) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 4122, "UVFAT_RemoveEntry_ex",
				"%s: Not start point of direntry set\n", "UVFAT_RemoveEntry_ex");
			result = -32;
			goto release_and_free;
		}
		secondaryCount = primary.file.secondaryCount;
		if ((SceUInt64)fileSystem->file.Position >= 32)
			fileSystem->file.Position -= 32;
		primary.file.entryType = 5;
		WRITE_UNLINK_ENTRY(primary, 0);
		if (result)
			goto release_and_free;
		primary.file.entryType = 0x40;
		while (secondaryCount) {
			secondaryCount--;
			WRITE_UNLINK_ENTRY(primary, 0);
			if (result) {
				UVFAT_FlushFdBuf(&fileSystem->file, fileSystem);
				goto release_and_free;
			}
		}
		result = UVFAT_FlushFdBuf(&fileSystem->file, fileSystem);
		if (!result)
			fnode->flags &= ~4;
		goto release_and_free;
	}
	result = UVFAT_InitDirEntry_ex(sharedResource, 0, fnode->DirCluster, fileSystem, 0, 0);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4216, "%s: UVFAT_InitDirEntry failed(%d)\n",
			"UVFAT_UnLinkFromDir", result);
		goto free_name;
	}
	result = _SearchFileFromDir(sharedResource, fileSystem, &entry, name);
	if (result)
		goto release_and_free;
	freeFatBuffer(name, 512);
	entry.name[0] = 0xe5;
	if ((SceUInt64)fileSystem->file.Position >= 32)
		fileSystem->file.Position -= 32;
	WRITE_UNLINK_ENTRY(entry, 0);
	if (result)
		goto release_file;
	if ((SceUInt64)fileSystem->file.Position >= 32)
		fileSystem->file.Position -= 32;
	for (;;) {
		if ((SceUInt64)fileSystem->file.Position > 31)
			fileSystem->file.Position -= 32;
		READ_UNLINK_ENTRY(longEntry, 1);
		if (result) {
			if (result == -7)
				result = -4;
			goto release_file;
		}
		if ((SceUInt64)fileSystem->file.Position > 31)
			fileSystem->file.Position -= 32;
		if (longEntry.attribute != 15)
			break;
		sequence = longEntry.sequence;
		longEntry.sequence = 0xe5;
		WRITE_UNLINK_ENTRY(longEntry, 1);
		if (result)
			goto release_file;
		if ((SceUInt64)fileSystem->file.Position > 31)
			fileSystem->file.Position -= 32;
		if (sequence & 0x40)
			break;
	}
	result = UVFAT_FlushFdBuf(&fileSystem->file, fileSystem);
release_file:
	RELEASE_UNLINK_FILE();
	return result;
release_and_free:
	RELEASE_UNLINK_FILE();
free_name:
	freeFatBuffer(name, 512);
#undef WRITE_UNLINK_ENTRY
#undef READ_UNLINK_ENTRY
#undef RELEASE_UNLINK_FILE
	return result;
}

int UVFAT_InitDirEntry(SceExfatfsSharedResource *sharedResource, int fd,
	SceUInt32 dirCluster, SceExfatfsFileSystem *fileSystem)
{
	return UVFAT_InitDirEntry_ex(sharedResource, fd, dirCluster, fileSystem, 0, 0);
}

int UVFAT_InitFd(SceExfatfsSharedResource *sharedResource, int fd,
	SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsFile *file = &fileSystem->file;

	if (fd)
		file = &sharedResource->files[fd];
	resetFilePosition(file);
	file->cacheFlags = 0;
	file->cacheIndex = (SceUInt64)-1;
	file->cacheCluster = g_codepageTablesEnabled ? (SceUInt32)-2 : 0;
	return 0;
}

int UVFAT_ReadDirEntry(SceExfatfsSharedResource *sharedResource, int fd,
	void *primary, void *secondary, SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsFile *file;
	void *entry;
	int result;

	if (fd) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6881, "%s: unexpected fd(%d)\n", "UVFAT_ReadDirEntry", fd);
		file = &sharedResource->files[fd];
		if (!file->fnode->Stat.StartCluster) {
			SceExfatfsBootSector *boot = fileSystem->PBRBuf;
			const unsigned char *rootEntries =
				(const unsigned char *)&boot->fat32.parameters.rootEntryCount;

			if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 6887, "%s: unexpected StartCluster[%#lx]\n", "UVFAT_ReadDirEntry", 0);
				file->fnode->Stat.StartCluster = _root_cluster(fileSystem);
			}
		}
	} else {
		file = &fileSystem->file;
		if (!file->fnode->DirCluster) {
			SceExfatfsBootSector *boot = fileSystem->PBRBuf;
			const unsigned char *rootEntries =
				(const unsigned char *)&boot->fat32.parameters.rootEntryCount;

			if (!(rootEntries[0] | ((unsigned int)rootEntries[1] << 8))) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 6870, "%s: unexpected StartCluster[%#lx]\n",
					"UVFAT_ReadDirEntry", file->fnode->Stat.StartCluster);
				file->fnode->DirCluster = _root_cluster(fileSystem);
			}
		}
	}
	if (primary) {
		entry = primary;
	} else if (secondary) {
		entry = secondary;
	} else {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6908, "%s: NULL buffer\n", "UVFAT_ReadDirEntry");
		return -32;
	}
	SCE_DBG_PRINTF_LEVEL(2, 0, 6919,
		"%s: fd(%d), fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], StartCluster[%#lx], filep[%p], Position[%#llx], Cluster[%#lx], _clusterIndex[%#lx]\n",
		"UVFAT_ReadDirEntry", fd, file->fnode, file->fnode->DirCluster,
		file->fnode->DirOffset, file->fnode->DirOffClust, file->fnode->Stat.StartCluster,
		file, file->Position, file->Cluster, file->clusterIndex);
	result = readDirectoryEntryBytes(sharedResource, fileSystem, file, entry);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 6923, "UVFAT_ReadData() Fail, er=%d\n", result);
		SCE_DBG_PRINTF_LEVEL(1, 0, 6924, "%s: UVFAT_ReadData failed[%#lx]\n",
			"UVFAT_ReadDirEntry", result);
		return result;
	}
	if (!result)
		return -7;
	if (secondary && primary)
		*(SceExfatfsDirectoryRecord *)secondary = *(const SceExfatfsDirectoryRecord *)primary;
	return 0;
}
