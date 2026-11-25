#include <common_imp.h>
#include <sysmem_utils_kernel.h>

SCE_MODULE_INFO("sceVideocodec_Driver", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD, 1, 3);
SCE_SDK_VERSION(0x06060110);

typedef struct SceVideocodecCodec {
    s32 version;      /* 0x00 */
    s32 reserved0;    /* 0x04 */
    s32 err;          /* 0x08 */
    void *meHandle;   /* 0x0c */
    void *codecInfo;  /* 0x10 */
    void *edramAddr;  /* 0x14 */
    u32 neededMem;    /* 0x18 */
    void *inBuf;      /* 0x1c */
    u32 inBufSize;    /* 0x20 */
    void *esBuf;      /* 0x24 */
    u32 esBufSize;    /* 0x28 */
    void *frameBufInfo; /* 0x2c */
    void *frameInfo;  /* 0x30 */
    void *decodeFlags;/* 0x34 */
    s32 picBufInfo;   /* 0x38 */
    u32 frameCount;   /* 0x3c */
    s32 reserved1[4]; /* 0x40..0x4f */
    void *seiBuf;     /* 0x50 */
    void *extraInfoBuf;/* 0x54 */
    s32 reserved2;    /* 0x58 */
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

static s32 validateOpenParams(SceVideocodecCodec *info, u32 type);
static s32 validateInitParams(SceVideocodecCodec *info, u32 type);
static void flushInitBuffers(SceVideocodecCodec *info, s32 type);
static s32 validateDecodeParams(SceVideocodecCodec *info, u32 type);
static void flushDecodeInputBuffers(SceVideocodecCodec *info, s32 type);
static void invalidateDecodeOutputBuffers(SceVideocodecCodec *info, s32 type);
static s32 validateStopParams(SceVideocodecCodec *info, u32 type);
static void flushStopBuffers(SceVideocodecCodec *info, s32 type);
static s32 validateDeleteParams(SceVideocodecCodec *info, u32 type);

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
    s32 ret = validateOpenParams(info, type);
    if (ret == 0) {
        info->version = 0x05100601;
        sceKernelDcacheWritebackInvalidateRange(info->codecInfo, type == 0 ? 0x28 : 0x100);
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
    s32 ret = validateOpenParams(info, type);
    if (ret >= 0) {
        if (!pspK1PtrOk(info->meHandle)) {
            ret = SCE_ERROR_PRIV_REQUIRED;
        }
    }
    if (ret == 0 && type == 0) {
        sceKernelDcacheWritebackInvalidateRange(info->codecInfo, 0x28);
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
     && pspK1StaBufOk(info->codecInfo, 0x100)
     && pspK1DynBufOk(info->esBuf, info->esBufSize)
     && pspK1PtrOk(info->meHandle)) {
        ret = 0x806201FE;
        if (type - 1 < 2) {
            sceKernelDcacheWritebackInvalidateRange(info->codecInfo, 0x100);
            if (((u32)info->esBuf & 0x1FFFFFFF) > 0x003FFFFF) {
                sceKernelDcacheWritebackInvalidateRange(info->esBuf, info->esBufSize);
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
        ret = validateOpenParams(info, 0);
        if (ret == 0) {
            if (!pspK1StaBufOk(info->seiBuf, 0xA4) || !pspK1PtrOk(info->meHandle)) {
                ret = SCE_ERROR_PRIV_REQUIRED;
            }
        }
        if (ret == 0) {
            sceKernelDcacheWritebackInvalidateRange(info->codecInfo, type == 0 ? 0x28 : 0x100);
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
    s32 ret = validateInitParams(info, type);
    if (ret == 0) {
        flushInitBuffers(info, type);
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
    s32 ret = validateDecodeParams(info, type);
    if (ret == 0) {
        flushDecodeInputBuffers(info, type);
        invalidateDecodeOutputBuffers(info, type);
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
    s32 ret = validateStopParams(info, type);
    if (ret == 0) {
        flushStopBuffers(info, type);
        invalidateDecodeOutputBuffers(info, type);
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
    s32 ret = validateDeleteParams(info, type);
    if (ret <= 0) {
        sceKernelDcacheWritebackInvalidateRange(info->codecInfo, type == 0 ? 0x28 : 0x100);
        s32 id = sceMeVideo_driver_8DD56014(type, info);
        s32 idx = ((u32)(id - 1) < (u32)-5) ? -4 : id;
        ret = g_retValues[idx + 4];
    }
    pspSetK1(oldK1);
    return ret;
}

static s32 validateOpenParams(SceVideocodecCodec *info, u32 type)
{
    if (!pspK1StaBufOk(info, 0x60))
        return SCE_ERROR_PRIV_REQUIRED;
    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    if (!pspK1DynBufOk(info->codecInfo, sz))
        return SCE_ERROR_PRIV_REQUIRED;
    return 0;
}

static s32 validateInitParams(SceVideocodecCodec *info, u32 type)
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
    if (!pspK1DynBufOk(info->codecInfo, sz))
        return SCE_ERROR_PRIV_REQUIRED;
    return 0;
}

static void flushInitBuffers(SceVideocodecCodec *info, s32 type)
{
    sceKernelDcacheWritebackInvalidateRange(info->codecInfo, type != 0 ? 0x100 : 0x28);
    if (((u32)info->edramAddr & 0x1FFFFFFF) > 0x003FFFFF) {
        sceKernelDcacheWritebackInvalidateRange(info->edramAddr, info->neededMem);
    }
    sceKernelDcacheWritebackInvalidateRange(info->inBuf, info->inBufSize);
}

static s32 validateDecodeParams(SceVideocodecCodec *info, u32 type)
{
    if (!pspK1StaBufOk(info, 0x60)
     || !pspK1PtrOk(info->meHandle)
     || !pspK1DynBufOk(info->edramAddr, info->neededMem)
     || !pspK1DynBufOk(info->inBuf, info->inBufSize)
     || !pspK1DynBufOk(info->esBuf, info->esBufSize))
        return SCE_ERROR_PRIV_REQUIRED;
    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    if (!pspK1StaBufOk(info->codecInfo, sz))
        return SCE_ERROR_PRIV_REQUIRED;
    if (type != 0)
        return 0;
    u32 cnt = info->frameCount;
    if (!pspK1DynBufOk(info->frameBufInfo, cnt * 0x2C) || !pspK1StaBufOk(info->frameInfo, 0x64))
        return SCE_ERROR_PRIV_REQUIRED;
    if (info->picBufInfo != 0) {
        if (!pspK1DynBufOk(info->decodeFlags, cnt * 0x148))
            return SCE_ERROR_PRIV_REQUIRED;
    }
    u32 flags = (u32)info->decodeFlags;
    if ((s32)flags < 0) {
        if (info->seiBuf != NULL && !pspK1StaBufOk(info->seiBuf, 0xA4))
            return SCE_ERROR_PRIV_REQUIRED;
    }
    if ((flags & 0x40000000) != 0) {
        if (info->extraInfoBuf != NULL && !pspK1StaBufOk(info->extraInfoBuf, 0x10))
            return SCE_ERROR_PRIV_REQUIRED;
    }
    return 0;
}

static void flushDecodeInputBuffers(SceVideocodecCodec *info, s32 type)
{
    if (((u32)info->esBuf & 0x1FFFFFFF) > 0x003FFFFF) {
        sceKernelDcacheWritebackInvalidateRange(info->esBuf, info->esBufSize);
    }
    if (type != 0) {
        if (((u32)info->edramAddr & 0x1FFFFFFF) > 0x003FFFFF) {
            sceKernelDcacheWritebackInvalidateRange(info->edramAddr, info->neededMem);
        }
        sceKernelDcacheWritebackInvalidateRange(info->inBuf, info->inBufSize);
        sceKernelDcacheWritebackInvalidateRange(info->codecInfo, 0x100);
    } else {
        sceKernelDcacheWritebackInvalidateRange(info->codecInfo, 0x28);
    }
}

static void invalidateDecodeOutputBuffers(SceVideocodecCodec *info, s32 type)
{
    if (type != 0)
        return;
    sceKernelDcacheInvalidateRangeForUser(info->frameBufInfo, (info->frameCount * 0x2C + 0x3F) & ~0x3FU);
    sceKernelDcacheInvalidateRangeForUser(info->frameInfo, 0x40);
    if (info->picBufInfo != 0) {
        sceKernelDcacheInvalidateRangeForUser((void *)info->picBufInfo, (info->frameCount * 0x148 + 0x3F) & ~0x3FU);
    }
    if ((s32)info->decodeFlags < 0 && info->seiBuf != NULL) {
        sceKernelDcacheInvalidateRangeForUser(info->seiBuf, 0xC0);
    }
}

static s32 validateStopParams(SceVideocodecCodec *info, u32 type)
{
    if (!pspK1StaBufOk(info, 0x60) || !pspK1PtrOk(info->meHandle))
        return SCE_ERROR_PRIV_REQUIRED;
    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    if (!pspK1StaBufOk(info->codecInfo, sz))
        return SCE_ERROR_PRIV_REQUIRED;
    if (type != 0)
        return 0;
    u32 cnt = info->frameCount;
    if (!pspK1DynBufOk(info->frameBufInfo, cnt * 0x2C) || !pspK1StaBufOk(info->frameInfo, 0x64))
        return SCE_ERROR_PRIV_REQUIRED;
    if (info->picBufInfo != 0) {
        if (!pspK1DynBufOk((void *)info->picBufInfo, cnt * 0x148))
            return SCE_ERROR_PRIV_REQUIRED;
    }
    return 0;
}

static void flushStopBuffers(SceVideocodecCodec *info, s32 type)
{
    u32 sz = 0x28;
    if (type != 0) {
        if (((u32)info->edramAddr & 0x1FFFFFFF) > 0x003FFFFF) {
            sceKernelDcacheWritebackInvalidateRange(info->edramAddr, info->neededMem);
        }
        sz = 0x100;
    }
    sceKernelDcacheWritebackInvalidateRange(info->codecInfo, sz);
}

static s32 validateDeleteParams(SceVideocodecCodec *info, u32 type)
{
    s32 ret = validateOpenParams(info, type);
    if (ret >= 0) {
        if (!pspK1PtrOk(info->meHandle))
            ret = SCE_ERROR_PRIV_REQUIRED;
    }
    return ret;
}
