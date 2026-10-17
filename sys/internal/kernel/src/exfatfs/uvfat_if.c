#include <psp2kern/types.h>
#include <string.h>
#include <sysmem/debug.h>
#include "exfatfs_internal.h"

extern int g_codepageTablesEnabled;
extern SceExfatfsFile *invalidateFileCachePosition(SceExfatfsFile *file);

int g_uvfatInitialized;

extern int UVFAT_ReadData(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFile *file,
	void *buffer, unsigned int size, char mode);
extern int UVFAT_LockMountPoint(SceExfatfsFileSystem *fileSystem);
extern int UVFAT_UnlockMountPoint(SceExfatfsFileSystem *fileSystem);

/* The release _ReserveUnusedFd contains this initializer inline. */
#if FW_VERSION >= 0x04000000
SceExfatfsFile *_InitializeFp(SceExfatfsFile *file)
{
	SceExfatfsFile *result;
	SceUInt32 cacheFlags;
	int codepageTablesEnabled;

	if (file->BufTop)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1925, "%s: unexpected BufTop[%p]\n",
			"_InitializeFp", file->BufTop);
	if (file->Buf)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1928, "%s: unexpected Buf[%p]\n",
			"_InitializeFp", file->Buf);
	if (file->fnode)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1931, "%s: unexpected fnode[%p]\n",
			"_InitializeFp", file->fnode);
	cacheFlags = file->cacheFlags;
	codepageTablesEnabled = g_codepageTablesEnabled;
	file->Position = 0;
	file->clusterIndex = 0x0fffffff;
	file->Buf = NULL;
	file->BufTop = NULL;
	file->Cluster = codepageTablesEnabled ? (SceUInt32)-2 : 0;
	if (cacheFlags) {
		if (cacheFlags & 2)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1898, "_InvalidateFpCachePosition",
				"%s: cache is still dirty because of write error\n",
				"_InvalidateFpCachePosition");
		else
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1900, "_InvalidateFpCachePosition",
				"%s: unexpected call for dirty cache\n",
				"_InvalidateFpCachePosition");
	}
	file->cacheFlags = 0;
	result = invalidateFileCachePosition(file);
	file->clustBlockNo = -1;
	return result;
}

#endif

extern int _exfat_lock_shared_resource(const char *caller);
extern int _exfat_unlock_shared_resource(const char *caller);
extern void _flush_fd_cache(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *fnode);
extern void clearClusterCacheStorage(SceExfatfsFnode *fnode);
extern void _invalidate_cluster_cache_data(SceExfatfsFnode *fnode);
extern int fatNameCompare(const SceUInt16 *left, const SceUInt16 *right);
extern int fatNameLength(const SceUInt16 *name);
extern SceUInt16 *fatNameCopyBounded(SceUInt16 *destination,
	const SceUInt16 *source, unsigned int count);

SceExfatfsFnode *_GetFnode(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *path)
{
	SceUInt32 count = sharedResource->initializedFnodeCount;
	SceUInt32 index;
	SceUInt32 initialized;
	SceExfatfsFnode *fnode;
	SceUInt16 *name;
	int length;

	if (count) {
		index = 0;
		do {
			fnode = &sharedResource->fnodes[index++];
			if (fnode->fileSystem == fileSystem && fnode->refCount &&
				fnode->FileName && !fatNameCompare(path, fnode->FilePath)) {
				fnode->refCount++;
				_flush_fd_cache(sharedResource, fnode);
				return fnode;
			}
		} while (index != count);
		if (_exfat_lock_shared_resource("_GetFnode") < 0)
			return NULL;
		index = 0;
		fnode = sharedResource->fnodes;
		if (fnode->refCount) {
			do {
				index++;
				fnode++;
				if (index == count)
					break;
			} while (fnode->refCount);
		}
		if (index < count) {
			fnode = &sharedResource->fnodes[index];
			fnode->refCount = 1;
			memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
			fnode->FileName = fnode->FilePath;
			fnode->fileSystem = NULL;
			memset(&fnode->Stat, 0, sizeof(fnode->Stat));
			if (g_codepageTablesEnabled) {
				fnode->DirCluster = -2;
				fnode->DirOffset = 0;
				fnode->DirOffClust = -2;
				fnode->Stat.StartCluster = -1;
			} else {
				fnode->DirCluster = 0;
				fnode->DirOffset = 0;
				fnode->DirOffClust = 0;
			}
			fnode->dirSize = 0;
			fnode->dirGeneralSecondaryFlags = 0;
			fnode->flags = 0;
			_invalidate_cluster_cache_data(fnode);
			fnode->fileSystem = fileSystem;
			goto found;
		}
	} else {
		if (_exfat_lock_shared_resource("_GetFnode") < 0)
			return NULL;
		index = 0;
	}
	initialized = sharedResource->initializedFnodeCount;
	if (initialized < sharedResource->fnodeCapacity) {
		fnode = &sharedResource->fnodes[initialized];
		clearClusterCacheStorage(fnode);
		_invalidate_cluster_cache_data(fnode);
		memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
		fnode->FileName = fnode->FilePath;
		fnode->fileSystem = NULL;
		memset(&fnode->Stat, 0, sizeof(fnode->Stat));
		if (g_codepageTablesEnabled) {
			fnode->DirCluster = -2;
			fnode->DirOffset = 0;
			fnode->DirOffClust = -2;
			fnode->Stat.StartCluster = -1;
		} else {
			fnode->DirCluster = 0;
			fnode->DirOffset = 0;
			fnode->DirOffClust = 0;
		}
		fnode->dirSize = 0;
		fnode->dirGeneralSecondaryFlags = 0;
		fnode->flags = 0;
		_invalidate_cluster_cache_data(fnode);
		initialized = sharedResource->initializedFnodeCount + 1;
		sharedResource->initializedFnodeCount = initialized;
	}
	if (index >= initialized) {
		_exfat_unlock_shared_resource("_GetFnode");
		SCE_DBG_PRINTF_LEVEL(2, 15, 1387, "No empty f_node\n");
		SCE_DBG_PRINTF_LEVEL(1, 0, 1388, "%s: No empty f_node, i(%d)\n",
			"_GetFnode", index);
		return NULL;
	}
	sharedResource->fnodes[index].refCount = 1;
	sharedResource->fnodes[index].fileSystem = fileSystem;
found:
	_exfat_unlock_shared_resource("_GetFnode");
	length = fatNameLength(path);
	if (!length || path[0] != '/')
		SCE_DBG_PRINTF_LEVEL(1, 0, 1404, "%s: unexpected path[0](%#x), len(%d)\n",
			"_GetFnode", path[0], length);
	fatNameCopyBounded(sharedResource->fnodes[index].FilePath, path, length);
	name = &sharedResource->fnodes[index].FilePath[length];
	sharedResource->fnodes[index].FileName = name;
	while (name[-1] != '/' && length) {
		name--;
		length--;
	}
	sharedResource->fnodes[index].FileName = name;
	return &sharedResource->fnodes[index];
}

/* Release firmware inlines this in UVFAT_ReleaseFnode and _ReleaseFp. */
#if FW_VERSION >= 0x04000000
int _ReleaseFnode(SceExfatfsFnode *fnode)
{
	int lockResult = -1;
	int result;
	unsigned int refCount;

	if (!fnode)
		return 0;
	if (!fnode->fileSystem) {
		lockResult = _exfat_lock_shared_resource("_ReleaseFnode");
		if (lockResult < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1433,
				"%s: _exfat_lock_shared_resource failed[%#x]\n",
				"_ReleaseFnode", lockResult);
			return -15;
		}
	}
	refCount = (SceUInt16)fnode->refCount;
	if (refCount == 1) {
		memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
		fnode->FileName = fnode->FilePath;
		fnode->fileSystem = NULL;
		memset(&fnode->Stat, 0, sizeof(fnode->Stat));
		if (g_codepageTablesEnabled) {
			fnode->DirCluster = -2;
			fnode->DirOffset = 0;
			fnode->DirOffClust = -2;
			fnode->Stat.StartCluster = -1;
		} else {
			fnode->DirCluster = 0;
			fnode->DirOffset = 0;
			fnode->DirOffClust = 0;
		}
		fnode->dirSize = 0;
		fnode->dirGeneralSecondaryFlags = 0;
		fnode->flags = 0;
		_invalidate_cluster_cache_data(fnode);
		refCount = (SceUInt16)fnode->refCount;
	}
	if (refCount)
		fnode->refCount = refCount - 1;
	else
		SCE_DBG_PRINTF_LEVEL(1, 0, 1449,
			"%s: unexpected refCount(%hd), fdCount(%hd), fnode[%p]\n",
			"_ReleaseFnode", 0, fnode->fdCount, fnode);
	if (lockResult != -1) {
		result = _exfat_unlock_shared_resource("_ReleaseFnode");
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1456,
				"%s: _exfat_unlock_shared_resource failed[%#x]\n",
				"_ReleaseFnode", result);
			return -15;
		}
	}
	return 0;
}
#endif

SceUInt32 getInitializedFnodeCount(SceExfatfsSharedResource *sharedResource)
{
	return sharedResource->initializedFnodeCount;
}

SceUInt32 getInitializedFileCount(SceExfatfsSharedResource *sharedResource)
{
	return sharedResource->initializedFileCount;
}

extern void *allocateCommonHeapMemory(int unused, SceSize size);

int UVFAT_Initialize(SceExfatfsSharedResource *sharedResource)
{
	SceExfatfsFile *files;
	SceExfatfsFile *file;
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsFnode *fnode;
	int index;

	if (g_uvfatInitialized == 1)
		return 0;
	files = sharedResource->files;
	if (!files) {
		files = allocateCommonHeapMemory(0, 512 * sizeof(*files));
		if (files) {
			sharedResource->files = files;
			sharedResource->fileCapacity = 512;
		} else {
			sharedResource->fileCapacity = 0;
		}
	}
	sharedResource->initializedFileCount = 0;
	index = 256;
	do {
		if (sharedResource->initializedFileCount < sharedResource->fileCapacity) {
			file = &sharedResource->files[sharedResource->initializedFileCount];
			memset(file, 0, sizeof(*file));
			file->mode = 0;
			sharedResource->initializedFileCount++;
		}
	} while (--index);
	if (!files)
		return -49;
	sharedResource->mountActivity[0].active = 0;
	sharedResource->mountActivity[1].active = 0;
	sharedResource->mountActivity[2].active = 0;
	sharedResource->mountActivity[3].active = 0;
	sharedResource->mountActivity[4].active = 0;
	sharedResource->mountActivity[5].active = 0;
	sharedResource->mountActivity[6].active = 0;
	sharedResource->mountActivity[7].active = 0;
	sharedResource->mountActivity[8].active = 0;
	sharedResource->mountActivity[9].active = 0;
	sharedResource->mountActivity[10].active = 0;
	sharedResource->mountActivity[11].active = 0;
	sharedResource->mountActivity[12].active = 0;
	sharedResource->mountActivity[13].active = 0;
	sharedResource->mountActivity[14].active = 0;
	index = 0;
	do {
		fileSystem = &sharedResource->fileSystems[index];
		memset(fileSystem, 0, sizeof(*fileSystem));
		fileSystem->file.fnode = &fileSystem->fnode;
		fileSystem->file.mode = 0;
		fnode = &fileSystem->fnode;
		clearClusterCacheStorage(fnode);
		_invalidate_cluster_cache_data(fnode);
		memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
		fnode->FileName = fnode->FilePath;
		fnode->fileSystem = NULL;
		memset(&fnode->Stat, 0, sizeof(fnode->Stat));
		if (g_codepageTablesEnabled) {
			fnode->DirCluster = -2;
			fnode->DirOffset = 0;
			fnode->DirOffClust = -2;
			fnode->Stat.StartCluster = -1;
		} else {
			fnode->DirCluster = 0;
			fnode->DirOffClust = 0;
			fnode->DirOffset = 0;
		}
		fnode->dirSize = 0;
		fnode->dirGeneralSecondaryFlags = 0;
		index++;
		fnode->flags = 0;
		_invalidate_cluster_cache_data(fnode);
		fnode->fileSystem = fileSystem;
		fileSystem->file.cacheIndex = -1;
		fileSystem->file.clustBlockNo = -1;
	} while (index != 15);
	sharedResource->fnodeCapacity = 4096;
	index = 0;
	do {
		fnode = &sharedResource->fnodes[index];
		clearClusterCacheStorage(fnode);
		_invalidate_cluster_cache_data(fnode);
		memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
		fnode->FileName = fnode->FilePath;
		fnode->fileSystem = NULL;
		memset(&fnode->Stat, 0, sizeof(fnode->Stat));
		if (g_codepageTablesEnabled) {
			fnode->DirCluster = -2;
			fnode->DirOffset = 0;
			fnode->DirOffClust = -2;
			fnode->Stat.StartCluster = -1;
		} else {
			fnode->DirCluster = 0;
			fnode->DirOffClust = 0;
			fnode->DirOffset = 0;
		}
		fnode->dirSize = 0;
		fnode->dirGeneralSecondaryFlags = 0;
		index++;
		fnode->flags = 0;
		_invalidate_cluster_cache_data(fnode);
	} while (index != 1024);
	g_uvfatInitialized = 1;
	sharedResource->initializedFnodeCount = 1024;
	return 0;
}

extern int UVFAT_MediaInitialize(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, int mountMode);
extern int UVFAT_InitLockMountPoint(SceExfatfsFileSystem *fileSystem);

int UVFAT_Mount(SceExfatfsSharedResource *sharedResource, const char *devname,
	int mode, const SceExfatfsMountParam *mountParam)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsFnode *fnode;
	SceExfatfsDrive *drive;
	int mountMode;
	int index = -1;
	int result;

	if (!devname)
		return -32;
	if (sharedResource->fileSystems[0].devname) {
		if (!strncmp(sharedResource->fileSystems[0].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 0;
	}
	if (sharedResource->fileSystems[1].devname) {
		if (!strncmp(sharedResource->fileSystems[1].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 1;
	}
	if (sharedResource->fileSystems[2].devname) {
		if (!strncmp(sharedResource->fileSystems[2].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 2;
	}
	if (sharedResource->fileSystems[3].devname) {
		if (!strncmp(sharedResource->fileSystems[3].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 3;
	}
	if (sharedResource->fileSystems[4].devname) {
		if (!strncmp(sharedResource->fileSystems[4].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 4;
	}
	if (sharedResource->fileSystems[5].devname) {
		if (!strncmp(sharedResource->fileSystems[5].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 5;
	}
	if (sharedResource->fileSystems[6].devname) {
		if (!strncmp(sharedResource->fileSystems[6].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 6;
	}
	if (sharedResource->fileSystems[7].devname) {
		if (!strncmp(sharedResource->fileSystems[7].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 7;
	}
	if (sharedResource->fileSystems[8].devname) {
		if (!strncmp(sharedResource->fileSystems[8].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 8;
	}
	if (sharedResource->fileSystems[9].devname) {
		if (!strncmp(sharedResource->fileSystems[9].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 9;
	}
	if (sharedResource->fileSystems[10].devname) {
		if (!strncmp(sharedResource->fileSystems[10].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 10;
	}
	if (sharedResource->fileSystems[11].devname) {
		if (!strncmp(sharedResource->fileSystems[11].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 11;
	}
	if (sharedResource->fileSystems[12].devname) {
		if (!strncmp(sharedResource->fileSystems[12].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 12;
	}
	if (sharedResource->fileSystems[13].devname) {
		if (!strncmp(sharedResource->fileSystems[13].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 13;
	}
	if (sharedResource->fileSystems[14].devname) {
		if (!strncmp(sharedResource->fileSystems[14].devname, devname, 1024))
			return -47;
	} else if (index == -1) {
		index = 14;
	}
	if (index == -1) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 239, "%s: no unused mount info\n",
			"UVFAT_Mount");
		return -10;
	}
	fileSystem = &sharedResource->fileSystems[index];
	memset(fileSystem, 0, sizeof(*fileSystem));
	fileSystem->file.fnode = &fileSystem->fnode;
	fnode = &fileSystem->fnode;
	clearClusterCacheStorage(fnode);
	_invalidate_cluster_cache_data(fnode);
	memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
	fnode->FileName = fnode->FilePath;
	fnode->fileSystem = NULL;
	memset(&fnode->Stat, 0, sizeof(fnode->Stat));
	if (g_codepageTablesEnabled) {
		fnode->DirCluster = -2;
		fnode->DirOffset = 0;
		fnode->DirOffClust = -2;
		fnode->Stat.StartCluster = -1;
	} else {
		fnode->DirCluster = 0;
		fnode->DirOffset = 0;
		fnode->DirOffClust = 0;
	}
	fnode->dirSize = 0;
	fnode->dirGeneralSecondaryFlags = 0;
	fnode->flags = 0;
	_invalidate_cluster_cache_data(fnode);
	fileSystem->bitmapIndex = 0;
	fileSystem->bitmapDirty = 0;
	drive = mountParam->drive;
	mountMode = mountParam->mount_mode;
	fileSystem->file.clustBlockNo = -1;
	fileSystem->devname = devname;
	fnode->fileSystem = fileSystem;
	fileSystem->file.cacheIndex = -1;
	fileSystem->PBRBuf = sharedResource->bootSectors[index];
	fileSystem->drive = drive;
	fileSystem->mount_mode = mountMode;
	fileSystem->PBRDirty = 0;
	fileSystem->mode = mode;
	fileSystem->state = 0;
	fileSystem->dataAreaStartSector = 0;
	fileSystem->rootDirectorySectors = 0;
	fileSystem->clusterCount = 0;
	fileSystem->fatDirty = 0;
	fileSystem->sectorSize = 0;
	fileSystem->clusterSectorCount = 0;
	fileSystem->firstClusterOfRoot = 0;
	fileSystem->devSectors = 0;
	fileSystem->bitmapFirstCluster = 0;
	fileSystem->bitmapSize = 0;
	fileSystem->bitmapBuffer = NULL;
	fileSystem->bitmapAllocation = NULL;
	fileSystem->bitmapDeviceSector = 0;
	fileSystem->bitmapNextCluster = 0;
	fileSystem->bitmapFreeClusterCount = 0;
	result = UVFAT_MediaInitialize(sharedResource, fileSystem, mountMode);
	if (!result)
		return UVFAT_InitLockMountPoint(fileSystem);
	fileSystem->devname = NULL;
	fileSystem->mode = 0;
	fileSystem->PBRBuf = NULL;
	return result;
}

int UVFAT_Read(SceExfatfsSharedResource *sharedResource, unsigned int fd,
	void *buffer, unsigned int size)
{
	SceExfatfsFile *files, *file;
	SceExfatfsFnode *fnode;
	SceUInt64 fileSize;
	int result;

	if (fd >= sharedResource->initializedFileCount)
		return -48;
	files = sharedResource->files;
	file = &files[fd];
	if (file->mode != 6 && file->mode != 1)
		return -2;
	if (!size)
		return 0;
	fnode = files[fd].fnode;
	fileSize = (SceUInt64)fnode->Stat.Size;
	if (!fileSize || (SceUInt64)file->Position >= fileSize)
		return 0;
	result = UVFAT_LockMountPoint(fnode->fileSystem);
	if (result)
		return result;
	result = UVFAT_ReadData(sharedResource,
		sharedResource->files[fd].fnode->fileSystem,
		&sharedResource->files[fd], buffer, size, 1);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 15, 652,
			"UVFAT_ReadData() Fail, er=%d\n", result);
	UVFAT_UnlockMountPoint(sharedResource->files[fd].fnode->fileSystem);
	return result;
}

extern void *allocateFatBuffer(unsigned int size);
/* Original callers pass the allocation size although the helper ignores it. */
extern void freeFatBuffer(void *buffer, unsigned int size);
extern int UVFAT_WriteData(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFile *file,
	const void *buffer, unsigned int size, int mode, void *result);
extern int UVFAT_FlushFdBuf(SceExfatfsFile *file,
	SceExfatfsFileSystem *fileSystem);
extern int flushFileSystemMetadata(SceExfatfsFileSystem *fileSystem);
extern int _update_metadata(SceExfatfsSharedResource *sharedResource,
	unsigned int fd);

int UVFAT_Write(SceExfatfsSharedResource *sharedResource, unsigned int fd,
	const void *buffer, unsigned int size)
{
	SceExfatfsFile *file = &sharedResource->files[fd];
	SceExfatfsFnode *fnode = file->fnode;
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceUInt64 position, end, current;
	void *allocation, *zeroBuffer;
	unsigned int writeSize, bufferSize;
	int result, flushResult;

	if (fd >= sharedResource->initializedFileCount)
		return -48;
	if ((file->mode & ~4) != 2)
		return -2;
	position = (SceUInt64)file->Position;
	if (fileSystem->fileSystemType != 2 && position + size >= 0x100000000ULL) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 677,
			"%s: file size limit of FAT < Position[%#llx] + size[%#lx], type(%d)\n",
			"UVFAT_Write", position, size, fileSystem->fileSystemType);
		return -32;
	}
	if ((SceUInt64)fnode->Stat.Size < position) {
		allocation = allocateFatBuffer(fileSystem->fileBufferSize + 512);
		if (!allocation) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 692, "No memory\n");
			return -15;
		}
		memset(allocation, 0, fileSystem->fileBufferSize + 512);
		file = &sharedResource->files[fd];
		end = (SceUInt64)file->Position;
		current = (SceUInt64)file->fnode->Stat.Size;
		file->Position = current;
		zeroBuffer = (void *)((((SceUIntPtr)allocation + 511) >> 9) << 9);
		while (current < end) {
			bufferSize = fileSystem->fileBufferSize;
			writeSize = end - current;
			if (end - current >= bufferSize)
				writeSize = bufferSize - current % bufferSize;
			result = UVFAT_WriteData(sharedResource, fileSystem,
				file, zeroBuffer, writeSize, 1, NULL);
			if (result < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 15, 714,
					"UVFAT_WriteData() Fail, er=%d\n", result);
				freeFatBuffer(allocation,
					sharedResource->files[fd].fnode->fileSystem->fileBufferSize + 512);
				return result;
			}
			current += result;
			if (current >= end)
				break;
			file = &sharedResource->files[fd];
		}
		freeFatBuffer(allocation,
			sharedResource->files[fd].fnode->fileSystem->fileBufferSize + 512);
		file = &sharedResource->files[fd];
	}
	result = UVFAT_WriteData(sharedResource, file->fnode->fileSystem,
		file, buffer, size, 1, NULL);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 15, 728,
			"UVFAT_WriteData() Fail, er=%d\n", result);
	if (fileSystem->mount_mode & 0x100) {
		file = &sharedResource->files[fd];
		flushResult = UVFAT_FlushFdBuf(file, file->fnode->fileSystem);
		if (flushResult) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 736,
				"%s: UVFAT_FlushFdBuf() Fail[%#x]\n", "UVFAT_Write", flushResult);
			flushFileSystemMetadata(sharedResource->files[fd].fnode->fileSystem);
			result = flushResult;
		} else {
			flushResult = _update_metadata(sharedResource, fd);
			if (flushResult)
				result = flushResult;
		}
	}
	return result;
}

int getFnodeReferenceCount(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *path)
{
	SceExfatfsFnode *fnode;
	unsigned int count = sharedResource->initializedFnodeCount;
	unsigned int index = 0;

	if (!count)
		return 0;
	do {
		fnode = &sharedResource->fnodes[index++];
		if (fnode->fileSystem == fileSystem && fnode->refCount &&
		    fnode->FileName && !fatNameCompare(path, fnode->FilePath))
			return fnode->refCount;
	} while (index != count);
	return 0;
}

SceExfatfsFnode *UVFAT_GetFnode(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *path)
{
	return _GetFnode(sharedResource, fileSystem, path);
}

extern int sceFatfsACSetFSAttributeByPermission(SceExfatfsDrive *drive,
	SceUInt32 permission, unsigned char *attribute);
extern int applyFatAttributeToStat(SceExfatfsDrive *drive,
	unsigned char attribute, SceExfatfsStat *stat);
extern int getCurrentFatDateTime(SceExfatfsDateTime *dateTime);
extern int UVFAT_LinkFromDir_ex(SceExfatfsSharedResource *sharedResource,
	const SceUInt16 *fileName, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, SceExfatfsFnode *parent);
extern int initializeFnode(SceExfatfsFnode *fnode);

int UVFAT_Create(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *parent, SceFatfsNameset *nameset, int mode,
	SceExfatfsFnode **output)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsFnode *fnode;
	SceUInt32 startCluster, directorySize;
	unsigned char generalSecondaryFlags;
	int result;
#if FW_VERSION < 0x04000000
	int lockResult;
	unsigned short references;
#endif

	if (!parent) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1484, "%s: NULL parentFnode\n", "UVFAT_Create");
		return -32;
	}
	fileSystem = parent->fileSystem;
	if (!fileSystem) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1490, "%s: NULL FileSystem, fnode[%p]\n",
			"UVFAT_Create", parent);
		return -32;
	}
	fnode = _GetFnode(sharedResource, fileSystem, nameset->filePath);
	if (!fnode) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1507, "%s: _GetFnode failed\n", "UVFAT_Create");
		return -22;
	}
	if (fnode->Stat.ctime.year) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1542, "%s: unexpected refered fnode[%p]\n",
			"UVFAT_Create", fnode);
		result = fnode->Stat.st_type == 6 ? -6 : -9;
		goto release_error;
	}
	generalSecondaryFlags = parent->Stat.generalSecondaryFlags;
	startCluster = parent->Stat.StartCluster;
	directorySize = parent->Stat.Size;
	fnode->Stat.st_attr = 32;
	fnode->dirGeneralSecondaryFlags = generalSecondaryFlags;
	fnode->DirOffset = 0;
	fnode->DirCluster = startCluster;
	fnode->DirOffClust = startCluster;
	fnode->dirSize = directorySize;
	result = sceFatfsACSetFSAttributeByPermission(fileSystem->drive, mode,
		&fnode->Stat.st_attr);
	if (result)
		goto release_error;
	result = applyFatAttributeToStat(fileSystem->drive, fnode->Stat.st_attr,
		&fnode->Stat);
	if (result)
		goto release_error;
	fnode->flags |= 4;
	getCurrentFatDateTime(&fnode->Stat.ctime);
	getCurrentFatDateTime(&fnode->Stat.mtime);
	getCurrentFatDateTime(&fnode->Stat.atime);
	if (fileSystem->fileSystemType != 2) {
		fnode->Stat.atime.hour = 0;
		fnode->Stat.atime.minute = 0;
		fnode->Stat.atime.second = 0;
	}
	fnode->Stat.Size = 0;
	fnode->Stat.StartCluster = 0;
	SCE_DBG_PRINTF_LEVEL(2, 0, 1585, "%s: st_mode=0x%x, st_type=0x%x, stat=0x%p\n",
		"UVFAT_Create", fnode->Stat.st_mode, fnode->Stat.st_type, &fnode->Stat);
	fnode->Stat.generalSecondaryFlags = 1;
	result = UVFAT_LinkFromDir_ex(sharedResource, fnode->FileName, fileSystem,
		fnode, parent);
	if (result)
		goto release_error;
	if (fnode->DirCluster == (SceUInt32)-2)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1596,
			"%s: unexpected DirCluster[%#lx], fnode[%p]\n",
			"UVFAT_Create", fnode->DirCluster, fnode);
	if (fnode->DirCluster != parent->Stat.StartCluster)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1599,
			"%s: fnode->DirCluster[%#lx] != expected[%#lx]\n",
			"UVFAT_Create", fnode->DirCluster, parent->Stat.StartCluster);
	if (fnode->dirGeneralSecondaryFlags != parent->Stat.generalSecondaryFlags)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1602,
			"%s: fnode->dirGeneralSecondaryFlags[%#x] != expected[%#x]\n",
			"UVFAT_Create", fnode->dirGeneralSecondaryFlags,
			parent->Stat.generalSecondaryFlags);
	if ((SceUInt64)fnode->dirSize != (SceUInt64)parent->Stat.Size)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1605,
			"%s: fnode->dirSize[%#x] != expected[%#llx]\n",
			"UVFAT_Create", fnode->dirSize, parent->Stat.Size);
	if (fnode->fileSystem != fileSystem) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1627,
			"%s: fnode->FileSystem[%p] != fileSystem[%p]\n",
			"UVFAT_Create", fnode->fileSystem, fileSystem);
		fnode->fileSystem = fileSystem;
	}
	if (output) {
		*output = fnode;
	} else {
#if FW_VERSION >= 0x04000000
		_ReleaseFnode(fnode);
#else
		lockResult = -1;
		if (fnode->fileSystem ||
		    (lockResult = _exfat_lock_shared_resource("_ReleaseFnode")) >= 0) {
			if (fnode->refCount == 1)
				initializeFnode(fnode);
			if (fnode->refCount)
				--fnode->refCount;
			if (lockResult != -1)
				_exfat_unlock_shared_resource("_ReleaseFnode");
		}
#endif
	}
	return result;

release_error:
#if FW_VERSION >= 0x04000000
	_ReleaseFnode(fnode);
#else
	lockResult = -1;
	if (fnode->fileSystem ||
	    (lockResult = _exfat_lock_shared_resource("_ReleaseFnode")) >= 0) {
		references = fnode->refCount;
		if (references == 1) {
			memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
			fnode->FileName = fnode->FilePath;
			fnode->fileSystem = NULL;
			memset(&fnode->Stat, 0, sizeof(fnode->Stat));
			if (g_codepageTablesEnabled) {
				fnode->DirCluster = -2;
				fnode->DirOffset = 0;
				fnode->DirOffClust = -2;
				fnode->Stat.StartCluster = -1;
			} else {
				fnode->DirCluster = 0;
				fnode->DirOffset = 0;
				fnode->DirOffClust = 0;
			}
			fnode->dirSize = 0;
			fnode->dirGeneralSecondaryFlags = 0;
			fnode->flags = 0;
			_invalidate_cluster_cache_data(fnode);
			references = fnode->refCount;
		}
		if (references)
			fnode->refCount = references - 1;
		if (lockResult != -1)
			_exfat_unlock_shared_resource("_ReleaseFnode");
	}
#endif
	if (output)
		*output = NULL;
	return result;
}

int _change_path_element(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *oldPath,
	int keepLength, SceExfatfsFnode *fnode)
{
	const SceUInt16 *path;
	SceExfatfsFnode *entry;
	SceUInt16 *name, *temporary = fileSystem->fnode.FilePath;
	unsigned int index, count, suffixLength;
	int oldLength, newLength, fullLength;

	path = oldPath;
	oldLength = *path;
	if (oldLength) {
		oldLength = 0;
		do {
			++oldLength;
		} while (*++path);
	}
	path = fnode->FilePath;
	newLength = *path;
	if (newLength) {
		newLength = 0;
		do {
			++newLength;
		} while (*++path);
	}
	count = sharedResource->initializedFnodeCount;
	if (!count)
		return 0;
	fullLength = newLength + keepLength;
	index = 0;
	entry = sharedResource->fnodes;
	do {
		__builtin_prefetch(&sharedResource->fnodes[index + 2].fileSystem);
		if (entry->fileSystem != fileSystem)
			continue;
		__builtin_prefetch(&sharedResource->fnodes[index + 2].FileName);
		if (!entry->FileName ||
		    memcmp(entry->FilePath, oldPath, oldLength * sizeof(*oldPath)))
			continue;
		if (entry->FilePath[oldLength] != '/' && entry->FilePath[oldLength])
			continue;
		path = &entry->FilePath[oldLength];
		suffixLength = 0;
		if (*path) {
			do {
				++suffixLength;
			} while (*++path);
		}
		++suffixLength;
		if (suffixLength + fullLength > 260) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1689,
				"%s: new full_path is too long, old_length(%d), new_length(%d), keep_length(%d), buffer_length(%d)\n",
				"_change_path_element", oldLength, fullLength, suffixLength, 260);
			return 0x8001005b;
		}
		memcpy(temporary, &entry->FilePath[oldLength], suffixLength * sizeof(*path));
		if (newLength)
			memcpy(&entry->FilePath[keepLength], fnode->FilePath,
				newLength * sizeof(*path));
		name = memcpy(&entry->FilePath[fullLength], temporary,
			suffixLength * sizeof(*path));
		if (suffixLength <= 1) {
			if (entry->FilePath[fullLength] != '/') {
				do {
					--name;
				} while (*name != '/');
			}
			entry->FileName = name + 1;
		} else {
			entry->FileName += fullLength - oldLength;
		}
	} while (++entry, ++index != count);
	return 0;
}

SceExfatfsFnode *UVFAT_GetParentFnode(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *path)
{
	SceUInt16 *buffer, *separator;
	SceUInt16 character;
	SceExfatfsFnode *fnode;

	if ((unsigned int)fatNameLength(path) > 260) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 1769, "Path length over\n");
		return NULL;
	}
	buffer = allocateFatBuffer(520);
	fatNameCopyBounded(buffer, path, fatNameLength(path));
	separator = &buffer[fatNameLength(path)];
	do {
		character = *--separator;
	} while (character != '/');
	if (buffer == separator) {
		buffer[0] = character;
		buffer[1] = 0;
	} else {
		*separator = 0;
	}
	fnode = _GetFnode(sharedResource, fileSystem, buffer);
	freeFatBuffer(buffer, 520);
	return fnode;
}

int UVFAT_ReleaseFnode(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *fnode)
{
#if FW_VERSION >= 0x04000000
	return _ReleaseFnode(fnode);
#else
	unsigned short references;
	int lockResult = -1;

	if (!fnode)
		return 0;
	if (!fnode->fileSystem) {
		lockResult = _exfat_lock_shared_resource("_ReleaseFnode");
		if (lockResult < 0)
			return -15;
	}
	references = fnode->refCount;
	if (references == 1) {
		memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
		fnode->FileName = fnode->FilePath;
		fnode->fileSystem = NULL;
		memset(&fnode->Stat, 0, sizeof(fnode->Stat));
		if (g_codepageTablesEnabled) {
			fnode->DirCluster = -2;
			fnode->DirOffset = 0;
			fnode->DirOffClust = -2;
			fnode->Stat.StartCluster = -1;
		} else {
			fnode->DirCluster = 0;
			fnode->DirOffset = 0;
			fnode->DirOffClust = 0;
		}
		fnode->dirSize = 0;
		fnode->dirGeneralSecondaryFlags = 0;
		fnode->flags = 0;
		_invalidate_cluster_cache_data(fnode);
		references = fnode->refCount;
	}
	if (references)
		fnode->refCount = references - 1;
	if (lockResult != -1) {
		lockResult = _exfat_unlock_shared_resource("_ReleaseFnode");
		if (lockResult < 0)
			return -15;
		return 0;
	}
	return 0;
#endif
}

int initializeFnode(SceExfatfsFnode *fnode)
{
	memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
	fnode->FileName = fnode->FilePath;
	fnode->fileSystem = NULL;
	memset(&fnode->Stat, 0, sizeof(fnode->Stat));
	if (g_codepageTablesEnabled) {
		fnode->DirCluster = -2;
		fnode->DirOffset = 0;
		fnode->DirOffClust = -2;
		fnode->Stat.StartCluster = -1;
	} else {
		fnode->DirCluster = 0;
		fnode->DirOffset = 0;
		fnode->DirOffClust = 0;
	}
	fnode->dirSize = 0;
	fnode->dirGeneralSecondaryFlags = 0;
	fnode->flags = 0;
	_invalidate_cluster_cache_data(fnode);
	return 0;
}

SceExfatfsFile *resetFilePosition(SceExfatfsFile *file)
{
	file->Position = 0;
	file->Cluster = g_codepageTablesEnabled ? (SceUInt32)-2 : 0;
	file->clusterIndex = 0x0fffffff;
	return file;
}

int _AllocateFdBuffer(SceExfatfsFile *file)
{
	unsigned int size;
	void *buffer;

	if (file->fnode) {
		if (file->fnode->fileSystem) {
			size = file->fnode->fileSystem->fileBufferSize + 512;
		} else {
			size = 1024;
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1862, "_fd_cache_size",
				"%s: unexpected NULL fileSystem\n", "_fd_cach");
		}
	} else {
		size = 1024;
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1855, "_fd_cache_size",
			"%s: unexpected NULL fnode\n", "_fd_cach");
	}
	if (file->BufTop || file->Buf)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1968, "%s: unexpected BufTop[%p], Buf[%p]\n",
			"_AllocateFdBuffer", file->BufTop, file->Buf);
	buffer = allocateFatBuffer(size);
	file->BufTop = buffer;
	if (!buffer) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1972, "%s: No memory\n", "_AllocateFdBuffer");
		return 0x8001000c;
	}
	file->Buf = (void *)(((SceUIntPtr)buffer + 511) >> 9 << 9);
	return 0;
}

void _FreeFpCache(SceExfatfsFile *file)
{
	unsigned int size;
	void *buffer = file->BufTop;

	if (buffer) {
		if (file->fnode) {
			if (file->fnode->fileSystem) {
				size = file->fnode->fileSystem->fileBufferSize + 512;
			} else {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1862, "_fd_cache_size",
					"%s: unexpected NULL fileSystem\n", "_fd_cach");
				size = 1024;
				buffer = file->BufTop;
			}
		} else {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1855, "_fd_cache_size",
				"%s: unexpected NULL fnode\n", "_fd_cach");
			size = 1024;
			buffer = file->BufTop;
		}
		if (file->Buf) {
			file->Buf = NULL;
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1989,
				"%s: unexpected BufTop[%p] with NULL Buf\n", "_FreeFpCache", buffer);
			buffer = file->BufTop;
		}
		freeFatBuffer(buffer, size);
		file->BufTop = NULL;
	} else if (file->Buf) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1994,
			"%s: unexpected Buf[%p] with NULL BufTop\n", "_FreeFpCache", file->Buf);
		file->Buf = NULL;
	}
}

extern int _deleted_entry(SceExfatfsFnode *fnode);
extern int _exfatfs_free_cluster_link(SceExfatfsFnode *fnode, SceUInt32 cluster);

void _ReleaseFp(SceExfatfsSharedResource *sharedResource, SceExfatfsFile *file)
{
	SceExfatfsFnode *fnode = file->fnode;
	unsigned short fdCount;
#if FW_VERSION < 0x04000000
	void *buffer = file->BufTop;
	SceExfatfsFileSystem *fileSystem;
	unsigned int size;
	unsigned short references;
	int lockResult;

	if (buffer) {
		if (fnode && fnode->fileSystem)
			size = fnode->fileSystem->fileBufferSize + 512;
		else
			size = 1024;
		if (file->Buf)
			file->Buf = NULL;
		freeFatBuffer(buffer, size);
		file->BufTop = NULL;
	} else if (file->Buf) {
		file->Buf = NULL;
	}
#else
	_FreeFpCache(file);
#endif
	if (fnode) {
		fdCount = fnode->fdCount;
		if (fdCount) {
			fnode->fdCount = fdCount - 1;
			SCE_DBG_PRINTF_LEVEL(2, 0, 2008,
				"%s: fdCount(%hd), refCount(%hd), fnode[%p]\n",
				"_ReleaseFp", fnode->fdCount, fnode->refCount, fnode);
			if (!fnode->fdCount && _deleted_entry(fnode) && fnode->fileSystem)
				_exfatfs_free_cluster_link(fnode, fnode->Stat.StartCluster);
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2016,
				"%s: unexpected fdCount(%hd), refCount(%hd), fnode[%p]\n",
				"_ReleaseFp", (unsigned short)fnode->fdCount, fnode->refCount, fnode);
		}
#if FW_VERSION < 0x04000000
		fileSystem = fnode->fileSystem;
#endif
		file->fnode = NULL;
#if FW_VERSION >= 0x04000000
		_ReleaseFnode(fnode);
#else
		lockResult = -1;
		if (fileSystem ||
		    (lockResult = _exfat_lock_shared_resource("_ReleaseFnode")) >= 0) {
			references = fnode->refCount;
			if (references == 1) {
				memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
				fnode->FileName = fnode->FilePath;
				fnode->fileSystem = NULL;
				memset(&fnode->Stat, 0, sizeof(fnode->Stat));
				if (g_codepageTablesEnabled) {
					fnode->DirCluster = -2;
					fnode->DirOffset = 0;
					fnode->DirOffClust = -2;
					fnode->Stat.StartCluster = -1;
				} else {
					fnode->DirCluster = 0;
					fnode->DirOffset = 0;
					fnode->DirOffClust = 0;
				}
				fnode->dirSize = 0;
				fnode->dirGeneralSecondaryFlags = 0;
				fnode->flags = 0;
				_invalidate_cluster_cache_data(fnode);
				references = fnode->refCount;
			}
			if (references)
				fnode->refCount = references - 1;
			if (lockResult != -1)
				_exfat_unlock_shared_resource("_ReleaseFnode");
		}
#endif
	}
	file->mode = 0;
}

unsigned int _ReserveUnusedFd(SceExfatfsSharedResource *sharedResource, int mode)
{
	unsigned int count = sharedResource->initializedFileCount;
	unsigned int fd = 1;
	unsigned int initialized;
	SceExfatfsFile *file, *nextFile;

	if (_exfat_lock_shared_resource("_ReserveUnusedFd") < 0)
		return count;
	if (count > 1) {
		file = &sharedResource->files[fd];
		if (file->mode) {
			nextFile = file + 1;
			do {
				++fd;
				file = nextFile++;
				if (fd == count)
					goto extend;
			} while (file->mode);
		}
#if FW_VERSION < 0x04000000
		file->Position = 0;
		file->mode = mode;
		file->clusterIndex = 0x0fffffff;
		file->Cluster = g_codepageTablesEnabled ? (SceUInt32)-2 : 0;
		file->Buf = NULL;
		file->BufTop = NULL;
		file->cacheFlags = 0;
		invalidateFileCachePosition(file);
		file->clustBlockNo = -1;
		goto unlock;
#else
		goto initialize;
#endif
	}
extend:
	initialized = sharedResource->initializedFileCount;
	if (initialized < sharedResource->fileCapacity) {
		memset(&sharedResource->files[initialized], 0, sizeof(*file));
		initialized = sharedResource->initializedFileCount + 1;
		sharedResource->initializedFileCount = initialized;
	}
	if (fd >= initialized)
		goto unlock;
	file = &sharedResource->files[fd];
#if FW_VERSION >= 0x04000000
initialize:
	file->mode = mode;
	_InitializeFp(file);
#else
	file->Position = 0;
	file->Cluster = g_codepageTablesEnabled ? (SceUInt32)-2 : 0;
	file->clusterIndex = 0x0fffffff;
	file->mode = mode;
	file->Buf = NULL;
	file->BufTop = NULL;
	file->cacheFlags = 0;
	invalidateFileCachePosition(file);
	file->clustBlockNo = -1;
#endif
unlock:
	_exfat_unlock_shared_resource("_ReserveUnusedFd");
	return fd;
}

int closeFileDescriptor(SceExfatfsSharedResource *sharedResource, unsigned int fd)
{
	_ReleaseFp(sharedResource, &sharedResource->files[fd]);
	return 0;
}
