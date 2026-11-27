#include "isofs_int.h"

s32 isofsClearBlockDevHandler(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    unit->handlerIdx = -1;
    return SCE_ERROR_OK;
}

// 00006060
s32 isofsHandlerReadDirSectors(IsofsUnit *unit, s64 lbn, s32 size, void *outdata, s32 outlen, s32 cmd,
                               __attribute__((unused)) u32 reservedParam) {
    if (unit == NULL || outdata == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->readDirSectors(unit, lbn, size, outdata, outlen, cmd);
}

// 000060c0
s32 isofsHandlerReadSectors(IsofsUnit *unit, IsofsFile *file, s32 size, void *outdata, s32 outlen, u32 readFlags,
                            __attribute__((unused)) s32 reservedParam) {
    if (unit == NULL || file == NULL || outdata == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->readSectors(unit, file, size, outdata, outlen, readFlags);
}

// 00006124
s32 isofsHandlerGetCurrentLbn(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->getCurrentLbn(unit);
}

// 00006170
s32 isofsHandlerGetSectorSize(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->getSectorSize(unit);
}

// 000061bc
s32 isofsHandlerGetTotalNumberOfSectors(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->getTotalNumberOfSectors(unit);
}

// 00006208
s64 isofsHandlerSeek(IsofsUnit *unit, s64 lbn) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->seek(unit, lbn);
}

// 00006254
s32 isofsHandlerDevctlExt(IsofsUnit *unit, s32 mode, void *indata, SceSize inlen, s32 outBuf, s32 outLen) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->devctlExt(unit, mode, indata, inlen, outBuf, outLen);
}

// 000062a0
s32 isofsHandlerReadPvd(IsofsUnit *unit, void *indata, SceSize inlen) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->readPvd(unit, indata, inlen);
}

// 000062ec
s32 isofsHandlerPrepareIntoCache(IsofsUnit *unit, IsofsFile *file, s32 cacheSize, s32 getStatusFlag) {
    if (unit == NULL || file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->prepareIntoCache(unit, file, cacheSize, getStatusFlag);
}

// 00006344
s32 isofsHandlerCacheControl(IsofsUnit *unit, IsofsFile *file, s32 flag, __attribute__((unused)) void *outdata) {
    if (unit == NULL || file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->cacheControl(unit, file, flag);
}

// 0000639c
s32 isofsHandlerCacheSuspend(IsofsUnit *unit, IsofsFile *file) {
    if (unit == NULL || file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->cacheSuspend(unit, file);
}

// 000063f4
s32 isofsHandlerCacheResume(IsofsUnit *unit, IsofsFile *file) {
    if (unit == NULL || file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return g_isofsHandlers[unit->handlerIdx].funcs->cacheResume(unit, file);
}

// 0000644c
s32 isofsUmdGetSectorSize(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return sceIoDevctl(unit->blockDev, DEVCTL_GET_SECTOR_SIZE, NULL, 0, &unit->sectorSize,
                       sizeof(unit->sectorSize));
}

// 0000649c
s32 isofsUmdGetTotalNumberOfSectors(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return sceIoDevctl(unit->blockDev, DEVCTL_GET_TOTAL_NUMBER_OF_SECTORS, NULL, 0, &unit->totalSectors,
                       sizeof(unit->totalSectors));
}

// 000064ec
s32 isofsUmdGetCurrentLbn(IsofsUnit *unit) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return sceIoDevctl(unit->blockDev, DEVCTL_GET_CURRENT_LBN, NULL, 0, &unit->currentLbn,
                       sizeof(unit->currentLbn));
}

// 0000653c
s32 isofsUmdNotSupported(void) {
    return SCE_ERROR_ERRNO_NOT_SUPPORTED;
}

// 00006548
s32 isofsUmdReadPvd(IsofsUnit *unit, void *indata, SceSize inlen) {
    u8 *outdata;
    sceIoDevctl(unit->blockDev, DEVCTL_GET_PVD, indata, inlen, &outdata, sizeof(outdata));
    unit->primaryVolumeDescriptor = outdata;
    unit->rootDirLbn = _readLe32(outdata + 0x9e);
    unit->rootDirSize = _readLe32(outdata + 0xa6);
    unit->pathTableSize = _readLe32(outdata + 0x84);
    unit->pathTableLbn = _readLe32(outdata + 0x8c);
    __builtin_memcpy(unit->umdId, outdata + 0x373, UMD_ID_SIZE);
    return SCE_ERROR_OK;
}

// 000066c4
s64 isofsUmdSeek(IsofsUnit *unit, s64 lbn) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return sceIoDevctl(unit->blockDev, DEVCTL_SEEK_DISC_RAW, &lbn, sizeof(lbn), NULL, 0);
}

// 00006734
s32 isofsUmdGetUnitNum(IsofsUnit *unit, void *indata, SceSize inlen) {
    if (unit == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return sceIoDevctl(unit->blockDev, DEVCTL_GET_UNIT_NUM, indata, inlen, &unit->unitNum, sizeof(unit->unitNum));
}

// 00006788
s32 isofsUmdCacheSuspend(IsofsUnit *unit, IsofsFile *file) {
    if (unit == NULL || file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 indata = file->cacheIndex;
    return sceIoDevctl(unit->blockDev, DEVCTL_CACHE_SUSPEND, &indata, sizeof(indata), NULL, 0);
}

// 000067ec
s32 isofsUmdCacheResume(IsofsUnit *unit, IsofsFile *file) {
    if (unit == NULL || file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 indata = file->cacheIndex;
    return sceIoDevctl(unit->blockDev, DEVCTL_CACHE_RESUME, &indata, sizeof(indata), NULL, 0);
}
