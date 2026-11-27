#include "isofs_int.h"

s32 isofsSetBlockDevHandler(IsofsUnit *unit) {
    if (unit == NULL || unit == (IsofsUnit *) 0xffffffe0) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    char *blockDev = unit->blockDev;
    s32 blockDevLen = strlen(blockDev);
    if (blockDevLen > 0x100) {
        return SCE_ERROR_ERRNO_NAME_TOO_LONG;
    }
    int leadSpaces = 0;
    if (blockDevLen > 0) {
        do {
            u32 ctype = look_ctype_table(unit->blockDev[leadSpaces]);
            if ((ctype & 8) == 0) {
                break;
            }
            leadSpaces = leadSpaces + 1;
            blockDev = blockDev + 1;
        } while (leadSpaces < blockDevLen);
    }
    int handlerNameLen = 0;
    if (blockDevLen - leadSpaces > 0) {
        do {
            char *blockDevOff = blockDev + handlerNameLen;
            u32 ctype = look_ctype_table(*blockDevOff);
            if ((ctype & 3) == 0) {
                break;
            }
            handlerNameLen = handlerNameLen + 1;
        } while (handlerNameLen < blockDevLen - leadSpaces);
    }
    for (int i = 0; i < 4; i++) {
        if (strncmp(blockDev, g_isofsHandlers[i].blockDev, handlerNameLen) == 0) {
            unit->handlerIdx = i;
            return SCE_ERROR_OK;
        }
    }
    return SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
}

// 00005048
s32 isofsUmdCacheControl(IsofsUnit *unit, IsofsFile *file, s32 flag) {
    if (unit == NULL || file == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 indata = file->cacheIndex;
    u32 cmd = DEVCTL_CACHE_GET_STATUS;
    if (flag == 0) {
        cmd = DEVCTL_CACHE_CANCEL;
    }
    return sceIoDevctl(unit->blockDev, cmd, &indata, sizeof(indata), NULL, 0);
}

// 00004f04
s32 isofsUmdPrepareIntoCache(IsofsUnit *unit, IsofsFile *file, s32 cacheSize, s32 getStatusFlag) {
    if (unit == NULL || file == NULL || cacheSize < 1) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    s32 sectorSize = unit->sectorSize;
    PrepareIntoCacheParams params;
    params.flags = 0;
    params.reserved = 0;
    params.lbnSize = 0;
    u64 posLbn = __udivdi3(file->fpos, sectorSize);
    params.lbn = file->lbn + (int) posLbn;
    params.lbnSize = 1;
    if (cacheSize > sectorSize) {
        params.lbnSize = cacheSize / sectorSize;
        if (cacheSize % sectorSize != 0) {
            params.lbnSize = params.lbnSize + 1;
        }
    }

    if (getStatusFlag == 0) {
        return sceIoDevctl(unit->blockDev, DEVCTL_PREPARE_INTO_CACHE, &params, sizeof(PrepareIntoCacheParams), NULL, 0);
    }
    u16 statusOut[2];
    statusOut[0] = 0;
    statusOut[1] = 0;
    s32 ret = sceIoDevctl(unit->blockDev, DEVCTL_PREPARE_INTO_CACHE_GET_STATUS, &params, sizeof(PrepareIntoCacheParams),
                          statusOut, 4);
    if (ret >= 0) {
        file->cacheIndex = statusOut[0];
    }
    return ret;
}

// 000050c4
s32 isofsUmdReadDirSectors(IsofsUnit *unit, s64 lbn, s32 size, void *outdata, s32 outlen, u32 cmd) {
    if (unit == NULL || outdata == NULL || outlen < size) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }

    LbnParams params;
    params.lbn = (u32) lbn;
    params.lbnSize = 0;

    u32 lbnSize = 0;
    s32 setupFields = 1;
    const u32 sectorSize = unit->sectorSize;
    if ((int) sectorSize < size) {
        lbnSize = size / (int) sectorSize;
        if (size % (int) sectorSize != 0) {
            setupFields = 0;
        } else {
            params.byteSizeMiddle = size;
        }
    } else {
        lbnSize = 1;
        params.byteSizeMiddle = sectorSize;
    }

    if (setupFields) {
        params.byteSizeLast = 0;
        params.byteSizeFirst = 0;
        params.lbnSize = lbnSize;
        params.size = params.byteSizeMiddle;
    }
    params.flags = (u32) ((cmd & 1) != 0);
    params.cmd = (u32) ((cmd & 2) != 0);
    if ((cmd & 4) != 0) {
        params.cmd = params.cmd | 2;
    }
    if ((cmd & 8) != 0) {
        params.cmd = params.cmd | 0xf0;
    }
    return sceIoDevctl(unit->blockDev, DEVCTL_READ_SECTORS, &params, sizeof(LbnParams), outdata, params.size);
}

// 000051d8
s32 isofsUmdReadSectors(IsofsUnit *unit, IsofsFile *file, s32 size, void *outdata, s32 outlen, u32 cmd) {
    if (unit == NULL || file == NULL || outdata == NULL || outlen < size) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (size == 0) {
        return SCE_ERROR_OK;
    }

    LbnParams params;
    params.flags = (u32) ((cmd & 1) != 0);
    params.cmd = (u32) ((cmd & 2) != 0);
    params.lbn = 0;
    params.lbnSize = 0;
    params.size = 0;
    params.byteSizeMiddle = 0;
    params.byteSizeFirst = 0;
    params.byteSizeLast = 0;
    params.reserved = 0;
    if ((cmd & 4) != 0) {
        params.cmd = params.cmd | 2;
    }
    if ((cmd & 8) != 0) {
        params.cmd = params.cmd | 0xf0;
    }

    const u64 filePos = (u64) file->fpos;
    const u64 fileSize = (u64) file->size;
    const u32 fposHi = (u32) (filePos >> 32);
    const u32 fpos = (u32) filePos;

    const u64 readEnd64 = filePos + (u64) (s64) size;
    if (fileSize < readEnd64) {
        size = (s32) ((u32) fileSize - fpos);
    }
    if (size == 0) {
        return SCE_ERROR_OK;
    }

    s32 ret;
    if ((fpos | fposHi) == 0) {
        ret = isofsReadParamsZero(unit, file, outdata, size, &params);
    } else if (fposHi == 0 && fpos < unit->sectorSize) {
        ret = isofsReadParamsInitial(unit, file, outdata, size, &params);
    } else {
        volatile u32 sectorSize = unit->sectorSize; // force not to optimize the following if to match asm
        if ((fpos | fposHi) == 0 || (fposHi == 0 && fpos < sectorSize)) {
            return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        }
        ret = isofsReadParams(unit, file, outdata, size, &params);
    }
    if (ret != SCE_ERROR_OK) {
        return ret;
    }
    ret = sceIoDevctl(unit->blockDev, DEVCTL_READ_SECTORS, &params, sizeof(LbnParams), outdata, size);
    if (ret < 0) {
        return ret;
    }
    return size;
}

// 000053fc
s32 isofsReadParamsZero(IsofsUnit *unit, IsofsFile *file, void *outdata, s32 size, LbnParams *params) {
    if (unit == NULL || file == NULL || params == NULL || size == 0) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    const s32 sectorSize = unit->sectorSize;
    const u32 outdataEnd = (u32) outdata + size;
    // read smaller than sector size
    if (size <= sectorSize) {
        params->lbn = file->lbn;
        params->lbnSize = 1;
        params->size = size;
        params->byteSizeMiddle = 0;
        params->byteSizeFirst = 0;
        params->byteSizeLast = size;
        return SCE_ERROR_OK;
    }

    // read smaller than size of two sectors, outdata is NOT aligned to 64 bytes
    if (size <= sectorSize * 2 && ((u32) outdata & 0x3f) != 0) {
        params->lbn = file->lbn;
        params->lbnSize = 2;
        params->size = size;
        params->byteSizeMiddle = 0;
        params->byteSizeFirst = sectorSize;
        params->byteSizeLast = size - sectorSize;
        return SCE_ERROR_OK;
    }

    // read smaller than size of two sectors, outdata is aligned to 64 bytes
    if (size <= sectorSize * 2) {
        params->lbn = file->lbn;
        params->lbnSize = 2;
        params->size = size;
        params->byteSizeMiddle = sectorSize;
        params->byteSizeFirst = 0;
        params->byteSizeLast = size - sectorSize;
        return SCE_ERROR_OK;
    }

    // read larger than two sectors, outdata start NOT aligned, end NOT aligned
    if (size > sectorSize * 2) {
        if (size % sectorSize == 0 && ((u32) outdata & 0x3f) != 0 && (outdataEnd & 0xffffffc0) != outdataEnd) {
            params->lbn = file->lbn;
            params->lbnSize = size / sectorSize;
            params->size = size;
            params->byteSizeMiddle = size - sectorSize * 2;
            params->byteSizeFirst = sectorSize;
            params->byteSizeLast = sectorSize;
            return SCE_ERROR_OK;
        }
    }

    // read larger than two sectors, outdata start aligned, end NOT aligned
    if (size > sectorSize * 2) {
        if (size % sectorSize == 0 && (outdataEnd & 0xffffffc0) != outdataEnd) {
            params->lbn = file->lbn;
            params->lbnSize = size / sectorSize;
            params->size = size;
            params->byteSizeMiddle = size - sectorSize;
            params->byteSizeFirst = 0;
            params->byteSizeLast = sectorSize;
            return SCE_ERROR_OK;
        }
    }

    // read larger than two sectors, outdata start NOT aligned, end aligned
    if (size > sectorSize * 2) {
        if (size % sectorSize == 0 && ((u32) outdata & 0x3f) != 0) {
            params->lbn = file->lbn;
            params->lbnSize = size / sectorSize;
            params->size = size;
            params->byteSizeMiddle = size - sectorSize;
            params->byteSizeFirst = sectorSize;
            params->byteSizeLast = 0;
            return SCE_ERROR_OK;
        }
    }

    // read larger than two sectors, outdata start and end aligned
    if (size > sectorSize * 2) {
        if (size % sectorSize == 0) {
            params->lbn = file->lbn;
            params->lbnSize = size / sectorSize;
            params->size = size;
            params->byteSizeMiddle = size;
            params->byteSizeFirst = 0;
            params->byteSizeLast = 0;
            return SCE_ERROR_OK;
        }
    }

    // read larger than two sectors, size is not divisible by the sector size, outdata start NOT aligned
    if (size > sectorSize * 2) {
        if (size % sectorSize != 0 && ((u32) outdata & 0x3f) != 0) {
            u32 diff = (size - sectorSize) % sectorSize;
            params->lbn = file->lbn;
            params->lbnSize = size / sectorSize + 1;
            params->size = size;
            params->byteSizeMiddle = size - sectorSize - diff;
            params->byteSizeFirst = sectorSize;
            params->byteSizeLast = diff;
            return SCE_ERROR_OK;
        }
    }

    // read larger than two sectors, size is not divisible by the sector size
    if (size > sectorSize * 2) {
        u32 bytesOver = size % sectorSize;
        if (bytesOver == 0) {
            return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        }
        params->lbn = file->lbn;
        params->lbnSize = size / sectorSize + 1;
        params->size = size;
        params->byteSizeMiddle = size - bytesOver;
        params->byteSizeFirst = 0;
        params->byteSizeLast = bytesOver;
        return SCE_ERROR_OK;
    }

    return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
}

// 000057c8
s32 isofsReadParamsInitial(IsofsUnit *unit, IsofsFile *file, __attribute__((unused)) void *outdata, s32 size,
                           LbnParams *params) {
    if (unit == NULL || file == NULL || params == NULL || size == 0) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }

    const u32 usize = size;
    const s32 sizeMsb = (s32) usize >> 0x1f;
    const u32 sectorSize = unit->sectorSize;
    const u64 filePos = (u64) file->fpos;
    const u32 fposHi = (u32) (filePos >> 32);
    const u32 fpos = (u32) filePos;
    const u32 readEnd = fpos + usize;
    const u32 byteSizeFirst = sectorSize - fpos;

    const s32 readEndHi = (s32) fposHi + sizeMsb + (u32) (readEnd < usize);
    const s32 pastFirstSector = readEndHi != 0 || readEnd > sectorSize;
    const s32 pastSecondSector = readEndHi != 0 || readEnd > sectorSize * 2;
    const s32 withinTwoSectors = readEndHi == 0 && readEnd <= sectorSize * 2;

    // read one sector
    if (fposHi == 0 && fpos < sectorSize && readEndHi == 0 && readEnd <= sectorSize) {
        params->lbn = file->lbn;
        params->lbnSize = 1;
        params->size = usize;
        params->byteSizeMiddle = 0;
        params->byteSizeFirst = fpos;
        params->byteSizeLast = 0;
        return SCE_ERROR_OK;
    }

    // read two sectors
    if (fposHi == 0 && fpos < sectorSize) {
        if (pastFirstSector && withinTwoSectors) {
            params->lbn = file->lbn;
            params->lbnSize = 2;
            params->size = usize;
            params->byteSizeMiddle = 0;
            params->byteSizeFirst = byteSizeFirst;
            params->byteSizeLast = usize - byteSizeFirst;
            return SCE_ERROR_OK;
        }
    }

    // read larger than two sectors, size is divisible by the sector size
    if (fposHi == 0 && fpos < sectorSize) {
        if (pastFirstSector && pastSecondSector) {
            u32 byteSizeMiddle = usize - byteSizeFirst;
            if (byteSizeMiddle % sectorSize == 0) {
                params->lbn = file->lbn;
                params->lbnSize = byteSizeMiddle / sectorSize + 1;
                params->size = usize;
                params->byteSizeMiddle = byteSizeMiddle;
                params->byteSizeFirst = byteSizeFirst;
                params->byteSizeLast = 0;
                return SCE_ERROR_OK;
            }
        }
    }

    // read larger than two sectors, size is not divisible by the sector size
    if (fposHi == 0 && fpos < sectorSize) {
        if (pastFirstSector && pastSecondSector) {
            u32 byteSizeLast = (usize - byteSizeFirst) % sectorSize;
            if (byteSizeLast != 0) {
                u32 byteSizeMiddle = usize - (byteSizeLast + byteSizeFirst);
                params->lbn = file->lbn;
                params->lbnSize = byteSizeMiddle / sectorSize + 2;
                params->size = usize;
                params->byteSizeMiddle = byteSizeMiddle;
                params->byteSizeFirst = byteSizeFirst;
                params->byteSizeLast = byteSizeLast;
                return SCE_ERROR_OK;
            }
        }
    }

    return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
}

// 00005b14
s32 isofsReadParams(IsofsUnit *unit, IsofsFile *file, void *outdata, s32 size, LbnParams *params) {
    if (unit == NULL || file == NULL || params == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (size == 0) {
        return SCE_ERROR_OK;
    }

    const u32 sectorSize = unit->sectorSize;
    const s64 sectorSizeS64 = (s32) sectorSize;
    const u32 outdataEnd = (u32) outdata + size;
    const u32 fposSectors = __udivdi3(file->fpos, sectorSizeS64);
    const u32 fposRemBytes = __umoddi3(file->fpos, sectorSizeS64);

    if (fposRemBytes != 0) {
        const u32 byteSizeFirst = sectorSize - fposRemBytes;
        const u32 byteSizeLast = size - byteSizeFirst;
        const u32 remainingBytes = byteSizeLast % sectorSize;
        if (sectorSize < fposRemBytes + size) {
            if (remainingBytes != 0) {
                if (byteSizeLast <= sectorSize && byteSizeFirst != 0) {
                    params->lbn = file->lbn + fposSectors;
                    params->lbnSize = 2;
                    params->size = size;
                    params->byteSizeMiddle = 0;
                    params->byteSizeFirst = byteSizeFirst;
                    params->byteSizeLast = byteSizeLast;
                    return SCE_ERROR_OK;
                }

                params->lbn = file->lbn + fposSectors;
                params->lbnSize = (byteSizeLast - remainingBytes) / sectorSize + 2;
                params->size = size;
                params->byteSizeMiddle = byteSizeLast - remainingBytes;
                params->byteSizeFirst = byteSizeFirst;
                params->byteSizeLast = remainingBytes;
                return SCE_ERROR_OK;
            }

            params->lbn = file->lbn + fposSectors;
            params->lbnSize = byteSizeLast / sectorSize + 1;
            params->size = size;
            params->byteSizeMiddle = byteSizeLast;
            params->byteSizeFirst = byteSizeFirst;
        } else {
            params->lbn = file->lbn + fposSectors;
            params->lbnSize = 1;
            params->size = size;
            params->byteSizeMiddle = 0;
            params->byteSizeFirst = fposRemBytes;
        }
        params->byteSizeLast = 0;
        return SCE_ERROR_OK;
    }

    if (size <= (s32) sectorSize) {
        params->lbn = file->lbn + fposSectors;
        params->lbnSize = 1;
        params->size = size;
        params->byteSizeMiddle = 0;
        params->byteSizeFirst = 0;
        params->byteSizeLast = size;
        return SCE_ERROR_OK;
    }

    if (size % (s32) sectorSize == 0 && ((u32) outdata & 0x3f) != 0 && (outdataEnd & 0xffffffc0) != outdataEnd) {
        params->lbn = file->lbn + fposSectors;
        params->lbnSize = size / (s32) unit->sectorSize;
        params->size = size;
        params->byteSizeMiddle = size + unit->sectorSize * -2;
        params->byteSizeFirst = unit->sectorSize;
        params->byteSizeLast = unit->sectorSize;
        return SCE_ERROR_OK;
    }

    if ((s32) sectorSize < size) {
        if (size % (s32) sectorSize == 0 && ((u32) outdata & 0x3f) != 0) {
            params->lbn = file->lbn + fposSectors;
            params->lbnSize = size / (s32) unit->sectorSize;
            params->size = size;
            params->byteSizeMiddle = size - unit->sectorSize;
            params->byteSizeFirst = unit->sectorSize;
            params->byteSizeLast = 0;
            return SCE_ERROR_OK;
        }
    }

    if ((s32) sectorSize < size) {
        if (size % (s32) sectorSize == 0 && (outdataEnd & 0xffffffc0) != outdataEnd) {
            params->lbn = file->lbn + fposSectors;
            params->lbnSize = size / (s32) unit->sectorSize;
            params->size = size;
            params->byteSizeMiddle = size - unit->sectorSize;
            params->byteSizeFirst = 0;
            params->byteSizeLast = unit->sectorSize;
            return SCE_ERROR_OK;
        }
    }

    if ((s32) sectorSize < size) {
        if (size % (s32) sectorSize == 0) {
            params->lbn = file->lbn + fposSectors;
            params->lbnSize = size / (s32) unit->sectorSize;
            params->size = size;
            params->byteSizeMiddle = size;
            params->byteSizeFirst = 0;
            params->byteSizeLast = 0;
            return SCE_ERROR_OK;
        }
    }

    if ((s32) sectorSize < size) {
        u32 byteSizeLast = size % (s32) sectorSize;
        if (byteSizeLast != 0 && ((u32) outdata & 0x3f) != 0) {
            params->lbn = file->lbn + fposSectors;
            params->lbnSize = size / (s32) sectorSize + 1;
            params->size = size;
            params->byteSizeMiddle = size - unit->sectorSize - byteSizeLast;
            params->byteSizeFirst = unit->sectorSize;
            params->byteSizeLast = byteSizeLast;
            return SCE_ERROR_OK;
        }
    }

    if (size <= (s32) sectorSize) {
        return SCE_ERROR_OK;
    }
    u32 byteSizeLast = size % (s32) sectorSize;
    if (byteSizeLast == 0) {
        return SCE_ERROR_OK;
    }
    params->lbn = file->lbn + fposSectors;
    params->lbnSize = size / (s32) sectorSize + 1;
    params->size = size;
    params->byteSizeMiddle = size - byteSizeLast;
    params->byteSizeFirst = 0;
    params->byteSizeLast = byteSizeLast;
    return SCE_ERROR_OK;
}

// 00005fcc
s32 isofsUmdDevctlExt(IsofsUnit *unit, s32 mode, void *indata, SceSize inlen, __attribute__((unused)) s32 outBuf,
                      __attribute__((unused)) s32 outLen) {
    if (unit == NULL || indata == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    if (mode != 1) {
        return SCE_ERROR_OK;
    }
    if (inlen <= 0x17) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return sceIoDevctl(unit->blockDev, DEVCTL_UMD_CTRL_EXT, indata, 0x18, NULL, 0);
}

// 00006040
