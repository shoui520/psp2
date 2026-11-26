/* Copyright (C) 2011, 2012, 2013 The uOFW team
   See the file COPYING for copying permission.
*/

#include "usersystemlib_int.h"

typedef struct __attribute__((packed)) {
    u32 val;
} UnalignedWord;

static inline u32 loadUnalignedWord(const void *ptr)
{
    return ((const UnalignedWord *)ptr)->val;
}

static inline void storeUnalignedWord(void *ptr, u32 val)
{
    ((UnalignedWord *)ptr)->val = val;
}

void *sceKernelMemcpy(void *dst, const void *src, SceSize size)
{
    u8 *dstByte = (u8 *)dst;
    const u8 *srcByte = (const u8 *)src;
    u8 *dstEnd = dstByte + size;

    if (dst == src) {
        return dst;
    }

    if (size < 8) {
        while (dstByte != dstEnd) {
            *dstByte++ = *srcByte++;
        }
        return dst;
    }

    u32 misalign = (u32)dstByte & 3;
    if (misalign != 0) {
        u32 head = 4 - misalign;
        for (u32 idx = 0; idx < head; idx++) {
            dstByte[idx] = srcByte[idx];
        }
        dstByte += head;
        srcByte += head;
    }

    if (((u32)dstByte & 0xE0000000u) == 0 && size >= 128) {
        u8 *lineStart = (u8 *)(((u32)dstByte + 63u) & ~63u);
        u8 *lineEnd = (u8 *)((u32)dstEnd & ~63u);

        if (((u32)srcByte & 3) == 0) {
            while (dstByte != lineStart) {
                *(u32 *)dstByte = *(const u32 *)srcByte;
                dstByte += 4;
                srcByte += 4;
            }
            while (dstByte != lineEnd) {
                u32 *dstWords = (u32 *)dstByte;
                const u32 *srcWords = (const u32 *)srcByte;
                pspCache(0x18, dstWords);
                dstWords[0] = srcWords[0];
                dstWords[1] = srcWords[1];
                dstWords[2] = srcWords[2];
                dstWords[3] = srcWords[3];
                dstWords[4] = srcWords[4];
                dstWords[5] = srcWords[5];
                dstWords[6] = srcWords[6];
                dstWords[7] = srcWords[7];
                dstWords[8] = srcWords[8];
                dstWords[9] = srcWords[9];
                dstWords[10] = srcWords[10];
                dstWords[11] = srcWords[11];
                dstWords[12] = srcWords[12];
                dstWords[13] = srcWords[13];
                dstWords[14] = srcWords[14];
                dstWords[15] = srcWords[15];
                dstByte += 64;
                srcByte += 64;
            }
        } else {
            while (dstByte != lineStart) {
                *(u32 *)dstByte = loadUnalignedWord(srcByte);
                dstByte += 4;
                srcByte += 4;
            }
            while (dstByte != lineEnd) {
                u32 *dstWords = (u32 *)dstByte;
                pspCache(0x18, dstWords);
                for (u32 wordIdx = 0; wordIdx < 16; wordIdx++) {
                    dstWords[wordIdx] = loadUnalignedWord(srcByte + wordIdx * 4);
                }
                dstByte += 64;
                srcByte += 64;
            }
        }
        if (dstByte == dstEnd) {
            return dst;
        }
    }

    u8 *wordEnd = (u8 *)((u32)dstEnd & ~3u);
    while (dstByte != wordEnd) {
        *(u32 *)dstByte = loadUnalignedWord(srcByte);
        dstByte += 4;
        srcByte += 4;
    }

    while (dstByte != dstEnd) {
        *dstByte++ = *srcByte++;
    }

    return dst;
}

void *sceKernelMemset(void *dst, s8 val, SceSize size)
{
    u8 *dstByte = (u8 *)dst;
    u8 *dstEnd = dstByte + size;

    if (size < 8) {
        while (dstByte != dstEnd) {
            *dstByte++ = (u8)val;
        }
        return dst;
    }

    u32 byteVal = (u8)val;
    u32 fill = byteVal | (byteVal << 8);
    fill |= fill << 16;

    u8 *alignedStart = (u8 *)(((u32)dstByte + 3u) & ~3u);
    if (alignedStart != dstByte) {
        storeUnalignedWord(dstByte, fill);
        size -= (SceSize)(alignedStart - dstByte);
    }

    if (((u32)dstEnd & 3u) != 0) {
        storeUnalignedWord(dstEnd - 4, fill);
    }

    SceSize wordBytes = size & ~3u;
    u8 *alignedEnd = alignedStart + wordBytes;

    if (((u32)alignedStart & 0xE0000000u) != 0 || wordBytes < 0x101u) {
        u8 *cur = alignedStart;
        while (cur != alignedEnd) {
            *(u32 *)cur = fill;
            cur += 4;
        }
        return dst;
    }

    u8 *lineStart = (u8 *)(((u32)alignedStart + 63u) & ~63u);
    u8 *cur = alignedStart;
    while (cur != lineStart) {
        *(u32 *)cur = fill;
        cur += 4;
    }

    u32 tailBytes = (u32)alignedEnd & 0x3Fu;
    u8 *lineEnd = alignedEnd - tailBytes;
    if (tailBytes != 0) {
        u8 *tailCur = lineEnd;
        while (tailCur != alignedEnd) {
            *(u32 *)tailCur = fill;
            tailCur += 4;
        }
    }

    while (cur != lineEnd) {
        u32 *words = (u32 *)cur;
        pspCache(0x18, words);
        words[0] = fill;
        words[1] = fill;
        words[2] = fill;
        words[3] = fill;
        words[4] = fill;
        words[5] = fill;
        words[6] = fill;
        words[7] = fill;
        words[8] = fill;
        words[9] = fill;
        words[10] = fill;
        words[11] = fill;
        words[12] = fill;
        words[13] = fill;
        words[14] = fill;
        words[15] = fill;
        cur += 64;
    }

    return dst;
}
