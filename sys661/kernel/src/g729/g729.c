#include <common_imp.h>
#include <interruptman.h>
#include <sysmem_sysclib.h>
#include <sysmem_utils_kernel.h>
#include <threadman_kernel.h>

SCE_MODULE_INFO("sceG729", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD, 1, 3);
SCE_SDK_VERSION(0x06060110);

#define ADDR_IS_RAM(addr) (((0x00220202 >> (((u32)(addr) >> 27) & 0x1F)) & 1) != 0)

typedef struct SceG729EncodeSlot {
    u8 outBuf[0x40];    /* 0x0000 */
    s32 active;         /* 0x0040 */
    s32 mode;           /* 0x0044 */
    s32 opt;            /* 0x0048 */
    u32 outSize;        /* 0x004C */
    u8 reserved[0x30];  /* 0x0050 */
    u8 meCtx[0x17C0];   /* 0x0080 */
} SceG729EncodeSlot;    /* size = 0x1840 */

typedef struct SceG729DecodeSlot {
    u8 outBuf[0xC0];    /* 0x0000 */
    s32 active;         /* 0x00C0 */
    u8 reserved[0x3C];  /* 0x00C4 */
    u8 meCtx[0x1300];   /* 0x0100 */
} SceG729DecodeSlot;    /* size = 0x1400 */

typedef struct SceG729EncodeResource {
    s32 count;                /* 0x00 */
    SceUID fplId;             /* 0x04 */
    void *rawBase;            /* 0x08 */
    SceG729EncodeSlot *slots; /* 0x0c */
} SceG729EncodeResource;

typedef struct SceG729DecodeResource {
    s32 count;                /* 0x00 */
    SceUID fplId;             /* 0x04 */
    void *rawBase;            /* 0x08 */
    SceG729DecodeSlot *slots; /* 0x0c */
} SceG729DecodeResource;

static SceG729EncodeResource g_encRes;
static SceG729DecodeResource g_decRes;

s32 sceMeCore_driver_635397BB(s32 cmd, ...);
s32 sceMeCore_driver_FA398D71(s32 cmd, ...);

s32 sceG729_driver_AA1E5462(u32 count)
{
    if (count >= 5)
        return 0x80680005;
    s32 oldK1 = pspShiftK1();
    if (g_encRes.rawBase != NULL) {
        pspSetK1(oldK1);
        return 0x80680003;
    }
    SceUID fpl = sceKernelCreateFpl("SceLibG729EncodeResource", 2, 0, count * sizeof(SceG729EncodeSlot) + 0x40, 1, NULL);
    if (fpl <= 0) {
        pspSetK1(oldK1);
        return 0x80680000;
    }
    g_encRes.fplId = fpl;
    s32 ret = sceKernelAllocateFpl(fpl, &g_encRes.rawBase, NULL);
    if (ret < 0) {
        sceKernelDeleteFpl(g_encRes.fplId);
        g_encRes.fplId = 0;
        pspSetK1(oldK1);
        return ret;
    }
    SceG729EncodeSlot *aligned = (SceG729EncodeSlot *)(((u32)g_encRes.rawBase + 0x3F) & ~0x3FU);
    g_encRes.slots = aligned;
    for (u32 i = 0; i < count; i++) {
        aligned[i].active = 0;
    }
    g_encRes.count = count;
    pspSetK1(oldK1);
    return 0;
}

s32 sceG729_driver_94714D50(void)
{
    s32 oldK1 = pspShiftK1();
    if (g_encRes.rawBase == NULL || g_encRes.count == 0 || g_encRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0;
    }
    for (s32 i = 0; i < g_encRes.count; i++) {
        if (g_encRes.slots[i].active == 1) {
            pspSetK1(oldK1);
            return 0x80680003;
        }
    }
    s32 ret = sceKernelFreeFpl(g_encRes.fplId, g_encRes.rawBase);
    if (ret < 0) {
        pspSetK1(oldK1);
        return ret;
    }
    SceUID fpl = g_encRes.fplId;
    g_encRes.slots = NULL;
    g_encRes.rawBase = NULL;
    ret = sceKernelDeleteFpl(fpl);
    if (ret < 0) {
        pspSetK1(oldK1);
        return ret;
    }
    g_encRes.fplId = 0;
    pspSetK1(oldK1);
    return 0;
}

s32 sceG729_driver_CFCD367C(s32 mode, u32 opt)
{
    s32 outSize;
    if (mode == 0) {
        outSize = 8;
    } else if (mode == 1) {
        outSize = 10;
    } else if (mode == 2) {
        outSize = 15;
    } else {
        return 0x80680001;
    }
    if (opt >= 2)
        return 0x80680001;
    s32 oldK1 = pspShiftK1();
    if (g_encRes.rawBase == NULL || g_encRes.count == 0 || g_encRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    s32 idx = 0;
    for (; idx < g_encRes.count; idx++) {
        if (g_encRes.slots[idx].active == 0) {
            g_encRes.slots[idx].active = 1;
            break;
        }
    }
    sceKernelCpuResumeIntr(intr);
    if (idx >= g_encRes.count) {
        pspSetK1(oldK1);
        return 0x80680000;
    }
    SceG729EncodeSlot *slot = &g_encRes.slots[idx];
    if (slot->active != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    slot->mode = mode;
    slot->opt = opt;
    slot->outSize = outSize;
    s32 ret = sceMeCore_driver_635397BB(0x160, slot->meCtx, mode, opt);
    pspSetK1(oldK1);
    return (ret < 0) ? ret : idx;
}

s32 sceG729_driver_8C87A2CA(s32 idx)
{
    s32 oldK1 = pspShiftK1();
    if (idx < 0 || idx >= g_encRes.count) {
        pspSetK1(oldK1);
        return 0x80680001;
    }
    if (g_encRes.rawBase == NULL || g_encRes.count == 0 || g_encRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    SceG729EncodeSlot *slot = &g_encRes.slots[idx];
    if (slot->active != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 ret = sceMeCore_driver_635397BB(0x160, slot->meCtx, slot->mode, slot->opt);
    pspSetK1(oldK1);
    return ret;
}

s32 sceG729_driver_5A409D1B(s32 idx)
{
    s32 oldK1 = pspShiftK1();
    if (idx < 0 || idx >= g_encRes.count) {
        pspSetK1(oldK1);
        return 0x80680001;
    }
    if (g_encRes.rawBase == NULL || g_encRes.count == 0 || g_encRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    g_encRes.slots[idx].active = 0;
    sceKernelCpuResumeIntr(intr);
    pspSetK1(oldK1);
    return 0;
}

s32 sceG729_driver_DB7259D5(s32 idx, void *inBuf, void *outBuf)
{
    if ((inBuf == NULL) | (outBuf == NULL))
        return 0x80680001;
    if (((u32)inBuf & 1) != 0)
        return 0x80680001;
    if (!ADDR_IS_RAM(inBuf) || !ADDR_IS_RAM(outBuf))
        return 0x80680001;
    s32 oldK1 = pspShiftK1();
    if (idx < 0 || idx >= g_encRes.count) {
        pspSetK1(oldK1);
        return 0x80680001;
    }
    if (g_encRes.rawBase == NULL || g_encRes.count == 0 || g_encRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    SceG729EncodeSlot *slot = &g_encRes.slots[idx];
    if (slot->active != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    u32 outSize = slot->outSize;
    if (!pspK1StaBufOk(inBuf, 0xA0) || !pspK1DynBufOk(outBuf, outSize)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    sceKernelDcacheWritebackInvalidateRange(inBuf, 0xA0);
    s32 ret = sceKernelDcacheInvalidateRangeForUser(slot->outBuf, (outSize + 0x3F) & ~0x3FU);
    if (ret >= 0) {
        ret = sceMeCore_driver_FA398D71(0x161, slot->meCtx, inBuf, ((u32)slot->outBuf & 0x1FFFFFFF) | 0x40000000);
        memcpy(outBuf, slot->outBuf, slot->outSize);
    }
    pspSetK1(oldK1);
    return ret;
}

s32 sceG729_driver_17C11696(u32 count)
{
    if (count >= 5)
        return 0x80680005;
    s32 oldK1 = pspShiftK1();
    if (g_decRes.rawBase != NULL) {
        pspSetK1(oldK1);
        return 0x80680003;
    }
    SceUID fpl = sceKernelCreateFpl("SceLibG729DecodeResource", 2, 0, count * sizeof(SceG729DecodeSlot) + 0x40, 1, NULL);
    if (fpl <= 0) {
        pspSetK1(oldK1);
        return 0x80680000;
    }
    g_decRes.fplId = fpl;
    s32 ret = sceKernelAllocateFpl(fpl, &g_decRes.rawBase, NULL);
    if (ret < 0) {
        sceKernelDeleteFpl(g_decRes.fplId);
        g_decRes.fplId = 0;
        pspSetK1(oldK1);
        return ret;
    }
    SceG729DecodeSlot *aligned = (SceG729DecodeSlot *)(((u32)g_decRes.rawBase + 0x3F) & ~0x3FU);
    g_decRes.slots = aligned;
    for (u32 i = 0; i < count; i++) {
        aligned[i].active = 0;
    }
    g_decRes.count = count;
    pspSetK1(oldK1);
    return 0;
}

s32 sceG729_driver_890B86AE(void)
{
    s32 oldK1 = pspShiftK1();
    if (g_decRes.rawBase == NULL || g_decRes.count == 0 || g_decRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0;
    }
    for (s32 i = 0; i < g_decRes.count; i++) {
        if (g_decRes.slots[i].active == 1) {
            pspSetK1(oldK1);
            return 0x80680003;
        }
    }
    s32 ret = sceKernelFreeFpl(g_decRes.fplId, g_decRes.rawBase);
    if (ret < 0) {
        pspSetK1(oldK1);
        return ret;
    }
    SceUID fpl = g_decRes.fplId;
    g_decRes.slots = NULL;
    g_decRes.rawBase = NULL;
    ret = sceKernelDeleteFpl(fpl);
    if (ret < 0) {
        pspSetK1(oldK1);
        return ret;
    }
    g_decRes.fplId = 0;
    pspSetK1(oldK1);
    return 0;
}

s32 sceG729_driver_55E14F75(void)
{
    s32 oldK1 = pspShiftK1();
    if (g_decRes.rawBase == NULL || g_decRes.count == 0 || g_decRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    s32 idx = 0;
    for (; idx < g_decRes.count; idx++) {
        if (g_decRes.slots[idx].active == 0) {
            g_decRes.slots[idx].active = 1;
            break;
        }
    }
    sceKernelCpuResumeIntr(intr);
    if (idx >= g_decRes.count) {
        pspSetK1(oldK1);
        return 0x80680000;
    }
    SceG729DecodeSlot *slot = &g_decRes.slots[idx];
    if (slot->active != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 ret = sceMeCore_driver_635397BB(0x162, slot->meCtx);
    pspSetK1(oldK1);
    return (ret < 0) ? ret : idx;
}

s32 sceG729_driver_74804D93(s32 idx)
{
    s32 oldK1 = pspShiftK1();
    if (idx < 0 || idx >= g_decRes.count) {
        pspSetK1(oldK1);
        return 0x80680001;
    }
    if (g_decRes.rawBase == NULL || g_decRes.count == 0 || g_decRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    SceG729DecodeSlot *slot = &g_decRes.slots[idx];
    if (slot->active != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 ret = sceMeCore_driver_635397BB(0x162, slot->meCtx);
    pspSetK1(oldK1);
    return (ret >= 0) ? idx : ret;
}

s32 sceG729_driver_13F1028A(s32 idx)
{
    s32 oldK1 = pspShiftK1();
    if (idx < 0 || idx >= g_decRes.count) {
        pspSetK1(oldK1);
        return 0x80680001;
    }
    if (g_decRes.rawBase == NULL) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    if (g_decRes.count == 0 || g_decRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    g_decRes.slots[idx].active = 0;
    sceKernelCpuResumeIntr(intr);
    pspSetK1(oldK1);
    return 0;
}

s32 sceG729_driver_3489D1F3(s32 idx, u32 bitLen, u32 opt, void *inBuf, void *outBuf)
{
    if (inBuf == NULL || outBuf == NULL)
        return 0x80680001;
    if (((u32)outBuf & 1) != 0)
        return 0x80680001;
    if (!ADDR_IS_RAM(inBuf) || !ADDR_IS_RAM(outBuf))
        return 0x80680001;
    if (bitLen != 0 && bitLen != 15 && bitLen != 16 && bitLen != 64 && bitLen != 80 && bitLen != 118)
        return 0x80680001;
    if (opt >= 2)
        return 0x80680001;
    s32 oldK1 = pspShiftK1();
    if (idx < 0 || idx >= g_decRes.count) {
        pspSetK1(oldK1);
        return 0x80680001;
    }
    if (g_decRes.rawBase == NULL || g_decRes.count == 0 || g_decRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    SceG729DecodeSlot *slot = &g_decRes.slots[idx];
    if (slot->active != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    u32 byteLen = (bitLen + 7) >> 3;
    if (!pspK1DynBufOk(inBuf, byteLen) || !pspK1StaBufOk(outBuf, 0xA0)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    sceKernelDcacheWritebackInvalidateRange(inBuf, byteLen);
    s32 ret = sceKernelDcacheInvalidateRangeForUser(slot->outBuf, sizeof(slot->outBuf));
    if (ret >= 0) {
        ret = sceMeCore_driver_FA398D71(0x163, slot->meCtx, bitLen, opt, inBuf, ((u32)slot->outBuf & 0x1FFFFFFF) | 0x40000000);
        memcpy(outBuf, slot->outBuf, 0xA0);
    }
    pspSetK1(oldK1);
    return ret;
}
