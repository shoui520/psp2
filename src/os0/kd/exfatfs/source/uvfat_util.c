#include <psp2kern/types.h>
#include <string.h>
#include <sce/debug.h>
#include "exfatfs_internal.h"

extern SceExfatfsFnode *UVFAT_GetParentFnode(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *path);
extern int UVFAT_ReleaseFnode(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *fnode);
extern void _print_fnode(SceExfatfsFnode *fnode, const char *caller);
extern int UVFAT_SearchFile(SceExfatfsSharedResource *sharedResource,
	SceFatfsNameset *nameset, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, int search);
extern int _UVFAT_SearchFile_from_root(SceExfatfsSharedResource *sharedResource,
	SceFatfsNameset *nameset, SceExfatfsFileSystem **fileSystem,
	SceExfatfsFnode *fnode, SceExfatfsFnode *parent);

void _check_parent_directory_data(SceExfatfsFnode *fnode,
	SceExfatfsSharedResource *sharedResource)
{
	SceExfatfsFnode *parent, *temporary;
	SceFatfsNameset nameset;
	SceUInt64 size;
	int result;

	if (fnode->FilePath[0] != '/') {
		SCE_DBG_PRINTF_LEVEL(1, 0, 482,
			"%s: fnode->FilePath[0] != '/', [%#x]\n",
			"_check_parent_directory_data", fnode->FilePath[0]);
		goto no_parent;
	}
	if (!fnode->FilePath[1]) {
		if (fnode->DirCluster == (SceUInt32)-2)
			return;
		SCE_DBG_PRINTF_LEVEL(1, 0, 485,
			"%s: fnode->FilePath[1] == 0, root_dir, fnode->DirCluster[%#lx]\n",
			"_check_parent_directory_data", fnode->DirCluster);
		goto no_parent;
	}
	parent = UVFAT_GetParentFnode(sharedResource, fnode->fileSystem, fnode->FilePath);
	if (!parent)
		goto no_parent;
	if (parent->refCount == 1) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 497,
			"%s: initialization for parentFnode is necessary, Bug 34089\n",
			"_check_parent_directory_data");
		_print_fnode(fnode, "_check_parent_directory_data");
		if (parent->refCount == 1) {
			memcpy(nameset.filePath, parent->FilePath, sizeof(parent->FilePath));
			if (parent->FilePath[0] != '/') {
				SCE_DBG_PRINTF_LEVEL(1, 0, 514,
					"%s: FilePath[0] != '/', [%#x]\n",
					"_check_parent_directory_data", parent->FilePath[0]);
				result = -32;
				goto search_error;
			}
			if (parent->FilePath[1]) {
				nameset.isRoot = 0;
				temporary = UVFAT_GetParentFnode(sharedResource,
					fnode->fileSystem, parent->FilePath);
				if (!temporary) {
					SCE_DBG_PRINTF_LEVEL(1, 0, 533, "%s: NULL temp\n",
						"_check_parent_directory_data");
					result = -22;
					goto search_error;
				}
				if (temporary->refCount == 1)
					result = _UVFAT_SearchFile_from_root(sharedResource, &nameset,
						&fnode->fileSystem, parent, temporary);
				else
					result = UVFAT_SearchFile(sharedResource, &nameset,
						fnode->fileSystem, parent, (int)temporary);
				UVFAT_ReleaseFnode(sharedResource, temporary);
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0, 519, "%s: FilePath[1] == 0, root_dir\n",
					"_check_parent_directory_data");
				nameset.isRoot = 1;
				result = UVFAT_SearchFile(sharedResource, &nameset,
					fnode->fileSystem, parent, 0);
			}
			if (result)
				goto search_error;
		}
	}
#if FW_VERSION < 0x04000000
	size = (SceUInt64)parent->Stat.Size;
#endif
	if (fnode->DirCluster != parent->Stat.StartCluster) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 551,
			"%s: DirCluster[%#lx] != cached[%#lx], shouldn't occur\n",
			"_check_parent_directory_data", fnode->DirCluster, parent->Stat.StartCluster);
		fnode->DirCluster = parent->Stat.StartCluster;
	}
	if (fnode->dirGeneralSecondaryFlags != parent->Stat.generalSecondaryFlags) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 555,
			"%s: dirGeneralSecondaryFlags[%#x] != cachedFlags[%#x], can occur when parent directory is extended\n",
			"_check_parent_directory_data", fnode->dirGeneralSecondaryFlags,
			parent->Stat.generalSecondaryFlags);
		fnode->dirGeneralSecondaryFlags = parent->Stat.generalSecondaryFlags;
	}
#if FW_VERSION >= 0x04000000
	size = (SceUInt64)parent->Stat.Size;
#endif
	if ((SceUInt64)fnode->dirSize != size) {
		if (fnode->dirSize)
			SCE_DBG_PRINTF_LEVEL(1, 0, 560,
				"%s: dirSize[%#x] != cachedSize[%#llx], can occur when parent directory is extended\n",
				"_check_parent_directory_data", fnode->dirSize, size);
		else
			SCE_DBG_PRINTF_LEVEL(1, 0, 562,
				"%s: uninitialized dirSize[%#x] != cachedSize[%#llx]\n",
				"_check_parent_directory_data", 0, size);
		fnode->dirSize = parent->Stat.Size;
	}
	UVFAT_ReleaseFnode(sharedResource, parent);
	return;

search_error:
	SCE_DBG_PRINTF_LEVEL(1, 0, 541, "%s: UVFAT_SearchFile failed(%d)\n",
		"_check_parent_directory_data", result);
	UVFAT_ReleaseFnode(sharedResource, parent);
	return;
no_parent:
	if (fnode->DirCluster != (SceUInt32)-2)
		SCE_DBG_PRINTF_LEVEL(1, 0, 492, "%s: no parentFnode\n",
			"_check_parent_directory_data");
}

extern SceUInt32 fileSystemClusterSize(SceExfatfsFileSystem *fileSystem);
extern SceUInt32 contiguousClusterAtIndex(SceExfatfsFileSystem *fileSystem,
	SceUInt32 start, SceUInt32 index, SceUInt64 size);
extern SceUInt32 nextContiguousCluster(SceExfatfsFileSystem *fileSystem,
	SceExfatfsStat *stat, SceUInt32 cluster);
extern SceUInt32 UVFAT_NextCluster(SceExfatfsSharedResource *sharedResource,
	SceUInt32 cluster, SceExfatfsFileSystem *fileSystem);
extern void _exfatfs_notify_broken_cluster_chain(SceExfatfsDrive *drive,
	SceUInt32 cluster, SceUInt32 next);
extern SceExfatfsFile *_exfatfs_open_common(SceExfatfsFnode *fnode, int mode,
	SceExfatfsSharedResource *sharedResource, int *error);
extern int UVFAT_WriteData(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFile *file,
	const void *buffer, unsigned int size, int mode, void *result);
extern int UVFAT_FlushFdBuf(SceExfatfsFile *file, SceExfatfsFileSystem *fileSystem);
extern void _ReleaseFp(SceExfatfsSharedResource *sharedResource, SceExfatfsFile *file);
extern int _exfatfs_flush_metadata(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *fnode);
extern int flushFileSystemMetadata(SceExfatfsFileSystem *fileSystem);
extern int UVFAT_SetEOF(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster);
extern int UVFAT_FreeClusterLink(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceUInt32 cluster);
extern int UVFAT_FreeClusterLink_ex_NFC(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsStat *stat, SceUInt32 cluster);
extern void _invalidate_cluster_cache_data(SceExfatfsFnode *fnode);

int _Update_FAT_Size(SceExfatfsFnode *fnode, SceExfatfsStat *stat,
	unsigned int updateTime, SceExfatfsSharedResource *sharedResource)
{
	SceExfatfsFileSystem *originalFileSystem = fnode->fileSystem;
	unsigned char originalFlags = fnode->Stat.generalSecondaryFlags;
	SceUInt32 originalStartCluster = fnode->Stat.StartCluster;
	SceUInt64 originalSize = fnode->Stat.Size;
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsFile *file;
	SceExfatfsStat saved;
	SceUInt64 currentSize, newSize, position, requestSize, clusterCount;
	SceUInt32 clusterSize, count, next, freeFirst = -1, lastCluster = -2;
	int error, result, written;

	_check_parent_directory_data(fnode, sharedResource);
	currentSize = fnode->Stat.Size;
	newSize = stat->Size;
	if (currentSize != newSize) {
		fileSystem = fnode->fileSystem;
		error = 0;
		clusterSize = fileSystemClusterSize(fileSystem);
		if (currentSize < newSize) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 587,
				"%s: extend file, %llu -> %llu\n",
				"_Update_FAT_Size", currentSize, newSize);
			if (newSize >= 0x100000000ULL && fileSystem->fileSystemType != 2) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 591,
					"%s: newSize[%#llx] > file size limit of FAT, type(%d)\n",
					"_Update_FAT_Size", newSize, fileSystem->fileSystemType);
				result = -32;
				goto error_flush;
			}
			file = _exfatfs_open_common(fnode, 8, sharedResource, &error);
			if (!file) {
				result = error == (int)0x8001000c ? -49 :
					error == (int)0x80010017 ? -22 : -32;
				goto error_flush;
			}
			file->Position = currentSize;
			position = currentSize;
			do {
				requestSize = newSize - position;
				if (requestSize > 0x40000000)
					requestSize = 0x40000000;
				written = UVFAT_WriteData(sharedResource, fileSystem,
					file, NULL, requestSize, 257, NULL);
				if (written < 0) {
					SCE_DBG_PRINTF_LEVEL(1, 0, 636,
						"%s: UVFAT_WriteData failed(%ld), position[%#llx], Size[%#llx]\n",
						"_Update_FAT_Size", written, position, fnode->Stat.Size);
					error = written;
					break;
				}
				if ((SceUInt64)(SceOff)written != requestSize)
					SCE_DBG_PRINTF_LEVEL(1, 0, 640,
						"%s: writeSize[%#lx] != request_size[%#llx]\n",
						"_Update_FAT_Size", written, requestSize);
				position += (SceOff)written;
			} while (position < newSize);
			if (!error) {
				error = UVFAT_FlushFdBuf(file, fileSystem);
				if (error)
					SCE_DBG_PRINTF_LEVEL(1, 0, 650,
						"%s: UVFAT_FlushFdBuf failed(%d)\n", "_Update_FAT_Size", error);
			}
			_ReleaseFp(sharedResource, file);
			if (error) {
				flushFileSystemMetadata(fileSystem);
				result = error;
				if (result)
					goto error_flush;
			} else {
				fnode->Stat.Size = newSize;
			}
		} else if (newSize < currentSize) {
			if (!newSize) {
				freeFirst = fnode->Stat.StartCluster;
				if (fileSystem->fileSystemType == 2)
					fnode->Stat.generalSecondaryFlags &= ~2;
				fnode->Stat.Size = 0;
				lastCluster = -1;
				fnode->Stat.StartCluster = 0;
				fnode->flags |= 4;
			} else {
				lastCluster = fnode->Stat.StartCluster;
				if (lastCluster != fileSystem->firstClusterOfRoot &&
				    fileSystem->fileSystemType == 2 &&
				    (fnode->Stat.generalSecondaryFlags & 2)) {
					lastCluster = contiguousClusterAtIndex(fileSystem, lastCluster,
						(newSize - 1) / clusterSize, fnode->Stat.Size);
					if (lastCluster >= fileSystem->ClusterMAX || !lastCluster) {
						SCE_DBG_PRINTF_LEVEL(2, 15, 692, "Not enough FAT chain, NFC\n");
						result = -15;
						goto error_flush;
					}
					freeFirst = nextContiguousCluster(fileSystem, &fnode->Stat, lastCluster);
				} else {
					clusterCount = (newSize - 1) / clusterSize;
					count = 0;
					while ((SceUInt64)count < clusterCount) {
						next = UVFAT_NextCluster(sharedResource, lastCluster, fileSystem);
						if (next >= fileSystem->ClusterMAX || !next) {
							SCE_DBG_PRINTF_LEVEL(2, 15, 705, "Not enough FAT chain\n");
							_exfatfs_notify_broken_cluster_chain(fnode->fileSystem->drive,
								lastCluster, next);
							result = -15;
							goto error_flush;
						}
						++count;
						lastCluster = next;
					}
					freeFirst = UVFAT_NextCluster(sharedResource, lastCluster, fileSystem);
				}
				fnode->Stat.Size = newSize;
				fnode->flags |= 4;
			}
		}
	}
	if (memcmp(stat, &fnode->Stat, sizeof(*stat))) {
		fnode->flags |= 4;
		if (stat->st_dev != fnode->Stat.st_dev)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 828, "UVFAT_SetStat_Core",
				"%s: change st_dev(%d)->(%d)\n", "UVFAT_SetStat_Core",
				fnode->Stat.st_dev, stat->st_dev);
		if (stat->st_uid != fnode->Stat.st_uid)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 831, "UVFAT_SetStat_Core",
				"%s: change st_uid(%d)->(%d)\n", "UVFAT_SetStat_Core",
				fnode->Stat.st_uid, stat->st_uid);
		if (stat->st_type != fnode->Stat.st_type)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 834, "UVFAT_SetStat_Core",
				"%s: change st_type(%d)->(%d)\n", "UVFAT_SetStat_Core",
				fnode->Stat.st_type, stat->st_type);
		fnode->Stat.st_type = stat->st_type;
		fnode->Stat.st_dev = stat->st_dev;
		fnode->Stat.st_uid = stat->st_uid;
		fnode->Stat.ctime = stat->ctime;
		fnode->Stat.atime = stat->atime;
		if (updateTime)
			fnode->Stat.mtime = stat->mtime;
		if (fnode->Stat.st_attr != stat->st_attr)
			fnode->Stat.st_attr = stat->st_attr;
		fnode->Stat.st_mode = stat->st_mode;
	}
	if (fnode->flags & 4) {
		result = _exfatfs_flush_metadata(sharedResource, fnode);
		if (result) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 864, "UVFAT_SetStat_Core",
				"%s: _exfatfs_flush_metadata failed(%d)\n", "UVFAT_SetStat_Core", result);
			return result;
		}
		fnode->flags &= ~4;
	}
	if (originalFileSystem->ClusterMAX > freeFirst && freeFirst > 1) {
		if (originalFileSystem->fileSystemType == 2 && (originalFlags & 2)) {
			saved.Size = originalSize;
			saved.StartCluster = originalStartCluster;
			UVFAT_FreeClusterLink_ex_NFC(sharedResource, originalFileSystem, &saved, freeFirst);
		} else {
			if (originalFileSystem->ClusterMAX > lastCluster) {
				result = UVFAT_SetEOF(originalFileSystem, lastCluster);
				if (result) {
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 891, "UVFAT_SetStat_Core", "Cannot set EOF\n");
					return result;
				}
			}
			UVFAT_FreeClusterLink(sharedResource, originalFileSystem, freeFirst);
		}
		_invalidate_cluster_cache_data(fnode);
	}
	return flushFileSystemMetadata(originalFileSystem);

error_flush:
	SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 815, "UVFAT_SetStat_Core",
		"%s: metadata_modified[%#x], Size[%#llx]\n", "UVFAT_SetStat_Core",
		fnode->flags & 4, fnode->Stat.Size);
	if (!_exfatfs_flush_metadata(sharedResource, fnode))
		fnode->flags &= ~4;
	return result;
}

extern int sceFatfsACSetFSAttributeByPermission(SceExfatfsDrive *drive,
	SceUInt32 permission, unsigned char *attribute);
extern int applyFatAttributeToStat(SceExfatfsDrive *drive,
	unsigned char attribute, SceExfatfsStat *stat);
extern int sceFatfsACGetDirPermission(SceExfatfsDrive *drive, unsigned char attribute);
extern int sceFatfsACGetFSAttributeByThreadAccessLevel(SceExfatfsDrive *drive,
	SceUInt32 accessLevel, SceUInt32 permission, unsigned char *attribute);
extern void getCurrentFatDateTime(SceExfatfsDateTime *dateTime);
extern void touchFnodeModificationTime(SceExfatfsFnode *fnode);
extern unsigned int fatDateTimeToTime(SceExfatfsDateTime *dateTime);
extern unsigned int UVFAT_TimeToFATDate(SceExfatfsDateTime *dateTime);
extern SceUInt32 UVFAT_SearchEmptyCluster(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceUInt32 start);
extern int UVFAT_WriteCblock(SceUInt32 cluster, SceUInt32 block,
	SceExfatfsFileSystem *fileSystem, void *buffer);
extern int UVFAT_allocationbitmapentry(SceExfatfsSharedResource *sharedResource,
	int mode, SceExfatfsFileSystem *fileSystem, SceUInt32 cluster, SceUInt32 *value);
extern int UVFAT_LinkFromDir_ex(SceExfatfsSharedResource *sharedResource,
	SceUInt16 *fileName, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, SceExfatfsFnode *parent);
extern int UVFAT_SetGetFAT(SceExfatfsFileSystem *fileSystem, int mode,
	SceUInt32 cluster, SceUInt32 *value);

int UVFAT_MakeDir_vop(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *parent, SceFatfsNameset *nameset, unsigned int mode,
	SceExfatfsFnode *fnode)
{
	SceExfatfsFileSystem *fileSystem = parent->fileSystem;
	SceExfatfsFatDirectoryEntry *entry;
	SceUInt32 cluster, parentCluster, bufferSize, clusterSize, remaining, block, value, index;
	int result, error, permission;

	if (fileSystem->file.mode)
		return -1;
	if (!fileSystem->devname)
		return -5;
	SCE_DBG_PRINTF_LEVEL(2, 0, 1064,
		"%s: fnode[%p], refCount(%d), StartCluster[%#lx], DirCluster[%#lx]\n",
		"UVFAT_MakeDir_vop", fnode, fnode->refCount,
		fnode->Stat.StartCluster, fnode->DirCluster);
	if (fnode->Stat.ctime.year)
		return -9;
	fnode->DirOffset = 0;
	fnode->Stat.st_attr = 16;
	fnode->DirCluster = parent->Stat.StartCluster;
	fnode->dirGeneralSecondaryFlags = parent->Stat.generalSecondaryFlags;
	fnode->dirSize = parent->Stat.Size;
	fnode->DirOffClust = parent->Stat.StartCluster;
	if (sceFatfsACSetFSAttributeByPermission(fileSystem->drive, mode, &fnode->Stat.st_attr) ||
	    applyFatAttributeToStat(fileSystem->drive, fnode->Stat.st_attr, &fnode->Stat))
		return -15;
	getCurrentFatDateTime(&fnode->Stat.ctime);
	getCurrentFatDateTime(&fnode->Stat.atime);
	if (fileSystem->fileSystemType != 2) {
		fnode->Stat.atime.hour = 0;
		fnode->Stat.atime.minute = 0;
		fnode->Stat.atime.second = 0;
	}
	touchFnodeModificationTime(fnode);
	fnode->Stat.Size = 0;
	cluster = UVFAT_SearchEmptyCluster(sharedResource, fileSystem, 0);
	if (cluster >= fileSystem->ClusterMAX) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1139, "%s: no empty cluster\n", "UVFAT_MakeDir_vop");
		return -10;
	}
	parentCluster = fnode->DirCluster;
	bufferSize = fileSystem->fileBufferSize;
	if (parentCluster != parent->Stat.StartCluster) {
		if (parent->Stat.StartCluster)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 913, "_write_directory_start_cluster",
				"%s: DirCluster[%#lx] != StartCluster[%#lx]\n",
				"_write_directory_start_cluster", parentCluster, parent->Stat.StartCluster);
		else if (parentCluster != fileSystem->firstClusterOfRoot)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 916, "_write_directory_start_cluster",
				"%s: DirCluster[%#lx] != firstClusterOfRoot[%#lx], StartCluster[%#lx]\n",
				"_write_directory_start_cluster", parentCluster,
				fileSystem->firstClusterOfRoot, parent->Stat.StartCluster);
	}
	memset(fileSystem->file.Buf, 0, bufferSize);
	if (fileSystem->fileSystemType != 2) {
		entry = fileSystem->file.Buf;
		for (index = 0; index < sizeof(entry->name); ++index)
			entry->name[index] = ' ';
		entry->name[0] = '.';
		entry->extension[0] = ' ';
		entry->extension[1] = ' ';
		entry->extension[2] = ' ';
		entry->attribute = fnode->Stat.st_attr;
		entry->modificationTime = fatDateTimeToTime(&fnode->Stat.mtime);
		entry->modificationDate = UVFAT_TimeToFATDate(&fnode->Stat.mtime);
		entry->creationTime = fatDateTimeToTime(&fnode->Stat.ctime);
		entry->creationDate = UVFAT_TimeToFATDate(&fnode->Stat.ctime);
		entry->accessDate = UVFAT_TimeToFATDate(&fnode->Stat.atime);
		entry->firstClusterLow = cluster;
		if (fileSystem->FATBits == 32)
			entry->firstClusterHigh = cluster >> 16;
		++entry;
		for (index = 0; index < sizeof(entry->name); ++index)
			entry->name[index] = ' ';
		entry->extension[0] = ' ';
		entry->extension[1] = ' ';
		entry->extension[2] = ' ';
		entry->name[0] = '.';
		entry->name[1] = '.';
		if (parent->Stat.st_attr == 15) {
			permission = sceFatfsACGetDirPermission(fileSystem->drive, parent->Stat.st_attr);
			if (permission < 0) {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 968, "_write_directory_start_cluster",
					"%s: sceFatfsACGetDirPermission failed[%#x], parentFnode->st_mode[%#o], parentFnode->st_type(%d)\n",
					"_write_directory_start_cluster", permission,
					parent->Stat.st_mode, parent->Stat.st_type);
				entry->attribute |= 16;
			} else {
				error = sceFatfsACGetFSAttributeByThreadAccessLevel(fileSystem->drive,
					permission & 0x1b6, 128, &entry->attribute);
				if (error) {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 975, "_write_directory_start_cluster",
						"%s: sceFatfsACGetFSAttributeByThreadAccessLevel failed[%#x], check_mode[%#o]\n",
						"_write_directory_start_cluster", error, permission);
					entry->attribute = 16;
				} else {
					entry->attribute |= 16;
				}
			}
		} else {
			entry->attribute = parent->Stat.st_attr;
		}
		entry->modificationTime = fatDateTimeToTime(&fnode->Stat.mtime);
		entry->modificationDate = UVFAT_TimeToFATDate(&fnode->Stat.mtime);
		entry->creationTime = fatDateTimeToTime(&fnode->Stat.ctime);
		entry->creationDate = UVFAT_TimeToFATDate(&fnode->Stat.ctime);
		entry->accessDate = UVFAT_TimeToFATDate(&fnode->Stat.atime);
		if (parentCluster != fileSystem->firstClusterOfRoot) {
			entry->firstClusterLow = parentCluster;
			if (fileSystem->FATBits == 32)
				entry->firstClusterHigh = parentCluster >> 16;
		}
	}
	fileSystem->file.cacheCluster = cluster;
	fileSystem->file.clustBlockNo = 0;
	result = UVFAT_WriteCblock(cluster, 0, fileSystem, fileSystem->file.Buf);
	if (result)
		goto write_error;
	clusterSize = fileSystemClusterSize(fileSystem);
	if (bufferSize < clusterSize) {
		memset(fileSystem->file.Buf, 0, bufferSize);
		remaining = clusterSize - bufferSize;
		block = 1;
		while (remaining) {
			fileSystem->file.clustBlockNo = block;
			result = UVFAT_WriteCblock(cluster, block, fileSystem, fileSystem->file.Buf);
			if (result) {
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1037, "_write_directory_start_cluster",
					"UVFAT_WriteCblock(, %ld, %d,,) Fail\n", cluster, block);
				goto write_error;
			}
			++block;
			remaining -= bufferSize;
		}
	}
	fnode->Stat.Size = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	if (fileSystem->fileSystemType == 2) {
		fnode->Stat.generalSecondaryFlags = 3;
	} else {
		fnode->Stat.generalSecondaryFlags = 0;
		result = UVFAT_SetEOF(fileSystem, cluster);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1166, "%s: Cannot set EOF\n", "UVFAT_MakeDir_vop");
			return result;
		}
	}
	fnode->Stat.StartCluster = cluster;
	result = UVFAT_LinkFromDir_ex(sharedResource, fnode->FileName, fileSystem, fnode, parent);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1176,
			"%s: UVFAT_LinkFromDir failed(%d), startCluster[%#lx]\n",
			"UVFAT_MakeDir_vop", result, cluster);
		fnode->Stat.StartCluster = -1;
		if (fileSystem->fileSystemType == 2) {
			error = UVFAT_allocationbitmapentry(sharedResource, 2, fileSystem, cluster, NULL);
			if (error)
				SCE_DBG_PRINTF_LEVEL(1, 0, 1182, "%s: free cluster failed(%d)\n",
					"UVFAT_MakeDir_vop", error);
		} else {
			value = 0;
			error = UVFAT_SetGetFAT(fileSystem, 1, cluster, &value);
			if (error)
				SCE_DBG_PRINTF_LEVEL(1, 0, 1188, "%s: UVFAT_SetGetFAT failed(%d)\n",
					"UVFAT_MakeDir_vop", error);
		}
	} else {
		if (fnode->DirCluster != parent->Stat.StartCluster)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1193,
				"%s: fnode->DirCluster[%#lx] != expected[%#lx]\n",
				"UVFAT_MakeDir_vop", fnode->DirCluster, parent->Stat.StartCluster);
		if (fnode->dirGeneralSecondaryFlags != parent->Stat.generalSecondaryFlags)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1196,
				"%s: fnode->dirGeneralSecondaryFlags[%#x] != expected[%#x]\n",
				"UVFAT_MakeDir_vop", fnode->dirGeneralSecondaryFlags,
				parent->Stat.generalSecondaryFlags);
		if ((SceUInt64)fnode->dirSize != (SceUInt64)parent->Stat.Size)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1199,
				"%s: fnode->dirSize[%#x] != expected[%#llx]\n",
				"UVFAT_MakeDir_vop", fnode->dirSize, parent->Stat.Size);
	}
	return result;

write_error:
	SCE_DBG_PRINTF_LEVEL(1, 0, 1145,
		"%s: _write_directory_start_cluster failed(%d)\n", "UVFAT_MakeDir_vop", result);
	if (fileSystem->fileSystemType == 2) {
		error = UVFAT_allocationbitmapentry(sharedResource, 2, fileSystem, cluster, NULL);
		if (error)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1151, "%s: free cluster failed(%d)\n",
				"UVFAT_MakeDir_vop", error);
	}
	return result;
}
