/*
 * ePSP 6.61 sceNetAdhocDownload_Library (pspnet_adhoc_download.prx)
 */

#include <stdint.h>
#include <stddef.h>

/* Imports from Kernel_Library */
int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int state);
void *sceKernelMemset(void *s, int c, size_t n);
void *sceKernelMemcpy(void *dst, const void *src, size_t n);
int _sceKernelCheckThreadStack(void);
int sceKernelLockLwMutex(void *workarea, int count, uint32_t *timeout);
int sceKernelUnlockLwMutex(void *workarea, int count);

/* Imports from ThreadManForUser */
int sceKernelDelayThread(uint32_t delay);
int sceKernelCreateSema(const char *name, uint32_t attr, int init_val, int max_val, void *option);
int sceKernelDeleteSema(int semaid);
int sceKernelSignalSema(int semaid, int signal);
int sceKernelWaitSema(int semaid, int signal, uint32_t *timeout);
int sceKernelCancelSema(int semaid, int new_count, int *num_wait_threads);
int sceKernelCreateEventFlag(const char *name, uint32_t attr, uint32_t bits, void *opt);
int sceKernelDeleteEventFlag(int evid);
int sceKernelSetEventFlag(int evid, uint32_t bits);
int sceKernelWaitEventFlag(int evid, uint32_t bits, uint32_t wait, uint32_t *out_bits, uint32_t *timeout);
int sceKernelPollEventFlag(int evid, uint32_t bits, uint32_t wait, uint32_t *out_bits);
int sceKernelCreateThread(const char *name, int (*entry)(uint32_t, void *), int init_priority,
                          int stack_size, uint32_t attr, void *option);
int sceKernelDeleteThread(int thid);
int sceKernelStartThread(int thid, uint32_t arglen, void *argp);
int sceKernelWaitThreadEnd(int thid, uint32_t *timeout);
int sceKernelCreateLwMutex(void *workarea, const char *name, uint32_t attr, int initial_count, void *opt);
int sceKernelDeleteLwMutex(void *workarea);
int sceKernelCreateVpl(const char *name, int part, uint32_t attr, uint32_t size, void *opt);
int sceKernelDeleteVpl(int uid);
int sceKernelTryAllocateVpl(int uid, uint32_t size, void **data);
int sceKernelFreeVpl(int uid, void *data);
int sceKernelReferVplStatus(int uid, void *info);

/* Imports from sceUtility */
int sceUtilityGetSystemParamString(int id, char *str, int len);

/* Imports from sceNet / sceNet_lib */
int sceNetGetLocalEtherAddr(uint8_t *mac);
int sceNetFreeThreadinfo(int thid);
int sceNet_lib_E0A81C7C(const void *a, const void *b, size_t len);

/* Imports from sceNetAdhoc */
int sceNetAdhocPtpOpen(const uint8_t *srcmac, uint16_t srcport, const uint8_t *destmac,
                       uint16_t destport, uint32_t bufsize, uint32_t delay, int count, int unk);
int sceNetAdhocPtpConnect(int id, uint32_t timeout, int nonblock);
int sceNetAdhocPtpListen(const uint8_t *srcmac, uint16_t srcport, uint32_t bufsize,
                         uint32_t delay, int count, int queue, int unk);
int sceNetAdhocPtpAccept(int id, uint8_t *mac, uint16_t *port, uint32_t timeout, int nonblock);
int sceNetAdhocPtpSend(int id, const void *data, int *datasize, uint32_t timeout, int nonblock);
int sceNetAdhocPtpRecv(int id, void *data, int *datasize, uint32_t timeout, int nonblock);
int sceNetAdhocPtpFlush(int id, uint32_t timeout, int nonblock);
int sceNetAdhocPtpClose(int id, int unk);
int sceNetAdhoc_73BFD52D(int id, int flags);

/* Imports from sceNetAdhocMatching */
int sceNetAdhocMatchingCreate(int mode, int maxpeers, uint16_t port, int bufsize,
                              uint32_t hellodelay, uint32_t pingdelay, int initcount,
                              uint32_t msgdelay, void *callback);
int sceNetAdhocMatchingDelete(int matchingid);
int sceNetAdhocMatching_E8454C65(int matchingid, int evth_pri, int evth_stack, int evth_part,
                                 int inth_pri, int inth_stack, int inth_part, int optlen, void *optdata);
int sceNetAdhocMatchingStop(int matchingid);
int sceNetAdhocMatchingSelectTarget(int matchingid, const uint8_t *mac, int optlen, void *optdata);
int sceNetAdhocMatchingCancelTarget(int matchingid, const uint8_t *mac);
int sceNetAdhocMatchingSetHelloOpt(int matchingid, int optlen, void *optdata);

/* Data structures */
typedef struct SceNetAdhocDownloadQueueItem {
    struct SceNetAdhocDownloadQueueItem *next; /* 0x00 */
    char nickname[0x80];                       /* 0x04 */
    uint8_t mac[6];                            /* 0x84 */
    uint16_t pad;                              /* 0x8a */
} SceNetAdhocDownloadQueueItem; /* 0x8c */

typedef struct SceNetAdhocDownloadQueue {
    SceNetAdhocDownloadQueueItem *head; /* 0x00 */
    SceNetAdhocDownloadQueueItem *tail; /* 0x04 */
} SceNetAdhocDownloadQueue;

typedef struct SceNetAdhocDownloadServerInfo {
    uint32_t size;     /* 0x00 */
    uint32_t unk_04;   /* 0x04 */
    uint32_t total;    /* 0x08 */
    uint32_t sent;     /* 0x0c */
} SceNetAdhocDownloadServerInfo;

typedef struct SceNetAdhocDownloadServerCtx {
    int state;                                                /* 0x00 */
    uint32_t opt_len;                                         /* 0x04 */
    void *opt_buf;                                            /* 0x08 */
    void (*req_cb)(void *arg, uint8_t *mac, char *nickname);  /* 0x0c */
    SceNetAdhocDownloadServerInfo *info;                      /* 0x10 */
    int matching_id;                                          /* 0x14 */
    int evf_id;                                               /* 0x18 */
    int ptp_id;                                               /* 0x1c */
    uint8_t peer_mac[6];                                      /* 0x20 */
    char peer_nickname[0x80];                                 /* 0x26 */
    uint16_t pad_a6;                                          /* 0xa6 */
    SceNetAdhocDownloadQueue queue;                           /* 0xa8 */
} SceNetAdhocDownloadServerCtx; /* 0xb0 */

typedef struct SceNetAdhocDownloadClientPeer {
    struct SceNetAdhocDownloadClientPeer *next; /* 0x00 */
    uint32_t raw_len;                           /* 0x04 */
    uint8_t *raw_buf;                           /* 0x08 */
    uint8_t mac[6];                             /* 0x0c */
    char nickname[0x80];                        /* 0x12 */
    uint16_t pad_92;                            /* 0x92 */
    uint32_t data_len;                          /* 0x94 */
    void *data_ptr;                             /* 0x98 */
    void *extra_ptr;                            /* 0x9c */
} SceNetAdhocDownloadClientPeer; /* 0xa0 */

typedef struct SceNetAdhocDownloadClientCtx {
    int state;                                                      /* 0x00 */
    uint32_t opt_len;                                               /* 0x04 */
    void *opt_buf;                                                  /* 0x08 */
    void (*hello_cb)(void *arg, uint8_t *mac, char *nick,
                     uint32_t dlen, void *dptr, void *eptr);        /* 0x0c */
    void *cb_arg;                                                   /* 0x10 */
    int matching_id;                                                /* 0x14 */
    int evf_id;                                                     /* 0x18 */
    int ptp_id;                                                     /* 0x1c */
    uint8_t target_mac[6];                                          /* 0x20 */
    uint16_t pad_26;                                                /* 0x26 */
} SceNetAdhocDownloadClientCtx; /* 0x28 */

typedef struct SceNetAdhocDistributeWorker {
    int thid;            /* 0x00 */
    int ptp_id;          /* 0x04 */
    int evf_id;          /* 0x08 */
    int state;           /* 0x0c */
    int error_code;      /* 0x10 */
    uint8_t mac[6];      /* 0x14 */
    uint16_t pad_1a;     /* 0x1a */
    char nickname[0x80]; /* 0x1c */
    uint32_t total_len;  /* 0x9c */
    uint32_t sent_len;   /* 0xa0 */
} SceNetAdhocDistributeWorker; /* 0xa4 */

typedef struct SceNetAdhocDistributeCtx {
    int state;                                                           /* 0x00 */
    int max_workers;                                                     /* 0x04 */
    uint32_t opt_len;                                                    /* 0x08 */
    void *opt_buf;                                                       /* 0x0c */
    int total_data_len;                                                  /* 0x10 */
    void *cb_arg;                                                        /* 0x14 */
    int (*send_cb)(int worker_idx, void *arg, int offset,
                   void **buf_out, int *len_out);                        /* 0x18 */
    int matching_id;                                                     /* 0x1c */
    int queue_sema;                                                      /* 0x20 */
    int max_concurrent;                                                  /* 0x24 */
    int concurrent_sema;                                                 /* 0x28 */
    SceNetAdhocDistributeWorker *workers;                                /* 0x2c */
    SceNetAdhocDownloadQueue queue;                                      /* 0x30 */
} SceNetAdhocDistributeCtx; /* 0x38 */

/* .bss globals (0x4470..0x45c8) */
static int g_server_inited;                                 /* 0x4470 */
static int g_client_inited;                                 /* 0x4474 */
static int g_distribute_inited;                             /* 0x4478 */
static SceNetAdhocDownloadServerCtx *g_server_ctx;          /* 0x447c */
static SceNetAdhocDistributeCtx *g_distribute_ctx;          /* 0x4480 */
static SceNetAdhocDownloadServerCtx g_server_ctx_buf;       /* 0x4484 (0xb0 bytes) */
static SceNetAdhocDistributeCtx g_distribute_ctx_buf;       /* 0x4534 (0x38 bytes) */
static int g_client_abort_flag;                             /* 0x456c */
static SceNetAdhocDownloadClientCtx *g_client_ctx;          /* 0x4570 */
static SceNetAdhocDownloadClientPeer *g_client_peer_list;   /* 0x4574 */
static SceNetAdhocDownloadClientCtx g_client_ctx_buf;       /* 0x4578 (0x28 bytes) */
static uint32_t g_client_lwmutex[8];                        /* 0x45a0 (0x20 bytes) */
static int g_vpl_id;                                        /* 0x45c0 */
static uint32_t g_max_vpl_used;                             /* 0x45c4 */

/* Forward declarations of internal helpers */
static void __attribute__((noinline)) sub_0928(int matching_id, int event, uint8_t *mac, int optlen, void *optdata);
static int __attribute__((noinline)) sub_1028(int evth_pri, int evth_stack, int evth_part,
                                              int inth_pri, int inth_stack, int inth_part);
static void __attribute__((noinline)) sub_195c(int matching_id, int event, uint8_t *mac, int optlen, void *optdata);
static int __attribute__((noinline)) sub_1be8(SceNetAdhocDownloadClientPeer *peer, const uint8_t *mac,
                                              uint32_t optlen, const void *optdata);
static SceNetAdhocDownloadClientPeer *__attribute__((noinline)) sub_1cdc(const uint8_t *mac);
static void __attribute__((noinline)) sub_1d38(SceNetAdhocDownloadClientPeer *peer);
static void __attribute__((noinline)) sub_1d7c(SceNetAdhocDownloadClientPeer *peer);
static void *__attribute__((noinline)) sub_23b4(int tag, const uint8_t *buf, uint32_t len, uint32_t *out_len);
static int __attribute__((noinline)) sub_2480(uint32_t pool_size, int part);
static void __attribute__((noinline)) sub_24bc(void);
static void *__attribute__((noinline)) sub_24f4(uint32_t size);
static void __attribute__((noinline)) sub_2578(void *ptr);
static uint8_t *__attribute__((noinline)) sub_25a8(int tag, const void *data, uint32_t len, uint8_t *dst);
static void __attribute__((noinline)) sub_261c(SceNetAdhocDownloadQueue *q, SceNetAdhocDownloadQueueItem *item);
static SceNetAdhocDownloadQueueItem *__attribute__((noinline)) sub_2638(SceNetAdhocDownloadQueue *q);
static SceNetAdhocDownloadQueueItem *__attribute__((noinline)) sub_2660(SceNetAdhocDownloadQueue *q, const uint8_t *mac);
static void __attribute__((noinline)) sub_26b8(SceNetAdhocDownloadQueue *q, SceNetAdhocDownloadQueueItem *item);
static void __attribute__((noinline)) sub_2b28(int matching_id, int event, uint8_t *mac, int optlen, void *optdata);
static SceNetAdhocDistributeWorker *__attribute__((noinline)) sub_2cf8(const uint8_t *mac);
static int __attribute__((noinline)) sub_2da0(uint32_t arglen, void *argp);
static void __attribute__((noinline)) sub_305c(SceNetAdhocDistributeWorker *worker);
static int __attribute__((noinline)) sub_30d4(int worker_idx);
static int __attribute__((noinline)) sub_378c(int max_workers, int worker_pri, int worker_stack, int worker_part,
                                              uint32_t data_len, void *data_ptr, void *extra_ptr,
                                              int total_send_len, void *cb_arg,
                                              int (*send_cb)(int, void *, int, void **, int *),
                                              int max_concurrent);
static int __attribute__((noinline)) sub_3bf0(int evth_pri, int evth_stack, int evth_part,
                                              int inth_pri, int inth_stack, int inth_part);

/* 0x0000 */
int sceNetAdhocDownload_13DAB550(uint32_t data_len, void *data_ptr, void *extra_ptr,
                                 void (*req_cb)(void *, uint8_t *, char *),
                                 SceNetAdhocDownloadServerInfo *info)
{
    char nickname[0x80];

    if (_sceKernelCheckThreadStack() < 0xf40) {
        return 0x80410005;
    }
    if (!g_server_inited) {
        return 0x80410c01;
    }
    if (data_len == 0 || data_ptr == NULL) {
        return 0x80410c03;
    }
    if (g_server_ctx != NULL) {
        return 0x80410c04;
    }

    int res = sceUtilityGetSystemParamString(1, nickname, 0x80);
    if (res < 0) {
        return res;
    }

    sceKernelMemset(&g_server_ctx_buf, 0, sizeof(g_server_ctx_buf));
    g_server_ctx_buf.evf_id = -1;
    g_server_ctx_buf.state = 0;
    g_server_ctx_buf.opt_len = data_len + 0x86;
    g_server_ctx = &g_server_ctx_buf;
    if (extra_ptr != NULL) {
        g_server_ctx_buf.opt_len = data_len + 0x91;
    }

    g_server_ctx->opt_buf = sub_24f4(g_server_ctx->opt_len);
    if (g_server_ctx->opt_buf == NULL) {
        g_server_ctx = NULL;
        return 0x80410c06;
    }

    uint8_t *p = sub_25a8(1, nickname, 0x80, (uint8_t *)g_server_ctx->opt_buf);
    p = sub_25a8(2, data_ptr, data_len, p);
    if (extra_ptr != NULL) {
        sub_25a8(3, extra_ptr, 8, p);
    }

    g_server_ctx->req_cb = req_cb;
    g_server_ctx->info = info;
    if (info != NULL && info->size >= 0x10) {
        info->unk_04 = 0x20000;
        info->sent = 0;
        info->total = 0;
    }

    sceKernelMemset(g_server_ctx->peer_mac, 0, 6);
    sceKernelMemset(g_server_ctx->peer_nickname, 0, 0x80);

    int mid = sceNetAdhocMatchingCreate(1, 2, 0x8001, 0x800, 1000000, 30000000, 3, 1000000, sub_0928);
    if (mid < 0) {
        sub_2578(g_server_ctx->opt_buf);
        g_server_ctx = NULL;
        return mid;
    }

    g_server_ctx->matching_id = mid;
    return 0;
}

/* 0x0238 */
int sceNetAdhocDownload_F8FC359E(uint8_t *mac, int reply_mode)
{
    uint32_t out_bits;

    if (_sceKernelCheckThreadStack() < 0xfd0) {
        return 0x80410005;
    }
    if (!g_server_inited) {
        return 0x80410c01;
    }
    if (g_server_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_server_ctx->state != 2) {
        return 0x80410c07;
    }

    if (reply_mode == 1) {
        int res = sceNetAdhocMatchingSetHelloOpt(g_server_ctx->matching_id, 0, NULL);
        if (res < 0) {
            return res;
        }
        int evf = sceKernelCreateEventFlag("SceNetAdhocDownloadReplySession", 0, 0, NULL);
        if (evf < 0) {
            return evf;
        }
        g_server_ctx->evf_id = evf;
        g_server_ctx->state = 3;
        res = sceNetAdhocMatchingSelectTarget(g_server_ctx->matching_id, mac, 0, NULL);
        if (res < 0) {
            g_server_ctx->state = 5;
            if ((uint32_t)(res + 0x7fbef7f4U) < 2U) {
                res = 0x80410c08;
            }
            sceKernelDeleteEventFlag(g_server_ctx->evf_id);
            g_server_ctx->evf_id = -1;
            return res;
        }
        sceKernelWaitEventFlag(g_server_ctx->evf_id, 3, 0x11, &out_bits, NULL);
        sceKernelDeleteEventFlag(g_server_ctx->evf_id);
        g_server_ctx->evf_id = -1;
        if ((out_bits & 2) != 0) {
            return 0x80410c0a;
        }
        if (g_server_ctx->state == 4) {
            return res;
        }
        return 0x80410c08;
    } else if (reply_mode < 2) {
        if (reply_mode != 0) {
            return 0x80410c03;
        }
        int res = sceNetAdhocMatchingCancelTarget(g_server_ctx->matching_id, mac);
        if (res == (int)0x8041080c) {
            res = 0;
        }
        int intr = sceKernelCpuSuspendIntr();
        SceNetAdhocDownloadQueueItem *item = sub_2638(&g_server_ctx->queue);
        if (item != NULL) {
            sceKernelMemcpy(g_server_ctx->peer_mac, item->mac, 6);
            sceKernelMemcpy(g_server_ctx->peer_nickname, item->nickname, 0x80);
            sub_2578(item);
        } else {
            sceKernelMemset(g_server_ctx->peer_mac, 0, 6);
            sceKernelMemset(g_server_ctx->peer_nickname, 0, 0x80);
            g_server_ctx->state = 1;
        }
        sceKernelCpuResumeIntr(intr);
        if (g_server_ctx->state == 2) {
            g_server_ctx->req_cb(g_server_ctx->info, g_server_ctx->peer_mac, g_server_ctx->peer_nickname);
        }
        return res;
    } else if (reply_mode == 2) {
        int res = sceNetAdhocMatchingSetHelloOpt(g_server_ctx->matching_id, 0, NULL);
        if (res >= 0) {
            g_server_ctx->state = 5;
        }
        return res;
    }

    return 0x80410c03;
}

/* 0x0510 */
int sceNetAdhocDownload_D0189004(int total_len, void *cb_arg,
                                 int (*send_cb)(void *, void **, int *))
{
    uint8_t local_mac[16];
    uint32_t out_bits;
    uint32_t timeout;
    uint32_t net_len;
    int chunk_len;
    void *chunk_ptr;
    int chunk_total;

    if (_sceKernelCheckThreadStack() < 0xfa0) {
        return 0x80410005;
    }
    if (!g_server_inited) {
        return 0x80410c01;
    }
    if (g_server_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_server_ctx->state != 4) {
        return 0x80410c07;
    }

    int res = sceNetGetLocalEtherAddr(local_mac);
    int ptp_id = -1;
    if (res < 0) {
        return res;
    }

    int evf = sceKernelCreateEventFlag("SceNetAdhocDownloadSend", 0, 0, NULL);
    if (evf < 0) {
        return evf;
    }

    g_server_ctx->evf_id = evf;
    if (g_server_ctx->info != NULL && g_server_ctx->info->size >= 0x10) {
        g_server_ctx->info->total = total_len;
    }

    int retries = 20;
    for (;;) {
        res = sceKernelPollEventFlag(g_server_ctx->evf_id, 2, 0x11, &out_bits);
        if (res != (int)0x800201af) {
            res = 0x80410c0a;
            goto cleanup;
        }

        ptp_id = sceNetAdhocPtpOpen(local_mac, 0x8001, g_server_ctx->peer_mac,
                                    0x8001, 0x2d20, 200000, 50, 0);
        if (ptp_id < 0) {
            res = ptp_id;
            goto cleanup;
        }
        g_server_ctx->ptp_id = ptp_id;

        res = sceNetAdhocPtpConnect(ptp_id, 10000000, 0);
        if (res >= 0) {
            break;
        }
        if (res != (int)0x80410718 || retries <= 0) {
            goto cleanup;
        }
        retries--;
        g_server_ctx->ptp_id = 0;
        sceNetAdhocPtpClose(ptp_id, 0);
        ptp_id = -1;
        timeout = 500000;
        sceKernelWaitEventFlag(g_server_ctx->evf_id, 2, 1, &out_bits, &timeout);
    }

    net_len = __builtin_bswap32((uint32_t)total_len);
    chunk_len = 4;
    res = sceNetAdhocPtpSend(ptp_id, &net_len, &chunk_len, 0, 0);
    if (res < 0) {
        goto cleanup;
    }
    res = sceNetAdhocPtpFlush(ptp_id, 0, 0);
    if (res < 0) {
        goto cleanup;
    }

    int rem = total_len;
    while (rem > 0) {
        if (send_cb(cb_arg, &chunk_ptr, &chunk_total) < 0) {
            res = 0x80410c0b;
            goto cleanup;
        }
        int offset = 0;
        while (offset < chunk_total) {
            int rem_chunk = chunk_total - offset;
            chunk_len = (rem_chunk < 0x5a5) ? rem_chunk : 0x5a4;
            res = sceNetAdhocPtpSend(ptp_id, (uint8_t *)chunk_ptr + offset, &chunk_len, 0, 0);
            offset += 0x5a4;
            if (res < 0) {
                goto cleanup;
            }
            if (g_server_ctx->info != NULL && g_server_ctx->info->size >= 0x10) {
                g_server_ctx->info->sent += chunk_len;
            }
        }
        rem -= chunk_total;
    }

    res = sceNetAdhocPtpFlush(ptp_id, 0, 0);

cleanup:
    g_server_ctx->state = 5;
    if (res == (int)0x80410708) {
        res = 0x80410c0a;
    }
    g_server_ctx->ptp_id = 0;
    if (ptp_id >= 0) {
        sceNetAdhocPtpClose(ptp_id, 0);
    }
    sceKernelDeleteEventFlag(g_server_ctx->evf_id);
    g_server_ctx->evf_id = -1;
    return res;
}

/* 0x0880 */
int __attribute__((optimize("no-crossjumping", "no-reorder-blocks")))
sceNetAdhocDownload_A70FDFBE(void)
{
    int res;

    if (_sceKernelCheckThreadStack() < 0xff0) {
        res = 0x80410005;
    } else if (!g_server_inited) {
        res = 0x80410c01;
    } else if (g_server_ctx == NULL) {
        res = 0x80410c05;
    } else if (g_server_ctx->state != 4) {
        res = 0x80410c07;
    } else {
        if (g_server_ctx->ptp_id > 0) {
            sceNetAdhoc_73BFD52D(g_server_ctx->ptp_id, 0x3f0);
        }
        if (g_server_ctx->evf_id >= 0) {
            sceKernelSetEventFlag(g_server_ctx->evf_id, 2);
        }
        res = 0;
    }
    return res;
}

/* 0x0928 */
static void __attribute__((noinline)) sub_0928(int matching_id, int event, uint8_t *mac, int optlen, void *optdata)
{
    uint32_t nick_len;

    if (g_server_ctx->matching_id != matching_id) {
        return;
    }

    switch (event) {
    case 2: {
        void *nick = sub_23b4(1, (const uint8_t *)optdata, optlen, &nick_len);
        if (nick == NULL || nick_len != 0x80) {
            return;
        }
        if (g_server_ctx->state == 1) {
            g_server_ctx->state = 2;
            sceKernelMemcpy(g_server_ctx->peer_mac, mac, 6);
            sceKernelMemcpy(g_server_ctx->peer_nickname, nick, 0x80);
            g_server_ctx->req_cb(g_server_ctx->info, g_server_ctx->peer_mac, g_server_ctx->peer_nickname);
            return;
        }
        int intr = sceKernelCpuSuspendIntr();
        if (g_server_ctx->state == 2 && sceNet_lib_E0A81C7C(g_server_ctx->peer_mac, mac, 6) != 0) {
            SceNetAdhocDownloadQueueItem *item = (SceNetAdhocDownloadQueueItem *)sub_24f4(sizeof(SceNetAdhocDownloadQueueItem));
            if (item != NULL) {
                sceKernelMemcpy(item->mac, mac, 6);
                sceKernelMemcpy(item->nickname, nick, 0x80);
                sub_261c(&g_server_ctx->queue, item);
            }
        }
        sceKernelCpuResumeIntr(intr);
        break;
    }
    case 7: {
        int intr = sceKernelCpuSuspendIntr();
        if (g_server_ctx->state == 3) {
            g_server_ctx->state = 4;
            sceKernelSetEventFlag(g_server_ctx->evf_id, 1);
        }
        sceKernelCpuResumeIntr(intr);
        break;
    }
    case 5:
    case 8:
    case 9:
    case 10: {
        int intr = sceKernelCpuSuspendIntr();
        if (g_server_ctx->state == 3 && sceNet_lib_E0A81C7C(g_server_ctx->peer_mac, mac, 6) == 0) {
            g_server_ctx->state = 5;
            sceKernelSetEventFlag(g_server_ctx->evf_id, 1);
        } else if (g_server_ctx->state == 2) {
            SceNetAdhocDownloadQueueItem *item = sub_2660(&g_server_ctx->queue, mac);
            if (item != NULL) {
                sub_26b8(&g_server_ctx->queue, item);
                sub_2578(item);
            }
        }
        sceKernelCpuResumeIntr(intr);
        break;
    }
    default:
        break;
    }
}

/* 0x0b84 */
int sceNetAdhocDownload_A21FEF45(uint32_t pool_size)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (g_server_inited) {
        return 0x80410c02;
    }
    int res = sub_2480(pool_size, 2);
    if (res < 0) {
        return res;
    }
    g_server_inited = 1;
    return 0;
}

/* 0x0c04 */
int sceNetAdhocDownload_04077014(uint32_t pool_size, int part)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (g_server_inited) {
        return 0x80410c02;
    }
    if (part != 2 && part != 5) {
        return 0x80410c03;
    }
    int res = sub_2480(pool_size, part);
    if (res < 0) {
        return res;
    }
    g_server_inited = 1;
    return 0;
}

/* 0x0cb0 */
int sceNetAdhocDownload_2B6FB0DA(int evth_pri, int evth_stack, int inth_pri, int inth_stack)
{
    if (_sceKernelCheckThreadStack() < 0xfd0) {
        return 0x80410005;
    }
    return sub_1028(evth_pri, evth_stack, 2, inth_pri, inth_stack, 2);
}

/* 0x0d30 */
int sceNetAdhocDownload_7CAC6934(int evth_pri, int evth_stack, int evth_part,
                                 int inth_pri, int inth_stack, int inth_part)
{
    if (_sceKernelCheckThreadStack() < 0xfd0) {
        return 0x80410005;
    }
    return sub_1028(evth_pri, evth_stack, evth_part, inth_pri, inth_stack, inth_part);
}

/* 0x0dc8 */
int sceNetAdhocDownload_C421875C(void)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_server_inited) {
        return 0x80410c01;
    }
    if (g_server_ctx == NULL) {
        return 0x80410c05;
    }
    int intr = sceKernelCpuSuspendIntr();
    if (g_server_ctx->state != 3) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410c07;
    }
    g_server_ctx->state = 5;
    sceKernelSetEventFlag(g_server_ctx->evf_id, 2);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x0e80 */
int sceNetAdhocDownload_F76147B1(void)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_server_inited) {
        return 0x80410c01;
    }
    if (g_server_ctx == NULL) {
        return 0x80410c05;
    }
    sceNetAdhocMatchingStop(g_server_ctx->matching_id);
    g_server_ctx->state = 0;
    sceKernelMemset(g_server_ctx->peer_mac, 0, 6);
    sceKernelMemset(g_server_ctx->peer_nickname, 0, 0x80);
    SceNetAdhocDownloadQueueItem *item;
    while ((item = sub_2638(&g_server_ctx->queue)) != NULL) {
        sub_2578(item);
    }
    return 0;
}

/* 0x0f38 */
int sceNetAdhocDownload_7A483F9E(void)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_server_inited) {
        return 0x80410c01;
    }
    if (g_server_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_server_ctx->state != 0) {
        return 0x80410c07;
    }
    sceNetAdhocMatchingDelete(g_server_ctx->matching_id);
    sub_2578(g_server_ctx->opt_buf);
    g_server_ctx = NULL;
    return 0;
}

/* 0x0fc0 */
int sceNetAdhocDownload_117CA01A(void)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_server_inited) {
        return 0;
    }
    if (g_server_ctx != NULL) {
        return 0x80410c04;
    }
    sub_24bc();
    g_server_inited = 0;
    return 0;
}

/* 0x1028 */
static int __attribute__((noinline)) sub_1028(int evth_pri, int evth_stack, int evth_part,
                                              int inth_pri, int inth_stack, int inth_part)
{
    if (!g_server_inited) {
        return 0x80410c01;
    }
    if (g_server_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_server_ctx->state != 0) {
        return 0x80410c07;
    }
    g_server_ctx->state = 1;
    int res = sceNetAdhocMatching_E8454C65(g_server_ctx->matching_id, evth_pri, evth_stack, evth_part,
                                           inth_pri, inth_stack, inth_part,
                                           g_server_ctx->opt_len, g_server_ctx->opt_buf);
    if (res < 0) {
        g_server_ctx->state = 0;
        return res;
    }
    if (g_server_ctx->info != NULL && g_server_ctx->info->size >= 0x10) {
        g_server_ctx->info->sent = 0;
        g_server_ctx->info->total = 0;
    }
    return 0;
}

/* 0x1100 */
int sceNetAdhocDownload_57A51DD0(void (*hello_cb)(void *, uint8_t *, char *, uint32_t, void *, void *),
                                 void *cb_arg)
{
    char nickname[0x80];

    if (_sceKernelCheckThreadStack() < 0xf50) {
        return 0x80410005;
    }
    if (!g_client_inited) {
        return 0x80410c01;
    }
    if (g_client_ctx != NULL) {
        return 0x80410c04;
    }

    int res = sceUtilityGetSystemParamString(1, nickname, 0x80);
    if (res < 0) {
        return res;
    }

    sceKernelMemset(&g_client_ctx_buf, 0, sizeof(g_client_ctx_buf));
    g_client_ctx_buf.opt_len = 0x83;
    g_client_ctx_buf.state = 0;
    g_client_ctx = &g_client_ctx_buf;
    g_client_ctx_buf.opt_buf = sub_24f4(0x83);
    if (g_client_ctx->opt_buf == NULL) {
        g_client_ctx = NULL;
        return 0x80410c06;
    }

    sub_25a8(1, nickname, 0x80, (uint8_t *)g_client_ctx->opt_buf);
    g_client_ctx->hello_cb = hello_cb;
    g_client_ctx->cb_arg = cb_arg;
    sceKernelMemset(g_client_ctx->target_mac, 0, 6);

    int mid = sceNetAdhocMatchingCreate(2, 2, 0x8001, 0x800, 1000000, 30000000, 3, 1000000, sub_195c);
    if (mid < 0) {
        sub_2578(g_client_ctx->opt_buf);
        g_client_ctx = NULL;
        return mid;
    }

    g_client_ctx->matching_id = mid;
    return 0;
}

/* 0x1284 */
int sceNetAdhocDownload_22C2BCC6(uint32_t *buflen, uint8_t *buf)
{
    if (_sceKernelCheckThreadStack() < 0xfd0) {
        return 0x80410005;
    }
    if (!g_client_inited) {
        return 0x80410c01;
    }
    if (g_client_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_client_ctx->state != 1) {
        return 0x80410c07;
    }

    sceKernelLockLwMutex(g_client_lwmutex, 1, NULL);
    uint32_t total = 0;
    uint8_t *prev = NULL;
    SceNetAdhocDownloadClientPeer *peer = g_client_peer_list;
    while (peer != NULL) {
        uint32_t aligned_dlen = (peer->data_len + 3) & ~3U;
        uint32_t item_size = (peer->extra_ptr != NULL) ? (aligned_dlen + 0xa0) : (aligned_dlen + 0x98);
        uint8_t *cur = buf + total;
        SceNetAdhocDownloadClientPeer *next_peer = peer->next;
        if (buf == NULL) {
            total += item_size;
            peer = next_peer;
            continue;
        }
        if (*buflen < total + item_size) {
            break;
        }
        sceKernelMemcpy(cur + 0x90, peer->mac, 6);
        sceKernelMemcpy(cur + 0x0c, peer->nickname, 0x80);
        *(void **)(cur + 0x08) = cur + 0x98;
        *(uint32_t *)(cur + 0x04) = peer->data_len;
        sceKernelMemcpy(cur + 0x98, peer->data_ptr, peer->data_len);
        if (peer->extra_ptr != NULL) {
            void *ext_dst = cur + ((peer->data_len + 3) & ~3U) + 0x98;
            *(void **)(cur + 0x8c) = ext_dst;
            sceKernelMemcpy(ext_dst, peer->extra_ptr, 8);
        } else {
            *(void **)(cur + 0x8c) = NULL;
        }
        *(void **)(cur + 0x00) = NULL;
        if (prev != NULL) {
            *(void **)prev = cur;
        }
        prev = cur;
        g_client_peer_list = peer->next;
        sub_2578(peer->raw_buf);
        sub_2578(peer);
        total += item_size;
        peer = next_peer;
    }
    sceKernelUnlockLwMutex(g_client_lwmutex, 1);
    *buflen = total;
    return 0;
}

/* 0x1470 */
int sceNetAdhocDownload_4E6029F1(const uint8_t *mac)
{
    uint32_t out_bits;

    if (_sceKernelCheckThreadStack() < 0xfe0) {
        return 0x80410005;
    }
    if (!g_client_inited) {
        return 0x80410c01;
    }
    if (g_client_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_client_ctx->state != 1) {
        return 0x80410c07;
    }

    int evf = sceKernelCreateEventFlag("SceNetAdhocDownloadRequestSession", 0, 0, NULL);
    if (evf < 0) {
        return evf;
    }
    g_client_ctx->evf_id = evf;
    sceKernelMemcpy(g_client_ctx->target_mac, mac, 6);
    g_client_ctx->state = 2;

    int res = sceNetAdhocMatchingSelectTarget(g_client_ctx->matching_id, mac,
                                              g_client_ctx->opt_len, g_client_ctx->opt_buf);
    if (res < 0) {
        if ((uint32_t)(res + 0x7fbef7f4U) < 2U) {
            res = 0x80410c09;
        }
        g_client_ctx->state = 4;
        sceKernelDeleteEventFlag(g_client_ctx->evf_id);
        return res;
    }

    sceKernelWaitEventFlag(g_client_ctx->evf_id, 3, 0x11, &out_bits, NULL);
    sceKernelDeleteEventFlag(g_client_ctx->evf_id);
    if ((out_bits & 2) != 0) {
        return 0x80410c0a;
    }
    return (g_client_ctx->state == 3) ? 0 : (int)0x80410c09;
}

/* 0x15e4 */
int sceNetAdhocDownload_8846D2B0(void *buf, int bufsize, void *cb_arg,
                                 int (*recv_cb)(void *, uint32_t, void *, int))
{
    uint8_t peer_mac[16];
    uint16_t peer_port;
    uint32_t total_len;
    int hdr_len;
    int chunk_len;

    if (_sceKernelCheckThreadStack() < 0xfb0) {
        return 0x80410005;
    }
    if (!g_client_inited) {
        return 0x80410c01;
    }
    if (g_client_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_client_ctx->state != 3) {
        return 0x80410c07;
    }

    int res = sceNetGetLocalEtherAddr(peer_mac);
    if (res < 0) {
        return res;
    }

    int listen_id = sceNetAdhocPtpListen(peer_mac, 0x8001, 0xb480, 200000, 50, 1, 0);
    int ptp_id = listen_id;
    res = listen_id;
    if (listen_id < 0) {
        goto cleanup;
    }
    g_client_ctx->ptp_id = listen_id;

    for (;;) {
        int acc_id = sceNetAdhocPtpAccept(listen_id, peer_mac, &peer_port, 10000000, 0);
        res = acc_id;
        if (acc_id < 0) {
            goto cleanup;
        }
        if (sceNet_lib_E0A81C7C(peer_mac, g_client_ctx->target_mac, 6) == 0 && peer_port == 0x8001) {
            g_client_ctx->ptp_id = acc_id;
            sceNetAdhocPtpClose(listen_id, 0);
            ptp_id = acc_id;
            break;
        }
        sceNetAdhocPtpClose(acc_id, 0);
    }

    if (g_client_abort_flag > 0) {
        res = 0x80410c0a;
        goto cleanup;
    }

    hdr_len = 4;
    res = sceNetAdhocPtpRecv(ptp_id, &total_len, &hdr_len, 10000000, 0);
    if (res < 0) {
        goto cleanup;
    }
    total_len = __builtin_bswap32(total_len);
    if (total_len == 0) {
        goto cleanup;
    }

    uint32_t recvd = 0;
    do {
        chunk_len = bufsize;
        res = sceNetAdhocPtpRecv(ptp_id, buf, &chunk_len, 10000000, 0);
        if (res < 0) {
            goto cleanup;
        }
        if (recv_cb(cb_arg, total_len, buf, chunk_len) < 0) {
            res = 0x80410c0c;
            goto cleanup;
        }
        recvd += (uint32_t)chunk_len;
    } while (recvd < total_len);

cleanup:
    if (res == (int)0x80410708) {
        res = 0x80410c0a;
    }
    g_client_ctx->state = 4;
    g_client_ctx->ptp_id = 0;
    if (ptp_id >= 0) {
        sceNetAdhocPtpClose(ptp_id, 0);
    }
    g_client_abort_flag = 0;
    return res;
}

/* 0x1874 */
int __attribute__((optimize("no-crossjumping", "no-reorder-blocks")))
sceNetAdhocDownload_469F6B83(void)
{
    int res;

    if (_sceKernelCheckThreadStack() < 0xfe0) {
        res = 0x80410005;
    } else if (!g_client_inited) {
        res = 0x80410c01;
    } else if (g_client_ctx == NULL) {
        res = 0x80410c05;
    } else {
        sceNetAdhocMatchingStop(g_client_ctx->matching_id);
        g_client_ctx->state = 0;
        sceKernelMemset(g_client_ctx->target_mac, 0, 6);

        sceKernelLockLwMutex(g_client_lwmutex, 1, NULL);
        SceNetAdhocDownloadClientPeer *peer = g_client_peer_list;
        while (peer != NULL) {
            SceNetAdhocDownloadClientPeer *next = peer->next;
            sub_2578(peer->raw_buf);
            sub_2578(peer);
            peer = next;
        }
        g_client_peer_list = NULL;
        sceKernelUnlockLwMutex(g_client_lwmutex, 1);
        res = 0;
    }
    return res;
}

/* 0x195c */
static void __attribute__((noinline)) sub_195c(int matching_id, int event, uint8_t *mac, int optlen, void *optdata)
{
    if (g_client_ctx->matching_id != matching_id) {
        return;
    }

    switch (event) {
    case 1: {
        if (g_client_ctx->state != 1) {
            return;
        }
        sceKernelLockLwMutex(g_client_lwmutex, 1, NULL);
        SceNetAdhocDownloadClientPeer *peer = sub_1cdc(mac);
        if (peer != NULL) {
            if (peer->raw_len == (uint32_t)optlen &&
                sceNet_lib_E0A81C7C(peer->raw_buf, optdata, optlen) == 0) {
                sceKernelUnlockLwMutex(g_client_lwmutex, 1);
                return;
            }
            sub_1d7c(peer);
            sub_2578(peer->raw_buf);
            if (optlen == 0) {
                sceKernelUnlockLwMutex(g_client_lwmutex, 1);
                return;
            }
        } else {
            if (optlen == 0) {
                sceKernelUnlockLwMutex(g_client_lwmutex, 1);
                return;
            }
            peer = (SceNetAdhocDownloadClientPeer *)sub_24f4(sizeof(SceNetAdhocDownloadClientPeer));
            if (peer == NULL) {
                sceKernelUnlockLwMutex(g_client_lwmutex, 1);
                return;
            }
        }
        sceKernelMemset(peer, 0, sizeof(SceNetAdhocDownloadClientPeer));
        if (sub_1be8(peer, mac, optlen, optdata) < 0) {
            sub_2578(peer);
        } else {
            sub_1d38(peer);
            g_client_ctx->hello_cb(g_client_ctx->cb_arg, mac, peer->nickname,
                                   peer->data_len, peer->data_ptr, peer->extra_ptr);
        }
        sceKernelUnlockLwMutex(g_client_lwmutex, 1);
        break;
    }
    case 6: {
        int intr = sceKernelCpuSuspendIntr();
        if (g_client_ctx->state == 2) {
            g_client_ctx->state = 3;
            sceKernelSetEventFlag(g_client_ctx->evf_id, 1);
        }
        sceKernelCpuResumeIntr(intr);
        break;
    }
    case 8:
    case 10: {
        if (g_client_ctx->state == 1) {
            sceKernelLockLwMutex(g_client_lwmutex, 1, NULL);
            SceNetAdhocDownloadClientPeer *peer = sub_1cdc(mac);
            if (peer != NULL) {
                sub_1d7c(peer);
                sub_2578(peer->raw_buf);
                sub_2578(peer);
            }
            sceKernelUnlockLwMutex(g_client_lwmutex, 1);
            return;
        }
        /* fallthrough */
    }
    case 4:
    case 9: {
        int intr = sceKernelCpuSuspendIntr();
        if (g_client_ctx->state == 2 &&
            sceNet_lib_E0A81C7C(g_client_ctx->target_mac, mac, 6) == 0) {
            g_client_ctx->state = 4;
            sceKernelSetEventFlag(g_client_ctx->evf_id, 1);
        }
        sceKernelCpuResumeIntr(intr);
        break;
    }
    default:
        break;
    }
}

/* 0x1be8 */
static int __attribute__((noinline)) sub_1be8(SceNetAdhocDownloadClientPeer *peer, const uint8_t *mac,
                                              uint32_t optlen, const void *optdata)
{
    uint32_t tag_len;

    void *buf = sub_24f4(optlen);
    peer->raw_buf = (uint8_t *)buf;
    if (buf == NULL) {
        return -1;
    }
    peer->raw_len = optlen;
    sceKernelMemcpy(buf, optdata, optlen);
    sceKernelMemcpy(peer->mac, mac, 6);

    void *nick = sub_23b4(1, peer->raw_buf, peer->raw_len, &tag_len);
    if (nick != NULL) {
        sceKernelMemcpy(peer->nickname, nick, 0x80);
    }

    void *dptr = sub_23b4(2, peer->raw_buf, peer->raw_len, &tag_len);
    peer->data_ptr = dptr;
    if (dptr != NULL) {
        peer->data_len = tag_len;
    } else {
        peer->data_len = 0;
    }

    peer->extra_ptr = sub_23b4(3, peer->raw_buf, peer->raw_len, &tag_len);
    return 0;
}

/* 0x1cdc */
static SceNetAdhocDownloadClientPeer *__attribute__((noinline)) sub_1cdc(const uint8_t *mac)
{
    SceNetAdhocDownloadClientPeer *p = g_client_peer_list;
    while (p != NULL) {
        if (sceNet_lib_E0A81C7C(p->mac, mac, 6) == 0) {
            return p;
        }
        p = p->next;
    }
    return NULL;
}

/* 0x1d38 */
static void __attribute__((noinline)) sub_1d38(SceNetAdhocDownloadClientPeer *peer)
{
    if (g_client_peer_list == NULL) {
        g_client_peer_list = peer;
    } else {
        SceNetAdhocDownloadClientPeer *cur = g_client_peer_list;
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = peer;
    }
    peer->next = NULL;
}

/* 0x1d7c */
static void __attribute__((noinline)) sub_1d7c(SceNetAdhocDownloadClientPeer *peer)
{
    if (g_client_peer_list == peer) {
        g_client_peer_list = peer->next;
    } else {
        SceNetAdhocDownloadClientPeer *prev = g_client_peer_list;
        while (prev != NULL && prev->next != peer) {
            prev = prev->next;
        }
        if (prev != NULL) {
            prev->next = peer->next;
        }
    }
    peer->next = NULL;
}

/* 0x1dd8 */
int sceNetAdhocDownload_3082F4E2(uint32_t pool_size)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (g_client_inited) {
        return 0x80410c02;
    }
    int res = sub_2480(pool_size, 2);
    if (res < 0) {
        return res;
    }
    res = sceKernelCreateLwMutex(g_client_lwmutex, "SceNetAdhocDownloadClient", 0, 0, NULL);
    if (res < 0) {
        sub_24bc();
        return res;
    }
    g_client_inited = 1;
    g_client_abort_flag = 0;
    return 0;
}

/* 0x1ea0 */
int sceNetAdhocDownload_C8402ACD(uint32_t pool_size, int part)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (g_client_inited) {
        return 0x80410c02;
    }
    if (part != 2 && part != 5) {
        return 0x80410c03;
    }
    int res = sub_2480(pool_size, part);
    if (res < 0) {
        return res;
    }
    res = sceKernelCreateLwMutex(g_client_lwmutex, "SceNetAdhocDownloadClient", 0, 0, NULL);
    if (res < 0) {
        sub_24bc();
        return res;
    }
    g_client_inited = 1;
    g_client_abort_flag = 0;
    return 0;
}

/* 0x1f90 */
int sceNetAdhocDownload_314ED31E(int evth_pri, int evth_stack, int inth_pri, int inth_stack)
{
    if (_sceKernelCheckThreadStack() < 0xfd0) {
        return 0x80410005;
    }
    if (!g_client_inited) {
        return 0x80410c01;
    }
    if (g_client_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_client_ctx->state != 0) {
        return 0x80410c07;
    }
    g_client_ctx->state = 1;
    int res = sceNetAdhocMatching_E8454C65(g_client_ctx->matching_id, evth_pri, evth_stack, 2,
                                           inth_pri, inth_stack, 2, 0, NULL);
    if (res < 0) {
        g_client_ctx->state = 0;
        return res;
    }
    return 0;
}

/* 0x2078 */
int sceNetAdhocDownload_24FD9B7A(int evth_pri, int evth_stack, int evth_part,
                                 int inth_pri, int inth_stack, int inth_part)
{
    if (_sceKernelCheckThreadStack() < 0xfd0) {
        return 0x80410005;
    }
    if (!g_client_inited) {
        return 0x80410c01;
    }
    if (g_client_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_client_ctx->state != 0) {
        return 0x80410c07;
    }
    g_client_ctx->state = 1;
    int res = sceNetAdhocMatching_E8454C65(g_client_ctx->matching_id, evth_pri, evth_stack, evth_part,
                                           inth_pri, inth_stack, inth_part, 0, NULL);
    if (res < 0) {
        g_client_ctx->state = 0;
        return res;
    }
    return 0;
}

/* 0x2178 */
int sceNetAdhocDownload_8A5500E0(void)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_client_inited) {
        return 0x80410c01;
    }
    if (g_client_ctx == NULL) {
        return 0x80410c05;
    }
    int intr = sceKernelCpuSuspendIntr();
    if (g_client_ctx->state != 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410c07;
    }
    g_client_ctx->state = 4;
    sceKernelSetEventFlag(g_client_ctx->evf_id, 2);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x2230 */
int sceNetAdhocDownload_1AD5CC88(void)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_client_inited) {
        return 0x80410c01;
    }
    if (g_client_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_client_ctx->state != 3) {
        return 0x80410c07;
    }
    if (g_client_ctx->ptp_id > 0) {
        g_client_abort_flag = 1;
        sceNetAdhoc_73BFD52D(g_client_ctx->ptp_id, 0x3f0);
    }
    return 0;
}

/* 0x22c0 */
int sceNetAdhocDownload_378D4311(void)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_client_inited) {
        return 0x80410c01;
    }
    if (g_client_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_client_ctx->state != 0) {
        return 0x80410c07;
    }
    sceNetAdhocMatchingDelete(g_client_ctx->matching_id);
    g_client_ctx = NULL;
    return 0;
}

/* 0x233c */
int sceNetAdhocDownload_BF1433F0(void)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_client_inited) {
        return 0;
    }
    if (g_client_ctx != NULL) {
        return 0x80410c04;
    }
    sceKernelDeleteLwMutex(g_client_lwmutex);
    sub_24bc();
    g_client_inited = 0;
    return 0;
}

/* 0x23b4 */
static void *__attribute__((noinline)) sub_23b4(int tag, const uint8_t *buf, uint32_t len, uint32_t *out_len)
{
    const uint8_t *end = buf + len;
    const uint8_t *p = buf;
    uint16_t raw_len;

    while (p < end) {
        uint8_t cur_tag = *p;
        if ((int)(end - (p + 1)) < 2) {
            return NULL;
        }
        sceKernelMemcpy(&raw_len, p + 1, 2);
        p += 3;
        uint16_t item_len = (uint16_t)((raw_len >> 8) | (raw_len << 8));
        if ((int)(end - p) < (int)item_len) {
            return NULL;
        }
        if (cur_tag == (tag & 0xff)) {
            *out_len = item_len;
            return (void *)p;
        }
        p += item_len;
    }
    return NULL;
}

/* 0x2480 */
static int __attribute__((noinline)) sub_2480(uint32_t pool_size, int part)
{
    int uid = sceKernelCreateVpl("SceNetAdhocDownloadMemoryPool", part, 0, pool_size, NULL);
    if (uid < 0) {
        return uid;
    }
    g_vpl_id = uid;
    return 0;
}

/* 0x24bc */
static void __attribute__((noinline)) sub_24bc(void)
{
    if (g_vpl_id > 0) {
        sceKernelDeleteVpl(g_vpl_id);
        g_vpl_id = 0;
    }
}

/* 0x24f4 */
static void *__attribute__((noinline)) sub_24f4(uint32_t size)
{
    uint32_t status[16];
    void *ptr = NULL;

    if (g_vpl_id <= 0) {
        return NULL;
    }
    if (sceKernelTryAllocateVpl(g_vpl_id, size, &ptr) < 0) {
        return NULL;
    }
    status[0] = 0x34;
    if (sceKernelReferVplStatus(g_vpl_id, status) == 0) {
        uint32_t used = status[10] - status[11];
        if (used > g_max_vpl_used) {
            g_max_vpl_used = used;
        }
    }
    return ptr;
}

/* 0x2578 */
static void __attribute__((noinline)) sub_2578(void *ptr)
{
    if (g_vpl_id > 0) {
        sceKernelFreeVpl(g_vpl_id, ptr);
    }
}

/* 0x25a8 */
static uint8_t *__attribute__((noinline)) sub_25a8(int tag, const void *data, uint32_t len, uint8_t *dst)
{
    uint16_t be_len = (uint16_t)(((len & 0xffff) >> 8) | ((len & 0xff) << 8));
    dst[0] = (uint8_t)tag;
    sceKernelMemcpy(dst + 1, &be_len, 2);
    sceKernelMemcpy(dst + 3, data, len);
    return dst + 3 + len;
}

/* 0x261c */
static void __attribute__((noinline)) sub_261c(SceNetAdhocDownloadQueue *q, SceNetAdhocDownloadQueueItem *item)
{
    item->next = NULL;
    if (q->tail != NULL) {
        q->tail->next = item;
    } else {
        q->head = item;
    }
    q->tail = item;
}

/* 0x2638 */
static SceNetAdhocDownloadQueueItem *__attribute__((noinline)) sub_2638(SceNetAdhocDownloadQueue *q)
{
    SceNetAdhocDownloadQueueItem *item = q->head;
    if (item != NULL) {
        q->head = item->next;
        if (item->next == NULL) {
            q->tail = NULL;
        }
        item->next = NULL;
    }
    return item;
}

/* 0x2660 */
static SceNetAdhocDownloadQueueItem *__attribute__((noinline)) sub_2660(SceNetAdhocDownloadQueue *q, const uint8_t *mac)
{
    SceNetAdhocDownloadQueueItem *cur = q->head;
    while (cur != NULL) {
        if (sceNet_lib_E0A81C7C(cur->mac, mac, 6) == 0) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/* 0x26b8 */
static void __attribute__((noinline)) sub_26b8(SceNetAdhocDownloadQueue *q, SceNetAdhocDownloadQueueItem *item)
{
    SceNetAdhocDownloadQueueItem *cur = q->head;
    SceNetAdhocDownloadQueueItem *prev = NULL;
    while (cur != NULL) {
        if (cur == item) {
            if (prev != NULL) {
                prev->next = item->next;
            } else {
                q->head = item->next;
            }
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    if (q->tail == item) {
        q->tail = prev;
    }
    item->next = NULL;
}

/* 0x2708 */
int sceNetAdhocDownload_4C542FD1(void)
{
    int num_wait;

    if (_sceKernelCheckThreadStack() < 0xfe0) {
        return 0x80410005;
    }
    if (!g_distribute_inited) {
        return 0x80410c01;
    }
    if (g_distribute_ctx == NULL) {
        return 0x80410c05;
    }

    g_distribute_ctx->state = 0;
    sceNetAdhocMatchingStop(g_distribute_ctx->matching_id);
    sceKernelCancelSema(g_distribute_ctx->queue_sema, 0, &num_wait);
    if (g_distribute_ctx->concurrent_sema >= 0) {
        sceKernelCancelSema(g_distribute_ctx->concurrent_sema, 0, &num_wait);
    }

    for (int i = 0; i < g_distribute_ctx->max_workers; i++) {
        SceNetAdhocDistributeWorker *w = &g_distribute_ctx->workers[i];
        sceKernelSetEventFlag(w->evf_id, 2);
        if (w->ptp_id >= 0) {
            sceNetAdhoc_73BFD52D(w->ptp_id, 0x3f0);
        }
        if (w->thid >= 0) {
            sceKernelWaitThreadEnd(w->thid, NULL);
        }
    }

    SceNetAdhocDownloadQueueItem *item;
    while ((item = sub_2638(&g_distribute_ctx->queue)) != NULL) {
        sub_2578(item);
    }
    return 0;
}

/* 0x2840 */
int sceNetAdhocDownload_A45230D9(void)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_distribute_inited) {
        return 0x80410c01;
    }
    if (g_distribute_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_distribute_ctx->state != 0) {
        return 0x80410c07;
    }

    for (int i = 0; i < g_distribute_ctx->max_workers; i++) {
        SceNetAdhocDistributeWorker *w = &g_distribute_ctx->workers[i];
        if (w->evf_id >= 0) {
            sceKernelDeleteEventFlag(w->evf_id);
        }
        if (w->thid >= 0) {
            sceKernelDeleteThread(w->thid);
        }
    }

    if (g_distribute_ctx->concurrent_sema >= 0) {
        sceKernelDeleteSema(g_distribute_ctx->concurrent_sema);
    }
    sceKernelDeleteSema(g_distribute_ctx->queue_sema);
    sceNetAdhocMatchingDelete(g_distribute_ctx->matching_id);
    sub_2578(g_distribute_ctx->opt_buf);
    sub_2578(g_distribute_ctx->workers);
    g_distribute_ctx = NULL;
    return 0;
}

/* 0x2960 */
int sceNetAdhocDownload_48F87AB1(int worker_idx)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_distribute_inited) {
        return 0x80410c01;
    }
    if (g_distribute_ctx == NULL) {
        return 0x80410c05;
    }
    if (worker_idx > g_distribute_ctx->max_workers) {
        return 0x80410c03;
    }
    if (g_distribute_ctx->state != 1) {
        return 0x80410c07;
    }

    SceNetAdhocDistributeWorker *w = &g_distribute_ctx->workers[worker_idx];
    if (w->state == 4) {
        sceKernelSetEventFlag(w->evf_id, 1);
        return 0;
    }
    return 0x80410c07;
}

/* 0x2a1c */
int __attribute__((optimize("no-crossjumping", "no-reorder-blocks")))
sceNetAdhocDownload_0A531FBE(int worker_idx, uint8_t *info_out)
{
    int res;

    if (_sceKernelCheckThreadStack() < 0xff0) {
        res = 0x80410005;
    } else if (!g_distribute_inited) {
        res = 0x80410c01;
    } else if (g_distribute_ctx == NULL) {
        res = 0x80410c05;
    } else if (worker_idx > g_distribute_ctx->max_workers || info_out == NULL) {
        res = 0x80410c03;
    } else if (g_distribute_ctx->state != 1) {
        res = 0x80410c07;
    } else {
        SceNetAdhocDistributeWorker *w = &g_distribute_ctx->workers[worker_idx];
        int intr = sceKernelCpuSuspendIntr();
        *(int *)(info_out + 0x00) = w->state;
        *(int *)(info_out + 0x04) = w->error_code;
        sceKernelMemcpy(info_out + 0x08, w->mac, 6);
        sceKernelMemcpy(info_out + 0x10, w->nickname, 0x80);
        *(uint32_t *)(info_out + 0x90) = w->total_len;
        *(uint32_t *)(info_out + 0x94) = w->sent_len;
        sceKernelCpuResumeIntr(intr);
        res = 0;
    }
    return res;
}

/* 0x2b28 */
static void __attribute__((noinline)) sub_2b28(int matching_id, int event, uint8_t *mac, int optlen, void *optdata)
{
    uint32_t nick_len;

    if (g_distribute_ctx->matching_id != matching_id) {
        return;
    }

    switch (event) {
    case 2: {
        void *nick = sub_23b4(1, (const uint8_t *)optdata, optlen, &nick_len);
        if (nick == NULL || nick_len != 0x80) {
            return;
        }
        SceNetAdhocDownloadQueueItem *item = (SceNetAdhocDownloadQueueItem *)sub_24f4(sizeof(SceNetAdhocDownloadQueueItem));
        if (item == NULL) {
            sceNetAdhocMatchingCancelTarget(g_distribute_ctx->matching_id, mac);
            return;
        }
        sceKernelMemcpy(item->mac, mac, 6);
        sceKernelMemcpy(item->nickname, nick, 0x80);
        int intr = sceKernelCpuSuspendIntr();
        sub_261c(&g_distribute_ctx->queue, item);
        sceKernelCpuResumeIntr(intr);
        sceKernelSignalSema(g_distribute_ctx->queue_sema, 1);
        break;
    }
    case 7: {
        int intr = sceKernelCpuSuspendIntr();
        SceNetAdhocDistributeWorker *w = sub_2cf8(mac);
        if (w != NULL) {
            w->state = 3;
            sceKernelSetEventFlag(w->evf_id, 1);
        }
        sceKernelCpuResumeIntr(intr);
        break;
    }
    case 5:
    case 8:
    case 9:
    case 10: {
        int intr = sceKernelCpuSuspendIntr();
        SceNetAdhocDistributeWorker *w = sub_2cf8(mac);
        if (w != NULL) {
            w->error_code = 0x80410c08;
            w->state = 4;
            sceKernelSetEventFlag(w->evf_id, 1);
        } else {
            SceNetAdhocDownloadQueueItem *item = sub_2660(&g_distribute_ctx->queue, mac);
            if (item != NULL) {
                sub_26b8(&g_distribute_ctx->queue, item);
                sub_2578(item);
            }
        }
        sceKernelCpuResumeIntr(intr);
        break;
    }
    default:
        break;
    }
}

/* 0x2cf8 */
static SceNetAdhocDistributeWorker *__attribute__((noinline)) sub_2cf8(const uint8_t *mac)
{
    for (int i = 0; i < g_distribute_ctx->max_workers; i++) {
        SceNetAdhocDistributeWorker *w = &g_distribute_ctx->workers[i];
        if (w->state == 2 && sceNet_lib_E0A81C7C(w->mac, mac, 6) == 0) {
            return w;
        }
    }
    return NULL;
}

/* 0x2da0 */
static int __attribute__((noinline)) sub_2da0(uint32_t arglen, void *argp)
{
    (void)arglen;
    uint32_t out_bits;
    int worker_idx = *(int *)argp;
    SceNetAdhocDistributeWorker *w = &g_distribute_ctx->workers[worker_idx];
    int res;

    for (;;) {
        sub_305c(w);
        res = sceKernelWaitSema(g_distribute_ctx->queue_sema, 1, NULL);
        if (res < 0) {
            break;
        }
        if (g_distribute_ctx->max_concurrent > 0) {
            res = sceKernelWaitSema(g_distribute_ctx->concurrent_sema, 1, NULL);
            if (res < 0) {
                break;
            }
        }

        int intr = sceKernelCpuSuspendIntr();
        SceNetAdhocDownloadQueueItem *item = sub_2638(&g_distribute_ctx->queue);
        sceKernelCpuResumeIntr(intr);

        if (item == NULL) {
            if (g_distribute_ctx->max_concurrent > 0) {
                sceKernelSignalSema(g_distribute_ctx->concurrent_sema, 1);
            }
            continue;
        }

        res = sceNetAdhocMatchingSelectTarget(g_distribute_ctx->matching_id, item->mac, 0, NULL);
        if (res < 0) {
            sub_2578(item);
            if ((uint32_t)(res + 0x7fbef7f4U) < 2U) {
                if (g_distribute_ctx->max_concurrent > 0) {
                    sceKernelSignalSema(g_distribute_ctx->concurrent_sema, 1);
                }
                continue;
            }
            break;
        }

        intr = sceKernelCpuSuspendIntr();
        sceKernelMemcpy(w->mac, item->mac, 6);
        sceKernelMemcpy(w->nickname, item->nickname, 0x80);
        w->state = 2;
        sceKernelCpuResumeIntr(intr);
        sub_2578(item);

        res = sceKernelWaitEventFlag(w->evf_id, 3, 0x11, &out_bits, NULL);
        if (res < 0 || (out_bits & 2) != 0) {
            break;
        }

        if (w->state == 4) {
            if (g_distribute_ctx->max_concurrent > 0) {
                sceKernelSignalSema(g_distribute_ctx->concurrent_sema, 1);
            }
        } else {
            res = sub_30d4(worker_idx);
            if (res < 0 && g_distribute_ctx->max_concurrent > 0) {
                sceKernelSignalSema(g_distribute_ctx->concurrent_sema, 1);
            }
            sceNetAdhocMatchingCancelTarget(g_distribute_ctx->matching_id, w->mac);
            intr = sceKernelCpuSuspendIntr();
            w->error_code = res;
            w->state = 4;
            sceKernelCpuResumeIntr(intr);
        }

        res = sceKernelWaitEventFlag(w->evf_id, 3, 0x11, &out_bits, NULL);
        if (res < 0 || (out_bits & 2) != 0) {
            break;
        }
        if (g_distribute_ctx->state != 1) {
            break;
        }
    }

    sceNetFreeThreadinfo(w->thid);
    int intr = sceKernelCpuSuspendIntr();
    w->error_code = res;
    w->state = 0;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x305c */
static void __attribute__((noinline)) sub_305c(SceNetAdhocDistributeWorker *worker)
{
    int intr = sceKernelCpuSuspendIntr();
    worker->state = 1;
    worker->error_code = 0;
    sceKernelMemset(worker->mac, 0, 6);
    sceKernelMemset(worker->nickname, 0, 0x80);
    worker->sent_len = 0;
    worker->total_len = g_distribute_ctx->total_data_len;
    sceKernelCpuResumeIntr(intr);
}

/* 0x30d4 */
static int __attribute__((noinline)) sub_30d4(int worker_idx)
{
    uint8_t local_mac[16];
    uint32_t net_len;
    int chunk_len;
    void *chunk_ptr;
    int chunk_total;

    SceNetAdhocDistributeWorker *w = &g_distribute_ctx->workers[worker_idx];
    int res = sceNetGetLocalEtherAddr(local_mac);
    if (res < 0) {
        return res;
    }

    int retries = 20;
    int ptp_id = -1;
    for (;;) {
        if (g_distribute_ctx->state != 1) {
            return 0x80410c07;
        }
        ptp_id = sceNetAdhocPtpOpen(local_mac, 0x8001, w->mac, 0x8001, 0x2d20, 200000, 50, 0);
        res = ptp_id;
        if (ptp_id < 0) {
            goto cleanup;
        }
        w->ptp_id = ptp_id;
        res = sceNetAdhocPtpConnect(ptp_id, 10000000, 0);
        if (res >= 0) {
            break;
        }
        if (res != (int)0x80410718 || retries <= 0) {
            goto cleanup;
        }
        retries--;
        w->ptp_id = -1;
        sceNetAdhocPtpClose(ptp_id, 0);
        sceKernelDelayThread(500000);
    }

    net_len = __builtin_bswap32((uint32_t)g_distribute_ctx->total_data_len);
    chunk_len = 4;
    res = sceNetAdhocPtpSend(ptp_id, &net_len, &chunk_len, 0, 0);
    if (res < 0) {
        goto cleanup;
    }
    res = sceNetAdhocPtpFlush(ptp_id, 0, 0);
    if (res < 0) {
        goto cleanup;
    }

    int rem = g_distribute_ctx->total_data_len;
    while (rem > 0) {
        if (g_distribute_ctx->send_cb(worker_idx, g_distribute_ctx->cb_arg,
                                      g_distribute_ctx->total_data_len - rem,
                                      &chunk_ptr, &chunk_total) < 0) {
            res = 0x80410c0b;
            goto cleanup;
        }
        int offset = 0;
        while (offset < chunk_total) {
            int rem_chunk = chunk_total - offset;
            chunk_len = (rem_chunk < 0x5a5) ? rem_chunk : 0x5a4;
            res = sceNetAdhocPtpSend(ptp_id, (uint8_t *)chunk_ptr + offset, &chunk_len, 0, 0);
            offset += 0x5a4;
            if (res < 0) {
                goto cleanup;
            }
            w->sent_len += (uint32_t)chunk_len;
        }
        rem -= chunk_total;
    }

    res = sceNetAdhocPtpFlush(ptp_id, 0, 0);

cleanup:
    w->ptp_id = -1;
    if (ptp_id >= 0) {
        sceNetAdhocPtpClose(ptp_id, 0);
    }
    return res;
}

/* 0x334c */
int sceNetAdhocDownload_48252AF0(uint32_t pool_size)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (g_distribute_inited) {
        return 0x80410c02;
    }
    int res = sub_2480(pool_size, 2);
    if (res < 0) {
        return res;
    }
    g_distribute_inited = 1;
    return 0;
}

/* 0x33cc */
int sceNetAdhocDownload_E01A6429(uint32_t pool_size, int part)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (g_distribute_inited) {
        return 0x80410c02;
    }
    if (part != 2 && part != 5) {
        return 0x80410c03;
    }
    int res = sub_2480(pool_size, part);
    if (res < 0) {
        return res;
    }
    g_distribute_inited = 1;
    return 0;
}

/* 0x3478 */
int sceNetAdhocDownload_3B6A7B35(int max_workers, int worker_pri, int worker_stack,
                                 uint32_t data_len, void *data_ptr, void *extra_ptr,
                                 int total_send_len, void *cb_arg,
                                 int (*send_cb)(int, void *, int, void **, int *),
                                 int max_concurrent)
{
    if (_sceKernelCheckThreadStack() < 0xfc0) {
        return 0x80410005;
    }
    return sub_378c(max_workers, worker_pri, worker_stack, 2,
                    data_len, data_ptr, extra_ptr, total_send_len,
                    cb_arg, send_cb, max_concurrent);
}

/* 0x3540 */
int sceNetAdhocDownload_90B616A4(int max_workers, int worker_pri, int worker_stack, int worker_part,
                                 uint32_t data_len, void *data_ptr, void *extra_ptr,
                                 int total_send_len, void *cb_arg,
                                 int (*send_cb)(int, void *, int, void **, int *),
                                 int max_concurrent)
{
    if (_sceKernelCheckThreadStack() < 0xfc0) {
        return 0x80410005;
    }
    return sub_378c(max_workers, worker_pri, worker_stack, worker_part,
                    data_len, data_ptr, extra_ptr, total_send_len,
                    cb_arg, send_cb, max_concurrent);
}

/* 0x360c */
int sceNetAdhocDownload_8A987F07(int evth_pri, int evth_stack, int inth_pri, int inth_stack)
{
    if (_sceKernelCheckThreadStack() < 0xfd0) {
        return 0x80410005;
    }
    return sub_3bf0(evth_pri, evth_stack, 2, inth_pri, inth_stack, 2);
}

/* 0x368c */
int sceNetAdhocDownload_3E91D2DC(int evth_pri, int evth_stack, int evth_part,
                                 int inth_pri, int inth_stack, int inth_part)
{
    if (_sceKernelCheckThreadStack() < 0xfd0) {
        return 0x80410005;
    }
    return sub_3bf0(evth_pri, evth_stack, evth_part, inth_pri, inth_stack, inth_part);
}

/* 0x3724 */
int sceNetAdhocDownload_827157E2(void)
{
    if (_sceKernelCheckThreadStack() < 0xff0) {
        return 0x80410005;
    }
    if (!g_distribute_inited) {
        return 0;
    }
    if (g_distribute_ctx != NULL) {
        return 0x80410c04;
    }
    sub_24bc();
    g_distribute_inited = 0;
    return 0;
}

/* 0x378c */
static int __attribute__((noinline)) sub_378c(int max_workers, int worker_pri, int worker_stack, int worker_part,
                                              uint32_t data_len, void *data_ptr, void *extra_ptr,
                                              int total_send_len, void *cb_arg,
                                              int (*send_cb)(int, void *, int, void **, int *),
                                              int max_concurrent)
{
    char nickname[0x80];
    uint32_t th_opt[2];

    if (!g_distribute_inited) {
        return 0x80410c01;
    }
    if (max_workers <= 0 || max_workers >= 9) {
        return 0x80410c03;
    }
    if (worker_part != 2 && worker_part != 5) {
        return 0x80410c03;
    }
    if (data_len == 0 || data_ptr == NULL) {
        return 0x80410c03;
    }
    if (g_distribute_ctx != NULL) {
        return 0x80410c04;
    }

    int res = sceUtilityGetSystemParamString(1, nickname, 0x80);
    if (res < 0) {
        return res;
    }

    sceKernelMemset(&g_distribute_ctx_buf, 0, sizeof(g_distribute_ctx_buf));
    g_distribute_ctx_buf.concurrent_sema = -1;
    g_distribute_ctx_buf.matching_id = -1;
    g_distribute_ctx_buf.queue_sema = -1;

    uint32_t workers_bytes = (uint32_t)max_workers * sizeof(SceNetAdhocDistributeWorker);
    SceNetAdhocDistributeWorker *workers = (SceNetAdhocDistributeWorker *)sub_24f4(workers_bytes);
    g_distribute_ctx_buf.workers = workers;
    if (workers == NULL) {
        return 0x80410c06;
    }

    sceKernelMemset(workers, 0, workers_bytes);
    for (int i = 0; i < max_workers; i++) {
        workers[i].thid = -1;
        workers[i].ptp_id = -1;
        workers[i].evf_id = -1;
    }

    g_distribute_ctx_buf.state = 0;
    g_distribute_ctx_buf.opt_len = data_len + 0x86;
    g_distribute_ctx = &g_distribute_ctx_buf;
    g_distribute_ctx_buf.max_workers = max_workers;
    if (extra_ptr != NULL) {
        g_distribute_ctx_buf.opt_len = data_len + 0x91;
    }

    res = 0x80410c06;
    g_distribute_ctx->opt_buf = sub_24f4(g_distribute_ctx->opt_len);
    if (g_distribute_ctx->opt_buf == NULL) {
        goto fail;
    }

    uint8_t *p = sub_25a8(1, nickname, 0x80, (uint8_t *)g_distribute_ctx->opt_buf);
    p = sub_25a8(2, data_ptr, data_len, p);
    if (extra_ptr != NULL) {
        sub_25a8(3, extra_ptr, 8, p);
    }

    g_distribute_ctx->max_concurrent = max_concurrent;
    g_distribute_ctx->total_data_len = total_send_len;
    g_distribute_ctx->cb_arg = cb_arg;
    g_distribute_ctx->send_cb = send_cb;

    res = sceNetAdhocMatchingCreate(1, max_workers + 1, 0x8001, 0x800, 1000000, 30000000, 3, 1000000, sub_2b28);
    if (res < 0) {
        goto fail;
    }
    g_distribute_ctx->matching_id = res;

    res = sceKernelCreateSema("SceNetAdhocDistribute", 0, 0, 0x80, NULL);
    if (res < 0) {
        goto fail;
    }
    g_distribute_ctx->queue_sema = res;

    if (g_distribute_ctx->max_concurrent > 0) {
        res = sceKernelCreateSema("SceNetAdhocDistributeCounter", 0,
                                  g_distribute_ctx->max_concurrent,
                                  g_distribute_ctx->max_concurrent, NULL);
        if (res < 0) {
            goto fail;
        }
        g_distribute_ctx->concurrent_sema = res;
    }

    sceKernelMemset(th_opt, 0, 8);
    th_opt[0] = 8;
    th_opt[1] = (uint32_t)worker_part;

    for (int i = 0; i < max_workers; i++) {
        res = sceKernelCreateThread("SceNetAdhocDistributeWorker", sub_2da0,
                                    worker_pri, worker_stack, 0, th_opt);
        if (res < 0) {
            goto fail;
        }
        g_distribute_ctx->workers[i].thid = res;
        res = sceKernelCreateEventFlag("SceNetAdhocDistributeWorker", 0, 0, NULL);
        if (res < 0) {
            goto fail;
        }
        g_distribute_ctx->workers[i].evf_id = res;
    }

    return 0;

fail:
    if (g_distribute_ctx != NULL) {
        for (int i = 0; i < max_workers; i++) {
            SceNetAdhocDistributeWorker *w = &g_distribute_ctx->workers[i];
            if (w->evf_id >= 0) {
                sceKernelDeleteEventFlag(w->evf_id);
            }
            if (w->thid >= 0) {
                sceKernelDeleteThread(w->thid);
            }
        }
        if (g_distribute_ctx->concurrent_sema >= 0) {
            sceKernelDeleteSema(g_distribute_ctx->concurrent_sema);
        }
        if (g_distribute_ctx->queue_sema >= 0) {
            sceKernelDeleteSema(g_distribute_ctx->queue_sema);
        }
        if (g_distribute_ctx->matching_id >= 0) {
            sceNetAdhocMatchingDelete(g_distribute_ctx->matching_id);
        }
        if (g_distribute_ctx->opt_buf != NULL) {
            sub_2578(g_distribute_ctx->opt_buf);
        }
        sub_2578(g_distribute_ctx->workers);
        g_distribute_ctx = NULL;
    }
    return res;
}

/* 0x3bf0 */
static int __attribute__((noinline)) sub_3bf0(int evth_pri, int evth_stack, int evth_part,
                                              int inth_pri, int inth_stack, int inth_part)
{
    int idx;
    int num_wait;
    uint32_t timeout;

    if (!g_distribute_inited) {
        return 0x80410c01;
    }
    if (g_distribute_ctx == NULL) {
        return 0x80410c05;
    }
    if (g_distribute_ctx->state != 0) {
        return 0x80410c07;
    }

    g_distribute_ctx->state = 1;
    int res = 0;
    for (idx = 0; idx < g_distribute_ctx->max_workers; idx++) {
        SceNetAdhocDistributeWorker *w = &g_distribute_ctx->workers[idx];
        sub_305c(w);
        if (w->thid >= 0) {
            res = sceKernelStartThread(w->thid, 4, &idx);
            if (res < 0) {
                goto fail;
            }
        }
    }

    res = sceNetAdhocMatching_E8454C65(g_distribute_ctx->matching_id,
                                       evth_pri, evth_stack, evth_part,
                                       inth_pri, inth_stack, inth_part,
                                       g_distribute_ctx->opt_len, g_distribute_ctx->opt_buf);
    if (res >= 0) {
        return 0;
    }

fail:
    for (;;) {
        sceKernelCancelSema(g_distribute_ctx->queue_sema, 0, &num_wait);
        int retry = 0;
        for (int i = 0; i < g_distribute_ctx->max_workers; i++) {
            int thid = g_distribute_ctx->workers[i].thid;
            if (thid >= 0) {
                timeout = 100000;
                if (sceKernelWaitThreadEnd(thid, &timeout) == (int)0x800201a8) {
                    retry = 1;
                    break;
                }
            }
        }
        if (!retry) {
            break;
        }
    }
    g_distribute_ctx->state = 0;
    return res;
}
