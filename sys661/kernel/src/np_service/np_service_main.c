#include "np_service_internal.h"

/* =========================================================================
 * Base64 encoder
 * ========================================================================= */

static const char g_base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int np_base64_encode(char *out, const u8 *in, u32 len)
{
    int written = 0;
    while ((int)len > 0) {
        u32 b0 = in[0];
        u32 b1 = (len >= 2) ? in[1] : 0;
        u32 b2 = (len >= 3) ? in[2] : 0;
        u32 triple = (b0 << 16) | (b1 << 8) | b2;

        out[0] = g_base64_chars[(triple >> 18) & 0x3F];
        out[1] = g_base64_chars[(triple >> 12) & 0x3F];
        out[2] = (len >= 2) ? g_base64_chars[(triple >> 6) & 0x3F] : '=';
        out[3] = (len >= 3) ? g_base64_chars[triple & 0x3F] : '=';

        in += 3;
        out += 4;
        written += 4;
        len -= 3;
    }
    *out = '\0';
    return written;
}

/* =========================================================================
 * VPL Memory Pool
 * ========================================================================= */

typedef struct SceKernelVplInfo {
    SceSize size;
    char name[32];
    u32 attr;
    int poolSize;
    int freeSize;
    int numWaitThreads;
} SceKernelVplInfo;

static SceUID g_vpl_id = -1;
static int g_vpl_peak_used = 0;

static int np_vpl_init(SceUID part_id, SceSize size, const char *name)
{
    SceUID id = sceKernelCreateVpl(name, part_id, 0, size, NULL);
    if (id >= 0) {
        g_vpl_id = id;
        g_vpl_peak_used = 0;
    }
    return id;
}

static int np_vpl_term(void)
{
    if (g_vpl_id > 0) {
        sceKernelDeleteVpl(g_vpl_id);
        g_vpl_id = -1;
        return -1;
    }
    return 0;
}

static int np_vpl_get_stat(int *stat)
{
    SceKernelVplInfo info;
    int ret;

    if (stat == NULL)
        return (int)0x80550101;

    info.size = sizeof(info);
    ret = sceKernelReferVplStatus(g_vpl_id, &info);
    if (ret >= 0) {
        sceKernelMemset(stat, 0, 12);
        stat[0] = info.poolSize;
        stat[1] = g_vpl_peak_used;
        stat[2] = info.freeSize;
        return 0;
    }
    return ret;
}

void *sceNpService_C5094BE6(SceSize size)
{
    void *ptr = NULL;
    SceKernelVplInfo info;

    if (sceKernelTryAllocateVpl(g_vpl_id, size, &ptr) >= 0) {
        info.size = sizeof(info);
        if (sceKernelReferVplStatus(g_vpl_id, &info) == 0) {
            int used = info.poolSize - info.freeSize;
            if (used > g_vpl_peak_used)
                g_vpl_peak_used = used;
        }
        return ptr;
    }
    return NULL;
}

int sceNpService_61AB2062(void *ptr)
{
    if (ptr != NULL)
        return sceKernelFreeVpl(g_vpl_id, ptr);
    return 0;
}

/* =========================================================================
 * sceNpManager Sign-in State Machine
 * ========================================================================= */

typedef struct NpManagerCbEntry {
    int id;
    void (*cb)(int state, int err, void *arg);
    void *arg;
    u32 gp;
} NpManagerCbEntry;

typedef struct NpManagerCtx {
    int state;                  /* 0x000 (w0) */
    SceUID mutex;               /* 0x004 (w1) */
    u8 online_id[20];           /* 0x008 (w2..6) */
    SceUID thid;                /* 0x01C (w7) */
    u8 signin_id[68];           /* 0x020 (w8..24) */
    u8 password[36];            /* 0x064 (w25..33) */
    u32 account_id_lo;          /* 0x088 (w34) */
    u32 account_id_hi;          /* 0x08C (w35) */
    void *ticket_buf;           /* 0x090 (w36) */
    int ticket_len;             /* 0x094 (w37) */
    int auth_req_id;            /* 0x098 (w38) */
    int abort_flag;             /* 0x09C (w39) */
    int busy;                   /* 0x0A0 (w40) */
    u8 country[4];              /* 0x0A4 */
    u8 pad_a8[4];               /* 0x0A8 */
    u8 profile_country[8];      /* 0x0AC (w43..44) */
    int profile_trans_id;       /* 0x0B4 (w45) */
    u8 profile[304];            /* 0x0B8 (w46..121) */
    int ptlp;                   /* 0x1E8 (w122) */
    int profile_upd;            /* 0x1EC (w123) */
    int profile_flags;          /* 0x1F0 (w124) */
    int avatar_size;            /* 0x1F4 (w125) */
    u64 profile_date;           /* 0x1F8 (w126..127) */
    u64 ticket_lifetime;        /* 0x200 (w128..129) */
    u32 parental_flags;         /* 0x208 (w130) */
    u32 lang_country;           /* 0x20C (w131) */
    u64 sub_account_info;       /* 0x210 (w132..133) */
    NpManagerCbEntry cbs[4];    /* 0x218 (w134..149) */
    u32 extra_stack;            /* 0x258 (w150) */
    int reserved_25c;           /* 0x25C (w151) */
    u32 stack_size;             /* 0x260 (w152) */
    int thread_prio;            /* 0x264 (w153) */
    int signin_mode;            /* 0x268 (w154) */
    int auth_flag;              /* 0x26C (w155) */
} NpManagerCtx; /* 624 bytes */

static int g_np_service_init = 0;
static int g_np_manager_init = 0;
static int g_signin_update_status = 0;
static int *g_signin_update_param = NULL;
static int g_alt_signin_status = 0;
static int *g_alt_signin_param = NULL;
static char g_np_env[16];
static int g_mgr_internal_cb_id = -1;
static NpManagerCtx g_mgr;
static int g_auth_cb_result = 0;
static SceUID g_auth_cb_sema = 0;

static const u32 g_auth_ver = 0x00010002u;

static int np_manager_thread(SceSize args, void *argp);
static int np_manager_add_cb(NpManagerCtx *ctx, void *cb, void *arg, int *out_id);
static int np_manager_del_cb(NpManagerCtx *ctx, u32 id);
static int np_manager_start_signin(const void *user, const void *pass, const void *acc_id,
                                   void *cb, void *arg, int mode);
static int np_manager_abort_signin(void);

const char *np_manager_get_env(void)
{
    return g_np_env;
}

static int np_manager_clear_internal_cb(void)
{
    np_manager_del_cb(&g_mgr, (u32)g_mgr_internal_cb_id);
    g_mgr_internal_cb_id = -1;
    return -1;
}

static int np_manager_get_creds(NpManagerCtx *ctx, void *user, void *pass, u32 *acc_id)
{
    int ret = sceKernelLockMutex(ctx->mutex, 1, NULL);
    if (ret >= 0) {
        if (user != NULL)
            sceKernelMemcpy(user, ctx->signin_id, 68);
        if (pass != NULL)
            sceKernelMemcpy(pass, ctx->password, 36);
        if (acc_id != NULL) {
            acc_id[0] = ctx->account_id_lo;
            acc_id[1] = ctx->account_id_hi;
        }
        sceKernelUnlockMutex(ctx->mutex, 1);
        return 0;
    }
    return ret;
}

static int np_manager_init(const char *env, u32 stack_size, int prio)
{
    SceUID mtx;
    SceUID thid;

    if (stack_size < 0x3800u)
        return (int)0x80550508;
    if (g_np_manager_init > 0)
        return (int)0x80550501;

    sceKernelMemset(&g_mgr, 0, sizeof(g_mgr));
    sceKernelMemset(g_np_env, 0, 9);
    sceNet_lib_B5CE388A(g_np_env, env, 9);
    g_np_env[8] = '\0';

    mtx = sceKernelCreateMutex("SceNpManager", 0x200, 0, NULL);
    if (mtx < 0)
        return mtx;
    g_mgr.mutex = mtx;

    thid = sceKernelCreateThread("SceNpManager", np_manager_thread, prio, (int)stack_size, 0, NULL);
    if (thid < 0) {
        if (g_mgr.mutex > 0) {
            sceKernelDeleteMutex(g_mgr.mutex);
            g_mgr.mutex = 0;
        }
        return thid;
    }

    g_np_manager_init = 1;
    g_mgr.state = 0;
    g_mgr.extra_stack = stack_size - 0x3800u;
    g_mgr.thread_prio = prio;
    g_mgr.stack_size = stack_size;
    g_mgr.auth_flag = 1;
    g_mgr.thid = thid;
    return thid;
}

static int np_manager_term(void)
{
    int ret = 0;

    if (g_np_manager_init <= 0)
        return (int)0x80550502;

    sceNpAuthAbortRequest(g_mgr.auth_req_id);
    sceNpService_43E635D2(g_mgr.profile_trans_id);
    g_mgr.abort_flag = 1;
    if (g_mgr.mutex > 0) {
        sceKernelDeleteMutex(g_mgr.mutex);
        g_mgr.mutex = 0;
    }
    sceNpAuthDestroyRequest(g_mgr.auth_req_id);
    if (g_mgr.online_id[0] != 0) {
        ret = sceNpService_5B74DE06(g_mgr.online_id);
        sceKernelMemset(g_mgr.online_id, 0, 16);
    }
    sceKernelWaitThreadEnd(g_mgr.thid, NULL);
    sceKernelDeleteThread(g_mgr.thid);
    g_mgr.busy = 0;
    g_np_manager_init = 0;
    g_mgr.thid = 0;
    return ret;
}

int sceNpServiceInit(SceSize pool_size, SceSize stack_size, int prio)
{
    SceUID part_id = 0;
    char env[16];
    int ret;

    if (_sceKernelCheckThreadStack() < 4048)
        return (int)0x80550004;
    if (g_np_service_init == 1)
        return (int)0x80550501;
    if (pool_size == 0)
        return (int)0x80550503;

    ret = sceKernelQueryMemoryInfo(&g_np_service_init, &part_id, NULL);
    if (ret >= 0) {
        ret = sceNpCore_52440ABF(env);
        if (ret >= 0) {
            ret = np_vpl_init(part_id, pool_size, "SceNpService");
            if (ret >= 0) {
                ret = np_httpxml_init();
                if (ret >= 0) {
                    ret = np_manager_init(env, stack_size, prio);
                    if (ret >= 0) {
                        ret = np_roster_init();
                        if (ret >= 0) {
                            g_np_service_init = 1;
                            return 0;
                        }
                    }
                }
            }
        }
    }

    np_roster_term();
    np_manager_term();
    np_httpxml_term();
    np_vpl_term();
    return ret;
}

int sceNpServiceTerm(void)
{
    int ret = _sceKernelCheckThreadStack();
    if (ret >= 4080 && g_np_service_init != 0) {
        np_roster_term();
        np_manager_term();
        np_httpxml_term();
        ret = np_vpl_term();
        g_np_service_init = 0;
    }
    return ret;
}

int sceNpServiceGetMemoryStat(int *stat)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return (int)0x80550004;
    if (g_np_service_init == 0)
        return (int)0x80550502;
    return np_vpl_get_stat(stat);
}

int sceNpService_63644C02(void)
{
    int ret;

    if (g_np_manager_init <= 0)
        return (int)0x80550502;
    if (_sceKernelCheckThreadStack() < 4064)
        return (int)0x80550004;

    ret = sceKernelLockMutex(g_mgr.mutex, 1, NULL);
    if (ret >= 0) {
        g_mgr.abort_flag = 1;
        sceNpAuthAbortRequest(g_mgr.auth_req_id);
        sceNpService_43E635D2(g_mgr.profile_trans_id);
        np_manager_del_cb(&g_mgr, (u32)g_mgr_internal_cb_id);
        g_mgr_internal_cb_id = -1;
        g_mgr.ticket_len = 0;
        sceNp_02CA8CAA(NULL, 0, NULL);
        sceKernelMemset(g_mgr.profile, 0, 304);
        sceKernelMemset(&g_mgr.lang_country, 0, 4);
        sceKernelMemset(&g_mgr.parental_flags, 0, 4);
        sceKernelMemset(g_mgr.profile_country, 0, 8);
        g_mgr.sub_account_info = 0;
        sceNp_C48F2847(g_mgr.profile, &g_mgr.lang_country, &g_mgr.parental_flags, g_mgr.profile_country);
        sceKernelUnlockMutex(g_mgr.mutex, 1);
        sceKernelWaitThreadEnd(g_mgr.thid, NULL);
        if (g_mgr.online_id[0] != 0) {
            sceNpService_5B74DE06(g_mgr.online_id);
            sceKernelMemset(g_mgr.online_id, 0, 16);
        }
        g_mgr.auth_flag = 1;
        g_mgr.busy = 0;
        return 0;
    }
    return ret;
}

int sceNpService_B2EA4530(u64 *out_usec)
{
    int ret;
    void *ticket = NULL;
    int tlen;
    u64 param_buf[32];

    if (g_np_manager_init <= 0)
        return (int)0x80550502;
    if (out_usec == NULL)
        return (int)0x80550503;

    ret = sceKernelLockMutex(g_mgr.mutex, 1, NULL);
    if (ret >= 0) {
        int res = (int)0x80550506;
        if (g_mgr.state == 4 || g_mgr.state == 2) {
            tlen = sceNp_9B87B19B(&ticket);
            res = tlen;
            if (tlen >= 0) {
                res = sceNpAuthGetTicketParam(ticket, tlen, 3, param_buf);
                if (res >= 0) {
                    u64 expire = param_buf[0];
                    res = sceNpAuthGetTicketParam(ticket, tlen, 2, param_buf);
                    if (res >= 0)
                        *out_usec = 1000ULL * (expire - param_buf[0]);
                }
            }
        }
        sceKernelUnlockMutex(g_mgr.mutex, 1);
        return res;
    }
    return ret;
}

static void np_signin_update_cb(int state, int err)
{
    if (err < 0) {
        g_signin_update_param[1] = err;
        g_signin_update_param[2] = 0;
        g_signin_update_param[3] = 0;
        g_signin_update_status = 2;
    } else if (state == 2) {
        g_signin_update_param[1] = sceNpService_B2EA4530((u64 *)(g_signin_update_param + 2));
        g_signin_update_status = 2;
    }
}

static void np_alt_signin_cb(int state, int err)
{
    if (err < 0) {
        g_alt_signin_param[1] = err;
        g_alt_signin_status = 2;
    } else if (state == 2) {
        g_alt_signin_param[1] = 0;
        g_alt_signin_status = 2;
    }
}

int sceNpManagerSigninUpdateInitStart(int *param)
{
    u8 user[68];
    u8 pass[36];
    int ret;

    if (g_signin_update_status != 0)
        return (int)0x80550506;
    if (_sceKernelCheckThreadStack() < 3952)
        return (int)0x80550004;
    if (param == NULL || param[0] != 16)
        return (int)0x80550503;

    sceKernelMemset(user, 0, sizeof(user));
    sceKernelMemset(pass, 0, sizeof(pass));
    ret = sceNp_B819A0C8(user, pass, NULL);
    if (ret >= 0) {
        g_signin_update_status = 1;
        g_signin_update_param = param;
        ret = np_manager_start_signin(user, pass, NULL, np_signin_update_cb, NULL, 1);
        if (ret < 0) {
            g_signin_update_param = NULL;
            g_signin_update_status = 0;
        }
    }
    return ret;
}

int sceNpService_41E99DD1(const void *user, const void *pass, const void *acc_id, void *cb, void *arg)
{
    return np_manager_start_signin(user, pass, acc_id, cb, arg, 0);
}

int sceNpManagerSigninUpdateAbort(void)
{
    if (_sceKernelCheckThreadStack() < 4064)
        return (int)0x80550004;
    return np_manager_abort_signin();
}

int sceNpService_EBC076F3(void *cb, void *arg)
{
    int ret;
    int id = 0;

    if (g_np_manager_init <= 0)
        return (int)0x80550502;
    ret = sceKernelLockMutex(g_mgr.mutex, 1, NULL);
    if (ret >= 0) {
        ret = np_manager_add_cb(&g_mgr, cb, arg, &id);
        if (ret >= 0)
            ret = id;
        sceKernelUnlockMutex(g_mgr.mutex, 1);
    }
    return ret;
}

int sceNpService_7FA900FA(u32 id)
{
    int ret;

    if (g_np_manager_init <= 0)
        return (int)0x80550502;
    ret = sceKernelLockMutex(g_mgr.mutex, 1, NULL);
    if (ret >= 0) {
        ret = np_manager_del_cb(&g_mgr, id);
        sceKernelUnlockMutex(g_mgr.mutex, 1);
    }
    return ret;
}

int sceNpService_D6E95870(int *state_out)
{
    if (g_np_manager_init <= 0)
        return (int)0x80550502;
    if (state_out == NULL)
        return (int)0x80550503;
    *state_out = g_mgr.state;
    return 0;
}

int sceNpManagerSigninUpdateGetStatus(void)
{
    return g_signin_update_status;
}

int sceNpManagerSigninUpdateShutdownStart(void)
{
    if (g_signin_update_status == 2) {
        g_signin_update_status = 0;
        g_signin_update_param = NULL;
        return 0;
    }
    return (int)0x80550506;
}

int sceNpService_53F01574(void)
{
    if (g_np_manager_init <= 0)
        return (int)0x80550502;
    g_mgr.auth_flag = 0;
    return 0;
}

int sceNpService_CAF9E5FD(int *param)
{
    char user[68];
    char pass[36];
    int ret;

    if (_sceKernelCheckThreadStack() < 3952)
        return (int)0x80550004;
    if (param == NULL || param[0] != 8)
        return (int)0x80550503;

    sceNet_lib_B5CE388A(user, "VrEa2uUV3s2hTgLj", 64);
    sceNet_lib_B5CE388A(pass, "BDFvcv2vQ2mGvxsJ", 32);
    g_alt_signin_param = param;
    g_alt_signin_status = 1;
    ret = np_manager_start_signin(user, pass, NULL, np_alt_signin_cb, NULL, 2);
    if (ret < 0) {
        g_alt_signin_param = NULL;
        g_alt_signin_status = 0;
    }
    return ret;
}

int sceNpService_B9D7D1A3(void)
{
    return g_alt_signin_status;
}

int sceNpService_A88B932E(void)
{
    if (g_alt_signin_status == 2) {
        g_alt_signin_status = 0;
        g_alt_signin_param = NULL;
        return 0;
    }
    return (int)0x80550506;
}

int sceNpService_D05205D6(void)
{
    if (_sceKernelCheckThreadStack() < 4064)
        return (int)0x80550004;
    return np_manager_abort_signin();
}

static int np_manager_start_signin(const void *user, const void *pass, const void *acc_id,
                                   void *cb, void *arg, int mode)
{
    NpManagerCtx *ctx_ptr = &g_mgr;
    int ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return (int)0x80550004;
    if (g_np_manager_init <= 0)
        return (int)0x80550502;
    if (g_mgr.busy != 0)
        return (int)0x80550506;
    if (user == NULL || pass == NULL || *(const u8 *)user == 0 || *(const u8 *)pass == 0)
        return (int)0x80550509;

    ret = sceKernelLockMutex(g_mgr.mutex, 1, NULL);
    if (ret < 0)
        return ret;

    g_mgr_internal_cb_id = -1;
    sceKernelMemcpy(g_mgr.signin_id, user, 68);
    sceKernelMemcpy(g_mgr.password, pass, 36);
    if (acc_id != NULL)
        sceKernelMemcpy(&g_mgr.account_id_lo, acc_id, 8);
    else
        sceKernelMemset(&g_mgr.account_id_lo, 0, 8);

    ret = np_manager_add_cb(&g_mgr, cb, arg, &g_mgr_internal_cb_id);
    if (ret >= 0) {
        g_mgr.busy = 1;
        g_mgr.signin_mode = mode;
        g_mgr.abort_flag = 0;
        ret = sceKernelStartThread(g_mgr.thid, 4, &ctx_ptr);
        if (ret >= 0) {
            sceKernelUnlockMutex(g_mgr.mutex, 1);
            return ret;
        }
        sceKernelUnlockMutex(g_mgr.mutex, 1);
    }

    np_manager_del_cb(&g_mgr, (u32)g_mgr_internal_cb_id);
    if (g_mgr.thid > 0) {
        g_mgr.abort_flag = 1;
        sceNpAuthAbortRequest(g_mgr.auth_req_id);
        sceNpService_43E635D2(g_mgr.profile_trans_id);
    }
    g_mgr.abort_flag = 0;
    g_mgr.busy = 0;
    sceKernelUnlockMutex(g_mgr.mutex, 1);
    return ret;
}

static int np_manager_abort_signin(void)
{
    int ret;

    if (g_np_manager_init <= 0)
        return (int)0x80550502;

    g_mgr.auth_flag = 1;
    ret = sceKernelLockMutex(g_mgr.mutex, 1, NULL);
    if (ret >= 0) {
        g_mgr.abort_flag = 1;
        sceNpAuthAbortRequest(g_mgr.auth_req_id);
        sceNpService_43E635D2(g_mgr.profile_trans_id);
        sceKernelUnlockMutex(g_mgr.mutex, 1);
        sceKernelWaitThreadEnd(g_mgr.thid, NULL);
        np_manager_del_cb(&g_mgr, (u32)g_mgr_internal_cb_id);
        g_mgr_internal_cb_id = -1;
        if (g_mgr.online_id[0] != 0) {
            sceNpService_5B74DE06(g_mgr.online_id);
            sceKernelMemset(g_mgr.online_id, 0, 16);
        }
        g_mgr.busy = 0;
        return 0;
    }
    return ret;
}

static int np_auth_ticket_cb(int req_id, int result, void *arg)
{
    NpManagerCtx *ctx = (NpManagerCtx *)arg;
    int ret = result;

    sceKernelLockMutex(ctx->mutex, 1, NULL);
    if (result > 0) {
        if (ctx->ticket_buf != NULL) {
            sceNpService_61AB2062(ctx->ticket_buf);
            ctx->ticket_buf = NULL;
        }
        ctx->ticket_len = result;
        ctx->ticket_buf = sceNpService_C5094BE6((SceSize)result);
        ret = (int)0x80550504;
        if (ctx->ticket_buf != NULL)
            ret = sceNpAuthGetTicket(req_id, ctx->ticket_buf, result);
    }
    g_auth_cb_result = ret;
    sceKernelUnlockMutex(ctx->mutex, 1);
    sceKernelSignalSema(g_auth_cb_sema, 1);
    return 0;
}

static int np_manager_do_auth(NpManagerCtx *ctx)
{
    u8 user[68];
    u8 pass[36];
    u64 expected_acc_id = 0;
    u64 param_buf[32];
    u32 ver = g_auth_ver;
    SceUID sema;
    int ret;

    g_auth_cb_sema = 0;
    ctx->auth_req_id = 0;

    sema = sceKernelCreateSema("SceNpManager", 0, 0, 1, NULL);
    ret = sema;
    if (sema < 0)
        goto cleanup;
    g_auth_cb_sema = sema;

    ret = np_manager_get_creds(ctx, user, pass, (u32 *)&expected_acc_id);
    if (ret < 0)
        goto cleanup;

    ret = sceKernelLockMutex(ctx->mutex, 1, NULL);
    if (ret < 0)
        goto cleanup;

    if (ctx->abort_flag > 0) {
        ret = (int)0x80550507;
        sceKernelUnlockMutex(ctx->mutex, 1);
        goto cleanup;
    }

    {
        const char *service_id = (ctx->signin_mode == 2) ? "SCEI-0" : "IV0001-NPXS01001_00";
        ret = sceNpAuth_CE85B3B8(&ctx->auth_req_id, &ver, service_id, ctx->auth_flag);
    }
    if (ret < 0) {
        sceKernelUnlockMutex(ctx->mutex, 1);
        goto cleanup;
    }

    ret = sceNpAuth_B714FBDD(ctx->auth_req_id, user, pass);
    if (ret < 0 && ret != (int)0x80550382) {
        sceKernelUnlockMutex(ctx->mutex, 1);
        goto cleanup;
    }

    g_auth_cb_result = 0;
    if (ctx->abort_flag > 0) {
        ret = (int)0x80550507;
        sceKernelUnlockMutex(ctx->mutex, 1);
        goto cleanup;
    }

    ret = sceNpAuth_DAD65284(ctx->auth_req_id, 0, np_auth_ticket_cb, ctx);
    if (ret < 0) {
        sceKernelUnlockMutex(ctx->mutex, 1);
        goto cleanup;
    }

    sceKernelUnlockMutex(ctx->mutex, 1);
    sceKernelWaitSema(g_auth_cb_sema, 1, NULL);

    while (sceNpAuth_61BB18B3(ctx->auth_req_id) < 4)
        sceKernelDelayThread(10000);

    ret = g_auth_cb_result;
    if (ret < 0)
        goto cleanup;

    sceKernelLockMutex(ctx->mutex, 1, NULL);

    ret = sceNpAuthGetTicketParam(ctx->ticket_buf, ctx->ticket_len, 4, param_buf);
    if (ret < 0)
        goto unlock_cleanup;
    if (expected_acc_id != 0 && expected_acc_id != param_buf[0]) {
        ret = (int)0x80550484;
        goto unlock_cleanup;
    }
    ctx->account_id_lo = (u32)param_buf[0];
    ctx->account_id_hi = (u32)(param_buf[0] >> 32);

    ret = sceNpAuthGetTicketParam(ctx->ticket_buf, ctx->ticket_len, 5, param_buf);
    if (ret < 0)
        goto unlock_cleanup;
    sceKernelMemcpy(ctx->online_id, param_buf, 20);

    ret = sceNpAuthGetTicketParam(ctx->ticket_buf, ctx->ticket_len, 6, param_buf);
    if (ret < 0)
        goto unlock_cleanup;
    sceKernelMemcpy(ctx->country, param_buf, 2);
    ctx->country[2] = '\0';
    sceKernelMemset(&ctx->lang_country, 0, 4);
    sceKernelMemcpy(&ctx->lang_country, param_buf, 2);
    {
        u8 lang_idx = (u8)(param_buf[0] >> 24);
        ((u8 *)&ctx->lang_country)[3] = (lang_idx < 12) ? lang_idx : 1;
    }

    ret = sceNpAuthGetTicketParam(ctx->ticket_buf, ctx->ticket_len, 9, param_buf);
    if (ret < 0)
        goto unlock_cleanup;
    sceKernelMemset(&ctx->parental_flags, 0, 4);
    {
        u32 p = (u32)param_buf[0];
        s8 age = (s8)(p & 0xFF);
        ctx->parental_flags = (ctx->parental_flags & 0xFF3FFF01u)
            | (2u * ((p >> 24) & 0x7Fu))
            | (((p & 0x100u) != 0) ? (1u << 23) : 0u)
            | (((p & 0x200u) != 0) ? (1u << 22) : 0u);
        ((s8 *)&ctx->parental_flags)[3] = age;
        if (age < 0) {
            ret = sceNpAuthGetTicketParam(ctx->ticket_buf, ctx->ticket_len, 10, param_buf);
            if (ret < 0)
                goto unlock_cleanup;
            ctx->sub_account_info = param_buf[0];
        } else {
            ctx->sub_account_info = 0;
        }
    }

    ret = sceNpAuthGetTicketParam(ctx->ticket_buf, ctx->ticket_len, 3, param_buf);
    if (ret < 0)
        goto unlock_cleanup;
    ctx->ticket_lifetime = param_buf[0];

    ret = sceNpAuthGetTicketParam(ctx->ticket_buf, ctx->ticket_len, 2, param_buf);
    if (ret < 0)
        goto unlock_cleanup;
    ctx->ticket_lifetime -= param_buf[0];

    ret = sceNpAuthGetTicketParam(ctx->ticket_buf, ctx->ticket_len, 2, param_buf);
    if (ret >= 0) {
        u64 rtc_tick = 1000ULL * param_buf[0] + 62135596800000000ULL;
        ret = sceRtc_508BA64B(&rtc_tick);
    }

unlock_cleanup:
    sceKernelUnlockMutex(ctx->mutex, 1);

cleanup:
    if (g_auth_cb_sema > 0) {
        sceKernelDeleteSema(g_auth_cb_sema);
        g_auth_cb_sema = 0;
    }
    if (ctx->auth_req_id > 0) {
        sceNpAuthDestroyRequest(ctx->auth_req_id);
        ctx->auth_req_id = 0;
    }
    ctx->auth_flag = 1;
    return ret;
}

static int np_manager_fetch_profile(NpManagerCtx *ctx)
{
    int ret;
    int tid;

    ret = sceNpService_D97B509F(ctx->online_id);
    if (ret < 0)
        goto fail;

    ret = np_user_set_env_country(ctx->online_id, np_manager_get_env(), (const char *)ctx->country);
    if (ret < 0)
        goto fail;

    tid = sceNpService_7EF4312E(ctx->online_id, 1, NULL);
    ret = tid;
    if (tid < 0)
        goto fail;

    ctx->profile_trans_id = tid;
    if (ctx->abort_flag > 0) {
        ret = (int)0x80550507;
        goto fail;
    }

    ctx->avatar_size = 2;
    ret = np_get_self_profile(tid, ctx->profile, &ctx->ptlp, &ctx->profile_upd,
                              &ctx->profile_flags, &ctx->profile_date,
                              (const u8 *)ctx->ticket_buf, (u32)ctx->ticket_len, 2);
    if (ret < 0)
        goto fail;

    sceNpService_DD8B5E53(ctx->profile_trans_id);
    ctx->profile_trans_id = 0;
    sceKernelMemcpy(ctx->profile_country, ctx->profile + 20, 2);
    ctx->profile_country[2] = '\0';
    sceNet_lib_B5CE388A((char *)(ctx->profile + 24), "psp", 3);
    return ret;

fail:
    sceNpService_DD8B5E53(ctx->profile_trans_id);
    ctx->profile_trans_id = 0;
    sceNpService_5B74DE06(ctx->online_id);
    return ret;
}

static int np_manager_state_0(NpManagerCtx *ctx, int prev, int *next, int *done)
{
    (void)ctx; (void)prev; (void)done;
    *next = 1;
    return 0;
}

static int np_manager_state_1(NpManagerCtx *ctx, int prev, int *next, int *done)
{
    int ret;
    u64 tick;

    if (prev != 0) {
        *next = 0;
        *done = 4;
        return (int)0x80550481;
    }

    ret = np_manager_do_auth(ctx);
    if (ret == (int)0x80550307) {
        sceKernelLockMutex(ctx->mutex, 1, NULL);
        *next = 0;
        sceKernelUnlockMutex(ctx->mutex, 1);
        *done = 4;
        return ret;
    }
    if (ret < 0) {
        *next = 0;
        *done = 4;
        return ret;
    }

    if (ctx->signin_mode == 1) {
        ret = sceRtcGetCurrentTick(&tick);
        if (ret < 0 || (ret = sceNp_02CA8CAA(ctx->ticket_buf, ctx->ticket_len, &tick)) < 0) {
            *next = 0;
            *done = 4;
            return ret;
        }
        *done = 4;
    } else if (ctx->signin_mode == 2) {
        *done = 4;
    }

    *next = 2;
    return ret;
}

static int np_manager_state_2(NpManagerCtx *ctx, int prev, int *next, int *done)
{
    int ret;

    if (prev != 1) {
        *next = 0;
        *done = 4;
        return (int)0x80550481;
    }

    if (sceNp_E24DA399() != 0) {
        *next = 3;
        return 0;
    }

    ret = np_manager_fetch_profile(ctx);
    if (ret == (int)0x80550707) {
        sceKernelLockMutex(ctx->mutex, 1, NULL);
        *next = 0;
        sceKernelUnlockMutex(ctx->mutex, 1);
        *done = 4;
        return ret;
    }
    if (ret >= 0) {
        *next = 3;
        return ret;
    }
    *next = 0;
    *done = 4;
    return ret;
}

static int np_manager_state_3(NpManagerCtx *ctx, int prev, int *next, int *done)
{
    int ret;
    u64 tick;

    (void)prev;

    if (ctx->abort_flag > 0) {
        ret = (int)0x80550507;
        goto fail;
    }
    ret = sceRtcGetCurrentTick(&tick);
    if (ret < 0) goto fail;
    ret = sceNp_02CA8CAA(ctx->ticket_buf, ctx->ticket_len, &tick);
    if (ret < 0) goto fail;
    ret = sceNp_C48F2847(ctx->profile, &ctx->lang_country, &ctx->parental_flags, ctx->profile_country);
    if (ret < 0) goto fail;
    ret = sceNp_66B86876(ctx->signin_id, ctx->password, ctx->account_id_lo, ctx->account_id_hi);
    if (ret < 0) goto fail;
    ret = sceNp_CF83CC3B(ctx->profile_flags & 1);
    if (ret < 0) goto fail;

    *next = 4;
    return ret;

fail:
    sceNp_CF83CC3B(0);
    *done = 4;
    *next = 0;
    return ret;
}

static int np_manager_state_4(NpManagerCtx *ctx, int prev, int *next, int *done)
{
    (void)ctx;
    *done = 4;
    if (prev != 3) {
        *next = 0;
        return (int)0x80550481;
    }
    return 0;
}

static int (*const g_mgr_state_table[5])(NpManagerCtx *, int, int *, int *) = {
    np_manager_state_0,
    np_manager_state_1,
    np_manager_state_2,
    np_manager_state_3,
    np_manager_state_4,
};

static int np_manager_notify_cbs(NpManagerCtx *ctx, int state, int err)
{
    int ret = sceKernelLockMutex(ctx->mutex, 1, NULL);
    if (ret >= 0) {
        int i;
        for (i = 0; i < 4; i++) {
            if (ctx->cbs[i].cb != NULL) {
                _sceKernelCheckThreadStack();
                ctx->cbs[i].cb(state, err, ctx->cbs[i].arg);
            }
        }
        sceKernelUnlockMutex(ctx->mutex, 1);
    }
    return ret;
}

static int np_manager_add_cb(NpManagerCtx *ctx, void *cb, void *arg, int *out_id)
{
    int ret = sceKernelLockMutex(ctx->mutex, 1, NULL);
    if (ret >= 0) {
        int i;
        for (i = 0; i < 4; i++) {
            if (ctx->cbs[i].cb == NULL)
                break;
        }
        if (i == 4) {
            sceKernelUnlockMutex(ctx->mutex, 1);
            return (int)0x80550482;
        }
        ctx->cbs[i].id = i;
        ctx->cbs[i].cb = (void (*)(int, int, void *))cb;
        ctx->cbs[i].arg = arg;
        if (sceKernelGetModuleGPByAddress(cb, &ctx->cbs[i].gp) < 0)
            ctx->cbs[i].gp = 0;
        *out_id = i;
        sceKernelUnlockMutex(ctx->mutex, 1);
    }
    return ret;
}

static int np_manager_del_cb(NpManagerCtx *ctx, u32 id)
{
    int ret = 0;
    if (id < 5u) {
        ret = sceKernelLockMutex(ctx->mutex, 1, NULL);
        if (ret >= 0) {
            if (ctx->cbs[id].cb != NULL) {
                ctx->cbs[id].cb = NULL;
                sceKernelUnlockMutex(ctx->mutex, 1);
                return ret;
            }
            sceKernelUnlockMutex(ctx->mutex, 1);
            return (int)0x80550483;
        }
    }
    return ret;
}

static int np_manager_thread(SceSize args, void *argp)
{
    NpManagerCtx *ctx = *(NpManagerCtx **)argp;
    int prev = 0;
    int next = 0;
    int done = 0;

    (void)args;

    ctx->state = 0;
    while (ctx->state < 5) {
        int err;
        done = 0;
        err = g_mgr_state_table[ctx->state](ctx, prev, &next, &done);
        sceKernelLockMutex(ctx->mutex, 1, NULL);
        prev = ctx->state;
        ctx->state = next;
        sceKernelUnlockMutex(ctx->mutex, 1);
        if (next != 4 || done != 4 || err != 0)
            np_manager_notify_cbs(ctx, next, err);
        if (done == 4)
            break;
    }

    sceKernelLockMutex(ctx->mutex, 1, NULL);
    np_manager_clear_internal_cb();
    ctx->abort_flag = 0;
    ctx->busy = 0;
    if (ctx->ticket_buf != NULL) {
        sceNpService_61AB2062(ctx->ticket_buf);
        ctx->ticket_buf = NULL;
    }
    sceKernelUnlockMutex(ctx->mutex, 1);
    sceNetFreeThreadinfo(ctx->thid);
    sceKernelExitThread(0);
    return 0;
}
