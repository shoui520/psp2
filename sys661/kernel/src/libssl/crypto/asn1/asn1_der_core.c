/*
 * SceSsl / sceSsl_Module (ePSP 6.61) - Core ASN.1 DER primitives & EVP ASN.1 helpers.
 *
 * Implements functions [375..403] (0x016e6c..0x0185ab):
 *   - a_int.c (i2d/d2i_ASN1_INTEGER, set/get)
 *   - a_object.c (i2d/d2i_ASN1_OBJECT, new/free)
 *   - a_print.c / a_utctm.c (d2i_ASN1_PRINTABLESTRING, i2d/d2i/check_ASN1_UTCTIME)
 *   - asn1_lib.c (check_eoc, get/put_object, object_size, Finish, Get_length, ASN1_STRING_*)
 *   - evp_asn1.c (ASN1_TYPE_set/get_octetstring, ASN1_TYPE_set/get_int_octetstring)
 */
#include "../../libssl_internal.h"

static const int g_utcTimeMin[8] = { 0, 1, 1, 0, 0, 0, 0, 0 };
static const int g_utcTimeMax[8] = { 99, 12, 31, 23, 59, 59, 12, 59 };

/* -------------------------------------------------------------------------
 * a_int.c [375..378]
 * ------------------------------------------------------------------------- */

/* [375] 0x016e6c */
LIBSSL_NOINLINE int sslI2dAsn1Integer(const SslAsn1String *asnInt, u8 **outPtr)
{
    int pad = 0;
    u8 padByte = 0;
    int bodyLen;
    int totalLen;

    if (asnInt == NULL || asnInt->data == NULL) {
        return 0;
    }

    if (asnInt->length == 0) {
        bodyLen = 1;
    } else if (asnInt->type == 2 && asnInt->data[0] >= 0x80U) {
        pad = 1;
        bodyLen = asnInt->length + 1;
    } else if (asnInt->type == 0x102 && asnInt->data[0] >= 0x81U) {
        pad = 1;
        padByte = 0xff;
        bodyLen = asnInt->length + 1;
    } else {
        bodyLen = asnInt->length;
    }

    totalLen = sslAsn1ObjectSize(0, bodyLen, 2);
    if (outPtr != NULL) {
        u8 *dst = *outPtr;
        sslAsn1PutObject(&dst, 0, bodyLen, 2, 0);
        if (pad != 0) {
            *dst++ = padByte;
        }
        if (asnInt->length == 0) {
            *dst++ = 0;
        } else if (asnInt->type == 2) {
            sslMemcpy(dst, asnInt->data, (u32)asnInt->length);
            dst += asnInt->length;
        } else {
            u8 *firstByte = dst;
            int idx;
            for (idx = 0; idx < asnInt->length; idx++) {
                *dst++ = (u8)(-(signed char)asnInt->data[idx]);
            }
            if (pad == 0) {
                *firstByte |= 0x80U;
            }
        }
        *outPtr = dst;
    }
    return totalLen;
}

/* [376] 0x017024 */
LIBSSL_NOINLINE SslAsn1String *sslD2iAsn1Integer(SslAsn1String **outInt, const u8 **inPtr, int maxLen)
{
    SslAsn1String *ret;
    const u8 *cur;
    int objLen = 0;
    int tag = 0;
    int xclass = 0;
    int errReason = 0;
    u8 *buf = NULL;

    if (outInt != NULL && *outInt != NULL) {
        ret = *outInt;
    } else {
        ret = sslAsn1StringTypeNew(2);
        if (ret == NULL) {
            return NULL;
        }
        ret->type = 2;
    }

    cur = *inPtr;
    if ((sslAsn1GetObject(&cur, &objLen, &tag, &xclass, maxLen) & 0x80) != 0 ||
        (maxLen - (int)(cur - *inPtr)) < objLen) {
        errReason = 101;
    } else if (tag != 2) {
        errReason = 113;
    } else {
        buf = (u8 *)sceSsl_lib_0861D934((u32)(objLen + 1));
        if (buf == NULL) {
            errReason = 33;
        }
    }

    if (errReason != 0) {
        sceSsl_lib_9CDCA0D7(13, 128, errReason, NULL, 0);
        if (ret != NULL && (outInt == NULL || *outInt != ret)) {
            sslAsn1StringFree(ret);
        }
        return NULL;
    }

    if ((signed char)(*cur) < 0) {
        int idx;
        ret->type = 0x102;
        if (*cur == 0xffU) {
            cur++;
            objLen--;
        }
        for (idx = 0; idx < objLen; idx++) {
            buf[idx] = (u8)(-(signed char)(*cur++));
        }
    } else {
        ret->type = 2;
        if (*cur == 0 && objLen != 1) {
            cur++;
            objLen--;
        }
        sslMemcpy(buf, cur, (u32)objLen);
        cur += objLen;
    }

    if (ret->data != NULL) {
        sceSsl_lib_5E5C873A(ret->data);
    }
    ret->data = buf;
    ret->length = objLen;
    if (outInt != NULL) {
        *outInt = ret;
    }
    *inPtr = cur;
    return ret;
}

/* [377] 0x017240 */
LIBSSL_NOINLINE int sslAsn1IntegerSet(SslAsn1String *asnInt, int value)
{
    u8 tmpBytes[4];
    int absVal;
    int byteCount = 0;
    int outPos = 0;
    int idx;

    asnInt->type = 2;
    if ((u32)asnInt->length < 5U) {
        if (asnInt->data != NULL) {
            sceSsl_lib_5E5C873A(asnInt->data);
        }
        asnInt->data = (u8 *)sceSsl_lib_0861D934(5U);
        if (asnInt->data != NULL) {
            sslMemset(asnInt->data, 0, 5U);
        }
    }
    if (asnInt->data == NULL) {
        sceSsl_lib_9CDCA0D7(13, 111, 33, NULL, 0);
        return 0;
    }

    absVal = value;
    if (value < 0) {
        asnInt->type = 0x102;
        absVal = -value;
    }

    for (idx = 0; idx < 4; idx++) {
        if (absVal == 0) {
            break;
        }
        tmpBytes[idx] = (u8)absVal;
        absVal >>= 8;
        byteCount++;
    }

    if (value < 0) {
        asnInt->data[outPos++] = 0;
    }
    for (idx = byteCount - 1; idx >= 0; idx--) {
        asnInt->data[outPos++] = tmpBytes[idx];
    }
    asnInt->length = outPos;
    return 1;
}

/* [378] 0x017354 */
LIBSSL_NOINLINE int sslAsn1IntegerGet(const SslAsn1String *asnInt)
{
    int isNeg = 0;
    int acc = 0;
    int idx;

    if (asnInt == NULL) {
        return 0;
    }
    if (asnInt->type == 0x102) {
        isNeg = 1;
    } else if (asnInt->type != 2) {
        return 0;
    }
    if ((u32)asnInt->length >= 5U) {
        return -1;
    }
    if (asnInt->data == NULL) {
        return 0;
    }
    for (idx = 0; idx < asnInt->length; idx++) {
        acc = (acc << 8) | asnInt->data[idx];
    }
    return isNeg ? -acc : acc;
}

/* -------------------------------------------------------------------------
 * a_object.c [379..382]
 * ------------------------------------------------------------------------- */

/* [379] 0x0173d8 */
LIBSSL_NOINLINE int sslI2dAsn1Object(const SslAsn1Object *obj, u8 **outPtr)
{
    if (obj == NULL || obj->data == NULL) {
        return 0;
    }
    if (outPtr == NULL) {
        return sslAsn1ObjectSize(0, obj->length, 6);
    }

    {
        u8 *dst = *outPtr;
        sslAsn1PutObject(&dst, 0, obj->length, 6, 0);
        sslMemcpy(dst, obj->data, (u32)obj->length);
        dst += obj->length;
        *outPtr = dst;
        return obj->length;
    }
}

/* [380] 0x017480 */
LIBSSL_NOINLINE SslAsn1Object *sslD2iAsn1Object(SslAsn1Object **outObj, const u8 **inPtr, int maxLen)
{
    SslAsn1Object *ret;
    const u8 *cur;
    int objLen = 0;
    int tag = 0;
    int xclass = 0;
    int errReason = 0;
    u8 *buf;

    if (outObj != NULL && *outObj != NULL && ((*outObj)->flags & 1) != 0) {
        ret = *outObj;
    } else {
        ret = sslAsn1ObjectNew();
        if (ret == NULL) {
            return NULL;
        }
    }

    cur = *inPtr;
    if ((sslAsn1GetObject(&cur, &objLen, &tag, &xclass, maxLen) & 0x80) != 0 ||
        (maxLen - (int)(cur - *inPtr)) < objLen) {
        errReason = 101;
    } else if (tag != 6) {
        errReason = 114;
    } else {
        buf = (u8 *)ret->data;
        if (buf == NULL || ret->length < objLen) {
            if (buf != NULL) {
                sceSsl_lib_5E5C873A(buf);
            }
            buf = (u8 *)sceSsl_lib_0861D934((u32)objLen);
            ret->data = buf;
            ret->flags |= 8;
            if (buf == NULL) {
                errReason = 33;
            }
        }
    }

    if (errReason != 0) {
        sceSsl_lib_9CDCA0D7(13, 129, errReason, NULL, 0);
        if (ret != NULL && (outObj == NULL || *outObj != ret)) {
            sslAsn1ObjectFree(ret);
        }
        return NULL;
    }

    sslMemcpy((void *)ret->data, cur, (u32)objLen);
    ret->shortName = NULL;
    ret->longName = NULL;
    ret->length = objLen;
    cur += objLen;
    if (outObj != NULL) {
        *outObj = ret;
    }
    *inPtr = cur;
    return ret;
}

/* [381] 0x017618 */
LIBSSL_NOINLINE SslAsn1Object *sslAsn1ObjectNew(void)
{
    SslAsn1Object *obj = (SslAsn1Object *)sceSsl_lib_0861D934(sizeof(SslAsn1Object));
    if (obj == NULL) {
        sceSsl_lib_9CDCA0D7(13, 113, 33, NULL, 0);
        return NULL;
    }
    obj->shortName = NULL;
    obj->longName = NULL;
    obj->nid = 0;
    obj->length = 0;
    obj->data = NULL;
    obj->flags = 1;
    return obj;
}

/* [382] 0x017680 */
LIBSSL_NOINLINE void sslAsn1ObjectFree(SslAsn1Object *obj)
{
    if (obj == NULL) {
        return;
    }
    if ((obj->flags & 4) != 0) {
        if (obj->shortName != NULL) {
            sceSsl_lib_5E5C873A((void *)obj->shortName);
        }
        if (obj->longName != NULL) {
            sceSsl_lib_5E5C873A((void *)obj->longName);
        }
        obj->shortName = NULL;
        obj->longName = NULL;
    }
    if ((obj->flags & 8) != 0) {
        if (obj->data != NULL) {
            sceSsl_lib_5E5C873A((void *)obj->data);
        }
        obj->data = NULL;
        obj->length = 0;
    }
    if ((obj->flags & 1) != 0) {
        sceSsl_lib_5E5C873A(obj);
    }
}

/* -------------------------------------------------------------------------
 * a_print.c / a_utctm.c [383..386]
 * ------------------------------------------------------------------------- */

/* [383] 0x017728 */
LIBSSL_NOINLINE SslAsn1String *sslD2iAsn1PrintableString(SslAsn1String **outStr,
                                                         const u8 **inPtr, int maxLen)
{
    return sslD2iAsn1TypeBytes(outStr, inPtr, maxLen, 2);
}

/* [384] 0x017744 */
LIBSSL_NOINLINE int sslI2dAsn1Utctime(const SslAsn1String *str, u8 **outPtr)
{
    return sslI2dAsn1Bytes(str, outPtr, 23, 0);
}

/* [385] 0x017764 */
LIBSSL_NOINLINE SslAsn1String *sslD2iAsn1Utctime(SslAsn1String **outStr,
                                                 const u8 **inPtr, int maxLen)
{
    SslAsn1String *ret = sslD2iAsn1Bytes(outStr, inPtr, maxLen, 23, 0);
    if (ret == NULL) {
        sceSsl_lib_9CDCA0D7(13, 135, 4, NULL, 0);
        return NULL;
    }
    if (!sslAsn1UtctimeCheck(ret)) {
        sceSsl_lib_9CDCA0D7(13, 135, 123, NULL, 0);
        if (outStr == NULL || *outStr != ret) {
            sslAsn1StringFree(ret);
        }
        return NULL;
    }
    return ret;
}

/* [386] 0x017814 */
LIBSSL_NOINLINE int sslAsn1UtctimeCheck(const SslAsn1String *str)
{
    const char *text;
    int len;
    int pos = 0;
    int idx;

    if (str->type != 23 || str->length < 11) {
        return 0;
    }

    len = str->length;
    text = (const char *)str->data;

    for (idx = 0; idx < 6; idx++) {
        u8 chA;
        u8 chB;
        int val;

        if (idx == 5 && (text[pos] == 'Z' || text[pos] == '+' || text[pos] == '-')) {
            break;
        }
        chA = (u8)text[pos++];
        if (chA < '0' || chA > '9' || pos > len) {
            return 0;
        }
        chB = (u8)text[pos++];
        if (chB < '0' || chB > '9' || pos > len) {
            return 0;
        }
        val = 10 * (chA - '0') + (chB - '0');
        if (val < g_utcTimeMin[idx] || val > g_utcTimeMax[idx]) {
            return 0;
        }
    }

    if (text[pos] == 'Z') {
        pos++;
        return (pos == len);
    }
    if (text[pos] != '+' && text[pos] != '-') {
        return (pos == len);
    }

    pos++;
    if (pos + 4 > len) {
        return 0;
    }
    for (idx = 6; idx < 8; idx++) {
        u8 chA = (u8)text[pos++];
        u8 chB = (u8)text[pos++];
        int val;
        if (chA < '0' || chA > '9' || chB < '0' || chB > '9') {
            return 0;
        }
        val = 10 * (chA - '0') + (chB - '0');
        if (val < g_utcTimeMin[idx] || val > g_utcTimeMax[idx]) {
            return 0;
        }
    }
    return (pos == len);
}

/* -------------------------------------------------------------------------
 * asn1_lib.c [387..399]
 * ------------------------------------------------------------------------- */

/* [387] 0x017a30 */
LIBSSL_NOINLINE int sslAsn1CheckEoc(const u8 **inPtr, int maxLen)
{
    if (maxLen <= 0) {
        return 1;
    }
    if (maxLen < 2) {
        return 0;
    }
    if ((*inPtr)[0] == 0 && (*inPtr)[1] == 0) {
        *inPtr += 2;
        return 1;
    }
    return 0;
}

/* [388] 0x017a74 */
LIBSSL_NOINLINE int sslAsn1GetObject(const u8 **inPtr, int *outLen, int *outTag,
                                     int *outClass, int maxLen)
{
    const u8 *cur = *inPtr;
    int remain = maxLen;
    int tag;
    int constructed;
    int xclass;
    int infFlag = 0;

    if (remain == 0) {
        sceSsl_lib_9CDCA0D7(13, 107, 120, NULL, 0);
        return 0x80;
    }

    tag = *cur & 0x1f;
    constructed = *cur & 0x20;
    xclass = *cur & 0xc0;
    cur++;
    remain--;

    if (tag == 31) {
        int acc = 0;
        if (remain == 0) {
            sceSsl_lib_9CDCA0D7(13, 107, 120, NULL, 0);
            return 0x80;
        }
        while (((signed char)(*cur)) < 0) {
            acc = (acc << 7) | (*cur & 0x7f);
            cur++;
            remain--;
            if (remain == 0) {
                sceSsl_lib_9CDCA0D7(13, 107, 120, NULL, 0);
                return 0x80;
            }
        }
        tag = (acc << 7) | (*cur & 0x7f);
        cur++;
        remain--;
    }

    if (remain == 0) {
        sceSsl_lib_9CDCA0D7(13, 107, 120, NULL, 0);
        return 0x80;
    }

    *outTag = tag;
    *outClass = xclass;
    if (!sslAsn1GetLength(&cur, &infFlag, outLen, remain)) {
        sceSsl_lib_9CDCA0D7(13, 107, 120, NULL, 0);
        return 0x80;
    }
    *inPtr = cur;
    return constructed | infFlag;
}

/* [389] 0x017bac */
LIBSSL_NOINLINE int sslAsn1GetLength(const u8 **inPtr, int *outInf, int *outLen, int maxLen)
{
    const u8 *cur = *inPtr;
    int lenVal = 0;
    int remain = maxLen - 1;

    if (maxLen <= 0) {
        return 0;
    }
    if (*cur == 0x80U) {
        *outInf = 1;
        *inPtr = cur + 1;
        *outLen = 0;
        return 1;
    }

    *outInf = 0;
    if (((signed char)(*cur)) >= 0) {
        lenVal = *cur++;
    } else {
        int count = *cur++ & 0x7f;
        if (count >= 5) {
            return 0;
        }
        if (remain == 0) {
            return 0;
        }
        remain--;
        while (count > 0) {
            if (remain < 0) {
                return 0;
            }
            lenVal = (lenVal << 8) | (*cur++);
            count--;
            remain--;
        }
    }

    if (lenVal < 0) {
        return 0;
    }
    *inPtr = cur;
    *outLen = lenVal;
    return 1;
}

/* [390] 0x017c7c */
LIBSSL_NOINLINE void sslAsn1PutObject(u8 **outPtr, int constructed, int bodyLen,
                                      int tag, int xclass)
{
    u8 *dst = *outPtr;
    u8 leadByte = (u8)(((constructed != 0) ? 0x20 : 0) | (xclass & 0xc0));

    if (tag >= 31) {
        *dst++ = (u8)(leadByte | 0x1f);
        while (tag >= 128) {
            *dst++ = (u8)((tag & 0x7f) | 0x80);
            tag >>= 7;
        }
        *dst++ = (u8)(tag & 0x7f);
    } else {
        *dst++ = (u8)(leadByte | (tag & 0x1f));
    }

    if (constructed == 2 && bodyLen == 0) {
        *dst++ = 0x80;
    } else {
        sslAsn1PutLength(&dst, bodyLen);
    }
    *outPtr = dst;
}

/* [391] 0x017d6c */
LIBSSL_NOINLINE void sslAsn1PutLength(u8 **outPtr, int bodyLen)
{
    u8 *dst = *outPtr;

    if (bodyLen < 128) {
        *dst++ = (u8)bodyLen;
    } else {
        int scan = bodyLen;
        int numBytes = 0;
        int idx;
        while (scan > 0) {
            scan >>= 8;
            numBytes++;
        }
        *dst++ = (u8)(numBytes | 0x80);
        for (idx = numBytes - 1; idx >= 0; idx--) {
            dst[idx] = (u8)bodyLen;
            bodyLen >>= 8;
        }
        dst += numBytes;
    }
    *outPtr = dst;
}

/* [392] 0x017ddc */
LIBSSL_NOINLINE int sslAsn1ObjectSize(int constructed, int bodyLen, int tag)
{
    int total = bodyLen + 1;

    if (tag >= 31) {
        while (tag > 0) {
            tag >>= 7;
            total++;
        }
    }
    if (bodyLen == 0 && constructed == 2) {
        total += 2;
    }
    total++;
    if (bodyLen >= 128) {
        while (bodyLen > 0) {
            bodyLen >>= 8;
            total++;
        }
    }
    return total;
}

/* [393] 0x017e3c */
LIBSSL_NOINLINE int sslAsn1Finish(SslAsn1Ctx *ctx)
{
    if (ctx->inf == 33 && ctx->eos == 0) {
        if (!sslAsn1CheckEoc(&ctx->cur, ctx->slen)) {
            ctx->error = 9;
            return 0;
        }
    }
    if (ctx->slen != 0) {
        if ((ctx->inf & 1) == 0 || ctx->slen < 0) {
            ctx->error = 8;
            return 0;
        }
    }
    return 1;
}

/* [394] 0x017ec0 */
LIBSSL_NOINLINE int sslAsn1SeqStart(SslAsn1Ctx *ctx, int *remainLen)
{
    const u8 *start = ctx->cur;

    ctx->inf = sslAsn1GetObject(&ctx->cur, &ctx->slen, &ctx->tag, &ctx->xclass, *remainLen);
    if ((ctx->inf & 0x80) != 0) {
        ctx->error = 6;
        return 0;
    }
    if (ctx->tag != 16) {
        ctx->error = 7;
        return 0;
    }

    *remainLen -= (int)(ctx->cur - start);
    if (ctx->maxPtr != NULL && *remainLen < 0) {
        ctx->error = 8;
        return 0;
    }
    if (ctx->inf == 33) {
        ctx->slen = (int)((*ctx->pp + *remainLen) - ctx->cur);
    }
    ctx->eos = 0;
    return 1;
}

/* [395] 0x017fa4 */
LIBSSL_NOINLINE int sslAsn1StringSet(SslAsn1String *str, const void *srcData, int dataLen)
{
    if (dataLen < 0) {
        if (srcData == NULL) {
            return 0;
        }
        dataLen = sslStrlen((const char *)srcData);
    }

    if (str->length < dataLen || str->data == NULL) {
        u8 *prevBuf = str->data;
        if (prevBuf != NULL) {
            str->data = (u8 *)sslCryptoRealloc(prevBuf, (u32)(dataLen + 1));
        } else {
            str->data = (u8 *)sceSsl_lib_0861D934((u32)(dataLen + 1));
        }
        if (str->data == NULL) {
            str->data = prevBuf;
            return 0;
        }
    }

    str->length = dataLen;
    if (srcData != NULL) {
        sslMemcpy(str->data, srcData, (u32)dataLen);
        str->data[dataLen] = 0;
    }
    str->flags = 0;
    return 1;
}

/* [396] 0x018088 */
LIBSSL_NOINLINE SslAsn1String *sslAsn1StringNew(void)
{
    return sslAsn1StringTypeNew(4);
}

/* [397] 0x0180a4 */
LIBSSL_NOINLINE SslAsn1String *sslAsn1StringTypeNew(int type)
{
    SslAsn1String *str = (SslAsn1String *)sceSsl_lib_0861D934(sizeof(SslAsn1String));
    if (str == NULL) {
        sceSsl_lib_9CDCA0D7(13, 116, 33, NULL, 0);
        return NULL;
    }
    str->length = 0;
    str->type = type;
    str->data = NULL;
    str->flags = 0;
    return str;
}

/* [398] 0x01810c */
LIBSSL_NOINLINE void sslAsn1StringFree(SslAsn1String *str)
{
    if (str == NULL) {
        return;
    }
    if (str->data != NULL) {
        sceSsl_lib_5E5C873A(str->data);
    }
    sceSsl_lib_5E5C873A(str);
}

/* [399] 0x01814c */
LIBSSL_NOINLINE void sslAsn1AddError(const u8 *addr, int offset)
{
    char addrText[16];
    char offText[16];

    sslSprintf(addrText, "%lu", (unsigned long)addr);
    sslSprintf(offText, "%d", offset);
    sslErrAddErrorData(4, "address=", addrText, " offset=", offText);
}

/* -------------------------------------------------------------------------
 * evp_asn1.c [400..403]
 * ------------------------------------------------------------------------- */

/* [400] 0x0181c0 */
LIBSSL_NOINLINE int sslAsn1TypeSetOctetstring(SslAsn1Type *asnType, const u8 *inData, int dataLen)
{
    SslAsn1String *str = sslAsn1StringTypeNew(4);
    if (str == NULL) {
        return 0;
    }
    if (!sslAsn1StringSet(str, inData, dataLen)) {
        return 0;
    }
    sslAsn1TypeSet(asnType, 4, str);
    return 1;
}

/* [401] 0x01824c */
LIBSSL_NOINLINE int sslAsn1TypeGetOctetstring(const SslAsn1Type *asnType, u8 *outData, int maxLen)
{
    const SslAsn1String *str;
    int copyLen;

    if (asnType->type != 4 || asnType->value.asn1String == NULL) {
        sceSsl_lib_9CDCA0D7(13, 118, 110, NULL, 0);
        return -1;
    }

    str = asnType->value.asn1String;
    {
        int fullLen = str->length;
        copyLen = (fullLen < maxLen) ? fullLen : maxLen;
        sslMemcpy(outData, str->data, (u32)copyLen);
        return fullLen;
    }
}

/* [402] 0x0182c4 */
LIBSSL_NOINLINE int sslAsn1TypeSetIntOctetstring(SslAsn1Type *asnType, int intVal,
                                                 const u8 *inData, int dataLen)
{
    SslAsn1String octStr;
    SslAsn1String intStr;
    u8 intBuf[32];
    int innerLen;
    int totalLen;
    SslAsn1String *seqStr;
    u8 *dst;

    intStr.data = intBuf;
    intStr.length = 32;
    intStr.type = 2;
    intStr.flags = 0;

    octStr.data = (u8 *)inData;
    octStr.length = dataLen;
    octStr.type = 4;
    octStr.flags = 0;

    sslAsn1IntegerSet(&intStr, intVal);
    innerLen = sslI2dAsn1Integer(&intStr, NULL);
    innerLen += sslI2dAsn1Bytes(&octStr, NULL, 4, 4);
    totalLen = sslAsn1ObjectSize(1, innerLen, 16);

    seqStr = sslAsn1StringNew();
    if (seqStr == NULL) {
        return 0;
    }

    sslAsn1StringSet(seqStr, NULL, totalLen);
    seqStr->length = totalLen;
    dst = seqStr->data;
    sslAsn1PutObject(&dst, 1, innerLen, 16, 0);
    sslI2dAsn1Integer(&intStr, &dst);
    sslI2dAsn1Bytes(&octStr, &dst, 4, 4);
    sslAsn1TypeSet(asnType, 16, seqStr);
    return 1;
}

/* [403] 0x0183e8 */
LIBSSL_NOINLINE int sslAsn1TypeGetIntOctetstring(const SslAsn1Type *asnType, int *outInt,
                                                 u8 *outData, int maxLen)
{
    SslAsn1Ctx ctx;
    SslAsn1String *intStr = NULL;
    SslAsn1String *octStr = NULL;
    const u8 *startPtr;
    int remain;
    int ret = -1;
    int ok = 1;

    if (asnType->type != 16 || asnType->value.asn1String == NULL) {
        ok = 0;
    }

    if (ok) {
        const SslAsn1String *seqStr = asnType->value.asn1String;
        startPtr = seqStr->data;
        remain = seqStr->length;
        ctx.cur = startPtr;
        ctx.error = 110;
        ctx.maxPtr = startPtr + remain;
        ctx.pp = &startPtr;

        if (!sslAsn1SeqStart(&ctx, &remain)) {
            ctx.line = 130;
            ok = 0;
        }
    }

    if (ok) {
        ctx.prev = ctx.cur;
        intStr = sslD2iAsn1Integer(NULL, &ctx.cur, ctx.slen);
        if (intStr == NULL) {
            ok = 0;
        } else {
            ctx.slen -= (int)(ctx.cur - ctx.prev);
            ctx.prev = ctx.cur;
            octStr = sslD2iAsn1OctetString(NULL, &ctx.cur, ctx.slen);
            if (octStr == NULL) {
                ok = 0;
            } else {
                ctx.slen -= (int)(ctx.cur - ctx.prev);
                if ((ctx.inf & 1) != 0) {
                    ctx.eos = sslAsn1CheckEoc(&ctx.cur, ctx.slen);
                    if (ctx.eos == 0) {
                        ok = 0;
                    }
                } else if (ctx.slen > 0) {
                    ok = 0;
                }
            }
        }
    }

    if (!ok) {
        sceSsl_lib_9CDCA0D7(13, 117, 110, NULL, 0);
    } else {
        int copyLen;
        if (outInt != NULL) {
            *outInt = sslAsn1IntegerGet(intStr);
        }
        ret = octStr->length;
        copyLen = (ret < maxLen) ? ret : maxLen;
        if (outData != NULL) {
            sslMemcpy(outData, octStr->data, (u32)copyLen);
        }
    }

    if (octStr != NULL) {
        sslAsn1StringFree(octStr);
    }
    if (intStr != NULL) {
        sslAsn1StringFree(intStr);
    }
    return ret;
}
