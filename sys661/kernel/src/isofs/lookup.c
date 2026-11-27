#include "isofs_int.h"

IsoDirectory *sceIsofsLookupFile(char *path, IsoDirectory *isoDirs, s32 flag, s32 *outRet) {
    if (path == NULL || isoDirs == NULL || flag < 1 || outRet == NULL) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        // bug, there is no return here!
    }
    *outRet = SCE_ERROR_ERRNO_FILE_NOT_FOUND;

    char curPath[32];
    __builtin_memset(curPath, 0, 32);

    char *lastSlash = strrchr(path, '/');
    if (lastSlash == NULL) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    u32 lastSegLen = strlen(lastSlash + 1);
    if (lastSegLen >= 0x20) {
        *outRet = SCE_ERROR_ERRNO_NAME_TOO_LONG;
        return NULL;
    }

    memcpy(curPath, lastSlash + 1, lastSegLen);
    curPath[lastSegLen] = 0;

    u32 offset = 0;
    IsoDirectory *dir = isoDirs;
    while (1) {
        u32 recLen = dir->recLen;
        if (recLen == 0) {
            if (flag < 2) {
                return NULL;
            }
            while (dir->recLen == 0) {
                dir = (IsoDirectory *) (&isoDirs->earLen + offset);
                offset = offset + 1;
            }
            recLen = (u32) dir->recLen;
        }
        if (dir->name[0] != 0 && dir->name[0] != 1) {
            if (strcmp(curPath, dir->name) == 0) {
                break;
            }
        }
        offset = offset + recLen;
        dir = (IsoDirectory *) (&isoDirs->recLen + offset);
        if ((s32) offset > 0x1000) {
            return NULL;
        }
    }
    *outRet = SCE_ERROR_OK;
    return dir;
}

// 00001094
IsofsPath *sceIsofsLookupDir(IsofsUnit *unit, char *pathSplit, u32 dirLevel, int pathLen, u32 flag, s32 *outRet) {
    if (outRet == NULL) {
        return NULL;
    }
    if (pathSplit == NULL || pathLen < 1 || ((s32) dirLevel < 1 || (s32) dirLevel > 8)) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    if (unit == NULL) {
        *outRet = SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
        return NULL;
    }
    if (unit->paths == NULL || unit->pathTableEntryCount == 0) {
        *outRet = SCE_ERROR_ERRNO_IO_ERROR;
        return NULL;
    }
    char pathStack[32];
    __builtin_memset(pathStack, 0, 32);
    if (dirLevel <= 1) {
        *outRet = SCE_ERROR_ERRNO_FILE_NOT_FOUND;
        return NULL;
    }
    *outRet = SCE_ERROR_ERRNO_FILE_NOT_FOUND;
    u32 curDirNum = 1;
    u32 curDirLevel = 1;
    for (u32 iDirLevel = 2; iDirLevel <= dirLevel; iDirLevel++) {
        u32 outSegLen = 0;
        pathSplit = isofs_get_dirname(pathSplit, &outSegLen, flag, outRet);
        if (pathSplit == NULL) {
            return NULL;
        }
        if (outSegLen > 0x1f) {
            *outRet = SCE_ERROR_ERRNO_NAME_TOO_LONG;
            return NULL;
        }
        __builtin_memset(pathStack, 0, 32);
        memcpy(pathStack, pathSplit, outSegLen);
        pathStack[outSegLen] = 0;
        for (s32 iPath = 0; iPath < unit->pathTableEntryCount; iPath++) {
            if (unit->paths[iPath].dirLevel == iDirLevel) {
                if (strcmp(unit->paths[iPath].name, pathStack) == 0) {
                    if (unit->paths[iPath].parentNum == curDirNum) {
                        curDirNum = unit->paths[iPath].dirNum;
                        curDirLevel++;
                        *outRet = SCE_ERROR_OK;
                        if (dirLevel == curDirLevel) {
                            return &unit->paths[curDirNum - 1];
                        }
                        break;
                    }
                }
            }
            *outRet = SCE_ERROR_ERRNO_FILE_NOT_FOUND;
        }
    }
    return NULL;
}

// 000012e4
s32 sceIsofsGetPathTable(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 outRet = SCE_ERROR_OK;
    IsoPath *pathTable = isofs_new_ptbl(unit, &outRet);
    unit->pathTable = pathTable;
    if (pathTable == NULL) {
        return outRet;
    }
    s32 pathTableEntryCount = isofs_get_direc_count(pathTable, unit->pathTableSize, &outRet);
    if (pathTableEntryCount > 0) {
        unit->pathTableEntryCount = (u16) pathTableEntryCount;
        IsofsPath *paths = isofs_create_ptbl(unit, unit->pathTable, unit->pathTableSize, pathTableEntryCount,
                                                      &outRet);
        unit->paths = paths;
        if (paths != NULL) {
            return isofs_set_topend_dirsize(unit);
        }
    }
    isofs_delete_ptbl(unit);
    return outRet;
}

// 00001388
IsoPath *isofs_new_ptbl(IsofsUnit *unit, s32 *outRet) {
    if (outRet == NULL) {
        return NULL;
    }
    if (unit == NULL) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    u32 pathTableSize = unit->pathTableSize;
    if (pathTableSize == 0) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    u32 sizeToRead = 0x800;
    if (pathTableSize > 0x800 && (sizeToRead = pathTableSize, (pathTableSize & 0x7ff) != 0)) {
        sizeToRead = (pathTableSize & 0xfffff800) + 0x800;
    }
    void *data = sceKernelAllocHeapMemory(unit->heapId, sizeToRead);
    if (data == NULL) {
        *outRet = SCE_ERROR_ERRNO_NO_MEMORY;
        return NULL;
    }
    s32 ret = isofsHandlerReadDirSectors(unit, unit->pathTableLbn, sizeToRead, data, sizeToRead, 6, 0);
    *outRet = ret;
    if (ret >= 0) {
        return data;
    }
    sceKernelFreeHeapMemory(unit->heapId, data);
    return NULL;
}

// 00001474
s32 isofs_get_direc_count(IsoPath *pathTable, s32 pathTableSize, s32 *outRet) {
    if (pathTable == NULL || pathTableSize < 9) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return 0;
    }
    s32 count = 0;
    s32 offset = 0;
    do {
        IsoPath *path = (IsoPath *) ((u8 *) pathTable + offset);
        u8 nameLen = path->nameLen;
        s32 nextOffset = offset + (u32) nameLen;
        if (nameLen < 2 || (offset = nextOffset + 8, (nameLen & 1) != 0)) {
            offset = nextOffset + 9;
        }
        count = count + 1;
    } while (offset < pathTableSize);
    return count;
}

// 000014e0
IsofsPath *isofs_create_ptbl(IsofsUnit *unit, IsoPath *pathTable, s32 pathTableSize, s32 pathTableEntryCount,
                                      s32 *outRet) {
    if (outRet == NULL) {
        return NULL;
    }
    if (pathTable == NULL || pathTableEntryCount < 1 || pathTableSize < 10) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    IsofsPath *paths = sceKernelAllocHeapMemory(unit->heapId, pathTableEntryCount * sizeof(IsofsPath));
    if (paths == NULL) {
        *outRet = SCE_ERROR_ERRNO_NO_MEMORY;
        return NULL;
    }
    memset(paths, 0, pathTableEntryCount * sizeof(IsofsPath));

    s32 isoOffset = 0;
    IsofsPath *currentPath = paths;
    u32 prevParentNum = 1;
    u32 dirNum = 1;
    u32 dirLevel = 1;
    u32 dirLevelNext;
    s32 currentPathIndex = 0;
    u32 isoPathLen;
    const u8 nullByte = 0;
    do {
        IsoPath *isoPath = (IsoPath *) ((u8 *) pathTable + isoOffset);
        u32 nameLen = isoPath->nameLen;
        if ((nameLen < 2 || (isoPathLen = nameLen + 8, (nameLen & 1) != 0)) &&
            (nameLen == 0 || (isoPathLen = nameLen + 9, (nameLen & 1) == 0))) {
            do {
                if (isoPath->nameLen != 0) {
                    break;
                }
                isoOffset = isoOffset + 1;
                isoPath = (IsoPath *) ((u8 *) pathTable + isoOffset);
            } while (isoOffset < pathTableSize);
            nameLen = isoPath->nameLen;
            if ((nameLen < 2 || (isoPathLen = nameLen + 8, (nameLen & 1) != 0)) &&
                (isoPathLen = 0, nameLen != 0) && (isoPathLen = nameLen + 9, (nameLen & 1) == 0)) {
                isoPathLen = 0;
            }
        }

        memcpy(currentPath, isoPath, isoPathLen);
        currentPath->name[isoPathLen - 8] = 0;
        if (currentPathIndex == 0) {
            currentPath->lbnSize = 1;
        } else {
            s32 prevLbn = currentPath[-1].lbn;
            currentPath->lbnSize = 1;
            s32 lbnDiff = currentPath->lbn - prevLbn;
            currentPath[-1].lbnSize = lbnDiff;
            if (lbnDiff == 0) {
                currentPath[-1].lbnSize = 1;
            }
        }

        if (currentPath->parentNum == prevParentNum &&
            memcmp(currentPath->name, &nullByte, currentPath->nameLen) == 0) {
            dirLevelNext = dirLevel + 1;
            currentPath->dirLevel = dirLevel;
            currentPath->dirNum = dirNum;
        } else {
            s32 updateEntry = 1;
            if (currentPath->parentNum != prevParentNum ||
                memcmp(currentPath->name, &nullByte, currentPath->nameLen) == 0) {
                dirLevelNext = dirLevel;
                if (prevParentNum == 0) {
                    updateEntry = 0;
                } else if (paths[currentPath->parentNum - 1].dirLevel != paths[prevParentNum - 1].dirLevel) {
                    dirLevel = dirLevel + 1;
                }
            }
            if (updateEntry) {
                dirNum = dirNum + 1;
                currentPath->dirNum = dirNum;
                currentPath->dirLevel = dirLevel;
                dirLevelNext = dirLevel;
            }
        }

        isoOffset = isoOffset + isoPathLen;
        currentPathIndex = currentPathIndex + 1;
        prevParentNum = (u32) currentPath->parentNum;
        currentPath = currentPath + 1;
        dirLevel = dirLevelNext;
        if (pathTableSize <= isoOffset) {
            return paths;
        }
    } while (1);
}

// 0000177c
s32 isofs_delete_ptbl(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (unit->pathTable != NULL) {
        sceKernelFreeHeapMemory(unit->heapId, unit->pathTable);
        unit->pathTable = NULL;
    }
    if (unit->paths != NULL) {
        sceKernelFreeHeapMemory(unit->heapId, unit->paths);
        unit->paths = NULL;
    }
    return SCE_ERROR_OK;
}

// 000017e4
s32 isofsCheckPath(char *name, u32 flag) {
    if (name == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (flag != 1 && flag != 2 && flag != 3) {
        return SCE_ERROR_ERRNO_FILE_NOT_FOUND;
    }
    s32 i = 0;
    while (1) {
        u32 nameLen = strlen(name);
        if ((int) nameLen <= i) {
            return SCE_ERROR_OK;
        }
        char ch = name[i];
        i++;
        u32 ctype = look_ctype_table(ch);
        if ((ctype & 7) == 0 && ch != '_' && ch != ';' && ch != '/' && ch != '.') {
            return SCE_ERROR_ERRNO_FILE_NOT_FOUND;
        }
    }
}

// 00001934
s32 isofsIsSceLbnPath(char *path) {
    if (path == NULL) {
        return 0;
    }
    char *sep;
    if (path[0] != '/' && (sep = strchr(path, '/'), sep == NULL)) {
        return 2;
    }
    if (strncmp(path, "/sce_lbn", 8) != 0) {
        return 2;
    }
    return 3;
}

// 000019a0
IsoDirectory *sceIsofsReadDirectoryRecord(IsofsUnit *unit, char *path, char *file, u32 flag, s32 *outRet) {
    *outRet = SCE_ERROR_OK;
    if (unit == NULL || path == NULL || file == NULL) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }

    // Count how many directory levels are in the path
    char *segPath = path;
    u32 segLen = 0;
    int levels = 1;
    while (1) {
        char *seg = strchr(segPath, '/');
        segPath = seg + 1;
        if (seg == NULL) {
            break;
        }
        levels = levels + 1;
        segLen = strlen(segPath);
    }

    // Construct new path to directory (without the last segment, i.e. file name)
    u32 pathLen = strlen(path);
    char pathStack[256];
    memset(pathStack, 0, 256);
    memcpy(pathStack, path, pathLen - segLen);
    pathStack[pathLen - segLen] = 0;

    // Lookup the directory LBN in path table
    s32 readLbn;
    if (strcmp(pathStack, "/") == 0) {
        readLbn = unit->rootDirLbn;
    } else {
        IsofsPath *foundPath = sceIsofsLookupDir(unit, pathStack, levels - 1, strlen(pathStack), flag, outRet);
        if (*outRet != 0) {
            if (*outRet != 0x20) {
                return NULL;
            }
            foundPath = unit->paths;
        }
        readLbn = foundPath->lbn;
    }

    // Check some UMD ids for compatible behaviour
    s32 idMatch = memcmp("ULUS-10013", unit->umdId, 10);
    if (idMatch == 0 || (idMatch = memcmp("ULJM-05006", unit->umdId, 10), idMatch == 0) ||
        (idMatch = memcmp("ULES-00123", unit->umdId, 10), idMatch == 0)) {
        g_isofsCurrentDirLbn = 0;
    }

    s32 openState;
    if (readLbn == g_isofsCurrentDirLbn) {
        // Required LBN is already read into the global
        s32 sdkVer = sceKernelGetCompiledSdkVersion();
        if (sdkVer < 0x3000000) {
            if (memcmp("ULJM-05127", unit, 10) != 0) {
                sceKernelDelayThread(1000);
            }
        }
        openState = unit->openState;
    } else {
        // Read the directory
        s32 ret = sceIsofsReadDirent(unit, readLbn, g_isofsCurrentDir);
        if (ret != 0) {
            *outRet = ret;
            return NULL;
        }
        openState = unit->openState;
        g_isofsCurrentDirLbn = readLbn;
    }

    u32 dirSize = ((IsoDirectory *) g_isofsCurrentDir)->size;
    if (openState == 2) {
        g_isofsCurrentDirLbn = 0;
        *outRet = SCE_UMD_ERROR_NOT_DEVICE_READY;
        return NULL;
    }
    IsoDirectory *foundDir;
    u32 noMoreDirSectors = ((IsoDirectory *) g_isofsCurrentDir)->size < 0x1000;
    u32 dirOffset = 0;
    do {
        readLbn = readLbn + 2;
        dirOffset = dirOffset + 0x1000;
        u32 findFlag;
        if (noMoreDirSectors == 0 || (s32) (dirSize - dirOffset) >= 0x1000) {
            findFlag = 2;
        } else {
            memset(g_isofsCurrentDir + 0x800, 0, 0x800);
            findFlag = 1;
        }
        foundDir = sceIsofsLookupFile(path, (IsoDirectory *) g_isofsCurrentDir, findFlag, outRet);
        if (foundDir != NULL) {
            break;
        }
        idMatch = memcmp("ULUS-10013", unit->umdId, 10);
        if (idMatch == 0 ||
            (idMatch = memcmp("ULJM-05006", unit->umdId, 10), idMatch == 0) ||
            (idMatch = memcmp("ULES-00123", unit->umdId, 10), idMatch == 0)) {
            g_isofsCurrentDirLbn = 0;
        }
        if (readLbn == g_isofsCurrentDirLbn) {
            s32 sdkVer = sceKernelGetCompiledSdkVersion();
            if (sdkVer < 0x3000000) {
                if (memcmp("ULJM-05127", unit, 10) != 0) {
                    sceKernelDelayThread(1000);
                }
            }
            openState = unit->openState;
        } else {
            // Read the directory
            s32 ret = sceIsofsReadDirent(unit, readLbn, g_isofsCurrentDir);
            if (ret != 0) {
                *outRet = ret;
                g_isofsCurrentDirLbn = 0;
                return NULL;
            }
            openState = unit->openState;
            g_isofsCurrentDirLbn = readLbn;
        }
        if (openState == 2) {
            *outRet = SCE_UMD_ERROR_NOT_DEVICE_READY;
            g_isofsCurrentDirLbn = 0;
            return NULL;
        }
    } while (dirOffset < dirSize);

    *outRet = SCE_ERROR_OK;
    return foundDir;
}

// 00001dbc
s32 sceIsofsCheckVFile(char *path, s32 *outLbn, s32 *outSize) {
    if (path == NULL || outLbn == NULL || outSize == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (strlen(path) > 0x1f) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (strncmp(path, "/sce_lbn", 8) != 0) {
        return SCE_ERROR_ERRNO_FILE_NOT_FOUND;
    }
    *outLbn = strtol(path + 8, NULL, 0x10);
    char *size = strstr(path, "_size");
    if (size == NULL) {
        return SCE_ERROR_ERRNO_FILE_NOT_FOUND;
    }
    *outSize = strtol(size + 5, NULL, 0x10);
    return SCE_ERROR_OK;
}

// 00002274
int sceIsofsReadDirent(IsofsUnit *unit, u32 lbn, void *outdata) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 size = (s32) unit->sectorSize * 2;
    s32 ret = isofsHandlerReadDirSectors(unit, lbn, size, outdata, size, 6, 0);
    if (ret > 0) {
        return SCE_ERROR_OK;
    }
    return ret;
}

// 00001e9c
IsoDirectory *isofsSetDirentFromIsoDirectory(IsofsDir *dir, SceIoDirent *dirent, s32 *outRet) {
    if (dir == NULL) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    IsoDirectory *isoDir = dir->isoDir;
    if (isoDir == NULL || dirent == NULL) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    u32 recLen = isoDir->recLen;
    u32 dirRecLen = dir->dirRecLen + recLen;
    dir->dirRecLen = dirRecLen;
    if (recLen == 0) {
        if (dir->lbnSize > 1) {
            u32 dirLbn = (dirRecLen & 0xfffff800) + 0x800;
            dir->lbnSize = dir->lbnSize - 1;
            dir->dirRecLen = dirLbn;
            return (IsoDirectory *) ((u32) isoDir + (dirLbn - dirRecLen));
        }
        *outRet = SCE_ERROR_ERRNO_FILE_NOT_FOUND;
        return NULL;
    }
    if (isoDir->name[0] == 0) {
        dirent->d_name[0] = '.';
        dirent->d_name[1] = 0;
        dir->lbnSize = isoDir->size >> 0xb;
        dirent->d_stat.st_size = isoDir->size;
    }
    memcpy(dirent->d_name, isoDir->name, isoDir->nameLen);
    dirent->d_name[isoDir->nameLen] = 0;
    dirent->d_stat.st_private[0] = isoDir->lbn;
    dirent->d_stat.st_private[1] = 0;
    dirent->d_stat.st_private[2] = 0;
    dirent->d_stat.st_private[3] = 0;
    dirent->d_stat.st_private[4] = 0;
    dirent->d_stat.st_private[5] = 0;
    dirent->d_stat.st_size = isoDir->size;
    dirent->d_stat.st_attr = 0;
    u16 year = isoDir->dateTime.yearsSince1900 + 1900;
    dirent->d_stat.st_atime.year = year;
    dirent->d_stat.st_mtime.year = year;
    dirent->d_stat.st_ctime.year = year;
    u8 isoMonth = isoDir->dateTime.month;
    dirent->d_stat.st_atime.month = isoMonth;
    dirent->d_stat.st_mtime.month = isoMonth;
    dirent->d_stat.st_ctime.month = isoMonth;
    u8 isoDay = isoDir->dateTime.day;
    dirent->d_stat.st_atime.day = isoDay;
    dirent->d_stat.st_mtime.day = isoDay;
    dirent->d_stat.st_ctime.day = isoDay;
    u8 isoHour = isoDir->dateTime.hour;
    dirent->d_stat.st_mtime.hour = isoHour;
    dirent->d_stat.st_atime.hour = isoHour;
    dirent->d_stat.st_ctime.hour = isoHour;
    u8 isoMinute = isoDir->dateTime.minute;
    dirent->d_stat.st_atime.minute = isoMinute;
    dirent->d_stat.st_mtime.minute = isoMinute;
    dirent->d_stat.st_ctime.minute = isoMinute;
    u8 isoSecond = isoDir->dateTime.second;
    dirent->d_stat.st_atime.second = isoSecond;
    dirent->d_stat.st_mtime.second = isoSecond;
    dirent->d_stat.st_ctime.second = isoSecond;
    dirent->d_stat.st_mtime.microsecond = 0;
    dirent->d_stat.st_ctime.microsecond = 0;
    dirent->d_stat.st_atime.microsecond = 0;
    dirent->d_stat.st_mode = 0x16d;
    s32 sdkVer = sceKernelGetCompiledSdkVersion();
    if (sdkVer >= 0x2000011) {
        u32 dateTimeOffset = isoDir->dateTime.offset;
        if (dateTimeOffset != 0) {
            ScePspDateTime time;
            u64 tick;
            s32 ret = sceRtcGetTick((pspTime *) &dirent->d_stat.st_ctime, &tick);
            *outRet = ret;
            if (ret == 0) {
                if (dateTimeOffset - 0xd1 < 0x2f) {
                    u64 numMs = (s64) (s32) (0x100 - dateTimeOffset) * 900000000;
                    sceRtcTickAddTicks(&tick, &tick, numMs);
                } else if (dateTimeOffset - 1 < 0x34) {
                    u64 numMs = (s64) (s32) dateTimeOffset * 900000000;
                    sceRtcTickAddTicks(&tick, &tick, numMs);
                }
                ret = sceRtcSetTick((pspTime *) &time, &tick);
                *outRet = ret;
                if (ret == 0) {
                    dirent->d_stat.st_atime.year = time.year;
                    dirent->d_stat.st_atime.month = time.month;
                    dirent->d_stat.st_atime.day = time.day;
                    dirent->d_stat.st_atime.hour = time.hour;
                    dirent->d_stat.st_atime.minute = time.minute;
                    dirent->d_stat.st_atime.second = time.second;
                    dirent->d_stat.st_atime.microsecond = time.microsecond;
                    dirent->d_stat.st_mtime.year = time.year;
                    dirent->d_stat.st_ctime.year = time.year;
                    dirent->d_stat.st_mtime.month = time.month;
                    dirent->d_stat.st_ctime.month = time.month;
                    dirent->d_stat.st_mtime.day = time.day;
                    dirent->d_stat.st_ctime.day = time.day;
                    dirent->d_stat.st_mtime.hour = time.hour;
                    dirent->d_stat.st_ctime.hour = time.hour;
                    dirent->d_stat.st_mtime.minute = time.minute;
                    dirent->d_stat.st_ctime.minute = time.minute;
                    dirent->d_stat.st_mtime.second = time.second;
                    dirent->d_stat.st_ctime.second = time.second;
                    dirent->d_stat.st_mtime.microsecond = time.microsecond;
                    dirent->d_stat.st_ctime.microsecond = time.microsecond;
                }
            }
        }
    }
    if ((isoDir->flags & 2) == 0) {
        dirent->d_stat.st_mode = dirent->d_stat.st_mode | 0x2000;
    } else {
        dirent->d_stat.st_mode = dirent->d_stat.st_mode | 0x1000;
    }
    isoDir = dir->isoDir;
    *outRet = SCE_ERROR_OK;
    return (IsoDirectory *) (&isoDir->recLen + recLen);
}

// 000022c0
s32 isofsFreeUnitPaths(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return isofs_delete_ptbl(unit);
}

// 000022f0
void __attribute__ ((noinline)) isofsClearCurrentDirLbn() {
    g_isofsCurrentDirLbn = 0;
}

// 000022fc
char *isofs_get_dirname(char *path, u32 *outLen, u32 flag, s32 *outRet) {
    if (path == NULL || outLen == NULL || outRet == NULL) {
        return NULL;
    }
    char *slash;
    if (flag - 2 < 4) {
        char *lastSlash = strrchr(path, '/');
        if (lastSlash == NULL || strchr(path + 1, '/') == NULL || (slash = strchr(path, '/'), slash == NULL)) {
            *outRet = SCE_ERROR_OK;
            return NULL;
        }
        path = slash + 1;
        char *firstSlash = strchr(path, '/');
        if (firstSlash == NULL) {
            *outLen = strlen(path);
        } else {
            *outLen = (int) firstSlash - (int) path;
        }
    } else {
        if (flag != 1) {
            *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
            return NULL;
        }
        slash = strchr(path, '/');
        if (slash != NULL) {
            path = slash + 1;
            char *firstSlash = strchr(path, '/');
            if (firstSlash == NULL) {
                *outLen = strlen(path);
            } else {
                *outLen = (int) firstSlash - (int) path;
            }
            return path;
        }
        *outRet = SCE_ERROR_OK;
        *outLen = strlen(path);
    }
    return path;
}

// 0000241c
s32 isofs_set_topend_dirsize(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    u32 rootDirSize = unit->rootDirSize;
    if (rootDirSize < 0x800) {
        unit->paths->lbnSize = 1;
    } else if ((rootDirSize & 0x7ff) == 0) {
        unit->paths->lbnSize = rootDirSize >> 0xb;
    } else {
        unit->paths->lbnSize = (rootDirSize >> 0xb) + 1;
    }
    return SCE_ERROR_OK;
}

// 00002474
