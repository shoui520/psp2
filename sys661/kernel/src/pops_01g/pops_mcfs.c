#include "pops_internal.h"

static s32 g_mcfsNotReady;
PopsMcfsCardState g_mcfsCards[5];

/* 0x018684: Write raw bytes to a bound PS1 memory card image */
s32 popsMcfsWriteRaw(u32 slotIdx, const void *src, u32 offset, u32 byteLen)
{
    u32 maxCopy;

    if (g_mcfsNotReady != 0) {
        return (s32)0x81018005U;
    }
    if (slotIdx >= 5U || src == NULL) {
        return (s32)0x81010002U;
    }
    if (offset > 0x83FFFFU) {
        return 0;
    }

    maxCopy = 0x840000U - offset;
    if (byteLen < maxCopy) {
        maxCopy = byteLen;
    }
    scePaf_B4652CFE((u8 *)g_mcfsCards[slotIdx].rawImagePtr + offset, src, maxCopy);
    return (s32)maxCopy;
}

/* 0x018738: Read raw bytes from a bound PS1 memory card image */
s32 popsMcfsReadRaw(u32 slotIdx, void *dst, u32 offset, u32 byteLen)
{
    u32 maxCopy;

    if (g_mcfsNotReady != 0) {
        return (s32)0x81018005U;
    }
    if (slotIdx >= 5U || dst == NULL) {
        return (s32)0x81010002U;
    }
    if (offset > 0x83FFFFU) {
        return 0;
    }

    maxCopy = 0x840000U - offset;
    if (byteLen < maxCopy) {
        maxCopy = byteLen;
    }
    scePaf_B4652CFE(dst, (const u8 *)g_mcfsCards[slotIdx].rawImagePtr + offset, maxCopy);
    return (s32)maxCopy;
}

/* 0x0187e0: Verify MCFS library initialization state */
s32 popsMcfsCheckReady(void)
{
    return (g_mcfsNotReady == 0) ? 0 : (s32)0x81018005U;
}

/* 0x0187f8: Unpack 32-bit word and advance stream pointer */
const u32 *popsMcfsReadU32(const u32 *src, u32 *outVal)
{
    *outVal = *src;
    return src + 1;
}

/* 0x018808: Unpack 16-bit halfword and advance stream pointer */
const u16 *popsMcfsReadU16(const u16 *src, u16 *outVal)
{
    *outVal = *src;
    return src + 1;
}

/* 0x018818: Pack 32-bit word and advance stream pointer */
u32 *popsMcfsWriteU32(u32 *dst, u32 val)
{
    *dst = val;
    return dst + 1;
}

/* 0x018824: Pack 16-bit halfword and advance stream pointer */
u16 *popsMcfsWriteU16(u16 *dst, u16 val)
{
    *dst = val;
    return dst + 1;
}

/* 0x018830: Compute 127-byte XOR frame checksum */
u32 popsMcfsComputeXor(const u8 *frameBytes)
{
    u32 xsum = 0U;
    u32 idx;

    for (idx = 0U; idx < 127U; idx++) {
        xsum ^= frameBytes[idx];
    }
    return xsum;
}

/* 0x018854: Read N 128-byte frames from a PS1 memory card */
s32 popsMcfsReadFrames(u32 slotIdx, void *dst, u32 offset, u32 byteLen)
{
    u32 count = byteLen >> 7;
    u8 *dstCur = (u8 *)dst;
    u32 idx;

    for (idx = 0U; idx < count; idx++) {
        s32 rc = popsMcfsReadRaw(slotIdx, dstCur, offset, 128U);
        if (rc < 0) {
            return rc;
        }
        dstCur += 128U;
        offset += 128U;
    }
    return 0;
}

/* 0x0188d8: Write N 128-byte frames to a PS1 memory card */
s32 popsMcfsWriteFrames(u32 slotIdx, const void *src, u32 offset, u32 byteLen)
{
    u32 count = byteLen >> 7;
    const u8 *srcCur = (const u8 *)src;
    u32 idx;

    for (idx = 0U; idx < count; idx++) {
        s32 rc = popsMcfsWriteRaw(slotIdx, srcCur, offset, 128U);
        if (rc < 0) {
            return rc;
        }
        srcCur += 128U;
        offset += 128U;
    }
    return 0;
}

/* 0x01895c: Serialize, checksum, and write a single directory frame */
s32 popsMcfsWriteDirEntry(u32 slotIdx, PopsMcfsDirEntry *entry)
{
    u32 frameBuf[32];
    u32 *cur32;
    u16 *cur16;
    u8 *tailBytes;
    u8 xsum;
    u32 relOff;

    cur32 = popsMcfsWriteU32(frameBuf, entry->allocState);
    cur32 = popsMcfsWriteU32(cur32, entry->fileSize);
    cur16 = popsMcfsWriteU16((u16 *)cur32, entry->nextBlock);
    tailBytes = (u8 *)cur16;

    scePaf_B4652CFE(tailBytes, entry->fileName, 21U);
    scePaf_B4652CFE(tailBytes + 21U, entry->unusedBytes, 95U);
    tailBytes[116] = entry->extraFlag;

    xsum = (u8)popsMcfsComputeXor((const u8 *)frameBuf);
    entry->checksum = xsum;
    tailBytes[117] = xsum;

    relOff = (u32)((u8 *)entry - (u8 *)&g_mcfsCards[slotIdx]) - 144U;
    return popsMcfsWriteFrames(slotIdx, frameBuf, (relOff & ~0x7FU) + 128U, 128U);
}

/* 0x018a30: Validate 'MC' magic signature on header frame */
s32 popsMcfsCheckMagic(u32 slotIdx)
{
    const PopsMcfsCardState *card = &g_mcfsCards[slotIdx];
    if (card->headerFrame[0] == 'M' || card->headerFrame[1] == 'C') {
        return 0;
    }
    return (s32)0x8101002FU;
}

/* 0x018a80: Format card header, 15 directory frames, and 20 broken-sector frames */
s32 popsMcfsFormatCardBody(u32 slotIdx)
{
    PopsMcfsCardState *card = &g_mcfsCards[slotIdx];
    u32 idx;
    s32 rc;

    scePaf_D9E2D6E1(card->headerFrame, 0, 0x2000U);
    card->headerFrame[0] = 'M';
    card->headerFrame[1] = 'C';
    card->headerFrame[127] = 0x0EU;

    rc = popsMcfsWriteFrames(slotIdx, card->headerFrame, 0U, 128U);
    if (rc < 0) {
        return rc;
    }

    for (idx = 0U; idx < 15U; idx++) {
        PopsMcfsDirEntry *dir = &card->dir[idx];
        dir->allocState = 0xA0U;
        dir->nextBlock = 0xFFFFU;
        dir->extraFlag = 0U;
        rc = popsMcfsWriteDirEntry(slotIdx, dir);
        if (rc < 0) {
            return rc;
        }
    }

    for (idx = 0U; idx < 20U; idx++) {
        u32 frameBuf[32];
        u32 *srcWords = (u32 *)(void *)card->brokenSectors[idx];
        u32 *dstWords = frameBuf;
        u8 *tail;
        u32 wordIdx;
        u8 xsum;

        srcWords[0] = 0xFFFFFFFFU;
        srcWords[2] = 0x0000FFFFU;
        for (wordIdx = 0U; wordIdx < 31U; wordIdx++) {
            dstWords = popsMcfsWriteU32(dstWords, srcWords[wordIdx]);
        }
        tail = (u8 *)dstWords;
        scePaf_B4652CFE(tail, &card->brokenSectors[idx][124], 3U);
        xsum = (u8)popsMcfsComputeXor((const u8 *)frameBuf);
        card->brokenSectors[idx][127] = xsum;
        tail[3] = xsum;

        rc = popsMcfsWriteFrames(slotIdx, frameBuf, 2048U + (idx << 7), 128U);
        if (rc < 0) {
            return rc;
        }
    }

    return 0;
}

/* 0x018c54: Mount PS1 memory card by loading header, directory, and broken-sector table */
s32 popsMcfsMountCard(u32 slotIdx)
{
    PopsMcfsCardState *card = &g_mcfsCards[slotIdx];
    u32 frameBuf[32];
    s32 rc;
    u32 idx;

    scePaf_D9E2D6E1(card->headerFrame, 0, 0x2000U);
    rc = popsMcfsReadFrames(slotIdx, card->headerFrame, 0U, 128U);
    if (rc < 0) {
        return rc;
    }

    for (idx = 0U; idx < 15U; idx++) {
        PopsMcfsDirEntry *dir = &card->dir[idx];
        const u32 *src32;
        const u16 *src16;
        const u8 *tail;

        rc = popsMcfsReadFrames(slotIdx, frameBuf, 128U + (idx << 7), 128U);
        if (rc < 0) {
            return rc;
        }
        src32 = popsMcfsReadU32(frameBuf, &dir->allocState);
        src32 = popsMcfsReadU32(src32, &dir->fileSize);
        src16 = popsMcfsReadU16((const u16 *)src32, &dir->nextBlock);
        tail = (const u8 *)src16;
        scePaf_B4652CFE(dir->fileName, tail, 21U);
        scePaf_B4652CFE(dir->unusedBytes, tail + 21U, 95U);
        dir->checksum = tail[117];
    }

    for (idx = 0U; idx < 20U; idx++) {
        u32 *dstWords = (u32 *)(void *)card->brokenSectors[idx];
        const u32 *src32 = frameBuf;
        const u8 *tail;
        u32 wordIdx;

        dstWords[0] = 0xFFFFFFFFU;
        rc = popsMcfsReadFrames(slotIdx, frameBuf, 2048U + (idx << 7), 128U);
        if (rc < 0) {
            return rc;
        }
        for (wordIdx = 0U; wordIdx < 31U; wordIdx++) {
            src32 = popsMcfsReadU32(src32, &dstWords[wordIdx]);
        }
        tail = (const u8 *)src32;
        scePaf_B4652CFE(&card->brokenSectors[idx][124], tail, 3U);
        card->brokenSectors[idx][127] = tail[3];
    }

    return 0;
}

/* 0x018e44: Count free 8 KB blocks on a mounted PS1 memory card */
s32 popsMcfsCountFreeBlocks(u32 slotIdx, u32 *outFreeBlocks)
{
    const PopsMcfsCardState *card = &g_mcfsCards[slotIdx];
    u32 idx;

    if (card->headerFrame[0] != 'M' && card->headerFrame[1] != 'C') {
        *outFreeBlocks = 0U;
        return (s32)0x8101002FU;
    }

    *outFreeBlocks = 0U;
    for (idx = 0U; idx < 15U; idx++) {
        if ((card->dir[idx].allocState & 0xF0U) != 0x50U) {
            (*outFreeBlocks)++;
        }
    }
    return 0;
}

/* 0x018ed0: Query card type, format validity, and free block count */
s32 popsMcfsGetCardInfo(u32 slotIdx, u32 *outInfo)
{
    s32 rc = popsMcfsCheckReady();
    if (rc >= 0) {
        u32 freeCount;
        u32 isFormatted = (popsMcfsCheckMagic(slotIdx) == 0) ? 1U : 0U;
        rc = popsMcfsCountFreeBlocks(slotIdx, &freeCount);
        if (outInfo != NULL) {
            outInfo[0] = g_mcfsCards[slotIdx].cardType;
            outInfo[1] = isFormatted;
            outInfo[2] = freeCount;
        }
    }
    return rc;
}

/* 0x018f6c: Format a PS1 memory card after checking library readiness */
s32 popsMcfsFormatCard(u32 slotIdx)
{
    s32 rc = popsMcfsCheckReady();
    if (rc < 0) {
        return rc;
    }
    return popsMcfsFormatCardBody(slotIdx);
}
