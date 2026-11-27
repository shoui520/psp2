#include "pops_internal.h"

/*
 * PS1 GPU (GP0 / GP1) -> PSP GE Display List Translator & Frame Pacing
 *
 * Translates PS1 GP0 drawing packets (polygons, lines, sprites, fill,
 * VRAM-to-VRAM, CPU-to-VRAM, VRAM-to-CPU) into PSP GE command lists in
 * uncached RAM (0x49A00000..0x49B7B800), maintains the 32-page swizzled
 * texture cache, and manages NTSC/PAL vblank timing and display flips.
 */

static __inline__ s32 popsMinS32(s32 a, s32 b)
{
    return (a < b) ? a : b;
}

static __inline__ s32 popsMaxS32(s32 a, s32 b)
{
    return (a > b) ? a : b;
}

static __inline__ s32 popsAbsS32(s32 v)
{
    return (v < 0) ? -v : v;
}

static __inline__ u32 popsClz32(u32 v)
{
    u32 res;
    __asm__("clz %0, %1" : "=r"(res) : "r"(v));
    return res;
}

static __inline__ void popsCacheDataHitWbInv(const void *addr)
{
    __asm__ __volatile__("cache 0x1B, 0(%0)" :: "r"(addr) : "memory");
}

static __inline__ void popsCacheDataHitWb(const void *addr)
{
    __asm__ __volatile__("cache 0x1A, 0(%0)" :: "r"(addr) : "memory");
}

static __inline__ void popsCacheDataHitInv(const void *addr)
{
    __asm__ __volatile__("cache 0x19, 0(%0)" :: "r"(addr) : "memory");
}

static __inline__ void popsCacheCreateDirtyGpu(const void *addr)
{
    __asm__ __volatile__("cache 0x18, 0(%0)" :: "r"(addr) : "memory");
}

static __inline__ s32 popsSignExtend11(u32 rawBits)
{
    return ((s32)(rawBits << 21)) >> 21;
}

static __inline__ s32 popsSignExtend11High(u32 rawWord)
{
    return ((s32)(rawWord << 5)) >> 21;
}

static __inline__ u32 popsGeEncodeFloatCoord(s32 coord)
{
    u32 bits = popsFloatBits((float)((coord * 2) + 1));
    return popsRotr32(bits + 59U, 8U);
}

static __inline__ void popsGpuInvalidateTexRect(u32 xStart, u32 width, u32 yStart, u32 height, s32 checkActivePage)
{
    u32 startCol = (xStart >> 6) & 0xFU;
    u32 endCol = ((xStart + width + 191U) >> 6) & 0xFU;
    s32 startOff = g_pops->texPages[startCol].groupOffset;
    s32 endOff = g_pops->texPages[endCol].groupOffset;
    u32 colBegin = (u32)((s32)startCol - startOff);
    u32 colLimit = (endOff != 3) ? (u32)((s32)endCol - endOff) : endCol;

    if ((s32)startCol != startOff && startCol == colBegin && colBegin != colLimit) {
        colBegin -= 3U;
    }
    if (width >= 961U) {
        colLimit = colBegin;
    }

    {
        u32 rowCur = ((yStart & 0x100U) != 0U) ? 1U : 0U;
        u32 rowEnd = (((yStart + height + 255U) & 0x100U) != 0U) ? 1U : 0U;
        u32 activePage = g_pops->gpuTexPageState & 0x1FU;

        do {
            u32 col = colBegin;
            do {
                u32 pageIdx = (rowCur * 16U) + col;
                if (checkActivePage != 0 && activePage >= pageIdx && activePage <= (pageIdx + 2U)) {
                    g_pops->gpuTexPageState |= 0x8000U;
                }
                g_pops->texPages[pageIdx].loadedFormat = 0U;
                if (col == 15U) {
                    col = 0U;
                } else {
                    g_pops->texPages[pageIdx + 1U].loadedFormat = 0U;
                    g_pops->texPages[pageIdx + 2U].loadedFormat = 0U;
                    col += 3U;
                }
            } while (col != colLimit);
            rowCur ^= 1U;
        } while (rowCur != rowEnd);
    }
}

static __inline__ u32 *popsGpuUploadTexPage(u32 *gePtr, u32 tpage, s32 emitScaleSuffix)
{
    u32 depthCode = (tpage >> 7) & 3U;
    u32 winOffY = g_pops->gpuTexWinOffY;
    u32 winOffX = g_pops->gpuTexWinOffX;
    u32 pageX = tpage & 0xFU;
    u32 pageY = (tpage & 0x10U) != 0U ? 1U : 0U;
    u32 bufStride = 1024U;
    u32 geTexFmt;
    u32 texAddr;
    u32 *outPtr = gePtr + 2;

    g_pops->gpuTexPageState = (u16)tpage;
    gePtr[0] = 0x13041B90U;
    gePtr[1] = 0x0A000080U | (((tpage >> 5) & 3U) << 4);

    if ((depthCode & 2U) != 0U) {
        u32 pxY = (8U * ((32U * pageY) + winOffY)) & 0x1FFU;
        u32 pxX = (8U * ((8U * pageX) + winOffX)) & 0x3FFU;
        texAddr = POPS_VRAM_CACHED_BASE | (pxY << 11) | (pxX * 2U);
        depthCode = 2U;
        geTexFmt = 1U;
    } else {
        u32 shiftedX = winOffX << depthCode;
        u32 blitExec = 0x0A0000C0U;
        geTexFmt = depthCode + 4U;

        if ((shiftedX & 3U) != 0U) {
            u32 srcCoord = ((2U * (shiftedX + (32U * pageX))) & 0xFFFC1FFFU) | ((winOffY & 0x1FU) << 13);
            bufStride = 32U;
            blitExec = 0x0A0000C4U;
            texAddr = 0x041A0000U;
            gePtr[2] = (((u32)g_pops->gpuTexWinMaskY) << 13) + 0xEDFFFC07U;
            outPtr = gePtr + 7;
            outPtr[-4] = (srcCoord & 0x3FFFFU) | ((pageY + 15040U) << 18);
            outPtr[-3] = (texAddr & 0xFFFFFFU) | 0xB4000000U;
            outPtr[-2] = popsRotr32((texAddr & 0xFF000000U) | (((bufStride & 0x3FFFFU) << 6) & 0xFFFF00U) | 0xB5U, 8U);
            outPtr[-1] = blitExec;
        } else {
            u32 pageIdx = tpage & 0x1FU;
            PopsTexPageSlot *slot = &g_pops->texPages[pageIdx];
            texAddr = slot->vramAddr + (4U * ((shiftedX & 0x3FFU) | (winOffY << 10)));
            if (slot->loadedFormat == 0U) {
                PopsTexPageSlot *groupBase = slot - slot->groupOffset;
                groupBase[0].loadedFormat = (u8)geTexFmt;
                groupBase[1].loadedFormat = (u8)geTexFmt;
                groupBase[2].loadedFormat = (u8)geTexFmt;
                if (pageX >= 12U) {
                    groupBase[3].loadedFormat = (u8)geTexFmt;
                }
                {
                    u32 dstAddr = groupBase->vramAddr;
                    u32 srcCoord = (u32)(s32)groupBase->blitCoord;
                    outPtr = gePtr + 6;
                    outPtr[-4] = (srcCoord & 0x3FFFFU) | ((pageY + 15040U) << 18);
                    outPtr[-3] = (dstAddr & 0xFFFFFFU) | 0xB4000000U;
                    outPtr[-2] = popsRotr32((dstAddr & 0xFF000000U) | (((bufStride & 0x3FFFFU) << 6) & 0xFFFF00U) | 0xB5U, 8U);
                    outPtr[-1] = blitExec;
                }
            }
        }
        bufStride >>= depthCode;
    }

    {
        u32 tbwCmd = ((bufStride & 0xFF00FFFFU) | ((texAddr >> 24) << 16)) + 0xA8000000U;
        u32 clzY = popsClz32((u32)g_pops->gpuTexWinMaskY);
        u32 clzX = popsClz32((u32)g_pops->gpuTexWinMaskX);
        s32 uvBias = (s32)g_pops->gpuUvBias;

        if (g_pops->gpuTexDepth != (u8)depthCode) {
            g_pops->gpuTexDepth = (u8)depthCode;
            *outPtr++ = 0xC3000000U | geTexFmt;
        }
        outPtr[0] = tbwCmd;
        outPtr[1] = 0xA0000000U | (texAddr & 0xFFFFFFU);
        outPtr[2] = (((31U - clzY) << 8) - clzX) + 0xB8000322U;
        outPtr[3] = (clzX << 15) + (u32)(s32)(s16)uvBias + 0x4AB27F8DU;
        outPtr[4] = (clzY << 15) + (u32)(uvBias >> 16) + 0x4BB27F8EU;
        if (emitScaleSuffix != 0) {
            outPtr[5] = 0xCB000000U;
            outPtr[6] = 0xCC000000U;
            outPtr[7] = (clzX << 15) + 0x48320000U;
            outPtr[8] = (clzY << 15) + 0x49320000U;
            return outPtr + 9;
        }
        return outPtr + 5;
    }
}

/* [142] 0x010244 */
__attribute__((noinline))
s32 popsGpuHandleMenuAndVmc(void)
{
    volatile PopsSharedMem *shared = POPS_SHARED_MEM;

    if (g_powerSuspending == 1U) {
        popsSavedataWaitIdle();
        popsHandleDiscError(0);
    }

    if (shared->pauseMenuOpen != 0U) {
        if (g_gpuPauseFlushDone != 0U) {
            popsSavedataWaitIdle();
        } else {
            g_gpuPauseFlushDone = 1U;
            popsMcFlushBothSlots();
        }
        return 1;
    }

    {
        s32 slotIdx;
        for (slotIdx = 0; slotIdx < 2; slotIdx++) {
            u8 changeReq = shared->mcChangeRequest[slotIdx];
            if (changeReq != 0U) {
                shared->mcChangeRequest[slotIdx] = 0U;
                popsMcSetSlotInserted((u32)slotIdx, -1);
                if (changeReq == 1U || g_gpuMcChangeTimer[slotIdx] == 0) {
                    g_gpuMcChangeTimer[slotIdx] = (changeReq == 1U) ? 60 : 1;
                }
            } else if (g_gpuMcChangeTimer[slotIdx] > 0) {
                g_gpuMcChangeTimer[slotIdx]--;
                if (g_gpuMcChangeTimer[slotIdx] == 0) {
                    if (slotIdx == 0) {
                        popsMcInitPocketStation();
                    }
                    popsMcSetSlotInserted((u32)slotIdx, 1);
                }
            }
        }
    }

    g_gpuPauseFlushDone = 0U;
    if (g_pops->haltFlags >= 0) {
        popsSetPauseState(0);
    }
    return 0;
}

/* [143] 0x0103b8 */
__attribute__((noinline))
void popsGpuVblankStartCallback(PopsEvent *ev)
{
    u32 dispFlags;
    float lineCycles;
    s32 scanlines;
    s32 vblankCycles;
    u32 curDispPos;

    (void)ev;
    g_pops->gpuFieldParity++;
    popsTimersNotifyVblank(1);

    dispFlags = g_pops->gpuDispFlags.word;
    lineCycles = ((dispFlags & 0x80000U) != 0U) ? 1083.8016f : 1076.2751f;
    {
        s32 palBit = ((dispFlags & 0x200000U) != 0U) ? 1 : 0;
        s32 regionBit = ((dispFlags & 0x80000U) != 0U) ? 1 : 0;
        s32 extraLines = (25703 >> (palBit * 8)) & (129 - (regionBit * 2));
        scanlines = extraLines - (2 * ((s32)g_pops->gpuVertEnd - (s32)g_pops->gpuVertStart)) + 525;
    }
    vblankCycles = popsRoundToInt((float)scanlines * lineCycles);

    g_pops->gpuVblankEvent.callback = popsGpuVblankEndCallback;
    popsEventSchedule(&g_pops->gpuVblankEvent, vblankCycles);
    popsIrqRaise(1U);

    curDispPos = (u32)g_pops->gpuDispX + ((u32)g_pops->gpuDispY << 10);
    if (curDispPos == g_pops->gpuLastDisplayPos || (g_pops->quirkFlags & 0x20000000U) != 0U) {
        if ((g_pops->gpuDispFlags.word & 0x80000U) == 0U || (g_pops->quirkFlags & 8U) == 0U) {
            g_pops->gpuDispFlags.bytes.flippedThisVblank = 1U;
            popsGpuFlipDisplay();
        }
    }
}

/* [144] 0x0104cc */
__attribute__((noinline))
void popsGpuThrottleSpeed(void)
{
    if (POPS_SHARED_MEM->fastForward == 0U && g_gpuInitPhase != 0U) {
        if ((g_pops->quirkFlagsExt & 0x20) != 0) {
            popsSpuCheckIrq();
        }
        {
            s32 spuDelta = (s32)(POPS_ME_SPU_SHARED->sampleCounter - g_pops->gpuSyncSpuTick);
            s32 cpuDelta = (s32)(g_pops->cycleTarget - (u32)g_pops->cycleRemaining) - g_pops->gpuSyncCpuCycle;
            s32 audioCycles = (g_pops->gpuPalMode != 0U) ? (768 * spuDelta) : (766 * spuDelta);
            s32 ahead = cpuDelta - audioCycles;
            if (ahead >= 3387) {
                if ((g_pops->gpuDispFlags.word & 0x100000U) == 0U || ahead >= (audioCycles >> 1)) {
                    u32 usec = ((u32)(1741 * ahead) >> 16) >> ((cpuDelta > 544272) ? 1 : 0);
                    sceKernelDelayThread(usec);
                }
            }
        }
    }
}

/* [145] 0x0105b8 */
__attribute__((noinline))
void popsGpuFlipDisplay(void)
{
    volatile PopsSharedMem *shared = POPS_SHARED_MEM;
    s32 pausedOnce = 0;

    if ((g_pops->gpuStatusReg & 0x04000000U) == 0U && g_pops->gpuPacketState == 8U) {
        g_pops->gpuDeferredFlip = 1U;
        return;
    }

    {
        u32 dispFlags = g_pops->gpuDispFlags.word;
        u8 prevPal = g_pops->gpuPalMode;
        u8 newPal = ((dispFlags & 0x200000U) != 0U) ? 1U : 0U;
        s32 prevFrameDirty = g_pops->gpuFrameDirty;
        s32 targetVcount = g_pops->gpuLastVcount + 1;
        s32 needDraw;
        s32 dispDisabled;
        s32 syncCpuCycle;

        g_pops->gpuDeferredFlip = 0U;
        g_pops->gpuInterlaceActive = ((dispFlags & 0x40000U) != 0U && (dispFlags & 0x200000U) != 0U) ? 1U : 0U;

        if (prevPal != newPal) {
            popsGpuSetPalMode((s32)newPal);
            g_pops->gpuFrameThrottle &= 1U;
        }

        if ((g_pops->quirkFlags & 8U) == 0U && (g_pops->gpuDispFlags.word & 0x80000U) != 0U) {
            s32 accum = g_pops->gpuPalFrameAccum;
            s32 stepPal = 112331;
            s32 stepNtsc = 114506;
            if (accum >= 0) {
                targetVcount = g_pops->gpuLastVcount + 2;
                stepPal = -452713;
                stepNtsc = -451614;
            }
            g_pops->gpuPalFrameAccum = accum + ((g_pops->gpuPalMode != 0U) ? stepPal : stepNtsc);
        }

        needDraw = prevFrameDirty | (s32)g_pops->gpuModeDirty;
        if ((g_pops->quirkFlags & 0x8000U) != 0U) {
            if (needDraw != 0) {
                g_pops->gpuDispFlags.bytes.idleFrames = 0U;
            } else if (g_pops->gpuDispFlags.bytes.idleFrames == 60U) {
                popsCdMarkDiscChangeReady();
            } else {
                g_pops->gpuDispFlags.bytes.idleFrames++;
            }
        }

        {
            u32 padCfg = ((u32)shared->padAssignMode * 2U) + (shared->analogMode != 0U ? 1U : 0U);
            if (g_gpuPadModeShadow != padCfg || (shared->padAssignMode & 0x80U) != 0U) {
                g_gpuPadModeShadow = padCfg;
                popsPadApplyConfig((u32)shared->padAssignMode, padCfg & 1U, 0U);
            }
        }

        shared->cdActiveIndicator = (u8)popsCdGetDiscChangeReady();
        if (shared->resetRequest == 1U || shared->resetRequest == 2U) {
            if (shared->resetRequest == 2U) {
                g_pops->haltFlags = -1;
            }
            shared->resetRequest = 0U;
            popsDiscSwapAndReload();
        }
        popsCdSetLoadSpeed((u32)shared->discLoadSpeed);
        shared->mcBusyIndicator = (popsMcIsWritebackIdle() == 0) ? 1U : 0U;

        dispDisabled = ((g_pops->gpuStatusReg & 0x00800000U) != 0U) ? 1 : 0;

        for (;;) {
            s32 menuPaused = popsGpuHandleMenuAndVmc();
            s32 shouldPresent = 0;

            if (g_pops->gpuFirstFlipDone == 0) {
                g_pops->gpuFirstFlipDone = -1;
                shouldPresent = 1;
            } else if (menuPaused != 0 || needDraw != 0) {
                shouldPresent = 1;
            }

            if (shouldPresent == 0) {
                g_pops->gpuLastVcount = targetVcount;
                if (shared->fastForward == 0U && g_gpuInitPhase != 0U) {
                    s32 curVcount;
                    for (;;) {
                        curVcount = sceDisplayGetVcount();
                        if ((curVcount - targetVcount) >= 0) {
                            break;
                        }
                        popsSavedataPollStatus();
                        sceDisplayWaitVblankStartCB();
                    }
                    if (curVcount != targetVcount) {
                        syncCpuCycle = (s32)(g_pops->cycleTarget - (u32)g_pops->cycleRemaining) + 564480;
                        break;
                    }
                }
                g_pops->gpuSyncSpuTick = POPS_ME_SPU_SHARED->sampleCounter;
                syncCpuCycle = (s32)(g_pops->cycleTarget - (u32)g_pops->cycleRemaining);
                break;
            }

            {
                u32 *gePtr = (u32 *)g_pops->gpuDlistCursor;
                if (g_pops->gpuListState < 0) {
                    sceGeListSync(g_pops->gpuGeListId, 0);
                    g_pops->gpuGeListId = sceGeListEnQueue((const void *)POPS_GE_DLIST_BASE, (void *)POPS_GE_DLIST_BASE, -1, (void *)0);
                }

                if (dispDisabled != 0) {
                    gePtr[0] = 0x13041B92U;
                    gePtr[1] = 0x0A000000U;
                    gePtr += 2;
                    popsGpuAdvanceFrameSlot(0, 0);
                } else {
                    u32 curDispFlags = g_pops->gpuDispFlags.word;
                    const u8 *modeRow = ((curDispFlags & 0x400000U) != 0U)
                        ? g_gpuDispModeTable[4]
                        : g_gpuDispModeTable[(curDispFlags >> 16) & 3U];
                    u8 interlaceShift = g_pops->gpuInterlaceActive;
                    u16 baseHeight = ((curDispFlags & 0x80000U) != 0U) ? 288U : 240U;

                    g_pops->gpuInterlaceHalf = (((g_pops->gpuTexPageState & 0x400U) == 0U) ? 1U : 0U) & interlaceShift;
                    g_pops->gpuDispWidth = (u16)(modeRow[7] * 16U);
                    g_pops->gpuDispHeight = (u16)(baseHeight << interlaceShift);

                    if (g_pops->haltFlags < 0) {
                        g_gpuInitPhase = 0U;
                    } else {
                        s32 topBase = 16;
                        s32 vertEnd = (s32)g_pops->gpuVertEnd;
                        s32 topBaseShifted;
                        s32 topExtra;
                        s32 vertStart;
                        s32 dispY = (s32)g_pops->gpuDispY;
                        s32 maxVert;
                        s32 cropTop;
                        s32 cropBottom;
                        s32 copyHeight;
                        s32 slotIdx;

                        g_pops->gpuListState = 0;
                        if ((curDispFlags & 0x80000U) != 0U) {
                            topBase = ((g_pops->quirkFlagsExt & 0x20000000) != 0) ? 45 : 35;
                        }
                        if (g_pops->displayHeightClamp > 0) {
                            vertEnd = popsMinS32(vertEnd, g_pops->displayHeightClamp + topBase);
                        }
                        topBaseShifted = topBase << interlaceShift;
                        topExtra = ((g_pops->gpuDispFlags.word & 0x100000U) != 0U)
                            ? g_pops->displayTopAdjustWide
                            : g_pops->displayTopAdjustNormal;
                        vertEnd -= ((g_pops->quirkFlags & 2U) != 0U) ? 1 : 0;
                        vertStart = (s32)g_pops->gpuVertStart << interlaceShift;
                        if (topExtra > 0) {
                            s32 topExtraShifted = topExtra << interlaceShift;
                            if (vertStart < topExtraShifted + topBaseShifted) {
                                dispY = (dispY + topExtraShifted + topBaseShifted - vertStart) & 0x1FF;
                                vertStart = topExtraShifted + topBaseShifted;
                            }
                        }
                        maxVert = ((curDispFlags & 0x80000U) != 0U)
                            ? popsMinS32(popsMaxS32(vertStart, topBaseShifted) + 256, 323)
                            : 256;
                        cropTop = vertStart - topBaseShifted;
                        cropBottom = (popsMinS32(vertEnd, maxVert) << interlaceShift) - topBaseShifted;
                        if (cropTop < 0) {
                            dispY = (dispY - cropTop) & 0x1FF;
                            cropTop = 0;
                        }
                        copyHeight = popsMinS32(cropBottom - cropTop, 511);
                        slotIdx = popsGpuAdvanceFrameSlot((s16)copyHeight, (s16)cropTop);

                        {
                            s32 horizBase = (s32)modeRow[6];
                            s32 horizStart = (s32)g_pops->gpuHorizStart;
                            s32 srcX = horizStart - horizBase;
                            s32 horizSpan = popsMinS32((s32)g_pops->gpuHorizEnd, horizBase - g_pops->displayWidthBias + 3072) - horizStart;
                            s32 copyWidth;

                            if (srcX < 512) {
                                srcX -= (s32)(modeRow[0] >> 4);
                            }
                            shared->slots[slotIdx].srcW = (s16)horizSpan;
                            shared->slots[slotIdx].srcX = (s16)srcX;
                            copyWidth = horizSpan / (s32)(modeRow[0] >> 3);

                            if (copyWidth != 0) {
                                u32 fbBase = POPS_FRAMEBUF_BASE + ((u32)g_pops->gpuRingBufIndex * 1310720U);
                                u32 dispX = g_pops->gpuDispX & 0x3FEU;

                                if ((g_pops->gpuDispFlags.word & 0x100000U) != 0U) {
                                    /* 24-bit RGB888 PS1 display -> 32-bit PSP framebuffer */
                                    s32 clampedWidth = popsMinS32(copyWidth, 640);
                                    u32 quadWords = (u32)((clampedWidth + 3) & ~3);
                                    s32 row;

                                    if ((g_pops->quirkFlagsExt & 0x2000) != 0) {
                                        g_pops->gpuDispWidth = (u16)(g_pops->gpuDispWidth + (g_pops->gpuDispWidth >> 1));
                                    }
                                    gePtr++;
                                    g_pops->gpuGeListId = sceMeAudio_D96DC042(g_pops->gpuGeListId, (void *)gePtr);
                                    if (shared->fastForward == 0U && (sceDisplayGetVcount() - g_pops->gpuLastVcount) <= 0) {
                                        sceDisplayWaitVblankStartCB();
                                    }
                                    for (row = 0; row < copyHeight; row++) {
                                        u32 rowBase = POPS_VRAM_CACHED_BASE | ((((u32)(row + dispY)) & 0x1FFU) << 11);
                                        u32 byteOff = (dispX & 0x3FFU) * 2U;
                                        u32 *dstLine = (u32 *)fbBase;
                                        u32 *dstEnd = dstLine + quadWords;

                                        while (dstLine != dstEnd) {
                                            const u8 *srcBytes = (const u8 *)(rowBase | (byteOff & 0x7FFU));
                                            u32 pixA = (u32)srcBytes[0] | ((u32)srcBytes[1] << 8) | ((u32)srcBytes[2] << 16) | ((u32)srcBytes[3] << 24);
                                            u32 pixB = (u32)srcBytes[3] | ((u32)srcBytes[4] << 8) | ((u32)srcBytes[5] << 16) | ((u32)srcBytes[6] << 24);
                                            u32 pixC = (u32)srcBytes[6] | ((u32)srcBytes[7] << 8) | ((u32)srcBytes[8] << 16) | ((u32)srcBytes[9] << 24);
                                            u32 pixD = (u32)srcBytes[9] | ((u32)srcBytes[10] << 8) | ((u32)srcBytes[11] << 16);
                                            dstLine[0] = pixA;
                                            dstLine[1] = pixB;
                                            dstLine[2] = pixC;
                                            dstLine[3] = pixD;
                                            dstLine += 4;
                                            byteOff += 12U;
                                        }
                                        *dstLine = 0U;
                                        fbBase += 2560U;
                                    }
                                    sceKernelDcacheInvalidateRangeForUser((const void *)POPS_VRAM_CACHED_BASE, 0x100000U);
                                } else {
                                    /* 16-bit PS1 VRAM -> GE block transfer to PSP framebuffer */
                                    s32 linesToBottom = 512 - dispY;
                                    gePtr[0] = 0xCC000000U;
                                    gePtr[1] = 0xB2000000U;
                                    gePtr[2] = 0xB3040400U;
                                    gePtr[3] = 0xB4000000U | (fbBase & 0xFFFFFFU);
                                    gePtr[4] = 0xB5000500U | (((fbBase >> 24) & 0xFU) << 16);
                                    gePtr[5] = 0xEC000000U;
                                    gePtr[6] = 0xEB000000U | (((u32)dispY & 0x1FFU) << 10) | (dispX & 0x3FFU);
                                    if (linesToBottom < copyHeight) {
                                        gePtr[7] = (u32)(copyWidth + (linesToBottom << 10) - 0x12000401);
                                        gePtr[8] = 0xEA000000U;
                                        gePtr[9] = 0xEB000000U | (dispX & 0x3FFU);
                                        gePtr[10] = 0xEC000000U | (((u32)linesToBottom & 0x1FFU) << 10);
                                        copyHeight = copyHeight + dispY - 512;
                                        gePtr += 4;
                                    }
                                    gePtr[7] = (u32)(copyWidth + (copyHeight << 10) - 0x12000401);
                                    gePtr[8] = 0xEA000000U;
                                    gePtr += 9;
                                }
                            }
                        }
                    }

                    if (g_pops->gpuModeDirty != 0U) {
                        u8 drawInTex = ((s32)g_pops->gpuDispWidth < (g_pops->gpuClipRight - g_pops->gpuClipLeft)
                            || (s32)g_pops->gpuDispHeight < (g_pops->gpuClipBottom - g_pops->gpuClipTop)) ? 1U : 0U;
                        if ((g_pops->quirkFlags & 1U) != 0U) {
                            drawInTex = 0U;
                        }
                        g_pops->gpuDrawInsideTex = drawInTex;
                        g_pops->gpuModeDirty = 0U;
                    }
                    sceMeAudio_899A3BC0((void *)gePtr);
                }

                /* Compact swizzled texture page cache between EDRAM and main RAM */
                {
                    s32 swapped = 0;
                    s32 half;
                    for (half = 0; half < 2 && swapped == 0; half++) {
                        s32 grp;
                        for (grp = 0; grp < 5 && swapped == 0; grp++) {
                            u32 idxA = (u32)(half * 16 + grp * 3);
                            PopsTexPageSlot *slotA = &g_pops->texPages[idxA];
                            u32 addrA = slotA->vramAddr;
                            if (slotA->loadedFormat == 0U && (addrA & 0x08000000U) == 0U) {
                                if (slotA[3].loadedFormat == 0U || slotA[3].groupOffset == 0) {
                                    s32 hScan;
                                    for (hScan = 0; hScan < 2 && swapped == 0; hScan++) {
                                        s32 gScan;
                                        for (gScan = 0; gScan < 5 && swapped == 0; gScan++) {
                                            u32 idxB = (u32)(hScan * 16 + gScan * 3);
                                            PopsTexPageSlot *slotB = &g_pops->texPages[idxB];
                                            u32 addrB = slotB->vramAddr;
                                            if (((u32)(u8)slotB->loadedFormat & (addrB >> 27)) != 0U) {
                                                slotA[0].vramAddr = addrB;
                                                slotA[1].vramAddr = slotB[1].vramAddr;
                                                slotA[2].vramAddr = slotB[2].vramAddr;
                                                slotB[0].vramAddr = addrA;
                                                slotB[1].vramAddr = addrA + 128U;
                                                slotB[2].vramAddr = addrA + 256U;
                                                slotB[0].loadedFormat = 0U;
                                                slotB[1].loadedFormat = 0U;
                                                slotB[2].loadedFormat = 0U;
                                                g_pops->texPages[15].loadedFormat = g_pops->texPages[14].loadedFormat;
                                                g_pops->texPages[15].vramAddr = g_pops->texPages[14].vramAddr + 128U;
                                                g_pops->texPages[31].loadedFormat = g_pops->texPages[30].loadedFormat;
                                                g_pops->texPages[31].vramAddr = g_pops->texPages[30].vramAddr + 128U;
                                                swapped = 1;
                                            }
                                        }
                                    }
                                    swapped = 1;
                                }
                            }
                        }
                    }
                }

                g_pops->gpuTexPageState |= 0xC000U;
                g_pops->gpuDlistCursor = POPS_GE_DLIST_BASE;
                g_pops->gpuTexDepth = 0xFFU;
                g_pops->gpuListState = -1;

                if (menuPaused != 0 || (g_pops->gpuDispFlags.word & 0x100000U) != 0U) {
                    gePtr[0] = 0x3A000009U;
                    gePtr[1] = popsGeEncodeFloatCoord((s32)g_pops->gpuDrawOffX);
                    gePtr[2] = popsGeEncodeFloatCoord((s32)g_pops->gpuDrawOffY);
                    gePtr[3] = 0x3A000000U;
                    gePtr[4] = 0x3B478000U;
                    gePtr[5] = 0x3A000004U;
                    gePtr[6] = 0x3B478000U;
                    gePtr += 7;
                }

                gePtr[0] = 0xD4000000U | (((u32)(u16)g_pops->gpuClipTop) << 10) | (u16)g_pops->gpuClipLeft;
                gePtr[1] = 0xD5000000U | (((u32)(u16)g_pops->gpuClipBottom) << 10) | (u16)g_pops->gpuClipRight;
                gePtr[2] = 0x13041B93U;
                gePtr[3] = 0x0A000000U;

                {
                    s32 curVcount;
                    if (shared->fastForward == 0U && g_gpuInitPhase != 0U) {
                        for (;;) {
                            curVcount = sceDisplayGetVcount();
                            if ((curVcount - targetVcount) >= 0) {
                                break;
                            }
                            popsSavedataPollStatus();
                            sceDisplayWaitVblankStartCB();
                        }
                    } else {
                        curVcount = sceDisplayGetVcount();
                    }

                    sceMeAudio_899A3BC0((void *)0);
                    g_pops->gpuLastVcount = curVcount;
                    sceGeDrawSync(0);
                    shared->frontIndex = g_pops->gpuFrontBufIndex;
                }

                if (menuPaused == 0) {
                    g_pops->gpuSyncSpuTick = POPS_ME_SPU_SHARED->sampleCounter;
                    syncCpuCycle = (s32)(g_pops->cycleTarget - (u32)g_pops->cycleRemaining);
                    break;
                }

                if (pausedOnce == 0) {
                    popsSetPauseState(1);
                    sceDisplayWaitVblankStartCB();
                }
                pausedOnce = 1;

                {
                    u32 padCfg = ((u32)shared->padAssignMode * 2U) + (shared->analogMode != 0U ? 1U : 0U);
                    if (g_gpuPadModeShadow != padCfg || (shared->padAssignMode & 0x80U) != 0U) {
                        g_gpuPadModeShadow = padCfg;
                        popsPadApplyConfig((u32)shared->padAssignMode, padCfg & 1U, 0U);
                    }
                }
                shared->cdActiveIndicator = (u8)popsCdGetDiscChangeReady();
                if (shared->resetRequest == 1U || shared->resetRequest == 2U) {
                    if (shared->resetRequest == 2U) {
                        g_pops->haltFlags = -1;
                    }
                    shared->resetRequest = 0U;
                    popsDiscSwapAndReload();
                }
                popsCdSetLoadSpeed((u32)shared->discLoadSpeed);
                shared->mcBusyIndicator = (popsMcIsWritebackIdle() == 0) ? 1U : 0U;
                popsSavedataPollStatus();
                sceDisplayWaitVblankStartCB();
                g_pops->gpuModeDirty = 2U;
            }
        }

        g_pops->gpuSyncCpuCycle = syncCpuCycle;

        if (g_pops->gpuSkipMode != 0 && g_pops->gpuFrameThrottle != 0U && g_pops->gpuPalMode != 0U && g_pops->gpuFrameDirty > 0) {
            g_pops->gpuFrameDirty = -1;
        } else {
            g_pops->gpuFrameDirty = 0;
        }

        if (g_pops->gpuDispFlags.bytes.muteCountdown >= 0) {
            g_pops->gpuDispFlags.bytes.muteCountdown--;
            if (g_pops->gpuDispFlags.bytes.muteCountdown < 0) {
                sceMeAudio_EB52DFE0();
            }
        }

        if (pausedOnce != 0) {
            popsGpuSetPalMode(((g_pops->gpuDispFlags.word & 0x200000U) != 0U) ? 1 : 0);
            if (g_pops->gpuPalMode == 0U) {
                g_pops->gpuFrameThrottle &= 1U;
            }
            sceDisplayWaitVblankStartCB();
            popsUpdateDisplayMute();
            g_pops->gpuLastVcount = sceDisplayGetVcount();
        }

        if ((g_pops->quirkFlagsExt & 0x20) != 0) {
            popsSpuCheckIrq();
        }

        if (g_pops->gpuVblankSlices > 0) {
            s32 sliceTotal = (s32)(u16)g_pops->gpuVblankSlices;
            s32 sliceFirst = (s32)((u32)g_pops->gpuVblankSlices >> 16) & 0x7FFF;
            g_pops->gpuSliceRemaining = sliceTotal - sliceFirst;
            if (g_pops->gpuSliceEvent.next != (PopsEvent *)0) {
                popsEventCancel(&g_pops->gpuSliceEvent);
            }
            if (syncCpuCycle == (s32)(g_pops->cycleTarget - (u32)g_pops->cycleRemaining)
                || (g_pops->quirkFlags & 8U) != 0U) {
                popsEventSchedule(&g_pops->gpuSliceEvent, (564480 / sliceTotal) * sliceFirst);
            }
        }
    }
}

/* [146] 0x011208 */
__attribute__((noinline))
void popsGpuBusyDoneCallback(PopsEvent *ev)
{
    u8 deferred = g_pops->gpuDeferredFlip;
    (void)ev;
    g_pops->gpuStatusReg |= 0x14000000U;
    if (deferred != 0U) {
        popsGpuFlipDisplay();
    } else if (g_pops->gpuListState > 0) {
        u32 nextStall = g_pops->gpuDlistCursor + 4U;
        g_pops->gpuListState = 0;
        g_pops->gpuDlistCursor = nextStall;
        g_pops->gpuGeListId = sceMeAudio_D96DC042(g_pops->gpuGeListId, (void *)nextStall);
    }
}

/* [147] 0x011274 */
__attribute__((noinline))
void popsGpuSetDisplayEnable(u8 enable)
{
    POPS_SHARED_MEM->displayEnable = enable;
}

/* [148] 0x011280 */
__attribute__((noinline))
s32 popsGpuMarkFrameDirty(void)
{
    g_pops->gpuFrameDirty = 1;
    g_pops->gpuModeDirty = 1U;
    return 0;
}

/* [149] 0x011294 */
__attribute__((noinline))
void popsGpuSetAnalogMode(u32 mode)
{
    POPS_SHARED_MEM->analogMode = (u8)mode;
    *(volatile u32 *)&g_gpuPadModeShadow = (g_gpuPadModeShadow & 0xFFFFFFFEU) | mode;
}

/* [150] 0x0112b4 */
__attribute__((noinline))
u32 popsGpuGetScreenMode(void)
{
    return (u32)POPS_SHARED_MEM->screenMode;
}

/* [151] 0x0112c0 */
__attribute__((noinline))
u32 popsGpuSetCustomParam(u32 idx, u32 val)
{
    POPS_SHARED_MEM->customScreenParams[idx] = val;
    return idx * 4U;
}

/* [152] 0x0112d4 */
__attribute__((noinline))
s32 popsGpuAdvanceFrameSlot(s16 height, s16 topOff)
{
    u32 nextRing = (u32)g_pops->gpuRingBufIndex + 1U;
    u8 nextFront = (u8)(g_pops->gpuFrontBufIndex + 1U);
    u8 blankMask = 0x80U;
    u8 blankFlag = ((g_pops->gpuStatusReg & 0x00800000U) != 0U) ? 1U : 0U;
    u32 slotIdx;

    if (nextRing >= 3U) {
        nextRing = 0U;
    }
    g_pops->gpuFrontBufIndex = nextFront;
    g_pops->gpuRingBufIndex = (u8)nextRing;
    POPS_SHARED_MEM->readyFlag = (g_gpuInitPhase == 2U) ? 1U : 0U;

    if (g_pops->gpuHorizStart >= g_pops->gpuHorizEnd) {
        blankFlag = 1U;
    }
    if (g_pops->gpuVertStart < g_pops->gpuVertEnd) {
        blankMask = (u8)(blankFlag << 7);
    }

    slotIdx = (u32)(nextFront & 1U);
    POPS_SHARED_MEM->slots[slotIdx].bufIndex = (u8)nextRing;
    POPS_SHARED_MEM->slots[slotIdx].modeByte = (u8)(g_pops->gpuDispFlags.bytes.dispModeByte + blankMask);
    POPS_SHARED_MEM->slots[slotIdx].srcY = topOff;
    POPS_SHARED_MEM->slots[slotIdx].srcH = height;
    return (s32)slotIdx;
}

/* [153] 0x011378 */
__attribute__((noinline))
void popsGpuVblankEndCallback(PopsEvent *ev)
{
    s32 irqDelay = g_pops->vblankIrqDelay;
    float lineCycles;
    s32 activeCycles;

    (void)ev;
    popsTimersNotifyVblank(0);
    lineCycles = ((g_pops->gpuDispFlags.word & 0x80000U) != 0U) ? 2167.6033f : 2152.5503f;
    activeCycles = popsRoundToInt((float)((s32)g_pops->gpuVertEnd - (s32)g_pops->gpuVertStart) * lineCycles);

    g_pops->gpuFrameStartCycle = g_pops->gpuVblankEvent.dueCycle;
    g_pops->gpuFieldParity++;
    if (irqDelay <= 0) {
        popsGpuScheduleNextVblank(activeCycles);
        return;
    }
    g_pops->gpuVblankEvent.callback = popsGpuVblankPostCallback;
    g_pops->gpuVblankRemainder = activeCycles - irqDelay;
    popsEventSchedule(&g_pops->gpuVblankEvent, irqDelay);
}

/* [154] 0x01142c */
__attribute__((noinline))
void popsGpuSliceCallback(PopsEvent *ev)
{
    s32 rem = g_pops->gpuSliceRemaining - 1;
    if (rem > 0) {
        s32 step = 564480 / (s32)(u16)g_pops->gpuVblankSlices;
        g_pops->gpuSliceRemaining = rem;
        popsEventSchedule(ev, step);
        popsGpuThrottleSpeed();
        return;
    }

    if ((g_pops->quirkFlags & 8U) == 0U) {
        popsGpuThrottleSpeed();
        return;
    }

    if (rem >= 0 || (g_pops->gpuDispFlags.word & 0x80000U) == 0U) {
        s32 sliceTotal = (s32)(u16)g_pops->gpuVblankSlices;
        s32 frameLines = (((g_pops->gpuDispFlags.word & 0x200000U) != 0U) ? 1 : 0) + 525;
        s32 totalCycles = popsRoundToInt(1076.2751f * (float)frameLines);
        s32 step = totalCycles - ((564480 / sliceTotal) * (sliceTotal + rem - 1));
        g_pops->gpuSliceRemaining = rem;
        popsEventSchedule(ev, step);
        popsGpuThrottleSpeed();
        return;
    }

    popsGpuFlipDisplay();
}

/* [155] 0x0114f8 */
__attribute__((noinline))
void popsGpuWriteReg(u32 addr, u32 value, u32 width)
{
    (void)width;
    if ((addr & 4U) == 0U) {
        /* GP0 register write */
        s32 rem = (s32)g_pops->gpuFifoRemaining;
        g_pops->gpuStatusReg &= 0xEBFFFFFFU;
        if (rem == 0) {
            u32 state = g_pops->gpuPacketState;
            popsCacheCreateDirtyGpu(g_pops->gpuCmdFifo);
            if (state == 0U) {
                rem = (s32)g_gpuCmdWordLen[value >> 26];
                if ((value >> 24) == 2U) {
                    rem = 2;
                }
            } else if (state == 9U) {
                u32 pixels = (u32)g_pops->gpuXferSize.wh.w * (u32)g_pops->gpuXferSize.wh.h;
                if (pixels >= 97U) {
                    pixels = 96U;
                }
                rem = (s32)(((pixels + 1U) >> 1) - 1U);
            } else if (state < 9U) {
                rem = ((g_pops->gpuXferSize.word & 0x10000000U) != 0U
                    && (value & 0xF000F000U) != 0x50005000U) ? 1 : 0;
            } else {
                rem = 0;
            }
            g_pops->gpuFifoRemaining = (u8)rem;
        }

        {
            s32 writePos = (s32)g_pops->gpuFifoWritePos + 1;
            g_pops->gpuCmdFifo[writePos - 1] = value;
            if (rem >= writePos) {
                g_pops->gpuFifoWritePos = (u8)writePos;
                return;
            }

            {
                u8 prevState = g_pops->gpuPacketState;
                s32 cost = popsGpuExecutePackets((u32)g_pops->gpuCmdFifo, (u32)(writePos * 4));
                if (g_pops->gpuBusyEvent.next != (PopsEvent *)0) {
                    g_pops->cycleRemaining = (s32)(g_pops->cycleTarget - g_pops->gpuBusyEvent.dueCycle);
                    popsEventCancel(&g_pops->gpuBusyEvent);
                }
                if (cost != 0) {
                    popsEventSchedule(&g_pops->gpuBusyEvent, cost);
                } else {
                    g_pops->gpuStatusReg |= 0x14000000U;
                }
                if (prevState == 9U && g_pops->gpuListState > 0) {
                    u32 nextStall = g_pops->gpuDlistCursor + 4U;
                    g_pops->gpuListState = 0;
                    g_pops->gpuDlistCursor = nextStall;
                    g_pops->gpuGeListId = sceMeAudio_D96DC042(g_pops->gpuGeListId, (void *)nextStall);
                }
                g_pops->gpuFifoRemaining = 0U;
                g_pops->gpuFifoWritePos = 0U;
            }
        }
        return;
    }

    /* GP1 control command */
    switch (value >> 24) {
        case 0U: {
            s32 listId = g_pops->gpuGeListId;
            g_pops->gpuListState = 0;
            if (listId != 0) {
                u32 *gePtr = (u32 *)g_pops->gpuDlistCursor;
                gePtr[0] = 0x0F000000U;
                gePtr[1] = 0x0C000000U;
                sceGeListUpdateStallAddr(listId, (void *)0);
            }
            sceGeListEnQueue(g_gpuGeInitList, (void *)0, -1, (void *)0);
            sceGeListEnQueue((const void *)0x041B9300U, (void *)0, -1, (void *)0);
            g_pops->gpuStatusReg = 0x1C800000U;
            g_pops->gpuTexPageState = 0xC000U;
            g_pops->gpuHorizStart = 512U;
            g_pops->gpuHorizEnd = 3072U;
            g_pops->gpuVertStart = 16U;
            g_pops->gpuVertEnd = 256U;
            g_pops->gpuTexWinMaskX = 32U;
            g_pops->gpuTexWinMaskY = 32U;
            g_pops->gpuDlistCursor = POPS_GE_DLIST_BASE;
            g_pops->gpuModeDirty = 2U;
            g_pops->gpuPalMode = 0xFFU;
            g_pops->gpuDispFlags.bytes.dispModeByte = 0U;
            g_pops->gpuTexDepth = 0xFFU;
            g_pops->gpuClipLeft = 0;
            g_pops->gpuClipTop = 0;
            g_pops->gpuClipRight = 0;
            g_pops->gpuClipBottom = 0;
            g_pops->gpuDrawOffX = 0;
            g_pops->gpuDrawOffY = 0;
            g_pops->gpuDispX = 0U;
            g_pops->gpuDispY = 0U;
            g_pops->gpuTexWinOffX = 0U;
            g_pops->gpuTexWinOffY = 0U;
            g_pops->gpuInterlaceActive = 0xFFU;
            sceGeDrawSync(0);
            g_pops->gpuGeListId = sceGeListEnQueue((const void *)POPS_GE_DLIST_BASE, (void *)POPS_GE_DLIST_BASE, -1, (void *)0);
        }
        /* fall through */
        case 1U:
            g_pops->gpuPacketState = 0U;
            g_pops->gpuFifoRemaining = 0U;
            g_pops->gpuFifoWritePos = 0U;
            return;

        case 3U: {
            u32 oldBit = ((g_pops->gpuStatusReg & 0x00800000U) != 0U) ? 1U : 0U;
            if (oldBit != (value & 1U)) {
                g_pops->gpuStatusReg = (g_pops->gpuStatusReg & 0xFF7FFFFFU) | ((value & 1U) << 23);
                g_pops->gpuFrameDirty = 2;
                if (g_gpuInitPhase == 1U) {
                    g_gpuInitPhase = 2U;
                }
            }
            return;
        }

        case 4U:
            g_pops->gpuStatusReg = (g_pops->gpuStatusReg & 0x9DFFFFFFU)
                | ((value & 3U) << 29)
                | (((value & 3U) != 0U) ? 0x02000000U : 0U);
            return;

        case 5U: {
            s32 newX = (s32)(value & 0x3FFU);
            s32 newY = (s32)((value >> 10) & 0x1FFU);
            if ((s32)g_pops->gpuDispX != newX || (s32)g_pops->gpuDispY != newY) {
                s32 dispH = (s32)g_pops->gpuDispHeight;
                u8 overlaps = (g_pops->gpuClipRight >= newX
                    && g_pops->gpuClipLeft < (newX + (s32)g_pops->gpuDispWidth)
                    && ((g_pops->gpuClipBottom >= newY && g_pops->gpuClipTop < (newY + dispH))
                        || (g_pops->gpuClipBottom >= (newY - 512) && g_pops->gpuClipTop < (newY + dispH - 512)))) ? 1U : 0U;
                g_pops->gpuFrameDirty = 2;
                g_pops->gpuDrawOverlapsDisp = overlaps;
                g_pops->gpuDispX = (u16)newX;
                g_pops->gpuDispY = (u16)newY;
            }
            return;
        }

        case 6U: {
            u16 hStart = (u16)(value & 0xFFFU);
            u16 hEnd = (u16)((value >> 12) & 0xFFFU);
            u32 diff = (u32)(g_pops->gpuHorizEnd - hEnd) | (u32)(g_pops->gpuHorizStart - hStart);
            g_pops->gpuHorizStart = hStart;
            g_pops->gpuHorizEnd = hEnd;
            if (diff != 0U) {
                g_pops->gpuModeDirty = 2U;
            }
            return;
        }

        case 7U: {
            u16 vStart = (u16)(value & 0x3FFU);
            u16 vEnd = (u16)((value >> 10) & 0x3FFU);
            u32 diff = (u32)(g_pops->gpuVertStart - vStart) | (u32)(g_pops->gpuVertEnd - vEnd);
            g_pops->gpuVertStart = vStart;
            g_pops->gpuVertEnd = vEnd;
            if (diff != 0U) {
                g_pops->gpuModeDirty = 2U;
            }
            return;
        }

        case 8U: {
            u8 prevMode = g_pops->gpuDispFlags.bytes.dispModeByte;
            g_pops->gpuDispFlags.bytes.dispModeByte = (u8)value;
            if (((prevMode ^ value) & 0x5FU) != 0U) {
                g_pops->gpuModeDirty = 2U;
                if ((g_gpuInitPhase == 0U && g_pops->gpuDispFlags.bytes.dispModeByte == 39U)
                    || (g_gpuInitPhase == 1U && g_pops->gpuDispFlags.bytes.dispModeByte != 39U)) {
                    g_gpuInitPhase++;
                }
            }
            return;
        }

        case 16U:
            g_pops->gpuReadMode = (u8)(value & 0xFU);
            return;

        default:
            return;
    }
}

/* [156] 0x0119bc */
__attribute__((noinline))
s32 popsGpuDmaTransfer(u32 addr, u32 bytes, u32 chcr)
{
    u32 *pktPtr = (u32 *)((addr & 0x1FFFFFU) | POPS_PSX_RAM_BASE);
    s32 retCycles = (s32)bytes;

    if ((chcr & 0x400U) != 0U) {
        /* Linked-list OT DMA */
        u32 shiftWord = g_pops->gpuDmaCycleShift;
        u32 rShift = shiftWord & 0xFFU;
        u32 lShift = shiftWord >> 8;
        u32 *prevPkt = (u32 *)1;
        s32 budget = 2153;
        s32 dmaWords = 0;
        s32 lastCost = 0;

        g_pops->gpuStatusReg &= 0xEBFFFFFFU;
        if (g_pops->gpuBusyEvent.next != (PopsEvent *)0) {
            budget = 2153 - (s32)(g_pops->gpuBusyEvent.dueCycle - (g_pops->cycleTarget - (u32)g_pops->cycleRemaining));
            popsEventCancel(&g_pops->gpuBusyEvent);
        }

        for (;;) {
            u32 hdr = *pktPtr;
            u32 wordCount = hdr >> 24;
            u32 nextLink = hdr & 0xFFFFFFFCU;

            lastCost = 0;
            if (wordCount == 0U) {
                dmaWords += 4;
            } else {
                s32 baseCost = (s32)wordCount + 7;
                s32 pktCycles = popsGpuExecutePackets((u32)(pktPtr + 1), wordCount * 4U);
                dmaWords += baseCost;
                lastCost = popsMaxS32(((pktCycles << lShift) >> rShift) - baseCost, 0);
                budget -= lastCost;
            }

            if ((nextLink & 0x00800000U) != 0U) {
                budget += lastCost;
                g_pops->gpuStatusReg |= 0x10000000U;
                retCycles = popsMaxS32(1, 2153 - budget);
                popsEventSchedule(&g_pops->gpuBusyEvent, retCycles + lastCost);
                break;
            }

            {
                u32 *nextPtr = (u32 *)((nextLink & 0x1FFFFFU) | POPS_PSX_RAM_BASE);
                retCycles = (s32)((u32)nextPtr - 0x89800000U);
                if (pktPtr == nextPtr || nextPtr == prevPkt) {
                    budget = -33868;
                    break;
                }
                prevPkt = pktPtr;
                if (budget < dmaWords && (*nextPtr & 0x00800000U) == 0U) {
                    break;
                }
                pktPtr = nextPtr;
            }
        }

        {
            s32 totalBusy = dmaWords - budget + 2153;
            sceMeAudio_899A3BC0((void *)g_pops->gpuDlistCursor);
            popsDmaDelayActiveChannels(2U, dmaWords, totalBusy);
            if (totalBusy < (dmaWords * 2)) {
                s32 stall = dmaWords - ((totalBusy - dmaWords) >> 3);
                if ((g_pops->gpuFieldParity & 1U) != 0U) {
                    g_pops->deferredCycles += stall;
                } else {
                    g_pops->cycleRemaining -= stall;
                }
            }
            if (retCycles < 0) {
                g_pops->dmaChannels[2].ev.callback = popsDmaContinueCallback;
                popsEventSchedule(&g_pops->dmaChannels[2].ev, totalBusy);
            }
            return retCycles;
        }
    }

    if ((chcr & 1U) != 0U) {
        /* RAM -> GPU block DMA */
        s32 busyCycles = (s32)(bytes >> 2);
        if (g_pops->gpuFifoWritePos != 0U) {
            u32 curBytes = (u32)g_pops->gpuFifoWritePos * 4U;
            if (curBytes + bytes >= 0xC0U) {
                popsGpuExecutePackets((u32)g_pops->gpuCmdFifo, curBytes);
                g_pops->gpuFifoRemaining = 0U;
            } else {
                u32 wordIdx;
                u32 countWords = bytes >> 2;
                for (wordIdx = 0U; wordIdx < countWords; wordIdx++) {
                    g_pops->gpuCmdFifo[g_pops->gpuFifoWritePos + wordIdx] = pktPtr[wordIdx];
                }
                pktPtr = g_pops->gpuCmdFifo;
                bytes += curBytes;
                g_pops->gpuFifoRemaining = 0U;
            }
            g_pops->gpuFifoWritePos = 0U;
        }

        g_pops->gpuStatusReg &= 0xEBFFFFFFU;
        popsGpuExecutePackets((u32)pktPtr, bytes);
        sceMeAudio_899A3BC0((void *)g_pops->gpuDlistCursor);

        if (g_pops->gpuBusyEvent.next != (PopsEvent *)0) {
            busyCycles += (s32)(g_pops->gpuBusyEvent.dueCycle - (g_pops->cycleTarget - (u32)g_pops->cycleRemaining));
            popsEventCancel(&g_pops->gpuBusyEvent);
        }
        if ((g_pops->quirkFlags & 0x10U) != 0U) {
            if ((g_pops->quirkFlagsExt & 0x1000) == 0) {
                busyCycles = 1;
                popsGpuBusyDoneCallback(&g_pops->gpuBusyEvent);
            }
            retCycles = 1;
        }
        popsEventSchedule(&g_pops->gpuBusyEvent, busyCycles);
        return retCycles;
    }

    /* GPU -> RAM block DMA */
    {
        u32 *lookupClear = (u32 *)(POPS_JIT_LOOKUP_RAM + (addr & 0x1FFFFCU));
        u32 countWords = bytes >> 2;
        u32 idx;
        for (idx = 0U; idx < countWords; idx++) {
            lookupClear[idx] = 0U;
        }
    }
    popsGpuReadVramData((addr & 0x1FFFFFU) | POPS_PSX_RAM_BASE, bytes);
    return retCycles;
}

/* [157] 0x011d0c */
__attribute__((noinline))
u32 popsGpuReadReg(u32 addr, u32 width)
{
    (void)width;
    if ((addr & 4U) != 0U) {
        /* GPUSTAT */
        u32 curTarget = g_pops->cycleTarget;
        s32 nextRem = g_pops->cycleRemaining - 1;
        u32 curCycle = curTarget - (u32)nextRem;
        u32 delta = curCycle - g_pops->gpuPrevStatCycle;

        g_pops->cycleRemaining = nextRem;
        if (delta < 30U) {
            s32 skip = popsMinS32((s32)((curCycle - g_pops->gpuFrameStartCycle) & 0x7FFU), nextRem);
            if (skip > 0) {
                g_pops->cycleRemaining = nextRem - skip;
            }
        }

        {
            u32 dispFlags = g_pops->gpuDispFlags.word;
            u32 status = (g_pops->gpuStatusReg & 0xFF81FFFFU)
                | (((u32)g_pops->gpuDispFlags.bytes.dispModeByte & 0x3FU) << 17);
            u32 oddLineBit;
            u32 invParity = ~(u32)g_pops->gpuFieldParity;

            g_pops->gpuPrevStatCycle = g_pops->gpuLastStatCycle;
            g_pops->gpuLastStatCycle = curCycle;

            status = (status & 0xFFFEB800U)
                | (((dispFlags >> 22) & 1U) << 16)
                | (((dispFlags >> 23) & 1U) << 14)
                | (g_pops->gpuTexPageState & 0x7FFU);

            if (g_pops->gpuPalMode != 0U) {
                u32 shifted = (u32)((s32)invParity >> 1);
                u32 both = shifted & invParity;
                oddLineBit = shifted;
                if ((g_pops->quirkFlags & 0x08000000U) != 0U) {
                    oddLineBit = both;
                }
            } else {
                u32 elapsed = curTarget - (u32)g_pops->cycleRemaining - g_pops->gpuFrameStartCycle;
                oddLineBit = (elapsed >> 11) & invParity;
            }
            return (status & 0x7FFFFFFFU) | ((oddLineBit & 1U) << 31);
        }
    }

    /* GPUREAD */
    g_pops->cycleRemaining -= g_pops->gpuReadCycleCost;
    popsGpuReadVramData((u32)&g_pops->gpuReadWord, 4U);
    return g_pops->gpuReadWord;
}

/* [158] 0x011e0c */
__attribute__((noinline))
void popsGpuReadVramData(u32 dstAddr, u32 byteLen)
{
    u32 curDst = dstAddr;
    u32 endDst = dstAddr + byteLen;
    u32 readMode = g_pops->gpuReadMode;

    if (readMode == 16U) {
        u32 *gePtr = (u32 *)g_pops->gpuDlistCursor;
        if (g_pops->gpuListState < 0) {
            s32 listId = g_pops->gpuGeListId;
            g_pops->gpuListState = 0;
            sceGeListSync(listId, 0);
            g_pops->gpuGeListId = sceGeListEnQueue((const void *)POPS_GE_DLIST_BASE, (void *)POPS_GE_DLIST_BASE, -1, (void *)0);
        }

        readMode = 17U;
        {
            u32 xferW = g_pops->gpuXferSize.wh.w;
            u32 xferH = g_pops->gpuXferSize.wh.h;
            u32 xferX = g_pops->gpuXferStart.xy.x;
            u32 xferY = g_pops->gpuXferStart.xy.y;
            if (((xferW & 7U) | (curDst & 0xFU)) == 0U
                && popsMaxS32((s32)(xferW + xferX), (s32)((xferH + xferY) * 2U)) < 1025
                && byteLen >= (xferW * xferH * 2U)) {
                gePtr[0] = 0xB5000000U | (((curDst >> 24) & 0xFU) << 16) | xferW;
                gePtr[1] = 0xB4000000U | (curDst & 0xFFFFFFU);
                gePtr[2] = 0xEB000000U | ((xferY & 0x1FFU) << 10) | (xferX & 0x3FFU);
                gePtr[3] = ((xferW & 0x3FFU) | ((xferH & 0x3FFU) << 10)) - 0x12000401U;
                gePtr[4] = 0x13041B90U;
                gePtr[5] = 0x0A0000C4U;

                {
                    u32 flushStart = curDst;
                    u32 flushEnd = endDst & 0xFFFFFFC0U;
                    if ((flushStart & 0x3FU) != 0U) {
                        popsCacheDataHitWbInv((const void *)flushStart);
                        flushStart = (flushStart & 0xFFFFFFC0U) + 64U;
                    }
                    if ((endDst & 0x3FU) != 0U) {
                        popsCacheDataHitWbInv((const void *)endDst);
                    }
                    if (((s32)(endDst - flushStart) >> 17) != 0) {
                        sceKernelDcacheInvalidateRangeForUser((const void *)flushStart, flushEnd - flushStart);
                    } else {
                        while (flushStart != flushEnd) {
                            popsCacheDataHitInv((const void *)flushStart);
                            flushStart += 64U;
                        }
                    }
                }

                gePtr += 6;
                readMode = 255U;
                curDst = (curDst + (xferW * xferH * 2U) + 2U) & 0xFFFFFFFCU;
            }
        }

        gePtr++;
        g_pops->gpuReadMode = (u8)readMode;
        g_pops->gpuDlistCursor = (u32)gePtr;
        g_pops->gpuGeListId = sceMeAudio_D96DC042(g_pops->gpuGeListId, (void *)gePtr);
        g_pops->gpuListState = 0;
        if (curDst == endDst) {
            g_pops->gpuLastReadLatch = readMode;
            return;
        }
        g_pops->gpuXferCur.word = 0U;
    }

    if (readMode == 17U) {
        u32 xferW = g_pops->gpuXferSize.wh.w;
        u32 xferH = g_pops->gpuXferSize.wh.h;
        u32 xferX = g_pops->gpuXferStart.xy.x;
        u32 xferY = g_pops->gpuXferStart.xy.y;
        u32 curCol = g_pops->gpuXferCur.xy.x;
        u32 curRow = g_pops->gpuXferCur.xy.y;

        for (;;) {
            u32 vramOff = POPS_VRAM_UNCACHED_BASE | (((xferY + curRow) & 0x1FFU) << 11) | (((xferX + curCol) & 0x3FFU) * 2U);
            *(u16 *)curDst = *(const volatile u16 *)vramOff;
            curCol++;
            if (curCol == xferW) {
                curCol = 0U;
                curRow++;
                g_pops->gpuXferCur.xy.y = (u16)curRow;
                if (curRow == xferH) {
                    break;
                }
            }
            curDst += 2U;
            if (curDst == endDst) {
                g_pops->gpuXferCur.xy.x = (u16)curCol;
                g_pops->gpuLastReadLatch = readMode;
                return;
            }
        }
        g_pops->gpuReadMode = 0xFFU;
        {
            u32 *alignedPtr = (u32 *)(curDst & 0xFFFFFFFCU);
            readMode = *alignedPtr;
            curDst = (u32)(alignedPtr + 1);
            if (curDst == endDst) {
                g_pops->gpuLastReadLatch = readMode;
                return;
            }
        }
    } else {
        switch (readMode) {
            case 2U:
                readMode = g_pops->gpuTexWindowRaw;
                break;
            case 3U:
                readMode = ((u32)(u16)g_pops->gpuClipTop << 10) | (u16)g_pops->gpuClipLeft;
                break;
            case 4U:
                readMode = ((u32)(u16)g_pops->gpuClipBottom << 10) | (u16)g_pops->gpuClipRight;
                break;
            case 5U:
                readMode = (((u32)g_pops->gpuDrawOffY & 0x7FFU) << 11) | ((u32)g_pops->gpuDrawOffX & 0x7FFU);
                break;
            case 7U:
                readMode = 0U;
                break;
            default:
                break;
        }
    }

    while (curDst < endDst) {
        *(u32 *)curDst = readMode;
        curDst += 4U;
    }
    g_pops->gpuLastReadLatch = readMode;
}

/* [159] 0x012120 */
__attribute__((noinline))
s32 popsGpuExecutePackets(u32 startAddr, u32 byteLen)
{
    u32 curAddr = startAddr;
    u32 endAddr = startAddr + byteLen;
    u32 *gePtr = (u32 *)g_pops->gpuDlistCursor;
    s32 totalCost = 0;
    u32 pktState;

    if ((u32)gePtr > POPS_GE_DLIST_LIMIT) {
        gePtr[0] = 0x0F000000U;
        gePtr[1] = 0x0C000000U;
        totalCost = 0x100000;
        gePtr = (u32 *)POPS_GE_DLIST_BASE;
        sceMeAudio_899A3BC0((void *)0);
        g_pops->gpuListState = 0;
        sceGeListSync(g_pops->gpuGeListId, 0);
        g_pops->gpuGeListId = sceGeListEnQueue((const void *)POPS_GE_DLIST_BASE, (void *)POPS_GE_DLIST_BASE, -1, (void *)0);
    } else if (g_pops->gpuListState < 0) {
        g_pops->gpuListState = 0;
        sceGeListSync(g_pops->gpuGeListId, 0);
        g_pops->gpuGeListId = sceGeListEnQueue((const void *)POPS_GE_DLIST_BASE, (void *)POPS_GE_DLIST_BASE, -1, (void *)0);
    }

    pktState = g_pops->gpuPacketState;

    while (curAddr < endAddr) {
        u32 cmdWord = *(const u32 *)curAddr;
        if (pktState == 0U) {
            pktState = cmdWord >> 29;
        }

        switch (pktState) {
            case 0U: {
                /* GP0 misc: NOP (0), clear texture cache (1), quick rectangle fill (2) */
                u32 subOp = (cmdWord >> 24) & 0x1FU;
                pktState = 0U;
                if (subOp == 1U) {
                    gePtr[0] = 0xB01C0000U;
                    gePtr[1] = 0xC4000001U;
                    gePtr += 2;
                    g_pops->gpuTexPageState |= 0x8000U;
                } else if (subOp == 2U) {
                    const u32 *pktWords = (const u32 *)curAddr;
                    u32 sizeWord;
                    u32 width;
                    u32 height;
                    curAddr += 8U;
                    if (curAddr == endAddr) {
                        break;
                    }
                    sizeWord = pktWords[2];
                    width = sizeWord & 0x3FFU;
                    height = (sizeWord >> 16) & 0x1FFU;
                    if (width == 0U || height == 0U) {
                        break;
                    }
                    {
                        u32 posWord = pktWords[1];
                        u32 xPos = ((g_pops->quirkFlags & 0x01000000U) != 0U) ? (posWord & 0x3FFU) : (posWord & 0x3F0U);
                        u32 yPos = (posWord >> 16) & 0x1FFU;
                        u32 yEnd = yPos + height;

                        gePtr[0] = 0xD4000000U | (yPos << 10) | xPos;
                        gePtr[1] = 0xD5000000U | ((yEnd - 1U) << 10) | (xPos + width - 1U);
                        totalCost += (s32)((width * height) >> (g_pops->gpuInterlaceHalf + 4U));
                        popsGpuInvalidateTexRect(xPos, width, yPos, height, 0);

                        if ((xPos - g_pops->gpuDispX) < g_pops->gpuDispWidth) {
                            if ((yPos - g_pops->gpuDispY) < g_pops->gpuDispHeight
                                || ((yEnd - 1U) - g_pops->gpuDispY) < g_pops->gpuDispHeight) {
                                g_pops->gpuFrameDirty = 1;
                            }
                        }

                        gePtr[2] = 0x55000000U | (cmdWord & 0xF8F8F8U);
                        gePtr[3] = 0x13041B90U;
                        gePtr[4] = 0x0A000040U;
                        gePtr[5] = 0xD4000000U | (((u32)(u16)g_pops->gpuClipTop) << 10) | (u16)g_pops->gpuClipLeft;
                        gePtr[6] = 0xD5000000U | (((u32)(u16)g_pops->gpuClipBottom) << 10) | (u16)g_pops->gpuClipRight;
                        gePtr += 7;
                    }
                }
                break;
            }

            case 1U: {
                /* GP0 Polygon (flat/gouraud, untextured/textured, tri/quad) */
                const u32 *pktWords = (const u32 *)curAddr;
                const u16 *pktHalves = (const u16 *)curAddr;
                s32 xA = popsSignExtend11(pktWords[1]);
                s32 yA = popsSignExtend11High(pktWords[1]);
                s32 minX;
                s32 maxX;
                s32 minY;
                s32 maxY;
                s32 areaSum = 0;
                s32 skipPoly = 0;

                totalCost += 30;

                if ((cmdWord & 0x04000000U) != 0U) {
                    /* Textured polygon */
                    u16 rawTpage = ((cmdWord & 0x10000000U) != 0U) ? pktHalves[11] : pktHalves[9];
                    u16 curTpage = g_pops->gpuTexPageState;
                    if ((curTpage & 0xC1FFU) != (rawTpage & 0x1FFU)) {
                        u32 mergedTpage = ((curTpage & 0xFE00U) | (rawTpage & 0x1FFU)) & 0x7FFU;
                        gePtr = popsGpuUploadTexPage(gePtr, mergedTpage, 1);
                        totalCost += g_pops->gpuTexPageCost;
                    }

                    {
                        u16 clutWord = pktHalves[5];
                        if ((clutWord & 0x3FU) >= 0x31U && g_pops->gpuTexDepth == 1U
                            && (g_pops->quirkFlagsExt & 0x100) != 0) {
                            clutWord = (u16)(clutWord - 64U);
                        }
                        *gePtr++ = 0xB0000000U | ((u32)(clutWord & 0x7FFFU) << 5);
                    }

                    {
                        u32 *drawBase = gePtr;
                        s32 isQuad = ((cmdWord & 0x08000000U) != 0U) ? 1 : 0;
                        u32 shadeColor;
                        u8 rawTexFlag = ((cmdWord & 0x01000000U) != 0U) ? 1U : 0U;

                        if ((cmdWord & 0x10000000U) == 0U) {
                            /* Flat textured tri/quad */
                            s32 xB = popsSignExtend11(pktWords[3]);
                            s32 yB = popsSignExtend11High(pktWords[3]);
                            s32 xC = popsSignExtend11(pktWords[5]);
                            s32 yC = popsSignExtend11High(pktWords[5]);
                            s32 xD = xB - xA;
                            s32 yD = yC - yA;
                            s32 crossA = ((xB - xA) * (yC - yA)) - ((xC - xA) * (yB - yA));

                            areaSum = popsAbsS32(crossA);
                            minX = popsMinS32(popsMinS32(xA, xB), xC);
                            maxX = popsMaxS32(popsMaxS32(xA, xB), xC);
                            minY = popsMinS32(popsMinS32(yA, yB), yC);
                            maxY = popsMaxS32(popsMaxS32(yA, yB), yC);

                            if (isQuad != 0) {
                                u32 posD = pktWords[7];
                                u16 uvD = pktHalves[16];
                                curAddr += 8U;
                                xD = popsSignExtend11(posD);
                                yD = popsSignExtend11High(posD);
                                drawBase[10] = (u32)uvD | ((u32)(u16)xD << 16);
                                drawBase[11] = (u32)yD;
                                minX = popsMinS32(minX, xD);
                                maxX = popsMaxS32(maxX, xD);
                                minY = popsMinS32(minY, yD);
                                maxY = popsMaxS32(maxY, yD);
                                areaSum += popsAbsS32(((xB - xD) * (yC - yD)) - ((xC - xD) * (yB - yD)));
                            }
                            curAddr += 24U;

                            if (popsMaxS32(maxX - minX, (maxY - minY) * 2) >= 1024) {
                                s32 validMask = 0;
                                if (isQuad != 0) {
                                    s32 bcMaxY = popsMaxS32(yB, yC);
                                    s32 bcMinY = popsMinS32(yB, yC);
                                    s32 bcMaxX = popsMaxS32(xB, xC);
                                    s32 bcMinX = popsMinS32(xB, xC);
                                    if (popsMaxS32(popsMaxS32(bcMaxX, xD) - popsMinS32(bcMinX, xD),
                                                   (popsMaxS32(bcMaxY, yD) - popsMinS32(bcMinY, yD)) * 2) < 1024) {
                                        validMask |= 1;
                                    }
                                    if (popsMaxS32(popsMaxS32(bcMaxX, xA) - popsMinS32(bcMinX, xA),
                                                   (popsMaxS32(bcMaxY, yA) - popsMinS32(bcMinY, yA)) * 2) < 1024) {
                                        validMask |= 2;
                                    }
                                }
                                if (validMask == 0) {
                                    skipPoly = 1;
                                } else if (validMask != 3) {
                                    areaSum = 0;
                                    cmdWord = (cmdWord & 0xF7FFFFFFU) | ((u32)(validMask & 1) << 27);
                                    isQuad = ((cmdWord & 0x08000000U) != 0U) ? 1 : 0;
                                    if ((validMask & 1) != 0) {
                                        xA = xB;
                                        yA = yB;
                                    }
                                }
                            }

                            if (skipPoly == 0) {
                                shadeColor = (rawTexFlag != 0U) ? 0x55808080U : (0x55000000U | (cmdWord & 0xFFFFFFU));
                                drawBase[0] = shadeColor;
                                drawBase[4] = (u32)pktHalves[4] | ((u32)(u16)xA << 16);
                                drawBase[5] = (u32)yA;
                                drawBase[6] = (u32)pktHalves[8] | ((u32)(u16)xB << 16);
                                drawBase[7] = (u32)yB;
                                drawBase[8] = (u32)pktHalves[12] | ((u32)(u16)xC << 16);
                                drawBase[9] = (u32)yC;
                                gePtr = drawBase + 1;
                            }
                        } else {
                            /* Gouraud textured tri/quad */
                            s32 xB = popsSignExtend11(pktWords[4]);
                            s32 yB = popsSignExtend11High(pktWords[4]);
                            s32 xC = popsSignExtend11(pktWords[7]);
                            s32 yC = popsSignExtend11High(pktWords[7]);
                            s32 xD = xB - xA;
                            s32 yD = yC - yA;
                            s32 crossA = ((xB - xA) * (yC - yA)) - ((xC - xA) * (yB - yA));

                            areaSum = popsAbsS32(crossA);
                            minX = popsMinS32(popsMinS32(xA, xB), xC);
                            maxX = popsMaxS32(popsMaxS32(xA, xB), xC);
                            minY = popsMinS32(popsMinS32(yA, yB), yC);
                            maxY = popsMaxS32(popsMaxS32(yA, yB), yC);

                            if (isQuad != 0) {
                                u32 posD = pktWords[10];
                                u32 uvD = pktHalves[22];
                                curAddr += 12U;
                                xD = popsSignExtend11(posD);
                                yD = popsSignExtend11High(posD);
                                drawBase[15] = uvD;
                                drawBase[17] = (u32)(u16)xD | ((u32)(u16)yD << 16);
                                minX = popsMinS32(minX, xD);
                                maxX = popsMaxS32(maxX, xD);
                                minY = popsMinS32(minY, yD);
                                maxY = popsMaxS32(maxY, yD);
                                areaSum += popsAbsS32(((xB - xD) * (yC - yD)) - ((xC - xD) * (yB - yD)));
                            }
                            curAddr += 32U;

                            if (popsMaxS32(maxX - minX, (maxY - minY) * 2) >= 1024) {
                                s32 validMask = 0;
                                if (isQuad != 0) {
                                    s32 bcMaxY = popsMaxS32(yB, yC);
                                    s32 bcMinY = popsMinS32(yB, yC);
                                    s32 bcMaxX = popsMaxS32(xB, xC);
                                    s32 bcMinX = popsMinS32(xB, xC);
                                    if (popsMaxS32(popsMaxS32(bcMaxX, xD) - popsMinS32(bcMinX, xD),
                                                   (popsMaxS32(bcMaxY, yD) - popsMinS32(bcMinY, yD)) * 2) < 1024) {
                                        validMask |= 1;
                                    }
                                    if (popsMaxS32(popsMaxS32(bcMaxX, xA) - popsMinS32(bcMinX, xA),
                                                   (popsMaxS32(bcMaxY, yA) - popsMinS32(bcMinY, yA)) * 2) < 1024) {
                                        validMask |= 2;
                                    }
                                }
                                if (validMask == 0) {
                                    skipPoly = 1;
                                } else if (validMask != 3) {
                                    areaSum = 0;
                                    cmdWord = (cmdWord & 0xF7FFFFFFU) | ((u32)(validMask & 1) << 27);
                                    isQuad = ((cmdWord & 0x08000000U) != 0U) ? 1 : 0;
                                    if ((validMask & 1) != 0) {
                                        xA = xB;
                                        yA = yB;
                                    }
                                }
                            }

                            if (skipPoly == 0) {
                                u32 colB = 0x80808080U;
                                u32 colC = 0x80808080U;
                                u32 colD = 0x80808080U;
                                shadeColor = 0x80808080U;
                                if (rawTexFlag == 0U) {
                                    colB = (pktWords[3] & 0xFFFFFFU) | 0x80000000U;
                                    colC = (pktWords[6] & 0xFFFFFFU) | 0x80000000U;
                                    colD = (pktWords[9] & 0xFFFFFFU) | 0x80000000U;
                                    shadeColor = cmdWord;
                                }
                                drawBase[3] = pktHalves[4];
                                drawBase[4] = shadeColor;
                                drawBase[5] = (u32)(u16)xA | ((u32)(u16)yA << 16);
                                drawBase[7] = pktHalves[10];
                                drawBase[8] = colB;
                                drawBase[9] = (u32)(u16)xB | ((u32)(u16)yB << 16);
                                drawBase[11] = pktHalves[16];
                                drawBase[12] = colC;
                                drawBase[13] = (u32)(u16)xC | ((u32)(u16)yC << 16);
                                drawBase[16] = colD;
                            }
                        }

                        if (skipPoly == 0) {
                            u16 tpage = g_pops->gpuTexPageState;
                            s32 maskMode = (((g_pops->gpuStatusReg >> 11) & 3U) != 3U) ? 1 : 0;
                            u32 ditherBit = ((tpage & 0x200U) == 0U) ? 1U : (u32)(rawTexFlag & 1U);

                            gePtr[0] = 0x14000000U;
                            gePtr[1] = (0x53B7F800U | (((cmdWord >> 25) & 0xFU) << 6) | (ditherBit * 4U)) - (u32)gePtr;
                            if ((shadeColor & 0x808080U) == 0x808080U) {
                                maskMode = -1;
                            } else if ((g_pops->gpuTexDepth & 2U) != 0U
                                       && ((u8)(shadeColor | (shadeColor >> 8) | (shadeColor >> 16))) >= 5U) {
                                s32 texLeft = (s32)((8U * (tpage & 0xFU)) + g_pops->gpuTexWinOffX);
                                s32 texTop = (s32)((g_pops->gpuTexWinOffY & 0xDFU) | (((tpage & 0x10U) != 0U) ? 32U : 0U));
                                if (8 * (texLeft + (s32)g_pops->gpuTexWinMaskX) >= g_pops->gpuClipLeft
                                    && g_pops->gpuClipRight >= (8 * texLeft)
                                    && 8 * (texTop + (s32)g_pops->gpuTexWinMaskY) >= g_pops->gpuClipTop
                                    && g_pops->gpuClipBottom >= (8 * texTop)) {
                                    maskMode = -1;
                                }
                            }
                            {
                                u32 maskOff = (0x51B7FE40U | (((g_pops->gpuStatusReg & 0x800U) != 0U) ? 4U : 0U)) + (u32)(maskMode << 6);
                                gePtr[2] = ((maskOff & 0xFFFFFEFFU) | ((u32)isQuad << 8)) - (u32)gePtr;
                            }
                            gePtr += 19;
                        }
                    }
                } else {
                    /* Untextured polygon */
                    if ((g_pops->gpuTexPageState & 0x4000U) != 0U) {
                        u16 newTpage = (u16)(g_pops->gpuTexPageState - 0x4000U);
                        g_pops->gpuTexPageState = newTpage;
                        gePtr[0] = 0x13041B90U;
                        gePtr[1] = 0x0A000080U | (((newTpage >> 5) & 3U) << 4);
                        gePtr += 2;
                    }
                    gePtr[0] = 0x14000000U;

                    if ((cmdWord & 0x10000000U) == 0U) {
                        /* Flat untextured tri/quad */
                        s32 xB = popsSignExtend11(pktWords[2]);
                        s32 yB = popsSignExtend11High(pktWords[2]);
                        s32 xC = popsSignExtend11(pktWords[3]);
                        s32 yC = popsSignExtend11High(pktWords[3]);
                        s32 xD = xB - xA;
                        s32 yD = yC - yA;
                        s32 crossA = ((xB - xA) * (yC - yA)) - ((xC - xA) * (yB - yA));

                        areaSum = popsAbsS32(crossA);
                        minX = popsMinS32(popsMinS32(xA, xB), xC);
                        maxX = popsMaxS32(popsMaxS32(xA, xB), xC);
                        minY = popsMinS32(popsMinS32(yA, yB), yC);
                        maxY = popsMaxS32(popsMaxS32(yA, yB), yC);

                        if ((cmdWord & 0x08000000U) != 0U) {
                            u32 posD = pktWords[4];
                            u16 *geHalf = (u16 *)gePtr;
                            curAddr += 4U;
                            xD = popsSignExtend11(posD);
                            yD = popsSignExtend11High(posD);
                            geHalf[15] = (u16)xD;
                            geHalf[16] = (u16)yD;
                            minX = popsMinS32(minX, xD);
                            maxX = popsMaxS32(maxX, xD);
                            minY = popsMinS32(minY, yD);
                            maxY = popsMaxS32(maxY, yD);
                            areaSum += popsAbsS32(((xB - xD) * (yC - yD)) - ((xC - xD) * (yB - yD)));
                        }
                        curAddr += 12U;

                        if (popsMaxS32(maxX - minX, (maxY - minY) * 2) >= 1024) {
                            s32 validMask = 0;
                            if ((cmdWord & 0x08000000U) != 0U) {
                                s32 bcMaxY = popsMaxS32(yB, yC);
                                s32 bcMinY = popsMinS32(yB, yC);
                                s32 bcMaxX = popsMaxS32(xB, xC);
                                s32 bcMinX = popsMinS32(xB, xC);
                                if (popsMaxS32(popsMaxS32(bcMaxX, xD) - popsMinS32(bcMinX, xD),
                                               (popsMaxS32(bcMaxY, yD) - popsMinS32(bcMinY, yD)) * 2) < 1024) {
                                    validMask |= 1;
                                }
                                if (popsMaxS32(popsMaxS32(bcMaxX, xA) - popsMinS32(bcMinX, xA),
                                               (popsMaxS32(bcMaxY, yA) - popsMinS32(bcMinY, yA)) * 2) < 1024) {
                                    validMask |= 2;
                                }
                            }
                            if (validMask == 0) {
                                skipPoly = 1;
                            } else if (validMask != 3) {
                                areaSum = 0;
                                cmdWord = (cmdWord & 0xF7FFFFFFU) | ((u32)(validMask & 1) << 27);
                                if ((validMask & 1) != 0) {
                                    xA = xB;
                                    yA = yB;
                                }
                            }
                        }

                        if (skipPoly == 0) {
                            u16 *geHalf = (u16 *)gePtr;
                            gePtr[1] = 0x55000000U | (cmdWord & 0xFFFFFFU);
                            gePtr[2] = (0x51B7F800U | (((cmdWord >> 25) & 0xFU) << 6)) - (u32)gePtr;
                            gePtr[3] = (u32)(u16)xA | ((u32)(u16)yA << 16);
                            geHalf[9] = (u16)xB;
                            geHalf[10] = (u16)yB;
                            gePtr[6] = (u32)(u16)xC | ((u32)(u16)yC << 16);
                            gePtr += 9;
                        }
                    } else {
                        /* Gouraud untextured tri/quad */
                        s32 xB = popsSignExtend11(pktWords[3]);
                        s32 yB = popsSignExtend11High(pktWords[3]);
                        s32 xC = popsSignExtend11(pktWords[5]);
                        s32 yC = popsSignExtend11High(pktWords[5]);
                        s32 xD = xB - xA;
                        s32 yD = yC - yA;
                        s32 crossA = ((xB - xA) * (yC - yA)) - ((xC - xA) * (yB - yA));

                        areaSum = popsAbsS32(crossA);
                        minX = popsMinS32(popsMinS32(xA, xB), xC);
                        maxX = popsMaxS32(popsMaxS32(xA, xB), xC);
                        minY = popsMinS32(popsMinS32(yA, yB), yC);
                        maxY = popsMaxS32(popsMaxS32(yA, yB), yC);

                        if ((cmdWord & 0x08000000U) != 0U) {
                            u32 posD = pktWords[7];
                            s32 screenY = yA + g_pops->gpuDrawOffY;
                            curAddr += 8U;
                            xD = popsSignExtend11(posD);
                            yD = popsSignExtend11High(posD);
                            minX = popsMinS32(minX, xD);
                            maxX = popsMaxS32(maxX, xD);
                            minY = popsMinS32(minY, yD);
                            maxY = popsMaxS32(maxY, yD);
                            if (screenY >= 1024) {
                                yA -= 2048;
                                yB -= 2048;
                                yC -= 2048;
                                yD -= 2048;
                            } else if (screenY < -1024) {
                                yA += 2048;
                                yB += 2048;
                                yC += 2048;
                                yD += 2048;
                            }
                            gePtr[11] = pktWords[6];
                            gePtr[12] = (u32)(u16)xD | ((u32)(u16)yD << 16);
                            areaSum += popsAbsS32(((xB - xD) * (yC - yD)) - ((xC - xD) * (yB - yD)));
                        }
                        curAddr += 20U;

                        if (popsMaxS32(maxX - minX, (maxY - minY) * 2) >= 1024) {
                            s32 validMask = 0;
                            if ((cmdWord & 0x08000000U) != 0U) {
                                s32 bcMaxY = popsMaxS32(yB, yC);
                                s32 bcMinY = popsMinS32(yB, yC);
                                s32 bcMaxX = popsMaxS32(xB, xC);
                                s32 bcMinX = popsMinS32(xB, xC);
                                if (popsMaxS32(popsMaxS32(bcMaxX, xD) - popsMinS32(bcMinX, xD),
                                               (popsMaxS32(bcMaxY, yD) - popsMinS32(bcMinY, yD)) * 2) < 1024) {
                                    validMask |= 1;
                                }
                                if (popsMaxS32(popsMaxS32(bcMaxX, xA) - popsMinS32(bcMinX, xA),
                                               (popsMaxS32(bcMaxY, yA) - popsMinS32(bcMinY, yA)) * 2) < 1024) {
                                    validMask |= 2;
                                }
                            }
                            if (validMask == 0) {
                                skipPoly = 1;
                            } else if (validMask != 3) {
                                areaSum = 0;
                                cmdWord = (cmdWord & 0xF7FFFFFFU) | ((u32)(validMask & 1) << 27);
                                if ((validMask & 1) != 0) {
                                    xA = xB;
                                    yA = yB;
                                }
                            }
                        }

                        if (skipPoly == 0) {
                            u32 ditherBase = 0x51B7F801U - (((g_pops->gpuTexPageState & 0x200U) != 0U) ? 1U : 0U);
                            gePtr[1] = ((ditherBase & 0xFFFFFC3FU) | (((cmdWord >> 25) & 0xFU) << 6)) - (u32)gePtr;
                            gePtr[2] = cmdWord;
                            gePtr[3] = (u32)(u16)xA | ((u32)(u16)yA << 16);
                            gePtr[5] = pktWords[2];
                            gePtr[6] = (u32)(u16)xB | ((u32)(u16)yB << 16);
                            gePtr[8] = pktWords[4];
                            gePtr[9] = (u32)(u16)xC | ((u32)(u16)yC << 16);
                            gePtr += 14;
                        }
                    }
                }

                if (skipPoly == 0) {
                    s32 offX = g_pops->gpuDrawOffX;
                    s32 offY = g_pops->gpuDrawOffY;
                    s32 boxMaxX = maxX + offX;
                    s32 boxMaxY = maxY + offY;
                    s32 boxMinX = minX + offX;
                    s32 boxMinY = minY + offY;

                    if ((g_pops->gpuClipRight + 1) >= boxMaxX && (g_pops->gpuClipBottom + 1) >= boxMaxY) {
                        if ((g_pops->quirkFlags & 0x20U) != 0U
                            && (boxMinX < g_pops->gpuClipLeft || boxMinY < g_pops->gpuClipTop)) {
                            areaSum = 0;
                        }
                        {
                            u32 untexShift = ((cmdWord & 0x04000000U) == 0U) ? 1U : 0U;
                            s32 fillCost = areaSum >> untexShift;
                            u32 blendCheck = (((cmdWord & 0x02000000U) != 0U) ? 1U : 0U)
                                | ((g_pops->gpuStatusReg >> 11) & 2U);
                            if (boxMaxX < g_pops->gpuClipLeft || boxMaxY < g_pops->gpuClipTop) {
                                blendCheck = 0U;
                            }
                            if (blendCheck != 0U) {
                                fillCost += areaSum;
                            }
                            totalCost += fillCost >> (g_pops->gpuInterlaceHalf + 1U);
                        }
                    }

                    g_pops->gpuFrameDirty |= (s8)g_pops->gpuDrawOverlapsDisp;
                    if (g_pops->gpuDrawInsideTex != 0U) {
                        popsGpuInvalidateTexRect((u32)boxMinX, (u32)(boxMaxX - boxMinX),
                                                 (u32)boxMinY, (u32)(boxMaxY - boxMinY), 1);
                    }
                }
                pktState = 0U;
                break;
            }

            case 2U:
            case 8U: {
                /* GP0 Line / Polyline (case 8 resumes an interrupted polyline) */
                u32 lineCmd;
                u32 vertCountCmd;
                u32 *vertCursor;
                u32 curColor;
                s32 resumePoly = (pktState == 8U) ? 1 : 0;
                s32 finishedLine = 1;

                if (resumePoly != 0) {
                    lineCmd = g_pops->gpuXferSize.word;
                    vertCountCmd = g_pops->gpuXferStart.word;
                    vertCursor = (u32 *)g_pops->gpuXferCur.word;
                    curColor = lineCmd;
                } else {
                    if ((g_pops->quirkFlags & 0x40U) != 0U && (cmdWord & 0x01000000U) != 0U
                        && ((s32)(endAddr - curAddr) >> 2) != 3) {
                        g_pops->gpuPacketState = 0U;
                        g_pops->gpuDlistCursor = (u32)gePtr;
                        return totalCost;
                    }
                    if ((g_pops->gpuTexPageState & 0x4000U) != 0U) {
                        u16 newTpage = (u16)(g_pops->gpuTexPageState - 0x4000U);
                        g_pops->gpuTexPageState = newTpage;
                        gePtr[0] = 0x13041B90U;
                        gePtr[1] = 0x0A000080U | (((newTpage >> 5) & 3U) << 4);
                        gePtr += 2;
                    }

                    {
                        const u32 *pktWords = (const u32 *)curAddr;
                        s32 xA = popsSignExtend11(pktWords[1]);
                        s32 yA = popsSignExtend11High(pktWords[1]);
                        s32 xB;
                        s32 yB;
                        s32 dx;
                        s32 dy;

                        lineCmd = cmdWord;
                        curColor = cmdWord;
                        vertCountCmd = 0x04020003U;
                        vertCursor = gePtr + 14;
                        gePtr[5] = cmdWord;
                        g_pops->gpuFrameDirty |= (s8)g_pops->gpuDrawOverlapsDisp;

                        if ((lineCmd & 0x10000000U) != 0U) {
                            curColor = pktWords[2];
                            curAddr += 4U;
                        }
                        curAddr += 8U;
                        {
                            u32 posB = *(const u32 *)curAddr;
                            xB = popsSignExtend11(posB);
                            yB = popsSignExtend11High(posB);
                        }
                        dx = popsAbsS32(xA - xB);
                        dy = popsAbsS32(yA - yB);

                        if (popsMaxS32(dx, dy * 2) >= 1024) {
                            vertCursor = gePtr + 11;
                            vertCountCmd = 0x04020002U;
                            if ((lineCmd & 0x08000000U) == 0U) {
                                pktState = 0U;
                                break;
                            }
                        } else {
                            s32 span = popsMaxS32(dx, dy);
                            s32 lineCost = span >> 1;
                            if (((lineCmd & 0x02000000U) != 0U) || ((g_pops->gpuStatusReg >> 11) & 2U) != 0U) {
                                lineCost += span;
                            }
                            totalCost += lineCost >> g_pops->gpuInterlaceHalf;
                            gePtr[6] = (u32)(u16)xA | ((u32)(u16)yA << 16);
                        }

                        vertCursor[-6] = curColor;
                        vertCursor[-5] = (u32)(u16)xB | ((u32)(u16)yB << 16);

                        if ((lineCmd & 0x08000000U) != 0U) {
                            curAddr += 4U;
                            resumePoly = 1;
                        }
                    }
                }

                if (resumePoly != 0) {
                    if (curAddr >= endAddr) {
                        g_pops->gpuXferStart.word = vertCountCmd;
                        g_pops->gpuXferSize.word = lineCmd;
                        g_pops->gpuXferCur.word = (u32)vertCursor;
                        g_pops->gpuPacketState = 8U;
                        g_pops->gpuDlistCursor = (u32)gePtr;
                        return totalCost;
                    }
                    while ((*(const u32 *)curAddr & 0xF000F000U) != 0x50005000U) {
                        if (curAddr >= endAddr) {
                            g_pops->gpuXferStart.word = vertCountCmd;
                            g_pops->gpuXferSize.word = lineCmd;
                            g_pops->gpuXferCur.word = (u32)vertCursor;
                            g_pops->gpuPacketState = 8U;
                            g_pops->gpuDlistCursor = (u32)gePtr;
                            return totalCost;
                        }
                        vertCountCmd++;
                        if ((lineCmd & 0x10000000U) != 0U) {
                            curColor = *(const u32 *)curAddr;
                            curAddr += 4U;
                        }
                        {
                            u32 nextPos = *(const u32 *)curAddr;
                            s32 prevX = (s16)(vertCursor[-5] & 0xFFFFU);
                            s32 prevY = (s16)(vertCursor[-5] >> 16);
                            s32 nextX = popsSignExtend11(nextPos);
                            s32 nextY = popsSignExtend11High(nextPos);
                            s32 segSpan = popsMaxS32(popsAbsS32(prevX - nextX) & 0x3FF,
                                                     popsAbsS32(prevY - nextY) & 0x1FF);
                            s32 segCost = segSpan >> 1;
                            vertCursor[-3] = curColor;
                            vertCursor[-2] = (u32)(u16)nextX | ((u32)(u16)nextY << 16);
                            if (((lineCmd & 0x02000000U) != 0U) || ((g_pops->gpuStatusReg >> 11) & 2U) != 0U) {
                                segCost += segSpan;
                            }
                            totalCost += segCost >> g_pops->gpuInterlaceHalf;
                            vertCursor += 3;
                            curAddr += 4U;
                        }
                    }
                }

                if (finishedLine != 0) {
                    vertCursor[-3] = curColor;
                    vertCursor[-2] = vertCursor[-5] + 1U;
                    gePtr[0] = 0x14000000U;
                    gePtr[1] = 0x21000000U | (((lineCmd & 0x02000000U) != 0U) ? 1U : 0U);
                    gePtr[2] = 0x53B7FC00U - (u32)gePtr;
                    gePtr[3] = vertCountCmd;
                    gePtr[4] = 0x08000000U + ((u32)vertCursor - (u32)gePtr);
                    gePtr = vertCursor;
                }
                pktState = 0U;
                break;
            }

            case 3U: {
                /* GP0 Rectangle / Sprite (variable, 1x1, 8x8, 16x16) */
                const u32 *pktWords = (const u32 *)curAddr;
                const u8 *pktBytes = (const u8 *)curAddr;
                const u16 *pktHalves = (const u16 *)curAddr;
                u32 sizeCode = (cmdWord >> 27) & 3U;
                s32 rawX = popsSignExtend11(pktWords[1]);
                s32 rawY = popsSignExtend11High(pktWords[1]);
                u32 uvOff = ((cmdWord & 0x04000000U) != 0U) ? 4U : 0U;
                s32 rectW;
                s32 rectH;

                if (sizeCode != 0U) {
                    rectH = (sizeCode == 1U) ? 1 : (s32)((sizeCode - 1U) * 8U);
                    rectW = rectH;
                    curAddr += uvOff + 4U;
                } else {
                    u32 dimWord = pktWords[2U + (uvOff >> 2)];
                    curAddr += uvOff + 8U;
                    rectW = (s32)(dimWord & 0x3FFU);
                    rectH = (s32)((dimWord >> 16) & 0x1FFU);
                }

                {
                    s32 offX = g_pops->gpuDrawOffX;
                    s32 offY = g_pops->gpuDrawOffY;
                    s32 screenX = rawX + offX;
                    s32 screenY = rawY + offY;
                    s32 visW;
                    s32 visH;

                    if (screenY >= 1024) {
                        screenY -= 2048;
                    }
                    if (screenX >= 1024) {
                        screenX -= 2048;
                    }
                    if (screenY < -1024) {
                        screenY += 2048;
                    }
                    if (screenX < -1024) {
                        screenX += 2048;
                    }

                    visW = popsMinS32(screenX + rectW, g_pops->gpuClipRight) - popsMaxS32(screenX, g_pops->gpuClipLeft);
                    visH = popsMinS32(screenY + rectH, g_pops->gpuClipBottom) - popsMaxS32(screenY, g_pops->gpuClipTop);
                    totalCost += 30;

                    if ((visW | visH) >= 0) {
                        s32 pixArea = visW * visH;
                        s32 fillCost = pixArea >> 1;
                        u32 drawX = (u32)(screenX - offX);
                        u32 drawY = (u32)(screenY - offY);
                        u32 fillColor = 0x55000000U | (cmdWord & 0xFFFFFFU);
                        u16 curTpage = g_pops->gpuTexPageState;

                        if (((cmdWord & 0x02000000U) != 0U) || ((g_pops->gpuStatusReg >> 11) & 2U) != 0U) {
                            fillCost += pixArea;
                        }
                        g_pops->gpuFrameDirty |= (s8)g_pops->gpuDrawOverlapsDisp;
                        totalCost += fillCost >> g_pops->gpuInterlaceHalf;

                        if ((cmdWord & 0x04000000U) != 0U) {
                            /* Textured rectangle */
                            u32 dirtyBits = curTpage & 0xC000U;
                            if (curTpage != (curTpage & 0x7FFU)) {
                                if (dirtyBits != 0U) {
                                    gePtr = popsGpuUploadTexPage(gePtr, curTpage & 0x7FFU, 0);
                                }
                                {
                                    s32 winArea = ((s32)g_pops->gpuTexWinMaskX * (s32)g_pops->gpuTexWinMaskY)
                                        << (g_pops->gpuTexDepth + 1U);
                                    s32 texCost;
                                    if (winArea < 128) {
                                        winArea = 0;
                                    }
                                    texCost = popsMinS32(winArea, pixArea);
                                    if ((g_pops->quirkFlags & 0x400000U) != 0U && texCost >= 1025) {
                                        texCost *= 2;
                                        g_pops->gpuTexPageState |= 0x2000U;
                                    }
                                    totalCost += texCost;
                                }
                            }

                            {
                                u16 clutWord = pktHalves[5];
                                if ((clutWord & 0x3FU) >= 0x31U && g_pops->gpuTexDepth == 1U
                                    && (g_pops->quirkFlagsExt & 0x100) != 0) {
                                    clutWord = (u16)(clutWord - 64U);
                                }
                                *gePtr++ = 0xB0000000U | ((u32)(clutWord & 0x7FFFU) << 5);
                            }

                            if (g_pops->gpuInterlaceActive != 0U
                                && (g_pops->quirkFlagsExt & 0x40000000) != 0
                                && rectW == 0x100 && rectH == 0x100
                                && (drawX & 0xFFU) == 0U && (drawY & 0xFFU) == 0U) {
                                gePtr[0] = 0x13041B91U;
                                gePtr[1] = 0x0A000020U;
                                gePtr += 2;
                            }

                            if ((g_pops->quirkFlags & 0x04000000U) != 0U) {
                                popsGpuInvalidateTexRect(drawX + (u32)offX, (u32)rectW,
                                                         drawY + (u32)offY, (u32)rectH, 0);
                            }

                            {
                                u32 uvPacked = (u32)pktBytes[8] | ((u32)pktBytes[9] << 16);
                                u32 uvEnd;
                                u32 clzX;
                                u32 clzY;
                                s32 maskMode;

                                gePtr[6] = uvPacked;
                                if (((rectW & 1) | (((uvPacked + (u32)rectW - 1U) << g_pops->gpuTexDepth) & 0xFU)) == 0U) {
                                    uvPacked &= ~1U;
                                }
                                uvEnd = popsRotr32(uvPacked, 16U) + ((u32)(u16)rectH | ((u32)(u16)rectW << 16));
                                gePtr[7] = (drawX & 0xFFFFU) | ((drawY & 0xFFFFU) << 16);
                                gePtr[8] = uvEnd;
                                gePtr[9] = (uvEnd & 0xFFFFU) | (((drawX + (u32)rectW) & 0xFFFFU) << 16);
                                gePtr[10] = drawY + (u32)rectH;

                                if ((cmdWord & 0x01000000U) != 0U) {
                                    fillColor = 0x55808080U;
                                } else if ((g_pops->gpuTexDepth & 2U) != 0U) {
                                    fillColor &= g_pops->gpuTexColorMask;
                                }

                                clzX = (popsClz32((u32)g_pops->gpuTexWinMaskX) + 36972U) << 15;
                                clzY = (popsClz32((u32)g_pops->gpuTexWinMaskY) + 37484U) << 15;
                                gePtr[0] = 0x14000000U;
                                gePtr[1] = clzX;
                                gePtr[2] = clzY;
                                gePtr[3] = fillColor;
                                gePtr[4] = (0x53B7FD00U | (((cmdWord >> 25) & 3U) << 6) | (dirtyBits == 0U ? 8U : 0U)) - (u32)gePtr;

                                maskMode = (((g_pops->gpuStatusReg >> 11) & 3U) != 3U) ? 1 : 0;
                                if ((fillColor & 0x808080U) == 0x808080U) {
                                    maskMode = -1;
                                } else if ((g_pops->gpuTexDepth & 2U) != 0U
                                           && ((u8)(fillColor | (fillColor >> 8) | (fillColor >> 16))) >= 5U) {
                                    u16 tp = g_pops->gpuTexPageState;
                                    s32 texLeft = (s32)((8U * (tp & 0xFU)) + g_pops->gpuTexWinOffX);
                                    s32 texTop = (s32)((g_pops->gpuTexWinOffY & 0xDFU) | (((tp & 0x10U) != 0U) ? 32U : 0U));
                                    if (8 * (texLeft + (s32)g_pops->gpuTexWinMaskX) >= g_pops->gpuClipLeft
                                        && g_pops->gpuClipRight >= (8 * texLeft)
                                        && 8 * (texTop + (s32)g_pops->gpuTexWinMaskY) >= g_pops->gpuClipTop
                                        && g_pops->gpuClipBottom >= (8 * texTop)) {
                                        maskMode = -1;
                                    }
                                }
                                gePtr[5] = ((0x51B7FC80U | (((g_pops->gpuStatusReg & 0x800U) != 0U) ? 4U : 0U)) + (u32)(maskMode << 6)) - (u32)gePtr;
                                gePtr[11] = clzX - 0x40000U;
                                gePtr[12] = clzY - 0x40000U;
                                gePtr += 13;

                                if (g_pops->gpuInterlaceActive != 0U && (g_pops->quirkFlagsExt & 0x40000000) != 0) {
                                    gePtr[0] = 0x13041B91U;
                                    gePtr[1] = 0x0A000000U | (((g_pops->gpuStatusReg >> 11) & 3U) << 4);
                                    gePtr += 2;
                                }
                            }
                        } else {
                            /* Untextured rectangle */
                            if ((curTpage & 0x4000U) != 0U) {
                                u16 newTpage = (u16)(curTpage - 0x4000U);
                                g_pops->gpuTexPageState = newTpage;
                                gePtr[0] = 0x13041B90U;
                                gePtr[1] = 0x0A000080U | (((newTpage >> 5) & 3U) << 4);
                                gePtr += 2;
                            }
                            if (g_pops->gpuDrawInsideTex != 0U) {
                                popsGpuInvalidateTexRect(drawX, (u32)rectW, drawY, (u32)rectH, 1);
                            }
                            {
                                u16 *geHalf = (u16 *)gePtr;
                                gePtr[0] = 0x14000000U;
                                gePtr[1] = fillColor;
                                gePtr[2] = (0x51B7FD00U | (((cmdWord >> 25) & 3U) << 6)) - (u32)gePtr;
                                gePtr[3] = (drawX & 0xFFFFU) | ((drawY & 0xFFFFU) << 16);
                                geHalf[9] = (u16)(drawX + (u32)rectW);
                                geHalf[10] = (u16)(drawY + (u32)rectH);
                                gePtr += 6;
                            }
                        }
                    }
                }
                pktState = 0U;
                break;
            }

            case 4U: {
                /* GP0 VRAM-to-VRAM copy (0x80) */
                const u32 *pktWords = (const u32 *)curAddr;
                u32 srcWord = pktWords[1];
                u32 dstWord = pktWords[2];

                if ((g_pops->quirkFlags & 0x40U) != 0U
                    && (cmdWord != 0x80000000U || ((srcWord | dstWord) & 0xFE00FC00U) != 0U)) {
                    g_pops->gpuPacketState = 0U;
                    g_pops->gpuDlistCursor = (u32)gePtr;
                    return totalCost;
                }
                curAddr += 12U;
                if (srcWord == dstWord) {
                    pktState = 0U;
                    break;
                }

                g_pops->gpuXferStart.word = srcWord & 0x01FF03FFU;
                g_pops->gpuXferSize.word = dstWord & 0x01FF03FFU;
                {
                    u32 dimWord = *(const u32 *)curAddr;
                    u32 copyW = ((dimWord - 1U) & 0x3FFU) + 1U;
                    u32 copyH = (((dimWord >> 16) - 1U) & 0x1FFU) + 1U;
                    s32 shiftVal = g_pops->gpuVramCopyShift;
                    u32 copyArea = copyW * copyH;
                    u32 dstX = g_pops->gpuXferSize.wh.w;
                    u32 dstY = g_pops->gpuXferSize.wh.h;
                    u32 srcX = g_pops->gpuXferStart.xy.x;
                    u32 srcY = g_pops->gpuXferStart.xy.y;

                    if (shiftVal < 0) {
                        totalCost += (s32)(copyArea << (-shiftVal));
                    } else {
                        totalCost += (s32)(copyArea >> shiftVal);
                    }

                    if ((dstX - g_pops->gpuDispX) < g_pops->gpuDispWidth) {
                        if ((dstY - g_pops->gpuDispY) < g_pops->gpuDispHeight
                            || ((dstY + copyH - 1U) - g_pops->gpuDispY) < g_pops->gpuDispHeight) {
                            g_pops->gpuFrameDirty = 1;
                        }
                    }

                    {
                        u32 gpuStat = g_pops->gpuStatusReg;
                        s32 overlaps = ((s32)dstX - (s32)copyW < (s32)srcX
                            && (s32)srcX < (s32)(dstX + copyW)
                            && (s32)dstY - (s32)copyH < (s32)srcY
                            && (s32)srcY < (s32)(dstY + copyH)) ? 1 : 0;
                        s32 maxSpan = popsMaxS32(popsMaxS32((s32)dstX, (s32)srcX) + (s32)copyW,
                                                 (popsMaxS32((s32)dstY, (s32)srcY) + (s32)copyH) * 2);
                        s32 skipInvalidate = 0;

                        if (overlaps == 0 && maxSpan < 1025 && (gpuStat & 0x800U) == 0U) {
                            gePtr[0] = 0xEB000000U | ((srcY & 0x1FFU) << 10) | (srcX & 0x3FFU);
                            gePtr[1] = 0xEC000000U | ((dstY & 0x1FFU) << 10) | (dstX & 0x3FFU);
                            gePtr[2] = copyW + (copyH << 10) - 0x12000401U;
                            gePtr[3] = 0x13041B90U;
                            gePtr[4] = 0x0A000000U;
                            gePtr += 5;
                        } else if ((gpuStat & 0x800U) == 0U && copyH == 1U) {
                            gePtr[0] = 0xEB000000U | ((srcY & 0x1FFU) << 10) | (srcX & 0x3FFU);
                            gePtr[1] = 0xEC0D4000U;
                            gePtr[2] = copyW + (copyH << 10) - 0x12000401U;
                            gePtr[3] = 0x13041B90U;
                            gePtr[4] = 0x0A000000U;
                            gePtr += 5;
                            g_pops->gpuTexPageState |= 0xC000U;
                            gePtr[0] = 0xEB0D4000U;
                            gePtr[1] = 0xEC000000U | ((dstY & 0x1FFU) << 10) | (dstX & 0x3FFU);
                            gePtr[2] = copyW + (copyH << 10) - 0x12000401U;
                            gePtr[3] = 0x13041B90U;
                            gePtr[4] = 0x0A000000U;
                            gePtr += 5;
                        } else {
                            /* Overlapping or masked VRAM copy on CPU */
                            s32 dx = (s32)srcX - (s32)dstX;
                            s32 dy = (s32)srcY - (s32)dstY;
                            s32 stepX = 1;
                            s32 stepY = 1;
                            s32 xStart = (s32)dstX;
                            s32 xStop = (s32)(dstX + copyW);
                            s32 yStart = (s32)dstY;
                            s32 yStop = (s32)(dstY + copyH);
                            u16 maskBit = ((gpuStat & 0x800U) != 0U) ? 0x8000U : 0U;

                            if (dx < 0) {
                                xStop = (s32)dstX - 1;
                                xStart = (s32)(dstX + copyW - 1U);
                                g_pops->gpuXferSize.wh.w = (u16)xStart;
                                stepX = -1;
                            }
                            if (dy < 0) {
                                yStop = (s32)dstY - 1;
                                yStart = (s32)(dstY + copyH - 1U);
                                stepY = -1;
                            }
                            g_pops->gpuListState = 0;
                            gePtr++;
                            g_pops->gpuGeListId = sceMeAudio_D96DC042(g_pops->gpuGeListId, (void *)gePtr);
                            {
                                s32 curY = yStart;
                                do {
                                    s32 curX = g_pops->gpuXferSize.wh.w;
                                    do {
                                        u32 sAddr = POPS_VRAM_CACHED_BASE | ((((u32)(curY + dy)) & 0x1FFU) << 11) | ((((u32)(curX + dx)) & 0x3FFU) * 2U);
                                        u32 dAddr = POPS_VRAM_CACHED_BASE | ((((u32)curY) & 0x1FFU) << 11) | ((((u32)curX) & 0x3FFU) * 2U);
                                        *(volatile u16 *)dAddr = (u16)(*(const volatile u16 *)sAddr | maskBit);
                                        curX += stepX;
                                    } while (curX != xStop);
                                    curY += stepY;
                                } while (curY != yStop);
                            }
                            sceKernelDcacheWritebackInvalidateRange((const void *)POPS_VRAM_CACHED_BASE, 0x100000U);
                            skipInvalidate = 0;
                        }

                        if (skipInvalidate == 0 && copyW == 1U && copyH == 1U && (gpuStat & 0x800U) == 0U && overlaps == 0 && maxSpan < 1025) {
                            u32 pageIdx = (((dstY >> 8) & 1U) << 4) | ((dstX >> 6) & 0xFU);
                            PopsTexPageSlot *slot = &g_pops->texPages[pageIdx];
                            if (slot->loadedFormat != 0U) {
                                u32 slotAddr = slot->vramAddr;
                                gePtr[0] = 0xB5000100U | (((slotAddr >> 24) & 0xFU) << 16);
                                gePtr[1] = 0xB4000000U | (slotAddr & 0xFFFFFFU);
                                gePtr[2] = 0xEC000000U | ((dstY & 0xFFU) << 10) | (dstX & 0x3FU);
                                gePtr[3] = 0xEA000000U;
                                if (slot->blitCoord != 0 && pageIdx > 0U) {
                                    g_pops->texPages[pageIdx - 1U].loadedFormat = 0U;
                                }
                                gePtr += 4;
                            }
                            skipInvalidate = 1;
                        }

                        if (skipInvalidate == 0) {
                            popsGpuInvalidateTexRect(g_pops->gpuXferSize.wh.w, copyW, dstY, copyH, 1);
                        }
                    }
                }
                pktState = 0U;
                break;
            }

            case 5U: {
                /* GP0 CPU-to-VRAM transfer header (0xA0) */
                const u32 *pktWords = (const u32 *)curAddr;
                if ((g_pops->quirkFlags & 0x40U) != 0U
                    && (cmdWord != 0xA0000000U || ((pktWords[1] | pktWords[2]) & 0xFE00FC00U) != 0U)) {
                    g_pops->gpuPacketState = 0U;
                    g_pops->gpuDlistCursor = (u32)gePtr;
                    return totalCost;
                }
                g_pops->gpuXferStart.word = pktWords[1] & 0x01FF03FFU;
                g_pops->gpuXferSize.word = pktWords[2];
                curAddr += 8U;
                pktState = 9U;
                break;
            }

            case 6U: {
                /* GP0 VRAM-to-CPU transfer header (0xC0) */
                const u32 *pktWords = (const u32 *)curAddr;
                u32 dimWord = pktWords[2];
                g_pops->gpuXferStart.word = pktWords[1] & 0x01FF03FFU;
                g_pops->gpuXferSize.wh.w = (u16)(((dimWord - 1U) & 0x3FFU) + 1U);
                g_pops->gpuXferSize.wh.h = (u16)((((dimWord >> 16) - 1U) & 0x1FFU) + 1U);
                g_pops->gpuReadMode = 16U;
                g_pops->gpuPacketState = 0U;
                g_pops->gpuDlistCursor = (u32)gePtr;
                return totalCost;
            }

            case 7U: {
                /* GP0 Environment commands (0xE1..0xE6) */
                u32 envOp = (cmdWord >> 24) & 0x1FU;
                u32 envVal = cmdWord & 0x7FFU;
                pktState = 0U;
                if (envOp == 1U) {
                    u16 oldTpage = g_pops->gpuTexPageState;
                    if (((envVal ^ oldTpage) & 0xC1FFU) != 0U) {
                        envVal |= 0xC000U;
                    }
                    g_pops->gpuTexPageState = (u16)(envVal | (oldTpage & 0x2000U));
                    g_pops->gpuInterlaceHalf = ((envVal & 0x400U) == 0U) ? (g_pops->gpuInterlaceActive & 1U) : 0U;
                } else if (envOp == 2U) {
                    u32 winRaw = cmdWord & 0xFFFFFU;
                    if (g_pops->gpuTexWindowRaw != winRaw) {
                        u32 maskX = (cmdWord | 0x20U) & (u32)(-(s32)(cmdWord | 0x20U));
                        u32 maskY = ((cmdWord >> 5) | 0x20U) & (u32)(-(s32)((cmdWord >> 5) | 0x20U));
                        g_pops->gpuTexWindowRaw = winRaw;
                        g_pops->gpuTexWinOffX = (u8)((cmdWord >> 10) & (u32)(-(s8)maskX) & 0x1FU);
                        g_pops->gpuTexWinOffY = (u8)((cmdWord >> 15) & (u32)(-(s8)maskY) & 0x1FU);
                        g_pops->gpuTexPageState |= 0x8000U;
                        g_pops->gpuTexWinMaskX = (u8)maskX;
                        g_pops->gpuTexWinMaskY = (u8)maskY;
                    }
                } else if (envOp == 3U || envOp == 4U) {
                    u32 geOp;
                    if (envOp == 3U) {
                        if (g_pops->quirkFlagsExt < 0 && cmdWord == 0xE3000401U) {
                            cmdWord = 1U;
                        }
                        g_pops->gpuClipLeft = (s16)(cmdWord & 0x3FFU);
                        g_pops->gpuClipTop = (s16)((cmdWord >> 10) & 0x1FFU);
                        geOp = 0xD4000000U;
                    } else {
                        g_pops->gpuClipRight = (s16)(cmdWord & 0x3FFU);
                        g_pops->gpuClipBottom = (s16)((cmdWord >> 10) & 0x1FFU);
                        geOp = 0xD5000000U;
                    }
                    *gePtr++ = geOp | (cmdWord & 0x7FFFFU);
                    {
                        s32 dispY = (s32)g_pops->gpuDispY;
                        s32 dispH = (s32)g_pops->gpuDispHeight;
                        s32 dispX = (s32)g_pops->gpuDispX;
                        s32 dispW = (s32)g_pops->gpuDispWidth;
                        u8 drawInTex = (dispW < (g_pops->gpuClipRight - g_pops->gpuClipLeft)
                            || dispH < (g_pops->gpuClipBottom - g_pops->gpuClipTop)) ? 1U : 0U;
                        u8 overlaps = (g_pops->gpuClipRight >= dispX
                            && g_pops->gpuClipLeft < (dispX + dispW)
                            && ((g_pops->gpuClipBottom >= dispY && g_pops->gpuClipTop < (dispY + dispH))
                                || (g_pops->gpuClipBottom >= (dispY - 512) && g_pops->gpuClipTop < (dispY + dispH - 512)))) ? 1U : 0U;
                        if ((g_pops->quirkFlags & 1U) != 0U) {
                            drawInTex = 0U;
                        }
                        g_pops->gpuDrawInsideTex = drawInTex;
                        g_pops->gpuDrawOverlapsDisp = overlaps;
                    }
                } else if (envOp == 5U) {
                    s32 offX = popsSignExtend11(cmdWord);
                    s32 offY = ((s32)(cmdWord << 10)) >> 21;
                    if ((g_pops->gpuClipLeft - offX) >= 1024) {
                        offX += 2048;
                    }
                    gePtr[0] = 0x3A000009U;
                    gePtr[1] = popsGeEncodeFloatCoord(offX);
                    gePtr[2] = popsGeEncodeFloatCoord(offY);
                    gePtr += 3;
                    g_pops->gpuDrawOffX = (s16)offX;
                    g_pops->gpuDrawOffY = (s16)offY;
                } else if (envOp == 6U) {
                    g_pops->gpuStatusReg = (g_pops->gpuStatusReg & 0xFFFFE7FFU) | ((cmdWord & 3U) << 11);
                    gePtr[0] = 0x13041B91U;
                    gePtr[1] = 0x0A000000U | ((cmdWord & 3U) << 4);
                    gePtr += 2;
                }
                break;
            }

            case 9U: {
                /* GP0 CPU-to-VRAM payload setup & fast GE blit / slow CPU fallback */
                u32 dimWord = g_pops->gpuXferSize.word;
                u32 xferX = g_pops->gpuXferStart.xy.x;
                u32 xferY = g_pops->gpuXferStart.xy.y;
                u32 xferW = ((dimWord - 1U) & 0x3FFU) + 1U;
                u32 xferH = (((dimWord >> 16) - 1U) & 0x1FFU) + 1U;
                u32 wordsNeeded = ((xferW * xferH) + 1U) >> 1;
                u32 availBytes = endAddr - curAddr;
                u32 bytesNeeded = wordsNeeded * 4U;
                s32 isPartial = (availBytes < bytesNeeded) ? 1 : 0;

                if ((xferX - g_pops->gpuDispX) < g_pops->gpuDispWidth && (xferY - g_pops->gpuDispY) < g_pops->gpuDispHeight) {
                    g_pops->gpuFrameDirty = 1;
                }
                if (((xferX + xferW - 1U) - g_pops->gpuDispX) < g_pops->gpuDispWidth
                    && ((xferY + xferH - 1U) - g_pops->gpuDispY) < g_pops->gpuDispHeight) {
                    g_pops->gpuFrameDirty = 1;
                }

                if ((isPartial != 0 && xferH != 1U)
                    || (g_pops->gpuStatusReg & 0x800U) != 0U
                    || popsMaxS32((s32)(xferW + ((curAddr >> 1) & 7U)), (s32)(xferX + xferW)) >= 1025
                    || (g_pops->quirkFlagsExt & 0x800) != 0) {
                    /* Slow word-by-word CPU write to uncached VRAM (transitions to state 10) */
                    g_pops->gpuXferSize.wh.w = (u16)(xferX + xferW);
                    g_pops->gpuXferSize.wh.h = (u16)(xferY + xferH);
                    popsGpuInvalidateTexRect(xferX, xferW, xferY, xferH, 1);
                    g_pops->gpuXferCur.word = g_pops->gpuXferStart.word;
                    gePtr++;
                    g_pops->gpuGeListId = sceMeAudio_D96DC042(g_pops->gpuGeListId, (void *)gePtr);
                    g_pops->gpuListState = 0;
                    pktState = 10U;
                    /* Write first word right away */
                    {
                        u32 curY = g_pops->gpuXferCur.xy.y;
                        u32 curX = g_pops->gpuXferCur.xy.x;
                        u16 maskBit = (u16)(((g_pops->gpuStatusReg >> 11) & 3U) << 15);
                        u32 limitX = g_pops->gpuXferSize.wh.w;
                        u32 limitY = g_pops->gpuXferSize.wh.h;
                        u32 done = 0U;

                        *(volatile u16 *)(POPS_VRAM_UNCACHED_BASE | ((curY & 0x1FFU) << 11) | ((curX & 0x3FFU) * 2U)) = (u16)(cmdWord | maskBit);
                        curX++;
                        if (curX == limitX) {
                            curY++;
                            g_pops->gpuXferCur.xy.y = (u16)curY;
                            if (curY == limitY) {
                                done = 1U;
                            } else {
                                curX = g_pops->gpuXferStart.xy.x;
                            }
                        }
                        if (done == 0U) {
                            *(volatile u16 *)(POPS_VRAM_UNCACHED_BASE | ((curY & 0x1FFU) << 11) | ((curX & 0x3FFU) * 2U)) = (u16)((cmdWord >> 16) | maskBit);
                            curX++;
                            if (curX == limitX) {
                                curY++;
                                g_pops->gpuXferCur.xy.y = (u16)curY;
                                if (curY == limitY) {
                                    done = 1U;
                                } else {
                                    curX = g_pops->gpuXferStart.xy.x;
                                }
                            }
                        }
                        if (done != 0U) {
                            pktState = 0U;
                        } else {
                            g_pops->gpuXferCur.xy.x = (u16)curX;
                        }
                    }
                    break;
                }

                /* Fast GE DMA transfer to VRAM */
                g_pops->gpuListState = 1;
                if (isPartial != 0) {
                    wordsNeeded = availBytes >> 2;
                    g_pops->gpuXferSize.wh.w = (u16)(xferW - (wordsNeeded * 2U));
                    xferW = wordsNeeded * 2U;
                    bytesNeeded = wordsNeeded * 4U;
                }
                totalCost += (s32)wordsNeeded;

                if ((bytesNeeded >> 17) != 0U) {
                    sceKernelDcacheWritebackRange((const void *)curAddr, bytesNeeded);
                } else {
                    u32 lastByte = curAddr + bytesNeeded - 1U;
                    u32 lineAddr = (curAddr & 0xFFFFFFC0U) | (lastByte & 0x3FU);
                    for (;;) {
                        popsCacheDataHitWb((const void *)lineAddr);
                        if (lineAddr == lastByte) {
                            break;
                        }
                        lineAddr += 64U;
                    }
                }

                {
                    u32 alignPix = (curAddr >> 1) & 7U;
                    if (isPartial == 0 && (xferW & 7U) == 0U && (xferY + xferH) < 513U && (xferW + alignPix) < 1025U) {
                        popsGpuInvalidateTexRect(g_pops->gpuXferStart.xy.x, xferW, g_pops->gpuXferStart.xy.y, xferH, 1);
                        gePtr[0] = 0xB2000000U | (curAddr & 0xFFFFFFU);
                        gePtr[1] = 0xB3000000U | (((curAddr >> 24) & 0xFU) << 16) | xferW;
                        gePtr[2] = 0xEB000000U | alignPix;
                        gePtr[3] = 0xEC000000U | ((g_pops->gpuXferStart.xy.y & 0x1FFU) << 10) | (g_pops->gpuXferStart.xy.x & 0x3FFU);
                        gePtr[4] = xferW + (xferH << 10) - 0x12000401U;
                        gePtr[5] = 0x13041B90U;
                        gePtr[6] = 0x0A000008U;
                        gePtr += 7;
                        curAddr += (wordsNeeded - 1U) * 4U;
                        pktState = 0U;
                    } else {
                        u32 startY = g_pops->gpuXferStart.xy.y;
                        u32 startX = g_pops->gpuXferStart.xy.x;
                        u32 srcAddrCmd = 0xB2000000U | (curAddr & 0xFFFFFFU);
                        u32 srcCoord = 0xEB000000U | ((srcAddrCmd >> 1) & 7U);
                        u32 dstCoord = 0xEC000000U | ((startY & 0x1FFU) << 10) | (startX & 0x3FFU);
                        u32 rowsLeft = xferH;

                        gePtr[0] = 0xB4000000U;
                        gePtr[1] = 0xB5040400U;
                        gePtr[2] = 0xB3000400U | (((curAddr >> 24) & 0xFU) << 16);
                        gePtr[3] = xferW - 0x12000001U;
                        gePtr[4] = srcAddrCmd;
                        gePtr += 5;

                        do {
                            gePtr[0] = dstCoord;
                            if (((srcCoord & 0x3FFU) + xferW) >= 1024U) {
                                srcAddrCmd += (srcCoord & 0x3F8U) * 2U;
                                gePtr[1] = srcAddrCmd;
                                srcCoord -= (srcCoord & 0x3F8U);
                                gePtr += 4;
                            } else {
                                gePtr += 3;
                            }
                            gePtr[-2] = srcCoord;
                            gePtr[-1] = 0xEA000000U;
                            dstCoord = (dstCoord + 1024U) & 0xFF07FFFFU;
                            srcCoord += xferW;
                            rowsLeft--;
                        } while (rowsLeft != 0U);

                        if ((g_pops->quirkFlags & 4U) != 0U
                            && ((((startX + xferW - 1U) ^ startX) & 0xFFFFFFC0U)
                                | (((startY + xferH - 1U) ^ startY) & 0xFFFFFF00U)) == 0U) {
                            u32 pageIdx = (((startY >> 8) & 1U) << 4) | ((startX >> 6) & 0xFU);
                            PopsTexPageSlot *slot = &g_pops->texPages[pageIdx];
                            if (slot->loadedFormat != 0U) {
                                u32 slotAddr = slot->vramAddr;
                                gePtr[0] = 0xEB000000U | ((startY & 0x1FFU) << 10) | (startX & 0x3FFU);
                                gePtr[1] = 0xB5000100U | (((slotAddr >> 24) & 0xFU) << 16);
                                gePtr[2] = 0xB4000000U | (slotAddr & 0xFFFFFFU);
                                gePtr[3] = 0xEC000000U | ((startY & 0xFFU) << 10) | (startX & 0x3FU);
                                gePtr[4] = ((xferW & 0xFFF003FFU) | ((xferH & 0x3FFU) << 10)) - 0x12000401U;
                                gePtr[5] = 0xCB000000U;
                                gePtr[6] = 0x13041B90U;
                                gePtr[7] = 0x0A0000C8U;
                                if (slot->blitCoord != 0 && pageIdx > 0U) {
                                    g_pops->texPages[pageIdx - 1U].loadedFormat = 0U;
                                }
                                gePtr += 8;
                            }
                        } else {
                            popsGpuInvalidateTexRect(startX, xferW, startY, xferH, 1);
                        }

                        curAddr += (wordsNeeded - 1U) * 4U;
                        if (isPartial != 0) {
                            g_pops->gpuXferStart.xy.x = (u16)(g_pops->gpuXferStart.xy.x + xferW);
                            g_pops->gpuPacketState = (u8)pktState;
                            g_pops->gpuDlistCursor = (u32)gePtr;
                            return totalCost;
                        }
                        pktState = 0U;
                    }
                }
                break;
            }

            default: {
                /* State 10: continuing word-by-word CPU-to-VRAM write */
                u32 curY = g_pops->gpuXferCur.xy.y;
                u32 curX = g_pops->gpuXferCur.xy.x;
                u16 maskBit = (u16)(((g_pops->gpuStatusReg >> 11) & 3U) << 15);
                u32 limitX = g_pops->gpuXferSize.wh.w;
                u32 limitY = g_pops->gpuXferSize.wh.h;
                u32 done = 0U;

                *(volatile u16 *)(POPS_VRAM_UNCACHED_BASE | ((curY & 0x1FFU) << 11) | ((curX & 0x3FFU) * 2U)) = (u16)(cmdWord | maskBit);
                curX++;
                if (curX == limitX) {
                    curY++;
                    g_pops->gpuXferCur.xy.y = (u16)curY;
                    if (curY == limitY) {
                        done = 1U;
                    } else {
                        curX = g_pops->gpuXferStart.xy.x;
                    }
                }
                if (done == 0U) {
                    *(volatile u16 *)(POPS_VRAM_UNCACHED_BASE | ((curY & 0x1FFU) << 11) | ((curX & 0x3FFU) * 2U)) = (u16)((cmdWord >> 16) | maskBit);
                    curX++;
                    if (curX == limitX) {
                        curY++;
                        g_pops->gpuXferCur.xy.y = (u16)curY;
                        if (curY == limitY) {
                            done = 1U;
                        } else {
                            curX = g_pops->gpuXferStart.xy.x;
                        }
                    }
                }
                if (done != 0U) {
                    pktState = 0U;
                } else {
                    g_pops->gpuXferCur.xy.x = (u16)curX;
                }
                break;
            }
        }

        curAddr += 4U;
    }

    g_pops->gpuPacketState = (u8)pktState;
    g_pops->gpuDlistCursor = (u32)gePtr;
    return totalCost;
}

/* [160] 0x014df0 */
__attribute__((noinline))
void popsGpuScheduleNextVblank(s32 cycles)
{
    g_pops->gpuVblankEvent.callback = popsGpuVblankStartCallback;
    popsEventSchedule(&g_pops->gpuVblankEvent, cycles);

    g_pops->cycleRemaining -= g_pops->deferredCycles;
    g_pops->deferredCycles = 0;

    if (g_pops->gpuDispFlags.bytes.flippedThisVblank == 0U) {
        if ((g_pops->gpuDispFlags.word & 0x80000U) == 0U || (g_pops->quirkFlags & 8U) == 0U) {
            popsGpuFlipDisplay();
        }
    }

    g_pops->gpuDispFlags.bytes.flippedThisVblank = 0U;
    g_pops->gpuLastDisplayPos = (u32)g_pops->gpuDispX + ((u32)g_pops->gpuDispY << 10);
}

/* [161] 0x014e84 */
__attribute__((noinline))
void popsGpuVblankPostCallback(PopsEvent *ev)
{
    (void)ev;
    popsGpuScheduleNextVblank(g_pops->gpuVblankRemainder);
}
