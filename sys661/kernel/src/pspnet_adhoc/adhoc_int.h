#ifndef PSPNET_ADHOC_INT_H
#define PSPNET_ADHOC_INT_H

/*
 * PSP 6.61 pspnet_adhoc.prx (sceNetAdhoc_Library)
 * Full instruction-accurate decompilation of kd/pspnet_adhoc.prx
 */

#include <common/types.h>

static inline u16 bswap16(u16 v)
{
	u16 res;
	__asm__("wsbh %0, %1" : "=r"(res) : "r"(v));
	return res;
}

static inline u32 bswap32(u32 v)
{
	u32 res;
	__asm__("wsbw %0, %1" : "=r"(res) : "r"(v));
	return res;
}

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
extern void sceNetIfhandle_lib_3E8DD3F8(void *m0, void *m);
extern void *sceNetIfhandle_lib_49EDBB18(void *m, s32 len);
extern void sceNetIfhandle_lib_76BAD213(void *ptr);
extern void sceNetIfhandle_lib_9A6261EC(void *m, s32 off, s32 len, void *dst);
extern void sceNetIfhandle_lib_BFF3CEA5(void *m, s32 len);
extern void *sceNetIfhandle_lib_F56FAC82(void *m);
extern s32 sceNetIfhandle_lib_F8825DC4(void *m);
extern void sceNet_lib_05D525E4(s32 id);
extern void sceNet_lib_0A5A8751(void);
extern u32 sceNet_lib_1858883D(void);
extern s32 sceNet_lib_1F94AFD9(void *cb);
extern void sceNet_lib_25CC373A(void *timer);
extern void sceNet_lib_2E005032(void *cb);
extern void sceNet_lib_33B230BD(void);
extern void sceNet_lib_515B2F33(s32 intr);
extern void sceNet_lib_6B294EE4(void *sock, s32 flags);
extern void *sceNet_lib_757085B0(void *a0, s32 a1);
extern void sceNet_lib_7574FDA1(void *chan);
extern void sceNet_lib_949F1FBB(void);
extern s32 sceNet_lib_94B44F26(void);
extern void sceNet_lib_A8B6205A(void *timer, u32 delay, void *cb, void *arg);
extern void sceNet_lib_A93A93E9(void *timer);
extern s32 sceNet_lib_B3A48B7F(void *rx_cb, void *ctrl_cb);
extern void *sceNet_lib_CA3CF5EB(s32 *out_err);
extern void *sceNet_lib_D60225A3(void);
extern s32 sceNet_lib_E0A81C7C(const void *a, const void *b, u32 len);
extern s32 sceNet_lib_F6DB0A0B(void *chan, u32 *timeout);
extern s32 sceWlanDevIsGameMode(void);
extern s32 sceWlanGPPrevEstablishActive(void);
extern s32 sceWlanGPRecv(s32 id, void *buf, u32 size, void *info);
extern s32 sceWlanGPSend(s32 id, void *buf, u32 size);

/* Mbuf structure used by sceNetIfhandle_lib */
typedef struct AdhocMbuf {
	struct AdhocMbuf *m_next;     /* 0x00 */
	struct AdhocMbuf *m_nextpkt;  /* 0x04 */
	u8 *m_data;                   /* 0x08 */
	s32 m_len;                    /* 0x0c */
	s16 m_type;                   /* 0x10 */
	u16 m_flags;                  /* 0x12 */
	void *m_rcvif;                /* 0x14 */
	s32 m_pkthdr_len;             /* 0x18 */
	u32 m_pkthdr_Aux;             /* 0x1c */
	u8 m_ext_or_pktdat[0xd0];     /* 0x20..0xef (m_ext at 0x30..0x4b) */
} AdhocMbuf;

/* Interface structure returned by sceNet_lib_D60225A3() */
typedef struct AdhocIfnet {
	u8 reserved_00[0x30];
	u16 if_flags;                 /* 0x30 */
	u8 reserved_32[0x56];
	u32 if_obytes_lo;             /* 0x88 */
	u32 if_obytes_hi;             /* 0x8c */
	u8 reserved_90[0x28];
	void (*if_start)(void *ifp);  /* 0xb8 */
	u8 reserved_bc[0x10];
	AdhocMbuf *snd_head;          /* 0xcc */
	AdhocMbuf *snd_tail;          /* 0xd0 */
	s32 snd_len;                  /* 0xd4 */
	s32 snd_maxlen;               /* 0xd8 */
	s32 snd_drops;                /* 0xdc */
	u8 *if_sadl;                  /* 0xe0 */
} AdhocIfnet;

/* Waiter node on sock->wait_list (linked via +0x58) */
typedef struct AdhocWaiter {
	u8 reserved_00[0x20];
	u32 wait_chan;                /* 0x20 */
	u8 reserved_24[0x28];
	s32 sema_id;                  /* 0x4c */
	u8 reserved_50[0x08];
	struct AdhocWaiter *next;     /* 0x58 */
} AdhocWaiter;

/* Socket descriptor (0x28 bytes) allocated by adhocSockCreate */
typedef struct AdhocSocket {
	struct AdhocSocket *next;     /* 0x00 */
	struct AdhocSocket **prev;    /* 0x04 */
	s32 id;                       /* 0x08 */
	u32 flags;                    /* 0x0c */
	s32 type;                     /* 0x10: 1 = PDP, 2 = PTP */
	void *pcb;                    /* 0x14 */
	u32 chan_rd;                  /* 0x18 */
	u32 chan_wr;                  /* 0x1c */
	u32 chan_ex;                  /* 0x20 */
	AdhocWaiter *wait_list;       /* 0x24 */
} AdhocSocket;

/* PDP fragment reassembly node (0x2c bytes) */
typedef struct PdpFragEntry {
	struct PdpFragEntry *next;    /* 0x00 */
	struct PdpFragEntry **prev;   /* 0x04 */
	u8 src_mac[6];                /* 0x08 */
	u16 sport;                    /* 0x0e */
	u8 timer[0x1c];               /* 0x10..0x2b */
	u16 seq;                      /* 0x2c */
	u16 cur_off;                  /* 0x2e */
	AdhocMbuf *m_head;            /* 0x30 */
} PdpFragEntry;

/* PDP PCB (0x3c bytes) allocated by pdpAllocPcb */
typedef struct ScePdpStatOut {
    struct ScePdpStatOut *next; /* 0x00 */
    s32 pdp_id;                 /* 0x04 */
    u8 mac[6];                  /* 0x08 */
    u16 port;                   /* 0x0e */
    u32 rcvd_data;              /* 0x10 */
} ScePdpStatOut;

typedef struct ScePtpStatOut {
    struct ScePtpStatOut *next; /* 0x00 */
    s32 ptp_id;                 /* 0x04 */
    u8 local_mac[6];            /* 0x08 */
    u8 peer_mac[6];             /* 0x0e */
    u16 local_port;             /* 0x14 */
    u16 peer_port;              /* 0x16 */
    u32 sent_data;              /* 0x18 */
    u32 rcvd_data;              /* 0x1c */
} ScePtpStatOut;

typedef struct PdpPcb {
	struct PdpPcb *next;          /* 0x00 */
	struct PdpPcb **prev;         /* 0x04 */
	AdhocSocket *sock;            /* 0x08 */
	u32 state_flags;              /* 0x0c */
	u8 mac[6];                    /* 0x10 */
	u16 port;                     /* 0x16 */
	AdhocMbuf *rx_head;           /* 0x18 */
	AdhocMbuf *rx_tail;           /* 0x1c */
	u32 rx_cc;                    /* 0x20 */
	u32 rx_hiwat;                 /* 0x24 */
	u32 rx_drops;                 /* 0x28 */
	u32 reserved_2c;                   /* 0x2c */
	PdpFragEntry *frag_head;      /* 0x30 */
	PdpFragEntry **frag_tail;     /* 0x34 */
	u32 frag_count;               /* 0x38 */
} PdpPcb;

/* PTP PCB (0xf0 bytes) allocated by ptpAllocPcb */
typedef struct PtpPcb {
	struct PtpPcb *next;          /* 0x00 */
	struct PtpPcb **prev;         /* 0x04 */
	struct PtpPcb *acc_next;      /* 0x08 */
	struct PtpPcb **acc_prev;     /* 0x0c */
	AdhocSocket *sock;            /* 0x10 */
	struct PtpPcb *parent;        /* 0x14 */
	u8 local_mac[6];              /* 0x18 */
	u8 peer_mac[6];               /* 0x1e */
	u16 local_port;               /* 0x24 */
	u16 peer_port;                /* 0x26 */
	AdhocMbuf *tx_head;           /* 0x28 */
	AdhocMbuf *tx_tail;           /* 0x2c */
	u32 tx_cc;                    /* 0x30 */
	u32 tx_hiwat;                 /* 0x34 */
	u32 tx_drops;                 /* 0x38 */
	u32 tx_flags;                 /* 0x3c */
	AdhocMbuf *rx_head;           /* 0x40 */
	AdhocMbuf *rx_tail;           /* 0x44 */
	u32 rx_cc;                    /* 0x48 */
	u32 rx_hiwat;                 /* 0x4c */
	u32 rx_drops;                 /* 0x50 */
	u32 rx_flags;                 /* 0x54 */
	s32 mode;                     /* 0x58: 1=client/child, 2=listener */
	s32 state;                    /* 0x5c: 0=CLOSED, 1=LISTEN, 2=SYN_SENT, 3=SYN_RCVD, 4=ESTABLISHED */
	u32 flags;                    /* 0x60 */
	u32 rcv_wnd;                  /* 0x64 */
	u32 iss;                      /* 0x68 */
	u32 snd_max;                  /* 0x6c */
	u32 snd_una;                  /* 0x70 */
	u32 snd_nxt;                  /* 0x74 */
	u32 rcv_nxt;                  /* 0x78 */
	u32 snd_wnd;                  /* 0x7c */
	u32 irs;                      /* 0x80 */
	u8 timer_rexmt[0x1c];         /* 0x84..0x9f */
	u32 timer_rexmt_active;       /* 0xa0 */
	u32 reserved_a4;                   /* 0xa4 */
	u8 timer_probe[0x1c];         /* 0xa8..0xc3 */
	u32 timer_probe_active;       /* 0xc4 */
	u32 reserved_c8;                   /* 0xc8 */
	u32 rexmt_intvl;              /* 0xcc */
	s32 rexmt_max;                /* 0xd0 */
	s32 rexmt_rem;                /* 0xd4 */
	struct PtpPcb *q_head;        /* 0xd8 */
	struct PtpPcb **q_tail;       /* 0xdc */
	s32 q_len;                    /* 0xe0 */
	s32 q_maxlen;                 /* 0xe4 */
	u32 q_drops;                  /* 0xe8 */
	u32 q_flags;                  /* 0xec */
} PtpPcb;

/* GameMode node (0x24 bytes) */
typedef struct GameModeNode {
	struct GameModeNode *next;    /* 0x00 */
	struct GameModeNode **prev;   /* 0x04 */
	s32 id;                       /* 0x08 */
	u8 mac[6];                    /* 0x0c */
	u16 reserved_12;                   /* 0x12 */
	void *user_buf;               /* 0x14 */
	u32 user_size;                /* 0x18 */
	void *work_buf;               /* 0x1c */
	u32 work_size;                /* 0x20 */
} GameModeNode;

/* Poll descriptor (0x0c bytes) */
typedef struct SceNetAdhocPollSd {
	s32 id;                       /* 0x00 */
	s32 events;                   /* 0x04 */
	s32 revents;                  /* 0x08 */
} SceNetAdhocPollSd;

extern const u16 g_adhoc_ethertype;
extern const u16 g_adhoc_hdr_ver;
extern const u8 g_bcast_mac[6];
extern const u8 g_zero_mac[6];
extern s32 g_adhoc_inited;
extern u32 g_global_wait_head;

/* Cross-module internal declarations */
s32 adhocIfOutput(void *ifp_arg, void *m_arg);
AdhocSocket *adhocSockFind(s32 type, s32 id);
s32 adhocSockAllocId(void);
s32 adhocSockCreate(s32 type, AdhocSocket **out_sock);
void adhocSockTryFree(AdhocSocket *sock);

void pdpInit(void);
void pdpTerm(void);
s32 pdpHasActive(void);
void pdpIfDetach(void *ifp_arg);
s32 pdpPollEvents(AdhocSocket *sock, s32 events);
void pdpInputPacket(AdhocMbuf *m);

void ptpInit(void);
void ptpTerm(void);
s32 ptpHasActive(void);
void ptpIfDetach(void *ifp_arg);
void ptpIfDetachNoop(void *ifp_arg);
s32 ptpPollEvents(AdhocSocket *sock, s32 events);
void ptpInputPacket(AdhocMbuf *m);

s32 gameModeInit(void);
void gameModeTerm(void);

#endif /* PSPNET_ADHOC_INT_H */
