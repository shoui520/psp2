/*
 * SceSsl / sceSsl_Module (ePSP 6.61) - BIO layer (buffer, memory, socket, core).
 *
 * Implements functions:
 *   - [231..273] (0x011318..0x012587): BIO_printf, bf_buff.c, bio flag helpers,
 *                                      bss_mem.c, bss_sock.c
 *   - [404..417] (0x0185ac..0x018d63): bio_lib.c + cert print adapter
 */
#include "../../libssl_internal.h"

typedef struct SslBioBufferCtx {
    int ibufSize;
    int obufSize;
    u8 *ibuf;
    int ibufLen;
    int ibufOff;
    u8 *obuf;
    int obufLen;
    int obufOff;
} SslBioBufferCtx;

static char g_bioPrintfBuf[2048];
static SslStack *g_bioExDataFuncs = NULL;

/* Forward declarations for static BIO_METHOD descriptors */
static int sslBufBioWrite(SslBio *bio, const void *inBuf, int inLen);
static int sslBufBioRead(SslBio *bio, void *outBuf, int outLen);
static int sslBufBioPuts(SslBio *bio, const char *str);
static int sslBufBioGets(SslBio *bio, char *outBuf, int maxLen);
static int sslBufBioCtrl(SslBio *bio, int cmd, int num, void *ptr);
static int sslBufBioNew(SslBio *bio);
static int sslBufBioFree(SslBio *bio);

static int sslMemBioWrite(SslBio *bio, const void *inBuf, int inLen);
static int sslMemBioRead(SslBio *bio, void *outBuf, int outLen);
static int sslMemBioPuts(SslBio *bio, const char *str);
static int sslMemBioGets(SslBio *bio, char *outBuf, int maxLen);
static int sslMemBioCtrl(SslBio *bio, int cmd, int num, void *ptr);
static int sslMemBioNew(SslBio *bio);
static int sslMemBioFree(SslBio *bio);

static int sslSockBioWrite(SslBio *bio, const void *inBuf, int inLen);
static int sslSockBioRead(SslBio *bio, void *outBuf, int outLen);
static int sslSockBioPuts(SslBio *bio, const char *str);
static int sslSockBioCtrl(SslBio *bio, int cmd, int num, void *ptr);
static int sslSockBioNew(SslBio *bio);
static int sslSockBioFree(SslBio *bio);

static const SslBioMethod g_bioBufferMethod = {
    0x209,
    "buffer",
    sslBufBioWrite,
    sslBufBioRead,
    sslBufBioPuts,
    sslBufBioGets,
    sslBufBioCtrl,
    sslBufBioNew,
    sslBufBioFree
};

static const SslBioMethod g_bioMemMethod = {
    0x401,
    "memory buffer",
    sslMemBioWrite,
    sslMemBioRead,
    sslMemBioPuts,
    sslMemBioGets,
    sslMemBioCtrl,
    sslMemBioNew,
    sslMemBioFree
};

static const SslBioMethod g_bioSockMethod = {
    0x505,
    "socket",
    sslSockBioWrite,
    sslSockBioRead,
    sslSockBioPuts,
    NULL,
    sslSockBioCtrl,
    sslSockBioNew,
    sslSockBioFree
};

/* -------------------------------------------------------------------------
 * BIO_printf & buffered BIO filter (bf_buff.c) [231..239]
 * ------------------------------------------------------------------------- */

/* [231] 0x011318 */
LIBSSL_NOINLINE int sceSsl_lib_3454E622(SslBio *bio, const char *fmt, ...)
{
    va_list args;
    int len;

    va_start(args, fmt);
    g_bioPrintfBuf[0] = '\0';
    sslVsprintf(g_bioPrintfBuf, fmt, &args);
    va_end(args);

    len = sslStrlen(g_bioPrintfBuf);
    return sceSsl_lib_CED07EAE(bio, g_bioPrintfBuf, len);
}

/* [232] 0x01138c */
LIBSSL_NOINLINE const SslBioMethod *sslBioFBuffer(void)
{
    return &g_bioBufferMethod;
}

/* [233] 0x011398 */
LIBSSL_NOINLINE static int sslBufBioNew(SslBio *bio)
{
    SslBioBufferCtx *ctx = (SslBioBufferCtx *)sceSsl_lib_0861D934(sizeof(SslBioBufferCtx));
    if (ctx == NULL) {
        return 0;
    }

    ctx->ibuf = (u8 *)sceSsl_lib_0861D934(1024);
    if (ctx->ibuf == NULL) {
        sceSsl_lib_5E5C873A(ctx);
        return 0;
    }

    ctx->obuf = (u8 *)sceSsl_lib_0861D934(1024);
    if (ctx->obuf == NULL) {
        sceSsl_lib_5E5C873A(ctx->ibuf);
        sceSsl_lib_5E5C873A(ctx);
        return 0;
    }

    ctx->ibufSize = 1024;
    ctx->obufSize = 1024;
    ctx->ibufLen = 0;
    ctx->ibufOff = 0;
    ctx->obufLen = 0;
    ctx->obufOff = 0;
    bio->ptr = ctx;
    bio->init = 1;
    bio->flags = 0;
    return 1;
}

/* [234] 0x01144c */
LIBSSL_NOINLINE static int sslBufBioFree(SslBio *bio)
{
    SslBioBufferCtx *ctx;

    if (bio == NULL) {
        return 0;
    }
    ctx = (SslBioBufferCtx *)bio->ptr;
    if (ctx->ibuf != NULL) {
        sceSsl_lib_5E5C873A(ctx->ibuf);
    }
    if (ctx->obuf != NULL) {
        sceSsl_lib_5E5C873A(ctx->obuf);
    }
    sceSsl_lib_5E5C873A(ctx);
    bio->ptr = NULL;
    bio->init = 0;
    bio->flags = 0;
    return 1;
}

/* [235] 0x0114c0 */
LIBSSL_NOINLINE static int sslBufBioRead(SslBio *bio, void *outBuf, int outLen)
{
    u8 *dst = (u8 *)outBuf;
    SslBioBufferCtx *ctx;
    int totalRead = 0;

    if (outBuf == NULL) {
        return 0;
    }
    ctx = (SslBioBufferCtx *)bio->ptr;
    if (ctx == NULL || bio->nextBio == NULL) {
        return 0;
    }

    sceSsl_lib_88A96ED0(bio);
    for (;;) {
        if (ctx->ibufLen != 0) {
            int copyLen = (ctx->ibufLen < outLen) ? ctx->ibufLen : outLen;
            sslMemcpy(dst, ctx->ibuf + ctx->ibufOff, (u32)copyLen);
            ctx->ibufOff += copyLen;
            ctx->ibufLen -= copyLen;
            totalRead += copyLen;
            dst += copyLen;
            if (outLen == copyLen) {
                return totalRead;
            }
            outLen -= copyLen;
        }

        if (outLen > ctx->ibufSize) {
            for (;;) {
                int got = sceSsl_lib_675AC5AA(bio->nextBio, dst, outLen);
                if (got <= 0) {
                    sceSsl_lib_624CB314(bio);
                    if (got < 0 && totalRead == 0) {
                        return got;
                    }
                    return totalRead;
                }
                totalRead += got;
                dst += got;
                if (outLen == got) {
                    return totalRead;
                }
                outLen -= got;
            }
        }

        {
            int got = sceSsl_lib_675AC5AA(bio->nextBio, ctx->ibuf, ctx->ibufSize);
            if (got <= 0) {
                sceSsl_lib_624CB314(bio);
                if (got < 0 && totalRead == 0) {
                    return got;
                }
                return totalRead;
            }
            ctx->ibufLen = got;
            ctx->ibufOff = 0;
        }
    }
}

/* [236] 0x01163c */
LIBSSL_NOINLINE static int sslBufBioWrite(SslBio *bio, const void *inBuf, int inLen)
{
    const u8 *src = (const u8 *)inBuf;
    SslBioBufferCtx *ctx;
    int totalWritten = 0;

    if (inBuf == NULL || inLen <= 0) {
        return 0;
    }
    ctx = (SslBioBufferCtx *)bio->ptr;
    if (ctx == NULL || bio->nextBio == NULL) {
        return 0;
    }

    sceSsl_lib_88A96ED0(bio);
    for (;;) {
        int space = ctx->obufSize - (ctx->obufLen + ctx->obufOff);
        if (space >= inLen) {
            sslMemcpy(ctx->obuf + ctx->obufOff + ctx->obufLen, src, (u32)inLen);
            ctx->obufLen += inLen;
            return totalWritten + inLen;
        }

        if (ctx->obufLen != 0) {
            if (space > 0) {
                sslMemcpy(ctx->obuf + ctx->obufOff + ctx->obufLen, src, (u32)space);
                src += space;
                inLen -= space;
                ctx->obufLen += space;
                totalWritten += space;
            }

            while (ctx->obufLen != 0) {
                int sent = sceSsl_lib_CED07EAE(bio->nextBio, ctx->obuf + ctx->obufOff, ctx->obufLen);
                if (sent <= 0) {
                    sceSsl_lib_624CB314(bio);
                    if (sent < 0 && totalWritten == 0) {
                        return sent;
                    }
                    return totalWritten;
                }
                ctx->obufOff += sent;
                ctx->obufLen -= sent;
            }
        }

        ctx->obufOff = 0;
        while (inLen >= ctx->obufSize) {
            int sent = sceSsl_lib_CED07EAE(bio->nextBio, src, inLen);
            if (sent <= 0) {
                sceSsl_lib_624CB314(bio);
                if (sent < 0 && totalWritten == 0) {
                    return sent;
                }
                return totalWritten;
            }
            totalWritten += sent;
            src += sent;
            inLen -= sent;
            if (inLen == 0) {
                return totalWritten;
            }
        }
    }
}

/* [237] 0x01181c */
LIBSSL_NOINLINE static int sslBufBioCtrl(SslBio *bio, int cmd, int num, void *ptr)
{
    SslBioBufferCtx *ctx = (SslBioBufferCtx *)bio->ptr;

    switch (cmd) {
    case 1:
        ctx->ibufLen = 0;
        ctx->ibufOff = 0;
        ctx->obufLen = 0;
        ctx->obufOff = 0;
        return sceSsl_lib_9566709F(bio->nextBio, cmd, num, ptr);

    case 3:
        return ctx->obufLen;

    case 10:
        if (ctx->ibufLen != 0) {
            return ctx->ibufLen;
        }
        return sceSsl_lib_9566709F(bio->nextBio, cmd, num, ptr);

    case 11:
        if (ctx->obufLen > 0) {
            for (;;) {
                sceSsl_lib_88A96ED0(bio);
                if (ctx->obufOff >= ctx->obufLen) {
                    break;
                }
                {
                    int sent = sceSsl_lib_CED07EAE(bio->nextBio, ctx->obuf + ctx->obufOff,
                                                   ctx->obufLen - ctx->obufOff);
                    sceSsl_lib_624CB314(bio);
                    if (sent <= 0) {
                        return sent;
                    }
                    ctx->obufOff += sent;
                }
            }
            ctx->obufOff = 0;
            ctx->obufLen = 0;
        }
        return sceSsl_lib_9566709F(bio->nextBio, cmd, num, ptr);

    case 12:
        if (sslBioIntCtrl((SslBio *)ptr, 117, ctx->ibufSize, 0) == 0) {
            return 0;
        }
        return (sslBioIntCtrl((SslBio *)ptr, 117, ctx->obufSize, 1) != 0);

    case 13:
        if (ctx->obufLen != 0) {
            return ctx->obufLen;
        }
        return sceSsl_lib_9566709F(bio->nextBio, cmd, num, ptr);

    case 101: {
        int res;
        sceSsl_lib_88A96ED0(bio);
        res = sceSsl_lib_9566709F(bio->nextBio, 101, num, ptr);
        sceSsl_lib_624CB314(bio);
        return res;
    }

    case 116: {
        int lines = 0;
        int pos;
        for (pos = ctx->ibufOff; pos < ctx->ibufLen; pos++) {
            if (ctx->ibuf[pos] == '\n') {
                lines++;
            }
        }
        return lines;
    }

    case 117: {
        int newIsize = num;
        int newOsize = num;
        u8 *newIbuf = ctx->ibuf;
        u8 *newObuf = ctx->obuf;

        if (ptr != NULL) {
            if (*(int *)ptr != 0) {
                newIsize = ctx->ibufSize;
            } else {
                newOsize = ctx->obufSize;
            }
        }

        if (newIsize > 1024 && newIsize != ctx->ibufSize) {
            newIbuf = (u8 *)sceSsl_lib_0861D934((u32)num);
            if (newIbuf == NULL) {
                sceSsl_lib_9CDCA0D7(32, 114, 33, NULL, 0);
                return 0;
            }
        }
        if (newOsize > 1024 && newOsize != ctx->obufSize) {
            newObuf = (u8 *)sceSsl_lib_0861D934((u32)num);
            if (newObuf == NULL) {
                if (newIbuf != ctx->ibuf) {
                    sceSsl_lib_5E5C873A(newIbuf);
                }
                sceSsl_lib_9CDCA0D7(32, 114, 33, NULL, 0);
                return 0;
            }
        }

        if (ctx->ibuf != newIbuf) {
            sceSsl_lib_5E5C873A(ctx->ibuf);
            ctx->ibuf = newIbuf;
            ctx->ibufSize = newIsize;
            ctx->ibufOff = 0;
            ctx->ibufLen = 0;
        }
        if (ctx->obuf != newObuf) {
            sceSsl_lib_5E5C873A(ctx->obuf);
            ctx->obuf = newObuf;
            ctx->obufSize = newOsize;
            ctx->obufOff = 0;
            ctx->obufLen = 0;
        }
        return 1;
    }

    case 122:
        if (num > ctx->ibufSize) {
            u8 *fresh = (u8 *)sceSsl_lib_0861D934((u32)num);
            if (fresh == NULL) {
                sceSsl_lib_9CDCA0D7(32, 114, 33, NULL, 0);
                return 0;
            }
            if (ctx->ibuf != NULL) {
                sceSsl_lib_5E5C873A(ctx->ibuf);
            }
            ctx->ibuf = fresh;
        }
        ctx->ibufOff = 0;
        ctx->ibufLen = num;
        sslMemcpy(ctx->ibuf, ptr, (u32)num);
        return 1;

    default:
        return sceSsl_lib_9566709F(bio->nextBio, cmd, num, ptr);
    }
}

/* [238] 0x011bd0 */
LIBSSL_NOINLINE static int sslBufBioGets(SslBio *bio, char *outBuf, int maxLen)
{
    SslBioBufferCtx *ctx = (SslBioBufferCtx *)bio->ptr;
    int remain = maxLen - 1;
    int total = 0;

    sceSsl_lib_88A96ED0(bio);
    for (;;) {
        while (ctx->ibufLen > 0) {
            int foundNl = 0;
            int taken = 0;
            const u8 *src = ctx->ibuf + ctx->ibufOff;

            while (taken < ctx->ibufLen && taken < remain) {
                u8 ch = src[taken++];
                *outBuf++ = (char)ch;
                if (ch == '\n') {
                    foundNl = 1;
                    break;
                }
            }

            remain -= taken;
            ctx->ibufOff += taken;
            ctx->ibufLen -= taken;
            total += taken;
            if (foundNl || remain == 0) {
                *outBuf = '\0';
                return total;
            }
        }

        {
            int got = sceSsl_lib_675AC5AA(bio->nextBio, ctx->ibuf, ctx->ibufSize);
            if (got <= 0) {
                sceSsl_lib_624CB314(bio);
                if (got < 0 && total == 0) {
                    return got;
                }
                return total;
            }
            ctx->ibufLen = got;
            ctx->ibufOff = 0;
        }
    }
}

/* [239] 0x011d14 */
LIBSSL_NOINLINE static int sslBufBioPuts(SslBio *bio, const char *str)
{
    return sceSsl_lib_CED07EAE(bio, str, sslStrlen(str));
}

/* -------------------------------------------------------------------------
 * BIO flag & callback helpers [240..256]
 * ------------------------------------------------------------------------- */

/* [240] 0x011d58 */
LIBSSL_NOINLINE int sslBioSetFlags(SslBio *bio, int flags)
{
    bio->flags |= flags;
    return bio->flags;
}

/* [241] 0x011d68 */
LIBSSL_NOINLINE int sceSsl_lib_EE624680(SslBio *bio)
{
    bio->flags |= 0x0c;
    return bio->flags;
}

/* [242] 0x011d78 */
LIBSSL_NOINLINE int sceSsl_lib_8A2F6963(SslBio *bio)
{
    bio->flags |= 0x09;
    return bio->flags;
}

/* [243] 0x011d88 */
LIBSSL_NOINLINE int sceSsl_lib_067173A8(SslBio *bio)
{
    bio->flags |= 0x0a;
    return bio->flags;
}

/* [244] 0x011d98 */
LIBSSL_NOINLINE int sceSsl_lib_88A96ED0(SslBio *bio)
{
    bio->flags &= ~0x0f;
    return bio->flags;
}

/* [245] 0x011da8 */
LIBSSL_NOINLINE int sceSsl_lib_CA6A3C74(const SslBio *bio)
{
    return bio->flags & 0x0f;
}

/* [246] 0x011db4 */
LIBSSL_NOINLINE int sceSsl_lib_E3C30923(const SslBio *bio)
{
    return bio->flags & 0x01;
}

/* [247] 0x011dc0 */
LIBSSL_NOINLINE int sceSsl_lib_089FF1F1(const SslBio *bio)
{
    return bio->flags & 0x02;
}

/* [248] 0x011dcc */
LIBSSL_NOINLINE int sceSsl_lib_ABCD28B8(const SslBio *bio)
{
    return bio->flags & 0x04;
}

/* [249] 0x011dd8 */
LIBSSL_NOINLINE int sceSsl_lib_1BCA32E3(const SslBio *bio)
{
    return bio->flags & 0x07;
}

/* [250] 0x011de4 */
LIBSSL_NOINLINE int sceSsl_lib_9052D21A(const SslBio *bio)
{
    return bio->flags & 0x08;
}

/* [251] 0x011df0 */
LIBSSL_NOINLINE void sceSsl_lib_26A7CF72(SslBio *bio, SslBioCb cb)
{
    bio->callback = cb;
}

/* [252] 0x011df8 */
LIBSSL_NOINLINE void sceSsl_lib_ECE07B61(SslBio *bio, char *cbArg)
{
    bio->cbArg = cbArg;
}

/* [253] 0x011e00 */
LIBSSL_NOINLINE char *sceSsl_lib_EC4A18D7(const SslBio *bio)
{
    return bio->cbArg;
}

/* [254] 0x011e08 */
LIBSSL_NOINLINE SslBioCb sceSsl_lib_558CAA41(const SslBio *bio)
{
    return bio->callback;
}

/* [255] 0x011e10 */
LIBSSL_NOINLINE const char *sceSsl_lib_0FD2F220(const SslBio *bio)
{
    return bio->method->name;
}

/* [256] 0x011e1c */
LIBSSL_NOINLINE int sceSsl_lib_19A0DB42(const SslBio *bio)
{
    return bio->method->type;
}

/* -------------------------------------------------------------------------
 * Memory BIO (bss_mem.c) [257..265]
 * ------------------------------------------------------------------------- */

/* [257] 0x011e28 */
LIBSSL_NOINLINE const SslBioMethod *sslBioSMem(void)
{
    return &g_bioMemMethod;
}

/* [258] 0x011e34 */
LIBSSL_NOINLINE SslBio *sceSsl_lib_58352068(void)
{
    return sceSsl_lib_5DED429A(sslBioSMem());
}

/* [259] 0x011e58 */
LIBSSL_NOINLINE static int sslMemBioNew(SslBio *bio)
{
    SslBufMem *buf = sslBufMemNew();
    if (buf == NULL) {
        return 0;
    }
    bio->ptr = buf;
    bio->init = 1;
    bio->num = -1;
    bio->shutdown = 1;
    return 1;
}

/* [260] 0x011ea4 */
LIBSSL_NOINLINE static int sslMemBioFree(SslBio *bio)
{
    if (bio == NULL) {
        return 0;
    }
    if (bio->shutdown != 0 && bio->init != 0 && bio->ptr != NULL) {
        sslBufMemFree((SslBufMem *)bio->ptr);
        bio->ptr = NULL;
    }
    return 1;
}

/* [261] 0x011f00 */
LIBSSL_NOINLINE static int sslMemBioRead(SslBio *bio, void *outBuf, int outLen)
{
    SslBufMem *buf = (SslBufMem *)bio->ptr;
    int copyLen;

    sceSsl_lib_88A96ED0(bio);
    copyLen = (outLen < buf->length) ? outLen : buf->length;
    if (outBuf == NULL || copyLen <= 0) {
        if (buf->length == 0) {
            if (bio->num != 0) {
                sceSsl_lib_8A2F6963(bio);
            }
            return bio->num;
        }
        return copyLen;
    }

    sslMemcpy(outBuf, buf->data, (u32)copyLen);
    buf->length -= copyLen;
    if (buf->length > 0) {
        int idx;
        for (idx = 0; idx < buf->length; idx++) {
            buf->data[idx] = buf->data[copyLen + idx];
        }
    }
    return copyLen;
}

/* [262] 0x011fe0 */
LIBSSL_NOINLINE static int sslMemBioWrite(SslBio *bio, const void *inBuf, int inLen)
{
    SslBufMem *buf = (SslBufMem *)bio->ptr;
    int prevLen;
    int nextLen;

    if (inBuf == NULL) {
        sceSsl_lib_9CDCA0D7(32, 117, 115, NULL, 0);
        return -1;
    }

    sceSsl_lib_88A96ED0(bio);
    prevLen = buf->length;
    nextLen = prevLen + inLen;
    if (sslBufMemGrow(buf, nextLen) != nextLen) {
        return -1;
    }
    sslMemcpy(buf->data + prevLen, inBuf, (u32)inLen);
    return inLen;
}

/* [263] 0x012094 */
LIBSSL_NOINLINE static int sslMemBioCtrl(SslBio *bio, int cmd, int num, void *ptr)
{
    SslBufMem *buf = (SslBufMem *)bio->ptr;

    switch (cmd) {
    case 1:
        if (buf->data != NULL) {
            sslMemset(buf->data, 0, (u32)buf->maxLen);
        }
        buf->length = 0;
        return 1;
    case 2:
        return (buf->length == 0);
    case 3:
        if (ptr != NULL) {
            *(u8 **)ptr = buf->data;
        }
        return buf->length;
    case 8:
        return bio->shutdown;
    case 9:
        bio->shutdown = num & 1;
        return 1;
    case 10:
        return buf->length;
    case 11:
    case 12:
        return 1;
    case 114:
        sslMemBioFree(bio);
        bio->shutdown = num & 1;
        bio->ptr = ptr;
        return 1;
    case 115:
        if (ptr != NULL) {
            *(SslBufMem **)ptr = buf;
        }
        return 1;
    case 130:
        bio->num = num;
        return 1;
    default:
        return 0;
    }
}

/* [264] 0x0121a4 */
LIBSSL_NOINLINE static int sslMemBioGets(SslBio *bio, char *outBuf, int maxLen)
{
    SslBufMem *buf = (SslBufMem *)bio->ptr;
    int lineLen;
    int reqLen;
    int got;

    sceSsl_lib_88A96ED0(bio);
    if (buf->length <= 0) {
        return 0;
    }

    lineLen = 0;
    while (lineLen < buf->length) {
        if (buf->data[lineLen] == '\n') {
            break;
        }
        lineLen++;
    }

    if (lineLen == buf->length) {
        sceSsl_lib_8A2F6963(bio);
    } else {
        lineLen++;
    }

    reqLen = (lineLen < maxLen - 1) ? lineLen : (maxLen - 1);
    got = sslMemBioRead(bio, outBuf, reqLen);
    if (got > 0) {
        outBuf[got] = '\0';
    }
    return got;
}

/* [265] 0x012268 */
LIBSSL_NOINLINE static int sslMemBioPuts(SslBio *bio, const char *str)
{
    return sslMemBioWrite(bio, str, sslStrlen(str));
}

/* -------------------------------------------------------------------------
 * Socket BIO (bss_sock.c) [266..273]
 * ------------------------------------------------------------------------- */

/* [266] 0x0122ac */
LIBSSL_NOINLINE const SslBioMethod *sslBioSSocket(void)
{
    return &g_bioSockMethod;
}

/* [267] 0x0122b8 */
LIBSSL_NOINLINE SslBio *sceSsl_lib_28B4DE33(int sockFd, int closeFlag)
{
    SslBio *bio = sceSsl_lib_5DED429A(sslBioSSocket());
    if (bio == NULL) {
        return NULL;
    }
    sslBioIntCtrl(bio, 104, closeFlag, sockFd);
    return bio;
}

/* [268] 0x012320 */
LIBSSL_NOINLINE static int sslSockBioNew(SslBio *bio)
{
    bio->flags = 0;
    bio->init = 0;
    bio->num = 0;
    bio->ptr = NULL;
    return 1;
}

/* [269] 0x012338 */
LIBSSL_NOINLINE static int sslSockBioFree(SslBio *bio)
{
    if (bio == NULL) {
        return 0;
    }
    if (bio->shutdown != 0) {
        if (bio->init != 0) {
            sceNetInetShutdown(bio->num, 2);
        }
        bio->flags = 0;
        bio->init = 0;
    }
    return 1;
}

/* [270] 0x01238c */
LIBSSL_NOINLINE static int sslSockBioRead(SslBio *bio, void *outBuf, int outLen)
{
    int ret = 0;
    if (outBuf != NULL) {
        ret = sceNetInetRecv(bio->num, outBuf, (u32)outLen, 0);
        sceSsl_lib_88A96ED0(bio);
    }
    return ret;
}

/* [271] 0x0123d8 */
LIBSSL_NOINLINE static int sslSockBioWrite(SslBio *bio, const void *inBuf, int inLen)
{
    int ret = sceNetInetSend(bio->num, inBuf, (u32)inLen, 0);
    sceSsl_lib_88A96ED0(bio);
    return ret;
}

/* [272] 0x01241c */
LIBSSL_NOINLINE static int sslSockBioCtrl(SslBio *bio, int cmd, int num, void *ptr)
{
    switch (cmd) {
    case 1:
    case 11:
    case 12:
        return 1;
    case 8:
        return bio->shutdown;
    case 9:
        bio->shutdown = num & 1;
        return 1;
    case 13:
        return 0;
    case 104:
        sslSockBioFree(bio);
        bio->num = *(int *)ptr;
        bio->shutdown = num & 1;
        bio->init = 1;
        return 1;
    case 105:
        if (bio->init == 0) {
            return -1;
        }
        if (ptr != NULL) {
            *(int *)ptr = bio->num;
        }
        return bio->num;
    case 128:
        return 1;
    default:
        return 0;
    }
}

/* [273] 0x012544 */
LIBSSL_NOINLINE static int sslSockBioPuts(SslBio *bio, const char *str)
{
    return sslSockBioWrite(bio, str, sslStrlen(str));
}

/* -------------------------------------------------------------------------
 * Core BIO framework (bio_lib.c) [404..417]
 * ------------------------------------------------------------------------- */

/* [404] 0x0185ac */
LIBSSL_NOINLINE SslBio *sceSsl_lib_5DED429A(const SslBioMethod *method)
{
    SslBio *bio = (SslBio *)sceSsl_lib_0861D934(sizeof(SslBio));
    if (bio == NULL) {
        sceSsl_lib_9CDCA0D7(32, 108, 33, NULL, 0);
        return NULL;
    }
    if (!sslBioSet(bio, method)) {
        sceSsl_lib_5E5C873A(bio);
        return NULL;
    }
    return bio;
}

/* [405] 0x018628 */
LIBSSL_NOINLINE int sslBioSet(SslBio *bio, const SslBioMethod *method)
{
    bio->method = method;
    bio->callback = NULL;
    bio->cbArg = NULL;
    bio->init = 0;
    bio->shutdown = 1;
    bio->flags = 0;
    bio->retryReason = 0;
    bio->num = 0;
    bio->ptr = NULL;
    bio->nextBio = NULL;
    bio->prevBio = NULL;
    bio->references = 1;
    bio->numRead = 0;
    bio->numWrite = 0;

    sslCryptoNewExData(g_bioExDataFuncs, bio, &bio->exData);
    if (method->create != NULL && !method->create(bio)) {
        return 0;
    }
    return 1;
}

/* [406] 0x0186c8 */
LIBSSL_NOINLINE int sceSsl_lib_3E040FD4(SslBio *bio)
{
    if (bio == NULL) {
        return 0;
    }
    if (sslCryptoAddLock(&bio->references, -1, 18, "crypto/bio/bio_lib.c", 99) > 0) {
        return 1;
    }
    if (bio->callback != NULL) {
        int cbRet = bio->callback(bio, 1, NULL, 0, 0, 1);
        if (cbRet <= 0) {
            return cbRet;
        }
    }
    sslCryptoFreeExData(g_bioExDataFuncs, bio, &bio->exData);
    if (bio->method != NULL && bio->method->destroy != NULL) {
        bio->method->destroy(bio);
        sceSsl_lib_5E5C873A(bio);
    }
    return 1;
}

/* [407] 0x018788 */
LIBSSL_NOINLINE int sceSsl_lib_675AC5AA(SslBio *bio, void *outBuf, int outLen)
{
    SslBioCb cb;
    int ret;

    if (bio == NULL || bio->method == NULL || bio->method->bread == NULL) {
        sceSsl_lib_9CDCA0D7(32, 111, 121, NULL, 0);
        return -2;
    }
    cb = bio->callback;
    if (cb != NULL) {
        ret = cb(bio, 2, (const char *)outBuf, outLen, 0, 1);
        if (ret <= 0) {
            return ret;
        }
    }
    if (bio->init == 0) {
        sceSsl_lib_9CDCA0D7(32, 111, 120, NULL, 0);
        return -2;
    }

    ret = bio->method->bread(bio, outBuf, outLen);
    if (ret > 0) {
        bio->numRead += (u32)ret;
    }
    if (cb != NULL) {
        ret = cb(bio, 130, (const char *)outBuf, outLen, 0, ret);
    }
    return ret;
}

/* [408] 0x018890 */
LIBSSL_NOINLINE int sceSsl_lib_CED07EAE(SslBio *bio, const void *inBuf, int inLen)
{
    SslBioCb cb;
    int ret;

    if (bio == NULL) {
        return 0;
    }
    cb = bio->callback;
    if (bio->method == NULL || bio->method->bwrite == NULL) {
        sceSsl_lib_9CDCA0D7(32, 113, 121, NULL, 0);
        return -2;
    }
    if (cb != NULL) {
        ret = cb(bio, 3, (const char *)inBuf, inLen, 0, 1);
        if (ret <= 0) {
            return ret;
        }
    }
    if (bio->init == 0) {
        sceSsl_lib_9CDCA0D7(32, 113, 120, NULL, 0);
        return -2;
    }

    ret = bio->method->bwrite(bio, inBuf, inLen);
    if (ret > 0) {
        bio->numWrite += (u32)ret;
    }
    if (bio->references <= 0) {
        sceSsl_lib_9CDCA0D7(32, 113, 42, NULL, 0);
        return 0;
    }
    if (cb != NULL) {
        ret = cb(bio, 131, (const char *)inBuf, inLen, 0, ret);
    }
    return ret;
}

/* [409] 0x0189c4 */
LIBSSL_NOINLINE int sceSsl_lib_AFD1D124(SslBio *bio, char *outBuf, int maxLen)
{
    SslBioCb cb;
    int ret;

    if (bio == NULL || bio->method == NULL || bio->method->bgets == NULL) {
        sceSsl_lib_9CDCA0D7(32, 104, 121, NULL, 0);
        return -2;
    }
    cb = bio->callback;
    if (cb != NULL) {
        ret = cb(bio, 5, outBuf, maxLen, 0, 1);
        if (ret <= 0) {
            return ret;
        }
    }
    if (bio->init == 0) {
        sceSsl_lib_9CDCA0D7(32, 104, 120, NULL, 0);
        return -2;
    }

    ret = bio->method->bgets(bio, outBuf, maxLen);
    if (cb != NULL) {
        ret = cb(bio, 133, outBuf, maxLen, 0, ret);
    }
    return ret;
}

/* [410] 0x018abc */
LIBSSL_NOINLINE int sslBioIntCtrl(SslBio *bio, int cmd, int num, int intVal)
{
    int localVal = intVal;
    return sceSsl_lib_9566709F(bio, cmd, num, &localVal);
}

/* [411] 0x018ae0 */
LIBSSL_NOINLINE int sceSsl_lib_9566709F(SslBio *bio, int cmd, int num, void *ptr)
{
    SslBioCb cb;
    int ret;

    if (bio == NULL) {
        return 0;
    }
    if (bio->method == NULL || bio->method->ctrl == NULL) {
        sceSsl_lib_9CDCA0D7(32, 103, 121, NULL, 0);
        return -2;
    }

    cb = bio->callback;
    if (cb != NULL) {
        ret = cb(bio, 6, (const char *)ptr, cmd, num, 1);
        if (ret <= 0) {
            return ret;
        }
    }

    ret = bio->method->ctrl(bio, cmd, num, ptr);
    if (cb != NULL) {
        ret = cb(bio, 134, (const char *)ptr, cmd, num, ret);
    }
    return ret;
}

/* [412] 0x018bd8 */
LIBSSL_NOINLINE SslBio *sceSsl_lib_1EC64594(SslBio *bio, SslBio *appendBio)
{
    SslBio *tail;

    if (bio == NULL) {
        return appendBio;
    }
    tail = bio;
    while (tail->nextBio != NULL) {
        tail = tail->nextBio;
    }
    tail->nextBio = appendBio;
    if (appendBio != NULL) {
        appendBio->prevBio = tail;
    }
    sceSsl_lib_9566709F(bio, 6, 0, NULL);
    return bio;
}

/* [413] 0x018c40 */
LIBSSL_NOINLINE SslBio *sceSsl_lib_732F4E44(SslBio *bio)
{
    SslBio *next;

    if (bio == NULL) {
        return NULL;
    }
    next = bio->nextBio;
    if (bio->prevBio != NULL) {
        bio->prevBio->nextBio = next;
    }
    if (next != NULL) {
        next->prevBio = bio->prevBio;
    }
    bio->prevBio = NULL;
    bio->nextBio = NULL;
    sceSsl_lib_9566709F(bio, 7, 0, NULL);
    return next;
}

/* [414] 0x018ca0 */
LIBSSL_NOINLINE int sceSsl_lib_4A6078CF(const SslBio *bio)
{
    return bio->retryReason;
}

/* [415] 0x018ca8 */
LIBSSL_NOINLINE void sceSsl_lib_959557F5(SslBio *bio)
{
    while (bio != NULL) {
        int refCount = bio->references;
        SslBio *next = bio->nextBio;
        sceSsl_lib_3E040FD4(bio);
        if (refCount > 1) {
            break;
        }
        bio = next;
    }
}

/* [416] 0x018cf8 */
LIBSSL_NOINLINE void sceSsl_lib_624CB314(SslBio *bio)
{
    int flags = sceSsl_lib_CA6A3C74(bio->nextBio);
    sslBioSetFlags(bio, flags);
    bio->retryReason = bio->nextBio->retryReason;
}

/* [417] 0x018d38 */
LIBSSL_NOINLINE int sslBioInvokeCertPrinter(SslBio *bio, const SslAsn1Item *certItem,
                                            int (*printFn)(SslBio *, const void *, u32))
{
    if (printFn == NULL) {
        return 0;
    }
    return printFn(bio, certItem->data, certItem->length);
}
