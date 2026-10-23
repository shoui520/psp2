#ifndef APPMGR_SAVEDATA_SLOT_H
#define APPMGR_SAVEDATA_SLOT_H

#include <scetypes.h>
#include <appmgr/appmgr_savedata.h>
#include <stddef.h>

typedef struct AppmgrSaveDataSlotInitParam {
	SceSize size;
	SceUInt32 slotCount;
	SceUInt32 flags;
	SceUInt8 unused[32];
	SceAppUtilMountPoint mountPoint;
} AppmgrSaveDataSlotInitParam;

typedef struct AppmgrSaveDataSlotFileHeader {
	char magic[4];
	SceUInt32 version;
	SceUInt32 slotCount;
	SceUInt8 unused[500];
	SceUInt8 occupied[512];
} AppmgrSaveDataSlotFileHeader;

/* a mounted host0 save data (SceAppMgrForDriver_E73C9516), in a list */
typedef struct AppmgrSaveDataMount {
	char titleId[16];
	SceUID processId;
	char mountPoint[16];
	struct AppmgrSaveDataMount *next;
} AppmgrSaveDataMount;

_Static_assert(sizeof(AppmgrSaveDataMount) == 40, "AppmgrSaveDataMount");

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

typedef struct AppmgrSaveDataSlotStatusParam {
	SceSize size;
	SceUInt32 slotId;
	SceInt32 status;
	SceAppUtilMountPoint mountPoint;
} AppmgrSaveDataSlotStatusParam;

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
void appmgrNotifySaveDataSlotRead(SceUInt32 slotId);
void *appmgrAllocSaveDataHeapMemory(SceSize size);
int appmgrFreeSaveDataHeapMemory(void *memory);

extern int g_appmgrAppInfoCacheKey;

/* _sceAppMgrPspSaveDataGetParams' buffer: SceAppUtilPspSaveDataParamSfo with a longer directory */
typedef struct AppmgrPspSaveDataParamSfo {
	char saveDataDirectory[64];
	char title[128];
	char saveDataTitle[128];
	char detail[1024];
	SceUInt8 parentalLev;
	char reserved[3];
} AppmgrPspSaveDataParamSfo;

_Static_assert(sizeof(AppmgrPspSaveDataParamSfo) == 1348, "PSP save data PARAM.SFO parameters");

/* _sceAppMgrPspSaveDataRead's parameters: SceAppUtilPspSaveDataParams, the file ID copied in */
typedef struct AppmgrPspSaveDataParams {
	char dirName[128];
	char fileName[13];
	char reserved[3];
	SceUInt32 fileType;
	SceUInt32 dataVersion;
	SceUInt8 secureFileId[16];
	AppmgrPspSaveDataParamSfo *paramSfo;
	void *dataBuf;
	SceSize dataBufSize;
} AppmgrPspSaveDataParams;

_Static_assert(sizeof(AppmgrPspSaveDataParams) == 180, "PSP save data read parameters");
_Static_assert(offsetof(AppmgrPspSaveDataParams, fileType) == 144, "PSP save data file type");
_Static_assert(offsetof(AppmgrPspSaveDataParams, paramSfo) == 168, "PSP save data PARAM.SFO buffer");

int sceAppMgrPspSaveDataGetParams(AppmgrPspSaveDataParamSfo *paramSfo);
int sceAppMgrPspSaveDataRead(AppmgrPspSaveDataParams *params);

#endif
