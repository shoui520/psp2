/*
 * PSP 6.61 pspnet_upnp.prx (sceNetUpnp_Library)
 * Reimplementation of kd/pspnet_upnp.prx (UPnP IGD + STUN client)
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char s8;
typedef signed short s16;
typedef int s32;
typedef unsigned long long u64;
typedef long long s64;

#ifndef NULL
#define NULL ((void *)0)
#endif

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

/* Globals (.bss) */
static s32 g_upnp_initialized;
static s32 g_stun_initialized;
static s32 g_xml_found_service;
static s32 g_xml_service_depth;
static s32 g_xml_service_idx;
static s32 g_xml_service_type;
static UpnpCtx g_upnp;
static UpnpHandler g_upnp_handlers[3];
static s32 g_apctl_connected;
static StunCtx g_stun_ctx[4];

/* Read-only tables */
static const u32 g_sha1_iv[5] = {
	0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0
};

static const char *const g_proto_names[2] = { "UDP", "TCP" };
static const char *const g_soap_actions[4] = {
	"AddPortMapping",
	"DeletePortMapping",
	"GetExternalIPAddress",
	"GetStatusInfo"
};
static const char *const g_service_types[2] = {
	"urn:schemas-upnp-org:service:WANIPConnection:1",
	"urn:schemas-upnp-org:service:WANPPPConnection:1"
};

static const XmlTagEntry g_desc_l0[] = { { "root", 1 }, { "", 1 } };
static const XmlTagEntry g_desc_l1[] = { { "URLBase", 1 }, { "device", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_l2[] = { { "serviceList", 0 }, { "deviceList", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_l3[] = { { "service", 0 }, { "device", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_l4[] = { { "serviceList", 0 }, { "deviceList", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_l5[] = { { "service", 0 }, { "device", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_l6[] = { { "serviceType", 1 }, { "controlURL", 1 }, { "serviceList", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_l7[] = { { "service", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_l8[] = { { "serviceType", 1 }, { "controlURL", 1 }, { "", 1 } };

static const XmlTagEntry *const g_desc_levels[9] = {
	g_desc_l0, g_desc_l1, g_desc_l2, g_desc_l3, g_desc_l4,
	g_desc_l5, g_desc_l6, g_desc_l7, g_desc_l8
};
static const XmlSchema g_desc_schema = { g_desc_levels, 9 };

static const XmlTagEntry g_extip_l0[] = { { "NewExternalIPAddress", 1 }, { "", 1 } };
static const XmlTagEntry *const g_extip_levels[1] = { g_extip_l0 };
static const XmlSchema g_extip_schema = { g_extip_levels, 1 };

static const XmlTagEntry g_soap_l0[] = { { "Envelope", 0 }, { "", 1 } };
static const XmlTagEntry g_soap_l1[] = { { "Body", 0 }, { "", 1 } };
static const XmlTagEntry g_soap_l2[] = { { "AddPortMappingResponse", 0 }, { "Fault", 0 }, { "", 1 } };
static const XmlTagEntry g_soap_l3[] = { { "detail", 0 }, { "", 1 } };
static const XmlTagEntry g_soap_l4[] = { { "UPnPError", 0 }, { "", 1 } };
static const XmlTagEntry g_soap_l5[] = { { "errorCode", 1 }, { "", 1 } };

static const XmlTagEntry *const g_soap_levels[6] = {
	g_soap_l0, g_soap_l1, g_soap_l2, g_soap_l3, g_soap_l4, g_soap_l5
};
static const XmlSchema g_soap_schema = { g_soap_levels, 6 };

static const u8 g_stun_hmac_key[20] = {
	0xa0, 0x07, 0x0e, 0x61, 0xad, 0x3f, 0xca, 0x50,
	0x6d, 0xb5, 0xad, 0x3d, 0x57, 0x13, 0x32, 0xce,
	0xa7, 0x32, 0x0d, 0x9e
};

/* Forward declarations */
static void sha1Compress(u32 *w, u32 *state);
static s32 soapWriteEnvelopeHeader(char *buf, s32 bufsize, s32 action, s32 svc_type);
static s32 soapWriteEnvelopeFooter(char *buf, s32 bufsize, s32 action);
static s32 upnpResolveControlUrl(const char *url, u32 *gw_ip, u16 *gw_port, const char *base_path, char *tmp_buf, u32 tmp_size, char *out_path, u32 out_size);
static s32 httpBuildGetRequest(char *buf, s32 bufsize, const SockAddrIn *addr, const char *path);
static s32 httpRecvResponse(s32 sock, char *buf, u32 bufsize, XmlParser *parser);
static s32 httpBuildSoapPostHeader(char *buf, s32 bufsize, const SockAddrIn *addr, const char *path, s32 action, s32 svc_type, s32 content_len);
static s32 httpConnectTimeout(s32 sock, const SockAddrIn *addr, u32 timeout_us);
static s32 httpRecvBodyToXml(s32 sock, char *buf, u32 initial_len, u32 content_len, u32 buf_cap, XmlParser *parser);
static s32 upnpInitInternal(s32 stack_size, s32 prio);
static s32 upnpAddHandlerInternal(UpnpHandlerCb cb, void *arg, s32 *out_id);
static s32 upnpGetNatInfoExInternal(UpnpNatInfoEx *info);
static s32 upnpMapErrorCode(s32 *out_err, s32 upnp_code);
static s32 upnpNotifyHandlers(UpnpCtx *ctx, s32 prev_state);
static void upnpTermInternal(void);
static s32 upnpDelHandlerInternal(s32 id);
static s32 upnpStartInternal(s32 ext_flag);
static s32 upnpStopInternal(void);
static s32 upnpAbortInternal(void);
static s32 upnpGetStateInternal(s32 *out_state);
static s32 upnpGetNatInfoInternal(UpnpNatInfo *info);
static s32 upnpGetLastErrorInternal(s32 *out_err);
static s32 upnpSetEventBits(u32 bits);
static s32 upnpClearEventBits(u32 bits);
static s32 upnpWaitEventBits(u32 bits, u32 *out_bits);
static s32 upnpPollEventBits(u32 bits, u32 *out_bits);
static s32 upnpReadRegistryString(const char *cat, const char *key, char *out, s32 out_size);
static s32 ssdpParseLocationUrl(char *url, u32 *expected_ip, u16 *out_port, char *out_path, u32 out_path_size);
static s32 ssdpRecvResponse(s32 sock, char *buf, u32 bufsize, u32 *gw_ip, u16 *gw_port, char *desc_path, u32 desc_path_size);
static s32 ssdpFindLocationHeader(char *buf, u32 len);
static s32 ssdpCheckStatusOk(char *line);
static s32 ssdpBuildMSearch(char *buf, s32 bufsize);
static s32 ssdpDiscoverGateway(s32 sock, char *buf, u32 bufsize, u32 *gw_ip, u16 *gw_port, char *desc_path, u32 desc_path_size);
static s32 upnpStateDiscoverIgd(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateAddPortMapping(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateDeletePortMapping(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateRunStunProbe(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpWorkerThread(u32 args, void *argp);
static s32 upnpStateWaitStart(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateIdle(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateWaitStop(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateWaitApctl(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static void upnpApctlHandler(s32 prev_state, s32 new_state, s32 event, s32 err, void *arg);
static s32 upnpExecSoapAction(UpnpCtx *ctx, s32 svc_idx, s32 action, u16 ext_port, s32 proto, u16 int_port, s32 *out_err, u32 timeout_us);
static void upnpStunCallback(s32 stun_id, void *arg, s32 event, void *data);
static s32 xmlParserFeed(XmlParser *p, const char *data, s32 len);
static s32 xmlParserInit(XmlParser *p, const XmlSchema *schema, XmlCallback cb, void *arg);
static s32 xmlParserMatchOpenTag(XmlParser *p, s32 *out_matched);
static s32 xmlParserMatchCloseTag(XmlParser *p, s32 *out_matched);
static void hmacSha1Compute(const u8 *data, s32 data_len, const u8 *key, s32 key_len, u8 *out_digest);
static s32 stunInitGlobal(void);
static s32 stunTermGlobal(void);
s32 sceNetStun_EBF3C24D(s32 stack_size, s32 prio, const char *host, u16 port, const StunConfig *cfg, StunCallback cb, s32 *out_id);
s32 sceNetStun_4403D9E7(s32 id);
s32 sceNetStun_A0CF3A47(s32 id, void *arg);
s32 sceNetStun_FBF60C6B(s32 id, u16 port, void *arg);
s32 sceNetStun_6CA54FC9(s32 id, s32 sock_fd, void *arg);
s32 sceNetStun_40971567(s32 id, void *arg);
static s32 stunCreateInternal(s32 stack_size, s32 prio, const char *host, u16 port, const StunConfig *cfg, StunCallback cb, s32 *out_id);
static s32 stunStopInternal(s32 id, void *arg);
static s32 stunInitLib(void);
static s32 stunTermLib(void);
static s32 stunDeleteInternal(s32 id);
static s32 stunStartNatCheckInternal(s32 id, void *arg);
static s32 stunStartPortCheckInternal(s32 id, u16 port, void *arg);
static s32 stunStartSockCheckInternal(s32 id, s32 sock_fd, void *arg);
static s32 stunWorkerThread(u32 args, void *argp);
static void stunDestroyContext(s32 idx);
static void stunExecPortCheck(StunCtx *ctx, u16 port, s32 sock_fd, void *arg);
static void stunExecNatCheck(StunCtx *ctx, void *arg);
static void stunExecExtNatCheck(StunCtx *ctx, void *arg);
static s32 stunResolveServer(StunCtx *ctx, const char *host, SockAddrIn *out_addr);
static s32 stunSendRecvRequest(s32 send_fd, s32 recv_fd, const SockAddrIn *dst_addr, const SockAddrIn *expect_addr, const StunReqOpts *req, StunRespData *resp, u8 *pkt_buf, u32 pkt_buf_size);
static s32 stunTestChangedAddrReachable(s32 sock_fd, const StunAddrAttr *dst, u32 *out_res);
static s32 stunBuildBindingRequest(u8 *buf, u32 bufsize, const u8 *tid, const StunReqOpts *req);
static s32 stunParseBindingResponse(u8 *buf, u32 bufsize, const u8 *tid, StunRespData *resp);
static s32 stunWriteHeader(u8 *buf, u32 total_len, const u8 *tid, u16 msg_type);
static s32 stunVerifyHeader(u8 *buf, u32 bufsize, const u8 *tid, u16 expect_type);
static s32 stunWriteAddrAttr(u8 *buf, u32 bufsize, u16 attr_type, u16 port, u32 addr);
static s32 stunReadAddrAttr(const u8 *buf, u32 bufsize, s32 *out_skip, StunAddrAttr *out_addr);
static s32 stunWriteChangeReqAttr(u8 *buf, u32 bufsize, u32 flags);
static s32 stunWriteBytesAttr(u8 *buf, u32 bufsize, u16 attr_type, const void *data, u32 data_len);
static s32 stunWriteMessageIntegrity(u8 *pkt_start, u32 bufsize, u8 *attr_pos, const u8 *key, u32 key_len);
static s32 stunVerifyMessageIntegrity(u8 *pkt_start, u32 bufsize, u8 *attr_pos, const u8 *key, u32 key_len);
static s32 stunVerifyExtAttr(const u8 *attr_pos, u32 rem, s32 check_ext, const void *ext_data, u32 ext_len);
static s32 sha1Update(Sha1Ctx *ctx, const u8 *data, u32 len);
static s32 sha1Final(Sha1Ctx *ctx, u8 *out_digest);
static s32 sha1Init(Sha1Ctx *ctx);

static const UpnpStateFn g_upnp_state_handlers[8] = {
	upnpStateWaitStart, upnpStateDiscoverIgd, upnpStateAddPortMapping, upnpStateDeletePortMapping,
	upnpStateIdle, upnpStateRunStunProbe, upnpStateWaitStop, upnpStateWaitApctl
};

/* =========================================================================
 * SHA-1 core (0x0000..0x047c)
 * ========================================================================= */

static void sha1TransformBlock(u32 *w, u32 *state)
{
	sha1Compress(w, state);
}

static void sha1Compress(u32 *w, u32 *state)
{
	u32 a = state[0];
	u32 b = state[1];
	u32 c = state[2];
	u32 d = state[3];
	u32 e = state[4];
	u32 i;

	for (i = 0; i < 80; i++) {
		u32 wi;
		u32 idx = i & 15;
		if (i < 16) {
			wi = w[idx];
		} else {
			wi = w[idx] ^ w[(i + 2) & 15] ^ w[(i + 8) & 15] ^ w[(i + 13) & 15];
			wi = rotr32(wi, 31);
			w[idx] = wi;
		}
		u32 f, k;
		if (i < 20) {
			f = (b & c) | ((~b) & d);
			k = 0x5a827999u;
		} else if (i < 40) {
			f = b ^ c ^ d;
			k = 0x6ed9eba1u;
		} else if (i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8f1bbcdcu;
		} else {
			f = b ^ c ^ d;
			k = 0xca62c1d6u;
		}
		u32 temp = rotr32(a, 27) + f + e + k + wi;
		e = d;
		d = c;
		c = rotr32(b, 2);
		b = a;
		a = temp;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
}

static s32 sha1ComputeDigest(const u8 *data, u32 len, u8 *out_digest)
{
	u32 state[5];
	u32 w[16];
	u32 bitlen = len << 3;
	u32 i;

	state[0] = g_sha1_iv[0];
	state[1] = g_sha1_iv[1];
	state[2] = g_sha1_iv[2];
	state[3] = g_sha1_iv[3];
	state[4] = g_sha1_iv[4];

	while (len >= 64) {
		for (i = 0; i < 16; i++) {
			u32 v = ((u32)data[0] << 24) | ((u32)data[1] << 16) |
			        ((u32)data[2] << 8) | (u32)data[3];
			w[i] = v;
			data += 4;
		}
		sha1Compress(w, state);
		len -= 64;
	}

	for (i = 0; i < 16; i++)
		w[i] = 0;

	if (len >= 56) {
		u8 *wb = (u8 *)w;
		for (i = 0; i < len; i++)
			wb[i] = data[i];
		wb[len] = 0x80;
		for (i = 0; i < 16; i++)
			w[i] = bswap32(w[i]);
		sha1Compress(w, state);
		for (i = 0; i < 16; i++)
			w[i] = 0;
		w[15] = bitlen;
		sha1Compress(w, state);
	} else {
		u8 *wb = (u8 *)w;
		for (i = 0; i < len; i++)
			wb[i] = data[i];
		wb[len] = 0x80;
		for (i = 0; i < 14; i++)
			w[i] = bswap32(w[i]);
		w[15] = bitlen;
		sha1Compress(w, state);
	}

	for (i = 0; i < 5; i++) {
		u32 v = state[i];
		out_digest[i * 4 + 0] = (u8)(v >> 24);
		out_digest[i * 4 + 1] = (u8)(v >> 16);
		out_digest[i * 4 + 2] = (u8)(v >> 8);
		out_digest[i * 4 + 3] = (u8)(v);
	}
	return 0;
}

/* =========================================================================
 * sceNetUpnp exports (0x0fa0..0x1394)
 * ========================================================================= */

s32 sceNetUpnp_E24220B5(s32 stack_size, s32 prio)
{
	s32 ret;
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (g_upnp_initialized > 0)
		return (s32)0x80411602;
	ret = upnpInitInternal(stack_size, prio);
	if (ret < 0)
		return ret;
	g_upnp_initialized = 1;
	return ret;
}

s32 sceNetUpnp_540491EF(void)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return 0;
	upnpTermInternal();
	g_upnp_initialized = 0;
	return 0;
}

s32 sceNetUpnp_8513C6D1(UpnpHandlerCb cb, void *arg, s32 *out_id)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	return upnpAddHandlerInternal(cb, arg, out_id);
}

s32 sceNetUpnp_74A51211(s32 id)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	return upnpDelHandlerInternal(id);
}

s32 sceNetUpnp_3432B2E5(void)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpStartInternal(0);
}

s32 sceNetUpnp_FDA78483(void)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpStartInternal(1);
}

s32 sceNetUpnp_3E32ED9E(void)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpStopInternal();
}

s32 sceNetUpnp_C3A35217(s32 *out_state)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpGetStateInternal(out_state);
}

s32 sceNetUpnp_27045362(UpnpNatInfo *info)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpGetNatInfoInternal(info);
}

s32 sceNetUpnp_1038E77A(UpnpNatInfoEx *info)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpGetNatInfoExInternal(info);
}

s32 sceNetUpnp_EAF1C841(s32 *out_err)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpGetLastErrorInternal(out_err);
}

/* =========================================================================
 * SOAP request builders & XML response callbacks (0x1398..0x2c5b)
 * ========================================================================= */

static s32 soapBuildAddPortMappingBody(char *buf, s32 bufsize, const char *client_ip, s32 svc_type, u16 ext_port, s32 proto, u16 int_port)
{
	char line[0x80];
	char *p;
	s32 rem, n;

	n = soapWriteEnvelopeHeader(buf, bufsize, 0, svc_type);
	if (n < 0)
		return n;
	p = buf + n;
	rem = bufsize - n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s</%s>\r\n", "NewRemoteHost", "", "NewRemoteHost");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%d</%s>\r\n", "NewExternalPort", (u32)ext_port, "NewExternalPort");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s</%s>\r\n", "NewProtocol", g_proto_names[proto], "NewProtocol");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%d</%s>\r\n", "NewInternalPort", (u32)int_port, "NewInternalPort");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s</%s>\r\n", "NewInternalClient", client_ip, "NewInternalClient");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%d</%s>\r\n", "NewEnabled", 1, "NewEnabled");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s:%d to %d (%s)</%s>\r\n",
		"NewPortMappingDescription", client_ip, (u32)int_port, (u32)ext_port,
		g_proto_names[proto], "NewPortMappingDescription");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%d</%s>\r\n", "NewLeaseDuration", 0, "NewLeaseDuration");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	n = soapWriteEnvelopeFooter(p, rem, 0);
	if (n < 0)
		return n;
	return (s32)((p + n) - buf);
}

static s32 soapBuildDeletePortMappingBody(char *buf, s32 bufsize, s32 svc_type, u16 ext_port, s32 proto)
{
	char line[0x80];
	char *p;
	s32 rem, n;

	n = soapWriteEnvelopeHeader(buf, bufsize, 1, svc_type);
	if (n < 0)
		return n;
	p = buf + n;
	rem = bufsize - n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s</%s>\r\n", "NewRemoteHost", "", "NewRemoteHost");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%d</%s>\r\n", "NewExternalPort", (u32)ext_port, "NewExternalPort");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s</%s>\r\n", "NewProtocol", g_proto_names[proto], "NewProtocol");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	n = soapWriteEnvelopeFooter(p, rem, 1);
	if (n < 0)
		return n;
	return (s32)((p + n) - buf);
}

static s32 soapBuildActionBody(char *buf, s32 bufsize, s32 action, s32 svc_type)
{
	s32 n = soapWriteEnvelopeHeader(buf, bufsize, action, svc_type);
	if (n < 0)
		return n;
	s32 m = soapWriteEnvelopeFooter(buf + n, bufsize - n, action);
	if (m < 0)
		return m;
	return n + m;
}

static s32 soapWriteEnvelopeHeader(char *buf, s32 bufsize, s32 action, s32 svc_type)
{
	char line[0x80];
	char *p = buf;
	s32 rem = bufsize;
	s32 n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "<s:%s xmlns:s=\"%s\" s:encodingStyle=\"%s\">\r\n",
		"Envelope",
		"http://schemas.xmlsoap.org/soap/envelope/",
		"http://schemas.xmlsoap.org/soap/encoding/");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "   <s:%s>\r\n", "Body");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "      <u:%s xmlns:u=\"%s\">\r\n",
		g_soap_actions[action], g_service_types[svc_type]);
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	return (s32)(p - buf);
}

static s32 soapWriteEnvelopeFooter(char *buf, s32 bufsize, s32 action)
{
	char line[0x40];
	char *p = buf;
	s32 rem = bufsize;
	s32 n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "      </u:%s>\r\n", g_soap_actions[action]);
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "   </s:%s>\r\n", "Body");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "</s:%s>\r\n", "Envelope");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	return (s32)(p - buf);
}

static s32 upnpDescXmlCallback(XmlParser *p, s32 event, s32 depth, s32 idx, const char *val1, const char *val2, void *arg)
{
	UpnpCtx *ctx = (UpnpCtx *)arg;
	s32 ret;

	if (event == 3 && depth == 1 && idx == 0) {
		ssdpParseLocationUrl((char *)val1, &ctx->gw_ip, &ctx->gw_port, ctx->url_base, 0x400);
		ctx->xml_parse_flags |= 1;
	}

	if (event == 3 && (depth == 4 || depth == 6 || depth == 8) && idx == 0) {
		if (sceNetStrcmp("urn:schemas-upnp-org:service:WANIPConnection:1", val1) == 0) {
			g_xml_service_type = 0;
			g_xml_found_service = 1;
			g_xml_service_depth = depth;
			g_xml_service_idx = idx;
		} else if (sceNetStrcmp("urn:schemas-upnp-org:service:WANPPPConnection:1", val1) == 0) {
			g_xml_service_type = 1;
			g_xml_found_service = 1;
			g_xml_service_depth = depth;
			g_xml_service_idx = idx;
		} else {
			return 0;
		}
	}

	if (event == 3 && g_xml_found_service == 1 && depth == g_xml_service_depth && idx == g_xml_service_idx + 1) {
		if (g_xml_service_type == 0) {
			if (ctx->found_svc_mask & 1)
				return 0;
			ret = upnpResolveControlUrl(val1, &ctx->gw_ip, &ctx->gw_port, ctx->url_base,
				ctx->http_buf, 0x400, ctx->ctrl_url[0], 0x400);
			if (ret < 0)
				return ret;
			ctx->found_svc_mask |= 1;
		} else if (g_xml_service_type == 1) {
			if (ctx->found_svc_mask & 2)
				return 0;
			ret = upnpResolveControlUrl(val1, &ctx->gw_ip, &ctx->gw_port, ctx->url_base,
				ctx->http_buf, 0x400, ctx->ctrl_url[1], 0x400);
			if (ret < 0)
				return ret;
			ctx->found_svc_mask |= 2;
		}
		if (ctx->found_svc_mask == 0)
			return (s32)0x8041160b;
		g_xml_found_service = 0;
	}

	if (event == 2 && depth == 0 && idx == 0) {
		if (!(ctx->xml_parse_flags & 1))
			return (s32)0x8041168d;
		ctx->xml_parse_flags |= 0x80000000u;
	}
	return 0;
}

static s32 upnpInitDescXmlParser(UpnpCtx *ctx, XmlParser *parser)
{
	return xmlParserInit(parser, &g_desc_schema, upnpDescXmlCallback, ctx);
}

static s32 upnpResetDescXmlState(void)
{
	return 0;
}

static s32 upnpResolveControlUrl(const char *url, u32 *gw_ip, u16 *gw_port, const char *base_path,
                    char *tmp_buf, u32 tmp_size, char *out_path, u32 out_size)
{
	u16 parsed_port;
	char combined[0x100];
	s32 ret;

	ret = ssdpParseLocationUrl((char *)url, gw_ip, &parsed_port, tmp_buf, tmp_size);
	if (ret >= 0) {
		*gw_port = parsed_port;
		if (sceNetStrlen(tmp_buf) >= out_size)
			return (s32)0x80411685;
		sceNetStrcpy(out_path, tmp_buf);
		return 0;
	}

	if (url[0] == '/') {
		if (sceNetStrlen(url) >= out_size)
			return (s32)0x80411685;
		sceNetStrcpy(out_path, url);
		return 0;
	}

	sceKernelMemset(combined, 0, sizeof(combined));
	ret = sceNetSprintf(combined, "%s%s", base_path, url);
	if (ret >= (s32)out_size)
		return (s32)0x80411685;
	sceNetStrcpy(out_path, combined);
	return 0;
}

static s32 upnpSendSoapRequest(s32 sock, char *buf, u32 bufsize, const SockAddrIn *addr, const char *path,
                    s32 action, s32 svc_type, const char *body, u32 body_len, XmlParser *parser, u32 timeout_us)
{
	s32 ret, hdr_len;

	ret = httpConnectTimeout(sock, addr, timeout_us);
	if (ret < 0)
		return ret;

	hdr_len = httpBuildSoapPostHeader(buf, (s32)bufsize, addr, path, action, svc_type, (s32)body_len);
	if (hdr_len < 0)
		return hdr_len;

	if ((u32)hdr_len + body_len <= bufsize) {
		sceKernelMemcpy(buf + hdr_len, body, body_len);
		ret = sceNetInetSend(sock, buf, (u32)hdr_len + body_len, 0);
		if (ret < 0)
			return ret;
	} else {
		ret = sceNetInetSend(sock, buf, (u32)hdr_len, 0);
		if (ret < 0)
			return ret;
		ret = sceNetInetSend(sock, body, body_len, 0);
		if (ret < 0)
			return ret;
	}

	return httpRecvResponse(sock, buf, bufsize, parser);
}

static s32 upnpExtIpXmlCallback(XmlParser *p, s32 event, s32 depth, s32 idx, const char *val1, const char *val2, void *arg)
{
	UpnpCtx *ctx = (UpnpCtx *)arg;
	s32 ret = 0;

	if (event == 3 && depth == 0 && idx == 0) {
		if (sceNetStrlen(val1) >= 0x10)
			return (s32)0x80411609;
		sceKernelMemset(ctx->ext_ip_str, 0, 0x10);
		sceNetStrcpy(ctx->ext_ip_str, val1);
		ctx->xml_parse_flags |= 2;
	}

	if (event == 2 && depth == 0 && idx == 0) {
		if (!(ctx->xml_parse_flags & 2))
			return (s32)0x8041168d;
		ctx->xml_parse_flags |= 0x80000000u;
	}
	return ret;
}

static s32 upnpHttpGetXml(s32 sock, char *buf, u32 bufsize, const SockAddrIn *addr, const char *path, XmlParser *parser)
{
	s32 ret;

	ret = sceNetInetConnect(sock, addr, sizeof(SockAddrIn));
	if (ret < 0)
		return ret;

	ret = httpBuildGetRequest(buf, (s32)bufsize, addr, path);
	if (ret < 0)
		return ret;

	ret = sceNetInetSend(sock, buf, (u32)ret, 0);
	if (ret < 0)
		return ret;

	return httpRecvResponse(sock, buf, bufsize, parser);
}

static s32 upnpSoapFaultXmlCallback(XmlParser *p, s32 event, s32 depth, s32 idx, const char *val1, const char *val2, void *arg)
{
	UpnpCtx *ctx = (UpnpCtx *)arg;
	s32 err = 0;

	if (event == 3 && depth == 5 && idx == 0) {
		s32 code = sceNetStrtoul(val1, NULL, 10);
		upnpMapErrorCode(&err, code);
		return err;
	}

	if (event == 2 && depth == 0 && idx == 0) {
		ctx->xml_parse_flags |= 0x80000000u;
	}
	return err;
}

static s32 upnpInitSoapXmlParser(UpnpCtx *ctx, XmlParser *parser, s32 action)
{
	if (action == 0 || action == 1)
		return xmlParserInit(parser, &g_soap_schema, upnpSoapFaultXmlCallback, ctx);
	if (action == 2)
		return xmlParserInit(parser, &g_extip_schema, upnpExtIpXmlCallback, ctx);
	return (s32)0x8041168f;
}

static s32 httpBuildGetRequest(char *buf, s32 bufsize, const SockAddrIn *addr, const char *path)
{
	char ip_str[0x10];
	char line[0x80];
	char *p = buf;
	s32 rem = bufsize;
	s32 n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "GET %s HTTP/1.1\r\n", path);
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	const char *host = sceNetInetInetNtop(2, &addr->sin_addr, ip_str, sizeof(ip_str));
	n = sceNetSprintf(line, "HOST: %s:%d\r\n", host, (u32)bswap16(addr->sin_port));
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "\r\n");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	return (s32)(p - buf);
}

static s32 httpRecvResponse(s32 sock, char *buf, u32 bufsize, XmlParser *parser)
{
	u32 cap = bufsize - 1;
	char *end = buf + cap;
	char *cur = buf;
	u32 buffered = 0;
	s32 discard_line = 0;
	s32 header_done = 0;
	u32 content_len = 0;
	s32 ret;

	*end = '\0';

	for (;;) {
		if (cur >= end) {
			discard_line = 1;
			cur = buf;
			buffered = 0;
		}
		ret = sceNetInetRecv(sock, cur, (u32)(end - cur), 0);
		if (ret < 0)
			return ret;
		if (ret == 0)
			return (s32)0x8041168b;
		buffered += (u32)ret;

		for (;;) {
			s32 line_len = ssdpFindLocationHeader(buf, buffered);
			if (line_len == (s32)0x80411688) {
				cur = buf + buffered;
				break;
			}
			if (line_len < 0)
				return (s32)0x8041168b;

			char *next_line = buf + line_len;
			if (discard_line == 1) {
				discard_line = 0;
			} else if (buf[0] == '\0') {
				header_done = 1;
			} else {
				s32 val_off = ssdpCheckStatusOk(buf);
				if (val_off != (s32)0x80411689) {
					if (val_off < 0)
						return (s32)0x8041168b;
					char *val = buf + val_off;
					if (sceNetStrcasecmp(buf, "Content-Length") == 0) {
						if (sceNetStrlen(val) < 5)
							content_len = (u32)sceNetStrtoul(val, NULL, 10);
					}
				}
			}

			buffered -= (u32)(next_line - buf);
			sceNetMemmove(buf, next_line, buffered);
			if (header_done == 1) {
				ret = httpRecvBodyToXml(sock, buf, buffered, content_len, cap, parser);
				if (ret < 0)
					return ret;
				if (content_len != 0 && (u32)ret != content_len)
					return (s32)0x8041168d;
				return ret;
			}
		}
	}
}

static s32 httpBuildSoapPostHeader(char *buf, s32 bufsize, const SockAddrIn *addr, const char *path,
                    s32 action, s32 svc_type, s32 content_len)
{
	char ip_str[0x10];
	char line[0x80];
	char *p = buf;
	s32 rem = bufsize;
	s32 n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "POST %s HTTP/1.1\r\n", path);
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	const char *host = sceNetInetInetNtop(2, &addr->sin_addr, ip_str, sizeof(ip_str));
	n = sceNetSprintf(line, "HOST: %s:%d\r\n", host, (u32)bswap16(addr->sin_port));
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "Content-Length: %d\r\n", content_len);
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "Content-Type: %s\r\n", "text/xml; charset=\"utf-8\"");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "SOAPACTION: \"%s#%s\"\r\n",
		g_service_types[svc_type], g_soap_actions[action]);
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "\r\n");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	return (s32)(p - buf);
}

static s32 httpConnectTimeout(s32 sock, const SockAddrIn *addr, u32 timeout_us)
{
	FdSet rfds, wfds;
	TimeVal tv;
	s32 nonblock;
	s32 so_err;
	s32 optlen;
	s32 ret;
	u32 elapsed = 0;

	if (timeout_us == 0)
		return sceNetInetConnect(sock, addr, sizeof(SockAddrIn));

	nonblock = 1;
	ret = sceNetInetSetsockopt(sock, 0xffff, 0x1009, &nonblock, 4);
	if (ret < 0)
		return ret;

	ret = sceNetInetConnect(sock, addr, sizeof(SockAddrIn));
	if (ret >= 0)
		return 0;

	if (sceNetInetGetPspError() != (s32)0x80010077)
		return ret;

	while (elapsed < timeout_us) {
		sceKernelMemset(&rfds, 0, sizeof(rfds));
		rfds.bits[sock >> 5] |= (1u << (sock & 31));
		sceKernelMemset(&wfds, 0, sizeof(wfds));
		wfds.bits[sock >> 5] |= (1u << (sock & 31));
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		ret = sceNetInetSelect(sock + 1, &rfds, &wfds, NULL, &tv);
		if (ret < 0)
			return ret;
		if (ret != 0) {
			if ((rfds.bits[sock >> 5] & (1u << (sock & 31))) ||
			    (wfds.bits[sock >> 5] & (1u << (sock & 31)))) {
				so_err = 0;
				optlen = 4;
				ret = sceNetInetGetsockopt(sock, 0xffff, 0x1007, &so_err, &optlen);
				if (ret < 0)
					return ret;
				if (so_err != 0)
					return (s32)(0x80010000u | (u32)so_err);
				break;
			}
		}
		elapsed += 100000;
	}

	if (elapsed >= timeout_us)
		return (s32)0x8041160a;

	nonblock = 0;
	return sceNetInetSetsockopt(sock, 0xffff, 0x1009, &nonblock, 4);
}

static s32 httpRecvBodyToXml(s32 sock, char *buf, u32 initial_len, u32 content_len, u32 buf_cap, XmlParser *parser)
{
	u32 total = 0;
	s32 ret;

	if (initial_len != 0) {
		u32 chunk = initial_len;
		if (content_len != 0 && chunk > content_len)
			chunk = content_len;
		ret = xmlParserFeed(parser, buf, (s32)chunk);
		if (ret < 0)
			return ret;
		total = chunk;
	}

	while (content_len == 0 || total < content_len) {
		ret = sceNetInetRecv(sock, buf, buf_cap, 0);
		if (ret < 0)
			return ret;
		if (ret == 0)
			return (s32)total;
		u32 chunk = (u32)ret;
		if (content_len != 0 && total + chunk > content_len)
			chunk = content_len - total;
		ret = xmlParserFeed(parser, buf, (s32)chunk);
		total += chunk;
		if (ret < 0)
			return ret;
	}

	return (s32)total;
}

/* =========================================================================
 * UPnP lifecycle, handlers, NAT info, event flag & registry helpers (0x2d74..0x44a7)
 * ========================================================================= */

static s32 upnpInitInternal(s32 stack_size, s32 prio)
{
	char env_buf[9];
	UpnpCtx *ctx = &g_upnp;
	s32 ret;

	sceKernelMemset(g_upnp_handlers, 0, sizeof(g_upnp_handlers));
	sceKernelMemset(ctx, 0, sizeof(UpnpCtx));
	sceKernelMemset(env_buf, 0, sizeof(env_buf));
	sceKernelMemset(ctx->stun_host, 0, sizeof(ctx->stun_host));

	ret = upnpReadRegistryString("/CONFIG/NP", "env", env_buf, sizeof(env_buf));
	if (ret >= 0) {
		sceNetSprintf(ctx->stun_host, "%s.%s.%s", "us", env_buf, "stun.playstation.net");
	}
	if (ret < 0)
		goto fail;

	ctx->stack_size = stack_size;
	ctx->thread_prio = prio;
	ctx->state = 0;
	ctx->apctl_handler_id = -1;
	ctx->sock_fd = -1;
	ctx->thread_id = -1;
	ctx->evf_id = -1;
	ctx->mutex_id = -1;

	ret = sceKernelCreateEventFlag("SceNetUpnpEvf", 0, 0, NULL);
	if (ret < 0)
		goto fail;
	ctx->evf_id = ret;

	ret = sceKernelCreateMutex("SceNetUpnpMutex", 0x200, 0, NULL);
	if (ret < 0)
		goto fail;
	ctx->mutex_id = ret;
	ctx->flags |= 8;

	ret = sceKernelCreateThread("SceNetUpnpThread", upnpWorkerThread, ctx->thread_prio, ctx->stack_size, 0, NULL);
	if (ret < 0)
		goto fail;
	ctx->thread_id = ret;

	ret = sceKernelStartThread(ctx->thread_id, 4, &ctx);
	if (ret < 0)
		goto fail;

	ret = stunInitGlobal();
	if (ret < 0)
		goto fail;

	return 0;

fail:
	stunTermGlobal();
	if (g_upnp.thread_id != -1) {
		sceKernelWaitThreadEnd(g_upnp.thread_id, NULL);
		sceKernelDeleteThread(g_upnp.thread_id);
		sceNetFreeThreadinfo(g_upnp.thread_id);
		g_upnp.thread_id = -1;
	}
	if (g_upnp.evf_id != -1) {
		sceKernelDeleteEventFlag(g_upnp.evf_id);
		g_upnp.evf_id = -1;
	}
	if (g_upnp.flags & 8) {
		sceKernelDeleteMutex(g_upnp.mutex_id);
		g_upnp.mutex_id = -1;
		g_upnp.flags &= ~8u;
	}
	return ret;
}

static s32 upnpAddHandlerInternal(UpnpHandlerCb cb, void *arg, s32 *out_id)
{
	s32 ret, i;

	if (cb == NULL)
		return (s32)0x80411605;

	ret = sceKernelLockMutex(g_upnp.mutex_id, 1, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < 3; i++) {
		if (g_upnp_handlers[i].cb == NULL)
			break;
	}
	if (i == 3) {
		ret = (s32)0x8041160c;
	} else {
		g_upnp_handlers[i].id = i;
		g_upnp_handlers[i].cb = cb;
		g_upnp_handlers[i].arg = arg;
		if (out_id != NULL)
			*out_id = i;
	}
	sceKernelUnlockMutex(g_upnp.mutex_id, 1);
	return ret;
}

static s32 upnpGetNatInfoExInternal(UpnpNatInfoEx *info)
{
	sceKernelMemset(info, 0, sizeof(UpnpNatInfoEx));
	sceKernelMemcpy(info->stun_mapped_ip, &g_upnp.stun_mapped_ip, 4);
	sceNetInetInetPton(2, g_upnp.ext_ip_str, info->upnp_ext_ip);

	if ((u32)(g_upnp.state - 5) >= 3u) {
		info->ip_match = 0;
		info->upnp_status = 0;
		info->stun_status = 0;
		return 0;
	}

	if (g_upnp.flags & 4) {
		u32 ip1, ip2;
		info->upnp_status = 2;
		info->ext_port = g_upnp.ext_port;
		sceKernelMemcpy(&ip1, info->stun_mapped_ip, 4);
		sceKernelMemcpy(&ip2, info->upnp_ext_ip, 4);
		info->ip_match = (ip1 == ip2) ? 2 : 1;
	} else {
		info->ext_port = 0x0e4a;
		info->ip_match = 1;
		info->upnp_status = 1;
	}

	if (g_upnp.state == 5) {
		info->stun_status = 0;
		return 0;
	}

	if (!(g_upnp.flags & 0x10)) {
		info->stun_status = 1;
		return 0;
	}

	info->stun_status = 2;
	sceKernelMemcpy(info->stun_result, g_upnp.stun_result, 0x10);
	info->stun_ext_ok = (g_upnp.stun_ext_ok == 1) ? 1 : 0;

	if (info->stun_result[0] == g_upnp.local_ip) {
		info->nat_type = 1;
	} else if (info->ip_match == 2) {
		info->nat_type = 2;
	} else if (info->stun_result[1] != 0) {
		info->nat_type = 3;
	} else {
		info->nat_type = 2;
	}
	return 0;
}

static s32 upnpMapErrorCode(s32 *out_err, s32 upnp_code)
{
	u32 err;
	switch (upnp_code) {
	case 401: err = 0x80411671u; break;
	case 402: err = 0x80411672u; break;
	case 404: err = 0x80411673u; break;
	case 501: err = 0x80411674u; break;
	case 714: err = 0x80411675u; break;
	case 715: err = 0x80411676u; break;
	case 716: err = 0x80411677u; break;
	case 718: err = 0x80411678u; break;
	case 724: err = 0x80411679u; break;
	case 725: err = 0x8041167au; break;
	case 726: err = 0x8041167bu; break;
	case 727: err = 0x8041167cu; break;
	default:  err = 0x8041167du; break;
	}
	*out_err = (s32)err;
	return 0;
}

static s32 upnpNotifyHandlers(UpnpCtx *ctx, s32 prev_state)
{
	s32 ret, i;

	ret = sceKernelLockMutex(ctx->mutex_id, 1, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < 3; i++) {
		if (g_upnp_handlers[i].cb != NULL) {
			s32 ev = ctx->event_code;
			s32 err = (ev == 4 || ev == 8 || ev == 9) ? ctx->last_error : 0;
			g_upnp_handlers[i].cb(prev_state, ctx->state, ev, err, g_upnp_handlers[i].arg);
		}
	}

	sceKernelUnlockMutex(ctx->mutex_id, 1);
	return 0;
}

static void upnpTermInternal(void)
{
	if (g_upnp.flags & 1) {
		if (g_upnp.sock_fd >= 0)
			sceNetInetSocketAbort(g_upnp.sock_fd);
		sceKernelSetEventFlag(g_upnp.evf_id, 4);
	}
	sceKernelSetEventFlag(g_upnp.evf_id, 1);
	stunTermGlobal();

	if (g_upnp.thread_id != -1) {
		sceKernelWaitThreadEnd(g_upnp.thread_id, NULL);
		sceKernelDeleteThread(g_upnp.thread_id);
		sceNetFreeThreadinfo(g_upnp.thread_id);
		g_upnp.thread_id = -1;
	}
	if (g_upnp.evf_id != -1) {
		sceKernelDeleteEventFlag(g_upnp.evf_id);
		g_upnp.evf_id = -1;
	}
	if (g_upnp.flags & 8) {
		sceKernelDeleteMutex(g_upnp.mutex_id);
		g_upnp.mutex_id = -1;
		g_upnp.flags &= ~8u;
	}
}

static s32 upnpDelHandlerInternal(s32 id)
{
	s32 ret;

	ret = sceKernelLockMutex(g_upnp.mutex_id, 1, NULL);
	if (ret < 0)
		return ret;

	if ((u32)id >= 3u) {
		ret = (s32)0x8041160d;
	} else if (g_upnp_handlers[id].cb == NULL) {
		ret = (s32)0x8041160e;
	} else {
		g_upnp_handlers[id].cb = NULL;
	}

	sceKernelUnlockMutex(g_upnp.mutex_id, 1);
	return ret;
}

static s32 upnpStartInternal(s32 ext_flag)
{
	if (g_upnp.flags & 1)
		return (s32)0x80411603;

	g_upnp.flags |= 1;
	if (ext_flag == 1)
		g_upnp.flags |= 0x800;
	else
		g_upnp.flags &= ~0x800u;

	g_upnp.use_upnp = 1;
	sceKernelSetEventFlag(g_upnp.evf_id, 2);
	return 0;
}

static s32 upnpStopInternal(void)
{
	if (g_upnp.sock_fd >= 0)
		sceNetInetSocketAbort(g_upnp.sock_fd);
	sceKernelSetEventFlag(g_upnp.evf_id, 4);
	return 0;
}

static s32 upnpAbortInternal(void)
{
	if (g_upnp.sock_fd >= 0)
		sceNetInetSocketAbort(g_upnp.sock_fd);
	sceKernelSetEventFlag(g_upnp.evf_id, 0x20);
	return 0;
}

static s32 upnpGetStateInternal(s32 *out_state)
{
	*out_state = g_upnp.state;
	return 0;
}

static s32 upnpGetNatInfoInternal(UpnpNatInfo *info)
{
	UpnpNatInfoEx ex;
	s32 ret;

	sceKernelMemset(&ex, 0, sizeof(ex));
	ret = upnpGetNatInfoExInternal(&ex);
	if (ret < 0)
		return ret;

	if (info != NULL) {
		info->upnp_status = ex.upnp_status;
		info->stun_status = ex.stun_status;
		info->nat_type = ex.nat_type;
		sceKernelMemcpy(info->stun_mapped_ip, ex.stun_mapped_ip, 4);
	}
	return ret;
}

static s32 upnpGetLastErrorInternal(s32 *out_err)
{
	*out_err = g_upnp.last_error;
	return 0;
}

static s32 upnpSetEventBits(u32 bits)
{
	return sceKernelClearEventFlag(g_upnp.evf_id, bits);
}

static s32 upnpClearEventBits(u32 bits)
{
	return sceKernelSetEventFlag(g_upnp.evf_id, bits);
}

static s32 upnpWaitEventBits(u32 bits, u32 *out_bits)
{
	return sceKernelWaitEventFlag(g_upnp.evf_id, bits, 0x21, out_bits, NULL);
}

static s32 upnpPollEventBits(u32 bits, u32 *out_bits)
{
	return sceKernelPollEventFlag(g_upnp.evf_id, bits, 0x21, out_bits);
}

static s32 upnpReadRegistryString(const char *cat, const char *key, char *out, s32 out_size)
{
	u32 reg_param[0x44];
	u32 reg_h, cat_h, key_h, key_type, key_size;
	s32 ret;

	sceKernelMemset(reg_param, 0, sizeof(reg_param));
	reg_param[0] = 1;
	reg_param[0x42] = 1;
	reg_param[0x43] = 1;

	ret = sceRegOpenRegistry(reg_param, 2, &reg_h);
	if (ret != 0)
		return ret;

	ret = sceRegOpenCategory(reg_h, cat, 2, &cat_h);
	if (ret != 0) {
		sceRegCloseRegistry(reg_h);
		return ret;
	}

	ret = sceRegGetKeyInfo(cat_h, key, &key_h, &key_type, &key_size);
	if (ret != 0) {
		sceRegCloseCategory(cat_h);
		sceRegCloseRegistry(reg_h);
		return ret;
	}

	if (key_type != 3) {
		sceRegCloseCategory(cat_h);
		sceRegCloseRegistry(reg_h);
		return (s32)0x80411690;
	}

	if (out_size < (s32)key_size) {
		sceRegCloseCategory(cat_h);
		sceRegCloseRegistry(reg_h);
		return (s32)0x80411691;
	}

	ret = sceRegGetKeyValue(cat_h, key_h, out, (u32)out_size);
	if (ret != 0) {
		sceRegCloseCategory(cat_h);
		sceRegCloseRegistry(reg_h);
		return ret;
	}

	if (out[key_size - 1] != '\0')
		out[key_size - 1] = '\0';

	sceRegCloseCategory(cat_h);
	sceRegCloseRegistry(reg_h);
	return 0;
}

static s32 ssdpParseLocationUrl(char *url, u32 *expected_ip, u16 *out_port, char *out_path, u32 out_path_size)
{
	const char *prefix = "http://";
	u32 plen = sceNetStrlen(prefix);
	char *host, *slash, *colon;
	u32 ip;

	if (sceNetStrncmp(url, prefix, plen) != 0)
		return (s32)0x80411689;

	host = url + sceNetStrlen(prefix);
	slash = sceNetStrchr(host, '/');
	if (slash != NULL) {
		if (sceNetStrlen(slash) >= out_path_size)
			return (s32)0x80411689;
		sceNetStrcpy(out_path, slash);
		*slash = '\0';
	} else {
		if (out_path_size < 2)
			return (s32)0x80411689;
		sceNetStrcpy(out_path, "/");
	}

	colon = sceNetStrchr(host, ':');
	if (colon != NULL) {
		char *port_str = colon + 1;
		*colon = '\0';
		if (sceNetStrlen(port_str) >= 6)
			return (s32)0x80411689;
		*out_port = (u16)sceNetStrtoul(port_str, NULL, 10);
	} else {
		*out_port = 80;
	}

	ip = sceNetInetInetAddr(host);
	if (ip == 0xffffffffu)
		return (s32)0x80411689;

	u32 exp;
	sceKernelMemcpy(&exp, expected_ip, 4);
	if (ip != exp)
		return (s32)0x80411689;

	return 0;
}

static s32 ssdpRecvResponse(s32 sock, char *buf, u32 bufsize, u32 *gw_ip, u16 *gw_port,
                    char *desc_path, u32 desc_path_size)
{
	SockAddrIn mcast_addr;
	TimeVal tv;
	FdSet rfds;
	u8 ttl = 4;
	s32 retry = 0;
	s32 ret;

	ret = sceNetInetSetsockopt(sock, 0, 10, &ttl, 1);
	if (ret < 0)
		return ret;

	sceKernelMemset(&mcast_addr, 0, sizeof(mcast_addr));
	mcast_addr.sin_addr = sceNetInetInetAddr("239.255.255.250");
	if (mcast_addr.sin_addr == 0xffffffffu)
		return (s32)0x80411686;
	mcast_addr.sin_family = 2;
	mcast_addr.sin_port = bswap16(1900);

	while ((u32)retry < 6u) {
		ret = ssdpBuildMSearch(buf, (s32)bufsize);
		retry++;
		if (ret < 0)
			return ret;

		ret = sceNetInetSendto(sock, buf, (u32)ret, 0, &mcast_addr, sizeof(mcast_addr));
		if (ret < 0)
			return ret;

		u64 deadline = sceKernelGetSystemTimeWide() + 2000000ULL;
		for (;;) {
			u64 now = sceKernelGetSystemTimeWide();
			if (now >= deadline)
				break;
			u64 rem = deadline - now;
			sceKernelMemset(&tv, 0, sizeof(tv));
			tv.tv_sec = (s32)(rem / 1000000ULL);
			tv.tv_usec = (s32)(rem % 1000000ULL);

			sceKernelMemset(&rfds, 0, sizeof(rfds));
			rfds.bits[sock >> 5] |= (1u << (sock & 31));
			ret = sceNetInetSelect(sock + 1, &rfds, NULL, NULL, &tv);
			if (ret < 0)
				return ret;
			if (ret == 0)
				break;

			ret = ssdpDiscoverGateway(sock, buf, bufsize, gw_ip, gw_port, desc_path, desc_path_size);
			if (ret != (s32)0x8041168a)
				return ret;
		}
	}

	return (s32)0x80411687;
}

static s32 ssdpFindLocationHeader(char *buf, u32 len)
{
	char *nl = sceNetStrchr(buf, '\n');
	if (nl == NULL || (u32)(nl - buf) >= len)
		return (s32)0x80411688;

	char *end = nl;
	*nl = '\0';
	char *next = nl + 1;

	if (end > buf && end[-1] == '\r') {
		end--;
		*end = '\0';
	}
	while (end > buf && (end[-1] == ' ' || end[-1] == '\t')) {
		end--;
		*end = '\0';
	}
	return (s32)(next - buf);
}

static s32 ssdpCheckStatusOk(char *line)
{
	char *colon = sceNetStrchr(line, ':');
	if (colon == NULL)
		return (s32)0x80411689;

	char *p = colon;
	*colon = '\0';
	char *val = colon + 1;

	while (p > line && (p[-1] == ' ' || p[-1] == '\t')) {
		p--;
		*p = '\0';
	}
	while (*val == ' ' || *val == '\t')
		val++;

	return (s32)(val - line);
}

static s32 ssdpBuildMSearch(char *buf, s32 bufsize)
{
	char line[0x40];
	char *p = buf;
	s32 rem = bufsize;
	s32 n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "M-SEARCH * HTTP/1.1\r\n");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "HOST: %s:%d\r\n", "239.255.255.250", 1900);
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "MAN: %s\r\n", "\"ssdp:discover\"");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "MX: %d\r\n", 5);
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "ST: %s\r\n", "urn:schemas-upnp-org:device:InternetGatewayDevice:1");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "\r\n");
	if (n < 0 || n >= rem)
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	return (s32)(p - buf);
}

static s32 ssdpDiscoverGateway(s32 sock, char *buf, u32 bufsize, u32 *gw_ip, u16 *gw_port,
                    char *desc_path, u32 desc_path_size)
{
	SockAddrIn from;
	s32 fromlen = sizeof(from);
	s32 got_loc = 0;
	s32 got_st = 0;
	s32 ret;

	buf[bufsize - 1] = '\0';
	ret = sceNetInetRecvfrom(sock, buf, bufsize - 1, 0, &from, &fromlen);
	if (ret < 0)
		return ret;

	u32 exp_ip;
	sceKernelMemcpy(&exp_ip, gw_ip, 4);
	if (from.sin_addr != exp_ip)
		return (s32)0x8041168a;

	u32 rem = (u32)ret;
	char *cur = buf;
	s32 line_len = ssdpFindLocationHeader(cur, rem);
	if (line_len < 0)
		return (s32)0x8041168a;
	cur += line_len;
	rem -= (u32)line_len;

	for (;;) {
		char *line = cur;
		line_len = ssdpFindLocationHeader(cur, rem);
		if (line_len < 0)
			return (s32)0x8041168a;
		if (cur[0] == '\0') {
			if (!got_loc || !got_st)
				return (s32)0x8041168a;
			return line_len;
		}
		cur += line_len;

		s32 val_off = ssdpCheckStatusOk(line);
		if (val_off != (s32)0x80411689) {
			if (val_off < 0)
				return (s32)0x8041168a;
			char *val = line + val_off;
			if (sceNetStrcasecmp(line, "Location") == 0) {
				if (ssdpParseLocationUrl(val, gw_ip, gw_port, desc_path, desc_path_size) >= 0)
					got_loc = 1;
			} else if (sceNetStrcasecmp(line, "ST") == 0) {
				if (sceNetStrcmp(val, "urn:schemas-upnp-org:device:InternetGatewayDevice:1") == 0)
					got_st = 1;
			}
		}
		rem -= (u32)(cur - line);
	}
}

/* =========================================================================
 * UPnP state machine (0x44a8..0x541b)
 * ========================================================================= */

static s32 upnpStateDiscoverIgd(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	SockAddrIn addr;
	char ip_str[0x80];
	XmlParser parser;
	s32 apctl_state;
	u32 ev_bits;
	s32 ret = 0;

	ctx->flags &= ~0x734u;
	ctx->ext_port = 0x0e4a;
	ctx->port2 = 0x244d;
	ctx->port3 = 0x244d;
	ctx->last_error = 0;
	sceKernelMemset(&ctx->stun_mapped_ip, 0, 4);

	if (ctx->apctl_handler_id < 0) {
		ret = sceNetApctlAddHandler(upnpApctlHandler, ctx);
		if (ret < 0)
			goto err_check;
		ctx->apctl_handler_id = ret;
	}

	ret = sceNetApctlGetState(&apctl_state);
	if (ret < 0)
		goto err_check;

	if (apctl_state != 4) {
		upnpWaitEventBits(0x1c, &ev_bits);
		if (ev_bits & 4) {
			*out_event = 6;
			*next_state = 0;
			goto stop_done;
		}
		if (ev_bits & 8)
			goto err_check_poll;
	}

	ret = sceNetApctlGetInfo(8, ip_str);
	if (ret < 0)
		goto err_check;
	ctx->local_ip = sceNetInetInetAddr(ip_str);

	if (!ctx->use_upnp) {
		*out_event = 10;
		*next_state = 5;
		return 0;
	}

	ret = sceNetApctlGetInfo(10, ip_str);
	if (ret < 0)
		goto err_check;
	ctx->gw_ip = sceNetInetInetAddr(ip_str);

	ret = sceNetInetSocket(2, 2, 0);
	if (ret < 0)
		goto err_check;
	ctx->sock_fd = ret;

	ret = ssdpRecvResponse(ctx->sock_fd, ctx->http_buf, 0x400, &ctx->gw_ip, &ctx->gw_port, ctx->desc_path, 0x80);
	sceNetInetClose(ctx->sock_fd);
	ctx->sock_fd = -1;
	if (ret < 0)
		goto err_check;

	ret = upnpInitDescXmlParser(ctx, &parser);
	if (ret < 0)
		goto err_check;

	ret = sceNetInetSocket(2, 1, 0);
	if (ret < 0)
		goto err_check;
	ctx->sock_fd = ret;

	sceKernelMemset(&addr, 0, sizeof(addr));
	addr.sin_family = 2;
	addr.sin_port = bswap16(ctx->gw_port);
	addr.sin_addr = ctx->gw_ip;

	ret = upnpHttpGetXml(ctx->sock_fd, ctx->http_buf, 0x400, &addr, ctx->desc_path, &parser);
	sceNetInetClose(ctx->sock_fd);
	ctx->sock_fd = -1;
	if (ret < 0)
		goto err_check;

	*out_event = 1;
	*next_state = 2;
	upnpResetDescXmlState();
	return 0;

err_check:
err_check_poll:
	if (upnpPollEventBits(0x24, &ev_bits) >= 0) {
		if (ev_bits & 4) {
stop_done:
			*out_event = 6;
			*next_state = 0;
			ctx->flags &= ~1u;
			upnpResetDescXmlState();
			return 0;
		}
		if (ev_bits & 0x20) {
			*out_event = 5;
			*next_state = 1;
			upnpResetDescXmlState();
			return 0;
		}
	}
	*out_event = 9;
	ctx->last_error = ret;
	*next_state = 5;
	upnpResetDescXmlState();
	return 0;
}

static s32 upnpStateAddPortMapping(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	s32 svc_idx = 0;
	s32 soap_err;
	u32 ev_bits;
	s32 ret;

	for (svc_idx = 0; svc_idx < 2; svc_idx++) {
		if (ctx->found_svc_mask & (1u << svc_idx))
			break;
	}
	if (svc_idx == 2) {
		ret = (s32)0x8041160b;
		*out_event = 9;
		goto handle_err;
	}

	ctx->flags |= 0x20;
	for (;;) {
		ret = upnpExecSoapAction(ctx, svc_idx, 2, 0, 0, 0, &soap_err, 0);
		if (ret == (s32)0x80411608) {
			ret = soap_err;
			*out_event = 4;
		} else if (ret < 0) {
			*out_event = 9;
		} else if (ctx->ext_ip_str[0] == '\0') {
			ret = (s32)0x8041167e;
			*out_event = 4;
		} else {
			ctx->active_svc_idx = svc_idx;
			*out_event = 3;
			*next_state = 3;
			return 0;
		}

handle_err:
		if (upnpPollEventBits(0x24, &ev_bits) >= 0) {
			if (ev_bits & 4) {
				*out_event = 6;
				*next_state = 0;
				ctx->flags &= ~1u;
				return 0;
			}
			if (ev_bits & 0x20) {
				*out_event = 5;
				*next_state = 1;
				return 0;
			}
		}
		ctx->last_error = ret;

		svc_idx++;
		while (svc_idx < 2) {
			if (ctx->found_svc_mask & (1u << svc_idx))
				break;
			svc_idx++;
		}
		if (svc_idx == 2) {
			*next_state = 3;
			return 0;
		}
	}
}

static s32 upnpStateDeletePortMapping(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	s32 soap_err = 0;
	u32 ev_bits;
	s32 ret;

	ret = upnpExecSoapAction(ctx, ctx->active_svc_idx, 0, ctx->ext_port, 0, ctx->ext_port, &soap_err, 0);
	if (ret == (s32)0x80411608) {
		if (soap_err == (s32)0x80411678) {
			u16 port = ctx->ext_port;
			if (port == 0x0e4a) {
				ctx->ext_port = 0xc000;
				*out_event = 11;
				*next_state = 3;
				return 0;
			}
			if ((u16)(port - 0xc000) < 14) {
				ctx->ext_port = port + 1;
				*out_event = 11;
				*next_state = 3;
				return 0;
			}
			ctx->ext_port = 0x0e4a;
		}
		ret = soap_err;
		*out_event = 4;
	} else if (ret < 0) {
		*out_event = 9;
	} else {
		ctx->flags |= 4;
		*out_event = 3;
		*next_state = 5;
		return 0;
	}

	if (upnpPollEventBits(0x24, &ev_bits) >= 0) {
		if (ev_bits & 4) {
			*out_event = 6;
			*next_state = 0;
			ctx->flags &= ~1u;
			return 0;
		}
		if (ev_bits & 0x20) {
			*out_event = 5;
			*next_state = 1;
			return 0;
		}
	}
	ctx->last_error = ret;
	*next_state = 5;
	return 0;
}

static s32 upnpStateRunStunProbe(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	StunConfig cfg;
	s32 stun_id = 0;
	u32 ev_bits;
	void *ext_buf = NULL;
	s32 ret;

	sceKernelMemset(&cfg, 0, sizeof(cfg));
	cfg.use_xor = 1;
	cfg.username = bswap32(1);
	sceKernelMemcpy(cfg.key, g_stun_hmac_key, sizeof(g_stun_hmac_key));

	if (ctx->flags & 0x800) {
		cfg.ext_data_len = 0x800;
		ext_buf = sceNetMallocInternal(0x800);
		if (ext_buf == NULL) {
			ret = (s32)0x80411604;
			goto cleanup;
		}
		cfg.has_ext_data = 1;
		cfg.ext_data = ext_buf;
	}

	ret = sceNetStun_EBF3C24D(ctx->stack_size, ctx->thread_prio, ctx->stun_host, 3478, &cfg, upnpStunCallback, &stun_id);
	if (ret < 0)
		goto stun_fail;

	sceKernelMemset(ctx->stun_result, 0, sizeof(ctx->stun_result));
	upnpSetEventBits(~0xc0u);

	ret = sceNetStun_A0CF3A47(stun_id, ctx);
	if (ret < 0)
		goto stun_fail;

	upnpWaitEventBits(0x64, &ev_bits);
	if (ev_bits & 4) {
		if (sceNetStun_40971567(stun_id, ctx) >= 0)
			upnpWaitEventBits(0x80, NULL);
		ctx->flags &= ~1u;
		*next_state = 0;
		*out_event = 6;
		goto cleanup;
	}
	if (ev_bits & 0x20) {
		if (sceNetStun_40971567(stun_id, ctx) >= 0)
			upnpWaitEventBits(0x80, NULL);
		*out_event = 5;
		*next_state = 1;
		goto cleanup;
	}

	if (ctx->stun_cb_status < 0) {
		ctx->last_error = ctx->stun_cb_status;
		*out_event = 8;
		*next_state = (ctx->flags & 4) ? 6 : 7;
		goto cleanup;
	}

	if (ctx->flags & 0x800) {
		ctx->stun_ext_ok = 1;
		ret = sceNetStun_FBF60C6B(stun_id, 0, ctx);
		if (ret >= 0) {
			upnpWaitEventBits(0x64, &ev_bits);
			if (ev_bits & 4) {
				if (sceNetStun_40971567(stun_id, ctx) >= 0)
					upnpWaitEventBits(0x80, NULL);
				ctx->flags &= ~1u;
				*next_state = 0;
				*out_event = 6;
				goto cleanup;
			}
			if (ev_bits & 0x20) {
				if (sceNetStun_40971567(stun_id, ctx) >= 0)
					upnpWaitEventBits(0x80, NULL);
				*out_event = 5;
				*next_state = 1;
				goto cleanup;
			}
			s32 st = ctx->stun_cb_status;
			if (st == (s32)0x80411703 || st == (s32)0x8041170b || st == (s32)0x8041170c)
				ctx->stun_ext_ok = 0;
		}
	}

	ctx->flags |= 0x10;
	*out_event = 7;
	*next_state = (ctx->flags & 4) ? 6 : 7;
	goto cleanup;

stun_fail:
	ctx->last_error = ret;
	*out_event = 9;
	*next_state = 7;

cleanup:
	if (stun_id != 0)
		sceNetStun_4403D9E7(stun_id);
	if (ext_buf != NULL)
		sceNetFreeInternal(ext_buf);
	return ret;
}

static s32 upnpWorkerThread(u32 args, void *argp)
{
	UpnpCtx *ctx = *(UpnpCtx **)argp;
	s32 next_state, out_event;

	for (;;) {
		s32 cur_state = ctx->state;
		g_upnp_state_handlers[cur_state](ctx, cur_state, &next_state, &out_event);
		if (ctx->state == 0 && (ctx->flags & 2)) {
			if (ctx->apctl_handler_id >= 0) {
				sceNetApctlDelHandler(ctx->apctl_handler_id);
				ctx->apctl_handler_id = -1;
			}
			sceKernelExitThread(0);
			return 0;
		}
		ctx->state = next_state;
		ctx->event_code = out_event;
		upnpNotifyHandlers(ctx, cur_state);
	}
}

static s32 upnpStateWaitStart(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	u32 ev_bits;

	ctx->flags &= ~0x734u;
	upnpWaitEventBits(0xffffffffu, &ev_bits);
	if (ev_bits & 1) {
		ctx->flags |= 2;
	} else if (ev_bits & 4) {
		ctx->flags &= ~1u;
		*next_state = 0;
		*out_event = 6;
	} else if (ev_bits & 2) {
		*next_state = 1;
		*out_event = 0;
	}
	return 0;
}

static s32 upnpStateIdle(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	return 0;
}

static s32 upnpStateWaitStop(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	u32 ev_bits;
	s32 soap_err;

	upnpWaitEventBits(0x24, &ev_bits);
	if (ev_bits & 0x20) {
		*out_event = 5;
		*next_state = 1;
		return 0;
	}

	upnpExecSoapAction(ctx, ctx->active_svc_idx, 1, ctx->ext_port, 0, ctx->ext_port, &soap_err, 1000000);
	*out_event = 6;
	*next_state = 0;
	ctx->flags &= ~1u;
	return 0;
}

static s32 upnpStateWaitApctl(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	u32 ev_bits;
	s32 soap_err;

	upnpWaitEventBits(0x24, &ev_bits);
	if (ev_bits & 0x20) {
		*out_event = 5;
		*next_state = 1;
		return 0;
	}

	upnpExecSoapAction(ctx, ctx->active_svc_idx, 1, ctx->ext_port, 0, ctx->ext_port, &soap_err, 1000000);
	*out_event = 6;
	*next_state = 0;
	ctx->flags &= ~1u;
	return 0;
}

static void upnpApctlHandler(s32 prev_state, s32 new_state, s32 event, s32 err, void *arg)
{
	UpnpCtx *ctx = (UpnpCtx *)arg;
	if (ctx == NULL)
		return;
	if (!(ctx->flags & 1))
		return;
	if (new_state == 4 && event == 4) {
		g_apctl_connected = 1;
		upnpClearEventBits(0x10);
	} else if (new_state == 0 && g_apctl_connected != 0) {
		upnpAbortInternal();
		g_apctl_connected = 0;
	}
}

static s32 upnpExecSoapAction(UpnpCtx *ctx, s32 svc_idx, s32 action, u16 ext_port, s32 proto,
                    u16 int_port, s32 *out_err, u32 timeout_us)
{
	char local_ip_str[0x80];
	SockAddrIn addr;
	XmlParser parser;
	TimeVal tv;
	s32 body_len;
	s32 ret;

	if (action == 0) {
		ret = sceNetApctlGetInfo(8, local_ip_str);
		if (ret < 0)
			return ret;
		body_len = soapBuildAddPortMappingBody(ctx->soap_buf, 0x400, local_ip_str, svc_idx, ext_port, proto, int_port);
	} else if (action == 1) {
		body_len = soapBuildDeletePortMappingBody(ctx->soap_buf, 0x400, svc_idx, ext_port, proto);
	} else if (action == 2) {
		body_len = soapBuildActionBody(ctx->soap_buf, 0x400, 2, svc_idx);
	} else {
		body_len = (s32)0x8041168f;
	}

	if (body_len < 0)
		return body_len;

	ret = upnpInitSoapXmlParser(ctx, &parser, action);
	if (ret < 0)
		return ret;

	ret = sceNetInetSocket(2, 1, 0);
	if (ret < 0)
		return ret;
	ctx->sock_fd = ret;

	if (timeout_us != 0) {
		tv.tv_sec = (s32)(timeout_us / 1000000u);
		tv.tv_usec = (s32)(timeout_us % 1000000u);
		ret = sceNetInetSetsockopt(ctx->sock_fd, 0xffff, 0x1006, &tv, sizeof(tv));
		if (ret < 0) {
			sceNetInetClose(ctx->sock_fd);
			ctx->sock_fd = -1;
			return ret;
		}
	}

	sceKernelMemset(&addr, 0, sizeof(addr));
	addr.sin_family = 2;
	addr.sin_port = bswap16(ctx->gw_port);
	addr.sin_addr = ctx->gw_ip;

	if ((u32)action < 3u) {
		ret = upnpSendSoapRequest(ctx->sock_fd, ctx->http_buf, 0x400, &addr, ctx->ctrl_url[svc_idx],
			action, svc_idx, ctx->soap_buf, (u32)body_len, &parser, timeout_us);
	} else {
		ret = (s32)0x8041168f;
	}

	sceNetInetClose(ctx->sock_fd);
	ctx->sock_fd = -1;

	if (ret < 0) {
		if (out_err != NULL)
			*out_err = ret;
		return (s32)0x80411608;
	}
	return ret;
}

static void upnpStunCallback(s32 stun_id, void *arg, s32 event, void *data)
{
	UpnpCtx *ctx = (UpnpCtx *)arg;

	if (event < 0) {
		ctx->stun_cb_status = event;
		upnpClearEventBits(0x40);
		return;
	}
	if (event == 4) {
		ctx->stun_cb_status = 0;
		upnpClearEventBits(0x80);
	} else if (event == 5) {
		sceKernelMemcpy(&ctx->stun_mapped_ip, data, 4);
	} else if (event == 1) {
		ctx->stun_cb_status = 0;
		sceKernelMemcpy(ctx->stun_result, data, 0x10);
		upnpClearEventBits(0x40);
	} else if (event == 2) {
		ctx->stun_cb_status = 0;
		upnpClearEventBits(0x40);
	}
}

/* =========================================================================
 * Streaming XML parser (0x541c..0x5cc7)
 * ========================================================================= */

static s32 xmlParserFeed(XmlParser *p, const char *data, s32 len)
{
	const char *cur = data;
	s32 state = p->state;
	s32 ret = 0;

	while ((s32)(cur - data) < len) {
		switch (state) {
		case 0:
			if ((u8)*cur == '<') {
				p->is_self_closing = 0;
				p->attr_flags = 0;
				p->buf_pos = 0;
				sceKernelMemset(p->tag_name, 0, 0xff);
				sceKernelMemset(p->text_val, 0, 0xff);
				state = 1;
			}
			cur++;
			break;

		case 1:
		case 4: {
			u8 ch = (u8)*cur;
			if (state == 1 && ch == '/' && p->buf_pos == 0) {
				state = 4;
				cur++;
				break;
			}
			if (p->buf_pos < 0xfe &&
			    ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' &&
			    ch != '/' && ch != '<' && ch != '>') {
				if (ch == ':') {
					sceKernelMemset(p->ns_prefix, 0, 0x40);
					sceKernelMemcpy(p->ns_prefix, p->tag_name, p->buf_pos);
					p->buf_pos = 0;
					sceKernelMemset(p->tag_name, 0, 0xff);
					cur++;
					break;
				}
				p->tag_name[p->buf_pos++] = (char)ch;
				cur++;
				break;
			}
			if (ch == '/')
				p->is_self_closing = 1;
			p->tag_name[p->buf_pos] = '\0';
			if (state == 1) {
				s32 matched = 0;
				ret = xmlParserMatchOpenTag(p, &matched);
				if (ret < 0)
					return ret;
				state = matched ? 2 : 0;
			} else if (state == 4) {
				s32 matched = 0;
				ret = xmlParserMatchCloseTag(p, &matched);
				if (ret < 0)
					return ret;
				if (matched == 1)
					p->depth--;
				state = 0;
				cur++;
			} else {
				cur++;
			}
			break;
		}

		case 2: {
			u8 ch = (u8)*cur;
			if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
				cur++;
			} else if (ch == '>') {
				s32 d = p->depth;
				p->depth = d + 1;
				if (p->is_self_closing == 1) {
					p->attr_flags = 0;
					state = 4;
				} else {
					const XmlTagEntry *ent = &p->schema->levels[d][p->match_idx[d]];
					if (ent->has_text == 1) {
						p->buf_pos = 0;
						sceKernelMemset(p->text_val, 0, 0xff);
						p->entity_state = 0;
						p->entity_len = 0;
						sceKernelMemset(p->entity_buf, 0, 6);
						state = 3;
						cur++;
					} else {
						state = 0;
						cur++;
					}
				}
			} else {
				p->buf_pos = 0;
				p->attr_flags = 0;
				sceKernelMemset(p->tag_name, 0, 0xff);
				sceKernelMemset(p->text_val, 0, 0xff);
				state = 5;
			}
			break;
		}

		case 3:
			if (p->buf_pos < 0xff && (u8)*cur != '<') {
				p->text_val[p->buf_pos++] = *cur++;
			} else {
				p->text_val[p->buf_pos] = '\0';
				p->buf_pos = 0;
				sceKernelMemset(p->tag_name, 0, 0xff);
				p->is_self_closing = 0;
				p->attr_flags = 0;
				state = 1;
				cur++;
			}
			break;

		case 5: {
			if (p->buf_pos < 0xfe) {
				if (p->attr_flags != 1) {
					u8 ch = (u8)*cur;
					if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' &&
					    ch != '/' && ch != '=' && ch != '>') {
						p->tag_name[p->buf_pos++] = (char)ch;
						cur++;
						break;
					}
					p->tag_name[p->buf_pos] = '\0';
					p->attr_flags = 1;
				}
				u8 ch = (u8)*cur;
				if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
					cur++;
					break;
				}
				if (ch == '/') {
					p->is_self_closing = 1;
					cur++;
					break;
				}
				if (ch == '=') {
					p->buf_pos = 0;
					p->attr_flags = 0;
					state = 6;
					cur++;
					break;
				}
			}
			ret = p->cb(p, 4, p->depth, p->match_idx[p->depth], p->tag_name, p->text_val, p->user_arg);
			if (ret < 0)
				return ret;
			state = 2;
			break;
		}

		case 6: {
			if (p->buf_pos < 0xfe) {
				u8 ch = (u8)*cur;
				if ((p->attr_flags & 2) && ch != '"' && ch != '\'') {
					p->text_val[p->buf_pos++] = (char)ch;
					cur++;
					break;
				}
				if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
					if (!(p->attr_flags & 1)) {
						cur++;
						break;
					}
				} else if (ch == '"' || ch == '\'') {
					if (p->attr_flags & 2)
						p->attr_flags &= ~2u;
					else
						p->attr_flags |= 3u;
					cur++;
					break;
				} else if (ch != '/' && ch != '>') {
					p->attr_flags |= 1u;
					p->text_val[p->buf_pos++] = (char)ch;
					cur++;
					break;
				} else if (ch == '/') {
					p->is_self_closing = 1;
				}
			}
			p->text_val[p->buf_pos] = '\0';
			ret = p->cb(p, 4, p->depth, p->match_idx[p->depth], p->tag_name, p->text_val, p->user_arg);
			if (ret < 0)
				return ret;
			state = 2;
			break;
		}

		default:
			p->state = state;
			return ret;
		}
	}

	p->state = state;
	return 0;
}

static s32 xmlParserInit(XmlParser *p, const XmlSchema *schema, XmlCallback cb, void *arg)
{
	sceKernelMemset(p, 0, sizeof(XmlParser));
	p->schema = schema;
	p->cb = cb;
	p->user_arg = arg;
	return 0;
}

static s32 xmlParserMatchOpenTag(XmlParser *p, s32 *out_matched)
{
	const XmlSchema *schema = p->schema;
	s32 depth = p->depth;

	if ((u32)depth >= schema->num_levels) {
		*out_matched = 0;
		return 0;
	}

	const XmlTagEntry *level = schema->levels[depth];
	s32 idx = 0;
	while (sceNetStrlen(level[idx].name) != 0) {
		if (sceNetStrcasecmp(level[idx].name, p->tag_name) == 0)
			break;
		idx++;
	}

	if (sceNetStrlen(level[idx].name) == 0) {
		*out_matched = 0;
		return 0;
	}

	*out_matched = 1;
	p->match_idx[depth] = idx;
	return p->cb(p, 1, depth, idx, NULL, NULL, p->user_arg);
}

static s32 xmlParserMatchCloseTag(XmlParser *p, s32 *out_matched)
{
	s32 depth = p->depth;
	if (depth == 0) {
		*out_matched = 0;
		return 0;
	}

	const XmlSchema *schema = p->schema;
	if ((u32)depth > schema->num_levels) {
		*out_matched = 0;
		return 0;
	}

	const XmlTagEntry *level = schema->levels[depth - 1];
	s32 idx = p->match_idx[depth - 1];
	if (p->is_self_closing != 1) {
		if (sceNetStrcasecmp(level[idx].name, p->tag_name) != 0) {
			*out_matched = 0;
			return 0;
		}
	}

	*out_matched = 1;
	if (level[idx].has_text == 1)
		return p->cb(p, 3, depth - 1, idx, p->text_val, NULL, p->user_arg);
	return p->cb(p, 2, depth - 1, idx, NULL, NULL, p->user_arg);
}

/* =========================================================================
 * HMAC-SHA1 (0x5cc8..0x5e47)
 * ========================================================================= */

static void hmacSha1Compute(const u8 *data, s32 data_len, const u8 *key, s32 key_len, u8 *out_digest)
{
	Sha1Ctx ctx;
	u8 k_ipad[0x41];
	u8 k_opad[0x41];
	u8 tk[0x14];
	Sha1Ctx tctx;
	s32 i;

	if (key_len > 64) {
		sha1Init(&tctx);
		sha1Update(&tctx, key, (u32)key_len);
		sha1Final(&tctx, tk);
		key = tk;
		key_len = 20;
	}

	sceKernelMemset(k_ipad, 0, sizeof(k_ipad));
	sceKernelMemset(k_opad, 0, sizeof(k_opad));
	sceKernelMemcpy(k_ipad, key, (u32)key_len);
	sceKernelMemcpy(k_opad, key, (u32)key_len);

	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	sha1Init(&ctx);
	sha1Update(&ctx, k_ipad, 64);
	sha1Update(&ctx, data, (u32)data_len);
	sha1Final(&ctx, out_digest);

	sha1Init(&ctx);
	sha1Update(&ctx, k_opad, 64);
	sha1Update(&ctx, out_digest, 20);
	sha1Final(&ctx, out_digest);
}

/* =========================================================================
 * sceNetStun init/term & exports (0x5e48..0x6013)
 * ========================================================================= */

static s32 stunInitGlobal(void)
{
	s32 ret;
	if (g_stun_initialized > 0)
		return (s32)0x80411701;
	ret = stunInitLib();
	if (ret < 0)
		return ret;
	g_stun_initialized = 1;
	return 0;
}

static s32 stunTermGlobal(void)
{
	s32 ret;
	if (!g_stun_initialized)
		return 0;
	ret = stunTermLib();
	if (ret < 0)
		return ret;
	g_stun_initialized = 0;
	return 0;
}

s32 sceNetStun_EBF3C24D(s32 stack_size, s32 prio, const char *host, u16 port,
                        const StunConfig *cfg, StunCallback cb, s32 *out_id)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunCreateInternal(stack_size, prio, host, port, cfg, cb, out_id);
}

s32 sceNetStun_4403D9E7(s32 id)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunDeleteInternal(id);
}

s32 sceNetStun_A0CF3A47(s32 id, void *arg)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunStartNatCheckInternal(id, arg);
}

s32 sceNetStun_FBF60C6B(s32 id, u16 port, void *arg)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunStartPortCheckInternal(id, port, arg);
}

s32 sceNetStun_6CA54FC9(s32 id, s32 sock_fd, void *arg)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunStartSockCheckInternal(id, sock_fd, arg);
}

s32 sceNetStun_40971567(s32 id, void *arg)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunStopInternal(id, arg);
}

/* =========================================================================
 * STUN client implementation (0x6014..0x85ef)
 * ========================================================================= */

static s32 stunCreateInternal(s32 stack_size, s32 prio, const char *host, u16 port,
                    const StunConfig *cfg, StunCallback cb, s32 *out_id)
{
	s32 idx, ret;
	StunCtx *ctx;

	for (idx = 0; idx < 4; idx++) {
		if (g_stun_ctx[idx].thread_id < 0)
			break;
	}
	if (idx == 4)
		return (s32)0x80411709;

	if (sceNetStrlen(host) >= 0x40)
		return (s32)0x8041170a;

	ctx = &g_stun_ctx[idx];
	sceKernelMemset(ctx->host, 0, 0x40);
	sceNetStrncpy(ctx->host, host, 0x3f);
	ctx->port = port;
	sceKernelMemcpy(&ctx->cfg, cfg, sizeof(StunConfig));
	ctx->cb = cb;
	ctx->sock_fd = -1;

	if (ctx->cfg.has_ext_data != 0) {
		ctx->ext_copy = sceNetMallocInternal(ctx->cfg.ext_data_len);
		if (ctx->ext_copy == NULL) {
			ret = (s32)0x8041170d;
			goto fail;
		}
		sceKernelMemcpy(ctx->ext_copy, ctx->cfg.ext_data, ctx->cfg.ext_data_len);
		ctx->pkt_buf_size = ctx->cfg.ext_data_len + 0x400;
	} else {
		ctx->pkt_buf_size = 0x400;
	}

	ctx->pkt_buf = sceNetMallocInternal(ctx->pkt_buf_size);
	if (ctx->pkt_buf == NULL) {
		ret = (s32)0x8041170d;
		goto fail;
	}

	ret = sceKernelCreateMbx("SceNetStun", 0, NULL);
	ctx->mbx_id = ret;
	if (ret < 0)
		goto fail;

	ret = sceKernelCreateThread("SceNetStun", stunWorkerThread, prio, stack_size, 0, NULL);
	ctx->thread_id = ret;
	if (ret < 0)
		goto fail;

	ret = sceKernelStartThread(ctx->thread_id, 4, &ctx);
	if (ret < 0)
		goto fail;

	*out_id = ctx->id;
	return ret;

fail:
	stunDestroyContext(idx);
	return ret;
}

static s32 stunStopInternal(s32 id, void *arg)
{
	StunCtx *ctx = &g_stun_ctx[id - 1];
	StunMsg *msg;

	if (ctx->thread_id < 0)
		return (s32)0x8041170a;

	ctx->flags |= 1;
	if (ctx->resolver_id > 0)
		sceNetResolverStop(ctx->resolver_id);
	if (ctx->sock_fd >= 0)
		sceNetInetSocketAbort(ctx->sock_fd);
	sceNetFreeThreadinfo(sceKernelGetThreadId());

	msg = (StunMsg *)sceNetMallocInternal(sizeof(StunMsg));
	if (msg == NULL)
		return (s32)0x8041170d;
	sceKernelMemset(msg, 0, sizeof(StunMsg));
	msg->cmd = 4;
	msg->user_arg = arg;
	return sceKernelSendMbx(ctx->mbx_id, msg);
}

static s32 stunInitLib(void)
{
	s32 i;
	sceKernelMemset(g_stun_ctx, 0, sizeof(g_stun_ctx));
	for (i = 0; i < 4; i++) {
		g_stun_ctx[i].id = i + 1;
		g_stun_ctx[i].thread_id = -1;
		g_stun_ctx[i].resolver_id = 0;
		g_stun_ctx[i].mbx_id = -1;
	}
	return 0;
}

static s32 stunTermLib(void)
{
	s32 i;
	for (i = 0; i < 4; i++) {
		StunCtx *ctx = &g_stun_ctx[i];
		if (ctx->thread_id >= 0) {
			ctx->flags |= 1;
			if (ctx->resolver_id != 0)
				sceNetResolverStop(ctx->resolver_id);
			if (ctx->sock_fd >= 0)
				sceNetInetSocketAbort(ctx->sock_fd);
			sceNetFreeThreadinfo(sceKernelGetThreadId());
			sceKernelMemset(&ctx->stop_msg, 0, sizeof(StunMsg));
			ctx->stop_msg.cmd = 5;
			if (sceKernelSendMbx(ctx->mbx_id, &ctx->stop_msg) >= 0)
				stunDestroyContext(i);
		}
	}
	return 0;
}

static s32 stunDeleteInternal(s32 id)
{
	s32 idx = id - 1;
	StunCtx *ctx = &g_stun_ctx[idx];

	if (ctx->thread_id < 0)
		return (s32)0x8041170a;

	ctx->flags |= 1;
	if (ctx->resolver_id != 0)
		sceNetResolverStop(ctx->resolver_id);
	if (ctx->sock_fd >= 0)
		sceNetInetSocketAbort(ctx->sock_fd);
	sceNetFreeThreadinfo(sceKernelGetThreadId());

	sceKernelMemset(&ctx->stop_msg, 0, sizeof(StunMsg));
	ctx->stop_msg.cmd = 5;
	if (sceKernelSendMbx(ctx->mbx_id, &ctx->stop_msg) >= 0)
		stunDestroyContext(idx);
	return 0;
}

static s32 stunStartNatCheckInternal(s32 id, void *arg)
{
	StunCtx *ctx = &g_stun_ctx[id - 1];
	StunMsg *msg;

	ctx->flags = 0;
	msg = (StunMsg *)sceNetMallocInternal(sizeof(StunMsg));
	if (msg == NULL)
		return (s32)0x8041170d;
	sceKernelMemset(msg, 0, sizeof(StunMsg));
	msg->cmd = 1;
	msg->user_arg = arg;
	return sceKernelSendMbx(ctx->mbx_id, msg);
}

static s32 stunStartPortCheckInternal(s32 id, u16 port, void *arg)
{
	StunCtx *ctx = &g_stun_ctx[id - 1];
	StunMsg *msg;

	ctx->flags = 0;
	msg = (StunMsg *)sceNetMallocInternal(sizeof(StunMsg));
	if (msg == NULL)
		return (s32)0x8041170d;
	sceKernelMemset(msg, 0, sizeof(StunMsg));
	msg->cmd = 2;
	msg->user_arg = arg;
	msg->bind_port = port;
	return sceKernelSendMbx(ctx->mbx_id, msg);
}

static s32 stunStartSockCheckInternal(s32 id, s32 sock_fd, void *arg)
{
	StunCtx *ctx = &g_stun_ctx[id - 1];
	StunMsg *msg;

	ctx->flags = 0;
	msg = (StunMsg *)sceNetMallocInternal(sizeof(StunMsg));
	if (msg == NULL)
		return (s32)0x8041170d;
	sceKernelMemset(msg, 0, sizeof(StunMsg));
	msg->cmd = 6;
	msg->user_arg = arg;
	msg->sock_fd = sock_fd;
	return sceKernelSendMbx(ctx->mbx_id, msg);
}

static s32 stunWorkerThread(u32 args, void *argp)
{
	StunCtx *ctx = *(StunCtx **)argp;
	StunMsg *msg = NULL;

	for (;;) {
		if (sceKernelReceiveMbx(ctx->mbx_id, (void **)&msg, NULL) < 0)
			break;
		switch (msg->cmd) {
		case 1:
			stunExecNatCheck(ctx, msg->user_arg);
			break;
		case 2:
			stunExecPortCheck(ctx, msg->bind_port, -1, msg->user_arg);
			break;
		case 4:
			if (ctx->flags & 1) {
				ctx->cb(ctx->id, msg->user_arg, 4, NULL);
				sceNetFreeThreadinfo(sceKernelGetThreadId());
				ctx->flags = 0;
			}
			break;
		case 5:
			sceNetFreeThreadinfo(sceKernelGetThreadId());
			return 0;
		case 6:
			stunExecPortCheck(ctx, 0, msg->sock_fd, msg->user_arg);
			break;
		case 7:
			stunExecExtNatCheck(ctx, msg->user_arg);
			break;
		default:
			break;
		}
		sceNetFreeInternal(msg);
		msg = NULL;
	}

	sceNetFreeThreadinfo(sceKernelGetThreadId());
	return 0;
}

static void stunDestroyContext(s32 idx)
{
	StunCtx *ctx = &g_stun_ctx[idx];

	if (ctx->thread_id >= 0) {
		if (sceKernelWaitThreadEnd(ctx->thread_id, NULL) >= 0) {
			sceKernelDeleteThread(ctx->thread_id);
			sceNetFreeThreadinfo(ctx->thread_id);
		}
		ctx->thread_id = -1;
	}
	if (ctx->mbx_id >= 0) {
		sceKernelDeleteMbx(ctx->mbx_id);
		ctx->mbx_id = -1;
	}
	if (ctx->pkt_buf != NULL) {
		sceNetFreeInternal(ctx->pkt_buf);
		ctx->pkt_buf = NULL;
	}
	if (ctx->ext_copy != NULL) {
		sceNetFreeInternal(ctx->ext_copy);
		ctx->ext_copy = NULL;
	}
}

static void stunExecPortCheck(StunCtx *ctx, u16 port, s32 sock_fd, void *arg)
{
	SockAddrIn local_addr, srv_addr;
	u8 mapped_out[8];
	StunReqOpts req;
	StunRespData resp;
	FdSet rfds;
	TimeVal tv;
	s32 ret;

	sceKernelMemset(mapped_out, 0, sizeof(mapped_out));
	ret = stunResolveServer(ctx, ctx->host, &srv_addr);
	if (ret < 0)
		goto done;
	srv_addr.sin_port = bswap16(ctx->port);

	if (sock_fd == -1) {
		ret = sceNetInetSocket(2, 2, 0);
		if (ret < 0)
			goto done;
		ctx->sock_fd = ret;

		sceKernelMemset(&local_addr, 0, sizeof(local_addr));
		local_addr.sin_family = 2;
		local_addr.sin_port = bswap16(port);
		local_addr.sin_addr = 0;
		ret = sceNetInetBind(ctx->sock_fd, &local_addr, sizeof(local_addr));
		if (ret < 0)
			goto done;
	} else {
		for (;;) {
			sceKernelMemset(&rfds, 0, sizeof(rfds));
			rfds.bits[sock_fd >> 5] |= (1u << (sock_fd & 31));
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			ret = sceNetInetSelect(sock_fd + 1, &rfds, NULL, NULL, &tv);
			if (ret <= 0)
				break;
			ret = sceNetInetRecvfrom(sock_fd, ctx->pkt_buf, ctx->pkt_buf_size, 0, NULL, NULL);
			if (ret < 0)
				break;
		}
		if (ret < 0)
			goto done;
		ctx->sock_fd = sock_fd;
	}

	sceKernelMemset(&req, 0, sizeof(req));
	if (ctx->cfg.username != 0) {
		req.username = &ctx->cfg.username;
		req.username_len = 4;
		req.hmac_key = ctx->cfg.key;
		req.hmac_key_len = 20;
	}
	if (ctx->cfg.use_xor != 0)
		req.use_xor = 1;
	if (ctx->cfg.has_ext_data != 0) {
		req.has_ext = 1;
		req.ext_data = ctx->ext_copy;
		req.ext_data_len = ctx->cfg.ext_data_len;
	}

	sceKernelMemset(&resp, 0, sizeof(resp));
	if (ctx->cfg.username != 0) {
		resp.hmac_key = ctx->cfg.key;
		resp.hmac_key_len = 20;
	}
	if (ctx->cfg.has_ext_data != 0) {
		resp.ext_data = ctx->ext_copy;
		resp.ext_data_len = ctx->cfg.ext_data_len;
	}

	ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &srv_addr, &srv_addr, &req, &resp,
		(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
	if (ret >= 0) {
		u16 mport = bswap16(resp.mapped.port);
		sceKernelMemcpy(mapped_out, &resp.mapped.addr, 4);
		sceKernelMemcpy(mapped_out + 4, &mport, 2);
	}

done:
	if (ctx->sock_fd >= 0) {
		if (sock_fd == -1)
			sceNetInetClose(ctx->sock_fd);
		ctx->sock_fd = -1;
	}

	if (ctx->flags & 1) {
		ctx->cb(ctx->id, arg, 4, NULL);
	} else if (ret < 0) {
		ctx->cb(ctx->id, arg, ret, NULL);
	} else {
		ctx->cb(ctx->id, arg, 2, mapped_out);
	}
	sceNetFreeThreadinfo(sceKernelGetThreadId());
	ctx->flags = 0;
}

static void stunExecNatCheck(StunCtx *ctx, void *arg)
{
	u32 result[4];
	u8 mapped_ev[8];
	StunReqOpts req;
	StunRespData resp;
	SockAddrIn local_addr, alt_addr, srv_addr;
	s32 namelen;
	s32 first_d1 = 0, first_d2 = 0, retry_pass = 0;
	s32 ret;

	for (;;) {
		sceKernelMemset(result, 0, sizeof(result));
		if (!retry_pass) {
			ret = stunResolveServer(ctx, ctx->host, &srv_addr);
			if (ret < 0)
				break;
			srv_addr.sin_port = bswap16(ctx->port);
		}

		ret = sceNetInetSocket(2, 2, 0);
		if (ret < 0)
			break;
		ctx->sock_fd = ret;

		sceKernelMemset(&local_addr, 0, sizeof(local_addr));
		local_addr.sin_family = 2;
		local_addr.sin_port = 0;
		local_addr.sin_addr = 0;
		ret = sceNetInetBind(ctx->sock_fd, &local_addr, sizeof(local_addr));
		if (ret < 0)
			break;

		namelen = sizeof(local_addr);
		ret = sceNetInetGetsockname(ctx->sock_fd, &local_addr, &namelen);
		if (ret < 0)
			break;

		/* Step 1: Binding request to primary server */
		sceKernelMemset(&req, 0, sizeof(req));
		if (ctx->cfg.username != 0) {
			req.username = &ctx->cfg.username;
			req.username_len = 4;
			req.hmac_key = ctx->cfg.key;
			req.hmac_key_len = 20;
		}
		if (ctx->cfg.use_xor != 0)
			req.use_xor = 1;

		sceKernelMemset(&resp, 0, sizeof(resp));
		if (ctx->cfg.username != 0) {
			resp.hmac_key = ctx->cfg.key;
			resp.hmac_key_len = 20;
		}

		ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &srv_addr, &srv_addr, &req, &resp,
			(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
		if (ret < 0)
			break;

		if (!retry_pass) {
			u16 mport = bswap16(resp.mapped.port);
			sceKernelMemcpy(mapped_ev, &resp.mapped.addr, 4);
			sceKernelMemcpy(mapped_ev + 4, &mport, 2);
			ctx->cb(ctx->id, arg, 5, mapped_ev);
		}

		u16 port1 = resp.mapped.port;
		result[0] = resp.mapped.addr;
		u32 src_ip = resp.source.addr;
		u32 chg_ip = resp.changed.addr;
		u16 chg_port = resp.changed.port;
		result[2] = (local_addr.sin_port == port1) ? 1 : 0;

		/* Step 2: Binding request to primary IP, changed port */
		sceKernelMemset(&alt_addr, 0, sizeof(alt_addr));
		alt_addr.sin_family = 2;
		alt_addr.sin_port = chg_port;
		alt_addr.sin_addr = src_ip;

		sceKernelMemset(&req, 0, sizeof(req));
		if (ctx->cfg.username != 0) {
			req.username = &ctx->cfg.username;
			req.username_len = 4;
			req.hmac_key = ctx->cfg.key;
			req.hmac_key_len = 20;
		}
		if (ctx->cfg.use_xor != 0)
			req.use_xor = 1;

		sceKernelMemset(&resp, 0, sizeof(resp));
		if (ctx->cfg.username != 0) {
			resp.hmac_key = ctx->cfg.key;
			resp.hmac_key_len = 20;
		}

		ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &alt_addr, &alt_addr, &req, &resp,
			(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
		if (ret < 0)
			break;

		u16 port2 = resp.mapped.port;
		u32 diff1 = (u32)(u16)(bswap16(port2) - bswap16(port1));
		result[3] = diff1;
		if (port2 != port1) {
			result[1] = 2;
			break;
		}

		/* Step 3: Binding request to changed IP, changed port */
		alt_addr.sin_addr = chg_ip;
		sceKernelMemset(&req, 0, sizeof(req));
		if (ctx->cfg.username != 0) {
			req.username = &ctx->cfg.username;
			req.username_len = 4;
			req.hmac_key = ctx->cfg.key;
			req.hmac_key_len = 20;
		}
		if (ctx->cfg.use_xor != 0)
			req.use_xor = 1;

		sceKernelMemset(&resp, 0, sizeof(resp));
		if (ctx->cfg.username != 0) {
			resp.hmac_key = ctx->cfg.key;
			resp.hmac_key_len = 20;
		}

		ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &alt_addr, &alt_addr, &req, &resp,
			(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
		if (ret < 0)
			break;

		u16 port3 = resp.mapped.port;
		result[1] = (port3 == port2) ? 0 : 1;

		u32 diff2 = (u32)(u16)(bswap16(port3) - bswap16(port2));
		if (diff2 == result[3])
			break;

		if (!retry_pass) {
			if (diff2 == 1 || result[3] == 1) {
				result[3] = 1;
				break;
			}
			first_d1 = (s32)(u16)result[3];
			first_d2 = (s32)diff2;
			retry_pass = 1;
			continue;
		} else {
			if (diff2 == 1 || result[3] == 1) {
				result[3] = 1;
				break;
			}
			if (diff2 == 2 || (result[3] == 2 && diff2 == result[3])) {
				if (result[3] == 2 && first_d1 == 2 && (u32)first_d2 == diff2) {
					result[3] = 2;
					break;
				}
			}
			result[3] = 1;
			break;
		}
	}

	if (ctx->sock_fd >= 0) {
		sceNetInetClose(ctx->sock_fd);
		ctx->sock_fd = -1;
	}

	if (ctx->flags & 1) {
		ctx->cb(ctx->id, arg, 4, NULL);
	} else if (ret < 0) {
		ctx->cb(ctx->id, arg, ret, NULL);
	} else {
		ctx->cb(ctx->id, arg, 1, result);
	}
	sceNetFreeThreadinfo(sceKernelGetThreadId());
	ctx->flags = 0;
}

static void stunExecExtNatCheck(StunCtx *ctx, void *arg)
{
	u32 out_res[2];
	StunReqOpts req;
	StunRespData resp;
	SockAddrIn local_addr, src_addr, expect_addr, srv_addr;
	s32 namelen;
	u32 chg_ip;
	s32 ret;

	sceKernelMemset(out_res, 0, sizeof(out_res));
	ret = stunResolveServer(ctx, ctx->host, &srv_addr);
	if (ret < 0)
		goto done;
	srv_addr.sin_port = bswap16(ctx->port);

	ret = sceNetInetSocket(2, 2, 0);
	if (ret < 0)
		goto done;
	ctx->sock_fd = ret;

	sceKernelMemset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = 2;
	local_addr.sin_port = 0;
	local_addr.sin_addr = 0;
	ret = sceNetInetBind(ctx->sock_fd, &local_addr, sizeof(local_addr));
	if (ret < 0)
		goto done;

	namelen = sizeof(local_addr);
	ret = sceNetInetGetsockname(ctx->sock_fd, &local_addr, &namelen);
	if (ret < 0)
		goto done;

	/* Request 1: basic binding */
	sceKernelMemset(&req, 0, sizeof(req));
	if (ctx->cfg.username != 0) {
		req.username = &ctx->cfg.username;
		req.username_len = 4;
		req.hmac_key = ctx->cfg.key;
		req.hmac_key_len = 20;
	}
	if (ctx->cfg.use_xor != 0)
		req.use_xor = 1;

	sceKernelMemset(&resp, 0, sizeof(resp));
	if (ctx->cfg.username != 0) {
		resp.hmac_key = ctx->cfg.key;
		resp.hmac_key_len = 20;
	}

	ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &srv_addr, &srv_addr, &req, &resp,
		(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
	if (ret < 0)
		goto done;

	chg_ip = resp.changed.addr;
	sceKernelMemset(&src_addr, 0, sizeof(src_addr));
	src_addr.sin_family = 2;
	src_addr.sin_port = resp.source.port;
	src_addr.sin_addr = resp.source.addr;

	sceKernelMemset(&expect_addr, 0, sizeof(expect_addr));
	expect_addr.sin_family = 2;
	expect_addr.sin_port = resp.changed.port;
	expect_addr.sin_addr = resp.source.addr;

	/* Request 2: CHANGE_PORT (2) */
	sceKernelMemset(&req, 0, sizeof(req));
	req.change_req = 2;
	if (ctx->cfg.username != 0) {
		req.username = &ctx->cfg.username;
		req.username_len = 4;
		req.hmac_key = ctx->cfg.key;
		req.hmac_key_len = 20;
	}
	if (ctx->cfg.use_xor != 0)
		req.use_xor = 1;

	ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &src_addr, &expect_addr, &req, &resp,
		(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
	if (ret == (s32)0x80411703) {
		out_res[0] = 2;
		ret = stunTestChangedAddrReachable(ctx->sock_fd, &resp.mapped, out_res);
		goto done;
	}
	if (ret < 0)
		goto done;

	/* Request 3: CHANGE_IP | CHANGE_PORT (6) */
	expect_addr.sin_addr = chg_ip;
	sceKernelMemset(&req, 0, sizeof(req));
	req.change_req = 6;
	if (ctx->cfg.username != 0) {
		req.username = &ctx->cfg.username;
		req.username_len = 4;
		req.hmac_key = ctx->cfg.key;
		req.hmac_key_len = 20;
	}
	if (ctx->cfg.use_xor != 0)
		req.use_xor = 1;

	ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &src_addr, &expect_addr, &req, &resp,
		(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
	if (ret == (s32)0x80411703) {
		out_res[0] = 1;
	} else if (ret < 0) {
		goto done;
	} else {
		out_res[0] = 0;
	}

	ret = stunTestChangedAddrReachable(ctx->sock_fd, &resp.mapped, out_res);

done:
	if (ctx->sock_fd >= 0) {
		sceNetInetClose(ctx->sock_fd);
		ctx->sock_fd = -1;
	}

	if (ctx->flags & 1) {
		ctx->cb(ctx->id, arg, 4, NULL);
	} else if (ret < 0) {
		ctx->cb(ctx->id, arg, ret, NULL);
	} else {
		ctx->cb(ctx->id, arg, 6, out_res);
	}
	sceNetFreeThreadinfo(sceKernelGetThreadId());
	ctx->flags = 0;
}

static s32 stunResolveServer(StunCtx *ctx, const char *host, SockAddrIn *out_addr)
{
	void *res_buf = NULL;
	s32 ret = 0;

	sceKernelMemset(out_addr, 0, sizeof(SockAddrIn));
	out_addr->sin_family = 2;

	u32 ip = sceNetInetInetAddr(host);
	if (ip != 0xffffffffu) {
		out_addr->sin_addr = ip;
		goto cleanup;
	}

	res_buf = sceNetMallocInternal(0x300);
	if (res_buf == NULL) {
		ret = (s32)0x8041170d;
		goto cleanup;
	}

	ret = sceNetResolverCreate(&ctx->resolver_id, res_buf, 0x300);
	if (ret < 0)
		goto cleanup;

	ret = sceNetResolverStartNtoA(ctx->resolver_id, host, &out_addr->sin_addr, 1000000, 5);
	if (ret >= 0)
		ret = 0;

cleanup:
	if (ctx->resolver_id > 0)
		sceNetResolverDelete(ctx->resolver_id);
	ctx->resolver_id = 0;
	if (res_buf != NULL)
		sceNetFreeInternal(res_buf);
	return ret;
}

static s32 stunSendRecvRequest(s32 send_fd, s32 recv_fd, const SockAddrIn *dst_addr, const SockAddrIn *expect_addr,
                    const StunReqOpts *req, StunRespData *resp, u8 *pkt_buf, u32 pkt_buf_size)
{
	u32 tid[4];
	SockAddrIn from;
	TimeVal tv;
	FdSet rfds;
	s32 fromlen;
	u32 rto = 100000;
	s32 attempt = 0;
	s32 i, ret;

	for (i = 0; i < 4; i++)
		tid[i] = sceNetRand();

	for (;;) {
		if (attempt > 0) {
			if (attempt >= 9)
				return (s32)0x80411703;
			if (rto <= 1600000u - 1u)
				rto <<= 1;
		}

		ret = stunBuildBindingRequest(pkt_buf, pkt_buf_size, (const u8 *)tid, req);
		if (ret < 0)
			return ret;

		ret = sceNetInetSendto(send_fd, pkt_buf, (u32)ret, 0, dst_addr, sizeof(SockAddrIn));
		if (ret < 0)
			return ret;

		u32 deadline = sceKernelGetSystemTimeLow() + rto;
		for (;;) {
			u32 now = sceKernelGetSystemTimeLow();
			if (now > deadline)
				break;
			u32 rem = deadline - now;
			tv.tv_sec = (s32)(rem / 1000000u);
			tv.tv_usec = (s32)(rem % 1000000u);

			sceKernelMemset(&rfds, 0, sizeof(rfds));
			rfds.bits[recv_fd >> 5] |= (1u << (recv_fd & 31));
			ret = sceNetInetSelect(recv_fd + 1, &rfds, NULL, NULL, &tv);
			if (ret < 0)
				return ret;
			if (ret == 0)
				break;

			fromlen = sizeof(from);
			ret = sceNetInetRecvfrom(recv_fd, pkt_buf, pkt_buf_size, 0, &from, &fromlen);
			if (ret < 0)
				continue;
			if (from.sin_family != expect_addr->sin_family)
				continue;
			u32 exp_ip;
			sceKernelMemcpy(&exp_ip, &expect_addr->sin_addr, 4);
			if (from.sin_addr != exp_ip)
				continue;

			ret = stunParseBindingResponse(pkt_buf, pkt_buf_size, (const u8 *)tid, resp);
			if (ret != (s32)0x80411704)
				return ret;
		}
		attempt++;
	}
}

static s32 stunTestChangedAddrReachable(s32 sock_fd, const StunAddrAttr *dst, u32 *out_res)
{
	SockAddrIn addr;
	TimeVal tv;
	u8 probe[0x40];
	FdSet rfds;
	s32 i, ret;

	sceKernelMemset(&addr, 0, sizeof(addr));
	addr.sin_family = 2;
	addr.sin_port = dst->port;
	addr.sin_addr = dst->addr;

	for (i = 0; i < 5; i++) {
		ret = sceNetInetSendto(sock_fd, probe, sizeof(probe), 0, &addr, sizeof(addr));
		if (ret < 0)
			return ret;

		sceKernelMemset(&rfds, 0, sizeof(rfds));
		rfds.bits[sock_fd >> 5] |= (1u << (sock_fd & 31));
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		ret = sceNetInetSelect(sock_fd + 1, &rfds, NULL, NULL, &tv);
		if (ret < 0)
			return ret;
		if (ret > 0)
			break;
	}

	out_res[1] = (i != 5) ? 1u : 0u;
	return ret;
}

static s32 stunBuildBindingRequest(u8 *buf, u32 bufsize, const u8 *tid, const StunReqOpts *req)
{
	u8 *p = buf + 0x14;
	u32 rem = bufsize - 0x14;
	s32 ret;

	if (req->has_resp_addr == 1) {
		ret = stunWriteAddrAttr(p, rem, 2, req->resp_port, req->resp_addr);
		if (ret < 0)
			return ret;
		p += ret;
		rem -= (u32)ret;
	}

	if (req->change_req != 0) {
		ret = stunWriteChangeReqAttr(p, rem, req->change_req);
		if (ret < 0)
			return ret;
		p += ret;
		rem -= (u32)ret;
	}

	if (req->use_xor == 1) {
		ret = stunWriteBytesAttr(p, rem, 0x0021, NULL, 0);
		if (ret < 0)
			return ret;
		p += ret;
		rem -= (u32)ret;
	}

	if (req->username != NULL) {
		ret = stunWriteBytesAttr(p, rem, 0x0006, req->username, req->username_len);
		if (ret < 0)
			return ret;
		p += ret;
		rem -= (u32)ret;
	}

	if (req->has_ext == 1) {
		ret = stunWriteBytesAttr(p, rem, 0x8099, req->ext_data, req->ext_data_len);
		if (ret < 0)
			return ret;
		p += ret;
	}

	if (req->hmac_key != NULL) {
		ret = stunWriteHeader(buf, (u32)(p - buf) + 0x18, tid, 1);
		if (ret < 0)
			return ret;
		ret = stunWriteMessageIntegrity(buf, bufsize, p, (const u8 *)req->hmac_key, req->hmac_key_len);
		if (ret < 0)
			return ret;
		return (s32)((p + ret) - buf);
	}

	ret = stunWriteHeader(buf, (u32)(p - buf), tid, 1);
	if (ret < 0)
		return ret;
	return (s32)(p - buf);
}

static s32 stunParseBindingResponse(u8 *buf, u32 bufsize, const u8 *tid, StunRespData *resp)
{
	StunAttrHdr hdr;
	s32 skip;
	s32 got_mapped = 0, got_source = 0, got_changed = 0;
	s32 rem, ret;
	u8 *p;

	rem = stunVerifyHeader(buf, bufsize, tid, 0x0101);
	if (rem < 0)
		return rem;

	p = buf + 0x14;
	while ((u32)rem >= 4u) {
		sceKernelMemcpy(&hdr, p, 4);
		u16 attr_len = bswap16(hdr.length);
		if ((u32)rem < (u32)attr_len + 4u)
			break;
		hdr.length = attr_len;
		u16 attr_type = bswap16(hdr.type);

		if (attr_type == 0x0001 || attr_type == 0x8020) {
			ret = stunReadAddrAttr(p, (u32)rem, &skip, &resp->mapped);
			if (ret < 0)
				return ret;
			if (skip != 1) {
				got_mapped = 1;
				if (attr_type == 0x8020) {
					u16 xport = (u16)((u32)tid[0] | ((u32)tid[1] << 8));
					u32 xaddr = (u32)tid[0] | ((u32)tid[1] << 8) |
					            ((u32)tid[2] << 16) | ((u32)tid[3] << 24);
					resp->mapped.port ^= xport;
					resp->mapped.addr ^= xaddr;
				}
			}
		} else if (attr_type == 0x0004) {
			ret = stunReadAddrAttr(p, (u32)rem, &skip, &resp->source);
			if (ret < 0)
				return ret;
			if (skip != 1)
				got_source = 1;
		} else if (attr_type == 0x0005) {
			ret = stunReadAddrAttr(p, (u32)rem, &skip, &resp->changed);
			if (ret < 0)
				return ret;
			if (skip != 1)
				got_changed = 1;
		} else if (attr_type == 0x0008) {
			ret = stunVerifyMessageIntegrity(buf, bufsize, p, (const u8 *)resp->hmac_key, resp->hmac_key_len);
			if (ret < 0)
				return ret;
		} else if (attr_type == 0x8099) {
			ret = stunVerifyExtAttr(p, (u32)rem, resp->check_ext, resp->ext_data, resp->ext_data_len);
			if (ret < 0)
				return ret;
		}

		hdr.length = bswap16(hdr.length);
		sceKernelMemcpy(p, &hdr, 4);
		p += attr_len + 4;
		rem -= (s32)attr_len + 4;
	}

	if (!got_mapped || !got_source || !got_changed)
		return (s32)0x80411705;
	return 0;
}

static s32 stunWriteHeader(u8 *buf, u32 total_len, const u8 *tid, u16 msg_type)
{
	StunHdr hdr;

	sceKernelMemset(&hdr, 0, sizeof(hdr));
	hdr.type = bswap16(msg_type);
	hdr.length = bswap16((u16)(total_len - 0x14));
	sceKernelMemcpy(hdr.tid, tid, 16);
	sceKernelMemcpy(buf, &hdr, sizeof(hdr));
	return (s32)total_len;
}

static s32 stunVerifyHeader(u8 *buf, u32 bufsize, const u8 *tid, u16 expect_type)
{
	StunHdr hdr;

	if (bufsize < 0x14)
		return (s32)0x80411704;

	sceKernelMemcpy(&hdr, buf, sizeof(hdr));
	if (sceNetMemcmp(tid, hdr.tid, 16) != 0)
		return (s32)0x80411704;

	if (hdr.type != expect_type)
		return (s32)0x80411704;

	u16 body_len = bswap16(hdr.length);
	if (bufsize < (u32)body_len + 0x14)
		return (s32)0x80411704;

	hdr.length = bswap16(body_len);
	sceKernelMemcpy(buf, &hdr, sizeof(hdr));
	return (s32)body_len;
}

static s32 stunWriteAddrAttr(u8 *buf, u32 bufsize, u16 attr_type, u16 port, u32 addr)
{
	StunAttrHdr hdr;
	StunAddrAttr val;

	if (bufsize < 12)
		return (s32)0x80411706;

	sceKernelMemset(&hdr, 0, sizeof(hdr));
	hdr.type = bswap16(attr_type);
	hdr.length = bswap16(8);
	sceKernelMemcpy(buf, &hdr, sizeof(hdr));

	sceKernelMemset(&val, 0, sizeof(val));
	val.family = 1;
	val.port = port;
	val.addr = addr;
	sceKernelMemcpy(buf + 4, &val, sizeof(val));
	return 12;
}

static s32 stunReadAddrAttr(const u8 *buf, u32 bufsize, s32 *out_skip, StunAddrAttr *out_addr)
{
	StunAttrHdr hdr;
	StunAddrAttr val;

	if (bufsize < 12)
		return (s32)0x80411706;

	sceKernelMemcpy(&hdr, buf, 4);
	hdr.length = bswap16(hdr.length);
	if (hdr.length != 8) {
		*out_skip = 1;
		return (s32)hdr.length + 4;
	}

	sceKernelMemcpy(&val, buf + 4, 8);
	if (val.family != 1) {
		*out_skip = 1;
		return (s32)hdr.length + 4;
	}

	sceKernelMemset(out_addr, 0, sizeof(StunAddrAttr));
	out_addr->family = val.family;
	out_addr->port = val.port;
	out_addr->addr = val.addr;
	*out_skip = 0;
	return 12;
}

static s32 stunWriteChangeReqAttr(u8 *buf, u32 bufsize, u32 flags)
{
	StunAttrHdr hdr;
	u32 val;

	if (bufsize < 8)
		return (s32)0x80411706;

	sceKernelMemset(&hdr, 0, sizeof(hdr));
	hdr.type = bswap16(3);
	hdr.length = bswap16(4);
	sceKernelMemcpy(buf, &hdr, 4);

	val = bswap32(flags & 6u);
	sceKernelMemcpy(buf + 4, &val, 4);
	return 8;
}

static s32 stunWriteBytesAttr(u8 *buf, u32 bufsize, u16 attr_type, const void *data, u32 data_len)
{
	StunAttrHdr hdr;
	u32 total = data_len + 4;

	if (bufsize < total)
		return (s32)0x80411706;

	sceKernelMemset(&hdr, 0, sizeof(hdr));
	hdr.type = bswap16(attr_type);
	hdr.length = bswap16((u16)data_len);
	sceKernelMemcpy(buf, &hdr, 4);

	if (data_len != 0)
		sceKernelMemcpy(buf + 4, data, data_len);
	return (s32)total;
}

static s32 stunWriteMessageIntegrity(u8 *pkt_start, u32 bufsize, u8 *attr_pos, const u8 *key, u32 key_len)
{
	StunAttrHdr hdr;
	u8 digest[20];
	s32 msg_len = (s32)(attr_pos - pkt_start);
	u32 padded_len = (msg_len <= 0) ? 0x40u : (((u32)(msg_len - 1) | 0x3fu) + 1u);

	if (bufsize < padded_len || bufsize < (u32)msg_len + 0x18u)
		return (s32)0x80411706;

	sceKernelMemset(attr_pos, 0, padded_len - (u32)msg_len);
	hmacSha1Compute(pkt_start, (s32)padded_len, key, (s32)key_len, digest);

	sceKernelMemset(&hdr, 0, sizeof(hdr));
	hdr.type = bswap16(8);
	hdr.length = bswap16(20);
	sceKernelMemcpy(attr_pos, &hdr, 4);
	sceKernelMemcpy(attr_pos + 4, digest, 20);
	return 24;
}

static s32 stunVerifyMessageIntegrity(u8 *pkt_start, u32 bufsize, u8 *attr_pos, const u8 *key, u32 key_len)
{
	u8 expected[20];
	u8 computed[20];
	s32 msg_len = (s32)(attr_pos - pkt_start);
	u32 padded_len = (msg_len <= 0) ? 0x40u : (((u32)(msg_len - 1) | 0x3fu) + 1u);

	if (bufsize < padded_len)
		return (s32)0x80411706;

	sceKernelMemcpy(expected, attr_pos + 4, 20);
	sceKernelMemset(attr_pos, 0, padded_len - (u32)msg_len);
	hmacSha1Compute(pkt_start, (s32)padded_len, key, (s32)key_len, computed);

	if (sceNetMemcmp(expected, computed, 20) != 0)
		return (s32)0x80411707;
	return 0;
}

static s32 stunVerifyExtAttr(const u8 *attr_pos, u32 rem, s32 check_ext, const void *ext_data, u32 ext_len)
{
	StunAttrHdr hdr;

	if (!check_ext)
		return 0;

	sceKernelMemcpy(&hdr, attr_pos, 4);
	hdr.length = bswap16(hdr.length);
	if (hdr.length != ext_len)
		return (s32)0x8041170b;

	if (sceNetMemcmp(attr_pos + 4, ext_data, ext_len) != 0)
		return (s32)0x8041170c;
	return 0;
}

/* =========================================================================
 * Streaming SHA-1 (0x85f0..0x8a24)
 * ========================================================================= */

static s32 sha1Update(Sha1Ctx *ctx, const u8 *data, u32 len)
{
	u32 block[16];
	u32 rem;
	u32 i;

	if (ctx == NULL || data == NULL)
		return (s32)0x80000103;
	if (ctx->finalized != 0)
		return (s32)0x80000021;

	ctx->total_bytes += len;
	rem = (u32)ctx->buf_len + len;

	while (rem >= 64) {
		u8 *bb = (u8 *)block;
		u32 cur = ctx->buf_len;
		if (cur != 0) {
			u32 fill = 64 - cur;
			for (i = 0; i < cur; i++)
				bb[i] = ctx->buf[i];
			for (i = 0; i < fill; i++)
				bb[cur + i] = data[i];
			data += fill;
			ctx->buf_len = 0;
		} else {
			for (i = 0; i < 64; i++)
				bb[i] = data[i];
			data += 64;
		}
		for (i = 0; i < 16; i++)
			block[i] = bswap32(block[i]);
		sha1TransformBlock(block, ctx->state);
		rem -= 64;
	}

	if (rem != 0) {
		u32 cur = ctx->buf_len;
		for (i = 0; i < rem - cur; i++)
			ctx->buf[cur + i] = data[i];
	}
	ctx->buf_len = (u16)rem;
	return 0;
}

static s32 sha1Final(Sha1Ctx *ctx, u8 *out_digest)
{
	u32 block[16];
	u32 i;

	if (ctx == NULL || out_digest == NULL)
		return (s32)0x80000103;

	if (ctx->finalized == 0) {
		u32 cur = ctx->buf_len;
		u8 *bb = (u8 *)block;
		for (i = 0; i < 16; i++)
			block[i] = 0;
		for (i = 0; i < cur; i++)
			bb[i] = ctx->buf[i];
		bb[cur] = 0x80;

		if (cur >= 56) {
			for (i = 0; i < 16; i++)
				block[i] = bswap32(block[i]);
			sha1TransformBlock(block, ctx->state);
			for (i = 0; i < 16; i++)
				block[i] = 0;
		}

		u64 bitlen = ctx->total_bytes << 3;
		for (i = 0; i < 8; i++)
			bb[56 + i] = (u8)(bitlen >> (56 - i * 8));

		for (i = 0; i < 16; i++)
			block[i] = bswap32(block[i]);
		sha1TransformBlock(block, ctx->state);

		for (i = 0; i < 5; i++)
			ctx->state[i] = bswap32(ctx->state[i]);
		ctx->finalized = 1;
		for (i = 0; i < 64; i++)
			ctx->buf[i] = 0;
	}

	const u8 *st = (const u8 *)ctx->state;
	for (i = 0; i < 20; i++)
		out_digest[i] = st[i];
	return 0;
}

static s32 sha1Init(Sha1Ctx *ctx)
{
	if (ctx == NULL)
		return (s32)0x80000103;
	ctx->state[0] = 0x67452301u;
	ctx->state[1] = 0xefcdab89u;
	ctx->state[2] = 0x98badcfeu;
	ctx->state[3] = 0x10325476u;
	ctx->state[4] = 0xc3d2e1f0u;
	ctx->buf_len = 0;
	ctx->finalized = 0;
	ctx->total_bytes = 0;
	return 0;
}
