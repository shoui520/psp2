/*
 * PSP 6.61 pspnet_adhoc.prx (sceNetAdhoc_Library)
 * Full instruction-accurate decompilation of kd/pspnet_adhoc.prx
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
	u8 pad_00[0x30];
	u16 if_flags;                 /* 0x30 */
	u8 pad_32[0x56];
	u32 if_obytes_lo;             /* 0x88 */
	u32 if_obytes_hi;             /* 0x8c */
	u8 pad_90[0x28];
	void (*if_start)(void *ifp);  /* 0xb8 */
	u8 pad_bc[0x10];
	AdhocMbuf *snd_head;          /* 0xcc */
	AdhocMbuf *snd_tail;          /* 0xd0 */
	s32 snd_len;                  /* 0xd4 */
	s32 snd_maxlen;               /* 0xd8 */
	s32 snd_drops;                /* 0xdc */
	u8 *if_sadl;                  /* 0xe0 */
} AdhocIfnet;

/* Waiter node on sock->wait_list (linked via +0x58) */
typedef struct AdhocWaiter {
	u8 pad_00[0x20];
	u32 wait_chan;                /* 0x20 */
	u8 pad_24[0x28];
	s32 sema_id;                  /* 0x4c */
	u8 pad_50[0x08];
	struct AdhocWaiter *next;     /* 0x58 */
} AdhocWaiter;

/* Socket descriptor (0x28 bytes) allocated by sub_0f60 */
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

/* PDP fragment reassembly node (0x18 bytes) */
typedef struct PdpFragEntry {
	struct PdpFragEntry *next;    /* 0x00 */
	struct PdpFragEntry **prev;   /* 0x04 */
	u8 src_mac[6];                /* 0x08 */
	u16 sport;                    /* 0x0e */
	u16 seq;                      /* 0x10 */
	u16 cur_off;                  /* 0x12 */
	AdhocMbuf *m_head;            /* 0x14 */
} PdpFragEntry;

/* PDP PCB (0x3c bytes) allocated by sub_2a9c */
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
	u32 pad_2c;                   /* 0x2c */
	PdpFragEntry *frag_head;      /* 0x30 */
	PdpFragEntry **frag_tail;     /* 0x34 */
	u32 frag_count;               /* 0x38 */
} PdpPcb;

/* PTP PCB (0xf0 bytes) allocated by sub_4a30 */
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
	u32 pad_a4;                   /* 0xa4 */
	u8 timer_probe[0x1c];         /* 0xa8..0xc3 */
	u32 timer_probe_active;       /* 0xc4 */
	u32 pad_c8;                   /* 0xc8 */
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
	u16 pad_12;                   /* 0x12 */
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

/* Forward declarations */
static void sub_0270(void);
static s32 sub_036c(void *ifp, void *m);
static s32 sub_047c(s32 a0, s32 a1, void **out);
static void sub_0490(void *ifp);
static s32 sub_0974(void *arr, s32 count);
static void sub_0dd0(void);
static void sub_0e18(void);
static AdhocSocket *sub_0e90(s32 type, s32 id);
static s32 sub_0ed8(void);
static s32 sub_0f60(s32 type, AdhocSocket **out_sock);
static void sub_0ffc(AdhocSocket *sock);
static s32 sub_1044(void);
static void sub_1078(AdhocSocket *sock, s32 flags);
static s32 sub_1620(PdpPcb *pdp, const u8 *dst_mac, u16 dport, const void *data, s32 len, u32 timeout, s32 flags);
static s32 sub_1c48(PdpPcb *pdp, u8 *src_mac, u16 *sport, void *data, s32 *data_len, u32 timeout, s32 flags);
static s32 sub_20c4(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 flags);
static void sub_225c(PdpFragEntry *fe);
static void sub_231c(AdhocMbuf *m);
static void sub_29ac(void);
static void sub_2a18(void);
static s32 sub_2a9c(PdpPcb **out_pdp);
static s32 sub_2b2c(PdpPcb *pdp, const u8 *mac, u16 port, u32 bufsize);
static void sub_2d04(PdpPcb *pdp);
static s32 sub_2d50(void);
static void sub_2d5c(void *ifp);
static s32 sub_2dc4(AdhocSocket *sock, s32 events);
static u16 sub_2e78(void);
static s32 sub_3afc(const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 queue_max, s32 mode);
static s32 sub_3c44(s32 id, u32 timeout, s32 nonblock);
static s32 sub_3c7c(PtpPcb *ptp, u32 timeout, s32 flags);
static s32 sub_3e30(PtpPcb *ptp, u8 *peer_mac, u16 *peer_port, u32 timeout, s32 flags, s32 *out_id);
static s32 sub_40ec(PtpPcb *ptp, const void *data, s32 *len, u32 timeout, s32 flags);
static s32 sub_44d4(PtpPcb *ptp, void *data, s32 *len, u32 timeout, s32 flags);
static s32 sub_4808(PtpPcb *ptp, u32 timeout, s32 flags);
static s32 sub_4a30(PtpPcb **out_ptp);
static void sub_4ad4(PtpPcb *ptp);
static s32 sub_4b7c(PtpPcb *ptp, const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 queue_max, s32 mode);
static void sub_4d48(PtpPcb *ptp);
static void sub_4e3c(PtpPcb *ptp);
static s32 sub_4fc8(AdhocSocket *sock, s32 events);
static void sub_5288(void);
static void sub_52e0(void);
static PtpPcb *sub_536c(const u8 *local_mac, u16 local_port, const u8 *peer_mac, u16 peer_port);
static void sub_542c(PtpPcb *ptp);
static void sub_5590(PtpPcb *ptp);
static void sub_55dc(PtpPcb *ptp);
static s32 sub_55f8(void);
static void sub_5604(void *ifp);
static void sub_572c(void *ifp);
static void sub_5750(PtpPcb *ptp);
static u16 sub_584c(void);
static void sub_5938(AdhocMbuf *m);
static void sub_637c(const u8 *src_mac, u16 sport, const u8 *dst_mac, u16 dport, u8 type, u32 seq_or_ack, AdhocMbuf *payload_m);
static void sub_668c(PtpPcb *ptp, u8 type);
static void sub_6878(PtpPcb *ptp, s32 rexmt_only);
static void sub_6a54(PtpPcb *ptp);
static void sub_6b20(PtpPcb *ptp);
static s32 sub_7008(void);
static void sub_7020(void);
static s32 sub_7188(const u8 *mac, void *buf, u32 size);
static s32 sub_72a4(s32 id);
static s32 sub_7338(s32 id, void *buf, s32 mode);

/* Read-only constants at 0x7984..0x7998 */
static const u16 g_adhoc_ethertype = 0xC888; /* 0x7984 */
static const u16 g_adhoc_hdr_ver = 0x0200;   /* 0x7988 */
static const u8 g_bcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* 0x798c */
static const u8 g_zero_mac[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };  /* 0x7994 */

/* .data (0x7a50) */
static s32 g_gm_master_id = -1;       /* 0x7a50 */

/* .bss (0x7a60..0x7b40) */
static s32 g_adhoc_inited;            /* 0x7a60 */
static struct {
	AdhocMbuf *head;              /* 0x7a64 */
	AdhocMbuf *tail;              /* 0x7a68 */
	s32 count;                    /* 0x7a6c */
	s32 max_count;                /* 0x7a70 */
	s32 drops;                    /* 0x7a74 */
} g_rx_queue;

static struct {
	AdhocSocket *head;            /* 0x7a78 */
	AdhocSocket **tail;           /* 0x7a7c */
} g_sock_list;

static s32 g_last_sock_id;            /* 0x7a80 */

static struct {
	PdpPcb *head;                 /* 0x7a84 */
	PdpPcb **tail;                /* 0x7a88 */
} g_pdp_list;

static u16 g_pdp_anon_port;           /* 0x7a8c */
static u16 g_ptp_anon_port;           /* 0x7a8e */

static struct {
	PtpPcb *head;                 /* 0x7a90 */
	PtpPcb **tail;                /* 0x7a94 */
} g_ptp_list;

static u32 g_ptp_initial_seq;         /* 0x7a98 */

static struct {
	GameModeNode *head;           /* 0x7a9c */
	GameModeNode **tail;          /* 0x7aa0 */
} g_gm_list;

static u32 g_global_wait_head;        /* 0x7aa4 */
static u32 g_pdp_stats[6];            /* 0x7aa8..0x7abf */
static u16 g_pdp_seq;                 /* 0x7ac0 */
static u32 g_ptp_stats[31];           /* 0x7ac4..0x7b3f */

/* === sceNetAdhocInit @ 0x0000 === */
s32 sceNetAdhocInit(void)
{
	s32 intr;
	s32 ret;

	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited > 0) {
		ret = 0x80410713;
		goto out;
	}

	g_rx_queue.head = NULL;
	g_rx_queue.tail = NULL;
	g_rx_queue.count = 0;
	g_rx_queue.max_count = 0x32;
	g_rx_queue.drops = 0;

	sub_0dd0();
	sub_29ac();
	sub_5288();

	ret = sceNet_lib_B3A48B7F(sub_0270, sub_0490);
	if (ret >= 0) {
		ret = sceNet_lib_1F94AFD9(sub_047c);
		if (ret >= 0) {
			ret = sub_7008();
			if (ret >= 0) {
				g_adhoc_inited = 1;
				ret = 0;
				goto out;
			}
			sceNet_lib_0A5A8751();
		}
		sceNet_lib_949F1FBB();
	}
	sub_52e0();
	sub_2a18();
	sub_0e18();

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhocTerm @ 0x0148 === */
s32 sceNetAdhocTerm(void)
{
	s32 intr;
	s32 ret;
	AdhocMbuf *m;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	ret = 0;
	if (g_adhoc_inited != 0) {
		if (sub_1044() != 0 || sub_2d50() != 0 || sub_55f8() != 0) {
			ret = 0x80410714;
		} else {
			sub_7020();
			sceNet_lib_0A5A8751();
			sceNet_lib_949F1FBB();
			sub_52e0();
			sub_2a18();
			sub_0e18();
			for (;;) {
				m = g_rx_queue.head;
				if (m == NULL)
					break;
				g_rx_queue.head = m->m_nextpkt;
				if (g_rx_queue.head == NULL)
					g_rx_queue.tail = NULL;
				g_rx_queue.count--;
				m->m_nextpkt = NULL;
				sceNetIfhandle_lib_F56FAC82(m);
			}
			g_adhoc_inited = 0;
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sub_0270 @ 0x0270 === */
static void sub_0270(void)
{
	AdhocMbuf *m;
	u8 proto;

	for (;;) {
		m = g_rx_queue.head;
		if (m == NULL)
			return;
		g_rx_queue.head = m->m_nextpkt;
		if (g_rx_queue.head == NULL)
			g_rx_queue.tail = NULL;
		g_rx_queue.count--;
		m->m_nextpkt = NULL;

		if (m->m_pkthdr_len < 0x31) {
			sceNetIfhandle_lib_F56FAC82(m);
			continue;
		}
		if (m->m_len < 0x31) {
			sceNetIfhandle_lib_9A6261EC(m, 0x30, 1, &proto);
		} else {
			proto = m->m_data[0x30];
		}
		if (proto == 1) {
			sub_231c(m);
		} else if (proto == 2) {
			sub_5938(m);
		} else {
			sceNetIfhandle_lib_F56FAC82(m);
		}
	}
}

/* === sub_036c @ 0x036c === */
static s32 sub_036c(void *ifp_arg, void *m_arg)
{
	AdhocIfnet *ifp = (AdhocIfnet *)ifp_arg;
	AdhocMbuf *m = (AdhocMbuf *)m_arg;
	u8 *sadl;

	sadl = ifp->if_sadl;
	if (sceNet_lib_E0A81C7C(m->m_data, sadl + sadl[5] + 8, 6) == 0) {
		m->m_rcvif = ifp;
		if (g_rx_queue.count >= g_rx_queue.max_count) {
			g_rx_queue.drops++;
			sceNetIfhandle_lib_F56FAC82(m);
			return 0;
		}
		m->m_nextpkt = NULL;
		if (g_rx_queue.tail == NULL)
			g_rx_queue.head = m;
		else
			g_rx_queue.tail->m_nextpkt = m;
		g_rx_queue.tail = m;
		g_rx_queue.count++;
		sceNet_lib_05D525E4(0x1d);
		return 0;
	}

	m->m_flags |= 0x8000;
	m->m_nextpkt = NULL;
	if (ifp->snd_tail == NULL)
		ifp->snd_head = m;
	else
		ifp->snd_tail->m_nextpkt = m;
	ifp->snd_tail = m;
	ifp->snd_len++;
	if (!(ifp->if_flags & 0x400))
		ifp->if_start(ifp);
	return 0;
}

/* === sub_047c @ 0x047c === */
static s32 sub_047c(s32 a0, s32 a1, void **out)
{
	(void)a0;
	(void)a1;
	*out = &g_rx_queue;
	return 0;
}

/* === sub_0490 @ 0x0490 === */
static void sub_0490(void *ifp)
{
	sub_2d5c(ifp);
	sub_5604(ifp);
	sub_572c(ifp);
}

/* === sceNetAdhoc_7A662D6B @ 0x04ac === */
s32 sceNetAdhoc_7A662D6B(SceNetAdhocPollSd *sds, s32 count, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	u32 rem_timeout;
	void **arr;
	s32 i, j;
	AdhocSocket *s;
	AdhocWaiter *w = NULL;

	rem_timeout = timeout;
	if (_sceKernelCheckThreadStack() < 0x7b0)
		return 0x80410005;
	if (sds == NULL)
		return 0x80410708;
	if (count <= 0)
		return 0x80410710;

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	arr = (void **)sceNetIfhandle_lib_15CFE3C0((u32)(count << 3));
	if (arr == NULL) {
		ret = 0x80410709;
		goto out;
	}

	for (i = 0; i < count; i++) {
		s = sub_0e90(0, sds[i].id);
		if (s == NULL) {
			ret = 0x80410707;
			goto free_arr;
		}
		for (j = 0; j < i; j++) {
			if ((AdhocSocket *)arr[j * 2] == s) {
				ret = 0x80410708;
				goto free_arr;
			}
		}
		arr[i * 2] = s;
		arr[i * 2 + 1] = &sds[i];
	}

	for (;;) {
		ret = sub_0974(arr, count);
		if (ret > 0 || nonblock != 0)
			break;

		for (i = 0; i < count; i++) {
			s = (AdhocSocket *)arr[i * 2];
			s32 ev = ((SceNetAdhocPollSd *)arr[i * 2 + 1])->events;
			if (s->type == 1) {
				if (ev & 1)
					s->flags |= 2;
				if (ev & 2)
					s->flags |= 1;
			} else if (s->type == 2) {
				PtpPcb *ptp = (PtpPcb *)s->pcb;
				if (ptp != NULL) {
					if (ptp->mode == 1) {
						if (ev & 4)
							ptp->flags |= 4;
						if (ev & 1)
							ptp->tx_flags |= 2;
						if (ev & 2)
							ptp->rx_flags |= 2;
					} else if (ptp->mode == 2) {
						if (ev & 8)
							ptp->q_flags |= 2;
					}
				}
			}
		}

		s = (AdhocSocket *)arr[0];
		for (i = 0; i < count; i++) {
			AdhocSocket *cur = (AdhocSocket *)arr[i * 2];
			cur->flags &= ~0x80;
		}

		ret = sceNet_lib_F6DB0A0B(&g_global_wait_head, &rem_timeout);

		for (i = 0; i < count; i++) {
			s = (AdhocSocket *)arr[i * 2];
			if (s->type == 1) {
				s->flags &= ~3;
			} else if (s->type == 2) {
				PtpPcb *ptp = (PtpPcb *)s->pcb;
				if (ptp != NULL) {
					if (ptp->mode == 1) {
						ptp->flags &= ~4;
						ptp->tx_flags &= ~2;
						ptp->rx_flags &= ~2;
					} else if (ptp->mode == 2) {
						ptp->q_flags &= ~2;
					}
				}
			}
		}

		for (i = 0; i < count; i++) {
			s = (AdhocSocket *)arr[i * 2];
			if (s->flags & 0x80) {
				s->flags &= ~0x80;
				ret = 0x8041070c;
				goto cleanup_socks;
			}
		}

		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				ret = 0;
			else if ((u32)ret == 0x800201a4)
				ret = 0x80410718;
			break;
		}
	}

cleanup_socks:
	for (i = 0; i < count; i++) {
		s = (AdhocSocket *)arr[i * 2];
		sub_0ffc(s);
	}

free_arr:
	sceNetIfhandle_lib_76BAD213(arr);

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sub_0974 @ 0x0974 === */
static s32 sub_0974(void *arr_arg, s32 count)
{
	void **arr = (void **)arr_arg;
	s32 ready = 0;
	s32 i;

	for (i = 0; i < count; i++) {
		AdhocSocket *s = (AdhocSocket *)arr[i * 2];
		SceNetAdhocPollSd *sd = (SceNetAdhocPollSd *)arr[i * 2 + 1];
		s32 rev = 0;

		if (s->type == 1) {
			rev = sub_2dc4(s, sd->events);
		} else if (s->type == 2) {
			rev = sub_4fc8(s, sd->events);
		}
		sd->revents = rev;
		if (rev != 0)
			ready++;
	}
	return ready;
}

/* === sceNetAdhoc_73BFD52D @ 0x0a2c === */
s32 sceNetAdhoc_73BFD52D(s32 id, s32 flag)
{
	s32 intr;
	s32 ret;
	AdhocSocket *s;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if ((flag & ~0x1f) != 0)
		return 0x80410708;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	if (flag & 0x10) {
		for (s = g_sock_list.head; s != NULL; s = s->next) {
			if (s->pcb != NULL) {
				s->flags = (s->flags & ~0xff) | (u32)(flag & 0xff);
				if (flag != 0)
					sceNet_lib_6B294EE4(s, flag);
			}
		}
		ret = 0;
	} else {
		s = sub_0e90(0, id);
		if (s == NULL) {
			ret = 0x80410707;
		} else {
			s->flags = (s->flags & ~0xff) | (u32)(flag & 0xff);
			if (flag != 0)
				sceNet_lib_6B294EE4(s, flag);
			ret = 0;
		}
	}

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhoc_4D2CE199 @ 0x0c58 === */
s32 sceNetAdhoc_4D2CE199(s32 id, s32 *flag)
{
	s32 intr;
	s32 ret;
	AdhocSocket *s;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (flag == NULL)
		return 0x80410708;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
	} else {
		s = sub_0e90(0, id);
		if (s == NULL) {
			ret = 0x80410707;
		} else {
			*flag = (s32)(s->flags & 0xff);
			ret = 0;
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sub_0dd0 @ 0x0dd0 === */
static void sub_0dd0(void)
{
	g_sock_list.head = NULL;
	g_sock_list.tail = &g_sock_list.head;
	sceNet_lib_2E005032(sub_1078);
	g_last_sock_id = 0;
}

/* === sub_0e18 @ 0x0e18 === */
static void sub_0e18(void)
{
	AdhocSocket *s;
	AdhocSocket *next;

	sceNet_lib_33B230BD();
	for (s = g_sock_list.head; s != NULL; s = next) {
		next = s->next;
		if (s->pcb == NULL) {
			if (s->next != NULL)
				s->next->prev = s->prev;
			else
				g_sock_list.tail = s->prev;
			*s->prev = s->next;
			sceNetIfhandle_lib_76BAD213(s);
		}
	}
}

/* === sub_0e90 @ 0x0e90 === */
static AdhocSocket *sub_0e90(s32 type, s32 id)
{
	AdhocSocket *s;

	for (s = g_sock_list.head; s != NULL; s = s->next) {
		if (s->id == id) {
			if (type == 0 || s->type == type)
				return s;
		}
	}
	return NULL;
}

/* === sub_0ed8 @ 0x0ed8 === */
static s32 sub_0ed8(void)
{
	s32 cand = g_last_sock_id + 1;
	s32 count = 0;

	if (cand >= 0xff)
		cand = 1;

	while (count < 0xfe) {
		if (sub_0e90(0, cand) == NULL) {
			g_last_sock_id = cand;
			return cand;
		}
		cand++;
		if (cand >= 0xff)
			cand = 1;
		count++;
	}
	return -1;
}

/* === sub_0f60 @ 0x0f60 === */
static s32 sub_0f60(s32 type, AdhocSocket **out_sock)
{
	AdhocSocket *s;

	s = (AdhocSocket *)sceNetIfhandle_lib_15CFE3C0(0x28);
	if (s == NULL)
		return 0x80410709;
	sceKernelMemset(s, 0, 0x28);
	s->id = -1;
	s->type = type;
	s->next = NULL;
	s->prev = g_sock_list.tail;
	*g_sock_list.tail = s;
	g_sock_list.tail = &s->next;
	*out_sock = s;
	return 0;
}

/* === sub_0ffc @ 0x0ffc === */
static void sub_0ffc(AdhocSocket *sock)
{
	if (sock->pcb == NULL && sock->wait_list == NULL) {
		if (sock->next != NULL)
			sock->next->prev = sock->prev;
		else
			g_sock_list.tail = sock->prev;
		*sock->prev = sock->next;
		sceNetIfhandle_lib_76BAD213(sock);
	}
}

/* === sub_1044 @ 0x1044 === */
static s32 sub_1044(void)
{
	AdhocSocket *s;

	for (s = g_sock_list.head; s != NULL; s = s->next) {
		if (s->wait_list != NULL)
			return 1;
	}
	return 0;
}

/* === sub_1078 @ 0x1078 === */
static void sub_1078(AdhocSocket *sock, s32 flags)
{
	AdhocWaiter *w;
	u32 chan;

	if (sock->wait_list == NULL)
		return;

	for (w = sock->wait_list; w != NULL; w = w->next) {
		chan = w->wait_chan;
		if (((flags & 1) && chan == sock->chan_rd) ||
		    ((flags & 2) && chan == sock->chan_wr) ||
		    ((flags & 4) && chan == sock->chan_ex) ||
		    ((flags & 8) && chan == (u32)&g_global_wait_head)) {
			if (chan == (u32)&g_global_wait_head)
				sock->flags |= 0x80;
			sceKernelSignalSema(w->sema_id, 1);
		}
	}
}

/* === sceNetAdhocPdpCreate @ 0x1130 === */
s32 sceNetAdhocPdpCreate(const u8 *mac, u16 port, u32 bufsize, s32 flags)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	PdpPcb *pdp;
	s32 id;
	u32 delay;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (mac == NULL)
		return 0x80410701;
	if (bufsize == 0)
		return 0x80410705;
	if (flags != 0)
		return 0x80410708;

	delay = sceNetIfhandle_lib_07505747(3);
	if (delay != 0)
		sceKernelDelayThread(delay);

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	ret = sub_0f60(1, &sock);
	if (ret < 0)
		goto out;

	ret = sub_2a9c(&pdp);
	if (ret < 0) {
		sub_0ffc(sock);
		goto out;
	}

	ret = sub_2b2c(pdp, mac, port, bufsize);
	if (ret < 0) {
		sub_2d04(pdp);
		sub_0ffc(sock);
		goto out;
	}

	id = sub_0ed8();
	if (id < 0) {
		sub_2d04(pdp);
		sub_0ffc(sock);
		ret = 0x8041070b;
		goto out;
	}

	sock->id = id;
	sock->pcb = pdp;
	sock->chan_rd = (u32)&pdp->rx_head;
	sock->chan_wr = (u32)&pdp->state_flags;
	sock->chan_ex = 0;
	pdp->sock = sock;
	ret = id;

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhocPdpSend @ 0x12ac === */
s32 sceNetAdhocPdpSend(s32 id, const u8 *dst_mac, u16 dport, const void *data, s32 len, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7c0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (dst_mac == NULL)
		return 0x80410701;
	if (dport == 0)
		return 0x80410702;
	if (len < 0 || len > 0xffff)
		return 0x80410705;
	if (len > 0 && data == NULL)
		return 0x80410706;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	sock = sub_0e90(1, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	ret = sub_1620((PdpPcb *)sock->pcb, dst_mac, dport, data, len, timeout, nonblock);
	sub_0ffc(sock);

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPdpDelete @ 0x1470 === */
s32 sceNetAdhocPdpDelete(s32 id, s32 flags)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	PdpPcb *pdp;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (flags != 0)
		return 0x80410708;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	sock = sub_0e90(1, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	pdp = (PdpPcb *)sock->pcb;
	sock->pcb = NULL;
	pdp->sock = NULL;
	sub_2d04(pdp);

	if (sock->wait_list != NULL) {
		if (sock->flags & 1)
			sceNet_lib_7574FDA1((void *)sock->chan_rd);
		if (sock->flags & 2)
			sceNet_lib_7574FDA1((void *)sock->chan_wr);
		sceNet_lib_7574FDA1(&g_global_wait_head);
	}
	sub_0ffc(sock);
	ret = 0;

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sub_1620 @ 0x1620 === */
static s32 sub_1620(PdpPcb *pdp, const u8 *dst_mac, u16 dport, const void *data, s32 len, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	AdhocIfnet *ifp;
	s32 frag_cnt;
	u16 seq;
	s32 cur_off;
	s32 rem_len;
	s32 ret;

	if ((dst_mac[0] & 1) && sceNet_lib_E0A81C7C(dst_mac, g_bcast_mac, 6) != 0)
		return 0x80410701;

	ifp = (AdhocIfnet *)sceNet_lib_D60225A3();
	if (ifp == NULL)
		return 0x80410715;

	frag_cnt = (len + 0x54f) / 0x550;
	if (frag_cnt == 0)
		frag_cnt = 1;

	if (frag_cnt > ifp->snd_maxlen)
		return 0x80410705;

	while (ifp->snd_maxlen - ifp->snd_len < frag_cnt) {
		if (flags & 1)
			return 0x80410709;
		pdp->state_flags |= 1;
		ret = sceNet_lib_F6DB0A0B(&pdp->state_flags, &rem_timeout);
		if (pdp->sock == NULL)
			return 0x8041070c;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
		ifp = (AdhocIfnet *)sceNet_lib_D60225A3();
		if (ifp == NULL)
			return 0x80410715;
	}

	seq = g_pdp_seq++;
	cur_off = 0;
	rem_len = len;

	do {
		s32 chunk = rem_len;
		u16 off_field;
		s32 tot_body;
		s32 pad_len;
		s32 alloc_len;
		AdhocMbuf *m;
		u8 *hdr;
		u16 tmp16;

		if (chunk > 0x550) {
			chunk = 0x550;
			off_field = (u16)(cur_off | 0x8000);
		} else {
			off_field = (u16)cur_off;
		}

		tot_body = chunk + 8;
		pad_len = tot_body & 0x0f;
		if (pad_len > 0)
			pad_len = 0x10 - pad_len;
		alloc_len = chunk + pad_len;

		m = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
		if (m == NULL) {
			g_pdp_stats[5]++;
			return 0x80410709;
		}
		m->m_next = NULL;
		m->m_nextpkt = NULL;
		m->m_data = (u8 *)m + 0x30;
		m->m_type = 1;
		m->m_flags = 2;
		m->m_pkthdr_Aux = 0;

		if (alloc_len + 0x38 >= 0xd1) {
			void *ext = sceNetIfhandle_lib_15CFE3C0(0x800);
			if (ext == NULL)
				ext = sceNetIfhandle_lib_15CFE3C0(0x800);
			*(void **)((u8 *)m + 0x30) = ext;
			if (ext != NULL) {
				m->m_data = (u8 *)ext;
				m->m_flags |= 9;
				*(u32 *)((u8 *)m + 0x3c) = 0x800;
				*(u32 *)((u8 *)m + 0x34) = 0;
				*(u32 *)((u8 *)m + 0x38) = 0;
				*(AdhocMbuf **)((u8 *)m + 0x48) = m;
				*(AdhocMbuf **)((u8 *)m + 0x44) = m;
			}
			if (!(m->m_flags & 1)) {
				sceNetIfhandle_lib_F8825DC4(m);
				g_pdp_stats[5]++;
				return 0x80410709;
			}
		}

		hdr = m->m_data;
		sceKernelMemcpy(hdr, dst_mac, 6);
		sceKernelMemcpy(hdr + 6, pdp->mac, 6);
		sceKernelMemcpy(hdr + 0x0c, &g_adhoc_ethertype, 2);
		sceKernelMemcpy(hdr + 0x0e, &g_adhoc_hdr_ver, 2);

		hdr = m->m_data;
		hdr[0x30] = 1;
		hdr[0x31] = 0;
		tmp16 = bswap16((u16)tot_body);
		sceKernelMemcpy(hdr + 0x32, &tmp16, 2);
		tmp16 = bswap16(pdp->port);
		sceKernelMemcpy(hdr + 0x34, &tmp16, 2);
		tmp16 = bswap16(dport);
		sceKernelMemcpy(hdr + 0x36, &tmp16, 2);
		tmp16 = bswap16(seq);
		sceKernelMemcpy(hdr + 0x38, &tmp16, 2);
		tmp16 = bswap16(off_field);
		sceKernelMemcpy(hdr + 0x3a, &tmp16, 2);

		if (chunk > 0)
			sceKernelMemcpy(m->m_data + 0x3c, (const u8 *)data + cur_off, (u32)chunk);
		if (pad_len > 0)
			sceKernelMemset(m->m_data + 0x3c + chunk, 0, (u32)pad_len);

		ret = sceNetAdhocAuth_lib_AAB06250(m->m_data + 0x10, (u32)(alloc_len + 0x2c));
		if (ret < 0) {
			sceNetIfhandle_lib_F8825DC4(m);
			return ret;
		}

		m->m_rcvif = NULL;
		m->m_pkthdr_len = alloc_len + 0x3c;
		m->m_len = alloc_len + 0x3c;
		{
			u32 pkt_len = (u32)(alloc_len + 0x3c);
			u32 new_lo = ifp->if_obytes_lo + pkt_len;
			u32 carry = (new_lo < pkt_len) ? 1 : 0;
			ifp->if_obytes_lo = new_lo;
			ifp->if_obytes_hi += carry;
		}
		sub_036c(ifp, m);
		g_pdp_stats[4]++;

		cur_off += chunk;
		rem_len -= chunk;
	} while (rem_len > 0);

	return 0;
}

/* === sub_1c48 @ 0x1c48 === */
static s32 sub_1c48(PdpPcb *pdp, u8 *src_mac, u16 *sport, void *data, s32 *data_len, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	AdhocMbuf *m;
	AdhocMbuf *cur;
	s32 pkt_len;
	u8 hdr_buf[8];
	s32 copy_off;
	s32 ret;

	for (;;) {
		m = pdp->rx_head;
		if (m != NULL)
			break;
		if (flags & 1)
			return 0x80410709;
		pdp->state_flags |= 2;
		ret = sceNet_lib_F6DB0A0B(&pdp->rx_head, &rem_timeout);
		if (pdp->sock == NULL)
			return 0x8041070c;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
	}

	pkt_len = m->m_pkthdr_len - 8;
	if (*data_len < pkt_len) {
		*data_len = pkt_len;
		return 0x8041070a;
	}

	pdp->rx_head = m->m_nextpkt;
	if (pdp->rx_head == NULL)
		pdp->rx_tail = NULL;
	pdp->rx_cc -= (u32)pkt_len;
	m->m_nextpkt = NULL;

	if (m->m_len < 8) {
		sceNetIfhandle_lib_9A6261EC(m, 0, 8, hdr_buf);
		sceKernelMemcpy(src_mac, hdr_buf, 6);
		sceKernelMemcpy(sport, hdr_buf + 6, 2);
	} else {
		sceKernelMemcpy(src_mac, m->m_data, 6);
		sceKernelMemcpy(sport, m->m_data + 6, 2);
	}
	sceNetIfhandle_lib_BFF3CEA5(m, 8);

	if (flags & 2) {
		*(AdhocMbuf **)data = m;
		*data_len = pkt_len;
		return 0;
	}

	copy_off = 0;
	for (cur = m; cur != NULL; cur = cur->m_next) {
		if (cur->m_len > 0) {
			sceKernelMemcpy((u8 *)data + copy_off, cur->m_data, (u32)cur->m_len);
			copy_off += cur->m_len;
		}
	}
	*data_len = copy_off;
	sceNetIfhandle_lib_F56FAC82(m);
	return 0;
}

/* === sceNetAdhocPdpRecv @ 0x1f30 === */
s32 sceNetAdhocPdpRecv(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 nonblock)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (src_mac == NULL || sport == NULL || buf == NULL || len == NULL)
		return 0x80410706;
	if (*len <= 0)
		return 0x80410705;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	return sub_20c4(id, src_mac, sport, buf, len, timeout, nonblock);
}

/* === sceNetAdhoc_67346A2A @ 0x2004 === */
s32 sceNetAdhoc_67346A2A(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 nonblock)
{
	if ((nonblock & ~1) != 0)
		return 0x80410708;
	return sub_20c4(id, src_mac, sport, buf, len, timeout, nonblock | 2);
}

/* === sceNetAdhoc_lib_F9240FBA @ 0x2060 === */
void sceNetAdhoc_lib_F9240FBA(void)
{
	sceKernelMemset(g_pdp_stats, 0, 0x18);
}

/* === sceNetAdhoc_lib_1059683E @ 0x2088 === */
s32 sceNetAdhoc_lib_1059683E(void *dst)
{
	if (dst == NULL)
		return 0x80410708;
	sceKernelMemcpy(dst, g_pdp_stats, 0x18);
	return 0;
}

/* === sub_20c4 @ 0x20c4 === */
static s32 sub_20c4(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 flags)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	AdhocWaiter *w = NULL;

	if (id <= 0)
		return 0x80410707;
	if (src_mac == NULL || sport == NULL || buf == NULL || len == NULL)
		return 0x80410706;
	if (*len <= 0)
		return 0x80410705;

	if ((flags & 1) == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	sock = sub_0e90(1, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	ret = sub_1c48((PdpPcb *)sock->pcb, src_mac, sport, buf, len, timeout, flags);
	sub_0ffc(sock);

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sub_225c @ 0x225c === */
static void sub_225c(PdpFragEntry *fe)
{
	AdhocMbuf *m;
	AdhocMbuf *next;

	for (m = fe->m_head; m != NULL; m = next) {
		next = m->m_nextpkt;
		m->m_nextpkt = NULL;
		sceNetIfhandle_lib_F56FAC82(m);
	}
	sceNetIfhandle_lib_76BAD213(fe);
}

/* === sub_231c @ 0x231c === */
static void sub_231c(AdhocMbuf *m)
{
	u8 dst_mac[6];
	u8 src_mac[6];
	u16 body_len;
	u16 sport;
	u16 dport;
	u16 seq;
	u16 off_field;
	u16 frag_off;
	s32 payload_len;
	PdpPcb *pdp;
	u8 *hdr;
	s32 is_bcast;

	if (m->m_pkthdr_len < 0x3c) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[0]++;
		return;
	}
	if (m->m_len < 0x3c) {
		m = (AdhocMbuf *)sceNetIfhandle_lib_49EDBB18(m, 0x3c);
		if (m == NULL) {
			g_pdp_stats[0]++;
			return;
		}
	}

	hdr = m->m_data;
	if (hdr[0x31] != 0) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[0]++;
		return;
	}

	sceKernelMemcpy(&body_len, hdr + 0x32, 2);
	body_len = bswap16(body_len);
	if (body_len < 8 || m->m_pkthdr_len < (s32)body_len + 0x34) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[0]++;
		return;
	}

	payload_len = (s32)body_len - 8;
	sceKernelMemcpy(dst_mac, hdr, 6);
	sceKernelMemcpy(src_mac, hdr + 6, 6);
	sceKernelMemcpy(&sport, hdr + 0x34, 2);
	sport = bswap16(sport);
	sceKernelMemcpy(&dport, hdr + 0x36, 2);
	dport = bswap16(dport);
	sceKernelMemcpy(&seq, hdr + 0x38, 2);
	seq = bswap16(seq);
	sceKernelMemcpy(&off_field, hdr + 0x3a, 2);
	off_field = bswap16(off_field);

	if (sport == 0 || dport == 0) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[0]++;
		return;
	}

	is_bcast = (sceNet_lib_E0A81C7C(dst_mac, g_bcast_mac, 6) == 0);
	for (pdp = g_pdp_list.head; pdp != NULL; pdp = pdp->next) {
		if (pdp->port == dport) {
			if (is_bcast || sceNet_lib_E0A81C7C(pdp->mac, dst_mac, 6) == 0)
				break;
		}
	}
	if (pdp == NULL) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[1]++;
		return;
	}

	if (sceNetAdhocAuth_lib_015A8A64(m->m_data + 0x10, (u32)(m->m_pkthdr_len - 0x10)) < 0) {
		sceNetIfhandle_lib_F56FAC82(m);
		return;
	}

	{
		s32 pad_trim = (payload_len + 0x3c) - m->m_pkthdr_len;
		if (pad_trim < 0)
			sceNetIfhandle_lib_BFF3CEA5(m, pad_trim);
	}

	frag_off = (u16)(off_field & 0x7fff);
	if (frag_off == 0 && !(off_field & 0x8000)) {
		PdpFragEntry *fe;
		PdpFragEntry *next_fe;

		for (fe = pdp->frag_head; fe != NULL; fe = next_fe) {
			next_fe = fe->next;
			if (sceNet_lib_E0A81C7C(fe->src_mac, src_mac, 6) == 0 && fe->sport == sport) {
				if (fe->next != NULL)
					fe->next->prev = fe->prev;
				else
					pdp->frag_tail = fe->prev;
				*fe->prev = fe->next;
				pdp->frag_count--;
				sub_225c(fe);
			}
		}

		sceNetIfhandle_lib_BFF3CEA5(m, 0x34);
		sceKernelMemcpy(m->m_data, src_mac, 6);
		sceKernelMemcpy(m->m_data + 6, &sport, 2);
	} else {
		PdpFragEntry *fe;

		sceNetIfhandle_lib_BFF3CEA5(m, 0x3c);
		for (fe = pdp->frag_head; fe != NULL; fe = fe->next) {
			if (sceNet_lib_E0A81C7C(fe->src_mac, src_mac, 6) == 0 && fe->sport == sport)
				break;
		}

		if (frag_off == 0) {
			AdhocMbuf *hdr_m;

			if (fe != NULL) {
				if (fe->next != NULL)
					fe->next->prev = fe->prev;
				else
					pdp->frag_tail = fe->prev;
				*fe->prev = fe->next;
				pdp->frag_count--;
				sub_225c(fe);
			}

			if (pdp->frag_count >= 4) {
				PdpFragEntry *old = pdp->frag_head;
				if (old != NULL) {
					if (old->next != NULL)
						old->next->prev = old->prev;
					else
						pdp->frag_tail = old->prev;
					*old->prev = old->next;
					pdp->frag_count--;
					sub_225c(old);
				}
			}

			fe = (PdpFragEntry *)sceNetIfhandle_lib_15CFE3C0(0x18);
			if (fe == NULL) {
				sceNetIfhandle_lib_F56FAC82(m);
				g_pdp_stats[2]++;
				return;
			}
			sceKernelMemcpy(fe->src_mac, src_mac, 6);
			fe->sport = sport;
			fe->seq = seq;
			fe->cur_off = (u16)payload_len;

			hdr_m = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
			if (hdr_m == NULL) {
				sceNetIfhandle_lib_76BAD213(fe);
				sceNetIfhandle_lib_F56FAC82(m);
				g_pdp_stats[2]++;
				return;
			}
			hdr_m->m_next = m;
			hdr_m->m_nextpkt = NULL;
			hdr_m->m_data = (u8 *)hdr_m + 0x30;
			hdr_m->m_len = 8;
			hdr_m->m_type = 1;
			hdr_m->m_flags = 2;
			hdr_m->m_rcvif = NULL;
			hdr_m->m_pkthdr_len = payload_len + 8;
			hdr_m->m_pkthdr_Aux = 0;
			sceKernelMemcpy(hdr_m->m_data, src_mac, 6);
			sceKernelMemcpy(hdr_m->m_data + 6, &sport, 2);

			fe->m_head = hdr_m;
			fe->next = NULL;
			fe->prev = pdp->frag_tail;
			*pdp->frag_tail = fe;
			pdp->frag_tail = &fe->next;
			pdp->frag_count++;
			return;
		} else {
			if (fe == NULL || fe->seq != seq || fe->cur_off != frag_off ||
			    (s32)fe->cur_off + payload_len > 0xffff) {
				if (fe != NULL) {
					if (fe->next != NULL)
						fe->next->prev = fe->prev;
					else
						pdp->frag_tail = fe->prev;
					*fe->prev = fe->next;
					pdp->frag_count--;
					sub_225c(fe);
				}
				sceNetIfhandle_lib_F56FAC82(m);
				g_pdp_stats[0]++;
				return;
			}

			sceNetIfhandle_lib_3E8DD3F8(fe->m_head, m);
			fe->m_head->m_pkthdr_len += payload_len;
			fe->cur_off = (u16)(fe->cur_off + payload_len);

			if (off_field & 0x8000)
				return;

			m = fe->m_head;
			fe->m_head = NULL;
			if (fe->next != NULL)
				fe->next->prev = fe->prev;
			else
				pdp->frag_tail = fe->prev;
			*fe->prev = fe->next;
			pdp->frag_count--;
			sceNetIfhandle_lib_76BAD213(fe);
			payload_len = m->m_pkthdr_len - 8;
		}
	}

	if (pdp->rx_hiwat - pdp->rx_cc < (u32)payload_len) {
		pdp->rx_drops++;
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[2]++;
		return;
	}

	m->m_nextpkt = NULL;
	if (pdp->rx_tail == NULL)
		pdp->rx_head = m;
	else
		pdp->rx_tail->m_nextpkt = m;
	pdp->rx_tail = m;
	pdp->rx_cc += (u32)payload_len;

	if (pdp->state_flags & 2) {
		pdp->state_flags &= ~2;
		sceNet_lib_7574FDA1(&pdp->rx_head);
	}
	if (pdp->sock != NULL && (pdp->sock->flags & 1)) {
		pdp->sock->flags &= ~1;
		sceNet_lib_7574FDA1(&g_global_wait_head);
	}
	g_pdp_stats[3]++;
}

/* === sceNetAdhocGetPdpStat @ 0x2850 === */
s32 sceNetAdhocGetPdpStat(s32 *buflen, void *buf)
{
	s32 intr;
	s32 ret;
	PdpPcb *pdp;
	s32 total_bytes;
	u8 *out;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (buflen == NULL)
		return 0x80410708;
	if (*buflen < 0)
		return 0x80410705;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	if (buf == NULL) {
		total_bytes = 0;
		for (pdp = g_pdp_list.head; pdp != NULL; pdp = pdp->next)
			total_bytes += 0x14;
		*buflen = total_bytes;
		ret = 0;
	} else {
		s32 max_len = *buflen;
		out = (u8 *)buf;
		total_bytes = 0;
		for (pdp = g_pdp_list.head; pdp != NULL; pdp = pdp->next) {
			if (total_bytes + 0x14 > max_len)
				break;
			*(u32 *)(out + 0x00) = 0;
			*(s32 *)(out + 0x04) = (pdp->sock != NULL) ? pdp->sock->id : -1;
			sceKernelMemcpy(out + 0x08, pdp->mac, 6);
			*(u16 *)(out + 0x0e) = pdp->port;
			*(u32 *)(out + 0x10) = pdp->rx_cc;
			if (total_bytes >= 0x14)
				*(void **)(out - 0x14) = out;
			out += 0x14;
			total_bytes += 0x14;
		}
		*buflen = total_bytes;
		ret = 0;
	}

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sub_29ac @ 0x29ac === */
static void sub_29ac(void)
{
	g_pdp_list.head = NULL;
	g_pdp_list.tail = &g_pdp_list.head;
	g_pdp_anon_port = (u16)((sceNet_lib_1858883D() % 0xc000) + 0x4000);
	sceKernelMemset(g_pdp_stats, 0, 0x18);
	g_pdp_seq = (u16)(sceNet_lib_1858883D() / 0x8001);
}

/* === sub_2a18 @ 0x2a18 === */
static void sub_2a18(void)
{
	PdpPcb *pdp;

	while ((pdp = g_pdp_list.head) != NULL) {
		if (pdp->sock != NULL) {
			pdp->sock->pcb = NULL;
			sub_0ffc(pdp->sock);
			pdp->sock = NULL;
		}
		sub_2d04(pdp);
	}
}

/* === sub_2a9c @ 0x2a9c === */
static s32 sub_2a9c(PdpPcb **out_pdp)
{
	PdpPcb *pdp;

	pdp = (PdpPcb *)sceNetIfhandle_lib_15CFE3C0(0x3c);
	if (pdp == NULL)
		return 0x80410709;
	sceKernelMemset(pdp, 0, 0x3c);
	pdp->frag_head = NULL;
	pdp->frag_tail = &pdp->frag_head;
	pdp->next = NULL;
	pdp->prev = g_pdp_list.tail;
	*g_pdp_list.tail = pdp;
	g_pdp_list.tail = &pdp->next;
	*out_pdp = pdp;
	return 0;
}

/* === sub_2b2c @ 0x2b2c === */
static s32 sub_2b2c(PdpPcb *pdp, const u8 *mac, u16 port, u32 bufsize)
{
	u8 local_mac[8];
	PdpPcb *cur;
	s32 ret;

	ret = sceNetGetLocalEtherAddr(local_mac);
	if (ret < 0)
		return ret;
	if (sceNet_lib_E0A81C7C(mac, local_mac, 6) != 0)
		return 0x80410701;

	if (port == 0) {
		port = sub_2e78();
		if (port == 0)
			return 0x8041070b;
	} else {
		for (cur = g_pdp_list.head; cur != NULL; cur = cur->next) {
			if (cur != pdp && cur->port == port &&
			    sceNet_lib_E0A81C7C(cur->mac, mac, 6) == 0)
				return 0x80410703;
		}
	}

	sceKernelMemcpy(pdp->mac, mac, 6);
	pdp->port = port;
	pdp->rx_hiwat = bufsize;
	return 0;
}

/* === sub_2d04 @ 0x2d04 === */
static void sub_2d04(PdpPcb *pdp)
{
	AdhocMbuf *m;
	PdpFragEntry *fe;

	if (pdp->next != NULL)
		pdp->next->prev = pdp->prev;
	else
		g_pdp_list.tail = pdp->prev;
	*pdp->prev = pdp->next;

	while ((m = pdp->rx_head) != NULL) {
		pdp->rx_head = m->m_nextpkt;
		if (pdp->rx_head == NULL)
			pdp->rx_tail = NULL;
		m->m_nextpkt = NULL;
		sceNetIfhandle_lib_F56FAC82(m);
	}

	while ((fe = pdp->frag_head) != NULL) {
		if (fe->next != NULL)
			fe->next->prev = fe->prev;
		else
			pdp->frag_tail = fe->prev;
		*fe->prev = fe->next;
		pdp->frag_count--;
		sub_225c(fe);
	}

	sceNetIfhandle_lib_76BAD213(pdp);
}

/* === sub_2d50 @ 0x2d50 === */
static s32 sub_2d50(void)
{
	return (g_pdp_list.head != NULL) ? 1 : 0;
}

/* === sub_2d5c @ 0x2d5c === */
static void sub_2d5c(void *ifp_arg)
{
	AdhocIfnet *ifp = (AdhocIfnet *)ifp_arg;
	PdpPcb *pdp;

	for (pdp = g_pdp_list.head; pdp != NULL; pdp = pdp->next) {
		if (pdp->state_flags & 1) {
			pdp->state_flags &= ~1;
			sceNet_lib_7574FDA1(&pdp->state_flags);
		}
		if (pdp->sock != NULL && (pdp->sock->flags & 2)) {
			if (ifp->snd_len < ifp->snd_maxlen) {
				pdp->sock->flags &= ~2;
				sceNet_lib_7574FDA1(&g_global_wait_head);
			}
		}
	}
}

/* === sub_2dc4 @ 0x2dc4 === */
static s32 sub_2dc4(AdhocSocket *sock, s32 events)
{
	PdpPcb *pdp = (PdpPcb *)sock->pcb;
	s32 rev = 0;

	if (pdp == NULL)
		return 0x10;

	if (events & 1) {
		AdhocIfnet *ifp = (AdhocIfnet *)sceNet_lib_D60225A3();
		if (ifp == NULL)
			return 0x100;
		if (ifp->snd_len < ifp->snd_maxlen)
			rev |= 1;
	}
	if (events & 2) {
		if (pdp->rx_head != NULL)
			rev |= 2;
	}
	return rev;
}

/* === sub_2e78 @ 0x2e78 === */
static u16 sub_2e78(void)
{
	u16 cand = g_pdp_anon_port;
	s32 count = 0;
	PdpPcb *cur;

	while (count < 0x4000) {
		s32 used = 0;
		for (cur = g_pdp_list.head; cur != NULL; cur = cur->next) {
			if (cur->port == cand) {
				used = 1;
				break;
			}
		}
		cand++;
		if (cand < 0xc000)
			cand = 0xc000;
		if (!used) {
			g_pdp_anon_port = cand;
			return (u16)(cand == 0xc000 ? 0xffff : cand - 1);
		}
		count++;
	}
	return 0;
}

/* === sceNetAdhocPtpOpen @ 0x2eec === */
s32 sceNetAdhocPtpOpen(const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 flags)
{
	u32 delay;

	if (_sceKernelCheckThreadStack() < 0x7c0)
		return 0x80410005;
	if (src_mac == NULL || dst_mac == NULL)
		return 0x80410701;
	if (dst_port == 0)
		return 0x80410702;
	if (bufsize == 0 || rexmt_int == 0 || rexmt_cnt <= 0)
		return 0x80410705;
	if (flags != 0)
		return 0x80410708;

	delay = sceNetIfhandle_lib_07505747(0);
	if (delay != 0)
		sceKernelDelayThread(delay);

	return sub_3afc(src_mac, src_port, dst_mac, dst_port, bufsize, rexmt_int, rexmt_cnt, 0, 1);
}

/* === sceNetAdhocPtpConnect @ 0x3060 === */
s32 sceNetAdhocPtpConnect(s32 id, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	u32 delay;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	delay = sceNetIfhandle_lib_07505747(4);
	if (delay != 0)
		sceKernelDelayThread(delay);

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	sock = sub_0e90(2, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	ret = sub_3c7c((PtpPcb *)sock->pcb, timeout, nonblock);
	sub_0ffc(sock);

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPtpListen @ 0x31fc === */
s32 sceNetAdhocPtpListen(const u8 *src_mac, u16 src_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 backlog, s32 flags)
{
	if (_sceKernelCheckThreadStack() < 0x7c0)
		return 0x80410005;
	if (src_mac == NULL)
		return 0x80410701;
	if (src_port == 0)
		return 0x80410702;
	if (bufsize == 0 || rexmt_int == 0 || rexmt_cnt <= 0 || backlog <= 0)
		return 0x80410705;
	if (flags != 0)
		return 0x80410708;

	return sub_3afc(src_mac, src_port, g_zero_mac, 0, bufsize, rexmt_int, rexmt_cnt, backlog, 2);
}

/* === sceNetAdhocPtpAccept @ 0x3334 === */
s32 sceNetAdhocPtpAccept(s32 id, u8 *peer_mac, u16 *peer_port, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	s32 new_id = -1;
	u32 delay;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7d0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if ((peer_mac != NULL && peer_port == NULL) || (peer_mac == NULL && peer_port != NULL))
		return 0x80410706;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	delay = sceNetIfhandle_lib_07505747(2);
	if (delay != 0)
		sceKernelDelayThread(delay);

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	sock = sub_0e90(2, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	ret = sub_3e30((PtpPcb *)sock->pcb, peer_mac, peer_port, timeout, nonblock, &new_id);
	sub_0ffc(sock);
	if (ret == 0)
		ret = new_id;

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPtpSend @ 0x34ec === */
s32 sceNetAdhocPtpSend(s32 id, const void *data, s32 *len, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7d0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (data == NULL || len == NULL)
		return 0x80410706;
	if (*len <= 0)
		return 0x80410705;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	sock = sub_0e90(2, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	ret = sub_40ec((PtpPcb *)sock->pcb, data, len, timeout, nonblock);
	sub_0ffc(sock);

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPtpRecv @ 0x36a8 === */
s32 sceNetAdhocPtpRecv(s32 id, void *data, s32 *len, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7d0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (data == NULL || len == NULL)
		return 0x80410706;
	if (*len <= 0)
		return 0x80410705;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
	} else {
		sock = sub_0e90(2, id);
		if (sock == NULL || sock->pcb == NULL) {
			ret = 0x80410707;
		} else {
			ret = sub_44d4((PtpPcb *)sock->pcb, data, len, timeout, nonblock);
			sub_0ffc(sock);
		}
	}
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPtpFlush @ 0x3870 === */
s32 sceNetAdhocPtpFlush(s32 id, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
	} else {
		sock = sub_0e90(2, id);
		if (sock == NULL || sock->pcb == NULL) {
			ret = 0x80410707;
		} else {
			ret = sub_4808((PtpPcb *)sock->pcb, timeout, nonblock);
			sub_0ffc(sock);
		}
	}

	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPtpClose @ 0x3a00 === */
s32 sceNetAdhocPtpClose(s32 id, s32 flags)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (flags != 0)
		return 0x80410711;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else {
		sock = sub_0e90(2, id);
		if (sock == NULL) {
			ret = 0x80410701;
		} else {
			sub_4e3c((PtpPcb *)sock->pcb);
			sub_5750((PtpPcb *)sock->pcb);
			sock->flags |= 1;
			sock->id = -1;
			if (sock->wait_list == NULL) {
				sub_542c((PtpPcb *)sock->pcb);
				sock->pcb = NULL;
				sub_0ffc(sock);
			}
			ret = 0;
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sub_3afc @ 0x3afc === */
static s32 sub_3afc(const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 queue_max, s32 mode)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	PtpPcb *ptp;
	s32 id;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	ret = sub_0f60(2, &sock);
	if (ret < 0)
		goto out;

	ret = sub_4a30(&ptp);
	if (ret < 0) {
		sub_0ffc(sock);
		goto out;
	}

	ret = sub_4b7c(ptp, src_mac, src_port, dst_mac, dst_port, bufsize, rexmt_int, rexmt_cnt, queue_max, mode);
	if (ret < 0) {
		sub_542c(ptp);
		sub_0ffc(sock);
		goto out;
	}

	id = sub_0ed8();
	if (id < 0) {
		sub_542c(ptp);
		sub_0ffc(sock);
		ret = 0x8041070b;
		goto out;
	}

	sock->id = id;
	sock->pcb = ptp;
	if (mode == 2) {
		sock->chan_rd = (u32)&ptp->q_head;
		sock->chan_wr = 0;
		sock->chan_ex = 0;
	} else {
		sock->chan_rd = (u32)&ptp->rx_head;
		sock->chan_wr = (u32)&ptp->tx_head;
		sock->chan_ex = (u32)&ptp->state;
	}
	ptp->sock = sock;
	ret = id;

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sub_3c44 @ 0x3c44 === */
static s32 sub_3c44(s32 id, u32 timeout, s32 nonblock)
{
	return sceNetAdhocPtpFlush(id, timeout, nonblock);
}

/* === sub_3c7c @ 0x3c7c === */
static s32 sub_3c7c(PtpPcb *ptp, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	s32 ret;

	if (ptp->mode != 1)
		return 0x80410704;

	if (ptp->state == 0) {
		AdhocIfnet *ifp = (AdhocIfnet *)sceNet_lib_D60225A3();
		if (ifp == NULL)
			return 0x80410715;
		ptp->state = 2;
		sub_668c(ptp, 1);
		ptp->rexmt_rem = ptp->rexmt_max;
		sceNet_lib_A8B6205A(ptp->timer_rexmt, ptp->rexmt_intvl, sub_6a54, ptp);
	} else if (ptp->state == 4) {
		return 0;
	} else if (ptp->state != 2 && ptp->state != 3) {
		return 0x80410704;
	}

	while (ptp->state != 4) {
		if (ptp->state == 0)
			return 0x80410717;
		if (flags & 1)
			return 0x80410709;
		ptp->flags |= 2;
		ret = sceNet_lib_F6DB0A0B(&ptp->state, &rem_timeout);
		if (ptp->sock == NULL)
			return 0x8041070c;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
	}
	return 0;
}

/* === sub_3e30 @ 0x3e30 === */
static s32 sub_3e30(PtpPcb *ptp, u8 *peer_mac, u16 *peer_port, u32 timeout, s32 flags, s32 *out_id)
{
	u32 rem_timeout = timeout;
	PtpPcb *child;
	AdhocSocket *new_sock;
	s32 id;
	s32 ret;

	if (ptp->mode != 2 || ptp->state != 1)
		return 0x80410704;

	for (;;) {
		for (child = ptp->q_head; child != NULL; child = child->acc_next) {
			if (child->state == 4)
				break;
		}
		if (child != NULL)
			break;
		if (flags & 1)
			return 0x80410709;
		ptp->q_flags |= 1;
		ret = sceNet_lib_F6DB0A0B(&ptp->q_head, &rem_timeout);
		if (ptp->sock == NULL)
			return 0x8041070c;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
	}

	ret = sub_0f60(2, &new_sock);
	if (ret < 0)
		return ret;

	id = sub_0ed8();
	if (id < 0) {
		sub_0ffc(new_sock);
		return 0x8041070b;
	}

	if (child->acc_next != NULL)
		child->acc_next->acc_prev = child->acc_prev;
	else
		ptp->q_tail = child->acc_prev;
	*child->acc_prev = child->acc_next;
	child->acc_next = NULL;
	child->acc_prev = NULL;
	child->parent = NULL;
	ptp->q_len--;

	new_sock->id = id;
	new_sock->pcb = child;
	new_sock->chan_rd = (u32)&child->rx_head;
	new_sock->chan_wr = (u32)&child->tx_head;
	new_sock->chan_ex = (u32)&child->state;
	child->sock = new_sock;

	if (peer_mac != NULL)
		sceKernelMemcpy(peer_mac, child->peer_mac, 6);
	if (peer_port != NULL)
		*peer_port = child->peer_port;
	*out_id = id;
	return 0;
}

/* === sub_40ec @ 0x40ec === */
static s32 sub_40ec(PtpPcb *ptp, const void *data, s32 *len, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	s32 req_len = *len;
	s32 sent = 0;
	s32 ret;

	*len = 0;
	if (ptp->mode != 1 || ptp->state != 4)
		return 0x80410704;

	while (sent < req_len) {
		u32 space = ptp->tx_hiwat - ptp->tx_cc;
		if (space == 0) {
			if (flags & 1)
				return (sent > 0) ? 0 : 0x80410709;
			ptp->tx_flags |= 1;
			ret = sceNet_lib_F6DB0A0B(&ptp->tx_head, &rem_timeout);
			if (ptp->sock == NULL)
				return 0x8041070c;
			if (ptp->state != 4)
				return 0x8041070d;
			if (ret < 0) {
				if ((u32)ret == 0x80410186)
					return 0x80410715;
				if ((u32)ret == 0x800201a4)
					return 0x80410718;
				return ret;
			}
			continue;
		}

		while (sent < req_len && ptp->tx_hiwat - ptp->tx_cc > 0) {
			u32 avail = ptp->tx_hiwat - ptp->tx_cc;
			s32 chunk = req_len - sent;
			AdhocMbuf *m;

			if ((u32)chunk > avail)
				chunk = (s32)avail;
			if (chunk > 0x54c)
				chunk = 0x54c;

			m = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
			if (m == NULL) {
				g_ptp_stats[13]++;
				return (sent > 0) ? 0 : 0x80410709;
			}
			m->m_next = NULL;
			m->m_nextpkt = NULL;
			m->m_data = (u8 *)m + 0x30;
			m->m_type = 1;
			m->m_flags = 2;
			m->m_pkthdr_Aux = 0;

			if (chunk + 0x20 >= 0xd1) {
				void *ext = sceNetIfhandle_lib_15CFE3C0(0x800);
				if (ext == NULL)
					ext = sceNetIfhandle_lib_15CFE3C0(0x800);
				*(void **)((u8 *)m + 0x30) = ext;
				if (ext != NULL) {
					m->m_data = (u8 *)ext;
					m->m_flags |= 9;
					*(u32 *)((u8 *)m + 0x3c) = 0x800;
					*(u32 *)((u8 *)m + 0x34) = 0;
					*(u32 *)((u8 *)m + 0x38) = 0;
					*(AdhocMbuf **)((u8 *)m + 0x48) = m;
					*(AdhocMbuf **)((u8 *)m + 0x44) = m;
				}
				if (!(m->m_flags & 1)) {
					sceNetIfhandle_lib_F8825DC4(m);
					g_ptp_stats[13]++;
					return (sent > 0) ? 0 : 0x80410709;
				}
			}

			sceKernelMemset(m->m_data, 0, 0x10);
			m->m_data += 0x10;
			sceKernelMemcpy(m->m_data, (const u8 *)data + sent, (u32)chunk);
			m->m_rcvif = NULL;
			m->m_pkthdr_len = chunk;
			m->m_len = chunk;

			if (ptp->tx_tail == NULL)
				ptp->tx_head = m;
			else
				ptp->tx_tail->m_nextpkt = m;
			ptp->tx_tail = m;
			ptp->tx_cc += (u32)chunk;
			ptp->snd_max++;

			sent += chunk;
			*len = sent;
		}

		sub_6878(ptp, 0);
		if (flags & 1)
			break;
	}

	return 0;
}

/* === sub_44d4 @ 0x44d4 === */
static s32 sub_44d4(PtpPcb *ptp, void *data, s32 *len, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	s32 req_len = *len;
	s32 rcvd = 0;
	AdhocMbuf *m;
	s32 ret;

	*len = 0;
	if (ptp->mode != 1 || ptp->state != 4)
		return 0x80410704;

	for (;;) {
		m = ptp->rx_head;
		if (m != NULL)
			break;
		if (flags & 1)
			return 0x80410709;
		ptp->rx_flags |= 1;
		ret = sceNet_lib_F6DB0A0B(&ptp->rx_head, &rem_timeout);
		if (ptp->sock == NULL)
			return 0x8041070c;
		if (ptp->state != 4)
			return 0x8041070d;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
	}

	while ((m = ptp->rx_head) != NULL && rcvd < req_len) {
		s32 pkt_len = m->m_pkthdr_len;
		s32 rem_req = req_len - rcvd;

		if (pkt_len <= rem_req) {
			AdhocMbuf *cur;

			ptp->rx_head = m->m_nextpkt;
			if (ptp->rx_head == NULL)
				ptp->rx_tail = NULL;
			m->m_nextpkt = NULL;

			for (cur = m; cur != NULL; cur = cur->m_next) {
				if (cur->m_len > 0) {
					sceKernelMemcpy((u8 *)data + rcvd, cur->m_data, (u32)cur->m_len);
					rcvd += cur->m_len;
				}
			}
			ptp->rx_cc -= (u32)pkt_len;
			sceNetIfhandle_lib_F56FAC82(m);
		} else {
			sceNetIfhandle_lib_9A6261EC(m, 0, rem_req, (u8 *)data + rcvd);
			sceNetIfhandle_lib_BFF3CEA5(m, rem_req);
			rcvd += rem_req;
			ptp->rx_cc -= (u32)rem_req;
		}
	}

	*len = rcvd;
	if ((ptp->flags & 1) && ptp->rx_hiwat - ptp->rx_cc >= 0x54c) {
		ptp->flags &= ~1;
		sub_668c(ptp, 5);
	}
	return 0;
}

/* === sub_4808 @ 0x4808 === */
static s32 sub_4808(PtpPcb *ptp, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	s32 ret;

	if (ptp->mode != 1 || ptp->state != 4)
		return 0x80410704;

	while (ptp->tx_cc != 0) {
		if (flags & 1)
			return 0x80410709;
		ptp->tx_flags |= 1;
		ret = sceNet_lib_F6DB0A0B(&ptp->tx_head, &rem_timeout);
		if (ptp->sock == NULL)
			return 0x8041070c;
		if (ptp->state != 4)
			return 0x8041070d;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
	}
	return 0;
}

/* === sceNetAdhoc_lib_62B875A8 @ 0x49cc === */
void sceNetAdhoc_lib_62B875A8(void)
{
	sceKernelMemset(g_ptp_stats, 0, 0x7c);
}

/* === sceNetAdhoc_lib_2CA12790 @ 0x49f4 === */
s32 sceNetAdhoc_lib_2CA12790(void *dst)
{
	if (dst == NULL)
		return 0x80410708;
	sceKernelMemcpy(dst, g_ptp_stats, 0x7c);
	return 0;
}

/* === sub_4a30 @ 0x4a30 === */
static s32 sub_4a30(PtpPcb **out_ptp)
{
	PtpPcb *ptp;

	ptp = (PtpPcb *)sceNetIfhandle_lib_15CFE3C0(0xf0);
	if (ptp == NULL)
		return 0x80410709;
	sceKernelMemset(ptp, 0, 0xf0);
	sceNet_lib_25CC373A(ptp->timer_rexmt);
	sceNet_lib_25CC373A(ptp->timer_probe);
	ptp->q_head = NULL;
	ptp->q_tail = &ptp->q_head;
	ptp->next = NULL;
	ptp->prev = g_ptp_list.tail;
	*g_ptp_list.tail = ptp;
	g_ptp_list.tail = &ptp->next;
	*out_ptp = ptp;
	return 0;
}

/* === sub_4ad4 @ 0x4ad4 === */
static void sub_4ad4(PtpPcb *ptp)
{
	ptp->iss = g_ptp_initial_seq;
	g_ptp_initial_seq += 0x100;
	ptp->snd_una = ptp->iss;
	ptp->snd_nxt = ptp->iss;
	ptp->snd_max = ptp->iss;
}

/* === sub_4b7c @ 0x4b7c === */
static s32 sub_4b7c(PtpPcb *ptp, const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 queue_max, s32 mode)
{
	u8 local_mac[8];
	PtpPcb *cur;
	s32 ret;

	ret = sceNetGetLocalEtherAddr(local_mac);
	if (ret < 0)
		return ret;
	if (sceNet_lib_E0A81C7C(src_mac, local_mac, 6) != 0)
		return 0x80410701;

	if (mode == 1) {
		if ((dst_mac[0] & 1) || sceNet_lib_E0A81C7C(dst_mac, g_zero_mac, 6) == 0)
			return 0x80410701;
	}

	if (src_port == 0) {
		src_port = sub_584c();
		if (src_port == 0)
			return 0x8041070b;
	} else {
		for (cur = g_ptp_list.head; cur != NULL; cur = cur->next) {
			if (cur == ptp)
				continue;
			if (cur->local_port == src_port &&
			    sceNet_lib_E0A81C7C(cur->local_mac, src_mac, 6) == 0) {
				if (mode == 2 || cur->mode == 2)
					return 0x80410703;
				if (cur->peer_port == dst_port &&
				    sceNet_lib_E0A81C7C(cur->peer_mac, dst_mac, 6) == 0)
					return 0x80410703;
			}
		}
	}

	sceKernelMemcpy(ptp->local_mac, src_mac, 6);
	ptp->local_port = src_port;
	sceKernelMemcpy(ptp->peer_mac, dst_mac, 6);
	ptp->peer_port = dst_port;
	ptp->tx_hiwat = bufsize;
	ptp->rx_hiwat = bufsize;
	ptp->rcv_wnd = bufsize;
	ptp->rexmt_intvl = rexmt_int;
	ptp->rexmt_max = rexmt_cnt;
	ptp->q_maxlen = queue_max;
	ptp->mode = mode;
	if (mode == 2)
		ptp->state = 1;
	else
		ptp->state = 0;
	sub_4ad4(ptp);
	return 0;
}

/* === sub_4d48 @ 0x4d48 === */
static void sub_4d48(PtpPcb *ptp)
{
	sceNet_lib_A93A93E9(ptp->timer_rexmt);
	ptp->state = 4;
	ptp->snd_una = ptp->iss + 1;
	ptp->snd_nxt = ptp->iss + 1;
	ptp->snd_max = ptp->iss + 1;
	ptp->rcv_nxt = ptp->irs + 1;

	sceNet_lib_A8B6205A(ptp->timer_probe, ptp->rexmt_intvl, sub_6b20, ptp);

	if (ptp->parent != NULL) {
		PtpPcb *parent = ptp->parent;
		if (parent->q_flags & 1) {
			parent->q_flags &= ~1;
			sceNet_lib_7574FDA1(&parent->q_head);
		}
		if (parent->q_flags & 2) {
			parent->q_flags &= ~2;
			sceNet_lib_7574FDA1(&g_global_wait_head);
		}
	} else {
		if (ptp->flags & 2) {
			ptp->flags &= ~2;
			sceNet_lib_7574FDA1(&ptp->state);
		}
		if (ptp->flags & 4) {
			ptp->flags &= ~4;
			sceNet_lib_7574FDA1(&g_global_wait_head);
		}
	}
}

/* === sub_4e3c @ 0x4e3c === */
static void sub_4e3c(PtpPcb *ptp)
{
	sceNet_lib_A93A93E9(ptp->timer_rexmt);
	sceNet_lib_A93A93E9(ptp->timer_probe);

	if (ptp->parent != NULL) {
		sub_542c(ptp);
		return;
	}

	ptp->state = 0;
	sub_5590(ptp);
	sub_55dc(ptp);

	if (ptp->flags & 2) {
		ptp->flags &= ~2;
		sceNet_lib_7574FDA1(&ptp->state);
	}
	if (ptp->tx_flags & 1) {
		ptp->tx_flags &= ~1;
		sceNet_lib_7574FDA1(&ptp->tx_head);
	}
	if (ptp->rx_flags & 1) {
		ptp->rx_flags &= ~1;
		sceNet_lib_7574FDA1(&ptp->rx_head);
	}
	if ((ptp->flags & 4) || (ptp->tx_flags & 2) || (ptp->rx_flags & 2)) {
		ptp->flags &= ~4;
		ptp->tx_flags &= ~2;
		ptp->rx_flags &= ~2;
		sceNet_lib_7574FDA1(&g_global_wait_head);
	}
}

/* === sub_4fc8 @ 0x4fc8 === */
static s32 sub_4fc8(AdhocSocket *sock, s32 events)
{
	PtpPcb *ptp = (PtpPcb *)sock->pcb;
	s32 rev = 0;

	if (ptp == NULL)
		return 0x10;

	if (ptp->mode == 1) {
		if (ptp->state == 0)
			return 0x10;
		if (events & 4) {
			if (ptp->state == 4)
				rev |= 4;
		}
		if (events & 1) {
			if (ptp->state == 4 && ptp->tx_cc < ptp->tx_hiwat)
				rev |= 1;
		}
		if (events & 2) {
			if (ptp->state == 4 && ptp->rx_head != NULL)
				rev |= 2;
		}
	} else if (ptp->mode == 2) {
		if (ptp->state != 1)
			return 0x10;
		if (events & 8) {
			PtpPcb *child;
			for (child = ptp->q_head; child != NULL; child = child->acc_next) {
				if (child->state == 4) {
					rev |= 8;
					break;
				}
			}
		}
	}
	return rev;
}

/* === sceNetAdhocGetPtpStat @ 0x5104 === */
s32 sceNetAdhocGetPtpStat(s32 *buflen, void *buf)
{
	s32 intr;
	s32 ret;
	PtpPcb *ptp;
	s32 total_bytes;
	u8 *out;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (buflen == NULL)
		return 0x80410708;
	if (*buflen < 0)
		return 0x80410705;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	if (buf == NULL) {
		total_bytes = 0;
		for (ptp = g_ptp_list.head; ptp != NULL; ptp = ptp->next) {
			if (ptp->parent == NULL)
				total_bytes += 0x20;
		}
		*buflen = total_bytes;
		ret = 0;
	} else {
		s32 max_len = *buflen;
		out = (u8 *)buf;
		total_bytes = 0;
		for (ptp = g_ptp_list.head; ptp != NULL; ptp = ptp->next) {
			if (ptp->parent != NULL)
				continue;
			if (total_bytes + 0x20 > max_len)
				break;
			*(u32 *)(out + 0x00) = 0;
			*(s32 *)(out + 0x04) = (ptp->sock != NULL) ? ptp->sock->id : -1;
			sceKernelMemcpy(out + 0x08, ptp->local_mac, 6);
			sceKernelMemcpy(out + 0x0e, ptp->peer_mac, 6);
			*(u16 *)(out + 0x14) = ptp->local_port;
			*(u16 *)(out + 0x16) = ptp->peer_port;
			*(u32 *)(out + 0x18) = ptp->tx_cc;
			*(u32 *)(out + 0x1c) = ptp->rx_cc;
			if (total_bytes >= 0x20)
				*(void **)(out - 0x20) = out;
			out += 0x20;
			total_bytes += 0x20;
		}
		*buflen = total_bytes;
		ret = 0;
	}

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sub_5288 @ 0x5288 === */
static void sub_5288(void)
{
	g_ptp_list.head = NULL;
	g_ptp_list.tail = &g_ptp_list.head;
	g_ptp_anon_port = (u16)((sceNet_lib_1858883D() % 0xc000) + 0x4000);
	g_ptp_initial_seq = sceNet_lib_1858883D();
	sceKernelMemset(g_ptp_stats, 0, 0x7c);
}

/* === sub_52e0 @ 0x52e0 === */
static void sub_52e0(void)
{
	PtpPcb *ptp;

	while ((ptp = g_ptp_list.head) != NULL) {
		if (ptp->sock != NULL) {
			ptp->sock->pcb = NULL;
			sub_0ffc(ptp->sock);
			ptp->sock = NULL;
		}
		sub_542c(ptp);
	}
}

/* === sub_536c @ 0x536c === */
static PtpPcb *sub_536c(const u8 *local_mac, u16 local_port, const u8 *peer_mac, u16 peer_port)
{
	PtpPcb *ptp;

	for (ptp = g_ptp_list.head; ptp != NULL; ptp = ptp->next) {
		if (ptp->local_port == local_port && ptp->peer_port == peer_port &&
		    sceNet_lib_E0A81C7C(ptp->local_mac, local_mac, 6) == 0 &&
		    sceNet_lib_E0A81C7C(ptp->peer_mac, peer_mac, 6) == 0)
			return ptp;
	}
	return NULL;
}

/* === sub_542c @ 0x542c === */
static void sub_542c(PtpPcb *ptp)
{
	PtpPcb *child;

	sceNet_lib_A93A93E9(ptp->timer_rexmt);
	sceNet_lib_A93A93E9(ptp->timer_probe);

	while ((child = ptp->q_head) != NULL) {
		sub_542c(child);
	}

	if (ptp->parent != NULL) {
		PtpPcb *parent = ptp->parent;
		if (ptp->acc_next != NULL)
			ptp->acc_next->acc_prev = ptp->acc_prev;
		else
			parent->q_tail = ptp->acc_prev;
		*ptp->acc_prev = ptp->acc_next;
		parent->q_len--;
	}

	if (ptp->next != NULL)
		ptp->next->prev = ptp->prev;
	else
		g_ptp_list.tail = ptp->prev;
	*ptp->prev = ptp->next;

	sub_5590(ptp);
	sub_55dc(ptp);
	sceNetIfhandle_lib_76BAD213(ptp);
}

/* === sub_5590 @ 0x5590 === */
static void sub_5590(PtpPcb *ptp)
{
	AdhocMbuf *m;

	while ((m = ptp->tx_head) != NULL) {
		ptp->tx_head = m->m_nextpkt;
		if (ptp->tx_head == NULL)
			ptp->tx_tail = NULL;
		m->m_nextpkt = NULL;
		sceNetIfhandle_lib_F56FAC82(m);
	}
	ptp->tx_cc = 0;
}

/* === sub_55dc @ 0x55dc === */
static void sub_55dc(PtpPcb *ptp)
{
	AdhocMbuf *m;

	while ((m = ptp->rx_head) != NULL) {
		ptp->rx_head = m->m_nextpkt;
		if (ptp->rx_head == NULL)
			ptp->rx_tail = NULL;
		m->m_nextpkt = NULL;
		sceNetIfhandle_lib_F56FAC82(m);
	}
	ptp->rx_cc = 0;
}

/* === sub_55f8 @ 0x55f8 === */
static s32 sub_55f8(void)
{
	return (g_ptp_list.head != NULL) ? 1 : 0;
}

/* === sub_5604 @ 0x5604 === */
static void sub_5604(void *ifp_arg)
{
	AdhocIfnet *ifp = (AdhocIfnet *)ifp_arg;
	PtpPcb *ptp;

	if (ifp->snd_len >= ifp->snd_maxlen)
		return;

	for (ptp = g_ptp_list.head; ptp != NULL; ptp = ptp->next) {
		if (ptp->state == 4 && ptp->tx_head != NULL && ptp->snd_nxt < ptp->snd_max) {
			sub_6878(ptp, 0);
			if (ifp->snd_len >= ifp->snd_maxlen)
				break;
		}
	}
}

/* === sub_572c @ 0x572c === */
static void sub_572c(void *ifp_arg)
{
	(void)ifp_arg;
}

/* === sub_5750 @ 0x5750 === */
static void sub_5750(PtpPcb *ptp)
{
	PtpPcb *child;

	if (ptp->q_len <= 0)
		return;

	for (child = ptp->q_head; child != NULL; child = child->acc_next) {
		if (child->state != 4) {
			sub_542c(child);
			return;
		}
	}
}

/* === sub_584c @ 0x584c === */
static u16 sub_584c(void)
{
	u16 cand = g_ptp_anon_port;
	s32 count = 0;
	PtpPcb *cur;

	while (count < 0x4000) {
		s32 used = 0;
		for (cur = g_ptp_list.head; cur != NULL; cur = cur->next) {
			if (cur->local_port == cand) {
				used = 1;
				break;
			}
		}
		cand++;
		if (cand < 0xc000)
			cand = 0xc000;
		if (!used) {
			g_ptp_anon_port = cand;
			return (u16)(cand == 0xc000 ? 0xffff : cand - 1);
		}
		count++;
	}
	return 0;
}

/* === sub_5938 @ 0x5938 === */
static void sub_5938(AdhocMbuf *m)
{
	u8 dst_mac[6];
	u8 src_mac[6];
	u16 body_len;
	u16 sport;
	u16 dport;
	u32 seq;
	u32 wnd;
	u8 type;
	s32 payload_len;
	PtpPcb *ptp;
	u8 *hdr;

	if (m->m_pkthdr_len < 0x3c) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_ptp_stats[0]++;
		return;
	}
	if (m->m_len < 0x3c) {
		m = (AdhocMbuf *)sceNetIfhandle_lib_49EDBB18(m, 0x3c);
		if (m == NULL) {
			g_ptp_stats[0]++;
			return;
		}
	}

	hdr = m->m_data;
	type = hdr[0x31];
	if (type == 0 || type >= 8) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_ptp_stats[0]++;
		return;
	}

	sceKernelMemcpy(&body_len, hdr + 0x32, 2);
	body_len = bswap16(body_len);
	if (body_len < 0x0c || m->m_pkthdr_len < (s32)body_len + 0x30) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_ptp_stats[0]++;
		return;
	}

	payload_len = (s32)body_len - 0x0c;
	if (((type == 1 || type == 2 || type == 5) && payload_len != 4) ||
	    ((type == 3 || type == 6 || type == 7) && payload_len != 0)) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_ptp_stats[0]++;
		return;
	}

	sceKernelMemcpy(dst_mac, hdr, 6);
	sceKernelMemcpy(src_mac, hdr + 6, 6);
	sceKernelMemcpy(&sport, hdr + 0x34, 2);
	sport = bswap16(sport);
	sceKernelMemcpy(&dport, hdr + 0x36, 2);
	dport = bswap16(dport);
	sceKernelMemcpy(&seq, hdr + 0x38, 4);
	seq = bswap32(seq);

	if (sport == 0 || dport == 0) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_ptp_stats[0]++;
		return;
	}

	wnd = 0;
	if (type == 1 || type == 2 || type == 5) {
		if (m->m_len < 0x40) {
			m = (AdhocMbuf *)sceNetIfhandle_lib_49EDBB18(m, 0x40);
			if (m == NULL) {
				g_ptp_stats[0]++;
				return;
			}
		}
		sceKernelMemcpy(&wnd, m->m_data + 0x3c, 4);
		wnd = bswap32(wnd);
	}

	{
		s32 pad_trim = (payload_len + 0x3c) - m->m_pkthdr_len;
		if (pad_trim < 0)
			sceNetIfhandle_lib_BFF3CEA5(m, pad_trim);
	}

	ptp = sub_536c(dst_mac, dport, src_mac, sport);
	if (ptp == NULL) {
		if (type == 1) {
			ptp = sub_536c(dst_mac, dport, g_zero_mac, 0);
			type = m->m_data[0x31];
		}
		if (ptp == NULL) {
			if (type != 7)
				sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
			g_ptp_stats[27]++;
			sceNetIfhandle_lib_F56FAC82(m);
			return;
		}
	}

	switch (type) {
	case 1: /* SYN */
		switch (ptp->state) {
		case 0:
		case 2:
		case 4:
			sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
			g_ptp_stats[3]++;
			break;
		case 1: {
			PtpPcb *child;
			if (ptp->q_len >= ptp->q_maxlen) {
				sub_5750(ptp);
				if (ptp->q_len >= ptp->q_maxlen) {
					sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
					g_ptp_stats[2]++;
					break;
				}
			}
			if (sub_4a30(&child) < 0) {
				sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[2]++;
				break;
			}
			sceKernelMemcpy(child->local_mac, dst_mac, 6);
			child->local_port = dport;
			sceKernelMemcpy(child->peer_mac, src_mac, 6);
			child->peer_port = sport;
			child->tx_hiwat = ptp->tx_hiwat;
			child->rx_hiwat = ptp->rx_hiwat;
			child->rcv_wnd = (wnd < ptp->rcv_wnd) ? wnd : ptp->rcv_wnd;
			child->snd_wnd = wnd;
			child->irs = seq;
			child->rexmt_intvl = ptp->rexmt_intvl;
			child->rexmt_max = ptp->rexmt_max;
			child->mode = 1;
			child->state = 3;
			child->parent = ptp;
			sub_4ad4(child);
			child->acc_next = NULL;
			child->acc_prev = ptp->q_tail;
			*ptp->q_tail = child;
			ptp->q_tail = &child->acc_next;
			ptp->q_len++;
			sub_668c(child, 2);
			child->rexmt_rem = child->rexmt_max;
			sceNet_lib_A8B6205A(child->timer_rexmt, child->rexmt_intvl, sub_6a54, child);
			g_ptp_stats[1]++;
			break;
		}
		case 3:
			if (ptp->irs != seq) {
				sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[3]++;
			} else {
				sub_668c(ptp, 2);
				g_ptp_stats[1]++;
			}
			break;
		default:
			break;
		}
		break;

	case 2: /* SYN-ACK */
		switch (ptp->state) {
		case 0:
		case 1:
		case 3:
			sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
			g_ptp_stats[7]++;
			break;
		case 2:
			if (ptp->iss != seq) {
				sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[7]++;
			} else {
				ptp->snd_wnd = wnd;
				if (wnd < ptp->rcv_wnd)
					ptp->rcv_wnd = wnd;
				sub_4d48(ptp);
				sub_668c(ptp, 3);
				g_ptp_stats[5]++;
			}
			break;
		case 4:
			if (ptp->iss != seq) {
				sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[7]++;
			} else {
				sub_668c(ptp, 3);
				g_ptp_stats[6]++;
			}
			break;
		default:
			break;
		}
		break;

	case 3: /* SYN-ACK-ACK */
		switch (ptp->state) {
		case 0:
			g_ptp_stats[11]++;
			break;
		case 1:
		case 2:
			sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
			g_ptp_stats[11]++;
			break;
		case 3:
			if (ptp->iss == seq) {
				sub_4d48(ptp);
				g_ptp_stats[9]++;
			} else {
				sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[11]++;
			}
			break;
		case 4:
			if (ptp->iss != seq) {
				sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[11]++;
			} else {
				g_ptp_stats[10]++;
			}
			break;
		default:
			break;
		}
		break;

	case 4: /* DATA */
		switch (ptp->state) {
		case 0:
			g_ptp_stats[16]++;
			break;
		case 1:
		case 2:
			sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
			g_ptp_stats[16]++;
			break;
		case 3:
			if (ptp->rcv_nxt == seq) {
				sub_668c(ptp, 2);
				sceNet_lib_A8B6205A(ptp->timer_rexmt, ptp->rexmt_intvl, sub_6a54, ptp);
				ptp->rexmt_rem = ptp->rexmt_max;
			}
			break;
		case 4:
			if (ptp->rcv_nxt != seq) {
				if ((s32)(ptp->rcv_nxt - seq) > 0) {
					sub_668c(ptp, 5);
					g_ptp_stats[15]++;
				} else {
					if (!(ptp->flags & 1)) {
						ptp->flags |= 1;
						sub_668c(ptp, 6);
					}
					g_ptp_stats[16]++;
				}
				break;
			}
			sceNetIfhandle_lib_BFF3CEA5(m, 0x3c);
			if ((s32)(ptp->rx_hiwat - ptp->rx_cc) < m->m_pkthdr_len) {
				g_ptp_stats[14]++;
				break;
			}
			m->m_nextpkt = NULL;
			ptp->flags &= ~1;
			if (ptp->rx_tail == NULL)
				ptp->rx_head = m;
			else
				ptp->rx_tail->m_nextpkt = m;
			ptp->rx_tail = m;
			ptp->rx_cc += (u32)m->m_pkthdr_len;
			ptp->rcv_nxt++;
			sub_668c(ptp, 5);
			if (ptp->rx_flags & 1) {
				ptp->rx_flags &= ~1;
				sceNet_lib_7574FDA1(&ptp->rx_head);
			}
			if (ptp->rx_flags & 2) {
				ptp->rx_flags &= ~2;
				sceNet_lib_7574FDA1(&g_global_wait_head);
			}
			g_ptp_stats[13]++;
			return;
		default:
			break;
		}
		break;

	case 5: /* ACK */
		if (ptp->state < 4) {
			if (ptp->state > 0) {
				sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[20]++;
			} else if (ptp->state == 0) {
				g_ptp_stats[20]++;
			}
			break;
		}
		if (ptp->state != 4)
			break;

		if ((s32)(ptp->snd_una - seq) > 0) {
			ptp->snd_wnd = wnd;
			if (ptp->tx_flags & 1) {
				ptp->tx_flags &= ~1;
				sceNet_lib_7574FDA1(&ptp->tx_head);
			}
			if (ptp->tx_flags & 2) {
				ptp->tx_flags &= ~2;
				sceNet_lib_7574FDA1(&g_global_wait_head);
			}
			sub_6878(ptp, 0);
			g_ptp_stats[19]++;
			break;
		}

		for (;;) {
			AdhocMbuf *txm = ptp->tx_head;
			if (txm == NULL) {
				sub_4e3c(ptp);
				g_ptp_stats[20]++;
				goto drop;
			}
			ptp->tx_head = txm->m_nextpkt;
			if (ptp->tx_head == NULL)
				ptp->tx_tail = NULL;
			ptp->snd_una++;
			ptp->tx_cc -= (u32)txm->m_pkthdr_len;
			txm->m_nextpkt = NULL;
			sceNetIfhandle_lib_F56FAC82(txm);
			if ((s32)(ptp->snd_una - seq) > 0)
				break;
		}

		if ((s32)(ptp->snd_nxt - ptp->snd_una) < 0)
			ptp->snd_nxt = ptp->snd_una;
		if (ptp->snd_una == ptp->snd_max)
			sceNet_lib_A93A93E9(ptp->timer_rexmt);

		ptp->snd_wnd = wnd;
		if (ptp->tx_flags & 1) {
			ptp->tx_flags &= ~1;
			sceNet_lib_7574FDA1(&ptp->tx_head);
		}
		if (ptp->tx_flags & 2) {
			ptp->tx_flags &= ~2;
			sceNet_lib_7574FDA1(&g_global_wait_head);
		}
		sub_6878(ptp, 0);
		g_ptp_stats[18]++;
		break;

	case 6: /* NACK / FAST REXMT */
		if (ptp->state < 4) {
			if (ptp->state > 0) {
				sub_637c(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[23]++;
			} else if (ptp->state == 0) {
				g_ptp_stats[23]++;
			}
			break;
		}
		if (ptp->state == 4) {
			if (ptp->snd_una != seq) {
				g_ptp_stats[23]++;
				break;
			}
			sceNet_lib_A93A93E9(ptp->timer_rexmt);
			ptp->snd_nxt = ptp->snd_una;
			sub_6878(ptp, 0);
			g_ptp_stats[22]++;
		}
		break;

	case 7: /* RST */
		if (ptp->state == 1 || ptp->state == 0) {
			g_ptp_stats[26]++;
		} else if (ptp->state >= 2 && ptp->state < 5) {
			g_ptp_stats[25]++;
			sub_4e3c(ptp);
		}
		break;

	default:
		break;
	}

drop:
	sceNetIfhandle_lib_F56FAC82(m);
}

/* === sub_637c @ 0x637c === */
static void sub_637c(const u8 *src_mac, u16 sport, const u8 *dst_mac, u16 dport, u8 type, u32 seq_or_ack, AdhocMbuf *payload_m)
{
	AdhocIfnet *ifp;
	s32 payload_len;
	s32 tot_body;
	s32 pad_len;
	s32 alloc_len;
	AdhocMbuf *m;
	u8 *hdr;
	u16 tmp16;
	u32 tmp32;

	ifp = (AdhocIfnet *)sceNet_lib_D60225A3();
	if (ifp == NULL)
		return;
	if (ifp->snd_len >= ifp->snd_maxlen)
		return;

	payload_len = (payload_m != NULL) ? payload_m->m_pkthdr_len : 0;
	m = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (m == NULL)
		return;

	tot_body = payload_len + 0x0c;
	pad_len = tot_body & 0x0f;
	if (pad_len > 0)
		pad_len = 0x10 - pad_len;
	alloc_len = payload_len + pad_len;

	m->m_next = NULL;
	m->m_nextpkt = NULL;
	m->m_data = (u8 *)m + 0x30;
	m->m_type = 1;
	m->m_flags = 2;
	m->m_pkthdr_Aux = 0;

	if (alloc_len + 0x3c >= 0xd1) {
		void *ext = sceNetIfhandle_lib_15CFE3C0(0x800);
		if (ext == NULL)
			ext = sceNetIfhandle_lib_15CFE3C0(0x800);
		*(void **)((u8 *)m + 0x30) = ext;
		if (ext != NULL) {
			m->m_data = (u8 *)ext;
			m->m_flags |= 9;
			*(u32 *)((u8 *)m + 0x3c) = 0x800;
			*(u32 *)((u8 *)m + 0x34) = 0;
			*(u32 *)((u8 *)m + 0x38) = 0;
			*(AdhocMbuf **)((u8 *)m + 0x48) = m;
			*(AdhocMbuf **)((u8 *)m + 0x44) = m;
		}
		if (!(m->m_flags & 1)) {
			sceNetIfhandle_lib_F8825DC4(m);
			return;
		}
	}

	hdr = m->m_data;
	sceKernelMemcpy(hdr, dst_mac, 6);
	sceKernelMemcpy(hdr + 6, src_mac, 6);
	sceKernelMemcpy(hdr + 0x0c, &g_adhoc_ethertype, 2);
	sceKernelMemcpy(hdr + 0x0e, &g_adhoc_hdr_ver, 2);

	hdr = m->m_data;
	hdr[0x30] = 2;
	hdr[0x31] = type;
	tmp16 = bswap16((u16)tot_body);
	sceKernelMemcpy(hdr + 0x32, &tmp16, 2);
	tmp16 = bswap16(sport);
	sceKernelMemcpy(hdr + 0x34, &tmp16, 2);
	tmp16 = bswap16(dport);
	sceKernelMemcpy(hdr + 0x36, &tmp16, 2);
	tmp32 = bswap32(seq_or_ack);
	sceKernelMemcpy(hdr + 0x38, &tmp32, 4);

	if (payload_len > 0) {
		if (payload_m->m_len >= payload_len)
			sceKernelMemcpy(m->m_data + 0x3c, payload_m->m_data, (u32)payload_len);
		else
			sceNetIfhandle_lib_9A6261EC(payload_m, 0, payload_len, m->m_data + 0x3c);
	}
	if (pad_len > 0)
		sceKernelMemset(m->m_data + 0x3c + payload_len, 0, (u32)pad_len);

	m->m_rcvif = NULL;
	m->m_pkthdr_len = alloc_len + 0x3c;
	m->m_len = alloc_len + 0x3c;
	{
		u32 pkt_len = (u32)(alloc_len + 0x3c);
		u32 new_lo = ifp->if_obytes_lo + pkt_len;
		u32 carry = (new_lo < pkt_len) ? 1 : 0;
		ifp->if_obytes_lo = new_lo;
		ifp->if_obytes_hi += carry;
	}
	sub_036c(ifp, m);
}

/* === sub_668c @ 0x668c === */
static void sub_668c(PtpPcb *ptp, u8 type)
{
	u32 seq_val;
	AdhocMbuf *m = NULL;

	switch (type) {
	case 1:
	case 2:
	case 3:
	case 7:
		seq_val = ptp->iss;
		break;
	case 5:
		seq_val = ptp->rcv_nxt - 1;
		break;
	case 6:
		seq_val = ptp->rcv_nxt;
		break;
	default:
		seq_val = 0;
		break;
	}

	if (type == 1 || type == 2 || type == 5) {
		u32 wnd_be;
		m = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
		if (m == NULL)
			return;
		m->m_next = NULL;
		m->m_nextpkt = NULL;
		m->m_data = (u8 *)m + 0x30;
		m->m_type = 1;
		m->m_flags = 2;
		m->m_rcvif = NULL;
		m->m_pkthdr_len = 4;
		m->m_len = 4;
		m->m_pkthdr_Aux = 0;
		wnd_be = bswap32(ptp->rx_hiwat - ptp->rx_cc);
		sceKernelMemcpy(m->m_data, &wnd_be, 4);
	}

	switch (type) {
	case 1:
		g_ptp_stats[0]++;
		break;
	case 2:
		g_ptp_stats[4]++;
		break;
	case 3:
		g_ptp_stats[8]++;
		break;
	case 5:
		g_ptp_stats[17]++;
		break;
	case 6:
		g_ptp_stats[21]++;
		break;
	case 7:
		g_ptp_stats[24]++;
		break;
	default:
		break;
	}

	sub_637c(ptp->local_mac, ptp->local_port, ptp->peer_mac, ptp->peer_port, type, seq_val, m);
	if (m != NULL)
		sceNetIfhandle_lib_F56FAC82(m);
}

/* === sub_6878 @ 0x6878 === */
static void sub_6878(PtpPcb *ptp, s32 rexmt_only)
{
	AdhocMbuf *m = ptp->tx_head;
	u32 off_bytes = 0;
	s32 sent_cnt = 0;
	u32 seq;

	if (m == NULL)
		return;

	for (seq = ptp->snd_una; seq != ptp->snd_nxt; seq++) {
		off_bytes += (u32)m->m_pkthdr_len;
		m = m->m_nextpkt;
		if (m == NULL)
			return;
	}

	while (m != NULL) {
		u32 *ts = (u32 *)((u8 *)m + 0x20);
		if ((s32)(ptp->snd_wnd - off_bytes) < m->m_pkthdr_len) {
			if (sent_cnt != 0)
				break;
			if (rexmt_only != 0) {
				sub_637c(ptp->local_mac, ptp->local_port, ptp->peer_mac, ptp->peer_port, 4, ptp->snd_una - 1, NULL);
				g_ptp_stats[29]++;
				return;
			}
			if (!(ptp->timer_rexmt_active & 1)) {
				g_ptp_stats[30]++;
				ptp->rexmt_rem = ptp->rexmt_max;
				sceNet_lib_A8B6205A(ptp->timer_rexmt, ptp->rexmt_intvl, sub_6a54, ptp);
			}
			break;
		}

		sub_637c(ptp->local_mac, ptp->local_port, ptp->peer_mac, ptp->peer_port, 4, ptp->snd_nxt, m);
		sent_cnt++;
		if ((ts[2] | ts[3]) == 0) {
			s64 now = sceKernelGetSystemTimeWide();
			ts[2] = (u32)now;
			ts[3] = (u32)((u64)now >> 32);
		}
		ptp->snd_nxt++;
		g_ptp_stats[12]++;
		off_bytes += (u32)m->m_pkthdr_len;
		m = m->m_nextpkt;
	}

	if (rexmt_only == 0 && sent_cnt > 0) {
		ptp->rexmt_rem = ptp->rexmt_max;
		sceNet_lib_A8B6205A(ptp->timer_rexmt, ptp->rexmt_intvl, sub_6a54, ptp);
	}
}

/* === sub_6a54 @ 0x6a54 === */
static void sub_6a54(PtpPcb *ptp)
{
	if (ptp->state < 2)
		return;

	if (ptp->rexmt_rem <= 0) {
		ptp->rexmt_rem--;
		sub_4e3c(ptp);
		return;
	}
	ptp->rexmt_rem--;

	if (ptp->state == 2) {
		sub_668c(ptp, 1);
	} else if (ptp->state == 3) {
		sub_668c(ptp, 2);
	} else if (ptp->state == 4) {
		ptp->snd_nxt = ptp->snd_una;
		sub_6878(ptp, 1);
		g_ptp_stats[28]++;
	}
	sceNet_lib_A8B6205A(ptp->timer_rexmt, ptp->rexmt_intvl, sub_6a54, ptp);
}

/* === sub_6b20 @ 0x6b20 === */
static void sub_6b20(PtpPcb *ptp)
{
	AdhocMbuf *m;

	if (ptp->state != 4)
		return;

	m = ptp->tx_head;
	if (m != NULL) {
		u32 *ts = (u32 *)((u8 *)m + 0x20);
		if ((ts[2] | ts[3]) != 0) {
			s64 now = sceKernelGetSystemTimeWide();
			s64 start = (s64)(((u64)ts[3] << 32) | ts[2]);
			s64 max_wait = (s64)ptp->rexmt_intvl * (s64)(ptp->rexmt_max + 1);
			if (now - start > max_wait) {
				sub_4e3c(ptp);
				return;
			}
		}
	}
	sceNet_lib_A8B6205A(ptp->timer_probe, ptp->rexmt_intvl, sub_6b20, ptp);
}

/* === sceNetAdhocGameModeCreateMaster @ 0x6c1c === */
s32 sceNetAdhocGameModeCreateMaster(void *buf, u32 size)
{
	u8 local_mac[16];
	s32 intr;
	s32 ret;

	if (_sceKernelCheckThreadStack() < 0x7d0)
		return 0x80410005;
	if (buf == NULL)
		return 0x80410711;
	if (size - 1 >= 0x400)
		return 0x80410705;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() == 0) {
		ret = 0x8041071b;
	} else {
		ret = sceNetGetLocalEtherAddr(local_mac);
		if (ret >= 0) {
			ret = sub_7188(local_mac, buf, size);
			if (ret >= 0) {
				g_gm_master_id = ret;
				ret = 0;
			}
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhocGameModeCreateReplica @ 0x6d0c === */
s32 sceNetAdhocGameModeCreateReplica(const u8 *mac, void *buf, u32 size)
{
	s32 intr;
	s32 ret;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (mac == NULL || buf == NULL)
		return 0x80410711;
	if (size - 1 >= 0x400)
		return 0x80410705;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() == 0) {
		ret = 0x8041071b;
	} else {
		ret = sub_7188(mac, buf, size);
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhocGameModeUpdateMaster @ 0x6df4 === */
s32 sceNetAdhocGameModeUpdateMaster(void)
{
	s32 intr;
	s32 ret;
	GameModeNode *gm;

	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() == 0) {
		ret = 0x8041071b;
	} else if (g_gm_master_id < 0) {
		ret = 0x8041071c;
	} else {
		for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
			if (gm->id == g_gm_master_id)
				break;
		}
		if (gm == NULL) {
			ret = 0x8041071c;
		} else {
			sceKernelMemcpy(gm->work_buf, gm->user_buf, gm->user_size);
			ret = sceNetAdhocAuth_lib_AAB06250(gm->work_buf, gm->work_size);
			if (ret >= 0)
				ret = sceWlanGPSend(0x10, gm->work_buf, gm->work_size);
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhocGameModeDeleteMaster @ 0x6f08 === */
s32 sceNetAdhocGameModeDeleteMaster(void)
{
	s32 intr;
	s32 ret;
	GameModeNode *gm;

	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (g_gm_master_id < 0) {
		ret = 0x8041071c;
	} else {
		for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
			if (gm->id == g_gm_master_id)
				break;
		}
		if (gm == NULL) {
			ret = 0x8041071c;
		} else {
			if (gm->next != NULL)
				gm->next->prev = gm->prev;
			else
				g_gm_list.tail = gm->prev;
			*gm->prev = gm->next;
			sceNetIfhandle_lib_76BAD213(gm->work_buf);
			sceNetIfhandle_lib_76BAD213(gm);
			ret = 0;
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sub_7008 @ 0x7008 === */
static s32 sub_7008(void)
{
	g_gm_list.head = NULL;
	g_gm_list.tail = &g_gm_list.head;
	return 0;
}

/* === sub_7020 @ 0x7020 === */
static void sub_7020(void)
{
	GameModeNode *gm;

	while ((gm = g_gm_list.head) != NULL) {
		if (gm->next != NULL)
			gm->next->prev = gm->prev;
		else
			g_gm_list.tail = gm->prev;
		*gm->prev = gm->next;
		sceNetIfhandle_lib_76BAD213(gm);
	}
}

/* === sceNetAdhocGameModeUpdateReplica @ 0x7084 === */
s32 sceNetAdhocGameModeUpdateReplica(s32 id, void *opt_data)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;
	return sub_7338(id, opt_data, 0);
}

/* === sceNetAdhoc_lib_AF6DFB15 @ 0x70e0 === */
s32 sceNetAdhoc_lib_AF6DFB15(s32 id, void *opt_data)
{
	return sub_7338(id, opt_data, 1);
}

/* === sceNetAdhocGameModeDeleteReplica @ 0x70fc === */
s32 sceNetAdhocGameModeDeleteReplica(s32 id)
{
	s32 intr;
	s32 ret;

	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else {
		ret = sub_72a4(id);
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sub_7188 @ 0x7188 === */
static s32 sub_7188(const u8 *mac, void *buf, u32 size)
{
	s32 id;
	GameModeNode *gm;
	u32 work_size;

	id = sceWlanGPPrevEstablishActive();
	if (id < 0)
		return id;

	for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
		if (gm->id == id)
			return 0x8041071a;
	}

	gm = (GameModeNode *)sceNetIfhandle_lib_15CFE3C0(0x24);
	if (gm == NULL)
		return 0x80410001;

	gm->id = id;
	sceKernelMemcpy(gm->mac, mac, 6);
	work_size = (size + 0x0f) & ~0x0fU;
	gm->user_buf = buf;
	gm->user_size = size;
	gm->work_size = work_size;
	gm->work_buf = sceNetIfhandle_lib_15CFE3C0(work_size);
	if (gm->work_buf == NULL) {
		sceNetIfhandle_lib_76BAD213(gm);
		return 0x80410001;
	}

	gm->next = NULL;
	gm->prev = g_gm_list.tail;
	*g_gm_list.tail = gm;
	g_gm_list.tail = &gm->next;
	return id;
}

/* === sub_72a4 @ 0x72a4 === */
static s32 sub_72a4(s32 id)
{
	GameModeNode *gm;

	for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
		if (gm->id == id)
			break;
	}
	if (gm == NULL)
		return 0x8041071c;

	if (gm->next != NULL)
		gm->next->prev = gm->prev;
	else
		g_gm_list.tail = gm->prev;
	*gm->prev = gm->next;
	sceNetIfhandle_lib_76BAD213(gm->work_buf);
	sceNetIfhandle_lib_76BAD213(gm);
	return 0;
}

/* === sub_7338 @ 0x7338 === */
static s32 sub_7338(s32 id, void *opt_data, s32 mode)
{
	u32 recv_info[6];
	s32 intr;
	s32 ret;
	GameModeNode *gm;

	if (id < 0)
		return 0x80410708;

	if (mode == 0 && opt_data != NULL) {
		if (*(u32 *)opt_data != 0x10)
			return 0x80410705;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() == 0) {
		ret = 0x8041071b;
	} else {
		for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
			if (gm->id == id)
				break;
		}
		if (gm == NULL) {
			ret = 0x8041071c;
		} else {
			ret = sceWlanGPRecv(id, gm->work_buf, gm->work_size, recv_info);
			if (ret >= 0) {
				if (recv_info[0] != 0) {
					ret = sceNetAdhocAuth_lib_015A8A64(gm->work_buf, gm->work_size);
					if (ret >= 0) {
						sceKernelMemcpy(gm->user_buf, gm->work_buf, gm->user_size);
						ret = 0;
					}
				}
				if (ret >= 0 && opt_data != NULL) {
					if (mode == 1) {
						*(u32 *)((u8 *)opt_data + 0x00) = recv_info[0];
						*(u32 *)((u8 *)opt_data + 0x08) = recv_info[2];
						*(u32 *)((u8 *)opt_data + 0x0c) = recv_info[3];
						*(u32 *)((u8 *)opt_data + 0x10) = recv_info[4];
					} else if (mode == 0) {
						*(u32 *)((u8 *)opt_data + 0x04) = recv_info[0];
						*(u32 *)((u8 *)opt_data + 0x08) = recv_info[2];
						*(u32 *)((u8 *)opt_data + 0x0c) = recv_info[3];
					}
				}
			}
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

s32 sceNetAdhoc_lib_67346A2A(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 nonblock)
	__attribute__((alias("sceNetAdhoc_67346A2A")));
