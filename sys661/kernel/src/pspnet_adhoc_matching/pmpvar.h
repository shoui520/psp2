#ifndef PMPVAR_H
#define PMPVAR_H

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


#include "pmp.h"

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


extern int g_vpl_id;
extern int g_initialized;
extern int g_pool_max_alloc;
extern int g_mutex_created;
extern MatchingCtx *g_ctx_list;
extern int g_last_ctx_id;
extern u8 g_mutex_work[0x20];
extern u8 g_self_mac[6];

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
int matchingStartInternal(int id, int evth_prio, int evth_stack, int evth_mpid, int inth_prio, int inth_stack, int inth_mpid, int optlen, const void *opt);
int matchingCancelTargetInternal(int id, const u8 *target, int optlen, const void *opt);
int matchingStartEventThread(MatchingCtx *ctx, int prio, int stack, int mpid);
int matchingStopEventThread(MatchingCtx *ctx, int *lock_res);
int matchingEventThread(u32 arglen, void *argp);
void matchingProcessPacket(MatchingCtx *ctx, MatchingPeer *peer);
void matchingHandleCtrlRetry(MatchingCtx *ctx, MatchingPeer *peer);
int matchingSendCtrlPkt(MatchingCtx *ctx, MatchingPeer *peer, int pkt_type, int optlen, const void *opt);
int matchingCreatePdp(MatchingCtx *ctx);
int matchingDeletePdp(MatchingCtx *ctx);
int matchingSendDataPkt(MatchingCtx *ctx, MatchingPeer *peer, int pkt_type, int datalen, const void *data);
int matchingBuildHelloPkt(MatchingCtx *ctx, int optlen, const void *opt);
void matchingFreeHelloPkt(MatchingCtx *ctx);
int matchingBroadcastHello(MatchingCtx *ctx);
int matchingBuildMembersPkt(MatchingCtx *ctx);
int matchingSendMembersPkt(MatchingCtx *ctx, MatchingPeer *peer);
int matchingCopyMembersPkt(MatchingCtx *ctx, const void *pkt, int len);
void matchingFreeMembersPkt(MatchingCtx *ctx);
int matchingBroadcastBye(MatchingCtx *ctx);
int matchingStartInputThread(MatchingCtx *ctx, int prio, int stack, int mpid);
int matchingStopInputThread(MatchingCtx *ctx, int *lock_res);
int matchingInputThread(u32 arglen, void *argp);
int matchingSetCtrlRetryAlarm(MatchingCtx *ctx, MatchingPeer *peer);
int matchingSetDataRetryAlarm(MatchingCtx *ctx, MatchingPeer *peer);
int matchingSetKeepAliveAlarm(MatchingCtx *ctx, MatchingPeer *peer);
int matchingSetHelloAlarm(MatchingCtx *ctx, u32 delay);
int matchingCancelPeerAlarms(MatchingPeer *peer);
int matchingCancelDataAlarm(MatchingPeer *peer);
int matchingCancelHelloAlarm(MatchingCtx *ctx);
u32 matchingCtrlRetryAlarmCb(void *common);
u32 matchingDataRetryAlarmCb(void *common);
u32 matchingKeepAliveAlarmCb(void *common);
u32 matchingHelloAlarmCb(void *common);
void matchingRemovePeer(MatchingCtx *ctx, MatchingPeer *peer);
void matchingSetPeerState(MatchingCtx *ctx, MatchingPeer *peer, int new_state);
void matchingSetPeerDataState(MatchingCtx *ctx, MatchingPeer *peer, int new_state);
int matchingCreateLock(void);
void matchingDeleteLock(void);
int matchingLock(void);
void matchingUnlock(int lock_res);
int matchingInitCtxList(void);
int matchingFreeAllCtxs(void);
int matchingCheckNoActiveCtx(void);
int matchingCreateVpl(int poolsize);
void matchingDeleteVpl(void);
void *matchingAllocVpl(int size);
int matchingFreeVpl(void *ptr);
int matchingGetPoolMaxAllocInternal(void);
int matchingGetPoolStatInternal(MatchingPoolStat *stat);
int matchingAllocCtx(u16 port);
int matchingFreeCtx(MatchingCtx *ctx);
MatchingCtx *matchingFindCtxById(int id);
MatchingPeer *matchingCreatePeer(MatchingCtx *ctx, const u8 *mac);
MatchingPeer *matchingFindPeerByMac(MatchingCtx *ctx, const u8 *mac);
int matchingDestroyAllPeers(MatchingCtx *ctx);
void matchingCollectPeerMacs(MatchingCtx *ctx, int min_state, u8 *out, int *count_inout);
int matchingCountPeersAtLeast(MatchingCtx *ctx, int min_state);
void matchingGetMembersInternal(MatchingCtx *ctx, int *buflen, void *buf);
int matchingCompareMac(const MatchingPeer *peer);


#endif /* PMPVAR_H */
