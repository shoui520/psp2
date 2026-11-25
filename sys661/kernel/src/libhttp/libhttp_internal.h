#ifndef LIBHTTP_INTERNAL_H
#define LIBHTTP_INTERNAL_H

#include <stddef.h>

typedef unsigned char      u8;
typedef signed char        s8;
typedef unsigned short     u16;
typedef signed short       s16;
typedef unsigned int       u32;
typedef signed int         s32;
typedef unsigned long long u64;
typedef signed long long   s64;

/* -------------------------------------------------------------------------
 * HTTP / HTTPS error codes
 * ------------------------------------------------------------------------- */
#define SCE_HTTP_ERROR_BEFORE_INIT          ((s32)0x80431001)
#define SCE_HTTP_ERROR_NOT_SUPPORTED        ((s32)0x80431004)
#define SCE_HTTP_ERROR_ALREADY_INITED       ((s32)0x80431020)
#define SCE_HTTP_ERROR_BUSY                 ((s32)0x80431021)
#define SCE_HTTP_ERROR_OUT_OF_MEMORY        ((s32)0x80431022)
#define SCE_HTTP_ERROR_NOT_FOUND            ((s32)0x80431025)
#define SCE_HTTP_ERROR_TOO_MANY_HANDLES     ((s32)0x80431060)
#define SCE_HTTP_ERROR_UNKNOWN_SCHEME       ((s32)0x80431061)
#define SCE_HTTP_ERROR_NETWORK              ((s32)0x80431063)
#define SCE_HTTP_ERROR_BAD_RESPONSE         ((s32)0x80431064)
#define SCE_HTTP_ERROR_BEFORE_SEND          ((s32)0x80431065)
#define SCE_HTTP_ERROR_AFTER_SEND           ((s32)0x80431066)
#define SCE_HTTP_ERROR_STILL_IN_USE         ((s32)0x80431067)
#define SCE_HTTP_ERROR_TIMEOUT              ((s32)0x80431068)
#define SCE_HTTP_ERROR_UNKNOWN_AUTH_TYPE    ((s32)0x80431069)
#define SCE_HTTP_ERROR_UNKNOWN_VERSION      ((s32)0x8043106A)
#define SCE_HTTP_ERROR_UNKNOWN_METHOD       ((s32)0x8043106B)
#define SCE_HTTP_ERROR_ABORTED              ((s32)0x8043106C)
#define SCE_HTTP_ERROR_NOT_CURRENT_REQ      ((s32)0x8043106E)
#define SCE_HTTP_ERROR_HEAD_NO_BODY         ((s32)0x8043106F)
#define SCE_HTTP_ERROR_NOT_CONNECTED        ((s32)0x80431070)
#define SCE_HTTP_ERROR_NO_CONTENT_LENGTH    ((s32)0x80431071)
#define SCE_HTTP_ERROR_CHUNK_ENC            ((s32)0x80431072)
#define SCE_HTTP_ERROR_SSL                  ((s32)0x80431075)
#define SCE_HTTP_ERROR_OUT_OF_STACK         ((s32)0x80431076)
#define SCE_HTTP_ERROR_POOL_TOO_SMALL       ((s32)0x80431077)
#define SCE_HTTP_ERROR_SSL_NOT_INITED       ((s32)0x80431079)
#define SCE_HTTP_ERROR_INVALID_ID           ((s32)0x80431100)
#define SCE_HTTP_ERROR_OUT_OF_SIZE          ((s32)0x80431104)
#define SCE_HTTP_ERROR_INVALID_VALUE        ((s32)0x804311FE)

#define SCE_HTTPS_ERROR_INTERNAL            ((s32)0x80435022)
#define SCE_HTTPS_ERROR_CERT                ((s32)0x80435060)
#define SCE_HTTPS_ERROR_HANDSHAKE           ((s32)0x80435061)
#define SCE_HTTPS_ERROR_IO                  ((s32)0x80435062)
#define SCE_HTTPS_ERROR_PROXY               ((s32)0x80435064)

#define SCE_KERNEL_ERROR_NO_MOD             ((s32)0x8002013A)
#define SCE_CERTLOADER_ERROR_END            ((s32)0x80000025)

/* Handle type masks */
#define HTTP_HANDLE_MASK                    0x0F000000
#define HTTP_HANDLE_TEMPLATE                0x00000000
#define HTTP_HANDLE_CONNECTION              0x01000000
#define HTTP_HANDLE_REQUEST                 0x02000000

/* HTTP methods */
#define HTTP_METHOD_GET                     0
#define HTTP_METHOD_POST                    1
#define HTTP_METHOD_HEAD                    2
#define HTTP_METHOD_OPTIONS                 3
#define HTTP_METHOD_PUT                     4
#define HTTP_METHOD_DELETE                  5
#define HTTP_METHOD_TRACE                   6
#define HTTP_METHOD_CONNECT                 7

/* HTTP schemes */
#define HTTP_SCHEME_HTTP                    0
#define HTTP_SCHEME_HTTPS                   1

/* Connection states */
#define HTTP_CONN_IDLE                      0
#define HTTP_CONN_RESOLVING                 1
#define HTTP_CONN_CONNECTING                2
#define HTTP_CONN_PROXY_TUNNEL              3
#define HTTP_CONN_ESTABLISHED               4
#define HTTP_CONN_SENDING                   5
#define HTTP_CONN_SENDING_BODY              6
#define HTTP_CONN_CLOSING                   7

/* Request states */
#define HTTP_REQ_CREATED                    0
#define HTTP_REQ_PROXY_AUTH_FAIL            1
#define HTTP_REQ_SENDING_BODY               2
#define HTTP_REQ_SENT                       3
#define HTTP_REQ_RECEIVING                  4
#define HTTP_REQ_DONE                       5
#define HTTP_REQ_EOF                        6
#define HTTP_REQ_ERROR                      7
#define HTTP_REQ_ABORTED                    8

/* Auth types */
#define HTTP_AUTH_BASIC                     0
#define HTTP_AUTH_DIGEST                    1
#define HTTP_AUTH_PROXY_BASIC               2
#define HTTP_AUTH_PROXY_DIGEST              3

/* Cache modes */
#define HTTP_CACHE_NONE                     0
#define HTTP_CACHE_MEM                      1
#define HTTP_CACHE_POOL                     2

/* Default HTTPS verification flags */
#define HTTPS_DEFAULT_OPTIONS               0x3D

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */
static inline u16 http_bswap16(u16 val)
{
    return (u16)((val << 8) | (val >> 8));
}

static inline s32 http_min_s32(s32 lhs, s32 rhs)
{
    return lhs < rhs ? lhs : rhs;
}

/* -------------------------------------------------------------------------
 * Data structures
 * ------------------------------------------------------------------------- */
typedef struct SceUriElement {
    u32   opaque;
    char *scheme;
    char *userinfo;
    char *password;
    char *host;
    char *path;
    char *query;
    char *fragment;
    u16   port;
    u16   pad;
    u32   reserved;
} SceUriElement;

typedef struct HttpHeaderNode {
    char                  *name;
    char                  *value;
    struct HttpHeaderNode *next;
} HttpHeaderNode;

typedef struct HttpSockAddrIn {
    u8  len;
    u8  family;
    u16 port;
    u32 addr;
    u8  zero[8];
} HttpSockAddrIn;

typedef struct HttpTemplate {
    s32             id;
    s32             http_version;
    s32             ref_count;
    s32             resolve_retry;
    s32             use_proxy;
    s32             cache_mode;
    u32             resolve_timeout;
    u32             connect_timeout;
    u32             recv_timeout;
    u32             send_timeout;
    u32             res_header_max_size;
    u32             recv_block_size;
    u32             cache_content_max_size;
    u32             reserved_34;
    u32             max_redirects;
    u32             max_auth_retries;
    char           *proxy_server;
    char           *auth_user;
    char           *auth_pass;
    s32             keep_alive;
    s32             auto_redirect;
    s32             cookie_enabled;
    s32             auth_enabled;
    s32             tcp_nodelay;
    u16             use_default_proxy;
    u16             allow_http09;
    u32             ssl_flags;
    u32             ssl_version;
    void           *auth_cb_legacy;
    void           *auth_cb;
    void           *auth_cb_arg;
    void           *redirect_cb;
    void           *redirect_cb_arg;
    void           *cookie_send_cb;
    void           *cookie_send_cb_arg;
    void           *cookie_recv_cb;
    void           *cookie_recv_cb_arg;
    void           *ssl_cb;
    void           *ssl_cb_arg;
    HttpHeaderNode *headers;
    u32             reserved_9c;
    u32             reserved_a0;
    struct HttpTemplate *next;
    struct HttpTemplate *prev;
    u16             proxy_port;
    u16             proxy_auth_enabled;
} HttpTemplate;

typedef struct HttpConnection {
    s32                    id;
    s32                    scheme;
    s32                    ref_count;
    s32                    pipelined_count;
    s32                    current_req_id;
    s32                    tail_req_id;
    s32                    active_post_req_id;
    s32                    resolver_id;
    s32                    net_errno;
    s32                    net_psp_err;
    s32                    ssl_err;
    u32                    ssl_cert_flags;
    s32                    sock_fd;
    s32                    state;
    char                  *host;
    u32                    reserved_3c;
    s32                    resolved;
    u32                    reserved_44;
    s32                    close_on_complete;
    void                  *ssl_handle;
    void                  *ssl_ctx;
    s32                    ssl_handshake_done;
    u32                    reserved_58[22];
    HttpTemplate           cfg;
    HttpSockAddrIn         addr;
    u8                    *recv_buf;
    u32                    recv_buf_size;
    u32                    recv_pos;
    u32                    recv_end;
    s32                    peer_closed;
    struct HttpConnection *redir_chain_next;
    struct HttpConnection *next;
    struct HttpConnection *prev;
    u16                    port;
    u16                    pad;
} HttpConnection;

typedef struct HttpResponseInfo {
    u64   content_length;
    s32   has_content_length;
    s32   is_chunked;
    s32   conn_close;
    s32   http_major;
    s32   http_minor;
    s32   status_code;
    char *reason_ptr;
    u32   reason_len;
    char *etag_ptr;
    u32   etag_len;
    char *server_ptr;
    u32   server_len;
    char *date_ptr;
    u32   date_len;
    char *last_mod_ptr;
    u32   last_mod_len;
    char *location_ptr;
    u32   location_len;
    char *www_auth_ptr;
    u32   www_auth_len;
    char *conn_hdr_ptr;
    u32   conn_hdr_len;
} HttpResponseInfo;

typedef struct HttpAuthChallenge {
    s32   type;
    char *realm;
    u32   realm_len;
    char *qop;
    u32   qop_len;
    char *nonce;
    u32   nonce_len;
    char *opaque;
    u32   opaque_len;
    char *algorithm;
    u32   algorithm_len;
    char *domain;
    u32   domain_len;
    s32   stale;
} HttpAuthChallenge;

typedef struct HttpDigestParams {
    char *realm;
    char *qop;
    char *nonce;
    char *opaque;
    char *algorithm;
    char *domain;
    u32   nc;
    void *buf;
} HttpDigestParams;

typedef struct HttpAuthEntry {
    u32                   hit_count;
    s32                   ref_count;
    s32                   status;
    s32                   auth_type;
    s32                   scheme;
    u16                   port;
    u16                   pad;
    char                 *host;
    char                 *path;
    u8                    realm_md5[16];
    char                  username[256];
    char                  password[256];
    s32                   save_flag;
    s32                   deleted;
    HttpDigestParams      digest;
    struct HttpAuthEntry *prev;
    struct HttpAuthEntry *next;
    struct HttpAuthEntry *free_next;
} HttpAuthEntry;

typedef struct HttpRedirEntry {
    u32                    hit_count;
    s32                    ref_count;
    u8                     url_md5[16];
    char                  *src_url;
    s32                    method;
    u8                     extra_md5[16];
    char                  *dst_url;
    s32                    deleted;
    struct HttpRedirEntry *prev;
    struct HttpRedirEntry *next;
    struct HttpRedirEntry *free_next;
} HttpRedirEntry;

typedef struct HttpCacheBlock {
    struct HttpCacheBlock *next;
    u32                    used;
    u8                     data[504];
} HttpCacheBlock;

typedef struct HttpCacheData {
    u64   timestamp;
    u32   reserved_08;
    u32   body_size;
    u32   header_size;
    u32   written_size;
    void *extra_buf;
    void *header_buf;
    void *body_buf;
    u32   reserved_24;
} HttpCacheData;

typedef struct HttpCacheEntry {
    u32                    hit_count;
    s32                    ref_count;
    u8                     url_md5[16];
    char                  *url;
    s32                    method;
    u8                     extra_md5[16];
    HttpCacheData         *data;
    s32                    deleted;
    struct HttpCacheEntry *lru_prev;
    struct HttpCacheEntry *lru_next;
    struct HttpCacheEntry *hash_next;
} HttpCacheEntry;

typedef struct HttpRequest {
    u64                 req_content_len;
    u64                 req_sent_bytes;
    u64                 res_read_bytes;
    s32                 id;
    s32                 method;
    s32                 ref_count;
    s32                 net_errno;
    s32                 net_psp_err;
    s32                 next_pipelined_id;
    s32                 prev_pipelined_id;
    s32                 ssl_err;
    u32                 ssl_cert_flags;
    s32                 state;
    s32                 cache_hit_state;
    u32                 res_header_len;
    u32                 redirect_count;
    u32                 auth_retry_count;
    u32                 post_chunk_len;
    u32                 cache_read_pos;
    u32                 chunk_remaining;
    u32                 all_header_len;
    s32                 force_close;
    u32                 reserved_64;
    s32                 sending_body;
    char               *url_full;
    char               *path;
    char               *query;
    char               *fragment;
    char               *res_header_buf;
    char               *all_header_buf;
    const void         *post_data;
    HttpTemplate        cfg;
    char                auth_user_buf[256];
    char                auth_pass_buf[256];
    HttpResponseInfo    res_info;
    HttpConnection     *conn;
    HttpConnection     *redir_conn;
    HttpCacheEntry     *cache_read_entry;
    HttpCacheEntry     *cache_write_entry;
    HttpAuthEntry      *auth_entry;
    HttpAuthEntry      *proxy_auth_entry;
    struct HttpRequest *next;
    struct HttpRequest *prev;
} HttpRequest;

typedef struct HttpCookie {
    char              *name;
    char              *value;
    char              *domain;
    char              *path;
    char              *comment;
    char              *comment_url;
    s32                secure;
    s32                discard;
    s32                deleted;
    u16               *ports;
    u32                port_count;
    s32                version;
    u64                expires;
    u32                size_bytes;
    s32                is_v2;
    struct HttpCookie *next;
    u32                reserved_44;
} HttpCookie;

typedef struct HttpCookieDomain {
    struct HttpCookieDomain *hash_next;
    struct HttpCookieDomain *hash_prev;
    struct HttpCookieDomain *lru_next;
    struct HttpCookieDomain *lru_prev;
    HttpCookie              *cookies;
    s32                      deleted;
    s32                      ref_count;
    u32                      cookie_count;
    char                    *domain;
} HttpCookieDomain;

typedef struct HttpCookieSortNode {
    HttpCookie                *cookie;
    struct HttpCookieSortNode *next;
    struct HttpCookieSortNode *prev;
} HttpCookieSortNode;

typedef struct HttpCookieSortChunk {
    HttpCookieSortNode          nodes[10];
    s32                         count;
    struct HttpCookieSortChunk *next;
} HttpCookieSortChunk;

typedef struct HttpCookieRefNode {
    HttpCookieDomain         *domain;
    struct HttpCookieRefNode *next;
} HttpCookieRefNode;

typedef struct HttpCertSlot {
    const void *data;
    u32         size;
} HttpCertSlot;

typedef struct HttpCertIter {
    u32 group;
    u32 bit;
    u32 advance_group;
} HttpCertIter;

typedef struct HttpCertNode {
    char                *pem;
    struct HttpCertNode *next;
} HttpCertNode;

/* -------------------------------------------------------------------------
 * Shared global state
 * ------------------------------------------------------------------------- */
extern const u8 g_oid_subject_alt_name[4];
extern const u8 g_b64_decode_tab[256];
extern const u8 g_md5_empty_digest[16];

/* Auth cache globals (http_core.c) */
extern s32            g_auth_inited;
extern HttpAuthEntry  g_auth_pool[16];
extern HttpAuthEntry *g_auth_free_list;
extern HttpAuthEntry *g_auth_head;
extern HttpAuthEntry *g_auth_tail;

/* HTTPS globals (http_ssl.c) */
extern u32            g_https_options;
extern s32            g_https_inited;
extern s32            g_https_ca_count;
extern void         **g_https_ca_list;
extern void          *g_https_client_cert;
extern u32            g_https_client_cert_len;
extern char          *g_https_client_key;

/* Cookie jar globals (http_cookie.c) */
extern s32               g_cookie_inited;
extern s32               g_cookie_loaded;
extern u32               g_cookie_max_bytes;
extern u32               g_cookie_cur_bytes;
extern u32               g_cookie_max_single;
extern u32               g_cookie_max_meta;
extern u32               g_cookie_max_per_domain;
extern u32               g_cookie_max_total;
extern u32               g_cookie_cur_total;
extern HttpCookieDomain *g_cookie_lru_head;
extern HttpCookieDomain *g_cookie_lru_tail;
extern HttpCookieDomain *g_cookie_buckets[16];

/* Cache globals (http_io.c) */
extern HttpCacheEntry *g_cache_mem_buckets[16];
extern HttpCacheEntry *g_cache_pool_buckets[16];
extern s32             g_cache_mem_inited;
extern u32             g_cache_mem_max_size;
extern u32             g_cache_mem_cur_size;
extern HttpCacheEntry *g_cache_mem_head;
extern HttpCacheEntry *g_cache_mem_tail;
extern s32             g_cache_pool_inited;
extern u32             g_cache_pool_base;
extern u32             g_cache_pool_size;
extern u32             g_cache_pool_used_blocks;
extern HttpCacheBlock *g_cache_pool_free_list;
extern HttpCacheEntry *g_cache_pool_head;
extern HttpCacheEntry *g_cache_pool_tail;

/* -------------------------------------------------------------------------
 * Imported library functions
 * ------------------------------------------------------------------------- */
extern s32   _sceKernelCheckThreadStack(void);
extern s32   sceCertLoader_DD629A24(u32 group, u32 bit, void *out_list, void *buf, u32 buf_size, u32 *out_size);
extern s32   sceHttpStorage_driver_24AA94F4(s32 id, const void *buf, s32 size);
extern s32   sceHttpStorage_driver_2D8DAE58(s32 id, void *stat_buf);
extern s32   sceHttpStorage_driver_700AAD44(s32 id, s32 flags, s32 mode);
extern s32   sceHttpStorage_driver_B33389CE(s32 id, s32 pad, s32 lo, s32 hi, s32 whence);
extern s32   sceHttpStorage_driver_CDA3D8F6(s32 id);
extern s32   sceHttpStorage_driver_CDDF1103(s32 id, void *buf, s32 size);
extern s32   sceKernelCpuResumeIntr(s32 intr);
extern s32   sceKernelCpuSuspendIntr(void);
extern s32   sceKernelCreateVpl(const char *name, s32 part, u32 attr, u32 size, void *opt);
extern s32   sceKernelDelayThread(u32 usec);
extern s32   sceKernelDeleteVpl(s32 vpl_id);
extern s32   sceKernelFreeVpl(s32 vpl_id, void *ptr);
extern s32   sceKernelGetCompiledSdkVersion(void);
extern u32   sceKernelGetSystemTimeLow(void);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 len);
extern void *sceKernelMemset(void *dst, s32 val, u32 len);
extern s32   sceKernelQueryMemoryInfo(void *addr, u32 *part_out, void *attr_out);
extern s32   sceKernelTryAllocateVpl(s32 vpl_id, u32 size, void **ptr_out);
extern s32   sceKernelUtilsMd5BlockInit(void *ctx);
extern s32   sceKernelUtilsMd5BlockResult(void *ctx, u8 *digest);
extern s32   sceKernelUtilsMd5BlockUpdate(void *ctx, const void *data, u32 len);
extern s32   sceKernelUtilsMd5Digest(const void *data, u32 len, u8 *digest);
extern s32   sceNetApctlGetInfo(s32 code, void *info);
extern s32   sceNetInetClose(s32 sock);
extern s32   sceNetInetConnect(s32 sock, const void *addr, s32 addrlen);
extern s32   sceNetInetGetErrno(void);
extern s32   sceNetInetGetPspError(void);
extern s32   sceNetInetGetTcpcbstat(s32 *buflen, void *buf);
extern s32   sceNetInetGetsockname(s32 sock, void *addr, s32 *addrlen);
extern s32   sceNetInetGetsockopt(s32 sock, s32 level, s32 optname, void *optval, s32 *optlen);
extern s32   sceNetInetInetPton(s32 af, const char *src, void *dst);
extern s32   sceNetInetRecv(s32 sock, void *buf, u32 len, s32 flags);
extern s32   sceNetInetSelect(s32 nfds, void *rfds, void *wfds, void *efds, void *timeout);
extern s32   sceNetInetSend(s32 sock, const void *buf, u32 len, s32 flags);
extern s32   sceNetInetSetsockopt(s32 sock, s32 level, s32 optname, const void *optval, s32 optlen);
extern s32   sceNetInetSocket(s32 domain, s32 type, s32 proto);
extern s32   sceNetInetSocketAbort(s32 sock);
extern s32   sceNetResolverCreate(s32 *rid, void *buf, u32 buflen);
extern s32   sceNetResolverDelete(s32 rid);
extern s32   sceNetResolverStartNtoA(s32 rid, const char *hostname, void *addr, u32 timeout, s32 retry);
extern s32   sceNetResolverStop(s32 rid);

/* sceNet_lib C string/ctype helpers */
extern s32   sceNet_lib_1FB2FDDD(const char *str);                             /* atoi */
extern u32   sceNet_lib_2A73ADDC(const char *str, char **endp, s32 base);      /* strtoul */
extern char *sceNet_lib_5705F6F9(char *dst, const char *src);                  /* strcat */
extern s32   sceNet_lib_750F705D(s32 ch);                                      /* ctype mask */
extern char *sceNet_lib_80C9F02A(char *dst, const char *src);                  /* strcpy */
extern s32   sceNet_lib_9CFBC7E3(const char *lhs, const char *rhs);            /* strcasecmp */
extern s32   sceNet_lib_A0F16ABD(const char *lhs, const char *rhs);            /* strcmp */
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, u32 n);           /* strncpy */
extern s32   sceNet_lib_B9085A96(const char *lhs, const char *rhs, u32 n);     /* strncasecmp */
extern char *sceNet_lib_BCBE14CF(const char *str, s32 ch);                     /* strchr */
extern s32   sceNet_lib_CF705E46(char *dst, const char *fmt, ...);             /* sprintf */
extern u32   sceNet_lib_D8722983(const char *str);                             /* strlen */
extern s32   sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, u32 n);     /* memcmp */
extern char *sceNet_lib_F48963C6(const char *str, s32 ch);                     /* strrchr */

extern s32   sceParseHttpResponseHeader(const char *hdr, u32 hdr_len, const char *field, char **val_out, u32 *val_len_out);
extern s32   sceParseHttpStatusLine(const char *hdr, u32 hdr_len, s32 *major, s32 *minor, s32 *code, char **reason, u32 *reason_len);
extern s32   sceRtcFormatRFC2822(char *out, const u64 *tick, s32 tz_min);
extern s32   sceRtcGetCurrentTick(u64 *tick);
extern s32   sceRtcParseDateTime(u64 *tick, const char *str);
extern s32   sceRtcTickAddSeconds(u64 *dst, const u64 *src, s64 secs);

/* sceSsl_lib functions */
extern void *sceSsl_lib_0217E73C(void *loader, const char *label, void *bio, void *cb, void *arg);
extern s32   sceSsl_lib_047AB6BB(void *lhs, void *rhs);
extern s32   sceSsl_lib_05E019A4(void *cert, u8 *tag, char **data, s32 *len);
extern void *sceSsl_lib_0861D934(u32 size);
extern s32   sceSsl_lib_0FF7BA68(void *cert, void *pubkey);
extern void *sceSsl_lib_12C7718F(void *cert);
extern void *sceSsl_lib_28B4DE33(s32 sock, s32 close_flag);
extern const char *sceSsl_lib_2B7BD166(void *oid, s32 oid_len, s32 flag);
extern s32   sceSsl_lib_341843AB(void *cert, u8 *tag, char **data, s32 *len);
extern s32   sceSsl_lib_3454E622(void *bio, const char *fmt, ...);
extern s32   sceSsl_lib_37C7B76C(void *cert, u8 *buf, s32 *len);
extern s32   sceSsl_lib_3E040FD4(void *bio);
extern s32   sceSsl_lib_3E3133D6(void *ssl);
extern void *sceSsl_lib_42FFA860(void);
extern void *sceSsl_lib_4BE9030A(void *cert);
extern s32   sceSsl_lib_4C75B8E9(void *name);
extern void *sceSsl_lib_5040E836(void *name, s32 idx);
extern void  sceSsl_lib_529A9477(void *ctx, void *cb, void *arg);
extern s32   sceSsl_lib_54A7D8F3(void *ssl);
extern void *sceSsl_lib_58352068(void);
extern void  sceSsl_lib_588F2FE8(void *ctx);
extern void  sceSsl_lib_5E5C873A(void *ptr);
extern void *sceSsl_lib_6038DEDE(void *out, const u8 **der, s32 len);
extern void  sceSsl_lib_69F1B678(void *ssl, s32 flag);
extern void  sceSsl_lib_7317C1BC(void *key);
extern s32   sceSsl_lib_76A46EDA(void *cert, const u8 *oid, s32 flag, u8 **val, s32 *len);
extern void *sceSsl_lib_7BDAF864(void *ca_list, s32 idx);
extern s32   sceSsl_lib_7D0E41F4(void *lhs, void *rhs);
extern void  sceSsl_lib_80608663(void *ssl);
extern void  sceSsl_lib_84833472(void *ssl);
extern s32   sceSsl_lib_8D484361(void *entry, s32 idx, void **oid, s32 *oid_len, u8 *tag, char **val, u32 *val_len);
extern s32   sceSsl_lib_9266C0D5(void);
extern s32   sceSsl_lib_9299C6CE(void *cert, u8 *tag, char **data, s32 *len);
extern s32   sceSsl_lib_92FA8B34(void *ca_list);
extern s32   sceSsl_lib_A7BED83B(void *time_obj);
extern s32   sceSsl_lib_A8F0AD39(void *ssl);
extern void *sceSsl_lib_A920B39E(void);
extern void  sceSsl_lib_AEBF278B(void *ctx, s32 mode, void *cb);
extern s32   sceSsl_lib_B3B04C58(void *ssl, s32 ret);
extern void *sceSsl_lib_B40D11EA(void);
extern s32   sceSsl_lib_B4D78E98(void *ctx, s32 cmd, u32 larg, void *parg);
extern s32   sceSsl_lib_B7CA8717(void *ssl, const void *buf, s32 len);
extern void *sceSsl_lib_B87FFE37(void *cert);
extern void  sceSsl_lib_B9C8CCE6(void *ssl, void *rbio, void *wbio);
extern void  sceSsl_lib_BF55C31C(void *ctx, void *cb);
extern void  sceSsl_lib_C1A2E14B(void *cert);
extern s32   sceSsl_lib_C85F6FCE(void *asn1_item, const u8 *data, s32 len);
extern s32   sceSsl_lib_CFA2F8EB(void *cert, u8 *is_ca, s32 *path_len, s32 *has_path_len);
extern s32   sceSsl_lib_D53ACF30(void *ssl);
extern void  sceSsl_lib_D5ABC603(void *ctx, s32 flag);
extern s32   sceSsl_lib_E7BFC0EF(const char *str, s32 len, s32 flag, void *time_obj);
extern s32   sceSsl_lib_E7C29542(void *ssl, void *buf, s32 len);
extern void *sceSsl_lib_EBFB0E3C(void *ctx);
extern void *sceSsl_lib_FB8273FE(void *method);

extern s32   sceUriBuild(char *dst, u32 *req_len, u32 dst_size, const SceUriElement *elem, u32 flags);
extern s32   sceUriParse(SceUriElement *elem, const char *uri, void *work, u32 *req_len, u32 work_size);
extern s32   sceUriSweepPath(char *dst, const char *src, u32 dst_size);

/* -------------------------------------------------------------------------
 * Internal module functions
 * ------------------------------------------------------------------------- */

/* http_core.c */
s32            http_base64_decode(u8 *dst, const u8 *src, s32 src_len);
s32            http_base64_encode(char *dst, const u8 *src, u32 src_len);
s32            http_send_request_core(HttpRequest *req);
HttpRedirEntry *http_follow_cached_redirects(HttpRequest *req);
s32            http_send_request_internal(s32 req_id, const void *post_data, u32 post_size);
s32            http_parse_auth_header(HttpAuthChallenge *out, const char *hdr, u32 hdr_len, s32 status_code);
s32            http_parse_auth_challenge(HttpAuthChallenge *out, const u8 *val, u32 len, s32 is_proxy);
u32            http_parse_auth_param(const u8 *src, u32 len, char **val_out, u32 *val_len_out);
s32            http_build_basic_auth(u8 *dst, u32 *req_len, u32 dst_size, const char *user, const char *pass);
s32            http_build_digest_auth(char *dst, u32 *req_len, u32 dst_size,
                                      const char *user, const char *pass, const char *uri,
                                      s32 method, const void *entity, u32 entity_len,
                                      const HttpDigestParams *params);
s32            http_init_digest_params(HttpDigestParams *dst, const HttpAuthChallenge *src);
void           http_digest_calc_ha1(const char *alg, const char *user, const char *realm,
                                    const char *pass, const char *nonce, const char *cnonce,
                                    char *ha1_hex);
void           http_digest_calc_response(const char *ha1_hex, const char *nonce, const char *nc_hex,
                                         const char *cnonce, const char *qop, const char *method,
                                         const char *uri, const char *entity_hex, char *resp_hex);
void           http_digest_make_cnonce(u32 seed, char *cnonce_hex);
s32            http_digest_hash_entity(const void *data, u32 len, char *out_hex);
s32            http_auth_cache_init(void);
s32            http_auth_cache_term(void);
void           http_auth_entry_mark_deleted(HttpAuthEntry *entry);
void           http_apply_cached_auth(HttpRequest *req);
void           http_apply_cached_proxy_auth(HttpRequest *req, HttpConnection *conn);
HttpAuthEntry *http_auth_cache_find_ready(HttpRequest *req, s32 is_proxy);

/* http_cookie.c */
HttpAuthEntry *http_auth_cache_find(HttpRequest *req, u32 auth_type, const char *realm);
HttpAuthEntry *http_auth_cache_alloc(HttpRequest *req, s32 auth_type, const char *realm);
void           http_auth_cache_insert(HttpAuthEntry *entry);
void           http_digest_params_free(HttpDigestParams *params);
void           http_auth_entry_release_locked(HttpAuthEntry *entry);
void           http_auth_entry_free_unused(HttpAuthEntry *entry);
s32            http_digest_append_kv(char *dst, u32 dst_size, u32 *written_out,
                                     const char *key, const char *val, s32 do_write);
void           http_md5_to_hex(const u8 *digest, char *hex_out);
void           http_auth_entry_release(HttpAuthEntry *entry);
s32            http_path_prefix_match(const char *prefix, const char *path);
void           http_kv_list_free(HttpHeaderNode *list);
u32            http_cookie_free_fields(HttpCookie *cookie);
s32            http_cookie_jar_insert(HttpCookie *cookies, u32 total_bytes, u32 count);
void           http_cookie_jar_init(void);
void           http_cookie_jar_term(void);
s32            http_cookie_domain_acquire(HttpCookieDomain *dom);
void           http_cookie_domain_release(HttpCookieDomain *dom);
s32            http_cookie_jar_lookup(HttpCookieSortChunk **out_chunks, const char *host,
                                      const char *path, s32 is_secure, HttpCookieRefNode **out_refs);
void           http_cookie_lookup_free(HttpCookieSortChunk *chunks, HttpCookieRefNode *refs);
s32            http_cookie_format_header(u8 *dst, HttpCookieSortChunk *chunk, u32 *len_out, u32 dst_size);
s32            http_cookie_process_response(const char *hdr, u32 hdr_len, const char *host,
                                            const char *path, u16 port, HttpRequest *req);
s32            http_cookie_add_raw(const char *header_val, u32 header_len, const char *host,
                                   const char *path, u16 port, s32 is_v2);
s32            http_cookie_ref_push(HttpCookieRefNode **head, HttpCookieDomain *dom);
s32            http_cookie_build_from_kv(HttpCookie **out_list, u32 *out_bytes, u32 *out_count,
                                         HttpHeaderNode *kv_list, const char *host, const char *path,
                                         u16 port, s32 secure, s32 discard, s32 is_v2);
s32            http_cookie_alloc_entry(HttpCookie **out_cookie, const char *name, const char *value,
                                       const char *comment, const char *comment_url,
                                       const char *domain, const char *path,
                                       const u16 *ports, u32 port_count,
                                       const u64 *expires, s32 version, s32 secure,
                                       s32 discard, s32 is_v2);
s32            http_cookie_parse_kv(const u8 *src, u32 len, HttpHeaderNode **out_list,
                                    u32 *out_secure, u32 *out_discard, s32 is_v2);
const char    *http_cookie_normalize_domain(const char *domain);
s32            http_cookie_domain_hash(const char *domain);
s32            http_cookie_match_keyword(const char *str, u32 rem, const char *kw);
void           http_cookie_evict_if_needed(u32 need_bytes, u32 need_count);
s32            http_cookie_collect_matching(HttpCookieSortChunk *head, HttpCookie *list,
                                            const char *path, s32 is_secure);
s32            http_cookie_domain_match(const char *cookie_domain, const char *host);

/* http_ssl.c */
s32            http_parse_port_list_alloc(const char *str, u16 match_port, u16 **out_ports);
char          *http_extract_dir_path(const char *path);
u32            http_domain_min_labels(const char *tld, s32 tld_len, u32 prev_label_len);
const char    *http_domain_skip_tail_labels(const char *domain, s32 count);
s32            http_domain_parse_tld(const char *domain, const char **tld_out, s32 *tld_len_out, u32 *prev_len_out);
void           http_cookie_sort_insert(HttpCookieSortNode *root, HttpCookieSortNode *node);
s32            http_parse_port_list(const char *str, u16 match_port, u16 *out_ports);
s32            http_ssl_verify_callback(void *ssl, void **certs, s32 cert_count, HttpConnection *conn, u32 *err_out);
s32            http_ssl_client_cert_callback(void *ca_list, void **cert_out, void **key_out);
s32            http_pem_to_der(const char *pem, u8 *der_out, u32 *der_len_out, u32 der_max);
s32            http_ssl_end_internal(void);
s32            http_ssl_proxy_connect(HttpRequest *req, s32 sock_fd);
s32            http_ssl_handshake(HttpRequest *req, s32 sock_fd);
s32            http_ssl_recv(HttpConnection *conn, void *buf, s32 len);
s32            http_ssl_send(HttpConnection *conn, const void *buf, s32 len);
s32            http_ssl_is_inited(void);
void           http_ssl_close_conn(HttpConnection *conn);
void           http_ssl_scan_x509_name(void *x509_name);
s32            http_ssl_match_hostname(const char *pattern, u32 pat_len, const char *host);
const char    *http_strnchr(const char *str, s32 ch, u32 max_len);
void           http_ssl_free_conn(HttpConnection *conn);
s32            http_ssl_check_cn(void *x509_name, const char *host);
s32            http_ssl_load_next_system_cert(HttpCertIter *iter, void **cert_out, u32 flags);
s32            http_handle_auth_response(HttpRequest *req);
s32            http_handle_redirect_and_auth(HttpRequest *req);
s32            http_handle_redirect(HttpRequest *req);
s32            http_retry_with_auth(HttpRequest *req);
s32            http_drain_and_reset_request(HttpRequest *req);

/* http_opts.c */
char          *http_resolve_relative_path(const char *base_path, const char *rel_path);
void          *http_malloc(u32 size);
void           http_free(void *ptr);
s32            http_pool_init(u32 size);
void           http_pool_term(void);
s32            http_pool_get_size(void);
s32            http_add_header_internal(s32 id, const char *name, const char *value, s32 mode);
s32            http_header_list_remove(HttpHeaderNode **head, const char *name);
s32            http_delete_header_internal(s32 id, const char *name);
s32            http_set_cache_mode(s32 id, s32 mode);
s32            http_redir_cache_init(void);
s32            http_redir_cache_term(void);
HttpRedirEntry *http_redir_cache_lookup_req(HttpRequest *req);
void           http_redir_cache_add(const char *src_url, s32 method, const u8 *extra_md5, const char *dst_url);
HttpRedirEntry *http_redir_cache_find(const char *url, s32 method, const u8 *extra_md5);
void           http_redir_entry_release_locked(HttpRedirEntry *entry);
void           http_redir_entry_invalidate_locked(HttpRedirEntry *entry);
void           http_redir_entry_release(HttpRedirEntry *entry);
void           http_cache_mem_entry_release(HttpCacheEntry *entry);
void           http_redir_entry_mark_deleted(HttpRedirEntry *entry);
void           http_cache_mem_entry_mark_deleted(HttpCacheEntry *entry);
HttpRedirEntry *http_redir_cache_find_unlocked(const char *url, s32 method, const u8 *extra_md5);
s32            http_is_inited(void);
s32            http_recv_chunked(HttpRequest *req, u8 *dst, u32 len, s32 flags);
s32            http_recv_raw(HttpRequest *req, void *dst, u32 len, s32 flags);
s32            http_read_chunk_size(HttpRequest *req, u32 *size_out, s32 flags);
s32            http_transport_recv(HttpConnection *conn, void *buf, u32 len);
s32            http_resolve_connection_host(HttpConnection *conn);

/* http_io.c */
s32            http_ensure_connected(HttpRequest *req);
s32            http_is_tcp_established(HttpConnection *conn);
s32            http_skip_chunk_trailers(HttpRequest *req, s32 flags);
s32            http_refresh_apctl_proxy(HttpTemplate *cfg);
s32            http_peek_raw(HttpRequest *req, void *dst, u32 len, s32 flags);
s32            http_transport_send(HttpConnection *conn, const void *buf, u32 len, s32 flags);
s32            http_buffered_recv(HttpRequest *req, u8 *dst, u32 len, s32 flags, s32 peek);
s32            http_apply_socket_options(s32 sock_fd, const HttpTemplate *cfg);
s32            http_connect_with_timeout(s32 sock_fd, const HttpSockAddrIn *addr, u32 timeout_usec,
                                         s32 *errno_out, s32 *psp_err_out);
s32            http_create_request_with_url_internal(s32 conn_id, s32 method, const char *url, u64 content_len);
s32            http_delete_request_internal(s32 req_id);
s32            http_lock_request(s32 req_id, HttpRequest **out_req);
s32            http_register_request(HttpRequest *req);
char          *http_build_request_url(HttpRequest *req);
void           http_request_list_init(void);
s32            http_create_request_internal(s32 conn_id, s32 method, const char *path, u64 content_len);
s32            http_create_request_core(s32 conn_id, s32 method, const char *path,
                                        const char *query, const char *fragment, u64 content_len,
                                        const char *user, const char *pass, const char *host_override);
s32            http_create_connection_internal(s32 tmpl_id, const char *host, const char *scheme,
                                               const char *user, const char *pass, u16 port,
                                               s32 keep_alive, s32 redir_mode, HttpRequest *owner_req);
s32            http_create_connection_with_url_internal(s32 tmpl_id, const char *url, s32 keep_alive,
                                                        s32 redir_mode, HttpRequest *owner_req);
s32            http_create_connection_core(s32 tmpl_id, const char *host, const char *scheme,
                                           const char *user, const char *pass, u16 port,
                                           s32 keep_alive, s32 redir_mode, HttpRequest *owner_req);
s32            http_delete_connection_internal(s32 conn_id);
s32            http_register_connection(HttpConnection *conn);
s32            http_lock_connection(s32 conn_id, HttpConnection **out_conn);
void           http_close_connection(HttpConnection *conn);
void           http_connection_list_init(void);
s32            http_find_redir_connection(HttpRequest *req, const char *host, s32 scheme, u16 port);
s32            http_create_template_internal(const char *user_agent, u32 http_ver, s32 auto_proxy);
s32            http_clone_template_internal(s32 tmpl_id);
s32            http_delete_template_internal(s32 tmpl_id);
void           http_template_free_fields(HttpTemplate *cfg);
s32            http_lock_template(s32 tmpl_id, HttpTemplate **out_tmpl);
s32            http_register_template(HttpTemplate *tmpl);
s32            http_template_copy(const HttpTemplate *src, HttpTemplate *dst);
void           http_template_list_init(void);
void           http_strip_proxy_host(char *host_buf);
s32            http_skip_crlf_line(HttpRequest *req, s32 flags);
s32            http_parse_scheme(const char *scheme);
s32            http_drain_response_body(HttpRequest *req);
s32            http_recv_response_header(HttpRequest *req);
u64            http_parse_u64(const char *str);
void           http_format_u64(char *dst, u64 val);
void           http_copy_ssl_errors(HttpConnection *conn, HttpRequest *req);
s32            http_handle_type(s32 id);
s32            http_storage_read_tlv_hdr(s32 id, u8 *tag_out, u16 *len_out);
s32            http_storage_write_tlv_hdr(s32 id, u8 tag, u16 len);
void           http_cache_entry_addref(HttpCacheEntry *entry);
HttpCacheEntry *http_cache_lookup_req(HttpRequest *req);
HttpCacheEntry *http_cache_lookup(s32 mode, const char *url, s32 method, const u8 *extra_md5);
HttpCacheEntry *http_cache_begin_write(HttpRequest *req);
void           http_cache_commit_write(HttpRequest *req);
void           http_cache_abort_write(HttpCacheEntry *entry);
u32            http_cache_read_bytes(void *dst, void **src_ptr, u32 offset, u32 len);
u32            http_cache_write_bytes(void **dst_ptr, u32 offset, const void *src, u32 len);
void           http_cache_data_free(HttpCacheData *data);
void           http_cache_mem_account_free(const HttpCacheData *data);
HttpCacheEntry *http_cache_mem_alloc_entry(HttpRequest *req);
HttpCacheBlock *http_cache_pool_alloc_block(void);
void           http_cache_pool_free_chain(HttpCacheBlock *head);
void           http_cache_pool_free_data(HttpCacheData *data);
void           http_cache_pool_entry_release(HttpCacheEntry *entry);
void           http_cache_pool_entry_mark_deleted(HttpCacheEntry *entry);
HttpCacheEntry *http_cache_pool_alloc_entry(HttpRequest *req);
void           http_cache_entry_release_locked(HttpCacheEntry *entry);
void           http_cache_entry_invalidate_locked(HttpCacheEntry *entry);
s32            http_cache_mem_is_inited(void);
u32            http_cache_mem_get_max_size(void);
s32            http_cache_pool_is_inited(void);
u32            http_cache_pool_get_max_size(void);
void           http_cache_hash_insert(HttpCacheEntry **buckets, HttpCacheEntry *entry);
void           http_cache_hash_remove(HttpCacheEntry **buckets, HttpCacheEntry *entry);
HttpCacheEntry *http_cache_hash_find(HttpCacheEntry **buckets, const char *url, s32 method, const u8 *extra_md5);
s32            http_cache_mem_reserve(u32 needed);
s32            http_load_cached_response_header(HttpRequest *req);
s32            http_attach_request_cookies(HttpRequest *req);
s32            http_attach_basic_auth_header(HttpRequest *req, const char *user, const char *pass, s32 is_proxy);
s32            http_attach_digest_auth_header(HttpRequest *req, const char *user, const char *pass,
                                              const void *entity, u32 entity_len,
                                              const HttpDigestParams *params, s32 is_proxy);
s32            http_header_node_alloc(HttpHeaderNode **out_node, const char *name, const char *value);
void           http_header_list_free(HttpHeaderNode *list);
u32            http_serialize_request_headers(HttpRequest *req, char *dst, s32 dry_run);
s32            http_header_list_clone(HttpHeaderNode **dst_head, const HttpHeaderNode *src);
s32            http_parse_response_fields(const char *hdr, u32 hdr_len, HttpResponseInfo *info);
s32            http_recv_and_parse_headers(HttpRequest *req, HttpConnection *conn);
s32            http_process_response(HttpRequest *req);
const char    *http_header_list_find(const HttpHeaderNode *list, const char *name);
s32            http_format_host_header(char **out_str, s32 scheme, const char *host, u16 port);
s32            http_build_request_header_buf(char **out_buf, HttpRequest *req);
s32            http_copy_all_response_headers(HttpRequest *req);
s32            http_intr_suspend(void);
void           http_intr_resume(s32 intr);
s32            http_md5_digest(const void *data, u32 len, u8 *out);
s32            http_md5_init(void *ctx);
s32            http_md5_update(void *ctx, const void *data, u32 len);
s32            http_md5_result(void *ctx, u8 *out);
s32            http_inet_pton(s32 af, const char *src, void *dst);
s32            http_rtc_get_tick(u64 *tick);
s32            http_rtc_add_seconds(u64 *dst, const u64 *src, s64 secs);
s32            http_rtc_parse_datetime(u64 *tick, const char *str);
s32            http_rtc_format_rfc2822(char *dst, const u64 *tick, s32 tz_min);
s32            http_storage_reset_file(s32 id);
s32            http_storage_open_body(s32 id, s32 flags, s32 mode);
s32            http_storage_verify(s32 id, u32 *size_out);
s32            http_storage_commit_checksum(s32 id, u32 body_size);
s32            http_storage_lock(void);
void           http_storage_unlock(void);
s32            http_storage_close(s32 id);
s32            http_storage_read_exact(s32 id, void *buf, s32 len);
s32            http_storage_write_exact(s32 id, const void *buf, s32 len);
s32            http_storage_seek(s32 id, s32 pad, s32 lo, s32 hi, s32 whence);

/* Exported API declarations */
s32 sceHttpInit(u32 pool_size);
s32 sceHttpEnd(void);
s32 module_reboot_before(void);
s32 sceHttpSetMallocFunction(void *(*alloc_fn)(u32), void (*free_fn)(void *), void *(*realloc_fn)(void *, u32));
s32 sceHttpCreateTemplate(const char *user_agent, s32 http_ver, s32 auto_proxy);
s32 sceHttpDeleteTemplate(s32 tmpl_id);
s32 sceHttp_4E4A284A(s32 tmpl_id);
s32 sceHttpCreateConnection(s32 tmpl_id, const char *host, const char *scheme, u16 port, s32 keep_alive);
s32 sceHttpCreateConnectionWithURL(s32 tmpl_id, const char *url, s32 keep_alive);
s32 sceHttpDeleteConnection(s32 conn_id);
s32 sceHttpCreateRequest(s32 conn_id, s32 method, const char *path, u64 content_len);
s32 sceHttpCreateRequestWithURL(s32 conn_id, s32 method, const char *url, u64 content_len);
s32 sceHttpDeleteRequest(s32 req_id);
s32 sceHttpSendRequest(s32 req_id, const void *data, u32 size);
s32 sceHttpSendRequestInCacheFirstMode(s32 req_id, const void *data, u32 size);
s32 sceHttpAbortRequest(s32 req_id);
s32 sceHttpReadData(s32 req_id, void *data, u32 size);
s32 sceHttpIsRequestInCache(s32 req_id);
s32 sceHttp_A909F2AE(void);
s32 sceHttpGetContentLength(s32 req_id, u64 *len_out);
s32 sceHttpGetStatusCode(s32 req_id, s32 *code_out);
s32 sceHttpGetNetworkErrno(s32 req_id, s32 *errno_out);
s32 sceHttpGetNetworkPspError(s32 req_id, s32 *err_out);
s32 sceHttpGetAllHeader(s32 req_id, char **hdr_out, u32 *len_out);
s32 sceHttpSetResolveRetry(s32 id, s32 retry);
s32 sceHttpSetResolveTimeOut(s32 id, u32 usec);
s32 sceHttpSetConnectTimeOut(s32 id, u32 usec);
s32 sceHttpSetSendTimeOut(s32 id, u32 usec);
s32 sceHttpSetRecvTimeOut(s32 id, u32 usec);
s32 sceHttp_CC920C12(s32 id);
s32 sceHttp_D29163DA(s32 id);
s32 sceHttpSetResHeaderMaxSize(s32 id, u32 size);
s32 sceHttpSetRecvBlockSize(s32 id, u32 size);
s32 sceHttpSetCacheContentLengthMaxSize(s32 id, u32 size);
s32 sceHttpEnableKeepAlive(s32 id);
s32 sceHttpDisableKeepAlive(s32 id);
s32 sceHttpEnableCookie(s32 id);
s32 sceHttpDisableCookie(s32 id);
s32 sceHttpEnableAuth(s32 id);
s32 sceHttpDisableAuth(s32 id);
s32 sceHttp_CDB0DC58(s32 id);
s32 sceHttp_951D310E(s32 id);
s32 sceHttp_D80BE761(s32 id);
s32 sceHttp_C0E69162(s32 id);
s32 sceHttpEnableRedirect(s32 id);
s32 sceHttpDisableRedirect(s32 id);
s32 sceHttpChangeHttpVersion(s32 id, s32 version);
s32 sceHttpEnableCache(s32 id);
s32 sceHttp_8046E250(s32 id);
s32 sceHttpDisableCache(s32 id);
s32 sceHttpSetAuthInfoCB(s32 id, void *cb);
s32 sceHttpSetAuthInfoCallback(s32 id, void *cb, void *arg);
s32 sceHttpSetRedirectCallback(s32 id, void *cb, void *arg);
s32 sceHttpSetCookieSendCallback(s32 id, void *cb, void *arg);
s32 sceHttpSetCookieRecvCallback(s32 id, void *cb, void *arg);
s32 sceHttpSetProxy(s32 id, s32 use_proxy, s32 proxy_mode, const char *server, u16 port);
s32 sceHttpGetProxy(s32 id, s32 *use_proxy, s32 *proxy_mode, char *server, u32 server_len, u16 *port);
s32 sceHttpAddExtraHeader(s32 id, const char *name, const char *value, s32 mode);
s32 sceHttpDeleteHeader(s32 id, const char *name);
s32 sceHttpFlushAuthList(void);
s32 sceHttp_77EE5319(void);
s32 sceHttp_DD6E7857(void);
s32 sceHttpSaveSystemCookie(void);
s32 sceHttpLoadSystemCookie(void);
s32 sceHttpGetCookie(const char *url, u8 *cookie_buf, u32 *len_out, u32 buf_size, s32 is_secure);
s32 sceHttpAddCookie(const char *url, const char *header_val, u32 header_len);
s32 sceHttp_457D221D(void);
s32 sceHttpInitCache(u32 max_size);
s32 sceHttpFlushCache(void);
s32 sceHttpEndCache(void);
s32 sceHttp_739C2D79(void *pool_mem, u32 pool_size);
s32 sceHttp_B0257723(void);
s32 sceHttp_A461A167(void);
s32 sceHttpsInit(s32 ca_count, HttpCertSlot **ca_list, HttpCertSlot *client_cert, HttpCertSlot *client_key);
s32 sceHttp_68AB0F86(s32 param_a, s32 param_b, s32 param_c);
s32 sceHttpsLoadDefaultCert(u32 group, u32 bit);
s32 sceHttp_87F1E666(s32 id, u32 flags);
s32 sceHttp_3C478044(s32 id, u32 flags);
s32 sceHttpsGetSslError(s32 req_id, s32 *ssl_err_out, u32 *cert_flags_out);
s32 sceHttpsSetSslCallback(s32 id, void *cb, void *arg);
s32 sceHttpsGetCaList(void ***list_out, s32 *count_out);
s32 sceHttpsEnd(void);
s32 sceHttpsEnableOption(u32 flags);
s32 sceHttpsDisableOption(u32 flags);

#endif /* LIBHTTP_INTERNAL_H */
