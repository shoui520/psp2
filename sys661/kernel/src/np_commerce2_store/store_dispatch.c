#include "store_internal.h"

u32 g_store_mgr;

/* Inline helpers matching Sony's context/request locking and keepalive check */
static inline __attribute__((always_inline)) int
store_lock_ctx_req(u32 req_id, StoreContext **ctx_out, StoreRequest **req_out)
{
    int ret;

    *ctx_out = NULL;
    *req_out = NULL;
    ret = sceNpCommerce2_0A6AB496(&g_store_mgr, req_id >> 16, ctx_out, 1);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_851E7844(&g_store_mgr, req_id, req_out);
}

static inline __attribute__((always_inline)) void
store_refresh_keepalive(StoreContext *ctx)
{
    u64 now_us;
    if (sceNpCommerce2_FC47B63C(&now_us) >= 0) {
        if ((now_us - ctx->last_activity_us) > STORE_KEEPALIVE_LIMIT_US)
            sceNpCommerce2_75DF99A9(ctx->http_tmpl_id, ctx->url_buf);
    }
}

static inline __attribute__((always_inline)) int
store_unlock_ctx_req(StoreContext *ctx, StoreRequest *req, int ret)
{
    if (req) {
        if (req->req_flags & 2U)
            ret = STORE_ERR_ABORTED;
        sceNpCommerce2_8642C532(&g_store_mgr);
    }
    if (ctx)
        sceNpCommerce2_77BE411B(&g_store_mgr);
    return ret;
}

/* =========================================================================
 * Response receive & dispatch (0x015264)
 * ========================================================================= */

int store_recv_response(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    u8 status_byte[4];
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    int trunc_flag;
    u32 err_flag;
    int ret;

    if (!resp || buf_size == 0U || !recv_size_out)
        return STORE_ERR_INVALID_ARG;

    ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret >= 0) {
        int allow = 1;
        if ((u32)(req->cmd_id - 3U) < 2U && ctx->no_size_check != 1U && buf_size <= 0x3FFFFU) {
            ret = STORE_ERR_RESP_BUF_LIMIT;
            allow = 0;
        }
        if (allow)
            ret = sceNpCommerce2_4D43B84D(req->http_req_id, &ctx->http_status);
        if (ret >= 0) {
            int hdr_len = sceNpCommerce2_C27FEAB1(ctx, req->http_req_id, req->cmd_id,
                                                  status_byte, resp, buf_size);
            ret = hdr_len;
            if (hdr_len >= 0) {
                int body_len = sceNpCommerce2_D4C319C6(req->http_req_id, resp, resp + hdr_len,
                                                       buf_size, &trunc_flag);
                ret = body_len;
                if (body_len >= 0) {
                    u32 total_len = (u32)(hdr_len + body_len);
                    int trunc_ok = 1;
                    if (trunc_flag == 1) {
                        u32 cid = req->cmd_id;
                        if (cid != STORE_CMD_GET_CAT && cid != STORE_CMD_GET_DRM &&
                            cid != STORE_CMD_SRCH_PROD && cid != 53U && cid != 54U &&
                            cid != 55U && cid != STORE_CMD_RECOM_PRODS) {
                            ret = STORE_ERR_TRUNCATED;
                            trunc_ok = 0;
                        }
                    }
                    if (trunc_ok) {
                        ret = sceNpCommerce2_ADAA054C(resp, buf_size, (u32)body_len);
                        if (ret >= 0) {
                            *recv_size_out = total_len;
                            if (status_byte[0] == 0U) {
                                ret = 0;
                                store_post_process_response(ctx, req, resp, buf_size,
                                                            *recv_size_out);
                            } else {
                                u32 cid = req->cmd_id;
                                int check_maint = 1;
                                ret = (int)((u32)status_byte[0] | (u32)STORE_ERR_SERVER_BASE);
                                if (cid == STORE_CMD_SET_RES_ADDR) {
                                    if (ret == STORE_ERR_SERVER_GENERIC) {
                                        ret = STORE_ERR_SERVER_ADDR_INVALID;
                                        check_maint = 0;
                                    }
                                } else if (cid == STORE_CMD_REG_CC || cid == 64U) {
                                    if (ret == STORE_ERR_SERVER_GENERIC) {
                                        ret = STORE_ERR_SERVER_CARD_INVALID;
                                        check_maint = 0;
                                    }
                                }
                                if (check_maint) {
                                    store_tlv_parse_err_flag(resp, *recv_size_out, &err_flag);
                                    if (err_flag == 1U)
                                        ret = STORE_ERR_MAINTENANCE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

/* =========================================================================
 * Request start dispatchers & creators (0x015564 .. 0x018064)
 * ========================================================================= */

int store_start_get_cat(u32 req_id, const char *cat_id, const char * const *reward_id,
                        u32 has_reward, u32 start_pos, u32 max_cnt)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        if (ctx->no_size_check == 0U && max_cnt >= 61U) {
            ret = STORE_ERR_INVALID_ARG;
        } else {
            store_refresh_keepalive(ctx);
            ret = store_send_get_cat(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                     req->cmd_id, &req->http_req_id,
                                     cat_id, reward_id, has_reward, start_pos, max_cnt);
            if (ret >= 0)
                sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
        }
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_start_guest_sess(u32 req_id, const char *country, const char *lang,
                           u32 yob, u32 mob, u32 dob)
{
    char console_id[80];
    StoreContext *ctx;
    StoreRequest *req;
    u32 proto_ver;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        ret = sceNpCommerce2_F0900551(ctx, &proto_ver);
        if (ret >= 0) {
            ret = sceNpCommerce2_157FA8AA(console_id, 65U);
            if (ret >= 0) {
                store_refresh_keepalive(ctx);
                ret = store_send_guest_sess(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                            req->cmd_id, &req->http_req_id, proto_ver,
                                            country, lang, yob, mob, dob, console_id);
                if (ret >= 0)
                    sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
            }
        }
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_get_country_caps(u32 ctx_id, const char *country, StoreCountryCaps *out_caps)
{
    char fallback_country[16];
    StoreContext *ctx = NULL;
    int ret;

    ret = sceNpCommerce2_0A6AB496(&g_store_mgr, ctx_id, &ctx, 1);
    if (ret >= 0) {
        sceKernelMemset(out_caps, 0, sizeof(*out_caps));
        if (!country) {
            ret = sceNpCommerce2_D5932A6C(ctx, fallback_country, 3U, 0);
            if (ret >= 0)
                country = fallback_country;
        }
        if (ret >= 0) {
            const char * const *scan;
            int found = 0;

            for (scan = g_country_tier_full; sceNet_lib_D8722983(*scan) != 0U; scan++) {
                if (sceNet_lib_A0F16ABD(*scan, country) == 0) {
                    out_caps->cap_flags = 7U;
                    sceNet_lib_B5CE388A(out_caps->country, *scan, 3U);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                for (scan = g_country_tier_mid; sceNet_lib_D8722983(*scan) != 0U; scan++) {
                    if (sceNet_lib_A0F16ABD(*scan, country) == 0) {
                        out_caps->cap_flags = 3U;
                        sceNet_lib_B5CE388A(out_caps->country, *scan, 3U);
                        found = 1;
                        break;
                    }
                }
            }
            if (!found) {
                for (scan = g_country_tier_basic; sceNet_lib_D8722983(*scan) != 0U; scan++) {
                    if (sceNet_lib_A0F16ABD(*scan, country) == 0) {
                        out_caps->cap_flags = 1U;
                        sceNet_lib_B5CE388A(out_caps->country, *scan, 3U);
                        found = 1;
                        break;
                    }
                }
            }
            if (!found)
                ret = STORE_ERR_COUNTRY_NOT_FOUND;
        }
    }
    if (ctx)
        sceNpCommerce2_77BE411B(&g_store_mgr);
    return ret;
}

int store_init_mgr(void)
{
    sceKernelMemset(&g_store_mgr, 0, sizeof(g_store_mgr));
    return 0;
}

void store_term_mgr(void)
{
}

int store_start_get_prod(u32 req_id, const char *cat_id, const char *prod_id,
                         const char * const *reward_id, u32 has_reward)
{
    StoreContext *ctx;
    StoreRequest *req;
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

int store_disp_create_add_cart(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_ADD_CART, req_id_out);
}

int store_start_add_cart(u32 req_id, const char * const *sku_ids, u32 sku_cnt,
                         const char * const *reward_ids, u32 reward_cnt)
{
    StoreContext *ctx;
    StoreRequest *req;
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

int store_disp_create_rem_cart(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_REM_CART, req_id_out);
}

int store_start_rem_cart(u32 req_id, const char *sku_id)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_rem_cart(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                  req->cmd_id, &req->http_req_id, sku_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_clear_cart(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_CLEAR_CART, req_id_out);
}

int store_start_clear_cart(u32 req_id)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_clear_cart(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                    req->cmd_id, &req->http_req_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_get_cart(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_CART, req_id_out);
}

int store_start_get_cart(u32 req_id)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_cart(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                  req->cmd_id, &req->http_req_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_pur_prev(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_PUR_PREV, req_id_out);
}

int store_disp_create_purch(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_PURCH, req_id_out);
}

int store_start_purchase(u32 req_id, const char *password, int quick_purchase, int auto_deposit)
{
    StoreContext *ctx;
    StoreRequest *req;
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

int store_disp_create_get_cc(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_CC, req_id_out);
}

int store_start_get_cc(u32 req_id)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_cc(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                req->cmd_id, &req->http_req_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_get_prov(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_PROV, req_id_out);
}

int store_start_get_prov(u32 req_id)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_prov(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                  req->cmd_id, &req->http_req_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_get_pay(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_PAY, req_id_out);
}

int store_disp_create_get_bill_info(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_BILL_INFO, req_id_out);
}

int store_start_payment_type(u32 req_id, u32 pay_type)
{
    StoreContext *ctx;
    StoreRequest *req;
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

int store_disp_create_get_drm(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_DRM, req_id_out);
}

int store_start_get_drm(u32 req_id, const char * const *sku_ids, u32 sku_cnt, const char *svc_id,
                        const char *reward_ids, u32 start_pos, u32 max_cnt, u32 drm_type)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_drm(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id,
                                 sku_ids, sku_cnt, svc_id, reward_ids,
                                 start_pos, max_cnt, drm_type);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_get_bill(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_BILL, req_id_out);
}

int store_disp_create_get_res_addr(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_RES_ADDR, req_id_out);
}

int store_start_get_bill(u32 req_id)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_bill(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                  req->cmd_id, &req->http_req_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_reg_cc(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_REG_CC, req_id_out);
}

int store_start_billing(u32 req_id, const StoreBillingInfo *billing)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_billing(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id, billing);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_get_addr(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_ADDR, req_id_out);
}

int store_start_postal_code(u32 req_id, const char *postal_code)
{
    StoreContext *ctx;
    StoreRequest *req;
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

int store_disp_create_get_wal(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_WAL, req_id_out);
}

int store_start_get_wal(u32 req_id)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_wal(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_topup_cc(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_TOPUP_CC, req_id_out);
}

int store_disp_create_top_edy(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_TOP_EDY, req_id_out);
}

int store_start_topup_amount(u32 req_id, u32 amount)
{
    StoreContext *ctx;
    StoreRequest *req;
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

int store_disp_create_top_edy_res(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_TOP_EDY_RES, req_id_out);
}

int store_start_edy_result(u32 req_id, const char *receipt_id, int is_ok)
{
    StoreContext *ctx;
    StoreRequest *req;
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

int store_disp_create_vchr_prev(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_VCHR_RED_PREV, req_id_out);
}

int store_disp_create_vchr_red(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_VCHR_RED, req_id_out);
}

int store_start_voucher(u32 req_id, const char *code_a, const char *code_b,
                        const char *code_c, int auto_deposit)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_voucher(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id,
                                 code_a, code_b, code_c, auto_deposit);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_event_log(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_EVENT_LOG, req_id_out);
}

int store_start_event_log(u32 req_id, u32 event_type, const char *id_str, const char *url_str)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_event_log(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                   req->cmd_id, &req->http_req_id,
                                   event_type, id_str, url_str);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_srch_prod(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_SRCH_PROD, req_id_out);
}

int store_start_search_prod(u32 req_id, u32 svc_type, u32 start_pos, u32 max_cnt,
                            const StoreSearchQuery *queries, u32 query_cnt, u32 sort_method)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_search_prod(svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                     req->cmd_id, &req->http_req_id,
                                     start_pos, max_cnt, queries, query_cnt, sort_method);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_get_cnsl_act(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_CNSL_ACT, req_id_out);
}

int store_start_get_cnsl_act(u32 req_id)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_prov(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                  req->cmd_id, &req->http_req_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_get_epw(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_EPW, req_id_out);
}

int store_start_get_epw(u32 req_id)
{
    StoreContext *ctx;
    StoreRequest *req;
    int ret = store_lock_ctx_req(req_id, &ctx, &req);

    if (ret >= 0) {
        store_refresh_keepalive(ctx);
        ret = store_send_get_epw(ctx->svc_type, ctx->http_tmpl_id, ctx->url_buf,
                                 req->cmd_id, &req->http_req_id);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->last_activity_us);
    }
    return store_unlock_ctx_req(ctx, req, ret);
}

int store_disp_create_guest_sess(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_CR_GUEST_SESS, req_id_out);
}

int store_disp_create_prod_list(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_PROD_LIST, req_id_out);
}

int store_start_prod_list(u32 req_id, const char * const *prod_ids, u32 prod_cnt)
{
    StoreContext *ctx;
    StoreRequest *req;
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

int store_disp_create_recom_prods(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_RECOM_PRODS, req_id_out);
}

int store_start_recom_prods(u32 req_id, const char * const *prod_ids, u32 prod_cnt,
                            u32 start_pos, u32 max_cnt, u32 recom_type)
{
    StoreContext *ctx;
    StoreRequest *req;
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

int store_disp_create_prod_ids(u32 ctx_id, u32 *req_id_out)
{
    return store_create_req(ctx_id, STORE_CMD_GET_PROD_IDS, req_id_out);
}

int store_start_get_prod_ids(u32 req_id, const char *content_id, const char *file_id)
{
    StoreContext *ctx;
    StoreRequest *req;
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

/* =========================================================================
 * Context configuration & request lifecycle (0x0181DC .. 0x018484)
 * ========================================================================= */

int store_set_ctx_option(u32 ctx_id, u32 option_val)
{
    StoreContext *ctx = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_store_mgr, ctx_id, &ctx, 1);

    if (ret >= 0)
        ret = sceNpCommerce2_EE7255FE(ctx, option_val);
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
    int ret = sceNpCommerce2_0A6AB496(&g_store_mgr, ctx_id, &ctx, 1);

    if (ret >= 0) {
        StoreRequest *req = (StoreRequest *)sceNetIfhandle_lib_15CFE3C0(sizeof(StoreRequest));
        if (!req) {
            ret = STORE_ERR_OUT_OF_MEMORY;
        } else {
            sceKernelMemset(req, 0, sizeof(StoreRequest));
            req->cmd_id = cmd_id;
            ret = sceNpCommerce2_06618C81(&g_store_mgr, req, ctx_id, req_id_out);
            if (ret < 0)
                sceNetIfhandle_lib_76BAD213(req);
        }
    }
    if (ctx)
        sceNpCommerce2_77BE411B(&g_store_mgr);
    return ret;
}

int store_post_process_response(StoreContext *ctx, StoreRequest *req,
                                const u8 *resp, u32 buf_size, u32 resp_size)
{
    u32 acct[24];
    u32 sess_root[12];
    u32 auth_root[16];
    u16 cid = req->cmd_id;

    if (cid == STORE_CMD_AUTHENTICATE || cid == STORE_CMD_CR_SESS || cid == STORE_CMD_CR_GUEST_SESS) {
        u32 *root_ptr;
        int parse_ret;
        if (cid == STORE_CMD_AUTHENTICATE) {
            root_ptr = auth_root;
            parse_ret = store_tlv_parse_auth_root(auth_root, resp, resp_size);
        } else {
            root_ptr = sess_root;
            parse_ret = store_tlv_init_sess_root(sess_root, resp, resp_size);
        }
        if (parse_ret >= 0 && store_tlv_parse_account_info(root_ptr, acct) >= 0) {
            sceNet_lib_B5CE388A(ctx->country_code, (const char *)(u32)acct[16], 4U);
            ctx->country_code[3] = '\0';
            sceNet_lib_B5CE388A(ctx->lang_code, (const char *)(u32)acct[17], 4U);
            ctx->country_code[3] = '\0';
            sceNpCommerce2_F8D175FA(resp, buf_size, ctx->country_code, ctx->lang_code);
        }
        cid = req->cmd_id;
    }

    if (cid == STORE_CMD_CR_SESS || cid == STORE_CMD_START_REG ||
        cid == STORE_CMD_AUTHENTICATE || cid == STORE_CMD_VERIFY_DOB ||
        cid == STORE_CMD_CR_GUEST_SESS) {
        if (resp_size <= sizeof(ctx->session_buf)) {
            ctx->session_len = resp_size;
            sceKernelMemcpy(ctx->session_buf, resp, resp_size);
        } else {
            ctx->session_len = 0U;
        }
    }
    return 0;
}
