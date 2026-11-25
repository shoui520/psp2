/*
 * Sony PSP sceSsl public API and module entry points.
 *
 * Implements the 12 public sceSsl exports (initialization, memory usage
 * queries, X.509 certificate inspection helpers) over the SSL-C core.
 */
#include "libssl_internal.h"

SslGlobals g_ssl;

static SceSslTimeSpec *sslGetUtcTimeCallback(SceSslTimeSpec *outTime)
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

static int sslConvertAsn1TimeToTick(const u8 *timeStr, u32 timeLen, int asn1Tag, SceSslRtcTick *outTick)
{
    u32 epochPair[2];
    int formatType;
    u64 epochSec;
    u64 ticks;

    formatType = (asn1Tag == ASN1_TAG_UTCTIME) ? 1 : 2;
    if (sceSsl_lib_E7BFC0EF(timeStr, timeLen, formatType, epochPair) != 0) {
        return SCE_SSL_INVALID_FORMAT;
    }

    /* Convert Unix epoch seconds to PSP RTC ticks (microseconds since 0001-01-01). */
    epochSec = ((u64)epochPair[0] << 32) | (u64)epochPair[1];
    ticks = epochSec * 1000000ULL + 62135596800000000ULL;
    outTick->low = (u32)(ticks & 0xFFFFFFFFU);
    outTick->high = (u32)(ticks >> 32);
    return 0;
}

static int sslLockCallback(int mode, int type, const char *file, int line)
{
    int ret;

    (void)type;
    (void)file;
    (void)line;

    if ((mode & 1) != 0) {
        ret = sceKernelLockLwMutex(&g_ssl.lwMutex, 1, NULL);
    } else {
        ret = sceKernelUnlockLwMutex(&g_ssl.lwMutex, 1);
    }
    return (ret < 0) ? ret : 1;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return 0;
}

int sceSslInit(u32 poolSize)
{
    int partitionId;
    int fplId;
    int ret;

    if (_sceKernelCheckThreadStack() < 976) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_ssl.initialized == 1) {
        return SCE_SSL_ALREADY_INITED;
    }
    if (poolSize == 0) {
        return SCE_SSL_INVALID_VALUE;
    }

    ret = sceKernelQueryMemoryInfo(&g_ssl.fplId, &partitionId, NULL);
    if (ret < 0) {
        return ret;
    }

    fplId = sceKernelCreateFpl("SceSsl", partitionId, 0, poolSize, 1, NULL);
    if (fplId < 0) {
        return fplId;
    }
    g_ssl.fplId = fplId;

    ret = sceKernelTryAllocateFpl(fplId, &g_ssl.poolBase);
    if (ret < 0) {
        sceKernelDeleteFpl(g_ssl.fplId);
        g_ssl.fplId = 0;
        return ret;
    }

    ret = sceKernelCreateLwMutex(&g_ssl.lwMutex, "SceSsl", 0x200, 0, NULL);
    if (ret < 0) {
        if (g_ssl.poolBase != NULL) {
            sceKernelFreeFpl(g_ssl.fplId, g_ssl.poolBase);
            g_ssl.poolBase = NULL;
        }
        if (g_ssl.fplId != 0) {
            sceKernelDeleteFpl(g_ssl.fplId);
            g_ssl.fplId = 0;
        }
        return ret;
    }

    sceSsl_lib_A9191C17(sslLockCallback);
    sceSsl_lib_9A789D37(g_ssl.poolBase, poolSize);
    sceSsl_lib_9274BFE3();
    sslSetTimeCallback(sslGetUtcTimeCallback);
    g_ssl.initialized = 1;
    return 0;
}

int sceSslEnd(void)
{
    if (_sceKernelCheckThreadStack() < 992) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_ssl.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }

    sceSsl_lib_CF78BEFB();
    if (g_ssl.poolBase != NULL) {
        sceKernelFreeFpl(g_ssl.fplId, g_ssl.poolBase);
        g_ssl.poolBase = NULL;
    }
    sceKernelDeleteLwMutex(&g_ssl.lwMutex);
    if (g_ssl.fplId != 0) {
        sceKernelDeleteFpl(g_ssl.fplId);
        g_ssl.fplId = 0;
    }
    sslPoolReset();
    g_ssl.initialized = 0;
    return 0;
}

int sceSslGetUsedMemoryMax(u32 *maxMem)
{
    if (_sceKernelCheckThreadStack() < 1008) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_ssl.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (maxMem == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    *maxMem = sceSsl_lib_9266C0D5();
    return 0;
}

int sceSslGetUsedMemoryCurrent(u32 *currentMem)
{
    if (_sceKernelCheckThreadStack() < 1008) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_ssl.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (currentMem == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    *currentMem = sslPoolGetUsedCurrent();
    return 0;
}

int sceSslGetSerialNumber(void *cert, const u8 **serialBuf, u32 *serialLen)
{
    int asn1Tag;
    const u8 *serialPtr;
    int ret;

    if (_sceKernelCheckThreadStack() < 992) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_ssl.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (cert == NULL || serialBuf == NULL || serialLen == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    ret = sceSsl_lib_9299C6CE((SslX509Cert *)cert, &asn1Tag, &serialPtr, serialLen);
    if (ret < 0) {
        return SCE_SSL_INVALID_FORMAT;
    }
    if (ret == 0) {
        return SCE_SSL_NOT_FOUND;
    }

    *serialBuf = serialPtr;
    return ret;
}

int sceSslGetNameEntryCount(void *certName)
{
    int count;

    if (_sceKernelCheckThreadStack() < 1008) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_ssl.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (certName == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    count = sceSsl_lib_4C75B8E9((SslX509Name *)certName);
    if (count < 0) {
        return SCE_SSL_INVALID_FORMAT;
    }
    return count;
}

int sceSslGetNameEntryInfo(void *certName, int entryNum, const char **oidName,
                           const u8 **valueBuf, u32 *valueLen)
{
    SslX509NameEntry *entry;
    const u8 *oidData;
    u32 oidLen;
    int valueTag;
    const u8 *valPtr;

    if (_sceKernelCheckThreadStack() < 976) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_ssl.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (certName == NULL || oidName == NULL || valueBuf == NULL || valueLen == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    entry = sceSsl_lib_5040E836((SslX509Name *)certName, entryNum);
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

int sceSslGetNotAfter(void *cert, SceSslRtcTick *limitTick)
{
    u8 timeBuf[128];
    int asn1Tag;
    const u8 *timeData;
    u32 timeLen;
    int ret;

    if (_sceKernelCheckThreadStack() < 864) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_ssl.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (cert == NULL || limitTick == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    if (sceSsl_lib_05E019A4((SslX509Cert *)cert, &asn1Tag, &timeData, &timeLen) == 0 || timeLen > 128) {
        return SCE_SSL_INVALID_FORMAT;
    }

    sceKernelMemcpy(timeBuf, timeData, timeLen);
    ret = sslConvertAsn1TimeToTick(timeBuf, timeLen, asn1Tag, limitTick);
    return (ret < 0) ? ret : 0;
}

int sceSslGetNotBefore(void *cert, SceSslRtcTick *limitTick)
{
    u8 timeBuf[128];
    int asn1Tag;
    const u8 *timeData;
    u32 timeLen;
    int ret;

    if (_sceKernelCheckThreadStack() < 864) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_ssl.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (cert == NULL || limitTick == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    if (sceSsl_lib_341843AB((SslX509Cert *)cert, &asn1Tag, &timeData, &timeLen) == 0 || timeLen > 128) {
        return SCE_SSL_INVALID_FORMAT;
    }

    sceKernelMemcpy(timeBuf, timeData, timeLen);
    ret = sslConvertAsn1TimeToTick(timeBuf, timeLen, asn1Tag, limitTick);
    return (ret < 0) ? ret : 0;
}

int sceSslGetKeyUsage(void *cert, int *critical, u32 *usage)
{
    int ret;

    if (_sceKernelCheckThreadStack() < 864) {
        return SCE_SSL_ERROR_INSUFFICIENT_STACK;
    }
    if (g_ssl.initialized != 1) {
        return SCE_SSL_BEFORE_INIT;
    }
    if (cert == NULL || critical == NULL || usage == NULL) {
        return SCE_SSL_INVALID_VALUE;
    }

    ret = sceSsl_lib_7781A781((SslX509Cert *)cert, critical, usage);
    if (ret != 1) {
        return SCE_SSL_NOT_FOUND;
    }
    if (*critical != 0) {
        *critical = 1;
    }
    return 0;
}

void *sceSslGetSubjectName(void *cert)
{
    if (_sceKernelCheckThreadStack() < 1008) {
        return NULL;
    }
    if (g_ssl.initialized != 1 || cert == NULL) {
        return NULL;
    }
    return sceSsl_lib_12C7718F((SslX509Cert *)cert);
}

void *sceSslGetIssuerName(void *cert)
{
    if (_sceKernelCheckThreadStack() < 1008) {
        return NULL;
    }
    if (g_ssl.initialized != 1 || cert == NULL) {
        return NULL;
    }
    return sceSsl_lib_4BE9030A((SslX509Cert *)cert);
}
