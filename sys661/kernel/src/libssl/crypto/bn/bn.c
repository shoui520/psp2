/*
 * RSA-COMMON / SSL-C 1.4.0 Big Number arithmetic (0x003480..0x007BCB, 63 functions).
 */
#include "libssl_internal.h"

static const signed char g_bnBitsTable[256] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
};

static const u32 g_bnWinSkipZero[1] = { 0 };
static const u32 g_bnWinSkipOne[2] = { 1, 0 };
static const u32 g_bnWinSkipTwo[4] = { 2, 0, 1, 0 };
static const u32 g_bnWinSkipThree[8] = { 3, 0, 1, 0, 2, 0, 1, 0 };
static const u32 g_bnWinSkipFour[16] = {
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};
static const u32 g_bnWinSkipFive[32] = {
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};
static const u32 g_bnWinSkipSix[64] = {
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

static const u32 *const g_bnWinSkipTables[7] = {
    g_bnWinSkipZero, g_bnWinSkipOne, g_bnWinSkipTwo, g_bnWinSkipThree,
    g_bnWinSkipFour, g_bnWinSkipFive, g_bnWinSkipSix
};

static const u8 g_bnExpScriptThree[4] = { 1, 1, 1, 0 };
static const u8 g_bnExpScriptSeventeen[4] = { 1, 4, 1, 0 };
static const u8 g_bnExpScriptFermatFour[4] = { 1, 16, 1, 0 };

static inline void bnFixTop(SslBignum *bn)
{
    while (bn->top > 0 && bn->words[bn->top - 1] == 0) {
        bn->top--;
    }
}

static inline int bnIsZero(const SslBignum *bn)
{
    return (bn->top == 0 || (bn->top == 1 && bn->words[0] == 0));
}

static inline int bnEnsureWords(SslBignum *bn, int needed)
{
    if (bn->maxWords >= needed) {
        return (bn != NULL) ? 1 : 0;
    }
    return (sslBnExpandWords(bn, needed) != NULL) ? 1 : 0;
}

/* 0x003480 */
int LIBSSL_NOINLINE sslBnAdd(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs)
{
    if (lhs->neg == rhs->neg) {
        out->neg = (lhs->neg != 0) ? 1 : 0;
        return (sslBnUadd(out, lhs, rhs) != 0) ? 1 : 0;
    }

    if (lhs->neg != 0) {
        const SslBignum *swapTmp = lhs;
        lhs = rhs;
        rhs = swapTmp;
    }

    if (sslBnUcmp(lhs, rhs) >= 0) {
        if (sslBnUsub(out, lhs, rhs) == 0) {
            return 0;
        }
        out->neg = 0;
        return 1;
    }
    if (sslBnUsub(out, rhs, lhs) == 0) {
        return 0;
    }
    out->neg = 1;
    return 1;
}

/* 0x003554 */
int LIBSSL_NOINLINE sslBnUadd(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs)
{
    const SslBignum *bigOp = lhs;
    const SslBignum *smallOp = rhs;
    int maxLen;
    int minLen;
    u32 carry;
    int idx;
    u32 *dstPtr;
    const u32 *srcPtr;

    if (lhs->top < rhs->top) {
        bigOp = rhs;
        smallOp = lhs;
    }
    maxLen = bigOp->top;
    minLen = smallOp->top;

    if (!bnEnsureWords(out, maxLen + 1)) {
        return 0;
    }

    out->top = maxLen;
    carry = sslBnAddWords(out->words, bigOp->words, smallOp->words, minLen);
    srcPtr = &bigOp->words[minLen];
    dstPtr = &out->words[minLen];
    idx = minLen;

    if (carry != 0) {
        while (idx < maxLen) {
            u32 wordVal = *srcPtr++;
            *dstPtr++ = wordVal + 1U;
            idx++;
            if (wordVal != 0xFFFFFFFFU) {
                carry = 0;
                break;
            }
        }
        if (idx >= maxLen && carry != 0) {
            *dstPtr++ = 1U;
            out->top++;
        }
    }

    if (dstPtr != srcPtr) {
        while (idx < maxLen) {
            *dstPtr++ = *srcPtr++;
            idx++;
        }
    }
    return 1;
}

/* 0x0036CC */
int LIBSSL_NOINLINE sslBnUsub(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs)
{
    int maxLen = lhs->top;
    int minLen = rhs->top;
    const u32 *lhsPtr;
    const u32 *rhsPtr;
    u32 *dstPtr;
    u32 borrow = 0;
    int idx = 0;

    if (maxLen < minLen) {
        return 0;
    }
    if (!bnEnsureWords(out, maxLen)) {
        return 0;
    }

    lhsPtr = lhs->words;
    rhsPtr = rhs->words;
    dstPtr = out->words;

    while (idx < minLen) {
        u32 lhsWord = *lhsPtr++;
        u32 rhsWord = *rhsPtr++;
        u32 diffWord = lhsWord - rhsWord;
        if (borrow != 0) {
            borrow = (lhsWord <= rhsWord) ? 1U : 0U;
            diffWord--;
        } else {
            borrow = (lhsWord < rhsWord) ? 1U : 0U;
        }
        *dstPtr++ = diffWord;
        idx++;
    }

    if (borrow != 0 && idx < maxLen) {
        while (idx < maxLen) {
            u32 lhsWord = *lhsPtr++;
            *dstPtr++ = lhsWord - 1U;
            idx++;
            if (lhsWord != 0) {
                break;
            }
        }
    }

    if (dstPtr != lhsPtr) {
        while (idx < maxLen) {
            *dstPtr++ = *lhsPtr++;
            idx++;
        }
    }

    out->top = maxLen;
    bnFixTop(out);
    return 1;
}

/* 0x00389C */
int LIBSSL_NOINLINE sslBnSub(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs)
{
    int addNeg = 0;
    const SslBignum *firstOp = lhs;
    const SslBignum *secondOp = rhs;
    int maxWords;

    if (lhs->neg != 0) {
        if (rhs->neg == 0) {
            addNeg = 1;
            if (sslBnUadd(out, lhs, rhs) == 0) {
                return 0;
            }
            out->neg = addNeg;
            return 1;
        }
        firstOp = rhs;
        secondOp = lhs;
    } else if (rhs->neg != 0) {
        if (sslBnUadd(out, lhs, rhs) == 0) {
            return 0;
        }
        out->neg = 0;
        return 1;
    }

    maxWords = (firstOp->top > secondOp->top) ? firstOp->top : secondOp->top;
    if (!bnEnsureWords(out, maxWords)) {
        return 0;
    }

    if (sslBnUcmp(firstOp, secondOp) >= 0) {
        if (sslBnUsub(out, firstOp, secondOp) == 0) {
            return 0;
        }
        out->neg = 0;
        return 1;
    }
    if (sslBnUsub(out, secondOp, firstOp) == 0) {
        return 0;
    }
    out->neg = 1;
    return 1;
}

/* 0x0039C4 */
u32 LIBSSL_NOINLINE sslBnMulAddWords(u32 *dst, const u32 *src, int count, u32 multWord)
{
    u32 carry = 0;

    while (count > 0) {
        u64 prod = (u64)(*src++) * (u64)multWord + (u64)(*dst) + (u64)carry;
        *dst++ = (u32)prod;
        carry = (u32)(prod >> 32);
        count--;
    }
    return carry;
}

/* 0x003BEC */
u32 LIBSSL_NOINLINE sslBnMulWords(u32 *dst, const u32 *src, int count, u32 multWord)
{
    u32 carry = 0;

    while (count > 0) {
        u64 prod = (u64)(*src++) * (u64)multWord + (u64)carry;
        *dst++ = (u32)prod;
        carry = (u32)(prod >> 32);
        count--;
    }
    return carry;
}

/* 0x003DD4 */
u32 LIBSSL_NOINLINE sslBnSqrWords(u32 *dst, const u32 *src, int count)
{
    u32 lastLow = 0;

    while (count > 0) {
        u64 sqVal = (u64)(*src) * (u64)(*src);
        src++;
        lastLow = (u32)sqVal;
        dst[0] = lastLow;
        dst[1] = (u32)(sqVal >> 32);
        dst += 2;
        count--;
    }
    return lastLow;
}

/* 0x003F20 */
u32 LIBSSL_NOINLINE sslBnDivWords(u32 hiWord, u32 loWord, u32 divisor)
{
    int bitLen;
    int shiftAmt;
    u32 divHi;
    u32 divLo;
    u32 quotOut = 0;
    int halfIdx;

    if (divisor == 0) {
        return 0xFFFFFFFFU;
    }

    bitLen = sslBnNumBitsWord(divisor);
    if (bitLen != 32 && hiWord > (1U << bitLen)) {
        return 0;
    }

    shiftAmt = 32 - bitLen;
    if (hiWord >= divisor) {
        hiWord -= divisor;
    }
    if (shiftAmt != 0) {
        hiWord = (hiWord << shiftAmt) | (loWord >> (32 - shiftAmt));
        loWord <<= shiftAmt;
        divisor <<= shiftAmt;
    }

    divHi = divisor >> 16;
    divLo = divisor & 0xFFFFU;

    for (halfIdx = 0; halfIdx < 2; halfIdx++) {
        u32 qDigit;
        u32 prodLo;
        u32 prodHi;
        u64 subLow;
        u32 newHi;

        if ((hiWord >> 16) == divHi) {
            qDigit = 0xFFFFU;
        } else {
            qDigit = hiWord / divHi;
        }

        prodLo = qDigit * divLo;
        prodHi = qDigit * divHi;
        while (((hiWord - prodHi) & 0xFFFF0000U) == 0 &&
               (((hiWord - prodHi) << 16) + (loWord >> 16)) < prodLo) {
            qDigit--;
            prodLo -= divLo;
            prodHi -= divHi;
        }

        subLow = (u64)(qDigit * divLo) << 16;
        newHi = qDigit * divHi + (u32)(subLow >> 32) + ((loWord < (u32)subLow) ? 1U : 0U);
        loWord -= (u32)subLow;
        if (hiWord < newHi) {
            hiWord += divisor;
            qDigit--;
        }
        hiWord -= newHi;

        if (halfIdx == 0) {
            quotOut = qDigit << 16;
            hiWord = (hiWord << 16) | (loWord >> 16);
            loWord <<= 16;
        } else {
            quotOut |= qDigit;
        }
    }
    return quotOut;
}

/* 0x0040A0 */
u32 LIBSSL_NOINLINE sslBnAddWords(u32 *dst, const u32 *lhs, const u32 *rhs, int count)
{
    u32 carry = 0;

    while (count > 0) {
        u64 sumVal = (u64)(*lhs++) + (u64)(*rhs++) + (u64)carry;
        *dst++ = (u32)sumVal;
        carry = (u32)(sumVal >> 32);
        count--;
    }
    return carry;
}

/* 0x004164 */
u32 LIBSSL_NOINLINE sslBnSubWords(u32 *dst, const u32 *lhs, const u32 *rhs, int count)
{
    u32 borrow = 0;

    while (count > 0) {
        u32 leftWord = *lhs++;
        u32 rightWord = *rhs++;
        *dst++ = leftWord - rightWord - borrow;
        if (leftWord != rightWord) {
            borrow = (leftWord < rightWord) ? 1U : 0U;
        }
        count--;
    }
    return borrow;
}

/* 0x00422C */
SslBnBlinding *LIBSSL_NOINLINE sslBnBlindingNew(const SslBignum *factorA, const SslBignum *factorAi,
                                                SslBignum *modulus)
{
    SslBnBlinding *blinding = (SslBnBlinding *)sceSsl_lib_0861D934(sizeof(SslBnBlinding));

    if (blinding == NULL) {
        sceSsl_lib_9CDCA0D7(3, 125, 33, NULL, 0);
        return NULL;
    }

    sslMemset(blinding, 0, sizeof(SslBnBlinding));
    blinding->factorA = sslBnNew();
    if (blinding->factorA != NULL) {
        blinding->factorAi = sslBnNew();
        if (blinding->factorAi != NULL &&
            sslBnCopy(blinding->factorA, factorA) != NULL &&
            sslBnCopy(blinding->factorAi, factorAi) != NULL) {
            blinding->modulus = modulus;
            return blinding;
        }
    }
    sslBnBlindingFree(blinding, 1);
    return NULL;
}

/* 0x004310 */
void LIBSSL_NOINLINE sslBnBlindingFree(SslBnBlinding *blinding, int freeSelf)
{
    if (blinding->factorA != NULL) {
        sslBnFree(blinding->factorA);
    }
    if (blinding->factorAi != NULL) {
        sslBnFree(blinding->factorAi);
    }
    if (freeSelf != 0) {
        sceSsl_lib_5E5C873A(blinding);
    }
}

/* 0x004374 */
int LIBSSL_NOINLINE sslBnBlindingUpdate(SslBnBlinding *blinding, SslBnCtx *ctx)
{
    if (blinding->factorA == NULL || blinding->factorAi == NULL) {
        sceSsl_lib_9CDCA0D7(3, 126, 141, NULL, 0);
        return 0;
    }
    if (sslBnModMul(blinding->factorA, blinding->factorA, blinding->factorA, blinding->modulus, ctx) == 0) {
        return 0;
    }
    return (sslBnModMul(blinding->factorAi, blinding->factorAi, blinding->factorAi, blinding->modulus, ctx) != 0) ? 1 : 0;
}

/* 0x00441C */
int LIBSSL_NOINLINE sslBnBlindingConvert(SslBignum *value, SslBnBlinding *blinding, SslBnCtx *ctx)
{
    if (blinding->factorA == NULL || blinding->factorAi == NULL) {
        sceSsl_lib_9CDCA0D7(3, 123, 141, NULL, 0);
        return 0;
    }
    return sslBnModMul(value, value, blinding->factorA, blinding->modulus, ctx);
}

/* 0x00447C */
int LIBSSL_NOINLINE sslBnBlindingInvert(SslBignum *value, SslBnBlinding *blinding, SslBnCtx *ctx)
{
    int mulRet;

    if (blinding->factorA == NULL || blinding->factorAi == NULL) {
        sceSsl_lib_9CDCA0D7(3, 124, 141, NULL, 0);
        return 0;
    }
    mulRet = sslBnModMul(value, value, blinding->factorAi, blinding->modulus, ctx);
    if (mulRet < 0) {
        return mulRet;
    }
    if (sslBnBlindingUpdate(blinding, ctx) == 0) {
        return 0;
    }
    return mulRet;
}

/* 0x00451C */
int LIBSSL_NOINLINE sslBnBlindingCopy(SslBnBlinding *dst, const SslBnBlinding *src)
{
    if (dst->factorA == NULL) {
        dst->factorA = sslBnNew();
        if (dst->factorA == NULL) {
            return 1;
        }
    }
    if (dst->factorAi == NULL) {
        dst->factorAi = sslBnNew();
        if (dst->factorAi == NULL) {
            return 1;
        }
    }
    if (sslBnCopy(dst->factorA, src->factorA) != NULL &&
        sslBnCopy(dst->factorAi, src->factorAi) != NULL) {
        dst->init = src->init;
        dst->modulus = src->modulus;
    }
    return 1;
}

/* 0x0045B8 */
u32 LIBSSL_NOINLINE sslBnMulComba8(u32 *dst, const u32 *lhs, const u32 *rhs)
{
    dst[8] = sslBnMulWords(dst, lhs, 8, rhs[0]);
    dst[9] = sslBnMulAddWords(&dst[1], lhs, 8, rhs[1]);
    dst[10] = sslBnMulAddWords(&dst[2], lhs, 8, rhs[2]);
    dst[11] = sslBnMulAddWords(&dst[3], lhs, 8, rhs[3]);
    dst[12] = sslBnMulAddWords(&dst[4], lhs, 8, rhs[4]);
    dst[13] = sslBnMulAddWords(&dst[5], lhs, 8, rhs[5]);
    dst[14] = sslBnMulAddWords(&dst[6], lhs, 8, rhs[6]);
    dst[15] = sslBnMulAddWords(&dst[7], lhs, 8, rhs[7]);
    return dst[15];
}

/* 0x0046A8 */
void LIBSSL_NOINLINE sslBnSqrComba8(u32 *dst, const u32 *src)
{
    u32 scratch[16];
    sslBnSqrNormal(dst, src, 8, scratch);
}

/* 0x0046C8 */
int LIBSSL_NOINLINE sslBnDiv(SslBignum *quotient, SslBignum *remainder, const SslBignum *dividend,
                             const SslBignum *divisor, SslBnCtx *ctx)
{
    SslBignum *tmpBn;
    SslBignum *snum;
    SslBignum *sdiv;
    SslBignum *res;
    SslBignum wnum;
    int normShift;
    int divLen;
    int numLen;
    int loopCount;
    u32 dTop;
    u32 dNext = 0;
    u32 *wnumTopPtr;
    u32 *resPtr;
    int stepIdx;

    if (bnIsZero(divisor)) {
        return 0;
    }

    if (sslBnUcmp(dividend, divisor) < 0) {
        if (remainder != NULL && sslBnCopy(remainder, dividend) == NULL) {
            return 0;
        }
        if (quotient != NULL) {
            sslBnSetWord(quotient, 0);
        }
        return 1;
    }

    tmpBn = &ctx->pool[ctx->tos];
    snum = &ctx->pool[ctx->tos + 1];
    sdiv = &ctx->pool[ctx->tos + 2];
    res = (quotient != NULL) ? quotient : &ctx->pool[ctx->tos + 3];
    tmpBn->neg = 0;

    normShift = 32 - (sslBnNumBits(divisor) % 32);
    sslBnLshift(sdiv, divisor, normShift);
    sdiv->neg = 0;
    normShift += 32;
    sslBnLshift(snum, dividend, normShift);
    snum->neg = 0;

    divLen = sdiv->top;
    numLen = snum->top;
    loopCount = numLen - divLen;

    sslBnInit(&wnum);
    wnum.words = &snum->words[loopCount];
    wnum.top = divLen;
    wnum.maxWords = snum->maxWords - loopCount;

    dTop = sdiv->words[divLen - 1];
    if (divLen != 1) {
        dNext = sdiv->words[divLen - 2];
    }

    if (!bnEnsureWords(res, loopCount + 1)) {
        return 0;
    }
    res->neg = dividend->neg ^ divisor->neg;
    res->top = loopCount;

    if (!bnEnsureWords(tmpBn, divLen + 1)) {
        return 0;
    }

    wnumTopPtr = &snum->words[numLen - 1];
    resPtr = &res->words[loopCount - 1];

    if (sslBnUcmp(&wnum, sdiv) >= 0) {
        if (sslBnUsub(&wnum, &wnum, sdiv) == 0) {
            return 0;
        }
        *resPtr = 1U;
    } else {
        res->top--;
    }

    resPtr--;
    for (stepIdx = 0; stepIdx < loopCount - 1; stepIdx++) {
        u32 nTop;
        u32 nNext;
        u32 qEst;
        int prevTop;

        wnum.words--;
        wnum.top++;
        nTop = wnumTopPtr[0];
        nNext = wnumTopPtr[-1];

        if (nTop == dTop) {
            qEst = 0xFFFFFFFFU;
        } else {
            qEst = sslBnDivWords(nTop, nNext, dTop);
        }

        for (;;) {
            u64 mulTop = (u64)qEst * (u64)dTop;
            u64 remPair = (((u64)nTop << 32) | (u64)nNext) - mulTop;
            u64 mulNext = (u64)qEst * (u64)dNext;
            if ((remPair >> 32) != 0) {
                break;
            }
            if ((u32)(mulNext >> 32) < (u32)remPair) {
                break;
            }
            if ((u32)(mulNext >> 32) == (u32)remPair &&
                (u32)mulNext <= wnumTopPtr[-2]) {
                break;
            }
            qEst--;
        }

        tmpBn->words[divLen] = sslBnMulWords(tmpBn->words, sdiv->words, divLen, qEst);
        tmpBn->top = divLen + 1;
        bnFixTop(tmpBn);

        prevTop = wnum.top;
        sslBnSub(&wnum, &wnum, tmpBn);
        snum->top += wnum.top - prevTop;
        if (wnum.neg != 0) {
            prevTop = wnum.top;
            sslBnAdd(&wnum, &wnum, sdiv);
            qEst--;
            snum->top += wnum.top - prevTop;
        }
        *resPtr-- = qEst;
        wnumTopPtr--;
    }

    bnFixTop(snum);
    if (remainder != NULL) {
        sslBnRshift(remainder, snum, normShift);
        remainder->neg = dividend->neg;
    }
    return 1;
}

/* 0x004C28 */
int LIBSSL_NOINLINE sslBnMod(SslBignum *remainder, const SslBignum *dividend,
                             const SslBignum *divisor, SslBnCtx *ctx)
{
    return sslBnDiv(NULL, remainder, dividend, divisor, ctx);
}

/* 0x004C5C */
SslBignum *LIBSSL_NOINLINE sslBnModInverse(SslBignum *out, const SslBignum *value,
                                           const SslBignum *modulus, SslBnCtx *ctx)
{
    SslBignum *valA;
    SslBignum *valB;
    SslBignum *valX;
    SslBignum *valD;
    SslBignum *valM;
    SslBignum *valY;
    SslBignum *retBn = NULL;
    SslBignum *resultBn = out;
    int signVal = 1;

    valA = &ctx->pool[ctx->tos];
    valB = &ctx->pool[ctx->tos + 1];
    valX = &ctx->pool[ctx->tos + 2];
    valD = &ctx->pool[ctx->tos + 3];
    valM = &ctx->pool[ctx->tos + 4];
    valY = &ctx->pool[ctx->tos + 5];
    ctx->tos += 6;

    if (resultBn == NULL) {
        resultBn = sslBnNew();
    }
    if (resultBn != NULL) {
        sslBnSetWord(valX, 0);
        sslBnSetWord(valY, 1);

        if (sslBnCopy(valA, value) != NULL && sslBnCopy(valB, modulus) != NULL) {
            int ok = 1;
            while (!bnIsZero(valB)) {
                SslBignum *oldA = valA;
                if (sslBnDiv(valD, valM, valA, valB, ctx) == 0) {
                    ok = 0;
                    break;
                }
                valA = valB;
                valB = valM;
                if (sslBnMul(oldA, valD, valX, ctx) == 0) {
                    ok = 0;
                    break;
                }
                if (sslBnAdd(oldA, oldA, valY) == 0) {
                    ok = 0;
                    break;
                }
                valM = valY;
                valY = valX;
                valX = oldA;
                signVal = -signVal;
            }
            if (ok) {
                if (signVal < 0) {
                    if (sslBnSub(valY, modulus, valY) == 0) {
                        ok = 0;
                    }
                }
                if (ok && valA->top == 1 && valA->words[0] == 1U) {
                    if (sslBnMod(resultBn, valY, modulus, ctx) != 0) {
                        retBn = resultBn;
                    }
                }
            }
        }
    }

    if (retBn == NULL && out == NULL) {
        sslBnFree(resultBn);
    }
    ctx->tos -= 6;
    return retBn;
}

/* 0x004E94 */
int LIBSSL_NOINLINE sslBnNumBitsWord(u32 word)
{
    if ((word & 0xFFFF0000U) != 0) {
        if ((word & 0xFF000000U) != 0) {
            return (int)g_bnBitsTable[word >> 24] + 24;
        }
        return (int)g_bnBitsTable[word >> 16] + 16;
    }
    if ((word & 0xFF00U) != 0) {
        return (int)g_bnBitsTable[word >> 8] + 8;
    }
    return (int)g_bnBitsTable[word];
}

/* 0x004F14 */
int LIBSSL_NOINLINE sslBnNumBits(const SslBignum *bn)
{
    if (bn->top == 0) {
        return 0;
    }
    if (bn->words[bn->top - 1] == 0) {
        return 0;
    }
    return (bn->top - 1) * 32 + sslBnNumBitsWord(bn->words[bn->top - 1]);
}

/* 0x004F6C */
void LIBSSL_NOINLINE sslBnClearFree(SslBignum *bn)
{
    int savedFlags;

    if (bn == NULL) {
        return;
    }
    if (bn->words != NULL) {
        sslMemset(bn->words, 0, (u32)bn->maxWords * 4U);
        if ((bn->flags & 2) == 0) {
            sceSsl_lib_5E5C873A(bn->words);
        }
    }
    savedFlags = bn->flags;
    sslMemset(bn, 0, sizeof(SslBignum));
    if ((savedFlags & 1) != 0) {
        sceSsl_lib_5E5C873A(bn);
    }
}

/* 0x004FF4 */
void LIBSSL_NOINLINE sslBnFree(SslBignum *bn)
{
    if (bn == NULL) {
        return;
    }
    if (bn->words != NULL && (bn->flags & 2) == 0) {
        sceSsl_lib_5E5C873A(bn->words);
    }
    bn->flags |= 0x8000;
    if ((bn->flags & 1) != 0) {
        sceSsl_lib_5E5C873A(bn);
    }
}

/* 0x005058 */
void LIBSSL_NOINLINE sslBnInit(SslBignum *bn)
{
    sslMemset(bn, 0, sizeof(SslBignum));
}

/* 0x005078 */
SslBignum *LIBSSL_NOINLINE sslBnNew(void)
{
    SslBignum *bn = (SslBignum *)sceSsl_lib_0861D934(sizeof(SslBignum));
    if (bn == NULL) {
        return NULL;
    }
    sslMemset(bn, 0, sizeof(SslBignum));
    bn->flags = 1;
    return bn;
}

/* 0x0050C8 */
SslBnCtx *LIBSSL_NOINLINE sslBnCtxNew(void)
{
    SslBnCtx *ctx = (SslBnCtx *)sceSsl_lib_0861D934(sizeof(SslBnCtx));
    if (ctx == NULL) {
        return NULL;
    }
    sslBnCtxInit(ctx);
    ctx->flags = 1;
    return ctx;
}

/* 0x005110 */
void LIBSSL_NOINLINE sslBnCtxInit(SslBnCtx *ctx)
{
    sslMemset(ctx, 0, sizeof(SslBnCtx));
    ctx->flags = 0;
    ctx->tos = 0;
}

/* 0x005144 */
void LIBSSL_NOINLINE sslBnCtxFree(SslBnCtx *ctx)
{
    int idx;
    for (idx = 0; idx < 12; idx++) {
        sslBnClearFree(&ctx->pool[idx]);
    }
    if ((ctx->flags & 1) != 0) {
        sceSsl_lib_5E5C873A(ctx);
    }
}

/* 0x0051A8 */
SslBignum *LIBSSL_NOINLINE sslBnExpandWords(SslBignum *bn, int neededWords)
{
    u32 *newWords;

    if (bn->maxWords >= neededWords) {
        return bn;
    }
    if ((bn->flags & 2) != 0) {
        return NULL;
    }

    newWords = (u32 *)sceSsl_lib_0861D934((u32)(neededWords + 1) * 4U);
    if (newWords == NULL) {
        return NULL;
    }

    if (bn->words != NULL) {
        int idx;
        for (idx = 0; idx < bn->top; idx++) {
            newWords[idx] = bn->words[idx];
        }
        sceSsl_lib_5E5C873A(bn->words);
    }
    bn->words = newWords;
    bn->maxWords = neededWords;
    return bn;
}

/* 0x0052F8 */
SslBignum *LIBSSL_NOINLINE sslBnDup(const SslBignum *src)
{
    SslBignum *copy = sslBnNew();
    if (copy == NULL) {
        return NULL;
    }
    return sslBnCopy(copy, src);
}

/* 0x00533C */
SslBignum *LIBSSL_NOINLINE sslBnCopy(SslBignum *dst, const SslBignum *src)
{
    int idx;

    if (dst == src) {
        return dst;
    }
    if (!bnEnsureWords(dst, src->top)) {
        return NULL;
    }
    for (idx = 0; idx < src->top; idx++) {
        dst->words[idx] = src->words[idx];
    }
    dst->top = src->top;
    if (src->top == 0 && dst->words != NULL) {
        dst->words[0] = 0;
    }
    dst->neg = src->neg;
    return dst;
}

/* 0x005490 */
u32 LIBSSL_NOINLINE sslBnGetWord(const SslBignum *bn)
{
    int byteCount = (sslBnNumBits(bn) + 7) / 8;
    int idx;
    u32 res = 0;

    if (byteCount >= 5) {
        return 0xFFFFFFFFU;
    }
    for (idx = bn->top - 1; idx >= 0; idx--) {
        res = bn->words[idx];
    }
    return res;
}

/* 0x005508 */
int LIBSSL_NOINLINE sslBnSetWord(SslBignum *bn, u32 word)
{
    if (bn->maxWords <= 0) {
        if (sslBnExpandWords(bn, 2) == NULL) {
            return 0;
        }
    } else if (bn == NULL) {
        return 0;
    }
    bn->neg = 0;
    bn->words[0] = word;
    bn->top = (word != 0) ? 1 : 0;
    return 1;
}

/* 0x005588 */
SslBignum *LIBSSL_NOINLINE sslBnBinToBn(const u8 *srcBytes, int byteLen, SslBignum *outBn)
{
    SslBignum *bn = outBn;
    int wordIdx;
    int byteInWord;
    u32 accWord = 0;
    int remBytes;

    if (bn == NULL) {
        bn = sslBnNew();
        if (bn == NULL) {
            return NULL;
        }
    }
    if (byteLen == 0) {
        bn->top = 0;
        return bn;
    }
    if (!bnEnsureWords(bn, (byteLen * 8 + 16) / 32 + 1)) {
        return NULL;
    }

    remBytes = byteLen - 1;
    wordIdx = (remBytes >> 2) + 1;
    bn->top = wordIdx;
    byteInWord = remBytes & 3;

    for (;;) {
        accWord = (accWord << 8) | *srcBytes++;
        byteInWord--;
        if (byteInWord < 0) {
            wordIdx--;
            bn->words[wordIdx] = accWord;
            byteInWord = 3;
            accWord = 0;
        }
        if (remBytes == 0) {
            break;
        }
        remBytes--;
    }

    bnFixTop(bn);
    return bn;
}

/* 0x0056F0 */
int LIBSSL_NOINLINE sslBnBnToBin(const SslBignum *bn, u8 *outBytes)
{
    int byteCount = (sslBnNumBits(bn) + 7) / 8;
    int byteIdx = byteCount - 1;

    while (byteIdx >= 0) {
        *outBytes++ = (u8)(bn->words[byteIdx / 4] >> ((byteIdx % 4) * 8));
        byteIdx--;
    }
    return byteCount;
}

/* 0x00577C */
int LIBSSL_NOINLINE sslBnUcmp(const SslBignum *lhs, const SslBignum *rhs)
{
    int idx;

    if (lhs->top != rhs->top) {
        return lhs->top - rhs->top;
    }
    for (idx = lhs->top - 1; idx >= 0; idx--) {
        if (lhs->words[idx] != rhs->words[idx]) {
            return (lhs->words[idx] > rhs->words[idx]) ? 1 : -1;
        }
    }
    return 0;
}

/* 0x0057E4 */
int LIBSSL_NOINLINE sslBnCmp(const SslBignum *lhs, const SslBignum *rhs)
{
    int gtVal;
    int ltVal;
    int idx;

    if (lhs == NULL || rhs == NULL) {
        if (lhs != NULL) {
            return -1;
        }
        return (rhs != NULL) ? 1 : 0;
    }
    if (lhs->neg != rhs->neg) {
        return (lhs->neg != 0) ? -1 : 1;
    }
    if (lhs->neg == 0) {
        gtVal = 1;
        ltVal = -1;
    } else {
        gtVal = -1;
        ltVal = 1;
    }
    if (lhs->top > rhs->top) {
        return gtVal;
    }
    if (lhs->top < rhs->top) {
        return ltVal;
    }
    for (idx = lhs->top - 1; idx >= 0; idx--) {
        if (lhs->words[idx] > rhs->words[idx]) {
            return gtVal;
        }
        if (lhs->words[idx] < rhs->words[idx]) {
            return ltVal;
        }
    }
    return 0;
}

/* 0x0058A8 */
int LIBSSL_NOINLINE sslBnSetBit(SslBignum *bn, int bitIndex)
{
    int wordIdx = bitIndex / 32;
    int bitPos = bitIndex % 32;

    if (wordIdx >= bn->top) {
        int clearIdx;
        if (!bnEnsureWords(bn, wordIdx + 1)) {
            return 0;
        }
        for (clearIdx = bn->top; clearIdx < wordIdx + 1; clearIdx++) {
            bn->words[clearIdx] = 0;
        }
        bn->top = wordIdx + 1;
    }
    bn->words[wordIdx] |= (1U << bitPos);
    return 1;
}

/* 0x005998 */
int LIBSSL_NOINLINE sslBnMaskBits(SslBignum *bn, int bitCount)
{
    int wordIdx = bitCount / 32;
    int bitPos = bitCount % 32;

    if (wordIdx >= bn->top) {
        return 0;
    }
    if (bitPos == 0) {
        bn->top = wordIdx;
    } else {
        bn->words[wordIdx] &= ~((u32)0xFFFFFFFFU << bitPos);
        bn->top = wordIdx + 1;
    }
    bnFixTop(bn);
    return 1;
}

/* 0x005A40 */
int LIBSSL_NOINLINE sslBnLshiftOne(SslBignum *out, const SslBignum *src)
{
    u32 carry = 0;
    int idx;
    u32 lastWord = 0;

    if (out != src) {
        if (!bnEnsureWords(out, src->top + 1)) {
            return 0;
        }
        out->neg = src->neg;
        out->top = src->top;
    } else {
        if (!bnEnsureWords(out, out->top + 1)) {
            return 0;
        }
    }

    for (idx = 0; idx < src->top; idx++) {
        lastWord = src->words[idx];
        out->words[idx] = (lastWord << 1) | carry;
        carry = lastWord >> 31;
    }
    if (carry != 0) {
        out->words[src->top] = 1U;
        out->top++;
    }
    return 1;
}

/* 0x005B54 */
int LIBSSL_NOINLINE sslBnLshift(SslBignum *out, const SslBignum *src, int shiftBits)
{
    int wordShift;
    int bitShift;
    int idx;

    if (shiftBits == 1) {
        return sslBnLshiftOne(out, src);
    }

    wordShift = shiftBits / 32;
    bitShift = shiftBits % 32;
    if (!bnEnsureWords(out, src->top + wordShift + 1)) {
        return 0;
    }

    out->neg = src->neg;
    out->words[src->top + wordShift] = 0;

    if (bitShift == 0) {
        for (idx = src->top - 1; idx >= 0; idx--) {
            out->words[idx + wordShift] = src->words[idx];
        }
    } else {
        int rShift = 32 - bitShift;
        for (idx = src->top - 1; idx >= 0; idx--) {
            u32 curWord = src->words[idx];
            out->words[idx + wordShift + 1] |= (curWord >> rShift);
            out->words[idx + wordShift] = (curWord << bitShift);
        }
    }

    sslMemset(out->words, 0, (u32)wordShift * 4U);
    out->top = src->top + wordShift + 1;
    bnFixTop(out);
    return 1;
}

/* 0x005D14 */
int LIBSSL_NOINLINE sslBnEncodeWindow(u8 *outScript, const SslBignum *exponent, int winBits)
{
    const u32 *skipTab;
    u32 mask;
    u32 bitPos = 0;
    u32 nextWord = 0;
    u32 curWord;
    int remWords;
    const u32 *wordPtr;
    u8 *writePtr;
    u8 *readPtr;
    u8 sqCount = 0;
    int outLen = 1;

    if (winBits > 6) {
        winBits = 6;
    }
    skipTab = g_bnWinSkipTables[winBits];
    mask = (1U << winBits) - 1U;
    writePtr = &outScript[2 * ((32 * exponent->top + winBits - 1) / winBits)];
    writePtr[1] = 0;
    writePtr[2] = 0;

    remWords = exponent->top;
    wordPtr = exponent->words;
    curWord = *wordPtr++;
    if (remWords >= 2) {
        nextWord = *wordPtr++;
    }

    for (;;) {
        u32 windowVal = (bitPos != 0) ? ((curWord >> bitPos) | (nextWord << (32U - bitPos))) : curWord;
        u32 lowBits = windowVal & mask;
        u32 skipStep = skipTab[lowBits];

        if (skipStep != 0) {
            bitPos += skipStep;
            sqCount = (u8)(sqCount + skipStep);
            if (bitPos >= 32U) {
                if (remWords >= 2) {
                    remWords--;
                    curWord = nextWord;
                    nextWord = (remWords >= 2) ? *wordPtr++ : 0;
                    bitPos -= 32U;
                    continue;
                }
            } else {
                continue;
            }
        }

        bitPos += (u32)winBits;
        if (lowBits == 0) {
            break;
        }
        *writePtr = sqCount;
        writePtr--;
        *writePtr = (u8)lowBits;
        sqCount = (u8)winBits;
        writePtr--;
        if (bitPos >= 32U) {
            if (remWords >= 2) {
                remWords--;
                curWord = nextWord;
                nextWord = (remWords >= 2) ? *wordPtr++ : 0;
                bitPos -= 32U;
            } else {
                break;
            }
        }
    }

    readPtr = writePtr + 1;
    while (*readPtr != 0) {
        *outScript++ = *readPtr++;
        *outScript++ = *readPtr++;
        outLen += 2;
    }
    outScript[0] = 0;
    outScript[1] = 0;
    return outLen;
}

/* 0x005E8C */
int LIBSSL_NOINLINE sslBnModExpMont(SslBignum *out, const SslBignum *base, const SslBignum *exponent,
                                    const SslBignum *modulus, SslBnCtx *ctx, SslBnMontCtx *montCtx)
{
    int savedTos = ctx->tos;
    int retOk = 0;
    SslBnMontCtx *activeMont = montCtx;
    const SslBignum *baseReduced;
    const u8 *expScript = NULL;
    int modWords;
    int winBits;
    int tableCount;
    int estFactor;
    int tableWords;
    int scriptWords;
    SslBignum *tblBn;
    SslBignum *sqrBn;
    SslBignum *accBn;
    SslBignum *prodBn;
    u32 *powerTable[16];
    u32 *accWords;
    u32 *prodWords;
    u32 *sqrWords;
    const u32 *rrWords;
    const u32 *modPtr;
    u32 invWord;
    int idx;
    u8 sqSteps;
    u8 multDigit;
    int didFinalMul = 0;

    if ((modulus->words[0] & 1U) == 0) {
        return 0;
    }
    if (bnIsZero(base) || bnIsZero(exponent)) {
        sslBnSetWord(out, 1);
        return 1;
    }
    if (exponent->top == 1 && exponent->words[0] == 1U) {
        sslBnCopy(out, base);
        return 1;
    }

    if (activeMont == NULL) {
        activeMont = sslBnMontCtxNew();
        if (activeMont == NULL) {
            return 0;
        }
        if (sslBnMontCtxSet(activeMont, modulus, ctx) == 0) {
            sslBnMontCtxFree(activeMont);
            ctx->tos = savedTos;
            return 0;
        }
    }

    modWords = activeMont->modVal.top;
    if (base->top == modWords && base->words[modWords - 1] < modulus->words[modWords - 1]) {
        baseReduced = base;
    } else if (base->top < modWords) {
        SslBignum *padded = &ctx->pool[ctx->tos++];
        padded->top = base->top;
        if (padded->maxWords < modWords) {
            sslBnExpandWords(padded, modWords);
        }
        for (idx = padded->top; idx < modWords; idx++) {
            padded->words[idx] = 0;
        }
        for (idx = 0; idx < base->top; idx++) {
            padded->words[idx] = base->words[idx];
        }
        baseReduced = padded;
    } else {
        SslBignum *reduced = &ctx->pool[ctx->tos++];
        if (sslBnMod(reduced, base, modulus, ctx) == 0) {
            if (montCtx == NULL) {
                sslBnMontCtxFree(activeMont);
            }
            ctx->tos = savedTos;
            return 0;
        }
        if (reduced->top < modWords) {
            if (reduced->maxWords < modWords) {
                sslBnExpandWords(reduced, modWords);
            }
            for (idx = reduced->top; idx < modWords; idx++) {
                reduced->words[idx] = 0;
            }
        }
        baseReduced = reduced;
    }

    if (exponent->top == 1) {
        u32 expWord = exponent->words[0];
        if (expWord == 0x10001U) {
            expScript = g_bnExpScriptFermatFour;
        } else if (expWord == 17U) {
            expScript = g_bnExpScriptSeventeen;
        } else if (expWord == 3U) {
            expScript = g_bnExpScriptThree;
        }
        winBits = 1;
        tableCount = 1;
        estFactor = 32;
    } else {
        int expBits = exponent->top * 32;
        if (expBits < 128) {
            winBits = 3;
            estFactor = 11;
            tableCount = 4;
        } else if (expBits < 256) {
            winBits = 4;
            estFactor = 8;
            tableCount = 8;
        } else {
            winBits = 5;
            estFactor = 7;
            tableCount = 16;
        }
    }

    tableWords = tableCount * modWords;
    scriptWords = tableWords + (2 * (exponent->top * estFactor) + 7) / 4;

    tblBn = &ctx->pool[ctx->tos];
    sqrBn = &ctx->pool[ctx->tos + 1];
    accBn = &ctx->pool[ctx->tos + 2];
    prodBn = &ctx->pool[ctx->tos + 3];
    ctx->tos += 4;

    if (out->maxWords < modWords) {
        sslBnExpandWords(out, modWords);
    }
    if (sqrBn->maxWords < modWords * 4) {
        sslBnExpandWords(sqrBn, modWords * 4);
    }
    if (accBn->maxWords < modWords * 2) {
        sslBnExpandWords(accBn, modWords * 2);
    }
    if (tblBn->maxWords < scriptWords) {
        sslBnExpandWords(tblBn, scriptWords);
    }
    if (prodBn->maxWords < modWords * 2) {
        sslBnExpandWords(prodBn, modWords * 2);
    }

    sqrWords = sqrBn->words;
    accWords = accBn->words;
    prodWords = prodBn->words;
    rrWords = activeMont->rrVal.words;
    modPtr = activeMont->modVal.words;
    invWord = activeMont->invLowWord;

    if (expScript == NULL) {
        u8 *scriptBuf = (u8 *)(&tblBn->words[tableWords]);
        sslBnEncodeWindow(scriptBuf, exponent, winBits);
        expScript = scriptBuf;
    }

    powerTable[0] = tblBn->words;
    if (modWords == 8) {
        sslBnMulNormal(prodWords, baseReduced->words, 8, rrWords, 8);
        sslBnMontReduceWords(powerTable[0], prodWords, modPtr, 8, invWord);
        if (tableCount >= 2) {
            sslBnSqrComba8(prodWords, powerTable[0]);
            sslBnMontReduceWords(sqrWords, prodWords, modPtr, 8, invWord);
            for (idx = 1; idx < tableCount; idx++) {
                powerTable[idx] = powerTable[idx - 1] + 8;
                sslBnMulComba8(prodWords, powerTable[idx - 1], sqrWords);
                sslBnMontReduceWords(powerTable[idx], prodWords, modPtr, 8, invWord);
            }
        }

        multDigit = expScript[0];
        sqSteps = expScript[1];
        expScript += 2;
        sslMemcpy(accWords, powerTable[multDigit >> 1], 32);

        while (sqSteps != 0) {
            do {
                sslBnSqrComba8(prodWords, accWords);
                sslBnMontReduceWords(accWords, prodWords, modPtr, 8, invWord);
                sqSteps--;
            } while (sqSteps != 0);

            multDigit = expScript[0];
            sqSteps = expScript[1];
            expScript += 2;
            if ((multDigit | sqSteps) == 0) {
                break;
            }
            if ((ctx->flags & 0x4000) != 0) {
                if (montCtx == NULL) {
                    sslBnMontCtxFree(activeMont);
                }
                ctx->tos = savedTos;
                return 0;
            }
            if (sqSteps == 0 && multDigit == 1U) {
                sslBnMulComba8(prodWords, accWords, baseReduced->words);
                didFinalMul = 1;
                break;
            }
            sslBnMulComba8(prodWords, accWords, powerTable[multDigit >> 1]);
            sslBnMontReduceWords(accWords, prodWords, modPtr, 8, invWord);
        }
    } else {
        sslBnMulNormal(prodWords, baseReduced->words, modWords, rrWords, modWords);
        sslBnMontReduceWords(powerTable[0], prodWords, modPtr, modWords, invWord);
        if (tableCount >= 2) {
            sslBnSqrNormal(prodWords, powerTable[0], modWords, sqrWords);
            sslBnMontReduceWords(sqrWords, prodWords, modPtr, modWords, invWord);
            for (idx = 1; idx < tableCount; idx++) {
                powerTable[idx] = powerTable[idx - 1] + modWords;
                sslBnMulNormal(prodWords, powerTable[idx - 1], modWords, sqrWords, modWords);
                sslBnMontReduceWords(powerTable[idx], prodWords, modPtr, modWords, invWord);
            }
        }

        multDigit = expScript[0];
        sqSteps = expScript[1];
        expScript += 2;
        sslMemcpy(accWords, powerTable[multDigit >> 1], (u32)modWords * 4U);

        while (sqSteps != 0) {
            do {
                sslBnSqrNormal(prodWords, accWords, modWords, sqrWords);
                sslBnMontReduceWords(accWords, prodWords, modPtr, modWords, invWord);
                sqSteps--;
            } while (sqSteps != 0);

            multDigit = expScript[0];
            sqSteps = expScript[1];
            expScript += 2;
            if ((multDigit | sqSteps) == 0) {
                break;
            }
            if ((ctx->flags & 0x4000) != 0) {
                if (montCtx == NULL) {
                    sslBnMontCtxFree(activeMont);
                }
                ctx->tos = savedTos;
                return 0;
            }
            if (sqSteps == 0 && multDigit == 1U) {
                sslBnMulNormal(prodWords, accWords, modWords, baseReduced->words, modWords);
                didFinalMul = 1;
                break;
            }
            sslBnMulNormal(prodWords, accWords, modWords, powerTable[multDigit >> 1], modWords);
            sslBnMontReduceWords(accWords, prodWords, modPtr, modWords, invWord);
        }
    }

    if (didFinalMul) {
        sslBnMontReduceWords(out->words, prodWords, modPtr, modWords, invWord);
    } else {
        for (idx = modWords; idx < modWords * 2; idx++) {
            accWords[idx] = 0;
        }
        sslBnMontReduceWords(out->words, accWords, modPtr, modWords, invWord);
    }

    out->top = modWords;
    bnFixTop(out);
    retOk = 1;

    if (montCtx == NULL && activeMont != NULL) {
        sslBnMontCtxFree(activeMont);
    }
    ctx->tos = savedTos;
    return retOk;
}

/* 0x006870 */
int LIBSSL_NOINLINE sslBnModMul(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs,
                                const SslBignum *modulus, SslBnCtx *ctx)
{
    SslBignum *tmpBn = &ctx->pool[ctx->tos++];
    int mulOk;
    int retOk = 0;

    if (lhs == rhs) {
        mulOk = sslBnSqr(tmpBn, lhs, ctx);
    } else {
        mulOk = sslBnMul(tmpBn, lhs, rhs, ctx);
    }
    if (mulOk != 0 && sslBnMod(out, tmpBn, modulus, ctx) != 0) {
        retOk = 1;
    }
    ctx->tos--;
    return retOk;
}

/* 0x006930 */
u32 LIBSSL_NOINLINE sslBnMontReduceWords(u32 *dst, u32 *accBuf, const u32 *modWords,
                                         int wordCount, u32 invWord)
{
    u32 carry = 0;
    int idx;
    int needSub = 0;

    for (idx = 0; idx < wordCount; idx++) {
        u32 multVal = accBuf[idx] * invWord;
        u32 addCarry = sslBnMulAddWords(&accBuf[idx], modWords, wordCount, multVal);
        u32 sumCarry = addCarry + carry;
        u32 overflowOne = (sumCarry < carry) ? 1U : 0U;
        u32 newHi = accBuf[idx + wordCount] + sumCarry;
        u32 overflowTwo = (newHi < sumCarry) ? 1U : 0U;
        accBuf[idx + wordCount] = newHi;
        carry = overflowOne + overflowTwo;
    }

    if (carry != 0 || modWords[wordCount - 1] < accBuf[2 * wordCount - 1]) {
        return sslBnSubWords(dst, &accBuf[wordCount], modWords, wordCount);
    }

    for (idx = wordCount - 1; idx >= 0; idx--) {
        u32 accWord = accBuf[wordCount + idx];
        u32 modWord = modWords[idx];
        if (accWord > modWord) {
            needSub = 1;
            break;
        }
        if (accWord < modWord) {
            break;
        }
    }

    if (needSub) {
        return sslBnSubWords(dst, &accBuf[wordCount], modWords, wordCount);
    }

    for (idx = 0; idx < wordCount; idx++) {
        dst[idx] = accBuf[wordCount + idx];
    }
    return (u32)wordCount * 4U;
}

/* 0x006ACC */
SslBnMontCtx *LIBSSL_NOINLINE sslBnMontCtxNew(void)
{
    SslBnMontCtx *mont = (SslBnMontCtx *)sceSsl_lib_0861D934(sizeof(SslBnMontCtx));
    if (mont == NULL) {
        return NULL;
    }
    sslBnMontCtxInit(mont);
    mont->flags = 1;
    return mont;
}

/* 0x006B14 */
void LIBSSL_NOINLINE sslBnMontCtxInit(SslBnMontCtx *mont)
{
    mont->ri = 0;
    mont->wordCount = 0;
    sslBnInit(&mont->rrVal);
    sslBnInit(&mont->modVal);
    sslBnInit(&mont->invVal);
    mont->flags = 0;
}

/* 0x006B58 */
void LIBSSL_NOINLINE sslBnMontCtxFree(SslBnMontCtx *mont)
{
    sslBnFree(&mont->rrVal);
    sslBnFree(&mont->modVal);
    sslBnFree(&mont->invVal);
    if ((mont->flags & 1) != 0) {
        sceSsl_lib_5E5C873A(mont);
    }
}

/* 0x006BA8 */
int LIBSSL_NOINLINE sslBnMontCtxSet(SslBnMontCtx *mont, const SslBignum *modulus, SslBnCtx *ctx)
{
    SslBignum rInv;
    SslBignum lowMod;
    u32 lowWords[2];
    int wordCount;
    int idx;

    sslBnInit(&rInv);
    sslBnCopy(&mont->modVal, modulus);
    mont->ri = 1;
    wordCount = (sslBnNumBits(modulus) + 31) / 32;
    mont->wordCount = wordCount;

    sslBnSetWord(&mont->rrVal, 0);
    sslBnSetBit(&mont->rrVal, 32);

    lowWords[0] = modulus->words[0];
    lowWords[1] = 0;
    lowMod.words = lowWords;
    lowMod.top = 1;
    lowMod.maxWords = 2;
    lowMod.neg = modulus->neg;
    lowMod.flags = 0;

    if (sslBnModInverse(&rInv, &mont->rrVal, &lowMod, ctx) == NULL) {
        return 0;
    }

    sslBnLshift(&rInv, &rInv, 32);
    if (!bnIsZero(&rInv)) {
        sslBnSubWord(&rInv, 1U);
    } else {
        sslBnSetWord(&rInv, 0xFFFFFFFFU);
    }
    sslBnDiv(&rInv, NULL, &rInv, &lowMod, ctx);
    mont->invLowWord = rInv.words[0];
    sslBnFree(&rInv);

    sslBnSetWord(&mont->rrVal, 0);
    sslBnSetBit(&mont->rrVal, wordCount * 64);
    sslBnMod(&mont->rrVal, &mont->rrVal, &mont->modVal, ctx);

    if (mont->rrVal.top < wordCount) {
        if (mont->rrVal.maxWords < wordCount) {
            sslBnExpandWords(&mont->rrVal, wordCount);
        }
        for (idx = mont->rrVal.top; idx < wordCount; idx++) {
            mont->rrVal.words[idx] = 0;
        }
    }
    if (mont->invVal.top < wordCount) {
        if (mont->invVal.maxWords < wordCount) {
            sslBnExpandWords(&mont->invVal, wordCount);
        }
        for (idx = mont->invVal.top; idx < wordCount; idx++) {
            mont->invVal.words[idx] = 0;
        }
    }
    return 1;
}

/* 0x006E04 */
int LIBSSL_NOINLINE sslBnMul(SslBignum *out, const SslBignum *lhs, const SslBignum *rhs, SslBnCtx *ctx)
{
    int lhsLen = lhs->top;
    int rhsLen = rhs->top;
    int resNeg;
    int totalLen;
    SslBignum *target = out;

    if (lhsLen == 0 || rhsLen == 0) {
        sslBnSetWord(out, 0);
        return 1;
    }

    resNeg = lhs->neg ^ rhs->neg;
    totalLen = lhsLen + rhsLen;
    if (out == lhs || out == rhs) {
        target = &ctx->pool[ctx->tos + 1];
    }
    if (!bnEnsureWords(target, totalLen)) {
        return 0;
    }

    target->top = totalLen;
    sslBnMulNormal(target->words, lhs->words, lhsLen, rhs->words, rhsLen);
    out->neg = resNeg;
    bnFixTop(target);

    if (out != target) {
        sslBnCopy(out, target);
    }
    return 1;
}

/* 0x006F6C */
u32 LIBSSL_NOINLINE sslBnMulNormal(u32 *dst, const u32 *lhs, int lhsLen, const u32 *rhs, int rhsLen)
{
    u32 *hiDst;
    u32 lastCarry;

    if (lhsLen < rhsLen) {
        int tmpLen = lhsLen;
        const u32 *tmpPtr = lhs;
        lhsLen = rhsLen;
        lhs = rhs;
        rhsLen = tmpLen;
        rhs = tmpPtr;
    }

    hiDst = &dst[lhsLen];
    lastCarry = sslBnMulWords(dst, lhs, lhsLen, rhs[0]);
    *hiDst = lastCarry;

    for (;;) {
        rhsLen--;
        if (rhsLen <= 0) {
            break;
        }
        dst++;
        rhs++;
        hiDst++;
        lastCarry = sslBnMulAddWords(dst, lhs, lhsLen, *rhs);
        *hiDst = lastCarry;
    }
    return lastCarry;
}

/* 0x007094 */
int LIBSSL_NOINLINE sslBnRand(SslBignum *out, int bits, int topMode, int bottomOdd)
{
    int byteCount = (bits + 7) / 8;
    int topBit = (bits - 1) % 8;
    int topMask = 0xFF << topBit;
    u8 *randBuf;
    u8 timeSeed[16];
    int retOk = 0;

    randBuf = (u8 *)sceSsl_lib_0861D934((u32)byteCount);
    if (randBuf == NULL) {
        return 0;
    }

    sceSsl_lib_A7BED83B((SceSslTimeSpec *)timeSeed);
    sceSsl_lib_DEA6EDB2(timeSeed, 12);
    sceSsl_lib_21F2A35C(randBuf, byteCount);

    if (topMode == 0) {
        randBuf[0] = (u8)((randBuf[0] | (1 << topBit)) & ~(topMask << 1));
    } else if (topBit != 0) {
        randBuf[0] = (u8)((randBuf[0] | (3 << (topBit - 1))) & ~(topMask << 1));
    } else {
        randBuf[0] = 1U;
        randBuf[1] |= 0x80U;
    }

    if (bottomOdd != 0) {
        randBuf[byteCount - 1] |= 1U;
    }

    if (sslBnBinToBn(randBuf, byteCount, out) != NULL) {
        retOk = 1;
    }
    sslMemset(randBuf, 0, (u32)byteCount);
    sceSsl_lib_5E5C873A(randBuf);
    return retOk;
}

/* 0x007214 */
int LIBSSL_NOINLINE sslBnRshiftOne(SslBignum *out, const SslBignum *src)
{
    int idx;
    u32 carry = 0;

    if (bnIsZero(src)) {
        sslBnSetWord(out, 0);
        return 1;
    }

    if (out != src) {
        if (!bnEnsureWords(out, src->top)) {
            return 0;
        }
        out->top = src->top;
        out->neg = src->neg;
    }

    for (idx = src->top - 1; idx >= 0; idx--) {
        u32 curWord = src->words[idx];
        out->words[idx] = (curWord >> 1) | carry;
        carry = curWord << 31;
    }
    bnFixTop(out);
    return 1;
}

/* 0x007334 */
int LIBSSL_NOINLINE sslBnRshift(SslBignum *out, const SslBignum *src, int shiftBits)
{
    int wordShift;
    int bitShift;
    int remWords;
    int idx;

    if (shiftBits == 1) {
        return sslBnRshiftOne(out, src);
    }

    wordShift = shiftBits / 32;
    bitShift = shiftBits % 32;
    if (src->top < wordShift) {
        sslBnSetWord(out, 0);
        return 1;
    }

    remWords = src->top - wordShift;
    if (out != src) {
        if (!bnEnsureWords(out, remWords + 2)) {
            return 0;
        }
        out->neg = src->neg;
    }

    out->top = remWords;
    if (bitShift != 0) {
        int lShift = 32 - bitShift;
        u32 curWord = src->words[wordShift];
        for (idx = 0; idx < remWords - 1; idx++) {
            u32 nextWord = src->words[wordShift + idx + 1];
            out->words[idx] = (curWord >> bitShift) | (nextWord << lShift);
            curWord = nextWord;
        }
        out->words[remWords - 1] = curWord >> bitShift;
        out->words[remWords] = 0;
    } else {
        for (idx = 0; idx <= remWords; idx++) {
            out->words[idx] = src->words[wordShift + idx];
        }
    }

    bnFixTop(out);
    return 1;
}

/* 0x0074EC */
int LIBSSL_NOINLINE sslBnSqr(SslBignum *out, const SslBignum *src, SslBnCtx *ctx)
{
    SslBignum *target = (out == src) ? &ctx->pool[ctx->tos + 1] : out;
    SslBignum *tmpBn = &ctx->pool[ctx->tos];
    int wordCount = src->top;
    int maxWords;
    u32 scratchFour[8];
    u32 scratchEight[16];
    u32 *scratchPtr;

    if (wordCount <= 0) {
        out->top = 0;
        return 1;
    }

    maxWords = wordCount * 2;
    if (!bnEnsureWords(target, maxWords)) {
        return 0;
    }
    target->top = maxWords;
    target->neg = 0;

    if (wordCount == 4) {
        scratchPtr = scratchFour;
    } else if (wordCount == 8) {
        scratchPtr = scratchEight;
    } else {
        if (!bnEnsureWords(tmpBn, maxWords)) {
            return 0;
        }
        scratchPtr = tmpBn->words;
    }

    sslBnSqrNormal(target->words, src->words, wordCount, scratchPtr);
    if (target->words[maxWords - 1] == 0) {
        target->top--;
    }
    if (target != out) {
        sslBnCopy(out, target);
    }
    return 1;
}

/* 0x007670 */
u32 LIBSSL_NOINLINE sslBnSqrNormal(u32 *dst, const u32 *src, int wordCount, u32 *scratch)
{
    int maxIdx = wordCount - 1;
    int dblCount = wordCount * 2;
    const u32 *curSrc = src;
    u32 *curDst = &dst[1];
    int remOuter = wordCount - 2;

    dst[0] = 0;
    dst[dblCount - 1] = 0;

    if (maxIdx > 0) {
        curDst[maxIdx] = sslBnMulWords(curDst, &curSrc[1], maxIdx, curSrc[0]);
        curSrc++;
        curDst += 2;
    }

    while (remOuter > 0) {
        u32 multWord = *curSrc++;
        maxIdx--;
        remOuter--;
        curDst[maxIdx] = sslBnMulAddWords(curDst, curSrc, maxIdx, multWord);
        curDst += 2;
    }

    sslBnAddWords(dst, dst, dst, dblCount);
    sslBnSqrWords(scratch, src, wordCount);
    return sslBnAddWords(dst, dst, scratch, dblCount);
}

/* 0x0077A0 */
u32 LIBSSL_NOINLINE sslBnModWord(const SslBignum *bn, u32 word)
{
    u32 rem = 0;
    int idx;

    for (idx = bn->top - 1; idx >= 0; idx--) {
        u32 curWord = bn->words[idx];
        rem = ((rem << 16) | (curWord >> 16)) % word;
        rem = ((rem << 16) | (curWord & 0xFFFFU)) % word;
    }
    return rem;
}

/* 0x007808 */
u32 LIBSSL_NOINLINE sslBnDivWord(SslBignum *bn, u32 word)
{
    u32 rem = 0;
    int idx;

    if (bn->top == 0) {
        return 0;
    }
    for (idx = bn->top - 1; idx >= 0; idx--) {
        u32 curWord = bn->words[idx];
        u32 quot = sslBnDivWords(rem, curWord, word);
        bn->words[idx] = quot;
        rem = curWord - quot * word;
    }
    if (bn->top > 0 && bn->words[bn->top - 1] == 0) {
        bn->top--;
    }
    return rem;
}

/* 0x0078CC */
int LIBSSL_NOINLINE sslBnAddWord(SslBignum *bn, u32 word)
{
    u32 carry = word;
    int idx = 0;

    if (word == 0) {
        return 1;
    }
    if (bn->neg != 0) {
        if (bn->top < 2) {
            u32 lowWord = bn->words[0];
            if (word >= lowWord) {
                if (word > lowWord) {
                    bn->words[0] = word - lowWord;
                } else {
                    bn->top = 0;
                }
                bn->neg = 0;
            } else {
                bn->words[0] = lowWord - word;
            }
            return 1;
        }
        bn->neg = 0;
        if (sslBnSubWord(bn, word) == 0) {
            bn->neg = 1;
            return 0;
        }
        bn->neg = 1;
        return 1;
    }

    if (!bnEnsureWords(bn, bn->top + 1)) {
        return 0;
    }
    bn->words[bn->top] = 0;
    while (carry != 0) {
        u32 oldWord = bn->words[idx];
        bn->words[idx] = oldWord + carry;
        carry = (bn->words[idx] < oldWord) ? 1U : 0U;
        if (carry != 0) {
            idx++;
        }
    }
    if (idx >= bn->top) {
        bn->top++;
    }
    return 1;
}

/* 0x0079F4 */
int LIBSSL_NOINLINE sslBnSubWord(SslBignum *bn, u32 word)
{
    int idx = 0;

    if (word == 0) {
        return 1;
    }
    if (bn->neg != 0) {
        int ret;
        bn->neg = 0;
        ret = sslBnAddWord(bn, word);
        bn->neg = 1;
        return ret;
    }

    if (bn->top >= 2) {
        while (bn->words[idx] < word) {
            bn->words[idx] -= word;
            word = 1U;
            idx++;
        }
        bn->words[idx] -= word;
        if (bn->words[idx] == 0 && idx == bn->top - 1) {
            bn->top = idx;
        }
        return 1;
    }

    if (bn->top != 0) {
        u32 lowWord = bn->words[0];
        if (lowWord == word) {
            bn->top = 0;
        } else if (word < lowWord) {
            bn->words[0] = lowWord - word;
        } else {
            bn->words[0] = word - lowWord;
            bn->neg = 1;
        }
        return 1;
    }

    if (!bnEnsureWords(bn, 1)) {
        return 0;
    }
    bn->words[0] = word;
    bn->top = 1;
    bn->neg = 1;
    return 1;
}

/* 0x007B30 */
int LIBSSL_NOINLINE sslBnMulWord(SslBignum *bn, u32 word)
{
    u32 carry;

    if (bn->top == 0) {
        return 1;
    }
    carry = sslBnMulWords(bn->words, bn->words, bn->top, word);
    if (carry == 0) {
        return 1;
    }
    if (!bnEnsureWords(bn, bn->top + 1)) {
        return 0;
    }
    bn->words[bn->top++] = carry;
    return 1;
}
