#include "pops_internal.h"

/* 0x014f34: Main emulator thread ("popsmain"), entered from module_start */
int popsMainThread(SceSize args, void *argp)
{
    SceUID powerCb;
    SceUID msCb;
    s32 rc;

    (void)args;
    (void)argp;

    /* Relocate stack pointer into the top of the 15.98 MB POPS stack region */
    __asm__ __volatile__("lui $sp, 0x09bf" ::: "memory");

    scePaf_A138A376(g_pops->buildBranchString, "branches/pops-for-vita-350/pops/src(r%d)", 13921);
    g_pops->bootInitFlag = 0xFFU;

    powerCb = sceKernelCreateCallback("powerCB", popsPowerCallback, NULL);
    scePowerRegisterCallback(-1, powerCb);

    msCb = sceKernelCreateCallback("msCB", popsMsCallback, NULL);
    sceIoDevctl("fatms0:", 0x02415821U, &msCb, 4, NULL, 0);

    popsPatchSyscallStubs();
    POPS_SHARED_MEM->screenMode = 0U;

    rc = popsDiscMountPbp();
    if (rc < 0) {
        s32 errCode = (rc == (s32)0x80010005U) ? (s32)0x80010005U : (s32)0x80010089U;
        scePopsMan_0090B2C8(errCode);
    } else if (popsInitSavedataConfig() != 0 ||
               popsDiscSelectImage() != 0 ||
               popsInitMcWorker() < 0 ||
               popsPadResetAll() < 0) {
        scePopsMan_0090B2C8((s32)0x80010089U);
    }

    popsInitAllSubsystems();
    popsMcSyncBeforeRun();
    sceKermitPeripheral_C0EBC631();
    popsMcInitPocketStation();

    if (POPS_SHARED_MEM->screenMode == 0U) {
        popsJitEnter();
    }
    for (;;) {
        popsDiscSelectImage();
        popsJitEnter();
    }
}

/* 0x014ea0: PRX entry point */
int module_start(SceSize args, void *argp)
{
    SceUID thid;
    s32 started;

    (void)args;
    (void)argp;

    sceKernelSetCompiledSdkVersion660(0x06060110U);
    sceKernelSetCompilerVersion(0x00030306U);
    sceDisplaySetHoldMode(1);

    thid = sceKernelCreateThread("popsmain", popsMainThread, 32, 0x00F3D900, 0x80104000U, NULL);
    started = sceKernelStartThread(thid, 0, NULL);
    if (started < 0) {
        s32 errCode = (started == (s32)0x80010005U) ? (s32)0x80010005U : (s32)0x80010089U;
        scePopsMan_0090B2C8(errCode);
        popsMainThread(0, NULL);
    }
    return 0;
}

/* 0x015088: Publish SPU quirk bytes to ME shared memory */
void popsPublishSpuQuirksA(void)
{
    const PopsScratchpad *spad = (const PopsScratchpad *)POPS_SCRATCHPAD_ADDR;
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    u32 extFlags = (u32)spad->quirkFlagsExt;
    u32 dmaLat = spad->spuDmaLatency;
    u8 flagsByte = (u8)((((extFlags >> 2) & 1U) << 7) +
                        (((extFlags >> 3) & 1U) << 1) +
                        ((dmaLat >> 18) & 1U));
    spu->quirkConfigBytes[3] = flagsByte;
    spu->quirkConfigBytes[2] = (u8)((spad->quirkFlags >> 7) & 1U);
    spu->quirkConfigBytes[1] = (u8)(((s32)dmaLat >> 8) - (s32)dmaLat);
    spu->quirkConfigBytes[0] = (u8)dmaLat;
}

/* 0x0150e0: Second copy of SPU quirk publisher called from quirk loader */
void popsPublishSpuQuirksB(void)
{
    const PopsScratchpad *spad = (const PopsScratchpad *)POPS_SCRATCHPAD_ADDR;
    volatile PopsMeSpuShared *spu = POPS_ME_SPU_SHARED;
    u32 extFlags = (u32)spad->quirkFlagsExt;
    u32 dmaLat = spad->spuDmaLatency;
    u8 flagsByte = (u8)((((extFlags >> 2) & 1U) << 7) +
                        (((extFlags >> 3) & 1U) << 1) +
                        ((dmaLat >> 18) & 1U));
    spu->quirkConfigBytes[3] = flagsByte;
    spu->quirkConfigBytes[2] = (u8)((spad->quirkFlags >> 7) & 1U);
    spu->quirkConfigBytes[1] = (u8)(((s32)dmaLat >> 8) - (s32)dmaLat);
    spu->quirkConfigBytes[0] = (u8)dmaLat;
}

/* 0x015138: Initialize JIT code cache pools and zero the 2.5 MB lookup tables */
u32 popsInitJitPools(void)
{
    s32 shiftVal = (s32)g_pops->jitRamLimitQuirk;
    u32 curAddr = POPS_JIT_LOOKUP_RAM;

    if (shiftVal < 0) {
        shiftVal = ((g_pops->jitModeFlags & 2) != 0) ? 1 : 0;
    }
    g_pops->blockCycleShift = (s8)shiftVal;
    g_pops->jitRamCodePtr = POPS_JIT_CODE_BASE;
    g_pops->jitBiosCodePtr = 0x09B80000U;
    g_pops->jitBiosExitTrampoline = 0U;

    while (curAddr != 0x09E80000U) {
        u32 uncached = curAddr | 0x40000000U;
        __builtin_allegrex_cache(0x19, (void *)(uintptr_t)curAddr);
        __asm__ __volatile__("sync\n\tsv.q R403.q, 0(%0), wb" :: "r"(uncached) : "memory");
        __builtin_allegrex_cache(0x19, (void *)(uintptr_t)(curAddr + 64U));
        __asm__ __volatile__(
            "sync\n\t"
            "sv.q R403.q, 0x10(%0), wb\n\t"
            "sv.q R403.q, 0x20(%0), wb\n\t"
            "sv.q R403.q, 0x30(%0), wb\n\t"
            "sv.q R403.q, 0x40(%0), wb\n\t"
            "sv.q R403.q, 0x50(%0), wb\n\t"
            "sv.q R403.q, 0x60(%0), wb\n\t"
            "sv.q R403.q, 0x70(%0), wb\n\t"
            "mfvc $zero, $135"
            :: "r"(uncached) : "memory");
        curAddr += 128U;
    }

    return curAddr;
}

/* 0x0151b4: Initialize SPU scratchpad state, DMA4, and SPU MMIO handlers */
void popsInitSpu(void)
{
    u32 *clearPtr = (u32 *)(void *)g_pops->spuTransferFifo;
    u32 wordsRem = (0x390U - 0x308U) / 4U;

    popsPublishSpuQuirksA();
    while (wordsRem-- != 0U) {
        *clearPtr++ = 0U;
    }

    g_pops->spuIrqEvent.callback = popsSpuIrqPollCallback;
    g_pops->spuDmaEvent.callback = popsSpuDmaDeferredCallback;
    popsRegisterDmaChannel(4U, popsSpuDmaTransfer);
    popsRegisterIoRange(0x1F801C00U, 0x260U, popsSpuReadReg, NULL);
    popsRegisterIoRange(0x1F801C00U, 0x200U, popsSpuReadReg, popsSpuWriteReg);
}

/* 0x01524c: Register read/write handlers for an 8-byte-aligned PS1 I/O range */
void popsRegisterIoRange(u32 startAddr, u32 byteLen,
                         u32 (*readFn)(u32, u32),
                         void (*writeFn)(u32, u32, u32))
{
    u32 startIdx;
    u32 endIdx;
    u32 idx;

    if (readFn == NULL) {
        readFn = popsIoReadDefault;
    }
    if (writeFn == NULL) {
        writeFn = popsIoWriteDefault;
    }

    startIdx = (startAddr & 0xFFFU) >> 3;
    endIdx = startIdx + (byteLen >> 3);
    for (idx = startIdx; idx < endIdx; idx++) {
        g_pops->ioTable[idx].readFn = readFn;
        g_pops->ioTable[idx].writeFn = writeFn;
    }
}

/* 0x0152a4: Initialize PS1 scratchpad RAM and default slow-I/O table */
void popsInitSlowIo(void)
{
    u32 fillVal = g_pops->padPollQuirk + 1U;
    u32 *spadWords = (u32 *)(void *)g_pops->psxScratchpad;
    u32 idx;

    for (idx = 0U; idx < 256U; idx++) {
        spadWords[idx] = fillVal;
    }

    popsRegisterIoRange(0x1F801000U, 0x1000U, popsIoReadSlow, popsIoWriteSlow);
    popsRegisterIoRange(0x1F801060U, 0x10U, NULL, NULL);
    popsRegisterIoRange(0x1F801000U, 0x40U, NULL, NULL);
}

/* 0x015330: Initialize PS1 DMA controller (7 channels + OTC + DPCR/DICR) */
void popsInitDma(void)
{
    u32 *clearPtr = (u32 *)(void *)g_pops->dmaChannels;
    u32 wordsRem = (sizeof(g_pops->dmaChannels) + 4U) / 4U;

    while (wordsRem-- != 0U) {
        *clearPtr++ = 0U;
    }

    g_pops->dmaChannels[6].ev.callback = popsDmaCompleteCallback;
    g_pops->dmaChannels[6].channelId = 6U;
    g_pops->dmaDpcr = 0x77777777U;
    g_pops->dmaChannels[6].transferFn = popsDmaExecuteOtc;

    popsRegisterIoRange(0x1F8010E0U, 8U, NULL, NULL);
    popsRegisterIoRange(0x1F8010E8U, 8U, NULL, popsDmaWriteChannelReg);
    popsRegisterIoRange(0x1F8010F0U, 8U, NULL, popsDmaWriteCtrl);
}

/* 0x0153dc: Bind a PS1 DMA channel callback and register its MMIO range */
void popsRegisterDmaChannel(u8 chanIdx, s32 (*transferFn)(u32, u32, u32))
{
    u32 baseAddr = 0x1F801080U + 16U * (u32)chanIdx;

    g_pops->dmaChannels[chanIdx].transferFn = transferFn;
    g_pops->dmaChannels[chanIdx].ev.callback = popsDmaCompleteCallback;
    g_pops->dmaChannels[chanIdx].channelId = chanIdx;

    popsRegisterIoRange(baseAddr, 8U, NULL, NULL);
    popsRegisterIoRange(baseAddr + 8U, 8U, NULL, popsDmaWriteChannelReg);
}

/* 0x015460: Initialize the PS1 cycle event queue sentinel */
void popsInitEventQueue(void)
{
    PopsEvent *head = (PopsEvent *)(void *)&g_pops->eventHead;

    g_pops->cycleTarget = 33868800U;
    g_pops->cycleRemaining = 33868800;
    head->prev = head;
    head->next = head;
}

/* 0x015480: Initialize PS1 R3000A COP0/PRID/reset PC and VFPU constant registers */
void popsInitCpuAndBios(void)
{
    g_pops->copZeroRegs[12] = 0x00400000U;
    g_pops->copZeroRegs[15] = 0x00000001U;
    g_pops->gte.named.lzcr = 32U;
    g_pops->pc = 0xBFC00000U;
    popsVfpuInitConstants();
}

/* 0x0154bc: Register PS1 I_STAT / I_MASK MMIO handlers */
void popsInitIrqIo(void)
{
    popsRegisterIoRange(0x1F801070U, 8U, popsIrqReadReg, popsIrqWriteReg);
}

/* 0x0154f0: Populate GTE UNR reciprocal lookup table below 0x09800000 */
void popsInitGteUnrTable(void)
{
    u32 denom;
    u32 *dst = (u32 *)0x097E0000U;

    for (denom = 0x8000U; denom <= 0xFFFFU; denom++) {
        s32 idx = (s32)(((denom + 64U) >> 7) - 256U);
        s32 clamped = __builtin_allegrex_min(idx, 255);
        s32 unr = (clamped + 262400) / (2 * clamped + 512);
        u32 corr = 128U - (u32)unr * denom;
        *dst-- = (((((corr >> 8) & 0x1FFFFU) * (u32)unr) + 128U) >> 8) << 14;
    }
}

/* 0x015584: Initialize PS1 root counters 0..2 and register MMIO */
void popsInitTimers(void)
{
    u32 idx;
    u8 irqBit = 16U;

    for (idx = 0U; idx < 3U; idx++) {
        PopsRootCounter *tmr = &g_pops->timers[idx];
        u32 ioBase = 0x1F801100U + 16U * idx;

        tmr->irqBit = irqBit;
        tmr->clock.shifts.divShift = 0U;
        tmr->targetAndFreeze = 0x10000;
        tmr->ev.callback = popsTimerEventCallback;

        popsRegisterIoRange(ioBase, 8U, popsTimerReadReg, popsTimerWriteReg);
        popsRegisterIoRange(ioBase + 8U, 8U, NULL, popsTimerWriteReg);
        irqBit <<= 1;
    }
}

/* 0x015670: Initialize PS1 SIO0 / SIO1 channels and MMIO */
void popsInitSio(void)
{
    g_pops->sio[1].statusReg = 0x195U;
    g_pops->sio[1].transferCallback = popsSioNop;
    g_pops->sio[0].portBit = 1U;
    g_pops->sio[1].portBit = 2U;
    g_pops->sio[0].statusReg = 5U;
    g_pops->sio[0].transferCallback = popsSioTransferCallback;

    popsRegisterIoRange(0x1F801040U, 8U, popsSioReadReg, popsSioWriteData);
    popsRegisterIoRange(0x1F801050U, 8U, popsSioReadReg, popsSioWriteData);
    popsRegisterIoRange(0x1F801048U, 8U, NULL, popsSioWriteCtrl);
    popsRegisterIoRange(0x1F801058U, 8U, NULL, popsSioWriteCtrl);
}

/* 0x015748: No-op SIO1 transfer callback */
void popsSioNop(PopsSioChannel *chan)
{
    (void)chan;
}

/* 0x015750: Disconnected SIO0 device handler */
s32 popsSioNullDevice(u32 byteIdx, u32 portSel, u8 txByte)
{
    (void)byteIdx;
    (void)portSel;
    (void)txByte;
    return 0x1FF;
}

/* 0x015758: Apply controller port assignment, analog mode, and multitap state */
s32 popsPadApplyConfig(s32 assignMode, s32 analogMode, s32 analogAllow)
{
    u8 wantAnalog = (analogMode != 0) ? 1U : 0U;
    u8 wantAllow = (analogAllow != 0) ? 1U : 0U;
    u32 idx;

    for (idx = 0U; idx < 8U; idx++) {
        PopsPadSlot *pad = &g_pops->pads[idx];
        pad->pspPadIndex = ((s32)idx == assignMode) ? -1 : (s8)idx;

        if (pad->analogEnabled != wantAnalog) {
            if (pad->modeLocked == 0U) {
                pad->analogEnabled = wantAnalog;
                pad->analogAllowed = wantAllow;
                pad->configMode = 0U;
                pad->modeId = (wantAnalog != 0U) ? 0x73U : 0x41U;
                pad->statusByte = 1U;
                pad->dualshockMode = 0U;
                pad->buttons = 0xFFFFU;
                pad->rightStick = 0x8080U;
                pad->leftStick = 0x8080U;
            } else {
                popsGpuSetAnalogMode(pad->analogEnabled);
            }
        }
    }

    if ((assignMode & 0x80) != 0) {
        u32 tapBits = (u32)g_pops->quirkFlagsExt;
        u32 port;

        if ((tapBits & 0x10000000U) != 0U) {
            tapBits &= (u32)assignMode;
        }
        tapBits &= 3U;

        for (port = 0U; port < 2U; port++) {
            u8 wantTap = (u8)((tapBits >> port) & 1U);
            PopsMultitapState *tap = &g_pops->taps[port];

            if (tap->enabled != wantTap) {
                tap->connectTimer = 60U;
                tap->enabled = wantTap;
            }
            if (tap->connectTimer != 0U) {
                tap->connectTimer--;
                if (tap->connectTimer == 0U) {
                    if (wantTap != 0U) {
                        scePaf_D9E2D6E1(tap->responseBuf, 0xFF, 32U);
                    } else {
                        popsPadResetAll();
                    }
                }
            }
        }
    }

    return 0;
}

/* 0x015920: Reset all 8 pad slots, multitaps, and PSP controller sampling mode */
s32 popsPadResetAll(void)
{
    u32 idx;

    sceCtrlSetSamplingMode(1);
    sceCtrlSetIdleCancelThreshold(64, 64);

    for (idx = 0U; idx < 8U; idx++) {
        PopsPadSlot *pad = &g_pops->pads[idx];
        u8 savedAllow = pad->analogAllowed;
        u8 savedAnalog = pad->analogEnabled;
        u32 *words = (u32 *)(void *)pad;
        u32 wordIdx;

        POPS_SHARED_MEM->padModes[idx] = 0U;
        for (wordIdx = 0U; wordIdx < 8U; wordIdx++) {
            words[wordIdx] = 0U;
        }
        pad->analogAllowed = savedAllow;
        pad->pspPadIndex = (s8)idx;
        pad->analogEnabled = savedAnalog;
        pad->modeId = (savedAnalog != 0U) ? 0x73U : 0x41U;
        pad->dualshockMode = 0U;
        pad->statusByte = 1U;
        pad->buttons = 0xFFFFU;
        pad->rightStick = 0x8080U;
        pad->leftStick = 0x8080U;
    }

    for (idx = 0U; idx < 2U; idx++) {
        PopsMultitapState *tap = &g_pops->taps[idx];
        u8 savedEnable = tap->enabled;
        u32 *words = (u32 *)(void *)tap;
        u32 wordIdx;

        for (wordIdx = 0U; wordIdx < 9U; wordIdx++) {
            words[wordIdx] = 0U;
        }
        tap->enabled = savedEnable;
    }

    sceCtrlSetSamplingCycle(((g_pops->quirkFlags & 0x00100000U) != 0U) ? 8341 : 0);
    return 0;
}

/* 0x015a28: Return 0 if savedata idle and no memory card slot is dirty, else -1 */
s32 popsMcIsWritebackIdle(void)
{
    if (sceUtilitySavedataGetStatus() != 0) {
        return -1;
    }
    if ((g_mcSlots[0].statusFlags & 2U) != 0U || (g_mcSlots[1].statusFlags & 2U) != 0U) {
        return -1;
    }
    return 0;
}

/* 0x015a90: Step asynchronous custom-title VMC reload state machine */
s32 popsMcPollInitialLoad(void)
{
    if (g_vmcInitState == 0U) {
        g_vmcInitState = 1U;
        g_vmcLoadStatusA = (u32)popsVmcLoadByCustomTitle(0U);
        return 0;
    }
    if (g_vmcInitState == 1U) {
        if (sceUtilitySavedataGetStatus() > 0) {
            return 0;
        }
        if (g_vmcLoadStatusA != 0U) {
            return -1;
        }
        g_vmcInitState = 2U;
        g_vmcLoadStatusB = (u32)popsVmcLoadByCustomTitle(1U);
        return 0;
    }
    if (g_vmcInitState == 2U) {
        if (sceUtilitySavedataGetStatus() > 0) {
            return 0;
        }
        if (g_vmcLoadStatusB != 0U) {
            return -1;
        }
        g_vmcInitState = 5U;
        return 0;
    }
    return (g_vmcInitState == 5U) ? 1 : 0;
}

/* 0x015b70: Writeback both memory card image buffers from CPU D-cache */
void popsMcFlushBothSlots(void)
{
    sceKernelDcacheWritebackRange(g_mcSlots[0].vmpHeader, 131200U);
    sceKernelDcacheWritebackRange(g_mcSlots[1].vmpHeader, 131200U);
}

/* 0x015bac: Insert or eject a virtual memory card slot */
s32 popsMcSetSlotInserted(u32 slotIdx, s32 inserted)
{
    if (inserted <= 0) {
        g_mcSlots[slotIdx].statusFlags &= (u8)~1U;
        return ((s32 (*)(const void *, u32))sceKernelDcacheInvalidateRangeForUser)(
            g_mcSlots[slotIdx].vmpHeader, 131200U);
    }
    g_mcSlots[slotIdx].flagByte = 8U;
    g_mcSlots[slotIdx].statusFlags |= 1U;
    return 0x20000;
}

/* 0x015c30: Wait for pending MC writeback alarm and boost main thread priority */
s32 popsMcSyncBeforeRun(void)
{
    while (g_mcWritebackAlarm >= 0) {
        sceKernelDelayThread(1000U);
    }
    sceKernelDcacheWritebackRange(g_mcSlots[0].vmpHeader, 131200U);
    sceKernelDcacheWritebackRange(g_mcSlots[1].vmpHeader, 131200U);
    return sceKernelChangeThreadPriority(0, 17);
}

/* 0x015ca0: Create memory card writeback semaphore and background mcworker thread */
SceUID popsInitMcWorker(void)
{
    SceUID thid;

    g_mcWritebackSema = sceKernelCreateSema("mcWriteBack", 0U, 0, 1, NULL);
    thid = sceKernelCreateThread("mcworker", popsMcWorkerThread, 27, 4096, 0x00102000U, NULL);
    sceKernelStartThread(thid, 0, NULL);
    return thid;
}

/* 0x015d20: Query PocketStation availability from ME audio helper */
s32 popsMcInitPocketStation(void)
{
    g_mcSlots[0].pocketEnable = (u8)sceMeAudio_C47D3670();
    return 0;
}

/* 0x015d50: Return whether PocketStation mode is enabled on slot 0 */
u32 popsMcGetPocketEnable(void)
{
    return g_mcSlots[0].pocketEnable;
}

/* 0x015d5c: 1-second deferred writeback alarm handler */
u32 popsMcWritebackAlarmCb(void *common)
{
    (void)common;
    if (sceUtilitySavedataGetStatus() == 0) {
        sceKernelSignalSema(g_mcWritebackSema, 1);
        g_mcWritebackAlarm = -1;
        return 0U;
    }
    return 500000U;
}

/* 0x015da8: Background memory card loader & writeback worker thread ("mcworker") */
int popsMcWorkerThread(SceSize args, void *argp)
{
    s32 allEmpty = 1;
    u32 slotIdx;

    (void)args;
    (void)argp;

    scePaf_D9E2D6E1(g_mcSlots, 0, sizeof(g_mcSlots));

    for (slotIdx = 0U; slotIdx < 2U; slotIdx++) {
        popsSavedataBindSlotBuf(0U, slotIdx, g_mcSlots[slotIdx].vmpHeader);
        if (popsVmcLoadCurrentTitle(slotIdx) != 0) {
            s32 saveErr = popsSavedataGetResult(0U);
            s32 ok = 0;

            if (popsSavedataClassifyError(saveErr) != 2) {
                scePopsMan_0090B2C8((s32)0x80010005U);
                return 0;
            }
            popsMcfsFormatCard(slotIdx);
            if (popsSavedataSaveVmc(slotIdx) >= 0) {
                s32 res;
                popsSavedataWaitIdle();
                res = popsSavedataGetResult(0U);
                if (res != 0 && popsGetVmcCreatedFlag() != 0) {
                    res = 0;
                }
                if (res >= 0) {
                    ok = 1;
                }
            }
            if (!ok) {
                if (slotIdx != 0U) {
                    scePopsMan_0090B2C8((s32)0x80010005U);
                    return 0;
                }
                popsSavedataSaveConfig();
                popsSavedataWaitIdle();
                if (popsSavedataGetResult(0U) != 0 || popsSavedataSaveVmc(0U) < 0) {
                    scePopsMan_0090B2C8((s32)0x80010005U);
                    return 0;
                }
                popsSavedataWaitIdle();
                {
                    s32 res = popsSavedataGetResult(0U);
                    if (res != 0 && popsGetVmcCreatedFlag() != 0) {
                        res = 0;
                    }
                    if (res < 0) {
                        scePopsMan_0090B2C8((s32)0x80010005U);
                        return 0;
                    }
                }
            }
        }

        popsMcfsRepairEmptyBlocks(slotIdx);
        allEmpty &= popsMcfsIsCardEmpty(slotIdx);
        g_mcSlots[slotIdx].statusFlags |= 1U;
        g_mcSlots[slotIdx].flagByte = 0U;
    }

    if (allEmpty != 0) {
        popsCheckVmcCreated();
    }
    popsNullStubA();
    popsGpuSetCustomParam(0U, (u32)(uintptr_t)g_mcSlots[0].vmpHeader);
    popsGpuSetCustomParam(1U, (u32)(uintptr_t)g_mcSlots[1].vmpHeader);
    g_mcWritebackAlarm = -1;

    for (;;) {
        s32 flushedCount = 0;
        s32 unlockIdx;

        sceKernelWaitSema(g_mcWritebackSema, 1, NULL);
        for (slotIdx = 0U; slotIdx < 2U; slotIdx++) {
            u32 irqState;
            u8 flags;

            __asm__ __volatile__("mfic %0, $0\n\tmtic $zero, $0" : "=r"(irqState));
            flags = g_mcSlots[slotIdx].statusFlags;
            g_mcSlots[slotIdx].statusFlags = (u8)(flags - (flags & 2U));
            __asm__ __volatile__("mtic %0, $0" :: "r"(irqState));

            if ((flags & 2U) != 0U) {
                flushedCount++;
                popsSavedataSaveVmc(slotIdx);
            }
        }

        for (unlockIdx = flushedCount - 1; unlockIdx >= 0; unlockIdx--) {
            sceKermitPeripheral_D27C5E03(1U);
            sceKernelPowerUnlock(0);
        }
    }
}

/* 0x016038: Begin multi-disc lid-open sequence and schedule lid-close timer */
void popsCdStartDiscSwap(void)
{
    s32 lidDelay = (s32)(67737600U * g_pops->cdReadLatency + 135475200U);

    g_pops->cdStatByte = 0x10U;
    g_pops->cdErrorCode = 8U;
    g_pops->cdErrorStatusMask = 4U;
    g_pops->cdTrayState = 1U;
    popsCdEmitImmediateResp(5);

    if (g_pops->cdSpinupEvent.next != NULL) {
        popsEventCancel(&g_pops->cdSpinupEvent);
    }
    if (g_pops->cdLidEvent.next != NULL) {
        popsEventCancel(&g_pops->cdLidEvent);
    }
    g_pops->cdXaInterleave = 0U;
    g_pops->cdXaNextLba = 0U;
    if (g_pops->cdReadEvent.next != NULL) {
        popsEventCancel(&g_pops->cdReadEvent);
    }

    g_pops->cdStatByte &= 0x17U;
    g_pops->cdLidEvent.callback = popsCdLidOpenCallback;
    popsEventSchedule(&g_pops->cdLidEvent, lidDelay);
}

/* 0x016104: Reset CD-ROM controller state, DMA3, and MMIO handlers */
void popsCdReset(void)
{
    u8 savedMuted = g_pops->cdMuted;
    u8 savedInit = g_pops->cdInitialized;
    u8 savedSpeedQuirk;
    u32 *clearPtr;
    u32 wordsRem;

    g_pops->cdXaInterleave = 0U;
    g_pops->cdXaNextLba = 0U;
    if (g_pops->cdReadEvent.next != NULL) {
        popsEventCancel(&g_pops->cdReadEvent);
    }
    g_pops->cdStatByte &= 0x17U;
    if (g_pops->cdRespEvent.ev.next != NULL) {
        popsEventCancel(&g_pops->cdRespEvent.ev);
    }
    if (g_pops->cdCmdEvent.ev.next != NULL) {
        popsEventCancel(&g_pops->cdCmdEvent.ev);
    }
    if (g_pops->cdLidEvent.next != NULL) {
        popsEventCancel(&g_pops->cdLidEvent);
    }
    if (g_pops->cdSpinupEvent.next != NULL) {
        popsEventCancel(&g_pops->cdSpinupEvent);
    }

    savedSpeedQuirk = g_pops->cdSpeedQuirkByte;
    clearPtr = (u32 *)(void *)&g_pops->cdCmdEvent;
    wordsRem = (14520U - 14336U) / 4U;
    while (wordsRem-- != 0U) {
        *clearPtr++ = 0U;
    }

    g_pops->cdInitialized = savedInit;
    g_pops->cdStatusPort &= (u8)~0x20U;
    g_pops->cdSpeedQuirkByte = savedSpeedQuirk;
    g_pops->cdCmdEvent.ev.callback = popsCdDeliverRespCallback;
    g_pops->cdRespEvent.ev.callback = popsCdDeliverRespCallback;
    g_pops->cdSpinupEvent.callback = popsCdSpinupCallback;
    g_pops->cdMuted = savedMuted;
    g_pops->cdStatByte = 2U;
    g_pops->cdModeReg = 0x20U;
    g_pops->cdSectorOffset = 12U;
    g_pops->cdSectorLimit = 2352U;
    g_pops->cdRngState = ((g_pops->cdSpeedScale & 0x800U) != 0U) ? sceKernelGetSystemTimeLow() : 0U;
    g_pops->cdQueuedCmd = -1;

    popsDiscStopCddaTrack();
    POPS_ME_SPU_SHARED->cdAudioVol = g_pops->cdAudioVol.word;
    popsRegisterDmaChannel(3U, popsCdDmaTransfer);
    popsRegisterIoRange(0x1F801800U, 0x10U, popsCdReadReg, popsCdWriteReg);
}
