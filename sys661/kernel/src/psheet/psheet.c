/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include <pspkernel.h>
#include <pspkerneltypes.h>
#include <string.h>

#define SCE_PSHEET_ERROR_SEMAID          0x80510108
#define SCE_PSHEET_ERROR_ILLEGAL_ADDRESS 0x80510109
#define SCE_PSHEET_ERROR_P               0x80510111
#define SCE_PSHEET_ERROR_P2              0x80510110
#define SCE_PSHEET_ERROR_P3              0x80510102
#define SCE_PSHEET_ERROR_P4              0x80510300
#define SCE_PSHEET_ERROR_P5              0x80510101
#define SCE_PSHEET_ERROR_P6              0x80510110
#define SCE_PSHEET_ERROR_P7              0x80510113
#define SCE_PSHEET_ERROR_P8              0x80510112
#define SCE_PSHEET_ERROR_ABORTED         0x80510120

#ifndef IOASSIGN_RDONLY
#define IOASSIGN_RDONLY PSP_O_RDONLY
#endif

static inline int _getK1(void) { int k1; __asm__ volatile ("move %0, $k1" : "=r"(k1)); return k1; }
static inline void _setK1(int k1) { __asm__ volatile ("move $k1, %0" : : "r"(k1)); }
#define pspSdkGetK1() _getK1()
#define pspSdkSetK1(k1) _setK1(k1)

extern void *sceKernelMemset(void *dst, int val, SceSize size);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize size);
extern int sceKernelIsToolMode(void);
extern int sceDrmBBCipherInit(void *ckey, int type, int mode, void *header_key, void *version_key, u32 seed);
extern int sceDrmBBCipherUpdate(void *ckey, void *buf, int size);
extern int sceDrmBBCipherFinal(void *ckey);
extern int sceDrmBBMacInit(void *mkey, int type);
extern int sceDrmBBMacUpdate(void *mkey, void *buf, int size);
extern int sceDrmBBMacFinal(void *mkey, void *buf, void *vkey);
extern int sceDrmBBMacFinal2(void *mkey, void *out, void *vkey);
extern int sceUtilsPrepareGetLoadModuleSLength(void *buf, int size, void *out);
extern int sceUtilsPrepareGetLoadModuleRLength(void *buf, int size, void *out);

/* .data @ 0x2a40..0x2a80 */
SceUID semaID = 0;          /* 0x2a40 */
SceUID unk1 = 0;            /* 0x2a44 */
SceUInt semaTimeout = 0;    /* 0x2a48 */
int unk3 = 0;               /* 0x2a4c */
int buf[4] = {0, 0, 0, 0};  /* 0x2a50..0x2a5c */
u32 g_pkgKey[8] = {         /* 0x2a60..0x2a7c */
    0x2d5de2ed, 0xe512f8bb, 0x32595c3c, 0x43e2e3fa,
    0xebfb7427, 0xd701a0a4, 0x339e5602, 0x8357198c,
};

/* .bss @ 0x2a80..0x2f50 (0x4d0 bytes) */
u8 g_unkP[0x4D0];

int getSemaIdStatus(SceUInt *sTimeout);
int getSemaIdStatusIncrement(SceUInt sTimeout);
int getSemaIdStatusUsingTimeout(int timeout, int a1);
int getSemaIdStatusUsingBuf(void);
int sub_01410(int a0);
int sub_014A8(char *file);
int sub_015D8(SceUID a0, int a1);
int sub_72C(int a0, int a1, void *a2);
int sub_864(int a0, int a1, int a2, void *a3);
int sub_9EC(SceUID fd, char *path, int vkey, int a3);
int sub_17C0(SceUID fd, int vkey, int idx, char *outName, u32 maxLen);
int sub_1998(SceUID fd, int vkey, u32 *params, u32 count);
int sub_1B3C(SceUID fd, int vkey, u32 desc, char *outStr, u32 maxLen);
int sub_1CEC(char *path, const char *newName);
int sub_1E14(char *path, u32 startOff);
int sub_1EF0(void *pgdHdr);
int sub_01F4C(void *a0, int a1, void *a2, void *a3);
int sub_2000(void *a0, int a1, void *a2, void *a3);
int sub_20C8(void *a0, int a1, void *a2, void *a3, void *t0);
int sub_2134(void *a0, int a1, int a2, void *a3, void *t0);
int sub_21F0(SceUID fd, int offset, int size, void *vkey, void *mac);

/* 0x0000: sceDRMInstallInit */
int sceDRMInstallInit(int address, int size)
{
    int k1 = pspSdkGetK1();
    pspSdkSetK1(k1 << 11);
    sceKernelMemset(g_unkP, 0, 0x4D0);

    if (((address | size | (address + size)) & (k1 << 11)) < 0) {
        pspSdkSetK1(k1);
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }

    getSemaIdStatusUsingTimeout(0, 1);
    sub_01410(0);
    buf[0] = address;
    buf[1] = size;

    if ((u32)address < 1 || (u32)size <= 0x0001FFFF) {
        buf[0] = 0;
        buf[1] = 0;
        pspSdkSetK1(k1);
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }

    pspSdkSetK1(k1);
    return 0;
}

/* 0x00cc: sceDRMInstallGetPkgInfo */
int sceDRMInstallGetPkgInfo(int address, int size, int a2)
{
    int ret;
    SceUID fd;
    int k1 = pspSdkGetK1();

    if (buf[0] == 0) {
        return SCE_PSHEET_ERROR_SEMAID;
    }
    if ((u32)address < 1 || (u32)size < 1 || a2 == 0) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    pspSdkSetK1(k1 << 11);
    if (((k1 << 11) & address) < 0 ||
        (((size + 16) | size) & (k1 << 11)) < 0 ||
        (((a2 + 16) | a2) & (k1 << 11)) < 0) {
        ret = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
        goto end;
    }
    sub_01410(0);
    fd = sub_014A8((char *)address);
    if (fd < 0) {
        ret = fd;
        goto end;
    }
    ret = sub_72C(fd, size, (void *)a2);
    sceIoClose(fd);

end:
    sceKernelMemset(g_unkP, 0, 0x4D0);
    pspSdkSetK1(k1);
    return ret;
}

/* 0x01ec: sceDRMInstallGetFileInfo */
int sceDRMInstallGetFileInfo(char *file, int size, int a2, int a3)
{
    int ret;
    int k1;
    SceUID fd;

    if (buf[0] == 0) {
        return SCE_PSHEET_ERROR_SEMAID;
    }
    if (file == NULL || size == 0 || a3 == 0) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    k1 = pspSdkGetK1();
    pspSdkSetK1(k1 << 11);
    if (((k1 << 11) & (int)file) < 0 ||
        (((size + 16) | size) & (k1 << 11)) < 0 ||
        (((a3 + 0x108) | a3) & (k1 << 11)) < 0) {
        ret = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
        goto end;
    }
    pspSdkSetK1(((u32)(k1 << 11)) >> 11);
    fd = sceIoOpen(file, IOASSIGN_RDONLY, 0);
    pspSdkSetK1(k1 << 11);
    if (fd < 0) {
        ret = fd;
        goto end;
    }
    sub_01410(0);
    ret = sub_864(fd, size, a2, (void *)a3);
    sceIoClose(fd);

end:
    sceKernelMemset(g_unkP, 0, 0x4D0);
    pspSdkSetK1(k1);
    return ret;
}

/* 0x0324: sceDRMInstallInstall */
int sceDRMInstallInstall(const char *path, int vkey, int a2)
{
    char localPath[0x100];
    SceUID fd = -1;
    int ret;
    int k1;
    int dirLen;

    if (buf[0] == 0) {
        return SCE_PSHEET_ERROR_SEMAID;
    }
    if (path == NULL || vkey == 0) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    k1 = pspSdkGetK1();
    pspSdkSetK1(k1 << 11);
    if (((k1 << 11) & (int)path) < 0 ||
        (((vkey + 16) | vkey) & (k1 << 11)) < 0 ||
        (((a2 + 16) | a2) & (k1 << 11)) < 0) {
        ret = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
        goto end;
    }

    sub_01410(0);
    sceKernelMemset(localPath, 0, sizeof(localPath));
    strncpy(localPath, path, 0xFE);
    ret = sub_1CEC(localPath, "_EBOOT.PBP");
    if (ret < 0) {
        goto end;
    }
    dirLen = ret;
    if (0x100 - dirLen < 13) {
        ret = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
        goto end;
    }
    fd = sub_014A8(localPath);
    if (fd < 0) {
        ret = fd;
        goto end;
    }
    localPath[dirLen] = '\0';
    ret = sub_9EC(fd, localPath, vkey, a2);
    if (ret < 0) {
        goto end;
    }
    ret = sceIoClose(fd);
    fd = -1;
    if (ret < 0) {
        goto end;
    }
    sceKernelMemcpy(localPath + dirLen, "_EBOOT.PBP", 11);
    ret = sceIoRemove(localPath);
    if (ret < 0) {
        goto end;
    }
    sceKernelMemcpy(localPath + dirLen, "_TEMP.PBP", 10);
    ret = sub_1CEC(localPath, "EBOOT.PBP");
    if (ret > 0) {
        ret = 0;
    }

end:
    sceKernelMemset(g_unkP, 0, 0x4D0);
    if (fd >= 0) {
        sceIoClose(fd);
    }
    pspSdkSetK1(k1);
    return ret;
}

/* 0x0564: module_start */
int module_start(SceSize argc __attribute__((unused)), void *argp __attribute__((unused)))
{
    SceUID s0 = sceKernelCreateSema("ScePsheet1", 0, 1, 1, 0);
    semaID = s0;
    if (s0 >= 0) {
        sceDRMInstallInit(0, 0);
        return 0;
    }
    if (unk1 > 0) {
        sceKernelDeleteSema(unk1);
    }
    sceKernelMemset(&semaID, 0, 64);
    sceKernelMemset(g_unkP, 0, 0x4D0);
    return (s0 < -1) ? s0 : -1;
}

/* 0x0610: module_stop / module_reboot_before */
int module_reboot_before(SceSize args __attribute__((unused)), void *argp __attribute__((unused)))
{
    if (semaID > 0) {
        sceKernelDeleteSema(semaID);
    }
    if (unk1 > 0) {
        sceKernelDeleteSema(unk1);
    }
    sceKernelMemset(&semaID, 0, 64);
    sceKernelMemset(g_unkP, 0, 0x4D0);
    return 0;
}

int module_stop(SceSize args, void *argp) __attribute__((alias("module_reboot_before")));

/* 0x0684: sceDRMInstallEnd */
int sceDRMInstallEnd(void)
{
    sceDRMInstallInit(0, 0);
    return 0;
}

/* 0x06a8: scePsheet_3BA93CFA */
int scePsheet_3BA93CFA(int a0)
{
    int k1 = pspSdkGetK1();
    if ((((a0 + 8) | a0) & (k1 << 11)) >= 0) {
        pspSdkSetK1(k1 << 11);
        int result = getSemaIdStatus((SceUInt *)a0);
        pspSdkSetK1(k1);
        return result;
    }
    pspSdkSetK1(k1);
    return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
}

int scePsheet_driver_3BA93CFA(int a0) __attribute__((alias("scePsheet_3BA93CFA")));

/* 0x06fc: sceDRMInstallAbort */
void sceDRMInstallAbort(void)
{
    int k1 = pspSdkGetK1();
    pspSdkSetK1(k1 << 11);
    sub_01410(1);
    pspSdkSetK1(k1);
}

/* 0x072c: sub_072c (get_package_info) */
int sub_72C(int a0, int a1, void *a2)
{
    u32 lbuf[4];
    u32 *s1 = (u32 *)a2;
    u8 *s4 = g_unkP;
    sceKernelMemset(s4, 0, 0x4D0);
    sceKernelMemset(s1, 0, 0x10);
    int res = sub_015D8(a0, a1);
    if (res >= 0) {
        sceKernelMemset(lbuf, 0, 0x10);
        lbuf[0] = 0x01010100;
        lbuf[2] = 0x01050200;
        sub_1998(a0, a1, lbuf, 2);
        s1[0] = *(u32 *)(s4 + 0x448);
        s1[1] = lbuf[1];
        sub_1B3C(a0, a1, lbuf[3], (char *)(s1 + 2), 7);
        return 0;
    }
    if (*(u32 *)(s4 + 0x4CC) == 0) {
        res = SCE_PSHEET_ERROR_P;
    }
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_package_info", res);
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_P2;
}

/* 0x0864: sub_0864 (get_file_info) */
int sub_864(int a0, int a1, int a2, void *a3)
{
    u32 lbuf[2];
    char *s3 = (char *)a3;
    char *s5;
    u8 *s6 = g_unkP;
    sceKernelMemset(s6, 0, 0x4D0);
    sceKernelMemset(s3, 0, 0x108);
    int res = sub_015D8(a0, a1);
    if (res >= 0) {
        s5 = s3 + 8;
        sceKernelMemset(lbuf, 0, 8);
        lbuf[0] = 0x01010100;
        sub_1998(a0, a1, lbuf, 1);
        res = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
        if ((u32)a2 >= lbuf[1]) {
            goto Loc;
        }
        res = sub_17C0(a0, a1, a2, s5, 0xFF);
        if (res < 0) {
            goto Loc;
        }
        *(u32 *)s3 = *(u32 *)(s6 + 0x40C);
        int cmp = strcmp(s5, "EBOOT.PBP");
        u32 t5 = *(u32 *)(s6 + 0x408);
        if (cmp != 0) {
            u32 t2 = (t5 + 15) & ~0xFU;
            u32 t4 = (t2 + 1023) & ~0x3FFU;
            u32 t3 = t4 >> 6;
            *(u32 *)(s3 + 4) = t2 + t3 + 0x90;
            return 0;
        }
        *(u32 *)(s3 + 4) = t5;
        return 0;
    }
Loc:
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_file_info", res);
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_P2;
}

/* 0x09ec: sub_09ec (install) */
int sub_9EC(SceUID fd, char *path, int vkey, int a3 __attribute__((unused)))
{
    u8 macKey[0x30];
    u32 params[2];
    u8 pgdHdr[0x90];
    u32 pathLen = strlen(path);
    u32 remLen = 0xFF - pathLen;
    u32 workSize = (u32)buf[1];
    char *filePart = path + pathLen;
    SceUID outFd = 0;
    int res;

    sceKernelMemset(g_unkP, 0, 0x4D0);
    getSemaIdStatusUsingTimeout(0, 1);
    res = sub_015D8(fd, vkey);
    if (res < 0) {
        goto err;
    }

    sceKernelMemset(params, 0, 8);
    params[0] = 0x01010100;
    sub_1998(fd, vkey, params, 1);
    u32 fileCount = params[1];
    for (u32 idx = 0; idx < fileCount; idx++) {
        sceKernelMemset(filePart, 0, remLen);
        res = sub_17C0(fd, vkey, idx, filePart, remLen);
        if (res < 0) {
            goto err;
        }
        if (idx == 0) {
            u32 totalWork = (*(u32 *)(g_unkP + 0x45C) - *(u32 *)(g_unkP + 0x404)) + 0x10;
            getSemaIdStatusUsingTimeout(0, totalWork);
        }

        u32 fileOff = *(u32 *)(g_unkP + 0x4C8) + *(u32 *)(g_unkP + 0x458) + *(u32 *)(g_unkP + 0x404);
        res = sceIoLseek32(fd, fileOff, PSP_SEEK_SET);
        if (res < 0) {
            goto err;
        }

        u8 *keyPtr = (u8 *)g_pkgKey;
        int fileType = 1;
        if (strcmp(filePart, "EBOOT.PBP") == 0) {
            Kprintf("%s is system file.\n", filePart);
            sceKernelMemcpy(filePart, "_TEMP.PBP", 10);
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

        res = sub_1E14(path, pathLen);
        if (res < 0) {
            goto err;
        }
        outFd = sceIoOpen(path, PSP_O_RDWR | PSP_O_CREAT | PSP_O_TRUNC, 0x1FF);
        if (outFd < 0) {
            res = outFd;
            goto err;
        }

        if (fileType != 0) {
            sceKernelMemset(pgdHdr, 0, 0x90);
            sceIoWrite(outFd, pgdHdr, 0x90);
            sub_1EF0(pgdHdr);
            *(u32 *)(pgdHdr + 0x44) = *(u32 *)(g_unkP + 0x408);
            *(u32 *)(pgdHdr + 0x48) = 0x400;
            *(u32 *)(pgdHdr + 0x40) = 0;
            res = sub_2000(path, strlen(path), (void *)vkey, pgdHdr + 0x20);
            res = sceDrmBBCipherInit(g_unkP + 0x488, 2, 1, pgdHdr + 0x30, (void *)vkey, 0);
            if (res < 0) {
                goto err;
            }
        }

        u32 dataLen = *(u32 *)(g_unkP + 0x408);
        u32 done = 0;
        while (done < dataLen) {
            u32 rem = dataLen - done;
            u32 alignedRem = (rem + 0xF) & ~0xFU;
            u32 readLen = alignedRem;
            u32 writeLen = (fileType != 0) ? alignedRem : rem;
            if (workSize < alignedRem) {
                readLen = workSize;
                writeLen = workSize;
            }

            res = sceIoRead(fd, (void *)buf[0], readLen);
            if (res < (int)readLen) {
                goto err;
            }
            for (u32 blk = 0; blk < readLen; blk += 0x400) {
                u32 seed = (*(u32 *)(g_unkP + 0x404) + done + blk) >> 4;
                sceKernelMemcpy(g_unkP, (u8 *)buf[0] + blk, 0x400);
                res = sub_2134(g_unkP, 0x400, seed, (void *)vkey, g_unkP + 0x430);
                if (res < 0) {
                    goto err;
                }
                if (fileType == 2 && (done + blk) == 0 && readLen >= 0x400) {
                    res = sceUtilsPrepareGetLoadModuleRLength(g_unkP, 0x400, (void *)vkey);
                    if (res < 0) {
                        if (sceKernelIsToolMode() != 0) {
                            Kprintf("Smode\n");
                            res = sceUtilsPrepareGetLoadModuleSLength(g_unkP, 0x400, (void *)vkey);
                        }
                        if (res < 0) {
                            Kprintf("low level error = %d\n", res);
                            res = SCE_PSHEET_ERROR_P3;
                            goto err;
                        }
                    }
                }
                if (fileType != 0) {
                    res = sceDrmBBCipherUpdate(g_unkP + 0x488, g_unkP, 0x400);
                    if (res < 0) {
                        goto err;
                    }
                }
                sceKernelMemcpy((u8 *)buf[0] + blk, g_unkP, 0x400);
            }

            res = sceIoWrite(outFd, (void *)buf[0], writeLen);
            if (res < (int)writeLen) {
                goto err;
            }
            getSemaIdStatusIncrement(readLen);
            res = getSemaIdStatusUsingBuf();
            if (res == 1) {
                res = SCE_PSHEET_ERROR_ABORTED;
            }
            if (res < 0) {
                goto err;
            }
            done += workSize;
        }

        if (fileType != 0) {
            res = sceDrmBBCipherFinal(g_unkP + 0x488);
            if (res < 0) {
                goto err;
            }
            res = sceDrmBBMacInit(macKey, 2);
            if (res < 0) {
                goto err;
            }
            u32 chunkLim = (workSize < 0x10000) ? workSize : 0x10000;
            done = 0;
            while (done < dataLen) {
                u32 rem = ((dataLen - done) + 0xF) & ~0xFU;
                if (rem > chunkLim) {
                    rem = chunkLim;
                }
                u32 macBytes = ((rem + 0x3FF) / 0x400) << 4;
                res = sceIoLseek32(outFd, done + 0x90, PSP_SEEK_SET);
                if (res < 0) {
                    goto err;
                }
                res = sceIoRead(outFd, (void *)buf[0], rem);
                if (res < (int)rem) {
                    goto err;
                }
                for (u32 blk = 0; blk < rem; blk += 0x400) {
                    u32 bsz = (rem - blk > 0x400) ? 0x400 : (rem - blk);
                    res = sub_2000((u8 *)buf[0] + blk, bsz, (void *)vkey, g_unkP + ((blk / 0x400) << 4));
                    if (res < 0) {
                        goto err;
                    }
                }
                res = sceDrmBBMacUpdate(macKey, g_unkP, macBytes);
                if (res < 0) {
                    sceKernelMemset(macKey, 0, 0x28);
                    return res;
                }
                res = sceIoLseek32(outFd, 0, PSP_SEEK_END);
                if (res < 0) {
                    goto err;
                }
                res = sceIoWrite(outFd, g_unkP, macBytes);
                if (res < (int)macBytes) {
                    goto err;
                }
                done += chunkLim;
            }
            res = sceDrmBBMacFinal(macKey, pgdHdr + 0x60, (void *)vkey);
            if (res < 0) {
                sceKernelMemset(macKey, 0, 0x28);
                return res;
            }
            res = sub_01F4C(pgdHdr + 0x30, 0x30, (void *)vkey, pgdHdr + 0x10);
            if (res < 0) {
                goto err;
            }
            res = sub_2000(pgdHdr, 0x70, (void *)vkey, pgdHdr + 0x70);
            if (res < 0) {
                goto err;
            }
            res = sub_2000(pgdHdr, 0x80, keyPtr, pgdHdr + 0x80);
            if (res < 0) {
                goto err;
            }
            res = sceIoLseek32(outFd, 0, PSP_SEEK_SET);
            if (res < 0) {
                goto err;
            }
            res = sceIoWrite(outFd, pgdHdr, 0x90);
            if (res < 0x90) {
                goto err;
            }
        }

        res = sceIoClose(outFd);
        outFd = 0;
        if (res < 0) {
            goto err;
        }
    }

    sceKernelMemset(macKey, 0, 0x28);
    getSemaIdStatusIncrement(0x10);
    return 0;

err:
    sceKernelMemset(macKey, 0, 0x28);
    if (outFd > 0) {
        sceIoClose(outFd);
        sceIoRemove(path);
    }
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "install", res);
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_P2;
}

/* 0x122c: getSemaIdStatus */
int getSemaIdStatus(SceUInt *sTimeout)
{
    int result = sceKernelWaitSema(semaID, 1, 0);
    if (result < 0) {
        return result;
    }
    sTimeout[0] = semaTimeout;
    sTimeout[1] = unk3;
    result = sceKernelSignalSema(semaID, 1);
    if (result < 0) {
        return result;
    }
    return 0;
}

/* 0x12a0: getSemaIdStatusIncrement */
int getSemaIdStatusIncrement(SceUInt sTimeout)
{
    int result = sceKernelWaitSema(semaID, 1, 0);
    if (result < 0) {
        return result;
    }
    semaTimeout += sTimeout;
    result = sceKernelSignalSema(semaID, 1);
    if (result < 0) {
        return result;
    }
    return 0;
}

/* 0x1310: getSemaIdStatusUsingTimeout */
int getSemaIdStatusUsingTimeout(int timeout, int a1)
{
    int result = sceKernelWaitSema(semaID, 1, 0);
    if (result < 0) {
        return result;
    }
    unk3 = a1;
    semaTimeout = timeout;
    result = sceKernelSignalSema(semaID, 1);
    if (result < 0) {
        return result;
    }
    return 0;
}

/* 0x1388: getSemaIdStatusUsingBuf */
int getSemaIdStatusUsingBuf(void)
{
    int lckdresult = sceKernelWaitSema(semaID, 1, 0);
    if (lckdresult < 0) {
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_state", lckdresult);
        return lckdresult;
    }
    int result = buf[2];
    lckdresult = sceKernelSignalSema(semaID, 1);
    if (lckdresult < 0) {
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_state", lckdresult);
        return lckdresult;
    }
    return result;
}

/* 0x1410: sub_01410 */
int sub_01410(int a0)
{
    int lckdresult = sceKernelWaitSema(semaID, 1, 0);
    if (lckdresult < 0) {
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "set_state", lckdresult);
        return lckdresult;
    }
    int result = buf[2];
    buf[2] = a0;
    lckdresult = sceKernelSignalSema(semaID, 1);
    if (lckdresult < 0) {
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "set_state", lckdresult);
        return lckdresult;
    }
    return result;
}

/* 0x14a8: sub_014A8 (open_and_mac_check) */
int sub_014A8(char *file)
{
    if (strncmp(file, "ms0:/", 5) != 0) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    SceUID fd = sceIoOpen(file, IOASSIGN_RDONLY, 0);
    int result = fd;
    if (fd >= 0) {
        result = sceIoLseek32(fd, 0, PSP_SEEK_END);
        if (result >= 0x38) {
            result = sub_21F0(fd, 0, result - 0x10, g_pkgKey, 0);
            if (result >= 0) {
                result = sceIoLseek32(fd, 0, PSP_SEEK_SET);
                if (result >= 0) {
                    return fd;
                }
            }
        }
        sceIoClose(fd);
    }
    if (result >= 0 || result == (int)SCE_PSHEET_ERROR_P5) {
        result = SCE_PSHEET_ERROR_P2;
    }
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "open_and_mac_check", result);
    return result;
}

/* 0x15d8: sub_015D8 (get_core_header) */
int sub_015D8(SceUID a0, int a1)
{
    u8 *s3 = g_unkP;
    int result = sceIoLseek32(a0, 0, PSP_SEEK_SET);
    if (result < 0) {
        goto exit;
    }
    result = sceIoRead(a0, s3, 0x28);
    if (result < 0x28) {
        goto exit;
    }
    u32 coreOff = *(u32 *)(s3 + 0x20);
    u32 endOff = *(u32 *)(s3 + 0x24);
    result = SCE_PSHEET_ERROR_P6;
    if (endOff < coreOff + 0x60) {
        goto exit;
    }
    *(u32 *)(s3 + 0x4CC) = endOff - coreOff;
    *(u32 *)(s3 + 0x4C8) = coreOff;
    if (sub_21F0(a0, coreOff, 0x50, (void *)a1, 0) < 0) {
        result = SCE_PSHEET_ERROR_P7;
        goto exit;
    }
    result = sceIoLseek32(a0, coreOff, PSP_SEEK_SET);
    if (result < (int)coreOff) {
        goto exit;
    }
    result = sceIoRead(a0, s3 + 0x410, 0x60);
    if (result < 0x60) {
        goto exit;
    }
    result = sub_2134(s3 + 0x420, 0x40, 0, g_pkgKey, s3 + 0x410);
    if (result < 0) {
        goto exit;
    }
    result = SCE_PSHEET_ERROR_P8;
    if (*(u32 *)(s3 + 0x440) != 1 || *(u32 *)(s3 + 0x448) != 1) {
        goto exit;
    }
    if (*(u32 *)(s3 + 0x444) == 0) {
        return 0;
    }
    result = sceKernelIsToolMode();
    if (result < 0) {
        goto exit;
    }
    u32 mask = (result == 0) ? 1 : 2;
    u32 arch = *(u32 *)(s3 + 0x444);
    if ((arch & mask) == 0) {
        Kprintf("target arch: 0x%08x\n", arch);
        result = SCE_PSHEET_ERROR_P8;
        goto exit;
    }
    if (arch == 2) {
        Kprintf("Package for PSP Development TOOL\n");
    }
    return 0;

exit:
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "get_core_header", result);
    return (result < 0) ? result : (int)SCE_PSHEET_ERROR_P6;
}

/* 0x17c0: sub_17C0 (get_archive_header) */
int sub_17C0(SceUID fd, int vkey, int idx, char *outName, u32 maxLen)
{
    u8 *s4 = g_unkP;
    u32 hdrOff = *(u32 *)(s4 + 0x4C8) + *(u32 *)(s4 + 0x458) + ((u32)idx << 4);
    int res = sceIoLseek32(fd, hdrOff, PSP_SEEK_SET);
    if (res < (int)hdrOff) {
        goto err;
    }
    res = sceIoRead(fd, s4 + 0x400, 0x10);
    if (res < 0x10) {
        goto err;
    }
    res = sub_2134(s4 + 0x400, 0x10, idx, (void *)vkey, s4 + 0x430);
    if (res < 0) {
        goto err;
    }
    u32 nameOff = *(u32 *)(s4 + 0x4C8) + *(u32 *)(s4 + 0x458) + *(u16 *)(s4 + 0x400);
    sceIoLseek32(fd, nameOff, PSP_SEEK_SET);
    u32 nameLen = (u32)(*(u16 *)(s4 + 0x402)) + 1;
    if (nameLen < maxLen) {
        maxLen = nameLen;
    }
    u32 readLen = (maxLen + 0xF) & ~0xFU;
    if (readLen > 0x400) {
        maxLen = 0x400;
    }
    res = sceIoRead(fd, s4, readLen);
    if (res < (int)readLen) {
        goto err;
    }
    res = sub_2134(s4, readLen, *(u16 *)(s4 + 0x400) >> 4, (void *)vkey, s4 + 0x430);
    if (res < 0) {
        goto err;
    }
    strncpy(outName, (char *)s4, maxLen);
    outName[maxLen - 1] = '\0';
    return 0;

err:
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "get_archive_header", res);
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_P2;
}

/* 0x1998: sub_1998 (get_params) */
int sub_1998(SceUID fd, int vkey, u32 *params, u32 count)
{
    u8 *s4 = g_unkP;
    u32 found = 0;
    sceIoLseek32(fd, *(u32 *)(s4 + 0x4C8) + *(u32 *)(s4 + 0x450), PSP_SEEK_SET);
    u32 totalParams = *(u32 *)(s4 + 0x454);
    for (u32 i = 0; i < totalParams; i++) {
        u32 slot = i & 0x7F;
        if (slot == 0) {
            u32 bytes = (totalParams - i) << 3;
            u32 readLen = (bytes <= 0x400) ? ((bytes + 0xF) & ~0xFU) : 0x400;
            int res = sceIoRead(fd, s4, readLen);
            if (res < (int)readLen) {
                Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "get_params", res);
                return (res < 0) ? res : (int)SCE_PSHEET_ERROR_P2;
            }
            sub_2134(s4, readLen, i >> 1, (void *)vkey, s4 + 0x420);
        }
        if (count > 0) {
            u32 *entry = (u32 *)(s4 + (slot << 3));
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

/* 0x1b3c: sub_1B3C (get_string) */
int sub_1B3C(SceUID fd, int vkey, u32 desc, char *outStr, u32 maxLen)
{
    if (desc == 0) {
        outStr[0] = '\0';
        return 0;
    }
    u8 *s3 = g_unkP;
    u32 lowOff = desc & 0xFFFF;
    u32 inBlkOff = desc & 0xF;
    u32 alignedOff = lowOff - inBlkOff;
    u32 strLen = desc >> 16;
    u32 seekPos = *(u32 *)(s3 + 0x4C8) + *(u32 *)(s3 + 0x450) + alignedOff;
    int res = sceIoLseek32(fd, seekPos, PSP_SEEK_SET);
    if (res < (int)seekPos) {
        goto err;
    }
    u32 totalLen = inBlkOff + strLen;
    char *dst = outStr;
    char *dstEnd = outStr + maxLen;
    for (u32 pos = 0; pos < totalLen; pos += 0x400) {
        u32 rem = totalLen - pos;
        u32 readLen = (rem <= 0x400) ? ((rem + 0xF) & ~0xFU) : 0x400;
        if (rem > 0x400) {
            rem = 0x400;
        }
        res = sceIoRead(fd, s3, readLen);
        if (res < (int)readLen) {
            goto err;
        }
        sub_2134(s3, readLen, alignedOff >> 4, (void *)vkey, s3 + 0x420);
        alignedOff += readLen;
        for (u32 k = inBlkOff; k < rem; k++) {
            if (dst == dstEnd) {
                break;
            }
            *dst++ = (char)s3[k];
        }
        inBlkOff = 0;
    }
    return 0;

err:
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "get_string", res);
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_P2;
}

/* 0x1cec: sub_1CEC (rename_in_dir) */
int sub_1CEC(char *path, const char *newName)
{
    char tmp[0x100];
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
    strncpy(tmp, path, dirLen);
    strncpy(tmp + dirLen, newName, 0xFF - dirLen);
    tmp[0xFF] = '\0';
    sceIoRemove(tmp);
    int res = sceIoRename(path, tmp);
    if (res < 0) {
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "rename_in_dir", res);
        return res;
    }
    strcpy(path, tmp);
    return dirLen;
}

/* 0x1e14: sub_1E14 (recursive_mkdir) */
int sub_1E14(char *path, u32 startOff)
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

/* 0x1ef0: sub_1EF0 */
int sub_1EF0(void *pgdHdr)
{
    static const u8 pgdMagic[4] = { 0x00, 'P', 'G', 'D' };
    u8 *p = (u8 *)pgdHdr;
    sceKernelMemset(p, 0, 0x90);
    sceKernelMemcpy(p, pgdMagic, 4);
    *(u32 *)(p + 0x04) = 1;
    *(u32 *)(p + 0x48) = 0x400;
    *(u32 *)(p + 0x4C) = 0x90;
    return 0;
}

/* 0x1f4c: sub_01F4C (data_encrypt) */
int sub_01F4C(void *a0, int a1, void *a2, void *a3)
{
    void *s2 = g_unkP + 0x488;
    int result = sceDrmBBCipherInit(s2, 2, 1, a3, a2, 0);
    if (result >= 0) {
        result = sceDrmBBCipherUpdate(s2, a0, a1);
        if (result >= 0) {
            result = sceDrmBBCipherFinal(s2);
            if (result >= 0) {
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n\n", "encrypt_wrapper.c", "data_encrypt", result);
    return SCE_PSHEET_ERROR_P3;
}

/* 0x2000: sub_2000 (create_mac) */
int sub_2000(void *a0, int a1, void *a2, void *a3)
{
    void *s2 = g_unkP + 0x4A0;
    int result = sceDrmBBMacInit(s2, 2);
    if (result >= 0) {
        result = sceDrmBBMacUpdate(s2, a0, a1);
        if (result >= 0) {
            result = sceDrmBBMacFinal(s2, a3, a2);
            if (result >= 0) {
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n\n", "encrypt_wrapper.c", "create_mac", result);
    return SCE_PSHEET_ERROR_P3;
}

/* 0x20c8: sub_20C8 */
int sub_20C8(void *a0, int a1, void *a2, void *a3, void *t0)
{
    int res = sub_01F4C(a0, a1, a2, a3);
    if (res >= 0) {
        res = sub_2000(a0, a1, a2, t0);
        if (res > 0) {
            res = 0;
        }
    }
    return res;
}

/* 0x2134: sub_2134 (drmbb_decrypt) */
int sub_2134(void *a0, int a1, int a2, void *a3, void *t0)
{
    void *s2 = g_unkP + 0x470;
    int result = sceDrmBBCipherInit(s2, 1, 2, t0, a3, a2);
    if (result >= 0) {
        result = sceDrmBBCipherUpdate(s2, a0, a1);
        if (result >= 0) {
            result = sceDrmBBCipherFinal(s2);
            if (result >= 0) {
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n\n", "decrypt_wrapper.c", "drmbb_decrypt", result);
    return SCE_PSHEET_ERROR_P3;
}

/* 0x21f0: sub_21F0 */
int sub_21F0(SceUID fd, int offset, int size, void *vkey, void *mac)
{
    void *mkey = g_unkP + 0x4A0;
    int res = sceIoLseek32(fd, offset, PSP_SEEK_SET);
    if (res < offset) {
        goto err;
    }
    res = sceDrmBBMacInit(mkey, 1);
    if (res < 0) {
        goto err;
    }
    u32 done = 0;
    while (done < (u32)size) {
        u32 chunk = (u32)size - done;
        if (chunk > (u32)buf[1]) {
            chunk = (u32)buf[1];
        }
        res = sceIoRead(fd, (void *)buf[0], chunk);
        if (res < (int)chunk) {
            goto err;
        }
        res = sceDrmBBMacUpdate(mkey, (void *)buf[0], chunk);
        if (res < 0) {
            goto err;
        }
        res = getSemaIdStatusUsingBuf();
        if (res == 1) {
            res = SCE_PSHEET_ERROR_ABORTED;
        }
        if (res < 0) {
            goto err;
        }
        done += (u32)buf[1];
    }
    if (mac == NULL) {
        res = sceIoRead(fd, (void *)buf[0], 0x10);
        if (res < 0x10) {
            goto err;
        }
        mac = (void *)buf[0];
    }
    res = sceDrmBBMacFinal2(mkey, mac, vkey);
    if (res >= 0) {
        return 0;
    }
    res = SCE_PSHEET_ERROR_P5;

err:
    Kprintf("%s: error %08x\n", "decrypt_wrapper.c", res);
    if (res == (int)SCE_PSHEET_ERROR_P4) {
        return SCE_PSHEET_ERROR_P5;
    }
    if (((u32)res & ~0xFFU) == SCE_PSHEET_ERROR_P4) {
        return SCE_PSHEET_ERROR_P3;
    }
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_P5;
}
