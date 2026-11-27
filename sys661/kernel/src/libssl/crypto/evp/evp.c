/*
 * SceSsl / sceSsl_Module (ePSP 6.61) - EVP cipher, digest, PKEY, and base64 layer.
 *
 * Implements functions [105..195] (0x00c64c..0x00e5bf):
 *   - Digest final wrappers (MD5, SHA-1, SHA-224/256/384/512)
 *   - EVP_DigestInit / Update / Final
 *   - EVP_CIPHER vtables (DES-EDE3-CBC, DES-CBC, RC2-CBC/64/40, ENC_NULL, RC4)
 *   - EVP_Decode* base64 decoder
 *   - EVP_MD vtables (MD2, MD5, SHA-1, SHA-224/256/384/512)
 *   - EVP_add_cipher / digest / get_cipherbyname / get_digestbyname / cleanup
 *   - EVP_PKEY lifecycle (sceSsl_lib_09C45275, FAF333D1, 7317C1BC)
 *   - EVP_CIPHER_CTX / EVP_MD_CTX / EVP_BytesToKey / generic ASN.1 IV helpers
 *   - Default time callback and setter
 */
#include "../../libssl_internal.h"

/* Base64 decoding lookup table (0x02e18c, 128 bytes) */
static const u8 g_b64DecodeTable[128] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xe0, 0xf0, 0xff, 0xff, 0xf1, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3e, 0xff, 0xf2, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff
};

/* Global time callback pointer (0x030e98) */
SslTimeCb g_sslTimeCallback = NULL;

/* -------------------------------------------------------------------------
 * Digest final argument-swap adapters [105..110]
 * ------------------------------------------------------------------------- */

/* [105] 0x00c64c */
LIBSSL_NOINLINE int sslEvpMd5Final(u8 *outDigest, void *mdCtx)
{
    return sceKernelUtilsMd5BlockResult((SceSslMd5Ctx *)mdCtx, outDigest);
}

/* [106] 0x00c670 */
LIBSSL_NOINLINE int sslEvpSha1Final(u8 *outDigest, void *mdCtx)
{
    return sceKernelUtilsSha1BlockResult((SceSslSha1Ctx *)mdCtx, outDigest);
}

/* [107] 0x00c694 */
LIBSSL_NOINLINE int sslEvpSha224Final(u8 *outDigest, void *mdCtx)
{
    return sslSha224Final((SceSha256Context *)mdCtx, outDigest);
}

/* [108] 0x00c6b8 */
LIBSSL_NOINLINE int sslEvpSha256Final(u8 *outDigest, void *mdCtx)
{
    return sslSha256Final((SceSha256Context *)mdCtx, outDigest);
}

/* [109] 0x00c6dc */
LIBSSL_NOINLINE int sslEvpSha384Final(u8 *outDigest, void *mdCtx)
{
    return sslSha384Final((SceSha512Context *)mdCtx, outDigest);
}

/* [110] 0x00c700 */
LIBSSL_NOINLINE int sslEvpSha512Final(u8 *outDigest, void *mdCtx)
{
    return sslSha512Final((SceSha512Context *)mdCtx, outDigest);
}

/* -------------------------------------------------------------------------
 * EVP_Digest* core [111..113]
 * ------------------------------------------------------------------------- */

/* [111] 0x00c724 */
LIBSSL_NOINLINE int sslEvpDigestInit(SslEvpMdCtx *ctx, const SslEvpMd *digestType)
{
    const SslEvpMd *md = digestType;

    if (md != NULL) {
        ctx->digest = md;
    } else {
        md = ctx->digest;
    }

    if (md == NULL) {
        sceSsl_lib_9CDCA0D7(6, 118, 112, NULL, 0);
        return 0;
    }

    if (ctx->mdData == NULL || ctx->mdDataSize < md->ctxSize) {
        if (ctx->mdData != NULL) {
            sceSsl_lib_5E5C873A(ctx->mdData);
        }
        ctx->mdData = sceSsl_lib_0861D934((u32)md->ctxSize);
        if (ctx->mdData == NULL) {
            sceSsl_lib_9CDCA0D7(6, 118, 33, NULL, 0);
            return 0;
        }
    }

    ctx->mdDataSize = md->ctxSize;
    md->init(ctx->mdData);
    return 1;
}

/* [112] 0x00c7e0 */
LIBSSL_NOINLINE void sslEvpDigestUpdate(SslEvpMdCtx *ctx, const void *input, u32 inputLen)
{
    ctx->digest->update(ctx->mdData, input, inputLen);
}

/* [113] 0x00c804 */
LIBSSL_NOINLINE void sslEvpDigestFinal(SslEvpMdCtx *ctx, u8 *outDigest, u32 *outSize)
{
    ctx->digest->finalFn(outDigest, ctx->mdData);
    if (outSize != NULL) {
        *outSize = (u32)ctx->digest->mdSize;
    }
    sslMemset(ctx->mdData, 0, (u32)ctx->mdDataSize);
}

/* -------------------------------------------------------------------------
 * Cipher implementations and EVP_CIPHER descriptors [114..134]
 * ------------------------------------------------------------------------- */

typedef struct SslDesEde3CipherData {
    SslDesKeySchedule ksOne;
    u32 padOne[16];
    SslDesKeySchedule ksTwo;
    u32 padTwo[16];
    SslDesKeySchedule ksThree;
    u32 padThree[16];
} SslDesEde3CipherData;

typedef struct SslRc4CipherData {
    u8 keyBytes[16];
    SslRc4Key keySched;
} SslRc4CipherData;

/* Forward declarations for static cipher descriptors */
static void sslEvpDesEde3InitKey(SslEvpCipherCtx *ctx, const u8 *key, const u8 *iv, int enc);
static void sslEvpDesEde3CbcCipher(SslEvpCipherCtx *ctx, u8 *out, const u8 *in, u32 len);
static void sslEvpDesInitKey(SslEvpCipherCtx *ctx, const u8 *key, const u8 *iv, int enc);
static void sslEvpDesCbcCipher(SslEvpCipherCtx *ctx, u8 *out, const u8 *in, u32 len);
static void sslEvpRc2InitKey(SslEvpCipherCtx *ctx, const u8 *key, const u8 *iv, int enc);
static void sslEvpRc2CbcCipher(SslEvpCipherCtx *ctx, u8 *out, const u8 *in, u32 len);
static int sslEvpRc2GetAsn1Params(SslEvpCipherCtx *ctx, SslAsn1Type *type);
static int sslEvpRc2SetAsn1Params(SslEvpCipherCtx *ctx, SslAsn1Type *type);
static void sslEvpNullInitKey(SslEvpCipherCtx *ctx, const u8 *key, const u8 *iv, int enc);
static void sslEvpNullCipher(SslEvpCipherCtx *ctx, u8 *out, const u8 *in, u32 len);
static void sslEvpRc4InitKey(SslEvpCipherCtx *ctx, const u8 *key, const u8 *iv, int enc);
static void sslEvpRc4Cipher(SslEvpCipherCtx *ctx, u8 *out, const u8 *in, u32 len);

static const SslEvpCipher g_evpDesEde3Cbc = {
    44, 8, 24, 8,
    sslEvpDesEde3InitKey, sslEvpDesEde3CbcCipher, NULL, 576,
    sslEvpCipherSetAsn1Iv, sslEvpCipherGetAsn1Iv
};

static const SslEvpCipher g_evpDesCbc = {
    31, 8, 8, 8,
    sslEvpDesInitKey, sslEvpDesCbcCipher, NULL, 192,
    sslEvpCipherGetAsn1Iv, sslEvpCipherSetAsn1Iv
};

static const SslEvpCipher g_evpRc2Cbc = {
    37, 8, 16, 8,
    sslEvpRc2InitKey, sslEvpRc2CbcCipher, NULL, 128,
    sslEvpRc2SetAsn1Params, sslEvpRc2GetAsn1Params
};

static const SslEvpCipher g_evpRc2_64Cbc = {
    98, 8, 8, 8,
    sslEvpRc2InitKey, sslEvpRc2CbcCipher, NULL, 128,
    sslEvpRc2SetAsn1Params, sslEvpRc2GetAsn1Params
};

static const SslEvpCipher g_evpRc2_40Cbc = {
    98, 8, 5, 8,
    sslEvpRc2InitKey, sslEvpRc2CbcCipher, NULL, 128,
    sslEvpRc2SetAsn1Params, sslEvpRc2GetAsn1Params
};

static const SslEvpCipher g_evpEncNull = {
    0, 1, 0, 0,
    sslEvpNullInitKey, sslEvpNullCipher, NULL, 0,
    NULL, NULL
};

static const SslEvpCipher g_evpRc4 = {
    5, 1, 16, 0,
    sslEvpRc4InitKey, sslEvpRc4Cipher, NULL, 1048,
    NULL, NULL
};

/* [114] 0x00c864 */
LIBSSL_NOINLINE const SslEvpCipher *sslEvpDesEde3Cbc(void)
{
    return &g_evpDesEde3Cbc;
}

/* [115] 0x00c870 */
LIBSSL_NOINLINE static void sslEvpDesEde3InitKey(SslEvpCipherCtx *ctx, const u8 *key,
                                                 const u8 *iv, int enc)
{
    SslDesEde3CipherData *ks = (SslDesEde3CipherData *)ctx->cipherData;
    (void)enc;

    if (iv != NULL) {
        __builtin_memcpy(ctx->oiv, iv, 8);
    }
    __builtin_memcpy(ctx->iv, ctx->oiv, 8);
    if (key != NULL) {
        sslDesSetKey(key, &ks->ksOne);
        sslDesSetKey(key + 8, &ks->ksTwo);
        sslDesSetKey(key + 16, &ks->ksThree);
    }
}

/* [116] 0x00c900 */
LIBSSL_NOINLINE static void sslEvpDesEde3CbcCipher(SslEvpCipherCtx *ctx, u8 *out,
                                                   const u8 *in, u32 len)
{
    SslDesEde3CipherData *ks = (SslDesEde3CipherData *)ctx->cipherData;
    sslDesEde3CbcEncrypt(in, out, (int)len,
                         &ks->ksOne, &ks->ksTwo, &ks->ksThree,
                         ctx->iv, ctx->encrypt);
}

/* [117] 0x00c93c */
LIBSSL_NOINLINE const SslEvpCipher *sslEvpDesCbc(void)
{
    return &g_evpDesCbc;
}

/* [118] 0x00c948 */
LIBSSL_NOINLINE static void sslEvpDesInitKey(SslEvpCipherCtx *ctx, const u8 *key,
                                             const u8 *iv, int enc)
{
    SslDesKeySchedule *ks = (SslDesKeySchedule *)ctx->cipherData;
    (void)enc;

    if (iv != NULL) {
        __builtin_memcpy(ctx->oiv, iv, 8);
    }
    __builtin_memcpy(ctx->iv, ctx->oiv, 8);
    if (key != NULL) {
        sslDesSetKey(key, ks);
    }
}

/* [119] 0x00c9ac */
LIBSSL_NOINLINE static void sslEvpDesCbcCipher(SslEvpCipherCtx *ctx, u8 *out,
                                               const u8 *in, u32 len)
{
    SslDesKeySchedule *ks = (SslDesKeySchedule *)ctx->cipherData;
    sslDesNcbcEncrypt(in, out, (int)len, ks, ctx->iv, ctx->encrypt);
}

/* [120] 0x00c9e0 */
LIBSSL_NOINLINE const SslEvpCipher *sslEvpRc2Cbc(void)
{
    return &g_evpRc2Cbc;
}

/* [121] 0x00c9ec */
LIBSSL_NOINLINE const SslEvpCipher *sslEvpRc2_64Cbc(void)
{
    return &g_evpRc2_64Cbc;
}

/* [122] 0x00c9f8 */
LIBSSL_NOINLINE const SslEvpCipher *sslEvpRc2_40Cbc(void)
{
    return &g_evpRc2_40Cbc;
}

/* [123] 0x00ca04 */
LIBSSL_NOINLINE static void sslEvpRc2InitKey(SslEvpCipherCtx *ctx, const u8 *key,
                                             const u8 *iv, int enc)
{
    SslRc2Key *ks = (SslRc2Key *)ctx->cipherData;
    (void)enc;

    if (iv != NULL) {
        __builtin_memcpy(ctx->oiv, iv, 8);
    }
    __builtin_memcpy(ctx->iv, ctx->oiv, 8);
    if (key != NULL) {
        int keyBits = sslEvpCipherCtxEffectiveKeyBits(ctx);
        int keyLen;
        if (keyBits == 0) {
            keyBits = 8 * sslEvpCipherCtxKeyLength(ctx);
        }
        keyLen = sslEvpCipherCtxKeyLength(ctx);
        sslRc2SetKey(ks, keyLen, key, keyBits);
    }
}

/* [124] 0x00cab4 */
LIBSSL_NOINLINE static void sslEvpRc2CbcCipher(SslEvpCipherCtx *ctx, u8 *out,
                                               const u8 *in, u32 len)
{
    SslRc2Key *ks = (SslRc2Key *)ctx->cipherData;
    sslRc2CbcEncrypt(in, out, (int)len, ks, ctx->iv, ctx->encrypt);
}

/* [125] 0x00cae8 */
LIBSSL_NOINLINE int sslEvpRc2KeyBitsToMagic(const SslEvpCipher *cipher)
{
    int keyLen = sslEvpCipherKeyLength(cipher);
    if (keyLen == 128) {
        return 58;
    }
    if (keyLen == 64) {
        return 120;
    }
    if (keyLen == 40) {
        return 160;
    }
    return 0;
}

/* [126] 0x00cb30 */
LIBSSL_NOINLINE const SslEvpCipher *sslEvpRc2MagicToCipher(int magic)
{
    switch (magic) {
    case 58:
        return sslEvpRc2Cbc();
    case 120:
        return sslEvpRc2_64Cbc();
    case 160:
        return sslEvpRc2_40Cbc();
    default:
        break;
    }
    sceSsl_lib_9CDCA0D7(6, 110, 109, NULL, 0);
    return NULL;
}

/* [127] 0x00cbb0 */
LIBSSL_NOINLINE static int sslEvpRc2GetAsn1Params(SslEvpCipherCtx *ctx, SslAsn1Type *type)
{
    int magic = 0;
    int ivLen;
    int gotLen;
    const SslEvpCipher *rc2Cipher;

    if (type == NULL) {
        return 0;
    }
    ivLen = sslEvpCipherCtxIvLength(ctx);
    gotLen = sslAsn1TypeGetIntOctetstring(type, &magic, ctx->oiv, ivLen);
    if (gotLen != ivLen) {
        return -1;
    }
    if (gotLen > 0) {
        sslMemcpy(ctx->iv, ctx->oiv, (u32)gotLen);
    }
    rc2Cipher = sslEvpRc2MagicToCipher(magic);
    if (rc2Cipher == NULL) {
        return -1;
    }
    if (rc2Cipher == sslEvpCipherCtxCipher(ctx)) {
        return gotLen;
    }
    if (!sslEvpCipherCtxSetCipher(ctx, rc2Cipher)) {
        return -1;
    }
    sslEvpRc2InitKey(ctx, NULL, NULL, 0);
    return gotLen;
}

/* [128] 0x00cca4 */
LIBSSL_NOINLINE static int sslEvpRc2SetAsn1Params(SslEvpCipherCtx *ctx, SslAsn1Type *type)
{
    int magic;
    int ivLen;

    if (type == NULL) {
        return 0;
    }
    magic = sslEvpRc2KeyBitsToMagic(sslEvpCipherCtxCipher(ctx));
    ivLen = sslEvpCipherCtxIvLength(ctx);
    return sslAsn1TypeSetIntOctetstring(type, magic, ctx->oiv, ivLen);
}

/* [129] 0x00cd10 */
LIBSSL_NOINLINE const SslEvpCipher *sslEvpEncNull(void)
{
    return &g_evpEncNull;
}

/* [130] 0x00cd1c */
LIBSSL_NOINLINE static void sslEvpNullInitKey(SslEvpCipherCtx *ctx, const u8 *key,
                                              const u8 *iv, int enc)
{
    (void)key;
    (void)iv;
    (void)enc;
    ctx->cipherDataSize = 0;
    ctx->cipherData = NULL;
}

/* [131] 0x00cd28 */
LIBSSL_NOINLINE static void sslEvpNullCipher(SslEvpCipherCtx *ctx, u8 *out,
                                             const u8 *in, u32 len)
{
    (void)ctx;
    if (in != out) {
        sslMemcpy(out, in, len);
    }
}

/* [132] 0x00cd5c */
LIBSSL_NOINLINE const SslEvpCipher *sslEvpRc4(void)
{
    return &g_evpRc4;
}

/* [133] 0x00cd68 */
LIBSSL_NOINLINE static void sslEvpRc4InitKey(SslEvpCipherCtx *ctx, const u8 *key,
                                             const u8 *iv, int enc)
{
    SslRc4CipherData *rc4Data = (SslRc4CipherData *)ctx->cipherData;
    int keyLen;
    (void)iv;
    (void)enc;

    if (key != NULL) {
        keyLen = sslEvpCipherCtxKeyLength(ctx);
        sslMemcpy(rc4Data->keyBytes, key, (u32)keyLen);
    }
    keyLen = sslEvpCipherCtxKeyLength(ctx);
    sslRc4SetKey(&rc4Data->keySched, keyLen, rc4Data->keyBytes);
}

/* [134] 0x00cdd4 */
LIBSSL_NOINLINE static void sslEvpRc4Cipher(SslEvpCipherCtx *ctx, u8 *out,
                                            const u8 *in, u32 len)
{
    SslRc4CipherData *rc4Data = (SslRc4CipherData *)ctx->cipherData;
    sslRc4Crypt(&rc4Data->keySched, len, in, out);
}

/* -------------------------------------------------------------------------
 * EVP_Decode* base64 decoding [135..140]
 * ------------------------------------------------------------------------- */

/* [135] 0x00ce00 */
LIBSSL_NOINLINE SslEvpEncodeCtx *sslEvpEncodeCtxNew(void)
{
    SslEvpEncodeCtx *ctx = (SslEvpEncodeCtx *)sceSsl_lib_0861D934(sizeof(SslEvpEncodeCtx));
    if (ctx == NULL) {
        sceSsl_lib_9CDCA0D7(6, 117, 33, NULL, 0);
        return NULL;
    }
    sslMemset(ctx, 0, sizeof(SslEvpEncodeCtx));
    return ctx;
}

/* [136] 0x00ce64 */
LIBSSL_NOINLINE void sslEvpEncodeCtxFree(SslEvpEncodeCtx *ctx)
{
    sceSsl_lib_5E5C873A(ctx);
}

/* [137] 0x00ce80 */
LIBSSL_NOINLINE void sslEvpDecodeInit(SslEvpEncodeCtx *ctx)
{
    ctx->length = 30;
    ctx->num = 0;
    ctx->lineNum = 0;
    ctx->expectNl = 0;
}

/* [138] 0x00ce98 */
LIBSSL_NOINLINE int sslEvpDecodeUpdate(SslEvpEncodeCtx *ctx, u8 *outBuf, int *outLen,
                                       const u8 *inBuf, int inLen)
{
    int eofCount = 0;
    int firstEqPos = -1;
    int totalOut = 0;
    int buffered = ctx->num;
    int lineNum = ctx->lineNum;
    int expectNl = ctx->expectNl;
    int ret = 0;
    int pos;

    if (inLen != 0 && (buffered != 0 || g_b64DecodeTable[inBuf[0] & 0x7f] != 0xf2)) {
        ret = 1;
        for (pos = 0; pos < inLen; pos++) {
            u8 ch;
            u8 code;

            if (lineNum >= 80) {
                ret = -1;
                break;
            }
            ch = *inBuf++;
            code = g_b64DecodeTable[ch & 0x7f];
            if ((code | 0x13) == 0xf3) {
                if (code == 0xff) {
                    ret = -1;
                    break;
                }
            } else {
                lineNum++;
                ctx->encData[buffered++] = ch;
            }

            if (ch == '=') {
                if (firstEqPos == -1) {
                    firstEqPos = buffered;
                }
                eofCount++;
            }

            if (code == 0xf0) {
                lineNum = 0;
                if (expectNl != 0) {
                    expectNl = 0;
                    continue;
                }
            }
            expectNl = 0;

            if (pos + 1 == inLen && ((buffered & 3) == 0 || eofCount != 0)) {
                code = 0xf2;
            }

            if (code == 0xf2 || buffered >= 64) {
                int decoded;
                expectNl = (code != 0xf2 && buffered >= 64) ? 1 : 0;
                if (buffered <= 0) {
                    eofCount = 1;
                    decoded = 0;
                } else {
                    decoded = sslEvpDecodeBlock(outBuf, ctx->encData, buffered);
                    if (decoded < 0) {
                        ret = 0;
                        break;
                    }
                    buffered = 0;
                    totalOut += decoded - eofCount;
                }
                if (decoded < ctx->length && eofCount != 0) {
                    ret = 0;
                    break;
                }
                ctx->length = decoded;
                if (firstEqPos >= 0) {
                    ret = 0;
                    break;
                }
                outBuf += decoded;
            }
        }
    }

    *outLen = totalOut;
    ctx->expectNl = expectNl;
    ctx->num = buffered;
    ctx->lineNum = lineNum;
    return ret;
}

/* [139] 0x00d0d0 */
LIBSSL_NOINLINE int sslEvpDecodeBlock(u8 *outBuf, const u8 *inBuf, int inLen)
{
    int outCount = 0;
    int pos;

    while (inLen > 0 && g_b64DecodeTable[inBuf[0] & 0x7f] == 0xe0) {
        inBuf++;
        inLen--;
    }

    while (inLen >= 4 && (g_b64DecodeTable[inBuf[inLen - 1] & 0x7f] | 0x13) == 0xf3) {
        inLen--;
    }

    if ((inLen & 3) != 0) {
        return -1;
    }

    for (pos = 0; pos < inLen; pos += 4) {
        u8 codeA = g_b64DecodeTable[inBuf[0] & 0x7f];
        u8 codeB = g_b64DecodeTable[inBuf[1] & 0x7f];
        u8 codeC = g_b64DecodeTable[inBuf[2] & 0x7f];
        u8 codeD = g_b64DecodeTable[inBuf[3] & 0x7f];
        u32 packed;

        inBuf += 4;
        if ((codeA & 0x80) != 0 || (codeB & 0x80) != 0 ||
            (codeC & 0x80) != 0 || (codeD & 0x80) != 0) {
            return -1;
        }
        packed = ((u32)codeA << 18) | ((u32)codeB << 12) | ((u32)codeC << 6) | (u32)codeD;
        outBuf[0] = (u8)(packed >> 16);
        outBuf[1] = (u8)(packed >> 8);
        outBuf[2] = (u8)packed;
        outBuf += 3;
        outCount += 3;
    }

    return outCount;
}

/* [140] 0x00d290 */
LIBSSL_NOINLINE int sslEvpDecodeFinal(SslEvpEncodeCtx *ctx, u8 *outBuf, int *outLen)
{
    *outLen = 0;
    if (ctx->num != 0) {
        int decoded = sslEvpDecodeBlock(outBuf, ctx->encData, ctx->num);
        if (decoded < 0) {
            return -1;
        }
        ctx->num = 0;
        *outLen = decoded;
    }
    return 1;
}

/* -------------------------------------------------------------------------
 * EVP_MD descriptors [141..150]
 * ------------------------------------------------------------------------- */

static const SslEvpMd g_evpMd2Rsa = {
    3, 7, 16,
    (SslMdInitFn)sslMd2Init, (SslMdUpdateFn)sslMd2Update, (SslMdFinalFn)sslMd2Final,
    sslRsaSign, sslRsaVerify,
    { 6, 19, 0, 0, 0 }, 16, 152
};

static const SslEvpMd g_evpMd2 = {
    3, 0, 16,
    (SslMdInitFn)sslMd2Init, (SslMdUpdateFn)sslMd2Update, (SslMdFinalFn)sslMd2Final,
    NULL, NULL,
    { 0, 0, 0, 0, 0 }, 16, 152
};

static const SslEvpMd g_evpMd5Rsa = {
    4, 8, 16,
    (SslMdInitFn)sceKernelUtilsMd5BlockInit, (SslMdUpdateFn)sceKernelUtilsMd5BlockUpdate,
    (SslMdFinalFn)sslEvpMd5Final,
    sslRsaSign, sslRsaVerify,
    { 6, 19, 0, 0, 0 }, 64, 100
};

static const SslEvpMd g_evpMd5 = {
    4, 0, 16,
    (SslMdInitFn)sceKernelUtilsMd5BlockInit, (SslMdUpdateFn)sceKernelUtilsMd5BlockUpdate,
    (SslMdFinalFn)sslEvpMd5Final,
    NULL, NULL,
    { 0, 0, 0, 0, 0 }, 64, 100
};

static const SslEvpMd g_evpSha1Rsa = {
    64, 65, 20,
    (SslMdInitFn)sceKernelUtilsSha1BlockInit, (SslMdUpdateFn)sceKernelUtilsSha1BlockUpdate,
    (SslMdFinalFn)sslEvpSha1Final,
    sslRsaSign, sslRsaVerify,
    { 6, 19, 0, 0, 0 }, 64, 100
};

static const SslEvpMd g_evpSha1 = {
    64, 0, 20,
    (SslMdInitFn)sceKernelUtilsSha1BlockInit, (SslMdUpdateFn)sceKernelUtilsSha1BlockUpdate,
    (SslMdFinalFn)sslEvpSha1Final,
    NULL, NULL,
    { 0, 0, 0, 0, 0 }, 64, 100
};

static const SslEvpMd g_evpSha224 = {
    126, 133, 28,
    (SslMdInitFn)sslSha224Init, (SslMdUpdateFn)sslSha224Update, (SslMdFinalFn)sslEvpSha224Final,
    sslRsaSign, sslRsaVerify,
    { 6, 19, 0, 0, 0 }, 64, 116
};

static const SslEvpMd g_evpSha256 = {
    127, 130, 32,
    (SslMdInitFn)sslSha256Init, (SslMdUpdateFn)sslSha256Update, (SslMdFinalFn)sslEvpSha256Final,
    sslRsaSign, sslRsaVerify,
    { 6, 19, 0, 0, 0 }, 64, 116
};

static const SslEvpMd g_evpSha384 = {
    128, 131, 48,
    (SslMdInitFn)sslSha384Init, (SslMdUpdateFn)sslSha384Update, (SslMdFinalFn)sslEvpSha384Final,
    sslRsaSign, sslRsaVerify,
    { 6, 19, 0, 0, 0 }, 128, 212
};

static const SslEvpMd g_evpSha512 = {
    129, 132, 64,
    (SslMdInitFn)sslSha512Init, (SslMdUpdateFn)sslSha512Update, (SslMdFinalFn)sslEvpSha512Final,
    sslRsaSign, sslRsaVerify,
    { 6, 19, 0, 0, 0 }, 128, 212
};

/* [141] 0x00d2fc */
LIBSSL_NOINLINE const SslEvpMd *sslEvpMd2Rsa(void) { return &g_evpMd2Rsa; }
/* [142] 0x00d308 */
LIBSSL_NOINLINE const SslEvpMd *sslEvpMd2(void) { return &g_evpMd2; }
/* [143] 0x00d314 */
LIBSSL_NOINLINE const SslEvpMd *sslEvpMd5Rsa(void) { return &g_evpMd5Rsa; }
/* [144] 0x00d320 */
LIBSSL_NOINLINE const SslEvpMd *sslEvpMd5(void) { return &g_evpMd5; }
/* [145] 0x00d32c */
LIBSSL_NOINLINE const SslEvpMd *sslEvpSha1Rsa(void) { return &g_evpSha1Rsa; }
/* [146] 0x00d338 */
LIBSSL_NOINLINE const SslEvpMd *sslEvpSha1(void) { return &g_evpSha1; }
/* [147] 0x00d344 */
LIBSSL_NOINLINE const SslEvpMd *sslEvpSha224(void) { return &g_evpSha224; }
/* [148] 0x00d350 */
LIBSSL_NOINLINE const SslEvpMd *sslEvpSha256(void) { return &g_evpSha256; }
/* [149] 0x00d35c */
LIBSSL_NOINLINE const SslEvpMd *sslEvpSha384(void) { return &g_evpSha384; }
/* [150] 0x00d368 */
LIBSSL_NOINLINE const SslEvpMd *sslEvpSha512(void) { return &g_evpSha512; }

/* -------------------------------------------------------------------------
 * EVP name registry [151..155]
 * ------------------------------------------------------------------------- */

/* [151] 0x00d374 */
LIBSSL_NOINLINE int sslEvpAddCipher(const SslEvpCipher *cipher)
{
    int ret;

    ret = sslObjNameAdd(sslObjNidToSn((u32)cipher->nid), 2, (const void *)cipher);
    if (ret == 0) {
        return 0;
    }
    return sslObjNameAdd(sslObjNidToLn((u32)cipher->nid), 2, (const void *)cipher);
}

/* [152] 0x00d3d4 */
LIBSSL_NOINLINE int sslEvpAddDigest(const SslEvpMd *digest)
{
    const char *shortName;
    int ret;

    shortName = sslObjNidToSn((u32)digest->type);
    ret = sslObjNameAdd(shortName, 1, (const void *)digest);
    if (ret == 0) {
        return 0;
    }
    ret = sslObjNameAdd(sslObjNidToLn((u32)digest->type), 1, (const void *)digest);
    if (ret == 0) {
        return 0;
    }
    if (digest->pkeyType != 0 && digest->type != digest->pkeyType) {
        ret = sslObjNameAdd(sslObjNidToSn((u32)digest->pkeyType), 0x8001, shortName);
        if (ret == 0) {
            return 0;
        }
        return sslObjNameAdd(sslObjNidToLn((u32)digest->pkeyType), 0x8001, shortName);
    }
    return ret;
}

/* [153] 0x00d4a4 */
LIBSSL_NOINLINE const SslEvpCipher *sslEvpGetCipherByName(const char *name)
{
    return (const SslEvpCipher *)sslObjNameGet(name, 2);
}

/* [154] 0x00d4c0 */
LIBSSL_NOINLINE const SslEvpMd *sslEvpGetDigestByName(const char *name)
{
    return (const SslEvpMd *)sslObjNameGet(name, 1);
}

/* [155] 0x00d4dc */
LIBSSL_NOINLINE void sslEvpCleanup(void)
{
    sslObjNameCleanup(2);
    sslObjNameCleanup(1);
}

/* -------------------------------------------------------------------------
 * EVP_PKEY lifecycle [156..160]
 * ------------------------------------------------------------------------- */

/* [156] 0x00d500 */
LIBSSL_NOINLINE SslEvpPkey *sceSsl_lib_09C45275(void)
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

/* [157] 0x00d564 */
LIBSSL_NOINLINE int sceSsl_lib_FAF333D1(SslEvpPkey *pkey)
{
    return sslCryptoAddLock(&pkey->references, 1, 10, "../common/evp/p_lib.c", 168);
}

/* [158] 0x00d594 */
LIBSSL_NOINLINE int sslEvpPkeyType(int type)
{
    switch (type) {
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

/* [159] 0x00d614 */
LIBSSL_NOINLINE void sceSsl_lib_7317C1BC(SslEvpPkey *pkey)
{
    if (pkey == NULL) {
        return;
    }
    if (sslCryptoAddLock(&pkey->references, -1, 10, "../common/evp/p_lib.c", 219) > 0) {
        return;
    }
    sslEvpPkeyFreeInternal(pkey);
    sceSsl_lib_5E5C873A(pkey);
}

/* [160] 0x00d670 */
LIBSSL_NOINLINE void sslEvpPkeyFreeInternal(SslEvpPkey *pkey)
{
    if (pkey->type == 6 || pkey->type == 19) {
        sslRsaFree((SslRsa *)pkey->pkey);
    }
}

/* -------------------------------------------------------------------------
 * Binary search & EVP_CIPHER_CTX [161..175]
 * ------------------------------------------------------------------------- */

/* [161] 0x00d6a4 */
LIBSSL_NOINLINE void *sslEvpBsearch(const void *key, const void *basePtr, int elemCount,
                                    int elemSize, SslQsortCmpFn cmpFn)
{
    const u8 *baseBytes = (const u8 *)basePtr;
    int low = 0;
    int high = elemCount - 1;

    while (low < high) {
        int mid = (low + high) / 2;
        const u8 *elem = &baseBytes[mid * elemSize];
        int cmp = cmpFn(key, elem);
        if (cmp == 0) {
            return (void *)elem;
        }
        if (cmp > 0) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    if (low == high) {
        const u8 *elem = &baseBytes[low * elemSize];
        if (cmpFn(key, elem) == 0) {
            return (void *)elem;
        }
    }
    return NULL;
}

/* [162] 0x00d788 */
LIBSSL_NOINLINE void sslEvpCipher(SslEvpCipherCtx *ctx, u8 *out, const u8 *in, u32 len)
{
    ctx->cipher->doCipher(ctx, out, in, len);
}

/* [163] 0x00d7ac */
LIBSSL_NOINLINE int sslEvpCipherCtxSetCipher(SslEvpCipherCtx *ctx, const SslEvpCipher *cipher)
{
    if (ctx->cipher != NULL) {
        if (ctx->cipherData != NULL) {
            sslMemset(ctx->cipherData, 0, (u32)ctx->cipherDataSize);
        }
        if (cipher == NULL || ctx->cipherDataSize != cipher->ctxSize) {
            sceSsl_lib_5E5C873A(ctx->cipherData);
            ctx->cipherData = NULL;
        }
    }

    if (cipher != NULL) {
        if (ctx->cipherData == NULL) {
            ctx->cipherData = sceSsl_lib_0861D934((u32)cipher->ctxSize);
            if (ctx->cipherData == NULL) {
                sceSsl_lib_9CDCA0D7(6, 113, 33, NULL, 0);
                return 0;
            }
        }
        ctx->cipherDataSize = cipher->ctxSize;
        sslMemset(ctx->cipherData, 0, (u32)cipher->ctxSize);
        ctx->cipher = cipher;
        ctx->effectiveKeyBits = 0;
    } else {
        ctx->effectiveKeyBits = 0;
    }
    ctx->keyLen = 0;
    return 1;
}

/* [164] 0x00d88c */
LIBSSL_NOINLINE int sslEvpCipherCtxInit(SslEvpCipherCtx *ctx)
{
    ctx->encrypt = 1;
    ctx->bufLen = 0;
    sslMemset(ctx->oiv, 0, sizeof(ctx->oiv));
    sslMemset(ctx->iv, 0, sizeof(ctx->iv));
    sslMemset(ctx->buf, 0, sizeof(ctx->buf));
    ctx->num = 0;

    if (ctx->cipher != NULL) {
        if (ctx->cipherDataSize != ctx->cipher->ctxSize) {
            if (ctx->cipherData != NULL) {
                sslMemset(ctx->cipherData, 0, (u32)ctx->cipherDataSize);
                sceSsl_lib_5E5C873A(ctx->cipherData);
            }
            ctx->cipherData = sceSsl_lib_0861D934((u32)ctx->cipher->ctxSize);
            if (ctx->cipherData == NULL) {
                sceSsl_lib_9CDCA0D7(6, 111, 33, NULL, 0);
                return 0;
            }
            ctx->cipherDataSize = ctx->cipher->ctxSize;
        }
    }

    if (ctx->cipherData != NULL) {
        sslMemset(ctx->cipherData, 0, (u32)ctx->cipherDataSize);
    }
    ctx->effectiveKeyBits = 0;
    ctx->keyLen = 0;
    return 1;
}

/* [165] 0x00d980 */
LIBSSL_NOINLINE SslEvpCipherCtx *sslEvpCipherCtxNew(void)
{
    SslEvpCipherCtx *ctx = (SslEvpCipherCtx *)sceSsl_lib_0861D934(sizeof(SslEvpCipherCtx));
    if (ctx == NULL) {
        sceSsl_lib_9CDCA0D7(6, 112, 33, NULL, 0);
        return NULL;
    }
    ctx->cipher = NULL;
    ctx->cipherData = NULL;
    ctx->cipherDataSize = 0;
    ctx->appData = NULL;
    ctx->keyLen = 0;
    ctx->effectiveKeyBits = 0;
    if (!sslEvpCipherCtxInit(ctx)) {
        sceSsl_lib_5E5C873A(ctx);
        sceSsl_lib_9CDCA0D7(6, 112, 33, NULL, 0);
        return NULL;
    }
    return ctx;
}

/* [166] 0x00da00 */
LIBSSL_NOINLINE int sslEvpCipherInit(SslEvpCipherCtx *ctx, const SslEvpCipher *cipher,
                                     const u8 *key, const u8 *iv, int enc)
{
    if (enc < 0) {
        enc = ctx->encrypt;
    }
    if (enc != 0) {
        return sslEvpEncryptInit(ctx, cipher, key, iv);
    }
    return sslEvpDecryptInit(ctx, cipher, key, iv);
}

/* [167] 0x00da3c */
LIBSSL_NOINLINE int sslEvpEncryptInit(SslEvpCipherCtx *ctx, const SslEvpCipher *cipher,
                                      const u8 *key, const u8 *iv)
{
    if (cipher != NULL) {
        if (!sslEvpCipherCtxSetCipher(ctx, cipher)) {
            return 0;
        }
    }
    ctx->cipher->init(ctx, key, iv, 1);
    ctx->encrypt = 1;
    ctx->bufLen = 0;
    return 1;
}

/* [168] 0x00dab8 */
LIBSSL_NOINLINE int sslEvpDecryptInit(SslEvpCipherCtx *ctx, const SslEvpCipher *cipher,
                                      const u8 *key, const u8 *iv)
{
    if (cipher != NULL) {
        if (!sslEvpCipherCtxSetCipher(ctx, cipher)) {
            return 0;
        }
    }
    ctx->cipher->init(ctx, key, iv, 0);
    ctx->bufLen = 0;
    ctx->encrypt = 0;
    return 1;
}

/* [169] 0x00db30 */
LIBSSL_NOINLINE void sslEvpEncryptUpdate(SslEvpCipherCtx *ctx, u8 *out, int *outLen,
                                         const u8 *in, int inLen)
{
    int buffered = ctx->bufLen;
    int blockSize = ctx->cipher->blockSize;
    int rem;
    int fullLen;

    *outLen = 0;
    if (inLen == 0 && buffered != blockSize) {
        return;
    }

    if (buffered != 0) {
        int needed = blockSize - buffered;
        if (buffered + inLen < blockSize) {
            sslMemcpy(&ctx->buf[buffered], in, (u32)inLen);
            ctx->bufLen = buffered + inLen;
            return;
        }
        if (needed != 0) {
            sslMemcpy(&ctx->buf[buffered], in, (u32)needed);
        }
        ctx->cipher->doCipher(ctx, out, ctx->buf, (u32)blockSize);
        in += needed;
        inLen -= needed;
        *outLen += blockSize;
        out += blockSize;
    }

    rem = inLen % blockSize;
    fullLen = inLen - rem;
    if (fullLen > 0) {
        ctx->cipher->doCipher(ctx, out, in, (u32)fullLen);
        *outLen += fullLen;
    }
    if (rem != 0) {
        sslMemcpy(ctx->buf, in + fullLen, (u32)rem);
        ctx->bufLen = rem;
    } else {
        ctx->bufLen = 0;
    }
}

/* [170] 0x00dc9c */
LIBSSL_NOINLINE void sslEvpDecryptUpdate(SslEvpCipherCtx *ctx, u8 *out, int *outLen,
                                         const u8 *in, int inLen)
{
    int holdFinalBlock = 0;
    int blockSize;

    *outLen = 0;
    if (inLen == 0) {
        return;
    }

    blockSize = ctx->cipher->blockSize;
    if (blockSize >= 2 && ((inLen + ctx->bufLen) % blockSize) == 0) {
        if (inLen < blockSize) {
            sslMemcpy(&ctx->buf[ctx->bufLen], in, (u32)inLen);
            ctx->bufLen = blockSize;
            *outLen = 0;
            return;
        }
        inLen -= blockSize;
        holdFinalBlock = 1;
    }

    sslEvpEncryptUpdate(ctx, out, outLen, in, inLen);
    if (holdFinalBlock != 0) {
        sslMemcpy(ctx->buf, in + inLen, (u32)blockSize);
        ctx->bufLen = blockSize;
    }
}

/* [171] 0x00dd80 */
LIBSSL_NOINLINE int sslEvpDecryptFinal(SslEvpCipherCtx *ctx, u8 *out, int *outLen)
{
    int blockSize = ctx->cipher->blockSize;
    int decLen = 0;
    int padByte;
    int plainLen;
    int idx;

    *outLen = 0;
    if (blockSize < 2) {
        return 1;
    }
    if (ctx->bufLen != blockSize) {
        sceSsl_lib_9CDCA0D7(6, 101, 110, NULL, 0);
        return 0;
    }

    sslEvpEncryptUpdate(ctx, ctx->buf, &decLen, ctx->buf, 0);
    if (decLen != blockSize) {
        return 0;
    }

    padByte = ctx->buf[blockSize - 1];
    if (padByte > blockSize) {
        sceSsl_lib_9CDCA0D7(6, 101, 100, NULL, 0);
        return 0;
    }

    for (idx = 0; idx < padByte; idx++) {
        if (ctx->buf[blockSize - 1 - idx] != (u8)padByte) {
            sceSsl_lib_9CDCA0D7(6, 101, 100, NULL, 0);
            return 0;
        }
    }

    plainLen = ctx->cipher->blockSize - padByte;
    for (idx = 0; idx < plainLen; idx++) {
        out[idx] = ctx->buf[idx];
    }
    *outLen = plainLen;
    return 1;
}

/* [172] 0x00debc */
LIBSSL_NOINLINE void sslEvpCipherCtxFree(SslEvpCipherCtx *ctx)
{
    sslEvpCipherCtxCleanup(ctx);
    if (ctx->cipherData != NULL) {
        sceSsl_lib_5E5C873A(ctx->cipherData);
    }
    sceSsl_lib_5E5C873A(ctx);
}

/* [173] 0x00defc */
LIBSSL_NOINLINE void sslEvpCipherCtxCleanup(SslEvpCipherCtx *ctx)
{
    if (ctx->cipher != NULL && ctx->cipher->cleanup != NULL) {
        ctx->cipher->cleanup(ctx);
    }
    if (ctx->cipherData != NULL) {
        sslMemset(ctx->cipherData, 0, (u32)ctx->cipherDataSize);
        sceSsl_lib_5E5C873A(ctx->cipherData);
        ctx->cipherData = NULL;
        ctx->cipherDataSize = 0;
    }
    sslMemset(ctx, 0, sizeof(SslEvpCipherCtx));
}

/* [174] 0x00df70 */
LIBSSL_NOINLINE u8 *sslEvpCipherCtxIv(SslEvpCipherCtx *ctx)
{
    return ctx->iv;
}

/* [175] 0x00df78 */
LIBSSL_NOINLINE int sslEvpBytesToKey(const SslEvpCipher *cipher, const SslEvpMd *md,
                                     const u8 *salt, const u8 *password, int passwordLen,
                                     u32 iterCount, u8 *outKey, u8 *outIv)
{
    u8 mdBuf[32];
    u32 mdLen = 0;
    int keyRem = cipher->keyLen;
    int ivRem = cipher->ivLen;
    int addMd = 0;
    SslEvpMdCtx *mdCtx;

    if (password == NULL) {
        return keyRem;
    }

    mdCtx = sslEvpMdCtxNew();
    if (mdCtx == NULL) {
        return 0;
    }

    do {
        u32 iter;
        u32 pos;

        addMd++;
        if (!sslEvpDigestInit(mdCtx, md)) {
            sslEvpMdCtxFree(mdCtx);
            return 0;
        }
        if (addMd != 1) {
            sslEvpDigestUpdate(mdCtx, mdBuf, mdLen);
        }
        sslEvpDigestUpdate(mdCtx, password, (u32)passwordLen);
        if (salt != NULL) {
            sslEvpDigestUpdate(mdCtx, salt, 8);
        }
        sslEvpDigestFinal(mdCtx, mdBuf, &mdLen);

        for (iter = 1; iter < iterCount; iter++) {
            sslEvpDigestInit(mdCtx, md);
            sslEvpDigestUpdate(mdCtx, mdBuf, mdLen);
            sslEvpDigestFinal(mdCtx, mdBuf, &mdLen);
        }

        pos = 0;
        while (keyRem != 0 && pos != mdLen) {
            if (outKey != NULL) {
                *outKey++ = mdBuf[pos];
            }
            keyRem--;
            pos++;
        }

        while (ivRem != 0 && pos != mdLen) {
            if (outIv != NULL) {
                *outIv++ = mdBuf[pos];
            }
            ivRem--;
            pos++;
        }
    } while (keyRem != 0 || ivRem != 0);

    sslEvpMdCtxFree(mdCtx);
    sslMemset(mdBuf, 0, 20);
    return cipher->keyLen;
}

/* -------------------------------------------------------------------------
 * Generic ASN.1 IV helpers & EVP accessors [176..188]
 * ------------------------------------------------------------------------- */

/* [176] 0x00e194 */
LIBSSL_NOINLINE int sslEvpCipherGetAsn1Iv(SslEvpCipherCtx *ctx, SslAsn1Type *type)
{
    int ivLen;
    int gotLen = 0;

    if (type != NULL) {
        ivLen = sslEvpCipherCtxIvLength(ctx);
        gotLen = sslAsn1TypeGetOctetstring(type, ctx->oiv, ivLen);
        if (gotLen != ivLen) {
            return -1;
        }
        if (gotLen > 0) {
            sslMemcpy(ctx->iv, ctx->oiv, (u32)gotLen);
        }
    }
    return gotLen;
}

/* [177] 0x00e228 */
LIBSSL_NOINLINE int sslEvpCipherSetAsn1Iv(SslEvpCipherCtx *ctx, SslAsn1Type *type)
{
    int ivLen;

    if (type == NULL) {
        return 0;
    }
    ivLen = sslEvpCipherCtxIvLength(ctx);
    return sslAsn1TypeSetOctetstring(type, ctx->oiv, ivLen);
}

/* [178] 0x00e274 */
LIBSSL_NOINLINE int sslEvpMdSize(const SslEvpMd *md)
{
    return md->mdSize;
}

/* [179] 0x00e27c */
LIBSSL_NOINLINE int sslEvpMdBlockSize(const SslEvpMd *md)
{
    return md->blockSize;
}

/* [180] 0x00e284 */
LIBSSL_NOINLINE int sslEvpMdCtxSize(const SslEvpMdCtx *ctx)
{
    return ctx->digest->mdSize;
}

/* [181] 0x00e290 */
LIBSSL_NOINLINE int sslEvpCipherNid(const SslEvpCipher *cipher)
{
    return cipher->nid;
}

/* [182] 0x00e298 */
LIBSSL_NOINLINE int sslEvpCipherKeyLength(const SslEvpCipher *cipher)
{
    return cipher->keyLen;
}

/* [183] 0x00e2a0 */
LIBSSL_NOINLINE int sslEvpCipherIvLength(const SslEvpCipher *cipher)
{
    return cipher->ivLen;
}

/* [184] 0x00e2a8 */
LIBSSL_NOINLINE const SslEvpCipher *sslEvpCipherCtxCipher(const SslEvpCipherCtx *ctx)
{
    return ctx->cipher;
}

/* [185] 0x00e2b0 */
LIBSSL_NOINLINE int sslEvpCipherCtxBlockSize(const SslEvpCipherCtx *ctx)
{
    return ctx->cipher->blockSize;
}

/* [186] 0x00e2bc */
LIBSSL_NOINLINE int sslEvpCipherCtxKeyLength(const SslEvpCipherCtx *ctx)
{
    if (ctx->keyLen != 0) {
        return ctx->keyLen;
    }
    return ctx->cipher->keyLen;
}

/* [187] 0x00e2d8 */
LIBSSL_NOINLINE int sslEvpCipherCtxEffectiveKeyBits(const SslEvpCipherCtx *ctx)
{
    return ctx->effectiveKeyBits;
}

/* [188] 0x00e2e0 */
LIBSSL_NOINLINE int sslEvpCipherCtxIvLength(const SslEvpCipherCtx *ctx)
{
    return ctx->cipher->ivLen;
}

/* -------------------------------------------------------------------------
 * EVP_MD_CTX lifecycle [189..193]
 * ------------------------------------------------------------------------- */

/* [189] 0x00e2ec */
LIBSSL_NOINLINE int sslEvpMdCtxSetDigest(SslEvpMdCtx *ctx, const SslEvpMd *digest)
{
    if (ctx->digest != NULL) {
        sslMemset(ctx->mdData, 0, (u32)ctx->mdDataSize);
        if (ctx->mdDataSize != digest->ctxSize) {
            sceSsl_lib_5E5C873A(ctx->mdData);
            ctx->mdData = NULL;
            ctx->mdDataSize = 0;
        }
    }

    if (digest != NULL) {
        if (ctx->mdData == NULL) {
            ctx->mdData = sceSsl_lib_0861D934((u32)digest->ctxSize);
            if (ctx->mdData == NULL) {
                sceSsl_lib_9CDCA0D7(6, 116, 33, NULL, 0);
                return 0;
            }
        }
        ctx->digest = digest;
        ctx->mdDataSize = digest->ctxSize;
    } else {
        ctx->digest = NULL;
    }

    if (ctx->mdDataSize > 0) {
        sslMemset(ctx->mdData, 0, (u32)ctx->mdDataSize);
    }
    return 1;
}

/* [190] 0x00e3c0 */
LIBSSL_NOINLINE int sslEvpMdCtxInit(SslEvpMdCtx *ctx)
{
    ctx->flags = 0;
    if (ctx->digest != NULL) {
        if (ctx->mdDataSize != ctx->digest->ctxSize) {
            if (ctx->mdData != NULL) {
                sslMemset(ctx->mdData, 0, (u32)ctx->mdDataSize);
                sceSsl_lib_5E5C873A(ctx->mdData);
            }
            ctx->mdData = sceSsl_lib_0861D934((u32)ctx->digest->ctxSize);
            if (ctx->mdData == NULL) {
                sceSsl_lib_9CDCA0D7(6, 122, 33, NULL, 0);
                return 0;
            }
            ctx->mdDataSize = ctx->digest->ctxSize;
        }
    }
    if (ctx->mdData != NULL) {
        sslMemset(ctx->mdData, 0, (u32)ctx->mdDataSize);
    }
    return 1;
}

/* [191] 0x00e474 */
LIBSSL_NOINLINE SslEvpMdCtx *sslEvpMdCtxNew(void)
{
    SslEvpMdCtx *ctx = (SslEvpMdCtx *)sceSsl_lib_0861D934(sizeof(SslEvpMdCtx));
    if (ctx == NULL) {
        sceSsl_lib_9CDCA0D7(6, 115, 33, NULL, 0);
        return NULL;
    }
    ctx->digest = NULL;
    ctx->mdData = NULL;
    ctx->mdDataSize = 0;
    if (!sslEvpMdCtxInit(ctx)) {
        sceSsl_lib_5E5C873A(ctx);
        sceSsl_lib_9CDCA0D7(6, 115, 33, NULL, 0);
        return NULL;
    }
    return ctx;
}

/* [192] 0x00e4e8 */
LIBSSL_NOINLINE void sslEvpMdCtxFree(SslEvpMdCtx *ctx)
{
    if (ctx->mdData != NULL) {
        sslMemset(ctx->mdData, 0, (u32)ctx->mdDataSize);
        sceSsl_lib_5E5C873A(ctx->mdData);
    }
    sceSsl_lib_5E5C873A(ctx);
}

/* [193] 0x00e530 */
LIBSSL_NOINLINE int sslEvpMdCtxCopy(SslEvpMdCtx *dst, const SslEvpMdCtx *src)
{
    if (!sslEvpMdCtxSetDigest(dst, src->digest)) {
        return 0;
    }
    dst->flags = src->flags;
    if (src->mdData != NULL) {
        sslMemcpy(dst->mdData, src->mdData, (u32)src->mdDataSize);
    }
    return 1;
}

/* -------------------------------------------------------------------------
 * Default time callback and setter [194..195]
 * ------------------------------------------------------------------------- */

/* [194] 0x00e594 */
LIBSSL_NOINLINE SceSslTimeSpec *sslDefaultTimeCallback(SceSslTimeSpec *outTime)
{
    if (outTime == NULL) {
        return NULL;
    }
    outTime->secHigh = 0;
    outTime->secLow = 0;
    outTime->nsec = 0;
    return outTime;
}

/* [195] 0x00e5b4 */
LIBSSL_NOINLINE SslTimeCb sslSetTimeCallback(SslTimeCb cb)
{
    g_sslTimeCallback = cb;
    return cb;
}
