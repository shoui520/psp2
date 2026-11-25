/* Copyright (C) 2011, 2012, 2013, 2014, 2015 The uOFW team
   See the file COPYING for copying permission.
 */

#include <common_imp.h>
#include <sysmem_kernel.h>
#include <sysmem_sysclib.h>
#include <iofilemgr_kernel.h>

#define SEEK_END 2

SCE_MODULE_INFO("sceHttpStorage_Service",
                SCE_MODULE_KERNEL | SCE_MODULE_ATTR_EXCLUSIVE_START | SCE_MODULE_ATTR_EXCLUSIVE_LOAD,
                1, 3);
SCE_MODULE_STOP("sceHttpStorage_driver_C59AC98A");
SCE_SDK_VERSION(SDK_VERSION);

extern int sceKernelUnloadModule(int modid);
extern int sceKernelStartModule(int modid, int args, void *argp, int *status, void *opt);
extern int sceKernelLoadModuleForKernel(const char *prx, int flags, void *opt);
extern int sceKernelSearchModuleByName(const char *module);
extern int sceKernelStopModule(int modid, int args, void *argp, int *status, void *opt);
extern int sceChkregGetPsCode(u8 *buf);

s32 sceHttpStorage_driver_24AA94F4(u32 id, const void *buf, SceSize size) __attribute__((alias("sceHttpStorageWrite")));
s32 sceHttpStorage_driver_2D8DAE58(s32 id, SceIoStat *stat) __attribute__((alias("sceHttpStorageGetstat")));
s32 sceHttpStorage_driver_700AAD44(u32 id, s32 flags, s32 mode) __attribute__((alias("sceHttpStorageOpen")));
s32 sceHttpStorage_driver_B33389CE(u32 id, s64 offset, s32 whence) __attribute__((alias("sceHttpStorageLseek")));
s32 sceHttpStorage_driver_CDA3D8F6(u32 id) __attribute__((alias("sceHttpStorageClose")));
s32 sceHttpStorage_driver_CDDF1103(u32 id, void *buf, SceSize size) __attribute__((alias("sceHttpStorageRead")));
s32 sceHttpStorage_driver_C59AC98A(void) __attribute__((alias("sceHttpStorageEnd")));

static SceUID g_fd[2] = { -1, -1 };

s32 sceHttpStorageOpen(u32 id, s32 flags, s32 mode)
{
    s32 oldk1 = pspShiftK1();
    if (id >= 2) {
        pspSetK1(oldk1);
        return 0x80000100;
    }
    if (g_fd[id] != -1) {
        pspSetK1(oldk1);
        return 0x80000020;
    }

    s32 fileDesc;
    if (id == 0) {
        fileDesc = sceIoOpen("flash1:/net/http/auth.dat", (flags & 0x02000603) | 0x04000000, mode & 0x1A4);
    } else if (id == 1) {
        fileDesc = sceIoOpen("flash1:/net/http/cookie.dat", (flags & 0x02000603) | 0x04000000, mode & 0x1A4);
    } else {
        fileDesc = 0x80000100;
    }
    if (fileDesc < 0) {
        pspSetK1(oldk1);
        return fileDesc;
    }

    g_fd[id] = fileDesc;
    pspSetK1(oldk1);
    return 0;
}

s32 sceHttpStorageClose(u32 id)
{
    s32 oldk1 = pspShiftK1();
    if (id >= 2) {
        pspSetK1(oldk1);
        return 0x80000100;
    }
    if (g_fd[id] >= 0) {
        sceIoClose(g_fd[id]);
        g_fd[id] = -1;
    }
    pspSetK1(oldk1);
    return 0;
}

s32 sceHttpStorageRead(u32 id, void *buf, SceSize size)
{
    s32 oldk1 = pspShiftK1();
    u32 addr = (u32)buf;
    if ((s32)((oldk1 << 11) & ((addr + size) | addr | size)) < 0) {
        pspSetK1(oldk1);
        return 0x800001FF;
    }
    if (id >= 2) {
        pspSetK1(oldk1);
        return 0x80000100;
    }
    if (g_fd[id] == -1) {
        pspSetK1(oldk1);
        return 0x80000001;
    }
    s32 ret = sceIoRead(g_fd[id], buf, size);
    pspSetK1(oldk1);
    return ret;
}

s32 sceHttpStorageWrite(u32 id, const void *buf, SceSize size)
{
    s32 oldk1 = pspShiftK1();
    u32 addr = (u32)buf;
    if ((s32)((oldk1 << 11) & ((addr + size) | addr | size)) < 0) {
        pspSetK1(oldk1);
        return 0x800001FF;
    }
    if (id >= 2) {
        pspSetK1(oldk1);
        return 0x80000100;
    }
    if (g_fd[id] == -1) {
        pspSetK1(oldk1);
        return 0x80000001;
    }
    s32 ret = sceIoWrite(g_fd[id], buf, size);
    pspSetK1(oldk1);
    return ret;
}

s32 sceHttpStorageLseek(u32 id, s64 offset, s32 whence)
{
    s32 oldk1 = pspShiftK1();
    if (id >= 2) {
        pspSetK1(oldk1);
        return 0x80000100;
    }
    if (g_fd[id] == -1) {
        pspSetK1(oldk1);
        return 0x80000001;
    }
    s32 ret = (s32)sceIoLseek(g_fd[id], offset, whence);
    pspSetK1(oldk1);
    return ret;
}

s32 sceHttpStorageGetstat(s32 id, SceIoStat *stat)
{
    s32 oldk1 = pspShiftK1();
    if ((s32)((oldk1 << 11) & (u32)stat) < 0) {
        pspSetK1(oldk1);
        return 0x800001FF;
    }
    const char *path;
    if (id == 0) {
        path = "flash1:/net/http/auth.dat";
    } else if (id == 1) {
        path = "flash1:/net/http/cookie.dat";
    } else {
        pspSetK1(oldk1);
        return 0x80000100;
    }
    s32 ret = sceIoGetstat(path, stat);
    pspSetK1(oldk1);
    return ret;
}

s32 sceHttpStorageEnd(void)
{
    for (u32 i = 0; i < 2; i++) {
        if (g_fd[i] != -1) {
            sceIoClose(g_fd[i]);
            g_fd[i] = -1;
        }
    }
    return 0;
}

s32 module_reboot_before(void)
{
    for (u32 i = 0; i < 2; i++) {
        if (g_fd[i] != -1) {
            sceIoClose(g_fd[i]);
            g_fd[i] = -1;
        }
    }
    return 0;
}

s32 sceHttpStorage_bridge_04EF00F8(u8 *psCode)
{
    int status;
    s32 loadedMod = -1;
    s32 oldk1 = pspShiftK1();
    u32 addr = (u32)psCode;

    if ((s32)(((addr + 8) | addr) & (oldk1 << 11)) < 0) {
        pspSetK1(oldk1);
        return 0x80000023;
    }

    s32 modid = sceKernelSearchModuleByName("sceChkreg");
    s32 res = 0;
    if (modid == (s32)0x8002012E) {
        modid = sceKernelLoadModuleForKernel("flash0:/kd/chkreg.prx", 0, NULL);
        if (modid < 0) {
            pspSetK1(oldk1);
            return modid;
        }
        loadedMod = modid;
        res = sceKernelStartModule(modid, 0, NULL, &status, NULL);
    } else if (modid < 0) {
        pspSetK1(oldk1);
        return modid;
    }

    if (res >= 0) {
        res = sceChkregGetPsCode(psCode);
    }

    if (loadedMod >= 0) {
        sceKernelStopModule(loadedMod, 0, NULL, NULL, NULL);
        s32 ures = sceKernelUnloadModule(loadedMod);
        if (ures < 0) {
            res = ures;
        }
    }
    pspSetK1(oldk1);
    return res;
}

s32 sceHttpStorage_bridge_EFE06A20(s32 type, char *buf, u32 size)
{
    s32 oldk1 = pspShiftK1();
    u32 addr = (u32)buf;

    if (buf == NULL || ((oldk1 << 11) & ((addr + size) | addr | size) & 0x80000000u) != 0) {
        pspSetK1(oldk1);
        return 0x80000103;
    }

    SceGameInfo *info = sceKernelGetGameInfo();
    switch (type) {
    case 1:
        if (size < 13) {
            pspSetK1(oldk1);
            return 0x80000104;
        }
        if ((u8)info->sfoGameDataId[0] == 0) {
            strncpy(buf, (const char *)info->sfoProductString, 13);
        } else {
            strncpy(buf, info->sfoGameDataId, 10);
        }
        pspSetK1(oldk1);
        return 0;
    case 2:
        if (size < 7) {
            pspSetK1(oldk1);
            return 0x80000104;
        }
        strncpy(buf, info->sfoAppVer, 7);
        pspSetK1(oldk1);
        return 0;
    case 3:
        if (size < 7) {
            pspSetK1(oldk1);
            return 0x80000104;
        }
        strncpy(buf, info->vsh_version, 7);
        pspSetK1(oldk1);
        return 0;
    default:
        pspSetK1(oldk1);
        return 0x800001FF;
    }
}
