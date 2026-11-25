/*
 * SSL-C memory pool allocator, crypto lock hooks, EVP_PKEY, and ASN.1 time helpers.
 *
 * Corresponds to Sony's crypto/e_malloc.c and common/evp/p_lib.c in libssl.prx.
 */
#include "libssl_internal.h"

static void sslLockMem(void)
{
    if (g_ssl.lockCallback != NULL) {
        g_ssl.lockCallback(1, 0, "crypto/e_malloc.c", 0);
    }
}

static void sslUnlockMem(void)
{
    if (g_ssl.lockCallback != NULL) {
        g_ssl.lockCallback(2, 0, "crypto/e_malloc.c", 0);
    }
}

void sslSetTimeCallback(SslTimeCb cb)
{
    g_ssl.timeCallback = cb;
}

SceSslTimeSpec *sslGetCurrentTimeSpec(SceSslTimeSpec *outTime)
{
    if (g_ssl.timeCallback != NULL) {
        return g_ssl.timeCallback(outTime);
    }
    return NULL;
}

void sslPoolReset(void)
{
    g_ssl.pool.base = NULL;
    g_ssl.pool.totalSize = 0;
    g_ssl.pool.usedCurrent = 0;
    g_ssl.pool.usedMax = 0;
    g_ssl.pool.head = NULL;
    g_ssl.pool.initialized = 0;
}

u32 sslPoolGetUsedCurrent(void)
{
    return g_ssl.pool.usedCurrent;
}

u32 sslPoolGetUsedMax(void)
{
    return g_ssl.pool.usedMax;
}

void *sslPoolAlloc(u32 size)
{
    SslPoolBlock *blk;
    u32 alignedSize;
    u32 hdrSize;

    if (!g_ssl.pool.initialized || size == 0) {
        return NULL;
    }

    hdrSize = (u32)sizeof(SslPoolBlock);
    alignedSize = (size + 7U) & ~7U;

    sslLockMem();
    blk = g_ssl.pool.head;
    while (blk != NULL) {
        if (!blk->inUse && blk->size >= alignedSize) {
            if (blk->size >= alignedSize + hdrSize + 16U) {
                SslPoolBlock *split = (SslPoolBlock *)((u8 *)blk + hdrSize + alignedSize);
                split->size = blk->size - alignedSize - hdrSize;
                split->inUse = 0;
                split->next = blk->next;
                split->prev = blk;
                if (blk->next != NULL) {
                    blk->next->prev = split;
                }
                blk->next = split;
                blk->size = alignedSize;
            }
            blk->inUse = 1;
            g_ssl.pool.usedCurrent += blk->size + hdrSize;
            if (g_ssl.pool.usedCurrent > g_ssl.pool.usedMax) {
                g_ssl.pool.usedMax = g_ssl.pool.usedCurrent;
            }
            sslUnlockMem();
            return (void *)((u8 *)blk + hdrSize);
        }
        blk = blk->next;
    }
    sslUnlockMem();
    return NULL;
}

void sslPoolFree(void *ptr)
{
    SslPoolBlock *blk;
    u32 hdrSize;

    if (!g_ssl.pool.initialized || ptr == NULL) {
        return;
    }

    hdrSize = (u32)sizeof(SslPoolBlock);
    blk = (SslPoolBlock *)((u8 *)ptr - hdrSize);

    sslLockMem();
    if (blk->inUse) {
        blk->inUse = 0;
        if (g_ssl.pool.usedCurrent >= blk->size + hdrSize) {
            g_ssl.pool.usedCurrent -= blk->size + hdrSize;
        } else {
            g_ssl.pool.usedCurrent = 0;
        }

        /* Coalesce with next free block */
        if (blk->next != NULL && !blk->next->inUse) {
            SslPoolBlock *nextBlk = blk->next;
            blk->size += hdrSize + nextBlk->size;
            blk->next = nextBlk->next;
            if (nextBlk->next != NULL) {
                nextBlk->next->prev = blk;
            }
        }
        /* Coalesce with previous free block */
        if (blk->prev != NULL && !blk->prev->inUse) {
            SslPoolBlock *prevBlk = blk->prev;
            prevBlk->size += hdrSize + blk->size;
            prevBlk->next = blk->next;
            if (blk->next != NULL) {
                blk->next->prev = prevBlk;
            }
        }
    }
    sslUnlockMem();
}

void *sslPoolRealloc(void *ptr, u32 size)
{
    SslPoolBlock *blk;
    void *newPtr;
    u32 copyLen;

    if (ptr == NULL) {
        return sslPoolAlloc(size);
    }
    if (size == 0) {
        sslPoolFree(ptr);
        return NULL;
    }

    blk = (SslPoolBlock *)((u8 *)ptr - sizeof(SslPoolBlock));
    if (blk->size >= size) {
        return ptr;
    }

    newPtr = sslPoolAlloc(size);
    if (newPtr == NULL) {
        return NULL;
    }
    copyLen = (blk->size < size) ? blk->size : size;
    sceKernelMemcpy(newPtr, ptr, copyLen);
    sslPoolFree(ptr);
    return newPtr;
}

/* Pool initialization export (sceSsl_lib_9A789D37) */
void *sceSsl_lib_9A789D37(void *poolMem, u32 poolSize)
{
    SslPoolBlock *initial;

    if (poolMem == NULL || poolSize <= sizeof(SslPoolBlock) + 16U) {
        return NULL;
    }

    g_ssl.pool.base = (u8 *)poolMem;
    g_ssl.pool.totalSize = poolSize;
    g_ssl.pool.usedCurrent = 0;
    g_ssl.pool.usedMax = 0;
    initial = (SslPoolBlock *)poolMem;
    initial->size = poolSize - (u32)sizeof(SslPoolBlock);
    initial->inUse = 0;
    initial->next = NULL;
    initial->prev = NULL;
    g_ssl.pool.head = initial;
    g_ssl.pool.initialized = 1;

    g_ssl.mallocFn = sslPoolAlloc;
    g_ssl.reallocFn = sslPoolRealloc;
    g_ssl.freeFn = sslPoolFree;
    g_ssl.lockedMallocFn = sslPoolAlloc;
    g_ssl.lockedFreeFn = sslPoolFree;
    return poolMem;
}

/* Peak memory usage query (sceSsl_lib_9266C0D5) */
u32 sceSsl_lib_9266C0D5(void)
{
    return g_ssl.pool.usedMax;
}

/* CRYPTO_set_mem_functions */
int sceSsl_lib_A2619268(SslMallocFn mfn, SslReallocFn rfn, SslFreeFn ffn)
{
    if (mfn == NULL || rfn == NULL || ffn == NULL) {
        return 0;
    }
    g_ssl.mallocFn = mfn;
    g_ssl.reallocFn = rfn;
    g_ssl.freeFn = ffn;
    g_ssl.lockedMallocFn = mfn;
    g_ssl.lockedFreeFn = ffn;
    return 1;
}

/* CRYPTO_get_mem_functions */
void sceSsl_lib_B0B90785(SslMallocFn *mfn, SslReallocFn *rfn, SslFreeFn *ffn)
{
    if (mfn != NULL) {
        *mfn = g_ssl.mallocFn;
    }
    if (rfn != NULL) {
        *rfn = g_ssl.reallocFn;
    }
    if (ffn != NULL) {
        *ffn = g_ssl.freeFn;
    }
}

/* CRYPTO_get_locked_mem_functions */
void sceSsl_lib_064F5DF3(SslMallocFn *mfn, SslFreeFn *ffn)
{
    if (mfn != NULL) {
        *mfn = g_ssl.lockedMallocFn;
    }
    if (ffn != NULL) {
        *ffn = g_ssl.lockedFreeFn;
    }
}

/* CRYPTO_malloc_locked */
void *sceSsl_lib_6053B79C(u32 size)
{
    return (g_ssl.lockedMallocFn != NULL) ? g_ssl.lockedMallocFn(size) : sslPoolAlloc(size);
}

/* CRYPTO_free_locked */
void sceSsl_lib_E5FC4992(void *ptr)
{
    if (g_ssl.lockedFreeFn != NULL) {
        g_ssl.lockedFreeFn(ptr);
    } else {
        sslPoolFree(ptr);
    }
}

/* CRYPTO_malloc */
void *sceSsl_lib_0861D934(u32 size)
{
    return (g_ssl.mallocFn != NULL) ? g_ssl.mallocFn(size) : sslPoolAlloc(size);
}

/* CRYPTO_free */
void sceSsl_lib_5E5C873A(void *ptr)
{
    if (g_ssl.freeFn != NULL) {
        g_ssl.freeFn(ptr);
    } else {
        sslPoolFree(ptr);
    }
}

/* CRYPTO_set_locking_callback and lock helpers */
void *sceSsl_lib_A9191C17(SslLockCb cb)
{
    g_ssl.lockCallback = cb;
    return (void *)cb;
}

void *sceSsl_lib_64AA4ED6(int lockId)
{
    (void)lockId;
    return (void *)g_ssl.lockCallback;
}

int sceSsl_lib_6CDEB823(void)
{
    return 32;
}

void *sceSsl_lib_5AD14F0B(int mode)
{
    (void)mode;
    return (void *)g_ssl.lockCallback;
}

int sceSsl_lib_9D8A385E(void)
{
    return 1;
}

SslLockCb sceSsl_lib_D84E2411(void)
{
    return g_ssl.lockCallback;
}

int sceSsl_lib_6BA650E4(void)
{
    return 1;
}

int sceSsl_lib_69105335(void)
{
    return 1;
}

/* EVP_PKEY_new / EVP_PKEY_size / EVP_PKEY_free (common/evp/p_lib.c) */
SslEvpPkey *sceSsl_lib_09C45275(void)
{
    SslEvpPkey *pkey = (SslEvpPkey *)sceSsl_lib_0861D934(sizeof(SslEvpPkey));
    if (pkey == NULL) {
        return NULL;
    }
    sceKernelMemset(pkey, 0, sizeof(SslEvpPkey));
    pkey->refCount = 1;
    return pkey;
}

int sceSsl_lib_FAF333D1(SslEvpPkey *pkey)
{
    if (pkey == NULL || pkey->rsa == NULL) {
        return 0;
    }
    return (int)pkey->rsa->modulusLen;
}

void sceSsl_lib_7317C1BC(SslEvpPkey *pkey)
{
    if (pkey == NULL) {
        return;
    }
    pkey->refCount--;
    if (pkey->refCount > 0) {
        return;
    }
    if (pkey->rsa != NULL) {
        if (pkey->rsa->modulus != NULL) {
            sceSsl_lib_5E5C873A(pkey->rsa->modulus);
        }
        if (pkey->rsa->exponent != NULL) {
            sceSsl_lib_5E5C873A(pkey->rsa->exponent);
        }
        if (pkey->rsa->privExponent != NULL) {
            sceSsl_lib_5E5C873A(pkey->rsa->privExponent);
        }
        sceSsl_lib_5E5C873A(pkey->rsa);
    }
    sceSsl_lib_5E5C873A(pkey);
}

/* Base64 / hex / digest helper exported at 0x028d4 */
int sceSsl_lib_C85F6FCE(int mode, u8 *outBuf, u32 outLen)
{
    u32 idx;

    if (outBuf == NULL || outLen == 0) {
        return 0;
    }
    for (idx = 0; idx < outLen; idx++) {
        g_ssl.randSeed = g_ssl.randSeed * 1103515245U + 12345U + (u32)mode;
        outBuf[idx] = (u8)((g_ssl.randSeed >> 16) & 0xFFU);
    }
    return 1;
}

/* ASN.1 time parsing helper */
static int sslReadTwoDigits(const u8 *ptr, int *outVal)
{
    if (ptr[0] < '0' || ptr[0] > '9' || ptr[1] < '0' || ptr[1] > '9') {
        return -1;
    }
    *outVal = (int)(ptr[0] - '0') * 10 + (int)(ptr[1] - '0');
    return 0;
}

static int sslIsLeapYear(int year)
{
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

int sslParseAsn1TimeToEpoch(const u8 *str, u32 len, int isGeneralized, u64 *outEpoch)
{
    static const int monthDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int year, month, day, hour, minute, second;
    int yrHi, yrLo;
    u32 pos;
    u64 totalDays;
    int yr, mo;

    if (str == NULL || outEpoch == NULL) {
        return -1;
    }

    pos = 0;
    if (isGeneralized) {
        if (len < 14) {
            return -1;
        }
        if (sslReadTwoDigits(str + 0, &yrHi) < 0 || sslReadTwoDigits(str + 2, &yrLo) < 0) {
            return -1;
        }
        year = yrHi * 100 + yrLo;
        pos = 4;
    } else {
        if (len < 12) {
            return -1;
        }
        if (sslReadTwoDigits(str + 0, &yrLo) < 0) {
            return -1;
        }
        year = (yrLo >= 50) ? (1900 + yrLo) : (2000 + yrLo);
        pos = 2;
    }

    if (sslReadTwoDigits(str + pos + 0, &month) < 0 ||
        sslReadTwoDigits(str + pos + 2, &day) < 0 ||
        sslReadTwoDigits(str + pos + 4, &hour) < 0 ||
        sslReadTwoDigits(str + pos + 6, &minute) < 0 ||
        sslReadTwoDigits(str + pos + 8, &second) < 0) {
        return -1;
    }

    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 60) {
        return -1;
    }

    totalDays = 0;
    if (year >= 1970) {
        for (yr = 1970; yr < year; yr++) {
            totalDays += sslIsLeapYear(yr) ? 366ULL : 365ULL;
        }
    }
    for (mo = 1; mo < month; mo++) {
        totalDays += (u64)monthDays[mo - 1];
        if (mo == 2 && sslIsLeapYear(year)) {
            totalDays += 1ULL;
        }
    }
    totalDays += (u64)(day - 1);

    *outEpoch = totalDays * 86400ULL + (u64)hour * 3600ULL + (u64)minute * 60ULL + (u64)second;
    return 0;
}

/* ASN.1 time / generalized time conversion exports */
int sceSsl_lib_2FE7BB42(void)
{
    return 1;
}

SceSslTimeSpec *sceSsl_lib_A7BED83B(SceSslTimeSpec *outTime)
{
    return sslGetCurrentTimeSpec(outTime);
}

int *sceSsl_lib_665D02F6(int *dst, const int *src, int cmpMode, int flags)
{
    (void)cmpMode;
    (void)flags;
    if (dst != NULL && src != NULL) {
        dst[0] = src[0];
        dst[1] = src[1];
    }
    return dst;
}

int sceSsl_lib_9EF66756(const int *lhs, int rhs, int cmpMode, int flags)
{
    (void)cmpMode;
    (void)flags;
    if (lhs == NULL) {
        return -1;
    }
    if (*lhs < rhs) {
        return -1;
    }
    return (*lhs > rhs) ? 1 : 0;
}

int sceSsl_lib_E7BFC0EF(const u8 *str, u32 len, int formatType, u32 *outEpochPair)
{
    u64 epochSec;

    if (outEpochPair == NULL) {
        return -1;
    }
    if (sslParseAsn1TimeToEpoch(str, len, (formatType == 2), &epochSec) < 0) {
        return -1;
    }
    outEpochPair[0] = (u32)(epochSec >> 32);
    outEpochPair[1] = (u32)(epochSec & 0xFFFFFFFFU);
    return 0;
}

int sceSsl_lib_047AB6BB(const u32 *timeA, const u32 *timeB)
{
    u64 valA, valB;

    if (timeA == NULL || timeB == NULL) {
        return 0;
    }
    valA = ((u64)timeA[0] << 32) | (u64)timeA[1];
    valB = ((u64)timeB[0] << 32) | (u64)timeB[1];
    if (valA < valB) {
        return -1;
    }
    return (valA > valB) ? 1 : 0;
}

void *sceSsl_lib_A920B39E(int type, void **outPtr, int len, int flags)
{
    void *buf;

    (void)type;
    (void)flags;
    if (len <= 0) {
        return NULL;
    }
    buf = sceSsl_lib_0861D934((u32)len);
    if (buf != NULL) {
        sceKernelMemset(buf, 0, (u32)len);
        if (outPtr != NULL) {
            *outPtr = buf;
        }
    }
    return buf;
}

int sceSsl_lib_3454E622(int mode, int arg, ...)
{
    (void)mode;
    (void)arg;
    return 0;
}
