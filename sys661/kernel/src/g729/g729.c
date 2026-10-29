#include <common_imp.h>
#include <interruptman.h>
#include <sysmem_sysclib.h>
#include <sysmem_utils_kernel.h>
#include <threadman_kernel.h>

SCE_MODULE_INFO("sceG729", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD, 1, 3);
SCE_SDK_VERSION(0x06060110);

#define ADDR_IS_RAM(addr) (((0x00220202 >> (((u32)(addr) >> 27) & 0x1F)) & 1) != 0)

typedef struct SceG729Resource {
    s32 count;      /* 0x00 */
    SceUID fplId;   /* 0x04 */
    void *rawBase;  /* 0x08 */
    u8 *base;       /* 0x0c */
} SceG729Resource;

static SceG729Resource g_encRes;
static SceG729Resource g_decRes;

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
    SceUID fpl = sceKernelCreateFpl("SceLibG729EncodeResource", 2, 0, count * 0x1840 + 0x40, 1, NULL);
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
    u8 *aligned = (u8 *)(((u32)g_encRes.rawBase + 0x3F) & ~0x3FU);
    g_encRes.base = aligned;
    for (u32 i = 0; i < count; i++) {
        *(s32 *)(aligned + i * 0x1840 + 0x40) = 0;
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
    u8 *ptr = g_encRes.base;
    for (s32 i = 0; i < g_encRes.count; i++) {
        if (*(s32 *)(ptr + 0x40) == 1) {
            pspSetK1(oldK1);
            return 0x80680003;
        }
        ptr += 0x1840;
    }
    s32 ret = sceKernelFreeFpl(g_encRes.fplId, g_encRes.rawBase);
    if (ret < 0) {
        pspSetK1(oldK1);
        return ret;
    }
    SceUID fpl = g_encRes.fplId;
    g_encRes.base = NULL;
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
    u8 *ptr = g_encRes.base;
    for (; idx < g_encRes.count; idx++, ptr += 0x1840) {
        if (*(s32 *)(ptr + 0x40) == 0) {
            *(s32 *)(ptr + 0x40) = 1;
            break;
        }
    }
    sceKernelCpuResumeIntr(intr);
    if (idx >= g_encRes.count) {
        pspSetK1(oldK1);
        return 0x80680000;
    }
    u8 *slot = g_encRes.base + idx * 0x1840;
    if (*(s32 *)(slot + 0x40) != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    *(s32 *)(slot + 0x44) = mode;
    *(s32 *)(slot + 0x48) = opt;
    *(s32 *)(slot + 0x4C) = outSize;
    s32 ret = sceMeCore_driver_635397BB(0x160, slot + 0x80, mode, opt);
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
    u8 *slot = g_encRes.base + idx * 0x1840;
    if (*(s32 *)(slot + 0x40) != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 ret = sceMeCore_driver_635397BB(0x160, slot + 0x80, *(s32 *)(slot + 0x44), *(s32 *)(slot + 0x48));
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
    if (g_encRes.rawBase == NULL) {
        pspSetK1(oldK1);
        return 0x80680001;
    }
    if (g_encRes.count == 0 || g_encRes.fplId <= 0) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    u8 *slot = g_encRes.base + idx * 0x1840;
    *(s32 *)(slot + 0x40) = 0;
    sceKernelCpuResumeIntr(intr);
    pspSetK1(oldK1);
    return 0;
}

s32 sceG729_driver_DB7259D5(s32 idx, void *inBuf, void *outBuf)
{
    if (inBuf == NULL || outBuf == NULL)
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
    u8 *slot = g_encRes.base + idx * 0x1840;
    if (*(s32 *)(slot + 0x40) != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    u32 outSize = *(u32 *)(slot + 0x4C);
    if (!pspK1StaBufOk(inBuf, 0xA0) || !pspK1DynBufOk(outBuf, outSize)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    sceKernelDcacheWritebackInvalidateRange(inBuf, 0xA0);
    s32 ret = sceKernelDcacheInvalidateRangeForUser(slot, (outSize + 0x3F) & ~0x3FU);
    if (ret >= 0) {
        ret = sceMeCore_driver_FA398D71(0x161, slot + 0x80, inBuf, ((u32)slot & 0x1FFFFFFF) | 0x40000000);
        memcpy(outBuf, slot, *(u32 *)(slot + 0x4C));
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
    SceUID fpl = sceKernelCreateFpl("SceLibG729DecodeResource", 2, 0, count * 0x1400 + 0x40, 1, NULL);
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
    u8 *aligned = (u8 *)(((u32)g_decRes.rawBase + 0x3F) & ~0x3FU);
    g_decRes.base = aligned;
    for (u32 i = 0; i < count; i++) {
        *(s32 *)(aligned + i * 0x1400 + 0xC0) = 0;
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
    u8 *ptr = g_decRes.base;
    for (s32 i = 0; i < g_decRes.count; i++) {
        if (*(s32 *)(ptr + 0xC0) == 1) {
            pspSetK1(oldK1);
            return 0x80680003;
        }
        ptr += 0x1400;
    }
    s32 ret = sceKernelFreeFpl(g_decRes.fplId, g_decRes.rawBase);
    if (ret < 0) {
        pspSetK1(oldK1);
        return ret;
    }
    SceUID fpl = g_decRes.fplId;
    g_decRes.base = NULL;
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
    u8 *ptr = g_decRes.base;
    for (; idx < g_decRes.count; idx++, ptr += 0x1400) {
        if (*(s32 *)(ptr + 0xC0) == 0) {
            *(s32 *)(ptr + 0xC0) = 1;
            break;
        }
    }
    sceKernelCpuResumeIntr(intr);
    if (idx >= g_decRes.count) {
        pspSetK1(oldK1);
        return 0x80680000;
    }
    u8 *slot = g_decRes.base + idx * 0x1400;
    if (*(s32 *)(slot + 0xC0) != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 ret = sceMeCore_driver_635397BB(0x162, slot + 0x100);
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
    u8 *slot = g_decRes.base + idx * 0x1400;
    if (*(s32 *)(slot + 0xC0) != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    s32 ret = sceMeCore_driver_635397BB(0x162, slot + 0x100);
    pspSetK1(oldK1);
    return (ret < 0) ? idx : ret;
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
    u8 *slot = g_decRes.base + idx * 0x1400;
    *(s32 *)(slot + 0xC0) = 0;
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
    u8 *slot = g_decRes.base + idx * 0x1400;
    if (*(s32 *)(slot + 0xC0) != 1) {
        pspSetK1(oldK1);
        return 0x80680002;
    }
    u32 byteLen = (bitLen + 7) >> 3;
    if (!pspK1DynBufOk(inBuf, byteLen) || !pspK1StaBufOk(outBuf, 0xA0)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    sceKernelDcacheWritebackInvalidateRange(inBuf, byteLen);
    s32 ret = sceKernelDcacheInvalidateRangeForUser(slot, 0xC0);
    if (ret >= 0) {
        ret = sceMeCore_driver_FA398D71(0x163, slot + 0x100, bitLen, opt, inBuf, ((u32)slot & 0x1FFFFFFF) | 0x40000000);
        u32 *src = (u32 *)slot;
        u32 *dst = (u32 *)outBuf;
        for (s32 i = 0; i < 10; i++) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            src += 4;
            dst += 4;
        }
    }
    pspSetK1(oldK1);
    return ret;
}
