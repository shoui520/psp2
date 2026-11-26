#ifndef PSPNET_ADHOC_INT_H
#define PSPNET_ADHOC_INT_H

/*
 * ePSP 6.61 pspnet_adhoc.prx (sceNetAdhoc_Library)
 * Reimplemented from the Vita ePSP 6.61 binary.
 */

#include <common/types.h>

static inline u16 adhocSwap16(u16 value)
{
    return (u16)__builtin_allegrex_wsbh(value);
}

static inline u32 adhocSwap32(u32 value)
{
    return (u32)__builtin_allegrex_wsbw(value);
}

/* Error codes */
#define ADHOC_ERR_NO_MEM              ((s32)0x80410001)
#define ADHOC_ERR_INTERNAL            ((s32)0x80410002)
#define ADHOC_ERR_STACK_SMALL         ((s32)0x80410005)
#define ADHOC_ERR_INVALID_SOCKET_ID   ((s32)0x80410701)
#define ADHOC_ERR_INVALID_ADDR        ((s32)0x80410702)
#define ADHOC_ERR_INVALID_PORT        ((s32)0x80410703)
#define ADHOC_ERR_INVALID_BUFLEN      ((s32)0x80410705)
#define ADHOC_ERR_BUFFER_TOO_SMALL    ((s32)0x80400706)
#define ADHOC_ERR_SOCKET_DELETED      ((s32)0x80410707)
#define ADHOC_ERR_SOCKET_ALERTED      ((s32)0x80410708)
#define ADHOC_ERR_WOULD_BLOCK         ((s32)0x80410709)
#define ADHOC_ERR_PORT_IN_USE         ((s32)0x8041070A)
#define ADHOC_ERR_NOT_CONNECTED       ((s32)0x8041070B)
#define ADHOC_ERR_DISCONNECTED        ((s32)0x8041070C)
#define ADHOC_ERR_NOT_OPENED          ((s32)0x8040070D)
#define ADHOC_ERR_NOT_LISTENED        ((s32)0x8040070E)
#define ADHOC_ERR_SOCKET_ID_NOT_AVAIL ((s32)0x8041070F)
#define ADHOC_ERR_PORT_NOT_AVAIL      ((s32)0x80410710)
#define ADHOC_ERR_INVALID_ARG         ((s32)0x80410711)
#define ADHOC_ERR_NOT_INITIALIZED     ((s32)0x80410712)
#define ADHOC_ERR_ALREADY_INITIALIZED ((s32)0x80410713)
#define ADHOC_ERR_BUSY                ((s32)0x80410714)
#define ADHOC_ERR_TIMEOUT             ((s32)0x80410715)
#define ADHOC_ERR_NO_ENTRY            ((s32)0x80410716)
#define ADHOC_ERR_EXCEPTION_EVENT     ((s32)0x80410717)
#define ADHOC_ERR_CONNECTION_REFUSED  ((s32)0x80410718)
#define ADHOC_ERR_THREAD_ABORTED      ((s32)0x80410719)
#define ADHOC_ERR_ALREADY_CREATED     ((s32)0x8041071A)
#define ADHOC_ERR_NOT_IN_GAMEMODE     ((s32)0x8041071B)
#define ADHOC_ERR_NOT_CREATED         ((s32)0x8041071C)
#define ADHOC_ERR_INVALID_ALIGNMENT   ((s32)0x8041071D)

/* Imports */
extern void *sceKernelMemset(void *dst, int c, u32 n);
extern s32 _sceKernelCheckThreadStack(void);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 n);
extern s32 sceKernelDelayThread(u32 delay);
extern s32 sceKernelSignalSema(s32 semaid, s32 signal);
extern s64 sceKernelGetSystemTimeWide(void);
extern s32 sceNetGetLocalEtherAddr(u8 *mac);
extern s32 sceNetAdhocAuth_lib_015A8A64(void *buf, u32 size);
extern s32 sceNetAdhocAuth_lib_AAB06250(void *buf, u32 size);
extern u32 sceNetIfhandle_lib_07505747(s32 level);
extern void *sceNetIfhandle_lib_15CFE3C0(u32 size);
extern void sceNetIfhandle_lib_3E8DD3F8(void *head_mbuf, void *tail_mbuf);
extern void *sceNetIfhandle_lib_49EDBB18(void *mbuf, s32 len);
extern void sceNetIfhandle_lib_76BAD213(void *ptr);
extern void sceNetIfhandle_lib_9A6261EC(void *mbuf, s32 offset, s32 len, void *dst);
extern void sceNetIfhandle_lib_BFF3CEA5(void *mbuf, s32 len);
extern void *sceNetIfhandle_lib_F56FAC82(void *mbuf);
extern s32 sceNetIfhandle_lib_F8825DC4(void *mbuf);
extern s32 sceNet_lib_05D525E4(s32 intr_id);
extern void sceNet_lib_0A5A8751(void);
extern u32 sceNet_lib_1858883D(void);
extern s32 sceNet_lib_1F94AFD9(void *cb);
extern void sceNet_lib_25CC373A(void *timer);
extern s32 sceNet_lib_2E005032(void *cb);
extern void sceNet_lib_33B230BD(void);
extern void sceNet_lib_515B2F33(s32 intr);
extern void sceNet_lib_6B294EE4(void *sock, s32 flags);
extern void *sceNet_lib_757085B0(void *waiter, s32 flags);
extern s32 sceNet_lib_7574FDA1(void *chan);
extern void sceNet_lib_949F1FBB(void);
extern s32 sceNet_lib_94B44F26(void);
extern void sceNet_lib_A8B6205A(void *timer, u32 delay, void *cb, void *arg);
extern void sceNet_lib_A93A93E9(void *timer);
extern s32 sceNet_lib_B3A48B7F(void *rx_cb, void *ctrl_cb);
extern void *sceNet_lib_CA3CF5EB(s32 *out_err);
extern void *sceNet_lib_D60225A3(const u8 *mac);
extern s32 sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, u32 len);
extern s32 sceNet_lib_F6DB0A0B(void *chan, u32 timeout);
extern s32 sceWlanDevIsGameMode(void);
extern s32 sceWlanGPPrevEstablishActive(void);
extern s32 sceWlanGPRecv(s32 id, void *buf, u32 size, void *info);
extern s32 sceWlanGPSend(s32 id, void *buf, u32 size);

/* Timer object used by sceNet_lib (36 bytes = 0x24) */
typedef struct AdhocTimer {
    u8 opaque[0x1C];
    u32 flags;
    u32 tail_pad;
} AdhocTimer;

/* Mbuf structure used by sceNetIfhandle_lib (256 bytes = 0x100) */
typedef struct AdhocMbuf {
    struct AdhocMbuf *m_next;     /* 0x00 */
    struct AdhocMbuf *m_nextpkt;  /* 0x04 */
    u8 *m_data;                   /* 0x08 */
    s32 m_len;                    /* 0x0C */
    s16 m_type;                   /* 0x10 */
    u16 m_flags;                  /* 0x12 */
    void *m_rcvif;                /* 0x14 */
    s32 m_pkthdr_len;             /* 0x18 */
    u32 m_pkthdr_aux;             /* 0x1C */
    union {
        struct {
            s16 rssi;             /* 0x20 */
            s16 rate;             /* 0x22 */
            u32 meta_pad;         /* 0x24 */
            s64 send_time;        /* 0x28 */
        } meta;
        u8 meta_bytes[0x10];      /* 0x20..0x2F */
    } hdr_meta;
    union {
        u8 inline_buf[0xD0];      /* 0x30..0xFF */
        struct {
            void *ext_buf;        /* 0x30 */
            u32 ext_free_fn;      /* 0x34 */
            u32 ext_arg;          /* 0x38 */
            u32 ext_size;         /* 0x3C */
            u32 ext_type;         /* 0x40 */
            struct AdhocMbuf *ext_nextref; /* 0x44 */
            struct AdhocMbuf *ext_prevref; /* 0x48 */
        } ext;
    } payload;
} AdhocMbuf;

/* Interface structure returned by sceNet_lib_D60225A3(mac) */
typedef struct AdhocIfnet {
    u8 head_pad[0x30];
    u16 if_flags;                 /* 0x30 */
    u8 mid_pad[0x56];
    u64 if_obytes;                /* 0x88 */
    u8 stat_pad[0x28];
    s32 (*if_start)(struct AdhocIfnet *ifp); /* 0xB8 */
    u8 func_pad[0x10];
    AdhocMbuf *snd_head;          /* 0xCC (204) */
    AdhocMbuf *snd_tail;          /* 0xD0 (208) */
    s32 snd_len;                  /* 0xD4 (212) */
    s32 snd_maxlen;               /* 0xD8 (216) */
    s32 snd_drops;                /* 0xDC (220) */
    u8 *if_sadl;                  /* 0xE0 (224) */
    u8 tail_pad[0x3C];
    u32 snd_wait_flags;           /* 0x120 (288) */
} AdhocIfnet;

/* Waiter node on sock->wait_list */
typedef struct AdhocWaiter {
    u8 head_pad[0x20];
    void *wait_chan;              /* 0x20 */
    u8 mid_pad[0x28];
    s32 sema_id;                  /* 0x4C */
    u8 tail_pad[0x08];
    struct AdhocWaiter *next;     /* 0x58 */
} AdhocWaiter;

/* Socket descriptor (40 bytes = 0x28) */
typedef struct AdhocSocket {
    struct AdhocSocket *next;     /* 0x00 */
    struct AdhocSocket **prev;    /* 0x04 */
    s32 id;                       /* 0x08 */
    u32 flags;                    /* 0x0C */
    u8 type;                      /* 0x10: 1 = PDP, 2 = PTP */
    u8 type_pad[3];               /* 0x11..0x13 */
    void *pcb;                    /* 0x14 */
    void *chan_tx;                /* 0x18 */
    void *chan_rx;                /* 0x1C */
    void *chan_poll;              /* 0x20 */
    AdhocWaiter *wait_list;       /* 0x24 */
} AdhocSocket;

struct PdpPcb;

/* PDP fragment reassembly node (56 bytes = 0x38) */
typedef struct PdpFragEntry {
    struct PdpFragEntry *next;    /* 0x00 */
    struct PdpFragEntry **prev;   /* 0x04 */
    u16 total_len;                /* 0x08 */
    u16 frag_pad;                 /* 0x0A */
    AdhocMbuf *m_head;            /* 0x0C */
    AdhocTimer timer;             /* 0x10..0x33 */
    struct PdpPcb *owner;         /* 0x34 */
} PdpFragEntry;

/* PDP PCB (52 bytes = 0x34) */
typedef struct PdpPcb {
    struct PdpPcb *next;          /* 0x00 */
    struct PdpPcb **prev;         /* 0x04 */
    u8 mac[6];                    /* 0x08 */
    u16 port;                     /* 0x0E */
    AdhocMbuf *rx_head;           /* 0x10 */
    AdhocMbuf *rx_tail;           /* 0x14 */
    u32 rx_cc;                    /* 0x18 */
    u32 rx_hiwat;                 /* 0x1C */
    u32 rx_lowat;                 /* 0x20 */
    u32 rx_wait_flags;            /* 0x24 */
    AdhocSocket *sock;            /* 0x28 */
    PdpFragEntry *frag_head;      /* 0x2C */
    PdpFragEntry **frag_tail;     /* 0x30 */
} PdpPcb;

/* PDP status output (20 bytes = 0x14) */
typedef struct ScePdpStatOut {
    struct ScePdpStatOut *next;   /* 0x00 */
    s32 pdp_id;                   /* 0x04 */
    u8 mac[6];                    /* 0x08 */
    u16 port;                     /* 0x0E */
    u32 rcvd_data;                /* 0x10 */
} ScePdpStatOut;

/* PDP extra receive metadata (8 bytes) */
typedef struct PdpRecvExtra {
    s32 rssi;
    s32 rate;
} PdpRecvExtra;

/* PTP PCB (228 bytes = 0xE4) */
typedef struct PtpPcb {
    struct PtpPcb *next;          /* 0x00 (0) */
    struct PtpPcb **prev;         /* 0x04 (1) */
    struct PtpPcb *acc_next;      /* 0x08 (2) */
    struct PtpPcb **acc_prev;     /* 0x0C (3) */
    struct PtpPcb *q_head;        /* 0x10 (4) */
    struct PtpPcb **q_tail;       /* 0x14 (5) */
    u8 self_mac[6];               /* 0x18 */
    u8 peer_mac[6];               /* 0x1E */
    u16 self_port;                /* 0x24 */
    u16 peer_port;                /* 0x26 */
    AdhocMbuf *tx_head;           /* 0x28 (10) */
    AdhocMbuf *tx_tail;           /* 0x2C (11) */
    u32 tx_cc;                    /* 0x30 (12) */
    u32 tx_hiwat;                 /* 0x34 (13) */
    u32 tx_lowat;                 /* 0x38 (14) */
    u32 tx_wait_flags;            /* 0x3C (15) */
    AdhocMbuf *rx_head;           /* 0x40 (16) */
    AdhocMbuf *rx_tail;           /* 0x44 (17) */
    u32 rx_cc;                    /* 0x48 (18) */
    u32 rx_hiwat;                 /* 0x4C (19) */
    u32 rx_lowat;                 /* 0x50 (20) */
    u32 rx_wait_flags;            /* 0x54 (21) */
    AdhocSocket *sock;            /* 0x58 (22) */
    s32 state;                    /* 0x5C (23): 0=CLOSED, 1=LISTEN, 2=SYN_SENT, 3=SYN_RCVD, 4=ESTABLISHED */
    u32 proto_flags;              /* 0x60 (24) */
    u32 mss;                      /* 0x64 (25) */
    u32 conn_id;                  /* 0x68 (26) */
    u32 snd_max;                  /* 0x6C (27) */
    u32 snd_una;                  /* 0x70 (28) */
    u32 snd_nxt;                  /* 0x74 (29) */
    u32 rcv_nxt;                  /* 0x78 (30) */
    u32 peer_wnd;                 /* 0x7C (31) */
    u32 seq_pad;                  /* 0x80 (32) */
    AdhocTimer timer_rexmt;       /* 0x84..0xA7 (33..41) */
    AdhocTimer timer_probe;       /* 0xA8..0xCB (42..50) */
    u32 rexmt_intvl;              /* 0xCC (51) */
    s32 rexmt_max;                /* 0xD0 (52) */
    s32 rexmt_rem;                /* 0xD4 (53) */
    s32 q_len;                    /* 0xD8 (54) */
    s32 q_maxlen;                 /* 0xDC (55) */
    struct PtpPcb *parent;        /* 0xE0 (56) */
} PtpPcb;

/* PTP status output (36 bytes = 0x24) */
typedef struct ScePtpStatOut {
    struct ScePtpStatOut *next;   /* 0x00 */
    s32 ptp_id;                   /* 0x04 */
    u8 self_mac[6];               /* 0x08 */
    u8 peer_mac[6];               /* 0x0E */
    u16 self_port;                /* 0x14 */
    u16 peer_port;                /* 0x16 */
    u32 sent_data;                /* 0x18 */
    u32 rcvd_data;                /* 0x1C */
    s32 state;                    /* 0x20 */
} ScePtpStatOut;

/* GameMode node (36 bytes = 0x24) */
typedef struct GameModeNode {
    struct GameModeNode *next;    /* 0x00 */
    struct GameModeNode **prev;   /* 0x04 */
    s32 id;                       /* 0x08 */
    u8 mac[6];                    /* 0x0C */
    u16 mac_pad;                  /* 0x12 */
    void *user_buf;               /* 0x14 */
    u32 user_size;                /* 0x18 */
    void *work_buf;               /* 0x1C */
    u32 work_size;                /* 0x20 */
} GameModeNode;

typedef struct GameModeReplicaOpt {
    u32 size;
    u32 flag;
    u32 last_update_lo;
    u32 last_update_hi;
} GameModeReplicaOpt;

typedef struct GameModeReplicaOptEx {
    u32 flag;
    u32 pad_word;
    u32 last_update_lo;
    u32 last_update_hi;
    u32 extra_status;
} GameModeReplicaOptEx;

/* Poll descriptor (12 bytes = 0x0C) */
typedef struct SceNetAdhocPollSd {
    s32 id;                       /* 0x00 */
    s32 events;                   /* 0x04 */
    s32 revents;                  /* 0x08 */
} SceNetAdhocPollSd;

typedef struct AdhocPollEntry {
    AdhocSocket *sock;
    SceNetAdhocPollSd *sd;
} AdhocPollEntry;

extern const u16 g_adhoc_ethertype;
extern const u16 g_adhoc_hdr_ver;
extern const u8 g_bcast_mac[6];
extern const u8 g_zero_mac[6];
extern s32 g_adhoc_inited;
extern u32 g_global_wait_head;

/* Cross-unit internal declarations */
s32 adhocIfOutput(AdhocIfnet *ifp, AdhocMbuf *mbuf);
AdhocSocket *adhocSockFind(u8 type, s32 id);
s32 adhocSockAllocId(void);
s32 adhocSockCreate(u8 type, AdhocSocket **out_sock);
void adhocSockTryFree(AdhocSocket *sock);

s32 pdpInit(void);
void pdpTerm(void);
void pdpFreePcb(AdhocSocket *sock);
s32 pdpIfOutputWake(AdhocIfnet *ifp);
s32 pdpPollEvents(AdhocSocket *sock, s32 events);
void pdpInputPacket(AdhocMbuf *mbuf);

s32 ptpInit(void);
void ptpTerm(void);
void ptpFreePcb(AdhocSocket *sock);
s32 ptpPollEvents(AdhocSocket *sock, s32 events);
void ptpInputPacket(AdhocMbuf *mbuf);

s32 gameModeInit(void);
void gameModeTerm(void);

#endif /* PSPNET_ADHOC_INT_H */
