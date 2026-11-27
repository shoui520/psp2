#include "isofs_int.h"

// 00006d04
SceIoDeviceFunction g_isofsDrvFuncs = {
    isofsDrvInit,
    isofsDrvExit,
    isofsDrvOpen,
    isofsDrvClose,
    isofsDrvRead,
    NULL,
    isofsDrvLseek,
    isofsDrvIoctl,
    NULL,
    NULL,
    NULL,
    isofsDrvDopen,
    isofsDrvDclose,
    isofsDrvDread,
    isofsDrvGetstat,
    NULL,
    NULL,
    isofsDrvChdir,
    isofsDrvMount,
    isofsDrvUmount,
    isofsDrvDevctl,
    NULL
};

// 00006d5c
SceIoDeviceTable g_isofsDrv = {"isofs", 0x10, 0x800, "ISOFS", &g_isofsDrvFuncs};

// 00006d74
const IsofsImplFuncs g_isofsUmdFuncs = {
    isofsUmdReadDirSectors,
    isofsUmdReadSectors,
    NULL,
    isofsUmdGetSectorSize,
    isofsUmdGetTotalNumberOfSectors,
    isofsUmdGetCurrentLbn,
    isofsUmdSeek,
    isofsUmdDevctlExt,
    isofsUmdNotSupported,
    isofsUmdGetUnitNum,
    isofsUmdReadPvd,
    isofsUmdPrepareIntoCache,
    isofsUmdCacheControl,
    isofsUmdCacheSuspend,
    isofsUmdCacheResume
};

// 00006db0
const IsofsHandler g_isofsHandlers[] = {
    {{'u', 'm', 'd', '\0'}, &g_isofsUmdFuncs},
    {{'n', 'u', 'l', 'l', '\0'}, NULL},
    {{'\0'}, NULL},
    {{'\0'}, NULL}
};

// 00006e80
IsofsUnit *g_isofsUnits;

// 00006e84
IsofsCacheEntry *g_isofsCache;

// 00006ec0
u8 __attribute__((aligned(64))) g_isofsCurrentDir[4096];

// 00007ec0;
s32 g_isofsCurrentDirLbn;

// 00007ec4
s32 g_isofsHasMountedOnce;

// 00007ec8
s32 g_isofsMounted;

// 00007ecc
s32 g_isofsInitialized;

// 00007ed0
IsofsDir *g_isofsOpenDirs;

// 00007ed4
IsofsMgr g_isofsMgr;


int isofsDrvInit(SceIoDeviceEntry *entry) {
    if (entry == NULL) {
        sceKernelSignalSema(g_isofsMgr.semaId, 1);
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    SceUID heapId = sceKernelCreateHeap(1, 0xa000, 1, "SceIsofsRootHeap");
    if (heapId < 0) {
        return heapId;
    }
    g_isofsMgr.heapId = heapId;
    s32 mgrSemaId = sceKernelCreateSema("SceIsofsMgr", 1, 1, 1, NULL);
    if (mgrSemaId > -1) {
        g_isofsMgr.semaId = mgrSemaId;
        s32 ret = isofsInitUnits();
        if (ret == 0 && (ret = sceKernelWaitSema(g_isofsMgr.semaId, 1, NULL), -1 < ret)) {
            for (s32 i = 0; i < g_isofsMgr.maxUnits; ++i) {
                IsofsUnit *unit = isofsGetUnit(i);
                s32 semaId = sceKernelCreateSema("SceIsofs", 1, 1, 1, NULL);
                if (semaId > -1) {
                    unit->semaId = semaId;
                    if (i >= g_isofsMgr.maxUnits) {
                        break;
                    }
                    continue;
                }

                // delete already created
                for (s32 prevIdx = 0; prevIdx < g_isofsMgr.maxUnits; ++prevIdx) {
                    IsofsUnit *prevUnit = isofsGetUnit(prevIdx);
                    if (prevUnit->semaId > -1) {
                        sceKernelDeleteSema(prevUnit->semaId);
                    }
                }
                sceKernelSignalSema(g_isofsMgr.semaId, 1);
                sceKernelDeleteHeap(g_isofsMgr.heapId);
                sceKernelDeleteSema(g_isofsMgr.semaId);
                return SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
            }

            IsofsUnit *unit = isofsGetUnit(0);
            entry->d_private = unit;
            if (isofsInitCache() == 0) {
                g_isofsInitialized = 1;
                sceKernelSignalSema(g_isofsMgr.semaId, 1);
                return SCE_ERROR_OK;
            } else {
                sceKernelDeleteSema(g_isofsMgr.semaId);
                sceKernelDeleteHeap(g_isofsMgr.heapId);
                sceKernelSignalSema(g_isofsMgr.semaId, 1);
                return SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
            }
        }
        sceKernelDeleteSema(g_isofsMgr.semaId);
    }
    sceKernelDeleteHeap(g_isofsMgr.heapId);
    return SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
}

// 00002680
int isofsDrvMount(SceIoIob *iob, const char *fs, const char *blockDev, int mode, void *outdata, int outlen) {
    if (pspGetK1() <= -1) {
        return SCE_ERROR_PRIV_REQUIRED;
    }
    if (iob == NULL || blockDev == NULL || fs == 0) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (iob->i_unit < 0 || iob->i_unit >= g_isofsMgr.maxUnits) {
        return SCE_ERROR_ERRNO_INVALID_UNIT_NUM;
    }
    s32 ret = sceKernelWaitSema(g_isofsMgr.semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    s32 outdataRead = 2;
    if (outdata != NULL && outlen > 3) {
        outdataRead = *(s32 *) outdata;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    ret = isofsCheckUnitMounted(unit, iob->i_unit);
    if (ret == 0 && (ret = isofsCheckMode(mode), ret == 0)) {
        u32 blockDevLen = strlen(blockDev);
        u32 devctlRet = 0;
        ret = sceIoDevctl(blockDev, DEVCTL_QUERY_BLOCK_DEV, (char *) blockDev, blockDevLen, &devctlRet,
                          sizeof(devctlRet));
        if (ret == 0) {
            unit->unitNum = (s16) iob->i_unit;
            unit->mountFlags = unit->mountFlags | 0x10;
            unit->activeFlag = 0;
            if (blockDevLen >= 0x20) {
                ret = SCE_ERROR_ERRNO_NAME_TOO_LONG;
            } else {
                memcpy(unit->blockDev, blockDev, blockDevLen);
                unit->blockDev[blockDevLen] = 0;
                ret = isofsSetBlockDevHandler(unit);
                if (ret >= 0) {
                    if (outdataRead == 2) {
                        return SCE_ERROR_NOT_SUPPORTED;
                    }
                    unit->mountState = 1;
                    ret = isofsRegisterOnUmdMounted(unit);
                    if (ret == 0) {
                        return sceKernelSignalSema(g_isofsMgr.semaId, 1);
                    }
                    isofsClearUnitFilesFdw(unit, g_isofsMgr.maxFiles);
                    isofsClearUnit(unit->unitNum);
                }
            }
        }
    }
    sceKernelSignalSema(g_isofsMgr.semaId, 1);
    return sceUmd_040A7090(ret);
}

// 000028a4
s32 isofsClearUnit(s32 unitNum) {
    if (unitNum < 0) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    for (s32 i = 0; i < g_isofsMgr.maxUnits; ++i) {
        IsofsUnit *unit = isofsGetUnit(i);
        if (unit->unitNum == unitNum && (unit->mountFlags & 0x10) != 0) {
            s32 ret = isofsFreeUnitPaths(unit);
            if (ret != 0) {
                return ret;
            }
            unit->pathTableSize = 0;
            unit->rootDirLbn = 0;
            unit->rootDirSize = 0;
            unit->nextLbn = 0;
            unit->lastLbn = 0;
            unit->remainingLbns = 0;
            unit->pathTableLbn = 0;
            unit->totalSectors = 0;
            unit->cachedCurrentLbn = 0;
            unit->openState = 0;
            unit->sectorSize = 0;
            _setUmdId(unit->umdId, 0);
            unit->unitNum = -1;
            unit->mountFlags = 0;
            unit->pathTableEntryCount = 0;
            unit->paths = NULL;
            unit->pathTable = NULL;
            unit->primaryVolumeDescriptor = NULL;
            unit->activeFlag = 0;
            unit->mountState = 0;
            g_isofsHasMountedOnce = 0;
            return SCE_ERROR_OK;
        }
    }
    return SCE_ERROR_ERRNO_DEVICE_BUSY;
}

// 000029c8
int isofsDrvDopen(SceIoIob *iob, char *path) {
    if (iob == NULL || path == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    u32 pathLen = strlen(path);
    if (pathLen == 0) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (pathLen + 1 > 0x100) {
        return SCE_ERROR_ERRNO_NAME_TOO_LONG;
    }
    if (g_isofsMgr.maxUnits <= iob->i_unit) {
        return SCE_ERROR_ERRNO_INVALID_UNIT_NUM;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    if (unit == NULL || g_isofsMounted == 0) {
        return SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
    }
    s32 ret = sceKernelWaitSema(unit->semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    ret = isofsCheckPath(path, 1);
    if (ret == 0) {
        IsofsDir *dir = sceKernelAllocHeapMemory(g_isofsMgr.heapId, sizeof(IsofsDir));
        if (dir == NULL) {
            ret = SCE_ERROR_ERRNO_NO_MEMORY;
        } else {
            dir->lbnSize = 0;
            dir->isoDir = NULL;
            s32 intr = sceKernelCpuSuspendIntr();
            __builtin_memcpy(dir->umdId, unit->umdId, UMD_ID_SIZE);
            dir->compatFlag = 0;
            dir->prevDir = NULL;
            if (g_isofsOpenDirs == NULL) {
                g_isofsOpenDirs = dir;
                dir->nextDir = NULL;
            } else {
                dir->nextDir = g_isofsOpenDirs;
                g_isofsOpenDirs->prevDir = dir;
            }
            sceKernelCpuResumeIntr(intr);
            s32 pathResolved = 0;
            if (strcmp(path, "/") != 0) {
                char pathStack[257];
                memset(pathStack, 0, 0x100);
                if (pathLen < 0x100) {
                    s32 dirLevel = 1;
                    if (strcmp(path + (pathLen - 1), "/") == 0) {
                        memcpy(pathStack, path, pathLen - 1);
                        pathStack[pathLen] = 0;
                    } else {
                        memcpy(pathStack, path, pathLen);
                        pathStack[pathLen + 1] = 0;
                    }
                    char *pathSeg = pathStack + 1;
                    do {
                        char *pathSep = strchr(pathSeg, '/');
                        if (pathSep == NULL) {
                            break;
                        }
                        dirLevel = dirLevel + 1;
                        pathSeg = pathSep + 1;
                    } while (1);
                    IsofsPath *paths = isofsFindPath(unit, pathStack, dirLevel, strlen(pathStack + 1), 1, &ret);
                    if (paths != NULL) {
                        dir->paths = paths;
                        dir->lbnSize = paths->lbnSize;
                        dir->lbn = paths->lbn;
                        pathResolved = 1;
                    }
                } else {
                    ret = SCE_ERROR_ERRNO_NAME_TOO_LONG;
                }
                if (!pathResolved) {
                    intr = sceKernelCpuSuspendIntr();
                    isofsUnlinkOpenedDir(dir);
                    sceKernelCpuResumeIntr(intr);
                    sceKernelFreeHeapMemory(g_isofsMgr.heapId, dir);
                    sceKernelSignalSema(unit->semaId, 1);
                    return sceUmd_040A7090(ret);
                }
            } else {
                dir->paths = unit->paths;
                u32 lbnSize = 1;
                if (0x7ff < unit->rootDirSize && (lbnSize = unit->rootDirSize >> 0xb, (unit->rootDirSize & 0x7ff) != 0)) {
                    lbnSize = (unit->rootDirSize >> 0xb) + 1;
                }
                dir->lbnSize = lbnSize;
                dir->lbn = unit->rootDirLbn;
            }
            dir->dirRecLen = 0;
            dir->fplData = NULL;
            dir->isoDir = NULL;
            if (dir->compatFlag == 0) {
                iob->i_private = dir;
            } else {
                intr = sceKernelCpuSuspendIntr();
                isofsUnlinkOpenedDir(dir);
                sceKernelCpuResumeIntr(intr);
                sceKernelFreeHeapMemory(g_isofsMgr.heapId, dir);
                ret = SCE_UMD_ERROR_NOT_DEVICE_READY;
            }
        }
    }
    sceKernelSignalSema(unit->semaId, 1);
    return sceUmd_040A7090(ret);
}

// 	00002d4c
int isofsDrvDread(SceIoIob *iob, SceIoDirent *dirent) {
    if (iob == NULL || iob->i_private == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    if (unit == NULL || dirent == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 ret = sceKernelWaitSema(unit->semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    IsoDirectory *data = NULL;
    IsofsDir *dir = iob->i_private;
    if (dir->compatFlag != 0) {
        ret = SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
    } else {
        s32 ready = 1;
        memset(dirent, 0, 0x160);
        if (dir->dirRecLen == 0) {
            ret = sceKernelTryAllocateFpl(g_isofsMgr.fplId, (void **) &data);
            if (ret < 0) {
                ready = 0;
            } else {
                dir->fplData = data;
                if (dir->dirRecLen == 0) {
                    ret = isofsReadDir(unit, dir->lbn, data);
                    if (ret != 0) {
                        ready = 0;
                    } else if (dir->compatFlag != 0) {
                        ret = SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
                        ready = 0;
                    } else {
                        dir->isoDir = data;
                    }
                }
            }
        }
        while (ready) {
            IsoDirectory *stat = dir->isoDir;
            if (stat->recLen != 0 && stat->nameLen == 1 && (stat->name[0] == 0 || stat->name[0] == 1)) {
                stat = isofsSetDirentFromIsoDirectory(dir, dirent, &ret);
                if (stat != NULL) {
                    dir->isoDir = stat;
                    ret = 1;
                    continue;
                }
                dir->isoDir = NULL;
                ret = 0;
                break;
            }
            if (stat->recLen == 0 && dir->lbnSize >= 2) {
                stat = isofsSetDirentFromIsoDirectory(dir, dirent, &ret);
                if (stat == NULL) {
                    dir->isoDir = NULL;
                    ret = 0;
                    break;
                }
                dir->isoDir = stat;
                ret = 1;
                if (dir->lbnSize == 0 || dir->dirRecLen == 0 || (dir->dirRecLen & 0xfff) != 0) {
                    continue;
                }
                dir->lbn = dir->lbn + 2;
                memset(dir->fplData, 0, 0x1000);
                ret = isofsReadDir(unit, dir->lbn, dir->fplData);
                if (ret != 0) {
                    dir->lbn = dir->lbn - 2;
                    break;
                }
                if (dir->compatFlag != 0) {
                    ret = SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
                    dir->lbn = dir->lbn - 2;
                    break;
                }
                ret = 1;
                dir->isoDir = (IsoDirectory *) dir->fplData;
                continue;
            }
            stat = isofsSetDirentFromIsoDirectory(dir, dirent, &ret);
            if (stat == NULL) {
                if (ret == (s32) SCE_ERROR_ERRNO_FILE_NOT_FOUND) {
                    ret = 0;
                }
                dir->isoDir = NULL;
                break;
            }
            dir->isoDir = stat;
            ret = 1;
            if (dir->lbnSize != 0 && dir->dirRecLen != 0 && (dir->dirRecLen & 0xfff) == 0) {
                dir->lbn = dir->lbn + 2;
                memset(dir->fplData, 0, 0x1000);
                ret = isofsReadDir(unit, dir->lbn, dir->fplData);
                if (ret != 0) {
                    dir->lbn = dir->lbn - 2;
                } else if (dir->compatFlag != 0) {
                    ret = SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
                    dir->lbn = dir->lbn - 2;
                } else {
                    ret = 1;
                    dir->isoDir = (IsoDirectory *) dir->fplData;
                }
            }
            break;
        }
    }
    sceKernelSignalSema(unit->semaId, 1);
    return sceUmd_040A7090(ret);
}

// 00003084
int isofsDrvClose(SceIoIob *iob) {
    if (iob == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    IsofsFile *file = iob->i_private;
    if (unit == NULL || file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 ret = sceKernelWaitSema(unit->semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    ret = isofsClearFile(file);
    if (ret == 0) {
        iob->i_private = NULL;
    }
    sceKernelSignalSema(unit->semaId, 1);
    return ret;
}

// 00003134
int isofsDrvDclose(SceIoIob *iob) {
    if (iob == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    s32 ret = sceKernelWaitSema(unit->semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    IsofsDir *dir = iob->i_private;
    s32 intr = sceKernelCpuSuspendIntr();
    isofsUnlinkOpenedDir(dir);
    sceKernelCpuResumeIntr(intr);
    sceKernelFreeFpl(g_isofsMgr.fplId, dir->fplData);
    sceKernelFreeHeapMemory(g_isofsMgr.heapId, dir);
    iob->i_private = NULL;
    sceKernelSignalSema(unit->semaId, 1);
    return ret;
}

// 000031f4
int isofsDrvUmount(SceIoIob *iob, __attribute__((unused)) char *blockDev) {
    if (iob == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    s32 ret = sceKernelWaitSema(g_isofsMgr.semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    ret = isofsUmdManUnRegisterInsertEjectUMDCallBack();
    if (ret == 0) {
        isofsClearBlockDevHandler(unit);
        ret = isofsClearUnitFilesFdw(unit, g_isofsMgr.maxFiles);
        if (ret == 0) {
            ret = isofsClearUnit(unit->unitNum);
            g_isofsMounted = 0;
            sceIoTerminateFd("isofs");
        }
    }
    sceKernelCpuResumeIntr(intr);
    sceKernelSignalSema(g_isofsMgr.semaId, 1);
    return ret;
}

// 000032d8
int isofsDrvExit(SceIoDeviceEntry *entry) {
    if (entry == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 ret = isofsFreeUnits();
    if (ret != 0) {
        return ret;
    }
    ret = isofsFreeCache();
    if (ret != 0) {
        return ret;
    }
    sceKernelDeleteHeap(g_isofsMgr.heapId);
    sceKernelDeleteSema(g_isofsMgr.semaId);
    g_isofsInitialized = 0;
    return SCE_ERROR_OK;
}

// 00003348
int isofsDrvGetstat(SceIoIob *iob, char *name, SceIoStat *stat) {
    if (iob == NULL || name == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (strlen(name) + 1 > 0x100 || g_isofsMgr.maxUnits <= iob->i_unit) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    s32 ret = sceKernelWaitSema(unit->semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    ret = isofsCheckPath(name, 2);
    s32 sceLbnRet = 0;
    if (ret != 0 || (sceLbnRet = isofsIsSceLbnPath(name), sceLbnRet != 2)) {
        sceKernelSignalSema(unit->semaId, 1);
        return sceUmd_040A7090(ret);
    }
    char *lastSlash = strrchr(name, '/');
    if (lastSlash == NULL) {
        ret = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        sceKernelSignalSema(unit->semaId, 1);
        return sceUmd_040A7090(ret);
    }
    IsoDirectory *isoDir = isofsReadIsoDir(unit, name, lastSlash + 1, 2, &ret);
    if (isoDir == NULL) {
        ret = SCE_ERROR_ERRNO_FILE_NOT_FOUND;
        sceKernelSignalSema(unit->semaId, 1);
        return sceUmd_040A7090(ret);
    }
    stat->st_private[0] = isoDir->lbn;
    stat->st_private[1] = 0;
    stat->st_private[2] = 0;
    stat->st_private[3] = 0;
    stat->st_private[4] = 0;
    stat->st_private[5] = 0;
    stat->st_mode = 0x16d;
    stat->st_size = isoDir->size;
    u16 year = isoDir->dateTime.yearsSince1900 + 1900;
    stat->st_ctime.year = year;
    stat->st_mtime.year = year;
    stat->st_atime.year = year;
    stat->st_ctime.month = (u16) isoDir->dateTime.month;
    stat->st_mtime.month = (u16) isoDir->dateTime.month;
    stat->st_atime.month = (u16) isoDir->dateTime.month;
    stat->st_ctime.day = (u16) isoDir->dateTime.day;
    stat->st_mtime.day = (u16) isoDir->dateTime.day;
    stat->st_atime.day = (u16) isoDir->dateTime.day;
    stat->st_ctime.hour = (u16) isoDir->dateTime.hour;
    stat->st_mtime.hour = (u16) isoDir->dateTime.hour;
    stat->st_atime.hour = (u16) isoDir->dateTime.hour;
    stat->st_ctime.minute = (u16) isoDir->dateTime.minute;
    stat->st_mtime.minute = (u16) isoDir->dateTime.minute;
    stat->st_atime.minute = (u16) isoDir->dateTime.minute;
    stat->st_ctime.second = (u16) isoDir->dateTime.second;
    stat->st_mtime.second = (u16) isoDir->dateTime.second;
    stat->st_atime.second = (u16) isoDir->dateTime.second;
    stat->st_ctime.microsecond = 0;
    stat->st_mtime.microsecond = 0;
    stat->st_atime.microsecond = 0;
    s32 sdkVer = sceKernelGetCompiledSdkVersion();
    if (sdkVer >= 0x2000011) {
        u32 dateTimeOffset = isoDir->dateTime.offset;
        u64 tick = 0;
        if (dateTimeOffset != 0 && (ret = sceRtcGetTick((pspTime *) &stat->st_ctime, &tick), ret == 0)) {
            if (dateTimeOffset - 0xd1 < 0x2f) {
                u64 numMs = (s64) (s32) (0x100 - dateTimeOffset) * 900000000;
                sceRtcTickAddTicks(&tick, &tick, numMs);
            } else if (dateTimeOffset - 1 < 0x34) {
                u64 numMs = (s64) (s32) dateTimeOffset * 900000000;
                sceRtcTickAddTicks(&tick, &tick, numMs);
            }
            ScePspDateTime time;
            ret = sceRtcSetTick((pspTime *) &time, &tick);
            if (ret == 0) {
                stat->st_atime.year = time.year;
                stat->st_atime.month = time.month;
                stat->st_atime.day = time.day;
                stat->st_atime.hour = time.hour;
                stat->st_atime.minute = time.minute;
                stat->st_atime.second = time.second;
                stat->st_atime.microsecond = time.microsecond;
                stat->st_mtime.year = time.year;
                stat->st_ctime.year = time.year;
                stat->st_mtime.month = time.month;
                stat->st_ctime.month = time.month;
                stat->st_mtime.day = time.day;
                stat->st_ctime.day = time.day;
                stat->st_mtime.hour = time.hour;
                stat->st_ctime.hour = time.hour;
                stat->st_mtime.minute = time.minute;
                stat->st_ctime.minute = time.minute;
                stat->st_mtime.second = time.second;
                stat->st_ctime.second = time.second;
                stat->st_mtime.microsecond = time.microsecond;
                stat->st_ctime.microsecond = time.microsecond;
            }
        }
    }
    if ((isoDir->flags & 2) == 0) {
        stat->st_mode = stat->st_mode | 0x2000;
    } else {
        stat->st_mode = stat->st_mode | 0x1000;
    }
    sceKernelSignalSema(unit->semaId, 1);
    return sceUmd_040A7090(ret);
}

// 000036e0
s32 isofsCheckUnitMounted(IsofsUnit *unit, s32 unitNum) {
    if (unit->primaryVolumeDescriptor != NULL || unit->unitNum == unitNum || unit->activeFlag != 0
        || (unit->mountFlags & 0x10) != 0) {
        return SCE_ERROR_ERRNO_DEVICE_BUSY;
    }
    return SCE_ERROR_OK;
}

// 00003720
s32 __attribute__ ((noinline)) isofsCheckMode(SceMode mode) {
    if ((mode & 0xf02U) != 0) {
        return SCE_ERROR_ERRNO_INVALID_FLAG;
    }
    return SCE_ERROR_OK;
}

// 00003734
int isofsDrvOpen(SceIoIob *iob, char *path, int flags, __attribute__((unused)) SceMode mode) {
    if (iob == NULL || path == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    u32 pathLen = strlen(path);
    if ((int) (pathLen + 1) > 0x100) {
        return SCE_ERROR_ERRNO_NAME_TOO_LONG;
    }
    if (g_isofsMgr.maxUnits <= iob->i_unit) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    if (unit == NULL || g_isofsMounted == 0) {
        return SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
    }
    s32 ret = sceKernelWaitSema(unit->semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    ret = isofsCheckMode(flags); // bug? should probably pass mode instead
    if (ret == 0 && (ret = isofsCheckPath(path, 2), ret == 0)) {
        IsofsFile *file;
        unit->openState = 1;
        s32 openMode = isofsIsSceLbnPath(path);
        if (openMode == 2) {
            char *pathSep = strrchr(path, '/');
            if (pathSep == NULL) {
                ret = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
            } else {
                pathLen = strlen(pathSep + 1);
                if (pathLen < 0x20) {
                    IsoDirectory *isoDir = isofsReadIsoDir(unit, path, pathSep + 1, 2, &ret);
                    if (isoDir == NULL || ret != 0) {
                        if (ret == (s32) SCE_UMD_ERROR_NOT_DEVICE_READY || ret == (s32) SCE_UMD_ERROR_NO_MEDIUM) {
                            unit->openState = 0;
                            sceKernelSignalSema(unit->semaId, 1);
                            return sceUmd_040A7090(ret);
                        }
                        ret = SCE_ERROR_ERRNO_FILE_NOT_FOUND;
                    } else {
                        file = isofsOpen(unit, iob, isoDir, &ret);
                        if (file != NULL) {
                            if ((flags & 0x4000) != 0) {
                                file->readFlags = file->readFlags | 8;
                            }
                            iob->i_private = file;
                            iob->i_fpos = 0;
                            unit->openState = 0;
                            sceKernelSignalSema(unit->semaId, 1);
                            return sceUmd_040A7090(ret);
                        }
                    }
                } else {
                    ret = SCE_ERROR_ERRNO_NAME_TOO_LONG;
                }
            }
        } else if (openMode == 3) {
            s32 lbn = 0;
            s32 size = 0;
            pathLen = strlen(path);
            if (pathLen > 0xff) {
                ret = SCE_ERROR_ERRNO_NAME_TOO_LONG;
                unit->openState = 0;
                sceKernelSignalSema(unit->semaId, 1);
                return sceUmd_040A7090(ret);
            }
            ret = isofsParseSceLbnPath(path, &lbn, &size);
            if (ret == 0) {
                u32 currentLbn = unit->currentLbn;
                if (currentLbn < (u32) lbn) {
                    ret = SCE_ERROR_ERRNO_FILE_NOT_FOUND;
                    unit->openState = 0;
                    sceKernelSignalSema(unit->semaId, 1);
                    return sceUmd_040A7090(ret);
                }
                u32 lbnSize = 1;
                if (size >= 0x800) {
                    lbnSize = (size >> 0xb) + 1;
                    if ((size & 0x7ff) == 0) {
                        lbnSize = size >> 0xb;
                    }
                }
                if (currentLbn < lbn + lbnSize - 1) {
                    size = (currentLbn - lbn + 1) * 0x800;
                }
                file = isofsOpenRaw(unit, iob, lbn, size, &ret);
                if (file != NULL) {
                    if ((flags & 0x4000) != 0) {
                        file->readFlags = file->readFlags | 8;
                    }
                    iob->i_private = file;
                    iob->i_fpos = 0;
                    unit->openState = 0;
                    sceKernelSignalSema(unit->semaId, 1);
                    return sceUmd_040A7090(ret);
                }
            }
        } else {
            ret = SCE_ERROR_ERRNO_FILE_NOT_FOUND;
        }
    }

    unit->openState = 0;
    sceKernelSignalSema(unit->semaId, 1);
    return sceUmd_040A7090(ret);
}

// 00003a1c
s32 isofsDrvRead(SceIoIob *iob, void *outdata, SceSize size) {
    if (iob == NULL || outdata == 0) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    IsofsFile *file = iob->i_private;
    if (unit == NULL || file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }

    s32 ret = sceKernelWaitSema(unit->semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    if ((file->flags & 8) != 0) {
        ret = SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
    } else {
        IsofsReadSectorsArgs readArgs;
        readArgs.flags = file->readFlags;
        readArgs.reserved = 0;
        readArgs.unit = unit;
        readArgs.file = file;
        readArgs.outsize = size;
        readArgs.outdata = outdata;
        ret = sceKernelExtendKernelStack(0x1800, isofsHandlerReadSectorsArgs, &readArgs);
        if ((file->flags & 8) != 0) {
            ret = SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
        } else if (ret >= 0) {
            file->fpos += ret;
            iob->i_fpos += ret;
            if ((u64) file->fpos > (u64) file->size) {
                file->fpos = file->size;
                iob->i_fpos = file->size;
            }
        }
    }
    sceKernelSignalSema(unit->semaId, 1);
    return sceUmd_040A7090(ret);
}

// 00003bd4
SceOff isofsDrvLseek(SceIoIob *iob, SceOff ofs, int whence) {
    if (iob == NULL) {
        return (s32) SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    IsofsFile *file = iob->i_private;
    if (unit == NULL || file == NULL) {
        return (s32) SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 ret = sceKernelWaitSema(unit->semaId, 1, NULL);
    if (ret >> 0x1f < 0) {
        return ret;
    }
    if ((file->flags & 8) != 0) {
        sceKernelSignalSema(unit->semaId, 1);
        return (s32) SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
    }
    if ((iob->i_flgs & 8U) != 0) {
        sceKernelSignalSema(unit->semaId, 1);
        return (s32) SCE_ERROR_ERRNO_IS_DIRECTORY;
    }
    SceOff seekRet = isofsLseek(file, ofs, whence);
    if (seekRet > -1) {
        iob->i_fpos = seekRet;
    }
    sceKernelSignalSema(unit->semaId, 1);
    return seekRet;
}

// 00003d0c
int isofsDrvIoctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen) {
    if (iob == NULL || 0 >= cmd) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    if (unit == NULL || g_isofsMounted == 0) {
        return SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
    }
    IsofsFile *file = iob->i_private;
    if (file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 ret = sceKernelWaitSema(unit->semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    if ((file->flags & 8) != 0) {
        ret = SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
    } else {
        ret = isofsIoctl(iob, cmd, indata, inlen, outdata, outlen);
        if ((file->flags & 8) != 0) {
            ret = SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
        }
    }
    sceKernelSignalSema(unit->semaId, 1);
    return sceUmd_040A7090(ret);
}

// 00003e4c
int isofsDrvDevctl(SceIoIob *iob, char *dev, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen) {
    if (iob == NULL || dev == NULL || cmd <= 1) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    if (unit == NULL || g_isofsMounted == 0) {
        return SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
    }
    u32 devLen = strlen(dev);
    if (devLen >= 0x20) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 ret = sceKernelWaitSema(unit->semaId, 1, NULL);
    if (ret < 0) {
        return ret;
    }
    ret = 0;
    if (cmd > 0x208811) {
        ret = sceIoDevctl(dev, cmd, indata, inlen, outdata, outlen);
    }
    sceKernelSignalSema(unit->semaId, 1);
    return sceUmd_040A7090(ret);
}

// 00003f8c
s32 isofsUmdMountedCb(int id, IsofsUnit *unit, int arg) {
    s32 ret = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    if (id == 0x20 && (ret = SCE_ERROR_ERRNO_INVALID_ARGUMENT, unit != NULL) &&
#ifndef INSTALLER
        (ret = sceKernelWaitSema(g_isofsMgr.semaId, 1, NULL), -1 < ret)) {
#else
        (ret = 0, -1 < ret)) {
#endif
        if (unit->unitNum < 0) {
#ifndef INSTALLER
            sceKernelSignalSema(g_isofsMgr.semaId, 1);
#endif
            ret = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        } else if (g_isofsMounted == 0 || arg != 0) {
            isofsClearCurrentDirLbn();
            while (1) {
                ret = isofsUnitSetLbns(unit->blockDev, unit->unitNum);
                if (ret == SCE_ERROR_OK) {
                    break;
                }
                u32 umdErrDelta = ret + 0x7fdeef60;
                if (ret + 0x7fdeffffU < 3 || ret == (s32) 0xc0210004 || ret == (s32) SCE_ERROR_ERRNO_INVALID_ARGUMENT ||
                    (ret = SCE_UMD_ERROR_NO_MEDIUM, umdErrDelta < 2)) {
#ifndef INSTALLER
                    sceKernelSignalSema(g_isofsMgr.semaId, 1);
#endif
                    return ret;
                }
            }
            ret = isofsReadPvd(unit->blockDev, unit->unitNum);
            if (ret < 0) {
#ifndef INSTALLER
                sceKernelSignalSema(g_isofsMgr.semaId, 1);
#endif
                return ret;
            }
            while (1) {
                isofsFreeUnitPaths(unit);
                ret = isofsCreatePaths(unit);
                if (ret == 0) {
                    break;
                }
                u32 umdErrDelta = ret + 0x7fdeef60U;
                if (ret + 0x7fdeffffU < 2 || ret == (s32) SCE_ERROR_ERRNO_INVALID_ARGUMENT ||
                    (ret == (s32) SCE_ERROR_ERRNO_NO_MEMORY || ret == (s32) SCE_UMD_ERROR_NO_MEDIUM) ||
                    (ret == (s32) 0xc0210004 || (ret = (s32) SCE_UMD_ERROR_NO_MEDIUM, umdErrDelta < 2))) {
#ifndef INSTALLER
                    sceKernelSignalSema(g_isofsMgr.semaId, 1);
#endif
                    return ret;
                }
            }
            unit->mountState = 2;
            if (g_isofsHasMountedOnce == 0) {
                g_isofsHasMountedOnce = 1;
            } else {
                sceUmdSetDriveStatus(4);
                s32 intr = sceKernelCpuSuspendIntr();
                IsofsDir *openDirs = g_isofsOpenDirs;
                if (unit->openState == 1) {
                    unit->openState = 2;
                    openDirs = g_isofsOpenDirs;
                }
                for (; openDirs != NULL; openDirs = openDirs->nextDir) {
                    openDirs->compatFlag = (u32) (memcmp(openDirs->umdId, unit->umdId, UMD_ID_SIZE) != 0);
                }
                for (s32 iFdw = 0; iFdw < 2; iFdw++) {
                    for (s32 iFile = 0; iFile < unit->fdw[iFdw].fileDesc.filesCount; iFile++) {
                        IsofsFile *file = &unit->fdw[iFdw].fileDesc.files[iFile];
                        if (file->flags != 0) {
                            if (memcmp(file->umdId, unit->umdId, UMD_ID_SIZE) == 0) {
                                file->flags = file->flags & 0xf7;
                            } else {
                                file->flags = file->flags | 8;
                            }
                        }
                    }
                }
                sceKernelCpuResumeIntr(intr);
            }
            g_isofsMounted = 1;
#ifndef INSTALLER
            sceKernelSignalSema(g_isofsMgr.semaId, 1);
#endif
        } else {
#ifndef INSTALLER
            sceKernelSignalSema(g_isofsMgr.semaId, 1);
#endif
            ret = 0;
        }
    }
    return ret;
}

// 000042c8
s32 isofsInit() {
    s32 driveType = sceUmdMan_driver_65E2B3E0();
    if (driveType == 0) {
        g_isofsMgr.driveType = 1;
    } else {
        g_isofsMgr.driveType = 2;
    }
    g_isofsMgr.fdwCount = 2;
    g_isofsMgr.maxUnits = 1;
    g_isofsMgr.maxFiles = 32;
    g_isofsMgr.initialized = 1;
    return sceIoAddDrv(&g_isofsDrv) < 0;
}

// 00004330
s32 isofsRebootBefore() {
    for (s32 i = 0; i < g_isofsMgr.maxUnits; i++) {
        IsofsUnit *unit = isofsGetUnit(i);
        if (unit != NULL && unit->semaId >= 0) {
            sceKernelDeleteSema(unit->semaId);
        }
    }
    if (g_isofsMgr.semaId >= 0) {
        sceKernelDeleteSema(g_isofsMgr.semaId);
    }
    return SCE_ERROR_OK;
}

// 000043c8
IsofsMgr * __attribute__ ((noinline)) isofsGetMgr() {
    return &g_isofsMgr;
}

// 000043d4
s32 isofsHandlerReadSectorsArgs(void *args) {
    IsofsReadSectorsArgs *a = args;
    if (a == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return isofsHandlerReadSectors(a->unit, a->file, a->outsize, a->outdata, a->outsize, a->flags, 0);
}

// 00004418
int isofsDrvChdir(__attribute__((unused)) SceIoIob *iob, __attribute__((unused)) char *path) {
    return SCE_ERROR_OK;
}

// 00004420
void isofsUnlinkOpenedDir(IsofsDir *dir) {
    IsofsDir *prev = dir->prevDir;
    IsofsDir *next = dir->nextDir;
    if (prev == NULL) {
        g_isofsOpenDirs = next;
        if (next != NULL) {
            next->prevDir = NULL;
        }
    } else {
        prev->nextDir = next;
        if (next != NULL) {
            next->prevDir = prev;
        }
    }
}

// 00004454
s32 isofsReadPvd(char *blockDev, s32 unitNum) {
    if ((unitNum >> 0x1f | (u32) (blockDev == NULL)) != 0) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(unitNum);
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
    }
    unit->unitNum = (s16) unitNum;
    return isofsHandlerReadPvd(unit, NULL, 0);
}

// 000044bc
s32 isofsUnitSetLbns(char *blockDev, s32 unitNum) {
    if (((u32) (blockDev == NULL) | unitNum >> 0x1f) != 0) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(unitNum);
    s32 ret = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    if (unit != NULL && (ret = isofsHandlerGetSectorSize(unit), ret > -1) &&
        (ret = isofsHandlerGetTotalNumberOfSectors(unit), ret > -1)) {
        unit->lastLbn = unit->totalSectors + 1;
        ret = isofsHandlerGetCurrentLbn(unit);
        if (ret > -1) {
            unit->remainingLbns = unit->currentLbn - unit->lastLbn + 1;
            unit->nextLbn = unit->currentLbn + 1;
            unit->cachedCurrentLbn = unit->currentLbn;
        }
    }
    return ret;
}

// 00004574
s32 isofsIoctl(SceIoIob *iob, s32 cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen) {
    if (iob == NULL || cmd < 1) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    IsofsUnit *unit = isofsGetUnit(iob->i_unit);
    IsofsFile *file = iob->i_private;
    if (unit == NULL || file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }

    if (cmd == 0x101000a) {
        if (indata == NULL || inlen < 4) {
            return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        }
        return isofsHandlerPrepareIntoCache(unit, file, *(s32 *) indata, 1);
    }
    if (cmd < 0x101000b) {
        if (cmd < 0x208083) {
            if (cmd > 0x208080) {
                return SCE_ERROR_OK;
            }
            if (cmd == 0x20800c) {
                return SCE_ERROR_OK;
            }
            if (cmd < 0x20800d) {
                if (cmd == 0x208001) {
                    return SCE_ERROR_OK;
                }
                if (cmd == 0x208006) {
                    return SCE_ERROR_OK;
                }
            } else if (cmd - 0x208010U < 2) {
                return SCE_ERROR_OK;
            }
        } else {
            if (cmd == 0x100000e) {
                return isofsHandlerCacheResume(unit, file);
            }
            if (cmd < 0x100000f) {
                if (cmd == 0x100000d) {
                    return isofsHandlerCacheSuspend(unit, file);
                }
            } else {
                if (cmd == 0x1010005) {
                    if (file == NULL) {
                        return SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
                    }
                    if (indata == NULL || inlen < 4) {
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                    const IsofsIoctlSeekParam *seekParam = (const IsofsIoctlSeekParam *) indata;
                    SceOff pos = isofsLseek(file, seekParam->offset, seekParam->whence);
                    if ((s32) pos < 0) {
                        return pos;
                    }
                    u32 sectorOff = 0;
                    if (pos >= 0x800) {
                        s64 adjPos = pos + (u64) ((u32) (pos >> 0x3f) >> 0x15);
                        sectorOff = (u32) adjPos >> 0xb | (s32) ((u64) adjPos >> 0x20) * 0x200000;
                    }
                    iob->i_fpos = pos;
                    return isofsHandlerSeek(unit, file->lbn + sectorOff);
                }
                if (cmd == 0x1010009) {
                    if (indata == NULL || inlen < 4) {
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                    return isofsHandlerPrepareIntoCache(unit, file, *(s32 *) indata, 0);
                }
            }
        }
    } else {
        if (cmd == 0x1020006) {
            if (outdata == NULL || outlen < 4) {
                return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
            }
            if (file == NULL) {
                return SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
            }
            *(u32 *) outdata = file->lbn;
            return SCE_ERROR_OK;
        }
        if (cmd < 0x1020007) {
            if (cmd == 0x1020002) {
                if (outdata == NULL) {
                    return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                }
                if (unit->pathTableSize <= outlen) {
                    memcpy(outdata, unit->pathTable, unit->pathTableSize);
                    return SCE_ERROR_OK;
                }
                return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
            }
            if (cmd > 0x1020002) {
                u32 size = 0;
                if (cmd == 0x1020003) {
                    size = 0x800;
                    if (outdata == NULL || outlen < 4) {
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                } else {
                    if (cmd != 0x1020004) {
                        return SCE_ERROR_ERRNO_NOT_SUPPORTED;
                    }
                    if (outlen < 4) {
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                    size = (u32) file->fpos;
                }
                *(u32 *) outdata = size;
                return SCE_ERROR_OK;
            }
            if (cmd == 0x101000b) {
                return isofsHandlerCacheControl(unit, file, 0, NULL);
            }
            if (cmd == 0x1020001) {
                if (outdata == NULL || outlen < 0x800) {
                    return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                }
                u8 *pvd = unit->primaryVolumeDescriptor;
                if ((((u32) pvd | (u32) outdata) & 3) != 0) {
                    __builtin_memcpy(outdata, unit->primaryVolumeDescriptor, 0x800);
                } else {
                    // Faster copy to aligned pointer
                    u32 *pvd32 = (u32 *) pvd;
                    u32 *pvdEnd = pvd32 + 0x200;
                    u32 *out32 = outdata;
                    do {
                        u32 wordA = pvd32[0];
                        u32 wordB = pvd32[1];
                        u32 wordC = pvd32[2];
                        u32 wordD = pvd32[3];
                        out32[0] = wordA;
                        out32[1] = wordB;
                        out32[2] = wordC;
                        out32[3] = wordD;
                        pvd32 += 4;
                        out32 += 4;
                    } while (pvd32 != pvdEnd);
                }
                return SCE_ERROR_OK;
            }
        } else {
            if (cmd == 0x103000c) {
                if (outdata == NULL || outlen < 4) {
                    return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                }
                return isofsHandlerCacheControl(unit, file, 1, outdata);
            }
            if (cmd < 0x103000d) {
                if (cmd == 0x1020007) {
                    if (outdata == NULL || outlen < 8) {
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                    if (file == NULL) {
                        return SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
                    }
                    *(SceOff *) outdata = file->size;
                    return SCE_ERROR_OK;
                }
                if (cmd == 0x1030008) {
                    if (file == NULL) {
                        return SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
                    }
                    if (indata == NULL || inlen < 4) {
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                    if (outdata == NULL) {
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                    IsofsReadSectorsArgs args;
                    args.unit = unit;
                    args.file = file;
                    args.flags = file->readFlags | 1;
                    args.reserved = 0;
                    args.outdata = outdata;
                    args.outsize = *(s32 *) indata;
                    if (outlen < (u32) args.outsize) {
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                    s32 ret = sceKernelExtendKernelStack(0x2000, isofsHandlerReadSectorsArgs, &args);
                    if ((file->flags & 8) != 0) {
                        return SCE_ERROR_ERRNO_INVALID_FILE_DESCRIPTOR;
                    }
                    if (ret < 0) {
                        return ret;
                    }
                    file->fpos += ret;
                    iob->i_fpos += ret;
                    if ((u64) file->fpos > (u64) file->size) {
                        file->fpos = file->size;
                        iob->i_fpos = file->size;
                    }
                    return ret;
                }
            } else {
                if (cmd == 0x1118011) {
                    if (indata == NULL) {
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                    return isofsHandlerDevctlExt(unit, 1, indata, inlen, 0, 0);
                }
                if (cmd == 0x1138001) {
                    if (indata == NULL || 3 < inlen) {
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                    if (outdata != NULL) {
                        IsofsReadSectorsArgs args;
                        args.unit = unit;
                        args.file = file;
                        args.flags = 6;
                        args.reserved = 0;
                        args.outdata = outdata;
                        args.outsize = *(s32 *) indata;
                        if (outlen < (u32) args.outsize) {
                            return sceKernelExtendKernelStack(0x1800, isofsHandlerReadSectorsArgs, &args);
                        }
                        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                    }
                    return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
                }
            }
        }
    }
    return SCE_ERROR_ERRNO_NOT_SUPPORTED;
}

// 00004c8c
SceOff isofsLseek(IsofsFile *file, SceOff pos, int whence) {
    if (file == NULL) {
        return (s32) SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    SceOff newPos;
    if (whence == SCE_SEEK_SET) {
        newPos = pos;
    } else if (whence == SCE_SEEK_CUR) {
        newPos = file->fpos + pos;
    } else if (whence == SCE_SEEK_END) {
        newPos = file->size + pos;
    } else {
        return (s32) SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (newPos > file->size || newPos < 0) {
        return (s32) SCE_ERROR_ERRNO_IO_ERROR;
    }
    file->fpos = newPos;
    return newPos;
}

// 00004d84
s32 isofsRegisterOnUmdMounted(IsofsUnit *unit) {
    s32 ret = sceUmdManRegisterInsertEjectUMDCallBack(0x20, isofsUmdMountedCb, unit);
    if (ret > 0) {
        return SCE_ERROR_OK;
    }
    return ret;
}

// 00004db4
s32 __attribute__ ((noinline)) isofsUmdManUnRegisterInsertEjectUMDCallBack() {
    sceUmdManUnRegisterInsertEjectUMDCallBack(0x20);
    return SCE_ERROR_OK;
}

// 00004dd4
