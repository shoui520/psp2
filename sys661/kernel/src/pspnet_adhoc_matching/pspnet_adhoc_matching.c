/*
 * ePSP 6.61 pspnet_adhoc_matching.prx (sceNetAdhocMatching_Library)
 * Reimplemented from the Vita ePSP 6.61 binary.
 */

#include <psptypes.h>

/* Kernel_Library */
extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int intr);
extern void *sceKernelMemset(void *dst, int c, u32 n);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 n);
extern int sceKernelLockLwMutex(void *workarea, int count, u32 *timeout);
extern int sceKernelUnlockLwMutex(void *workarea, int count);
extern int sceKernelCheckThreadStack(void);

/* ModuleMgrForUser */
extern int sceKernelGetModuleGPByAddress(void *addr, u32 *gp);

/* SysMemUserForUser */
extern int sceKernelGetCompiledSdkVersion(void);

/* ThreadManForUser */
extern int sceKernelCreateThread(const char *name, void *entry, int prio, int stack, u32 attr, void *opt);
extern int sceKernelDeleteThread(int thid);
extern int sceKernelStartThread(int thid, u32 arglen, void *argp);
extern int sceKernelWaitThreadEnd(int thid, u32 *timeout);
extern int sceKernelCreateMbx(const char *name, u32 attr, void *opt);
extern int sceKernelDeleteMbx(int mbxid);
extern int sceKernelSendMbx(int mbxid, void *msg);
extern int sceKernelReceiveMbx(int mbxid, void **msg, u32 *timeout);
extern int sceKernelCreateVpl(const char *name, int part, u32 attr, u32 size, void *opt);
extern int sceKernelDeleteVpl(int vplid);
extern int sceKernelTryAllocateVpl(int vplid, u32 size, void **data);
extern int sceKernelFreeVpl(int vplid, void *data);
extern int sceKernelReferVplStatus(int vplid, void *info);
extern int sceKernelSetAlarm(u32 clock, u32 (*handler)(void *), void *common);
extern int sceKernelCancelAlarm(int alarmid);
extern u32 sceKernelGetSystemTimeLow(void);
extern int sceKernelCreateLwMutex(void *workarea, const char *name, u32 attr, int initCount, void *opt);
extern int sceKernelDeleteLwMutex(void *workarea);

/* sceNet */
extern int sceNetFreeThreadinfo(int thid);

/* sceNet_lib */
extern u32 sceNet_lib_1858883D(void);
extern int sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, u32 len);

/* sceNetAdhoc */
extern int sceNetAdhocPdpCreate(const u8 *mac, u16 port, u32 bufsize, int flag);
extern int sceNetAdhocPdpDelete(int id, int flag);
extern int sceNetAdhocPdpSend(int id, const u8 *dstmac, u16 port, const void *data, int len, u32 timeout, int flag);
extern int sceNetAdhocPdpRecv(int id, u8 *srcmac, u16 *port, void *data, int *len, u32 timeout, int flag);
extern int sceNetAdhocSetSocketAlert(int id, int flag);

/* sceWlanDrv */
extern int sceWlanGetEtherAddr(u8 *mac);

/* Error codes */
#define MATCHING_ERR_STACK_SMALL        ((int)0x80410005)
#define MATCHING_ERR_PDP_WOULDBLOCK     ((int)0x80410709)
#define MATCHING_ERR_INVALID_MODE       ((int)0x80410801)
#define MATCHING_ERR_INVALID_MAXNUM     ((int)0x80410803)
#define MATCHING_ERR_RXBUF_TOO_SHORT    ((int)0x80410804)
#define MATCHING_ERR_INVALID_OPTLEN     ((int)0x80410805)
#define MATCHING_ERR_INVALID_ARG        ((int)0x80410806)
#define MATCHING_ERR_INVALID_ID         ((int)0x80410807)
#define MATCHING_ERR_ID_NOT_AVAIL       ((int)0x80410808)
#define MATCHING_ERR_NO_SPACE           ((int)0x80410809)
#define MATCHING_ERR_IS_RUNNING         ((int)0x8041080A)
#define MATCHING_ERR_NOT_RUNNING        ((int)0x8041080B)
#define MATCHING_ERR_UNKNOWN_TARGET     ((int)0x8041080C)
#define MATCHING_ERR_TARGET_NOT_READY   ((int)0x8041080D)
#define MATCHING_ERR_EXCEED_MAXNUM      ((int)0x8041080E)
#define MATCHING_ERR_REQUEST_IN_PROGRESS ((int)0x8041080F)
#define MATCHING_ERR_ALREADY_ESTABLISHED ((int)0x80410810)
#define MATCHING_ERR_BUSY               ((int)0x80410811)
#define MATCHING_ERR_ALREADY_INITIALIZED ((int)0x80410812)
#define MATCHING_ERR_NOT_INITIALIZED    ((int)0x80410813)
#define MATCHING_ERR_PORT_IN_USE        ((int)0x80410814)
#define MATCHING_ERR_STACKSIZE_TOO_SHORT ((int)0x80410815)
#define MATCHING_ERR_INVALID_DATALEN    ((int)0x80410816)
#define MATCHING_ERR_NOT_ESTABLISHED    ((int)0x80410817)
#define MATCHING_ERR_DATA_BUSY          ((int)0x80410818)
#define MATCHING_ERR_UNALIGNED          ((int)0x80410819)

/* Modes */
#define MATCHING_MODE_HOST   1
#define MATCHING_MODE_CLIENT 2
#define MATCHING_MODE_PTP    3

/* Peer states */
#define PEER_STATE_IDLE        1
#define PEER_STATE_JOIN_RECV   2
#define PEER_STATE_ACCEPTED    3
#define PEER_STATE_SELECT_SENT 4
#define PEER_STATE_ESTABLISHED 5

/* Data states */
#define DATA_STATE_IDLE    1
#define DATA_STATE_SENDING 2

/* Wire packet types */
#define PKT_HELLO          1
#define PKT_JOIN           2
#define PKT_ACCEPT         3
#define PKT_COMPLETE       4
#define PKT_CANCEL         5
#define PKT_KEEPALIVE      6
#define PKT_KEEPALIVE_ACK  7
#define PKT_BYE            8
#define PKT_JOIN_ACK       9
#define PKT_DATA          10
#define PKT_DATA_ACK      11

/* User callback events */
#define MATCHING_EV_HELLO          1
#define MATCHING_EV_JOIN           2
#define MATCHING_EV_LEFT           3
#define MATCHING_EV_REJECT         4
#define MATCHING_EV_CANCEL         5
#define MATCHING_EV_ACCEPT         6
#define MATCHING_EV_ESTABLISHED    7
#define MATCHING_EV_TIMEOUT        8
#define MATCHING_EV_ERROR          9
#define MATCHING_EV_BYE           10
#define MATCHING_EV_DATA          11
#define MATCHING_EV_DATA_CONFIRM  12
#define MATCHING_EV_DATA_TIMEOUT  13

/* Internal mailbox message types */
#define MBX_MSG_PACKET     1
#define MBX_MSG_CTRL_RETRY 2
#define MBX_MSG_KEEPALIVE  3
#define MBX_MSG_HELLO      4
#define MBX_MSG_DATA_RETRY 5

typedef void (*SceNetAdhocMatchingHandler)(int matchingid, int event, const u8 *mac, int optlen, void *opt);

typedef struct MatchingPoolStat {
    u32 size;
    u32 maxsize;
    u32 freesize;
} MatchingPoolStat;

typedef struct MatchingMember {
    struct MatchingMember *next;
    u8 mac[6];
    u8 pad[2];
} MatchingMember;

typedef struct MatchingVplInfo {
    u32 size;
    char name[32];
    u32 attr;
    int pool_size;
    int free_size;
    int wait_threads;
} MatchingVplInfo;

typedef struct MatchingThreadOpt {
    u32 size;
    int stack_mpid;
} MatchingThreadOpt;

struct MatchingPeer;

typedef struct MatchingMbxMsg {
    u32 mbx_next;
    u32 mbx_pad;
    int msg_type;
    struct MatchingPeer *peer;
    u32 pending_flag;
} MatchingMbxMsg;

typedef struct MatchingPeer {
    struct MatchingPeer *next;      /* 0x00 (0) */
    int state;                      /* 0x04 (1) */
    u8 mac[6];                      /* 0x08 (2) */
    u8 mac_pad[2];                  /* 0x0E */
    u32 peer_hello_delay;           /* 0x10 (4) */
    u32 peer_ping_delay;            /* 0x14 (5) */
    int rx_raw_len;                 /* 0x18 (6) */
    u8 *rx_pkt;                     /* 0x1C (7) */
    int rx_wire_len;                /* 0x20 (8) */
    int opt_len;                    /* 0x24 (9) */
    void *opt_buf;                  /* 0x28 (10) */
    MatchingMbxMsg rx_msg;          /* 0x2C (11..15) */
    MatchingMbxMsg ctrl_alarm_msg;  /* 0x40 (16..20) */
    int ctrl_alarm_id;              /* 0x54 (21) */
    int ping_retry;                 /* 0x58 (22) */
    int select_retry;               /* 0x5C (23) */
    int event_mbx;                  /* 0x60 (24) */
    u32 peer_caps;                  /* 0x64 (25) */
    int pending_free;               /* 0x68 (26) */
    u32 own_session_id;           /* 0x6C (27) */
    u32 remote_session_id;          /* 0x70 (28) */
    u32 tx_seq;                     /* 0x74 (29) */
    u32 rx_expected_seq;            /* 0x78 (30) */
    int data_state;                 /* 0x7C (31) */
    int data_len;                   /* 0x80 (32) */
    void *data_buf;                 /* 0x84 (33) */
    MatchingMbxMsg data_alarm_msg;  /* 0x88 (34..38) */
    int data_alarm_id;              /* 0x9C (39) */
    int data_retry;                 /* 0xA0 (40) */
} MatchingPeer;

typedef struct MatchingCtx {
    struct MatchingCtx *next;       /* 0x00 (0) */
    int id;                         /* 0x04 (1) */
    int running;                    /* 0x08 (2) */
    int mode;                       /* 0x0C (3) */
    int max_peers;                  /* 0x10 (4) */
    u16 port;                       /* 0x14 (5) */
    u16 port_pad;                   /* 0x16 */
    int rx_buflen;                  /* 0x18 (6) */
    u8 *rx_buf;                     /* 0x1C (7) */
    u32 hello_delay;                /* 0x20 (8) */
    u32 ping_delay;                 /* 0x24 (9) */
    int init_count;                 /* 0x28 (10) */
    u32 msg_delay;                  /* 0x2C (11) */
    SceNetAdhocMatchingHandler handler; /* 0x30 (12) */
    int min_stack;                  /* 0x34 (13) */
    int pdp_id;                     /* 0x38 (14) */
    int event_thid;                 /* 0x3C (15) */
    int event_mbx;                  /* 0x40 (16) */
    int input_thid;                 /* 0x44 (17) */
    int hello_pkt_len;              /* 0x48 (18) */
    u8 *hello_pkt;                  /* 0x4C (19) */
    MatchingMbxMsg hello_msg;       /* 0x50 (20..24) */
    int members_pkt_len;            /* 0x64 (25) */
    u8 *members_pkt;                /* 0x68 (26) */
    int hello_alarm_id;             /* 0x6C (27) */
    u8 hello_trailer[4];            /* 0x70 (28) */
    MatchingPeer *peers;            /* 0x74 (29) */
    u32 handler_gp;                 /* 0x78 (30) */
} MatchingCtx;

static int g_vpl_id = -1;
static int g_initialized;
static int g_pool_max_alloc;
static int g_mutex_created;
static MatchingCtx *g_ctx_list;
static int g_last_ctx_id;
static u8 g_mutex_work[0x20];
static u8 g_self_mac[6];

static inline u16 matchingSwap16(u16 x)
{
    return (u16)__builtin_allegrex_wsbh(x);
}

static inline u32 matchingSwap32(u32 x)
{
    return (u32)__builtin_allegrex_wsbw(x);
}

static inline __attribute__((always_inline))
void matchingInvokeHandler(MatchingCtx *ctx, int event, const u8 *mac, int optlen, void *opt)
{
    int rem = sceKernelCheckThreadStack() - ctx->min_stack;
    SceNetAdhocMatchingHandler cb = ctx->handler;
    int pad = (rem > 0) ? rem : 0;
    if (cb != NULL) {
        u32 saved_gp;
        __asm__ volatile ("move %0, $gp" : "=r"(saved_gp));
        __asm__ volatile ("move $gp, %0" : : "r"(ctx->handler_gp));
        {
            void * volatile stack_pad = __builtin_alloca(((u32)pad + 15u) & ~15u);
            (void)stack_pad;
            cb(ctx->id, event, mac, optlen, opt);
        }
        __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
    }
}

/* Forward declarations in original binary address order */
static int matchingStartInternal(int id, int evth_prio, int evth_stack, int evth_mpid, int inth_prio, int inth_stack, int inth_mpid, int optlen, const void *opt);
static int matchingCancelTargetInternal(int id, const u8 *target, int optlen, const void *opt);
static int matchingStartEventThread(MatchingCtx *ctx, int prio, int stack, int mpid);
static int matchingStopEventThread(MatchingCtx *ctx, int *lock_res);
static int matchingEventThread(u32 arglen, void *argp);
static void matchingProcessPacket(MatchingCtx *ctx, MatchingPeer *peer);
static void matchingHandleCtrlRetry(MatchingCtx *ctx, MatchingPeer *peer);
static int matchingSendCtrlPkt(MatchingCtx *ctx, MatchingPeer *peer, int pkt_type, int optlen, const void *opt);
static int matchingCreatePdp(MatchingCtx *ctx);
static int matchingDeletePdp(MatchingCtx *ctx);
static int matchingSendDataPkt(MatchingCtx *ctx, MatchingPeer *peer, int pkt_type, int datalen, const void *data);
static int matchingBuildHelloPkt(MatchingCtx *ctx, int optlen, const void *opt);
static void matchingFreeHelloPkt(MatchingCtx *ctx);
static int matchingBroadcastHello(MatchingCtx *ctx);
static int matchingBuildMembersPkt(MatchingCtx *ctx);
static int matchingSendMembersPkt(MatchingCtx *ctx, MatchingPeer *peer);
static int matchingCopyMembersPkt(MatchingCtx *ctx, const void *pkt, int len);
static void matchingFreeMembersPkt(MatchingCtx *ctx);
static int matchingBroadcastBye(MatchingCtx *ctx);
static int matchingStartInputThread(MatchingCtx *ctx, int prio, int stack, int mpid);
static int matchingStopInputThread(MatchingCtx *ctx, int *lock_res);
static int matchingInputThread(u32 arglen, void *argp);
static int matchingSetCtrlRetryAlarm(MatchingCtx *ctx, MatchingPeer *peer);
static int matchingSetDataRetryAlarm(MatchingCtx *ctx, MatchingPeer *peer);
static int matchingSetKeepAliveAlarm(MatchingCtx *ctx, MatchingPeer *peer);
static int matchingSetHelloAlarm(MatchingCtx *ctx, u32 delay);
static int matchingCancelPeerAlarms(MatchingPeer *peer);
static int matchingCancelDataAlarm(MatchingPeer *peer);
static int matchingCancelHelloAlarm(MatchingCtx *ctx);
static u32 matchingCtrlRetryAlarmCb(void *common);
static u32 matchingDataRetryAlarmCb(void *common);
static u32 matchingKeepAliveAlarmCb(void *common);
static u32 matchingHelloAlarmCb(void *common);
static void matchingRemovePeer(MatchingCtx *ctx, MatchingPeer *peer);
static void matchingSetPeerState(MatchingCtx *ctx, MatchingPeer *peer, int new_state);
static void matchingSetPeerDataState(MatchingCtx *ctx, MatchingPeer *peer, int new_state);
static int matchingCreateLock(void);
static void matchingDeleteLock(void);
static int matchingLock(void);
static void matchingUnlock(int lock_res);
static int matchingInitCtxList(void);
static int matchingFreeAllCtxs(void);
static int matchingCheckNoActiveCtx(void);
static int matchingCreateVpl(int poolsize);
static void matchingDeleteVpl(void);
static void *matchingAllocVpl(int size);
static int matchingFreeVpl(void *ptr);
static int matchingGetPoolMaxAllocInternal(void);
static int matchingGetPoolStatInternal(MatchingPoolStat *stat);
static int matchingAllocCtx(u16 port);
static int matchingFreeCtx(MatchingCtx *ctx);
static MatchingCtx *matchingFindCtxById(int id);
static MatchingPeer *matchingCreatePeer(MatchingCtx *ctx, const u8 *mac);
static MatchingPeer *matchingFindPeerByMac(MatchingCtx *ctx, const u8 *mac);
static int matchingDestroyAllPeers(MatchingCtx *ctx);
static void matchingCollectPeerMacs(MatchingCtx *ctx, int min_state, u8 *out, int *count_inout);
static int matchingCountPeersAtLeast(MatchingCtx *ctx, int min_state);
static void matchingGetMembersInternal(MatchingCtx *ctx, int *buflen, void *buf);
static int matchingCompareMac(const MatchingPeer *peer);

/* 0x000000 */
int sceNetAdhocMatchingInit(int poolsize)
{
    u8 mac[16];
    int ret;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized > 0)
        return MATCHING_ERR_ALREADY_INITIALIZED;

    ret = matchingCreateLock();
    if (ret < 0)
        return ret;
    ret = matchingCreateVpl(poolsize);
    if (ret >= 0) {
        ret = matchingInitCtxList();
        if (ret >= 0) {
            ret = sceWlanGetEtherAddr(mac);
            if (ret >= 0) {
                sceKernelMemcpy(g_self_mac, mac, 6);
                g_initialized = 1;
                return 0;
            }
            matchingFreeAllCtxs();
        }
        matchingDeleteVpl();
    }
    matchingDeleteLock();
    return ret;
}

/* 0x0000dc */
int sceNetAdhocMatchingCreate(int mode, int maxpeers, u16 port, int bufsize,
                              u32 hellodelay, u32 pingdelay, int initcount,
                              u32 msgdelay, SceNetAdhocMatchingHandler callback)
{
    int lock_res;
    int ret;

    if (sceKernelCheckThreadStack() < 3008)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ret = MATCHING_ERR_INVALID_MODE;
    if ((u32)(mode - 1) < 3u) {
        ret = MATCHING_ERR_INVALID_MAXNUM;
        if ((u32)(maxpeers - 2) < 15u) {
            ret = MATCHING_ERR_RXBUF_TOO_SHORT;
            if (bufsize >= maxpeers * 6 + 4) {
                ret = MATCHING_ERR_INVALID_ARG;
                if (((mode != MATCHING_MODE_HOST && mode != MATCHING_MODE_PTP) ||
                     (hellodelay != 0 && pingdelay != 0)) &&
                    initcount >= 0 && msgdelay != 0) {
                    int id = matchingAllocCtx(port);
                    ret = id;
                    if (id >= 0) {
                        MatchingCtx *ctx = matchingFindCtxById(id);
                        void *rx_buf;
                        ctx->mode = mode;
                        ctx->max_peers = (mode == MATCHING_MODE_HOST) ? maxpeers : 2;
                        ctx->port = port;
                        ctx->rx_buflen = bufsize;
                        rx_buf = matchingAllocVpl(bufsize);
                        ctx->rx_buf = (u8 *)rx_buf;
                        if (rx_buf != NULL) {
                            u32 cur_gp;
                            ctx->hello_delay = hellodelay;
                            ctx->ping_delay = pingdelay;
                            ctx->init_count = initcount;
                            ctx->msg_delay = msgdelay;
                            ctx->handler = callback;
                            ctx->hello_alarm_id = -1;
                            ctx->hello_trailer[0] = 1;
                            ctx->hello_trailer[1] = 0;
                            ctx->hello_trailer[2] = 0;
                            ctx->hello_trailer[3] = 0;
                            ctx->peers = NULL;
                            if (sceKernelGetModuleGPByAddress((void *)callback, &ctx->handler_gp) < 0) {
                                __asm__ volatile ("move %0, $gp" : "=r"(cur_gp));
                                ctx->handler_gp = cur_gp;
                            }
                            ret = id;
                        } else {
                            matchingFreeCtx(ctx);
                            ret = MATCHING_ERR_NO_SPACE;
                        }
                    }
                }
            }
        }
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000304 */
int sceNetAdhocMatchingStop(int matchingid)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->running == 2) {
            ctx->running = 1;
            matchingStopInputThread(ctx, &lock_res);
            matchingStopEventThread(ctx, &lock_res);
            if (ctx->mode == MATCHING_MODE_HOST || ctx->mode == MATCHING_MODE_PTP) {
                matchingCancelHelloAlarm(ctx);
                matchingFreeHelloPkt(ctx);
                ctx->hello_msg.pending_flag &= ~1u;
            }
            matchingDestroyAllPeers(ctx);
            matchingFreeMembersPkt(ctx);
            matchingDeletePdp(ctx);
            ctx->running = 0;
        }
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000424 */
int sceNetAdhocMatchingDelete(int matchingid)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3056)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->running != 0) {
            ret = MATCHING_ERR_IS_RUNNING;
        } else {
            matchingFreeVpl(ctx->rx_buf);
            ctx->rx_buf = NULL;
            matchingFreeCtx(ctx);
        }
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x0004f0 */
int sceNetAdhocMatchingSelectTarget(int matchingid, const u8 *target, int optlen, const void *opt)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (target == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx == NULL) {
        ret = MATCHING_ERR_INVALID_ID;
    } else if (ctx->running != 2) {
        ret = MATCHING_ERR_NOT_RUNNING;
    } else {
        MatchingPeer *peer = matchingFindPeerByMac(ctx, target);
        if (peer == NULL) {
            ret = MATCHING_ERR_UNKNOWN_TARGET;
        } else if ((u32)optlen > 0xFFD7u || (optlen > 0 && opt == NULL)) {
            ret = MATCHING_ERR_INVALID_OPTLEN;
        } else {
            int active_count = matchingCountPeersAtLeast(ctx, PEER_STATE_ACCEPTED);
            switch (peer->state) {
            case PEER_STATE_IDLE:
                if (ctx->mode == MATCHING_MODE_HOST) {
                    ret = MATCHING_ERR_TARGET_NOT_READY;
                } else if (active_count + 1 >= ctx->max_peers) {
                    ret = MATCHING_ERR_EXCEED_MAXNUM;
                } else {
                    if (peer->opt_len > 0) {
                        matchingFreeVpl(peer->opt_buf);
                        peer->opt_len = 0;
                        peer->opt_buf = NULL;
                    }
                    if (optlen > 0) {
                        void *buf = matchingAllocVpl(optlen);
                        peer->opt_buf = buf;
                        if (buf == NULL) {
                            ret = MATCHING_ERR_NO_SPACE;
                            break;
                        }
                        sceKernelMemcpy(buf, opt, (u32)optlen);
                        peer->opt_len = optlen;
                    }
                    peer->own_session_id++;
                    if (peer->own_session_id == 0)
                        peer->own_session_id = 1;
                    matchingSendCtrlPkt(ctx, peer, PKT_JOIN, peer->opt_len, peer->opt_buf);
                    matchingSetCtrlRetryAlarm(ctx, peer);
                    matchingSetPeerState(ctx, peer, PEER_STATE_SELECT_SENT);
                    peer->select_retry = ctx->init_count;
                }
                break;
            case PEER_STATE_JOIN_RECV:
                if (active_count + 1 >= ctx->max_peers) {
                    ret = MATCHING_ERR_EXCEED_MAXNUM;
                } else {
                    if (peer->opt_len > 0) {
                        matchingFreeVpl(peer->opt_buf);
                        peer->opt_len = 0;
                        peer->opt_buf = NULL;
                    }
                    if (optlen > 0) {
                        void *buf = matchingAllocVpl(optlen);
                        peer->opt_buf = buf;
                        if (buf == NULL) {
                            ret = MATCHING_ERR_NO_SPACE;
                            break;
                        }
                        sceKernelMemcpy(buf, opt, (u32)optlen);
                        peer->opt_len = optlen;
                    }
                    peer->own_session_id++;
                    if (peer->own_session_id == 0)
                        peer->own_session_id = 1;
                    matchingSendCtrlPkt(ctx, peer, PKT_ACCEPT, peer->opt_len, peer->opt_buf);
                    matchingSetCtrlRetryAlarm(ctx, peer);
                    matchingSetPeerState(ctx, peer, PEER_STATE_ACCEPTED);
                    peer->select_retry = ctx->init_count;
                }
                break;
            case PEER_STATE_ACCEPTED:
            case PEER_STATE_SELECT_SENT:
                ret = MATCHING_ERR_REQUEST_IN_PROGRESS;
                break;
            case PEER_STATE_ESTABLISHED:
                ret = MATCHING_ERR_ALREADY_ESTABLISHED;
                break;
            default:
                break;
            }
        }
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x0007f4 */
int sceNetAdhocMatchingSetHelloOpt(int matchingid, int optlen, const void *opt)
{
    int lock_res;
    int ret;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ret = MATCHING_ERR_INVALID_ID;
    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        ret = MATCHING_ERR_INVALID_MODE;
        if (ctx->mode != MATCHING_MODE_CLIENT) {
            ret = MATCHING_ERR_NOT_RUNNING;
            if (ctx->running == 2)
                ret = matchingBuildHelloPkt(ctx, optlen, opt);
        }
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x0008e0 */
int sceNetAdhocMatchingGetHelloOpt(int matchingid, int *optlen, void *opt)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (optlen == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->mode == MATCHING_MODE_CLIENT) {
            ret = MATCHING_ERR_INVALID_MODE;
        } else if (ctx->running == 2) {
            int len = ctx->hello_pkt_len - 12;
            if (opt != NULL && len > 0) {
                if (*optlen < len)
                    len = *optlen;
                sceKernelMemcpy(opt, ctx->hello_pkt + 12, (u32)len);
            }
            *optlen = len;
        } else {
            ret = MATCHING_ERR_NOT_RUNNING;
        }
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000a08 */
int sceNetAdhocMatchingGetMembers(int matchingid, int *buflen, void *buf)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (((u32)buf & 3u) != 0)
        return MATCHING_ERR_UNALIGNED;
    if (buflen == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->running == 2)
            matchingGetMembersInternal(ctx, buflen, buf);
        else
            ret = MATCHING_ERR_NOT_RUNNING;
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000b00 */
int sceNetAdhocMatchingSendData(int matchingid, const u8 *target, int datalen, const void *data)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (target == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->running == 2) {
            MatchingPeer *peer = matchingFindPeerByMac(ctx, target);
            if (peer != NULL) {
                if ((u32)(datalen - 1) > 0xFFD6u) {
                    ret = MATCHING_ERR_INVALID_DATALEN;
                } else if (data != NULL) {
                    if (peer->state == PEER_STATE_ESTABLISHED) {
                        if (peer->data_state == DATA_STATE_IDLE) {
                            void *buf;
                            if (peer->data_len > 0) {
                                matchingFreeVpl(peer->data_buf);
                                peer->data_len = 0;
                                peer->data_buf = NULL;
                            }
                            buf = matchingAllocVpl(datalen);
                            peer->data_buf = buf;
                            if (buf != NULL) {
                                sceKernelMemcpy(buf, data, (u32)datalen);
                                peer->data_len = datalen;
                                peer->tx_seq++;
                                matchingSendDataPkt(ctx, peer, PKT_DATA, datalen, peer->data_buf);
                                matchingSetDataRetryAlarm(ctx, peer);
                                matchingSetPeerDataState(ctx, peer, DATA_STATE_SENDING);
                                peer->data_retry = ctx->init_count;
                            } else {
                                ret = MATCHING_ERR_NO_SPACE;
                            }
                        } else if (peer->data_state == DATA_STATE_SENDING) {
                            ret = MATCHING_ERR_DATA_BUSY;
                        }
                    } else {
                        ret = MATCHING_ERR_NOT_ESTABLISHED;
                    }
                } else {
                    ret = MATCHING_ERR_INVALID_ARG;
                }
            } else {
                ret = MATCHING_ERR_UNKNOWN_TARGET;
            }
        } else {
            ret = MATCHING_ERR_NOT_RUNNING;
        }
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000d00 */
int sceNetAdhocMatchingAbortSendData(int matchingid, const u8 *target)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (target == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->running == 2) {
            MatchingPeer *peer = matchingFindPeerByMac(ctx, target);
            if (peer != NULL) {
                matchingCancelDataAlarm(peer);
                matchingSetPeerDataState(ctx, peer, DATA_STATE_IDLE);
            } else {
                ret = MATCHING_ERR_UNKNOWN_TARGET;
            }
        } else {
            ret = MATCHING_ERR_NOT_RUNNING;
        }
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000e08 */
int sceNetAdhocMatchingTerm(void)
{
    int ret;

    if (sceKernelCheckThreadStack() < 3056)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return 0;

    ret = matchingCheckNoActiveCtx();
    if (ret >= 0) {
        matchingFreeAllCtxs();
        matchingDeleteVpl();
        matchingDeleteLock();
        g_initialized = 0;
    }
    return ret;
}

/* 0x000e88 */
int sceNetAdhocMatchingStart(int matchingid, int evthpri, int evthstack, int inthpri, int inthstack, int optlen, const void *opt)
{
    if (sceKernelCheckThreadStack() < 3024)
        return MATCHING_ERR_STACK_SMALL;
    return matchingStartInternal(matchingid, evthpri, evthstack, 2, inthpri, inthstack, 2, optlen, opt);
}

/* 0x000f34 */
int sceNetAdhocMatching_E8454C65(int matchingid, int evthpri, int evthstack, int evthmpid,
                                 int inthpri, int inthstack, int inthmpid, int optlen, const void *opt)
{
    if (sceKernelCheckThreadStack() < 3024)
        return MATCHING_ERR_STACK_SMALL;
    return matchingStartInternal(matchingid, evthpri, evthstack, evthmpid, inthpri, inthstack, inthmpid, optlen, opt);
}

/* 0x000ff0 */
int sceNetAdhocMatchingCancelTarget(int matchingid, const u8 *target)
{
    if (sceKernelCheckThreadStack() < 3056)
        return MATCHING_ERR_STACK_SMALL;
    return matchingCancelTargetInternal(matchingid, target, 0, NULL);
}

/* 0x001050 */
int sceNetAdhocMatchingCancelTargetWithOpt(int matchingid, const u8 *target, int optlen, const void *opt)
{
    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    return matchingCancelTargetInternal(matchingid, target, optlen, opt);
}

/* 0x0010c8 */
int sceNetAdhocMatchingGetPoolMaxAlloc(void)
{
    if (sceKernelCheckThreadStack() < 3056)
        return MATCHING_ERR_STACK_SMALL;
    return matchingGetPoolMaxAllocInternal();
}

/* 0x001104 */
int sceNetAdhocMatchingGetPoolStat(MatchingPoolStat *poolstat)
{
    if (sceKernelCheckThreadStack() < 3056)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (poolstat == NULL)
        return MATCHING_ERR_INVALID_ARG;
    return matchingGetPoolStatInternal(poolstat);
}

/* 0x00116c */
static int __attribute__((noinline))
matchingStartInternal(int id, int evth_prio, int evth_stack, int evth_mpid,
                      int inth_prio, int inth_stack, int inth_mpid, int optlen, const void *opt)
{
    int lock_res;
    MatchingCtx *ctx;
    int ret;

    (void)inth_stack;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (evth_stack < 3072)
        return MATCHING_ERR_STACKSIZE_TOO_SHORT;
    if ((evth_mpid != 2 && evth_mpid != 5) || (inth_mpid != 2 && inth_mpid != 5))
        return MATCHING_ERR_INVALID_ID;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(id);
    ret = MATCHING_ERR_INVALID_ID;
    if (ctx != NULL) {
        ret = MATCHING_ERR_IS_RUNNING;
        if (ctx->running == 0) {
            if (((u32)optlen <= 0xFFCFu && (optlen <= 0 || opt != NULL)) ||
                sceKernelGetCompiledSdkVersion() <= 0x05FFFFFF) {
                ret = matchingCreatePdp(ctx);
                if (ret >= 0) {
                    ret = matchingStartEventThread(ctx, evth_prio, evth_stack, evth_mpid);
                    if (ret >= 0) {
                        ret = matchingStartInputThread(ctx, inth_prio, 3072, inth_mpid);
                        if (ret >= 0) {
                            if (ctx->mode == MATCHING_MODE_HOST || ctx->mode == MATCHING_MODE_PTP) {
                                ret = matchingBuildHelloPkt(ctx, optlen, opt);
                                if (ret >= 0) {
                                    u32 now = sceKernelGetSystemTimeLow();
                                    matchingSetHelloAlarm(ctx, now / (0xFFFFFFFFu / ctx->hello_delay + 1u));
                                }
                            }
                            if (ret >= 0) {
                                matchingBuildMembersPkt(ctx);
                                ctx->min_stack = evth_stack - 3072;
                                ctx->running = 2;
                                matchingUnlock(lock_res);
                                return 0;
                            }
                            matchingStopInputThread(ctx, &lock_res);
                        }
                        matchingStopEventThread(ctx, &lock_res);
                    }
                    matchingDeletePdp(ctx);
                }
            } else {
                ret = MATCHING_ERR_INVALID_OPTLEN;
            }
        }
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x0013cc */
static int __attribute__((noinline))
matchingCancelTargetInternal(int id, const u8 *target, int optlen, const void *opt)
{
    int lock_res;
    int ret = 0;
    void *opt_copy = NULL;
    MatchingCtx *ctx;

    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (target == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(id);
    if (ctx == NULL) {
        ret = MATCHING_ERR_INVALID_ID;
    } else if (ctx->running != 2) {
        ret = MATCHING_ERR_NOT_RUNNING;
    } else {
        MatchingPeer *peer = matchingFindPeerByMac(ctx, target);
        if (peer == NULL) {
            ret = MATCHING_ERR_UNKNOWN_TARGET;
        } else if ((u32)optlen > 0xFFD7u || (optlen > 0 && opt == NULL)) {
            ret = MATCHING_ERR_INVALID_OPTLEN;
        } else if (peer->state != PEER_STATE_IDLE) {
            int ok = 1;
            if (peer->state > 0 && peer->state < 6 && optlen > 0) {
                opt_copy = matchingAllocVpl(optlen);
                if (opt_copy == NULL) {
                    ret = MATCHING_ERR_NO_SPACE;
                    ok = 0;
                } else {
                    sceKernelMemcpy(opt_copy, opt, (u32)optlen);
                }
            }
            if (ok) {
                matchingCancelPeerAlarms(peer);
                matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, optlen, opt);
                matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
                if (peer->opt_len > 0) {
                    matchingFreeVpl(peer->opt_buf);
                    peer->opt_len = 0;
                    peer->opt_buf = NULL;
                }
                if (optlen > 0) {
                    peer->opt_buf = opt_copy;
                    peer->opt_len = optlen;
                }
            }
        }
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x001598 */
static int __attribute__((noinline))
matchingStartEventThread(MatchingCtx *ctx, int prio, int stack, int mpid)
{
    MatchingThreadOpt opt;
    MatchingCtx *arg = ctx;
    int mbx;
    int thid;
    int ret;

    mbx = sceKernelCreateMbx("SceNetAdhocMatchingEvent", 0, NULL);
    arg->event_mbx = mbx;
    if (mbx < 0)
        return arg->event_mbx;

    sceKernelMemset(&opt, 0, sizeof(opt));
    opt.size = sizeof(opt);
    opt.stack_mpid = mpid;
    thid = sceKernelCreateThread("SceNetAdhocMatchingEvent", (void *)matchingEventThread, prio, stack, 0, &opt);
    arg->event_thid = thid;
    if (thid < 0) {
        ret = arg->event_thid;
    } else {
        ret = sceKernelStartThread(arg->event_thid, 4, &arg);
        if (ret >= 0)
            return 0;
        sceKernelDeleteThread(arg->event_thid);
    }
    sceKernelDeleteMbx(arg->event_mbx);
    return ret;
}

/* 0x0016ac */
static int __attribute__((noinline))
matchingStopEventThread(MatchingCtx *ctx, int *lock_res)
{
    sceKernelDeleteMbx(ctx->event_mbx);
    matchingUnlock(*lock_res);
    sceKernelWaitThreadEnd(ctx->event_thid, NULL);
    *lock_res = matchingLock();
    sceNetFreeThreadinfo(ctx->event_thid);
    return sceKernelDeleteThread(ctx->event_thid);
}

/* 0x001710 */
static int matchingEventThread(u32 arglen, void *argp)
{
    MatchingCtx *ctx = *(MatchingCtx **)argp;
    void *raw_msg;

    (void)arglen;
    while (sceKernelReceiveMbx(ctx->event_mbx, &raw_msg, NULL) >= 0) {
        MatchingMbxMsg *msg = (MatchingMbxMsg *)raw_msg;
        int lock_res = matchingLock();
        int msg_type = msg->msg_type;
        MatchingPeer *peer = msg->peer;
        int intr = sceKernelCpuSuspendIntr();
        msg->pending_flag &= ~1u;
        sceKernelCpuResumeIntr(intr);

        switch (msg_type) {
        case MBX_MSG_PACKET:
            matchingProcessPacket(ctx, peer);
            if (peer->rx_pkt != NULL)
                matchingFreeVpl(peer->rx_pkt);
            peer->rx_raw_len = 0;
            peer->rx_pkt = NULL;
            peer->rx_wire_len = 0;
            break;
        case MBX_MSG_CTRL_RETRY:
            matchingHandleCtrlRetry(ctx, peer);
            break;
        case MBX_MSG_KEEPALIVE:
            if (peer->state == PEER_STATE_ESTABLISHED) {
                peer->ping_retry--;
                if (peer->ping_retry > 0) {
                    if (ctx->mode == MATCHING_MODE_HOST ||
                        (ctx->mode == MATCHING_MODE_PTP && matchingCompareMac(peer) != 0)) {
                        matchingSendMembersPkt(ctx, peer);
                    }
                    matchingSetKeepAliveAlarm(ctx, peer);
                } else {
                    matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
                    matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, 0, NULL);
                    matchingInvokeHandler(ctx, MATCHING_EV_TIMEOUT, peer->mac, 0, NULL);
                }
            }
            break;
        case MBX_MSG_HELLO:
            if (matchingCountPeersAtLeast(ctx, PEER_STATE_ACCEPTED) + 1 < ctx->max_peers)
                matchingBroadcastHello(ctx);
            matchingSetHelloAlarm(ctx, ctx->hello_delay);
            break;
        case MBX_MSG_DATA_RETRY:
            if (peer->data_state == DATA_STATE_SENDING) {
                peer->data_retry--;
                if (peer->data_retry <= 0) {
                    matchingSetPeerDataState(ctx, peer, DATA_STATE_IDLE);
                    matchingInvokeHandler(ctx, MATCHING_EV_DATA_TIMEOUT, peer->mac, 0, NULL);
                } else {
                    matchingSendDataPkt(ctx, peer, PKT_DATA, peer->data_len, peer->data_buf);
                    matchingSetDataRetryAlarm(ctx, peer);
                }
            }
            break;
        default:
            break;
        }

        if (peer != NULL &&
            peer->pending_free == 1 &&
            (peer->rx_msg.pending_flag & 1u) == 0 &&
            (peer->ctrl_alarm_msg.pending_flag & 1u) == 0) {
            matchingRemovePeer(ctx, peer);
        }
        matchingUnlock(lock_res);
    }
    return 0;
}

/* 0x001a40 */
static void __attribute__((noinline))
matchingProcessPacket(MatchingCtx *ctx, MatchingPeer *peer)
{
    u32 pkt_sess_id = 0;
    u32 pkt_seq;
    int mode = ctx->mode;
    u32 pkt_type = peer->rx_pkt[1];
    int active_count;

    if (mode == MATCHING_MODE_CLIENT) {
        if (pkt_type == PKT_JOIN || pkt_type == PKT_KEEPALIVE_ACK)
            return;
    } else if (mode == MATCHING_MODE_HOST) {
        if (pkt_type == PKT_HELLO || pkt_type == PKT_KEEPALIVE)
            return;
    } else if (mode == MATCHING_MODE_PTP) {
        u32 blocked = (matchingCompareMac(peer) != 0) ? PKT_KEEPALIVE : PKT_KEEPALIVE_ACK;
        if (pkt_type == blocked)
            return;
    }

    if (pkt_type == PKT_JOIN || pkt_type == PKT_ACCEPT) {
        int wire_len = peer->rx_wire_len;
        if (peer->rx_raw_len - wire_len >= 16) {
            sceKernelMemcpy(&pkt_sess_id, peer->rx_pkt + wire_len, 4);
            pkt_sess_id = matchingSwap32(pkt_sess_id);
            if (pkt_sess_id != peer->remote_session_id) {
                switch (peer->state) {
                case PEER_STATE_JOIN_RECV:
                case PEER_STATE_ACCEPTED:
                    matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
                    matchingCancelPeerAlarms(peer);
                    matchingInvokeHandler(ctx, MATCHING_EV_CANCEL, peer->mac, 0, NULL);
                    break;
                case PEER_STATE_ESTABLISHED:
                    matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
                    matchingCancelPeerAlarms(peer);
                    matchingInvokeHandler(ctx, MATCHING_EV_LEFT, peer->mac, 0, NULL);
                    break;
                default:
                    break;
                }
            }
        }
    }

    active_count = matchingCountPeersAtLeast(ctx, PEER_STATE_ACCEPTED);
    switch (pkt_type) {
    case PKT_HELLO:
        if (peer->rx_wire_len - 4 >= 8 && peer->state == PEER_STATE_IDLE) {
            u32 h_delay, p_delay;
            int wire_len;
            sceKernelMemcpy(&h_delay, peer->rx_pkt + 4, 4);
            peer->peer_hello_delay = matchingSwap32(h_delay);
            sceKernelMemcpy(&p_delay, peer->rx_pkt + 8, 4);
            peer->peer_ping_delay = matchingSwap32(p_delay);
            wire_len = peer->rx_wire_len;
            if (peer->rx_raw_len - wire_len >= 16)
                sceKernelMemcpy(&peer->peer_caps, peer->rx_pkt + wire_len, 4);
            if (active_count + 1 < ctx->max_peers) {
                int optlen = peer->rx_wire_len - 12;
                void *opt = peer->rx_pkt + 12;
                if (optlen > 0)
                    matchingInvokeHandler(ctx, MATCHING_EV_HELLO, peer->mac, optlen, opt);
                else
                    matchingInvokeHandler(ctx, MATCHING_EV_HELLO, peer->mac, 0, NULL);
            }
        }
        break;
    case PKT_JOIN:
        switch (peer->state) {
        case PEER_STATE_IDLE:
            if (active_count + 1 >= ctx->max_peers) {
                matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, 0, NULL);
            } else {
                int optlen = peer->rx_wire_len - 4;
                void *opt = peer->rx_pkt + 4;
                matchingSetPeerState(ctx, peer, PEER_STATE_JOIN_RECV);
                peer->remote_session_id = pkt_sess_id;
                matchingSendCtrlPkt(ctx, peer, PKT_JOIN_ACK, 0, NULL);
                if (optlen > 0)
                    matchingInvokeHandler(ctx, MATCHING_EV_JOIN, peer->mac, optlen, opt);
                else
                    matchingInvokeHandler(ctx, MATCHING_EV_JOIN, peer->mac, 0, NULL);
            }
            break;
        case PEER_STATE_JOIN_RECV:
            if (active_count + 1 < ctx->max_peers) {
                matchingSendCtrlPkt(ctx, peer, PKT_JOIN_ACK, 0, NULL);
            } else {
                matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
                matchingCancelPeerAlarms(peer);
                matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, 0, NULL);
                matchingInvokeHandler(ctx, MATCHING_EV_CANCEL, peer->mac, 0, NULL);
            }
            break;
        case PEER_STATE_ACCEPTED:
            matchingSendCtrlPkt(ctx, peer, PKT_ACCEPT, peer->opt_len, peer->opt_buf);
            matchingSetCtrlRetryAlarm(ctx, peer);
            break;
        case PEER_STATE_SELECT_SENT: {
            int optlen = peer->rx_wire_len - 4;
            void *opt = peer->rx_pkt + 4;
            matchingSetPeerState(ctx, peer, PEER_STATE_ACCEPTED);
            peer->remote_session_id = pkt_sess_id;
            matchingSendCtrlPkt(ctx, peer, PKT_ACCEPT, peer->opt_len, peer->opt_buf);
            matchingSetCtrlRetryAlarm(ctx, peer);
            if (optlen > 0)
                matchingInvokeHandler(ctx, MATCHING_EV_ACCEPT, peer->mac, optlen, opt);
            else
                matchingInvokeHandler(ctx, MATCHING_EV_ACCEPT, peer->mac, 0, NULL);
            break;
        }
        case PEER_STATE_ESTABLISHED:
            matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
            matchingCancelPeerAlarms(peer);
            matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ERROR, peer->mac, 0, NULL);
            break;
        default:
            break;
        }
        break;
    case PKT_ACCEPT:
        switch (peer->state) {
        case PEER_STATE_IDLE:
            matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, peer->opt_len, peer->opt_buf);
            break;
        case PEER_STATE_JOIN_RECV:
            matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
            matchingCancelPeerAlarms(peer);
            matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ERROR, peer->mac, 0, NULL);
            break;
        case PEER_STATE_ACCEPTED:
            matchingSetPeerState(ctx, peer, PEER_STATE_ESTABLISHED);
            matchingSendCtrlPkt(ctx, peer, PKT_COMPLETE, 0, NULL);
            matchingSetKeepAliveAlarm(ctx, peer);
            peer->ping_retry = ctx->init_count;
            matchingInvokeHandler(ctx, MATCHING_EV_ESTABLISHED, peer->mac, 0, NULL);
            break;
        case PEER_STATE_SELECT_SENT: {
            int optlen = peer->rx_wire_len - 4;
            void *opt = peer->rx_pkt + 4;
            matchingSetPeerState(ctx, peer, PEER_STATE_ESTABLISHED);
            peer->remote_session_id = pkt_sess_id;
            matchingSendCtrlPkt(ctx, peer, PKT_COMPLETE, 0, NULL);
            matchingSetKeepAliveAlarm(ctx, peer);
            peer->ping_retry = ctx->init_count;
            if (optlen > 0)
                matchingInvokeHandler(ctx, MATCHING_EV_ACCEPT, peer->mac, optlen, opt);
            else
                matchingInvokeHandler(ctx, MATCHING_EV_ACCEPT, peer->mac, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ESTABLISHED, peer->mac, 0, NULL);
            break;
        }
        case PEER_STATE_ESTABLISHED:
            matchingSendCtrlPkt(ctx, peer, PKT_COMPLETE, 0, NULL);
            break;
        default:
            break;
        }
        break;
    case PKT_COMPLETE:
        switch (peer->state) {
        case PEER_STATE_IDLE:
            matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, peer->opt_len, peer->opt_buf);
            break;
        case PEER_STATE_JOIN_RECV:
        case PEER_STATE_SELECT_SENT:
            matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
            matchingCancelPeerAlarms(peer);
            matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ERROR, peer->mac, 0, NULL);
            break;
        case PEER_STATE_ACCEPTED:
            matchingSetPeerState(ctx, peer, PEER_STATE_ESTABLISHED);
            matchingSetKeepAliveAlarm(ctx, peer);
            peer->ping_retry = ctx->init_count;
            matchingInvokeHandler(ctx, MATCHING_EV_ESTABLISHED, peer->mac, 0, NULL);
            break;
        default:
            break;
        }
        break;
    case PKT_CANCEL:
        switch (peer->state) {
        case PEER_STATE_JOIN_RECV:
        case PEER_STATE_ACCEPTED: {
            int optlen = peer->rx_wire_len - 4;
            void *opt = peer->rx_pkt + 4;
            matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
            matchingCancelPeerAlarms(peer);
            if (optlen > 0)
                matchingInvokeHandler(ctx, MATCHING_EV_CANCEL, peer->mac, optlen, opt);
            else
                matchingInvokeHandler(ctx, MATCHING_EV_CANCEL, peer->mac, 0, NULL);
            break;
        }
        case PEER_STATE_SELECT_SENT: {
            int optlen = peer->rx_wire_len - 4;
            void *opt = peer->rx_pkt + 4;
            matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
            matchingCancelPeerAlarms(peer);
            if (optlen > 0)
                matchingInvokeHandler(ctx, MATCHING_EV_REJECT, peer->mac, optlen, opt);
            else
                matchingInvokeHandler(ctx, MATCHING_EV_REJECT, peer->mac, 0, NULL);
            break;
        }
        case PEER_STATE_ESTABLISHED: {
            int optlen = peer->rx_wire_len - 4;
            void *opt = peer->rx_pkt + 4;
            matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
            matchingCancelPeerAlarms(peer);
            if (optlen > 0)
                matchingInvokeHandler(ctx, MATCHING_EV_LEFT, peer->mac, optlen, opt);
            else
                matchingInvokeHandler(ctx, MATCHING_EV_LEFT, peer->mac, 0, NULL);
            break;
        }
        default:
            break;
        }
        break;
    case PKT_KEEPALIVE:
        switch (peer->state) {
        case PEER_STATE_IDLE:
            matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, peer->opt_len, peer->opt_buf);
            break;
        case PEER_STATE_JOIN_RECV:
        case PEER_STATE_SELECT_SENT:
            matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
            matchingCancelPeerAlarms(peer);
            matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ERROR, peer->mac, 0, NULL);
            break;
        case PEER_STATE_ACCEPTED:
            matchingSetPeerState(ctx, peer, PEER_STATE_ESTABLISHED);
            matchingSendCtrlPkt(ctx, peer, PKT_KEEPALIVE_ACK, 0, NULL);
            matchingSetKeepAliveAlarm(ctx, peer);
            peer->ping_retry = ctx->init_count;
            if (ctx->mode == MATCHING_MODE_CLIENT)
                matchingCopyMembersPkt(ctx, peer->rx_pkt, peer->rx_wire_len);
            matchingInvokeHandler(ctx, MATCHING_EV_ESTABLISHED, peer->mac, 0, NULL);
            break;
        case PEER_STATE_ESTABLISHED:
            matchingSendCtrlPkt(ctx, peer, PKT_KEEPALIVE_ACK, 0, NULL);
            peer->ping_retry = ctx->init_count;
            if (ctx->mode == MATCHING_MODE_CLIENT)
                matchingCopyMembersPkt(ctx, peer->rx_pkt, peer->rx_wire_len);
            break;
        default:
            break;
        }
        break;
    case PKT_KEEPALIVE_ACK:
        if (peer->state == PEER_STATE_ESTABLISHED) {
            peer->ping_retry = ctx->init_count;
        } else if (peer->state == PEER_STATE_IDLE) {
            matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, peer->opt_len, peer->opt_buf);
        } else if (peer->state >= PEER_STATE_JOIN_RECV && peer->state < PEER_STATE_ESTABLISHED) {
            matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
            matchingCancelPeerAlarms(peer);
            matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ERROR, peer->mac, 0, NULL);
        }
        break;
    case PKT_BYE:
        if (peer->state >= PEER_STATE_JOIN_RECV && peer->state <= PEER_STATE_ESTABLISHED) {
            matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
            matchingCancelPeerAlarms(peer);
        }
        matchingInvokeHandler(ctx, MATCHING_EV_BYE, peer->mac, 0, NULL);
        peer->pending_free = 1;
        break;
    case PKT_JOIN_ACK:
        if (peer->state == PEER_STATE_SELECT_SENT)
            peer->select_retry = ctx->init_count;
        /* When already established, fall through to DATA/DATA_ACK handling */
        if (peer->state != PEER_STATE_ESTABLISHED)
            break;
        /* fall through */
    case PKT_DATA:
    case PKT_DATA_ACK:
        if (peer->state != PEER_STATE_ESTABLISHED)
            break;
        sceKernelMemcpy(&pkt_sess_id, peer->rx_pkt + 4, 4);
        pkt_sess_id = matchingSwap32(pkt_sess_id);
        sceKernelMemcpy(&pkt_seq, peer->rx_pkt + 8, 4);
        pkt_seq = matchingSwap32(pkt_seq);
        if (pkt_sess_id != peer->remote_session_id)
            break;
        if (pkt_type == PKT_DATA) {
            if (pkt_seq >= peer->rx_expected_seq) {
                int datalen = peer->rx_wire_len - 12;
                void *data = peer->rx_pkt + 12;
                peer->rx_expected_seq = pkt_seq + 1;
                if (datalen > 0)
                    matchingInvokeHandler(ctx, MATCHING_EV_DATA, peer->mac, datalen, data);
                else
                    matchingInvokeHandler(ctx, MATCHING_EV_DATA, peer->mac, 0, NULL);
            }
            matchingSendDataPkt(ctx, peer, PKT_DATA_ACK, 0, NULL);
        } else if (peer->data_state == DATA_STATE_SENDING && pkt_seq == peer->tx_seq) {
            matchingSetPeerDataState(ctx, peer, DATA_STATE_IDLE);
            matchingInvokeHandler(ctx, MATCHING_EV_DATA_CONFIRM, peer->mac, 0, NULL);
        }
        break;
    default:
        break;
    }
}

/* 0x002884 */
static void __attribute__((noinline))
matchingHandleCtrlRetry(MatchingCtx *ctx, MatchingPeer *peer)
{
    if (peer->state == PEER_STATE_SELECT_SENT) {
        if ((peer->peer_caps & 1u) != 0) {
            peer->select_retry--;
            if (peer->select_retry <= 0) {
                matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
                matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, 0, NULL);
                matchingInvokeHandler(ctx, MATCHING_EV_TIMEOUT, peer->mac, 0, NULL);
            } else {
                matchingSendCtrlPkt(ctx, peer, PKT_JOIN, peer->opt_len, peer->opt_buf);
                matchingSetCtrlRetryAlarm(ctx, peer);
            }
        } else {
            matchingSendCtrlPkt(ctx, peer, PKT_JOIN, peer->opt_len, peer->opt_buf);
            matchingSetCtrlRetryAlarm(ctx, peer);
        }
    }
    if (peer->state == PEER_STATE_ACCEPTED) {
        peer->select_retry--;
        if (peer->select_retry <= 0) {
            matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
            matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_TIMEOUT, peer->mac, 0, NULL);
        } else {
            matchingSendCtrlPkt(ctx, peer, PKT_ACCEPT, peer->opt_len, peer->opt_buf);
            matchingSetCtrlRetryAlarm(ctx, peer);
        }
    }
}

/* 0x002a6c */
static int __attribute__((noinline))
matchingSendCtrlPkt(MatchingCtx *ctx, MatchingPeer *peer, int pkt_type, int optlen, const void *opt)
{
    int has_trailer = ((u32)(pkt_type - 2) < 2u);
    int total_len = has_trailer ? (optlen + 20) : (optlen + 4);
    u8 *pkt = (u8 *)matchingAllocVpl(total_len);
    u16 be_len = matchingSwap16((u16)optlen);
    u32 be_sess;
    int ret;

    if (pkt == NULL)
        return MATCHING_ERR_NO_SPACE;

    pkt[0] = 1;
    pkt[1] = (u8)pkt_type;
    sceKernelMemcpy(pkt + 2, &be_len, 2);
    if (optlen > 0)
        sceKernelMemcpy(pkt + 4, opt, (u32)optlen);
    if (has_trailer) {
        be_sess = matchingSwap32(peer->own_session_id);
        sceKernelMemcpy(pkt + optlen + 4, &be_sess, 4);
        sceKernelMemset(pkt + optlen + 8, 0, 12);
    }
    ret = sceNetAdhocPdpSend(ctx->pdp_id, peer->mac, ctx->port, pkt, total_len, 0, 1);
    matchingFreeVpl(pkt);
    return (ret == MATCHING_ERR_PDP_WOULDBLOCK) ? 0 : ret;
}

/* 0x002bc4 */
static int __attribute__((noinline))
matchingCreatePdp(MatchingCtx *ctx)
{
    int pdp = sceNetAdhocPdpCreate(g_self_mac, ctx->port, (u32)ctx->rx_buflen, 0);
    if (pdp >= 0) {
        ctx->pdp_id = pdp;
        return 0;
    }
    return pdp;
}

/* 0x002c10 */
static int __attribute__((noinline))
matchingDeletePdp(MatchingCtx *ctx)
{
    int ret = sceNetAdhocPdpDelete(ctx->pdp_id, 0);
    ctx->pdp_id = 0;
    return ret;
}

/* 0x002c40 */
static int __attribute__((noinline))
matchingSendDataPkt(MatchingCtx *ctx, MatchingPeer *peer, int pkt_type, int datalen, const void *data)
{
    int total_len = datalen + 12;
    u8 *pkt = (u8 *)matchingAllocVpl(total_len);
    u16 be_len = matchingSwap16((u16)(datalen + 8));
    u32 be_val;
    int ret;

    if (pkt == NULL)
        return MATCHING_ERR_NO_SPACE;

    pkt[0] = 1;
    pkt[1] = (u8)pkt_type;
    sceKernelMemcpy(pkt + 2, &be_len, 2);
    be_val = matchingSwap32(peer->own_session_id);
    sceKernelMemcpy(pkt + 4, &be_val, 4);
    be_val = matchingSwap32((pkt_type == PKT_DATA) ? peer->tx_seq : (peer->rx_expected_seq - 1));
    sceKernelMemcpy(pkt + 8, &be_val, 4);
    if (datalen > 0)
        sceKernelMemcpy(pkt + 12, data, (u32)datalen);

    ret = sceNetAdhocPdpSend(ctx->pdp_id, peer->mac, ctx->port, pkt, total_len, 0, 1);
    matchingFreeVpl(pkt);
    return (ret == MATCHING_ERR_PDP_WOULDBLOCK) ? 0 : ret;
}

/* 0x002d88 */
static int __attribute__((noinline))
matchingBuildHelloPkt(MatchingCtx *ctx, int optlen, const void *opt)
{
    int total_len = optlen + 28;
    u8 *pkt = (u8 *)matchingAllocVpl(total_len);
    u16 be_len = matchingSwap16((u16)(optlen + 8));
    u32 be_val;

    if (pkt == NULL)
        return MATCHING_ERR_NO_SPACE;

    pkt[0] = 1;
    pkt[1] = PKT_HELLO;
    sceKernelMemcpy(pkt + 2, &be_len, 2);
    be_val = matchingSwap32(ctx->hello_delay);
    sceKernelMemcpy(pkt + 4, &be_val, 4);
    be_val = matchingSwap32(ctx->ping_delay);
    sceKernelMemcpy(pkt + 8, &be_val, 4);
    if (optlen > 0)
        sceKernelMemcpy(pkt + 12, opt, (u32)optlen);
    sceKernelMemcpy(pkt + optlen + 12, ctx->hello_trailer, 4);
    sceKernelMemset(pkt + optlen + 16, 0, 12);

    if (ctx->hello_pkt != NULL)
        matchingFreeVpl(ctx->hello_pkt);
    ctx->hello_pkt = pkt;
    ctx->hello_pkt_len = total_len;
    return 0;
}

/* 0x002ebc */
static void __attribute__((noinline))
matchingFreeHelloPkt(MatchingCtx *ctx)
{
    if (ctx->hello_pkt_len > 0) {
        matchingFreeVpl(ctx->hello_pkt);
        ctx->hello_pkt = NULL;
        ctx->hello_pkt_len = 0;
    }
}

/* 0x002ef4 */
static int __attribute__((noinline))
matchingBroadcastHello(MatchingCtx *ctx)
{
    u8 bcast[16];
    int ret;

    sceKernelMemset(bcast, 0xFF, 6);
    ret = sceNetAdhocPdpSend(ctx->pdp_id, bcast, ctx->port, ctx->hello_pkt, ctx->hello_pkt_len, 0, 1);
    return (ret == MATCHING_ERR_PDP_WOULDBLOCK) ? 0 : ret;
}

/* 0x002f54 */
static int __attribute__((noinline))
matchingBuildMembersPkt(MatchingCtx *ctx)
{
    int count = matchingCountPeersAtLeast(ctx, PEER_STATE_ESTABLISHED);
    int total_len = count * 6 + 10;
    u8 *pkt = (u8 *)matchingAllocVpl(total_len);
    u16 be_len = matchingSwap16((u16)(count * 6 + 6));

    if (pkt == NULL)
        return MATCHING_ERR_NO_SPACE;

    pkt[0] = 1;
    pkt[1] = PKT_KEEPALIVE;
    sceKernelMemcpy(pkt + 2, &be_len, 2);
    sceKernelMemcpy(pkt + 4, g_self_mac, 6);
    matchingCollectPeerMacs(ctx, PEER_STATE_ESTABLISHED, pkt + 10, &count);
    if (ctx->members_pkt != NULL)
        matchingFreeVpl(ctx->members_pkt);
    ctx->members_pkt = pkt;
    ctx->members_pkt_len = total_len;
    return 0;
}

/* 0x003040 */
static int __attribute__((noinline))
matchingSendMembersPkt(MatchingCtx *ctx, MatchingPeer *peer)
{
    int ret = sceNetAdhocPdpSend(ctx->pdp_id, peer->mac, ctx->port, ctx->members_pkt, ctx->members_pkt_len, 0, 1);
    return (ret == MATCHING_ERR_PDP_WOULDBLOCK) ? 0 : ret;
}

/* 0x003088 */
static int __attribute__((noinline))
matchingCopyMembersPkt(MatchingCtx *ctx, const void *pkt, int len)
{
    u8 *buf = (u8 *)matchingAllocVpl(len);
    if (buf == NULL)
        return MATCHING_ERR_NO_SPACE;
    sceKernelMemcpy(buf, pkt, (u32)len);
    if (ctx->members_pkt != NULL)
        matchingFreeVpl(ctx->members_pkt);
    ctx->members_pkt_len = len;
    ctx->members_pkt = buf;
    return 0;
}

/* 0x00311c */
static void __attribute__((noinline))
matchingFreeMembersPkt(MatchingCtx *ctx)
{
    if (ctx->members_pkt_len > 0) {
        matchingFreeVpl(ctx->members_pkt);
        ctx->members_pkt = NULL;
        ctx->members_pkt_len = 0;
    }
}

/* 0x003154 */
static int __attribute__((noinline))
matchingBroadcastBye(MatchingCtx *ctx)
{
    u8 bcast[16];
    u8 pkt[4];
    int ret;

    sceKernelMemset(bcast, 0xFF, 6);
    pkt[0] = 1;
    pkt[1] = PKT_BYE;
    sceKernelMemset(pkt + 2, 0, 2);
    ret = sceNetAdhocPdpSend(ctx->pdp_id, bcast, ctx->port, pkt, 4, 0, 1);
    return (ret == MATCHING_ERR_PDP_WOULDBLOCK) ? 0 : ret;
}

/* 0x0031d4 */
static int __attribute__((noinline))
matchingStartInputThread(MatchingCtx *ctx, int prio, int stack, int mpid)
{
    MatchingThreadOpt opt;
    MatchingCtx *arg = ctx;
    int thid;
    int ret;

    sceKernelMemset(&opt, 0, sizeof(opt));
    opt.size = sizeof(opt);
    opt.stack_mpid = mpid;
    thid = sceKernelCreateThread("SceNetAdhocMatchingInput", (void *)matchingInputThread, prio, stack, 0, &opt);
    arg->input_thid = thid;
    if (thid < 0)
        return arg->input_thid;

    ret = sceKernelStartThread(arg->input_thid, 4, &arg);
    if (ret >= 0)
        return 0;
    sceKernelDeleteThread(arg->input_thid);
    return ret;
}

/* 0x0032a4 */
static int __attribute__((noinline))
matchingStopInputThread(MatchingCtx *ctx, int *lock_res)
{
    sceNetAdhocSetSocketAlert(ctx->pdp_id, 0x20);
    matchingUnlock(*lock_res);
    sceKernelWaitThreadEnd(ctx->input_thid, NULL);
    *lock_res = matchingLock();
    sceNetFreeThreadinfo(ctx->input_thid);
    return sceKernelDeleteThread(ctx->input_thid);
}

/* 0x00330c */
static int matchingInputThread(u32 arglen, void *argp)
{
    MatchingCtx *ctx = *(MatchingCtx **)argp;
    u8 src_mac[16];
    u16 src_port;
    u16 body_len;
    int recv_len;

    (void)arglen;
    for (;;) {
        recv_len = ctx->rx_buflen;
        if (sceNetAdhocPdpRecv(ctx->pdp_id, src_mac, &src_port, ctx->rx_buf, &recv_len, 0, 0) < 0)
            return 0;
        if (sceNet_lib_E0A81C7C(g_self_mac, src_mac, 6) == 0)
            continue;
        if (src_port != ctx->port)
            continue;
        if (ctx->rx_buf[0] != 1)
            continue;
        sceKernelMemcpy(&body_len, ctx->rx_buf + 2, 2);
        {
            int wire_len = (int)matchingSwap16(body_len) + 4;
            int lock_res;
            MatchingPeer *peer;

            if (recv_len < wire_len)
                continue;

            lock_res = matchingLock();
            peer = matchingFindPeerByMac(ctx, src_mac);
            if (peer == NULL) {
                u8 pkt_type = ctx->rx_buf[1];
                if ((pkt_type == PKT_JOIN && (ctx->mode == MATCHING_MODE_HOST || ctx->mode == MATCHING_MODE_PTP)) ||
                    (pkt_type == PKT_HELLO && (u32)(ctx->mode - 2) < 2u)) {
                    peer = matchingCreatePeer(ctx, src_mac);
                }
            }
            if (peer != NULL && (peer->rx_msg.pending_flag & 1u) == 0) {
                u8 *pkt_copy = (u8 *)matchingAllocVpl(recv_len);
                if (pkt_copy != NULL) {
                    sceKernelMemcpy(pkt_copy, ctx->rx_buf, (u32)recv_len);
                    peer->rx_wire_len = wire_len;
                    peer->rx_raw_len = recv_len;
                    peer->rx_pkt = pkt_copy;
                    peer->rx_msg.msg_type = MBX_MSG_PACKET;
                    peer->rx_msg.peer = peer;
                    peer->rx_msg.pending_flag |= 1u;
                    sceKernelSendMbx(ctx->event_mbx, &peer->rx_msg);
                }
            }
            matchingUnlock(lock_res);
        }
    }
}

/* 0x003508 */
static int __attribute__((noinline))
matchingSetCtrlRetryAlarm(MatchingCtx *ctx, MatchingPeer *peer)
{
    int intr = sceKernelCpuSuspendIntr();
    if (peer->ctrl_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->ctrl_alarm_id);
        peer->ctrl_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    peer->ctrl_alarm_id = sceKernelSetAlarm(ctx->msg_delay, matchingCtrlRetryAlarmCb, peer);
    return peer->ctrl_alarm_id;
}

/* 0x003580 */
static int __attribute__((noinline))
matchingSetDataRetryAlarm(MatchingCtx *ctx, MatchingPeer *peer)
{
    int intr = sceKernelCpuSuspendIntr();
    if (peer->data_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->data_alarm_id);
        peer->data_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    peer->data_alarm_id = sceKernelSetAlarm(ctx->msg_delay, matchingDataRetryAlarmCb, peer);
    return peer->data_alarm_id;
}

/* 0x0035f8 */
static int __attribute__((noinline))
matchingSetKeepAliveAlarm(MatchingCtx *ctx, MatchingPeer *peer)
{
    u32 delay = (ctx->mode == MATCHING_MODE_CLIENT) ? peer->peer_ping_delay : ctx->ping_delay;
    int intr = sceKernelCpuSuspendIntr();
    if (peer->ctrl_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->ctrl_alarm_id);
        peer->ctrl_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    peer->ctrl_alarm_id = sceKernelSetAlarm(delay, matchingKeepAliveAlarmCb, peer);
    return peer->ctrl_alarm_id;
}

/* 0x003684 */
static int __attribute__((noinline))
matchingSetHelloAlarm(MatchingCtx *ctx, u32 delay)
{
    int intr = sceKernelCpuSuspendIntr();
    if (ctx->hello_alarm_id >= 0) {
        sceKernelCancelAlarm(ctx->hello_alarm_id);
        ctx->hello_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    ctx->hello_alarm_id = sceKernelSetAlarm(delay, matchingHelloAlarmCb, ctx);
    return ctx->hello_alarm_id;
}

/* 0x0036fc */
static int __attribute__((noinline))
matchingCancelPeerAlarms(MatchingPeer *peer)
{
    int intr = sceKernelCpuSuspendIntr();
    if (peer->ctrl_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->ctrl_alarm_id);
        peer->ctrl_alarm_id = -1;
    }
    if (peer->data_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->data_alarm_id);
        peer->data_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x00376c */
static int __attribute__((noinline))
matchingCancelDataAlarm(MatchingPeer *peer)
{
    int intr = sceKernelCpuSuspendIntr();
    if (peer->data_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->data_alarm_id);
        peer->data_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x0037c0 */
static int __attribute__((noinline))
matchingCancelHelloAlarm(MatchingCtx *ctx)
{
    int intr = sceKernelCpuSuspendIntr();
    if (ctx->hello_alarm_id >= 0) {
        sceKernelCancelAlarm(ctx->hello_alarm_id);
        ctx->hello_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x003814 */
static u32 matchingCtrlRetryAlarmCb(void *common)
{
    MatchingPeer *peer = (MatchingPeer *)common;
    if ((peer->ctrl_alarm_msg.pending_flag & 1u) == 0) {
        peer->ctrl_alarm_msg.pending_flag |= 1u;
        peer->ctrl_alarm_msg.msg_type = MBX_MSG_CTRL_RETRY;
        peer->ctrl_alarm_msg.peer = peer;
        sceKernelSendMbx(peer->event_mbx, &peer->ctrl_alarm_msg);
    }
    peer->ctrl_alarm_id = -1;
    return 0;
}

/* 0x003874 */
static u32 matchingDataRetryAlarmCb(void *common)
{
    MatchingPeer *peer = (MatchingPeer *)common;
    if ((peer->data_alarm_msg.pending_flag & 1u) == 0) {
        peer->data_alarm_msg.pending_flag |= 1u;
        peer->data_alarm_msg.msg_type = MBX_MSG_DATA_RETRY;
        peer->data_alarm_msg.peer = peer;
        sceKernelSendMbx(peer->event_mbx, &peer->data_alarm_msg);
    }
    peer->data_alarm_id = -1;
    return 0;
}

/* 0x0038d4 */
static u32 matchingKeepAliveAlarmCb(void *common)
{
    MatchingPeer *peer = (MatchingPeer *)common;
    if ((peer->ctrl_alarm_msg.pending_flag & 1u) == 0) {
        peer->ctrl_alarm_msg.pending_flag |= 1u;
        peer->ctrl_alarm_msg.msg_type = MBX_MSG_KEEPALIVE;
        peer->ctrl_alarm_msg.peer = peer;
        sceKernelSendMbx(peer->event_mbx, &peer->ctrl_alarm_msg);
    }
    peer->ctrl_alarm_id = -1;
    return 0;
}

/* 0x003934 */
static u32 matchingHelloAlarmCb(void *common)
{
    MatchingCtx *ctx = (MatchingCtx *)common;
    if ((ctx->hello_msg.pending_flag & 1u) == 0) {
        ctx->hello_msg.pending_flag |= 1u;
        ctx->hello_msg.msg_type = MBX_MSG_HELLO;
        ctx->hello_msg.peer = NULL;
        sceKernelSendMbx(ctx->event_mbx, &ctx->hello_msg);
    }
    ctx->hello_alarm_id = -1;
    return 0;
}

/* 0x003994 */
static void __attribute__((noinline))
matchingRemovePeer(MatchingCtx *ctx, MatchingPeer *peer)
{
    MatchingPeer *prev = NULL;
    MatchingPeer *cur = ctx->peers;

    while (cur != NULL && cur != peer) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == NULL)
        return;

    if (prev != NULL)
        prev->next = cur->next;
    else
        ctx->peers = cur->next;

    if (peer->opt_len > 0)
        sceKernelFreeVpl(g_vpl_id, peer->opt_buf);
    if (peer->rx_pkt != NULL)
        sceKernelFreeVpl(g_vpl_id, peer->rx_pkt);
    if (peer->data_len > 0)
        sceKernelFreeVpl(g_vpl_id, peer->data_buf);
    sceKernelFreeVpl(g_vpl_id, peer);
}

/* 0x003a58 */
static void __attribute__((noinline))
matchingSetPeerState(MatchingCtx *ctx, MatchingPeer *peer, int new_state)
{
    int old_state = peer->state;
    if (old_state == new_state)
        return;

    if ((old_state == PEER_STATE_ACCEPTED || old_state == PEER_STATE_SELECT_SENT) &&
        new_state != PEER_STATE_ACCEPTED && new_state != PEER_STATE_SELECT_SENT &&
        peer->opt_len > 0) {
        sceKernelFreeVpl(g_vpl_id, peer->opt_buf);
        peer->opt_len = 0;
        peer->opt_buf = NULL;
    }
    if (old_state != PEER_STATE_ESTABLISHED && new_state == PEER_STATE_ESTABLISHED) {
        peer->rx_expected_seq = 0;
        peer->tx_seq = 0;
    }
    if (old_state == PEER_STATE_ESTABLISHED && new_state != PEER_STATE_ESTABLISHED)
        matchingSetPeerDataState(ctx, peer, DATA_STATE_IDLE);

    peer->state = new_state;
    if (old_state == PEER_STATE_ESTABLISHED || new_state == PEER_STATE_ESTABLISHED)
        matchingBuildMembersPkt(ctx);
}

/* 0x003b50 */
static void __attribute__((noinline))
matchingSetPeerDataState(MatchingCtx *ctx, MatchingPeer *peer, int new_state)
{
    int old_state = peer->data_state;
    int closing = (old_state == DATA_STATE_SENDING) & (new_state == DATA_STATE_IDLE);
    (void)ctx;
    if (old_state != new_state) {
        if (closing && peer->data_len > 0) {
            sceKernelFreeVpl(g_vpl_id, peer->data_buf);
            peer->data_len = 0;
            peer->data_buf = NULL;
        }
        peer->data_state = new_state;
    }
}

/* 0x003bc4 */
static int __attribute__((noinline))
matchingCreateLock(void)
{
    int ret = sceKernelCreateLwMutex(g_mutex_work, "SceNetAdhocMatchingLock", 0x200, 0, NULL);
    if (ret >= 0) {
        g_mutex_created = 1;
        return 0;
    }
    return ret;
}

/* 0x003c14 */
static void __attribute__((noinline))
matchingDeleteLock(void)
{
    if (g_mutex_created != 0)
        sceKernelDeleteLwMutex(g_mutex_work);
    g_mutex_created = 0;
}

/* 0x003c58 */
static int __attribute__((noinline))
matchingLock(void)
{
    return sceKernelLockLwMutex(g_mutex_work, 1, NULL);
}

/* 0x003c80 */
static void __attribute__((noinline))
matchingUnlock(int lock_res)
{
    if (lock_res == 0)
        sceKernelUnlockLwMutex(g_mutex_work, 1);
}

/* 0x003cb8 */
static int __attribute__((noinline))
matchingInitCtxList(void)
{
    g_ctx_list = NULL;
    return 0;
}

/* 0x003cc8 */
static int __attribute__((noinline))
matchingFreeAllCtxs(void)
{
    MatchingCtx *cur = g_ctx_list;
    while (cur != NULL) {
        MatchingCtx *next = cur->next;
        sceKernelFreeVpl(g_vpl_id, cur->rx_buf);
        cur->rx_buf = NULL;
        sceKernelFreeVpl(g_vpl_id, cur);
        cur = next;
    }
    return 0;
}

/* 0x003d34 */
static int __attribute__((noinline))
matchingCheckNoActiveCtx(void)
{
    MatchingCtx *cur = g_ctx_list;
    while (cur != NULL) {
        if (cur->running != 0)
            return MATCHING_ERR_BUSY;
        cur = cur->next;
    }
    return 0;
}

/* 0x003d6c */
static int __attribute__((noinline))
matchingCreateVpl(int poolsize)
{
    int vpl = sceKernelCreateVpl("SceNetAdhocMatchingMemoryPool", 2, 0, (u32)poolsize, NULL);
    g_vpl_id = vpl;
    if (vpl >= 0) {
        g_pool_max_alloc = 0;
        return 0;
    }
    return vpl;
}

/* 0x003dbc */
static void __attribute__((noinline))
matchingDeleteVpl(void)
{
    if (g_vpl_id >= 0) {
        sceKernelDeleteVpl(g_vpl_id);
        g_vpl_id = -1;
    }
}

/* 0x003df8 */
static void *__attribute__((noinline))
matchingAllocVpl(int size)
{
    void *ptr;
    MatchingVplInfo info;
    if (sceKernelTryAllocateVpl(g_vpl_id, (u32)size, &ptr) < 0)
        return NULL;
    info.size = sizeof(info);
    if (sceKernelReferVplStatus(g_vpl_id, &info) == 0) {
        int used = info.pool_size - info.free_size;
        if (used > g_pool_max_alloc)
            g_pool_max_alloc = used;
    }
    return ptr;
}

/* 0x003e6c */
static int __attribute__((noinline))
matchingFreeVpl(void *ptr)
{
    return sceKernelFreeVpl(g_vpl_id, ptr);
}

/* 0x003e94 */
static int __attribute__((noinline))
matchingGetPoolMaxAllocInternal(void)
{
    return g_pool_max_alloc;
}

/* 0x003ea0 */
static int __attribute__((noinline))
matchingGetPoolStatInternal(MatchingPoolStat *stat)
{
    MatchingVplInfo info;
    int ret;

    info.size = sizeof(info);
    ret = sceKernelReferVplStatus(g_vpl_id, &info);
    if (ret >= 0) {
        stat->size = (u32)info.pool_size;
        stat->maxsize = (u32)g_pool_max_alloc;
        stat->freesize = (u32)info.free_size;
        ret = 0;
    }
    return ret;
}

/* 0x003f04 */
static int __attribute__((noinline))
matchingAllocCtx(u16 port)
{
    MatchingCtx *cur = g_ctx_list;
    int cand;
    int chosen = MATCHING_ERR_ID_NOT_AVAIL;
    MatchingCtx *ctx;
    void *raw = NULL;
    MatchingVplInfo info;

    while (cur != NULL) {
        if (cur->port == port)
            return MATCHING_ERR_PORT_IN_USE;
        cur = cur->next;
    }

    cand = (g_last_ctx_id == 15) ? 1 : (g_last_ctx_id + 1);
    while (cand != g_last_ctx_id) {
        MatchingCtx *it = g_ctx_list;
        while (it != NULL && it->id != cand)
            it = it->next;
        if (it == NULL) {
            chosen = cand;
            g_last_ctx_id = cand;
            break;
        }
        cand = (cand == 15) ? 1 : (cand + 1);
    }

    if (chosen < 0)
        return chosen;

    if (sceKernelTryAllocateVpl(g_vpl_id, sizeof(MatchingCtx), &raw) < 0)
        return MATCHING_ERR_NO_SPACE;
    info.size = sizeof(info);
    if (sceKernelReferVplStatus(g_vpl_id, &info) == 0) {
        int used = info.pool_size - info.free_size;
        if (used > g_pool_max_alloc)
            g_pool_max_alloc = used;
    }
    ctx = (MatchingCtx *)raw;
    if (ctx == NULL)
        return MATCHING_ERR_NO_SPACE;

    sceKernelMemset(ctx, 0, sizeof(MatchingCtx));
    ctx->id = chosen;
    ctx->next = g_ctx_list;
    g_ctx_list = ctx;
    return chosen;
}

/* 0x00407c */
static int __attribute__((noinline))
matchingFreeCtx(MatchingCtx *ctx)
{
    MatchingCtx *cur = g_ctx_list;
    MatchingCtx *prev = NULL;

    if (cur != NULL) {
        while (cur != NULL && cur != ctx) {
            prev = cur;
            cur = cur->next;
        }
        if (cur != NULL) {
            if (prev != NULL)
                prev->next = ctx->next;
            else
                g_ctx_list = ctx->next;
            cur->next = NULL;
        }
    }
    return sceKernelFreeVpl(g_vpl_id, ctx);
}

/* 0x0040e8 */
static MatchingCtx *__attribute__((noinline))
matchingFindCtxById(int id)
{
    MatchingCtx *cur = g_ctx_list;
    while (cur != NULL && cur->id != id)
        cur = cur->next;
    return cur;
}

/* 0x004118 */
static MatchingPeer *__attribute__((noinline))
matchingCreatePeer(MatchingCtx *ctx, const u8 *mac)
{
    MatchingPeer *peer;
    void *raw = NULL;
    MatchingVplInfo info;
    u32 sess;

    if (sceKernelTryAllocateVpl(g_vpl_id, sizeof(MatchingPeer), &raw) < 0)
        return NULL;
    info.size = sizeof(info);
    if (sceKernelReferVplStatus(g_vpl_id, &info) == 0) {
        int used = info.pool_size - info.free_size;
        if (used > g_pool_max_alloc)
            g_pool_max_alloc = used;
    }
    peer = (MatchingPeer *)raw;
    if (peer == NULL)
        return NULL;

    sceKernelMemset(peer, 0, sizeof(MatchingPeer));
    matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
    sceKernelMemcpy(peer->mac, mac, 6);
    peer->event_mbx = ctx->event_mbx;
    sess = sceNet_lib_1858883D();
    peer->own_session_id = (sess == 0) ? 1u : sess;
    peer->ctrl_alarm_id = -1;
    matchingSetPeerDataState(ctx, peer, DATA_STATE_IDLE);
    peer->data_alarm_id = -1;
    peer->next = ctx->peers;
    ctx->peers = peer;
    return peer;
}

/* 0x004230 */
static MatchingPeer *__attribute__((noinline))
matchingFindPeerByMac(MatchingCtx *ctx, const u8 *mac)
{
    MatchingPeer *cur = ctx->peers;
    while (cur != NULL) {
        if (sceNet_lib_E0A81C7C(cur->mac, mac, 6) == 0 && cur->pending_free == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

/* 0x004294 */
static int __attribute__((noinline))
matchingDestroyAllPeers(MatchingCtx *ctx)
{
    MatchingPeer *cur = ctx->peers;
    while (cur != NULL) {
        MatchingPeer *next = cur->next;
        matchingCancelPeerAlarms(cur);
        if (cur->opt_len > 0)
            sceKernelFreeVpl(g_vpl_id, cur->opt_buf);
        if (cur->data_len > 0)
            sceKernelFreeVpl(g_vpl_id, cur->data_buf);
        if (cur->rx_pkt != NULL)
            sceKernelFreeVpl(g_vpl_id, cur->rx_pkt);
        sceKernelFreeVpl(g_vpl_id, cur);
        cur = next;
    }
    ctx->peers = NULL;
    return matchingBroadcastBye(ctx);
}

/* 0x004354 */
static void __attribute__((noinline))
matchingCollectPeerMacs(MatchingCtx *ctx, int min_state, u8 *out, int *count_inout)
{
    MatchingPeer *cur = ctx->peers;
    int count = 0;
    u8 *dst = out;

    while (cur != NULL) {
        if (cur->state >= min_state) {
            if (out != NULL) {
                if (count >= *count_inout) {
                    *count_inout = count;
                    return;
                }
                sceKernelMemcpy(dst, cur->mac, 6);
            }
            dst += 6;
            count++;
        }
        cur = cur->next;
    }
    *count_inout = count;
}

/* 0x004404 */
static int __attribute__((noinline))
matchingCountPeersAtLeast(MatchingCtx *ctx, int min_state)
{
    MatchingPeer *cur = ctx->peers;
    int count = 0;
    while (cur != NULL) {
        if (cur->state >= min_state)
            count++;
        cur = cur->next;
    }
    return count;
}

/* 0x00443c */
static void __attribute__((noinline))
matchingGetMembersInternal(MatchingCtx *ctx, int *buflen, void *buf)
{
    int written = 0;
    int off = 4;
    MatchingMember *prev = NULL;
    MatchingMember *base = (MatchingMember *)buf;

    while (off < ctx->members_pkt_len) {
        int next_len = written + (int)sizeof(MatchingMember);
        if (buf != NULL) {
            MatchingMember *slot = &base[written / (int)sizeof(MatchingMember)];
            if (*buflen < next_len) {
                *buflen = written;
                return;
            }
            sceKernelMemcpy(slot->mac, ctx->members_pkt + off, 6);
            slot->next = NULL;
            if (prev != NULL)
                prev->next = slot;
            prev = slot;
        }
        off += 6;
        written = next_len;
    }
    *buflen = written;
}

/* 0x004504 */
static int __attribute__((noinline))
matchingCompareMac(const MatchingPeer *peer)
{
    u32 peer_hi = ((u32)peer->mac[0] << 16) | ((u32)peer->mac[1] << 8) | peer->mac[2];
    u32 self_hi = ((u32)g_self_mac[0] << 16) | ((u32)g_self_mac[1] << 8) | g_self_mac[2];
    u32 peer_lo;
    u32 self_lo;

    if (self_hi < peer_hi)
        return 1;
    if (peer_hi < self_hi)
        return 0;

    peer_lo = ((u32)peer->mac[3] << 16) | ((u32)peer->mac[4] << 8) | peer->mac[5];
    self_lo = ((u32)g_self_mac[3] << 16) | ((u32)g_self_mac[4] << 8) | g_self_mac[5];
    if (self_lo < peer_lo)
        return 1;
    if (peer_lo < self_lo)
        return 0;
    return -1;
}
