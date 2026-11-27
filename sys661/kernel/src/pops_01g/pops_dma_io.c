#include "pops_internal.h"

static __inline__ __attribute__((always_inline)) void popsSpuWaitMeClock(void)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    s32 elapsed = (s32)(g_pops->cycleTarget - (u32)g_pops->cycleRemaining);
    s32 delta = elapsed - (s32)g_pops->spuLastSyncCycle;
    u32 curSample = spu->sampleCounter;

    g_pops->spuLastSyncCycle = (u32)elapsed;
    if (delta >= 768 && curSample == g_pops->spuSampleClock) {
        do {
            sceKernelDelayThread(22U);
        } while (curSample == spu->sampleCounter);
        curSample = spu->sampleCounter;
    }
    g_pops->spuSampleClock = curSample;
}

static __inline__ void popsSpuAcquireMeLock(void)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    spu->cpuLockWant = 1U;
    spu->lockState.parts.lockTurn = 1U;
    while (spu->lockState.pair == 0x0101U) {
        /* spin until ME releases Peterson lock */
    }
}

static __inline__ void popsSpuReleaseMeLock(void)
{
    POPS_ME_SPU_SHARED->cpuLockWant = 0U;
}

static __inline__ __attribute__((always_inline)) void popsDcacheWritebackLines(u32 startAddr, u32 byteLen)
{
    u32 endAddr = startAddr + byteLen;
    if ((byteLen >> 17) != 0U) {
        sceKernelDcacheWritebackRange((const void *)(uintptr_t)startAddr, byteLen);
    } else {
        u32 lastByte = endAddr - 1U;
        u32 line = (startAddr & ~0x3FU) | (lastByte & 0x3FU);
        for (;;) {
            __builtin_allegrex_cache(0x1A, (void *)(uintptr_t)line);
            if (line == lastByte) {
                break;
            }
            line += 64U;
        }
    }
}

static __inline__ __attribute__((always_inline)) void popsDcacheInvalidateLines(u32 startAddr, u32 byteLen)
{
    u32 cur = startAddr;
    u32 endAddr = startAddr + byteLen;
    u32 alignedEnd;

    if ((startAddr & 0x3FU) != 0U) {
        __builtin_allegrex_cache(0x1B, (void *)(uintptr_t)startAddr);
        cur = (startAddr & ~0x3FU) + 64U;
    }
    if ((endAddr & 0x3FU) != 0U) {
        __builtin_allegrex_cache(0x1B, (void *)(uintptr_t)endAddr);
    }
    alignedEnd = endAddr & ~0x3FU;
    if (((endAddr - cur) >> 17) != 0U) {
        sceKernelDcacheInvalidateRangeForUser((const void *)(uintptr_t)cur, alignedEnd - cur);
    } else {
        while (cur != alignedEnd) {
            __builtin_allegrex_cache(0x19, (void *)(uintptr_t)cur);
            cur += 64U;
        }
    }
}

/* 0x006580: Check pending SPU IRQ from ME audio engine */
void popsSpuCheckIrq(void)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;

    if (spu->cddaCtrl.parts.irqFired == 0U) {
        s32 combined = (s32)g_pops->spuCtrlReg & (s32)(spu->spuRegs[0xD7] | (u16)g_pops->spuIrqStatusBit);
        u32 fired = ((u32)combined >> 6) & 1U;
        if (fired != 0U) {
            spu->cddaCtrl.parts.irqFired = (u8)fired;
            popsIrqRaise(0x200U);
        }
    }
}

/* 0x0065d4: PS1 SPU MMIO register write (0x1F801C00..0x1F801DFF) */
void popsSpuWriteReg(u32 addr, u32 value, u32 width)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    u32 regOff = addr & 0x1FFU;
    u32 regVal;

    if (width == 1U) {
        regVal = value & 0xFFFFU;
    } else if (width != 0U) {
        popsSpuWriteReg(regOff, value, 1U);
        regOff += 2U;
        regVal = value >> 16;
    } else {
        regVal = value & 0xFFU;
        if ((addr & 1U) != 0U) {
            regVal <<= 8;
            regOff--;
        }
    }

    g_pops->cycleRemaining--;
    if (regOff == 0x1AEU) {
        return;
    }

    if (regOff < 0x180U && (regOff & 0xEU) == 6U && regVal < 0x200U) {
        regVal = 0x202U;
    }
    spu->spuRegs[regOff >> 1] = (u16)regVal;

    if (regOff < 0x180U) {
        u32 voiceBit = 1U << (regOff >> 4);
        popsSpuWaitMeClock();
        popsSpuAcquireMeLock();
        spu->voiceDirtyMask |= voiceBit;
        if ((regOff & 0xFU) == 0xEU) {
            spu->voiceRepeatMask |= voiceBit;
        }
        popsSpuReleaseMeLock();
        return;
    }

    if (regOff == 0x1A4U) {
        if ((spu->spuRegs[0xD5] & 0x40U) != 0U && (g_pops->spuTransferAddr >> 2) == regVal) {
            g_pops->spuIrqStatusBit = 0x40U;
        }
        return;
    }

    if (regOff >= 0x188U && regOff < 0x18CU) {
        if (regVal != 0U) {
            u32 mask = regVal << ((regOff - 0x188U) * 8U);
            popsSpuWaitMeClock();
            popsSpuAcquireMeLock();
            spu->keyOnMask |= mask;
            spu->keyOffMask &= ~mask;
            popsSpuReleaseMeLock();
        }
        return;
    }

    if (regOff >= 0x18CU && regOff < 0x190U) {
        if (regVal != 0U) {
            u32 mask = regVal << ((regOff - 0x18CU) * 8U);
            popsSpuWaitMeClock();
            popsSpuAcquireMeLock();
            spu->keyOffMask |= mask;
            popsSpuReleaseMeLock();
        }
        return;
    }

    if (regOff == 0x1A8U) {
        u32 count = g_pops->spuFifoCount;
        if (count < 32U) {
            g_pops->spuTransferFifo[count] = (u16)regVal;
            g_pops->spuFifoCount = (u8)(count + 1U);
        }
        return;
    }

    if (regOff >= 0x190U && regOff < 0x19CU) {
        popsSpuWaitMeClock();
        return;
    }

    if (regOff == 0x1AAU) {
        u32 spuEnabled = (regVal & 0x8000U) != 0U;
        g_pops->spuCtrlReg = (s16)regVal;

        if ((g_pops->quirkFlagsExt & 0x10) == 0 ||
            ((g_pops->spuCtrlShadow ^ regVal) & 0x8040U) != 0U) {
            popsSpuWaitMeClock();
        }
        g_pops->spuCtrlShadow = regVal;

        if (!spuEnabled) {
            spu->spuRegs[0xD5] = (u16)(regVal & 0x3FU);
        }

        if (((regVal >> 4) & 3U) == 1U) {
            u32 pending = g_pops->spuFifoCount;
            u32 xferWord = g_pops->spuTransferAddr;
            u32 irqWord = 4U * (u32)spu->spuRegs[0xD2];
            u32 idx;

            for (idx = 0U; idx < pending; idx++) {
                if (xferWord == irqWord) {
                    g_pops->spuIrqStatusBit = 0x40U;
                }
                spu->spuRam[xferWord] = g_pops->spuTransferFifo[idx];
                xferWord = (xferWord + 1U) & 0x3FFFFU;
            }
            g_pops->spuTransferAddr = xferWord;
            g_pops->spuFifoCount = 0U;
        }

        if ((regVal & 0x40U) != 0U) {
            if (g_pops->spuIrqEvent.next == NULL) {
                s32 period = ((g_pops->quirkFlagsExt & 0x80) != 0) ? 34448 : 2153;
                popsEventSchedule(&g_pops->spuIrqEvent, period);
            }
        } else {
            if (g_pops->spuIrqEvent.next != NULL) {
                popsEventCancel(&g_pops->spuIrqEvent);
            }
            spu->cddaCtrl.parts.irqFired = 1U;
            g_pops->spuIrqStatusBit = 0U;
        }
        return;
    }

    if (regOff == 0x1A6U) {
        u32 xferMode = ((u32)(s32)g_pops->spuCtrlReg >> 4) & 3U;
        g_pops->spuTransferAddr = 4U * regVal;
        if (xferMode != 0U && (spu->spuRegs[0xD5] & 0x40U) != 0U &&
            regVal == spu->spuRegs[0xD2]) {
            g_pops->spuIrqStatusBit = 0x40U;
        }
        return;
    }

    if (regOff == 0x1A2U || (regOff >= 0x1C0U && regOff < 0x200U)) {
        popsSpuAcquireMeLock();
        spu->voiceDirtyMask |= 0x80000000U;
        popsSpuReleaseMeLock();
    }
}

/* 0x006b48: Enqueue decoded CD-DA / XA audio sector to ME SPU ring buffer */
s32 popsSpuQueueCddaSector(u32 sectorAddr, s32 mode)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    u32 ctrlWord;
    s32 modeByte;
    s32 queued;
    u32 flushBase;
    u32 flushLen;
    u32 lastAddr;
    u32 line;

    for (;;) {
        s32 spaceAvail;
        ctrlWord = spu->cddaCtrl.word;
        queued = (s32)((ctrlWord - (ctrlWord >> 8)) & 7U);
        modeByte = (s32)ctrlWord >> 24;

        if (modeByte >= 0 || modeByte == -2) {
            spaceAvail = (queued != 7);
        } else {
            do {
                sceKernelDelayThreadCB(100U);
                modeByte = spu->cddaCtrl.parts.cddaMode;
                popsUpdateDisplayMute();
                spaceAvail = (queued < 7);
            } while (modeByte != -2);
        }

        if (spaceAvail) {
            break;
        }
        if (POPS_SHARED_MEM->fastForward != 0U) {
            return queued;
        }
        if ((spu->spuRegs[0xD5] >> 15) == 0U) {
            break;
        }
        sceKernelDelayThreadCB(100U);
        popsUpdateDisplayMute();
    }

    if (mode == 2) {
        flushBase = sectorAddr + 16U;
        flushLen = 2312U;
    } else {
        flushBase = sectorAddr;
        flushLen = 2352U;
    }

    lastAddr = flushBase + flushLen - 1U;
    line = (flushBase & ~0x3FU) | (lastAddr & 0x3FU);
    for (;;) {
        __builtin_allegrex_cache(0x1A, (void *)(uintptr_t)line);
        if (line == lastAddr) {
            break;
        }
        line += 64U;
    }

    spu->cddaQueue[ctrlWord & 7U] = sectorAddr;
    spu->cddaCtrl.parts.writeIdx = (u8)((ctrlWord & 0xFFU) + 1U);

    if (modeByte < 0 && g_pops->spuDelayQuirk != 0U) {
        mode = 0;
    }
    if (modeByte < 1 && mode > 0) {
        popsGpuThrottleSpeed();
    }
    spu->cddaCtrl.parts.cddaMode = (s8)mode;
    return (s32)flushLen;
}

/* 0x006ca8: Number of CD-DA/XA sectors currently buffered for ME SPU */
s32 popsSpuGetCddaQueuedCount(void)
{
    u32 idxPair = POPS_ME_SPU_SHARED->cddaCtrl.idxPair;
    return (s32)((idxPair - (idxPair >> 8)) & 7U);
}

/* 0x006cc0: Number of pending audio sectors ahead of active playback */
s32 popsSpuGetCddaPending(void)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    if (spu->cddaCtrl.parts.cddaMode > 0) {
        u32 idxPair = spu->cddaCtrl.idxPair;
        return (s32)((idxPair - (idxPair >> 8)) & 7U) - 1;
    }
    return 1;
}

/* 0x006cf0: Periodic SPU IRQ event callback */
void popsSpuIrqPollCallback(PopsEvent *ev)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    (void)ev;

    if ((g_pops->quirkFlagsExt & 0x20) != 0) {
        return;
    }

    if (spu->cddaCtrl.parts.irqFired != 0U) {
        popsEventSchedule(&g_pops->spuIrqEvent, 2153);
        return;
    }

    if (((u16)g_pops->spuCtrlReg & (spu->spuRegs[0xD7] | (u16)g_pops->spuIrqStatusBit) & 0x40U) != 0U) {
        spu->cddaCtrl.parts.irqFired = 1U;
        popsIrqRaise(0x200U);
    } else {
        popsEventSchedule(&g_pops->spuIrqEvent, 2153);
    }
}

/* 0x006d70: PS1 SPU MMIO register read */
u32 popsSpuReadReg(u32 addr, u32 width)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    u32 regOff = addr & 0x3FFU;
    u32 regPtr = POPS_ME_SPU_SHARED_BASE + regOff;

    g_pops->cycleRemaining -= 10;

    if (regOff >= 0x180U && ((width & 3U) == 1U) && regOff == 0x1AEU) {
        u16 statusVal;
        if ((g_pops->quirkFlagsExt & 0x40) != 0) {
            popsSpuSyncStatusRead();
        } else {
            popsSpuWaitMeClock();
        }
        statusVal = spu->spuRegs[regOff >> 1];
        return (u32)((((statusVal & 0x20U) != 0U) ? 0x80U : 0U) |
                     (statusVal & 0xF7FU) |
                     (u32)g_pops->spuIrqStatusBit);
    }

    if (regOff < 0x180U) {
        popsSpuSyncVoiceRead();
    }

    if (width == 5U) {
        return *(const volatile u16 *)(uintptr_t)regPtr;
    }
    switch (width) {
    case 0U:
        return (u32)(s32)*(const volatile s8 *)(uintptr_t)regPtr;
    case 1U:
        return (u32)(s32)*(const volatile s16 *)(uintptr_t)regPtr;
    case 2U:
        g_pops->cycleRemaining -= 7;
        return *(const volatile u32 *)(uintptr_t)regPtr;
    default:
        return *(const volatile u8 *)(uintptr_t)regPtr;
    }
}

/* 0x006ebc: PS1 DMA4 (SPU RAM <-> Main RAM) transfer handler */
s32 popsSpuDmaTransfer(u32 addr, u32 bytes, u32 chcr)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    u32 ramAddr = (addr & 0x1FFFFFU) | POPS_PSX_RAM_BASE;
    u32 spuByteOff = 2U * g_pops->spuTransferAddr;
    u32 xferBytes = bytes;
    u32 dstAddr;
    u32 srcAddr;
    u8 ctrlLo;

    if (bytes == 0U) {
        return 1;
    }

    if ((g_pops->spuDmaLatency & 0x10000U) != 0U && (chcr & 1U) != 0U && (s32)addr >= 0) {
        popsEventSchedule(&g_pops->spuDmaEvent, (s32)(4U * bytes));
        g_pops->spuDmaSrc = addr | 0x80000000U;
        g_pops->spuDmaLen = bytes;
        return (s32)bytes;
    }

    ctrlLo = (u8)spu->spuRegs[0xD5];
    g_pops->spuTransferAddr = ((spuByteOff + xferBytes) >> 1) & 0x3FFFFU;
    if (spuByteOff + xferBytes > 0x80000U) {
        xferBytes = 0x80000U - spuByteOff;
    }

    if ((ctrlLo & 0x40U) != 0U) {
        u32 endLimit = spuByteOff + bytes + (((chcr & 1U) == 0U) ? 32U : 0U);
        u32 irqByteOff = 8U * (u32)spu->spuRegs[0xD2];
        if ((u32)(irqByteOff - spuByteOff) < (endLimit - spuByteOff)) {
            g_pops->spuIrqStatusBit = 0x40U;
        }
    }

    if ((chcr & 1U) != 0U) {
        dstAddr = POPS_ME_SPU_SHARED_BASE + 0x2C0U + spuByteOff;
        srcAddr = ramAddr;
    } else {
        u32 *jitClear = (u32 *)(uintptr_t)(ramAddr + 0x400000U);
        u32 *jitEnd = (u32 *)(uintptr_t)(ramAddr + 0x400000U + xferBytes);
        while (jitClear != jitEnd) {
            *jitClear++ = 0U;
        }
        dstAddr = ramAddr;
        srcAddr = POPS_ME_SPU_SHARED_BASE + 0x2C0U + spuByteOff;
    }

    if (xferBytes <= 0x1000U) {
        sceKernelMemcpy((void *)(uintptr_t)dstAddr, (const void *)(uintptr_t)srcAddr, xferBytes);
    } else {
        if ((chcr & 1U) != 0U) {
            popsDcacheWritebackLines(srcAddr, xferBytes);
        } else {
            popsDcacheInvalidateLines(dstAddr, xferBytes);
        }
        sceDmacMemcpy((void *)(uintptr_t)dstAddr, (const void *)(uintptr_t)srcAddr, xferBytes);
    }

    if ((g_pops->quirkFlags & 0x100U) == 0U) {
        return (s32)bytes;
    }
    return 1;
}

/* 0x0070c8: Deferred SPU DMA completion callback */
void popsSpuDmaDeferredCallback(PopsEvent *ev)
{
    (void)ev;
    popsSpuDmaTransfer(g_pops->spuDmaSrc, g_pops->spuDmaLen, 1U);
}

/* 0x0070ec: Synchronize CPU cycle clock with ME SPU during voice reads */
void popsSpuSyncVoiceRead(void)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    s32 curCycle = (s32)(g_pops->cycleTarget - (u32)g_pops->cycleRemaining);
    s32 delta = curCycle - (s32)g_pops->spuLastSyncCycle;

    g_pops->spuLastSyncCycle = (u32)curCycle;
    if ((u32)(delta - 768) <= 0x89A00U) {
        u32 startSample = g_pops->spuSampleClock;
        s32 clamped = __builtin_allegrex_min(delta, 6144);
        for (;;) {
            u32 nowSample = spu->sampleCounter;
            s32 sampleCycles = 768 * (s32)(nowSample - startSample);
            if (sampleCycles < 0 || clamped < sampleCycles) {
                break;
            }
            sceKernelDelayThread(22U);
            g_pops->spuSampleClock = nowSample;
        }
    }
}

/* 0x007198: Synchronize CPU cycle clock with ME SPU during status reads */
void popsSpuSyncStatusRead(void)
{
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    s32 curCycle = (s32)(g_pops->cycleTarget - (u32)g_pops->cycleRemaining);
    s32 delta = curCycle - (s32)g_pops->spuLastSyncCycle;

    if ((u32)(delta - 768) <= 0x89A00U) {
        u32 startSample = g_pops->spuSampleClock;
        s32 clamped = __builtin_allegrex_min(delta, 6144);
        g_pops->spuLastSyncCycle = (u32)curCycle;
        for (;;) {
            u32 nowSample = spu->sampleCounter;
            s32 sampleCycles = 768 * (s32)(nowSample - startSample);
            if (sampleCycles < 0 || clamped < sampleCycles) {
                break;
            }
            sceKernelDelayThread(22U);
            g_pops->spuSampleClock = nowSample;
        }
    }
}

/* 0x007244: Slow-path PS1 I/O, BIOS ROM, and cache-control read */
u32 popsIoReadSlow(u32 addr, u32 width)
{
    if (addr == 0xFFFE0130U) {
        return g_pops->cacheControlReg;
    }

    if (addr != 0x1F802030U && addr != 0x1F802040U) {
        u32 phys;
        if (addr - 0x1F000000U <= 0x7FFFFFU) {
            return 0U;
        }
        phys = addr & 0x1FFFFFFFU;
        if (phys - 0x1FC00000U <= 0x7FFFFU) {
            const u8 *romPtr = &g_psxBiosRom[phys - 0x1FC00000U];
            switch (width) {
            case 0U:
                return (u32)(s32)*(const s8 *)romPtr;
            case 1U:
                return (u32)(s32)*(const s16 *)romPtr;
            case 2U:
                return *(const u32 *)romPtr;
            case 4U:
                return *romPtr;
            default:
                return *(const u16 *)romPtr;
            }
        }
    }

    if ((s32)(width - 4U) <= 0) {
        return (width == 4U) ? 0xFFU : 0xFFFFU;
    }
    return 0xFFFFFFFFU;
}

/* 0x007328: Slow-path PS1 I/O and cache-control write */
void popsIoWriteSlow(u32 addr, u32 value, u32 width)
{
    if (addr == 0xFFFE0130U) {
        g_pops->cacheControlReg = value;
        if (value == 0x0804U) {
            g_pops->cycleRemaining -= 32;
            popsJitFlushCodeCache();
        }
        return;
    }

    if (addr < 0x1F802040U || addr > 0x1F802041U) {
        if (addr != 0x1F802030U) {
            u32 phys = addr & 0x1FFFFFFFU;
            if (phys - 0x1FC00000U <= 0x7FFFFU) {
                g_pops->cycleRemaining -= (1 << (width & 3U)) + 15;
            }
        }
    }
}

/* 0x0074a4: PS1 DMA channel completion callback */
void popsDmaCompleteCallback(PopsEvent *ev)
{
    PopsDmaChannel *chan = (PopsDmaChannel *)ev;
    u32 chanId = chan->channelId;
    u32 dicr;

    g_pops->cycleRemaining -= chan->pendingCycles;
    g_pops->dmaRegs[chanId].chcr &= ~0x01000000U;
    g_pops->dmaRegs[chanId].bcr = 0U;

    dicr = g_pops->dmaDicr;
    if ((((dicr >> 23) & (dicr >> (chanId + 16U))) & 1U) != 0U) {
        g_pops->dmaDicr = dicr | (0x01000000U << chanId) | 0x80000000U;
        popsIrqRaise(8U);
    }

    if (g_pops->dmaPendingMask != 0U) {
        popsDmaRunPendingQueue();
    }
}

/* 0x007540: Stall lower-priority active DMA channels for bus contention */
u32 popsDmaDelayActiveChannels(u16 syncMask, u32 addCycles, u32 scaleLimit)
{
    u32 totalAdded = 0U;
    s32 chanIdx;

    for (chanIdx = 4; chanIdx >= 0; chanIdx--) {
        PopsDmaChannel *chan = &g_pops->dmaChannels[chanIdx];
        u32 curCycles = addCycles;
        s32 nowCycle;
        s32 remaining;

        if (chan->ev.next == NULL || (chan->syncMode & syncMask) == 0U) {
            continue;
        }
        if (chanIdx == 0) {
            if ((s32)(addCycles - 6144U) <= 0) {
                break;
            }
            curCycles = addCycles - 6144U;
        }

        nowCycle = (s32)(g_pops->cycleTarget - (u32)g_pops->cycleRemaining);
        remaining = (s32)chan->ev.dueCycle - nowCycle;
        if (remaining <= 0) {
            continue;
        }

        if ((u32)remaining < scaleLimit) {
            curCycles = (addCycles * (u32)remaining) / scaleLimit;
        }
        totalAdded += curCycles;

        chan->ev.prev->next = chan->ev.next;
        chan->ev.next->prev = chan->ev.prev;
        popsEventSchedule(&chan->ev, remaining + (s32)curCycles);
    }

    return totalAdded;
}

/* 0x007634: Multi-chunk DMA continuation callback */
void popsDmaContinueCallback(PopsEvent *ev)
{
    PopsDmaChannel *chan = (PopsDmaChannel *)ev;
    u32 chanId = chan->channelId;

    if (((g_pops->dmaDpcr >> (4U * chanId)) & 8U) != 0U) {
        s32 res = chan->transferFn(g_pops->dmaRegs[chanId].madr,
                                   g_pops->dmaRegs[chanId].bcr,
                                   g_pops->dmaRegs[chanId].chcr);
        if (res < 0) {
            g_pops->dmaRegs[chanId].madr = (u32)res & 0xFFFFFCU;
        } else {
            g_pops->dmaRegs[chanId].madr = 0U;
            chan->ev.callback = popsDmaCompleteCallback;
            if (res != 0) {
                popsEventSchedule(&chan->ev, res);
            } else {
                popsDmaCompleteCallback(&chan->ev);
            }
        }
    } else {
        if (chanId == 2U) {
            popsGpuBusyDoneCallback(NULL);
        }
        chan->ev.callback = popsDmaCompleteCallback;
    }
}

/* 0x007710: Round-robin dispatch of deferred DMA channels */
void popsDmaRunPendingQueue(void)
{
    u32 startIdx = g_pops->dmaRoundRobinIdx;
    u32 idx = startIdx;

    do {
        u16 pending = g_pops->dmaPendingMask;
        if (((pending >> idx) & 1U) != 0U) {
            g_pops->dmaPendingMask = (u16)(pending - (u16)(1U << idx));
            popsDmaStartChannel(idx);
            g_pops->dmaRoundRobinIdx = (u16)idx;
        }
        idx++;
        if (idx >= 7U) {
            idx = 0U;
        }
    } while (idx != startIdx);
}

/* 0x007788: Update MDEC DMA0/DMA1 addresses and bus contention cycles */
u32 popsDmaAdjustMdecCycles(u32 inDelta, u32 outDelta, u32 scaleLimit)
{
    g_pops->dmaRegs[0].madr += inDelta;
    g_pops->dmaChannels[0].syncMode = 5U;
    g_pops->dmaRegs[1].madr += outDelta;
    return popsDmaDelayActiveChannels(2U, (u32)((s32)(33U * (inDelta + outDelta)) >> 5), scaleLimit);
}

/* 0x0077d4: Start a PS1 DMA channel transfer (channels 0..6) */
void popsDmaStartChannel(u32 chanIdx)
{
    PopsDmaChannel *chan = &g_pops->dmaChannels[chanIdx];
    u32 chcr = g_pops->dmaRegs[chanIdx].chcr;
    u32 dpcr = g_pops->dmaDpcr;
    u32 priShift = 4U * chanIdx;
    s32 higherActive;
    s32 otherIdx;
    u32 syncMode;
    u32 byteCount;
    u32 madr;
    s32 xferRes;

    if (chan->ev.next != NULL) {
        return;
    }
    if ((chcr & 0x01000000U) == 0U || ((dpcr >> (priShift + 3U)) & 1U) == 0U) {
        return;
    }

    higherActive = (((dpcr >> 28) & 7U) < ((dpcr >> priShift) & 7U)) ? 0 : -1;
    for (otherIdx = 4; otherIdx >= 0; otherIdx--) {
        if (g_pops->dmaChannels[otherIdx].ev.next != NULL) {
            if ((g_pops->dmaRegs[otherIdx].chcr & 0x100U) != 0U) {
                g_pops->dmaPendingMask |= (u16)(1U << chanIdx);
                return;
            }
            if ((g_pops->dmaChannels[otherIdx].syncMode & 2U) != 0U) {
                if (((dpcr >> (4U * (u32)otherIdx)) & 7U) < ((dpcr >> priShift) & 7U)) {
                    higherActive++;
                }
                if (higherActive > 0) {
                    g_pops->dmaPendingMask |= (u16)(1U << chanIdx);
                    return;
                }
            }
        }
    }

    syncMode = (chcr >> 8) & 7U;
    byteCount = 0U;
    chan->pendingCycles = 0;

    if (syncMode != 4U) {
        u32 bcr = g_pops->dmaRegs[chanIdx].bcr;
        u32 words = bcr & 0xFFFFU;
        if (words == 0U) {
            words = 0x10000U;
        }
        if ((s32)(syncMode - 2U) >= 0) {
            if (syncMode != 2U) {
                return;
            }
            words *= (bcr >> 16);
        }
        byteCount = 4U * words;
    }

    madr = g_pops->dmaRegs[chanIdx].madr;
    chan->syncMode = (u16)syncMode;
    if ((madr & 0x800000U) != 0U) {
        popsDmaCompleteCallback(&chan->ev);
        return;
    }

    xferRes = chan->transferFn(madr & 0xFFFFFCU, byteCount, chcr);
    if (xferRes > 0) {
        u32 wordsDone = (u32)xferRes >> 2;
        u32 busCost = wordsDone << ((0x01035000U >> priShift) & ((chcr & 1U) ^ 7U));
        u32 curMadr = g_pops->dmaRegs[chanIdx].madr;
        u32 schedCycles;
        s32 cpuPenalty;

        g_pops->dmaRegs[chanIdx].madr = ((chcr & 2U) != 0U)
            ? (curMadr - 4U * wordsDone)
            : (curMadr + 4U * wordsDone);

        if (syncMode == 0U) {
            popsDmaDelayActiveChannels(7U, busCost + 3U, 0U);
            g_pops->cycleRemaining -= (s32)(busCost + 3U);
            popsDmaCompleteCallback(&chan->ev);
            return;
        }

        if (syncMode < 2U) {
            schedCycles = (((wordsDone - 1U) >> ((chcr >> 16) & 7U)) << ((chcr >> 20) & 7U)) +
                          3U * (u32)xferRes;
            popsDmaDelayActiveChannels(7U, schedCycles, 0U);
            cpuPenalty = 0;
        } else if (syncMode == 2U) {
            u32 blockCount = g_pops->dmaRegs[chanIdx].bcr >> 16;
            u32 baseDur;
            cpuPenalty = (s32)(busCost + 2U * blockCount);
            baseDur = (u32)cpuPenalty + 2U * blockCount +
                      popsDmaDelayActiveChannels(2U, (u32)cpuPenalty, (u32)cpuPenalty + 2U * blockCount);
            if (((g_pops->dmaDpcr >> 28) & 7U) < ((g_pops->dmaDpcr >> priShift) & 7U)) {
                cpuPenalty -= (s32)(4U * blockCount);
                if (cpuPenalty < 0) {
                    cpuPenalty = 0;
                }
                schedCycles = baseDur + 4U * blockCount;
            } else {
                schedCycles = baseDur;
            }
        } else {
            schedCycles = (u32)xferRes;
            cpuPenalty = 0;
        }

        popsEventSchedule(&chan->ev, (s32)schedCycles);
        chan->pendingCycles = cpuPenalty >> 1;
        g_pops->cycleRemaining -= cpuPenalty >> 1;
        return;
    }

    if (xferRes != 0) {
        g_pops->dmaRegs[chanIdx].madr = (u32)xferRes & 0xFFFFFCU;
    } else if (chanIdx < 2U) {
        chan->syncMode = 0U;
    }
}

/* 0x007ae0: PS1 DMA DPCR (0x1F8010F0) / DICR (0x1F8010F4) write */
void popsDmaWriteCtrl(u32 addr, u32 value, u32 width)
{
    if ((addr & 4U) != 0U) {
        u32 prev = g_pops->dmaDicr;
        u32 merged = value;
        u32 ackMask;
        u32 masterIrq;

        if (width != 2U) {
            u32 shift = (addr & 3U) * 8U;
            u32 rot = popsRotr32(prev & 0x80FF7FFFU, shift);
            if (width == 0U) {
                rot = (rot & 0xFFFFFF00U) | (value & 0xFFU);
            } else {
                rot = (rot & 0xFFFF0000U) | (value & 0xFFFFU);
            }
            merged = popsRotr32(rot, (32U - shift) & 31U);
        }

        ackMask = ((prev & ~merged) ^ merged) & 0x7F008000U;
        merged ^= ackMask;
        masterIrq = ((merged & 0x00800000U) != 0U && (merged & 0x7F000000U) != 0U) ? 1U : 0U;
        g_pops->dmaDicr = (merged & 0x7FFFFFFFU) | (masterIrq << 31);
    } else {
        u32 prev = g_pops->dmaDpcr;
        u32 newlyEnabled = value & ~prev;
        u32 chanIdx = 0U;

        g_pops->dmaDpcr = value;
        while (newlyEnabled != 0U) {
            if ((newlyEnabled & 8U) != 0U) {
                popsDmaStartChannel(chanIdx);
            }
            newlyEnabled >>= 4;
            chanIdx++;
        }
    }
}

/* 0x007bc8: PS1 DMA per-channel register write (0x1F801080..0x1F8010EF) */
void popsDmaWriteChannelReg(u32 addr, u32 value, u32 width)
{
    u32 regOff;
    u32 chanIdx;

    if ((s32)width < 2) {
        popsIoWriteDefault(addr, value, width);
        return;
    }

    regOff = addr & 0x7FU;
    chanIdx = (regOff >> 4) & 7U;
    ((u32 *)(void *)&g_pops->dmaRegs[0])[regOff >> 2] = value;

    if ((addr & 7U) == 0U) {
        if ((value & 0x01000000U) != 0U) {
            popsDmaStartChannel(chanIdx);
        } else {
            PopsDmaChannel *chan = &g_pops->dmaChannels[chanIdx];
            if (chan->ev.next != NULL) {
                if (chanIdx == 2U) {
                    chan->ev.callback = popsDmaCompleteCallback;
                    popsGpuBusyDoneCallback(NULL);
                }
                popsEventCancel(&chan->ev);
            }
            if (g_pops->dmaPendingMask != 0U) {
                popsDmaRunPendingQueue();
            }
        }
    }
}

/* 0x007c88: PS1 DMA6 (OTC reverse ordering table clear) */
s32 popsDmaExecuteOtc(u32 addr, u32 bytes, u32 chcr)
{
    u32 count = bytes;
    u32 startOff;
    u32 ramAddr;
    u32 curOff;

    if (chcr != 0x11000002U) {
        return 1;
    }
    if (addr < count) {
        count = addr;
    }
    if (count == 0U) {
        return 1;
    }

    startOff = addr - count;
    ramAddr = (startOff & 0x1FFFFFU) | POPS_PSX_RAM_BASE;

    if (count >= 0x2001U) {
        popsDcacheInvalidateLines(ramAddr + 4U, count);
        ramAddr |= 0x40000000U;
    }

    curOff = startOff + 4U;
    {
        volatile u32 *otcCursor = (volatile u32 *)(uintptr_t)ramAddr;
        otcCursor[1] = 0x00FFFFFFU;
        while (curOff != addr) {
            otcCursor[2] = curOff;
            otcCursor++;
            curOff += 4U;
        }
    }

    return (s32)count;
}

/* 0x007d80: Insert an event into the sorted PS1 cycle event queue */
s32 popsEventSchedule(PopsEvent *ev, s32 deltaCycles)
{
    s32 remaining = g_pops->cycleRemaining;
    PopsEvent *head = (PopsEvent *)(void *)&g_pops->eventHead;
    u32 targetCycle = g_pops->cycleTarget - (u32)remaining + (u32)deltaCycles;
    PopsEvent *prev = head;
    PopsEvent *cur = head->prev;
    s32 updateBudget;

    while (cur != head && (s32)(cur->dueCycle - targetCycle) <= 0) {
        prev = cur;
        cur = cur->prev;
    }

    updateBudget = (remaining > 0 && prev == head);
    ev->prev = cur;
    ev->next = prev;
    prev->prev = ev;
    cur->next = ev;

    if (updateBudget) {
        g_pops->cycleRemaining = deltaCycles;
        g_pops->cycleTarget = targetCycle;
    }
    ev->dueCycle = targetCycle;
    return updateBudget;
}

/* 0x007de8: Deliver a PS1 R3000A CPU exception (COP0 SR/CAUSE/vector) */
u32 *popsCpuRaiseException(u32 exCode)
{
    u32 sr = g_pops->copZeroRegs[12];
    u32 *entry;

    g_pops->copZeroRegs[13] = (g_pops->copZeroRegs[13] & 0xFFFFFFC3U) | ((exCode & 0xFU) << 2);
    if ((sr & 0x00400000U) != 0U) {
        g_pops->pc = 0xBFC00000U;
        entry = *(u32 *const *)(uintptr_t)POPS_JIT_LOOKUP_BIOS;
    } else {
        g_pops->pc = 0x80000080U;
        entry = (u32 *)(uintptr_t)g_pops->jitExceptionHandler;
    }

    g_pops->copZeroRegs[12] = (sr & 0xFFFFFFC0U) | ((sr << 2) & 0x3FU);
    if (entry == NULL) {
        entry = popsJitRecompile(g_pops->pc);
    }
    g_pops->jitReturnAddr = (u32)(uintptr_t)entry;
    return entry;
}

/* 0x007e60: Fire due PS1 events and check pending hardware interrupts */
u32 popsEventDispatch(void)
{
    PopsEvent *head = (PopsEvent *)(void *)&g_pops->eventHead;
    u32 nowCycle = g_pops->cycleTarget - (u32)g_pops->cycleRemaining;
    s32 nextDelta;
    s16 haltState;
    u32 sr;

    for (;;) {
        PopsEvent *ev = head->prev;
        u32 evCycle = ev->dueCycle;
        nextDelta = (s32)(evCycle - nowCycle);
        g_pops->cycleTarget = evCycle;

        if (nextDelta - 1 > 0) {
            break;
        }

        g_pops->cycleRemaining = 0;
        ev->prev->next = ev->next;
        ev->next->prev = ev->prev;
        ev->next = NULL;
        ev->callback(ev);
        nowCycle -= (u32)g_pops->cycleRemaining;
    }

    haltState = g_pops->haltFlags;
    sr = g_pops->copZeroRegs[12];

    if (haltState < 0) {
        popsResetMachine();
        return g_pops->jitReturnAddr;
    }

    g_pops->cycleRemaining = nextDelta;
    if (haltState != 0) {
        g_pops->haltFlags = 0;
        popsJitFlushCodeCache();
    }

    if ((sr & 1U) != 0U && (sr & g_pops->copZeroRegs[13] & 0xFF00U) != 0U) {
        u32 curPc = g_pops->pc;
        if (((curPc >> 23) & 0x3FU) == 0U) {
            for (;;) {
                u32 insn = *(const u32 *)(uintptr_t)((curPc & 0x1FFFFFU) | POPS_PSX_RAM_BASE);
                if ((insn >> 25) != 0x25U) {
                    break;
                }
                curPc += 4U;
                g_pops->cycleRemaining--;
                g_gteDispatchTable[insn & 0x3FU](insn);
            }
        }
        g_pops->copZeroRegs[14] = curPc;
        popsCpuRaiseException(0U);
    }

    return g_pops->jitReturnAddr;
}

/* 0x007f8c: Remove an event from the PS1 cycle event queue */
void popsEventCancel(PopsEvent *ev)
{
    PopsEvent *head = (PopsEvent *)(void *)&g_pops->eventHead;
    PopsEvent *prev = ev->prev;
    PopsEvent *next = ev->next;

    prev->next = next;
    next->prev = prev;
    if (next == head && g_pops->cycleRemaining > 0) {
        u32 nextDue = prev->dueCycle;
        g_pops->cycleRemaining += (s32)(nextDue - g_pops->cycleTarget);
        g_pops->cycleTarget = nextDue;
    }
    ev->next = NULL;
}

/* 0x007fd0: Check whether COP0 SR/CAUSE enables an immediate interrupt */
u32 popsCpuCheckIrq(void)
{
    u32 sr = g_pops->copZeroRegs[12];
    u32 cause = g_pops->copZeroRegs[13];
    u32 pending;

    if ((sr & 1U) == 0U) {
        cause = 0U;
    }
    pending = sr & cause & 0xFF00U;
    if (pending != 0U) {
        g_pops->cycleTarget -= (u32)g_pops->cycleRemaining;
        g_pops->cycleRemaining = 0;
    }
    return pending;
}

/* 0x008008: Assert a PS1 I_STAT interrupt bit and wake CPU if unmasked */
u32 popsIrqRaise(u32 irqBit)
{
    u32 prevStat = g_pops->irqStatus;
    u32 newStat = prevStat | irqBit;

    if (prevStat != newStat) {
        g_pops->irqStatus = newStat;
        if ((irqBit & g_pops->irqMask) != 0U) {
            u32 sr = g_pops->copZeroRegs[12];
            u32 newCause = g_pops->copZeroRegs[13] | 0x400U;
            u32 gated = newCause;
            u32 pending;

            g_pops->copZeroRegs[13] = newCause;
            if ((sr & 1U) == 0U) {
                gated = 0U;
            }
            pending = gated & sr & 0xFF00U;
            if (pending != 0U) {
                g_pops->cycleTarget -= (u32)g_pops->cycleRemaining;
                g_pops->cycleRemaining = 0;
            }
            return pending;
        }
    }
    return newStat;
}

/* 0x00806c..0x00814c: GTE command dispatch wrappers called via g_gteDispatchTable */
void popsGteMvmvaWrapper(u32 cmd)
{
    u32 mat = g_gteMvmvaTable[(cmd >> 17) & 3U] | (((cmd >> 19) & 1U) << 16);
    s32 limMin = (((cmd >> 10) & 1U) != 0U) ? 0 : -32768;
    u32 vec = g_gteMvmvaTable[4U + ((cmd >> 15) & 3U)];
    u32 trans = g_gteMvmvaTable[8U + ((cmd >> 13) & 3U)];

    popsGteMvmva(mat, limMin, vec, trans);
}

void popsGteSqrWrapper(u32 cmd)
{
    popsGteSqr(((cmd >> 19) & 1U) * 12U);
}

void popsGteGpfWrapper(u32 cmd)
{
    popsGteGpf(((cmd >> 19) & 1U) * 12U);
}

void popsGteGplWrapper(u32 cmd)
{
    popsGteGpl(((cmd >> 19) & 1U) * 12U);
}

void popsGteOpWrapper(u32 cmd)
{
    popsGteOp(((cmd >> 19) & 1U) * 12U);
}
