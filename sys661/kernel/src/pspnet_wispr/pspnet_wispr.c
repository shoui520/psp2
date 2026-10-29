#include <common_imp.h>

extern void sceKernelMemset(void *dst, int val, u32 size);
extern int sceKernelCreateSema(const char *name, u32 attr, int initVal, int maxVal, void *opt);
extern int sceKernelDeleteSema(int semaid);
extern int sceKernelWaitSema(int semaid, int signal, u32 *timeout);
extern int sceKernelSignalSema(int semaid, int signal);
extern int sceKernelCreateEventFlag(const char *name, u32 attr, int bits, void *opt);
extern int sceKernelDeleteEventFlag(int evid);
extern int sceKernelSetEventFlag(int evid, u32 bits);
extern int sceKernelPollEventFlag(int evid, u32 bits, u32 wait, u32 *outBits);
extern int sceKernelWaitEventFlag(int evid, u32 bits, u32 wait, u32 *outBits, u32 *timeout);
extern int sceKernelCreateThread(const char *name, void *entry, int prio, int stackSize, u32 attr, void *opt);
extern int sceKernelStartThread(int thid, u32 arglen, void *argp);
extern int sceKernelWaitThreadEnd(int thid, u32 *timeout);
extern int sceKernelTerminateThread(int thid);
extern int sceKernelDeleteThread(int thid);

extern int sceHttpCreateTemplate(const char *agent, int ver, int autoProxy);
extern int sceHttpDeleteTemplate(int tmplId);
extern int sceHttp_B3FAF831(int unk0, int tmplId);
extern int sceHttpCreateConnectionWithURL(int tmplId, const char *url, int enableKeepalive);
extern int sceHttpDeleteConnection(int connId);
extern int sceHttpCreateRequestWithURL(int connId, int method, const char *url, u64 contentLength);
extern int sceHttpDeleteRequest(int reqId);
extern int sceHttpAddExtraHeader(int id, const char *name, const char *value, int mode);
extern int sceHttpSendRequest(int reqId, const void *data, u32 size);
extern int sceHttpAbortRequest(int reqId);
extern int sceHttpGetStatusCode(int reqId, int *statusCode);
extern int sceHttpGetAllHeader(int reqId, char **header, u32 *headerSize);
extern int sceHttpReadData(int reqId, void *data, u32 size);
extern int sceHttpGetNetworkErrno(int reqId, int *errNum);
extern int sceHttp_AB1540D5(int reqId, int *err1, int *err2);
extern int sceHttpEnableRedirect(int id);
extern int sceHttpDisableRedirect(int id);

extern int sceNetFreeThreadinfo(int thid);
extern int sceNetApctlGetInfo(int code, void *pInfo);
extern int sceNet_lib_2A73ADDC(const char *str, char **endptr, int base);
extern int sceNet_lib_9CFBC7E3(const char *s1, const char *s2);
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, u32 n);
extern int sceNet_lib_B9085A96(const char *s1, const char *s2, u32 n);
extern char *sceNet_lib_BCBE14CF(const char *s, int c);
extern int sceNet_lib_CF705E46(char *buf, const char *fmt, ...);
extern u32 sceNet_lib_D8722983(const char *s);

extern int sceParseHttp_AD7BFDEF(const char *header, u32 headerLen, const char *fieldName, const char **valPtr, u32 *valLen);
extern int sceParseUri_49E950EC(char *dst, const char *unk, u32 dstSize, const char *src);
extern int sceUtilityCheckNetParam(int id);
extern int sceUtility_28D35634(char *buf);
extern int sceUtility_EF3582B2(char *buf);
extern int sceUtility_netparam_internal_67C2105B(int id, int code, void *buf);
extern int sceWlanGetEtherAddr(u8 *etherAddr);

typedef struct WisprXmlEntry {
    int tag_id;
    char value[0x100];
} WisprXmlEntry;

typedef struct WisprXmlDoc {
    int root_tag;       /* 0x00 */
    int msg_tag;        /* 0x04 */
    int msg_type_id;    /* 0x08 */
    int msg_type_val;   /* 0x0c */
    int resp_code_id;   /* 0x10 */
    int resp_code_val;  /* 0x14 */
    WisprXmlEntry entries[7]; /* 0x18..0x733 */
} WisprXmlDoc; /* size 0x734 */

typedef struct WisprCtx {
    int state;              /* 0x000 */
    int sub_state;          /* 0x004 */
    int thid;               /* 0x008 */
    int evid;               /* 0x00c */
    int semaid;             /* 0x010 */
    int netparam_id;        /* 0x014 */
    int error_code;         /* 0x018 */
    u32 flags;              /* 0x01c */
    u32 unk_020;            /* 0x020 */
    char reply_msg[0x100];  /* 0x024 */
    char login_url[0x100];  /* 0x124 */
    char abort_url[0x100];  /* 0x224 */
    void (*callback)(int prev_state, int state, int sub_state, int err, void *info, void *arg); /* 0x324 */
    void *cb_arg;           /* 0x328 */
    u32 saved_gp;           /* 0x32c */
    char xml_buf[0x734];    /* 0x330 */
    int provider_type;      /* 0xa64 */
    char username[0x80];    /* 0xa68 */
    char password[0x80];    /* 0xae8 */
    u32 unk_b68;            /* 0xb68 */
} WisprCtx; /* size 0xb6c */

typedef struct WisprProviderOps {
    int (*setup_redirect)(WisprCtx *ctx, int tmpl_id);
    int (*get_init_url)(WisprCtx *ctx, const char **url);
    int (*check_init_status)(WisprCtx *ctx, int status, int *next_sub);
    int (*parse_init_reply)(WisprCtx *ctx, int req_id, int *next_sub, const char **login_url, const char **loc_ptr, u32 *loc_len);
    int (*build_login_body)(WisprCtx *ctx, char *buf, int max_len);
    int (*check_login_status)(WisprCtx *ctx, int status, int *next_sub);
    int (*parse_login_reply)(WisprCtx *ctx, int req_id, int *next_sub, void *info, const char **reply_msg, const char **loc_ptr, u32 *loc_len, const char **abort_url);
} WisprProviderOps;

static int g_initialized;           /* 0x2ea0 */
static WisprCtx g_ctx;              /* 0x2ea4..0x3a10 */
static int g_http_tmpl_id;          /* 0x3a10 */
static int g_http_conn_id;          /* 0x3a14 */
static int g_http_req_id;           /* 0x3a18 */
static char g_read_buf[0x100];      /* 0x3a1c */
static char g_logoff_urls[4][0x100];/* 0x3b1c */
static struct {
    char tag_name[0xff];            /* 0x000 */
    char val_buf[0x101];            /* 0x0ff */
    int depth;                      /* 0x200 */
    int state;                      /* 0x204 */
    u32 pos;                        /* 0x208 */
} g_xml_parser;                     /* 0x3f1c..0x4128 */
static WisprXmlDoc *g_xml_doc_ptr;  /* 0x4128 */
static int g_xml_tag_stack[3];      /* 0x412c */

static int sub_0124(void *cb, int stackSize, int prio, int unk3, void *cb_arg);
static int sub_0310(void);
static int sub_03d0(WisprCtx *ctx);
static int sub_04a8(int netparam_id);
static int sub_0530(int netparam_id);
static int sub_05b8(void);
static int sub_05f4(int prev_state);
static int sub_0698(WisprCtx *ctx, int prev_state, int *next_state, int *next_sub);
static int sub_0780(WisprCtx *ctx, int prev_state, int *next_state, int *next_sub);
static int sub_0860(WisprCtx *ctx, int prev_state, int *next_state, int *next_sub);
static int sub_0930(WisprCtx *ctx, int prev_state, int *next_state, int *next_sub);
static int sub_0a04(u32 arglen, void *argp);
static int sub_0aa0(WisprCtx *ctx, int *next_sub);
static int sub_0d40(WisprCtx *ctx, int *next_sub);
static int sub_10d8(WisprCtx *ctx, int *next_sub);
static int sub_12a0(WisprCtx *ctx);
static int sub_12fc(WisprCtx *ctx);
static int sub_1334(WisprCtx *ctx);
static int sub_1364(WisprCtx *ctx, int req_id, char *buf, u32 buf_size);
static int sub_1470(const char *data, int len);
static int sub_1798(void *doc_buf, u32 doc_size);
static int sub_1814(WisprXmlDoc **out_doc);
static int sub_1828(int *matched);
static int sub_191c(int tag_id);
static int sub_1a24(const char **out_login_url);
static int sub_1b00(int *out_sub, int *out_resp_code, const char **out_reply_msg, const char **out_logoff_url);
static int sub_1c74(int resp_code);
static int sub_1d44(WisprXmlDoc *doc, int expected_msg_tag, int expected_msg_type);
static int sub_1e40(int msg_tag);
static int sub_1ebc(WisprCtx *ctx, int req_id, int *next_sub, const char **login_url, const char **loc_ptr, u32 *loc_len);
static int sub_2038(WisprCtx *ctx, int tmpl_id);
static int sub_2054(WisprCtx *ctx, const char **url);
static int sub_2068(WisprCtx *ctx, int status, int *next_sub);
static int sub_209c(WisprCtx *ctx, char *buf, int max_len);
static int sub_2170(WisprCtx *ctx, int status, int *next_sub);
static int sub_21a4(WisprCtx *ctx, int req_id, int *next_sub, void *info, const char **reply_msg, const char **loc_ptr, u32 *loc_len, const char **abort_url);
static int sub_225c(WisprCtx *ctx, int req_id, int *next_sub, const char **login_url, const char **loc_ptr, u32 *loc_len);
static int sub_22f4(WisprCtx *ctx, char *buf, int max_len);
static int sub_23f8(WisprCtx *ctx, int tmpl_id);
static int sub_2414(WisprCtx *ctx, const char **url);
static int sub_2428(WisprCtx *ctx, int status, int *next_sub);
static int sub_243c(WisprCtx *ctx, int status, int *next_sub);
static int sub_2450(WisprCtx *ctx, int req_id, int *next_sub, void *info, const char **reply_msg, const char **loc_ptr, u32 *loc_len, const char **abort_url);

int sceNetWispr_757BDC9F(void);

static int (* const g_state_handlers[4])(WisprCtx *, int, int *, int *) = {
    sub_0698,
    sub_0780,
    sub_0860,
    sub_0930
};

static const char * const g_xml_tags[3][14] = {
    { "XXX", "WISPAccessGatewayParam", "", NULL },
    { "XXX", "Redirect", "Proxy", "AuthenticationReply", "AuthenticationPollReply", "LogoffReply", "AbortLoginReply", "", NULL },
    { "XXX", "AccessProcedure", "AccessLocation", "LocationName", "LoginURL", "AbortLoginURL", "MessageType", "ResponseCode", "NextURL", "Delay", "ReplyMessage", "LoginResultsURL", "LogoffURL", "" }
};

static const WisprProviderOps g_provider_ops[3] = {
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { sub_2038, sub_2054, sub_2068, sub_1ebc, sub_209c, sub_2170, sub_21a4 },
    { sub_23f8, sub_2414, sub_2428, sub_225c, sub_22f4, sub_243c, sub_2450 }
};

/* 0x0000 */
int sceNetWispr_BD745B6D(void *cb, int stackSize, int prio, int unk3, void *cb_arg)
{
    if (g_initialized == 1) {
        return 0x80411502;
    }
    int ret = sub_0124(cb, stackSize, prio, unk3, cb_arg);
    if (ret < 0) {
        return ret;
    }
    g_initialized = 1;
    return 0;
}

/* 0x005c */
int sceNetWispr_BC05EC73(void)
{
    if (g_initialized != 0) {
        sub_0310();
        g_initialized = 0;
    }
    return 0;
}

/* 0x0094 */
int sceNetWispr_19235F8C(int netparam_id)
{
    if (g_initialized == 0) {
        return 0x80411501;
    }
    return sub_04a8(netparam_id);
}

/* 0x00c4 */
int sceNetWispr_757BDC9F(void)
{
    if (g_initialized == 0) {
        return 0x80411501;
    }
    return sub_05b8();
}

/* 0x00f4 */
int sceNetWispr_C856AAAC(int netparam_id)
{
    if (g_initialized == 0) {
        return 0x80411501;
    }
    return sub_0530(netparam_id);
}

/* 0x0124 */
static int sub_0124(void *cb, int stackSize, int prio, int unk3, void *cb_arg)
{
    WisprCtx *ctx = &g_ctx;
    int ret = sub_12a0(ctx);
    if (ret < 0) {
        goto fail;
    }
    sceKernelMemset(ctx, 0, sizeof(WisprCtx));
    ctx->cb_arg = cb_arg;
    ctx->flags |= 1;
    ctx->thid = -1;
    ctx->callback = cb;
    ctx->semaid = -1;
    ctx->evid = -1;

    ret = sceKernelCreateSema("SceNetWispr", 0, 1, 1, NULL);
    if (ret < 0) {
        goto fail;
    }
    ctx->semaid = ret;

    ret = sceKernelCreateEventFlag("SceNetWispr", 0, 0, NULL);
    if (ret < 0) {
        goto fail;
    }
    ctx->evid = ret;

    u32 opt[2];
    sceKernelMemset(opt, 0, 8);
    opt[0] = 8;
    opt[1] = (u32)stackSize;

    ret = sceKernelCreateThread("SceNetWispr", sub_0a04, unk3, prio, 0, opt);
    if (ret < 0) {
        goto fail;
    }
    ctx->thid = ret;

    ret = sceKernelStartThread(ret, 4, &ctx);
    if (ret < 0) {
        goto fail;
    }
    return ret;

fail:
    if (ctx->thid >= 0) {
        sceKernelTerminateThread(ctx->thid);
        sceKernelDeleteThread(ctx->thid);
        sceNetFreeThreadinfo(ctx->thid);
        ctx->thid = -1;
    }
    if (ctx->evid >= 0) {
        sceKernelDeleteEventFlag(ctx->evid);
        ctx->evid = -1;
    }
    if (ctx->semaid >= 0) {
        sceKernelDeleteSema(ctx->semaid);
        ctx->semaid = -1;
    }
    sub_12fc(ctx);
    return ret;
}

/* 0x0310 */
static int sub_0310(void)
{
    WisprCtx *ctx = &g_ctx;
    if ((ctx->flags & 1) == 0) {
        sceNetWispr_757BDC9F();
    }
    sceKernelSetEventFlag(ctx->evid, 8);
    if (ctx->thid >= 0) {
        int ret = sceKernelWaitThreadEnd(ctx->thid, NULL);
        if (ret < 0) {
            return ret;
        }
        sceKernelDeleteThread(ctx->thid);
        sceNetFreeThreadinfo(ctx->thid);
        ctx->thid = -1;
    }
    if (ctx->evid >= 0) {
        sceKernelDeleteEventFlag(ctx->evid);
        ctx->evid = -1;
    }
    if (ctx->semaid >= 0) {
        sceKernelDeleteSema(ctx->semaid);
        ctx->semaid = -1;
    }
    return sub_12fc(ctx);
}

/* 0x03d0 */
static int sub_03d0(WisprCtx *ctx)
{
    u8 buf[0x80];
    int ret = sceUtility_netparam_internal_67C2105B(ctx->netparam_id, 0x18, buf);
    if (ret < 0) {
        return ret;
    }
    int type = *(int *)buf;
    if ((u32)(type - 1) >= 3) {
        return 0x80411506;
    }
    ctx->provider_type = type;

    ret = sceUtility_netparam_internal_67C2105B(ctx->netparam_id, 0x19, buf);
    if (ret < 0) {
        return ret;
    }
    sceNet_lib_B5CE388A(ctx->username, (const char *)buf, 0x80);
    ctx->username[0x7f] = 0;

    ret = sceUtility_netparam_internal_67C2105B(ctx->netparam_id, 0x1a, buf);
    if (ret < 0) {
        return ret;
    }
    sceNet_lib_B5CE388A(ctx->password, (const char *)buf, 0x80);
    ctx->password[0x7f] = 0;

    ret = sceUtility_netparam_internal_67C2105B(ctx->netparam_id, 0x1b, buf);
    if (ret < 0) {
        return ret;
    }
    ctx->unk_b68 = *(u32 *)buf;
    return ret;
}

/* 0x04a8 */
static int sub_04a8(int netparam_id)
{
    WisprCtx *ctx = &g_ctx;
    int ret = 0x80411503;
    if (ctx->flags & 1) {
        ctx->flags &= ~1;
        ret = sceUtilityCheckNetParam(netparam_id);
        if (ret < 0) {
            ctx->flags |= 1;
        } else {
            ctx->netparam_id = netparam_id;
            sceKernelSetEventFlag(ctx->evid, 1);
        }
    }
    return ret;
}

/* 0x0530 */
static int sub_0530(int netparam_id)
{
    WisprCtx *ctx = &g_ctx;
    int ret = 0x80411503;
    if (ctx->flags & 1) {
        ctx->flags &= ~1;
        ret = sceUtilityCheckNetParam(netparam_id);
        if (ret < 0) {
            ctx->flags |= 1;
        } else {
            ctx->netparam_id = netparam_id;
            sceKernelSetEventFlag(ctx->evid, 0x10);
        }
    }
    return ret;
}

/* 0x05b8 */
static int sub_05b8(void)
{
    WisprCtx *ctx = &g_ctx;
    sub_1334(ctx);
    sceKernelSetEventFlag(ctx->evid, 2);
    return 0;
}

/* 0x05f4 */
static int sub_05f4(int prev_state)
{
    WisprCtx *ctx = &g_ctx;
    sceKernelWaitSema(ctx->semaid, 1, NULL);
    if (ctx->callback != NULL) {
        u32 saved_gp;
        __asm__ volatile ("move %0, $gp" : "=r"(saved_gp));
        __asm__ volatile ("move $gp, %0" : : "r"(ctx->saved_gp));
        int err = (ctx->sub_state == 4) ? ctx->error_code : 0;
        ctx->callback(prev_state, ctx->state, ctx->sub_state, err, &ctx->unk_020, ctx->cb_arg);
        __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
    }
    sceKernelSignalSema(ctx->semaid, 1);
    return 0;
}

/* 0x0698 */
static int sub_0698(WisprCtx *ctx, int prev_state, int *next_state, int *next_sub)
{
    ctx->error_code = 0;
    sceKernelMemset(&ctx->provider_type, 0, 0x108);
    sceKernelMemset(&ctx->unk_020, 0, 0x304);

    u32 out_bits = 0;
    sceKernelWaitEventFlag(ctx->evid, 0xffffffff, 0x21, &out_bits, NULL);
    if (out_bits & 8) {
        ctx->flags |= 2;
    } else if (out_bits & 2) {
        *next_state = 0;
        *next_sub = 3;
        ctx->flags |= 1;
    } else if (out_bits & 1) {
        *next_state = 1;
        *next_sub = 0;
    } else if (out_bits & 0x10) {
        *next_state = 3;
        *next_sub = 5;
    }
    return 0;
}

/* 0x0780 */
static int sub_0780(WisprCtx *ctx, int prev_state, int *next_state, int *next_sub)
{
    int sub = 0;
    int ret = sub_03d0(ctx);
    if (ret < 0) {
        goto err_out;
    }
    sub_0aa0(ctx, &sub);
    ret = sceKernelPollEventFlag(ctx->evid, 2, 0x21, NULL);
    if (ret == 0) {
        *next_state = 0;
        *next_sub = 3;
    } else if (ret < 0) {
        goto err_out;
    } else if (sub == 1) {
        *next_state = 0;
        *next_sub = 3;
    } else if (sub == 2) {
        *next_state = 0;
        *next_sub = 3;
    } else {
        *next_state = 2;
        *next_sub = 1;
    }
    goto done;

err_out:
    *next_state = 0;
    *next_sub = 4;
    ctx->error_code = ret;

done:
    if (*next_state == 0) {
        ctx->flags |= 1;
    }
    return 0;
}

/* 0x0860 */
static int sub_0860(WisprCtx *ctx, int prev_state, int *next_state, int *next_sub)
{
    int sub = 0;
    sub_0d40(ctx, &sub);
    int ret = sceKernelPollEventFlag(ctx->evid, 2, 0x21, NULL);
    if (ret == 0) {
        *next_state = 0;
        *next_sub = 3;
    } else if (ret < 0) {
        goto err_out;
    } else if (sub == 1) {
        *next_state = 0;
        *next_sub = 3;
    } else if (sub == 3) {
        ret = 0x80411513;
        goto err_out;
    } else {
        *next_state = 0;
        *next_sub = 2;
    }
    goto done;

err_out:
    *next_state = 0;
    *next_sub = 4;
    ctx->error_code = ret;

done:
    if (*next_state == 0) {
        ctx->flags |= 1;
    }
    return 0;
}

/* 0x0930 */
static int sub_0930(WisprCtx *ctx, int prev_state, int *next_state, int *next_sub)
{
    int sub = 0;
    int ret = sub_03d0(ctx);
    if (ret < 0) {
        goto err_out;
    }
    sub_10d8(ctx, &sub);
    ret = sceKernelPollEventFlag(ctx->evid, 2, 0x21, NULL);
    if (ret == 0 || (ret >= 0 && sub == 1)) {
        *next_state = 0;
        *next_sub = 3;
    } else if (ret < 0) {
        goto err_out;
    } else {
        *next_state = 0;
        *next_sub = 6;
    }
    goto done;

err_out:
    *next_state = 0;
    *next_sub = 4;
    ctx->error_code = ret;

done:
    if (*next_state == 0) {
        ctx->flags |= 1;
    }
    return 0;
}

/* 0x0a04 */
static int sub_0a04(u32 arglen, void *argp)
{
    WisprCtx *ctx = *(WisprCtx **)argp;
    int prev_state = 0;
    for (;;) {
        int next_state, next_sub;
        g_state_handlers[ctx->state](ctx, prev_state, &next_state, &next_sub);
        prev_state = ctx->state;
        if (prev_state == 0 && (ctx->flags & 2)) {
            break;
        }
        ctx->state = next_state;
        ctx->sub_state = next_sub;
        sub_05f4(prev_state);
    }
    return 0;
}

/* 0x0aa0 */
static int sub_0aa0(WisprCtx *ctx, int *next_sub)
{
    const WisprProviderOps *ops = &g_provider_ops[ctx->provider_type];
    const char *url = NULL;
    int status = 0;
    const char *login_url = NULL;
    const char *loc_ptr = NULL;
    u32 loc_len = 0;

    int ret = ops->setup_redirect(ctx, g_http_tmpl_id);
    if (ret < 0) {
        goto cleanup;
    }
    ret = ops->get_init_url(ctx, &url);
    if (ret < 0) {
        goto cleanup;
    }
    ret = sceHttpCreateConnectionWithURL(g_http_tmpl_id, url, 1);
    if (ret < 0) {
        goto cleanup;
    }
    g_http_conn_id = ret;

    ret = sceHttpCreateRequestWithURL(ret, 0, url, 0);
    if (ret < 0) {
        goto cleanup;
    }
    g_http_req_id = ret;

    ret = sceHttpSendRequest(ret, NULL, 0);
    if (ret < 0) {
        if (ret == (int)0x80431063) {
            int errNum = 0;
            if (sceHttpGetNetworkErrno(g_http_req_id, &errNum) >= 0) {
                ret = 0x80010000 | (errNum & 0xff);
            } else {
                ret = 0x80431063;
            }
        } else if (ret == (int)0x80431075) {
            int err1 = 0, err2 = 0;
            int r2 = sceHttp_AB1540D5(g_http_req_id, &err1, &err2);
            ret = (r2 >= 0) ? err1 : r2;
        }
        goto cleanup;
    }

    ret = sceHttpGetStatusCode(g_http_req_id, &status);
    if (ret < 0) {
        goto cleanup;
    }
    ret = ops->check_init_status(ctx, status, next_sub);
    if (ret < 0) {
        goto cleanup;
    }
    if (*next_sub == 2) {
        ret = 0;
        goto cleanup;
    }

    ret = sub_1364(ctx, g_http_req_id, g_read_buf, 0x100);
    if (ret < 0) {
        goto cleanup;
    }
    ret = ops->parse_init_reply(ctx, g_http_req_id, next_sub, &login_url, &loc_ptr, &loc_len);
    if (ret < 0) {
        goto cleanup;
    }
    if (*next_sub == 2) {
        ret = 0;
        if (loc_ptr != NULL) {
            if (loc_len >= 0x100) {
                loc_len = 0xff;
            }
            sceNet_lib_B5CE388A(ctx->login_url, loc_ptr, loc_len);
            ctx->login_url[loc_len] = 0;
        } else {
            sceKernelMemset(ctx->login_url, 0, 0x100);
        }
    } else {
        sceNet_lib_B5CE388A(ctx->login_url, login_url, 0x100);
        ctx->login_url[0xff] = 0;
    }

cleanup:
    if (g_http_req_id > 0) {
        sceHttpDeleteRequest(g_http_req_id);
        g_http_req_id = 0;
    }
    if (g_http_conn_id > 0) {
        sceHttpDeleteConnection(g_http_conn_id);
        g_http_conn_id = 0;
    }
    return ret;
}

/* 0x0d40 */
static int sub_0d40(WisprCtx *ctx, int *next_sub)
{
    int status = 0;
    const char *reply_msg = NULL;
    const char *loc_ptr = NULL;
    u32 loc_len = 0;
    const char *abort_url = NULL;

    u32 len = sceNet_lib_D8722983("UserName");
    len += sceNet_lib_D8722983("Password");
    len += sceNet_lib_D8722983("button");
    len += sceNet_lib_D8722983("Login");
    len += sceNet_lib_D8722983("FNAME");
    len += sceNet_lib_D8722983(NULL);
    len += sceNet_lib_D8722983("OriginatingServer");
    if (len + 0x209 >= 0x735) {
        int ret = 0x80411504;
        goto cleanup_only;
    }

    const WisprProviderOps *ops = &g_provider_ops[ctx->provider_type];
    int body_len = ops->build_login_body(ctx, ctx->xml_buf, 0x734);
    if (body_len < 0) {
        int ret = body_len;
        goto cleanup;
    }

    int ret = sceHttpCreateConnectionWithURL(g_http_tmpl_id, ctx->login_url, 1);
    if (ret < 0) {
        goto cleanup;
    }
    g_http_conn_id = ret;

    ret = sceHttpCreateRequestWithURL(ret, 1, ctx->login_url, (u32)body_len);
    if (ret < 0) {
        goto cleanup;
    }
    g_http_req_id = ret;

    ret = sceHttpAddExtraHeader(ret, "Content-Type", "application/x-www-form-urlencoded", 0);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sceHttpSendRequest(g_http_req_id, ctx->xml_buf, body_len);
    if (ret < 0) {
        if (ret == (int)0x80431063) {
            int errNum = 0;
            if (sceHttpGetNetworkErrno(g_http_req_id, &errNum) >= 0) {
                ret = 0x80010000 | (errNum & 0xff);
            } else {
                ret = 0x80431063;
            }
        } else if (ret == (int)0x80431075) {
            int err1 = 0, err2 = 0;
            int r2 = sceHttp_AB1540D5(g_http_req_id, &err1, &err2);
            ret = (r2 >= 0) ? err1 : r2;
        }
        goto cleanup;
    }

    ret = sceHttpGetStatusCode(g_http_req_id, &status);
    if (ret < 0) {
        goto cleanup;
    }
    ret = ops->check_login_status(ctx, status, next_sub);
    if (ret < 0) {
        goto cleanup;
    }
    if (*next_sub == 2) {
        ret = 0;
        goto cleanup;
    }

    ret = sub_1364(ctx, g_http_req_id, g_read_buf, 0x100);
    if (ret < 0) {
        goto cleanup;
    }
    ret = ops->parse_login_reply(ctx, g_http_req_id, next_sub, &ctx->unk_020, &reply_msg, &loc_ptr, &loc_len, &abort_url);
    if (ret < 0) {
        goto cleanup;
    }

    if (reply_msg != NULL) {
        sceNet_lib_B5CE388A(ctx->reply_msg, reply_msg, 0x100);
        ctx->reply_msg[0xff] = 0;
    } else if (*next_sub == 3) {
        sceNet_lib_B5CE388A(ctx->reply_msg, "Authentication Failure.", 0x100);
        ctx->reply_msg[0xff] = 0;
    } else {
        sceKernelMemset(ctx->reply_msg, 0, 0x100);
    }

    if (loc_ptr != NULL) {
        if (loc_len >= 0x100) {
            loc_len = 0xff;
        }
        sceNet_lib_B5CE388A(ctx->login_url, loc_ptr, loc_len);
        ctx->login_url[loc_len] = 0;
    } else {
        sceKernelMemset(ctx->login_url, 0, 0x100);
    }

    if (abort_url != NULL) {
        sceNet_lib_B5CE388A(ctx->abort_url, abort_url, 0x100);
        ctx->abort_url[0xff] = 0;
    } else {
        sceKernelMemset(ctx->abort_url, 0, 0x100);
    }

cleanup:
cleanup_only:
    if (g_http_req_id > 0) {
        sceHttpDeleteRequest(g_http_req_id);
        g_http_req_id = 0;
    }
    if (g_http_conn_id > 0) {
        sceHttpDeleteConnection(g_http_conn_id);
        g_http_conn_id = 0;
    }
    return ret;
}

/* 0x10d8 */
static int sub_10d8(WisprCtx *ctx, int *next_sub)
{
    int status = 0;
    char *logoff_url = g_logoff_urls[ctx->provider_type];
    if (sceNet_lib_D8722983(logoff_url) == 0) {
        int ret = 0x80411507;
        goto cleanup;
    }

    const WisprProviderOps *ops = &g_provider_ops[ctx->provider_type];
    int ret = ops->setup_redirect(ctx, g_http_tmpl_id);
    if (ret < 0) {
        goto cleanup;
    }
    ret = sceHttpCreateConnectionWithURL(g_http_tmpl_id, logoff_url, 1);
    if (ret < 0) {
        goto cleanup;
    }
    g_http_conn_id = ret;

    ret = sceHttpCreateRequestWithURL(ret, 0, logoff_url, 0);
    if (ret < 0) {
        goto cleanup;
    }
    g_http_req_id = ret;

    ret = sceHttpSendRequest(ret, NULL, 0);
    if (ret < 0) {
        if (ret == (int)0x80431063) {
            int errNum = 0;
            if (sceHttpGetNetworkErrno(g_http_req_id, &errNum) >= 0) {
                ret = 0x80010000 | (errNum & 0xff);
            } else {
                ret = 0x80431063;
            }
        } else if (ret == (int)0x80431075) {
            int err1 = 0, err2 = 0;
            int r2 = sceHttp_AB1540D5(g_http_req_id, &err1, &err2);
            ret = (r2 >= 0) ? err1 : r2;
        }
        goto cleanup;
    }

    ret = sceHttpGetStatusCode(g_http_req_id, &status);
    if (ret < 0) {
        goto cleanup;
    }
    ret = sub_1364(ctx, g_http_req_id, g_read_buf, 0x100);

cleanup:
    if (g_http_req_id > 0) {
        sceHttpDeleteRequest(g_http_req_id);
        g_http_req_id = 0;
    }
    if (g_http_conn_id > 0) {
        sceHttpDeleteConnection(g_http_conn_id);
        g_http_conn_id = 0;
    }
    return ret;
}

/* 0x12a0 */
static int sub_12a0(WisprCtx *ctx)
{
    sceKernelMemset(g_logoff_urls, 0, 0x400);
    int ret = sceHttpCreateTemplate("WISPr/0.1 (PSP (PlayStation Portable); 3.00)", 0, 0);
    if (ret >= 0) {
        g_http_tmpl_id = ret;
        ret = sceHttp_B3FAF831(0x3a, ret);
    }
    return ret;
}

/* 0x12fc */
static int sub_12fc(WisprCtx *ctx)
{
    if (g_http_tmpl_id > 0) {
        sceHttpDeleteTemplate(g_http_tmpl_id);
        g_http_tmpl_id = 0;
    }
    return 0;
}

/* 0x1334 */
static int sub_1334(WisprCtx *ctx)
{
    if (g_http_req_id > 0) {
        sceHttpAbortRequest(g_http_req_id);
    }
    return 0;
}

/* 0x1364 */
static int sub_1364(WisprCtx *ctx, int req_id, char *buf, u32 buf_size)
{
    int eof = 0;
    int ret = sub_1798(ctx->xml_buf, 0x734);
    if (ret < 0) {
        return ret;
    }
    while (eof != 1) {
        int n = sceHttpReadData(req_id, buf, buf_size);
        if (n < 0) {
            if (n == (int)0x80431063) {
                int errNum = 0;
                if (sceHttpGetNetworkErrno(req_id, &errNum) >= 0) {
                    n = 0x80010000 | (errNum & 0xff);
                } else {
                    n = 0x80431063;
                }
            } else if (n == (int)0x80431075) {
                int err1 = 0, err2 = 0;
                int r2 = sceHttp_AB1540D5(req_id, &err1, &err2);
                n = (r2 >= 0) ? err1 : r2;
            }
            return n;
        }
        if (n == 0) {
            eof = 1;
        }
        ret = sub_1470(buf, n);
        if (ret < 0) {
            return ret;
        }
    }
    return ret;
}

/* 0x1470 */
static int sub_1470(const char *data, int len)
{
    const char *p = data;
    int state = g_xml_parser.state;
    int idx = 0;

    while (idx < len && (u32)state < 5) {
        switch (state) {
        case 0:
            if (*p == '<') {
                g_xml_parser.pos = 0;
                sceKernelMemset(g_xml_parser.tag_name, 0, 0xff);
                state = 1;
            }
            p++;
            idx = p - data;
            break;

        case 1:
            if (*p == '/' && g_xml_parser.pos == 0) {
                p++;
                idx = p - data;
                state = 4;
                break;
            }
            /* fallthrough */
        case 4:
            if (g_xml_parser.pos < 0xff) {
                char c = *p;
                if (c != ' ' && c != '\t' && c != '<' && c != '>') {
                    g_xml_parser.tag_name[g_xml_parser.pos++] = c;
                    p++;
                    idx = p - data;
                    break;
                }
            }
            g_xml_parser.tag_name[g_xml_parser.pos] = 0;
            if (state == 1) {
                int matched = 0;
                if ((u32)g_xml_parser.depth < 3) {
                    const char * const *table = g_xml_tags[g_xml_parser.depth];
                    int i = 1;
                    while (sceNet_lib_D8722983(table[i]) != 0) {
                        if (sceNet_lib_9CFBC7E3(table[i], g_xml_parser.tag_name) == 0) {
                            break;
                        }
                        i++;
                    }
                    if (sceNet_lib_D8722983(table[i]) != 0) {
                        matched = 1;
                        g_xml_tag_stack[g_xml_parser.depth] = i;
                    }
                }
                state = matched ? 2 : 0;
                idx = p - data;
            } else if (state == 4) {
                int matched = 0;
                sub_1828(&matched);
                if (matched == 1) {
                    g_xml_parser.depth--;
                }
                state = 0;
                p++;
                idx = p - data;
            } else {
                p++;
                idx = p - data;
            }
            break;

        case 2:
            if (*p == '>') {
                int d = g_xml_parser.depth + 1;
                g_xml_parser.depth = d;
                if (d == 3) {
                    g_xml_parser.pos = 0;
                    sceKernelMemset(g_xml_parser.tag_name, 0, 0xff);
                    state = 3;
                    p++;
                    idx = p - data;
                    break;
                }
                state = 0;
            }
            p++;
            idx = p - data;
            break;

        case 3:
            if (g_xml_parser.pos < 0xff && *p != '<') {
                g_xml_parser.val_buf[g_xml_parser.pos++] = *p;
                p++;
                idx = p - data;
            } else {
                g_xml_parser.val_buf[g_xml_parser.pos] = 0;
                g_xml_parser.pos = 0;
                sceKernelMemset(g_xml_parser.tag_name, 0, 0xff);
                state = 1;
                p++;
                idx = p - data;
            }
            break;
        }
    }
    g_xml_parser.state = state;
    return 0;
}

/* 0x1798 */
static int sub_1798(void *doc_buf, u32 doc_size)
{
    if (doc_size < 0x734) {
        return 0x80411505;
    }
    g_xml_doc_ptr = (WisprXmlDoc *)doc_buf;
    sceKernelMemset(&g_xml_parser, 0, sizeof(g_xml_parser));
    sceKernelMemset(g_xml_doc_ptr, 0, sizeof(WisprXmlDoc));
    sceKernelMemset(g_xml_tag_stack, 0, sizeof(g_xml_tag_stack));
    return 0;
}

/* 0x1814 */
static int sub_1814(WisprXmlDoc **out_doc)
{
    *out_doc = g_xml_doc_ptr;
    return 0;
}

/* 0x1828 */
static int sub_1828(int *matched)
{
    int d = g_xml_parser.depth;
    if ((u32)(d - 1) >= 3) {
        *matched = 0;
        return 0;
    }
    int tag_id = g_xml_tag_stack[d - 1];
    if (sceNet_lib_9CFBC7E3(g_xml_tags[d - 1][tag_id], g_xml_parser.tag_name) != 0) {
        *matched = 0;
        return 0;
    }
    *matched = 1;
    int idx = g_xml_parser.depth - 1;
    if (idx == 0) {
        g_xml_doc_ptr->root_tag = tag_id;
    } else if (idx == 1) {
        g_xml_doc_ptr->msg_tag = tag_id;
    } else if (idx == 2) {
        sub_191c(tag_id);
    }
    return 0;
}

/* 0x191c */
static int sub_191c(int tag_id)
{
    if ((u32)tag_id >= 13) {
        return 0;
    }
    if (tag_id == 6) {
        g_xml_doc_ptr->msg_type_id = tag_id;
        g_xml_doc_ptr->msg_type_val = sceNet_lib_2A73ADDC(g_xml_parser.val_buf, NULL, 10);
    } else if (tag_id == 7) {
        g_xml_doc_ptr->resp_code_id = tag_id;
        g_xml_doc_ptr->resp_code_val = sceNet_lib_2A73ADDC(g_xml_parser.val_buf, NULL, 10);
    } else if (tag_id >= 1 && tag_id <= 12) {
        int i;
        for (i = 0; i < 7; i++) {
            if (g_xml_doc_ptr->entries[i].tag_id == 0) {
                break;
            }
        }
        if (i < 7) {
            g_xml_doc_ptr->entries[i].tag_id = tag_id;
            sceNet_lib_B5CE388A(g_xml_doc_ptr->entries[i].value, g_xml_parser.val_buf, 0xff);
            g_xml_doc_ptr->entries[i].value[0xfe] = 0;
        }
    }
    return 0;
}

/* 0x1a24 */
static int sub_1a24(const char **out_login_url)
{
    *out_login_url = NULL;
    WisprXmlDoc *doc = NULL;
    int ret = sub_1814(&doc);
    if (ret < 0) {
        return ret;
    }
    ret = sub_1d44(doc, 1, 100);
    if (ret < 0) {
        return ret;
    }
    if (doc->resp_code_val != 0) {
        return sub_1c74(doc->resp_code_val);
    }
    int found = 0;
    for (int i = 0; i < 7; i++) {
        if (doc->entries[i].tag_id == 4) {
            *out_login_url = doc->entries[i].value;
            found = 1;
            break;
        }
    }
    return found ? 0 : 0x80411539;
}

/* 0x1b00 */
static int sub_1b00(int *out_sub, int *out_resp_code, const char **out_reply_msg, const char **out_logoff_url)
{
    *out_reply_msg = NULL;
    *out_logoff_url = NULL;
    WisprXmlDoc *doc = NULL;
    int ret = sub_1814(&doc);
    if (ret < 0) {
        return ret;
    }
    ret = sub_1d44(doc, 3, 120);
    if (ret < 0) {
        return ret;
    }
    int found_reply = 0;
    for (int i = 0; i < 7; i++) {
        if (doc->entries[i].tag_id == 10) {
            *out_reply_msg = doc->entries[i].value;
            found_reply = 1;
            break;
        }
    }
    if (!found_reply) {
        *out_reply_msg = NULL;
    }
    *out_resp_code = doc->resp_code_val;
    if (doc->resp_code_val != 50) {
        *out_sub = 3;
        return 0;
    }
    int found_logoff = 0;
    for (int i = 0; i < 7; i++) {
        if (doc->entries[i].tag_id == 12) {
            *out_logoff_url = doc->entries[i].value;
            found_logoff = 1;
            break;
        }
    }
    if (!found_logoff) {
        *out_logoff_url = NULL;
    }
    return 0;
}

/* 0x1c74 */
static int sub_1c74(int resp_code)
{
    switch (resp_code) {
    case 0:   return 0x8041152e;
    case 50:  return 0x8041152f;
    case 100: return 0x80411530;
    case 102: return 0x80411531;
    case 105: return 0x80411532;
    case 150: return 0x80411533;
    case 151: return 0x80411534;
    case 200: return 0x80411535;
    case 201: return 0x80411536;
    case 255: return 0x80411537;
    default:  return 0x80411538;
    }
}

/* 0x1d44 */
static int sub_1d44(WisprXmlDoc *doc, int expected_msg_tag, int expected_msg_type)
{
    if (doc->root_tag != 1) {
        return 0x80411512;
    }
    if (doc->msg_tag != expected_msg_tag) {
        return sub_1e40(doc->msg_tag);
    }
    if (doc->msg_type_id != 6) {
        return 0x8041153a;
    }
    if (doc->msg_type_val == expected_msg_type) {
        return (doc->resp_code_id == 7) ? 0 : 0x8041153b;
    }
    switch (doc->msg_type_val) {
    case 100: return 0x80411527;
    case 110: return 0x80411528;
    case 120: return 0x80411529;
    case 130: return 0x8041152a;
    case 140: return 0x8041152b;
    case 150: return 0x8041152c;
    default:  return 0x8041152d;
    }
}

/* 0x1e40 */
static int sub_1e40(int msg_tag)
{
    switch (msg_tag) {
    case 1:  return 0x80411520;
    case 2:  return 0x80411521;
    case 3:  return 0x80411522;
    case 4:  return 0x80411523;
    case 5:  return 0x80411524;
    case 6:  return 0x80411525;
    default: return 0x80411526;
    }
}

/* 0x1ebc */
static int sub_1ebc(WisprCtx *ctx, int req_id, int *next_sub, const char **login_url, const char **loc_ptr, u32 *loc_len)
{
    *login_url = NULL;
    *loc_ptr = NULL;
    int ret = sub_1a24(login_url);
    if (ret == (int)0x80411512) {
        *loc_ptr = NULL;
        char *hdr = NULL;
        u32 hdr_len = 0;
        ret = sceHttpGetAllHeader(req_id, &hdr, &hdr_len);
        if (ret >= 0) {
            ret = sceParseHttp_AD7BFDEF(hdr, hdr_len, "Location", loc_ptr, loc_len);
        }
        if (ret >= 0) {
            *next_sub = 2;
        }
        return ret;
    }
    if (ret < 0) {
        return ret;
    }
    const char *https = "https://";
    const char *u = *login_url;
    if (sceNet_lib_B9085A96(https, u, sceNet_lib_D8722983(https)) != 0) {
        return 0x80411514;
    }
    u += sceNet_lib_D8722983(https);
    const char *slash = sceNet_lib_BCBE14CF(u, '/');
    if (slash == NULL) {
        slash = u + sceNet_lib_D8722983(u);
    }
    const char *domain = ".hotspot.t-mobile.com";
    if ((int)(slash - u) < (int)sceNet_lib_D8722983(domain)) {
        return 0x80411515;
    }
    u32 dlen = sceNet_lib_D8722983(domain);
    if (sceNet_lib_B9085A96(slash - dlen, domain, dlen) != 0) {
        return 0x80411516;
    }
    return 0;
}

/* 0x2038 */
static int sub_2038(WisprCtx *ctx, int tmpl_id)
{
    return sceHttpDisableRedirect(tmpl_id);
}

/* 0x2054 */
static int sub_2054(WisprCtx *ctx, const char **url)
{
    *url = "http://welcome.hotspot.t-mobile.com/";
    return 0;
}

/* 0x2068 */
static int sub_2068(WisprCtx *ctx, int status, int *next_sub)
{
    if (status == 200) {
        *next_sub = 2;
        return 0;
    }
    if (status == 302) {
        return 0;
    }
    return 0x80411510;
}

/* 0x209c */
static int sub_209c(WisprCtx *ctx, char *buf, int max_len)
{
    char ssid[0x80];
    u8 mac[8];
    int n1 = sceNet_lib_CF705E46(buf, "UserName=%s&", ctx->username);
    char *p = buf + n1;
    int ret = sceNetApctlGetInfo(8, ssid);
    if (ret < 0) {
        return ret;
    }
    ret = sceWlanGetEtherAddr(mac);
    if (ret < 0) {
        return ret;
    }
    int n2 = sceNet_lib_CF705E46(p, "Password=0101/C/%s/%02x%02x%02x%02x%02x%02x/%s&",
                                 ssid, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ctx->password);
    p += n2;
    int n3 = sceNet_lib_CF705E46(p, "button=Login&FNAME=0&OriginatingServer=%s", "http://welcome.hotspot.t-mobile.com/");
    return (p + n3) - buf;
}

/* 0x2170 */
static int sub_2170(WisprCtx *ctx, int status, int *next_sub)
{
    if (status == 200) {
        *next_sub = 2;
        return 0;
    }
    if (status == 302) {
        return 0;
    }
    return 0x80411511;
}

/* 0x21a4 */
static int sub_21a4(WisprCtx *ctx, int req_id, int *next_sub, void *info, const char **reply_msg, const char **loc_ptr, u32 *loc_len, const char **abort_url)
{
    *reply_msg = NULL;
    *loc_ptr = NULL;
    *abort_url = NULL;
    int ret = sub_1b00(next_sub, (int *)info, reply_msg, abort_url);
    if (ret < 0) {
        return ret;
    }
    if (*next_sub != 0) {
        return ret;
    }
    *loc_ptr = NULL;
    char *hdr = NULL;
    u32 hdr_len = 0;
    ret = sceHttpGetAllHeader(req_id, &hdr, &hdr_len);
    if (ret >= 0) {
        ret = sceParseHttp_AD7BFDEF(hdr, hdr_len, "Location", loc_ptr, loc_len);
    }
    return ret;
}

/* 0x225c */
static int sub_225c(WisprCtx *ctx, int req_id, int *next_sub, const char **login_url, const char **loc_ptr, u32 *loc_len)
{
    *login_url = NULL;
    *loc_ptr = NULL;
    int ret = sub_1a24(login_url);
    if (ret == (int)0x80411512) {
        *next_sub = 2;
        return 0;
    }
    if (ret < 0) {
        return ret;
    }
    const char *https = "https://";
    if (sceNet_lib_B9085A96(https, *login_url, sceNet_lib_D8722983(https)) != 0) {
        return 0x80411514;
    }
    return 0;
}

/* 0x22f4 */
static int sub_22f4(WisprCtx *ctx, char *buf, int max_len)
{
    char enc_buf[0x180];
    char user_buf[0x40];
    char pass_buf[0x40];
    user_buf[0] = 0;
    pass_buf[0] = 0;

    int ret = sceUtility_28D35634(user_buf);
    if (ret < 0) {
        return ret;
    }
    ret = sceParseUri_49E950EC(enc_buf, NULL, 0x180, user_buf);
    if (ret < 0) {
        return ret;
    }
    int n1 = sceNet_lib_CF705E46(buf, "UserName=%s&", enc_buf);
    char *p = buf + n1;

    ret = sceUtility_EF3582B2(pass_buf);
    if (ret < 0) {
        return ret;
    }
    ret = sceParseUri_49E950EC(enc_buf, NULL, 0x180, pass_buf);
    if (ret < 0) {
        return ret;
    }
    int n2 = sceNet_lib_CF705E46(p, "Password=%s&", enc_buf);
    p += n2;

    ret = sceParseUri_49E950EC(enc_buf, NULL, 0x180, "http://www.playstation.com/");
    if (ret < 0) {
        return ret;
    }
    int n3 = sceNet_lib_CF705E46(p, "button=Login&FNAME=0&OriginatingServer=%s", enc_buf);
    return (p + n3) - buf;
}

/* 0x23f8 */
static int sub_23f8(WisprCtx *ctx, int tmpl_id)
{
    return sceHttpEnableRedirect(tmpl_id);
}

/* 0x2414 */
static int sub_2414(WisprCtx *ctx, const char **url)
{
    *url = "http://www.playstation.com/";
    return 0;
}

/* 0x2428 */
static int sub_2428(WisprCtx *ctx, int status, int *next_sub)
{
    return (status == 200) ? 0 : 0x80411510;
}

/* 0x243c */
static int sub_243c(WisprCtx *ctx, int status, int *next_sub)
{
    return (status == 200) ? 0 : 0x80411510;
}

/* 0x2450 */
static int sub_2450(WisprCtx *ctx, int req_id, int *next_sub, void *info, const char **reply_msg, const char **loc_ptr, u32 *loc_len, const char **abort_url)
{
    *reply_msg = NULL;
    *loc_ptr = NULL;
    *abort_url = NULL;
    return sub_1b00(next_sub, (int *)info, reply_msg, abort_url);
}
