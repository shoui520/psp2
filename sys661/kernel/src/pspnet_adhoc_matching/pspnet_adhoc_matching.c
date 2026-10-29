/*
 * ePSP 6.61 pspnet_adhoc_matching.prx (sceNetAdhocMatching_Library v1.6)
 * Reimplemented from 3.650.011-TOOL pcff flash0:/kd/pspnet_adhoc_matching.prx
 */

#include <common_imp.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef struct SceKernelLwMutexWork {
	u32 data[8];
} SceKernelLwMutexWork;

typedef struct SceKernelVplInfo {
	u32 size;
	char name[32];
	u32 attr;
	int poolSize;
	int freeSize;
	int numWaitThreads;
} SceKernelVplInfo;

typedef struct SceNetAdhocMatchingPoolStat {
	int size;
	int maxsize;
	int freesize;
} SceNetAdhocMatchingPoolStat;

/* External imports */
extern int sceNetFreeThreadinfo(int thid);
extern int sceNet_lib_1858883D(int arg0);
extern int sceNet_lib_E0A81C7C(const void *mac1, const void *mac2);

extern int sceNetAdhocPdpCreate(const void *mac, u16 port, u32 bufsize, int flag);
extern int sceNetAdhoc_73BFD52D(int id, int flag);
extern int sceNetAdhocPdpDelete(int id, int flag);
extern int sceNetAdhocPdpSend(int id, const void *destMacAddr, u16 port, const void *data, unsigned int len, unsigned int timeout, int nonblock);
extern int sceNetAdhocPdpRecv(int id, void *srcMacAddr, u16 *port, void *data, void *dataLength, unsigned int timeout, int nonblock);

extern int sceKernelGetCompiledSdkVersion(void);
extern int sceWlanGetEtherAddr(void *etherAddr);

extern int sceKernelCpuSuspendIntr(void);
extern void *sceKernelMemset(void *dst, int c, u32 n);
extern int sceKernelLockLwMutex(SceKernelLwMutexWork *work, int count, u32 *timeout);
extern int _sceKernelCheckThreadStack(void);
extern int sceKernelUnlockLwMutex(SceKernelLwMutexWork *work, int count);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 n);
extern void sceKernelCpuResumeIntr(int intr);

extern int sceKernelGetModuleGPByAddress(const void *addr, u32 *gp);

extern int sceKernelSendMbx(int mbxid, void *msg);
extern int sceKernelStartThread(int thid, u32 arglen, void *argp);
extern int sceKernelWaitThreadEnd(int thid, u32 *timeout);
extern int sceKernelGetSystemTimeLow(void);
extern int sceKernelReferVplStatus(int uid, SceKernelVplInfo *info);
extern int sceKernelCreateThread(const char *name, int (*entry)(u32, void *), int initPriority, int stackSize, u32 attr, void *option);
extern int sceKernelCreateVpl(const char *name, int part, int attr, unsigned int size, void *opt);
extern int sceKernelDeleteLwMutex(SceKernelLwMutexWork *work);
extern int sceKernelSetAlarm(u32 clock, u32 (*handler)(void *), void *common);
extern int sceKernelCancelAlarm(int alarmid);
extern int sceKernelCreateMbx(const char *name, u32 attr, void *option);
extern int sceKernelDeleteMbx(int mbxid);
extern int sceKernelDeleteVpl(int uid);
extern int sceKernelReceiveMbx(int mbxid, void **pmessage, u32 *timeout);
extern int sceKernelDeleteThread(int thid);
extern int sceKernelCreateLwMutex(SceKernelLwMutexWork *work, const char *name, u32 attr, int initCount, void *opt);
extern int sceKernelTryAllocateVpl(int uid, unsigned int size, void **data);
extern int sceKernelFreeVpl(int uid, void *data);

/* Segment 1 globals (0x4b40..0x4b8c) */
int g_vpl_id = -1;                           /* 0x4b40 */
int g_is_init = 0;                           /* 0x4b44 */
SceNetAdhocMatchingPoolStat g_pool_stat;     /* 0x4b48: size(0x4b48), maxsize(0x4b4c), freesize(0x4b50) */
u32 g_max_alloc = 0;                         /* 0x4b54 */
void *g_ctx_list = NULL;                     /* 0x4b5c */
SceKernelLwMutexWork g_lock;                 /* 0x4b64..0x4b83 */
u8 g_local_mac[8];                           /* 0x4b84..0x4b8b */

/* Matching peer structure (0x88 bytes) */
typedef struct MatchingPeer {
	struct MatchingPeer *next; /* 0x00 */
	int state;                 /* 0x04 */
	u8 mac[6];                 /* 0x08 */
	u8 pad0e[2];               /* 0x0e */
	int field_10;              /* 0x10 */
	int member_count;          /* 0x14 */
	u8 *member_macs;           /* 0x18 */
	void *hello_buf;           /* 0x1c */
	int opt_len;               /* 0x20 */
	void *opt_data;            /* 0x24 */
	u32 last_recv_time;        /* 0x28 */
	u32 timeout_us;            /* 0x2c */
	u32 last_send_time;        /* 0x30 */
	u32 last_keepalive_time;   /* 0x34 */
	int field_38;              /* 0x38 */
	u8 pad3c[68];              /* 0x3c..0x7f */
	int bulk_len;              /* 0x80 */
	void *bulk_data;           /* 0x84 */
} MatchingPeer;

/* Note on MatchingPeer offsets from disassembly:
 * 0x00: next
 * 0x04: state
 * 0x08..0x0d: mac[6]
 * 0x14: member_count
 * 0x18: member_macs (inside hello_buf)
 * 0x1c: hello_buf
 * 0x24: opt_len
 * 0x28: opt_data
 * 0x2c: last_recv_time
 * 0x30: timeout_us
 * 0x34: last_send_time
 * 0x38: last_keepalive_time
 * 0x3c: keepalive_retry_count
 * 0x80: bulk_len
 * 0x84: bulk_data
 */
typedef struct PeerNode {
	struct PeerNode *next;     /* 0x00 */
	int state;                 /* 0x04 */
	u8 mac[6];                 /* 0x08 */
	u8 pad0e[2];               /* 0x0e */
	u32 pad10;                 /* 0x10 */
	int member_count;          /* 0x14 */
	u8 *member_macs;           /* 0x18 */
	u8 *hello_buf;             /* 0x1c */
	u32 pad20;                 /* 0x20 */
	int opt_len;               /* 0x24 */
	void *opt_data;            /* 0x28 */
	u32 last_recv_time;        /* 0x2c */
	u32 timeout_us;            /* 0x30 */
	u32 last_send_time;        /* 0x34 */
	u32 last_keepalive_time;   /* 0x38 */
	int keepalive_count;       /* 0x3c */
	u8 pad40[0x40];            /* 0x40..0x7f */
	int bulk_len;              /* 0x80 */
	void *bulk_data;           /* 0x84 */
} PeerNode;

/* Matching context structure (0x90 bytes) */
typedef struct MatchingCtx {
	struct MatchingCtx *next;  /* 0x00 */
	int id;                    /* 0x04 */
	int running;               /* 0x08 */
	int mode;                  /* 0x0c */
	int max_peers;             /* 0x10 */
	u16 port;                  /* 0x14 */
	u16 pad16;                 /* 0x16 */
	int rx_buflen;             /* 0x18 */
	void *rx_buf;              /* 0x1c */
	u32 hello_delay;           /* 0x20 */
	u32 ping_delay;            /* 0x24 */
	int init_count;            /* 0x28 */
	u32 rexmt_int;             /* 0x2c */
	u32 keepalive_timeout;     /* 0x30 */
	u32 timeout_total;         /* 0x34 */
	void (*handler)(int, int, void *, int, void *); /* 0x38 */
	u32 handler_gp;            /* 0x3c */
	int pdp_id;                /* 0x40 */
	int event_thid;            /* 0x44 */
	int input_thid;            /* 0x48 */
	int event_stack_size;      /* 0x4c */
	int input_stack_size;      /* 0x50 */
	int event_mbx;             /* 0x54 */
	int send_mbx;              /* 0x58 */
	int stop_event_thread;     /* 0x5c */
	int stop_input_thread;     /* 0x60 */
	int hello_pkt_len;         /* 0x64 */
	u8 *hello_pkt;             /* 0x68 */
	int hello_opt_len;         /* 0x6c */
	void *hello_opt_data;      /* 0x70 */
	PeerNode *peer_list;       /* 0x74 */
	int alarm1_id;             /* 0x78 */
	int alarm2_id;             /* 0x7c */
	int join_opt_len;          /* 0x80 */
	void *join_opt_data;       /* 0x84 */
	u8 local_mac[6];           /* 0x88 */
	u8 pad8e[2];               /* 0x8e */
} MatchingCtx;

typedef struct EventMsg {
	void *hdr_next;            /* 0x00 */
	u8 hdr_prio;               /* 0x04 */
	u8 hdr_resv[3];            /* 0x05 */
	struct EventMsg *next;     /* 0x08 */
	int event;                 /* 0x0c */
	u8 mac[6];                 /* 0x10 */
	u8 pad16[2];               /* 0x16 */
	int opt_len;               /* 0x18 */
	void *opt_data;            /* 0x1c */
	int state;                 /* 0x20 */
} EventMsg;

typedef struct SendMsg {
	void *hdr_next;            /* 0x00 */
	u8 hdr_prio;               /* 0x04 */
	u8 hdr_resv[3];            /* 0x05 */
	struct SendMsg *next;      /* 0x08 */
	int pkt_type;              /* 0x0c */
	u8 mac[6];                 /* 0x10 */
	u8 pad16[2];               /* 0x16 */
	int pkt_len;               /* 0x18 */
	u8 *pkt_data;              /* 0x1c */
} SendMsg;

/* Forward declarations of internal helpers */
static int sub_11ac(int matchingId, int evPri, int evStack, int inPri, int inStack, int optLen, const void *optData);
static int sub_1424(int matchingId, const u8 *mac, int optLen, const void *optData);
static void *sub_1678(u32 size);
static int sub_1710(u32 args, void *argp);
static MatchingCtx *sub_2890(int matchingId);
static int sub_28c8(void);
static MatchingCtx *sub_293c(u16 port);
static void sub_2974(MatchingCtx *ctx);
static int sub_29d0(MatchingCtx *ctx, int event, const u8 *mac, int optLen, const void *optData);
static int sub_2ac4(MatchingCtx *ctx, int pktType, const u8 *mac, int optLen, const void *optData);
static int sub_2ba8(MatchingCtx *ctx, const u8 *mac, int dataLen, const void *data);
static void sub_2d18(MatchingCtx *ctx);
static void sub_2ddc(MatchingCtx *ctx, PeerNode *peer);
static int sub_2e98(MatchingCtx *ctx, PeerNode *peer);
static u32 sub_2f08(void *common);
static u32 sub_2f68(void *common);
static void sub_2fc8(MatchingCtx *ctx, u32 now);
static int sub_3154(MatchingCtx *ctx);
static void sub_3208(MatchingCtx *ctx);
static int sub_330c(u32 args, void *argp);
static PeerNode *sub_3d64(MatchingCtx *ctx, const u8 *mac, int optLen, const void *optData);
static void sub_3de0(MatchingCtx *ctx, PeerNode *peer);
static int sub_3efc(MatchingCtx *ctx, PeerNode *peer, const u8 *rxBuf, u32 rxLen);
static void sub_4018(MatchingCtx *ctx, PeerNode *peer, u32 rxLen);
static PeerNode *sub_40b8(MatchingCtx *ctx, const u8 *mac, int optLen, const void *optData);
static PeerNode *sub_418c(MatchingCtx *ctx, const u8 *mac);
static PeerNode *sub_41ec(MatchingCtx *ctx, int state);
static int sub_4238(MatchingCtx *ctx, PeerNode *peer);
static void sub_4294(MatchingCtx *ctx);
static void sub_4354(MatchingCtx *ctx, int minState, u8 *dstMacs, int *pCount);
static int sub_4404(MatchingCtx *ctx, int minState);
static void sub_443c(MatchingCtx *ctx, int *pSize, u8 *buf);
static int sub_4504(const PeerNode *peer);

/* 0x0000: sceNetAdhocMatchingInit */
int sceNetAdhocMatchingInit(u32 memsize)
{
	if (_sceKernelCheckThreadStack() < 0xbe0)
		return 0x80410719;
	if (g_is_init != 0)
		return 0x80410812;
	if (memsize == 0)
		memsize = 0x8000;
	if (memsize < 0x800)
		return 0x80410815;

	int ret = sceKernelCreateLwMutex(&g_lock, "SceNetAdhocMatchingLock", 0x200, 0, NULL);
	if (ret < 0)
		return ret;

	int vpl = sceKernelCreateVpl("SceNetAdhocMatchingMemoryPool", 2, 0x100, memsize, NULL);
	if (vpl < 0) {
		sceKernelDeleteLwMutex(&g_lock);
		return 0x8041080c;
	}
	g_vpl_id = vpl;
	g_pool_stat.size = memsize - 0x20;
	g_pool_stat.maxsize = 0;
	g_pool_stat.freesize = memsize - 0x20;
	g_ctx_list = NULL;
	g_is_init = 1;
	return 0;
}

/* 0x00dc: sceNetAdhocMatchingCreate */
int sceNetAdhocMatchingCreate(int mode, int maxPeers, u16 port, int bufSize, u32 helloDelay, u32 pingDelay, int initCount, u32 msgDelay, void (*handler)(int, int, void *, int, void *))
{
	u32 gp_val[4];

	if (_sceKernelCheckThreadStack() < 0xb80)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	if ((u32)(mode - 1) >= 3u)
		return 0x80410801;
	if (mode == 3) {
		if (maxPeers != 2)
			return 0x80410802;
	} else {
		if ((u32)(maxPeers - 2) >= 0xfu)
			return 0x80410802;
	}
	if (port == 0)
		return 0x8041080b;
	if (bufSize <= 0)
		return 0x80410803;
	if (mode != 2 && helloDelay == 0)
		return 0x80410806;
	if (pingDelay == 0)
		return 0x80410806;
	if ((u32)(initCount - 1) >= 0xffu)
		return 0x80410808;
	if (msgDelay == 0)
		return 0x80410806;
	if (handler == NULL)
		return 0x80410807;

	sceKernelLockLwMutex(&g_lock, 1, NULL);
	if (sub_293c(port) != NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410810;
	}

	MatchingCtx *ctx = (MatchingCtx *)sub_1678(0x90);
	if (ctx == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080c;
	}
	sceKernelMemset(ctx, 0, 0x90);

	void *rx_buf = sub_1678(bufSize);
	ctx->rx_buf = rx_buf;
	if (rx_buf == NULL) {
		sceKernelFreeVpl(g_vpl_id, ctx);
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080c;
	}
	sceKernelMemset(rx_buf, 0, bufSize);

	int id = sub_28c8();
	if (id <= 0) {
		sceKernelFreeVpl(g_vpl_id, ctx->rx_buf);
		sceKernelFreeVpl(g_vpl_id, ctx);
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410809;
	}

	ctx->id = id;
	ctx->mode = mode;
	ctx->max_peers = maxPeers;
	ctx->port = port;
	ctx->rx_buflen = bufSize;
	ctx->hello_delay = helloDelay;
	ctx->ping_delay = pingDelay;
	ctx->init_count = initCount;
	ctx->rexmt_int = msgDelay;
	ctx->keepalive_timeout = pingDelay * initCount;
	ctx->timeout_total = pingDelay * initCount + 500000;
	ctx->handler = handler;

	if (sceKernelGetCompiledSdkVersion() >= 0x02060000) {
		if (sceKernelGetModuleGPByAddress(handler, gp_val) >= 0) {
			ctx->handler_gp = gp_val[0];
		} else {
			register u32 cur_gp asm("gp");
			ctx->handler_gp = cur_gp;
		}
	} else {
		ctx->handler_gp = 0;
	}

	ctx->next = (MatchingCtx *)g_ctx_list;
	g_ctx_list = ctx;
	sceKernelUnlockLwMutex(&g_lock, 1);
	return ctx->id;
}

/* Helper functions factored out to match original call structure */
static int sub_3c58(void)
{
	return sceKernelLockLwMutex(&g_lock, 1, NULL);
}

static void sub_3c80(int lock_ret)
{
	sceKernelUnlockLwMutex(&g_lock, 1);
}

static void sub_32a4(MatchingCtx *ctx)
{
	if (ctx->input_thid > 0) {
		ctx->stop_input_thread = 1;
		sceKernelUnlockLwMutex(&g_lock, 1);
		sceKernelWaitThreadEnd(ctx->input_thid, NULL);
		sceKernelLockLwMutex(&g_lock, 1, NULL);
		sceNetFreeThreadinfo(ctx->input_thid);
		sceKernelDeleteThread(ctx->input_thid);
		ctx->input_thid = 0;
	}
}

static void sub_16ac(MatchingCtx *ctx)
{
	if (ctx->event_thid > 0) {
		ctx->stop_event_thread = 1;
		sceKernelUnlockLwMutex(&g_lock, 1);
		sceKernelWaitThreadEnd(ctx->event_thid, NULL);
		sceKernelLockLwMutex(&g_lock, 1, NULL);
		sceNetFreeThreadinfo(ctx->event_thid);
		sceKernelDeleteThread(ctx->event_thid);
		ctx->event_thid = 0;
	}
}

static void sub_2c10(MatchingCtx *ctx)
{
	sceKernelCancelAlarm(ctx->alarm1_id);
	sceKernelCancelAlarm(ctx->alarm2_id);
	sub_2d18(ctx);
	sceKernelDeleteMbx(ctx->event_mbx);
	ctx->event_mbx = 0;
	sceKernelDeleteMbx(ctx->send_mbx);
	ctx->send_mbx = 0;
	sceNetAdhocPdpDelete(ctx->pdp_id, 0);
	ctx->pdp_id = 0;
}

static int sub_2d88(MatchingCtx *ctx, int optLen, const void *optData)
{
	if (optLen < 0 || (optLen != 0 && optData == NULL))
		return 0x80410805;
	if (optLen > ctx->rx_buflen)
		return 0x80410805;

	void *new_buf = NULL;
	if (optLen > 0) {
		new_buf = sub_1678(optLen);
		if (new_buf == NULL)
			return 0x8041080c;
		sceKernelMemcpy(new_buf, optData, optLen);
	}
	if (ctx->hello_opt_len > 0)
		sceKernelFreeVpl(g_vpl_id, ctx->hello_opt_data);
	ctx->hello_opt_len = optLen;
	ctx->hello_opt_data = new_buf;
	return 0;
}

static int sub_3ea0(SceNetAdhocMatchingPoolStat *poolstat)
{
	SceKernelVplInfo info;

	sceKernelMemset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	int ret = sceKernelReferVplStatus(g_vpl_id, &info);
	if (ret < 0)
		return 0;

	g_pool_stat.size = info.poolSize;
	g_pool_stat.freesize = info.freeSize;
	g_pool_stat.maxsize = g_max_alloc;
	sceKernelMemcpy(poolstat, &g_pool_stat, sizeof(g_pool_stat));
	return 0;
}

static void sub_443c_all(MatchingCtx *ctx, int *pSize, void *buf)
{
	if (ctx->mode == 2) {
		if (sub_41ec(ctx, 7) != NULL) {
			sub_443c(ctx, pSize, (u8 *)buf);
		} else {
			*pSize = 0;
		}
	} else {
		int count = sub_4404(ctx, 7);
		if (count <= 0) {
			*pSize = 0;
		} else if (ctx->mode == 1) {
			sub_443c(ctx, pSize, (u8 *)buf);
		} else {
			u8 *p = (u8 *)buf;
			if (p != NULL) {
				if (*pSize < 24) {
					*pSize = 0;
				} else {
					PeerNode *peer = sub_41ec(ctx, 7);
					*(void **)(p + 0) = p + 12;
					sceKernelMemcpy(p + 4, ctx->local_mac, 6);
					*(void **)(p + 12) = NULL;
					sceKernelMemcpy(p + 16, peer->mac, 6);
					*pSize = 24;
				}
			} else {
				*pSize = (count + 1) * 12;
			}
		}
	}
}

/* 0x0304: sceNetAdhocMatchingStop */
int sceNetAdhocMatchingStop(int matchingId)
{
	if (_sceKernelCheckThreadStack() < 0xbe0)
		return 0x80410005;
	if (g_is_init == 0)
		return 0x80410813;

	int lret = sub_3c58();
	if (lret < 0)
		return lret;

	MatchingCtx *ctx = sub_2890(matchingId);
	if (ctx == NULL) {
		sub_3c80(lret);
		return 0x80410807;
	}
	if (ctx->running == 0) {
		sub_3c80(lret);
		return 0x80410814;
	}
	ctx->running = 0;

	if (ctx->pdp_id > 0)
		sceNetAdhoc_73BFD52D(ctx->pdp_id, 0);

	sub_32a4(ctx);
	sub_16ac(ctx);
	sub_4294(ctx);
	sub_2c10(ctx);
	sub_3c80(lret);
	return 0;
}

/* 0x0424: sceNetAdhocMatchingDelete */
int sceNetAdhocMatchingDelete(int matchingId)
{
	if (_sceKernelCheckThreadStack() < 0xfa0)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	if (matchingId <= 0)
		return 0x80410804;

	sceKernelLockLwMutex(&g_lock, 1, NULL);
	MatchingCtx *ctx = sub_2890(matchingId);
	if (ctx == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410804;
	}
	if (ctx->running != 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080a;
	}
	sceKernelFreeVpl(g_vpl_id, ctx->rx_buf);
	sub_2974(ctx);
	sceKernelFreeVpl(g_vpl_id, ctx);
	sceKernelUnlockLwMutex(&g_lock, 1);
	return 0;
}

/* 0x04f0: sceNetAdhocMatchingSelectTarget */
int sceNetAdhocMatchingSelectTarget(int matchingId, const u8 *mac, int optLen, const void *optData)
{
	if (_sceKernelCheckThreadStack() < 0xb80)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	if (matchingId <= 0)
		return 0x80410804;
	if (mac == NULL)
		return 0x8041080f;
	if (optLen < 0 || (optLen != 0 && optData == NULL))
		return 0x80410805;

	sceKernelLockLwMutex(&g_lock, 1, NULL);
	MatchingCtx *ctx = sub_2890(matchingId);
	if (ctx == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410804;
	}
	if (ctx->running == 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410814;
	}
	if (optLen > ctx->rx_buflen) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410805;
	}

	PeerNode *peer = sub_418c(ctx, mac);
	if (peer == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080e;
	}

	int mode = ctx->mode;
	int ret = 0;
	if (mode == 2) {
		if (sub_41ec(ctx, 1) != NULL) {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x80410811;
		}
		if (peer->state != 1) {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x80410811;
		}
		if (sub_4404(ctx, 4) != 0) {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x80410811;
		}
		ret = sub_2ac4(ctx, 2, mac, optLen, optData);
		if (ret == 0) {
			peer->timeout_us = ctx->rexmt_int;
			peer->state = 4;
		}
	} else if (mode < 2) {
		if (mode == 1) {
			if (peer->state != 3) {
				sceKernelUnlockLwMutex(&g_lock, 1);
				return 0x80410811;
			}
			if (sub_4404(ctx, 7) >= ctx->max_peers - 1) {
				sceKernelUnlockLwMutex(&g_lock, 1);
				return 0x8041080d;
			}
			if (optLen > 0) {
				if (ctx->join_opt_len != 0) {
					ctx->join_opt_len = 0;
					sceKernelFreeVpl(g_vpl_id, ctx->join_opt_data);
					ctx->join_opt_data = NULL;
				}
				void *buf = sub_1678(optLen);
				ctx->join_opt_data = buf;
				if (buf == NULL) {
					sceKernelUnlockLwMutex(&g_lock, 1);
					return 0x8041080c;
				}
				sceKernelMemcpy(buf, optData, optLen);
				ctx->join_opt_len = optLen;
			}
			ret = sub_2ac4(ctx, 3, mac, optLen, optData);
			if (ret == 0) {
				peer->timeout_us = ctx->rexmt_int;
				peer->state = 5;
			}
		}
	} else if (mode == 3) {
		if (sub_4404(ctx, 7) >= ctx->max_peers - 1) {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x8041080d;
		}
		int st = peer->state;
		if (st == 1) {
			if (sub_4404(ctx, 4) != 0) {
				sceKernelUnlockLwMutex(&g_lock, 1);
				return 0x80410811;
			}
			ret = sub_2ac4(ctx, 2, mac, optLen, optData);
			if (ret == 0) {
				peer->timeout_us = ctx->rexmt_int;
				peer->state = 4;
			}
		} else if (st == 3) {
			if (optLen > 0) {
				if (ctx->join_opt_len != 0) {
					ctx->join_opt_len = 0;
					sceKernelFreeVpl(g_vpl_id, ctx->join_opt_data);
					ctx->join_opt_data = NULL;
				}
				void *buf = sub_1678(optLen);
				ctx->join_opt_data = buf;
				if (buf == NULL) {
					sceKernelUnlockLwMutex(&g_lock, 1);
					return 0x8041080c;
				}
				sceKernelMemcpy(buf, optData, optLen);
				ctx->join_opt_len = optLen;
			}
			ret = sub_2ac4(ctx, 3, mac, optLen, optData);
			if (ret == 0) {
				peer->timeout_us = ctx->rexmt_int;
				peer->state = 5;
			}
		} else {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x80410811;
		}
	}

	sceKernelUnlockLwMutex(&g_lock, 1);
	return ret;
}

/* 0x07f4: sceNetAdhocMatchingSetHelloOpt */
int sceNetAdhocMatchingSetHelloOpt(int matchingId, int optLen, const void *optData)
{
	if (_sceKernelCheckThreadStack() < 0xbe0)
		return 0x80410005;
	if (g_is_init == 0)
		return 0x80410813;

	int lret = sub_3c58();
	if (lret < 0)
		return lret;

	int ret = 0x80410807;
	MatchingCtx *ctx = sub_2890(matchingId);
	if (ctx != NULL) {
		if (ctx->mode == 2) {
			ret = 0x80410801;
		} else if (ctx->running == 0) {
			ret = 0x8041080b;
		} else {
			ret = sub_2d88(ctx, optLen, optData);
		}
	}
	sub_3c80(lret);
	return ret;
}

/* 0x08e0: sceNetAdhocMatchingGetHelloOpt */
int sceNetAdhocMatchingGetHelloOpt(int matchingId, int *pOptLen, void *optData)
{
	if (_sceKernelCheckThreadStack() < 0xbd0)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	if (matchingId <= 0)
		return 0x80410804;
	if (pOptLen == NULL)
		return 0x8041080f;

	sceKernelLockLwMutex(&g_lock, 1, NULL);
	MatchingCtx *ctx = sub_2890(matchingId);
	if (ctx == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410804;
	}
	if (ctx->mode == 2) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080a;
	}
	if (ctx->running == 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410814;
	}

	int cur_len = ctx->hello_opt_len;
	if (cur_len > 0 && optData != NULL) {
		int copy_len = *pOptLen;
		if (cur_len < copy_len)
			copy_len = cur_len;
		if (copy_len > 0)
			sceKernelMemcpy(optData, ctx->hello_opt_data, copy_len);
		cur_len = ctx->hello_opt_len;
	}
	*pOptLen = cur_len;
	sceKernelUnlockLwMutex(&g_lock, 1);
	return 0;
}

/* 0x0a08: sceNetAdhocMatchingGetMembers */
int sceNetAdhocMatchingGetMembers(int matchingId, int *pSize, void *buf)
{
	if (_sceKernelCheckThreadStack() < 0xbe0)
		return 0x80410005;
	if (g_is_init == 0)
		return 0x80410813;
	if (((u32)buf & 3) != 0)
		return 0x80410819;
	if (pSize == NULL)
		return 0x80410806;

	int lret = sub_3c58();
	if (lret < 0)
		return lret;

	int ret = 0;
	MatchingCtx *ctx = sub_2890(matchingId);
	if (ctx == NULL) {
		ret = 0x80410807;
	} else if (ctx->running == 0) {
		ret = 0x8041080b;
	} else {
		sub_443c_all(ctx, pSize, buf);
	}
	sub_3c80(lret);
	return ret;
}

/* 0x0b00: sceNetAdhocMatchingSendData */
int sceNetAdhocMatchingSendData(int matchingId, const u8 *mac, int dataLen, const void *data)
{
	if (_sceKernelCheckThreadStack() < 0xfa0)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	if (matchingId <= 0)
		return 0x80410804;
	if (mac == NULL)
		return 0x8041080f;
	if (dataLen <= 0 || data == NULL)
		return 0x80410805;

	sceKernelLockLwMutex(&g_lock, 1, NULL);
	MatchingCtx *ctx = sub_2890(matchingId);
	if (ctx == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410804;
	}
	if (ctx->running == 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410814;
	}
	if (dataLen > ctx->rx_buflen) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410805;
	}

	PeerNode *peer = sub_418c(ctx, mac);
	if (peer == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080e;
	}

	int st = peer->state;
	if (st != 7) {
		if (st == 8) {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x80410817;
		}
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080e;
	}

	if (ctx->mode == 2 && sub_41ec(ctx, 1) != NULL) {
		u8 *p = ctx->hello_pkt + 4;
		int found = 0;
		for (int i = 4; i < ctx->hello_pkt_len; i += 6, p += 6) {
			if (sceNet_lib_E0A81C7C(p, mac) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x8041080e;
		}
	}

	int ret = sub_2ba8(ctx, mac, dataLen, data);
	if (ret != 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return ret;
	}

	if (peer->bulk_len > 0) {
		peer->bulk_len = 0;
		sceKernelFreeVpl(g_vpl_id, peer->bulk_data);
		peer->bulk_data = NULL;
	}

	void *buf = sub_1678(dataLen);
	peer->bulk_data = buf;
	if (buf == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080c;
	}
	sceKernelMemcpy(buf, data, dataLen);
	peer->bulk_len = dataLen;
	peer->timeout_us = ctx->rexmt_int;
	peer->state = 8;
	sceKernelUnlockLwMutex(&g_lock, 1);
	return 0;
}

/* 0x0d00: sceNetAdhocMatchingAbortSendData */
int sceNetAdhocMatchingAbortSendData(int matchingId, const u8 *mac)
{
	if (_sceKernelCheckThreadStack() < 0xbb0)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	if (matchingId <= 0)
		return 0x80410804;
	if (mac == NULL)
		return 0x8041080f;

	sceKernelLockLwMutex(&g_lock, 1, NULL);
	MatchingCtx *ctx = sub_2890(matchingId);
	if (ctx == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410804;
	}
	if (ctx->running == 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410814;
	}

	PeerNode *peer = sub_418c(ctx, mac);
	if (peer == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080e;
	}
	if (peer->state != 8) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080a;
	}

	peer->state = 7;
	peer->timeout_us = ctx->keepalive_timeout;
	if (peer->bulk_len > 0) {
		peer->bulk_len = 0;
		sceKernelFreeVpl(g_vpl_id, peer->bulk_data);
		peer->bulk_data = NULL;
	}
	sub_2ddc(ctx, peer);
	sceKernelUnlockLwMutex(&g_lock, 1);
	return 0;
}

/* 0x0e08: sceNetAdhocMatchingTerm */
int sceNetAdhocMatchingTerm(void)
{
	if (_sceKernelCheckThreadStack() < 0xbe0)
		return 0x80410719;
	if (g_is_init == 0)
		return 0;
	if (g_ctx_list != NULL)
		return 0x8041080a;

	int ret = sceKernelDeleteLwMutex(&g_lock);
	if (ret < 0)
		return ret;
	ret = sceKernelDeleteVpl(g_vpl_id);
	if (ret < 0)
		return ret;
	g_vpl_id = -1;
	g_is_init = 0;
	return 0;
}

/* 0x0e88: sceNetAdhocMatchingStart */
int sceNetAdhocMatchingStart(int matchingId, int evPri, int evStack, int inPri, int inStack, int optLen, const void *optData)
{
	if (_sceKernelCheckThreadStack() < 0xb80)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	if (matchingId <= 0)
		return 0x80410804;
	if ((u32)(evPri - 8) >= 0x69u || (u32)(inPri - 8) >= 0x69u)
		return 0x8041080a;
	if (evStack < 0x1c00 || inStack < 0x1000)
		return 0x80410818;
	if (optLen < 0 || (optLen != 0 && optData == NULL))
		return 0x80410805;

	return sub_11ac(matchingId, evPri, evStack, inPri, inStack, optLen, optData);
}

/* 0x0f34: sceNetAdhocMatching_E8454C65 */
int sceNetAdhocMatching_E8454C65(int matchingId, int evPri, int evStack, int inPri, int inStack, int optLen, const void *optData)
{
	if (_sceKernelCheckThreadStack() < 0xb80)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	if (matchingId <= 0)
		return 0x80410804;
	if ((u32)(evPri - 8) >= 0x69u || (u32)(inPri - 8) >= 0x69u)
		return 0x8041080a;
	if (evStack < 0x400 || inStack < 0x400)
		return 0x80410818;
	if (optLen < 0 || (optLen != 0 && optData == NULL))
		return 0x80410805;

	return sub_11ac(matchingId, evPri, evStack, inPri, inStack, optLen, optData);
}

/* 0x0ff0: sceNetAdhocMatchingCancelTarget */
int sceNetAdhocMatchingCancelTarget(int matchingId, const u8 *mac)
{
	if (_sceKernelCheckThreadStack() < 0xb80)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	if (matchingId <= 0)
		return 0x80410804;
	if (mac == NULL)
		return 0x8041080f;

	return sub_1424(matchingId, mac, 0, NULL);
}

/* 0x1050: sceNetAdhocMatchingCancelTargetWithOpt */
int sceNetAdhocMatchingCancelTargetWithOpt(int matchingId, const u8 *mac, int optLen, const void *optData)
{
	if (_sceKernelCheckThreadStack() < 0xb80)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	if (matchingId <= 0)
		return 0x80410804;
	if (mac == NULL)
		return 0x8041080f;
	if (optLen < 0 || (optLen != 0 && optData == NULL))
		return 0x80410805;

	return sub_1424(matchingId, mac, optLen, optData);
}

/* 0x10c8: sceNetAdhocMatchingGetPoolMaxAlloc */
int sceNetAdhocMatchingGetPoolMaxAlloc(void)
{
	if (_sceKernelCheckThreadStack() < 0xbe0)
		return 0x80410719;
	if (g_is_init == 0)
		return 0x80410813;
	return g_max_alloc;
}

/* 0x1104: sceNetAdhocMatchingGetPoolStat */
int sceNetAdhocMatchingGetPoolStat(SceNetAdhocMatchingPoolStat *poolstat)
{
	if (_sceKernelCheckThreadStack() < 0xbf0)
		return 0x80410005;
	if (g_is_init == 0)
		return 0x80410813;
	if (poolstat == NULL)
		return 0x80410806;

	return sub_3ea0(poolstat);
}

/* 0x11ac: sub_11ac */
static int sub_11ac(int matchingId, int evPri, int evStack, int inPri, int inStack, int optLen, const void *optData)
{
	sceKernelLockLwMutex(&g_lock, 1, NULL);
	MatchingCtx *ctx = sub_2890(matchingId);
	if (ctx == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410804;
	}
	if (ctx->running != 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410812;
	}
	if (optLen > 0) {
		if (ctx->mode == 2) {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x8041080a;
		}
		if (optLen > ctx->rx_buflen) {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x80410805;
		}
	}

	int ret = sceWlanGetEtherAddr(ctx->local_mac);
	if (ret < 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return ret;
	}
	ret = sceWlanGetEtherAddr(g_local_mac);
	if (ret < 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return ret;
	}

	int pdp = sceNetAdhocPdpCreate(ctx->local_mac, ctx->port, ctx->rx_buflen * 2, 0);
	ctx->pdp_id = pdp;
	if (pdp < 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return pdp;
	}

	ret = sub_3154(ctx);
	if (ret < 0)
		goto fail_pdp;

	if (optLen > 0) {
		void *buf = sub_1678(optLen);
		ctx->hello_opt_data = buf;
		if (buf == NULL) {
			ret = 0x8041080c;
			goto fail_hello;
		}
		sceKernelMemcpy(buf, optData, optLen);
	}
	ctx->hello_opt_len = optLen;

	int mbx1 = sceKernelCreateMbx("SceNetAdhocMatchingEvent", 0, NULL);
	ctx->event_mbx = mbx1;
	if (mbx1 <= 0) {
		ret = mbx1;
		goto fail_opt;
	}

	int mbx2 = sceKernelCreateMbx("SceNetAdhocMatchingInput", 0, NULL);
	ctx->send_mbx = mbx2;
	if (mbx2 <= 0) {
		ret = mbx2;
		goto fail_mbx1;
	}

	ctx->event_stack_size = evStack;
	ctx->input_stack_size = inStack;
	ctx->stop_event_thread = 0;

	int th1 = sceKernelCreateThread("SceNetAdhocMatchingEvent", sub_1710, evPri, evStack, 0, NULL);
	ctx->event_thid = th1;
	if (th1 <= 0) {
		ret = th1;
		goto fail_mbx2;
	}
	ret = sceKernelStartThread(th1, 4, &ctx->id);
	if (ret < 0)
		goto fail_th1;

	ctx->stop_input_thread = 0;
	int th2 = sceKernelCreateThread("SceNetAdhocMatchingInput", sub_330c, inPri, inStack, 0, NULL);
	ctx->input_thid = th2;
	if (th2 <= 0) {
		ret = th2;
		goto fail_th1_run;
	}
	ret = sceKernelStartThread(th2, 4, &ctx->id);
	if (ret < 0)
		goto fail_th2;

	ctx->running = 1;
	sceKernelUnlockLwMutex(&g_lock, 1);
	return 0;

fail_th2:
	sceNetFreeThreadinfo(ctx->input_thid);
	sceKernelDeleteThread(ctx->input_thid);
fail_th1_run:
	ctx->stop_event_thread = 1;
	sceKernelWaitThreadEnd(ctx->event_thid, NULL);
fail_th1:
	sceNetFreeThreadinfo(ctx->event_thid);
	sceKernelDeleteThread(ctx->event_thid);
fail_mbx2:
	sceKernelDeleteMbx(ctx->send_mbx);
fail_mbx1:
	sceKernelDeleteMbx(ctx->event_mbx);
fail_opt:
	if (optLen > 0) {
		sceKernelFreeVpl(g_vpl_id, ctx->hello_opt_data);
		ctx->hello_opt_data = NULL;
	}
fail_hello:
	sceKernelFreeVpl(g_vpl_id, ctx->hello_pkt);
	ctx->hello_pkt = NULL;
fail_pdp:
	sceNetAdhocPdpDelete(ctx->pdp_id, 0);
	sceKernelUnlockLwMutex(&g_lock, 1);
	return ret;
}

/* 0x1424: sub_1424 */
static int sub_1424(int matchingId, const u8 *mac, int optLen, const void *optData)
{
	sceKernelLockLwMutex(&g_lock, 1, NULL);
	MatchingCtx *ctx = sub_2890(matchingId);
	if (ctx == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410804;
	}
	if (ctx->running == 0) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x80410814;
	}

	PeerNode *peer = sub_418c(ctx, mac);
	if (peer == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0x8041080e;
	}

	int mode = ctx->mode;
	int st = peer->state;
	if (mode == 2) {
		if (st != 4 && st != 7 && st != 8) {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x80410811;
		}
	} else if (mode < 2) {
		if (mode == 1) {
			if (st != 3 && st != 5 && st != 7 && st != 8) {
				sceKernelUnlockLwMutex(&g_lock, 1);
				return 0x80410811;
			}
		}
	} else if (mode == 3) {
		if ((u32)(st - 3) >= 6u || st == 6) {
			sceKernelUnlockLwMutex(&g_lock, 1);
			return 0x80410811;
		}
	}

	if (peer->bulk_len > 0) {
		peer->bulk_len = 0;
		sceKernelFreeVpl(g_vpl_id, peer->bulk_data);
		peer->bulk_data = NULL;
	}
	sub_2ddc(ctx, peer);

	int ret = 0;
	if (ctx->mode == 1 && peer->state == 3) {
		ret = sub_2ac4(ctx, 5, mac, optLen, optData);
		if (ret == 0) {
			peer->timeout_us = ctx->timeout_total;
			peer->state = 2;
		}
	} else if (ctx->mode == 1 && peer->state == 5) {
		ret = sub_2ac4(ctx, 6, mac, optLen, optData);
		if (ret == 0)
			sub_4238(ctx, peer);
	} else if (ctx->mode == 2) {
		PeerNode *parent = sub_41ec(ctx, 1);
		if (parent != NULL && parent != peer) {
			if (peer->state == 7 || peer->state == 8) {
				sub_4238(ctx, peer);
			} else {
				sceKernelUnlockLwMutex(&g_lock, 1);
				return 0x8041080e;
			}
		} else {
			ret = sub_2ac4(ctx, 6, mac, optLen, optData);
			if (ret == 0)
				sub_4294(ctx);
		}
	} else if (ctx->mode == 3 && peer->state == 3) {
		ret = sub_2ac4(ctx, 5, mac, optLen, optData);
		if (ret == 0) {
			peer->timeout_us = ctx->timeout_total;
			peer->state = 1;
		}
	} else {
		ret = sub_2ac4(ctx, 6, mac, optLen, optData);
		if (ret == 0) {
			sub_4238(ctx, peer);
			int count = sub_4404(ctx, 7);
			if (count > 0) {
				u8 *pkt = ctx->hello_pkt;
				int pkt_len = (count + 1) * 6 + 4;
				ctx->hello_pkt_len = pkt_len;
				sceKernelMemset(pkt, 0, pkt_len);
				int actual = count;
				sub_4354(ctx, 7, pkt + 10, &actual);
				pkt[0] = 8;
				int total_members = actual + 1;
				pkt[2] = (u8)((u32)total_members >> 8);
				pkt[3] = (u8)total_members;
				sceKernelMemcpy(pkt + 4, ctx->local_mac, 6);
				for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
					if (it->state >= 7 && sceNet_lib_E0A81C7C(it->mac, mac) != 0)
						sub_2ac4(ctx, 8, it->mac, 0, NULL);
				}
			} else {
				sub_3154(ctx);
			}
		}
	}

	sceKernelUnlockLwMutex(&g_lock, 1);
	return ret;
}

/* 0x1678: sub_1678 */
static void *sub_1678(u32 size)
{
	void *ptr;
	SceKernelVplInfo info;

	int intr = sceKernelCpuSuspendIntr();
	int ret = sceKernelTryAllocateVpl(g_vpl_id, size, &ptr);
	if (ret != 0) {
		sceKernelCpuResumeIntr(intr);
		return NULL;
	}
	sceKernelMemset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	if (sceKernelReferVplStatus(g_vpl_id, &info) >= 0) {
		u32 used = info.poolSize - info.freeSize;
		if (g_max_alloc < used)
			g_max_alloc = used;
	}
	sceKernelCpuResumeIntr(intr);
	return ptr;
}

/* Helper to call user handler with its module GP */
static void call_handler(MatchingCtx *ctx, int id, int event, void *mac, int optLen, void *optData)
{
	void (*fn)(int, int, void *, int, void *) = ctx->handler;
	u32 new_gp = ctx->handler_gp;
	if (new_gp != 0) {
		u32 saved_gp;
		__asm__ volatile (
			"move %0, $gp\n\t"
			"move $gp, %1"
			: "=&r"(saved_gp)
			: "r"(new_gp)
		);
		fn(id, event, mac, optLen, optData);
		__asm__ volatile (
			"move $gp, %0"
			:
			: "r"(saved_gp)
		);
	} else {
		fn(id, event, mac, optLen, optData);
	}
}

/* 0x1710: sub_1710 ("SceNetAdhocMatchingEvent" thread) */
static int sub_1710(u32 args, void *argp)
{
	u32 timeout;
	EventMsg *msg;

	sceKernelLockLwMutex(&g_lock, 1, NULL);
	MatchingCtx *ctx = sub_2890(*(int *)argp);
	if (ctx == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0;
	}
	sceNet_lib_1858883D(0);

	while (!ctx->stop_event_thread) {
		msg = NULL;
		timeout = 100000;
		sceKernelUnlockLwMutex(&g_lock, 1);
		int ret = sceKernelReceiveMbx(ctx->event_mbx, (void **)&msg, &timeout);
		sceKernelLockLwMutex(&g_lock, 1, NULL);
		if (ret != 0)
			continue;

		PeerNode *peer = sub_418c(ctx, msg->mac);
		if (peer != NULL) {
			int ev = msg->event;
			int mode = ctx->mode;
			int st = peer->state;

			switch (ev) {
			case 1: /* HELLO */
				if ((mode == 2 && st == 1 && sub_41ec(ctx, 1) == NULL) ||
				    (mode == 3 && st == 1)) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, ev, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
				}
				break;

			case 2: /* JOIN */
				if ((mode == 1 && st == 3) || (mode == 3 && st == 3)) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, ev, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
				}
				break;

			case 3: /* LEFT */
				if ((mode == 1 && (st == 2 || st == 7 || st == 8)) ||
				    (mode == 2 && (st == 7 || st == 8)) ||
				    (mode == 3 && (st == 1 || st == 7 || st == 8))) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, ev, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
					peer = sub_418c(ctx, msg->mac);
					if (peer != NULL && (peer->state == 7 || peer->state == 8)) {
						if (ctx->mode == 1 || ctx->mode == 3) {
							sub_4238(ctx, peer);
							int count = sub_4404(ctx, 7);
							if (count > 0) {
								u8 *pkt = ctx->hello_pkt;
								int pkt_len = (count + 1) * 6 + 4;
								ctx->hello_pkt_len = pkt_len;
								sceKernelMemset(pkt, 0, pkt_len);
								int actual = count;
								sub_4354(ctx, 7, pkt + 10, &actual);
								pkt[0] = 8;
								int total = actual + 1;
								pkt[2] = (u8)((u32)total >> 8);
								pkt[3] = (u8)total;
								sceKernelMemcpy(pkt + 4, ctx->local_mac, 6);
								for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
									if (it->state >= 7 && sceNet_lib_E0A81C7C(it->mac, msg->mac) != 0)
										sub_2ac4(ctx, 8, it->mac, 0, NULL);
								}
							} else {
								sub_3154(ctx);
							}
						} else if (ctx->mode == 2) {
							PeerNode *parent = sub_41ec(ctx, 1);
							if (parent != NULL) {
								if (parent == peer) {
									sub_4294(ctx);
								} else if (peer->state == 7 || peer->state == 8) {
									sub_4238(ctx, peer);
								}
							}
						}
					}
				}
				break;

			case 4: /* REJECT */
				if ((mode == 2 && st == 1) || (mode == 3 && st == 1)) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, ev, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
				}
				break;

			case 5: /* CANCEL */
				if ((mode == 1 && st == 2) ||
				    (mode == 2 && st == 1) ||
				    (mode == 3 && st == 1)) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, ev, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
				}
				break;

			case 6: /* ACCEPT */
				if ((mode == 1 && st == 6) ||
				    (mode == 2 && st == 6) ||
				    (mode == 3 && st == 6)) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, ev, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
				}
				break;

			case 7: /* COMPLETE */
				if ((mode == 1 && (st == 7 || st == 8)) ||
				    (mode == 2 && (st == 7 || st == 8)) ||
				    (mode == 3 && (st == 7 || st == 8))) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, ev, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
					peer = sub_418c(ctx, msg->mac);
					if (peer != NULL) {
						if (ctx->mode == 2) {
							u8 *pmac = peer->member_macs + 6;
							for (int i = 1; i < peer->member_count; i++, pmac += 6) {
								if (sceNet_lib_E0A81C7C(ctx->local_mac, pmac) == 0)
									continue;
								PeerNode *m = sub_418c(ctx, pmac);
								if (m == NULL) {
									m = sub_40b8(ctx, pmac, 0, NULL);
									if (m == NULL)
										break;
								}
								m->state = 7;
								m->timeout_us = 0;
								sceKernelUnlockLwMutex(&g_lock, 1);
								call_handler(ctx, ctx->id, 7, pmac, 0, NULL);
								sceKernelLockLwMutex(&g_lock, 1, NULL);
							}
							int total = peer->member_count;
							u8 *pkt = ctx->hello_pkt;
							ctx->hello_pkt_len = total * 6 + 4;
							pkt[0] = 8;
							pkt[1] = 0;
							pkt[2] = (u8)((u32)total >> 8);
							pkt[3] = (u8)total;
							sceKernelMemcpy(pkt + 4, peer->member_macs, total * 6);
							peer->member_count = 0;
							peer->member_macs = NULL;
							sceKernelFreeVpl(g_vpl_id, peer->hello_buf);
							peer->hello_buf = NULL;
						} else if (ctx->mode == 1 || ctx->mode == 3) {
							for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
								if (it->state >= 7 && sceNet_lib_E0A81C7C(it->mac, peer->mac) != 0)
									sub_2ac4(ctx, 7, it->mac, 0, NULL);
							}
							if (ctx->mode == 3) {
								for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
									if (it != peer && (it->state == 3 || it->state == 4 || it->state == 5)) {
										if (sub_2ac4(ctx, 6, it->mac, 0, NULL) == 0) {
											it->timeout_us = ctx->timeout_total;
											it->state = 1;
										}
									}
								}
							}
						}
					}
				}
				break;

			case 8: /* TIMEOUT */
				{
					int mstate = msg->state;
					if ((mode == 1 && (mstate == 3 || mstate == 5 || mstate == 6 || mstate == 7 || mstate == 8)) ||
					    (mode == 2 && (mstate == 4 || mstate == 6 || mstate == 7 || mstate == 8)) ||
					    (mode == 3 && ((u32)(mstate - 3) < 6u))) {
						int id = ctx->id;
						int olen = msg->opt_len;
						void *odata = msg->opt_data;
						if (olen <= 0)
							odata = NULL;
						sceKernelUnlockLwMutex(&g_lock, 1);
						call_handler(ctx, id, ev, msg->mac, olen, odata);
						sceKernelLockLwMutex(&g_lock, 1, NULL);
						peer = sub_418c(ctx, msg->mac);
						if (peer != NULL) {
							int m = ctx->mode;
							if (m == 2) {
								int pst = msg->state;
								if (pst == 6 || pst == 7 || pst == 8) {
									sub_4294(ctx);
								} else if (pst == 4) {
									sub_4238(ctx, peer);
								}
							} else if (m < 2) {
								if (m == 1) {
									int pst = msg->state;
									if (pst == 6 || pst == 7 || pst == 8) {
										sub_4238(ctx, peer);
										int count = sub_4404(ctx, 7);
										if (count > 0) {
											u8 *pkt = ctx->hello_pkt;
											int pkt_len = (count + 1) * 6 + 4;
											ctx->hello_pkt_len = pkt_len;
											sceKernelMemset(pkt, 0, pkt_len);
											int actual = count;
											sub_4354(ctx, 7, pkt + 10, &actual);
											pkt[0] = 8;
											int total = actual + 1;
											pkt[2] = (u8)((u32)total >> 8);
											pkt[3] = (u8)total;
											sceKernelMemcpy(pkt + 4, ctx->local_mac, 6);
											for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
												if (it->state >= 7 && sceNet_lib_E0A81C7C(it->mac, msg->mac) != 0)
													sub_2ac4(ctx, 8, it->mac, 0, NULL);
											}
										} else {
											sub_3154(ctx);
										}
									} else if (pst == 3 || pst == 5) {
										sub_4238(ctx, peer);
									}
								}
							} else if (m == 3) {
								int pst = msg->state;
								if (pst == 6 || pst == 7 || pst == 8) {
									sub_4294(ctx);
								} else if (pst >= 3 && pst <= 5) {
									sub_4238(ctx, peer);
								}
							}
						}
					}
				}
				break;

			case 9: /* ERROR */
				if ((mode == 1 && (st == 7 || st == 8)) ||
				    (mode == 2 && (st == 7 || st == 8)) ||
				    (mode == 3 && (st == 7 || st == 8))) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, ev, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
					peer = sub_418c(ctx, msg->mac);
					if (peer != NULL)
						sub_4294(ctx);
				}
				break;

			case 10: /* DATA */
			case 11: /* DATA_CONFIRM */
				if ((mode == 1 && (st == 7 || st == 8)) ||
				    (mode == 2 && (st == 7 || st == 8)) ||
				    (mode == 3 && (st == 7 || st == 8))) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, ev, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
				}
				break;

			case 12: /* DATA_TIMEOUT */
				if ((mode == 1 && st == 7) ||
				    (mode == 2 && st == 7) ||
				    (mode == 3 && st == 7)) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, ev, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
					peer = sub_418c(ctx, msg->mac);
					if (peer != NULL)
						sub_2ddc(ctx, peer);
				}
				break;

			case 101: /* Established internal notification (0x65) */
				if (mode == 2 && st == 7) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, 7, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
				}
				break;

			case 102: /* Left internal notification (0x66) */
				if (mode == 2 && (st == 7 || st == 8)) {
					int id = ctx->id;
					int olen = msg->opt_len;
					void *odata = msg->opt_data;
					if (olen <= 0)
						odata = NULL;
					sceKernelUnlockLwMutex(&g_lock, 1);
					call_handler(ctx, id, 3, msg->mac, olen, odata);
					sceKernelLockLwMutex(&g_lock, 1, NULL);
					peer = sub_418c(ctx, msg->mac);
					if (peer != NULL && (peer->state == 7 || peer->state == 8))
						sub_4238(ctx, peer);
				}
				break;

			default:
				break;
			}
		}

		if (msg->opt_data != NULL)
			sceKernelFreeVpl(g_vpl_id, msg->opt_data);
		sceKernelFreeVpl(g_vpl_id, msg);
	}

	sceKernelUnlockLwMutex(&g_lock, 1);
	return 0;
}

/* 0x2890: sub_2890 */
static MatchingCtx *sub_2890(int matchingId)
{
	for (MatchingCtx *ctx = (MatchingCtx *)g_ctx_list; ctx != NULL; ctx = ctx->next) {
		if (ctx->id == matchingId)
			return ctx;
	}
	return NULL;
}

/* 0x28c8: sub_28c8 */
static int sub_28c8(void)
{
	MatchingCtx *head = (MatchingCtx *)g_ctx_list;
	int id = 1;
	do {
		MatchingCtx *it = head;
		int used = 0;
		while (it != NULL) {
			if (it->id == id) {
				used = 1;
				break;
			}
			it = it->next;
		}
		if (!used)
			return id;
		id++;
	} while (id < 0x7fff);
	return 0;
}

/* 0x293c: sub_293c */
static MatchingCtx *sub_293c(u16 port)
{
	for (MatchingCtx *ctx = (MatchingCtx *)g_ctx_list; ctx != NULL; ctx = ctx->next) {
		if (ctx->port == port)
			return ctx;
	}
	return NULL;
}

/* 0x2974: sub_2974 */
static void sub_2974(MatchingCtx *ctx)
{
	MatchingCtx *cur = (MatchingCtx *)g_ctx_list;
	if (cur == NULL)
		return;
	if (cur == ctx) {
		g_ctx_list = cur->next;
		return;
	}
	MatchingCtx *prev = cur;
	for (cur = cur->next; cur != NULL; prev = cur, cur = cur->next) {
		if (cur == ctx) {
			prev->next = cur->next;
			return;
		}
	}
}

/* 0x29d0: sub_29d0 */
static int sub_29d0(MatchingCtx *ctx, int event, const u8 *mac, int optLen, const void *optData)
{
	EventMsg *msg = (EventMsg *)sub_1678(0x24);
	if (msg == NULL)
		return 0x8041080c;
	sceKernelMemset(msg, 0, 0x24);

	if (optLen > 0) {
		void *buf = sub_1678(optLen);
		msg->opt_data = buf;
		if (buf == NULL) {
			sceKernelFreeVpl(g_vpl_id, msg);
			return 0x8041080c;
		}
		sceKernelMemcpy(buf, optData, optLen);
	} else {
		msg->opt_data = NULL;
	}

	msg->event = event;
	sceKernelMemcpy(msg->mac, mac, 6);
	msg->opt_len = optLen;
	msg->next = NULL;

	PeerNode *peer = sub_418c(ctx, mac);
	if (peer != NULL)
		msg->state = peer->state;

	sceKernelSendMbx(ctx->event_mbx, msg);
	return 0;
}

/* 0x2ac4: sub_2ac4 */
static int sub_2ac4(MatchingCtx *ctx, int pktType, const u8 *mac, int optLen, const void *optData)
{
	SendMsg *msg = (SendMsg *)sub_1678(0x20);
	if (msg == NULL)
		return 0x8041080c;
	sceKernelMemset(msg, 0, 0x20);

	int pkt_len = optLen + 4;
	u8 *pkt = (u8 *)sub_1678(pkt_len);
	msg->pkt_data = pkt;
	if (pkt == NULL) {
		sceKernelFreeVpl(g_vpl_id, msg);
		return 0x8041080c;
	}
	sceKernelMemset(pkt, 0, pkt_len);
	pkt[0] = (u8)pktType;
	pkt[2] = (u8)((u32)optLen >> 8);
	pkt[3] = (u8)optLen;
	if (optLen > 0)
		sceKernelMemcpy(pkt + 4, optData, optLen);

	msg->pkt_type = pktType;
	sceKernelMemcpy(msg->mac, mac, 6);
	msg->pkt_len = pkt_len;
	msg->next = NULL;
	sceKernelSendMbx(ctx->send_mbx, msg);
	return 0;
}

/* 0x2ba8: sub_2ba8 */
static int sub_2ba8(MatchingCtx *ctx, const u8 *mac, int dataLen, const void *data)
{
	SendMsg *msg = (SendMsg *)sub_1678(0x20);
	if (msg == NULL)
		return 0x8041080c;
	sceKernelMemset(msg, 0, 0x20);

	int pkt_len = dataLen + 11;
	u8 *pkt = (u8 *)sub_1678(pkt_len);
	msg->pkt_data = pkt;
	if (pkt == NULL) {
		sceKernelFreeVpl(g_vpl_id, msg);
		return 0x8041080c;
	}
	sceKernelMemset(pkt, 0, pkt_len);
	pkt[0] = 10;
	pkt[1] = (u8)((u32)dataLen >> 16);
	pkt[2] = (u8)((u32)dataLen >> 8);
	pkt[3] = (u8)dataLen;
	if (ctx->mode == 2 && sub_41ec(ctx, 1) != NULL) {
		sceKernelMemcpy(pkt + 4, mac, 6);
		pkt[10] = 1;
	} else {
		sceKernelMemset(pkt + 4, 0, 7);
	}
	sceKernelMemcpy(pkt + 11, data, dataLen);

	msg->pkt_type = 10;
	if (ctx->mode == 2) {
		PeerNode *parent = sub_41ec(ctx, 1);
		if (parent != NULL)
			mac = parent->mac;
	}
	sceKernelMemcpy(msg->mac, mac, 6);
	msg->pkt_len = pkt_len;
	msg->next = NULL;
	sceKernelSendMbx(ctx->send_mbx, msg);
	return 0;
}

/* 0x2d18: sub_2d18 */
static void sub_2d18(MatchingCtx *ctx)
{
	u32 timeout;
	EventMsg *ev_msg;
	SendMsg *snd_msg;

	for (;;) {
		ev_msg = NULL;
		timeout = 0;
		if (sceKernelReceiveMbx(ctx->event_mbx, (void **)&ev_msg, &timeout) != 0)
			break;
		if (ev_msg != NULL) {
			if (ev_msg->opt_data != NULL)
				sceKernelFreeVpl(g_vpl_id, ev_msg->opt_data);
			sceKernelFreeVpl(g_vpl_id, ev_msg);
		}
	}

	for (;;) {
		snd_msg = NULL;
		timeout = 0;
		if (sceKernelReceiveMbx(ctx->send_mbx, (void **)&snd_msg, &timeout) != 0)
			break;
		if (snd_msg != NULL) {
			if (snd_msg->pkt_data != NULL)
				sceKernelFreeVpl(g_vpl_id, snd_msg->pkt_data);
			sceKernelFreeVpl(g_vpl_id, snd_msg);
		}
	}
}

/* 0x2ddc: sub_2ddc */
static void sub_2ddc(MatchingCtx *ctx, PeerNode *peer)
{
	u32 timeout;
	SendMsg *msg;
	SendMsg *keep_head = NULL;
	SendMsg *keep_tail = NULL;

	for (;;) {
		msg = NULL;
		timeout = 0;
		if (sceKernelReceiveMbx(ctx->send_mbx, (void **)&msg, &timeout) != 0)
			break;
		if (msg != NULL) {
			if (msg->pkt_type == 10 && sceNet_lib_E0A81C7C(msg->mac, peer->mac) == 0) {
				if (msg->pkt_data != NULL)
					sceKernelFreeVpl(g_vpl_id, msg->pkt_data);
				sceKernelFreeVpl(g_vpl_id, msg);
			} else {
				if (keep_head == NULL)
					keep_head = msg;
				else
					keep_tail->next = msg;
				keep_tail = msg;
			}
		}
	}

	for (msg = keep_head; msg != NULL; msg = keep_tail) {
		keep_tail = msg->next;
		sceKernelSendMbx(ctx->send_mbx, msg);
	}
}

/* 0x2e98: sub_2e98 */
static int sub_2e98(MatchingCtx *ctx, PeerNode *peer)
{
	u32 timeout;
	SendMsg *msg;
	SendMsg *keep_head = NULL;
	SendMsg *keep_tail = NULL;
	int count = 0;

	for (;;) {
		msg = NULL;
		timeout = 0;
		if (sceKernelReceiveMbx(ctx->send_mbx, (void **)&msg, &timeout) != 0)
			break;
		if (msg != NULL) {
			if (msg->pkt_type == 10 && sceNet_lib_E0A81C7C(msg->mac, peer->mac) == 0)
				count++;
			if (keep_head == NULL)
				keep_head = msg;
			else
				keep_tail->next = msg;
			keep_tail = msg;
		}
	}

	for (msg = keep_head; msg != NULL; msg = keep_tail) {
		keep_tail = msg->next;
		sceKernelSendMbx(ctx->send_mbx, msg);
	}
	return count;
}

/* 0x2f08: sub_2f08 */
static u32 sub_2f08(void *common)
{
	MatchingCtx *ctx = (MatchingCtx *)common;
	static const u8 bcast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (ctx->mode == 1) {
		if (sub_4404(ctx, 7) < ctx->max_peers - 1)
			sub_2ac4(ctx, 1, bcast_mac, ctx->hello_opt_len, ctx->hello_opt_data);
	} else if (sub_4404(ctx, 4) == 0) {
		sub_2ac4(ctx, 1, bcast_mac, ctx->hello_opt_len, ctx->hello_opt_data);
	}
	return ctx->hello_delay;
}

/* 0x2f68: sub_2f68 */
static u32 sub_2f68(void *common)
{
	MatchingCtx *ctx = (MatchingCtx *)common;
	static const u8 bcast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
		int st = it->state;
		if ((u32)(st - 3) < 6u) {
			sub_2ac4(ctx, 9, bcast_mac, 0, NULL);
			return ctx->ping_delay;
		}
	}
	return ctx->ping_delay;
}

/* 0x2fc8: sub_2fc8 */
static void sub_2fc8(MatchingCtx *ctx, u32 now)
{
	PeerNode *next;
	for (PeerNode *peer = ctx->peer_list; peer != NULL; peer = next) {
		next = peer->next;
		u32 timeout = peer->timeout_us;
		if (timeout != 0 && timeout < now - peer->last_recv_time) {
			int mode = ctx->mode;
			int st = peer->state;
			if ((mode == 1 && (st == 1 || st == 2)) ||
			    (mode == 2 && st == 1) ||
			    (mode == 3 && st == 1)) {
				sub_4238(ctx, peer);
			} else if (st == 8) {
				if (peer->bulk_len > 0) {
					peer->bulk_len = 0;
					sceKernelFreeVpl(g_vpl_id, peer->bulk_data);
					peer->bulk_data = NULL;
				}
				peer->state = 7;
				peer->timeout_us = ctx->keepalive_timeout;
				sub_29d0(ctx, 12, peer->mac, 0, NULL);
			} else {
				sub_29d0(ctx, 8, peer->mac, 0, NULL);
				peer->timeout_us = 0;
			}
		}

		if (peer != NULL && (peer->state == 4 || peer->state == 5 || peer->state == 6 || peer->state == 8)) {
			if (now - peer->last_keepalive_time > ctx->ping_delay) {
				int count = peer->keepalive_count + 1;
				peer->keepalive_count = count;
				if (count < ctx->init_count && now - peer->last_send_time > ctx->ping_delay) {
					int st = peer->state;
					if (st == 5) {
						sub_2ac4(ctx, 3, peer->mac, ctx->join_opt_len, ctx->join_opt_data);
					} else if (st < 5) {
						if (st == 4)
							sub_2ac4(ctx, 2, peer->mac, peer->opt_len, peer->opt_data);
					} else if (st == 6) {
						sub_2ac4(ctx, 4, peer->mac, ctx->hello_pkt_len, ctx->hello_pkt);
					} else if (st == 8) {
						if (sub_2e98(ctx, peer) == 0)
							sub_2ba8(ctx, peer->mac, peer->bulk_len, peer->bulk_data);
					}
					peer->last_keepalive_time = now;
				}
			}
		}
	}
}

/* 0x3154: sub_3154 */
static int sub_3154(MatchingCtx *ctx)
{
	if (ctx->hello_opt_len > 0) {
		ctx->hello_opt_len = 0;
		sceKernelFreeVpl(g_vpl_id, ctx->hello_opt_data);
		ctx->hello_opt_data = NULL;
	}
	if (ctx->join_opt_len > 0) {
		ctx->join_opt_len = 0;
		sceKernelFreeVpl(g_vpl_id, ctx->join_opt_data);
		ctx->join_opt_data = NULL;
	}
	int max_pkt_len = ctx->max_peers * 6 + 4;
	if (ctx->hello_pkt == NULL) {
		u8 *buf = (u8 *)sub_1678(max_pkt_len);
		ctx->hello_pkt = buf;
		if (buf == NULL)
			return 0x8041080c;
	}
	sceKernelMemset(ctx->hello_pkt, 0, max_pkt_len);
	ctx->hello_pkt_len = 0;
	return 0;
}

/* 0x3208: sub_3208 */
static void sub_3208(MatchingCtx *ctx)
{
	u32 timeout;
	SendMsg *msg;
	u8 bcast_mac[8];

	bcast_mac[0] = 0xff;
	bcast_mac[1] = 0xff;
	bcast_mac[2] = 0xff;
	bcast_mac[3] = 0xff;
	bcast_mac[4] = 0xff;
	bcast_mac[5] = 0xff;

	for (;;) {
		msg = NULL;
		timeout = 0;
		if (sceKernelReceiveMbx(ctx->send_mbx, (void **)&msg, &timeout) != 0)
			break;
		if (msg != NULL) {
			if (ctx->stop_input_thread) {
				if (msg->pkt_data != NULL)
					sceKernelFreeVpl(g_vpl_id, msg->pkt_data);
				sceKernelFreeVpl(g_vpl_id, msg);
				break;
			}
			sceNetAdhocPdpSend(ctx->pdp_id, msg->mac, ctx->port, msg->pkt_data, msg->pkt_len, 0, 1);
			u32 now = sceKernelGetSystemTimeLow();
			if (sceNet_lib_E0A81C7C(msg->mac, bcast_mac) == 0) {
				for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next)
					it->last_send_time = now;
			} else {
				PeerNode *peer = sub_418c(ctx, msg->mac);
				if (peer != NULL)
					peer->last_send_time = now;
			}
			if (msg->pkt_data != NULL)
				sceKernelFreeVpl(g_vpl_id, msg->pkt_data);
			sceKernelFreeVpl(g_vpl_id, msg);
		}
	}
}

/* 0x330c: sub_330c ("SceNetAdhocMatchingInput" thread) */
static int sub_330c(u32 args, void *argp)
{
	u8 src_mac[8];
	u16 src_port;
	u32 rx_len;

	sceKernelLockLwMutex(&g_lock, 1, NULL);
	MatchingCtx *ctx = sub_2890(*(int *)argp);
	if (ctx == NULL) {
		sceKernelUnlockLwMutex(&g_lock, 1);
		return 0;
	}
	sceNet_lib_1858883D(0);

	if (ctx->mode != 2)
		ctx->alarm1_id = sceKernelSetAlarm(ctx->hello_delay, sub_2f08, ctx);
	ctx->alarm2_id = sceKernelSetAlarm(ctx->ping_delay, sub_2f68, ctx);
	sceKernelUnlockLwMutex(&g_lock, 1);

	while (!ctx->stop_input_thread) {
		rx_len = ctx->rx_buflen * 2;
		int ret = sceNetAdhocPdpRecv(ctx->pdp_id, src_mac, &src_port, ctx->rx_buf, &rx_len, 100000, 0);
		sceKernelLockLwMutex(&g_lock, 1, NULL);
		u32 now = sceKernelGetSystemTimeLow();

		if (ret == 0 && rx_len > 0 && src_port == ctx->port) {
			u8 *buf = (u8 *)ctx->rx_buf;
			u8 pkt_type = buf[0];
			PeerNode *peer = sub_418c(ctx, src_mac);
			int mode = ctx->mode;

			switch (pkt_type) {
			case 1: /* HELLO */
				if ((mode == 2 && sub_4404(ctx, 4) == 0) ||
				    (mode == 3 && sub_4404(ctx, 7) < ctx->max_peers - 1 && sub_4404(ctx, 4) == 0)) {
					int olen = ((u32)buf[2] << 8) | buf[3];
					if ((u32)(olen + 4) <= rx_len) {
						u8 *odata = (olen > 0) ? (buf + 4) : NULL;
						peer = sub_3d64(ctx, src_mac, olen, odata);
					}
				}
				break;

			case 2: /* JOIN */
				if ((mode == 1 || mode == 3) && sub_4404(ctx, 7) < ctx->max_peers - 1) {
					int olen = ((u32)buf[2] << 8) | buf[3];
					if ((u32)(olen + 4) <= rx_len) {
						u8 *odata = (olen > 0) ? (buf + 4) : NULL;
						if (ctx->mode == 1) {
							if (peer == NULL) {
								peer = sub_40b8(ctx, src_mac, olen, odata);
								if (peer != NULL) {
									peer->state = 3;
									peer->timeout_us = ctx->timeout_total;
									sub_29d0(ctx, 2, src_mac, olen, odata);
								}
							} else if (peer->state == 3) {
								peer->timeout_us = ctx->timeout_total;
							}
						} else if (ctx->mode == 3) {
							if (peer == NULL) {
								peer = sub_40b8(ctx, src_mac, olen, odata);
								if (peer == NULL)
									break;
							}
							int st = peer->state;
							if (st == 2 || st == 1) {
								peer->state = 3;
								peer->timeout_us = ctx->timeout_total;
								sub_29d0(ctx, 2, src_mac, olen, odata);
							} else if (st == 4) {
								if (sub_4504(peer) >= 0) {
									peer->state = 3;
									peer->timeout_us = ctx->timeout_total;
									sub_29d0(ctx, 2, src_mac, olen, odata);
								}
							} else if (st == 3) {
								peer->timeout_us = ctx->timeout_total;
							}
						}
					}
				}
				break;

			case 3: /* ACCEPT */
				if (peer != NULL &&
				    ((mode == 2 && peer->state == 4 && sub_41ec(ctx, 1) == NULL) ||
				     (mode == 3 && peer->state == 4))) {
					int olen = ((u32)buf[2] << 8) | buf[3];
					if ((u32)(olen + 4) <= rx_len) {
						u8 *odata = (olen > 0) ? (buf + 4) : NULL;
						if (peer->opt_len > 0) {
							peer->opt_len = 0;
							sceKernelFreeVpl(g_vpl_id, peer->opt_data);
							peer->opt_data = NULL;
						}
						int count = sub_4404(ctx, 7);
						u8 *pkt = ctx->hello_pkt;
						int pkt_len = (count + 2) * 6 + 4;
						ctx->hello_pkt_len = pkt_len;
						sceKernelMemset(pkt, 0, pkt_len);
						int actual = count + 1;
						sub_4354(ctx, 7, pkt + 10, &actual);
						pkt[0] = 4;
						int total = actual + 2;
						pkt[2] = (u8)((u32)total >> 8);
						pkt[3] = (u8)total;
						sceKernelMemcpy(pkt + 4, ctx->local_mac, 6);
						sceKernelMemcpy(pkt + 4 + (actual + 1) * 6, src_mac, 6);
						if (sub_2ac4(ctx, 4, src_mac, ctx->hello_pkt_len, ctx->hello_pkt) == 0) {
							peer->timeout_us = ctx->rexmt_int;
							peer->state = 6;
							sub_29d0(ctx, 6, src_mac, olen, odata);
						} else {
							sub_3154(ctx);
						}
					}
				}
				break;

			case 4: /* COMPLETE */
				if (peer != NULL &&
				    ((mode == 1 && (peer->state == 5 || peer->state == 7)) ||
				     (mode == 3 && (peer->state == 5 || peer->state == 7)))) {
					int mcount = (((u32)buf[2] << 8) | buf[3]) / 6;
					if (mcount > 1 && (u32)(mcount * 6 + 8) <= rx_len) {
						if (ctx->join_opt_len != 0) {
							ctx->join_opt_len = 0;
							sceKernelFreeVpl(g_vpl_id, ctx->join_opt_data);
							ctx->join_opt_data = NULL;
						}
						if (sub_2ac4(ctx, 4, src_mac, 0, NULL) == 0) {
							if (peer->state == 5) {
								if (ctx->mode == 1) {
									u8 *hbuf = (u8 *)sub_1678(rx_len - 4);
									peer->hello_buf = hbuf;
									if (hbuf == NULL)
										break;
									peer->member_count = mcount;
									peer->member_macs = hbuf + 4;
									sceKernelMemcpy(hbuf, buf + 4, rx_len - 4);
								}
								peer->state = 7;
								peer->timeout_us = ctx->keepalive_timeout;
								sub_29d0(ctx, 6, src_mac, ctx->join_opt_len, ctx->join_opt_data);
								sub_29d0(ctx, 7, src_mac, 0, NULL);
							}
						}
					} else if (peer->state == 6) {
						peer->state = 7;
						peer->timeout_us = ctx->keepalive_timeout;
						sub_29d0(ctx, 7, src_mac, 0, NULL);
					}
				} else if (peer != NULL && peer->state == 6 &&
				           (mode == 1 || mode == 2 || mode == 3)) {
					peer->state = 7;
					peer->timeout_us = ctx->keepalive_timeout;
					sub_29d0(ctx, 7, src_mac, 0, NULL);
				}
				break;

			case 5: /* REJECT */
				if (peer != NULL &&
				    ((mode == 2 && peer->state == 4 && sub_41ec(ctx, 1) == NULL) ||
				     (mode == 3 && peer->state == 4))) {
					int olen = ((u32)buf[2] << 8) | buf[3];
					if ((u32)(olen + 4) <= rx_len) {
						u8 *odata = (olen > 0) ? (buf + 4) : NULL;
						if (peer->opt_len > 0) {
							peer->opt_len = 0;
							sceKernelFreeVpl(g_vpl_id, peer->opt_data);
							peer->opt_data = NULL;
						}
						peer->timeout_us = ctx->timeout_total;
						peer->state = 1;
						sub_29d0(ctx, 4, src_mac, olen, odata);
					}
				}
				break;

			case 6: /* CANCEL / LEAVE */
				if (peer != NULL &&
				    ((mode == 1 && (peer->state == 3 || peer->state == 6 || peer->state == 7 || peer->state == 8)) ||
				     (mode == 2 && (peer->state == 4 || peer->state == 6 || peer->state == 7 || peer->state == 8)) ||
				     (mode == 3 && ((u32)(peer->state - 3) < 6u)))) {
					int olen = ((u32)buf[2] << 8) | buf[3];
					if ((u32)(olen + 4) <= rx_len) {
						u8 *odata = (olen > 0) ? (buf + 4) : NULL;
						sub_3de0(ctx, peer);
						int pst = peer->state;
						if (pst == 3) {
							peer->timeout_us = ctx->timeout_total;
							int next_st = 2;
							if (ctx->mode != 1)
								next_st = (ctx->mode == 3) ? 1 : 0;
							peer->state = next_st;
							sub_29d0(ctx, 5, src_mac, olen, odata);
						} else if (pst == 7 || pst == 8) {
							sub_29d0(ctx, 3, src_mac, olen, odata);
						} else {
							sub_4238(ctx, peer);
						}
					}
				}
				break;

			case 7: /* BIRTH (new member joined parent) */
				if (mode == 2 && peer != NULL && (peer->state == 7 || peer->state == 8) &&
				    peer == sub_41ec(ctx, 1) && rx_len >= 10) {
					u8 *new_mac = buf + 4;
					if (sceNet_lib_E0A81C7C(new_mac, ctx->local_mac) != 0) {
						PeerNode *m = sub_418c(ctx, new_mac);
						if (m == NULL)
							m = sub_40b8(ctx, new_mac, 0, NULL);
						if (m != NULL) {
							m->state = 7;
							m->timeout_us = 0;
							int new_len = ctx->hello_pkt_len + 6;
							ctx->hello_pkt_len = new_len;
							u8 *pkt = ctx->hello_pkt;
							int count = (new_len - 4) / 6;
							pkt[2] = (u8)((u32)count >> 8);
							pkt[3] = (u8)count;
							sceKernelMemcpy(pkt + new_len - 6, new_mac, 6);
							sub_29d0(ctx, 101, new_mac, 0, NULL);
						} else {
							sub_29d0(ctx, 9, peer->mac, 0, NULL);
						}
					}
				}
				break;

			case 8: /* DEATH (member left parent) */
				if (mode == 2 && peer != NULL && (peer->state == 7 || peer->state == 8) &&
				    peer == sub_41ec(ctx, 1)) {
					int mcount = (((u32)buf[2] << 8) | buf[3]) / 6;
					if (mcount > 0 && (u32)(mcount * 6 + 8) <= rx_len) {
						PeerNode *next_it;
						for (PeerNode *it = ctx->peer_list; it != NULL; it = next_it) {
							next_it = it->next;
							u8 *pmac = buf + 8;
							int found = 0;
							for (int i = 0; i < mcount; i++, pmac += 6) {
								if (sceNet_lib_E0A81C7C(it->mac, pmac) == 0) {
									found = 1;
									break;
								}
							}
							if (!found && (it->state == 7 || it->state == 8)) {
								sub_3de0(ctx, it);
								sub_29d0(ctx, 102, it->mac, 0, NULL);
							}
						}
						int pkt_len = mcount * 6 + 4;
						ctx->hello_pkt_len = pkt_len;
						u8 *pkt = ctx->hello_pkt;
						sceKernelMemset(pkt, 0, pkt_len);
						pkt[0] = 8;
						pkt[2] = (u8)((u32)mcount >> 8);
						pkt[3] = (u8)mcount;
						sceKernelMemcpy(pkt + 4, buf + 8, mcount * 6);
					}
				}
				break;

			case 9: /* KEEPALIVE */
				break;

			case 10: /* DATA */
				if (peer != NULL && (peer->state == 7 || peer->state == 8))
					sub_3efc(ctx, peer, buf, rx_len);
				break;

			case 11: /* DATA ACK */
				if (peer != NULL)
					sub_4018(ctx, peer, rx_len);
				break;

			default:
				break;
			}

			if (peer != NULL) {
				peer->last_recv_time = now;
				peer->keepalive_count = 0;
			}
		}

		sub_2fc8(ctx, now);
		sub_3208(ctx);
		sceKernelUnlockLwMutex(&g_lock, 1);
	}

	return 0;
}

/* 0x3d64: sub_3d64 */
static PeerNode *sub_3d64(MatchingCtx *ctx, const u8 *mac, int optLen, const void *optData)
{
	PeerNode *peer = sub_418c(ctx, mac);
	if (peer == NULL) {
		if (sub_4404(ctx, 1) >= ctx->max_peers - 1)
			return NULL;
		peer = sub_40b8(ctx, mac, optLen, optData);
		if (peer == NULL)
			return NULL;
		peer->state = 1;
	} else if (peer->state != 1) {
		return peer;
	}
	peer->timeout_us = ctx->timeout_total;
	sub_29d0(ctx, 1, mac, optLen, optData);
	return peer;
}

/* 0x3de0: sub_3de0 */
static void sub_3de0(MatchingCtx *ctx, PeerNode *peer)
{
	u32 timeout;
	EventMsg *msg;
	EventMsg *keep_head = NULL;
	EventMsg *keep_tail = NULL;

	if (peer->opt_len > 0) {
		peer->opt_len = 0;
		sceKernelFreeVpl(g_vpl_id, peer->opt_data);
		peer->opt_data = NULL;
	}
	if (peer->bulk_len > 0) {
		peer->bulk_len = 0;
		sceKernelFreeVpl(g_vpl_id, peer->bulk_data);
		peer->bulk_data = NULL;
	}
	sub_2ddc(ctx, peer);

	for (;;) {
		msg = NULL;
		timeout = 0;
		if (sceKernelReceiveMbx(ctx->event_mbx, (void **)&msg, &timeout) != 0)
			break;
		if (msg != NULL) {
			if (sceNet_lib_E0A81C7C(msg->mac, peer->mac) == 0) {
				if (msg->opt_data != NULL)
					sceKernelFreeVpl(g_vpl_id, msg->opt_data);
				sceKernelFreeVpl(g_vpl_id, msg);
			} else {
				if (keep_head == NULL)
					keep_head = msg;
				else
					keep_tail->next = msg;
				keep_tail = msg;
			}
		}
	}

	for (msg = keep_head; msg != NULL; msg = keep_tail) {
		keep_tail = msg->next;
		sceKernelSendMbx(ctx->event_mbx, msg);
	}
}

/* 0x3efc: sub_3efc */
static int sub_3efc(MatchingCtx *ctx, PeerNode *peer, const u8 *rxBuf, u32 rxLen)
{
	if (rxLen < 12)
		return -1;
	int dlen = ((u32)rxBuf[1] << 16) | ((u32)rxBuf[2] << 8) | rxBuf[3];
	if (dlen <= 0 || (u32)(dlen + 11) > rxLen)
		return -1;

	if (ctx->mode == 1 && rxBuf[10] != 0) {
		const u8 *dest_mac = rxBuf + 4;
		PeerNode *dest_peer = sub_418c(ctx, dest_mac);
		if (dest_peer == NULL)
			return -1;
		SendMsg *msg = (SendMsg *)sub_1678(0x20);
		if (msg == NULL)
			return -1;
		sceKernelMemset(msg, 0, 0x20);
		u8 *pkt = (u8 *)sub_1678(rxLen);
		msg->pkt_data = pkt;
		if (pkt == NULL) {
			sceKernelFreeVpl(g_vpl_id, msg);
			return -1;
		}
		sceKernelMemcpy(pkt, rxBuf, rxLen);
		sceKernelMemcpy(pkt + 4, peer->mac, 6);
		pkt[10] = 0;
		msg->pkt_type = 10;
		sceKernelMemcpy(msg->mac, dest_mac, 6);
		msg->pkt_len = rxLen;
		msg->next = NULL;
		sceKernelSendMbx(ctx->send_mbx, msg);
		return 0;
	}

	const u8 *ack_mac = peer->mac;
	u8 ack_opt[7];
	int ack_opt_len = 0;
	u8 *ack_opt_ptr = NULL;

	if (ctx->mode == 2 && peer == sub_41ec(ctx, 1)) {
		ack_mac = rxBuf + 4;
		ack_opt_ptr = ack_opt;
		sceKernelMemcpy(ack_opt, ack_mac, 6);
		ack_opt[6] = 1;
		ack_opt_len = 7;
	}

	if (sub_2ac4(ctx, 11, peer->mac, ack_opt_len, ack_opt_ptr) != 0)
		return -1;
	sub_29d0(ctx, 10, ack_mac, dlen, rxBuf + 11);
	return 0;
}

/* 0x4018: sub_4018 */
static void sub_4018(MatchingCtx *ctx, PeerNode *peer, u32 rxLen)
{
	if (ctx->mode == 1 && rxLen >= 11) {
		u8 *rx = (u8 *)ctx->rx_buf;
		if (rx[10] != 0) {
			sub_2ac4(ctx, 11, rx + 4, 7, peer->mac);
			return;
		}
	}
	if (ctx->mode == 2 && rxLen >= 11 && peer == sub_41ec(ctx, 1)) {
		u8 *rx = (u8 *)ctx->rx_buf;
		peer = sub_418c(ctx, rx + 4);
		if (peer == NULL)
			return;
	}
	if (peer->state != 8)
		return;
	if (peer->bulk_len > 0) {
		peer->bulk_len = 0;
		sceKernelFreeVpl(g_vpl_id, peer->bulk_data);
		peer->bulk_data = NULL;
	}
	peer->state = 7;
	peer->timeout_us = ctx->keepalive_timeout;
	sub_29d0(ctx, 11, peer->mac, 0, NULL);
}

/* 0x40b8: sub_40b8 */
static PeerNode *sub_40b8(MatchingCtx *ctx, const u8 *mac, int optLen, const void *optData)
{
	PeerNode *peer = (PeerNode *)sub_1678(0x88);
	if (peer == NULL)
		return NULL;
	sceKernelMemset(peer, 0, 0x88);

	if (optLen > 0) {
		void *buf = sub_1678(optLen);
		peer->opt_data = buf;
		if (buf == NULL) {
			sceKernelFreeVpl(g_vpl_id, peer);
			return NULL;
		}
		sceKernelMemcpy(buf, optData, optLen);
	}

	peer->state = 2;
	sceKernelMemcpy(peer->mac, mac, 6);
	peer->opt_len = optLen;
	u32 now = sceKernelGetSystemTimeLow();
	peer->last_recv_time = now;
	peer->timeout_us = 0;
	peer->last_send_time = now;
	peer->next = NULL;

	PeerNode *head = ctx->peer_list;
	if (head == NULL) {
		ctx->peer_list = peer;
	} else {
		while (head->next != NULL)
			head = head->next;
		head->next = peer;
	}
	return peer;
}

/* 0x418c: sub_418c */
static PeerNode *sub_418c(MatchingCtx *ctx, const u8 *mac)
{
	for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
		if (sceNet_lib_E0A81C7C(it->mac, mac) == 0)
			return it;
	}
	return NULL;
}

/* 0x41ec: sub_41ec */
static PeerNode *sub_41ec(MatchingCtx *ctx, int state)
{
	if (ctx->mode == 2 && state == 1) {
		u8 *parent_mac = ctx->hello_pkt + 4;
		for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
			if (sceNet_lib_E0A81C7C(it->mac, parent_mac) == 0)
				return it;
		}
		return NULL;
	}
	for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
		if (it->state == state)
			return it;
	}
	return NULL;
}

/* 0x4238: sub_4238 */
static int sub_4238(MatchingCtx *ctx, PeerNode *peer)
{
	PeerNode *cur = ctx->peer_list;
	if (cur == NULL)
		return 0;

	if (cur == peer) {
		ctx->peer_list = cur->next;
	} else {
		PeerNode *prev = cur;
		for (cur = cur->next; cur != NULL; prev = cur, cur = cur->next) {
			if (cur == peer) {
				prev->next = cur->next;
				break;
			}
		}
		if (cur == NULL)
			return 0;
	}

	if (peer->opt_len > 0)
		sceKernelFreeVpl(g_vpl_id, peer->opt_data);
	if (peer->bulk_len > 0)
		sceKernelFreeVpl(g_vpl_id, peer->bulk_data);
	if (peer->hello_buf != NULL)
		sceKernelFreeVpl(g_vpl_id, peer->hello_buf);
	return sceKernelFreeVpl(g_vpl_id, peer);
}

/* 0x4294: sub_4294 */
static void sub_4294(MatchingCtx *ctx)
{
	PeerNode *next;
	for (PeerNode *it = ctx->peer_list; it != NULL; it = next) {
		sub_3de0(ctx, it);
		if (it->opt_len > 0)
			sceKernelFreeVpl(g_vpl_id, it->opt_data);
		if (it->bulk_len > 0)
			sceKernelFreeVpl(g_vpl_id, it->bulk_data);
		if (it->hello_buf != NULL)
			sceKernelFreeVpl(g_vpl_id, it->hello_buf);
		next = it->next;
		sceKernelFreeVpl(g_vpl_id, it);
	}
	ctx->peer_list = NULL;
	sub_3154(ctx);
}

/* 0x4354: sub_4354 */
static void sub_4354(MatchingCtx *ctx, int minState, u8 *dstMacs, int *pCount)
{
	int count = 0;
	u8 *dst = dstMacs;
	for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
		if (it->state >= minState) {
			if (dstMacs != NULL) {
				if (count >= *pCount) {
					*pCount = count;
					return;
				}
				sceKernelMemcpy(dst, it->mac, 6);
			}
			dst += 6;
			count++;
		}
	}
	*pCount = count;
}

/* 0x4404: sub_4404 */
static int sub_4404(MatchingCtx *ctx, int minState)
{
	int count = 0;
	for (PeerNode *it = ctx->peer_list; it != NULL; it = it->next) {
		if (it->state >= minState)
			count++;
	}
	return count;
}

/* 0x443c: sub_443c */
static void sub_443c(MatchingCtx *ctx, int *pSize, u8 *buf)
{
	int offset = 0;
	u8 *prev = NULL;
	for (int i = 4; i < ctx->hello_pkt_len; i += 6) {
		u8 *cur = buf + offset;
		int next_offset = offset + 12;
		if (buf != NULL) {
			if (*pSize < next_offset) {
				*pSize = offset;
				return;
			}
			sceKernelMemcpy(cur + 4, ctx->hello_pkt + i, 6);
			*(void **)cur = NULL;
			if (prev != NULL)
				*(void **)prev = cur;
			prev = cur;
		}
		offset = next_offset;
	}
	*pSize = offset;
}

/* 0x4504: sub_4504 */
static int sub_4504(const PeerNode *peer)
{
	u32 local_hi = ((u32)g_local_mac[0] << 16) | ((u32)g_local_mac[1] << 8) | g_local_mac[2];
	u32 peer_hi = ((u32)peer->mac[0] << 16) | ((u32)peer->mac[1] << 8) | peer->mac[2];
	if (local_hi < peer_hi)
		return 1;
	if (peer_hi < local_hi)
		return 0;
	u32 local_lo = ((u32)g_local_mac[3] << 16) | ((u32)g_local_mac[4] << 8) | g_local_mac[5];
	u32 peer_lo = ((u32)peer->mac[3] << 16) | ((u32)peer->mac[4] << 8) | peer->mac[5];
	if (local_lo < peer_lo)
		return 1;
	if (peer_lo < local_lo)
		return 0;
	return -1;
}
