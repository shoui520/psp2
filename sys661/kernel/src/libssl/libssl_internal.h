/*
 * SceSsl / sceSsl_Module (ePSP 6.61) internal definitions.
 *
 * Wraps RSA BSAFE SSL-C 1.4.0 (SSLeay/OpenSSL-compatible TLS/SSLv3/X.509
 * library) with the PSP sceSsl API and fixed-pool memory allocator.
 */
#ifndef LIBSSL_INTERNAL_H
#define LIBSSL_INTERNAL_H

#include <stdarg.h>
#include "common_header.h"

/* sceSsl error codes (PSP 6.60 SDK libssl.h) */
#define SCE_SSL_ERROR_INSUFFICIENT_STACK    ((int)0x80410005)
#define SCE_SSL_BEFORE_INIT                 ((int)0x80435001)
#define SCE_SSL_ALREADY_INITED              ((int)0x80435020)
#define SCE_SSL_NOT_FOUND                   ((int)0x80435025)
#define SCE_SSL_INVALID_FORMAT              ((int)0x80435108)
#define SCE_SSL_INVALID_VALUE               ((int)0x804351FE)

/* SSL_get_error return codes */
#define SSL_ERROR_NONE                      0
#define SSL_ERROR_SSL                       1
#define SSL_ERROR_WANT_READ                 2
#define SSL_ERROR_WANT_WRITE                3
#define SSL_ERROR_WANT_X509_LOOKUP          4
#define SSL_ERROR_SYSCALL                   5
#define SSL_ERROR_ZERO_RETURN               6
#define SSL_ERROR_WANT_CONNECT              7

/* SSL shutdown flags */
#define SSL_SENT_SHUTDOWN                   1
#define SSL_RECEIVED_SHUTDOWN               2

/* SSL rwstate flags */
#define SSL_NOTHING                         1
#define SSL_WRITING                         2
#define SSL_READING                         3
#define SSL_X509_LOOKUP                     4

/* BIO control commands */
#define BIO_CTRL_RESET                      1
#define BIO_CTRL_EOF                        2
#define BIO_CTRL_INFO                       3
#define BIO_CTRL_SET                        4
#define BIO_CTRL_GET                        5
#define BIO_CTRL_PUSH                       6
#define BIO_CTRL_POP                        7
#define BIO_CTRL_GET_CLOSE                  8
#define BIO_CTRL_SET_CLOSE                  9
#define BIO_CTRL_PENDING                    10
#define BIO_CTRL_FLUSH                      11
#define BIO_CTRL_DUP                        12
#define BIO_CTRL_WPENDING                   13
#define BIO_C_SET_FD                        104
#define BIO_C_GET_FD                        105

/* ASN.1 tag types */
#define ASN1_TAG_INTEGER                    0x02
#define ASN1_TAG_BIT_STRING                 0x03
#define ASN1_TAG_OCTET_STRING               0x04
#define ASN1_TAG_NULL                       0x05
#define ASN1_TAG_OBJECT                     0x06
#define ASN1_TAG_UTF8STRING                 0x0C
#define ASN1_TAG_PRINTABLESTRING            0x13
#define ASN1_TAG_IA5STRING                  0x16
#define ASN1_TAG_UTCTIME                    0x17
#define ASN1_TAG_GENERALIZEDTIME            0x18
#define ASN1_TAG_SEQUENCE                   0x30
#define ASN1_TAG_SET                        0x31

/* Limits */
#define SSL_MAX_CERT_NAME_ENTRIES           16
#define SSL_MAX_CERT_CHAIN                  8
#define SSL_ERR_QUEUE_SIZE                  16

/* Imported kernel / network / RTC / crypto primitives */
typedef struct SceSslLwMutexWorkarea {
    u32 words[8];
} SceSslLwMutexWorkarea;

typedef struct SceSslRtcTick {
    u32 low;
    u32 high;
} SceSslRtcTick;

typedef struct SceSslRtcDateTime {
    u16 year;
    u16 month;
    u16 day;
    u16 hour;
    u16 minute;
    u16 second;
    u32 microsecond;
} SceSslRtcDateTime;

typedef struct SceSslTimeSpec {
    u32 secHigh;
    u32 secLow;
    u32 nsec;
} SceSslTimeSpec;

typedef struct SceSslMd5Ctx {
    u32 state[4];
    u32 count[2];
    u8 buffer[64];
} SceSslMd5Ctx;

typedef struct SceSslSha1Ctx {
    u32 state[5];
    u32 count[2];
    u8 buffer[64];
} SceSslSha1Ctx;

/* External imports matching module.yml */
extern void *sceKernelMemcpy(void *dst, const void *src, u32 len);
extern void *sceKernelMemset(void *dst, int val, u32 len);
extern int _sceKernelCheckThreadStack(void);
extern int sceKernelLockLwMutex(SceSslLwMutexWorkarea *workarea, int lockCount, u32 *timeout);
extern int sceKernelUnlockLwMutex(SceSslLwMutexWorkarea *workarea, int unlockCount);

extern int sceKernelCreateFpl(const char *name, int partitionId, u32 attr,
                              u32 blockSize, u32 numBlocks, void *opt);
extern int sceKernelDeleteFpl(int uid);
extern int sceKernelTryAllocateFpl(int uid, void **data);
extern int sceKernelFreeFpl(int uid, void *data);
extern int sceKernelCreateLwMutex(SceSslLwMutexWorkarea *workarea, const char *name,
                                  u32 attr, int initCount, void *opt);
extern int sceKernelDeleteLwMutex(SceSslLwMutexWorkarea *workarea);
extern int sceKernelQueryMemoryInfo(void *addr, int *partitionId, void *blockId);

extern int sceKernelUtilsMd5Digest(u8 *data, u32 size, u8 *digest);
extern int sceKernelUtilsMd5BlockInit(SceSslMd5Ctx *ctx);
extern int sceKernelUtilsMd5BlockUpdate(SceSslMd5Ctx *ctx, u8 *data, u32 size);
extern int sceKernelUtilsMd5BlockResult(SceSslMd5Ctx *ctx, u8 *digest);
extern int sceKernelUtilsSha1BlockInit(SceSslSha1Ctx *ctx);
extern int sceKernelUtilsSha1BlockUpdate(SceSslSha1Ctx *ctx, u8 *data, u32 size);
extern int sceKernelUtilsSha1BlockResult(SceSslSha1Ctx *ctx, u8 *digest);

extern int sceRtcGetCurrentClock(SceSslRtcDateTime *time, int tz);
extern int sceRtcGetTime64_t(const SceSslRtcDateTime *time, u64 *epochSec);

extern int sceNetInetSend(int sock, const void *buf, u32 len, int flags);
extern int sceNetInetRecv(int sock, void *buf, u32 len, int flags);
extern int sceNetInetShutdown(int sock, int how);

/* Fixed-pool memory allocator block header */
typedef struct SslPoolBlock {
    u32 size;
    u32 inUse;
    struct SslPoolBlock *next;
    struct SslPoolBlock *prev;
} SslPoolBlock;

typedef struct SslMemPool {
    u8 *base;
    u32 totalSize;
    u32 usedCurrent;
    u32 usedMax;
    SslPoolBlock *head;
    int initialized;
} SslMemPool;

/* Crypto allocator and lock callback signatures */
typedef void *(*SslMallocFn)(u32 size);
typedef void *(*SslReallocFn)(void *ptr, u32 size);
typedef void (*SslFreeFn)(void *ptr);
typedef int (*SslLockCb)(int mode, int type, const char *file, int line);
typedef SceSslTimeSpec *(*SslTimeCb)(SceSslTimeSpec *outTime);

/* RSA / EVP key structs */
typedef struct SslRsaKey {
    u8 *modulus;
    u32 modulusLen;
    u8 *exponent;
    u32 exponentLen;
    u8 *privExponent;
    u32 privExponentLen;
    int refCount;
} SslRsaKey;

typedef struct SslEvpPkey {
    int type;
    int saveType;
    int refCount;
    SslRsaKey *rsa;
} SslEvpPkey;

/* BIO structs */
typedef struct SslBio SslBio;

typedef struct SslBioMethod {
    int type;
    const char *name;
    int (*bwrite)(SslBio *bio, const char *buf, int len);
    int (*bread)(SslBio *bio, char *buf, int len);
    int (*bputs)(SslBio *bio, const char *str);
    int (*bgets)(SslBio *bio, char *buf, int len);
    int (*ctrl)(SslBio *bio, int cmd, int num, void *ptr);
    int (*create)(SslBio *bio);
    int (*destroy)(SslBio *bio);
} SslBioMethod;

struct SslBio {
    const SslBioMethod *method;
    void *callback;
    char *cbArg;
    int init;
    int shutdown;
    int flags;
    int retryReason;
    int num;
    void *ptr;
    SslBio *nextBio;
    SslBio *prevBio;
    int refCount;
    u32 numRead;
    u32 numWrite;
    u8 *memBuf;
    u32 memLen;
    u32 memPos;
    u32 memCap;
};

/* X.509 Tiny Cert structs */
typedef struct SslX509NameEntry {
    const u8 *oidData;
    u32 oidLen;
    int valueTag;
    const u8 *valueData;
    u32 valueLen;
} SslX509NameEntry;

typedef struct SslX509Name {
    int entryCount;
    SslX509NameEntry entries[SSL_MAX_CERT_NAME_ENTRIES];
    const u8 *rawDer;
    u32 rawDerLen;
} SslX509Name;

typedef struct SslX509Cert {
    u8 *derData;
    u32 derLen;
    int version;
    const u8 *serialData;
    u32 serialLen;
    int serialTag;
    const u8 *notBeforeData;
    u32 notBeforeLen;
    int notBeforeTag;
    const u8 *notAfterData;
    u32 notAfterLen;
    int notAfterTag;
    SslX509Name issuer;
    SslX509Name subject;
    SslEvpPkey *pubKey;
    int hasKeyUsage;
    int keyUsageCritical;
    u32 keyUsageBits;
    int hasBasicConstraints;
    int basicConstraintsCritical;
    int pathLenConstraint;
    int isCa;
    const u8 *tbsData;
    u32 tbsLen;
    const u8 *sigData;
    u32 sigLen;
    int sigAlgNid;
    int refCount;
} SslX509Cert;

typedef struct SslX509StoreCtx {
    SslX509Cert *cert;
    SslX509Cert *chain[SSL_MAX_CERT_CHAIN];
    int chainLen;
    int error;
    int errorDepth;
    void *appData;
} SslX509StoreCtx;

/* SSL / TLS protocol structs */
typedef struct SslConn SslConn;
typedef struct SslCtx SslCtx;

typedef struct SslCipher {
    int valid;
    const char *name;
    u32 id;
    u32 algorithms;
    int strengthBits;
    int algBits;
} SslCipher;

typedef struct SslMethod {
    int version;
    int (*sslNew)(SslConn *ssl);
    void (*sslClear)(SslConn *ssl);
    void (*sslFree)(SslConn *ssl);
    int (*sslAccept)(SslConn *ssl);
    int (*sslConnect)(SslConn *ssl);
    int (*sslRead)(SslConn *ssl, void *buf, int len);
    int (*sslPeek)(SslConn *ssl, void *buf, int len);
    int (*sslWrite)(SslConn *ssl, const void *buf, int len);
    int (*sslShutdown)(SslConn *ssl);
    int (*sslRenegotiate)(SslConn *ssl);
    int (*sslRenegotiateCheck)(SslConn *ssl);
    int (*sslCtrl)(SslConn *ssl, int cmd, int larg, void *parg);
    int (*sslCtxCtrl)(SslCtx *ctx, int cmd, int larg, void *parg);
    int (*sslPending)(const SslConn *ssl);
} SslMethod;

typedef struct SslSession {
    int sslVersion;
    u32 keyArgLength;
    u8 keyArg[8];
    int masterKeyLength;
    u8 masterKey[48];
    u32 sessionIdLength;
    u8 sessionId[32];
    SslX509Cert *peer;
    const SslCipher *cipher;
    u32 timeout;
    u32 time;
    int refCount;
    int verifyResult;
} SslSession;

typedef int (*SslVerifyCb)(int preverifyOk, SslX509StoreCtx *storeCtx);
typedef int (*SslCertVerifyCb)(SslX509StoreCtx *storeCtx, void *arg);

struct SslCtx {
    const SslMethod *method;
    u32 options;
    u32 mode;
    int verifyMode;
    int verifyDepth;
    SslVerifyCb verifyCallback;
    SslCertVerifyCb appVerifyCallback;
    void *appVerifyArg;
    SslX509Cert *clientCert;
    SslEvpPkey *clientKey;
    u32 sessionCacheMode;
    u32 sessionTimeout;
    int refCount;
    void *appData;
    int quietShutdown;
    u32 maxCertList;
    u32 readAhead;
};

struct SslConn {
    int version;
    int type;
    const SslMethod *method;
    SslBio *rbio;
    SslBio *wbio;
    SslBio *bbio;
    int rwstate;
    int inHandshake;
    int (*handshakeFunc)(SslConn *ssl);
    int server;
    int newSession;
    int quietShutdown;
    int shutdown;
    int state;
    int rstate;
    u32 options;
    u32 mode;
    int verifyMode;
    SslVerifyCb verifyCallback;
    SslSession *session;
    SslCtx *ctx;
    SslX509Cert *peerCert;
    SslX509Cert *peerChain[SSL_MAX_CERT_CHAIN];
    int peerChainLen;
    int verifyResult;
    int refCount;
    void *appData;
    int hit;
    int errorCode;
    u8 clientRandom[32];
    u8 serverRandom[32];
    u32 readAhead;
    u32 maxSendFragment;
    void *infoCallback;
    void *msgCallback;
    void *msgCallbackArg;
    int debug;
};

/* Error queue state */
typedef struct SslErrState {
    u32 errors[SSL_ERR_QUEUE_SIZE];
    int top;
    int bottom;
} SslErrState;

/* Global state shared across translation units */
typedef struct SslGlobals {
    int initialized;
    int fplId;
    void *poolBase;
    SceSslLwMutexWorkarea lwMutex;
    SslMemPool pool;
    SslMallocFn mallocFn;
    SslReallocFn reallocFn;
    SslFreeFn freeFn;
    SslMallocFn lockedMallocFn;
    SslFreeFn lockedFreeFn;
    SslLockCb lockCallback;
    SslTimeCb timeCallback;
    SslErrState errState;
    u32 randSeed;
} SslGlobals;

extern SslGlobals g_ssl;

/* Internal cross-file helpers */
void sslPoolReset(void);
u32 sslPoolGetUsedCurrent(void);
u32 sslPoolGetUsedMax(void);
void *sslPoolAlloc(u32 size);
void *sslPoolRealloc(void *ptr, u32 size);
void sslPoolFree(void *ptr);
void sslSetTimeCallback(SslTimeCb cb);
SceSslTimeSpec *sslGetCurrentTimeSpec(SceSslTimeSpec *outTime);
void sslErrPush(int lib, int func, int reason);
int sslParseAsn1TimeToEpoch(const u8 *str, u32 len, int isGeneralized, u64 *outEpoch);

/* Exported sceSsl_lib helpers referenced across translation units */
int sceSsl_lib_C85F6FCE(int mode, u8 *outBuf, u32 outLen);
void *sceSsl_lib_0861D934(u32 size);
void sceSsl_lib_5E5C873A(void *ptr);
void *sceSsl_lib_9A789D37(void *poolMem, u32 poolSize);
u32 sceSsl_lib_9266C0D5(void);
void *sceSsl_lib_A9191C17(SslLockCb cb);
int sceSsl_lib_9274BFE3(void);
int sceSsl_lib_CF78BEFB(void);
int sceSsl_lib_E7BFC0EF(const u8 *str, u32 len, int formatType, u32 *outEpochPair);
int sceSsl_lib_9299C6CE(SslX509Cert *cert, int *outTag, const u8 **outData, u32 *outLen);
int sceSsl_lib_05E019A4(SslX509Cert *cert, int *outTag, const u8 **outData, u32 *outLen);
int sceSsl_lib_341843AB(SslX509Cert *cert, int *outTag, const u8 **outData, u32 *outLen);
int sceSsl_lib_4C75B8E9(SslX509Name *name);
SslX509NameEntry *sceSsl_lib_5040E836(SslX509Name *name, int index);
int sceSsl_lib_8D484361(SslX509NameEntry *entry, int *outSet, const u8 **outOid,
                        u32 *outOidLen, int *outTag, const u8 **outVal, u32 *outValLen);
const char *sceSsl_lib_2B7BD166(const u8 *oidData, u32 oidLen, int shortName);
int sceSsl_lib_7781A781(SslX509Cert *cert, int *outCritical, u32 *outUsage);
SslX509Name *sceSsl_lib_12C7718F(SslX509Cert *cert);
SslX509Name *sceSsl_lib_4BE9030A(SslX509Cert *cert);

#endif /* LIBSSL_INTERNAL_H */
