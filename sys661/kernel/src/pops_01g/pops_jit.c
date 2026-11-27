#include "pops_internal.h"

/*
 * PS1 MIPS R3000A -> PSP Allegrex Dynamic Recompiler (JIT)
 *
 * Translates PS1 basic blocks into native Allegrex code in the 0x09540000
 * (RAM) and 0x097B0000 (BIOS) translation caches, with 12 dynamically
 * allocated host registers, constant propagation, load/store fast paths,
 * block checksum guards for self-modifying code, and direct GTE VFPU calls.
 */

#define POPS_IR_NODES ((PopsJitIrNode *)POPS_JIT_IR_BASE)

static __inline__ u32 popsJitJumpWord(u32 targetAddr)
{
    return (targetAddr >> 2) & 0x03FFFFFFU;
}

static __inline__ u32 popsJitJalEncode(const void *fnPtr)
{
    return ((u32)fnPtr + 0x30000000U) >> 2;
}

static __inline__ u32 popsJitJumpEncode(const void *fnPtr)
{
    return ((u32)fnPtr + 0x20000000U) >> 2;
}

static __inline__ void popsCacheHitWritebackInv(const void *addr)
{
    __asm__ __volatile__("cache 0x1B, 0(%0)" :: "r"(addr) : "memory");
}

static __inline__ void popsCacheCreateDirty(const void *addr)
{
    __asm__ __volatile__("cache 0x18, 0(%0)" :: "r"(addr) : "memory");
}

static __inline__ void popsCacheDataHitWriteback(const void *addr)
{
    __asm__ __volatile__("cache 0x1A, 0(%0)" :: "r"(addr) : "memory");
}

static __inline__ void popsCacheInstHitInv(const void *addr)
{
    __asm__ __volatile__("cache 0x08, 0(%0)" :: "r"(addr) : "memory");
}

static __inline__ void popsCacheInstHitFill(const void *addr)
{
    __asm__ __volatile__("cache 0x0A, 0(%0)" :: "r"(addr) : "memory");
}

static __inline__ void popsCacheDataHitInv(const void *addr)
{
    __asm__ __volatile__("cache 0x19, 0(%0)" :: "r"(addr) : "memory");
}

static __inline__ u32 popsSpillOffsetForGuest(u32 guestReg)
{
    return ((guestReg & 0xE0U) | ((guestReg & 7U) * 4U)) + 384U;
}

static __inline__ u32 *popsLookupSlotForPc(u32 guestPc)
{
    u32 phys = guestPc & 0x1FFFFFFFU;
    if ((phys >> 23) != 0U) {
        if ((phys - 0x1FC00000U) > 0x7FFFFU) {
            return (u32 *)0;
        }
        return (u32 *)(POPS_JIT_LOOKUP_BIOS + (phys & 0x1FFFFCU));
    }
    return (u32 *)(POPS_JIT_LOOKUP_RAM + (guestPc & 0x1FFFFCU));
}

static __inline__ u32 *popsRamLookupSlot(u32 guestPc)
{
    return (u32 *)(POPS_JIT_LOOKUP_RAM + (guestPc & 0x1FFFFCU));
}

static __inline__ u32 *popsRegionLookupSlot(u32 guestPc, u32 isBios)
{
    u32 base = isBios ? POPS_JIT_LOOKUP_BIOS : POPS_JIT_LOOKUP_RAM;
    return (u32 *)(base + (guestPc & 0x1FFFFCU));
}

static __inline__ u32 popsNodePc(const PopsJitIrNode *node)
{
    return g_pops->jitBlockStartPc + (u32)((node - POPS_IR_NODES) * 4);
}

static __inline__ PopsJitIrNode *popsNodeForPc(u32 guestPc)
{
    return &POPS_IR_NODES[(guestPc - g_pops->jitBlockStartPc) >> 2];
}

/* [12] 0x000f8c */
__attribute__((noinline))
s32 popsJitFindGuestReg(s32 guestReg)
{
    s32 mapped = (s32)g_pops->jitGuestRegMap[guestReg];
    if (mapped < 0) {
        s32 slot;
        for (slot = 0; slot < 12; slot++) {
            if ((s32)g_pops->jitSlotGuestReg[slot] == guestReg) {
                return (s32)g_pops->jitSlotHostReg[slot];
            }
        }
    }
    return mapped;
}

/* [13] 0x000fcc */
__attribute__((noinline))
s32 popsJitPrevNotBranch(u32 emitPtr)
{
    if (emitPtr == g_pops->jitBlockStartPtr) {
        return 0;
    }
    {
        u32 prevInsn = ((const u32 *)emitPtr)[-2];
        u32 hiBits = prevInsn >> 29;
        if (hiBits == 0U) {
            u32 lowOp = (prevInsn >> 26) & 7U;
            s32 res = (s32)((prevInsn & 0x38U) - 8U);
            if (lowOp != 0U) {
                res = 0;
            }
            return res;
        }
        return (s32)hiBits;
    }
}

/* [14] 0x001000 */
__attribute__((noinline))
u32 *popsJitLoadGuestReg(u32 *emitPtr, s32 hostReg, s32 guestReg)
{
    u32 spillOff = popsSpillOffsetForGuest((u32)guestReg);
    s32 mapped = (s32)g_pops->jitGuestRegMap[guestReg];
    if (mapped == -1) {
        return popsJitEmitCopRead(emitPtr, hostReg | 0x80, (s32)spillOff);
    }
    {
        u32 prevWord = emitPtr[-1];
        s32 canFuse = popsJitPrevNotBranch((u32)emitPtr);
        u32 fpuReg = (u32)(-mapped) & 0x1FU;
        u32 expectedMtc = 0x44800000U - ((u32)mapped << 11);
        if (canFuse != 0 && (prevWord & 0xFFE0FFFFU) == expectedMtc) {
            u32 srcHost = (prevWord >> 16) & 0x1FU;
            *emitPtr = (srcHost << 21) | (((u32)hostReg & 0x1FU) << 11) | 0x00000021U;
            return emitPtr + 1;
        }
        *emitPtr = 0x44000000U | (((u32)hostReg & 0x1FU) << 16) | (fpuReg << 11);
        return emitPtr + 1;
    }
}

/* [15] 0x0010c4 */
__attribute__((noinline))
u32 *popsJitEmitImm(u32 *emitPtr, s32 hostReg, u32 imm)
{
    u32 rtField = ((u32)hostReg & 0x1FU) << 16;
    u32 hiHalf = imm >> 16;
    u32 loHalf = imm & 0xFFFFU;

    if (imm == (u32)(s32)(s16)imm) {
        *emitPtr = 0x24000000U | rtField | loHalf;
        return emitPtr + 1;
    }
    if (hiHalf == 0U) {
        *emitPtr = 0x34000000U | rtField | loHalf;
        return emitPtr + 1;
    }
    if (loHalf == 0U) {
        *emitPtr = 0x3C000000U | rtField | hiHalf;
        return emitPtr + 1;
    }
    if (hiHalf == 1U) {
        *emitPtr = 0x37800000U | rtField | loHalf;
        return emitPtr + 1;
    }
    if (g_pops->jitCachedBasePtr != 0U && hiHalf == g_pops->jitCachedUpperAddr) {
        *emitPtr = 0x34400000U | rtField | loHalf;
        return emitPtr + 1;
    }
    if (hiHalf == 0x0980U) {
        *emitPtr = 0x37A00000U | rtField | loHalf;
        return emitPtr + 1;
    }

    {
        s32 regIdx;
        for (regIdx = 1; regIdx < 32; regIdx++) {
            if ((s32)(g_pops->jitConstValidMask << regIdx) < 0) {
                u32 knownVal = g_pops->jitConstValues[regIdx];
                s32 foundHost = popsJitFindGuestReg(regIdx);
                if (foundHost >= 0) {
                    s32 delta = (s32)(imm - knownVal);
                    if (delta == (s32)(s16)delta) {
                        *emitPtr = 0x24000000U | (((u32)foundHost & 0x1FU) << 21) | rtField | ((u32)delta & 0xFFFFU);
                        return emitPtr + 1;
                    }
                    if (((imm ^ knownVal) & 0xFFFF0000U) == 0U) {
                        *emitPtr = 0x38000000U | (((u32)foundHost & 0x1FU) << 21) | rtField | ((imm ^ knownVal) & 0xFFFFU);
                        return emitPtr + 1;
                    }
                } else if (imm == knownVal) {
                    return popsJitLoadGuestReg(emitPtr, hostReg, regIdx);
                }
            }
        }
    }

    emitPtr[0] = 0x3C000000U | rtField | hiHalf;
    emitPtr[1] = 0x34000000U | (((u32)hostReg & 0x1FU) << 21) | rtField | loHalf;
    return emitPtr + 2;
}

/* [16] 0x001270 */
__attribute__((noinline))
u32 *popsJitSpillSlot(s32 slotIdx, u32 *emitPtr)
{
    if (g_pops->jitSlotDirty[slotIdx] != 0U) {
        u32 guestReg = g_pops->jitSlotGuestReg[slotIdx];
        u32 hostReg = g_pops->jitSlotHostReg[slotIdx];
        g_pops->jitSlotDirty[slotIdx] = 0U;
        if (guestReg >= 32U || g_pops->jitGuestRegMap[guestReg] == -1) {
            u32 spillOff = popsSpillOffsetForGuest(guestReg);
            *emitPtr = 0xAF800000U | ((hostReg & 0x1FU) << 16) | (spillOff & 0xFFFFU);
            return emitPtr + 1;
        } else {
            u32 fpuReg = (u32)(-g_pops->jitGuestRegMap[guestReg]) & 0x1FU;
            *emitPtr = 0x44800000U | ((hostReg & 0x1FU) << 16) | (fpuReg << 11);
            return emitPtr + 1;
        }
    }
    return emitPtr;
}

/* [17] 0x0012ec */
__attribute__((noinline))
s32 popsJitAllocTempReg(s32 fallbackReg, u8 dirtyFlag)
{
    s32 slot;
    for (slot = 0; slot < 12; slot++) {
        if (g_pops->jitSlotGuestReg[slot] == 0U) {
            g_pops->jitSlotDirty[slot] = dirtyFlag;
            g_pops->jitSlotGuestReg[slot] = 32U;
            return (s32)g_pops->jitSlotHostReg[slot];
        }
    }
    if (fallbackReg == 2) {
        g_pops->jitCachedBasePtr = 0U;
    } else {
        g_pops->jitBaseGuestReg = 0U;
    }
    return fallbackReg;
}

/* [18] 0x001338 */
__attribute__((noinline))
void popsJitFreeTempReg(s32 hostReg)
{
    if (hostReg != 2 && hostReg != 4) {
        s32 slot;
        for (slot = 0; slot < 12; slot++) {
            if ((s32)g_pops->jitSlotHostReg[slot] == hostReg) {
                g_pops->jitSlotGuestReg[slot] = 0U;
                return;
            }
        }
    }
}

/* [19] 0x001378 */
__attribute__((noinline))
u32 *popsJitFlushHiLo(u32 *emitPtr)
{
    if (g_pops->jitHiLoDirtyMask != 0U) {
        s32 tempHost = popsJitAllocTempReg(2, 0U);
        u32 rdBits = ((u32)tempHost & 0x1FU) << 11;
        u32 rtBits = ((u32)tempHost & 0x1FU) << 16;
        if ((g_pops->jitHiLoDirtyMask & 1U) != 0U) {
            emitPtr[0] = rdBits | 0x00000010U;
            emitPtr[1] = rtBits | 0xAF8001A4U;
            emitPtr += 2;
        }
        if ((g_pops->jitHiLoDirtyMask & 2U) != 0U) {
            emitPtr[0] = rdBits | 0x00000012U;
            emitPtr[1] = rtBits | 0xAF8001A8U;
            emitPtr += 2;
        }
        g_pops->jitHiLoDirtyMask = 0U;
        popsJitFreeTempReg(tempHost);
    }
    return emitPtr;
}

/* [20] 0x001424 */
__attribute__((noinline))
u32 *popsJitFlushSlots(u32 *emitPtr, s32 maxSlot)
{
    s32 slot = maxSlot;
    do {
        if (g_pops->jitSlotGuestReg[slot] != 0U) {
            emitPtr = popsJitSpillSlot(slot, emitPtr);
            g_pops->jitSlotGuestReg[slot] = 0U;
        }
        slot--;
    } while (slot >= 0);

    g_pops->jitCachedBasePtr = 0U;
    g_pops->jitBaseGuestReg = 0U;
    emitPtr = popsJitFlushHiLo(emitPtr);
    g_pops->jitHiLoLiveMask = 0U;
    return emitPtr;
}

/* [21] 0x001494 */
__attribute__((noinline))
u32 *popsJitSaveAllDirty(u32 *emitPtr)
{
    s32 slot = 11;
    do {
        emitPtr = popsJitSpillSlot(slot, emitPtr);
        slot--;
    } while (slot >= 0);
    return popsJitFlushHiLo(emitPtr);
}

/* [22] 0x0014d8 */
__attribute__((noinline))
u32 popsJitAcquireReg(u32 *emitPtr, s32 guestReg, u32 liveMask, u8 modeFlags)
{
    u8 isWrite = (u8)(modeFlags & 1U);
    s32 freeSlot = 0;

    if (isWrite != 0U && guestReg != 0) {
        g_pops->jitConstValidMask &= ~(0x80000000U >> guestReg);
        if ((u8)guestReg == g_pops->jitBaseGuestReg) {
            g_pops->jitBaseGuestReg = 0U;
        }
    }

    {
        s32 fixedHost = (s32)g_pops->jitGuestRegMap[guestReg];
        if (fixedHost >= 0) {
            return (((u32)emitPtr) << 3) | (u32)fixedHost;
        }
    }

    {
        s32 slot;
        for (slot = 0; slot < 12; slot++) {
            u8 slotGuest = g_pops->jitSlotGuestReg[slot];
            if (slotGuest == 0U) {
                freeSlot = slot;
            }
            if (slotGuest == (u8)guestReg) {
                if (isWrite != 0U) {
                    g_pops->jitSlotDirty[slot] = isWrite;
                }
                return (((u32)emitPtr) << 3) | (u32)g_pops->jitSlotHostReg[slot];
            }
        }
    }

    if (freeSlot == 0) {
        s32 bestScore = 100;
        s32 cursor = (s32)g_pops->jitEvictCursor + 1;
        for (;;) {
            if (cursor >= 12) {
                cursor = 0;
            }
            {
                u32 curGuest = g_pops->jitSlotGuestReg[cursor];
                if (curGuest < 32U && ((liveMask >> curGuest) & 1U) == 0U) {
                    s32 score = (g_pops->jitGuestRegMap[curGuest] == -1) ? 1 : 0;
                    if (g_pops->jitSlotDirty[cursor] != 0U) {
                        score = (score * 2) + 1;
                    }
                    if (((s32)(g_pops->jitConstValidMask << curGuest) >> 31) == 0) {
                        score++;
                    }
                    if (score < bestScore) {
                        bestScore = score;
                        freeSlot = cursor;
                    }
                }
            }
            if (cursor == (s32)g_pops->jitEvictCursor) {
                break;
            }
            cursor++;
        }
        g_pops->jitEvictCursor = (u8)freeSlot;
        emitPtr = popsJitSpillSlot(freeSlot, emitPtr);
    }

    g_pops->jitSlotDirty[freeSlot] = isWrite;
    g_pops->jitSlotGuestReg[freeSlot] = (u8)guestReg;
    {
        u32 hostReg = g_pops->jitSlotHostReg[freeSlot];
        if ((modeFlags & 2U) != 0U) {
            if ((s32)(g_pops->jitConstValidMask << guestReg) < 0) {
                u32 knownVal = g_pops->jitConstValues[guestReg];
                if ((knownVal + 0x8000U) <= 0x27FFFU) {
                    emitPtr = popsJitEmitImm(emitPtr, (s32)hostReg, knownVal);
                    return (((u32)emitPtr) << 3) | hostReg;
                }
            }
            emitPtr = popsJitLoadGuestReg(emitPtr, (s32)hostReg, guestReg);
        }
        return (((u32)emitPtr) << 3) | hostReg;
    }
}

/* [23] 0x0016c8 */
__attribute__((noinline))
u32 *popsJitEmitCallWithDelay(u32 *emitPtr, u32 encodedTarget)
{
    u32 callWord = encodedTarget >> 2;
    s32 canDelay = popsJitPrevNotBranch((u32)emitPtr);
    u32 delayInsn = emitPtr[-1];

    g_pops->jitCachedBasePtr = 0U;
    g_pops->jitBaseGuestReg = 0U;
    if (canDelay == 0) {
        emitPtr++;
        delayInsn = 0U;
    }
    emitPtr[-1] = callWord;
    emitPtr[0] = delayInsn;
    return emitPtr + 1;
}

/* [24] 0x00171c */
__attribute__((noinline))
u32 *popsJitAcquirePair(u32 *emitPtr, s32 dstGuest, s32 srcGuest, s32 *outDst, s32 *outSrc)
{
    u8 dstMode = (dstGuest == srcGuest) ? 3U : 1U;
    u32 packedDst = popsJitAcquireReg(emitPtr, dstGuest, 1U << srcGuest, dstMode);
    u32 *curPtr = (u32 *)((packedDst >> 5) * 4U);
    s32 dstHost = (s32)(packedDst & 0x1FU);
    s32 srcHost;

    if (dstGuest == srcGuest) {
        srcHost = dstHost;
        if (popsJitPrevNotBranch((u32)emitPtr) != 0) {
            u32 prevWord = emitPtr[-1];
            if ((prevWord & 0xFC1FFFFFU) == (((u32)dstHost << 11) + 0x21U)) {
                srcHost = (s32)((prevWord >> 21) & 0x1FU);
                curPtr--;
            }
        }
    } else {
        u32 packedSrc = popsJitAcquireReg(curPtr, srcGuest, 1U << dstGuest, 2U);
        curPtr = (u32 *)((packedSrc >> 5) * 4U);
        srcHost = (s32)(packedSrc & 0x1FU);
    }

    *outDst = dstHost;
    *outSrc = srcHost;
    return curPtr;
}

/* [25] 0x00180c */
__attribute__((noinline))
u32 *popsJitEmitAluImm(u32 op, s32 dstGuest, s32 srcGuest, u32 imm, u32 *emitPtr)
{
    s32 dstHost = 0;
    s32 srcHost = 0;
    u32 *curPtr = popsJitAcquirePair(emitPtr, dstGuest, srcGuest, &dstHost, &srcHost);

    if (op == 129U) {
        *curPtr = 0x7C000420U | (((u32)srcHost & 0x1FU) << 16) | (((u32)dstHost & 0x1FU) << 11);
        return curPtr + 1;
    }

    if (op == 130U) {
        if (popsJitPrevNotBranch((u32)curPtr) != 0) {
            u32 prevWord = curPtr[-1];
            u32 hiMask = (prevWord >> 16) & 0xFC1FU;
            if (hiMask == ((u32)srcHost + 0x8C00U)
                || hiMask == ((u32)srcHost + 0x9400U)
                || hiMask == ((u32)srcHost + 0x8400U)) {
                u32 baseHost = (prevWord >> 21) & 0x1FU;
                if (dstHost == srcHost) {
                    curPtr--;
                }
                *curPtr = 0x84000000U | (baseHost << 21) | (((u32)dstHost & 0x1FU) << 16) | (prevWord & 0xFFFFU);
                return curPtr + 1;
            }
        }
        *curPtr = 0x7C000620U | (((u32)srcHost & 0x1FU) << 16) | (((u32)dstHost & 0x1FU) << 11);
        return curPtr + 1;
    }

    if (op == 128U) {
        if (imm == 0U && g_pops->jitGuestRegMap[dstGuest] < -1) {
            s32 slot;
            for (slot = 0; slot < 12; slot++) {
                if (g_pops->jitSlotGuestReg[slot] == (u8)dstGuest) {
                    g_pops->jitSlotDirty[slot] = 0U;
                    g_pops->jitSlotGuestReg[slot] = 0U;
                    break;
                }
            }
            {
                u32 fpuReg = (u32)(-g_pops->jitGuestRegMap[dstGuest]) & 0x1FU;
                *curPtr = 0x44800000U | (fpuReg << 11);
                return curPtr + 1;
            }
        }
        return popsJitEmitImm(curPtr, dstHost, imm);
    }

    if (op == 12U && (imm == 0xFFU || imm == 0xFFFFU)) {
        if (popsJitPrevNotBranch((u32)curPtr) != 0) {
            u32 prevWord = curPtr[-1];
            u32 hiMask = (prevWord >> 16) & 0xFC1FU;
            if (hiMask == ((u32)srcHost + 0x9400U)
                || hiMask == ((u32)srcHost + 0x8C00U)
                || hiMask == ((u32)srcHost + 0x8400U)) {
                u32 baseHost = (prevWord >> 21) & 0x1FU;
                u32 loadOp = (imm == 0xFFU) ? 0x90000000U : 0x94000000U;
                if (dstHost == srcHost) {
                    curPtr--;
                }
                *curPtr = loadOp | (baseHost << 21) | (((u32)dstHost & 0x1FU) << 16) | (prevWord & 0xFFFFU);
                return curPtr + 1;
            }
        }
    }

    *curPtr = (op << 26) | (((u32)srcHost & 0x1FU) << 21) | (((u32)dstHost & 0x1FU) << 16) | (imm & 0xFFFFU);
    return curPtr + 1;
}

/* [26] 0x001a94 */
__attribute__((noinline))
u32 *popsJitMoveToHost(u32 *emitPtr, s32 hostReg, s32 guestOrHost)
{
    if (hostReg == 4) {
        g_pops->jitBaseGuestReg = 0U;
    }
    if (hostReg == 2) {
        g_pops->jitCachedBasePtr = 0U;
    }
    if ((guestOrHost & 0x7F) == guestOrHost) {
        s32 foundHost = popsJitFindGuestReg(guestOrHost);
        if (foundHost < 0) {
            return popsJitLoadGuestReg(emitPtr, hostReg, guestOrHost);
        }
        if (popsJitPrevNotBranch((u32)emitPtr) != 0
            && emitPtr[-1] == ((((u32)foundHost) << 11) + (((u32)hostReg) << 21) + 0x21U)) {
            return emitPtr;
        }
        *emitPtr = (((u32)foundHost & 0x1FU) << 21) | (((u32)hostReg & 0x1FU) << 11) | 0x21U;
        return emitPtr + 1;
    }
    if ((guestOrHost & 0x7F) == hostReg) {
        return emitPtr;
    }
    *emitPtr = (((u32)guestOrHost & 0x1FU) << 21) | (((u32)hostReg & 0x1FU) << 11) | 0x21U;
    return emitPtr + 1;
}

/* [27] 0x001b80 */
__attribute__((noinline))
u32 *popsJitStoreFromV0(u32 *emitPtr, s32 guestOrHost, s32 allowDirectSpill)
{
    s32 regIdx = guestOrHost & 0x7F;
    if (regIdx != guestOrHost) {
        if (regIdx == 2) {
            return emitPtr;
        }
        *emitPtr = 0x00400021U | (((u32)guestOrHost & 0x1FU) << 11);
        return emitPtr + 1;
    }
    if (regIdx == 0) {
        return emitPtr;
    }
    {
        s32 mapped = (s32)g_pops->jitGuestRegMap[regIdx];
        if (mapped >= 0 || allowDirectSpill == 0) {
            u32 packed = popsJitAcquireReg(emitPtr, guestOrHost, 0U, 1U);
            u32 *curPtr = (u32 *)((packed >> 5) * 4U);
            *curPtr = 0x00400021U | ((packed & 0x1FU) << 11);
            return curPtr + 1;
        }
        {
            s32 slot;
            for (slot = 0; slot < 12; slot++) {
                if (g_pops->jitSlotGuestReg[slot] == (u8)regIdx) {
                    g_pops->jitSlotDirty[slot] = 0U;
                    g_pops->jitSlotGuestReg[slot] = 0U;
                    break;
                }
            }
        }
        if (mapped == -1) {
            u32 spillOff = popsSpillOffsetForGuest((u32)guestOrHost);
            *emitPtr = 0xAF820000U | (spillOff & 0xFFFFU);
        } else {
            u32 fpuReg = (u32)(-mapped) & 0x1FU;
            *emitPtr = 0x44820000U | (fpuReg << 11);
        }
        return emitPtr + 1;
    }
}

/* [28] 0x001c68 */
__attribute__((noinline))
u32 *popsJitEmitConstMemAccess(u32 *emitPtr, s32 op, s32 guestReg, u8 region, u32 addr, s32 directSpill)
{
    s32 isStore = (op & 8);
    u32 curRegion = region;
    u32 curAddr = addr;
    u32 *curPtr = emitPtr;
    s32 curOp = op;

    if (guestReg != 0 && isStore == 0 && (guestReg & 0x80) == 0) {
        g_pops->jitConstValidMask &= ~(0x80000000U >> guestReg);
    }

    if (curRegion == 0U) {
        u32 portIdx = (curAddr - 0x1F801000U) >> 3;
        u32 width = (u32)(op & 7);
        if (portIdx >= 512U) {
            portIdx = 511U;
        }
        if (width == 3U) {
            width = 2U;
        }
        curPtr = popsJitSaveAllDirty(curPtr);
        if (isStore != 0) {
            void (*writeFn)(u32, u32, u32) = g_pops->ioTable[portIdx].writeFn;
            if (writeFn == popsIoWriteSlow || writeFn != popsIoWriteDefault) {
                if (writeFn != popsIoWriteSlow) {
                    curAddr &= 0x1FFFU;
                }
                *curPtr = 0x34060000U | (width & 0xFFFFU);
                curPtr = popsJitEmitImm(curPtr + 1, 4, curAddr);
                curPtr = popsJitMoveToHost(curPtr, 5, guestReg);
                curPtr[0] = popsJitJalEncode((const void *)writeFn);
                curPtr[1] = 0xAF9901B0U;
                curPtr[2] = 0x8F9901B0U;
                curPtr += 3;
                {
                    s32 slot;
                    for (slot = 0; slot < 12; slot++) {
                        g_pops->jitSlotGuestReg[slot] = 0U;
                    }
                }
                g_pops->jitCachedBasePtr = 0U;
                g_pops->jitBaseGuestReg = 0U;
                g_pops->jitHiLoLiveMask = 0U;
                return curPtr;
            }
            curAddr &= 0x1FFFU;
        } else {
            u32 (*readFn)(u32, u32) = g_pops->ioTable[portIdx].readFn;
            if (readFn == popsIoReadSlow) {
                curRegion = 3U;
            } else {
                curAddr &= 0x1FFFU;
                if (readFn != popsIoReadDefault) {
                    *curPtr = 0x34050000U | (width & 0xFFFFU);
                    curPtr = popsJitEmitImm(curPtr + 1, 4, curAddr);
                    curPtr[0] = popsJitJalEncode((const void *)readFn);
                    curPtr[1] = 0xAF9901B0U;
                    curPtr[2] = 0x8F9901B0U;
                    curPtr += 3;
                    {
                        s32 slot;
                        for (slot = 0; slot < 12; slot++) {
                            g_pops->jitSlotGuestReg[slot] = 0U;
                        }
                    }
                    g_pops->jitCachedBasePtr = 0U;
                    g_pops->jitBaseGuestReg = 0U;
                    g_pops->jitHiLoLiveMask = 0U;
                    return popsJitStoreFromV0(curPtr, guestReg, directSpill);
                }
            }
        }
    }

    {
        u8 needReadVal = (isStore != 0 || (curOp & 0x13) == 2) ? 1U : 0U;
        s32 hostReg = guestReg & 0x7F;
        u32 phys = curAddr & 0x1FFFFFFFU;

        if (hostReg == guestReg) {
            s32 usedFpuStore = 0;
            if (curOp == 43) {
                s32 foundHost = popsJitFindGuestReg(hostReg);
                if (foundHost < -1) {
                    u32 prevWord = curPtr[-1];
                    hostReg = -foundHost;
                    if (popsJitPrevNotBranch((u32)curPtr) != 0
                        && (prevWord & 0xFFE0FFFFU) == (((u32)hostReg << 11) + 0x44800000U)) {
                        hostReg = (s32)((prevWord >> 16) & 0x1FU);
                    } else {
                        curOp = 57;
                    }
                    usedFpuStore = 1;
                }
            }
            if (usedFpuStore == 0) {
                u8 modeBits = (u8)((needReadVal * 2U) + (isStore == 0 ? 1U : 0U));
                u32 packed = popsJitAcquireReg(curPtr, guestReg, 0U, modeBits);
                curPtr = (u32 *)((packed >> 5) * 4U);
                hostReg = (s32)(packed & 0x1FU);
            }
        }

        if (curRegion == 1U) {
            u32 hostAddr = (phys & 0x1FFFFFU) + POPS_PSX_RAM_BASE;
            u32 upperHalf = (hostAddr >> 16) + (((phys & 0x8000U) != 0U) ? 1U : 0U);
            u32 baseField;
            if (upperHalf == 0x0980U) {
                baseField = 0x03A00000U;
            } else if (isStore != 0 && hostReg == 2) {
                g_pops->jitBaseGuestReg = 0U;
                *curPtr++ = 0x3C040000U | (upperHalf & 0xFFFFU);
                baseField = 0x00800000U;
            } else {
                if (g_pops->jitCachedBasePtr == 0U || g_pops->jitCachedUpperAddr != upperHalf) {
                    *curPtr++ = 0x3C020000U | (upperHalf & 0xFFFFU);
                }
                if (hostReg == 2) {
                    g_pops->jitCachedBasePtr = 0U;
                } else {
                    g_pops->jitCachedUpperAddr = upperHalf;
                    g_pops->jitCachedBasePtr = (u32)curPtr;
                }
                baseField = 0x00400000U;
            }
            *curPtr = ((u32)curOp << 26) | baseField | (((u32)hostReg & 0x1FU) << 16) | (phys & 0xFFFFU);
            return curPtr + 1;
        }

        if (curRegion < 2U) {
            u32 ioOff = (phys & 0xFFFU) + (u32)g_pops + 0x2000U;
            if (hostReg == 2) {
                g_pops->jitCachedBasePtr = 0U;
            }
            *curPtr = ((u32)curOp << 26) | 0x03800000U | (((u32)hostReg & 0x1FU) << 16) | (ioOff & 0xFFFFU);
            return curPtr + 1;
        }

        if (curRegion == 2U) {
            u32 spOff = (phys + 0x3000U) & 0xFFFFU;
            if (hostReg == 2) {
                g_pops->jitCachedBasePtr = 0U;
            }
            *curPtr = ((u32)curOp << 26) | 0x03800000U | (((u32)hostReg & 0x1FU) << 16) | spOff;
            return curPtr + 1;
        }

        if (curRegion == 3U && isStore == 0) {
            u32 width = (curOp == 0x23) ? 2U : (u32)(curOp & 7);
            u32 biosVal = popsIoReadSlow(phys, width);
            curPtr = popsJitEmitImm(curPtr, hostReg, biosVal);
            g_pops->jitConstValues[guestReg] = biosVal;
            g_pops->jitConstValidMask |= (0x80000000U >> guestReg);
            return curPtr;
        }
    }

    return curPtr;
}

/* [29] 0x0020cc */
__attribute__((noinline))
u32 *popsJitEmitMemAccess(s32 op, s32 valReg, s32 baseReg, s16 offset, u32 *emitPtr, s32 directSpill)
{
    s32 isStore = (op & 8);
    s32 curOff = offset;
    s32 curOp = op;
    u32 *curPtr = emitPtr;

    if (isStore == 0 && (valReg & 0x80) == 0 && g_pops->jitGuestRegMap[valReg] < 0) {
        s32 slot;
        for (slot = 0; slot < 12; slot++) {
            if (g_pops->jitSlotGuestReg[slot] == (u8)valReg) {
                g_pops->jitSlotDirty[slot] = 0U;
                break;
            }
        }
    }

    if ((s32)(g_pops->jitConstValidMask << baseReg) < 0) {
        u32 effAddr = g_pops->jitConstValues[baseReg] + (u32)(s32)offset;
        u32 phys = effAddr & 0x1FFFFFFFU;
        u8 region = 1U;
        s32 cycleAdj = 0;

        if ((phys >> 23) != 0U) {
            region = 2U;
            if ((effAddr - 0x1F800000U) >= 0x400U) {
                region = 3U;
                if ((phys - 0x1FC00000U) > 0x7FFFFU) {
                    region = 0U;
                }
            }
        }
        if (region == 3U) {
            cycleAdj = (curOp & 3) + (isStore != 0 ? 15 : 0);
        } else if (region == 2U && isStore == 0) {
            cycleAdj = -4;
        }
        if ((curOp & 0x10) != 0) {
            cycleAdj *= 2;
        }
        g_pops->jitPendingCycles += cycleAdj;

        if ((curOp & 0x10) != 0) {
            if ((effAddr & 3U) != 0U) {
                s32 leftOp = curOp & 0x2A;
                curOp = leftOp + 4;
                curPtr = popsJitEmitConstMemAccess(emitPtr, leftOp, valReg, region, effAddr + 3U, directSpill);
            } else {
                curOp -= 19;
            }
        }
        return popsJitEmitConstMemAccess(curPtr, curOp, valReg, region, effAddr, directSpill);
    }

    if (valReg != 0 && isStore == 0 && (valReg & 0x80) == 0) {
        g_pops->jitConstValidMask &= ~(0x80000000U >> valReg);
    }

    {
        s32 useFastBase = 0;
        if ((g_pops->quirkFlags & 0x80000U) == 0U && baseReg == 29) {
            useFastBase = 1;
        } else {
            s32 opMask = curOp & 0x0B;
            if (baseReg == (s32)g_pops->jitBaseGuestReg
                || opMask == 10
                || (opMask == 2 && (u32)emitPtr <= 0x09B7FFFFU)) {
                useFastBase = 1;
            }
        }

        if (useFastBase != 0) {
            s32 effOff = offset;
            u32 *fastPtr = emitPtr;
            s32 tempReg = 0;

            if (baseReg != (s32)g_pops->jitBaseGuestReg || (u32)emitPtr == g_pops->jitBlockStartPtr) {
                s32 baseHost = popsJitFindGuestReg(baseReg);
                if (baseHost < 0) {
                    fastPtr = popsJitLoadGuestReg(curPtr, 4, baseReg);
                    baseHost = 4;
                }
                if (baseHost != 4 || curOff != 0) {
                    *fastPtr++ = 0x24040000U | (((u32)baseHost & 0x1FU) << 21) | ((u32)curOff & 0xFFFFU);
                }
                tempReg = popsJitAllocTempReg(2, 0U);
                *fastPtr++ = 0x7C8005C0U | (((u32)tempReg & 0x1FU) << 16);
                effOff = 0;
                g_pops->jitCachedSpOffset = curOff;
                g_pops->jitBaseGuestReg = (u8)baseReg;
            } else {
                s32 delta = (s32)offset - g_pops->jitCachedSpOffset;
                if (delta == (s32)(s16)delta) {
                    effOff = delta;
                } else {
                    *fastPtr++ = 0x24840000U | ((u32)(-g_pops->jitCachedSpOffset) & 0xFFFFU);
                    g_pops->jitCachedSpOffset = 0;
                }
            }

            {
                s32 valHost = valReg & 0x7F;
                if (valHost != valReg) {
                    if (valHost == 2) {
                        g_pops->jitCachedBasePtr = 0U;
                    }
                } else if (isStore != 0) {
                    s32 fpuMapped = 0;
                    if (curOp == 43) {
                        s32 found = popsJitFindGuestReg(valReg);
                        if (found < -1) {
                            curOp = 57;
                            valHost = -found;
                            fpuMapped = 1;
                        }
                    }
                    if (fpuMapped == 0) {
                        u32 packed = popsJitAcquireReg(fastPtr, valReg, 1U << baseReg, 2U);
                        fastPtr = (u32 *)((packed >> 5) * 4U);
                        valHost = (s32)(packed & 0x1FU);
                    }
                } else if ((curOp & 0x13) == 2) {
                    u32 packed = popsJitAcquireReg(fastPtr, valReg, 1U << baseReg, 3U);
                    fastPtr = (u32 *)((packed >> 5) * 4U);
                    valHost = (s32)(packed & 0x1FU);
                } else if (directSpill != 0 && curOp == 35 && g_pops->jitGuestRegMap[valReg] < -1) {
                    s32 slot;
                    for (slot = 0; slot < 12; slot++) {
                        if (g_pops->jitSlotGuestReg[slot] == (u8)valReg) {
                            g_pops->jitSlotDirty[slot] = 0U;
                            g_pops->jitSlotGuestReg[slot] = 0U;
                            break;
                        }
                    }
                    curOp = 49;
                    valHost = -g_pops->jitGuestRegMap[valReg];
                } else {
                    u32 packed = popsJitAcquireReg(fastPtr, valReg, 1U << baseReg, 1U);
                    fastPtr = (u32 *)((packed >> 5) * 4U);
                    valHost = (s32)(packed & 0x1FU);
                }

                if (tempReg > 0) {
                    fastPtr[0] = 0x3406004CU;
                    fastPtr[1] = 0x7CC4EA84U;
                    fastPtr[2] = 0x54000001U | (((u32)tempReg & 0x1FU) << 21);
                    fastPtr[3] = 0x7CC4E7C4U;
                    fastPtr += 4;
                    popsJitFreeTempReg(tempReg);
                }

                if ((curOp & 0x11) == 0x10) {
                    if (baseReg != 29 || (((g_pops->jitCachedSpOffset + effOff) & 3) != 0)) {
                        u32 leftOp = (u32)(isStore + 34);
                        u32 rightOp = (u32)(isStore + 38);
                        *fastPtr++ = (leftOp << 26) | 0x00800000U | (((u32)valHost & 0x1FU) << 16) | ((u32)(effOff + 3) & 0xFFFFU);
                        *fastPtr++ = (rightOp << 26) | 0x00800000U | (((u32)valHost & 0x1FU) << 16) | ((u32)effOff & 0xFFFFU);
                    } else {
                        u32 wordOp = (u32)(isStore + 35);
                        *fastPtr++ = (wordOp << 26) | 0x00800000U | (((u32)valHost & 0x1FU) << 16) | ((u32)effOff & 0xFFFFU);
                    }
                } else {
                    s32 fused = 0;
                    if (curOp == 35 && popsJitPrevNotBranch((u32)fastPtr) != 0) {
                        u32 prevWord = fastPtr[-1];
                        if ((prevWord & 0xFFE0FFFFU) == (0xAC800000U | ((u32)effOff & 0xFFFFU))) {
                            s32 prevRt = (s32)((prevWord >> 16) & 0x1FU);
                            if (prevRt != valHost) {
                                *fastPtr++ = (((u32)prevRt) << 21) | (((u32)valHost & 0x1FU) << 11) | 0x21U;
                            }
                            fused = 1;
                        }
                    }
                    if (fused == 0) {
                        *fastPtr++ = ((u32)curOp << 26) | 0x00800000U | (((u32)valHost & 0x1FU) << 16) | ((u32)effOff & 0xFFFFU);
                    }
                }

                if (valReg != baseReg && isStore == 0) {
                    g_pops->jitBaseGuestReg = (u8)baseReg;
                }
                return fastPtr;
            }
        }
    }

    /* Slow / general memory access helper call path */
    {
        s32 rewrotePrev = 0;
        if (isStore == 0 && baseReg == valReg && popsJitPrevNotBranch((u32)curPtr) != 0 && (curOp & 0x13) != 2) {
            s32 found = popsJitFindGuestReg(valReg);
            if (found >= 0) {
                u32 prevWord = curPtr[-1];
                u32 prevOp = prevWord >> 26;
                if ((prevWord & 0xFC00FFC0U) == ((u32)found << 11)
                    || (((prevWord >> 16) & 0x1FU) == (u32)found
                        && ((prevOp - 8U) < 8U || (prevOp - 32U) < 7U))) {
                    if ((prevOp & 0x1FU) != 0U) {
                        curPtr[-1] = (prevWord & 0xFFE0FFFFU) | 0x00040000U;
                    } else {
                        curPtr[-1] = (prevWord & 0xFFFF07FFU) | 0x00002000U;
                    }
                    rewrotePrev = 1;
                }
            }
        }
        if (rewrotePrev == 0) {
            curPtr = popsJitMoveToHost(curPtr, 4, baseReg);
        }
    }

    if (curOff != 0) {
        u32 prevWord = curPtr[-1];
        if ((prevWord & 0xFC1FFFFFU) == 0x00002021U) {
            curPtr[-1] = 0x24040000U | (((prevWord >> 21) & 0x1FU) << 21) | ((u32)curOff & 0xFFFFU);
            curOff = 0;
        } else if ((prevWord & 0xFFE0FFFFU) == 0x00002021U) {
            curPtr[-1] = 0x24040000U | (((prevWord >> 16) & 0x1FU) << 21) | ((u32)curOff & 0xFFFFU);
            curOff = 0;
        }
    }

    if (isStore != 0) {
        curPtr = popsJitMoveToHost(curPtr, 5, valReg);
    }
    if ((curOp & 3) != 2) {
        curPtr = popsJitFlushSlots(curPtr, 11);
    }
    if (curOff != 0) {
        *curPtr++ = 0x24840000U | ((u32)curOff & 0xFFFFU);
    }

    switch (curOp) {
        case 32:
            curPtr = popsJitEmitCallWithDelay(curPtr, popsJitJalEncode((const void *)popsMemReadByte) << 2);
            return popsJitStoreFromV0(curPtr, valReg, directSpill);
        case 33:
            curPtr = popsJitEmitCallWithDelay(curPtr, popsJitJalEncode((const void *)popsMemReadHalf) << 2);
            return popsJitStoreFromV0(curPtr, valReg, directSpill);
        case 35:
        case 39:
            curPtr = popsJitEmitCallWithDelay(curPtr, popsJitJalEncode((const void *)popsMemReadWord) << 2);
            return popsJitStoreFromV0(curPtr, valReg, directSpill);
        case 36:
            curPtr = popsJitEmitCallWithDelay(curPtr, popsJitJalEncode((const void *)popsMemReadByteUnsigned) << 2);
            return popsJitStoreFromV0(curPtr, valReg, directSpill);
        case 37:
            curPtr = popsJitEmitCallWithDelay(curPtr, popsJitJalEncode((const void *)popsMemReadHalfUnsigned) << 2);
            return popsJitStoreFromV0(curPtr, valReg, directSpill);
        case 40:
            return popsJitEmitCallWithDelay(curPtr, popsJitJalEncode((const void *)popsMemWriteByte) << 2);
        case 41:
            return popsJitEmitCallWithDelay(curPtr, popsJitJalEncode((const void *)popsMemWriteHalf) << 2);
        case 43:
            return popsJitEmitCallWithDelay(curPtr, popsJitJalEncode((const void *)popsMemWriteWord) << 2);
        case 34:
        case 38:
        case 54: {
            u8 acqMode = ((curOp & 0x10) != 0) ? 1U : 3U;
            u32 packed = popsJitAcquireReg(curPtr, valReg, 0U, acqMode);
            u32 hostReg = packed & 0x1FU;
            u32 *unalignPtr = (u32 *)((packed >> 5) * 4U);
            unalignPtr = popsJitEmitCallWithDelay(unalignPtr, popsJitJalEncode((const void *)popsMemTranslateUnaligned) << 2);
            if ((curOp & 0x10) != 0) {
                unalignPtr[0] = 0x88800003U | (hostReg << 16);
                unalignPtr[1] = 0x98800000U | (hostReg << 16);
                return unalignPtr + 2;
            }
            unalignPtr[0] = ((u32)curOp << 26) | 0x00800000U | (hostReg << 16);
            return unalignPtr + 1;
        }
        default:
            return popsJitStoreFromV0(curPtr, valReg, directSpill);
    }
}

/* [30] 0x0028e0 */
__attribute__((noinline))
u32 *popsJitEmitExitBranch(u32 *emitPtr, u32 **patchSlot, s32 cycles, u32 target)
{
    u32 *curPtr = popsJitSaveAllDirty(emitPtr);
    u32 delayWord;

    if (cycles != 0 || popsJitPrevNotBranch((u32)curPtr) == 0) {
        s32 clamped = (cycles > 0) ? cycles : 0;
        delayWord = 0x27390000U | ((u32)(-clamped) & 0xFFFFU);
    } else {
        curPtr--;
        delayWord = *curPtr;
    }

    curPtr[1] = delayWord;
    *patchSlot = curPtr;
    curPtr[0] = (target + 0x30000000U) >> 2;
    return curPtr + 2;
}

/* [31] 0x00297c */
__attribute__((noinline))
u32 *popsJitEmitCondBranch(u32 branchOp, s32 regA, s32 regB, s16 cycles, u32 **patchSlot, u32 *emitPtr)
{
    u32 *curPtr = emitPtr;
    s32 hostA;
    s32 hostB = 0;

    if (regA >= 0) {
        u32 packedA = popsJitAcquireReg(curPtr, regA, 1U << regB, 2U);
        u32 *nextPtr = (u32 *)((packedA >> 5) * 4U);
        u32 packedB = popsJitAcquireReg(nextPtr, regB, 1U << regA, 2U);
        hostA = (s32)(packedA & 0x1FU);
        curPtr = (u32 *)((packedB >> 5) * 4U);
        hostB = (s32)(packedB & 0x1FU);
    } else {
        s32 slot;
        s32 found = -1;
        for (slot = 0; slot < 12; slot++) {
            if (g_pops->jitSlotGuestReg[slot] >= 32U) {
                found = slot;
                break;
            }
        }
        if (found >= 0) {
            hostA = (s32)g_pops->jitSlotHostReg[found];
            g_pops->jitSlotDirty[found] = 0U;
        } else {
            g_pops->jitBaseGuestReg = 0U;
            hostA = 4;
            *curPtr++ = 0x8F8401A0U;
        }
    }

    curPtr = popsJitSaveAllDirty(curPtr);
    *patchSlot = curPtr;

    if (branchOp >= 0xC2U && branchOp < 0xC6U) {
        *curPtr = ((branchOp - 190U) << 26) | (((u32)hostA & 0x1FU) << 21) | (((u32)hostB & 0x1FU) << 16);
    } else {
        u32 condBit = (branchOp ^ 1U) & 1U;
        *curPtr = 0x04000000U | (((u32)hostA & 0x1FU) << 21) | (condBit << 16);
    }

    curPtr++;
    if (regA < 0) {
        popsJitFreeTempReg(hostA);
    }
    *curPtr = 0x27390000U | ((u32)(-cycles) & 0xFFFFU);
    return curPtr + 1;
}

/* [32] 0x002b18 */
__attribute__((noinline))
u32 *popsJitEmitCycleSub(s32 cycles, u32 *emitPtr)
{
    if (cycles == 0 || cycles < 0) {
        return emitPtr;
    }
    *emitPtr = 0x27390000U | ((u32)(-(s16)cycles) & 0xFFFFU);
    return emitPtr + 1;
}

/* [33] 0x002b40 */
__attribute__((noinline))
u32 *popsJitEmitCopRead(u32 *emitPtr, s32 dstReg, s32 copOffset)
{
    s32 hostReg = dstReg & 0x7F;
    u32 *curPtr = emitPtr;
    s32 effOff = copOffset;

    if (hostReg == dstReg) {
        u32 packed = popsJitAcquireReg(curPtr, dstReg, 0U, 1U);
        curPtr = (u32 *)((packed >> 5) * 4U);
        hostReg = (s32)(packed & 0x1FU);
    }

    if (effOff == 0x3C) {
        effOff = 56;
    }
    if (effOff == 0x70) {
        effOff = 116;
    }
    if (effOff == 116) {
        curPtr = popsJitEmitCallWithDelay(curPtr, (u32)(uintptr_t)popsGteReadRgb + 0x30000000U);
        *curPtr = 0x00800021U | (((u32)hostReg & 0x1FU) << 11);
        return curPtr + 1;
    }
    if (effOff == 252) {
        s32 tempReg = popsJitAllocTempReg(4, 0U);
        *curPtr = 0x4860000FU | (((u32)hostReg & 0x1FU) << 16);
        curPtr = popsJitEmitImm(curPtr + 1, tempReg, 0x7F87E000U);
        curPtr[0] = (((u32)hostReg & 0x1FU) << 21) | (((u32)tempReg & 0x1FU) << 16) | (((u32)tempReg & 0x1FU) << 11) | 0x24U;
        curPtr[1] = (((u32)tempReg & 0x1FU) << 16) | (((u32)tempReg & 0x1FU) << 11) | 0x2BU;
        curPtr[2] = 0x7C00FFC4U | (((u32)tempReg & 0x1FU) << 21) | (((u32)hostReg & 0x1FU) << 16);
        popsJitFreeTempReg(tempReg);
        return curPtr + 3;
    }

    if (popsJitPrevNotBranch((u32)curPtr) != 0) {
        u32 prevWord = curPtr[-1];
        if ((prevWord & 0xFFE0FFFFU) == (0xAF800000U | ((u32)effOff & 0xFFFFU))) {
            u32 prevRt = (prevWord >> 16) & 0x1FU;
            *curPtr = (prevRt << 21) | (((u32)hostReg & 0x1FU) << 11) | 0x21U;
            return curPtr + 1;
        }
        if ((prevWord & 0xFFE0FFFFU) == (0xE7800000U | ((u32)effOff & 0xFFFFU))) {
            u32 prevFt = (prevWord >> 16) & 0x1FU;
            *curPtr = 0x44000000U | (prevFt << 16) | (((u32)hostReg & 0x1FU) << 11);
            return curPtr + 1;
        }
    }

    *curPtr = 0x8F800000U | (((u32)hostReg & 0x1FU) << 16) | ((u32)effOff & 0xFFFFU);
    return curPtr + 1;
}

/* [34] 0x002cdc */
__attribute__((noinline))
u32 *popsJitEmitCopWrite(u8 kind, s32 srcReg, u32 copOffset, u32 *emitPtr)
{
    u32 *curPtr = emitPtr;
    u32 effOff = copOffset;
    s32 curSrc = srcReg;

    if (kind == 3U) {
        return curPtr;
    }

    /* COP0 SR (offset 0x130 = 304) */
    if (effOff == 304U) {
        s32 hostReg = curSrc & 0x7F;
        if (hostReg == curSrc) {
            u32 packed = popsJitAcquireReg(curPtr, curSrc, 0U, 2U);
            curPtr = (u32 *)((packed >> 5) * 4U);
            hostReg = (s32)(packed & 0x1FU);
        }
        if (curSrc == 0x80) {
            curSrc = 0;
        }
        if ((curSrc & 0x80) == 0 && (s32)(g_pops->jitConstValidMask << curSrc) < 0) {
            u32 constVal = g_pops->jitConstValues[curSrc];
            if ((constVal & 1U) == 0U) {
                if ((constVal & 0xF27DFF3FU) != constVal) {
                    curPtr = popsJitEmitImm(curPtr, 5, constVal & 0xF27DFF3FU);
                    hostReg = 5;
                }
                *curPtr = 0xAF800130U | (((u32)hostReg & 0x1FU) << 16);
                return curPtr + 1;
            }
        }
        curPtr = popsJitEmitImm(curPtr, 5, 0xF27DFF3FU);
        *curPtr++ = 0x00052824U | (((u32)hostReg & 0x1FU) << 21);
        *curPtr++ = 0xAF850130U;
        curPtr = popsJitFlushSlots(curPtr, 11);
        curPtr[0] = popsJitJalEncode((const void *)popsCpuCheckIrq);
        curPtr[1] = 0xAF9901B0U;
        curPtr[2] = 0x8F9901B0U;
        return curPtr + 3;
    }

    /* COP0 CAUSE (offset 0x134 = 308) */
    if (effOff == 308U) {
        s32 hostReg = curSrc & 0x7F;
        if (hostReg == curSrc) {
            u32 packed = popsJitAcquireReg(curPtr, curSrc, 0U, 2U);
            curPtr = (u32 *)((packed >> 5) * 4U);
            hostReg = (s32)(packed & 0x1FU);
        }
        *curPtr++ = 0x83850135U;
        if (hostReg != 0) {
            *curPtr++ = 0x7C060A00U | (((u32)hostReg & 0x1FU) << 21);
            hostReg = 6;
        }
        *curPtr++ = 0x7C050804U | (((u32)hostReg & 0x1FU) << 21);
        *curPtr++ = 0xA3850135U;
        if (hostReg != 0) {
            curPtr = popsJitFlushSlots(curPtr, 11);
            curPtr[0] = popsJitJalEncode((const void *)popsCpuCheckIrq);
            curPtr[1] = 0xAF9901B0U;
            curPtr[2] = 0x8F9901B0U;
            return curPtr + 3;
        }
        return curPtr;
    }

    /* GTE SXYP (offset 60) shifts SXY FIFO */
    if (effOff == 60U) {
        curPtr[0] = 0x8F860034U;
        curPtr[1] = 0x8F850038U;
        curPtr[2] = 0xAF860030U;
        curPtr[3] = 0xAF850034U;
        curPtr += 4;
        effOff = 56U;
    }

    {
        s32 hostReg = curSrc & 0x7F;
        if (hostReg != curSrc) {
            *curPtr++ = 0xAF800000U | (((u32)hostReg & 0x1FU) << 16) | (effOff & 0xFFFFU);
        } else {
            s32 emitStandard = 1;
            if (kind == 1U) {
                if (!((s32)(g_pops->jitConstValidMask << hostReg) < 0
                      && g_pops->jitConstValues[hostReg] == (u32)(s32)(s16)g_pops->jitConstValues[hostReg])) {
                    s32 tempReg = popsJitAllocTempReg(4, 0U);
                    s32 found = popsJitFindGuestReg(hostReg);
                    if (found < 0) {
                        if (found != -1) {
                            curPtr = popsJitLoadGuestReg(curPtr, tempReg, hostReg);
                            *curPtr++ = 0x7C000620U | (((u32)tempReg & 0x1FU) << 16) | (((u32)tempReg & 0x1FU) << 11);
                        } else {
                            u32 spillOff = popsSpillOffsetForGuest((u32)hostReg & 0x7FU);
                            *curPtr++ = 0x87800000U | (((u32)tempReg & 0x1FU) << 16) | (spillOff & 0xFFFFU);
                        }
                    } else {
                        *curPtr++ = 0x7C000620U | (((u32)found & 0x1FU) << 16) | (((u32)tempReg & 0x1FU) << 11);
                    }
                    *curPtr++ = 0xAF800000U | (((u32)tempReg & 0x1FU) << 16) | (effOff & 0xFFFFU);
                    popsJitFreeTempReg(tempReg);
                    emitStandard = 0;
                }
            } else if (kind >= 2U) {
                if (kind != 2U) {
                    return curPtr;
                }
                {
                    u32 packed = popsJitAcquireReg(curPtr, hostReg, 0U, 2U);
                    curPtr = (u32 *)((packed >> 5) * 4U);
                    hostReg = (s32)(packed & 0x1FU);
                    *curPtr++ = 0xA7800000U | (((u32)hostReg & 0x1FU) << 16) | (effOff & 0xFFFFU);
                    emitStandard = 0;
                }
            }

            if (emitStandard != 0) {
                s32 found = popsJitFindGuestReg(curSrc);
                if (found < -1 && effOff != 120U) {
                    u32 prevWord = curPtr[-1];
                    s32 canFuse = popsJitPrevNotBranch((u32)curPtr);
                    u32 storeOp = 57U;
                    hostReg = -found;
                    if (canFuse != 0 && (prevWord & 0xFFE0FFFFU) == (((u32)hostReg << 11) + 0x44800000U)) {
                        hostReg = (s32)((prevWord >> 16) & 0x1FU);
                        storeOp = 43U;
                    }
                    *curPtr++ = (storeOp << 26) | 0x03800000U | (((u32)hostReg & 0x1FU) << 16) | (effOff & 0xFFFFU);
                } else {
                    u32 packed = popsJitAcquireReg(curPtr, curSrc, 0U, 2U);
                    curPtr = (u32 *)((packed >> 5) * 4U);
                    hostReg = (s32)(packed & 0x1FU);
                    *curPtr++ = 0xAF800000U | (((u32)hostReg & 0x1FU) << 16) | (effOff & 0xFFFFU);
                }
            }
        }

        if (effOff == 120U) {
            curPtr[0] = 0x00002827U | (((u32)hostReg & 0x1FU) << 16);
            curPtr[1] = 0x00A0282CU | (((u32)hostReg & 0x1FU) << 16);
            curPtr[2] = 0x00A02816U;
            curPtr[3] = 0xAF85007CU;
            return curPtr + 4;
        }
        if (effOff == 112U) {
            return popsJitEmitCallWithDelay(curPtr, popsJitJalEncode((const void *)popsGteWriteIrgb) << 2);
        }
        return curPtr;
    }
}

/* [35] 0x003180 */
__attribute__((noinline))
u32 *popsJitEmitBlockChecksum(u32 guestPc, u32 *emitPtr, s32 insnCount)
{
    s32 limit = g_pops->jitChecksumLimit;
    s32 count;

    if ((g_pops->jitCompileFlags & 0x4000U) != 0U) {
        limit = 0xFFFF;
    }
    count = (limit < insnCount) ? limit : insnCount;

    if (count < 3) {
        u32 checkPc = guestPc;
        if (insnCount >= 3) {
            const u32 *guestWords = (const u32 *)(g_pops->jitGuestMemBias + g_pops->jitBlockStartPc);
            if ((guestWords[0] & 0xFFFF8000U) == 0x27BD8000U) {
                checkPc += 4U;
                if (insnCount >= 4 && (guestWords[1] & 0xFFFF8000U) == 0xAFBF0000U) {
                    checkPc += 4U;
                }
            }
        }
        {
            u32 hostAddr = (checkPc & 0x1FFFFFU) | POPS_PSX_RAM_BASE;
            u32 nextAddr = hostAddr + 4U;
            u32 hiCur = (hostAddr >> 16) + (((hostAddr & 0x8000U) != 0U) ? 1U : 0U);
            u32 hiNext = (nextAddr >> 16) + (((nextAddr & 0x8000U) != 0U) ? 1U : 0U);
            const u32 *ramWords = (const u32 *)hostAddr;
            u32 sumVal = ramWords[0] + ramWords[1];
            u32 baseReg = 29U;

            if (hiCur != 0x0980U) {
                *emitPtr++ = 0x3C020000U | (hiCur & 0xFFFFU);
                baseReg = 2U;
                g_pops->jitCachedBasePtr = (u32)emitPtr;
                g_pops->jitCachedUpperAddr = hiCur;
            }
            if (hiCur == hiNext) {
                *emitPtr++ = 0x8C030000U | (baseReg << 21) | (nextAddr & 0xFFFFU);
            } else {
                *emitPtr++ = 0x3C030000U | (hiNext & 0xFFFFU);
                *emitPtr++ = 0x8C630000U | (nextAddr & 0xFFFFU);
            }
            *emitPtr++ = 0x8C040000U | (baseReg << 21) | (hostAddr & 0xFFFFU);
            if (sumVal != 0U) {
                emitPtr = popsJitEmitImm(emitPtr, 5, sumVal);
            }
            *emitPtr++ = 0x00832021U;
            {
                s32 branchDelta = (s32)(g_pops->jitChecksumFailTarget - (u32)emitPtr - 4U) >> 2;
                u32 branchOp = (sumVal == 0U) ? 0x14800000U : 0x14850000U;
                emitPtr[0] = branchOp | ((u32)branchDelta & 0xFFFFU);
                emitPtr[1] = 0U;
                emitPtr += 2;
                g_pops->jitBlockStartPtr = (u32)emitPtr;
                return emitPtr;
            }
        }
    } else {
        const u32 *ramWords = (const u32 *)((g_pops->jitBlockStartPc & 0x1FFFFFU) | POPS_PSX_RAM_BASE);
        u32 sumVal = 0U;
        s32 idx;
        for (idx = 0; idx < count; idx++) {
            sumVal += ramWords[idx];
        }
        {
            u32 tailCount = (u32)(count & 3);
            u32 startHost = ((g_pops->jitBlockStartPc & 0x1FFFFFU) | POPS_PSX_RAM_BASE) + (tailCount * 4U);
            u32 *curPtr = popsJitEmitImm(emitPtr, 4, startHost);
            u32 loadIdx = 0U;
            if (tailCount != 0U) {
                s32 negOff = -4;
                do {
                    loadIdx++;
                    *curPtr++ = 0x8C800000U | ((loadIdx & 0x1FU) << 16) | ((u32)negOff & 0xFFFFU);
                    negOff -= 4;
                } while (loadIdx != tailCount);
            }
            {
                s32 bulkCount = count - (s32)loadIdx;
                if (bulkCount != 0) {
                    *curPtr++ = 0x24850000U | ((u32)(bulkCount * 4) & 0xFFFFU);
                }
            }
            curPtr = popsJitEmitImm(curPtr, 6, sumVal);
            {
                u32 subIdx;
                for (subIdx = 0U; subIdx < tailCount; subIdx++) {
                    *curPtr++ = 0x00C03023U | (((subIdx + 1U) & 0x1FU) << 16);
                }
                if ((u32)count == tailCount) {
                    s32 branchDelta = (s32)(g_pops->jitChecksumFailTarget - (u32)curPtr - 4U) >> 2;
                    curPtr[0] = 0x14C00000U | ((u32)branchDelta & 0xFFFFU);
                    curPtr[1] = 0U;
                    return curPtr + 2;
                }
                return popsJitEmitCallWithDelay(curPtr, popsJitJalEncode((const void *)popsJitVerifyChecksum) << 2);
            }
        }
    }
}

/* [36] 0x003454 */
__attribute__((noinline))
u32 *popsJitInvalidateBlock(void)
{
    if ((g_pops->jitModeFlags & 1) != 0) {
        popsJitFlushCodeCache();
    } else {
        u32 *lookupPtr = popsRamLookupSlot(g_pops->pc);
        u32 *codePtr = (u32 *)(*lookupPtr);
        if (codePtr != (u32 *)0) {
            u32 spanWords = codePtr[-1] >> 2;
            u32 idx;
            for (idx = 0U; idx < spanWords; idx++) {
                lookupPtr[idx] = 0U;
            }
            codePtr[0] = popsJitJumpEncode((const void *)popsJitLinkPatchEntry);
            codePtr[1] = 0x8F8401A0U;
            {
                u32 endAddr = (u32)(codePtr + 2);
                u32 lineAddr = ((u32)codePtr & 0xFFFFFFC0U) | (endAddr & 0x3FU);
                for (;;) {
                    popsCacheInstHitInv((const void *)lineAddr);
                    popsCacheDataHitWriteback((const void *)lineAddr);
                    if (lineAddr == endAddr) {
                        break;
                    }
                    lineAddr += 64U;
                }
            }
        }
    }
    return popsJitRecompile(g_pops->pc);
}

/* [37] 0x003500 */
__attribute__((noinline))
u32 *popsJitTrackConst(s32 guestReg, u32 constVal, u32 *emitPtr, PopsJitIrNode *node)
{
    s32 targetReg = guestReg;
    u32 trackedVal = constVal;
    u32 *curPtr = emitPtr;
    s32 skipEmit = 0;

    if ((s32)(g_pops->jitConstValidMask << targetReg) < 0
        && g_pops->jitConstValues[targetReg] == trackedVal) {
        return curPtr;
    }

    if (node != (PopsJitIrNode *)0 && node->flags < 0) {
        PopsJitIrNode *nextNode = node + 1;
        u8 nextOp = nextNode->opcode;
        u8 nextClass = (u8)(nextOp & 0xF8U);

        if (nextNode->dstReg == (u8)targetReg
            && nextNode->srcRegA == (u8)targetReg
            && (nextOp == 9U || nextOp == 13U)) {
            s16 nextImm = (s16)(nextNode->rawInsnOrTarget & 0xFFFFU);
            if (nextOp == 9U) {
                trackedVal += (u32)(s32)nextImm;
            } else {
                trackedVal |= (u32)(u16)nextImm;
            }
            nextNode->kind = 0U;
        } else if (nextClass == 32U && (nextOp & 3U) != 2U) {
            if (nextNode->dstReg == (u8)targetReg && nextNode->srcRegA == (u8)targetReg) {
                skipEmit = 1;
            } else if (nextNode->flags < 0) {
                PopsJitIrNode *thirdNode = node + 2;
                if (thirdNode->dstReg == (u8)targetReg) {
                    u8 thirdOp = thirdNode->opcode;
                    if (thirdOp == 15U && nextNode->srcRegA == (u8)targetReg) {
                        targetReg = (s32)nextNode->dstReg;
                        nextNode->srcRegA = nextNode->dstReg;
                        skipEmit = 1;
                    }
                    if ((thirdOp & 0xF8U) == 32U && (thirdOp & 3U) != 2U) {
                        skipEmit = 1;
                    }
                }
            }
        }
    }

    if (skipEmit == 0) {
        curPtr = popsJitEmitAluImm(0x80U, targetReg, 0, trackedVal, curPtr);
    }
    g_pops->jitConstValues[targetReg] = trackedVal;
    g_pops->jitConstValidMask |= (0x80000000U >> targetReg);
    return curPtr;
}

/* [38] 0x0036a0 */
__attribute__((noinline))
u32 *popsJitEmitBranchComparePreload(PopsJitIrNode *node, u32 *emitPtr)
{
    s32 regA = (s32)node->srcRegA;
    s32 regB = (s32)node->srcRegB;
    s32 tempHost = popsJitAllocTempReg(4, 1U);
    u32 *curPtr;

    if ((regA != 0) & (regB != 0)) {
        u32 packedA = popsJitAcquireReg(emitPtr, regA, 1U << regB, 2U);
        u32 hostA = packedA & 0x1FU;
        u32 *midPtr = (u32 *)((packedA >> 5) * 4U);
        u32 packedB = popsJitAcquireReg(midPtr, regB, 1U << regA, 2U);
        u32 hostB = packedB & 0x1FU;
        curPtr = (u32 *)((packedB >> 5) * 4U);
        *curPtr++ = (hostA << 21) | (hostB << 16) | (((u32)tempHost & 0x1FU) << 11) | 0x26U;
    } else {
        curPtr = popsJitMoveToHost(emitPtr, tempHost, regA | regB);
    }

    if (tempHost == 4) {
        *curPtr++ = 0xAF8401A0U;
    }
    return curPtr;
}

/* [39] 0x003790 */
__attribute__((noinline))
s32 popsJitDecodePass(PopsJitIrNode *startNode)
{
    PopsJitIrNode *curNode = startNode;
    u32 curPc = popsNodePc(curNode);
    u32 windowLimit = curPc + 0x1000U;
    s16 curFlags;

    if (g_pops->jitBlockLimitPc < windowLimit) {
        windowLimit = g_pops->jitBlockLimitPc;
    }
    curFlags = curNode->flags;

    for (;;) {
        if ((curFlags & 9) != 0) {
            windowLimit = curPc + 0x1000U;
            if (g_pops->jitBlockLimitPc < windowLimit) {
                windowLimit = g_pops->jitBlockLimitPc;
            }
        }
        {
            u32 rawInsn = *(const u32 *)(g_pops->jitGuestMemBias + curPc);
            u8 instCost = 1U;
            u16 irKind = 0U;
            u32 op = rawInsn >> 26;
            u32 rs = (rawInsn >> 21) & 0x1FU;
            u32 rd = (rawInsn >> 11) & 0x1FU;
            u32 rt = (rawInsn >> 16) & 0x1FU;

            curNode->rawInsnOrTarget = rawInsn;
            if (curPc == (g_pops->jitBreakAddress & 0xFFFFFFFEU)) {
                curFlags = (s16)(curFlags | 0x1000);
            }
            if (op == 0U) {
                op = (rawInsn & 0x3FU) | 0x40U;
            }

            switch (op) {
                case 1:
                    if ((rawInsn & 0xE0000U) != 0U) {
                        irKind = (curPc == g_pops->jitBlockStartPc) ? 15U : 5U;
                        curFlags = (s16)(curFlags | 0x10);
                        rt = 0U;
                        rs = 0U;
                    } else {
                        if ((rawInsn & 0x100000U) != 0U) {
                            curNode->dstReg = 31U;
                        }
                        op = (((rawInsn & 0x10000U) != 0U) ? 2U : 3U) + 190U;
                        rt = 0U;
                    }
                    break;

                case 2:
                    rt = 0U;
                    rs = 0U;
                    break;

                case 3:
                    curNode->dstReg = 31U;
                    rt = 0U;
                    rs = 0U;
                    break;

                case 4:
                case 5:
                    op += 190U;
                    break;

                case 6:
                case 7:
                    rt = 0U;
                    op += 190U;
                    break;

                case 8:
                    op |= 1U;
                    irKind = (rt == 0U) ? 19U : 9U;
                    curNode->dstReg = (u8)rt;
                    rt = 0U;
                    break;

                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                    irKind = (rt == 0U) ? 19U : 9U;
                    curNode->dstReg = (u8)rt;
                    rt = 0U;
                    break;

                case 15:
                    rs = 0U;
                    op |= 1U;
                    irKind = (rt == 0U) ? 19U : 9U;
                    curNode->dstReg = (u8)rt;
                    rt = 0U;
                    break;

                case 16:
                    curNode->copReg = (u8)(rd + 64U);
                    if (rs == 4U) {
                        irKind = 10U;
                        if (rd == 12U && (curFlags & 1) == 0 && ((curPc >> 29) & 3U) != 0U) {
                            curNode[1].kind = 5U;
                            curNode[1].flags = (s16)(curNode[1].flags | 0x10);
                        }
                    } else if (rs == 0U) {
                        irKind = (rt == 0U) ? 19U : 6U;
                        curNode->dstReg = (u8)rt;
                        rt = 0U;
                    } else if (rs == 16U) {
                        irKind = 7U;
                        rt = 0U;
                        curNode->dstReg = 0U;
                    } else {
                        irKind = 19U;
                        rt = 0U;
                        curNode->dstReg = 0U;
                    }
                    rs = 0U;
                    break;

                case 18: {
                    instCost = 3U;
                    if (rs == 0U || rs == 2U) {
                        if (rs == 2U) {
                            rd += 32U;
                        }
                        if (rt != 0U) {
                            irKind = 6U;
                            curFlags = (s16)(curFlags | 0x40);
                            curNode->dstReg = (u8)rt;
                        } else {
                            irKind = 19U;
                            curNode->dstReg = 0xFFU;
                        }
                        rt = 0U;
                    } else if (rs == 4U || rs == 6U) {
                        if (rs == 6U) {
                            rd += 32U;
                        }
                        irKind = 10U;
                    } else {
                        rt = 0U;
                        if (((rawInsn >> 21) & 0x10U) != 0U
                            && g_gteCycleTable[rawInsn & 0x3FU] != 1U) {
                            instCost = g_gteCycleTable[rawInsn & 0x3FU];
                            irKind = 18U;
                            curFlags = (s16)(curFlags | 0x4000);
                            curNode->dstReg = 0xFFU;
                        } else {
                            curNode->dstReg = 0U;
                            irKind = 19U;
                            instCost = 1U;
                        }
                    }
                    rs = 0U;
                    curNode->copReg = (u8)rd;
                    break;
                }

                case 32:
                case 33:
                case 34:
                case 35:
                case 36:
                case 37:
                case 38:
                    curNode->dstReg = (u8)rt;
                    if (rt != 0U) {
                        curFlags = (s16)(curFlags | 0x40);
                    }
                    instCost = 5U;
                    irKind = 16U;
                    rt = 0U;
                    break;

                case 40:
                case 41:
                case 42:
                case 43:
                case 46:
                    if ((g_pops->copZeroRegs[12] & 0x10000U) != 0U) {
                        irKind = 19U;
                        rt = 0U;
                        rs = 0U;
                        curNode->dstReg = 0U;
                    } else {
                        irKind = 16U;
                    }
                    break;

                case 48:
                case 56:
                    irKind = 19U;
                    rt = 0U;
                    rs = 0U;
                    curNode->dstReg = 0U;
                    break;

                case 50:
                    curNode->copReg = (u8)rt;
                    instCost = 8U;
                    rt = 0U;
                    irKind = 8U;
                    break;

                case 58:
                    curNode->copReg = (u8)rt;
                    instCost = 4U;
                    rt = 0U;
                    irKind = 4U;
                    break;

                case 64:
                case 66:
                case 67:
                    curNode->dstReg = (u8)rd;
                    irKind = (rd == 0U) ? 19U : 17U;
                    break;

                case 68:
                case 70:
                case 71:
                case 97:
                case 99:
                case 100:
                case 101:
                case 102:
                case 103:
                case 106:
                case 107:
                    curNode->dstReg = (u8)rd;
                    irKind = (rd == 0U) ? 19U : 13U;
                    break;

                case 72:
                case 73:
                    if (op == 73U) {
                        curNode->dstReg = (u8)rd;
                    }
                    if (curNode[1].kind == 0U) {
                        if ((g_pops->jitCompileFlags & 2U) == 0U && curNode->dstReg != 0U) {
                            curNode[2].flags = (s16)(curNode[2].flags | 8);
                        } else {
                            curNode[1].flags = (s16)(curNode[1].flags | 0x10);
                        }
                    }
                    irKind = 1U;
                    rt = 0U;
                    curNode[1].flags = (s16)(curNode[1].flags | 1);
                    break;

                case 76:
                case 77:
                    irKind = 15U;
                    curFlags = (s16)(curFlags | 0x10);
                    rt = 0U;
                    rs = 0U;
                    break;

                case 80:
                case 82:
                    rt = 0U;
                    rs = 0U;
                    curNode->dstReg = (u8)rd;
                    irKind = (rd == 0U) ? 19U : 2U;
                    break;

                case 81:
                case 83:
                    irKind = 11U;
                    rt = 0U;
                    break;

                case 88:
                case 89:
                    instCost = 12U;
                    irKind = 3U;
                    break;

                case 90:
                case 91:
                    instCost = 35U;
                    irKind = 3U;
                    break;

                case 96:
                case 98:
                    op |= 1U;
                    curNode->dstReg = (u8)rd;
                    irKind = (rd == 0U) ? 19U : 13U;
                    break;

                default:
                    irKind = (curPc == g_pops->jitBlockStartPc) ? 15U : 5U;
                    curFlags = (s16)(curFlags | 0x10);
                    rt = 0U;
                    rs = 0U;
                    break;
            }

            curNode->flags = curFlags;
            curNode->srcRegB = (u8)rt;
            curNode->srcRegA = (u8)rs;
            curNode->opcode = (u8)op;
            curNode->instCycles = (u8)(g_pops->jitBaseInstCycles + instCost);

            if (irKind != 0U) {
                curNode->kind = irKind;
            } else {
                /* Branch or jump decoding */
                s32 offsetWords = (s32)(s16)rawInsn;
                u32 basePc = curPc + 4U;
                u32 targetPc;
                s32 isCond = 0;

                if (op < 192U) {
                    offsetWords = (s32)(rawInsn & 0x03FFFFFFU);
                    basePc = curPc & 0xF0000000U;
                }
                targetPc = basePc + ((u32)offsetWords * 4U);

                if (op >= 192U && ((op & 1U) != 0U || rs != rt)) {
                    u32 fallPc = curPc + 8U;
                    u32 *fallSlot = popsLookupSlotForPc(fallPc);
                    isCond = 1;
                    if (fallSlot != (u32 *)0 && *fallSlot != 0U) {
                        curNode[2].kind = 5U;
                        curNode[2].flags = (s16)(curNode[2].flags | 0x10);
                    }
                }

                curNode->rawInsnOrTarget = targetPc;

                if (targetPc == curPc + 8U && curNode->dstReg == 0U) {
                    curNode->kind = 19U;
                    curNode->dstReg = 0U;
                } else if (curNode[1].kind != 0U && isCond != 0) {
                    curNode->kind = 5U;
                    curNode->flags = (s16)(curNode->flags | 0x10);
                } else {
                    PopsJitIrNode *targetNode = (PopsJitIrNode *)0;
                    if ((g_pops->jitCompileFlags & 2U) != 0U || curNode->dstReg == 0U) {
                        if (isCond == 0 && (curNode[1].flags & 8) == 0) {
                            curNode[1].flags = (s16)(curNode[1].flags | 0x10);
                        }
                    } else {
                        curNode[2].flags = (s16)(curNode[2].flags | 8);
                    }
                    curNode->kind = (isCond != 0) ? 12U : 14U;
                    curNode[1].flags = (s16)(curNode[1].flags | 1);

                    if ((g_pops->jitCompileFlags & 8U) == 0U
                        && targetPc >= g_pops->jitBlockStartPc
                        && targetPc < g_pops->jitBlockLimitPc) {
                        u32 *targetSlot = popsLookupSlotForPc(targetPc);
                        if (targetSlot == (u32 *)0 || *targetSlot == 0U) {
                            PopsJitIrNode *cand = popsNodeForPc(targetPc);
                            s16 candFlags = cand->flags;
                            if (!((candFlags & 1) != 0 && cand[-1].kind == 12U) && (candFlags & 0x10) == 0) {
                                curNode->flags = (s16)(curNode->flags | 4);
                                if ((candFlags & 8) != 0) {
                                    cand->flags = (s16)(candFlags & (s16)0xFCFF);
                                } else {
                                    u16 existingKind = cand->kind;
                                    cand->flags = (cand < curNode) ? (s16)(candFlags | 0x108) : (s16)0x208;
                                    if (existingKind == 0U) {
                                        targetNode = cand;
                                    }
                                }
                            }
                        }
                    }

                    if (targetNode != (PopsJitIrNode *)0) {
                        popsJitDecodePass(targetNode);
                    }
                }
            }

            if ((curNode->flags & 0x10) != 0) {
                break;
            }
            curPc += 4U;
            curNode++;
            if (curPc >= windowLimit
                || (((curPc >> 23) & 0x3FU) == 0U && *popsRamLookupSlot(curPc) != 0U)) {
                if ((curNode->flags & 1) == 0) {
                    curNode->kind = 5U;
                    curNode->flags = (s16)(curNode->flags | 0x10);
                }
            }
            if (curNode->kind != 0U) {
                break;
            }
            curFlags = curNode->flags;
        }
    }

    if (curNode > &POPS_IR_NODES[1]) {
        PopsJitIrNode *backNode = curNode - 1;
        do {
            backNode--;
            if (backNode->kind == 0U) {
                break;
            }
            if (backNode->kind != 16U && backNode->kind != 19U) {
                u16 hiHalf = (u16)(backNode->rawInsnOrTarget >> 16);
                if (hiHalf != 0x27BD && hiHalf != 0x2402) {
                    break;
                }
            }
            backNode->flags = (s16)(backNode->flags | 0x80);
        } while (backNode != POPS_IR_NODES);
    }

    if (g_pops->jitIrEndPtr < (u32)curNode) {
        g_pops->jitIrEndPtr = (u32)curNode;
        return 1;
    }
    return 0;
}

/* [40] 0x003efc */
__attribute__((noinline))
u32 *popsJitRecompile(u32 guestPc)
{
    u32 curPc = guestPc;
    u32 phys = curPc & 0x1FFFFFFFU;
    u32 isBios;
    u32 *poolPtr;
    u16 compileFlags;
    u16 baseInstCycles;

    if ((phys >> 23) != 0U && (phys - 0x1FC00000U) > 0x7FFFFU) {
        g_pops->copZeroRegs[8] = curPc;
        popsCpuRaiseException(4U);
        curPc = g_pops->pc;
        phys = curPc & 0x1FFFFFFFU;
    }

    isBios = ((phys >> 23) != 0U) ? 1U : 0U;
    if (isBios != 0U) {
        g_pops->jitGuestMemBias = (u32)g_psxBiosRom - (curPc & 0xFFF80000U);
        compileFlags = 0x8000U;
        baseInstCycles = 2U;
        poolPtr = (u32 *)g_pops->jitBiosCodePtr;
    } else {
        poolPtr = (u32 *)g_pops->jitRamCodePtr;
        g_pops->jitGuestMemBias = POPS_PSX_RAM_BASE - (curPc & 0xFFE00000U);
        if ((u32)poolPtr > POPS_JIT_CODE_LIMIT) {
            poolPtr = (u32 *)popsJitFlushCodeCache();
        }
        compileFlags = (g_pops->jitModeFlags == -1) ? 3U : (u16)g_pops->jitModeFlags;
        if (curPc == g_pops->jitWatchAddress) {
            compileFlags = 0x4007U;
        }
        baseInstCycles = (((curPc >> 29) & 3U) == 0U) ? 0U : 5U;
    }

    g_pops->jitCompileFlags = compileFlags;
    {
        u32 limitPc = curPc + ((compileFlags & 4U) != 0U ? 0x2000U : 0x0C00U);
        if (curPc < g_pops->jitSplitAddress && g_pops->jitSplitAddress < limitPc) {
            limitPc = g_pops->jitSplitAddress;
        }
        g_pops->jitBaseInstCycles = baseInstCycles;
        g_pops->jitBlockLimitPc = limitPc;
    }

    g_pops->jitIrEndPtr = (u32)POPS_IR_NODES;
    g_pops->jitBlockStartPc = curPc;
    popsJitDecodePass(POPS_IR_NODES);

    {
        PopsJitIrNode *endNode = ((PopsJitIrNode *)g_pops->jitIrEndPtr) + 1;
        u32 endByteOff = (u32)((endNode - 1) - POPS_IR_NODES) * 4U;
        u32 endPage = ((g_pops->jitBlockStartPc + endByteOff) >> 16) & 0x1FU;
        u32 startPage = (g_pops->jitBlockStartPc >> 16) & 0x1FU;

        g_pops->jitPendingCycles = 0;
        g_pops->jitDirtyPageMask |= (2U << endPage) - (1U << startPage);

        /* Pass 1: hazard scheduling and basic-block cycle accounting */
        {
            PopsJitIrNode *scan = POPS_IR_NODES;
            PopsJitIrNode *segStart = POPS_IR_NODES;
            s32 accumCycles = 0;
            u32 segCount = 0U;

            while (scan < endNode) {
                s16 flags = scan->flags;
                if (scan->kind != 0U) {
                    if (((flags & 0x11) | (scan[1].flags & 8)) == 0) {
                        flags = (s16)(flags | (s16)0x8000);
                        scan->flags = flags;
                    }
                    segCount++;
                    {
                        s32 nodeCycles = (s32)scan->instCycles;
                        s32 baseCost = (s32)g_pops->jitBaseInstCycles;
                        if (nodeCycles > baseCost + 1 && (flags & 1) == 0) {
                            u32 op = scan->opcode;
                            s32 latency = nodeCycles - baseCost;
                            if ((op - 32U) < 7U && baseCost == 0) {
                                PopsJitIrNode *look = scan + 1;
                                u8 loadDst = scan->dstReg;
                                while (look < endNode) {
                                    if (look->kind == 16U) {
                                        break;
                                    }
                                    if (look != scan + 1
                                        && (loadDst == look->srcRegA || loadDst == look->srcRegB)) {
                                        break;
                                    }
                                    latency -= (s32)look->instCycles;
                                    if (latency < 2) {
                                        latency = 1;
                                        break;
                                    }
                                    if ((look->flags & 1) != 0) {
                                        latency >>= 1;
                                        break;
                                    }
                                    look++;
                                }
                            } else if ((flags & 0x4000) != 0) {
                                PopsJitIrNode *back = scan;
                                while ((back->flags & 0x2000) == 0) {
                                    back->flags = (s16)(back->flags | 0x2000);
                                    if (back == POPS_IR_NODES) {
                                        break;
                                    }
                                    back--;
                                    if ((s8)back->dstReg > 0 && back->copReg == 63U) {
                                        break;
                                    }
                                    if ((back->flags & 0x11) != 0) {
                                        break;
                                    }
                                }
                                {
                                    PopsJitIrNode *look = scan + 1;
                                    while (look < endNode) {
                                        if ((look->opcode == 18U && look->dstReg != 0U) || look->opcode == 58U) {
                                            break;
                                        }
                                        latency -= (s32)look->instCycles;
                                        if (latency < 2) {
                                            latency = 1;
                                            break;
                                        }
                                        if ((look->flags & 1) != 0) {
                                            latency >>= 1;
                                            break;
                                        }
                                        look++;
                                    }
                                }
                            } else if ((op - 88U) < 4U) {
                                PopsJitIrNode *look = scan + 1;
                                while (look < endNode && look->opcode != 0x50U && look->opcode != 0x52U) {
                                    latency -= (s32)look->instCycles;
                                    if ((look->opcode - 88U) < 4U || latency < 2) {
                                        latency = 1;
                                        break;
                                    }
                                    if ((look->flags & 1) != 0) {
                                        latency >>= 1;
                                        break;
                                    }
                                    look++;
                                }
                            }
                            if ((g_pops->quirkFlags & 0x10000000U) != 0U) {
                                latency = 1;
                            }
                            nodeCycles = latency + baseCost;
                        }

                        accumCycles += nodeCycles;
                        if ((flags & 0x19) != 0) {
                            s32 carryCycles = ((flags & 8) != 0) ? nodeCycles : 0;
                            if (g_pops->jitBaseInstCycles == 0U) {
                                s32 needCachePenalty = 0;
                                if ((flags & 0x10) != 0) {
                                    needCachePenalty = 1;
                                } else if ((flags & 1) != 0) {
                                    if ((flags & 4) == 0 || scan >= popsNodeForPc(scan->rawInsnOrTarget)) {
                                        if ((u32)((const u8 *)scan - (const u8 *)segStart) >= 0x4001U) {
                                            needCachePenalty = 1;
                                        }
                                    }
                                }
                                if (needCachePenalty != 0) {
                                    s32 penalty = (s32)(segCount >> (u8)g_pops->blockCycleShift);
                                    if (segStart == POPS_IR_NODES) {
                                        g_pops->jitPendingCycles = penalty;
                                        penalty = 0;
                                    }
                                    accumCycles = (s32)(s16)(accumCycles + penalty);
                                    segStart = scan + 1;
                                    segCount = 0U;
                                }
                            }
                            scan->cycles = (u16)(scan->cycles + (accumCycles - carryCycles));
                            accumCycles = carryCycles;
                        }
                    }
                } else if (segCount == 0U) {
                    segStart = scan;
                }
                scan++;
            }
        }

        /* Pass 2: reset register allocator and emit prologue */
        popsCacheCreateDirty(poolPtr + 15);
        g_pops->jitBlockStartPtr = 0U;
        g_pops->jitCachedBasePtr = 0U;
        g_pops->jitCachedSpOffset = 0;
        g_pops->jitCachedUpperAddr = 0U;
        g_pops->jitBaseGuestReg = 0U;
        g_pops->jitHiLoLiveMask = 0U;
        g_pops->jitHiLoDirtyMask = 0U;
        g_pops->jitEvictCursor = 0U;
        {
            s32 idx;
            static const u8 hostRegSlots[12] = { 8, 9, 10, 11, 12, 13, 14, 15, 24, 3, 1, 7 };
            for (idx = 0; idx < 12; idx++) {
                g_pops->jitSlotDirty[idx] = 0U;
                g_pops->jitSlotGuestReg[idx] = 0U;
                g_pops->jitSlotHostReg[idx] = hostRegSlots[idx];
            }
            for (idx = 0; idx < 32; idx++) {
                g_pops->jitGuestRegMap[idx] = g_jitDefaultRegMap[idx];
            }
        }

        {
            u32 *hdrPtr = poolPtr;
            u32 *codeStart;
            u32 *emitPtr;
            u32 ramEmptyConstMask = (((g_pops->jitBlockStartPc >> 23) & 0x3FU) == 0U) ? 0U : 0x80000000U;

            if ((g_pops->jitCompileFlags & 0x8000U) == 0U
                && (g_pops->jitChecksumFailTarget + 0x1FFD0U) < (u32)poolPtr) {
                poolPtr[0] = popsJitJalEncode((const void *)popsJitInvalidateEntry);
                poolPtr[1] = 0U;
                g_pops->jitChecksumFailTarget = (u32)poolPtr;
                hdrPtr = poolPtr + 2;
            }
            if ((g_pops->jitCompileFlags & 0x8001U) != 0U) {
                hdrPtr--;
            }

            codeStart = hdrPtr + 1;
            emitPtr = codeStart;
            g_pops->jitBlockStartPtr = (u32)codeStart;
            g_pops->jitConstValidMask = 0x80000000U;
            g_pops->jitConstValues[0] = 0U;

            if ((s16)g_pops->jitCompileFlags >= 0) {
                emitPtr = popsJitEmitBlockChecksum(g_pops->jitBlockStartPc, emitPtr, (s32)(endNode - POPS_IR_NODES));
            }

            /* Pass 3: emit each IR node */
            {
                PopsJitIrNode *curNode = POPS_IR_NODES;
                while (curNode < endNode) {
                    u16 curKind = curNode->kind;
                    if (curKind == 0U) {
                        curNode++;
                        continue;
                    }

                    popsCacheCreateDirty(emitPtr + 15);

                    if ((curNode->flags & 8) != 0) {
                        s32 totalCycles = (s32)curNode->cycles + g_pops->jitPendingCycles;
                        if (totalCycles > 0) {
                            emitPtr = popsJitEmitCycleSub(totalCycles, emitPtr);
                        }
                        g_pops->jitConstValidMask = 0x80000000U;
                        g_pops->jitPendingCycles = 0;
                        g_pops->jitConstValues[0] = 0U;
                        emitPtr = popsJitFlushSlots(emitPtr, 11);
                        g_pops->jitBlockStartPtr = (u32)emitPtr;
                        *(u32 *)&curNode->kind = (u32)emitPtr;
                    } else if (g_pops->jitConstValidMask == ramEmptyConstMask) {
                        s32 slot;
                        u32 busy = 0U;
                        for (slot = 0; slot < 12; slot++) {
                            if (g_pops->jitSlotGuestReg[slot] != 0U) {
                                busy = 1U;
                                break;
                            }
                        }
                        if (busy == 0U) {
                            busy = g_pops->jitCachedBasePtr | g_pops->jitBaseGuestReg | g_pops->jitHiLoDirtyMask;
                        }
                        if (busy == 0U) {
                            curNode->flags = (s16)(curNode->flags | 8);
                            *(u32 *)&curNode->kind = (u32)emitPtr;
                        }
                    }

                    if (curNode->flags < 0 && (curNode->flags & 0x4000) == 0 && curKind != 16U) {
                        u8 nextSrcA = curNode[1].srcRegA;
                        u8 nextSrcB = curNode[1].srcRegB;
                        if (!(curNode[1].kind == 13U && (nextSrcA == 0U || nextSrcB == 0U))) {
                            u8 curDst = curNode->dstReg;
                            if (nextSrcB == curDst) {
                                nextSrcB = 0U;
                            }
                            if (nextSrcA == curDst) {
                                nextSrcA = 0U;
                            }
                            if (nextSrcA != 0U || nextSrcB != 0U) {
                                u8 curSrcA = curNode->srcRegA;
                                u8 curSrcB = curNode->srcRegB;
                                if (curSrcB != 0U) {
                                    u32 packed = popsJitAcquireReg(emitPtr, (s32)curSrcB, (1U << curSrcA) | (1U << nextSrcB), 2U);
                                    emitPtr = (u32 *)((packed >> 5) * 4U);
                                }
                                if (curSrcA != 0U) {
                                    u32 packed = popsJitAcquireReg(emitPtr, (s32)curSrcA, (1U << curSrcB) | (1U << nextSrcB), 2U);
                                    emitPtr = (u32 *)((packed >> 5) * 4U);
                                }
                                if (nextSrcB != 0U) {
                                    u32 packed = popsJitAcquireReg(emitPtr, (s32)nextSrcB, (1U << curSrcB) | (1U << curSrcA), 2U);
                                    emitPtr = (u32 *)((packed >> 5) * 4U);
                                }
                                if (nextSrcA != 0U) {
                                    u32 packed = popsJitAcquireReg(emitPtr, (s32)nextSrcA, (1U << curSrcB) | (1U << curSrcA), 2U);
                                    emitPtr = (u32 *)((packed >> 5) * 4U);
                                }
                            }
                        }
                    }

                    if ((curNode->flags & 0x1000) != 0) {
                        u32 breakAddr = g_pops->jitBreakAddress;
                        s32 tempReg = popsJitAllocTempReg(4, 0U);
                        u32 rtBits = ((u32)tempReg & 0x1FU) << 16;
                        emitPtr[0] = 0x34000001U | rtBits;
                        emitPtr[1] = 0xA78001C0U | rtBits;
                        emitPtr += 2;
                        if ((breakAddr & 1U) != 0U) {
                            emitPtr[0] = 0x8F8001ACU | rtBits;
                            emitPtr[1] = 0x00190023U | (((u32)tempReg & 0x1FU) << 21) | (((u32)tempReg & 0x1FU) << 11);
                            emitPtr[2] = 0x34190000U;
                            emitPtr[3] = 0xAF8001ACU | rtBits;
                            emitPtr += 4;
                        }
                        popsJitFreeTempReg(tempReg);
                    }

                    if ((curNode[1].flags & 1) == 0) {
                        if ((curNode->flags & 0x40) != 0) {
                            u8 curDst = curNode->dstReg;
                            if ((curNode[1].srcRegA == curDst || curNode[1].srcRegB == curDst)
                                && curNode->flags < 0) {
                                if ((curNode[2].flags & 1) != 0) {
                                    if (curNode[1].opcode >= 0xC0U) {
                                        emitPtr = popsJitEmitBranchComparePreload(&curNode[1], emitPtr);
                                        curNode[1].flags = (s16)(curNode[1].flags | 0x20);
                                    }
                                } else if (curNode[1].dstReg != curDst) {
                                    emitPtr = popsJitEmitIrNode((s32)curNode[1].kind, &curNode[1], emitPtr, 0);
                                    curNode[1].kind = 0U;
                                }
                            }
                        }
                        emitPtr = popsJitEmitIrNode((s32)curKind, curNode, emitPtr, 0);
                        curNode++;
                    } else {
                        /* Branch/jump with delay slot in curNode[1] */
                        u32 nodePc = popsNodePc(curNode);
                        u8 linkReg = curNode->dstReg;
                        s32 delayMode = 0;

                        if ((curNode[1].flags & 0x40) != 0 && curKind != 1U) {
                            u8 delayDst = curNode[1].dstReg;
                            u32 targetWord = *(const u32 *)(g_pops->jitGuestMemBias + curNode->rawInsnOrTarget);
                            if ((targetWord >> 16) == (delayDst + 0x4880U)) {
                                delayMode = 1;
                            } else if (((targetWord >> 26) & 0x38U) == 0x28U) {
                                u32 targetRs = (targetWord >> 21) & 0x1FU;
                                u32 targetRt = (targetWord >> 16) & 0x1FU;
                                if (targetRt == delayDst || targetRs == delayDst) {
                                    delayMode = 1;
                                }
                            }
                        }

                        if ((curNode[2].flags & 1) != 0 && curNode->rawInsnOrTarget == curNode[1].rawInsnOrTarget) {
                            if (curKind == 1U) {
                                curNode[2].flags = (s16)(curNode[2].flags - 1);
                                curNode[1].kind = 0U;
                            } else {
                                delayMode = -1;
                            }
                        }

                        {
                            s32 skipBranchEmit = 0;
                            if (curKind == 12U && (curNode->flags & 0x20) == 0) {
                                u8 srcA = curNode->srcRegA;
                                u8 srcB = curNode->srcRegB;
                                if (((s32)(g_pops->jitConstValidMask << srcB) & (s32)(g_pops->jitConstValidMask << srcA)) < 0) {
                                    u8 branchOp = curNode->opcode;
                                    s32 valA = (s32)g_pops->jitConstValues[srcA];
                                    s32 taken;
                                    if ((branchOp & 6U) == 2U) {
                                        taken = (valA == (s32)g_pops->jitConstValues[srcB]) ? 1 : 0;
                                    } else if ((branchOp & 6U) >= 3U) {
                                        taken = ((branchOp & 6U) == 4U) ? (valA < 1 ? 1 : 0) : 1;
                                    } else {
                                        taken = ((branchOp & 6U) == 0U) ? (valA >= 0 ? 1 : 0) : 1;
                                    }
                                    curKind = 14U;
                                    if (taken == (s32)(branchOp & 1U)) {
                                        curNode[1].flags = (s16)(curNode[1].flags - 1);
                                        if ((curNode->flags & 4) != 0) {
                                            PopsJitIrNode *tNode = popsNodeForPc(curNode->rawInsnOrTarget);
                                            if ((tNode->flags & 0x200) != 0) {
                                                tNode->flags = (s16)(tNode->flags & (s16)0xFDF7);
                                            }
                                        }
                                        curNode++;
                                        skipBranchEmit = 1;
                                    }
                                } else {
                                    u8 delayDst = curNode[1].dstReg;
                                    if ((delayDst != 0U && (delayDst == srcA || delayDst == srcB))
                                        || (linkReg != 0U && (linkReg == srcA || linkReg == srcB))) {
                                        emitPtr = popsJitEmitBranchComparePreload(curNode, emitPtr);
                                        curNode->flags = (s16)(curNode->flags | 0x20);
                                    }
                                }
                            } else if (curKind == 1U) {
                                u8 jrReg = curNode->srcRegA;
                                if ((s32)(g_pops->jitConstValidMask << jrReg) < 0
                                    && ((((nodePc >> 23) & 0x3FU) == 0U)
                                        || (((g_pops->jitConstValues[jrReg] >> 23) & 0x3FU) != 0U))) {
                                    curKind = 14U;
                                    curNode->rawInsnOrTarget = g_pops->jitConstValues[jrReg] & 0xFFFFFFFCU;
                                } else {
                                    emitPtr = popsJitEmitCopWrite(0U, (s32)jrReg, 0x1A0U, emitPtr);
                                }
                                if ((curNode[2].flags & 1) != 0) {
                                    curNode[2].flags = (s16)(curNode[2].flags - 1);
                                    delayMode = 1;
                                }
                            }

                            if (skipBranchEmit == 0) {
                                if (linkReg != 0U) {
                                    emitPtr = popsJitTrackConst((s32)linkReg, nodePc + 8U, emitPtr, (PopsJitIrNode *)0);
                                }
                                if (delayMode == 0) {
                                    emitPtr = popsJitEmitIrNode((s32)curNode[1].kind, &curNode[1], emitPtr, 0);
                                }
                                if ((curNode[2].flags & 1) == 0) {
                                    emitPtr = popsJitEmitIrNode((s32)curKind, curNode, emitPtr, (s32)curNode[1].cycles);
                                }
                                if (delayMode == 0 && (curNode[1].flags & 8) == 0) {
                                    curNode += 2;
                                } else {
                                    curNode++;
                                    if (delayMode < 0) {
                                        curNode++;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            /* Pass 4: finalize, patch internal branches, register sub-block entries, clear IR */
            emitPtr = popsJitFlushSlots(emitPtr, 11);
            g_pops->jitBlockStartPtr = (u32)emitPtr;

            {
                PopsJitIrNode *patchNode = POPS_IR_NODES;
                while (patchNode < endNode) {
                    s16 pFlags = patchNode->flags;
                    if ((pFlags & 2) != 0) {
                        u32 *branchPtr = *(u32 **)&patchNode->srcRegA;
                        u32 branchWord = *branchPtr;
                        PopsJitIrNode *targetIr = popsNodeForPc(patchNode->rawInsnOrTarget);
                        u32 *targetCode = *(u32 **)&targetIr->kind;
                        if ((branchWord >> 27) == 1U) {
                            *branchPtr = (targetCode == branchPtr)
                                ? 0U
                                : ((branchWord & 0xFC000000U) | popsJitJumpWord((u32)targetCode));
                        } else {
                            s32 relDelta = (s32)((const u8 *)targetCode - (const u8 *)branchPtr - 4) >> 2;
                            *branchPtr = (branchWord & 0xFFFF0000U) | ((u32)relDelta & 0xFFFFU);
                        }
                    }

                    if ((g_pops->jitCompileFlags & 0x8001U) != 0U && (pFlags & 0x18) == 8) {
                        u32 entryPc = popsNodePc(patchNode);
                        u32 *entryCode = *(u32 **)&patchNode->kind;
                        if ((g_pops->jitCompileFlags & 2U) != 0U) {
                            s32 remainInsns = (s32)(endNode - patchNode);
                            popsCacheCreateDirty(emitPtr + 15);
                            if ((g_pops->jitCompileFlags & 0x10U) != 0U) {
                                remainInsns = 2;
                            }
                            {
                                u32 *guardStart = emitPtr;
                                if ((g_pops->jitCompileFlags & 0x8000U) == 0U
                                    && (g_pops->jitChecksumFailTarget + 0x1FFD0U) < (u32)emitPtr) {
                                    emitPtr[0] = popsJitJalEncode((const void *)popsJitInvalidateEntry);
                                    emitPtr[1] = 0U;
                                    g_pops->jitChecksumFailTarget = (u32)emitPtr;
                                    guardStart = emitPtr + 2;
                                }
                                g_pops->jitConstValidMask = 0x80000000U;
                                g_pops->jitConstValues[0] = 0U;
                                emitPtr = popsJitEmitBlockChecksum(entryPc, guardStart, remainInsns);
                                emitPtr = popsJitEmitCallWithDelay(emitPtr, (u32)entryCode + 0x30000000U);
                                entryCode = guardStart;
                            }
                        }
                        *popsRegionLookupSlot(entryPc, ((entryPc >> 23) & 0x3FU) != 0U ? 1U : 0U) = (u32)entryCode;
                    }

                    popsClearQuadVfpu(patchNode);
                    patchNode++;
                }
                popsClearQuadVfpu(patchNode);
            }

            if (g_pops->jitBaseInstCycles != 0U) {
                g_pops->cycleRemaining -= (s32)(endNode - POPS_IR_NODES);
            }
            if ((g_pops->jitCompileFlags & 0x8001U) == 0U) {
                *hdrPtr = (u32)((endNode - POPS_IR_NODES) * 4);
            }

            {
                u32 *poolSlot = isBios ? &g_pops->jitBiosCodePtr : &g_pops->jitRamCodePtr;
                u32 prevPool = *poolSlot;
                u32 endAddr = (u32)emitPtr;
                u32 lineAddr = (prevPool & 0xFFFFFFC0U) | (endAddr & 0x3FU);
                *poolSlot = endAddr;
                for (;;) {
                    popsCacheInstHitInv((const void *)lineAddr);
                    popsCacheDataHitWriteback((const void *)lineAddr);
                    if (lineAddr == endAddr) {
                        break;
                    }
                    lineAddr += 64U;
                }
            }

            *popsRegionLookupSlot(curPc, isBios) = (u32)codeStart;
            if (curPc == 0x80000080U) {
                g_pops->jitExceptionHandler = (u32)codeStart;
            }
            popsCacheInstHitFill(codeStart);
            return codeStart;
        }
    }
}

/* [41] 0x004dc0 */
__attribute__((noinline))
u32 *popsJitEmitBlockLink(u32 targetPc, u32 *emitPtr)
{
    u32 effPc = targetPc;
    u32 *slotPtr = popsLookupSlotForPc(effPc);
    u32 targetCode = 0U;

    if (slotPtr == (u32 *)0 || (s32)(targetCode = *slotPtr) < 0) {
        targetCode = *popsRamLookupSlot(0x00000600U);
        effPc = 0x00000600U;
    }

    {
        u32 *curPtr = popsJitSaveAllDirty(emitPtr);
        u32 *afterImm = popsJitEmitImm(curPtr, 4, effPc);
        u32 *delaySlotPtr = afterImm - 1;
        u32 savedImmInsn = *delaySlotPtr;
        s32 needTrampoline = 0;

        if (((u32)delaySlotPtr - g_pops->jitRamExitTrampoline) <= 0x1FFFFU) {
            s32 delta = ((s32)(g_pops->jitRamExitTrampoline - (u32)delaySlotPtr) >> 2) - 1;
            *delaySlotPtr = 0x1B200000U | ((u32)delta & 0xFFFFU);
        } else if (((u32)delaySlotPtr - g_pops->jitBiosExitTrampoline) <= 0x1FFFFU) {
            s32 delta = ((s32)(g_pops->jitBiosExitTrampoline - (u32)delaySlotPtr) >> 2) - 1;
            *delaySlotPtr = 0x1B200000U | ((u32)delta & 0xFFFFU);
        } else {
            *delaySlotPtr = 0x1B200003U;
            needTrampoline = 1;
        }

        if (targetCode == 0U) {
            targetCode = (u32)popsJitLinkPatchEntry;
        }

        delaySlotPtr[1] = savedImmInsn;
        delaySlotPtr[2] = (targetCode + 0x30000000U) >> 2;
        delaySlotPtr[3] = 0xAF8401A0U;
        curPtr = delaySlotPtr + 4;

        if (needTrampoline != 0) {
            if ((u32)curPtr <= 0x09B7FFFFU) {
                g_pops->jitRamExitTrampoline = (u32)curPtr;
            } else {
                g_pops->jitBiosExitTrampoline = (u32)curPtr;
            }
            curPtr[0] = popsJitJalEncode((const void *)popsJitEnter);
            curPtr[1] = 0xAF8401A0U;
            curPtr += 2;
        }
        return curPtr;
    }
}

/* [42] 0x004f6c */
__attribute__((noinline))
u32 *popsJitEmitIrNode(s32 kind, PopsJitIrNode *node, u32 *emitPtr, s32 delayCycles)
{
    s32 totalCycles = delayCycles + g_pops->jitPendingCycles;
    if (totalCycles < 2) {
        totalCycles = 2;
    }

    switch (kind) {
        case 1: {
            const void *dispatchFn = ((g_pops->quirkFlagsExt & 0x400) != 0)
                ? (const void *)popsJitDispatchAlt
                : (const void *)popsJitDispatch;
            u32 *res = popsJitEmitExitBranch(emitPtr, (u32 **)&node->srcRegA, totalCycles, (u32)dispatchFn);
            g_pops->jitPendingCycles = 0;
            return res;
        }

        case 2:
            node->srcRegA = node->dstReg;
            /* fall through into HI/LO handler */
        case 11: {
            u8 op = node->opcode;
            s32 regIdx = (s32)node->srcRegA;
            s32 isLo = ((op & 2U) != 0U) ? 1 : 0;
            u8 bitMask = (u8)(isLo + 1);
            u32 regOff = (isLo != 0) ? 424U : 420U;

            if ((op & 1U) != 0U) {
                g_pops->jitHiLoDirtyMask &= (u8)(~bitMask);
                g_pops->jitHiLoLiveMask &= (u8)(~bitMask);
                return popsJitEmitCopWrite(0U, regIdx, regOff, emitPtr);
            }
            if ((g_pops->jitHiLoLiveMask & bitMask) != 0U) {
                u32 packed = popsJitAcquireReg(emitPtr, regIdx, 0U, 1U);
                u32 hostReg = packed & 0x1FU;
                u32 *curPtr = (u32 *)((packed >> 5) * 4U);
                *curPtr++ = (hostReg << 11) | (isLo != 0 ? 0x12U : 0x10U);
                if ((g_pops->jitHiLoDirtyMask & bitMask) != 0U) {
                    g_pops->jitHiLoDirtyMask = (u8)(g_pops->jitHiLoDirtyMask - bitMask);
                    *curPtr++ = 0xAF800000U | (hostReg << 16) | regOff;
                }
                return curPtr;
            }
            return popsJitEmitCopRead(emitPtr, regIdx, (s32)regOff);
        }

        case 3:
            g_pops->jitHiLoDirtyMask = 3U;
            g_pops->jitHiLoLiveMask = 3U;
            /* fall through into ALU R-type */
        case 13: {
            s32 srcA = (s32)node->srcRegA;
            s32 srcB = (s32)node->srcRegB;
            s32 dstReg = (s32)node->dstReg;
            u32 mask = g_pops->jitConstValidMask;
            u32 bothConst = (((u32)((s32)(mask << srcA) & (s32)(mask << srcB))) >> 31);
            u32 foldVal = 0U;

            if (bothConst != 0U) {
                u32 valA = g_pops->jitConstValues[srcA];
                u32 valB = g_pops->jitConstValues[srcB];
                u8 op = node->opcode;
                if (valA == 0U || valB == 0U) {
                    bothConst |= 2U;
                }
                if (op == 97U) {
                    foldVal = valA + valB;
                } else if (op == 99U) {
                    foldVal = valA - valB;
                } else if (op == 100U) {
                    foldVal = valA & valB;
                } else if (op == 101U) {
                    foldVal = valA | valB;
                } else {
                    bothConst = 0U;
                }
            }

            if ((bothConst & 2U) != 0U) {
                return popsJitTrackConst(dstReg, foldVal, emitPtr, node);
            }

            {
                u32 funct = (u32)node->opcode & 0xFFFFFFBFU;
                s32 foundDst = popsJitFindGuestReg(dstReg);
                u32 *curPtr = emitPtr;

                if (foundDst < -1 && (funct == 0x21U || funct == 0x25U) && (srcA == 0 || srcB == 0)) {
                    s32 srcOnly = srcA | srcB;
                    s32 foundSrc;
                    g_pops->jitConstValidMask &= ~(0x80000000U >> dstReg);
                    if (g_pops->jitBaseGuestReg == (u8)dstReg) {
                        g_pops->jitBaseGuestReg = 0U;
                    }
                    foundSrc = popsJitFindGuestReg(srcOnly);
                    if (foundSrc < -1) {
                        u32 dstFpu = (u32)(-foundDst) & 0x1FU;
                        u32 srcFpu = (u32)(-foundSrc) & 0x1FU;
                        *curPtr++ = 0x46000006U | (srcFpu << 11) | (dstFpu << 6);
                    } else if (foundSrc < 0) {
                        u32 dstFpu = (u32)(-foundDst) & 0x1FU;
                        u32 spillOff = popsSpillOffsetForGuest((u32)srcOnly);
                        *curPtr++ = 0xC7800000U | (dstFpu << 16) | (spillOff & 0xFFFFU);
                    } else {
                        u32 dstFpu = (u32)(-foundDst) & 0x1FU;
                        *curPtr++ = 0x44800000U | (((u32)foundSrc & 0x1FU) << 16) | (dstFpu << 11);
                    }
                } else {
                    s32 hostDst = 0;
                    s32 hostA = 0;
                    s32 hostB = 0;
                    if (srcA == srcB) {
                        curPtr = popsJitAcquirePair(curPtr, dstReg, srcB, &hostDst, &hostB);
                        hostA = hostB;
                    } else {
                        u8 dstMode = (dstReg == srcA || dstReg == srcB) ? 3U : 1U;
                        u32 packedDst = popsJitAcquireReg(curPtr, dstReg, (1U << srcA) | (1U << srcB), dstMode);
                        hostDst = (s32)(packedDst & 0x1FU);
                        curPtr = (u32 *)((packedDst >> 5) * 4U);

                        if (dstReg == srcA) {
                            hostA = hostDst;
                            if (popsJitPrevNotBranch((u32)emitPtr) != 0
                                && (emitPtr[-1] & 0xFC1FFFFFU) == (((u32)hostDst << 11) + 0x21U)) {
                                hostA = (s32)((emitPtr[-1] >> 21) & 0x1FU);
                                curPtr--;
                            }
                        } else {
                            u32 packedA = popsJitAcquireReg(curPtr, srcA, (1U << dstReg) | (1U << srcB), 2U);
                            curPtr = (u32 *)((packedA >> 5) * 4U);
                            hostA = (s32)(packedA & 0x1FU);
                        }

                        if (dstReg == srcB) {
                            hostB = hostDst;
                            if (popsJitPrevNotBranch((u32)emitPtr) != 0
                                && (emitPtr[-1] & 0xFC1FFFFFU) == (((u32)hostDst << 11) + 0x21U)) {
                                hostB = (s32)((emitPtr[-1] >> 21) & 0x1FU);
                                if (emitPtr != curPtr) {
                                    emitPtr[-1] = emitPtr[0];
                                }
                                curPtr--;
                            }
                        } else {
                            u32 packedB = popsJitAcquireReg(curPtr, srcB, (1U << dstReg) | (1U << srcA), 2U);
                            curPtr = (u32 *)((packedB >> 5) * 4U);
                            hostB = (s32)(packedB & 0x1FU);
                        }
                    }

                    *curPtr++ = (funct & 0xFC0007FFU)
                        | (((u32)hostA & 0x1FU) << 21)
                        | (((u32)hostB & 0x1FU) << 16)
                        | (((u32)hostDst & 0x1FU) << 11);
                }

                if (bothConst != 0U) {
                    g_pops->jitConstValues[dstReg] = foldVal;
                    g_pops->jitConstValidMask |= (0x80000000U >> dstReg);
                }
                return curPtr;
            }
        }

        case 4: {
            s16 off = (s16)(node->rawInsnOrTarget & 0xFFFFU);
            u32 *curPtr = popsJitEmitCopRead(emitPtr, 133, (s32)node->copReg * 4);
            return popsJitEmitMemAccess(43, 133, (s32)node->srcRegA, off, curPtr, 0);
        }

        case 5: {
            u32 *curPtr = popsJitEmitCycleSub(totalCycles, emitPtr);
            u32 *res = popsJitEmitBlockLink(popsNodePc(node), curPtr);
            g_pops->jitPendingCycles = 0;
            return res;
        }

        case 6: {
            s32 copIdx = (s32)node->copReg;
            s32 dstReg = (s32)node->dstReg;
            u32 *curPtr = popsJitEmitCopRead(emitPtr, dstReg, copIdx * 4);
            if (copIdx == 58) {
                return popsJitEmitAluImm(130U, dstReg, dstReg, 0U, curPtr);
            }
            return curPtr;
        }

        case 7: {
            u32 *curPtr = emitPtr;
            curPtr[0] = 0x8F850130U;
            curPtr[1] = 0x00053082U;
            curPtr[2] = 0x7CC50904U;
            curPtr[3] = 0xAF850130U;
            curPtr = popsJitFlushSlots(curPtr + 4, 11);
            curPtr[0] = popsJitJalEncode((const void *)popsCpuCheckIrq);
            curPtr[1] = 0xAF9901B0U;
            curPtr[2] = 0x8F9901B0U;
            return curPtr + 3;
        }

        case 8: {
            u8 copIdx = node->copReg;
            u8 writeKind = g_copWriteKindTable[copIdx];
            s32 loadOp = (writeKind == 1U) ? 33 : (writeKind == 2U ? 37 : 35);
            s16 off = (s16)(node->rawInsnOrTarget & 0xFFFFU);
            u32 *curPtr = popsJitEmitMemAccess(loadOp, 130, (s32)node->srcRegA, off, emitPtr, 0);
            return popsJitEmitCopWrite(writeKind, 130, (u32)copIdx * 4U, curPtr);
        }

        case 9: {
            s32 srcReg = (s32)node->srcRegA;
            s32 dstReg = (s32)node->dstReg;
            u32 imm = node->rawInsnOrTarget & 0xFFFFU;
            u8 op = node->opcode;

            if ((s32)(g_pops->jitConstValidMask << srcReg) < 0) {
                u32 srcVal = g_pops->jitConstValues[srcReg];
                u32 foldVal;
                switch (op) {
                    case 10U:
                        foldVal = ((s32)srcVal < (s32)(s16)imm) ? 1U : 0U;
                        break;
                    case 11U:
                        foldVal = (srcVal < (u32)(s32)(s16)imm) ? 1U : 0U;
                        break;
                    case 12U:
                        foldVal = srcVal & imm;
                        break;
                    case 13U:
                        foldVal = srcVal | imm;
                        break;
                    case 14U:
                        foldVal = srcVal ^ imm;
                        break;
                    case 15U:
                        foldVal = imm << 16;
                        break;
                    default:
                        foldVal = srcVal + (u32)(s32)(s16)imm;
                        break;
                }
                return popsJitTrackConst(dstReg, foldVal, emitPtr, node);
            }
            return popsJitEmitAluImm((u32)op, dstReg, srcReg, imm, emitPtr);
        }

        case 10: {
            u8 copIdx = node->copReg;
            if (copIdx == 67U
                && (s32)(g_pops->jitConstValidMask << node->srcRegB) >= 0
                && node != POPS_IR_NODES
                && (u8)(node[-1].opcode - 100U) < 2U) {
                volatile u32 *bpcPatch = (volatile u32 *)0x09E812B0U;
                u32 keyWord = *bpcPatch;
                if (keyWord != 0U && keyWord != 0x72D0EE59U) {
                    s32 tempReg = popsJitAllocTempReg(4, 0U);
                    u32 *curPtr = popsJitEmitImm(emitPtr, tempReg, keyWord ^ 0x72D0EE59U);
                    *curPtr++ = 0xAF80010CU | (((u32)tempReg & 0x1FU) << 16);
                    popsJitFreeTempReg(tempReg);
                    return curPtr;
                }
            }
            return popsJitEmitCopWrite(g_copWriteKindTable[copIdx], (s32)node->srcRegB, (u32)copIdx * 4U, emitPtr);
        }

        case 12: {
            s16 flags = node->flags;
            s32 regA = ((flags & 0x20) != 0) ? -1 : (s32)node->srcRegA;
            s32 regB = (s32)node->srcRegB;
            u32 branchOp = node->opcode;
            u32 *curPtr;

            if ((flags & 4) != 0) {
                PopsJitIrNode *targetIr = popsNodeForPc(node->rawInsnOrTarget);
                if (node < targetIr) {
                    node->flags = (s16)(flags | 2);
                    curPtr = popsJitEmitCondBranch(branchOp, regA, regB, (s16)totalCycles, (u32 **)&node->srcRegA, emitPtr);
                    g_pops->jitPendingCycles = 0;
                    return curPtr;
                }
                curPtr = popsJitEmitCondBranch(branchOp ^ 1U, regA, regB, (s16)totalCycles, (u32 **)&node->srcRegA, emitPtr);
                {
                    u32 targetCode = *(u32 *)&targetIr->kind;
                    s32 backDelta = (s32)(targetCode - (u32)curPtr - 4U) >> 2;
                    *curPtr = 0x1F200000U | ((u32)backDelta & 0xFFFFU);
                    curPtr = popsJitEmitImm(curPtr + 1, 31, targetCode);
                    curPtr = popsJitEmitImm(curPtr, 2, node->rawInsnOrTarget);
                    curPtr[0] = curPtr[-1];
                    curPtr[-1] = popsJitJumpEncode((const void *)popsJitEventLoop);
                    curPtr++;
                }
            } else {
                curPtr = popsJitEmitCondBranch(branchOp ^ 1U, regA, regB, (s16)totalCycles, (u32 **)&node->srcRegA, emitPtr);
                curPtr = popsJitEmitBlockLink(node->rawInsnOrTarget, curPtr);
            }

            {
                u32 *patchPtr = *(u32 **)&node->srcRegA;
                u32 patchWord = *patchPtr;
                if ((patchWord >> 27) == 1U) {
                    *patchPtr = (curPtr == patchPtr) ? 0U : ((patchWord & 0xFC000000U) | popsJitJumpWord((u32)curPtr));
                } else {
                    s32 relDelta = (s32)((const u8 *)curPtr - (const u8 *)patchPtr - 4) >> 2;
                    *patchPtr = (patchWord & 0xFFFF0000U) | ((u32)relDelta & 0xFFFFU);
                }
            }
            g_pops->jitPendingCycles = 0;
            return curPtr;
        }

        case 14: {
            s32 jumpCycles = totalCycles + g_pops->branchCycleBias;
            if ((node->flags & 4) != 0) {
                PopsJitIrNode *targetIr = popsNodeForPc(node->rawInsnOrTarget);
                if (node >= targetIr) {
                    u32 *curPtr = popsJitEmitCycleSub(jumpCycles, emitPtr);
                    curPtr = popsJitSaveAllDirty(curPtr);
                    {
                        u32 targetCode = *(u32 *)&targetIr->kind;
                        s32 backDelta = (s32)(targetCode - (u32)curPtr - 4U) >> 2;
                        *curPtr = 0x1F200000U | ((u32)backDelta & 0xFFFFU);
                        curPtr = popsJitEmitImm(curPtr + 1, 31, targetCode);
                        curPtr = popsJitEmitImm(curPtr, 2, node->rawInsnOrTarget);
                        curPtr[0] = curPtr[-1];
                        curPtr[-1] = popsJitJumpEncode((const void *)popsJitEventLoop);
                        g_pops->jitPendingCycles = 0;
                        return curPtr + 1;
                    }
                } else {
                    PopsJitIrNode *scan = node + 2;
                    while (scan < targetIr && scan->kind == 0U) {
                        scan++;
                    }
                    if (scan == targetIr) {
                        u32 *res = popsJitEmitCycleSub(jumpCycles, emitPtr);
                        g_pops->jitPendingCycles = 0;
                        return res;
                    }
                    node->flags = (s16)(node->flags | 2);
                    {
                        u32 *res = popsJitEmitExitBranch(emitPtr, (u32 **)&node->srcRegA, jumpCycles, 0U);
                        g_pops->jitPendingCycles = 0;
                        return res;
                    }
                }
            } else {
                u32 *curPtr = popsJitEmitCycleSub(jumpCycles, emitPtr);
                u32 *res = popsJitEmitBlockLink(node->rawInsnOrTarget, curPtr);
                g_pops->jitPendingCycles = 0;
                return res;
            }
        }

        case 15: {
            u32 epc = popsNodePc(node);
            u32 exCode;
            switch (node->opcode) {
                case 0x11U:
                case 0x31U:
                case 0x39U:
                    exCode = 0x0400000BU;
                    break;
                case 0x13U:
                case 0x33U:
                case 0x3BU:
                    exCode = 0x0C00000BU;
                    break;
                case 0x4CU:
                    exCode = 8U;
                    break;
                case 0x4DU:
                    exCode = 9U;
                    break;
                default:
                    exCode = 10U;
                    break;
            }
            if ((node->flags & 1) != 0) {
                exCode |= 0x20000000U;
                epc -= 4U;
            }
            {
                s32 tempReg = popsJitAllocTempReg(4, 0U);
                u32 *curPtr = popsJitEmitCycleSub(totalCycles, emitPtr);
                curPtr = popsJitEmitImm(curPtr, tempReg, epc);
                *curPtr++ = 0xAF800138U | (((u32)tempReg & 0x1FU) << 16);
                popsJitFreeTempReg(tempReg);
                curPtr = popsJitFlushSlots(curPtr, 11);
                *curPtr++ = 0x24040000U | (exCode & 0xFFFFU);
                if ((exCode & 0xFFFF0000U) != 0U) {
                    curPtr = popsJitEmitImm(curPtr, 5, exCode >> 22);
                    *curPtr++ = 0xA3850137U;
                }
                curPtr[0] = popsJitJalEncode((const void *)popsCpuRaiseException);
                curPtr[1] = 0xAF9901B0U;
                curPtr[2] = 0x00400008U;
                curPtr[3] = 0x8F9901B0U;
                g_pops->jitPendingCycles = 0;
                return curPtr + 4;
            }
        }

        case 16: {
            u16 flags = (u16)node->flags;
            s32 op = (s32)node->opcode;
            s32 baseReg = (s32)node->srcRegA;
            s32 valReg = (s32)(node->srcRegB | node->dstReg);
            s16 off = (s16)(node->rawInsnOrTarget & 0xFFFFU);
            s32 isStore = ((op & 8) != 0) ? 1 : 0;
            s32 directSpill = 0;

            if ((flags & 0x8000U) != 0U
                && (op & 3) == 2
                && node[1].opcode == (u8)(op ^ 4)
                && baseReg == (s32)node[1].srcRegA
                && valReg == (s32)(node[1].srcRegB | node[1].dstReg)
                && baseReg != valReg) {
                s32 curOff = off;
                s32 nextOff = (s16)(node[1].rawInsnOrTarget & 0xFFFFU);
                s32 loOff = ((op & 4) != 0) ? curOff : nextOff;
                s32 hiOff = ((op & 4) != 0) ? nextOff : curOff;
                if (hiOff - 3 == loOff) {
                    off = (s16)loOff;
                    op |= 0x34;
                    node[1].kind = 0U;
                }
            } else {
                if ((flags & 0x8000U) == 0U) {
                    directSpill = 1;
                } else {
                    u8 nextOp = node[1].opcode;
                    if ((nextOp & 0xF0U) == 0x20U && (nextOp & 3U) != 2U) {
                        if (node[1].srcRegA != 29U || (g_pops->jitConstValidMask & 4U) == 0U) {
                            directSpill = 1;
                        }
                    }
                    if ((flags & 0x80U) != 0U) {
                        directSpill = 1;
                    }
                }
            }

            if (isStore != 0
                && (s32)(g_pops->jitConstValidMask << baseReg) < 0
                && (g_pops->quirkFlags & 0x40000U) == 0U) {
                u32 storeAddr = g_pops->jitConstValues[baseReg] + (u32)(s32)off;
                u32 endPc = popsNodePc((const PopsJitIrNode *)g_pops->jitIrEndPtr);
                if (storeAddr > popsNodePc(node) && storeAddr < endPc) {
                    PopsJitIrNode *modNode = popsNodeForPc(storeAddr);
                    if (modNode->kind != 0U) {
                        if ((modNode->flags & 1) != 0) {
                            modNode--;
                            node->flags--;
                        }
                        modNode->kind = 5U;
                        modNode->flags = (s16)(modNode->flags | 0x10);
                    }
                }
            }

            {
                u32 *res = popsJitEmitMemAccess(op, valReg, baseReg, off, emitPtr, directSpill);
                if (isStore != 0 && ((node[1].opcode & 0xF8U) == 0x20U || g_pops->jitBaseInstCycles != 0U)) {
                    g_pops->jitPendingCycles += 2;
                }
                return res;
            }
        }

        case 17: {
            s32 srcReg = (s32)node->srcRegB;
            u32 shamt = (node->rawInsnOrTarget >> 6) & 0x1FU;
            s32 isConst = ((s32)(g_pops->jitConstValidMask << srcReg) >> 31);
            u32 constVal = g_pops->jitConstValues[srcReg];
            s32 dstReg = (s32)node->dstReg;
            u32 op = node->opcode;
            u32 *curPtr = (u32 *)0;
            s32 fusedShift = 0;

            if (((u16)node->flags & 0x8000U) != 0U
                && node[1].kind == 17U
                && node[1].srcRegB == node[1].dstReg
                && dstReg == (s32)node[1].srcRegB) {
                u32 nextOp = node[1].opcode;
                u32 nextShamt = (node[1].rawInsnOrTarget >> 6) & 0x1FU;

                if (dstReg == srcReg && shamt == nextShamt && (op - 66U) < 2U && nextOp == 0x40U) {
                    u32 packed = popsJitAcquireReg(emitPtr, dstReg, 0U, 3U);
                    curPtr = (u32 *)((packed >> 5) * 4U);
                    *curPtr++ = 0x7C000004U | ((packed & 0x1FU) << 16) | (((shamt - 1U) & 0x1FU) << 11);
                    if (isConst != 0) {
                        constVal &= (~0U << shamt);
                    }
                    node[1].kind = 0U;
                    fusedShift = 1;
                } else if (op == 0x40U && nextOp == 0x42U && nextShamt >= shamt) {
                    s32 hostDst = 0;
                    s32 hostSrc = 0;
                    curPtr = popsJitAcquirePair(emitPtr, dstReg, srcReg, &hostDst, &hostSrc);
                    *curPtr++ = 0x7C000000U
                        | (((u32)hostSrc & 0x1FU) << 21)
                        | (((u32)hostDst & 0x1FU) << 16)
                        | (((31U - nextShamt) & 0x1FU) << 11)
                        | (((nextShamt - shamt) & 0x1FU) << 6);
                    if (isConst != 0) {
                        constVal = (constVal << shamt) >> nextShamt;
                    }
                    node[1].kind = 0U;
                    fusedShift = 1;
                } else if (op == 0x40U && nextOp == 0x43U && shamt == nextShamt
                           && (shamt == 16U || shamt == 24U)) {
                    u32 extOp = (shamt == 16U) ? 130U : 129U;
                    curPtr = popsJitEmitAluImm(extOp, dstReg, srcReg, 0U, emitPtr);
                    node[1].kind = 0U;
                    if (isConst != 0) {
                        constVal = (shamt == 16U) ? (u32)(s32)(s16)constVal : (u32)(s32)(s8)constVal;
                    }
                    fusedShift = 1;
                }
            }

            if (fusedShift == 0) {
                s32 hostDst = 0;
                s32 hostSrc = 0;
                curPtr = popsJitAcquirePair(emitPtr, dstReg, srcReg, &hostDst, &hostSrc);
                *curPtr++ = (op & 0xFFE007BFU)
                    | ((shamt & 0x1FU) << 6)
                    | (((u32)hostSrc & 0x1FU) << 16)
                    | (((u32)hostDst & 0x1FU) << 11);
                if (isConst != 0) {
                    if (op == 64U) {
                        constVal <<= shamt;
                    } else if (op == 66U) {
                        constVal >>= shamt;
                    } else {
                        constVal = (u32)((s32)constVal >> shamt);
                    }
                }
            }

            if (isConst != 0) {
                g_pops->jitConstValues[dstReg] = constVal;
                g_pops->jitConstValidMask |= (0x80000000U >> dstReg);
            }
            return curPtr;
        }

        case 18: {
            /* GTE command: determine if FLAG register (cop2 ctrl 31 = reg 63) is live after this op */
            PopsJitIrNode *irEnd = (PopsJitIrNode *)g_pops->jitIrEndPtr;
            PopsJitIrNode *scan = node + 1;
            PopsJitIrNode *furthest = node;
            u32 rawCmd = node->rawInsnOrTarget;
            s32 needFlags = 1;

            while (scan <= irEnd) {
                if ((s8)scan->dstReg > 0 && scan->copReg == 63U) {
                    break;
                }
                if ((scan->flags & 0x10) != 0) {
                    break;
                }
                if ((scan->flags & 1) != 0) {
                    if ((scan[-1].flags & 4) == 0) {
                        break;
                    }
                    {
                        PopsJitIrNode *tNode = popsNodeForPc(scan[-1].rawInsnOrTarget);
                        if (tNode < node) {
                            if ((tNode->flags & 0x2000) == 0) {
                                break;
                            }
                            if (scan[-1].opcode < 0xC0U) {
                                needFlags = 0;
                                break;
                            }
                        } else {
                            if (scan[-1].opcode < 0xC0U) {
                                scan = tNode;
                                continue;
                            }
                            if ((tNode->flags & 0x2000) == 0 && furthest < tNode) {
                                furthest = tNode;
                            }
                        }
                    }
                }
                if (scan >= furthest && (scan->flags & 0x2000) != 0) {
                    needFlags = 0;
                    break;
                }
                scan++;
            }

            {
                u32 *curPtr = emitPtr;
                const void *gteFn = (const void *)0;
                s32 flushMax = -1;
                s32 flushHiLoOnly = 0;

                switch (rawCmd & 0x3FU) {
                    case 1:
                        gteFn = (needFlags != 0) ? (const void *)popsGteRtpsFlagged : (const void *)popsGteRtps;
                        flushMax = 11;
                        break;
                    case 6:
                        gteFn = (const void *)popsGteNclip;
                        flushMax = 0;
                        break;
                    case 12:
                        *curPtr++ = 0x34040000U | (((rawCmd & 0x80000U) != 0U) ? 12U : 0U);
                        gteFn = (const void *)popsGteOp;
                        flushMax = 6;
                        break;
                    case 16:
                        gteFn = (needFlags != 0) ? (const void *)popsGteDpclFlagged : (const void *)popsGteVfpuKernel;
                        break;
                    case 17:
                        gteFn = (const void *)popsGteIntpl;
                        flushMax = 6;
                        break;
                    case 18: {
                        u32 mx = (rawCmd >> 17) & 3U;
                        u32 vx = (rawCmd >> 15) & 3U;
                        u32 tx = (rawCmd >> 13) & 3U;
                        u32 lm = ((rawCmd & 0x400U) != 0U) ? 0U : 0x8000U;
                        u32 sfReg = ((rawCmd & 0x80000U) != 0U) ? 28U : 0U;
                        u32 matOff = (mx == 0U) ? 128U : (mx == 1U ? 160U : (mx == 2U ? 192U : 1U));
                        s16 vecOff = (vx == 0U) ? -36 : (vx == 1U ? -28 : (vx == 2U ? -20 : 0));
                        curPtr = popsJitFlushHiLo(curPtr);
                        curPtr[0] = 0x24040000U | (sfReg << 21) | matOff;
                        curPtr[1] = 0x24050000U | lm;
                        curPtr[2] = 0x27860000U | ((u32)(u16)vecOff);
                        if (tx == 3U) {
                            curPtr[3] = 0x34020000U;
                        } else {
                            u32 transOff = (tx == 0U) ? 148U : (tx == 1U ? 180U : 212U);
                            curPtr[3] = 0x27820000U | transOff;
                        }
                        curPtr += 4;
                        gteFn = (needFlags != 0) ? (const void *)popsGteMvmvaFlaggedInner : (const void *)popsGteMvmva;
                        flushMax = 11;
                        break;
                    }
                    case 19:
                        gteFn = (const void *)popsGteNcds;
                        break;
                    case 20:
                        gteFn = (const void *)popsGteCdp;
                        break;
                    case 22:
                        gteFn = (const void *)popsGteNcdt;
                        break;
                    case 27:
                        gteFn = (const void *)popsGteNccs;
                        break;
                    case 28:
                        gteFn = (const void *)popsGteCc;
                        break;
                    case 30:
                        gteFn = (const void *)popsGteNcs;
                        break;
                    case 32:
                        gteFn = (const void *)popsGteNct;
                        break;
                    case 40:
                        *curPtr++ = 0x34040000U | (((rawCmd & 0x80000U) != 0U) ? 12U : 0U);
                        gteFn = (const void *)popsGteSqr;
                        flushMax = 0;
                        break;
                    case 41:
                        gteFn = (const void *)popsGteDpcs;
                        flushMax = 7;
                        break;
                    case 42:
                        gteFn = (const void *)popsGteDpct;
                        flushMax = 11;
                        break;
                    case 45:
                        gteFn = (const void *)popsGteAvsz3;
                        flushHiLoOnly = 1;
                        break;
                    case 46:
                        gteFn = (const void *)popsGteAvsz4;
                        flushHiLoOnly = 1;
                        break;
                    case 48:
                        gteFn = (needFlags != 0) ? (const void *)popsGteRtptFlagged : (const void *)popsGteRtpt;
                        flushMax = 11;
                        break;
                    case 61:
                        *curPtr++ = 0x34040000U | (((rawCmd & 0x80000U) != 0U) ? 12U : 0U);
                        gteFn = (const void *)popsGteGpf;
                        flushMax = 4;
                        break;
                    case 62:
                        *curPtr++ = 0x34040000U | (((rawCmd & 0x80000U) != 0U) ? 12U : 0U);
                        gteFn = (const void *)popsGteGpl;
                        flushMax = 4;
                        break;
                    case 63:
                        gteFn = (const void *)popsGteNcct;
                        break;
                    default:
                        gteFn = (const void *)0;
                        break;
                }

                if (flushHiLoOnly != 0) {
                    curPtr = popsJitFlushHiLo(curPtr);
                    g_pops->jitHiLoLiveMask = 0U;
                } else if (flushMax >= 0) {
                    curPtr = popsJitFlushSlots(curPtr, flushMax);
                }

                return popsJitEmitCallWithDelay(curPtr, popsJitJalEncode(gteFn) << 2);
            }
        }

        default:
            return emitPtr;
    }
}

/* [43] 0x0064d0 */
__attribute__((noinline))
u32 popsJitFlushCodeCache(void)
{
    u32 dirtyMask = g_pops->jitDirtyPageMask;
    u32 pageBase = POPS_JIT_LOOKUP_RAM;

    while (dirtyMask != 0U) {
        if ((dirtyMask & 1U) != 0U) {
            u32 addr = pageBase;
            u32 endAddr = pageBase + 0x10000U;
            do {
                u32 uncached = 0x40000000U | (addr & 0x1FFFFFFFU);
                popsCacheDataHitInv((const void *)addr);
                __asm__ __volatile__(
                    "sync\n\t"
                    "sv.q R403.q, 0(%0), wb\n\t"
                    "cache 0x19, 64(%1)\n\t"
                    "sync\n\t"
                    "sv.q R403.q, 16(%0), wb\n\t"
                    "sv.q R403.q, 32(%0), wb\n\t"
                    "sv.q R403.q, 48(%0), wb\n\t"
                    "sv.q R403.q, 64(%0), wb\n\t"
                    "sv.q R403.q, 80(%0), wb\n\t"
                    "sv.q R403.q, 96(%0), wb\n\t"
                    "sv.q R403.q, 112(%0), wb\n\t"
                    ".word 0x486000ff\n\t"
                    :: "r"(uncached), "r"(addr) : "memory");
                addr += 128U;
            } while (addr != endAddr);
        }
        dirtyMask >>= 1;
        pageBase += 0x10000U;
    }

    g_pops->jitRamCodePtr = POPS_JIT_CODE_BASE;
    g_pops->jitDirtyPageMask = 0U;
    g_pops->jitExceptionHandler = 0U;
    g_pops->jitRamExitTrampoline = 0U;
    g_pops->jitChecksumFailTarget = 0U;
    return POPS_JIT_CODE_BASE;
}
