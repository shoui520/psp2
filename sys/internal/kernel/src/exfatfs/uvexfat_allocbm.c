#include <sysmem/debug.h>
#include "exfatfs_internal.h"

extern int UVFAT_InitDirEntry_ex(SceExfatfsSharedResource *sharedResource,
	int fd, SceUInt32 cluster, SceExfatfsFileSystem *fileSystem,
	unsigned int flags, SceUInt32 size);
extern int UVFAT_ReadDirEntry(SceExfatfsSharedResource *sharedResource,
	int fd, void *entry, int flags, SceExfatfsFileSystem *fileSystem);
extern int _exfatfs_release_temporal_file(SceExfatfsFileSystem *fileSystem);

int _check_allocation_bitmap(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsBootSector *boot = fileSystem->PBRBuf;
	unsigned int bitmapIndex = *(const unsigned char *)&boot->exfat.volumeFlags;
	SceExfatfsAllocationBitmapEntry entry;
	SceUInt32 firstCluster, lengthLow, lengthHigh;
	int result;

	result = UVFAT_InitDirEntry_ex(sharedResource, 0, fileSystem->firstClusterOfRoot,
		fileSystem, 0, 0);
	bitmapIndex &= 1;
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 41, "%s: UVFAT_InitDirEntry_ex failed(%d)\n",
			"_check_allocation_bitmap", result);
		goto out;
	}
	for (;;) {
		result = UVFAT_ReadDirEntry(sharedResource, 0, &entry, 0, fileSystem);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 50, "%s: UVFAT_ReadDirEntry failed[%#x]\n",
				"_check_allocation_bitmap", result);
			if (result == -7)
				result = -37;
			break;
		}
		if (entry.entryType == 0x81) {
			if (bitmapIndex == (entry.bitmapFlags & 1))
				goto found;
		} else if (!entry.entryType) {
			result = -37;
			break;
		}
	}
	_exfatfs_release_temporal_file(fileSystem);
out:
	if (result == -37)
		result = -15;
	return result;
found:
	_exfatfs_release_temporal_file(fileSystem);
	firstCluster = (entry.firstCluster[2] << 16) | (entry.firstCluster[3] << 24) |
		entry.firstCluster[0] | (entry.firstCluster[1] << 8);
	lengthLow = (entry.dataLength[2] << 16) | (entry.dataLength[3] << 24) |
		entry.dataLength[0] | (entry.dataLength[1] << 8);
	lengthHigh = (entry.dataLength[6] << 16) | (entry.dataLength[7] << 24) |
		entry.dataLength[4] | (entry.dataLength[5] << 8);
	fileSystem->bitmapFirstCluster = firstCluster;
	fileSystem->bitmapIndex = entry.bitmapFlags & 1;
	fileSystem->bitmapSize = ((SceUInt64)lengthHigh << 32) | lengthLow;
	return result;
}

extern int hasCachedFreeClusterCount(SceExfatfsFileSystem *fileSystem);
extern void cacheFreeClusterCount(SceExfatfsFileSystem *fileSystem, SceUInt32 count);
extern SceUInt64 clusterToDeviceSector(SceUInt32 cluster,
	SceExfatfsFileSystem *fileSystem);
extern SceUInt32 UVFAT_NextCluster(SceExfatfsSharedResource *sharedResource,
	SceUInt32 cluster, SceExfatfsFileSystem *fileSystem);
extern int UVFAT_ReadDevice(SceExfatfsFileSystem *fileSystem, SceUInt64 sector,
	SceSize size, void *buffer);
extern int UVFAT_WriteDevice(SceExfatfsFileSystem *fileSystem, SceUInt64 sector,
	SceSize size, const void *buffer);
extern void *allocateFatBuffer(unsigned int size);

int UVFAT_readallocationbitmap(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem)
{
	SceUInt32 cluster, sectorCount, remaining, tailBytes, clusterCount;
	SceUInt32 sectorSize, readSize, bytesToCount = 1;
	SceUInt64 sector = (SceUInt64)-1, clusterSector;
	unsigned char *lastBuffer = NULL;
	int result;

	if (hasCachedFreeClusterCount(fileSystem))
		return 0;
	if (!fileSystem->bitmapFirstCluster) {
		result = _check_allocation_bitmap(sharedResource, fileSystem);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 141, "%s: can not find allocation bitmap entry.\n",
				"UVFAT_readallocationbitmap");
			return result;
		}
	}
	fileSystem->bitmapFreeClusterCount = 0;
	if (fileSystem->bitmapBuffer) {
		if (fileSystem->bitmapDirty) {
			result = UVFAT_WriteDevice(fileSystem, fileSystem->bitmapDeviceSector,
				fileSystem->sectorSize, fileSystem->bitmapBuffer);
			if (result)
				return result;
			fileSystem->bitmapDirty = 0;
		}
	} else {
		fileSystem->bitmapAllocation = allocateFatBuffer(fileSystem->sectorSize + 512);
		if (!fileSystem->bitmapAllocation) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 159, "%s: salloc failed\n",
				"UVFAT_readallocationbitmap");
			return -10;
		}
		fileSystem->bitmapDirty = 0;
		fileSystem->bitmapBuffer = (void *)(((uintptr_t)fileSystem->bitmapAllocation + 511) & ~511U);
	}
	sectorSize = fileSystem->sectorSize;
	cluster = fileSystem->bitmapFirstCluster;
	sectorCount = (fileSystem->bitmapSize + (sectorSize - 1)) / sectorSize;
	tailBytes = fileSystem->bitmapSize % sectorSize;
	clusterCount = fileSystem->clusterCount;
	remaining = sectorCount;
	readSize = sectorSize;
	for (;;) {
		unsigned int sectorIndex;

		if (cluster <= 1 || cluster > fileSystem->ClusterMAX) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 187, "Ellegal cluster no found(%ld)\n", cluster);
			return -15;
		}
		clusterSector = clusterToDeviceSector(cluster, fileSystem);
		cluster = UVFAT_NextCluster(sharedResource, cluster, fileSystem);
		for (sectorIndex = 0; sectorIndex < fileSystem->clusterSectorCount && remaining;
			sectorIndex = (unsigned char)(sectorIndex + 1)) {
			unsigned char *buffer;
			unsigned int byteIndex;

			sector = clusterSector + (unsigned char)sectorIndex;
			result = UVFAT_ReadDevice(fileSystem, sector, readSize,
				fileSystem->bitmapBuffer);
			if (result)
				return result;
			lastBuffer = fileSystem->bitmapBuffer;
			if (remaining == 1 && tailBytes)
				readSize = tailBytes;
			bytesToCount = readSize;
			buffer = lastBuffer;
			for (byteIndex = 0; byteIndex != bytesToCount; byteIndex++) {
				unsigned int value = *buffer++;

				if (!value) {
					fileSystem->bitmapFreeClusterCount += 8;
					continue;
				}
				if (!(value & 1)) {
					fileSystem->bitmapFreeClusterCount++;
					value = buffer[-1];
				}
				if (!(value & 2)) {
					fileSystem->bitmapFreeClusterCount++;
					value = buffer[-1];
				}
				if (!(value & 4)) {
					fileSystem->bitmapFreeClusterCount++;
					value = buffer[-1];
				}
				if (!(value & 8)) {
					fileSystem->bitmapFreeClusterCount++;
					value = buffer[-1];
				}
				if (!(value & 0x10)) {
					fileSystem->bitmapFreeClusterCount++;
					value = buffer[-1];
				}
				if (!(value & 0x20)) {
					fileSystem->bitmapFreeClusterCount++;
					value = buffer[-1];
				}
				if (!(value & 0x40)) {
					fileSystem->bitmapFreeClusterCount++;
					value = buffer[-1];
				}
				if (!(value >> 7))
					fileSystem->bitmapFreeClusterCount++;
			}
			remaining--;
		}
		if (!remaining)
			break;
	}
	if ((clusterCount & 7) && lastBuffer) {
		unsigned char *tail = lastBuffer + bytesToCount - 1;
		unsigned int bit;

		for (bit = clusterCount & 7; bit != 8; bit++) {
			if (!((*tail >> bit) & 1))
				fileSystem->bitmapFreeClusterCount--;
		}
	}
	fileSystem->bitmapDeviceSector = sector;
	fileSystem->bitmapNextCluster = cluster;
	cacheFreeClusterCount(fileSystem, fileSystem->bitmapFreeClusterCount);
	return 0;
}

int updateallocbitmaps(SceExfatfsFileSystem *fileSystem)
{
	int result = fileSystem->bitmapDirty;

	if (!result)
		return result;
	if (!fileSystem->bitmapBuffer) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 286, "updateallocbitmaps: allocbitmap_bp is NULL.\n");
		return -15;
	}
	result = UVFAT_WriteDevice(fileSystem, fileSystem->bitmapDeviceSector,
		fileSystem->sectorSize, fileSystem->bitmapBuffer);
	if (!result)
		fileSystem->bitmapDirty = 0;
	return result;
}

extern void accountAllocatedCluster(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster);
extern void accountFreedCluster(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster);

int UVFAT_allocationbitmapentry(SceExfatfsSharedResource *sharedResource,
	int mode, SceExfatfsFileSystem *fileSystem, SceUInt32 cluster, SceUInt32 *value)
{
	SceExfatfsBootSector *boot;
	unsigned char sectorShift, bitmapClusterShift;
	SceUInt32 bitIndex, bitmapClusterIndex, bitmapCluster, byteIndex, bitMask;
	SceUInt64 sector;
	unsigned int index;
	int result;

	if (fileSystem->nextFreeCluster < fileSystem->firstClusterOfRoot)
		fileSystem->nextFreeCluster = fileSystem->firstClusterOfRoot;
	if (cluster <= 1 || cluster > fileSystem->clusterCount + 1)
		return -32;
	boot = fileSystem->PBRBuf;
	bitIndex = cluster - 2;
	sectorShift = boot->exfat.bytesPerSectorShift;
	bitmapClusterShift = sectorShift + boot->exfat.sectorsPerClusterShift + 3;
	bitmapClusterIndex = (bitIndex >> bitmapClusterShift) + 1;
	bitmapCluster = fileSystem->bitmapFirstCluster;
	if (!bitmapCluster) {
		result = _check_allocation_bitmap(sharedResource, fileSystem);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 493, "%s: can not find allocation bitmap entry.\n",
				"UVFAT_allocationbitmapentry");
			return result;
		}
		bitmapCluster = fileSystem->bitmapFirstCluster;
	}
	for (index = 1; index < bitmapClusterIndex; index++) {
		bitmapCluster = UVFAT_NextCluster(NULL, bitmapCluster, fileSystem);
		if (bitmapCluster <= 1 || bitmapCluster >= fileSystem->ClusterMAX) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 510,
				"allocation bitmap's fat cluster chain is invalid.");
			return -15;
		}
	}
	byteIndex = (bitIndex & ((1U << (sectorShift + 3)) - 1)) >> 3;
	sector = (SceUInt64)fileSystem->dataAreaStartSector +
		((bitIndex & ((1U << bitmapClusterShift) - 1)) >> (sectorShift + 3));
	sector += (SceUInt64)fileSystem->clusterSectorCount * (bitmapCluster - 2);
	if (fileSystem->bitmapBuffer) {
		if (fileSystem->bitmapDeviceSector == sector)
			goto entry;
		if (fileSystem->bitmapDirty) {
			result = UVFAT_WriteDevice(fileSystem, fileSystem->bitmapDeviceSector,
				fileSystem->sectorSize, fileSystem->bitmapBuffer);
			if (result)
				return result;
			fileSystem->bitmapDirty = 0;
		}
	} else {
		fileSystem->bitmapAllocation = allocateFatBuffer((1U << sectorShift) + 512);
		if (!fileSystem->bitmapAllocation) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 537, "%s: salloc failed\n",
				"UVFAT_allocationbitmapentry");
			return -10;
		}
		fileSystem->bitmapDirty = 0;
		fileSystem->bitmapBuffer = (void *)(((uintptr_t)fileSystem->bitmapAllocation + 511) & ~511U);
		if (!fileSystem->bitmapBuffer) {
#if FW_DEBUG
			ksceKernelPrintfLevelWithInfo(1, 0,
				SCE_DBGINFO_FUNC(365, "flush_and_read_allocationbitmap_entry"),
				"allocbitmap_bp == NULL\n");
#endif
			return -15;
		}
	}
	result = UVFAT_ReadDevice(fileSystem, sector, 1U << sectorShift,
		fileSystem->bitmapBuffer);
	if (result)
		return result;
	fileSystem->bitmapDeviceSector = sector;
entry:
	bitMask = 1U << (bitIndex & 7);
	if (mode & 1) {
		if (((unsigned char *)fileSystem->bitmapBuffer)[byteIndex] & bitMask) {
			*value = 1;
			if (fileSystem->nextFreeCluster == cluster)
				fileSystem->nextFreeCluster = cluster + 1;
		} else {
			*value = 0;
			if (cluster < fileSystem->nextFreeCluster)
				fileSystem->nextFreeCluster = cluster;
		}
	}
	if (mode & 2) {
		unsigned char *buffer = fileSystem->bitmapBuffer;

		if (buffer[byteIndex] & (unsigned char)bitMask) {
			buffer[byteIndex] &= ~(unsigned char)bitMask;
			fileSystem->bitmapDirty = 1;
			accountFreedCluster(fileSystem, cluster);
		}
	}
	if (mode & 4) {
		unsigned char *buffer = fileSystem->bitmapBuffer;

		if (!(buffer[byteIndex] & (unsigned char)bitMask)) {
			buffer[byteIndex] |= bitMask;
			fileSystem->bitmapDirty = 1;
			accountAllocatedCluster(fileSystem, cluster);
		}
	}
	fileSystem->bitmapNextCluster = cluster;
	return 0;
}
