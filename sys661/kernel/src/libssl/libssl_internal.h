/*
 * SceSsl / sceSsl_Module (ePSP 6.61) internal definitions.
 *
 * Implements RSA BSAFE SSL-C 1.4.0 Patch 19 (SSLeay-derived TLSv1/SSLv3/X509TC
 * library), FIPS 180-2 SHA-224/256/384/512, and the PSP sceSsl API over a
 * fixed-pool memory allocator and lightweight mutex locking.
 */
#ifndef LIBSSL_INTERNAL_H
#define LIBSSL_INTERNAL_H

#include <stdarg.h>
#include <stddef.h>
#include "common_header.h"

#define LIBSSL_NOINLINE __attribute__((noinline))

/* sceSsl error codes (PSP 6.60 SDK libssl.h) */
#define SCE_SSL_ERROR_INSUFFICIENT_STACK    ((int)0x80410005)
#define SCE_SSL_BEFORE_INIT                 ((int)0x80435001)
#define SCE_SSL_ALREADY_INITED              ((int)0x80435020)
#define SCE_SSL_NOT_FOUND                   ((int)0x80435025)
#define SCE_SSL_INVALID_FORMAT              ((int)0x80435108)
#define SCE_SSL_INVALID_VALUE               ((int)0x804351FE)

/* SHA-2 error codes */
#define SCE_SHA_ERROR_INVALID_POINTER       ((int)0x80000103)
#define SCE_SHA_ERROR_INVALID_STATE         ((int)0x80000021)

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

/* Forward declarations */
typedef struct SslAsn1String SslAsn1String;
typedef struct SslAsn1Object SslAsn1Object;
typedef struct SslAsn1Type SslAsn1Type;
typedef struct SslAsn1Ctx SslAsn1Ctx;
typedef struct SslX509Algor SslX509Algor;
typedef struct SslX509Sig SslX509Sig;
typedef struct SslBufMem SslBufMem;
typedef struct SslStack SslStack;
typedef struct SslCryptoExData SslCryptoExData;
typedef struct SslCryptoExDataFuncs SslCryptoExDataFuncs;
typedef struct SslLhashNode SslLhashNode;
typedef struct SslLhash SslLhash;
typedef struct SslEvpCipher SslEvpCipher;
typedef struct SslEvpCipherCtx SslEvpCipherCtx;
typedef struct SslEvpMd SslEvpMd;
typedef struct SslEvpMdCtx SslEvpMdCtx;
typedef struct SslEvpPkey SslEvpPkey;
typedef struct SslEvpEncodeCtx SslEvpEncodeCtx;
typedef struct SslRandMethod SslRandMethod;
typedef struct SslRsaMethod SslRsaMethod;
typedef struct SslRsa SslRsa;
typedef struct SslBioMethod SslBioMethod;
typedef struct SslBio SslBio;
typedef struct SslX509TcName SslX509TcName;
typedef struct SslX509TcCert SslX509TcCert;
typedef struct SslHmacCtx SslHmacCtx;
typedef struct SslErrState SslErrState;
typedef struct SslCipher SslCipher;
typedef struct SslMethod SslMethod;
typedef struct SslEncMethod SslEncMethod;
typedef struct SslCertPkey SslCertPkey;
typedef struct SslCert SslCert;
typedef struct SslSession SslSession;
typedef struct Ssl3Record Ssl3Record;
typedef struct Ssl3State Ssl3State;
typedef struct SslCtxStats SslCtxStats;
typedef struct SslCtx SslCtx;
typedef struct Ssl Ssl;

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

typedef struct SceSha256Context {
    u32 state[8];
    u32 padWord;
    u16 remains;
    u16 computed;
    u64 totalLen;
    u8 blockBuf[64];
} SceSha256Context;

typedef struct SceSha512Context {
    u64 state[8];
    u32 padWord;
    u16 remains;
    u16 computed;
    u64 totalLen;
    u8 blockBuf[128];
} SceSha512Context;

/* RSA-COMMON ASN.1 BER/DER tree structures */
typedef struct SslAsn1Element {
    u32 length;
    const u8 *data;
    u32 tag;
    u8 encoding;
    u8 headerLength;
    u8 flags;
    u8 prefix;
    struct SslAsn1Element *parent;
    struct SslAsn1Element *next;
    struct SslAsn1Element *child;
} SslAsn1Element;

typedef struct SslAsn1Tree {
    u32 count;
    u32 capacity;
    SslAsn1Element *elements;
    u32 flags;
} SslAsn1Tree;

typedef struct SslAsn1Item {
    u32 itemType;
    u32 length;
    void *data;
    u32 reservedWord;
    u32 flags;
} SslAsn1Item;

typedef struct SslAsn1ItemList {
    u32 count;
    u32 capacity;
    SslAsn1Item *items;
    u32 flags;
} SslAsn1ItemList;

typedef struct SslAsn1FieldSpec {
    u32 expectedType;
    u32 targetType;
    const u8 *navScript;
    u32 allowedTagMask;
} SslAsn1FieldSpec;

/* BIGNUM & symmetric cipher contexts */
typedef struct SslBignum {
    u32 *words;
    int top;
    int maxWords;
    int neg;
    int flags;
} SslBignum;

typedef struct SslBnCtx {
    int tos;
    SslBignum pool[12];
    int extraWords[5];
    int flags;
} SslBnCtx;

typedef struct SslBnBlinding {
    int init;
    SslBignum *factorA;
    SslBignum *factorAi;
    SslBignum *modulus;
} SslBnBlinding;

typedef struct SslBnMontCtx {
    int ri;
    int useWord;
    int wordCount;
    SslBignum rrVal;
    SslBignum modVal;
    SslBignum invVal;
    u32 invLowWord;
    int flags;
} SslBnMontCtx;

typedef struct SslDesKeySchedule {
    u32 subkeys[32];
} SslDesKeySchedule;

typedef struct SslRc2Key {
    u16 data[64];
} SslRc2Key;

typedef struct SslRc4Key {
    u32 idxX;
    u32 idxY;
    u32 state[256];
} SslRc4Key;

typedef struct SslMd2Ctx {
    int num;
    u8 dataBuf[16];
    u32 checksum[16];
    u32 state[16];
} SslMd2Ctx;

/* Callback typedefs */
typedef int (*SslLockCb)(int mode, int type, const char *file, int line);
typedef int (*SslAddLockCb)(int *ptr, int amount, int type, const char *file, int line);
typedef u32 (*SslThreadIdCb)(void);
typedef SceSslTimeSpec *(*SslTimeCb)(SceSslTimeSpec *outTime);
typedef SceSslTimeSpec *(*SslTimeAddCb)(SceSslTimeSpec *outTime, const SceSslTimeSpec *inTime,
                                        int secDelta, int nsecDelta);
typedef int (*SslTimeFormatCb)(const SceSslTimeSpec *timeVal, int fmtType, char *outStr, int outLen);
typedef int (*SslTimeParseCb)(const u8 *inStr, u32 inLen, int fmtType, SceSslTimeSpec *outTime);
typedef int (*SslTimeCmpCb)(const SceSslTimeSpec *lhs, const SceSslTimeSpec *rhs);
typedef int (*SslQsortCmpFn)(const void *lhs, const void *rhs);
typedef void *(*SslMallocFn)(u32 size);
typedef void *(*SslReallocFn)(void *ptr, u32 size);
typedef void (*SslFreeFn)(void *ptr);
typedef void (*SslFreeCb)(void *ptr);
typedef u32 (*SslLhHashFn)(const void *value);
typedef int (*SslLhCmpFn)(const void *lhs, const void *rhs);
typedef void (*SslLhDoAllFn)(void *value);
typedef void (*SslLhDoAllArgFn)(void *value, void *userArg);
typedef u32 (*SslObjNameHashFn)(const char *name);
typedef int (*SslObjNameCmpFn)(const char *lhs, const char *rhs);
typedef void (*SslObjNameFreeFn)(const char *name, int type, const void *value);
typedef int (*SslMdInitFn)(void *mdCtx);
typedef int (*SslMdUpdateFn)(void *mdCtx, const void *data, u32 len);
typedef int (*SslMdFinalFn)(u8 *outDigest, void *mdCtx);
typedef int (*SslRsaBnCb)(void *arg, int *flagsPtr, int state);
typedef int (*SslBioCb)(SslBio *bio, int oper, const char *argp, int argi, int argl, int ret);
typedef int (*SslPemPasswordCb)(char *buf, int size, int rwflag);
typedef void *(*SslPemD2iFn)(void **outObj, const u8 **inPtr, int len);
typedef void *(*SslPemD2iPrivateKeyFn)(int keyType, void **outObj, const u8 **inPtr, int len);
typedef void (*SslInfoCb)(const Ssl *ssl, int type, int val);
typedef void (*SslAlertCb)(const Ssl *ssl, int type, int val, void *arg);
typedef void (*SslMsgCb)(const Ssl *ssl, int eventType, void *arg);
typedef int (*SslAppVerifyCb)(void *storeCtx, void *arg);
typedef int (*SslClientCertCb)(Ssl *ssl, void **outCert, SslEvpPkey **outPkey);
typedef int (*SslNewSessionCb)(Ssl *ssl, SslSession *session);
typedef void (*SslRemoveSessionCb)(SslCtx *ctx, SslSession *session);

/* Legacy ASN.1, buffer, stack, ex_data, and lhash structures */
struct SslAsn1String {
    int length;
    int type;
    u8 *data;
    int flags;
};

struct SslAsn1Object {
    const char *shortName;
    const char *longName;
    int nid;
    int length;
    const u8 *data;
    int flags;
};

struct SslAsn1Type {
    int type;
    union {
        void *ptr;
        int boolean;
        SslAsn1String *asn1String;
        SslAsn1Object *object;
    } value;
};

struct SslAsn1Ctx {
    const u8 *cur;
    int eos;
    int error;
    int inf;
    int tag;
    int xclass;
    int slen;
    const u8 *maxPtr;
    const u8 *prev;
    const u8 **pp;
    int line;
};

struct SslX509Algor {
    SslAsn1Object *algorithm;
    SslAsn1Type *parameter;
};

struct SslX509Sig {
    SslX509Algor *algor;
    SslAsn1String *digest;
};

struct SslBufMem {
    int length;
    char *data;
    int maxLen;
};

struct SslStack {
    int num;
    void **data;
    int sorted;
    int numAlloc;
    SslQsortCmpFn comp;
};

struct SslCryptoExData {
    SslStack *stack;
    int reservedWord;
};

struct SslCryptoExDataFuncs {
    int argLong;
    void *argPtr;
    int (*newFunc)(void *parent, void *ptr, SslCryptoExData *ad, int idx, int argl, void *argp);
    void (*freeFunc)(void *parent, void *ptr, SslCryptoExData *ad, int idx, int argl, void *argp);
    int (*dupFunc)(SslCryptoExData *to, SslCryptoExData *from, void *fromD, int idx, int argl, void *argp);
};

struct SslLhashNode {
    void *value;
    SslLhashNode *next;
    u32 hash;
};

struct SslLhash {
    SslLhashNode **buckets;
    SslLhCmpFn comp;
    SslLhHashFn hash;
    u32 numNodes;
    u32 numAllocNodes;
    u32 splitPos;
    u32 pmax;
    u32 upLoad;
    u32 downLoad;
    u32 numItems;
    u32 numExpands;
    u32 numExpandReallocs;
    u32 numContracts;
    u32 numContractReallocs;
    u32 numHashCalls;
    u32 numCompCalls;
    u32 numInsert;
    u32 numReplace;
    u32 numDelete;
    u32 numNoDelete;
    u32 numRetrieve;
    u32 numRetrieveMiss;
    u32 numHashComps;
    int error;
};

/* EVP structures */
struct SslEvpCipher {
    int nid;
    int blockSize;
    int keyLen;
    int ivLen;
    void (*init)(SslEvpCipherCtx *ctx, const u8 *key, const u8 *iv, int enc);
    void (*doCipher)(SslEvpCipherCtx *ctx, u8 *out, const u8 *in, u32 len);
    int (*cleanup)(SslEvpCipherCtx *ctx);
    int ctxSize;
    int (*setAsn1Params)(SslEvpCipherCtx *ctx, SslAsn1Type *type);
    int (*getAsn1Params)(SslEvpCipherCtx *ctx, SslAsn1Type *type);
};

struct SslEvpCipherCtx {
    const SslEvpCipher *cipher;
    int encrypt;
    int bufLen;
    u8 oiv[32];
    u8 iv[32];
    u8 buf[32];
    int num;
    void *appData;
    int cipherDataSize;
    void *cipherData;
    int keyScheduleFlags;
    int keyLen;
    int effectiveKeyBits;
};

struct SslEvpMd {
    int type;
    int pkeyType;
    int mdSize;
    SslMdInitFn init;
    SslMdUpdateFn update;
    SslMdFinalFn finalFn;
    int (*sign)(int type, const u8 *digest, u32 digestLen, u8 *sigOut, u32 *sigLen, void *rsa);
    int (*verify)(int type, const u8 *digest, u32 digestLen, const u8 *sigBuf, u32 sigLen, void *rsa);
    int requiredPkeyType[5];
    int blockSize;
    int ctxSize;
};

struct SslEvpMdCtx {
    const SslEvpMd *digest;
    u32 flags;
    int mdDataSize;
    void *mdData;
};

struct SslEvpPkey {
    int type;
    int saveType;
    int references;
    void *pkey;
    int saveParameters;
    SslStack *attributes;
};

struct SslEvpEncodeCtx {
    int num;
    int length;
    u8 encData[80];
    int lineNum;
    int expectNl;
};

/* RAND, RSA, BIO, X509TC, HMAC, ERR structures */
struct SslRandMethod {
    int (*seed)(const void *buf, int num);
    int (*bytes)(u8 *outBuf, int num);
    void (*cleanup)(void);
};

struct SslRsaMethod {
    const char *name;
    int (*rsaPubEnc)(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
    int (*rsaPubDec)(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
    int (*rsaPrivEnc)(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
    int (*rsaPrivDec)(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
    int (*rsaModExp)(SslBignum *out, const SslBignum *base, SslRsa *rsa);
    int (*bnModExp)(SslBignum *out, const SslBignum *base, const SslBignum *exp,
                    const SslBignum *mod, SslBnCtx *ctx, SslBnMontCtx *mont);
    int (*init)(SslRsa *rsa);
    int (*finish)(SslRsa *rsa);
    int flags;
    void *appData;
};

struct SslRsa {
    int padWord;
    int version;
    const SslRsaMethod *meth;
    SslBignum *modN;
    SslBignum *pubE;
    SslBignum *privD;
    SslBignum *primeP;
    SslBignum *primeQ;
    SslBignum *expDmp;
    SslBignum *expDmq;
    SslBignum *coeffIqmp;
    SslCryptoExData exData;
    int references;
    int flags;
    SslBnMontCtx *montModN;
    SslBnMontCtx *montPrimeP;
    SslBnMontCtx *montPrimeQ;
    void *bignumData;
    SslBnBlinding *blinding;
    int blindingMode;
    SslRsaBnCb bnCb;
    void *bnCbArg;
};

struct SslBioMethod {
    int type;
    const char *name;
    int (*bwrite)(SslBio *bio, const void *inBuf, int inLen);
    int (*bread)(SslBio *bio, void *outBuf, int outLen);
    int (*bputs)(SslBio *bio, const char *str);
    int (*bgets)(SslBio *bio, char *outBuf, int maxLen);
    int (*ctrl)(SslBio *bio, int cmd, int num, void *ptr);
    int (*create)(SslBio *bio);
    int (*destroy)(SslBio *bio);
};

struct SslBio {
    const SslBioMethod *method;
    SslBioCb callback;
    char *cbArg;
    int init;
    int shutdown;
    int flags;
    int retryReason;
    int num;
    void *ptr;
    SslBio *nextBio;
    SslBio *prevBio;
    int references;
    u32 numRead;
    u32 numWrite;
    SslCryptoExData exData;
};

struct SslX509TcName {
    SslAsn1Item rawItem;
    SslAsn1ItemList entries;
};

struct SslX509TcCert {
    SslAsn1Item rawCert;
    SslX509TcName issuer;
    SslX509TcName subject;
    SslAsn1Item serialNumber;
    SslAsn1Item notAfter;
    SslAsn1Item notBefore;
    SslEvpPkey *pubKey;
    int cachedSigType;
    int cachedVersion;
    int parsedFlag;
    int refCount;
};

struct SslHmacCtx {
    const SslEvpMd *md;
    SslEvpMdCtx *mdCtx;
    SslEvpMdCtx *innerCtx;
    SslEvpMdCtx *outerCtx;
    u32 keyBlockFlags;
    u32 keyLength;
    u8 key[64];
};

struct SslErrState {
    u32 pid;
    u32 errBuffer[16];
    char *errData[16];
    int errDataFlags[16];
    const char *errFile[16];
    int errLine[16];
    int top;
    int bottom;
};

/* SSLv3 / TLSv1 structures */
struct SslCipher {
    int valid;
    const char *name;
    u32 id;
    u32 algorithms;
    u32 algStrength;
    u32 strengthMask;
};

struct SslEncMethod {
    int (*enc)(Ssl *ssl, int sendFlag);
    int (*mac)(Ssl *ssl, u8 *outMd, int sendFlag);
    int (*setupKeyBlock)(Ssl *ssl);
    int (*generateMasterSecret)(Ssl *ssl, u8 *outMaster, const u8 *preMaster, int preMasterLen);
    int (*changeCipherState)(Ssl *ssl, int which);
    int (*finalFinishMac)(Ssl *ssl, SslEvpMdCtx *md5Ctx, SslEvpMdCtx *sha1Ctx,
                          const char *label, int labelLen, u8 *outBuf);
    int finishMacLength;
    int (*certVerifyMac)(Ssl *ssl, SslEvpMdCtx *mdCtx, u8 *outBuf);
    const char clientFinishedLabel[20];
    int clientFinishedLabelLen;
    const char serverFinishedLabel[20];
    int serverFinishedLabelLen;
    int (*alertValue)(int code);
};

struct SslMethod {
    int version;
    int (*sslNew)(Ssl *ssl);
    void (*sslClear)(Ssl *ssl);
    void (*sslFree)(Ssl *ssl);
    int (*sslAccept)(Ssl *ssl);
    int (*sslConnect)(Ssl *ssl);
    int (*sslRead)(Ssl *ssl, u8 *buf, int len);
    int (*sslPeek)(Ssl *ssl, u8 *buf, int len);
    int (*sslWrite)(Ssl *ssl, const u8 *buf, int len);
    int (*sslShutdown)(Ssl *ssl);
    int (*sslRenegotiate)(Ssl *ssl);
    int (*sslRenegotiateCheck)(Ssl *ssl);
    int (*sslCtrl)(Ssl *ssl, int cmd, int larg, void *parg);
    int (*sslCtxCtrl)(SslCtx *ctx, int cmd, int larg, void *parg);
    const SslCipher *(*getCipherByChar)(const u8 *wireBytes);
    int (*putCipherByChar)(const SslCipher *cipher, u8 *outBytes);
    int (*sslPending)(const Ssl *ssl);
    int (*numCiphers)(void);
    const SslCipher *(*getCipher)(int idx);
    const SslMethod *(*getSslMethod)(int version);
    int (*getTimeout)(void);
    const SslEncMethod *ssl3Enc;
};

struct SslCertPkey {
    void *certObj;
    SslEvpPkey *privateKey;
};

struct SslCert {
    int peerKeyType;
    SslCertPkey *activeKey;
    int valid;
    int mask;
    int exportMask;
    SslRsa *rsaTmp;
    SslRsa *rsaTmpExport;
    SslRsa *ephemeralRsa;
    void *rsaTmpCb;
    void *dhTmp;
    void *dhTmpCb;
    void *peerDhTmp;
    void *peerRsaKeyUnused;
    SslCertPkey pkeys[5];
    SslStack *certChain;
    int references;
};

struct SslSession {
    int sslVersion;
    u32 keyArgLength;
    u8 keyArg[8];
    int masterKeyLength;
    u8 masterKey[48];
    u32 sessionIdLength;
    u8 sessionId[32];
    u32 sidCtxLength;
    u8 sidCtx[32];
    int notResumable;
    SslCert *sessCert;
    void *peer;
    int references;
    int timeout;
    SceSslTimeSpec createdTime;
    const SslCipher *cipher;
    u32 cipherId;
    SslStack *ciphers;
    SslCryptoExData exData;
    SslSession *prev;
    SslSession *next;
};

struct Ssl3Record {
    int type;
    u32 length;
    u32 off;
    u8 *data;
    u8 *input;
    u8 *comp;
};

struct Ssl3State {
    u32 flags;
    int delayBufPopRet;
    u8 readSequence[8];
    u8 readMacSecret[20];
    u8 writeSequence[8];
    u8 writeMacSecret[20];
    u8 serverRandom[32];
    u8 clientRandom[32];
    int rbufAlignPad;
    int rbufReserved;
    int rbufLeft;
    int rbufOffset;
    u8 *rbufBuf;
    int rbufLen;
    int wbufAlignPad;
    int wbufReserved;
    int wbufLeft;
    int wbufOffset;
    u8 *wbufBuf;
    int wbufLen;
    Ssl3Record rrec;
    Ssl3Record wrec;
    int wnum;
    int wpendTot;
    int wpendType;
    int wpendRet;
    const u8 *wpendBuf;
    SslEvpMdCtx *finishDg1;
    SslEvpMdCtx *finishDg2;
    int changeCipherSpec;
    int warnAlert;
    int fatalAlert;
    int alertDispatch;
    u8 sendAlert[2];
    u8 alertPad[2];
    int renegotiate;
    int totalRenegotiations;
    int numRenegotiations;
    int inReadAppData;
    u8 tmpPeerFinishMd[36];
    int tmpFinishMdLen;
    u32 tmpMessageSize;
    int tmpMessageType;
    const SslCipher *tmpNewCipher;
    void *tmpDh;
    int tmpNextState;
    int tmpReuseMessage;
    int tmpCertReq;
    int tmpNumCertTypes;
    u8 tmpCertTypes[8];
    SslStack *tmpCaNames;
    int tmpUseRsaTmp;
    int tmpKeyBlockLen;
    u8 *tmpKeyBlock;
    const SslEvpCipher *tmpNewSymEnc;
    const SslEvpMd *tmpNewHash;
    void *tmpNewCompression;
};

struct SslCtxStats {
    int sessConnect;
    int sessConnectRenegotiate;
    int sessConnectGood;
    int sessAccept;
    int sessAcceptRenegotiate;
    int sessAcceptGood;
    int sessHit;
    int sessCbHit;
    int sessCacheFull;
    int sessMiss;
    int sessTimeout;
};

struct SslCtx {
    const SslMethod *method;
    u32 options;
    SslStack *cipherList;
    SslStack *cipherListById;
    void *certStore;
    SslLhash *sessions;
    int sessionCacheSize;
    SslSession *sessionCacheHead;
    SslSession *sessionCacheTail;
    int sessionCacheMode;
    int sessionTimeout;
    SslNewSessionCb newSessionCb;
    SslRemoveSessionCb removeSessionCb;
    void *getSessionCb;
    SslCtxStats stats;
    int references;
    SslInfoCb infoCallback;
    SslAlertCb alertCallback;
    void *alertCallbackArg;
    SslMsgCb msgCallback;
    void *msgCallbackArg;
    void *defaultVerifyCb;
    void *defaultVerifyArg;
    SslAppVerifyCb appVerifyCb;
    void *appVerifyArg;
    SslCert *cert;
    int mode;
    int defaultVerifyDepth;
    int verifyMode;
    void *verifyArg;
    u32 sidCtxLength;
    u8 sidCtx[32];
    void *defaultPasswdCb;
    SslClientCertCb clientCertCb;
    SslStack *clientCa;
    int quietShutdown;
    int shutdownMode;
    int closeNotifyLevel;
    SslCryptoExData exData;
    const SslEvpMd *md5Ssl2;
    const SslEvpMd *md5;
    const SslEvpMd *sha1;
    const SslEvpMd *dsaSha1;
    SslStack *extraCerts;
    u32 maxSendFragment;
    int readBufferSize;
    int rsaBlinding;
};

struct Ssl {
    int version;
    int type;
    const SslMethod *method;
    SslBio *rbio;
    SslBio *wbio;
    SslBio *bbio;
    int rwstate;
    int inHandshake;
    int (*handshakeFunc)(Ssl *ssl);
    int server;
    int newSession;
    int quietShutdown;
    int shutdownMode;
    int shutdown;
    int state;
    int rstatePrev;
    int rstate;
    SslBufMem *initBuf;
    int initNum;
    int initOff;
    u8 *packet;
    u32 packetLength;
    void *ssl2State;
    Ssl3State *ssl3State;
    int mode;
    int hit;
    SslStack *cipherList;
    SslStack *cipherListById;
    u32 readMacSize;
    void *readHash;
    SslEvpCipherCtx *encReadCtx;
    u32 writeMacSize;
    void *writeHash;
    SslEvpCipherCtx *encWriteCtx;
    SslFreeCb macFreeFunc;
    SslCert *cert;
    SslStack *extraCerts;
    u32 sidCtxLength;
    u8 sidCtx[32];
    SslSession *session;
    int verifyMode;
    int verifyDepth;
    void *verifyCallback;
    SslInfoCb infoCallback;
    SslAlertCb alertCallback;
    void *alertCallbackArg;
    SslMsgCb msgCallback;
    void *msgCallbackArg;
    int debug;
    int verifyFlagReserved;
    SslCtx *ctx;
    int unbuffered;
    int verifyResult;
    SslCryptoExData exData;
    SslStack *clientCa;
    int references;
    u32 options;
    int firstPacket;
    int clientVersion;
    u32 maxSendFragment;
    int readBufferSize;
    int rsaBlinding;
    SslEvpMdCtx *mdCtx;
};

/* Global time callback (defined in crypto/evp/evp.c) */
extern SslTimeCb g_sslTimeCallback;

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

extern int sceNetInetSend(int sockFd, const void *buf, u32 len, int flags);
extern int sceNetInetRecv(int sockFd, void *buf, u32 len, int flags);
extern int sceNetInetShutdown(int sockFd, int how);

/* sceNet_lib libc imports */
extern void *sceNet_lib_4753D878(void *dst, const void *src, int len);       /* memmove */
extern char *sceNet_lib_5705F6F9(char *dst, const char *src);                /* strcat */
extern char *sceNet_lib_80C9F02A(char *dst, const char *src);                /* strcpy */
extern int sceNet_lib_8687B5AB(char *buf, const char *fmt, void *ap);        /* vsprintf */
extern int sceNet_lib_94DCA9F0(const char *lhs, const char *rhs, int len);   /* strncmp */
extern int sceNet_lib_A0F16ABD(const char *lhs, const char *rhs);            /* strcmp */
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, int len);       /* strncpy */
extern int sceNet_lib_D8722983(const char *str);                             /* strlen */
extern int sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, int len);   /* memcmp */

/* sce_ssl.c platform wrappers (0x02881C..0x028BDF) */
void sslQuickSortRange(void *basePtr, u32 elemSize, SslQsortCmpFn cmpFn, int lowIdx, int highIdx);
void sslQsort(void *basePtr, u32 elemCount, u32 elemSize, SslQsortCmpFn cmpFn);
int sslSprintf(char *outBuf, const char *fmt, ...);
int sslMemcmp(const void *lhs, const void *rhs, int len);
void *sslMemcpy(void *dst, const void *src, u32 len);
void *sslMemset(void *dst, int byteVal, u32 len);
void *sslMemmove(void *dst, const void *src, int len);
int sslVsprintf(char *outBuf, const char *fmt, void *ap);
int sslStrlen(const char *str);
int sslStrcmp(const char *lhs, const char *rhs);
int sslStrncmp(const char *lhs, const char *rhs, int len);
char *sslStrncpy(char *dst, const char *src, int len);
char *sslStrcat(char *dst, const char *src);

/* crypto/sha2.c */
void sslSha224Transform(const u32 blockWords[16], u32 state[8]);
void sslSha256Transform(const u32 blockWords[16], u32 state[8]);
void sslSha384Transform(const u64 blockWords[16], u64 state[8]);
void sslSha512Transform(const u64 blockWords[16], u64 state[8]);
int sslSha224Init(SceSha256Context *ctx);
int sslSha224Update(SceSha256Context *ctx, const u8 *input, u32 len);
int sslSha224Final(SceSha256Context *ctx, u8 *digest);
int sslSha256Init(SceSha256Context *ctx);
int sslSha256Update(SceSha256Context *ctx, const u8 *input, u32 len);
int sslSha256Final(SceSha256Context *ctx, u8 *digest);
int sslSha384Init(SceSha512Context *ctx);
int sslSha384Update(SceSha512Context *ctx, const u8 *input, u32 len);
int sslSha384Final(SceSha512Context *ctx, u8 *digest);
int sslSha512Init(SceSha512Context *ctx);
int sslSha512Update(SceSha512Context *ctx, const u8 *input, u32 len);
int sslSha512Final(SceSha512Context *ctx, u8 *digest);

/* crypto/asn1/asn1_tree.c */
int sslAsn1NavigateElement(SslAsn1Element *outElem, const SslAsn1Element *rootElem, const u8 *navScript);
int sslAsn1InitTree(SslAsn1Tree *tree, SslAsn1Element *elements, u32 count, u32 capacity);
int sslAsn1AppendElement(SslAsn1Tree *tree, u32 *outIndex);
int sslAsn1ReserveTree(SslAsn1Tree *tree, u32 newCap);
int sslAsn1DestroyTree(SslAsn1Tree *tree);
int sslAsn1DecodeFlatList(SslAsn1Tree *tree, const u8 *derBuf, u32 derLen);
void sslAsn1ClearTreeStruct(SslAsn1Tree *tree);
int sceSsl_lib_C85F6FCE(SslAsn1Element *elem, const u8 *derBuf, u32 derLen);
int sslAsn1DecodeTree(SslAsn1Tree *tree, const u8 *derBuf, u32 derLen);
int sslAsn1ExtractFields(const SslAsn1Item *rootItem, SslAsn1Item **outItems,
                         const SslAsn1FieldSpec *specs, u32 specCount, u32 flags);
int sslAsn1DecodeItemList(const SslAsn1Item *srcItem, SslAsn1ItemList *outList, u32 flags);
void sslAsn1ClearItem(SslAsn1Item *item);
int sslAsn1SetItemData(SslAsn1Item *item, u32 itemType, const void *srcData, u32 dataLen, u32 flags);
int sslAsn1DecodeSingleItem(SslAsn1Item *outItem, u32 *consumedOut, u32 itemType,
                            const u8 *derBuf, u32 derLen, u32 flags);
int sslAsn1InitItemList(SslAsn1ItemList *list, SslAsn1Item *items, u32 count, u32 capacity);
int sslAsn1AppendItem(SslAsn1ItemList *list, u32 *outIndex);
int sslAsn1ReserveItemList(SslAsn1ItemList *list, u32 newCap);
void sslAsn1DestroyItemList(SslAsn1ItemList *list);
void sslAsn1FreeItem(SslAsn1Item *item);

/* crypto/bn/bn.c */
int sslBnAdd(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs);
int sslBnUadd(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs);
int sslBnUsub(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs);
int sslBnSub(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs);
u32 sslBnMulAddWords(u32 *dst, const u32 *src, int count, u32 multWord);
u32 sslBnMulWords(u32 *dst, const u32 *src, int count, u32 multWord);
u32 sslBnSqrWords(u32 *dst, const u32 *src, int count);
u32 sslBnDivWords(u32 hiWord, u32 loWord, u32 divisor);
u32 sslBnAddWords(u32 *dst, const u32 *lhs, const u32 *rhs, int count);
u32 sslBnSubWords(u32 *dst, const u32 *lhs, const u32 *rhs, int count);
SslBnBlinding *sslBnBlindingNew(const SslBignum *factorA, const SslBignum *factorAi, SslBignum *modulus);
void sslBnBlindingFree(SslBnBlinding *blinding, int freeSelf);
int sslBnBlindingUpdate(SslBnBlinding *blinding, SslBnCtx *ctx);
int sslBnBlindingConvert(SslBignum *value, SslBnBlinding *blinding, SslBnCtx *ctx);
int sslBnBlindingInvert(SslBignum *value, SslBnBlinding *blinding, SslBnCtx *ctx);
int sslBnBlindingCopy(SslBnBlinding *dst, const SslBnBlinding *src);
u32 sslBnMulComba8(u32 *dst, const u32 *lhs, const u32 *rhs);
void sslBnSqrComba8(u32 *dst, const u32 *src);
int sslBnDiv(SslBignum *quotient, SslBignum *remainder, const SslBignum *dividend,
             const SslBignum *divisor, SslBnCtx *ctx);
int sslBnMod(SslBignum *remainder, const SslBignum *dividend, const SslBignum *divisor, SslBnCtx *ctx);
SslBignum *sslBnModInverse(SslBignum *out, const SslBignum *value, const SslBignum *modulus, SslBnCtx *ctx);
int sslBnNumBitsWord(u32 word);
int sslBnNumBits(const SslBignum *bn);
void sslBnClearFree(SslBignum *bn);
void sslBnFree(SslBignum *bn);
void sslBnInit(SslBignum *bn);
SslBignum *sslBnNew(void);
SslBnCtx *sslBnCtxNew(void);
void sslBnCtxInit(SslBnCtx *ctx);
void sslBnCtxFree(SslBnCtx *ctx);
SslBignum *sslBnExpandWords(SslBignum *bn, int neededWords);
SslBignum *sslBnDup(const SslBignum *src);
SslBignum *sslBnCopy(SslBignum *dst, const SslBignum *src);
u32 sslBnGetWord(const SslBignum *bn);
int sslBnSetWord(SslBignum *bn, u32 word);
SslBignum *sslBnBinToBn(const u8 *srcBytes, int byteLen, SslBignum *outBn);
int sslBnBnToBin(const SslBignum *bn, u8 *outBytes);
int sslBnUcmp(const SslBignum *lhs, const SslBignum *rhs);
int sslBnCmp(const SslBignum *lhs, const SslBignum *rhs);
int sslBnSetBit(SslBignum *bn, int bitIndex);
int sslBnMaskBits(SslBignum *bn, int bitCount);
int sslBnLshiftOne(SslBignum *out, const SslBignum *src);
int sslBnLshift(SslBignum *out, const SslBignum *src, int shiftBits);
int sslBnEncodeWindow(u8 *outScript, const SslBignum *exponent, int winBits);
int sslBnModExpMont(SslBignum *out, const SslBignum *base, const SslBignum *exponent,
                    const SslBignum *modulus, SslBnCtx *ctx, SslBnMontCtx *montCtx);
int sslBnModMul(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs,
                const SslBignum *modulus, SslBnCtx *ctx);
u32 sslBnMontReduceWords(u32 *dst, u32 *accBuf, const u32 *modWords, int wordCount, u32 invWord);
SslBnMontCtx *sslBnMontCtxNew(void);
void sslBnMontCtxInit(SslBnMontCtx *mont);
void sslBnMontCtxFree(SslBnMontCtx *mont);
int sslBnMontCtxSet(SslBnMontCtx *mont, const SslBignum *modulus, SslBnCtx *ctx);
int sslBnMul(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs, SslBnCtx *ctx);
u32 sslBnMulNormal(u32 *dst, const u32 *lhs, int lhsLen, const u32 *rhs, int rhsLen);
int sslBnRand(SslBignum *out, int bits, int topMode, int bottomOdd);
int sslBnRshiftOne(SslBignum *out, const SslBignum *src);
int sslBnRshift(SslBignum *out, const SslBignum *src, int shiftBits);
int sslBnSqr(SslBignum *out, const SslBignum *src, SslBnCtx *ctx);
u32 sslBnSqrNormal(u32 *dst, const u32 *src, int wordCount, u32 *scratch);
u32 sslBnModWord(const SslBignum *bn, u32 word);
u32 sslBnDivWord(SslBignum *bn, u32 word);
int sslBnAddWord(SslBignum *bn, u32 word);
int sslBnSubWord(SslBignum *bn, u32 word);
int sslBnMulWord(SslBignum *bn, u32 word);

/* crypto/ciphers/ciphers.c */
void sslDesEncrypt(u32 blockWords[2], const SslDesKeySchedule *ks, int encFlag);
void sslDesEncryptCore(u32 blockWords[2], const SslDesKeySchedule *ks, int encFlag);
void sslDesEncryptTriple(u32 blockWords[2], const SslDesKeySchedule *ksOne,
                         const SslDesKeySchedule *ksTwo, const SslDesKeySchedule *ksThree);
void sslDesDecryptTriple(u32 blockWords[2], const SslDesKeySchedule *ksOne,
                         const SslDesKeySchedule *ksTwo, const SslDesKeySchedule *ksThree);
void sslDesNcbcEncrypt(const u8 *inBuf, u8 *outBuf, int byteLen,
                       const SslDesKeySchedule *ks, u8 ivBuf[8], int encFlag);
void sslDesEde3CbcEncrypt(const u8 *inBuf, u8 *outBuf, int byteLen,
                          const SslDesKeySchedule *ksOne, const SslDesKeySchedule *ksTwo,
                          const SslDesKeySchedule *ksThree, u8 ivBuf[8], int encFlag);
int sslDesCheckKeyParity(const u8 keyBytes[8]);
int sslDesIsWeakKey(const u8 keyBytes[8]);
int sslDesSetKey(const u8 keyBytes[8], SslDesKeySchedule *ks);
void sslRc2CbcEncrypt(const u8 *inBuf, u8 *outBuf, int byteLen,
                      const SslRc2Key *ks, u8 ivBuf[8], int encFlag);
void sslRc2Encrypt(u32 blockWords[2], const SslRc2Key *ks);
void sslRc2Decrypt(u32 blockWords[2], const SslRc2Key *ks);
void sslRc2SetKey(SslRc2Key *ks, int keyLen, const u8 *keyBytes, int effectiveBits);
void sslRc4Crypt(SslRc4Key *key, u32 byteLen, const u8 *inBuf, u8 *outBuf);
void sslRc4SetKey(SslRc4Key *key, int keyLen, const u8 *keyBytes);
void sslMd2Init(SslMd2Ctx *ctx);
void sslMd2Update(SslMd2Ctx *ctx, const u8 *inBuf, u32 byteLen);
void sslMd2Block(SslMd2Ctx *ctx, const u8 blockBytes[16]);
void sslMd2Final(u8 outDigest[16], SslMd2Ctx *ctx);

/* crypto/evp/evp.c */
int sslEvpMd5Final(u8 *outDigest, void *mdCtx);
int sslEvpSha1Final(u8 *outDigest, void *mdCtx);
int sslEvpSha224Final(u8 *outDigest, void *mdCtx);
int sslEvpSha256Final(u8 *outDigest, void *mdCtx);
int sslEvpSha384Final(u8 *outDigest, void *mdCtx);
int sslEvpSha512Final(u8 *outDigest, void *mdCtx);
int sslEvpDigestInit(SslEvpMdCtx *ctx, const SslEvpMd *digestType);
void sslEvpDigestUpdate(SslEvpMdCtx *ctx, const void *input, u32 inputLen);
void sslEvpDigestFinal(SslEvpMdCtx *ctx, u8 *outDigest, u32 *outSize);
const SslEvpCipher *sslEvpDesEde3Cbc(void);
const SslEvpCipher *sslEvpDesCbc(void);
const SslEvpCipher *sslEvpRc2Cbc(void);
const SslEvpCipher *sslEvpRc2_64Cbc(void);
const SslEvpCipher *sslEvpRc2_40Cbc(void);
int sslEvpRc2KeyBitsToMagic(const SslEvpCipher *cipher);
const SslEvpCipher *sslEvpRc2MagicToCipher(int magic);
const SslEvpCipher *sslEvpEncNull(void);
const SslEvpCipher *sslEvpRc4(void);
SslEvpEncodeCtx *sslEvpEncodeCtxNew(void);
void sslEvpEncodeCtxFree(SslEvpEncodeCtx *ctx);
void sslEvpDecodeInit(SslEvpEncodeCtx *ctx);
int sslEvpDecodeUpdate(SslEvpEncodeCtx *ctx, u8 *outBuf, int *outLen, const u8 *inBuf, int inLen);
int sslEvpDecodeBlock(u8 *outBuf, const u8 *inBuf, int inLen);
int sslEvpDecodeFinal(SslEvpEncodeCtx *ctx, u8 *outBuf, int *outLen);
const SslEvpMd *sslEvpMd2Rsa(void);
const SslEvpMd *sslEvpMd2(void);
const SslEvpMd *sslEvpMd5Rsa(void);
const SslEvpMd *sslEvpMd5(void);
const SslEvpMd *sslEvpSha1Rsa(void);
const SslEvpMd *sslEvpSha1(void);
const SslEvpMd *sslEvpSha224(void);
const SslEvpMd *sslEvpSha256(void);
const SslEvpMd *sslEvpSha384(void);
const SslEvpMd *sslEvpSha512(void);
int sslEvpAddCipher(const SslEvpCipher *cipher);
int sslEvpAddDigest(const SslEvpMd *digest);
const SslEvpCipher *sslEvpGetCipherByName(const char *name);
const SslEvpMd *sslEvpGetDigestByName(const char *name);
void sslEvpCleanup(void);
SslEvpPkey *sceSsl_lib_09C45275(void);
int sceSsl_lib_FAF333D1(SslEvpPkey *pkey);
int sslEvpPkeyType(int type);
void sceSsl_lib_7317C1BC(SslEvpPkey *pkey);
void sslEvpPkeyFreeInternal(SslEvpPkey *pkey);
void *sslEvpBsearch(const void *key, const void *basePtr, int elemCount, int elemSize, SslQsortCmpFn cmpFn);
void sslEvpCipher(SslEvpCipherCtx *ctx, u8 *out, const u8 *in, u32 len);
int sslEvpCipherCtxSetCipher(SslEvpCipherCtx *ctx, const SslEvpCipher *cipher);
int sslEvpCipherCtxInit(SslEvpCipherCtx *ctx);
SslEvpCipherCtx *sslEvpCipherCtxNew(void);
int sslEvpCipherInit(SslEvpCipherCtx *ctx, const SslEvpCipher *cipher, const u8 *key, const u8 *iv, int enc);
int sslEvpEncryptInit(SslEvpCipherCtx *ctx, const SslEvpCipher *cipher, const u8 *key, const u8 *iv);
int sslEvpDecryptInit(SslEvpCipherCtx *ctx, const SslEvpCipher *cipher, const u8 *key, const u8 *iv);
void sslEvpEncryptUpdate(SslEvpCipherCtx *ctx, u8 *out, int *outLen, const u8 *in, int inLen);
void sslEvpDecryptUpdate(SslEvpCipherCtx *ctx, u8 *out, int *outLen, const u8 *in, int inLen);
int sslEvpDecryptFinal(SslEvpCipherCtx *ctx, u8 *out, int *outLen);
void sslEvpCipherCtxFree(SslEvpCipherCtx *ctx);
void sslEvpCipherCtxCleanup(SslEvpCipherCtx *ctx);
u8 *sslEvpCipherCtxIv(SslEvpCipherCtx *ctx);
int sslEvpBytesToKey(const SslEvpCipher *cipher, const SslEvpMd *md, const u8 *salt,
                     const u8 *password, int passwordLen, u32 iterCount, u8 *outKey, u8 *outIv);
int sslEvpCipherGetAsn1Iv(SslEvpCipherCtx *ctx, SslAsn1Type *type);
int sslEvpCipherSetAsn1Iv(SslEvpCipherCtx *ctx, SslAsn1Type *type);
int sslEvpMdSize(const SslEvpMd *md);
int sslEvpMdBlockSize(const SslEvpMd *md);
int sslEvpMdCtxSize(const SslEvpMdCtx *ctx);
int sslEvpCipherNid(const SslEvpCipher *cipher);
int sslEvpCipherKeyLength(const SslEvpCipher *cipher);
int sslEvpCipherIvLength(const SslEvpCipher *cipher);
const SslEvpCipher *sslEvpCipherCtxCipher(const SslEvpCipherCtx *ctx);
int sslEvpCipherCtxBlockSize(const SslEvpCipherCtx *ctx);
int sslEvpCipherCtxKeyLength(const SslEvpCipherCtx *ctx);
int sslEvpCipherCtxEffectiveKeyBits(const SslEvpCipherCtx *ctx);
int sslEvpCipherCtxIvLength(const SslEvpCipherCtx *ctx);
int sslEvpMdCtxSetDigest(SslEvpMdCtx *ctx, const SslEvpMd *digest);
int sslEvpMdCtxInit(SslEvpMdCtx *ctx);
SslEvpMdCtx *sslEvpMdCtxNew(void);
void sslEvpMdCtxFree(SslEvpMdCtx *ctx);
int sslEvpMdCtxCopy(SslEvpMdCtx *dst, const SslEvpMdCtx *src);
SceSslTimeSpec *sslDefaultTimeCallback(SceSslTimeSpec *outTime);
SslTimeCb sslSetTimeCallback(SslTimeCb cb);

/* crypto/asn1/asn1_time_der.c */
SslTimeCb sceSsl_lib_2FE7BB42(void);
SceSslTimeSpec *sceSsl_lib_A7BED83B(SceSslTimeSpec *outTime);
SceSslTimeSpec *sslTimeAddDefault(SceSslTimeSpec *outTime, const SceSslTimeSpec *inTime,
                                  int secDelta, int nsecDelta);
SceSslTimeSpec *sceSsl_lib_665D02F6(SceSslTimeSpec *outTime, const SceSslTimeSpec *inTime,
                                    int secDelta, int nsecDelta);
int sslTimeFormatDefault(const SceSslTimeSpec *timeVal, int fmtType, char *outStr, int outLen);
int sceSsl_lib_9EF66756(const SceSslTimeSpec *timeVal, int fmtType, char *outStr, int outLen);
int sslTimeParseDefault(const u8 *inStr, u32 inLen, int fmtType, SceSslTimeSpec *outTime);
int sceSsl_lib_E7BFC0EF(const u8 *inStr, u32 inLen, int fmtType, SceSslTimeSpec *outTime);
int sslTimeCompareDefault(const SceSslTimeSpec *lhs, const SceSslTimeSpec *rhs);
int sceSsl_lib_047AB6BB(const SceSslTimeSpec *lhs, const SceSslTimeSpec *rhs);
int sslI2dAsn1BitString(const SslAsn1String *str, u8 **outPtr);
SslAsn1String *sslD2iAsn1BitString(SslAsn1String **outStr, const u8 **inPtr, int maxLen);
SslAsn1String *sslD2iAsn1TypeBytes(SslAsn1String **outStr, const u8 **inPtr, int maxLen, int typeMask);
int sslI2dAsn1Bytes(const SslAsn1String *str, u8 **outPtr, int tag, int xclass);
SslAsn1String *sslD2iAsn1Bytes(SslAsn1String **outStr, const u8 **inPtr, int maxLen, int tag, int xclass);
int sslAsn1CollectConstructedBytes(SslAsn1String *outStr, SslAsn1Ctx *ctx);
int sslI2dAsn1OctetString(const SslAsn1String *str, u8 **outPtr);
SslAsn1String *sslD2iAsn1OctetString(SslAsn1String **outStr, const u8 **inPtr, int maxLen);
int sslI2dAsn1Type(const SslAsn1Type *asnType, u8 **outPtr);
SslAsn1Type *sslD2iAsn1Type(SslAsn1Type **outType, const u8 **inPtr, int maxLen);
SslAsn1Type *sslAsn1TypeNew(void);
void sslAsn1TypeFree(SslAsn1Type *asnType);
void sslAsn1TypeSet(SslAsn1Type *asnType, int type, void *value);
void sslAsn1TypeFreeValue(SslAsn1Type *asnType);
SslEvpPkey *sceSsl_lib_A920B39E(int keyType, SslEvpPkey **outKey, const u8 **inPtr, int maxLen);
SslRsa *sslD2iRsaPrivateKey(SslRsa **outRsa, const u8 **inPtr, int maxLen);
int sslI2dX509Algor(const SslX509Algor *algor, u8 **outPtr);
SslX509Algor *sslD2iX509Algor(SslX509Algor **outAlgor, const u8 **inPtr, int maxLen);
SslX509Algor *sslX509AlgorNew(void);
void sslX509AlgorFree(SslX509Algor *algor);
int sslI2dX509Sig(const SslX509Sig *sig, u8 **outPtr);
SslX509Sig *sslD2iX509Sig(SslX509Sig **outSig, const u8 **inPtr, int maxLen);
SslX509Sig *sslX509SigNew(void);
void sslX509SigFree(SslX509Sig *sig);

/* crypto/e_malloc.c */
void sceSsl_lib_9A789D37(void *poolMem, u32 poolSize);
u32 sslPoolInit(void);
void *sslPoolAlloc(u32 reqSize);
void *sslPoolRealloc(void *ptr, u32 newSize);
void sslPoolFree(void *ptr);
void sslPoolErrorHook(void);
int sslPoolVerify(void);
u32 sceSsl_lib_9266C0D5(void);
u32 sslPoolGetUsedCurrent(void);
void sslPoolReset(void);
int sceSsl_lib_A2619268(SslMallocFn mallocFn, SslReallocFn reallocFn, SslFreeFn freeFn);
void sceSsl_lib_B0B90785(SslMallocFn *mallocOut, SslReallocFn *reallocOut, SslFreeFn *freeOut);
void sceSsl_lib_064F5DF3(SslMallocFn *mallocOut, SslFreeFn *freeOut);
void *sceSsl_lib_6053B79C(u32 size);
void sceSsl_lib_E5FC4992(void *ptr);
void *sceSsl_lib_0861D934(u32 size);
void *sslCryptoRealloc(void *ptr, u32 newSize);
void sceSsl_lib_5E5C873A(void *ptr);

/* crypto/lhash/lhash_obj_buf.c */
SslBufMem *sslBufMemNew(void);
void sslBufMemFree(SslBufMem *buf);
int sslBufMemGrow(SslBufMem *buf, int newLen);
void *sslCryptoGetExData(const SslCryptoExData *exData, int idx);
void sslCryptoFreeExData(SslStack *funcs, void *ownerObj, SslCryptoExData *exData);
void sslCryptoNewExData(SslStack *funcs, void *ownerObj, SslCryptoExData *exData);
SslLhash *sslLhNew(SslLhHashFn hashFn, SslLhCmpFn cmpFn);
void sslLhFree(SslLhash *lh);
void *sslLhInsert(SslLhash *lh, void *value);
void *sslLhDelete(SslLhash *lh, const void *value);
void *sslLhRetrieve(SslLhash *lh, const void *value);
void sslLhDoAll(SslLhash *lh, SslLhDoAllFn cb);
void sslLhDoAllArg(SslLhash *lh, SslLhDoAllArgFn cb, void *userArg);
void sslLhExpand(SslLhash *lh);
void sslLhContract(SslLhash *lh);
SslLhashNode **sslLhGetBucketSlot(SslLhash *lh, const void *value, u32 *outHash);
u32 sslLhStrHash(const char *str);
int sslObjNameInit(void);
int sslObjNameCmp(const void *lhsPtr, const void *rhsPtr);
u32 sslObjNameHash(const void *entryPtr);
const void *sslObjNameGet(const char *name, int type);
int sslObjNameAdd(const char *name, int type, const void *value);
int sslObjNameRemove(const char *name, int type);
void sslObjNameCleanupCallback(void *entryPtr);
void sslObjNameCleanup(int type);
void sslObjCleanup(void);
SslAsn1Object *sslObjNidToObj(u32 nid);
const char *sslObjNidToSn(u32 nid);
const char *sslObjNidToLn(u32 nid);
int sslObjObjToNid(const SslAsn1Object *obj);
int sslObjSnToNid(const char *shortName);
int sslObjOidCmp(const void *lhsPtr, const void *rhsPtr);
const void *sslObjBsearch(const void *key, const void *basePtr, int elemCount, int elemSize, SslQsortCmpFn cmpFn);
int sslObjSnCmp(const void *lhsPtr, const void *rhsPtr);

/* crypto/rsa/rand_rsa_stack.c */
const SslRandMethod *sceSsl_lib_A1C3AB31(void);
void sceSsl_lib_1C2728A5(const SslRandMethod *meth);
const SslRandMethod *sceSsl_lib_4AE72675(void);
void sceSsl_lib_56E19CEB(void);
void sceSsl_lib_DEA6EDB2(const void *buf, int num);
int sceSsl_lib_21F2A35C(u8 *outBuf, int num);
const SslRsaMethod *sslRsaPkcs1Ssleay(void);
SslRsa *sslRsaNew(void);
SslRsa *sslRsaNewMethod(const SslRsaMethod *meth);
int sslRsaSetBlindingMode(SslRsa *rsa, int mode);
void sslRsaFree(SslRsa *rsa);
int sslRsaSize(const SslRsa *rsa);
int sslRsaPublicEncrypt(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
int sslRsaPrivateEncrypt(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
int sslRsaPublicDecrypt(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
int sslRsaFlags(const SslRsa *rsa);
void sceSsl_lib_A87C65C7(SslRsa *rsa, SslRsaBnCb cb, void *cbArg);
int sceSsl_lib_72D77445(int mode);
int sslRsaBlindingOn(SslRsa *rsa, SslBnCtx *ctx);
int sslRsaPaddingAddNone(u8 *to, int toLen, const u8 *from, int fromLen);
int sslRsaPaddingCheckNone(u8 *to, int toLen, const u8 *from, int fromLen, int num);
int sslRsaPaddingAddPkcs1TypeOne(u8 *to, int toLen, const u8 *from, int fromLen);
int sslRsaPaddingCheckPkcs1TypeOne(u8 *to, int toLen, const u8 *from, int fromLen, int num);
int sslRsaPaddingAddPkcs1TypeTwo(u8 *to, int toLen, const u8 *from, int fromLen);
int sslRsaPaddingCheckPkcs1TypeTwo(u8 *to, int toLen, const u8 *from, int fromLen, int num);
int sslRsaSign(int mdType, const u8 *digest, u32 digestLen, u8 *sigRet, u32 *sigLen, SslRsa *rsa);
int sslRsaVerify(int mdType, const u8 *digest, u32 digestLen, const u8 *sigBuf, u32 sigLen, SslRsa *rsa);
int sslRsaPaddingAddSslv23(u8 *to, int toLen, const u8 *from, int fromLen);
int sslRsaPaddingCheckSslv23(u8 *to, int toLen, const u8 *from, int fromLen, int num);
SslQsortCmpFn sslSkSetCmpFunc(SslStack *sk, SslQsortCmpFn cmpFn);
SslStack *sslSkDup(const SslStack *sk);
SslStack *sslSkNew(SslQsortCmpFn cmpFn);
int sslSkInsert(SslStack *sk, void *item, int pos);
int sslSkFind(SslStack *sk, void *item);
int sslSkPush(SslStack *sk, void *item);
void sslSkPopFree(SslStack *sk, SslFreeFn freeFn);
void sslSkFree(SslStack *sk);
int sslRsaDecodePubKeyItem(SslEvpPkey *pkey, const SslAsn1Item *spkiItem);
SslEvpPkey *sslD2iPublicKey(int keyType, SslEvpPkey **outKey, const u8 **inPtr, int maxLen);

/* crypto/asn1/asn1_der_core.c */
int sslI2dAsn1Integer(const SslAsn1String *asnInt, u8 **outPtr);
SslAsn1String *sslD2iAsn1Integer(SslAsn1String **outInt, const u8 **inPtr, int maxLen);
int sslAsn1IntegerSet(SslAsn1String *asnInt, int value);
int sslAsn1IntegerGet(const SslAsn1String *asnInt);
int sslI2dAsn1Object(const SslAsn1Object *obj, u8 **outPtr);
SslAsn1Object *sslD2iAsn1Object(SslAsn1Object **outObj, const u8 **inPtr, int maxLen);
SslAsn1Object *sslAsn1ObjectNew(void);
void sslAsn1ObjectFree(SslAsn1Object *obj);
SslAsn1String *sslD2iAsn1PrintableString(SslAsn1String **outStr, const u8 **inPtr, int maxLen);
int sslI2dAsn1Utctime(const SslAsn1String *str, u8 **outPtr);
SslAsn1String *sslD2iAsn1Utctime(SslAsn1String **outStr, const u8 **inPtr, int maxLen);
int sslAsn1UtctimeCheck(const SslAsn1String *str);
int sslAsn1CheckEoc(const u8 **inPtr, int maxLen);
int sslAsn1GetObject(const u8 **inPtr, int *outLen, int *outTag, int *outClass, int maxLen);
int sslAsn1GetLength(const u8 **inPtr, int *outInf, int *outLen, int maxLen);
void sslAsn1PutObject(u8 **outPtr, int constructed, int bodyLen, int tag, int xclass);
void sslAsn1PutLength(u8 **outPtr, int bodyLen);
int sslAsn1ObjectSize(int constructed, int bodyLen, int tag);
int sslAsn1Finish(SslAsn1Ctx *ctx);
int sslAsn1SeqStart(SslAsn1Ctx *ctx, int *remainLen);
int sslAsn1StringSet(SslAsn1String *str, const void *srcData, int dataLen);
SslAsn1String *sslAsn1StringNew(void);
SslAsn1String *sslAsn1StringTypeNew(int type);
void sslAsn1StringFree(SslAsn1String *str);
void sslAsn1AddError(const u8 *addr, int offset);
int sslAsn1TypeSetOctetstring(SslAsn1Type *asnType, const u8 *inData, int dataLen);
int sslAsn1TypeGetOctetstring(const SslAsn1Type *asnType, u8 *outData, int maxLen);
int sslAsn1TypeSetIntOctetstring(SslAsn1Type *asnType, int intVal, const u8 *inData, int dataLen);
int sslAsn1TypeGetIntOctetstring(const SslAsn1Type *asnType, int *outInt, u8 *outData, int maxLen);

/* crypto/bio/bio_lib.c */
int sceSsl_lib_3454E622(SslBio *bio, const char *fmt, ...);
const SslBioMethod *sslBioFBuffer(void);
int sslBioSetFlags(SslBio *bio, int flags);
int sceSsl_lib_EE624680(SslBio *bio);
int sceSsl_lib_8A2F6963(SslBio *bio);
int sceSsl_lib_067173A8(SslBio *bio);
int sceSsl_lib_88A96ED0(SslBio *bio);
int sceSsl_lib_CA6A3C74(const SslBio *bio);
int sceSsl_lib_E3C30923(const SslBio *bio);
int sceSsl_lib_089FF1F1(const SslBio *bio);
int sceSsl_lib_ABCD28B8(const SslBio *bio);
int sceSsl_lib_1BCA32E3(const SslBio *bio);
int sceSsl_lib_9052D21A(const SslBio *bio);
void sceSsl_lib_26A7CF72(SslBio *bio, SslBioCb cb);
void sceSsl_lib_ECE07B61(SslBio *bio, char *cbArg);
char *sceSsl_lib_EC4A18D7(const SslBio *bio);
SslBioCb sceSsl_lib_558CAA41(const SslBio *bio);
const char *sceSsl_lib_0FD2F220(const SslBio *bio);
int sceSsl_lib_19A0DB42(const SslBio *bio);
const SslBioMethod *sslBioSMem(void);
SslBio *sceSsl_lib_58352068(void);
const SslBioMethod *sslBioSSocket(void);
SslBio *sceSsl_lib_28B4DE33(int sockFd, int closeFlag);
SslBio *sceSsl_lib_5DED429A(const SslBioMethod *method);
int sslBioSet(SslBio *bio, const SslBioMethod *method);
int sceSsl_lib_3E040FD4(SslBio *bio);
int sceSsl_lib_675AC5AA(SslBio *bio, void *outBuf, int outLen);
int sceSsl_lib_CED07EAE(SslBio *bio, const void *inBuf, int inLen);
int sceSsl_lib_AFD1D124(SslBio *bio, char *outBuf, int maxLen);
int sslBioIntCtrl(SslBio *bio, int cmd, int num, int intVal);
int sceSsl_lib_9566709F(SslBio *bio, int cmd, int num, void *ptr);
SslBio *sceSsl_lib_1EC64594(SslBio *bio, SslBio *appendBio);
SslBio *sceSsl_lib_732F4E44(SslBio *bio);
int sceSsl_lib_4A6078CF(const SslBio *bio);
void sceSsl_lib_959557F5(SslBio *bio);
void sceSsl_lib_624CB314(SslBio *bio);
int sslBioInvokeCertPrinter(SslBio *bio, const SslAsn1Item *certItem,
                            int (*printFn)(SslBio *, const void *, u32));

/* crypto/x509tc/x509tc.c */
int sslX509TcInit(void);
int sceSsl_lib_1E257243(int optId, int value);
int sceSsl_lib_B1E75EA9(int optId);
void *sceSsl_lib_0A241EDE(void);
void sceSsl_lib_C1A2E14B(void *cert);
int sceSsl_lib_0FF7BA68(void *cert, SslEvpPkey *pubKey);
int sslX509Digest(void *cert, const SslEvpMd *md, u8 *outBuf, u32 *outLen);
int sceSsl_lib_A739BFD9(const void *cert, u8 **outPtr);
void *sceSsl_lib_6038DEDE(void **outCert, const u8 **inPtr, u32 len);
void sceSsl_lib_999D7614(void *cert);
int sslX509GetSignatureType(void *cert);
int sceSsl_lib_1B93AD27(const void *lhs, const void *rhs);
void *sceSsl_lib_4BE9030A(void *cert);
void *sceSsl_lib_12C7718F(void *cert);
SslEvpPkey *sceSsl_lib_B87FFE37(void *cert);
int sceSsl_lib_9299C6CE(void *cert, int *outTag, const u8 **outData, u32 *outLen);
int sceSsl_lib_05E019A4(void *cert, int *outTag, const u8 **outData, u32 *outLen);
int sceSsl_lib_341843AB(void *cert, int *outTag, const u8 **outData, u32 *outLen);
void *sceSsl_lib_45514AE3(void);
void sceSsl_lib_ED95A457(void *name);
void *sceSsl_lib_338C8FDD(void *name);
u32 sceSsl_lib_BE4A711C(const void *name);
int sceSsl_lib_7D0E41F4(const void *lhs, const void *rhs);
void *sceSsl_lib_D579CE04(void **outName, const u8 **inPtr, int len);
int sceSsl_lib_90BCFCE2(const void *name, u8 **outPtr);
int sceSsl_lib_4C75B8E9(void *name);
void *sceSsl_lib_5040E836(void *name, int entryIndex);
int sceSsl_lib_8D484361(void *entry, int *outOidTag, const u8 **outOid,
                        u32 *outOidLen, int *outValTag, const u8 **outVal, u32 *outValLen);
int sceSsl_lib_6443D0EE(void *entry, int *outOidTag, const u8 **outOid, u32 *outOidLen);
int sceSsl_lib_6E1E9E0D(void *entry, int *outValTag, const u8 **outVal, u32 *outValLen);
void *sslX509StoreNew(void);
void sslX509StoreFree(void *store);
int sslX509StoreSetVerifyArg(void *store, void *verifyArg);
int sslX509StoreGetIssuer(void *storeCtx, void **outCert, void *issuerName);
void *sslX509StoreCtxNew(void);
void sslX509StoreCtxInit(void *storeCtx, void *store, void *cert, void *chain);
void sslX509StoreCtxFree(void *storeCtx);
void sslX509StoreCtxCleanup(void *storeCtx);
int sslX509VerifyCert(void *storeCtx);
int sslX509StoreCtxGetError(void *storeCtx);
int sslX509AppVerifyCertChain(Ssl *ssl, SslStack *chain, SslAppVerifyCb cb,
                              void *arg, int *verifyResult);
int sslX509DefaultVerifyCertChain(Ssl *ssl, SslStack *chain, void *store,
                                  void *cb, void *arg, int *verifyResult);
int sceSsl_lib_C6ED5C87(void *cert, SslEvpPkey *privKey);
void *sceSsl_lib_8B78A6C0(SslBio *bio, void **outCert, void *passCb);
SslEvpPkey *sceSsl_lib_737BED1C(void);
void sceSsl_lib_9307DE23(SslEvpPkey *pkey);
SslEvpPkey *sceSsl_lib_EDBE00D6(int keyType, SslEvpPkey **outKey, const u8 **inPtr, int len);
void sceSsl_lib_E11361C2(SslEvpPkey *pkey);
int sceSsl_lib_76A46EDA(void *cert, const u8 *oid, int *outCrit, const u8 **outData, u32 *outLen);
int sceSsl_lib_7781A781(void *cert, int *outCrit, u32 *outUsage);
int sceSsl_lib_CFA2F8EB(void *cert, int *outCrit, int *outPathLen, int *outIsCa);
const char *sceSsl_lib_2B7BD166(const u8 *oidData, u32 oidLen, int shortName);
int sceSsl_lib_37C7B76C(void *cert, u8 *outBuf, u32 *outLen);
void sslX509DisableMontCache(void);
int sslMontCacheEnabled(void);
void sslMontCacheDisable(void);

/* crypto/err/err.c */
SslLockCb sceSsl_lib_6BA650E4(void);
SslAddLockCb sceSsl_lib_69105335(void);
void sceSsl_lib_A9191C17(SslLockCb cb);
void sceSsl_lib_64AA4ED6(SslAddLockCb cb);
SslThreadIdCb sceSsl_lib_6CDEB823(void);
void sceSsl_lib_5AD14F0B(SslThreadIdCb cb);
u32 sceSsl_lib_9D8A385E(void);
void sceSsl_lib_D84E2411(int mode, int type, const char *file, int line);
int sslCryptoAddLock(int *counterPtr, int amount, int type, const char *file, int line);
void *sceSsl_lib_0217E73C(SslPemD2iFn d2iFn, const char *name, SslBio *bio, void **outObj, void *passCb);
int sceSsl_lib_BE5FF9F1(SslEvpCipherCtx *cipherCtx, u8 *dataBuf, int *lenPtr, SslPemPasswordCb passCb);
SslHmacCtx *sslHmacCtxNew(void);
void sslHmacCtxFree(SslHmacCtx *ctx);
const SslEvpMd *sslHmacCtxMd(const SslHmacCtx *ctx);
int sslHmacCtxCopy(SslHmacCtx *dst, const SslHmacCtx *src);
int sslHmacInit(SslHmacCtx *ctx, const void *key, int keyLen, const SslEvpMd *md);
void sslHmacUpdate(SslHmacCtx *ctx, const void *data, u32 len);
int sslHmacFinal(SslHmacCtx *ctx, u8 *outMd, u32 *outLen);
void sceSsl_lib_101D418A(void);
void sceSsl_lib_9CDCA0D7(int lib, int func, int reason, const char *file, int line);
void sceSsl_lib_04E677AF(void);
u32 sceSsl_lib_E78AD94D(void);
u32 sceSsl_lib_47F7B97F(void);
void sceSsl_lib_D8C05497(u32 pid);
SslErrState *sslErrGetState(void);
void sslErrSetErrorData(char *dataStr, int dataFlags);
void sslErrAddErrorData(int numStrings, ...);

/* ssl/s3_core.c */
const SslMethod *sceSsl_lib_B40D11EA(void);
int ssl3Connect(Ssl *ssl);
int ssl3WriteBytes(Ssl *ssl, int recType, const u8 *buf, int len);
int ssl3ReadBytes(Ssl *ssl, int recType, u8 *buf, int len);
int ssl3SendAlert(Ssl *ssl, int level, int desc);
int ssl3DispatchAlert(Ssl *ssl);
int ssl3DoWrite(Ssl *ssl, int recType);

/* ssl/t1_ciph_cert.c */
const SslMethod *sceSsl_lib_42FFA860(void);
const SslMethod *tls1BaseMethod(void);
int sslCipherGetEvp(const SslCipher *cipher, const SslEvpCipher **outCipher, const SslEvpMd **outMd);
SslStack *sslCreateCipherList(const SslMethod *method, SslStack **outCipherList,
                              SslStack **outCipherListById, const char *ruleStr);
const char *sceSsl_lib_A2CE8DCD(const SslCipher *cipher);
SslSession *sceSsl_lib_EF5A4BD4(const Ssl *ssl);
int sslGetNewSession(Ssl *ssl, int generateId);
int sslCtxAddSession(SslCtx *ctx, SslSession *session);
int sceSsl_lib_B81AD643(SslCtx *ctx, SslSession *session);
void sceSsl_lib_BDAD0841(SslSession *session);
int sceSsl_lib_86D0771E(SslSession *session);
int sceSsl_lib_69F1B678(Ssl *ssl, SslSession *session);
int sceSsl_lib_4214B7AF(SslCtx *ctx, int timeout);
int sceSsl_lib_795B9EDF(const SslCtx *ctx);
void sceSsl_lib_4E0F2CCC(SslCtx *ctx, int useCurrentTime);
int sslBadSessionCheck(Ssl *ssl);
void sslSessionListRemove(SslCtx *ctx, SslSession *session);
void sslSessionListAddHead(SslCtx *ctx, SslSession *session);

/* ssl/ssl_lib.c */
int sslEvpPkeySize(const SslEvpPkey *pkey);
int sslEvpPkeyCopyParameters(SslEvpPkey *dst, const SslEvpPkey *src);
int sslEvpPkeyMissingParams(const SslEvpPkey *pkey);
SslEvpPkey *sslEvpPkeyNew(void);
int sslEvpPkeyUpRef(SslEvpPkey *pkey);
int sslEvpPkeyAssign(SslEvpPkey *pkey, int keyType, void *keyPtr);
int sslEvpPkeyGetType(const SslEvpPkey *pkey);
int sslEvpPkeyNormalizeType(int keyType);
void sslEvpPkeyFree(SslEvpPkey *pkey);
int sslCertGetMask(int sigType, const SslEvpPkey *pubKey);
int ssl3SendFinished(Ssl *ssl, int stateA, int stateB, const char *label, int labelLen);
int ssl3GetFinished(Ssl *ssl, int stateA, int stateB);
int ssl3SendChangeCipherSpec(Ssl *ssl, int stateA, int stateB);
int ssl3OutputCertChain(Ssl *ssl, void *leafCert);
int ssl3GetMessage(Ssl *ssl, int stateA, int stateB, int expectedType, u32 maxLen, int *okOut);
int sslCertType(void *cert, SslEvpPkey *pubKey);
int sslVerifyAlarmType(int verifyErr);
int ssl3SetupBuffers(Ssl *ssl);
int ssl3ResizeReadBuffer(Ssl *ssl, int targetSize);
void ssl3CleanupKeyBlock(Ssl *ssl);
int ssl3InitFinishedMac(Ssl *ssl);
void ssl3FinishMac(Ssl *ssl, const u8 *buf, int len);
const SslMethod *ssl3BaseMethod(void);
int ssl3NumCiphers(void);
const SslCipher *ssl3GetCipher(int idx);
int ssl3Pending(const Ssl *ssl);
int ssl3New(Ssl *ssl);
void ssl3Free(Ssl *ssl);
void ssl3Clear(Ssl *ssl);
int ssl3Ctrl(Ssl *ssl, int cmd, int larg, void *parg);
int ssl3CtxCtrl(SslCtx *ctx, int cmd, int larg, void *parg);
const SslCipher *ssl3GetCipherByChar(const u8 *wireBytes);
int ssl3PutCipherByChar(const SslCipher *cipher, u8 *outBytes);
int ssl3Shutdown(Ssl *ssl);
int ssl3Write(Ssl *ssl, const u8 *buf, int len);
int ssl3Read(Ssl *ssl, u8 *buf, int len);
int ssl3Peek(Ssl *ssl, u8 *buf, int len);
int ssl3Renegotiate(Ssl *ssl);
int ssl3RenegotiateCheck(Ssl *ssl);
int sceSsl_lib_9274BFE3(void);
int sceSsl_lib_CF78BEFB(void);
int sceSsl_lib_BB1564A3(void);
SslCert *sslCertNew(void);
void sslCertFree(SslCert *cert);
int sslVerifyCertChain(Ssl *ssl, SslStack *certChain);
int sceSsl_lib_92FA8B34(const Ssl *ssl);
void *sceSsl_lib_7BDAF864(const Ssl *ssl, int idx);
void sceSsl_lib_BF55C31C(SslCtx *ctx, SslClientCertCb cb);
int sceSsl_lib_54A7D8F3(Ssl *ssl);
Ssl *sceSsl_lib_EBFB0E3C(SslCtx *ctx);
void sceSsl_lib_84833472(Ssl *ssl);
void sceSsl_lib_B9C8CCE6(Ssl *ssl, SslBio *rbio, SslBio *wbio);
SslBio *sceSsl_lib_2E2E2B09(const Ssl *ssl);
SslBio *sceSsl_lib_868C15DF(const Ssl *ssl);
int sceSsl_lib_D49A570C(const Ssl *ssl);
int sceSsl_lib_E7C29542(Ssl *ssl, void *buf, int num);
int sceSsl_lib_B7CA8717(Ssl *ssl, const void *buf, int num);
int sceSsl_lib_3E3133D6(Ssl *ssl);
int sceSsl_lib_B4D78E98(SslCtx *ctx, int cmd, int larg, void *parg);
int sslCipherIdCmp(const void *lhs, const void *rhs);
SslStack *sslGetCiphers(const Ssl *ssl);
SslStack *sslGetCiphersById(const Ssl *ssl);
int sslCipherListToBytes(const Ssl *ssl, const SslStack *ciphers, u8 *outBytes);
SslCtx *sceSsl_lib_FB8273FE(const SslMethod *method);
void sceSsl_lib_588F2FE8(SslCtx *ctx);
void sceSsl_lib_529A9477(SslCtx *ctx, SslAppVerifyCb cb, void *arg);
void sceSsl_lib_AEBF278B(SslCtx *ctx, int mode, void *verifyArg);
void sslUpdateCache(Ssl *ssl, int modeFlag);
int sslSetSslMethod(Ssl *ssl, const SslMethod *method);
int sceSsl_lib_B3B04C58(const Ssl *ssl, int retVal);
int sceSsl_lib_D53ACF30(Ssl *ssl);
void sceSsl_lib_80608663(Ssl *ssl);
int sslUndefinedFunction(Ssl *ssl);
const SslMethod *sslUndefinedConstFunction(int version);
const SslCipher *sceSsl_lib_88897D26(const Ssl *ssl);
int sslInitWbioBuffer(Ssl *ssl, int pushNow);
void sslFreeWbioBuffer(Ssl *ssl);
void sceSsl_lib_42DF4592(SslCtx *ctx, int quiet);
int sceSsl_lib_E14101E4(const SslCtx *ctx);
void sceSsl_lib_4AF959E3(Ssl *ssl, int quiet);
int sceSsl_lib_6A19E0AA(const Ssl *ssl);
void sceSsl_lib_D5ABC603(SslCtx *ctx, int mode);
int sceSsl_lib_909E9D31(const SslCtx *ctx);
void sceSsl_lib_AE3986D3(Ssl *ssl, int mode);
int sceSsl_lib_340F8829(const Ssl *ssl);
void sceSsl_lib_86D4034D(Ssl *ssl, int mode);
int sceSsl_lib_D1205C58(const Ssl *ssl);
int sceSsl_lib_57F2E960(const Ssl *ssl);
SslCtx *sceSsl_lib_E92302C5(const Ssl *ssl);
void sceSsl_lib_C270B3A8(Ssl *ssl, SslInfoCb cb);
void sceSsl_lib_BE423CD1(Ssl *ssl, SslAlertCb cb, void *arg);
SslInfoCb sceSsl_lib_95E346AB(const Ssl *ssl);
void sceSsl_lib_9CB1A62F(Ssl *ssl, SslMsgCb cb, void *arg);
int sceSsl_lib_104F749D(const Ssl *ssl);
void sceSsl_lib_5605C9FE(Ssl *ssl, int verifyResult);
int sceSsl_lib_AA3B27CF(const Ssl *ssl);
int sceSsl_lib_C97D0510(const Ssl *ssl);
int sceSsl_lib_A8F0AD39(const Ssl *ssl);
int sslResetSupportedAlgorithms(void);
u32 sslAlgorithmSupported(u32 algFlag);
int sceSsl_lib_C0ABBB57(Ssl *ssl, void *certObj);
int sceSsl_lib_8390B321(Ssl *ssl, SslEvpPkey *pkey);

#endif /* LIBSSL_INTERNAL_H */
