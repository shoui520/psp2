/*
 * ePSP 6.61 sceDNAS_Library (libdnas.prx)
 * Reimplemented from 3.650.011-TOOL pcff flash0:/kd/libdnas.prx
 */

#include <stdint.h>
#include <stddef.h>

typedef int SceUID;
typedef unsigned int SceSize;
typedef unsigned int SceUInt;

/* Imports */
extern void *sceKernelMemset(void *s, int c, SceSize n);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize n);

extern SceUID sceKernelCreateEventFlag(const char *name, int attr, int bits, void *opt);
extern int sceKernelDeleteEventFlag(SceUID evid);
extern int sceKernelSetEventFlag(SceUID evid, unsigned int bits);
extern int sceKernelPollEventFlag(SceUID evid, unsigned int bits, int wait, unsigned int *outBits);
extern int sceKernelWaitEventFlag(SceUID evid, unsigned int bits, int wait, unsigned int *outBits, SceUInt *timeout);
extern SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *), int initPriority, int stackSize, SceUInt attr, void *opt);
extern int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern int sceKernelWaitThreadEnd(SceUID thid, SceUInt *timeout);
extern int sceKernelTerminateDeleteThread(SceUID thid);

extern int sceCertLoader_DD629A24(int a0, int a1, void ***out_list, void *buf, int buf_size, int *out_size);

extern int sceDNASCore_lib_15096ECD(char *host, uint16_t *port, const char *redirect_str);
extern int sceDNASCore_lib_2370130E(void);
extern int sceDNASCore_lib_26E1E2BD(const void *buf, int len);
extern int sceDNASCore_lib_2B6C67EA(void *arg0, int arg1);
extern int sceDNASCore_lib_4108128B(void);
extern int sceDNASCore_lib_80CEC43A(void);
extern int sceDNASCore_lib_822357BB(void *buf, int len);
extern int sceDNASCore_lib_8309549E(const void *buf, int len);
extern int sceDNASCore_lib_B6C76A14(char *redirect_buf);
extern int sceDNASCore_lib_BA0A32CA(void);
extern int sceDNASCore_lib_BA0D27F8(void);
extern int sceDNASCore_lib_BF6A7475(void *arg0);
extern int sceDNASCore_lib_C54657B7(const void *buf, int len);
extern int sceDNASCore_lib_D5E80301(void);
extern int sceDNASCore_lib_DA5939B4(void *buf, int len);
extern int sceDNASCore_lib_F0EB4367(void *buf, int len);
extern int sceDNASCore_lib_F479F616(const char *cn);
extern int sceDNASCore_lib_FA571A75(void *auth_arg, const char *proxy_url, uint16_t proxy_port);

extern int sceNetFreeThreadinfo(int thid);
extern int sceNetApctlGetInfo(int code, void *pInfo);

extern int sceNetInetSocket(int domain, int type, int protocol);
extern int sceNetInetConnect(int s, const void *name, unsigned int namelen);
extern int sceNetInetSend(int s, const void *buf, unsigned int len, int flags);
extern int sceNetInetRecv(int s, void *buf, unsigned int len, int flags);
extern int sceNetInetClose(int s);
extern int sceNetInet_80A21ABD(int s);
extern int sceNetInet_8CA3A97E(void);
extern int sceNetInetInetAddr(const char *cp);

extern int sceNetResolverCreate(int *rid, void *buf, SceSize buflen);
extern int sceNetResolverStartNtoA(int rid, const char *hostname, uint32_t *addr, unsigned int timeout, int retry);
extern int sceNetResolverStop(int rid);
extern int sceNetResolverDelete(int rid);

extern char *sceNet_lib_9CFBC7E3(char *dst, const char *src, unsigned int n);
extern char *sceNet_lib_A0F16ABD(char *dst, const char *src);
extern char *sceNet_lib_BCBE14CF(const char *s, int c);
extern int sceNet_lib_D8722983(const char *s1, const char *s2);

extern void *sceSsl_lib_0861D934(unsigned int size);
extern int sceSsl_lib_0FF7BA68(void *cert, void *pubkey);
extern void *sceSsl_lib_12C7718F(void *cert);
extern void *sceSsl_lib_28B4DE33(int sock, int close_flag);
extern const char *sceSsl_lib_2B7BD166(int nid1, int nid2, int flags);
extern void sceSsl_lib_37C7B76C(void *cert, void *a1, int *out_count);
extern void sceSsl_lib_3E3133D6(void *ssl);
extern void *sceSsl_lib_4BE9030A(void *cert);
extern int sceSsl_lib_4C75B8E9(void *name);
extern void *sceSsl_lib_5040E836(void *name, int idx);
extern void sceSsl_lib_529A9477(void *ssl_ctx, int (*cb)(int, void **, int, void *, int *), void *arg);
extern void sceSsl_lib_54A7D8F3(void *ssl);
extern void sceSsl_lib_588F2FE8(void *ssl_ctx);
extern void sceSsl_lib_5E5C873A(void *ptr);
extern void *sceSsl_lib_6038DEDE(void *a0, void **der_ptr, int der_len);
extern int sceSsl_lib_69F1B678(void *ssl, void *ssl_session);
extern int sceSsl_lib_7D0E41F4(void *name1, void *name2);
extern void sceSsl_lib_80608663(void *ssl);
extern void sceSsl_lib_84833472(void *ssl);
extern void sceSsl_lib_86D0771E(void *ssl_session);
extern int sceSsl_lib_8D484361(void *entry, int a1, int *out_nid1, int *out_nid2, void *a4, void **out_data, int *out_len);
extern int sceSsl_lib_9299C6CE(void *cert, void *a1, void **out_ptr, int *out_count);
extern void sceSsl_lib_AEBF278B(void *ssl_ctx, int depth, void *arg);
extern int sceSsl_lib_B3B04C58(void *ssl, int ret);
extern void *sceSsl_lib_B40D11EA(void);
extern void sceSsl_lib_B4D78E98(void *ssl_ctx, int a1, unsigned int a2, int a3);
extern int sceSsl_lib_B7CA8717(void *ssl, const void *buf, int len);
extern void *sceSsl_lib_B87FFE37(void *cert);
extern void sceSsl_lib_B9C8CCE6(void *ssl, void *rbio, void *wbio);
extern void sceSsl_lib_BDAD0841(void *ssl_session);
extern void sceSsl_lib_BF55C31C(void *ssl_ctx, int mode);
extern void sceSsl_lib_C1A2E14B(void *cert);
extern int sceSsl_lib_CFA2F8EB(void *cert, void *a1, int *out_pathlen, int *out_isca);
extern int sceSsl_lib_E7C29542(void *ssl, void *buf, int len);
extern void *sceSsl_lib_EBFB0E3C(void *ssl_ctx);
extern void *sceSsl_lib_EF5A4BD4(void *ssl);
extern void *sceSsl_lib_FB8273FE(void *method);

typedef struct DnasContext {
    SceUID thid;              /* 0x00 */
    SceUID ef_int;            /* 0x04 */
    SceUID ef_ext;            /* 0x08 */
    int state;                /* 0x0c */
    void *auth_arg;           /* 0x10 */
    int error;                /* 0x14 */
    int sock;                 /* 0x18 */
    int rid;                  /* 0x1c */
    int use_proxy;            /* 0x20 */
    int redirect_count;       /* 0x24 */
    int ssl_verify_err;       /* 0x28 */
    uint16_t port;            /* 0x2c */
    char host[0x92];          /* 0x2e */
    uint32_t resolved_ip;     /* 0xc0 */
    void *ssl;                /* 0xc4 */
    void *ssl_ctx;            /* 0xc8 */
    void *ssl_session;        /* 0xcc */
    int skip_cn_check;        /* 0xd0 */
    void *ext_arg;            /* 0xd4 */
} DnasContext;

/* Forward declarations */
static void sub_08c8(DnasContext *ctx);
static int sub_0938(DnasContext *ctx, int check_abort);
static int sub_0a4c(SceSize args, void *argp);
static void sub_0c38(DnasContext *ctx);
static void sub_0de0(DnasContext *ctx);
static void sub_0ec8(DnasContext *ctx);
static void sub_0ff4(DnasContext *ctx);
static void sub_10d0(DnasContext *ctx);
static int sub_11b4(DnasContext *ctx);
static int sub_12e4(DnasContext *ctx);
static uint16_t sub_16cc(uint16_t val);
static int sub_1700(DnasContext *ctx, const void *buf, int len);
static int sub_1844(DnasContext *ctx, void *buf, int len);
static int sub_1910(DnasContext *ctx);
static void sub_19f4(DnasContext *ctx);
static int sub_1aa0(DnasContext *ctx);
static int sub_1c68(DnasContext *ctx);
static int sub_1d7c(DnasContext *ctx);
static void sub_1de4(void);
static int sub_1e90(void *subj, const char *expected_cn);
static int sub_2098(int preverify_ok, void **cert_chain, int chain_len, void *arg, int *alert);
static int sub_25f0(const char *pem, uint8_t *out_buf, int *out_len, unsigned int max_len);
static int sub_28c8(void);
static int sub_2c08(DnasContext *ctx);
static void sub_2d5c(DnasContext *ctx);
static int sub_2de0(DnasContext *ctx);
static int sub_3028(DnasContext *ctx);
static int sub_3118(DnasContext *ctx, int *redirected);
static int sub_3270(DnasContext *ctx);
static int sub_3360(DnasContext *ctx);
static int sub_3410(uint8_t *dst, const char *src, int len);

/* .rodata base64 decode table (0x3b60..0x3c60) */
static const int8_t g_b64_table[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1,  0, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

/* .data (0x3c60..0x3c78) */
static void (*g_state_handlers[5])(DnasContext *) = {
    sub_0c38,
    sub_0de0,
    sub_0ec8,
    sub_0ff4,
    sub_10d0
};
static int g_ssl_initialized = -1; /* 0x3c74 */

/* .bss (0x3c78..0x4474) */
static int g_initialized;          /* 0x3c80 */
static DnasContext g_ctx;          /* 0x3c84 */
static char g_resolver_buf[0x300]; /* 0x3d5c */
static int g_root_cert_count;      /* 0x405c */
static void *g_root_certs[5];      /* 0x4060 */
static uint8_t g_io_buf[0x400];    /* 0x4074 */

/* === sceDNAS_lib_0D560144 @ 0x0000 === */
int sceDNAS_lib_0D560144(void *arg0, int prio)
{
    int ret = 0;

    if (g_initialized != 0) {
        return 0x80530182;
    }

    sceKernelMemset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.thid = -1;
    g_ctx.ef_int = -1;
    g_ctx.ef_ext = -1;
    sub_08c8(&g_ctx);

    ret = sub_2c08(&g_ctx);
    if (ret < 0) {
        goto fail;
    }

    ret = sceKernelCreateEventFlag("SceDNASInternal", 0x200, 0, NULL);
    if (ret <= 0) {
        goto fail;
    }
    g_ctx.ef_int = ret;

    ret = sceKernelCreateEventFlag("SceDNASExternal", 0x200, 0, NULL);
    if (ret <= 0) {
        goto fail;
    }
    g_ctx.ef_ext = ret;

    ret = sceKernelCreateThread("SceDNAS", sub_0a4c, prio, 0x2000, 0, NULL);
    if (ret <= 0) {
        goto fail;
    }
    g_ctx.thid = ret;

    {
        DnasContext *ctx_ptr = &g_ctx;
        ret = sceKernelStartThread(g_ctx.thid, 4, &ctx_ptr);
    }
    if (ret < 0) {
        goto fail;
    }

    ret = sceDNASCore_lib_2B6C67EA(arg0, 0);
    if (ret < 0) {
        goto fail;
    }

    g_initialized = 1;
    return 0;

fail:
    if (g_ctx.thid > 0) {
        sceKernelTerminateDeleteThread(g_ctx.thid);
        sceNetFreeThreadinfo(g_ctx.thid);
        g_ctx.thid = -1;
    }
    if (g_ctx.ef_ext > 0) {
        sceKernelDeleteEventFlag(g_ctx.ef_ext);
        g_ctx.ef_ext = -1;
    }
    if (g_ctx.ef_int > 0) {
        sceKernelDeleteEventFlag(g_ctx.ef_int);
        g_ctx.ef_int = -1;
    }
    sub_2d5c(&g_ctx);
    return ret;
}

/* === sceDNAS_lib_CA8B8909 @ 0x02d8 === */
int sceDNAS_lib_CA8B8909(void)
{
    int ret = 0;

    if (g_initialized == 0) {
        return 0x80530181;
    }
    if (g_ctx.state != 0) {
        return 0x80530183;
    }

    ret = sceKernelPollEventFlag(g_ctx.ef_int, 1, 0, NULL);
    if (ret == 0) {
        return 0x80530183;
    }
    if (ret != (int)0x80020192) {
        return ret;
    }

    g_ctx.auth_arg = NULL;
    ret = sceKernelSetEventFlag(g_ctx.ef_int, 1);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

/* === sceDNAS_lib_F3787AD8 @ 0x03ec === */
int sceDNAS_lib_F3787AD8(void *auth_arg)
{
    int ret = 0;

    if (g_initialized == 0) {
        return 0x80530181;
    }
    if (auth_arg == NULL) {
        return 0x80530180;
    }
    if (g_ctx.state != 0) {
        return 0x80530183;
    }

    ret = sceKernelPollEventFlag(g_ctx.ef_int, 1, 0, NULL);
    if (ret == 0) {
        return 0x80530183;
    }
    if (ret != (int)0x80020192) {
        return ret;
    }

    g_ctx.auth_arg = auth_arg;
    ret = sceKernelSetEventFlag(g_ctx.ef_int, 1);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

/* === sceDNAS_lib_9FF48DD3 @ 0x0528 === */
int sceDNAS_lib_9FF48DD3(void)
{
    int ret = 0;

    if (g_initialized == 0) {
        return 0x80530181;
    }
    if (g_ctx.state == 0) {
        return 0x80530184;
    }

    ret = sceKernelSetEventFlag(g_ctx.ef_int, 2);
    if (ret < 0) {
        return ret;
    }

    if (g_ctx.rid >= 0) {
        sceNetResolverStop(g_ctx.rid);
    }
    if (g_ctx.sock >= 0) {
        sceNetInet_80A21ABD(g_ctx.sock);
    }

    return 0;
}

/* === sceDNAS_lib_45C1AAF5 @ 0x05f0 === */
int sceDNAS_lib_45C1AAF5(SceUID *out_ef)
{
    volatile int ret = 0;
    SceUID * volatile p = out_ef;

    if (g_initialized == 0) {
        ret = 0x80530102;
    } else if (p == NULL) {
        ret = 0x80530103;
    } else {
        *p = g_ctx.ef_ext;
        ret = 0;
    }
    return ret;
}

/* === sceDNAS_lib_BE0998D7 @ 0x0674 === */
int sceDNAS_lib_BE0998D7(void)
{
    int ret = 0;

    if (g_initialized == 0) {
        return 0x80530181;
    }

    ret = sceKernelSetEventFlag(g_ctx.ef_int, 4);
    if (g_ctx.rid >= 0) {
        sceNetResolverStop(g_ctx.rid);
    }
    if (g_ctx.sock >= 0) {
        sceNetInet_80A21ABD(g_ctx.sock);
    }

    if (g_ctx.thid > 0) {
        ret = sceKernelWaitThreadEnd(g_ctx.thid, NULL);
        ret = sceKernelTerminateDeleteThread(g_ctx.thid);
        ret = sceNetFreeThreadinfo(g_ctx.thid);
        g_ctx.thid = -1;
    }

    if (g_ctx.ef_ext > 0) {
        ret = sceKernelDeleteEventFlag(g_ctx.ef_ext);
        g_ctx.ef_ext = -1;
    }
    if (g_ctx.ef_int > 0) {
        ret = sceKernelDeleteEventFlag(g_ctx.ef_int);
        g_ctx.ef_int = -1;
    }

    sceDNASCore_lib_D5E80301();
    sub_2d5c(&g_ctx);
    g_initialized = 0;
    (void)ret;
    return 0;
}

/* === sceDNAS_lib_A646E771 @ 0x0814 === */
int sceDNAS_lib_A646E771(int *out_state, int *out_error)
{
    if (g_initialized == 0) {
        return 0x80530181;
    }

    if (out_state != NULL) {
        *out_state = g_ctx.state;
    }
    if (out_error != NULL) {
        *out_error = g_ctx.error;
    }
    return 0;
}

/* === sceDNAS_lib_6929100C @ 0x0890 === */
int sceDNAS_lib_6929100C(void *arg0)
{
    return sceDNASCore_lib_BF6A7475(arg0);
}

/* === sub_08c8 @ 0x08c8 === */
static void sub_08c8(DnasContext *ctx)
{
    ctx->sock = -1;
    ctx->rid = -1;
    ctx->redirect_count = 0;
    sceKernelMemset(ctx->host, 0, 0x80);
    ctx->resolved_ip = 0;
    ctx->ssl = NULL;
    ctx->ssl_session = NULL;
}

/* === sub_0938 @ 0x0938 === */
static int sub_0938(DnasContext *ctx, int check_abort)
{
    int ret = 0;
    unsigned int out_bits = 0;
    unsigned int mask = (check_abort != 0) ? 6 : 4;

    ret = sceKernelPollEventFlag(ctx->ef_int, mask, 0x11, &out_bits);
    if (ret == (int)0x80020192) {
        return 0;
    }
    if (ret < 0) {
        ctx->state = 0;
        ctx->error = ret;
        sceKernelSetEventFlag(ctx->ef_ext, 1);
        return 2;
    }
    if ((out_bits & 4) != 0) {
        return 1;
    }
    if ((out_bits & 2) != 0) {
        ctx->state = 0;
        ctx->error = 0x80530106;
        sceKernelSetEventFlag(ctx->ef_ext, 1);
        return 2;
    }
    return 0;
}

/* === sub_0a4c @ 0x0a4c === */
static int sub_0a4c(SceSize args, void *argp)
{
    DnasContext *ctx = *(DnasContext **)argp;
    int status;
    (void)args;

    for (;;) {
        status = sub_0938(ctx, (ctx->state != 0) ? 1 : 0);
        if (status == 1) {
            break;
        }
        if (status == 2) {
            continue;
        }

        if (ctx->state >= 0 && ctx->state < 5) {
            g_state_handlers[ctx->state](ctx);
        } else {
            ctx->state = 0;
            ctx->error = 0x80530186;
            sceKernelSetEventFlag(ctx->ef_ext, 1);
            continue;
        }

        status = sub_0938(ctx, (ctx->state != 0) ? 1 : 0);
        if (status == 1) {
            break;
        }
        if (status == 2) {
            continue;
        }

        if (ctx->error < 0 || ctx->state == 0) {
            sub_1910(ctx);
            ctx->state = 0;
            sceKernelSetEventFlag(ctx->ef_ext, 1);
        }
    }

    sub_1910(ctx);
    return 0;
}

/* === sub_0c38 @ 0x0c38 === */
static void sub_0c38(DnasContext *ctx)
{
    int ret = 0;
    unsigned int out_bits = 0;

    ret = sceKernelWaitEventFlag(ctx->ef_int, 7, 0x11, &out_bits, NULL);
    if (ret < 0) {
        ctx->error = ret;
        return;
    }
    if ((out_bits & 4) != 0) {
        sceKernelSetEventFlag(ctx->ef_int, 4);
        return;
    }
    if ((out_bits & 2) != 0) {
        return;
    }

    ctx->error = 0;
    sub_08c8(ctx);
    ret = sceDNASCore_lib_FA571A75(ctx->auth_arg, NULL, 0);
    if (ret < 0) {
        ctx->error = ret;
        return;
    }

    ctx->state++;
    sceKernelSetEventFlag(ctx->ef_ext, 1);
}

/* === sub_0de0 @ 0x0de0 === */
static void sub_0de0(DnasContext *ctx)
{
    int ret = 0;

    if (ctx->ssl_session != NULL) {
        sub_08c8(ctx);
    }

    ret = sub_3028(ctx);
    if (ret < 0) {
        ctx->error = ret;
        return;
    }

    ctx->state++;
    sceKernelSetEventFlag(ctx->ef_ext, 1);
}

/* === sub_0ec8 @ 0x0ec8 === */
static void sub_0ec8(DnasContext *ctx)
{
    int ret = 0;
    int redirected = 0;

    ret = sub_3118(ctx, &redirected);
    if (ret < 0) {
        ctx->error = ret;
        return;
    }

    if (redirected != 0) {
        sub_1910(ctx);
        ctx->state = 1;
        return;
    }

    sub_19f4(ctx);
    ctx->state++;
    sceKernelSetEventFlag(ctx->ef_ext, 1);
}

/* === sub_0ff4 @ 0x0ff4 === */
static void sub_0ff4(DnasContext *ctx)
{
    int ret = 0;

    ret = sub_3270(ctx);
    if (ret < 0) {
        ctx->error = ret;
        return;
    }

    ctx->state++;
    sceKernelSetEventFlag(ctx->ef_ext, 1);
}

/* === sub_10d0 @ 0x10d0 === */
static void sub_10d0(DnasContext *ctx)
{
    int ret = 0;

    ret = sub_3360(ctx);
    if (ret < 0) {
        ctx->error = ret;
        return;
    }

    sub_08c8(ctx);
    ctx->state = 0;
    sceKernelSetEventFlag(ctx->ef_ext, 1);
}

/* === sub_11b4 @ 0x11b4 === */
static int sub_11b4(DnasContext *ctx)
{
    int ret = 0;

    ret = sceNetApctlGetInfo(13, &ctx->use_proxy);
    if (ret < 0) {
        return ret;
    }
    if (ctx->use_proxy == 0) {
        return 0;
    }

    ret = sceNetApctlGetInfo(14, ctx->host);
    if (ret < 0) {
        return ret;
    }

    ret = sceNetApctlGetInfo(15, &ctx->port);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

/* === sub_12e4 @ 0x12e4 === */
static int sub_12e4(DnasContext *ctx)
{
    int ret = 0;
    uint32_t addr = 0;
    uint8_t sin[16];
    int retry;

    if (ctx->resolved_ip == 0) {
        addr = (uint32_t)sceNetInetInetAddr(ctx->host);
        if (addr == 0xffffffffU) {
            ret = sceNetResolverCreate(&ctx->rid, g_resolver_buf, sizeof(g_resolver_buf));
            if (ret < 0) {
                goto out;
            }
            ret = sceNetResolverStartNtoA(ctx->rid, ctx->host, &addr, 2000000, 4);
            if (ret < 0) {
                goto out;
            }
            sceNetResolverDelete(ctx->rid);
            ctx->rid = -1;
        }
        ctx->resolved_ip = addr;
    }

    for (retry = 0; retry < 5; retry++) {
        ret = sceNetInetSocket(2, 1, 0);
        if (ret < 0) {
            ret = sceNetInet_8CA3A97E();
            goto out;
        }
        ctx->sock = ret;

        sceKernelMemset(sin, 0, sizeof(sin));
        sin[0] = 16;
        sin[1] = 2;
        *(uint16_t *)&sin[2] = sub_16cc(ctx->port);
        *(uint32_t *)&sin[4] = ctx->resolved_ip;

        ret = sceNetInetConnect(ctx->sock, sin, sizeof(sin));
        if (ret == 0) {
            return 0;
        }
        ret = sceNetInet_8CA3A97E();
        sub_1d7c(ctx);
        if (ret != (int)0x8001006f) {
            break;
        }
    }

    ctx->resolved_ip = 0;

out:
    if (ctx->rid >= 0) {
        sceNetResolverDelete(ctx->rid);
        ctx->rid = -1;
    }
    return ret;
}

/* === sub_16cc @ 0x16cc === */
static uint16_t sub_16cc(uint16_t val)
{
    uint16_t res;
    __asm__ volatile ("wsbh %0, %1" : "=r"(res) : "r"(val));
    return res;
}

/* === sub_1700 @ 0x1700 === */
static int sub_1700(DnasContext *ctx, const void *buf, int len)
{
    int ret = 0;
    int err;

    ctx->ssl_verify_err = 0;
    ret = sceSsl_lib_B7CA8717(ctx->ssl, buf, len);
    err = sceSsl_lib_B3B04C58(ctx->ssl, ret);
    if (err == 1) {
        return (ctx->ssl_verify_err != 0) ? ctx->ssl_verify_err : (int)0x80530183;
    }
    if (err == 5) {
        return (ctx->ssl_verify_err != 0) ? ctx->ssl_verify_err : (int)0x80530184;
    }
    return ret;
}

/* === sub_1844 @ 0x1844 === */
static int sub_1844(DnasContext *ctx, void *buf, int len)
{
    int ret = 0;
    int err;

    ctx->ssl_verify_err = 0;
    ret = sceSsl_lib_E7C29542(ctx->ssl, buf, len);
    err = sceSsl_lib_B3B04C58(ctx->ssl, ret);
    if (err == 1) {
        return (ctx->ssl_verify_err != 0) ? ctx->ssl_verify_err : (int)0x80530183;
    }
    return ret;
}

/* === sub_1910 @ 0x1910 === */
static int sub_1910(DnasContext *ctx)
{
    if (ctx->ssl != NULL) {
        sceSsl_lib_3E3133D6(ctx->ssl);
        sceSsl_lib_54A7D8F3(ctx->ssl);
        sceSsl_lib_69F1B678(ctx->ssl, NULL);
        sceSsl_lib_B9C8CCE6(ctx->ssl, NULL, NULL);
        sceSsl_lib_84833472(ctx->ssl);
        ctx->ssl = NULL;
    }
    if (ctx->ssl_session != NULL) {
        sceSsl_lib_BDAD0841(ctx->ssl_session);
        ctx->ssl_session = NULL;
    }
    return 0;
}

/* === sub_19f4 @ 0x19f4 === */
static void sub_19f4(DnasContext *ctx)
{
    if (ctx->ssl != NULL) {
        if (ctx->ssl_session != NULL) {
            sceSsl_lib_BDAD0841(ctx->ssl_session);
            ctx->ssl_session = NULL;
        }
        ctx->ssl_session = sceSsl_lib_EF5A4BD4(ctx->ssl);
        if (ctx->ssl_session != NULL) {
            sceSsl_lib_86D0771E(ctx->ssl_session);
        }
    }
}

/* === sub_1aa0 @ 0x1aa0 === */
static int sub_1aa0(DnasContext *ctx)
{
    int ret = 0;
    void *bio;

    if (ctx->ssl != NULL && ctx->ssl_session != NULL) {
        ret = sceSsl_lib_69F1B678(ctx->ssl, ctx->ssl_session);
        if (ret != 1) {
            sub_1910(ctx);
            ctx->ssl = sceSsl_lib_EBFB0E3C(ctx->ssl_ctx);
            if (ctx->ssl == NULL) {
                ret = 0x80530104;
                goto fail;
            }
        }
    } else {
        sub_1910(ctx);
        ctx->ssl = sceSsl_lib_EBFB0E3C(ctx->ssl_ctx);
        if (ctx->ssl == NULL) {
            ret = 0x80530104;
            goto fail;
        }
    }

    bio = sceSsl_lib_28B4DE33(ctx->sock, 0);
    if (bio == NULL) {
        ret = 0x80530104;
        goto fail;
    }

    sceSsl_lib_B9C8CCE6(ctx->ssl, bio, bio);
    sceSsl_lib_80608663(ctx->ssl);
    return ret;

fail:
    if (ctx->ssl != NULL) {
        sub_1910(ctx);
    }
    return ret;
}

/* === sub_1c68 @ 0x1c68 === */
static int sub_1c68(DnasContext *ctx)
{
    int ret = 0;

    ret = sub_11b4(ctx);
    if (ret < 0) {
        goto fail;
    }

    if (ctx->use_proxy == 0) {
        ctx->port = 443;
        ret = sceDNASCore_lib_15096ECD(ctx->host, NULL, NULL);
        if (ret < 0) {
            goto fail;
        }
    }

    ret = sub_12e4(ctx);
    if (ret < 0) {
        goto fail;
    }

    if (ctx->use_proxy != 0) {
        ret = sub_2de0(ctx);
        if (ret < 0) {
            goto fail;
        }
    }

    ret = sub_1aa0(ctx);
    if (ret < 0) {
        goto fail;
    }

    return ret;

fail:
    sub_1d7c(ctx);
    return ret;
}

/* === sub_1d7c @ 0x1d7c === */
static int sub_1d7c(DnasContext *ctx)
{
    int ret = 0;

    if (ctx->sock >= 0) {
        ret = sceNetInetClose(ctx->sock);
        ctx->sock = -1;
    }
    return ret;
}

/* === sub_1de4 @ 0x1de4 === */
static void sub_1de4(void)
{
    int i;

    for (i = 0; i < 5; i++) {
        if (g_root_certs[i] != NULL) {
            sceSsl_lib_C1A2E14B(g_root_certs[i]);
        }
        g_root_certs[i] = NULL;
    }
}

/* === sub_1e90 @ 0x1e90 === */
static int sub_1e90(void *subj, const char *expected_cn)
{
    int count;
    int matched = 0;
    int i;
    int j;
    void *entry;
    int nid1;
    int nid2;
    int dummy_a8;
    void *val_ptr;
    int val_len;
    const char *attr_name;
    char cn_buf[0x80];

    count = sceSsl_lib_4C75B8E9(subj);
    if (g_ssl_initialized < 0) {
        return 0x80530182;
    }

    for (i = 0; i < count; i++) {
        entry = sceSsl_lib_5040E836(subj, i);
        if (sceSsl_lib_8D484361(entry, 0, &nid1, &nid2, &dummy_a8, &val_ptr, &val_len) != 0) {
            attr_name = sceSsl_lib_2B7BD166(nid1, nid2, 0);
            if (attr_name == NULL) {
                attr_name = "<UNKNOWN>";
            }
            if (sceNet_lib_A0F16ABD((char *)attr_name, "commonName") == 0) {
                if (val_len < 0x7f) {
                    sceKernelMemcpy(cn_buf, val_ptr, (SceSize)val_len);
                    cn_buf[val_len] = '\0';
                    if (sceNet_lib_D8722983(expected_cn, NULL) == val_len) {
                        if (sceNet_lib_9CFBC7E3(cn_buf, expected_cn, 0) == NULL) {
                            matched = 1;
                        }
                    }
                }
            }
            for (j = 0; j < val_len; j++) {
                /* empty loop from original */
            }
        }
    }

    return (matched == 1) ? 0 : -1;
}

/* === sub_2098 @ 0x2098 === */
static int sub_2098(int preverify_ok, void **cert_chain, int chain_len, void *arg, int *alert)
{
    DnasContext *ctx = (DnasContext *)arg;
    int depth;
    int i;
    int k;
    void *subj;
    void *root_subj;
    void *issuer;
    void *pubkey;
    uint32_t dummy_ac;
    int pathlen = 10;
    int is_ca;
    int ext_count;
    void *ext_ptr;
    int dummy_d4;
    int dummy_b8;
    char expected_cn[0x80];
    (void)preverify_ok;

    if (g_ssl_initialized < 0) {
        ctx->ssl_verify_err = 0x80530182;
        return 0;
    }

    if (chain_len == 0) {
        ctx->ssl_verify_err = 0x80530188;
        return 0;
    }

    for (depth = 0; depth < chain_len; depth++) {
        if (sceSsl_lib_9299C6CE(cert_chain[depth], &dummy_d4, &ext_ptr, &ext_count) != 0) {
            if (ext_ptr != NULL) {
                sceSsl_lib_5E5C873A(ext_ptr);
                ext_ptr = NULL;
            }
        }
        subj = sceSsl_lib_4BE9030A(cert_chain[depth]);
        if (subj == NULL) {
            *alert = 2;
        }
        sceSsl_lib_37C7B76C(cert_chain[depth], &dummy_b8, &ext_count);
    }

    subj = sceSsl_lib_12C7718F(cert_chain[0]);
    if (subj != NULL && ctx->skip_cn_check != 1) {
        sceDNASCore_lib_F479F616(expected_cn);
        if (sub_1e90(subj, expected_cn) < 0) {
            ctx->ssl_verify_err = 0x80530186;
            return 0;
        }
    }

    for (depth = 0; ; depth++) {
        subj = sceSsl_lib_4BE9030A(cert_chain[depth]);
        for (i = 0; i < g_root_cert_count; i++) {
            root_subj = sceSsl_lib_4BE9030A(g_root_certs[i]);
            if (subj != NULL && root_subj != NULL) {
                if (sceSsl_lib_7D0E41F4(root_subj, subj) == 0) {
                    goto found_root;
                }
            }
        }

        if (depth + 1 >= chain_len) {
            ctx->ssl_verify_err = 0x8053018b;
            return 0;
        }

        issuer = sceSsl_lib_12C7718F(cert_chain[depth + 1]);
        if (subj == NULL || issuer == NULL || sceSsl_lib_7D0E41F4(subj, issuer) != 0) {
            ctx->ssl_verify_err = 0x8053018a;
            return 0;
        }
    }

found_root:
    pubkey = sceSsl_lib_B87FFE37(g_root_certs[i]);
    if (pubkey == NULL) {
        ctx->ssl_verify_err = 0x80530104;
        *alert = 0x11;
        return 0;
    }

    if (sceSsl_lib_0FF7BA68(cert_chain[depth], pubkey) == 0) {
        ctx->ssl_verify_err = 0x80530189;
        return 0;
    }

    for (k = depth; k >= 0; k--) {
        if (k > 0) {
            if (sceSsl_lib_CFA2F8EB(cert_chain[k], &dummy_ac, &pathlen, &is_ca) == 1) {
                if (is_ca == 0) {
                    ctx->ssl_verify_err = 0x8053018a;
                } else if ((unsigned int)(pathlen + 1) < (unsigned int)k) {
                    return 0;
                }
            }
        }
    }

    *alert = 0;
    return 1;
}

/* === sub_25f0 @ 0x25f0 === */
static int sub_25f0(const char *pem, uint8_t *out_buf, int *out_len, unsigned int max_len)
{
    int ret = 0;
    int total = 0;
    int line_len = 0;
    int chunk_len = 0;
    int has_buf = (out_buf != NULL) ? 1 : 0;
    const char *p;
    const char *end;
    const char *lf;
    const char *cr;

    p = sceNet_lib_BCBE14CF(pem, '-');
    if (p == NULL) {
        return 0x8053018c;
    }
    p = sceNet_lib_BCBE14CF(p, '\n');
    if (p == NULL) {
        return 0x8053018c;
    }
    end = sceNet_lib_BCBE14CF(p, '-');
    if (end == NULL) {
        return 0x8053018c;
    }
    p++;

    while (p < end) {
        lf = sceNet_lib_BCBE14CF(p, '\n');
        if (lf == NULL) {
            return 0x8053018c;
        }
        cr = sceNet_lib_BCBE14CF(p, '\r');
        if (cr != NULL && cr < lf) {
            line_len = (int)(lf - p) - 1;
        } else {
            line_len = (int)(lf - p);
        }

        chunk_len = (line_len >> 2) * 3;
        if (has_buf == 1) {
            if (max_len < (unsigned int)(total + chunk_len)) {
                return 0x80530104;
            }
            ret = sub_3410(out_buf + total, p, line_len);
            if (ret < 0) {
                return ret;
            }
            chunk_len = ret;
        }
        total += chunk_len;
        p = lf + 1;
    }

    if (out_len != NULL) {
        if (has_buf == 1) {
            *out_len = total;
        } else {
            *out_len = total + 1;
        }
    }

    return 0;
}

/* === sub_28c8 @ 0x28c8 === */
static int sub_28c8(void)
{
    int ret = 0;
    int cert_size = 0;
    int der_len = 0;
    void *cert_buf = NULL;
    void *der_buf = NULL;
    void *x509 = NULL;
    void **cert_list = NULL;
    int i;
    void *der_ptr;

    ret = sceCertLoader_DD629A24(3, 0x1f, NULL, NULL, 0, &cert_size);
    if (ret < 0) {
        goto fail;
    }
    if (cert_size != 0x1bbc) {
        ret = 0x8053018d;
        goto fail;
    }

    cert_buf = sceSsl_lib_0861D934((unsigned int)cert_size);
    if (cert_buf == NULL) {
        ret = 0x80530104;
        goto fail;
    }

    ret = sceCertLoader_DD629A24(3, 0x1f, &cert_list, cert_buf, cert_size, &der_len);
    if (ret < 0 || cert_list == NULL) {
        goto fail;
    }
    if (ret != 5) {
        ret = 0x8053018e;
        goto fail;
    }

    g_root_cert_count = ret;
    sub_1de4();
    sceKernelMemset(g_root_certs, 0, (SceSize)(g_root_cert_count * sizeof(void *)));

    for (i = 0; cert_list != NULL; i++) {
        ret = sub_25f0((const char *)cert_list[0], NULL, &cert_size, 0);
        if (ret < 0) {
            goto fail;
        }

        der_buf = sceSsl_lib_0861D934((unsigned int)cert_size);
        if (der_buf == NULL) {
            ret = 0x80530104;
            goto fail;
        }

        ret = sub_25f0((const char *)cert_list[0], (uint8_t *)der_buf, &der_len, (unsigned int)cert_size);
        if (ret < 0) {
            goto fail;
        }

        der_ptr = der_buf;
        x509 = sceSsl_lib_6038DEDE(NULL, &der_ptr, der_len);
        if (x509 == NULL) {
            ret = 0x80530104;
            goto fail;
        }

        if (der_buf != NULL) {
            sceSsl_lib_5E5C873A(der_buf);
            der_buf = NULL;
        }

        g_root_certs[i] = x509;
        cert_list = (void **)cert_list[1];
    }

    if (cert_buf != NULL) {
        sceSsl_lib_5E5C873A(cert_buf);
    }
    return 0;

fail:
    sub_1de4();
    if (cert_buf != NULL) {
        sceSsl_lib_5E5C873A(cert_buf);
    }
    if (der_buf != NULL) {
        sceSsl_lib_5E5C873A(der_buf);
    }
    return ret;
}

/* === sub_2c08 @ 0x2c08 === */
static int sub_2c08(DnasContext *ctx)
{
    int ret = 0;

    if (g_ssl_initialized >= 0) {
        return 0x80530182;
    }

    if (ctx->ssl_ctx == NULL) {
        ctx->ssl_ctx = sceSsl_lib_FB8273FE(sceSsl_lib_B40D11EA());
        if (ctx->ssl_ctx == NULL) {
            return 0x80530104;
        }
        sceSsl_lib_B4D78E98(ctx->ssl_ctx, 0x20, 0x2000ffffU, 0);
        sceSsl_lib_AEBF278B(ctx->ssl_ctx, 5, NULL);
        sceSsl_lib_529A9477(ctx->ssl_ctx, sub_2098, ctx);
        sceSsl_lib_BF55C31C(ctx->ssl_ctx, 0);
    }

    ret = sub_28c8();
    if (ret >= 0) {
        g_ssl_initialized = 1;
    }
    return ret;
}

/* === sub_2d5c @ 0x2d5c === */
static void sub_2d5c(DnasContext *ctx)
{
    if (g_ssl_initialized >= 0) {
        sub_1de4();
        g_root_cert_count = 0;
        if (ctx->ssl_ctx != NULL) {
            sceSsl_lib_588F2FE8(ctx->ssl_ctx);
            ctx->ssl_ctx = NULL;
        }
        g_ssl_initialized = -1;
    }
}

/* === sub_2de0 @ 0x2de0 === */
static int sub_2de0(DnasContext *ctx)
{
    int ret = 0;
    int total = 0;
    char hdr[4];

    ret = sceDNASCore_lib_BA0D27F8();
    if (ret < 0) {
        return ret;
    }

    for (;;) {
        ret = sceDNASCore_lib_DA5939B4(g_io_buf, sizeof(g_io_buf));
        if (ret == 0) {
            break;
        }
        if (ret < 0) {
            return ret;
        }
        ret = sceNetInetSend(ctx->sock, g_io_buf, (unsigned int)ret, 0);
        if (ret < 0) {
            return sceNetInet_8CA3A97E();
        }
    }

    sceKernelMemset(hdr, 0, sizeof(hdr));
    do {
        ret = sceNetInetRecv(ctx->sock, g_io_buf, 1, 0);
        if (ret < 0) {
            return sceNetInet_8CA3A97E();
        }
        total += ret;
        ret = sceDNASCore_lib_C54657B7(g_io_buf, ret);
        if (ret == 0) {
            break;
        }
        if (ret < 0) {
            return ret;
        }
        if (hdr[2] == '\r' && hdr[1] == '\n' && hdr[0] == '\r' && g_io_buf[0] == '\n') {
            ret = sceDNASCore_lib_C54657B7(NULL, 0);
            if (ret < 0) {
                return ret;
            }
            break;
        }
        hdr[3] = hdr[2];
        hdr[2] = hdr[1];
        hdr[1] = hdr[0];
        hdr[0] = (char)g_io_buf[0];
    } while (total < 0x80);

    ret = sceDNASCore_lib_2370130E();
    if (ret < 0 && ret == (int)0x80530412) {
        ret = 0x80530101;
    }
    return ret;
}

/* === sub_3028 @ 0x3028 === */
static int sub_3028(DnasContext *ctx)
{
    int ret = 0;

    ret = sceDNASCore_lib_4108128B();
    if (ret < 0) {
        goto fail;
    }

    ret = sub_1c68(ctx);
    if (ret < 0) {
        goto fail;
    }

    for (;;) {
        ret = sceDNASCore_lib_F0EB4367(g_io_buf, sizeof(g_io_buf));
        if (ret == 0) {
            return 0;
        }
        if (ret < 0) {
            goto fail;
        }
        ret = sub_1700(ctx, g_io_buf, ret);
        if (ret < 0) {
            goto fail;
        }
    }

fail:
    sub_1d7c(ctx);
    return ret;
}

/* === sub_3118 @ 0x3118 === */
static int sub_3118(DnasContext *ctx, int *redirected)
{
    int ret = 0;
    char redirect_buf[0x80];

    for (;;) {
        ret = sub_1844(ctx, g_io_buf, sizeof(g_io_buf));
        if (ret < 0) {
            goto out;
        }
        ret = sceDNASCore_lib_26E1E2BD(g_io_buf, ret);
        if (ret == 0) {
            break;
        }
        if (ret < 0) {
            goto out;
        }
    }

    sceKernelMemset(redirect_buf, 0, sizeof(redirect_buf));
    ret = sceDNASCore_lib_B6C76A14(redirect_buf);
    if (ret < 0) {
        goto out;
    }

    if (redirect_buf[0] != '\0') {
        if (ctx->redirect_count > 0) {
            ret = 0x8053018f;
            goto out;
        }
        *redirected = 1;
        ctx->redirect_count++;
        ctx->resolved_ip = 0;
        sceKernelMemcpy(ctx->host, redirect_buf, sizeof(redirect_buf));
    }

out:
    sub_1d7c(ctx);
    return ret;
}

/* === sub_3270 @ 0x3270 === */
static int sub_3270(DnasContext *ctx)
{
    int ret = 0;

    ret = sceDNASCore_lib_80CEC43A();
    if (ret < 0) {
        goto fail;
    }

    ret = sub_1c68(ctx);
    if (ret < 0) {
        goto fail;
    }

    for (;;) {
        ret = sceDNASCore_lib_822357BB(g_io_buf, sizeof(g_io_buf));
        if (ret == 0) {
            return 0;
        }
        if (ret < 0) {
            goto fail;
        }
        ret = sub_1700(ctx, g_io_buf, ret);
        if (ret < 0) {
            goto fail;
        }
    }

fail:
    sub_1d7c(ctx);
    return ret;
}

/* === sub_3360 @ 0x3360 === */
static int sub_3360(DnasContext *ctx)
{
    int ret = 0;

    for (;;) {
        ret = sub_1844(ctx, g_io_buf, sizeof(g_io_buf));
        if (ret < 0) {
            goto out;
        }
        ret = sceDNASCore_lib_8309549E(g_io_buf, ret);
        if (ret == 0) {
            break;
        }
        if (ret < 0) {
            goto out;
        }
    }

    ret = sceDNASCore_lib_BA0A32CA();

out:
    sub_1d7c(ctx);
    return ret;
}

/* === sub_3410 @ 0x3410 === */
static int sub_3410(uint8_t *dst, const char *src, int len)
{
    int out_len = 0;

    if (len != 0) {
        do {
            uint8_t c0 = (uint8_t)src[0];
            uint8_t c1 = (uint8_t)src[1];
            uint8_t c2 = (uint8_t)src[2];
            uint8_t c3 = (uint8_t)src[3];
            int8_t v0, v1, v2, v3;
            uint32_t w;

            if (c2 == '=') {
                out_len--;
            }
            if (c3 == '=') {
                out_len--;
            }

            v0 = g_b64_table[c0];
            v1 = g_b64_table[c1];
            v2 = g_b64_table[c2];
            v3 = g_b64_table[c3];

            if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) {
                return -1;
            }

            w = ((uint32_t)v0 << 18) | ((uint32_t)v1 << 12) | ((uint32_t)v2 << 6) | (uint32_t)v3;
            dst[0] = (uint8_t)(w >> 16);
            dst[1] = (uint8_t)(w >> 8);
            dst[2] = (uint8_t)w;

            src += 4;
            len -= 4;
            dst += 3;
            out_len += 3;
        } while (len > 0);
    }

    *dst = 0;
    return out_len;
}
