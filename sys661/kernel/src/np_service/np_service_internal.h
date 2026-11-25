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

static inline u16 bswap16(u16 x) { return (u16)((x >> 8) | (x << 8)); }

/* Kernel_Library */
extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int intr);
extern void *sceKernelMemset(void *dst, int c, SceSize len);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize len);
extern int sceKernelLockLwMutex(void *workarea, int count, u32 *timeout);
extern int sceKernelUnlockLwMutex(void *workarea, int count);
extern int _sceKernelCheckThreadStack(void);

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
extern int sceKernelTryAllocateVpl(SceUID vplid, SceSize size, void **data);
extern int sceKernelFreeVpl(SceUID vplid, void *data);
extern int sceKernelReferVplStatus(SceUID vplid, void *info);

/* sceRtc */
extern int sceRtcGetCurrentTick(u64 *tick);
extern int sceRtc_508BA64B(const u64 *tick);
extern int sceRtcParseDateTime(u64 *tick, const char *str);

/* sceNet / sceNet_lib */
extern int sceNetFreeThreadinfo(SceUID thid);
extern int sceNet_lib_1FB2FDDD(const char *s);
extern u32 sceNet_lib_2A73ADDC(const char *s, char **endptr, int base);
extern int sceNet_lib_384EFE14(const void *data, int data_len, const void *key, int key_len, void *out);
extern int sceNet_lib_5705F6F9(void);
extern char *sceNet_lib_80C9F02A(char *dst, const char *src);
extern int sceNet_lib_94DCA9F0(const char *s1, const char *s2, int n);
extern int sceNet_lib_A0F16ABD(const char *s1, const char *s2);
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, int n);
extern int sceNet_lib_B9085A96(const char *s1, const char *s2, int n);
extern int sceNet_lib_CF705E46(char *buf, const char *fmt, ...);
extern int sceNet_lib_D8722983(const char *s);
extern int sceNet_lib_E0A81C7C(const void *s1, const void *s2, int n);
extern u8 sceNet_lib_750F705D(int c);

/* sceParseHttp */
extern int sceParseHttpResponseHeader(const void *hdr, u32 hdr_len, const char *field, const char **val, u32 *val_len);

/* sceHttp */
extern int sceHttpCreateTemplate(const char *agent, int httpver, int proxy);
extern int sceHttpDeleteTemplate(int tid);
extern int sceHttpCreateConnection(int tid, const char *host, const char *scheme, u16 port, int keepalive);
extern int sceHttpCreateConnectionWithURL(int tid, const char *url, int keepalive);
extern int sceHttpDeleteConnection(int cid);
extern int sceHttpCreateRequest(int cid, int method, const char *path, u64 content_len);
extern int sceHttpCreateRequestWithURL(int cid, int method, const char *url, u64 content_len);
extern int sceHttpDeleteRequest(int rid);
extern int sceHttpSendRequest(int rid, const void *data, u32 len);
extern int sceHttpAbortRequest(int rid);
extern int sceHttpReadData(int rid, void *buf, u32 len);
extern int sceHttpGetStatusCode(int rid, int *code);
extern int sceHttpGetContentLength(int rid, u64 *len);
extern int sceHttpGetAllHeader(int rid, const char **hdr, u32 *hdr_len);
extern int sceHttpAddExtraHeader(int id, const char *name, const char *val, int mode);
extern int sceHttpDeleteHeader(int id, const char *name);
extern int sceHttpSetConnectTimeOut(int id, u32 usec);
extern int sceHttpSetRecvTimeOut(int id, u32 usec);
extern int sceHttpSetSendTimeOut(int id, u32 usec);
extern int sceHttpSetAuthInfoCallback(int id, void *cb, void *arg);
extern int sceHttpsGetSslError(int id, int *err, u32 *detail);
extern int sceHttp_3C478044(int id, int val);
extern int sceHttp_87F1E666(int id, int val);

/* sceNpCore / sceNp / sceNpAuth */
extern int sceNpCore_52440ABF(char *env);
extern int sceNp_02CA8CAA(void *ticket, int len, const u64 *tick);
extern int sceNp_0B8BEE48(const void *jid, char *out, u32 out_len, u32 *req_len);
extern int sceNp_1916432C(void *parser, const void *data, u32 len);
extern int sceNp_4B09907A(char *dst, const char *src, u32 size);
extern int sceNp_5FA879D8(void *jid, const char *str, void *buf, u32 buf_len, u32 *req_len);
extern int sceNp_64F72B22(void *dst, const char *str);
extern int sceNp_66B86876(const void *user, const void *pass, u32 lo, u32 hi);
extern int sceNp_67BCF9E3(int flags, u16 *ticket_len);
extern int sceNp_6999EDC4(void *parser, const void *schema, void *cb, void *arg);
extern int sceNp_9B87B19B(void **ticket_out);
extern int sceNp_B2BFADB2(void *jid, const void *npid, void *buf, u32 buf_len, u32 *req_len);
extern int sceNp_B819A0C8(void *user, void *pass, void *opt);
extern int sceNp_C0B3616C(void *npid, const void *jid);
extern int sceNp_C250A650(void);
extern int sceNp_C48F2847(const void *profile, const void *lang, const void *flags, const void *country);
extern int sceNp_CF83CC3B(int val);
extern int sceNp_E24DA399(void);
extern int sceNp_E41288E7(char *dst, u32 dst_len, const char *src, u32 src_len, u32 *req_len);
extern int sceNp_E90DFBC4(const void *title_id, char *out, u32 out_len);
extern int sceNp_F2B95034(void *online_id, void *country, void *a3, void *a4, void *a5);

extern int sceNpAuthGetTicket(int req_id, void *buf, int len);
extern int sceNpAuthGetTicketParam(const void *ticket, int ticket_len, int param_id, void *out);
extern int sceNpAuth_61BB18B3(int req_id);
extern int sceNpAuthDestroyRequest(int req_id);
extern int sceNpAuth_B714FBDD(int req_id, const void *user, const void *pass);
extern int sceNpAuth_CE85B3B8(int *req_id, const void *ver, const char *service_id, int flag);
extern int sceNpAuthAbortRequest(int req_id);
extern int sceNpAuth_DAD65284(int req_id, int flags, void *cb, void *arg);

/* Shared VPL allocator (np_service_main.c) */
extern void *sceNpService_C5094BE6(SceSize size);
extern int sceNpService_61AB2062(void *ptr);
extern int np_base64_encode(char *out, const u8 *in, u32 len);
extern const char *np_manager_get_env(void);

/* Shared XML helpers (np_xml.c) */
extern int np_xml_feed(void *parser, const void *data, u32 len);
extern int np_xml_profile_init(void *parser, int mode, void *ctx);
extern int np_xml_profile_check(void *ctx);
extern int np_xml_fqdn_init(void *parser, int mode, void *title_id, void *ctx);
extern int np_xml_fqdn_check(void *ctx);
extern int np_xml_searchjid_init(void *parser, void *ctx);
extern int np_xml_searchjid_check(void *ctx);
extern int np_xml_uploader_list_init(void *parser, void *ctx);
extern int np_xml_uploader_list_check(void *ctx);
extern int np_xml_uploader_dl_init(void *parser, void *ctx);
extern int np_xml_uploader_dl_check(void *ctx);
extern int np_xml_avatar_cat_init(void *parser, void *ctx);
extern int np_xml_avatar_cat_check(void *ctx);
extern int np_xml_avatar_list_init(void *parser, void *ctx);
extern int np_xml_avatar_list_check(void *ctx);
extern int np_validate_online_id(const char *s);
extern int np_verify_hmac_sig(const void *data, int len, const u8 *sig, int mode);
extern int np_http_read_exact(void *trans, void *buf, u32 len, u32 *read_out);
extern int np_http_parse_headers(void *trans, int *status_out, u64 *clen_out, u32 *info_len_out, u8 *sig_out, int mode);
extern int np_jid_str_to_npid(void *npid, const char *jid_str);
extern int np_npid_to_jid_str(char **out_str, const void *npid);
extern int np_xml_escape_str(char **out_str, const char *src);
extern int np_map_ssl_error(int req_id, int err);
extern s64 np_parse_s64(const char *s);
extern int np_trans_close_http(void *trans);
extern int np_free_fqdn_list(void *head);

/* Shared HTTP/XML transaction subsystem (np_profile.c) */
extern int np_httpxml_init(void);
extern int np_httpxml_term(void);
extern int np_httpxml_is_init(void);
extern void *np_alloc(SceSize size);
extern void np_free(void *ptr);
extern int sceNpService_7EF4312E(const void *online_id, int type, const void *arg);
extern int np_trans_create_lookup(const void *online_id, int type, const void *arg);
extern int np_user_set_env_country(const void *online_id, const char *env, const char *country);
extern int sceNpService_76867C01(int trans_id, void **list_out);
extern int sceNpService_00041295(int trans_id, void *npid_out);
extern int sceNpService_E00CDEBB(int trans_id, const void *npid, int avatar_size, void *profile_out);
extern int np_get_self_profile(int trans_id, void *profile_out, int *a3, int *ptlp_out, int *upd_out, u64 *date_out, const u8 *ticket, u32 ticket_len, int avatar_size);
extern int sceNpService_43E635D2(int trans_id);
extern int sceNpService_D97B509F(const void *online_id);
extern int sceNpService_5B74DE06(const void *online_id);
extern int sceNpService_DD8B5E53(int trans_id);
extern int np_trans_destroy_lookup(int trans_id);
extern int np_trans_download_url(int trans_id, const char *url, u32 max_size, u32 *total_out, u32 chunk_size, void *buf, int opt);
extern int np_trans_acquire(int trans_id, int **out_trans);
extern int np_trans_release(void *trans);
extern int np_trans_track_alloc(void *trans, void *ptr);

/* Shared Roster init/term (np_roster.c) */
extern int np_roster_init(void);
extern int np_roster_term(void);

#endif /* NP_SERVICE_INTERNAL_H */
