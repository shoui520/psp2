/* Complete AppMgr formatter owners recovered from original constructors/readers. */
#pragma once

#include <scetypes.h>
#include <stdint.h>

typedef struct AppmgrFatFormatArguments {
	uint64_t volumeSize;
	const char *deviceName;
	void *buffer;
	uint32_t bufferSize;
	uint32_t clusterSize;
	uint32_t initialImageSize;
	uint32_t writtenDataSize;
	uint32_t serial;
	const char *volumeLabel;
	uint32_t phase;
	uint32_t type;
} AppmgrFatFormatArguments;

typedef int (*AppmgrFatFormatNotification)(AppmgrFatFormatArguments *arguments);

typedef struct AppmgrFatFormatAlternatePartition {
	uint64_t startLba;
	uint64_t sectorCount;
	uint32_t reservedSectors;
	uint32_t fatSectors;
	uint32_t reserved;
	uint32_t clusterCount;
	uint32_t clusterSize;
	uint32_t upcaseTableSize;
	uint32_t bitmapCluster1;
	uint32_t bitmapCluster2;
	uint32_t upcaseCluster;
	uint32_t rootCluster;
	uint32_t firstUnusedCluster;
	uint32_t reservedTail;
	uint16_t rootEntryCount;
	uint8_t fatCount;
	uint8_t type;
} AppmgrFatFormatAlternatePartition;

typedef struct AppmgrFatFormatContext {
	AppmgrFatFormatArguments *arguments;
	AppmgrFatFormatNotification notification;
	uint32_t reserved;
	SceUID file;
	uint64_t currentLba;
	uint32_t writeAlignment;
	uint32_t startLba;
	uint64_t sectorCount;
	uint32_t fatSectors;
	uint32_t clusterCount;
	uint32_t rootCluster;
	uint16_t bytesPerSector;
	uint16_t legacyReservedSectors;
	uint16_t rootEntryCount;
	uint8_t fatCount;
	uint8_t type;
	uint32_t reservedSectors;
	uint32_t upcaseTableSize;
	uint32_t bitmapCluster1;
	uint32_t bitmapCluster2;
	uint32_t upcaseCluster;
	uint32_t firstUnusedCluster;
	AppmgrFatFormatAlternatePartition alternate;
} AppmgrFatFormatContext;

typedef struct AppmgrFat32FsInfoSector {
	uint8_t leadSignature[4];
	uint8_t reserved0[480];
	uint8_t structureSignature[4];
	uint8_t freeClusterCount[4];
	uint8_t nextFreeCluster[4];
	uint8_t reserved1[12];
	uint8_t trailSignature[4];
} AppmgrFat32FsInfoSector;

_Static_assert(sizeof(AppmgrFatFormatArguments) == 48, "formatter arguments");
_Static_assert(sizeof(AppmgrFatFormatAlternatePartition) == 72,
	"alternate formatter partition");
_Static_assert(sizeof(AppmgrFatFormatContext) == 160, "formatter context");
_Static_assert(sizeof(AppmgrFat32FsInfoSector) == 512, "FAT32 FSInfo sector");

#if FW_DEBUG
int error_notification(AppmgrFatFormatArguments *const *arguments,
	AppmgrFatFormatNotification *notification, int result);
#endif

int _prepare_FAT_PBR(AppmgrFatFormatContext *context);
int zero_fill(AppmgrFatFormatContext *context, unsigned int bufferOffset,
	unsigned int length, uint64_t byteOffset, int index);

unsigned int appmgrPackFatDateTime(const SceDateTime *time);
int FatFormat(AppmgrFatFormatArguments *arguments);
