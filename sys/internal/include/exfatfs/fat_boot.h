/* FAT/exFAT boot-sector wire records shared by filesystem and media drivers. */
#pragma once

#include <scetypes.h>

typedef struct __attribute__((packed)) SceExfatfsFatBootParameters {
	unsigned char jump[3];
	unsigned char oemName[8];
	SceUInt16 bytesPerSector;
	unsigned char sectorsPerCluster;
	SceUInt16 reservedSectorCount;
	unsigned char numberOfFats;
	SceUInt16 rootEntryCount;
	SceUInt16 totalSectors16;
	unsigned char media;
	SceUInt16 fatSectors16;
	SceUInt16 sectorsPerTrack;
	SceUInt16 numberOfHeads;
	SceUInt32 hiddenSectors;
	SceUInt32 totalSectors32;
} SceExfatfsFatBootParameters;

typedef union SceExfatfsBootSector {
	struct __attribute__((packed)) {
		SceExfatfsFatBootParameters parameters;
		unsigned char driveNumber;
		unsigned char reserved;
		unsigned char extendedSignature;
		unsigned char volumeSerial[4];
		unsigned char volumeLabel[11];
		unsigned char fileSystemName[8];
		unsigned char bootCode[448];
		SceUInt16 signature;
	} fat16;
	struct __attribute__((packed)) {
		SceExfatfsFatBootParameters parameters;
		SceUInt32 fatSectors32;
		SceUInt16 extendedFlags;
		SceUInt16 version;
		SceUInt32 rootCluster;
		SceUInt16 fsInfoSector;
		SceUInt16 backupBootSector;
		unsigned char reserved[12];
		unsigned char driveNumber;
		unsigned char reservedDrive;
		unsigned char extendedSignature;
		SceUInt32 volumeSerial;
		unsigned char volumeLabel[11];
		unsigned char fileSystemName[8];
		unsigned char bootCode[420];
		SceUInt16 signature;
	} fat32;
	struct __attribute__((packed)) {
		unsigned char jump[3];
		unsigned char fileSystemName[8];
		unsigned char mustBeZero[53];
		SceUInt64 partitionOffset;
		SceUInt64 volumeLength;
		SceUInt32 fatOffset;
		SceUInt32 fatLength;
		SceUInt32 clusterHeapOffset;
		SceUInt32 clusterCount;
		SceUInt32 rootCluster;
		unsigned char volumeSerial[4];
		SceUInt16 revision;
		SceUInt16 volumeFlags;
		unsigned char bytesPerSectorShift;
		unsigned char sectorsPerClusterShift;
		unsigned char numberOfFats;
		unsigned char driveSelect;
		unsigned char percentInUse;
		unsigned char reserved[7];
		unsigned char bootCode[390];
		SceUInt16 signature;
	} exfat;
} SceExfatfsBootSector;

_Static_assert(sizeof(SceExfatfsFatBootParameters) == 36,
	"SceExfatfsFatBootParameters");
_Static_assert(sizeof(SceExfatfsBootSector) == 512,
	"SceExfatfsBootSector");
