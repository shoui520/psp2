#include <common_imp.h>
#include <sysmem_utils_kernel.h>

SCE_MODULE_INFO("sceVideocodec_Driver", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD, 1, 3);
SCE_SDK_VERSION(0x06060110);

typedef struct SceVideocodecCodec {
    s32 unk0;
    s32 unk4;
    s32 unk8;
    void *unk12;      /* 0x0c */
    void *unk16;      /* 0x10 */
    void *edramAddr;  /* 0x14 */
    u32 neededMem;    /* 0x18 */
    void *inBuf;      /* 0x1c */
    u32 inBufSize;    /* 0x20 */
    void *unk36;      /* 0x24 */
    u32 unk40;        /* 0x28 */
    void *unk44;      /* 0x2c */
    void *unk48;      /* 0x30 */
    void *unk52;      /* 0x34 */
    s32 unk56;        /* 0x38 */
    u32 unk60;        /* 0x3c */
    s32 unk64;        /* 0x40 */
    s32 unk68;        /* 0x44 */
    s32 unk72;        /* 0x48 */
    s32 unk76;        /* 0x4c */
    void *seiBuf;     /* 0x50 */
    void *unk84;      /* 0x54 */
    s32 unk88;        /* 0x58 */
    void *allocMem;   /* 0x5c */
} SceVideocodecCodec;

void *sceMeCalloc(u32 num, u32 size);
void sceMeFree(void *ptr);
s32 sceMePower_driver_984E2608(s32 arg0);
s32 sceMePowerControlAvcPower(s32 arg0);
s32 sceMeVideo_driver_4D78330C(s32 type, SceVideocodecCodec *info);
s32 sceMeVideo_driver_6D68B223(s32 type, s32 cmd, SceVideocodecCodec *info);
s32 sceMeVideo_driver_8768915D(s32 type, SceVideocodecCodec *info);
s32 sceMeVideo_driver_8DD56014(s32 type, SceVideocodecCodec *info);
s32 sceMeVideo_driver_C441994C(s32 type, SceVideocodecCodec *info);
s32 sceMeVideo_driver_E8CD3C75(s32 type, SceVideocodecCodec *info);

s32 g_retValues[] = { 0x80628002, 0x80628001, 0x80620002, 0x806201FE, 0x00000000 };

static s32 sub_07a8(SceVideocodecCodec *info, u32 type);
static s32 sub_080c(SceVideocodecCodec *info, u32 type);
static void sub_08b0(SceVideocodecCodec *info, s32 type);
static s32 sub_0924(SceVideocodecCodec *info, u32 type);
static void sub_0ae0(SceVideocodecCodec *info, s32 type);
static void sub_0b8c(SceVideocodecCodec *info, s32 type);
static s32 sub_0c38(SceVideocodecCodec *info, u32 type);
static void sub_0d2c(SceVideocodecCodec *info, s32 type);
static s32 sub_0d94(SceVideocodecCodec *info, u32 type);

s32 sceVideocodecGetEDRAM(SceVideocodecCodec *info, s32 type)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = SCE_ERROR_PRIV_REQUIRED;
    if (pspK1StaBufOk(info, 0x60)) {
        ret = 0x806201FE;
        if (info->allocMem == NULL) {
            void *alloc = sceMeCalloc(((info->neededMem + 0x3F) | 0x3F), 1);
            if (alloc != NULL) {
                info->allocMem = alloc;
                info->edramAddr = (void *)(((u32)alloc + 0x3F) & ~0x3FU);
                ret = 0;
            }
        }
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceVideocodecReleaseEDRAM(SceVideocodecCodec *info)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = SCE_ERROR_PRIV_REQUIRED;
    if (pspK1StaBufOk(info, 0x60)) {
        ret = 0x806201FE;
        if (info->allocMem != NULL) {
            sceMeFree(info->allocMem);
            info->edramAddr = NULL;
            info->allocMem = NULL;
            ret = 0;
        }
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceVideocodecOpen(SceVideocodecCodec *info, u32 type)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = sub_07a8(info, type);
    if (ret == 0) {
        info->unk0 = 0x05100601;
        sceKernelDcacheWritebackInvalidateRange(info->unk16, type == 0 ? 0x28 : 0x100);
        s32 id = sceMeVideo_driver_C441994C(type, info);
        ret = id;
        if (id != 0) {
            sceMePower_driver_984E2608(0);
            sceMePowerControlAvcPower(0);
            s32 idx = ((u32)(id - 1) < (u32)-5) ? id : -4;
            ret = g_retValues[idx + 4];
        }
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceVideocodecSetMemory(SceVideocodecCodec *info, u32 type)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = sub_07a8(info, type);
    if (ret >= 0) {
        if (!pspK1PtrOk(info->unk12)) {
            ret = SCE_ERROR_PRIV_REQUIRED;
        }
    }
    if (ret == 0 && type == 0) {
        sceKernelDcacheWritebackInvalidateRange(info->unk16, 0x28);
        s32 id = sceMeVideo_driver_6D68B223(0, 1, info);
        s32 idx = ((u32)(id - 1) < (u32)-5) ? -4 : id;
        ret = g_retValues[idx + 4];
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceVideocodecScanHeader(SceVideocodecCodec *info, u32 type)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = SCE_ERROR_PRIV_REQUIRED;
    if (pspK1StaBufOk(info, 0x60)
     && pspK1StaBufOk(info->unk16, 0x100)
     && pspK1DynBufOk(info->unk36, info->unk40)
     && pspK1PtrOk(info->unk12)) {
        ret = 0x806201FE;
        if (type - 1 < 2) {
            sceKernelDcacheWritebackInvalidateRange(info->unk16, 0x100);
            if (((u32)info->unk36 & 0x1FFFFFFF) > 0x003FFFFF) {
                sceKernelDcacheWritebackInvalidateRange(info->unk36, info->unk40);
            }
            s32 id = sceMeVideo_driver_6D68B223(type, 2, info);
            s32 idx = ((u32)(id - 1) < (u32)-5) ? -4 : id;
            ret = g_retValues[idx + 4];
        }
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceVideocodecGetVersion(SceVideocodecCodec *info, u32 type)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = SCE_ERROR_PRIV_REQUIRED;
    if (pspK1StaBufOk(info, 0x60)) {
        ret = 0x806201FE;
        if (type < 3) {
            s32 id = sceMeVideo_driver_6D68B223(type, 3, info);
            s32 idx = ((u32)(id - 1) < (u32)-5) ? -4 : id;
            ret = g_retValues[idx + 4];
        }
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceVideocodecGetSEI(SceVideocodecCodec *info, u32 type)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = 0x806201FE;
    if (type == 0) {
        ret = sub_07a8(info, 0);
        if (ret == 0) {
            if (!pspK1StaBufOk(info->seiBuf, 0xA4) || !pspK1PtrOk(info->unk12)) {
                ret = SCE_ERROR_PRIV_REQUIRED;
            }
        }
        if (ret == 0) {
            sceKernelDcacheWritebackInvalidateRange(info->unk16, type == 0 ? 0x28 : 0x100);
            sceKernelDcacheInvalidateRangeForUser(info->seiBuf, 0xC0);
            s32 id = sceMeVideo_driver_6D68B223(type, 5, info);
            s32 idx = ((u32)(id - 1) < (u32)-5) ? -4 : id;
            ret = g_retValues[idx + 4];
        }
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceVideocodec_driver_CB3312D1(void)
{
    sceMePower_driver_984E2608(0);
    sceMePowerControlAvcPower(0);
    return 0;
}

s32 sceVideocodecInit(SceVideocodecCodec *info, u32 type)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = sub_080c(info, type);
    if (ret == 0) {
        sub_08b0(info, type);
        s32 id = sceMeVideo_driver_E8CD3C75(type, info);
        s32 idx = ((u32)(id - 1) < (u32)-5) ? -4 : id;
        ret = g_retValues[idx + 4];
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceVideocodecDecode(SceVideocodecCodec *info, u32 type)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = sub_0924(info, type);
    if (ret == 0) {
        sub_0ae0(info, type);
        sub_0b8c(info, type);
        s32 id = sceMeVideo_driver_8768915D(type, info);
        s32 idx = ((u32)(id - 1) < (u32)-5) ? -4 : id;
        ret = g_retValues[idx + 4];
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceVideocodecStop(SceVideocodecCodec *info, u32 type)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = sub_0c38(info, type);
    if (ret == 0) {
        sub_0d2c(info, type);
        sub_0b8c(info, type);
        s32 id = sceMeVideo_driver_4D78330C(type, info);
        s32 idx = ((u32)(id - 1) < (u32)-5) ? -4 : id;
        ret = g_retValues[idx + 4];
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceVideocodecDelete(SceVideocodecCodec *info, u32 type)
{
    s32 oldK1 = pspShiftK1();
    s32 ret = sub_0d94(info, type);
    if (ret <= 0) {
        sceKernelDcacheWritebackInvalidateRange(info->unk16, type == 0 ? 0x28 : 0x100);
        s32 id = sceMeVideo_driver_8DD56014(type, info);
        s32 idx = ((u32)(id - 1) < (u32)-5) ? -4 : id;
        ret = g_retValues[idx + 4];
    }
    pspSetK1(oldK1);
    return ret;
}

static s32 sub_07a8(SceVideocodecCodec *info, u32 type)
{
    if (!pspK1StaBufOk(info, 0x60))
        return SCE_ERROR_PRIV_REQUIRED;
    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    if (!pspK1DynBufOk(info->unk16, sz))
        return SCE_ERROR_PRIV_REQUIRED;
    return 0;
}

static s32 sub_080c(SceVideocodecCodec *info, u32 type)
{
    if (!pspK1StaBufOk(info, 0x60)
     || !pspK1DynBufOk(info->edramAddr, info->neededMem)
     || !pspK1DynBufOk(info->inBuf, info->inBufSize))
        return SCE_ERROR_PRIV_REQUIRED;
    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    if (!pspK1DynBufOk(info->unk16, sz))
        return SCE_ERROR_PRIV_REQUIRED;
    return 0;
}

static void sub_08b0(SceVideocodecCodec *info, s32 type)
{
    sceKernelDcacheWritebackInvalidateRange(info->unk16, type != 0 ? 0x100 : 0x28);
    if (((u32)info->edramAddr & 0x1FFFFFFF) > 0x003FFFFF) {
        sceKernelDcacheWritebackInvalidateRange(info->edramAddr, info->neededMem);
    }
    sceKernelDcacheWritebackInvalidateRange(info->inBuf, info->inBufSize);
}

static s32 sub_0924(SceVideocodecCodec *info, u32 type)
{
    if (!pspK1StaBufOk(info, 0x60)
     || !pspK1PtrOk(info->unk12)
     || !pspK1DynBufOk(info->edramAddr, info->neededMem)
     || !pspK1DynBufOk(info->inBuf, info->inBufSize)
     || !pspK1DynBufOk(info->unk36, info->unk40))
        return SCE_ERROR_PRIV_REQUIRED;
    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    if (!pspK1StaBufOk(info->unk16, sz))
        return SCE_ERROR_PRIV_REQUIRED;
    if (type != 0)
        return 0;
    u32 cnt = info->unk60;
    if (!pspK1DynBufOk(info->unk44, cnt * 0x2C) || !pspK1StaBufOk(info->unk48, 0x64))
        return SCE_ERROR_PRIV_REQUIRED;
    if (info->unk56 != 0) {
        if (!pspK1DynBufOk(info->unk52, cnt * 0x148))
            return SCE_ERROR_PRIV_REQUIRED;
    }
    u32 flags = (u32)info->unk52;
    if ((s32)flags < 0) {
        if (info->seiBuf != NULL && !pspK1StaBufOk(info->seiBuf, 0xA4))
            return SCE_ERROR_PRIV_REQUIRED;
    }
    if ((flags & 0x40000000) != 0) {
        if (info->unk84 != NULL && !pspK1StaBufOk(info->unk84, 0x10))
            return SCE_ERROR_PRIV_REQUIRED;
    }
    return 0;
}

static void sub_0ae0(SceVideocodecCodec *info, s32 type)
{
    if (((u32)info->unk36 & 0x1FFFFFFF) > 0x003FFFFF) {
        sceKernelDcacheWritebackInvalidateRange(info->unk36, info->unk40);
    }
    if (type != 0) {
        if (((u32)info->edramAddr & 0x1FFFFFFF) > 0x003FFFFF) {
            sceKernelDcacheWritebackInvalidateRange(info->edramAddr, info->neededMem);
        }
        sceKernelDcacheWritebackInvalidateRange(info->inBuf, info->inBufSize);
        sceKernelDcacheWritebackInvalidateRange(info->unk16, 0x100);
    } else {
        sceKernelDcacheWritebackInvalidateRange(info->unk16, 0x28);
    }
}

static void sub_0b8c(SceVideocodecCodec *info, s32 type)
{
    if (type != 0)
        return;
    sceKernelDcacheInvalidateRangeForUser(info->unk44, (info->unk60 * 0x2C + 0x3F) & ~0x3FU);
    sceKernelDcacheInvalidateRangeForUser(info->unk48, 0x40);
    if (info->unk56 != 0) {
        sceKernelDcacheInvalidateRangeForUser((void *)info->unk56, (info->unk60 * 0x148 + 0x3F) & ~0x3FU);
    }
    if ((s32)info->unk52 < 0 && info->seiBuf != NULL) {
        sceKernelDcacheInvalidateRangeForUser(info->seiBuf, 0xC0);
    }
}

static s32 sub_0c38(SceVideocodecCodec *info, u32 type)
{
    if (!pspK1StaBufOk(info, 0x60) || !pspK1PtrOk(info->unk12))
        return SCE_ERROR_PRIV_REQUIRED;
    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    if (!pspK1StaBufOk(info->unk16, sz))
        return SCE_ERROR_PRIV_REQUIRED;
    if (type != 0)
        return 0;
    u32 cnt = info->unk60;
    if (!pspK1DynBufOk(info->unk44, cnt * 0x2C) || !pspK1StaBufOk(info->unk48, 0x64))
        return SCE_ERROR_PRIV_REQUIRED;
    if (info->unk56 != 0) {
        if (!pspK1DynBufOk((void *)info->unk56, cnt * 0x148))
            return SCE_ERROR_PRIV_REQUIRED;
    }
    return 0;
}

static void sub_0d2c(SceVideocodecCodec *info, s32 type)
{
    u32 sz = 0x28;
    if (type != 0) {
        if (((u32)info->edramAddr & 0x1FFFFFFF) > 0x003FFFFF) {
            sceKernelDcacheWritebackInvalidateRange(info->edramAddr, info->neededMem);
        }
        sz = 0x100;
    }
    sceKernelDcacheWritebackInvalidateRange(info->unk16, sz);
}

static s32 sub_0d94(SceVideocodecCodec *info, u32 type)
{
    s32 ret = sub_07a8(info, type);
    if (ret >= 0) {
        if (!pspK1PtrOk(info->unk12))
            ret = SCE_ERROR_PRIV_REQUIRED;
    }
    return ret;
}
