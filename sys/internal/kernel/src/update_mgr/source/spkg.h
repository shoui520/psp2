#ifndef UPDATE_MGR_SPKG_H
#define UPDATE_MGR_SPKG_H

#include <stdint.h>
#include <stddef.h>
#include <psp2kern/types.h>

typedef struct SpkgThreadArgs {
	uint32_t nodeType;
	int packageType;
	void *buffer;
	SceSize size;
	uint32_t flags;
	uint32_t requestId;
	SceUID pid;
} SpkgThreadArgs;

_Static_assert(sizeof(SpkgThreadArgs) == 28, "s-package thread request size");

/* The package-info record is separate from the following chunk-info record. */
typedef struct SpkgPackageInfo {
	uint32_t version;
	uint32_t type;
	uint64_t hardwareInfo;
	uint64_t updateVersion;
	uint64_t finalSize;
	uint64_t decryptedSize;
	uint32_t requirements;
	uint32_t platform;
	uint32_t downgradeBarrier;
	uint32_t baseVersion;
	uint8_t description[8];
} SpkgPackageInfo;

_Static_assert(sizeof(SpkgPackageInfo) == 64, "s-package information record size");
_Static_assert(offsetof(SpkgPackageInfo, updateVersion) == 16 &&
	       offsetof(SpkgPackageInfo, requirements) == 40 &&
	       offsetof(SpkgPackageInfo, baseVersion) == 52,
	       "s-package information record layout");

/* Descriptions are serialized secure-service output, not kernel-owned fields. */
typedef struct SpkgChunkInfo {
	uint8_t description[16];
	uint64_t offset;
	uint64_t size;
	uint64_t chunkIndex;
	uint64_t chunkCount;
	uint8_t extension[16];
} SpkgChunkInfo;

_Static_assert(sizeof(SpkgChunkInfo) == 64, "s-package chunk information size");
_Static_assert(offsetof(SpkgChunkInfo, offset) == 16 &&
	       offsetof(SpkgChunkInfo, chunkCount) == 40,
	       "s-package chunk information layout");

#endif
