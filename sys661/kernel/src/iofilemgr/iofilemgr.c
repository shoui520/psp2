#include <common_imp.h>

#include "interruptman.h"
#define sceKernelCallUIDObjCommonFunction _hdr_sceKernelCallUIDObjCommonFunction
#include "sysmem_kernel.h"
#undef sceKernelCallUIDObjCommonFunction
#include "sysmem_kdebug.h"
#include "sysmem_suspend_kernel.h"
#include "sysmem_sysclib.h"
#include "threadman_kernel.h"

#include "iofilemgr_kernel.h"
#include "iofilemgr_stdio.h"

s32 sceKernelCallUIDObjCommonFunction(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, void *ap);

SCE_MODULE_INFO("sceIOFileManager", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD
                                    | SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 7);
SCE_MODULE_BOOTSTART("IoFileMgrInit");
SCE_MODULE_REBOOT_BEFORE("IoFileMgrRebootBefore");
SCE_SDK_VERSION(SDK_VERSION);

typedef struct SceIoDeviceList
{
    struct SceIoDeviceList *next; // 0
    SceIoDeviceEntry arg;
} SceIoDeviceList;

typedef struct SceIoAlias
{
    struct SceIoAlias *next; // 0
    SceIoDeviceEntry *dev; // 4
    int attr; // 8
    int i_unit; // 12
    char alias[32]; // 16
    char blockDev[32]; // 48
    char fs[32]; // 80
} SceIoAlias; // size: 112

typedef struct SceIoHookList
{
    struct SceIoHookList *next;
    SceIoHookArg arg;
} SceIoHookList;

int deleted_func();
int deleted_func_close();
s64 deleted_func_offt();

// 69B8
SceIoDeviceFunction deleted_dt_func =
{
    deleted_func,
    deleted_func,
    deleted_func,
    deleted_func_close,
    (SceSSize (*)())deleted_func,
    (SceSSize (*)())deleted_func,
    deleted_func_offt,
    deleted_func,
    deleted_func,
    deleted_func,
    deleted_func,
    deleted_func,
    deleted_func_close,
    deleted_func,
    deleted_func,
    deleted_func,
    deleted_func,
    deleted_func,
    deleted_func,
    deleted_func,
    deleted_func,
    deleted_func
};

// 6A10
SceIoDeviceTable deleted_devtable = { "", 0, 0, "", &deleted_dt_func };

int _nulldev();
s64 _nulldev_offt();
int _nulldev_write(SceIoIob *iob __attribute__((unused)), const char *data, int len);

// 6A24
SceIoDeviceFunction _nullcon_function =
{
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    (SceSSize (*)())_nulldev,
    (SceSSize (*)())_nulldev_write,
    _nulldev_offt,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev,
    _nulldev
};

// 6A7C
SceIoDeviceTable _nullcon_driver = { "dummy_drv_iofile", 0, 0x00000800, "DUMMY_DRV", &_nullcon_function };

s32 iob_do_initialize(SceSysmemUidCB *cb, SceSysmemUidCB *uidWithFunc, s32 funcid, void *ap);
s32 iob_do_delete(SceSysmemUidCB *cb, SceSysmemUidCB *uidWithFunc, s32 funcid, void *ap);

// 6A90
SceSysmemUidLookupFunc IobFuncs[] =
{
    { 0xD310D2D9, (SceSysmemUidFunc)iob_do_initialize },
    { 0x87089863, (SceSysmemUidFunc)iob_do_delete },
    { 0, NULL }
};

// 6ADC
int default_thread_priority = 0x0F;

// 6AE0
int g_deleted_error = SCE_ERROR_KERNEL_DRIVER_DELETED; // exported as E4D75BC0

// 6AE4
SceIoDeviceEntry deleted_device = { &deleted_devtable, NULL, 0 };

int IoFileMgrForKernel_var_E6F7A8DF;
int g_savedAsyncPrio;

// 6B0C
SceIoDeviceList *g_devList;

// 6B10
SceIoAlias *g_aliasList;

// 6B14
SceIoHookList *g_hookList;

// 6B18
int g_pathbufCount;

// 6B1C
SceUID g_heap;

// 6B20
SceUID g_ktls;

// 6B24
int g_iobCount;

// 6B28
SceSysmemUidCB *g_uid_type;

// 6B2C
SceUID g_UIDs[64];

// 6C2C
SceUID g_uidXor;
char *g_pathbufBuf[2];

int validate_fd(int fileDesc, int reqFlags, int permMask, int ctrlFlags, SceIoIob **outIob);
int alloc_iob(SceIoIob **outIob, int allocLowFd);
int init_iob(SceIoIob *iob, int devType, SceIoDeviceEntry *dev, int userMode, int i_unit);
int free_iob(SceIoIob *iob);
void *alloc_pathbuf();
void free_pathbuf(void *ptr);
char *parsedev(const char *dev, char *outDev, int *i_unit);
SceIoAlias *findAliasByDev(const char *dev);
SceIoAlias *findAliasByName(const char *name);
int resolvePathUser(const char *path, SceIoDeviceEntry **dev, int *i_unit, char **dirNamePtr);
int resolvePath(const char *path, SceIoDeviceEntry **dev, int *i_unit, char **dirNamePtr, int userMode);
int strcmp_bs(const char *lhs, const char *rhs);
int open_main(SceIoIob *iob);
int __attribute__((noinline)) start_async_op(SceIoIob *iob, int cmd);
int __attribute__((noinline)) create_async_thread(SceIoIob *iob);
int iob_power_lock(SceIoIob *iob);
int iob_power_unlock(SceIoIob *iob);
int preobe_fdhook(SceIoIob *iob, char *file, int flags, SceMode mode);
int do_get_async_stat(SceUID fileDesc, SceInt64 *res, int poll, int cb, char *func);
int do_close(SceUID fileDesc, int async, int remove);
int do_open(const char *path, int flags, SceMode mode, int async, int retAddr, int oldK1);
int do_read(SceUID fileDesc, void *data, SceSize size, int async);
int do_write(SceUID fileDesc, const void *data, SceSize size, int async);
SceOff do_lseek(SceUID fileDesc, SceOff offset, int whence, int async);
int do_ioctl(SceUID fileDesc, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen, int async);
int xx_dir(const char *path, SceMode mode, int action);
int xx_stat(const char *file, SceIoStat *stat, int bits, int get);
int do_devctl(const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);
int do_deldrv(SceIoDeviceEntry *dev);
int sceIoGetUID(int fileDesc);
void add_device_list(SceIoDeviceList *list);
int delete_device_list(SceIoDeviceList *list);
SceIoDeviceList *alloc_device_list(void);
void free_device_list(SceIoDeviceList *list);
SceIoAlias *alloc_alias_tbl(void);
void free_alias_tbl(SceIoAlias *alias);
void add_alias_tbl(SceIoAlias *alias);
int delete_alias_tbl(SceIoAlias *alias);
SceIoAlias *lookup_alias_tbl(char *drive);
SceIoDeviceList *lookup_device_list(const char *drive);
void free_cwd(void *ktls);
s32 async_loop(SceSize args, void *argp);

int sceIoChangeAsyncPriority(int fileDesc, int prio)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    if (IoFileMgrForKernel_var_E6F7A8DF != 0 && default_thread_priority != g_savedAsyncPrio)
        prio = default_thread_priority;
    if (prio != -1 && (prio <= 0 || prio >= 127))
        return SCE_ERROR_KERNEL_ILLEGAL_PRIORITY;
    int oldK1 = pspShiftK1();
    if (pspK1IsUserMode() && prio != -1 && (prio <= 7 || prio >= 120))
    {
        // 0F34
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_PRIORITY;
    }
    // 0E8C
    if (fileDesc == -1)
    {
        // 0F20
        pspSetK1(oldK1);
        default_thread_priority = prio;
        return 0;
    }
    int ret = validate_fd(fileDesc, 0, 2, 1, &iob);
    if (ret < 0) {
        pspSetK1(oldK1);
        return ret;
    }
    iob->asyncPrio = prio;
    if (iob->asyncThread == 0) {
        pspSetK1(oldK1);
        return 0;
    }
    if (prio < 0) {
        // 0F04
        prio = sceKernelGetThreadCurrentPriority();
    }
    // 0ECC
    ret = sceKernelChangeThreadPriority(iob->asyncThread, prio);
    if (ret != 0) {
        pspSetK1(oldK1);
        return ret;
    }
    pspSetK1(oldK1);
    return 0;
}

void sceIoCloseAll()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    SceSysmemUidCB *cur = g_uid_type->PARENT0;
    // 0F74
    while (cur != g_uid_type)
    {
        int ret = validate_fd(cur->uid ^ g_uidXor, 0, 16, 14, &iob);
        if (ret >= 0 && (iob->i_flgs & 8) == 0)
        {
            // 0FD4
            if (iob->hook.arg != NULL) {
                // 1020
                iob->hook.arg->hook->funcs->Close(&iob->hook);
            }
            else if (iob->i_de != &deleted_device && iob->i_de->d_dp->dt_func->df_close != NULL) {
                // 1010
                iob->i_de->d_dp->dt_func->df_close(iob);
            }

            // 1000
            free_iob(iob);
        }
        cur = g_uid_type->PARENT0;
        // 0FAC
    }

    // 0FB4
    pspSetK1(oldK1);
}

int open_iob(SceIoIob *iob, const char *path, int flags, SceMode mode, int async)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int ret;
    SceIoDeviceEntry *dev;
    int i_unit;
    char *realPath;
    char *pathbuf = alloc_pathbuf();
    if (pathbuf == NULL)
        return SCE_ERROR_KERNEL_NO_MEMORY;

    if ((flags & 0x04000000) != 0)
        iob->userLevel = 8;

    do
    {
        if ((flags & 0x04000000) != 0 && iob->userMode != 0)
        {
            ret = SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
            break;
        }

        realPath = pathbuf;
        ret = resolvePath(path, &dev, &i_unit, &realPath, ((flags >> 26) ^ 1) & 1);
        if (ret < 0)
            break;

        ret = init_iob(iob, ret, dev, flags, i_unit);
        if (ret < 0)
            break;

        if (dev->d_dp->dt_func->df_open == NULL)
        {
            ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
            break;
        }

        iob->asyncArgs[2] = flags;
        iob->asyncArgs[1] = (int)realPath;
        iob->asyncArgs[3] = mode;
        iob->asyncArgs[0] = 0;
        if (async == 0)
        {
            ret = open_main(iob);
        }
        else
        {
            iob->asyncArgs[0] = (int)pathbuf;
            ret = start_async_op(iob, 1);
            if (ret >= 0)
            {
                ret = iob->fileDesc;
                pathbuf = NULL;
            }
        }
    } while (0);

    if (pathbuf != NULL)
        free_pathbuf(pathbuf);
    return ret;
}

int open_main(SceIoIob *iob)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if ((iob->i_flgs & 0x02000000) != 0)
        iob_power_lock(iob);

    int ret;
    if (preobe_fdhook(iob, (char *)iob->asyncArgs[1], iob->asyncArgs[2], iob->asyncArgs[3]) == 0)
        ret = iob->i_de->d_dp->dt_func->df_open(iob, (char *)iob->asyncArgs[1], iob->asyncArgs[2], iob->asyncArgs[3]);
    else
        ret = iob->hook.arg->hook->funcs->Open(&iob->hook, (char *)iob->asyncArgs[1], iob->asyncArgs[2], iob->asyncArgs[3]);

    if (ret >= 0)
        ret = iob->fileDesc;

    if (iob->asyncArgs[0] != 0)
    {
        free_pathbuf((void *)iob->asyncArgs[0]);
        iob->asyncArgs[0] = 0;
    }
    return ret;
}

int sceIoReopen(const char *file, int flags, SceMode mode, int fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int ra = pspGetRa();
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(file))
    {
        // 1434
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    int ret = do_close(fileDesc, 0, 0);
    if (ret < 0)
    {
        // 142C
        pspSetK1(oldK1);
        return ret;
    }
    ret = validate_fd(fileDesc, 0, 2, 3, &iob);
    if (ret < 0) {
        pspSetK1(oldK1);
        return ret;
    }
    iob_power_unlock(iob);
    ret = open_iob(iob, file, flags, mode, 0);
    if (ret < 0)
    {
        // 141C
        free_iob(iob);
        pspSetK1(oldK1);
        return ret;
    }
    if (sceKernelDeci2pReferOperations() != 0)
    {
        if (iob->newPath != NULL) {
            // 140C
            free_pathbuf(iob->newPath);
        }
        // 13A0
        char *path = alloc_pathbuf();
        if (path != NULL) {
            strncpy(path, file, 1023);
            path[1023] = '\0';
        }
        // 13C4
        iob->newPath = path;
    }
    // 13CC
    iob->retAddr = ra;
    iob->curThread = sceKernelGetThreadId();
    pspSetK1(oldK1);
    return ret;
}

SceUID sceIoDopen(const char *dirname)
{
    dbg_printf("sceIoDopen(%s)\n", dirname);
    SceIoIob *iob;
    SceIoDeviceEntry *dev;
    int i_unit;
    char *dirName;
    int ra = pspGetRa();
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(dirname))
    {
        // 15A8
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    char *path = alloc_pathbuf();
    if (path == NULL)
    {
        // 1598
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_MEMORY;
    }
    int ret = alloc_iob(&iob, 1);
    if (ret >= 0)
    {
        sceKernelRenameUID(sceIoGetUID(ret), dirname);
        if (pspK1IsUserMode())
            iob->retAddr = sceKernelGetSyscallRA();
        else
            iob->retAddr = ra;

        dirName = path;
        ret = resolvePathUser(dirname, &dev, &i_unit, &dirName);
        if (ret >= 0)
            ret = init_iob(iob, ret, dev, 0x01000008, i_unit);
        if (ret >= 0)
        {
            if (dev->d_dp->dt_func->df_dopen == NULL)
                ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
            else
                ret = dev->d_dp->dt_func->df_dopen(iob, dirName);
        }
        if (ret >= 0)
            ret = iob->fileDesc;
        else
            free_iob(iob);
    }

    if (path != NULL)
        free_pathbuf(path);
    pspSetK1(oldK1);
    dbg_printf("-> dopen returned %d\n", ret);
    return ret;
}

int sceIoDread(int fileDesc, SceIoDirent *dir)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(dir)) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    int ret = validate_fd(fileDesc, 8, 4, 0, &iob);
    if (ret < 0) {
        pspSetK1(oldK1);
        return ret;
    }
    if (iob->i_de->d_dp->dt_func->df_dread == NULL) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
    }
    // 1650
    ret = iob->i_de->d_dp->dt_func->df_dread(iob, dir);
    pspSetK1(oldK1);
    return ret;
}

int sceIoDclose(int fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    int ret = validate_fd(fileDesc, 8, 4, 2, &iob);
    if (ret < 0)
    {
        // 1704
        pspSetK1(oldK1);
        return ret;
    }
    ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
    if (iob->i_de == &deleted_device)
    {
        // 16F0
        free_iob(iob);
        pspSetK1(oldK1);
        return 0;
    }
    if (iob->i_de->d_dp->dt_func->df_dclose != NULL)
    {
        // 16DC
        ret = iob->i_de->d_dp->dt_func->df_dclose(iob);
        if (ret >= 0)
        {
            // 16F0
            free_iob(iob);
            pspSetK1(oldK1);
            return 0;
        }
    }
    // 16C4
    pspSetK1(oldK1);
    return ret;
}

int sceIoRemove(const char *file)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    SceIoDeviceEntry *dev;
    int i_unit;
    char *realPath;
    int ret;
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(file))
    {
        // 181C
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    char *path = alloc_pathbuf();
    if (path == NULL)
    {
        // 180C
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_MEMORY;
    }
    ret = alloc_iob(&iob, 0);
    if (ret >= 0)
    {
        realPath = path;
        ret = resolvePathUser(file, &dev, &i_unit, &realPath);
        if (ret >= 0)
            ret = init_iob(iob, ret, dev, 0x1000000, i_unit);
        if (ret >= 0)
        {
            if (dev->d_dp->dt_func->df_remove == NULL)
                ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
            else
                ret = dev->d_dp->dt_func->df_remove(iob, realPath);
        }
        free_iob(iob);
    }

    if (path != NULL)
        free_pathbuf(path);
    pspSetK1(oldK1);
    return ret;
}

int sceIoRename(const char *oldname, const char *newname)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(oldname) || !pspK1PtrOk(newname))
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    char *oldPathBuf = alloc_pathbuf();
    if (oldPathBuf == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_MEMORY;
    }
    char *newPathBuf = alloc_pathbuf();
    if (newPathBuf == NULL)
    {
        free_pathbuf(oldPathBuf);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_MEMORY;
    }
    SceIoIob *iob;
    int ret = alloc_iob(&iob, 0);
    if (ret >= 0)
    {
        SceIoDeviceEntry *olddev;
        int oldi_unit;
        char *oldrealPath = oldPathBuf;
        const char *targetPath = newname;
        int oldDevType = resolvePathUser(oldname, &olddev, &oldi_unit, &oldrealPath);
        ret = oldDevType;
        if (ret >= 0)
        {
            if (strchr(newname, ':') == 0)
            {
                if (strlen(newname) >= 1023)
                    ret = SCE_ERROR_KERNEL_FILENAME_TOO_LONG;
            }
            else
            {
                SceIoDeviceEntry *newdev;
                int newi_unit;
                char *newrealPath = newPathBuf;
                ret = resolvePathUser(newname, &newdev, &newi_unit, &newrealPath);
                if (ret >= 0)
                {
                    if (newdev != olddev || newi_unit != oldi_unit)
                        ret = SCE_ERROR_KERNEL_XDEV;
                    else
                        targetPath = newrealPath;
                }
            }
        }
        if (ret >= 0)
            ret = init_iob(iob, oldDevType, olddev, 0x1000000, oldi_unit);
        if (ret >= 0)
        {
            ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
            if (olddev->d_dp->dt_func->df_rename != NULL)
                ret = olddev->d_dp->dt_func->df_rename(iob, oldrealPath, targetPath);
        }
        free_iob(iob);
    }

    if (oldPathBuf != NULL)
        free_pathbuf(oldPathBuf);
    if (newPathBuf != NULL)
        free_pathbuf(newPathBuf);
    pspSetK1(oldK1);
    return ret;
}

int sceIoDevctl(const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    dbg_printf("%s(dev = %s, cmd = %08x)\n", __FUNCTION__, dev, cmd);
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(dev) || !pspK1DynBufOk(indata, inlen) || !pspK1DynBufOk(outdata, outlen))
    {
        // 1A9C
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    if (pspK1IsUserMode() && ((cmd >> 15) & 1) != 0)
    {
        // 1A90
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }
    // 1A74
    int ret = do_devctl(dev, cmd, indata, inlen, outdata, outlen);
    pspSetK1(oldK1);
    return ret;
}

int sceIoAssign(const char *dev, const char *blockDev, const char *fs, int mode, void* mountArg, int mountArgLen)
{
    dbg_init(0, FB_NONE, FAT_NONE);
    dbg_printf("sceIoAssign(%s, %s, %s, ...)\n", dev, blockDev, fs);
    char outDev[32];
    char blkAliasName[32];
    char fsAliasName[32];
    int i_unit;
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(dev) || !pspK1PtrOk(blockDev) || !pspK1PtrOk(fs) || !pspK1DynBufOk(mountArg, mountArgLen))
    {
        // 1FE8
        pspSetK1(oldK1);
    	dbg_init(0, FB_NONE, FAT_NOINIT);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    if (dev == NULL || (blockDev == NULL && fs != NULL))
    {
        // 1F44
        pspSetK1(oldK1);
    	dbg_init(0, FB_NONE, FAT_NOINIT);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    if (blockDev == NULL && fs == NULL)
        blockDev = "dummy_drv_iofile:";
    if (findAliasByDev(dev) != NULL)
    {
        // 1FD8
        pspSetK1(oldK1);
    	dbg_init(0, FB_NONE, FAT_NOINIT);
        return SCE_ERROR_KERNEL_ALIAS_USED;
    }
    if (fs != NULL && (~mode >> 31) != 0)
    {
        // 1F98
        parsedev(fs, outDev, &i_unit);
        SceIoAlias *alias = lookup_alias_tbl(outDev);
        if (alias != NULL) {
            snprintf(fsAliasName, 32, "%s%d:", alias->blockDev, alias->i_unit);
            fs = fsAliasName;
        }
    }
    // 1B78
    SceIoAlias *blkAlias = NULL;
    if (blockDev != NULL && (~mode >> 31) != 0)
    {
        // 1F50
        parsedev(blockDev, outDev, &i_unit);
        blkAlias = lookup_alias_tbl(outDev);
        if (blkAlias != NULL) {
            snprintf(blkAliasName, 32, "%s%d:", blkAlias->blockDev, blkAlias->i_unit);
            blockDev = blkAliasName;
        }
    }
    // 1B8C
    const char *curDev = blockDev;
    int attr = 0x20;
    int fsset = 0;
    if (fs != NULL)
    {
        fsset = 1;
        curDev = fs;
        attr = 0x10;
    }
    // 1BAC
    parsedev(curDev, outDev, &i_unit);
    SceIoDeviceList *list = lookup_device_list(outDev);
    if (list == NULL)
    {
        // 1F44
        pspSetK1(oldK1);
    	dbg_init(0, FB_NONE, FAT_NOINIT);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    if (blkAlias != NULL)
        attr |= 0x100;
    attr |= (((list->arg.d_dp->dt_type >> 24) & 0xF) << 24) | (((list->arg.d_dp->dt_type >> 16) & 0xFF) << 16);
    SceIoAlias *newAlias = alloc_alias_tbl();
    if (newAlias == NULL)
    {
        // 1F34
        pspSetK1(oldK1);
    	dbg_init(0, FB_NONE, FAT_NOINIT);
        return SCE_ERROR_KERNEL_TOO_MANY_OPEN_FILES;
    }
    newAlias->attr = attr;
    newAlias->dev = &list->arg;
    newAlias->i_unit = i_unit;
    int ret = SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    do
    {
        while (*dev == ' ')
            dev++;
        char *colon = strchr(dev, ':');
        if (colon == NULL || colon - dev >= 31)
            break;
        strncpy(newAlias->alias, dev, colon - dev);
        newAlias->alias[colon - dev] = '\0';
        strncpy(newAlias->blockDev, curDev, 31);
        newAlias->blockDev[31] = '\0';
        while (*blockDev == ' ')
            blockDev++;
        colon = strchr(blockDev, ':');
        if (colon == NULL || colon - blockDev >= 31)
            break;
        for (;;)
        {
            if (colon == blockDev)
                break;
            if ((look_ctype_table(*colon) & 4) != 0)
                break;
            colon--;
        }
        strncpy(newAlias->fs, blockDev, colon - blockDev);
        newAlias->fs[colon - blockDev] = '\0';
        if (blkAlias == NULL)
        {
            if (strcmp_bs("disc0", newAlias->alias) != 0)
            {
                if (strcmp_bs("ms0", newAlias->alias) != 0)
                {
                    if (strcmp_bs("flash0", newAlias->alias) == 0
                     || strcmp_bs("flash1", newAlias->alias) == 0
                     || strcmp_bs("flash2", newAlias->alias) == 0
                     || strcmp_bs("flash3", newAlias->alias) == 0)
                    {
                        if ((newAlias->attr & 0xFF) != 0x10 || strcmp_bs("lflash", newAlias->fs) != 0)
                            break;
                    }
                }
                else
                {
                    if ((newAlias->attr & 0xFF) != 0x10)
                        break;
                    if (strcmp_bs("msstor0p", newAlias->fs) != 0 && strcmp_bs("msstor", newAlias->fs) != 0 && strcmp_bs("eflash0a0f0p", newAlias->fs) != 0)
                        break;
                }
            }
            else
            {
                if ((newAlias->attr & 0xFF) != 0x10 || strcmp_bs("emu", newAlias->fs) != 0)
                    break;
            }
        }
        ret = alloc_iob(&iob, 0);
        if (ret < 0)
            break;
        ret = init_iob(iob, attr, &list->arg, 0x1000000, i_unit);
        if (ret >= 0 && fsset)
        {
            ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
            if (list->arg.d_dp->dt_func->df_mount != NULL)
                ret = list->arg.d_dp->dt_func->df_mount(iob, fs, blockDev, mode, mountArg, mountArgLen);
        }
        if (ret < 0)
        {
            free_iob(iob);
            break;
        }
        add_alias_tbl(newAlias);
        free_iob(iob);
        pspSetK1(oldK1);
        dbg_init(0, FB_NONE, FAT_NOINIT);
        return 0;
    } while (0);

    free_alias_tbl(newAlias);
    pspSetK1(oldK1);
    dbg_init(0, FB_NONE, FAT_NOINIT);
    return ret;
}

int sceIoUnassign(const char *dev)
{
    dbg_printf("sceIoUnassign(%s)\n", dev);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(dev))
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    if (dev == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    SceIoAlias *alias = findAliasByDev(dev);
    if (alias == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    int ret = alloc_iob(&iob, 0);
    if (ret < 0)
    {
        pspSetK1(oldK1);
        return ret;
    }
    ret = init_iob(iob, alias->attr, alias->dev, 0x1000000, alias->i_unit);
    if (ret >= 0 && (alias->attr & 0xFF) == 0x10)
    {
        ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
        if (alias->dev->d_dp->dt_func->df_umount != 0)
            ret = alias->dev->d_dp->dt_func->df_umount(iob, alias->blockDev);
    }
    if (ret < 0)
    {
        free_iob(iob);
        pspSetK1(oldK1);
        dbg_printf("-> error\n");
        return ret;
    }
    if (delete_alias_tbl(alias) != 0)
    {
        free_iob(iob);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    free_alias_tbl(alias);
    free_iob(iob);
    pspSetK1(oldK1);
    dbg_printf("-> unassign ok\n");
    return 0;
}

int IoFileMgrForKernel_E5323C5B(const char *aliasName, const char *blockDev)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    int i_unit;
    char name[32];
    if (!pspK1PtrOk(aliasName) || !pspK1PtrOk(blockDev))
    {
        // 22C0
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    if (aliasName == NULL)
    {
        // 22A4
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    if (findAliasByName(aliasName) != NULL)
    {
        // 22AC
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ALIAS_USED;
    }
    parsedev(blockDev, name, &i_unit);
    if (lookup_device_list(name) == NULL)
    {
        // 22A4
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    SceIoAlias *alias = alloc_alias_tbl();
    if (alias == NULL)
    {
        // 2294
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_TOO_MANY_OPEN_FILES;
    }
    alias->attr = 0x100;
    alias->dev = NULL;
    alias->i_unit = i_unit;
    // 2278, 2280
    while (*aliasName == ' ')
        aliasName++;
    // 21F0
    char *colon = strchr(aliasName, ':');
    int devLen = colon - aliasName;
    if (colon == NULL || devLen >= 31)
    {
        // 220C
        free_alias_tbl(alias);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    // 223C
    strncpy(alias->alias, aliasName, devLen);
    alias->alias[devLen] = '\0';
    strncpy(alias->blockDev, name, 31);
    alias->blockDev[31] = '\0';
    add_alias_tbl(alias);
    pspSetK1(oldK1);
    return 0;
}

int IoFileMgrForKernel_E972F70B(const char *name)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(name)) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    if (name == NULL) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    SceIoAlias *alias = findAliasByName(name);
    if (alias == NULL) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    if (delete_alias_tbl(alias) != 0) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    free_alias_tbl(alias);
    pspSetK1(oldK1);
    return 0;
}

int sceIoChangeThreadCwd(SceUID threadId, const char *path)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoDeviceEntry *dev;
    int i_unit;
    char *realPath;
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(path))
    {
        // 2494
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    char *buf = alloc_pathbuf();
    if (buf == NULL)
    {
        // 2484
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_MEMORY;
    }
    realPath = buf;
    int ret = resolvePathUser(path, &dev, &i_unit, &realPath);
    if (ret >= 0)
    {
        char **cwdSlot = (char **)sceKernelGetThreadKTLS(g_ktls, threadId, 1);
        if (cwdSlot == NULL)
            ret = SCE_ERROR_KERNEL_NO_MEMORY;
        else
        {
            if (*cwdSlot != NULL)
                sceKernelFreeHeapMemory(g_heap, *cwdSlot);
            int len = strlen(realPath) + 1;
            char *heap = sceKernelAllocHeapMemory(g_heap, len);
            *cwdSlot = heap;
            if (heap == NULL)
                ret = SCE_ERROR_KERNEL_NO_MEMORY;
            else
            {
                memcpy(heap, realPath, len);
                ret = 0;
            }
        }
    }
    if (buf != NULL)
        free_pathbuf(buf);
    pspSetK1(oldK1);
    return ret;
}

int sceIoCancel(int fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    int ret = validate_fd(fileDesc, 0, 8, 1, &iob);
    if (ret < 0)
    {
        // 2588
        pspSetK1(oldK1);
        return ret;
    }
    if (iob->i_de->d_dp->dt_func->df_cancel == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
    }
    ret = iob->i_de->d_dp->dt_func->df_cancel(iob);
    if (ret < 0)
    {
        // 2588
        pspSetK1(oldK1);
        return ret;
    }
    int oldIntr = sceKernelCpuSuspendIntr();
    if (iob->asyncThread != 0)
    {
        // 2554
        sceKernelTerminateDeleteThread(iob->asyncThread);
        sceKernelDeleteSema(iob->asyncSema);
        sceKernelDeleteEventFlag(iob->asyncEvFlag);
        iob->asyncThread = 0;
        iob->asyncEvFlag = 0;
        iob->asyncSema = 0;
    }
    // 252C
    sceKernelCpuResumeIntr(oldIntr);
    pspSetK1(oldK1);
    return 0;
}

int sceIoGetFdList(SceUID *fds, int numFd, int *count)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    if (!pspK1DynBufOk(fds, numFd * 4) || !pspK1StaBufOk(count, 4))
    {
        // 25F0
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    // 261C
    int stored = 0;
    int oldIntr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cur = g_uid_type->PARENT0;
    int curCount = 0;
    // 2648
    while (cur != g_uid_type)
    {
        u8 perm = cur->attr;
        if (!pspK1IsUserMode())
            perm = 0xFF;
        if (perm != 0)
        {
            // 2680
            curCount++;
            if (stored < numFd) {
                stored++;
                *(fds++) = UID_CB_TO_DATA(cur, g_uid_type, SceIoIob)->fileDesc;
            }
        }
        // 2658
        cur = cur->PARENT0;
    }
    // 2664
    if (count != NULL)
        *count = curCount;
    // 266C
    sceKernelCpuResumeIntr(oldIntr);
    pspSetK1(oldK1);
    return stored;
}

int sceIoGetFdDebugInfo(int fileDesc, SceIoFdDebugInfo *outInfo)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoFdDebugInfo info;
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    if (!pspK1StaBufOk(outInfo, 88))
    {
        // 2864
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    int oldIntr = sceKernelCpuSuspendIntr();
    int ret = validate_fd(fileDesc, 0, 0xFF, 31, &iob);
    if (ret < 0)
    {
        // 2850
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return ret;
    }
    // 2718
    int i;
    for (i = 0; i < (int)(sizeof(info) / sizeof(int)); i++)
        ((int *)&info)[i] = 0;
    info.size = 88;
    char *name = UID_DATA_TO_CB(iob, g_uid_type)->name;
    if (name != NULL) {
        // 2840
        strncpy(info.name, name, 31);
    }
    // 2750
    info.attribute = UID_DATA_TO_CB(iob, g_uid_type)->attr;
    info.flags = iob->i_flgs;
    if (iob->i_de != NULL)
        info.drvName = iob->i_de->d_dp->dt_string;
    // 2774
    info.fsNum = iob->i_unit;
    int isAsync = 0;
    info.newPath = iob->newPath;
    info.curThread = iob->curThread;
    info.retAddr = iob->retAddr;
    if (iob->asyncThread != 0)
    {
        // 2814
        if (sceKernelPollEventFlag(iob->asyncEvFlag, 1, 1, 0) == 0)
        {
            // 2838
            isAsync = 1;
        }
        else
            isAsync = 0;
    }
    // 27A0
    info.isAsync = isAsync;
    if (isAsync != 0) {
        info.asyncThread = iob->asyncThread;
        info.asyncCmd = iob->asyncCmd;
    }
    // 27B8
    int size = outInfo->size;
    if (info.size < outInfo->size)
        size = info.size;
    info.fpos = iob->i_fpos;
    info.thread = iob->i_thread;
    info.iob = iob;
    memcpy(outInfo, &info, size);
    sceKernelCpuResumeIntr(oldIntr);
    pspSetK1(oldK1);
    return 0;
}

int sceIoAddDrv(SceIoDeviceTable *drv)
{
    dbg_printf("sceIoAddDrv(): %s\n", drv->dt_string);
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(drv))
    {
        // 2954
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    if (lookup_device_list(drv->dt_string) != 0)
    {
        // 2948
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_REGDEV;
    }
    SceIoDeviceList *list = alloc_device_list();
    if (list == NULL)
    {
        // 2938
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_MEMORY;
    }
    list->arg.d_dp = drv;
    list->arg.d_userfd_count = 0;
    if (drv->dt_func->df_init == NULL) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
    }
    // 2904
    int ret = drv->dt_func->df_init(&list->arg);
    dbg_printf("function ioinit at %08x\n", drv->dt_func->df_init);
    if (ret < 0)
    {
        // 2928
        free_device_list(list);
        pspSetK1(oldK1);
        return ret;
    }
    add_device_list(list);
    pspSetK1(oldK1);
    dbg_printf(" -> adddrv ok\n");
    return 0;
}

int sceIoDelDrv(const char *drv)
{
    dbg_printf("sceIoDelDrv(%s)\n", drv);
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(drv)) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    SceIoDeviceList *dev = lookup_device_list(drv);
    if (dev == NULL) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    if (dev->arg.d_dp->dt_func->df_exit == NULL) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
    }
    // 29F4
    delete_device_list(dev);
    int ret = dev->arg.d_dp->dt_func->df_exit(&dev->arg);
    if (ret < 0)
    {
        // 2A34
        add_device_list(dev);
        pspSetK1(oldK1);
        return ret;
    }
    do_deldrv(&dev->arg);
    free_device_list(dev);
    pspSetK1(oldK1);
    return 0;
}

// 2A4C
SceIoDeviceList *lookup_device_list(const char *drive)
{
    dbg_printf("lookup_device_list(%s)\n", drive);
    int oldIntr = sceKernelCpuSuspendIntr();
    SceIoDeviceList *cur = g_devList;
    // 2A7C
    while (cur != NULL)
    {
        if (strcmp_bs(drive, cur->arg.d_dp->dt_string) == 0)
        {
            // 2AC0
            sceKernelCpuResumeIntr(oldIntr);
            return cur;
        }
        cur = cur->next;
    }
    // 2A9C
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

void add_device_list(SceIoDeviceList *list)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldIntr = sceKernelCpuSuspendIntr();
    dbg_printf("1\n");
    list->next = g_devList;
    dbg_printf("2\n");
    g_devList = list;
    dbg_printf("3\n");
    sceKernelCpuResumeIntr(oldIntr);
}

int delete_device_list(SceIoDeviceList *list)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldIntr = sceKernelCpuSuspendIntr();
    SceIoDeviceList *cur = g_devList;
    SceIoDeviceList *prev = (SceIoDeviceList*)&g_devList;
    // 2B34
    while (cur != NULL)
    {
        if (cur == list)
        {
            // 2B68
            prev->next = cur->next;
            sceKernelCpuResumeIntr(oldIntr);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    // 2B4C
    sceKernelCpuResumeIntr(oldIntr);
    return -1;
}

SceIoDeviceList *alloc_device_list(void)
{
    dbg_printf("alloc_device_list()\n");
    SceIoDeviceList *list = sceKernelAllocHeapMemory(g_heap, sizeof(SceIoDeviceList));
    if (list != NULL)
    {
        dbg_printf(" -> ok\n");
        list->next = NULL;
        list->arg.d_dp = NULL;
        list->arg.d_private = NULL;
        list->arg.d_userfd_count = 0;
    }
    return list;
}

void free_device_list(SceIoDeviceList *list)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    sceKernelFreeHeapMemory(g_heap, list);
}

void add_alias_tbl(SceIoAlias *alias)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldIntr = sceKernelCpuSuspendIntr();
    alias->next = g_aliasList;
    g_aliasList = alias;
    sceKernelCpuResumeIntr(oldIntr);
}

int delete_alias_tbl(SceIoAlias *alias)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldIntr = sceKernelCpuSuspendIntr();
    SceIoAlias *cur = g_aliasList;
    SceIoAlias *prev = (SceIoAlias*)&g_aliasList;
    // 2C3C
    while (cur != NULL)
    {
        if (cur == alias)
        {
            // 2C70
            prev->next = cur->next;
            sceKernelCpuResumeIntr(oldIntr);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    sceKernelCpuResumeIntr(oldIntr);
    return -1;
}

SceIoAlias *alloc_alias_tbl(void)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    void *buf = sceKernelAllocHeapMemory(g_heap, sizeof(SceIoAlias));
    if (buf != NULL)
    {
        int *intbuf = buf;
        // 2CA4
        while (intbuf != buf + sizeof(SceIoAlias))
            *(intbuf++) = 0;
    }
    return buf;
}

void free_alias_tbl(SceIoAlias *alias)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    sceKernelFreeHeapMemory(g_heap, alias);
}

// 2CE4
SceIoAlias *lookup_alias_tbl(char *drive)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldIntr = sceKernelCpuSuspendIntr();
    SceIoAlias *curAlias = g_aliasList;
    // 2D14
    while (curAlias != NULL)
    {
        if (curAlias->dev == NULL && strcmp_bs(drive, curAlias->alias) == 0) { // 2D50
            sceKernelCpuResumeIntr(oldIntr);
            return curAlias;
        }
        curAlias = curAlias->next;
        // 2D24
    }
    // 2D2C
    sceKernelCpuResumeIntr(oldIntr);
    return NULL;
}

int preobe_fdhook(SceIoIob *iob, char *file, int flags, SceMode mode)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoHookList *cur = g_hookList;
    iob->hook.iob = iob;
    // 2DB8
    while (cur != NULL)
    {
        iob->hook.arg = &cur->arg;
        if (cur->arg.hook->funcs->Preobe(&iob->hook, file, flags, mode) == 1)
        {
            // 2E24
            iob->hook.funcs = iob->hook.iob->i_de->d_dp->dt_func;
            return 1;
        }
        cur = cur->next;
    }
    // 2DF0
    iob->hook.funcs = NULL;
    iob->hook.arg = NULL;
    return 0;
}

int sceIoGetUID(int fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (fileDesc < 0)
        return SCE_ERROR_KERNEL_BAD_FILE_DESCRIPTOR;
    if (fileDesc >= 64)
        return fileDesc;
    SceUID uid = g_UIDs[fileDesc];
    if (uid == 0)
        return SCE_ERROR_KERNEL_BAD_FILE_DESCRIPTOR;
    return uid;
}

static inline int reportBadFd(int fileDesc, int ctrlFlags)
{
    if (fileDesc < 0 || (ctrlFlags & 4) == 0)
    {
        if (sceKernelIsIntrContext() != 0)
            Kprintf("bad file descriptor fd=0x%08X\n", fileDesc);
        else
            Kprintf("bad file descriptor fd=0x%08X, thid=0x%08X\n", fileDesc, sceKernelGetThreadId());
    }
    return SCE_ERROR_KERNEL_BAD_FILE_DESCRIPTOR;
}

int validate_fd(int fileDesc, int reqFlags, int permMask, int ctrlFlags, SceIoIob **outIob)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceUID id = fileDesc;
    if (fileDesc < 0)
        return reportBadFd(fileDesc, ctrlFlags);
    if (fileDesc < 64)
    {
        id = g_UIDs[fileDesc];
        if (id == 0)
            return reportBadFd(fileDesc, ctrlFlags);
    }
    SceSysmemUidCB *block;
    if (sceKernelGetUIDcontrolBlockWithType(id, g_uid_type, &block) != 0)
        return reportBadFd(fileDesc, ctrlFlags);
    SceIoIob *iob = UID_CB_TO_DATA(block, g_uid_type, SceIoIob);
    if ((ctrlFlags & 0x10) == 0 && sceKernelIsIntrContext() != 0)
        return SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((ctrlFlags & 2) == 0 && (iob->i_de == &deleted_device || iob->i_de == NULL))
        return g_deleted_error;
    if ((ctrlFlags & 1) == 0 && iob->asyncThread != 0 && sceKernelPollEventFlag(iob->asyncEvFlag, 1, 1, 0) == 0)
    {
        if ((ctrlFlags & 4) == 0)
            Kprintf("fd 0x%08X: Async mode BUSY\n", fileDesc);
        return SCE_ERROR_KERNEL_ASYNC_BUSY;
    }
    if (reqFlags != 0 && (iob->i_flgs & reqFlags) == 0)
        return reportBadFd(fileDesc, ctrlFlags);
    if ((ctrlFlags & 8) == 0 && (iob->i_flgs & 0x1000000) != 0 && (ctrlFlags & 0x10) == 0)
    {
        int thread = sceKernelGetThreadId();
        if (thread != iob->curThread && thread != iob->asyncThread)
            return reportBadFd(fileDesc, ctrlFlags);
    }
    int flag = block->attr & permMask;
    if (pspK1IsUserMode() == 0)
        flag = permMask;
    if (flag == 0)
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    if ((ctrlFlags & 0x10) == 0 && sceKernelGetUserLevel() < ((iob->d_type >> 24) & 0xF))
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    if (pspK1IsUserMode() && (iob->i_flgs & 0x4000000) != 0)
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    *outIob = iob;
    return 0;
}

// 3114
int alloc_iob(SceIoIob **outIob, int allocLowFd)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (sceKernelIsIntrContext() != 0)
        return SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    int oldIntr = sceKernelCpuSuspendIntr();
    if (allocLowFd != 0 && !pspK1IsUserMode())
        allocLowFd = 0;
    SceUID *ptr = g_UIDs;
    int count = 0;
    if (allocLowFd != 0)
    {
        for (count = 0; count < 64 && *ptr != 0; count++)
            ptr++;

        if (count >= 64)
        {
            sceKernelCpuResumeIntr(oldIntr);
            return SCE_ERROR_KERNEL_TOO_MANY_OPEN_FILES;
        }
    }
    int lvl = sceKernelGetUserLevel();
    char usrMode = pspK1IsUserMode();
    if (usrMode && (lvl < 4))
    {
        if (g_iobCount >= 64)
        {
            sceKernelCpuResumeIntr(oldIntr);
            return SCE_ERROR_KERNEL_TOO_MANY_OPEN_FILES;
        }
        g_iobCount++;
    }
    SceSysmemUidCB *blk;
    int ret = sceKernelCreateUID(g_uid_type, "Iob", (pspK1IsUserMode() == 1 ? 0xFF : 0), &blk);
    if (ret == 0)
    {
        SceIoIob *iob = UID_CB_TO_DATA(blk, g_uid_type, SceIoIob);
        if (allocLowFd == 0)
            ret = blk->uid ^ g_uidXor;
        else
        {
            ret = count;
            *ptr = blk->uid ^ g_uidXor;
        }
        *outIob = iob;
        iob->asyncPrio = default_thread_priority;
        iob->fileDesc = ret;
        iob->i_de = &deleted_device;
        iob->curThread = sceKernelGetThreadId();
        iob->userMode = usrMode;
        iob->userLevel = lvl;
        iob->powerLocked = 0;
    }
    sceKernelCpuResumeIntr(oldIntr);
    return ret;
}

// 32B4
int free_iob(SceIoIob *iob)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    iob_power_unlock(iob);
    int oldIntr = sceKernelCpuSuspendIntr();
    SceUID thId = iob->asyncThread;
    if (thId != 0)
    {
        // 33A8
        sceKernelTerminateDeleteThread(thId);
        sceKernelDeleteSema(iob->asyncSema);
        sceKernelDeleteEventFlag(iob->asyncEvFlag);
        iob->asyncEvFlag = 0;
        iob->asyncSema = 0;
        iob->asyncThread = 0;
    }
    // 32E0
    iob->i_flgs = 0;
    if (iob->userMode != 0 && iob->userLevel < 4)
    {
        g_iobCount--;
        if ((iob->i_de->d_dp->dt_type & 0x00FF0000) != 0)
            iob->i_de->d_userfd_count--;
    }
    // (3328)
    // 332C
    iob->i_de = &deleted_device;
    if (iob->newPath != NULL)
    {
        // 3398
        free_pathbuf(iob->newPath);
        iob->newPath = NULL;
    }
    // 333C
    SceUID fileId = iob->fileDesc;;
    if (fileId < 64)
        g_UIDs[fileId] = 0; // contains u32s
    // 3360
    sceKernelDeleteUID(UID_DATA_TO_CB(iob, g_uid_type)->uid ^ g_uidXor);
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

// 33D0
int init_iob(SceIoIob *iob, int devType, SceIoDeviceEntry *dev, int flags, int unit)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (iob->userLevel < ((devType >> 24) & 0xF))
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    if (iob->userMode != 0 && iob->userLevel < 4)
    {
        int count = (dev->d_dp->dt_type & 0x00FF0000) >> 16;
        if (count != 0 && dev->d_userfd_count >= count)
            return SCE_ERROR_KERNEL_TOO_MANY_OPEN_FILES;
        // 3420
        dev->d_userfd_count++;
    }
    // (3428)
    iob->i_unit = unit;
    // 342C
    iob->d_type = devType;
    iob->i_de = dev;
    iob->i_flgs = flags;
    return 0;
}

// 3444
int iob_power_lock(SceIoIob *iob)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (iob->powerLocked == 0)
    {
        iob->powerLocked = 1;
        if (iob->userMode == 0)
            sceKernelPowerLock(0);
        else
            sceKernelPowerLockForUser(0);
    }
    return 0;
}

// 3494
int iob_power_unlock(SceIoIob *iob)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (iob->powerLocked)
    {
        iob->powerLocked = 0;
        if (iob->userMode == 0)
            sceKernelPowerUnlock(0);
        else
            sceKernelPowerUnlockForUser(0);
    }
    return 0;
}

char *parsedev(const char *dev, char *outDev, int *i_unit)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    // 35B4, 35BC
    while (*dev == ' ')
        dev++;
    // 351C
    char *colon = strchr(dev, ':');
    if (colon == NULL)
        return 0;
    int devLen = colon - dev;
    if (devLen >= 31)
        return 0;
    // 3560
    strncpy(outDev, dev, devLen);
    outDev[devLen] = '\0';
    char *end = &outDev[devLen];
    // 3574
    for (;;)
    {
        if (end == outDev)
            break;
        if ((look_ctype_table(end[-1]) & 4) == 0)
            break;
        end--;
    }
    // 3598
    *i_unit = strtol(end, 0, 10);
    *end = '\0';
    return colon + 1;
}

SceIoAlias *findAliasByDev(const char *dev)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    // 36A8, 36B0
    while (*dev == ' ')
        dev++;
    // 35F8
    char *colonStr = strchr(dev, ':');
    int colon = colonStr - dev;
    char buf[32];
    if (colonStr != NULL && colon < 31)
    {
        // 3630
        strncpy(buf, dev, colon);
        buf[colon] = '\0';
        int oldIntr = sceKernelCpuSuspendIntr();
        SceIoAlias *curAlias = g_aliasList;
        // 3660
        while (curAlias != NULL)
        {
            if (curAlias->dev != NULL && strcmp_bs(buf, curAlias->alias) == 0) { // 3688
                sceKernelCpuResumeIntr(oldIntr);
                return curAlias;
            }
            curAlias = curAlias->next;
            // 3670
        }
        // 3678
        sceKernelCpuResumeIntr(oldIntr);
    }
    return 0;
}

SceIoAlias *findAliasByName(const char *name)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    char buf[32];
    // 3740, 3748
    while (*name == ' ')
        name++;
    // 36E8
    char *colon = strchr(name, ':');
    int len = colon - name;
    if (colon == NULL || len >= 31)
        return NULL;
    strncpy(buf, name, len);
    buf[len] = '\0';
    return lookup_alias_tbl(buf);
}

int resolvePathUser(const char *path, SceIoDeviceEntry **dev, int *i_unit, char **dirNamePtr)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return resolvePath(path, dev, i_unit, dirNamePtr, 1);
}

int resolvePath(const char *path, SceIoDeviceEntry **dev, int *i_unit, char **dirNamePtr, int userMode)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    char drive[32];
    if (path[0] == ' ')
    {
        // 3A54
        // 3A5C
        while (*(++path) == ' ')
            ;
    }
    // 37C8
    int pathLen = strlen(path);
    char *dirName = *dirNamePtr;
    int dirLen = 0;
    if (strchr(path, ':') == NULL)
    {
        // 39BC
        void *ktls = sceKernelGetKTLS(g_ktls);
        if (ktls == NULL || *(char**)ktls == NULL) {
            // 39DC
            return SCE_ERROR_KERNEL_NOCWD;
        }
        // 39E8
        dirLen = strlen(*(char**)ktls);
        memcpy(dirName, *(char**)ktls, dirLen);
        dirName += dirLen;
        if (*(dirName - 1) == '/')
        {
            // 3A3C
            if (*path == *(dirName - 1))
            {
                path++;
                pathLen--;
            }
        }
        else if (*(dirName - 1) != ':' && *path != '/')
        {
            *dirName = '/';
            dirLen++;
            dirName++;
        }
    }
    // 37F4
    if (dirLen + pathLen >= 1023)
        return SCE_ERROR_KERNEL_FILENAME_TOO_LONG;
    memcpy(dirName, path, pathLen);
    dirName[pathLen] = '\0';
    char *dirStart = *dirNamePtr;
    char *colon = strchr(dirStart, ':');
    if (colon == NULL)
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    int colonOff = colon - dirStart;
    if (colonOff >= 31)
        return SCE_ERROR_KERNEL_FILENAME_TOO_LONG;
    memcpy(drive, dirStart, colonOff);
    char *curBuf = drive + colonOff;
    *curBuf = '\0';
    // 3868
    while (curBuf != drive)
    {
        if ((look_ctype_table(*(curBuf - 1)) & CTYPE_CIPHER) == 0)
            break;
        curBuf--;
    }
    // 388C
    *i_unit = strtol(curBuf, NULL, 10);
    SceIoAlias *aliasNoDrv = NULL;
    if (userMode != 0)
    {
        // 398C
        aliasNoDrv = lookup_alias_tbl(drive);
        if (aliasNoDrv != NULL) {
            *i_unit = aliasNoDrv->i_unit;
            strncpy(drive, aliasNoDrv->blockDev, 32);
        }
    }
    // 38A0
    int oldIntr = sceKernelCpuSuspendIntr();
    SceIoAlias *curAlias = g_aliasList;
  	dbg_printf("== listing all aliases ==\n");
  	while (curAlias != NULL) {
	  	dbg_printf("alias %s fs %s\n", curAlias->alias, curAlias->fs);
	  	curAlias = curAlias->next;
	}
	dbg_printf("== end ==\n");
	curAlias = g_aliasList;
    SceIoAlias *alias = NULL;
    // 38BC
    while (curAlias != NULL)
    {
    	dbg_printf(". alias %s, fs %s\n", curAlias->alias, curAlias->fs);
        if (curAlias->dev != NULL && strcmp_bs(drive, curAlias->alias) == 0) { // 396C
            alias = curAlias;
            break;
        }
        curAlias = curAlias->next;
        // 38CC
    }
    // 38D4
    sceKernelCpuResumeIntr(oldIntr);
    int ret;
    // 38E0
    if (alias == NULL)
    {
        // 3938
        if (aliasNoDrv == NULL)
            *curBuf = '\0';
        // 3940
        SceIoDeviceList *list = lookup_device_list(drive);
        if (list == NULL)
            return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
        ret = list->arg.d_dp->dt_type;
        *dev = &list->arg;
    }
    else
    {
        *i_unit = alias->i_unit;
        *dev = alias->dev;
        ret = alias->attr;
    }
    // 38FC
    *dirNamePtr = colon + 1;
    return ret;
}

// 3A70
int strcmp_bs(const char *lhs, const char *rhs)
{
    dbg_printf("strcmp_bs(%s, %s)\n", lhs, rhs);
    if (lhs == NULL || rhs == NULL)
    {
        if (lhs == rhs)
            return 0;
        if (lhs != NULL)
            return -1;
        return 1;
    }
    do
    {
        if (tolower(*lhs) != tolower(*rhs))
            return *lhs - *rhs;
        rhs++;
        lhs++;
    } while (*lhs != '\0');
    return 0;
}

// 3B10
void *alloc_pathbuf()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldIntr = sceKernelCpuSuspendIntr();
    char *path;
    if (g_pathbufCount <= 0) {
        // 3B74
        path = sceKernelAllocHeapMemory(g_heap, 1024);
    }
    else
        path = g_pathbufBuf[--g_pathbufCount];
    // 3B54
    sceKernelCpuResumeIntr(oldIntr);
    return path;
}

// 3B88
void free_pathbuf(void *ptr)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    sceKernelMemset32(ptr, 0, 1024);
    int oldIntr = sceKernelCpuSuspendIntr();
    if (g_pathbufCount < 2) {
        // 3BF4
        g_pathbufBuf[g_pathbufCount++] = ptr;
    }
    else
        sceKernelFreeHeapMemory(g_heap, ptr);
    // 3BD8
    sceKernelCpuResumeIntr(oldIntr);
}

int StdioInit(int, int);

int IoFileMgrInit()
{
    SceSysmemUidCB *in, *out, *err;
    dbg_init(1, FB_NONE, FAT_HARDWARE);
    dbg_printf("-- iofilemgr init\n");
    g_uidXor = sceKernelGetInitialRandomValue();
    g_heap = sceKernelCreateHeap(1, 0x2000, 1, "SceIofile");
    sceKernelCreateUIDtype("Iob", 0x90, IobFuncs, 0, &g_uid_type);
    g_ktls = sceKernelAllocateKTLS(4, (void*)free_cwd, 0);
    sceIoDelDrv("dummy_drv_iofile");
    sceIoAddDrv(&_nullcon_driver);
    StdioInit(0, 0);
    if (sceKernelGetUIDcontrolBlock(sceIoGetUID(sceKernelStdin()), &in) == 0)
        in->attr |= 5;
    if (sceKernelGetUIDcontrolBlock(sceIoGetUID(sceKernelStdout()), &out) == 0)
        out->attr |= 3;
    if (sceKernelGetUIDcontrolBlock(sceIoGetUID(sceKernelStderr()), &err) == 0)
        err->attr |= 3;
    g_UIDs[0] = sceKernelStdin();
    g_UIDs[1] = sceKernelStdout();
    g_UIDs[2] = sceKernelStderr();
    dbg_printf("-- init finished\n");
    return 0;
}

int IoFileMgrRebootBefore(void *rebootArg __attribute__((unused)), s32 rebootParam __attribute__((unused)), s32 rebootFlags __attribute__((unused)), s32 rebootOpt __attribute__((unused)))
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoDeviceList *cur = g_devList;
    // 3D7C
    while (cur != NULL)
    {
        if (cur->arg.d_dp->dt_func->df_exit != NULL) {
            // 3DC4
            cur->arg.d_dp->dt_func->df_exit(&cur->arg);
        }
        cur = cur->next;
        // 3D90
    }
    // 3D98
    sceKernelFreeKTLS(g_ktls);
    sceKernelDeleteHeap(g_heap);
    return 0;
}

int sceIoPollAsync(SceUID fileDesc, SceInt64 *res)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_get_async_stat(fileDesc, res, 1, 0, "SceIoPollAsync:");
}

int sceIoWaitAsync(SceUID fileDesc, SceInt64 *res)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_get_async_stat(fileDesc, res, 0, 0, "SceIoWaitAsync:");
}

int sceIoWaitAsyncCB(SceUID fileDesc, SceInt64 *res)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_get_async_stat(fileDesc, res, 0, 1, "SceIoWaitAsyncCB:");
}

int sceIoGetAsyncStat(SceUID fileDesc, int poll, SceInt64 *res)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_get_async_stat(fileDesc, res, poll, 0, "");
}

int sceIoSetAsyncCallback(SceUID fileDesc, SceUID cb, void *argp)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceSysmemUidCB *blk;
    SceIoIob *iob;
    if (sceKernelGetThreadmanIdType(cb) != 8)
        return SCE_ERROR_KERNEL_ILLEGAL_ARGUMENT;
    int oldK1 = pspShiftK1();
    if (sceKernelGetUIDcontrolBlock(cb, &blk) != 0)
    {
        // 3F4C
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }
    int flag = 2;
    if (pspK1IsUserMode())
        flag = blk->attr & 2;
    if (flag == 0)
    {
        // 3F4C
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }
    int ret = validate_fd(fileDesc, 0, 2, 1, &iob);
    pspSetK1(oldK1);
    if (ret < 0)
        return ret;
    iob->asyncCbArgp = argp;
    iob->asyncCb = cb;
    return 0;
}

int sceIoValidateFd(SceUID fileDesc, int permMask)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    SceIoIob *iob;
    int ret = validate_fd(fileDesc, 0, permMask, 0, &iob);
    pspSetK1(oldK1);
    return ret;
}

int sceIoClose(SceUID fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_close(fileDesc, 0, 1);
}

int sceIoCloseAsync(SceUID fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_close(fileDesc, 1, 0);
}

SceUID sceIoOpen(const char *file, int flags, SceMode mode)
{
    dbg_printf("sceIoOpen(%s, %08x, %08x)\n", file, flags, mode);
    int oldK1 = pspShiftK1();
    int retAddr = pspGetRa();
    int ret;
    if (pspK1IsUserMode() && (ret = sceKernelGetSyscallRA()) != 0)
        retAddr = ret;
    ret = do_open(file, flags, mode, 0, retAddr, oldK1);
    dbg_printf("-> open returned %d\n", ret);
    return ret;
}

SceUID sceIoOpenAsync(const char *file, int flags, SceMode mode)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    int retAddr = pspGetRa();
    int ret;
    if (pspK1IsUserMode() && (ret = sceKernelGetSyscallRA()) != 0)
        retAddr = ret;
    ret = do_open(file, flags, mode, 1, retAddr, oldK1);
    dbg_printf("-> open returned %d\n", ret);
    return ret;
}

int sceIoRead(SceUID fileDesc, void *data, SceSize size)
{
    dbg_printf("Calling %s(%d, ...)\n", __FUNCTION__, fileDesc);
    return do_read(fileDesc, data, size, 0);
}

int sceIoReadAsync(SceUID fileDesc, void *data, SceSize size)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_read(fileDesc, data, size, 1);
}

int sceIoWrite(SceUID fileDesc, const void *data, SceSize size)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_write(fileDesc, data, size, 0);
}

int sceIoWriteAsync(SceUID fileDesc, const void *data, SceSize size)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_write(fileDesc, data, size, 1);
}

SceOff sceIoLseek(SceUID fileDesc, SceOff offset, int whence)
{
    dbg_printf("Calling %s(%d, ...)\n", __FUNCTION__, fileDesc);
    return do_lseek(fileDesc, offset, whence, 0);
}

SceOff sceIoLseekAsync(SceUID fileDesc, SceOff offset, int whence)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_lseek(fileDesc, offset, whence, 1);
}

int sceIoLseek32(SceUID fileDesc, int offset, int whence)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_lseek(fileDesc, offset, whence, 0);
}

int sceIoLseek32Async(SceUID fileDesc, int offset, int whence)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_lseek(fileDesc, offset, whence, 1);
}

int sceIoIoctl(SceUID fileDesc, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_ioctl(fileDesc, cmd, indata, inlen, outdata, outlen, 0);
}

int sceIoIoctlAsync(SceUID fileDesc, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return do_ioctl(fileDesc, cmd, indata, inlen, outdata, outlen, 1);
}

int sceIoMkdir(const char *path, SceMode mode)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return xx_dir(path, mode, 1);
}

int sceIoRmdir(const char *path)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return xx_dir(path, 0, 2);
}

int sceIoChdir(const char *path)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return xx_dir(path, 0, 0);
}

int sceIoGetstat(const char *file, SceIoStat *stat)
{
    dbg_printf("sceIoGetstat(%s, %p)\n", file, stat);
    return xx_stat(file, stat, 0, 1);
}

int sceIoChstat(const char *file, SceIoStat *stat, int bits)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return xx_stat(file, stat, bits, 0);
}

int sceIoSync(const char *device, unsigned int flags)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int buf[4];
    buf[0] = flags;
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(device)) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    int ret = do_devctl(device, 256, buf, 4, 0, 0);
    pspSetK1(oldK1);
    return ret;
}

int sceIoGetDevType(SceUID fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    int ret = validate_fd(fileDesc, 0, 1, 0, &iob);
    pspSetK1(oldK1);
    if (ret < 0)
        return ret;
    return (u8)iob->d_type;
}

int sceIoGetThreadCwd(SceUID uid, char *dir, int len)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    if (!pspK1DynBufOk(dir, len))
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    int ret = 0;
    char **cwdSlot = (char **)sceKernelGetThreadKTLS(g_ktls, uid, 0);
    if (cwdSlot != NULL)
    {
        char *cwd = *cwdSlot;
        if (cwd != NULL)
        {
            ret = strlen(cwd) + 1;
            if ((u32)len >= (u32)ret)
                len = ret;
            strncpy(dir, *cwdSlot, len);
        }
    }
    pspSetK1(oldK1);
    return ret;
}

int sceIoTerminateFd(char *drive)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(drive)) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    SceIoDeviceList *dev = lookup_device_list(drive);
    if (dev == NULL) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_SUCH_DEVICE;
    }
    do_deldrv(&dev->arg);
    pspSetK1(oldK1);
    return 0;
}

int sceIoAddHook(SceIoHookType *hook)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(hook)) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    SceIoHookList *new = sceKernelAllocHeapMemory(g_heap, sizeof(SceIoHookList));
    if (new == NULL) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_MEMORY;
    }
    new->arg.hook = hook;
    new->next = g_hookList;
    g_hookList = new;
    new->arg.hook->funcs->Add(&new->arg.hook);
    pspSetK1(oldK1);
    return 0;
}

int sceIoGetIobUserLevel(SceIoIob *iob)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return iob->userLevel;
}

void free_cwd(void *ktls)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    char **cwdSlot = (char **)ktls;
    char *cwd = *cwdSlot;
    if (cwd != NULL)
    {
        sceKernelFreeHeapMemory(g_heap, cwd);
        *cwdSlot = NULL;
    }
}

int deleted_func()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return g_deleted_error;
}

int deleted_func_close()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return 0;
}

s64 deleted_func_offt()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return g_deleted_error;
}

int __attribute__((noinline)) start_async_op(SceIoIob *iob, int cmd)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int ret;
    if (iob->asyncThread == 0)
    {
        ret = create_async_thread(iob);
        if (ret < 0)
            return ret;
    }
    if (sceKernelPollSema(iob->asyncSema, 1) < 0)
        return SCE_ERROR_KERNEL_ASYNC_BUSY;
    if (iob->asyncPrio < 0 && sceKernelGetCompiledSdkVersion() >= 0x04020000)
    {
        int prio = 0;
        if (IoFileMgrForKernel_var_E6F7A8DF != 0 && default_thread_priority != g_savedAsyncPrio)
            prio = default_thread_priority;
        sceKernelChangeThreadPriority(iob->asyncThread, prio);
    }
    iob->asyncCmd = cmd;
    iob->k1 = pspGetK1();
    return sceKernelSetEventFlag(iob->asyncEvFlag, 3);
}

int __attribute__((noinline)) create_async_thread(SceIoIob *iob)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int ret;
    iob->asyncBusy = 0;
    int prio = iob->asyncPrio;
    if (prio < 0)
        prio = sceKernelGetThreadCurrentPriority();
    if (IoFileMgrForKernel_var_E6F7A8DF != 0)
    {
        g_savedAsyncPrio = prio;
        prio = default_thread_priority;
    }
    ret = sceKernelCreateThread("SceIofileAsync", async_loop, prio, 2048,
                                (sceKernelIsUserModeThread() == 0 ? 0x00100000 : 0x08100000), 0);
    if (ret < 0)
        return ret;
    iob->asyncThread = ret;
    ret = sceKernelCreateSema("SceIofileAsync", 0, 1, 1, 0);
    if (ret < 0)
    {
        sceKernelDeleteThread(iob->asyncThread);
        iob->asyncThread = 0;
        return ret;
    }
    iob->asyncSema = ret;
    ret = sceKernelCreateEventFlag("SceIofileAsync", 512, 0, 0);
    if (ret < 0)
    {
        sceKernelDeleteThread(iob->asyncThread);
        sceKernelDeleteSema(iob->asyncSema);
        iob->asyncThread = 0;
        iob->asyncSema = 0;
        return ret;
    }
    iob->asyncEvFlag = ret;
    ret = sceKernelStartThread(iob->asyncThread, 4, &iob);
    if (ret < 0)
    {
        sceKernelDeleteThread(iob->asyncThread);
        sceKernelDeleteSema(iob->asyncSema);
        sceKernelDeleteEventFlag(iob->asyncEvFlag);
        iob->asyncThread = 0;
        iob->asyncSema = 0;
        iob->asyncEvFlag = 0;
    }
    return ret;
}

static inline int reportNoAsyncOp(char *func __attribute__((unused)), SceUID fileDesc __attribute__((unused)), int oldK1)
{
    sceKernelIsIntrContext();
    pspSetK1(oldK1);
    return SCE_ERROR_KERNEL_NO_ASYNC_OP;
}

int do_get_async_stat(SceUID fileDesc, SceInt64 *res, int poll, int cb, char *func)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    if (sceKernelGetCompiledSdkVersion() >= 0x05070000 && res == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    if (!pspK1StaBufOk(res, 8))
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    int ret = validate_fd(fileDesc, 0, 4, 3, &iob);
    if (ret < 0)
    {
        pspSetK1(oldK1);
        return ret;
    }
    if (iob->asyncThread == 0)
        return reportNoAsyncOp(func, fileDesc, oldK1);
    u32 bits;
    ret = sceKernelPollEventFlag(iob->asyncEvFlag, 5, 32, &bits);
    if (ret != 0)
    {
        if ((bits & 1) == 0)
            return reportNoAsyncOp(func, fileDesc, oldK1);
        if (poll != 0)
        {
            pspSetK1(oldK1);
            return 1;
        }
        if (cb == 0)
            ret = sceKernelWaitEventFlag(iob->asyncEvFlag, 5, 32, &bits, NULL);
        else
            ret = sceKernelWaitEventFlagCB(iob->asyncEvFlag, 5, 32, &bits, NULL);
        if (ret < 0)
        {
            pspSetK1(oldK1);
            return ret;
        }
    }
    *res = iob->asyncRet;
    if (iob->asyncBusy == 0)
    {
        ret = sceKernelSignalSema(iob->asyncSema, 1);
        pspSetK1(oldK1);
        if (ret < 0)
            return ret;
    }
    else
    {
        free_iob(iob);
        pspSetK1(oldK1);
    }
    return 0;
}

int do_close(SceUID fileDesc, int async, int remove)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    int sdk = sceKernelGetCompiledSdkVersion();
    int ret = validate_fd(fileDesc, 0, (sdk >= 0x3050010) ? 0x10 : 0xFE, (async == 0) ? 3 : 2, &iob);
    if (ret < 0)
    {
        pspSetK1(oldK1);
        return ret;
    }
    if ((iob->i_flgs & 8) != 0)
    {
        Kprintf("bad file descriptor %d\n", fileDesc);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_BAD_FILE_DESCRIPTOR;
    }
    if (iob->i_de == &deleted_device && iob->hook.arg == NULL)
    {
        if (async == 0)
            ret = 0;
        else
            ret = start_async_op(iob, 0);
    }
    else if (iob->i_de->d_dp->dt_func->df_close == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
    }
    else if (async)
    {
        ret = start_async_op(iob, 2);
    }
    else
    {
        if (iob->asyncThread != 0)
        {
            ret = sceKernelPollSema(iob->asyncSema, 1);
            if (ret < 0 && sdk > 0x02060010)
            {
                pspSetK1(oldK1);
                return SCE_ERROR_KERNEL_ASYNC_BUSY;
            }
        }
        if (iob->hook.arg == NULL)
            ret = iob->i_de->d_dp->dt_func->df_close(iob);
        else
            ret = iob->hook.arg->hook->funcs->Close(&iob->hook);
        if (ret >= 0)
            ret = 0;
    }

    if (remove != 0 && ret >= 0)
        free_iob(iob);
    pspSetK1(oldK1);
    return ret;
}

// 4C04
int do_open(const char *path, int flags, SceMode mode, int async, int retAddr, int oldK1)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    if (!pspK1PtrOk(path))
    {
        // 4D2C
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    int ret = alloc_iob(&iob, (flags & 0x08000000) == 0);
    if (ret < 0)
    {
        // 4D24
        pspSetK1(oldK1);
        return ret;
    }
    sceKernelRenameUID(UID_DATA_TO_CB(iob, g_uid_type)->uid ^ g_uidXor, path);
    if (sceKernelDeci2pReferOperations() != 0)
    {
        // 4CF4
        char *newPath = alloc_pathbuf();
        if (newPath != NULL) {
            strncpy(newPath, path, 0x3FF);
            newPath[0x3FF] = '\0';
        }
        // 4D18
        iob->newPath = newPath;
    }
    // 4C94
    iob->retAddr = retAddr;
    ret = open_iob(iob, path, flags, mode, async);
    if (ret < 0) {
        // 4CE4
        free_iob(iob);
    }
    pspSetK1(oldK1);
    return ret;
}

int do_read(SceUID fileDesc, void *data, SceSize size, int async)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    if (!pspK1DynBufOk(data, size))
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    int ret = validate_fd(fileDesc, 1, 4, 0, &iob);
    if (ret < 0)
    {
        pspSetK1(oldK1);
        return ret;
    }
    if (iob->i_de->d_dp->dt_func->df_read == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
    }
    if (async)
    {
        iob->asyncArgs[0] = (int)data;
        iob->asyncArgs[1] = size;
        ret = start_async_op(iob, 3);
        pspSetK1(oldK1);
        return ret;
    }
    if (iob->hook.arg == NULL)
        ret = iob->i_de->d_dp->dt_func->df_read(iob, data, size);
    else
        ret = iob->hook.arg->hook->funcs->Read(&iob->hook, data, size);
    pspSetK1(oldK1);
    return ret;
}

int do_write(SceUID fileDesc, const void *data, SceSize size, int async)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    if (!pspK1DynBufOk(data, size))
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    int ret = validate_fd(fileDesc, 2, 2, 0, &iob);
    if (ret < 0)
    {
        pspSetK1(oldK1);
        return ret;
    }
    if (iob->i_de->d_dp->dt_func->df_write == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
    }
    if (async)
    {
        iob->asyncArgs[0] = (int)data;
        iob->asyncArgs[1] = size;
        ret = start_async_op(iob, 4);
        pspSetK1(oldK1);
        return ret;
    }
    if (iob->hook.arg == NULL)
        ret = iob->i_de->d_dp->dt_func->df_write(iob, data, size);
    else
        ret = iob->hook.arg->hook->funcs->Write(&iob->hook, data, size);
    pspSetK1(oldK1);
    return ret;
}

SceOff do_lseek(SceUID fileDesc, SceOff offset, int whence, int async)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    s64 ret = validate_fd(fileDesc, 0, 4, 0, &iob);
    if (ret < 0)
    {
        pspSetK1(oldK1);
        return ret;
    }
    if (whence < 0 || whence >= 3)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_INVALID_ARGUMENT;
    }
    if (iob->i_de->d_dp->dt_func->df_lseek == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
    }
    if (async)
    {
        iob->asyncArgs[0] = (int)offset;
        iob->asyncArgs[1] = (int)(offset >> 32);
        iob->asyncArgs[2] = whence;
        ret = start_async_op(iob, 5);
        pspSetK1(oldK1);
        return ret;
    }
    if (iob->hook.arg == NULL)
        ret = iob->i_de->d_dp->dt_func->df_lseek(iob, offset, whence);
    else
        ret = iob->hook.arg->hook->funcs->Lseek(&iob->hook, offset, whence);
    pspSetK1(oldK1);
    return ret;
}

int do_ioctl(SceUID fileDesc, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen, int async)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    if (!pspK1DynBufOk(indata, inlen) || !pspK1DynBufOk(outdata, outlen))
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    if (pspK1IsUserMode() && ((cmd >> 15) & 1) != 0)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }
    int ret = validate_fd(fileDesc, 0, 2, 0, &iob);
    if (ret < 0)
    {
        pspSetK1(oldK1);
        return ret;
    }
    if (iob->i_de->d_dp->dt_func->df_ioctl == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
    }
    if (async)
    {
        iob->asyncArgs[0] = cmd;
        iob->asyncArgs[1] = (int)indata;
        iob->asyncArgs[2] = inlen;
        iob->asyncArgs[3] = (int)outdata;
        iob->asyncArgs[4] = outlen;
        ret = start_async_op(iob, 6);
        pspSetK1(oldK1);
        return ret;
    }
    if (iob->hook.arg == NULL)
        ret = iob->i_de->d_dp->dt_func->df_ioctl(iob, cmd, indata, inlen, outdata, outlen);
    else
        ret = iob->hook.arg->hook->funcs->Ioctl(&iob->hook, cmd, indata, inlen, outdata, outlen);
    pspSetK1(oldK1);
    return ret;
}

// 5448
int xx_dir(const char *path, SceMode mode, int action) // action: 0 = chdir, 1 = mkdir, 2 = rmdir
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int ret;
    char str[32];
    SceIoDeviceEntry *dev;
    int i_unit;
    char *dirName;
    SceIoIob *iob;
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(path))
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    char *pathBuf = alloc_pathbuf();
    if (pathBuf == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_MEMORY;
    }
    ret = alloc_iob(&iob, 0);
    if (ret >= 0)
    {
        dirName = pathBuf;
        ret = resolvePathUser(path, &dev, &i_unit, &dirName);
        if (ret >= 0)
            ret = init_iob(iob, ret, dev, 0x1000000, i_unit);
        if (ret >= 0)
        {
            switch (action)
            {
            case 1: // mkdir
                if (dev->d_dp->dt_func->df_mkdir != NULL)
                    ret = dev->d_dp->dt_func->df_mkdir(iob, dirName, mode);
                else
                    ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
                break;
            case 0: // chdir
                snprintf(str, 32, "%s%d:", dev->d_dp->dt_string, i_unit);
                {
                    char **ptr = sceKernelGetKTLS(g_ktls);
                    if (ptr == NULL)
                        ret = SCE_ERROR_KERNEL_NO_MEMORY;
                    else
                    {
                        if (*ptr != NULL)
                            sceKernelFreeHeapMemory(g_heap, *ptr);
                        int size = strlen(dirName) + strlen(str) + 1;
                        char *curPath = sceKernelAllocHeapMemory(g_heap, size);
                        *ptr = curPath;
                        if (curPath != NULL)
                        {
                            snprintf(curPath, size, "%s%s", str, dirName);
                            ret = 0;
                        }
                        else
                            ret = SCE_ERROR_KERNEL_NO_MEMORY;
                    }
                }
                break;
            case 2: // rmdir
                if (dev->d_dp->dt_func->df_rmdir != NULL)
                    ret = dev->d_dp->dt_func->df_rmdir(iob, pathBuf);
                else
                    ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
                break;
            default:
                ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
                break;
            }
        }
        free_iob(iob);
    }

    if (pathBuf != NULL)
        free_pathbuf(pathBuf);
    pspSetK1(oldK1);
    return ret;
}

int xx_stat(const char *file, SceIoStat *stat, int bits, int get)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    SceIoDeviceEntry *dev;
    int i_unit;
    char *dirNamePtr;
    int oldK1 = pspShiftK1();
    if (!pspK1PtrOk(file) || !pspK1PtrOk(stat))
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    char *buf = alloc_pathbuf();
    if (buf == NULL)
    {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_NO_MEMORY;
    }
    int ret = alloc_iob(&iob, 0);
    if (ret >= 0)
    {
        dirNamePtr = buf;
        ret = resolvePathUser(file, &dev, &i_unit, &dirNamePtr);
        if (ret >= 0)
            ret = init_iob(iob, ret, dev, 0x1000000, i_unit);
        if (ret >= 0)
        {
            if (get == 0)
            {
                ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
                if (dev->d_dp->dt_func->df_chstat != NULL)
                    ret = dev->d_dp->dt_func->df_chstat(iob, dirNamePtr, stat, bits);
            }
            else if (get == 1)
            {
                ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
                if (dev->d_dp->dt_func->df_getstat != NULL)
                    ret = dev->d_dp->dt_func->df_getstat(iob, dirNamePtr, stat);
            }
            else
                ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
        }
        free_iob(iob);
    }

    if (buf != NULL)
        free_pathbuf(buf);
    pspSetK1(oldK1);
    return ret;
}

int do_devctl(const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob;
    SceIoDeviceEntry *arg;
    int i_unit;
    char *path;
    char *buf = alloc_pathbuf();
    if (buf == NULL)
        return SCE_ERROR_KERNEL_NO_MEMORY;
    int ret = alloc_iob(&iob, 0);
    if (ret >= 0)
    {
        path = buf;
        ret = resolvePath(dev, &arg, &i_unit, &path, iob->userMode);
        if (ret >= 0)
            ret = init_iob(iob, ret, arg, 0x1000000, i_unit);
        if (ret >= 0)
        {
            ret = SCE_ERROR_KERNEL_UNSUPPORTED_OPERATION;
            if (arg->d_dp->dt_func->df_devctl != NULL)
                ret = arg->d_dp->dt_func->df_devctl(iob, path, cmd, indata, inlen, outdata, outlen);
        }
        free_iob(iob);
    }

    if (buf != NULL)
        free_pathbuf(buf);
    return ret;
}

int do_deldrv(SceIoDeviceEntry *dev)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldIntr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cur = g_uid_type->PARENT0;
    // 59C4
    while (cur != g_uid_type)
    {
        SceIoIob *iob;
        if (validate_fd(cur->uid ^ g_uidXor, 0, 0xFF, 13, &iob) == 0)
        {
            if (iob->i_de == dev)
            {
                // 5A30
                if (iob->hook.arg != NULL)
                    iob->hook.funcs = &deleted_dt_func;
                // 5A3C
                iob->i_de = &deleted_device;
                if (iob->userMode != 0 && iob->userLevel < 4 && (dev->d_dp->dt_type & 0xFF0000) != 0)
                    dev->d_userfd_count--;
            }
        }
        // 59F0
        cur = cur->PARENT0;
        // 59F4
    }
    // 59FC
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

int _nulldev()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return 0;
}

s64 _nulldev_offt()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return 0;
}

int _nulldev_write(SceIoIob *iob __attribute__((unused)), const char *data, int len)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (sceKernelDipsw(59) == 1) {
        // 5ADC
        sceKernelDebugWrite(0, data, len);
    }
    return len;
}

s32 iob_do_initialize(SceSysmemUidCB *cb, SceSysmemUidCB *uidWithFunc, s32 funcid, void *ap)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    sceKernelCallUIDObjCommonFunction(cb, uidWithFunc, funcid, ap);
    return cb->uid ^ g_uidXor;
}

s32 iob_do_delete(SceSysmemUidCB *cb, SceSysmemUidCB *uidWithFunc, s32 funcid, void *ap)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    sceKernelCallUIDObjCommonFunction(cb, uidWithFunc, funcid, ap);
    return 0;
}

s32 async_loop(SceSize args __attribute__((unused)), void *argp)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceIoIob *iob = *(SceIoIob**)argp;
    do
    {
        u32 bits;
        if (sceKernelWaitEventFlag(iob->asyncEvFlag, 2, 33, &bits, 0) != 0)
            return 0;
        s64 ret = SCE_ERROR_KERNEL_BAD_FILE_DESCRIPTOR;
        pspSetK1(iob->k1);
        switch (iob->asyncCmd)
        {
        case 0:
            // 5C0C
            iob->asyncBusy = 1;
            ret = 0;
            break;

        case 1:
            // 5BA4
            ret = open_main(iob);
            if (ret < 0)
                iob->asyncBusy = 1;
            break;

        case 2:
            // 5C24
            if (iob->hook.arg == NULL) {
                // 5C5C
                ret = iob->i_de->d_dp->dt_func->df_close(iob);
            }
            else
                ret = iob->hook.arg->hook->funcs->Close(&iob->hook);
            // 5C40
            if (ret >= 0)
                iob->asyncBusy = 1;
            break;

        case 3:
            // 5C70
            if (iob->hook.arg == NULL) {
                // 5CA8
                ret = iob->i_de->d_dp->dt_func->df_read(iob, (void*)iob->asyncArgs[0], iob->asyncArgs[1]);
            }
            else
                ret = iob->hook.arg->hook->funcs->Read(&iob->hook, (void*)iob->asyncArgs[0], iob->asyncArgs[1]);
            break;

        case 4:
            // 5CC4
            if (iob->hook.arg == NULL) {
                // 5CEC
                ret = iob->i_de->d_dp->dt_func->df_write(iob, (void*)iob->asyncArgs[0], iob->asyncArgs[1]);
            }
            else
                ret = iob->hook.arg->hook->funcs->Write(&iob->hook, (void*)iob->asyncArgs[0], iob->asyncArgs[1]);
            break;

        case 5:
            // 5D04
            if (iob->hook.arg == NULL) {
                // 5D40
                ret = iob->i_de->d_dp->dt_func->df_lseek(iob, ((s64)iob->asyncArgs[1] << 32) | iob->asyncArgs[0], iob->asyncArgs[2]);
            }
            else
                ret = iob->hook.arg->hook->funcs->Lseek(&iob->hook, ((s64)iob->asyncArgs[1] << 32) | iob->asyncArgs[0], iob->asyncArgs[2]);
            // 5D2C
            break;

        case 6:
            // 5D60
            if (iob->hook.arg == NULL) {
                // 5DA0
                ret = iob->i_de->d_dp->dt_func->df_ioctl(iob, iob->asyncArgs[0], (void*)iob->asyncArgs[1], iob->asyncArgs[2], (void*)iob->asyncArgs[3], iob->asyncArgs[4]);
            }
            else
                ret = iob->hook.arg->hook->funcs->Ioctl(&iob->hook, iob->asyncArgs[0], (void*)iob->asyncArgs[1], iob->asyncArgs[2], (void*)iob->asyncArgs[3], iob->asyncArgs[4]);
            // 5D90
            break;

        default:
            break;
        }
        // (5BB8)
        // 5BBC
        iob->asyncRet = ret;
        if (iob->asyncCb > 0)
            sceKernelNotifyCallback(iob->asyncCb, (int)iob->asyncCbArgp);
        // 5BD8
    } while (sceKernelSetEventFlag(iob->asyncEvFlag, 4) == 0);
    return 0;
}

