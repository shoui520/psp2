#include "store_internal.h"

const char * const g_endpoint_names[86] = {
    "crSess",           "delSess",          "getCat",           "getProd",
    "srchProd",         "getWal",           "getDrm",           "purPrev",
    "purch",            "getCC",            "topupCC",          "vchrRedPrev",
    "vchrRed",          "topEdy",           "topEdyRes",        "addCart",
    "remCart",          "clearCart",        "getCart",          "getProv",
    "getPay",           "getBill",          "regCC",            "getAddr",
    "getResAddr",       "eventLog",         "getCountries",     "bindAccount",
    "startReg",         "getSecQ",          "setCred",          "setLid",
    "updPass",          "verifyOid",        "setOid",           "getName",
    "setName",          "setResAddr",       "setRrn",           "getOptIn",
    "setOptIn",         "authMaster",       "getBillInfo",      "setSubCtrl",
    "resetSubPass",     "getWalOpt",        "setWalOpt",        "submitReg",
    "authenticate",     "remPay",           "getTranHist",      "getTranDetail",
    "getSrvs",          "getSrvDetail",     "getSubscr",        "getSubscrPlan",
    "changeSubscr",     "cancelSubscr",     "getCnslAct",       "acptEula",
    "verifyDob",        "verifySecAns",     "sendPassResetMail","verifyMasterBill",
    "getEpw",           "crGuestSess",      "resetPass",        "getMasterBill",
    "sendConsentMail",  "getMasterResAddr", "getProdList",      "getSkuList",
    "getReward",        "setSecQ",          "submitUpgrade",    "recomProds",
    "verifyBill",       "regDevice",        "deregDevice",      "getProdIds",
    "crSessTicket",     "getUsersFromCID",  "getActCode",       "setAccountLang",
    "getPivotForProd",  "getProdForPivot",
};

const char * const g_service_prefixes[9] = {
    "", "store/", "store/", "store/", "store/",
    "reg/", "cam/", "upgrade/", "pwd/",
};

const StoreFormKey g_form_keys[81] = {
    {  1, "&" },
    {  1, "," },
    {  8, "version=" },
    {  9, "username=" },
    {  9, "password=" },
    { 10, "consoleId=" },
    { 11, "categoryId=" },
    { 14, "startPosition=" },
    { 18, "maxCountOfResults=" },
    { 10, "productId=" },
    { 10, "storeType=" },
    {  9, "queryKey=" },
    { 11, "queryValue=" },
    { 18, "contentSortMethod=" },
    {  6, "skuId=" },
    {  7, "skuIds=" },
    {  8, "titleId=" },
    { 15, "drmContentType=" },
    { 14, "quickPurchase=" },
    { 12, "topupAmount=" },
    {  2, "1=" },
    {  2, "2=" },
    {  2, "3=" },
    { 13, "edyReceiptId=" },
    { 21, "resultOfTopupFromEdy=" },
    { 12, "paymentType=" },
    { 16, "paymentMethodId=" },
    { 11, "holderName=" },
    { 11, "cardNumber=" },
    { 10, "startYear=" },
    { 11, "startMonth=" },
    { 11, "expireYear=" },
    { 12, "expireMonth=" },
    { 13, "securityCode=" },
    { 12, "issueNumber=" },
    {  5, "city=" },
    {  9, "province=" },
    { 11, "postalCode=" },
    { 10, "eventType=" },
    {  3, "id=" },
    {  4, "url=" },
    {  8, "country=" },
    {  9, "language=" },
    {  4, "yob=" },
    {  4, "mob=" },
    {  4, "dob=" },
    { 10, "loginName=" },
    { 17, "securityQuestion=" },
    { 15, "securityAnswer=" },
    {  9, "psHandle=" },
    { 10, "firstName=" },
    {  9, "lastName=" },
    { 15, "firstNameASCII=" },
    { 14, "lastNameASCII=" },
    {  7, "gender=" },
    { 14, "scexOptinFlag=" },
    { 20, "affiliatesOptinFlag=" },
    { 13, "subLoginName=" },
    { 14, "contentRating=" },
    { 12, "chatControl=" },
    { 14, "searchControl=" },
    { 18, "spendingLimitFlag=" },
    { 14, "spendingLimit=" },
    { 16, "autoDepositFlag=" },
    { 21, "commercePasswordFlag=" },
    {  9, "fromYear=" },
    { 10, "fromMonth=" },
    {  8, "fromDay=" },
    {  7, "toYear=" },
    {  8, "toMonth=" },
    {  6, "toDay=" },
    {  9, "timezone=" },
    { 14, "transactionId=" },
    { 10, "serviceId=" },
    {  5, "save=" },
    {  9, "rewardId=" },
    { 10, "rewardIds=" },
    { 11, "productIds=" },
    { 19, "recommendationType=" },
    { 10, "contentId=" },
    {  7, "fileId=" },
};

const char * const g_video_query_keys[7] = {
    "nativeText", "genre", "actor", "writer",
    "nativeTitle", "nativeDirectorProducer", "nativeDescription",
};

const char * const g_game_query_keys[7] = {
    "gameAll", "XXX", "XXX", "XXX", "gameTitle", "XXX", "gameDescription",
};

const char * const g_comic_query_keys[7] = {
    "comicAll", "XXX", "XXX", "XXX", "comicTitle", "XXX", "XXX",
};

const char * const g_country_tier_full[2] = {
    "us", "",
};

const char * const g_country_tier_mid[33] = {
    "ca", "mx", "gb", "ie", "be", "lu", "nl", "fr", "de", "at", "ch",
    "it", "pt", "dk", "fi", "no", "se", "au", "nz", "es", "ru", "ae",
    "za", "pl", "gr", "sa", "cz", "hk", "tw", "sg", "kr", "jp", "",
};

const char * const g_country_tier_basic[24] = {
    "cl", "pe", "ar", "co", "br", "my", "bg", "hr", "ro", "si", "hu",
    "sk", "tr", "bh", "kw", "lb", "om", "qa", "il", "mt", "is", "cy",
    "in", "",
};

/* Forward declarations for internal builders and low-level writers */
static int store_emit_str_field(int write_pass, int http_req, u32 key_id,
                                const char *val, int need_amp) __attribute__((noinline));
static int store_emit_uint_field(int write_pass, int http_req, u32 key_id,
                                 u32 val, int need_amp) __attribute__((noinline));
static int store_build_get_cat(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                               const char *cat_id, const char * const *reward_id, u32 has_reward,
                               u32 start_pos, u32 max_cnt) __attribute__((noinline));
static int store_build_get_prod(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                const char *cat_id, const char *prod_id,
                                const char * const *reward_id, u32 has_reward) __attribute__((noinline));
static int store_build_add_cart(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                const char * const *sku_ids, u32 sku_cnt,
                                const char * const *reward_ids, u32 reward_cnt) __attribute__((noinline));
static int store_build_purchase(int write_pass, int http_req, u64 *len_out, u16 cmd_id,
                                const char *password, int quick_purchase,
                                int auto_deposit) __attribute__((noinline));
static int store_build_get_drm(int write_pass, int http_req, u64 *len_out,
                               const char * const *sku_ids, u32 sku_cnt, const char *svc_id,
                               const char *reward_ids, u32 start_pos, u32 max_cnt,
                               u32 drm_type) __attribute__((noinline));
static int store_build_billing(int write_pass, int http_req, u64 *len_out, int is_account,
                               const StoreBillingInfo *billing, int with_save,
                               int save_flag) __attribute__((noinline));
static int store_build_edy_result(int write_pass, int http_req, u64 *len_out,
                                  const char *receipt_id, int is_ok) __attribute__((noinline));
static int store_build_voucher(int write_pass, int http_req, u64 *len_out,
                               const char *code_a, const char *code_b, const char *code_c,
                               int auto_deposit) __attribute__((noinline));
static int store_build_event_log(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                 u32 event_type, const char *id_str,
                                 const char *url_str) __attribute__((noinline));
static int store_build_search_prod(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                   u32 start_pos, u32 max_cnt, const StoreSearchQuery *queries,
                                   u32 query_cnt, u32 sort_method) __attribute__((noinline));
static int store_build_guest_sess(int write_pass, int http_req, u64 *len_out, u32 ver,
                                  const char *country, const char *lang, u32 yob, u32 mob,
                                  u32 dob, const char *console_id) __attribute__((noinline));
static int store_build_prod_list(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                 const char * const *prod_ids,
                                 u32 prod_cnt) __attribute__((noinline));
static int store_build_recom_prods(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                   const char * const *prod_ids, u32 prod_cnt,
                                   u32 start_pos, u32 max_cnt,
                                   u32 recom_type) __attribute__((noinline));
static int store_build_prod_ids(int write_pass, int http_req, u64 *len_out,
                                const char *content_id,
                                const char *file_id) __attribute__((noinline));
static int store_write_prefixed(int http_req, const char * const *prefixes, u32 prefix_cnt,
                                u32 key_id, const char *val, u32 val_len,
                                int need_amp) __attribute__((noinline));
static int store_write_str_list(int http_req, u32 key_id, const char * const *list, u32 count,
                                char *enc_buf, u32 enc_size, int repeat_first,
                                int need_amp) __attribute__((noinline));
static int store_write_query_vals(int http_req, u32 key_id, const StoreSearchQuery *queries,
                                  u32 count, char *enc_buf, u32 enc_size,
                                  int need_amp) __attribute__((noinline));
static int store_write_query_keys(int http_req, u32 key_id, const StoreSearchQuery *queries,
                                  const char * const *key_table, u32 count, char *enc_buf,
                                  u32 enc_size, int need_amp) __attribute__((noinline));
static int store_write_uint(int http_req, u32 key_id, u32 val,
                            int need_amp) __attribute__((noinline));
static int store_measure_str_list(u32 key_id, const char * const *list, u32 count,
                                  char *enc_buf, u32 enc_size, int repeat_first,
                                  int need_amp) __attribute__((noinline));

/* Inline helpers matching Sony's inlined field emission & HTTP request creation */
static inline __attribute__((always_inline)) int
store_open_http(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                int *http_req_out, u64 content_len)
{
    const char *prefix = NULL;
    const char *endpoint = NULL;
    int ret = store_resolve_endpoint(svc_type, cmd_id, &prefix, &endpoint);
    if (ret >= 0) {
        ret = sceNpCommerce2_382803C8(http_tmpl, svc_type, cmd_id, prefix, endpoint, 0,
                                      url_buf, http_req_out,
                                      (u32)(content_len & 0xFFFFFFFFULL),
                                      (u32)(content_len >> 32));
    }
    return ret;
}

static inline __attribute__((always_inline)) int
store_inline_str(int write_pass, int http_req, u32 key_id, const char *val, int need_amp)
{
    if (write_pass == 1) {
        if (need_amp == 1) {
            int ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[SFKEY_AMP].str, g_form_keys[SFKEY_AMP].len);
            if (ret < 0)
                return ret;
        }
        {
            int ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[key_id].str,
                                              g_form_keys[key_id].len);
            if (ret < 0)
                return ret;
        }
        return sceNpCommerce2_AE614CC6(http_req, val, sceNet_lib_D8722983(val));
    }
    return (int)(g_form_keys[key_id].len + (u32)(need_amp == 1) + sceNet_lib_D8722983(val));
}

static inline __attribute__((always_inline)) int
store_inline_uint(int write_pass, int http_req, u32 key_id, u32 val, int need_amp)
{
    char num[16];
    int len;
    if (write_pass == 1)
        return store_write_uint(http_req, key_id, val, need_amp);
    len = sceNet_lib_CF705E46(num, "%u", val);
    if (len >= (int)sizeof(num))
        return STORE_ERR_BUF_OVERFLOW;
    return len + (int)g_form_keys[key_id].len + (need_amp == 1);
}

static inline __attribute__((always_inline)) int
store_inline_prefixed(int write_pass, int http_req, const char * const *prefixes, u32 prefix_cnt,
                      u32 key_id, const char *val, u32 val_len, int need_amp)
{
    u32 idx;
    int total;
    if (write_pass == 1)
        return store_write_prefixed(http_req, prefixes, prefix_cnt, key_id, val, val_len, need_amp);
    total = need_amp;
    for (idx = 0; idx < prefix_cnt; idx++)
        total += (int)sceNet_lib_D8722983(prefixes[idx]);
    return total + (int)val_len + (int)g_form_keys[key_id].len;
}

static inline __attribute__((always_inline)) int
store_send_empty_body(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out)
{
    int ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, 0ULL);
    if (ret < 0)
        return ret;
    return store_min_s32(sceNpCommerce2_AE614CC6(*http_req_out, NULL, 0), 0);
}

/* =========================================================================
 * Endpoint resolution (0x005534)
 * ========================================================================= */

int store_resolve_endpoint(u32 svc_type, u16 cmd_id, const char **prefix_out,
                           const char **endpoint_out)
{
    int env_ver;
    int ret;
    u32 idx = cmd_id;

    if (idx >= 87U)
        return STORE_ERR_INVALID_CMD;
    if (svc_type - 1U >= 8U)
        return STORE_ERR_INVALID_SERVICE;

    ret = sceNpCommerce2_460A1D92(&env_ver, 9);
    if (ret < 0)
        return ret;

    *prefix_out = g_service_prefixes[svc_type];
    if (svc_type == 7U) {
        if (idx == 31U)
            idx = 74U;
        else if (idx == 48U)
            idx = 75U;
    }
    *endpoint_out = g_endpoint_names[idx - 1U];
    return ret;
}

/* =========================================================================
 * Per-command request senders (0x005610 .. 0x00707C)
 * ========================================================================= */

int store_send_get_cat(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const char *cat_id, const char * const *reward_id, u32 has_reward,
                       u32 start_pos, u32 max_cnt)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_get_cat(pass != 0U, *http_req_out, &content_len, svc_type,
                                      cat_id, reward_id, has_reward, start_pos, max_cnt);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_get_prod(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char *cat_id, const char *prod_id,
                        const char * const *reward_id, u32 has_reward)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_get_prod(pass != 0U, *http_req_out, &content_len, svc_type,
                                       cat_id, prod_id, reward_id, has_reward);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_add_cart(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char * const *sku_ids, u32 sku_cnt,
                        const char * const *reward_ids, u32 reward_cnt)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_add_cart(pass != 0U, *http_req_out, &content_len, svc_type,
                                       sku_ids, sku_cnt, reward_ids, reward_cnt);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_rem_cart(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char *sku_id)
{
    char enc[784];
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int http_req = *http_req_out;
        int ret = sceNpCommerce2_66EC0829(enc, 769U, sku_id, sceNet_lib_D8722983(sku_id), 0);
        if (ret >= 0) {
            ret = store_emit_str_field(pass != 0U, http_req, SFKEY_SKU_ID, enc, 0);
            if (ret >= 0) {
                if (pass == 0U)
                    content_len = (u64)(s64)ret;
                ret = 0;
            }
        }
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_clear_cart(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out)
{
    return store_send_empty_body(svc_type, http_tmpl, url_buf, cmd_id, http_req_out);
}

int store_send_get_cart(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out)
{
    return store_send_empty_body(svc_type, http_tmpl, url_buf, cmd_id, http_req_out);
}

int store_send_purchase(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char *password, int quick_purchase, int auto_deposit)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_purchase(pass != 0U, *http_req_out, &content_len, cmd_id,
                                       password, quick_purchase, auto_deposit);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_get_cc(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out)
{
    return store_send_empty_body(svc_type, http_tmpl, url_buf, cmd_id, http_req_out);
}

int store_send_get_prov(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out)
{
    return store_send_empty_body(svc_type, http_tmpl, url_buf, cmd_id, http_req_out);
}

static const char *g_payment_types[1] = { "CC" };

int store_send_payment_type(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                            int *http_req_out, u32 pay_type)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret;
        if (pay_type != 0U)
            return STORE_ERR_INVALID_PAYMENT;
        ret = store_emit_str_field(pass != 0U, *http_req_out, SFKEY_PAYMENT_TYPE,
                                   g_payment_types[pay_type], 0);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            content_len = (u64)(s64)ret;
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_get_drm(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const char * const *sku_ids, u32 sku_cnt, const char *svc_id,
                       const char *reward_ids, u32 start_pos, u32 max_cnt, u32 drm_type)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_get_drm(pass != 0U, *http_req_out, &content_len,
                                      sku_ids, sku_cnt, svc_id, reward_ids,
                                      start_pos, max_cnt, drm_type);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_get_bill(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out)
{
    return store_send_empty_body(svc_type, http_tmpl, url_buf, cmd_id, http_req_out);
}

int store_send_billing(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const StoreBillingInfo *billing)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_billing(pass != 0U, *http_req_out, &content_len, 0, billing, 0, 0);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_postal_code(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                           int *http_req_out, const char *postal_code)
{
    char enc[784];
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int http_req = *http_req_out;
        int ret = sceNpCommerce2_66EC0829(enc, 769U, postal_code,
                                          sceNet_lib_D8722983(postal_code), 0);
        if (ret >= 0) {
            ret = store_emit_str_field(pass != 0U, http_req, SFKEY_POSTAL_CODE, enc, 0);
            if (ret >= 0) {
                if (pass == 0U)
                    content_len = (u64)(s64)ret;
                ret = 0;
            }
        }
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_get_wal(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out)
{
    return store_send_empty_body(svc_type, http_tmpl, url_buf, cmd_id, http_req_out);
}

int store_send_topup_amount(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                            int *http_req_out, u32 amount)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_emit_uint_field(pass != 0U, *http_req_out, SFKEY_TOPUP_AMOUNT, amount, 0);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            content_len = (u64)(s64)ret;
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_edy_result(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                          const char *receipt_id, int is_ok)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_edy_result(pass != 0U, *http_req_out, &content_len,
                                         receipt_id, is_ok);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_voucher(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const char *code_a, const char *code_b, const char *code_c,
                       int auto_deposit)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_voucher(pass != 0U, *http_req_out, &content_len,
                                      code_a, code_b, code_c, auto_deposit);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_event_log(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         u32 event_type, const char *id_str, const char *url_str)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_event_log(pass != 0U, *http_req_out, &content_len, svc_type,
                                        event_type, id_str, url_str);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_search_prod(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                           int *http_req_out, u32 start_pos, u32 max_cnt,
                           const StoreSearchQuery *queries, u32 query_cnt, u32 sort_method)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_search_prod(pass != 0U, *http_req_out, &content_len, svc_type,
                                          start_pos, max_cnt, queries, query_cnt, sort_method);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_get_epw(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out)
{
    return store_send_empty_body(svc_type, http_tmpl, url_buf, cmd_id, http_req_out);
}

int store_send_guest_sess(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                          u32 ver, const char *country, const char *lang,
                          u32 yob, u32 mob, u32 dob, const char *console_id)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_guest_sess(pass != 0U, *http_req_out, &content_len, ver,
                                         country, lang, yob, mob, dob, console_id);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_prod_list(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         const char * const *prod_ids, u32 prod_cnt)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_prod_list(pass != 0U, *http_req_out, &content_len, svc_type,
                                        prod_ids, prod_cnt);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_recom_prods(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                            int *http_req_out, const char * const *prod_ids, u32 prod_cnt,
                            u32 start_pos, u32 max_cnt, u32 recom_type)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_recom_prods(pass != 0U, *http_req_out, &content_len, svc_type,
                                          prod_ids, prod_cnt, start_pos, max_cnt, recom_type);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_get_prod_ids(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                            int *http_req_out, const char *content_id, const char *file_id)
{
    u64 content_len = 0ULL;
    u32 pass;

    for (pass = 0; pass < 2U; pass++) {
        int ret = store_build_prod_ids(pass != 0U, *http_req_out, &content_len,
                                       content_id, file_id);
        if (ret < 0)
            return ret;
        if (pass == 0U) {
            ret = store_open_http(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, content_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

/* =========================================================================
 * Out-of-line field emitters (0x0071A4, 0x007294)
 * ========================================================================= */

static int store_emit_str_field(int write_pass, int http_req, u32 key_id,
                                const char *val, int need_amp)
{
    return store_inline_str(write_pass, http_req, key_id, val, need_amp);
}

static int store_emit_uint_field(int write_pass, int http_req, u32 key_id,
                                 u32 val, int need_amp)
{
    return store_inline_uint(write_pass, http_req, key_id, val, need_amp);
}

/* =========================================================================
 * Two-pass form body builders (0x007328 .. 0x00A4D4)
 * ========================================================================= */

static int store_build_get_cat(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                               const char *cat_id, const char * const *reward_id, u32 has_reward,
                               u32 start_pos, u32 max_cnt)
{
    char enc[784];
    s64 total = 0;
    int ret;

    if (reward_id && has_reward)
        svc_type = 1U;

    sceNet_lib_CF705E46(enc, "%u", svc_type);
    ret = store_inline_str(write_pass, http_req, SFKEY_STORE_TYPE, enc, 0);
    if (ret < 0)
        return ret;
    total = (s64)ret;

    ret = sceNpCommerce2_66EC0829(enc, 769U, cat_id, sceNet_lib_D8722983(cat_id), 0);
    if (ret < 0)
        return ret;
    ret = store_inline_str(write_pass, http_req, SFKEY_CATEGORY_ID, enc, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (reward_id && has_reward) {
        ret = sceNpCommerce2_66EC0829(enc, 769U, *reward_id, sceNet_lib_D8722983(*reward_id), 0);
        if (ret < 0)
            return ret;
        ret = store_inline_str(write_pass, http_req, SFKEY_REWARD_ID, enc, 1);
        if (ret < 0)
            return ret;
        total += (s64)ret;
    }

    ret = store_inline_uint(write_pass, http_req, SFKEY_START_POSITION, start_pos, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    ret = store_inline_uint(write_pass, http_req, SFKEY_MAX_COUNT, max_cnt, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_get_prod(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                const char *cat_id, const char *prod_id,
                                const char * const *reward_id, u32 has_reward)
{
    char enc[784];
    s64 total = 0;
    int ret;

    if (reward_id && has_reward)
        svc_type = 1U;

    sceNet_lib_CF705E46(enc, "%u", svc_type);
    ret = store_inline_str(write_pass, http_req, SFKEY_STORE_TYPE, enc, 0);
    if (ret < 0)
        return ret;
    total = (s64)ret;

    ret = sceNpCommerce2_66EC0829(enc, 769U, prod_id, sceNet_lib_D8722983(prod_id), 0);
    if (ret < 0)
        return ret;
    ret = store_inline_str(write_pass, http_req, SFKEY_PRODUCT_ID, enc, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (cat_id && cat_id[0] != '\0') {
        ret = sceNpCommerce2_66EC0829(enc, 769U, cat_id, sceNet_lib_D8722983(cat_id), 0);
        if (ret < 0)
            return ret;
        ret = store_inline_str(write_pass, http_req, SFKEY_CATEGORY_ID, enc, 1);
        if (ret < 0)
            return ret;
        total += (s64)ret;
    }

    if (reward_id && has_reward) {
        ret = sceNpCommerce2_66EC0829(enc, 769U, *reward_id, sceNet_lib_D8722983(*reward_id), 0);
        if (ret < 0)
            return ret;
        ret = store_inline_str(write_pass, http_req, SFKEY_REWARD_ID, enc, 1);
        if (ret < 0)
            return ret;
        total += (s64)ret;
    }

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_add_cart(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                const char * const *sku_ids, u32 sku_cnt,
                                const char * const *reward_ids, u32 reward_cnt)
{
    char enc[784];
    s64 total = 0;
    int ret;

    sceNet_lib_CF705E46(enc, "%u", svc_type);
    ret = store_inline_str(write_pass, http_req, SFKEY_STORE_TYPE, enc, 0);
    if (ret < 0)
        return ret;
    total = (s64)ret;

    if (write_pass == 1)
        ret = store_write_str_list(http_req, SFKEY_SKU_ID, sku_ids, sku_cnt, enc, 769U, 0, 1);
    else
        ret = store_measure_str_list(SFKEY_SKU_ID, sku_ids, sku_cnt, enc, 769U, 0, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (reward_ids && reward_cnt != 0U && sku_cnt == reward_cnt) {
        u32 idx;
        for (idx = 0; idx < reward_cnt; idx++) {
            if (reward_ids[idx] && reward_ids[idx][0] != '\0')
                break;
        }
        if (idx != reward_cnt) {
            if (write_pass == 1) {
                ret = store_write_str_list(http_req, SFKEY_REWARD_IDS, reward_ids, reward_cnt,
                                           enc, 769U, 0, 1);
            } else {
                ret = store_measure_str_list(SFKEY_REWARD_IDS, reward_ids, reward_cnt,
                                             enc, 769U, 0, 1);
            }
            if (ret < 0)
                return ret;
            total += (s64)ret;
        }
    }

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_purchase(int write_pass, int http_req, u64 *len_out, u16 cmd_id,
                                const char *password, int quick_purchase, int auto_deposit)
{
    char enc[784];
    s64 total = 0;
    int has_field = 0;
    int ret;
    const char *bool_str;

    (void)cmd_id;

    if (password && password[0] != '\0') {
        ret = sceNpCommerce2_66EC0829(enc, 769U, password, sceNet_lib_D8722983(password), 0);
        if (ret < 0)
            return ret;
        ret = store_inline_str(write_pass, http_req, SFKEY_PASSWORD, enc, 0);
        if (ret < 0)
            return ret;
        has_field = 1;
        total = (s64)ret;
    }

    bool_str = (quick_purchase == 1) ? "true" : "false";
    ret = store_inline_str(write_pass, http_req, SFKEY_QUICK_PURCHASE, bool_str, has_field);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    bool_str = (auto_deposit == 1) ? "true" : "false";
    ret = store_inline_str(write_pass, http_req, SFKEY_AUTO_DEPOSIT_FLAG, bool_str, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_get_drm(int write_pass, int http_req, u64 *len_out,
                               const char * const *sku_ids, u32 sku_cnt, const char *svc_id,
                               const char *reward_ids, u32 start_pos, u32 max_cnt, u32 drm_type)
{
    char enc[784];
    s64 total = 0;
    int has_field = 0;
    int ret;

    if (sku_ids && sku_cnt != 0U) {
        if (write_pass == 1)
            ret = store_write_str_list(http_req, SFKEY_SKU_IDS, sku_ids, sku_cnt, enc, 769U, 0, 0);
        else
            ret = store_measure_str_list(SFKEY_SKU_IDS, sku_ids, sku_cnt, enc, 769U, 0, 0);
        if (ret < 0)
            return ret;
        has_field = 1;
        total = (s64)ret;
    }

    if (svc_id && svc_id[0] != '\0') {
        ret = sceNpCommerce2_66EC0829(enc, 769U, svc_id, sceNet_lib_D8722983(svc_id), 0);
        if (ret < 0)
            return ret;
        ret = store_inline_str(write_pass, http_req, SFKEY_SERVICE_ID, enc, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (reward_ids) {
        ret = sceNpCommerce2_66EC0829(enc, 769U, reward_ids, sceNet_lib_D8722983(reward_ids), 0);
        if (ret < 0)
            return ret;
        ret = store_inline_str(write_pass, http_req, SFKEY_REWARD_IDS, enc, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    ret = store_inline_uint(write_pass, http_req, SFKEY_START_POSITION, start_pos, has_field);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    ret = store_inline_uint(write_pass, http_req, SFKEY_MAX_COUNT, max_cnt, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    ret = store_inline_uint(write_pass, http_req, SFKEY_DRM_CONTENT_TYPE, drm_type, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_billing(int write_pass, int http_req, u64 *len_out, int is_account,
                               const StoreBillingInfo *billing, int with_save, int save_flag)
{
    char enc[784];
    const char *prefixes[8];
    char num_sy[16];
    char num_sm[16];
    char num_ey[16];
    char num_em[16];
    s64 total = 0;
    int has_field = 0;
    int ret;

    sceKernelMemset(prefixes, 0, sizeof(prefixes));
    if (is_account == 0)
        prefixes[0] = "creditCard.";
    else if (is_account == 1)
        prefixes[0] = "account.";
    prefixes[1] = "address.";
    prefixes[2] = "address";

    if (billing->payment_method_id && billing->payment_method_id[0] != '\0' &&
        billing->payment_method_id_len != 0U) {
        u32 plen = billing->payment_method_id_len;
        if (billing->payment_method_id[plen - 1U] == '\0')
            plen--;
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                    SFKEY_PAYMENT_METHOD_ID, billing->payment_method_id,
                                    plen, 0);
        if (ret < 0)
            return ret;
        total = (s64)ret;
        has_field = 1;
    }

    if (billing->holder_name && billing->holder_name[0] != '\0' &&
        billing->holder_name_len != 0U) {
        u32 elen;
        ret = sceNpCommerce2_66EC0829(enc, 769U, billing->holder_name,
                                      billing->holder_name_len, 0);
        if (ret < 0)
            return ret;
        elen = sceNet_lib_D8722983(enc);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                    SFKEY_HOLDER_NAME, enc, elen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->card_number && billing->card_number[0] != '\0') {
        u32 clen = sceNet_lib_D8722983(billing->card_number);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                    SFKEY_CARD_NUMBER, billing->card_number, clen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->start_year != 0U) {
        u32 nlen;
        if (sceNet_lib_CF705E46(num_sy, "%u", billing->start_year) >= (int)sizeof(num_sy))
            return STORE_ERR_BUF_OVERFLOW;
        nlen = sceNet_lib_D8722983(num_sy);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                    SFKEY_START_YEAR, num_sy, nlen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->start_month != 0U) {
        u32 nlen;
        if (sceNet_lib_CF705E46(num_sm, "%u", billing->start_month) >= (int)sizeof(num_sm))
            return STORE_ERR_BUF_OVERFLOW;
        nlen = sceNet_lib_D8722983(num_sm);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                    SFKEY_START_MONTH, num_sm, nlen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->expire_year != 0U) {
        u32 nlen;
        if (sceNet_lib_CF705E46(num_ey, "%u", billing->expire_year) >= (int)sizeof(num_ey))
            return STORE_ERR_BUF_OVERFLOW;
        nlen = sceNet_lib_D8722983(num_ey);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                    SFKEY_EXPIRE_YEAR, num_ey, nlen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->expire_month != 0U) {
        u32 nlen;
        if (sceNet_lib_CF705E46(num_em, "%u", billing->expire_month) >= (int)sizeof(num_em))
            return STORE_ERR_BUF_OVERFLOW;
        nlen = sceNet_lib_D8722983(num_em);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                    SFKEY_EXPIRE_MONTH, num_em, nlen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->security_code && billing->security_code[0] != '\0') {
        u32 slen = sceNet_lib_D8722983(billing->security_code);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                    SFKEY_SECURITY_CODE, billing->security_code, slen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->issue_number && billing->issue_number[0] != '\0') {
        u32 ilen = sceNet_lib_D8722983(billing->issue_number);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                    SFKEY_ISSUE_NUMBER, billing->issue_number, ilen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->street_a && billing->street_a[0] != '\0' && billing->street_a_len != 0U) {
        u32 elen;
        ret = sceNpCommerce2_66EC0829(enc, 769U, billing->street_a, billing->street_a_len, 0);
        if (ret < 0)
            return ret;
        elen = sceNet_lib_D8722983(enc);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 3U,
                                    SFKEY_SUFFIX_A, enc, elen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->street_b && billing->street_b[0] != '\0' && billing->street_b_len != 0U) {
        u32 elen;
        ret = sceNpCommerce2_66EC0829(enc, 769U, billing->street_b, billing->street_b_len, 0);
        if (ret < 0)
            return ret;
        elen = sceNet_lib_D8722983(enc);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 3U,
                                    SFKEY_SUFFIX_B, enc, elen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->street_c && billing->street_c[0] != '\0' && billing->street_c_len != 0U) {
        u32 elen;
        ret = sceNpCommerce2_66EC0829(enc, 769U, billing->street_c, billing->street_c_len, 0);
        if (ret < 0)
            return ret;
        elen = sceNet_lib_D8722983(enc);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 3U,
                                    SFKEY_SUFFIX_C, enc, elen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->city && billing->city[0] != '\0' && billing->city_len != 0U) {
        u32 elen;
        ret = sceNpCommerce2_66EC0829(enc, 769U, billing->city, billing->city_len, 0);
        if (ret < 0)
            return ret;
        elen = sceNet_lib_D8722983(enc);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 2U,
                                    SFKEY_CITY, enc, elen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->province && billing->province[0] != '\0' && billing->province_len != 0U) {
        u32 elen;
        ret = sceNpCommerce2_66EC0829(enc, 769U, billing->province, billing->province_len, 0);
        if (ret < 0)
            return ret;
        elen = sceNet_lib_D8722983(enc);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 2U,
                                    SFKEY_PROVINCE, enc, elen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (billing->postal_code && billing->postal_code[0] != '\0' &&
        billing->postal_code_len != 0U) {
        u32 elen;
        ret = sceNpCommerce2_66EC0829(enc, 769U, billing->postal_code,
                                      billing->postal_code_len, 0);
        if (ret < 0)
            return ret;
        elen = sceNet_lib_D8722983(enc);
        ret = store_inline_prefixed(write_pass, http_req, prefixes, 2U,
                                    SFKEY_POSTAL_CODE, enc, elen, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
        has_field = 1;
    }

    if (with_save == 1) {
        const char *save_str = (save_flag == 1) ? "true" : "false";
        ret = store_inline_str(write_pass, http_req, SFKEY_SAVE, save_str, has_field);
        if (ret < 0)
            return ret;
        total += (s64)ret;
    }

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_edy_result(int write_pass, int http_req, u64 *len_out,
                                  const char *receipt_id, int is_ok)
{
    char enc[784];
    s64 total = 0;
    const char *res_str;
    int ret;

    ret = sceNpCommerce2_66EC0829(enc, 769U, receipt_id, sceNet_lib_D8722983(receipt_id), 0);
    if (ret < 0)
        return ret;
    ret = store_inline_str(write_pass, http_req, SFKEY_EDY_RECEIPT_ID, enc, 0);
    if (ret < 0)
        return ret;
    total = (s64)ret;

    res_str = (is_ok == 1) ? "OK" : "NG";
    ret = store_inline_str(write_pass, http_req, SFKEY_RESULT_EDY_TOPUP, res_str, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_voucher(int write_pass, int http_req, u64 *len_out,
                               const char *code_a, const char *code_b, const char *code_c,
                               int auto_deposit)
{
    char enc[784];
    const char *prefixes[1];
    s64 total = 0;
    u32 elen;
    const char *dep_str;
    int ret;

    sceKernelMemset(prefixes, 0, sizeof(prefixes));
    prefixes[0] = "voucherCode";

    ret = sceNpCommerce2_66EC0829(enc, 769U, code_a, sceNet_lib_D8722983(code_a), 0);
    if (ret < 0)
        return ret;
    elen = sceNet_lib_D8722983(enc);
    ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                SFKEY_SUFFIX_A, enc, elen, 0);
    if (ret < 0)
        return ret;
    total = (s64)ret;

    ret = sceNpCommerce2_66EC0829(enc, 769U, code_b, sceNet_lib_D8722983(code_b), 0);
    if (ret < 0)
        return ret;
    elen = sceNet_lib_D8722983(enc);
    ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                SFKEY_SUFFIX_B, enc, elen, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    ret = sceNpCommerce2_66EC0829(enc, 769U, code_c, sceNet_lib_D8722983(code_c), 0);
    if (ret < 0)
        return ret;
    elen = sceNet_lib_D8722983(enc);
    ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U,
                                SFKEY_SUFFIX_C, enc, elen, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    dep_str = (auto_deposit == 1) ? "true" : "false";
    ret = store_inline_str(write_pass, http_req, SFKEY_AUTO_DEPOSIT_FLAG, dep_str, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_event_log(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                 u32 event_type, const char *id_str, const char *url_str)
{
    char num[16];
    s64 total = 0;
    int ret;

    sceNet_lib_CF705E46(num, "%u", svc_type);
    ret = store_inline_str(write_pass, http_req, SFKEY_STORE_TYPE, num, 0);
    if (ret < 0)
        return ret;
    total = (s64)ret;

    sceNet_lib_CF705E46(num, "%u", event_type);
    ret = store_inline_str(write_pass, http_req, SFKEY_EVENT_TYPE, num, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    ret = store_inline_str(write_pass, http_req, SFKEY_ID, id_str, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (url_str && url_str[0] != '\0') {
        ret = store_inline_str(write_pass, http_req, SFKEY_URL, url_str, 1);
        if (ret < 0)
            return ret;
        total += (s64)ret;
    }

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_search_prod(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                   u32 start_pos, u32 max_cnt, const StoreSearchQuery *queries,
                                   u32 query_cnt, u32 sort_method)
{
    char enc[784];
    const char * const *key_table;
    s64 total = 0;
    int ret;

    (void)sort_method;

    sceNet_lib_CF705E46(enc, "%u", svc_type);
    ret = store_inline_str(write_pass, http_req, SFKEY_STORE_TYPE, enc, 0);
    if (ret < 0)
        return ret;
    total = (s64)ret;

    ret = store_inline_uint(write_pass, http_req, SFKEY_START_POSITION, start_pos, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    ret = store_inline_uint(write_pass, http_req, SFKEY_MAX_COUNT, max_cnt, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (svc_type == 1U || svc_type == 3U)
        key_table = g_game_query_keys;
    else if (svc_type == 4U)
        key_table = g_comic_query_keys;
    else
        key_table = g_video_query_keys;

    if (write_pass == 1) {
        ret = store_write_query_keys(http_req, SFKEY_QUERY_KEY, queries, key_table,
                                     query_cnt, enc, 769U, 1);
    } else {
        u32 idx;
        int sum = 10;
        for (idx = 0; idx < query_cnt; idx++) {
            const char *kname = key_table[queries[idx].key_index];
            sum += (idx != 0U);
            ret = sceNpCommerce2_66EC0829(enc, 769U, kname, sceNet_lib_D8722983(kname), 0);
            if (ret < 0)
                return ret;
            sum += (int)sceNet_lib_D8722983(enc);
        }
        ret = sum;
    }
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (write_pass == 1) {
        ret = store_write_query_vals(http_req, SFKEY_QUERY_VALUE, queries,
                                     query_cnt, enc, 769U, 1);
    } else {
        u32 idx;
        int sum = 12;
        for (idx = 0; idx < query_cnt; idx++) {
            sum += (idx != 0U);
            ret = sceNpCommerce2_66EC0829(enc, 769U, queries[idx].value,
                                          queries[idx].value_len, 0);
            if (ret < 0)
                return ret;
            sum += (int)sceNet_lib_D8722983(enc);
        }
        ret = sum;
    }
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_guest_sess(int write_pass, int http_req, u64 *len_out, u32 ver,
                                  const char *country, const char *lang, u32 yob, u32 mob,
                                  u32 dob, const char *console_id)
{
    const char *prefixes[2];
    char work[16];
    s64 total = 0;
    u32 idx;
    u32 wlen;
    int ret;

    sceKernelMemset(prefixes, 0, sizeof(prefixes));
    prefixes[0] = "account.";

    ret = store_inline_uint(write_pass, http_req, SFKEY_VERSION, ver, 0);
    if (ret < 0)
        return ret;
    total = (s64)ret;

    for (idx = 0; idx < sceNet_lib_D8722983(country) && idx < sizeof(work); idx++)
        work[idx] = (char)sceNet_lib_C13C9307((int)country[idx]);
    work[idx == sizeof(work) ? sizeof(work) - 1U : idx] = '\0';
    wlen = sceNet_lib_D8722983(work);
    ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U, SFKEY_COUNTRY, work, wlen, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    for (idx = 0; idx < sceNet_lib_D8722983(lang) && idx < sizeof(work); idx++)
        work[idx] = (char)sceNet_lib_96EF9DA1((int)lang[idx]);
    work[idx == sizeof(work) ? sizeof(work) - 1U : idx] = '\0';
    wlen = sceNet_lib_D8722983(work);
    ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U, SFKEY_LANGUAGE, work, wlen, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    sceNet_lib_CF705E46(work, "%u", yob);
    wlen = sceNet_lib_D8722983(work);
    ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U, SFKEY_YOB, work, wlen, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    sceNet_lib_CF705E46(work, "%u", mob);
    wlen = sceNet_lib_D8722983(work);
    ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U, SFKEY_MOB, work, wlen, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    sceNet_lib_CF705E46(work, "%u", dob);
    wlen = sceNet_lib_D8722983(work);
    ret = store_inline_prefixed(write_pass, http_req, prefixes, 1U, SFKEY_DOB, work, wlen, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    ret = store_inline_str(write_pass, http_req, SFKEY_CONSOLE_ID, console_id, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_prod_list(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                 const char * const *prod_ids, u32 prod_cnt)
{
    char enc[784];
    s64 total;
    int ret;

    sceNet_lib_CF705E46(enc, "%u", svc_type);
    ret = store_emit_str_field(write_pass, http_req, SFKEY_STORE_TYPE, enc, 0);
    if (ret < 0)
        return ret;
    total = (s64)ret;

    if (write_pass == 1)
        ret = store_write_str_list(http_req, SFKEY_PRODUCT_IDS, prod_ids, prod_cnt, enc, 769U, 0, 1);
    else
        ret = store_measure_str_list(SFKEY_PRODUCT_IDS, prod_ids, prod_cnt, enc, 769U, 0, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_recom_prods(int write_pass, int http_req, u64 *len_out, u32 svc_type,
                                   const char * const *prod_ids, u32 prod_cnt,
                                   u32 start_pos, u32 max_cnt, u32 recom_type)
{
    char enc[784];
    s64 total;
    int ret;

    sceNet_lib_CF705E46(enc, "%u", svc_type);
    ret = store_inline_str(write_pass, http_req, SFKEY_STORE_TYPE, enc, 0);
    if (ret < 0)
        return ret;
    total = (s64)ret;

    if (prod_ids && prod_cnt != 0U) {
        if (write_pass == 1) {
            ret = store_write_str_list(http_req, SFKEY_PRODUCT_IDS, prod_ids, prod_cnt,
                                       enc, 769U, 0, 1);
        } else {
            ret = store_measure_str_list(SFKEY_PRODUCT_IDS, prod_ids, prod_cnt,
                                         enc, 769U, 0, 1);
        }
        if (ret < 0)
            return ret;
        total += (s64)ret;
    }

    ret = store_inline_uint(write_pass, http_req, SFKEY_START_POSITION, start_pos, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    ret = store_inline_uint(write_pass, http_req, SFKEY_MAX_COUNT, max_cnt, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    ret = store_inline_uint(write_pass, http_req, SFKEY_RECOM_TYPE, recom_type, 1);
    if (ret < 0)
        return ret;
    total += (s64)ret;

    if (write_pass != 1)
        *len_out = (u64)total;
    return 0;
}

static int store_build_prod_ids(int write_pass, int http_req, u64 *len_out,
                                const char *content_id, const char *file_id)
{
    char enc[784];
    int ret;

    if (content_id) {
        ret = sceNpCommerce2_66EC0829(enc, 769U, content_id, sceNet_lib_D8722983(content_id), 0);
        if (ret < 0)
            return ret;
        ret = store_inline_str(write_pass, http_req, SFKEY_CONTENT_ID, enc, 0);
    } else {
        ret = sceNpCommerce2_66EC0829(enc, 769U, file_id, sceNet_lib_D8722983(file_id), 0);
        if (ret < 0)
            return ret;
        ret = store_inline_str(write_pass, http_req, SFKEY_FILE_ID, enc, 0);
    }
    if (ret < 0)
        return ret;
    if (write_pass != 1)
        *len_out = (u64)(s64)ret;
    return 0;
}

/* =========================================================================
 * Low-level HTTP form writers & measurer (0x00A638 .. 0x00AC4C)
 * ========================================================================= */

static int store_write_prefixed(int http_req, const char * const *prefixes, u32 prefix_cnt,
                                u32 key_id, const char *val, u32 val_len, int need_amp)
{
    u32 idx;
    int ret;

    if (need_amp == 1) {
        ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[SFKEY_AMP].str, g_form_keys[SFKEY_AMP].len);
        if (ret < 0)
            return ret;
    }
    for (idx = 0; idx < prefix_cnt; idx++) {
        if (prefixes[idx]) {
            ret = sceNpCommerce2_AE614CC6(http_req, prefixes[idx],
                                          sceNet_lib_D8722983(prefixes[idx]));
            if (ret < 0)
                return ret;
        }
    }
    ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_AE614CC6(http_req, val, val_len);
}

static int store_write_str_list(int http_req, u32 key_id, const char * const *list, u32 count,
                                char *enc_buf, u32 enc_size, int repeat_first, int need_amp)
{
    u32 idx;
    int ret;

    if (need_amp == 1) {
        ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[SFKEY_AMP].str, g_form_keys[SFKEY_AMP].len);
        if (ret < 0)
            return ret;
    }
    ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;

    for (idx = 0; idx < count; idx++) {
        const char *str = (repeat_first == 1) ? list[0] : list[idx];
        if (idx != 0U) {
            ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[SFKEY_COMMA].str, g_form_keys[SFKEY_COMMA].len);
            if (ret < 0)
                return ret;
        }
        if (str && str[0] != '\0') {
            ret = sceNpCommerce2_66EC0829(enc_buf, enc_size, str, sceNet_lib_D8722983(str), 0);
            if (ret < 0)
                return ret;
            ret = sceNpCommerce2_AE614CC6(http_req, enc_buf, sceNet_lib_D8722983(enc_buf));
            if (ret < 0)
                return ret;
        }
    }
    return ret;
}

static int store_write_query_vals(int http_req, u32 key_id, const StoreSearchQuery *queries,
                                  u32 count, char *enc_buf, u32 enc_size, int need_amp)
{
    u32 idx;
    int ret;

    if (need_amp == 1) {
        ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[SFKEY_AMP].str, g_form_keys[SFKEY_AMP].len);
        if (ret < 0)
            return ret;
    }
    ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;

    for (idx = 0; idx < count; idx++) {
        if (idx != 0U) {
            ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[SFKEY_COMMA].str, g_form_keys[SFKEY_COMMA].len);
            if (ret < 0)
                return ret;
        }
        ret = sceNpCommerce2_66EC0829(enc_buf, enc_size, queries[idx].value,
                                      queries[idx].value_len, 0);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_AE614CC6(http_req, enc_buf, sceNet_lib_D8722983(enc_buf));
        if (ret < 0)
            return ret;
    }
    return ret;
}

static int store_write_query_keys(int http_req, u32 key_id, const StoreSearchQuery *queries,
                                  const char * const *key_table, u32 count, char *enc_buf,
                                  u32 enc_size, int need_amp)
{
    u32 idx;
    int ret;

    if (need_amp == 1) {
        ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[SFKEY_AMP].str, g_form_keys[SFKEY_AMP].len);
        if (ret < 0)
            return ret;
    }
    ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;

    for (idx = 0; idx < count; idx++) {
        const char *kname = key_table[queries[idx].key_index];
        if (idx != 0U) {
            ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[SFKEY_COMMA].str, g_form_keys[SFKEY_COMMA].len);
            if (ret < 0)
                return ret;
        }
        ret = sceNpCommerce2_66EC0829(enc_buf, enc_size, kname, sceNet_lib_D8722983(kname), 0);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_AE614CC6(http_req, enc_buf, sceNet_lib_D8722983(enc_buf));
        if (ret < 0)
            return ret;
    }
    return ret;
}

static int store_write_uint(int http_req, u32 key_id, u32 val, int need_amp)
{
    char num[16];
    int len;
    int ret;

    if (need_amp == 1) {
        ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[SFKEY_AMP].str, g_form_keys[SFKEY_AMP].len);
        if (ret < 0)
            return ret;
    }
    ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;
    len = sceNet_lib_CF705E46(num, "%u", val);
    if (len >= (int)sizeof(num))
        return STORE_ERR_BUF_OVERFLOW;
    return sceNpCommerce2_AE614CC6(http_req, num, sceNet_lib_D8722983(num));
}

static int store_measure_str_list(u32 key_id, const char * const *list, u32 count,
                                  char *enc_buf, u32 enc_size, int repeat_first, int need_amp)
{
    u32 idx;
    int total = (int)g_form_keys[key_id].len + (need_amp == 1);

    for (idx = 0; idx < count; idx++) {
        const char *str = (repeat_first == 1) ? list[0] : list[idx];
        if (idx != 0U)
            total += 1;
        if (str && str[0] != '\0') {
            int ret = sceNpCommerce2_66EC0829(enc_buf, enc_size, str,
                                              sceNet_lib_D8722983(str), 0);
            if (ret < 0)
                return ret;
            total += (int)sceNet_lib_D8722983(enc_buf);
        }
    }
    return total;
}
