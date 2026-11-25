#ifndef PSPNET_UPNP_INT_H
#define PSPNET_UPNP_INT_H

/*
 * PSP 6.61 pspnet_upnp.prx (sceNetUpnp_Library)
 * Reimplementation of kd/pspnet_upnp.prx (UPnP IGD + STUN client)
 */

#include <common/types.h>

static inline __attribute__((always_inline)) u16 bswap16(u16 v)
{
	u16 res;
	__asm__("wsbh %0, %1" : "=r"(res) : "r"(v));
	return res;
}

static inline __attribute__((always_inline)) u32 bswap32(u32 v)
{
	u32 res;
	__asm__("wsbw %0, %1" : "=r"(res) : "r"(v));
	return res;
}

static inline __attribute__((always_inline)) u32 rotr32(u32 v, u32 n)
{
	return (v >> n) | (v << (32 - n));
}

/* Imports: Kernel_Library */
extern void *sceKernelMemset(void *dst, int c, u32 n);
extern s32 _sceKernelCheckThreadStack(void);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 n);
extern s32 sceKernelGetThreadId(void);

/* Imports: ThreadManForUser */
extern s32 sceKernelSetEventFlag(s32 evid, u32 bits);
extern s32 sceKernelSendMbx(s32 mbxid, void *msg);
extern s32 sceKernelDeleteEventFlag(s32 evid);
extern s32 sceKernelStartThread(s32 thid, u32 arglen, void *argp);
extern s32 sceKernelDeleteMutex(s32 mid);
extern s32 sceKernelWaitThreadEnd(s32 thid, u32 *timeout);
extern s32 sceKernelPollEventFlag(s32 evid, u32 bits, u32 wait, u32 *outBits);
extern u32 sceKernelGetSystemTimeLow(void);
extern s32 sceKernelWaitEventFlag(s32 evid, u32 bits, u32 wait, u32 *outBits, u32 *timeout);
extern s32 sceKernelCreateThread(const char *name, void *entry, s32 prio, s32 stackSize, u32 attr, void *opt);
extern s32 sceKernelCreateEventFlag(const char *name, u32 attr, u32 bits, void *opt);
extern s32 sceKernelUnlockMutex(s32 mid, s32 count);
extern s32 sceKernelClearEventFlag(s32 evid, u32 bits);
extern s32 sceKernelCreateMbx(const char *name, u32 attr, void *opt);
extern u64 sceKernelGetSystemTimeWide(void);
extern s32 sceKernelDeleteMbx(s32 mbxid);
extern s32 sceKernelReceiveMbx(s32 mbxid, void **msg, u32 *timeout);
extern s32 sceKernelDeleteThread(s32 thid);
extern s32 sceKernelExitThread(s32 status);
extern s32 sceKernelLockMutex(s32 mid, s32 count, u32 *timeout);
extern s32 sceKernelCreateMutex(const char *name, u32 attr, s32 initCount, void *opt);

/* Imports: sceNet / sceNetApctl / sceNetIfhandle_lib */
extern s32 sceNetFreeThreadinfo(s32 thid);
extern s32 sceNetApctlGetInfo(s32 code, void *info);
extern s32 sceNetApctlDelHandler(s32 id);
extern s32 sceNetApctlGetState(s32 *state);
extern s32 sceNetApctlAddHandler(void *handler, void *arg);
extern void *sceNetMallocInternal(u32 size);
extern void sceNetFreeInternal(void *ptr);

/* Imports: sceNetInet */
extern s32 sceNetInetSendto(s32 s, const void *buf, u32 len, s32 flags, const void *to, s32 tolen);
extern s32 sceNetInetGetsockname(s32 s, void *name, s32 *namelen);
extern s32 sceNetInetBind(s32 s, const void *addr, s32 addrlen);
extern s32 sceNetInetSetsockopt(s32 s, s32 level, s32 optname, const void *optval, s32 optlen);
extern s32 sceNetInetConnect(s32 s, const void *name, s32 namelen);
extern s32 sceNetInetGetsockopt(s32 s, s32 level, s32 optname, void *optval, s32 *optlen);
extern s32 sceNetInetSelect(s32 nfds, void *readfds, void *writefds, void *exceptfds, void *timeout);
extern s32 sceNetInetSend(s32 s, const void *buf, u32 len, s32 flags);
extern s32 sceNetInetSocketAbort(s32 s);
extern s32 sceNetInetSocket(s32 domain, s32 type, s32 protocol);
extern s32 sceNetInetGetPspError(void);
extern s32 sceNetInetClose(s32 s);
extern u32 sceNetInetInetAddr(const char *cp);
extern s32 sceNetInetRecvfrom(s32 s, void *buf, u32 len, s32 flags, void *from, s32 *fromlen);
extern s32 sceNetInetRecv(s32 s, void *buf, u32 len, s32 flags);
extern const char *sceNetInetInetNtop(s32 af, const void *src, char *dst, u32 size);
extern s32 sceNetInetInetPton(s32 af, const char *src, void *dst);

/* Imports: sceNetResolver */
extern s32 sceNetResolverStartNtoA(s32 rid, const char *hostname, void *addr, u32 timeout, s32 retry);
extern s32 sceNetResolverCreate(s32 *rid, void *buf, u32 buflen);
extern s32 sceNetResolverStop(s32 rid);
extern s32 sceNetResolverDelete(s32 rid);

/* Imports: sceNet_lib */
extern u32 sceNetRand(void);
extern s32 sceNetStrtoul(const char *s, char **endptr, s32 base);
extern void *sceNetMemmove(void *dst, const void *src, u32 n);
extern char *sceNetStrcpy(char *dst, const char *src);
extern s32 sceNetStrncmp(const char *s1, const char *s2, u32 n);
extern s32 sceNetStrcasecmp(const char *s1, const char *s2);
extern s32 sceNetStrcmp(const char *s1, const char *s2);
extern char *sceNetStrncpy(char *dst, const char *src, u32 n);
extern char *sceNetStrchr(const char *s, s32 c);
extern s32 sceNetSprintf(char *dst, const char *fmt, ...);
extern u32 sceNetStrlen(const char *s);
extern s32 sceNetMemcmp(const void *a, const void *b, u32 n);

/* Imports: sceReg */
extern s32 sceRegCloseCategory(u32 hd);
extern s32 sceRegOpenCategory(u32 h, const char *name, s32 mode, u32 *hd);
extern s32 sceRegGetKeyValue(u32 hd, u32 hk, void *buf, u32 size);
extern s32 sceRegOpenRegistry(void *reg, s32 mode, u32 *h);
extern s32 sceRegGetKeyInfo(u32 hd, const char *name, u32 *hk, u32 *type, u32 *size);
extern s32 sceRegCloseRegistry(u32 h);

/* Data structures */
typedef struct SockAddrIn {
	u8 sin_len;
	u8 sin_family;
	u16 sin_port;
	u32 sin_addr;
	u8 sin_zero[8];
} SockAddrIn;

typedef struct TimeVal {
	s32 tv_sec;
	s32 tv_usec;
} TimeVal;

typedef struct FdSet {
	u32 bits[8];
} FdSet;

typedef struct XmlTagEntry {
	const char *name;
	u32 has_text;
} XmlTagEntry;

typedef struct XmlSchema {
	const XmlTagEntry *const *levels;
	u32 num_levels;
} XmlSchema;

typedef struct XmlParser XmlParser;
typedef s32 (*XmlCallback)(XmlParser *p, s32 event, s32 depth, s32 idx, const char *val1, const char *val2, void *arg);

struct XmlParser {
	const XmlSchema *schema;  /* 0x000 */
	char tag_name[0xff];      /* 0x004 */
	char text_val[0xff];      /* 0x103 */
	char ns_prefix[0x40];     /* 0x202 */
	char entity_buf[6];          /* 0x242 */
	u32 entity_len;              /* 0x248 */
	u32 entity_state;              /* 0x24c */
	s32 depth;                /* 0x250 */
	s32 match_idx[10];        /* 0x254 */
	s32 state;                /* 0x27c */
	u32 attr_flags;           /* 0x280 */
	u32 buf_pos;              /* 0x284 */
	s32 is_self_closing;      /* 0x288 */
	XmlCallback cb;           /* 0x28c */
	void *user_arg;           /* 0x290 */
}; /* 0x294 */

typedef struct Sha1Ctx {
	u32 state[5];             /* 0x00 */
	u16 buf_len;              /* 0x14 */
	u16 finalized;            /* 0x16 */
	u64 total_bytes;          /* 0x18 */
	u8 buf[0x40];             /* 0x20 */
} Sha1Ctx; /* 0x60 */

typedef void (*UpnpHandlerCb)(s32 prev_state, s32 state, s32 event, s32 err, void *arg);

typedef struct UpnpHandler {
	s32 id;                   /* 0x00 */
	UpnpHandlerCb cb;         /* 0x04 */
	void *arg;                /* 0x08 */
} UpnpHandler; /* 0x0c */

typedef struct UpnpNatInfoEx {
	s32 upnp_status;          /* 0x00 */
	s32 stun_status;          /* 0x04 */
	s32 nat_type;             /* 0x08 */
	u32 stun_result[4];       /* 0x0c..0x1b */
	s32 stun_ext_ok;          /* 0x1c */
	u16 ext_port;             /* 0x20 */
	u8 stun_mapped_ip[4];     /* 0x22 */
	u8 upnp_ext_ip[4];        /* 0x26 */
	u8 reserved[2];             /* 0x2a */
	s32 ip_match;             /* 0x2c */
} UpnpNatInfoEx; /* 0x30 */

typedef struct UpnpNatInfo {
	s32 upnp_status;          /* 0x00 */
	s32 stun_status;          /* 0x04 */
	s32 nat_type;             /* 0x08 */
	u8 stun_mapped_ip[4];     /* 0x0c */
} UpnpNatInfo; /* 0x10 */

typedef struct UpnpCtx {
	char stun_host[0x24];     /* 0x0000 */
	s32 stack_size;           /* 0x0024 */
	s32 thread_prio;          /* 0x0028 */
	s32 apctl_handler_id;     /* 0x002c */
	s32 sock_fd;              /* 0x0030 */
	s32 state;                /* 0x0034 */
	s32 event_code;           /* 0x0038 */
	s32 last_error;           /* 0x003c */
	s32 thread_id;            /* 0x0040 */
	s32 evf_id;               /* 0x0044 */
	s32 mutex_id;             /* 0x0048 */
	u32 flags;                /* 0x004c */
	s32 use_upnp;             /* 0x0050 */
	u32 stun_result[4];       /* 0x0054 */
	s32 stun_cb_status;       /* 0x0064 */
	s32 stun_ext_ok;          /* 0x0068 */
	u16 ext_port;             /* 0x006c */
	u16 port2;                /* 0x006e */
	u16 port3;                /* 0x0070 */
	u16 reserved0;               /* 0x0072 */
	u32 reserved1[2];            /* 0x0074 */
	u32 stun_mapped_ip;       /* 0x007c */
	char http_buf[0x400];     /* 0x0080 */
	char soap_buf[0x400];     /* 0x0480 */
	char desc_path[0x80];     /* 0x0880 */
	u32 local_ip;             /* 0x0900 */
	u32 gw_ip;                /* 0x0904 */
	u16 gw_port;              /* 0x0908 */
	u16 reserved2;              /* 0x090a */
	char url_base[0x400];     /* 0x090c */
	s32 active_svc_idx;       /* 0x0d0c */
	u32 found_svc_mask;       /* 0x0d10 */
	char ctrl_url[2][0x400];  /* 0x0d14 */
	char ext_ip_str[0x10];    /* 0x1514 */
	u32 xml_parse_flags;      /* 0x1524 */
} UpnpCtx; /* 0x1528 */

typedef struct StunMsg {
	void *next;               /* 0x00 */
	u32 prio;                 /* 0x04 */
	s32 cmd;                  /* 0x08 */
	void *user_arg;           /* 0x0c */
	s32 sock_fd;              /* 0x10 */
	u16 bind_port;            /* 0x14 */
	u16 reserved;               /* 0x16 */
} StunMsg; /* 0x18 */

typedef struct StunConfig {
	s32 use_xor;              /* 0x00 */
	u32 username;             /* 0x04 */
	u8 key[0x14];             /* 0x08 */
	s32 has_ext_data;         /* 0x1c */
	void *ext_data;           /* 0x20 */
	u32 ext_data_len;         /* 0x24 */
} StunConfig; /* 0x28 */

typedef void (*StunCallback)(s32 id, void *arg, s32 event, void *data);

typedef struct StunCtx {
	s32 id;                   /* 0x00 */
	u32 flags;                /* 0x04 */
	s32 thread_id;            /* 0x08 */
	s32 mbx_id;               /* 0x0c */
	StunMsg stop_msg;         /* 0x10 */
	s32 resolver_id;          /* 0x28 */
	s32 sock_fd;              /* 0x2c */
	char host[0x40];          /* 0x30 */
	u16 port;                 /* 0x70 */
	u16 reserved0;               /* 0x72 */
	StunConfig cfg;           /* 0x74 */
	StunCallback cb;          /* 0x9c */
	void *pkt_buf;            /* 0xa0 */
	void *ext_copy;           /* 0xa4 */
	u32 pkt_buf_size;         /* 0xa8 */
} StunCtx; /* 0xac */

typedef struct StunReqOpts {
	s32 has_resp_addr;        /* 0x00 */
	u16 reserved;               /* 0x04 */
	u16 resp_port;            /* 0x06 */
	u32 resp_addr;            /* 0x08 */
	u32 change_req;           /* 0x0c */
	s32 use_xor;              /* 0x10 */
	void *username;           /* 0x14 */
	u32 username_len;         /* 0x18 */
	void *hmac_key;           /* 0x1c */
	u32 hmac_key_len;         /* 0x20 */
	s32 has_ext;              /* 0x24 */
	void *ext_data;           /* 0x28 */
	u32 ext_data_len;         /* 0x2c */
} StunReqOpts; /* 0x30 */

typedef struct StunAddrAttr {
	u8 unused;
	u8 family;
	u16 port;
	u32 addr;
} StunAddrAttr; /* 0x08 */

typedef struct StunRespData {
	StunAddrAttr mapped;      /* 0x00 */
	StunAddrAttr source;      /* 0x08 */
	StunAddrAttr changed;     /* 0x10 */
	u32 reserved[2];            /* 0x18 */
	void *hmac_key;           /* 0x20 */
	u32 hmac_key_len;         /* 0x24 */
	s32 check_ext;            /* 0x28 */
	void *ext_data;           /* 0x2c */
	u32 ext_data_len;         /* 0x30 */
} StunRespData; /* 0x34 */

typedef struct StunHdr {
	u16 type;
	u16 length;
	u8 tid[16];
} StunHdr; /* 0x14 */

typedef struct StunAttrHdr {
	u16 type;
	u16 length;
} StunAttrHdr; /* 0x04 */

typedef s32 (*UpnpStateFn)(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);

/* Cross-module internal declarations */
void hmacSha1Compute(const u8 *data, s32 data_len, const u8 *key, s32 key_len, u8 *out_digest);
s32 xmlParserInit(XmlParser *p, const XmlSchema *schema, XmlCallback cb, void *userdata);
s32 xmlParserFeed(XmlParser *p, const char *data, s32 len);
s32 stunInitGlobal(void);
s32 stunTermGlobal(void);

#endif /* PSPNET_UPNP_INT_H */
