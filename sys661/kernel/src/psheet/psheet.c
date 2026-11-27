/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include "psheet_int.h"

static inline int get_k1(void)
{
    int savedK1;
    __asm__ volatile ("move %0, $k1" : "=r"(savedK1));
    return savedK1;
}

static inline void set_k1(int savedK1)
{
    __asm__ volatile ("move $k1, %0" : : "r"(savedK1));
}

/* .data @ 0x2a40..0x2a80 */
ScePsheetState g_state;
u32 g_pkgKey[8] = {
    0x2d5de2ed, 0xe512f8bb, 0x32595c3c, 0x43e2e3fa,
    0xebfb7427, 0xd701a0a4, 0x339e5602, 0x8357198c,
};

/* .bss @ 0x2a80..0x2f50 (0x4d0 bytes) */
ScePsheetWork g_work;

/* 0x0000: sceDRMInstallInit */
int sceDRMInstallInit(void *address, u32 size)
{
    int savedK1 = get_k1();
    set_k1(savedK1 << 11);
    sceKernelMemset(&g_work, 0, sizeof(g_work));

    if ((int)((((u32)address) | size | ((u32)address + size)) & (savedK1 << 11)) < 0) {
        set_k1(savedK1);
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }

    getSemaIdStatusUsingTimeout(0, 1);
    set_state(0);
    g_state.workBuf = address;
    g_state.workBufSize = size;

    if ((u32)address < 1 || size <= 0x0001FFFF) {
        g_state.workBuf = NULL;
        g_state.workBufSize = 0;
        set_k1(savedK1);
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }

    set_k1(savedK1);
    return 0;
}

/* 0x00cc: sceDRMInstallGetPkgInfo */
int sceDRMInstallGetPkgInfo(char *pkgPath, void *vkey, void *pkgInfo)
{
    int ret;
    int savedK1 = get_k1();

    if (g_state.workBuf == NULL) {
        return SCE_PSHEET_ERROR_NOT_INITIALIZED;
    }
    if (pkgPath == NULL || vkey == NULL || pkgInfo == NULL) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    set_k1(savedK1 << 11);
    if ((int)(((savedK1 << 11) & (u32)pkgPath)) < 0 ||
        (int)((((u32)vkey + 16) | (u32)vkey) & (savedK1 << 11)) < 0 ||
        (int)((((u32)pkgInfo + 16) | (u32)pkgInfo) & (savedK1 << 11)) < 0) {
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
    set_k1(savedK1);
    return ret;
}

/* 0x01ec: sceDRMInstallGetFileInfo */
int sceDRMInstallGetFileInfo(char *file, void *vkey, int fileIdx, void *fileInfo)
{
    int ret;
    int savedK1;

    if (g_state.workBuf == NULL) {
        return SCE_PSHEET_ERROR_NOT_INITIALIZED;
    }
    if (file == NULL || vkey == NULL || fileInfo == NULL) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    savedK1 = get_k1();
    set_k1(savedK1 << 11);
    if ((int)(((savedK1 << 11) & (u32)file)) < 0 ||
        (int)((((u32)vkey + 16) | (u32)vkey) & (savedK1 << 11)) < 0 ||
        (int)((((u32)fileInfo + 0x108) | (u32)fileInfo) & (savedK1 << 11)) < 0) {
        ret = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    } else {
        set_k1(((u32)(savedK1 << 11)) >> 11);
        SceUID pkgFd = sceIoOpen(file, IOASSIGN_RDONLY, 0);
        set_k1(savedK1 << 11);
        if (pkgFd < 0) {
            ret = pkgFd;
        } else {
            set_state(0);
            ret = get_file_info(pkgFd, vkey, fileIdx, fileInfo);
            sceIoClose(pkgFd);
        }
    }

    sceKernelMemset(&g_work, 0, sizeof(g_work));
    set_k1(savedK1);
    return ret;
}

/* 0x0324: sceDRMInstallInstall */
int sceDRMInstallInstall(const char *path, void *vkey, void *opt)
{
    char localPath[0x100];
    SceUID pkgFd = -1;
    int ret;
    int savedK1;

    if (g_state.workBuf == NULL) {
        return SCE_PSHEET_ERROR_NOT_INITIALIZED;
    }
    if (path == NULL || vkey == NULL) {
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    savedK1 = get_k1();
    set_k1(savedK1 << 11);
    do {
        if ((int)(((savedK1 << 11) & (u32)path)) < 0 ||
            (int)((((u32)vkey + 16) | (u32)vkey) & (savedK1 << 11)) < 0 ||
            (int)((((u32)opt + 16) | (u32)opt) & (savedK1 << 11)) < 0) {
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
    set_k1(savedK1);
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
    int savedK1 = get_k1();
    if ((int)((((u32)progressOut + 8) | (u32)progressOut) & (savedK1 << 11)) >= 0) {
        set_k1(savedK1 << 11);
        int result = getSemaIdStatus(progressOut);
        set_k1(savedK1);
        return result;
    }
    set_k1(savedK1);
    return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
}

int scePsheet_driver_3BA93CFA(SceUInt *progressOut) __attribute__((alias("scePsheet_3BA93CFA")));

/* 0x06fc: sceDRMInstallAbort */
void sceDRMInstallAbort(void)
{
    int savedK1 = get_k1();
    set_k1(savedK1 << 11);
    set_state(1);
    set_k1(savedK1);
}
