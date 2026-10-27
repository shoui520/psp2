/*
 * NSKBL: SceSfat, the boot loader's read-only FAT driver (Sony's name from its heap,
 * "SceSfat"; its file and function names are not known: these are ours). A volume reads
 * its sectors through the callback its mounter gives; one file is open at a time.
 */
#pragma once

#include <scetypes.h>

/* the mount flags */
#define SFAT_MOUNT_NO_MBR	0x00008000	/* the volume starts at sector 0 */
#define SFAT_MOUNT_SCE_MBR	0x00010000	/* the active partition of Sony's MBR */
#define SFAT_MOUNT_CACHE_FAT	0x00100000	/* the whole FAT read at mount */

/* reads count sectors from sector on into buffer */
typedef int (*SfatReadSectors)(void *argument, SceUInt32 sector, SceUInt32 count, void *buffer);

/* name unknown: a mounted volume */
typedef struct SfatVolume {
	SceUInt32 flags;
	SceUInt32 fatType;		/* 0x04: 0 FAT12, 1 FAT16, 2 FAT32 */
	void *fatCacheBlock;		/* 0x08: the FAT, SFAT_MOUNT_CACHE_FAT */
	void *fatCache;			/* 0x0c: the same, 32-byte aligned */
	void *clusterBlock;		/* 0x10: one cluster */
	void *cluster;			/* 0x14: the same, 32-byte aligned */
	SceUInt32 partitionStart;	/* 0x18: sectors */
	SceUInt32 reservedSectors;	/* 0x1c */
	SceUInt32 fatStart;		/* 0x20 */
	SceUInt32 fatSectors;		/* 0x24 */
	SceUInt32 fatCount;		/* 0x28 */
	SceUInt32 rootCluster;		/* 0x2c: FAT32 */
	SceUInt32 rootStart;		/* 0x30 */
	SceUInt32 rootEntries;		/* 0x34: FAT12 and FAT16 */
	SceUInt32 dataStart;		/* 0x38 */
	SceUInt32 totalSectors;		/* 0x3c */
	SceUInt32 clusterCount;		/* 0x40 */
	SceUInt32 freeHint;		/* 0x44 */
	SceUInt32 sectorsPerCluster;	/* 0x48 */
	SceUInt32 clusterSize;		/* 0x4c */
	SceUInt32 sectorSize;		/* 0x50 */
	SfatReadSectors read;		/* 0x54 */
	void *readArgument;		/* 0x58 */
} SfatVolume;

_Static_assert(sizeof(SfatVolume) == 0x5c, "SfatVolume");

int SfatInit(void);
int SfatTerm(void);
int SfatMount(SfatVolume *volume, SceUInt32 flags, SfatReadSectors read, void *argument);
int SfatUnmount(SfatVolume *volume, int flags);
int SfatOpen(SfatVolume *volume, const char *path, int flags);
int SfatClose(int fd);
int SfatRead(int fd, void *buffer, SceSize size);
int SfatLseek(int fd, int offset, int whence);

/* the volumes kbl_main.c mounts */
extern SfatVolume g_os0Volume;
extern SfatVolume g_sd0Volume;
