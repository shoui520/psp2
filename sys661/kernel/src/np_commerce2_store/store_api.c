#include "store_internal.h"

u32 g_store_init = 0;
u32 g_store_mgr = 0;

static inline int store_check_ready(void)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return 0;
}

/* =========================================================================
 * Module lifecycle & standalone ID / root-category helpers
 * ========================================================================= */

int sceNpCommerce2Store_59D8A74D(void)
{
    int ret;

    if (g_store_init)
        return STORE_ERR_ALREADY_INITIALIZED;
    ret = store_init_mgr();
    if (ret < 0)
        return ret;
    g_store_init = 1;
    return 0;
}

int sceNpCommerce2Store_8455D3ED(void)
{
    if (g_store_init)
        g_store_init = 0;
    return 0;
}

int sceNpCommerce2Store_E3D1EA90(char *id_str)
{
    char *cur = id_str;
    char *dash;
    u32 count = 0;

    if (!id_str)
        return STORE_ERR_INVALID_ID;
    while (count < 2) {
        dash = sceNet_lib_BCBE14CF(cur, '-');
        if (!dash)
            return STORE_ERR_INVALID_ID;
        cur = dash + 1;
        count++;
    }
    dash = sceNet_lib_BCBE14CF(cur, '-');
    if (dash)
        *dash = '\0';
    return 0;
}

int sceNpCommerce2Store_B20C910B(const char *id_str, char *out_buf, u32 out_size)
{
    const char *cur = id_str;
    const char *dash;
    u32 count = 0;
    u32 len;

    if (!id_str)
        return STORE_ERR_INVALID_ID;
    while (count < 2) {
        dash = sceNet_lib_BCBE14CF(cur, '-');
        if (!dash)
            return STORE_ERR_INVALID_ID;
        cur = dash + 1;
        count++;
    }
    dash = sceNet_lib_BCBE14CF(cur, '-');
    if (!dash)
        dash = id_str + sceNet_lib_D8722983(id_str);
    len = (u32)(dash - id_str);
    if (len >= out_size)
        return STORE_ERR_BUF_TOO_SMALL;
    sceKernelMemcpy(out_buf, id_str, len);
    out_buf[len] = '\0';
    return 0;
}

int sceNpCommerce2Store_6CD2ED8C(const char *id_str)
{
    const char *cur = id_str;
    const char *dash;
    u32 count = 0;

    if (!id_str)
        return STORE_ERR_INVALID_ARG;
    while (count < 2) {
        dash = sceNet_lib_BCBE14CF(cur, '-');
        if (!dash)
            return STORE_ERR_INVALID_ARG;
        cur = dash + 1;
        count++;
    }
    return (int)(cur - id_str - 1);
}

int sceNpCommerce2Store_DA7B5568(const char *id_str, u32 *is_store_out)
{
    u32 pfx1_len, pfx2_len;
    const char *p;

    if (!id_str || !is_store_out)
        return STORE_ERR_INVALID_ARG;

    pfx1_len = sceNet_lib_D8722983("IP910");
    pfx2_len = sceNet_lib_D8722983("-NPIA9000");
    *is_store_out = 0;

    if (sceNet_lib_D8722983(id_str) < pfx1_len + pfx2_len + 4)
        return 0;
    if (sceNet_lib_94DCA9F0("IP910", id_str, pfx1_len) != 0)
        return 0;
    p = id_str + pfx1_len;
    if ((u8)(*p - '1') >= 5U)
        return 0;
    p++;
    if (sceNet_lib_94DCA9F0("-NPIA9000", p, pfx2_len) != 0)
        return 0;
    p += pfx2_len;
    *is_store_out = ((u8)(*p - '5') < 5U && p[1] == '_');
    return 0;
}

int sceNpCommerce2Store_7B6C5AEB(const char *id_str, u32 *is_cc_out)
{
    u32 pfx1_len, pfx2_len;
    const char *p;

    if (!id_str || !is_cc_out)
        return STORE_ERR_INVALID_ARG;

    pfx1_len = sceNet_lib_D8722983("IP9100-NPIA0900");
    pfx2_len = sceNet_lib_D8722983("_00-CC");
    *is_cc_out = 0;

    if (sceNet_lib_D8722983(id_str) < pfx1_len + pfx2_len + 1)
        return 0;
    if (sceNet_lib_94DCA9F0("IP9100-NPIA0900", id_str, pfx1_len) != 0)
        return 0;
    p = id_str + pfx1_len;
    if ((u8)(*p - '2') >= 5U)
        return 0;
    if (sceNet_lib_94DCA9F0("_00-CC", p + 1, pfx2_len) != 0)
        return 0;
    *is_cc_out = 1;
    return 0;
}

int sceNpCommerce2Store_3F18219A(const void *ctx, const void *info, const u32 *acct,
                                 char *out_buf, u32 out_size)
{
    static const char * const root_prefixes[5] = {
        "IP9101-NPIA90005",
        "IP9102-NPIA90006",
        "IP9103-NPIA90007",
        "IP9104-NPIA90008",
        "IP9105-NPIA90009",
    };
    u32 region;
    const char *pfx;

    if (!ctx || !info || !acct || !out_buf)
        return STORE_ERR_INVALID_ARG;

    region = acct[21];
    if (region < 1 || region > 5)
        return STORE_ERR_INVALID_ARG;

    pfx = root_prefixes[region - 1];
    if (sceNet_lib_D8722983(pfx) + sceNet_lib_D8722983("_01--STORE") >= out_size)
        return STORE_ERR_BUF_TOO_SMALL;

    sceNet_lib_CF705E46(out_buf, "%s%s", pfx, "_01--STORE");
    return 0;
}

/* =========================================================================
 * Request start wrappers with argument validation
 * ========================================================================= */

int sceNpCommerce2Store_8E76A121(u32 req_id, u32 svc_type, u32 start_pos, u32 max_cnt,
                                 const StoreSearchQuery *queries, u32 query_cnt, u32 sort_method)
{
    u32 idx;

    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (svc_type < 1 || svc_type > 8 || query_cnt != 1 || sort_method >= 8)
        return STORE_ERR_INVALID_ARG;
    for (idx = 0; idx < query_cnt; idx++) {
        if (queries[idx].key_index >= 7 || !queries[idx].value || queries[idx].value_len == 0)
            return STORE_ERR_INVALID_ARG;
    }
    return store_start_search_prod(req_id, start_pos, max_cnt, queries, query_cnt);
}

int sceNpCommerce2Store_81044B28(u32 req_id, const char * const *prod_ids, u32 prod_cnt,
                                 u32 start_pos, u32 max_cnt, u32 recom_type)
{
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (recom_type == 1) {
        if (!prod_ids || prod_cnt == 0 || max_cnt >= 61)
            return STORE_ERR_INVALID_ARG;
    } else if (recom_type == 2) {
        if (max_cnt >= 61)
            return STORE_ERR_INVALID_ARG;
    } else {
        return STORE_ERR_INVALID_ARG;
    }
    return store_start_recom_prods(req_id, prod_ids, prod_cnt, start_pos, max_cnt, recom_type);
}

/* =========================================================================
 * Guarded wrapper macros for Store API exports
 * ========================================================================= */

#define DEFINE_STORE_CREATE(fn, cmd) \
    int fn(u32 ctx_id, u32 *req_id_out) { \
        int err = store_check_ready(); \
        if (err < 0) return err; \
        if (!req_id_out) return STORE_ERR_INVALID_ARG; \
        return store_create_req(ctx_id, (cmd), req_id_out); \
    }

#define DEFINE_STORE_START_EMPTY(fn) \
    int fn(u32 req_id) { \
        int err = store_check_ready(); \
        if (err < 0) return err; \
        return store_start_empty(req_id); \
    }

#define DEFINE_STORE_RECV(fn) \
    int fn(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out) { \
        int err = store_check_ready(); \
        if (err < 0) return err; \
        return store_recv_response(req_id, buf, buf_size, recv_size_out); \
    }

#define DEFINE_STORE_TLV_ROOT(fn, parser) \
    int fn(u32 *out, const u8 *buf, u32 buf_size) { \
        int err = store_check_ready(); \
        if (err < 0) return err; \
        if (!out || !buf) return STORE_ERR_INVALID_ARG; \
        return parser(out, buf, buf_size); \
    }

#define DEFINE_STORE_TLV_COUNTED(fn, tag) \
    int fn(u32 *out, const u8 *buf, u32 buf_size) { \
        int err = store_check_ready(); \
        if (err < 0) return err; \
        if (!out || !buf) return STORE_ERR_INVALID_ARG; \
        return store_tlv_parse_counted_root(out, buf, buf_size, (tag)); \
    }

#define DEFINE_STORE_TLV_PAGED(fn, tag, min_len) \
    int fn(u32 *out, const u8 *buf, u32 buf_size) { \
        int err = store_check_ready(); \
        if (err < 0) return err; \
        if (!out || !buf) return STORE_ERR_INVALID_ARG; \
        return store_tlv_parse_paged_root(out, buf, buf_size, (tag), (min_len)); \
    }

#define DEFINE_STORE_TLV_SUB(fn, parser) \
    int fn(const u32 *parent, u32 *out) { \
        int err = store_check_ready(); \
        if (err < 0) return err; \
        if (!parent || !out) return STORE_ERR_INVALID_ARG; \
        return parser(parent, out); \
    }

#define DEFINE_STORE_TLV_ITEM(fn, parser) \
    int fn(const u32 *parent, u32 index, u32 *out) { \
        int err = store_check_ready(); \
        if (err < 0) return err; \
        if (!parent || !out) return STORE_ERR_INVALID_ARG; \
        return parser(parent, index, out); \
    }

#define DEFINE_STORE_TLV_TAG_PTR(fn, tag, from_body) \
    int fn(const u32 *parent, u32 index, u32 *out_ptr) { \
        int err = store_check_ready(); \
        if (err < 0) return err; \
        if (!parent || !out_ptr) return STORE_ERR_INVALID_ARG; \
        return store_tlv_parse_tag_ptr_item(parent, index, (tag), (from_body), out_ptr); \
    }

#define DEFINE_STORE_TLV_REWARD(fn, is_alt) \
    int fn(const u32 *parent, u32 *out) { \
        int err = store_check_ready(); \
        if (err < 0) return err; \
        if (!parent || !out) return STORE_ERR_INVALID_ARG; \
        return store_tlv_parse_reward_info(parent[0], (const u8 *)(size_t)parent[1], \
                                           (const u8 *)(size_t)parent[3], parent[4], \
                                           (const u8 *)(size_t)parent[5], out, (is_alt)); \
    }

/* Error / Session / Account / RegRules */
DEFINE_STORE_TLV_COUNTED(sceNpCommerce2Store_48FE9646, 120)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_CD27E505, store_tlv_parse_err_item)
int sceNpCommerce2Store_873887CF(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    return store_checked_recv(req_id, buf, buf_size, recv_size_out, 0);
}
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_2A06624D, store_tlv_parse_sess_root)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_45E24DCA, store_tlv_parse_currency)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_D601DE6A, store_tlv_parse_eula_flag)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_7D7FB404, store_tlv_parse_account_info)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_ABF975F8, store_tlv_parse_reg_rules)

/* GetCategoryContents */
int sceNpCommerce2Store_28EF259E(u32 req_id, const char *cat_id, const char * const *reward_id,
                                 u32 has_reward, u32 start_pos, u32 max_cnt)
{
    return store_checked_get_cat(req_id, cat_id, reward_id, has_reward, start_pos, max_cnt, 0);
}
int sceNpCommerce2Store_98F66F2C(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    return store_checked_recv(req_id, buf, buf_size, recv_size_out, 0);
}
int sceNpCommerce2Store_607146F0(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_category_info(parent, out, 0);
}
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_B2DDD9D5, store_tlv_parse_rating_nth)
int sceNpCommerce2Store_C228C7BF(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cat_sku(parent, out, 0);
}
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_B93B30CD, store_tlv_parse_subscr_ent)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_869624DB, store_tlv_parse_subscr_sku)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_4E84A105, store_tlv_parse_download_file)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_E7BBC497, store_tlv_parse_subscr_sku)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_B477732B, store_tlv_parse_subscr_sku)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_F7D8224B, store_tlv_parse_subscr_sku)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_219B9CD4, store_tlv_parse_subscr_sku)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_A7C4A4E0, store_tlv_parse_entitlement)
DEFINE_STORE_TLV_REWARD(sceNpCommerce2Store_F5E850B6, 0)

/* GetProductInfo */
int sceNpCommerce2Store_EEAC383D(u32 req_id, const char *cat_id, const char *prod_id,
                                 const char * const *reward_id, u32 has_reward)
{
    return store_checked_get_prod(req_id, cat_id, prod_id, reward_id, has_reward, 0);
}
int sceNpCommerce2Store_619AFBAE(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    return store_checked_recv(req_id, buf, buf_size, recv_size_out, 0);
}
int sceNpCommerce2Store_9B001542(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_product_info(parent, out, 0);
}
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_5CAF925C, store_tlv_parse_rating_nth)
int sceNpCommerce2Store_99A17682(const u32 *parent, u32 index, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_product_sku(parent, index, out, 0);
}
int sceNpCommerce2Store_4B3A29D4(const u32 *parent, u32 index, u32 *code_out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !code_out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_audio_lang(parent, index, code_out, NULL);
}
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_39D8AB1E, 62, 1)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_92393647, store_tlv_parse_entitlement)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_9DBCFF3D, store_tlv_parse_entitlement)
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_E5ECA4DF, 35, 1)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_783CC832, store_tlv_parse_credit_info)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_236FC217, store_tlv_parse_link_info)
DEFINE_STORE_TLV_REWARD(sceNpCommerce2Store_7A08C301, 0)

/* Cart operations (Add / Remove / Clear / Get) */
DEFINE_STORE_CREATE(sceNpCommerce2Store_ECF70261, STORE_CMD_ADD_CART)
int sceNpCommerce2Store_EF8F317D(u32 req_id, const char * const *sku_ids, u32 sku_cnt,
                                 const char * const *reward_ids, u32 reward_cnt)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_add_cart(req_id, sku_ids, sku_cnt, reward_ids, reward_cnt);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_FB5F3C12)
int sceNpCommerce2Store_CCA8601C(u32 *out, const u8 *buf, u32 buf_size)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!out || !buf) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_root(out, buf, buf_size, 0);
}
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_5115E0CB, store_tlv_parse_wallet_info)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_5CE497F6, store_tlv_parse_cart_summary)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_61FD9968, store_tlv_parse_cart_entry)
int sceNpCommerce2Store_EFFCA1ED(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_item(parent, 0, 0, out);
}
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_B8E4EED6, store_tlv_parse_rating_first)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_74B79FF2, store_tlv_parse_cart_sku_brief)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_782ED6EB, store_tlv_parse_entitlement)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_D2E48887, store_tlv_parse_download_file)
int sceNpCommerce2Store_CADDC96B(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_sku_full(parent, out, 2);
}
int sceNpCommerce2Store_E1776B29(const u32 *parent, u32 index, u32 *code_out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !code_out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_audio_lang(parent, index, code_out, NULL);
}
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_264E9CAA, 62, 1)
DEFINE_STORE_TLV_REWARD(sceNpCommerce2Store_F4261841, 0)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_DD8F307B, store_tlv_parse_entitlement)

DEFINE_STORE_CREATE(sceNpCommerce2Store_B9133D27, STORE_CMD_REM_CART)
int sceNpCommerce2Store_CA9FB11B(u32 req_id, const char *sku_id)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_sku_id(req_id, sku_id);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_4534FD80)
int sceNpCommerce2Store_21134841(u32 *out, const u8 *buf, u32 buf_size)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!out || !buf) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_root(out, buf, buf_size, 0);
}
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_BE626428, store_tlv_parse_wallet_info)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_C8133F70, store_tlv_parse_cart_summary)

DEFINE_STORE_CREATE(sceNpCommerce2Store_AE6DAF88, STORE_CMD_CLEAR_CART)
DEFINE_STORE_START_EMPTY(sceNpCommerce2Store_AD64726E)
DEFINE_STORE_RECV(sceNpCommerce2Store_59E39F46)

DEFINE_STORE_CREATE(sceNpCommerce2Store_6301672F, STORE_CMD_GET_CART)
DEFINE_STORE_START_EMPTY(sceNpCommerce2Store_701385D5)
DEFINE_STORE_RECV(sceNpCommerce2Store_850F5E58)
int sceNpCommerce2Store_99300AC2(u32 *out, const u8 *buf, u32 buf_size)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!out || !buf) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_root(out, buf, buf_size, 0);
}
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_211DFA3C, store_tlv_parse_wallet_info)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_48F4C175, store_tlv_parse_cart_summary)

/* PurchasePreview & Purchase */
DEFINE_STORE_CREATE(sceNpCommerce2Store_41C60662, STORE_CMD_PUR_PREV)
int sceNpCommerce2Store_ECE95BB2(u32 req_id, const char *password, int quick_purchase, int auto_deposit)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_purchase(req_id, password, quick_purchase, auto_deposit);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_1F4FFF9A)
int sceNpCommerce2Store_4E280B0A(u32 *out, const u8 *buf, u32 buf_size)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!out || !buf) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_root(out, buf, buf_size, 1);
}
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_E1DC1A08, store_tlv_parse_wallet_info)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_A8B9FFDB, store_tlv_parse_cart_summary)
int sceNpCommerce2Store_694667CF(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_tax_info(parent, (const u8 *)(size_t)parent[3], parent[4], out);
}

DEFINE_STORE_CREATE(sceNpCommerce2Store_CAB4A8F6, STORE_CMD_PURCH)
int sceNpCommerce2Store_F5381482(u32 req_id, const char *password, int quick_purchase, int auto_deposit)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_purchase(req_id, password, quick_purchase, auto_deposit);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_B1957179)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_0A7B6B92, store_tlv_parse_purchase_root)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_849E6B93, store_tlv_parse_wallet_info)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_FB5D86D3, store_tlv_parse_cart_summary)
int sceNpCommerce2Store_19FAC163(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_tax_info(parent, (const u8 *)(size_t)parent[3], parent[4], out);
}

/* GetCC / GetProv / GetPay */
DEFINE_STORE_CREATE(sceNpCommerce2Store_A89F0478, STORE_CMD_GET_CC)
DEFINE_STORE_START_EMPTY(sceNpCommerce2Store_8C0C2F7C)
DEFINE_STORE_RECV(sceNpCommerce2Store_EBBA8ECE)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_7136C275, store_tlv_parse_simple_root)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_259AE74B, store_tlv_parse_card_info)

DEFINE_STORE_CREATE(sceNpCommerce2Store_320C18DB, STORE_CMD_GET_PROV)
DEFINE_STORE_START_EMPTY(sceNpCommerce2Store_39757926)
DEFINE_STORE_RECV(sceNpCommerce2Store_6ACDFAC8)
DEFINE_STORE_TLV_COUNTED(sceNpCommerce2Store_FFEBE5FF, 55)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_998EB0CC, store_tlv_parse_province_info)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_6C5AE9E5, store_tlv_parse_city_info)

DEFINE_STORE_CREATE(sceNpCommerce2Store_73AE0FA0, STORE_CMD_GET_PAY)
int sceNpCommerce2Store_995ECF93(u32 req_id, u32 pay_type)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_payment_type(req_id, pay_type);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_CBBFF272)
DEFINE_STORE_TLV_COUNTED(sceNpCommerce2Store_4D5DA905, 57)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_91749B96, store_tlv_parse_pay_method)

/* GetDrm (Download List) */
DEFINE_STORE_CREATE(sceNpCommerce2Store_B961BF22, STORE_CMD_GET_DRM)
int sceNpCommerce2Store_1A7F75D7(u32 req_id, const char * const *sku_ids, u32 sku_cnt,
                                 const char *svc_id, const char *reward_ids,
                                 u32 start_pos, u32 max_cnt, u32 drm_type)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_get_drm(req_id, sku_ids, sku_cnt, svc_id, reward_ids,
                               start_pos, max_cnt, drm_type);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_A52FFFF8)
DEFINE_STORE_TLV_PAGED(sceNpCommerce2Store_18457EB9, 43, 125)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_57DD55E5, store_tlv_parse_drm_item)
int sceNpCommerce2Store_063A16A4(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_drm_reward(parent, out, 0);
}
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_CE84A5DC, store_tlv_parse_entitlement)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_6961822C, store_tlv_parse_drm_subitem)

/* GetBill / RegCC / GetAddr / GetWal */
DEFINE_STORE_CREATE(sceNpCommerce2Store_B1BEAFED, STORE_CMD_GET_BILL)
DEFINE_STORE_START_EMPTY(sceNpCommerce2Store_7A1291A6)
DEFINE_STORE_RECV(sceNpCommerce2Store_E55BC0DF)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_EAF03127, store_tlv_parse_address_root)

DEFINE_STORE_CREATE(sceNpCommerce2Store_D58BC86C, STORE_CMD_REG_CC)
int sceNpCommerce2Store_12378B6A(u32 req_id, const StoreBillingInfo *info)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_billing(req_id, info);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_53716B04)

DEFINE_STORE_CREATE(sceNpCommerce2Store_8EABFF3E, STORE_CMD_GET_ADDR)
int sceNpCommerce2Store_472E4D84(u32 req_id, const char *postal_code)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_postal_code(req_id, postal_code);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_B0E5DFD8)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_6EB54FA3, store_tlv_parse_addr_lookup_root)

DEFINE_STORE_CREATE(sceNpCommerce2Store_DA757050, STORE_CMD_GET_WAL)
DEFINE_STORE_START_EMPTY(sceNpCommerce2Store_B8889A67)
DEFINE_STORE_RECV(sceNpCommerce2Store_0E2B656E)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_C7B9E02B, store_tlv_parse_simple_root)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_8B81197E, store_tlv_parse_wallet_info)

/* TopupCC / TopEdy / TopEdyRes */
DEFINE_STORE_CREATE(sceNpCommerce2Store_7B3A8218, STORE_CMD_TOPUP_CC)
int sceNpCommerce2Store_D2517B02(u32 req_id, u32 amount)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_topup_amount(req_id, amount);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_ECE10E5B)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_49E768E9, store_tlv_parse_topup_root)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_9AA823E4, store_tlv_parse_card_info)

DEFINE_STORE_CREATE(sceNpCommerce2Store_0A5949EF, STORE_CMD_TOP_EDY)
int sceNpCommerce2Store_C57CF651(u32 req_id, u32 amount)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_topup_amount(req_id, amount);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_6FE1F29E)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_346E1F56, store_tlv_parse_simple_root)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_3572E4D7, store_tlv_parse_edy_info)

DEFINE_STORE_CREATE(sceNpCommerce2Store_75C205F7, STORE_CMD_TOP_EDY_RES)
int sceNpCommerce2Store_68B07CDC(u32 req_id, const char *receipt_id, int is_ok)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_edy_result(req_id, receipt_id, is_ok);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_334F89A3)

/* VoucherRedeemPreview & VoucherRedeem */
DEFINE_STORE_CREATE(sceNpCommerce2Store_2E290D53, STORE_CMD_VCHR_RED_PREV)
int sceNpCommerce2Store_47A8A5B4(u32 req_id, const char *code1, const char *code2,
                                 const char *code3, int auto_deposit)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_voucher(req_id, code1, code2, code3, auto_deposit);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_FEDE1E87)
int sceNpCommerce2Store_2180FE6C(u32 *out, const u8 *buf, u32 buf_size)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!out || !buf) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_voucher_root(out, buf, buf_size, 0);
}
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_5EC8E7B8, store_tlv_parse_voucher_summary)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_00292401, store_tlv_parse_voucher_items)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_FC6408EC, store_tlv_parse_voucher_item)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_25579A90, store_tlv_parse_rating_first)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_66BC1072, store_tlv_parse_download_list)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_D22ADDA7, store_tlv_parse_download_file)
int sceNpCommerce2Store_65D72F22(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_video_info(parent, out, 0);
}
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_2581BC37, store_tlv_parse_entitlement)
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_59443417, 35, 1)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_1C4CE9CB, store_tlv_parse_credit_info)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_7063CC9D, store_tlv_parse_link_info)
DEFINE_STORE_TLV_REWARD(sceNpCommerce2Store_548C6487, 0)
int sceNpCommerce2Store_F036856B(const u32 *parent, u32 index, u32 *code_out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !code_out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_audio_lang(parent, index, code_out, NULL);
}
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_7F642484, 62, 1)
int sceNpCommerce2Store_9D3888D8(const u32 *parent, u32 index, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_video_sku(parent, index, out, 0);
}

DEFINE_STORE_CREATE(sceNpCommerce2Store_92E53CD4, STORE_CMD_VCHR_RED)
int sceNpCommerce2Store_D232FB91(u32 req_id, const char *code1, const char *code2,
                                 const char *code3, int auto_deposit)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_voucher(req_id, code1, code2, code3, auto_deposit);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_6AECBA79)
int sceNpCommerce2Store_D63D12BF(u32 *out, const u8 *buf, u32 buf_size)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!out || !buf) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_voucher_root(out, buf, buf_size, 1);
}
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_ED2C778E, store_tlv_parse_voucher_summary)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_CBC27838, store_tlv_parse_voucher_items)

/* GetResAddr / EventLog / SearchProducts */
DEFINE_STORE_CREATE(sceNpCommerce2Store_7C45D60E, STORE_CMD_GET_RES_ADDR)
DEFINE_STORE_START_EMPTY(sceNpCommerce2Store_09B3E27D)
DEFINE_STORE_RECV(sceNpCommerce2Store_8644CCFB)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_770C5947, store_tlv_parse_address_root)

DEFINE_STORE_CREATE(sceNpCommerce2Store_78BA836A, STORE_CMD_EVENT_LOG)
int sceNpCommerce2Store_7D654C92(u32 req_id, u32 event_type, const char *id_str, const char *url_str)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_event_log(req_id, event_type, id_str, url_str);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_C0BB62AD)

DEFINE_STORE_CREATE(sceNpCommerce2Store_DDD2C398, STORE_CMD_SRCH_PROD)
DEFINE_STORE_RECV(sceNpCommerce2Store_D70F1117)
DEFINE_STORE_TLV_PAGED(sceNpCommerce2Store_54D94925, 40, 4)
int sceNpCommerce2Store_94E8DAB3(const u32 *parent, u32 index, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_search_item(parent, index, 1, out);
}
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_465A6BBA, 92, 1)
int sceNpCommerce2Store_E6009051(const u32 *parent, u32 index, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_rating_item(parent, 1, index, out);
}
int sceNpCommerce2Store_B1F9111F(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_rating_item(parent, 0, 0, out);
}

/* GetBillingInfo / ConsoleActivation / EPW / GuestSession */
DEFINE_STORE_CREATE(sceNpCommerce2Store_6DDED3F7, STORE_CMD_GET_BILL_INFO)
int sceNpCommerce2Store_E1A8D7FF(u32 req_id, u32 unused1, u32 unused2, u32 unused3)
{
    int err = store_check_ready();
    (void)unused1; (void)unused2; (void)unused3;
    if (err < 0) return err;
    return store_start_payment_type(req_id, 0);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_5857AF83)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_4E3E0594, store_tlv_parse_billing_root)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_FBF61E40, store_tlv_parse_province_info)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_BC7F8DA6, store_tlv_parse_pay_method)
DEFINE_STORE_TLV_SUB(sceNpCommerce2Store_A93E02A6, store_tlv_parse_card_info)

DEFINE_STORE_CREATE(sceNpCommerce2Store_166EA2D5, STORE_CMD_GET_CNSL_ACT)
DEFINE_STORE_START_EMPTY(sceNpCommerce2Store_F22C4985)
DEFINE_STORE_RECV(sceNpCommerce2Store_47540138)
DEFINE_STORE_TLV_COUNTED(sceNpCommerce2Store_0E083AEA, 116)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_84C4226F, store_tlv_parse_console_act)

DEFINE_STORE_CREATE(sceNpCommerce2Store_A165F819, STORE_CMD_GET_EPW)
DEFINE_STORE_START_EMPTY(sceNpCommerce2Store_42769097)
DEFINE_STORE_RECV(sceNpCommerce2Store_C42B0607)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_BCD5575A, store_tlv_parse_epw_root)

DEFINE_STORE_CREATE(sceNpCommerce2Store_C7AFBA23, STORE_CMD_CR_GUEST_SESS)
int sceNpCommerce2Store_D31C1673(u32 req_id, const char *country, const char *lang,
                                 u32 yob, u32 mob, u32 dob)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_guest_sess(req_id, country, lang, yob, mob, dob);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_5C15E8DA)

/* GetProductInfoList & extended-format TLV parsers */
DEFINE_STORE_CREATE(sceNpCommerce2Store_C2A7C748, STORE_CMD_GET_PROD_LIST)
int sceNpCommerce2Store_E3A5AC0C(u32 req_id, const char * const *prod_ids, u32 prod_cnt)
{
    return store_checked_get_prod_list(req_id, prod_ids, prod_cnt, 1);
}
int sceNpCommerce2Store_85F7B364(u32 req_id, const char * const *prod_ids, u32 prod_cnt)
{
    return store_checked_get_prod_list(req_id, prod_ids, prod_cnt, 0);
}
int sceNpCommerce2Store_C9652539(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    if (_sceKernelCheckThreadStack() < STORE_MIN_STACK_STD)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (buf_size < 0x40000U)
        return STORE_ERR_RESP_BUF_LIMIT;
    return store_recv_response(req_id, buf, buf_size, recv_size_out);
}
int sceNpCommerce2Store_B9108E1C(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    if (_sceKernelCheckThreadStack() < STORE_MIN_STACK_STD)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    return store_recv_response(req_id, buf, buf_size, recv_size_out);
}
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_20A1FDAD, store_tlv_parse_prod_list_root)
int sceNpCommerce2Store_7BBE5E6F(const u32 *parent, u32 index, u32 *out)
{
    u32 tmp[12];
    int ret;

    if (_sceKernelCheckThreadStack() < STORE_MIN_STACK_STD)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!parent || !out)
        return STORE_ERR_INVALID_ARG;
    ret = store_tlv_parse_search_item(parent, index, 0, tmp);
    if (ret < 0)
        return ret;
    return store_tlv_parse_category_item(tmp, out);
}

DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_0F42E599, store_tlv_parse_entitlement)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_F132A92F, store_tlv_parse_entitlement)
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_8C9B3738, 62, 1)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_C6FDB930, store_tlv_parse_entitlement)
DEFINE_STORE_TLV_REWARD(sceNpCommerce2Store_96E5B56F, 1)
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_EF296653, 62, 1)
int sceNpCommerce2Store_7EB1A34C(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cart_sku_full(parent, out, 3);
}
int sceNpCommerce2Store_43CF3A7A(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_video_info(parent, out, 3);
}
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_BAFBB9D2, 35, 1)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_31FFC518, store_tlv_parse_credit_info)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_E9E20646, store_tlv_parse_link_info)
DEFINE_STORE_TLV_REWARD(sceNpCommerce2Store_CC69487B, 1)
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_26781C1F, 62, 1)
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_E2367A5C, 17, 1)
int sceNpCommerce2Store_8E216CEB(const u32 *parent, u32 index, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_video_sku(parent, index, out, 3);
}
int sceNpCommerce2Store_4F460A0D(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_cat_sku(parent, out, 3);
}
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_4F945E73, store_tlv_parse_rating_nth)
DEFINE_STORE_TLV_REWARD(sceNpCommerce2Store_3370450A, 1)
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_7411D8E7, 17, 1)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_9E63F7D5, store_tlv_parse_rating_nth)
int sceNpCommerce2Store_D830992C(const u32 *parent, u32 index, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_product_sku(parent, index, out, 3);
}
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_94E55872, 35, 1)
DEFINE_STORE_TLV_REWARD(sceNpCommerce2Store_E4E600A5, 1)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_93A4DBE4, store_tlv_parse_credit_info)
DEFINE_STORE_TLV_ITEM(sceNpCommerce2Store_E1B3A08D, store_tlv_parse_link_info)
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_A47A19C1, 17, 1)
int sceNpCommerce2Store_1FBC9F13(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_category_info(parent, out, 3);
}
int sceNpCommerce2Store_4C78B957(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_product_info(parent, out, 3);
}
int sceNpCommerce2Store_0DD765EC(const u32 *parent, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_drm_reward(parent, out, 3);
}

/* RecommendedProducts / GetProductIds / Context Settings */
DEFINE_STORE_CREATE(sceNpCommerce2Store_47DC8C9F, STORE_CMD_RECOM_PRODS)
DEFINE_STORE_RECV(sceNpCommerce2Store_DA223D9D)
DEFINE_STORE_TLV_ROOT(sceNpCommerce2Store_7B0C0787, store_tlv_parse_recom_root)
int sceNpCommerce2Store_B7CD88D7(const u32 *parent, u32 index, u32 *out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!parent || !out) return STORE_ERR_INVALID_ARG;
    return store_tlv_parse_search_item(parent, index, 1, out);
}

DEFINE_STORE_CREATE(sceNpCommerce2Store_264EAA5B, STORE_CMD_GET_PROD_IDS)
int sceNpCommerce2Store_31B3119F(u32 req_id, const char *content_id, const char *file_id)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_start_get_prod_ids(req_id, content_id, file_id);
}
DEFINE_STORE_RECV(sceNpCommerce2Store_861ACADC)
DEFINE_STORE_TLV_COUNTED(sceNpCommerce2Store_00E9235B, 56)
DEFINE_STORE_TLV_TAG_PTR(sceNpCommerce2Store_FBE5903B, 56, 0)

int sceNpCommerce2Store_6E216646(u32 ctx_id, u32 val)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_set_ctx_option(ctx_id, val);
}

int sceNpCommerce2Store_DA570F5D(u32 ctx_id, u32 svc_type)
{
    int err = store_check_ready();
    if (err < 0) return err;
    return store_set_svc_type(ctx_id, svc_type);
}

int sceNpCommerce2Store_B953E742(u32 ctx_id, const char *country, u32 *out_info)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!out_info) return STORE_ERR_INVALID_ARG;
    return store_get_country_caps(ctx_id, country, out_info);
}

int sceNpCommerce2Store_39D8C7D9(u32 ctx_id, u32 *status_out)
{
    int err = store_check_ready();
    if (err < 0) return err;
    if (!status_out) return STORE_ERR_INVALID_ARG;
    return store_get_http_status(ctx_id, status_out);
}
