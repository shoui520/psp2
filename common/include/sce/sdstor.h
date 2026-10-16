#ifndef SCE_SDSTOR_H
#define SCE_SDSTOR_H

#include <stdint.h>
#include <stddef.h>

/* Complete response to SceSdstor's VFS device-control command 1. */
typedef struct SceSdstorPartitionInfo {
	uint32_t startSector;
	uint32_t sectorCount;
	uint32_t eraseBlockCount;
	uint8_t writeProtected;
	uint8_t memoryCardPort;
	uint8_t reserved[2];
} SceSdstorPartitionInfo;

_Static_assert(sizeof(SceSdstorPartitionInfo) == 16,
	       "SceSdstorPartitionInfo");
_Static_assert(offsetof(SceSdstorPartitionInfo, eraseBlockCount) == 8,
	       "SceSdstorPartitionInfo eraseBlockCount");

#endif
