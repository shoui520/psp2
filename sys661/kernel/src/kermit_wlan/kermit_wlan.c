#include <kermit.h>

/*SysclibForKernel imports */
void *memset(void *s, int c, SceSize n);
void *memcpy(void *dst, const void *src, SceSize n);

/* ThreadManForKernel imports */
typedef struct SceKernelMsgPacket {
	struct SceKernelMsgPacket *next;
	u8 msgPriority;
	u8 dummy[3];
} SceKernelMsgPacket;

typedef SceUInt (*SceKernelAlarmHandler)(void *common);

SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *), int initPriority, int stackSize, SceUInt attr, void *option);
int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
int sceKernelWaitThreadEnd(SceUID thid, SceUInt *timeout);
int sceKernelTerminateThread(SceUID thid);
int sceKernelExitDeleteThread(int status);
int sceKernelChangeThreadPriority(SceUID thid, int priority);
SceUID sceKernelCreateMbx(const char *name, SceUInt attr, void *option);
int sceKernelDeleteMbx(SceUID mbxid);
int sceKernelSendMbx(SceUID mbxid, void *message);
int sceKernelReceiveMbx(SceUID mbxid, void **pmessage, SceUInt *timeout);
SceUID sceKernelCreateVpl(const char *name, int part, int attr, unsigned int size, void *opt);
int sceKernelDeleteVpl(SceUID uid);
int sceKernelAllocateVpl(SceUID uid, unsigned int size, void **data, unsigned int *timeout);
int sceKernelTryAllocateVpl(SceUID uid, unsigned int size, void **data);
int sceKernelFreeVpl(SceUID uid, void *data);
SceUID sceKernelSetAlarm(SceUInt clock, SceKernelAlarmHandler handler, void *common);
int sceKernelCancelAlarm(SceUID alarmid);
s64 sceKernelGetSystemTimeWide(void);

/* sceIdStorage_driver imports */
int sceIdStorageLookup(u16 key, u32 offset, void *buf, u32 len);

/* scePower_driver imports */
int scePower_driver_8C6BEFD9(void);

/* sceNetIfhandle_driver packet and interface structures */
typedef struct SceNetMbuf {
	struct SceNetMbuf *next;
	u32 unk_04;
	u8 *data;
	s32 len;
	u16 unk_10;
	u16 flags;
	u32 unk_14;
	u32 total_len;
	u32 unk_1c;
	s16 rssi_a;
	s16 rssi_b;
	u32 unk_24[3];
	void *ext_buf;
	u32 ext_34;
	u32 ext_38;
	u32 ext_size;
	u32 ext_40;
	struct SceNetMbuf *ext_ref1;
	struct SceNetMbuf *ext_ref2;
} SceNetMbuf;

int sceNetIfhandle_driver_16042084(void *ifp);
int sceNetIfhandle_driver_C9344A59(void *ifp);
int sceNetIfhandle_driver_AE81C0CB(void *ifp, void *mac, const char *name);
int sceNetIfhandle_driver_54D1AEA1(void *ifp);
int sceNetIfhandle_driver_F94BAF52(void *ifp, int code, int status);
int sceNetIfhandle_driver_263767F6(void *ifp, int event, int arg);
SceNetMbuf *sceNetIfhandle_driver_E2F4F1C9(void *ifp);
int sceNetIfhandle_driver_C28F6FF2(void *ifp, SceNetMbuf *m);
int sceNetIfhandle_driver_16246B99(void *ifp, SceNetMbuf *m);
int sceNetIfhandle_driver_C6D14282(void *ifp, void *ctx);
void *sceNetIfhandle_driver_D5AD6DEA(void *ifp);
SceNetMbuf *sceNetIfhandle_driver_15CFE3C0(u32 size);
int sceNetIfhandle_driver_F8825DC4(SceNetMbuf *m);
int sceNetIfhandle_driver_F56FAC82(SceNetMbuf *m, int flags);

/* Internal data structures */
typedef struct SceWlanTxNode {
	u32 busy;
	struct SceWlanTxNode *ring_next;
	struct SceWlanTxNode *queue_next;
	u32 len;
	u8 *buf;
} SceWlanTxNode;

typedef struct SceWlanQueue {
	SceWlanTxNode *head;
	SceWlanTxNode *tail;
} SceWlanQueue;

typedef struct SceWlanRssiNode {
	u16 flags;
	s16 rssi;
	struct SceWlanRssiNode *next;
} SceWlanRssiNode;

typedef struct SceWlanMbxMsg {
	SceKernelMsgPacket hdr;
	u32 type;
	u32 unk_0c;
	void *arg_ptr;
	u32 arg_val;
	u32 flags;
	u32 param0;
	u32 param1;
} SceWlanMbxMsg;

typedef struct SceWlanCtx {
	u32 flags;                       /* +0x000 */
	u32 unk_004;                     /* +0x004 */
	u8 conn_info[0x98];              /* +0x008..+0x09f */
	u32 unk_0a0;                     /* +0x0a0 */
	u32 unk_0a4;                     /* +0x0a4 */
	u32 unk_0a8;                     /* +0x0a8 */
	u32 rate_idx;                    /* +0x0ac */
	u16 channel_mask;                /* +0x0b0 */
	u8 unk_0b2;                      /* +0x0b2 */
	u8 rssi_base;                    /* +0x0b3 */
	u32 unk_0b4;                     /* +0x0b4 */
	u8 mac_addr[8];                  /* +0x0b8..+0x0bf */
	u32 power_mode;                  /* +0x0c0 */
	u32 unk_0c4;                     /* +0x0c4 */
	u16 unk_0c8;                     /* +0x0c8 */
	u16 unk_0ca;                     /* +0x0ca */
	u8 unk_0cc;                      /* +0x0cc */
	u8 pad_0cd;                      /* +0x0cd */
	u16 unk_0ce;                     /* +0x0ce */
	u16 unk_0d0;                     /* +0x0d0 */
	u16 unk_0d2;                     /* +0x0d2 */
	u32 unk_0d4[2];                  /* +0x0d4..+0x0db */
	u32 unk_0dc;                     /* +0x0dc */
	u32 unk_0e0;                     /* +0x0e0 */
	u32 unk_0e4[20];                 /* +0x0e4..+0x133 */
	u32 tx_buf_stride;               /* +0x134 */
	u32 tx_node_count;               /* +0x138 */
	SceWlanTxNode *tx_pool;          /* +0x13c */
	SceWlanTxNode *tx_cur;           /* +0x140 */
	u32 unk_144[4];                  /* +0x144..+0x153 */
	SceWlanRssiNode *rssi_pool;      /* +0x154 */
	SceWlanRssiNode *rssi_head;      /* +0x158 */
	u16 unk_15c;                     /* +0x15c */
	u16 unk_15e;                     /* +0x15e */
	s32 thread_running;              /* +0x160 */
	u32 unk_164;                     /* +0x164 */
	SceUID mac_thid;                 /* +0x168 */
	SceUID worker_thid;              /* +0x16c */
	SceUID lock_sema;                /* +0x170 */
	SceUID mbx_id;                   /* +0x174 */
	u8 country_str[4];               /* +0x178..+0x17b */
	SceUID vpl_id;                   /* +0x17c */
	u32 unk_180[2];                  /* +0x180..+0x187 */
	struct {
		void *if_internal;           /* +0x188 */
		u32 unk_18c;                 /* +0x18c */
		void (*cb_up)(void *);       /* +0x190 */
		void (*cb_down)(void *);     /* +0x194 */
		void (*cb_tx)(void *);       /* +0x198 */
		void (*cb_ioctl)(void *, u32, void *, u32 *); /* +0x19c */
		u32 reserved[5];             /* +0x1a0..+0x1b3 */
	} netif;
	SceWlanQueue tx_queue;           /* +0x1b4..+0x1bb */
	SceWlanQueue rx_queue;           /* +0x1bc..+0x1c3 */
	u8 *rx_buf;                      /* +0x1c4 */
	SceUID alarm_id;                 /* +0x1c8 */
	u32 unk_1cc;                     /* +0x1cc */
	u32 unk_1d0;                     /* +0x1d0 */
	u32 version_or_mode;             /* +0x1d4 */
	u16 idstorage_45_0;              /* +0x1d8 */
	u8 idstorage_45_2;               /* +0x1da */
	u8 idstorage_45_3;               /* +0x1db */
	SceUID callback_id;              /* +0x1dc */
} SceWlanCtx;

/* Forward declarations */
static int wlanSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static void wlanIfUp(void *ifp);
static void wlanIfDown(void *ifp);
static void wlanIfTx(void *ifp);
static void wlanIfIoctl(void *ifp, u32 cmd, void *arg2, u32 *arg3);
static int wlanKermitIntr7(void);
static int wlanKermitIntr8(void);
static SceUInt wlanAlarmHandler(void *common);
static SceUInt wlanSuspendAlarmHandler(void *common);
static void wlanPumpTx(SceWlanCtx *ctx);
static void wlanDeliverRx(void *ifp, void *buf, u32 len, s16 rssi_a, s16 rssi_b);
static u16 wlanGetChannelMaskFromIdStorage(void);
static int wlanResumeOrWakeup(SceWlanCtx *ctx, int is_suspend);
static int wlanHandleIoctlCmd(SceWlanCtx *ctx, u32 cmd, void *arg2, u32 *params);
static void wlanExportBssInfo(void *dst, const void *src, u32 ver);
static void wlanImportBssInfo(void *dst, const void *src, u32 ver);
static int wlanJoinIbss(SceWlanCtx *ctx, void *arg2, void *bss_info);
static void wlanQueueInit(SceWlanQueue *q);
static void wlanQueueDestroy(SceWlanQueue *q);
static int wlanQueuePushBack(SceWlanQueue *q, SceWlanTxNode *node);
static int wlanQueuePushFront(SceWlanQueue *q, SceWlanTxNode *node);
static SceWlanTxNode *wlanQueuePop(SceWlanQueue *q);
static int wlanQueueIsNotEmpty(SceWlanQueue *q);
static SceUID wlanCreateHeap(u32 size);
static int wlanDeleteHeap(SceUID uid);
static void *wlanAlloc(SceWlanCtx *ctx, u32 size);
static void *wlanTryAlloc(SceWlanCtx *ctx, u32 size);
static int wlanFree(SceWlanCtx *ctx, void *ptr);
static void wlanInitPrng(u32 seed);
static int wlanDoScan(void *scan_req, u32 *inout_len, void *out_buf, int compact);
static int wlanKermitGetSwitchAndPower(void);
static int wlanKermitInit(void);
static int wlanKermitTerm(int arg);
static int wlanPollSwitchState(void);
static int wlanGetMacAddr(void *out_mac);
static int wlanKermitJoin(void *bss);
static int wlanKermitCreateIbss(void *bss);
static int wlanKermitDisconnect(void);
static int wlanKermitTxData(void *buf, u32 len);
static int wlanKermitRxData(void *buf, u32 max_len);
static int wlanKermitSetWakeup(void *cfg);
static int wlanKermitGetResumeInfo(void *buf);
static int wlanKermitSetHostDiscover(void *cfg);
static int wlanKermitPollEvent(void);
static int wlanKermitSimple1Arg(u32 cmd, u32 val);
static int wlanKermitCmdBuf(u32 cmd, void *buf, u32 size, u32 dir_flags);
static int wlanKermitSimple2Arg(u32 cmd, u32 arg0, u32 arg1);
static int wlanKermitTransferBuf(u32 cmd, void *buf, u32 size, u32 dir_flags);
static int wlanKermitGetParam(u32 cmd, u32 id, void *buf, u32 *inout_len, u32 *out_aux);
static int wlanKermitGetSetOpt(u32 cmd, u32 id, u32 opt, u32 flags, void *buf, u32 *inout_len);
static int wlanKermitSetKey(u32 cmd, u32 id, const void *buf, u32 len);
static int wlanKermitSendGameMode(u32 cmd, void *kbuf, u32 id, u32 total_len, u32 flags, void *hdr_buf, u32 *hdr_len, u32 *out_sent);
static int wlanKermitPollScanState(void *out_buf);
static int wlanValidateOpt(u32 level, u32 opt);

int sceWlanDrv_driver_8DF43D4C(void);
int sceWlanDrv_driver_EE744B5C(SceSize args, void *argp);

/* .rodata tables */
static const u8 g_rate_vals[5] = { 0x00, 0x02, 0x04, 0x0b, 0x16 };

static const u32 g_freq_table[15] = {
	0x00000000,
	0x0024cde0,
	0x0024e168,
	0x0024f4f0,
	0x017254b0,
	0x01731800,
	0x00252f88,
	0x00254310,
	0x00255698,
	0x00256a20,
	0x00257da8,
	0x0177abe0,
	0x01786f30,
	0x0025b840,
	0x0025e720,
};

static const u16 g_region_codes[6] = {
	0x0010, 0x0020, 0x0030, 0x0031, 0x0032, 0x0040
};

static const u8 g_region_channels[6][14] = {
	{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0 },
	{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0 },
	{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0 },
	{ 10, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 10, 11, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 },
};

/* .data globals (0x7970 .. 0x79b8) */
static SceSysEventHandler g_sys_event_handler = {
	.size = sizeof(SceSysEventHandler),
	.name = "WlanSysEventHandler",
	.type_mask = 0x00514502,
	.handler = wlanSysEventHandler,
};

static u8 g_ether_addr[8] = {
	0x00, 0x22, 0x58, 0x83, 0x73, 0x28, 0x00, 0x00
};

/* .bss globals (0x79c0 .. 0x856c) */
static SceWlanCtx *g_wlan_ctx;
static s32 g_tx_delay_us;
static s32 g_unknown_79c8;
static s32 g_stop_delay_us;
static s32 g_unknown_79d0;
static s32 g_suspend_arg;
static u8 g_wakeup_cfg[0x28];
static u8 g_host_discover_cfg[0x28];
static SceWlanMbxMsg g_mbx_msg_7a28;
static SceWlanMbxMsg g_mbx_msg_7a4c;
static SceWlanMbxMsg g_mbx_msg_7a70;
static SceWlanMbxMsg g_mbx_msg_7a94;
static SceWlanMbxMsg g_mbx_msg_7ab8;
static SceWlanMbxMsg g_mbx_msg_7adc;
static u8 g_resume_buf[0x170];
static u32 g_drop_rate;
static u32 g_drop_duration;
static s32 g_prng_inited;
static u32 g_drop_end_time;
static u32 g_drop_next_time;
static s32 g_drop_active;
static u32 g_mt_state[521];
static s32 g_mt_index;
static u32 g_unknown_84b0;
static u32 g_unknown_84b4;
static u32 g_switch_and_power_state;
static s32 g_mac_cached;
static s32 g_scan_ready;
static u32 g_max_send_size;
static u8 g_scan_state_buf[0x90];
static u64 g_next_switch_poll_time;
static SceUID g_kermit_net_mutex;
static u8 *g_kermit_net_buf;
static u32 g_kermit_net_buf_size;

/* 0x0000 */
static SceWlanTxNode *wlanAllocTxNode(SceWlanCtx *ctx, u32 busy_val)
{
	SceWlanTxNode *res = NULL;
	int intr = sceKernelCpuSuspendIntr();
	SceWlanTxNode *cur = ctx->tx_cur;

	if (cur->busy == 0) {
		res = cur;
		cur->busy = busy_val;
	} else {
		SceWlanTxNode *p = cur->ring_next;
		while (p != cur) {
			if (p->busy == 0) {
				res = p;
				p->busy = busy_val;
				break;
			}
			p = p->ring_next;
		}
		if (res == NULL) {
			sceKernelCpuResumeIntr(intr);
			return NULL;
		}
	}

	SceWlanTxNode *next = cur->ring_next;
	while (next != cur) {
		if (next->busy == 0) {
			ctx->tx_cur = next;
			break;
		}
		next = next->ring_next;
	}

	sceKernelCpuResumeIntr(intr);
	return res;
}

/* 0x03b4 */
static int wlanInitCtxDefaults(SceWlanCtx *ctx)
{
	ctx->flags |= 0x200;
	ctx->unk_0a0 = 6;
	ctx->unk_0a8 = 2;
	ctx->channel_mask = 0x10;
	ctx->power_mode = 0x63;
	ctx->unk_0ca = 0x92a;
	ctx->unk_0cc = 4;
	ctx->unk_0c4 = 1;
	ctx->unk_0ce = 3;
	ctx->unk_0d0 = 0x64;
	ctx->unk_0a4 = 1;
	ctx->rate_idx = 0;
	ctx->unk_0c8 = 0x92a;
	ctx->unk_0d2 = 0;
	ctx->unk_0dc = 0;
	return 0;
}

/* 0x0424 */
static int wlanSetupMacAndChannel(SceWlanCtx *ctx)
{
	wlanGetMacAddr(ctx->mac_addr);
	ctx->channel_mask = wlanGetChannelMaskFromIdStorage();
	return 0;
}

/* 0x0e14 */
static int wlanMacThread(SceSize args, void *argp);

static int wlanStartMacThread(SceWlanCtx *ctx)
{
	SceWlanCtx *arg = ctx;
	SceUID thid = sceKernelCreateThread("SceWlanMac", wlanMacThread, 0x27, 0xc00, 0x00100001, NULL);
	ctx->mac_thid = thid;
	if (thid < 0) {
		return -1;
	}
	int ret = sceKernelStartThread(thid, sizeof(arg), &arg);
	return (ret == 0) ? 0 : -1;
}

/* 0x00b8: module_start / sceWlanDrv_driver_A9E6A56D */
int sceWlanDrv_driver_A9E6A56D(SceSize args, void *argp)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);

	int status = 0;
	SceWlanCtx *ctx = NULL;

	if (g_wlan_ctx != NULL) {
		status = 1;
		goto done;
	}

	SceUID vpl = wlanCreateHeap(0xe800);
	if (vpl < 0) {
		status = 1;
		goto done;
	}

	void *mem = NULL;
	if (sceKernelAllocateVpl(vpl, sizeof(SceWlanCtx), &mem, NULL) < 0) {
		mem = NULL;
	}
	ctx = (SceWlanCtx *)mem;
	if (ctx == NULL) {
		status = 1;
		goto done;
	}

	memset(ctx, 0, sizeof(SceWlanCtx));
	ctx->version_or_mode = 0x10000;
	ctx->vpl_id = vpl;
	ctx->lock_sema = sceKernelCreateSema("SceWlanMacIfLock", 0x101, 1, 1, NULL);
	ctx->mbx_id = sceKernelCreateMbx("SceWlanMac", 1, NULL);

	wlanQueueInit(&ctx->tx_queue);
	wlanQueueInit(&ctx->rx_queue);

	SceWlanTxNode *pool = (SceWlanTxNode *)wlanAlloc(ctx, 0x79e0);
	ctx->tx_pool = pool;
	memset(pool, 0, 0x79e0);

	u8 *buf_ptr = (u8 *)pool + 0x190;
	for (int i = 0; i < 20; i++) {
		pool[i].busy = 0;
		pool[i].ring_next = &pool[i + 1];
		pool[i].buf = buf_ptr;
		buf_ptr += 0x604;
	}
	pool[19].ring_next = pool;
	ctx->tx_buf_stride = 0x604;
	ctx->tx_node_count = 20;
	ctx->tx_cur = pool;

	ctx->rx_buf = (u8 *)wlanAlloc(ctx, 0x92a);

	SceWlanRssiNode *rpool = (SceWlanRssiNode *)wlanAlloc(ctx, 0xc8);
	ctx->rssi_pool = rpool;
	for (int i = 0; i < 24; i++) {
		rpool[i].flags = 0;
		rpool[i].rssi = 0;
		rpool[i].next = &rpool[i + 1];
	}
	rpool[24].flags = 0;
	rpool[24].rssi = 0;
	rpool[24].next = rpool;
	ctx->rssi_head = rpool;
	rpool[0].flags |= 2;

	u16 id_val = 0;
	if (sceIdStorageLookup(0x45, 0, &id_val, 2) >= 0) {
		ctx->idstorage_45_0 = id_val & 0xf000;
	} else {
		ctx->idstorage_45_0 = 0;
	}

	u8 b_val = 0;
	if (sceIdStorageLookup(0x45, 3, &b_val, 1) >= 0 && b_val == 1) {
		u8 b_val2 = 0;
		if (sceIdStorageLookup(0x45, 4, &b_val2, 1) >= 0) {
			ctx->idstorage_45_2 = 1;
			ctx->idstorage_45_3 = b_val2;
		} else {
			ctx->idstorage_45_2 = 0;
		}
	} else {
		ctx->idstorage_45_2 = 0;
	}

	if (ctx->idstorage_45_0 == 0x1000 && ctx->idstorage_45_2 != 1) {
		ctx->idstorage_45_2 = 1;
		ctx->idstorage_45_3 = 2;
	}

	wlanInitCtxDefaults(ctx);
	wlanStartMacThread(ctx);
	sceKernelRegisterSysEventHandler(&g_sys_event_handler);
	g_wlan_ctx = ctx;

done:
	wlanKermitInit();
	wlanSetupMacAndChannel(ctx);
	pspSetK1(k1);
	return status;
}

/* 0x0444: sceWlanDevAttach / sceWlanDrv_driver_1C18F5FE */
int sceWlanDrv_driver_1C18F5FE(void)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);

	int ret = 0x80410d0d;
	SceWlanCtx *ctx = g_wlan_ctx;
	if (ctx != NULL) {
		if (ctx->netif.if_internal != NULL) {
			ret = 0x80410d0f;
		} else {
			ret = sceNetIfhandle_driver_16042084(&ctx->netif);
			if (ret < 0) {
				ret = 0x80410d0d;
			} else {
				sceNetIfhandle_driver_C6D14282(&ctx->netif, ctx);
				u8 mac[8];
				*(u32 *)&mac[0] = *(u32 *)&ctx->mac_addr[0];
				*(u16 *)&mac[4] = *(u16 *)&ctx->mac_addr[4];
				ctx->netif.cb_up = wlanIfUp;
				ctx->netif.cb_down = wlanIfDown;
				ctx->netif.cb_tx = wlanIfTx;
				ctx->netif.cb_ioctl = wlanIfIoctl;
				ret = sceNetIfhandle_driver_AE81C0CB(&ctx->netif, mac, "wlan");
			}
		}
	}

	if (ret < 0 && ret != (int)0x80410d0f) {
		sceWlanDrv_driver_8DF43D4C();
	}

	pspSetK1(k1);
	return ret;
}

/* 0x0564: sceWlanDevDetach / sceWlanDrv_driver_8DF43D4C */
int sceWlanDrv_driver_8DF43D4C(void)
{
	u32 k1 = pspGetK1();
	SceWlanCtx *ctx = g_wlan_ctx;
	if (ctx != NULL) {
		pspSetK1(k1 << 11);
		sceNetIfhandle_driver_54D1AEA1(&ctx->netif);
		sceNetIfhandle_driver_C9344A59(&ctx->netif);
		ctx->netif.if_internal = NULL;
	}
	pspSetK1(k1);
	return 0;
}

/* 0x05c4 */
static void wlanIfUp(void *ifp)
{
	if (g_tx_delay_us > 0) {
		sceKernelDelayThread(g_tx_delay_us);
		g_tx_delay_us = 0;
	}

	SceWlanCtx *ctx = (SceWlanCtx *)sceNetIfhandle_driver_D5AD6DEA(ifp);
	if (ctx->flags & 0x80) {
		int intr = sceKernelCpuSuspendIntr();
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_F94BAF52(&ctx->netif, 1, 0);
		}
		sceKernelCpuResumeIntr(intr);
		return;
	}

	SceWlanMbxMsg *msg = (SceWlanMbxMsg *)wlanTryAlloc(ctx, sizeof(SceWlanMbxMsg));
	if (msg != NULL) {
		msg->type = 0x8000000a;
		msg->flags = 0;
		sceKernelSendMbx(ctx->mbx_id, msg);
	} else {
		int intr = sceKernelCpuSuspendIntr();
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_F94BAF52(ifp, 4, 0x80410d12);
		}
		sceKernelCpuResumeIntr(intr);
	}
}

/* 0x06b8 */
static void wlanIfDown(void *ifp)
{
	SceWlanCtx *ctx = (SceWlanCtx *)sceNetIfhandle_driver_D5AD6DEA(ifp);
	if (!(ctx->flags & 0x80)) {
		int intr = sceKernelCpuSuspendIntr();
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_F94BAF52(&ctx->netif, 2, 0);
		}
		sceKernelCpuResumeIntr(intr);
		return;
	}

	SceWlanMbxMsg *msg = (SceWlanMbxMsg *)wlanTryAlloc(ctx, sizeof(SceWlanMbxMsg));
	if (msg != NULL) {
		msg->type = 0x8000000b;
		msg->flags = 0;
		sceKernelSendMbx(ctx->mbx_id, msg);
	} else {
		int intr = sceKernelCpuSuspendIntr();
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_F94BAF52(ifp, 4, 0x80410d12);
		}
		sceKernelCpuResumeIntr(intr);
	}
}

/* 0x0794 */
static void wlanIfIoctl(void *ifp, u32 cmd, void *arg2, u32 *arg3)
{
	SceWlanCtx *ctx = (SceWlanCtx *)sceNetIfhandle_driver_D5AD6DEA(ifp);
	if (ctx->version_or_mode > 0x1ffff && (g_mbx_msg_7a28.flags & 1) != 0) {
		int intr = sceKernelCpuSuspendIntr();
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_F94BAF52(ifp, 4, 0x80410d0c);
		}
		sceKernelCpuResumeIntr(intr);
		return;
	}

	SceWlanMbxMsg *msg = (SceWlanMbxMsg *)wlanAlloc(ctx, sizeof(SceWlanMbxMsg));
	u32 flags = ctx->flags;
	int err = 0;
	u32 msg_type = cmd;

	if (!(flags & 0x200)) {
		err = 0x80410d0e;
		goto check_err;
	}

	if (cmd >= 0x48) {
		err = 0x80410d05;
		goto free_and_err;
	}

	switch (cmd) {
	case 0x02:
		msg_type = 7;
		goto send_msg;
	case 0x03:
		msg_type = 8;
		goto send_msg;
	case 0x06:
		msg_type = 0x0b;
		goto send_msg;
	case 0x07:
		msg_type = 0x0c;
		goto send_msg;
	case 0x32:
		if (flags & 4) {
			err = 0x80410d02;
			goto free_and_err;
		}
		if (flags & 1) {
			err = 0x80410d01;
			goto free_and_err;
		}
		if (flags & 8) {
			err = 0x80410d01;
			goto free_and_err;
		}
		msg_type = 0x102;
		goto send_msg;
	case 0x33:
		msg_type = 0x103;
		goto send_msg;
	case 0x34:
		if (!(flags & 2)) {
			msg_type = 2;
			goto send_msg;
		}
		((s32 *)arg2)[7] = -2;
		err = 0x80410d03;
		goto free_and_err;
	case 0x35:
		msg_type = 5;
		goto check_join_ibss;
	case 0x36:
		msg_type = 3;
	check_join_ibss:
		if (flags & 2) {
			((s32 *)arg2)[5] = -2;
			err = 0x80410d03;
			goto free_and_err;
		}
		if (flags & 4) {
			((s32 *)arg2)[5] = -3;
			err = 0x80410d02;
			goto free_and_err;
		}
		if (flags & 1) {
			((s32 *)arg2)[5] = -4;
			err = 0x80410d01;
			goto free_and_err;
		}
		if (flags & 8) {
			((s32 *)arg2)[5] = -5;
			err = 0x80410d01;
			goto free_and_err;
		}
		{
			u8 *cfg = (u8 *)((void **)arg2)[4];
			u32 mode = *(u32 *)(cfg + 0x28);
			u16 submode = *(u16 *)(cfg + 0x3e) & 3;
			if (mode == 1) {
				if (submode != 1) {
					((s32 *)arg2)[5] = -0x65;
					err = 0x80410d04;
					goto free_and_err;
				}
				goto send_msg_keep_type;
			} else {
				if (submode == 2) {
					goto send_msg;
				}
				((s32 *)arg2)[5] = -0xc9;
				err = 0x80410d04;
				goto free_and_err;
			}
		}
	case 0x37:
		msg_type = 0x10c;
		if (!(ctx->flags & 9)) {
			err = 0x80410d06;
		}
		goto check_err;
	case 0x38: {
		u32 cur_flags = ctx->flags;
		msg_type = (flags & 0x800) ? 0x301 : 4;
		if (!(cur_flags & 9)) {
			err = 0x80410d06;
			goto free_and_err;
		}
		if (cur_flags & 2) {
			((s32 *)arg2)[4] = -2;
			err = 0x80410d03;
			goto free_and_err;
		}
		if (cur_flags & 4) {
			((s32 *)arg2)[4] = -3;
			err = 0x80410d02;
			goto free_and_err;
		}
		if (g_unknown_79c8 != 0) {
			sceKernelDelayThread(100000);
			goto send_msg_keep_type;
		}
		sceKernelDelayThread(500);
		wlanPumpTx(ctx);
		for (int i = 50; i > 0; i--) {
			if (!wlanQueueIsNotEmpty(&ctx->tx_queue)) {
				break;
			}
			if (ctx->flags & 0x3000) {
				goto send_msg_keep_type;
			}
			sceKernelDelayThread(10000);
			wlanPumpTx(ctx);
		}
		goto send_msg;
	}
	case 0x39: {
		u8 rates[5];
		memcpy(rates, g_rate_vals, 5);
		u8 *out = (u8 *)arg2;
		out[0x10] = ctx->rssi_base;
		out[0x11] = rates[ctx->rate_idx];
		SceWlanRssiNode *head = ctx->rssi_head;
		s16 count = 0;
		s16 sum = 0;
		for (SceWlanRssiNode *p = head->next; p != head; p = p->next) {
			if (p->flags & 1) {
				count++;
				sum += p->rssi;
			}
		}
		s16 avg = (count > 0) ? (sum / count) : 0;
		*(s16 *)(out + 0x12) = avg;
		*(u16 *)(out + 0x14) = ctx->unk_15e;
		u8 bar = 1;
		if (avg > 0) {
			if (avg < 40) {
				bar = (u8)((avg * 100) / 40);
			} else {
				bar = 100;
			}
		}
		out[0x16] = bar;
		goto free_and_ok;
	}
	case 0x3a: {
		u32 state = 1;
		if (flags & 2) {
			state = 2;
		} else if (flags & 4) {
			state = 3;
		} else if (flags & 9) {
			state = 5;
		} else if (flags & 0x400) {
			state = 4;
		}
		((u32 *)arg2)[4] = state;
		goto free_and_ok;
	}
	case 0x3e: {
		u32 subcmd = ((u32 *)arg2)[4];
		switch (subcmd) {
		case 1: msg_type = 0x101; goto send_msg;
		case 2: msg_type = 0x110; goto send_msg;
		case 3: msg_type = 0x202; goto send_msg;
		case 5: msg_type = 0x215; goto send_msg;
		case 6: msg_type = 0x213; goto send_msg;
		case 7: msg_type = 0x211; goto send_msg;
		case 8: msg_type = 0x204; goto send_msg;
		case 10: msg_type = 0x400; goto send_msg;
		default:
			err = 0x80410d05;
			goto free_and_err;
		}
	}
	case 0x3f: {
		u32 subcmd = ((u32 *)arg2)[4];
		switch (subcmd) {
		case 2: msg_type = 0x111; goto send_msg;
		case 3: msg_type = 0x201; goto send_msg;
		case 5: msg_type = 0x214; goto send_msg;
		case 6: msg_type = 0x212; goto send_msg;
		case 7: msg_type = 0x210; goto send_msg;
		case 8: msg_type = 0x203; goto send_msg;
		case 9: msg_type = 0x220; goto send_msg;
		default:
			err = 0x80410d05;
			goto free_and_err;
		}
	}
	case 0x40:
		if (flags & 1) {
			err = 0x80410d01;
			goto free_and_err;
		}
		if (flags & 8) {
			err = 0x80410d01;
			goto free_and_err;
		}
		msg_type = 0x10a;
		goto send_msg;
	case 0x42:
		msg_type = 0x10d;
		goto send_msg;
	case 0x43:
		msg_type = 0x10e;
		goto send_msg;
	case 0x44:
		if (flags & 1) {
			err = 0x80410d01;
			goto free_and_err;
		}
		if (flags & 8) {
			err = 0x80410d01;
			goto free_and_err;
		}
		msg_type = 0x300;
		goto send_msg_keep_type;
	case 0x45:
		msg_type = 0x500;
		goto send_msg;
	case 0x46:
		msg_type = 0x501;
		goto send_msg;
	case 0x47:
		if (flags & 1) {
			err = 0x80410d01;
			goto free_and_err;
		}
		if (flags & 8) {
			err = 0x80410d01;
			goto free_and_err;
		}
		msg_type = 0x10f;
		goto send_msg;
	default:
		err = 0x80410d05;
		goto free_and_err;
	}

check_err:
	if (err == 0) {
		goto send_msg_keep_type;
	}
	if (err <= 0) {
		goto free_and_err;
	}

free_and_ok:
	wlanFree(ctx, msg);
	{
		int intr = sceKernelCpuSuspendIntr();
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_F94BAF52(ifp, 3, 0);
		}
		sceKernelCpuResumeIntr(intr);
	}
	return;

free_and_err:
	wlanFree(ctx, msg);
	{
		int intr = sceKernelCpuSuspendIntr();
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_F94BAF52(ifp, 4, err);
		}
		sceKernelCpuResumeIntr(intr);
	}
	return;

send_msg:
send_msg_keep_type:
	msg->type = msg_type;
	msg->arg_ptr = arg2;
	msg->flags = 0;
	msg->param0 = arg3[0];
	msg->param1 = arg3[1];
	sceKernelSendMbx(ctx->mbx_id, msg);
}

/* 0x216c */
static void wlanNotifyCallback(SceWlanCtx *ctx, int arg)
{
	if (ctx->callback_id > 0) {
		sceKernelNotifyCallback(ctx->callback_id, arg);
	}
}

/* 0x1b94 */
static void wlanFlushQueues(SceWlanCtx *ctx)
{
	SceWlanTxNode *node;
	while ((node = wlanQueuePop(&ctx->tx_queue)) != NULL) {
		int intr = sceKernelCpuSuspendIntr();
		ctx->tx_cur = node;
		node->busy = 0;
		sceKernelCpuResumeIntr(intr);
	}
	while ((node = wlanQueuePop(&ctx->rx_queue)) != NULL) {
		int intr = sceKernelCpuSuspendIntr();
		ctx->tx_cur = node;
		node->busy = 0;
		sceKernelCpuResumeIntr(intr);
	}
}

/* 0x1a74 */
static void wlanHandlePowerSwitchEvent(int switch_on, SceWlanCtx *ctx)
{
	if (ctx->flags & 0x2000) {
		return;
	}
	if (switch_on == 0) {
		if (ctx->alarm_id != 0) {
			ctx->unk_1d0 = 0;
		} else {
			ctx->unk_1d0 = 0;
			SceUID aid = sceKernelSetAlarm(0x186a00, wlanAlarmHandler, ctx);
			if (aid > ctx->alarm_id) {
				ctx->alarm_id = aid;
			}
		}
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_263767F6(&ctx->netif, 4, 0);
		}
	} else {
		ctx->unk_1d0 = 1;
		if (ctx->alarm_id != 0) {
			sceKernelCancelAlarm(ctx->alarm_id);
			ctx->alarm_id = 0;
		}
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_263767F6(&ctx->netif, 3, 0);
		}
		int intr = sceKernelCpuSuspendIntr();
		SceWlanMbxMsg *m = &g_mbx_msg_7a28;
		m->type = 0x80000006;
		if (!(m->flags & 1)) {
			m->flags = 3;
			sceKernelSendMbx(ctx->mbx_id, m);
		}
		sceKernelCpuResumeIntr(intr);
	}
}

/* 0x0ecc */
static int wlanMacThread(SceSize args, void *argp)
{
	sceKermit_driver_5280B410(7, wlanKermitIntr7);
	sceKermit_driver_5280B410(8, wlanKermitIntr8);

	SceWlanCtx *ctx = *(SceWlanCtx **)argp;
	ctx->thread_running = 1;
	int need_pump = 0;

	while (ctx->thread_running != 0) {
		SceWlanMbxMsg *msg = NULL;
		sceKernelReceiveMbx(ctx->mbx_id, (void **)&msg, NULL);

		u32 flags = ctx->flags;
		u32 type = msg->type;

		if (flags & 0x3000) {
			if (type == 0x8000000a) {
				int intr = sceKernelCpuSuspendIntr();
				if (ctx->netif.if_internal != NULL) {
					sceNetIfhandle_driver_F94BAF52(&ctx->netif, 1, 0x80410d16);
				}
				sceKernelCpuResumeIntr(intr);
			} else if (type > 0x8000000a) {
				if (type <= 0x8000000d) {
					if (type >= 0x8000000c) {
						int intr = sceKernelCpuSuspendIntr();
						if (ctx->netif.if_internal != NULL) {
							sceNetIfhandle_driver_F94BAF52(&ctx->netif, 2, 0x80410d16);
						}
						sceKernelCpuResumeIntr(intr);
					}
				} else if (type == 0x8000000e) {
					wlanResumeOrWakeup(ctx, (int)msg->arg_val);
					ctx->flags = (ctx->flags | 0x1000) & ~0x2000u;
				} else if (type == 0x8000000f) {
					if (ctx->alarm_id != 0) {
						sceKernelCancelAlarm(ctx->alarm_id);
						ctx->alarm_id = 0;
					}
					ctx->flags &= ~0x4000u;
					if (g_unknown_79d0 != 0) {
						sceKernelDelayThread(g_unknown_79d0 * 1000);
					}
					ctx->flags &= ~0x1000u;
				} else if (type != 0xc0000000u) {
					int intr = sceKernelCpuSuspendIntr();
					if (ctx->netif.if_internal != NULL) {
						sceNetIfhandle_driver_F94BAF52(&ctx->netif, 4, 0x80410d16);
					}
					sceKernelCpuResumeIntr(intr);
				}
			} else {
				if (type == 0x80000005) {
					ctx->flags &= ~0x200u;
					if (flags & 9) {
						wlanFlushQueues(ctx);
						memset(ctx->conn_info, 0, sizeof(ctx->conn_info));
						ctx->unk_15c = 0;
						ctx->flags &= ~9u;
						ctx->unk_15e = 0;
						wlanNotifyCallback(ctx, 0);
					}
					if (ctx->flags & 0x80) {
						ctx->flags &= ~0x80u;
					}
					ctx->thread_running = 0;
				} else if (type < 0x80000001u) {
					int intr = sceKernelCpuSuspendIntr();
					if (ctx->netif.if_internal != NULL) {
						sceNetIfhandle_driver_F94BAF52(&ctx->netif, 4, 0x80410d16);
					}
					sceKernelCpuResumeIntr(intr);
				}
			}
		} else {
			if (type <= 0x80000009u) {
				if (type < 0x80000008u) {
					if (type == 0x80000001u) {
						g_mbx_msg_7a94.flags &= ~1u;
						if ((ctx->flags & 0x80) && !(ctx->flags & 0x800)) {
							for (int i = 0; i < 10; i++) {
								int rx_len = wlanKermitRxData(ctx->rx_buf, 0x92a);
								if (rx_len <= 0) {
									ctx->flags &= ~0x100u;
									break;
								}
								wlanDeliverRx(&ctx->netif, ctx->rx_buf, rx_len, 0x50, -0x5a);
							}
						}
					} else if (type > 0x80000001u) {
						if (type == 0x80000002u) {
							if ((flags & 0x80) && !(flags & 0x800)) {
								SceWlanTxNode *node;
								while ((node = wlanQueuePop(&ctx->tx_queue)) != NULL) {
									if (ctx->flags & 9) {
										if (wlanKermitTxData(node->buf, node->len) < 0) {
											wlanQueuePushFront(&ctx->tx_queue, node);
											break;
										}
									}
									int intr = sceKernelCpuSuspendIntr();
									ctx->tx_cur = node;
									node->busy = 0;
									sceKernelCpuResumeIntr(intr);
								}
								ctx->flags &= ~0x100u;
								need_pump = 1;
							}
						} else if (type == 0x80000005u) {
							ctx->flags &= ~0x200u;
							if (flags & 9) {
								wlanFlushQueues(ctx);
								memset(ctx->conn_info, 0, sizeof(ctx->conn_info));
								ctx->unk_15c = 0;
								ctx->flags &= ~9u;
								ctx->unk_15e = 0;
								wlanNotifyCallback(ctx, 0);
							}
							if (ctx->flags & 0x80) {
								ctx->flags &= ~0x80u;
							}
							ctx->thread_running = 0;
						} else if (type == 0x80000006u || type == 0x80000007u) {
							int intr = sceKernelCpuSuspendIntr();
							msg->flags &= ~1u;
							ctx->flags &= ~0x200u;
							sceKernelCpuResumeIntr(intr);
							if (type == 0x80000006u) {
								if (ctx->flags & 0x80) {
									ctx->flags &= ~0x80u;
								}
								ctx->flags = (ctx->flags & 0x16000) | 0x200;
							} else {
								if (ctx->flags & 0x80) {
									ctx->flags &= ~0x80u;
								}
								ctx->flags &= ~0x200u;
								if (ctx->flags & 9) {
									wlanFlushQueues(ctx);
									memset(ctx->conn_info, 0, sizeof(ctx->conn_info));
									ctx->unk_15c = 0;
									ctx->flags &= ~9u;
									ctx->unk_15e = 0;
									wlanNotifyCallback(ctx, 0);
								}
								ctx->unk_0e0 = 0;
								memset(ctx->unk_0e4, 0, sizeof(ctx->unk_0e4));
								ctx->flags &= ~0x20000u;
							}
						} else {
							wlanHandleIoctlCmd(ctx, msg->type, msg->arg_ptr, &msg->param0);
						}
					} else if (type == 0x600) {
						u32 hd_pkt[44];
						memset(hd_pkt, 0, 0x30);
						if (ctx->flags & 0x10000) {
							if (ctx->flags & 0x20000) {
								if (ctx->flags & 0x80) {
									ctx->flags &= ~0x80u;
								}
								ctx->flags &= 0x16000u;
								wlanKermitSetHostDiscover(NULL);
								ctx->flags = (ctx->flags | 0x200) & ~0x20000u;
							}
							hd_pkt[0] = 1;
							hd_pkt[1] = 0;
							hd_pkt[2] = g_host_discover_cfg[5];
							hd_pkt[3] = 0xffffffffu;
							memcpy(&hd_pkt[4], &g_host_discover_cfg[6], 0x20);
							s32 end_idx = -1;
							int has_star = 0;
							for (int i = 0; i < 32; i++) {
								u8 ch = g_host_discover_cfg[6 + i];
								if (ch == 0 && end_idx == -1) {
									end_idx = i;
								}
								if (ch == '*') {
									has_star = 1;
								}
							}
							hd_pkt[1] = has_star;
							hd_pkt[3] = (u32)end_idx;
							if (!(ctx->flags & 1)) {
								ctx->flags &= ~1u;
							}
							if (!(ctx->flags & 8)) {
								memset(&hd_pkt[12], 0, 0x70);
								memcpy(&hd_pkt[14], &hd_pkt[4], 0x20);
								*((u8 *)&hd_pkt[12] + 7) = (u8)hd_pkt[3];
								*((u8 *)&hd_pkt[12] + 6) = (u8)hd_pkt[2];
								ctx->flags |= 4;
								if (wlanKermitCreateIbss(&hd_pkt[12]) != 0) {
									ctx->flags |= 8;
									wlanNotifyCallback(ctx, (int)hd_pkt[2]);
								}
								ctx->flags &= ~4u;
							}
							wlanKermitSetHostDiscover(hd_pkt);
							ctx->flags |= 0x20000;
						} else if (ctx->flags & 0x20000) {
							if (ctx->flags & 0x80) {
								ctx->flags &= ~0x80u;
							}
							ctx->flags &= 0x16000u;
							wlanKermitSetHostDiscover(NULL);
							ctx->flags = (ctx->flags | 0x200) & ~0x20000u;
						}
					} else if (type == 0x601) {
						g_mbx_msg_7ab8.flags &= ~1u;
						if (ctx->flags & 0x20000) {
							wlanKermitSetHostDiscover(NULL);
							ctx->flags &= ~8u;
							wlanNotifyCallback(ctx, 0);
							ctx->flags &= ~0x20000u;
							if (ctx->flags & 0x10000) {
								memset(g_host_discover_cfg, 0, sizeof(g_host_discover_cfg));
								ctx->flags &= ~0x10000u;
								int intr = sceKernelCpuSuspendIntr();
								if (ctx->netif.if_internal != NULL) {
									sceNetIfhandle_driver_263767F6(&ctx->netif, 12, 0);
								}
								sceKernelCpuResumeIntr(intr);
							}
						}
					} else {
						wlanHandleIoctlCmd(ctx, msg->type, msg->arg_ptr, &msg->param0);
					}
				}
			} else if (type == 0x8000000au) {
				if (ctx->flags & 0x200) {
					if (!(ctx->flags & 0x80)) {
						ctx->flags |= 0x80;
					}
					ctx->flags &= ~0x100u;
					ctx->unk_004 = 2;
					int intr = sceKernelCpuSuspendIntr();
					if (ctx->netif.if_internal != NULL) {
						sceNetIfhandle_driver_F94BAF52(&ctx->netif, 1, 0);
					}
					sceKernelCpuResumeIntr(intr);
				} else {
					int intr = sceKernelCpuSuspendIntr();
					if (ctx->netif.if_internal != NULL) {
						sceNetIfhandle_driver_F94BAF52(&ctx->netif, 1, 0x80410d0c);
					}
					sceKernelCpuResumeIntr(intr);
				}
			} else if (type == 0x8000000bu) {
				if ((ctx->flags & 0x280) == 0x280) {
					ctx->flags &= ~0x80u;
				}
				int intr = sceKernelCpuSuspendIntr();
				if (ctx->netif.if_internal != NULL) {
					sceNetIfhandle_driver_F94BAF52(&ctx->netif, 2, 0);
				}
				sceKernelCpuResumeIntr(intr);
			} else if (type == 0x8000000du) {
				need_pump = 1;
				int intr = sceKernelCpuSuspendIntr();
				msg->flags &= ~1u;
				sceKernelCpuResumeIntr(intr);
			} else if (type == 0x80000013u) {
				g_mbx_msg_7adc.flags &= ~1u;
				int ev = wlanKermitPollEvent();
				if (ev & 2) {
					int sw = wlanKermitGetSwitchAndPower();
					wlanHandlePowerSwitchEvent(sw, ctx);
				}
				if (ev & 1) {
					g_mbx_msg_7ab8.flags &= ~1u;
					if (ctx->flags & 0x20000) {
						wlanKermitSetHostDiscover(NULL);
						ctx->flags &= ~8u;
						wlanNotifyCallback(ctx, 0);
						ctx->flags &= ~0x20000u;
						if (ctx->flags & 0x10000) {
							memset(g_host_discover_cfg, 0, sizeof(g_host_discover_cfg));
							ctx->flags &= ~0x10000u;
							int intr = sceKernelCpuSuspendIntr();
							if (ctx->netif.if_internal != NULL) {
								sceNetIfhandle_driver_263767F6(&ctx->netif, 12, 0);
							}
							sceKernelCpuResumeIntr(intr);
						}
					}
				}
			} else if (type != 0x80000010u && type != 0xc0000000u && type != 0xc0000001u) {
				wlanHandleIoctlCmd(ctx, msg->type, msg->arg_ptr, &msg->param0);
			}
		}

		u32 msg_flags = msg->flags;
		if (msg_flags & 2) {
			int intr = sceKernelCpuSuspendIntr();
			msg->flags &= ~1u;
			sceKernelCpuResumeIntr(intr);
			msg_flags = msg->flags;
		}
		if (!(msg_flags & 2)) {
			wlanFree(ctx, msg);
		}
		if (need_pump) {
			wlanPumpTx(ctx);
			need_pump = 0;
		}
	}

	sceKernelExitDeleteThread(0);
	return 0;
}

/* 0x1c34: sceWlanSetWakeUp / sceWlanDrv_driver_0CAB500F */
int sceWlanDrv_driver_0CAB500F(int enable, const void *cfg)
{
	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	SceWlanCtx *ctx = g_wlan_ctx;

	if ((s32)((((u32)cfg + 0x28) | (u32)cfg) & k1_shifted) < 0) {
		pspSetK1(k1);
		return 0x80000103;
	}

	memset(g_wakeup_cfg, 0, sizeof(g_wakeup_cfg));
	if (enable != 0) {
		if (ctx->flags & 0x3000) {
			ctx->flags &= ~0x4000u;
			pspSetK1(k1);
			return 0x80410d16;
		}
		memcpy(g_wakeup_cfg, cfg, sizeof(g_wakeup_cfg));
		ctx->flags |= 0x4000;
	} else {
		ctx->flags &= ~0x4000u;
	}

	pspSetK1(k1);
	return 0;
}

/* 0x1d24: sceWlanSetHostDiscover / sceWlanDrv_driver_58EDD140 */
int sceWlanDrv_driver_58EDD140(int enable, const void *cfg)
{
	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	pspSetK1(k1_shifted);

	if ((s32)((((u32)cfg + 0x28) | (u32)cfg) & k1_shifted) < 0) {
		pspSetK1(k1);
		return 0x80000103;
	}

	SceWlanCtx *ctx = g_wlan_ctx;
	memset(g_host_discover_cfg, 0, sizeof(g_host_discover_cfg));
	if (enable != 0) {
		if (ctx->flags & 0x3000) {
			ctx->flags &= ~0x10000u;
			pspSetK1(k1);
			return 0x80410d16;
		}
		memcpy(g_host_discover_cfg, cfg, sizeof(g_host_discover_cfg));
		ctx->flags |= 0x10000;
	} else {
		ctx->flags &= ~0x10000u;
	}

	SceWlanMbxMsg *msg = (SceWlanMbxMsg *)wlanTryAlloc(ctx, sizeof(SceWlanMbxMsg));
	if (msg == NULL) {
		pspSetK1(k1);
		return 0x80410d12;
	}
	msg->type = 0x600;
	msg->flags = 0;
	sceKernelSendMbx(ctx->mbx_id, msg);
	pspSetK1(k1);
	return 0;
}

/* 0x1e84 */
static int wlanSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	SceWlanCtx *ctx = g_wlan_ctx;
	if (ev_id == 0x4000) {
		return 0;
	}
	if (ev_id < 0x4001) {
		if (ev_id == 0x400) {
			if (ctx->netif.if_internal != NULL) {
				sceNetIfhandle_driver_263767F6(&ctx->netif, 7, 0);
			}
			if (ctx->flags & 9) {
				sceKernelSetAlarm(0xf4240, wlanSuspendAlarmHandler, &g_suspend_arg);
				return 0;
			}
			int intr = sceKernelCpuSuspendIntr();
			s32 arg_val = g_suspend_arg;
			ctx->flags |= 0x2000;
			sceKernelCpuResumeIntr(intr);
			if (ctx->mac_thid > 0) {
				sceKernelChangeThreadPriority(ctx->mac_thid, 0x10);
			}
			intr = sceKernelCpuSuspendIntr();
			SceWlanMbxMsg *m = &g_mbx_msg_7a70;
			m->type = 0x8000000e;
			if (!(m->flags & 1)) {
				m->flags = 3;
				m->arg_val = (u32)arg_val;
				sceKernelSendMbx(ctx->mbx_id, m);
			}
			sceKernelCpuResumeIntr(intr);
			return 0;
		}
		if (ev_id == 0x402) {
			if (!(ctx->flags & 0x1000)) {
				return 0x80000021;
			}
			u32 *p = ((u32 **)param)[4];
			if (ctx->flags & 0x4000) {
				*p |= 0x10;
			} else {
				*p &= ~0x10u;
			}
			return 0;
		}
		return 0;
	}
	if (ev_id == 0x100000) {
		if (wlanKermitGetResumeInfo(g_resume_buf) == 0 && *(u32 *)g_resume_buf != 0) {
			if (ctx->netif.if_internal != NULL) {
				sceNetIfhandle_driver_263767F6(&ctx->netif, 9, 0);
			}
			int intr = sceKernelCpuSuspendIntr();
			SceWlanMbxMsg *m = &g_mbx_msg_7a70;
			m->type = 0x8000000f;
			if (!(m->flags & 1)) {
				m->flags = 3;
				m->arg_val = 1;
				sceKernelSendMbx(ctx->mbx_id, m);
			}
			sceKernelCpuResumeIntr(intr);
		} else {
			if (ctx->netif.if_internal != NULL) {
				sceNetIfhandle_driver_263767F6(&ctx->netif, 8, 0);
			}
			int intr = sceKernelCpuSuspendIntr();
			SceWlanMbxMsg *m = &g_mbx_msg_7a70;
			if (!(m->flags & 1)) {
				m->type = 0x8000000f;
				m->flags = 3;
				m->arg_val = 0;
				sceKernelSendMbx(ctx->mbx_id, m);
			}
			sceKernelCpuResumeIntr(intr);
		}
		return 0;
	}
	if (ev_id == 0x100002) {
		if (!(ctx->flags & 0x1000)) {
			if (ctx->mac_thid > 0) {
				sceKernelChangeThreadPriority(ctx->mac_thid, 0x27);
			}
			return 0;
		}
		return 0x80000021;
	}
	return 0;
}

/* 0x2194: sceWlanDrv_driver_EDD207B1 */
int sceWlanDrv_driver_EDD207B1(int cmd, void *arg)
{
	u32 val = (u32)arg;
	if (cmd == 4) {
		g_unknown_84b0 = val;
	}
	if (cmd == 5) {
		g_unknown_79c8 = (s32)val;
	}
	if (cmd == 6) {
		g_stop_delay_us = (s32)(val * 1000);
	}
	if (cmd == 7) {
		g_tx_delay_us = (s32)(val * 1000);
	}
	if (cmd == 8) {
		g_unknown_84b4 = val;
	}
	if (cmd == 9) {
		g_unknown_79d0 = (s32)val;
	}
	if (cmd != 0) {
		return -1;
	}
	const s8 *s = (const s8 *)arg;
	if (s[0] == 0 || s[1] == 0 || s[2] == 0) {
		return 0x80000101;
	}
	SceWlanCtx *ctx = g_wlan_ctx;
	ctx->country_str[0] = (u8)s[0];
	ctx->country_str[1] = (u8)s[1];
	ctx->country_str[2] = (u8)s[2];
	return 0;
}

/* 0x2284 */
static int wlanLock(SceWlanCtx *ctx)
{
	return sceKernelWaitSema(ctx->lock_sema, 1, NULL);
}

/* 0x22a8 */
static int wlanUnlock(SceWlanCtx *ctx)
{
	return sceKernelSignalSema(ctx->lock_sema, 1);
}

/* 0x22c8 */
static void wlanResetRssiList(SceWlanCtx *ctx)
{
	SceWlanRssiNode *head = ctx->rssi_head;
	for (SceWlanRssiNode *p = head->next; p != head; p = p->next) {
		p->flags = 0;
		p->rssi = 0;
	}
	head->flags |= 2;
}

/* 0x274c */
static void wlanStopMacThread(SceWlanCtx *ctx)
{
	s32 delay = 0;
	if (ctx->flags & 8) {
		delay = g_stop_delay_us;
	}
	if (ctx->mac_thid > 0) {
		SceWlanMbxMsg msg;
		msg.type = 0x80000005;
		msg.flags = 2;
		if (ctx->worker_thid > 0) {
			sceKernelTerminateThread(ctx->worker_thid);
		}
		sceKernelSendMbx(ctx->mbx_id, &msg);
		sceKernelWaitThreadEnd(ctx->mac_thid, NULL);
	}
	if (delay > 0) {
		sceKernelDelayThread(delay);
	}
}

/* 0x22fc: module_stop / sceWlanDrv_driver_EE744B5C */
int sceWlanDrv_driver_EE744B5C(SceSize args, void *argp)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);

	SceWlanCtx *ctx = g_wlan_ctx;
	if (ctx != NULL) {
		SceUID vpl = ctx->vpl_id;
		sceKernelUnregisterSysEventHandler(&g_sys_event_handler);
		wlanStopMacThread(ctx);
		wlanFree(ctx, ctx->rssi_pool);
		wlanFree(ctx, ctx->rx_buf);
		wlanQueueDestroy(&ctx->tx_queue);
		wlanQueueDestroy(&ctx->rx_queue);
		wlanFree(ctx, ctx->tx_pool);
		sceKernelDeleteSema(ctx->lock_sema);
		sceKernelDeleteMbx(ctx->mbx_id);
		sceKernelFreeVpl(vpl, ctx);
		wlanDeleteHeap(vpl);
	}
	g_wlan_ctx = NULL;
	pspSetK1(k1);
	return 0;
}

/* 0x23bc: module_reboot_before */
int module_reboot_before(SceSize args, void *argp)
{
	sceWlanDrv_driver_EE744B5C(0, NULL);
	return 0;
}

/* 0x23dc: sceWlanDevIsPowerOn / sceWlanDrv_driver_0387523A */
int sceWlanDrv_driver_0387523A(void)
{
	return 1;
}

/* 0x23e4: sceWlanDevGetMacAddr */
int sceWlanDevGetMacAddr(u8 *mac)
{
	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	SceWlanCtx *ctx = g_wlan_ctx;

	if (mac == NULL || (s32)(k1_shifted & (u32)mac) < 0) {
		pspSetK1(k1);
		return 0x80410d13;
	}
	if (ctx == NULL) {
		pspSetK1(k1);
		return 0x80410d0d;
	}
	memcpy(mac, ctx->mac_addr, 6);
	pspSetK1(k1);
	return 0;
}

/* 0x2454: sceWlanDevIsGameMode / sceWlanDrv_driver_7C0EC2A9 */
int sceWlanDrv_driver_7C0EC2A9(void)
{
	return (g_wlan_ctx->flags >> 11) & 1;
}

/* 0x2468: sceWlanGPPrevEstablishActive / sceWlanDrv_driver_ED79DCD4 */
int sceWlanDrv_driver_ED79DCD4(void)
{
	return 0x80410d05;
}

/* 0x2474: sceWlanDrv_lib_6D5C839E / sceWlanDrv_driver_60A078E9 */
int sceWlanDrv_driver_60A078E9(void)
{
	return 0x80410d05;
}

/* 0x2480: sceWlanGPSend / sceWlanDrv_driver_EEC2F8AB */
int sceWlanDrv_driver_EEC2F8AB(void)
{
	return 0x80410d05;
}

/* 0x248c: sceWlanGPRecv / sceWlanDrv_driver_04DBA413 */
int sceWlanDrv_driver_04DBA413(void)
{
	return 0x80410d05;
}

/* 0x2498: sceWlanGPRegisterCallback / sceWlanDrv_driver_02E66C38 */
int sceWlanDrv_driver_02E66C38(void)
{
	return 0x80410d05;
}

/* 0x24a4: sceWlanGPUnRegisterCallback / sceWlanDrv_driver_54764A18 */
int sceWlanDrv_driver_54764A18(void)
{
	return 0x80410d05;
}

/* 0x24b0: sceWlanDrv_lib_81579D36 / sceWlanDrv_driver_5E3A5AD3 */
int sceWlanDrv_driver_5E3A5AD3(void)
{
	return 0x80410d05;
}

/* 0x24bc: sceWlanDrv_lib_5BAA1FE5 / sceWlanDrv_driver_B17481BB */
int sceWlanDrv_driver_B17481BB(void)
{
	return 0x80410d05;
}

/* 0x24c8: sceWlanDrv_lib_4C14BACA / sceWlanDrv_driver_F713143F */
int sceWlanDrv_driver_F713143F(void)
{
	return 0x80410d05;
}

/* 0x24d4: sceWlanDrv_lib_2D0FAE4E / sceWlanDrv_driver_041810C3 */
int sceWlanDrv_driver_041810C3(void)
{
	return 0x80410d05;
}

/* 0x24e0: sceWlanDrv_lib_56F467CA / sceWlanDrv_driver_53F89D4E */
int sceWlanDrv_driver_53F89D4E(void)
{
	return 0x80410d05;
}

/* 0x24ec: sceWlanGetSwitchState */
int sceWlanGetSwitchState(void)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	int ret = wlanPollSwitchState();
	pspSetK1(k1);
	return ret;
}

/* 0x2518: sceWlanGetEtherAddr */
int sceWlanGetEtherAddr(u8 *addr)
{
	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	pspSetK1(k1_shifted);

	if ((s32)((((u32)addr + 8) | (u32)addr) & k1_shifted) < 0) {
		pspSetK1(k1);
		return 0x80410d13;
	}
	int ret = wlanGetMacAddr(addr);
	pspSetK1(k1);
	return ret;
}

/* 0x256c */
static int wlanIsChannelValid(const u16 *region_ptr, u8 channel)
{
	u16 reg = *region_ptr;
	for (int c = 0; c < 14; c++) {
		int idx = 0;
		for (; idx < 6; idx++) {
			if (g_region_codes[idx] == reg) {
				break;
			}
		}
		if (idx > 5) {
			idx = 5;
		}
		if (g_region_channels[idx][c] == channel) {
			return 1;
		}
	}
	return 0;
}

/* 0x25e8 */
static u32 wlanChannelToFreq(u32 channel)
{
	return g_freq_table[channel];
}

/* 0x2600 */
static u32 wlanFreqToChannel(u32 freq)
{
	for (u32 ch = 1; ch < 15; ch++) {
		if (g_freq_table[ch] == freq) {
			return ch;
		}
	}
	return 15;
}

/* 0x2634: sceWlanDrv_lib_40B0AA4A / sceWlanDrv_driver_A082E6A2 */
int sceWlanDrv_driver_A082E6A2(void)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	pspSetK1(k1);
	return 0x80000003;
}

/* 0x264c: sceWlanDevSetGPIO / sceWlanDrv_driver_C4BCD411 */
int sceWlanDrv_driver_C4BCD411(void)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	pspSetK1(k1);
	return 0x80000003;
}

/* 0x2664: sceWlanDevGetStateGPIO / sceWlanDrv_driver_67645D5F */
int sceWlanDrv_driver_67645D5F(void)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	pspSetK1(k1);
	return 0x80000003;
}

/* 0x267c: sceWlanDrv_lib_8D5F551B / sceWlanDrv_driver_561FD176 */
int sceWlanDrv_driver_561FD176(u32 ver)
{
	SceWlanCtx *ctx = g_wlan_ctx;
	if ((ver & 0xff000000u) != 0) {
		return 0x80410d83;
	}
	ctx->version_or_mode = ver;
	return 0;
}

/* 0x26a8: sceWlanDrv_driver_90E5530F */
void sceWlanDrv_driver_90E5530F(void)
{
}

/* 0x26b0: sceWlanDrv_driver_7DD3787C */
int sceWlanDrv_driver_7DD3787C(void)
{
	u32 flags = g_wlan_ctx->flags;
	if (flags & 0x800) {
		return 3;
	}
	if (flags & 8) {
		return 2;
	}
	return (flags & 1) ? 4 : 1;
}

/* 0x26ec: sceWlanDrv_driver_1AE1B369 */
int sceWlanDrv_driver_1AE1B369(SceUID cb)
{
	u32 k1 = pspGetK1();
	SceWlanCtx *ctx = g_wlan_ctx;
	int ret = 0;

	if (cb <= 0) {
		ret = 0x80410d13;
	} else if (ctx->callback_id <= 0) {
		ctx->callback_id = cb;
	} else {
		ret = 0x80410d88;
	}

	pspSetK1(k1);
	return ret;
}

/* 0x272c: sceWlanDrv_driver_4ADEA1CF */
int sceWlanDrv_driver_4ADEA1CF(void)
{
	SceWlanCtx *ctx = g_wlan_ctx;
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	pspSetK1(k1);
	ctx->callback_id = 0;
	return 0;
}

/* 0x27e4 */
static void wlanIfTx(void *ifp)
{
	SceWlanCtx *ctx = (SceWlanCtx *)sceNetIfhandle_driver_D5AD6DEA(ifp);
	int intr = sceKernelCpuSuspendIntr();
	SceWlanMbxMsg *m = &g_mbx_msg_7a4c;
	m->type = 0x8000000d;
	if (!(m->flags & 1)) {
		m->flags = 3;
		sceKernelSendMbx(ctx->mbx_id, m);
	}
	sceKernelCpuResumeIntr(intr);
}

/* 0x285c */
static int wlanKermitIntr7(void)
{
	SceWlanCtx *ctx = g_wlan_ctx;
	if (ctx != NULL) {
		int intr = sceKernelCpuSuspendIntr();
		SceWlanMbxMsg *m = &g_mbx_msg_7a94;
		m->type = 0x80000001;
		if (!(m->flags & 1)) {
			m->flags = 3;
			sceKernelSendMbx(ctx->mbx_id, m);
		}
		sceKernelCpuResumeIntr(intr);
	}
	return 0;
}

/* 0x28dc */
static int wlanKermitIntr8(void)
{
	SceWlanCtx *ctx = g_wlan_ctx;
	if (ctx != NULL) {
		int intr = sceKernelCpuSuspendIntr();
		SceWlanMbxMsg *m = &g_mbx_msg_7adc;
		m->type = 0x80000013;
		if (!(m->flags & 1)) {
			m->flags = 3;
			sceKernelSendMbx(ctx->mbx_id, m);
		}
		sceKernelCpuResumeIntr(intr);
	}
	return 0;
}

/* 0x3064 */
static void wlanCopyMbufToBuf(SceNetMbuf *m, s32 offset, s32 len, u8 *dst)
{
	if (dst == NULL) {
		return;
	}
	while (offset > 0) {
		if ((s32)m < 0) {
			return;
		}
		if (offset < m->len) {
			break;
		}
		offset -= m->len;
		m = m->next;
	}
	while (len > 0) {
		if ((s32)m < 0) {
			return;
		}
		s32 avail = m->len - offset;
		s32 chunk = (avail < len) ? avail : len;
		u8 *src = m->data + offset;
		if ((s32)src < 0 || (s32)(src + chunk) < 0) {
			return;
		}
		memcpy(dst, src, chunk);
		dst += chunk;
		len -= chunk;
		m = m->next;
		offset = 0;
	}
}

/* 0x295c */
static void wlanPumpTx(SceWlanCtx *ctx)
{
	u32 flags = ctx->flags;
	if (!(flags & 0x200) || !(flags & 9) || (flags & 0x100)) {
		return;
	}
	ctx->flags = flags | 0x100;

	int count = 0;
	SceWlanMbxMsg *msg = NULL;

	for (;;) {
		SceNetMbuf *m = sceNetIfhandle_driver_E2F4F1C9(&ctx->netif);
		if (m == NULL) {
			break;
		}
		if ((m->flags & 2) && (u32)m->total_len < 0x605) {
			SceWlanTxNode *node = wlanAllocTxNode(ctx, 1);
			if (node == NULL) {
				sceNetIfhandle_driver_16246B99(&ctx->netif, m);
				break;
			}
			if (count == 0) {
				msg = (SceWlanMbxMsg *)wlanTryAlloc(ctx, sizeof(SceWlanMbxMsg));
				if (msg == NULL) {
					int intr = sceKernelCpuSuspendIntr();
					ctx->tx_cur = node;
					node->busy = 0;
					sceKernelCpuResumeIntr(intr);
					sceNetIfhandle_driver_16246B99(&ctx->netif, m);
					break;
				}
				msg->type = 0x80000002;
				msg->flags = 0;
			}
			node->len = m->total_len;
			wlanCopyMbufToBuf(m, 0, m->total_len, node->buf);
			wlanQueuePushBack(&ctx->tx_queue, node);
			count++;
		}
		sceNetIfhandle_driver_F56FAC82(m, 0);
	}

	if (count != 0) {
		sceKernelSendMbx(ctx->mbx_id, msg);
	} else if (wlanQueueIsNotEmpty(&ctx->tx_queue)) {
		msg = (SceWlanMbxMsg *)wlanTryAlloc(ctx, sizeof(SceWlanMbxMsg));
		if (msg != NULL) {
			msg->type = 0x80000002;
			msg->flags = 0;
			sceKernelSendMbx(ctx->mbx_id, msg);
		}
	} else {
		ctx->flags &= ~0x100u;
	}
}

/* 0x2b24 */
static void wlanDeliverRx(void *ifp, void *buf, u32 len, s16 rssi_a, s16 rssi_b)
{
	if (*(u32 *)ifp == 0) {
		return;
	}
	SceWlanCtx *ctx = (SceWlanCtx *)sceNetIfhandle_driver_D5AD6DEA(ifp);
	if (!(ctx->flags & 0x200) || !(ctx->flags & 9)) {
		return;
	}

	SceNetMbuf *m = sceNetIfhandle_driver_15CFE3C0(0x100);
	if (m == NULL) {
		return;
	}
	u32 pad = 2;
	m->unk_10 = 1;
	m->data = (u8 *)m + 0x30;
	m->next = NULL;
	m->unk_04 = 0;
	m->flags = 2;
	m->unk_1c = 0;
	m->total_len = len;

	if (len >= 14) {
		const u8 *b = (const u8 *)buf;
		if (b[12] == 0x88 && b[13] == 0xc8) {
			pad = 0;
		}
	}

	if (0xd0 - pad < len) {
		void *ext = sceNetIfhandle_driver_15CFE3C0(0x800);
		if (ext == NULL) {
			ext = sceNetIfhandle_driver_15CFE3C0(0x800);
		}
		m->ext_buf = ext;
		if (ext != NULL) {
			m->data = (u8 *)ext;
			m->flags |= 9;
			m->ext_size = 0x800;
			m->ext_34 = 0;
			m->ext_38 = 0;
			m->ext_ref2 = m;
			m->ext_ref1 = m;
		}
		if (!(m->flags & 1)) {
			sceNetIfhandle_driver_F8825DC4(m);
			return;
		}
	}

	m->len = len;
	u8 *dst = m->data + pad;
	if ((s32)dst < 0) {
		return;
	}
	m->data = dst;
	memcpy(dst, buf, len);
	m->rssi_a = rssi_a;
	m->rssi_b = rssi_b;
	sceNetIfhandle_driver_C28F6FF2(ifp, m);
}

/* 0x2cd4 */
static u16 wlanGetChannelMaskFromIdStorage(void)
{
	u16 val = 0;
	u16 res = 0x10;
	if (sceIdStorageLookup(0x45, 0, &val, 2) == 0) {
		u16 code = val & 0xfff;
		if (code == 1) {
			res = 0x10;
		} else if (code < 2) {
			if (code == 0) {
				res = 0x20;
			}
		} else if (code == 2) {
			res = 0x30;
		} else if (code == 3) {
			res = 0x40;
		}
	}
	return (u16)(res << 8);
}

/* 0x2d4c */
static int wlanResumeOrWakeup(SceWlanCtx *ctx, int is_suspend)
{
	if (((ctx->flags >> 14) & 1) && is_suspend == 0) {
		u32 pkt[11];
		sceKernelMemcpy(&pkt[3], g_wakeup_cfg + 6, 0x20);
		pkt[0] = 0;
		int end_idx = 0;
		int has_star = 0;
		for (int i = 0; i < 32; i++) {
			u8 ch = g_wakeup_cfg[6 + i];
			if (ch == 0) {
				end_idx = i;
			}
			if (ch == '*') {
				has_star = 1;
			}
		}
		pkt[0] = has_star;
		pkt[2] = end_idx;
		pkt[1] = g_wakeup_cfg[5];

		if (!(ctx->flags & 1)) {
			ctx->flags &= ~1u;
			wlanNotifyCallback(ctx, 0);
		}
		if (ctx->flags & 0x20000) {
			if (ctx->flags & 0x80) {
				scePower_driver_8C6BEFD9();
				ctx->flags &= ~0x80u;
			}
			wlanSetupMacAndChannel(ctx);
			ctx->flags &= ~0x20000u;
		}
		if (ctx->flags & 8) {
			ctx->flags &= ~8u;
		}
		wlanKermitSetWakeup(pkt);
	} else {
		wlanKermitTerm(0);
	}

	if (ctx->flags & 9) {
		wlanFlushQueues(ctx);
		memset(ctx->conn_info, 0, sizeof(ctx->conn_info));
		ctx->unk_15c = 0;
		ctx->flags &= ~9u;
		ctx->unk_15e = 0;
		wlanNotifyCallback(ctx, 0);
	}
	ctx->unk_0e0 = 0;
	memset(ctx->unk_0e4, 0, sizeof(ctx->unk_0e4));
	return 0;
}

/* 0x2f14 */
static SceUInt wlanAlarmHandler(void *common)
{
	SceWlanCtx *ctx = (SceWlanCtx *)common;
	ctx->alarm_id = 0;
	int intr = sceKernelCpuSuspendIntr();
	SceWlanMbxMsg *m = &g_mbx_msg_7a28;
	m->type = 0x80000007;
	if (!(m->flags & 1)) {
		m->flags = 3;
		sceKernelSendMbx(ctx->mbx_id, m);
	}
	sceKernelCpuResumeIntr(intr);
	if (ctx->worker_thid > 0) {
		sceKernelTerminateThread(ctx->worker_thid);
	}
	return 0;
}

/* 0x2fa4 */
static SceUInt wlanSuspendAlarmHandler(void *common)
{
	SceWlanCtx *ctx = g_wlan_ctx;
	int intr = sceKernelCpuSuspendIntr();
	s32 arg_val = *(s32 *)common;
	ctx->flags |= 0x2000;
	sceKernelCpuResumeIntr(intr);

	if (ctx->mac_thid > 0) {
		sceKernelChangeThreadPriority(ctx->mac_thid, 0x10);
	}

	intr = sceKernelCpuSuspendIntr();
	SceWlanMbxMsg *m = &g_mbx_msg_7a70;
	m->type = 0x8000000e;
	if (!(m->flags & 1)) {
		m->flags = 3;
		m->arg_val = (u32)arg_val;
		sceKernelSendMbx(ctx->mbx_id, m);
	}
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* 0x36f0 */
static void wlanExportBssInfo(void *dst_ptr, const void *src_ptr, u32 ver)
{
	u8 *dst = (u8 *)dst_ptr;
	const u8 *src = (const u8 *)src_ptr;
	memset(dst, 0, (ver > 0x1ffff) ? 0x5c : 0x50);

	*(u32 *)(dst + 0x00) = *(const u32 *)(src + 0x04);
	*(u16 *)(dst + 0x04) = *(const u16 *)(src + 0x08);
	u8 ssid_len = src[0x0a];
	dst[0x07] = ssid_len;
	if (ssid_len != 0) {
		if (ssid_len >= 0x21) {
			ssid_len = 0x20;
		}
		dst[0x07] = ssid_len;
		memcpy(dst + 0x08, src + 0x0c, ssid_len);
	}

	*(u32 *)(dst + 0x28) = (*(const u32 *)(src + 0x5c) == 0) ? 2 : 1;
	*(u16 *)(dst + 0x3e) = src[0x30];
	*(u32 *)(dst + 0x2c) = *(const u32 *)(src + 0x3c);
	*(u32 *)(dst + 0x30) = src[0x32];
	dst[0x06] = (u8)wlanFreqToChannel(*(const u32 *)(src + 0x4c));
	*(u16 *)(dst + 0x3c) = *(const u16 *)(src + 0x40);

	for (int i = 0; i < 8; i++) {
		u8 r = src[0x61 + i * 2];
		u8 basic = (src[0x60 + i * 2] == 0) ? 0x80 : 0;
		dst[0x40 + i] = r | basic;
	}

	dst[0x4a] = src[0x80] & 7;
	dst[0x4b] = src[0x81];
	dst[0x4c] = src[0x82];
	dst[0x4d] = src[0x83];
	dst[0x4e] = src[0x84];
	*(s16 *)(dst + 0x48) = (s8)src[0x2c];

	if (ver <= 0x1ffff) {
		return;
	}

	if (src[0x88] != 0) {
		dst[0x50] = 1;
		u8 b89 = src[0x89];
		if (b89 & 1) dst[0x51] |= 1;
		if (b89 & 2) dst[0x51] |= 2;
		if (b89 & 4) dst[0x51] |= 4;
		if (b89 & 8) dst[0x51] |= 8;
		if ((s8)b89 < 0) dst[0x51] |= 0x80;

		u8 b8a = src[0x8a];
		if (b8a & 4) dst[0x52] |= 4;
		if (b8a & 8) dst[0x52] |= 8;
		if ((s8)b8a < 0) dst[0x52] |= 0x80;

		u8 b8b = src[0x8b];
		if (b8b & 1) dst[0x53] |= 1;
		if (b8b & 2) dst[0x53] |= 2;
		if ((s8)b8b < 0) dst[0x53] |= 0x80;
	}

	if (src[0x8e] != 0) {
		dst[0x56] = 1;
		u8 b8f = src[0x8f];
		if (b8f & 1) dst[0x57] |= 1;
		if (b8f & 2) dst[0x57] |= 2;
		if (b8f & 4) dst[0x57] |= 4;
		if (b8f & 8) dst[0x57] |= 8;
		if ((s8)b8f < 0) dst[0x57] |= 0x80;

		u8 b90 = src[0x90];
		if (b90 & 4) dst[0x58] |= 4;
		if (b90 & 8) dst[0x58] |= 8;
		if ((s8)b90 < 0) dst[0x58] |= 0x80;

		u8 b91 = src[0x91];
		if (b91 & 1) dst[0x59] |= 1;
		if (b91 & 2) dst[0x59] |= 2;
		if ((s8)b91 < 0) dst[0x59] |= 0x80;
	}
}

/* 0x3afc */
static void wlanImportBssInfo(void *dst_ptr, const void *src_ptr, u32 ver)
{
	u8 *dst = (u8 *)dst_ptr;
	const u8 *src = (const u8 *)src_ptr;

	*(u32 *)(dst + 0x04) = *(const u32 *)(src + 0x00);
	*(u16 *)(dst + 0x08) = *(const u16 *)(src + 0x04);
	*(u32 *)(dst + 0x0c) = *(const u32 *)(src + 0x08);
	*(u32 *)(dst + 0x10) = *(const u32 *)(src + 0x0c);
	*(u32 *)(dst + 0x14) = *(const u32 *)(src + 0x10);
	*(u32 *)(dst + 0x18) = *(const u32 *)(src + 0x14);
	*(u32 *)(dst + 0x1c) = *(const u32 *)(src + 0x18);
	*(u32 *)(dst + 0x20) = *(const u32 *)(src + 0x1c);
	*(u32 *)(dst + 0x24) = *(const u32 *)(src + 0x20);
	*(u32 *)(dst + 0x28) = *(const u32 *)(src + 0x24);

	u8 ssid_len = src[0x07];
	if (ssid_len >= 0x21) {
		ssid_len = 0x20;
	}
	*(u16 *)(dst + 0x0a) = ssid_len;
	*(u32 *)(dst + 0x5c) = (*(const u32 *)(src + 0x28) == 2) ? 0 : 1;

	u32 w30 = *(u32 *)(dst + 0x30);
	u16 s3e = *(const u16 *)(src + 0x3e);
	w30 = (w30 & ~0xffu) | (s3e & 0xffu);
	*(u32 *)(dst + 0x30) = w30;

	*(u32 *)(dst + 0x3c) = *(const u32 *)(src + 0x2c);
	dst[0x32] = src[0x30];
	*(u32 *)(dst + 0x4c) = wlanChannelToFreq(src[0x06]);
	*(u32 *)(dst + 0x40) = *(const u16 *)(src + 0x3c);

	for (int i = 0; i < 8; i++) {
		u8 r = src[0x40 + i];
		dst[0x60 + i * 2] = (r & 0x80) ? 0 : 1;
		dst[0x61 + i * 2] = r & 0x7f;
	}

	*(s32 *)(dst + 0x2c) = *(const s16 *)(src + 0x48);
	if (ver <= 0x1ffff) {
		return;
	}

	if (src[0x50] == 1) {
		dst[0x88] = 1;
		u8 s51 = src[0x51];
		if (s51 & 1) dst[0x89] |= 1;
		if (s51 & 2) dst[0x89] |= 2;
		if (s51 & 4) dst[0x89] |= 4;
		if (s51 & 8) dst[0x89] |= 8;
		if ((s8)s51 < 0) dst[0x89] |= 0x80;

		u8 s52 = src[0x52];
		if (s52 & 4) dst[0x8a] |= 4;
		if (s52 & 8) dst[0x8a] |= 8;
		if ((s8)s52 < 0) dst[0x8a] |= 0x80;

		u8 s53 = src[0x53];
		if (s53 & 1) dst[0x8b] |= 1;
		if (s53 & 2) dst[0x8b] |= 2;
		if ((s8)s53 < 0) dst[0x8b] |= 0x80;
	}

	if (src[0x56] == 1) {
		dst[0x8e] = 1;
		u8 s57 = src[0x57];
		if (s57 & 1) dst[0x8f] |= 1;
		if (s57 & 2) dst[0x8f] |= 2;
		if (s57 & 4) dst[0x8f] |= 4;
		if (s57 & 8) dst[0x8f] |= 8;
		if ((s8)s57 < 0) dst[0x8f] |= 0x80;

		u8 s58 = src[0x58];
		if (s58 & 4) dst[0x90] |= 4;
		if (s58 & 8) dst[0x90] |= 8;
		if ((s8)s58 < 0) dst[0x90] |= 0x80;

		u8 s59 = src[0x59];
		if (s59 & 1) dst[0x91] |= 1;
		if (s59 & 2) dst[0x91] |= 2;
		if ((s8)s59 < 0) dst[0x91] |= 0x80;
	}
}

/* 0x3f58 */
static int wlanJoinIbss(SceWlanCtx *ctx, void *arg2, void *bss_info)
{
	ctx->flags |= 4;
	int ret = wlanKermitJoin(bss_info);
	if (ret == 0) {
		ctx->flags |= 8;
		wlanNotifyCallback(ctx, ((const u8 *)bss_info)[6]);
	} else {
		s32 code = -1;
		if (ret == (int)0x80410d04) {
			code = -0xc9;
		} else if (ret == (int)0x80410d11) {
			code = -0xca;
		}
		((s32 *)arg2)[5] = code;
		ret = 0x80410d0b;
	}
	ctx->flags &= ~4u;
	return ret;
}

/* 0x3128 */
static int wlanHandleIoctlCmd(SceWlanCtx *ctx, u32 cmd, void *arg2, u32 *params)
{
	int err = 0;
	wlanLock(ctx);

	if (cmd == 0x10a) {
		goto done_ok;
	}
	if (cmd < 0x10b) {
		if (cmd < 0x0a) {
			if (cmd < 7) {
				if (cmd == 3) {
					void *bss = (void *)params[0];
					if (ctx->flags & 2) {
						err = 0x80410d03;
						goto done_err;
					}
					if (ctx->flags & 4) {
						err = 0x80410d02;
						goto done_err;
					}
					u16 reg = ctx->channel_mask >> 8;
					if (!wlanIsChannelValid(&reg, ((const u8 *)bss)[6]) || ((const u8 *)bss)[7] >= 0x21) {
						err = 0x80410d04;
						goto done_err;
					}
					if (*(const u32 *)((const u8 *)bss + 0x28) != 1) {
						if (ctx->unk_0c4 == 1) {
							ctx->unk_0c4 = 0;
						}
						err = wlanJoinIbss(ctx, arg2, bss);
						if (err != 0) {
							goto check_err;
						}
					}
					memset(ctx->conn_info, 0, sizeof(ctx->conn_info));
					wlanImportBssInfo(ctx->conn_info, bss, ctx->version_or_mode);
					goto check_err;
				}
				if (cmd < 4) {
					if (cmd == 2) {
						u32 out_buf = params[1];
						u32 orig_len = ((u32 *)arg2)[5];
						u32 scan_req = params[0];
						ctx->flags |= 2;
						for (int tries = 2; tries >= 0; tries--) {
							((u32 *)arg2)[5] = orig_len;
							err = wlanDoScan((void *)scan_req, &((u32 *)arg2)[5], (void *)out_buf, ctx->version_or_mode > 0x1ffff);
							if (err == 0 && ((s32 *)arg2)[5] > 0) {
								break;
							}
						}
						ctx->flags &= ~2u;
						goto check_err;
					}
					goto done_ok;
				}
				if (cmd == 5) {
					void *bss = (void *)params[0];
					if (ctx->flags & 2) {
						err = 0x80410d03;
						goto done_err;
					}
					if (ctx->flags & 4) {
						err = 0x80410d02;
						goto done_err;
					}
					u16 reg = ctx->channel_mask >> 8;
					if (!wlanIsChannelValid(&reg, ((const u8 *)bss)[6]) || ((const u8 *)bss)[7] >= 0x21 || *(const u32 *)((const u8 *)bss + 0x28) != 2) {
						err = 0x80410d04;
						goto done_err;
					}
					if (ctx->unk_0c4 == 1) {
						ctx->unk_0c4 = 0;
					}
					err = wlanKermitCreateIbss(bss);
					if (err == 0) {
						ctx->flags |= 8;
						wlanNotifyCallback(ctx, ((const u8 *)bss)[6]);
					}
					ctx->flags &= ~4u;
					if (err != 0) {
						err = 0x80410d04;
						goto done_err;
					}
					memset(ctx->conn_info, 0, sizeof(ctx->conn_info));
					wlanImportBssInfo(ctx->conn_info, bss, ctx->version_or_mode);
					goto check_err;
				}
				if (cmd == 4) {
					if (ctx->flags & 9) {
						ctx->flags |= 0x400;
						if (ctx->unk_0c4 != 1) {
							int r = wlanKermitDisconnect();
							if (r == 0) {
								ctx->flags &= ~8u;
								wlanNotifyCallback(ctx, 0);
							} else {
								((s32 *)arg2)[4] = (r == (int)0x80410d11) ? -0x12d : -1;
								err = 0x80410d15;
							}
						}
					}
					wlanFlushQueues(ctx);
					memset(ctx->conn_info, 0, sizeof(ctx->conn_info));
					wlanResetRssiList(ctx);
					ctx->unk_15c = 0;
					ctx->flags &= ~0x400u;
					ctx->unk_15e = 0;
					goto check_err;
				}
				/* cmd == 6 */
				u32 pm = *(u32 *)arg2;
				ctx->power_mode = pm;
				if ((pm & 3) == 3) {
					ctx->flags |= 0x80;
				} else {
					ctx->flags &= ~0x80u;
				}
				goto done_ok;
			}
			goto done_ok;
		}
		if (cmd == 0x0a) {
			/* copy BSSID from conn_info + 4 (i.e. ctx + 0x0c) */
			goto done_ok;
		}
		if (cmd == 0x101) {
			u32 v = *(u32 *)arg2;
			if (v == 1 || v == 0 || v == 2) {
				err = -1;
				goto done_err;
			}
			if (v != 3) {
				err = 0x80410d04;
				goto check_err;
			}
			err = -1;
			goto done_err;
		}
		goto done_ok;
	}

	if (cmd == 0x10c) {
		wlanExportBssInfo((void *)params[0], ctx->conn_info, ctx->version_or_mode);
		goto done_ok;
	}
	if (cmd == 0x400) {
		for (int i = 0; i < 3; i++) {
			int r = wlanKermitDisconnect();
			if (r == 0) {
				ctx->flags &= ~8u;
				wlanNotifyCallback(ctx, 0);
				if (i == 2) {
					goto done_ok;
				}
			}
		}
		goto check_err;
	}

done_ok:
	{
		int intr = sceKernelCpuSuspendIntr();
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_F94BAF52(&ctx->netif, 3, 0);
		}
		sceKernelCpuResumeIntr(intr);
	}
	wlanUnlock(ctx);
	return err;

check_err:
	if (err >= 0) {
		goto done_ok;
	}
done_err:
	{
		int intr = sceKernelCpuSuspendIntr();
		if (ctx->netif.if_internal != NULL) {
			sceNetIfhandle_driver_F94BAF52(&ctx->netif, 4, err);
		}
		sceKernelCpuResumeIntr(intr);
	}
	wlanUnlock(ctx);
	return err;
}

/* 0x4010 */
static void wlanQueueInit(SceWlanQueue *q)
{
	q->tail = NULL;
	q->head = NULL;
}

/* 0x4020 */
static void wlanQueueDestroy(SceWlanQueue *q)
{
	(void)q;
}

/* 0x4028 */
static int wlanQueuePushBack(SceWlanQueue *q, SceWlanTxNode *node)
{
	int intr = sceKernelCpuSuspendIntr();
	node->queue_next = NULL;
	if (q->tail != NULL) {
		q->tail->queue_next = node;
	} else {
		q->head = node;
	}
	q->tail = node;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* 0x407c */
static int wlanQueuePushFront(SceWlanQueue *q, SceWlanTxNode *node)
{
	int intr = sceKernelCpuSuspendIntr();
	node->queue_next = q->head;
	if (q->tail == NULL) {
		q->tail = node;
	}
	q->head = node;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* 0x40d0 */
static SceWlanTxNode *wlanQueuePop(SceWlanQueue *q)
{
	int intr = sceKernelCpuSuspendIntr();
	SceWlanTxNode *node = q->head;
	if (node != NULL) {
		SceWlanTxNode *next = node->queue_next;
		q->head = next;
		if (next == NULL) {
			q->tail = NULL;
		}
		node->queue_next = NULL;
	}
	sceKernelCpuResumeIntr(intr);
	return node;
}

/* 0x4128 */
static int wlanQueueIsNotEmpty(SceWlanQueue *q)
{
	return q->head != NULL;
}

/* 0x4134 */
static SceUID wlanCreateHeap(u32 size)
{
	return sceKernelCreateVpl("SceWlanDrvHeap", 1, 1, size, NULL);
}

/* 0x4164 */
static int wlanDeleteHeap(SceUID uid)
{
	return sceKernelDeleteVpl(uid);
}

/* 0x4180 */
static void *wlanAlloc(SceWlanCtx *ctx, u32 size)
{
	void *ptr = NULL;
	int ret = sceKernelAllocateVpl(ctx->vpl_id, size, &ptr, NULL);
	return (ret < 0) ? NULL : ptr;
}

/* 0x41b4 */
static void *wlanTryAlloc(SceWlanCtx *ctx, u32 size)
{
	void *ptr = NULL;
	int ret = sceKernelTryAllocateVpl(ctx->vpl_id, size, &ptr);
	return (ret < 0) ? NULL : ptr;
}

/* 0x41e4 */
static int wlanFree(SceWlanCtx *ctx, void *ptr)
{
	return sceKernelFreeVpl(ctx->vpl_id, ptr);
}

/* 0x4200: sceWlanDrv_driver_2519EAA7 */
int sceWlanDrv_driver_2519EAA7(void)
{
	if (g_drop_rate == 0) {
		return 0;
	}

	u32 now = sceKernelGetSystemTimeLow() / 1000000u;
	int intr = sceKernelCpuSuspendIntr();
	int drop = 0;

	if (g_drop_active == 1) {
		if (now < g_drop_end_time) {
			sceKernelCpuResumeIntr(intr);
			return 1;
		}
		g_drop_end_time = 0;
		g_drop_active = 0;
	}

	if (now >= g_drop_next_time) {
		int idx = g_mt_index + 1;
		g_mt_index = idx;
		if (idx >= 521) {
			for (int i = 0; i < 32; i++) {
				g_mt_state[i] ^= g_mt_state[i + 489];
			}
			for (int i = 32; i < 521; i++) {
				g_mt_state[i] ^= g_mt_state[i - 32];
			}
			g_mt_index = 0;
		}
		u32 r = g_mt_state[g_mt_index];
		u32 pct = (u32)(((u64)r * 0xc7fffffdULL) >> 57);
		if (pct < g_drop_rate) {
			g_drop_active = 1;
			g_drop_end_time = now + g_drop_duration;
			drop = 1;
		}
		g_drop_next_time = now + g_drop_duration;
	}

	sceKernelCpuResumeIntr(intr);
	return drop;
}

/* 0x4388: sceWlanDrv_driver_B6A9700D */
int sceWlanDrv_driver_B6A9700D(u32 *out_rate, u32 *out_duration)
{
	if (out_rate == NULL || out_duration == NULL) {
		return 0x80410003;
	}
	*out_rate = g_drop_rate;
	*out_duration = g_drop_duration;
	return 0;
}

/* 0x43c4: sceWlanDrv_driver_325F7172 */
int sceWlanDrv_driver_325F7172(u32 rate, u32 duration)
{
	if (rate >= 101) {
		return 0x80410003;
	}
	if (!g_prng_inited) {
		g_prng_inited = 1;
		wlanInitPrng(sceKernelGetSystemTimeLow());
	}
	int intr = sceKernelCpuSuspendIntr();
	g_drop_rate = rate;
	g_drop_duration = duration;
	g_drop_end_time = 0;
	g_drop_next_time = 0;
	g_drop_active = 0;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* 0x4468 */
static void wlanInitPrng(u32 seed)
{
	u32 s = seed;
	for (int i = 0; i < 17; i++) {
		u32 w = 0;
		for (int b = 0; b < 32; b++) {
			s = s * 0x5d588b65u + 1;
			w = (w >> 1) | (s & 0x80000000u);
		}
		g_mt_state[i] = w;
	}
	g_mt_state[16] = (g_mt_state[16] << 23) ^ (g_mt_state[0] >> 9) ^ g_mt_state[15];
	for (int i = 17; i < 521; i++) {
		g_mt_state[i] = (g_mt_state[i - 17] << 23) ^ (g_mt_state[i - 16] >> 9) ^ g_mt_state[i - 1];
	}
	for (int pass = 0; pass < 4; pass++) {
		for (int i = 0; i < 32; i++) {
			g_mt_state[i] ^= g_mt_state[i + 489];
		}
		for (int i = 32; i < 521; i++) {
			g_mt_state[i] ^= g_mt_state[i - 32];
		}
	}
	g_mt_index = 520;
}

/* 0x4624 */
static int wlanDoScan(void *scan_req, u32 *inout_len, void *out_buf, int compact)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	u32 half_size = g_kermit_net_buf_size >> 1;
	u8 *kbuf = g_kermit_net_buf;
	int ret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (ret != 0) {
		return ret;
	}

	u8 *kout = kbuf + half_size;
	sceKernelMemcpy(kbuf, scan_req, 0x4c);
	sceKermitMemory_driver_AAF047AC(req, 0, kbuf, half_size, 1);
	sceKermitMemory_driver_AAF047AC(req, 1, kout, half_size, 2);
	req->args[2] = *inout_len;

	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x4c, 3, 0, &resp);
	s32 res = (kret != 0) ? kret : (s32)resp;

	if (kret == 0 && res > 0) {
		*inout_len = 0;
		if (!compact) {
			*inout_len = (u32)(res * 0x54);
			u8 *dst = (u8 *)out_buf;
			u8 *src = kout;
			for (s32 i = 0; i < res - 1; i++) {
				sceKernelMemcpy(dst, src, 0x54);
				*(u32 *)dst = (u32)(dst + 0x54);
				dst += 0x54;
				src += 0x60;
			}
			sceKernelMemcpy(dst, src, 0x54);
			*(u32 *)dst = 0;
		} else {
			u32 total = (u32)(res * 0x60);
			*inout_len = total;
			sceKernelMemcpy(out_buf, kout, total);
			u8 *dst = (u8 *)out_buf;
			for (s32 i = 0; i < res - 1; i++) {
				*(u32 *)dst = (u32)(dst + 0x60);
				dst += 0x60;
			}
			*(u32 *)dst = 0;
		}
		res = 0;
	}

	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : res;
}

/* 0x4830: sceWlanDrv_lib_19602A86 */
int sceWlanDrv_lib_19602A86(u32 a0, u32 a1, u32 a2, s32 *out_val)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);

	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	if ((s32)(pspGetK1() & (u32)out_val) < 0) {
		pspSetK1(k1);
		return 0x80410d13;
	}
	if (g_scan_ready == 0) {
		*out_val = -1;
		pspSetK1(k1);
		return 0;
	}

	int ret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (ret != 0) {
		pspSetK1(k1);
		return ret;
	}

	u8 *kbuf = g_kermit_net_buf;
	req->args[0] = a0;
	req->args[1] = a1;
	req->args[2] = a2;
	req->args[3] = 4;
	sceKermitMemory_driver_AAF047AC(req, 4, kbuf, 4, 2);

	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x40, 5, 0, &resp);
	int res = (kret != 0) ? kret : (int)resp;
	if (kret == 0 && res == 0) {
		*out_val = *(s32 *)kbuf;
	}

	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	pspSetK1(k1);
	return (uret != 0) ? uret : res;
}

/* 0x49a8: sceWlanDrv_lib_9506F322 */
int sceWlanDrv_lib_9506F322(u32 id, void *buf, s32 len, u32 flags, void *hdr_buf, u32 *hdr_len, s32 *out_len)
{
	s32 max_len = (g_max_send_size != 0) ? (s32)g_max_send_size : len;
	if (max_len > 0x7530) {
		max_len = 0x7530;
	}

	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	pspSetK1(k1_shifted);

	if (max_len < 0 ||
	    (s32)((((u32)buf + (u32)max_len) | (u32)buf | (u32)max_len) & k1_shifted) < 0 ||
	    (s32)((u32)out_len & k1_shifted) < 0 ||
	    (s32)((u32)hdr_len & k1_shifted) < 0 ||
	    (s32)((((u32)hdr_buf + 0x10) | (u32)hdr_buf) & k1_shifted) < 0) {
		pspSetK1(k1);
		return 0x80410d13;
	}

	u8 *kbuf = g_kermit_net_buf;
	int ret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (ret != 0) {
		pspSetK1(k1);
		return ret;
	}

	int res = wlanKermitSendGameMode(0x39, kbuf, id, (u32)max_len, flags, hdr_buf, hdr_len, (u32 *)out_len);
	if (res == 0) {
		if (hdr_buf != NULL && hdr_len != NULL) {
			u32 hlen = *(u32 *)kbuf;
			if (hlen >= 0x11) {
				hlen = 0x10;
			}
			if (hlen != 0) {
				sceKernelMemcpy(hdr_buf, kbuf + 8, hlen);
			}
			*hdr_len = hlen;
		}
		if (out_len != NULL) {
			s32 rlen = *(s32 *)(kbuf + 0x104);
			if (rlen > 0 && (s32)((((u32)buf + (u32)max_len) | (u32)buf | (u32)max_len) & k1_shifted) >= 0) {
				sceKernelMemcpy(buf, kbuf + 0x10c, rlen);
			}
			*out_len = rlen;
		}
		g_max_send_size = 0;
	}

	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	pspSetK1(k1);
	return (uret != 0) ? uret : res;
}

/* 0x4b90: sceWlanDrv_lib_C934D53F */
int sceWlanDrv_lib_C934D53F(u32 id, const void *buf, s32 len, u32 flags, const void *hdr_buf, u32 hdr_len, u32 *out_sent)
{
	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	pspSetK1(k1_shifted);

	if (len < 0 ||
	    (s32)((((u32)buf + (u32)len) | (u32)buf | (u32)len) & k1_shifted) < 0 ||
	    (s32)((u32)out_sent & k1_shifted) < 0 ||
	    ((s32)((((u32)hdr_buf + 0x10) | (u32)hdr_buf) & k1_shifted) < 0 && hdr_buf != NULL)) {
		pspSetK1(k1);
		return 0x80410d13;
	}

	if (hdr_len >= 0x11) {
		hdr_len = 0x10;
	}

	u32 total_sent = 0;
	u32 chunk_sent = 0;
	int res = 0;
	int retries = 0;
	const u8 *p = (const u8 *)buf;
	u32 nonblock = flags & 0x80;

	while (len > 0) {
		int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
		u8 *kbuf = g_kermit_net_buf;
		s32 chunk = (len < 0x7530) ? len : 0x7530;
		if (lret == 0) {
			sceKernelMemcpy(kbuf + 0x10c, p, chunk);
			u32 hlen = hdr_len;
			int kret = wlanKermitSendGameMode(0x36, kbuf, id, (u32)chunk, flags, (void *)hdr_buf, &hlen, &chunk_sent);
			int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
			res = (uret != 0) ? uret : kret;
		} else {
			res = lret;
		}
		if (res < 0) {
			pspSetK1(k1);
			return res;
		}
		total_sent += chunk_sent;
		p += chunk_sent;
		len -= (s32)chunk_sent;
		sceKernelDelayThread((u32)(chunk * 20));
		chunk_sent = total_sent;
		if (res != 0) {
			if (res != 0x0b || nonblock != 0 || retries >= 4) {
				break;
			}
			retries++;
			sceKernelDelayThread(500000);
			res = 0;
		} else {
			retries = 0;
		}
	}

	*out_sent = chunk_sent;
	pspSetK1(k1);
	return res;
}

typedef struct SceWlanIov {
	const void *base;
	u32 len;
} SceWlanIov;

/* 0x4dac: sceWlanDrv_lib_BFE46C6D */
int sceWlanDrv_lib_BFE46C6D(u32 id, const SceWlanIov *iov, u32 iov_cnt, u32 flags, const void *hdr_buf, u32 hdr_len, u32 *out_sent)
{
	u8 *kbuf = g_kermit_net_buf;
	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	pspSetK1(k1_shifted);

	if (iov_cnt > 0x2fffffu ||
	    (s32)((((u32)iov + (iov_cnt << 3)) | (u32)iov | (iov_cnt << 3)) & k1_shifted) < 0 ||
	    (s32)((u32)out_sent & k1_shifted) < 0 ||
	    iov_cnt >= 0xfdu ||
	    ((s32)((((u32)hdr_buf + 0x10) | (u32)hdr_buf) & k1_shifted) < 0 && hdr_buf != NULL)) {
		pspSetK1(k1);
		return 0x80410d13;
	}

	if (hdr_len >= 0x11) {
		hdr_len = 0x10;
	}

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		pspSetK1(k1);
		return lret;
	}

	int res = 0;
	u32 total_len = 0;
	u8 *dst = kbuf + 0x10c;
	for (u32 i = 0; i < iov_cnt; i++) {
		const void *base = iov[i].base;
		u32 len = iov[i].len;
		total_len += len;
		if (len >= 0x7531u || total_len >= 0x7531u) {
			res = 0x80410d12;
			goto unlock_out;
		}
		if ((s32)((((u32)base + len) | (u32)base | len) & k1_shifted) < 0) {
			res = 0x80410d13;
			goto unlock_out;
		}
		sceKernelMemcpy(dst, base, len);
		dst += len;
	}

	{
		u32 hlen = hdr_len;
		res = wlanKermitSendGameMode(0x36, kbuf, id, total_len, flags, (void *)hdr_buf, &hlen, out_sent);
	}

unlock_out:
	{
		int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
		pspSetK1(k1);
		return (uret != 0) ? uret : res;
	}
}

/* 0x4f9c: sceWlanDrv_lib_CAC1FB0D */
int sceWlanDrv_lib_CAC1FB0D(void *out_buf)
{
	u8 req1_buf[128] __attribute__((aligned(64)));
	u8 req2_buf[128] __attribute__((aligned(64)));
	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	pspSetK1(k1_shifted);

	if ((s32)((((u32)out_buf + 0x90) | (u32)out_buf) & k1_shifted) < 0) {
		pspSetK1(k1);
		return 0x80410d13;
	}

	sceKernelDcacheInvalidateRange(req1_buf, 64);
	SceKermitRequest *req1 = KERMIT_UNCACHED(req1_buf);
	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	int start_res = lret;
	if (lret == 0) {
		req1->args[0] = 1;
		u64 resp1 = 0;
		int kret = sceKermit_driver_36666181(req1, KERMIT_MODE_WLAN, 0x50, 1, 0, &resp1);
		int sres = (kret != 0) ? kret : (int)resp1;
		int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
		start_res = (uret != 0) ? uret : sres;
	}
	if (start_res < 0) {
		pspSetK1(k1);
		return 0x80410a08;
	}

	int poll_res;
	for (;;) {
		poll_res = wlanKermitPollScanState(g_scan_state_buf);
		if (poll_res == 4) {
			g_scan_ready = 1;
			poll_res = 0;
			break;
		}
		if (poll_res < 0) {
			break;
		}
		sceKernelDelayThread(250000);
	}

	sceKernelDcacheInvalidateRange(req2_buf, 64);
	SceKermitRequest *req2 = KERMIT_UNCACHED(req2_buf);
	if (sceKernelLockMutex(g_kermit_net_mutex, 1, NULL) == 0) {
		req2->args[0] = 0;
		u64 resp2 = 0;
		sceKermit_driver_36666181(req2, KERMIT_MODE_WLAN, 0x50, 1, 0, &resp2);
		sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	}

	sceKernelMemcpy(out_buf, g_scan_state_buf, 0x90);
	pspSetK1(k1);
	return poll_res;
}

/* 0x5184: sceWlanDrv_lib_51FF0D5F */
int sceWlanDrv_lib_51FF0D5F(u32 id, u32 cmd, void *buf, u32 size)
{
	if (((cmd >> 8) & 0xff) == 0x72) {
		return 0;
	}

	int valid = 0;
	if (cmd <= 0x802450ccu) {
		if (cmd >= 0x802450cbu ||
		    cmd == 0x80146985u ||
		    cmd == 0x800250cfu ||
		    cmd == 0x200050c8u ||
		    cmd == 0x80146972u ||
		    cmd == 0x80146983u ||
		    cmd == 0x80206919u ||
		    cmd == 0x801c69dbu ||
		    cmd == 0x8020690cu ||
		    cmd == 0x80206986u ||
		    cmd == 0x802450c9u) {
			valid = 1;
		}
	} else {
		if (cmd == 0xc014697bu ||
		    cmd == 0x8040691au ||
		    cmd == 0x8024697au ||
		    cmd == 0x81186979u ||
		    cmd == 0xc00250d0u ||
		    cmd == 0xc02450cau ||
		    cmd == 0xc014697du ||
		    cmd == 0xc0246978u ||
		    cmd == 0xc1146974u) {
			valid = 1;
		}
	}
	if (!valid) {
		return 0;
	}

	u8 req_buf[128] __attribute__((aligned(64)));
	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	sceKernelDcacheInvalidateRange(req_buf, 64);
	pspSetK1(k1_shifted);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);
	u8 *kbuf = g_kermit_net_buf;

	if (size != 0x24 || (s32)((((u32)buf + 0x24) | (u32)buf) & k1_shifted) < 0) {
		pspSetK1(k1);
		return 0x80410d13;
	}

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		pspSetK1(k1);
		return lret;
	}

	if (buf != NULL) {
		sceKernelMemcpy(kbuf, buf, 0x24);
	}
	req->args[0] = id;
	req->args[1] = cmd;
	sceKermitMemory_driver_AAF047AC(req, 2, kbuf, 0x24, 3);

	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x33, 3, 0, &resp);
	int res = (kret != 0) ? kret : (int)resp;
	if (kret == 0 && res == 0 && buf != NULL && (cmd & 0x40000000u) != 0) {
		sceKernelMemcpy(buf, kbuf, 0x24);
	}

	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	pspSetK1(k1);
	return (uret != 0) ? uret : res;
}

/* 0x545c */
static int wlanKermitGetSwitchAndPower(void)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	int res = lret;
	if (lret == 0) {
		u64 resp = 0;
		int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x49, 0, 0, &resp);
		int sres = (kret != 0) ? kret : (int)resp;
		int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
		res = (uret != 0) ? uret : sres;
	}
	if (res >= 0) {
		g_switch_and_power_state = (u32)res;
	}
	return res;
}

/* 0x5514 */
static int wlanKermitInit(void)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	g_kermit_net_buf = (u8 *)0xabd54000u;
	g_kermit_net_buf_size = 0x4000;
	SceUID mtx = sceKernelCreateMutex("SceKermitNet", 0x100, 0, NULL);
	g_kermit_net_mutex = mtx;
	if (mtx <= 0) {
		return 1;
	}

	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);
	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	int res = lret;
	if (lret == 0) {
		u64 resp = 0;
		int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x34, 0, 0, &resp);
		int sres = (kret != 0) ? kret : (int)resp;
		int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
		res = (uret != 0) ? uret : sres;
	}
	return (res < 0) ? 1 : 0;
}

/* 0x5610 */
static int wlanKermitTerm(int arg)
{
	(void)arg;
	u8 req_buf[128] __attribute__((aligned(64)));
	g_mac_cached = 0;
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}
	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x3f, 0, 0, &resp);
	int sres = (kret != 0) ? kret : (int)resp;
	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : sres;
}

/* 0x56c4 */
static int wlanPollSwitchState(void)
{
	u64 now = (u64)sceKernelGetSystemTimeWide();
	if (now > g_next_switch_poll_time) {
		g_next_switch_poll_time = now + 1500000ULL;
		wlanKermitGetSwitchAndPower();
	}
	return (int)(g_switch_and_power_state & 1u);
}

/* 0x5754 */
static int wlanGetMacAddr(void *out_mac)
{
	if (g_mac_cached != 0) {
		sceKernelMemcpy(out_mac, g_ether_addr, 8);
		return 0;
	}
	int ret = wlanKermitCmdBuf(0x45, out_mac, 8, 2);
	if (ret >= 0) {
		sceKernelMemcpy(g_ether_addr, out_mac, 8);
		g_mac_cached = 1;
	}
	return ret;
}

/* 0x57e8 */
static int wlanKermitJoin(void *bss)
{
	return wlanKermitCmdBuf(0x3a, bss, 0x70, 1);
}

/* 0x5810 */
static int wlanKermitCreateIbss(void *bss)
{
	return wlanKermitCmdBuf(0x4e, bss, 0x70, 3);
}

/* 0x5838 */
static int wlanKermitDisconnect(void)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}
	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x3e, 0, 0, &resp);
	int sres = (kret != 0) ? kret : (int)resp;
	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : sres;
}

/* 0x58e4 */
static int wlanKermitTxData(void *buf, u32 len)
{
	return wlanKermitTransferBuf(0x4f, buf, len, 1);
}

/* 0x590c */
static int wlanKermitRxData(void *buf, u32 max_len)
{
	return wlanKermitTransferBuf(0x4a, buf, max_len, 2);
}

/* 0x5934 */
static int wlanKermitSetWakeup(void *cfg)
{
	return wlanKermitCmdBuf(0x3d, cfg, 0x2c, 1);
}

/* 0x595c */
static int wlanKermitGetResumeInfo(void *buf)
{
	return wlanKermitCmdBuf(0x38, buf, 0x170, 2);
}

/* 0x5984 */
static int wlanKermitSetHostDiscover(void *cfg)
{
	return wlanKermitCmdBuf(0x37, cfg, 0x30, 1);
}

/* 0x59ac */
static int wlanKermitPollEvent(void)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}
	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x30, 0, 0, &resp);
	int sres = (kret != 0) ? kret : (int)resp;
	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : sres;
}

/* 0x5a58: sceWlanDrv_lib_B7865DB5 */
int sceWlanDrv_lib_B7865DB5(void)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	wlanKermitGetSwitchAndPower();
	int res = (int)(g_switch_and_power_state & 2u);
	pspSetK1(k1);
	return res;
}

/* 0x5a90: sceWlanDrv_lib_E47015D7 */
u32 sceWlanDrv_lib_E47015D7(void)
{
	return g_unknown_84b0;
}

/* 0x5a9c: sceWlanDrv_lib_768E275B */
int sceWlanDrv_lib_768E275B(void)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	u32 k1 = pspGetK1();
	sceKernelDcacheInvalidateRange(req_buf, 64);
	pspSetK1(k1 << 11);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	int res = lret;
	if (lret == 0) {
		u64 resp = 0;
		int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x46, 0, 0, &resp);
		int sres = (kret != 0) ? kret : (int)resp;
		int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
		res = (uret != 0) ? uret : sres;
	}
	pspSetK1(k1);
	return res;
}

/* 0x5b5c: sceWlanDrv_lib_E093A922 */
int sceWlanDrv_lib_E093A922(void)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	u32 k1 = pspGetK1();
	sceKernelDcacheInvalidateRange(req_buf, 64);
	pspSetK1(k1 << 11);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	int res = lret;
	if (lret == 0) {
		u64 resp = 0;
		int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x51, 0, 0, &resp);
		int sres = (kret != 0) ? kret : (int)resp;
		int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
		res = (uret != 0) ? uret : sres;
	}
	pspSetK1(k1);
	return res;
}

/* 0x5c1c: sceWlanDrv_lib_7EED1723 */
int sceWlanDrv_lib_7EED1723(u32 val)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	int ret = wlanKermitSimple1Arg(0x41, val);
	pspSetK1(k1);
	return ret;
}

/* 0x5c50: sceWlanDrv_lib_107E61D0 */
int sceWlanDrv_lib_107E61D0(u32 id, const void *buf, u32 len)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	int ret = wlanKermitSetKey(0x4b, id, buf, len);
	pspSetK1(k1);
	return ret;
}

/* 0x5c90: sceWlanDrv_lib_A2DA9314 */
int sceWlanDrv_lib_A2DA9314(u32 a0, u32 a1)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	int ret = wlanKermitSimple2Arg(0x3b, a0, a1);
	pspSetK1(k1);
	return ret;
}

/* 0x5cc8: sceWlanDrv_lib_5CFFAAB1 */
int sceWlanDrv_lib_5CFFAAB1(u32 id, const void *buf, u32 len)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	int ret = wlanKermitSetKey(0x47, id, buf, len);
	g_max_send_size = g_unknown_84b4;
	pspSetK1(k1);
	return ret;
}

/* 0x5d18: sceWlanDrv_lib_91E04A18 */
int sceWlanDrv_lib_91E04A18(u32 a0, u32 a1)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	int ret = wlanKermitSimple2Arg(0x42, a0, a1);
	pspSetK1(k1);
	return ret;
}

/* 0x5d50: sceWlanDrv_lib_56AE09D2 */
int sceWlanDrv_lib_56AE09D2(u32 a0, u32 a1)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	int ret = wlanKermitSimple2Arg(0x43, a0, a1);
	pspSetK1(k1);
	return ret;
}

/* 0x5d88: sceWlanDrv_lib_F7713C18 */
int sceWlanDrv_lib_F7713C18(u32 id, void *buf, u32 *inout_len, u32 *out_aux)
{
	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	pspSetK1(k1_shifted);

	int ret;
	if ((s32)(k1_shifted & (u32)out_aux) < 0) {
		ret = 0x80410d13;
	} else {
		ret = wlanKermitGetParam(0x3c, id, buf, inout_len, out_aux);
	}
	pspSetK1(k1);
	return ret;
}

/* 0x5df4: sceWlanDrv_lib_327C9FCE */
int sceWlanDrv_lib_327C9FCE(u32 id, void *buf, u32 *inout_len)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	int ret = wlanKermitGetParam(0x35, id, buf, inout_len, NULL);
	pspSetK1(k1);
	return ret;
}

/* 0x5e38: sceWlanDrv_lib_6417F8E3 */
int sceWlanDrv_lib_6417F8E3(u32 id, void *buf, u32 *inout_len)
{
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	int ret = wlanKermitGetParam(0x32, id, buf, inout_len, NULL);
	pspSetK1(k1);
	return ret;
}

/* 0x5e7c: sceWlanDrv_lib_E39F7F88 */
int sceWlanDrv_lib_E39F7F88(u32 id, u32 level, u32 opt, void *buf, u32 *inout_len)
{
	if (!wlanValidateOpt(level, opt)) {
		return 0;
	}
	u32 k1 = pspGetK1();
	u32 k1_shifted = k1 << 11;
	pspSetK1(k1_shifted);
	if ((s32)(k1_shifted & (u32)inout_len) < 0) {
		pspSetK1(k1);
		return 0x80410d13;
	}
	u32 len = *inout_len;
	int ret = wlanKermitGetSetOpt(0x31, id, level, opt, buf, &len);
	*inout_len = len;
	pspSetK1(k1);
	return ret;
}

/* 0x5f44: sceWlanDrv_lib_9ED787C8 */
int sceWlanDrv_lib_9ED787C8(u32 id, u32 level, u32 opt, void *buf, u32 len)
{
	if (level == 0xffffu) {
		if (opt < 0x1005u) {
			if (opt >= 0x1003u || opt == 0x10u) {
				return 0;
			}
		} else if (opt == 0x1009u) {
			return 0;
		}
	}
	if (!wlanValidateOpt(level, opt)) {
		return 0;
	}
	u32 k1 = pspGetK1();
	pspSetK1(k1 << 11);
	u32 l = len;
	int ret = wlanKermitGetSetOpt(0x44, id, level, opt, buf, &l);
	pspSetK1(k1);
	return ret;
}

/* 0x6024: sceWlanDrv_lib_78C84FEC */
int sceWlanDrv_lib_78C84FEC(void)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	u32 k1 = pspGetK1();
	sceKernelDcacheInvalidateRange(req_buf, 64);
	pspSetK1(k1 << 11);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	int res = lret;
	if (lret == 0) {
		u64 resp = 0;
		int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x48, 0, 0, &resp);
		int sres = (kret != 0) ? kret : (int)resp;
		int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
		res = (uret != 0) ? uret : sres;
	}
	pspSetK1(k1);
	return res;
}

/* 0x60e4: sceWlanDrv_lib_80B7162E */
int sceWlanDrv_lib_80B7162E(void)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	u32 k1 = pspGetK1();
	g_scan_ready = 0;
	sceKernelDcacheInvalidateRange(req_buf, 64);
	pspSetK1(k1 << 11);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	int res = lret;
	if (lret == 0) {
		u64 resp = 0;
		int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x4d, 0, 0, &resp);
		int sres = (kret != 0) ? kret : (int)resp;
		int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
		res = (uret != 0) ? uret : sres;
	}
	pspSetK1(k1);
	return res;
}

/* 0x61ac: sceWlanDrv_lib_3BE42BCF */
int sceWlanDrv_lib_3BE42BCF(void)
{
	return g_scan_ready;
}

/* 0x61b8 */
static int wlanKermitSimple1Arg(u32 cmd, u32 val)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}
	req->args[0] = val;
	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, cmd, 1, 0, &resp);
	int sres = (kret != 0) ? kret : (int)resp;
	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : sres;
}

/* 0x6280 */
static int wlanKermitCmdBuf(u32 cmd, void *buf, u32 size, u32 dir_flags)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);
	u8 *kbuf = g_kermit_net_buf;

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}

	if (buf != NULL) {
		if (dir_flags & 1) {
			sceKernelMemcpy(kbuf, buf, size);
		}
		sceKermitMemory_driver_AAF047AC(req, 0, kbuf, size, dir_flags);
	} else {
		sceKermitMemory_driver_AAF047AC(req, 0, NULL, 0, dir_flags);
	}

	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, cmd, 1, 0, &resp);
	int res = (kret != 0) ? kret : (int)resp;
	if (kret == 0 && res == 0 && (dir_flags & 2) != 0 && buf != NULL) {
		sceKernelMemcpy(buf, kbuf, size);
	}

	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : res;
}

/* 0x63dc */
static int wlanKermitSimple2Arg(u32 cmd, u32 arg0, u32 arg1)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}
	req->args[0] = arg0;
	req->args[1] = arg1;
	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, cmd, 2, 0, &resp);
	int sres = (kret != 0) ? kret : (int)resp;
	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : sres;
}

/* 0x64b4 */
static int wlanKermitTransferBuf(u32 cmd, void *buf, u32 size, u32 dir_flags)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	u32 total_buf_size = g_kermit_net_buf_size;
	u8 *kbuf = g_kermit_net_buf;
	if (size > total_buf_size) {
		return 0x80410d12;
	}

	u32 half_size = total_buf_size >> 1;
	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}

	if (dir_flags & 1) {
		sceKernelMemcpy(kbuf, buf, size);
	}
	sceKermitMemory_driver_AAF047AC(req, 0, kbuf, half_size, dir_flags);
	req->args[1] = size;

	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, cmd, 2, 0, &resp);
	int res = (kret != 0) ? kret : (int)resp;
	if (kret == 0 && res > 0 && (dir_flags & 2) != 0) {
		sceKernelMemcpy(buf, kbuf, (u32)res);
	}

	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : res;
}

/* 0x6628 */
static int wlanKermitGetParam(u32 cmd, u32 id, void *buf, u32 *inout_len, u32 *out_aux)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);
	u32 k1 = pspGetK1();
	u8 *kbuf = g_kermit_net_buf;

	if ((s32)(k1 & (u32)inout_len) < 0) {
		return 0x80410d13;
	}
	u32 len = *inout_len;
	if (len >= 0x11) {
		len = 0x10;
	}
	if ((s32)((((u32)buf + 0x10) | (u32)buf) & k1) < 0) {
		return 0x80410d13;
	}

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}

	u32 xfer_size = 0x104;
	if (buf != NULL) {
		*(u32 *)kbuf = len;
		xfer_size = len + 8;
	}
	req->args[0] = id;
	req->args[1] = xfer_size;
	sceKermitMemory_driver_AAF047AC(req, 2, kbuf, xfer_size, 3);

	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, cmd, 3, 0, &resp);
	int res = (kret != 0) ? kret : (int)resp;
	if (kret == 0 && res == 0) {
		if (buf != NULL) {
			u32 rlen = *(u32 *)kbuf;
			if (rlen >= 0x11) {
				rlen = 0x10;
			}
			if (rlen != 0) {
				sceKernelMemcpy(buf, kbuf + 8, rlen);
			}
			*inout_len = rlen;
		}
		if (out_aux != NULL) {
			*out_aux = *(u32 *)(kbuf + 4);
		}
	}

	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : res;
}

/* 0x67d4 */
static int wlanKermitGetSetOpt(u32 cmd, u32 id, u32 opt, u32 flags, void *buf, u32 *inout_len)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);
	u32 k1 = pspGetK1();
	u32 len = *inout_len;
	u8 *kbuf = g_kermit_net_buf;

	if ((s32)((((u32)buf + len) | (u32)buf | len) & k1) < 0 || len >= 0xf1u) {
		return 0x80410d13;
	}

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}

	u32 xfer_size;
	u32 dir_flags;
	if (cmd == 0x31) {
		*(u32 *)kbuf = *inout_len;
		xfer_size = *inout_len + 8;
		dir_flags = 3;
	} else {
		sceKernelMemcpy(kbuf, buf, *inout_len);
		xfer_size = *inout_len;
		dir_flags = 1;
	}

	req->args[0] = id;
	req->args[1] = opt;
	req->args[2] = flags;
	req->args[3] = xfer_size;
	sceKermitMemory_driver_AAF047AC(req, 4, kbuf, xfer_size, dir_flags);

	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, cmd, 5, 0, &resp);
	int res = (kret != 0) ? kret : (int)resp;
	if (kret == 0 && res == 0 && cmd == 0x31) {
		u32 rlen = *(u32 *)kbuf;
		if (rlen - 1u < 0xf0u) {
			*inout_len = rlen;
			if ((s32)((((u32)buf + rlen) | (u32)buf | rlen) & k1) >= 0) {
				sceKernelMemcpy(buf, kbuf + 8, rlen);
			}
		}
	}

	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : res;
}

/* 0x69b0 */
static int wlanKermitSetKey(u32 cmd, u32 id, const void *buf, u32 len)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);
	u32 k1 = pspGetK1();

	u32 orig_len = len;
	if (len >= 0x11) {
		len = 0x10;
	}
	u8 *kbuf = g_kermit_net_buf;
	if (orig_len >= 0x100u || (s32)((((u32)buf + len) | (u32)buf | len) & k1) < 0) {
		return 0x80410d13;
	}

	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}

	req->args[0] = id;
	req->args[1] = len;
	sceKernelMemcpy(kbuf, buf, len);
	sceKermitMemory_driver_AAF047AC(req, 2, kbuf, len, 1);

	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, cmd, 3, 0, &resp);
	int sres = (kret != 0) ? kret : (int)resp;
	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : sres;
}

/* 0x6b04 */
static int wlanKermitSendGameMode(u32 cmd, void *kbuf, u32 id, u32 total_len, u32 flags, void *hdr_buf, u32 *hdr_len, u32 *out_sent)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	u32 xfer_size = total_len + 0x10c;
	if (xfer_size > (g_kermit_net_buf_size << 1)) {
		return 0x80410d12;
	}

	flags &= ~0xc1u;
	u32 dir_flags;
	u8 *kb = (u8 *)kbuf;
	if (cmd == 0x36) {
		*(u32 *)kb = 0;
		dir_flags = 1;
		if (hdr_buf != NULL) {
			u32 hlen = *hdr_len;
			if (hlen != 0) {
				*(u32 *)kb = hlen;
				sceKernelMemcpy(kb + 8, hdr_buf, hlen);
			}
		}
	} else {
		*(u32 *)kb = 0;
		dir_flags = 3;
		if (hdr_buf != NULL && hdr_len != NULL) {
			*(u32 *)kb = 0x10;
		}
	}

	*(u32 *)(kb + 0x104) = total_len;
	*(u32 *)(kb + 0x108) = flags;
	req->args[0] = id;
	req->args[1] = xfer_size;
	sceKermitMemory_driver_AAF047AC(req, 2, kb, xfer_size, dir_flags);

	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, cmd, 3, 0, &resp);
	if (kret != 0) {
		return kret;
	}
	int res = (int)resp;
	if (cmd == 0x36 && out_sent != NULL) {
		*out_sent = (res == 0) ? *(u32 *)(kb + 0x104) : 0;
	}
	return res;
}

/* 0x6cac */
static int wlanKermitPollScanState(void *out_buf)
{
	u8 req_buf[128] __attribute__((aligned(64)));
	sceKernelDcacheInvalidateRange(req_buf, 64);
	SceKermitRequest *req = KERMIT_UNCACHED(req_buf);

	u32 half_size = g_kermit_net_buf_size >> 1;
	u8 *kbuf = g_kermit_net_buf;
	int lret = sceKernelLockMutex(g_kermit_net_mutex, 1, NULL);
	if (lret != 0) {
		return lret;
	}

	req->args[0] = 0x90;
	sceKermitMemory_driver_AAF047AC(req, 1, kbuf, half_size, 2);

	u64 resp = 0;
	int kret = sceKermit_driver_36666181(req, KERMIT_MODE_WLAN, 0x50, 2, 0, &resp);
	int res = (kret != 0) ? kret : (int)resp;
	if (kret == 0 && res == 4) {
		sceKernelMemcpy(out_buf, kbuf, 0x90);
	}

	int uret = sceKernelUnlockMutex(g_kermit_net_mutex, 1);
	return (uret != 0) ? uret : res;
}

/* 0x6dcc */
static int wlanValidateOpt(u32 level, u32 opt)
{
	if (level == 6) {
		return (opt - 1u < 2u) ? 1 : 0;
	}
	if ((s32)level < 7) {
		if (level == 0) {
			return (opt - 9u < 5u) ? 1 : 0;
		}
		return 0;
	}
	if (level != 0xffffu) {
		return 0;
	}
	if (opt == 0x200u || opt == 0x20u || opt == 4u || opt == 8u ||
	    opt == 0x80u || opt == 0x100u || opt == 0x800u ||
	    (opt >= 0x1000u && opt < 0x1003u) ||
	    (opt >= 0x1005u && opt < 0x1009u) ||
	    opt == 0x2000u) {
		return 1;
	}
	return 0;
}

