/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include "psheet_int.h"

/* 0x072c: get_package_info */
int get_package_info(SceUID pkgFd, void *vkey, void *pkgInfo)
{
    u32 paramBuf[4];
    u32 *outWords = (u32 *)pkgInfo;
    sceKernelMemset(&g_work, 0, sizeof(g_work));
    sceKernelMemset(outWords, 0, 0x10);
    int res = get_core_header(pkgFd, vkey);
    if (res >= 0) {
        sceKernelMemset(paramBuf, 0, 0x10);
        paramBuf[0] = 0x01010100;
        paramBuf[2] = 0x01050200;
        get_params(pkgFd, vkey, paramBuf, 2);
        outWords[0] = g_work.pkgType;
        outWords[1] = paramBuf[1];
        get_string(pkgFd, vkey, paramBuf[3], (char *)(outWords + 2), 7);
        return 0;
    }
    if (g_work.coreSize == 0) {
        res = SCE_PSHEET_ERROR_NO_CORE_HEADER;
    }
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_package_info", res);
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_INVALID_FORMAT;
}

/* 0x0864: get_file_info */
int get_file_info(SceUID pkgFd, void *vkey, int fileIdx, void *fileInfo)
{
    u32 paramBuf[2];
    ScePsheetFileInfo *info = (ScePsheetFileInfo *)fileInfo;
    sceKernelMemset(&g_work, 0, sizeof(g_work));
    sceKernelMemset(info, 0, sizeof(*info));
    int res = get_core_header(pkgFd, vkey);
    if (res >= 0) {
        sceKernelMemset(paramBuf, 0, 8);
        paramBuf[0] = 0x01010100;
        get_params(pkgFd, vkey, paramBuf, 1);
        res = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
        if ((u32)fileIdx < paramBuf[1]) {
            res = get_archive_header(pkgFd, vkey, fileIdx, info->name, 0xFF);
            if (res >= 0) {
                info->flags = g_work.arcHdr.flags;
                int cmp = strcmp(info->name, "EBOOT.PBP");
                u32 rawSize = g_work.arcHdr.dataSize;
                if (cmp != 0) {
                    u32 alignedSize = (rawSize + 15) & ~0xFU;
                    u32 blockAligned = (alignedSize + 1023) & ~0x3FFU;
                    u32 macTableSize = blockAligned >> 6;
                    info->size = alignedSize + macTableSize + 0x90;
                    return 0;
                }
                info->size = rawSize;
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_file_info", res);
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_INVALID_FORMAT;
}

/* 0x09ec: install */
int install_package(SceUID pkgFd, char *path, void *vkey, void *opt __attribute__((unused)))
{
    u8 macKey[0x30];
    u32 params[2];
    ScePsheetPgdHeader pgdHdr;
    u32 pathLen = strlen(path);
    u32 remLen = 0xFF - pathLen;
    u32 workSize = g_state.workBufSize;
    char *filePart = path + pathLen;
    SceUID outFd = 0;
    int res = 0;
    int failed = 0;

    sceKernelMemset(&g_work, 0, sizeof(g_work));
    getSemaIdStatusUsingTimeout(0, 1);
    res = get_core_header(pkgFd, vkey);
    if (res < 0) {
        failed = 1;
    } else {
        sceKernelMemset(params, 0, 8);
        params[0] = 0x01010100;
        get_params(pkgFd, vkey, params, 1);
        u32 fileCount = params[1];
        for (u32 idx = 0; idx < fileCount && !failed; idx++) {
            sceKernelMemset(filePart, 0, remLen);
            res = get_archive_header(pkgFd, vkey, idx, filePart, remLen);
            if (res < 0) {
                failed = 1;
                break;
            }
            if (idx == 0) {
                u32 totalWork = (g_work.dataSize - g_work.arcHdr.dataOff) + 0x10;
                getSemaIdStatusUsingTimeout(0, totalWork);
            }

            u32 fileOff = g_work.coreOff + g_work.dataOff + g_work.arcHdr.dataOff;
            res = sceIoLseek32(pkgFd, fileOff, PSP_SEEK_SET);
            if (res < 0) {
                failed = 1;
                break;
            }

            u8 *keyPtr = (u8 *)g_pkgKey;
            int fileType = 1;
            if (strcmp(filePart, "EBOOT.PBP") == 0) {
                Kprintf("%s is system file.\n", filePart);
                __builtin_memcpy(filePart, "_TEMP.PBP", 10);
                fileType = 0;
            } else {
                u32 fnLen = strlen(filePart);
                if (strcmp(filePart + fnLen - 4, ".PRX") == 0) {
                    Kprintf("%s is program.\n", filePart);
                    fileType = 2;
                    keyPtr = (u8 *)g_pkgKey + 0x10;
                } else {
                    Kprintf("%s is normal file.\n", filePart);
                }
            }

            res = recursive_mkdir(path, pathLen);
            if (res < 0) {
                failed = 1;
                break;
            }
            outFd = sceIoOpen(path, PSP_O_RDWR | PSP_O_CREAT | PSP_O_TRUNC, 0x1FF);
            if (outFd < 0) {
                res = outFd;
                failed = 1;
                break;
            }

            if (fileType != 0) {
                sceKernelMemset(&pgdHdr, 0, sizeof(pgdHdr));
                sceIoWrite(outFd, &pgdHdr, sizeof(pgdHdr));
                init_pgd_header(&pgdHdr);
                pgdHdr.dataSize = g_work.arcHdr.dataSize;
                pgdHdr.blockSize = 0x400;
                pgdHdr.dataOffset = 0;
                res = create_mac(path, strlen(path), vkey, pgdHdr.pathMac);
                res = sceDrmBBCipherInit(g_work.encryptCtx, 2, 1, pgdHdr.dataKey, vkey, 0);
                if (res < 0) {
                    failed = 1;
                    break;
                }
            }

            u32 dataLen = g_work.arcHdr.dataSize;
            u32 done = 0;
            while (done < dataLen && !failed) {
                u32 rem = dataLen - done;
                u32 alignedRem = (rem + 0xF) & ~0xFU;
                u32 readLen = alignedRem;
                u32 writeLen = (fileType != 0) ? alignedRem : rem;
                if (workSize < alignedRem) {
                    readLen = workSize;
                    writeLen = workSize;
                }

                res = sceIoRead(pkgFd, g_state.workBuf, readLen);
                if (res < (int)readLen) {
                    failed = 1;
                    break;
                }
                for (u32 blk = 0; blk < readLen; blk += 0x400) {
                    u32 seed = (g_work.arcHdr.dataOff + done + blk) >> 4;
                    u8 *workPtr = (u8 *)g_state.workBuf;
                    sceKernelMemcpy(g_work.ioBuf, workPtr + blk, 0x400);
                    res = drmbb_decrypt(g_work.ioBuf, 0x400, seed, vkey, g_work.dataKey);
                    if (res < 0) {
                        failed = 1;
                        break;
                    }
                    if (fileType == 2 && (done + blk) == 0 && readLen >= 0x400) {
                        res = sceUtilsPrepareGetLoadModuleRLength(g_work.ioBuf, 0x400, vkey);
                        if (res < 0) {
                            if (sceKernelIsToolMode() != 0) {
                                Kprintf("Smode\n");
                                res = sceUtilsPrepareGetLoadModuleSLength(g_work.ioBuf, 0x400, vkey);
                            }
                            if (res < 0) {
                                Kprintf("low level error = %d\n", res);
                                res = SCE_PSHEET_ERROR_CRYPTO_FAILED;
                                failed = 1;
                                break;
                            }
                        }
                    }
                    if (fileType != 0) {
                        res = sceDrmBBCipherUpdate(g_work.encryptCtx, g_work.ioBuf, 0x400);
                        if (res < 0) {
                            failed = 1;
                            break;
                        }
                    }
                    workPtr = (u8 *)g_state.workBuf;
                    sceKernelMemcpy(workPtr + blk, g_work.ioBuf, 0x400);
                }
                if (failed) {
                    break;
                }

                res = sceIoWrite(outFd, g_state.workBuf, writeLen);
                if (res < (int)writeLen) {
                    failed = 1;
                    break;
                }
                getSemaIdStatusIncrement(readLen);
                res = get_state();
                if (res == 1) {
                    res = SCE_PSHEET_ERROR_ABORTED;
                }
                if (res < 0) {
                    failed = 1;
                    break;
                }
                done += workSize;
            }
            if (failed) {
                break;
            }

            if (fileType != 0) {
                res = sceDrmBBCipherFinal(g_work.encryptCtx);
                if (res < 0) {
                    failed = 1;
                    break;
                }
                res = sceDrmBBMacInit(macKey, 2);
                if (res < 0) {
                    failed = 1;
                    break;
                }
                u32 chunkLim = (workSize < 0x10000) ? workSize : 0x10000;
                done = 0;
                while (done < dataLen && !failed) {
                    u32 rem = ((dataLen - done) + 0xF) & ~0xFU;
                    if (rem > chunkLim) {
                        rem = chunkLim;
                    }
                    u32 macBytes = ((rem + 0x3FF) / 0x400) << 4;
                    res = sceIoLseek32(outFd, done + 0x90, PSP_SEEK_SET);
                    if (res < 0) {
                        failed = 1;
                        break;
                    }
                    res = sceIoRead(outFd, g_state.workBuf, rem);
                    if (res < (int)rem) {
                        failed = 1;
                        break;
                    }
                    for (u32 blk = 0; blk < rem; blk += 0x400) {
                        u32 bsz = (rem - blk > 0x400) ? 0x400 : (rem - blk);
                        u8 *workPtr = (u8 *)g_state.workBuf;
                        res = create_mac(workPtr + blk, bsz, vkey, g_work.ioBuf + ((blk / 0x400) << 4));
                        if (res < 0) {
                            failed = 1;
                            break;
                        }
                    }
                    if (failed) {
                        break;
                    }
                    res = sceDrmBBMacUpdate(macKey, g_work.ioBuf, macBytes);
                    if (res < 0) {
                        return res;
                    }
                    res = sceIoLseek32(outFd, 0, PSP_SEEK_END);
                    if (res < 0) {
                        failed = 1;
                        break;
                    }
                    res = sceIoWrite(outFd, g_work.ioBuf, macBytes);
                    if (res < (int)macBytes) {
                        failed = 1;
                        break;
                    }
                    done += chunkLim;
                }
                if (failed) {
                    break;
                }
                res = sceDrmBBMacFinal(macKey, pgdHdr.tableMac, vkey);
                if (res < 0) {
                    return res;
                }
                res = data_encrypt(pgdHdr.dataKey, 0x30, vkey, pgdHdr.headerKey);
                if (res < 0) {
                    failed = 1;
                    break;
                }
                res = create_mac(&pgdHdr, 0x70, vkey, pgdHdr.headerMacVkey);
                if (res < 0) {
                    failed = 1;
                    break;
                }
                res = create_mac(&pgdHdr, 0x80, keyPtr, pgdHdr.headerMacPkgKey);
                if (res < 0) {
                    failed = 1;
                    break;
                }
                res = sceIoLseek32(outFd, 0, PSP_SEEK_SET);
                if (res < 0) {
                    failed = 1;
                    break;
                }
                res = sceIoWrite(outFd, &pgdHdr, sizeof(pgdHdr));
                if (res < 0x90) {
                    failed = 1;
                    break;
                }
            }

            res = sceIoClose(outFd);
            outFd = 0;
            if (res < 0) {
                failed = 1;
                break;
            }
        }
    }

    if (!failed) {
        sceKernelMemset(macKey, 0, 0x28);
        getSemaIdStatusIncrement(0x10);
        return 0;
    }

    sceKernelMemset(macKey, 0, 0x28);
    if (outFd > 0) {
        sceIoClose(outFd);
        sceIoRemove(path);
    }
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "install", res);
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_INVALID_FORMAT;
}

/* 0x122c: getSemaIdStatus */
int getSemaIdStatus(SceUInt *progressOut)
{
    int result = sceKernelWaitSema(g_state.semaId, 1, 0);
    if (result < 0) {
        return result;
    }
    progressOut[0] = g_state.progressBytes;
    progressOut[1] = g_state.totalBytes;
    result = sceKernelSignalSema(g_state.semaId, 1);
    if (result < 0) {
        return result;
    }
    return 0;
}

/* 0x12a0: getSemaIdStatusIncrement */
int getSemaIdStatusIncrement(SceUInt delta)
{
    int result = sceKernelWaitSema(g_state.semaId, 1, 0);
    if (result < 0) {
        return result;
    }
    g_state.progressBytes += delta;
    result = sceKernelSignalSema(g_state.semaId, 1);
    if (result < 0) {
        return result;
    }
    return 0;
}

/* 0x1310: getSemaIdStatusUsingTimeout */
int getSemaIdStatusUsingTimeout(int progress, int total)
{
    int result = sceKernelWaitSema(g_state.semaId, 1, 0);
    if (result < 0) {
        return result;
    }
    g_state.totalBytes = total;
    g_state.progressBytes = progress;
    result = sceKernelSignalSema(g_state.semaId, 1);
    if (result < 0) {
        return result;
    }
    return 0;
}

/* 0x1388: get_state */
int get_state(void)
{
    int lockRes = sceKernelWaitSema(g_state.semaId, 1, 0);
    if (lockRes < 0) {
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_state", lockRes);
        return lockRes;
    }
    int result = g_state.abortState;
    lockRes = sceKernelSignalSema(g_state.semaId, 1);
    if (lockRes < 0) {
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_state", lockRes);
        return lockRes;
    }
    return result;
}

/* 0x1410: set_state */
int set_state(int state)
{
    int lockRes = sceKernelWaitSema(g_state.semaId, 1, 0);
    if (lockRes < 0) {
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "set_state", lockRes);
        return lockRes;
    }
    int prev = g_state.abortState;
    g_state.abortState = state;
    lockRes = sceKernelSignalSema(g_state.semaId, 1);
    if (lockRes < 0) {
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "set_state", lockRes);
        return lockRes;
    }
    return prev;
}
