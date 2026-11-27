#ifndef PSPNET_INTERNAL_H
#define PSPNET_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

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

/* Error codes */
#define SCE_NET_ERROR_ENOMEM         ((int)0x80410001)
#define SCE_NET_ERROR_ENOENT         ((int)0x80410104)
#define SCE_NET_ERROR_EBUSY          ((int)0x80410105)
#define SCE_NET_ERROR_EINVAL         ((int)0x80410103)
#define SCE_NET_ERROR_SCAN_NONE      ((int)0x80410106)
#define SCE_NET_ERROR_JOIN_FAIL      ((int)0x80410107)
#define SCE_NET_ERROR_NO_IF          ((int)0x80410180)
#define SCE_NET_ERROR_IF_BUSY        ((int)0x80410181)
#define SCE_NET_ERROR_IOCTL_ERR      ((int)0x80410182)
#define SCE_NET_ERROR_IOCTL_CANCELED ((int)0x80410183)
#define SCE_NET_ERROR_IOCTL_ENOENT   ((int)0x80410184)
#define SCE_NET_ERROR_NO_BSS         ((int)0x80410185)
#define SCE_NET_ERROR_STACK_SMALL    ((int)0x80410005)
#define SCE_NET_ERROR_ADHOC_CANCEL   ((int)0x80410D11)

/* Kernel_Library imports */
extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int intr);
extern void *sceKernelMemset(void *dst, int ch, SceSize len);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize len);
extern int sceKernelLockLwMutex(void *workarea, int count, u32 *timeout);
extern int sceKernelUnlockLwMutex(void *workarea, int count);
extern int sceKernelCheckThreadStack(void);
extern SceUID sceKernelGetThreadId(void);

/* SysMemUserForUser imports */
extern int sceKernelQueryMemoryInfo(void *addr, SceUID *partitionId, int *attr);

/* ThreadManForUser imports */
extern int sceKernelDelayThread(u32 usec);
extern SceUID sceKernelCreateSema(const char *name, u32 attr, int initVal, int maxVal, void *opt);
extern int sceKernelDeleteSema(SceUID semaid);
extern int sceKernelSignalSema(SceUID semaid, int count);
extern int sceKernelWaitSema(SceUID semaid, int count, u32 *timeout);
extern int sceKernelPollSema(SceUID semaid, int count);
extern SceUID sceKernelCreateEventFlag(const char *name, u32 attr, u32 bits, void *opt);
extern int sceKernelDeleteEventFlag(SceUID evid);
extern int sceKernelSetEventFlag(SceUID evid, u32 bits);
extern int sceKernelWaitEventFlag(SceUID evid, u32 bits, u32 wait, u32 *outBits, u32 *timeout);
extern SceUID sceKernelCreateThread(const char *name, void *entry, int initPriority, int stackSize, u32 attr, void *opt);
extern int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern int sceKernelTerminateThread(SceUID thid);
extern int sceKernelDeleteThread(SceUID thid);
extern SceUID sceKernelSetAlarm(u32 clock, void *handler, void *common);
extern int sceKernelCancelAlarm(SceUID alarmid);
extern int sceKernelCreateLwMutex(void *workarea, const char *name, u32 attr, int initCount, void *opt);
extern int sceKernelDeleteLwMutex(void *workarea);
extern int sceKernelGetSystemTime(void *sysclock);
extern u32 sceKernelGetSystemTimeLow(void);
extern s64 sceKernelGetSystemTimeWide(void);
extern int sceKernelSysClock2USec(void *sysclock, u32 *low, u32 *high);

/* UtilsForUser imports */
extern int sceKernelUtilsSha1BlockInit(void *ctx);
extern int sceKernelUtilsSha1BlockUpdate(void *ctx, const void *data, u32 len);
extern int sceKernelUtilsSha1BlockResult(void *ctx, u8 *digest);

/* sceWlanDrv_lib imports */
extern int sceWlanDrv_lib_8D5F551B(int flags);

/* sceNetIfhandle_lib imports */
extern int sceNetIfhandle_lib_0296C7D6(void *ifp, int cmd, void *arg);
extern int sceNetIfhandle_lib_0C391E9F(SceUID partId, int poolSize);
extern void sceNetIfhandle_lib_0FB8AE0D(void);
extern void *sceNetIfhandle_lib_15CFE3C0(u32 size);
extern void sceNetIfhandle_lib_29ED84C5(void);
extern void sceNetIfhandle_lib_30F69334(SceUID evf);
extern int sceNetIfhandle_lib_4C2886CB(void *stat);
extern int sceNetIfhandle_lib_4FB43BCE(void **ifp, SceUID *sema);
extern void sceNetIfhandle_lib_76BAD213(void *ptr);
extern int sceNetIfhandle_lib_8FCB05A1(void *ifp);
extern int sceNetIfhandle_lib_9173FD47(void **ifp, SceUID *sema);
extern int sceNetIfhandle_lib_B1F5BB87(void *ifp, void *mbuf);
extern int sceNetIfhandle_lib_B40A882F(void **ifp, SceUID *sema);
extern int sceNetIfhandle_lib_B8188F96(void **ifp, SceUID *sema);
extern int sceNetIfhandle_lib_B9096E48(void);
extern void sceNetIfhandle_lib_BFF3CEA5(void *mbuf, int len);
extern void *sceNetIfhandle_lib_E440A7D8(void);
extern int sceNetIfhandle_lib_EAD3A759(void *ifp);
extern void sceNetIfhandle_lib_F56FAC82(void *mbuf);

/* LwMutex workarea (32 bytes on PSP) */
typedef struct SceLwMutexWorkarea {
    u32 data[8];
} SceLwMutexWorkarea;

/* Thread opt param */
typedef struct SceKernelThreadOptParam {
    SceSize size;
    SceUID stackMpid;
} SceKernelThreadOptParam;

/* Callout structure (36 bytes) */
typedef struct NetCallout {
    struct NetCallout *ready_next;
    struct NetCallout **ready_prev;
    struct NetCallout *pend_next;
    struct NetCallout **pend_prev;
    void (*func)(void *);
    void *arg;
    u32 timeout_us;
    u32 flags;
    SceUID alarm_id;
} NetCallout;

/* Thread info structure (92 bytes) */
typedef struct NetThreadInfo {
    struct NetThreadInfo *list_next;   /* 0x00 */
    struct NetThreadInfo **list_prev;  /* 0x04 */
    SceUID thid;                       /* 0x08 */
    s16 refcount;                      /* 0x0C */
    u16 pad;                           /* 0x0E */
    int err;                           /* 0x10 */
    u32 reserved_status;               /* 0x14 */
    struct NetThreadInfo *slp_next;    /* 0x18 */
    struct NetThreadInfo **slp_prev;   /* 0x1C */
    u32 wchan;                         /* 0x20 */
    NetCallout callout;                /* 0x24..0x47 */
    u32 slp_flags;                     /* 0x48 */
    SceUID sema_id;                    /* 0x4C */
    u32 reserved_tail[3];              /* 0x50..0x5B */
} NetThreadInfo;

/* Timeval structure (8 bytes) */
typedef struct NetTimeval {
    u32 tv_sec;
    u32 tv_usec;
} NetTimeval;

/* uio / iovec structures for BPF */
typedef struct NetIovec {
    void *iov_base;
    u32 iov_len;
} NetIovec;

typedef struct NetUio {
    NetIovec *uio_iov;
    int uio_iovcnt;
    u32 uio_offset;
    u32 uio_resid;
    int uio_rw;
} NetUio;

/* BSD mbuf layout */
struct NetIfnet;

typedef struct NetMbufExt {
    u8 *ext_buf;                       /* 0x30 */
    void *ext_free;                    /* 0x34 */
    void *ext_arg;                     /* 0x38 */
    u32 ext_size;                      /* 0x3C */
    u32 ext_type;                      /* 0x40 */
    struct NetMbuf *ext_nextref;       /* 0x44 */
    struct NetMbuf *ext_prevref;       /* 0x48 */
} NetMbufExt;

typedef struct NetMbuf {
    struct NetMbuf *m_next;            /* 0x00 */
    struct NetMbuf *m_nextpkt;         /* 0x04 */
    u8 *m_data;                        /* 0x08 */
    u32 m_len;                         /* 0x0C */
    u16 m_type;                        /* 0x10 */
    u16 m_flags;                       /* 0x12 */
    struct NetIfnet *m_pkthdr_rcvif;   /* 0x14 */
    u32 m_pkthdr_len;                  /* 0x18 */
    u32 m_pkthdr_csum;                 /* 0x1C */
    u32 m_pkthdr_aux[4];               /* 0x20..0x2F */
    union {
        NetMbufExt m_ext;              /* 0x30..0x4B */
        u8 m_pktdat[208];              /* 0x30..0xFF */
    };
} NetMbuf;

/* BSD sockaddr / sockaddr_dl */
typedef struct NetSockaddr {
    u8 sa_len;
    u8 sa_family;
    u8 sa_data[14];
} NetSockaddr;

typedef struct NetSockaddrDl {
    u8 sdl_len;                        /* 0x00 */
    u8 sdl_family;                     /* 0x01 */
    u16 sdl_index;                     /* 0x02 */
    u8 sdl_type;                       /* 0x04 */
    u8 sdl_nlen;                       /* 0x05 */
    u8 sdl_alen;                       /* 0x06 */
    u8 sdl_slen;                       /* 0x07 */
    u8 sdl_data[12];                   /* 0x08..0x13 */
} NetSockaddrDl;

/* BSD ifaddr (56 bytes) */
typedef struct NetIfaddr {
    NetSockaddr *ifa_addr;             /* 0x00 */
    NetSockaddr *ifa_dstaddr;          /* 0x04 */
    NetSockaddr *ifa_netmask;          /* 0x08 */
    struct NetIfnet *ifa_ifp;          /* 0x0C */
    struct NetIfaddr *ifa_next;        /* 0x10 */
    struct NetIfaddr **ifa_prev;       /* 0x14 */
    u32 ifa_spare[4];                  /* 0x18..0x27 */
    void (*ifa_rtrequest)(int, int, int); /* 0x28 */
    u32 ifa_flags;                     /* 0x2C */
    s32 ifa_refcnt;                    /* 0x30 */
    s32 ifa_metric;                    /* 0x34 */
} NetIfaddr;

/* BSD if_data (120 bytes) */
typedef struct NetIfData {
    u8 ifi_type;                       /* 0x00 (ifp + 56) */
    u8 ifi_addrlen;                    /* 0x01 (ifp + 57) */
    u8 ifi_hdrlen;                     /* 0x02 (ifp + 58) */
    u8 ifi_link_state;                 /* 0x03 (ifp + 59) */
    u32 ifi_recvquota;                 /* 0x04 (ifp + 60) */
    u64 ifi_mtu;                       /* 0x08 (ifp + 64) */
    u64 ifi_metric;                    /* 0x10 (ifp + 72) */
    u64 ifi_baudrate;                  /* 0x18 (ifp + 80) */
    u64 ifi_ipackets;                  /* 0x20 (ifp + 88) */
    u64 ifi_ierrors;                   /* 0x28 (ifp + 96) */
    u64 ifi_opackets;                  /* 0x30 (ifp + 104) */
    u64 ifi_oerrors;                   /* 0x38 (ifp + 112) */
    u64 ifi_collisions;                /* 0x40 (ifp + 120) */
    u64 ifi_ibytes;                    /* 0x48 (ifp + 128) */
    u64 ifi_obytes;                    /* 0x50 (ifp + 136) */
    u64 ifi_imcasts;                   /* 0x58 (ifp + 144) */
    u64 ifi_omcasts;                   /* 0x60 (ifp + 152) */
    u64 ifi_iqdrops;                   /* 0x68 (ifp + 160) */
    u64 ifi_noproto;                   /* 0x70 (ifp + 168) */
} NetIfData;

/* BSD ifqueue (20 bytes) */
typedef struct NetIfQueue {
    NetMbuf *ifq_head;                 /* 0x00 */
    NetMbuf *ifq_tail;                 /* 0x04 */
    s32 ifq_len;                       /* 0x08 */
    s32 ifq_maxlen;                    /* 0x0C */
    u32 ifq_drops;                     /* 0x10 */
} NetIfQueue;

/* Interface event queue node (16 bytes) */
typedef struct NetIfEvent {
    struct NetIfEvent *ev_next;        /* 0x00 */
    struct NetIfEvent **ev_prev;       /* 0x04 */
    u32 ev_code;                       /* 0x08 */
    u32 ev_value;                      /* 0x0C */
} NetIfEvent;

/* BSD ifnet + PSP driver extensions (288 bytes) */
typedef struct NetIfnet {
    void *if_softc;                    /* 0x00 */
    struct NetIfnet *if_next;          /* 0x04 */
    struct NetIfnet **if_prev;         /* 0x08 */
    NetIfaddr *if_addrhead;            /* 0x0C */
    NetIfaddr **if_addrtail;           /* 0x10 */
    char if_xname[16];                 /* 0x14 */
    int if_pcount;                     /* 0x24 */
    void *if_bpf;                      /* 0x28 */
    u16 if_index;                      /* 0x2C */
    u16 if_timer;                      /* 0x2E */
    u16 if_flags;                      /* 0x30 */
    u16 if_pad;                        /* 0x32 */
    u32 if_capabilities;               /* 0x34 */
    NetIfData if_data;                 /* 0x38..0xAF */
    int (*if_output)(struct NetIfnet *, NetMbuf *, void *, void *); /* 0xB0 */
    void (*if_input)(struct NetIfnet *, NetMbuf *);                 /* 0xB4 */
    void (*if_start)(struct NetIfnet *);                            /* 0xB8 */
    int (*if_ioctl)(struct NetIfnet *, u32, void *);                /* 0xBC */
    int (*if_init)(struct NetIfnet *);                              /* 0xC0 */
    void (*if_watchdog)(struct NetIfnet *);                         /* 0xC4 */
    void (*if_drain)(struct NetIfnet *);                            /* 0xC8 */
    NetIfQueue if_snd;                 /* 0xCC..0xDF */
    NetSockaddrDl *if_sadl;            /* 0xE0 */
    const u8 *if_broadcastaddr;        /* 0xE4 */
    void *if_bridge;                   /* 0xE8 */
    u8 if_hwaddr[8];                   /* 0xEC */
    int (*if_drv_up)(void *);          /* 0xF4 */
    int (*if_drv_down)(void *);        /* 0xF8 */
    int (*if_drv_ioctl)(void *, int, void *); /* 0xFC */
    SceUID if_ioctl_done_sema;         /* 0x100 */
    SceUID if_ioctl_lock_sema;         /* 0x104 */
    s32 if_ioctl_pending;              /* 0x108 */
    int if_ioctl_result;               /* 0x10C */
    NetIfEvent *if_ev_head;            /* 0x110 */
    NetIfEvent **if_ev_tail;           /* 0x114 */
    u32 if_ev_mask;                    /* 0x118 */
    u32 if_ev_arg;                     /* 0x11C */
} NetIfnet;

/* PPP softc extending NetIfnet */
typedef struct NetPppSoftc {
    NetIfnet sc_if;                    /* 0x000..0x11F */
    u32 sc_pre_flags[7];               /* 0x120..0x13B */
    u32 sc_flags;                      /* 0x13C (316) */
    u32 sc_lcp_state[67];              /* 0x140..0x24B */
    u32 sc_tx_accm_ctl;                /* 0x24C (588, word 147) */
    u32 sc_rx_accm_ctl;                /* 0x250 (592, word 148) */
    u32 sc_accm_reserved[7];           /* 0x254..0x26F */
    u32 sc_raccm;                      /* 0x270 (624, word 156) */
    u32 sc_xaccm_lo;                   /* 0x274 (628, word 157) */
    u32 sc_drv_cfg[4];                 /* 0x278..0x287 (words 158..161) */
    u32 sc_ppp_buf[2106];              /* 0x288..0x236F */
    void (*sc_start_link)(void *);     /* 0x2370 (9072, word 2268) */
    void (*sc_stop_link)(void *);      /* 0x2374 (9076, word 2269) */
    int (*sc_sync_to_drv)(void *);     /* 0x2378 (9080, word 2270) */
    u32 sc_cb_reserved[3];             /* 0x237C..0x2387 */
    int (*sc_sync_from_drv)(void *);   /* 0x2388 (9096, word 2274) */
    u32 sc_sync_pad;                   /* 0x238C (9100, word 2275) */
    u32 sc_xaccm[8];                   /* 0x2390..0x23AF (9104..9135) */
    u32 sc_raccm_cfg;                  /* 0x23B0 (9136, word 2284) */
} NetPppSoftc;

/* Generic ifreq union */
typedef struct NetIfreq {
    char ifr_name[16];                 /* 0x00 */
    union {
        NetSockaddr ifr_addr;          /* 0x10 */
        u16 ifr_flags;                 /* 0x10 */
        s32 ifr_metric;                /* 0x10 */
        u32 ifr_mtu;                   /* 0x10 */
        void *ifr_ptr;                 /* 0x10 */
        u32 ifr_word;                  /* 0x10 */
        u8 ifr_mac[6];                 /* 0x10 */
    };
} NetIfreq;

/* Compile-time layout assertions (C99) */
#define PSPNET_STATIC_ASSERT(name, cond) typedef char pspnet_assert_##name[(cond) ? 1 : -1]
PSPNET_STATIC_ASSERT(thread_info_size, sizeof(NetThreadInfo) == 92);
PSPNET_STATIC_ASSERT(mbuf_ext_off, offsetof(NetMbuf, m_ext) == 48);
PSPNET_STATIC_ASSERT(ifaddr_rtreq, offsetof(NetIfaddr, ifa_rtrequest) == 40);
PSPNET_STATIC_ASSERT(ifaddr_size, sizeof(NetIfaddr) == 56);
PSPNET_STATIC_ASSERT(ifdata_size, sizeof(NetIfData) == 120);
PSPNET_STATIC_ASSERT(ifnet_data_off, offsetof(NetIfnet, if_data) == 56);
PSPNET_STATIC_ASSERT(ifnet_output_off, offsetof(NetIfnet, if_output) == 176);
PSPNET_STATIC_ASSERT(ifnet_snd_off, offsetof(NetIfnet, if_snd) == 204);
PSPNET_STATIC_ASSERT(ifnet_sadl_off, offsetof(NetIfnet, if_sadl) == 224);
PSPNET_STATIC_ASSERT(ifnet_ev_off, offsetof(NetIfnet, if_ev_head) == 272);
PSPNET_STATIC_ASSERT(ifnet_size, sizeof(NetIfnet) == 288);
PSPNET_STATIC_ASSERT(ppp_flags_off, offsetof(NetPppSoftc, sc_flags) == 316);
PSPNET_STATIC_ASSERT(ppp_tx_accm_off, offsetof(NetPppSoftc, sc_tx_accm_ctl) == 588);
PSPNET_STATIC_ASSERT(ppp_raccm_off, offsetof(NetPppSoftc, sc_raccm) == 624);
PSPNET_STATIC_ASSERT(ppp_start_link_off, offsetof(NetPppSoftc, sc_start_link) == 9072);
PSPNET_STATIC_ASSERT(ppp_xaccm_off, offsetof(NetPppSoftc, sc_xaccm) == 9104);
PSPNET_STATIC_ASSERT(ppp_raccm_cfg_off, offsetof(NetPppSoftc, sc_raccm_cfg) == 9136);

/* Core exported functions used across files */
extern int sceNet_lib_94B44F26(void); /* splnet */
extern int sceNet_lib_515B2F33(int level); /* splx */
extern void sceNet_lib_7C3B86C5(int bit); /* schednetisr_nosig */
extern int sceNet_lib_05D525E4(int bit); /* schednetisr */
extern NetThreadInfo *sceNet_lib_CA3CF5EB(int *errp);
extern int sceNet_lib_757085B0(NetThreadInfo *ti, int err);
extern NetThreadInfo *sceNet_lib_BB2B3DDB(SceUID thid);
extern void sceNet_lib_A8B6205A(NetCallout *co, u32 usec, void (*fn)(void *), void *arg);
extern int sceNet_lib_A93A93E9(NetCallout *co);
extern void sceNet_lib_25CC373A(NetCallout *co);
extern void sceNet_lib_DCBC596E(NetCallout *co);
extern void sceNet_lib_7C86FBA4(NetCallout *co, u32 ticks, void (*fn)(void *), void *arg);
extern int sceNet_lib_F6DB0A0B(u32 wchan, u32 usec);
extern int sceNet_lib_205E8D17(u32 wchan, u32 ticks);
extern int sceNet_lib_7574FDA1(u32 wchan);
extern int sceNet_lib_B8C4A858(SceUID thid, u32 wchan);
extern int sceNet_lib_C9C97945(NetThreadInfo *ti);
extern u64 sceNet_lib_4B2B3416(NetTimeval *tv);
extern int sceNet_lib_2B42872F(void *cp, int len, NetUio *uio);
extern void sceNet_lib_92633D8D(NetThreadInfo *ti, SceUID *sel_thid);
extern void sceNet_lib_B9C780C7(SceUID *sel_thid);
extern void *sceNet_lib_B68E1EEA(void);
extern int sceNet_lib_13A8B98A(void *so, u32 cmd, void *data, int wait);
extern NetIfnet *sceNet_lib_1D10419C(const char *name);
extern int sceNet_lib_701DDDC3(NetIfnet *ifp);
extern int sceNet_lib_D5A03BC0(NetIfnet *ifp);
extern void sceNet_lib_EDB11CB4(NetIfnet *ifp);
extern int sceNet_lib_8C55B410(void);
extern void sceNet_lib_EB6DE71A(int req, int rt, int rtinfo);
extern int sceNet_lib_1858883D(void);
extern int sceNet_lib_DA02F383(const char *ifname, u32 *out);
extern int sceNet_lib_83FE280A(const char *ifname, void *arg);
extern int sceNet_lib_891723D5(const char *ifname, void *arg);
extern int sceNet_lib_A55C914F(const char *ifname, void *arg);

/* Libc exports */
extern int sceNet_lib_C13C9307(int ch); /* toupper */
extern int sceNet_lib_96EF9DA1(int ch); /* tolower */
extern int sceNet_lib_750F705D(int ch); /* ctype lookup */
extern int sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, int len); /* memcmp */
extern void *sceNet_lib_4753D878(void *dst, const void *src, int len); /* memmove */
extern void *sceNet_lib_8E798A2F(const void *buf, int ch, int len); /* memchr */
extern int sceNet_lib_4BF83CF4(const void *lhs, const void *rhs, int len); /* bcmp */
extern void *sceNet_lib_2F305274(const void *src, void *dst, int len); /* bcopy */
extern void *sceNet_lib_8B615F52(void *dst, int len); /* bzero */
extern int sceNet_lib_CF705E46(char *buf, const char *fmt, ...); /* sprintf */
extern int sceNet_lib_8687B5AB(char *buf, const char *fmt, void *ap); /* vsprintf */
extern int sceNet_lib_A0F16ABD(const char *lhs, const char *rhs); /* strcmp */
extern char *sceNet_lib_80C9F02A(char *dst, const char *src); /* strcpy */
extern int sceNet_lib_D8722983(const char *str); /* strlen */
extern int sceNet_lib_94DCA9F0(const char *lhs, const char *rhs, int len); /* strncmp */
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, int len); /* strncpy */

/* BPF internal hooks called by pspnet.c */
extern void bpf_init(int count);
extern void bpf_attach(void **bp_ptr, NetIfnet *ifp, u32 dlt, u32 hdrlen);
extern void bpf_detach(NetIfnet *ifp);
extern void bpf_mtap(void *bp, NetMbuf *mbuf);

/* Select wait channel */
extern u32 g_selwait;

#endif /* PSPNET_INTERNAL_H */
