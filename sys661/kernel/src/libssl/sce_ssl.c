/*
 * Sony PSP sceSsl public API and platform glue (0x027DA0..0x028BDF).
 */
#include "libssl_internal.h"

typedef struct SslPlatformState {
    int initialized;
    int fplId;
    void *poolBase;
    SceSslLwMutexWorkarea lwMutex;
} SslPlatformState;

static SslPlatformState g_sslState;

static SceSslTimeSpec *LIBSSL_NOINLINE sslGetUtcTimeCallback(SceSslTimeSpec *outTime);
static int LIBSSL_NOINLINE sslConvertAsn1TimeToTick(const u8 *timeStr, u32 timeLen,
                                                    int asn1Tag, SceSslRtcTick *outTick);
static int LIBSSL_NOINLINE sslLockCallback(int mode, int type, const char *file, int line);

/* 0x027DA0 */
int LIBSSL_NOINLINE sceSslInit(u32 poolSize)
{
    int partitionId;
    int fplId;
    int ret;

    if (_sceKernelCheckThreadStack() < 976) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_sslState.initialized == 1) {
        return SCE_SSL_ALREADY_INITED;
    }
    if (poolSize == 0) {
        ret = SCE_SSL_INVALID_VALUE;
    } else {
        ret = sceKernelQueryMemoryInfo(&g_sslState.fplId, &partitionId, NULL);
        if (ret >= 0) {
            fplId = sceKernelCreateFpl("SceSsl", partitionId, 0, poolSize, 1, NULL);
            ret = fplId;
            if (fplId >= 0) {
                g_sslState.fplId = fplId;
                ret = sceKernelTryAllocateFpl(fplId, &g_sslState.poolBase);
                if (ret >= 0) {
                    ret = sceKernelCreateLwMutex(&g_sslState.lwMutex, "SceSsl", 512, 0, NULL);
                    if (ret >= 0) {
                        sceSsl_lib_A9191C17(sslLockCallback);
                        sceSsl_lib_9A789D37(g_sslState.poolBase, poolSize);
                        sceSsl_lib_9274BFE3();
                        sslSetTimeCallback(sslGetUtcTimeCallback);
                        g_sslState.initialized = 1;
                        return 0;
                    }
                }
            }
        }
    }

    if (g_sslState.poolBase != NULL) {
        sceKernelFreeFpl(g_sslState.fplId, g_sslState.poolBase);
        g_sslState.poolBase = NULL;
    }
    if (g_sslState.fplId != 0) {
        sceKernelDeleteFpl(g_sslState.fplId);
        g_sslState.fplId = 0;
    }
    return ret;
}

/* 0x027F30 */
int LIBSSL_NOINLINE sceSslEnd(void)
{
    if (_sceKernelCheckThreadStack() < 992) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_sslState.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }

    sceSsl_lib_CF78BEFB();
    if (g_sslState.poolBase != NULL) {
        sceKernelFreeFpl(g_sslState.fplId, g_sslState.poolBase);
        g_sslState.poolBase = NULL;
    }
    sceKernelDeleteLwMutex(&g_sslState.lwMutex);
    if (g_sslState.fplId != 0) {
        sceKernelDeleteFpl(g_sslState.fplId);
        g_sslState.fplId = 0;
    }
    sslPoolReset();
    g_sslState.initialized = 0;
    return 0;
}

/* 0x028004 */
int LIBSSL_NOINLINE sceSslGetUsedMemoryMax(u32 *maxMem)
{
    if (_sceKernelCheckThreadStack() < 1008) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_sslState.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (maxMem == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }
    *maxMem = sceSsl_lib_9266C0D5();
    return 0;
}

/* 0x028078 */
int LIBSSL_NOINLINE sceSslGetUsedMemoryCurrent(u32 *currentMem)
{
    if (_sceKernelCheckThreadStack() < 1008) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_sslState.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (currentMem == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }
    *currentMem = sslPoolGetUsedCurrent();
    return 0;
}

/* 0x0280EC */
int LIBSSL_NOINLINE sceSslGetSerialNumber(void *cert, const u8 **serialBuf, u32 *serialLen)
{
    int asn1Tag;
    const u8 *serialPtr;
    int ret;

    if (_sceKernelCheckThreadStack() < 992) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_sslState.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (cert == NULL || serialBuf == NULL || serialLen == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    ret = sceSsl_lib_9299C6CE(cert, &asn1Tag, &serialPtr, serialLen);
    if (ret < 0) {
        return SCE_SSL_INVALID_FORMAT;
    }
    if (ret == 0) {
        return SCE_SSL_NOT_FOUND;
    }
    *serialBuf = serialPtr;
    return ret;
}

/* 0x0281BC */
int LIBSSL_NOINLINE sceSslGetNameEntryCount(void *certName)
{
    int count;

    if (_sceKernelCheckThreadStack() < 1008) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_sslState.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (certName == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    count = sceSsl_lib_4C75B8E9(certName);
    if (count < 0) {
        return SCE_SSL_INVALID_FORMAT;
    }
    return count;
}

/* 0x028240 */
int LIBSSL_NOINLINE sceSslGetNameEntryInfo(void *certName, int entryNum, const char **oidName,
                                           const u8 **valueBuf, u32 *valueLen)
{
    void *entry;
    const u8 *oidData;
    u32 oidLen;
    int valueTag;
    const u8 *valPtr;

    if (_sceKernelCheckThreadStack() < 976) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_sslState.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (certName == NULL || oidName == NULL || valueBuf == NULL || valueLen == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    entry = sceSsl_lib_5040E836(certName, entryNum);
    if (entry == NULL) {
        return SCE_SSL_INVALID_FORMAT;
    }
    if (sceSsl_lib_8D484361(entry, NULL, &oidData, &oidLen, &valueTag, &valPtr, valueLen) == 0) {
        return SCE_SSL_INVALID_FORMAT;
    }

    *valueBuf = valPtr;
    *oidName = sceSsl_lib_2B7BD166(oidData, oidLen, 0);
    return 0;
}

/* 0x02835C */
int LIBSSL_NOINLINE sceSslGetNotAfter(void *cert, SceSslRtcTick *limitTick)
{
    u8 timeBuf[128];
    int asn1Tag;
    const u8 *timeData;
    u32 timeLen;
    int ret;

    if (_sceKernelCheckThreadStack() < 864) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_sslState.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (cert == NULL || limitTick == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    if (sceSsl_lib_05E019A4(cert, &asn1Tag, &timeData, &timeLen) == 0 || timeLen > 128) {
        return SCE_SSL_INVALID_FORMAT;
    }

    sslMemcpy(timeBuf, timeData, timeLen);
    ret = sslConvertAsn1TimeToTick(timeBuf, timeLen, asn1Tag, limitTick);
    return (ret < 0) ? ret : 0;
}

/* 0x028444 */
int LIBSSL_NOINLINE sceSslGetNotBefore(void *cert, SceSslRtcTick *limitTick)
{
    u8 timeBuf[128];
    int asn1Tag;
    const u8 *timeData;
    u32 timeLen;
    int ret;

    if (_sceKernelCheckThreadStack() < 864) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_sslState.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (cert == NULL || limitTick == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    if (sceSsl_lib_341843AB(cert, &asn1Tag, &timeData, &timeLen) == 0 || timeLen > 128) {
        return SCE_SSL_INVALID_FORMAT;
    }

    sslMemcpy(timeBuf, timeData, timeLen);
    ret = sslConvertAsn1TimeToTick(timeBuf, timeLen, asn1Tag, limitTick);
    return (ret < 0) ? ret : 0;
}

/* 0x02852C */
int LIBSSL_NOINLINE sceSslGetKeyUsage(void *cert, int *critical, u32 *usage)
{
    int ret;

    if (_sceKernelCheckThreadStack() < 864) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_sslState.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (cert == NULL || critical == NULL || usage == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    ret = sceSsl_lib_7781A781(cert, critical, usage);
    if (ret != 1) {
        return SCE_SSL_NOT_FOUND;
    }
    if (*critical != 0) {
        *critical = 1;
    }
    return 0;
}

/* 0x0285EC */
void *LIBSSL_NOINLINE sceSslGetSubjectName(void *cert)
{
    if (_sceKernelCheckThreadStack() < 1008) {
        return NULL;
    }
    if (g_sslState.initialized != 1 || cert == NULL) {
        return NULL;
    }
    return sceSsl_lib_12C7718F(cert);
}

/* 0x02864C */
void *LIBSSL_NOINLINE sceSslGetIssuerName(void *cert)
{
    if (_sceKernelCheckThreadStack() < 1008) {
        return NULL;
    }
    if (g_sslState.initialized != 1 || cert == NULL) {
        return NULL;
    }
    return sceSsl_lib_4BE9030A(cert);
}

/* 0x0286AC */
static SceSslTimeSpec *LIBSSL_NOINLINE sslGetUtcTimeCallback(SceSslTimeSpec *outTime)
{
    SceSslRtcDateTime clockTime;
    u64 epochSec;

    if (outTime == NULL) {
        return NULL;
    }
    if (sceRtcGetCurrentClock(&clockTime, 0) < 0) {
        return NULL;
    }
    if (sceRtcGetTime64_t(&clockTime, &epochSec) < 0) {
        return NULL;
    }

    outTime->secHigh = (u32)(epochSec >> 32);
    outTime->secLow = (u32)(epochSec & 0xFFFFFFFFU);
    outTime->nsec = 0;
    return outTime;
}

/* 0x02871C */
static int LIBSSL_NOINLINE sslConvertAsn1TimeToTick(const u8 *timeStr, u32 timeLen,
                                                    int asn1Tag, SceSslRtcTick *outTick)
{
    u32 epochPair[2];
    int formatType;
    u64 epochSec;
    u64 ticks;

    formatType = (asn1Tag == ASN1_TAG_UTCTIME) ? 1 : 2;
    if (sceSsl_lib_E7BFC0EF(timeStr, timeLen, formatType, (SceSslTimeSpec *)epochPair) != 0) {
        return SCE_SSL_INVALID_FORMAT;
    }

    epochSec = ((u64)epochPair[0] << 32) | (u64)epochPair[1];
    ticks = epochSec * 1000000ULL + 62135596800000000ULL;
    outTick->low = (u32)(ticks & 0xFFFFFFFFU);
    outTick->high = (u32)(ticks >> 32);
    return 0;
}

/* 0x0287BC */
static int LIBSSL_NOINLINE sslLockCallback(int mode, int type, const char *file, int line)
{
    int ret;

    (void)type;
    (void)file;
    (void)line;

    if ((mode & 1) != 0) {
        ret = sceKernelLockLwMutex(&g_sslState.lwMutex, 1, NULL);
    } else {
        ret = sceKernelUnlockLwMutex(&g_sslState.lwMutex, 1);
    }
    return (ret < 0) ? ret : 1;
}

/* 0x02881C */
void LIBSSL_NOINLINE sslQuickSortRange(void *basePtr, u32 elemSize, SslQsortCmpFn cmpFn,
                                       int lowIdx, int highIdx)
{
    u8 *bytes = (u8 *)basePtr;
    void *pivotBuf;
    void *swapBuf;
    int leftIdx = lowIdx;
    int rightIdx = highIdx;

    pivotBuf = sceSsl_lib_0861D934(elemSize);
    if (pivotBuf == NULL) {
        return;
    }
    swapBuf = sceSsl_lib_0861D934(elemSize);
    if (swapBuf != NULL) {
        int midIdx = (lowIdx + highIdx) / 2;
        sslMemcpy(pivotBuf, &bytes[(u32)midIdx * elemSize], elemSize);

        for (;;) {
            while (cmpFn(&bytes[(u32)leftIdx * elemSize], pivotBuf) < 0) {
                leftIdx++;
            }
            while (cmpFn(pivotBuf, &bytes[(u32)rightIdx * elemSize]) < 0) {
                rightIdx--;
            }
            if (rightIdx < leftIdx) {
                break;
            }
            if (leftIdx != rightIdx) {
                sslMemcpy(swapBuf, &bytes[(u32)leftIdx * elemSize], elemSize);
                sslMemcpy(&bytes[(u32)leftIdx * elemSize], &bytes[(u32)rightIdx * elemSize], elemSize);
                sslMemcpy(&bytes[(u32)rightIdx * elemSize], swapBuf, elemSize);
            }
            leftIdx++;
            rightIdx--;
            if (rightIdx < leftIdx) {
                break;
            }
        }

        if (lowIdx < rightIdx) {
            sslQuickSortRange(basePtr, elemSize, cmpFn, lowIdx, rightIdx);
        }
        if (leftIdx < highIdx) {
            sslQuickSortRange(basePtr, elemSize, cmpFn, leftIdx, highIdx);
        }
    }

    sceSsl_lib_5E5C873A(pivotBuf);
    if (swapBuf != NULL) {
        sceSsl_lib_5E5C873A(swapBuf);
    }
}

/* 0x028A40 */
void LIBSSL_NOINLINE sslQsort(void *basePtr, u32 elemCount, u32 elemSize, SslQsortCmpFn cmpFn)
{
    if (basePtr != NULL && cmpFn != NULL && elemCount != 0 && elemSize != 0) {
        sslQuickSortRange(basePtr, elemSize, cmpFn, 0, (int)elemCount - 1);
    }
}

/* 0x028A94 */
int LIBSSL_NOINLINE sslSprintf(char *outBuf, const char *fmt, ...)
{
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = sceNet_lib_8687B5AB(outBuf, fmt, &args);
    va_end(args);
    return ret;
}

/* 0x028AC8 */
int LIBSSL_NOINLINE sslMemcmp(const void *lhs, const void *rhs, int len)
{
    return sceNet_lib_E0A81C7C(lhs, rhs, len);
}

/* 0x028AE4 */
void *LIBSSL_NOINLINE sslMemcpy(void *dst, const void *src, u32 len)
{
    return sceKernelMemcpy(dst, src, len);
}

/* 0x028B00 */
void *LIBSSL_NOINLINE sslMemset(void *dst, int byteVal, u32 len)
{
    return sceKernelMemset(dst, byteVal, len);
}

/* 0x028B1C */
void *LIBSSL_NOINLINE sslMemmove(void *dst, const void *src, int len)
{
    return sceNet_lib_4753D878(dst, src, len);
}

/* 0x028B38 */
int LIBSSL_NOINLINE sslVsprintf(char *outBuf, const char *fmt, void *ap)
{
    return sceNet_lib_8687B5AB(outBuf, fmt, ap);
}

/* 0x028B54 */
int LIBSSL_NOINLINE sslStrlen(const char *str)
{
    return sceNet_lib_D8722983(str);
}

/* 0x028B70 */
int LIBSSL_NOINLINE sslStrcmp(const char *lhs, const char *rhs)
{
    return sceNet_lib_A0F16ABD(lhs, rhs);
}

/* 0x028B8C */
int LIBSSL_NOINLINE sslStrncmp(const char *lhs, const char *rhs, int len)
{
    return sceNet_lib_94DCA9F0(lhs, rhs, len);
}

/* 0x028BA8 */
char *LIBSSL_NOINLINE sslStrncpy(char *dst, const char *src, int len)
{
    return sceNet_lib_B5CE388A(dst, src, len);
}

/* 0x028BC4 */
char *LIBSSL_NOINLINE sslStrcat(char *dst, const char *src)
{
    return sceNet_lib_5705F6F9(dst, src);
}
