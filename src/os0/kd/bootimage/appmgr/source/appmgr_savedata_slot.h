#ifndef APPMGR_SAVEDATA_SLOT_H
#define APPMGR_SAVEDATA_SLOT_H

#include <psp2kern/types.h>
#include <psp2/appmgr.h>
#include <stddef.h>

typedef struct AppmgrSaveDataSlotInitParam {
	SceSize size;
	SceUInt32 slotCount;
	SceUInt32 flags;
	SceUInt8 unused[32];
	char mountPoint[16];
} AppmgrSaveDataSlotInitParam;

typedef struct AppmgrSaveDataSlotFileHeader {
	char magic[4];
	SceUInt32 version;
	SceUInt32 slotCount;
	SceUInt8 unused[500];
	SceUInt8 occupied[512];
} AppmgrSaveDataSlotFileHeader;

typedef struct AppmgrSaveDataSlotCache {
	SceUID processId;
	SceUID fd;
	char mountPoint[16];
	SceUInt32 windowIndex;
	SceSize windowLength;
	AppmgrSaveDataSlotFileHeader *header;
	void *readWindow;
} AppmgrSaveDataSlotCache;

typedef struct AppmgrSaveDataSlotFileInfo {
	SceUInt32 version;
	SceUInt32 slotCount;
	SceUInt8 unused[32];
	SceUInt8 occupied[512];
} AppmgrSaveDataSlotFileInfo;

typedef struct SceAppMgrSaveDataSlotParam {
	SceInt32 status;
	char title[64];
	char subTitle[128];
	char detail[512];
	char iconPath[64];
	SceInt32 userParam;
	SceUInt32 sizeKiB;
	SceDateTime modifiedTime;
	SceUInt8 reserved[228];
} SceAppMgrSaveDataSlotParam;

typedef struct AppmgrSaveDataSlotKernelParam {
	SceSize size;
	SceUInt32 slotId;
	SceAppMgrSaveDataSlotParam param;
	char mountPoint[16];
} AppmgrSaveDataSlotKernelParam;

typedef struct AppmgrSaveDataSlotStatusParam {
	SceSize size;
	SceUInt32 slotId;
	SceInt32 status;
	char mountPoint[16];
} AppmgrSaveDataSlotStatusParam;

typedef struct AppmgrSaveDataOperationParam {
	SceSize size;
	SceUInt32 slotId;
	SceAppMgrSaveDataSlotParam *slotParam;
	SceUInt8 unused[32];
	SceAppUtilSaveDataFile *files;
	SceUInt32 fileNum;
	SceAppUtilSaveDataMountPoint mountPoint;
	SceUInt32 flags;
	SceUInt32 requiredSizeKiB;
} AppmgrSaveDataOperationParam;

_Static_assert(sizeof(AppmgrSaveDataOperationParam) == 76,
	       "native save-data operation arguments");
_Static_assert(offsetof(AppmgrSaveDataOperationParam, slotId) == 4 &&
	       offsetof(AppmgrSaveDataOperationParam, slotParam) == 8 &&
	       offsetof(AppmgrSaveDataOperationParam, unused) == 12 &&
	       offsetof(AppmgrSaveDataOperationParam, files) == 44 &&
	       offsetof(AppmgrSaveDataOperationParam, fileNum) == 48 &&
	       offsetof(AppmgrSaveDataOperationParam, mountPoint) == 52 &&
	       offsetof(AppmgrSaveDataOperationParam, flags) == 68 &&
	       offsetof(AppmgrSaveDataOperationParam, requiredSizeKiB) == 72,
	       "native save-data operation member offsets");

_Static_assert(sizeof(AppmgrSaveDataSlotStatusParam) == 28,
	       "save-data slot status arguments");
_Static_assert(offsetof(AppmgrSaveDataSlotStatusParam, slotId) == 4,
	       "save-data status slot identifier");
_Static_assert(offsetof(AppmgrSaveDataSlotStatusParam, status) == 8,
	       "save-data slot status");
_Static_assert(offsetof(AppmgrSaveDataSlotStatusParam, mountPoint) == 12,
	       "save-data status mount point");

typedef struct AppmgrAddContentEntry {
	SceInt32 mountSelector;
	char contentId[17];
} AppmgrAddContentEntry;

typedef struct SceAppMgrAppInfoCache {
	char titleId[16];
	SceUInt32 safeMemoryFlags;
	SceInt32 safeMemoryInitialized;
	SceInt32 safeMemoryDirty;
	void *safeMemory;
	SceSize safeMemorySize;
	char safeMemoryDirectory[64];
	SceUInt32 saveDataFlags;
	SceInt32 neededSaveDataKiB;
	char psmContentId[48];
	char psmTitle[128];
	SceInt32 addContentCapacity;
	AppmgrAddContentEntry addContent[16];
	SceInt32 registeredPathKind;
	char registeredPath[64];
	SceUInt32 writtenSinceNoticeKiB;
	SceUInt32 totalWrittenKiB;
	SceUInt8 unused[276];
} SceAppMgrAppInfoCache;

_Static_assert(sizeof(AppmgrAddContentEntry) == 24,
	       "add-content cache entry");
_Static_assert(offsetof(AppmgrAddContentEntry, contentId) == 4,
	       "add-content identifier");
_Static_assert(sizeof(SceAppMgrAppInfoCache) == 1024,
	       "process application information cache");
_Static_assert(offsetof(SceAppMgrAppInfoCache, safeMemoryFlags) == 16,
	       "safe-memory flags");
_Static_assert(offsetof(SceAppMgrAppInfoCache, safeMemory) == 28,
	       "safe-memory buffer");
_Static_assert(offsetof(SceAppMgrAppInfoCache, safeMemoryDirectory) == 36,
	       "safe-memory directory");
_Static_assert(offsetof(SceAppMgrAppInfoCache, saveDataFlags) == 100,
	       "save-data flags");
_Static_assert(offsetof(SceAppMgrAppInfoCache, psmContentId) == 108,
	       "PSM content identifier");
_Static_assert(offsetof(SceAppMgrAppInfoCache, psmTitle) == 156,
	       "PSM title");
_Static_assert(offsetof(SceAppMgrAppInfoCache, addContentCapacity) == 284,
	       "add-content cache capacity");
_Static_assert(offsetof(SceAppMgrAppInfoCache, addContent) == 288,
	       "add-content cache");
_Static_assert(offsetof(SceAppMgrAppInfoCache, registeredPathKind) == 672,
	       "registered path kind");
_Static_assert(offsetof(SceAppMgrAppInfoCache, registeredPath) == 676,
	       "registered path");
_Static_assert(offsetof(SceAppMgrAppInfoCache, writtenSinceNoticeKiB) == 740,
	       "write-notice counter");
_Static_assert(offsetof(SceAppMgrAppInfoCache, totalWrittenKiB) == 744,
	       "total write counter");
_Static_assert(offsetof(SceAppMgrAppInfoCache, unused) == 748,
	       "unused application information cache range");

_Static_assert(sizeof(SceAppMgrSaveDataSlotParam) == 1024,
	       "native save-data slot parameters");
_Static_assert(offsetof(SceAppMgrSaveDataSlotParam, title) == 4,
	       "save-data slot title");
_Static_assert(offsetof(SceAppMgrSaveDataSlotParam, subTitle) == 68,
	       "save-data slot subtitle");
_Static_assert(offsetof(SceAppMgrSaveDataSlotParam, detail) == 196,
	       "save-data slot detail");
_Static_assert(offsetof(SceAppMgrSaveDataSlotParam, iconPath) == 708,
	       "save-data slot icon path");
_Static_assert(offsetof(SceAppMgrSaveDataSlotParam, userParam) == 772,
	       "save-data slot user parameter");
_Static_assert(offsetof(SceAppMgrSaveDataSlotParam, sizeKiB) == 776,
	       "save-data slot size");
_Static_assert(offsetof(SceAppMgrSaveDataSlotParam, modifiedTime) == 780,
	       "save-data slot modification time");
_Static_assert(offsetof(SceAppMgrSaveDataSlotParam, reserved) == 796,
	       "save-data slot reserved range");
_Static_assert(sizeof(AppmgrSaveDataSlotKernelParam) == 1048,
	       "kernel save-data slot arguments");
_Static_assert(offsetof(AppmgrSaveDataSlotKernelParam, param) == 8,
	       "kernel save-data slot parameters");
_Static_assert(offsetof(AppmgrSaveDataSlotKernelParam, mountPoint) == 1032,
	       "kernel save-data slot mount point");

_Static_assert(sizeof(AppmgrSaveDataSlotFileInfo) == 552,
	       "save-data slot file information");
_Static_assert(offsetof(AppmgrSaveDataSlotFileInfo, slotCount) == 4,
	       "save-data file information slot count");
_Static_assert(offsetof(AppmgrSaveDataSlotFileInfo, occupied) == 40,
	       "save-data file information occupancy table");

_Static_assert(sizeof(AppmgrSaveDataSlotInitParam) == 60,
	       "save-data slot initialization parameters");
_Static_assert(offsetof(AppmgrSaveDataSlotInitParam, slotCount) == 4,
	       "save-data initialization slot count");
_Static_assert(offsetof(AppmgrSaveDataSlotInitParam, flags) == 8,
	       "save-data initialization flags");
_Static_assert(offsetof(AppmgrSaveDataSlotInitParam, mountPoint) == 44,
	       "save-data initialization mount point");
_Static_assert(sizeof(AppmgrSaveDataSlotFileHeader) == 1024,
	       "save-data slot file header");
_Static_assert(offsetof(AppmgrSaveDataSlotFileHeader, version) == 4,
	       "save-data slot file version");
_Static_assert(offsetof(AppmgrSaveDataSlotFileHeader, slotCount) == 8,
	       "save-data slot file count");
_Static_assert(offsetof(AppmgrSaveDataSlotFileHeader, occupied) == 512,
	       "save-data slot occupancy table");
_Static_assert(sizeof(AppmgrSaveDataSlotCache) == 40,
	       "save-data slot cache");
_Static_assert(offsetof(AppmgrSaveDataSlotCache, fd) == 4,
	       "save-data slot cache file descriptor");
_Static_assert(offsetof(AppmgrSaveDataSlotCache, mountPoint) == 8,
	       "save-data slot cache mount point");
_Static_assert(offsetof(AppmgrSaveDataSlotCache, windowIndex) == 24,
	       "save-data slot cache window index");
_Static_assert(offsetof(AppmgrSaveDataSlotCache, windowLength) == 28,
	       "save-data slot cache window length");
_Static_assert(offsetof(AppmgrSaveDataSlotCache, header) == 32,
	       "save-data slot cache header");
_Static_assert(offsetof(AppmgrSaveDataSlotCache, readWindow) == 36,
	       "save-data slot cache read window");

extern AppmgrSaveDataSlotCache g_appmgrSaveDataSlotCache;
int appmgrInitializeSaveDataSlotCache(void);
int appmgrResetSaveDataSlotCache(SceUID pid);

#endif
