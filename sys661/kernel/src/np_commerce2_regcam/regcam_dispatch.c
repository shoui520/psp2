#include "regcam_internal.h"

static inline int regcam_lock_ctx_req(u32 req_id, RegCamContext **ctx_out, RegCamRequest **req_out)
{
    int ret;

    *ctx_out = NULL;
    *req_out = NULL;
    ret = sceNpCommerce2_0A6AB496(&g_regcam_mgr, req_id >> 16, ctx_out, 1);
    if (ret < 0)
        return ret;
    ret = sceNpCommerce2_851E7844(&g_regcam_mgr, req_id, req_out);
    return ret;
}

static inline void regcam_refresh_keepalive(RegCamContext *ctx)
{
    u64 now_us;

    if (sceNpCommerce2_FC47B63C(&now_us) < 0)
        return;
    if ((u64)(now_us - ctx->last_activity_us) > REGCAM_KEEPALIVE_TIMEOUT_US)
        sceNpCommerce2_75DF99A9(ctx->http_tmpl_id, ctx->url_buf);
}

static inline __attribute__((always_inline)) int regcam_unlock_ctx_req(RegCamContext *ctx, RegCamRequest *req, int ret)
{
    if (req != NULL) {
        if ((req->flags & 2U) != 0)
            ret = REGCAM_ERR_ABORTED;
        sceNpCommerce2_8642C532(&g_regcam_mgr);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_regcam_mgr);
    return ret;
}

/* =========================================================================
 * Response receive & string validation (0x00f474 .. 0x01072b)
 * ========================================================================= */

int regcam_recv_response(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    u8 server_status = 0;
    int truncated = 0;
    u32 err_flag = 0;
    int hdr_len, body_len;
    u16 cmd;
    int ret;

    if (buf == NULL || buf_size == 0 || recv_size_out == NULL)
        return REGCAM_ERR_INVALID_ARG;

    ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret < 0)
        return regcam_unlock_ctx_req(ctx, req, ret);

    cmd = req->cmd_id;
    if ((cmd == REGCAM_CMD_GET_CAT || cmd == REGCAM_CMD_GET_PROD) &&
        ctx->no_size_check != 1 && buf_size < 0x40000U) {
        return regcam_unlock_ctx_req(ctx, req, REGCAM_ERR_RESP_BUF_LIMIT);
    }

    ret = sceNpCommerce2_4D43B84D(req->http_req_id, &ctx->http_status);
    if (ret < 0)
        return regcam_unlock_ctx_req(ctx, req, ret);

    hdr_len = sceNpCommerce2_C27FEAB1(ctx, req->http_req_id, cmd, &server_status, buf, buf_size);
    if (hdr_len < 0)
        return regcam_unlock_ctx_req(ctx, req, hdr_len);

    body_len = sceNpCommerce2_D4C319C6(req->http_req_id, buf, buf + hdr_len, buf_size, &truncated);
    if (body_len < 0)
        return regcam_unlock_ctx_req(ctx, req, body_len);

    if (truncated == 1) {
        if (cmd != REGCAM_CMD_GET_CAT && cmd != REGCAM_CMD_GET_DRM &&
            cmd != REGCAM_CMD_SRCH_PROD && cmd != REGCAM_CMD_GET_SRVS &&
            cmd != REGCAM_CMD_GET_SRV_DETAIL && cmd != REGCAM_CMD_GET_SUBSCR &&
            cmd != REGCAM_CMD_RECOM_PRODS) {
            return regcam_unlock_ctx_req(ctx, req, REGCAM_ERR_RESP_TOO_SMALL);
        }
    }

    ret = sceNpCommerce2_ADAA054C(buf, buf_size, (u32)body_len);
    if (ret < 0)
        return regcam_unlock_ctx_req(ctx, req, ret);

    *recv_size_out = (u32)(hdr_len + body_len);
    if (server_status == 0) {
        regcam_post_process_response(ctx, req, buf, buf_size, *recv_size_out);
        return regcam_unlock_ctx_req(ctx, req, 0);
    }

    ret = (int)(REGCAM_ERR_SERVER_BASE | (u32)server_status);
    if (cmd == REGCAM_CMD_SET_RES_ADDR && ret == REGCAM_ERR_SERVER_GENERIC)
        return regcam_unlock_ctx_req(ctx, req, REGCAM_ERR_SERVER_ADDR_INVALID);
    if ((cmd == REGCAM_CMD_REG_CC || cmd == REGCAM_CMD_VERIFY_MASTER_BILL) &&
        ret == REGCAM_ERR_SERVER_GENERIC) {
        return regcam_unlock_ctx_req(ctx, req, REGCAM_ERR_SERVER_CARD_INVALID);
    }

    regcam_tlv_parse_err_flag(buf, *recv_size_out, &err_flag);
    if (err_flag == 1)
        ret = REGCAM_ERR_MAINTENANCE;

    return regcam_unlock_ctx_req(ctx, req, ret);
}

int regcam_start_get_countries(u32 req_id)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    u32 version = 0;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0)
        ret = sceNpCommerce2_F0900551(ctx, &version);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_version_only(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                       req->cmd_id, &req->http_req_id, version);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

int regcam_start_bind_account(u32 req_id, const char *user, const char *pass)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    char console_id[80];
    u32 version = 0;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) ret = sceNpCommerce2_F0900551(ctx, &version);
    if (ret >= 0) ret = sceNpCommerce2_157FA8AA(console_id, 65);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_bind_account(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                       req->cmd_id, &req->http_req_id, version, user, pass, console_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

int regcam_start_start_reg(u32 req_id, const char *country, const char *lang, u32 yob, u32 mob, u32 dob)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    char console_id[80];
    u32 version = 0;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) ret = sceNpCommerce2_F0900551(ctx, &version);
    if (ret >= 0) ret = sceNpCommerce2_157FA8AA(console_id, 65);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_start_reg(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                    req->cmd_id, &req->http_req_id, version, country, lang, yob, mob, dob, console_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

int regcam_start_authenticate(u32 req_id, int use_explicit, const char *user, const char *pass)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    char console_id[80];
    char stored_user[80];
    char stored_pass[48];
    u32 version = 0;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) ret = sceNpCommerce2_F0900551(ctx, &version);
    if (ret >= 0 && use_explicit == 0) {
        ret = sceNpCommerce2_02CADA41(ctx, stored_user, stored_pass, 0);
        user = stored_user;
        pass = stored_pass;
    }
    if (ret >= 0) ret = sceNpCommerce2_157FA8AA(console_id, 65);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_authenticate(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                       req->cmd_id, &req->http_req_id, version, user, pass, console_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

int regcam_start_get_tran_hist(u32 req_id, int use_explicit, const char *login,
                               u32 from_y, u32 from_m, u32 from_d,
                               u32 to_y, u32 to_m, u32 to_d, s32 tz_min)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    char stored_user[80];
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0 && use_explicit == 0) {
        ret = sceNpCommerce2_02CADA41(ctx, stored_user, NULL, 0);
        login = stored_user;
    }
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_get_tran_hist(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                        req->cmd_id, &req->http_req_id, login,
                                        from_y, from_m, from_d, to_y, to_m, to_d, tz_min);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

int regcam_start_verify_dob(u32 req_id, const char *login, u32 yob, u32 mob, u32 dob)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    char console_id[80];
    u32 version = 0;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) ret = sceNpCommerce2_F0900551(ctx, &version);
    if (ret >= 0) ret = sceNpCommerce2_157FA8AA(console_id, 65);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_verify_dob(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                     req->cmd_id, &req->http_req_id, version, login, yob, mob, dob, console_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

int regcam_validate_str(u32 field_type, const char *str, u32 *flags_out)
{
    u32 len = sceNet_lib_D8722983(str);
    u32 idx;

    switch (field_type) {
    case 0x10U: { /* Online ID: 3..16 chars, starts with alpha, ASCII letters, digits, '_', '-' */
        if (len == 0) {
            *flags_out |= 1U;
            return 0;
        }
        if (len < 3 || len > 16)
            *flags_out |= 2U;
        if (!((u8)(str[0] - 'A') < 26U || (u8)(str[0] - 'a') < 26U))
            *flags_out |= 0x40U;
        for (idx = 1; idx < len; idx++) {
            char ch = str[idx];
            int is_alpha = ((u8)(ch - 'A') < 26U) || ((u8)(ch - 'a') < 26U);
            int is_digit = ((u8)(ch - '0') < 10U);
            if (!is_alpha && !is_digit && ch != '_' && ch != '-') {
                *flags_out |= 0x80U;
                break;
            }
        }
        return 0;
    }
    case 0x27U: { /* Email address */
        const char *at_ptr;
        if (len == 0) {
            *flags_out |= 1U;
            return 0;
        }
        for (idx = 0; idx < len; idx++) {
            if ((s8)str[idx] < 0 || (sceNet_lib_750F705D((u8)str[idx]) & 0x97) == 0)
                break;
        }
        if (idx != len)
            *flags_out |= 0x80U;
        if (str[0] == '@' || str[len - 1] == '@') {
            *flags_out |= 0x80U;
            return 0;
        }
        at_ptr = sceNet_lib_BCBE14CF(str, '@');
        if (at_ptr == NULL || sceNet_lib_BCBE14CF(at_ptr, '.') == NULL)
            *flags_out |= 0x80U;
        return 0;
    }
    case 0x28U: { /* Password: 8..30 printable ASCII, digit + letter, <3 consecutive, >=3 distinct */
        char distinct[16];
        u32 distinct_cnt = 0;
        u32 run_len = 0;
        int prev_ch = 0;

        if (len == 0) {
            *flags_out |= 1U;
            return 0;
        }
        for (idx = 0; idx < len; idx++) {
            if ((s8)str[idx] < 0 || (sceNet_lib_750F705D((u8)str[idx]) & 0x97) == 0)
                break;
        }
        if (idx != len)
            *flags_out |= 0x80U;
        if (len < 8 || len > 30)
            *flags_out |= 2U;

        for (idx = 0; idx < len && (u8)(str[idx] - '0') >= 10U; idx++) {}
        if (idx == len)
            *flags_out |= 4U;

        for (idx = 0; idx < len && (u8)(str[idx] - 'A') >= 26U && (u8)(str[idx] - 'a') >= 26U; idx++) {}
        if (idx == len)
            *flags_out |= 8U;

        for (idx = 0; idx < len; idx++) {
            if (sceNet_lib_C13C9307(prev_ch) == sceNet_lib_C13C9307((u8)str[idx]))
                run_len++;
            else
                run_len = 1;
            if (run_len >= 3) {
                *flags_out |= 0x10U;
                break;
            }
            prev_ch = (u8)str[idx];
        }

        for (idx = 0; idx < len; idx++) {
            u32 seen;
            for (seen = 0; seen < distinct_cnt; seen++) {
                if (distinct[seen] == str[idx])
                    break;
            }
            if (seen == distinct_cnt)
                distinct[distinct_cnt++] = str[idx];
            if (distinct_cnt >= 3)
                break;
        }
        if (idx == len)
            *flags_out |= 0x20U;
        return 0;
    }
    case 0xFF00U: { /* Printable ASCII string */
        if (len == 0) {
            *flags_out |= 1U;
            return 0;
        }
        for (idx = 0; idx < len; idx++) {
            if ((s8)str[idx] < 0 || (sceNet_lib_750F705D((u8)str[idx]) & 0x97) == 0)
                break;
        }
        if (idx != len)
            *flags_out |= 0x80U;
        return 0;
    }
    default:
        return REGCAM_ERR_INVALID_ARG;
    }
}

/* =========================================================================
 * Core manager & request lifecycle (0x01072c .. 0x014323)
 * ========================================================================= */

int regcam_init_mgr(void)
{
    sceKernelMemset(&g_regcam_mgr, 0, sizeof(g_regcam_mgr));
    return 0;
}

void regcam_term_mgr(void)
{
}

int regcam_destroy_req(u32 req_id)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret;

    ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        sceNpCommerce2_02C307C0(&g_regcam_mgr, req_id);
        ret = 0;
    }
    if (req != NULL)
        sceNpCommerce2_8642C532(&g_regcam_mgr);
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_regcam_mgr);
    return ret;
}

int regcam_abort_req(u32 req_id)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret;

    ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        sceNpCommerce2_63B96D73(req->http_req_id);
        req->flags |= 2U;
        ret = 0;
    }
    if (req != NULL)
        sceNpCommerce2_8642C532(&g_regcam_mgr);
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_regcam_mgr);
    return ret;
}

#define DEFINE_DISP_CREATE(fn_name, cmd_const) \
    int fn_name(u32 ctx_id, u32 *req_id_out) { \
        return regcam_create_req(ctx_id, (cmd_const), req_id_out); \
    }

#define DEFINE_DISP_START_EMPTY(fn_name, send_fn, clear_sess) \
    int fn_name(u32 req_id) { \
        RegCamContext *ctx = NULL; \
        RegCamRequest *req = NULL; \
        int ret = regcam_lock_ctx_req(req_id, &ctx, &req); \
        if (ret >= 0) { \
            regcam_refresh_keepalive(ctx); \
            ret = send_fn(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf, \
                          req->cmd_id, &req->http_req_id); \
            if (ret >= 0) { \
                sceNpCommerce2_FC47B63C(&ctx->last_activity_us); \
                if (clear_sess) ctx->session_len = 0; \
            } \
        } \
        return regcam_unlock_ctx_req(ctx, req, ret); \
    }

DEFINE_DISP_CREATE(regcam_disp_create_del_sess, REGCAM_CMD_DEL_SESS)
DEFINE_DISP_START_EMPTY(regcam_start_del_sess, regcam_send_del_sess, 1)
DEFINE_DISP_START_EMPTY(regcam_start_rem_pay, regcam_send_rem_pay, 0)

DEFINE_DISP_CREATE(regcam_disp_create_get_prov, REGCAM_CMD_GET_PROV)
DEFINE_DISP_START_EMPTY(regcam_start_get_prov, regcam_send_get_prov, 0)

DEFINE_DISP_CREATE(regcam_disp_create_get_master_bill, REGCAM_CMD_GET_MASTER_BILL)
int regcam_start_get_master_bill(u32 req_id, u32 pay_type)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_payment_type(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                       req->cmd_id, &req->http_req_id, pay_type);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_get_master_res_addr, REGCAM_CMD_GET_MASTER_RES_ADDR)
DEFINE_DISP_START_EMPTY(regcam_start_get_master_res_addr, regcam_send_get_master_res_addr, 0)

DEFINE_DISP_CREATE(regcam_disp_create_reg_cc, REGCAM_CMD_REG_CC)
int regcam_start_reg_cc(u32 req_id, const RegCamBillingInfo *info)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_reg_cc(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id, info);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_get_countries, REGCAM_CMD_GET_COUNTRIES)
DEFINE_DISP_CREATE(regcam_disp_create_bind_account, REGCAM_CMD_BIND_ACCOUNT)
DEFINE_DISP_CREATE(regcam_disp_create_start_reg, REGCAM_CMD_START_REG)
DEFINE_DISP_CREATE(regcam_disp_create_get_sec_q, REGCAM_CMD_GET_SEC_Q)
DEFINE_DISP_START_EMPTY(regcam_start_get_sec_q, regcam_send_get_sec_q, 0)

DEFINE_DISP_CREATE(regcam_disp_create_set_cred, REGCAM_CMD_SET_CRED)
int regcam_start_set_cred(u32 req_id, const char *login, const char *pass,
                          u32 sec_q, const char *sec_ans, u32 sec_ans_len)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_set_cred(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                   req->cmd_id, &req->http_req_id, login, pass, sec_q, sec_ans, sec_ans_len);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_set_lid, REGCAM_CMD_SET_LID)
int regcam_start_set_lid(u32 req_id, const char *login)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_set_lid(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                  req->cmd_id, &req->http_req_id, login);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_upd_pass, REGCAM_CMD_UPD_PASS)
int regcam_start_upd_pass(u32 req_id, const char *pass)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_upd_pass(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                   req->cmd_id, &req->http_req_id, pass);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_verify_oid, REGCAM_CMD_VERIFY_OID)
DEFINE_DISP_CREATE(regcam_disp_create_set_oid, REGCAM_CMD_SET_OID)
int regcam_start_oid(u32 req_id, const char *oid)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_oid(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                              req->cmd_id, &req->http_req_id, oid);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_get_name, REGCAM_CMD_GET_NAME)
DEFINE_DISP_START_EMPTY(regcam_start_get_name, regcam_send_get_name, 0)

DEFINE_DISP_CREATE(regcam_disp_create_set_name, REGCAM_CMD_SET_NAME)
int regcam_start_set_name(u32 req_id, const char *first, u32 first_len, const char *last, u32 last_len,
                          const char *first_ascii, const char *last_ascii, u32 gender)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_set_name(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                   req->cmd_id, &req->http_req_id, first, first_len, last, last_len,
                                   first_ascii, last_ascii, gender);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_set_res_addr, REGCAM_CMD_SET_RES_ADDR)
int regcam_start_set_res_addr(u32 req_id, const RegCamBillingInfo *info)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_set_res_addr(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                       req->cmd_id, &req->http_req_id, info);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_set_rrn, REGCAM_CMD_SET_RRN)
int regcam_start_set_rrn(u32 req_id, const char *rrn_front, const char *rrn_back)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_set_rrn(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                  req->cmd_id, &req->http_req_id, rrn_front, rrn_back);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_get_opt_in, REGCAM_CMD_GET_OPT_IN)
DEFINE_DISP_START_EMPTY(regcam_start_get_opt_in, regcam_send_get_opt_in, 0)

DEFINE_DISP_CREATE(regcam_disp_create_set_opt_in, REGCAM_CMD_SET_OPT_IN)
int regcam_start_set_opt_in(u32 req_id, u32 scex_flag, u32 aff_flag)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_set_opt_in(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                     req->cmd_id, &req->http_req_id, scex_flag, aff_flag);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_auth_master, REGCAM_CMD_AUTH_MASTER)
int regcam_start_auth_master(u32 req_id, const char *login, const char *pass)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_auth_master(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                      req->cmd_id, &req->http_req_id, login, pass);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_set_subacct_ctrl, REGCAM_CMD_SET_SUB_CTRL)
int regcam_start_set_subacct_ctrl(u32 req_id, const char *child_login, u32 content_rating,
                                  u32 chat_ctrl, u32 limit_flag, u32 limit_val)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_set_subacct_ctrl(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                           req->cmd_id, &req->http_req_id, child_login,
                                           content_rating, chat_ctrl, limit_flag, limit_val);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_reset_subacct_pass, REGCAM_CMD_RESET_SUB_PASS)
int regcam_start_reset_subacct_pass(u32 req_id, const char *child_login, const char *new_pass)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_reset_subacct_pass(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                             req->cmd_id, &req->http_req_id, child_login, new_pass);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_get_wal_opt, REGCAM_CMD_GET_WAL_OPT)
DEFINE_DISP_START_EMPTY(regcam_start_get_wal_opt, regcam_send_get_wal_opt, 0)

DEFINE_DISP_CREATE(regcam_disp_create_set_wal_opt, REGCAM_CMD_SET_WAL_OPT)
int regcam_start_set_wal_opt(u32 req_id, u32 auto_deposit, u32 pwd_flag)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_set_wal_opt(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                      req->cmd_id, &req->http_req_id, auto_deposit, pwd_flag);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_submit_reg, REGCAM_CMD_SUBMIT_REG)
DEFINE_DISP_START_EMPTY(regcam_start_submit_reg, regcam_send_submit_reg, 0)

DEFINE_DISP_CREATE(regcam_disp_create_authenticate, REGCAM_CMD_AUTHENTICATE)
DEFINE_DISP_CREATE(regcam_disp_create_rem_pay, REGCAM_CMD_REM_PAY)
DEFINE_DISP_CREATE(regcam_disp_create_get_tran_hist, REGCAM_CMD_GET_TRAN_HIST)
DEFINE_DISP_CREATE(regcam_disp_create_get_tran_detail, REGCAM_CMD_GET_TRAN_DETAIL)
int regcam_start_get_tran_detail(u32 req_id, u64 tx_id)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_get_tran_detail(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                          req->cmd_id, &req->http_req_id, tx_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_get_srvs, REGCAM_CMD_GET_SRVS)
int regcam_start_get_srvs(u32 req_id, u32 start_pos, u32 max_cnt)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_get_srvs(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                   req->cmd_id, &req->http_req_id, start_pos, max_cnt);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_get_srv_detail, REGCAM_CMD_GET_SRV_DETAIL)
int regcam_start_get_srv_detail(u32 req_id, const char *srv_id, u32 start_pos, u32 max_cnt)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_get_srv_detail(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                         req->cmd_id, &req->http_req_id, srv_id, start_pos, max_cnt);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_get_subscr, REGCAM_CMD_GET_SUBSCR)
int regcam_start_get_subscr(u32 req_id, const char *srv_id, u32 start_pos, u32 max_cnt)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_get_subscr(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                     req->cmd_id, &req->http_req_id, srv_id, start_pos, max_cnt);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_change_subscr, REGCAM_CMD_CHANGE_SUBSCR)
int regcam_start_change_subscr(u32 req_id, const char *sku_id)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_sku_id(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id, sku_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_cancel_subscr, REGCAM_CMD_CANCEL_SUBSCR)
int regcam_start_cancel_subscr(u32 req_id, const char *sku_id)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_sku_id(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id, sku_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_acpt_eula, REGCAM_CMD_ACPT_EULA)
DEFINE_DISP_CREATE(regcam_disp_create_verify_dob, REGCAM_CMD_VERIFY_DOB)
DEFINE_DISP_CREATE(regcam_disp_create_verify_sec_ans, REGCAM_CMD_VERIFY_SEC_ANS)
int regcam_start_verify_sec_ans(u32 req_id, u32 sec_q, const char *sec_ans, u32 sec_ans_len)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_verify_sec_ans(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                         req->cmd_id, &req->http_req_id, sec_q, sec_ans, sec_ans_len);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_send_pass_reset_mail, REGCAM_CMD_SEND_PASS_RESET_MAIL)
DEFINE_DISP_CREATE(regcam_disp_create_verify_master_bill, REGCAM_CMD_VERIFY_MASTER_BILL)
int regcam_start_verify_master_bill(u32 req_id, const RegCamBillingInfo *info, u32 save_flag)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_verify_master_bill(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                             req->cmd_id, &req->http_req_id, info, save_flag);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_reset_pass, REGCAM_CMD_RESET_PASS)
DEFINE_DISP_CREATE(regcam_disp_create_send_consent_mail, REGCAM_CMD_SEND_CONSENT_MAIL)
DEFINE_DISP_START_EMPTY(regcam_start_send_mail, regcam_send_mail, 1)

DEFINE_DISP_CREATE(regcam_disp_create_verify_bill, REGCAM_CMD_VERIFY_BILL)
int regcam_start_verify_bill(u32 req_id, const RegCamBillingInfo *info)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_verify_bill(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                      req->cmd_id, &req->http_req_id, info);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

DEFINE_DISP_CREATE(regcam_disp_create_set_account_lang, REGCAM_CMD_SET_ACCOUNT_LANG)
int regcam_start_set_account_lang(u32 req_id, const char *lang)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret = regcam_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        regcam_refresh_keepalive(ctx);
        ret = regcam_send_set_account_lang(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                           req->cmd_id, &req->http_req_id, lang);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return regcam_unlock_ctx_req(ctx, req, ret);
}

int regcam_set_svc_type(u32 ctx_id, u32 svc_type)
{
    RegCamContext *ctx = NULL;
    int ret;

    ret = sceNpCommerce2_0A6AB496(&g_regcam_mgr, ctx_id, &ctx, 1);
    if (ret >= 0)
        ctx->svc_type = svc_type;
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_regcam_mgr);
    return ret;
}

int regcam_create_req(u32 ctx_id, u16 cmd_id, u32 *req_id_out)
{
    RegCamContext *ctx = NULL;
    RegCamRequest *req = NULL;
    int ret;

    ret = sceNpCommerce2_0A6AB496(&g_regcam_mgr, ctx_id, &ctx, 1);
    if (ret >= 0) {
        req = (RegCamRequest *)sceNetIfhandle_lib_15CFE3C0(sizeof(RegCamRequest));
        if (req == NULL) {
            ret = REGCAM_ERR_OUT_OF_MEMORY;
        } else {
            sceKernelMemset(req, 0, sizeof(RegCamRequest));
            req->cmd_id = cmd_id;
            ret = sceNpCommerce2_06618C81(&g_regcam_mgr, req, ctx_id, req_id_out);
            if (ret < 0)
                sceNetIfhandle_lib_76BAD213(req);
        }
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_regcam_mgr);
    return ret;
}

int regcam_post_process_response(RegCamContext *ctx, RegCamRequest *req,
                                  const u8 *buf, u32 buf_size, u32 resp_size)
{
    u32 parsed_root[16];
    u32 acct_info[24];
    u16 cmd = req->cmd_id;
    int ret;

    if (cmd == REGCAM_CMD_AUTHENTICATE || cmd == REGCAM_CMD_CR_SESS || cmd == REGCAM_CMD_CR_GUEST_SESS) {
        if (cmd == REGCAM_CMD_AUTHENTICATE)
            ret = regcam_tlv_init_auth_root(parsed_root, buf, resp_size);
        else
            ret = regcam_tlv_init_sess_root(parsed_root, buf, resp_size);
        if (ret >= 0 && regcam_tlv_parse_account_info(parsed_root, acct_info) >= 0) {
            sceNet_lib_B5CE388A(ctx->country_code, (const char *)acct_info[16], 4);
            ctx->country_code[3] = '\0';
            sceNet_lib_B5CE388A(ctx->lang_code, (const char *)acct_info[17], 4);
            ctx->country_code[3] = '\0';
            sceNpCommerce2_F8D175FA(buf, buf_size, ctx->country_code, ctx->lang_code);
        }
    }

    cmd = req->cmd_id;
    if (cmd == REGCAM_CMD_CR_SESS || cmd == REGCAM_CMD_START_REG ||
        cmd == REGCAM_CMD_AUTHENTICATE || cmd == REGCAM_CMD_VERIFY_DOB ||
        cmd == REGCAM_CMD_CR_GUEST_SESS) {
        if (resp_size <= sizeof(ctx->session_buf)) {
            ctx->session_len = resp_size;
            sceKernelMemcpy(ctx->session_buf, buf, resp_size);
        } else {
            ctx->session_len = 0;
        }
    }
    return 0;
}
