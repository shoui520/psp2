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

static inline int regcam_check_ready(void)
{
    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    return 0;
}

/* =========================================================================
 * Module lifecycle & string validation
 * ========================================================================= */

int sceNpCommerce2RegCam_1EA0D72D(u32 field_type, const u16 *utf16_str, u32 *flags_out)
{
    char ascii_buf[256];
    u32 idx;

    if (!g_regcam_init)
        return REGCAM_ERR_NOT_INITIALIZED;
    if (!utf16_str || !flags_out)
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
    if (idx >= sizeof(ascii_buf))
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

int sceNpCommerce2RegCam_5F271C1B(u32 field_type, const char *str, u32 *flags_out)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_validate_str(field_type, str, flags_out);
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
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_create_req(ctx_id, REGCAM_CMD_DEL_SESS, req_id_out);
}

int regcam_api_start_del_sess(u32 req_id)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_empty(req_id, 1);
}

int regcam_api_recv_del_sess(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_recv_response(req_id, buf, buf_size, recv_size_out);
}

int regcam_api_create_get_prov_req(u32 ctx_id, u32 *req_id_out)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_create_req(ctx_id, REGCAM_CMD_GET_PROV, req_id_out);
}

int regcam_api_start_get_prov(u32 req_id)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_empty(req_id, 0);
}

int regcam_api_recv_get_prov(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_recv_response(req_id, buf, buf_size, recv_size_out);
}

int regcam_api_init_prov_result(u32 *out, const u8 *buf, u32 buf_size)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_tlv_parse_counted_root(out, buf, buf_size, 55);
}

int regcam_api_get_prov_info(const u32 *parent, u32 index, u32 *out)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_tlv_parse_province_info(parent, index, out);
}

int regcam_api_get_city_info(const u32 *parent, u32 index, u32 *out)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_tlv_parse_city_info(parent, index, out);
}

int regcam_api_create_reg_cc_req(u32 ctx_id, u32 *req_id_out)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_create_req(ctx_id, REGCAM_CMD_REG_CC, req_id_out);
}

int regcam_api_start_reg_cc(u32 req_id, const RegCamBillingInfo *info)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_billing(req_id, 0, info, 0, 0);
}

int regcam_api_recv_reg_cc(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_recv_response(req_id, buf, buf_size, recv_size_out);
}

int regcam_api_set_svc_type(u32 ctx_id, u32 svc_type)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_set_svc_type(ctx_id, svc_type);
}

/* =========================================================================
 * Exported Commerce2 RegCam request/response API wrappers
 * ========================================================================= */

#define DEFINE_CREATE_API(fn_name, cmd_const) \
    int fn_name(u32 ctx_id, u32 *req_id_out) { \
        int err = regcam_check_ready(); \
        if (err < 0) return err; \
        if (!req_id_out) return REGCAM_ERR_INVALID_ARG; \
        return regcam_create_req(ctx_id, (cmd_const), req_id_out); \
    }

#define DEFINE_RECV_API(fn_name) \
    int fn_name(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out) { \
        int err = regcam_check_ready(); \
        if (err < 0) return err; \
        return regcam_recv_response(req_id, buf, buf_size, recv_size_out); \
    }

#define DEFINE_START_EMPTY_API(fn_name, clear_sess) \
    int fn_name(u32 req_id) { \
        int err = regcam_check_ready(); \
        if (err < 0) return err; \
        return regcam_start_empty(req_id, (clear_sess)); \
    }

#define DEFINE_TLV_ROOT_API(fn_name, parser_fn) \
    int fn_name(u32 *out, const u8 *buf, u32 buf_size) { \
        int err = regcam_check_ready(); \
        if (err < 0) return err; \
        return parser_fn(out, buf, buf_size); \
    }

#define DEFINE_TLV_COUNTED_API(fn_name, tag_id) \
    int fn_name(u32 *out, const u8 *buf, u32 buf_size) { \
        int err = regcam_check_ready(); \
        if (err < 0) return err; \
        return regcam_tlv_parse_counted_root(out, buf, buf_size, (tag_id)); \
    }

#define DEFINE_TLV_PAGED_API(fn_name, tag_id, min_len) \
    int fn_name(u32 *out, const u8 *buf, u32 buf_size) { \
        int err = regcam_check_ready(); \
        if (err < 0) return err; \
        return regcam_tlv_parse_paged_root(out, buf, buf_size, (tag_id), (min_len)); \
    }

#define DEFINE_TLV_ITEM_API(fn_name, parser_fn) \
    int fn_name(const u32 *parent, u32 index, u32 *out) { \
        int err = regcam_check_ready(); \
        if (err < 0) return err; \
        return parser_fn(parent, index, out); \
    }

#define DEFINE_TLV_SUB_API(fn_name, parser_fn) \
    int fn_name(const u32 *parent, u32 *out) { \
        int err = regcam_check_ready(); \
        if (err < 0) return err; \
        return parser_fn(parent, out); \
    }

/* GetCountries */
DEFINE_CREATE_API(sceNpCommerce2RegCam_1E1C2D9D, REGCAM_CMD_GET_COUNTRIES)
int sceNpCommerce2RegCam_CA669133(u32 req_id)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_get_countries(req_id);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_D1C2D16F)
DEFINE_TLV_COUNTED_API(sceNpCommerce2RegCam_49967C41, 91)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_38BE19B7, regcam_tlv_parse_country_info)

/* BindAccount */
DEFINE_CREATE_API(sceNpCommerce2RegCam_ECB37716, REGCAM_CMD_BIND_ACCOUNT)
int sceNpCommerce2RegCam_F692E73E(u32 req_id, const char *user, const char *pass)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_bind_account(req_id, user, pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_CB9EC34C)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_DEA9E110, regcam_tlv_parse_bind_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_3E015183, regcam_tlv_parse_account_info)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_43A10942, regcam_tlv_parse_bind_result)

/* StartRegistration */
DEFINE_CREATE_API(sceNpCommerce2RegCam_06B1824F, REGCAM_CMD_START_REG)
int sceNpCommerce2RegCam_7A315723(u32 req_id, const char *country, const char *lang,
                                  u32 yob, u32 mob, u32 dob)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_start_reg(req_id, country, lang, yob, mob, dob);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_4B1A9E4E)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_856C52E9, regcam_tlv_parse_sess_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_BD48E118, regcam_tlv_parse_reg_rules)

/* GetSecurityQuestions */
DEFINE_CREATE_API(sceNpCommerce2RegCam_DB478D24, REGCAM_CMD_GET_SEC_Q)
DEFINE_START_EMPTY_API(sceNpCommerce2RegCam_5F45DF28, 0)
DEFINE_RECV_API(sceNpCommerce2RegCam_6EA71511)
DEFINE_TLV_COUNTED_API(sceNpCommerce2RegCam_EC918C7E, 97)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_FAC30D4E, regcam_tlv_parse_sec_q_nth)

/* SetCredentials */
DEFINE_CREATE_API(sceNpCommerce2RegCam_B56A1200, REGCAM_CMD_SET_CRED)
int sceNpCommerce2RegCam_13E0AD53(u32 req_id, const char *login, const char *pass,
                                  u32 sec_q, const char *sec_ans, u32 sec_ans_len)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_set_cred(req_id, login, pass, sec_q, sec_ans, sec_ans_len);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_F5E2A7B0)

/* SetLoginId */
DEFINE_CREATE_API(sceNpCommerce2RegCam_2054F881, REGCAM_CMD_SET_LID)
int sceNpCommerce2RegCam_9018A33F(u32 req_id, const char *login)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_set_lid(req_id, login);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_F3D3D32F)

/* UpdatePassword */
DEFINE_CREATE_API(sceNpCommerce2RegCam_35685D12, REGCAM_CMD_UPD_PASS)
int sceNpCommerce2RegCam_90E218A2(u32 req_id, const char *pass)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_upd_pass(req_id, pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_FA305116)

/* VerifyOnlineId / SetOnlineId */
DEFINE_CREATE_API(sceNpCommerce2RegCam_5EE50062, REGCAM_CMD_VERIFY_OID)
int sceNpCommerce2RegCam_1A0507A7(u32 req_id, const char *oid)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_oid(req_id, oid);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_D059973F)

DEFINE_CREATE_API(sceNpCommerce2RegCam_6F0783DA, REGCAM_CMD_SET_OID)
int sceNpCommerce2RegCam_CD9F9F24(u32 req_id, const char *oid)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_oid(req_id, oid);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_10E44562)

/* GetName / SetName */
DEFINE_CREATE_API(sceNpCommerce2RegCam_E68AD08A, REGCAM_CMD_GET_NAME)
DEFINE_START_EMPTY_API(sceNpCommerce2RegCam_DC403471, 0)
DEFINE_RECV_API(sceNpCommerce2RegCam_54E7925E)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_9AFBDA97, regcam_tlv_parse_simple_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_29CB32DA, regcam_tlv_parse_name_info)

DEFINE_CREATE_API(sceNpCommerce2RegCam_B4CC8AE6, REGCAM_CMD_SET_NAME)
int sceNpCommerce2RegCam_EE818631(u32 req_id, const char *first, u32 first_len,
                                  const char *last, u32 last_len,
                                  const char *first_ascii, const char *last_ascii, u32 gender)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_set_name(req_id, first, first_len, last, last_len,
                                 first_ascii, last_ascii, gender);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_F6415A7A)

/* SetResidentialAddress */
DEFINE_CREATE_API(sceNpCommerce2RegCam_3328C751, REGCAM_CMD_SET_RES_ADDR)
int sceNpCommerce2RegCam_461DD73D(u32 req_id, const RegCamBillingInfo *info)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_billing(req_id, 1, info, 0, 0);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_28541A7A)

/* SetRrn */
DEFINE_CREATE_API(sceNpCommerce2RegCam_DF407CA3, REGCAM_CMD_SET_RRN)
int sceNpCommerce2RegCam_0EE4DB04(u32 req_id, const char *rrn, u32 rrn_len)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_set_rrn(req_id, rrn, rrn_len);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_993C822E)

/* GetOptIn / SetOptIn */
DEFINE_CREATE_API(sceNpCommerce2RegCam_7D3AD059, REGCAM_CMD_GET_OPT_IN)
DEFINE_START_EMPTY_API(sceNpCommerce2RegCam_505C1C2B, 0)
DEFINE_RECV_API(sceNpCommerce2RegCam_544545B6)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_F8BA328E, regcam_tlv_parse_simple_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_6BFAB2ED, regcam_tlv_parse_opt_in)

DEFINE_CREATE_API(sceNpCommerce2RegCam_D99AEC05, REGCAM_CMD_SET_OPT_IN)
int sceNpCommerce2RegCam_726FF626(u32 req_id, u32 scex_flag, u32 aff_flag)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_set_opt_in(req_id, scex_flag, aff_flag);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_FA13A517)

/* AuthMaster */
DEFINE_CREATE_API(sceNpCommerce2RegCam_289E304C, REGCAM_CMD_AUTH_MASTER)
int sceNpCommerce2RegCam_BD9DD225(u32 req_id, const char *login, const char *pass)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_auth_master(req_id, login, pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_B816C674)

/* SetSubAccountControl */
DEFINE_CREATE_API(sceNpCommerce2RegCam_AA077B50, REGCAM_CMD_SET_SUB_CTRL)
int sceNpCommerce2RegCam_C1D499F5(u32 req_id, const char *sub_login, u32 content_rating,
                                  u32 chat_ctrl, u32 search_ctrl, u32 limit_flag, u32 limit_val)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_set_sub_ctrl(req_id, sub_login, content_rating,
                                     chat_ctrl, search_ctrl, limit_flag, limit_val);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_CE4F0A96)

/* ResetSubAccountPassword */
DEFINE_CREATE_API(sceNpCommerce2RegCam_32016B1A, REGCAM_CMD_RESET_SUB_PASS)
int sceNpCommerce2RegCam_68315782(u32 req_id, const char *sub_login, const char *new_pass)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_reset_sub_pass(req_id, sub_login, new_pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_E8B01984)

/* GetWalletOptions / SetWalletOptions */
DEFINE_CREATE_API(sceNpCommerce2RegCam_1C3B41DA, REGCAM_CMD_GET_WAL_OPT)
DEFINE_START_EMPTY_API(sceNpCommerce2RegCam_4D7EEABD, 0)
DEFINE_RECV_API(sceNpCommerce2RegCam_3EFA6523)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_14626EC6, regcam_tlv_parse_simple_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_37F3D00E, regcam_tlv_parse_wal_opt)

DEFINE_CREATE_API(sceNpCommerce2RegCam_3B9334BA, REGCAM_CMD_SET_WAL_OPT)
int sceNpCommerce2RegCam_EF397E50(u32 req_id, u32 auto_deposit, u32 pwd_flag)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_set_wal_opt(req_id, auto_deposit, pwd_flag);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_7AB45618)

/* SubmitRegistration */
DEFINE_CREATE_API(sceNpCommerce2RegCam_64D92E9C, REGCAM_CMD_SUBMIT_REG)
DEFINE_START_EMPTY_API(sceNpCommerce2RegCam_130756C8, 0)
DEFINE_RECV_API(sceNpCommerce2RegCam_A2A91596)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_C0CAEE74, regcam_tlv_parse_simple_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_F5BCBE33, regcam_tlv_parse_account_info)

/* Authenticate */
DEFINE_CREATE_API(sceNpCommerce2RegCam_B140C5D7, REGCAM_CMD_AUTHENTICATE)
int sceNpCommerce2RegCam_19679B00(u32 req_id, int use_explicit, const char *user, const char *pass)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_authenticate(req_id, use_explicit, user, pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_9C9E0B84)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_BBAE6864, regcam_tlv_parse_auth_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_43ABA1B2, regcam_tlv_parse_account_info)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_6CDE3C3A, regcam_tlv_parse_reg_rules)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_DA4D6AC6, regcam_tlv_parse_currency)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_C8124194, regcam_tlv_parse_sub_account)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_17843035, regcam_tlv_parse_spend_limit)

/* RemovePaymentMethod */
DEFINE_CREATE_API(sceNpCommerce2RegCam_B0EEB010, REGCAM_CMD_REM_PAY)
DEFINE_START_EMPTY_API(sceNpCommerce2RegCam_4328E348, 0)
DEFINE_RECV_API(sceNpCommerce2RegCam_8061F181)

/* GetTransactionHistory */
DEFINE_CREATE_API(sceNpCommerce2RegCam_B4EE6FD8, REGCAM_CMD_GET_TRAN_HIST)
int sceNpCommerce2RegCam_53BB97C3(u32 req_id, int use_explicit, const char *login,
                                  u32 from_y, u32 from_m, u32 from_d,
                                  u32 to_y, u32 to_m, u32 to_d, s32 tz_min)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_get_tran_hist(req_id, use_explicit, login,
                                      from_y, from_m, from_d,
                                      to_y, to_m, to_d, tz_min);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_8BBBCF11)
DEFINE_TLV_COUNTED_API(sceNpCommerce2RegCam_F8462A1F, 107)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_ECFD766F, regcam_tlv_parse_tran_hist_item)

/* GetTransactionDetail */
DEFINE_CREATE_API(sceNpCommerce2RegCam_D0320D95, REGCAM_CMD_GET_TRAN_DETAIL)
int sceNpCommerce2RegCam_F66BE4F0(u32 req_id, const char *login, const char *tx_id, s32 tz_min)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_get_tran_detail(req_id, login, tx_id, tz_min);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_CA1F31C0)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_3C07C95A, regcam_tlv_parse_simple_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_9A5600B9, regcam_tlv_parse_tran_detail)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_3D77FBF8, regcam_tlv_parse_tran_order)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_C48D59AE, regcam_tlv_parse_tran_item)
int sceNpCommerce2RegCam_99D0CFA1(const u32 *parent, u32 *out, int is_alt)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_tlv_parse_price_info(parent, out, is_alt);
}
int sceNpCommerce2RegCam_10FAC26F(const u32 *parent, u32 *out, int is_alt)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_tlv_parse_price_info(parent, out, is_alt);
}
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_0AA280E7, regcam_tlv_parse_eula_info)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_BA099CBE, regcam_tlv_parse_eula_ext)

/* GetServices */
DEFINE_CREATE_API(sceNpCommerce2RegCam_90F2F814, REGCAM_CMD_GET_SRVS)
int sceNpCommerce2RegCam_290B7E82(u32 req_id, u32 start_pos, u32 max_cnt)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_get_srvs(req_id, start_pos, max_cnt);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_0F0D10D7)
DEFINE_TLV_PAGED_API(sceNpCommerce2RegCam_0933A995, 113, 49)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_D8F02D94, regcam_tlv_parse_service_info)

/* GetServiceDetail */
DEFINE_CREATE_API(sceNpCommerce2RegCam_F59AC300, REGCAM_CMD_GET_SRV_DETAIL)
int sceNpCommerce2RegCam_4EAA3049(u32 req_id, const char *srv_id, u32 start_pos, u32 max_cnt)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_get_srv_detail(req_id, srv_id, start_pos, max_cnt);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_7C50D970)
DEFINE_TLV_PAGED_API(sceNpCommerce2RegCam_A9925EBC, 114, 81)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_6E001343, regcam_tlv_parse_entitlement)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_63FAD5E2, regcam_tlv_parse_reward)

/* GetSubscriptions */
DEFINE_CREATE_API(sceNpCommerce2RegCam_6EFF8564, REGCAM_CMD_GET_SUBSCR)
int sceNpCommerce2RegCam_56AB8C1F(u32 req_id, const char *srv_id, u32 start_pos, u32 max_cnt)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_get_subscr(req_id, srv_id, start_pos, max_cnt);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_46A43687)
DEFINE_TLV_PAGED_API(sceNpCommerce2RegCam_69F468DB, 115, 85)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_E2AD8DB9, regcam_tlv_parse_subscription)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_04AF3D66, regcam_tlv_parse_subscr_plan)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_552B0DF7, regcam_tlv_parse_reward)

/* ChangeSubscription / CancelSubscription */
DEFINE_CREATE_API(sceNpCommerce2RegCam_F373282E, REGCAM_CMD_CHANGE_SUBSCR)
int sceNpCommerce2RegCam_86966193(u32 req_id, const char *sku_id)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_sku_id(req_id, sku_id);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_427495CA)

DEFINE_CREATE_API(sceNpCommerce2RegCam_4C191E40, REGCAM_CMD_CANCEL_SUBSCR)
int sceNpCommerce2RegCam_524A8607(u32 req_id, const char *sku_id)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_sku_id(req_id, sku_id);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_A2337870)

/* AcceptEula */
DEFINE_CREATE_API(sceNpCommerce2RegCam_69817297, REGCAM_CMD_ACPT_EULA)
DEFINE_START_EMPTY_API(sceNpCommerce2RegCam_BCFD9CC5, 0)
DEFINE_RECV_API(sceNpCommerce2RegCam_E256E232)

/* VerifyDob */
DEFINE_CREATE_API(sceNpCommerce2RegCam_4FDB9B84, REGCAM_CMD_VERIFY_DOB)
int sceNpCommerce2RegCam_FA10DDCA(u32 req_id, const char *login, u32 yob, u32 mob, u32 dob)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_verify_dob(req_id, login, yob, mob, dob);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_48D75015)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_00E61B9F, regcam_tlv_parse_simple_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_BE4E06A6, regcam_tlv_parse_sec_q_first)

/* VerifySecurityAnswer */
DEFINE_CREATE_API(sceNpCommerce2RegCam_F6BBF9FC, REGCAM_CMD_VERIFY_SEC_ANS)
int sceNpCommerce2RegCam_1CB4F2B3(u32 req_id, u32 sec_q, const char *sec_ans, u32 sec_ans_len)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_verify_sec_ans(req_id, sec_q, sec_ans, sec_ans_len);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_4D6A55F2)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_F1711F2E, regcam_tlv_parse_simple_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_EB7C8574, regcam_tlv_parse_account_info)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_51D546D5, regcam_tlv_parse_reg_rules)

/* SendPasswordResetMail */
DEFINE_CREATE_API(sceNpCommerce2RegCam_BA6DA16C, REGCAM_CMD_SEND_PASS_RESET_MAIL)
DEFINE_START_EMPTY_API(sceNpCommerce2RegCam_1BC6B384, 0)
DEFINE_RECV_API(sceNpCommerce2RegCam_748B85C7)

/* VerifyMasterBilling */
DEFINE_CREATE_API(sceNpCommerce2RegCam_01B124E9, REGCAM_CMD_VERIFY_MASTER_BILL)
int sceNpCommerce2RegCam_A4BD8ECD(u32 req_id, const RegCamBillingInfo *info, u32 save_flag)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_billing(req_id, 0, info, 1, save_flag);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_EDBDE24A)

/* ResetPassword */
DEFINE_CREATE_API(sceNpCommerce2RegCam_B9AA1D92, REGCAM_CMD_RESET_PASS)
int sceNpCommerce2RegCam_FB32EF43(u32 req_id, const char *pass)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_upd_pass(req_id, pass);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_46BA74F3)

/* GetMasterBilling */
DEFINE_CREATE_API(sceNpCommerce2RegCam_1D971360, REGCAM_CMD_GET_MASTER_BILL)
int sceNpCommerce2RegCam_DA23FE50(u32 req_id)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_payment_type(req_id, 0);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_AF8404F6)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_9D44ED38, regcam_tlv_parse_billing_root)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_999908AF, regcam_tlv_parse_province_info)
DEFINE_TLV_ITEM_API(sceNpCommerce2RegCam_3E8C6A5C, regcam_tlv_parse_pay_method)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_4F2E8993, regcam_tlv_parse_card_info)

/* SendConsentMail */
DEFINE_CREATE_API(sceNpCommerce2RegCam_1E8D3613, REGCAM_CMD_SEND_CONSENT_MAIL)
DEFINE_START_EMPTY_API(sceNpCommerce2RegCam_9FCC267C, 1)
DEFINE_RECV_API(sceNpCommerce2RegCam_DE2BB689)

/* GetMasterResidentialAddress */
DEFINE_CREATE_API(sceNpCommerce2RegCam_BD83257C, REGCAM_CMD_GET_MASTER_RES_ADDR)
DEFINE_START_EMPTY_API(sceNpCommerce2RegCam_4937BAA2, 0)
DEFINE_RECV_API(sceNpCommerce2RegCam_0F42FC76)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_CD307E5C, regcam_tlv_parse_address_root)

/* VerifyBilling */
DEFINE_CREATE_API(sceNpCommerce2RegCam_6925F26A, REGCAM_CMD_VERIFY_BILL)
int sceNpCommerce2RegCam_249407CF(u32 req_id, const RegCamBillingInfo *info)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_billing(req_id, 0, info, 0, 0);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_BF007C60)
DEFINE_TLV_ROOT_API(sceNpCommerce2RegCam_2FC38094, regcam_tlv_parse_simple_root)
DEFINE_TLV_SUB_API(sceNpCommerce2RegCam_28907860, regcam_tlv_parse_account_info)

/* SetAccountLanguage */
DEFINE_CREATE_API(sceNpCommerce2RegCam_D6C671FA, REGCAM_CMD_SET_ACCOUNT_LANG)
int sceNpCommerce2RegCam_F8FBAE67(u32 req_id, const char *lang)
{
    int err = regcam_check_ready();
    if (err < 0)
        return err;
    return regcam_start_set_account_lang(req_id, lang);
}
DEFINE_RECV_API(sceNpCommerce2RegCam_E68CE3E1)
