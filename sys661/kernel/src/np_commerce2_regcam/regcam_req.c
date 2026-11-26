#include "regcam_internal.h"

const char * const g_endpoint_names[86] = {
    "crSess", "delSess", "getCat", "getProd", "srchProd", "getWal", "getDrm",
    "purPrev", "purch", "getCC", "topupCC", "vchrRedPrev", "vchrRed",
    "topEdy", "topEdyRes", "addCart", "remCart", "clearCart", "getCart",
    "getProv", "getPay", "getBill", "regCC", "getAddr", "getResAddr",
    "eventLog", "getCountries", "bindAccount", "startReg", "getSecQ",
    "setCred", "setLid", "updPass", "verifyOid", "setOid", "getName",
    "setName", "setResAddr", "setRrn", "getOptIn", "setOptIn", "authMaster",
    "getBillInfo", "setSubCtrl", "resetSubPass", "getWalOpt", "setWalOpt",
    "submitReg", "authenticate", "remPay", "getTranHist", "getTranDetail",
    "getSrvs", "getSrvDetail", "getSubscr", "getSubscrPlan", "changeSubscr",
    "cancelSubscr", "getCnslAct", "acptEula", "verifyDob", "verifySecAns",
    "sendPassResetMail", "verifyMasterBill", "getEpw", "crGuestSess",
    "resetPass", "getMasterBill", "sendConsentMail", "getMasterResAddr",
    "getProdList", "getSkuList", "getReward", "setSecQ", "submitUpgrade",
    "recomProds", "verifyBill", "regDevice", "deregDevice", "getProdIds",
    "crSessTicket", "getUsersFromCID", "getActCode", "setAccountLang",
    "getPivotForProd", "getProdForPivot",
};

const char * const g_service_prefixes[9] = {
    "", "store/", "store/", "store/", "store/",
    "reg/", "cam/", "upgrade/", "pwd/",
};

const RegCamFormKey g_form_keys[81] = {
    { 1,  "&" },                     { 1,  "," },
    { 8,  "version=" },              { 9,  "username=" },
    { 9,  "password=" },             { 10, "consoleId=" },
    { 11, "categoryId=" },           { 14, "startPosition=" },
    { 18, "maxCountOfResults=" },    { 10, "productId=" },
    { 10, "storeType=" },            { 9,  "queryKey=" },
    { 11, "queryValue=" },           { 18, "contentSortMethod=" },
    { 6,  "skuId=" },                { 7,  "skuIds=" },
    { 8,  "titleId=" },              { 15, "drmContentType=" },
    { 14, "quickPurchase=" },        { 12, "topupAmount=" },
    { 2,  "1=" },                    { 2,  "2=" },
    { 2,  "3=" },                    { 13, "edyReceiptId=" },
    { 21, "resultOfTopupFromEdy=" }, { 12, "paymentType=" },
    { 16, "paymentMethodId=" },      { 11, "holderName=" },
    { 11, "cardNumber=" },           { 10, "startYear=" },
    { 11, "startMonth=" },           { 11, "expireYear=" },
    { 12, "expireMonth=" },          { 13, "securityCode=" },
    { 12, "issueNumber=" },          { 5,  "city=" },
    { 9,  "province=" },             { 11, "postalCode=" },
    { 10, "eventType=" },            { 3,  "id=" },
    { 4,  "url=" },                  { 8,  "country=" },
    { 9,  "language=" },             { 4,  "yob=" },
    { 4,  "mob=" },                  { 4,  "dob=" },
    { 10, "loginName=" },            { 17, "securityQuestion=" },
    { 15, "securityAnswer=" },       { 9,  "psHandle=" },
    { 10, "firstName=" },            { 9,  "lastName=" },
    { 15, "firstNameASCII=" },       { 14, "lastNameASCII=" },
    { 7,  "gender=" },               { 14, "scexOptinFlag=" },
    { 20, "affiliatesOptinFlag=" },  { 13, "subLoginName=" },
    { 14, "contentRating=" },        { 12, "chatControl=" },
    { 14, "searchControl=" },        { 18, "spendingLimitFlag=" },
    { 14, "spendingLimit=" },        { 16, "autoDepositFlag=" },
    { 21, "commercePasswordFlag=" }, { 9,  "fromYear=" },
    { 10, "fromMonth=" },            { 8,  "fromDay=" },
    { 7,  "toYear=" },               { 8,  "toMonth=" },
    { 6,  "toDay=" },                { 9,  "timezone=" },
    { 14, "transactionId=" },        { 10, "serviceId=" },
    { 5,  "save=" },                 { 9,  "rewardId=" },
    { 10, "rewardIds=" },            { 11, "productIds=" },
    { 19, "recommendationType=" },   { 10, "contentId=" },
    { 7,  "fileId=" },
};

const char * const g_gender_strs[3] = { "XXX", "m", "f" };
const char *g_payment_types[1] = { "CC" };

/* Forward declarations for 2-pass body builders */
static __attribute__((noinline)) int regcam_build_billing_form(int write_pass, int http_req, u64 *len_out,
                                                               int prefix_mode, const RegCamBillingInfo *info,
                                                               int include_save, u32 save_flag);
static __attribute__((noinline)) int regcam_build_bind_account(int write_pass, int http_req, u64 *len_out,
                                                               u32 ver, const char *user, const char *pass_str, const char *cid);
static __attribute__((noinline)) int regcam_build_start_reg(int write_pass, int http_req, u64 *len_out,
                                                            u32 ver, const char *country, const char *lang,
                                                            u32 yob, u32 mob, u32 dob, const char *cid);
static __attribute__((noinline)) int regcam_build_set_cred(int write_pass, int http_req, u64 *len_out,
                                                           const char *login, const char *pass_str,
                                                           u32 sec_q, const char *sec_ans, u32 sec_ans_len);
static __attribute__((noinline)) int regcam_build_set_lid(int write_pass, int http_req, u64 *len_out, const char *login);
static __attribute__((noinline)) int regcam_build_upd_pass(int write_pass, int http_req, u64 *len_out, const char *pass_str);
static __attribute__((noinline)) int regcam_build_set_oid(int write_pass, int http_req, u64 *len_out, const char *oid);
static __attribute__((noinline)) int regcam_build_set_name(int write_pass, int http_req, u64 *len_out,
                                                           const char *first, u32 first_len, const char *last, u32 last_len,
                                                           const char *first_ascii, const char *last_ascii, u32 gender);
static __attribute__((noinline)) int regcam_build_set_opt_in(int write_pass, int http_req, u64 *len_out,
                                                             u32 scex_flag, u32 aff_flag);
static __attribute__((noinline)) int regcam_build_auth_master(int write_pass, int http_req, u64 *len_out,
                                                              const char *login, const char *pass_str);
static __attribute__((noinline)) int regcam_build_set_subacct_ctrl(int write_pass, int http_req, u64 *len_out,
                                                                   const char *child_login, u32 content_rating,
                                                                   u32 chat_ctrl, u32 limit_flag, u32 limit_val);
static __attribute__((noinline)) int regcam_build_reset_subacct_pass(int write_pass, int http_req, u64 *len_out,
                                                                     const char *child_login, const char *new_pass);
static __attribute__((noinline)) int regcam_build_set_wal_opt(int write_pass, int http_req, u64 *len_out,
                                                              u32 auto_deposit, u32 pwd_flag);
static __attribute__((noinline)) int regcam_build_authenticate(int write_pass, int http_req, u64 *len_out,
                                                               u32 ver, const char *user, const char *pass_str, const char *cid);
static __attribute__((noinline)) int regcam_build_get_tran_hist(int write_pass, int http_req, u64 *len_out,
                                                                const char *login, u32 from_y, u32 from_m, u32 from_d,
                                                                u32 to_y, u32 to_m, u32 to_d, s32 tz_min);
static __attribute__((noinline)) int regcam_build_get_srv_detail(int write_pass, int http_req, u64 *len_out,
                                                                 const char *srv_id, u32 start_pos, u32 max_cnt);
static __attribute__((noinline)) int regcam_build_get_subscr(int write_pass, int http_req, u64 *len_out,
                                                             const char *srv_id, u32 start_pos, u32 max_cnt);
static __attribute__((noinline)) int regcam_build_verify_dob(int write_pass, int http_req, u64 *len_out,
                                                             u32 ver, const char *login, u32 yob, u32 mob, u32 dob, const char *cid);
static __attribute__((noinline)) int regcam_build_verify_sec_ans(int write_pass, int http_req, u64 *len_out,
                                                                 u32 sec_q, const char *sec_ans, u32 sec_ans_len);
static __attribute__((noinline)) int regcam_build_set_account_lang(int write_pass, int http_req, u64 *len_out, const char *lang);

static inline int regcam_open_post(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                                   int *http_req_out, u64 content_len)
{
    const char *prefix = NULL;
    const char *endpoint = NULL;
    int ret;

    ret = regcam_resolve_endpoint(svc_type, cmd_id, &prefix, &endpoint);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_382803C8(http_tmpl, svc_type, cmd_id, prefix, endpoint, 0,
                                   url_buf, http_req_out, (u32)content_len, (u32)(content_len >> 32));
}

static inline __attribute__((always_inline)) int regcam_emit_plain_field(int write_pass, int http_req, u32 key_id,
                                                                         const char *val, int prepend_amp, u64 *total_out)
{
    int ret;
    if (write_pass == 1) {
        if (prepend_amp == 1) {
            ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[FKEY_AMP].str, g_form_keys[FKEY_AMP].len);
            if (ret < 0)
                return ret;
        }
        ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[key_id].str, g_form_keys[key_id].len);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_AE614CC6(http_req, val, sceNet_lib_D8722983(val));
        if (ret < 0)
            return ret;
    } else {
        *total_out += (u32)((prepend_amp == 1 ? 1U : 0U) + g_form_keys[key_id].len + sceNet_lib_D8722983(val));
    }
    return 0;
}

static inline int regcam_emit_plain_uint(int write_pass, int http_req, u32 key_id,
                                         u32 val, int prepend_amp, u64 *total_out)
{
    char num_buf[16];
    int len;

    if (write_pass == 1)
        return regcam_write_uint_field(http_req, key_id, val, prepend_amp);
    len = sceNet_lib_CF705E46(num_buf, "%u", val);
    if (len >= 16)
        return REGCAM_ERR_BUF_OVERFLOW;
    *total_out += (u32)len + (prepend_amp == 1 ? 1U : 0U) + g_form_keys[key_id].len;
    return 0;
}

static inline __attribute__((always_inline)) int regcam_emit_prefixed_raw(int write_pass, int http_req, const char * const *prefixes,
                                                                          u32 prefix_cnt, u32 key_id, const void *val, u32 val_len,
                                                                          int prepend_amp, u64 *total_out)
{
    u32 idx;
    int ret;

    if (write_pass == 1) {
        ret = regcam_write_prefixed_field(http_req, prefixes, prefix_cnt, key_id, val, val_len, prepend_amp);
        if (ret < 0)
            return ret;
    } else {
        u32 bytes = (prepend_amp == 1 ? 1U : 0U) + g_form_keys[key_id].len + val_len;
        for (idx = 0; idx < prefix_cnt; idx++) {
            if (prefixes[idx] != NULL)
                bytes += sceNet_lib_D8722983(prefixes[idx]);
        }
        *total_out += bytes;
    }
    return 0;
}

static inline int regcam_emit_urlenc_prefixed(char *enc_buf, int write_pass, int http_req, const char * const *prefixes,
                                              u32 prefix_cnt, u32 key_id, const char *src, u32 src_len,
                                              int prepend_amp, u64 *total_out)
{
    int ret;

    ret = sceNpCommerce2_66EC0829(enc_buf, 769, src, src_len, 0);
    if (ret < 0)
        return ret;
    if (prefixes == NULL)
        return regcam_emit_plain_field(write_pass, http_req, key_id, enc_buf, prepend_amp, total_out);
    return regcam_emit_prefixed_raw(write_pass, http_req, prefixes, prefix_cnt,
                                    key_id, enc_buf, sceNet_lib_D8722983(enc_buf),
                                    prepend_amp, total_out);
}

static inline int regcam_emit_uint_prefixed(int write_pass, int http_req, const char * const *prefixes,
                                            u32 prefix_cnt, u32 key_id, u32 val, int prepend_amp, u64 *total_out)
{
    char num_buf[16];
    int len = sceNet_lib_CF705E46(num_buf, "%u", val);
    if (len >= 16)
        return REGCAM_ERR_BUF_OVERFLOW;
    return regcam_emit_prefixed_raw(write_pass, http_req, prefixes, prefix_cnt,
                                    key_id, num_buf, sceNet_lib_D8722983(num_buf),
                                    prepend_amp, total_out);
}

/* =========================================================================
 * Endpoint resolution & per-command request senders (0x0032b8 .. 0x005f23)
 * ========================================================================= */

int regcam_resolve_endpoint(u32 svc_type, u16 cmd_id, const char **prefix_out, const char **endpoint_out)
{
    int env_ver;
    int ret;

    if (cmd_id >= 87U)
        return REGCAM_ERR_INVALID_CMD;
    if ((u32)(svc_type - 1U) >= 8U)
        return REGCAM_ERR_INVALID_SERVICE;

    ret = sceNpCommerce2_460A1D92(&env_ver, 9);
    if (ret < 0)
        return ret;

    *prefix_out = g_service_prefixes[svc_type];
    if (svc_type == REGCAM_SVC_UPGRADE) {
        if (cmd_id == REGCAM_CMD_SET_CRED)
            cmd_id = REGCAM_CMD_SET_SEC_Q;
        else if (cmd_id == REGCAM_CMD_SUBMIT_REG)
            cmd_id = REGCAM_CMD_SUBMIT_UPGRADE;
    }
    *endpoint_out = g_endpoint_names[cmd_id - 1];
    return ret;
}

#define DEFINE_SEND_EMPTY(fn_name) \
    int fn_name(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out) { \
        int ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, 0); \
        if (ret < 0) return ret; \
        return regcam_min_s32(sceNpCommerce2_AE614CC6(*http_req_out, NULL, 0), 0); \
    }

DEFINE_SEND_EMPTY(regcam_send_del_sess)

int regcam_send_sku_id(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                       int *http_req_out, const char *sku_id)
{
    char enc_buf[784];
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        int http_req = *http_req_out;
        ret = sceNpCommerce2_66EC0829(enc_buf, 769, sku_id, sceNet_lib_D8722983(sku_id), 0);
        if (ret < 0)
            return ret;
        ret = regcam_emit_str_field(pass, http_req, FKEY_SKU_ID, enc_buf, 0);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            total = (u64)(s64)ret;
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

DEFINE_SEND_EMPTY(regcam_send_rem_pay)
DEFINE_SEND_EMPTY(regcam_send_get_prov)

int regcam_send_payment_type(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                             int *http_req_out, u32 pay_type)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        int http_req = *http_req_out;
        if (pay_type != 0)
            return REGCAM_ERR_INVALID_PAYMENT;
        ret = regcam_emit_str_field(pass, http_req, FKEY_PAYMENT_TYPE, g_payment_types[0], 0);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            total = (u64)(s64)ret;
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

DEFINE_SEND_EMPTY(regcam_send_get_master_res_addr)

int regcam_send_reg_cc(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                       int *http_req_out, const RegCamBillingInfo *info)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_billing_form(pass, *http_req_out, &total, 0, info, 0, 0);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int regcam_send_version_only(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                             int *http_req_out, u32 ver)
{
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_emit_uint_field(pass, *http_req_out, FKEY_VERSION, ver, 0);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, (u64)(s64)ret);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int regcam_send_bind_account(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                             u32 ver, const char *user, const char *pass_str, const char *cid)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_bind_account(pass, *http_req_out, &total, ver, user, pass_str, cid);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_start_reg(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                          u32 ver, const char *country, const char *lang, u32 yob, u32 mob, u32 dob, const char *cid)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_start_reg(pass, *http_req_out, &total, ver, country, lang, yob, mob, dob, cid);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

DEFINE_SEND_EMPTY(regcam_send_get_sec_q)

int regcam_send_set_cred(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         const char *login, const char *pass_str, u32 sec_q, const char *sec_ans, u32 sec_ans_len)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_set_cred(pass, *http_req_out, &total, login, pass_str, sec_q, sec_ans, sec_ans_len);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_set_lid(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out, const char *login)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_set_lid(pass, *http_req_out, &total, login);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_upd_pass(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out, const char *pass_str)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_upd_pass(pass, *http_req_out, &total, pass_str);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_oid(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out, const char *oid)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_set_oid(pass, *http_req_out, &total, oid);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

DEFINE_SEND_EMPTY(regcam_send_get_name)

int regcam_send_set_name(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         const char *first, u32 first_len, const char *last, u32 last_len,
                         const char *first_ascii, const char *last_ascii, u32 gender)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_set_name(pass, *http_req_out, &total, first, first_len, last, last_len,
                                    first_ascii, last_ascii, gender);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_set_res_addr(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                             int *http_req_out, const RegCamBillingInfo *info)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_billing_form(pass, *http_req_out, &total, 1, info, 0, 0);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_set_rrn(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char *rrn_front, const char *rrn_back)
{
    const char *kor_prefix[8];
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        int http_req = *http_req_out;
        int part_a, part_b;
        sceKernelMemset(kor_prefix, 0, sizeof(kor_prefix));
        kor_prefix[0] = "koreanId";
        part_a = regcam_emit_prefixed_str(pass, http_req, kor_prefix, 1, FKEY_LINE_A, rrn_front, 0);
        if (part_a < 0) return part_a;
        part_b = regcam_emit_prefixed_str(pass, http_req, kor_prefix, 1, FKEY_LINE_B, rrn_back, 1);
        if (part_b < 0) return part_b;
        if (pass == 0) {
            total = (u64)(s64)part_a + (u64)(s64)part_b;
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

DEFINE_SEND_EMPTY(regcam_send_get_opt_in)

int regcam_send_set_opt_in(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                           u32 scex_flag, u32 aff_flag)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_set_opt_in(pass, *http_req_out, &total, scex_flag, aff_flag);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_auth_master(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                            const char *login, const char *pass_str)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_auth_master(pass, *http_req_out, &total, login, pass_str);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_set_subacct_ctrl(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                                 const char *child_login, u32 content_rating, u32 chat_ctrl, u32 limit_flag, u32 limit_val)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_set_subacct_ctrl(pass, *http_req_out, &total, child_login,
                                            content_rating, chat_ctrl, limit_flag, limit_val);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_reset_subacct_pass(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                                   const char *child_login, const char *new_pass)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_reset_subacct_pass(pass, *http_req_out, &total, child_login, new_pass);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

DEFINE_SEND_EMPTY(regcam_send_get_wal_opt)

int regcam_send_set_wal_opt(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                            u32 auto_deposit, u32 pwd_flag)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_set_wal_opt(pass, *http_req_out, &total, auto_deposit, pwd_flag);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

DEFINE_SEND_EMPTY(regcam_send_submit_reg)

int regcam_send_authenticate(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                             u32 ver, const char *user, const char *pass_str, const char *cid)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_authenticate(pass, *http_req_out, &total, ver, user, pass_str, cid);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_get_tran_hist(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                              const char *login, u32 from_y, u32 from_m, u32 from_d,
                              u32 to_y, u32 to_m, u32 to_d, s32 tz_min)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_get_tran_hist(pass, *http_req_out, &total, login,
                                         from_y, from_m, from_d, to_y, to_m, to_d, tz_min);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_get_tran_detail(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out, u64 tx_id)
{
    char tx_str[32];
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        int http_req = *http_req_out;
        sceNpCommerce2_22CEC5E2(tx_str, sizeof(tx_str), (u32)tx_id, (u32)(tx_id >> 32), 1);
        ret = regcam_emit_str_field(pass, http_req, FKEY_TRANSACTION_ID, tx_str, 0);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, (u64)(s64)ret);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_get_srvs(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         u32 start_pos, u32 max_cnt)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        int http_req = *http_req_out;
        int len_a = regcam_emit_uint_field(pass, http_req, FKEY_START_POSITION, start_pos, 0);
        int len_b;
        if (len_a < 0) return len_a;
        len_b = regcam_emit_uint_field(pass, http_req, FKEY_MAX_COUNT, max_cnt, 1);
        if (len_b < 0) return len_b;
        if (pass == 0) {
            total = (u64)(s64)len_a + (u64)(s64)len_b;
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_get_srv_detail(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                               const char *srv_id, u32 start_pos, u32 max_cnt)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_get_srv_detail(pass, *http_req_out, &total, srv_id, start_pos, max_cnt);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_get_subscr(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                           const char *srv_id, u32 start_pos, u32 max_cnt)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_get_subscr(pass, *http_req_out, &total, srv_id, start_pos, max_cnt);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_verify_dob(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                           u32 ver, const char *login, u32 yob, u32 mob, u32 dob, const char *cid)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_verify_dob(pass, *http_req_out, &total, ver, login, yob, mob, dob, cid);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_verify_sec_ans(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                               u32 sec_q, const char *sec_ans, u32 sec_ans_len)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_verify_sec_ans(pass, *http_req_out, &total, sec_q, sec_ans, sec_ans_len);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_verify_master_bill(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                                   const RegCamBillingInfo *info, u32 save_flag)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_billing_form(pass, *http_req_out, &total, 0, info, 1, save_flag);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

DEFINE_SEND_EMPTY(regcam_send_mail)

int regcam_send_verify_bill(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                            const RegCamBillingInfo *info)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_billing_form(pass, *http_req_out, &total, 0, info, 0, 0);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

int regcam_send_set_account_lang(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                                 const char *lang)
{
    u64 total = 0;
    int pass, ret;

    for (pass = 0; pass < 2; pass++) {
        ret = regcam_build_set_account_lang(pass, *http_req_out, &total, lang);
        if (ret < 0) return ret;
        if (pass == 0) {
            ret = regcam_open_post(svc_type, http_tmpl, url_buf, cmd_id, http_req_out, total);
            if (ret < 0) return ret;
        }
    }
    return 0;
}

/* =========================================================================
 * Emit helpers (0x005f24 .. 0x00619f)
 * ========================================================================= */

int regcam_emit_str_field(int write_pass, int http_req, u32 key_id, const char *val, int prepend_amp)
{
    int ret;

    if (write_pass != 1)
        return (int)((prepend_amp == 1 ? 1U : 0U) + g_form_keys[key_id].len + sceNet_lib_D8722983(val));
    if (prepend_amp == 1) {
        ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[FKEY_AMP].str, g_form_keys[FKEY_AMP].len);
        if (ret < 0)
            return ret;
    }
    ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_AE614CC6(http_req, val, sceNet_lib_D8722983(val));
}

int regcam_emit_uint_field(int write_pass, int http_req, u32 key_id, u32 val, int prepend_amp)
{
    char num_buf[16];
    int num_len;

    if (write_pass == 1)
        return regcam_write_uint_field(http_req, key_id, val, prepend_amp);
    num_len = sceNet_lib_CF705E46(num_buf, "%u", val);
    if (num_len >= 16)
        return REGCAM_ERR_BUF_OVERFLOW;
    return num_len + (int)((prepend_amp == 1 ? 1U : 0U) + g_form_keys[key_id].len);
}

int regcam_emit_prefixed_str(int write_pass, int http_req, const char * const *prefixes,
                             u32 prefix_cnt, u32 key_id, const char *val, int prepend_amp)
{
    u32 val_len = sceNet_lib_D8722983(val);
    u32 total, idx;

    if (write_pass == 1)
        return regcam_write_prefixed_field(http_req, prefixes, prefix_cnt, key_id, val, val_len, prepend_amp);
    total = (prepend_amp == 1 ? 1U : 0U);
    for (idx = 0; idx < prefix_cnt; idx++)
        total += sceNet_lib_D8722983(prefixes[idx]);
    return (int)(total + g_form_keys[key_id].len + val_len);
}

/* =========================================================================
 * 2-pass body builders (0x0061a0 .. 0x009e37)
 * ========================================================================= */

static __attribute__((noinline)) int regcam_build_billing_form(int write_pass, int http_req, u64 *len_out,
                                                               int prefix_mode, const RegCamBillingInfo *info,
                                                               int include_save, u32 save_flag)
{
    char enc_buf[784];
    const char *prefixes[3];
    u64 total = 0;
    int has_field = 0;
    int ret;

    sceKernelMemset(prefixes, 0, sizeof(prefixes));
    if (prefix_mode == 0)
        prefixes[0] = "creditCard.";
    else if (prefix_mode == 1)
        prefixes[0] = "account.";
    prefixes[1] = "address.";
    prefixes[2] = "address";

    if (info->payment_method_id != NULL && info->payment_method_id[0] != '\0' && info->payment_method_id_len != 0) {
        u32 len = info->payment_method_id_len;
        if (info->payment_method_id[len - 1] == '\0')
            len--;
        ret = regcam_emit_prefixed_raw(write_pass, http_req, prefixes, 1,
                                       FKEY_PAYMENT_METHOD_ID, info->payment_method_id, len, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->holder_name != NULL && info->holder_name[0] != '\0' && info->holder_name_len != 0) {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, prefixes, 1,
                                          FKEY_HOLDER_NAME, info->holder_name, info->holder_name_len, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->card_number != NULL && info->card_number[0] != '\0') {
        ret = regcam_emit_prefixed_raw(write_pass, http_req, prefixes, 1,
                                       FKEY_CARD_NUMBER, info->card_number, sceNet_lib_D8722983(info->card_number),
                                       has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->start_year != 0) {
        ret = regcam_emit_uint_prefixed(write_pass, http_req, prefixes, 1, FKEY_START_YEAR, info->start_year, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->start_month != 0) {
        ret = regcam_emit_uint_prefixed(write_pass, http_req, prefixes, 1, FKEY_START_MONTH, info->start_month, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->expire_year != 0) {
        ret = regcam_emit_uint_prefixed(write_pass, http_req, prefixes, 1, FKEY_EXPIRE_YEAR, info->expire_year, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->expire_month != 0) {
        ret = regcam_emit_uint_prefixed(write_pass, http_req, prefixes, 1, FKEY_EXPIRE_MONTH, info->expire_month, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->security_code != NULL && info->security_code[0] != '\0') {
        ret = regcam_emit_prefixed_raw(write_pass, http_req, prefixes, 1,
                                       FKEY_SECURITY_CODE, info->security_code, sceNet_lib_D8722983(info->security_code),
                                       has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->issue_number != NULL && info->issue_number[0] != '\0') {
        ret = regcam_emit_prefixed_raw(write_pass, http_req, prefixes, 1,
                                       FKEY_ISSUE_NUMBER, info->issue_number, sceNet_lib_D8722983(info->issue_number),
                                       has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->street_a != NULL && info->street_a[0] != '\0' && info->street_a_len != 0) {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, prefixes, 3,
                                          FKEY_LINE_A, info->street_a, info->street_a_len, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->street_b != NULL && info->street_b[0] != '\0' && info->street_b_len != 0) {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, prefixes, 3,
                                          FKEY_LINE_B, info->street_b, info->street_b_len, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->street_c != NULL && info->street_c[0] != '\0' && info->street_c_len != 0) {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, prefixes, 3,
                                          FKEY_LINE_C, info->street_c, info->street_c_len, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->city != NULL && info->city[0] != '\0' && info->city_len != 0) {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, prefixes, 2,
                                          FKEY_CITY, info->city, info->city_len, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->province != NULL && info->province[0] != '\0' && info->province_len != 0) {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, prefixes, 2,
                                          FKEY_PROVINCE, info->province, info->province_len, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (info->postal_code != NULL && info->postal_code[0] != '\0' && info->postal_code_len != 0) {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, prefixes, 2,
                                          FKEY_POSTAL_CODE, info->postal_code, info->postal_code_len, has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (include_save == 1) {
        const char *flag_str = (save_flag == 1) ? "true" : "false";
        ret = regcam_emit_plain_field(write_pass, http_req, FKEY_SAVE, flag_str, has_field, &total);
        if (ret < 0) return ret;
    }
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_bind_account(int write_pass, int http_req, u64 *len_out,
                                                               u32 ver, const char *user, const char *pass_str, const char *cid)
{
    char enc_buf[784];
    u64 total = 0;
    int ret;

    ret = regcam_emit_plain_uint(write_pass, http_req, FKEY_VERSION, ver, 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, NULL, 0, FKEY_USERNAME,
                                      user, sceNet_lib_D8722983(user), 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, NULL, 0, FKEY_PASSWORD,
                                      pass_str, sceNet_lib_D8722983(pass_str), 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_field(write_pass, http_req, FKEY_CONSOLE_ID, cid, 1, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_start_reg(int write_pass, int http_req, u64 *len_out,
                                                            u32 ver, const char *country, const char *lang,
                                                            u32 yob, u32 mob, u32 dob, const char *cid)
{
    const char *acct_prefix[2];
    char norm_buf[16];
    u64 total = 0;
    u32 idx, len;
    int ret;

    sceKernelMemset(acct_prefix, 0, sizeof(acct_prefix));
    acct_prefix[0] = "account.";

    ret = regcam_emit_plain_uint(write_pass, http_req, FKEY_VERSION, ver, 0, &total);
    if (ret < 0) return ret;

    len = sceNet_lib_D8722983(country);
    for (idx = 0; idx < len && idx < 15; idx++)
        norm_buf[idx] = (char)sceNet_lib_C13C9307((u8)country[idx]);
    norm_buf[idx] = '\0';
    ret = regcam_emit_prefixed_raw(write_pass, http_req, acct_prefix, 1, FKEY_COUNTRY,
                                   norm_buf, sceNet_lib_D8722983(norm_buf), 1, &total);
    if (ret < 0) return ret;

    len = sceNet_lib_D8722983(lang);
    for (idx = 0; idx < len && idx < 15; idx++)
        norm_buf[idx] = (char)sceNet_lib_96EF9DA1((u8)lang[idx]);
    norm_buf[idx] = '\0';
    ret = regcam_emit_prefixed_raw(write_pass, http_req, acct_prefix, 1, FKEY_LANGUAGE,
                                   norm_buf, sceNet_lib_D8722983(norm_buf), 1, &total);
    if (ret < 0) return ret;

    ret = regcam_emit_uint_prefixed(write_pass, http_req, acct_prefix, 1, FKEY_YOB, yob, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, acct_prefix, 1, FKEY_MOB, mob, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, acct_prefix, 1, FKEY_DOB, dob, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_field(write_pass, http_req, FKEY_CONSOLE_ID, cid, 1, &total);
    if (ret < 0) return ret;

    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_set_cred(int write_pass, int http_req, u64 *len_out,
                                                           const char *login, const char *pass_str,
                                                           u32 sec_q, const char *sec_ans, u32 sec_ans_len)
{
    char enc_buf[784];
    const char *acct_prefix[2];
    u64 total = 0;
    int has_field = 0;
    int ret;

    sceKernelMemset(acct_prefix, 0, sizeof(acct_prefix));
    acct_prefix[0] = "account.";

    if (login != NULL) {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_LOGIN_NAME,
                                          login, sceNet_lib_D8722983(login), has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (pass_str != NULL) {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_PASSWORD,
                                          pass_str, sceNet_lib_D8722983(pass_str), has_field, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    ret = regcam_emit_uint_prefixed(write_pass, http_req, acct_prefix, 1, FKEY_SECURITY_QUESTION, sec_q, has_field, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_SECURITY_ANSWER,
                                      sec_ans, sec_ans_len, 1, &total);
    if (ret < 0) return ret;

    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_set_lid(int write_pass, int http_req, u64 *len_out, const char *login)
{
    char enc_buf[784];
    const char *acct_prefix[2];
    u64 total = 0;
    int ret;

    sceKernelMemset(acct_prefix, 0, sizeof(acct_prefix));
    acct_prefix[0] = "account.";
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_LOGIN_NAME,
                                      login, sceNet_lib_D8722983(login), 0, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_upd_pass(int write_pass, int http_req, u64 *len_out, const char *pass_str)
{
    char enc_buf[784];
    const char *acct_prefix[2];
    u64 total = 0;
    int ret;

    sceKernelMemset(acct_prefix, 0, sizeof(acct_prefix));
    acct_prefix[0] = "account.";
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_PASSWORD,
                                      pass_str, sceNet_lib_D8722983(pass_str), 0, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_set_oid(int write_pass, int http_req, u64 *len_out, const char *oid)
{
    char enc_buf[784];
    const char *acct_prefix[2];
    u64 total = 0;
    int ret;

    sceKernelMemset(acct_prefix, 0, sizeof(acct_prefix));
    acct_prefix[0] = "account.";
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_PS_HANDLE,
                                      oid, sceNet_lib_D8722983(oid), 0, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_set_name(int write_pass, int http_req, u64 *len_out,
                                                           const char *first, u32 first_len, const char *last, u32 last_len,
                                                           const char *first_ascii, const char *last_ascii, u32 gender)
{
    char enc_buf[784];
    const char *acct_prefix[2];
    u64 total = 0;
    int ret;

    sceKernelMemset(acct_prefix, 0, sizeof(acct_prefix));
    acct_prefix[0] = "account.";

    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_FIRST_NAME, first, first_len, 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_LAST_NAME, last, last_len, 1, &total);
    if (ret < 0) return ret;
    if (first_ascii != NULL && first_ascii[0] != '\0') {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_FIRST_NAME_ASCII,
                                          first_ascii, sceNet_lib_D8722983(first_ascii), 1, &total);
        if (ret < 0) return ret;
    }
    if (last_ascii != NULL && last_ascii[0] != '\0') {
        ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_LAST_NAME_ASCII,
                                          last_ascii, sceNet_lib_D8722983(last_ascii), 1, &total);
        if (ret < 0) return ret;
    }
    if (gender == 1U || gender == 2U) {
        const char *gstr = g_gender_strs[gender];
        ret = regcam_emit_prefixed_raw(write_pass, http_req, acct_prefix, 1, FKEY_GENDER,
                                       gstr, sceNet_lib_D8722983(gstr), 1, &total);
        if (ret < 0) return ret;
    }
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_set_opt_in(int write_pass, int http_req, u64 *len_out,
                                                             u32 scex_flag, u32 aff_flag)
{
    u64 total = 0;
    int has_field = 0;
    int ret;

    if (scex_flag != 0) {
        const char *scex_str = (scex_flag == 2) ? "true" : "false";
        ret = regcam_emit_plain_field(write_pass, http_req, FKEY_SCEX_OPTIN_FLAG, scex_str, 0, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (aff_flag != 0) {
        const char *aff_str = (aff_flag == 2) ? "true" : "false";
        ret = regcam_emit_plain_field(write_pass, http_req, FKEY_AFFILIATES_OPTIN_FLAG, aff_str, has_field, &total);
        if (ret < 0) return ret;
    }
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_auth_master(int write_pass, int http_req, u64 *len_out,
                                                              const char *login, const char *pass_str)
{
    char enc_buf[784];
    u64 total = 0;
    int ret;

    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, NULL, 0, FKEY_USERNAME,
                                      login, sceNet_lib_D8722983(login), 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, NULL, 0, FKEY_PASSWORD,
                                      pass_str, sceNet_lib_D8722983(pass_str), 1, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_set_subacct_ctrl(int write_pass, int http_req, u64 *len_out,
                                                                   const char *child_login, u32 content_rating,
                                                                   u32 chat_ctrl, u32 limit_flag, u32 limit_val)
{
    char enc_buf[784];
    const char *rating_str = (content_rating == 1) ? "true" : "false";
    const char *chat_str   = (chat_ctrl == 1) ? "true" : "false";
    const char *limit_str  = (limit_flag == 1) ? "true" : "false";
    u64 total = 0;
    int ret;

    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, NULL, 0, FKEY_SUB_LOGIN_NAME,
                                      child_login, sceNet_lib_D8722983(child_login), 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_field(write_pass, http_req, FKEY_CONTENT_RATING, rating_str, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_field(write_pass, http_req, FKEY_CHAT_CONTROL, chat_str, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_field(write_pass, http_req, FKEY_SEARCH_CONTROL, "true", 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_field(write_pass, http_req, FKEY_SPENDING_LIMIT_FLAG, limit_str, 1, &total);
    if (ret < 0) return ret;
    if (limit_flag == 1) {
        ret = regcam_emit_plain_uint(write_pass, http_req, FKEY_SPENDING_LIMIT, limit_val, 1, &total);
        if (ret < 0) return ret;
    }
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_reset_subacct_pass(int write_pass, int http_req, u64 *len_out,
                                                                     const char *child_login, const char *new_pass)
{
    char enc_buf[784];
    const char *acct_prefix[2];
    u64 total = 0;
    int ret;

    sceKernelMemset(acct_prefix, 0, sizeof(acct_prefix));
    acct_prefix[0] = "account.";

    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, NULL, 0, FKEY_SUB_LOGIN_NAME,
                                      child_login, sceNet_lib_D8722983(child_login), 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_PASSWORD,
                                      new_pass, sceNet_lib_D8722983(new_pass), 1, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_set_wal_opt(int write_pass, int http_req, u64 *len_out,
                                                              u32 auto_deposit, u32 pwd_flag)
{
    u64 total = 0;
    int has_field = 0;
    int ret;

    if (auto_deposit != 0) {
        const char *dep_str = (auto_deposit == 2) ? "true" : "false";
        ret = regcam_emit_plain_field(write_pass, http_req, FKEY_AUTO_DEPOSIT_FLAG, dep_str, 0, &total);
        if (ret < 0) return ret;
        has_field = 1;
    }
    if (pwd_flag != 0) {
        const char *pwd_str = (pwd_flag == 2) ? "true" : "false";
        ret = regcam_emit_plain_field(write_pass, http_req, FKEY_COMMERCE_PASSWORD_FLAG, pwd_str, has_field, &total);
        if (ret < 0) return ret;
    }
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_authenticate(int write_pass, int http_req, u64 *len_out,
                                                               u32 ver, const char *user, const char *pass_str, const char *cid)
{
    char enc_buf[784];
    u64 total = 0;
    int ret;

    ret = regcam_emit_plain_uint(write_pass, http_req, FKEY_VERSION, ver, 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, NULL, 0, FKEY_USERNAME,
                                      user, sceNet_lib_D8722983(user), 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, NULL, 0, FKEY_PASSWORD,
                                      pass_str, sceNet_lib_D8722983(pass_str), 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_field(write_pass, http_req, FKEY_CONSOLE_ID, cid, 1, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_get_tran_hist(int write_pass, int http_req, u64 *len_out,
                                                                const char *login, u32 from_y, u32 from_m, u32 from_d,
                                                                u32 to_y, u32 to_m, u32 to_d, s32 tz_min)
{
    char enc_buf[784];
    const char *tx_prefix[2];
    char tz_buf[16];
    u64 total = 0;
    int ret;

    sceKernelMemset(tx_prefix, 0, sizeof(tx_prefix));
    tx_prefix[0] = "transactionSearchCriteria.";

    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, tx_prefix, 1, FKEY_LOGIN_NAME,
                                      login, sceNet_lib_D8722983(login), 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, tx_prefix, 1, FKEY_FROM_YEAR, from_y, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, tx_prefix, 1, FKEY_FROM_MONTH, from_m, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, tx_prefix, 1, FKEY_FROM_DAY, from_d, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, tx_prefix, 1, FKEY_TO_YEAR, to_y, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, tx_prefix, 1, FKEY_TO_MONTH, to_m, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, tx_prefix, 1, FKEY_TO_DAY, to_d, 1, &total);
    if (ret < 0) return ret;

    sceNet_lib_CF705E46(tz_buf, "%d", tz_min);
    ret = regcam_emit_prefixed_raw(write_pass, http_req, tx_prefix, 1, FKEY_TIMEZONE,
                                   tz_buf, sceNet_lib_D8722983(tz_buf), 1, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_get_srv_detail(int write_pass, int http_req, u64 *len_out,
                                                                 const char *srv_id, u32 start_pos, u32 max_cnt)
{
    char enc_buf[784];
    u64 total = 0;
    int ret;

    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, NULL, 0, FKEY_SERVICE_ID,
                                      srv_id, sceNet_lib_D8722983(srv_id), 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_uint(write_pass, http_req, FKEY_START_POSITION, start_pos, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_uint(write_pass, http_req, FKEY_MAX_COUNT, max_cnt, 1, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_get_subscr(int write_pass, int http_req, u64 *len_out,
                                                             const char *srv_id, u32 start_pos, u32 max_cnt)
{
    char enc_buf[784];
    u64 total = 0;
    int ret;

    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, NULL, 0, FKEY_SERVICE_ID,
                                      srv_id, sceNet_lib_D8722983(srv_id), 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_uint(write_pass, http_req, FKEY_START_POSITION, start_pos, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_uint(write_pass, http_req, FKEY_MAX_COUNT, max_cnt, 1, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_verify_dob(int write_pass, int http_req, u64 *len_out,
                                                             u32 ver, const char *login, u32 yob, u32 mob, u32 dob, const char *cid)
{
    char enc_buf[784];
    const char *acct_prefix[2];
    u64 total = 0;
    int ret;

    sceKernelMemset(acct_prefix, 0, sizeof(acct_prefix));
    acct_prefix[0] = "account.";

    ret = regcam_emit_plain_uint(write_pass, http_req, FKEY_VERSION, ver, 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_LOGIN_NAME,
                                      login, sceNet_lib_D8722983(login), 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, acct_prefix, 1, FKEY_YOB, yob, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, acct_prefix, 1, FKEY_MOB, mob, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_uint_prefixed(write_pass, http_req, acct_prefix, 1, FKEY_DOB, dob, 1, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_plain_field(write_pass, http_req, FKEY_CONSOLE_ID, cid, 1, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_verify_sec_ans(int write_pass, int http_req, u64 *len_out,
                                                                 u32 sec_q, const char *sec_ans, u32 sec_ans_len)
{
    char enc_buf[784];
    const char *acct_prefix[2];
    u64 total = 0;
    int ret;

    sceKernelMemset(acct_prefix, 0, sizeof(acct_prefix));
    acct_prefix[0] = "account.";

    ret = regcam_emit_uint_prefixed(write_pass, http_req, acct_prefix, 1, FKEY_SECURITY_QUESTION, sec_q, 0, &total);
    if (ret < 0) return ret;
    ret = regcam_emit_urlenc_prefixed(enc_buf, write_pass, http_req, acct_prefix, 1, FKEY_SECURITY_ANSWER,
                                      sec_ans, sec_ans_len, 1, &total);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = total;
    return 0;
}

static __attribute__((noinline)) int regcam_build_set_account_lang(int write_pass, int http_req, u64 *len_out, const char *lang)
{
    const char *acct_prefix[2];
    char lang_buf[16];
    u32 idx = 0;
    int ret;

    sceKernelMemset(acct_prefix, 0, sizeof(acct_prefix));
    acct_prefix[0] = "account.";

    while (idx < sceNet_lib_D8722983(lang) && idx < 16U) {
        lang_buf[idx] = (char)sceNet_lib_96EF9DA1((u8)lang[idx]);
        idx++;
    }
    if (idx == 16U)
        lang_buf[15] = '\0';
    else
        lang_buf[idx] = '\0';

    ret = regcam_emit_prefixed_str(write_pass, http_req, acct_prefix, 1, FKEY_LANGUAGE, lang_buf, 0);
    if (ret < 0) return ret;
    if (write_pass != 1)
        *len_out = (u64)(s64)ret;
    return 0;
}

/* =========================================================================
 * Low-level form writers (0x009e38 .. 0x00a023)
 * ========================================================================= */

int regcam_write_prefixed_field(int http_req, const char * const *prefixes, u32 prefix_cnt,
                                u32 key_id, const void *val, u32 val_len, int prepend_amp)
{
    u32 idx;
    int ret;

    if (prepend_amp == 1) {
        ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[FKEY_AMP].str, g_form_keys[FKEY_AMP].len);
        if (ret < 0)
            return ret;
    }
    for (idx = 0; idx < prefix_cnt; idx++) {
        if (prefixes[idx] != NULL) {
            ret = sceNpCommerce2_AE614CC6(http_req, prefixes[idx], sceNet_lib_D8722983(prefixes[idx]));
            if (ret < 0)
                return ret;
        }
    }
    ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_AE614CC6(http_req, val, val_len);
}

int regcam_write_uint_field(int http_req, u32 key_id, u32 val, int prepend_amp)
{
    char num_buf[16];
    int len;
    int ret;

    if (prepend_amp == 1) {
        ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[FKEY_AMP].str, g_form_keys[FKEY_AMP].len);
        if (ret < 0)
            return ret;
    }
    ret = sceNpCommerce2_AE614CC6(http_req, g_form_keys[key_id].str, g_form_keys[key_id].len);
    if (ret < 0)
        return ret;
    len = sceNet_lib_CF705E46(num_buf, "%u", val);
    if (len >= 16)
        return REGCAM_ERR_BUF_OVERFLOW;
    return sceNpCommerce2_AE614CC6(http_req, num_buf, sceNet_lib_D8722983(num_buf));
}
