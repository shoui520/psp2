#ifndef STORE_INTERNAL_H
#define STORE_INTERNAL_H

#include <stddef.h>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long long u64;
typedef signed long long s64;

/* Commerce2 / Store error codes */
#define STORE_ERR_STACK_OVERFLOW        ((int)0x80550004)
#define STORE_ERR_NOT_INITIALIZED       ((int)0x80550F01)
#define STORE_ERR_ALREADY_INITIALIZED   ((int)0x80550F02)
#define STORE_ERR_INVALID_ARG           ((int)0x80550F03)
#define STORE_ERR_OUT_OF_RANGE          ((int)0x80550F06)
#define STORE_ERR_INVALID_ID            ((int)0x80550F07)
#define STORE_ERR_OUT_OF_MEMORY         ((int)0x80550F0B)
#define STORE_ERR_ABORTED               ((int)0x80550F10)
#define STORE_ERR_BUF_OVERFLOW          ((int)0x80550F11)
#define STORE_ERR_TRUNCATED             ((int)0x80550F12)
#define STORE_ERR_RESP_TOO_SMALL        ((int)0x80550F14)
#define STORE_ERR_RESP_BUF_LIMIT        ((int)0x80550F18)
#define STORE_ERR_INVALID_CMD           ((int)0x80550F81)
#define STORE_ERR_INVALID_PAYMENT       ((int)0x80550F82)
#define STORE_ERR_BUF_TOO_SMALL         ((int)0x80550F84)
#define STORE_ERR_TAG_NOT_FOUND         ((int)0x80550F87)
#define STORE_ERR_COUNTRY_NOT_FOUND     ((int)0x80550F92)
#define STORE_ERR_MAINTENANCE           ((int)0x80550F93)
#define STORE_ERR_INVALID_SERVICE       ((int)0x80550F96)
#define STORE_ERR_INVALID_STREET        ((int)0x8055102A)
#define STORE_ERR_SERVER_BASE           ((int)0x80551000)
#define STORE_ERR_SERVER_GENERIC        ((int)0x80551001)
#define STORE_ERR_SERVER_CARD_INVALID   ((int)0x8055102B)
#define STORE_ERR_SERVER_ADDR_INVALID   ((int)0x80551041)

/* SDK version and runtime thresholds */
#define STORE_SDK_VER_550               0x0505000FU
#define STORE_SDK_VER_620               0x0602FFFFU
#define STORE_MIN_STACK_INNER           4032
#define STORE_MIN_STACK_STD             4080
#define STORE_MIN_STACK_LARGE           14320
#define STORE_KEEPALIVE_LIMIT_US        9999999ULL
#define STORE_UNIX_EPOCH_US             0x00DCBFFEFF2BC000ULL

/* Store request command IDs (1-based index into g_endpoint_names) */
enum StoreCmdId {
    STORE_CMD_CR_SESS            = 1,
    STORE_CMD_DEL_SESS           = 2,
    STORE_CMD_GET_CAT            = 3,
    STORE_CMD_GET_PROD           = 4,
    STORE_CMD_SRCH_PROD          = 5,
    STORE_CMD_GET_WAL            = 6,
    STORE_CMD_GET_DRM            = 7,
    STORE_CMD_PUR_PREV           = 8,
    STORE_CMD_PURCH              = 9,
    STORE_CMD_GET_CC             = 10,
    STORE_CMD_TOPUP_CC           = 11,
    STORE_CMD_VCHR_RED_PREV      = 12,
    STORE_CMD_VCHR_RED           = 13,
    STORE_CMD_TOP_EDY            = 14,
    STORE_CMD_TOP_EDY_RES        = 15,
    STORE_CMD_ADD_CART           = 16,
    STORE_CMD_REM_CART           = 17,
    STORE_CMD_CLEAR_CART         = 18,
    STORE_CMD_GET_CART           = 19,
    STORE_CMD_GET_PROV           = 20,
    STORE_CMD_GET_PAY            = 21,
    STORE_CMD_GET_BILL           = 22,
    STORE_CMD_REG_CC             = 23,
    STORE_CMD_GET_ADDR           = 24,
    STORE_CMD_GET_RES_ADDR       = 25,
    STORE_CMD_EVENT_LOG          = 26,
    STORE_CMD_START_REG          = 29,
    STORE_CMD_SET_RES_ADDR       = 38,
    STORE_CMD_GET_BILL_INFO      = 43,
    STORE_CMD_AUTHENTICATE       = 49,
    STORE_CMD_GET_CNSL_ACT       = 59,
    STORE_CMD_VERIFY_DOB         = 61,
    STORE_CMD_GET_EPW            = 65,
    STORE_CMD_CR_GUEST_SESS      = 66,
    STORE_CMD_GET_PROD_LIST      = 71,
    STORE_CMD_RECOM_PRODS        = 76,
    STORE_CMD_GET_PROD_IDS       = 80,
};

/* Form key indices into g_form_keys */
enum StoreFormKeyId {
    SFKEY_AMP                   = 0,
    SFKEY_COMMA                 = 1,
    SFKEY_VERSION               = 2,
    SFKEY_USERNAME              = 3,
    SFKEY_PASSWORD              = 4,
    SFKEY_CONSOLE_ID            = 5,
    SFKEY_CATEGORY_ID           = 6,
    SFKEY_START_POSITION        = 7,
    SFKEY_MAX_COUNT             = 8,
    SFKEY_PRODUCT_ID            = 9,
    SFKEY_STORE_TYPE            = 10,
    SFKEY_QUERY_KEY             = 11,
    SFKEY_QUERY_VALUE           = 12,
    SFKEY_SORT_METHOD           = 13,
    SFKEY_SKU_ID                = 14,
    SFKEY_SKU_IDS               = 15,
    SFKEY_TITLE_ID              = 16,
    SFKEY_DRM_CONTENT_TYPE      = 17,
    SFKEY_QUICK_PURCHASE        = 18,
    SFKEY_TOPUP_AMOUNT          = 19,
    SFKEY_SUFFIX_A              = 20,
    SFKEY_SUFFIX_B              = 21,
    SFKEY_SUFFIX_C              = 22,
    SFKEY_EDY_RECEIPT_ID        = 23,
    SFKEY_RESULT_EDY_TOPUP      = 24,
    SFKEY_PAYMENT_TYPE          = 25,
    SFKEY_PAYMENT_METHOD_ID     = 26,
    SFKEY_HOLDER_NAME           = 27,
    SFKEY_CARD_NUMBER           = 28,
    SFKEY_START_YEAR            = 29,
    SFKEY_START_MONTH           = 30,
    SFKEY_EXPIRE_YEAR           = 31,
    SFKEY_EXPIRE_MONTH          = 32,
    SFKEY_SECURITY_CODE         = 33,
    SFKEY_ISSUE_NUMBER          = 34,
    SFKEY_CITY                  = 35,
    SFKEY_PROVINCE              = 36,
    SFKEY_POSTAL_CODE           = 37,
    SFKEY_EVENT_TYPE            = 38,
    SFKEY_ID                    = 39,
    SFKEY_URL                   = 40,
    SFKEY_COUNTRY               = 41,
    SFKEY_LANGUAGE              = 42,
    SFKEY_YOB                   = 43,
    SFKEY_MOB                   = 44,
    SFKEY_DOB                   = 45,
    SFKEY_AUTO_DEPOSIT_FLAG     = 63,
    SFKEY_SERVICE_ID            = 73,
    SFKEY_SAVE                  = 74,
    SFKEY_REWARD_ID             = 75,
    SFKEY_REWARD_IDS            = 76,
    SFKEY_PRODUCT_IDS           = 77,
    SFKEY_RECOM_TYPE            = 78,
    SFKEY_CONTENT_ID            = 79,
    SFKEY_FILE_ID               = 80,
};

typedef struct {
    u32 len;
    const char *str;
} StoreFormKey;

typedef struct {
    u32 key_index;
    const char *value;
    u32 value_len;
} StoreSearchQuery;

/* Commerce2 context layout (shared with sceNpCommerce2) */
typedef struct {
    u8  ctx_prefix[24];
    int http_tmpl_id;         /* +0x018 */
    char url_buf[8];          /* +0x01C */
    u8  session_buf[1024];    /* +0x024 */
    u32 session_len;          /* +0x424 */
    u32 svc_type;             /* +0x428 */
    u32 pad;                  /* +0x42C */
    u64 last_activity_us;     /* +0x430 */
    u32 no_size_check;        /* +0x438 */
    u32 http_status;          /* +0x43C */
    char country_code[4];     /* +0x440 */
    char lang_code[4];        /* +0x444 */
} StoreContext;

typedef struct {
    u32 header_words[3];
    u32 req_flags;            /* +0x0C (bit 1 = aborted) */
    int http_req_id;          /* +0x10 */
    u16 cmd_id;               /* +0x14 */
    u16 pad;
} StoreRequest;

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
    const char *street_a;
    u32 street_a_len;
    const char *street_b;
    u32 street_b_len;
    const char *street_c;
    u32 street_c_len;
    const char *city;
    u32 city_len;
    const char *province;
    u32 province_len;
    const char *postal_code;
    u32 postal_code_len;
} StoreBillingInfo;

typedef struct {
    char country[4];
    u32  cap_flags;
} StoreCountryCaps;

static inline u32 store_bswap32(u32 val)
{
    return ((val & 0x000000FFU) << 24) |
           ((val & 0x0000FF00U) << 8)  |
           ((val & 0x00FF0000U) >> 8)  |
           ((val & 0xFF000000U) >> 24);
}

static inline u16 store_bswap16(u16 val)
{
    return (u16)((val << 8) | (val >> 8));
}

static inline s32 store_min_s32(s32 lhs, s32 rhs) { return lhs < rhs ? lhs : rhs; }

/* Global tables & state */
extern const char * const g_endpoint_names[86];
extern const char * const g_service_prefixes[9];
extern const StoreFormKey g_form_keys[81];
extern const char * const g_video_query_keys[7];
extern const char * const g_game_query_keys[7];
extern const char * const g_comic_query_keys[7];
extern const char * const g_country_tier_full[2];
extern const char * const g_country_tier_mid[33];
extern const char * const g_country_tier_basic[24];

extern u32 g_store_init;
extern u32 g_store_mgr;

/* Imported library functions */
extern int   KprintfForUser(const char *fmt, ...);
extern int   _sceKernelCheckThreadStack(void);
extern int   sceKernelGetCompiledSdkVersion(void);
extern void *sceKernelMemcpy(void *dst, const void *src, size_t len);
extern void *sceKernelMemset(void *dst, int ch, size_t len);
extern void *sceNetIfhandle_lib_15CFE3C0(u32 size);
extern void  sceNetIfhandle_lib_76BAD213(void *ptr);
extern int   sceNet_lib_94DCA9F0(const char *lhs, const char *rhs, size_t len);
extern int   sceNet_lib_96EF9DA1(int ch);
extern int   sceNet_lib_A0F16ABD(const char *lhs, const char *rhs);
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, size_t len);
extern char *sceNet_lib_BCBE14CF(const char *str, int ch);
extern int   sceNet_lib_C13C9307(int ch);
extern int   sceNet_lib_CF705E46(char *out, const char *fmt, ...);
extern u32   sceNet_lib_D8722983(const char *str);
extern int   sceNp_2E4769C3(void);

extern int sceNpCommerce2_06618C81(u32 *mgr, StoreRequest *req, u32 ctx_id, u32 *req_id_out);
extern int sceNpCommerce2_0A6AB496(u32 *mgr, u32 ctx_id, StoreContext **ctx_out, int lock_flag);
extern int sceNpCommerce2_157FA8AA(char *console_id_out, u32 buf_size);
extern int sceNpCommerce2_22CEC5E2(void *dst, u32 dst_size, u32 lo, u32 hi, u32 zero_flag);
extern int sceNpCommerce2_3546C737(const u8 *scan, u32 scan_len, u32 tag, u32 min_len,
                                   u32 *count_out, u32 extra);
extern int sceNpCommerce2_382803C8(int http_tmpl, u32 svc_type, u32 cmd_id, const char *prefix,
                                   const char *endpoint, int zero_flag, char *url_buf,
                                   int *http_req_out, u32 len_lo, u32 len_hi);
extern int sceNpCommerce2_3F6D413C(const u8 *scan, u32 scan_len, u32 tag, u16 *total_len_out,
                                   u32 *count_out);
extern int sceNpCommerce2_460A1D92(int *out_ver, int kind);
extern int sceNpCommerce2_4D43B84D(int http_req, u32 *status_out);
extern int sceNpCommerce2_66EC0829(char *dst, u32 dst_size, const char *src, u32 src_len,
                                   int mode);
extern int sceNpCommerce2_69FEA1C8(const u8 *scan, u32 scan_len, u32 tag, u32 min_len,
                                   u32 *range_out, u32 extra);
extern int sceNpCommerce2_75DF99A9(int http_tmpl, char *url_buf);
extern int sceNpCommerce2_77BE411B(u32 *mgr);
extern int sceNpCommerce2_851E7844(u32 *mgr, u32 req_id, StoreRequest **req_out);
extern int sceNpCommerce2_8642C532(u32 *mgr);
extern int sceNpCommerce2_ADAA054C(const void *resp, u32 buf_size, u32 body_len);
extern int sceNpCommerce2_AE614CC6(int http_req, const void *payload, u32 len);
extern int sceNpCommerce2_C27FEAB1(StoreContext *ctx, int http_req, u32 cmd_id,
                                   u8 *status_out, void *resp, u32 buf_size);
extern int sceNpCommerce2_D4C319C6(int http_req, void *resp, void *body_pos,
                                   u32 buf_size, int *trunc_out);
extern int sceNpCommerce2_D5932A6C(StoreContext *ctx, char *country_out, u32 size, int mode);
extern int sceNpCommerce2_E3455E27(const u8 *scan, u32 scan_len, u32 tag, u32 min_len,
                                   u16 *len_out);
extern int sceNpCommerce2_EE7255FE(StoreContext *ctx, u32 option_val);
extern int sceNpCommerce2_F0900551(StoreContext *ctx, u32 *ver_out);
extern int sceNpCommerce2_F8D175FA(const void *resp, u32 buf_size, const char *country,
                                   const char *lang);
extern int sceNpCommerce2_FC47B63C(u64 *time_out);

/* Request senders & helpers (store_req.c) */
int store_resolve_endpoint(u32 svc_type, u16 cmd_id, const char **prefix_out,
                           const char **endpoint_out);
int store_send_get_cat(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const char *cat_id, const char * const *reward_id, u32 has_reward,
                       u32 start_pos, u32 max_cnt);
int store_send_get_prod(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char *cat_id, const char *prod_id,
                        const char * const *reward_id, u32 has_reward);
int store_send_add_cart(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char * const *sku_ids, u32 sku_cnt,
                        const char * const *reward_ids, u32 reward_cnt);
int store_send_rem_cart(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char *sku_id);
int store_send_clear_cart(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                          int *http_req_out);
int store_send_get_cart(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                        int *http_req_out);
int store_send_purchase(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                        const char *password, int quick_purchase, int auto_deposit);
int store_send_get_cc(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out);
int store_send_get_prov(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out);
int store_send_payment_type(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                            int *http_req_out, u32 pay_type);
int store_send_get_drm(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const char * const *sku_ids, u32 sku_cnt, const char *svc_id,
                       const char *reward_ids, u32 start_pos, u32 max_cnt, u32 drm_type);
int store_send_get_bill(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out);
int store_send_billing(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const StoreBillingInfo *billing);
int store_send_postal_code(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                           int *http_req_out, const char *postal_code);
int store_send_get_wal(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out);
int store_send_topup_amount(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                            int *http_req_out, u32 amount);
int store_send_edy_result(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                          const char *receipt_id, int is_ok);
int store_send_voucher(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                       const char *code_a, const char *code_b, const char *code_c,
                       int auto_deposit);
int store_send_event_log(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         u32 event_type, const char *id_str, const char *url_str);
int store_send_search_prod(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                           int *http_req_out, u32 start_pos, u32 max_cnt,
                           const StoreSearchQuery *queries, u32 query_cnt, u32 sort_method);
int store_send_get_epw(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out);
int store_send_guest_sess(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                          u32 ver, const char *country, const char *lang,
                          u32 yob, u32 mob, u32 dob, const char *console_id);
int store_send_prod_list(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id, int *http_req_out,
                         const char * const *prod_ids, u32 prod_cnt);
int store_send_recom_prods(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                            int *http_req_out, const char * const *prod_ids, u32 prod_cnt,
                            u32 start_pos, u32 max_cnt, u32 recom_type);
int store_send_get_prod_ids(u32 svc_type, int http_tmpl, char *url_buf, u16 cmd_id,
                            int *http_req_out, const char *content_id, const char *file_id);

/* TLV parsers (store_tlv.c) */
int store_tlv_parse_category_info(const u32 *parent, u32 *out, int api_ver);
int store_tlv_parse_cat_sku(const u32 *parent, u32 *out, int api_ver);
int store_tlv_parse_product_info(const u32 *parent, u32 *out, int api_ver);
int store_tlv_parse_product_sku(const u32 *parent, u32 index, u32 *out, int api_ver);
int store_tlv_parse_link_info(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_entitlement(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_subscr_sku(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_download_file(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_video_sku(const u32 *parent, u32 index, u32 *out, int api_ver);
int store_tlv_parse_cart_entry(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_cart_sku_brief(const u32 *parent, u32 *out);
int store_tlv_parse_cart_sku_full(const u32 *parent, u32 *out, int api_ver);
int store_tlv_parse_audio_lang(const u32 *parent, u32 index, u32 *code_out, u32 *flags_out);
int store_tlv_parse_card_info(const u32 *parent, u32 *out);
int store_tlv_parse_drm_item(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_drm_reward(const u32 *parent, u32 *out, int api_ver);
int store_tlv_parse_drm_child(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_address_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_parse_edy_info(const u32 *parent, u32 *out);
int store_tlv_parse_voucher_item(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_download_list(const u32 *parent, u32 *out);
int store_tlv_parse_video_info(const u32 *parent, u32 *out, int api_ver);
int store_tlv_parse_res_addr_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_parse_account_info(const u32 *parent, u32 *out);
int store_tlv_parse_reg_rules(const u32 *parent, u32 *out);
int store_tlv_parse_billing_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_parse_auth_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_parse_console_act(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_err_flag(const u8 *resp, u32 resp_size, u32 *flag_out);
int store_tlv_init_err_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_parse_err_item(const u32 *parent, u32 index, u32 *out);
int store_tlv_init_sess_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_sess_currency(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_eula_flag(const u32 *parent, u32 *out);
int store_tlv_get_cat_icon_url(const u32 *parent, u32 index, u32 *out_ptr);
int store_tlv_parse_category_item(const u32 *parent, u32 *out);
int store_tlv_parse_rating_first(const u32 *parent, u32 *out);
int store_tlv_parse_rating_nth_cat(const u32 *parent, u32 index, u32 *out);
int store_tlv_get_reward_std(const u32 *parent, u32 *out);
int store_tlv_get_reward_ext(const u32 *parent, u32 *out);
int store_tlv_parse_rating_nth_prod(const u32 *parent, u32 index, u32 *out);
int store_tlv_get_prod_genre(const u32 *parent, u32 index, u32 *out_ptr);
int store_tlv_parse_credit_info(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_subscr_ent(const u32 *parent, u32 index, u32 *out);
int store_tlv_init_add_cart_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_add_cart_wallet(const u32 *parent, u32 *out);
int store_tlv_get_add_cart_summary(const u32 *parent, u32 *out);
int store_tlv_parse_cart_item(const u32 *parent, int indexed, u32 index, u32 *out);
int store_tlv_get_subtitle_lang(const u32 *parent, u32 index, u32 *out_ptr);
int store_tlv_init_rem_cart_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_rem_cart_wallet(const u32 *parent, u32 *out);
int store_tlv_get_rem_cart_summary(const u32 *parent, u32 *out);
int store_tlv_init_get_cart_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_cart_wallet(const u32 *parent, u32 *out);
int store_tlv_get_cart_summary(const u32 *parent, u32 *out);
int store_tlv_init_pur_prev_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_pur_prev_wallet(const u32 *parent, u32 *out);
int store_tlv_get_pur_prev_tax(const u32 *parent, u32 *out);
int store_tlv_get_pur_prev_summary(const u32 *parent, u32 *out);
int store_tlv_init_purch_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_purch_wallet(const u32 *parent, u32 *out);
int store_tlv_get_purch_summary(const u32 *parent, u32 *out);
int store_tlv_get_purch_tax(const u32 *parent, u32 *out);
int store_tlv_init_get_cc_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_cc_card(const u32 *parent, u32 *out);
int store_tlv_init_get_prov_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_prov_item(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_city_info(const u32 *parent, u32 index, u32 *out);
int store_tlv_init_get_pay_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_pay_item(const u32 *parent, u32 index, u32 *out);
int store_tlv_init_get_drm_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_init_addr_lookup_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_init_get_wal_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_wal_info(const u32 *parent, u32 *out);
int store_tlv_init_topup_cc_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_topup_cc_card(const u32 *parent, u32 *out);
int store_tlv_init_top_edy_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_init_vchr_prev_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_vchr_prev_summary(const u32 *parent, u32 *out);
int store_tlv_get_vchr_prev_items(const u32 *parent, u32 *out);
int store_tlv_init_vchr_red_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_vchr_red_summary(const u32 *parent, u32 *out);
int store_tlv_get_vchr_red_items(const u32 *parent, u32 *out);
int store_tlv_init_search_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_parse_search_item_paged(const u32 *parent, u32 index, u32 *out);
int store_tlv_get_search_tag_ptr(const u32 *parent, u32 index, u32 *out_ptr);
int store_tlv_parse_search_item_ext(const u32 *parent, u32 index, u32 *out);
int store_tlv_parse_search_item_single(const u32 *parent, u32 *out);
int store_tlv_get_billing_prov(const u32 *parent, u32 index, u32 *out);
int store_tlv_get_billing_pay(const u32 *parent, u32 index, u32 *out);
int store_tlv_get_billing_card(const u32 *parent, u32 *out);
int store_tlv_init_cnsl_act_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_init_epw_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_init_prod_list_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_parse_prod_list_item(const u32 *parent, u32 index, u32 *out);
int store_tlv_init_recom_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_init_prod_ids_root(u32 *out, const u8 *resp, u32 resp_size);
int store_tlv_get_prod_id_ptr(const u32 *parent, u32 index, u32 *out_ptr);

/* Core manager & request dispatchers (store_dispatch.c) */
int  store_recv_response(u32 req_id, u8 *resp, u32 buf_size, u32 *recv_size_out);
int  store_start_get_cat(u32 req_id, const char *cat_id, const char * const *reward_id,
                         u32 has_reward, u32 start_pos, u32 max_cnt);
int  store_start_guest_sess(u32 req_id, const char *country, const char *lang,
                            u32 yob, u32 mob, u32 dob);
int  store_get_country_caps(u32 ctx_id, const char *country, StoreCountryCaps *out_caps);
int  store_init_mgr(void);
void store_term_mgr(void);
int  store_start_get_prod(u32 req_id, const char *cat_id, const char *prod_id,
                          const char * const *reward_id, u32 has_reward);
int  store_disp_create_add_cart(u32 ctx_id, u32 *req_id_out);
int  store_start_add_cart(u32 req_id, const char * const *sku_ids, u32 sku_cnt,
                          const char * const *reward_ids, u32 reward_cnt);
int  store_disp_create_rem_cart(u32 ctx_id, u32 *req_id_out);
int  store_start_rem_cart(u32 req_id, const char *sku_id);
int  store_disp_create_clear_cart(u32 ctx_id, u32 *req_id_out);
int  store_start_clear_cart(u32 req_id);
int  store_disp_create_get_cart(u32 ctx_id, u32 *req_id_out);
int  store_start_get_cart(u32 req_id);
int  store_disp_create_pur_prev(u32 ctx_id, u32 *req_id_out);
int  store_disp_create_purch(u32 ctx_id, u32 *req_id_out);
int  store_start_purchase(u32 req_id, const char *password, int quick_purchase, int auto_deposit);
int  store_disp_create_get_cc(u32 ctx_id, u32 *req_id_out);
int  store_start_get_cc(u32 req_id);
int  store_disp_create_get_prov(u32 ctx_id, u32 *req_id_out);
int  store_start_get_prov(u32 req_id);
int  store_disp_create_get_pay(u32 ctx_id, u32 *req_id_out);
int  store_disp_create_get_bill_info(u32 ctx_id, u32 *req_id_out);
int  store_start_payment_type(u32 req_id, u32 pay_type);
int  store_disp_create_get_drm(u32 ctx_id, u32 *req_id_out);
int  store_start_get_drm(u32 req_id, const char * const *sku_ids, u32 sku_cnt, const char *svc_id,
                         const char *reward_ids, u32 start_pos, u32 max_cnt, u32 drm_type);
int  store_disp_create_get_bill(u32 ctx_id, u32 *req_id_out);
int  store_disp_create_get_res_addr(u32 ctx_id, u32 *req_id_out);
int  store_start_get_bill(u32 req_id);
int  store_disp_create_reg_cc(u32 ctx_id, u32 *req_id_out);
int  store_start_billing(u32 req_id, const StoreBillingInfo *billing);
int  store_disp_create_get_addr(u32 ctx_id, u32 *req_id_out);
int  store_start_postal_code(u32 req_id, const char *postal_code);
int  store_disp_create_get_wal(u32 ctx_id, u32 *req_id_out);
int  store_start_get_wal(u32 req_id);
int  store_disp_create_topup_cc(u32 ctx_id, u32 *req_id_out);
int  store_disp_create_top_edy(u32 ctx_id, u32 *req_id_out);
int  store_start_topup_amount(u32 req_id, u32 amount);
int  store_disp_create_top_edy_res(u32 ctx_id, u32 *req_id_out);
int  store_start_edy_result(u32 req_id, const char *receipt_id, int is_ok);
int  store_disp_create_vchr_prev(u32 ctx_id, u32 *req_id_out);
int  store_disp_create_vchr_red(u32 ctx_id, u32 *req_id_out);
int  store_start_voucher(u32 req_id, const char *code_a, const char *code_b,
                         const char *code_c, int auto_deposit);
int  store_disp_create_event_log(u32 ctx_id, u32 *req_id_out);
int  store_start_event_log(u32 req_id, u32 event_type, const char *id_str, const char *url_str);
int  store_disp_create_srch_prod(u32 ctx_id, u32 *req_id_out);
int  store_start_search_prod(u32 req_id, u32 svc_type, u32 start_pos, u32 max_cnt,
                             const StoreSearchQuery *queries, u32 query_cnt, u32 sort_method);
int  store_disp_create_get_cnsl_act(u32 ctx_id, u32 *req_id_out);
int  store_start_get_cnsl_act(u32 req_id);
int  store_disp_create_get_epw(u32 ctx_id, u32 *req_id_out);
int  store_start_get_epw(u32 req_id);
int  store_disp_create_guest_sess(u32 ctx_id, u32 *req_id_out);
int  store_disp_create_prod_list(u32 ctx_id, u32 *req_id_out);
int  store_start_prod_list(u32 req_id, const char * const *prod_ids, u32 prod_cnt);
int  store_disp_create_recom_prods(u32 ctx_id, u32 *req_id_out);
int  store_start_recom_prods(u32 req_id, const char * const *prod_ids, u32 prod_cnt,
                             u32 start_pos, u32 max_cnt, u32 recom_type);
int  store_disp_create_prod_ids(u32 ctx_id, u32 *req_id_out);
int  store_start_get_prod_ids(u32 req_id, const char *content_id, const char *file_id);
int  store_set_ctx_option(u32 ctx_id, u32 option_val);
int  store_set_svc_type(u32 ctx_id, u32 svc_type);
int  store_get_http_status(u32 ctx_id, u32 *status_out);
int  store_create_req(u32 ctx_id, u16 cmd_id, u32 *req_id_out);
int  store_post_process_response(StoreContext *ctx, StoreRequest *req,
                                 const u8 *resp, u32 buf_size, u32 resp_size);

#endif /* STORE_INTERNAL_H */
