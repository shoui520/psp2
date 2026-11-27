/*
 * SceSsl / sceSsl_Module (ePSP 6.61) - RAND, RSA, STACK, and RSA public key decode.
 *
 * Implements functions [326..374] (0x0140f0..0x016e6b):
 *   - crypto/rand/md_rand.c & rand_lib.c ([326..334])
 *   - crypto/rsa/rsa_eay.c, rsa_lib.c, rsa_none.c, rsa_pk1.c, rsa_sign.c, rsa_ssl.c ([335..364])
 *   - crypto/stack/stack.c ([365..372])
 *   - RSA SubjectPublicKeyInfo decode from RSA-COMMON ASN1_ITEM ([373..374])
 */
#include "../../libssl_internal.h"

/* -------------------------------------------------------------------------
 * RAND state & method (0x030454..0x030464, 0x0316dc..0x0317a3)
 * ------------------------------------------------------------------------- */

static int sslMdRandAdd(const void *buf, int num);
static int sslMdRandBytes(u8 *outBuf, int num);
static void sslMdRandCleanup(void);

static const SslRandMethod g_randSsleayMethod = {
    sslMdRandAdd,
    sslMdRandBytes,
    sslMdRandCleanup
};

static int g_randInitialized = 1;
static int g_randStateNum = 0;
static int g_randStateIndex = 0;
static u32 g_randMdCount[2] = { 0, 0 };
static u8 g_randState[143];
static u8 g_randMd[16];

static int g_randMethodLocked = 0;
static const SslRandMethod *g_randDefaultMethod = NULL;

/* -------------------------------------------------------------------------
 * RSA globals & method (0x030468..0x030493)
 * ------------------------------------------------------------------------- */

static int sslRsaEayPublicEncrypt(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
static int sslRsaEayPublicDecrypt(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
static int sslRsaEayPrivateEncrypt(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
static int sslRsaEayPrivateDecrypt(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding);
static int sslRsaEayModExp(SslBignum *out, const SslBignum *base, SslRsa *rsa);
static int sslRsaEayInit(SslRsa *rsa);
static int sslRsaEayFinish(SslRsa *rsa);

static const SslRsaMethod g_rsaPkcs1SsleayMethod = {
    "Eric Young's PKCS#1 RSA",
    sslRsaEayPublicEncrypt,
    sslRsaEayPublicDecrypt,
    sslRsaEayPrivateEncrypt,
    sslRsaEayPrivateDecrypt,
    sslRsaEayModExp,
    sslBnModExpMont,
    sslRsaEayInit,
    sslRsaEayFinish,
    0,
    NULL
};

static const SslRsaMethod *g_rsaDefaultMethod = NULL;
static SslStack *g_rsaExDataFuncs = NULL;

/* rsaEncryption OID (1.2.840.113549.1.1.1) and SubjectPublicKeyInfo FieldSpec */
static const u8 g_rsaEncryptionOid[9] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01
};

static const u8 g_navSpkiAlgOid[] = { 0x80, 0xc0, 0x00 };
static const u8 g_navSpkiAlgParam[] = { 0x80, 0xc1, 0x00 };
static const u8 g_navSpkiPubKey[] = { 0xc1, 0x00 };

static SslAsn1FieldSpec g_rsaSpkiSpecs[3] = {
    { 41, 32, g_navSpkiAlgOid, 0x40 },
    { 41, 43, g_navSpkiAlgParam, 0x00 },
    { 41, 44, g_navSpkiPubKey, 0x08 }
};

/* -------------------------------------------------------------------------
 * crypto/rand/md_rand.c & rand_lib.c [326..334]
 * ------------------------------------------------------------------------- */

/* [326] 0x0140f0 */
LIBSSL_NOINLINE const SslRandMethod *sceSsl_lib_A1C3AB31(void)
{
    return &g_randSsleayMethod;
}

/* [327] 0x0140fc */
LIBSSL_NOINLINE static void sslMdRandCleanup(void)
{
    sslMemset(g_randState, 0, sizeof(g_randState));
    g_randStateNum = 0;
    g_randStateIndex = 0;
    sslMemset(g_randMd, 0, sizeof(g_randMd));
    g_randMdCount[0] = 0;
    g_randMdCount[1] = 0;
}

/* [328] 0x014158 */
LIBSSL_NOINLINE static int sslMdRandAdd(const void *buf, int num)
{
    const u8 *inBytes = (const u8 *)buf;
    SceSslMd5Ctx mdCtx;
    int stIdx;
    int nextIdx;
    int offset;

    sceSsl_lib_D84E2411(9, 16, "crypto/rand/md_rand.c", 144);
    stIdx = g_randStateIndex;
    nextIdx = stIdx + num;
    g_randStateIndex = nextIdx;
    if (nextIdx < 127) {
        if (g_randStateNum < 127 && g_randStateNum < nextIdx) {
            g_randStateNum = nextIdx;
        }
    } else {
        g_randStateNum = 127;
        g_randStateIndex = nextIdx % 127;
    }
    sceSsl_lib_D84E2411(10, 16, "crypto/rand/md_rand.c", 159);

    for (offset = 0; offset < num; offset += 16) {
        int chunk = num - offset;
        int idx;
        if (chunk > 16) {
            chunk = 16;
        }

        sceKernelUtilsMd5BlockInit(&mdCtx);
        sceKernelUtilsMd5BlockUpdate(&mdCtx, g_randMd, 16);
        if (stIdx + chunk - 127 > 0) {
            sceKernelUtilsMd5BlockUpdate(&mdCtx, &g_randState[stIdx], (u32)(127 - stIdx));
            sceKernelUtilsMd5BlockUpdate(&mdCtx, g_randState, (u32)(stIdx + chunk - 127));
        } else {
            sceKernelUtilsMd5BlockUpdate(&mdCtx, &g_randState[stIdx], (u32)chunk);
        }
        sceKernelUtilsMd5BlockUpdate(&mdCtx, (u8 *)inBytes, (u32)chunk);
        sceKernelUtilsMd5BlockUpdate(&mdCtx, (u8 *)g_randMdCount, 8);
        sslEvpMd5Final(g_randMd, &mdCtx);

        inBytes += chunk;
        g_randMdCount[1]++;

        for (idx = 0; idx < chunk; idx++) {
            g_randState[stIdx] ^= g_randMd[idx];
            stIdx++;
            if (stIdx >= 127) {
                stIdx = 0;
            }
        }
    }

    sslMemset(&mdCtx, 0, sizeof(mdCtx));
    return 1;
}

/* [329] 0x01438c */
LIBSSL_NOINLINE static int sslMdRandBytes(u8 *outBuf, int num)
{
    SceSslMd5Ctx mdCtx;
    u8 timeBuf[16];
    SceSslTimeSpec nowTime;
    int stIdx;
    int stNum;
    int remain = num;

    sceSsl_lib_D84E2411(9, 16, "crypto/rand/md_rand.c", 232);
    if (g_randInitialized != 0) {
        g_randInitialized = 0;
        sceSsl_lib_D84E2411(10, 16, "crypto/rand/md_rand.c", 237);
        sslMdRandAdd(&mdCtx, (int)sizeof(mdCtx));
        sceSsl_lib_A7BED83B(&nowTime);
        sceSsl_lib_9EF66756(&nowTime, 3, (char *)timeBuf, 8);
        sslMdRandAdd(timeBuf, 8);
        sceSsl_lib_D84E2411(9, 16, "crypto/rand/md_rand.c", 286);
    }

    stIdx = g_randStateIndex;
    stNum = g_randStateNum;
    g_randStateIndex += remain;
    if (g_randStateIndex > stNum) {
        g_randStateIndex %= stNum;
    }
    sceSsl_lib_D84E2411(10, 16, "crypto/rand/md_rand.c", 295);

    while (remain > 0) {
        int chunk = (remain > 8) ? 8 : remain;
        int idx;

        sceKernelUtilsMd5BlockInit(&mdCtx);
        sceKernelUtilsMd5BlockUpdate(&mdCtx, &g_randMd[8], 8);
        sceKernelUtilsMd5BlockUpdate(&mdCtx, (u8 *)g_randMdCount, 8);
        sceKernelUtilsMd5BlockUpdate(&mdCtx, outBuf, (u32)chunk);
        remain -= chunk;

        if (stIdx + chunk - stNum > 0) {
            sceKernelUtilsMd5BlockUpdate(&mdCtx, &g_randState[stIdx], (u32)(stNum - stIdx));
            sceKernelUtilsMd5BlockUpdate(&mdCtx, g_randState, (u32)(stIdx + chunk - stNum));
        } else {
            sceKernelUtilsMd5BlockUpdate(&mdCtx, &g_randState[stIdx], (u32)chunk);
        }
        sslEvpMd5Final(g_randMd, &mdCtx);

        for (idx = 0; idx < chunk; idx++) {
            if (stIdx >= stNum) {
                stIdx = 0;
            }
            g_randState[stIdx++] ^= g_randMd[idx];
            *outBuf++ = g_randMd[idx + 8];
        }
    }

    sceKernelUtilsMd5BlockInit(&mdCtx);
    sceKernelUtilsMd5BlockUpdate(&mdCtx, (u8 *)g_randMdCount, 8);
    g_randMdCount[0]++;
    sceKernelUtilsMd5BlockUpdate(&mdCtx, g_randMd, 16);
    sslEvpMd5Final(g_randMd, &mdCtx);
    sslMemset(&mdCtx, 0, sizeof(mdCtx));
    return 1;
}

/* [330] 0x014630 */
LIBSSL_NOINLINE void sceSsl_lib_1C2728A5(const SslRandMethod *meth)
{
    g_randMethodLocked = 1;
    g_randDefaultMethod = meth;
}

/* [331] 0x014648 */
LIBSSL_NOINLINE const SslRandMethod *sceSsl_lib_4AE72675(void)
{
    if (g_randMethodLocked == 0 && g_randDefaultMethod == NULL) {
        sceSsl_lib_1C2728A5(sceSsl_lib_A1C3AB31());
    }
    return g_randDefaultMethod;
}

/* [332] 0x014690 */
LIBSSL_NOINLINE void sceSsl_lib_56E19CEB(void)
{
    if (g_randMethodLocked == 0 && g_randDefaultMethod == NULL) {
        sceSsl_lib_1C2728A5(sceSsl_lib_A1C3AB31());
    }
    if (g_randDefaultMethod != NULL && g_randDefaultMethod->cleanup != NULL) {
        g_randDefaultMethod->cleanup();
    }
}

/* [333] 0x0146f4 */
LIBSSL_NOINLINE void sceSsl_lib_DEA6EDB2(const void *buf, int num)
{
    if (g_randMethodLocked == 0 && g_randDefaultMethod == NULL) {
        sceSsl_lib_1C2728A5(sceSsl_lib_A1C3AB31());
    }
    if (g_randDefaultMethod != NULL && g_randDefaultMethod->seed != NULL) {
        g_randDefaultMethod->seed(buf, num);
    }
}

/* [334] 0x014774 */
LIBSSL_NOINLINE int sceSsl_lib_21F2A35C(u8 *outBuf, int num)
{
    if (g_randMethodLocked == 0 && g_randDefaultMethod == NULL) {
        sceSsl_lib_1C2728A5(sceSsl_lib_A1C3AB31());
    }
    if (g_randDefaultMethod != NULL) {
        return g_randDefaultMethod->bytes(outBuf, num);
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * crypto/rsa/rsa_eay.c [335..342]
 * ------------------------------------------------------------------------- */

/* [335] 0x0147ec */
LIBSSL_NOINLINE const SslRsaMethod *sslRsaPkcs1Ssleay(void)
{
    return &g_rsaPkcs1SsleayMethod;
}

/* [336] 0x0147f8 */
LIBSSL_NOINLINE static int sslRsaEayPublicEncrypt(int flen, const u8 *from, u8 *to,
                                                  SslRsa *rsa, int padding)
{
    SslBignum inputBn;
    SslBignum resultBn;
    SslBnCtx *ctx;
    u8 *buf = NULL;
    int modLen = 0;
    int ret = -1;
    int padOk = 0;

    sslBnInit(&inputBn);
    sslBnInit(&resultBn);
    ctx = sslBnCtxNew();
    if (ctx != NULL) {
        if (rsa->bnCb != NULL) {
            rsa->bnCb(rsa->bnCbArg, &ctx->flags, 1);
        }
        modLen = (sslBnNumBits(rsa->modN) + 7) / 8;
        buf = (u8 *)sceSsl_lib_0861D934((u32)modLen);
        if (buf == NULL) {
            sceSsl_lib_9CDCA0D7(4, 103, 33, NULL, 0);
        } else {
            switch (padding) {
            case 1:
                padOk = sslRsaPaddingAddPkcs1TypeTwo(buf, modLen, from, flen);
                break;
            case 2:
                padOk = sslRsaPaddingAddSslv23(buf, modLen, from, flen);
                break;
            case 3:
                padOk = sslRsaPaddingAddNone(buf, modLen, from, flen);
                break;
            default:
                sceSsl_lib_9CDCA0D7(4, 103, 118, NULL, 0);
                padOk = 0;
                break;
            }

            if (padOk > 0 && sslBnBinToBn(buf, modLen, &inputBn) != NULL) {
                int montOk = 1;
                if (sslMontCacheEnabled() && rsa->montModN == NULL && (rsa->flags & 2) != 0) {
                    rsa->montModN = sslBnMontCtxNew();
                    if (rsa->montModN != NULL && !sslBnMontCtxSet(rsa->montModN, rsa->modN, ctx)) {
                        montOk = 0;
                    }
                }
                if (montOk &&
                    rsa->meth->bnModExp(&resultBn, &inputBn, rsa->pubE, rsa->modN, ctx, rsa->montModN) != 0) {
                    int outBytes = (sslBnNumBits(&resultBn) + 7) / 8;
                    if (outBytes > (sslBnNumBits(rsa->modN) + 7) / 8) {
                        sceSsl_lib_9CDCA0D7(4, 103, 42, NULL, 0);
                    } else {
                        int written = sslBnBnToBin(&resultBn, to + modLen - outBytes);
                        int zeroIdx;
                        for (zeroIdx = 0; zeroIdx < modLen - written; zeroIdx++) {
                            to[zeroIdx] = 0;
                        }
                        ret = modLen;
                    }
                }
            }
        }
        if (rsa->bnCb != NULL) {
            rsa->bnCb(rsa->bnCbArg, &ctx->flags, 0);
        }
        sslBnCtxFree(ctx);
    }

    sslBnClearFree(&inputBn);
    sslBnClearFree(&resultBn);
    if (buf != NULL) {
        sslMemset(buf, 0, (u32)modLen);
        sceSsl_lib_5E5C873A(buf);
    }
    return ret;
}

/* [337] 0x014b0c */
LIBSSL_NOINLINE static int sslRsaEayPrivateEncrypt(int flen, const u8 *from, u8 *to,
                                                   SslRsa *rsa, int padding)
{
    SslBignum inputBn;
    SslBignum resultBn;
    SslBnBlinding localBlind;
    SslBnCtx *ctx;
    u8 *buf = NULL;
    int modLen = 0;
    int ret = -1;
    int padOk = 0;
    int ok = 1;

    sslMemset(&localBlind, 0, sizeof(localBlind));
    sslBnInit(&inputBn);
    sslBnInit(&resultBn);
    ctx = sslBnCtxNew();
    if (ctx != NULL) {
        modLen = (sslBnNumBits(rsa->modN) + 7) / 8;
        buf = (u8 *)sceSsl_lib_0861D934((u32)modLen);
        if (buf == NULL) {
            sceSsl_lib_9CDCA0D7(4, 101, 33, NULL, 0);
            ok = 0;
        } else {
            if (padding == 1) {
                padOk = sslRsaPaddingAddPkcs1TypeOne(buf, modLen, from, flen);
            } else if (padding == 3) {
                padOk = sslRsaPaddingAddNone(buf, modLen, from, flen);
            } else {
                sceSsl_lib_9CDCA0D7(4, 101, 118, NULL, 0);
                ok = 0;
            }
        }

        if (ok && padOk > 0 && sslBnBinToBn(buf, modLen, &inputBn) != NULL) {
            if (rsa->blindingMode > 0) {
                if (rsa->blinding == NULL) {
                    sceSsl_lib_D84E2411(9, 9, "crypto/rsa/rsa_eay.c", 248);
                    sslRsaBlindingOn(rsa, ctx);
                    sceSsl_lib_D84E2411(10, 9, "crypto/rsa/rsa_eay.c", 250);
                }
                if (rsa->blindingMode > 0) {
                    sceSsl_lib_D84E2411(9, 9, "crypto/rsa/rsa_eay.c", 254);
                    if (!sslBnBlindingCopy(&localBlind, rsa->blinding)) {
                        sceSsl_lib_D84E2411(10, 9, "crypto/rsa/rsa_eay.c", 327);
                        ok = 0;
                    } else if (rsa->blindingMode == 2) {
                        sslBnBlindingFree(rsa->blinding, 1);
                        rsa->blinding = NULL;
                        sceSsl_lib_D84E2411(10, 9, "crypto/rsa/rsa_eay.c", 271);
                    } else if (rsa->blindingMode == 3 && !sslBnBlindingUpdate(rsa->blinding, ctx)) {
                        sceSsl_lib_D84E2411(10, 9, "crypto/rsa/rsa_eay.c", 327);
                        ok = 0;
                    } else {
                        sceSsl_lib_D84E2411(10, 9, "crypto/rsa/rsa_eay.c", 271);
                    }
                    if (ok && !sslBnBlindingConvert(&inputBn, &localBlind, ctx)) {
                        ok = 0;
                    }
                }
            }

            if (ok) {
                if (rsa->primeP != NULL && rsa->primeQ != NULL && rsa->expDmp != NULL &&
                    rsa->expDmq != NULL && rsa->coeffIqmp != NULL) {
                    if (!rsa->meth->rsaModExp(&resultBn, &inputBn, rsa)) {
                        ok = 0;
                    }
                } else {
                    if (rsa->bnCb != NULL) {
                        rsa->bnCb(rsa->bnCbArg, &ctx->flags, 1);
                    }
                    if (!rsa->meth->bnModExp(&resultBn, &inputBn, rsa->privD, rsa->modN, ctx, rsa->montModN)) {
                        ok = 0;
                    } else if (rsa->bnCb != NULL) {
                        rsa->bnCb(rsa->bnCbArg, &ctx->flags, 0);
                    }
                }
            }

            if (ok && (rsa->blindingMode <= 0 || sslBnBlindingInvert(&resultBn, &localBlind, ctx) != 0)) {
                int outBytes = (sslBnNumBits(&resultBn) + 7) / 8;
                if (outBytes > (sslBnNumBits(rsa->modN) + 7) / 8) {
                    sceSsl_lib_9CDCA0D7(4, 101, 42, NULL, 0);
                } else {
                    int zeroCount = modLen - sslBnBnToBin(&resultBn, to + modLen - outBytes);
                    int idx;
                    for (idx = 0; idx < zeroCount; idx++) {
                        to[idx] = 0;
                    }
                    ret = modLen;
                }
            }
        }
    }

    sslBnBlindingFree(&localBlind, 0);
    if (ctx != NULL) {
        sslBnCtxFree(ctx);
    }
    sslBnClearFree(&resultBn);
    sslBnClearFree(&inputBn);
    if (buf != NULL) {
        sslMemset(buf, 0, (u32)modLen);
        sceSsl_lib_5E5C873A(buf);
    }
    return ret;
}

/* [338] 0x014f80 */
LIBSSL_NOINLINE static int sslRsaEayPrivateDecrypt(int flen, const u8 *from, u8 *to,
                                                   SslRsa *rsa, int padding)
{
    SslBignum inputBn;
    SslBignum resultBn;
    SslBnBlinding localBlind;
    SslBnCtx *ctx;
    u8 *buf = NULL;
    int modLen = 0;
    int ret = -1;
    int ok = 1;

    sslMemset(&localBlind, 0, sizeof(localBlind));
    sslBnInit(&inputBn);
    sslBnInit(&resultBn);
    ctx = sslBnCtxNew();
    if (ctx != NULL) {
        modLen = (sslBnNumBits(rsa->modN) + 7) / 8;
        buf = (u8 *)sceSsl_lib_0861D934((u32)modLen);
        if (buf == NULL) {
            sceSsl_lib_9CDCA0D7(4, 100, 33, NULL, 0);
            ok = 0;
        } else if (flen > modLen) {
            sceSsl_lib_9CDCA0D7(4, 100, 108, NULL, 0);
            ok = 0;
        } else if (sslBnBinToBn(from, flen, &inputBn) == NULL) {
            ok = 0;
        }

        if (ok && rsa->blindingMode > 0) {
            if (rsa->blinding == NULL) {
                sceSsl_lib_D84E2411(9, 9, "crypto/rsa/rsa_eay.c", 395);
                sslRsaBlindingOn(rsa, ctx);
                sceSsl_lib_D84E2411(10, 9, "crypto/rsa/rsa_eay.c", 397);
            }
            if (rsa->blindingMode > 0) {
                sceSsl_lib_D84E2411(9, 9, "crypto/rsa/rsa_eay.c", 401);
                if (!sslBnBlindingCopy(&localBlind, rsa->blinding)) {
                    sceSsl_lib_D84E2411(10, 9, "crypto/rsa/rsa_eay.c", 470);
                    ok = 0;
                } else if (rsa->blindingMode == 2) {
                    sslBnBlindingFree(rsa->blinding, 1);
                    rsa->blinding = NULL;
                    sceSsl_lib_D84E2411(10, 9, "crypto/rsa/rsa_eay.c", 418);
                } else if (rsa->blindingMode == 3 && !sslBnBlindingUpdate(rsa->blinding, ctx)) {
                    sceSsl_lib_D84E2411(10, 9, "crypto/rsa/rsa_eay.c", 470);
                    ok = 0;
                } else {
                    sceSsl_lib_D84E2411(10, 9, "crypto/rsa/rsa_eay.c", 418);
                }
                if (ok && !sslBnBlindingConvert(&inputBn, &localBlind, ctx)) {
                    ok = 0;
                }
            }
        }

        if (ok) {
            if (rsa->primeP != NULL && rsa->primeQ != NULL && rsa->expDmp != NULL &&
                rsa->expDmq != NULL && rsa->coeffIqmp != NULL) {
                if (!rsa->meth->rsaModExp(&resultBn, &inputBn, rsa)) {
                    ok = 0;
                }
            } else {
                if (rsa->bnCb != NULL) {
                    rsa->bnCb(rsa->bnCbArg, &ctx->flags, 1);
                }
                if (!rsa->meth->bnModExp(&resultBn, &inputBn, rsa->privD, rsa->modN, ctx, rsa->montModN)) {
                    ok = 0;
                } else if (rsa->bnCb != NULL) {
                    rsa->bnCb(rsa->bnCbArg, &ctx->flags, 0);
                }
            }
        }

        if (ok && (rsa->blindingMode <= 0 || sslBnBlindingInvert(&resultBn, &localBlind, ctx) != 0)) {
            int plainLen = sslBnBnToBin(&resultBn, buf);
            switch (padding) {
            case 1:
                ret = sslRsaPaddingCheckPkcs1TypeTwo(to, modLen, buf, plainLen, modLen);
                break;
            case 2:
                ret = sslRsaPaddingCheckSslv23(to, modLen, buf, plainLen, modLen);
                break;
            case 3:
                ret = sslRsaPaddingCheckNone(to, modLen, buf, plainLen, modLen);
                break;
            default:
                sceSsl_lib_9CDCA0D7(4, 100, 118, NULL, 0);
                ok = 0;
                break;
            }
            if (ok && ret < 0) {
                sceSsl_lib_9CDCA0D7(4, 100, 114, NULL, 0);
            }
        }
    }

    sslBnBlindingFree(&localBlind, 0);
    if (ctx != NULL) {
        sslBnCtxFree(ctx);
    }
    sslBnClearFree(&inputBn);
    sslBnClearFree(&resultBn);
    if (buf != NULL) {
        sslMemset(buf, 0, (u32)modLen);
        sceSsl_lib_5E5C873A(buf);
    }
    return ret;
}

/* [339] 0x0153c4 */
LIBSSL_NOINLINE static int sslRsaEayPublicDecrypt(int flen, const u8 *from, u8 *to,
                                                  SslRsa *rsa, int padding)
{
    SslBignum inputBn;
    SslBignum resultBn;
    SslBnCtx *ctx;
    u8 *buf = NULL;
    int modLen = 0;
    int ret = -1;

    sslBnInit(&inputBn);
    sslBnInit(&resultBn);
    ctx = sslBnCtxNew();
    if (ctx != NULL) {
        if (rsa->bnCb != NULL) {
            rsa->bnCb(rsa->bnCbArg, &ctx->flags, 1);
        }
        modLen = (sslBnNumBits(rsa->modN) + 7) / 8;
        buf = (u8 *)sceSsl_lib_0861D934((u32)modLen);
        if (buf == NULL) {
            sceSsl_lib_9CDCA0D7(4, 102, 33, NULL, 0);
        } else if (flen > modLen) {
            sceSsl_lib_9CDCA0D7(4, 102, 108, NULL, 0);
        } else if (sslBnBinToBn(from, flen, &inputBn) != NULL) {
            int montOk = 1;
            if (sslMontCacheEnabled() && rsa->montModN == NULL && (rsa->flags & 2) != 0) {
                rsa->montModN = sslBnMontCtxNew();
                if (rsa->montModN != NULL && !sslBnMontCtxSet(rsa->montModN, rsa->modN, ctx)) {
                    montOk = 0;
                }
            }
            if (montOk &&
                rsa->meth->bnModExp(&resultBn, &inputBn, rsa->pubE, rsa->modN, ctx, rsa->montModN) != 0) {
                int plainLen = sslBnBnToBin(&resultBn, buf);
                if (padding == 1) {
                    ret = sslRsaPaddingCheckPkcs1TypeOne(to, modLen, buf, plainLen, modLen);
                    if (ret < 0) {
                        sceSsl_lib_9CDCA0D7(4, 102, 114, NULL, 0);
                    }
                } else if (padding == 3) {
                    ret = sslRsaPaddingCheckNone(to, modLen, buf, plainLen, modLen);
                    if (ret < 0) {
                        sceSsl_lib_9CDCA0D7(4, 102, 114, NULL, 0);
                    }
                } else {
                    sceSsl_lib_9CDCA0D7(4, 102, 118, NULL, 0);
                }
            }
        }
        if (rsa->bnCb != NULL) {
            rsa->bnCb(rsa->bnCbArg, &ctx->flags, 0);
        }
        sslBnCtxFree(ctx);
    }

    sslBnClearFree(&inputBn);
    sslBnClearFree(&resultBn);
    if (buf != NULL) {
        sslMemset(buf, 0, (u32)modLen);
        sceSsl_lib_5E5C873A(buf);
    }
    return ret;
}

/* [340] 0x01563c */
LIBSSL_NOINLINE static int sslRsaEayModExp(SslBignum *out, const SslBignum *base, SslRsa *rsa)
{
    SslBignum rOne;
    SslBignum mOne;
    SslBnCtx *ctx;
    int ret = 0;
    int montOk = 1;

    ctx = sslBnCtxNew();
    if (ctx != NULL) {
        sslBnInit(&mOne);
        sslBnInit(&rOne);
        if (rsa->bnCb != NULL) {
            rsa->bnCb(rsa->bnCbArg, &ctx->flags, 1);
        }

        if (sslMontCacheEnabled() && (rsa->flags & 4) != 0) {
            if (rsa->montPrimeP == NULL) {
                rsa->montPrimeP = sslBnMontCtxNew();
                if (rsa->montPrimeP != NULL && !sslBnMontCtxSet(rsa->montPrimeP, rsa->primeP, ctx)) {
                    montOk = 0;
                }
            }
            if (montOk && rsa->montPrimeQ == NULL) {
                rsa->montPrimeQ = sslBnMontCtxNew();
                if (rsa->montPrimeQ != NULL && !sslBnMontCtxSet(rsa->montPrimeQ, rsa->primeQ, ctx)) {
                    montOk = 0;
                }
            }
        }

        if (montOk &&
            sslBnMod(&rOne, base, rsa->primeQ, ctx) &&
            rsa->meth->bnModExp(&mOne, &rOne, rsa->expDmq, rsa->primeQ, ctx, rsa->montPrimeQ) &&
            sslBnMod(&rOne, base, rsa->primeP, ctx) &&
            rsa->meth->bnModExp(out, &rOne, rsa->expDmp, rsa->primeP, ctx, rsa->montPrimeP) &&
            sslBnSub(out, out, &mOne)) {
            int subOk = 1;
            if (out->neg != 0) {
                if (!sslBnAdd(out, out, rsa->primeP)) {
                    subOk = 0;
                } else if (out->neg != 0 && !sslBnMod(out, out, rsa->primeP, ctx)) {
                    subOk = 0;
                }
            }
            if (subOk &&
                sslBnMul(&rOne, out, rsa->coeffIqmp, ctx) &&
                sslBnMod(out, &rOne, rsa->primeP, ctx) &&
                sslBnMul(&rOne, out, rsa->primeQ, ctx) &&
                sslBnAdd(out, &rOne, &mOne)) {
                ret = 1;
            }
        }

        if (rsa->bnCb != NULL) {
            rsa->bnCb(rsa->bnCbArg, &ctx->flags, 0);
        }
        sslBnCtxFree(ctx);
        sslBnClearFree(&mOne);
        sslBnClearFree(&rOne);
    }
    return ret;
}

/* [341] 0x0158d0 */
LIBSSL_NOINLINE static int sslRsaEayInit(SslRsa *rsa)
{
    rsa->flags |= 6;
    return 1;
}

/* [342] 0x0158e4 */
LIBSSL_NOINLINE static int sslRsaEayFinish(SslRsa *rsa)
{
    if (rsa->montModN != NULL) {
        sslBnMontCtxFree(rsa->montModN);
    }
    if (rsa->montPrimeP != NULL) {
        sslBnMontCtxFree(rsa->montPrimeP);
    }
    if (rsa->montPrimeQ != NULL) {
        sslBnMontCtxFree(rsa->montPrimeQ);
    }
    return 1;
}

/* -------------------------------------------------------------------------
 * crypto/rsa/rsa_lib.c [343..354]
 * ------------------------------------------------------------------------- */

/* [343] 0x015944 */
LIBSSL_NOINLINE SslRsa *sslRsaNew(void)
{
    return sslRsaNewMethod(NULL);
}

/* [344] 0x015960 */
LIBSSL_NOINLINE SslRsa *sslRsaNewMethod(const SslRsaMethod *meth)
{
    SslRsa *rsa;

    if (g_rsaDefaultMethod == NULL) {
        g_rsaDefaultMethod = sslRsaPkcs1Ssleay();
    }

    rsa = (SslRsa *)sceSsl_lib_0861D934(sizeof(SslRsa));
    if (rsa == NULL) {
        sceSsl_lib_9CDCA0D7(4, 106, 33, NULL, 0);
        return NULL;
    }

    rsa->meth = (meth != NULL) ? meth : g_rsaDefaultMethod;
    rsa->padWord = 0;
    rsa->version = 0;
    rsa->modN = NULL;
    rsa->pubE = NULL;
    rsa->privD = NULL;
    rsa->primeP = NULL;
    rsa->primeQ = NULL;
    rsa->expDmp = NULL;
    rsa->expDmq = NULL;
    rsa->coeffIqmp = NULL;
    rsa->references = 1;
    rsa->flags = rsa->meth->flags;
    rsa->montModN = NULL;
    rsa->montPrimeP = NULL;
    rsa->montPrimeQ = NULL;
    rsa->bignumData = NULL;
    rsa->bnCb = NULL;
    rsa->bnCbArg = NULL;
    rsa->blindingMode = 1;
    rsa->blinding = NULL;

    if (rsa->meth->init != NULL && !rsa->meth->init(rsa)) {
        sceSsl_lib_5E5C873A(rsa);
        sceSsl_lib_9CDCA0D7(4, 106, 120, NULL, 0);
        return NULL;
    }

    sslCryptoNewExData(g_rsaExDataFuncs, rsa, &rsa->exData);
    return rsa;
}

/* [345] 0x015a88 */
LIBSSL_NOINLINE int sslRsaSetBlindingMode(SslRsa *rsa, int mode)
{
    rsa->blindingMode = mode;
    return 1;
}

/* [346] 0x015a94 */
LIBSSL_NOINLINE void sslRsaFree(SslRsa *rsa)
{
    if (rsa == NULL) {
        return;
    }
    if (sslCryptoAddLock(&rsa->references, -1, 9, "crypto/rsa/rsa_lib.c", 109) > 0) {
        return;
    }

    sslCryptoFreeExData(g_rsaExDataFuncs, rsa, &rsa->exData);
    if (rsa->meth->finish != NULL) {
        rsa->meth->finish(rsa);
    }

    if (rsa->modN != NULL) sslBnClearFree(rsa->modN);
    if (rsa->pubE != NULL) sslBnClearFree(rsa->pubE);
    if (rsa->privD != NULL) sslBnClearFree(rsa->privD);
    if (rsa->primeP != NULL) sslBnClearFree(rsa->primeP);
    if (rsa->primeQ != NULL) sslBnClearFree(rsa->primeQ);
    if (rsa->expDmp != NULL) sslBnClearFree(rsa->expDmp);
    if (rsa->expDmq != NULL) sslBnClearFree(rsa->expDmq);
    if (rsa->coeffIqmp != NULL) sslBnClearFree(rsa->coeffIqmp);
    if (rsa->blinding != NULL) sslBnBlindingFree(rsa->blinding, 1);
    if (rsa->bignumData != NULL) sceSsl_lib_E5FC4992(rsa->bignumData);
    sceSsl_lib_5E5C873A(rsa);
}

/* [347] 0x015bd8 */
LIBSSL_NOINLINE int sslRsaSize(const SslRsa *rsa)
{
    return (sslBnNumBits(rsa->modN) + 7) / 8;
}

/* [348] 0x015c08 */
LIBSSL_NOINLINE int sslRsaPublicEncrypt(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding)
{
    return rsa->meth->rsaPubEnc(flen, from, to, rsa, padding);
}

/* [349] 0x015c2c */
LIBSSL_NOINLINE int sslRsaPrivateEncrypt(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding)
{
    return rsa->meth->rsaPrivEnc(flen, from, to, rsa, padding);
}

/* [350] 0x015c50 */
LIBSSL_NOINLINE int sslRsaPublicDecrypt(int flen, const u8 *from, u8 *to, SslRsa *rsa, int padding)
{
    return rsa->meth->rsaPubDec(flen, from, to, rsa, padding);
}

/* [351] 0x015c74 */
LIBSSL_NOINLINE int sslRsaFlags(const SslRsa *rsa)
{
    if (rsa == NULL) {
        return 0;
    }
    return rsa->meth->flags;
}

/* [352] 0x015c8c */
LIBSSL_NOINLINE void sceSsl_lib_A87C65C7(SslRsa *rsa, SslRsaBnCb cb, void *cbArg)
{
    rsa->bnCb = cb;
    rsa->bnCbArg = cbArg;
}

/* [353] 0x015c98 */
LIBSSL_NOINLINE int sceSsl_lib_72D77445(int mode)
{
    return mode | 0x4000;
}

/* [354] 0x015ca0 */
LIBSSL_NOINLINE int sslRsaBlindingOn(SslRsa *rsa, SslBnCtx *ctx)
{
    SslBnCtx *useCtx = ctx;
    int ret = 0;

    if (useCtx == NULL) {
        useCtx = sslBnCtxNew();
    }
    if (useCtx != NULL) {
        SslBignum *randBn;
        SslBignum *invBn;

        if (rsa->blinding != NULL) {
            sslBnBlindingFree(rsa->blinding, 1);
        }
        randBn = &useCtx->pool[useCtx->tos++];
        if (sslBnRand(randBn, sslBnNumBits(rsa->modN) / 2, 1, 0) != 0) {
            invBn = sslBnModInverse(NULL, randBn, rsa->modN, useCtx);
            if (invBn != NULL &&
                rsa->meth->bnModExp(randBn, randBn, rsa->pubE, rsa->modN, useCtx, rsa->montModN) != 0) {
                rsa->blinding = sslBnBlindingNew(randBn, invBn, rsa->modN);
                useCtx->tos--;
                rsa->flags |= 8;
                sslBnFree(invBn);
                ret = 1;
            }
        }
        if (useCtx != ctx) {
            sslBnCtxFree(useCtx);
        }
    }
    return ret;
}

/* -------------------------------------------------------------------------
 * RSA padding & PKCS#1 sign/verify [355..364]
 * ------------------------------------------------------------------------- */

/* [355] 0x015de8 */
LIBSSL_NOINLINE int sslRsaPaddingAddNone(u8 *to, int toLen, const u8 *from, int fromLen)
{
    if (fromLen >= toLen) {
        sceSsl_lib_9CDCA0D7(4, 107, 110, NULL, 0);
        return 0;
    }
    to[0] = 0;
    sslMemcpy(to + 1, from, (u32)fromLen);
    return 1;
}

/* [356] 0x015e54 */
LIBSSL_NOINLINE int sslRsaPaddingCheckNone(u8 *to, int toLen, const u8 *from, int fromLen, int num)
{
    int payloadLen;

    if (fromLen + 1 > toLen) {
        sceSsl_lib_9CDCA0D7(4, 111, 109, NULL, 0);
        return -1;
    }
    if (fromLen + 1 >= num) {
        sceSsl_lib_9CDCA0D7(4, 111, 105, NULL, 0);
        return -1;
    }
    payloadLen = fromLen - 1;
    sslMemset(to, 0, (u32)(toLen - payloadLen));
    sslMemcpy(to + toLen - payloadLen, from + 1, (u32)payloadLen);
    return payloadLen;
}

/* [357] 0x015efc */
LIBSSL_NOINLINE int sslRsaPaddingAddPkcs1TypeOne(u8 *to, int toLen, const u8 *from, int fromLen)
{
    int padLen;

    if (fromLen > toLen - 11) {
        sceSsl_lib_9CDCA0D7(4, 108, 110, NULL, 0);
        return 0;
    }
    padLen = toLen - fromLen - 3;
    to[0] = 0;
    to[1] = 1;
    sslMemset(to + 2, 0xff, (u32)padLen);
    to[2 + padLen] = 0;
    sslMemcpy(to + 3 + padLen, from, (u32)fromLen);
    return 1;
}

/* [358] 0x015fac */
LIBSSL_NOINLINE int sslRsaPaddingCheckPkcs1TypeOne(u8 *to, int toLen, const u8 *from,
                                                   int fromLen, int num)
{
    const u8 *scan;
    int remain;
    int padCount;
    int payloadLen;
    (void)toLen;

    if (num != fromLen + 1 || from[0] != 1) {
        sceSsl_lib_9CDCA0D7(4, 112, 106, NULL, 0);
        return -1;
    }

    scan = from + 1;
    remain = fromLen - 1;
    for (padCount = 0; padCount < remain; padCount++) {
        if (*scan != 0xff) {
            if (*scan != 0) {
                sceSsl_lib_9CDCA0D7(4, 112, 102, NULL, 0);
                return -1;
            }
            scan++;
            break;
        }
        scan++;
    }

    if (padCount == remain) {
        sceSsl_lib_9CDCA0D7(4, 112, 113, NULL, 0);
        return -1;
    }
    if (padCount < 8) {
        sceSsl_lib_9CDCA0D7(4, 112, 103, NULL, 0);
        return -1;
    }

    payloadLen = remain - (padCount + 1);
    sslMemcpy(to, scan, (u32)payloadLen);
    return payloadLen;
}

/* [359] 0x016080 */
LIBSSL_NOINLINE int sslRsaPaddingAddPkcs1TypeTwo(u8 *to, int toLen, const u8 *from, int fromLen)
{
    int padLen;
    u8 *dst;

    if (fromLen > toLen - 11) {
        sceSsl_lib_9CDCA0D7(4, 109, 110, NULL, 0);
        return 0;
    }

    to[0] = 0;
    to[1] = 2;
    padLen = toLen - fromLen - 3;
    dst = to + 2;
    sceSsl_lib_21F2A35C(dst, padLen);
    while (padLen > 0) {
        while (*dst == 0) {
            sceSsl_lib_21F2A35C(dst, 1);
        }
        dst++;
        padLen--;
    }
    *dst = 0;
    sslMemcpy(dst + 1, from, (u32)fromLen);
    return 1;
}

/* [360] 0x016164 */
LIBSSL_NOINLINE int sslRsaPaddingCheckPkcs1TypeTwo(u8 *to, int toLen, const u8 *from,
                                                   int fromLen, int num)
{
    const u8 *scan;
    int remain;
    int padCount;
    int payloadLen;
    (void)toLen;

    if (num != fromLen + 1 || from[0] != 2) {
        sceSsl_lib_9CDCA0D7(4, 113, 107, NULL, 0);
        return -1;
    }

    scan = from + 1;
    remain = fromLen - 1;
    for (padCount = 0; padCount < remain; padCount++) {
        u8 ch = *scan++;
        if (ch == 0) {
            break;
        }
    }

    if (padCount == remain) {
        sceSsl_lib_9CDCA0D7(4, 113, 113, NULL, 0);
        return -1;
    }
    if (padCount < 8) {
        sceSsl_lib_9CDCA0D7(4, 113, 103, NULL, 0);
        return -1;
    }

    payloadLen = remain - (padCount + 1);
    sslMemcpy(to, scan, (u32)payloadLen);
    return payloadLen;
}

/* [361] 0x016220 */
LIBSSL_NOINLINE int sslRsaSign(int mdType, const u8 *digest, u32 digestLen,
                               u8 *sigRet, u32 *sigLen, SslRsa *rsa)
{
    SslX509Sig sig;
    SslX509Algor algor;
    SslAsn1Type param;
    SslAsn1String digestStr;
    int derLen;
    int rsaBytes;
    u8 *derBuf;
    u8 *dst;
    int encLen;
    int ret = 1;

    sig.algor = &algor;
    algor.algorithm = sslObjNidToObj((u32)mdType);
    if (algor.algorithm == NULL) {
        sceSsl_lib_9CDCA0D7(4, 117, 117, NULL, 0);
        return 0;
    }
    if (algor.algorithm->length == 0) {
        sceSsl_lib_9CDCA0D7(4, 117, 116, NULL, 0);
        return 0;
    }

    param.type = 5;
    param.value.ptr = NULL;
    algor.parameter = &param;

    digestStr.length = (int)digestLen;
    digestStr.type = 4;
    digestStr.data = (u8 *)digest;
    digestStr.flags = 0;
    sig.digest = &digestStr;

    derLen = sslI2dX509Sig(&sig, NULL);
    rsaBytes = sslRsaSize(rsa);
    if (rsaBytes < derLen - 1) {
        sceSsl_lib_9CDCA0D7(4, 117, 112, NULL, 0);
        return 0;
    }

    derBuf = (u8 *)sceSsl_lib_0861D934((u32)(rsaBytes + 1));
    if (derBuf == NULL) {
        sceSsl_lib_9CDCA0D7(4, 117, 33, NULL, 0);
        return 0;
    }

    dst = derBuf;
    sslI2dX509Sig(&sig, &dst);
    encLen = sslRsaPrivateEncrypt(derLen, derBuf, sigRet, rsa, 1);
    if (encLen <= 0) {
        ret = 0;
    } else {
        *sigLen = (u32)encLen;
    }

    sslMemset(derBuf, 0, (u32)(rsaBytes + 1));
    sceSsl_lib_5E5C873A(derBuf);
    return ret;
}

/* [362] 0x0163a0 */
LIBSSL_NOINLINE int sslRsaVerify(int mdType, const u8 *digest, u32 digestLen,
                                 const u8 *sigBuf, u32 sigLen, SslRsa *rsa)
{
    u8 *decBuf;
    int decLen;
    const u8 *cur;
    SslX509Sig *sig = NULL;
    int ret = 0;

    if (sigLen != (u32)sslRsaSize(rsa)) {
        sceSsl_lib_9CDCA0D7(4, 119, 119, NULL, 0);
        return 0;
    }

    decBuf = (u8 *)sceSsl_lib_0861D934(sigLen);
    if (decBuf == NULL) {
        sceSsl_lib_9CDCA0D7(4, 119, 33, NULL, 0);
        return 0;
    }

    decLen = sslRsaPublicDecrypt((int)sigLen, sigBuf, decBuf, rsa, 1);
    if (decLen > 0) {
        cur = decBuf;
        sig = sslD2iX509Sig(NULL, &cur, decLen);
        if (sig == NULL) {
            if ((int)(cur - decBuf) != decLen) {
                sceSsl_lib_9CDCA0D7(4, 119, 104, NULL, 0);
            }
        } else {
            if ((int)(cur - decBuf) != decLen) {
                sceSsl_lib_9CDCA0D7(4, 119, 104, NULL, 0);
            } else if (sslObjObjToNid(sig->algor->algorithm) != mdType) {
                sceSsl_lib_9CDCA0D7(4, 119, 100, NULL, 0);
            } else if (sig->algor->parameter != NULL && sig->algor->parameter->type != 5) {
                sceSsl_lib_9CDCA0D7(4, 119, 104, NULL, 0);
            } else if ((u32)sig->digest->length == digestLen &&
                       sslMemcmp(digest, sig->digest->data, (int)digestLen) == 0) {
                ret = 1;
            } else {
                sceSsl_lib_9CDCA0D7(4, 119, 104, NULL, 0);
            }
            sslX509SigFree(sig);
        }
    }

    sslMemset(decBuf, 0, sigLen);
    sceSsl_lib_5E5C873A(decBuf);
    return ret;
}

/* [363] 0x016564 */
LIBSSL_NOINLINE int sslRsaPaddingAddSslv23(u8 *to, int toLen, const u8 *from, int fromLen)
{
    int padLen;
    u8 *dst;

    if (fromLen > toLen - 11) {
        sceSsl_lib_9CDCA0D7(4, 110, 110, NULL, 0);
        return 0;
    }

    to[0] = 0;
    to[1] = 2;
    padLen = toLen - fromLen - 11;
    dst = to + 2;
    sceSsl_lib_21F2A35C(dst, padLen);
    while (padLen > 0) {
        while (*dst == 0) {
            sceSsl_lib_21F2A35C(dst, 1);
        }
        dst++;
        padLen--;
    }
    sslMemset(dst, 3, 8);
    dst += 8;
    *dst = 0;
    sslMemcpy(dst + 1, from, (u32)fromLen);
    return 1;
}

/* [364] 0x01665c */
LIBSSL_NOINLINE int sslRsaPaddingCheckSslv23(u8 *to, int toLen, const u8 *from,
                                             int fromLen, int num)
{
    const u8 *scan;
    int remain;
    int padCount;
    int checkIdx;
    int payloadLen;
    (void)toLen;

    if (fromLen < 10) {
        sceSsl_lib_9CDCA0D7(4, 114, 111, NULL, 0);
        return -1;
    }
    if (num != fromLen + 1 || from[0] != 2) {
        sceSsl_lib_9CDCA0D7(4, 114, 107, NULL, 0);
        return -1;
    }

    scan = from + 1;
    remain = fromLen - 1;
    for (padCount = 0; padCount < remain; padCount++) {
        u8 ch = *scan++;
        if (ch == 0) {
            break;
        }
    }

    if (padCount == remain || padCount < 8) {
        sceSsl_lib_9CDCA0D7(4, 114, 113, NULL, 0);
        return -1;
    }

    for (checkIdx = -8; checkIdx < 0; checkIdx++) {
        if (scan[checkIdx] != 3) {
            break;
        }
    }
    if (checkIdx == 0) {
        sceSsl_lib_9CDCA0D7(4, 114, 115, NULL, 0);
        return -1;
    }

    payloadLen = remain - (padCount + 1);
    sslMemcpy(to, scan, (u32)payloadLen);
    return payloadLen;
}

/* -------------------------------------------------------------------------
 * crypto/stack/stack.c [365..372]
 * ------------------------------------------------------------------------- */

/* [365] 0x016760 */
LIBSSL_NOINLINE SslQsortCmpFn sslSkSetCmpFunc(SslStack *sk, SslQsortCmpFn cmpFn)
{
    SslQsortCmpFn prev = sk->comp;
    if (prev != cmpFn) {
        sk->sorted = 0;
    }
    sk->comp = cmpFn;
    return prev;
}

/* [366] 0x016774 */
LIBSSL_NOINLINE SslStack *sslSkDup(const SslStack *sk)
{
    SslStack *copy = sslSkNew(sk->comp);
    void **grown;

    if (copy == NULL) {
        return NULL;
    }
    grown = (void **)sslCryptoRealloc(copy->data, (u32)sk->numAlloc * sizeof(void *));
    if (grown == NULL) {
        return NULL;
    }
    copy->data = grown;
    copy->num = sk->num;
    sslMemcpy(copy->data, sk->data, (u32)sk->num * sizeof(void *));
    copy->sorted = sk->sorted;
    copy->numAlloc = sk->numAlloc;
    copy->comp = sk->comp;
    return copy;
}

/* [367] 0x016810 */
LIBSSL_NOINLINE SslStack *sslSkNew(SslQsortCmpFn cmpFn)
{
    SslStack *sk = (SslStack *)sceSsl_lib_0861D934(sizeof(SslStack));
    int idx;

    if (sk == NULL) {
        return NULL;
    }
    sk->data = (void **)sceSsl_lib_0861D934(40U * sizeof(void *));
    if (sk->data == NULL) {
        sceSsl_lib_5E5C873A(sk);
        return NULL;
    }
    for (idx = 0; idx < 40; idx++) {
        sk->data[idx] = NULL;
    }
    sk->num = 0;
    sk->sorted = 0;
    sk->numAlloc = 40;
    sk->comp = cmpFn;
    return sk;
}

/* [368] 0x0168a0 */
LIBSSL_NOINLINE int sslSkInsert(SslStack *sk, void *item, int pos)
{
    if (sk->num + 1 >= sk->numAlloc) {
        void **grown = (void **)sslCryptoRealloc(
            sk->data, (u32)(2 * sk->numAlloc) * sizeof(void *));
        if (grown == NULL) {
            return 0;
        }
        sk->data = grown;
        sk->numAlloc *= 2;
    }

    if (pos >= sk->num || pos < 0) {
        sk->data[sk->num] = item;
    } else {
        int idx;
        for (idx = sk->num; idx > pos; idx--) {
            sk->data[idx] = sk->data[idx - 1];
        }
        sk->data[pos] = item;
    }

    sk->num++;
    sk->sorted = 0;
    return sk->num;
}

/* [369] 0x016988 */
LIBSSL_NOINLINE int sslSkFind(SslStack *sk, void *item)
{
    void *key = item;

    if (sk->comp == NULL) {
        int idx;
        for (idx = 0; idx < sk->num; idx++) {
            if (sk->data[idx] == key) {
                return idx;
            }
        }
        return -1;
    }

    if (sk->sorted == 0) {
        sslQsort(sk->data, (u32)sk->num, sizeof(void *), sk->comp);
        sk->sorted = 1;
    }
    if (key == NULL) {
        return -1;
    }

    {
        void **found = (void **)sslEvpBsearch(&key, sk->data, sk->num, (int)sizeof(void *), sk->comp);
        int idx;
        if (found == NULL) {
            return -1;
        }
        idx = (int)(found - sk->data);
        while (idx > 0 && sk->comp(&sk->data[idx - 1], &key) >= 0) {
            idx--;
        }
        return idx;
    }
}

/* [370] 0x016aa0 */
LIBSSL_NOINLINE int sslSkPush(SslStack *sk, void *item)
{
    return sslSkInsert(sk, item, sk->num);
}

/* [371] 0x016ac0 */
LIBSSL_NOINLINE void sslSkPopFree(SslStack *sk, SslFreeFn freeFn)
{
    int idx;

    if (sk == NULL) {
        return;
    }
    for (idx = 0; idx < sk->num; idx++) {
        if (sk->data[idx] != NULL) {
            freeFn(sk->data[idx]);
        }
    }
    sslSkFree(sk);
}

/* [372] 0x016b40 */
LIBSSL_NOINLINE void sslSkFree(SslStack *sk)
{
    if (sk == NULL) {
        return;
    }
    if (sk->data != NULL) {
        sceSsl_lib_5E5C873A(sk->data);
    }
    sceSsl_lib_5E5C873A(sk);
}

/* -------------------------------------------------------------------------
 * RSA SubjectPublicKeyInfo decode [373..374]
 * ------------------------------------------------------------------------- */

/* [373] 0x016b80 */
LIBSSL_NOINLINE int sslRsaDecodePubKeyItem(SslEvpPkey *pkey, const SslAsn1Item *spkiItem)
{
    SslAsn1Item algOidItem;
    SslAsn1Item algParamItem;
    SslAsn1Item pubKeyBitsItem;
    SslAsn1Item *outItems[3];
    SslAsn1Element elems[3];
    SslAsn1Tree tree;
    SslAsn1Element bitStrElem;
    int err;
    SslRsa *rsa;

    outItems[0] = &algOidItem;
    outItems[1] = &algParamItem;
    outItems[2] = &pubKeyBitsItem;

    if (spkiItem == NULL || spkiItem->itemType != 41) {
        return 2;
    }

    sslAsn1ClearItem(&algOidItem);
    sslAsn1ClearItem(&algParamItem);
    sslAsn1ClearItem(&pubKeyBitsItem);

    err = sslAsn1ExtractFields(spkiItem, outItems, g_rsaSpkiSpecs, 3, 1);
    if (err != 0) {
        return err;
    }

    sslAsn1InitTree(&tree, elems, 0, 3);
    if (algOidItem.length != 9 || sslMemcmp(algOidItem.data, g_rsaEncryptionOid, 9) != 0) {
        return 999;
    }

    sslAsn1ClearTreeStruct((SslAsn1Tree *)&bitStrElem);
    err = sceSsl_lib_C85F6FCE(&bitStrElem, (const u8 *)pubKeyBitsItem.data, pubKeyBitsItem.length);
    if (err != 0) {
        return err;
    }

    err = sslAsn1DecodeTree(&tree, bitStrElem.data + 1, bitStrElem.length - 1);
    if (err != 0) {
        return err;
    }

    if (tree.count != 3 || elems[0].tag != 16 || elems[1].tag != 2 || elems[2].tag != 2) {
        return 10;
    }

    rsa = sslRsaNew();
    if (rsa == NULL) {
        return 5;
    }

    rsa->modN = sslBnBinToBn(elems[1].data, (int)elems[1].length, NULL);
    if (rsa->modN == NULL) {
        sslRsaFree(rsa);
        return 0;
    }
    rsa->pubE = sslBnBinToBn(elems[2].data, (int)elems[2].length, NULL);
    if (rsa->pubE == NULL) {
        sslRsaFree(rsa);
        return 0;
    }
    if (!sslEvpPkeyAssign(pkey, 6, rsa)) {
        sslRsaFree(rsa);
        return 0;
    }
    return 0;
}

/* [374] 0x016d74 */
LIBSSL_NOINLINE SslEvpPkey *sslD2iPublicKey(int keyType, SslEvpPkey **outKey,
                                            const u8 **inPtr, int maxLen)
{
    SslEvpPkey *pkey = NULL;
    SslAsn1Item spkiItem;

    if (keyType == 6) {
        if (outKey != NULL && *outKey != NULL) {
            pkey = *outKey;
        } else {
            pkey = sslEvpPkeyNew();
        }
        if (pkey != NULL) {
            sslAsn1ClearItem(&spkiItem);
            if (sslAsn1SetItemData(&spkiItem, 41, *inPtr, (u32)maxLen, 1) == 0 &&
                sslRsaDecodePubKeyItem(pkey, &spkiItem) == 0) {
                if (outKey != NULL && *outKey == NULL) {
                    *outKey = pkey;
                }
                *inPtr += maxLen;
                return pkey;
            }
        }
    }

    if (outKey == NULL || *outKey == NULL) {
        if (pkey != NULL) {
            sslEvpPkeyFree(pkey);
        }
    }
    return NULL;
}
