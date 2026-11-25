#include "store_internal.h"

/* =========================================================================
 * Shared context/request lock & keep-alive helpers
 * ========================================================================= */

int store_lock_ctx_req(u32 req_id, StoreContext **ctx_out, StoreRequest **req_out)
{
    int ret;

    *ctx_out = NULL;
    *req_out = NULL;
    ret = sceNpCommerce2_0A6AB496(&g_store_mgr, req_id >> 16, ctx_out, 1);
    if (ret < 0)
        return ret;
    ret = sceNpCommerce2_851E7844(&g_store_mgr, req_id, req_out);
    return ret;
}

void store_refresh_keepalive(StoreContext *ctx)
{
    u64 now_us = 0;

    if (sceNpCommerce2_FC47B63C(&now_us) >= 0) {
        if (now_us < ctx->last_activity_us ||
            (now_us - ctx->last_activity_us) >= STORE_KEEPALIVE_TIMEOUT_US) {
            sceNpCommerce2_75DF99A9(ctx->http_tmpl_id, ctx->url_buf);
        }
    }
}

int store_unlock_ctx_req(StoreContext *ctx, StoreRequest *req, int ret)
{
    if (req) {
        if (req->flags & 2)
            ret = STORE_ERR_ABORTED;
        sceNpCommerce2_8642C532(&g_store_mgr);
    }
    if (ctx)
        sceNpCommerce2_77BE411B(&g_store_mgr);
    return ret;
}

/* =========================================================================
 * Context options, request creation, and response post-processing
 * ========================================================================= */

int store_set_ctx_option(u32 ctx_id, u32 val)
{
    StoreContext *ctx = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_store_mgr, ctx_id, &ctx, 1);
    if (ret >= 0)
        ret = sceNpCommerce2_EE7255FE(ctx, val);
    if (ctx)
        sceNpCommerce2_77BE411B(&g_store_mgr);
    return ret;
}

int store_set_svc_type(u32 ctx_id, u32 svc_type)
{
    StoreContext *ctx = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_store_mgr, ctx_id, &ctx, 1);
    if (ret >= 0)
        ctx->svc_type = svc_type;
    if (ctx)
        sceNpCommerce2_77BE411B(&g_store_mgr);
    return ret;
}

int store_get_http_status(u32 ctx_id, u32 *status_out)
{
    StoreContext *ctx = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_store_mgr, ctx_id, &ctx, 1);
    if (ret >= 0)
        *status_out = ctx->http_status;
    if (ctx)
        sceNpCommerce2_77BE411B(&g_store_mgr);
    return ret;
}

int store_create_req(u32 ctx_id, u16 cmd_id, u32 *req_id_out)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_store_mgr, ctx_id, &ctx, 1);
    if (ret >= 0) {
        req = (StoreRequest *)sceNetIfhandle_lib_15CFE3C0(sizeof(StoreRequest));
        if (!req) {
            ret = STORE_ERR_OUT_OF_MEMORY;
        } else {
            sceKernelMemset(req, 0, sizeof(StoreRequest));
            req->cmd_id = cmd_id;
            ret = sceNpCommerce2_06618C81(&g_store_mgr, req, ctx_id, req_id_out);
            if (ret < 0)
                sceNetIfhandle_lib_76BAD213(req, 0);
        }
    }
    if (ctx)
        sceNpCommerce2_77BE411B(&g_store_mgr);
    return ret;
}

int store_post_process_response(StoreContext *ctx, StoreRequest *req,
                                const u8 *buf, u32 buf_size, u32 resp_size)
{
    u32 acct_info[24];
    u32 sess_cursor[16];
    u16 cmd = req->cmd_id;

    if (cmd == STORE_CMD_AUTHENTICATE || cmd == STORE_CMD_CR_SESS ||
        cmd == STORE_CMD_CR_GUEST_SESS) {
        int parse_ret = (cmd == STORE_CMD_AUTHENTICATE)
            ? store_tlv_parse_auth_root(sess_cursor, buf, resp_size)
            : store_tlv_parse_sess_root(sess_cursor, buf, resp_size);
        if (parse_ret >= 0 && store_tlv_parse_account_info(sess_cursor, acct_info) >= 0) {
            sceNet_lib_B5CE388A(ctx->country_code, (const char *)acct_info[16], 4);
            ctx->country_code[3] = '\0';
            sceNet_lib_B5CE388A(ctx->lang_code, (const char *)acct_info[17], 4);
            ctx->lang_code[3] = '\0';
            sceNpCommerce2_F8D175FA(buf, buf_size, ctx->country_code, ctx->lang_code);
        }
        cmd = req->cmd_id;
    }

    if (cmd == STORE_CMD_AUTHENTICATE || cmd == STORE_CMD_CR_SESS ||
        cmd == STORE_CMD_START_REG || cmd == STORE_CMD_VERIFY_DOB ||
        cmd == STORE_CMD_CR_GUEST_SESS) {
        if (resp_size <= sizeof(ctx->session_buf)) {
            ctx->session_len = resp_size;
            sceKernelMemcpy(ctx->session_buf, buf, resp_size);
        } else {
            ctx->session_len = 0;
        }
    }
    return 0;
}

/* =========================================================================
 * Request start dispatchers
 * ========================================================================= */

int store_start_empty(u32 req_id)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_empty(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                               req->cmd_id, &req->http_req_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_get_cat(u32 req_id, const char *cat_id, const char * const *reward_id,
                        u32 has_reward, u32 start_pos, u32 max_cnt)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        if (ctx->no_size_check == 0 && max_cnt >= 61)
            return store_unlock_ctx_req(ctx, req, STORE_ERR_INVALID_ARG);
        store_refresh_keepalive(ctx);
        ret = store_send_get_cat(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id,
                                 cat_id, reward_id, has_reward, start_pos, max_cnt);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_guest_sess(u32 req_id, const char *country, const char *lang,
                           u32 yob, u32 mob, u32 dob)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    char console_id[64];
    u32 proto_ver = 0;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = sceNpCommerce2_F0900551(ctx, &proto_ver);
        if (ret >= 0)
            ret = sceNpCommerce2_157FA8AA(console_id, sizeof(console_id));
        if (ret >= 0)
            ret = store_send_guest_sess(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                        req->cmd_id, &req->http_req_id,
                                        proto_ver, country, lang, yob, mob, dob, console_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_get_prod(u32 req_id, const char *cat_id, const char *prod_id,
                         const char * const *reward_id, u32 has_reward)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_prod(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                  req->cmd_id, &req->http_req_id,
                                  cat_id, prod_id, reward_id, has_reward);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_add_cart(u32 req_id, const char * const *sku_ids, u32 sku_cnt,
                         const char * const *reward_ids, u32 reward_cnt)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_add_cart(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                  req->cmd_id, &req->http_req_id,
                                  sku_ids, sku_cnt, reward_ids, reward_cnt);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_sku_id(u32 req_id, const char *sku_id)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_sku_id(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                req->cmd_id, &req->http_req_id, sku_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_purchase(u32 req_id, const char *password, int quick_purchase, int auto_deposit)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_purchase(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                  req->cmd_id, &req->http_req_id,
                                  password, quick_purchase, auto_deposit);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_payment_type(u32 req_id, u32 pay_type)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_payment_type(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                      req->cmd_id, &req->http_req_id, pay_type);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_get_drm(u32 req_id, const char * const *sku_ids, u32 sku_cnt, const char *svc_id,
                        const char *reward_ids, u32 start_pos, u32 max_cnt, u32 drm_type)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_drm(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id,
                                 sku_ids, sku_cnt, svc_id, reward_ids, start_pos, max_cnt, drm_type);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_billing(u32 req_id, const StoreBillingInfo *info)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_billing(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id, info);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_postal_code(u32 req_id, const char *postal_code)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_postal_code(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                     req->cmd_id, &req->http_req_id, postal_code);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_topup_amount(u32 req_id, u32 amount)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_topup_amount(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                      req->cmd_id, &req->http_req_id, amount);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_edy_result(u32 req_id, const char *receipt_id, int is_ok)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_edy_result(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                    req->cmd_id, &req->http_req_id, receipt_id, is_ok);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_voucher(u32 req_id, const char *code1, const char *code2,
                        const char *code3, int auto_deposit)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_voucher(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id, code1, code2, code3, auto_deposit);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_event_log(u32 req_id, u32 event_type, const char *id_str, const char *url_str)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_event_log(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                   req->cmd_id, &req->http_req_id, event_type, id_str, url_str);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_search_prod(u32 req_id, u32 start_pos, u32 max_cnt,
                            const StoreSearchQuery *queries, u32 query_cnt)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_search_prod(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                     req->cmd_id, &req->http_req_id,
                                     start_pos, max_cnt, queries, query_cnt);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_prod_list(u32 req_id, const char * const *prod_ids, u32 prod_cnt)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_prod_list(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                   req->cmd_id, &req->http_req_id, prod_ids, prod_cnt);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_recom_prods(u32 req_id, const char * const *prod_ids, u32 prod_cnt,
                            u32 start_pos, u32 max_cnt, u32 recom_type)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_recom_prods(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                     req->cmd_id, &req->http_req_id,
                                     prod_ids, prod_cnt, start_pos, max_cnt, recom_type);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_get_prod_ids(u32 req_id, const char *content_id, const char *file_id)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_prod_ids(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                      req->cmd_id, &req->http_req_id, content_id, file_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}
