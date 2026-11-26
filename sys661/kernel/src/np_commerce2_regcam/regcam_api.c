#include "regcam_internal.h"

u32 g_regcam_init = 0;
u32 g_regcam_mgr = 0;
u32 g_quick_req_id = 0;
u32 g_quick_abort_flag = 0;

/* SDK version threshold (5.50.0.15) above which thread stack depth is checked */
#define REGCAM_STACK_CHECK_SDK_VER  0x0505000F
#define REGCAM_MIN_THREAD_STACK     4080

static inline int regcam_check_stack_and_init(void)
{
    if (sceKernelGetCompiledSdkVersion() > REGCAM_STACK_CHECK_SDK_VER) {
        if (_sceKernelCheckThreadStack() < REGCAM_MIN_THREAD_STACK)
            return REGCAM_ERR_STACK_OVERFLOW;
    }
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return 0;
}

/* =========================================================================
 * Module lifecycle & UTF-16 string validation
 * ========================================================================= */

int sceNpCommerce2RegCam_1EA0D72D(u32 field_type, const u16 *utf16_str, u32 *flags_out)
{
    char ascii_buf[256];
    u32 idx;

    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!utf16_str) | (!flags_out))
        return REGCAM_ERR_INVALID_ARG;

    *flags_out = 0;
    idx = 0;
    while (utf16_str[idx] != 0 && idx < sizeof(ascii_buf)) {
        u16 ch = utf16_str[idx];
        if (ch >= 0x80U)
            *flags_out |= 0x80U;
        ascii_buf[idx] = (char)(ch & 0xFFU);
        idx++;
    }
    if (idx == sizeof(ascii_buf))
        return REGCAM_ERR_INVALID_ARG;

    ascii_buf[idx] = '\0';
    return regcam_validate_str(field_type, ascii_buf, flags_out);
}

int sceNpCommerce2RegCam_78E3D827(void)
{
    int ret;

    if (g_regcam_init)
        return REGCAM_ERR_ALREADY_INITIALIZED;
    ret = regcam_init_mgr();
    if (ret < 0)
        return ret;
    g_regcam_init = 1;
    return 0;
}

int sceNpCommerce2RegCam_0400E86A(void)
{
    if (g_regcam_init) {
        regcam_term_mgr();
        g_regcam_init = 0;
    }
    return 0;
}

/* =========================================================================
 * Internal guarded API helpers (used by quick-registration flow)
 * ========================================================================= */

int regcam_api_destroy_req(u32 req_id)
{
    int err = regcam_check_stack_and_init();
    if (err < 0)
        return err;
    return regcam_destroy_req(req_id);
}

int regcam_api_abort_req(u32 req_id)
{
    int err = regcam_check_stack_and_init();
    if (err < 0)
        return err;
    return regcam_abort_req(req_id);
}

int regcam_api_create_del_sess_req(u32 ctx_id, u32 *req_id_out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_disp_create_del_sess(ctx_id, req_id_out);
}

int regcam_api_start_del_sess(u32 req_id)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return regcam_start_del_sess(req_id);
}

int regcam_api_recv_del_sess(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return regcam_recv_response(req_id, buf, buf_size, recv_size_out);
}

int regcam_api_create_get_prov_req(u32 ctx_id, u32 *req_id_out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_disp_create_get_prov(ctx_id, req_id_out);
}

int regcam_api_start_get_prov(u32 req_id)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return regcam_start_get_prov(req_id);
}

int regcam_api_recv_get_prov(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return regcam_recv_response(req_id, buf, buf_size, recv_size_out);
}

int regcam_api_init_prov_result(u32 *out, const u8 *buf, u32 buf_size)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!buf) | (!buf_size))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_tlv_init_prov_result(out, buf, buf_size);
}

int regcam_api_get_prov_info(const u32 *parent, u32 index, u32 *out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_tlv_parse_prov_wrap(parent, index, out);
}

int regcam_api_get_city_info(const u32 *parent, u32 index, u32 *out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_tlv_parse_city_info(parent, index, out);
}

int regcam_api_create_reg_cc_req(u32 ctx_id, u32 *req_id_out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!req_id_out)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_disp_create_reg_cc(ctx_id, req_id_out);
}

int regcam_api_start_reg_cc(u32 req_id, const RegCamBillingInfo *info)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!info)
        return REGCAM_ERR_INVALID_ARG;
    if (!info->street_a_len)
        return REGCAM_ERR_MISSING_STREET;
    return regcam_start_reg_cc(req_id, info);
}

int regcam_api_recv_reg_cc(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return regcam_recv_response(req_id, buf, buf_size, recv_size_out);
}

/* =========================================================================
 * Exported Commerce2 RegCam request/response API wrappers
 * ========================================================================= */

#define DEFINE_CREATE_API(fn_name, disp_fn) \
    int fn_name(u32 ctx_id, u32 *req_id_out) { \
        if (!g_regcam_init) return REGCAM_ERR_NOT_INITIALIZED; \
        if (!req_id_out) return REGCAM_ERR_INVALID_ARG; \
        return disp_fn(ctx_id, req_id_out); \
    }

#define DEFINE_START0_API(fn_name, start_fn) \
    int fn_name(u32 req_id) { \
        if (!g_regcam_init) return REGCAM_ERR_NOT_INITIALIZED; \
        return start_fn(req_id); \
    }

#define DEFINE_RECV_API(fn_name) \
    int fn_name(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out) { \
        if (!g_regcam_init) return REGCAM_ERR_NOT_INITIALIZED; \
        return regcam_recv_response(req_id, buf, buf_size, recv_size_out); \
    }

#define DEFINE_TLV_ROOT_API(fn_name, parser_fn) \
    int fn_name(u32 *out, const u8 *buf, u32 buf_size) { \
        if (!g_regcam_init) return REGCAM_ERR_NOT_INITIALIZED; \
        if ((!buf) | (!buf_size)) return REGCAM_ERR_INVALID_ARG; \
        return parser_fn(out, buf, buf_size); \
    }

#define DEFINE_TLV_ITEM_API(fn_name, parser_fn) \
    int fn_name(const u32 *parent, u32 index, u32 *out) { \
        if (!g_regcam_init) return REGCAM_ERR_NOT_INITIALIZED; \
        if ((!parent) | (!out)) return REGCAM_ERR_INVALID_ARG; \
        return parser_fn(parent, index, out); \
    }

#define DEFINE_TLV_CHILD_API(fn_name, parser_fn) \
    int fn_name(const u32 *parent, u32 *out) { \
        if (!g_regcam_init) return REGCAM_ERR_NOT_INITIALIZED; \
        if ((!parent) | (!out)) return REGCAM_ERR_INVALID_ARG; \
        return parser_fn(parent, out); \
    }

/* GetCountries */
DEFINE_CREATE_API(sceNpCommerce2RegCam_1E1C2D9D, regcam_disp_create_get_countries)
DEFINE_START0_API(sceNpCommerce2RegCam_CA669133, regcam_start_get_countries)
DEFINE_RECV_API(sceNpCommerce2RegCam_D1C2D16F)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_49967C41, regcam_tlv_init_countries_result)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_38BE19B7, regcam_tlv_parse_country_info)

/* BindAccount */
DEFINE_CREATE_API(sceNpCommerce2RegCam_ECB37716, regcam_disp_create_bind_account)
int sceNpCommerce2RegCam_F692E73E(u32 req_id, const char *user, const char *pass)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!user) | (!pass))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_bind_account(req_id, user, pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_CB9EC34C)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_DEA9E110, regcam_tlv_init_bind_root)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_3E015183, regcam_tlv_parse_account_info)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_43A10942, regcam_tlv_parse_bind_result)

/* StartRegistration */
DEFINE_CREATE_API(sceNpCommerce2RegCam_06B1824F, regcam_disp_create_start_reg)
int sceNpCommerce2RegCam_7A315723(u32 req_id, const char *country, const char *lang,
                                  u32 yob, u32 mob, u32 dob)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!country || !lang || !yob || !mob || !dob)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_start_reg(req_id, country, lang, yob, mob, dob);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_4B1A9E4E)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_856C52E9, regcam_tlv_init_start_reg_result)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_BD48E118, regcam_tlv_parse_reg_rules)

/* GetSecurityQuestions */
DEFINE_CREATE_API(sceNpCommerce2RegCam_DB478D24, regcam_disp_create_get_sec_q)
DEFINE_START0_API(sceNpCommerce2RegCam_5F45DF28, regcam_start_get_sec_q)
DEFINE_RECV_API(sceNpCommerce2RegCam_6EA71511)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_EC918C7E, regcam_tlv_init_sec_q_result)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_FAC30D4E, regcam_tlv_parse_sec_q_nth)

/* SetCredentials */
DEFINE_CREATE_API(sceNpCommerce2RegCam_B56A1200, regcam_disp_create_set_cred)
int sceNpCommerce2RegCam_13E0AD53(u32 req_id, const char *login, const char *pass,
                                  u32 sec_q, const char *sec_ans, u32 sec_ans_len)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!sec_ans) | (!sec_ans_len))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_set_cred(req_id, login, pass, sec_q, sec_ans, sec_ans_len);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_F5E2A7B0)

/* SetLoginId */
DEFINE_CREATE_API(sceNpCommerce2RegCam_2054F881, regcam_disp_create_set_lid)
int sceNpCommerce2RegCam_9018A33F(u32 req_id, const char *login)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!login)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_set_lid(req_id, login);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_F3D3D32F)

/* UpdatePassword */
DEFINE_CREATE_API(sceNpCommerce2RegCam_35685D12, regcam_disp_create_upd_pass)
int sceNpCommerce2RegCam_90E218A2(u32 req_id, const char *pass)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!pass)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_upd_pass(req_id, pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_FA305116)

/* VerifyOnlineId / SetOnlineId */
DEFINE_CREATE_API(sceNpCommerce2RegCam_5EE50062, regcam_disp_create_verify_oid)
int sceNpCommerce2RegCam_1A0507A7(u32 req_id, const char *oid)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!oid)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_oid(req_id, oid);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_D059973F)

DEFINE_CREATE_API(sceNpCommerce2RegCam_6F0783DA, regcam_disp_create_set_oid)
int sceNpCommerce2RegCam_CD9F9F24(u32 req_id, const char *oid)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!oid)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_oid(req_id, oid);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_10E44562)

/* GetName / SetName */
DEFINE_CREATE_API(sceNpCommerce2RegCam_E68AD08A, regcam_disp_create_get_name)
DEFINE_START0_API(sceNpCommerce2RegCam_DC403471, regcam_start_get_name)
DEFINE_RECV_API(sceNpCommerce2RegCam_54E7925E)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_9AFBDA97, regcam_tlv_init_get_name_result)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_29CB32DA, regcam_tlv_parse_name_info)

DEFINE_CREATE_API(sceNpCommerce2RegCam_B4CC8AE6, regcam_disp_create_set_name)
int sceNpCommerce2RegCam_EE818631(u32 req_id, const char *first, u32 first_len,
                                  const char *last, u32 last_len,
                                  const char *first_ascii, const char *last_ascii, u32 gender)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!first || !first_len || !last || !last_len)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_set_name(req_id, first, first_len, last, last_len,
                                 first_ascii, last_ascii, gender);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_F6415A7A)

/* SetResidentialAddress */
DEFINE_CREATE_API(sceNpCommerce2RegCam_3328C751, regcam_disp_create_set_res_addr)
int sceNpCommerce2RegCam_461DD73D(u32 req_id, const RegCamBillingInfo *info)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!info)
        return REGCAM_ERR_INVALID_ARG;
    if (!info->street_a_len)
        return REGCAM_ERR_MISSING_STREET;
    return regcam_start_set_res_addr(req_id, info);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_28541A7A)

/* SetRrn */
DEFINE_CREATE_API(sceNpCommerce2RegCam_DF407CA3, regcam_disp_create_set_rrn)
int sceNpCommerce2RegCam_0EE4DB04(u32 req_id, const char *rrn_front, const char *rrn_back)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!rrn_front) | (!rrn_back))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_set_rrn(req_id, rrn_front, rrn_back);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_993C822E)

/* GetOptIn / SetOptIn */
DEFINE_CREATE_API(sceNpCommerce2RegCam_7D3AD059, regcam_disp_create_get_opt_in)
DEFINE_START0_API(sceNpCommerce2RegCam_505C1C2B, regcam_start_get_opt_in)
DEFINE_RECV_API(sceNpCommerce2RegCam_544545B6)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_F8BA328E, regcam_tlv_init_opt_in_result)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_6BFAB2ED, regcam_tlv_parse_opt_in)

DEFINE_CREATE_API(sceNpCommerce2RegCam_D99AEC05, regcam_disp_create_set_opt_in)
int sceNpCommerce2RegCam_726FF626(u32 req_id, u32 scex_flag, u32 aff_flag)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return regcam_start_set_opt_in(req_id, scex_flag, aff_flag);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_FA13A517)

/* AuthMaster */
DEFINE_CREATE_API(sceNpCommerce2RegCam_289E304C, regcam_disp_create_auth_master)
int sceNpCommerce2RegCam_BD9DD225(u32 req_id, const char *login, const char *pass)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!login) | (!pass))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_auth_master(req_id, login, pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_B816C674)

/* SetSubAccountControl */
DEFINE_CREATE_API(sceNpCommerce2RegCam_AA077B50, regcam_disp_create_set_subacct_ctrl)
int sceNpCommerce2RegCam_C1D499F5(u32 req_id, const char *child_login, u32 content_rating,
                                  u32 chat_ctrl, u32 limit_flag, u32 limit_val)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!child_login)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_set_subacct_ctrl(req_id, child_login, content_rating,
                                         chat_ctrl, limit_flag, limit_val);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_CE4F0A96)

/* ResetSubAccountPassword */
DEFINE_CREATE_API(sceNpCommerce2RegCam_32016B1A, regcam_disp_create_reset_subacct_pass)
int sceNpCommerce2RegCam_68315782(u32 req_id, const char *child_login, const char *new_pass)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!child_login) | (!new_pass))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_reset_subacct_pass(req_id, child_login, new_pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_E8B01984)

/* GetWalletOptions / SetWalletOptions */
DEFINE_CREATE_API(sceNpCommerce2RegCam_1C3B41DA, regcam_disp_create_get_wal_opt)
DEFINE_START0_API(sceNpCommerce2RegCam_4D7EEABD, regcam_start_get_wal_opt)
DEFINE_RECV_API(sceNpCommerce2RegCam_3EFA6523)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_14626EC6, regcam_tlv_init_wal_opt_result)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_37F3D00E, regcam_tlv_parse_wal_opt)

DEFINE_CREATE_API(sceNpCommerce2RegCam_3B9334BA, regcam_disp_create_set_wal_opt)
int sceNpCommerce2RegCam_EF397E50(u32 req_id, u32 auto_deposit, u32 pwd_flag)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return regcam_start_set_wal_opt(req_id, auto_deposit, pwd_flag);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_7AB45618)

/* SubmitRegistration */
DEFINE_CREATE_API(sceNpCommerce2RegCam_64D92E9C, regcam_disp_create_submit_reg)
DEFINE_START0_API(sceNpCommerce2RegCam_130756C8, regcam_start_submit_reg)
DEFINE_RECV_API(sceNpCommerce2RegCam_A2A91596)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_C0CAEE74, regcam_tlv_init_submit_reg_result)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_F5BCBE33, regcam_tlv_parse_account_info)

/* Authenticate */
DEFINE_CREATE_API(sceNpCommerce2RegCam_B140C5D7, regcam_disp_create_authenticate)
int sceNpCommerce2RegCam_19679B00(u32 req_id, int use_explicit, const char *user, const char *pass)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (use_explicit == 1 && ((!user) | (!pass)))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_authenticate(req_id, use_explicit, user, pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_9C9E0B84)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_BBAE6864, regcam_tlv_init_auth_root)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_43ABA1B2, regcam_tlv_parse_account_info)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_6CDE3C3A, regcam_tlv_parse_reg_rules)
int sceNpCommerce2RegCam_DA4D6AC6(const u32 *parent, u32 index, RegCamCurrencyInfo *out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_tlv_parse_currency_wrap(parent, index, out);
}
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_C8124194, regcam_tlv_parse_subacct_info)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_17843035, regcam_tlv_parse_spend_limit)

/* RemovePaymentMethod */
DEFINE_CREATE_API(sceNpCommerce2RegCam_B0EEB010, regcam_disp_create_rem_pay)
DEFINE_START0_API(sceNpCommerce2RegCam_4328E348, regcam_start_rem_pay)
DEFINE_RECV_API(sceNpCommerce2RegCam_8061F181)

/* GetTransactionHistory */
DEFINE_CREATE_API(sceNpCommerce2RegCam_B4EE6FD8, regcam_disp_create_get_tran_hist)
int sceNpCommerce2RegCam_53BB97C3(u32 req_id, int use_explicit, const char *login,
                                  u32 from_y, u32 from_m, u32 from_d,
                                  u32 to_y, u32 to_m, u32 to_d, s32 tz_min)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((use_explicit == 1 && !login) ||
        !from_y || !from_m || !from_d || !to_y || !to_m || !to_d)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_get_tran_hist(req_id, use_explicit, login,
                                      from_y, from_m, from_d,
                                      to_y, to_m, to_d, tz_min);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_8BBBCF11)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_F8462A1F, regcam_tlv_init_tran_hist_result)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_ECFD766F, regcam_tlv_parse_tran_hist_item)

/* GetTransactionDetail */
DEFINE_CREATE_API(sceNpCommerce2RegCam_D0320D95, regcam_disp_create_get_tran_detail)
int sceNpCommerce2RegCam_F66BE4F0(u32 req_id, u64 tx_id)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return regcam_start_get_tran_detail(req_id, tx_id);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_CA1F31C0)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_3C07C95A, regcam_tlv_init_tran_detail_result)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_9A5600B9, regcam_tlv_parse_tran_detail)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_3D77FBF8, regcam_tlv_parse_tran_order)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_C48D59AE, regcam_tlv_parse_tran_item)
int sceNpCommerce2RegCam_99D0CFA1(const u32 *parent, u32 *out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_tlv_parse_price_info(parent, out, 2);
}
int sceNpCommerce2RegCam_10FAC26F(const u32 *parent, u32 *out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!parent) | (!out))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_tlv_parse_price_info(parent, out, 3);
}
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_0AA280E7, regcam_tlv_parse_eula_info_wrap)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_BA099CBE, regcam_tlv_parse_eula_ext_wrap)

/* GetServices */
DEFINE_CREATE_API(sceNpCommerce2RegCam_90F2F814, regcam_disp_create_get_srvs)
int sceNpCommerce2RegCam_290B7E82(u32 req_id, u32 start_pos, u32 max_cnt)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return regcam_start_get_srvs(req_id, start_pos, max_cnt);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_0F0D10D7)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_0933A995, regcam_tlv_init_srvs_result)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_D8F02D94, regcam_tlv_parse_service_info)

/* GetServiceDetail */
DEFINE_CREATE_API(sceNpCommerce2RegCam_F59AC300, regcam_disp_create_get_srv_detail)
int sceNpCommerce2RegCam_4EAA3049(u32 req_id, const char *srv_id, u32 start_pos, u32 max_cnt)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!srv_id)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_get_srv_detail(req_id, srv_id, start_pos, max_cnt);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_7C50D970)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_A9925EBC, regcam_tlv_init_srv_detail_result)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_6E001343, regcam_tlv_parse_entitlement)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_63FAD5E2, regcam_tlv_parse_reward)

/* GetSubscriptions */
DEFINE_CREATE_API(sceNpCommerce2RegCam_6EFF8564, regcam_disp_create_get_subscr)
int sceNpCommerce2RegCam_56AB8C1F(u32 req_id, const char *srv_id, u32 start_pos, u32 max_cnt)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!srv_id)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_get_subscr(req_id, srv_id, start_pos, max_cnt);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_46A43687)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_69F468DB, regcam_tlv_init_subscr_result)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_E2AD8DB9, regcam_tlv_parse_subscription)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_04AF3D66, regcam_tlv_parse_subscr_plan)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_552B0DF7, regcam_tlv_parse_reward)

/* ChangeSubscription / CancelSubscription */
DEFINE_CREATE_API(sceNpCommerce2RegCam_F373282E, regcam_disp_create_change_subscr)
int sceNpCommerce2RegCam_86966193(u32 req_id, const char *sku_id)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!sku_id)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_change_subscr(req_id, sku_id);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_427495CA)

DEFINE_CREATE_API(sceNpCommerce2RegCam_4C191E40, regcam_disp_create_cancel_subscr)
int sceNpCommerce2RegCam_524A8607(u32 req_id, const char *sku_id)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!sku_id)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_cancel_subscr(req_id, sku_id);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_A2337870)

/* AcceptEula */
DEFINE_CREATE_API(sceNpCommerce2RegCam_69817297, regcam_disp_create_acpt_eula)
DEFINE_START0_API(sceNpCommerce2RegCam_BCFD9CC5, regcam_start_rem_pay)
DEFINE_RECV_API(sceNpCommerce2RegCam_E256E232)

/* VerifyDob */
DEFINE_CREATE_API(sceNpCommerce2RegCam_4FDB9B84, regcam_disp_create_verify_dob)
int sceNpCommerce2RegCam_FA10DDCA(u32 req_id, const char *login, u32 yob, u32 mob, u32 dob)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!login || !yob || !mob || !dob)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_verify_dob(req_id, login, yob, mob, dob);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_48D75015)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_00E61B9F, regcam_tlv_init_verify_dob_result)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_BE4E06A6, regcam_tlv_parse_sec_q_first)

/* VerifySecurityAnswer */
DEFINE_CREATE_API(sceNpCommerce2RegCam_F6BBF9FC, regcam_disp_create_verify_sec_ans)
int sceNpCommerce2RegCam_1CB4F2B3(u32 req_id, u32 sec_q, const char *sec_ans, u32 sec_ans_len)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!sec_ans) | (!sec_ans_len))
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_verify_sec_ans(req_id, sec_q, sec_ans, sec_ans_len);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_4D6A55F2)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_F1711F2E, regcam_tlv_init_verify_sec_ans_result)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_EB7C8574, regcam_tlv_parse_account_info)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_51D546D5, regcam_tlv_parse_reg_rules)

/* SendPasswordResetMail */
DEFINE_CREATE_API(sceNpCommerce2RegCam_BA6DA16C, regcam_disp_create_send_pass_reset_mail)
DEFINE_START0_API(sceNpCommerce2RegCam_1BC6B384, regcam_start_send_mail)
DEFINE_RECV_API(sceNpCommerce2RegCam_748B85C7)

/* VerifyMasterBilling */
DEFINE_CREATE_API(sceNpCommerce2RegCam_01B124E9, regcam_disp_create_verify_master_bill)
int sceNpCommerce2RegCam_A4BD8ECD(u32 req_id, const RegCamBillingInfo *info, u32 save_flag)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!info)
        return REGCAM_ERR_INVALID_ARG;
    if (!info->street_a_len)
        return REGCAM_ERR_MISSING_STREET;
    return regcam_start_verify_master_bill(req_id, info, save_flag);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_EDBDE24A)

/* ResetPassword */
DEFINE_CREATE_API(sceNpCommerce2RegCam_B9AA1D92, regcam_disp_create_reset_pass)
int sceNpCommerce2RegCam_FB32EF43(u32 req_id, const char *pass)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!pass)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_upd_pass(req_id, pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_46BA74F3)

/* GetMasterBilling */
DEFINE_CREATE_API(sceNpCommerce2RegCam_1D971360, regcam_disp_create_get_master_bill)
int sceNpCommerce2RegCam_DA23FE50(u32 req_id)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return regcam_start_get_master_bill(req_id, 0);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_AF8404F6)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_9D44ED38, regcam_tlv_init_billing_root)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_999908AF, regcam_tlv_parse_bill_prov_wrap)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_3E8C6A5C, regcam_tlv_parse_bill_pay_wrap)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_4F2E8993, regcam_tlv_parse_bill_card_wrap)

/* SendConsentMail */
DEFINE_CREATE_API(sceNpCommerce2RegCam_1E8D3613, regcam_disp_create_send_consent_mail)
DEFINE_START0_API(sceNpCommerce2RegCam_9FCC267C, regcam_start_send_mail)
DEFINE_RECV_API(sceNpCommerce2RegCam_DE2BB689)

/* GetMasterResidentialAddress */
DEFINE_CREATE_API(sceNpCommerce2RegCam_BD83257C, regcam_disp_create_get_master_res_addr)
DEFINE_START0_API(sceNpCommerce2RegCam_4937BAA2, regcam_start_get_master_res_addr)
DEFINE_RECV_API(sceNpCommerce2RegCam_0F42FC76)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_CD307E5C, regcam_tlv_init_address_root)

/* VerifyBilling */
DEFINE_CREATE_API(sceNpCommerce2RegCam_6925F26A, regcam_disp_create_verify_bill)
int sceNpCommerce2RegCam_249407CF(u32 req_id, const RegCamBillingInfo *info)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!info)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_verify_bill(req_id, info);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_BF007C60)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_2FC38094, regcam_tlv_init_verify_bill_result)
DEFINE_TLV_CHILD_API(sceNpCommerce2RegCam_28907860, regcam_tlv_parse_account_info)

/* SetAccountLanguage */
DEFINE_CREATE_API(sceNpCommerce2RegCam_D6C671FA, regcam_disp_create_set_account_lang)
int sceNpCommerce2RegCam_F8FBAE67(u32 req_id, const char *lang)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!lang)
        return REGCAM_ERR_INVALID_ARG;
    return regcam_start_set_account_lang(req_id, lang);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_E68CE3E1)

int regcam_api_set_svc_type(u32 ctx_id, u32 svc_type)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (svc_type < 1U || svc_type >= 9U)
        return REGCAM_ERR_INVALID_SERVICE;
    return regcam_set_svc_type(ctx_id, svc_type);
}

int sceNpCommerce2RegCam_5F271C1B(u32 field_type, const char *str, u32 *flags_out)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if ((!str) | (!flags_out))
        return REGCAM_ERR_INVALID_ARG;
    *flags_out = 0;
    return regcam_validate_str(field_type, str, flags_out);
}
