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

const char * const g_country_table[59] = {
    "us", "",
    "ca", "mx", "gb", "ie", "be", "lu", "nl", "fr", "de", "at", "ch",
    "it", "pt", "dk", "fi", "no", "se", "au", "nz", "es", "ru", "ae",
    "za", "pl", "gr", "sa", "cz", "hk", "tw", "sg", "kr", "jp", "",
    "cl", "pe", "ar", "co", "br", "my", "bg", "hr", "ro", "si", "hu",
    "sk", "tr", "bh", "kw", "lb", "om", "qa", "il", "mt", "is", "cy",
    "in", "",
};

/* =========================================================================
 * Two-pass form payload writer (pass 0 measures length, pass 1 streams HTTP)
 * ========================================================================= */

typedef struct {
    int write_pass;
    int http_req;
    u64 total_len;
    int has_field;
} StoreFormWriter;

static int fw_raw(StoreFormWriter *fw, const void *data, u32 len)
{
    if (fw->write_pass)
        return sceNpCommerce2_AE614CC6(fw->http_req, data, len);
    fw->total_len += len;
    return 0;
}

static int fw_sep(StoreFormWriter *fw)
{
    if (fw->has_field) {
        int ret = fw_raw(fw, "&", 1);
        if (ret < 0)
            return ret;
    }
    fw->has_field = 1;
    return 0;
}

static int fw_prefixed_val(StoreFormWriter *fw, const char * const *prefixes, u32 prefix_cnt,
                           u32 key_id, const char *val, u32 val_len)
{
    u32 idx;
    int ret = fw_sep(fw);
    if (ret < 0)
        return ret;
    for (idx = 0; idx < prefix_cnt; idx++) {
        if (prefixes[idx]) {
            ret = fw_raw(fw, prefixes[idx], sceNet_lib_D8722983(prefixes[idx]));
            if (ret < 0)
                return ret;
        }
    }
    ret = fw_raw(fw, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;
    return fw_raw(fw, val, val_len);
}

static int fw_str(StoreFormWriter *fw, u32 key_id, const char *val)
{
    return fw_prefixed_val(fw, NULL, 0, key_id, val, sceNet_lib_D8722983(val));
}

static int fw_encoded_str(StoreFormWriter *fw, u32 key_id, const char *val)
{
    char enc[784];
    int ret = sceNpCommerce2_66EC0829(enc, 769, val, sceNet_lib_D8722983(val), 0);
    if (ret < 0)
        return ret;
    return fw_str(fw, key_id, enc);
}

static int fw_prefixed_encoded(StoreFormWriter *fw, const char * const *prefixes, u32 prefix_cnt,
                               u32 key_id, const char *val, u32 val_len)
{
    char enc[784];
    int ret = sceNpCommerce2_66EC0829(enc, 769, val, val_len, 0);
    if (ret < 0)
        return ret;
    return fw_prefixed_val(fw, prefixes, prefix_cnt, key_id, enc, sceNet_lib_D8722983(enc));
}

static int fw_uint(StoreFormWriter *fw, u32 key_id, u32 val)
{
    char num[16];
    int len = sceNet_lib_CF705E46(num, "%u", val);
    if (len >= (int)sizeof(num))
        return STORE_ERR_BUF_OVERFLOW;
    return fw_prefixed_val(fw, NULL, 0, key_id, num, (u32)len);
}

static int fw_prefixed_uint(StoreFormWriter *fw, const char * const *prefixes, u32 prefix_cnt,
                            u32 key_id, u32 val)
{
    char num[16];
    int len = sceNet_lib_CF705E46(num, "%u", val);
    if (len >= (int)sizeof(num))
        return STORE_ERR_BUF_OVERFLOW;
    return fw_prefixed_val(fw, prefixes, prefix_cnt, key_id, num, (u32)len);
}

static int fw_bool(StoreFormWriter *fw, u32 key_id, int flag)
{
    return fw_str(fw, key_id, flag == 1 ? "true" : "false");
}

static int fw_str_list(StoreFormWriter *fw, u32 key_id, const char * const *list,
                       u32 count, int repeat_first)
{
    char enc[784];
    u32 idx;
    int ret = fw_sep(fw);
    if (ret < 0)
        return ret;
    ret = fw_raw(fw, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;
    for (idx = 0; idx < count; idx++) {
        const char *item = repeat_first ? list[0] : list[idx];
        if (idx != 0) {
            ret = fw_raw(fw, ",", 1);
            if (ret < 0)
                return ret;
        }
        if (item && item[0] != '\0') {
            ret = sceNpCommerce2_66EC0829(enc, 769, item, sceNet_lib_D8722983(item), 0);
            if (ret < 0)
                return ret;
            ret = fw_raw(fw, enc, sceNet_lib_D8722983(enc));
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

static int fw_query_keys(StoreFormWriter *fw, u32 key_id, const StoreSearchQuery *queries,
                         const char * const *key_table, u32 count)
{
    char enc[784];
    u32 idx;
    int ret = fw_sep(fw);
    if (ret < 0)
        return ret;
    ret = fw_raw(fw, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;
    for (idx = 0; idx < count; idx++) {
        const char *kname = key_table[queries[idx].key_index];
        if (idx != 0) {
            ret = fw_raw(fw, ",", 1);
            if (ret < 0)
                return ret;
        }
        ret = sceNpCommerce2_66EC0829(enc, 769, kname, sceNet_lib_D8722983(kname), 0);
        if (ret < 0)
            return ret;
        ret = fw_raw(fw, enc, sceNet_lib_D8722983(enc));
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int fw_query_vals(StoreFormWriter *fw, u32 key_id, const StoreSearchQuery *queries, u32 count)
{
    char enc[784];
    u32 idx;
    int ret = fw_sep(fw);
    if (ret < 0)
        return ret;
    ret = fw_raw(fw, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;
    for (idx = 0; idx < count; idx++) {
        if (idx != 0) {
            ret = fw_raw(fw, ",", 1);
            if (ret < 0)
                return ret;
        }
        ret = sceNpCommerce2_66EC0829(enc, 769, queries[idx].value, queries[idx].value_len, 0);
        if (ret < 0)
            return ret;
        ret = fw_raw(fw, enc, sceNet_lib_D8722983(enc));
        if (ret < 0)
            return ret;
    }
    return 0;
}

/* =========================================================================
 * Endpoint resolution & HTTP request creation
 * ========================================================================= */

int store_resolve_endpoint(u32 svc_type, u16 cmd_id, const char **prefix_out, const char **endpoint_out)
{
    int env_ver;
    int ret;

    if (cmd_id >= 87)
        return STORE_ERR_INVALID_CMD;
    if (svc_type < 1 || svc_type > 8)
        return STORE_ERR_INVALID_SERVICE;

    ret = sceNpCommerce2_460A1D92(&env_ver, 9);
    if (ret < 0)
        return ret;

    *prefix_out = g_service_prefixes[svc_type];
    if (svc_type == 7) {
        if (cmd_id == 31)
            cmd_id = 74;
        else if (cmd_id == 48)
            cmd_id = 75;
    }
    *endpoint_out = g_endpoint_names[cmd_id - 1];
    return 0;
}

static int store_open_http_req(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                               int *http_req_out, u64 content_len)
{
    const char *prefix = NULL;
    const char *endpoint = NULL;
    int ret = store_resolve_endpoint(svc_type, cmd_id, &prefix, &endpoint);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_382803C8(http_tmpl, svc_type, cmd_id, prefix, endpoint, 0, url_buf,
                                   http_req_out, (u32)content_len, (u32)(content_len >> 32));
}

/* =========================================================================
 * Store request senders
 * ========================================================================= */

int store_send_empty(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out)
{
    return store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, 0);
}

int store_send_get_cat(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const char *cat_id, const char * const *reward_id, u32 has_reward,
                       u32 start_pos, u32 max_cnt)
{
    int pass;
    u32 eff_svc = (reward_id && has_reward) ? 1 : svc_type;

    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret;

        ret = fw_uint(&fw, SFKEY_STORE_TYPE, eff_svc);
        if (ret >= 0) ret = fw_encoded_str(&fw, SFKEY_CATEGORY_ID, cat_id);
        if (ret >= 0 && reward_id && has_reward)
            ret = fw_encoded_str(&fw, SFKEY_REWARD_ID, *reward_id);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_START_POSITION, start_pos);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_MAX_COUNT, max_cnt);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
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
    int pass;
    u32 eff_svc = (reward_id && has_reward) ? 1 : svc_type;

    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret;

        ret = fw_uint(&fw, SFKEY_STORE_TYPE, eff_svc);
        if (ret >= 0) ret = fw_encoded_str(&fw, SFKEY_PRODUCT_ID, prod_id);
        if (ret >= 0 && cat_id && cat_id[0] != '\0')
            ret = fw_encoded_str(&fw, SFKEY_CATEGORY_ID, cat_id);
        if (ret >= 0 && reward_id && has_reward)
            ret = fw_encoded_str(&fw, SFKEY_REWARD_ID, *reward_id);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
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
    int pass;
    int has_nonempty_reward = 0;

    if (reward_ids && reward_cnt != 0 && reward_cnt == sku_cnt) {
        u32 idx;
        for (idx = 0; idx < reward_cnt; idx++) {
            if (reward_ids[idx] && reward_ids[idx][0] != '\0') {
                has_nonempty_reward = 1;
                break;
            }
        }
    }

    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret;

        ret = fw_uint(&fw, SFKEY_STORE_TYPE, svc_type);
        if (ret >= 0) ret = fw_str_list(&fw, SFKEY_SKU_ID, sku_ids, sku_cnt, 0);
        if (ret >= 0 && has_nonempty_reward)
            ret = fw_str_list(&fw, SFKEY_REWARD_IDS, reward_ids, reward_cnt, 0);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_sku_id(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                      const char *sku_id)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret = fw_encoded_str(&fw, SFKEY_SKU_ID, sku_id);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_purchase(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char *password, int quick_purchase, int auto_deposit)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret = 0;

        if (password && password[0] != '\0')
            ret = fw_encoded_str(&fw, SFKEY_PASSWORD, password);
        if (ret >= 0) ret = fw_bool(&fw, SFKEY_QUICK_PURCHASE, quick_purchase);
        if (ret >= 0) ret = fw_bool(&fw, SFKEY_AUTO_DEPOSIT_FLAG, auto_deposit);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_payment_type(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                            u32 pay_type)
{
    int pass;
    if (pay_type != 0)
        return STORE_ERR_INVALID_PAYMENT;

    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret = fw_str(&fw, SFKEY_PAYMENT_TYPE, "CC");
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
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
    int pass;
    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret = 0;

        if (sku_ids && sku_cnt != 0)
            ret = fw_str_list(&fw, SFKEY_SKU_IDS, sku_ids, sku_cnt, 0);
        if (ret >= 0 && svc_id && svc_id[0] != '\0')
            ret = fw_encoded_str(&fw, SFKEY_SERVICE_ID, svc_id);
        if (ret >= 0 && reward_ids)
            ret = fw_encoded_str(&fw, SFKEY_REWARD_IDS, reward_ids);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_START_POSITION, start_pos);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_MAX_COUNT, max_cnt);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_DRM_CONTENT_TYPE, drm_type);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_billing(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const StoreBillingInfo *info)
{
    static const char * const cc_pfx[1]   = { "creditCard." };
    static const char * const addr_pfx[2] = { "creditCard.", "address." };
    static const char * const street_pfx[2] = { "creditCard.", "address" };
    int pass;

    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret = 0;

        if (info->payment_method_id && info->payment_method_id_len)
            ret = fw_prefixed_encoded(&fw, cc_pfx, 1, SFKEY_PAYMENT_METHOD_ID,
                                      info->payment_method_id, info->payment_method_id_len);
        if (ret >= 0 && info->holder_name && info->holder_name_len)
            ret = fw_prefixed_encoded(&fw, cc_pfx, 1, SFKEY_HOLDER_NAME,
                                      info->holder_name, info->holder_name_len);
        if (ret >= 0 && info->card_number && info->card_number[0] != '\0')
            ret = fw_prefixed_encoded(&fw, cc_pfx, 1, SFKEY_CARD_NUMBER,
                                      info->card_number, sceNet_lib_D8722983(info->card_number));
        if (ret >= 0 && info->start_year != 0)
            ret = fw_prefixed_uint(&fw, cc_pfx, 1, SFKEY_START_YEAR, info->start_year);
        if (ret >= 0 && info->start_month != 0)
            ret = fw_prefixed_uint(&fw, cc_pfx, 1, SFKEY_START_MONTH, info->start_month);
        if (ret >= 0 && info->expire_year != 0)
            ret = fw_prefixed_uint(&fw, cc_pfx, 1, SFKEY_EXPIRE_YEAR, info->expire_year);
        if (ret >= 0 && info->expire_month != 0)
            ret = fw_prefixed_uint(&fw, cc_pfx, 1, SFKEY_EXPIRE_MONTH, info->expire_month);
        if (ret >= 0 && info->security_code && info->security_code[0] != '\0')
            ret = fw_prefixed_encoded(&fw, cc_pfx, 1, SFKEY_SECURITY_CODE,
                                      info->security_code, sceNet_lib_D8722983(info->security_code));
        if (ret >= 0 && info->issue_number && info->issue_number[0] != '\0')
            ret = fw_prefixed_encoded(&fw, cc_pfx, 1, SFKEY_ISSUE_NUMBER,
                                      info->issue_number, sceNet_lib_D8722983(info->issue_number));
        if (ret >= 0 && info->street1 && info->street1_len)
            ret = fw_prefixed_encoded(&fw, street_pfx, 2, SFKEY_1, info->street1, info->street1_len);
        if (ret >= 0 && info->street2 && info->street2_len)
            ret = fw_prefixed_encoded(&fw, street_pfx, 2, SFKEY_2, info->street2, info->street2_len);
        if (ret >= 0 && info->street3 && info->street3_len)
            ret = fw_prefixed_encoded(&fw, street_pfx, 2, SFKEY_3, info->street3, info->street3_len);
        if (ret >= 0 && info->city && info->city_len)
            ret = fw_prefixed_encoded(&fw, addr_pfx, 2, SFKEY_CITY, info->city, info->city_len);
        if (ret >= 0 && info->province && info->province_len)
            ret = fw_prefixed_encoded(&fw, addr_pfx, 2, SFKEY_PROVINCE, info->province, info->province_len);
        if (ret >= 0 && info->postal_code && info->postal_code_len)
            ret = fw_prefixed_encoded(&fw, addr_pfx, 2, SFKEY_POSTAL_CODE, info->postal_code, info->postal_code_len);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_postal_code(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                           const char *postal_code)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret = fw_encoded_str(&fw, SFKEY_POSTAL_CODE, postal_code);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_topup_amount(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                            u32 amount)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret = fw_uint(&fw, SFKEY_TOPUP_AMOUNT, amount);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_edy_result(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                          const char *receipt_id, int is_ok)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret = fw_encoded_str(&fw, SFKEY_EDY_RECEIPT_ID, receipt_id);
        if (ret >= 0) ret = fw_str(&fw, SFKEY_RESULT_EDY_TOPUP, is_ok == 1 ? "OK" : "NG");
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_voucher(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const char *code1, const char *code2, const char *code3, int auto_deposit)
{
    static const char * const vchr_pfx[1] = { "voucherCode" };
    int pass;

    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret;

        ret = fw_prefixed_encoded(&fw, vchr_pfx, 1, SFKEY_1, code1, sceNet_lib_D8722983(code1));
        if (ret >= 0) ret = fw_prefixed_encoded(&fw, vchr_pfx, 1, SFKEY_2, code2, sceNet_lib_D8722983(code2));
        if (ret >= 0) ret = fw_prefixed_encoded(&fw, vchr_pfx, 1, SFKEY_3, code3, sceNet_lib_D8722983(code3));
        if (ret >= 0) ret = fw_bool(&fw, SFKEY_AUTO_DEPOSIT_FLAG, auto_deposit);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_event_log(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         u32 event_type, const char *id_str, const char *url_str)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret;

        ret = fw_uint(&fw, SFKEY_STORE_TYPE, svc_type);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_EVENT_TYPE, event_type);
        if (ret >= 0) ret = fw_str(&fw, SFKEY_ID, id_str);
        if (ret >= 0 && url_str && url_str[0] != '\0')
            ret = fw_str(&fw, SFKEY_URL, url_str);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_search_prod(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                           u32 start_pos, u32 max_cnt, const StoreSearchQuery *queries, u32 query_cnt)
{
    const char * const *key_table;
    int pass;

    if (svc_type == 1 || svc_type == 3)
        key_table = g_game_query_keys;
    else if (svc_type == 4)
        key_table = g_comic_query_keys;
    else
        key_table = g_video_query_keys;

    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret;

        ret = fw_uint(&fw, SFKEY_STORE_TYPE, svc_type);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_START_POSITION, start_pos);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_MAX_COUNT, max_cnt);
        if (ret >= 0) ret = fw_query_keys(&fw, SFKEY_QUERY_KEY, queries, key_table, query_cnt);
        if (ret >= 0) ret = fw_query_vals(&fw, SFKEY_QUERY_VALUE, queries, query_cnt);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_guest_sess(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                          u32 ver, const char *country, const char *lang,
                          u32 yob, u32 mob, u32 dob, const char *console_id)
{
    static const char * const acct_pfx[1] = { "account." };
    char upper_cc[16];
    char lower_lang[16];
    u32 idx, len;
    int pass;

    len = sceNet_lib_D8722983(country);
    for (idx = 0; idx < len && idx < 15; idx++)
        upper_cc[idx] = (char)sceNet_lib_C13C9307((u8)country[idx]);
    upper_cc[idx] = '\0';

    len = sceNet_lib_D8722983(lang);
    for (idx = 0; idx < len && idx < 15; idx++)
        lower_lang[idx] = (char)sceNet_lib_96EF9DA1((u8)lang[idx]);
    lower_lang[idx] = '\0';

    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret;

        ret = fw_uint(&fw, SFKEY_VERSION, ver);
        if (ret >= 0) ret = fw_prefixed_val(&fw, acct_pfx, 1, SFKEY_COUNTRY, upper_cc, sceNet_lib_D8722983(upper_cc));
        if (ret >= 0) ret = fw_prefixed_val(&fw, acct_pfx, 1, SFKEY_LANGUAGE, lower_lang, sceNet_lib_D8722983(lower_lang));
        if (ret >= 0) ret = fw_prefixed_uint(&fw, acct_pfx, 1, SFKEY_YOB, yob);
        if (ret >= 0) ret = fw_prefixed_uint(&fw, acct_pfx, 1, SFKEY_MOB, mob);
        if (ret >= 0) ret = fw_prefixed_uint(&fw, acct_pfx, 1, SFKEY_DOB, dob);
        if (ret >= 0) ret = fw_str(&fw, SFKEY_CONSOLE_ID, console_id);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_prod_list(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         const char * const *prod_ids, u32 prod_cnt)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret;

        ret = fw_uint(&fw, SFKEY_STORE_TYPE, svc_type);
        if (ret >= 0) ret = fw_str_list(&fw, SFKEY_PRODUCT_IDS, prod_ids, prod_cnt, 0);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_recom_prods(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                           const char * const *prod_ids, u32 prod_cnt,
                           u32 start_pos, u32 max_cnt, u32 recom_type)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret;

        ret = fw_uint(&fw, SFKEY_STORE_TYPE, svc_type);
        if (ret >= 0 && prod_ids && prod_cnt != 0)
            ret = fw_str_list(&fw, SFKEY_PRODUCT_IDS, prod_ids, prod_cnt, 0);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_START_POSITION, start_pos);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_MAX_COUNT, max_cnt);
        if (ret >= 0) ret = fw_uint(&fw, SFKEY_RECOM_TYPE, recom_type);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int store_send_get_prod_ids(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                            const char *content_id, const char *file_id)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        StoreFormWriter fw = { pass, *http_req_out, 0, 0 };
        int ret;

        if (content_id)
            ret = fw_encoded_str(&fw, SFKEY_CONTENT_ID, content_id);
        else
            ret = fw_encoded_str(&fw, SFKEY_FILE_ID, file_id);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = store_open_http_req(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, fw.total_len);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

/* =========================================================================
 * SDK-checked high-level wrappers (with NP debug ID prefix logging)
 * ========================================================================= */

static void store_log_id_prefix(const char *tag, const char * const *ids, u32 count)
{
    char snippet[32];
    u32 idx;

    if (!sceNp_2E4769C3() || !tag || !ids || count == 0)
        return;

    for (idx = 0; idx < count; idx++) {
        const char *id = ids[idx];
        const char *p = id;
        u32 len;

        while (*p && *p != '-')
            p++;
        if (*p)
            p++;
        while (*p && *p != '-')
            p++;
        len = (u32)(p - id);
        if (len > 31)
            len = 31;
        sceKernelMemcpy(snippet, id, len);
        snippet[len] = '\0';

        if (count < 2) {
            KprintfForUser("%s: %s\n", tag, snippet);
        } else if (idx == 0) {
            KprintfForUser("%s: %s", tag, snippet);
        } else {
            KprintfForUser(", %s", snippet);
        }
    }
    if (count >= 2)
        KprintfForUser("\n");
}

int store_checked_recv(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out, int strict_size)
{
    int sdk_ver = sceKernelGetCompiledSdkVersion();
    if (sdk_ver > STORE_SDK_VER_550 && _sceKernelCheckThreadStack() < STORE_MIN_STACK_LARGE)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (sdk_ver > STORE_SDK_VER_550 && strict_size == 1 && buf_size < 0x40000U)
        return STORE_ERR_RESP_BUF_LIMIT;
    return store_recv_response(req_id, buf, buf_size, recv_size_out);
}

int store_checked_get_cat(u32 req_id, const char *cat_id, const char * const *reward_id,
                          u32 has_reward, u32 start_pos, u32 max_cnt, int strict_check)
{
    int sdk_ver = sceKernelGetCompiledSdkVersion();
    if (sdk_ver > STORE_SDK_VER_550 && _sceKernelCheckThreadStack() < STORE_MIN_STACK_LARGE)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!cat_id || (strict_check == 1 && sdk_ver > STORE_SDK_VER_550 && max_cnt >= 61))
        return STORE_ERR_INVALID_ARG;
    if (strict_check == 1)
        store_log_id_prefix("GetCategoryContents", &cat_id, 1);
    return store_start_get_cat(req_id, cat_id, reward_id, has_reward, start_pos, max_cnt);
}

int store_checked_get_prod(u32 req_id, const char *cat_id, const char *prod_id,
                           const char * const *reward_id, u32 has_reward, int strict_check)
{
    if (sceKernelGetCompiledSdkVersion() > STORE_SDK_VER_550 &&
        _sceKernelCheckThreadStack() < STORE_MIN_STACK_LARGE)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!prod_id)
        return STORE_ERR_INVALID_ARG;
    if (strict_check == 1)
        store_log_id_prefix("GetProductInfo", &prod_id, 1);
    return store_start_get_prod(req_id, cat_id, prod_id, reward_id, has_reward);
}

int store_checked_get_prod_list(u32 req_id, const char * const *prod_ids, u32 prod_cnt, int strict_check)
{
    if (_sceKernelCheckThreadStack() < STORE_MIN_STACK_STD)
        return STORE_ERR_STACK_OVERFLOW;
    if (!g_store_init)
        return STORE_ERR_NOT_INITIALIZED;
    if (!prod_ids || prod_cnt == 0 || (strict_check == 1 && prod_cnt >= 61))
        return STORE_ERR_INVALID_ARG;
    if (strict_check == 1)
        store_log_id_prefix("GetProductInfoList", prod_ids, prod_cnt);
    return store_start_prod_list(req_id, prod_ids, prod_cnt);
}
