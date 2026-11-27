/*
 * MemoryFatFs (in updater_core, 0x8100de68..0x810104a3): builds a FAT12/FAT16/FAT32
 * filesystem image in memory ("Updater Memory File System") with BPB, FSInfo, 8.3/LFN
 * directory entries and cluster chains for differential os0:/vs0: updates. The class
 * and method names are ours.
 */
#ifndef UPDATER_CORE_MEMORY_FAT_FS_H
#define UPDATER_CORE_MEMORY_FAT_FS_H

#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include "updater_object.h"

enum MemoryFatType {
	MEMORY_FAT_TYPE_12	= 1,
	MEMORY_FAT_TYPE_16	= 16,
	MEMORY_FAT_TYPE_32	= 256,
};

/* 1152 bytes (0x480) */
class MemoryFatFs : public UpdaterObject {
public:
	MemoryFatFs();
	virtual ~MemoryFatFs();

	int ReadSectors(void *dst, SceUInt32 startSector, SceUInt32 sectorCount, SceSize dstSize);
	int WriteSectors(const void *src, SceUInt32 startSector, SceUInt32 sectorCount);
	int FormatVolume(SceUInt32 sectorSize, SceUInt32 clusterSize, void *buffer,
			 SceUInt64 totalSize, const char *volumeLabel);
	int Initialize(SceUInt32 sectorSize, SceUInt32 clusterSize, SceUInt64 totalSize,
		       const char *volumeLabel, SceUInt32 serialNumber, void *buffer);
	int Finalize(void);
	SceUInt32 ClusterToSector(SceUInt32 cluster);
	SceUInt8 ComputeShortNameChecksum(void);
	int SetFatEntry(SceUInt32 cluster, SceUInt32 value);
	SceUInt32 AllocateCluster(int *error);
	int WriteDirectoryEntry(const char *name, const SceIoStat *stat, int needsLfn,
				int lfnLength, SceUInt32 timestamp, SceUInt32 firstCluster);
#ifdef UPDATER_CORE_2014
	__attribute__((noinline)) bool IsEndOfClusterChain(SceUInt32 cluster);
#else
	bool IsEndOfClusterChain(SceUInt32 cluster);
#endif
	SceUInt32 GetFatEntry(SceUInt32 cluster);
	int ShortNameExistsInCurrentDir(const void *shortName);
	int MakeShortAndLongName(const char *name, int *lfnLength);
	int CreateDirectory(const char *name, const SceIoStat *stat, SceUInt32 *outCluster);
	int AddFileFromPath(const char *name, const char *srcPath, const SceIoStat *stat);
	int AddFileFromMemory(const char *name, const SceIoStat *stat, const void *fileData);
	void SetDirectoryContext(SceUInt64 entryIndex, SceUInt64 currentCluster,
				 SceUInt64 dirStartCluster);
	void GetDirectoryContext(SceUInt64 *entryIndex, SceUInt64 *currentCluster,
				 SceUInt64 *dirStartCluster);
	SceUInt64 GetUsedImageSize(void);

private:
	char m_shortName[12];
	SceUInt16 m_longName[256];
	SceUInt32 m_unusedState;
	SceUInt8 *m_clusterBuffer;
	SceUInt8 *m_fatBuffer;
	SceUInt8 m_sectorBuffer[512];
	SceUInt32 m_serialNumber;
	SceUInt32 m_fatStartSector;
	SceUInt32 m_rootDirStartSector;
	SceUInt32 m_dataStartSector;
	SceUInt32 m_sectorsPerFat;
	SceUInt32 m_sectorsPerCluster;
	SceUInt32 m_totalClusters;
	SceUInt32 m_totalSectors;
	SceUInt32 m_fatType;
	SceUInt32 m_sectorSize;
	SceUInt32 m_clusterSize;
	SceUInt64 m_dirEntryIndex;
	SceUInt64 m_currentDirCluster;
	SceUInt64 m_dirStartCluster;
	SceUInt64 m_nextFreeCluster;
	SceUInt32 m_previousFatType;
	SceUInt8 *m_imageBuffer;
	SceUInt32 m_imageSize;
	SceUInt64 m_usedImageSize;
};

#endif
