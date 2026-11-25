#ifndef REGCAM_INTERNAL_H
#define REGCAM_INTERNAL_H

#include <stddef.h>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long long u64;
typedef signed long long s64;

/* Commerce2 / RegCam error codes */
#define REGCAM_ERR_NOT_INITIALIZED      ((int)0x80550F01)
#define REGCAM_ERR_ALREADY_INITIALIZED  ((int)0x80550F02)
#define REGCAM_ERR_INVALID_ARG          ((int)0x80550F03)
#define REGCAM_ERR_OUT_OF_MEMORY        ((int)0x80550F0B)
#define REGCAM_ERR_ABORTED              ((int)0x80550F10)
#define REGCAM_ERR_BUF_OVERFLOW         ((int)0x80550F11)
#define REGCAM_ERR_RESP_TOO_SMALL       ((int)0x80550F12)
#define REGCAM_ERR_RESP_BUF_LIMIT       ((int)0x80550F18)
#define REGCAM_ERR_INVALID_CMD          ((int)0x80550F81)
#define REGCAM_ERR_INVALID_PAYMENT      ((int)0x80550F82)
#define REGCAM_ERR_TAG_NOT_FOUND        ((int)0x80550F87)
#define REGCAM_ERR_MAINTENANCE          ((int)0x80550F93)
#define REGCAM_ERR_INVALID_SERVICE      ((int)0x80550F96)
#define REGCAM_ERR_QUICK_OVERFLOW       ((int)0x80550FD1)
#define REGCAM_ERR_QUICK_ABORTED        ((int)0x80550FD2)
#define REGCAM_ERR_STACK_OVERFLOW       ((int)0x80550004)
#define REGCAM_ERR_SERVER_BASE          ((int)0x80551000)
#define REGCAM_ERR_SERVER_GENERIC       ((int)0x80551001)
#define REGCAM_ERR_SERVER_NO_PROV       ((int)0x80551004)
#define REGCAM_ERR_SERVER_CARD_INVALID  ((int)0x8055102B)
#define REGCAM_ERR_SERVER_ADDR_INVALID  ((int)0x80551041)

/* Keep-alive timeout: 10 seconds in microseconds */
#define REGCAM_KEEPALIVE_TIMEOUT_US     10000000ULL

/* Unix epoch (1970-01-01) offset from PSP RTC epoch (0001-01-01) in microseconds */
#define REGCAM_UNIX_EPOCH_US            0x00DCBFFEFF2BC000ULL

/* Service types */
#define REGCAM_SVC_STORE                1
#define REGCAM_SVC_REG                  5
#define REGCAM_SVC_CAM                  6
#define REGCAM_SVC_UPGRADE              7
#define REGCAM_SVC_PWD                  8

/* Request command IDs (1-based index into g_endpoint_names) */
enum RegCamCmdId {
    REGCAM_CMD_CR_SESS               = 1,
    REGCAM_CMD_DEL_SESS              = 2,
    REGCAM_CMD_GET_CAT               = 3,
    REGCAM_CMD_GET_PROD              = 4,
    REGCAM_CMD_SRCH_PROD             = 5,
    REGCAM_CMD_GET_DRM               = 7,
    REGCAM_CMD_GET_PROV              = 20,
    REGCAM_CMD_REG_CC                = 23,
    REGCAM_CMD_GET_COUNTRIES         = 27,
    REGCAM_CMD_BIND_ACCOUNT          = 28,
    REGCAM_CMD_START_REG             = 29,
    REGCAM_CMD_GET_SEC_Q             = 30,
    REGCAM_CMD_SET_CRED              = 31,
    REGCAM_CMD_SET_LID               = 32,
    REGCAM_CMD_UPD_PASS              = 33,
    REGCAM_CMD_VERIFY_OID            = 34,
    REGCAM_CMD_SET_OID               = 35,
    REGCAM_CMD_GET_NAME              = 36,
    REGCAM_CMD_SET_NAME              = 37,
    REGCAM_CMD_SET_RES_ADDR          = 38,
    REGCAM_CMD_SET_RRN               = 39,
    REGCAM_CMD_GET_OPT_IN            = 40,
    REGCAM_CMD_SET_OPT_IN            = 41,
    REGCAM_CMD_AUTH_MASTER           = 42,
    REGCAM_CMD_SET_SUB_CTRL          = 44,
    REGCAM_CMD_RESET_SUB_PASS        = 45,
    REGCAM_CMD_GET_WAL_OPT           = 46,
    REGCAM_CMD_SET_WAL_OPT           = 47,
    REGCAM_CMD_SUBMIT_REG            = 48,
    REGCAM_CMD_AUTHENTICATE          = 49,
    REGCAM_CMD_REM_PAY               = 50,
    REGCAM_CMD_GET_TRAN_HIST         = 51,
    REGCAM_CMD_GET_TRAN_DETAIL       = 52,
    REGCAM_CMD_GET_SRVS              = 53,
    REGCAM_CMD_GET_SRV_DETAIL        = 54,
    REGCAM_CMD_GET_SUBSCR            = 55,
    REGCAM_CMD_CHANGE_SUBSCR         = 57,
    REGCAM_CMD_CANCEL_SUBSCR         = 58,
    REGCAM_CMD_ACPT_EULA             = 60,
    REGCAM_CMD_VERIFY_DOB            = 61,
    REGCAM_CMD_VERIFY_SEC_ANS        = 62,
    REGCAM_CMD_SEND_PASS_RESET_MAIL  = 63,
    REGCAM_CMD_VERIFY_MASTER_BILL    = 64,
    REGCAM_CMD_CR_GUEST_SESS         = 66,
    REGCAM_CMD_RESET_PASS            = 67,
    REGCAM_CMD_GET_MASTER_BILL       = 68,
    REGCAM_CMD_SEND_CONSENT_MAIL     = 69,
    REGCAM_CMD_GET_MASTER_RES_ADDR   = 70,
    REGCAM_CMD_SET_SEC_Q             = 74,
    REGCAM_CMD_SUBMIT_UPGRADE        = 75,
    REGCAM_CMD_RECOM_PRODS           = 76,
    REGCAM_CMD_VERIFY_BILL           = 77,
    REGCAM_CMD_SET_ACCOUNT_LANG      = 84,
};

/* Form key indices into g_form_keys */
enum RegCamFormKeyId {
    FKEY_AMP                    = 0,
    FKEY_COMMA                  = 1,
    FKEY_VERSION                = 2,
    FKEY_USERNAME               = 3,
    FKEY_PASSWORD               = 4,
    FKEY_CONSOLE_ID             = 5,
    FKEY_START_POSITION         = 7,
    FKEY_MAX_COUNT              = 8,
    FKEY_SKU_ID                 = 14,
    FKEY_1                      = 20,
    FKEY_2                      = 21,
    FKEY_3                      = 22,
    FKEY_PAYMENT_TYPE           = 25,
    FKEY_PAYMENT_METHOD_ID      = 26,
    FKEY_HOLDER_NAME            = 27,
    FKEY_CARD_NUMBER            = 28,
    FKEY_START_YEAR             = 29,
    FKEY_START_MONTH            = 30,
    FKEY_EXPIRE_YEAR            = 31,
    FKEY_EXPIRE_MONTH           = 32,
    FKEY_SECURITY_CODE          = 33,
    FKEY_ISSUE_NUMBER           = 34,
    FKEY_CITY                   = 35,
    FKEY_PROVINCE               = 36,
    FKEY_POSTAL_CODE            = 37,
    FKEY_COUNTRY                = 41,
    FKEY_LANGUAGE               = 42,
    FKEY_YOB                    = 43,
    FKEY_MOB                    = 44,
    FKEY_DOB                    = 45,
    FKEY_LOGIN_NAME             = 46,
    FKEY_SECURITY_QUESTION      = 47,
    FKEY_SECURITY_ANSWER        = 48,
    FKEY_PS_HANDLE              = 49,
    FKEY_FIRST_NAME             = 50,
    FKEY_LAST_NAME              = 51,
    FKEY_FIRST_NAME_ASCII       = 52,
    FKEY_LAST_NAME_ASCII        = 53,
    FKEY_GENDER                 = 54,
    FKEY_SCEX_OPTIN_FLAG        = 55,
    FKEY_AFFILIATES_OPTIN_FLAG  = 56,
    FKEY_SUB_LOGIN_NAME         = 57,
    FKEY_CONTENT_RATING         = 58,
    FKEY_CHAT_CONTROL           = 59,
    FKEY_SEARCH_CONTROL         = 60,
    FKEY_SPENDING_LIMIT_FLAG    = 61,
    FKEY_SPENDING_LIMIT         = 62,
    FKEY_AUTO_DEPOSIT_FLAG      = 63,
    FKEY_COMMERCE_PASSWORD_FLAG = 64,
    FKEY_FROM_YEAR              = 65,
    FKEY_FROM_MONTH             = 66,
    FKEY_FROM_DAY               = 67,
    FKEY_TO_YEAR                = 68,
    FKEY_TO_MONTH               = 69,
    FKEY_TO_DAY                 = 70,
    FKEY_TIMEZONE               = 71,
    FKEY_TRANSACTION_ID         = 72,
    FKEY_SERVICE_ID             = 73,
    FKEY_SAVE                   = 74,
};

typedef struct {
    u32 len;
    const char *str;
} RegCamFormKey;

/* Commerce2 context layout (shared with sceNpCommerce2) */
typedef struct {
    u8  reserved0[24];
    int http_tmpl_id;         /* +0x018 */
    char url_buf[8];          /* +0x01C (session/url header storage) */
    u8  session_buf[1024];    /* +0x024 */
    u32 session_len;          /* +0x424 (1060) */
    u32 svc_type;             /* +0x428 (1064) */
    u32 pad_42c;              /* +0x42C */
    u64 last_activity_us;     /* +0x430 (1072) */
    u32 no_size_check;        /* +0x438 (1080) */
    u32 http_status;          /* +0x43C (1084) */
    char country_code[4];     /* +0x440 (1088) */
    char lang_code[4];        /* +0x444 (1092) */
} RegCamContext;

/* Commerce2 request handle object */
typedef struct {
    u32 reserved[3];
    u32 flags;                /* +0x0C (bit 1 = aborted) */
    int http_req_id;          /* +0x10 */
    u16 cmd_id;               /* +0x14 */
    u16 pad;
} RegCamRequest;

/* Generic TLV response cursor (40-byte header common to all parsed results) */
typedef struct {
    u32 version;              /* +0x00 */
    const u8 *base;           /* +0x04 (buffer start; base+4 is empty string sentinel) */
    u32 total_size;           /* +0x08 */
    const u8 *data;           /* +0x0C (current TLV payload start) */
    u32 data_size;            /* +0x10 (current TLV payload size) */
    const u8 *body;           /* +0x14 (sub-TLV scan start) */
    u32 reserved[4];          /* +0x18..0x24 */
} RegCamTlvHeader;

/* Credit card / billing / residential address input struct */
typedef struct {
    const char *payment_method_id;
    u32 payment_method_id_len;
    const char *holder_name;
    u32 holder_name_len;
    const char *card_number;
    u32 start_year;
    u32 start_month;
    u32 expire_year;
    u32 expire_month;
    const char *security_code;
    const char *issue_number;
    const char *street1;
    u32 street1_len;
    const char *street2;
    u32 street2_len;
    const char *street3;
    u32 street3_len;
    const char *city;
    u32 city_len;
    const char *province;
    u32 province_len;
    const char *postal_code;
    u32 postal_code_len;
} RegCamBillingInfo;

static inline u32 regcam_bswap32(u32 v)
{
    return ((v & 0x000000FFU) << 24) |
           ((v & 0x0000FF00U) << 8)  |
           ((v & 0x00FF0000U) >> 8)  |
           ((v & 0xFF000000U) >> 24);
}

static inline u16 regcam_bswap16(u16 v)
{
    return (u16)((v << 8) | (v >> 8));
}

static inline s32 regcam_min_s32(s32 a, s32 b)
{
    return a < b ? a : b;
}

static inline u32 regcam_read_be32(const void *ptr)
{
    const u8 *p = (const u8 *)ptr;
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static inline u64 regcam_read_be64(const void *ptr)
{
    const u8 *p = (const u8 *)ptr;
    u64 hi = (u64)regcam_read_be32(p);
    u64 lo = (u64)regcam_read_be32(p + 4);
    return (hi << 32) | lo;
}

static inline u64 regcam_unix_ms_to_rtc(u64 ms)
{
    return ms ? (ms * 1000ULL + REGCAM_UNIX_EPOCH_US) : 0ULL;
}

/* Global tables & state */
extern const char * const g_endpoint_names[86];
extern const char * const g_service_prefixes[9];
extern const RegCamFormKey g_form_keys[81];
extern const char * const g_gender_strs[3];

extern u32 g_regcam_init;
extern u32 g_regcam_mgr;
extern u32 g_quick_req_id;
extern u32 g_quick_abort_flag;

/* Imported library functions */
extern int sceKernelGetCompiledSdkVersion(void);
extern int _sceKernelCheckThreadStack(void);
extern void *sceKernelMemset(void *dst, int ch, size_t n);
extern void *sceKernelMemcpy(void *dst, const void *src, size_t n);

extern void *sceNetIfhandle_lib_15CFE3C0(u32 size);
extern void  sceNetIfhandle_lib_76BAD213(void *ptr, u32 unused);

extern int   sceNet_lib_750F705D(int ch);
extern int   sceNet_lib_80C9F02A(const char *a, const char *b);
extern int   sceNet_lib_96EF9DA1(int ch);
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, size_t n);
extern int   sceNet_lib_B9085A96(const char *a, const char *b, size_t n);
extern char *sceNet_lib_BCBE14CF(const char *s, int ch);
extern int   sceNet_lib_C13C9307(int ch);
extern int   sceNet_lib_CF705E46(char *buf, const char *fmt, ...);
extern u32   sceNet_lib_D8722983(const char *s);

extern int sceNpCommerce2_02C307C0(u32 *mgr, u32 req_id);
extern int sceNpCommerce2_02CADA41(RegCamContext *ctx, char *email_out, char *pwd_out, int flag);
extern int sceNpCommerce2_06618C81(u32 *mgr, RegCamRequest *req, u32 ctx_id, u32 *req_id_out);
extern int sceNpCommerce2_0A6AB496(u32 *mgr, u32 ctx_id, RegCamContext **ctx_out, int lock_flag);
extern int sceNpCommerce2_157FA8AA(char *console_id_out, u32 buf_size);
extern int sceNpCommerce2_22CEC5E2(int a, int b);
extern int sceNpCommerce2_382803C8(int http_tmpl, u32 svc_type, u32 cmd_id, const char *prefix,
                                   const char *endpoint, int flag, char *url_buf,
                                   int *http_req_out, u32 len_lo, u32 len_hi);
extern int sceNpCommerce2_3F6D413C(const u8 *buf, u32 size, u32 tag, u32 min_len, u32 *count_out);
extern int sceNpCommerce2_460A1D92(int *out_ver, int kind);
extern int sceNpCommerce2_4D43B84D(int http_req, u32 *status_out);
extern int sceNpCommerce2_63B96D73(int http_req);
extern int sceNpCommerce2_66EC0829(char *dst, u32 dst_size, const char *src, u32 src_len, int flag);
extern int sceNpCommerce2_69FEA1C8(u32 *out_val);
extern int sceNpCommerce2_75DF99A9(int http_tmpl, char *url_buf);
extern int sceNpCommerce2_77BE411B(u32 *mgr);
extern int sceNpCommerce2_851E7844(u32 *mgr, u32 req_id, RegCamRequest **req_out);
extern int sceNpCommerce2_8642C532(u32 *mgr);
extern int sceNpCommerce2_ADAA054C(const void *buf, u32 buf_size, u32 body_len);
extern int sceNpCommerce2_AE614CC6(int http_req, const void *data, u32 len);
extern int sceNpCommerce2_C27FEAB1(RegCamContext *ctx, int http_req, u32 cmd_id,
                                   u8 *status_out, void *buf, u32 buf_size);
extern int sceNpCommerce2_D4C319C6(int http_req, void *buf, void *body_ptr,
                                   u32 buf_size, int *trunc_out);
extern int sceNpCommerce2_E3455E27(const u8 *buf, u32 size, u32 tag, u32 min_len, u16 *len_out);
extern int sceNpCommerce2_F0900551(RegCamContext *ctx, u32 *ver_out);
extern int sceNpCommerce2_F8D175FA(const void *buf, u32 buf_size, const char *country, const char *lang);
extern int sceNpCommerce2_FC47B63C(u64 *time_out);

/* Core lifecycle & request helpers */
int  regcam_init_mgr(void);
void regcam_term_mgr(void);
int  regcam_validate_str(u32 field_type, const char *str, u32 *flags_out);
int  regcam_destroy_req(u32 req_id);
int  regcam_abort_req(u32 req_id);
int  regcam_create_req(u32 ctx_id, u16 cmd_id, u32 *req_id_out);
int  regcam_set_svc_type(u32 ctx_id, u32 svc_type);
int  regcam_recv_response(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int  regcam_post_process_response(RegCamContext *ctx, RegCamRequest *req,
                                  const u8 *buf, u32 buf_size, u32 resp_size);

/* Internal request start dispatchers */
int regcam_start_empty(u32 req_id, int clear_session);
int regcam_start_get_countries(u32 req_id);
int regcam_start_bind_account(u32 req_id, const char *user, const char *pass);
int regcam_start_start_reg(u32 req_id, const char *country, const char *lang, u32 yob, u32 mob, u32 dob);
int regcam_start_authenticate(u32 req_id, int use_explicit, const char *user, const char *pass);
int regcam_start_get_tran_hist(u32 req_id, int use_explicit, const char *login,
                               u32 from_y, u32 from_m, u32 from_d,
                               u32 to_y, u32 to_m, u32 to_d, s32 tz_min);
int regcam_start_verify_dob(u32 req_id, const char *login, u32 yob, u32 mob, u32 dob);
int regcam_start_payment_type(u32 req_id, u32 pay_type);
int regcam_start_billing(u32 req_id, int prefix_mode, const RegCamBillingInfo *info,
                         int include_save, u32 save_flag);
int regcam_start_set_cred(u32 req_id, const char *login, const char *pass,
                          u32 sec_q, const char *sec_ans, u32 sec_ans_len);
int regcam_start_set_lid(u32 req_id, const char *login);
int regcam_start_upd_pass(u32 req_id, const char *pass);
int regcam_start_oid(u32 req_id, const char *oid);
int regcam_start_set_name(u32 req_id, const char *first, u32 first_len, const char *last, u32 last_len,
                          const char *first_ascii, const char *last_ascii, u32 gender);
int regcam_start_set_rrn(u32 req_id, const char *rrn, u32 rrn_len);
int regcam_start_set_opt_in(u32 req_id, u32 scex_flag, u32 aff_flag);
int regcam_start_auth_master(u32 req_id, const char *login, const char *pass);
int regcam_start_set_sub_ctrl(u32 req_id, const char *sub_login, u32 content_rating,
                              u32 chat_ctrl, u32 search_ctrl, u32 limit_flag, u32 limit_val);
int regcam_start_reset_sub_pass(u32 req_id, const char *sub_login, const char *new_pass);
int regcam_start_set_wal_opt(u32 req_id, u32 auto_deposit, u32 pwd_flag);
int regcam_start_get_tran_detail(u32 req_id, const char *login, const char *tx_id, s32 tz_min);
int regcam_start_get_srvs(u32 req_id, u32 start_pos, u32 max_cnt);
int regcam_start_get_srv_detail(u32 req_id, const char *srv_id, u32 start_pos, u32 max_cnt);
int regcam_start_get_subscr(u32 req_id, const char *srv_id, u32 start_pos, u32 max_cnt);
int regcam_start_sku_id(u32 req_id, const char *sku_id);
int regcam_start_verify_sec_ans(u32 req_id, u32 sec_q, const char *sec_ans, u32 sec_ans_len);
int regcam_start_set_account_lang(u32 req_id, const char *lang);

/* Shared endpoint & form-building helpers */
int regcam_resolve_endpoint(u32 svc_type, u16 cmd_id, const char **prefix_out, const char **endpoint_out);
int regcam_write_prefixed_field(int http_req, const char * const *prefixes, u32 prefix_cnt,
                                u32 key_id, const void *val, u32 val_len, int prepend_amp);
int regcam_write_uint_field(int http_req, u32 key_id, u32 val, int prepend_amp);
int regcam_emit_str_field(int write_pass, int http_req, u32 key_id, const char *val, int prepend_amp);
int regcam_emit_uint_field(int write_pass, int http_req, u32 key_id, u32 val, int prepend_amp);
int regcam_emit_prefixed_str(int write_pass, int http_req, const char * const *prefixes,
                             u32 prefix_cnt, u32 key_id, const char *val, int prepend_amp);

/* Request senders */
int regcam_send_empty(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out);
int regcam_send_sku_id(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out, const char *sku_id);
int regcam_send_payment_type(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out, u32 pay_type);
int regcam_send_billing(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        int prefix_mode, const RegCamBillingInfo *info, int include_save, u32 save_flag);
int regcam_send_version_only(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out, u32 ver);
int regcam_send_bind_account(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                             u32 ver, const char *user, const char *pass, const char *cid);
int regcam_send_start_reg(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                          u32 ver, const char *country, const char *lang, u32 yob, u32 mob, u32 dob, const char *cid);
int regcam_send_set_cred(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         const char *login, const char *pass, u32 sec_q, const char *sec_ans, u32 sec_ans_len);
int regcam_send_set_lid(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out, const char *login);
int regcam_send_upd_pass(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out, const char *pass);
int regcam_send_oid(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out, const char *oid);
int regcam_send_set_name(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         const char *first, u32 first_len, const char *last, u32 last_len,
                         const char *first_ascii, const char *last_ascii, u32 gender);
int regcam_send_set_rrn(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char *rrn, u32 rrn_len);
int regcam_send_set_opt_in(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                           u32 scex_flag, u32 aff_flag);
int regcam_send_auth_master(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                            const char *login, const char *pass);
int regcam_send_set_sub_ctrl(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                             const char *sub_login, u32 content_rating, u32 chat_ctrl, u32 search_ctrl, u32 limit_flag, u32 limit_val);
int regcam_send_reset_sub_pass(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                               const char *sub_login, const char *new_pass);
int regcam_send_set_wal_opt(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                            u32 auto_deposit, u32 pwd_flag);
int regcam_send_authenticate(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                             u32 ver, const char *user, const char *pass, const char *cid);
int regcam_send_get_tran_hist(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                              const char *login, u32 from_y, u32 from_m, u32 from_d,
                              u32 to_y, u32 to_m, u32 to_d, s32 tz_min);
int regcam_send_get_tran_detail(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                                const char *login, const char *tx_id, s32 tz_min);
int regcam_send_get_srvs(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         u32 start_pos, u32 max_cnt);
int regcam_send_get_srv_detail(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                               const char *srv_id, u32 start_pos, u32 max_cnt);
int regcam_send_get_subscr(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                           const char *srv_id, u32 start_pos, u32 max_cnt);
int regcam_send_verify_dob(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                           u32 ver, const char *login, u32 yob, u32 mob, u32 dob, const char *cid);
int regcam_send_verify_sec_ans(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                               u32 sec_q, const char *sec_ans, u32 sec_ans_len);
int regcam_send_set_account_lang(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                                 const char *lang);

/* TLV response parser helpers */
int regcam_tlv_parse_root(const u8 *buf, u32 buf_size, u32 *ver_out, u32 *body_size_out, u32 *extra_out);
int regcam_tlv_find_nth(const u8 *buf, u32 buf_size, u16 tag, u16 min_len, u32 index, u16 *len_out);
int regcam_tlv_find_nth_ex(const u8 *buf, u32 buf_size, u16 tag, u16 min_len,
                           u32 index, u16 *len_out, int extra_mode);
int regcam_tlv_parse_err_flag(const u8 *buf, u32 buf_size, u32 *flag_out);
int regcam_tlv_parse_simple_root(u32 *out, const u8 *buf, u32 buf_size);
int regcam_tlv_parse_counted_root(u32 *out, const u8 *buf, u32 buf_size, u32 tag);
int regcam_tlv_parse_paged_root(u32 *out, const u8 *buf, u32 buf_size, u16 tag, u16 min_len);
int regcam_tlv_parse_sess_root(u32 *out, const u8 *buf, u32 buf_size);
int regcam_tlv_parse_auth_root(u32 *out, const u8 *buf, u32 buf_size);
int regcam_tlv_parse_bind_root(u32 *out, const u8 *buf, u32 buf_size);
int regcam_tlv_parse_address_root(u32 *out, const u8 *buf, u32 buf_size);
int regcam_tlv_parse_billing_root(u32 *out, const u8 *buf, u32 buf_size);
int regcam_tlv_parse_account_info(const u32 *parent, u32 *out);
int regcam_tlv_parse_country_info(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_reg_rules(const u32 *parent, u32 *out);
int regcam_tlv_parse_name_info(const u32 *parent, u32 *out);
int regcam_tlv_parse_opt_in(const u32 *parent, u32 *out);
int regcam_tlv_parse_wal_opt(const u32 *parent, u32 *out);
int regcam_tlv_parse_card_info(const u32 *parent, u32 *out);
int regcam_tlv_parse_sec_q_item(const u32 *parent, const u8 *payload, u16 plen, u32 *out);
int regcam_tlv_parse_sec_q_nth(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_sec_q_first(const u32 *parent, u32 *out);
int regcam_tlv_parse_province_info(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_city_info(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_pay_method(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_bind_result(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_currency(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_sub_account(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_spend_limit(const u32 *parent, u32 index, u32 *limit_out);
int regcam_tlv_parse_tran_hist_item(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_tran_detail(const u32 *parent, u32 *out);
int regcam_tlv_parse_tran_order(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_tran_item(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_price_info(const u32 *parent, u32 *out, int is_alt);
int regcam_tlv_parse_service_info(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_entitlement(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_subscription(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_subscr_plan(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_reward(const u32 *parent, u32 index, u32 *out);
int regcam_tlv_parse_eula_info(const u32 *parent, u32 *out);
int regcam_tlv_parse_eula_ext(const u32 *parent, u32 *out);

/* Internal guarded API wrappers used by quick-registration */
int regcam_api_destroy_req(u32 req_id);
int regcam_api_abort_req(u32 req_id);
int regcam_api_create_del_sess_req(u32 ctx_id, u32 *req_id_out);
int regcam_api_start_del_sess(u32 req_id);
int regcam_api_recv_del_sess(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int regcam_api_create_get_prov_req(u32 ctx_id, u32 *req_id_out);
int regcam_api_start_get_prov(u32 req_id);
int regcam_api_recv_get_prov(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int regcam_api_init_prov_result(u32 *out, const u8 *buf, u32 buf_size);
int regcam_api_get_prov_info(const u32 *parent, u32 index, u32 *out);
int regcam_api_get_city_info(const u32 *parent, u32 index, u32 *out);
int regcam_api_create_reg_cc_req(u32 ctx_id, u32 *req_id_out);
int regcam_api_start_reg_cc(u32 req_id, const RegCamBillingInfo *info);
int regcam_api_recv_reg_cc(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int regcam_api_set_svc_type(u32 ctx_id, u32 svc_type);

/* Exported API prototypes used by quick-registration */
int sceNpCommerce2RegCam_5F271C1B(u32 field_type, const char *str, u32 *flags_out);
int sceNpCommerce2RegCam_06B1824F(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_7A315723(u32 req_id, const char *country, const char *lang, u32 yob, u32 mob, u32 dob);
int sceNpCommerce2RegCam_4B1A9E4E(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int sceNpCommerce2RegCam_856C52E9(u32 *out, const u8 *buf, u32 buf_size);
int sceNpCommerce2RegCam_BD48E118(const u32 *parent, u32 *out);
int sceNpCommerce2RegCam_DB478D24(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_5F45DF28(u32 req_id);
int sceNpCommerce2RegCam_6EA71511(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int sceNpCommerce2RegCam_EC918C7E(u32 *out, const u8 *buf, u32 buf_size);
int sceNpCommerce2RegCam_FAC30D4E(const u32 *parent, u32 index, u32 *out);
int sceNpCommerce2RegCam_B56A1200(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_13E0AD53(u32 req_id, const char *login, const char *pass,
                                  u32 sec_q, const char *sec_ans, u32 sec_ans_len);
int sceNpCommerce2RegCam_F5E2A7B0(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int sceNpCommerce2RegCam_69817297(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_BCFD9CC5(u32 req_id);
int sceNpCommerce2RegCam_E256E232(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int sceNpCommerce2RegCam_6F0783DA(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_CD9F9F24(u32 req_id, const char *oid);
int sceNpCommerce2RegCam_10E44562(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int sceNpCommerce2RegCam_B4CC8AE6(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_EE818631(u32 req_id, const char *first, u32 first_len,
                                  const char *last, u32 last_len,
                                  const char *first_ascii, const char *last_ascii, u32 gender);
int sceNpCommerce2RegCam_F6415A7A(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int sceNpCommerce2RegCam_3328C751(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_461DD73D(u32 req_id, const RegCamBillingInfo *info);
int sceNpCommerce2RegCam_28541A7A(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int sceNpCommerce2RegCam_DF407CA3(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_0EE4DB04(u32 req_id, const char *rrn, u32 rrn_len);
int sceNpCommerce2RegCam_993C822E(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int sceNpCommerce2RegCam_D99AEC05(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_726FF626(u32 req_id, u32 scex_flag, u32 aff_flag);
int sceNpCommerce2RegCam_FA13A517(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int sceNpCommerce2RegCam_64D92E9C(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_130756C8(u32 req_id);
int sceNpCommerce2RegCam_A2A91596(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);
int sceNpCommerce2RegCam_B140C5D7(u32 ctx_id, u32 *req_id_out);
int sceNpCommerce2RegCam_19679B00(u32 req_id, int use_explicit, const char *user, const char *pass);
int sceNpCommerce2RegCam_9C9E0B84(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out);

#endif /* REGCAM_INTERNAL_H */
