#ifndef NP_SERVICE_INTERNAL_H
#define NP_SERVICE_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

typedef int SceUID;
typedef unsigned int SceSize;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

static inline u16 bswap16(u16 val) { return (u16)((val >> 8) | (val << 8)); }

/* =========================================================================
 * Shared Semantic Structures
 * ========================================================================= */

typedef struct NpId {
    u8 handle[16];
    u8 handle_pad[4];
    u8 opt_country[2];
    u8 opt_region[2];
    char platform[4];
    u8 opt_flags[8];
} NpId; /* 36 bytes */

typedef struct NpProfileBase {
    NpId npid;
    char online_name[52];
    char avatar_url[128];
} NpProfileBase; /* 216 bytes */

typedef struct NpLangArray {
    int codes[4];
} NpLangArray; /* 16 bytes */

typedef struct NpAboutMe {
    char text[64];
} NpAboutMe; /* 64 bytes */

typedef struct NpProfileRecord {
    NpProfileBase base;
    u32 country_code;
    NpLangArray languages;
    int avatar_id;
    NpAboutMe about_me;
} NpProfileRecord; /* 304 bytes */

typedef struct NpAllocNode {
    void *block;
    struct NpAllocNode *next;
} NpAllocNode; /* 8 bytes */

typedef struct NpFqdnEntry {
    char *host;
    u16 port;
    u16 server_id;
    u32 status;
    struct NpFqdnEntry *next;
} NpFqdnEntry; /* 16 bytes + trailing hostname */

typedef struct NpUploaderEntry {
    char reference[64];
    s64 file_size;
    u8 reserved[40];
    u8 application[16];
    u64 created_date;
    s64 keep_days;
    struct NpUploaderEntry *next;
    u32 tail_pad;
} NpUploaderEntry; /* 152 bytes */

typedef struct NpUploaderDlInfo {
    s64 file_size;
    int quota_kind;
    NpId npid;
    u8 application[16];
    u64 created_date;
} NpUploaderDlInfo; /* 72 bytes */

typedef struct NpAvatarCategory {
    int category_id;
    int avatar_count;
    char name[400];
} NpAvatarCategory; /* 408 bytes */

typedef struct NpUserCtx {
    int user_id;
    int ref_count;
    int destroying;
    u8 online_id[20];
    u8 lwmutex[32];
    char env[15];
    char country[9];
    NpFqdnEntry *searchjid_fqdn;
    NpAllocNode *alloc_list;
    struct NpUserCtx *next;
    struct NpUserCtx *prev;
} NpUserCtx; /* 104 bytes */

typedef struct NpTransCtx {
    int trans_id;
    int ref_count;
    int destroying;
    int conn_id;
    int req_id;
    int owner;
    int aborted;
    int sending;
    NpAllocNode *alloc_list;
    NpUserCtx *user;
    struct NpTransCtx *next;
    struct NpTransCtx *prev;
    int reserved;
    int kind;
    void *extra;
    u8 lwmutex[32];
} NpTransCtx; /* 92 bytes */

typedef struct NpXmlProfileCtx {
    int parse_kind;
    NpProfileRecord *profile;
    int update_flag;
    int ptlp;
    u32 seen_mask;
    u32 align_pad;
    u64 updated_date;
} NpXmlProfileCtx; /* 32 bytes */

typedef struct NpXmlFqdnCtx {
    int parse_kind;
    const void *title_id;
    NpFqdnEntry **list_head;
    NpFqdnEntry *list_tail;
    u32 pending_status;
    u16 pending_id;
    u16 pending_port;
    u32 seen_mask;
} NpXmlFqdnCtx; /* 28 bytes */

typedef struct NpXmlSearchJidCtx {
    NpId *npid_out;
    u32 seen_mask;
} NpXmlSearchJidCtx; /* 8 bytes */

typedef struct NpXmlUploaderListCtx {
    NpUploaderEntry **list_head;
    u32 expected_count;
    u32 parsed_count;
    NpUploaderEntry *current;
    NpUploaderEntry *list_tail;
    u32 seen_mask;
} NpXmlUploaderListCtx; /* 24 bytes */

typedef struct NpXmlUploaderDlCtx {
    NpUploaderDlInfo *dl_info;
    u32 seen_mask;
} NpXmlUploaderDlCtx; /* 8 bytes */

typedef struct NpXmlAvatarCatCtx {
    NpAvatarCategory *entries;
    u32 total_count;
    u32 parsed_count;
    u32 max_count;
    u32 seen_mask;
} NpXmlAvatarCatCtx; /* 20 bytes */

typedef struct NpXmlAvatarListCtx {
    char (*urls)[128];
    int *avatar_ids;
    u32 total_count;
    u32 parsed_count;
    u32 max_count;
    u32 seen_mask;
} NpXmlAvatarListCtx; /* 24 bytes */

/* =========================================================================
 * Imported Functions
 * ========================================================================= */

/* Kernel_Library */
extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int intr);
extern void *sceKernelMemset(void *dst, int c, SceSize len);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize len);
extern int sceKernelLockLwMutex(void *workarea, int count, u32 *timeout);
extern int sceKernelUnlockLwMutex(void *workarea, int count);
extern int sceKernelCheckThreadStack(void);

/* ModuleMgrForUser */
extern int sceKernelGetModuleGPByAddress(void *addr, u32 *gp);

/* SysMemUserForUser */
extern int sceKernelQueryMemoryInfo(void *addr, SceUID *partId, int *attr);
extern u32 sceKernelGetCompiledSdkVersion(void);

/* ThreadManForUser */
extern int sceKernelDelayThread(u32 usec);
extern SceUID sceKernelCreateSema(const char *name, u32 attr, int initVal, int maxVal, void *opt);
extern int sceKernelDeleteSema(SceUID semaid);
extern int sceKernelSignalSema(SceUID semaid, int count);
extern int sceKernelWaitSema(SceUID semaid, int count, u32 *timeout);
extern SceUID sceKernelCreateMutex(const char *name, u32 attr, int initCount, void *opt);
extern int sceKernelDeleteMutex(SceUID mutexid);
extern int sceKernelLockMutex(SceUID mutexid, int count, u32 *timeout);
extern int sceKernelUnlockMutex(SceUID mutexid, int count);
extern int sceKernelCreateLwMutex(void *workarea, const char *name, u32 attr, int initCount, void *opt);
extern int sceKernelDeleteLwMutex(void *workarea);
extern SceUID sceKernelCreateThread(const char *name, void *entry, int prio, int stackSize, u32 attr, void *opt);
extern int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern int sceKernelWaitThreadEnd(SceUID thid, u32 *timeout);
extern int sceKernelDeleteThread(SceUID thid);
extern int sceKernelExitThread(int status);
extern SceUID sceKernelCreateVpl(const char *name, SceUID partId, u32 attr, SceSize size, void *opt);
extern int sceKernelDeleteVpl(SceUID vplid);
extern int sceKernelTryAllocateVpl(SceUID vplid, SceSize size, void **out_ptr);
extern int sceKernelFreeVpl(SceUID vplid, void *block);
extern int sceKernelReferVplStatus(SceUID vplid, void *status_out);

/* sceRtc */
extern int sceRtcGetCurrentTick(u64 *tick);
extern int sceRtc_508BA64B(const u64 *tick);
extern int sceRtcParseDateTime(u64 *tick, const char *str);

/* sceNet / sceNet_lib */
extern int sceNetFreeThreadinfo(SceUID thid);
extern int sceNet_lib_1FB2FDDD(const char *str);
extern u32 sceNet_lib_2A73ADDC(const char *str, char **endptr, int base);
extern int sceNet_lib_384EFE14(const void *in_buf, int in_len, const void *key, int key_len, void *out);
extern int sceNet_lib_5705F6F9(void);
extern char *sceNet_lib_80C9F02A(char *dst, const char *src);
extern int sceNet_lib_94DCA9F0(const char *lhs, const char *rhs, int len);
extern int sceNet_lib_A0F16ABD(const char *lhs, const char *rhs);
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, int len);
extern int sceNet_lib_B9085A96(const char *lhs, const char *rhs, int len);
extern int sceNet_lib_CF705E46(char *out_buf, const char *fmt, ...);
extern int sceNet_lib_D8722983(const char *str);
extern int sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, int len);
extern u8 sceNet_lib_750F705D(int ch);

/* sceParseHttp */
extern int sceParseHttpResponseHeader(const void *hdr, u32 hdr_len, const char *field_name, const char **val, u32 *val_len);

/* sceHttp */
extern int sceHttpCreateTemplate(const char *agent, int httpver, int proxy);
extern int sceHttpDeleteTemplate(int tid);
extern int sceHttpCreateConnection(int tid, const char *host, const char *scheme, u16 port, int keepalive);
extern int sceHttpCreateConnectionWithURL(int tid, const char *url, int keepalive);
extern int sceHttpDeleteConnection(int cid);
extern int sceHttpCreateRequest(int cid, int method, const char *path, u64 content_len);
extern int sceHttpCreateRequestWithURL(int cid, int method, const char *url, u64 content_len);
extern int sceHttpDeleteRequest(int rid);
extern int sceHttpSendRequest(int rid, const void *body, u32 len);
extern int sceHttpAbortRequest(int rid);
extern int sceHttpReadData(int rid, void *out_buf, u32 len);
extern int sceHttpGetStatusCode(int rid, int *code);
extern int sceHttpGetContentLength(int rid, u64 *len);
extern int sceHttpGetAllHeader(int rid, const char **hdr, u32 *hdr_len);
extern int sceHttpAddExtraHeader(int id, const char *name, const char *val, int mode);
extern int sceHttpDeleteHeader(int id, const char *name);
extern int sceHttpSetConnectTimeOut(int id, u32 usec);
extern int sceHttpSetRecvTimeOut(int id, u32 usec);
extern int sceHttpSetSendTimeOut(int id, u32 usec);
extern int sceHttpSetAuthInfoCallback(int id, void *cb, void *cb_arg);
extern int sceHttpsGetSslError(int id, int *err, u32 *detail);
extern int sceHttp_3C478044(int id, int val);
extern int sceHttp_87F1E666(int id, int val);

/* sceNpCore / sceNp / sceNpAuth */
extern int sceNpCore_52440ABF(char *env);
extern int sceNp_02CA8CAA(void *ticket, int len, const u64 *tick);
extern int sceNp_0B8BEE48(const void *jid, char *out, u32 out_len, u32 *req_len);
extern int sceNp_1916432C(void *parser, const void *xml_chunk, u32 len);
extern int sceNp_4B09907A(char *dst, const char *src, u32 size);
extern int sceNp_5FA879D8(void *jid, const char *str, void *work_buf, u32 work_len, u32 *req_len);
extern int sceNp_64F72B22(void *dst, const char *str);
extern int sceNp_66B86876(const void *user, const void *pass, u32 lo, u32 hi);
extern int sceNp_67BCF9E3(int flags, u16 *ticket_len);
extern int sceNp_6999EDC4(void *parser, const void *schema, void *cb, void *cb_arg);
extern int sceNp_9B87B19B(void **ticket_out);
extern int sceNp_B2BFADB2(void *jid, const void *npid, void *work_buf, u32 work_len, u32 *req_len);
extern int sceNp_B819A0C8(void *user, void *pass, void *opt);
extern int sceNp_C0B3616C(void *npid, const void *jid);
extern int sceNp_C250A650(void);
extern int sceNp_C48F2847(const void *profile, const void *lang, const void *flags, const void *country);
extern int sceNp_CF83CC3B(int val);
extern int sceNp_E24DA399(void);
extern int sceNp_E41288E7(char *dst, u32 dst_len, const char *src, u32 src_len, u32 *req_len);
extern int sceNp_E90DFBC4(const void *title_id, char *out, u32 out_len);
extern int sceNp_F2B95034(void *online_id, void *country, void *lang_out, void *region_out, void *extra_out);

extern int sceNpAuthGetTicket(int req_id, void *out_buf, int len);
extern int sceNpAuthGetTicketParam(const void *ticket, int ticket_len, int ticket_field, void *out);
extern int sceNpAuth_61BB18B3(int req_id);
extern int sceNpAuthDestroyRequest(int req_id);
extern int sceNpAuth_B714FBDD(int req_id, const void *user, const void *pass);
extern int sceNpAuth_CE85B3B8(int *req_id, const void *ver, const char *service_id, int flag);
extern int sceNpAuthAbortRequest(int req_id);
extern int sceNpAuth_DAD65284(int req_id, int flags, void *cb, void *cb_arg);

/* Shared VPL allocator (np_service_main.c) */
extern void *sceNpService_C5094BE6(SceSize size);
extern int sceNpService_61AB2062(void *block);
extern int np_base64_encode(char *out, const u8 *in, u32 len);
extern const char *np_manager_get_env(void);

/* Shared XML helpers (np_xml.c) */
extern int np_xml_feed(void *parser, const void *xml_chunk, u32 len);
extern int np_xml_profile_init(void *parser, int parse_kind, NpXmlProfileCtx *ctx);
extern int np_xml_profile_check(NpXmlProfileCtx *ctx);
extern int np_xml_fqdn_init(void *parser, int parse_kind, const void *title_id, NpXmlFqdnCtx *ctx);
extern int np_xml_fqdn_check(NpXmlFqdnCtx *ctx);
extern int np_xml_searchjid_init(void *parser, NpXmlSearchJidCtx *ctx);
extern int np_xml_searchjid_check(NpXmlSearchJidCtx *ctx);
extern int np_xml_uploader_list_init(void *parser, NpXmlUploaderListCtx *ctx);
extern int np_xml_uploader_list_check(NpXmlUploaderListCtx *ctx);
extern int np_xml_uploader_dl_init(void *parser, NpXmlUploaderDlCtx *ctx);
extern int np_xml_uploader_dl_check(NpXmlUploaderDlCtx *ctx);
extern int np_xml_avatar_cat_init(void *parser, NpXmlAvatarCatCtx *ctx);
extern int np_xml_avatar_cat_check(NpXmlAvatarCatCtx *ctx);
extern int np_xml_avatar_list_init(void *parser, NpXmlAvatarListCtx *ctx);
extern int np_xml_avatar_list_check(NpXmlAvatarListCtx *ctx);
extern int np_validate_online_id(const char *str);
extern int np_verify_hmac_sig(const void *payload, int len, const u8 *sig, int sig_kind);
extern int np_http_read_exact(NpTransCtx *trans, void *out_buf, u32 len, u32 *read_out);
extern int np_http_parse_headers(NpTransCtx *trans, int *status_out, u64 *clen_out, u32 *hdr_info_len_out, u8 *sig_out, int sig_kind);
extern int np_jid_str_to_npid(NpId *npid, const char *jid_str);
extern int np_npid_to_jid_str(char **out_str, const NpId *npid);
extern int np_xml_escape_str(char **out_str, const char *src);
extern int np_map_ssl_error(int req_id, int err);
extern s64 np_parse_s64(const char *str);
extern int np_trans_close_http(NpTransCtx *trans);
extern int np_free_fqdn_list(NpFqdnEntry *head);

/* Shared HTTP/XML transaction subsystem (np_profile.c) */
extern int np_httpxml_init(void);
extern int np_httpxml_term(void);
extern int np_httpxml_is_init(void);
extern void *np_alloc(SceSize size);
extern void np_free(void *block);
extern int sceNpService_7EF4312E(const void *online_id, int trans_kind, const void *extra_arg);
extern int np_trans_create_lookup(const void *online_id, int trans_kind, const void *extra_arg);
extern int np_user_set_env_country(const void *online_id, const char *env, const char *country);
extern int sceNpService_76867C01(int trans_id, NpFqdnEntry **list_out);
extern int sceNpService_00041295(int trans_id, NpId *npid_out);
extern int sceNpService_E00CDEBB(int trans_id, const NpId *npid, int avatar_size, NpProfileRecord *profile_out);
extern int np_get_self_profile(int trans_id, NpProfileRecord *profile_out, int *reserved_out, int *ptlp_out, int *upd_out, u64 *date_out, const u8 *ticket, u32 ticket_len, int avatar_size);
extern int sceNpService_43E635D2(int trans_id);
extern int sceNpService_D97B509F(const void *online_id);
extern int sceNpService_5B74DE06(const void *online_id);
extern int sceNpService_DD8B5E53(int trans_id);
extern int np_trans_destroy_lookup(int trans_id);
extern int np_trans_download_url(int trans_id, const char *url, u32 max_size, u32 *total_out, u32 chunk_size, void *out_buf, int opt);
extern int np_trans_acquire(int trans_id, NpTransCtx **out_trans);
extern int np_trans_release(NpTransCtx *trans);
extern int np_trans_track_alloc(NpTransCtx *trans, void *block);

/* Shared Roster init/term (np_roster.c) */
extern int np_roster_init(void);
extern int np_roster_term(void);

#endif /* NP_SERVICE_INTERNAL_H */
