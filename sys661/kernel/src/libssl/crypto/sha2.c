/*
 * FIPS 180-2 SHA-224, SHA-256, SHA-384, and SHA-512 (0x000000..0x0021DF, 0x028BE0..0x029F07).
 */
#include "libssl_internal.h"

static const u32 g_sha256RoundConsts[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static const u64 g_sha512RoundConsts[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static inline u32 shaRor32(u32 word, u32 shift)
{
    return (word >> shift) | (word << (32U - shift));
}

static inline u64 shaRor64(u64 word, u32 shift)
{
    return (word >> shift) | (word << (64U - shift));
}

static inline u32 shaBswap32(u32 word)
{
    return ((word & 0x000000FFU) << 24) |
           ((word & 0x0000FF00U) << 8)  |
           ((word & 0x00FF0000U) >> 8)  |
           ((word & 0xFF000000U) >> 24);
}

static inline u64 shaBswap64(u64 word)
{
    u32 loWord = shaBswap32((u32)(word & 0xFFFFFFFFU));
    u32 hiWord = shaBswap32((u32)(word >> 32));
    return ((u64)loWord << 32) | (u64)hiWord;
}

static inline __attribute__((always_inline)) void sha256CompressCore(const u32 blockWords[16], u32 state[8])
{
    u32 sched[64];
    u32 regA, regB, regC, regD, regE, regF, regG, regH;
    u32 idx;

    for (idx = 0; idx < 16; idx++) {
        sched[idx] = blockWords[idx];
    }
    for (idx = 16; idx < 64; idx++) {
        u32 sigmaSmall0 = shaRor32(sched[idx - 15], 7) ^ shaRor32(sched[idx - 15], 18) ^ (sched[idx - 15] >> 3);
        u32 sigmaSmall1 = shaRor32(sched[idx - 2], 17) ^ shaRor32(sched[idx - 2], 19) ^ (sched[idx - 2] >> 10);
        sched[idx] = sched[idx - 16] + sigmaSmall0 + sched[idx - 7] + sigmaSmall1;
    }

    regA = state[0];
    regB = state[1];
    regC = state[2];
    regD = state[3];
    regE = state[4];
    regF = state[5];
    regG = state[6];
    regH = state[7];

    for (idx = 0; idx < 64; idx++) {
        u32 sigmaBig1 = shaRor32(regE, 6) ^ shaRor32(regE, 11) ^ shaRor32(regE, 25);
        u32 chooseWord = (regE & regF) ^ ((~regE) & regG);
        u32 roundSumA = regH + sigmaBig1 + chooseWord + g_sha256RoundConsts[idx] + sched[idx];
        u32 sigmaBig0 = shaRor32(regA, 2) ^ shaRor32(regA, 13) ^ shaRor32(regA, 22);
        u32 majWord = (regA & regB) ^ (regA & regC) ^ (regB & regC);
        u32 roundSumB = sigmaBig0 + majWord;

        regH = regG;
        regG = regF;
        regF = regE;
        regE = regD + roundSumA;
        regD = regC;
        regC = regB;
        regB = regA;
        regA = roundSumA + roundSumB;
    }

    state[0] += regA;
    state[1] += regB;
    state[2] += regC;
    state[3] += regD;
    state[4] += regE;
    state[5] += regF;
    state[6] += regG;
    state[7] += regH;
}

static inline __attribute__((always_inline)) void sha512CompressCore(const u64 blockWords[16], u64 state[8])
{
    u64 sched[80];
    u64 regA, regB, regC, regD, regE, regF, regG, regH;
    u32 idx;

    for (idx = 0; idx < 16; idx++) {
        sched[idx] = blockWords[idx];
    }
    for (idx = 16; idx < 80; idx++) {
        u64 sigmaSmall0 = shaRor64(sched[idx - 15], 1) ^ shaRor64(sched[idx - 15], 8) ^ (sched[idx - 15] >> 7);
        u64 sigmaSmall1 = shaRor64(sched[idx - 2], 19) ^ shaRor64(sched[idx - 2], 61) ^ (sched[idx - 2] >> 6);
        sched[idx] = sched[idx - 16] + sigmaSmall0 + sched[idx - 7] + sigmaSmall1;
    }

    regA = state[0];
    regB = state[1];
    regC = state[2];
    regD = state[3];
    regE = state[4];
    regF = state[5];
    regG = state[6];
    regH = state[7];

    for (idx = 0; idx < 80; idx++) {
        u64 sigmaBig1 = shaRor64(regE, 14) ^ shaRor64(regE, 18) ^ shaRor64(regE, 41);
        u64 chooseWord = (regE & regF) ^ ((~regE) & regG);
        u64 roundSumA = regH + sigmaBig1 + chooseWord + g_sha512RoundConsts[idx] + sched[idx];
        u64 sigmaBig0 = shaRor64(regA, 28) ^ shaRor64(regA, 34) ^ shaRor64(regA, 39);
        u64 majWord = (regA & regB) ^ (regA & regC) ^ (regB & regC);
        u64 roundSumB = sigmaBig0 + majWord;

        regH = regG;
        regG = regF;
        regF = regE;
        regE = regD + roundSumA;
        regD = regC;
        regC = regB;
        regB = regA;
        regA = roundSumA + roundSumB;
    }

    state[0] += regA;
    state[1] += regB;
    state[2] += regC;
    state[3] += regD;
    state[4] += regE;
    state[5] += regF;
    state[6] += regG;
    state[7] += regH;
}

/* 0x000000 */
void LIBSSL_NOINLINE sslSha224Transform(const u32 blockWords[16], u32 state[8])
{
    sha256CompressCore(blockWords, state);
}

/* 0x000470 */
void LIBSSL_NOINLINE sslSha256Transform(const u32 blockWords[16], u32 state[8])
{
    sha256CompressCore(blockWords, state);
}

/* 0x0008E0 */
void LIBSSL_NOINLINE sslSha384Transform(const u64 blockWords[16], u64 state[8])
{
    sha512CompressCore(blockWords, state);
}

/* 0x001560 */
void LIBSSL_NOINLINE sslSha512Transform(const u64 blockWords[16], u64 state[8])
{
    sha512CompressCore(blockWords, state);
}

/* 0x028BE0 */
int LIBSSL_NOINLINE sslSha224Update(SceSha256Context *ctx, const u8 *input, u32 len)
{
    u32 blockWords[16];
    u8 *blockBytes = (u8 *)blockWords;
    u32 pending;
    u32 total;
    u32 idx;

    if (ctx == NULL || input == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    if (ctx->computed != 0) {
        return SCE_SHA_ERROR_INVALID_STATE;
    }

    pending = ctx->remains;
    total = pending + len;
    ctx->totalLen += (u64)len;

    if (total >= 64U) {
        for (;;) {
            if (pending != 0) {
                u32 fillLen = 64U - pending;
                for (idx = 0; idx < pending; idx++) {
                    blockBytes[idx] = ctx->blockBuf[idx];
                }
                for (idx = 0; idx < fillLen; idx++) {
                    blockBytes[pending + idx] = input[idx];
                }
                input += fillLen;
                ctx->remains = 0;
            } else {
                for (idx = 0; idx < 64U; idx++) {
                    blockBytes[idx] = input[idx];
                }
                input += 64U;
            }
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = shaBswap32(blockWords[idx]);
            }
            total -= 64U;
            sslSha224Transform(blockWords, ctx->state);
            if (total < 64U) {
                break;
            }
            pending = ctx->remains;
        }
    }

    if (total != 0) {
        u32 curRem = ctx->remains;
        u32 copyLen = total - curRem;
        for (idx = 0; idx < copyLen; idx++) {
            ctx->blockBuf[curRem + idx] = input[idx];
        }
        ctx->remains = (u16)total;
    } else {
        ctx->remains = 0;
    }
    return 0;
}

/* 0x028DB0 */
int LIBSSL_NOINLINE sslSha224Final(SceSha256Context *ctx, u8 *digest)
{
    u32 blockWords[16];
    u8 *blockBytes = (u8 *)blockWords;
    const u8 *stateBytes = (const u8 *)ctx->state;
    u32 idx;

    if (ctx == NULL || digest == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    if (ctx->computed == 0) {
        u32 pending = ctx->remains;
        u64 bitCount = ctx->totalLen * 8ULL;

        for (idx = 0; idx < 16U; idx++) {
            blockWords[idx] = 0;
        }
        for (idx = 0; idx < pending; idx++) {
            blockBytes[idx] = ctx->blockBuf[idx];
        }
        blockBytes[pending] = 0x80U;
        if (pending >= 56U) {
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = shaBswap32(blockWords[idx]);
            }
            sslSha224Transform(blockWords, ctx->state);
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = 0;
            }
        }
        for (idx = 0; idx < 8U; idx++) {
            blockBytes[56U + idx] = (u8)(bitCount >> (56U - idx * 8U));
        }
        for (idx = 0; idx < 16U; idx++) {
            blockWords[idx] = shaBswap32(blockWords[idx]);
        }
        sslSha224Transform(blockWords, ctx->state);
        for (idx = 0; idx < 8U; idx++) {
            ctx->state[idx] = shaBswap32(ctx->state[idx]);
        }
        ctx->computed = 1;
        for (idx = 0; idx < 64U; idx++) {
            ctx->blockBuf[idx] = 0;
        }
    }

    for (idx = 0; idx < 28U; idx++) {
        digest[idx] = stateBytes[idx];
    }
    return 0;
}

/* 0x028FAC */
int LIBSSL_NOINLINE sslSha224Init(SceSha256Context *ctx)
{
    if (ctx == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    ctx->state[0] = 0xc1059ed8U;
    ctx->state[1] = 0x367cd507U;
    ctx->state[2] = 0x3070dd17U;
    ctx->state[3] = 0xf70e5939U;
    ctx->state[4] = 0xffc00b31U;
    ctx->state[5] = 0x68581511U;
    ctx->state[6] = 0x64f98fa7U;
    ctx->state[7] = 0xbefa4fa4U;
    ctx->remains = 0;
    ctx->computed = 0;
    ctx->totalLen = 0;
    return 0;
}

/* 0x02903C */
int LIBSSL_NOINLINE sslSha256Update(SceSha256Context *ctx, const u8 *input, u32 len)
{
    u32 blockWords[16];
    u8 *blockBytes = (u8 *)blockWords;
    u32 pending;
    u32 total;
    u32 idx;

    if (ctx == NULL || input == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    if (ctx->computed != 0) {
        return SCE_SHA_ERROR_INVALID_STATE;
    }

    pending = ctx->remains;
    total = pending + len;
    ctx->totalLen += (u64)len;

    if (total >= 64U) {
        for (;;) {
            if (pending != 0) {
                u32 fillLen = 64U - pending;
                for (idx = 0; idx < pending; idx++) {
                    blockBytes[idx] = ctx->blockBuf[idx];
                }
                for (idx = 0; idx < fillLen; idx++) {
                    blockBytes[pending + idx] = input[idx];
                }
                input += fillLen;
                ctx->remains = 0;
            } else {
                for (idx = 0; idx < 64U; idx++) {
                    blockBytes[idx] = input[idx];
                }
                input += 64U;
            }
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = shaBswap32(blockWords[idx]);
            }
            total -= 64U;
            sslSha256Transform(blockWords, ctx->state);
            if (total < 64U) {
                break;
            }
            pending = ctx->remains;
        }
    }

    if (total != 0) {
        u32 curRem = ctx->remains;
        u32 copyLen = total - curRem;
        for (idx = 0; idx < copyLen; idx++) {
            ctx->blockBuf[curRem + idx] = input[idx];
        }
        ctx->remains = (u16)total;
    } else {
        ctx->remains = 0;
    }
    return 0;
}

/* 0x02920C */
int LIBSSL_NOINLINE sslSha256Final(SceSha256Context *ctx, u8 *digest)
{
    u32 blockWords[16];
    u8 *blockBytes = (u8 *)blockWords;
    const u8 *stateBytes = (const u8 *)ctx->state;
    u32 idx;

    if (ctx == NULL || digest == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    if (ctx->computed == 0) {
        u32 pending = ctx->remains;
        u64 bitCount = ctx->totalLen * 8ULL;

        for (idx = 0; idx < 16U; idx++) {
            blockWords[idx] = 0;
        }
        for (idx = 0; idx < pending; idx++) {
            blockBytes[idx] = ctx->blockBuf[idx];
        }
        blockBytes[pending] = 0x80U;
        if (pending >= 56U) {
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = shaBswap32(blockWords[idx]);
            }
            sslSha256Transform(blockWords, ctx->state);
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = 0;
            }
        }
        for (idx = 0; idx < 8U; idx++) {
            blockBytes[56U + idx] = (u8)(bitCount >> (56U - idx * 8U));
        }
        for (idx = 0; idx < 16U; idx++) {
            blockWords[idx] = shaBswap32(blockWords[idx]);
        }
        sslSha256Transform(blockWords, ctx->state);
        for (idx = 0; idx < 8U; idx++) {
            ctx->state[idx] = shaBswap32(ctx->state[idx]);
        }
        ctx->computed = 1;
        for (idx = 0; idx < 64U; idx++) {
            ctx->blockBuf[idx] = 0;
        }
    }

    for (idx = 0; idx < 32U; idx++) {
        digest[idx] = stateBytes[idx];
    }
    return 0;
}

/* 0x029408 */
int LIBSSL_NOINLINE sslSha256Init(SceSha256Context *ctx)
{
    if (ctx == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
    ctx->remains = 0;
    ctx->computed = 0;
    ctx->totalLen = 0;
    return 0;
}

/* 0x029498 */
int LIBSSL_NOINLINE sslSha384Update(SceSha512Context *ctx, const u8 *input, u32 len)
{
    u64 blockWords[16];
    u8 *blockBytes = (u8 *)blockWords;
    u32 pending;
    u32 total;
    u32 idx;

    if (ctx == NULL || input == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    if (ctx->computed != 0) {
        return SCE_SHA_ERROR_INVALID_STATE;
    }

    pending = ctx->remains;
    total = pending + len;
    ctx->totalLen += (u64)len;

    if (total >= 128U) {
        for (;;) {
            if (pending != 0) {
                u32 fillLen = 128U - pending;
                for (idx = 0; idx < pending; idx++) {
                    blockBytes[idx] = ctx->blockBuf[idx];
                }
                for (idx = 0; idx < fillLen; idx++) {
                    blockBytes[pending + idx] = input[idx];
                }
                input += fillLen;
                ctx->remains = 0;
            } else {
                for (idx = 0; idx < 128U; idx++) {
                    blockBytes[idx] = input[idx];
                }
                input += 128U;
            }
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = shaBswap64(blockWords[idx]);
            }
            total -= 128U;
            sslSha384Transform(blockWords, ctx->state);
            if (total < 128U) {
                break;
            }
            pending = ctx->remains;
        }
    }

    if (total != 0) {
        u32 curRem = ctx->remains;
        u32 copyLen = total - curRem;
        for (idx = 0; idx < copyLen; idx++) {
            ctx->blockBuf[curRem + idx] = input[idx];
        }
        ctx->remains = (u16)total;
    } else {
        ctx->remains = 0;
    }
    return 0;
}

/* 0x029684 */
int LIBSSL_NOINLINE sslSha384Final(SceSha512Context *ctx, u8 *digest)
{
    u64 blockWords[16];
    u8 *blockBytes = (u8 *)blockWords;
    const u8 *stateBytes = (const u8 *)ctx->state;
    u32 idx;

    if (ctx == NULL || digest == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    if (ctx->computed == 0) {
        u32 pending = ctx->remains;
        u64 bitCount = ctx->totalLen * 8ULL;

        for (idx = 0; idx < 16U; idx++) {
            blockWords[idx] = 0;
        }
        for (idx = 0; idx < pending; idx++) {
            blockBytes[idx] = ctx->blockBuf[idx];
        }
        blockBytes[pending] = 0x80U;
        if (pending >= 112U) {
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = shaBswap64(blockWords[idx]);
            }
            sslSha384Transform(blockWords, ctx->state);
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = 0;
            }
        }
        for (idx = 0; idx < 8U; idx++) {
            blockBytes[112U + idx] = 0;
        }
        for (idx = 0; idx < 8U; idx++) {
            blockBytes[120U + idx] = (u8)(bitCount >> (56U - idx * 8U));
        }
        for (idx = 0; idx < 16U; idx++) {
            blockWords[idx] = shaBswap64(blockWords[idx]);
        }
        sslSha384Transform(blockWords, ctx->state);
        for (idx = 0; idx < 8U; idx++) {
            ctx->state[idx] = shaBswap64(ctx->state[idx]);
        }
        ctx->computed = 1;
        for (idx = 0; idx < 128U; idx++) {
            ctx->blockBuf[idx] = 0;
        }
    }

    for (idx = 0; idx < 48U; idx++) {
        digest[idx] = stateBytes[idx];
    }
    return 0;
}

/* 0x0298DC */
int LIBSSL_NOINLINE sslSha384Init(SceSha512Context *ctx)
{
    if (ctx == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    ctx->state[0] = 0xcbbb9d5dc1059ed8ULL;
    ctx->state[1] = 0x629a292a367cd507ULL;
    ctx->state[2] = 0x9159015a3070dd17ULL;
    ctx->state[3] = 0x152fecd8f70e5939ULL;
    ctx->state[4] = 0x67332667ffc00b31ULL;
    ctx->state[5] = 0x8eb44a8768581511ULL;
    ctx->state[6] = 0xdb0c2e0d64f98fa7ULL;
    ctx->state[7] = 0x47b5481dbefa4fa4ULL;
    ctx->remains = 0;
    ctx->computed = 0;
    ctx->totalLen = 0;
    return 0;
}

/* 0x0299D0 */
int LIBSSL_NOINLINE sslSha512Update(SceSha512Context *ctx, const u8 *input, u32 len)
{
    u64 blockWords[16];
    u8 *blockBytes = (u8 *)blockWords;
    u32 pending;
    u32 total;
    u32 idx;

    if (ctx == NULL || input == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    if (ctx->computed != 0) {
        return SCE_SHA_ERROR_INVALID_STATE;
    }

    pending = ctx->remains;
    total = pending + len;
    ctx->totalLen += (u64)len;

    if (total >= 128U) {
        for (;;) {
            if (pending != 0) {
                u32 fillLen = 128U - pending;
                for (idx = 0; idx < pending; idx++) {
                    blockBytes[idx] = ctx->blockBuf[idx];
                }
                for (idx = 0; idx < fillLen; idx++) {
                    blockBytes[pending + idx] = input[idx];
                }
                input += fillLen;
                ctx->remains = 0;
            } else {
                for (idx = 0; idx < 128U; idx++) {
                    blockBytes[idx] = input[idx];
                }
                input += 128U;
            }
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = shaBswap64(blockWords[idx]);
            }
            total -= 128U;
            sslSha512Transform(blockWords, ctx->state);
            if (total < 128U) {
                break;
            }
            pending = ctx->remains;
        }
    }

    if (total != 0) {
        u32 curRem = ctx->remains;
        u32 copyLen = total - curRem;
        for (idx = 0; idx < copyLen; idx++) {
            ctx->blockBuf[curRem + idx] = input[idx];
        }
        ctx->remains = (u16)total;
    } else {
        ctx->remains = 0;
    }
    return 0;
}

/* 0x029BBC */
int LIBSSL_NOINLINE sslSha512Final(SceSha512Context *ctx, u8 *digest)
{
    u64 blockWords[16];
    u8 *blockBytes = (u8 *)blockWords;
    const u8 *stateBytes = (const u8 *)ctx->state;
    u32 idx;

    if (ctx == NULL || digest == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    if (ctx->computed == 0) {
        u32 pending = ctx->remains;
        u64 bitCount = ctx->totalLen * 8ULL;

        for (idx = 0; idx < 16U; idx++) {
            blockWords[idx] = 0;
        }
        for (idx = 0; idx < pending; idx++) {
            blockBytes[idx] = ctx->blockBuf[idx];
        }
        blockBytes[pending] = 0x80U;
        if (pending >= 112U) {
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = shaBswap64(blockWords[idx]);
            }
            sslSha512Transform(blockWords, ctx->state);
            for (idx = 0; idx < 16U; idx++) {
                blockWords[idx] = 0;
            }
        }
        for (idx = 0; idx < 8U; idx++) {
            blockBytes[112U + idx] = 0;
        }
        for (idx = 0; idx < 8U; idx++) {
            blockBytes[120U + idx] = (u8)(bitCount >> (56U - idx * 8U));
        }
        for (idx = 0; idx < 16U; idx++) {
            blockWords[idx] = shaBswap64(blockWords[idx]);
        }
        sslSha512Transform(blockWords, ctx->state);
        for (idx = 0; idx < 8U; idx++) {
            ctx->state[idx] = shaBswap64(ctx->state[idx]);
        }
        ctx->computed = 1;
        for (idx = 0; idx < 128U; idx++) {
            ctx->blockBuf[idx] = 0;
        }
    }

    for (idx = 0; idx < 64U; idx++) {
        digest[idx] = stateBytes[idx];
    }
    return 0;
}

/* 0x029E14 */
int LIBSSL_NOINLINE sslSha512Init(SceSha512Context *ctx)
{
    if (ctx == NULL) {
        return SCE_SHA_ERROR_INVALID_POINTER;
    }
    ctx->state[0] = 0x6a09e667f3bcc908ULL;
    ctx->state[1] = 0xbb67ae8584caa73bULL;
    ctx->state[2] = 0x3c6ef372fe94f82bULL;
    ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
    ctx->state[4] = 0x510e527fade682d1ULL;
    ctx->state[5] = 0x9b05688c2b3e6c1fULL;
    ctx->state[6] = 0x1f83d9abfb41bd6bULL;
    ctx->state[7] = 0x5be0cd19137e2179ULL;
    ctx->remains = 0;
    ctx->computed = 0;
    ctx->totalLen = 0;
    return 0;
}
