/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include "psheet_int.h"

/* 0x14a8: open_and_mac_check */
int open_and_mac_check(char *file)
{
    if (strncmp(file, "ms0:/", 5) != 0) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    SceUID pkgFd = sceIoOpen(file, IOASSIGN_RDONLY, 0);
    int result = pkgFd;
    if (pkgFd >= 0) {
        result = sceIoLseek32(pkgFd, 0, PSP_SEEK_END);
        if (result >= 0x38) {
            result = verify_file_mac(pkgFd, 0, result - 0x10, g_pkgKey, NULL);
            if (result >= 0) {
                result = sceIoLseek32(pkgFd, 0, PSP_SEEK_SET);
                if (result >= 0) {
                    return pkgFd;
                }
            }
        }
        sceIoClose(pkgFd);
    }
    if (result >= 0 || result == (int)SCE_PSHEET_ERROR_MAC_MISMATCH) {
        result = SCE_PSHEET_ERROR_INVALID_FORMAT;
    }
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "open_and_mac_check", result);
    return result;
}

/* 0x15d8: get_core_header */
int get_core_header(SceUID pkgFd, void *vkey)
{
    int result;
    do {
        result = sceIoLseek32(pkgFd, 0, PSP_SEEK_SET);
        if (result < 0) {
            break;
        }
        result = sceIoRead(pkgFd, g_work.ioBuf, 0x28);
        if (result < 0x28) {
            break;
        }
        u32 *hdrWords = (u32 *)g_work.ioBuf;
        u32 coreOff = hdrWords[8];
        u32 endOff = hdrWords[9];
        result = SCE_PSHEET_ERROR_INVALID_FORMAT;
        if (endOff < coreOff + 0x60) {
            break;
        }
        g_work.coreSize = endOff - coreOff;
        g_work.coreOff = coreOff;
        if (verify_file_mac(pkgFd, coreOff, 0x50, vkey, NULL) < 0) {
            result = SCE_PSHEET_ERROR_CORE_MAC;
            break;
        }
        result = sceIoLseek32(pkgFd, coreOff, PSP_SEEK_SET);
        if (result < (int)coreOff) {
            break;
        }
        result = sceIoRead(pkgFd, g_work.coreHeaderKey, 0x60);
        if (result < 0x60) {
            break;
        }
        result = drmbb_decrypt(g_work.paramKey, 0x40, 0, g_pkgKey, g_work.coreHeaderKey);
        if (result < 0) {
            break;
        }
        result = SCE_PSHEET_ERROR_UNSUPPORTED;
        if (g_work.version != 1 || g_work.pkgType != 1) {
            break;
        }
        if (g_work.targetArch == 0) {
            return 0;
        }
        result = sceKernelIsToolMode();
        if (result < 0) {
            break;
        }
        u32 mask = (result == 0) ? 1 : 2;
        u32 arch = g_work.targetArch;
        if ((arch & mask) == 0) {
            Kprintf("target arch: 0x%08x\n", arch);
            result = SCE_PSHEET_ERROR_UNSUPPORTED;
            break;
        }
        if (arch == 2) {
            Kprintf("Package for PSP Development TOOL\n");
        }
        return 0;
    } while (0);

    Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "get_core_header", result);
    return (result < 0) ? result : (int)SCE_PSHEET_ERROR_INVALID_FORMAT;
}

/* 0x17c0: get_archive_header */
int get_archive_header(SceUID pkgFd, void *vkey, int idx, char *outName, u32 maxLen)
{
    int res;
    do {
        u32 hdrOff = g_work.coreOff + g_work.dataOff + ((u32)idx << 4);
        res = sceIoLseek32(pkgFd, hdrOff, PSP_SEEK_SET);
        if (res < (int)hdrOff) {
            break;
        }
        res = sceIoRead(pkgFd, &g_work.arcHdr, sizeof(g_work.arcHdr));
        if (res < 0x10) {
            break;
        }
        res = drmbb_decrypt(&g_work.arcHdr, sizeof(g_work.arcHdr), idx, vkey, g_work.dataKey);
        if (res < 0) {
            break;
        }
        u32 nameOff = g_work.coreOff + g_work.dataOff + g_work.arcHdr.nameOff;
        sceIoLseek32(pkgFd, nameOff, PSP_SEEK_SET);
        u32 nameLen = (u32)g_work.arcHdr.nameLen + 1;
        if (nameLen < maxLen) {
            maxLen = nameLen;
        }
        u32 readLen = (maxLen + 0xF) & ~0xFU;
        if (readLen > 0x400) {
            maxLen = 0x400;
        }
        res = sceIoRead(pkgFd, g_work.ioBuf, readLen);
        if (res < (int)readLen) {
            break;
        }
        res = drmbb_decrypt(g_work.ioBuf, readLen, g_work.arcHdr.nameOff >> 4, vkey, g_work.dataKey);
        if (res < 0) {
            break;
        }
        strncpy(outName, (char *)g_work.ioBuf, maxLen);
        outName[maxLen - 1] = '\0';
        return 0;
    } while (0);

    Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "get_archive_header", res);
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_INVALID_FORMAT;
}

/* 0x1998: get_params */
int get_params(SceUID pkgFd, void *vkey, u32 *params, u32 count)
{
    u32 found = 0;
    sceIoLseek32(pkgFd, g_work.coreOff + g_work.paramOff, PSP_SEEK_SET);
    u32 totalParams = g_work.paramCount;
    for (u32 i = 0; i < totalParams; i++) {
        u32 slot = i & 0x7F;
        if (slot == 0) {
            u32 bytes = (totalParams - i) << 3;
            u32 readLen = (bytes <= 0x400) ? ((bytes + 0xF) & ~0xFU) : 0x400;
            int res = sceIoRead(pkgFd, g_work.ioBuf, readLen);
            if (res < (int)readLen) {
                Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "get_params", res);
                return (res < 0) ? res : (int)SCE_PSHEET_ERROR_INVALID_FORMAT;
            }
            drmbb_decrypt(g_work.ioBuf, readLen, i >> 1, vkey, g_work.paramKey);
        }
        if (count > 0) {
            u32 *entry = (u32 *)(g_work.ioBuf + (slot << 3));
            for (u32 j = 0; j < count; j++) {
                if (params[j * 2] == entry[0]) {
                    params[j * 2 + 1] = entry[1];
                    found++;
                    break;
                }
            }
        }
        if (found == count) {
            break;
        }
    }
    return found;
}

/* 0x1b3c: get_string */
int get_string(SceUID pkgFd, void *vkey, u32 desc, char *outStr, u32 maxLen)
{
    if (desc == 0) {
        outStr[0] = '\0';
        return 0;
    }
    u32 lowOff = desc & 0xFFFF;
    u32 inBlkOff = desc & 0xF;
    u32 alignedOff = lowOff - inBlkOff;
    u32 strLen = desc >> 16;
    u32 seekPos = g_work.coreOff + g_work.paramOff + alignedOff;
    int res = sceIoLseek32(pkgFd, seekPos, PSP_SEEK_SET);
    if (res >= (int)seekPos) {
        u32 totalLen = inBlkOff + strLen;
        char *dst = outStr;
        char *dstEnd = outStr + maxLen;
        int failed = 0;
        for (u32 pos = 0; pos < totalLen; pos += 0x400) {
            u32 rem = totalLen - pos;
            u32 readLen = (rem <= 0x400) ? ((rem + 0xF) & ~0xFU) : 0x400;
            if (rem > 0x400) {
                rem = 0x400;
            }
            res = sceIoRead(pkgFd, g_work.ioBuf, readLen);
            if (res < (int)readLen) {
                failed = 1;
                break;
            }
            drmbb_decrypt(g_work.ioBuf, readLen, alignedOff >> 4, vkey, g_work.paramKey);
            alignedOff += readLen;
            for (u32 k = inBlkOff; k < rem; k++) {
                if (dst == dstEnd) {
                    break;
                }
                *dst++ = (char)g_work.ioBuf[k];
            }
            inBlkOff = 0;
        }
        if (!failed) {
            return 0;
        }
    }

    Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "get_string", res);
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_INVALID_FORMAT;
}

/* 0x1cec: rename_in_dir */
int rename_in_dir(char *path, const char *newName)
{
    char newPath[0x100];
    if (strncmp(path, "ms0:/", 5) != 0 || path[0] == '\0') {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    int lastSlash = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        if ((u32)i >= 0xFF) {
            return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
        }
        if (path[i] == '/') {
            lastSlash = i;
        }
    }
    if (lastSlash == 0) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    int dirLen = lastSlash + 1;
    strncpy(newPath, path, dirLen);
    strncpy(newPath + dirLen, newName, 0xFF - dirLen);
    newPath[0xFF] = '\0';
    sceIoRemove(newPath);
    int res = sceIoRename(path, newPath);
    if (res < 0) {
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "rename_in_dir", res);
        return res;
    }
    strcpy(path, newPath);
    return dirLen;
}

/* 0x1e14: recursive_mkdir */
int recursive_mkdir(char *path, u32 startOff)
{
    for (u32 i = startOff; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            path[i] = '\0';
            int res = sceIoMkdir(path, 0x1FF);
            if (res == (int)0x80010011) {
                res = 0;
            }
            path[i] = '/';
            if (res < 0) {
                Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "recursive_mkdir", res);
                return res;
            }
        }
    }
    return 0;
}

/* 0x1ef0: init_pgd_header */
int init_pgd_header(void *pgdHdr)
{
    static const u8 pgdMagic[4] = { 0x00, 'P', 'G', 'D' };
    ScePsheetPgdHeader *hdr = (ScePsheetPgdHeader *)pgdHdr;
    sceKernelMemset(hdr, 0, sizeof(*hdr));
    sceKernelMemcpy(hdr->magic, pgdMagic, 4);
    hdr->version = 1;
    hdr->blockSize = 0x400;
    hdr->headerSize = 0x90;
    return 0;
}
