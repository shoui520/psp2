/*
 * SceSsl / sceSsl_Module (ePSP 6.61) - Embedded pool allocator and CRYPTO_* mem hooks.
 *
 * Implements functions [277..286] (0x0126e8..0x012d97, crypto/e_malloc.c)
 * and [301..308] (0x013788..0x0138e3, crypto/mem.c).
 */
#include "../libssl_internal.h"

typedef struct SslPoolBlock {
    struct SslPoolBlock *selfPayload;
    u32 size;
    struct SslPoolBlock *prev;
    struct SslPoolBlock *next;
} SslPoolBlock;

typedef struct SslPoolHeader {
    void *selfPayload;
    u32 size;
} SslPoolHeader;

static u8 *g_poolBase = NULL;
static u32 g_poolSize = 0;
static u32 g_poolAllocCount = 0;
static u32 g_poolFreeCount = 0;
static u32 g_poolErrorCount = 0;
static u32 g_poolUsedCurrent = 0;
static u32 g_poolUsedPeak = 0;
static u32 g_poolUsableTotal = 0;
static SslPoolBlock *g_poolFreeHead = NULL;

/* Default CRYPTO_* memory function pointers (0x02f1b8..0x02f1c8) */
SslMallocFn g_lockedMallocFn = sslPoolAlloc;
SslFreeFn g_lockedFreeFn = sslPoolFree;
SslMallocFn g_mallocFn = sslPoolAlloc;
SslReallocFn g_reallocFn = sslPoolRealloc;
SslFreeFn g_freeFn = sslPoolFree;

/* -------------------------------------------------------------------------
 * crypto/e_malloc.c [277..286]
 * ------------------------------------------------------------------------- */

/* [277] 0x0126e8 */
LIBSSL_NOINLINE void sceSsl_lib_9A789D37(void *poolMem, u32 poolSize)
{
    g_poolBase = (u8 *)poolMem;
    g_poolSize = poolSize;
}

/* [278] 0x0126fc */
LIBSSL_NOINLINE u32 sslPoolInit(void)
{
    u32 alignPad = ((u32)g_poolBase) & 3U;
    u32 lostBytes;
    SslPoolBlock *firstBlock;

    if (alignPad != 0) {
        alignPad = 4U - alignPad;
    }
    firstBlock = (SslPoolBlock *)(g_poolBase + alignPad);
    lostBytes = (alignPad != 0) ? 4U : 0U;

    firstBlock->size = g_poolSize - lostBytes - sizeof(SslPoolHeader);
    firstBlock->selfPayload = (SslPoolBlock *)((SslPoolHeader *)firstBlock + 1);
    firstBlock->prev = NULL;
    firstBlock->next = NULL;

    g_poolAllocCount = 0;
    g_poolFreeCount = 0;
    g_poolErrorCount = 0;
    g_poolUsedCurrent = 0;
    g_poolUsedPeak = 0;
    g_poolFreeHead = firstBlock;
    g_poolUsableTotal = firstBlock->size + sizeof(SslPoolHeader);
    return g_poolUsableTotal;
}

/* [279] 0x012778 */
LIBSSL_NOINLINE void *sslPoolAlloc(u32 reqSize)
{
    void *result = NULL;

    sceSsl_lib_D84E2411(9, 17, "crypto/e_malloc.c", 129);
    if (g_poolFreeHead == NULL) {
        sslPoolInit();
    }

    if (sslPoolVerify() == 0) {
        SslPoolBlock *cur = g_poolFreeHead;
        SslPoolBlock *best = NULL;
        u32 bestSize = 0x0fffffffU;
        u32 alignedSize;

        if (reqSize < 8U) {
            reqSize = 8U;
        }
        alignedSize = (reqSize + 3U) & ~3U;

        while (cur != NULL) {
            if (cur->size >= alignedSize && cur->size < bestSize) {
                best = cur;
                bestSize = cur->size;
                if (cur->size == alignedSize) {
                    break;
                }
            }
            cur = cur->next;
        }

        if (best != NULL) {
            u32 oldSize = best->size;
            u32 remain = oldSize - alignedSize;
            u8 *payload = (u8 *)best->selfPayload;

            result = payload;
            if (remain >= 16U) {
                SslPoolBlock *splitBlk = (SslPoolBlock *)(payload + alignedSize);
                splitBlk->selfPayload = (SslPoolBlock *)((SslPoolHeader *)splitBlk + 1);
                splitBlk->size = remain - sizeof(SslPoolHeader);
                splitBlk->prev = best->prev;
                splitBlk->next = best->next;

                best->size = alignedSize;
                if (best->prev != NULL) {
                    best->prev->next = splitBlk;
                }
                if (best->next != NULL) {
                    best->next->prev = splitBlk;
                }
                if (best == g_poolFreeHead) {
                    g_poolFreeHead = splitBlk;
                }
                g_poolUsedCurrent += best->size + sizeof(SslPoolHeader);
            } else {
                if (best == g_poolFreeHead) {
                    g_poolFreeHead = best->next;
                    if (g_poolFreeHead != NULL) {
                        g_poolFreeHead->prev = NULL;
                    }
                } else {
                    best->prev->next = best->next;
                    if (best->next != NULL) {
                        best->next->prev = best->prev;
                    }
                }
                g_poolUsedCurrent += oldSize + sizeof(SslPoolHeader);
            }

            g_poolAllocCount++;
            if (g_poolUsedPeak < g_poolUsedCurrent) {
                g_poolUsedPeak = g_poolUsedCurrent;
            }
        } else {
            sslPoolErrorHook();
        }
    }

    sceSsl_lib_D84E2411(10, 17, "crypto/e_malloc.c", 248);
    return result;
}

/* [280] 0x012964 */
LIBSSL_NOINLINE void *sslPoolRealloc(void *ptr, u32 newSize)
{
    void *result = NULL;

    sceSsl_lib_D84E2411(9, 17, "crypto/e_malloc.c", 260);
    if (sslPoolVerify() == 0) {
        if ((((u32)ptr) & 3U) != 0) {
            g_poolErrorCount++;
            sslPoolErrorHook();
        } else {
            SslPoolHeader *hdr = ((SslPoolHeader *)ptr) - 1;
            if (hdr->selfPayload != ptr || (hdr->size & 3U) != 0) {
                g_poolErrorCount++;
                sslPoolErrorHook();
            } else {
                u32 copyLen = (hdr->size < newSize) ? hdr->size : newSize;
                void *fresh = sslPoolAlloc(newSize);
                if (fresh != NULL) {
                    result = fresh;
                    sslMemcpy(fresh, ptr, copyLen);
                    sslPoolFree(ptr);
                }
            }
        }
    }

    sceSsl_lib_D84E2411(10, 17, "crypto/e_malloc.c", 322);
    return result;
}

/* [281] 0x012a78 */
LIBSSL_NOINLINE void sslPoolFree(void *ptr)
{
    sceSsl_lib_D84E2411(9, 17, "crypto/e_malloc.c", 331);
    if (sslPoolVerify() == 0 && ptr != NULL) {
        if ((((u32)ptr) & 3U) != 0) {
            g_poolErrorCount++;
            sslPoolErrorHook();
        } else {
            SslPoolBlock *blk = (SslPoolBlock *)(((SslPoolHeader *)ptr) - 1);
            if ((void *)blk->selfPayload != ptr || (blk->size & 3U) != 0) {
                g_poolErrorCount++;
                sslPoolErrorHook();
            } else {
                SslPoolBlock *cur = g_poolFreeHead;
                SslPoolBlock *prev = NULL;
                int ok = 1;

                g_poolUsedCurrent -= blk->size + sizeof(SslPoolHeader);

                while (cur != NULL && (u32)blk >= (u32)cur) {
                    prev = cur;
                    cur = cur->next;
                }

                if (cur != NULL) {
                    SslPoolBlock *curPrev = cur->prev;
                    if (curPrev != NULL) {
                        curPrev->next = blk;
                    }
                    blk->prev = curPrev;
                    blk->next = cur;
                    cur->prev = blk;
                    if (g_poolFreeHead == cur) {
                        g_poolFreeHead = blk;
                    }
                } else if (prev != NULL) {
                    blk->prev = prev;
                    blk->next = prev->next;
                    prev->next = blk;
                } else {
                    g_poolErrorCount++;
                    ok = 0;
                }

                if (ok) {
                    /* Coalesce with following contiguous free blocks */
                    while (blk->next != NULL &&
                           ((u8 *)blk->selfPayload + blk->size) == (u8 *)blk->next) {
                        SslPoolBlock *merged = blk->next;
                        blk->next = merged->next;
                        if (blk->next != NULL) {
                            blk->next->prev = blk;
                        }
                        blk->size += merged->size + sizeof(SslPoolHeader);
                    }

                    /* Coalesce with preceding contiguous free blocks */
                    while (blk->prev != NULL &&
                           ((u8 *)blk->prev->selfPayload + blk->prev->size) == (u8 *)blk) {
                        SslPoolBlock *left = blk->prev;
                        left->next = blk->next;
                        if (blk->next != NULL) {
                            blk->next->prev = left;
                        }
                        left->size += blk->size + sizeof(SslPoolHeader);
                        blk = left;
                    }

                    g_poolFreeCount++;
                }
            }
        }
    }

    sceSsl_lib_D84E2411(10, 17, "crypto/e_malloc.c", 473);
}

/* [282] 0x012c8c */
LIBSSL_NOINLINE void sslPoolErrorHook(void)
{
}

/* [283] 0x012c94 */
LIBSSL_NOINLINE int sslPoolVerify(void)
{
    SslPoolBlock *cur = g_poolFreeHead;
    u32 freeTotal = 0;
    int errCode = 0;

    while (cur != NULL) {
        SslPoolBlock *next = cur->next;
        if (next != NULL && next->prev != cur) {
            errCode = 1;
            break;
        }
        if ((((u32)cur->selfPayload) & 3U) != 0) {
            errCode = 2;
            break;
        }
        if ((void *)cur->selfPayload != (void *)((SslPoolHeader *)cur + 1)) {
            errCode = 3;
            break;
        }
        freeTotal += cur->size + sizeof(SslPoolHeader);
        if ((cur->size & 3U) != 0) {
            errCode = 4;
            break;
        }
        if (cur->size < 8U) {
            errCode = 5;
            break;
        }
        cur = next;
    }

    if (errCode == 0) {
        if (g_poolUsableTotal == freeTotal + g_poolUsedCurrent) {
            return 0;
        }
        errCode = 6;
    }

    sslPoolErrorHook();
    return errCode;
}

/* [284] 0x012d50 */
LIBSSL_NOINLINE u32 sceSsl_lib_9266C0D5(void)
{
    return g_poolUsedPeak;
}

/* [285] 0x012d5c */
LIBSSL_NOINLINE u32 sslPoolGetUsedCurrent(void)
{
    return g_poolUsedCurrent;
}

/* [286] 0x012d68 */
LIBSSL_NOINLINE void sslPoolReset(void)
{
    g_poolFreeHead = NULL;
    sslMemset(g_poolBase, 0, 4);
}

/* -------------------------------------------------------------------------
 * crypto/mem.c [301..308]
 * ------------------------------------------------------------------------- */

/* [301] 0x013788 */
LIBSSL_NOINLINE int sceSsl_lib_A2619268(SslMallocFn mallocFn, SslReallocFn reallocFn,
                                        SslFreeFn freeFn)
{
    if (mallocFn == NULL || reallocFn == NULL || freeFn == NULL) {
        return 0;
    }
    g_mallocFn = mallocFn;
    g_reallocFn = reallocFn;
    g_freeFn = freeFn;
    g_lockedMallocFn = mallocFn;
    g_lockedFreeFn = freeFn;
    return 1;
}

/* [302] 0x0137d0 */
LIBSSL_NOINLINE void sceSsl_lib_B0B90785(SslMallocFn *mallocOut, SslReallocFn *reallocOut,
                                         SslFreeFn *freeOut)
{
    if (mallocOut != NULL) {
        *mallocOut = g_mallocFn;
    }
    if (reallocOut != NULL) {
        *reallocOut = g_reallocFn;
    }
    if (freeOut != NULL) {
        *freeOut = g_freeFn;
    }
}

/* [303] 0x013808 */
LIBSSL_NOINLINE void sceSsl_lib_064F5DF3(SslMallocFn *mallocOut, SslFreeFn *freeOut)
{
    if (mallocOut != NULL) {
        *mallocOut = g_lockedMallocFn;
    }
    if (freeOut != NULL) {
        *freeOut = g_lockedFreeFn;
    }
}

/* [304] 0x013830 */
LIBSSL_NOINLINE void *sceSsl_lib_6053B79C(u32 size)
{
    return g_lockedMallocFn(size);
}

/* [305] 0x013854 */
LIBSSL_NOINLINE void sceSsl_lib_E5FC4992(void *ptr)
{
    g_lockedFreeFn(ptr);
}

/* [306] 0x013878 */
LIBSSL_NOINLINE void *sceSsl_lib_0861D934(u32 size)
{
    return g_mallocFn(size);
}

/* [307] 0x01389c */
LIBSSL_NOINLINE void *sslCryptoRealloc(void *ptr, u32 newSize)
{
    return g_reallocFn(ptr, newSize);
}

/* [308] 0x0138c0 */
LIBSSL_NOINLINE void sceSsl_lib_5E5C873A(void *ptr)
{
    g_freeFn(ptr);
}
