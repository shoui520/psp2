#include "pops_internal.h"

/* Fixed PSISOIMG0000 header & index memory layout at 0x09E80000 */
#define POPS_ISO_HDR_ADDR          0x09E80000U
#define POPS_ISO_TITLE_ID_ADDR     0x09E80400U
#define POPS_ISO_QUIRK_VER_ADDR    0x09E80420U
#define POPS_ISO_QUIRK_MIN_ADDR    0x09E80424U
#define POPS_ISO_QUIRK_BLOCK_ADDR  0x09E80428U
#define POPS_ISO_TOC_FIRST_TRK     (*(const volatile u8 *)0x09E80807U)
#define POPS_ISO_TOC_LAST_TRK      (*(const volatile u8 *)0x09E80811U)
#define POPS_ISO_TOC_LEADOUT       ((const u8 *)0x09E8081BU)
#define POPS_ISO_TOC_TRK2          ((const u8 *)0x09E8082FU)
#define POPS_ISO_DATA_OFF          (*(const volatile u32 *)0x09E80BFCU)
#define POPS_ISO_CDDA_TBL          ((volatile u32 *)0x09E80BE0U)
#define POPS_ISO_SAVEDATA_FLAG     (*(const volatile u32 *)0x09E812ACU)
#define POPS_ISO_SAVEDATA_TITLE    ((const char *)0x09E812ACU)
#define POPS_ISO_SUBCH_FLAG        (*(const volatile u32 *)0x09E812D4U)
#define POPS_ISO_SUBCH_COUNT       (*(const volatile u32 *)0x09E812D8U)
#define POPS_ISO_CUSTOM_CFG        ((const u8 *)0x09E8135CU)
#define POPS_ISO_INDEX_ADDR        0x09E84000U

/* VRAM / GE workspace addresses initialized in popsInitGpu */
#define POPS_VRAM_DLIST_BASE       0x49B7B800U
#define POPS_VRAM_SUBLIST_BASE     0x041B9200U
#define POPS_VRAM_GE_CONSTS        0x441B9000U
#define POPS_VRAM_BLIT_VERTS       0x041BB600U
#define POPS_CD_CACHE_POOL_BASE    0x09492600U

typedef struct PopsCustomConfigBuf {
    u16 headerTag;
    u16 bodyTag;
    u32 configId;
    u8 modeByte;
    u8 payload[231];
} PopsCustomConfigBuf;

POPS_STATIC_ASSERT(sizeof(PopsCustomConfigBuf) == 240, custom_cfg_size);

static u8 g_savedataConfigBuf[1024];
static u32 g_titleHeaderCopy[32];
static u32 g_iconChecksumKey;
static PopsCustomConfigBuf g_customConfig;
static u32 g_dispModeShadow[4];

/* Linker symbol marking the end of the PRX .text section (syscall stubs end) */
extern u32 _popsTextEnd[] __asm__("_etext");
extern u32 _popsTextStart[] __asm__("popsJitEnter");

/* 0x01627c: Mark disc-change ready flag */
void popsCdMarkDiscChangeReady(void)
{
    g_pops->cdDiscChangeReady = 1U;
}

/* 0x016288: Query disc-change ready flag */
u32 popsCdGetDiscChangeReady(void)
{
    return g_pops->cdDiscChangeReady;
}

/* 0x016290: Apply fast/normal disc load speed setting */
void popsCdSetLoadSpeed(s32 fastMode)
{
    u32 speed = 0U;
    if (fastMode != 0) {
        speed = *(const volatile u32 *)&g_pops->cdSpeedScale;
    }
    g_pops->cdSpeedQuirkByte = (u8)speed;
}

/* 0x0162a0: Zero CD-ROM scratchpad area and restore initial audio volume & subchannel table */
void popsCdInitState(void)
{
    u8 savedSpeed = g_pops->cdSpeedQuirkByte;
    u32 *clearPtr = (u32 *)(void *)&g_pops->cdCmdEvent;
    u32 wordsRem = (14528U - 14336U) / 4U;

    while (wordsRem-- != 0U) {
        *clearPtr++ = 0U;
    }

    g_pops->cdSpeedQuirkByte = savedSpeed;
    g_pops->cdAudioVol.bytes[0] = 0x80U;
    g_pops->cdAudioVol.bytes[2] = 0x80U;
    g_pops->cdMuted = 1U;

    if (g_subchannelAllocPtr != 0U) {
        g_pops->subchannelTable = (u8 *)(uintptr_t)(g_subchannelAllocPtr + 12U);
    }
}

/* 0x0162ec: Lid-open timer completion callback (tray now ready for Getstat) */
void popsCdLidOpenCallback(PopsEvent *ev)
{
    (void)ev;
    g_pops->cdTrayState = 2U;
}

/* 0x0162f8: Validate 9-character PlayStation title ID (4 uppercase letters + 5 digits) */
s32 popsValidateTitleId(const char *titleId)
{
    s32 idx;

    for (idx = 0; idx < 4; idx++) {
        if ((u8)(titleId[idx] - 'A') >= 26U) {
            return -1;
        }
    }
    for (idx = 4; idx < 9; idx++) {
        if ((u8)(titleId[idx] - '0') >= 10U) {
            return -1;
        }
    }
    return 0;
}

/* 0x01636c: Load selected PSISOIMG0000 disc header, decrypt index, and apply game quirks */
s32 popsDiscSelectImage(void)
{
    u32 wantDisc = popsGpuGetScreenMode();
    u32 discBase;
    u32 dataStart;
    u32 idx;
    char *titleId;
    s32 lastChunk;
    u32 trackIdx;

    if (g_pops->discActiveIndex == wantDisc) {
        return 0;
    }
    if (wantDisc >= g_pops->discCount) {
        scePopsMan_0090B2C8((s32)0x80010089U);
        return -1;
    }

    popsDiscWaitWorkerIdle();
    for (idx = 0U; idx < 17U; idx++) {
        g_pops->discCache[idx].baseLba = 0x80000000U;
    }

    g_pops->discLastReadOff = -1;
    discBase = g_pops->discPbpBaseOff + g_pops->discOffsets[wantDisc];
    if (sceMeAudio_A651EA7B(g_pops->discFileFd, (void *)(uintptr_t)POPS_ISO_HDR_ADDR, discBase, 1024U) != 1024) {
        return -1;
    }
    if (scePaf_9CD6C5F4((const char *)(uintptr_t)POPS_ISO_HDR_ADDR, "PSISOIMG0000", 12U) != 0) {
        return -1;
    }
    if (popsLatchDiscLicense(sceMeAudio_F1502A62((void *)(uintptr_t)POPS_ISO_HDR_ADDR, wantDisc)) < 0) {
        return -1;
    }

    titleId = popsStripTitleSeparators((char *)(uintptr_t)POPS_ISO_TITLE_ID_ADDR, 32);
    if (popsValidateTitleId(titleId) != 0 ||
        popsLoadGameQuirks(titleId,
                           *(const volatile u32 *)(uintptr_t)POPS_ISO_QUIRK_VER_ADDR,
                           *(const volatile u32 *)(uintptr_t)POPS_ISO_QUIRK_MIN_ADDR,
                           (const u8 *)(uintptr_t)POPS_ISO_QUIRK_BLOCK_ADDR) < 0) {
        return -1;
    }

    g_pops->discActiveIndex = (u8)wantDisc;
    g_pops->cdCacheQuirk = (u32)(popsMsfToLba(POPS_ISO_TOC_LEADOUT) - 1);

    if (g_subchannelAllocPtr != 0U) {
        scePaf_71B92320((void *)(uintptr_t)g_subchannelAllocPtr);
        g_pops->subchannelTable = NULL;
        g_subchannelAllocPtr = 0U;
    }

    if (POPS_ISO_SUBCH_FLAG != 0U) {
        g_subchannelAllocPtr = (u32)(uintptr_t)scePaf_0FCDFA1E(12U * POPS_ISO_SUBCH_COUNT);
        if (g_subchannelAllocPtr == 0U) {
            return -1;
        }
        if (sceMeAudio_83609AC9((void *)(uintptr_t)POPS_ISO_HDR_ADDR, wantDisc) < 0) {
            return -1;
        }
        g_pops->subchannelTable = (u8 *)(uintptr_t)(g_subchannelAllocPtr + 12U);
    }

    dataStart = discBase + POPS_ISO_DATA_OFF;
    if ((g_pops->quirkFlags & 0x2000U) != 0U ||
        ((POPS_ISO_TOC_LAST_TRK - 6U * (POPS_ISO_TOC_LAST_TRK >> 4)) -
         (POPS_ISO_TOC_FIRST_TRK - 6U * (POPS_ISO_TOC_FIRST_TRK >> 4)) + 1U) < 2U) {
        lastChunk = (s32)((g_pops->cdCacheQuirk - 1U) >> 4);
    } else {
        lastChunk = (s32)((u32)popsMsfToLba(POPS_ISO_TOC_TRK2) >> 4);
    }

    while (lastChunk >= 0) {
        volatile u32 *entry = (volatile u32 *)(uintptr_t)(POPS_ISO_INDEX_ADDR + 32U * (u32)lastChunk);
        u32 compSize = entry[1] & 0xFFFFU;
        u32 absOff = entry[0] + dataStart;
        entry[0] = absOff;
        if (compSize != 0U) {
            compSize = (compSize + (absOff & 0x1FFU) + 511U) & ~0x1FFU;
        }
        entry[6] = compSize;
        lastChunk--;
    }

    if (POPS_ISO_CDDA_TBL[9] != 0U) {
        trackIdx = 2U;
        do {
            volatile u32 *cddaEntry = &POPS_ISO_CDDA_TBL[4U * trackIdx];
            u32 absStart = cddaEntry[0] + dataStart;
            cddaEntry[1] += absStart;
            cddaEntry[0] = absStart;
            trackIdx++;
        } while (POPS_ISO_CDDA_TBL[4U * trackIdx + 1U] != 0U);
    }

    return 0;
}

/* 0x016648: Mount single-disc or multi-disc PBP container and start cdworker thread */
s32 popsDiscMountPbp(void)
{
    u32 headerBuf[256];
    u32 idx;
    u32 bufAddr;
    s32 rc;
    const char *saveTitle;
    const char *saveGameId;
    const u8 *customCfg;
    u32 saveFlag;
    SceUID workerThid;

    g_subchannelAllocPtr = 0U;
    g_pops->discCount = 1U;
    popsSetDiscCountShadow(1U);
    g_pops->discActiveIndex = 0xFFU;
    g_pops->discPendingLba = -1;

    /* Initialize circular 17-entry disc cache list */
    g_pops->discCacheHead = &g_pops->discCache[0];
    g_pops->discCache[0].next = &g_pops->discCache[0];
    g_pops->discCache[0].prev = &g_pops->discCache[0];
    bufAddr = POPS_CD_CACHE_POOL_BASE;

    for (idx = 0U; idx < 17U; idx++) {
        PopsDiscCacheSlot *slot = &g_pops->discCache[idx];
        slot->bufAddr = bufAddr;
        bufAddr += 37632U;
        if (idx != 0U) {
            PopsDiscCacheSlot *tail = g_pops->discCache[0].prev;
            slot->prev = tail;
            slot->next = tail->next;
            slot->next->prev = slot;
            tail->next = slot;
        }
    }

    if (popsDiscOpenImageFd() < 0) {
        return g_pops->discFileFd;
    }

    rc = popsLoadPbpIconPng(g_pops->discFileFd);
    if (rc < 0) {
        return rc;
    }

    if (sceMeAudio_A651EA7B(g_pops->discFileFd, headerBuf, g_pops->discPbpBaseOff, 1024U) != 1024) {
        return (s32)0x80000004U;
    }

    if (scePaf_9CD6C5F4((const char *)headerBuf, "PSTITLEIMG000000", 16U) == 0) {
        u32 prevOff = 0U;
        u32 discIdx;
        char *titleIdBuf = (char *)&headerBuf[153];
        u32 *hdrCopySrc = &headerBuf[163];
        u32 *titleTextSrc = &headerBuf[195];

        if (popsLatchDiscLicense(sceMeAudio_34F2548F(headerBuf)) < 0) {
            return -1;
        }
        for (discIdx = 0U; discIdx < 5U; discIdx++) {
            u32 off = headerBuf[128U + discIdx];
            if (off < prevOff) {
                break;
            }
            prevOff = off;
            g_pops->discOffsets[discIdx] = off;
        }
        g_pops->discCount = (u8)discIdx;
        popsSetDiscCountShadow((u8)discIdx);
        popsStripTitleSeparators(titleIdBuf, 32);
        popsCopyTitleHeaderBlock(hdrCopySrc);

        saveFlag = titleTextSrc[32];
        rc = popsLoadGameQuirks(titleIdBuf, 0U, 0U, NULL);
        if (rc < 0) {
            return rc;
        }
        saveTitle = (const char *)titleTextSrc;
        saveGameId = titleIdBuf;
        customCfg = (const u8 *)&headerBuf[228];
    } else if (scePaf_9CD6C5F4((const char *)headerBuf, "PSISOIMG0000", 12U) == 0) {
        rc = popsDiscSelectImage();
        if (rc < 0) {
            return rc;
        }
        saveFlag = POPS_ISO_SAVEDATA_FLAG;
        saveTitle = POPS_ISO_SAVEDATA_TITLE;
        saveGameId = (const char *)(uintptr_t)POPS_ISO_TITLE_ID_ADDR;
        customCfg = POPS_ISO_CUSTOM_CFG;
    } else {
        return -1;
    }

    popsSavedataInitContext(saveTitle, saveGameId, saveFlag);
    popsInitCustomScreenConfig(saveTitle, saveGameId, customCfg);

    g_pops->discEventFlag = sceKernelCreateEventFlag("cdread", 0x200U, 0, NULL);
    workerThid = sceKernelCreateThread("cdworker", popsDiscWorkerThread, 22, 6144, 0x00100000U, NULL);
    return sceKernelStartThread(workerThid, 0, NULL);
}

/* 0x0168b0: Open (or reopen) the underlying PBP disc image handle via ME audio driver */
SceUID popsDiscOpenImageFd(void)
{
    if (g_pops->discFileFd > 0) {
        sceIoClose(g_pops->discFileFd);
    }
    g_pops->discFileFd = sceMeAudio_A8176E49(&g_pops->discPbpBaseOff);
    return g_pops->discFileFd;
}

/* 0x0168e4: Return number of discs in the loaded PBP */
u32 popsDiscGetCount(void)
{
    return g_pops->discCount;
}

/* 0x0168ec: Perform multi-disc swap and trigger PS1 lid-open sequence */
s32 popsDiscSwapAndReload(void)
{
    while (popsDiscSelectImage() < 0) {
        popsHandleDiscError(0);
    }
    popsCdStartDiscSwap();
    return 0;
}

/* 0x01692c: Return non-zero if multi-disc swap is currently permitted */
s32 popsDiscCanSwap(void)
{
    if (g_pops->discCount >= 2U) {
        return (popsCdGetDiscChangeReady() != 0U) ? 1 : 0;
    }
    return 0;
}

/* 0x016960: Strip '_' and '-' characters in-place from a PlayStation title ID */
char *popsStripTitleSeparators(char *str, s32 maxLen)
{
    const char *src = str;
    s32 outPos = 0;

    while (outPos < maxLen) {
        char ch = *src++;
        if (ch != '_' && ch != '-') {
            str[outPos++] = ch;
            if (ch == '\0') {
                break;
            }
        }
    }
    return str;
}

/* 0x0169bc: Initialize MDEC DMA0/DMA1, MMIO handlers, and VFPU weights */
void popsInitMdec(void)
{
    popsRegisterDmaChannel(0U, popsMdecDmaIn);
    popsRegisterDmaChannel(1U, popsMdecDmaOut);
    popsRegisterIoRange(0x1F801820U, 8U, popsMdecStatusRead, popsMdecControlWrite);
    sceKernelMemcpy(g_pops->mdecVfpuConstBlock, g_mdecInitWeights, 28U);
    g_pops->mdecEvent.callback = (void (*)(PopsEvent *))popsMdecDecodeVfpu;
}

/* 0x016a30: Apply per-game GPU DMA cycle shift and GE dithering matrix */
void popsGpuApplyClocks(void)
{
    s32 dmaShift = (s32)g_pops->gpuDmaCycleShift;
    volatile u32 *ditherRegs = (volatile u32 *)(uintptr_t)(POPS_VRAM_SUBLIST_BASE + 0x104U);

    if (dmaShift <= 0xFFFF) {
        s32 nextVal = dmaShift + 1;
        s32 negPart = 0;
        if (nextVal < 0) {
            negPart = -nextVal;
            nextVal = 0;
        }
        g_pops->gpuDmaCycleShift = (u32)((nextVal << 8) + negPart + 0x10000);
    }

    if (g_pops->gpuClockDivisor == 0U) {
        ditherRegs[0] = 0xE2000F01U;
        ditherRegs[1] = 0xE3000100U;
        ditherRegs[2] = 0xE400000FU;
        ditherRegs[3] = 0xE500F001U;
    } else if (g_pops->gpuClockDivisor == 1U) {
        ditherRegs[0] = 0xE200D9C8U;
        ditherRegs[1] = 0xE300BFAEU;
        ditherRegs[2] = 0xE400C8D9U;
        ditherRegs[3] = 0xE500AEBFU;
    }
}

/* 0x016b0c: Set PAL/NTSC display mode flag */
void popsGpuSetPalMode(s32 mode)
{
    if (mode < 0) {
        mode = (s32)((g_pops->gpuDispFlags.word >> 21) & 1U);
    }
    g_pops->gpuPalMode = (u8)mode;
}

/* 0x016b24: Recompute custom screen scaling table parameters */
void popsGpuBuildDispAdjust(s32 xShift, s8 yShift, s32 zoomDelta, s32 heightMode)
{
    u32 idx;
    u32 scaledHeight = (240U * (u32)heightMode + 9616U) >> 5;
    s32 baseTop = (s32)((272U - scaledHeight) >> 1) + (s32)yShift;
    s32 adjY = (s32)scaledHeight + (s32)(s8)baseTop - 300;
    u32 scaleZoom = (u32)(zoomDelta + 36);

    g_dispAdjustBytes[3] = (u8)scaleZoom;
    for (idx = 0U; idx < 5U; idx++) {
        u32 wideVal = (u32)g_gpuDispModeTable[idx][0] * 10U * scaleZoom + 160U;
        g_gpuDispModeTable[idx][4] = (u8)(wideVal / 320U);
    }

    if (adjY < -128) {
        adjY = -128;
    }
    if (adjY > 127) {
        adjY = 127;
    }
    if (xShift < -128) {
        xShift = -128;
    }
    if (xShift > 127) {
        xShift = 127;
    }

    g_dispAdjustBytes[0] = (u8)xShift;
    g_dispAdjustBytes[1] = (u8)baseTop;
    g_dispAdjustBytes[2] = (u8)adjY;
    g_dispAdjustBytes[4] = (u8)(((scaledHeight << 8) / 240U) - 44U);
    g_dispAdjustBytes[5] = (u8)(((scaledHeight << 8) / 288U) - 44U);
}

/* 0x016c28: Mirror disc count into VSH shared memory */
void popsSetDiscCountShadow(u8 count)
{
    POPS_SHARED_MEM->discCountShadow = count;
}

/* 0x016c34: Terminate active GE display list and wait for draw sync */
s32 popsGpuFlushGeList(void)
{
    volatile u32 *dlist = (volatile u32 *)(uintptr_t)g_pops->gpuDlistCursor;
    dlist[0] = 0x0F000000U;
    dlist[1] = 0x0C000000U;
    g_pops->gpuListState = 0;
    sceGeListUpdateStallAddr(g_pops->gpuGeListId, NULL);
    return sceGeDrawSync(0);
}

/* 0x016c74: Enqueue stalled GE display list at current cursor */
s32 popsGpuRestartGeList(void)
{
    void *cursor = (void *)(uintptr_t)g_pops->gpuDlistCursor;
    g_pops->gpuModeDirty = 2U;
    g_pops->gpuGeListId = sceGeListEnQueue(cursor, cursor, -1, NULL);
    return g_pops->gpuGeListId;
}

/* 0x016ca8: Initialize GPU VRAM texture pages, GE sublists, shared display block, and DMA2 */
void popsInitGpu(void)
{
    if (g_pops->texPages[0].vramAddr == 0U) {
        u32 texAddr = (u32)(uintptr_t)scePaf_0C2CD696(1024U, 655360U);
        u32 bank;
        u32 page;
        u32 sub;
        u32 dst;
        u32 idx;
        volatile u16 *blitVerts;
        u8 savedScreenMode;
        u8 savedCustomFlag;
        u8 savedCustomState;
        u8 savedDiscCount;
        u32 savedParamA;
        u32 savedParamB;

        for (bank = 0U; bank < 2U; bank++) {
            for (page = 0U; page < 15U; page += 3U) {
                u32 curAddr = texAddr;
                for (sub = 0U; sub < 3U; sub++) {
                    PopsTexPageSlot *slot = &g_pops->texPages[16U * bank + page + sub];
                    slot->vramAddr = curAddr;
                    slot->loadedFormat = 0U;
                    slot->groupOffset = (s8)sub;
                    slot->blitCoord = (sub == 0U) ? (s16)(page << 6) : 0;
                    curAddr += 128U;
                }
                texAddr += 0x20000U;
            }
            {
                PopsTexPageSlot *last = &g_pops->texPages[16U * bank + 15U];
                last->vramAddr = g_pops->texPages[16U * bank + 14U].vramAddr + 128U;
                last->loadedFormat = 0U;
                last->groupOffset = 3;
                last->blitCoord = 0;
            }
            texAddr = 0x04100000U;
        }

        /* Auxiliary scratch texture pages in VRAM */
        texAddr = 0x041BC000U;
        for (page = 0U; page < 6U; page += 3U) {
            u32 curAddr = texAddr;
            for (sub = 0U; sub < 3U; sub++) {
                PopsTexPageSlot *slot = &g_pops->texPages[32U + page + sub];
                slot->vramAddr = curAddr;
                slot->loadedFormat = 0U;
                slot->groupOffset = (s8)sub;
                slot->blitCoord = (sub == 0U) ? (s16)(page << 6) : 0;
                curAddr += 128U;
            }
            texAddr += 0x20000U;
        }

        g_pops->gpuFirstFlipDone = 0;
        sceGeEdramSetAddrTranslation(512);

        /* Upload prebuilt GE sublists into uncached EDRAM/main-RAM staging blocks */
        dst = POPS_VRAM_DLIST_BASE;
        for (idx = 0U; idx < 16U; idx++) {
            sceKernelMemcpy((void *)(uintptr_t)dst, g_gpuGeDrawTableA[idx], 64U);
            dst += 64U;
        }

        dst = POPS_VRAM_DLIST_BASE + 0x600U;
        for (idx = 0U; idx < 8U; idx++) {
            if (g_gpuGeDrawTableD[idx] != NULL) {
                sceKernelMemcpy((void *)(uintptr_t)dst, g_gpuGeDrawTableD[idx], 64U);
            }
            dst += 64U;
        }

        dst = POPS_VRAM_DLIST_BASE + 0x500U;
        for (idx = 0U; idx < 4U; idx++) {
            sceKernelMemcpy((void *)(uintptr_t)dst, g_gpuGeDrawTableB[idx], 64U);
            dst += 64U;
        }

        dst = POPS_VRAM_DLIST_BASE + 0x440U;
        for (idx = 0U; idx < 3U; idx++) {
            sceKernelMemcpy((void *)(uintptr_t)dst, g_gpuGeDrawTableC[idx], 64U);
            dst += 64U;
        }

        sceKernelMemcpy((void *)(uintptr_t)(POPS_VRAM_DLIST_BASE + 0x400U), &g_gpuGeSubLists[0], 24U);
        sceKernelMemcpy((void *)(uintptr_t)0x041B92C0U, &g_gpuGeSubLists[377], 36U);
        sceKernelMemcpy((void *)(uintptr_t)POPS_VRAM_GE_CONSTS, &g_gpuGeSubLists[354], 52U);
        sceKernelMemcpy((void *)(uintptr_t)(POPS_VRAM_GE_CONSTS + 0x40U), &g_gpuGeSubLists[367], 40U);
        sceKernelMemcpy((void *)(uintptr_t)(POPS_VRAM_GE_CONSTS + 0x80U), &g_gpuGeSubLists[315], 64U);
        sceKernelMemcpy((void *)(uintptr_t)(POPS_VRAM_GE_CONSTS + 0xC0U), &g_gpuGeSubLists[347], 28U);
        sceKernelMemcpy((void *)(uintptr_t)(POPS_VRAM_GE_CONSTS + 0x100U), &g_gpuGeSubLists[331], 64U);
        sceKernelMemcpy((void *)(uintptr_t)0x041B9300U, &g_gpuGePostList[25], 76U);
        sceKernelMemcpy((void *)(uintptr_t)POPS_VRAM_SUBLIST_BASE, &g_gpuGePostList[0], 100U);

        {
            s32 zeroSetting = __builtin_allegrex_max(popsGetZeroSetting(), 0);
            *(volatile u32 *)(uintptr_t)POPS_VRAM_SUBLIST_BASE =
                ((1052688U * (u32)zeroSetting) & 0xFFFFFFU) | 0x55000000U;
        }

        /* Build 16 vertical blit strip vertex pairs (64 px wide x 512 px tall) */
        blitVerts = (volatile u16 *)(uintptr_t)POPS_VRAM_BLIT_VERTS;
        for (idx = 0U; idx < 16U; idx++) {
            u16 xStart = (u16)(idx << 6);
            blitVerts[0] = xStart;
            blitVerts[1] = 0U;
            blitVerts[2] = 0U;
            blitVerts[3] = (u16)(xStart + 64U);
            blitVerts[4] = 512U;
            blitVerts[5] = 0U;
            blitVerts += 6;
        }

        popsGpuApplyClocks();

        /* Preserve VSH shared config fields across full zero-init */
        savedScreenMode = POPS_SHARED_MEM->screenMode;
        savedCustomFlag = POPS_SHARED_MEM->customModeFlag;
        savedCustomState = POPS_SHARED_MEM->customDisplayState;
        savedDiscCount = POPS_SHARED_MEM->discCountShadow;
        savedParamA = POPS_SHARED_MEM->customScreenParams[0];
        savedParamB = POPS_SHARED_MEM->customScreenParams[1];

        scePaf_D9E2D6E1((void *)(uintptr_t)POPS_DISPLAY_SHARED_BASE, 0, 432U);

        POPS_SHARED_MEM->screenMode = savedScreenMode;
        POPS_SHARED_MEM->discCountShadow = savedDiscCount;
        sceKernelMemcpy((void *)POPS_SHARED_MEM->customConfigBlock, &g_customConfig, 240U);
        POPS_SHARED_MEM->customModeFlag = savedCustomFlag;
        POPS_SHARED_MEM->customDisplayState = savedCustomState;
        POPS_SHARED_MEM->discQuirkFlag = ((g_pops->quirkFlags & 0x10000U) != 0U) ? 1U : 0U;
        POPS_SHARED_MEM->customScreenParams[0] = savedParamA;
        POPS_SHARED_MEM->customScreenParams[1] = savedParamB;

        for (idx = 0U; idx < 8U; idx++) {
            sceKernelMemcpy((void *)POPS_SHARED_MEM->padInputs[idx], g_padDefaultState, 8U);
        }
        sceKernelDcacheWritebackAll();
    }

    g_pops->gpuBusyEvent.prev = NULL;
    g_pops->gpuBusyEvent.next = NULL;
    g_pops->gpuBusyEvent.dueCycle = 0U;
    g_pops->gpuBusyEvent.callback = popsGpuBusyDoneCallback;

    g_pops->gpuSliceEvent.prev = NULL;
    g_pops->gpuSliceEvent.next = NULL;
    g_pops->gpuSliceEvent.dueCycle = 0U;
    g_pops->gpuSliceEvent.callback = popsGpuSliceCallback;

    g_pops->gpuVblankEvent.prev = NULL;
    g_pops->gpuVblankEvent.next = NULL;
    g_pops->gpuVblankEvent.dueCycle = 0U;
    g_pops->gpuVblankEvent.callback = NULL;

    g_pops->gpuFieldParity = 0U;
    popsGpuWriteReg(4U, 0U, 2U);

    {
        volatile u32 *dlist = (volatile u32 *)(uintptr_t)g_pops->gpuDlistCursor;
        dlist[0] = 0xD4000000U;
        dlist[1] = 0xD507FFFFU;
        dlist[2] = 0x55000000U;
        dlist[3] = 0x13040000U | 0x00001BB0U;
        dlist[4] = 0x0A000040U;
        g_pops->gpuDlistCursor = (u32)(uintptr_t)(dlist + 5);
    }

    popsGpuVblankStartCallback(NULL);
    popsRegisterDmaChannel(2U, popsGpuDmaTransfer);
    popsRegisterIoRange(0x1F801810U, 8U, popsGpuReadReg, popsGpuWriteReg);

    g_displayHoldState = 0xFFFFFFFFU;
    g_gpuInitPhase = 0U;
}

/* 0x017230: Look up per-title compatibility quirks (1,043 titles) and apply overrides */
s32 popsLoadGameQuirks(const char *titleId, u32 pkgVersion, u32 minVersion, const u8 *extraConfig)
{
    u32 *quirkWords = (u32 *)(void *)&g_pops->jitModeFlags;
    u32 titleHash;
    u32 idx;
    s32 jitFlags;

    if (minVersion > 0x06070060U) {
        return (s32)0x80000002U;
    }

    scePaf_D9E2D6E1(quirkWords, 0xFF, 128U);
    g_pops->spuDmaLatency = 0xFFFFU;
    g_pops->quirkFlags = 0U;
    g_pops->displayWidthBias = 0;
    g_pops->cdSeekDelayBias = 0U;
    g_pops->gpuTexPageCost = 0;
    g_pops->gpuReadCycleCost = 0;
    g_pops->quirkFlagsExt = 0;

    if (titleId == NULL) {
        popsPublishSpuQuirksB();
        return 0;
    }

    /* Apply PAL BIOS patch for European ('P' in 3rd char, e.g. SCES/SLES) titles */
    if (titleId[2] == 'P') {
        sceKernelMemcpy(&g_psxBiosRom[0x65E00], g_psxBiosPatch, sizeof(g_psxBiosPatch));
    }

    titleHash = (((u32)(u8)titleId[0] << 24) + ((u32)(u8)titleId[1] << 16) +
                 ((u32)(u8)titleId[2] << 8) + (u32)(u8)titleId[3]) ^
                (4097U * (u32)scePaf_3C4BC2CD(titleId + 4, NULL, 16));

    for (idx = 0U; idx < 1043U && g_popsQuirkIndex[idx].titleHash != 0U; idx++) {
        if (g_popsQuirkIndex[idx].titleHash == titleHash) {
            const PopsQuirkPair *pairs = g_popsQuirkIndex[idx].quirkPairs;
            u32 pairIdx;
            for (pairIdx = 0U; pairIdx < g_popsQuirkIndex[idx].entryCount; pairIdx++) {
                s32 slot = pairs[pairIdx].slot;
                u32 val = pairs[pairIdx].value;
                if (slot >= 0) {
                    quirkWords[slot] = val;
                } else {
                    g_pops->quirkFlags = val;
                }
            }
            break;
        }
    }

    if (scePaf_9CD6C5F4(titleId, "PSRM", 4U) == 0) {
        g_pops->cdSpeedScale = (u32)-2;
        g_pops->displayHeightClamp = 232;
        g_pops->displayTopAdjustNormal = 12;
        g_pops->jitRamLimitQuirk = 0x10004U;
        g_pops->displayTopAdjustWide = 12;
    }

    if (pkgVersion > 0x06070060U && extraConfig != NULL) {
        sceKernelMemcpy(&g_pops->quirkFlags, extraConfig + 132, 4U);
        sceKernelMemcpy(quirkWords, extraConfig + 136, 128U);
    }

    popsGpuApplyClocks();
    jitFlags = g_pops->jitModeFlags;
    g_pops->branchCycleBias++;
    if (jitFlags == -1 || (jitFlags & 4) == 0) {
        g_pops->jitChecksumLimit = 2;
    } else if (g_pops->jitChecksumLimit < 0) {
        g_pops->jitChecksumLimit = 0xFFFF;
    }

    if ((g_pops->quirkFlags & 0x40000000U) != 0U) {
        g_pops->displayWidthBias = -128;
    }

    popsPublishSpuQuirksB();
    popsGpuApplyClocks();
    return 0;
}

/* 0x017508: Relocate PRX import syscall stubs behind popsSyscallPatchTrampoline */
s32 popsPatchSyscallStubs(void)
{
    u32 *stubEnd = _popsTextEnd;
    u32 *stubStart = _popsTextStart;
    u32 *copyBuf;
    u32 byteLen;
    u32 curOff;
    u32 jumpInsn;

    while (stubEnd[-2] != 0x03E00008U || (stubEnd[-1] & 0xFC00003FU) != 0x0CU) {
        if ((stubEnd[-2] & 0xFC000000U) == 0x08000000U && stubEnd[-1] == 0U) {
            break;
        }
        stubEnd--;
    }

    while ((stubStart[1] & 0xFC00003FU) != 0x0CU) {
        stubStart++;
    }

    if (stubEnd == stubStart) {
        return 0;
    }

    byteLen = (u32)((u8 *)stubEnd - (u8 *)stubStart);
    copyBuf = (u32 *)scePaf_0FCDFA1E(byteLen);
    if (copyBuf == NULL) {
        return -1;
    }

    scePaf_B4652CFE(copyBuf, stubStart, byteLen);
    sceKernelDcacheWritebackRange(copyBuf, byteLen);

    g_syscallTargetDelta = (s32)((u8 *)copyBuf - (u8 *)stubStart);
    g_syscallStubBase = (u32)(uintptr_t)stubStart;

    jumpInsn = 0x08000000U | (((u32)(uintptr_t)popsSyscallPatchTrampoline >> 2) & 0x03FFFFFFU);
    curOff = 0x34020000U;

    while (stubStart != stubEnd) {
        if ((stubStart[1] & 0xFC00003FU) == 0x0CU) {
            stubStart[0] = jumpInsn;
            stubStart[1] = curOff;
            __builtin_allegrex_cache(0x1B, &stubStart[0]);
            __builtin_allegrex_cache(0x1B, &stubStart[1]);
            __builtin_allegrex_cache(0x08, &stubStart[0]);
            __builtin_allegrex_cache(0x08, &stubStart[1]);
        }
        stubStart += 2;
        curOff += 8U;
    }
    return 0;
}

/* 0x017658: Perform full PS1 machine cold/warm reset and re-initialize all hardware blocks */
u32 *popsResetMachine(void)
{
    u32 *spadWords = (u32 *)(void *)g_pops;
    u32 idx;
    u32 ramAddr;

    for (idx = 0U; idx < 431U; idx++) {
        spadWords[idx] = 0U;
    }
    for (idx = 2048U; idx < 3072U; idx++) {
        spadWords[idx] = 0U;
    }

    popsSetPauseState(3U);
    popsUpdateDisplayMute();

    popsInitCpuAndBios();
    popsInitSlowIo();
    popsInitDma();
    popsInitMdec();
    popsInitIrqIo();
    popsCdInitState();
    popsInitSio();
    popsInitJitPools();
    popsCdReset();
    popsInitEventQueue();
    popsInitTimers();
    popsInitSpu();

    popsSetPauseState(0U);
    popsUpdateDisplayMute();

    popsInitGpu();
    popsPadResetAll();

    /* Zero 2 MB PS1 main RAM (0x09800000..0x099FFFFF) using VFPU R403.q */
    ramAddr = POPS_PSX_RAM_BASE;
    while (ramAddr != 0x09A00000U) {
        u32 uncached = ramAddr | 0x40000000U;
        __builtin_allegrex_cache(0x19, (void *)(uintptr_t)ramAddr);
        __asm__ __volatile__("sync\n\tsv.q R403.q, 0(%0), wb" :: "r"(uncached) : "memory");
        __builtin_allegrex_cache(0x19, (void *)(uintptr_t)(ramAddr + 64U));
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
        ramAddr += 128U;
    }

    return popsCpuRaiseException(32U);
}

/* 0x017780: Cold boot entry: build GTE UNR table and reset PS1 machine */
u32 *popsInitAllSubsystems(void)
{
    popsInitGteUnrTable();
    return popsResetMachine();
}

/* 0x01779c: Initialize custom screen / ME audio config block from PBP header */
s32 popsInitCustomScreenConfig(const char *titleStr, const char *gameId, const u8 *srcCfg)
{
    (void)titleStr;
    (void)gameId;

    scePaf_D9E2D6E1(&g_customConfig, 0, sizeof(g_customConfig));
    POPS_SHARED_MEM->customDisplayState = 1U;

    if (*(const s8 *)srcCfg == 0) {
        g_customConfig.headerTag = 0U;
        return 0;
    }

    sceKernelMemcpy(&g_customConfig.bodyTag, srcCfg, 62U);
    if (g_customConfig.bodyTag != 169U) {
        g_customConfig.headerTag = 169U;
        return g_customConfig.bodyTag;
    }

    if (g_customConfig.configId == 0U) {
        g_customConfig.headerTag = 169U;
        POPS_SHARED_MEM->customDisplayState = 0U;
        return 0;
    }

    {
        u32 meParam[184096];
        s32 rc;
        meParam[1205] = g_customConfig.configId;
        meParam[1206] = 20U;
        POPS_SHARED_MEM->customModeFlag = g_customConfig.modeByte;
        rc = sceMeAudio_83609AC9(meParam, 0U);
        if (rc != 0) {
            g_customConfig.headerTag = 0U;
            return rc;
        }
        if (POPS_SHARED_MEM->customModeFlag != 0U) {
            POPS_SHARED_MEM->customDisplayState = 2U;
        }
        return 2;
    }
}

/* 0x017894: Unused menu stub returning 0 */
__attribute__((noinline))
s32 popsMenuNoop(void)
{
    return 0;
}

/* 0x01789c: Query current emulator pause state */
u32 popsGetPauseState(void)
{
    return g_pauseReason;
}

/* 0x0178a8: Transition pause state and mute/restore controller rumble */
u32 popsSetPauseState(u32 newState)
{
    u32 prev = g_pauseReason;
    g_pauseReason = newState;
    if (prev != newState) {
        if (newState != 0U) {
            popsGpuSetDisplayEnable(1U);
            popsPadMuteAllRumble();
        } else {
            popsPadRestoreAllRumble();
        }
    }
    return newState;
}

/* 0x017904: Update display mute flag from pause and hold states */
void popsUpdateDisplayMute(void)
{
    if (g_displayHoldState != 0U) {
        popsGpuSetDisplayEnable(1U);
    } else if (g_pauseReason != 0U) {
        popsGpuSetDisplayEnable(1U);
    } else {
        popsGpuSetDisplayEnable(0U);
    }
}

/* 0x01792c: Empty callback hook */
void popsNullStubA(void)
{
}

/* 0x017934: Return default zero setting for GPU clear color */
s32 popsGetZeroSetting(void)
{
    return 0;
}

/* 0x01793c: Query whether a newly formatted VMC was created */
s32 popsGetVmcCreatedFlag(void)
{
    return (s32)g_discKeyInitDone;
}

/* 0x017948: Latch VMC-created flag from savedata slot state */
s32 popsCheckVmcCreated(void)
{
    s32 hasSlot = popsSavedataHasSlot();
    g_discKeyInitDone = (u32)hasSlot;
    return hasSlot;
}

/* 0x01796c: Latch disc license verification token once */
s32 popsLatchDiscLicense(s32 token)
{
    if (g_discKeyMask != 1U) {
        g_discKeyMask = 1U;
        g_discLicenseFlag = (u32)token ^ 0x9136B780U;
    }
    return token;
}

/* 0x01799c: Copy 128-byte title header block into BSS */
void popsCopyTitleHeaderBlock(const u32 *srcWords)
{
    u32 idx;
    for (idx = 0U; idx < 32U; idx++) {
        g_titleHeaderCopy[idx] = srcWords[idx];
    }
}

/* 0x0179cc: Store scrambled ICON0.PNG width key */
void popsSetIconChecksumKey(s8 widthByte)
{
    u32 signedVal = (u32)(s32)widthByte;
    u32 rep = signedVal | (signedVal << 8) | (signedVal << 16) | (signedVal << 24);
    g_iconChecksumKey = rep ^ 0xFA499C89U;
}

/* 0x017a00: Bind CONFIG.BIN savedata buffer and reset quirks if unlicensed */
s32 popsInitSavedataConfig(void)
{
    if (popsSavedataSetConfigBuf(g_savedataConfigBuf, sizeof(g_savedataConfigBuf)) == 0) {
        return -1;
    }

    g_discKeyInitDone = 0U;
    if (g_discLicenseFlag == 0x9D7247C0U) {
        scePaf_D9E2D6E1(&g_pops->jitModeFlags, 0xFF, 128U);
        g_pops->quirkFlags = 0U;
    }
    popsCdSetLoadSpeed(0);
    sceKernelDcacheWritebackAll();
    return 0;
}

/* 0x017a78: Check for exit request and transition menu state */
s32 popsCheckExitState(void)
{
    s32 exitReq = popsCheckExitRequest();
    if (exitReq == 2) {
        g_menuState = 86U;
    }
    return exitReq;
}

/* 0x017aac: Handle disc read error / memory stick resume recovery */
void popsHandleDiscError(s32 throttleOnResume)
{
    g_discLicenseFlag = 0U;
    if (g_menuState == 73U) {
        return;
    }

    if (g_menuState == 0U) {
        popsSetPauseState(1U);
        popsGpuFlushGeList();
    }

    if ((g_menuState >= 47U && g_menuState < 60U) || g_menuState == 87U) {
        popsMenuNoop();
    }

    do {
        sceDisplayWaitVblankStartCB();
    } while (g_powerSuspending != 0U);

    if (g_menuState != 86U) {
        if (g_msInsertedState >= 0) {
            s32 tries = 3;
            while (tries-- > 0) {
                if (popsDiscOpenImageFd() >= 0) {
                    break;
                }
                if (tries == 0) {
                    scePopsMan_0090B2C8((s32)0x80010005U);
                    break;
                }
            }
        }
        popsCheckExitState();
        if (g_menuState == 0U) {
            popsSetPauseState(0U);
            if (throttleOnResume > 0) {
                popsGpuThrottleSpeed();
            }
            popsGpuRestartGeList();
            popsUpdateDisplayMute();
        }
    }
}

/* 0x017bd4: PSP power callback (suspend / resume) */
int popsPowerCallback(int notifyId, int powerInfo, void *common)
{
    (void)notifyId;
    (void)common;

    if ((powerInfo & 0x90000) != 0) {
        g_powerSuspending = 1U;
    } else if ((powerInfo & 0x40000) != 0) {
        g_powerSuspending = 0U;
        sceMeAudio_F7CD1362(0U);
        popsGpuMarkFrameDirty();
    }
    return 0;
}

/* 0x017c2c: Memory Stick insertion/ejection callback */
int popsMsCallback(int notifyId, int msEvent, void *common)
{
    (void)notifyId;
    (void)common;

    if (msEvent == 1) {
        g_msInsertedState = 0;
    } else if (msEvent == 2) {
        g_msInsertedState = 1;
    }
    return 0;
}

/* 0x017c60: Exit request check stub */
s32 popsCheckExitRequest(void)
{
    return 0;
}
