/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include <common/types.h>
#include <iofilemgr_kernel.h>
#include <threadman_kernel.h>
#include <sysmem_kdebug.h>
#include <string.h>

#ifndef PSP_O_RDONLY
#define PSP_O_RDONLY 0x0001
#define PSP_O_WRONLY 0x0002
#define PSP_O_RDWR   0x0003
#define PSP_O_CREAT  0x0200
#define PSP_O_TRUNC  0x0400
#endif
#ifndef PSP_SEEK_SET
#define PSP_SEEK_SET 0
#endif
#ifndef PSP_SEEK_CUR
#define PSP_SEEK_CUR 1
#endif
#ifndef PSP_SEEK_END
#define PSP_SEEK_END 2
#endif

#define SCE_PSHEET_ERROR_NOT_INITIALIZED 0x80510108
#define SCE_PSHEET_ERROR_ILLEGAL_ADDRESS 0x80510109
#define SCE_PSHEET_ERROR_NO_CORE_HEADER  0x80510111
#define SCE_PSHEET_ERROR_INVALID_FORMAT  0x80510110
#define SCE_PSHEET_ERROR_CRYPTO_FAILED   0x80510102
#define SCE_PSHEET_ERROR_AMCTRL_BASE     0x80510300
#define SCE_PSHEET_ERROR_MAC_MISMATCH    0x80510101
#define SCE_PSHEET_ERROR_CORE_MAC        0x80510113
#define SCE_PSHEET_ERROR_UNSUPPORTED     0x80510112
#define SCE_PSHEET_ERROR_ABORTED         0x80510120

#ifndef IOASSIGN_RDONLY
#define IOASSIGN_RDONLY PSP_O_RDONLY
#endif

static inline int get_k1(void)
{
    int k1;
    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    return k1;
}

static inline void set_k1(int k1)
{
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
}

extern void *sceKernelMemset(void *dst, int val, SceSize size);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize size);
extern int sceKernelIsToolMode(void);
extern int sceDrmBBCipherInit(void *ckey, int type, int mode, void *headerKey, void *versionKey, u32 seed);
extern int sceDrmBBCipherUpdate(void *ckey, void *buf, int size);
extern int sceDrmBBCipherFinal(void *ckey);
extern int sceDrmBBMacInit(void *mkey, int type);
extern int sceDrmBBMacUpdate(void *mkey, void *buf, int size);
extern int sceDrmBBMacFinal(void *mkey, void *buf, void *vkey);
extern int sceDrmBBMacFinal2(void *mkey, void *out, void *vkey);
extern int sceUtilsPrepareGetLoadModuleSLength(void *buf, int size, void *out);
extern int sceUtilsPrepareGetLoadModuleRLength(void *buf, int size, void *out);

typedef struct {
    SceUID semaId;              /* 0x00 */
    SceUID auxSemaId;           /* 0x04 */
    SceUInt progressBytes;      /* 0x08 */
    SceUInt totalBytes;         /* 0x0C */
    void *workBuf;              /* 0x10 */
    u32 workBufSize;            /* 0x14 */
    int abortState;             /* 0x18 */
    int pad;                    /* 0x1C */
} ScePsheetState;

/* .data @ 0x2a40..0x2a80 */
static ScePsheetState g_state;
static u32 g_pkgKey[8] = {
    0x2d5de2ed, 0xe512f8bb, 0x32595c3c, 0x43e2e3fa,
    0xebfb7427, 0xd701a0a4, 0x339e5602, 0x8357198c,
};

typedef struct {
    u8 magic[4];                /* 0x00 */
    u32 version;                /* 0x04 */
    u32 keyIndex;               /* 0x08 */
    u32 headerPad;              /* 0x0C */
    u8 headerKey[0x10];         /* 0x10 */
    u8 pathMac[0x10];           /* 0x20 */
    u8 dataKey[0x10];           /* 0x30 */
    u32 dataOffset;             /* 0x40 */
    u32 dataSize;               /* 0x44 */
    u32 blockSize;              /* 0x48 */
    u32 headerSize;             /* 0x4C */
    u8 tablePad[0x10];          /* 0x50 */
    u8 tableMac[0x10];          /* 0x60 */
    u8 headerMacVkey[0x10];     /* 0x70 */
    u8 headerMacPkgKey[0x10];   /* 0x80 */
} ScePsheetPgdHeader;

typedef struct {
    u32 flags;
    u32 size;
    char name[0x100];
} ScePsheetFileInfo;

typedef struct {
    u8 ioBuf[0x400];            /* 0x000 */
    struct {
        u16 nameOff;            /* 0x400 */
        u16 nameLen;            /* 0x402 */
        u32 dataOff;            /* 0x404 */
        u32 dataSize;           /* 0x408 */
        u32 flags;              /* 0x40C */
    } arcHdr;
    u8 coreHeaderKey[0x10];     /* 0x410 */
    u8 paramKey[0x10];          /* 0x420 */
    u8 dataKey[0x10];           /* 0x430 */
    u32 version;                /* 0x440 */
    u32 targetArch;             /* 0x444 */
    u32 pkgType;                /* 0x448 */
    u32 pad;                    /* 0x44C */
    u32 paramOff;               /* 0x450 */
    u32 paramCount;             /* 0x454 */
    u32 dataOff;                /* 0x458 */
    u32 dataSize;               /* 0x45C */
    u8 coreMac[0x10];           /* 0x460 */
    u8 decryptCtx[0x18];        /* 0x470 */
    u8 encryptCtx[0x18];        /* 0x488 */
    u8 macCtx[0x28];            /* 0x4A0 */
    u32 coreOff;                /* 0x4C8 */
    u32 coreSize;               /* 0x4CC */
} ScePsheetWork;

/* .bss @ 0x2a80..0x2f50 (0x4d0 bytes) */
static ScePsheetWork g_work;

int getSemaIdStatus(SceUInt *progressOut);
int getSemaIdStatusIncrement(SceUInt delta);
int getSemaIdStatusUsingTimeout(int progress, int total);
int getSemaIdStatusUsingBuf(void);
int set_state(int state);
int open_and_mac_check(char *file);
int get_core_header(SceUID pkgFd, void *vkey);
int get_package_info(SceUID pkgFd, void *vkey, void *pkgInfo);
int get_file_info(SceUID pkgFd, void *vkey, int fileIdx, void *fileInfo);
int install_package(SceUID pkgFd, char *path, void *vkey, void *opt);
int get_archive_header(SceUID pkgFd, void *vkey, int idx, char *outName, u32 maxLen);
int get_params(SceUID pkgFd, void *vkey, u32 *params, u32 count);
int get_string(SceUID pkgFd, void *vkey, u32 desc, char *outStr, u32 maxLen);
int rename_in_dir(char *path, const char *newName);
int recursive_mkdir(char *path, u32 startOff);
int init_pgd_header(void *pgdHdr);
int data_encrypt(void *buf, int size, void *vkey, void *headerKey);
int create_mac(void *buf, int size, void *vkey, void *macOut);
int encrypt_and_mac(void *buf, int size, void *vkey, void *headerKey, void *macOut);
int drmbb_decrypt(void *buf, int size, int seed, void *vkey, void *headerKey);
int verify_file_mac(SceUID pkgFd, int offset, int size, void *vkey, void *mac);

/* 0x0000: sceDRMInstallInit */
int sceDRMInstallInit(void *address, u32 size)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    sceKernelMemset(&g_work, 0, sizeof(g_work));

    if ((int)((((u32)address) | size | ((u32)address + size)) & (k1 << 11)) < 0) {
        set_k1(k1);
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }

    getSemaIdStatusUsingTimeout(0, 1);
    set_state(0);
    g_state.workBuf = address;
    g_state.workBufSize = size;

    if ((u32)address < 1 || size <= 0x0001FFFF) {
        g_state.workBuf = NULL;
        g_state.workBufSize = 0;
        set_k1(k1);
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }

    set_k1(k1);
    return 0;
}

/* 0x00cc: sceDRMInstallGetPkgInfo */
int sceDRMInstallGetPkgInfo(char *pkgPath, void *vkey, void *pkgInfo)
{
    int ret;
    int k1 = get_k1();

    if (g_state.workBuf == NULL) {
        return SCE_PSHEET_ERROR_NOT_INITIALIZED;
    }
    if (pkgPath == NULL || vkey == NULL || pkgInfo == NULL) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    set_k1(k1 << 11);
    if ((int)(((k1 << 11) & (u32)pkgPath)) < 0 ||
        (int)((((u32)vkey + 16) | (u32)vkey) & (k1 << 11)) < 0 ||
        (int)((((u32)pkgInfo + 16) | (u32)pkgInfo) & (k1 << 11)) < 0) {
        ret = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    } else {
        set_state(0);
        SceUID pkgFd = open_and_mac_check(pkgPath);
        if (pkgFd < 0) {
            ret = pkgFd;
        } else {
            ret = get_package_info(pkgFd, vkey, pkgInfo);
            sceIoClose(pkgFd);
        }
    }

    sceKernelMemset(&g_work, 0, sizeof(g_work));
    set_k1(k1);
    return ret;
}

/* 0x01ec: sceDRMInstallGetFileInfo */
int sceDRMInstallGetFileInfo(char *file, void *vkey, int fileIdx, void *fileInfo)
{
    int ret;
    int k1;

    if (g_state.workBuf == NULL) {
        return SCE_PSHEET_ERROR_NOT_INITIALIZED;
    }
    if (file == NULL || vkey == NULL || fileInfo == NULL) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)(((k1 << 11) & (u32)file)) < 0 ||
        (int)((((u32)vkey + 16) | (u32)vkey) & (k1 << 11)) < 0 ||
        (int)((((u32)fileInfo + 0x108) | (u32)fileInfo) & (k1 << 11)) < 0) {
        ret = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    } else {
        set_k1(((u32)(k1 << 11)) >> 11);
        SceUID pkgFd = sceIoOpen(file, IOASSIGN_RDONLY, 0);
        set_k1(k1 << 11);
        if (pkgFd < 0) {
            ret = pkgFd;
        } else {
            set_state(0);
            ret = get_file_info(pkgFd, vkey, fileIdx, fileInfo);
            sceIoClose(pkgFd);
        }
    }

    sceKernelMemset(&g_work, 0, sizeof(g_work));
    set_k1(k1);
    return ret;
}

/* 0x0324: sceDRMInstallInstall */
int sceDRMInstallInstall(const char *path, void *vkey, void *opt)
{
    char localPath[0x100];
    SceUID pkgFd = -1;
    int ret;
    int k1;

    if (g_state.workBuf == NULL) {
        return SCE_PSHEET_ERROR_NOT_INITIALIZED;
    }
    if (path == NULL || vkey == NULL) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    k1 = get_k1();
    set_k1(k1 << 11);
    do {
        if ((int)(((k1 << 11) & (u32)path)) < 0 ||
            (int)((((u32)vkey + 16) | (u32)vkey) & (k1 << 11)) < 0 ||
            (int)((((u32)opt + 16) | (u32)opt) & (k1 << 11)) < 0) {
            ret = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
            break;
        }

        set_state(0);
        sceKernelMemset(localPath, 0, sizeof(localPath));
        strncpy(localPath, path, 0xFE);
        ret = rename_in_dir(localPath, "_EBOOT.PBP");
        if (ret < 0) {
            break;
        }
        int dirLen = ret;
        if (0x100 - dirLen < 13) {
            ret = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
            break;
        }
        pkgFd = open_and_mac_check(localPath);
        if (pkgFd < 0) {
            ret = pkgFd;
            break;
        }
        localPath[dirLen] = '\0';
        ret = install_package(pkgFd, localPath, vkey, opt);
        if (ret < 0) {
            break;
        }
        ret = sceIoClose(pkgFd);
        pkgFd = -1;
        if (ret < 0) {
            break;
        }
        __builtin_memcpy(localPath + dirLen, "_EBOOT.PBP", 11);
        ret = sceIoRemove(localPath);
        if (ret < 0) {
            break;
        }
        __builtin_memcpy(localPath + dirLen, "_TEMP.PBP", 10);
        ret = rename_in_dir(localPath, "EBOOT.PBP");
        if (ret > 0) {
            ret = 0;
        }
    } while (0);

    sceKernelMemset(&g_work, 0, sizeof(g_work));
    if (pkgFd >= 0) {
        sceIoClose(pkgFd);
    }
    set_k1(k1);
    return ret;
}

/* 0x0564: module_start */
int module_start(SceSize argc __attribute__((unused)), void *argp __attribute__((unused)))
{
    SceUID sema = sceKernelCreateSema("ScePsheet1", 0, 1, 1, 0);
    g_state.semaId = sema;
    if (sema >= 0) {
        sceDRMInstallInit(NULL, 0);
        return 0;
    }
    if (g_state.auxSemaId > 0) {
        sceKernelDeleteSema(g_state.auxSemaId);
    }
    sceKernelMemset(&g_state, 0, 64);
    sceKernelMemset(&g_work, 0, sizeof(g_work));
    return (sema < -1) ? sema : -1;
}

/* 0x0610: module_stop / module_reboot_before */
int module_reboot_before(SceSize args __attribute__((unused)), void *argp __attribute__((unused)))
{
    if (g_state.semaId > 0) {
        sceKernelDeleteSema(g_state.semaId);
    }
    if (g_state.auxSemaId > 0) {
        sceKernelDeleteSema(g_state.auxSemaId);
    }
    sceKernelMemset(&g_state, 0, 64);
    sceKernelMemset(&g_work, 0, sizeof(g_work));
    return 0;
}

int module_stop(SceSize args, void *argp) __attribute__((alias("module_reboot_before")));

/* 0x0684: sceDRMInstallEnd */
int sceDRMInstallEnd(void)
{
    sceDRMInstallInit(NULL, 0);
    return 0;
}

/* 0x06a8: scePsheet_3BA93CFA */
int scePsheet_3BA93CFA(SceUInt *progressOut)
{
    int k1 = get_k1();
    if ((int)((((u32)progressOut + 8) | (u32)progressOut) & (k1 << 11)) >= 0) {
        set_k1(k1 << 11);
        int result = getSemaIdStatus(progressOut);
        set_k1(k1);
        return result;
    }
    set_k1(k1);
    return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
}

int scePsheet_driver_3BA93CFA(SceUInt *progressOut) __attribute__((alias("scePsheet_3BA93CFA")));

/* 0x06fc: sceDRMInstallAbort */
void sceDRMInstallAbort(void)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    set_state(1);
    set_k1(k1);
}

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

/* 0x09ec: install_package (install) */
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
                res = getSemaIdStatusUsingBuf();
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

/* 0x1388: getSemaIdStatusUsingBuf (get_state) */
int getSemaIdStatusUsingBuf(void)
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

/* 0x1f4c: data_encrypt */
int data_encrypt(void *buf, int size, void *vkey, void *headerKey)
{
    void *ctx = g_work.encryptCtx;
    int result = sceDrmBBCipherInit(ctx, 2, 1, headerKey, vkey, 0);
    if (result >= 0) {
        result = sceDrmBBCipherUpdate(ctx, buf, size);
        if (result >= 0) {
            result = sceDrmBBCipherFinal(ctx);
            if (result >= 0) {
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n\n", "encrypt_wrapper.c", "data_encrypt", result);
    return SCE_PSHEET_ERROR_CRYPTO_FAILED;
}

/* 0x2000: create_mac */
int create_mac(void *buf, int size, void *vkey, void *macOut)
{
    void *ctx = g_work.macCtx;
    int result = sceDrmBBMacInit(ctx, 2);
    if (result >= 0) {
        result = sceDrmBBMacUpdate(ctx, buf, size);
        if (result >= 0) {
            result = sceDrmBBMacFinal(ctx, macOut, vkey);
            if (result >= 0) {
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n\n", "encrypt_wrapper.c", "create_mac", result);
    return SCE_PSHEET_ERROR_CRYPTO_FAILED;
}

/* 0x20c8: encrypt_and_mac */
int encrypt_and_mac(void *buf, int size, void *vkey, void *headerKey, void *macOut)
{
    int res = data_encrypt(buf, size, vkey, headerKey);
    if (res >= 0) {
        res = create_mac(buf, size, vkey, macOut);
        if (res > 0) {
            res = 0;
        }
    }
    return res;
}

/* 0x2134: drmbb_decrypt */
int drmbb_decrypt(void *buf, int size, int seed, void *vkey, void *headerKey)
{
    void *ctx = g_work.decryptCtx;
    int result = sceDrmBBCipherInit(ctx, 1, 2, headerKey, vkey, seed);
    if (result >= 0) {
        result = sceDrmBBCipherUpdate(ctx, buf, size);
        if (result >= 0) {
            result = sceDrmBBCipherFinal(ctx);
            if (result >= 0) {
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n\n", "decrypt_wrapper.c", "drmbb_decrypt", result);
    return SCE_PSHEET_ERROR_CRYPTO_FAILED;
}

/* 0x21f0: verify_file_mac */
int verify_file_mac(SceUID pkgFd, int offset, int size, void *vkey, void *mac)
{
    void *mkey = g_work.macCtx;
    int res;
    do {
        res = sceIoLseek32(pkgFd, offset, PSP_SEEK_SET);
        if (res < offset) {
            break;
        }
        res = sceDrmBBMacInit(mkey, 1);
        if (res < 0) {
            break;
        }
        u32 done = 0;
        int failed = 0;
        while (done < (u32)size) {
            u32 chunk = (u32)size - done;
            if (chunk > g_state.workBufSize) {
                chunk = g_state.workBufSize;
            }
            res = sceIoRead(pkgFd, g_state.workBuf, chunk);
            if (res < (int)chunk) {
                failed = 1;
                break;
            }
            res = sceDrmBBMacUpdate(mkey, g_state.workBuf, chunk);
            if (res < 0) {
                failed = 1;
                break;
            }
            res = getSemaIdStatusUsingBuf();
            if (res == 1) {
                res = SCE_PSHEET_ERROR_ABORTED;
            }
            if (res < 0) {
                failed = 1;
                break;
            }
            done += g_state.workBufSize;
        }
        if (failed) {
            break;
        }
        if (mac == NULL) {
            res = sceIoRead(pkgFd, g_state.workBuf, 0x10);
            if (res < 0x10) {
                break;
            }
            mac = g_state.workBuf;
        }
        res = sceDrmBBMacFinal2(mkey, mac, vkey);
        if (res >= 0) {
            return 0;
        }
        res = SCE_PSHEET_ERROR_MAC_MISMATCH;
    } while (0);

    Kprintf("%s: error %08x\n", "decrypt_wrapper.c", res);
    if (res == (int)SCE_PSHEET_ERROR_AMCTRL_BASE) {
        return SCE_PSHEET_ERROR_MAC_MISMATCH;
    }
    if (((u32)res & ~0xFFU) == SCE_PSHEET_ERROR_AMCTRL_BASE) {
        return SCE_PSHEET_ERROR_CRYPTO_FAILED;
    }
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_MAC_MISMATCH;
}
