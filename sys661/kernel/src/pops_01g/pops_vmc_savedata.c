#include "pops_internal.h"

/* Savedata context layout (1,816 bytes = 454 words at g_savedataCtx) */
typedef struct PopsSavedataContext {
    u32 iconBufferPtr;          /* +0 (word 0) */
    u32 utilityParams[384];     /* +4..+1539 (words 1..384, size 1536) */
    u32 slotBufPtrs[2];         /* +1540..+1547 (words 385..386) */
    u32 iconBufferSize;         /* +1548 (word 387) */
    u32 configBufPtr;           /* +1552 (word 388) */
    u32 configBufSize;          /* +1556 (word 389) */
    u8 titleName[128];          /* +1560..+1687 (words 390..421) */
    u8 gameIdStr[128];          /* +1688..+1815 (words 422..453) */
} PopsSavedataContext;

POPS_STATIC_ASSERT(sizeof(PopsSavedataContext) == 1816, savedata_ctx_size);

static PopsSavedataContext g_savedataCtx;
static s32 g_savedataSlotFlags[10];
static u8 *g_pngSrcBuf;
static s32 g_pngReadErr;
static u32 g_pngSrcLenShadow;
static u8 *g_pngSrcBufShadow;
static u32 g_pngReadOffset;
static u8 *g_pngRowPtrs[480];
static u32 g_pngSrcLen;
static u32 g_mcfsSlotIds[2] = { 0U, 1U };

/* 0x017c68: Read ICON0.PNG from the PBP header and probe its width/checksum */
s32 popsLoadPbpIconPng(SceUID fileDesc)
{
    u32 pbpHeader[10];
    u32 iconOff;
    u32 iconEnd;
    u32 iconSize;
    u8 *iconBuf;
    s32 got;

    g_savedataCtx.iconBufferSize = 0U;
    if (sceMeAudio_A651EA7B(fileDesc, pbpHeader, 0U, 40U) != 40) {
        return -1;
    }
    if (pbpHeader[0] != 0x50425000U) {
        return -1;
    }
    iconOff = pbpHeader[3];
    iconEnd = pbpHeader[4];
    if (iconEnd < iconOff) {
        return -1;
    }

    iconSize = iconEnd - iconOff;
    g_savedataCtx.iconBufferSize = iconSize;
    iconBuf = (u8 *)scePaf_0FCDFA1E(iconSize);
    g_savedataCtx.iconBufferPtr = (u32)(uintptr_t)iconBuf;

    if (iconBuf != NULL) {
        got = sceMeAudio_A651EA7B(fileDesc, iconBuf, iconOff, iconSize);
        if ((u32)got == g_savedataCtx.iconBufferSize) {
            s32 width = popsProbePngWidth(iconBuf, iconSize);
            popsSetIconChecksumKey((s8)width);
            return 0;
        }
    }

    scePaf_71B92320(iconBuf);
    return -1;
}

/* 0x017d48: Initialize savedata title strings and slot status markers */
void popsSavedataInitContext(const char *titleStr, const char *gameId, u32 unusedParam)
{
    (void)unusedParam;
    g_savedataKey[3] = (u8)(g_savedataKey[3] + (u8)(titleStr[0] == '\0'));
    if (gameId[0] != '\0') {
        sceKernelMemcpy(g_savedataCtx.gameIdStr, gameId, 128U);
    }
    scePaf_D9E2D6E1(g_savedataSlotFlags, -1, sizeof(g_savedataSlotFlags));
    g_savedataStatus = -1;
    if (titleStr[0] != '\0') {
        sceKernelMemcpy(g_savedataCtx.titleName, titleStr, 128U);
    }
}

/* 0x017de8: Populate SceUtilitySavedataParam and start savedata operation */
s32 popsSavedataStartOp(PopsSavedataContext *ctx, u32 mode, void *dataBuf,
                        u32 dataSize, const char *fileName, const char *gameName)
{
    u32 *params = ctx->utilityParams;
    u8 *paramBytes = (u8 *)(void *)params;

    scePaf_D9E2D6E1(params, 0, 1536U);
    params[0] = 1536U;
    if (sceImposeGetLanguageMode((int *)&params[1], (int *)&params[2]) < 0) {
        return -1;
    }

    params[3] = 24U;
    params[4] = 26U;
    params[5] = 25U;
    params[6] = 23U;
    params[12] = mode;

    sceKernelMemcpy(&params[15], gameName, 12U);
    paramBytes[72] = 0U;
    paramBytes[76] = 0U;
    sceKernelMemcpy(&params[375], g_savedataKey, 16U);

    params[29] = (u32)(uintptr_t)dataBuf;
    params[30] = dataSize;
    scePaf_1F02DD65((char *)&params[25], fileName, 13U);
    params[14] = 1U;

    if (mode < 19U) {
        if (((1U << mode) & 0x00060002U) != 0U) {
            params[31] = dataSize;
            sceKernelMemcpy(&params[32], ctx->titleName, 128U);
            paramBytes[1408] = 1U;
            paramBytes[256] = 0U;
            paramBytes[384] = 0U;
            params[353] = ctx->iconBufferPtr;
            params[354] = ctx->iconBufferSize;
            params[355] = ctx->iconBufferSize;
        } else if (((1U << mode) & 0x00018000U) != 0U) {
            scePaf_D9E2D6E1(&params[32], 0, 1284U);
        }
    }

    sceUtilitySavedataInitStart(params);
    return 1;
}

/* 0x017f88: Classify savedata error as missing-file (2) vs fatal (1) */
s32 popsSavedataClassifyError(s32 resultCode)
{
    if (resultCode == (s32)0x80110327U || resultCode == (s32)0x80110329U) {
        return 2;
    }
    return 1;
}

/* 0x017fb0: Save CONFIG.BIN for current title */
s32 popsSavedataSaveConfig(void)
{
    return popsSavedataStartOp(&g_savedataCtx, 1U,
                               (void *)(uintptr_t)g_savedataCtx.configBufPtr,
                               g_savedataCtx.configBufSize,
                               "CONFIG.BIN",
                               (const char *)g_savedataCtx.gameIdStr);
}

/* 0x017fd8: Read savedata dialog result code from SceUtilitySavedataParam */
s32 popsSavedataGetResult(u32 ctxIdx)
{
    return (s32)(&g_savedataCtx)[ctxIdx].utilityParams[7];
}

/* 0x017ff8: Check whether savedata slot 0 flag is non-negative */
s32 popsSavedataHasSlot(void)
{
    return (s32)((u32)g_savedataSlotFlags[0] >> 31) ^ 1;
}

/* 0x01800c: Bind CONFIG.BIN buffer pointer and size */
s32 popsSavedataSetConfigBuf(void *bufPtr, u32 bufSize)
{
    g_savedataCtx.configBufSize = bufSize;
    g_savedataCtx.configBufPtr = (u32)(uintptr_t)bufPtr;
    return 1;
}

/* 0x018024: Bind VMP buffer pointer for a memory card slot */
s32 popsSavedataBindSlotBuf(u32 ctxIdx, u32 slotIdx, void *vmpBuf)
{
    (&g_savedataCtx)[ctxIdx].slotBufPtrs[slotIdx] = (u32)(uintptr_t)vmpBuf;
    popsMcfsBindRawBuffer(2U * ctxIdx + slotIdx, (u8 *)vmpBuf + 128U);
    return 1;
}

/* 0x018070: Poll and drive sceUtilitySavedataUpdate / ShutdownStart */
s32 popsSavedataPollStatus(void)
{
    s32 status = sceUtilitySavedataGetStatus();
    s32 wasBusy;

    __asm__ __volatile__("mtic $zero, $0\n\tnop" ::: "memory");
    wasBusy = g_savedataBusyFlag;
    g_savedataBusyFlag = 1;
    __asm__ __volatile__("mtic %0, $0" :: "r"(1) : "memory");

    if (wasBusy == 0) {
        if (popsGetPauseState() != 0U || popsSpuGetCddaPending() > 0 ||
            (++g_savedataTickCount >= 10)) {
            g_savedataTickCount = 0;
            if (status == 2) {
                sceUtilitySavedataUpdate(1);
            } else if (status == 3) {
                sceUtilitySavedataShutdownStart();
            }
        }
        g_savedataBusyFlag = 0;
    }
    return status;
}

/* 0x01813c: Block until sceUtilitySavedata returns to idle state */
s32 popsSavedataWaitIdle(void)
{
    s32 status;
    for (;;) {
        status = popsSavedataPollStatus();
        if (status <= 0) {
            break;
        }
        sceDisplayWaitVblankStart();
    }
    return status;
}

/* 0x01816c: Encrypt and save SCEVMC0.VMP / SCEVMC1.VMP */
s32 popsSavedataSaveVmc(u32 slotIdx)
{
    if (slotIdx == 0U && popsMcGetPocketEnable() != 0U) {
        return 0;
    }
    popsSavedataWaitIdle();
    pspvmc_38A87A12((void *)(uintptr_t)g_savedataCtx.slotBufPtrs[slotIdx]);
    return popsSavedataStartOp(&g_savedataCtx, 18U,
                               (void *)(uintptr_t)g_savedataCtx.slotBufPtrs[slotIdx],
                               0x20080U,
                               g_vmcFileNames[slotIdx],
                               (const char *)g_savedataCtx.gameIdStr);
}

/* 0x01820c: Common VMP load, magic check ('\0PMV'), and pspvmc decryption */
static __inline__ __attribute__((always_inline)) s32 popsVmcLoadCommon(u32 ctxIdx, u32 slotIdx, const char *gameName)
{
    PopsSavedataContext *ctx = &(&g_savedataCtx)[ctxIdx];
    u32 *vmpBuf = (u32 *)(uintptr_t)ctx->slotBufPtrs[slotIdx];
    s32 rc;
    s32 resCode;
    u32 outSize;

    rc = popsSavedataStartOp(ctx, 16U, vmpBuf, 0x20080U, g_vmcFileNames[slotIdx], gameName);
    if (rc < 0) {
        return -1;
    }
    popsSavedataWaitIdle();

    resCode = (s32)ctx->utilityParams[7];
    if (resCode != 0) {
        if (resCode == (s32)0x80110327U || resCode == (s32)0x80110329U) {
            return 2;
        }
        return 1;
    }

    if (*vmpBuf != 0x564D5000U) {
        return 2;
    }
    rc = pspvmc_0820B291(vmpBuf, &outSize);
    if (outSize != 128U) {
        return 1;
    }
    return (rc != 0) ? 1 : 0;
}

/* 0x0182f4: Load VMC using custom savedata title ID */
s32 popsVmcLoadByCustomTitle(u32 slotIdx)
{
    if (g_customTitleIdPtr == NULL) {
        return -1;
    }
    return popsVmcLoadCommon(0U, slotIdx, g_customTitleIdPtr);
}

/* 0x01831c: Load VMC using current game title ID */
s32 popsVmcLoadCurrentTitle(u32 slotIdx)
{
    return popsVmcLoadCommon(0U, slotIdx, (const char *)g_savedataCtx.gameIdStr);
}

/* 0x018330: Bind raw 128 KB card memory buffer to MCFS driver slot */
s32 popsMcfsBindRawBuffer(u32 slotIdx, void *rawCardBuf)
{
    g_mcfsCards[slotIdx].rawImagePtr = rawCardBuf;
    return 0;
}

/* 0x01835c: Repair uninitialized broken-sector list entries on a PS1 memory card */
s32 popsMcfsRepairEmptyBlocks(u32 slotIdx)
{
    u8 *rawBase = (u8 *)g_mcfsCards[slotIdx].rawImagePtr;
    u32 offset;

    for (offset = 0U; offset < 2560U; offset += 128U) {
        u32 *entry = (u32 *)(void *)(rawBase + 2048U + offset);
        if (entry[0] == 0U && entry[2] == 0U) {
            entry[2] = 0xFFFFU;
            entry[0] = 0xFFFFFFFFU;
        }
    }
    return 0;
}

/* 0x0183c4: Return 1 if card has all 15 blocks free (empty), else 0 */
s32 popsMcfsIsCardEmpty(u32 slotIdx)
{
    u32 cardInfo[4];

    if (popsMcfsMountCard(slotIdx) < 0) {
        return 0;
    }
    if (popsMcfsGetCardInfo(g_mcfsSlotIds[slotIdx], cardInfo) < 0) {
        return 0;
    }
    if (cardInfo[2] == 15U) {
        return 1;
    }
    return 0;
}

/* 0x018428: libpng custom memory stream reader callback */
void popsPngReadCallback(void *pngStruct, void *outBuf, u32 byteCount)
{
    (void)pngStruct;
    if ((s32)(g_pngReadOffset + byteCount) >= (s32)g_pngSrcLen) {
        g_pngReadErr = -1;
        return;
    }
    sceKernelMemcpy(outBuf, g_pngSrcBuf + g_pngReadOffset, byteCount);
    g_pngReadOffset += byteCount;
}

/* 0x01849c: Decode PNG icon via scePaf libpng 1.2.37 */
static s32 popsDecodePngIcon(u32 expectedWidth)
{
    void *pngPtr;
    void *infoPtr;
    u32 width = 0U;
    u32 height = 0U;
    s32 bitDepth = 0;
    s32 colorType = 0;
    s32 interlace = 0;

    g_pngReadErr = 0;
    g_pngReadOffset = 0U;

    pngPtr = scePaf_8C5CC663("1.2.37", NULL, NULL, NULL);
    if (pngPtr == NULL) {
        return -1;
    }

    infoPtr = scePaf_43759F51(pngPtr);
    if (infoPtr == NULL) {
        scePaf_FF9C876B(&pngPtr, &infoPtr, NULL);
        return -1;
    }

    scePaf_5A12583F(pngPtr, NULL, popsPngReadCallback);
    scePaf_4E43A742(pngPtr, infoPtr);
    if (g_pngReadErr < 0) {
        return -1;
    }

    scePaf_3E564415(pngPtr, infoPtr, &width, &height, &bitDepth, &colorType, &interlace, NULL, NULL);
    if (expectedWidth == 0U) {
        return (s32)width;
    }

    if (width != expectedWidth || height > 480U) {
        height = (u32)-1;
    } else {
        if (bitDepth == 16) {
            scePaf_6D55F3F5(pngPtr);
        } else if (bitDepth < 8) {
            scePaf_A631AC8B(pngPtr);
        }
        if (colorType == 0 || colorType == 4) {
            scePaf_66CCA794(pngPtr);
        }
        if (colorType == 3) {
            scePaf_20BEF384(pngPtr);
        }
        if (colorType == 0 || colorType == 2 || colorType == 3) {
            scePaf_E49835DC(pngPtr, 0xFF000000U, 1);
        }
        scePaf_9D854500(pngPtr, g_pngRowPtrs);
    }

    scePaf_FF9C876B(&pngPtr, &infoPtr, NULL);
    return (s32)height;
}

/* 0x018658: Probe PNG width from an in-memory PNG buffer */
s32 popsProbePngWidth(void *pngData, u32 pngSize)
{
    g_pngSrcBufShadow = (u8 *)pngData;
    g_pngSrcLenShadow = pngSize;
    g_pngSrcBuf = (u8 *)pngData;
    g_pngSrcLen = pngSize;
    return popsDecodePngIcon(0U);
}
