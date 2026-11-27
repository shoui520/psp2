/*
 * RSA BSAFE SSL-C TLSv1 encryption/PRF, cipher suite registry, and session
 * cache management (t1_clnt.c, t1_enc.c, t1_lib.c, ssl_ciph.c, ssl_sess.c —
 * 0x020D2C..0x0238A7).
 */
#include "libssl_internal.h"

typedef struct SslCipherOrder {
    const SslCipher *cipher;
    int active;
    int dead;
    struct SslCipherOrder *next;
    struct SslCipherOrder *prev;
} SslCipherOrder;

typedef struct SslCipherRule {
    const char *name;
    int ruleOp;
    u32 algBits;
    u32 maskBits;
    int exactName;
    u32 exactId;
} SslCipherRule;

typedef struct SslSessionTimeoutArg {
    SslCtx *ctx;
    SceSslTimeSpec nowTime;
    SslLhash *cache;
} SslSessionTimeoutArg;

static int g_tls1ClientMethodInit = 1;
static SslMethod g_tls1ClientMethod;
static int g_sslCipherTablesNeedInit = 1;
static const SslEvpCipher *g_sslCipherTable[6];
static const SslEvpMd *g_sslDigestTable[2];
static SslStack *g_sslSessionExData;

static const u8 g_tls1ZeroSeq[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const char g_tls1EmptySecret[] = "";
static const char g_sessFilePath[] = "ssl/ssl_sess.c";

/* Forward declarations for TLSv1 and cipher/session helpers */
static const SslMethod *LIBSSL_NOINLINE tls1GetClientMethod(int version);
static int LIBSSL_NOINLINE tls1PHash(const SslEvpMd *md, const u8 *sec, int secLen,
                                     const u8 *seed, int seedLen, u8 *outBuf, int outLen);
static int LIBSSL_NOINLINE tls1Prf(const SslEvpMd *md5, const SslEvpMd *sha1,
                                   const u8 *seed, int seedLen, const u8 *sec, int secLen,
                                   u8 *outFirst, u8 *outScratch, int outLen);
static int LIBSSL_NOINLINE tls1GenerateKeyBlock(Ssl *ssl, u8 *keyBlock, u8 *scratch, int numBytes);
static int LIBSSL_NOINLINE tls1ChangeCipherState(Ssl *ssl, int which);
static int LIBSSL_NOINLINE tls1SetupKeyBlock(Ssl *ssl);
static int LIBSSL_NOINLINE tls1Enc(Ssl *ssl, int sendFlag);
static int LIBSSL_NOINLINE tls1CertVerifyMac(Ssl *ssl, SslEvpMdCtx *mdCtx, u8 *outBuf);
static int LIBSSL_NOINLINE tls1FinalFinishMac(Ssl *ssl, SslEvpMdCtx *md5Ctx, SslEvpMdCtx *sha1Ctx,
                                              const char *label, int labelLen, u8 *outBuf);
static int LIBSSL_NOINLINE tls1Mac(Ssl *ssl, u8 *outMd, int sendFlag);
static int LIBSSL_NOINLINE tls1GenerateMasterSecret(Ssl *ssl, u8 *outMaster,
                                                    const u8 *preMaster, int preMasterLen);
static int LIBSSL_NOINLINE tls1AlertCode(int code);
static int LIBSSL_NOINLINE tls1DefaultTimeout(void);
static int LIBSSL_NOINLINE tls1New(Ssl *ssl);
static void LIBSSL_NOINLINE tls1Free(Ssl *ssl);
static void LIBSSL_NOINLINE tls1Clear(Ssl *ssl);
static int LIBSSL_NOINLINE sslCipherNameCmp(const void *lhs, const void *rhs);
static int LIBSSL_NOINLINE sslCipherIdMaskCmp(const void *lhs, const void *rhs);
static void LIBSSL_NOINLINE sslLoadCiphers(void);
static void LIBSSL_NOINLINE sslCipherOrderMoveTail(SslCipherOrder **headPtr, SslCipherOrder *node,
                                                   SslCipherOrder **tailPtr);
static SslSession *LIBSSL_NOINLINE sslSessionNew(void);
static void LIBSSL_NOINLINE sslSessionTimeoutCb(SslSession *session, SslSessionTimeoutArg *arg);

/* TLSv1 SSL_ENC_METHOD (0x0305F0) */
static const SslEncMethod g_tls1EncData = {
    tls1Enc,
    tls1Mac,
    tls1SetupKeyBlock,
    tls1GenerateMasterSecret,
    tls1ChangeCipherState,
    tls1FinalFinishMac,
    12,
    tls1CertVerifyMac,
    "client finished",
    15,
    "server finished",
    15,
    tls1AlertCode
};

/* TLSv1 base SSL_METHOD (0x030644) */
static const SslMethod g_tls1BaseMethod = {
    0x0301,
    tls1New,
    tls1Clear,
    tls1Free,
    sslUndefinedFunction,
    sslUndefinedFunction,
    ssl3Read,
    ssl3Peek,
    ssl3Write,
    ssl3Shutdown,
    ssl3Renegotiate,
    ssl3RenegotiateCheck,
    ssl3Ctrl,
    ssl3CtxCtrl,
    ssl3GetCipherByChar,
    ssl3PutCipherByChar,
    ssl3Pending,
    ssl3NumCiphers,
    ssl3GetCipher,
    sslUndefinedConstFunction,
    tls1DefaultTimeout,
    &g_tls1EncData
};

/* Cipher alias table (37 entries at 0x0306A0) */
static SslCipher g_cipherAliases[37] = {
    { 0, "ALL",        0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU },
    { 0, "kRSA",       0x00000000U, 0x00000001U, 0x00000000U, 0x0000001FU },
    { 0, "kDHr",       0x00000000U, 0x00000002U, 0x00000000U, 0x0000001FU },
    { 0, "kDHd",       0x00000000U, 0x00000004U, 0x00000000U, 0x0000001FU },
    { 0, "kEDH",       0x00000000U, 0x00000010U, 0x00000000U, 0x0000001FU },
    { 0, "kFZA",       0x00000000U, 0x00000008U, 0x00000000U, 0x0000001FU },
    { 0, "DH",         0x00000000U, 0x00000016U, 0x00000000U, 0x0000001FU },
    { 0, "EDH",        0x00000000U, 0x000002F0U, 0x00000000U, 0x000003FFU },
    { 0, "aRSA",       0x00000000U, 0x00000020U, 0x00000000U, 0x000003E0U },
    { 0, "aDSS",       0x00000000U, 0x00000040U, 0x00000000U, 0x000003E0U },
    { 0, "aFZA",       0x00000000U, 0x00000080U, 0x00000000U, 0x000003E0U },
    { 0, "aNULL",      0x00000000U, 0x00000100U, 0x00000000U, 0x000003E0U },
    { 0, "aDH",        0x00000000U, 0x00000200U, 0x00000000U, 0x000003E0U },
    { 0, "DSS",        0x00000000U, 0x00000040U, 0x00000000U, 0x000003E0U },
    { 0, "DES",        0x00000000U, 0x00000400U, 0x00000000U, 0x0001FC00U },
    { 0, "3DES",       0x00000000U, 0x00000800U, 0x00000000U, 0x0001FC00U },
    { 0, "RC4",        0x00000000U, 0x00001000U, 0x00000000U, 0x0001FC00U },
    { 0, "RC2",        0x00000000U, 0x00002000U, 0x00000000U, 0x0001FC00U },
    { 0, "IDEA",       0x00000000U, 0x00004000U, 0x00000000U, 0x0001FC00U },
    { 0, "eNULL",      0x00000000U, 0x00010000U, 0x00000000U, 0x0001FC00U },
    { 0, "eFZA",       0x00000000U, 0x00008000U, 0x00000000U, 0x0001FC00U },
    { 0, "MD5",        0x00000000U, 0x00020000U, 0x00000000U, 0x00060000U },
    { 0, "SHA1",       0x00000000U, 0x00040000U, 0x00000000U, 0x00060000U },
    { 0, "SHA",        0x00000000U, 0x00040000U, 0x00000000U, 0x00060000U },
    { 0, "NULL",       0x00000000U, 0x00010000U, 0x00000000U, 0x0001FC00U },
    { 0, "RSA",        0x00000000U, 0x00000021U, 0x00000000U, 0x000003FFU },
    { 0, "ADH",        0x00000000U, 0x00000110U, 0x00000000U, 0x000003FFU },
    { 0, "FZA",        0x00000000U, 0x00008088U, 0x00000000U, 0x0001FFFFU },
    { 0, "EXP",        0x00000000U, 0x00100000U, 0x00000000U, 0x00300000U },
    { 0, "EXPORT",     0x00000000U, 0x00100000U, 0x00000000U, 0x00300000U },
    { 0, "SSLv2",      0x00000000U, 0x00400000U, 0x00000000U, 0x00C00000U },
    { 0, "SSLv3",      0x00000000U, 0x00800000U, 0x00000000U, 0x00C00000U },
    { 0, "TLSv1",      0x00000000U, 0x00800000U, 0x00000000U, 0x00C00000U },
    { 0, "LOW",        0x00000000U, 0x01000000U, 0x00000000U, 0x07000000U },
    { 0, "MEDIUM",     0x00000000U, 0x02000000U, 0x00000000U, 0x07000000U },
    { 0, "HIGH",       0x00000000U, 0x04000000U, 0x00000000U, 0x07000000U },
    { 0, "RC4-64-MD5", 0x00000000U, 0x01521021U, 0x00000408U, 0x0037FFFFU }
};

/* ==================== t1_clnt.c (0x020D2C..0x020E03) ==================== */

/* 0x020D2C */
static const SslMethod *LIBSSL_NOINLINE tls1GetClientMethod(int version)
{
    if (version == 0x0301) {
        return sceSsl_lib_42FFA860();
    }
    return NULL;
}

/* 0x020D5C */
const SslMethod *LIBSSL_NOINLINE sceSsl_lib_42FFA860(void)
{
    if (g_tls1ClientMethodInit != 0) {
        g_tls1ClientMethodInit = 0;
        g_tls1ClientMethod = *tls1BaseMethod();
        g_tls1ClientMethod.getSslMethod = tls1GetClientMethod;
        g_tls1ClientMethod.sslConnect = ssl3Connect;
    }
    return &g_tls1ClientMethod;
}

/* ==================== t1_enc.c (0x020E04..0x022097) ==================== */

/* 0x020E04 */
static int LIBSSL_NOINLINE tls1PHash(const SslEvpMd *md, const u8 *sec, int secLen,
                                     const u8 *seed, int seedLen, u8 *outBuf, int outLen)
{
    u8 aDigest[64];
    u32 aDigestLen = 0;
    u32 outChunkLen = 0;
    int chunkSize = sslEvpMdSize(md);
    SslHmacCtx *ctxMain = sslHmacCtxNew();
    SslHmacCtx *ctxNext = sslHmacCtxNew();
    int ret = 0;

    if (ctxMain != NULL && ctxNext != NULL && sslHmacInit(ctxMain, sec, secLen, md) != 0) {
        sslHmacUpdate(ctxMain, seed, (u32)seedLen);
        if (sslHmacFinal(ctxMain, aDigest, &aDigestLen) != 0) {
            while (sslHmacInit(ctxMain, NULL, 0, NULL) != 0) {
                sslHmacUpdate(ctxMain, aDigest, aDigestLen);
                if (sslHmacCtxCopy(ctxNext, ctxMain) == 0) {
                    break;
                }
                sslHmacUpdate(ctxMain, seed, (u32)seedLen);
                if (outLen <= chunkSize) {
                    if (sslHmacFinal(ctxMain, aDigest, &aDigestLen) != 0) {
                        sslMemcpy(outBuf, aDigest, (u32)outLen);
                        ret = 1;
                    }
                    break;
                }
                if (sslHmacFinal(ctxMain, outBuf, &outChunkLen) == 0) {
                    break;
                }
                outLen -= (int)outChunkLen;
                outBuf += outChunkLen;
                if (sslHmacFinal(ctxNext, aDigest, &aDigestLen) == 0) {
                    break;
                }
            }
        }
    }

    if (ctxMain != NULL) {
        sslHmacCtxFree(ctxMain);
    }
    if (ctxNext != NULL) {
        sslHmacCtxFree(ctxNext);
    }
    sslMemset(aDigest, 0, sizeof(aDigest));
    return ret;
}

/* 0x020FE0 */
static int LIBSSL_NOINLINE tls1Prf(const SslEvpMd *md5, const SslEvpMd *sha1,
                                   const u8 *seed, int seedLen, const u8 *sec, int secLen,
                                   u8 *outFirst, u8 *outScratch, int outLen)
{
    int halfLen = secLen / 2 + (secLen & 1);
    const u8 *secondHalf = sec + (secLen / 2);
    int idx;

    if (tls1PHash(md5, sec, halfLen, seed, seedLen, outFirst, outLen) == 0) {
        return 0;
    }
    if (tls1PHash(sha1, secondHalf, halfLen, seed, seedLen, outScratch, outLen) == 0) {
        return 0;
    }
    for (idx = 0; idx < outLen; idx++) {
        outFirst[idx] ^= outScratch[idx];
    }
    return 1;
}

/* 0x0210E0 */
static int LIBSSL_NOINLINE tls1GenerateKeyBlock(Ssl *ssl, u8 *keyBlock, u8 *scratch, int numBytes)
{
    u8 seedBuf[77];

    __builtin_memcpy(seedBuf, "key expansion", 13U);
    __builtin_memcpy(seedBuf + 13, ssl->ssl3State->serverRandom, 32U);
    __builtin_memcpy(seedBuf + 45, ssl->ssl3State->clientRandom, 32U);
    return tls1Prf(ssl->ctx->md5, ssl->ctx->sha1, seedBuf, 77,
                   ssl->session->masterKey, ssl->session->masterKeyLength,
                   keyBlock, scratch, numBytes);
}

/* 0x021268 */
static int LIBSSL_NOINLINE tls1ChangeCipherState(Ssl *ssl, int which)
{
    u8 labelSeed[80];
    u8 keyFinal[32];
    u8 keyScratch[32];
    u8 ivFinal[64];
    u8 ivScratch[64];
    Ssl3State *ssl3State = ssl->ssl3State;
    const SslEvpCipher *evpCipher = ssl3State->tmpNewSymEnc;
    const SslEvpMd *evpMd = ssl3State->tmpNewHash;
    int isExport = ((ssl3State->tmpNewCipher->algorithms & 0x100000U) != 0);
    SslEvpCipherCtx **cipherCtxSlot;
    void **macCtxSlot;
    u32 *macSizeSlot;
    u8 *macSecretDst;
    SslEvpCipherCtx *cipherCtx;
    u8 *keyBlock;
    int macLen;
    int keyLen;
    int ivLen;
    const u8 *macSrc;
    const u8 *keySrc;
    const u8 *ivSrc;
    const char *keyLabel;
    int isClientWrite;
    int neededLen;

    if ((which & 1) != 0) {
        sslMemset(ssl3State->readSequence, 0, sizeof(ssl3State->readSequence));
        macSizeSlot = &ssl->readMacSize;
        macCtxSlot = &ssl->readHash;
        cipherCtxSlot = &ssl->encReadCtx;
        macSecretDst = ssl3State->readMacSecret;
    } else {
        sslMemset(ssl3State->writeSequence, 0, sizeof(ssl3State->writeSequence));
        macSizeSlot = &ssl->writeMacSize;
        macCtxSlot = &ssl->writeHash;
        cipherCtxSlot = &ssl->encWriteCtx;
        macSecretDst = ssl3State->writeMacSecret;
    }

    if (*cipherCtxSlot == NULL) {
        *cipherCtxSlot = sslEvpCipherCtxNew();
        if (*cipherCtxSlot == NULL) {
            sceSsl_lib_9CDCA0D7(20, 210, 33, NULL, 0);
            return 0;
        }
    }
    if (*macCtxSlot == NULL) {
        *macCtxSlot = sslHmacCtxNew();
        if (*macCtxSlot == NULL) {
            sceSsl_lib_9CDCA0D7(20, 210, 33, NULL, 0);
            return 0;
        }
    }
    ssl->macFreeFunc = (SslFreeCb)sslHmacCtxFree;
    if (sslHmacInit((SslHmacCtx *)*macCtxSlot, NULL, 0, evpMd) == 0) {
        sceSsl_lib_9CDCA0D7(20, 210, 33, NULL, 0);
        return 0;
    }

    *macSizeSlot = (u32)sslEvpMdSize(evpMd);
    cipherCtx = *cipherCtxSlot;
    if (sslEvpCipherCtxInit(cipherCtx) == 0) {
        sceSsl_lib_9CDCA0D7(20, 210, 33, NULL, 0);
        return 0;
    }

    keyBlock = ssl3State->tmpKeyBlock;
    macLen = sslEvpMdSize(evpMd);
    keyLen = (int)(ssl3State->tmpNewCipher->algStrength & 0xFFU);
    if (keyLen == 0) {
        keyLen = sslEvpCipherKeyLength(evpCipher);
    }
    ivLen = sslEvpCipherIvLength(evpCipher);

    if (which == 0x12 || which == 0x21) {
        macSrc = keyBlock;
        keySrc = keyBlock + 2 * macLen;
        ivSrc = keyBlock + 2 * macLen + 2 * keyLen;
        neededLen = 2 * macLen + 2 * keyLen + 2 * ivLen;
        keyLabel = "client write key";
        isClientWrite = 1;
    } else {
        macSrc = keyBlock + macLen;
        keySrc = keyBlock + 2 * macLen + keyLen;
        ivSrc = keyBlock + 2 * macLen + 2 * keyLen + ivLen;
        neededLen = 2 * macLen + 2 * keyLen + 2 * ivLen;
        keyLabel = "server write key";
        isClientWrite = 0;
    }

    if (ssl3State->tmpKeyBlockLen < neededLen) {
        sceSsl_lib_9CDCA0D7(20, 210, 157, NULL, 0);
        return 0;
    }

    sslMemcpy(macSecretDst, macSrc, (u32)macLen);

    if (isExport != 0) {
        int fullKeyLen = sslEvpCipherKeyLength(evpCipher);

        sslMemcpy(labelSeed, keyLabel, 16U);
        sslMemcpy(labelSeed + 16, ssl3State->clientRandom, 32U);
        sslMemcpy(labelSeed + 48, ssl3State->serverRandom, 32U);
        if (tls1Prf(ssl->ctx->md5, ssl->ctx->sha1, labelSeed, 80,
                    keySrc, keyLen, keyFinal, keyScratch, fullKeyLen) == 0) {
            return 0;
        }
        keySrc = keyFinal;
        if (ivLen > 0) {
            sslMemcpy(labelSeed, "IV block", 8U);
            sslMemcpy(labelSeed + 8, ssl3State->clientRandom, 32U);
            sslMemcpy(labelSeed + 40, ssl3State->serverRandom, 32U);
            if (tls1Prf(ssl->ctx->md5, ssl->ctx->sha1, labelSeed, 72,
                        (const u8 *)g_tls1EmptySecret, 0,
                        ivFinal, ivScratch, 2 * ivLen) == 0) {
                return 0;
            }
            ivSrc = (isClientWrite != 0) ? ivFinal : (ivFinal + ivLen);
        }
    }

    ssl->session->keyArgLength = 0;
    if (sslEvpCipherInit(cipherCtx, evpCipher, keySrc, ivSrc, which & 2) == 0) {
        return 0;
    }
    sslMemset(keyFinal, 0, sizeof(keyFinal));
    sslMemset(keyScratch, 0, sizeof(keyScratch));
    sslMemset(ivFinal, 0, sizeof(ivFinal));
    sslMemset(ivScratch, 0, sizeof(ivScratch));
    return 1;
}

/* 0x021808 */
static int LIBSSL_NOINLINE tls1SetupKeyBlock(Ssl *ssl)
{
    const SslEvpCipher *evpCipher;
    const SslEvpMd *evpMd;
    int blockLen;
    u8 *keyBuf;
    u8 *scratchBuf;

    if (ssl->ssl3State->tmpKeyBlockLen != 0) {
        return 1;
    }
    if (sslCipherGetEvp(ssl->session->cipher, &evpCipher, &evpMd) == 0) {
        sceSsl_lib_9CDCA0D7(20, 212, 138, NULL, 0);
        return 0;
    }
    ssl->ssl3State->tmpNewSymEnc = evpCipher;
    ssl->ssl3State->tmpNewHash = evpMd;

    blockLen = 2 * (sslEvpCipherKeyLength(evpCipher) + sslEvpMdSize(evpMd) +
                    sslEvpCipherIvLength(evpCipher));
    ssl3CleanupKeyBlock(ssl);

    keyBuf = (u8 *)sceSsl_lib_0861D934((u32)blockLen);
    if (keyBuf != NULL) {
        scratchBuf = (u8 *)sceSsl_lib_0861D934((u32)blockLen);
        if (scratchBuf != NULL) {
            ssl->ssl3State->tmpKeyBlock = keyBuf;
            ssl->ssl3State->tmpKeyBlockLen = blockLen;
            if (tls1GenerateKeyBlock(ssl, keyBuf, scratchBuf, blockLen) != 0) {
                sslMemset(scratchBuf, 0, (u32)blockLen);
                sceSsl_lib_5E5C873A(scratchBuf);
                return 1;
            }
            return 0;
        }
    }
    sceSsl_lib_9CDCA0D7(20, 212, 33, NULL, 0);
    return 0;
}

/* 0x021950 */
static int LIBSSL_NOINLINE tls1Enc(Ssl *ssl, int sendFlag)
{
    Ssl3Record *rec;
    SslEvpCipherCtx *cipherCtx;
    const SslEvpCipher *evpCipher = NULL;
    int recLen;
    int blockSize;

    if (sendFlag != 0) {
        rec = &ssl->ssl3State->wrec;
        cipherCtx = ssl->encWriteCtx;
    } else {
        rec = &ssl->ssl3State->rrec;
        cipherCtx = ssl->encReadCtx;
    }
    if (cipherCtx != NULL) {
        evpCipher = sslEvpCipherCtxCipher(cipherCtx);
    }

    if (ssl->session == NULL || cipherCtx == NULL || evpCipher == NULL) {
        sslMemcpy(rec->data, rec->input, rec->length);
        rec->input = rec->data;
        return 1;
    }

    recLen = (int)rec->length;
    blockSize = sslEvpCipherCtxBlockSize(cipherCtx);
    if (blockSize != 1 && sendFlag != 0) {
        int padLen = blockSize - (recLen % blockSize);
        u8 padByte = (u8)(padLen - 1);
        int idx;

        if ((ssl->options & 0x200U) != 0 && (ssl->ssl3State->flags & 8U) != 0) {
            padByte = (u8)padLen;
        }
        for (idx = recLen; idx < recLen + padLen; idx++) {
            rec->input[idx] = padByte;
        }
        recLen += padLen;
        rec->length += (u32)padLen;
    }

    sslEvpCipher(cipherCtx, rec->data, rec->input, (u32)recLen);

    if (blockSize != 1 && sendFlag == 0) {
        int padByteVal = rec->data[recLen - 1];
        int trimLen = padByteVal + 1;
        int idx;

        if ((ssl->options & 0x200U) != 0) {
            if (sslMemcmp(ssl->ssl3State->readSequence, g_tls1ZeroSeq, 8) == 0 && (padByteVal & 1) == 0) {
                ssl->ssl3State->flags |= 8U;
            }
            if ((ssl->ssl3State->flags & 8U) != 0) {
                trimLen--;
            }
        }
        if ((int)rec->length < trimLen) {
            return 0;
        }
        for (idx = recLen - trimLen; idx < recLen; idx++) {
            if (rec->data[idx] != (u8)padByteVal) {
                return 0;
            }
        }
        rec->length -= (u32)trimLen;
    }
    return 1;
}

/* 0x021B8C */
static int LIBSSL_NOINLINE tls1CertVerifyMac(Ssl *ssl, SslEvpMdCtx *mdCtx, u8 *outBuf)
{
    u32 outLen;

    if (sslEvpMdCtxCopy(ssl->mdCtx, mdCtx) == 0) {
        return 0;
    }
    sslEvpDigestFinal(ssl->mdCtx, outBuf, &outLen);
    return (int)outLen;
}

/* 0x021BE0 */
static int LIBSSL_NOINLINE tls1FinalFinishMac(Ssl *ssl, SslEvpMdCtx *md5Ctx, SslEvpMdCtx *sha1Ctx,
                                              const char *label, int labelLen, u8 *outBuf)
{
    u8 seedBuf[64];
    u8 scratchBuf[16];
    u32 digestLen;
    u8 *cursor;

    sslMemcpy(seedBuf, label, (u32)labelLen);
    if (sslEvpMdCtxCopy(ssl->mdCtx, md5Ctx) == 0) {
        return 0;
    }
    sslEvpDigestFinal(ssl->mdCtx, seedBuf + labelLen, &digestLen);
    cursor = seedBuf + labelLen + digestLen;

    if (sslEvpMdCtxCopy(ssl->mdCtx, sha1Ctx) == 0) {
        return 0;
    }
    sslEvpDigestFinal(ssl->mdCtx, cursor, &digestLen);
    cursor += digestLen;

    if (tls1Prf(ssl->ctx->md5, ssl->ctx->sha1, seedBuf, (int)(cursor - seedBuf),
                ssl->session->masterKey, ssl->session->masterKeyLength,
                outBuf, scratchBuf, 12) == 0) {
        return 0;
    }
    return 12;
}

/* 0x021CE8 */
static int LIBSSL_NOINLINE tls1Mac(Ssl *ssl, u8 *outMd, int sendFlag)
{
    u8 headerBuf[5];
    u32 mdLen;
    SslHmacCtx *hmacCtx;
    u8 *seqNum;
    u8 *macSecret;
    Ssl3Record *rec;
    int idx;

    if (sendFlag != 0) {
        hmacCtx = (SslHmacCtx *)ssl->writeHash;
        seqNum = ssl->ssl3State->writeSequence;
        macSecret = ssl->ssl3State->writeMacSecret;
        rec = &ssl->ssl3State->wrec;
    } else {
        hmacCtx = (SslHmacCtx *)ssl->readHash;
        seqNum = ssl->ssl3State->readSequence;
        macSecret = ssl->ssl3State->readMacSecret;
        rec = &ssl->ssl3State->rrec;
    }

    mdLen = (u32)sslEvpMdSize(sslHmacCtxMd(hmacCtx));
    headerBuf[0] = (u8)rec->type;
    headerBuf[1] = 3;
    headerBuf[2] = 1;
    headerBuf[3] = (u8)(rec->length >> 8);
    headerBuf[4] = (u8)(rec->length & 0xFF);

    if (sslHmacInit(hmacCtx, macSecret, (int)mdLen, NULL) == 0) {
        return 0;
    }
    sslHmacUpdate(hmacCtx, seqNum, 8U);
    sslHmacUpdate(hmacCtx, headerBuf, 5U);
    sslHmacUpdate(hmacCtx, rec->input, rec->length);
    if (sslHmacFinal(hmacCtx, outMd, &mdLen) == 0) {
        return 0;
    }

    for (idx = 7; idx >= 0; idx--) {
        seqNum[idx]++;
        if (seqNum[idx] != 0) {
            break;
        }
    }
    return (int)mdLen;
}

/* 0x021E28 */
static int LIBSSL_NOINLINE tls1GenerateMasterSecret(Ssl *ssl, u8 *outMaster,
                                                    const u8 *preMaster, int preMasterLen)
{
    u8 seedBuf[77];
    u8 scratchBuf[48];

    (void)outMaster;
    __builtin_memcpy(seedBuf, "master secret", 13U);
    __builtin_memcpy(seedBuf + 13, ssl->ssl3State->clientRandom, 32U);
    __builtin_memcpy(seedBuf + 45, ssl->ssl3State->serverRandom, 32U);
    if (tls1Prf(ssl->ctx->md5, ssl->ctx->sha1, seedBuf, 77,
                preMaster, preMasterLen,
                ssl->session->masterKey, scratchBuf, 48) == 0) {
        return 0;
    }
    return 48;
}

/* 0x021FAC */
static int LIBSSL_NOINLINE tls1AlertCode(int code)
{
    switch (code) {
    case 0:   return 0;
    case 10:  return 10;
    case 20:  return 20;
    case 21:  return 21;
    case 22:  return 22;
    case 30:  return 30;
    case 40:  return 40;
    case 41:  return -1;
    case 42:  return 42;
    case 43:  return 43;
    case 44:  return 44;
    case 45:  return 45;
    case 46:  return 46;
    case 47:  return 47;
    case 48:  return 48;
    case 49:  return 49;
    case 50:  return 50;
    case 51:  return 51;
    case 60:  return 60;
    case 70:  return 70;
    case 71:  return 71;
    case 80:  return 80;
    case 90:  return 90;
    case 100: return 100;
    default:  return -1;
    }
}

/* ==================== t1_lib.c (0x022098..0x02213B) ==================== */

/* 0x022098 */
static int LIBSSL_NOINLINE tls1DefaultTimeout(void)
{
    return 7200;
}

/* 0x0220A0 */
const SslMethod *LIBSSL_NOINLINE tls1BaseMethod(void)
{
    return &g_tls1BaseMethod;
}

/* 0x0220AC */
static int LIBSSL_NOINLINE tls1New(Ssl *ssl)
{
    if (ssl3New(ssl) == 0) {
        return 0;
    }
    ssl->method->sslClear(ssl);
    return 1;
}

/* 0x0220F4 */
static void LIBSSL_NOINLINE tls1Free(Ssl *ssl)
{
    ssl3Free(ssl);
}

/* 0x022110 */
static void LIBSSL_NOINLINE tls1Clear(Ssl *ssl)
{
    ssl3Clear(ssl);
    ssl->version = 0x0301;
}

/* ==================== ssl_ciph.c (0x02213C..0x022F2F) ==================== */

/* 0x02213C */
static int LIBSSL_NOINLINE sslCipherNameCmp(const void *lhs, const void *rhs)
{
    const SslCipher *const *leftPtr = (const SslCipher *const *)lhs;
    const SslCipher *const *rightPtr = (const SslCipher *const *)rhs;
    return sslStrcmp((*leftPtr)->name, (*rightPtr)->name);
}

/* 0x022164 */
static int LIBSSL_NOINLINE sslCipherIdMaskCmp(const void *lhs, const void *rhs)
{
    const SslCipher *const *leftPtr = (const SslCipher *const *)lhs;
    const SslCipher *const *rightPtr = (const SslCipher *const *)rhs;
    u32 leftId = (*leftPtr)->id & 0x00FFFFFFU;
    u32 rightId = (*rightPtr)->id & 0x00FFFFFFU;

    if (leftId == rightId) {
        return 0;
    }
    return (leftId > rightId) ? 1 : -1;
}

/* 0x02219C */
static void LIBSSL_NOINLINE sslLoadCiphers(void)
{
    g_sslCipherTablesNeedInit = 0;
    g_sslCipherTable[0] = sslEvpGetCipherByName("DES-CBC");
    g_sslCipherTable[1] = sslEvpGetCipherByName("DES-EDE3-CBC");
    g_sslCipherTable[2] = sslEvpGetCipherByName("RC4");
    g_sslCipherTable[3] = sslEvpGetCipherByName("RC2-CBC");
    g_sslCipherTable[4] = sslEvpGetCipherByName("IDEA-CBC");
    g_sslDigestTable[0] = sslEvpGetDigestByName("MD5");
    g_sslDigestTable[1] = sslEvpGetDigestByName("SHA1");
}

/* 0x022240 */
int LIBSSL_NOINLINE sslCipherGetEvp(const SslCipher *cipher,
                                    const SslEvpCipher **outCipher,
                                    const SslEvpMd **outMd)
{
    u32 encMask;
    u32 macMask;

    if (cipher == NULL) {
        return 0;
    }
    encMask = cipher->algorithms & 0x1FC00U;
    switch (encMask) {
    case 0x00400U: *outCipher = g_sslCipherTable[0]; break;
    case 0x00800U: *outCipher = g_sslCipherTable[1]; break;
    case 0x01000U: *outCipher = g_sslCipherTable[2]; break;
    case 0x02000U: *outCipher = g_sslCipherTable[3]; break;
    case 0x04000U: *outCipher = g_sslCipherTable[4]; break;
    case 0x10000U: *outCipher = sslEvpEncNull();     break;
    default:       *outCipher = NULL;                break;
    }

    macMask = cipher->algorithms & 0x60000U;
    if (macMask == 0x20000U) {
        *outMd = g_sslDigestTable[0];
    } else if (macMask == 0x40000U) {
        *outMd = g_sslDigestTable[1];
    } else {
        *outMd = NULL;
    }

    return (*outCipher != NULL && *outMd != NULL) ? 1 : 0;
}

/* 0x0223B4 */
static void LIBSSL_NOINLINE sslCipherOrderMoveTail(SslCipherOrder **headPtr,
                                                   SslCipherOrder *node,
                                                   SslCipherOrder **tailPtr)
{
    if (node == *tailPtr) {
        return;
    }
    if (node == *headPtr) {
        *headPtr = node->next;
    }
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }
    (*tailPtr)->next = node;
    node->next = NULL;
    node->prev = *tailPtr;
    *tailPtr = node;
}

/* 0x022410 */
SslStack *LIBSSL_NOINLINE sslCreateCipherList(const SslMethod *method,
                                              SslStack **outCipherList,
                                              SslStack **outCipherListById,
                                              const char *ruleStr)
{
    SslStack *resultList = NULL;
    SslStack *selected = NULL;
    SslStack *nameStack = NULL;
    SslStack *idStack = NULL;
    SslCipherOrder *orderArray = NULL;
    SslCipherRule *rules = NULL;
    char *expandedRule = NULL;
    SslCipherOrder *listHead = NULL;
    SslCipherOrder *listTail = NULL;
    u32 disabledMask;
    int numMethodCiphers;
    int availCount = 0;
    int ruleCount = 0;
    int idx;
    int failed = 0;

    if (ruleStr == NULL) {
        return NULL;
    }

    if (sslStrncmp(ruleStr, "DEFAULT", 7) == 0) {
        int strLen = sslStrlen(ruleStr);
        expandedRule = (char *)sceSsl_lib_0861D934((u32)(strLen + 49));
        if (expandedRule == NULL) {
            sceSsl_lib_9CDCA0D7(20, 166, 33, NULL, 0);
            return NULL;
        }
        __builtin_memcpy(expandedRule, "ALL:!ADH:RC4+RSA:+HIGH:+MEDIUM:+LOW:+SSLv2:+EXP", 48U);
        sslStrcat(expandedRule, ":");
        sslStrcat(expandedRule, ruleStr + 7);
        ruleStr = expandedRule;
    }

    if (g_sslCipherTablesNeedInit != 0) {
        sslLoadCiphers();
    }

    sslAlgorithmSupported(2);
    sslAlgorithmSupported(1);
    disabledMask = 606U;
    if (sslAlgorithmSupported(4) == 0) {
        disabledMask |= 0x10000U;
    }
    if (sslAlgorithmSupported(8) == 0) {
        disabledMask |= 0x100U;
    }
    if (g_sslCipherTable[0] == NULL) disabledMask |= (1U << 10);
    if (g_sslCipherTable[1] == NULL) disabledMask |= (1U << 11);
    if (g_sslCipherTable[2] == NULL) disabledMask |= (1U << 12);
    if (g_sslCipherTable[3] == NULL) disabledMask |= (1U << 13);
    if (g_sslCipherTable[4] == NULL) disabledMask |= (1U << 14);
    if (g_sslCipherTable[5] == NULL) disabledMask |= (1U << 15);
    if (g_sslDigestTable[0] == NULL) disabledMask |= (1U << 17);
    if (g_sslDigestTable[1] == NULL) disabledMask |= (1U << 18);

    numMethodCiphers = method->numCiphers();
    selected = sslSkNew(NULL);
    nameStack = sslSkNew(sslCipherNameCmp);
    idStack = sslSkNew(sslCipherIdMaskCmp);
    orderArray = (SslCipherOrder *)sceSsl_lib_0861D934((u32)(numMethodCiphers * (int)sizeof(SslCipherOrder)));
    if (selected == NULL || nameStack == NULL || idStack == NULL || orderArray == NULL) {
        failed = 1;
    }

    if (failed == 0) {
        for (idx = 0; idx < numMethodCiphers; idx++) {
            const SslCipher *ciph = method->getCipher(idx);
            if (ciph != NULL && ciph->valid != 0 && (ciph->algorithms & disabledMask) == 0) {
                orderArray[availCount].cipher = ciph;
                orderArray[availCount].active = 0;
                orderArray[availCount].next = NULL;
                orderArray[availCount].prev = NULL;
                availCount++;
                if (sslSkPush(nameStack, (void *)ciph) == 0 ||
                    sslSkPush(idStack, (void *)ciph) == 0) {
                    failed = 1;
                    break;
                }
            }
        }
    }

    if (failed == 0) {
        for (idx = 1; idx < availCount - 1; idx++) {
            orderArray[idx].prev = &orderArray[idx - 1];
            orderArray[idx].next = &orderArray[idx + 1];
        }
        if (availCount > 0) {
            listHead = &orderArray[0];
            listHead->next = (availCount > 1) ? &orderArray[1] : NULL;
            listHead->prev = NULL;
            listTail = &orderArray[availCount - 1];
            listTail->prev = (availCount >= 2) ? &orderArray[availCount - 2] : NULL;
            listTail->next = NULL;
        }

        g_cipherAliases[0].algorithms = ~disabledMask;
        for (idx = 0; idx < 37; idx++) {
            u32 aliasAlg = g_cipherAliases[idx].algorithms;
            if ((aliasAlg & disabledMask) != aliasAlg) {
                if (sslSkPush(nameStack, &g_cipherAliases[idx]) == 0) {
                    failed = 1;
                    break;
                }
            }
        }
    }

    if (failed == 0) {
        int maxRules = 1;
        const char *scan = ruleStr;
        while (*scan != '\0') {
            char ch = *scan++;
            if (ch == ':' || ch == ' ' || ch == ';' || ch == ',') {
                maxRules++;
            }
        }
        rules = (SslCipherRule *)sceSsl_lib_0861D934((u32)(maxRules * (int)sizeof(SslCipherRule)));
        if (rules == NULL) {
            failed = 1;
        } else {
            const char *cur = ruleStr;
            SslCipherRule *curRule = rules;

            sslMemset(rules, 0, (u32)(maxRules * (int)sizeof(SslCipherRule)));

            while (*cur != '\0') {
                char lead = *cur;
                int ruleOp = 1;

                if (lead == '-') {
                    ruleOp = 3;
                    cur++;
                } else if (lead == '+') {
                    ruleOp = 4;
                    cur++;
                } else if (lead == '!') {
                    ruleOp = 2;
                    cur++;
                } else if (lead == ':' || lead == ' ' || lead == ';' || lead == ',') {
                    cur++;
                    continue;
                }

                curRule->ruleOp = ruleOp;
                {
                    int ruleValid = 1;
                    for (;;) {
                        char tokenBuf[40];
                        int tokLen = 0;
                        char termCh;
                        int multiPart = 0;
                        SslCipher lookupKey;
                        int foundIdx;
                        const SslCipher *matched = NULL;

                        while (tokLen < 38) {
                            char c = *cur;
                            if ((c >= 'A' && c <= 'Z') ||
                                (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') ||
                                c == '-') {
                                tokenBuf[tokLen++] = c;
                                cur++;
                            } else {
                                break;
                            }
                        }
                        tokenBuf[tokLen] = '\0';
                        termCh = *cur;

                        if (tokLen == 0) {
                            while (*cur != '\0' && *cur != ':' && *cur != ' ' &&
                                   *cur != ';' && *cur != ',') {
                                cur++;
                            }
                            ruleValid = 0;
                            break;
                        }

                        if (termCh == '+') {
                            multiPart = 1;
                            cur++;
                        }

                        lookupKey.name = tokenBuf;
                        foundIdx = sslSkFind(nameStack, &lookupKey);
                        if (foundIdx >= 0) {
                            curRule->exactId = 0;
                            matched = (const SslCipher *)nameStack->data[foundIdx];
                        } else {
                            const char *hexPtr = tokenBuf;
                            u32 parsedId = 0;
                            int hexOk = 1;

                            if (hexPtr[0] == '0' && hexPtr[1] == 'x') {
                                hexPtr += 2;
                            }
                            while (*hexPtr != '\0') {
                                char hc = *hexPtr++;
                                if (hc >= '0' && hc <= '9') {
                                    parsedId = (parsedId << 4) | (u32)(hc - '0');
                                } else if (hc >= 'A' && hc <= 'F') {
                                    parsedId = (parsedId << 4) | (u32)(hc - 'A' + 10);
                                } else if (hc >= 'a' && hc <= 'f') {
                                    parsedId = (parsedId << 4) | (u32)(hc - 'a' + 10);
                                } else {
                                    hexOk = 0;
                                    break;
                                }
                            }
                            if (hexOk != 0) {
                                lookupKey.id = parsedId;
                                foundIdx = sslSkFind(idStack, &lookupKey);
                                if (foundIdx >= 0) {
                                    matched = (const SslCipher *)idStack->data[foundIdx];
                                    curRule->exactId = matched->id;
                                }
                            }
                            if (matched == NULL) {
                                ruleValid = 0;
                                break;
                            }
                        }

                        curRule->algBits |= matched->algorithms;
                        curRule->maskBits |= matched->strengthMask;
                        curRule->exactName = (matched->valid == 1) ? 1 : 0;
                        curRule->name = matched->name;
                        if (multiPart == 0 || curRule->exactName != 0) {
                            break;
                        }
                    }

                    if (ruleValid != 0) {
                        ruleCount++;
                        curRule++;
                    }
                    while (*cur == ':' || *cur == ' ' || *cur == ';' || *cur == ',') {
                        cur++;
                    }
                }
            }
        }
    }

    if (failed == 0) {
        for (idx = 0; idx < ruleCount; idx++) {
            SslCipherRule *rule = &rules[idx];
            SslCipherOrder *curNode = listHead;
            SslCipherOrder *stopNode = listTail;
            SslCipherOrder *prevVisited = NULL;

            while (curNode != NULL && prevVisited != stopNode) {
                SslCipherOrder *nextNode = curNode->next;
                const SslCipher *ciph = curNode->cipher;
                int matches = 1;

                prevVisited = curNode;
                if (rule->exactName != 0 && sslStrcmp(rule->name, ciph->name) != 0) {
                    matches = 0;
                }
                if (matches != 0 && rule->exactId != 0 && ciph->id != rule->exactId) {
                    matches = 0;
                }
                if (matches != 0) {
                    u32 masked = rule->maskBits & ciph->algorithms;
                    if (masked == 0 || (masked & rule->algBits) != masked) {
                        matches = 0;
                    }
                }

                if (matches != 0) {
                    if (rule->ruleOp == 1) {
                        if (curNode->active == 0) {
                            sslCipherOrderMoveTail(&listHead, curNode, &listTail);
                            curNode->active = 1;
                        }
                    } else if (rule->ruleOp == 4) {
                        if (curNode->active != 0) {
                            sslCipherOrderMoveTail(&listHead, curNode, &listTail);
                        }
                    } else if (rule->ruleOp == 3) {
                        curNode->active = 0;
                    } else if (rule->ruleOp == 2) {
                        if (listHead == curNode) {
                            listHead = curNode->next;
                        } else {
                            curNode->prev->next = curNode->next;
                        }
                        if (listTail == curNode) {
                            listTail = curNode->prev;
                        }
                        curNode->active = 0;
                        if (curNode->next != NULL) {
                            curNode->next->prev = curNode->prev;
                        }
                        if (curNode->prev != NULL) {
                            curNode->prev->next = curNode->next;
                        }
                        curNode->next = NULL;
                        curNode->prev = NULL;
                    }
                }
                curNode = nextNode;
            }
        }

        {
            SslCipherOrder *walk = listHead;
            while (walk != NULL) {
                if (walk->active != 0) {
                    sslSkPush(selected, (void *)walk->cipher);
                }
                walk = walk->next;
            }
        }

        if (outCipherList != NULL) {
            if (*outCipherList != NULL) {
                sslSkFree(*outCipherList);
            }
            *outCipherList = selected;
        }
        if (outCipherListById != NULL) {
            SslStack *dupStack;
            if (*outCipherListById != NULL) {
                sslSkFree(*outCipherListById);
            }
            dupStack = sslSkDup(selected);
            *outCipherListById = dupStack;
            if (dupStack != NULL) {
                sslSkSetCmpFunc(dupStack, sslCipherIdCmp);
                resultList = selected;
                selected = NULL;
            }
        } else {
            resultList = selected;
            selected = NULL;
        }
    }

    if (expandedRule != NULL) {
        sceSsl_lib_5E5C873A(expandedRule);
    }
    if (rules != NULL) {
        sceSsl_lib_5E5C873A(rules);
    }
    if (selected != NULL) {
        sslSkFree(selected);
    }
    if (nameStack != NULL) {
        sslSkFree(nameStack);
    }
    if (idStack != NULL) {
        sslSkFree(idStack);
    }
    if (orderArray != NULL) {
        sceSsl_lib_5E5C873A(orderArray);
    }
    return resultList;
}

/* 0x022F18 */
const char *LIBSSL_NOINLINE sceSsl_lib_A2CE8DCD(const SslCipher *cipher)
{
    if (cipher != NULL) {
        return cipher->name;
    }
    return "(NONE)";
}

/* ==================== ssl_sess.c (0x022F30..0x0238A7) ==================== */

/* 0x022F30 */
SslSession *LIBSSL_NOINLINE sceSsl_lib_EF5A4BD4(const Ssl *ssl)
{
    return ssl->session;
}

/* 0x022F38 */
static SslSession *LIBSSL_NOINLINE sslSessionNew(void)
{
    SslSession *session = (SslSession *)sceSsl_lib_0861D934(sizeof(SslSession));

    if (session == NULL) {
        sceSsl_lib_9CDCA0D7(20, 189, 33, NULL, 0);
        return NULL;
    }
    sslMemset(session, 0, sizeof(SslSession));
    session->references = 1;
    session->timeout = 304;
    sceSsl_lib_A7BED83B(&session->createdTime);
    session->prev = NULL;
    session->next = NULL;
    sslCryptoNewExData(g_sslSessionExData, session, &session->exData);
    return session;
}

/* 0x022FD0 */
int LIBSSL_NOINLINE sslGetNewSession(Ssl *ssl, int generateId)
{
    SslSession *session = sslSessionNew();
    int timeout;

    if (session == NULL) {
        return 0;
    }
    timeout = ssl->ctx->sessionTimeout;
    if (timeout == 0) {
        timeout = sceSsl_lib_D49A570C(ssl);
    }
    session->timeout = timeout;
    if (ssl->session != NULL) {
        sceSsl_lib_BDAD0841(ssl->session);
        ssl->session = NULL;
    }

    if (generateId != 0) {
        if (ssl->version == 2) {
            session->sslVersion = 2;
            session->sessionIdLength = 16;
        } else if (ssl->version == 0x0300 || ssl->version == 0x0301) {
            session->sslVersion = ssl->version;
            session->sessionIdLength = 32;
        } else {
            sceSsl_lib_9CDCA0D7(20, 181, 259, NULL, 0);
            sceSsl_lib_BDAD0841(session);
            return 0;
        }

        for (;;) {
            void *existing;
            sceSsl_lib_21F2A35C(session->sessionId, (int)session->sessionIdLength);
            sceSsl_lib_D84E2411(5, 12, g_sessFilePath, 260);
            existing = sslLhRetrieve(ssl->ctx->sessions, session);
            sceSsl_lib_D84E2411(6, 12, g_sessFilePath, 262);
            if (existing == NULL) {
                break;
            }
        }
    } else {
        session->sessionIdLength = 0;
    }

    sslMemcpy(session->sidCtx, ssl->sidCtx, ssl->sidCtxLength);
    session->sidCtxLength = ssl->sidCtxLength;
    ssl->session = session;
    session->sslVersion = ssl->version;
    return 1;
}

/* 0x023140 */
int LIBSSL_NOINLINE sslCtxAddSession(SslCtx *ctx, SslSession *session)
{
    SslSession *prevEntry;
    int added = 0;

    sslCryptoAddLock(&session->references, 1, 14, g_sessFilePath, 412);
    sceSsl_lib_D84E2411(9, 12, g_sessFilePath, 414);
    prevEntry = (SslSession *)sslLhInsert(ctx->sessions, session);
    if (prevEntry != NULL) {
        sceSsl_lib_BDAD0841(prevEntry);
    } else {
        sslSessionListAddHead(ctx, session);
        added = 1;
        if (sceSsl_lib_B4D78E98(ctx, 43, 0, NULL) > 0) {
            while (sceSsl_lib_B4D78E98(ctx, 20, 0, NULL) >
                   sceSsl_lib_B4D78E98(ctx, 43, 0, NULL)) {
                if (sceSsl_lib_B81AD643(ctx, ctx->sessionCacheTail) == 0) {
                    break;
                }
                ctx->stats.sessCacheFull++;
            }
        }
    }
    sceSsl_lib_D84E2411(10, 12, g_sessFilePath, 443);
    return added;
}

/* 0x023278 */
int LIBSSL_NOINLINE sceSsl_lib_B81AD643(SslCtx *ctx, SslSession *session)
{
    SslSession *removed;
    int didRemove = 0;

    if (session == NULL || session->sessionIdLength == 0) {
        return 0;
    }
    sceSsl_lib_D84E2411(9, 12, g_sessFilePath, 456);
    removed = (SslSession *)sslLhDelete(ctx->sessions, session);
    if (removed != NULL) {
        didRemove = 1;
        sslSessionListRemove(ctx, session);
    }
    sceSsl_lib_D84E2411(10, 12, g_sessFilePath, 464);

    if (didRemove != 0) {
        removed->notResumable = 1;
        if (ctx->removeSessionCb != NULL) {
            ctx->removeSessionCb(ctx, removed);
        }
        sceSsl_lib_BDAD0841(removed);
    }
    return didRemove;
}

/* 0x023360 */
void LIBSSL_NOINLINE sceSsl_lib_BDAD0841(SslSession *session)
{
    if (sslCryptoAddLock(&session->references, -1, 14, g_sessFilePath, 480) > 0) {
        return;
    }
    sslCryptoFreeExData(g_sslSessionExData, session, &session->exData);
    session->keyArg[0] = 0;
    session->keyArg[1] = 0;
    session->keyArg[2] = 0;
    session->keyArg[3] = 0;
    session->keyArg[4] = 0;
    session->keyArg[5] = 0;
    session->keyArg[6] = 0;
    session->keyArg[7] = 0;
    sslMemset(session->masterKey, 0, sizeof(session->masterKey));
    sslMemset(session->sessionId, 0, sizeof(session->sessionId));
    if (session->sessCert != NULL) {
        sslCertFree(session->sessCert);
    }
    if (session->peer != NULL) {
        sceSsl_lib_C1A2E14B(session->peer);
    }
    if (session->ciphers != NULL) {
        sslSkFree(session->ciphers);
    }
    sslMemset(session, 0, sizeof(SslSession));
    sceSsl_lib_5E5C873A(session);
}

/* 0x023430 */
int LIBSSL_NOINLINE sceSsl_lib_86D0771E(SslSession *session)
{
    return sslCryptoAddLock(&session->references, 1, 14, g_sessFilePath, 502);
}

/* 0x023460 */
int LIBSSL_NOINLINE sceSsl_lib_69F1B678(Ssl *ssl, SslSession *session)
{
    if (session != NULL) {
        const SslMethod *method = ssl->ctx->method->getSslMethod(session->sslVersion);
        if (method == NULL) {
            method = ssl->method->getSslMethod(session->sslVersion);
            if (method == NULL) {
                sceSsl_lib_9CDCA0D7(20, 196, 240, NULL, 0);
                return 0;
            }
        }
        if (method != ssl->method) {
            int timeout;
            if (sslSetSslMethod(ssl, method) == 0) {
                return 0;
            }
            timeout = ssl->ctx->sessionTimeout;
            session->timeout = (timeout != 0) ? timeout : sceSsl_lib_D49A570C(ssl);
        }
        sslCryptoAddLock(&session->references, 1, 14, g_sessFilePath, 530);
        if (ssl->session != NULL) {
            sceSsl_lib_BDAD0841(ssl->session);
        }
        ssl->session = session;
        return 1;
    }

    if (ssl->session != NULL) {
        sceSsl_lib_BDAD0841(ssl->session);
        ssl->session = NULL;
    }
    if (ssl->ctx->method == ssl->method) {
        return 1;
    }
    return (sslSetSslMethod(ssl, ssl->ctx->method) != 0) ? 1 : 0;
}

/* 0x0235A4 */
int LIBSSL_NOINLINE sceSsl_lib_4214B7AF(SslCtx *ctx, int timeout)
{
    int prev;

    if (ctx == NULL) {
        return 0;
    }
    prev = ctx->sessionTimeout;
    ctx->sessionTimeout = timeout;
    return prev;
}

/* 0x0235BC */
int LIBSSL_NOINLINE sceSsl_lib_795B9EDF(const SslCtx *ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    return ctx->sessionTimeout;
}

/* 0x0235D0 */
static void LIBSSL_NOINLINE sslSessionTimeoutCb(SslSession *session, SslSessionTimeoutArg *arg)
{
    SceSslTimeSpec expireTime;

    sceSsl_lib_665D02F6(&expireTime, &session->createdTime, session->timeout, 0);
    if (sceSsl_lib_047AB6BB(&expireTime, &arg->nowTime) <= 0) {
        sslLhDelete(arg->cache, session);
        sslSessionListRemove(arg->ctx, session);
        session->notResumable = 1;
        if (arg->ctx->removeSessionCb != NULL) {
            arg->ctx->removeSessionCb(arg->ctx, session);
        }
        sceSsl_lib_BDAD0841(session);
    }
}

/* 0x023664 */
void LIBSSL_NOINLINE sceSsl_lib_4E0F2CCC(SslCtx *ctx, int useCurrentTime)
{
    SslSessionTimeoutArg arg;
    u32 savedDownLoad;

    arg.ctx = ctx;
    arg.cache = ctx->sessions;
    if (arg.cache == NULL) {
        return;
    }
    if (useCurrentTime != 0) {
        sceSsl_lib_A7BED83B(&arg.nowTime);
    } else {
        sceSsl_lib_E7BFC0EF((const u8 *)"99991231235959.9Z", 17U, 2, &arg.nowTime);
    }
    sceSsl_lib_D84E2411(9, 12, g_sessFilePath, 587);
    savedDownLoad = arg.cache->downLoad;
    arg.cache->downLoad = 0;
    sslLhDoAllArg(arg.cache, (SslLhDoAllArgFn)sslSessionTimeoutCb, &arg);
    arg.cache->downLoad = savedDownLoad;
    sceSsl_lib_D84E2411(10, 12, g_sessFilePath, 592);
}

/* 0x023724 */
int LIBSSL_NOINLINE sslBadSessionCheck(Ssl *ssl)
{
    int state;

    if (ssl->session == NULL || (ssl->shutdown & 1) != 0) {
        return 0;
    }
    state = sceSsl_lib_104F749D(ssl);
    if ((state & 0x3000) == state || (state & 0x4000) != 0 || state == 3) {
        return 0;
    }
    ssl->session->notResumable = 1;
    sceSsl_lib_B81AD643(ssl->ctx, ssl->session);
    return 1;
}

/* 0x0237C8 */
void LIBSSL_NOINLINE sslSessionListRemove(SslCtx *ctx, SslSession *session)
{
    SslSession *next = session->next;
    SslSession *prev = session->prev;
    SslSession *headSentinel = (SslSession *)&ctx->sessionCacheHead;
    SslSession *tailSentinel = (SslSession *)&ctx->sessionCacheTail;

    if (next == NULL || prev == NULL) {
        return;
    }
    if (next == tailSentinel) {
        if (prev == headSentinel) {
            ctx->sessionCacheHead = NULL;
            ctx->sessionCacheTail = NULL;
        } else {
            ctx->sessionCacheTail = prev;
            prev->next = next;
        }
    } else {
        if (prev == headSentinel) {
            ctx->sessionCacheHead = next;
            next->prev = prev;
        } else {
            next->prev = prev;
            prev->next = next;
        }
    }
    session->prev = NULL;
    session->next = NULL;
}

/* 0x02382C */
void LIBSSL_NOINLINE sslSessionListAddHead(SslCtx *ctx, SslSession *session)
{
    SslSession *headSentinel = (SslSession *)&ctx->sessionCacheHead;
    SslSession *tailSentinel = (SslSession *)&ctx->sessionCacheTail;
    SslSession *oldHead;

    if (session->next != NULL && session->prev != NULL) {
        sslSessionListRemove(ctx, session);
    }
    oldHead = ctx->sessionCacheHead;
    if (oldHead != NULL) {
        oldHead->prev = session;
        session->next = oldHead;
        session->prev = headSentinel;
        ctx->sessionCacheHead = session;
    } else {
        ctx->sessionCacheHead = session;
        ctx->sessionCacheTail = session;
        session->next = tailSentinel;
        session->prev = headSentinel;
    }
}
