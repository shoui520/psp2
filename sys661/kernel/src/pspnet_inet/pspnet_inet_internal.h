#ifndef PSPNET_INET_INTERNAL_H
#define PSPNET_INET_INTERNAL_H

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

#define INET_NOINLINE __attribute__((noinline))
#define INET_STATIC_ASSERT(name, cond) typedef char inet_assert_##name[(cond) ? 1 : -1]

/* Unaligned wire-format helpers (replaces raw offset pointer casts) */
static __inline__ __attribute__((always_inline)) u16 inetReadU16At(const void *base, s32 byteOff)
{
    const u8 *p = (const u8 *)base;
    return (u16)((u16)p[byteOff] | ((u16)p[byteOff + 1] << 8));
}

static __inline__ __attribute__((always_inline)) u32 inetReadU32At(const void *base, s32 byteOff)
{
    const u8 *p = (const u8 *)base;
    return (u32)p[byteOff] |
           ((u32)p[byteOff + 1] << 8) |
           ((u32)p[byteOff + 2] << 16) |
           ((u32)p[byteOff + 3] << 24);
}

static __inline__ __attribute__((always_inline)) void inetWriteU16At(void *base, s32 byteOff, u16 val)
{
    u8 *p = (u8 *)base;
    p[byteOff] = (u8)(val & 0xFFU);
    p[byteOff + 1] = (u8)((val >> 8) & 0xFFU);
}

static __inline__ __attribute__((always_inline)) void inetWriteU32At(void *base, s32 byteOff, u32 val)
{
    u8 *p = (u8 *)base;
    p[byteOff] = (u8)(val & 0xFFU);
    p[byteOff + 1] = (u8)((val >> 8) & 0xFFU);
    p[byteOff + 2] = (u8)((val >> 16) & 0xFFU);
    p[byteOff + 3] = (u8)((val >> 24) & 0xFFU);
}

/* Byte-order helpers (Allegrex MIPS little-endian hardware instructions) */
static __inline__ __attribute__((always_inline)) u16 inetBswap16(u16 val)
{
    return (u16)__builtin_allegrex_wsbh((u32)val);
}

static __inline__ __attribute__((always_inline)) u32 inetBswap32(u32 val)
{
    return (u32)__builtin_allegrex_wsbw(val);
}

/* SCE NetInet error codes */
#define SCE_NET_INET_ERROR_INSUFFICIENT_STACK  ((int)0x80410005)
#define SCE_NET_INET_ERROR_ENOMEM              ((int)0x80410001)
#define SCE_NET_INET_ERROR_ALREADY_INIT        ((int)0x80410201)
#define SCE_NET_INET_ERROR_NO_IF               ((int)0x80410203)
#define SCE_NET_INET_ERROR_IF_ADDR_GET         ((int)0x80410204)
#define SCE_NET_INET_ERROR_IF_ADDR_SET         ((int)0x80410205)
#define SCE_NET_INET_ERROR_IF_ADDR_DEL         ((int)0x80410206)
#define SCE_NET_INET_ERROR_NO_GATEWAY          ((int)0x80410207)
#define SCE_NET_INET_ERROR_ROUTE_SYSCTL        ((int)0x80410208)
#define SCE_NET_INET_ERROR_ROUTE_SEND          ((int)0x80410209)
#define SCE_NET_INET_ERROR_ROUTE_DEL           ((int)0x8041020A)
#define SCE_NET_INET_ERROR_EINVAL_PARAM        ((int)0x8041020B)
#define SCE_NET_INET_ERROR_SOCKET_CREATE       ((int)0x80410280)
#define SCE_NET_INET_ERROR_ARP_SEND            ((int)0x80410281)
#define SCE_NET_INET_ERROR_ARP_RECV            ((int)0x80410282)
#define SCE_NET_INET_ERROR_ARP_ADD             ((int)0x80410283)
#define SCE_NET_INET_ERROR_ARP_DEL             ((int)0x80410284)
#define SCE_NET_INET_ERROR_ARP_FLUSH           ((int)0x80410285)

/* PSP/BSD errno constants used by SceNetInet */
#define INET_EPERM         1
#define INET_ENOENT        2
#define INET_ESRCH         3
#define INET_EINTR         4
#define INET_EIO           5
#define INET_ENXIO         6
#define INET_EBADF         9
#define INET_EAGAIN        11
#define INET_ENOMEM        12
#define INET_EACCES        13
#define INET_EFAULT        14
#define INET_EBUSY         16
#define INET_EEXIST        17
#define INET_ENODEV        19
#define INET_ENOTDIR       20
#define INET_EISDIR        21
#define INET_EINVAL        22
#define INET_EMFILE        24
#define INET_ENOTTY        25
#define INET_ENOSPC        28
#define INET_EPIPE         32
#define INET_ERANGE        34
#define INET_EOPNOTSUPP    95
#define INET_ENOBUFS       105
#define INET_EAFNOSUPPORT  106
#define INET_EPROTOTYPE    107
#define INET_ENOTSOCK      108
#define INET_ENOPROTOOPT   109
#define INET_ESHUTDOWN     110
#define INET_ECONNREFUSED  111
#define INET_EADDRINUSE    112
#define INET_ECONNABORTED  113
#define INET_ENETUNREACH   114
#define INET_ENETDOWN      115
#define INET_ETIMEDOUT     116
#define INET_EHOSTDOWN     117
#define INET_EHOSTUNREACH  118
#define INET_EINPROGRESS   119
#define INET_EALREADY      120
#define INET_EDESTADDRREQ  121
#define INET_EMSGSIZE      122
#define INET_EPROTONOSUPPORT 123
#define INET_ESOCKTNOSUPPORT 124
#define INET_EADDRNOTAVAIL 125
#define INET_ENETRESET     126
#define INET_EISCONN       127
#define INET_ENOTCONN      128
#define INET_ETOOMANYREFS  129
#define INET_ECONNRESET    104

/* Address families and socket types */
#define INET_AF_UNSPEC     0
#define INET_AF_INET       2
#define INET_AF_ROUTE      17
#define INET_AF_LINK       18

#define INET_SOCK_STREAM     1
#define INET_SOCK_DGRAM      2
#define INET_SOCK_RAW        3
#define INET_SOCK_DGRAM_P2P  6
#define INET_SOCK_STREAM_P2P 10

#define INET_IPPROTO_IP      0
#define INET_IPPROTO_ICMP    1
#define INET_IPPROTO_IGMP    2
#define INET_IPPROTO_TCP     6
#define INET_IPPROTO_UDP     17
#define INET_IPPROTO_RAW     255

#define INET_SOL_SOCKET      0xFFFF

/* Protocol request codes (BSD pr_usrreq) */
#define INET_PRU_ATTACH       0
#define INET_PRU_DETACH       1
#define INET_PRU_BIND         2
#define INET_PRU_LISTEN       3
#define INET_PRU_CONNECT      4
#define INET_PRU_ACCEPT       5
#define INET_PRU_DISCONNECT   6
#define INET_PRU_SHUTDOWN     7
#define INET_PRU_RCVD         8
#define INET_PRU_SEND         9
#define INET_PRU_ABORT        10
#define INET_PRU_CONTROL      11
#define INET_PRU_SENSE        12
#define INET_PRU_RCVOOB       13
#define INET_PRU_SENDOOB      14
#define INET_PRU_SOCKADDR     15
#define INET_PRU_PEERADDR     16
#define INET_PRU_CONNECT2     17
#define INET_PRU_FASTTIMO     18
#define INET_PRU_SLOWTIMO     19
#define INET_PRU_PROTORCV     20
#define INET_PRU_PROTOSEND    21
#define INET_PRU_PURGEIF      22

#define INET_PRCO_GETOPT      0
#define INET_PRCO_SETOPT      1

/* Socket option bits (so_options) */
#define INET_SO_DEBUG         0x0001
#define INET_SO_ACCEPTCONN    0x0002
#define INET_SO_REUSEADDR     0x0004
#define INET_SO_KEEPALIVE     0x0008
#define INET_SO_DONTROUTE     0x0010
#define INET_SO_BROADCAST     0x0020
#define INET_SO_USELOOPBACK   0x0040
#define INET_SO_LINGER        0x0080
#define INET_SO_OOBINLINE     0x0100
#define INET_SO_REUSEPORT     0x0200
#define INET_SO_TIMESTAMP     0x0400
#define INET_SO_ONESBCAST     0x0800
#define INET_SO_SNDBUF        0x1001
#define INET_SO_RCVBUF        0x1002
#define INET_SO_SNDLOWAT      0x1003
#define INET_SO_RCVLOWAT      0x1004
#define INET_SO_SNDTIMEO      0x1005
#define INET_SO_RCVTIMEO      0x1006
#define INET_SO_ERROR         0x1007
#define INET_SO_TYPE          0x1008
#define INET_SO_NBIO          0x1009

/* Socket state bits (so_state) */
#define INET_SS_NOFDREF         0x0001
#define INET_SS_ISCONNECTED     0x0002
#define INET_SS_ISCONNECTING    0x0004
#define INET_SS_ISDISCONNECTING 0x0008
#define INET_SS_CANTSENDMORE    0x0010
#define INET_SS_CANTRCVMORE     0x0020
#define INET_SS_RCVATMARK       0x0040
#define INET_SS_NBIO            0x0080
#define INET_SS_ISDISCONNECTED  0x0200
#define INET_SS_MORETOCOME      0x0400
#define INET_SS_ABORTED         0x0800

/* Socket buffer flags (sb_flags) */
#define INET_SB_LOCK    0x0001
#define INET_SB_WANT    0x0002
#define INET_SB_WAIT    0x0004
#define INET_SB_SEL     0x0008
#define INET_SB_ASYNC   0x0010
#define INET_SB_UPCALL  0x0020
#define INET_SB_NOINTR  0x0040

/* Protosw flags (pr_flags) */
#define INET_PR_ATOMIC     0x0001
#define INET_PR_ADDR       0x0002
#define INET_PR_CONNREQUIRED 0x0004
#define INET_PR_WANTRCVD   0x0008
#define INET_PR_RIGHTS     0x0010
#define INET_PR_LISTEN     0x0020
#define INET_PR_LASTHDR    0x0040

/* Mbuf types & flags */
#define INET_MT_DATA       1
#define INET_MT_HEADER     2
#define INET_MT_SONAME     3
#define INET_MT_SOOPTS     4
#define INET_MT_FTABLE     5
#define INET_MT_CONTROL    6
#define INET_MT_OOBDATA    7

#define INET_M_EXT         0x0001
#define INET_M_PKTHDR      0x0002
#define INET_M_EOR         0x0004
#define INET_M_CLUSTER     0x0008
#define INET_M_BCAST       0x0100
#define INET_M_MCAST       0x0200
#define INET_M_LINK0       0x1000
#define INET_M_DONTWAIT    1
#define INET_M_WAIT        0

/* Route flags & messages */
#define INET_RTF_UP        0x0001
#define INET_RTF_GATEWAY   0x0002
#define INET_RTF_HOST      0x0004
#define INET_RTF_REJECT    0x0008
#define INET_RTF_DYNAMIC   0x0010
#define INET_RTF_MODIFIED  0x0020
#define INET_RTF_DONE      0x0040
#define INET_RTF_CLONING   0x0100
#define INET_RTF_XRESOLVE  0x0200
#define INET_RTF_LLINFO    0x0400
#define INET_RTF_STATIC    0x0800
#define INET_RTF_BLACKHOLE 0x1000
#define INET_RTF_CLONED    0x2000
#define INET_RTF_PROTO2    0x4000
#define INET_RTF_PROTO1    0x8000

#define INET_RTM_ADD       1
#define INET_RTM_DELETE    2
#define INET_RTM_CHANGE    3
#define INET_RTM_GET       4
#define INET_RTM_LOSING    5
#define INET_RTM_REDIRECT  6
#define INET_RTM_MISS      7
#define INET_RTM_LOCK      8
#define INET_RTM_RESOLVE   11
#define INET_RTM_NEWADDR   12
#define INET_RTM_DELADDR   13
#define INET_RTM_IFINFO    14
#define INET_RTM_IFANNOUNCE 16
#define INET_RTM_VERSION   3

#define INET_RTA_DST       0x01
#define INET_RTA_GATEWAY   0x02
#define INET_RTA_NETMASK   0x04
#define INET_RTA_GENMASK   0x08
#define INET_RTA_IFP       0x10
#define INET_RTA_IFA       0x20
#define INET_RTA_AUTHOR    0x40
#define INET_RTA_BRD       0x80

/* Interface flags */
#define INET_IFF_UP          0x0001
#define INET_IFF_BROADCAST   0x0002
#define INET_IFF_DEBUG       0x0004
#define INET_IFF_LOOPBACK    0x0008
#define INET_IFF_POINTOPOINT 0x0010
#define INET_IFF_RUNNING     0x0040
#define INET_IFF_NOARP       0x0080
#define INET_IFF_PROMISC     0x0100
#define INET_IFF_ALLMULTI    0x0200
#define INET_IFF_OACTIVE     0x0400
#define INET_IFF_SIMPLEX     0x0800
#define INET_IFF_MULTICAST   0x8000

/* ===================================================================== */
/* Shared pspnet / ifhandle / kernel / wlan structures                   */
/* ===================================================================== */

typedef struct InetCallout {
    struct InetCallout *ready_next;
    struct InetCallout **ready_prev;
    struct InetCallout *pend_next;
    struct InetCallout **pend_prev;
    void (*cb)(void *);
    void *cb_arg;
    u32 timeout_us;
    u32 flags;
    SceUID alarm_id;
} InetCallout;
INET_STATIC_ASSERT(callout_size, sizeof(InetCallout) == 36);

struct InetSocket;

typedef struct InetThreadInfo {
    struct InetThreadInfo *list_next;   /* 0x00 */
    struct InetThreadInfo **list_prev;  /* 0x04 */
    SceUID thid;                        /* 0x08 */
    s16 refcount;                       /* 0x0C */
    u16 pad;                            /* 0x0E */
    int err;                            /* 0x10 */
    u32 status;                         /* 0x14 */
    struct InetThreadInfo *slp_next;    /* 0x18 */
    struct InetThreadInfo **slp_prev;   /* 0x1C */
    u32 wchan;                          /* 0x20 */
    InetCallout callout;                /* 0x24..0x47 */
    u32 slp_flags;                      /* 0x48 */
    SceUID sema_id;                     /* 0x4C */
    struct InetSocket *active_sock;     /* 0x50 (80) */
    u32 reserved_tail[2];               /* 0x54..0x5B */
} InetThreadInfo;
INET_STATIC_ASSERT(thread_info_size, sizeof(InetThreadInfo) == 92);

typedef struct InetTimeval {
    u32 tv_sec;
    u32 tv_usec;
} InetTimeval;

typedef struct InetIovec {
    void *iov_base;
    s32 iov_len;
} InetIovec;

typedef struct InetUio {
    InetIovec *uio_iov;                 /* 0x00 */
    s32 uio_iovcnt;                     /* 0x04 */
    u32 uio_offset;                     /* 0x08 */
    s32 uio_resid;                      /* 0x0C */
    s32 uio_rw;                         /* 0x10 */
} InetUio;

typedef struct InetMsghdr {
    void *msg_name;                     /* 0x00 */
    u32 msg_namelen;                    /* 0x04 */
    InetIovec *msg_iov;                 /* 0x08 */
    u32 msg_iovlen;                     /* 0x0C */
    void *msg_control;                  /* 0x10 */
    u32 msg_controllen;                 /* 0x14 */
    u32 msg_flags;                      /* 0x18 */
} InetMsghdr;
INET_STATIC_ASSERT(msghdr_size, sizeof(InetMsghdr) == 28);

typedef struct InetCmsghdr {
    u32 cmsg_len;
    s32 cmsg_level;
    s32 cmsg_type;
} InetCmsghdr;

typedef struct InetPollfd {
    s32 sock;
    s16 events;
    s16 revents;
} InetPollfd;

struct InetIfnet;

typedef struct InetMbufExt {
    u8 *ext_buf;                        /* 0x30 (48) */
    void *ext_free;                     /* 0x34 (52) */
    void *ext_arg;                      /* 0x38 (56) */
    u32 ext_size;                       /* 0x3C (60) */
    u32 ext_type;                       /* 0x40 (64) */
    struct InetMbuf *ext_nextref;       /* 0x44 (68) */
    struct InetMbuf *ext_prevref;       /* 0x48 (72) */
} InetMbufExt;

typedef struct InetMbuf {
    struct InetMbuf *m_next;            /* 0x00 (0) */
    struct InetMbuf *m_nextpkt;         /* 0x04 (4) */
    u8 *m_data;                         /* 0x08 (8) */
    s32 m_len;                          /* 0x0C (12) */
    s16 m_type;                         /* 0x10 (16) */
    u16 m_flags;                        /* 0x12 (18) */
    union {
        struct {
            struct InetIfnet *rcvif;    /* 0x14 (20) */
            s32 len;                    /* 0x18 (24) */
            u32 csum_flags;             /* 0x1C (28) */
            u32 aux[4];                 /* 0x20..0x2F */
            union {
                InetMbufExt ext;        /* 0x30..0x4B */
                u8 pktdat[208];         /* 0x30..0xFF */
            } u;
        } mh_pkt;
        u8 mh_dat[236];                 /* 0x14..0xFF */
    } mh;
} InetMbuf;
INET_STATIC_ASSERT(mbuf_size, sizeof(InetMbuf) == 256);
INET_STATIC_ASSERT(mbuf_ext_off, offsetof(InetMbuf, mh.mh_pkt.u.ext) == 48);

#define M_RCVIF(m)    ((m)->mh.mh_pkt.rcvif)
#define M_PKTLEN(m)   ((m)->mh.mh_pkt.len)
#define M_CSUM(m)     ((m)->mh.mh_pkt.csum_flags)
#define M_EXT(m)      ((m)->mh.mh_pkt.u.ext)
#define M_PKTDAT(m)   ((m)->mh.mh_pkt.u.pktdat)
#define M_DAT(m)      ((m)->mh.mh_dat)
#define m_pkthdr      mh.mh_pkt
#define m_ext         mh.mh_pkt.u.ext

typedef struct InetSockaddr {
    u8 sa_len;
    u8 sa_family;
    u8 sa_data[14];
} InetSockaddr;

typedef struct InetSockaddrIn {
    u8 sin_len;
    u8 sin_family;
    u16 sin_port;
    u32 sin_addr;
    u8 sin_zero[8];
} InetSockaddrIn;
INET_STATIC_ASSERT(sockaddr_in_size, sizeof(InetSockaddrIn) == 16);

typedef struct InetSockaddrDl {
    u8 sdl_len;                         /* 0x00 */
    u8 sdl_family;                      /* 0x01 */
    u16 sdl_index;                      /* 0x02 */
    u8 sdl_type;                        /* 0x04 */
    u8 sdl_nlen;                        /* 0x05 */
    u8 sdl_alen;                        /* 0x06 */
    u8 sdl_slen;                        /* 0x07 */
    u8 sdl_data[12];                    /* 0x08..0x13 */
} InetSockaddrDl;
INET_STATIC_ASSERT(sockaddr_dl_size, sizeof(InetSockaddrDl) == 20);

typedef struct InetIfaddr {
    InetSockaddr *ifa_addr;             /* 0x00 */
    InetSockaddr *ifa_dstaddr;          /* 0x04 */
    InetSockaddr *ifa_netmask;          /* 0x08 */
    struct InetIfnet *ifa_ifp;          /* 0x0C */
    struct InetIfaddr *ifa_next;        /* 0x10 */
    struct InetIfaddr **ifa_prev;       /* 0x14 */
    u32 ifa_spare[4];                   /* 0x18..0x27 */
    void (*ifa_rtrequest)(int, void *, void *); /* 0x28 */
    u32 ifa_flags;                      /* 0x2C */
    s32 ifa_refcnt;                     /* 0x30 */
    s32 ifa_metric;                     /* 0x34 */
} InetIfaddr;
INET_STATIC_ASSERT(ifaddr_size, sizeof(InetIfaddr) == 56);

typedef struct InetIfData {
    u8 ifi_type;                        /* 0x00 */
    u8 ifi_addrlen;                     /* 0x01 */
    u8 ifi_hdrlen;                      /* 0x02 */
    u8 ifi_link_state;                  /* 0x03 */
    u32 ifi_recvquota;                  /* 0x04 */
    u64 ifi_mtu;                        /* 0x08 */
    u64 ifi_metric;                     /* 0x10 */
    u64 ifi_baudrate;                   /* 0x18 */
    u64 ifi_ipackets;                   /* 0x20 */
    u64 ifi_ierrors;                    /* 0x28 */
    u64 ifi_opackets;                   /* 0x30 */
    u64 ifi_oerrors;                    /* 0x38 */
    u64 ifi_collisions;                 /* 0x40 */
    u64 ifi_ibytes;                     /* 0x48 */
    u64 ifi_obytes;                     /* 0x50 */
    u64 ifi_imcasts;                    /* 0x58 */
    u64 ifi_omcasts;                    /* 0x60 */
    u64 ifi_iqdrops;                    /* 0x68 */
    u64 ifi_noproto;                    /* 0x70 */
} InetIfData;
INET_STATIC_ASSERT(ifdata_size, sizeof(InetIfData) == 120);

typedef struct InetIfQueue {
    InetMbuf *ifq_head;                 /* 0x00 */
    InetMbuf *ifq_tail;                 /* 0x04 */
    s32 ifq_len;                        /* 0x08 */
    s32 ifq_maxlen;                     /* 0x0C */
    u32 ifq_drops;                      /* 0x10 */
} InetIfQueue;

typedef struct InetIfnet {
    void *if_softc;                     /* 0x00 */
    struct InetIfnet *if_next;          /* 0x04 */
    struct InetIfnet **if_prev;         /* 0x08 */
    InetIfaddr *if_addrhead;            /* 0x0C */
    InetIfaddr **if_addrtail;           /* 0x10 */
    char if_xname[16];                  /* 0x14 */
    s32 if_pcount;                      /* 0x24 */
    void *if_bpf;                       /* 0x28 */
    u16 if_index;                       /* 0x2C */
    u16 if_timer;                       /* 0x2E */
    u16 if_flags;                       /* 0x30 */
    u16 if_pad;                         /* 0x32 */
    u32 if_capabilities;                /* 0x34 */
    InetIfData if_data;                 /* 0x38..0xAF */
    int (*if_output)(struct InetIfnet *, InetMbuf *, InetSockaddr *, void *); /* 0xB0 */
    void (*if_input)(struct InetIfnet *, InetMbuf *);                         /* 0xB4 */
    void (*if_start)(struct InetIfnet *);                                     /* 0xB8 */
    int (*if_ioctl)(struct InetIfnet *, u32, void *);                         /* 0xBC */
    int (*if_init)(struct InetIfnet *);                                       /* 0xC0 */
    void (*if_watchdog)(struct InetIfnet *);                                  /* 0xC4 */
    void (*if_drain)(struct InetIfnet *);                                     /* 0xC8 */
    InetIfQueue if_snd;                 /* 0xCC..0xDF */
    InetSockaddrDl *if_sadl;            /* 0xE0 */
    const u8 *if_broadcastaddr;         /* 0xE4 */
    void *if_bridge;                    /* 0xE8 */
    u8 if_hwaddr[8];                    /* 0xEC */
    int (*if_drv_up)(void *);           /* 0xF4 */
    int (*if_drv_down)(void *);         /* 0xF8 */
    int (*if_drv_ioctl)(void *, int, void *); /* 0xFC */
    SceUID if_ioctl_done_sema;          /* 0x100 */
    SceUID if_ioctl_lock_sema;          /* 0x104 */
    s32 if_ioctl_pending;               /* 0x108 */
    s32 if_ioctl_result;                /* 0x10C */
    void *if_ev_head;                   /* 0x110 */
    void *if_ev_tail;                   /* 0x114 */
    u32 if_ev_mask;                     /* 0x118 */
    u32 if_ev_arg;                      /* 0x11C */
} InetIfnet;
INET_STATIC_ASSERT(ifnet_size, sizeof(InetIfnet) == 288);

/* ===================================================================== */
/* Domain, Protosw, Socket, and SockBuf structures                       */
/* ===================================================================== */

struct InetDomain;

typedef struct InetProtosw {
    s32 pr_type;                        /* 0x00 */
    struct InetDomain *pr_domain;       /* 0x04 */
    s16 pr_protocol;                    /* 0x08 */
    u16 pr_flags;                       /* 0x0A */
    void (*pr_input)(InetMbuf *, ...);  /* 0x0C */
    int (*pr_output)(InetMbuf *, ...);  /* 0x10 */
    void *(*pr_ctlinput)(int, InetSockaddr *, void *); /* 0x14 */
    int (*pr_ctloutput)(int, struct InetSocket *, int, int, InetMbuf **); /* 0x18 */
    int (*pr_usrreq)(struct InetSocket *, int, InetMbuf *, InetMbuf *, InetMbuf *, void *); /* 0x1C */
    void (*pr_init)(void);              /* 0x20 */
    void (*pr_fasttimo)(void);          /* 0x24 */
    void (*pr_slowtimo)(void);          /* 0x28 */
    void (*pr_drain)(void);             /* 0x2C */
    int (*pr_sysctl)(int *, u32, void *, u32 *, void *, u32); /* 0x30 */
} InetProtosw;
INET_STATIC_ASSERT(protosw_size, sizeof(InetProtosw) == 52);

typedef struct InetDomain {
    s32 dom_family;                     /* 0x00 */
    const char *dom_name;               /* 0x04 */
    void (*dom_init)(void);             /* 0x08 */
    int (*dom_externalize)(InetMbuf *); /* 0x0C */
    void (*dom_dispose)(InetMbuf *);    /* 0x10 */
    InetProtosw *dom_protosw;           /* 0x14 */
    InetProtosw *dom_protosw_end;       /* 0x18 */
    struct InetDomain *dom_next;        /* 0x1C */
    int (*dom_rtattach)(void **, int);  /* 0x20 */
    s32 dom_rtoffset;                   /* 0x24 */
    s32 dom_maxrtkey;                   /* 0x28 */
} InetDomain;
INET_STATIC_ASSERT(domain_size, sizeof(InetDomain) == 44);

typedef struct InetSockBuf {
    u32 sb_cc;                          /* 0x00 */
    u32 sb_hiwat;                       /* 0x04 */
    u32 sb_mbcnt;                       /* 0x08 */
    u32 sb_mbmax;                       /* 0x0C */
    s32 sb_lowat;                       /* 0x10 */
    InetMbuf *sb_mb;                    /* 0x14 */
    SceUID sb_sel_thid;                 /* 0x18 */
    u32 sb_sel_flags;                   /* 0x1C */
    u16 sb_flags;                       /* 0x20 */
    u16 sb_pad;                         /* 0x22 */
    s32 sb_timeo;                       /* 0x24 */
} InetSockBuf;
INET_STATIC_ASSERT(sockbuf_size, sizeof(InetSockBuf) == 40);

typedef struct InetSocketQueue {
    struct InetSocket *tqh_first;
    struct InetSocket **tqh_last;
} InetSocketQueue;

typedef struct InetSocket {
    s16 so_type;                        /* 0x00 (0) */
    u16 so_options;                     /* 0x02 (2) */
    s32 so_linger;                      /* 0x04 (4) */
    u16 so_state;                       /* 0x08 (8) */
    u16 so_pad;                         /* 0x0A (10) */
    void *so_pcb;                       /* 0x0C (12) */
    InetProtosw *so_proto;              /* 0x10 (16) */
    struct InetSocket *so_head;         /* 0x14 (20) */
    InetSocketQueue *so_onq;            /* 0x18 (24) */
    InetSocketQueue so_q0;              /* 0x1C (28) */
    InetSocketQueue so_q;               /* 0x24 (36) */
    struct InetSocket *so_qe_next;      /* 0x2C (44) */
    struct InetSocket **so_qe_prev;     /* 0x30 (48) */
    s16 so_q0len;                       /* 0x34 (52) */
    s16 so_qlen;                        /* 0x36 (54) */
    s16 so_qlimit;                      /* 0x38 (56) */
    u16 so_timeo;                       /* 0x3A (58) */
    u16 so_error;                       /* 0x3C (60) */
    u16 so_oob_pad;                     /* 0x3E (62) */
    u32 so_oobmark;                     /* 0x40 (64) */
    InetSockBuf so_rcv;                 /* 0x44 (68..107) */
    InetSockBuf so_snd;                 /* 0x6C (108..147) */
    u32 so_uid_pad;                     /* 0x94 (148) */
    void (*so_upcall)(struct InetSocket *, void *, int); /* 0x98 (152) */
    void *so_upcallarg;                 /* 0x9C (156) */
    int (*so_send)(struct InetSocket *, InetMbuf *, InetUio *, InetMbuf *, InetMbuf *, int); /* 0xA0 (160) */
    int (*so_receive)(struct InetSocket *, InetMbuf **, InetUio *, InetMbuf **, InetMbuf **, u32 *); /* 0xA4 (164) */
    s32 so_sock_id;                     /* 0xA8 (168) */
    s32 so_kermit_id;                   /* 0xAC (172) */
    struct InetSocket *so_list_next;    /* 0xB0 (176) */
    struct InetSocket **so_list_prev;   /* 0xB4 (180) */
    InetThreadInfo *so_owner_ti;        /* 0xB8 (184) */
    struct InetSocket *so_owner_next;   /* 0xBC (188) */
} InetSocket;
INET_STATIC_ASSERT(socket_size, sizeof(InetSocket) == 192);
INET_STATIC_ASSERT(socket_rcv_off, offsetof(InetSocket, so_rcv) == 68);
INET_STATIC_ASSERT(socket_snd_off, offsetof(InetSocket, so_snd) == 108);
INET_STATIC_ASSERT(socket_id_off, offsetof(InetSocket, so_sock_id) == 168);
INET_STATIC_ASSERT(socket_owner_off, offsetof(InetSocket, so_owner_ti) == 184);

/* ===================================================================== */
/* Routing, Radix, Raw, ARP, IP, UDP, TCP, SPPP, PPPoE structures        */
/* ===================================================================== */

typedef struct InetRtMetrics {
    u32 rmx_locks;                      /* 0x00 */
    u32 rmx_mtu;                        /* 0x04 */
    u32 rmx_hopcount;                   /* 0x08 */
    u32 rmx_expire;                     /* 0x0C */
    u32 rmx_recvpipe;                   /* 0x10 */
    u32 rmx_sendpipe;                   /* 0x14 */
    u32 rmx_ssthresh;                   /* 0x18 */
    u32 rmx_rtt;                        /* 0x1C */
    u32 rmx_rttvar;                     /* 0x20 */
    u32 rmx_pksent;                     /* 0x24 */
} InetRtMetrics;
INET_STATIC_ASSERT(rtmetrics_size, sizeof(InetRtMetrics) == 40);

typedef struct InetRtMsghdr {
    u16 rtm_msglen;                     /* 0x00 */
    u8 rtm_version;                     /* 0x02 */
    u8 rtm_type;                        /* 0x03 */
    u16 rtm_index;                      /* 0x04 */
    u16 rtm_pad;                        /* 0x06 */
    s32 rtm_flags;                      /* 0x08 */
    s32 rtm_addrs;                      /* 0x0C */
    s32 rtm_pid;                        /* 0x10 */
    s32 rtm_seq;                        /* 0x14 */
    s32 rtm_errno;                      /* 0x18 */
    s32 rtm_use;                        /* 0x1C */
    u32 rtm_inits;                      /* 0x20 */
    InetRtMetrics rtm_rmx;              /* 0x24..0x4B */
} InetRtMsghdr;
INET_STATIC_ASSERT(rtmsghdr_size, sizeof(InetRtMsghdr) == 76);

typedef struct InetIfAliasReq {
    char ifra_name[16];                 /* 0x00 */
    InetSockaddrIn ifra_addr;           /* 0x10 */
    InetSockaddrIn ifra_broadaddr;      /* 0x20 */
    InetSockaddrIn ifra_mask;           /* 0x30 */
} InetIfAliasReq;
INET_STATIC_ASSERT(ifaliasreq_size, sizeof(InetIfAliasReq) == 64);

typedef struct InetIfreq {
    char ifr_name[16];                  /* 0x00 */
    union {
        InetSockaddr ifru_addr;         /* 0x10 */
        InetSockaddrIn ifru_sin;        /* 0x10 */
        u16 ifru_flags;                 /* 0x10 */
        s32 ifru_metric;                /* 0x10 */
        s32 ifru_mtu;                   /* 0x10 */
        void *ifru_ptr;                 /* 0x10 */
        u32 ifru_words[4];              /* 0x10 */
    } ifr_u;
} InetIfreq;

typedef struct InetRouteEntryInfo {
    struct InetRouteEntryInfo *next;    /* 0x00 */
    u32 flags;                          /* 0x04 */
    u32 dst;                            /* 0x08 */
    u32 gateway;                        /* 0x0C */
    u32 mask;                           /* 0x10 */
    char ifname[16];                    /* 0x14..0x23 */
} InetRouteEntryInfo;
INET_STATIC_ASSERT(route_entry_info_size, sizeof(InetRouteEntryInfo) == 36);

typedef struct InetArpEntryInfo {
    struct InetArpEntryInfo *next;      /* 0x00 */
    u32 flags;                          /* 0x04 */
    u32 ip_addr;                        /* 0x08 */
    u8 mac_addr[6];                     /* 0x0C */
    u16 pad;                            /* 0x12 */
    char ifname[16];                    /* 0x14..0x23 */
} InetArpEntryInfo;
INET_STATIC_ASSERT(arp_entry_info_size, sizeof(InetArpEntryInfo) == 36);

typedef struct SceNetInetTcpcbstat {
    struct SceNetInetTcpcbstat *next;   /* 0x00 */
    u32 send_queue_len;                 /* 0x04 */
    u32 recv_queue_len;                 /* 0x08 */
    u32 laddr;                          /* 0x0C */
    u32 peer_addr;                      /* 0x10 */
    u16 lport;                          /* 0x14 */
    u16 peer_port;                      /* 0x16 */
    u16 state;                          /* 0x18 */
    u16 pad;                            /* 0x1A */
} SceNetInetTcpcbstat;
INET_STATIC_ASSERT(tcpcbstat_size, sizeof(SceNetInetTcpcbstat) == 28);

typedef struct SceNetInetUdpcbstat {
    struct SceNetInetUdpcbstat *next;   /* 0x00 */
    u32 send_queue_len;                 /* 0x04 */
    u32 recv_queue_len;                 /* 0x08 */
    u32 laddr;                          /* 0x0C */
    u32 peer_addr;                      /* 0x10 */
    u16 lport;                          /* 0x14 */
    u16 peer_port;                      /* 0x16 */
} SceNetInetUdpcbstat;
INET_STATIC_ASSERT(udpcbstat_size, sizeof(SceNetInetUdpcbstat) == 24);

typedef struct InetRadixMask {
    s16 rm_bit;                         /* 0x00 */
    u8 rm_unused;                       /* 0x02 */
    u8 rm_flags;                        /* 0x03 */
    struct InetRadixMask *rm_mklist;    /* 0x04 */
    union {
        u8 *rmu_mask;
        struct InetRadixNode *rmu_leaf;
    } rm_u;                             /* 0x08 */
    s32 rm_refs;                        /* 0x0C */
} InetRadixMask;
INET_STATIC_ASSERT(radix_mask_size, sizeof(InetRadixMask) == 16);

typedef struct InetRadixNode {
    InetRadixMask *rn_mklist;           /* 0x00 */
    struct InetRadixNode *rn_parent;    /* 0x04 */
    s16 rn_bit;                         /* 0x08 */
    u8 rn_bmask;                        /* 0x0A */
    u8 rn_flags;                        /* 0x0B */
    union {
        struct {
            u8 *rn_key;                 /* 0x0C */
            u8 *rn_mask;                /* 0x10 */
            struct InetRadixNode *rn_dupedkey; /* 0x14 */
        } rn_leaf;
        struct {
            s32 rn_offset;              /* 0x0C */
            struct InetRadixNode *rn_left;  /* 0x10 */
            struct InetRadixNode *rn_right; /* 0x14 */
        } rn_node;
    } rn_u;
} InetRadixNode;
INET_STATIC_ASSERT(radix_node_size, sizeof(InetRadixNode) == 24);

typedef struct InetRadixNodeHead {
    InetRadixNode *rnh_treetop;         /* 0x00 */
    s32 rnh_addrsize;                   /* 0x04 */
    s32 rnh_pktsize;                    /* 0x08 */
    InetRadixNode *(*rnh_addaddr)(void *, void *, struct InetRadixNodeHead *, InetRadixNode *); /* 0x0C */
    InetRadixNode *(*rnh_addpkt)(void *, void *, struct InetRadixNodeHead *, InetRadixNode *);  /* 0x10 */
    InetRadixNode *(*rnh_deladdr)(void *, void *, struct InetRadixNodeHead *);                  /* 0x14 */
    InetRadixNode *(*rnh_delpkt)(void *, void *, struct InetRadixNodeHead *);                   /* 0x18 */
    InetRadixNode *(*rnh_matchaddr)(void *, struct InetRadixNodeHead *);                        /* 0x1C */
    InetRadixNode *(*rnh_lookup)(void *, void *, struct InetRadixNodeHead *);                   /* 0x20 */
    InetRadixNode *(*rnh_matchpkt)(void *, struct InetRadixNodeHead *);                         /* 0x24 */
    int (*rnh_walktree)(struct InetRadixNodeHead *, int (*)(InetRadixNode *, void *), void *);  /* 0x28 */
    InetRadixNode rnh_nodes[3];         /* 0x2C..0x73 */
} InetRadixNodeHead;

struct InetRttimer;
struct InetRttimerQueue;

typedef struct InetRttimer {
    struct InetRttimer *rtt_next;       /* 0x00 */
    struct InetRttimer **rtt_prev;      /* 0x04 */
    struct InetRttimer *rtt_link_next;  /* 0x08 */
    struct InetRttimer **rtt_link_prev; /* 0x0C */
    struct InetRttimerQueue *rtt_queue; /* 0x10 */
    struct InetRtentry *rtt_rt;         /* 0x14 */
    void (*rtt_func)(struct InetRtentry *, struct InetRttimer *); /* 0x18 */
    u32 rtt_time;                       /* 0x1C */
} InetRttimer;
INET_STATIC_ASSERT(rttimer_size, sizeof(InetRttimer) == 32);

typedef struct InetRttimerQueue {
    s32 rtq_timeout;                    /* 0x00 */
    InetRttimer *rtq_head;              /* 0x04 */
    InetRttimer **rtq_tail;             /* 0x08 */
    struct InetRttimerQueue *rtq_next;  /* 0x0C */
    struct InetRttimerQueue **rtq_prev; /* 0x10 */
} InetRttimerQueue;
INET_STATIC_ASSERT(rttimer_queue_size, sizeof(InetRttimerQueue) == 20);

typedef struct InetRtentry {
    InetRadixNode rt_nodes[2];          /* 0x00..0x2F (48 bytes) */
    InetSockaddr *rt_gateway;           /* 0x30 (48) */
    s32 rt_flags;                       /* 0x34 (52) */
    s32 rt_refcnt;                      /* 0x38 (56) */
    u32 rt_use;                         /* 0x3C (60) */
    InetIfnet *rt_ifp;                  /* 0x40 (64) */
    InetIfaddr *rt_ifa;                 /* 0x44 (68) */
    InetSockaddr *rt_genmask;           /* 0x48 (72) */
    void *rt_llinfo;                    /* 0x4C (76) */
    InetRtMetrics rt_rmx;               /* 0x50..0x77 (80..119) */
    struct InetRtentry *rt_gwroute;     /* 0x78 (120) */
    InetRttimer *rt_timer;              /* 0x7C (124) */
    struct InetRtentry *rt_parent;      /* 0x80 (128) */
} InetRtentry;
INET_STATIC_ASSERT(rtentry_size, sizeof(InetRtentry) == 132);
INET_STATIC_ASSERT(rtentry_gw_off, offsetof(InetRtentry, rt_gateway) == 48);
INET_STATIC_ASSERT(rtentry_ifp_off, offsetof(InetRtentry, rt_ifp) == 64);
INET_STATIC_ASSERT(rtentry_gwroute_off, offsetof(InetRtentry, rt_gwroute) == 120);
INET_STATIC_ASSERT(rtentry_timer_off, offsetof(InetRtentry, rt_timer) == 124);
INET_STATIC_ASSERT(rtentry_parent_off, offsetof(InetRtentry, rt_parent) == 128);

typedef struct InetRoute {
    InetRtentry *ro_rt;                 /* 0x00 */
    InetSockaddr ro_dst;                /* 0x04..0x13 */
} InetRoute;
INET_STATIC_ASSERT(route_size, sizeof(InetRoute) == 20);

typedef struct InetRouteCb {
    s32 ip_count;
    s32 ip6_count;
    s32 ipx_count;
    s32 ns_count;
    s32 iso_count;
    s32 any_count;
} InetRouteCb;
INET_STATIC_ASSERT(routecb_size, sizeof(InetRouteCb) == 24);

typedef struct InetRtAddrinfo {
    s32 rti_addrs;                      /* 0x00 */
    InetSockaddr *rti_info[8];          /* 0x04..0x23 */
    s32 rti_flags;                      /* 0x24 */
    InetIfaddr *rti_ifa;                /* 0x28 */
    InetIfnet *rti_ifp;                 /* 0x2C */
    InetSockaddr *rti_netmask;          /* 0x30 */
} InetRtAddrinfo;

typedef struct InetRawcb {
    struct InetRawcb *rcb_next;         /* 0x00 */
    struct InetRawcb **rcb_prev;        /* 0x04 */
    InetSocket *rcb_socket;             /* 0x08 */
    InetSockaddr *rcb_faddr;            /* 0x0C */
    InetSockaddr *rcb_laddr;            /* 0x10 */
    u16 rcb_family;                     /* 0x14 */
    u16 rcb_protocol;                   /* 0x16 */
} InetRawcb;
INET_STATIC_ASSERT(rawcb_size, sizeof(InetRawcb) == 24);

/* Van Jacobson TCP/IP header compression state */
typedef struct InetSlCstate {
    struct InetSlCstate *cs_next;       /* 0x00 */
    u16 cs_hlen;                        /* 0x04 */
    u8 cs_id;                           /* 0x06 */
    u8 cs_filler;                       /* 0x07 */
    u8 cs_hdr[236];                     /* 0x08..0xF3 */
} InetSlCstate;
INET_STATIC_ASSERT(sl_cstate_size, sizeof(InetSlCstate) == 244);

typedef struct InetSlCompress {
    InetSlCstate *last_cs;              /* 0x00 */
    u8 last_recv;                       /* 0x04 */
    u8 last_xmit;                       /* 0x05 */
    u16 flags;                          /* 0x06 */
    u32 sls_packets;                    /* 0x08 */
    u32 sls_compressed;                 /* 0x0C */
    u32 sls_searches;                   /* 0x10 */
    u32 sls_misses;                     /* 0x14 */
    u32 sls_uncompressedin;             /* 0x18 */
    u32 sls_compressedin;               /* 0x1C */
    u32 sls_errorin;                    /* 0x20 */
    u32 sls_tossed;                     /* 0x24 */
    InetSlCstate tstate[16];            /* 0x28..0xF67 */
    InetSlCstate rstate[16];            /* 0xF68..0x1EA7 */
} InetSlCompress;
INET_STATIC_ASSERT(sl_compress_size, sizeof(InetSlCompress) == 7848);

/* ARP link-layer info */
typedef struct InetLlinfoArp {
    struct InetLlinfoArp *la_next;      /* 0x00 */
    struct InetLlinfoArp **la_prev;     /* 0x04 */
    InetRtentry *la_rt;                 /* 0x08 */
    InetMbuf *la_hold;                  /* 0x0C */
    s32 la_asked;                       /* 0x10 */
} InetLlinfoArp;
INET_STATIC_ASSERT(llinfo_arp_size, sizeof(InetLlinfoArp) == 20);

/* IPv4 header (20 bytes, packed for unaligned wire access) */
typedef struct __attribute__((packed)) InetIpHdr {
    u8 ip_vhl;                          /* 0x00 */
    u8 ip_tos;                          /* 0x01 */
    u16 ip_len;                         /* 0x02 */
    u16 ip_id;                          /* 0x04 */
    u16 ip_off;                         /* 0x06 */
    u8 ip_ttl;                          /* 0x08 */
    u8 ip_p;                            /* 0x09 */
    u16 ip_sum;                         /* 0x0A */
    u32 ip_src;                         /* 0x0C */
    u32 ip_dst;                         /* 0x10 */
} InetIpHdr;
INET_STATIC_ASSERT(ip_hdr_size, sizeof(InetIpHdr) == 20);

/* IPv4 interface address & multicast */
struct InetInIfaddr;

typedef struct InetInMulti {
    u32 inm_addr;                       /* 0x00 */
    char inm_ifname[16];                /* 0x04 */
    struct InetInIfaddr *inm_ia;        /* 0x14 */
    u32 inm_refcount;                   /* 0x18 */
    u32 inm_timer;                      /* 0x1C */
    struct InetInMulti *inm_next;       /* 0x20 */
    struct InetInMulti **inm_prev;      /* 0x24 */
    u32 inm_state;                      /* 0x28 */
    void *inm_rti;                      /* 0x2C */
} InetInMulti;
INET_STATIC_ASSERT(in_multi_size, sizeof(InetInMulti) == 48);

typedef struct InetInIfaddr {
    InetIfaddr ia_ifa;                  /* 0x00..0x37 (56) */
    u32 ia_net;                         /* 0x38 */
    u32 ia_netmask;                     /* 0x3C */
    u32 ia_subnet;                      /* 0x40 */
    u32 ia_subnetmask;                  /* 0x44 */
    u32 ia_netbroadcast;                /* 0x48 */
    struct InetInIfaddr *ia_hash_next;  /* 0x4C */
    struct InetInIfaddr **ia_hash_prev; /* 0x50 */
    struct InetInIfaddr *ia_list_next;  /* 0x54 */
    struct InetInIfaddr **ia_list_prev; /* 0x58 */
    InetSockaddrIn ia_addr;             /* 0x5C..0x6B (92) */
    InetSockaddrIn ia_dstaddr;          /* 0x6C..0x7B (108) */
    InetSockaddrIn ia_sockmask;         /* 0x7C..0x8B (124) */
    InetInMulti *ia_multiaddrs;         /* 0x8C (140) */
    InetInMulti *ia_allhosts;           /* 0x90 (144) */
    u32 ia_pad;                         /* 0x94 (148) */
} InetInIfaddr;
INET_STATIC_ASSERT(in_ifaddr_size, sizeof(InetInIfaddr) == 152);
INET_STATIC_ASSERT(in_ifaddr_addr_off, offsetof(InetInIfaddr, ia_addr) == 92);
#define ia_broadaddr ia_dstaddr
#define ia_next      ia_list_next

typedef struct InetIpMoptions {
    InetIfnet *imo_multicast_ifp;       /* 0x00 */
    u8 imo_multicast_ttl;               /* 0x04 */
    u8 imo_multicast_loop;              /* 0x05 */
    u16 imo_num_memberships;            /* 0x06 */
    InetInMulti *imo_membership[20];    /* 0x08..0x57 */
} InetIpMoptions;

/* ===================================================================== */
/* Inpcb / InpcbTable / Rijndael structures                              */
/* ===================================================================== */

struct InetInpcbTable;

/* Exact Inpcb layout verified from in_pcb.c & sceNetInetGetTcpcbstat/Udpcbstat:
 *   0x00 = inp_hash_next
 *   0x04 = inp_hash_prev
 *   0x08 = inp_queue_next
 *   0x0C = inp_queue_prev
 *   0x10 = inp_ppcb
 *   0x14 = inp_state (0=detached, 1=bound, 2=connected)
 *   0x18 = inp_fport (u16)
 *   0x1A = inp_lport (u16)
 *   0x1C = inp_socket
 *   0x20 = inp_route (20 bytes)
 *   0x34 = inp_flags
 *   0x38 = inp_ip (20 bytes; ip_src at 0x44, ip_dst at 0x48)
 *   0x4C = inp_options
 *   0x50 = inp_moptions
 *   0x54 = inp_errormtu
 *   0x58 = inp_table
 */
typedef struct InetInpcbReal {
    struct InetInpcbReal *inp_hash_next;   /* 0x00 (0) */
    struct InetInpcbReal **inp_hash_prev;  /* 0x04 (4) */
    struct InetInpcbReal *inp_queue_next;  /* 0x08 (8) */
    struct InetInpcbReal **inp_queue_prev; /* 0x0C (12) */
    void *inp_ppcb;                        /* 0x10 (16) */
    s32 inp_state;                         /* 0x14 (20) */
    u16 inp_fport;                         /* 0x18 (24) */
    u16 inp_lport;                         /* 0x1A (26) */
    InetSocket *inp_socket;                /* 0x1C (28) */
    InetRoute inp_route;                   /* 0x20..0x33 (32..51) */
    s32 inp_flags;                         /* 0x34 (52) */
    InetIpHdr inp_ip;                      /* 0x38..0x4B (56..75) */
    InetMbuf *inp_options;                 /* 0x4C (76) */
    InetIpMoptions *inp_moptions;          /* 0x50 (80) */
    s32 inp_errormtu;                      /* 0x54 (84) */
    struct InetInpcbTable *inp_table;      /* 0x58 (88) */
} InetInpcbReal;
INET_STATIC_ASSERT(inpcb_size, sizeof(InetInpcbReal) == 92);
INET_STATIC_ASSERT(inpcb_laddr_off, offsetof(InetInpcbReal, inp_ip.ip_src) == 68);
INET_STATIC_ASSERT(inpcb_faddr_off, offsetof(InetInpcbReal, inp_ip.ip_dst) == 72);
INET_STATIC_ASSERT(inpcb_table_off, offsetof(InetInpcbReal, inp_table) == 88);

#define inp_laddr inp_ip.ip_src
#define inp_faddr inp_ip.ip_dst

typedef struct InetInpcbTable {
    InetInpcbReal *inpt_queue_first;       /* 0x00 */
    InetInpcbReal *inpt_queue_last;        /* 0x04 */
    InetInpcbReal **inpt_bindhashtbl;      /* 0x08 */
    InetInpcbReal **inpt_connecthashtbl;   /* 0x0C */
    u32 inpt_bindhash;                     /* 0x10 */
    u32 inpt_connecthash;                  /* 0x14 */
    u16 inpt_lastport;                     /* 0x18 */
    u16 inpt_lastlow;                      /* 0x1A */
} InetInpcbTable;
INET_STATIC_ASSERT(inpcb_table_size, sizeof(InetInpcbTable) == 28);

/* IPv4 statistics (232 bytes = 29 x u64 at 0x363E8) */
typedef struct InetIpStat {
    u64 ips_total;
    u64 ips_badsum;
    u64 ips_tooshort;
    u64 ips_toosmall;
    u64 ips_badhlen;
    u64 ips_badlen;
    u64 ips_fragments;
    u64 ips_fragdropped;
    u64 ips_fragtimeout;
    u64 ips_forward;
    u64 ips_fastforward;
    u64 ips_cantforward;
    u64 ips_redirectsent;
    u64 ips_noproto;
    u64 ips_delivered;
    u64 ips_localout;
    u64 ips_odropped;
    u64 ips_reassembled;
    u64 ips_fragmented;
    u64 ips_ofragments;
    u64 ips_cantfrag;
    u64 ips_badoptions;
    u64 ips_noroute;
    u64 ips_badvers;
    u64 ips_rawout;
    u64 ips_badfrags;
    u64 ips_fragdropped_nomem;
    u64 ips_toolong;
    u64 ips_nogif;
} InetIpStat;
INET_STATIC_ASSERT(ipstat_size, sizeof(InetIpStat) == 232);

/* ICMP header wire layout (RFC 792) */
typedef struct InetIcmpHdr {
    u8 icmp_type;
    u8 icmp_code;
    u16 icmp_cksum;
    union {
        u32 ih_gwaddr;
        struct {
            u16 icd_id;
            u16 icd_seq;
        } ih_idseq;
        struct {
            u16 ipm_void;
            u16 ipm_nextmtu;
        } ih_pmtu;
        u8 ih_pptr;
        u32 ih_void;
    } icmp_hun;
    union {
        struct {
            u32 its_otime;
            u32 its_rtime;
            u32 its_ttime;
        } id_ts;
        InetIpHdr id_ip;
        u32 id_mask;
    } icmp_dun;
} InetIcmpHdr;

/* Sony P2P peer key table entry (48 bytes) */
typedef struct InetP2pKeyEntry {
    struct InetP2pKeyEntry *next;
    struct InetP2pKeyEntry **prev;
    u16 sin_len_family;
    u16 peer_port;
    u32 peer_addr;
    u8 sin_zero[8];
    u8 aes_key[16];
    u16 lport;
    u16 pad_align;
    s32 refcnt;
} InetP2pKeyEntry;
INET_STATIC_ASSERT(p2p_key_entry_size, sizeof(InetP2pKeyEntry) == 48);

/* TCP header wire layout (20 bytes) */
typedef struct InetTcpHdr {
    u16 th_sport;
    u16 th_dport;
    u32 th_seq;
    u32 th_ack;
    u8 th_offx2;
    u8 th_flags;
    u16 th_win;
    u16 th_sum;
    u16 th_urp;
} InetTcpHdr;
INET_STATIC_ASSERT(tcp_hdr_size, sizeof(InetTcpHdr) == 20);

/* TCP reassembly queue entry (40 bytes) */
typedef struct InetTcpqent {
    struct InetTcpqent *tqe_q_next;
    struct InetTcpqent **tqe_q_prev;
    u32 tqe_unused_a;
    InetMbuf *tqe_m;
    u32 tqe_unused_b;
    struct InetTcpqent *tqe_timeq_next;
    struct InetTcpqent **tqe_timeq_prev;
    u32 tqe_seq;
    u32 tqe_len;
    u32 tqe_flags;
} InetTcpqent;
INET_STATIC_ASSERT(tcpqent_size, sizeof(InetTcpqent) == 40);

/* TCP statistics (544 bytes = 68 x u64 at 0x36810) */
typedef struct InetTcpStat {
    u64 tcps_connattempt;
    u64 tcps_accepts;
    u64 tcps_connects;
    u64 tcps_drops;
    u64 tcps_conndrops;
    u64 tcps_closed;
    u64 tcps_segstimed;
    u64 tcps_rttupdated;
    u64 tcps_delack;
    u64 tcps_timeoutdrop;
    u64 tcps_rexmttimeo;
    u64 tcps_persisttimeo;
    u64 tcps_keeptimeo;
    u64 tcps_keepprobe;
    u64 tcps_keepdrops;
    u64 tcps_persistdrop;
    u64 tcps_connsdrained;
    u64 tcps_pmtublackhole;
    u64 tcps_sndtotal;
    u64 tcps_sndpack;
    u64 tcps_sndbyte;
    u64 tcps_sndrexmitpack;
    u64 tcps_sndrexmitbyte;
    u64 tcps_sndacks;
    u64 tcps_sndprobe;
    u64 tcps_sndurg;
    u64 tcps_sndwinup;
    u64 tcps_sndctrl;
    u64 tcps_rcvtotal;
    u64 tcps_rcvpack;
    u64 tcps_rcvbyte;
    u64 tcps_rcvbadsum;
    u64 tcps_rcvbadoff;
    u64 tcps_rcvmemdrop;
    u64 tcps_rcvshort;
    u64 tcps_rcvduppack;
    u64 tcps_rcvdupbyte;
    u64 tcps_rcvpartduppack;
    u64 tcps_rcvpartdupbyte;
    u64 tcps_rcvoopack;
    u64 tcps_rcvoobyte;
    u64 tcps_rcvpackafterwin;
    u64 tcps_rcvbyteafterwin;
    u64 tcps_rcvafterclose;
    u64 tcps_rcvwinprobe;
    u64 tcps_rcvdupack;
    u64 tcps_rcvacktoomuch;
    u64 tcps_rcvackpack;
    u64 tcps_rcvackbyte;
    u64 tcps_rcvwinupd;
    u64 tcps_pawsdrop;
    u64 tcps_predack;
    u64 tcps_preddat;
    u64 tcps_pcbhashmiss;
    u64 tcps_noport;
    u64 tcps_badsyn;
    u64 tcps_reass_drained;
    u64 tcps_sc_added;
    u64 tcps_sc_completed;
    u64 tcps_sc_timed_out;
    u64 tcps_sc_overflowed;
    u64 tcps_sc_reset;
    u64 tcps_sc_unreach;
    u64 tcps_sc_bucketoverflow;
    u64 tcps_sc_aborted;
    u64 tcps_sc_dupesyn;
    u64 tcps_sc_dropped;
    u64 tcps_sc_retransmitted;
} InetTcpStat;
INET_STATIC_ASSERT(tcpstat_size, sizeof(InetTcpStat) == 544);

struct InetSynCache;

/* TCP control block (188 bytes) */
typedef struct InetTcpcb {
    s32 t_family;                           /* 0x00 (0) */
    InetTcpqent *segq_first;                /* 0x04 (4) */
    s32 t_timer[4];                         /* 0x08..0x17 (8..23) */
    s16 t_state;                            /* 0x18 (24) */
    s16 t_rxtshift;                         /* 0x1A (26) */
    s16 t_rxtcur;                           /* 0x1C (28) */
    s16 t_dupacks;                          /* 0x1E (30) */
    u16 t_peermss;                          /* 0x20 (32) */
    u16 t_ourmss;                           /* 0x22 (34) */
    u16 t_maxseg_opt;                       /* 0x24 (36) */
    u16 t_segsz;                            /* 0x26 (38) */
    s32 t_force;                            /* 0x28 (40) */
    u32 t_flags;                            /* 0x2C (44) */
    InetMbuf *t_template;                   /* 0x30 (48) */
    InetInpcbReal *t_inpcb;                 /* 0x34 (52) */
    void *t_in6pcb;                         /* 0x38 (56) */
    struct InetTcpcb *t_delack_next;        /* 0x3C (60) */
    struct InetTcpcb **t_delack_prev;       /* 0x40 (64) */
    u32 snd_una;                            /* 0x44 (68) */
    u32 snd_nxt;                            /* 0x48 (72) */
    u32 snd_up;                             /* 0x4C (76) */
    u32 snd_wl_seq;                         /* 0x50 (80) */
    u32 snd_wl_ack;                         /* 0x54 (84) */
    u32 iss;                                /* 0x58 (88) */
    u32 snd_wnd;                            /* 0x5C (92) */
    u32 snd_recover;                        /* 0x60 (96) */
    u32 rcv_wnd;                            /* 0x64 (100) */
    u32 rcv_nxt;                            /* 0x68 (104) */
    u32 rcv_up;                             /* 0x6C (108) */
    u32 irs;                                /* 0x70 (112) */
    u32 rcv_adv;                            /* 0x74 (116) */
    u32 snd_max;                            /* 0x78 (120) */
    u32 snd_cwnd;                           /* 0x7C (124) */
    u32 snd_ssthresh;                       /* 0x80 (128) */
    s16 t_idle;                             /* 0x84 (132) */
    s16 t_rtt;                              /* 0x86 (134) */
    u32 t_rtseq;                            /* 0x88 (136) */
    s16 t_srtt;                             /* 0x8C (140) */
    s16 t_rttvar;                           /* 0x8E (142) */
    u16 t_rttmin;                           /* 0x90 (144) */
    u16 t_pad_rtt;                          /* 0x92 (146) */
    u32 max_sndwnd;                         /* 0x94 (148) */
    u8 t_oobflags;                          /* 0x98 (152) */
    u8 t_iobc;                              /* 0x99 (153) */
    s16 t_softerror;                        /* 0x9A (154) */
    u8 snd_scale;                           /* 0x9C (156) */
    u8 rcv_scale;                           /* 0x9D (157) */
    u8 request_r_scale;                     /* 0x9E (158) */
    u8 requested_s_scale;                   /* 0x9F (159) */
    u32 ts_recent;                          /* 0xA0 (160) */
    u32 ts_recent_age;                      /* 0xA4 (164) */
    u32 last_ack_sent;                      /* 0xA8 (168) */
    InetTcpqent *t_segq_time_first;         /* 0xAC (172) */
    s32 t_mtudisc;                          /* 0xB0 (176) */
    struct InetSynCache *t_sc_head;         /* 0xB4 (180) */
    u16 t_p2p_lport;                        /* 0xB8 (184) */
    u16 t_p2p_fport;                        /* 0xBA (186) */
} InetTcpcb;
INET_STATIC_ASSERT(tcpcb_size, sizeof(InetTcpcb) == 188);
INET_STATIC_ASSERT(tcpcb_inpcb_off, offsetof(InetTcpcb, t_inpcb) == 52);
INET_STATIC_ASSERT(tcpcb_p2p_off, offsetof(InetTcpcb, t_p2p_lport) == 184);

/* TCP SYN cache entry (136 bytes) */
typedef struct InetSynCache {
    struct InetSynCache *sc_bucketq_next;   /* 0x00 (0) */
    struct InetSynCache **sc_bucketq_prev;  /* 0x04 (4) */
    struct InetSynCache *sc_timerq_next;    /* 0x08 (8) */
    struct InetSynCache **sc_timerq_prev;   /* 0x0C (12) */
    InetRoute sc_route;                     /* 0x10..0x23 (16..35) */
    s32 sc_win;                             /* 0x24 (36) */
    s32 sc_bucketidx;                       /* 0x28 (40) */
    u32 sc_hash;                            /* 0x2C (44) */
    u32 sc_timestamp;                       /* 0x30 (48) */
    InetSockaddrIn sc_src;                  /* 0x34..0x43 (52..67) */
    InetSockaddrIn sc_dst;                  /* 0x44..0x53 (68..83) */
    u32 sc_irs;                             /* 0x54 (84) */
    u32 sc_iss;                             /* 0x58 (88) */
    u32 sc_rxtcur;                          /* 0x5C (92) */
    u32 sc_rxt_delta;                       /* 0x60 (96) */
    u32 sc_rxttot;                          /* 0x64 (100) */
    u16 sc_rxtshift;                        /* 0x68 (104) */
    u16 sc_flags;                           /* 0x6A (106) */
    InetMbuf *sc_ipopts;                    /* 0x6C (108) */
    u16 sc_peermss;                         /* 0x70 (112) */
    u16 sc_ourmss;                          /* 0x72 (114) */
    u8 sc_scale;                            /* 0x74 (116) */
    u8 sc_pad[3];                           /* 0x75..0x77 */
    InetTcpcb *sc_tp;                       /* 0x78 (120) */
    struct InetSynCache *sc_tpq_next;       /* 0x7C (124) */
    struct InetSynCache **sc_tpq_prev;      /* 0x80 (128) */
    u16 sc_p2p_port;                        /* 0x84 (132) */
    u16 sc_pad_end;                         /* 0x86 (134) */
} InetSynCache;
INET_STATIC_ASSERT(syncache_size, sizeof(InetSynCache) == 136);

/* SYN cache hash bucket (8 bytes) */
typedef struct InetSynCacheHead {
    InetSynCache *sch_first;
    u16 sch_length;
    u16 sch_pad;
} InetSynCacheHead;
INET_STATIC_ASSERT(syncache_head_size, sizeof(InetSynCacheHead) == 8);

/* Synchronous PPP interface state (9104 bytes) */
typedef struct InetSppp {
    InetIfnet pp_if;                        /* 0x0000..0x011F (288 bytes) */
    u32 pp_if_tail[2];                      /* 0x0120..0x0127 (8 bytes) */
    InetIfQueue pp_fastq;                   /* 0x0128..0x013B (20 bytes) */
    u32 pp_flags;                           /* 0x013C (316) */
    u32 pp_framebytes;                      /* 0x0140 (320) */
    u16 pp_alivecnt;                        /* 0x0144 (324) */
    u16 pp_loopcnt;                         /* 0x0146 (326) */
    u32 pp_seq[4];                          /* 0x0148..0x0157 (328) */
    u32 pp_rseq[4];                         /* 0x0158..0x0167 (344) */
    u64 pp_saved_mtu;                       /* 0x0168..0x016F (360) */
    s32 pp_phase;                           /* 0x0170 (368) */
    s32 pp_max_phase;                       /* 0x0174 (372) */
    u32 pp_dns_main;                        /* 0x0178 (376) */
    u32 pp_dns_alt;                         /* 0x017C (380) */
    u32 state[4];                           /* 0x0180..0x018F (384) */
    u8 confid[4];                           /* 0x0190..0x0193 (400) */
    s32 rst_counter[4];                     /* 0x0194..0x01A3 (404) */
    s32 fail_counter[4];                    /* 0x01A4..0x01B3 (420) */
    InetCallout ch[4][1];                   /* 0x01B4..0x0243 (436..579, 144 bytes) */
    u32 lcp_opts;                           /* 0x0244 (580) */
    u32 lcp_magic;                          /* 0x0248 (584) */
    u32 lcp_mru;                            /* 0x024C (588) */
    u32 lcp_their_mru;                      /* 0x0250 (592) */
    u32 lcp_protos;                         /* 0x0254 (596) */
    u32 lcp_echoid;                         /* 0x0258 (600) */
    u32 lcp_timeout;                        /* 0x025C (604) */
    u32 lcp_max_terminate;                  /* 0x0260 (608) */
    u32 lcp_max_configure;                  /* 0x0264 (612) */
    u32 lcp_max_failure;                    /* 0x0268 (616) */
    u32 lcp_my_accm;                        /* 0x026C (620) */
    u32 lcp_their_accm;                     /* 0x0270 (624) */
    u32 lcp_my_pcomp;                       /* 0x0274 (628) */
    u32 lcp_their_pcomp;                    /* 0x0278 (632) */
    u32 lcp_my_accomp;                      /* 0x027C (636) */
    u32 lcp_their_accomp;                   /* 0x0280 (640) */
    u32 lcp_flags_cfg;                      /* 0x0284 (644) */
    u32 ipcp_opts;                          /* 0x0288 (648) */
    u32 ipcp_flags;                         /* 0x028C (652) */
    u32 ipcp_myaddr;                        /* 0x0290 (656) */
    u32 ipcp_hisaddr;                       /* 0x0294 (660) */
    u32 ipcp_my_vj;                         /* 0x0298 (664) */
    u32 ipcp_his_vj;                        /* 0x029C (668) */
    u32 ipcp_cfg_flags;                     /* 0x02A0 (672) */
    InetSlCompress slcomp;                  /* 0x02A4..0x214B (676..8523, 7848 bytes) */
    u16 myauth_proto;                       /* 0x214C (8524) */
    u16 myauth_pad;                         /* 0x214E (8526) */
    char myauth_name[128];                  /* 0x2150..0x21CF (8528) */
    char myauth_secret[128];                /* 0x21D0..0x224F (8656) */
    u8 myauth_name_len;                     /* 0x2250 (8784) */
    u8 myauth_secret_len;                   /* 0x2251 (8785) */
    u8 myauth_reserved[18];                 /* 0x2252..0x2263 (8786..8803) */
    u32 auth_timeout;                       /* 0x2264 (8804) */
    u32 auth_max_retry;                     /* 0x2268 (8808) */
    u8 chap_id;                             /* 0x226C (8812) */
    u8 chap_challenge[255];                 /* 0x226D..0x236B (8813..9067) */
    u32 chap_challenge_len;                 /* 0x236C (9068) */
    void (*pp_up)(struct InetSppp *);       /* 0x2370 (9072) */
    void (*pp_down)(struct InetSppp *);     /* 0x2374 (9076) */
    void (*pp_tls)(struct InetSppp *);      /* 0x2378 (9080) */
    void (*pp_tlf)(struct InetSppp *);      /* 0x237C (9084) */
    void (*pp_con)(struct InetSppp *);      /* 0x2380 (9088) */
    void (*pp_chg)(struct InetSppp *, int); /* 0x2384 (9092) */
    void (*pp_lcp_up)(struct InetSppp *);   /* 0x2388 (9096) */
    u32 pp_tail_pad;                        /* 0x238C (9100) */
} InetSppp;
INET_STATIC_ASSERT(sppp_size, sizeof(InetSppp) == 9104);
INET_STATIC_ASSERT(sppp_flags_off, offsetof(InetSppp, pp_flags) == 316);
INET_STATIC_ASSERT(sppp_secret_off, offsetof(InetSppp, myauth_secret) == 8656);
INET_STATIC_ASSERT(sppp_namelen_off, offsetof(InetSppp, myauth_name_len) == 8784);
INET_STATIC_ASSERT(sppp_authtimeout_off, offsetof(InetSppp, auth_timeout) == 8804);
INET_STATIC_ASSERT(sppp_chapid_off, offsetof(InetSppp, chap_id) == 8812);
INET_STATIC_ASSERT(sppp_tls_off, offsetof(InetSppp, pp_tls) == 9080);

/* PPPoE softc (9456 bytes = 0x24F0) */
typedef struct InetPppoeSoftc {
    InetSppp sc_sppp;                       /* 0x0000..0x238F (9104 bytes) */
    InetIfnet *sc_eth_if;                   /* 0x2390 (9104) */
    s32 sc_state;                           /* 0x2394 (9108) */
    u8 sc_dest[6];                          /* 0x2398..0x239D (9112) */
    u16 sc_session;                         /* 0x239E (9118) */
    char *sc_service_name;                  /* 0x23A0 (9120) */
    char *sc_concentrator_name;             /* 0x23A4 (9124) */
    u8 *sc_ac_cookie;                       /* 0x23A8 (9128) */
    u32 sc_ac_cookie_len;                   /* 0x23AC (9132) */
    InetCallout sc_timeout[1];              /* 0x23B0..0x23D3 (9136, 36 bytes) */
    u32 sc_padi_retried;                    /* 0x23D4 (9172) */
    u32 sc_padr_retried;                    /* 0x23D8 (9176) */
    s32 sc_last_stage;                      /* 0x23DC (9180) */
    u32 sc_last_error;                      /* 0x23E0 (9184) */
    char sc_last_errmsg[256];               /* 0x23E4..0x24E3 (9188) */
    u8 *sc_relay_sid;                       /* 0x24E4 (9444) */
    u32 sc_relay_sid_len;                   /* 0x24E8 (9448) */
    u32 sc_pad_end;                         /* 0x24EC (9452) */
} InetPppoeSoftc;
INET_STATIC_ASSERT(pppoe_softc_size, sizeof(InetPppoeSoftc) == 9456);
INET_STATIC_ASSERT(pppoe_eth_off, offsetof(InetPppoeSoftc, sc_eth_if) == 9104);
INET_STATIC_ASSERT(pppoe_relay_off, offsetof(InetPppoeSoftc, sc_relay_sid) == 9444);

/* Rijndael context (972 bytes) */
typedef struct RijndaelCtx {
    u32 enc_keys[120];                  /* 0x000..0x1DF (480 bytes) */
    u32 dec_keys[120];                  /* 0x1E0..0x3BF (480 bytes) */
    s32 nk;                             /* 0x3C0 (960) */
    s32 nb;                             /* 0x3C4 (964) */
    s32 nr;                             /* 0x3C8 (968) */
} RijndaelCtx;
INET_STATIC_ASSERT(rijndael_ctx_size, sizeof(RijndaelCtx) == 972);

typedef struct RijndaelTables {
    u32 te0[256];                       /* 0x000..0x3FF (1024 bytes) */
    u32 td0[256];                       /* 0x400..0x7FF (1024 bytes) */
    u8 inv_sbox[256];                   /* 0x800..0x8FF (256 bytes) */
    u8 gf_log[256];                     /* 0x900..0x9FF (256 bytes) */
    u8 gf_alog[256];                    /* 0xA00..0xAFF (256 bytes) */
} RijndaelTables;
INET_STATIC_ASSERT(rijndael_tables_size, sizeof(RijndaelTables) == 2816);

/* ===================================================================== */
/* All 109 imported PRX functions                                        */
/* ===================================================================== */

/* Kernel_Library */
extern void *sceKernelMemset(void *dst, int ch, SceSize len);
extern int sceKernelCheckThreadStack(void);
#define sceKernelCheckThreadStack sceKernelCheckThreadStack
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize len);
extern SceUID sceKernelGetThreadId(void);

/* ThreadManForUser */
extern int sceKernelSignalSema(SceUID semaid, int count);
extern s64 sceKernelGetSystemTimeWide(void);

/* UtilsForUser */
extern int sceKernelUtilsMd5BlockInit(void *ctx);
extern int sceKernelUtilsMd5BlockUpdate(void *ctx, const void *data, u32 len);
extern int sceKernelUtilsMd5BlockResult(void *ctx, u8 *digest);

/* sceNetIfhandle_lib */
extern void sceNetIfhandle_lib_1560F143(InetMbuf *m, int off, int len, const void *cp);
extern void *sceNetIfhandle_lib_15CFE3C0(u32 size);
extern void sceNetIfhandle_lib_3E8DD3F8(InetMbuf *m, InetMbuf *n);
extern InetMbuf *sceNetIfhandle_lib_456E3146(InetMbuf *m, int off, int len, int wait);
extern InetMbuf *sceNetIfhandle_lib_49EDBB18(InetMbuf *n, int len);
extern InetMbuf *sceNetIfhandle_lib_4CF15C43(int how, u16 type);
extern InetMbuf *sceNetIfhandle_lib_59F0D619(int how, u16 type);
extern int sceNetIfhandle_lib_5FB31C72(u32 *outA, u32 *outB, u32 *outC, u32 *outD);
extern int sceNetIfhandle_lib_62B20015(u32 cfgA, u32 cfgB, u32 cfgC, u32 cfgD);
extern void sceNetIfhandle_lib_76BAD213(void *ptr);
extern void sceNetIfhandle_lib_9A6261EC(InetMbuf *m, int off, int len, void *cp);
extern InetMbuf *sceNetIfhandle_lib_A493AA5F(int how, u16 type);
extern void sceNetIfhandle_lib_BFF3CEA5(InetMbuf *m, int len);
extern InetMbuf *sceNetIfhandle_lib_C3325FDC(InetMbuf *m, int len, int how);
extern InetMbuf *sceNetIfhandle_lib_E80F00A4(InetMbuf *m, int off, int len, int *offp);
extern InetMbuf *sceNetIfhandle_lib_E9BF5332(InetMbuf *m, int off, int len, int wait);
extern void sceNetIfhandle_lib_F56FAC82(InetMbuf *m);
extern InetMbuf *sceNetIfhandle_lib_F8825DC4(InetMbuf *m);

/* sceNet_lib */
extern int sceNet_lib_05D525E4(int bit);
extern int sceNet_lib_13672F83(void (*ip_fn)(void), void (*arp_fn)(void));
extern int sceNet_lib_13A8B98A(void *so, u32 cmd, void *data);
extern u32 sceNet_lib_1858883D(void);
extern InetIfnet *sceNet_lib_1D10419C(const char *name);
extern int sceNet_lib_205E8D17(void *wchan, u32 ticks);
extern void sceNet_lib_25CC373A(InetCallout *co);
extern int sceNet_lib_2B42872F(void *cp, int len, InetUio *uio);
extern void *sceNet_lib_2F305274(const void *src, void *dst, int len);
extern int sceNet_lib_31F3CDA1(void);
extern int sceNet_lib_3A0668D5(void *ctx, const void *buf, u32 len);
extern int sceNet_lib_41621EB0(void);
extern int sceNet_lib_435843CB(void);
extern InetIfnet *sceNet_lib_45945E8D(int ifIndex);
extern void *sceNet_lib_4753D878(void *dst, const void *src, int len);
extern int sceNet_lib_4A3E75C1(u8 *digest, void *ctx);
extern u64 sceNet_lib_4B2B3416(InetTimeval *tv);
extern int sceNet_lib_4BF83CF4(const void *lhs, const void *rhs, int len);
extern int sceNet_lib_4C8FD452(void);
extern int sceNet_lib_515B2F33(int level);
extern int sceNet_lib_51C209B2(void *so);
extern int sceNet_lib_5505D820(void);
extern int sceNet_lib_572AD6ED(void (*fn)(void));
extern int sceNet_lib_5C7C7381(void);
extern int sceNet_lib_5ED457BE(void *out_cb, void *pre_cb, void *in_cb, void *mc_cb);
extern int sceNet_lib_701DDDC3(InetIfnet *ifp);
extern int sceNet_lib_710BD467(const char *name);
extern int sceNet_lib_750F705D(int ch);
extern int sceNet_lib_757085B0(InetThreadInfo *ti, int err);
extern int sceNet_lib_7574FDA1(void *wchan);
extern u32 sceNet_lib_75D9985C(void);
extern void sceNet_lib_7C3B86C5(int bit);
extern void sceNet_lib_7C86FBA4(InetCallout *co, u32 ticks, void (*fn)(void *), void *arg);
extern char *sceNet_lib_80C9F02A(char *dst, const char *src);
extern int sceNet_lib_87DC7A7E(void);
extern void *sceNet_lib_8B615F52(void *dst, int len);
extern int sceNet_lib_8C55B410(void);
extern int sceNet_lib_8D34E686(void *ctx, const void *key, int keyLen);
extern void sceNet_lib_92633D8D(InetThreadInfo *ti, SceUID *selThid);
extern int sceNet_lib_94B44F26(void);
extern int sceNet_lib_94DCA9F0(const char *lhs, const char *rhs, int len);
extern int sceNet_lib_976AB1E9(void *rtreq, void *init_cb, void *up_cb, void *down_cb,
                               void *purge_cb, void *delrt_cb, void *ann_cb, void *down2_cb,
                               void *ifmsg_cb, void *ifmsg2_cb, void *zero_cb, void *chk_cb,
                               void *ctl_cb);
extern int sceNet_lib_991FF86D(void (*cb)(InetIfnet *));
extern int sceNet_lib_A93A93E9(InetCallout *co);
extern int sceNet_lib_B51452D2(void *att, void *det, void *out_cb, void *in_cb, void *ioctl_cb, void *deq_cb);
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, int len);
extern void *sceNet_lib_B68E1EEA(void);
extern void sceNet_lib_B9C780C7(SceUID *selThid);
extern int sceNet_lib_C2EC2EEA(s32 *indexOut, InetIfnet ***ifindexTblOut, InetIfaddr ***addrsOut, InetIfnet **headOut);
extern void *sceNet_lib_C4261339(int elements, u32 *hashmask);
extern InetThreadInfo *sceNet_lib_CA3CF5EB(int *errp);
extern int sceNet_lib_CF705E46(char *buf, const char *fmt, ...);
extern int sceNet_lib_D5A03BC0(InetIfnet *ifp);
extern int sceNet_lib_D861EF33(int err);
extern int sceNet_lib_D8722983(const char *str);
extern int sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, int len);
extern int sceNet_lib_E155112D(void (*preCb)(InetThreadInfo *), void (*postCb)(InetSocket *));
extern int sceNet_lib_E9D457B6(InetIfnet *ifp, u32 cmd, void *reqPtr);
extern void sceNet_lib_EB6DE71A(int req, void *rt, void *rtinfo);
extern void sceNet_lib_EDB11CB4(InetIfnet *ifp);
extern void sceNet_lib_EDCC871E(void *ptr);
extern int sceNet_lib_F6DB0A0B(void *wchan, u32 usec);
extern int sceNet_lib_F6FE6D8E(void);
extern void sceNet_lib_FA6DE6A6(InetIfnet *ifp, int unused);

/* sceWlanDrv_lib */
extern int sceWlanDrv_lib_107E61D0(s32 kermitId, const void *addr, u32 addrLen);
extern int sceWlanDrv_lib_19602A86(int family, int sockType, int protocol, s32 *outId);
extern int sceWlanDrv_lib_327C9FCE(s32 kermitId, void *addr, u32 *addrLen);
extern int sceWlanDrv_lib_3BE42BCF(void);
extern int sceWlanDrv_lib_51FF0D5F(s32 kermitId, u32 cmd, void *buf);
extern int sceWlanDrv_lib_56AE09D2(s32 kermitId, u32 flags);
extern int sceWlanDrv_lib_5CFFAAB1(s32 kermitId, const void *addr, u32 addrLen);
extern int sceWlanDrv_lib_6417F8E3(s32 kermitId, void *addr, u32 *addrLen);
extern int sceWlanDrv_lib_78C84FEC(void);
extern int sceWlanDrv_lib_7EED1723(s32 kermitId);
extern int sceWlanDrv_lib_91E04A18(s32 kermitId, u32 how);
extern int sceWlanDrv_lib_9506F322(s32 kermitId, void *buf, s32 len, u32 flags, void *addr, u32 *addrLen, s32 *outLen);
extern int sceWlanDrv_lib_9ED787C8(s32 kermitId, int level, int optname, const void *optval, u32 optlen);
extern int sceWlanDrv_lib_A2DA9314(s32 kermitId, s32 backlog);
extern int sceWlanDrv_lib_BFE46C6D(s32 kermitId, const InetIovec *iov, u32 iovCnt, u32 flags, const void *name, u32 nameLen, s32 *outSent);
extern int sceWlanDrv_lib_C934D53F(s32 kermitId, const void *buf, s32 len, u32 flags, const void *addr, u32 addrLen, s32 *outSent);
extern int sceWlanDrv_lib_E39F7F88(s32 kermitId, int level, int optname, void *optval, u32 *optlen);
extern int sceWlanDrv_lib_F7713C18(s32 kermitId, void *addr, u32 *addrLen, s32 *outNewId);

/* ===================================================================== */
/* Exported SceNetInet / SceNetInet_lib declarations                     */
/* ===================================================================== */

extern int sceNetInetInit(void);
extern int sceNetInetTerm(void);
extern int sceNetInetGetErrno(void);
extern u32 sceNetInetGetPspError(void);
extern int sceNetInetGetTcpcbstat(s32 *bufLen, SceNetInetTcpcbstat *buf);
extern int sceNetInetGetUdpcbstat(s32 *bufLen, SceNetInetUdpcbstat *buf);
extern int sceNetInetSocket(int domain, int type, int protocol);
extern int sceNetInetBind(int sock, const void *addr, u32 addrLen);
extern int sceNetInetListen(int sock, s32 backlog);
extern int sceNetInetAccept(int sock, void *addr, u32 *addrLen);
extern int sceNetInetConnect(int sock, const void *addr, u32 addrLen);
extern int sceNetInetSend(int sock, const void *buf, s32 len, u32 flags);
extern int sceNetInetSendto(int sock, const void *buf, s32 len, u32 flags, const void *to, u32 toLen);
extern int sceNetInetSendmsg(int sock, const InetMsghdr *msg, u32 flags);
extern int sceNetInetRecv(int sock, void *buf, s32 len, u32 flags);
extern int sceNetInetRecvfrom(int sock, void *buf, s32 len, u32 flags, void *from, u32 *fromLen);
extern int sceNetInetRecvmsg(int sock, InetMsghdr *msg, u32 flags);
extern int sceNetInetSetsockopt(int sock, int level, int optname, const void *optval, u32 optlen);
extern int sceNetInetGetsockopt(int sock, int level, int optname, void *optval, u32 *optlen);
extern int sceNetInetGetsockname(int sock, void *name, u32 *nameLen);
extern int sceNetInetGetpeername(int sock, void *name, u32 *nameLen);
extern int sceNetInetShutdown(int sock, u32 how);
extern int sceNetInetClose(int sock);
extern int sceNetInetCloseWithRST(int sock);
extern int sceNetInetSocketAbort(int sock);
extern int sceNetInetSelect(s32 maxSock, void *readSet, void *writeSet, void *exceptSet, const InetTimeval *timeout);
extern int sceNetInetPoll(InetPollfd *fds, s32 nfds, s32 timeoutMs);
extern const char *sceNetInetInetNtop(int af, const void *src, char *dst, u32 size);
extern int sceNetInetInetPton(int af, const char *src, void *dst);
extern u32 sceNetInetInetAddr(const char *cp);
extern int sceNetInetInetAton(const char *cp, void *inp);
extern int sceNetInet_CCC18C45(const u8 *macAddr, const u32 *ipAddr);
extern int sceNetInet_2D5868C0(int unusedArg, const u32 *ipAddr);
extern int sceNetInet_1D023504(s32 bufSize, InetArpEntryInfo *outBuf);

extern int sceNetInet_lib_0238B6DF(RijndaelCtx *ctx, int blockBits, int keyBits, const u8 *key);
extern int sceNetInet_lib_D6598787(const RijndaelCtx *ctx, const void *src, void *dst);
extern int sceNetInet_lib_D609AD36(const RijndaelCtx *ctx, const void *src, void *dst);
extern int sceNetInet_lib_4F68DB0E(const char *ifname, const u32 *addr, const u32 *dstAddr, const u32 *mask);
extern int sceNetInet_lib_6A046357(const char *ifname);
extern int sceNetInet_lib_5155EC8A(const char *ifname, u32 *addr, u32 *dstAddr, u32 *mask);
extern int sceNetInet_lib_E6F67D14(const char *ifname);
extern int sceNetInet_lib_3D905F34(s32 *bufSize, InetRouteEntryInfo *outBuf);
extern int sceNetInet_lib_59561561(const u8 *macAddr, const u32 *ipAddr);
extern int sceNetInet_lib_DC38FEE9(int unusedArg, const u32 *ipAddr);
extern int sceNetInet_lib_DE9E5174(const char *ifname);
extern int sceNetInet_lib_824CBC6D(s32 bufSize, InetArpEntryInfo *outBuf);
extern int sceNetInet_lib_4459EAAF(const u32 *dst, const u32 *gw, const u32 *mask);
extern int sceNetInet_lib_A94A75E7(const u32 *gw);
extern int sceNetInet_lib_EB7A7E4F(const u32 *dst, const u32 *gw, const u32 *mask);
extern int sceNetInet_lib_FDE938A7(const u32 *gw);
extern int sceNetInet_lib_7776A492(const u32 *gw);
extern int sceNetInet_lib_3FDDB0BA(void);
extern int sceNetInet_lib_8FE19FC4(u8 *outGw);
extern int sceNetInet_lib_AC9D90A5(u32 *outMainDns, u32 *outAltDns);
extern int sceNetInet_lib_AAF4895A(const u32 *inMainDns, const u32 *inAltDns);
extern int sceNetInet_lib_AEE60F84(int sock, u32 cmd, void *arg);
extern int sceNetInet_lib_B3FF98EB(const int *name, int nameLen, void *oldp, u32 *oldLenp, void *newp, u32 newLen);
extern InetIfQueue *sceNetInet_lib_D7928A6D(void);
extern int sceNetInet_lib_7CB1D9E3(const char *ifname);
extern int sceNetInet_lib_4C13BE10(void);
extern char *sceNetInet_lib_2595064C(u32 addr);

/* ===================================================================== */
/* Internal cross-file globals and prototypes                            */
/* ===================================================================== */

extern const RijndaelTables g_rijndael_tables;
extern const u8 g_rijndael_rcon[30];

/* rijndael_asm.S & rijndael.c */
extern u32 rijndaelSboxWord(u32 word);
extern u32 rijndaelGfMul(u32 lhs, u32 rhs);
extern u32 rijndaelInvMixColByte(u32 colWord, u32 coeffWord);
extern u32 rijndaelInvMixColWord(u32 colWord);
extern int rijndaelEncryptBlock(const RijndaelCtx *ctx, const void *src, void *dst);
extern int rijndaelDecryptBlock(const RijndaelCtx *ctx, const void *src, void *dst);
extern int rijndaelKeySetup(RijndaelCtx *ctx, int blockBits, int keyBits, const u8 *key);
extern int rijndaelCbcEncrypt(u8 *dst, const u8 *src, u32 len, const u8 *key, int keyBits, const u8 *iv);
extern int rijndaelCbcDecrypt(u8 *dst, const u8 *src, u32 len, const u8 *key, int keyBits, const u8 *iv);

/* Domains & protocol switches */
extern InetDomain g_routedomain;
extern InetDomain g_arpdomain;
extern InetDomain g_inetdomain;
extern InetDomain *g_domains;

/* Socket / PCB tables & globals */
extern InetSocket *g_socket_list_head;
extern InetSocket **g_socket_list_tail;
extern u32 g_somaxconn;
extern s32 g_last_sock_id;
extern InetInpcbTable g_tcbtable;
extern InetInpcbTable g_udbtable;
extern InetIfQueue g_ipintrq;
extern InetIfQueue g_arpintrq;
extern InetRadixNodeHead *g_rt_tables[32];

/* Helper prototypes across files */
extern void inetIfctrlResetSeq(void);
extern int inetGetErrnoRaw(void);
extern int inetSoabortInit(void);
extern int inetSoabortTerm(void);
extern int inetDomainInit(void);
extern void inetDomainTerm(void);
extern InetDomain *inetFindDomain(int family);
extern InetProtosw *inetFindType(int family, int type);
extern InetProtosw *inetFindProto(int family, int protocol, int type);
extern int inetNetSysctl(int *name, u32 namelen, void *oldp, u32 *oldlenp, void *newp, u32 newlen);
extern void inetPfCtlinput(int cmd, InetSockaddr *sa);
extern void inetPfSlowtimo(void *arg);
extern void inetPfFasttimo(void *arg);

extern int inetSoCreate(int dom, InetSocket **aso, int type, int proto);
extern void inetSoFree(InetSocket *so);
extern int inetSoClose(InetSocket *so);
extern int inetSoAbort(InetSocket *so);
extern int inetSoDisconnect(InetSocket *so);
extern int inetSoSend(InetSocket *so, InetMbuf *addr, InetUio *uio, InetMbuf *top, InetMbuf *control, int flags);
extern int inetSoReceive(InetSocket *so, InetMbuf **paddr, InetUio *uio, InetMbuf **mp0, InetMbuf **controlp, u32 *flagsp);
extern void inetSorFlush(InetSocket *so);
extern int inetSoSetopt(InetSocket *so, int level, int optname, InetMbuf *m);
extern int inetSoGetopt(InetSocket *so, int level, int optname, InetMbuf **mp);
extern int inetSoPoll(InetSocket *so, int events, InetThreadInfo *ti);
extern void inetSoInit(void);
extern int inetSoBind(InetSocket *so, InetMbuf *nam);
extern int inetSoListen(InetSocket *so, s32 backlog);
extern int inetSoAccept(InetSocket *so, InetMbuf *nam);
extern int inetSoConnect(InetSocket *so, InetMbuf *nam);
extern int inetSoShutdown(InetSocket *so, u32 how);
extern void inetSoSelWakeup(InetSocket *so);
extern int inetSooIoctl(InetSocket *so, u32 cmd, void *data);

extern void inetSoIsConnected(InetSocket *so);
extern InetSocket *inetSoNewConn(InetSocket *head, int connstatus);
extern void inetSoQinsque(InetSocket *head, InetSocket *so, int qtype);
extern int inetSoQremque(InetSocket *so, int qtype);
extern void inetSoWakeup(InetSocket *so, InetSockBuf *sb);
extern int inetSoReserve(InetSocket *so, u32 sndcc, u32 rcvcc);
extern int inetSbReserve(InetSockBuf *sb, u32 cc);
extern void inetSbRelease(InetSockBuf *sb);
extern int inetSbAppendAddr(InetSockBuf *sb, const InetSockaddr *asa, InetMbuf *mbuf, InetMbuf *control);
extern void inetSbCompress(InetSockBuf *sb, InetMbuf *m, InetMbuf *n);
extern void inetSbFlush(InetSockBuf *sb);
extern void inetSbDrop(InetSockBuf *sb, int len);
extern void inetSbDropRecord(InetSockBuf *sb);
extern InetMbuf *inetSbCreateControl(const void *p, int size, int type, int level);
extern int inetSockAllocId(void);
extern InetSocket *inetSockFind(int sockId);
extern void inetSoIsConnecting(InetSocket *so);
extern void inetSoIsDisconnected(InetSocket *so);
extern void inetSoIsAborted(InetSocket *so);
extern void inetSoCantSendMore(InetSocket *so);
extern void inetSoCantRcvMore(InetSocket *so);
extern int inetSbWait(InetSockBuf *sb);
extern int inetSbLock(InetSockBuf *sb);
extern void inetSbAppend(InetSockBuf *sb, InetMbuf *m);
extern u32 inetCmsgAlignMask(void);
extern int inetSockCreateInternal(int domain, int type, int protocol);

extern int inetCloseInternal(int sock);
extern int inetSysctlInt(void *oldp, u32 *oldlenp, void *newp, u32 newlen, int *valp);
extern int inetSysctlRdInt(void *oldp, u32 *oldlenp, void *newp, int val);
extern int inetSysctlRdStruct(void *oldp, u32 *oldlenp, void *newp, const void *sp, u32 len);
extern char *inetNtoaStatic(u32 addr);

extern int inetIfHookInit(void);
extern int inetIfHookTerm(void);
extern int inetEtherHookInit(void);
extern int inetEtherHookTerm(void);
extern int inetEtherSetPppoeInput(int (*cb)(InetMbuf *, u16, InetIfnet *));
extern int inetEtherClearPppoeInput(void);
extern void inetLoAttach(void);
extern int inetRtIoctl(u32 cmd, void *data, InetIfnet *ifp);
extern void inetArpIntr(void);
extern void inetArpIfDetach(InetIfnet *ifp);
extern void inetIpIntr(void);
extern int inetInGetIfaddrInfo(const char *ifname, u32 *addr, u32 *dstAddr, u32 *mask);
/* Cross-file globals */
extern u32 g_inet_ticks;
extern const int g_inetctlerrmap[21];
extern InetProtosw g_inetsw[9];
extern const u8 g_etherbroadcastaddr[6];
extern const u8 g_ether_ipmulticast_min[6];
extern const u8 g_ether_ipmulticast_max[6];
extern s32 g_subnetsarelocal;
extern s32 g_hostzeroisbroadcast;
extern s32 g_anonportmin;
extern s32 g_anonportmax;
extern s32 g_lowportmin;
extern s32 g_lowportmax;
extern u32 g_in_maxmtu;
extern u32 g_in_ifaddr_hashmask;
extern u32 g_zeroin_addr;
extern InetInIfaddr **g_in_ifaddr_hashtbl;
extern InetInIfaddr *g_in_ifaddr_head;
extern InetInIfaddr **g_in_ifaddr_tail;
extern InetRouteCb g_route_cb;
extern InetIfnet g_lo0if;
extern s32 g_ipforwarding;
extern s32 g_ipsendredirects;
extern s32 g_ip_defttl;
extern s32 g_ip_forwsrcrt;
extern s32 g_ip_directedbcast;
extern s32 g_ip_allowsrcrt;
extern s32 g_ip_mtudisc;
extern s32 g_ip_mtudisc_timeout;
extern s32 g_ipqmaxlen;
extern s32 g_ip_maxfragpackets;
extern u8 g_ip_protox[256];
extern u16 g_ip_id;
extern InetRoute g_ipforward_rt;
extern InetIpStat g_ipstat;
extern InetRttimerQueue *g_ip_mtudisc_timeout_q;
extern int g_icmp_errppslim;
extern InetInpcbTable g_rawcb_table;
extern u32 g_rip_sendspace;
extern u32 g_rip_recvspace;
extern int g_udpcksum;
extern u32 g_udp_sendspace;
extern u32 g_udp_recvspace;
extern u32 g_udp_p2p_sendspace;
extern u32 g_udp_p2p_recvspace;
extern u16 g_p2p_default_port;
extern int g_tcp_syn_cache_size;
extern int g_tcp_syn_cache_limit;
extern int g_tcp_syn_bucket_limit;
extern int g_tcp_syn_cache_interval;
extern int g_tcp_rstppslim;
extern int g_tcprexmtthresh;
extern InetSynCacheHead g_tcp_syn_cache[293];
extern u32 g_tcp_syn_cache_count;
extern u32 g_tcp_now;
extern InetTcpStat g_tcpstat;
extern InetTcpcb *g_tcp_delack_head;
extern u32 g_tcp_iss_seq;
extern u32 g_tcp_keepidle;
extern u32 g_tcp_keepintvl;
extern u32 g_tcp_keepcnt;
extern s32 g_tcp_maxpersistidle;
extern const int g_tcp_backoff[13];
extern int g_tcp_totbackoff;
extern u32 g_tcp_sendspace;
extern u32 g_tcp_recvspace;
extern s32 g_tcp_maxidle;
extern u32 g_tcp_syn_cache_timer_tick;
extern int g_tcp_cwm_burstsize;
extern int g_tcp_mssdflt;
extern int g_tcp_rttdflt;
extern int g_tcp_do_rfc1323;
extern int g_tcp_do_sack;
extern int g_tcp_do_win_scale;
extern int g_tcp_do_timestamps;
extern int g_tcp_init_win;
extern u32 g_tcbhashsize;
extern int g_tcp_cwm;
extern int g_tcp_do_newreno;
extern int g_tcp_ack_on_push;
extern int g_tcp_mss_ifmtu;
extern int g_tcp_compat_42;
extern int g_tcp_log_refused;
extern int g_tcp_delack_ticks;

/* Cross-file prototypes: if_util_ether, radix, route, rtsock_raw, slcompress, if_arp */
extern InetIfaddr *inetIfaWithNet(const InetSockaddr *addr);
extern InetIfaddr *inetIfaOfIfpForAddr(const InetSockaddr *addr, InetIfnet *ifp);
extern int inetRtPurgeIfWalk(InetRadixNode *rn, void *ctx);
extern InetIfaddr *inetIfaWithAddr(const InetSockaddr *addr);
extern InetIfaddr *inetIfaWithDstAddr(const InetSockaddr *addr);
extern InetIfaddr *inetIfaWithAnyAddr(const InetSockaddr *addr);
extern void inetIfHookEmpty(void);
extern void inetLinkRtRequest(int cmd, InetRtentry *rt, InetRtAddrinfo *rtInfo);
extern int inetEtherOutput(InetIfnet *ifp, InetMbuf *m, InetSockaddr *dst, InetRtentry *rt0);

extern InetRadixNode *inetRnMatch(void *vArg, InetRadixNodeHead *head);
extern InetRadixNode *inetRnInsert(const u8 *key, InetRadixNodeHead *head, int *dupEntry, InetRadixNode *nodes);
extern InetRadixNode *inetRnAddmask(const u8 *netmask, int searchOnly, int skip);
extern InetRadixNode *inetRnAddroute(void *vArg, void *nArg, InetRadixNodeHead *head, InetRadixNode *treenodes);
extern InetRadixNode *inetRnDelete(void *vArg, void *nArg, InetRadixNodeHead *head);
extern int inetRnWalktree(InetRadixNodeHead *head, int (*cb)(InetRadixNode *, void *), void *arg);
extern int inetRnRefines(const u8 *mTest, const u8 *nBase);
extern InetRadixNode *inetRnLookup(void *vArg, void *mArg, InetRadixNodeHead *head);
extern int inetRnInithead(void **headOut, int offBits);
extern void inetRnInit(void);

extern InetRtentry *inetRtAlloc1(InetSockaddr *dst, int report);
extern void inetRtRedirect(InetSockaddr *dst, InetSockaddr *gateway, InetSockaddr *netmask, int flags, InetSockaddr *src, InetRtentry **outRt);
extern InetIfaddr *inetIfaIfWithRoute(int flags, InetSockaddr *dst, InetSockaddr *gateway);
extern int inetRtRequest(int req, InetSockaddr *dst, InetSockaddr *gateway, InetSockaddr *netmask, u32 flags, InetRtentry **retRt);
extern int inetRtSetgate(InetRtentry *rt, InetSockaddr *dst, InetSockaddr *gate);
extern void inetRtMaskedCopy(const u8 *src, u8 *dst, const u8 *mask);
extern int inetRtInit(InetIfaddr *ifa, int cmd, int flags);
extern void inetRtTimerQueueDestroy(InetRttimerQueue *queue, int destroyRoutes);
extern void inetRtTimerRemoveAll(InetRtentry *rt);
extern int inetRtTimerAdd(InetRtentry *rt, void (*func)(InetRtentry *, InetRttimer *), InetRttimerQueue *queue);
extern void inetRtTimerRun(void *unusedArg);
extern void inetRouteInit(void);
extern void inetRtAlloc(InetRoute *ro);
extern void inetRtFree(InetRtentry *rt);
extern InetRttimerQueue *inetRtTimerQueueCreate(s32 timeout);
extern void inetRtTimerQueueChange(InetRttimerQueue *queue, s32 timeout);
extern int inetRtFlushClone1(InetRadixNode *rn, void *parentCtx);
extern int inetRtFlushClone(InetRtentry *rt);

extern void inetRtSetMetrics(u32 which, const InetRtMetrics *inMetrics, InetRtMetrics *outMetrics);
extern void inetRtXaddrs(u8 *cp, const u8 *cplim, InetRtAddrinfo *rtInfo);
extern int inetRtMsg2(int type, InetRtAddrinfo *rtInfo, u8 *cp, void *walkPtr, int *lenp);
extern void inetRtNewAddrMsg(int cmd, InetIfaddr *ifa, int err, InetRtentry *rt);
extern void inetRtMissMsg(int type, InetRtAddrinfo *rtInfo, int flags, int err);
extern void inetRtIfMsg(InetIfnet *ifp);
extern void inetRtIfAnnounceMsg(InetIfnet *ifp, u16 what);
extern InetMbuf *inetRtMsg1(int type, InetRtAddrinfo *rtInfo, const void *hdrData, int hdrLen);
extern int inetLoOutput(InetIfnet *ifp, InetMbuf *m, InetSockaddr *dst, InetRtentry *rt);

extern u8 inetSlCompressTcp(InetMbuf *m, InetIpHdr *ip, InetSlCompress *comp, int compressCid);
extern int inetSlUncompressTcp(u8 *buf, u32 len, u16 totalLen, int type, InetSlCompress *comp, u8 **outHdr, u32 *outHdrLen);
extern void inetSlCompressInit(InetSlCompress *comp, int maxSlot);

extern void inetArpRtRequest(int req, void *rtPtr, void *rtInfo);
extern void inetArpRequest(InetIfnet *ifp, const u32 *sip, const u32 *tip, const u8 *enaddr);
extern int inetArpResolve(InetIfnet *ifp, InetRtentry *rt, InetMbuf *m, InetSockaddr *dst, u8 *destEnaddr);
extern void inetInArpInput(InetMbuf *m);
extern InetLlinfoArp *inetArpLookup(const u32 *addr, int create, int proxy);
extern void inetInRevarpInput(InetMbuf *m);
extern void inetArpIfInit(InetIfnet *ifp, InetIfaddr *ifa);
extern void inetRevarpInput(InetMbuf *m);

/* Cross-file prototypes: in_pcb, ip_input, ip_output, ip_icmp_igmp_raw, udp, tcp, sppp, init */
extern int inetInControl(InetSocket *so, u32 cmd, void *data, InetIfnet *ifp);
extern void inetInPurgeAddr(InetIfaddr *ifa, InetIfnet *ifp);
extern void inetInIfScrub(InetIfnet *ifp, InetInIfaddr *ia);
extern int inetInIfInit(InetIfnet *ifp, InetInIfaddr *ia, const InetSockaddrIn *sin, int scrub);
extern void inetInSaveMulti(InetInIfaddr *ia);
extern void inetInRestoreMulti(InetInIfaddr *ia, InetIfnet *ifp);
extern InetInMulti *inetInAddMulti(const u32 *addr, InetIfnet *ifp);
extern void inetInDelMulti(InetInMulti *inm);
extern int inetInLocalAddr(u32 addr);
extern int inetInCanForward(u32 addr);
extern void inetInPurgeIf(InetIfnet *ifp);
extern int inetInBroadcast(u32 addr, InetIfnet *ifp);
extern void inetInInit(void);
extern u16 inetIn4Cksum(InetMbuf *m, u8 proto, s32 off, s32 len);
extern u16 inetInCksum(InetMbuf *m, s32 len);
extern int inetInPcbBind(InetInpcbReal *inp, InetMbuf *nam);
extern int inetInPcbConnect(InetInpcbReal *inp, InetMbuf *nam);
extern void inetInPcbPurgeIf(InetInpcbTable *table, InetIfnet *ifp);
extern void inetInRtChange(InetInpcbReal *inp, int err);
extern InetInpcbReal *inetInPcbLookupPort(InetInpcbTable *table, u32 laddr, u16 lport, int lookupWild);
extern InetInpcbReal *inetInPcbLookupConnect(InetInpcbTable *table, u32 faddr, u16 fport, u32 laddr, u16 lport);
extern InetInpcbReal *inetInPcbLookupBind(InetInpcbTable *table, u32 laddr, u16 lport);
extern void inetInPcbState(InetInpcbReal *inp, int state);
extern InetSockaddrIn *inetInSelectSrc(InetSockaddrIn *sin, InetRoute *ro, int soOpts, InetIpMoptions *mopts, int *errp);
extern void inetInPcbInit(InetInpcbTable *table, int bindSize, int connectSize);
extern int inetInPcbAlloc(InetSocket *so, InetInpcbTable *table);
extern void inetInPcbDisconnect(InetInpcbReal *inp);
extern void inetInPcbDetach(InetInpcbReal *inp);
extern void inetInSetSockaddr(InetInpcbReal *inp, InetMbuf *nam);
extern void inetInSetPeeraddr(InetInpcbReal *inp, InetMbuf *nam);
extern int inetInPcbNotify(InetInpcbTable *table, u32 faddr, u16 fport, u32 laddr, u16 lport, int err, void (*notify)(InetInpcbReal *, int));
extern void inetInPcbNotifyAll(InetInpcbTable *table, u32 faddr, int err, void (*notify)(InetInpcbReal *, int));
extern void inetInLosing(InetInpcbReal *inp);
extern InetRtentry *inetInPcbRtEntry(InetInpcbReal *inp);

extern void inetTcpSynCacheInit(void);
extern void inetIpInit(void);
extern void inetIpInput(InetMbuf *m);
extern void inetIpSaveControl(InetInpcbReal *inp, InetMbuf **mp, InetIpHdr *ip, InetMbuf *m);
extern int inetIpSysctl(int *name, u32 namelen, void *oldp, u32 *oldlenp, void *newp, u32 newlen);
extern void inetIpSlowtimo(void);
extern void inetIpDrain(void);
extern InetMbuf *inetIpSrcroute(void);
extern void inetIpStripOptions(InetMbuf *m);

extern int inetIpOutput(InetMbuf *m, InetMbuf *opt, InetRoute *ro, int flags, InetIpMoptions *imo, ...);
extern int inetIpCtloutput(int op, InetSocket *so, int level, int optname, InetMbuf **mp);
extern s32 inetIpOptlen(InetInpcbReal *inp);
extern void inetIpFreeMoptions(InetIpMoptions *imo);

extern u32 inetIptime(void);
extern void inetIcmpError(InetMbuf *n, int type, int code, u32 dest, InetIfnet *destifp);
extern void inetIcmpInput(InetMbuf *m, int hlen, int proto);
extern int inetIcmpSysctl(int *name, u32 namelen, void *oldp, u32 *oldlenp, void *newp, u32 newlen);
extern void inetIcmpMtudisc(InetIcmpHdr *icp, u32 faddr);
extern void inetIcmpMtudiscCallbackRegister(void (*func)(u32));
extern void inetIcmpInit(void);
extern void inetIgmpInput(InetMbuf *m, int hlen, int proto);
extern void inetIgmpFasttimo(void);
extern void inetIgmpInit(void);
extern void inetIgmpJoingroup(InetInMulti *inm);
extern void inetIgmpLeavegroup(InetInMulti *inm);
extern void inetIgmpSlowtimo(void);
extern void inetRipInput(InetMbuf *m, int hlen, int proto);
extern int inetRipOutput(InetMbuf *m, InetInpcbReal *inp);
extern int inetRipCtloutput(int op, InetSocket *so, int level, int optname, InetMbuf **mp);
extern int inetRipUsrreq(InetSocket *so, int req, InetMbuf *m, InetMbuf *nam, InetMbuf *control);
extern void inetRipInit(void);

extern void inetUdpInput(InetMbuf *m, int iphlen, int proto);
extern int inetUdpUsrreq(InetSocket *so, int req, InetMbuf *m, InetMbuf *nam, InetMbuf *control);
extern void inetUdpInit(void);
extern void inetUdpCtlinput(int cmd, InetSockaddr *sa, void *d);
extern int inetUdpSysctl(int *name, u32 namelen, void *oldp, u32 *oldlenp, void *newp, u32 newlen);
extern int inetTcpP2pOutput(InetSockaddrIn *src, InetSockaddrIn *dst, InetMbuf *m, InetRoute *ro, InetSocket *so, int doEnc, int doAuth);
extern int inetUdpP2pUsrreq(InetSocket *so, int req, InetMbuf *m, InetMbuf *nam, InetMbuf *control);
extern InetP2pKeyEntry *inetP2pFindKey(u32 addr, u16 port);
extern InetMbuf *inetP2pDecrypt(InetMbuf *m, s32 off, u32 len, u8 protoByte);
extern void inetUdpP2pInit(void);

extern int inetTcpReass(InetTcpcb *tp, InetTcpHdr *th, InetMbuf *m, u32 *tlenp);
extern int inetTcpP2pDeliver(InetSockaddrIn *src, InetSockaddrIn *dst, InetMbuf *m, s32 off, u32 len, u8 protoByte);
extern void inetTcpInput(InetMbuf *m, int iphlen, int p2pSrcPort);
extern void inetSynCacheTimer(void);
extern void inetSynCacheCleanup(InetTcpcb *tp);
extern void inetSynCacheUnreach(InetSockaddrIn *src, InetSockaddrIn *dst, InetTcpHdr *th);
extern void inetSynCacheInit(void);

extern int inetTcpOutput(InetTcpcb *tp);
extern void inetTcpSetPersist(InetTcpcb *tp);
extern void inetTcpInit(void);
extern InetMbuf *inetTcpTemplate(InetTcpcb *tp);
extern int inetTcpRespond(InetTcpcb *tp, InetMbuf *mtemplate, InetMbuf *m, InetTcpHdr *th, u32 ack, u32 seq, int flags);
extern InetTcpcb *inetTcpNewTcpcb(int family, InetInpcbReal *inp);
extern InetTcpcb *inetTcpClose(InetTcpcb *tp);
extern void inetTcpCtlinput(int cmd, InetSockaddr *sa, void *d);
extern void inetTcpMssFromPeer(InetTcpcb *tp, u32 offer);
extern void inetTcpRttFromRt(InetTcpcb *tp);
extern InetTcpcb *inetTcpDrop(InetTcpcb *tp, int err);
extern void inetTcpDrain(void);
extern u32 inetTcpMssToAdvertise(InetIfnet *ifp, int family, InetTcpcb *tp);
extern void inetTcpEstablished(InetTcpcb *tp);
extern u32 inetTcpNewIss(void *ctx, u32 len, u32 addin);
extern void inetTcpP2pDropPeer(const InetSockaddrIn *peer);

extern void inetTcpSlowtimo(void);
extern void inetTcpFasttimo(void);
extern void inetTcpCancelTimers(InetTcpcb *tp);
extern void inetTcpTimerInit(void);
extern int inetTcpUsrreq(InetSocket *so, int req, InetMbuf *m, InetMbuf *nam, InetMbuf *control);
extern int inetTcpCtloutput(int op, InetSocket *so, int level, int optname, InetMbuf **mp);
extern int inetTcpSysctl(int *name, u32 namelen, void *oldp, u32 *oldlenp, void *newp, u32 newlen);

extern void inetSpppInput(InetIfnet *ifp, InetMbuf *m);
extern int inetSpppOutput(InetIfnet *ifp, InetMbuf *m, InetSockaddr *dst, InetRtentry *rt);
extern void inetSpppFlush(InetSppp *sp);
extern InetMbuf *inetSpppDequeue(InetIfnet *ifp);
extern int inetSpppIoctl(InetIfnet *ifp, u32 cmd, void *data);
extern void inetSpppAttach(InetIfnet *ifp);
extern void inetSpppDetach(InetIfnet *ifp);
extern int inetSpppIsEmpty(InetIfnet *ifp);
extern int inetSpppHookInit(void);
extern int inetSpppHookTerm(void);

extern int inetAtonInternal(const char *cp, u32 *addrOut, int strictDecimal);

/* Inline helpers for active-socket tracking on current thread */
static __inline__ __attribute__((always_inline)) void inetThreadAttachSock(InetThreadInfo *ti, InetSocket *so)
{
    so->so_owner_next = ti->active_sock;
    so->so_owner_ti = ti;
    ti->active_sock = so;
}

static __inline__ __attribute__((always_inline)) void inetThreadDetachSock(InetThreadInfo *ti, InetSocket *so)
{
    InetSocket *cur = ti->active_sock;
    so->so_owner_ti = NULL;
    if (cur == so) {
        ti->active_sock = cur->so_owner_next;
    } else if (cur != NULL) {
        while (cur->so_owner_next != NULL) {
            if (cur->so_owner_next == so) {
                cur->so_owner_next = so->so_owner_next;
                break;
            }
            cur = cur->so_owner_next;
        }
    }
}

/* Inline helper for freeing a single mbuf (exact inline expansion of MFREE) */
static __inline__ __attribute__((always_inline)) InetMbuf *inetMfreeOne(InetMbuf *m)
{
    InetMbuf *next;
    if ((m->m_flags & INET_M_EXT) != 0) {
        InetMbuf *ref = M_EXT(m).ext_nextref;
        if (ref == m) {
            sceNetIfhandle_lib_76BAD213(M_EXT(m).ext_buf);
        } else {
            M_EXT(ref).ext_prevref = M_EXT(m).ext_prevref;
            M_EXT(M_EXT(m).ext_prevref).ext_nextref = ref;
        }
        m->m_flags &= (u16)~(INET_M_EXT | INET_M_CLUSTER);
        M_EXT(m).ext_size = 0;
    }
    next = m->m_next;
    sceNetIfhandle_lib_76BAD213(m);
    return next;
}

#endif /* PSPNET_INET_INTERNAL_H */
