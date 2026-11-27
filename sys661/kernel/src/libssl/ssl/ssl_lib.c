/*
 * RSA BSAFE SSL-C EVP_PKEY helpers, SSLv3 encryption/library, SSL_CTX and SSL
 * public API (p_lib.c, s3_both.c, s3_enc.c, s3_lib.c, ssl_algs.c, ssl_cert.c,
 * ssl_lib.c, ssl_rsa.c — 0x0238A8..0x027D9F).
 */
#include "libssl_internal.h"

static int g_ssl3CipherSortInit = 1;
static int g_sslCertExDataIndex = -1;
static const SslCipher *g_ssl3CiphersById[35];
static SslStack *g_sslExData;
static SslStack *g_sslCtxExData;
static u32 g_sslEnabledAlgorithms;

/* SSLv3 MAC pad blocks (0x030A1C and 0x030A4C) */
static const u8 g_ssl3PadOne[48] = {
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36
};

static const u8 g_ssl3PadTwo[48] = {
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C
};

static const char *g_ssl3MasterSecretSalt[3] = { "A", "BB", "CCC" };

/* SSLv3 cipher table (35 entries at 0x030A88) */
static const SslCipher g_ssl3Ciphers[35] = {
    { 1, "NULL-MD5",                0x03000001U, 0x00A30021U, 0x00000000U, 0x0037FFFFU },
    { 1, "NULL-SHA",                0x03000002U, 0x00A50021U, 0x00000000U, 0x0037FFFFU },
    { 1, "EXP-ADH-RC4-MD5",         0x03000017U, 0x00921110U, 0x00000205U, 0x0037FFFFU },
    { 1, "ADH-RC4-MD5",             0x03000018U, 0x00A21110U, 0x00000000U, 0x0037FFFFU },
    { 1, "EXP-ADH-DES-CBC-SHA",     0x03000019U, 0x00940510U, 0x00000205U, 0x0037FFFFU },
    { 1, "ADH-DES-CBC-SHA",         0x0300001AU, 0x00A40510U, 0x00000000U, 0x0037FFFFU },
    { 1, "ADH-DES-CBC3-SHA",        0x0300001BU, 0x00A40910U, 0x00000000U, 0x0037FFFFU },
    { 1, "EXP-RC4-MD5",             0x03000003U, 0x00921021U, 0x00000205U, 0x0037FFFFU },
    { 1, "RC4-MD5",                 0x03000004U, 0x02A21021U, 0x00000000U, 0x0037FFFFU },
    { 1, "RC4-SHA",                 0x03000005U, 0x02A41021U, 0x00000000U, 0x0037FFFFU },
    { 1, "EXP-RC2-CBC-MD5",         0x03000006U, 0x00922021U, 0x00000205U, 0x0037FFFFU },
    { 1, "IDEA-CBC-SHA",            0x03000007U, 0x02A44021U, 0x00000000U, 0x0037FFFFU },
    { 1, "EXP-DES-CBC-SHA",         0x03000008U, 0x00940421U, 0x00000205U, 0x0037FFFFU },
    { 1, "DES-CBC-SHA",             0x03000009U, 0x01A40421U, 0x00000000U, 0x0037FFFFU },
    { 1, "DES-CBC3-SHA",            0x0300000AU, 0x04A40821U, 0x00000000U, 0x0037FFFFU },
    { 0, "EXP-DH-DSS-DES-CBC-SHA",  0x0300000BU, 0x00940604U, 0x00000205U, 0x0037FFFFU },
    { 0, "DH-DSS-DES-CBC-SHA",      0x0300000CU, 0x01A40604U, 0x00000000U, 0x0037FFFFU },
    { 0, "DH-DSS-DES-CBC3-SHA",     0x0300000DU, 0x04A40A04U, 0x00000000U, 0x0037FFFFU },
    { 0, "EXP-DH-RSA-DES-CBC-SHA",  0x0300000EU, 0x00940602U, 0x00000205U, 0x0037FFFFU },
    { 0, "DH-RSA-DES-CBC-SHA",      0x0300000FU, 0x01A40602U, 0x00000000U, 0x0037FFFFU },
    { 0, "DH-RSA-DES-CBC3-SHA",     0x03000010U, 0x04A40A02U, 0x00000000U, 0x0037FFFFU },
    { 1, "EXP-EDH-DSS-DES-CBC-SHA", 0x03000011U, 0x00940450U, 0x00000205U, 0x0037FFFFU },
    { 1, "EDH-DSS-DES-CBC-SHA",     0x03000012U, 0x01A40450U, 0x00000000U, 0x0037FFFFU },
    { 1, "EDH-DSS-DES-CBC3-SHA",    0x03000013U, 0x04A40850U, 0x00000000U, 0x0037FFFFU },
    { 1, "EXP-EDH-RSA-DES-CBC-SHA", 0x03000014U, 0x00940430U, 0x00000205U, 0x0037FFFFU },
    { 1, "EDH-RSA-DES-CBC-SHA",     0x03000015U, 0x01A40430U, 0x00000000U, 0x0037FFFFU },
    { 1, "EDH-RSA-DES-CBC3-SHA",    0x03000016U, 0x04A40830U, 0x00000000U, 0x0037FFFFU },
    { 0, "FZA-NULL-SHA",            0x0300001CU, 0x00A50088U, 0x00000000U, 0x0037FFFFU },
    { 0, "FZA-FZA-CBC-SHA",         0x0300001DU, 0x00A48088U, 0x00000000U, 0x0037FFFFU },
    { 0, "FZA-RC4-SHA",             0x0300001EU, 0x00A41088U, 0x00000000U, 0x0037FFFFU },
    { 1, "EXP-RC4-56-SHA",          0x03000064U, 0x00941021U, 0x00000407U, 0x0037FFFFU },
    { 1, "EXP-DES-56-SHA",          0x03000062U, 0x00940421U, 0x00000408U, 0x0037FFFFU },
    { 1, "EDH-DSS-RC4-SHA",         0x03000066U, 0x00841050U, 0x00000000U, 0x0037FFFFU },
    { 1, "EXP-EDH-DSS-RC4-56-SHA",  0x03000065U, 0x00941050U, 0x00000407U, 0x0037FFFFU },
    { 1, "EXP-EDH-DSS-DES-56-SHA",  0x03000063U, 0x00940450U, 0x00000408U, 0x0037FFFFU }
};

/* Forward declarations for SSLv3 and internal helpers */
static void LIBSSL_NOINLINE sslEvpPkeyFreeInner(SslEvpPkey *pkey);
static void LIBSSL_NOINLINE ssl3GenerateKeyBlock(Ssl *ssl, u8 *keyBlock, int numBytes);
static int LIBSSL_NOINLINE ssl3ChangeCipherState(Ssl *ssl, int which);
static int LIBSSL_NOINLINE ssl3SetupKeyBlock(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3Enc(Ssl *ssl, int sendFlag);
static int LIBSSL_NOINLINE ssl3CertVerifyMac(Ssl *ssl, SslEvpMdCtx *mdCtx, u8 *outBuf);
static int LIBSSL_NOINLINE ssl3FinalFinishMac(Ssl *ssl, SslEvpMdCtx *md5Ctx, SslEvpMdCtx *sha1Ctx,
                                              const char *label, int labelLen, u8 *outBuf);
static int LIBSSL_NOINLINE ssl3HandshakeMac(Ssl *ssl, SslEvpMdCtx *mdCtx, const char *label,
                                            int labelLen, u8 *outBuf);
static int LIBSSL_NOINLINE ssl3Mac(Ssl *ssl, u8 *outMd, int sendFlag);
static int LIBSSL_NOINLINE ssl3GenerateMasterSecret(Ssl *ssl, u8 *outMaster,
                                                    const u8 *preMaster, int preMasterLen);
static int LIBSSL_NOINLINE ssl3AlertCode(int code);
static int LIBSSL_NOINLINE ssl3DefaultTimeout(void);
static int LIBSSL_NOINLINE ssl3ReadReturn(Ssl *ssl, int ret);
static int LIBSSL_NOINLINE sslRegisterStandardAlgs(void);
static SslStack *LIBSSL_NOINLINE sslGetClientCaList(const Ssl *ssl);
static u32 LIBSSL_NOINLINE sslSessionHash(const SslSession *session);
static int LIBSSL_NOINLINE sslSessionCmp(const SslSession *lhs, const SslSession *rhs);
static void LIBSSL_NOINLINE sslClearCipherCtx(Ssl *ssl);
static int LIBSSL_NOINLINE sslSetPrivateKey(SslCert *cert, SslEvpPkey *pkey, int rsaBlinding);
static int LIBSSL_NOINLINE sslSetCert(SslCert *cert, void *certObj);

/* SSLv3 SSL_ENC_METHOD (0x030DD0) */
static const SslEncMethod g_ssl3EncData = {
    ssl3Enc,
    ssl3Mac,
    ssl3SetupKeyBlock,
    ssl3GenerateMasterSecret,
    ssl3ChangeCipherState,
    ssl3FinalFinishMac,
    36,
    ssl3CertVerifyMac,
    "CLNT",
    4,
    "SRVR",
    4,
    ssl3AlertCode
};

/* SSLv3 base SSL_METHOD (0x030E24) */
static const SslMethod g_ssl3BaseMethod = {
    0x0300,
    ssl3New,
    ssl3Clear,
    ssl3Free,
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
    ssl3DefaultTimeout,
    &g_ssl3EncData
};

/* ==================== EVP_PKEY helpers (0x0238A8..0x023C4B) ==================== */

/* 0x0238A8 */
int LIBSSL_NOINLINE sslEvpPkeySize(const SslEvpPkey *pkey)
{
    if (pkey != NULL && pkey->type == 6) {
        return sslRsaSize((const SslRsa *)pkey->pkey);
    }
    return 0;
}

/* 0x0238E4 */
int LIBSSL_NOINLINE sslEvpPkeyCopyParameters(SslEvpPkey *dst, const SslEvpPkey *src)
{
    int errReason = 101;

    if (dst->type == src->type) {
        if (sslEvpPkeyMissingParams(src) == 0) {
            return 1;
        }
        errReason = 104;
    }
    sceSsl_lib_9CDCA0D7(6, 104, errReason, NULL, 0);
    return 0;
}

/* 0x023954 */
int LIBSSL_NOINLINE sslEvpPkeyMissingParams(const SslEvpPkey *pkey)
{
    (void)pkey;
    return 0;
}

/* 0x02395C */
SslEvpPkey *LIBSSL_NOINLINE sslEvpPkeyNew(void)
{
    SslEvpPkey *pkey = (SslEvpPkey *)sceSsl_lib_0861D934(sizeof(SslEvpPkey));

    if (pkey == NULL) {
        sceSsl_lib_9CDCA0D7(6, 107, 33, NULL, 0);
        return NULL;
    }
    pkey->type = 0;
    pkey->references = 1;
    pkey->pkey = NULL;
    pkey->saveParameters = 1;
    pkey->attributes = NULL;
    return pkey;
}

/* 0x0239C0 */
int LIBSSL_NOINLINE sslEvpPkeyUpRef(SslEvpPkey *pkey)
{
    return sslCryptoAddLock(&pkey->references, 1, 10, "ssl/sslp_lib.c", 206);
}

/* 0x0239F0 */
int LIBSSL_NOINLINE sslEvpPkeyAssign(SslEvpPkey *pkey, int keyType, void *keyPtr)
{
    if (pkey == NULL) {
        return 0;
    }
    if (pkey->pkey != NULL) {
        sslEvpPkeyFreeInner(pkey);
    }
    pkey->type = sslEvpPkeyNormalizeType(keyType);
    pkey->saveType = keyType;
    pkey->pkey = keyPtr;
    return 1;
}

/* 0x023A60 */
int LIBSSL_NOINLINE sslEvpPkeyGetType(const SslEvpPkey *pkey)
{
    return sslEvpPkeyNormalizeType(pkey->type);
}

/* 0x023A7C */
int LIBSSL_NOINLINE sslEvpPkeyNormalizeType(int keyType)
{
    switch (keyType) {
    case 6:
    case 19:
        return 6;
    case 28:
        return 28;
    case 66:
    case 67:
    case 70:
    case 113:
    case 116:
        return 116;
    default:
        return 0;
    }
}

/* 0x023AFC */
void LIBSSL_NOINLINE sslEvpPkeyFree(SslEvpPkey *pkey)
{
    if (pkey == NULL) {
        return;
    }
    if (sslCryptoAddLock(&pkey->references, -1, 10, "ssl/sslp_lib.c", 257) > 0) {
        return;
    }
    sslEvpPkeyFreeInner(pkey);
    sceSsl_lib_5E5C873A(pkey);
}

/* 0x023B58 */
static void LIBSSL_NOINLINE sslEvpPkeyFreeInner(SslEvpPkey *pkey)
{
    if (pkey->type == 6 || pkey->type == 19) {
        sslRsaFree((SslRsa *)pkey->pkey);
    }
}

/* 0x023B8C */
int LIBSSL_NOINLINE sslCertGetMask(int sigType, const SslEvpPkey *pubKey)
{
    int mask = 0;
    int keyType;

    if (pubKey == NULL) {
        return 0;
    }
    keyType = sslEvpPkeyGetType(pubKey);
    if (keyType == 6) {
        mask = 49;
    } else if (keyType == 28) {
        mask = 68;
    } else if (keyType == 116) {
        mask = 18;
    }
    if (sigType == 6) {
        mask |= 0x100;
    } else if (sigType == 512) {
        mask |= 0x200;
    }
    if (sslEvpPkeySize(pubKey) < 513) {
        mask |= 0x1000;
    }
    return mask;
}

/* ==================== s3_both.c (0x023C4C..0x0246DB) ==================== */

/* 0x023C4C */
int LIBSSL_NOINLINE ssl3SendFinished(Ssl *ssl, int stateA, int stateB,
                                     const char *label, int labelLen)
{
    if (ssl->state == stateA) {
        u8 *bufBase = (u8 *)ssl->initBuf->data;
        int finLen = ssl->method->ssl3Enc->finalFinishMac(ssl, ssl->ssl3State->finishDg1,
                                                          ssl->ssl3State->finishDg2,
                                                          label, labelLen, bufBase + 4);
        if (finLen == 0) {
            return -1;
        }
        bufBase[0] = 20;
        bufBase[1] = (u8)(finLen >> 16);
        bufBase[2] = (u8)(finLen >> 8);
        bufBase[3] = (u8)(finLen & 0xFF);
        ssl->initNum = finLen + 4;
        ssl->state = stateB;
        ssl->initOff = 0;
    }
    return ssl3DoWrite(ssl, 22);
}

/* 0x023CF8 */
int LIBSSL_NOINLINE ssl3GetFinished(Ssl *ssl, int stateA, int stateB)
{
    int ok = 0;
    int msgLen = ssl3GetMessage(ssl, stateA, stateB, 20, 64U, &ok);
    int expectedLen;
    int errReason;
    int alertCode;

    if (ok == 0) {
        return msgLen;
    }
    if (ssl->ssl3State->changeCipherSpec == 0) {
        errReason = 154;
        alertCode = 10;
    } else {
        ssl->ssl3State->changeCipherSpec = 0;
        expectedLen = ssl->method->ssl3Enc->finishMacLength;
        if (msgLen != expectedLen) {
            errReason = 111;
            alertCode = 50;
        } else if (sslMemcmp(ssl->initBuf->data, ssl->ssl3State->tmpPeerFinishMd, expectedLen) != 0) {
            errReason = 149;
            alertCode = 51;
        } else {
            return 1;
        }
    }

    sceSsl_lib_9CDCA0D7(20, 140, errReason, NULL, 0);
    ssl3SendAlert(ssl, 2, alertCode);
    return -1;
}

/* 0x023DF0 */
int LIBSSL_NOINLINE ssl3SendChangeCipherSpec(Ssl *ssl, int stateA, int stateB)
{
    if (ssl->state == stateA) {
        u8 *bufBase = (u8 *)ssl->initBuf->data;
        bufBase[0] = 1;
        ssl->state = stateB;
        ssl->initNum = 1;
        ssl->initOff = 0;
    }
    return ssl3DoWrite(ssl, 20);
}

/* 0x023E3C */
int LIBSSL_NOINLINE ssl3OutputCertChain(Ssl *ssl, void *leafCert)
{
    SslBufMem *buf = ssl->initBuf;
    int pos = 7;
    void *storeCtx = NULL;
    u8 *outPtr;
    int totalLen;
    int idx;

    if (sslBufMemGrow(buf, 2048) == 0) {
        sceSsl_lib_9CDCA0D7(20, 148, 2055, NULL, 0);
        return 0;
    }

    if (leafCert != NULL) {
        void *curCert = leafCert;

        if (ssl->ctx->certStore != NULL) {
            storeCtx = sslX509StoreCtxNew();
            if (storeCtx == NULL) {
                return 0;
            }
            sslX509StoreCtxInit(storeCtx, ssl->ctx->certStore, NULL, NULL);
        }

        for (;;) {
            int derLen = sceSsl_lib_A739BFD9(curCert, NULL);
            void *subjName;
            void *issuerName;
            void *nextCert = NULL;

            if (sslBufMemGrow(buf, pos + derLen + 3) == 0) {
                sceSsl_lib_9CDCA0D7(20, 148, 2055, NULL, 0);
                return 0;
            }
            outPtr = (u8 *)buf->data + pos;
            pos += derLen + 3;
            outPtr[0] = (u8)(derLen >> 16);
            outPtr[1] = (u8)(derLen >> 8);
            outPtr[2] = (u8)(derLen & 0xFF);
            outPtr += 3;
            sceSsl_lib_A739BFD9(curCert, &outPtr);

            subjName = sceSsl_lib_12C7718F(curCert);
            issuerName = sceSsl_lib_4BE9030A(curCert);
            if (sceSsl_lib_7D0E41F4(subjName, issuerName) == 0) {
                break;
            }
            if (ssl->ctx->certStore == NULL || ssl->extraCerts != NULL) {
                break;
            }
            issuerName = sceSsl_lib_4BE9030A(curCert);
            if (sslX509StoreGetIssuer(storeCtx, &nextCert, issuerName) <= 0) {
                break;
            }
            sceSsl_lib_C1A2E14B(nextCert);
            curCert = nextCert;
        }

        if (ssl->ctx->certStore != NULL) {
            sslX509StoreCtxCleanup(storeCtx);
            sslX509StoreCtxFree(storeCtx);
        }
    }

    if (ssl->extraCerts != NULL) {
        for (idx = 0; idx < ssl->extraCerts->num; idx++) {
            void *extra = ssl->extraCerts->data[idx];
            int derLen = sceSsl_lib_A739BFD9(extra, NULL);
            if (sslBufMemGrow(buf, pos + derLen + 3) == 0) {
                sceSsl_lib_9CDCA0D7(20, 148, 2055, NULL, 0);
                return 0;
            }
            outPtr = (u8 *)buf->data + pos;
            pos += derLen + 3;
            outPtr[0] = (u8)(derLen >> 16);
            outPtr[1] = (u8)(derLen >> 8);
            outPtr[2] = (u8)(derLen & 0xFF);
            outPtr += 3;
            sceSsl_lib_A739BFD9(extra, &outPtr);
        }
    }

    if (ssl->ctx->extraCerts != NULL) {
        for (idx = 0; idx < ssl->ctx->extraCerts->num; idx++) {
            void *extra = ssl->ctx->extraCerts->data[idx];
            int derLen = sceSsl_lib_A739BFD9(extra, NULL);
            if (sslBufMemGrow(buf, pos + derLen + 3) == 0) {
                sceSsl_lib_9CDCA0D7(20, 148, 2055, NULL, 0);
                return 0;
            }
            outPtr = (u8 *)buf->data + pos;
            pos += derLen + 3;
            outPtr[0] = (u8)(derLen >> 16);
            outPtr[1] = (u8)(derLen >> 8);
            outPtr[2] = (u8)(derLen & 0xFF);
            outPtr += 3;
            sceSsl_lib_A739BFD9(extra, &outPtr);
        }
    }

    totalLen = pos - 7;
    outPtr = (u8 *)buf->data + 4;
    outPtr[0] = (u8)(totalLen >> 16);
    outPtr[1] = (u8)(totalLen >> 8);
    outPtr[2] = (u8)(totalLen & 0xFF);
    totalLen += 3;
    outPtr = (u8 *)buf->data;
    outPtr[0] = 11;
    outPtr[1] = (u8)(totalLen >> 16);
    outPtr[2] = (u8)(totalLen >> 8);
    outPtr[3] = (u8)(totalLen & 0xFF);
    return totalLen + 4;
}

/* 0x0241F0 */
int LIBSSL_NOINLINE ssl3GetMessage(Ssl *ssl, int stateA, int stateB,
                                   int expectedType, u32 maxLen, int *okOut)
{
    Ssl3State *ssl3State = ssl->ssl3State;
    u8 *bufBase;
    int msgLen;
    int remaining;

    if (ssl3State->tmpReuseMessage != 0) {
        ssl3State->tmpReuseMessage = 0;
        if (expectedType >= 0 && ssl3State->tmpMessageType != expectedType) {
            sceSsl_lib_9CDCA0D7(20, 142, 244, NULL, 0);
            ssl3SendAlert(ssl, 2, 10);
            *okOut = 0;
            return -1;
        }
        *okOut = 1;
        return (int)ssl3State->tmpMessageSize;
    }

    bufBase = (u8 *)ssl->initBuf->data;
    if (ssl->state == stateA) {
        for (;;) {
            int got = ssl3ReadBytes(ssl, 22, bufBase + ssl->initNum, 4 - ssl->initNum);
            if (got >= 4 - ssl->initNum) {
                break;
            }
            if (got <= 0) {
                *okOut = 0;
                return ssl3ReadReturn(ssl, got);
            }
            ssl->initNum += got;
        }

        if (expectedType >= 0 && bufBase[0] != (u8)expectedType) {
            sceSsl_lib_9CDCA0D7(20, 142, 244, NULL, 0);
            ssl3SendAlert(ssl, 2, 10);
            *okOut = 0;
            return -1;
        }

        ssl3State->tmpMessageType = bufBase[0];
        msgLen = (int)(((u32)bufBase[1] << 16) | ((u32)bufBase[2] << 8) | bufBase[3]);
        if ((u32)msgLen > maxLen) {
            sceSsl_lib_9CDCA0D7(20, 142, 152, NULL, 0);
            ssl3SendAlert(ssl, 2, 47);
            *okOut = 0;
            return -1;
        }
        if (msgLen != 0 && sslBufMemGrow(ssl->initBuf, msgLen) == 0) {
            sceSsl_lib_9CDCA0D7(20, 142, 2055, NULL, 0);
            *okOut = 0;
            return -1;
        }
        ssl->state = stateB;
        ssl3State->tmpMessageSize = (u32)msgLen;
        ssl->initNum = 0;
    }

    msgLen = (int)ssl3State->tmpMessageSize;
    bufBase = (u8 *)ssl->initBuf->data;
    remaining = msgLen;
    while (remaining > 0) {
        int got = ssl3ReadBytes(ssl, 22, bufBase + ssl->initNum, remaining);
        if (got == remaining) {
            break;
        }
        if (got <= 0) {
            *okOut = 0;
            return ssl3ReadReturn(ssl, got);
        }
        remaining -= got;
        ssl->initNum += got;
    }

    *okOut = 1;
    return msgLen;
}

/* 0x024428 */
int LIBSSL_NOINLINE sslCertType(void *cert, SslEvpPkey *pubKey)
{
    int keyType;

    if (pubKey == NULL) {
        pubKey = sceSsl_lib_B87FFE37(cert);
    }
    if (pubKey == NULL) {
        return -1;
    }
    keyType = pubKey->type;
    if (keyType == 6) {
        return 0;
    }
    if (keyType == 116) {
        return 2;
    }
    if (keyType == 28) {
        int sigType;
        if (cert == NULL) {
            return 4;
        }
        sigType = sslX509GetSignatureType(cert);
        if (sigType == 6) {
            return 3;
        }
        if (sigType == 116) {
            return 4;
        }
    }
    return -1;
}

/* 0x0244C8 */
int LIBSSL_NOINLINE sslVerifyAlarmType(int verifyErr)
{
    switch (verifyErr) {
    case 2:
    case 3:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
        return 48;
    case 4:
    case 5:
    case 6:
    case 9:
    case 11:
    case 13:
    case 14:
    case 15:
    case 16:
        return 42;
    case 7:
    case 8:
        return 51;
    case 10:
    case 12:
        return 45;
    case 17:
        return 80;
    case 23:
        return 44;
    case 50:
        return 40;
    default:
        return 46;
    }
}

/* 0x024530 */
int LIBSSL_NOINLINE ssl3SetupBuffers(Ssl *ssl)
{
    if (ssl->ssl3State->rbufBuf == NULL) {
        if (ssl3ResizeReadBuffer(ssl, ssl->readBufferSize) == 0) {
            sceSsl_lib_9CDCA0D7(20, 157, 33, NULL, 0);
            return 0;
        }
    }
    if (ssl->ssl3State->wbufBuf == NULL) {
        u32 allocSize;
        u8 *wbuf;

        if (ssl->maxSendFragment == 0 || ssl->maxSendFragment > 16384U) {
            ssl->maxSendFragment = 16384U;
        }
        allocSize = ssl->maxSendFragment + 57U;
        wbuf = (u8 *)sceSsl_lib_0861D934(allocSize);
        if (wbuf == NULL) {
            sceSsl_lib_9CDCA0D7(20, 157, 33, NULL, 0);
            return 0;
        }
        ssl->ssl3State->wbufLen = (int)allocSize;
        ssl->ssl3State->wbufBuf = wbuf;
    }
    ssl->packet = ssl->ssl3State->rbufBuf;
    return 1;
}

/* 0x0245F8 */
int LIBSSL_NOINLINE ssl3ResizeReadBuffer(Ssl *ssl, int targetSize)
{
    int reqSize = (targetSize <= 0) ? 18437 : targetSize;
    u8 *oldBuf = ssl->ssl3State->rbufBuf;
    u8 *newBuf;
    int packetOff = 0;
    int hadPacket = 0;

    if (ssl->readBufferSize >= reqSize && oldBuf != NULL) {
        return 1;
    }
    if (ssl->packet != NULL && oldBuf != NULL) {
        packetOff = (int)(ssl->packet - oldBuf);
        hadPacket = 1;
    }
    if (oldBuf != NULL) {
        newBuf = (u8 *)sslCryptoRealloc(oldBuf, (u32)reqSize);
    } else {
        newBuf = (u8 *)sceSsl_lib_0861D934((u32)reqSize);
    }
    if (newBuf == NULL) {
        return 0;
    }
    ssl->readBufferSize = reqSize;
    ssl->ssl3State->rbufBuf = newBuf;
    ssl->ssl3State->rbufLen = reqSize;
    if (hadPacket != 0) {
        ssl->packet = newBuf + packetOff;
    }
    return 1;
}

/* ==================== s3_enc.c (0x0246DC..0x025487) ==================== */

/* 0x0246DC */
static void LIBSSL_NOINLINE ssl3GenerateKeyBlock(Ssl *ssl, u8 *keyBlock, int numBytes)
{
    SceSslMd5Ctx md5Ctx;
    SceSslSha1Ctx sha1Ctx;
    u8 saltBuf[16];
    u8 digestBuf[32];
    u8 letter = 'A';
    int generated = 0;
    int saltLen = 0;

    while (generated < numBytes) {
        int idx;
        saltLen++;
        for (idx = 0; idx < saltLen; idx++) {
            saltBuf[idx] = letter;
        }
        letter++;

        sceKernelUtilsSha1BlockInit(&sha1Ctx);
        sceKernelUtilsSha1BlockUpdate(&sha1Ctx, saltBuf, (u32)saltLen);
        sceKernelUtilsSha1BlockUpdate(&sha1Ctx, ssl->session->masterKey,
                                      (u32)ssl->session->masterKeyLength);
        sceKernelUtilsSha1BlockUpdate(&sha1Ctx, ssl->ssl3State->serverRandom, 32U);
        sceKernelUtilsSha1BlockUpdate(&sha1Ctx, ssl->ssl3State->clientRandom, 32U);
        sslEvpSha1Final(digestBuf, &sha1Ctx);

        sceKernelUtilsMd5BlockInit(&md5Ctx);
        sceKernelUtilsMd5BlockUpdate(&md5Ctx, ssl->session->masterKey,
                                     (u32)ssl->session->masterKeyLength);
        sceKernelUtilsMd5BlockUpdate(&md5Ctx, digestBuf, 20U);

        if (generated + 16 > numBytes) {
            sslEvpMd5Final(digestBuf, &md5Ctx);
            sslMemcpy(keyBlock, digestBuf, (u32)(numBytes - generated));
        } else {
            sslEvpMd5Final(keyBlock, &md5Ctx);
        }
        generated += 16;
        keyBlock += 16;
    }

    sslMemset(digestBuf, 0, 20U);
}

/* 0x024864 */
static int LIBSSL_NOINLINE ssl3ChangeCipherState(Ssl *ssl, int which)
{
    u8 exportKeyBuf[32];
    u8 exportIvBuf[32];
    SceSslMd5Ctx md5Ctx;
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
    u8 *firstRand;
    u8 *secondRand;
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
            sceSsl_lib_9CDCA0D7(20, 129, 33, NULL, 0);
            return 0;
        }
    }
    if (*macCtxSlot == NULL) {
        *macCtxSlot = sslEvpMdCtxNew();
        if (*macCtxSlot == NULL) {
            sceSsl_lib_9CDCA0D7(20, 129, 33, NULL, 0);
            return 0;
        }
    }
    ssl->macFreeFunc = (SslFreeCb)sslEvpMdCtxFree;
    if (sslEvpDigestInit((SslEvpMdCtx *)*macCtxSlot, evpMd) == 0) {
        sceSsl_lib_9CDCA0D7(20, 129, 33, NULL, 0);
        return 0;
    }

    *macSizeSlot = (u32)sslEvpMdSize(evpMd);
    cipherCtx = *cipherCtxSlot;
    if (sslEvpCipherCtxInit(cipherCtx) == 0) {
        sceSsl_lib_9CDCA0D7(20, 129, 33, NULL, 0);
        return 0;
    }

    keyBlock = ssl3State->tmpKeyBlock;
    macLen = sslEvpMdSize(evpMd);
    if (isExport != 0) {
        keyLen = (int)(ssl3State->tmpNewCipher->algStrength & 0xFFU);
    } else {
        keyLen = sslEvpCipherKeyLength(evpCipher);
    }
    ivLen = sslEvpCipherIvLength(evpCipher);

    if (which == 0x12 || which == 0x21) {
        macSrc = keyBlock;
        keySrc = keyBlock + 2 * macLen;
        ivSrc = keyBlock + 2 * macLen + 2 * keyLen;
        neededLen = 2 * macLen + 2 * keyLen + 2 * ivLen;
        firstRand = ssl3State->clientRandom;
        secondRand = ssl3State->serverRandom;
    } else {
        macSrc = keyBlock + macLen;
        keySrc = keyBlock + 2 * macLen + keyLen;
        ivSrc = keyBlock + 2 * macLen + 2 * keyLen + ivLen;
        neededLen = 2 * macLen + 2 * keyLen + 2 * ivLen;
        firstRand = ssl3State->serverRandom;
        secondRand = ssl3State->clientRandom;
    }

    if (ssl3State->tmpKeyBlockLen < neededLen) {
        sceSsl_lib_9CDCA0D7(20, 129, 157, NULL, 0);
        return 0;
    }

    sslMemcpy(macSecretDst, macSrc, (u32)macLen);

    if (isExport != 0) {
        sceKernelUtilsMd5BlockInit(&md5Ctx);
        sceKernelUtilsMd5BlockUpdate(&md5Ctx, (u8 *)keySrc, (u32)keyLen);
        sceKernelUtilsMd5BlockUpdate(&md5Ctx, firstRand, 32U);
        sceKernelUtilsMd5BlockUpdate(&md5Ctx, secondRand, 32U);
        sslEvpMd5Final(exportKeyBuf, &md5Ctx);
        keySrc = exportKeyBuf;

        if (ivLen > 0) {
            sceKernelUtilsMd5BlockInit(&md5Ctx);
            sceKernelUtilsMd5BlockUpdate(&md5Ctx, firstRand, 32U);
            sceKernelUtilsMd5BlockUpdate(&md5Ctx, secondRand, 32U);
            sslEvpMd5Final(exportIvBuf, &md5Ctx);
            ivSrc = exportIvBuf;
        }
    }

    ssl->session->keyArgLength = 0;
    if (sslEvpCipherInit(cipherCtx, evpCipher, keySrc, ivSrc, which & 2) == 0) {
        return 0;
    }
    sslMemset(exportKeyBuf, 0, sizeof(exportKeyBuf));
    sslMemset(exportIvBuf, 0, sizeof(exportIvBuf));
    return 1;
}

/* 0x024B88 */
static int LIBSSL_NOINLINE ssl3SetupKeyBlock(Ssl *ssl)
{
    const SslEvpCipher *evpCipher;
    const SslEvpMd *evpMd;
    int blockLen;
    u8 *keyBuf;

    if (ssl->ssl3State->tmpKeyBlockLen != 0) {
        return 1;
    }
    if (sslCipherGetEvp(ssl->session->cipher, &evpCipher, &evpMd) == 0) {
        sceSsl_lib_9CDCA0D7(20, 158, 138, NULL, 0);
        return 0;
    }
    ssl->ssl3State->tmpNewSymEnc = evpCipher;
    ssl->ssl3State->tmpNewHash = evpMd;

    blockLen = 2 * (sslEvpCipherKeyLength(evpCipher) + sslEvpMdSize(evpMd) +
                    sslEvpCipherIvLength(evpCipher));
    ssl3CleanupKeyBlock(ssl);

    keyBuf = (u8 *)sceSsl_lib_0861D934((u32)blockLen);
    if (keyBuf == NULL) {
        sceSsl_lib_9CDCA0D7(20, 158, 33, NULL, 0);
        return 0;
    }
    ssl->ssl3State->tmpKeyBlock = keyBuf;
    ssl->ssl3State->tmpKeyBlockLen = blockLen;
    ssl3GenerateKeyBlock(ssl, keyBuf, blockLen);
    return 1;
}

/* 0x024C88 */
void LIBSSL_NOINLINE ssl3CleanupKeyBlock(Ssl *ssl)
{
    if (ssl->ssl3State->tmpKeyBlock != NULL) {
        sslMemset(ssl->ssl3State->tmpKeyBlock, 0, (u32)ssl->ssl3State->tmpKeyBlockLen);
        sceSsl_lib_5E5C873A(ssl->ssl3State->tmpKeyBlock);
        ssl->ssl3State->tmpKeyBlock = NULL;
    }
    ssl->ssl3State->tmpKeyBlockLen = 0;
}

/* 0x024CE0 */
static int LIBSSL_NOINLINE ssl3Enc(Ssl *ssl, int sendFlag)
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
        sslMemmove(rec->data, rec->input, (int)rec->length);
        rec->input = rec->data;
        return 1;
    }

    recLen = (int)rec->length;
    blockSize = sslEvpCipherCtxBlockSize(cipherCtx);
    if (blockSize != 1 && sendFlag != 0) {
        int padLen = blockSize - (recLen % blockSize);
        recLen += padLen;
        rec->length += (u32)padLen;
        rec->input[recLen - 1] = (u8)(padLen - 1);
    }

    sslEvpCipher(cipherCtx, rec->data, rec->input, (u32)recLen);

    if (blockSize != 1 && sendFlag == 0) {
        int trimLen = rec->data[recLen - 1] + 1;
        if (trimLen > blockSize) {
            return 0;
        }
        rec->length -= (u32)trimLen;
    }
    return 1;
}

/* 0x024E44 */
int LIBSSL_NOINLINE ssl3InitFinishedMac(Ssl *ssl)
{
    if (sslEvpDigestInit(ssl->ssl3State->finishDg1, ssl->ctx->md5) == 0) {
        return 0;
    }
    return (sslEvpDigestInit(ssl->ssl3State->finishDg2, ssl->ctx->sha1) != 0) ? 1 : 0;
}

/* 0x024E9C */
void LIBSSL_NOINLINE ssl3FinishMac(Ssl *ssl, const u8 *buf, int len)
{
    sslEvpDigestUpdate(ssl->ssl3State->finishDg1, buf, (u32)len);
    sslEvpDigestUpdate(ssl->ssl3State->finishDg2, buf, (u32)len);
}

/* 0x024EF4 */
static int LIBSSL_NOINLINE ssl3CertVerifyMac(Ssl *ssl, SslEvpMdCtx *mdCtx, u8 *outBuf)
{
    return ssl3HandshakeMac(ssl, mdCtx, NULL, 0, outBuf);
}

/* 0x024F18 */
static int LIBSSL_NOINLINE ssl3FinalFinishMac(Ssl *ssl, SslEvpMdCtx *md5Ctx, SslEvpMdCtx *sha1Ctx,
                                              const char *label, int labelLen, u8 *outBuf)
{
    int md5Len = ssl3HandshakeMac(ssl, md5Ctx, label, labelLen, outBuf);
    int shaLen;

    if (md5Len == 0) {
        return 0;
    }
    shaLen = ssl3HandshakeMac(ssl, sha1Ctx, label, labelLen, outBuf + md5Len);
    if (shaLen == 0) {
        return 0;
    }
    return md5Len + shaLen;
}

/* 0x024FB8 */
static int LIBSSL_NOINLINE ssl3HandshakeMac(Ssl *ssl, SslEvpMdCtx *mdCtx, const char *label,
                                            int labelLen, u8 *outBuf)
{
    u8 innerDigest[32];
    u32 innerLen = 0;
    u32 finalLen = 0;
    int mdSize;
    int padLen;
    const SslEvpMd *md;

    if (sslEvpMdCtxCopy(ssl->mdCtx, mdCtx) == 0) {
        return 0;
    }
    mdSize = sslEvpMdCtxSize(ssl->mdCtx);
    padLen = (48 / mdSize) * mdSize;
    if (label != NULL) {
        sslEvpDigestUpdate(ssl->mdCtx, label, (u32)labelLen);
    }
    sslEvpDigestUpdate(ssl->mdCtx, ssl->session->masterKey, (u32)ssl->session->masterKeyLength);
    sslEvpDigestUpdate(ssl->mdCtx, g_ssl3PadOne, (u32)padLen);
    sslEvpDigestFinal(ssl->mdCtx, innerDigest, &innerLen);

    md = (const SslEvpMd *)(u32)sslEvpCipherNid((const SslEvpCipher *)ssl->mdCtx);
    sslEvpDigestInit(ssl->mdCtx, md);
    sslEvpDigestUpdate(ssl->mdCtx, ssl->session->masterKey, (u32)ssl->session->masterKeyLength);
    sslEvpDigestUpdate(ssl->mdCtx, g_ssl3PadTwo, (u32)padLen);
    sslEvpDigestUpdate(ssl->mdCtx, innerDigest, innerLen);
    sslEvpDigestFinal(ssl->mdCtx, outBuf, &finalLen);
    return (int)finalLen;
}

/* 0x0250E4 */
static int LIBSSL_NOINLINE ssl3Mac(Ssl *ssl, u8 *outMd, int sendFlag)
{
    SslEvpMdCtx *mdCtx;
    u32 mdSize;
    u8 *seqNum;
    u8 *macSecret;
    Ssl3Record *rec;
    u8 recTypeByte;
    int padLen;
    int idx;

    if (sendFlag != 0) {
        mdSize = ssl->writeMacSize;
        mdCtx = (SslEvpMdCtx *)ssl->writeHash;
        seqNum = ssl->ssl3State->writeSequence;
        macSecret = ssl->ssl3State->writeMacSecret;
        rec = &ssl->ssl3State->wrec;
    } else {
        mdSize = ssl->readMacSize;
        mdCtx = (SslEvpMdCtx *)ssl->readHash;
        seqNum = ssl->ssl3State->readSequence;
        macSecret = ssl->ssl3State->readMacSecret;
        rec = &ssl->ssl3State->rrec;
    }

    padLen = (int)((48U / mdSize) * mdSize);
    sslEvpDigestInit(mdCtx, NULL);
    sslEvpDigestUpdate(mdCtx, macSecret, mdSize);
    sslEvpDigestUpdate(mdCtx, g_ssl3PadOne, (u32)padLen);
    sslEvpDigestUpdate(mdCtx, seqNum, 8U);
    recTypeByte = (u8)rec->type;
    sslEvpDigestUpdate(mdCtx, &recTypeByte, 1U);
    outMd[0] = (u8)(rec->length >> 8);
    outMd[1] = (u8)(rec->length & 0xFF);
    sslEvpDigestUpdate(mdCtx, outMd, 2U);
    sslEvpDigestUpdate(mdCtx, rec->input, rec->length);
    sslEvpDigestFinal(mdCtx, outMd, NULL);

    sslEvpDigestInit(mdCtx, NULL);
    sslEvpDigestUpdate(mdCtx, macSecret, mdSize);
    sslEvpDigestUpdate(mdCtx, g_ssl3PadTwo, (u32)padLen);
    sslEvpDigestUpdate(mdCtx, outMd, mdSize);
    sslEvpDigestFinal(mdCtx, outMd, &mdSize);

    for (idx = 7; idx >= 0; idx--) {
        seqNum[idx]++;
        if (seqNum[idx] != 0) {
            break;
        }
    }
    return (int)mdSize;
}

/* 0x0252A0 */
static int LIBSSL_NOINLINE ssl3GenerateMasterSecret(Ssl *ssl, u8 *outMaster,
                                                    const u8 *preMaster, int preMasterLen)
{
    u8 shaDigest[32];
    u32 outLen;
    int totalLen = 0;
    int idx;

    for (idx = 0; idx < 3; idx++) {
        const char *salt;
        if (sslEvpDigestInit(ssl->mdCtx, ssl->ctx->sha1) == 0) {
            return 0;
        }
        salt = g_ssl3MasterSecretSalt[idx];
        sslEvpDigestUpdate(ssl->mdCtx, salt, (u32)sslStrlen(salt));
        sslEvpDigestUpdate(ssl->mdCtx, preMaster, (u32)preMasterLen);
        sslEvpDigestUpdate(ssl->mdCtx, ssl->ssl3State->clientRandom, 32U);
        sslEvpDigestUpdate(ssl->mdCtx, ssl->ssl3State->serverRandom, 32U);
        sslEvpDigestFinal(ssl->mdCtx, shaDigest, &outLen);

        if (sslEvpDigestInit(ssl->mdCtx, ssl->ctx->md5) == 0) {
            return 0;
        }
        sslEvpDigestUpdate(ssl->mdCtx, preMaster, (u32)preMasterLen);
        sslEvpDigestUpdate(ssl->mdCtx, shaDigest, outLen);
        sslEvpDigestFinal(ssl->mdCtx, outMaster, &outLen);
        totalLen += (int)outLen;
        outMaster += outLen;
    }
    return totalLen;
}

/* 0x0253F4 */
static int LIBSSL_NOINLINE ssl3AlertCode(int code)
{
    switch (code) {
    case 0:   return 0;
    case 10:  return 10;
    case 20:
    case 21:
    case 22:  return 20;
    case 30:  return 30;
    case 40:
    case 49:
    case 50:
    case 51:
    case 60:
    case 70:
    case 71:
    case 80:
    case 90:  return 40;
    case 41:  return 41;
    case 42:
    case 48:  return 42;
    case 43:  return 43;
    case 44:  return 44;
    case 45:  return 45;
    case 46:  return 46;
    case 47:  return 47;
    case 100: return -1;
    default:  return -1;
    }
}

/* ==================== s3_lib.c (0x025488..0x025EEF) ==================== */

/* 0x025488 */
static int LIBSSL_NOINLINE ssl3DefaultTimeout(void)
{
    return 7200;
}

/* 0x025490 */
const SslMethod *LIBSSL_NOINLINE ssl3BaseMethod(void)
{
    return &g_ssl3BaseMethod;
}

/* 0x02549C */
int LIBSSL_NOINLINE ssl3NumCiphers(void)
{
    return 35;
}

/* 0x0254A4 */
const SslCipher *LIBSSL_NOINLINE ssl3GetCipher(int idx)
{
    if ((u32)idx < 35U) {
        return &g_ssl3Ciphers[34 - idx];
    }
    return NULL;
}

/* 0x0254D8 */
int LIBSSL_NOINLINE ssl3Pending(const Ssl *ssl)
{
    if (ssl->rstate != 241) {
        return (int)ssl->ssl3State->rrec.length;
    }
    return 0;
}

/* 0x0254F8 */
int LIBSSL_NOINLINE ssl3New(Ssl *ssl)
{
    Ssl3State *ssl3State = (Ssl3State *)sceSsl_lib_0861D934(sizeof(Ssl3State));

    if (ssl3State != NULL) {
        sslMemset(ssl3State, 0, sizeof(Ssl3State));
        ssl3State->finishDg1 = sslEvpMdCtxNew();
        if (ssl3State->finishDg1 != NULL) {
            ssl3State->finishDg2 = sslEvpMdCtxNew();
            if (ssl3State->finishDg2 != NULL) {
                ssl->ssl3State = ssl3State;
                ssl->method->sslClear(ssl);
                return 1;
            }
        }
    }
    sceSsl_lib_9CDCA0D7(20, 147, 33, NULL, 0);
    return 0;
}

/* 0x0255D0 */
void LIBSSL_NOINLINE ssl3Free(Ssl *ssl)
{
    ssl3CleanupKeyBlock(ssl);
    if (ssl->ssl3State->finishDg1 != NULL) {
        sslEvpMdCtxFree(ssl->ssl3State->finishDg1);
    }
    if (ssl->ssl3State->finishDg2 != NULL) {
        sslEvpMdCtxFree(ssl->ssl3State->finishDg2);
    }
    if (ssl->ssl3State->rbufBuf != NULL) {
        sceSsl_lib_5E5C873A(ssl->ssl3State->rbufBuf);
    }
    if (ssl->ssl3State->wbufBuf != NULL) {
        sceSsl_lib_5E5C873A(ssl->ssl3State->wbufBuf);
    }
    if (ssl->ssl3State->rrec.comp != NULL) {
        sceSsl_lib_5E5C873A(ssl->ssl3State->rrec.comp);
    }
    if (ssl->ssl3State->tmpCaNames != NULL) {
        sslSkPopFree(ssl->ssl3State->tmpCaNames, sceSsl_lib_ED95A457);
    }
    sslMemset(ssl->ssl3State, 0, sizeof(Ssl3State));
    sceSsl_lib_5E5C873A(ssl->ssl3State);
    ssl->ssl3State = NULL;
}

/* 0x0256A4 */
void LIBSSL_NOINLINE ssl3Clear(Ssl *ssl)
{
    Ssl3State *ssl3State = ssl->ssl3State;
    u8 *savedRbuf;
    int savedRbufLen;
    u8 *savedWbuf;
    int savedWbufLen;
    SslEvpMdCtx *savedDg1;
    SslEvpMdCtx *savedDg2;

    ssl3CleanupKeyBlock(ssl);
    if (ssl3State->tmpCaNames != NULL) {
        sslSkPopFree(ssl3State->tmpCaNames, sceSsl_lib_ED95A457);
    }

    savedRbuf = ssl3State->rbufBuf;
    savedRbufLen = ssl3State->rbufLen;
    savedWbuf = ssl3State->wbufBuf;
    savedWbufLen = ssl3State->wbufLen;
    savedDg1 = ssl3State->finishDg1;
    savedDg2 = ssl3State->finishDg2;

    if (ssl3State->rrec.comp != NULL) {
        sceSsl_lib_5E5C873A(ssl3State->rrec.comp);
        ssl3State->rrec.comp = NULL;
    }

    sslMemset(ssl3State, 0, sizeof(Ssl3State));
    ssl3State->finishDg1 = savedDg1;
    ssl3State->finishDg2 = savedDg2;
    ssl3State->wbufBuf = savedWbuf;
    ssl3State->wbufLen = savedWbufLen;
    ssl3State->rbufBuf = savedRbuf;
    ssl3State->rbufLen = savedRbufLen;
    if (savedRbuf != NULL) {
        sslMemset(savedRbuf, 0, (u32)savedRbufLen);
    }
    sslFreeWbioBuffer(ssl);
    ssl->packetLength = 0;
    ssl3State->renegotiate = 0;
    ssl3State->totalRenegotiations = 0;
    ssl3State->numRenegotiations = 0;
    ssl3State->inReadAppData = 0;
    ssl->version = 0x0300;
    ssl3State->rrec.length = 0;
    ssl3State->rrec.off = 0;
}

/* 0x0257C4 */
int LIBSSL_NOINLINE ssl3Ctrl(Ssl *ssl, int cmd, int larg, void *parg)
{
    (void)larg;
    switch (cmd) {
    case 8:
        return ssl->ssl3State->numRenegotiations;
    case 9: {
        int prev = ssl->ssl3State->numRenegotiations;
        ssl->ssl3State->numRenegotiations = 0;
        return prev;
    }
    case 10:
        return ssl->ssl3State->totalRenegotiations;
    case 11:
        return (int)ssl->ssl3State->flags;
    case 17:
        if (ssl->extraCerts == NULL) {
            ssl->extraCerts = sslSkNew(NULL);
            if (ssl->extraCerts == NULL) {
                return 0;
            }
        }
        sslSkPush(ssl->extraCerts, parg);
        return 1;
    case 18:
        if (ssl->extraCerts != NULL) {
            sslSkPopFree(ssl->extraCerts, sceSsl_lib_C1A2E14B);
            ssl->extraCerts = NULL;
        }
        return 1;
    default:
        return 0;
    }
}

/* 0x0258A8 */
int LIBSSL_NOINLINE ssl3CtxCtrl(SslCtx *ctx, int cmd, int larg, void *parg)
{
    (void)larg;
    if (cmd == 12) {
        if (ctx->extraCerts == NULL) {
            ctx->extraCerts = sslSkNew(NULL);
            if (ctx->extraCerts == NULL) {
                return 0;
            }
        }
        sslSkPush(ctx->extraCerts, parg);
        return 1;
    }
    return 0;
}

/* 0x025914 */
const SslCipher *LIBSSL_NOINLINE ssl3GetCipherByChar(const u8 *wireBytes)
{
    SslCipher keyCipher;
    const SslCipher *keyPtr = &keyCipher;
    const SslCipher **found;

    if (g_ssl3CipherSortInit != 0) {
        int idx;
        g_ssl3CipherSortInit = 0;
        for (idx = 0; idx < 35; idx++) {
            g_ssl3CiphersById[idx] = &g_ssl3Ciphers[idx];
        }
        sslQsort(g_ssl3CiphersById, 35U, sizeof(const SslCipher *), sslCipherIdCmp);
    }

    keyCipher.id = 0x03000000U | ((u32)wireBytes[0] << 8) | wireBytes[1];
    found = (const SslCipher **)sslObjBsearch(&keyPtr, g_ssl3CiphersById, 35,
                                              (int)sizeof(const SslCipher *), sslCipherIdCmp);
    if (found == NULL || (*found)->valid == 0) {
        return NULL;
    }
    return *found;
}

/* 0x0259F0 */
int LIBSSL_NOINLINE ssl3PutCipherByChar(const SslCipher *cipher, u8 *outBytes)
{
    if (outBytes == NULL) {
        return 2;
    }
    if ((cipher->id & 0xFF000000U) == 0x03000000U) {
        outBytes[0] = (u8)((cipher->id >> 8) & 0xFFU);
        outBytes[1] = (u8)(cipher->id & 0xFFU);
        return 2;
    }
    return 0;
}

/* 0x025A24 */
static int LIBSSL_NOINLINE ssl3ReadReturn(Ssl *ssl, int ret)
{
    ssl->rwstate = 3;
    if (ret >= 0) {
        ssl->initNum += ret;
        return 0;
    }
    return ret;
}

/* 0x025A4C */
int LIBSSL_NOINLINE ssl3Shutdown(Ssl *ssl)
{
    if (ssl->quietShutdown != 0 || (ssl->state == 0x4000 && ssl->shutdownMode == 0)) {
        ssl->shutdown = 3;
        return 1;
    }

    if ((ssl->shutdown & 1) == 0) {
        int ret;
        ssl->shutdown |= 1;
        ret = ssl3SendAlert(ssl, ssl->ctx->closeNotifyLevel, 0);
        if (ret <= 0) {
            return ret;
        }
    } else if (ssl->ssl3State->alertDispatch != 0) {
        int ret = ssl3DispatchAlert(ssl);
        if (ret <= 0) {
            return ret;
        }
    }

    if ((ssl->shutdown & 1) != 0 && ssl->shutdownMode != 0) {
        return 1;
    }
    if ((ssl->shutdown & 2) == 0) {
        int ret = ssl3ReadBytes(ssl, 0, NULL, 0);
        if (ret <= 0) {
            return ret;
        }
    }
    if (ssl->shutdown == 3 && ssl->ssl3State->alertDispatch == 0) {
        return 1;
    }
    return 0;
}

/* 0x025B5C */
int LIBSSL_NOINLINE ssl3Write(Ssl *ssl, const u8 *buf, int len)
{
    int written;

    if (ssl->ssl3State->renegotiate != 0) {
        ssl3RenegotiateCheck(ssl);
    }

    if ((ssl->ssl3State->flags & 4U) != 0 && ssl->wbio == ssl->bbio) {
        if (ssl->ssl3State->delayBufPopRet == 0) {
            written = ssl3WriteBytes(ssl, 23, buf, len);
            if (written <= 0) {
                return written;
            }
            ssl->ssl3State->delayBufPopRet = written;
        }
    } else {
        written = ssl3WriteBytes(ssl, 23, buf, len);
        if (written <= 0) {
            return written;
        }
        if ((ssl->ssl3State->flags & 4U) == 0 || ssl->wbio != ssl->bbio) {
            return written;
        }
        ssl->ssl3State->delayBufPopRet = written;
    }

    ssl->rwstate = 2;
    written = sceSsl_lib_9566709F(ssl->wbio, 11, 0, NULL);
    if (written > 0) {
        int retLen;
        ssl->rwstate = 1;
        sslFreeWbioBuffer(ssl);
        retLen = ssl->ssl3State->delayBufPopRet;
        ssl->ssl3State->delayBufPopRet = 0;
        ssl->ssl3State->flags &= ~4U;
        return retLen;
    }
    return written;
}

/* 0x025CA0 */
int LIBSSL_NOINLINE ssl3Read(Ssl *ssl, u8 *buf, int len)
{
    int ret;

    if (ssl->ssl3State->renegotiate != 0) {
        ssl3RenegotiateCheck(ssl);
    }
    ssl->ssl3State->inReadAppData = 1;
    ret = ssl3ReadBytes(ssl, 23, buf, len);
    if (ret == -1 && ssl->ssl3State->inReadAppData == 0) {
        sceSsl_lib_E78AD94D();
        ssl->ssl3State->inReadAppData = 0;
        ssl->inHandshake++;
        ret = ssl3ReadBytes(ssl, 23, buf, len);
        ssl->inHandshake--;
        return ret;
    }
    ssl->ssl3State->inReadAppData = 0;
    return ret;
}

/* 0x025D74 */
int LIBSSL_NOINLINE ssl3Peek(Ssl *ssl, u8 *buf, int len)
{
    Ssl3Record *rrec = &ssl->ssl3State->rrec;
    u32 copyLen;

    if (rrec->length == 0 || rrec->type != 23) {
        int got = ssl3Read(ssl, buf, 1);
        if (got <= 0) {
            return got;
        }
        rrec->length++;
        rrec->off--;
    }
    copyLen = rrec->length;
    if (copyLen >= (u32)len) {
        copyLen = (u32)len;
    }
    sslMemcpy(buf, rrec->data + rrec->off, copyLen);
    return (int)copyLen;
}

/* 0x025E28 */
int LIBSSL_NOINLINE ssl3Renegotiate(Ssl *ssl)
{
    if (ssl->handshakeFunc == NULL) {
        return 1;
    }
    if ((ssl->ssl3State->flags & 1U) != 0) {
        return 0;
    }
    ssl->ssl3State->renegotiate = 1;
    return 1;
}

/* 0x025E5C */
int LIBSSL_NOINLINE ssl3RenegotiateCheck(Ssl *ssl)
{
    Ssl3State *ssl3State = ssl->ssl3State;

    if (ssl3State->renegotiate == 0) {
        return 0;
    }
    if (ssl3State->rbufLeft != 0 || ssl3State->wbufLeft != 0) {
        return 0;
    }
    if ((sceSsl_lib_104F749D(ssl) & 0x3000) != 0) {
        return 0;
    }
    ssl->state = 12292;
    ssl3State->renegotiate = 0;
    ssl3State->totalRenegotiations++;
    ssl3State->numRenegotiations++;
    return 1;
}

/* ==================== ssl_algs.c (0x025EF0..0x026083) ==================== */

/* 0x025EF0 */
int LIBSSL_NOINLINE sceSsl_lib_9274BFE3(void)
{
    sslResetSupportedAlgorithms();
    sslRegisterStandardAlgs();
    sslX509TcInit();
    return 1;
}

/* 0x025F20 */
static int LIBSSL_NOINLINE sslRegisterStandardAlgs(void)
{
    sslEvpAddCipher(sslEvpDesCbc());
    sslEvpAddCipher(sslEvpDesEde3Cbc());
    sslEvpAddCipher(sslEvpRc4());
    sslEvpAddCipher(sslEvpRc2Cbc());

    sslEvpAddDigest(sslEvpMd2Rsa());
    sslEvpAddDigest(sslEvpMd5Rsa());
    sslObjNameAdd("ssl2-md5", 32769, "MD5");
    sslObjNameAdd("ssl3-md5", 32769, "MD5");
    sslEvpAddDigest(sslEvpSha1Rsa());
    sslObjNameAdd("ssl3-sha1", 32769, "SHA1");
    sslEvpAddDigest(sslEvpSha256());
    sslEvpAddDigest(sslEvpSha384());
    sslEvpAddDigest(sslEvpSha512());
    sslEvpAddDigest(sslEvpSha224());
    return 1;
}

/* 0x02602C */
int LIBSSL_NOINLINE sceSsl_lib_CF78BEFB(void)
{
    sslEvpCleanup();
    sceSsl_lib_56E19CEB();
    sslObjNameCleanup(-1);
    sslObjCleanup();
    sslX509StoreCtxGetError(NULL);
    sslResetSupportedAlgorithms();
    sceSsl_lib_D8C05497(0);
    sceSsl_lib_101D418A();
    return 1;
}

/* ==================== ssl_cert.c (0x026084..0x0263BB) ==================== */

/* 0x026084 */
int LIBSSL_NOINLINE sceSsl_lib_BB1564A3(void)
{
    if (g_sslCertExDataIndex < 0) {
        g_sslCertExDataIndex = sslX509VerifyCert(NULL);
    }
    return g_sslCertExDataIndex;
}

/* 0x0260D4 */
SslCert *LIBSSL_NOINLINE sslCertNew(void)
{
    SslCert *cert = (SslCert *)sceSsl_lib_0861D934(sizeof(SslCert));

    if (cert == NULL) {
        sceSsl_lib_9CDCA0D7(20, 163, 33, NULL, 0);
        return NULL;
    }
    sslMemset(cert, 0, sizeof(SslCert));
    cert->activeKey = &cert->pkeys[0];
    cert->references = 1;
    return cert;
}

/* 0x02614C */
void LIBSSL_NOINLINE sslCertFree(SslCert *cert)
{
    int idx;

    if (sslCryptoAddLock(&cert->references, -1, 13, "ssl/ssl_cert.c", 79) > 0) {
        return;
    }
    if (cert->rsaTmp != NULL) {
        sslRsaFree(cert->rsaTmp);
    }
    if (cert->rsaTmpExport != NULL) {
        sslRsaFree(cert->rsaTmpExport);
    }
    if (cert->ephemeralRsa != NULL) {
        sslRsaFree(cert->ephemeralRsa);
    }
    for (idx = 0; idx < 5; idx++) {
        if (cert->pkeys[idx].certObj != NULL) {
            sceSsl_lib_C1A2E14B(cert->pkeys[idx].certObj);
        }
        if (cert->pkeys[idx].privateKey != NULL) {
            sceSsl_lib_9307DE23(cert->pkeys[idx].privateKey);
        }
    }
    if (cert->certChain != NULL) {
        sslSkPopFree(cert->certChain, sceSsl_lib_C1A2E14B);
    }
    sceSsl_lib_5E5C873A(cert);
}

/* 0x026234 */
int LIBSSL_NOINLINE sslVerifyCertChain(Ssl *ssl, SslStack *certChain)
{
    int verified;

    if (certChain == NULL || certChain->num == 0) {
        return 0;
    }
    if (ssl->ctx->appVerifyCb != NULL) {
        verified = sslX509AppVerifyCertChain(ssl, certChain, ssl->ctx->appVerifyCb,
                                             ssl->ctx->appVerifyArg, &ssl->verifyResult);
    } else {
        verified = sslX509DefaultVerifyCertChain(ssl, certChain, ssl->ctx->certStore,
                                                 ssl->ctx->defaultVerifyCb,
                                                 ssl->ctx->defaultVerifyArg,
                                                 &ssl->verifyResult);
    }
    if (ssl->verifyResult == 50) {
        sceSsl_lib_9CDCA0D7(20, 208, 194, NULL, 0);
    }
    if (verified != 0) {
        sceSsl_lib_04E677AF();
    }
    return verified;
}

/* 0x0262E0 */
static SslStack *LIBSSL_NOINLINE sslGetClientCaList(const Ssl *ssl)
{
    if (ssl->type == 4096) {
        if ((ssl->version >> 8) == 3) {
            if (ssl->ssl3State == NULL) {
                return NULL;
            }
            return ssl->ssl3State->tmpCaNames;
        }
        return NULL;
    }
    if (ssl->clientCa != NULL) {
        return ssl->clientCa;
    }
    return ssl->ctx->clientCa;
}

/* 0x026330 */
int LIBSSL_NOINLINE sceSsl_lib_92FA8B34(const Ssl *ssl)
{
    SslStack *list = sslGetClientCaList(ssl);
    if (list != NULL) {
        return list->num;
    }
    return 0;
}

/* 0x02635C */
void *LIBSSL_NOINLINE sceSsl_lib_7BDAF864(const Ssl *ssl, int idx)
{
    SslStack *list = sslGetClientCaList(ssl);
    if (list != NULL && idx >= 0 && idx < list->num) {
        return list->data[idx];
    }
    return NULL;
}

/* 0x0263B4 */
void LIBSSL_NOINLINE sceSsl_lib_BF55C31C(SslCtx *ctx, SslClientCertCb cb)
{
    ctx->clientCertCb = cb;
}

/* ==================== ssl_lib.c (0x0263BC..0x0278F3) ==================== */

/* 0x0263BC */
int LIBSSL_NOINLINE sceSsl_lib_54A7D8F3(Ssl *ssl)
{
    if (ssl->method == NULL) {
        sceSsl_lib_9CDCA0D7(20, 165, 188, NULL, 0);
        return 0;
    }
    if (sslBadSessionCheck(ssl) != 0) {
        sceSsl_lib_BDAD0841(ssl->session);
        ssl->session = NULL;
    }
    ssl->state = (ssl->server != 0) ? 24576 : 20480;
    ssl->debug = 0;
    ssl->hit = 0;
    ssl->shutdown = 0;
    ssl->type = 0;
    ssl->rwstate = 1;
    ssl->clientVersion = ssl->method->version;
    ssl->rstate = 240;
    ssl->version = ssl->method->version;
    ssl->mode = ssl->ctx->mode;
    if (ssl->initBuf != NULL) {
        sslBufMemFree(ssl->initBuf);
        ssl->initBuf = NULL;
    }
    sslClearCipherCtx(ssl);
    ssl->firstPacket = 0;
    if (ssl->session == NULL || ssl->handshakeFunc == ssl->method->sslAccept) {
        return (sslSetSslMethod(ssl, ssl->ctx->method) != 0) ? 1 : 0;
    }
    ssl->method->sslClear(ssl);
    return 1;
}

/* 0x0264DC */
Ssl *LIBSSL_NOINLINE sceSsl_lib_EBFB0E3C(SslCtx *ctx)
{
    Ssl *ssl;
    int errReason = 33;

    if (ctx == NULL) {
        sceSsl_lib_9CDCA0D7(20, 186, 195, NULL, 0);
        return NULL;
    }
    if (ctx->method == NULL) {
        sceSsl_lib_9CDCA0D7(20, 186, 228, NULL, 0);
        return NULL;
    }

    ssl = (Ssl *)sceSsl_lib_0861D934(sizeof(Ssl));
    if (ssl != NULL) {
        sslMemset(ssl, 0, sizeof(Ssl));
        if (ctx->cert != NULL) {
            sslCryptoAddLock(&ctx->cert->references, 1, 13, "ssl/ssl_lib.c", 236);
            ssl->cert = ctx->cert;
        }
        ssl->sidCtxLength = ctx->sidCtxLength;
        __builtin_memcpy(ssl->sidCtx, ctx->sidCtx, sizeof(ssl->sidCtx));
        ssl->verifyMode = ctx->verifyMode;
        ssl->verifyCallback = ctx->defaultVerifyCb;
        sslCryptoAddLock(&ctx->references, 1, 12, "ssl/ssl_lib.c", 247);
        ssl->ctx = ctx;
        ssl->method = ctx->method;
        ssl->verifyResult = 0;
        ssl->mdCtx = sslEvpMdCtxNew();
        if (ssl->mdCtx != NULL && ssl->method->sslNew(ssl) != 0) {
            ssl->quietShutdown = ctx->quietShutdown;
            ssl->shutdownMode = ctx->shutdownMode;
            ssl->references = 1;
            ssl->server = (ctx->method->sslAccept != sslUndefinedFunction) ? 1 : 0;
            ssl->extraCerts = NULL;
            ssl->readBufferSize = ctx->readBufferSize;
            ssl->options = ctx->options;
            ssl->maxSendFragment = ctx->maxSendFragment;
            ssl->rsaBlinding = ctx->rsaBlinding;
            sceSsl_lib_54A7D8F3(ssl);
            sslCryptoNewExData(g_sslExData, ssl, &ssl->exData);
            return ssl;
        }
        if (ssl->mdCtx != NULL) {
            sslEvpMdCtxFree(ssl->mdCtx);
        }
        sceSsl_lib_588F2FE8(ctx);
        sceSsl_lib_5E5C873A(ssl);
    }

    sceSsl_lib_9CDCA0D7(20, 186, errReason, NULL, 0);
    return NULL;
}

/* 0x0266F0 */
void LIBSSL_NOINLINE sceSsl_lib_84833472(Ssl *ssl)
{
    if (sslCryptoAddLock(&ssl->references, -1, 15, "ssl/ssl_lib.c", 294) > 0) {
        return;
    }
    sslCryptoFreeExData(g_sslExData, ssl, &ssl->exData);
    if (ssl->bbio != NULL) {
        if (ssl->bbio == ssl->wbio) {
            ssl->wbio = sceSsl_lib_732F4E44(ssl->bbio);
        }
        sceSsl_lib_3E040FD4(ssl->bbio);
        ssl->bbio = NULL;
    }
    if (ssl->rbio != NULL) {
        sceSsl_lib_959557F5(ssl->rbio);
    }
    if (ssl->wbio != NULL && ssl->wbio != ssl->rbio) {
        sceSsl_lib_959557F5(ssl->wbio);
    }
    if (ssl->initBuf != NULL) {
        sslBufMemFree(ssl->initBuf);
    }
    if (ssl->cipherList != NULL) {
        sslSkFree(ssl->cipherList);
    }
    if (ssl->cipherListById != NULL) {
        sslSkFree(ssl->cipherListById);
    }
    if (ssl->session != NULL) {
        sslBadSessionCheck(ssl);
        sceSsl_lib_BDAD0841(ssl->session);
    }
    sslClearCipherCtx(ssl);
    if (ssl->extraCerts != NULL) {
        sslSkPopFree(ssl->extraCerts, sceSsl_lib_C1A2E14B);
    }
    if (ssl->cert != NULL) {
        sslCertFree(ssl->cert);
    }
    if (ssl->mdCtx != NULL) {
        sslEvpMdCtxFree(ssl->mdCtx);
    }
    if (ssl->ctx != NULL) {
        sceSsl_lib_588F2FE8(ssl->ctx);
    }
    if (ssl->clientCa != NULL) {
        sslSkPopFree(ssl->clientCa, sceSsl_lib_ED95A457);
    }
    if (ssl->method != NULL) {
        ssl->method->sslFree(ssl);
    }
    sceSsl_lib_5E5C873A(ssl);
}

/* 0x026898 */
void LIBSSL_NOINLINE sceSsl_lib_B9C8CCE6(Ssl *ssl, SslBio *rbio, SslBio *wbio)
{
    if (ssl->bbio != NULL && ssl->wbio == ssl->bbio) {
        ssl->wbio = ssl->bbio->nextBio;
        ssl->bbio->nextBio = NULL;
    }
    if (ssl->rbio != NULL && ssl->rbio != rbio) {
        sceSsl_lib_959557F5(ssl->rbio);
    }
    if (ssl->wbio != NULL && ssl->wbio != wbio && ssl->rbio != ssl->wbio) {
        sceSsl_lib_959557F5(ssl->wbio);
    }
    ssl->rbio = rbio;
    ssl->wbio = wbio;
}

/* 0x02693C */
SslBio *LIBSSL_NOINLINE sceSsl_lib_2E2E2B09(const Ssl *ssl)
{
    return ssl->rbio;
}

/* 0x026944 */
SslBio *LIBSSL_NOINLINE sceSsl_lib_868C15DF(const Ssl *ssl)
{
    return ssl->wbio;
}

/* 0x02694C */
int LIBSSL_NOINLINE sceSsl_lib_D49A570C(const Ssl *ssl)
{
    return ssl->method->getTimeout();
}

/* 0x026970 */
int LIBSSL_NOINLINE sceSsl_lib_E7C29542(Ssl *ssl, void *buf, int num)
{
    if ((ssl->shutdown & 2) != 0) {
        ssl->rwstate = 1;
        return 0;
    }
    return ssl->method->sslRead(ssl, (u8 *)buf, num);
}

/* 0x0269B8 */
int LIBSSL_NOINLINE sceSsl_lib_B7CA8717(Ssl *ssl, const void *buf, int num)
{
    if ((ssl->shutdown & 1) != 0) {
        ssl->rwstate = 1;
        sceSsl_lib_9CDCA0D7(20, 209, 207, NULL, 0);
        return -1;
    }
    return ssl->method->sslWrite(ssl, (const u8 *)buf, num);
}

/* 0x026A10 */
int LIBSSL_NOINLINE sceSsl_lib_3E3133D6(Ssl *ssl)
{
    if (ssl == NULL) {
        return 1;
    }
    if ((sceSsl_lib_104F749D(ssl) & 0x3000) == 0 || sceSsl_lib_340F8829(ssl) != 0) {
        return ssl->method->sslShutdown(ssl);
    }
    return 1;
}

/* 0x026A74 */
int LIBSSL_NOINLINE sceSsl_lib_B4D78E98(SslCtx *ctx, int cmd, int larg, void *parg)
{
    int prev;

    switch (cmd) {
    case 20: return (int)ctx->sessions->numItems;
    case 21: return ctx->stats.sessConnect;
    case 22: return ctx->stats.sessConnectGood;
    case 23: return ctx->stats.sessConnectRenegotiate;
    case 24: return ctx->stats.sessAccept;
    case 25: return ctx->stats.sessAcceptGood;
    case 26: return ctx->stats.sessAcceptRenegotiate;
    case 27: return ctx->stats.sessMiss;
    case 28: return ctx->stats.sessTimeout;
    case 29: return ctx->stats.sessHit;
    case 30: return ctx->stats.sessCbHit;
    case 31: return ctx->stats.sessCacheFull;
    case 32:
        ctx->options |= (u32)larg;
        return (int)ctx->options;
    case 40: return ctx->mode;
    case 41:
        prev = ctx->mode;
        ctx->mode = larg;
        return prev;
    case 42:
        prev = ctx->sessionCacheSize;
        ctx->sessionCacheSize = larg;
        return prev;
    case 43: return ctx->sessionCacheSize;
    case 44:
        prev = ctx->sessionCacheMode;
        ctx->sessionCacheMode = larg;
        return prev;
    case 45: return ctx->sessionCacheMode;
    case 50:
        prev = (int)ctx->maxSendFragment;
        ctx->maxSendFragment = (u32)larg;
        return prev;
    case 51: return (int)ctx->maxSendFragment;
    case 53:
        prev = ctx->readBufferSize;
        ctx->readBufferSize = larg;
        return prev;
    case 54: return ctx->readBufferSize;
    case 55: return ctx->references;
    case 56:
        prev = ctx->rsaBlinding;
        ctx->rsaBlinding = larg;
        return prev;
    default:
        return ctx->method->sslCtxCtrl(ctx, cmd, larg, parg);
    }
}

/* 0x026BB8 */
int LIBSSL_NOINLINE sslCipherIdCmp(const void *lhs, const void *rhs)
{
    const SslCipher *const *leftPtr = (const SslCipher *const *)lhs;
    const SslCipher *const *rightPtr = (const SslCipher *const *)rhs;
    int diff = (int)((*leftPtr)->id - (*rightPtr)->id);

    if (diff == 0) {
        return 0;
    }
    return (diff > 0) ? 1 : -1;
}

/* 0x026BEC */
SslStack *LIBSSL_NOINLINE sslGetCiphers(const Ssl *ssl)
{
    if (ssl != NULL) {
        if (ssl->cipherList != NULL) {
            return ssl->cipherList;
        }
        if (ssl->ctx != NULL) {
            return ssl->ctx->cipherList;
        }
    }
    return NULL;
}

/* 0x026C24 */
SslStack *LIBSSL_NOINLINE sslGetCiphersById(const Ssl *ssl)
{
    if (ssl != NULL) {
        if (ssl->cipherListById != NULL) {
            return ssl->cipherListById;
        }
        if (ssl->ctx != NULL) {
            return ssl->ctx->cipherListById;
        }
    }
    return NULL;
}

/* 0x026C5C */
int LIBSSL_NOINLINE sslCipherListToBytes(const Ssl *ssl, const SslStack *ciphers, u8 *outBytes)
{
    u8 *cursor = outBytes;
    int idx;

    if (ciphers == NULL) {
        return 0;
    }
    for (idx = 0; idx < ciphers->num; idx++) {
        const SslCipher *ciph = (const SslCipher *)ciphers->data[idx];
        int step = ssl->method->putCipherByChar(ciph, cursor);
        if (cursor != NULL) {
            cursor += step;
        }
    }
    return (int)(cursor - outBytes);
}

/* 0x026CF8 */
static u32 LIBSSL_NOINLINE sslSessionHash(const SslSession *session)
{
    const u8 *id = session->sessionId;
    return (u32)id[0] | ((u32)id[1] << 8) | ((u32)id[2] << 16) | ((u32)id[3] << 24);
}

/* 0x026D24 */
static int LIBSSL_NOINLINE sslSessionCmp(const SslSession *lhs, const SslSession *rhs)
{
    if (lhs->sslVersion == rhs->sslVersion && lhs->sessionIdLength == rhs->sessionIdLength) {
        return sslMemcmp(lhs->sessionId, rhs->sessionId, (int)lhs->sessionIdLength);
    }
    return 1;
}

/* 0x026D70 */
SslCtx *LIBSSL_NOINLINE sceSsl_lib_FB8273FE(const SslMethod *method)
{
    SslCtx *ctx;
    int errReason = 33;

    if (method == NULL) {
        sceSsl_lib_9CDCA0D7(20, 168, 196, NULL, 0);
        return NULL;
    }

    ctx = (SslCtx *)sceSsl_lib_0861D934(sizeof(SslCtx));
    if (ctx != NULL) {
        sslMemset(ctx, 0, sizeof(SslCtx));
        ctx->sessionCacheMode = 2;
        ctx->sessionCacheSize = 20480;
        ctx->method = method;
        ctx->certStore = NULL;
        ctx->sessionCacheHead = NULL;
        ctx->sessionCacheTail = NULL;
        ctx->sessionTimeout = method->getTimeout();
        ctx->newSessionCb = NULL;
        ctx->removeSessionCb = NULL;
        ctx->getSessionCb = NULL;
        sslMemset(&ctx->stats, 0, sizeof(ctx->stats));
        ctx->references = 1;
        ctx->defaultVerifyDepth = -1;
        ctx->quietShutdown = 0;
        ctx->shutdownMode = 0;
        ctx->infoCallback = NULL;
        ctx->alertCallback = NULL;
        ctx->alertCallbackArg = NULL;
        ctx->msgCallback = NULL;
        ctx->msgCallbackArg = NULL;
        ctx->defaultVerifyCb = NULL;
        ctx->defaultVerifyArg = NULL;
        ctx->appVerifyCb = NULL;
        ctx->appVerifyArg = NULL;
        ctx->mode = 0;
        ctx->verifyMode = 0;
        ctx->verifyArg = NULL;

        ctx->cert = sslCertNew();
        if (ctx->cert != NULL) {
            ctx->defaultPasswdCb = NULL;
            ctx->clientCertCb = NULL;
            ctx->sessions = sslLhNew((SslLhHashFn)sslSessionHash, (SslLhCmpFn)sslSessionCmp);
            if (ctx->sessions != NULL) {
                ctx->certStore = sslX509StoreNew();
                if (ctx->certStore != NULL && sceSsl_lib_BB1564A3() < 0) {
                    sceSsl_lib_9CDCA0D7(20, 168, 269, NULL, 0);
                    errReason = 33;
                } else {
                    sslCreateCipherList(ctx->method, &ctx->cipherList, &ctx->cipherListById,
                                        "ALL:!ADH:RC4+RSA:+HIGH:+MEDIUM:+LOW:+SSLv2:+EXP");
                    if (ctx->cipherList == NULL || ctx->cipherList->num <= 0) {
                        errReason = 161;
                    } else {
                        ctx->md5Ssl2 = sslEvpGetDigestByName("ssl2-md5");
                        if (ctx->md5Ssl2 == NULL) {
                            errReason = 241;
                        } else {
                            ctx->md5 = sslEvpGetDigestByName("ssl3-md5");
                            if (ctx->md5 == NULL) {
                                errReason = 242;
                            } else {
                                ctx->sha1 = sslEvpGetDigestByName("ssl3-sha1");
                                if (ctx->sha1 == NULL) {
                                    errReason = 243;
                                } else {
                                    int dsaOk = 1;
                                    if (sslAlgorithmSupported(2) != 0) {
                                        ctx->dsaSha1 = sslEvpGetDigestByName("DSA-SHA1");
                                        if (ctx->dsaSha1 == NULL) {
                                            errReason = 1102;
                                            dsaOk = 0;
                                        }
                                    } else {
                                        ctx->dsaSha1 = NULL;
                                    }
                                    if (dsaOk != 0) {
                                        ctx->clientCa = sslSkNew(NULL);
                                        if (ctx->clientCa != NULL) {
                                            sslCryptoNewExData(g_sslCtxExData, ctx, &ctx->exData);
                                            ctx->readBufferSize = 4096;
                                            ctx->maxSendFragment = 16384U;
                                            ctx->closeNotifyLevel = 1;
                                            ctx->extraCerts = NULL;
                                            ctx->rsaBlinding = 1;
                                            return ctx;
                                        }
                                        errReason = 33;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    sceSsl_lib_9CDCA0D7(20, 168, errReason, NULL, 0);
    if (ctx != NULL) {
        sceSsl_lib_588F2FE8(ctx);
    }
    return NULL;
}

/* 0x027028 */
void LIBSSL_NOINLINE sceSsl_lib_588F2FE8(SslCtx *ctx)
{
    if (ctx == NULL || sslCryptoAddLock(&ctx->references, -1, 12, "ssl/ssl_lib.c", 1321) > 0) {
        return;
    }
    sslCryptoFreeExData(g_sslCtxExData, ctx, &ctx->exData);
    if (ctx->sessions != NULL) {
        sceSsl_lib_4E0F2CCC(ctx, 0);
        sslLhFree(ctx->sessions);
    }
    if (ctx->certStore != NULL) {
        sslX509StoreFree(ctx->certStore);
    }
    if (ctx->cipherList != NULL) {
        sslSkFree(ctx->cipherList);
    }
    if (ctx->cipherListById != NULL) {
        sslSkFree(ctx->cipherListById);
    }
    if (ctx->cert != NULL) {
        sslCertFree(ctx->cert);
    }
    if (ctx->clientCa != NULL) {
        sslSkPopFree(ctx->clientCa, sceSsl_lib_ED95A457);
    }
    if (ctx->extraCerts != NULL) {
        sslSkPopFree(ctx->extraCerts, sceSsl_lib_C1A2E14B);
    }
    sceSsl_lib_5E5C873A(ctx);
}

/* 0x027128 */
void LIBSSL_NOINLINE sceSsl_lib_529A9477(SslCtx *ctx, SslAppVerifyCb cb, void *arg)
{
    ctx->appVerifyCb = cb;
    ctx->appVerifyArg = arg;
}

/* 0x027134 */
void LIBSSL_NOINLINE sceSsl_lib_AEBF278B(SslCtx *ctx, int mode, void *verifyArg)
{
    ctx->verifyMode = mode;
    ctx->verifyArg = verifyArg;
    if (ctx->certStore != NULL) {
        sslX509StoreSetVerifyArg(ctx->certStore, verifyArg);
    }
}

/* 0x027170 */
void LIBSSL_NOINLINE sslUpdateCache(Ssl *ssl, int modeFlag)
{
    SslCtx *ctx = ssl->ctx;

    if (ssl->session->sessionIdLength == 0) {
        return;
    }
    if ((ctx->sessionCacheMode & modeFlag) != 0 && ssl->hit == 0) {
        if (sslCtxAddSession(ctx, ssl->session) != 0 && ctx->newSessionCb != NULL) {
            sslCryptoAddLock(&ssl->session->references, 1, 14, "ssl/ssl_lib.c", 1649);
            if (ctx->newSessionCb(ssl, ssl->session) == 0) {
                sceSsl_lib_BDAD0841(ssl->session);
            }
        }
    }

    if ((ctx->sessionCacheMode & 0x80) == 0 &&
        (ctx->sessionCacheMode & modeFlag) == modeFlag) {
        u8 counter = ((modeFlag & 1) != 0)
            ? (u8)(ctx->stats.sessConnectGood & 0xFF)
            : (u8)(ctx->stats.sessAcceptGood & 0xFF);
        if (counter == 255U) {
            sceSsl_lib_4E0F2CCC(ctx, 1);
        }
    }
}

/* 0x027274 */
int LIBSSL_NOINLINE sslSetSslMethod(Ssl *ssl, const SslMethod *method)
{
    int ret = 1;
    int wasConnect = -1;

    if (ssl->method != method) {
        if (ssl->handshakeFunc != NULL) {
            wasConnect = (ssl->handshakeFunc == ssl->method->sslConnect) ? 1 : 0;
        }
        if (ssl->method->version == method->version) {
            ssl->method = method;
        } else {
            ssl->method->sslFree(ssl);
            ssl->method = method;
            ret = method->sslNew(ssl);
        }
        if (wasConnect == 1) {
            ssl->handshakeFunc = method->sslConnect;
        } else if (wasConnect == 0) {
            ssl->handshakeFunc = method->sslAccept;
        }
    }
    ssl->method->sslClear(ssl);
    return ret;
}

/* 0x027344 */
int LIBSSL_NOINLINE sceSsl_lib_B3B04C58(const Ssl *ssl, int retVal)
{
    u32 errCode;

    if (retVal > 0) {
        return 0;
    }
    errCode = sceSsl_lib_47F7B97F();
    if (errCode != 0) {
        return ((errCode >> 24) == 2U) ? 5 : 1;
    }

    if (retVal < 0) {
        if (sceSsl_lib_C97D0510(ssl) == 3) {
            SslBio *bio = sceSsl_lib_2E2E2B09(ssl);
            if (sceSsl_lib_E3C30923(bio) != 0) {
                return 2;
            }
            if (sceSsl_lib_089FF1F1(bio) != 0) {
                return 3;
            }
            if (sceSsl_lib_ABCD28B8(bio) != 0) {
                return (sceSsl_lib_4A6078CF(bio) == 2) ? 7 : 5;
            }
        }
        if (sceSsl_lib_C97D0510(ssl) == 2) {
            SslBio *bio = sceSsl_lib_868C15DF(ssl);
            if (sceSsl_lib_089FF1F1(bio) != 0) {
                return 3;
            }
            if (sceSsl_lib_E3C30923(bio) != 0) {
                return 2;
            }
            if (sceSsl_lib_ABCD28B8(bio) != 0) {
                return (sceSsl_lib_4A6078CF(bio) == 2) ? 7 : 5;
            }
        }
        if (sceSsl_lib_C97D0510(ssl) == 4) {
            return 4;
        }
    }

    if (retVal == 0) {
        if (ssl->version == 2) {
            return 6;
        }
        if ((ssl->shutdown & 2) != 0 && ssl->ssl3State->warnAlert == 0) {
            return 6;
        }
    }
    return 5;
}

/* 0x0274C8 */
int LIBSSL_NOINLINE sceSsl_lib_D53ACF30(Ssl *ssl)
{
    if (ssl->handshakeFunc == NULL) {
        sceSsl_lib_9CDCA0D7(20, 180, 144, NULL, 0);
        return -1;
    }
    ssl->method->sslRenegotiateCheck(ssl);
    if ((sceSsl_lib_104F749D(ssl) & 0x3000) != 0 ||
        (sceSsl_lib_104F749D(ssl) & 0x4000) != 0) {
        return ssl->handshakeFunc(ssl);
    }
    return 1;
}

/* 0x027568 */
void LIBSSL_NOINLINE sceSsl_lib_80608663(Ssl *ssl)
{
    ssl->state = 20480;
    ssl->server = 0;
    ssl->handshakeFunc = ssl->method->sslConnect;
    ssl->shutdown = 0;
    sslClearCipherCtx(ssl);
}

/* 0x02759C */
int LIBSSL_NOINLINE sslUndefinedFunction(Ssl *ssl)
{
    (void)ssl;
    sceSsl_lib_9CDCA0D7(20, 198, 34, NULL, 0);
    return 0;
}

/* 0x0275CC */
const SslMethod *LIBSSL_NOINLINE sslUndefinedConstFunction(int version)
{
    (void)version;
    sceSsl_lib_9CDCA0D7(20, 161, 34, NULL, 0);
    return NULL;
}

/* 0x0275FC */
static void LIBSSL_NOINLINE sslClearCipherCtx(Ssl *ssl)
{
    typedef struct SslCipherDirState {
        u32 macSize;
        void *hashCtx;
        SslEvpCipherCtx *cipherCtx;
    } SslCipherDirState;

    SslCipherDirState *dirs[2];
    int idx;

    dirs[0] = (SslCipherDirState *)&ssl->readMacSize;
    dirs[1] = (SslCipherDirState *)&ssl->writeMacSize;
    for (idx = 0; idx < 2; idx++) {
        if (dirs[idx]->cipherCtx != NULL) {
            sslEvpCipherCtxFree(dirs[idx]->cipherCtx);
            dirs[idx]->cipherCtx = NULL;
        }
        if (dirs[idx]->hashCtx != NULL) {
            ssl->macFreeFunc(dirs[idx]->hashCtx);
            dirs[idx]->hashCtx = NULL;
            dirs[idx]->macSize = 0;
        }
    }
}

/* 0x02769C */
const SslCipher *LIBSSL_NOINLINE sceSsl_lib_88897D26(const Ssl *ssl)
{
    if (ssl->session != NULL) {
        return ssl->session->cipher;
    }
    return NULL;
}

/* 0x0276C0 */
int LIBSSL_NOINLINE sslInitWbioBuffer(Ssl *ssl, int pushNow)
{
    SslBio *bbio = ssl->bbio;

    if (bbio != NULL) {
        if (bbio == ssl->wbio) {
            ssl->wbio = sceSsl_lib_732F4E44(bbio);
        }
    } else {
        bbio = sceSsl_lib_5DED429A(sslBioFBuffer());
        if (bbio == NULL) {
            return 0;
        }
        ssl->bbio = bbio;
    }

    sceSsl_lib_9566709F(bbio, 1, 0, NULL);
    if (sslBioIntCtrl(bbio, 117, 2048, 1) == 0 &&
        sslBioIntCtrl(bbio, 117, 1, 0) == 0) {
        sceSsl_lib_9CDCA0D7(20, 184, 2055, NULL, 0);
        return 0;
    }

    if (pushNow != 0) {
        if (ssl->wbio != bbio) {
            ssl->wbio = sceSsl_lib_1EC64594(bbio, ssl->wbio);
        }
    } else {
        if (ssl->wbio == bbio) {
            ssl->wbio = sceSsl_lib_732F4E44(bbio);
        }
    }
    return 1;
}

/* 0x0277D4 */
void LIBSSL_NOINLINE sslFreeWbioBuffer(Ssl *ssl)
{
    if (ssl->bbio != NULL) {
        if (ssl->bbio == ssl->wbio) {
            SslBio *nextBio = sceSsl_lib_732F4E44(ssl->bbio);
            if (nextBio != NULL) {
                ssl->wbio = nextBio;
            }
        }
        sceSsl_lib_3E040FD4(ssl->bbio);
        ssl->bbio = NULL;
    }
}

/* 0x027824 */
void LIBSSL_NOINLINE sceSsl_lib_42DF4592(SslCtx *ctx, int quiet)
{
    ctx->quietShutdown = quiet;
}

/* 0x02782C */
int LIBSSL_NOINLINE sceSsl_lib_E14101E4(const SslCtx *ctx)
{
    return ctx->quietShutdown;
}

/* 0x027834 */
void LIBSSL_NOINLINE sceSsl_lib_4AF959E3(Ssl *ssl, int quiet)
{
    ssl->quietShutdown = quiet;
}

/* 0x02783C */
int LIBSSL_NOINLINE sceSsl_lib_6A19E0AA(const Ssl *ssl)
{
    return ssl->quietShutdown;
}

/* 0x027844 */
void LIBSSL_NOINLINE sceSsl_lib_D5ABC603(SslCtx *ctx, int mode)
{
    ctx->shutdownMode = mode;
}

/* 0x02784C */
int LIBSSL_NOINLINE sceSsl_lib_909E9D31(const SslCtx *ctx)
{
    return ctx->shutdownMode;
}

/* 0x027854 */
void LIBSSL_NOINLINE sceSsl_lib_AE3986D3(Ssl *ssl, int mode)
{
    ssl->shutdownMode = mode;
}

/* 0x02785C */
int LIBSSL_NOINLINE sceSsl_lib_340F8829(const Ssl *ssl)
{
    return ssl->shutdownMode;
}

/* 0x027864 */
void LIBSSL_NOINLINE sceSsl_lib_86D4034D(Ssl *ssl, int mode)
{
    ssl->shutdown = mode;
}

/* 0x02786C */
int LIBSSL_NOINLINE sceSsl_lib_D1205C58(const Ssl *ssl)
{
    return ssl->shutdown;
}

/* 0x027874 */
int LIBSSL_NOINLINE sceSsl_lib_57F2E960(const Ssl *ssl)
{
    return ssl->version;
}

/* 0x02787C */
SslCtx *LIBSSL_NOINLINE sceSsl_lib_E92302C5(const Ssl *ssl)
{
    return ssl->ctx;
}

/* 0x027884 */
void LIBSSL_NOINLINE sceSsl_lib_C270B3A8(Ssl *ssl, SslInfoCb cb)
{
    ssl->infoCallback = cb;
}

/* 0x02788C */
void LIBSSL_NOINLINE sceSsl_lib_BE423CD1(Ssl *ssl, SslAlertCb cb, void *arg)
{
    ssl->alertCallback = cb;
    ssl->alertCallbackArg = arg;
}

/* 0x027898 */
SslInfoCb LIBSSL_NOINLINE sceSsl_lib_95E346AB(const Ssl *ssl)
{
    return ssl->infoCallback;
}

/* 0x0278A0 */
void LIBSSL_NOINLINE sceSsl_lib_9CB1A62F(Ssl *ssl, SslMsgCb cb, void *arg)
{
    ssl->msgCallback = cb;
    ssl->msgCallbackArg = arg;
}

/* 0x0278AC */
int LIBSSL_NOINLINE sceSsl_lib_104F749D(const Ssl *ssl)
{
    return ssl->state;
}

/* 0x0278B4 */
void LIBSSL_NOINLINE sceSsl_lib_5605C9FE(Ssl *ssl, int verifyResult)
{
    ssl->verifyResult = verifyResult;
}

/* 0x0278BC */
int LIBSSL_NOINLINE sceSsl_lib_AA3B27CF(const Ssl *ssl)
{
    return ssl->verifyResult;
}

/* 0x0278C4 */
int LIBSSL_NOINLINE sceSsl_lib_C97D0510(const Ssl *ssl)
{
    return ssl->rwstate;
}

/* 0x0278CC */
int LIBSSL_NOINLINE sceSsl_lib_A8F0AD39(const Ssl *ssl)
{
    return ssl->hit;
}

/* 0x0278D4 */
int LIBSSL_NOINLINE sslResetSupportedAlgorithms(void)
{
    g_sslEnabledAlgorithms = 0;
    return 1;
}

/* 0x0278E4 */
u32 LIBSSL_NOINLINE sslAlgorithmSupported(u32 algFlag)
{
    return g_sslEnabledAlgorithms & algFlag;
}

/* ==================== ssl_rsa.c (0x0278F4..0x027D9F) ==================== */

/* 0x0278F4 */
int LIBSSL_NOINLINE sceSsl_lib_C0ABBB57(Ssl *ssl, void *certObj)
{
    if (certObj == NULL) {
        sceSsl_lib_9CDCA0D7(20, 199, 35, NULL, 0);
        return 0;
    }
    if (ssl->cert == NULL || ssl->cert == ssl->ctx->cert) {
        SslCert *newCert = sslCertNew();
        if (newCert == NULL) {
            sceSsl_lib_9CDCA0D7(20, 199, 33, NULL, 0);
            return 0;
        }
        if (ssl->cert != NULL) {
            sslCertFree(ssl->cert);
        }
        ssl->cert = newCert;
    }
    return sslSetCert(ssl->cert, certObj);
}

/* 0x0279B0 */
static int LIBSSL_NOINLINE sslSetPrivateKey(SslCert *cert, SslEvpPkey *pkey, int rsaBlinding)
{
    int slotIdx = sslCertType(NULL, pkey);
    int keyType;

    if (slotIdx < 0) {
        sceSsl_lib_9CDCA0D7(20, 194, 247, NULL, 0);
        return 0;
    }

    if (cert->pkeys[slotIdx].certObj != NULL) {
        SslEvpPkey *pubKey = sceSsl_lib_B87FFE37(cert->pkeys[slotIdx].certObj);
        sslEvpPkeyCopyParameters(pubKey, pkey);
        sceSsl_lib_04E677AF();

        if (pkey->type != 6 || (sslRsaFlags((const SslRsa *)pkey->pkey) & 1) == 0) {
            if (sceSsl_lib_C6ED5C87(cert->pkeys[slotIdx].certObj, pkey) == 0) {
                if (slotIdx == 3 || slotIdx == 4) {
                    slotIdx = (slotIdx == 3) ? 4 : 3;
                    if (cert->pkeys[slotIdx].certObj != NULL &&
                        sceSsl_lib_C6ED5C87(cert->pkeys[slotIdx].certObj, pkey) == 0) {
                        sceSsl_lib_C1A2E14B(cert->pkeys[slotIdx].certObj);
                        cert->pkeys[slotIdx].certObj = NULL;
                        return 0;
                    }
                } else {
                    sceSsl_lib_C1A2E14B(cert->pkeys[slotIdx].certObj);
                    cert->pkeys[slotIdx].certObj = NULL;
                    return 0;
                }
            }
        }
    }

    keyType = pkey->type;
    if (keyType == 6) {
        sslRsaSetBlindingMode((SslRsa *)pkey->pkey, rsaBlinding);
    }
    if (cert->pkeys[slotIdx].privateKey != NULL) {
        sceSsl_lib_9307DE23(cert->pkeys[slotIdx].privateKey);
    }
    sslCryptoAddLock(&pkey->references, 1, 10, "ssl/ssl_rsa.c", 286);
    cert->pkeys[slotIdx].privateKey = pkey;
    cert->activeKey = &cert->pkeys[slotIdx];
    cert->valid = 0;
    return 1;
}

/* 0x027B60 */
int LIBSSL_NOINLINE sceSsl_lib_8390B321(Ssl *ssl, SslEvpPkey *pkey)
{
    if (pkey == NULL) {
        sceSsl_lib_9CDCA0D7(20, 202, 35, NULL, 0);
        return 0;
    }
    if (ssl->cert == NULL || ssl->cert == ssl->ctx->cert) {
        SslCert *newCert = sslCertNew();
        if (newCert == NULL) {
            sceSsl_lib_9CDCA0D7(20, 202, 33, NULL, 0);
            return 0;
        }
        if (ssl->cert != NULL) {
            sslCertFree(ssl->cert);
        }
        ssl->cert = newCert;
    }
    return sslSetPrivateKey(ssl->cert, pkey, ssl->rsaBlinding);
}

/* 0x027C20 */
static int LIBSSL_NOINLINE sslSetCert(SslCert *cert, void *certObj)
{
    SslEvpPkey *pubKey = sceSsl_lib_B87FFE37(certObj);
    int slotIdx;

    if (pubKey == NULL) {
        sceSsl_lib_9CDCA0D7(20, 191, 268, NULL, 0);
        return 0;
    }
    slotIdx = sslCertType(certObj, pubKey);
    if (slotIdx < 0) {
        sceSsl_lib_9CDCA0D7(20, 191, 247, NULL, 0);
        return 0;
    }

    if (cert->pkeys[slotIdx].privateKey != NULL) {
        SslEvpPkey *privKey = cert->pkeys[slotIdx].privateKey;
        sslEvpPkeyCopyParameters(pubKey, privKey);
        sceSsl_lib_04E677AF();

        if (privKey->type != 6 || (sslRsaFlags((const SslRsa *)privKey->pkey) & 1) == 0) {
            if (sceSsl_lib_C6ED5C87(certObj, privKey) == 0) {
                if (slotIdx == 3 || slotIdx == 4) {
                    slotIdx = (slotIdx == 3) ? 4 : 3;
                    if (cert->pkeys[slotIdx].privateKey != NULL &&
                        sceSsl_lib_C6ED5C87(certObj, cert->pkeys[slotIdx].privateKey) == 0) {
                        sceSsl_lib_9307DE23(cert->pkeys[slotIdx].privateKey);
                        cert->pkeys[slotIdx].privateKey = NULL;
                    }
                } else {
                    sceSsl_lib_9307DE23(cert->pkeys[slotIdx].privateKey);
                    cert->pkeys[slotIdx].privateKey = NULL;
                }
            }
        }
    }

    if (cert->pkeys[slotIdx].certObj != NULL) {
        sceSsl_lib_C1A2E14B(cert->pkeys[slotIdx].certObj);
    }
    sceSsl_lib_999D7614(certObj);
    cert->pkeys[slotIdx].certObj = certObj;
    cert->activeKey = &cert->pkeys[slotIdx];
    cert->valid = 0;
    return 1;
}
