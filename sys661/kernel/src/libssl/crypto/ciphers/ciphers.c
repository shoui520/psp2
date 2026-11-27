/*
 * DES, 3DES, RC2, RC4, and MD2 primitives (0x007BCC..0x00C64B, 19 functions).
 */
#include "libssl_internal.h"
#include "cipher_tables.h"

static int g_desCheckKey = 0;
static const u8 g_rc2EmptyKey[4] = { 0, 0, 0, 0 };
static const u32 g_desKeyShifts[16] = {
    0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0
};

static inline u32 desRor32(u32 val, u32 shift)
{
    return (val >> shift) | (val << (32U - shift));
}

static inline u32 desRoundSbox(u32 evenBits, u32 oddBits)
{
    return g_desSpTrans[(evenBits >> 2) & 0x3FU] ^
           g_desSpTrans[128 + ((evenBits >> 10) & 0x3FU)] ^
           g_desSpTrans[256 + ((evenBits >> 18) & 0x3FU)] ^
           g_desSpTrans[384 + ((evenBits >> 26) & 0x3FU)] ^
           g_desSpTrans[64 + ((oddBits >> 2) & 0x3FU)] ^
           g_desSpTrans[192 + ((oddBits >> 10) & 0x3FU)] ^
           g_desSpTrans[320 + ((oddBits >> 18) & 0x3FU)] ^
           g_desSpTrans[448 + ((oddBits >> 26) & 0x3FU)];
}

static inline void desInitialPerm(u32 *leftOut, u32 *rightOut, u32 leftIn, u32 rightIn)
{
    u32 tmp;

    tmp = ((rightIn >> 4) ^ leftIn) & 0x0F0F0F0FU;
    leftIn ^= tmp;
    rightIn ^= (tmp << 4);

    tmp = ((leftIn >> 16) ^ rightIn) & 0x0000FFFFU;
    rightIn ^= tmp;
    leftIn ^= (tmp << 16);

    tmp = ((rightIn >> 2) ^ leftIn) & 0x33333333U;
    leftIn ^= tmp;
    rightIn ^= (tmp << 2);

    tmp = ((leftIn >> 8) ^ rightIn) & 0x00FF00FFU;
    rightIn ^= tmp;
    leftIn ^= (tmp << 8);

    tmp = ((rightIn >> 1) ^ leftIn) & 0x55555555U;
    leftIn ^= tmp;
    rightIn ^= (tmp << 1);

    *leftOut = leftIn;
    *rightOut = rightIn;
}

static inline void desFinalPerm(u32 *leftOut, u32 *rightOut, u32 leftIn, u32 rightIn)
{
    u32 tmp;

    tmp = ((leftIn >> 1) ^ rightIn) & 0x55555555U;
    rightIn ^= tmp;
    leftIn ^= (tmp << 1);

    tmp = ((rightIn >> 8) ^ leftIn) & 0x00FF00FFU;
    leftIn ^= tmp;
    rightIn ^= (tmp << 8);

    tmp = ((leftIn >> 2) ^ rightIn) & 0x33333333U;
    rightIn ^= tmp;
    leftIn ^= (tmp << 2);

    tmp = ((rightIn >> 16) ^ leftIn) & 0x0000FFFFU;
    leftIn ^= tmp;
    rightIn ^= (tmp << 16);

    tmp = ((leftIn >> 4) ^ rightIn) & 0x0F0F0F0FU;
    rightIn ^= tmp;
    leftIn ^= (tmp << 4);

    *leftOut = rightIn;
    *rightOut = leftIn;
}

static inline u32 loadLe32(const u8 *src)
{
    return (u32)src[0] | ((u32)src[1] << 8) | ((u32)src[2] << 16) | ((u32)src[3] << 24);
}

static inline void storeLe32(u8 *dst, u32 val)
{
    dst[0] = (u8)(val & 0xFFU);
    dst[1] = (u8)((val >> 8) & 0xFFU);
    dst[2] = (u8)((val >> 16) & 0xFFU);
    dst[3] = (u8)((val >> 24) & 0xFFU);
}

static inline __attribute__((always_inline)) void loadPartialLe64(const u8 *src, int remLen, u32 *loOut, u32 *hiOut)
{
    u32 loVal = 0;
    u32 hiVal = 0;
    int idx;

    for (idx = remLen - 1; idx >= 0; idx--) {
        if (idx >= 4) {
            hiVal = (hiVal << 8) | src[idx];
        } else {
            loVal = (loVal << 8) | src[idx];
        }
    }
    *loOut = loVal;
    *hiOut = hiVal;
}

static inline __attribute__((always_inline)) void storePartialLe64(u8 *dst, int remLen, u32 loVal, u32 hiVal)
{
    int idx;

    for (idx = 0; idx < remLen; idx++) {
        if (idx < 4) {
            dst[idx] = (u8)(loVal >> (idx * 8));
        } else {
            dst[idx] = (u8)(hiVal >> ((idx - 4) * 8));
        }
    }
}

/* 0x007BCC */
void LIBSSL_NOINLINE sslDesEncrypt(u32 blockWords[2], const SslDesKeySchedule *ks, int encFlag)
{
    u32 left;
    u32 right;
    int roundIdx;

    desInitialPerm(&left, &right, blockWords[0], blockWords[1]);
    left = desRor32(left, 29);
    right = desRor32(right, 29);

    if (encFlag != 0) {
        const u32 *keyPtr = ks->subkeys;
        for (roundIdx = 0; roundIdx < 16; roundIdx += 4) {
            right ^= desRoundSbox(left ^ keyPtr[0], desRor32(left ^ keyPtr[1], 4));
            left ^= desRoundSbox(right ^ keyPtr[2], desRor32(right ^ keyPtr[3], 4));
            right ^= desRoundSbox(left ^ keyPtr[4], desRor32(left ^ keyPtr[5], 4));
            left ^= desRoundSbox(right ^ keyPtr[6], desRor32(right ^ keyPtr[7], 4));
            keyPtr += 8;
        }
    } else {
        const u32 *keyPtr = &ks->subkeys[30];
        for (roundIdx = 0; roundIdx < 16; roundIdx += 4) {
            right ^= desRoundSbox(left ^ keyPtr[0], desRor32(left ^ keyPtr[1], 4));
            left ^= desRoundSbox(right ^ keyPtr[-2], desRor32(right ^ keyPtr[-1], 4));
            right ^= desRoundSbox(left ^ keyPtr[-4], desRor32(left ^ keyPtr[-3], 4));
            left ^= desRoundSbox(right ^ keyPtr[-6], desRor32(right ^ keyPtr[-5], 4));
            keyPtr -= 8;
        }
    }

    left = desRor32(left, 3);
    right = desRor32(right, 3);
    desFinalPerm(&blockWords[0], &blockWords[1], left, right);
}

/* 0x0082C8 */
void LIBSSL_NOINLINE sslDesEncryptCore(u32 blockWords[2], const SslDesKeySchedule *ks, int encFlag)
{
    u32 left = desRor32(blockWords[0], 29);
    u32 right = desRor32(blockWords[1], 29);
    int roundIdx;

    if (encFlag != 0) {
        const u32 *keyPtr = ks->subkeys;
        for (roundIdx = 0; roundIdx < 16; roundIdx += 4) {
            right ^= desRoundSbox(left ^ keyPtr[0], desRor32(left ^ keyPtr[1], 4));
            left ^= desRoundSbox(right ^ keyPtr[2], desRor32(right ^ keyPtr[3], 4));
            right ^= desRoundSbox(left ^ keyPtr[4], desRor32(left ^ keyPtr[5], 4));
            left ^= desRoundSbox(right ^ keyPtr[6], desRor32(right ^ keyPtr[7], 4));
            keyPtr += 8;
        }
    } else {
        const u32 *keyPtr = &ks->subkeys[30];
        for (roundIdx = 0; roundIdx < 16; roundIdx += 4) {
            right ^= desRoundSbox(left ^ keyPtr[0], desRor32(left ^ keyPtr[1], 4));
            left ^= desRoundSbox(right ^ keyPtr[-2], desRor32(right ^ keyPtr[-1], 4));
            right ^= desRoundSbox(left ^ keyPtr[-4], desRor32(left ^ keyPtr[-3], 4));
            left ^= desRoundSbox(right ^ keyPtr[-6], desRor32(right ^ keyPtr[-5], 4));
            keyPtr -= 8;
        }
    }

    blockWords[0] = desRor32(right, 3);
    blockWords[1] = desRor32(left, 3);
}

/* 0x008894 */
void LIBSSL_NOINLINE sslDesEncryptTriple(u32 blockWords[2], const SslDesKeySchedule *ksOne,
                                         const SslDesKeySchedule *ksTwo, const SslDesKeySchedule *ksThree)
{
    u32 left;
    u32 right;

    desInitialPerm(&left, &right, blockWords[0], blockWords[1]);
    blockWords[0] = left;
    blockWords[1] = right;
    sslDesEncryptCore(blockWords, ksOne, 1);
    sslDesEncryptCore(blockWords, ksTwo, 0);
    sslDesEncryptCore(blockWords, ksThree, 1);
    desFinalPerm(&blockWords[0], &blockWords[1], blockWords[1], blockWords[0]);
}

/* 0x008A44 */
void LIBSSL_NOINLINE sslDesDecryptTriple(u32 blockWords[2], const SslDesKeySchedule *ksOne,
                                         const SslDesKeySchedule *ksTwo, const SslDesKeySchedule *ksThree)
{
    u32 left;
    u32 right;

    desInitialPerm(&left, &right, blockWords[0], blockWords[1]);
    blockWords[0] = left;
    blockWords[1] = right;
    sslDesEncryptCore(blockWords, ksThree, 0);
    sslDesEncryptCore(blockWords, ksTwo, 1);
    sslDesEncryptCore(blockWords, ksOne, 0);
    desFinalPerm(&blockWords[0], &blockWords[1], blockWords[1], blockWords[0]);
}

/* 0x008BF8 */
void LIBSSL_NOINLINE sslDesNcbcEncrypt(const u8 *inBuf, u8 *outBuf, int byteLen,
                                       const SslDesKeySchedule *ks, u8 ivBuf[8], int encFlag)
{
    u32 ivLo = loadLe32(ivBuf);
    u32 ivHi = loadLe32(&ivBuf[4]);
    u32 blockWords[2];
    int remLen = byteLen;

    if (encFlag != 0) {
        while (remLen >= 8) {
            blockWords[0] = loadLe32(inBuf) ^ ivLo;
            blockWords[1] = loadLe32(&inBuf[4]) ^ ivHi;
            inBuf += 8;
            remLen -= 8;
            sslDesEncrypt(blockWords, ks, 1);
            ivLo = blockWords[0];
            ivHi = blockWords[1];
            storeLe32(outBuf, ivLo);
            storeLe32(&outBuf[4], ivHi);
            outBuf += 8;
        }
        if (remLen > 0) {
            u32 tailLo;
            u32 tailHi;
            loadPartialLe64(inBuf, remLen, &tailLo, &tailHi);
            blockWords[0] = tailLo ^ ivLo;
            blockWords[1] = tailHi ^ ivHi;
            sslDesEncrypt(blockWords, ks, 1);
            ivLo = blockWords[0];
            ivHi = blockWords[1];
            storeLe32(outBuf, ivLo);
            storeLe32(&outBuf[4], ivHi);
        }
    } else {
        while (remLen >= 8) {
            u32 cipherLo = loadLe32(inBuf);
            u32 cipherHi = loadLe32(&inBuf[4]);
            inBuf += 8;
            remLen -= 8;
            blockWords[0] = cipherLo;
            blockWords[1] = cipherHi;
            sslDesEncrypt(blockWords, ks, 0);
            storeLe32(outBuf, blockWords[0] ^ ivLo);
            storeLe32(&outBuf[4], blockWords[1] ^ ivHi);
            outBuf += 8;
            ivLo = cipherLo;
            ivHi = cipherHi;
        }
        if (remLen > 0) {
            u32 cipherLo = loadLe32(inBuf);
            u32 cipherHi = loadLe32(&inBuf[4]);
            blockWords[0] = cipherLo;
            blockWords[1] = cipherHi;
            sslDesEncrypt(blockWords, ks, 0);
            storePartialLe64(outBuf, remLen, blockWords[0] ^ ivLo, blockWords[1] ^ ivHi);
            ivLo = cipherLo;
            ivHi = cipherHi;
        }
    }

    storeLe32(ivBuf, ivLo);
    storeLe32(&ivBuf[4], ivHi);
}

/* 0x00920C */
void LIBSSL_NOINLINE sslDesEde3CbcEncrypt(const u8 *inBuf, u8 *outBuf, int byteLen,
                                          const SslDesKeySchedule *ksOne, const SslDesKeySchedule *ksTwo,
                                          const SslDesKeySchedule *ksThree, u8 ivBuf[8], int encFlag)
{
    u32 ivLo = loadLe32(ivBuf);
    u32 ivHi = loadLe32(&ivBuf[4]);
    u32 blockWords[2];
    int remLen = byteLen;

    if (encFlag != 0) {
        while (remLen >= 8) {
            blockWords[0] = loadLe32(inBuf) ^ ivLo;
            blockWords[1] = loadLe32(&inBuf[4]) ^ ivHi;
            inBuf += 8;
            remLen -= 8;
            sslDesEncryptTriple(blockWords, ksOne, ksTwo, ksThree);
            ivLo = blockWords[0];
            ivHi = blockWords[1];
            storeLe32(outBuf, ivLo);
            storeLe32(&outBuf[4], ivHi);
            outBuf += 8;
        }
        if (remLen > 0) {
            u32 tailLo;
            u32 tailHi;
            loadPartialLe64(inBuf, remLen, &tailLo, &tailHi);
            blockWords[0] = tailLo ^ ivLo;
            blockWords[1] = tailHi ^ ivHi;
            sslDesEncryptTriple(blockWords, ksOne, ksTwo, ksThree);
            ivLo = blockWords[0];
            ivHi = blockWords[1];
            storeLe32(outBuf, ivLo);
            storeLe32(&outBuf[4], ivHi);
        }
    } else {
        while (remLen >= 8) {
            u32 cipherLo = loadLe32(inBuf);
            u32 cipherHi = loadLe32(&inBuf[4]);
            inBuf += 8;
            remLen -= 8;
            blockWords[0] = cipherLo;
            blockWords[1] = cipherHi;
            sslDesDecryptTriple(blockWords, ksOne, ksTwo, ksThree);
            storeLe32(outBuf, blockWords[0] ^ ivLo);
            storeLe32(&outBuf[4], blockWords[1] ^ ivHi);
            outBuf += 8;
            ivLo = cipherLo;
            ivHi = cipherHi;
        }
        if (remLen > 0) {
            u32 cipherLo = loadLe32(inBuf);
            u32 cipherHi = loadLe32(&inBuf[4]);
            blockWords[0] = cipherLo;
            blockWords[1] = cipherHi;
            sslDesDecryptTriple(blockWords, ksOne, ksTwo, ksThree);
            storePartialLe64(outBuf, remLen, blockWords[0] ^ ivLo, blockWords[1] ^ ivHi);
            ivLo = cipherLo;
            ivHi = cipherHi;
        }
    }

    storeLe32(ivBuf, ivLo);
    storeLe32(&ivBuf[4], ivHi);
}

/* 0x00983C */
int LIBSSL_NOINLINE sslDesCheckKeyParity(const u8 keyBytes[8])
{
    u32 idx;

    for (idx = 0; idx < 8U; idx++) {
        if (keyBytes[idx] != g_desOddParity[keyBytes[idx]]) {
            return 0;
        }
    }
    return 1;
}

/* 0x00987C */
int LIBSSL_NOINLINE sslDesIsWeakKey(const u8 keyBytes[8])
{
    int idx;

    for (idx = 0; idx < 16; idx++) {
        if (sslMemcmp(g_desWeakKeys[idx], keyBytes, 8) == 0) {
            return 1;
        }
    }
    return 0;
}

/* 0x0098E4 */
int LIBSSL_NOINLINE sslDesSetKey(const u8 keyBytes[8], SslDesKeySchedule *ks)
{
    u32 regC;
    u32 regD;
    u32 tmp;
    int roundIdx;

    if (g_desCheckKey != 0) {
        if (sslDesCheckKeyParity(keyBytes) == 0) {
            return -1;
        }
        if (sslDesIsWeakKey(keyBytes) != 0) {
            return -2;
        }
    }

    regC = loadLe32(keyBytes);
    regD = loadLe32(&keyBytes[4]);

    tmp = ((regD >> 4) ^ regC) & 0x0F0F0F0FU;
    regC ^= tmp;
    regD ^= (tmp << 4);

    tmp = ((regD << 18) ^ regD) & 0xCCCC0000U;
    regD ^= tmp ^ (tmp >> 18);
    tmp = ((regC << 18) ^ regC) & 0xCCCC0000U;
    regC ^= tmp ^ (tmp >> 18);

    tmp = ((regD >> 1) ^ regC) & 0x55555555U;
    regC ^= tmp;
    regD ^= (tmp << 1);

    tmp = ((regC >> 8) ^ regD) & 0x00FF00FFU;
    regD ^= tmp;
    regC ^= (tmp << 8);

    tmp = ((regD >> 1) ^ regC) & 0x55555555U;
    regC ^= tmp;
    regD ^= (tmp << 1);

    regD = ((regD & 0x000000FFU) << 16) |
           (regD & 0x0000FF00U) |
           ((regD & 0x00FF0000U) >> 16) |
           ((regC & 0xF0000000U) >> 4);
    regC &= 0x0FFFFFFFU;

    for (roundIdx = 0; roundIdx < 16; roundIdx++) {
        u32 sLo;
        u32 sHi;

        if (g_desKeyShifts[roundIdx] != 0) {
            regC = ((regC >> 2) | (regC << 26)) & 0x0FFFFFFFU;
            regD = ((regD >> 2) | (regD << 26)) & 0x0FFFFFFFU;
        } else {
            regC = ((regC >> 1) | (regC << 27)) & 0x0FFFFFFFU;
            regD = ((regD >> 1) | (regD << 27)) & 0x0FFFFFFFU;
        }

        sLo = g_desSkb[regC & 0x3FU] |
              g_desSkb[64 + (((regC >> 6) & 0x03U) | ((regC >> 7) & 0x3CU))] |
              g_desSkb[128 + (((regC >> 13) & 0x0FU) | ((regC >> 14) & 0x30U))] |
              g_desSkb[192 + (((regC >> 20) & 0x01U) | ((regC >> 21) & 0x06U) | ((regC >> 22) & 0x38U))];

        sHi = g_desSkb[256 + (regD & 0x3FU)] |
              g_desSkb[320 + (((regD >> 7) & 0x03U) | ((regD >> 8) & 0x3CU))] |
              g_desSkb[384 + ((regD >> 15) & 0x3FU)] |
              g_desSkb[448 + (((regD >> 21) & 0x0FU) | ((regD >> 22) & 0x30U))];

        ks->subkeys[roundIdx * 2] = desRor32((sHi << 16) | (sLo & 0xFFFFU), 30);
        ks->subkeys[roundIdx * 2 + 1] = desRor32((sLo >> 16) | (sHi & 0xFFFF0000U), 26);
    }
    return 0;
}

/* 0x009BE8 */
void LIBSSL_NOINLINE sslRc2CbcEncrypt(const u8 *inBuf, u8 *outBuf, int byteLen,
                                      const SslRc2Key *ks, u8 ivBuf[8], int encFlag)
{
    u32 ivLo = loadLe32(ivBuf);
    u32 ivHi = loadLe32(&ivBuf[4]);
    u32 blockWords[2];
    int remLen = byteLen;

    if (encFlag != 0) {
        while (remLen >= 8) {
            blockWords[0] = loadLe32(inBuf) ^ ivLo;
            blockWords[1] = loadLe32(&inBuf[4]) ^ ivHi;
            inBuf += 8;
            remLen -= 8;
            sslRc2Encrypt(blockWords, ks);
            ivLo = blockWords[0];
            ivHi = blockWords[1];
            storeLe32(outBuf, ivLo);
            storeLe32(&outBuf[4], ivHi);
            outBuf += 8;
        }
        if (remLen > 0) {
            u32 tailLo;
            u32 tailHi;
            loadPartialLe64(inBuf, remLen, &tailLo, &tailHi);
            blockWords[0] = tailLo ^ ivLo;
            blockWords[1] = tailHi ^ ivHi;
            sslRc2Encrypt(blockWords, ks);
            ivLo = blockWords[0];
            ivHi = blockWords[1];
            storeLe32(outBuf, ivLo);
            storeLe32(&outBuf[4], ivHi);
        }
    } else {
        while (remLen >= 8) {
            u32 cipherLo = loadLe32(inBuf);
            u32 cipherHi = loadLe32(&inBuf[4]);
            inBuf += 8;
            remLen -= 8;
            blockWords[0] = cipherLo;
            blockWords[1] = cipherHi;
            sslRc2Decrypt(blockWords, ks);
            storeLe32(outBuf, blockWords[0] ^ ivLo);
            storeLe32(&outBuf[4], blockWords[1] ^ ivHi);
            outBuf += 8;
            ivLo = cipherLo;
            ivHi = cipherHi;
        }
        if (remLen > 0) {
            u32 cipherLo = loadLe32(inBuf);
            u32 cipherHi = loadLe32(&inBuf[4]);
            blockWords[0] = cipherLo;
            blockWords[1] = cipherHi;
            sslRc2Decrypt(blockWords, ks);
            storePartialLe64(outBuf, remLen, blockWords[0] ^ ivLo, blockWords[1] ^ ivHi);
            ivLo = cipherLo;
            ivHi = cipherHi;
        }
    }

    storeLe32(ivBuf, ivLo);
    storeLe32(&ivBuf[4], ivHi);
}

static inline u16 rc2Rol16(u16 val, u32 shift)
{
    return (u16)((val << shift) | (val >> (16U - shift)));
}

static inline u16 rc2Ror16(u16 val, u32 shift)
{
    return (u16)((val >> shift) | (val << (16U - shift)));
}

/* 0x00A210 */
void LIBSSL_NOINLINE sslRc2Encrypt(u32 blockWords[2], const SslRc2Key *ks)
{
    u16 regZero = (u16)(blockWords[0] & 0xFFFFU);
    u16 regOne = (u16)(blockWords[0] >> 16);
    u16 regTwo = (u16)(blockWords[1] & 0xFFFFU);
    u16 regThree = (u16)(blockWords[1] >> 16);
    const u16 *keyPtr = ks->data;
    int roundIdx;

    for (roundIdx = 0; roundIdx < 16; roundIdx++) {
        regZero = rc2Rol16((u16)(regZero + keyPtr[0] + (regThree & regTwo) + ((u16)(~regThree) & regOne)), 1);
        regOne = rc2Rol16((u16)(regOne + keyPtr[1] + (regZero & regThree) + ((u16)(~regZero) & regTwo)), 2);
        regTwo = rc2Rol16((u16)(regTwo + keyPtr[2] + (regOne & regZero) + ((u16)(~regOne) & regThree)), 3);
        regThree = rc2Rol16((u16)(regThree + keyPtr[3] + (regTwo & regOne) + ((u16)(~regTwo) & regZero)), 5);
        keyPtr += 4;

        if (roundIdx == 4 || roundIdx == 10) {
            regZero = (u16)(regZero + ks->data[regThree & 0x3FU]);
            regOne = (u16)(regOne + ks->data[regZero & 0x3FU]);
            regTwo = (u16)(regTwo + ks->data[regOne & 0x3FU]);
            regThree = (u16)(regThree + ks->data[regTwo & 0x3FU]);
        }
    }

    blockWords[0] = (u32)regZero | ((u32)regOne << 16);
    blockWords[1] = (u32)regTwo | ((u32)regThree << 16);
}

/* 0x00B010 */
void LIBSSL_NOINLINE sslRc2Decrypt(u32 blockWords[2], const SslRc2Key *ks)
{
    u16 regZero = (u16)(blockWords[0] & 0xFFFFU);
    u16 regOne = (u16)(blockWords[0] >> 16);
    u16 regTwo = (u16)(blockWords[1] & 0xFFFFU);
    u16 regThree = (u16)(blockWords[1] >> 16);
    const u16 *keyPtr = &ks->data[63];
    int roundIdx;

    for (roundIdx = 15; roundIdx >= 0; roundIdx--) {
        regThree = (u16)(rc2Ror16(regThree, 5) - keyPtr[0] - (regTwo & regOne) - ((u16)(~regTwo) & regZero));
        regTwo = (u16)(rc2Ror16(regTwo, 3) - keyPtr[-1] - (regOne & regZero) - ((u16)(~regOne) & regThree));
        regOne = (u16)(rc2Ror16(regOne, 2) - keyPtr[-2] - (regZero & regThree) - ((u16)(~regZero) & regTwo));
        regZero = (u16)(rc2Ror16(regZero, 1) - keyPtr[-3] - (regThree & regTwo) - ((u16)(~regThree) & regOne));
        keyPtr -= 4;

        if (roundIdx == 11 || roundIdx == 5) {
            regThree = (u16)(regThree - ks->data[regTwo & 0x3FU]);
            regTwo = (u16)(regTwo - ks->data[regOne & 0x3FU]);
            regOne = (u16)(regOne - ks->data[regZero & 0x3FU]);
            regZero = (u16)(regZero - ks->data[regThree & 0x3FU]);
        }
    }

    blockWords[0] = (u32)regZero | ((u32)regOne << 16);
    blockWords[1] = (u32)regTwo | ((u32)regThree << 16);
}

/* 0x00BD04 */
void LIBSSL_NOINLINE sslRc2SetKey(SslRc2Key *ks, int keyLen, const u8 *keyBytes, int effectiveBits)
{
    u8 expanded[128];
    int effBytes;
    u8 topMask;
    int idx;

    if (keyLen < 0) {
        keyBytes = g_rc2EmptyKey;
        keyLen = 1;
    } else if (keyLen > 128) {
        keyLen = 128;
    }

    if (effectiveBits == 0) {
        effectiveBits = 1024;
    }
    effBytes = (effectiveBits + 7) / 8;
    topMask = (u8)(0xFFU >> (8 * effBytes - effectiveBits));

    sslMemcpy(expanded, keyBytes, (u32)keyLen);
    for (idx = keyLen; idx < 128; idx++) {
        expanded[idx] = g_rc2KeyTable[(u8)(expanded[idx - 1] + expanded[idx - keyLen])];
    }

    expanded[128 - effBytes] = g_rc2KeyTable[(u8)(expanded[128 - effBytes] & topMask)];
    for (idx = 127 - effBytes; idx >= 0; idx--) {
        expanded[idx] = g_rc2KeyTable[(u8)(expanded[idx + 1] ^ expanded[idx + effBytes])];
    }

    for (idx = 0; idx < 64; idx++) {
        ks->data[idx] = (u16)((u32)expanded[idx * 2] | ((u32)expanded[idx * 2 + 1] << 8));
    }
}

/* 0x00BE84 */
void LIBSSL_NOINLINE sslRc4Crypt(SslRc4Key *key, u32 byteLen, const u8 *inBuf, u8 *outBuf)
{
    u32 idxX;
    u32 idxY;
    u32 *state;
    u32 idx;

    if (byteLen == 0) {
        return;
    }

    state = key->state;
    idxX = key->idxX;
    idxY = key->idxY;

    for (idx = 0; idx < byteLen; idx++) {
        u32 tx;
        u32 ty;

        idxX = (idxX + 1U) & 0xFFU;
        tx = state[idxX];
        idxY = (idxY + tx) & 0xFFU;
        ty = state[idxY];
        state[idxX] = ty;
        state[idxY] = tx;
        outBuf[idx] = (u8)(inBuf[idx] ^ (u8)state[(tx + ty) & 0xFFU]);
    }

    key->idxX = idxX;
    key->idxY = idxY;
}

/* 0x00C1E0 */
void LIBSSL_NOINLINE sslRc4SetKey(SslRc4Key *key, int keyLen, const u8 *keyBytes)
{
    u32 *state = key->state;
    int idx;
    int keyPos = 0;
    u32 mixIdx = 0;

    key->idxX = 0;
    key->idxY = 0;
    for (idx = 0; idx < 256; idx++) {
        state[idx] = (u32)idx;
    }

    for (idx = 0; idx < 256; idx++) {
        u32 tmpVal = state[idx];
        mixIdx = (mixIdx + (u32)keyBytes[keyPos] + tmpVal) & 0xFFU;
        keyPos++;
        if (keyPos == keyLen) {
            keyPos = 0;
        }
        state[idx] = state[mixIdx];
        state[mixIdx] = tmpVal;
    }
}

/* 0x00C268 */
void LIBSSL_NOINLINE sslMd2Init(SslMd2Ctx *ctx)
{
    ctx->num = 0;
    sslMemset(ctx->state, 0, sizeof(ctx->state));
    sslMemset(ctx->checksum, 0, sizeof(ctx->checksum));
    sslMemset(ctx->dataBuf, 0, sizeof(ctx->dataBuf));
}

/* 0x00C2C0 */
void LIBSSL_NOINLINE sslMd2Update(SslMd2Ctx *ctx, const u8 *inBuf, u32 byteLen)
{
    if (byteLen == 0) {
        return;
    }
    if (ctx->num != 0) {
        if ((u32)ctx->num + byteLen < 16U) {
            sslMemcpy(&ctx->dataBuf[ctx->num], inBuf, byteLen);
            ctx->num += (int)byteLen;
            return;
        }
        sslMemcpy(&ctx->dataBuf[ctx->num], inBuf, 16U - (u32)ctx->num);
        sslMd2Block(ctx, ctx->dataBuf);
        inBuf += (16U - (u32)ctx->num);
        byteLen -= (16U - (u32)ctx->num);
        ctx->num = 0;
    }

    while (byteLen >= 16U) {
        sslMd2Block(ctx, inBuf);
        inBuf += 16;
        byteLen -= 16U;
    }

    sslMemcpy(ctx->dataBuf, inBuf, byteLen);
    ctx->num = (int)byteLen;
}

/* 0x00C3A8 */
void LIBSSL_NOINLINE sslMd2Block(SslMd2Ctx *ctx, const u8 blockBytes[16])
{
    u32 workBuf[48];
    u32 lastCheck = ctx->checksum[15];
    u32 mixByte;
    int idx;
    int roundIdx;

    for (idx = 0; idx < 16; idx++) {
        u32 curByte = blockBytes[idx];
        u32 stWord = ctx->state[idx];
        workBuf[idx] = stWord;
        workBuf[16 + idx] = curByte;
        workBuf[32 + idx] = curByte ^ stWord;
        lastCheck = ctx->checksum[idx] ^ g_md2PermTable[(curByte ^ lastCheck) & 0xFFU];
        ctx->checksum[idx] = lastCheck;
    }

    mixByte = 0;
    for (roundIdx = 0; roundIdx < 18; roundIdx++) {
        for (idx = 0; idx < 48; idx++) {
            mixByte = workBuf[idx] ^ g_md2PermTable[mixByte & 0xFFU];
            workBuf[idx] = mixByte;
        }
        mixByte = (mixByte + (u32)roundIdx) & 0xFFU;
    }

    for (idx = 0; idx < 16; idx++) {
        ctx->state[idx] = workBuf[idx];
    }
    sslMemset(workBuf, 0, sizeof(workBuf));
}

/* 0x00C570 */
void LIBSSL_NOINLINE sslMd2Final(u8 outDigest[16], SslMd2Ctx *ctx)
{
    int idx;
    u8 padByte = (u8)(16 - ctx->num);

    for (idx = ctx->num; idx < 16; idx++) {
        ctx->dataBuf[idx] = padByte;
    }
    sslMd2Block(ctx, ctx->dataBuf);

    for (idx = 0; idx < 16; idx++) {
        ctx->dataBuf[idx] = (u8)ctx->checksum[idx];
    }
    sslMd2Block(ctx, ctx->dataBuf);

    for (idx = 0; idx < 16; idx++) {
        outDigest[idx] = (u8)ctx->state[idx];
    }
}
