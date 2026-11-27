#ifndef ISOFS_INT_H
#define ISOFS_INT_H

/* Copyright (C) 2011 - 2024 The uOFW team
   See the file COPYING for copying permission.
*/

#include <common_imp.h>

#include <interruptman.h>
#include <sysmem_kernel.h>
#include <sysmem_sysclib.h>
#include <iofilemgr_kernel.h>
#include <mediaman_kernel.h>
#include <umd_error.h>
#include <umdman.h>
#include <rtc.h>

SCE_MODULE_INFO("sceIsofs_driver", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD |
                SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 5);
SCE_MODULE_BOOTSTART("isofsInit");
SCE_MODULE_REBOOT_BEFORE("isofsRebootBefore");
SCE_SDK_VERSION(SDK_VERSION);

#define UMD_ID_SIZE 32

#define ISOFS_CACHE_ENTRIES 0x10

#define DEVCTL_GET_UNIT_NUM 0x1e080a8
#define DEVCTL_GET_PVD 0x1e28035
#define DEVCTL_READ_SECTORS 0x1e380c0
#define DEVCTL_GET_SECTOR_SIZE 0x1e280a9
#define DEVCTL_SEEK_DISC_RAW 0x1f100a3
#define DEVCTL_UMD_CTRL_EXT 0x1e180d3
#define DEVCTL_QUERY_BLOCK_DEV 0x1e38034
#define DEVCTL_PREPARE_INTO_CACHE 0x1f100a4
#define DEVCTL_PREPARE_INTO_CACHE_GET_STATUS 0x1f300a5
#define DEVCTL_CACHE_CANCEL 0x1f100a6
#define DEVCTL_CACHE_GET_STATUS 0x1f300a7
#define DEVCTL_CACHE_SUSPEND 0x1f100a8
#define DEVCTL_CACHE_RESUME 0x1f100a9
#define DEVCTL_GET_TOTAL_NUMBER_OF_SECTORS 0x1f20003
#define DEVCTL_GET_CURRENT_LBN 0x1f20002

/**
 * Types
 */

/** ISO date time as defined in the ISO specs. */
typedef struct __attribute__((packed)) {
    u8 yearsSince1900;
    u8 month;
    u8 day;
    u8 hour;
    u8 minute;
    u8 second;
    u8 offset;
} IsoDirectoryDateTime;

/** ISO directory record as defined in the ISO specs.
 * Directory records are used to describe both files and directories. */
typedef struct __attribute__((packed)) {
    u8 recLen;
    u8 earLen;
    u32 lbn;
    u32 lbnMsb;
    u32 size;
    u32 sizeMsb;
    IsoDirectoryDateTime dateTime;
    u8 flags;
    u8 fileUnitSize;
    u8 interleaveGap;
    u16 volumeSequenceNumber;
    u16 volumeSequenceNumberMsb;
    u8 nameLen;
    char name[];
} IsoDirectory;

/** ISO path record as defined in the ISO specs.
 * Path table is used to speed-up lookup for directories. */
typedef struct __attribute__((packed)) {
    u8 nameLen;
    u8 earLen;
    u32 lbn;
    u16 parentNum;
    char name[];
} IsoPath;

/** Represents parsed ISO path record. Most notably it has fixed size name and few additional fields. */
typedef struct {
    u8 nameLen;
    u8 earLen;
    __attribute__((packed)) u32 lbn; // Only this field is packed, the rest is not to avoid unnecessary unaligned loads.
    u16 parentNum;
    char name[32];
    u32 dirNum;
    u32 dirLevel;
    u32 lbnSize;
    u32 reserved;
} IsofsPath;

/** Global manager data for this module. */
typedef struct {
    s32 initialized;
    s32 reserved[2];
    s32 maxUnits;
    s32 maxFiles;
    s32 fdwCount;
    SceUID semaId;
    SceUID heapId;
    SceUID fplId;
    s32 driveType;
} IsofsMgr;

typedef struct IsofsFdw IsofsFdw;
typedef struct IsofsFd IsofsFd;
typedef struct IsofsUnit IsofsUnit;

/** Stores information about opened ISO file. */
typedef struct {
    u32 readFlags;
    u8 openFlags; // 3 = raw open, 2 = normal open, 0 = available
    u8 flags;
    s8 usedFlag;
    u8 reservedByte;
    u16 reservedHalf;
    s16 cacheIndex;
    u32 lbn;
    u32 reservedLbnPad[2];
    SceOff size;
    SceOff fpos;
    ScePspDateTime dateTime;
    u32 reservedDatePad[4];
    ScePspDateTime modifyDateTime;
    SceIoIob *iob;
    IsofsFd *fileDesc;
    char umdId[UMD_ID_SIZE];
} IsofsFile;

typedef struct IsofsDir IsofsDir;

/** Stores information about opened ISO directory. */
struct IsofsDir {
    IsoDirectory *isoDir;
    IsofsPath *paths;
    void *fplData;
    u32 lbnSize;
    u32 dirRecLen;
    u32 lbn;
    char umdId[UMD_ID_SIZE];
    u32 compatFlag;
    IsofsDir *prevDir;
    IsofsDir *nextDir;
};

/** Holds a list of opened file descriptors. A single unit can have more than one list. */
struct IsofsFd {
    u16 reservedHead;
    u16 filesCount;
    u32 idx;
    u32 reservedMid[2];
    SceUID heapId;
    u32 reservedTail;
    IsofsFile *files;
    IsofsFdw *fdw;
    IsofsUnit *unit;
};

/** Wraps IsofsFd with its wrapper index. */
struct IsofsFdw {
    u32 idx;
    u32 reserved;
    IsofsFd fileDesc;
};

/** Represents single device capable of reading ISOs. */
struct IsofsUnit {
    char umdId[UMD_ID_SIZE];
    char blockDev[32];
    SceUID heapId;
    u16 activeFlag;
    s16 cacheSlot; // Always -1
    s16 unitNum;
    u16 defaultFlag; // Always 1
    u16 mountFlags;
    u16 pathTableEntryCount;
    u8 fdwCount;
    u8 reservedByte;
    u16 mountState;
    u32 pathTableSize;
    SceUID semaId;
    u32 pathTableLbn;
    u32 rootDirLbn;
    u32 rootDirSize;
    u32 nextLbn;
    u32 lastLbn;
    u32 remainingLbns;
    u32 totalSectors;
    u32 cachedCurrentLbn;
    u32 reservedTail[5];
    IsofsPath *paths;
    IsofsFdw *fdw;
    IsoPath *pathTable;
    u8 *primaryVolumeDescriptor;
    u32 openState;
    u32 currentLbn;
    u32 sectorSize;
    s32 handlerIdx;
};

/** Functions used by ISOFS unit to interface with the actual device reading the ISO. */
typedef struct {
    s32 (*readDirSectors)(IsofsUnit *, s64, s32, void *, s32, u32);
    s32 (*readSectors)(IsofsUnit *, IsofsFile *, s32, void *, s32, u32);
    void *reservedOp;
    s32 (*getSectorSize)(IsofsUnit *);
    s32 (*getTotalNumberOfSectors)(IsofsUnit *);
    s32 (*getCurrentLbn)(IsofsUnit *);
    s64 (*seek)(IsofsUnit *, s64);
    s32 (*devctlExt)(IsofsUnit *, s32, void *, SceSize, s32, s32);
    s32 (*unsupportedOp)(void);
    s32 (*getUnitNum)(IsofsUnit *, void *, SceSize);
    s32 (*readPvd)(IsofsUnit *, void *, SceSize);
    s32 (*prepareIntoCache)(IsofsUnit *, IsofsFile *, s32, s32);
    s32 (*cacheControl)(IsofsUnit *, IsofsFile *, s32);
    s32 (*cacheSuspend)(IsofsUnit *, IsofsFile *);
    s32 (*cacheResume)(IsofsUnit *, IsofsFile *);
} IsofsImplFuncs;

/** Definition of device reading the ISO. */
typedef struct {
    char blockDev[32];
    const IsofsImplFuncs *funcs;
} IsofsHandler;

/** ISOFS cache entry descriptor. */
typedef struct {
    u32 headerWords[7];
    u32 devctlExt;
    s8 used;
    s8 padByte;
    s16 padHalf;
    u32 tailWords[2];
    void *memory;
} IsofsCacheEntry;

/** Input args for reading with the sceKernelExtendKernelStack function. */
typedef struct {
    IsofsUnit *unit;
    IsofsFile *file;
    u32 flags;
    s32 outsize;
    void *outdata;
    u32 reserved;
} IsofsReadSectorsArgs;

/** Input parameter block for ioctl 0x1010005 (seek disc to file position). */
typedef struct {
    SceOff offset;
    u32 reserved;
    s32 whence;
} IsofsIoctlSeekParam;

/**
 * Declarations
 */

/** module_start function. */
s32 isofsInit();
/** module_reboot_before function. */
s32 isofsRebootBefore();

/** Returns global ISOFS manager. */
IsofsMgr *isofsGetMgr();

/** Initializes all global units. */
s32 sceIsofsLbInit();
/** Frees resources for all global units. */
s32 sceIsofsLbTerm();
/** Returns global unit by its number. */
IsofsUnit *isofsGetUnit(s32 unitNum);
/** Clears unit's fields, e.g. after unmount. */
s32 isofs_release_blkdev(s32 unitNum);

/** Initializes global cache. */
s32 sceIsofsCacheCheckData();
/** Frees global cache. */
s32 isofsFreeCache();
/** Frees global cache data. */
s32 sceIsofsCacheDeleteData();

// Driver functions
int isofs_init(SceIoDeviceEntry *entry);
int isofs_exit(SceIoDeviceEntry *entry);
int isofs_open(SceIoIob *iob, char *path, int flags, SceMode mode);
int isofs_close(SceIoIob *iob);
s32 isofs_read(SceIoIob *iob, void *outdata, SceSize size);
SceOff isofs_lseek(SceIoIob *iob, SceOff ofs, int whence);
int isofs_ioctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
int isofs_dopen(SceIoIob *iob, char *path);
int isofs_dclose(SceIoIob *iob);
int isofs_dread(SceIoIob *iob, SceIoDirent *dirent);
int isofs_getstat(SceIoIob *iob, char *name, SceIoStat *stat);
int isofs_chdir(SceIoIob *iob, char *path);
int isofs_mount(SceIoIob *iob, const char *fs, const char *blockDev, int mode, void *outdata, int outlen);
int isofs_umount(SceIoIob *iob, char *blockDev);
int isofs_devctl(SceIoIob *iob, char *dev, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);

/** Opens file from the ISO directory descriptor. */
IsofsFile *sceIsofsFileAllocNfile(IsofsUnit *unit, SceIoIob *iob, IsoDirectory *isoDir, s32 *outRet);
/** Opens file directly by the specified LBN and size params. */
IsofsFile *sceIsofsFileAllocVfile(IsofsUnit *unit, SceIoIob *iob, u32 lbn, u32 size, s32 *outRet);
/** Seek to specified position in the file. */
s64 isofs_set_lseek(IsofsFile *file, s64 pos, int whence);
/** Main ioctl implementation. */
s32 isofs_exec_ioctl_cmd(SceIoIob *iob, s32 cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);

// Rather simple functions to initialize and free file descriptors related structs.
s32 isofsInitUnitFdw(IsofsUnit *unit, u32 fdwCount, u16 filesCount);
void isofsFreeUnitFdw(IsofsUnit *unit);
s32 sceIsofsSbInit(IsofsFdw *fdw, u32 idx, u32 filesCount);
void isofsClearFdwFiles(IsofsFdw *fdw, s32 fdwCount);
s32 isofsClearUnitFilesFdw(IsofsUnit *unit, s32 fdwCount);
s32 sceIsofsSbTerm(IsofsFd *fileDesc, u32 filesCount);
s32 isofsFreeFdFiles(IsofsFd *fileDesc);
s32 sceIsofsFileTerm(IsofsFile *files);
s32 isofsClearFile(IsofsFile *file);

/** Set file params from ISO directory records. */
IsofsFile *isofs_init_fd(IsofsFile *file, IsoDirectory *isoDir, SceIoIob *iob, s32 *outRet);
/** Set file params for raw open with provided LBN and size. */
IsofsFile *isofs_init_vfd(IsofsFile *file, SceIoIob *iob, u32 lbn, u32 size, s32 *outRet);
/** Find unused file descriptor for normal open. */
IsofsFile *isofs_alloc_fd(IsofsUnit *unit, IsoDirectory *isoDir, s32 *outRet);
/** Find unused file descriptor for raw open. */
IsofsFile *isofs_alloc_vfd(IsofsUnit *unit, u32 lbn, s32 *outRet);

/** Select implementation handlers based on the block device name. */
s32 isofsSetBlockDevHandler(IsofsUnit *unit);
/** Clears selected implementation handlers. */
s32 isofsClearBlockDevHandler(IsofsUnit *unit);

/** Initialize unit with data from the ISO's primary volume descriptor (PVD). */
s32 isofs_get_pvd(char *blockDev, s32 unitNum);
/** Reads ISO directory into the output buffer. */
int sceIsofsReadDirent(IsofsUnit *unit, u32 lbn, void *outdata);
/** Wraps sector reading function for use with sceKernelExtendKernelStack. */
s32 isofsHandlerReadSectorsArgs(void *args);

/** Checks if the path is in the "/sce_lbn" format (used for raw read from the ISO). */
s32 isofsIsSceLbnPath(char *path);
/** Extracts LBN and size from the "/sce_lbn" path. */
s32 sceIsofsCheckVFile(char *path, s32 *outLbn, s32 *outSize);

/** Reads and parses ISO path table to create unit's paths. */
s32 sceIsofsGetPathTable(IsofsUnit *unit);
/** Frees unit paths. */
s32 isofs_delete_ptbl(IsofsUnit *unit);
/** Frees unit paths. (effectively same as isofs_delete_ptbl) */
s32 isofsFreeUnitPaths(IsofsUnit *unit);
/** Reads ISO path table. */
IsoPath *isofs_new_ptbl(IsofsUnit *unit, s32 *outRet);
/** Counts entries in the ISO path table. */
s32 isofs_get_direc_count(IsoPath *pathTable, s32 pathTableSize, s32 *outRet);
/** Parses entries in the ISO path table and converts them to ISOFS path struct. */
IsofsPath *isofs_create_ptbl(IsofsUnit *unit, IsoPath *pathTable, s32 pathTableSize, s32 pathTableEntryCount,
                                      s32 *outRet);
/** Calculate and update LBN size of the path table. */
s32 isofs_set_topend_dirsize(IsofsUnit *unit);

/** Check if the unit is already mounted. */
s32 isofsCheckUnitMounted(IsofsUnit *unit, s32 unitNum);
/** Sets LBN bounds fields in the unit based on data queried from the block device. */
s32 isofs_get_umdpfi(char *blockDev, s32 unitNum);
/** Clear global variable with the current directory LBN. */
void isofsClearCurrentDirLbn();
/** Remove directory for the open directories linked list. */
void isofsUnlinkOpenedDir(IsofsDir *dir);
s32 isofsCheckMode(SceMode mode);
/** Checks paths for invalid characters. */
s32 isofsCheckPath(char *name, u32 flag);
/** Find ISO directory entry by name. */
IsoDirectory *sceIsofsLookupFile(char *path, IsoDirectory *isoDirs, s32 flag, s32 *outRet);
/** Set dirent fields from ISO directory. */
IsoDirectory *isofsSetDirentFromIsoDirectory(IsofsDir *dir, SceIoDirent *dirent, s32 *outRet);
/** Read ISO directory for file open or stat. */
IsoDirectory *sceIsofsReadDirectoryRecord(IsofsUnit *unit, char *path, char *file, u32 flag, s32 *outRet);
/** Find the path record for directory. */
IsofsPath *sceIsofsLookupDir(IsofsUnit *unit, char *pathSplit, u32 dirLevel, int pathLen, u32 flag, s32 *outRet);
/** Split path by slash separator. */
char *isofs_get_dirname(char *path, u32 *outLen, u32 flag, s32 *outRet);

// Simple functions delegating work to the actual device implementation.
s32 isofsHandlerReadDirSectors(IsofsUnit *unit, s64 lbn, s32 size, void *outdata, s32 outlen, s32 cmd, u32 reservedParam);
s32 isofsHandlerReadSectors(IsofsUnit *unit, IsofsFile *file, s32 size, void *outdata, s32 outlen, u32 readFlags,
                            s32 reservedParam);
s32 isofsHandlerGetSectorSize(IsofsUnit *unit);
s32 isofsHandlerGetTotalNumberOfSectors(IsofsUnit *unit);
s32 isofsHandlerGetCurrentLbn(IsofsUnit *unit);
s64 isofsHandlerSeek(IsofsUnit *unit, s64 lbn);
s32 isofsHandlerDevctlExt(IsofsUnit *unit, s32 mode, void *indata, SceSize inlen, s32 outBuf, s32 outLen);
s32 isofsHandlerReadPvd(IsofsUnit *unit, void *indata, SceSize inlen);
s32 isofsHandlerPrepareIntoCache(IsofsUnit *unit, IsofsFile *file, s32 cacheSize, s32 getStatusFlag);
s32 isofsHandlerCacheControl(IsofsUnit *unit, IsofsFile *file, s32 flag, void *outdata);
s32 isofsHandlerCacheSuspend(IsofsUnit *unit, IsofsFile *file);
s32 isofsHandlerCacheResume(IsofsUnit *unit, IsofsFile *file);

// All IsofsImplFuncs for interacting with the UMD device.
s32 isofsUmdReadDirSectors(IsofsUnit *unit, s64 lbn, s32 size, void *outdata, s32 outlen, u32 cmd);
s32 isofsUmdReadSectors(IsofsUnit *unit, IsofsFile *file, s32 size, void *outdata, s32 outlen, u32 cmd);
s32 isofsUmdGetSectorSize(IsofsUnit *unit);
s32 isofsUmdGetTotalNumberOfSectors(IsofsUnit *unit);
s32 isofsUmdGetCurrentLbn(IsofsUnit *unit);
s64 isofsUmdSeek(IsofsUnit *unit, s64 lbn);
s32 isofsUmdDevctlExt(IsofsUnit *unit, s32 mode, void *indata, SceSize inlen, s32 outBuf, s32 outLen);
s32 isofsUmdNotSupported(void);
s32 isofsUmdGetUnitNum(IsofsUnit *unit, void *indata, SceSize inlen);
s32 isofsUmdReadPvd(IsofsUnit *unit, void *indata, SceSize inlen);
s32 isofsUmdPrepareIntoCache(IsofsUnit *unit, IsofsFile *file, s32 cacheSize, s32 getStatusFlag);
s32 isofsUmdCacheControl(IsofsUnit *unit, IsofsFile *file, s32 flag);
s32 isofsUmdCacheSuspend(IsofsUnit *unit, IsofsFile *file);
s32 isofsUmdCacheResume(IsofsUnit *unit, IsofsFile *file);

// UMD callbacks management and implementation.
s32 isofsRegisterOnUmdMounted(IsofsUnit *unit);
s32 isofsUmdManUnRegisterInsertEjectUMDCallBack();
s32 isofsUmdMountedCb(int id, IsofsUnit *unit, int arg);

/** Sets params for reading from UMD assuming that current file position is at 0. */
s32 isofsReadParamsZero(IsofsUnit *unit, IsofsFile *file, void *outdata, s32 size, LbnParams *params);
/** Sets params for reading from UMD assuming that current file position is less than sector size. */
s32 isofsReadParamsInitial(IsofsUnit *unit, IsofsFile *file, void *outdata, s32 size, LbnParams *params);
/** Sets params for reading from UMD for other cases without assumptions. */
s32 isofsReadParams(IsofsUnit *unit, IsofsFile *file, void *outdata, s32 size, LbnParams *params);


extern SceIoDeviceFunction g_isofsDrvFuncs;
extern SceIoDeviceTable g_isofsDrv;
extern const IsofsImplFuncs g_isofsUmdFuncs;
extern const IsofsHandler g_isofsHandlers[];
extern IsofsUnit *g_isofsUnits;
extern IsofsCacheEntry *g_isofsCache;
extern u8 g_isofsCurrentDir[4096];
extern s32 g_isofsCurrentDirLbn;
extern s32 g_isofsHasMountedOnce;
extern s32 g_isofsMounted;
extern s32 g_isofsInitialized;
extern IsofsDir *g_isofsOpenDirs;
extern IsofsMgr g_isofsMgr;

static inline u32 _readLe32(const u8 *buf) {
    return (u32) buf[0] | ((u32) buf[1] << 8) | ((u32) buf[2] << 16) | ((u32) buf[3] << 24);
}

static inline void _setUmdId(char *umdId, int value) {
    s32 *id = (s32 *) umdId;
    id[0] = value;
    id[1] = value;
    id[2] = value;
    id[3] = value;
    id[4] = value;
    id[5] = value;
    id[6] = value;
    id[7] = value;
}


#endif /* ISOFS_INT_H */
