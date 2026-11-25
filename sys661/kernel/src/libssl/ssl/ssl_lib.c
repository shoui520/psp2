/*
 * SSL-C / TLSv1 / SSLv3 connection, context, and session management.
 *
 * Corresponds to Sony's ssl/ssl_lib.c, ssl/sslp_lib.c, ssl/ssl_cert.c,
 * ssl/ssl_sess.c, and ssl/ssl_rsa.c in libssl.prx.
 */
#include "libssl_internal.h"

extern int sceSsl_lib_675AC5AA(SslBio *bio, void *buf, int len);
extern int sceSsl_lib_CED07EAE(SslBio *bio, const void *buf, int len);
extern int sceSsl_lib_3E040FD4(SslBio *bio);
extern void sceSsl_lib_959557F5(SslBio *bio);
extern int sceSsl_lib_C1A2E14B(SslX509Cert *cert);
extern void sceSsl_lib_7317C1BC(SslEvpPkey *pkey);
extern u32 sceSsl_lib_56E19CEB(void);

static const SslCipher g_defaultCipher = {
    1,
    "RC4-SHA",
    0x03000005U,
    0,
    128,
    128
};

/* --- Protocol method callbacks (SSLv23 / TLSv1 client) --- */

static int sslProtoNew(SslConn *ssl)
{
    ssl->state = 0x1000;
    return 1;
}

static void sslProtoClear(SslConn *ssl)
{
    ssl->state = 0x1000;
    ssl->shutdown = 0;
    ssl->rwstate = SSL_NOTHING;
    ssl->inHandshake = 0;
}

static void sslProtoFree(SslConn *ssl)
{
    (void)ssl;
}

static int sslProtoConnect(SslConn *ssl)
{
    if (ssl == NULL || ssl->rbio == NULL || ssl->wbio == NULL) {
        sslErrPush(20, 1, 1);
        return -1;
    }
    ssl->inHandshake = 1;
    ssl->state = 0x0003; /* SSL_ST_OK */
    ssl->inHandshake = 0;
    return 1;
}

static int sslProtoAccept(SslConn *ssl)
{
    return sslProtoConnect(ssl);
}

static int sslProtoRead(SslConn *ssl, void *buf, int len)
{
    if (ssl == NULL || ssl->rbio == NULL) {
        return -1;
    }
    if ((ssl->shutdown & SSL_RECEIVED_SHUTDOWN) != 0) {
        return 0;
    }
    ssl->rwstate = SSL_READING;
    return sceSsl_lib_675AC5AA(ssl->rbio, buf, len);
}

static int sslProtoWrite(SslConn *ssl, const void *buf, int len)
{
    if (ssl == NULL || ssl->wbio == NULL) {
        return -1;
    }
    if ((ssl->shutdown & SSL_SENT_SHUTDOWN) != 0) {
        return -1;
    }
    ssl->rwstate = SSL_WRITING;
    return sceSsl_lib_CED07EAE(ssl->wbio, buf, len);
}

static int sslProtoShutdown(SslConn *ssl)
{
    if (ssl == NULL) {
        return -1;
    }
    ssl->shutdown |= SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN;
    return 1;
}

static int sslProtoPending(const SslConn *ssl)
{
    (void)ssl;
    return 0;
}

static int sslProtoCtrl(SslConn *ssl, int cmd, int larg, void *parg)
{
    (void)ssl;
    (void)cmd;
    (void)larg;
    (void)parg;
    return 1;
}

static int sslProtoCtxCtrl(SslCtx *ctx, int cmd, int larg, void *parg)
{
    if (ctx == NULL) {
        return 0;
    }
    if (cmd == 32) { /* SSL_CTRL_OPTIONS */
        ctx->options |= (u32)larg;
        return (int)ctx->options;
    }
    if (cmd == 33) { /* SSL_CTRL_MODE */
        ctx->mode |= (u32)larg;
        return (int)ctx->mode;
    }
    (void)parg;
    return 1;
}

static const SslMethod g_sslv23ClientMethod = {
    0x0301,
    sslProtoNew,
    sslProtoClear,
    sslProtoFree,
    sslProtoAccept,
    sslProtoConnect,
    sslProtoRead,
    sslProtoRead,
    sslProtoWrite,
    sslProtoShutdown,
    sslProtoConnect,
    sslProtoConnect,
    sslProtoCtrl,
    sslProtoCtxCtrl,
    sslProtoPending
};

static const SslMethod g_tlsv1ClientMethod = {
    0x0301,
    sslProtoNew,
    sslProtoClear,
    sslProtoFree,
    sslProtoAccept,
    sslProtoConnect,
    sslProtoRead,
    sslProtoRead,
    sslProtoWrite,
    sslProtoShutdown,
    sslProtoConnect,
    sslProtoConnect,
    sslProtoCtrl,
    sslProtoCtxCtrl,
    sslProtoPending
};

/* --- SSL library init / cleanup --- */

int sceSsl_lib_9274BFE3(void)
{
    g_ssl.errState.top = 0;
    g_ssl.errState.bottom = 0;
    g_ssl.randSeed = 0x53434553U;
    return 1;
}

int sceSsl_lib_CF78BEFB(void)
{
    g_ssl.errState.top = 0;
    g_ssl.errState.bottom = 0;
    return 1;
}

int sceSsl_lib_BB1564A3(void) { return 1; }

/* Protocol method exports */
const SslMethod *sceSsl_lib_B40D11EA(void) { return &g_sslv23ClientMethod; }
const SslMethod *sceSsl_lib_42FFA860(void) { return &g_tlsv1ClientMethod; }
const SslMethod *sceSsl_lib_101D418A(void) { return &g_sslv23ClientMethod; }
const SslMethod *sceSsl_lib_D8C05497(int ver) { return (ver == 0x0301) ? &g_tlsv1ClientMethod : &g_sslv23ClientMethod; }

int sceSsl_lib_9CDCA0D7(SslConn *ssl, s16 major, s16 minor)
{
    (void)major;
    (void)minor;
    return (ssl != NULL) ? 1 : 0;
}

int sceSsl_lib_04E677AF(void) { return 1; }
int sceSsl_lib_E78AD94D(void) { return 1; }
int sceSsl_lib_47F7B97F(void) { return 1; }

/* --- SSL_SESSION management (ssl/ssl_sess.c) --- */

const char *sceSsl_lib_A2CE8DCD(const SslCipher *cipher)
{
    return (cipher != NULL && cipher->name != NULL) ? cipher->name : "(NONE)";
}

SslSession *sceSsl_lib_EF5A4BD4(const SslConn *ssl)
{
    return (ssl != NULL) ? ssl->session : NULL;
}

SslSession *sceSsl_lib_B81AD643(int version, int flags)
{
    SslSession *sess;

    (void)flags;
    sess = (SslSession *)sceSsl_lib_0861D934(sizeof(SslSession));
    if (sess == NULL) {
        return NULL;
    }
    sceKernelMemset(sess, 0, sizeof(SslSession));
    sess->sslVersion = version;
    sess->refCount = 1;
    sess->timeout = 300;
    sess->cipher = &g_defaultCipher;
    return sess;
}

void sceSsl_lib_BDAD0841(SslSession *sess)
{
    if (sess == NULL) {
        return;
    }
    sess->refCount--;
    if (sess->refCount > 0) {
        return;
    }
    if (sess->peer != NULL) {
        sceSsl_lib_C1A2E14B(sess->peer);
    }
    sceSsl_lib_5E5C873A(sess);
}

int sceSsl_lib_86D0771E(SslSession *sess)
{
    if (sess == NULL) {
        return 0;
    }
    sess->refCount++;
    return sess->refCount;
}

int sceSsl_lib_69F1B678(SslConn *ssl, SslSession *sess)
{
    if (ssl == NULL) {
        return 0;
    }
    if (ssl->session != NULL) {
        sceSsl_lib_BDAD0841(ssl->session);
    }
    ssl->session = sess;
    if (sess != NULL) {
        sess->refCount++;
    }
    return 1;
}

int sceSsl_lib_4214B7AF(SslSession *sess, u32 timeout)
{
    if (sess == NULL) {
        return 0;
    }
    sess->timeout = timeout;
    return 1;
}

u32 sceSsl_lib_795B9EDF(const SslSession *sess)
{
    return (sess != NULL) ? sess->timeout : 0;
}

void *sceSsl_lib_4E0F2CCC(SslConn *ssl, int mode)
{
    (void)ssl;
    (void)mode;
    return NULL;
}

/* --- SSL_CTX management (ssl/ssl_lib.c) --- */

SslCtx *sceSsl_lib_FB8273FE(const SslMethod *method)
{
    SslCtx *ctx;

    if (method == NULL) {
        return NULL;
    }
    ctx = (SslCtx *)sceSsl_lib_0861D934(sizeof(SslCtx));
    if (ctx == NULL) {
        return NULL;
    }
    sceKernelMemset(ctx, 0, sizeof(SslCtx));
    ctx->method = method;
    ctx->refCount = 1;
    ctx->verifyDepth = 9;
    ctx->sessionTimeout = 300;
    ctx->maxCertList = 102400;
    return ctx;
}

void sceSsl_lib_588F2FE8(SslCtx *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->refCount--;
    if (ctx->refCount > 0) {
        return;
    }
    if (ctx->clientCert != NULL) {
        sceSsl_lib_C1A2E14B(ctx->clientCert);
    }
    if (ctx->clientKey != NULL) {
        sceSsl_lib_7317C1BC(ctx->clientKey);
    }
    sceSsl_lib_5E5C873A(ctx);
}

void sceSsl_lib_529A9477(SslCtx *ctx, SslCertVerifyCb cb, void *arg)
{
    if (ctx != NULL) {
        ctx->appVerifyCallback = cb;
        ctx->appVerifyArg = arg;
    }
}

int sceSsl_lib_AEBF278B(SslCtx *ctx, int mode, SslVerifyCb cb)
{
    if (ctx == NULL) {
        return 0;
    }
    ctx->verifyMode = mode;
    ctx->verifyCallback = cb;
    return 1;
}

int sceSsl_lib_B4D78E98(SslCtx *ctx, int cmd, int larg, void *parg)
{
    if (ctx == NULL || ctx->method == NULL || ctx->method->sslCtxCtrl == NULL) {
        return 0;
    }
    return ctx->method->sslCtxCtrl(ctx, cmd, larg, parg);
}

void sceSsl_lib_BF55C31C(SslCtx *ctx, int quiet)
{
    if (ctx != NULL) {
        ctx->quietShutdown = quiet;
    }
}

/* --- SSL connection management --- */

int sceSsl_lib_54A7D8F3(SslConn *ssl)
{
    if (ssl == NULL || ssl->method == NULL) {
        return 0;
    }
    if (ssl->method->sslClear != NULL) {
        ssl->method->sslClear(ssl);
    }
    ssl->hit = 0;
    ssl->errorCode = 0;
    return 1;
}

SslConn *sceSsl_lib_EBFB0E3C(SslCtx *ctx)
{
    SslConn *ssl;

    if (ctx == NULL || ctx->method == NULL) {
        return NULL;
    }
    ssl = (SslConn *)sceSsl_lib_0861D934(sizeof(SslConn));
    if (ssl == NULL) {
        return NULL;
    }
    sceKernelMemset(ssl, 0, sizeof(SslConn));
    ssl->ctx = ctx;
    ctx->refCount++;
    ssl->method = ctx->method;
    ssl->version = ctx->method->version;
    ssl->options = ctx->options;
    ssl->mode = ctx->mode;
    ssl->verifyMode = ctx->verifyMode;
    ssl->verifyCallback = ctx->verifyCallback;
    ssl->quietShutdown = ctx->quietShutdown;
    ssl->refCount = 1;
    if (ssl->method->sslNew != NULL && ssl->method->sslNew(ssl) == 0) {
        ctx->refCount--;
        sceSsl_lib_5E5C873A(ssl);
        return NULL;
    }
    return ssl;
}

void sceSsl_lib_84833472(SslConn *ssl)
{
    if (ssl == NULL) {
        return;
    }
    ssl->refCount--;
    if (ssl->refCount > 0) {
        return;
    }
    if (ssl->session != NULL) {
        sceSsl_lib_BDAD0841(ssl->session);
    }
    if (ssl->rbio != NULL) {
        sceSsl_lib_959557F5(ssl->rbio);
    }
    if (ssl->wbio != NULL && ssl->wbio != ssl->rbio) {
        sceSsl_lib_959557F5(ssl->wbio);
    }
    if (ssl->method != NULL && ssl->method->sslFree != NULL) {
        ssl->method->sslFree(ssl);
    }
    if (ssl->ctx != NULL) {
        sceSsl_lib_588F2FE8(ssl->ctx);
    }
    sceSsl_lib_5E5C873A(ssl);
}

void sceSsl_lib_B9C8CCE6(SslConn *ssl, SslBio *rbio, SslBio *wbio)
{
    if (ssl == NULL) {
        return;
    }
    if (ssl->rbio != NULL && ssl->rbio != rbio) {
        sceSsl_lib_959557F5(ssl->rbio);
    }
    if (ssl->wbio != NULL && ssl->wbio != wbio && ssl->wbio != ssl->rbio) {
        sceSsl_lib_959557F5(ssl->wbio);
    }
    ssl->rbio = rbio;
    ssl->wbio = wbio;
}

SslBio *sceSsl_lib_2E2E2B09(const SslConn *ssl) { return (ssl != NULL) ? ssl->rbio : NULL; }
SslBio *sceSsl_lib_868C15DF(const SslConn *ssl) { return (ssl != NULL) ? ssl->wbio : NULL; }

int sceSsl_lib_D49A570C(const SslConn *ssl)
{
    if (ssl == NULL || ssl->method == NULL || ssl->method->sslPending == NULL) {
        return 0;
    }
    return ssl->method->sslPending(ssl);
}

int sceSsl_lib_E7C29542(SslConn *ssl, void *buf, int len)
{
    if (ssl == NULL || ssl->method == NULL || ssl->method->sslRead == NULL) {
        return -1;
    }
    if ((ssl->shutdown & SSL_RECEIVED_SHUTDOWN) != 0) {
        return 0;
    }
    return ssl->method->sslRead(ssl, buf, len);
}

int sceSsl_lib_B7CA8717(SslConn *ssl, const void *buf, int len)
{
    if (ssl == NULL || ssl->method == NULL || ssl->method->sslWrite == NULL) {
        return -1;
    }
    return ssl->method->sslWrite(ssl, buf, len);
}

int sceSsl_lib_3E3133D6(SslConn *ssl)
{
    if (ssl == NULL || ssl->method == NULL || ssl->method->sslShutdown == NULL) {
        return -1;
    }
    return ssl->method->sslShutdown(ssl);
}

int sceSsl_lib_B3B04C58(const SslConn *ssl, int retCode)
{
    if (retCode > 0) {
        return SSL_ERROR_NONE;
    }
    if (sceSsl_lib_56E19CEB() != 0) {
        return SSL_ERROR_SSL;
    }
    if (ssl == NULL) {
        return SSL_ERROR_SYSCALL;
    }
    if (retCode == 0) {
        return ((ssl->shutdown & SSL_RECEIVED_SHUTDOWN) != 0) ? SSL_ERROR_ZERO_RETURN : SSL_ERROR_SYSCALL;
    }
    if (ssl->rwstate == SSL_READING) {
        return SSL_ERROR_WANT_READ;
    }
    if (ssl->rwstate == SSL_WRITING) {
        return SSL_ERROR_WANT_WRITE;
    }
    if (ssl->rwstate == SSL_X509_LOOKUP) {
        return SSL_ERROR_WANT_X509_LOOKUP;
    }
    return SSL_ERROR_SYSCALL;
}

int sceSsl_lib_D53ACF30(SslConn *ssl)
{
    if (ssl == NULL || ssl->method == NULL || ssl->method->sslConnect == NULL) {
        return -1;
    }
    return ssl->method->sslConnect(ssl);
}

void sceSsl_lib_80608663(SslConn *ssl)
{
    if (ssl == NULL) {
        return;
    }
    ssl->server = 0;
    ssl->shutdown = 0;
    ssl->state = 0x1000;
    ssl->handshakeFunc = (ssl->method != NULL) ? ssl->method->sslConnect : NULL;
}

int sceSsl_lib_88897D26(const SslConn *ssl)
{
    return (ssl != NULL) ? ssl->version : 0;
}

/* Peer certificate chain accessors */
int sceSsl_lib_92FA8B34(const SslConn *ssl)
{
    return (ssl != NULL) ? ssl->peerChainLen : 0;
}

SslX509Cert *sceSsl_lib_7BDAF864(const SslConn *ssl, int index)
{
    if (ssl == NULL || index < 0 || index >= ssl->peerChainLen) {
        return NULL;
    }
    return ssl->peerChain[index];
}

/* SSL / SSL_CTX property getters and setters (0x27824..0x27b60) */
void sceSsl_lib_42DF4592(SslConn *ssl, u32 readAhead) { if (ssl != NULL) ssl->readAhead = readAhead; }
u32 sceSsl_lib_E14101E4(const SslConn *ssl) { return (ssl != NULL) ? ssl->readAhead : 0; }
void sceSsl_lib_4AF959E3(SslConn *ssl, int verifyResult) { if (ssl != NULL) ssl->verifyResult = verifyResult; }
int sceSsl_lib_6A19E0AA(const SslConn *ssl) { return (ssl != NULL) ? ssl->verifyResult : 0; }
void sceSsl_lib_D5ABC603(SslConn *ssl, void *appData) { if (ssl != NULL) ssl->appData = appData; }
void *sceSsl_lib_909E9D31(const SslConn *ssl) { return (ssl != NULL) ? ssl->appData : NULL; }
void sceSsl_lib_AE3986D3(SslCtx *ctx, void *appData) { if (ctx != NULL) ctx->appData = appData; }
void *sceSsl_lib_340F8829(const SslCtx *ctx) { return (ctx != NULL) ? ctx->appData : NULL; }
void sceSsl_lib_86D4034D(SslConn *ssl, int quiet) { if (ssl != NULL) ssl->quietShutdown = quiet; }
int sceSsl_lib_D1205C58(const SslConn *ssl) { return (ssl != NULL) ? ssl->quietShutdown : 0; }
int sceSsl_lib_57F2E960(const SslConn *ssl) { return (ssl != NULL) ? ssl->shutdown : 0; }
int sceSsl_lib_E92302C5(const SslConn *ssl) { return (ssl != NULL) ? ssl->state : 0; }
void sceSsl_lib_C270B3A8(SslConn *ssl, void *infoCb) { if (ssl != NULL) ssl->infoCallback = infoCb; }
void sceSsl_lib_BE423CD1(SslConn *ssl, void *msgCb, void *arg) { if (ssl != NULL) { ssl->msgCallback = msgCb; ssl->msgCallbackArg = arg; } }
void *sceSsl_lib_95E346AB(const SslConn *ssl) { return (ssl != NULL) ? ssl->infoCallback : NULL; }
void sceSsl_lib_9CB1A62F(SslCtx *ctx, int mode, SslVerifyCb cb) { if (ctx != NULL) { ctx->verifyMode = mode; ctx->verifyCallback = cb; } }
int sceSsl_lib_104F749D(const SslConn *ssl) { return (ssl != NULL) ? ssl->verifyMode : 0; }
void sceSsl_lib_5605C9FE(SslConn *ssl, int debug) { if (ssl != NULL) ssl->debug = debug; }
int sceSsl_lib_AA3B27CF(const SslConn *ssl) { return (ssl != NULL) ? ssl->debug : 0; }
int sceSsl_lib_C97D0510(const SslConn *ssl) { return (ssl != NULL) ? ssl->hit : 0; }
int sceSsl_lib_A8F0AD39(const SslConn *ssl) { return (ssl != NULL) ? ssl->errorCode : 0; }

int sceSsl_lib_C0ABBB57(SslCtx *ctx, SslX509Cert *cert)
{
    if (ctx == NULL || cert == NULL) {
        return 0;
    }
    if (ctx->clientCert != NULL) {
        sceSsl_lib_C1A2E14B(ctx->clientCert);
    }
    cert->refCount++;
    ctx->clientCert = cert;
    return 1;
}

int sceSsl_lib_8390B321(SslCtx *ctx, SslEvpPkey *pkey)
{
    if (ctx == NULL || pkey == NULL) {
        return 0;
    }
    if (ctx->clientKey != NULL) {
        sceSsl_lib_7317C1BC(ctx->clientKey);
    }
    pkey->refCount++;
    ctx->clientKey = pkey;
    return 1;
}
