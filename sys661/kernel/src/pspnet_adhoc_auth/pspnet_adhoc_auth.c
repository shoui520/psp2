/*
 * ePSP 6.61 sceNetAdhocAuth_Service (pspnet_adhoc_auth.prx)
 */

#include <stdint.h>
#include <stddef.h>

/* Imports from InterruptManagerForKernel */
int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int state);

/* Imports from SysMemForKernel */
int sceKernelGetModel(void);
int sceKernelGetCompiledSdkVersion(void);
int sceKernelDevkitVersion(void);

/* Imports from SysclibForKernel */
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

/* Imports from ThreadManForKernel */
int sceKernelReceiveMbx(int mbxid, void **pmessage, uint32_t *timeout);
int sceKernelWaitThreadEnd(int thid, uint32_t *timeout);
int sceKernelDeleteSema(int semaid);
int sceKernelGetThreadId(void);
int sceKernelSignalSema(int semaid, int signal);
int sceKernelCreateThread(const char *name, int (*entry)(uint32_t, void *), int init_priority,
                          int stack_size, uint32_t attr, void *option);
int sceKernelWaitSema(int semaid, int signal, uint32_t *timeout);
int sceKernelTerminateThread(int thid);
int sceKernelSetAlarm(uint32_t clock, uint32_t (*handler)(void *), void *common);
int sceKernelCancelAlarm(int alarmid);
int sceKernelCreateMbx(const char *name, uint32_t attr, void *option);
uint64_t sceKernelGetSystemTimeWide(void);
int sceKernelDeleteMbx(int mbxid);
int sceKernelDeleteThread(int thid);
int sceKernelCreateSema(const char *name, uint32_t attr, int init_val, int max_val, void *option);
int sceKernelSendMbx(int mbxid, void *message);
int sceKernelStartThread(int thid, uint32_t arglen, void *argp);

/* Imports from sceMemab_driver */
int sceMemab_driver_16594684(void *buf);
int sceMemab_driver_23E4659B(void *ctx, const void *pkt, void *out);
int sceMemab_driver_3C15BC8C(void *ctx, void *payload, int payload_len, void *hdr, int flag);
int sceMemab_driver_4B54EAAD(void *ctx, void *out);
int sceMemab_driver_6DD7339A(void);
int sceMemab_driver_8ABE3445(void *ctx, const void *pkt);
int sceMemab_driver_9BF0C95D(void *ctx, const void *pkt, void *out);
int sceMemab_driver_9BF1A0A4(const void *key, void *dst, uint32_t len);
int sceMemab_driver_9DE8C8CD(const void *key, void *dst, uint32_t len);
int sceMemab_driver_C3981EE1(void *ctx, const void *pkt, void *out);
int sceMemab_driver_CB5D3916(void *ctx, void *payload, int payload_len, void *hdr, int f1, int f2);
int sceMemab_driver_D47A50B1(void *ctx, void *payload, int payload_len, void *hdr, int f1, int f2);
void sceMemab_driver_D57856A7(void);
int sceMemab_driver_F742F283(void *payload, int payload_len, void *hdr, int flag);

/* Imports from sceNetIfhandle_driver */
int sceNetIfhandle_driver_0542835F(int (*rx_filter)(void *), int (*tx_filter)(void *), int (*ctrl_rx)(void *));
void sceNetIfhandle_driver_1560F143(void *mbuf, int offset, int len, const void *src);
void *sceNetIfhandle_driver_15CFE3C0(int size);
void sceNetIfhandle_driver_76BAD213(void *buf);
void sceNetIfhandle_driver_773FC77C(void);
void *sceNetIfhandle_driver_9A6261EC(void *mbuf, int offset, int len, void *dst);
void *sceNetIfhandle_driver_9CBA24D4(const char *ifname);
int sceNetIfhandle_driver_C5623112(void *ifp, uint8_t *mac_out);
void sceNetIfhandle_driver_F56FAC82(void *mbuf);
void sceNetIfhandle_driver_F8825DC4(void *mbuf);

/* Data structures */
typedef struct SceNetAdhocAuthMbxMsg {
    void *next;      /* 0x00 */
    uint32_t prio;   /* 0x04 */
    uint32_t type;   /* 0x08 */
    uint8_t mac[6];  /* 0x0c */
    uint16_t pad_12; /* 0x12 */
} SceNetAdhocAuthMbxMsg; /* 0x14 */

typedef struct SceNetAdhocAuthPeer {
    struct SceNetAdhocAuthPeer *next;      /* 0x00 */
    struct SceNetAdhocAuthPeer **prev_ptr; /* 0x04 */
    struct SceNetAdhocAuthCtx *ctx;        /* 0x08 */
    int state;                             /* 0x0c */
    uint8_t memab_ctx[0xb0];               /* 0x10..0xc0 */
    struct SceNetAdhocAuthPeer *hash_next; /* 0xc0 */
    void *hash_prev_ptr;                   /* 0xc4 */
    uint8_t hash_mac[6];                   /* 0xc8 (offset 0xd8 in peer) */
    uint16_t pad_de;                       /* 0xce */
    uint64_t timestamp;                    /* 0xd0 (offset 0xe0 in peer) */
    uint8_t ibss[0x80];                    /* 0xd8 (offset 0xe8 in peer) */
    uint8_t msg1_data[0x120];              /* 0x158 (offset 0x168 in peer) */
    uint8_t msg4_data[0x110];              /* 0x278 (offset 0x288 in peer) */
    uint8_t msg5_data[0x100];              /* 0x388 (offset 0x398 in peer) */
    int retry_alarm;                       /* 0x488 (offset 0x498 in peer) */
    int keepalive_tx_alarm;                /* 0x48c (offset 0x49c in peer) */
    int keepalive_rx_alarm;                /* 0x490 (offset 0x4a0 in peer) */
    uint32_t retry_count;                  /* 0x494 (offset 0x4a4 in peer) */
    uint32_t tx_seq;                       /* 0x498 (offset 0x4a8 in peer) */
    uint32_t rx_seq;                       /* 0x49c (offset 0x4ac in peer) */
    uint32_t peer_devkit_ver;              /* 0x4a0 (offset 0x4b0 in peer) */
    uint32_t peer_sdk_ver;                 /* 0x4a4 (offset 0x4b4 in peer) */
    uint32_t peer_model;                   /* 0x4a8 (offset 0x4b8 in peer) */
    uint32_t pad_4bc;                      /* 0x4ac (offset 0x4bc in peer) */
} SceNetAdhocAuthPeer; /* 0x4c0 */

typedef struct SceNetAdhocAuthCtx {
    int state;                            /* 0x00 */
    int mbx_id;                           /* 0x04 */
    int thid;                             /* 0x08 */
    uint32_t flags;                       /* 0x0c */
    SceNetAdhocAuthPeer *peer_head;       /* 0x10 */
    SceNetAdhocAuthPeer **peer_tail_ptr;  /* 0x14 */
    int peer_count;                       /* 0x18 */
    void *ifp;                            /* 0x1c */
    uint8_t local_mac[6];                 /* 0x20 */
    uint8_t ibss[0x80];                   /* 0x26 */
    uint16_t pad_a6;                      /* 0xa6 */
    int max_peers;                        /* 0xa8 */
    SceNetAdhocAuthMbxMsg rx_mbx_msg;     /* 0xac..0xc0 */
    uint32_t beacon_interval;             /* 0xc0 */
    int beacon_alarm;                     /* 0xc4 */
    SceNetAdhocAuthMbxMsg beacon_mbx_msg; /* 0xc8..0xdc */
    SceNetAdhocAuthMbxMsg msg_pool[16];   /* 0xdc..0x21c */
    uint32_t msg_pool_bitmap;             /* 0x21c */
    uint32_t retry_interval;              /* 0x220 */
    uint32_t max_retries;                 /* 0x224 */
    uint32_t keepalive_tx_interval;       /* 0x228 */
    uint32_t keepalive_rx_interval;       /* 0x22c */
    uint32_t devkit_ver;                  /* 0x230 */
    uint32_t sdk_ver;                     /* 0x234 */
    uint32_t model;                       /* 0x238 */
    uint8_t tlv_buf[0x40];                /* 0x23c */
    uint32_t tlv_len;                     /* 0x27c */
} SceNetAdhocAuthCtx; /* 0x280 */

typedef struct SceNetAdhocAuthHashBucket {
    uint8_t *head;      /* 0x00 (points to peer + 0x10) */
    uint8_t **tail_ptr; /* 0x04 */
} SceNetAdhocAuthHashBucket;

typedef struct SceNetAdhocAuthMbufQueue {
    void *head;     /* 0x00 */
    void *tail;     /* 0x04 */
    int count;      /* 0x08 */
    int max_count;  /* 0x0c */
    uint32_t unk10; /* 0x10 */
} SceNetAdhocAuthMbufQueue;

/* .rodata constants */
static const uint16_t g_pkt_hdr_sizes[8] = {
    0x0000, 0x0000, 0x0080, 0x0120, 0x0110, 0x0100, 0x0060, 0x0020
};
static const uint8_t g_bcast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static const uint8_t g_eth_proto_sub[2] = { 0x00, 0x01 };
static const uint8_t g_eth_proto_main[2] = { 0x88, 0xc8 };

/* .data (0x4770..0x477c) */
static int g_hash_sema = -1;       /* 0x4770 */
static int g_lock_sema = -1;       /* 0x4774 */
static int g_lock_owner_thid = -1; /* 0x4778 */

/* .bss (0x4780..0x4ab0) */
static int g_inited;                                 /* 0x4780 */
static SceNetAdhocAuthCtx *g_ctx;                    /* 0x4784 */
static SceNetAdhocAuthCtx g_ctx_buf;                 /* 0x4788 (0x280 bytes) */
static SceNetAdhocAuthHashBucket g_hash_buckets[16]; /* 0x4a08 (0x80 bytes) */
static uint32_t g_key_len;                           /* 0x4a88 */
static uint8_t g_key_buf[16];                        /* 0x4a8c */
static SceNetAdhocAuthMbufQueue g_rx_queue;          /* 0x4a9c (0x14 bytes) */

/* Forward declarations */
static int __attribute__((noinline)) sub_008c(void *mbuf);
static int __attribute__((noinline)) sub_01b4(void *mbuf);
static int __attribute__((noinline)) sub_0444(void);
static int __attribute__((noinline)) sub_0468(void *mbuf);
static int __attribute__((noinline)) sub_072c(void);
static int __attribute__((noinline)) sub_1854(void);
static void __attribute__((noinline)) sub_18d4(void);
static uint8_t *__attribute__((noinline)) sub_1910(const uint8_t *mac);
static int __attribute__((noinline)) sub_1980(uint8_t *memab_ctx);
static int __attribute__((noinline)) sub_1a68(uint8_t *memab_ctx);
static int __attribute__((noinline)) sub_1b60(void);
static void __attribute__((noinline)) sub_1b94(void);
static int __attribute__((noinline)) sub_1e0c(uint32_t arglen, void *argp);
static void *__attribute__((noinline)) sub_1f84(SceNetAdhocAuthCtx *ctx, int pkt_type,
                                                const void *hdr_data, const void *extra_data,
                                                uint32_t extra_len);
static void __attribute__((noinline)) sub_2130(SceNetAdhocAuthCtx *ctx, const uint8_t *dst_mac, void *mbuf);
static void __attribute__((noinline)) sub_2260(SceNetAdhocAuthCtx *ctx);
static void __attribute__((noinline)) sub_2324(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer);
static void __attribute__((noinline)) sub_23e8(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer);
static void __attribute__((noinline)) sub_255c(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer);
static void __attribute__((noinline)) sub_26c8(SceNetAdhocAuthCtx *ctx);
static void __attribute__((noinline)) sub_378c(SceNetAdhocAuthCtx *ctx);
static uint32_t __attribute__((noinline)) sub_389c(void *common);
static uint32_t __attribute__((noinline)) sub_38e4(void *common);
static uint32_t __attribute__((noinline)) sub_395c(void *common);
static uint32_t __attribute__((noinline)) sub_39d4(void *common);
static void __attribute__((noinline)) sub_3a4c(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer,
                                               const uint8_t *mac, const uint8_t *ibss);
static SceNetAdhocAuthPeer *__attribute__((noinline)) sub_3bb0(SceNetAdhocAuthCtx *ctx, const uint8_t *mac);
static void __attribute__((noinline)) sub_3c08(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer);
static void __attribute__((noinline)) sub_3d2c(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer, int new_state);
static void __attribute__((noinline)) sub_3e00(SceNetAdhocAuthCtx *ctx, int state);
static int __attribute__((noinline)) sub_3e08(void);
static int __attribute__((noinline)) sub_3e5c(void);
static int __attribute__((noinline)) sub_3e98(void);
static void __attribute__((noinline)) sub_3f18(int locked);
static void __attribute__((noinline)) sub_3f40(void);
static SceNetAdhocAuthMbxMsg *__attribute__((noinline)) sub_3f64(SceNetAdhocAuthCtx *ctx);
static void __attribute__((noinline)) sub_3fac(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthMbxMsg *msg);
static void __attribute__((noinline)) sub_3ff8(SceNetAdhocAuthPeer *peer, const uint8_t *buf, int len);
static void __attribute__((noinline)) sub_412c(SceNetAdhocAuthCtx *ctx, uint8_t *dst, uint32_t *out_len);

/* 0x0000 */
int sceNetAdhocAuth_lib_6074D8F1(void)
{
    uint32_t k1;
    __asm__ volatile("move %0, $k1\n\tsll $k1, $k1, 11" : "=r"(k1));

    if (g_inited > 0) {
        sceNetIfhandle_driver_773FC77C();
        sub_1b94();
        sub_072c();
        sub_18d4();
        g_inited = 0;
    }

    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return 0;
}
int sceNetAdhocAuth_driver_6074D8F1(void)
    __attribute__((alias("sceNetAdhocAuth_lib_6074D8F1")));

/* 0x0064 */
int module_reboot_before(void)
{
    sub_3f40();
    sceNetAdhocAuth_lib_6074D8F1();
    return 0;
}

/* 0x008c */
static int __attribute__((noinline)) sub_008c(void *mbuf)
{
    uint8_t *m = (uint8_t *)mbuf;
    uint8_t *data = *(uint8_t **)(m + 0x08);
    if ((intptr_t)data < 0) {
        return -1;
    }
    uint16_t flags = *(uint16_t *)(m + 0x12);
    int pkt_len = *(int *)(m + 0x18);
    if ((flags & 0x100) != 0) {
        if (sceMemab_driver_F742F283(data + 0x30, pkt_len - 0x30, data + 0x10, 1) < 0) {
            return -1;
        }
        return 0;
    }
    if ((flags & 0x200) != 0) {
        return -1;
    }

    sceKernelWaitSema(g_hash_sema, 1, NULL);
    uint8_t *ctx = sub_1910(data);
    if (ctx == NULL ||
        sceMemab_driver_3C15BC8C(ctx, data + 0x30, pkt_len - 0x30, data + 0x10, 1) < 0) {
        sceKernelSignalSema(g_hash_sema, 1);
        return -1;
    }
    sceKernelSignalSema(g_hash_sema, 1);
    return 0;
}

/* 0x01b4 */
static int __attribute__((noinline)) sub_01b4(void *mbuf)
{
    uint8_t *m = (uint8_t *)mbuf;
    uint8_t *data = *(uint8_t **)(m + 0x08);
    void * bounce_buf = NULL;
    uint8_t *hdr_ptr;

    if ((intptr_t)data < 0) {
        sceKernelWaitSema(g_hash_sema, 1, NULL);
        sceKernelSignalSema(g_hash_sema, 1);
        return -1;
    }

    int pkt_len = *(int *)(m + 0x18);
    int first_len = *(int *)(m + 0x0c);
    hdr_ptr = data + 0x10;
    if (pkt_len != first_len) {
        bounce_buf = sceNetIfhandle_driver_15CFE3C0(pkt_len - 0x10);
        if (bounce_buf == NULL) {
            return -1;
        }
        hdr_ptr = (uint8_t *)sceNetIfhandle_driver_9A6261EC(m, 0x10, pkt_len - 0x10, bounce_buf);
    }

    sceKernelWaitSema(g_hash_sema, 1, NULL);
    uint8_t *ctx = sub_1910(data + 6);
    if (ctx != NULL) {
        uint16_t flags = *(uint16_t *)(m + 0x12);
        int res;
        if ((flags & 0x100) != 0) {
            res = sceMemab_driver_CB5D3916(ctx, hdr_ptr + 0x20, *(int *)(m + 0x18) - 0x30, hdr_ptr, 1, 1);
        } else if ((flags & 0x200) == 0) {
            res = sceMemab_driver_D47A50B1(ctx, hdr_ptr + 0x20, *(int *)(m + 0x18) - 0x30, hdr_ptr, 1, 1);
        } else {
            res = -1;
        }
        if (res >= 0) {
            if (bounce_buf != NULL) {
                sceNetIfhandle_driver_1560F143(m, 0x30, *(int *)(m + 0x18) - 0x30, hdr_ptr);
                sceNetIfhandle_driver_76BAD213(bounce_buf);
            }
            uint64_t now = sceKernelGetSystemTimeWide();
            *(uint64_t *)(ctx + 0xd0) = now;
            sceKernelSignalSema(g_hash_sema, 1);
            return 0;
        }
    }

    sceKernelSignalSema(g_hash_sema, 1);
    if (bounce_buf != NULL) {
        sceNetIfhandle_driver_76BAD213(bounce_buf);
    }
    return -1;
}

/* 0x036c */
int __attribute__((optimize("no-crossjumping", "no-reorder-blocks")))
sceNetAdhocAuth_lib_86004235(void)
{
    uint32_t k1;
    __asm__ volatile("move %0, $k1\n\tsll $k1, $k1, 11" : "=r"(k1));

    int res = 0x80410601;
    if (g_inited <= 0) {
        res = sub_1854();
        if (res >= 0) {
            res = sub_0444();
            if (res >= 0) {
                res = sub_1b60();
                if (res >= 0) {
                    res = sceNetIfhandle_driver_0542835F(sub_01b4, sub_008c, sub_0468);
                    if (res >= 0) {
                        g_inited = 1;
                        res = 0;
                    } else {
                        sub_1b94();
                        sub_072c();
                        sub_18d4();
                    }
                } else {
                    sub_072c();
                    sub_18d4();
                }
            } else {
                sub_18d4();
            }
        }
    }

    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return res;
}
int sceNetAdhocAuth_driver_86004235(void)
    __attribute__((alias("sceNetAdhocAuth_lib_86004235")));

/* 0x0444 */
static int __attribute__((noinline)) sub_0444(void)
{
    int res = sub_3e08();
    return (res < 0) ? res : 0;
}

/* 0x0468 */
static int __attribute__((noinline)) sub_0468(void *mbuf)
{
    uint8_t *m = (uint8_t *)mbuf;
    if (*(int *)(m + 0x18) < 0x14) {
        return -1;
    }

    int intr = sceKernelCpuSuspendIntr();
    if (g_ctx != NULL && g_rx_queue.count < g_rx_queue.max_count) {
        *(void **)(m + 0x04) = NULL;
        if (g_rx_queue.tail == NULL) {
            g_rx_queue.head = m;
        } else {
            *(void **)((uint8_t *)g_rx_queue.tail + 0x04) = m;
        }
        g_rx_queue.tail = m;
        g_rx_queue.count++;
        if ((g_ctx->flags & 1) == 0) {
            g_ctx->flags |= 1;
            sceKernelSendMbx(g_ctx->mbx_id, &g_ctx->rx_mbx_msg);
        }
        sceKernelCpuResumeIntr(intr);
        return 0;
    }
    sceKernelCpuResumeIntr(intr);
    return -1;
}

/* 0x0580 */
int sceNetAdhocAuth_lib_2E6AA271(void)
{
    uint32_t k1;
    __asm__ volatile("move %0, $k1\n\tsll $k1, $k1, 11" : "=r"(k1));

    if (g_inited <= 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410681;
    }
    if (g_ctx == NULL) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410682;
    }

    SceNetAdhocAuthMbxMsg msg;
    msg.next = NULL;
    msg.prio = 0;
    msg.type = 6;
    *(uint32_t *)&msg.mac[0] = 0;
    *(uint32_t *)&msg.mac[4] = 0;
    sceKernelSendMbx(g_ctx->mbx_id, &msg);

    sceKernelWaitThreadEnd(g_ctx->thid, NULL);
    sceKernelTerminateThread(g_ctx->thid);
    sceKernelDeleteThread(g_ctx->thid);

    int locked = sub_3e98();
    int intr = sceKernelCpuSuspendIntr();
    if (g_ctx->beacon_alarm > 0) {
        sceKernelCancelAlarm(g_ctx->beacon_alarm);
        g_ctx->beacon_alarm = 0;
    }
    sceKernelCpuResumeIntr(intr);

    sceKernelDeleteMbx(g_ctx->mbx_id);

    intr = sceKernelCpuSuspendIntr();
    while (g_rx_queue.head != NULL) {
        uint8_t *m = (uint8_t *)g_rx_queue.head;
        void *next = *(void **)(m + 0x04);
        g_rx_queue.head = next;
        if (next == NULL) {
            g_rx_queue.tail = NULL;
        }
        *(void **)(m + 0x04) = NULL;
        g_rx_queue.count--;
        sceNetIfhandle_driver_F56FAC82(m);
    }
    g_ctx = NULL;
    sceKernelCpuResumeIntr(intr);

    sub_3f18(locked);
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return 0;
}
int sceNetAdhocAuth_driver_2E6AA271(void)
    __attribute__((alias("sceNetAdhocAuth_lib_2E6AA271")));

/* 0x072c */
static int __attribute__((noinline)) sub_072c(void)
{
    sceNetAdhocAuth_lib_2E6AA271();
    return sub_3e5c();
}

/* 0x0748 */
int sceNetAdhocAuth_lib_6CE209A3(void)
{
    uint32_t k1;
    __asm__ volatile("move %0, $k1\n\tsll $k1, $k1, 11" : "=r"(k1));

    if (g_inited <= 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410681;
    }
    if (g_ctx == NULL) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410682;
    }
    if (g_ctx->state != 1) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0;
    }

    int locked = sub_3e98();
    int intr = sceKernelCpuSuspendIntr();
    if (g_ctx->beacon_alarm > 0) {
        sceKernelCancelAlarm(g_ctx->beacon_alarm);
        g_ctx->beacon_alarm = 0;
    }
    sceKernelCpuResumeIntr(intr);

    SceNetAdhocAuthPeer *peer = g_ctx->peer_head;
    while (peer != NULL) {
        if (peer->state == 4) {
            intr = sceKernelCpuSuspendIntr();
            if (peer->keepalive_rx_alarm > 0) {
                sceKernelCancelAlarm(peer->keepalive_rx_alarm);
                peer->keepalive_rx_alarm = 0;
            }
            sceKernelCpuResumeIntr(intr);

            intr = sceKernelCpuSuspendIntr();
            if (peer->keepalive_tx_alarm > 0) {
                sceKernelCancelAlarm(peer->keepalive_tx_alarm);
                peer->keepalive_tx_alarm = 0;
            }
            sceKernelCpuResumeIntr(intr);
        }
        peer = peer->next;
    }

    sub_3e00(g_ctx, 2);
    sub_3f18(locked);
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return 0;
}
int sceNetAdhocAuth_driver_6CE209A3(void)
    __attribute__((alias("sceNetAdhocAuth_lib_6CE209A3")));

/* 0x08b0 */
int sceNetAdhocAuth_lib_2AD8C677(void)
{
    uint32_t k1;
    __asm__ volatile("move %0, $k1\n\tsll $k1, $k1, 11" : "=r"(k1));

    if (g_inited <= 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410681;
    }
    if (g_ctx == NULL) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410682;
    }
    if (g_ctx->state == 1) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0;
    }

    int locked = sub_3e98();
    int intr = sceKernelCpuSuspendIntr();
    if (g_ctx->beacon_alarm > 0) {
        sceKernelCancelAlarm(g_ctx->beacon_alarm);
        g_ctx->beacon_alarm = 0;
    }
    g_ctx->beacon_alarm = sceKernelSetAlarm(g_ctx->beacon_interval, sub_389c, g_ctx);
    sceKernelCpuResumeIntr(intr);

    SceNetAdhocAuthPeer *peer = g_ctx->peer_head;
    while (peer != NULL) {
        if (peer->state == 4) {
            intr = sceKernelCpuSuspendIntr();
            if (peer->keepalive_rx_alarm > 0) {
                sceKernelCancelAlarm(peer->keepalive_rx_alarm);
                peer->keepalive_rx_alarm = 0;
            }
            peer->keepalive_rx_alarm = sceKernelSetAlarm(g_ctx->keepalive_rx_interval, sub_395c, peer);
            sceKernelCpuResumeIntr(intr);

            intr = sceKernelCpuSuspendIntr();
            if (peer->keepalive_tx_alarm > 0) {
                sceKernelCancelAlarm(peer->keepalive_tx_alarm);
                peer->keepalive_tx_alarm = 0;
            }
            peer->keepalive_tx_alarm = sceKernelSetAlarm(g_ctx->keepalive_tx_interval, sub_39d4, peer);
            sceKernelCpuResumeIntr(intr);
        }
        peer = peer->next;
    }

    sub_3e00(g_ctx, 1);
    sub_3f18(locked);
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return 0;
}
int sceNetAdhocAuth_driver_2AD8C677(void)
    __attribute__((alias("sceNetAdhocAuth_lib_2AD8C677")));

/* 0x0a78 */
int sceNetAdhocAuth_lib_89F2A732(const char *ifname, int prio, int stack_size,
                                 const uint32_t *params, int max_peers, const uint8_t *ibss)
{
    uint32_t k1, k1_shifted;
    __asm__ volatile("move %0, $k1\n\tsll %1, $k1, 11\n\tmove $k1, %1" : "=r"(k1), "=r"(k1_shifted));

    if (ifname == NULL || ((int32_t)(k1_shifted & (uint32_t)ifname) < 0) ||
        params == NULL || ((int32_t)(k1_shifted & (uint32_t)params) < 0) ||
        ibss == NULL || ((int32_t)(k1_shifted & (uint32_t)ibss) < 0)) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410687;
    }

    if (g_inited <= 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410681;
    }
    if (g_ctx != NULL) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410683;
    }

    int locked = sub_3e98();
    int intr = sceKernelCpuSuspendIntr();
    memset(&g_ctx_buf, 0, sizeof(g_ctx_buf));
    g_ctx = &g_ctx_buf;
    g_rx_queue.max_count = 0x32;
    g_rx_queue.tail = NULL;
    g_rx_queue.count = 0;
    g_rx_queue.unk10 = 0;
    g_rx_queue.head = NULL;
    sceKernelCpuResumeIntr(intr);

    g_ctx->peer_tail_ptr = &g_ctx->peer_head;
    g_ctx->state = 0;
    g_ctx->peer_head = NULL;
    g_ctx->ifp = sceNetIfhandle_driver_9CBA24D4(ifname);
    if (g_ctx->ifp == NULL) {
        int res = 0x80410687;
        goto fail_cleanup;
    }

    int res = sceNetIfhandle_driver_C5623112(g_ctx->ifp, g_ctx->local_mac);
    if (res < 0) {
        goto fail_cleanup;
    }

    memcpy(g_ctx->ibss, ibss, 0x80);
    g_ctx->max_peers = max_peers;
    g_ctx->beacon_mbx_msg.type = 3;
    g_ctx->beacon_interval = params[0];
    g_ctx->rx_mbx_msg.type = 0;
    g_ctx->retry_interval = params[1];
    g_ctx->max_retries = params[2];
    g_ctx->keepalive_tx_interval = params[3];
    g_ctx->keepalive_rx_interval = params[4];

    g_ctx->devkit_ver = sceKernelDevkitVersion();
    g_ctx->sdk_ver = sceKernelGetCompiledSdkVersion();
    g_ctx->model = sceKernelGetModel();
    sub_412c(g_ctx, g_ctx->tlv_buf, &g_ctx->tlv_len);

    g_ctx->mbx_id = sceKernelCreateMbx("SceNetAdhocAuth", 1, NULL);
    if (g_ctx->mbx_id < 0) {
        res = g_ctx->mbx_id;
        goto fail_cleanup;
    }

    g_ctx->thid = sceKernelCreateThread("SceNetAdhocAuth", sub_1e0c, prio, stack_size, 0x200001, NULL);
    if (g_ctx->thid < 0) {
        res = g_ctx->thid;
        sceKernelDeleteMbx(g_ctx->mbx_id);
        goto fail_cleanup;
    }

    res = sceKernelStartThread(g_ctx->thid, 4, &g_ctx);
    if (res < 0) {
        sceKernelDeleteThread(g_ctx->thid);
        sceKernelDeleteMbx(g_ctx->mbx_id);
        goto fail_cleanup;
    }

    intr = sceKernelCpuSuspendIntr();
    if (g_ctx->beacon_alarm > 0) {
        sceKernelCancelAlarm(g_ctx->beacon_alarm);
        g_ctx->beacon_alarm = 0;
    }
    g_ctx->beacon_alarm = sceKernelSetAlarm(g_ctx->beacon_interval, sub_389c, g_ctx);
    sceKernelCpuResumeIntr(intr);

    sub_3e00(g_ctx, 1);
    sub_3f18(locked);
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return 0;

fail_cleanup:
    intr = sceKernelCpuSuspendIntr();
    while (g_rx_queue.head != NULL) {
        uint8_t *m = (uint8_t *)g_rx_queue.head;
        void *next = *(void **)(m + 0x04);
        g_rx_queue.head = next;
        if (next == NULL) {
            g_rx_queue.tail = NULL;
        }
        *(void **)(m + 0x04) = NULL;
        g_rx_queue.count--;
        sceNetIfhandle_driver_F56FAC82(m);
    }
    g_ctx = NULL;
    sceKernelCpuResumeIntr(intr);
    sub_3f18(locked);
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return res;
}
int sceNetAdhocAuth_driver_89F2A732(const char *ifname, int prio, int stack_size,
                                    const uint32_t *params, int max_peers, const uint8_t *ibss)
    __attribute__((alias("sceNetAdhocAuth_lib_89F2A732")));

/* 0x0ecc */
int sceNetAdhocAuth_lib_1F9A90B8(const uint8_t *ibss, uint32_t *buflen, uint8_t *buf)
{
    uint32_t k1, k1_shifted;
    __asm__ volatile("move %0, $k1\n\tsll %1, $k1, 11\n\tmove $k1, %1" : "=r"(k1), "=r"(k1_shifted));

    if (ibss == NULL || ((int32_t)(k1_shifted & (uint32_t)ibss) < 0) ||
        buflen == NULL || ((int32_t)(k1_shifted & (uint32_t)buflen) < 0)) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410687;
    }
    if (buf != NULL) {
        uint32_t len = *buflen;
        uint32_t end = (uint32_t)buf + len;
        if ((int32_t)(k1_shifted & (len | (uint32_t)buf | end)) < 0) {
            __asm__ volatile("move $k1, %0" : : "r"(k1));
            return 0x80410687;
        }
    }

    if (g_inited <= 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410681;
    }

    int locked = sub_3e98();
    if (locked < 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return locked;
    }

    if (g_ctx == NULL) {
        sub_3f18(locked);
        *buflen = 0;
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410682;
    }

    uint32_t count = 0;
    uint32_t total_len = 0;
    uint8_t *prev = NULL;
    SceNetAdhocAuthPeer *peer = g_ctx->peer_head;
    while (peer != NULL) {
        if (peer->state == 4 && memcmp(peer->ibss, ibss, 0x80) == 0) {
            count++;
            uint32_t next_len = total_len + 0x98;
            if (buf != NULL) {
                if (*buflen < next_len) {
                    break;
                }
                uint8_t *cur = buf + total_len;
                *(void **)(cur + 0x00) = NULL;
                memcpy(cur + 0x84, peer->hash_mac, 6);
                memcpy(cur + 0x04, peer->ibss, 0x80);
                if (prev != NULL) {
                    *(void **)prev = cur;
                }
                prev = cur;
            }
            total_len = next_len;
        }
        peer = peer->next;
    }

    sub_3f18(locked);
    *buflen = total_len;
    int res = (count != 0) ? 0 : (int)0x80410716;
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return res;
}
int sceNetAdhocAuth_driver_1F9A90B8(const uint8_t *ibss, uint32_t *buflen, uint8_t *buf)
    __attribute__((alias("sceNetAdhocAuth_lib_1F9A90B8")));

/* 0x118c */
int sceNetAdhocAuth_lib_76F26AB0(const uint8_t *mac, uint8_t *ibss_out)
{
    uint32_t k1, k1_shifted;
    __asm__ volatile("move %0, $k1\n\tsll %1, $k1, 11\n\tmove $k1, %1" : "=r"(k1), "=r"(k1_shifted));

    if (mac == NULL || ((int32_t)(k1_shifted & (uint32_t)mac) < 0) ||
        ibss_out == NULL || ((int32_t)(k1_shifted & (uint32_t)ibss_out) < 0)) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410687;
    }
    if (g_inited <= 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410681;
    }

    int locked = sub_3e98();
    if (locked < 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return locked;
    }
    if (g_ctx == NULL) {
        sub_3f18(locked);
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410682;
    }

    SceNetAdhocAuthPeer *peer = g_ctx->peer_head;
    while (peer != NULL) {
        if (peer->state == 4 && memcmp(peer->hash_mac, mac, 6) == 0) {
            memcpy(ibss_out, peer->ibss, 0x80);
            sub_3f18(locked);
            __asm__ volatile("move $k1, %0" : : "r"(k1));
            return 0;
        }
        peer = peer->next;
    }

    sub_3f18(locked);
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return 0x80410716;
}
int sceNetAdhocAuth_driver_76F26AB0(const uint8_t *mac, uint8_t *ibss_out)
    __attribute__((alias("sceNetAdhocAuth_lib_76F26AB0")));

/* 0x1378 */
int sceNetAdhocAuth_lib_BD144DA6(uint32_t *buflen, uint8_t *buf)
{
    uint32_t k1, k1_shifted;
    __asm__ volatile("move %0, $k1\n\tsll %1, $k1, 11\n\tmove $k1, %1" : "=r"(k1), "=r"(k1_shifted));

    if (buflen == NULL || ((int32_t)(k1_shifted & (uint32_t)buflen) < 0)) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410687;
    }
    if (buf != NULL) {
        uint32_t len = *buflen;
        uint32_t end = (uint32_t)buf + len;
        if ((int32_t)(k1_shifted & (len | (uint32_t)buf | end)) < 0) {
            __asm__ volatile("move $k1, %0" : : "r"(k1));
            return 0x80410687;
        }
    }

    if (g_inited <= 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410681;
    }

    int locked = sub_3e98();
    if (locked < 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return locked;
    }

    if (g_ctx == NULL) {
        sub_3f18(locked);
        *buflen = 0;
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410682;
    }

    uint32_t total_len = 0;
    uint8_t *prev = NULL;
    SceNetAdhocAuthPeer *peer = g_ctx->peer_head;
    while (peer != NULL) {
        if (peer->state == 4) {
            uint32_t next_len = total_len + 0x98;
            if (buf != NULL) {
                if (*buflen < next_len) {
                    break;
                }
                uint8_t *cur = buf + total_len;
                *(void **)(cur + 0x00) = NULL;
                memcpy(cur + 0x84, peer->hash_mac, 6);
                memcpy(cur + 0x04, peer->ibss, 0x80);
                *(uint64_t *)(cur + 0x90) = peer->timestamp;
                if (prev != NULL) {
                    *(void **)prev = cur;
                }
                prev = cur;
            }
            total_len = next_len;
        }
        peer = peer->next;
    }

    sub_3f18(locked);
    *buflen = total_len;
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return 0;
}
int sceNetAdhocAuth_driver_BD144DA6(uint32_t *buflen, uint8_t *buf)
    __attribute__((alias("sceNetAdhocAuth_lib_BD144DA6")));

/* 0x15f4 */
int sceNetAdhocAuth_lib_CF4D9BED(const uint8_t *mac, uint32_t buflen, uint8_t *buf)
{
    uint32_t k1, k1_shifted;
    __asm__ volatile("move %0, $k1\n\tsll %1, $k1, 11\n\tmove $k1, %1" : "=r"(k1), "=r"(k1_shifted));

    if (mac == NULL || ((int32_t)(k1_shifted & (uint32_t)mac) < 0) ||
        buf == NULL || ((int32_t)(k1_shifted & (buflen | (uint32_t)buf | ((uint32_t)buf + buflen))) < 0)) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410687;
    }

    if (g_inited <= 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410681;
    }

    int locked = sub_3e98();
    if (locked < 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return locked;
    }

    if (g_ctx == NULL) {
        sub_3f18(locked);
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410682;
    }

    SceNetAdhocAuthPeer *peer = g_ctx->peer_head;
    while (peer != NULL) {
        if (peer->state == 4 && memcmp(peer->hash_mac, mac, 6) == 0) {
            int res;
            if (buflen < 0x98) {
                res = 0x80410704;
            } else {
                *(void **)(buf + 0x00) = NULL;
                memcpy(buf + 0x84, peer->hash_mac, 6);
                memcpy(buf + 0x04, peer->ibss, 0x80);
                *(uint64_t *)(buf + 0x90) = peer->timestamp;
                res = 0;
            }
            sub_3f18(locked);
            __asm__ volatile("move $k1, %0" : : "r"(k1));
            return res;
        }
        peer = peer->next;
    }

    sub_3f18(locked);
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return 0x80410716;
}
int sceNetAdhocAuth_driver_CF4D9BED(const uint8_t *mac, uint32_t buflen, uint8_t *buf)
    __attribute__((alias("sceNetAdhocAuth_lib_CF4D9BED")));

/* 0x1854 */
static int __attribute__((noinline)) sub_1854(void)
{
    if (sceMemab_driver_6DD7339A() < 0) {
        return 0x80410688;
    }
    for (int i = 0; i < 16; i++) {
        g_hash_buckets[i].head = NULL;
        g_hash_buckets[i].tail_ptr = &g_hash_buckets[i].head;
    }
    int sema = sceKernelCreateSema("SceNetAdhocAuth", 1, 1, 1, NULL);
    if (sema < 0) {
        return sema;
    }
    g_hash_sema = sema;
    return 0;
}

/* 0x18d4 */
static void __attribute__((noinline)) sub_18d4(void)
{
    if (g_hash_sema >= 0) {
        sceKernelDeleteSema(g_hash_sema);
    }
    g_hash_sema = -1;
    sceMemab_driver_D57856A7();
}

/* 0x1910 */
static uint8_t *__attribute__((noinline)) sub_1910(const uint8_t *mac)
{
    uint32_t idx = mac[5] & 0xf;
    uint8_t *cur = g_hash_buckets[idx].head;
    while (cur != NULL) {
        if (memcmp(cur + 0xc8, mac, 6) == 0) {
            return cur;
        }
        cur = *(uint8_t **)(cur + 0xc0);
    }
    return NULL;
}

/* 0x1980 */
static int __attribute__((noinline)) sub_1980(uint8_t *memab_ctx)
{
    uint8_t *mac = memab_ctx + 0xc8;
    uint32_t idx = mac[5] & 0xf;
    uint8_t *cur = g_hash_buckets[idx].head;
    while (cur != NULL) {
        if (memcmp(cur + 0xc8, mac, 6) == 0) {
            return 0x80410686;
        }
        cur = *(uint8_t **)(cur + 0xc0);
    }

    SceNetAdhocAuthHashBucket *b = &g_hash_buckets[memab_ctx[0xcd] & 0xf];
    *(uint8_t **)(memab_ctx + 0xc0) = b->head;
    if (b->head != NULL) {
        *(void **)(b->head + 0xc4) = memab_ctx + 0xc0;
    } else {
        b->tail_ptr = (uint8_t **)(memab_ctx + 0xc0);
    }
    *(void **)(memab_ctx + 0xc4) = &b->head;
    b->head = memab_ctx;
    return 0;
}

/* 0x1a68 */
static int __attribute__((noinline)) sub_1a68(uint8_t *memab_ctx)
{
    uint8_t *mac = memab_ctx + 0xc8;
    uint32_t idx = mac[5] & 0xf;
    uint8_t *cur = g_hash_buckets[idx].head;
    while (cur != NULL) {
        if (memcmp(cur + 0xc8, mac, 6) == 0) {
            SceNetAdhocAuthHashBucket *b = &g_hash_buckets[memab_ctx[0xcd] & 0xf];
            uint8_t *next = *(uint8_t **)(memab_ctx + 0xc0);
            uint8_t **prev_ptr = *(uint8_t ***)(memab_ctx + 0xc4);
            if (next != NULL) {
                *(uint8_t ***)(next + 0xc4) = prev_ptr;
            } else {
                b->tail_ptr = prev_ptr;
            }
            *prev_ptr = next;
            return 0;
        }
        cur = *(uint8_t **)(cur + 0xc0);
    }
    return 0x80410685;
}

/* 0x1b60 */
static int __attribute__((noinline)) sub_1b60(void)
{
    g_key_len = 0;
    memset(g_key_buf, 0, 0x10);
    return 0;
}

/* 0x1b94 */
static void __attribute__((noinline)) sub_1b94(void)
{
}

/* 0x1b9c */
int sceNetAdhocAuth_lib_015A8A64(void *dst, uint32_t len)
{
    uint32_t k1, k1_shifted;
    __asm__ volatile("move %0, $k1\n\tsll %1, $k1, 11\n\tmove $k1, %1" : "=r"(k1), "=r"(k1_shifted));

    if (dst == NULL || ((int32_t)(k1_shifted & (len | (uint32_t)dst | ((uint32_t)dst + len))) < 0)) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410687;
    }
    if (g_key_len == 0) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x8041071b;
    }

    int res = sceMemab_driver_9BF1A0A4(g_key_buf, dst, len);
    if (res < 0) {
        res = 0x80410688;
    }
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return res;
}
int sceNetAdhocAuth_driver_015A8A64(void *dst, uint32_t len)
    __attribute__((alias("sceNetAdhocAuth_lib_015A8A64")));

/* 0x1c34 */
int sceNetAdhocAuth_lib_72AAC6D3(uint32_t len, const void *src)
{
    uint32_t k1, k1_shifted;
    __asm__ volatile("move %0, $k1\n\tsll %1, $k1, 11\n\tmove $k1, %1" : "=r"(k1), "=r"(k1_shifted));

    if ((len != 0 && src == NULL) ||
        ((int32_t)(k1_shifted & (len | (uint32_t)src | ((uint32_t)src + len))) < 0) ||
        len > 0x10) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410687;
    }

    g_key_len = len;
    memset(g_key_buf, 0, 0x10);
    if (len != 0) {
        memcpy(g_key_buf, src, len);
    }
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return 0;
}
int sceNetAdhocAuth_driver_72AAC6D3(uint32_t len, const void *src)
    __attribute__((alias("sceNetAdhocAuth_lib_72AAC6D3")));

/* 0x1d0c */
int sceNetAdhocAuth_lib_312BD812(void *buf)
{
    uint32_t k1, k1_shifted;
    __asm__ volatile("move %0, $k1\n\tsll %1, $k1, 11\n\tmove $k1, %1" : "=r"(k1), "=r"(k1_shifted));

    if (buf == NULL || ((int32_t)(k1_shifted & ((uint32_t)buf | ((uint32_t)buf + 0x10))) < 0)) {
        __asm__ volatile("move $k1, %0" : : "r"(k1));
        return 0x80410687;
    }

    int res = sceMemab_driver_16594684(buf);
    if (res < 0) {
        res = 0x80410688;
    }
    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return res;
}
int sceNetAdhocAuth_driver_312BD812(void *buf)
    __attribute__((alias("sceNetAdhocAuth_lib_312BD812")));

/* 0x1d74 */
int __attribute__((optimize("no-crossjumping", "no-reorder-blocks")))
sceNetAdhocAuth_lib_AAB06250(void *dst, uint32_t len)
{
    uint32_t k1, k1_shifted;
    __asm__ volatile("move %0, $k1\n\tsll %1, $k1, 11\n\tmove $k1, %1" : "=r"(k1), "=r"(k1_shifted));

    int res;
    if (dst == NULL || ((int32_t)(k1_shifted & (len | (uint32_t)dst | ((uint32_t)dst + len))) < 0)) {
        res = 0x80410687;
    } else if (g_key_len == 0) {
        res = 0x8041071b;
    } else {
        res = sceMemab_driver_9DE8C8CD(g_key_buf, dst, len);
        if (res < 0) {
            res = 0x80410688;
        }
    }

    __asm__ volatile("move $k1, %0" : : "r"(k1));
    return res;
}
int sceNetAdhocAuth_driver_AAB06250(void *dst, uint32_t len)
    __attribute__((alias("sceNetAdhocAuth_lib_AAB06250")));

/* 0x1e0c */
static int __attribute__((noinline)) sub_1e0c(uint32_t arglen, void *argp)
{
    (void)arglen;
    SceNetAdhocAuthCtx *ctx = *(SceNetAdhocAuthCtx **)argp;
    SceNetAdhocAuthMbxMsg *msg;

    while (sceKernelReceiveMbx(ctx->mbx_id, (void **)&msg, NULL) >= 0) {
        uint32_t type = msg->type;
        SceNetAdhocAuthPeer *peer = NULL;
        if (type < 6 && ((1U << type) & 0x32U) != 0) {
            peer = sub_3bb0(ctx, msg->mac);
            int intr = sceKernelCpuSuspendIntr();
            sub_3fac(ctx, msg);
            sceKernelCpuResumeIntr(intr);
            if (peer == NULL) {
                continue;
            }
            type = msg->type;
        }
        switch (type) {
        case 0: {
            int intr = sceKernelCpuSuspendIntr();
            ctx->flags &= ~1U;
            sceKernelCpuResumeIntr(intr);
            sub_26c8(ctx);
            break;
        }
        case 1:
            sub_255c(ctx, peer);
            break;
        case 2:
            goto done;
        case 3: {
            int intr = sceKernelCpuSuspendIntr();
            ctx->flags &= ~2U;
            sceKernelCpuResumeIntr(intr);
            sub_378c(ctx);
            break;
        }
        case 4:
            sub_23e8(ctx, peer);
            break;
        case 5:
            sub_2324(ctx, peer);
            break;
        case 6:
            sub_2260(ctx);
            goto done;
        default:
            break;
        }
    }

done:
    return 0;
}

/* 0x1f84 */
static void *__attribute__((noinline)) sub_1f84(SceNetAdhocAuthCtx *ctx, int pkt_type,
                                                const void *hdr_data, const void *extra_data,
                                                uint32_t extra_len)
{
    (void)ctx;
    uint8_t type_byte = (uint8_t)pkt_type;
    uint16_t hdr_len = g_pkt_hdr_sizes[type_byte];
    uint8_t *m = (uint8_t *)sceNetIfhandle_driver_15CFE3C0(0x100);
    if (m == NULL) {
        return NULL;
    }

    *(uint32_t *)(m + 0x1c) = 0;
    *(uint16_t *)(m + 0x10) = 1;
    *(uint16_t *)(m + 0x12) = 2;
    *(uint32_t *)(m + 0x00) = 0;
    *(uint32_t *)(m + 0x04) = 0;
    uint8_t *data = m + 0x30;
    *(uint8_t **)(m + 0x08) = data;

    if (hdr_len + extra_len + 0x14 >= 0xd1) {
        void *ext = sceNetIfhandle_driver_15CFE3C0(0x800);
        *(void **)(m + 0x30) = ext;
        if (ext == NULL) {
            ext = sceNetIfhandle_driver_15CFE3C0(0x800);
            *(void **)(m + 0x30) = ext;
        }
        uint16_t fl = *(uint16_t *)(m + 0x12);
        if (ext != NULL) {
            *(void **)(m + 0x08) = ext;
            fl |= 9;
            *(uint16_t *)(m + 0x12) = fl;
            *(uint32_t *)(m + 0x3c) = 0x800;
            *(uint32_t *)(m + 0x34) = 0;
            *(uint32_t *)(m + 0x38) = 0;
            *(void **)(m + 0x48) = m;
            *(void **)(m + 0x44) = m;
        }
        if ((fl & 1) == 0) {
            sceNetIfhandle_driver_F8825DC4(m);
            return NULL;
        }
        data = *(uint8_t **)(m + 0x08);
    }

    uint32_t total_len = hdr_len + extra_len + 0x14;
    *(uint32_t *)(m + 0x0c) = total_len;
    if ((intptr_t)data < 0) {
        sceNetIfhandle_driver_F8825DC4(m);
        return NULL;
    }

    if (hdr_data != NULL) {
        memcpy(data + 0x14, hdr_data, hdr_len);
    }
    if (extra_data != NULL) {
        memcpy(data + 0x14 + hdr_len, extra_data, extra_len);
    }

    data[0x10] = 1;
    data[0x11] = type_byte;
    data[0x12] = (uint8_t)(hdr_len >> 8);
    data[0x13] = (uint8_t)(hdr_len & 0xff);
    *(uint32_t *)(m + 0x14) = 0;
    *(uint32_t *)(m + 0x18) = *(uint32_t *)(m + 0x0c);
    return m;
}

/* 0x2130 */
static void __attribute__((noinline)) sub_2130(SceNetAdhocAuthCtx *ctx, const uint8_t *dst_mac, void *mbuf)
{
    uint8_t *m = (uint8_t *)mbuf;
    uint8_t *data = *(uint8_t **)(m + 0x08);
    if ((intptr_t)data < 0) {
        return;
    }

    memcpy(data + 0x00, dst_mac, 6);
    memcpy(data + 0x06, ctx->local_mac, 6);
    data[0x0c] = g_eth_proto_main[0];
    data[0x0d] = g_eth_proto_main[1];
    data[0x0e] = g_eth_proto_sub[0];
    data[0x0f] = g_eth_proto_sub[1];

    int intr = sceKernelCpuSuspendIntr();
    uint8_t *ifp = (uint8_t *)ctx->ifp;
    *(void **)(m + 0x04) = NULL;
    void *tail = *(void **)(ifp + 0x1c);
    if (tail == NULL) {
        *(void **)(ifp + 0x18) = m;
    } else {
        *(void **)((uint8_t *)tail + 0x04) = m;
    }
    *(void **)(ifp + 0x1c) = m;
    (*(uint32_t *)(ifp + 0x20))++;
    sceKernelCpuResumeIntr(intr);

    void (*start_fn)(void *) = *(void (**)(void *))((uint8_t *)ctx->ifp + 0x10);
    start_fn(ctx->ifp);
}

/* 0x2260 */
static void __attribute__((noinline)) sub_2260(SceNetAdhocAuthCtx *ctx)
{
    uint8_t hdr[0x20];
    int locked = sub_3e98();
    if (sceMemab_driver_F742F283(NULL, 0, hdr, 0) >= 0) {
        uint8_t *m = (uint8_t *)sub_1f84(ctx, 7, hdr, NULL, 0);
        if (m != NULL) {
            *(uint16_t *)(m + 0x12) |= 0x100;
            sub_2130(ctx, g_bcast_mac, m);
        }
    }
    while (ctx->peer_head != NULL) {
        SceNetAdhocAuthPeer *peer = ctx->peer_head;
        sub_3d2c(ctx, peer, 0);
        sub_3c08(ctx, peer);
    }
    sub_3f18(locked);
}

/* 0x2324 */
static void __attribute__((noinline)) sub_2324(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer)
{
    uint8_t hdr[0x20];
    int locked = sub_3e98();
    if (peer != NULL && ctx->state != 2) {
        if (sceMemab_driver_3C15BC8C(peer->memab_ctx, NULL, 0, hdr, 0) >= 0) {
            void *m = sub_1f84(ctx, 7, hdr, NULL, 0);
            if (m != NULL) {
                sub_2130(ctx, peer->hash_mac, m);
            }
        }
        sub_3d2c(ctx, peer, 0);
        sub_3c08(ctx, peer);
    }
    sub_3f18(locked);
}

/* 0x23e8 */
static void __attribute__((noinline)) sub_23e8(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer)
{
    uint8_t hdr[0x20];
    uint32_t payload[4];
    int locked = sub_3e98();
    if (peer != NULL && peer->state == 4 && ctx->state != 2) {
        uint32_t seq = peer->tx_seq + 1;
        peer->tx_seq = seq;
        payload[0] = __builtin_bswap32(seq);
        payload[1] = 0;
        payload[2] = 0;
        payload[3] = 0;
        memset(hdr, 0, sizeof(hdr));
        if (sceMemab_driver_3C15BC8C(peer->memab_ctx, payload, 0x10, hdr, 1) >= 0) {
            void *m = sub_1f84(ctx, 8, hdr, NULL, 0);
            if (m != NULL) {
                sub_2130(ctx, peer->hash_mac, m);
            }
        }
        int intr = sceKernelCpuSuspendIntr();
        if (peer->keepalive_tx_alarm > 0) {
            sceKernelCancelAlarm(peer->keepalive_tx_alarm);
            peer->keepalive_tx_alarm = 0;
        }
        peer->keepalive_tx_alarm = sceKernelSetAlarm(ctx->keepalive_tx_interval, sub_39d4, peer);
        sceKernelCpuResumeIntr(intr);
    }
    sub_3f18(locked);
}

/* 0x255c */
static void __attribute__((noinline)) sub_255c(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer)
{
    int locked = sub_3e98();
    if (peer == NULL || (uint32_t)(peer->state - 1) >= 3U) {
        sub_3f18(locked);
        return;
    }

    if (peer->retry_count >= ctx->max_retries || ctx->state == 2) {
        sub_3d2c(ctx, peer, 0);
        sub_3c08(ctx, peer);
        sub_3f18(locked);
        return;
    }

    if (peer->state == 1) {
        void *m = sub_1f84(ctx, 3, peer->msg1_data, ctx->tlv_buf, ctx->tlv_len);
        if (m != NULL) {
            sub_2130(ctx, peer->hash_mac, m);
        }
    } else if (peer->state == 2) {
        void *m = sub_1f84(ctx, 5, peer->msg5_data, NULL, 0);
        if (m != NULL) {
            sub_2130(ctx, peer->hash_mac, m);
        }
    }

    peer->retry_count++;
    int intr = sceKernelCpuSuspendIntr();
    if (peer->retry_alarm > 0) {
        sceKernelCancelAlarm(peer->retry_alarm);
        peer->retry_alarm = 0;
    }
    peer->retry_alarm = sceKernelSetAlarm(ctx->retry_interval, sub_38e4, peer);
    sceKernelCpuResumeIntr(intr);
    sub_3f18(locked);
}

/* 0x26c8 */
static void __attribute__((noinline)) sub_26c8(SceNetAdhocAuthCtx *ctx)
{
    uint8_t tmp_ctx[0xc0];
    uint8_t out_buf[0x220];

    for (;;) {
        int intr = sceKernelCpuSuspendIntr();
        uint8_t *m = (uint8_t *)g_rx_queue.head;
        if (m != NULL) {
            void *next = *(void **)(m + 0x04);
            g_rx_queue.head = next;
            if (next == NULL) {
                g_rx_queue.tail = NULL;
            }
            *(void **)(m + 0x04) = NULL;
            g_rx_queue.count--;
        }
        sceKernelCpuResumeIntr(intr);

        if (m == NULL) {
            break;
        }

        int pkt_len = *(int *)(m + 0x18);
        int first_len = *(int *)(m + 0x0c);
        uint8_t *raw = *(uint8_t **)(m + 0x08);
        if (pkt_len != first_len) {
            raw = (uint8_t *)sceNetIfhandle_driver_15CFE3C0(pkt_len);
            if (raw == NULL) {
                sceNetIfhandle_driver_F56FAC82(m);
                continue;
            }
            sceNetIfhandle_driver_9A6261EC(m, 0, pkt_len, raw);
            pkt_len = *(int *)(m + 0x18);
        } else if ((intptr_t)raw < 0) {
            sceNetIfhandle_driver_F56FAC82(m);
            continue;
        }

        const uint8_t *src_mac = raw + 6;
        if (memcmp(ctx->local_mac, src_mac, 6) != 0 && raw[0x10] == 1) {
            uint8_t pkt_type = raw[0x11];
            if ((uint8_t)(pkt_type - 1) < 8U) {
                uint16_t hdr_len = ((uint16_t)raw[0x12] << 8) | raw[0x13];
                if (hdr_len == g_pkt_hdr_sizes[pkt_type] && pkt_len >= (int)hdr_len + 0x14) {
                    int extra_len = pkt_len - hdr_len - 0x14;
                    int locked = sub_3e98();
                    SceNetAdhocAuthPeer *peer = sub_3bb0(ctx, src_mac);
                    if (peer != NULL && peer->state == 4) {
                        peer->timestamp = sceKernelGetSystemTimeWide();
                    }
                    switch (pkt_type) {
                    case 1:
                    case 2:
                        if (ctx->state == 1 && ctx->peer_count < ctx->max_peers && peer == NULL) {
                            peer = (SceNetAdhocAuthPeer *)sceNetIfhandle_driver_15CFE3C0(sizeof(SceNetAdhocAuthPeer));
                            if (peer != NULL) {
                                sub_3a4c(ctx, peer, src_mac, raw + 0x14);
                                sub_3ff8(peer, raw + 0x94, extra_len);
                                if (sceMemab_driver_4B54EAAD(peer->memab_ctx, out_buf) < 0) {
                                    sub_3c08(ctx, peer);
                                } else {
                                    memcpy(out_buf + 0xa0, ctx->ibss, 0x80);
                                    void *tx_m = sub_1f84(ctx, 3, out_buf, ctx->tlv_buf, ctx->tlv_len);
                                    memcpy(peer->msg1_data, out_buf, 0x120);
                                    if (tx_m != NULL) {
                                        sub_2130(ctx, peer->hash_mac, tx_m);
                                    }
                                    peer->retry_count = 0;
                                    int s = sceKernelCpuSuspendIntr();
                                    if (peer->retry_alarm > 0) {
                                        sceKernelCancelAlarm(peer->retry_alarm);
                                        peer->retry_alarm = 0;
                                    }
                                    peer->retry_alarm = sceKernelSetAlarm(ctx->retry_interval, sub_38e4, peer);
                                    sceKernelCpuResumeIntr(s);
                                    sub_3d2c(ctx, peer, 1);
                                }
                            }
                        }
                        break;
                    case 3:
                        if (ctx->state == 1 && ctx->peer_count < ctx->max_peers &&
                            sceMemab_driver_9BF0C95D(tmp_ctx, raw + 0x14, out_buf) >= 0) {
                            if (peer == NULL) {
                                peer = (SceNetAdhocAuthPeer *)sceNetIfhandle_driver_15CFE3C0(sizeof(SceNetAdhocAuthPeer));
                                if (peer == NULL) {
                                    break;
                                }
                                sub_3a4c(ctx, peer, src_mac, raw + 0xb4);
                                sub_3ff8(peer, raw + 0x134, extra_len);
                            } else {
                                if (peer->state == 3) {
                                    if (memcmp(peer->msg1_data, raw + 0x14, 0x120) != 0) {
                                        int s = sceKernelCpuSuspendIntr();
                                        if (peer->retry_alarm > 0) {
                                            sceKernelCancelAlarm(peer->retry_alarm);
                                            peer->retry_alarm = 0;
                                        }
                                        sceKernelCpuResumeIntr(s);
                                        sub_3d2c(ctx, peer, 0);
                                    }
                                } else if (peer->state == 1) {
                                    for (int i = 0; i < 6; i++) {
                                        if (ctx->local_mac[i] < peer->hash_mac[i]) {
                                            break;
                                        }
                                        if (peer->hash_mac[i] < ctx->local_mac[i]) {
                                            int s = sceKernelCpuSuspendIntr();
                                            if (peer->retry_alarm > 0) {
                                                sceKernelCancelAlarm(peer->retry_alarm);
                                                peer->retry_alarm = 0;
                                            }
                                            sceKernelCpuResumeIntr(s);
                                            sub_3d2c(ctx, peer, 0);
                                            break;
                                        }
                                    }
                                } else if (peer->state == 2) {
                                    int s = sceKernelCpuSuspendIntr();
                                    if (peer->retry_alarm > 0) {
                                        sceKernelCancelAlarm(peer->retry_alarm);
                                        peer->retry_alarm = 0;
                                    }
                                    sceKernelCpuResumeIntr(s);
                                    sub_3d2c(ctx, peer, 0);
                                } else if (peer->state == 4) {
                                    int s = sceKernelCpuSuspendIntr();
                                    if (peer->keepalive_rx_alarm > 0) {
                                        sceKernelCancelAlarm(peer->keepalive_rx_alarm);
                                        peer->keepalive_rx_alarm = 0;
                                    }
                                    sceKernelCpuResumeIntr(s);
                                    s = sceKernelCpuSuspendIntr();
                                    if (peer->keepalive_tx_alarm > 0) {
                                        sceKernelCancelAlarm(peer->keepalive_tx_alarm);
                                        peer->keepalive_tx_alarm = 0;
                                    }
                                    sceKernelCpuResumeIntr(s);
                                    s = sceKernelCpuSuspendIntr();
                                    if (peer->retry_alarm > 0) {
                                        sceKernelCancelAlarm(peer->retry_alarm);
                                        peer->retry_alarm = 0;
                                    }
                                    sceKernelCpuResumeIntr(s);
                                    sub_3d2c(ctx, peer, 0);
                                }
                                memcpy(peer->ibss, raw + 0xb4, 0x80);
                                sub_3ff8(peer, raw + 0x134, extra_len);
                            }

                            if (peer->state == 0) {
                                memcpy(peer->memab_ctx, tmp_ctx, 0xc0);
                                void *tx_m = sub_1f84(ctx, 4, out_buf, NULL, 0);
                                if (tx_m != NULL) {
                                    sub_2130(ctx, peer->hash_mac, tx_m);
                                }
                                peer->retry_count = 0;
                                int s = sceKernelCpuSuspendIntr();
                                if (peer->retry_alarm > 0) {
                                    sceKernelCancelAlarm(peer->retry_alarm);
                                    peer->retry_alarm = 0;
                                }
                                peer->retry_alarm = sceKernelSetAlarm(ctx->retry_interval, sub_38e4, peer);
                                sceKernelCpuResumeIntr(s);
                                memcpy(peer->msg1_data, raw + 0x14, 0x120);
                                memcpy(peer->msg4_data, out_buf, 0x110);
                                sub_3d2c(ctx, peer, 3);
                            } else if (peer->state == 3) {
                                void *tx_m = sub_1f84(ctx, 4, peer->msg4_data, NULL, 0);
                                if (tx_m != NULL) {
                                    sub_2130(ctx, peer->hash_mac, tx_m);
                                }
                                peer->retry_count = 0;
                                int s = sceKernelCpuSuspendIntr();
                                if (peer->retry_alarm > 0) {
                                    sceKernelCancelAlarm(peer->retry_alarm);
                                    peer->retry_alarm = 0;
                                }
                                peer->retry_alarm = sceKernelSetAlarm(ctx->retry_interval, sub_38e4, peer);
                                sceKernelCpuResumeIntr(s);
                            }
                        }
                        break;
                    case 4:
                        if (ctx->state == 1 && peer != NULL && peer->state == 1) {
                            memcpy(tmp_ctx, peer->memab_ctx, 0xc0);
                            if (sceMemab_driver_C3981EE1(tmp_ctx, raw + 0x14, out_buf) >= 0) {
                                int s = sceKernelCpuSuspendIntr();
                                if (peer->retry_alarm > 0) {
                                    sceKernelCancelAlarm(peer->retry_alarm);
                                    peer->retry_alarm = 0;
                                }
                                sceKernelCpuResumeIntr(s);
                                memcpy(peer->memab_ctx, tmp_ctx, 0xc0);
                                void *tx_m = sub_1f84(ctx, 5, out_buf, NULL, 0);
                                memcpy(peer->msg5_data, out_buf, 0x100);
                                if (tx_m != NULL) {
                                    sub_2130(ctx, peer->hash_mac, tx_m);
                                }
                                peer->retry_count = 0;
                                s = sceKernelCpuSuspendIntr();
                                if (peer->retry_alarm > 0) {
                                    sceKernelCancelAlarm(peer->retry_alarm);
                                    peer->retry_alarm = 0;
                                }
                                peer->retry_alarm = sceKernelSetAlarm(ctx->retry_interval, sub_38e4, peer);
                                sceKernelCpuResumeIntr(s);
                                sub_3d2c(ctx, peer, 2);
                            }
                        }
                        break;
                    case 5:
                        if (ctx->state == 1 && peer != NULL && peer->state == 3) {
                            memcpy(tmp_ctx, peer->memab_ctx, 0xc0);
                            if (sceMemab_driver_23E4659B(tmp_ctx, raw + 0x14, out_buf) >= 0) {
                                int s = sceKernelCpuSuspendIntr();
                                if (peer->retry_alarm > 0) {
                                    sceKernelCancelAlarm(peer->retry_alarm);
                                    peer->retry_alarm = 0;
                                }
                                sceKernelCpuResumeIntr(s);
                                memcpy(peer->memab_ctx, tmp_ctx, 0xc0);
                                void *tx_m = sub_1f84(ctx, 6, out_buf, NULL, 0);
                                peer->tx_seq = 0;
                                peer->rx_seq = 0;
                                if (tx_m != NULL) {
                                    sub_2130(ctx, peer->hash_mac, tx_m);
                                }
                                s = sceKernelCpuSuspendIntr();
                                if (peer->keepalive_rx_alarm > 0) {
                                    sceKernelCancelAlarm(peer->keepalive_rx_alarm);
                                    peer->keepalive_rx_alarm = 0;
                                }
                                peer->keepalive_rx_alarm = sceKernelSetAlarm(ctx->keepalive_rx_interval, sub_395c, peer);
                                sceKernelCpuResumeIntr(s);

                                s = sceKernelCpuSuspendIntr();
                                if (peer->keepalive_tx_alarm > 0) {
                                    sceKernelCancelAlarm(peer->keepalive_tx_alarm);
                                    peer->keepalive_tx_alarm = 0;
                                }
                                peer->keepalive_tx_alarm = sceKernelSetAlarm(ctx->keepalive_tx_interval, sub_39d4, peer);
                                sceKernelCpuResumeIntr(s);
                                sub_3d2c(ctx, peer, 4);
                            }
                        }
                        break;
                    case 6:
                        if (ctx->state == 1 && peer != NULL && peer->state == 2) {
                            memcpy(tmp_ctx, peer->memab_ctx, 0xc0);
                            if (sceMemab_driver_8ABE3445(tmp_ctx, raw + 0x14) >= 0) {
                                int s = sceKernelCpuSuspendIntr();
                                if (peer->retry_alarm > 0) {
                                    sceKernelCancelAlarm(peer->retry_alarm);
                                    peer->retry_alarm = 0;
                                }
                                sceKernelCpuResumeIntr(s);
                                memcpy(peer->memab_ctx, tmp_ctx, 0xc0);
                                peer->tx_seq = 0;
                                peer->rx_seq = 0;

                                s = sceKernelCpuSuspendIntr();
                                if (peer->keepalive_rx_alarm > 0) {
                                    sceKernelCancelAlarm(peer->keepalive_rx_alarm);
                                    peer->keepalive_rx_alarm = 0;
                                }
                                peer->keepalive_rx_alarm = sceKernelSetAlarm(ctx->keepalive_rx_interval, sub_395c, peer);
                                sceKernelCpuResumeIntr(s);

                                s = sceKernelCpuSuspendIntr();
                                if (peer->keepalive_tx_alarm > 0) {
                                    sceKernelCancelAlarm(peer->keepalive_tx_alarm);
                                    peer->keepalive_tx_alarm = 0;
                                }
                                peer->keepalive_tx_alarm = sceKernelSetAlarm(ctx->keepalive_tx_interval, sub_39d4, peer);
                                sceKernelCpuResumeIntr(s);
                                sub_3d2c(ctx, peer, 4);
                            }
                        }
                        break;
                    case 7:
                        if (peer != NULL) {
                            int verify_res;
                            if (memcmp(raw, g_bcast_mac, 6) == 0) {
                                verify_res = sceMemab_driver_CB5D3916(peer->memab_ctx, NULL, 0, raw + 0x14, 0, 0);
                            } else {
                                verify_res = sceMemab_driver_D47A50B1(peer->memab_ctx, NULL, 0, raw + 0x14, 0, 0);
                            }
                            if (verify_res >= 0) {
                                sub_3d2c(ctx, peer, 0);
                                sub_3c08(ctx, peer);
                            }
                        }
                        break;
                    case 8:
                        if (peer != NULL && peer->state == 4) {
                            if (sceMemab_driver_D47A50B1(peer->memab_ctx, raw + 0x34, 0x10, raw + 0x14, 1, 0) >= 0) {
                                uint32_t seq;
                                memcpy(&seq, raw + 0x34, 4);
                                seq = __builtin_bswap32(seq);
                                if (peer->rx_seq < seq) {
                                    peer->rx_seq = seq;
                                    int s = sceKernelCpuSuspendIntr();
                                    if (peer->keepalive_rx_alarm > 0) {
                                        sceKernelCancelAlarm(peer->keepalive_rx_alarm);
                                        peer->keepalive_rx_alarm = 0;
                                    }
                                    peer->keepalive_rx_alarm = sceKernelSetAlarm(ctx->keepalive_rx_interval, sub_395c, peer);
                                    sceKernelCpuResumeIntr(s);
                                }
                            }
                        }
                        break;
                    default:
                        break;
                    }
                    sub_3f18(locked);
                }
            }
        }

        if (*(int *)(m + 0x18) != *(int *)(m + 0x0c)) {
            sceNetIfhandle_driver_76BAD213(raw);
        }
        sceNetIfhandle_driver_F56FAC82(m);
    }
}

/* 0x378c */
static void __attribute__((noinline)) sub_378c(SceNetAdhocAuthCtx *ctx)
{
    int locked = sub_3e98();
    if (ctx->state != 1) {
        sub_3f18(locked);
        return;
    }

    void *m;
    if (ctx->peer_count < ctx->max_peers) {
        m = sub_1f84(ctx, 2, ctx->ibss, ctx->tlv_buf, ctx->tlv_len);
    } else {
        m = sub_1f84(ctx, 1, NULL, NULL, 0);
    }
    if (m != NULL) {
        *(uint16_t *)((uint8_t *)m + 0x12) |= 0x100;
        sub_2130(ctx, g_bcast_mac, m);
    }

    int intr = sceKernelCpuSuspendIntr();
    if (ctx->beacon_alarm > 0) {
        sceKernelCancelAlarm(ctx->beacon_alarm);
        ctx->beacon_alarm = 0;
    }
    ctx->beacon_alarm = sceKernelSetAlarm(ctx->beacon_interval, sub_389c, ctx);
    sceKernelCpuResumeIntr(intr);
    sub_3f18(locked);
}

/* 0x389c */
static uint32_t __attribute__((noinline)) sub_389c(void *common)
{
    SceNetAdhocAuthCtx *ctx = (SceNetAdhocAuthCtx *)common;
    if ((ctx->flags & 2) == 0) {
        ctx->flags |= 2;
        sceKernelSendMbx(ctx->mbx_id, &ctx->beacon_mbx_msg);
    }
    ctx->beacon_alarm = 0;
    return 0;
}

/* 0x38e4 */
static uint32_t __attribute__((noinline)) sub_38e4(void *common)
{
    SceNetAdhocAuthPeer *peer = (SceNetAdhocAuthPeer *)common;
    SceNetAdhocAuthMbxMsg *msg = sub_3f64(peer->ctx);
    if (msg != NULL) {
        msg->type = 1;
        memcpy(msg->mac, peer->hash_mac, 6);
        sceKernelSendMbx(peer->ctx->mbx_id, msg);
    }
    peer->retry_alarm = 0;
    return 0;
}

/* 0x395c */
static uint32_t __attribute__((noinline)) sub_395c(void *common)
{
    SceNetAdhocAuthPeer *peer = (SceNetAdhocAuthPeer *)common;
    SceNetAdhocAuthMbxMsg *msg = sub_3f64(peer->ctx);
    if (msg != NULL) {
        msg->type = 5;
        memcpy(msg->mac, peer->hash_mac, 6);
        sceKernelSendMbx(peer->ctx->mbx_id, msg);
    }
    peer->keepalive_rx_alarm = 0;
    return 0;
}

/* 0x39d4 */
static uint32_t __attribute__((noinline)) sub_39d4(void *common)
{
    SceNetAdhocAuthPeer *peer = (SceNetAdhocAuthPeer *)common;
    SceNetAdhocAuthMbxMsg *msg = sub_3f64(peer->ctx);
    if (msg != NULL) {
        msg->type = 4;
        memcpy(msg->mac, peer->hash_mac, 6);
        sceKernelSendMbx(peer->ctx->mbx_id, msg);
    }
    peer->keepalive_tx_alarm = 0;
    return 0;
}

/* 0x3a4c */
static void __attribute__((noinline)) sub_3a4c(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer,
                                               const uint8_t *mac, const uint8_t *ibss)
{
    memset(peer, 0, sizeof(SceNetAdhocAuthPeer));
    memcpy(peer->hash_mac, mac, 6);
    memcpy(peer->ibss, ibss, 0x80);
    peer->next = NULL;
    peer->prev_ptr = ctx->peer_tail_ptr;
    ctx->peer_count++;
    *ctx->peer_tail_ptr = peer;
    ctx->peer_tail_ptr = &peer->next;
    peer->state = 0;
    peer->ctx = ctx;
}

/* 0x3bb0 */
static SceNetAdhocAuthPeer *__attribute__((noinline)) sub_3bb0(SceNetAdhocAuthCtx *ctx, const uint8_t *mac)
{
    SceNetAdhocAuthPeer *peer = ctx->peer_head;
    while (peer != NULL) {
        if (memcmp(peer->hash_mac, mac, 6) == 0) {
            return peer;
        }
        peer = peer->next;
    }
    return NULL;
}

/* 0x3c08 */
static void __attribute__((noinline)) sub_3c08(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer)
{
    if (peer == NULL) {
        return;
    }

    int intr = sceKernelCpuSuspendIntr();
    if (peer->keepalive_tx_alarm > 0) {
        sceKernelCancelAlarm(peer->keepalive_tx_alarm);
        peer->keepalive_tx_alarm = 0;
    }
    sceKernelCpuResumeIntr(intr);

    intr = sceKernelCpuSuspendIntr();
    if (peer->retry_alarm > 0) {
        sceKernelCancelAlarm(peer->retry_alarm);
        peer->retry_alarm = 0;
    }
    sceKernelCpuResumeIntr(intr);

    intr = sceKernelCpuSuspendIntr();
    if (peer->keepalive_rx_alarm > 0) {
        sceKernelCancelAlarm(peer->keepalive_rx_alarm);
        peer->keepalive_rx_alarm = 0;
    }
    sceKernelCpuResumeIntr(intr);

    SceNetAdhocAuthPeer *next = peer->next;
    if (next != NULL) {
        next->prev_ptr = peer->prev_ptr;
    } else {
        ctx->peer_tail_ptr = peer->prev_ptr;
    }
    *peer->prev_ptr = next;
    ctx->peer_count--;
    sceNetIfhandle_driver_76BAD213(peer);
}

/* 0x3d2c */
static void __attribute__((noinline)) sub_3d2c(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthPeer *peer, int new_state)
{
    (void)ctx;
    sceKernelWaitSema(g_hash_sema, 1, NULL);
    int old_state = peer->state;
    if (old_state != 4 && new_state == 4) {
        sub_1980(peer->memab_ctx);
        peer->timestamp = sceKernelGetSystemTimeWide();
        old_state = peer->state;
    }
    if (old_state == 4 && new_state != 4) {
        sub_1a68(peer->memab_ctx);
    }
    sceKernelSignalSema(g_hash_sema, 1);
    peer->state = new_state;
}

/* 0x3e00 */
static void __attribute__((noinline)) sub_3e00(SceNetAdhocAuthCtx *ctx, int state)
{
    ctx->state = state;
}

/* 0x3e08 */
static int __attribute__((noinline)) sub_3e08(void)
{
    int sema = sceKernelCreateSema("SceNetAdhocAuthLock", 1, 1, 1, NULL);
    if (sema < 0) {
        return sema;
    }
    g_lock_sema = sema;
    g_lock_owner_thid = -1;
    return 0;
}

/* 0x3e5c */
static int __attribute__((noinline)) sub_3e5c(void)
{
    if (g_lock_sema >= 0) {
        sceKernelDeleteSema(g_lock_sema);
    }
    g_lock_sema = -1;
    return 0;
}

/* 0x3e98 */
static int __attribute__((noinline)) sub_3e98(void)
{
    int thid = sceKernelGetThreadId();
    if (thid < 0) {
        return thid;
    }
    if (thid == g_lock_owner_thid) {
        return 0;
    }
    int res = sceKernelWaitSema(g_lock_sema, 1, NULL);
    if (res < 0) {
        return res;
    }
    g_lock_owner_thid = thid;
    return 1;
}

/* 0x3f18 */
static void __attribute__((noinline)) sub_3f18(int locked)
{
    if (locked != 0) {
        g_lock_owner_thid = -1;
        sceKernelSignalSema(g_lock_sema, 1);
    }
}

/* 0x3f40 */
static void __attribute__((noinline)) sub_3f40(void)
{
    if (g_lock_sema >= 0) {
        sceKernelSignalSema(g_lock_sema, 1);
    }
}

/* 0x3f64 */
static SceNetAdhocAuthMbxMsg *__attribute__((noinline)) sub_3f64(SceNetAdhocAuthCtx *ctx)
{
    uint32_t bitmap = ctx->msg_pool_bitmap;
    for (int i = 0; i < 16; i++) {
        uint32_t mask = 1U << i;
        if ((bitmap & mask) == 0) {
            ctx->msg_pool_bitmap = bitmap | mask;
            return &ctx->msg_pool[i];
        }
    }
    return NULL;
}

/* 0x3fac */
static void __attribute__((noinline)) sub_3fac(SceNetAdhocAuthCtx *ctx, SceNetAdhocAuthMbxMsg *msg)
{
    for (int i = 0; i < 16; i++) {
        if (&ctx->msg_pool[i] == msg) {
            ctx->msg_pool_bitmap &= ~(1U << i);
            return;
        }
    }
}

/* 0x3ff8 */
static void __attribute__((noinline)) sub_3ff8(SceNetAdhocAuthPeer *peer, const uint8_t *buf, int len)
{
    if (len <= 1) {
        return;
    }
    int offset = 0;
    while (offset < len) {
        int rem = len - offset;
        if (rem < 2) {
            return;
        }
        const uint8_t *p = buf + offset;
        uint8_t tag = p[0];
        int8_t signed_len = (int8_t)p[1];
        uint32_t item_len = (uint8_t)signed_len;
        if (rem < (int)(item_len + 2)) {
            return;
        }
        if (tag == 2) {
            if (signed_len == 4) {
                memcpy(&peer->peer_sdk_ver, p + 2, item_len);
            }
        } else if (tag == 3) {
            if (signed_len == 4) {
                memcpy(&peer->peer_model, p + 2, item_len);
            }
        } else if (tag == 1) {
            if (signed_len == 4) {
                memcpy(&peer->peer_devkit_ver, p + 2, item_len);
            }
        }
        offset += item_len + 2;
    }
}

/* 0x412c */
static void __attribute__((noinline)) sub_412c(SceNetAdhocAuthCtx *ctx, uint8_t *dst, uint32_t *out_len)
{
    dst[0] = 1;
    dst[1] = 4;
    memcpy(dst + 2, &ctx->devkit_ver, 4);
    dst[6] = 2;
    dst[7] = 4;
    memcpy(dst + 8, &ctx->sdk_ver, 4);
    dst[12] = 3;
    dst[13] = 4;
    memcpy(dst + 14, &ctx->model, 4);
    *out_len = 0x12;
}

/* 0x41b4 */
int module_start(uint32_t args, void *argp)
{
    (void)args;
    (void)argp;
    return 0;
}

/* 0x41bc */
int module_stop(uint32_t args, void *argp)
{
    (void)args;
    (void)argp;
    return 0;
}
