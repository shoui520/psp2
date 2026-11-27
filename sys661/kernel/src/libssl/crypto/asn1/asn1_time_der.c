/*
 * SceSsl / sceSsl_Module (ePSP 6.61) - RSA-COMMON date/time & ASN.1 DER codecs.
 *
 * Implements functions [196..230] (0x00e5c0..0x011317):
 *   - Time / calendar formatting, parsing, arithmetic, comparison
 *     (sceSsl_lib_2FE7BB42, A7BED83B, 665D02F6, 9EF66756, E7BFC0EF, 047AB6BB)
 *   - ASN.1 BIT STRING, OCTET STRING, generic string, ASN1_TYPE codecs
 *   - d2i_PrivateKey (sceSsl_lib_A920B39E), d2i_RSAPrivateKey
 *   - X509_ALGOR & X509_SIG codecs
 */
#include "../../libssl_internal.h"

typedef struct SslDateParseToken {
    int value;
    int maxDigits;
    char delim;
} SslDateParseToken;

static SslTimeAddCb g_sslTimeAddCb = NULL;
static SslTimeFormatCb g_sslTimeFormatCb = NULL;
static SslTimeParseCb g_sslTimeParseCb = NULL;
static SslTimeCmpCb g_sslTimeCmpCb = NULL;

/* Cumulative days before each month (0x02e20c): [0]=non-leap, [1]=leap */
static const int g_monthDayOffsets[2][13] = {
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

/* ASN.1 tag-to-bitmask table (0x02f0cc) */
static const u32 g_asn1TagToBit[32] = {
    0x00000000U, 0x00000000U, 0x00000000U, 0x00000400U,
    0x00000200U, 0x00000000U, 0x00000000U, 0x00001000U,
    0x00001000U, 0x00001000U, 0x00001000U, 0x00001000U,
    0x00001000U, 0x00001000U, 0x00001000U, 0x00001000U,
    0x00000000U, 0x00000000U, 0x00000001U, 0x00000002U,
    0x00000004U, 0x00000008U, 0x00000010U, 0x00000000U,
    0x00000000U, 0x00000020U, 0x00000040U, 0x00000080U,
    0x00000100U, 0x00001000U, 0x00000800U, 0x00001000U
};

static inline __attribute__((always_inline)) int sslIsDigitChar(int ch)
{
    return (ch >= '0' && ch <= '9');
}

static inline __attribute__((always_inline)) int sslIsLeapYearBn(const SslBignum *yearBn)
{
    if (sslBnModWord(yearBn, 400U) == 0) {
        return 1;
    }
    if (sslBnModWord(yearBn, 4U) == 0 && sslBnModWord(yearBn, 100U) != 0) {
        return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Time / Calendar routines [196..206]
 * ------------------------------------------------------------------------- */

/* [196] 0x00e5c0 */
LIBSSL_NOINLINE SslTimeCb sceSsl_lib_2FE7BB42(void)
{
    return g_sslTimeCallback;
}

/* [197] 0x00e5cc */
LIBSSL_NOINLINE SceSslTimeSpec *sceSsl_lib_A7BED83B(SceSslTimeSpec *outTime)
{
    if (g_sslTimeCallback != NULL) {
        return g_sslTimeCallback(outTime);
    }
    return sslDefaultTimeCallback(outTime);
}

/* [198] 0x00e608 */
LIBSSL_NOINLINE SceSslTimeSpec *sslTimeAddDefault(SceSslTimeSpec *outTime,
                                                  const SceSslTimeSpec *inTime,
                                                  int secDelta, int nsecDelta)
{
    int carry = 0;
    int secHi;
    u32 secLo;
    u32 nsec;

    if (outTime == NULL || inTime == NULL) {
        return NULL;
    }

    secHi = (int)inTime->secHigh;
    if (secHi < 0) {
        nsecDelta = -nsecDelta;
    }

    nsec = inTime->nsec;
    if (nsecDelta < 0 && nsec < (u32)(-nsecDelta)) {
        secDelta--;
        outTime->nsec = (u32)((int)nsec + nsecDelta + 1000000000);
    } else {
        u32 nextNsec = (u32)((int)nsec + nsecDelta);
        outTime->nsec = nextNsec;
        if (nextNsec > 999999999U) {
            outTime->nsec = nextNsec - 1000000000U;
            secDelta++;
        }
    }

    secLo = inTime->secLow;
    outTime->secLow = secLo + (u32)secDelta;
    if (secDelta > 0 && outTime->secLow < secLo) {
        carry = 1;
    } else if (secDelta < 0 && secLo < outTime->secLow) {
        carry = -1;
    }

    outTime->secHigh = (u32)(secHi + carry);
    return outTime;
}

/* [199] 0x00e6dc */
LIBSSL_NOINLINE SceSslTimeSpec *sceSsl_lib_665D02F6(SceSslTimeSpec *outTime,
                                                    const SceSslTimeSpec *inTime,
                                                    int secDelta, int nsecDelta)
{
    if (g_sslTimeAddCb != NULL) {
        return g_sslTimeAddCb(outTime, inTime, secDelta, nsecDelta);
    }
    return sslTimeAddDefault(outTime, inTime, secDelta, nsecDelta);
}

/* [200] 0x00e718 */
LIBSSL_NOINLINE int sslTimeFormatDefault(const SceSslTimeSpec *timeVal, int fmtType,
                                         char *outStr, int outLen)
{
    const SceSslTimeSpec *src = timeVal;
    SceSslTimeSpec negCopy;
    int isNeg = 0;

    if (outStr == NULL) {
        return 2;
    }

    if (fmtType == 1 || fmtType == 2) {
        SslBignum *daysBn;
        SslBignum *yearBn;
        SslBignum *baseDaysBn;
        SslBignum *leapBn;
        u32 secPart;
        u32 minPart;
        u32 hourPart;
        int dayOfYear;
        int isLeap;
        int month;
        int dayOfMonth = -1;
        int yearVal;

        daysBn = sslBnNew();
        if (daysBn == NULL) {
            return 7;
        }

        if ((int)src->secHigh < 0) {
            negCopy.secHigh = ~src->secHigh;
            negCopy.secLow = ~src->secLow;
            negCopy.nsec = src->nsec;
            sceSsl_lib_665D02F6(&negCopy, &negCopy, 1, 0);
            src = &negCopy;
            isNeg = 1;
        }

        sslBnSetWord(daysBn, src->secHigh);
        sslBnLshift(daysBn, daysBn, 32);
        sslBnAddWord(daysBn, src->secLow);
        if (isNeg != 0) {
            daysBn->neg = 1;
        }

        secPart = sslBnDivWord(daysBn, 60U);
        minPart = sslBnDivWord(daysBn, 60U);
        hourPart = sslBnDivWord(daysBn, 24U);

        yearBn = sslBnDup(daysBn);
        if (yearBn == NULL) {
            return 7;
        }
        sslBnMulWord(yearBn, 400U);
        sslBnDivWord(yearBn, 146097U);

        baseDaysBn = sslBnDup(yearBn);
        if (baseDaysBn == NULL) {
            return 7;
        }
        sslBnAddWord(yearBn, 1970U);

        leapBn = sslBnDup(yearBn);
        if (leapBn == NULL) {
            return 7;
        }

        if (daysBn->neg == 0) {
            sslBnSubWord(leapBn, 1U);
        }
        sslBnMulWord(baseDaysBn, 365U);
        sslBnDivWord(leapBn, 4U);
        sslBnAdd(baseDaysBn, baseDaysBn, leapBn);
        sslBnDivWord(leapBn, 25U);
        sslBnSub(baseDaysBn, baseDaysBn, leapBn);
        sslBnDivWord(leapBn, 4U);
        sslBnAdd(baseDaysBn, baseDaysBn, leapBn);
        sslBnSubWord(baseDaysBn, 477U);

        if (daysBn->neg != 0 && yearBn->neg == 0 && !sslIsLeapYearBn(yearBn)) {
            sslBnAddWord(baseDaysBn, 1U);
        }
        sslBnSub(daysBn, daysBn, baseDaysBn);

        if (daysBn->neg != 0) {
            u32 yearLen;
            sslBnSubWord(yearBn, 1U);
            yearLen = sslIsLeapYearBn(yearBn) ? 366U : 365U;
            sslBnAddWord(daysBn, yearLen);
        }

        dayOfYear = (int)sslBnGetWord(daysBn);
        isLeap = 0;
        if (!sslIsLeapYearBn(yearBn)) {
            if (dayOfYear >= 365) {
                sslBnAddWord(yearBn, 1U);
                dayOfYear -= 365;
                if (sslIsLeapYearBn(yearBn)) {
                    isLeap = 1;
                }
            }
        } else {
            if (dayOfYear >= 366) {
                sslBnAddWord(yearBn, 1U);
                dayOfYear -= 366;
            } else {
                isLeap = 1;
            }
        }

        for (month = 1; month < 13; month++) {
            if (dayOfYear < g_monthDayOffsets[isLeap][month]) {
                dayOfMonth = dayOfYear - g_monthDayOffsets[isLeap][month - 1];
                break;
            }
        }
        dayOfMonth++;

        if (fmtType == 1) {
            yearVal = (int)sslBnGetWord(yearBn);
            if (yearBn->neg != 0 || yearVal >= 2050 || yearVal < 1950 || outLen < 14) {
                return 1;
            }
            sslSprintf(outStr, "%02ld%02ld%02ld%02ld%02ld%02ldZ",
                       yearVal % 100, month, dayOfMonth, hourPart, minPart, secPart);
        } else {
            yearVal = (int)sslBnGetWord(yearBn);
            if (yearBn->neg != 0 || yearVal >= 10000 || outLen < 17) {
                return 1;
            }
            sslSprintf(outStr, "%04ld%02ld%02ld%02ld%02ld%02ld.%ldZ",
                       yearVal, month, dayOfMonth, hourPart, minPart, secPart,
                       src->nsec / 100000000U);
        }

        sslBnFree(daysBn);
        sslBnFree(yearBn);
        sslBnFree(baseDaysBn);
        sslBnFree(leapBn);
        return 0;
    }

    if (fmtType != 3) {
        return 5;
    }

    {
        int signMul = ((int)src->secHigh < 0) ? -1 : 1;
        int fillByte = ((int)src->secHigh < 0) ? -1 : 0;
        int sigBytes = 8;
        int shiftIdx;
        u8 *dst;
        int written;
        u8 rawLo[4];
        u8 rawHi[4];

        for (shiftIdx = 3; shiftIdx >= 0; shiftIdx--) {
            if ((((u32)(signMul * (int)src->secHigh) >> (8 * shiftIdx)) & 0xffU) != 0) {
                break;
            }
            sigBytes--;
        }
        if (shiftIdx < 0) {
            for (shiftIdx = 3; shiftIdx >= 0; shiftIdx--) {
                if ((((u32)(signMul * (int)src->secLow) >> (8 * shiftIdx)) & 0xffU) != 0) {
                    break;
                }
                sigBytes--;
            }
        }

        if (outLen < sigBytes) {
            return 4;
        }

        sslMemset(outStr, fillByte, (u32)(outLen - sigBytes));
        rawLo[0] = (u8)(src->secLow);
        rawLo[1] = (u8)(src->secLow >> 8);
        rawLo[2] = (u8)(src->secLow >> 16);
        rawLo[3] = (u8)(src->secLow >> 24);
        rawHi[0] = (u8)(src->secHigh);
        rawHi[1] = (u8)(src->secHigh >> 8);
        rawHi[2] = (u8)(src->secHigh >> 16);
        rawHi[3] = (u8)(src->secHigh >> 24);

        dst = (u8 *)outStr + outLen - 1;
        written = 0;
        while (written < 4) {
            *dst-- = rawLo[written];
            if (written == sigBytes) {
                return 0;
            }
            written++;
        }
        if (written < sigBytes) {
            int hiIdx = 0;
            while (hiIdx < 4 && (hiIdx + 4) < sigBytes) {
                *dst-- = rawHi[hiIdx++];
            }
        }
    }
    return 0;
}

/* [201] 0x00eda8 */
LIBSSL_NOINLINE int sceSsl_lib_9EF66756(const SceSslTimeSpec *timeVal, int fmtType,
                                        char *outStr, int outLen)
{
    if (g_sslTimeFormatCb != NULL) {
        return g_sslTimeFormatCb(timeVal, fmtType, outStr, outLen);
    }
    return sslTimeFormatDefault(timeVal, fmtType, outStr, outLen);
}

/* [202] 0x00ede4 */
LIBSSL_NOINLINE int sslTimeScanTokens(const char *inStr, int inLen,
                                      SslDateParseToken *tokens, int maxTokens)
{
    int srcPos = 0;
    int tokIdx;

    for (tokIdx = 0; tokIdx < maxTokens; tokIdx++) {
        int ch;
        int digitCount;

        if (srcPos >= inLen) {
            break;
        }
        ch = (signed char)inStr[srcPos++];
        if (ch == 0) {
            break;
        }
        if (sslIsDigitChar(ch)) {
            tokens[tokIdx].value = ch - '0';
            digitCount = 1;
        } else {
            tokens[tokIdx].delim = (char)ch;
            tokens[tokIdx].value = 0;
            digitCount = 0;
        }

        while (digitCount < tokens[tokIdx].maxDigits) {
            if (srcPos >= inLen) {
                return tokIdx;
            }
            ch = (signed char)inStr[srcPos++];
            if (ch == 0 || !sslIsDigitChar(ch)) {
                return tokIdx;
            }
            tokens[tokIdx].value = 10 * tokens[tokIdx].value + (ch - '0');
            digitCount++;
        }
    }
    return tokIdx;
}

/* [203] 0x00eec8 */
LIBSSL_NOINLINE int sslTimeParseDefault(const u8 *inStr, u32 inLen, int fmtType,
                                        SceSslTimeSpec *outTime)
{
    int tzSign = 0;
    int tzHours = 0;
    int tzMins = 0;
    int tenths = 0;

    if (inStr == NULL) {
        return 2;
    }

    if (fmtType == 1 || fmtType == 2) {
        static const SslDateParseToken kInitTokens[10] = {
            { 0, 2, 0 }, { 0, 2, 0 }, { 0, 2, 0 }, { 0, 2, 0 }, { 0, 2, 0 },
            { 0, 2, 0 }, { 0, 2, 0 }, { 0, 2, 0 }, { 0, 2, 0 }, { 0, 0, 0 }
        };
        SslDateParseToken tokens[10];
        SslBignum *yearBn;
        SslBignum *leapBn;
        SslBignum tmpBn;
        int parsedCount;
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        int trailingDelim;
        int isLeap = 0;
        int nonLeapPos = 0;
        u8 binBuf[8];
        int binLen;
        u32 hiWord;
        u32 loWord;
        int tzOffsetSec;

        __builtin_memcpy(tokens, kInitTokens, sizeof(tokens));
        yearBn = sslBnNew();
        if (yearBn == NULL) {
            return 7;
        }

        if (fmtType == 2) {
            tokens[0].maxDigits = 4;
            tokens[6].maxDigits = 1;
        }

        parsedCount = sslTimeScanTokens((const char *)inStr, (int)inLen, tokens, 10);
        year = tokens[0].value;
        month = tokens[1].value;
        day = tokens[2].value;
        hour = tokens[3].value;
        minute = tokens[4].value;
        second = tokens[5].value;
        trailingDelim = (signed char)tokens[6].delim;

        if (fmtType == 1) {
            if (parsedCount < 5) {
                return 6;
            }
            if (year < 50) {
                year += 100;
            }
            year += 1900;
            if (parsedCount == 5) {
                trailingDelim = (signed char)tokens[5].delim;
                second = 0;
            } else if (parsedCount == 7) {
                tzSign = (signed char)tokens[5].delim;
                tzHours = tokens[5].value;
                tzMins = tokens[6].value;
            } else if (parsedCount == 8) {
                tzHours = tokens[6].value;
                tzMins = tokens[7].value;
                tzSign = trailingDelim;
            }
        } else {
            if (parsedCount < 7) {
                return 6;
            }
            trailingDelim = (signed char)tokens[7].delim;
            tenths = tokens[6].value;
            if (parsedCount == 9) {
                tzHours = tokens[7].value;
                tzMins = tokens[8].value;
                tzSign = trailingDelim;
            }
        }

        sslBnSetWord(yearBn, (u32)year);
        if (!sslIsLeapYearBn(yearBn)) {
            if (yearBn->neg == 0) {
                nonLeapPos = 1;
            }
        } else {
            isLeap = 1;
        }

        if (month >= 13 ||
            g_monthDayOffsets[isLeap][month] < g_monthDayOffsets[isLeap][month - 1] + day ||
            hour >= 24 || minute >= 60 || second >= 61 || tzHours >= 13 || tzMins >= 60) {
            return 6;
        }

        if (tzSign != '+' && tzSign != '-' && trailingDelim != 'Z' && trailingDelim != 'z') {
            return 6;
        }

        if (second == 60) {
            second = 59;
        }

        leapBn = sslBnDup(yearBn);
        if (leapBn == NULL) {
            return 7;
        }

        sslBnSubWord(yearBn, 1970U);
        if (yearBn->neg == 0) {
            sslBnSubWord(leapBn, 1U);
        }
        sslBnMulWord(yearBn, 365U);
        sslBnDivWord(leapBn, 4U);
        sslBnAdd(yearBn, yearBn, leapBn);
        sslBnDivWord(leapBn, 25U);
        sslBnSub(yearBn, yearBn, leapBn);
        sslBnDivWord(leapBn, 4U);
        sslBnAdd(yearBn, yearBn, leapBn);
        sslBnSubWord(yearBn, 477U);

        if (nonLeapPos != 0 && yearBn->neg != 0) {
            sslBnAddWord(yearBn, 1U);
        }

        sslBnAddWord(yearBn, (u32)g_monthDayOffsets[isLeap][month - 1]);
        sslBnAddWord(yearBn, (u32)(day - 1));
        sslBnMulWord(yearBn, 24U);
        sslBnAddWord(yearBn, (u32)hour);
        sslBnMulWord(yearBn, 60U);
        sslBnAddWord(yearBn, (u32)minute);
        sslBnMulWord(yearBn, 60U);
        sslBnAddWord(yearBn, (u32)second);

        sslBnInit(&tmpBn);
        sslBnCopy(&tmpBn, yearBn);
        sslBnMaskBits(&tmpBn, 64);
        __builtin_memset(binBuf, 0, sizeof(binBuf));
        binLen = (sslBnNumBits(&tmpBn) + 7) / 8;
        sslBnBnToBin(&tmpBn, &binBuf[8 - binLen]);
        sslBnFree(&tmpBn);

        hiWord = ((u32)binBuf[0] << 24) | ((u32)binBuf[1] << 16) |
                 ((u32)binBuf[2] << 8) | (u32)binBuf[3];
        loWord = ((u32)binBuf[4] << 24) | ((u32)binBuf[5] << 16) |
                 ((u32)binBuf[6] << 8) | (u32)binBuf[7];

        outTime->secHigh = hiWord;
        outTime->secLow = loWord;
        if (yearBn->neg != 0) {
            outTime->secHigh = ~hiWord;
            outTime->secLow = ~loWord;
            sceSsl_lib_665D02F6(outTime, outTime, 1, 0);
        }
        outTime->nsec = (u32)(100000000 * tenths);

        sslBnFree(yearBn);
        sslBnFree(leapBn);

        tzOffsetSec = 60 * (60 * tzHours + tzMins);
        if (tzSign == '+') {
            tzOffsetSec = -tzOffsetSec;
        }
        sceSsl_lib_665D02F6(outTime, outTime, tzOffsetSec, 0);
        return 0;
    }

    if (fmtType != 3) {
        return 5;
    }
    if (inLen >= 9U) {
        return 8;
    }

    {
        u32 idx = 0;
        u32 loWord = 0;
        u32 hiWord = 0;
        const u8 *src = inStr + inLen;

        outTime->secHigh = 0;
        outTime->secLow = 0;
        outTime->nsec = 0;

        while (src > inStr && idx < 4U) {
            src--;
            loWord |= ((u32)(*src)) << (8U * idx);
            idx++;
        }
        idx = 0;
        while (src > inStr && idx < 4U) {
            src--;
            hiWord |= ((u32)(*src)) << (8U * idx);
            idx++;
        }
        outTime->secLow = loWord;
        outTime->secHigh = hiWord;
    }
    return 0;
}

/* [204] 0x00f53c */
LIBSSL_NOINLINE int sceSsl_lib_E7BFC0EF(const u8 *inStr, u32 inLen, int fmtType,
                                        SceSslTimeSpec *outTime)
{
    if (g_sslTimeParseCb != NULL) {
        return g_sslTimeParseCb(inStr, inLen, fmtType, outTime);
    }
    return sslTimeParseDefault(inStr, inLen, fmtType, outTime);
}

/* [205] 0x00f578 */
LIBSSL_NOINLINE int sslTimeCompareDefault(const SceSslTimeSpec *lhs, const SceSslTimeSpec *rhs)
{
    SceSslTimeSpec zeroTime;

    sslMemset(&zeroTime, 0, sizeof(zeroTime));
    if (lhs == NULL) {
        lhs = &zeroTime;
    }
    if (rhs == NULL) {
        rhs = &zeroTime;
    }

    if ((int)lhs->secHigh > (int)rhs->secHigh) {
        return 1;
    }
    if ((int)lhs->secHigh < (int)rhs->secHigh) {
        return -1;
    }
    if (lhs->secLow > rhs->secLow) {
        return 1;
    }
    if (lhs->secLow < rhs->secLow) {
        return -1;
    }
    if (lhs->nsec > rhs->nsec) {
        return 1;
    }
    if (lhs->nsec < rhs->nsec) {
        return -1;
    }
    return 0;
}

/* [206] 0x00f620 */
LIBSSL_NOINLINE int sceSsl_lib_047AB6BB(const SceSslTimeSpec *lhs, const SceSslTimeSpec *rhs)
{
    if (g_sslTimeCmpCb != NULL) {
        return g_sslTimeCmpCb(lhs, rhs);
    }
    return sslTimeCompareDefault(lhs, rhs);
}

/* -------------------------------------------------------------------------
 * ASN.1 BIT STRING, OCTET STRING, generic strings, ASN1_TYPE [207..220]
 * ------------------------------------------------------------------------- */

/* [207] 0x00f65c */
LIBSSL_NOINLINE int sslI2dAsn1BitString(const SslAsn1String *str, u8 **outPtr)
{
    int len;
    u8 unusedBits = 0;
    int totalLen;

    if (str == NULL) {
        return 0;
    }

    len = str->length;
    if (len > 0 && (str->flags & 8) != 0) {
        unusedBits = (u8)(str->flags & 7);
    }

    totalLen = sslAsn1ObjectSize(0, len + 1, 3);
    if (outPtr != NULL) {
        u8 *dst = *outPtr;
        sslAsn1PutObject(&dst, 0, len + 1, 3, 0);
        *dst++ = unusedBits;
        sslMemcpy(dst, str->data, (u32)len);
        dst += len;
        if (len > 0) {
            dst[-1] &= (u8)(0xffU << unusedBits);
        }
        *outPtr = dst;
    }
    return totalLen;
}

/* [208] 0x00f764 */
LIBSSL_NOINLINE SslAsn1String *sslD2iAsn1BitString(SslAsn1String **outStr, const u8 **inPtr,
                                                   int maxLen)
{
    SslAsn1String *ret;
    const u8 *cur;
    int objLen = 0;
    int tag = 0;
    int xclass = 0;
    int errReason = 0;
    u8 unusedBits;
    u8 *buf = NULL;

    if (outStr != NULL && *outStr != NULL) {
        ret = *outStr;
    } else {
        ret = sslAsn1StringTypeNew(3);
        if (ret == NULL) {
            return NULL;
        }
    }

    cur = *inPtr;
    if ((sslAsn1GetObject(&cur, &objLen, &tag, &xclass, maxLen) & 0x80) != 0 ||
        (maxLen - (int)(cur - *inPtr)) < objLen) {
        errReason = 101;
    } else if (tag != 3) {
        errReason = 116;
    } else if (objLen <= 0) {
        errReason = 134;
    } else {
        unusedBits = *cur++;
        objLen--;
        ret->flags = (ret->flags & ~0x0f) | (unusedBits & 7) | 8;
        if (objLen > 0) {
            buf = (u8 *)sceSsl_lib_0861D934((u32)objLen);
            if (buf == NULL) {
                errReason = 33;
            } else {
                sslMemcpy(buf, cur, (u32)objLen);
                buf[objLen - 1] &= (u8)(0xffU << unusedBits);
                cur += objLen;
            }
        }
    }

    if (errReason != 0) {
        sceSsl_lib_9CDCA0D7(13, 123, errReason, NULL, 0);
        if (ret != NULL && (outStr == NULL || *outStr != ret)) {
            sslAsn1StringFree(ret);
        }
        return NULL;
    }

    if (ret->data != NULL) {
        sceSsl_lib_5E5C873A(ret->data);
    }
    ret->length = objLen;
    ret->data = buf;
    ret->type = 3;
    if (outStr != NULL) {
        *outStr = ret;
    }
    *inPtr = cur;
    return ret;
}

/* [209] 0x00f940 */
LIBSSL_NOINLINE SslAsn1String *sslD2iAsn1TypeBytes(SslAsn1String **outStr, const u8 **inPtr,
                                                   int maxLen, int typeMask)
{
    SslAsn1String *ret = NULL;
    const u8 *cur = *inPtr;
    int objLen = 0;
    int tag = 0;
    int xclass = 0;
    int errReason = 0;
    u8 *buf = NULL;

    if ((sslAsn1GetObject(&cur, &objLen, &tag, &xclass, maxLen) & 0x80) != 0 ||
        (maxLen - (int)(cur - *inPtr)) < objLen) {
        errReason = 101;
    } else if (tag >= 32) {
        errReason = 135;
    } else if ((g_asn1TagToBit[tag] & (u32)typeMask) == 0) {
        errReason = 150;
    } else {
        if (tag == 3) {
            return sslD2iAsn1BitString(outStr, inPtr, maxLen);
        }
        if (outStr != NULL && *outStr != NULL) {
            ret = *outStr;
        } else {
            ret = sslAsn1StringNew();
            if (ret == NULL) {
                return NULL;
            }
        }

        if (objLen != 0) {
            buf = (u8 *)sceSsl_lib_0861D934((u32)(objLen + 1));
            if (buf == NULL) {
                errReason = 33;
            } else {
                sslMemcpy(buf, cur, (u32)objLen);
                buf[objLen] = 0;
                cur += objLen;
            }
        }
    }

    if (errReason != 0) {
        sceSsl_lib_9CDCA0D7(13, 134, errReason, NULL, 0);
        if (ret != NULL && (outStr == NULL || *outStr != ret)) {
            sslAsn1StringFree(ret);
        }
        return NULL;
    }

    if (ret->data != NULL) {
        sceSsl_lib_5E5C873A(ret->data);
    }
    ret->data = buf;
    ret->length = objLen;
    ret->type = tag;
    if (outStr != NULL) {
        *outStr = ret;
    }
    *inPtr = cur;
    return ret;
}

/* [210] 0x00fb20 */
LIBSSL_NOINLINE int sslI2dAsn1Bytes(const SslAsn1String *str, u8 **outPtr, int tag, int xclass)
{
    int totalLen;
    int constructed;

    if (str == NULL) {
        return 0;
    }
    if (tag == 3) {
        return sslI2dAsn1BitString(str, outPtr);
    }

    {
        int len = str->length;
        totalLen = sslAsn1ObjectSize(0, len, tag);
        if (outPtr != NULL) {
            u8 *dst = *outPtr;
            constructed = (tag == 16 || tag == 17) ? 1 : 0;
            sslAsn1PutObject(&dst, constructed, len, tag, xclass);
            sslMemcpy(dst, str->data, (u32)str->length);
            dst += str->length;
            *outPtr = dst;
        }
    }
    return totalLen;
}

/* [211] 0x00fc00 */
LIBSSL_NOINLINE SslAsn1String *sslD2iAsn1Bytes(SslAsn1String **outStr, const u8 **inPtr,
                                               int maxLen, int tag, int xclass)
{
    SslAsn1String *ret;
    const u8 *cur;
    int objLen = 0;
    int gotTag = 0;
    int gotClass = 0;
    int objFlags;
    int errReason = 0;

    if (outStr != NULL && *outStr != NULL) {
        ret = *outStr;
    } else {
        ret = sslAsn1StringNew();
        if (ret == NULL) {
            return NULL;
        }
    }

    cur = *inPtr;
    objFlags = sslAsn1GetObject(&cur, &objLen, &gotTag, &gotClass, maxLen);
    if ((objFlags & 0x80) != 0 || (maxLen - (int)(cur - *inPtr)) < objLen) {
        errReason = 101;
    } else if (gotTag != tag) {
        errReason = 149;
    } else if ((objFlags & 0x20) != 0) {
        SslAsn1Ctx ctx;
        ctx.cur = cur;
        ctx.eos = 0;
        ctx.error = 0;
        ctx.inf = objFlags;
        ctx.tag = tag;
        ctx.xclass = xclass;
        ctx.slen = objLen;
        ctx.maxPtr = (maxLen == 0) ? NULL : (cur + maxLen);
        ctx.prev = cur;
        ctx.pp = inPtr;
        ctx.line = 0;
        if (!sslAsn1CollectConstructedBytes(ret, &ctx)) {
            errReason = ctx.error;
        } else {
            cur = ctx.cur;
        }
    } else {
        u8 *buf = NULL;
        if (objLen == 0) {
            if (ret->data != NULL) {
                sceSsl_lib_5E5C873A(ret->data);
            }
        } else {
            if (ret->length >= objLen + 1 && ret->data != NULL) {
                buf = ret->data;
            } else {
                if (ret->data != NULL) {
                    sceSsl_lib_5E5C873A(ret->data);
                }
                buf = (u8 *)sceSsl_lib_0861D934((u32)(objLen + 1));
                if (buf == NULL) {
                    errReason = 33;
                }
            }
            if (errReason == 0) {
                sslMemcpy(buf, cur, (u32)objLen);
                buf[objLen] = 0;
                cur += objLen;
            }
        }
        if (errReason == 0) {
            ret->data = buf;
            ret->length = objLen;
            ret->type = tag;
        }
    }

    if (errReason != 0) {
        if (ret != NULL && (outStr == NULL || *outStr != ret)) {
            sslAsn1StringFree(ret);
        }
        sceSsl_lib_9CDCA0D7(13, 126, errReason, NULL, 0);
        return NULL;
    }

    if (outStr != NULL) {
        *outStr = ret;
    }
    *inPtr = cur;
    return ret;
}

/* [212] 0x00fe34 */
LIBSSL_NOINLINE int sslAsn1CollectConstructedBytes(SslAsn1String *outStr, SslAsn1Ctx *ctx)
{
    SslBufMem accBuf;
    SslAsn1String *chunk = NULL;
    int totalLen = 0;
    int failed = 0;

    accBuf.length = 0;
    accBuf.data = NULL;
    accBuf.maxLen = 0;

    if (outStr == NULL) {
        ctx->error = 35;
        failed = 1;
    }

    while (!failed) {
        if ((ctx->inf & 1) != 0) {
            ctx->eos = sslAsn1CheckEoc(&ctx->cur, (int)(ctx->maxPtr - ctx->cur));
            if (ctx->eos != 0) {
                break;
            }
        } else if (ctx->slen <= 0) {
            break;
        }

        ctx->prev = ctx->cur;
        if (sslD2iAsn1Bytes(&chunk, &ctx->cur, (int)(ctx->maxPtr - ctx->cur),
                            ctx->tag, ctx->xclass) == NULL) {
            ctx->error = 2061;
            failed = 1;
            break;
        }
        if (sslBufMemGrow(&accBuf, totalLen + chunk->length) == 0) {
            ctx->error = 2055;
            failed = 1;
            break;
        }
        sslMemcpy(accBuf.data + totalLen, chunk->data, (u32)chunk->length);
        if ((ctx->inf & 1) == 0) {
            ctx->slen -= (int)(ctx->cur - ctx->prev);
        }
        totalLen += chunk->length;
    }

    if (!failed && !sslAsn1Finish(ctx)) {
        failed = 1;
    }

    if (failed) {
        sceSsl_lib_9CDCA0D7(13, 103, ctx->error, NULL, 0);
        if (chunk != NULL) {
            sslAsn1StringFree(chunk);
        }
        if (accBuf.data != NULL) {
            sceSsl_lib_5E5C873A(accBuf.data);
        }
        return 0;
    }

    outStr->length = totalLen;
    if (outStr->data != NULL) {
        sceSsl_lib_5E5C873A(outStr->data);
    }
    outStr->data = accBuf.data;
    if (chunk != NULL) {
        sslAsn1StringFree(chunk);
    }
    return 1;
}

/* [213] 0x00ffe4 */
LIBSSL_NOINLINE int sslI2dAsn1OctetString(const SslAsn1String *str, u8 **outPtr)
{
    return sslI2dAsn1Bytes(str, outPtr, 4, 0);
}

/* [214] 0x010004 */
LIBSSL_NOINLINE SslAsn1String *sslD2iAsn1OctetString(SslAsn1String **outStr, const u8 **inPtr,
                                                     int maxLen)
{
    SslAsn1String *ret = sslD2iAsn1Bytes(outStr, inPtr, maxLen, 4, 0);
    if (ret == NULL) {
        sceSsl_lib_9CDCA0D7(13, 130, 4, NULL, 0);
    }
    return ret;
}

/* [215] 0x010050 */
LIBSSL_NOINLINE int sslI2dAsn1Type(const SslAsn1Type *asnType, u8 **outPtr)
{
    if (asnType == NULL) {
        return 0;
    }

    switch (asnType->type) {
    case 2:
    case 258:
        return sslI2dAsn1Integer(asnType->value.asn1String, outPtr);
    case 3:
        return sslI2dAsn1BitString(asnType->value.asn1String, outPtr);
    case 4:
        return sslI2dAsn1OctetString(asnType->value.asn1String, outPtr);
    case 5:
        if (outPtr != NULL) {
            sslAsn1PutObject(outPtr, 0, 0, 5, 0);
        }
        return 2;
    case 6:
        return sslI2dAsn1Object(asnType->value.object, outPtr);
    case 16:
    case 17: {
        const SslAsn1String *seqStr = asnType->value.asn1String;
        if (seqStr == NULL) {
            return 0;
        }
        if (outPtr != NULL) {
            sslMemcpy(*outPtr, seqStr->data, (u32)seqStr->length);
            *outPtr += seqStr->length;
        }
        return seqStr->length;
    }
    case 19:
    case 20:
    case 22:
    case 27:
    case 28:
    case 30:
        return sslI2dAsn1Bytes(asnType->value.asn1String, outPtr, asnType->type, 0);
    case 23:
        return sslI2dAsn1Utctime(asnType->value.asn1String, outPtr);
    default:
        return 0;
    }
}

/* [216] 0x010260 */
LIBSSL_NOINLINE SslAsn1Type *sslD2iAsn1Type(SslAsn1Type **outType, const u8 **inPtr, int maxLen)
{
    SslAsn1Type *ret;
    const u8 *cur;
    const u8 *endPtr;
    const u8 *bodyPtr;
    int objLen = 0;
    int tag = 0;
    int xclass = 0;
    int ok = 1;

    if (outType != NULL && *outType != NULL) {
        ret = *outType;
    } else {
        ret = sslAsn1TypeNew();
        if (ret == NULL) {
            return NULL;
        }
    }

    cur = *inPtr;
    endPtr = cur + maxLen;
    bodyPtr = cur;
    if ((sslAsn1GetObject(&bodyPtr, &objLen, &tag, &xclass, maxLen) & 0x80) != 0) {
        if (ret != NULL && (outType == NULL || *outType != ret)) {
            sslAsn1TypeFree(ret);
        }
        return NULL;
    }

    sslAsn1TypeFreeValue(ret);
    switch (tag) {
    case 2:
        ret->value.asn1String = sslD2iAsn1Integer(NULL, &cur, (int)(endPtr - cur));
        ok = (ret->value.asn1String != NULL);
        break;
    case 3:
        ret->value.asn1String = sslD2iAsn1BitString(NULL, &cur, (int)(endPtr - cur));
        ok = (ret->value.asn1String != NULL);
        break;
    case 4:
        ret->value.asn1String = sslD2iAsn1OctetString(NULL, &cur, (int)(endPtr - cur));
        ok = (ret->value.asn1String != NULL);
        break;
    case 5:
        cur = bodyPtr;
        ret->value.ptr = NULL;
        break;
    case 6:
        ret->value.object = sslD2iAsn1Object(NULL, &cur, (int)(endPtr - cur));
        ok = (ret->value.object != NULL);
        break;
    case 16:
    case 17:
        if ((int)(endPtr - cur) < objLen) {
            sceSsl_lib_9CDCA0D7(13, 133, 137, NULL, 0);
            ok = 0;
        } else {
            SslAsn1String *rawSeq = sslAsn1StringNew();
            ret->value.asn1String = rawSeq;
            if (rawSeq == NULL) {
                ok = 0;
            } else {
                int fullLen = objLen + (int)(bodyPtr - cur);
                rawSeq->type = tag;
                if (!sslAsn1StringSet(rawSeq, cur, fullLen)) {
                    ok = 0;
                } else {
                    cur += fullLen;
                }
            }
        }
        break;
    case 19:
        ret->value.asn1String = sslD2iAsn1PrintableString(NULL, &cur, (int)(endPtr - cur));
        ok = (ret->value.asn1String != NULL);
        break;
    case 20:
        ret->value.asn1String = sslD2iAsn1TypeBytes(NULL, &cur, (int)(endPtr - cur), 4);
        ok = (ret->value.asn1String != NULL);
        break;
    case 22:
        ret->value.asn1String = sslD2iAsn1TypeBytes(NULL, &cur, (int)(endPtr - cur), 16);
        ok = (ret->value.asn1String != NULL);
        break;
    case 23:
        ret->value.asn1String = sslD2iAsn1Utctime(NULL, &cur, (int)(endPtr - cur));
        ok = (ret->value.asn1String != NULL);
        break;
    case 27:
        ret->value.asn1String = sslD2iAsn1TypeBytes(NULL, &cur, (int)(endPtr - cur), 128);
        ok = (ret->value.asn1String != NULL);
        break;
    case 28:
        ret->value.asn1String = sslD2iAsn1TypeBytes(NULL, &cur, (int)(endPtr - cur), 256);
        ok = (ret->value.asn1String != NULL);
        break;
    case 30:
        ret->value.asn1String = sslD2iAsn1TypeBytes(NULL, &cur, (int)(endPtr - cur), 2048);
        ok = (ret->value.asn1String != NULL);
        break;
    default:
        sceSsl_lib_9CDCA0D7(13, 133, 106, NULL, 0);
        ok = 0;
        break;
    }

    if (!ok) {
        if (ret != NULL && (outType == NULL || *outType != ret)) {
            sslAsn1TypeFree(ret);
        }
        return NULL;
    }

    ret->type = tag;
    if (outType != NULL) {
        *outType = ret;
    }
    *inPtr = cur;
    return ret;
}

/* [217] 0x010554 */
LIBSSL_NOINLINE SslAsn1Type *sslAsn1TypeNew(void)
{
    SslAsn1Type *ret = (SslAsn1Type *)sceSsl_lib_0861D934(sizeof(SslAsn1Type));
    if (ret == NULL) {
        sceSsl_lib_9CDCA0D7(13, 119, 33, NULL, 0);
        return NULL;
    }
    ret->type = -1;
    ret->value.ptr = NULL;
    return ret;
}

/* [218] 0x0105b4 */
LIBSSL_NOINLINE void sslAsn1TypeFree(SslAsn1Type *asnType)
{
    if (asnType == NULL) {
        return;
    }
    sslAsn1TypeFreeValue(asnType);
    sceSsl_lib_5E5C873A(asnType);
}

/* [219] 0x0105e8 */
LIBSSL_NOINLINE void sslAsn1TypeSet(SslAsn1Type *asnType, int type, void *value)
{
    if (asnType->value.ptr != NULL) {
        sslAsn1TypeFreeValue(asnType);
    }
    asnType->type = type;
    asnType->value.ptr = value;
}

/* [220] 0x010638 */
LIBSSL_NOINLINE void sslAsn1TypeFreeValue(SslAsn1Type *asnType)
{
    if (asnType == NULL || asnType->value.ptr == NULL) {
        return;
    }

    if (asnType->type == 6) {
        sslAsn1ObjectFree(asnType->value.object);
    } else if ((asnType->type >= 2 && asnType->type <= 4) ||
               (asnType->type >= 16 && asnType->type <= 28) ||
               asnType->type == 30 || asnType->type == 258) {
        sslAsn1StringFree(asnType->value.asn1String);
    }
    asnType->value.ptr = NULL;
    asnType->type = 0;
}

/* -------------------------------------------------------------------------
 * Private key & X509_ALGOR / X509_SIG DER codecs [221..230]
 * ------------------------------------------------------------------------- */

/* [221] 0x0106e0 */
LIBSSL_NOINLINE SslEvpPkey *sceSsl_lib_A920B39E(int keyType, SslEvpPkey **outKey,
                                                const u8 **inPtr, int maxLen)
{
    SslEvpPkey *ret;
    int errReason;

    if (outKey != NULL && *outKey != NULL) {
        ret = *outKey;
    } else {
        ret = sceSsl_lib_09C45275();
        if (ret == NULL) {
            sceSsl_lib_9CDCA0D7(13, 155, 2054, NULL, 0);
            return NULL;
        }
    }

    ret->saveType = keyType;
    ret->type = sslEvpPkeyType(keyType);
    if (ret->type == 6) {
        ret->pkey = sslD2iRsaPrivateKey(NULL, inPtr, maxLen);
        if (ret->pkey != NULL) {
            if (outKey != NULL) {
                *outKey = ret;
            }
            return ret;
        }
        errReason = 2061;
    } else {
        errReason = 143;
    }

    sceSsl_lib_9CDCA0D7(13, 155, errReason, NULL, 0);
    if (outKey == NULL || *outKey != ret) {
        sceSsl_lib_7317C1BC(ret);
    }
    return NULL;
}

/* [222] 0x0107fc */
LIBSSL_NOINLINE SslRsa *sslD2iRsaPrivateKey(SslRsa **outRsa, const u8 **inPtr, int maxLen)
{
    SslAsn1Ctx ctx;
    SslAsn1String *intStr = NULL;
    SslRsa *ret = NULL;
    int remain = maxLen;
    int errReason = 130;
    int ok = 0;

    sslMemset(&ctx, 0, sizeof(ctx));
    ctx.error = 4;
    ctx.prev = *inPtr;
    ctx.pp = inPtr;

    if (outRsa != NULL && *outRsa != NULL) {
        ret = *outRsa;
    } else {
        ret = sslRsaNew();
        if (ret == NULL) {
            ctx.line = 47;
            ok = -1;
        }
    }

    if (ok == 0) {
        ctx.cur = *inPtr;
        ctx.maxPtr = (remain == 0) ? NULL : (*inPtr + remain);
        if (!sslAsn1SeqStart(&ctx, &remain)) {
            ctx.line = 50;
            ok = -1;
        }
    }

    /* Version integer */
    if (ok == 0) {
        ctx.prev = ctx.cur;
        if (sslD2iAsn1Integer(&intStr, &ctx.cur, ctx.slen) == NULL) {
            ctx.line = 51;
            ok = -1;
        } else {
            ctx.slen -= (int)(ctx.cur - ctx.prev);
            ret->version = (intStr->length != 0) ? intStr->data[0] : 0;
        }
    }

    /* 8 RSA BIGNUM components: n, e, d, p, q, dmp1, dmq1, iqmp */
    if (ok == 0) {
        SslBignum **bnSlots[8];
        static const int kLineNums[8] = { 55, 57, 59, 61, 63, 65, 68, 71 };
        int idx;

        bnSlots[0] = &ret->modN;
        bnSlots[1] = &ret->pubE;
        bnSlots[2] = &ret->privD;
        bnSlots[3] = &ret->primeP;
        bnSlots[4] = &ret->primeQ;
        bnSlots[5] = &ret->expDmp;
        bnSlots[6] = &ret->expDmq;
        bnSlots[7] = &ret->coeffIqmp;

        for (idx = 0; idx < 8; idx++) {
            ctx.prev = ctx.cur;
            if (sslD2iAsn1Integer(&intStr, &ctx.cur, ctx.slen) == NULL) {
                ctx.line = kLineNums[idx];
                ok = -1;
                break;
            }
            ctx.slen -= (int)(ctx.cur - ctx.prev);
            *bnSlots[idx] = sslBnBinToBn(intStr->data, intStr->length, *bnSlots[idx]);
            if (*bnSlots[idx] == NULL) {
                errReason = 2051;
                ok = -1;
                break;
            }
        }
    }

    if (ok == 0) {
        sslAsn1StringFree(intStr);
        intStr = NULL;
        if (!sslAsn1Finish(&ctx)) {
            ctx.line = 78;
            ok = -1;
        }
    }

    if (ok != 0) {
        sceSsl_lib_9CDCA0D7(13, 157, errReason, NULL, 0);
        if (ret != NULL && (outRsa == NULL || *outRsa != ret)) {
            sslRsaFree(ret);
        }
        if (intStr != NULL) {
            sslAsn1StringFree(intStr);
        }
        return NULL;
    }

    *inPtr = ctx.cur;
    if (outRsa != NULL) {
        *outRsa = ret;
    }
    return ret;
}

/* [223] 0x010c6c */
LIBSSL_NOINLINE int sslI2dX509Algor(const SslX509Algor *algor, u8 **outPtr)
{
    int innerLen;
    int totalLen;

    if (algor == NULL) {
        return 0;
    }
    innerLen = sslI2dAsn1Object(algor->algorithm, NULL);
    if (algor->parameter != NULL) {
        innerLen += sslI2dAsn1Type(algor->parameter, NULL);
    }
    totalLen = sslAsn1ObjectSize(1, innerLen, 16);
    if (outPtr != NULL) {
        u8 *dst = *outPtr;
        sslAsn1PutObject(&dst, 1, innerLen, 16, 0);
        sslI2dAsn1Object(algor->algorithm, &dst);
        if (algor->parameter != NULL) {
            sslI2dAsn1Type(algor->parameter, &dst);
        }
        *outPtr = dst;
    }
    return totalLen;
}

/* [224] 0x010d4c */
LIBSSL_NOINLINE SslX509Algor *sslD2iX509Algor(SslX509Algor **outAlgor, const u8 **inPtr, int maxLen)
{
    SslAsn1Ctx ctx;
    SslX509Algor *ret = NULL;
    int remain = maxLen;
    int ok = 1;

    sslMemset(&ctx, 0, sizeof(ctx));
    ctx.error = 4;
    ctx.prev = *inPtr;
    ctx.pp = inPtr;

    if (outAlgor != NULL && *outAlgor != NULL) {
        ret = *outAlgor;
    } else {
        ret = sslX509AlgorNew();
        if (ret == NULL) {
            ctx.line = 43;
            ok = 0;
        }
    }

    if (ok) {
        ctx.cur = *inPtr;
        ctx.maxPtr = (remain == 0) ? NULL : (*inPtr + remain);
        if (!sslAsn1SeqStart(&ctx, &remain)) {
            ctx.line = 46;
            ok = 0;
        }
    }

    if (ok) {
        ctx.prev = ctx.cur;
        if (sslD2iAsn1Object(&ret->algorithm, &ctx.cur, ctx.slen) == NULL) {
            ctx.line = 47;
            ok = 0;
        } else {
            ctx.slen -= (int)(ctx.cur - ctx.prev);
        }
    }

    if (ok) {
        int atEnd;
        if ((ctx.inf & 1) != 0) {
            ctx.eos = sslAsn1CheckEoc(&ctx.cur, ctx.slen);
            atEnd = (ctx.eos != 0);
        } else {
            atEnd = (ctx.slen <= 0);
        }

        if (atEnd) {
            sslAsn1TypeFree(ret->parameter);
            ret->parameter = NULL;
        } else {
            ctx.prev = ctx.cur;
            if (sslD2iAsn1Type(&ret->parameter, &ctx.cur, ctx.slen) == NULL) {
                ctx.line = 49;
                ok = 0;
            } else {
                ctx.slen -= (int)(ctx.cur - ctx.prev);
            }
        }
    }

    if (ok && !sslAsn1Finish(&ctx)) {
        ctx.line = 55;
        ok = 0;
    }

    if (!ok) {
        sceSsl_lib_9CDCA0D7(13, 160, ctx.error, NULL, 0);
        sslAsn1AddError(*inPtr, (int)(ctx.prev - *inPtr));
        if (ret != NULL && (outAlgor == NULL || *outAlgor != ret)) {
            sslX509AlgorFree(ret);
        }
        return NULL;
    }

    *inPtr = ctx.cur;
    if (outAlgor != NULL) {
        *outAlgor = ret;
    }
    return ret;
}

/* [225] 0x010f38 */
LIBSSL_NOINLINE SslX509Algor *sslX509AlgorNew(void)
{
    SslX509Algor *ret = (SslX509Algor *)sceSsl_lib_0861D934(sizeof(SslX509Algor));
    if (ret == NULL) {
        sceSsl_lib_9CDCA0D7(13, 202, 33, NULL, 0);
        return NULL;
    }
    ret->algorithm = sslObjNidToObj(0);
    ret->parameter = NULL;
    return ret;
}

/* [226] 0x010fa8 */
LIBSSL_NOINLINE void sslX509AlgorFree(SslX509Algor *algor)
{
    if (algor == NULL) {
        return;
    }
    sslAsn1ObjectFree(algor->algorithm);
    sslAsn1TypeFree(algor->parameter);
    sceSsl_lib_5E5C873A(algor);
}

/* [227] 0x010fe4 */
LIBSSL_NOINLINE int sslI2dX509Sig(const SslX509Sig *sig, u8 **outPtr)
{
    int innerLen;
    int totalLen;

    if (sig == NULL) {
        return 0;
    }
    innerLen = sslI2dX509Algor(sig->algor, NULL);
    innerLen += sslI2dAsn1OctetString(sig->digest, NULL);
    totalLen = sslAsn1ObjectSize(1, innerLen, 16);
    if (outPtr != NULL) {
        u8 *dst = *outPtr;
        sslAsn1PutObject(&dst, 1, innerLen, 16, 0);
        sslI2dX509Algor(sig->algor, &dst);
        sslI2dAsn1OctetString(sig->digest, &dst);
        *outPtr = dst;
    }
    return totalLen;
}

/* [228] 0x0110ac */
LIBSSL_NOINLINE SslX509Sig *sslD2iX509Sig(SslX509Sig **outSig, const u8 **inPtr, int maxLen)
{
    SslAsn1Ctx ctx;
    SslX509Sig *ret = NULL;
    int remain = maxLen;
    int ok = 1;

    sslMemset(&ctx, 0, sizeof(ctx));
    ctx.error = 4;
    ctx.prev = *inPtr;
    ctx.pp = inPtr;

    if (outSig != NULL && *outSig != NULL) {
        ret = *outSig;
    } else {
        ret = sslX509SigNew();
        if (ret == NULL) {
            ctx.line = 41;
            ok = 0;
        }
    }

    if (ok) {
        ctx.cur = *inPtr;
        ctx.maxPtr = (remain == 0) ? NULL : (*inPtr + remain);
        if (!sslAsn1SeqStart(&ctx, &remain)) {
            ctx.line = 44;
            ok = 0;
        }
    }

    if (ok) {
        ctx.prev = ctx.cur;
        if (sslD2iX509Algor(&ret->algor, &ctx.cur, ctx.slen) == NULL) {
            ctx.line = 45;
            ok = 0;
        } else {
            ctx.slen -= (int)(ctx.cur - ctx.prev);
        }
    }

    if (ok) {
        ctx.prev = ctx.cur;
        if (sslD2iAsn1OctetString(&ret->digest, &ctx.cur, ctx.slen) == NULL) {
            ctx.line = 46;
            ok = 0;
        } else {
            ctx.slen -= (int)(ctx.cur - ctx.prev);
        }
    }

    if (ok && !sslAsn1Finish(&ctx)) {
        ctx.line = 47;
        ok = 0;
    }

    if (!ok) {
        sceSsl_lib_9CDCA0D7(13, 174, ctx.error, NULL, 0);
        sslAsn1AddError(*inPtr, (int)(ctx.prev - *inPtr));
        if (ret != NULL && (outSig == NULL || *outSig != ret)) {
            sslX509SigFree(ret);
        }
        return NULL;
    }

    *inPtr = ctx.cur;
    if (outSig != NULL) {
        *outSig = ret;
    }
    return ret;
}

/* [229] 0x011258 */
LIBSSL_NOINLINE SslX509Sig *sslX509SigNew(void)
{
    SslX509Sig *ret = (SslX509Sig *)sceSsl_lib_0861D934(sizeof(SslX509Sig));
    if (ret == NULL) {
        sceSsl_lib_9CDCA0D7(13, 219, 33, NULL, 0);
        return NULL;
    }
    ret->algor = sslX509AlgorNew();
    if (ret->algor == NULL) {
        return NULL;
    }
    ret->digest = sslAsn1StringTypeNew(4);
    if (ret->digest == NULL) {
        return NULL;
    }
    return ret;
}

/* [230] 0x0112dc */
LIBSSL_NOINLINE void sslX509SigFree(SslX509Sig *sig)
{
    if (sig == NULL) {
        return;
    }
    sslX509AlgorFree(sig->algor);
    sslAsn1StringFree(sig->digest);
    sceSsl_lib_5E5C873A(sig);
}
