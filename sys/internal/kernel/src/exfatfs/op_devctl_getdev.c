#include <psp2kern/types.h>
#include <iofilemgr/iofilemgr.h>
#include <string.h>
#include <sysmem/debug.h>
#include "exfatfs_internal.h"


typedef struct SceExfatfsDeviceInfo {
	SceIoDevInfo base;
	SceUInt32 volumeSerial;
	unsigned char volumeLabel[12];
} SceExfatfsDeviceInfo;

extern int UVFAT_readallocationbitmap(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem);
extern int UVFAT_GetFreeClusterCount(SceExfatfsFileSystem *fileSystem,
	SceUInt32 *count);
extern int exfatfsErrorToIoError(int error);

int _exfatfs_get_devinf(SceExfatfsFileSystem *fileSystem, void *information,
	SceSize informationSize, SceExfatfsSharedResource *sharedResource)
{
	SceIoDevInfo *base = information;
	SceExfatfsDeviceInfo *extended;
	SceExfatfsBootSector *boot;
	const unsigned char *volumeLabel;
	SceUInt64 freeSize;
	SceUInt32 count, clusterSize;
	int error;

	if (informationSize == sizeof(*extended)) {
		extended = information;
	} else {
		if (informationSize != sizeof(*base)) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 147,
				"%s: buflen(%d) != expected(%d or %d)\n", "_exfatfs_get_devinf",
				informationSize, sizeof(*base), sizeof(*extended));
			return exfatfsErrorToIoError(-32);
		}
		extended = NULL;
	}
	clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	base->clusterSize = clusterSize;
	base->maxSize = (SceUInt64)clusterSize * fileSystem->clusterCount;
	if (fileSystem->fileSystemType == 2) {
		error = UVFAT_readallocationbitmap(sharedResource, fileSystem);
		if (!error) {
			count = fileSystem->bitmapFreeClusterCount;
			goto free_count;
		}
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 123, "_get_free_count",
			"Cannot count free cluster num\n");
	} else {
		error = UVFAT_GetFreeClusterCount(fileSystem, &count);
		if (!error)
			goto free_count;
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 131, "_get_free_count",
			"Cannot get free cluster num\n");
	}
	error = exfatfsErrorToIoError(error);
	if (error)
		return exfatfsErrorToIoError(error);

free_count:
	if (fileSystem->fakeFreeClusterCount >= count) {
		count = 0;
		freeSize = 0;
	} else {
		count -= fileSystem->fakeFreeClusterCount;
		freeSize = (SceUInt64)count * base->clusterSize;
	}
	base->freeSize = freeSize;
	if (extended) {
		extended->volumeLabel[11] = 0;
		extended->base.reserved = NULL;
		boot = fileSystem->PBRBuf;
		if (fileSystem->fileSystemType == 2) {
			extended->volumeSerial = ((SceUInt32)boot->exfat.volumeSerial[2] << 16) |
				((SceUInt32)boot->exfat.volumeSerial[3] << 24) |
				boot->exfat.volumeSerial[0] | ((SceUInt32)boot->exfat.volumeSerial[1] << 8);
			memset(extended->volumeLabel, ' ', 11);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 57, "_exfat_get_volume_label",
				"%s: not supported yet\n", "_exfat_get_volume_label");
		} else {
			if (boot->fat16.parameters.rootEntryCount) {
				extended->volumeSerial = ((SceUInt32)boot->fat16.volumeSerial[2] << 16) |
					((SceUInt32)boot->fat16.volumeSerial[3] << 24) |
					boot->fat16.volumeSerial[0] | ((SceUInt32)boot->fat16.volumeSerial[1] << 8);
				volumeLabel = boot->fat16.volumeLabel;
			} else {
				extended->volumeSerial = boot->fat32.volumeSerial;
				volumeLabel = boot->fat32.volumeLabel;
			}
			__builtin_memcpy(extended->volumeLabel, volumeLabel, 11);
		}
	}
	return 0;
}

int _exfatfs_set_fake_free(SceExfatfsFileSystem *fileSystem,
	SceExfatfsSharedResource *resource, SceUInt64 fakeFree,
	SceExfatfsSharedResource *sharedResource)
{
	SceUInt64 clusterSize = (SceUInt64)fileSystem->clusterSectorCount * fileSystem->sectorSize;
	SceUInt64 capacity = clusterSize * fileSystem->clusterCount;
	SceUInt64 actualFree;
	SceUInt32 count;
	int error;

	if (capacity < fakeFree) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 194,
			"%s: fake free[%#llx] > capacity[%#llx]\n",
			"_exfatfs_set_fake_free", fakeFree, capacity);
		error = -10;
		goto translate_error;
	}
	if (fileSystem->fileSystemType == 2) {
		error = UVFAT_readallocationbitmap(sharedResource, fileSystem);
		if (!error) {
			count = fileSystem->bitmapFreeClusterCount;
			goto free_count;
		}
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 123, "_get_free_count",
			"Cannot count free cluster num\n");
	} else {
		error = UVFAT_GetFreeClusterCount(fileSystem, &count);
		if (!error)
			goto free_count;
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 131, "_get_free_count",
			"Cannot get free cluster num\n");
	}
	error = exfatfsErrorToIoError(error);
	if (error)
		goto translate_error;

free_count:
	actualFree = clusterSize * count;
	if (actualFree < fakeFree) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 209,
			"%s: fake free[%#llx] > actual free[%#llx]\n",
			"_exfatfs_set_fake_free", fakeFree, actualFree);
		error = -10;
		goto translate_error;
	}
	fileSystem->fakeFreeClusterCount = count - fakeFree / clusterSize;
	return 0;

translate_error:
	return exfatfsErrorToIoError(error);
}
