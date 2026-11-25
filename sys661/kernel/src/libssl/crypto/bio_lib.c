/*
 * SSL-C BIO (Basic I/O), ERR, RAND, and RSA helper exports.
 *
 * Corresponds to Sony's crypto/bio/bio_lib.c, crypto/err/err.c,
 * crypto/rand/md_rand.c, and crypto/rsa/rsa_lib.c in libssl.prx.
 */
#include "libssl_internal.h"

/* --- Error queue (crypto/err/err.c) --- */

void sslErrPush(int lib, int func, int reason)
{
    u32 code = (((u32)lib & 0xFFU) << 24) | (((u32)func & 0xFFFU) << 12) | ((u32)reason & 0xFFFU);
    int nextTop = (g_ssl.errState.top + 1) % SSL_ERR_QUEUE_SIZE;

    if (nextTop == g_ssl.errState.bottom) {
        g_ssl.errState.bottom = (g_ssl.errState.bottom + 1) % SSL_ERR_QUEUE_SIZE;
    }
    g_ssl.errState.errors[g_ssl.errState.top] = code;
    g_ssl.errState.top = nextTop;
}

static u32 sslErrPop(int peekOnly)
{
    u32 code;

    if (g_ssl.errState.top == g_ssl.errState.bottom) {
        return 0;
    }
    code = g_ssl.errState.errors[g_ssl.errState.bottom];
    if (!peekOnly) {
        g_ssl.errState.bottom = (g_ssl.errState.bottom + 1) % SSL_ERR_QUEUE_SIZE;
    }
    return code;
}

/* ERR_get_error / ERR_peek_error / ERR_clear_error / ERR_put_error */
u32 sceSsl_lib_4AE72675(void)
{
    return sslErrPop(0);
}

u32 sceSsl_lib_56E19CEB(void)
{
    return sslErrPop(1);
}

void *sceSsl_lib_DEA6EDB2(int errCode, int flags)
{
    (void)errCode;
    (void)flags;
    return &g_ssl.errState;
}

int sceSsl_lib_21F2A35C(int errCode, int flags)
{
    (void)flags;
    if (errCode == 0) {
        g_ssl.errState.top = 0;
        g_ssl.errState.bottom = 0;
    }
    return 0;
}

void *sceSsl_lib_1C2728A5(int flags)
{
    (void)flags;
    return &g_ssl.errState;
}

/* RAND_SSLeay (crypto/rand/md_rand.c) */
static int sslRandBytes(u8 *buf, int num)
{
    return sceSsl_lib_C85F6FCE(0, buf, (u32)num);
}

static int (*const g_randMethodTable[2])(u8 *, int) = {
    sslRandBytes,
    sslRandBytes
};

const void *sceSsl_lib_A1C3AB31(void)
{
    return (const void *)g_randMethodTable;
}

/* RSA helpers (crypto/rsa/rsa_lib.c) */
void sceSsl_lib_A87C65C7(SslRsaKey *rsa, int flags, int extra)
{
    (void)rsa;
    (void)flags;
    (void)extra;
}

int sceSsl_lib_72D77445(const SslRsaKey *rsa)
{
    if (rsa == NULL) {
        return 0;
    }
    return (int)rsa->modulusLen;
}

/* --- Socket BIO method --- */

static int sslSockBioWrite(SslBio *bio, const char *buf, int len)
{
    int ret;

    if (bio == NULL || buf == NULL || len <= 0) {
        return 0;
    }
    ret = sceNetInetSend(bio->num, buf, (u32)len, 0);
    if (ret > 0) {
        bio->numWrite += (u32)ret;
    }
    return ret;
}

static int sslSockBioRead(SslBio *bio, char *buf, int len)
{
    int ret;

    if (bio == NULL || buf == NULL || len <= 0) {
        return 0;
    }
    ret = sceNetInetRecv(bio->num, buf, (u32)len, 0);
    if (ret > 0) {
        bio->numRead += (u32)ret;
    }
    return ret;
}

static int sslSockBioPuts(SslBio *bio, const char *str)
{
    int len = 0;

    if (str == NULL) {
        return 0;
    }
    while (str[len] != '\0') {
        len++;
    }
    return sslSockBioWrite(bio, str, len);
}

static int sslSockBioCtrl(SslBio *bio, int cmd, int num, void *ptr)
{
    if (bio == NULL) {
        return 0;
    }
    switch (cmd) {
    case BIO_C_SET_FD:
        bio->num = (ptr != NULL) ? *(const int *)ptr : 0;
        bio->shutdown = num;
        bio->init = 1;
        return 1;
    case BIO_C_GET_FD:
        if (!bio->init) {
            return -1;
        }
        if (ptr != NULL) {
            *(int *)ptr = bio->num;
        }
        return bio->num;
    case BIO_CTRL_GET_CLOSE:
        return bio->shutdown;
    case BIO_CTRL_SET_CLOSE:
        bio->shutdown = num;
        return 1;
    case BIO_CTRL_FLUSH:
    case BIO_CTRL_DUP:
        return 1;
    default:
        return 0;
    }
}

static int sslSockBioCreate(SslBio *bio)
{
    bio->init = 0;
    bio->num = -1;
    bio->ptr = NULL;
    bio->flags = 0;
    return 1;
}

static int sslSockBioDestroy(SslBio *bio)
{
    if (bio == NULL) {
        return 0;
    }
    if (bio->shutdown && bio->init && bio->num >= 0) {
        sceNetInetShutdown(bio->num, 2);
        bio->init = 0;
        bio->num = -1;
    }
    return 1;
}

static const SslBioMethod g_socketBioMethod = {
    0x0505,
    "socket",
    sslSockBioWrite,
    sslSockBioRead,
    sslSockBioPuts,
    NULL,
    sslSockBioCtrl,
    sslSockBioCreate,
    sslSockBioDestroy
};

/* --- Memory BIO method --- */

static int sslMemBioWrite(SslBio *bio, const char *buf, int len)
{
    u32 newLen;
    u8 *newBuf;

    if (bio == NULL || buf == NULL || len <= 0) {
        return 0;
    }
    newLen = bio->memLen + (u32)len;
    if (newLen > bio->memCap) {
        u32 cap = (newLen + 255U) & ~255U;
        newBuf = (u8 *)sslPoolRealloc(bio->memBuf, cap);
        if (newBuf == NULL) {
            return -1;
        }
        bio->memBuf = newBuf;
        bio->memCap = cap;
    }
    sceKernelMemcpy(bio->memBuf + bio->memLen, buf, (u32)len);
    bio->memLen = newLen;
    bio->numWrite += (u32)len;
    return len;
}

static int sslMemBioRead(SslBio *bio, char *buf, int len)
{
    u32 avail;
    u32 toRead;

    if (bio == NULL || buf == NULL || len <= 0) {
        return 0;
    }
    if (bio->memPos >= bio->memLen) {
        return 0;
    }
    avail = bio->memLen - bio->memPos;
    toRead = ((u32)len < avail) ? (u32)len : avail;
    sceKernelMemcpy(buf, bio->memBuf + bio->memPos, toRead);
    bio->memPos += toRead;
    bio->numRead += toRead;
    return (int)toRead;
}

static int sslMemBioCtrl(SslBio *bio, int cmd, int num, void *ptr)
{
    if (bio == NULL) {
        return 0;
    }
    switch (cmd) {
    case BIO_CTRL_RESET:
        bio->memPos = 0;
        bio->memLen = 0;
        return 1;
    case BIO_CTRL_EOF:
        return (bio->memPos >= bio->memLen) ? 1 : 0;
    case BIO_CTRL_PENDING:
        return (int)(bio->memLen - bio->memPos);
    case BIO_CTRL_INFO:
        if (ptr != NULL) {
            *(u8 **)ptr = bio->memBuf + bio->memPos;
        }
        return (int)(bio->memLen - bio->memPos);
    case BIO_CTRL_GET_CLOSE:
        return bio->shutdown;
    case BIO_CTRL_SET_CLOSE:
        bio->shutdown = num;
        return 1;
    case BIO_CTRL_FLUSH:
        return 1;
    default:
        return 0;
    }
}

static int sslMemBioCreate(SslBio *bio)
{
    bio->init = 1;
    bio->shutdown = 1;
    bio->memBuf = NULL;
    bio->memLen = 0;
    bio->memPos = 0;
    bio->memCap = 0;
    return 1;
}

static int sslMemBioDestroy(SslBio *bio)
{
    if (bio == NULL) {
        return 0;
    }
    if (bio->shutdown && bio->memBuf != NULL) {
        sceSsl_lib_5E5C873A(bio->memBuf);
        bio->memBuf = NULL;
    }
    return 1;
}

static const SslBioMethod g_memBioMethod = {
    0x0401,
    "memory buffer",
    sslMemBioWrite,
    sslMemBioRead,
    NULL,
    NULL,
    sslMemBioCtrl,
    sslMemBioCreate,
    sslMemBioDestroy
};

/* --- Public BIO exports (crypto/bio/bio_lib.c) --- */

/* BIO_new */
SslBio *sceSsl_lib_5DED429A(const SslBioMethod *method)
{
    SslBio *bio;

    if (method == NULL) {
        return NULL;
    }
    bio = (SslBio *)sceSsl_lib_0861D934(sizeof(SslBio));
    if (bio == NULL) {
        return NULL;
    }
    sceKernelMemset(bio, 0, sizeof(SslBio));
    bio->method = method;
    bio->shutdown = 1;
    bio->refCount = 1;
    if (method->create != NULL && method->create(bio) == 0) {
        sceSsl_lib_5E5C873A(bio);
        return NULL;
    }
    return bio;
}

/* BIO_free */
int sceSsl_lib_3E040FD4(SslBio *bio)
{
    if (bio == NULL) {
        return 0;
    }
    bio->refCount--;
    if (bio->refCount > 0) {
        return 1;
    }
    if (bio->method != NULL && bio->method->destroy != NULL) {
        bio->method->destroy(bio);
    }
    sceSsl_lib_5E5C873A(bio);
    return 1;
}

/* BIO_read */
int sceSsl_lib_675AC5AA(SslBio *bio, void *buf, int len)
{
    if (bio == NULL || bio->method == NULL || bio->method->bread == NULL) {
        return -2;
    }
    if (!bio->init) {
        return -2;
    }
    return bio->method->bread(bio, (char *)buf, len);
}

/* BIO_write */
int sceSsl_lib_CED07EAE(SslBio *bio, const void *buf, int len)
{
    if (bio == NULL || bio->method == NULL || bio->method->bwrite == NULL) {
        return -2;
    }
    if (!bio->init) {
        return -2;
    }
    return bio->method->bwrite(bio, (const char *)buf, len);
}

/* BIO_puts */
int sceSsl_lib_AFD1D124(SslBio *bio, const char *str)
{
    if (bio == NULL || bio->method == NULL) {
        return -2;
    }
    if (bio->method->bputs != NULL) {
        return bio->method->bputs(bio, str);
    }
    if (bio->method->bwrite != NULL && str != NULL) {
        int len = 0;
        while (str[len] != '\0') {
            len++;
        }
        return bio->method->bwrite(bio, str, len);
    }
    return -2;
}

/* BIO_ctrl */
int sceSsl_lib_9566709F(SslBio *bio, int cmd, int larg, void *parg)
{
    if (bio == NULL || bio->method == NULL || bio->method->ctrl == NULL) {
        return -2;
    }
    return bio->method->ctrl(bio, cmd, larg, parg);
}

/* BIO_push */
SslBio *sceSsl_lib_1EC64594(SslBio *top, SslBio *append)
{
    SslBio *cur;

    if (top == NULL) {
        return append;
    }
    cur = top;
    while (cur->nextBio != NULL) {
        cur = cur->nextBio;
    }
    cur->nextBio = append;
    if (append != NULL) {
        append->prevBio = cur;
    }
    sceSsl_lib_9566709F(top, BIO_CTRL_PUSH, 0, cur);
    return top;
}

/* BIO_pop */
SslBio *sceSsl_lib_732F4E44(SslBio *bio)
{
    SslBio *ret;

    if (bio == NULL) {
        return NULL;
    }
    ret = bio->nextBio;
    sceSsl_lib_9566709F(bio, BIO_CTRL_POP, 0, bio);
    if (bio->prevBio != NULL) {
        bio->prevBio->nextBio = bio->nextBio;
    }
    if (bio->nextBio != NULL) {
        bio->nextBio->prevBio = bio->prevBio;
    }
    bio->nextBio = NULL;
    bio->prevBio = NULL;
    return ret;
}

/* BIO_next */
SslBio *sceSsl_lib_4A6078CF(SslBio *bio)
{
    return (bio != NULL) ? bio->nextBio : NULL;
}

/* BIO_free_all */
void sceSsl_lib_959557F5(SslBio *bio)
{
    SslBio *next;

    while (bio != NULL) {
        int refs = bio->refCount;
        next = bio->nextBio;
        sceSsl_lib_3E040FD4(bio);
        if (refs > 1) {
            break;
        }
        bio = next;
    }
}

/* BIO_copy_next_retry */
int sceSsl_lib_624CB314(SslBio *bio)
{
    if (bio == NULL || bio->nextBio == NULL) {
        return 0;
    }
    bio->flags |= (bio->nextBio->flags & 0x0F);
    bio->retryReason = bio->nextBio->retryReason;
    return 1;
}

/* BIO_new_socket */
SslBio *sceSsl_lib_28B4DE33(int sockFd, int closeFlag)
{
    SslBio *bio = sceSsl_lib_5DED429A(&g_socketBioMethod);
    if (bio == NULL) {
        return NULL;
    }
    sceSsl_lib_9566709F(bio, BIO_C_SET_FD, closeFlag, &sockFd);
    return bio;
}

/* BIO_s_mem / BIO_new_mem_buf */
const SslBioMethod *sceSsl_lib_58352068(void)
{
    return &g_memBioMethod;
}

SslBio *sceSsl_lib_1E257243(const void *buf, int len)
{
    SslBio *bio;

    if (buf == NULL) {
        return NULL;
    }
    bio = sceSsl_lib_5DED429A(&g_memBioMethod);
    if (bio == NULL) {
        return NULL;
    }
    if (len < 0) {
        const char *str = (const char *)buf;
        len = 0;
        while (str[len] != '\0') {
            len++;
        }
    }
    if (len > 0) {
        sslMemBioWrite(bio, (const char *)buf, len);
    }
    return bio;
}

/* BIO_set_flags / BIO_test_flags / BIO_clear_flags / BIO_retry helpers */
int sceSsl_lib_EE624680(const SslBio *bio) { return (bio != NULL) ? bio->flags : 0; }
int sceSsl_lib_8A2F6963(const SslBio *bio) { return (bio != NULL) ? bio->retryReason : 0; }
int sceSsl_lib_067173A8(const SslBio *bio) { return (bio != NULL) ? bio->init : 0; }
u32 sceSsl_lib_88A96ED0(const SslBio *bio) { return (bio != NULL) ? bio->numRead : 0; }
int sceSsl_lib_CA6A3C74(const SslBio *bio) { return (bio != NULL) ? bio->num : -1; }
int sceSsl_lib_E3C30923(const SslBio *bio) { return (bio != NULL) ? bio->shutdown : 0; }
int sceSsl_lib_089FF1F1(const SslBio *bio) { return (bio != NULL) ? (int)bio->numWrite : 0; }
int sceSsl_lib_ABCD28B8(const SslBio *bio) { return (bio != NULL && bio->method != NULL) ? bio->method->type : 0; }
int sceSsl_lib_1BCA32E3(const SslBio *bio) { return (bio != NULL) ? bio->refCount : 0; }
int sceSsl_lib_9052D21A(const SslBio *bio) { return (bio != NULL) ? (bio->flags & 0x08) : 0; }
void sceSsl_lib_26A7CF72(SslBio *bio, int flags) { if (bio != NULL) bio->flags |= flags; }
void sceSsl_lib_ECE07B61(SslBio *bio, int flags) { if (bio != NULL) bio->flags &= ~flags; }
int sceSsl_lib_EC4A18D7(const SslBio *bio) { return (bio != NULL) ? (bio->flags & 0x01) : 0; }
int sceSsl_lib_558CAA41(const SslBio *bio) { return (bio != NULL) ? (bio->flags & 0x02) : 0; }
int sceSsl_lib_0FD2F220(const SslBio *bio) { return (bio != NULL) ? (bio->flags & 0x04) : 0; }
int sceSsl_lib_19A0DB42(const SslBio *bio) { return (bio != NULL) ? (bio->flags & 0x0F) : 0; }
