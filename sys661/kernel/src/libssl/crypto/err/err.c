/*
 * RSA BSAFE SSL-C cryptlib locking, PEM reader, HMAC, and ERR state stack
 * (0x01A694..0x01B523, 0x01CA20..0x01D60F).
 */
#include "libssl_internal.h"

static SslLockCb g_lockingCallback;
static SslAddLockCb g_addLockCallback;
static SslThreadIdCb g_threadIdCallback;

static SslLhash *g_errStringHash;
static SslLhash *g_errStateHash;
static SslErrState g_errFallbackState;

static const char g_errFilePath[] = "crypto/err/err.c";
static const char g_emptyStr[] = "";

static int LIBSSL_NOINLINE sslPemDefaultPasswordCb(void);
static int LIBSSL_NOINLINE sslPemGetEvpCipherInfo(char *headerBuf, SslEvpCipherCtx *cipherCtx);
static int LIBSSL_NOINLINE sslPemLoadIv(char **cursorPtr, u8 *ivOut, int ivLen);
static int LIBSSL_NOINLINE sslPemReadBioRaw(SslBio *bio, char **outName, char **outHeader,
                                            u8 **outData, int *outLen);
static void LIBSSL_NOINLINE sslErrStateFree(SslErrState *state);
static u32 LIBSSL_NOINLINE sslErrGetErrorLineData(int popFlag, const char **fileOut, int *lineOut,
                                                  const char **dataOut, int *flagsOut);
static u32 LIBSSL_NOINLINE sslErrStateHash(const SslErrState *state);
static int LIBSSL_NOINLINE sslErrStateCmp(const SslErrState *lhs, const SslErrState *rhs);

/* ==================== cryptlib.c (0x01A694..0x01A7F7) ==================== */

/* 0x01A694 */
SslLockCb LIBSSL_NOINLINE sceSsl_lib_6BA650E4(void)
{
    return g_lockingCallback;
}

/* 0x01A6A0 */
SslAddLockCb LIBSSL_NOINLINE sceSsl_lib_69105335(void)
{
    return g_addLockCallback;
}

/* 0x01A6AC */
void LIBSSL_NOINLINE sceSsl_lib_A9191C17(SslLockCb cb)
{
    sslX509DisableMontCache();
    g_lockingCallback = cb;
}

/* 0x01A6D8 */
void LIBSSL_NOINLINE sceSsl_lib_64AA4ED6(SslAddLockCb cb)
{
    g_addLockCallback = cb;
}

/* 0x01A6E4 */
SslThreadIdCb LIBSSL_NOINLINE sceSsl_lib_6CDEB823(void)
{
    return g_threadIdCallback;
}

/* 0x01A6F0 */
void LIBSSL_NOINLINE sceSsl_lib_5AD14F0B(SslThreadIdCb cb)
{
    g_threadIdCallback = cb;
}

/* 0x01A6FC */
u32 LIBSSL_NOINLINE sceSsl_lib_9D8A385E(void)
{
    if (g_threadIdCallback != NULL) {
        return g_threadIdCallback();
    }
    return 0;
}

/* 0x01A728 */
void LIBSSL_NOINLINE sceSsl_lib_D84E2411(int mode, int type, const char *file, int line)
{
    if (g_lockingCallback != NULL) {
        g_lockingCallback(mode, type, file, line);
    }
}

/* 0x01A754 */
int LIBSSL_NOINLINE sslCryptoAddLock(int *counterPtr, int amount, int type, const char *file, int line)
{
    int updated;

    if (g_addLockCallback != NULL) {
        return g_addLockCallback(counterPtr, amount, type, file, line);
    }
    sceSsl_lib_D84E2411(9, type, file, line);
    updated = *counterPtr + amount;
    *counterPtr = updated;
    sceSsl_lib_D84E2411(10, type, file, line);
    return updated;
}

/* ==================== pem_lib.c (0x01A7F8..0x01B523) ==================== */

/* 0x01A7F8 */
static int LIBSSL_NOINLINE sslPemDefaultPasswordCb(void)
{
    sceSsl_lib_9CDCA0D7(9, 119, 34, NULL, 0);
    return -1;
}

/* 0x01A828 */
void *LIBSSL_NOINLINE sceSsl_lib_0217E73C(SslPemD2iFn d2iFn, const char *name, SslBio *bio,
                                          void **outObj, void *passCb)
{
    char *pemName = NULL;
    char *pemHeader = NULL;
    u8 *pemData = NULL;
    int pemLen = 0;
    void *resultObj = NULL;

    for (;;) {
        if (sslPemReadBioRaw(bio, &pemName, &pemHeader, &pemData, &pemLen) == 0) {
            return NULL;
        }
        if (sslStrcmp(pemName, name) == 0 ||
            (sslStrcmp(pemName, "RSA PRIVATE KEY") == 0 && sslStrcmp(name, "PRIVATE KEY") == 0) ||
            (sslStrcmp(pemName, "DSA PRIVATE KEY") == 0 && sslStrcmp(name, "PRIVATE KEY") == 0) ||
            (sslStrcmp(pemName, "X509 CERTIFICATE") == 0 && sslStrcmp(name, "CERTIFICATE") == 0) ||
            (sslStrcmp(pemName, "NEW CERTIFICATE REQUEST") == 0 && sslStrcmp(name, "CERTIFICATE REQUEST") == 0)) {
            SslEvpCipherCtx *cipherCtx = sslEvpCipherCtxNew();
            if (cipherCtx != NULL &&
                sslPemGetEvpCipherInfo(pemHeader, cipherCtx) != 0 &&
                sceSsl_lib_BE5FF9F1(cipherCtx, pemData, &pemLen, (SslPemPasswordCb)passCb) != 0) {
                const u8 *derCursor = pemData;
                if (sslStrcmp(name, "PRIVATE KEY") != 0) {
                    resultObj = d2iFn(outObj, &derCursor, pemLen);
                    if (resultObj == NULL) {
                        sceSsl_lib_9CDCA0D7(9, 103, 2061, NULL, 0);
                    }
                } else {
                    int keyType = 0;
                    if (sslStrcmp(pemName, "RSA PRIVATE KEY") == 0) {
                        keyType = 6;
                    } else if (sslStrcmp(pemName, "DSA PRIVATE KEY") == 0) {
                        keyType = 116;
                    }
                    if (keyType != 0) {
                        SslPemD2iPrivateKeyFn privFn = (SslPemD2iPrivateKeyFn)d2iFn;
                        resultObj = privFn(keyType, outObj, &derCursor, pemLen);
                        if (resultObj == NULL) {
                            sceSsl_lib_9CDCA0D7(9, 103, 2061, NULL, 0);
                        }
                    } else {
                        sceSsl_lib_9CDCA0D7(9, 103, 2061, NULL, 0);
                    }
                }
            }
            sceSsl_lib_5E5C873A(pemName);
            sceSsl_lib_5E5C873A(pemHeader);
            sceSsl_lib_5E5C873A(pemData);
            if (cipherCtx != NULL) {
                sslEvpCipherCtxFree(cipherCtx);
            }
            return resultObj;
        }
        sceSsl_lib_5E5C873A(pemName);
        sceSsl_lib_5E5C873A(pemHeader);
        sceSsl_lib_5E5C873A(pemData);
    }
}

/* 0x01AA98 */
int LIBSSL_NOINLINE sceSsl_lib_BE5FF9F1(SslEvpCipherCtx *cipherCtx, u8 *dataBuf,
                                        int *lenPtr, SslPemPasswordCb passCb)
{
    u8 derivedKey[32];
    char passBuf[1024];
    int decLen;
    int finalLen = *lenPtr;
    int passLen;
    int finalOk;
    const SslEvpCipher *cipher = sslEvpCipherCtxCipher(cipherCtx);

    if (cipher == NULL) {
        return 1;
    }
    if (passCb != NULL) {
        passLen = passCb(passBuf, sizeof(passBuf), 0);
    } else {
        passLen = sslPemDefaultPasswordCb();
    }
    if (passLen <= 0) {
        sceSsl_lib_9CDCA0D7(9, 106, 104, NULL, 0);
        return 0;
    }

    if (sslEvpBytesToKey(sslEvpCipherCtxCipher(cipherCtx), sslEvpMd5(),
                         sslEvpCipherCtxIv(cipherCtx), (const u8 *)passBuf,
                         passLen, 1U, derivedKey, NULL) == 0) {
        return 0;
    }
    if (sslEvpDecryptInit(cipherCtx, NULL, derivedKey, NULL) == 0) {
        return 0;
    }
    sslEvpDecryptUpdate(cipherCtx, dataBuf, &decLen, dataBuf, finalLen);
    finalOk = sslEvpDecryptFinal(cipherCtx, dataBuf + decLen, &finalLen);
    sslEvpCipherCtxCleanup(cipherCtx);
    sslMemset(passBuf, 0, sizeof(passBuf));
    sslMemset(derivedKey, 0, sizeof(derivedKey));
    finalLen += decLen;
    if (finalOk != 0) {
        *lenPtr = finalLen;
        return 1;
    }
    sceSsl_lib_9CDCA0D7(9, 106, 101, NULL, 0);
    return 0;
}

/* 0x01AC4C */
static int LIBSSL_NOINLINE sslPemGetEvpCipherInfo(char *headerBuf, SslEvpCipherCtx *cipherCtx)
{
    u8 ivBuf[32];
    char *cursor = headerBuf;
    const SslEvpCipher *cipher;
    char *cipherName;
    char savedChar;
    int errReason = 0;

    sslEvpCipherCtxCleanup(cipherCtx);
    if (cursor == NULL || *cursor == '\0' || *cursor == '\n') {
        return 1;
    }
    if (sslStrncmp(cursor, "Proc-Type: ", 11) != 0) {
        errReason = 107;
    } else {
        cursor += 11;
        if (*cursor != '4') {
            return 0;
        }
        cursor++;
        if (*cursor != ',') {
            return 0;
        }
        cursor++;
        if (sslStrncmp(cursor, "ENCRYPTED", 9) != 0) {
            errReason = 106;
        } else {
            while (*cursor != '\n' && *cursor != '\0') {
                cursor++;
            }
            if (*cursor == '\0') {
                errReason = 112;
            } else {
                cursor++;
                if (sslStrncmp(cursor, "DEK-Info: ", 10) != 0) {
                    errReason = 105;
                } else {
                    cursor += 10;
                    cipherName = cursor;
                    for (;;) {
                        char ch = *cursor;
                        if (!((ch >= 'A' && ch <= 'Z') || ch == '-' || (ch >= '0' && ch <= '9'))) {
                            savedChar = ch;
                            break;
                        }
                        cursor++;
                    }
                    *cursor = '\0';
                    sslObjSnToNid(cipherName);
                    cipher = sslEvpGetCipherByName(cipherName);
                    *cursor++ = savedChar;
                    if (cipher == NULL) {
                        errReason = 114;
                    } else {
                        int ivLen = sslEvpCipherIvLength(cipher);
                        if (sslPemLoadIv(&cursor, ivBuf, ivLen) == 0) {
                            return 0;
                        }
                        return sslEvpDecryptInit(cipherCtx, cipher, NULL, ivBuf);
                    }
                }
            }
        }
    }
    sceSsl_lib_9CDCA0D7(9, 118, errReason, NULL, 0);
    return 0;
}

/* 0x01AE98 */
static int LIBSSL_NOINLINE sslPemLoadIv(char **cursorPtr, u8 *ivOut, int ivLen)
{
    char *cursor = *cursorPtr;
    int idx;
    int nibCount = 2 * ivLen;

    for (idx = 0; idx < ivLen; idx++) {
        ivOut[idx] = 0;
    }
    for (idx = 0; idx < nibCount; idx++) {
        char ch = *cursor;
        u8 nibble;

        if (ch >= '0' && ch <= '9') {
            nibble = (u8)(ch - '0');
        } else if (ch >= 'A' && ch <= 'F') {
            nibble = (u8)(ch - 'A' + 10);
        } else if (ch >= 'a' && ch <= 'f') {
            nibble = (u8)(ch - 'a' + 10);
        } else {
            sceSsl_lib_9CDCA0D7(9, 101, 103, NULL, 0);
            return 0;
        }
        cursor++;
        if ((idx & 1) == 0) {
            ivOut[idx / 2] |= (u8)(nibble << 4);
        } else {
            ivOut[idx / 2] |= nibble;
        }
    }
    *cursorPtr = cursor;
    return 1;
}

/* 0x01AF84 */
static int LIBSSL_NOINLINE sslPemReadBioRaw(SslBio *bio, char **outName, char **outHeader,
                                            u8 **outData, int *outLen)
{
    char lineBuf[256];
    int decodedLen = 0;
    int finalLen = 0;
    int shortLineSeen = 0;
    int endSeenInHeader = 0;
    SslBufMem *nameBuf = sslBufMemNew();
    SslBufMem *headerBuf = sslBufMemNew();
    SslBufMem *dataBuf = sslBufMemNew();
    SslEvpEncodeCtx *decodeCtx = sslEvpEncodeCtxNew();
    int errReason = 33;
    int gotBlock = 0;

    if (nameBuf != NULL && headerBuf != NULL && dataBuf != NULL && decodeCtx != NULL) {
        int nameLen = 0;
        int headerLen = 0;
        int lineLen;

        lineBuf[254] = '\0';
        for (;;) {
            lineLen = sceSsl_lib_AFD1D124(bio, lineBuf, 254);
            if (lineLen <= 0) {
                errReason = 108;
                break;
            }
            if (lineBuf[lineLen] < 33) {
                int trimIdx = lineLen - 1;
                while (trimIdx >= 0 && lineBuf[trimIdx] < 33) {
                    trimIdx--;
                }
                lineLen = trimIdx + 1;
            } else {
                lineLen++;
            }
            lineBuf[lineLen] = '\n';
            lineBuf[lineLen + 1] = '\0';
            if (sslStrncmp(lineBuf, "-----BEGIN ", 11) == 0) {
                nameLen = sslStrlen(&lineBuf[11]);
                if (sslStrncmp(&lineBuf[nameLen + 5], "-----\n", 6) == 0) {
                    gotBlock = 1;
                    break;
                }
            }
        }

        if (gotBlock != 0) {
            gotBlock = 0;
            if (sslBufMemGrow(nameBuf, nameLen + 9) != 0) {
                sslStrncpy(nameBuf->data, &lineBuf[11], nameLen - 6);
                nameBuf->data[nameLen - 6] = '\0';
                if (sslBufMemGrow(headerBuf, 256) != 0) {
                    int growFailed = 0;
                    for (;;) {
                        headerBuf->data[headerLen] = '\0';
                        lineLen = sceSsl_lib_AFD1D124(bio, lineBuf, 254);
                        if (lineLen <= 0) {
                            break;
                        }
                        if (lineBuf[lineLen] < 33) {
                            int trimIdx = lineLen - 1;
                            while (trimIdx >= 0 && lineBuf[trimIdx] < 33) {
                                trimIdx--;
                            }
                            lineLen = trimIdx + 1;
                        } else {
                            lineLen++;
                        }
                        lineBuf[lineLen] = '\n';
                        lineLen++;
                        lineBuf[lineLen] = '\0';
                        if (lineBuf[0] == '\n') {
                            break;
                        }
                        if (sslBufMemGrow(headerBuf, headerLen + lineLen + 9) == 0) {
                            errReason = 33;
                            growFailed = 1;
                            break;
                        }
                        if (sslStrncmp(lineBuf, "-----END ", 9) == 0) {
                            endSeenInHeader = 1;
                            break;
                        }
                        sslStrncpy(headerBuf->data + headerLen, lineBuf, lineLen);
                        headerLen += lineLen;
                    }

                    decodedLen = 0;
                    if (growFailed == 0 && sslBufMemGrow(dataBuf, 1024) != 0) {
                        dataBuf->data[0] = '\0';
                        if (endSeenInHeader != 0) {
                            SslBufMem *swapBuf = headerBuf;
                            decodedLen = headerLen;
                            headerBuf = dataBuf;
                            dataBuf = swapBuf;
                        } else {
                            for (;;) {
                                lineLen = sceSsl_lib_AFD1D124(bio, lineBuf, 254);
                                if (lineLen <= 0) {
                                    break;
                                }
                                if (lineBuf[lineLen] < 33) {
                                    int trimIdx = lineLen - 1;
                                    while (trimIdx >= 0 && lineBuf[trimIdx] < 33) {
                                        trimIdx--;
                                    }
                                    lineLen = trimIdx + 1;
                                } else {
                                    lineLen++;
                                }
                                lineBuf[lineLen] = '\n';
                                lineLen++;
                                lineBuf[lineLen] = '\0';
                                if (lineLen != 65) {
                                    shortLineSeen = 1;
                                }
                                if (sslStrncmp(lineBuf, "-----END ", 9) == 0 || lineLen >= 66) {
                                    break;
                                }
                                if (sslBufMemGrow(dataBuf, decodedLen + lineLen + 9) == 0) {
                                    errReason = 33;
                                    growFailed = 1;
                                    break;
                                }
                                sslStrncpy(dataBuf->data + decodedLen, lineBuf, lineLen);
                                dataBuf->data[decodedLen + lineLen] = '\0';
                                decodedLen += lineLen;
                                if (shortLineSeen != 0) {
                                    lineBuf[0] = '\0';
                                    lineLen = sceSsl_lib_AFD1D124(bio, lineBuf, 254);
                                    if (lineLen > 0) {
                                        if (lineBuf[lineLen] < 33) {
                                            int trimIdx = lineLen - 1;
                                            while (trimIdx >= 0 && lineBuf[trimIdx] < 33) {
                                                trimIdx--;
                                            }
                                            lineLen = trimIdx + 1;
                                        } else {
                                            lineLen++;
                                        }
                                        lineBuf[lineLen] = '\n';
                                        lineBuf[lineLen + 1] = '\0';
                                    }
                                    break;
                                }
                            }
                        }

                        if (growFailed == 0) {
                            nameLen = sslStrlen(nameBuf->data);
                            if (sslStrncmp(lineBuf, "-----END ", 9) != 0 ||
                                sslStrncmp(nameBuf->data, &lineBuf[9], nameLen) != 0 ||
                                sslStrncmp(&lineBuf[9 + nameLen], "-----\n", 6) != 0) {
                                errReason = 102;
                            } else {
                                sslEvpDecodeInit(decodeCtx);
                                if (sslEvpDecodeUpdate(decodeCtx, (u8 *)dataBuf->data, &decodedLen,
                                                       (const u8 *)dataBuf->data, decodedLen) < 0 ||
                                    sslEvpDecodeFinal(decodeCtx, (u8 *)dataBuf->data + decodedLen, &finalLen) < 0) {
                                    errReason = 100;
                                } else {
                                    decodedLen += finalLen;
                                    if (decodedLen != 0) {
                                        *outLen = decodedLen;
                                        *outName = nameBuf->data;
                                        *outHeader = headerBuf->data;
                                        *outData = (u8 *)dataBuf->data;
                                        sceSsl_lib_5E5C873A(nameBuf);
                                        sceSsl_lib_5E5C873A(headerBuf);
                                        sceSsl_lib_5E5C873A(dataBuf);
                                        sslEvpEncodeCtxFree(decodeCtx);
                                        return 1;
                                    }
                                    errReason = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (errReason != 0) {
        sceSsl_lib_9CDCA0D7(9, 109, errReason, NULL, 0);
    }
    if (decodeCtx != NULL) {
        sslEvpEncodeCtxFree(decodeCtx);
    }
    if (nameBuf != NULL) {
        sslBufMemFree(nameBuf);
        sslBufMemFree(headerBuf);
    }
    if (dataBuf != NULL) {
        sslBufMemFree(dataBuf);
    }
    return 0;
}

/* ==================== hmac.c (0x01CA20..0x01CE77) ==================== */

/* 0x01CA20 */
SslHmacCtx *LIBSSL_NOINLINE sslHmacCtxNew(void)
{
    SslHmacCtx *ctx = (SslHmacCtx *)sceSsl_lib_0861D934(sizeof(SslHmacCtx));
    if (ctx == NULL) {
        return NULL;
    }
    sslMemset(ctx, 0, sizeof(SslHmacCtx));
    ctx->mdCtx = sslEvpMdCtxNew();
    if (ctx->mdCtx != NULL) {
        ctx->innerCtx = sslEvpMdCtxNew();
        if (ctx->innerCtx != NULL) {
            ctx->outerCtx = sslEvpMdCtxNew();
            if (ctx->outerCtx != NULL) {
                return ctx;
            }
        }
    }
    if (ctx->mdCtx != NULL) {
        sslEvpMdCtxFree(ctx->mdCtx);
    }
    if (ctx->innerCtx != NULL) {
        sslEvpMdCtxFree(ctx->innerCtx);
    }
    if (ctx->outerCtx != NULL) {
        sslEvpMdCtxFree(ctx->outerCtx);
    }
    sceSsl_lib_9CDCA0D7(24, 100, 33, NULL, 0);
    return NULL;
}

/* 0x01CAF0 */
void LIBSSL_NOINLINE sslHmacCtxFree(SslHmacCtx *ctx)
{
    if (ctx->mdCtx != NULL) {
        sslEvpMdCtxFree(ctx->mdCtx);
    }
    if (ctx->innerCtx != NULL) {
        sslEvpMdCtxFree(ctx->innerCtx);
    }
    if (ctx->outerCtx != NULL) {
        sslEvpMdCtxFree(ctx->outerCtx);
    }
    sslMemset(ctx, 0, sizeof(SslHmacCtx));
    sceSsl_lib_5E5C873A(ctx);
}

/* 0x01CB64 */
const SslEvpMd *LIBSSL_NOINLINE sslHmacCtxMd(const SslHmacCtx *ctx)
{
    return ctx->md;
}

/* 0x01CB6C */
int LIBSSL_NOINLINE sslHmacCtxCopy(SslHmacCtx *dst, const SslHmacCtx *src)
{
    int wordIdx;

    dst->md = src->md;
    if (sslEvpMdCtxCopy(dst->mdCtx, src->mdCtx) == 0) {
        return 0;
    }
    if (sslEvpMdCtxCopy(dst->innerCtx, src->innerCtx) == 0) {
        return 0;
    }
    if (sslEvpMdCtxCopy(dst->outerCtx, src->outerCtx) == 0) {
        return 0;
    }
    dst->keyBlockFlags = src->keyBlockFlags;
    dst->keyLength = src->keyLength;
    for (wordIdx = 0; wordIdx < 64; wordIdx++) {
        dst->key[wordIdx] = src->key[wordIdx];
    }
    return 1;
}

/* 0x01CC2C */
int LIBSSL_NOINLINE sslHmacInit(SslHmacCtx *ctx, const void *key, int keyLen, const SslEvpMd *md)
{
    u8 padBuf[64];
    int reset = 0;
    int idx;

    if (md != NULL) {
        reset = 1;
        ctx->md = md;
    } else {
        md = ctx->md;
    }

    if (key != NULL) {
        reset = 1;
        if (sslEvpMdBlockSize(md) < keyLen) {
            if (sslEvpDigestInit(ctx->mdCtx, md) == 0) {
                return 0;
            }
            sslEvpDigestUpdate(ctx->mdCtx, key, (u32)keyLen);
            sslEvpDigestFinal(ctx->mdCtx, ctx->key, &ctx->keyLength);
        } else {
            sslMemcpy(ctx->key, key, (u32)keyLen);
            sslMemset(ctx->key + keyLen, 0, (u32)(64 - keyLen));
            ctx->keyLength = (u32)keyLen;
        }
    }

    if (reset != 0) {
        for (idx = 0; idx < 64; idx++) {
            padBuf[idx] = (u8)(ctx->key[idx] ^ 0x36U);
        }
        if (sslEvpDigestInit(ctx->innerCtx, md) == 0) {
            return 0;
        }
        sslEvpDigestUpdate(ctx->innerCtx, padBuf, (u32)sslEvpMdBlockSize(md));

        for (idx = 0; idx < 64; idx++) {
            padBuf[idx] = (u8)(ctx->key[idx] ^ 0x5CU);
        }
        sslEvpDigestInit(ctx->outerCtx, md);
        sslEvpDigestUpdate(ctx->outerCtx, padBuf, (u32)sslEvpMdBlockSize(md));
    }

    return (sslEvpMdCtxCopy(ctx->mdCtx, ctx->innerCtx) != 0) ? 1 : 0;
}

/* 0x01CDD0 */
void LIBSSL_NOINLINE sslHmacUpdate(SslHmacCtx *ctx, const void *data, u32 len)
{
    sslEvpDigestUpdate(ctx->mdCtx, data, len);
}

/* 0x01CDEC */
int LIBSSL_NOINLINE sslHmacFinal(SslHmacCtx *ctx, u8 *outMd, u32 *outLen)
{
    u8 innerDigest[32];
    u32 innerLen;

    sslEvpMdBlockSize(ctx->md);
    sslEvpDigestFinal(ctx->mdCtx, innerDigest, &innerLen);
    if (sslEvpMdCtxCopy(ctx->mdCtx, ctx->outerCtx) == 0) {
        return 0;
    }
    sslEvpDigestUpdate(ctx->mdCtx, innerDigest, innerLen);
    sslEvpDigestFinal(ctx->mdCtx, outMd, outLen);
    return 1;
}

/* ==================== err.c (0x01CE78..0x01D60F) ==================== */

/* 0x01CE78 */
static void LIBSSL_NOINLINE sslErrStateFree(SslErrState *state)
{
    int idx;

    for (idx = 0; idx < 16; idx++) {
        if (state->errData[idx] != NULL) {
            if ((state->errDataFlags[idx] & 1) != 0) {
                sceSsl_lib_5E5C873A(state->errData[idx]);
                state->errData[idx] = NULL;
            }
        }
        state->errDataFlags[idx] = 0;
    }
    sceSsl_lib_5E5C873A(state);
}

/* 0x01CEEC */
void LIBSSL_NOINLINE sceSsl_lib_101D418A(void)
{
    sceSsl_lib_D84E2411(9, 1, g_errFilePath, 257);
    if (g_errStringHash != NULL) {
        sslLhFree(g_errStringHash);
        g_errStringHash = NULL;
    }
    sceSsl_lib_D84E2411(10, 1, g_errFilePath, 265);
}

/* 0x01CF5C */
void LIBSSL_NOINLINE sceSsl_lib_9CDCA0D7(int lib, int func, int reason, const char *file, int line)
{
    SslErrState *state = sslErrGetState();
    int topIdx = (state->top + 1) % 16;
    u32 packedErr = ((u32)lib << 24) | (((u32)func & 0xFFFU) << 12) | ((u32)reason & 0xFFFU);

    state->top = topIdx;
    if (topIdx == state->bottom) {
        state->bottom = (topIdx + 1) % 16;
    }
    state->errBuffer[topIdx] = packedErr;
    state->errFile[topIdx] = file;
    state->errLine[topIdx] = line;
    if (state->errData[topIdx] != NULL && (state->errDataFlags[topIdx] & 1) != 0) {
        sceSsl_lib_5E5C873A(state->errData[topIdx]);
        state->errData[topIdx] = NULL;
    }
    state->errDataFlags[state->top] = 0;
}

/* 0x01D070 */
void LIBSSL_NOINLINE sceSsl_lib_04E677AF(void)
{
    SslErrState *state = sslErrGetState();

    state->top = 0;
    state->bottom = 0;
}

/* 0x01D094 */
u32 LIBSSL_NOINLINE sceSsl_lib_E78AD94D(void)
{
    return sslErrGetErrorLineData(1, NULL, NULL, NULL, NULL);
}

/* 0x01D0C0 */
u32 LIBSSL_NOINLINE sceSsl_lib_47F7B97F(void)
{
    return sslErrGetErrorLineData(0, NULL, NULL, NULL, NULL);
}

/* 0x01D0EC */
static u32 LIBSSL_NOINLINE sslErrGetErrorLineData(int popFlag, const char **fileOut, int *lineOut,
                                                  const char **dataOut, int *flagsOut)
{
    SslErrState *state = sslErrGetState();
    int slotIdx;
    u32 errCode;

    if (state->bottom == state->top) {
        return 0;
    }
    slotIdx = (state->bottom + 1) % 16;
    errCode = state->errBuffer[slotIdx];
    if (popFlag != 0) {
        state->bottom = slotIdx;
        state->errBuffer[slotIdx] = 0;
    }
    if (fileOut != NULL && lineOut != NULL) {
        if (state->errFile[slotIdx] != NULL) {
            *fileOut = state->errFile[slotIdx];
            *lineOut = state->errLine[slotIdx];
        } else {
            *fileOut = "NA";
            *lineOut = 0;
        }
    }
    if (dataOut != NULL) {
        if (state->errData[slotIdx] != NULL) {
            *dataOut = state->errData[slotIdx];
            if (flagsOut != NULL) {
                *flagsOut = state->errDataFlags[slotIdx];
            }
        } else {
            *dataOut = g_emptyStr;
            if (flagsOut != NULL) {
                *flagsOut = 0;
            }
        }
    }
    return errCode;
}

/* 0x01D210 */
void LIBSSL_NOINLINE sceSsl_lib_D8C05497(u32 pid)
{
    SslErrState keyState;
    SslErrState *removed;

    if (g_errStateHash == NULL) {
        return;
    }
    if (pid == 0) {
        pid = sceSsl_lib_9D8A385E();
    }
    keyState.pid = pid;
    sceSsl_lib_D84E2411(9, 1, g_errFilePath, 536);
    removed = (SslErrState *)sslLhDelete(g_errStateHash, &keyState);
    if (g_errStateHash->numItems == 0) {
        sslLhFree(g_errStateHash);
        g_errStateHash = NULL;
    }
    sceSsl_lib_D84E2411(10, 1, g_errFilePath, 545);
    if (removed != NULL) {
        sslErrStateFree(removed);
    }
}

/* 0x01D2D0 */
static u32 LIBSSL_NOINLINE sslErrStateHash(const SslErrState *state)
{
    return state->pid * 13U;
}

/* 0x01D2E8 */
static int LIBSSL_NOINLINE sslErrStateCmp(const SslErrState *lhs, const SslErrState *rhs)
{
    return (int)(lhs->pid - rhs->pid);
}

/* 0x01D2F8 */
SslErrState *LIBSSL_NOINLINE sslErrGetState(void)
{
    SslErrState keyState;
    SslErrState *state;
    SslErrState *replaced;
    u32 pid = sceSsl_lib_9D8A385E();
    int idx;

    sceSsl_lib_D84E2411(5, 1, g_errFilePath, 573);
    if (g_errStateHash != NULL) {
        keyState.pid = pid;
        state = (SslErrState *)sslLhRetrieve(g_errStateHash, &keyState);
        sceSsl_lib_D84E2411(6, 1, g_errFilePath, 591);
        if (state != NULL) {
            return state;
        }
    } else {
        sceSsl_lib_D84E2411(6, 1, g_errFilePath, 576);
        sceSsl_lib_D84E2411(9, 1, g_errFilePath, 577);
        if (g_errStateHash != NULL) {
            sceSsl_lib_D84E2411(10, 1, g_errFilePath, 585);
        } else {
            g_errStateHash = sslLhNew((SslLhHashFn)sslErrStateHash, (SslLhCmpFn)sslErrStateCmp);
            sceSsl_lib_D84E2411(10, 1, g_errFilePath, 581);
            if (g_errStateHash == NULL) {
                return &g_errFallbackState;
            }
        }
    }

    state = (SslErrState *)sceSsl_lib_0861D934(sizeof(SslErrState));
    if (state == NULL) {
        return &g_errFallbackState;
    }
    state->pid = pid;
    state->top = 0;
    state->bottom = 0;
    for (idx = 0; idx < 16; idx++) {
        state->errData[idx] = NULL;
        state->errDataFlags[idx] = 0;
    }
    sceSsl_lib_D84E2411(9, 1, g_errFilePath, 613);
    replaced = (SslErrState *)sslLhInsert(g_errStateHash, state);
    sceSsl_lib_D84E2411(10, 1, g_errFilePath, 615);
    if (replaced != NULL) {
        sslErrStateFree(replaced);
    }
    return state;
}

/* 0x01D4B0 */
void LIBSSL_NOINLINE sslErrSetErrorData(char *dataStr, int dataFlags)
{
    SslErrState *state = sslErrGetState();
    int topIdx = state->top;
    int slotIdx = (topIdx != 0) ? topIdx : 15;

    state->errData[slotIdx] = dataStr;
    state->errDataFlags[topIdx] = dataFlags;
}

/* 0x01D504 */
void LIBSSL_NOINLINE sslErrAddErrorData(int numStrings, ...)
{
    va_list args;
    int cap = 64;
    int totalLen = 0;
    int idx;
    char *msgBuf = (char *)sceSsl_lib_0861D934(65U);

    if (msgBuf == NULL) {
        return;
    }
    msgBuf[0] = '\0';

    va_start(args, numStrings);
    for (idx = 0; idx < numStrings; idx++) {
        const char *piece = va_arg(args, const char *);
        if (piece != NULL) {
            totalLen += sslStrlen(piece);
            if (cap < totalLen) {
                char *grown;
                cap = totalLen + 20;
                grown = (char *)sslCryptoRealloc(msgBuf, (u32)(totalLen + 21));
                if (grown == NULL) {
                    va_end(args);
                    sceSsl_lib_5E5C873A(msgBuf);
                    return;
                }
                msgBuf = grown;
            }
            sslStrcat(msgBuf, piece);
        }
    }
    va_end(args);
    sslErrSetErrorData(msgBuf, 3);
}
