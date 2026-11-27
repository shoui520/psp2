#include "store_internal.h"

u32 g_store_init;

/* Internal SDK-checked helpers (placed at end of TU, matching Sony 0x004B58..0x005248) */
static int store_checked_recv_a(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out,
                                int strict_size) __attribute__((noinline));
static int store_checked_get_cat(u32 req_id, const char *cat_id, const char * const *reward_id,
                                 u32 has_reward, u32 start_pos, u32 max_cnt,
                                 int strict_check) __attribute__((noinline));
static int store_checked_recv_b(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out,
                                int strict_size) __attribute__((noinline));
static int store_checked_get_prod(u32 req_id, const char *cat_id, const char *prod_id,
                                  const char * const *reward_id, u32 has_reward,
                                  int strict_check) __attribute__((noinline));
static int store_checked_recv_c(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out,
                                int strict_size) __attribute__((noinline));
static int store_checked_get_prod_list(u32 req_id, const char * const *prod_ids, u32 prod_cnt,
                                       int strict_check) __attribute__((noinline));

static inline __attribute__((always_inline)) void
store_extract_id_prefix(char *dst, const char *src)
{
    const char *cur = src;
    u32 len;

    while (*cur != '\0' && *cur != '-')
        cur++;
    if (*cur != '\0')
        cur++;
    while (*cur != '\0' && *cur != '-')
        cur++;
    len = (u32)(cur - src);
    if (len > 31U)
        len = 31U;
    sceKernelMemcpy(dst, src, len);
    dst[len] = '\0';
}

/* =========================================================================
 * Exported API wrappers in exact Sony address order (0x000000 .. 0x004A0C)
 * ========================================================================= */

int sceNpCommerce2Store_8E76A121(u32 req_id, u32 svc_type, u32 start_pos, u32 max_cnt,
                                 const StoreSearchQuery *queries, u32 query_cnt, u32 sort_method)
{
    u32 idx;

    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (svc_type - 1U >= 8U)
        return STORE_ERR_INVALID_ARG;
    if (query_cnt != 1U || sort_method >= 8U)
        return STORE_ERR_INVALID_ARG;
    for (idx = 0U; idx < query_cnt; idx++) {
        if (queries[idx].key_index >= 7U || !queries[idx].value || queries[idx].value_len == 0U)
            return STORE_ERR_INVALID_ARG;
    }
    return store_start_search_prod(req_id, svc_type, start_pos, max_cnt,
                                   queries, query_cnt, sort_method);
}

int sceNpCommerce2Store_81044B28(u32 req_id, const char * const *prod_ids, u32 prod_cnt,
                                 u32 start_pos, u32 max_cnt, u32 recom_type)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (recom_type == 1U) {
        if (!prod_ids || prod_cnt == 0U || max_cnt >= 61U)
            return STORE_ERR_INVALID_ARG;
    } else if (recom_type == 2U) {
        if (max_cnt >= 61U)
            return STORE_ERR_INVALID_ARG;
    } else {
        return STORE_ERR_INVALID_ARG;
    }
    return store_start_recom_prods(req_id, prod_ids, prod_cnt, start_pos, max_cnt, recom_type);
}

int sceNpCommerce2Store_E3D1EA90(char *content_id)
{
    char *cur = content_id;
    u32 dash_cnt = 0U;

    if (!cur)
        return STORE_ERR_INVALID_ID;
    while (1) {
        char *hit = sceNet_lib_BCBE14CF(cur, '-');
        dash_cnt++;
        if (!hit)
            return STORE_ERR_INVALID_ID;
        cur = hit + 1;
        if (dash_cnt >= 2U) {
            char *third = sceNet_lib_BCBE14CF(cur, '-');
            if (third)
                *third = '\0';
            return 0;
        }
    }
}

int sceNpCommerce2Store_B20C910B(const char *content_id, char *out_buf, u32 out_size)
{
    const char *cur = content_id;
    u32 dash_cnt = 0U;

    if (!cur)
        return STORE_ERR_INVALID_ID;
    while (1) {
        const char *hit = sceNet_lib_BCBE14CF(cur, '-');
        dash_cnt++;
        if (!hit)
            return STORE_ERR_INVALID_ID;
        cur = hit + 1;
        if (dash_cnt >= 2U) {
            const char *end = sceNet_lib_BCBE14CF(cur, '-');
            u32 len;
            if (!end)
                end = content_id + sceNet_lib_D8722983(content_id);
            len = (u32)(end - content_id);
            if (len >= out_size)
                return STORE_ERR_BUF_TOO_SMALL;
            sceKernelMemcpy(out_buf, content_id, len);
            out_buf[len] = '\0';
            return 0;
        }
    }
}

int sceNpCommerce2Store_DA7B5568(const char *cat_id, u32 *is_store_root)
{
    u32 id_len;
    u32 pfx_len;

    if ((!cat_id) | (!is_store_root))
        return STORE_ERR_INVALID_ARG;
    id_len = sceNet_lib_D8722983(cat_id);
    pfx_len = sceNet_lib_D8722983("IP910");
    if (id_len >= pfx_len + sceNet_lib_D8722983("-NPIA9000") + 4U) {
        if (sceNet_lib_94DCA9F0("IP910", cat_id, sceNet_lib_D8722983("IP910")) != 0) {
            *is_store_root = 0U;
        } else {
            const char *digit = cat_id + sceNet_lib_D8722983("IP910");
            if ((u8)(*digit - '1') < 5U) {
                const char *mid = digit + 1;
                if (sceNet_lib_94DCA9F0("-NPIA9000", mid, sceNet_lib_D8722983("-NPIA9000")) != 0) {
                    *is_store_root = 0U;
                } else {
                    const char *tail = mid + sceNet_lib_D8722983("-NPIA9000");
                    *is_store_root = ((u8)(tail[0] - '5') < 5U && tail[1] == '_') ? 1U : 0U;
                }
            } else {
                *is_store_root = 0U;
            }
        }
    } else {
        *is_store_root = 0U;
    }
    return 0;
}

int sceNpCommerce2Store_7B6C5AEB(const char *cat_id, u32 *is_cc_cat)
{
    u32 id_len;
    u32 pfx_len;

    if ((!cat_id) | (!is_cc_cat))
        return STORE_ERR_INVALID_ARG;
    id_len = sceNet_lib_D8722983(cat_id);
    pfx_len = sceNet_lib_D8722983("IP9100-NPIA0900");
    if (id_len >= pfx_len + sceNet_lib_D8722983("_00-CC") + 1U) {
        if (sceNet_lib_94DCA9F0("IP9100-NPIA0900", cat_id,
                                sceNet_lib_D8722983("IP9100-NPIA0900")) != 0) {
            *is_cc_cat = 0U;
        } else {
            const char *digit = cat_id + sceNet_lib_D8722983("IP9100-NPIA0900");
            if ((u8)(*digit - '2') < 5U) {
                if (sceNet_lib_94DCA9F0("_00-CC", digit + 1,
                                        sceNet_lib_D8722983("_00-CC")) != 0) {
                    *is_cc_cat = 0U;
                } else {
                    sceNet_lib_D8722983("_00-CC");
                    *is_cc_cat = 1U;
                }
            } else {
                *is_cc_cat = 0U;
            }
        }
    } else {
        *is_cc_cat = 0U;
    }
    return 0;
}

int sceNpCommerce2Store_59D8A74D(void)
{
    int ret;
    if (g_store_init)
        return STORE_ERR_ALREADY_INITIALIZED;
    ret = store_init_mgr();
    if (ret < 0)
        return ret;
    g_store_init = 1U;
    return 0;
}

int sceNpCommerce2Store_8455D3ED(void)
{
    if (g_store_init) {
        store_term_mgr();
        g_store_init = 0U;
    }
    return 0;
}

int sceNpCommerce2Store_48FE9646(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_err_root(out, resp, resp_size);
}

int sceNpCommerce2Store_CD27E505(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_err_item(parent, index, out);
}

int sceNpCommerce2Store_873887CF(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    return store_checked_recv_a(req_id, resp, buf_size, recv_size_out, 0);
}

int sceNpCommerce2Store_2A06624D(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_sess_root(out, resp, resp_size);
}

int sceNpCommerce2Store_45E24DCA(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_sess_currency(parent, index, out);
}

int sceNpCommerce2Store_D601DE6A(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_eula_flag(parent, out);
}

int sceNpCommerce2Store_7D7FB404(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_account_info(parent, out);
}

int sceNpCommerce2Store_ABF975F8(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_reg_rules(parent, out);
}

int sceNpCommerce2Store_28EF259E(u32 req_id, const char *cat_id, const char * const *reward_id,
                                 u32 has_reward, u32 start_pos, u32 max_cnt)
{
    return store_checked_get_cat(req_id, cat_id, reward_id, has_reward, start_pos, max_cnt, 0);
}

int sceNpCommerce2Store_98F66F2C(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    return store_checked_recv_b(req_id, resp, buf_size, recv_size_out, 0);
}

int sceNpCommerce2Store_607146F0(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_category_info(parent, out, 2);
}

int sceNpCommerce2Store_B2DDD9D5(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_rating_nth_cat(parent, index, out);
}

int sceNpCommerce2Store_C228C7BF(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cat_sku(parent, out, 2);
}

int sceNpCommerce2Store_B93B30CD(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_subscr_ent(parent, index, out);
}

int sceNpCommerce2Store_869624DB(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_subscr_sku(parent, index, out);
}

int sceNpCommerce2Store_4E84A105(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_download_file(parent, index, out);
}

int sceNpCommerce2Store_E7BBC497(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_subscr_sku(parent, index, out);
}

int sceNpCommerce2Store_B477732B(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_subscr_sku(parent, index, out);
}

int sceNpCommerce2Store_F7D8224B(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_subscr_sku(parent, index, out);
}

int sceNpCommerce2Store_219B9CD4(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_subscr_sku(parent, index, out);
}

int sceNpCommerce2Store_A7C4A4E0(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_entitlement(parent, index, out);
}

int sceNpCommerce2Store_F5E850B6(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_reward_std(parent, out);
}

int sceNpCommerce2Store_EEAC383D(u32 req_id, const char *cat_id, const char *prod_id,
                                 const char * const *reward_id, u32 has_reward)
{
    return store_checked_get_prod(req_id, cat_id, prod_id, reward_id, has_reward, 0);
}

int sceNpCommerce2Store_619AFBAE(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    return store_checked_recv_c(req_id, resp, buf_size, recv_size_out, 0);
}

int sceNpCommerce2Store_9B001542(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_product_info(parent, out, 2);
}

int sceNpCommerce2Store_5CAF925C(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_rating_nth_prod(parent, index, out);
}

int sceNpCommerce2Store_99A17682(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_product_sku(parent, index, out, 2);
}

int sceNpCommerce2Store_4B3A29D4(const u32 *parent, u32 index, u32 *code_out, u32 *flags_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!code_out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_audio_lang(parent, index, code_out, flags_out);
}

int sceNpCommerce2Store_39D8AB1E(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_subtitle_lang(parent, index, out_ptr);
}

int sceNpCommerce2Store_92393647(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_entitlement(parent, index, out);
}

int sceNpCommerce2Store_9DBCFF3D(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_entitlement(parent, index, out);
}

int sceNpCommerce2Store_E5ECA4DF(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_prod_genre(parent, index, out_ptr);
}

int sceNpCommerce2Store_783CC832(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_credit_info(parent, index, out);
}

int sceNpCommerce2Store_236FC217(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_link_info(parent, index, out);
}

int sceNpCommerce2Store_7A08C301(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_reward_std(parent, out);
}

int sceNpCommerce2Store_ECF70261(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_add_cart(ctx_id, req_id_out);
}

int sceNpCommerce2Store_EF8F317D(u32 req_id, const char * const *sku_ids, u32 sku_cnt,
                                 const char * const *reward_ids, u32 reward_cnt)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!sku_ids || sku_cnt == 0U || (reward_cnt != 0U && reward_cnt != sku_cnt))
        return STORE_ERR_INVALID_ARG;
    return store_start_add_cart(req_id, sku_ids, sku_cnt, reward_ids, reward_cnt);
}

int sceNpCommerce2Store_FB5F3C12(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_CCA8601C(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_add_cart_root(out, resp, resp_size);
}

int sceNpCommerce2Store_5115E0CB(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_add_cart_wallet(parent, out);
}

int sceNpCommerce2Store_5CE497F6(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_add_cart_summary(parent, out);
}

int sceNpCommerce2Store_61FD9968(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_entry(parent, index, out);
}

int sceNpCommerce2Store_EFFCA1ED(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_item(parent, 0, 0U, out);
}

int sceNpCommerce2Store_B8E4EED6(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_rating_first(parent, out);
}

int sceNpCommerce2Store_74B79FF2(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_sku_brief(parent, out);
}

int sceNpCommerce2Store_782ED6EB(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_entitlement(parent, index, out);
}

int sceNpCommerce2Store_D2E48887(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_download_file(parent, index, out);
}

int sceNpCommerce2Store_CADDC96B(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_sku_full(parent, out, 2);
}

int sceNpCommerce2Store_E1776B29(const u32 *parent, u32 index, u32 *code_out, u32 *flags_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!code_out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_audio_lang(parent, index, code_out, flags_out);
}

int sceNpCommerce2Store_264E9CAA(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_subtitle_lang(parent, index, out_ptr);
}

int sceNpCommerce2Store_F4261841(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_reward_std(parent, out);
}

int sceNpCommerce2Store_DD8F307B(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_entitlement(parent, index, out);
}

int sceNpCommerce2Store_B9133D27(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_rem_cart(ctx_id, req_id_out);
}

int sceNpCommerce2Store_CA9FB11B(u32 req_id, const char *sku_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!sku_id)
        return STORE_ERR_INVALID_ARG;
    return store_start_rem_cart(req_id, sku_id);
}

int sceNpCommerce2Store_4534FD80(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_21134841(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_rem_cart_root(out, resp, resp_size);
}

int sceNpCommerce2Store_BE626428(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_rem_cart_wallet(parent, out);
}

int sceNpCommerce2Store_C8133F70(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_rem_cart_summary(parent, out);
}

int sceNpCommerce2Store_AE6DAF88(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_clear_cart(ctx_id, req_id_out);
}

int sceNpCommerce2Store_AD64726E(u32 req_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_clear_cart(req_id);
}

int sceNpCommerce2Store_59E39F46(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_6301672F(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_cart(ctx_id, req_id_out);
}

int sceNpCommerce2Store_701385D5(u32 req_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_get_cart(req_id);
}

int sceNpCommerce2Store_850F5E58(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_99300AC2(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_get_cart_root(out, resp, resp_size);
}

int sceNpCommerce2Store_211DFA3C(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_cart_wallet(parent, out);
}

int sceNpCommerce2Store_48F4C175(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_cart_summary(parent, out);
}

int sceNpCommerce2Store_41C60662(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_pur_prev(ctx_id, req_id_out);
}

int sceNpCommerce2Store_ECE95BB2(u32 req_id, const char *password, int quick_purchase,
                                 int auto_deposit)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_purchase(req_id, password, quick_purchase, auto_deposit);
}

int sceNpCommerce2Store_1F4FFF9A(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_4E280B0A(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_pur_prev_root(out, resp, resp_size);
}

int sceNpCommerce2Store_E1DC1A08(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_pur_prev_wallet(parent, out);
}

int sceNpCommerce2Store_A8B9FFDB(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_pur_prev_summary(parent, out);
}

int sceNpCommerce2Store_694667CF(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_pur_prev_tax(parent, out);
}

int sceNpCommerce2Store_CAB4A8F6(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_purch(ctx_id, req_id_out);
}

int sceNpCommerce2Store_F5381482(u32 req_id, const char *password, int quick_purchase,
                                 int auto_deposit)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_purchase(req_id, password, quick_purchase, auto_deposit);
}

int sceNpCommerce2Store_B1957179(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_0A7B6B92(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_purch_root(out, resp, resp_size);
}

int sceNpCommerce2Store_849E6B93(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_purch_wallet(parent, out);
}

int sceNpCommerce2Store_FB5D86D3(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_purch_summary(parent, out);
}

int sceNpCommerce2Store_19FAC163(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_purch_tax(parent, out);
}

int sceNpCommerce2Store_A89F0478(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_cc(ctx_id, req_id_out);
}

int sceNpCommerce2Store_8C0C2F7C(u32 req_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_get_cc(req_id);
}

int sceNpCommerce2Store_EBBA8ECE(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_7136C275(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_get_cc_root(out, resp, resp_size);
}

int sceNpCommerce2Store_259AE74B(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_cc_card(parent, out);
}

int sceNpCommerce2Store_320C18DB(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_prov(ctx_id, req_id_out);
}

int sceNpCommerce2Store_39757926(u32 req_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_get_prov(req_id);
}

int sceNpCommerce2Store_6ACDFAC8(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_FFEBE5FF(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_get_prov_root(out, resp, resp_size);
}

int sceNpCommerce2Store_998EB0CC(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_prov_item(parent, index, out);
}

int sceNpCommerce2Store_6C5AE9E5(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_city_info(parent, index, out);
}

int sceNpCommerce2Store_73AE0FA0(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_pay(ctx_id, req_id_out);
}

int sceNpCommerce2Store_995ECF93(u32 req_id, u32 pay_type)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_payment_type(req_id, pay_type);
}

int sceNpCommerce2Store_CBBFF272(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_4D5DA905(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_get_pay_root(out, resp, resp_size);
}

int sceNpCommerce2Store_91749B96(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_pay_item(parent, index, out);
}

int sceNpCommerce2Store_B961BF22(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_drm(ctx_id, req_id_out);
}

int sceNpCommerce2Store_1A7F75D7(u32 req_id, const char * const *sku_ids, u32 sku_cnt,
                                 const char *svc_id, const char *reward_ids, u32 start_pos,
                                 u32 max_cnt, u32 drm_type)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_get_drm(req_id, sku_ids, sku_cnt, svc_id, reward_ids,
                               start_pos, max_cnt, drm_type);
}

int sceNpCommerce2Store_A52FFFF8(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_18457EB9(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_get_drm_root(out, resp, resp_size);
}

int sceNpCommerce2Store_57DD55E5(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_drm_item(parent, index, out);
}

int sceNpCommerce2Store_063A16A4(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_drm_reward(parent, out, 2);
}

int sceNpCommerce2Store_CE84A5DC(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_entitlement(parent, index, out);
}

int sceNpCommerce2Store_6961822C(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_drm_child(parent, index, out);
}

int sceNpCommerce2Store_B1BEAFED(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_bill(ctx_id, req_id_out);
}

int sceNpCommerce2Store_7A1291A6(u32 req_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_get_bill(req_id);
}

int sceNpCommerce2Store_E55BC0DF(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_EAF03127(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_address_root(out, resp, resp_size);
}

int sceNpCommerce2Store_D58BC86C(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_reg_cc(ctx_id, req_id_out);
}

int sceNpCommerce2Store_12378B6A(u32 req_id, const StoreBillingInfo *billing)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!billing)
        return STORE_ERR_INVALID_ARG;
    if (billing->street_a_len == 0U)
        return STORE_ERR_INVALID_STREET;
    return store_start_billing(req_id, billing);
}

int sceNpCommerce2Store_53716B04(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_8EABFF3E(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_addr(ctx_id, req_id_out);
}

int sceNpCommerce2Store_472E4D84(u32 req_id, const char *postal_code)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!postal_code)
        return STORE_ERR_INVALID_ARG;
    return store_start_postal_code(req_id, postal_code);
}

int sceNpCommerce2Store_B0E5DFD8(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_6EB54FA3(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_addr_lookup_root(out, resp, resp_size);
}

int sceNpCommerce2Store_DA757050(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_wal(ctx_id, req_id_out);
}

int sceNpCommerce2Store_B8889A67(u32 req_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_get_wal(req_id);
}

int sceNpCommerce2Store_0E2B656E(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_C7B9E02B(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_get_wal_root(out, resp, resp_size);
}

int sceNpCommerce2Store_8B81197E(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_wal_info(parent, out);
}

int sceNpCommerce2Store_7B3A8218(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_topup_cc(ctx_id, req_id_out);
}

int sceNpCommerce2Store_D2517B02(u32 req_id, u32 amount)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_topup_amount(req_id, amount);
}

int sceNpCommerce2Store_ECE10E5B(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_49E768E9(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_topup_cc_root(out, resp, resp_size);
}

int sceNpCommerce2Store_9AA823E4(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_topup_cc_card(parent, out);
}

int sceNpCommerce2Store_0A5949EF(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_top_edy(ctx_id, req_id_out);
}

int sceNpCommerce2Store_C57CF651(u32 req_id, u32 amount)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_topup_amount(req_id, amount);
}

int sceNpCommerce2Store_6FE1F29E(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_346E1F56(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_top_edy_root(out, resp, resp_size);
}

int sceNpCommerce2Store_3572E4D7(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_edy_info(parent, out);
}

int sceNpCommerce2Store_75C205F7(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_top_edy_res(ctx_id, req_id_out);
}

int sceNpCommerce2Store_68B07CDC(u32 req_id, const char *receipt_id, int is_ok)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!receipt_id)
        return STORE_ERR_INVALID_ARG;
    return store_start_edy_result(req_id, receipt_id, is_ok);
}

int sceNpCommerce2Store_334F89A3(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_2E290D53(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_vchr_prev(ctx_id, req_id_out);
}

int sceNpCommerce2Store_47A8A5B4(u32 req_id, const char *code_a, const char *code_b,
                                 const char *code_c)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!code_a) | (!code_b) | (!code_c))
        return STORE_ERR_INVALID_ARG;
    return store_start_voucher(req_id, code_a, code_b, code_c, 0);
}

int sceNpCommerce2Store_FEDE1E87(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_2180FE6C(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_vchr_prev_root(out, resp, resp_size);
}

int sceNpCommerce2Store_5EC8E7B8(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_vchr_prev_summary(parent, out);
}

int sceNpCommerce2Store_00292401(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_vchr_prev_items(parent, out);
}

int sceNpCommerce2Store_FC6408EC(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_voucher_item(parent, index, out);
}

int sceNpCommerce2Store_25579A90(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_rating_first(parent, out);
}

int sceNpCommerce2Store_66BC1072(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_download_list(parent, out);
}

int sceNpCommerce2Store_D22ADDA7(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_download_file(parent, index, out);
}

int sceNpCommerce2Store_65D72F22(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_video_info(parent, out, 2);
}

int sceNpCommerce2Store_2581BC37(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_entitlement(parent, index, out);
}

int sceNpCommerce2Store_59443417(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_prod_genre(parent, index, out_ptr);
}

int sceNpCommerce2Store_1C4CE9CB(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_credit_info(parent, index, out);
}

int sceNpCommerce2Store_7063CC9D(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_link_info(parent, index, out);
}

int sceNpCommerce2Store_548C6487(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_reward_std(parent, out);
}

int sceNpCommerce2Store_F036856B(const u32 *parent, u32 index, u32 *code_out, u32 *flags_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!code_out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_audio_lang(parent, index, code_out, flags_out);
}

int sceNpCommerce2Store_7F642484(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_subtitle_lang(parent, index, out_ptr);
}

int sceNpCommerce2Store_9D3888D8(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_video_sku(parent, index, out, 2);
}

int sceNpCommerce2Store_92E53CD4(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_vchr_red(ctx_id, req_id_out);
}

int sceNpCommerce2Store_D232FB91(u32 req_id, const char *code_a, const char *code_b,
                                 const char *code_c, int auto_deposit)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!code_a) | (!code_b) | (!code_c))
        return STORE_ERR_INVALID_ARG;
    return store_start_voucher(req_id, code_a, code_b, code_c, auto_deposit);
}

int sceNpCommerce2Store_6AECBA79(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_D63D12BF(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_vchr_red_root(out, resp, resp_size);
}

int sceNpCommerce2Store_ED2C778E(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_vchr_red_summary(parent, out);
}

int sceNpCommerce2Store_CBC27838(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_vchr_red_items(parent, out);
}

int sceNpCommerce2Store_7C45D60E(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_res_addr(ctx_id, req_id_out);
}

int sceNpCommerce2Store_09B3E27D(u32 req_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_get_bill(req_id);
}

int sceNpCommerce2Store_8644CCFB(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_770C5947(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_res_addr_root(out, resp, resp_size);
}

int sceNpCommerce2Store_78BA836A(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_event_log(ctx_id, req_id_out);
}

int sceNpCommerce2Store_7D654C92(u32 req_id, u32 event_type, const char *id_str,
                                 const char *url_str)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_event_log(req_id, event_type, id_str, url_str);
}

int sceNpCommerce2Store_C0BB62AD(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_DDD2C398(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_srch_prod(ctx_id, req_id_out);
}

int sceNpCommerce2Store_D70F1117(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_54D94925(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_search_root(out, resp, resp_size);
}

int sceNpCommerce2Store_94E8DAB3(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_search_item_paged(parent, index, out);
}

int sceNpCommerce2Store_465A6BBA(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_search_tag_ptr(parent, index, out_ptr);
}

int sceNpCommerce2Store_E6009051(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_search_item_ext(parent, index, out);
}

int sceNpCommerce2Store_B1F9111F(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_search_item_single(parent, out);
}

int sceNpCommerce2Store_6DDED3F7(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_bill_info(ctx_id, req_id_out);
}

int sceNpCommerce2Store_E1A8D7FF(u32 req_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_payment_type(req_id, 0U);
}

int sceNpCommerce2Store_5857AF83(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_4E3E0594(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_billing_root(out, resp, resp_size);
}

int sceNpCommerce2Store_FBF61E40(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_billing_prov(parent, index, out);
}

int sceNpCommerce2Store_BC7F8DA6(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_billing_pay(parent, index, out);
}

int sceNpCommerce2Store_A93E02A6(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_billing_card(parent, out);
}

int sceNpCommerce2Store_166EA2D5(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_cnsl_act(ctx_id, req_id_out);
}

int sceNpCommerce2Store_F22C4985(u32 req_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_get_cnsl_act(req_id);
}

int sceNpCommerce2Store_47540138(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_0E083AEA(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_cnsl_act_root(out, resp, resp_size);
}

int sceNpCommerce2Store_84C4226F(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_console_act(parent, index, out);
}

int sceNpCommerce2Store_A165F819(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_get_epw(ctx_id, req_id_out);
}

int sceNpCommerce2Store_42769097(u32 req_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_start_get_epw(req_id);
}

int sceNpCommerce2Store_C42B0607(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_BCD5575A(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_epw_root(out, resp, resp_size);
}

int sceNpCommerce2Store_C7AFBA23(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_guest_sess(ctx_id, req_id_out);
}

int sceNpCommerce2Store_D31C1673(u32 req_id, const char *country, const char *lang,
                                 u32 yob, u32 mob, u32 dob)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!country) | (!lang) | (yob == 0U) | (mob == 0U) | (dob == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_start_guest_sess(req_id, country, lang, yob, mob, dob);
}

int sceNpCommerce2Store_5C15E8DA(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_C2A7C748(u32 ctx_id, u32 *req_id_out)
{
    if (sceKernelCheckThreadStack() < STORE_MIN_STACK_STD)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_prod_list(ctx_id, req_id_out);
}

int sceNpCommerce2Store_E3A5AC0C(u32 req_id, const char * const *prod_ids, u32 prod_cnt)
{
    return store_checked_get_prod_list(req_id, prod_ids, prod_cnt, 1);
}

int sceNpCommerce2Store_85F7B364(u32 req_id, const char * const *prod_ids, u32 prod_cnt)
{
    return store_checked_get_prod_list(req_id, prod_ids, prod_cnt, 0);
}

int sceNpCommerce2Store_C9652539(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (sceKernelCheckThreadStack() < STORE_MIN_STACK_STD)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (buf_size <= 0x3FFFFU)
        return STORE_ERR_RESP_BUF_LIMIT;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_B9108E1C(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (sceKernelCheckThreadStack() < STORE_MIN_STACK_STD)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_20A1FDAD(u32 *out, const u8 *resp, u32 resp_size)
{
    if (sceKernelCheckThreadStack() < STORE_MIN_STACK_STD)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_prod_list_root(out, resp, resp_size);
}

int sceNpCommerce2Store_7BBE5E6F(const u32 *parent, u32 index, u32 *out)
{
    u32 elem[12];
    int ret;

    if (sceKernelCheckThreadStack() < STORE_MIN_STACK_INNER)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (sceKernelCheckThreadStack() < STORE_MIN_STACK_STD)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!parent)
        return STORE_ERR_INVALID_ARG;
    ret = store_tlv_parse_prod_list_item(parent, index, elem);
    if (ret < 0)
        return ret;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!out)
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_category_item(elem, out);
}

int sceNpCommerce2Store_0F42E599(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_entitlement(parent, index, out);
}

int sceNpCommerce2Store_F132A92F(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_entitlement(parent, index, out);
}

int sceNpCommerce2Store_8C9B3738(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_subtitle_lang(parent, index, out_ptr);
}

int sceNpCommerce2Store_C6FDB930(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_entitlement(parent, index, out);
}

int sceNpCommerce2Store_96E5B56F(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_reward_ext(parent, out);
}

int sceNpCommerce2Store_EF296653(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_subtitle_lang(parent, index, out_ptr);
}

int sceNpCommerce2Store_7EB1A34C(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_sku_full(parent, out, 3);
}

int sceNpCommerce2Store_43CF3A7A(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_video_info(parent, out, 3);
}

int sceNpCommerce2Store_BAFBB9D2(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_prod_genre(parent, index, out_ptr);
}

int sceNpCommerce2Store_31FFC518(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_credit_info(parent, index, out);
}

int sceNpCommerce2Store_E9E20646(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_link_info(parent, index, out);
}

int sceNpCommerce2Store_CC69487B(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_reward_ext(parent, out);
}

int sceNpCommerce2Store_26781C1F(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_subtitle_lang(parent, index, out_ptr);
}

int sceNpCommerce2Store_E2367A5C(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_cat_icon_url(parent, index, out_ptr);
}

int sceNpCommerce2Store_8E216CEB(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_video_sku(parent, index, out, 3);
}

int sceNpCommerce2Store_4F460A0D(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cat_sku(parent, out, 3);
}

int sceNpCommerce2Store_4F945E73(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_rating_nth_cat(parent, index, out);
}

int sceNpCommerce2Store_3370450A(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_reward_ext(parent, out);
}

int sceNpCommerce2Store_7411D8E7(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_cat_icon_url(parent, index, out_ptr);
}

int sceNpCommerce2Store_9E63F7D5(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_rating_nth_prod(parent, index, out);
}

int sceNpCommerce2Store_D830992C(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_product_sku(parent, index, out, 3);
}

int sceNpCommerce2Store_94E55872(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_prod_genre(parent, index, out_ptr);
}

int sceNpCommerce2Store_E4E600A5(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_reward_ext(parent, out);
}

int sceNpCommerce2Store_93A4DBE4(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_credit_info(parent, index, out);
}

int sceNpCommerce2Store_E1B3A08D(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_link_info(parent, index, out);
}

int sceNpCommerce2Store_A47A19C1(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_cat_icon_url(parent, index, out_ptr);
}

int sceNpCommerce2Store_1FBC9F13(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_category_info(parent, out, 3);
}

int sceNpCommerce2Store_4C78B957(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_product_info(parent, out, 3);
}

int sceNpCommerce2Store_0DD765EC(const u32 *parent, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_drm_reward(parent, out, 3);
}

int sceNpCommerce2Store_47DC8C9F(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_recom_prods(ctx_id, req_id_out);
}

int sceNpCommerce2Store_DA223D9D(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_7B0C0787(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_recom_root(out, resp, resp_size);
}

int sceNpCommerce2Store_B7CD88D7(const u32 *parent, u32 index, u32 *out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_search_item_paged(parent, index, out);
}

int sceNpCommerce2Store_264EAA5B(u32 ctx_id, u32 *req_id_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return STORE_ERR_INVALID_ARG;
    return store_disp_create_prod_ids(ctx_id, req_id_out);
}

int sceNpCommerce2Store_31B3119F(u32 req_id, const char *content_id, const char *file_id)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (((u32)content_id | (u32)file_id) == 0U)
        return STORE_ERR_INVALID_ARG;
    return store_start_get_prod_ids(req_id, content_id, file_id);
}

int sceNpCommerce2Store_861ACADC(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

int sceNpCommerce2Store_00E9235B(u32 *out, const u8 *resp, u32 resp_size)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!resp) | (resp_size == 0U))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_init_prod_ids_root(out, resp, resp_size);
}

int sceNpCommerce2Store_FBE5903B(const u32 *parent, u32 index, u32 *out_ptr)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out_ptr))
        return STORE_ERR_INVALID_ARG;
    return store_tlv_get_prod_id_ptr(parent, index, out_ptr);
}

int sceNpCommerce2Store_6CD2ED8C(const char *content_id)
{
    const char *cur = content_id;
    u32 dash_cnt = 0U;

    if (!cur)
        return STORE_ERR_INVALID_ARG;
    while (1) {
        const char *hit = sceNet_lib_BCBE14CF(cur, '-');
        dash_cnt++;
        if (!hit)
            return STORE_ERR_INVALID_ARG;
        cur = hit + 1;
        if (dash_cnt >= 2U)
            return (int)(cur - content_id) - 1;
    }
}

int sceNpCommerce2Store_6E216646(u32 ctx_id, u32 option_val)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (option_val == 0U)
        return STORE_ERR_INVALID_ARG;
    return store_set_ctx_option(ctx_id, option_val);
}

int sceNpCommerce2Store_DA570F5D(u32 ctx_id, u32 svc_type)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (svc_type - 1U >= 8U)
        return STORE_ERR_INVALID_SERVICE;
    return store_set_svc_type(ctx_id, svc_type);
}

int sceNpCommerce2Store_B953E742(u32 ctx_id, const char *country, StoreCountryCaps *out_caps)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!out_caps)
        return STORE_ERR_INVALID_ARG;
    return store_get_country_caps(ctx_id, country, out_caps);
}

int sceNpCommerce2Store_39D8C7D9(u32 ctx_id, u32 *status_out)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_get_http_status(ctx_id, status_out);
}

int sceNpCommerce2Store_3F18219A(u32 ctx_id, const char *unused_str, const u32 *account_info,
                                 char *out_buf, u32 out_size)
{
    const char *root_pfx;

    if (ctx_id == 0U || !unused_str || !account_info || !out_buf)
        return STORE_ERR_INVALID_ARG;
    switch (account_info[21]) {
    case 1U: root_pfx = "IP9101-NPIA90005"; break;
    case 2U: root_pfx = "IP9102-NPIA90006"; break;
    case 3U: root_pfx = "IP9103-NPIA90007"; break;
    case 4U: root_pfx = "IP9104-NPIA90008"; break;
    case 5U: root_pfx = "IP9105-NPIA90009"; break;
    default: return STORE_ERR_INVALID_ARG;
    }
    if (sceNet_lib_D8722983(root_pfx) + sceNet_lib_D8722983("_01--STORE") < out_size) {
        sceNet_lib_CF705E46(out_buf, "%s%s", root_pfx, "_01--STORE");
        return 0;
    }
    return STORE_ERR_BUF_TOO_SMALL;
}

/* =========================================================================
 * Internal SDK-checked helpers (0x004B58 .. 0x005248)
 * ========================================================================= */

static inline __attribute__((always_inline)) int
store_checked_recv_impl(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out, int strict_size)
{
    if ((u32)sceKernelGetCompiledSdkVersion() > STORE_SDK_VER_550) {
        if (sceKernelCheckThreadStack() < STORE_MIN_STACK_LARGE)
            return STORE_ERR_STACK_OVERFLOW;
    }
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if ((u32)sceKernelGetCompiledSdkVersion() > STORE_SDK_VER_550) {
        if (strict_size == 1 && buf_size <= 0x3FFFFU)
            return STORE_ERR_RESP_BUF_LIMIT;
    }
    return store_recv_response(req_id, resp, buf_size, recv_size_out);
}

static int store_checked_recv_a(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out,
                                int strict_size)
{
    return store_checked_recv_impl(req_id, resp, buf_size, recv_size_out, strict_size);
}

static int store_checked_get_cat(u32 req_id, const char *cat_id, const char * const *reward_id,
                                 u32 has_reward, u32 start_pos, u32 max_cnt, int strict_check)
{
    char prefix[32];

    if ((u32)sceKernelGetCompiledSdkVersion() > STORE_SDK_VER_550) {
        if (sceKernelCheckThreadStack() < STORE_MIN_STACK_LARGE)
            return STORE_ERR_STACK_OVERFLOW;
    }
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!cat_id || (strict_check == 1 &&
                    (u32)sceKernelGetCompiledSdkVersion() > STORE_SDK_VER_550 &&
                    max_cnt >= 61U)) {
        return STORE_ERR_INVALID_ARG;
    }
    if (strict_check == 1 && sceNp_2E4769C3() != 0) {
        store_extract_id_prefix(prefix, cat_id);
        KprintfForUser("%s: %s\n", "GetCategoryContents", prefix);
    }
    return store_start_get_cat(req_id, cat_id, reward_id, has_reward, start_pos, max_cnt);
}

static int store_checked_recv_b(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out,
                                int strict_size)
{
    return store_checked_recv_impl(req_id, resp, buf_size, recv_size_out, strict_size);
}

static int store_checked_get_prod(u32 req_id, const char *cat_id, const char *prod_id,
                                  const char * const *reward_id, u32 has_reward, int strict_check)
{
    char prefix[32];

    if ((u32)sceKernelGetCompiledSdkVersion() > STORE_SDK_VER_550) {
        if (sceKernelCheckThreadStack() < STORE_MIN_STACK_LARGE)
            return STORE_ERR_STACK_OVERFLOW;
    }
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!prod_id)
        return STORE_ERR_INVALID_ARG;
    if (strict_check == 1 && sceNp_2E4769C3() != 0) {
        store_extract_id_prefix(prefix, prod_id);
        KprintfForUser("%s: %s\n", "GetProductInfo", prefix);
    }
    return store_start_get_prod(req_id, cat_id, prod_id, reward_id, has_reward);
}

static int store_checked_recv_c(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out,
                                int strict_size)
{
    return store_checked_recv_impl(req_id, resp, buf_size, recv_size_out, strict_size);
}

static int store_checked_get_prod_list(u32 req_id, const char * const *prod_ids, u32 prod_cnt,
                                       int strict_check)
{
    char prefix[32];

    if (sceKernelCheckThreadStack() < STORE_MIN_STACK_STD)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!prod_ids || prod_cnt == 0U || (strict_check == 1 && prod_cnt >= 61U))
        return STORE_ERR_INVALID_ARG;
    if (strict_check == 1 && sceNp_2E4769C3() != 0) {
        store_extract_id_prefix(prefix, prod_ids[0]);
        if (prod_cnt < 2U) {
            KprintfForUser("%s: %s\n", "GetProductInfoList", prefix);
        } else {
            u32 idx;
            KprintfForUser("%s: %s", "GetProductInfoList", prefix);
            for (idx = 1U; idx < prod_cnt; idx++) {
                store_extract_id_prefix(prefix, prod_ids[idx]);
                KprintfForUser(", %s", prefix);
            }
            KprintfForUser("\n");
        }
    }
    return store_start_prod_list(req_id, prod_ids, prod_cnt);
}
