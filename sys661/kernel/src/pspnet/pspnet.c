#include "pspnet_internal.h"

/* =========================================================================
 * Local WLAN / interface ioctl structures
 * ========================================================================= */

typedef struct NetWlanScanReq {
    u32 bss_type;               /* 0x00 */
    u8 bssid[6];                /* 0x04 */
    u8 channels[14];            /* 0x0A */
    u8 ssid_len;                /* 0x18 */
    u8 ssid_pad[3];             /* 0x19 */
    u8 ssid[32];                /* 0x1C */
    u32 scan_type;              /* 0x3C */
    u32 probe_delay;            /* 0x40 */
    u32 min_dwell;              /* 0x44 */
    u32 max_dwell;              /* 0x48 */
} NetWlanScanReq;

typedef struct NetWlanBssDesc {
    u8 bssid[6];                /* 0x00 */
    u8 channel;                 /* 0x06 */
    u8 ssid_len;                /* 0x07 */
    u8 ssid[32];                /* 0x08 */
    u32 bss_type;               /* 0x28 */
    u32 beacon_period;          /* 0x2C */
    u8 rates[14];               /* 0x30 */
    u16 capability;             /* 0x3E */
    u8 ie_body[8];              /* 0x40 */
    s16 rssi;                   /* 0x48 */
    u8 tail[18];                /* 0x4A */
} NetWlanBssDesc;

typedef struct NetWlanScanEntry {
    struct NetWlanScanEntry *next; /* 0x00 */
    NetWlanBssDesc desc;           /* 0x04 */
} NetWlanScanEntry;

typedef struct NetWlanJoinReq {
    NetWlanBssDesc bss;         /* 0x00 */
    u8 extra_ies[20];           /* 0x5C */
} NetWlanJoinReq;

typedef struct NetWlanWepKey {
    u32 key_type;
    u8 key_bytes[16];
} NetWlanWepKey;

typedef struct NetWlanWepConfig {
    u32 mode;
    u32 tx_key_index;
    u32 reserved;
    NetWlanWepKey keys[4];
    u8 pad[68];
} NetWlanWepConfig;

typedef struct NetWlanLinkStats {
    char name[16];
    u8 rssi_dbm;
    u8 link_quality;
    u16 tx_rate;
    u16 rx_rate;
    u8 channel;
    u8 pad;
} NetWlanLinkStats;

typedef struct NetIfnetList {
    NetIfnet *head;
    NetIfnet **tail;
} NetIfnetList;

typedef struct NetPppSyncPkt {
    u32 tx_accm_ctl;            /* 0 */
    u32 rx_accm_ctl;            /* 1 */
    u32 drv_cfg[4];             /* 2..5 */
    u32 xaccm[8];               /* 6..13 */
    u32 raccm;                  /* 14 */
} NetPppSyncPkt;

PSPNET_STATIC_ASSERT(wlan_scan_req_size, sizeof(NetWlanScanReq) == 76);
PSPNET_STATIC_ASSERT(wlan_bss_desc_size, sizeof(NetWlanBssDesc) == 92);
PSPNET_STATIC_ASSERT(wlan_scan_entry_desc, offsetof(NetWlanScanEntry, desc) == 4);
PSPNET_STATIC_ASSERT(wlan_scan_entry_rssi, offsetof(NetWlanScanEntry, desc) + offsetof(NetWlanBssDesc, rssi) == 76);
PSPNET_STATIC_ASSERT(wlan_join_req_size, sizeof(NetWlanJoinReq) == 112);
PSPNET_STATIC_ASSERT(wlan_wep_cfg_size, sizeof(NetWlanWepConfig) == 160);

/* =========================================================================
 * Global state
 * ========================================================================= */

/* WLAN scan buffer & active BSS pointer */
static u8 g_wlan_scan_buf[1536];
static NetWlanScanEntry *g_wlan_cur_bss = NULL;
static u32 g_mem_anchor = 0;

/* Netintr state */
static SceUID g_netintr_thid = -1;
static SceUID g_netintr_evf = -1;
static u32 g_netisr_bits = 0;

/* Protocol / module hook registrations */
static int g_ip_hooks_reg = 0;
static int g_adhoc_slp_reg = 0;
static int g_if_hooks_reg = 0;
static int g_ether_hooks_reg = 0;
static int g_arp_hooks_reg = 0;
static int g_ppp_hooks_reg = 0;
static int g_unused_hook_reg = 0;
static int g_adhoc_intr_reg = 0;
static int g_adhoc_updown_reg = 0;
static int g_slp_notify_reg = 0;
static int g_eapol_hook_reg = 0;

static void (*g_ip_intr_fn)(void) = NULL;
static void (*g_arp_intr_fn)(void) = NULL;
static void (*g_unused_intr_fn)(void) = NULL;
static void (*g_adhoc_intr_fn)(void) = NULL;
static void (*g_adhoc_if_cb)(void *) = NULL;
static void (*g_adhoc_updown_cb)(NetIfnet *) = NULL;

/* Thread info list */
static SceUID g_threadlist_init = -1;
static NetThreadInfo *g_thread_head = NULL;
static NetThreadInfo **g_thread_tail = &g_thread_head;
static SceLwMutexWorkarea g_thread_mtx;

/* Callout subsystem */
static SceUID g_callout_sema = -1;
static SceUID g_callout_thid = -1;
static NetCallout *g_callout_ready_head = NULL;
static NetCallout **g_callout_ready_tail = &g_callout_ready_head;
static NetCallout *g_callout_pend_head = NULL;
static NetCallout **g_callout_pend_tail = &g_callout_pend_head;

/* Sleep queue subsystem */
typedef struct SleepBucket {
    NetThreadInfo *head;
    NetThreadInfo **tail;
} SleepBucket;

static SleepBucket g_slpque[128];
static SceLwMutexWorkarea g_slpque_mtx;
static SceUID g_slpque_init = -1;
static void (*g_slp_pre_cb)(NetThreadInfo *) = NULL;
static void (*g_slp_notify_cb)(int, int) = NULL;
static void (*g_slp_post_cb)(int) = NULL;

/* SPL mutex */
static SceLwMutexWorkarea g_spl_mtx;
static SceUID g_spl_init = -1;

/* Select wait channel */
u32 g_selwait = 0;

/* Interface table & hooks */
static void (*g_if_rtrequest_cb)(int, int, int) = NULL;
static void (*g_if_init_cb)(void) = NULL;
static void (*g_if_attach_cb)(NetIfnet *) = NULL;
static void (*g_if_detach_addr_cb)(NetIfaddr *) = NULL;
static void (*g_if_purge_cb)(NetIfnet *, int) = NULL;
static void (*g_if_detach_cb)(NetIfnet *) = NULL;
static void (*g_if_detach_post_cb)(NetIfnet *, u32) = NULL;
static void (*g_if_down_pre_cb)(void) = NULL;
static void (*g_if_down_post_cb)(NetIfnet *) = NULL;
static void (*g_if_up_cb)(NetIfnet *) = NULL;
static int (*g_if_ioctl_pre_cb)(u32, void *, void *, int *) = NULL;
static int (*g_if_ioctl_so_cb)(void *, int *) = NULL;
static int (*g_if_ioctl_post_cb)(void *, u32, void *, NetIfnet *) = NULL;
static void (*g_if_extra_init_cb)(void) = NULL;
static int g_if_extra_init_reg = 0;

static s32 g_if_index = 0;
static NetIfaddr *g_ifnet_addrs_tbl[8];
static NetIfnet *g_ifindex2ifnet_tbl[8];
static NetIfaddr **g_ifnet_addrs = g_ifnet_addrs_tbl;
static NetIfnet **g_ifindex2ifnet = g_ifindex2ifnet_tbl;
static NetIfnetList g_ifnet_list = { NULL, &g_ifnet_list.head };
static NetCallout g_if_slowtimo_callout;
static u32 g_ifq_maxlen = 50;

/* Ethernet hooks */
static int (*g_ether_output_cb)(NetIfnet *, NetMbuf *, void *, void *) = NULL;
static void (*g_ether_input_pre_cb)(NetIfnet *, NetMbuf *, u16, int *) = NULL;
static void (*g_ether_input_dispatch_cb)(NetIfnet *, NetMbuf *, u16, u8 *, NetIfQueue **, int *, int *) = NULL;
static int (*g_ether_multi_cb)(void *, u8 *, u8 *, int *) = NULL;
static int (*g_eapol_input_cb)(NetIfnet *, NetMbuf *, NetIfQueue **) = NULL;
static void (*g_arp_ifinit_cb)(NetIfnet *, void *) = NULL;
static const u8 g_ether_broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* PPP hooks */
static void (*g_ppp_attach_cb)(NetIfnet *) = NULL;
static void (*g_ppp_detach_cb)(NetIfnet *) = NULL;
static int (*g_ppp_output_cb)(NetIfnet *, NetMbuf *, void *, void *) = NULL;
static void (*g_ppp_input_cb)(NetIfnet *, NetMbuf *) = NULL;
static int (*g_ppp_ioctl_cb)(NetIfnet *, u32, void *) = NULL;
static NetMbuf *(*g_ppp_dequeue_cb)(NetIfnet *) = NULL;

/* R521 PRNG state */
static u32 g_prng_state[521];
static int g_prng_idx = 0;

/* Forward declarations */
static void net_prng_seed(u32 seed);
static int net_ifhandle_init(SceUID partId, int poolSize);
static int net_ifhandle_term(void);
static int callout_init(int prio, SceUID partId, int stackSize);
static int callout_term(void);
static int spl_init(void);
static int spl_term(void);
static int slpque_init(void);
static int slpque_term(void);
static int threadlist_init(void);
static int threadlist_term(void);
static int netintr_init(int prio, SceUID partId, int stackSize);
static int netintr_term(void);
static void ifinit(void);
static void if_slowtimo(void *arg);
static void ether_ifattach(NetIfnet *ifp, const void *lla);
static void ether_ifdetach(NetIfnet *ifp);
static int ether_ioctl(NetIfnet *ifp, u32 cmd, void *req_ptr);
static void ether_start(NetIfnet *ifp);
static void ppp_attach(NetIfnet *ifp);
static void ppp_detach(NetIfnet *ifp);
static int ifioctl(void *so, u32 cmd, void *req_ptr);

/* =========================================================================
 * Inline helpers matching Sony's compiler inlining
 * ========================================================================= */

static inline __attribute__((always_inline)) int bssid_less_inline(const u8 *lhs, const u8 *rhs)
{
    u32 hi_lhs = ((u32)lhs[0] << 16) | ((u32)lhs[1] << 8) | lhs[2];
    u32 hi_rhs = ((u32)rhs[0] << 16) | ((u32)rhs[1] << 8) | rhs[2];
    if (hi_lhs != hi_rhs)
        return hi_lhs < hi_rhs;
    u32 lo_lhs = ((u32)lhs[3] << 16) | ((u32)lhs[4] << 8) | lhs[5];
    u32 lo_rhs = ((u32)rhs[3] << 16) | ((u32)rhs[4] << 8) | rhs[5];
    return lo_lhs < lo_rhs;
}

static inline __attribute__((always_inline)) void free_if_events_inline(NetIfnet *ifp)
{
    NetIfEvent *ev = ifp->if_ev_head;
    while (ev != NULL) {
        NetIfEvent *next = ev->ev_next;
        if (next != NULL)
            next->ev_prev = ev->ev_prev;
        else
            ifp->if_ev_tail = ev->ev_prev;
        *ev->ev_prev = next;
        sceNetIfhandle_lib_76BAD213(ev);
        ev = ifp->if_ev_head;
    }
}

static inline __attribute__((always_inline)) NetIfnet *ifunit_inline(const char *name)
{
    int spl = sceNet_lib_94B44F26();
    NetIfnet *ifp = g_ifnet_list.head;
    while (ifp != NULL) {
        if ((void *)ifp->if_output != (void *)sceNet_lib_8C55B410 &&
            sceNet_lib_A0F16ABD(ifp->if_xname, name) == 0) {
            sceNet_lib_515B2F33(spl);
            return ifp;
        }
        ifp = ifp->if_next;
    }
    sceNet_lib_515B2F33(spl);
    return NULL;
}

static inline __attribute__((always_inline)) void if_slowtimo_body_inline(void)
{
    int spl = sceNet_lib_94B44F26();
    NetIfnet *ifp = g_ifnet_list.head;
    while (ifp != NULL) {
        u16 timer = ifp->if_timer;
        if (timer != 0) {
            ifp->if_timer = (u16)(timer - 1);
            if (timer == 1 && ifp->if_watchdog != NULL)
                ifp->if_watchdog(ifp);
        }
        ifp = ifp->if_next;
    }
    sceNet_lib_515B2F33(spl);
    sceNet_lib_7C86FBA4(&g_if_slowtimo_callout, 100, if_slowtimo, NULL);
}

static inline __attribute__((always_inline)) void prng_twist_inline(void)
{
    int idx;
    for (idx = 0; idx < 32; idx++)
        g_prng_state[idx] ^= g_prng_state[idx + 489];
    for (idx = 32; idx < 521; idx++)
        g_prng_state[idx] ^= g_prng_state[idx - 32];
}

static inline __attribute__((always_inline)) int hex_val_inline(char ch)
{
    if ((u8)(ch - '0') < 10)
        return ch - '0';
    if ((u8)(ch - 'A') < 26)
        return ch - 'A' + 10;
    if ((u8)(ch - 'a') < 26)
        return ch - 'a' + 10;
    return 9999999;
}

/* =========================================================================
 * WLAN helper / ioctl wrappers
 * ========================================================================= */

int sceNet_lib_43EE76EC(const char *ifname, const void *ssid, u32 ssid_len,
                        void *wep_cfg, void *auth_cfg, u32 bss_type)
{
    NetWlanScanReq scan_req;
    NetWlanJoinReq join_req;
    u8 ssid_cmp[32];
    struct {
        char ifname[16];
        NetWlanScanReq *req;
        u32 buf_len;
        void *buf;
    } io_scan;
    struct {
        char ifname[16];
        NetWlanJoinReq *req;
        u32 unused;
    } io_join;
    NetWlanScanEntry *sorted_head = NULL;
    u32 res_len = 1536;
    int idx, ret;

    sceKernelMemset(&scan_req, 0, sizeof(scan_req));
    if (ssid != NULL)
        sceKernelMemcpy(scan_req.ssid, ssid, ssid_len);
    scan_req.ssid_len = (u8)ssid_len;
    scan_req.bss_type = bss_type;
    for (idx = 0; idx < 14; idx++)
        scan_req.channels[idx] = (u8)(idx + 1);
    scan_req.scan_type = 1;
    scan_req.probe_delay = 0;
    scan_req.min_dwell = 6;
    scan_req.max_dwell = 100;

    sceKernelMemset(g_wlan_scan_buf, 0, sizeof(g_wlan_scan_buf));
    sceKernelMemset(&io_scan, 0, sizeof(io_scan));
    sceNet_lib_B5CE388A(io_scan.ifname, ifname, 16);
    io_scan.req = &scan_req;
    io_scan.buf_len = 1536;
    io_scan.buf = g_wlan_scan_buf;

    ret = sceNet_lib_13A8B98A(NULL, 0xC02069D4u, &io_scan, 1);
    if (ret == 0)
        res_len = io_scan.buf_len;
    if (ret < 0)
        return ret;
    if (res_len == 0)
        return SCE_NET_ERROR_SCAN_NONE;

    sceKernelMemset(ssid_cmp, 0, sizeof(ssid_cmp));
    if (ssid != NULL)
        sceKernelMemcpy(ssid_cmp, ssid, ssid_len);

    {
        NetWlanScanEntry *cur = (NetWlanScanEntry *)g_wlan_scan_buf;
        while (cur != NULL) {
            NetWlanScanEntry *next = cur->next;
            if (ssid_len != 0 && sceNet_lib_E0A81C7C(cur->desc.ssid, ssid_cmp, 32) != 0) {
                cur = next;
                continue;
            }
            if (bss_type != 0 && cur->desc.bss_type != bss_type) {
                cur = next;
                continue;
            }
            /* Insert sorted by RSSI descending */
            NetWlanScanEntry **pp = &sorted_head;
            while (*pp != NULL && (*pp)->desc.rssi >= cur->desc.rssi)
                pp = &(*pp)->next;
            cur->next = *pp;
            *pp = cur;
            cur = next;
        }
    }

    ret = SCE_NET_ERROR_JOIN_FAIL;
    while (sorted_head != NULL) {
        NetWlanScanEntry *bss = sorted_head;
        ret = sceNet_lib_83FE280A(ifname, wep_cfg);
        if (ret < 0)
            break;
        ret = sceNet_lib_891723D5(ifname, auth_cfg);
        if (ret < 0)
            break;

        sceKernelMemset(&join_req, 0, sizeof(join_req));
        sceKernelMemcpy(&join_req.bss, &bss->desc, sizeof(NetWlanBssDesc));
        g_wlan_cur_bss = bss;

        sceKernelMemset(&io_join, 0, sizeof(io_join));
        sceNet_lib_B5CE388A(io_join.ifname, ifname, 16);
        io_join.req = &join_req;
        ret = sceNet_lib_13A8B98A(NULL, 0xC01869D6u, &io_join, 1);
        if (ret == 0)
            break;
        g_wlan_cur_bss = NULL;
        sorted_head = bss->next;
    }
    return ret;
}

int sceNet_lib_4BF9E1DE(const char *ifname, const void *ssid, u32 ssid_len,
                        const void *key, u32 key_len, void *auth_cfg)
{
    NetWlanWepConfig wep;
    int idx;

    sceKernelMemset(&wep, 0, sizeof(wep));
    if (key != NULL) {
        if (key_len != 5 && key_len != 13)
            return SCE_NET_ERROR_EINVAL;
        wep.mode = 1;
        wep.tx_key_index = 1;
        for (idx = 0; idx < 4; idx++) {
            if (key_len == 5)
                wep.keys[idx].key_type = 1;
            else if (key_len == 13)
                wep.keys[idx].key_type = 2;
            sceKernelMemcpy(wep.keys[idx].key_bytes, key, key_len);
        }
    } else {
        wep.mode = 0;
        wep.tx_key_index = 0;
    }
    return sceNet_lib_43EE76EC(ifname, ssid, ssid_len, &wep, auth_cfg, 1);
}

int sceNet_lib_D5B64E37(const char *ifname, const void *ssid, u32 ssid_len, u32 channel)
{
    NetWlanScanReq scan_req;
    NetWlanJoinReq bss_req;
    NetWlanBssDesc cur_bss;
    u8 ssid_cmp[32];
    struct {
        char ifname[16];
        NetWlanScanReq *req;
        u32 buf_len;
        void *buf;
        u32 status;
    } io_scan;
    struct {
        char ifname[16];
        NetWlanJoinReq *req;
        u32 status;
    } io_cmd;
    u32 status = 0;
    u32 res_len = 1536;
    u32 start_us, scan_us;
    NetWlanBssDesc *best = NULL;
    int ret, attempt;

    if ((channel - 1) >= 14 || ssid == NULL || ssid_len == 0)
        return SCE_NET_ERROR_EINVAL;

    sceKernelMemset(&scan_req, 0, sizeof(scan_req));
    sceKernelMemcpy(scan_req.ssid, ssid, ssid_len);
    scan_req.bss_type = 2;
    scan_req.channels[0] = (u8)channel;
    scan_req.channels[1] = 0;
    scan_req.ssid_len = (u8)ssid_len;
    scan_req.scan_type = 1;
    scan_req.probe_delay = 0;
    scan_req.min_dwell = 6;
    scan_req.max_dwell = 100;

    sceKernelMemset(g_wlan_scan_buf, 0, sizeof(g_wlan_scan_buf));
    start_us = sceKernelGetSystemTimeLow();
    sceKernelMemset(&io_scan, 0, 32);
    sceNet_lib_B5CE388A(io_scan.ifname, ifname, 16);
    io_scan.req = &scan_req;
    io_scan.buf_len = 1536;
    io_scan.buf = g_wlan_scan_buf;
    ret = sceNet_lib_13A8B98A(NULL, 0xC02069D4u, &io_scan, 1);
    if (ret == 0) {
        res_len = io_scan.buf_len;
        status = io_scan.status;
    }
    if (ret < 0)
        return ret;

    scan_us = sceKernelGetSystemTimeLow() - start_us;
    if (res_len != 0) {
        sceKernelMemset(ssid_cmp, 0, sizeof(ssid_cmp));
        sceKernelMemcpy(ssid_cmp, ssid, ssid_len);
        NetWlanScanEntry *cur = (NetWlanScanEntry *)g_wlan_scan_buf;
        while (cur != NULL) {
            if (sceNet_lib_E0A81C7C(cur->desc.ssid, ssid_cmp, 32) == 0 && cur->desc.bss_type == 2) {
                if (best == NULL || bssid_less_inline(cur->desc.bssid, best->bssid))
                    best = &cur->desc;
            }
            cur = cur->next;
        }
    }

    if (best != NULL) {
        sceKernelMemset(&bss_req, 0, sizeof(bss_req));
        sceKernelMemcpy(&bss_req.bss, best, sizeof(NetWlanBssDesc));
        status = 0;
        sceKernelMemset(&io_cmd, 0, sizeof(io_cmd));
        sceNet_lib_B5CE388A(io_cmd.ifname, ifname, 16);
        io_cmd.req = &bss_req;
        ret = sceNet_lib_13A8B98A(NULL, 0xC01869D6u, &io_cmd, 1);
        if (ret == 0)
            status = io_cmd.status;
    } else {
        sceKernelMemset(&bss_req, 0, sizeof(bss_req));
        bss_req.bss.channel = (u8)channel;
        bss_req.bss.ssid_len = (u8)ssid_len;
        sceKernelMemcpy(bss_req.bss.ssid, ssid, ssid_len);
        bss_req.bss.bss_type = 2;
        bss_req.bss.beacon_period = 100;
        bss_req.bss.capability = 34;
        sceKernelMemset(&io_cmd, 0, sizeof(io_cmd));
        sceNet_lib_B5CE388A(io_cmd.ifname, ifname, 16);
        io_cmd.req = &bss_req;
        ret = sceNet_lib_13A8B98A(NULL, 0xC01869D5u, &io_cmd, 1);
        if (ret == 0) {
            status = io_cmd.status;
            sceKernelDelayThread(500000);
        }
    }

    if (ret < 0) {
        status = 0;
        sceNet_lib_DA02F383(ifname, &status);
        return ret;
    }

    for (attempt = 0; attempt < 3; attempt++) {
        u32 rand_delay = 0;
        if (scan_us < 1000000u) {
            u32 span = 1000000u - scan_us;
            rand_delay = (u32)sceNet_lib_1858883D() / (0xFFFFFFFFu / span + 1);
            sceKernelDelayThread(rand_delay);
        }
        ret = sceNet_lib_A55C914F(ifname, &cur_bss);
        if (ret < 0) {
            status = 0;
            sceNet_lib_DA02F383(ifname, &status);
            return ret;
        }

        sceKernelMemset(g_wlan_scan_buf, 0, sizeof(g_wlan_scan_buf));
        status = 0;
        sceKernelMemset(&io_scan, 0, 32);
        sceNet_lib_B5CE388A(io_scan.ifname, ifname, 16);
        res_len = 1536;
        io_scan.req = &scan_req;
        io_scan.buf_len = 1536;
        io_scan.buf = g_wlan_scan_buf;
        ret = sceNet_lib_13A8B98A(NULL, 0xC02069D4u, &io_scan, 1);
        if (ret == 0) {
            res_len = io_scan.buf_len;
            status = io_scan.status;
        }
        if (ret == SCE_NET_ERROR_ADHOC_CANCEL)
            return 0;
        if (ret < 0) {
            status = 0;
            sceNet_lib_DA02F383(ifname, &status);
            return ret;
        }

        NetWlanBssDesc *better = NULL;
        if (res_len != 0) {
            NetWlanScanEntry *cur = (NetWlanScanEntry *)g_wlan_scan_buf;
            while (cur != NULL) {
                if (sceNet_lib_E0A81C7C(cur->desc.ssid, cur_bss.ssid, 32) == 0 && cur->desc.bss_type == 2) {
                    const u8 *ref = (better != NULL) ? better->bssid : cur_bss.bssid;
                    if (bssid_less_inline(cur->desc.bssid, ref))
                        better = &cur->desc;
                }
                cur = cur->next;
            }
        }

        if (better != NULL) {
            status = 0;
            ret = sceNet_lib_DA02F383(ifname, &status);
            if (ret < 0) {
                status = 0;
                sceNet_lib_DA02F383(ifname, &status);
                return ret;
            }
            sceKernelDelayThread(3000000);
            sceKernelMemset(&bss_req, 0, sizeof(bss_req));
            sceKernelMemcpy(&bss_req.bss, better, sizeof(NetWlanBssDesc));
            status = 0;
            sceKernelMemset(&io_cmd, 0, sizeof(io_cmd));
            sceNet_lib_B5CE388A(io_cmd.ifname, ifname, 16);
            io_cmd.req = &bss_req;
            ret = sceNet_lib_13A8B98A(NULL, 0xC01869D6u, &io_cmd, 1);
            if (ret == 0)
                status = io_cmd.status;
            if (ret < 0) {
                status = 0;
                sceNet_lib_DA02F383(ifname, &status);
                return ret;
            }
        } else {
            if (attempt < 2 && scan_us < 1000000u)
                sceKernelDelayThread(1000000u - scan_us - rand_delay);
        }
    }
    return ret;
}

int sceNet_lib_DA02F383(const char *ifname, u32 *out)
{
    struct { char name[16]; u32 val; } req;
    sceKernelMemset(&req, 0, 20);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    int ret = sceNet_lib_13A8B98A(NULL, 0xC01469D8u, &req, 1);
    g_wlan_cur_bss = NULL;
    if (ret == 0)
        *out = req.val;
    return ret;
}

int sceNet_lib_83FE280A(const char *ifname, void *arg)
{
    struct { char name[16]; void *arg; } req;
    sceKernelMemset(&req, 0, 20);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.arg = arg;
    return sceNet_lib_13A8B98A(NULL, 0x801469E4u, &req, 1);
}

int sceNet_lib_891723D5(const char *ifname, void *arg)
{
    struct { char name[16]; void *arg; } req;
    sceKernelMemset(&req, 0, 20);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.arg = arg;
    return sceNet_lib_13A8B98A(NULL, 0x801469DFu, &req, 1);
}

int sceNet_lib_F355C73B(const char *ifname, int stat_type, void *out)
{
    if (stat_type >= 6) {
        struct { char name[16]; u32 val; } req;
        sceKernelMemset(&req, 0, 20);
        sceNet_lib_B5CE388A(req.name, ifname, 16);
        int ret = sceNet_lib_13A8B98A(NULL, 0x401469DAu, &req, 1);
        if (ret >= 0)
            *(u32 *)out = req.val;
        return ret;
    } else {
        NetWlanLinkStats req;
        sceKernelMemset(&req, 0, sizeof(req));
        sceNet_lib_B5CE388A(req.name, ifname, 16);
        int ret = sceNet_lib_13A8B98A(NULL, 0x401869D9u, &req, 1);
        if (ret >= 0) {
            switch (stat_type) {
            case 1: *(u8 *)out = req.rssi_dbm; break;
            case 2: *(u8 *)out = req.link_quality; break;
            case 3: *(u16 *)out = req.tx_rate; break;
            case 4: *(u16 *)out = req.rx_rate; break;
            case 5: *(u8 *)out = req.channel; break;
            default: ret = SCE_NET_ERROR_EINVAL; break;
            }
        }
        return ret;
    }
}

int sceNet_lib_A55C914F(const char *ifname, void *arg)
{
    struct { char name[16]; void *arg; } req;
    sceKernelMemset(&req, 0, 20);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.arg = arg;
    return sceNet_lib_13A8B98A(NULL, 0xC01469D7u, &req, 1);
}

int sceNet_lib_7BA3ED91(const char *ifname, void *scan_req, u32 *len_inout, void *buf, u32 *status_out)
{
    struct { char name[16]; void *req; u32 len; void *buf; u32 status; } req;
    sceKernelMemset(&req, 0, 32);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.req = scan_req;
    req.len = *len_inout;
    req.buf = buf;
    int ret = sceNet_lib_13A8B98A(NULL, 0xC02069D4u, &req, 1);
    if (ret == 0) {
        *len_inout = req.len;
        *status_out = req.status;
    }
    return ret;
}

int sceNet_lib_03164B12(const char *ifname, void *arg, u32 *out)
{
    struct { char name[16]; void *arg; u32 val; } req;
    sceKernelMemset(&req, 0, 24);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.arg = arg;
    int ret = sceNet_lib_13A8B98A(NULL, 0xC01869D5u, &req, 1);
    if (ret == 0)
        *out = req.val;
    return ret;
}

int sceNet_lib_389728AB(const char *ifname, void *arg, u32 *out)
{
    struct { char name[16]; void *arg; u32 val; } req;
    sceKernelMemset(&req, 0, 24);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.arg = arg;
    int ret = sceNet_lib_13A8B98A(NULL, 0xC01869D6u, &req, 1);
    if (ret == 0)
        *out = req.val;
    return ret;
}

int sceNet_lib_B755FA98(const char *ifname, const void *ssid, u32 ssid_len, void *wep_cfg, void *auth_cfg)
{
    return sceNet_lib_43EE76EC(ifname, ssid, ssid_len, wep_cfg, auth_cfg, 1);
}

int sceNet_lib_EB38431E(const char *ifname, void *out)
{
    (void)ifname;
    if (g_wlan_cur_bss == NULL)
        return SCE_NET_ERROR_NO_BSS;
    sceKernelMemcpy(out, &g_wlan_cur_bss->desc, sizeof(NetWlanBssDesc));
    return 0;
}

int sceNet_lib_AFA11338(const char *ifname, void *arg)
{
    struct { char name[16]; void *arg; } req;
    sceKernelMemset(&req, 0, 20);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.arg = arg;
    return sceNet_lib_13A8B98A(NULL, 0x801469D2u, &req, 1);
}

int sceNet_lib_B20F84F8(const char *ifname, void *arg)
{
    struct { char name[16]; void *arg; } req;
    sceKernelMemset(&req, 0, 20);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.arg = arg;
    return sceNet_lib_13A8B98A(NULL, 0x401469D3u, &req, 1);
}

int sceNet_lib_4F8F3808(const char *ifname, void *arg)
{
    struct { char name[16]; void *arg; } req;
    sceKernelMemset(&req, 0, 20);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.arg = arg;
    return sceNet_lib_13A8B98A(NULL, 0x401469DEu, &req, 1);
}

int sceNet_lib_0DFF67F9(const char *ifname, u32 *out)
{
    struct { char name[16]; u32 val; } req;
    sceKernelMemset(&req, 0, 20);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    int ret = sceNet_lib_13A8B98A(NULL, 0x401469E0u, &req, 1);
    if (ret == 0)
        *out = req.val;
    return ret;
}

int sceNet_lib_0D633F53(const char *ifname, u32 key_type, u32 key_flags, u32 *out)
{
    struct { char name[16]; u32 key_type; u32 key_flags; u32 out; } req;
    sceKernelMemset(&req, 0, 28);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.key_type = key_type;
    req.key_flags = key_flags;
    int ret = sceNet_lib_13A8B98A(NULL, 0xC01C69E1u, &req, 1);
    if (ret == 0)
        *out = req.out;
    return ret;
}

int sceNet_lib_AB375964(const char *ifname, const void *arg)
{
    struct { char name[16]; u8 key_material[32]; } req;
    sceKernelMemset(&req, 0, 48);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    sceKernelMemcpy(req.key_material, arg, 32);
    return sceNet_lib_13A8B98A(NULL, 0x803069E2u, &req, 1);
}

int sceNet_lib_8DA79EB0(const char *ifname, const void *arg)
{
    struct { char name[16]; u8 key_material[32]; } req;
    sceKernelMemset(&req, 0, 48);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    sceKernelMemcpy(req.key_material, arg, 32);
    return sceNet_lib_13A8B98A(NULL, 0x803069E3u, &req, 1);
}

int sceNet_lib_5216CBF5(const char *ifname)
{
    struct { char name[16]; u16 flags; u8 pad[14]; } req;
    sceKernelMemset(&req, 0, 32);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    int ret = sceNet_lib_13A8B98A(NULL, 0xC0206911u, &req, 1);
    if (ret == 0) {
        req.flags |= 1;
        return sceNet_lib_13A8B98A(NULL, 0x80206910u, &req, 1);
    }
    return ret;
}

int sceNet_lib_D2422E4D(const char *ifname)
{
    struct { char name[16]; u16 flags; u8 pad[14]; } req;
    sceKernelMemset(&req, 0, 32);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    int ret = sceNet_lib_13A8B98A(NULL, 0xC0206911u, &req, 1);
    if (ret == 0) {
        req.flags &= ~1u;
        return sceNet_lib_13A8B98A(NULL, 0x80206910u, &req, 1);
    }
    return ret;
}

int sceNet_lib_8D33C11D(const char *ifname, void *addr)
{
    NetIfnet *ifp = sceNet_lib_1D10419C(ifname);
    if (ifp == NULL)
        return SCE_NET_ERROR_NO_IF;
    NetSockaddrDl *sdl = ifp->if_sadl;
    sceKernelMemcpy(addr, sdl->sdl_data + sdl->sdl_nlen, 6);
    return 0;
}

int sceNetGetLocalEtherAddr(void *addr)
{
    if (_sceKernelCheckThreadStack() < 1008)
        return SCE_NET_ERROR_STACK_SMALL;
    NetIfnet *ifp = sceNet_lib_1D10419C("wlan");
    if (ifp == NULL)
        return SCE_NET_ERROR_NO_IF;
    NetSockaddrDl *sdl = ifp->if_sadl;
    sceKernelMemcpy(addr, sdl->sdl_data + sdl->sdl_nlen, 6);
    return 0;
}

int sceNet_lib_D1BE2CE9(const char *ifname, u32 *code_out, u32 *value_out)
{
    struct { char name[16]; void *ptr; u32 pad[3]; } req;
    u32 vals[2];
    sceKernelMemset(&req, 0, 32);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.ptr = vals;
    sceKernelMemset(vals, 0, 8);
    int ret = sceNet_lib_13A8B98A(NULL, 0xC020693Du, &req, 1);
    if (ret == 0) {
        *code_out = vals[0];
        *value_out = vals[1];
    }
    return ret;
}

int sceNet_lib_AB7DD9A5(const char *ifname, u32 ev_mask, u32 ev_arg)
{
    struct { char name[16]; void *ptr; u32 pad[3]; } req;
    u32 vals[2];
    sceKernelMemset(&req, 0, 32);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    vals[0] = ev_mask;
    vals[1] = ev_arg;
    req.ptr = vals;
    return sceNet_lib_13A8B98A(NULL, 0x8020693Eu, &req, 1);
}

int sceNet_lib_80E1933E(const char *ifname, u32 *mask_out, u32 *arg_out)
{
    struct { char name[16]; void *ptr; u32 pad[3]; } req;
    u32 vals[2];
    sceKernelMemset(&req, 0, 32);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.ptr = vals;
    sceKernelMemset(vals, 0, 8);
    int ret = sceNet_lib_13A8B98A(NULL, 0xC020693Eu, &req, 1);
    if (ret == 0) {
        *mask_out = vals[0];
        *arg_out = vals[1];
    }
    return ret;
}

int sceNet_lib_C1D2A7FB(const char *ifname, const void *mac)
{
    struct { char name[16]; u8 mac[6]; } req;
    sceKernelMemset(&req, 0, 22);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    sceKernelMemcpy(req.mac, mac, 6);
    return sceNet_lib_13A8B98A(NULL, 0x801669FAu, &req, 1);
}

int sceNet_lib_CFA79880(const char *ifname, const void *mac)
{
    struct { char name[16]; u8 mac[6]; } req;
    sceKernelMemset(&req, 0, 22);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    sceKernelMemcpy(req.mac, mac, 6);
    return sceNet_lib_13A8B98A(NULL, 0x801669FBu, &req, 1);
}

/* =========================================================================
 * sceNetInit / sceNetTerm
 * ========================================================================= */

int sceNetInit(int poolsize, int calloutprio, int calloutstack, int netintrprio, int netintrstack)
{
    SceUID partId;
    int ret;
    (void)calloutstack;
    (void)netintrstack;

    if (_sceKernelCheckThreadStack() < 976)
        return SCE_NET_ERROR_STACK_SMALL;

    net_prng_seed(sceKernelGetSystemTimeLow());
    ret = sceKernelQueryMemoryInfo(&g_mem_anchor, &partId, NULL);
    if (ret >= 0) {
        ret = net_ifhandle_init(partId, poolsize);
        if (ret >= 0) {
            sceNetIfhandle_lib_0FB8AE0D();
            ret = callout_init(calloutprio, partId, 4096);
            if (ret >= 0) {
                ret = spl_init();
                if (ret >= 0) {
                    ret = slpque_init();
                    if (ret >= 0) {
                        ret = threadlist_init();
                        if (ret >= 0) {
                            ret = netintr_init(netintrprio, partId, 4096);
                            if (ret >= 0) {
                                ifinit();
                                bpf_init(8);
                                sceWlanDrv_lib_8D5F551B(0x20000);
                                return 0;
                            }
                        }
                    }
                }
            }
        }
    }

    netintr_term();
    threadlist_term();
    slpque_term();
    spl_term();
    callout_term();
    net_ifhandle_term();
    sceWlanDrv_lib_8D5F551B(0x10000);
    return ret;
}

int sceNetTerm(void)
{
    if (_sceKernelCheckThreadStack() < 1008)
        return SCE_NET_ERROR_STACK_SMALL;
    netintr_term();
    threadlist_term();
    slpque_term();
    spl_term();
    callout_term();
    net_ifhandle_term();
    sceWlanDrv_lib_8D5F551B(0x10000);
    return 0;
}

/* =========================================================================
 * Netintr thread & protocol ISR registration
 * ========================================================================= */

static int netintr_thread(SceSize args, void *argp)
{
    u32 bits;
    void *ifp_raw;
    SceUID sema;
    (void)args;
    (void)argp;

    for (;;) {
        sceKernelWaitEventFlag(g_netintr_evf, 0xFFFFFFFFu, 17, &bits, NULL);
        int spl = sceNet_lib_94B44F26();

        if (bits & 1) {
            if (sceNetIfhandle_lib_B8188F96(&ifp_raw, &sema) == 0) {
                NetIfnet *ifp = (NetIfnet *)ifp_raw;
                ifp->if_start = ether_start;
                ifp->if_ioctl = ether_ioctl;
                ifp->if_flags = 0x8822;
                ifp->if_ev_tail = &ifp->if_ev_head;
                ifp->if_drv_up = sceNetIfhandle_lib_8FCB05A1;
                ifp->if_drv_down = sceNetIfhandle_lib_EAD3A759;
                ifp->if_drv_ioctl = sceNetIfhandle_lib_0296C7D6;
                ifp->if_ev_head = NULL;
                sceNet_lib_701DDDC3(ifp);
                ether_ifattach(ifp, ifp->if_hwaddr);
                bpf_attach(&ifp->if_bpf, ifp, 1, 14);
                sceKernelSignalSema(sema, 1);
            }
        }

        if (bits & 2) {
            if (sceNetIfhandle_lib_4FB43BCE(&ifp_raw, &sema) == 0) {
                NetIfnet *ifp = (NetIfnet *)ifp_raw;
                bpf_detach(ifp);
                ether_ifdetach(ifp);
                sceNet_lib_D5A03BC0(ifp);
                if (g_adhoc_updown_reg > 0)
                    g_adhoc_updown_cb(ifp);
                free_if_events_inline(ifp);
                sceKernelSignalSema(sema, 1);
            }
        }

        if (bits & 4) {
            for (;;) {
                NetMbuf *mbuf = (NetMbuf *)sceNetIfhandle_lib_E440A7D8();
                if (mbuf == NULL)
                    break;
                NetIfnet *ifp = mbuf->m_pkthdr_rcvif;
                if (ifp == NULL)
                    break;
                if (ifp->if_bpf != NULL)
                    bpf_mtap(ifp->if_bpf, mbuf);
                ifp->if_input(ifp, mbuf);
            }
        }

        if ((bits & 8) && g_adhoc_updown_reg > 0) {
            NetIfnet *ifp = g_ifnet_list.head;
            while (ifp != NULL) {
                g_adhoc_updown_cb(ifp);
                ifp = ifp->if_next;
            }
        }

        if (bits & 0x20) {
            if (sceNetIfhandle_lib_B40A882F(&ifp_raw, &sema) == 0) {
                NetIfnet *ifp = (NetIfnet *)ifp_raw;
                ifp->if_ev_tail = &ifp->if_ev_head;
                ifp->if_drv_up = sceNetIfhandle_lib_8FCB05A1;
                ifp->if_drv_down = sceNetIfhandle_lib_EAD3A759;
                ifp->if_drv_ioctl = sceNetIfhandle_lib_0296C7D6;
                ifp->if_ev_head = NULL;
                sceNet_lib_701DDDC3(ifp);
                ppp_attach(ifp);
                bpf_attach(&ifp->if_bpf, ifp, 0, 0);
                sceKernelSignalSema(sema, 1);
            }
        }

        if (bits & 0x40) {
            if (sceNetIfhandle_lib_9173FD47(&ifp_raw, &sema) == 0) {
                NetIfnet *ifp = (NetIfnet *)ifp_raw;
                bpf_detach(ifp);
                ppp_detach(ifp);
                sceNet_lib_D5A03BC0(ifp);
                free_if_events_inline(ifp);
                sceKernelSignalSema(sema, 1);
            }
        }

        u32 isr = g_netisr_bits;
        if (g_ip_hooks_reg > 0) {
            if (isr & 0x00000004u) {
                g_netisr_bits &= ~0x00000004u;
                g_ip_intr_fn();
            }
            if (isr & 0x10000000u) {
                g_netisr_bits &= ~0x10000000u;
                g_arp_intr_fn();
            }
        }
        if (g_adhoc_intr_reg > 0 && (isr & 0x40000000u)) {
            g_netisr_bits &= ~0x40000000u;
            g_adhoc_if_cb((void *)0x40000000u);
        }
        if (g_adhoc_updown_reg > 0 && (isr & 0x20000000u)) {
            g_netisr_bits &= ~0x20000000u;
            g_adhoc_intr_fn();
        }

        sceNet_lib_515B2F33(spl);
    }
    return 0;
}

static int netintr_init(int prio, SceUID partId, int stackSize)
{
    SceKernelThreadOptParam opt;
    int ret;

    g_netisr_bits = 0;
    ret = sceKernelCreateEventFlag("SceNetNetintr", 0, 0, NULL);
    if (ret >= 0) {
        g_netintr_evf = ret;
        opt.size = sizeof(opt);
        opt.stackMpid = partId;
        ret = sceKernelCreateThread("SceNetNetintr", netintr_thread, prio, stackSize, 0, &opt);
        if (ret >= 0) {
            g_netintr_thid = ret;
            ret = sceKernelStartThread(g_netintr_thid, 0, NULL);
            if (ret >= 0) {
                sceNetIfhandle_lib_30F69334(g_netintr_evf);
                return 0;
            }
        }
    }

    if (g_netintr_thid >= 0) {
        sceKernelTerminateThread(g_netintr_thid);
        sceKernelDeleteThread(g_netintr_thid);
    }
    g_netintr_thid = -1;
    if (g_netintr_evf >= 0)
        sceKernelDeleteEventFlag(g_netintr_evf);
    g_netintr_evf = -1;
    sceNetIfhandle_lib_B9096E48();
    return ret;
}

void sceNet_lib_7C3B86C5(int bit)
{
    g_netisr_bits |= (1u << bit);
}

int sceNet_lib_05D525E4(int bit)
{
    g_netisr_bits |= (1u << bit);
    return sceKernelSetEventFlag(g_netintr_evf, 16);
}

static int netintr_term(void)
{
    if (g_netintr_thid >= 0) {
        sceKernelTerminateThread(g_netintr_thid);
        sceKernelDeleteThread(g_netintr_thid);
    }
    g_netintr_thid = -1;
    if (g_netintr_evf >= 0)
        sceKernelDeleteEventFlag(g_netintr_evf);
    g_netintr_evf = -1;
    return sceNetIfhandle_lib_B9096E48();
}

int sceNet_lib_13672F83(void (*ip_fn)(void), void (*arp_fn)(void))
{
    g_ip_intr_fn = ip_fn;
    g_arp_intr_fn = arp_fn;
    g_ip_hooks_reg = 1;
    return 0;
}

int sceNet_lib_5C7C7381(void)
{
    g_ip_hooks_reg = 0;
    g_ip_intr_fn = NULL;
    g_arp_intr_fn = NULL;
    return 0;
}

int sceNet_lib_86B6DCD9(void (*fn)(void))
{
    g_unused_intr_fn = fn;
    g_unused_hook_reg = 1;
    return 0;
}

int sceNet_lib_7AE91FB4(void)
{
    g_unused_hook_reg = 0;
    g_unused_intr_fn = NULL;
    return 0;
}

int sceNet_lib_572AD6ED(void (*fn)(void *))
{
    g_adhoc_if_cb = fn;
    g_adhoc_intr_reg = 1;
    return 0;
}

int sceNet_lib_87DC7A7E(void)
{
    g_adhoc_intr_reg = 0;
    g_adhoc_if_cb = NULL;
    return 0;
}

int sceNet_lib_B3A48B7F(void (*intr_fn)(void), void (*updown_cb)(NetIfnet *))
{
    g_adhoc_intr_fn = intr_fn;
    g_adhoc_updown_cb = updown_cb;
    g_adhoc_updown_reg = 1;
    return 0;
}

int sceNet_lib_949F1FBB(void)
{
    g_adhoc_updown_reg = 0;
    g_adhoc_intr_fn = NULL;
    g_adhoc_updown_cb = NULL;
    return 0;
}

/* =========================================================================
 * Thread info list
 * ========================================================================= */

NetThreadInfo *sceNet_lib_CA3CF5EB(int *errp)
{
    SceUID thid = sceKernelGetThreadId();
    int ret;
    if (thid < 0) {
        *errp = thid;
        return NULL;
    }
    ret = sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    if (ret < 0) {
        *errp = ret;
        return NULL;
    }

    NetThreadInfo *ti = g_thread_head;
    while (ti != NULL && ti->thid != thid)
        ti = ti->list_next;

    if (ti == NULL) {
        ti = (NetThreadInfo *)sceNetIfhandle_lib_15CFE3C0(sizeof(NetThreadInfo));
        if (ti == NULL) {
            *errp = SCE_NET_ERROR_ENOMEM;
            sceKernelUnlockLwMutex(&g_thread_mtx, 1);
            return NULL;
        }
        sceKernelMemset(ti, 0, sizeof(NetThreadInfo));
        ti->list_next = NULL;
        ti->list_prev = g_thread_tail;
        ti->thid = thid;
        *g_thread_tail = ti;
        g_thread_tail = &ti->list_next;

        ti->sema_id = sceKernelCreateSema("SceNetThreadInfo", 0, 0, 1, NULL);
        if (ti->sema_id < 0) {
            ti->err = 46;
            *errp = ti->sema_id;
            sceKernelUnlockLwMutex(&g_thread_mtx, 1);
            return NULL;
        }
    }

    ti->err = 0;
    ti->refcount++;
    *errp = 0;
    sceKernelUnlockLwMutex(&g_thread_mtx, 1);
    return ti;
}

int sceNetFreeThreadinfo(SceUID thid)
{
    int ret = 0;
    if (_sceKernelCheckThreadStack() < 992)
        return SCE_NET_ERROR_STACK_SMALL;

    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    NetThreadInfo *ti = g_thread_head;
    while (ti != NULL && ti->thid != thid)
        ti = ti->list_next;

    if (ti == NULL) {
        ret = SCE_NET_ERROR_ENOENT;
    } else if (ti->refcount > 0) {
        ret = SCE_NET_ERROR_EBUSY;
    } else {
        if (ti->list_next != NULL)
            ti->list_next->list_prev = ti->list_prev;
        else
            g_thread_tail = ti->list_prev;
        *ti->list_prev = ti->list_next;
        sceNet_lib_DCBC596E(&ti->callout);
        sceKernelDeleteSema(ti->sema_id);
        sceNetIfhandle_lib_76BAD213(ti);
    }

    sceKernelUnlockLwMutex(&g_thread_mtx, 1);
    return ret;
}

static int threadlist_init(void)
{
    g_thread_tail = &g_thread_head;
    g_thread_head = NULL;
    int ret = sceKernelCreateLwMutex(&g_thread_mtx, "SceNetThreadList", 0, 0, NULL);
    if (ret >= 0) {
        g_threadlist_init = 0;
        return 0;
    }
    return ret;
}

NetThreadInfo *sceNet_lib_BB2B3DDB(SceUID thid)
{
    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    NetThreadInfo *ti = g_thread_head;
    while (ti != NULL && ti->thid != thid)
        ti = ti->list_next;
    sceKernelUnlockLwMutex(&g_thread_mtx, 1);
    return ti;
}

NetThreadInfo *sceNet_lib_6D5D42D7(SceUID thid)
{
    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    NetThreadInfo *ti = g_thread_head;
    while (ti != NULL && ti->thid != thid)
        ti = ti->list_next;
    if (ti != NULL)
        ti->refcount++;
    sceKernelUnlockLwMutex(&g_thread_mtx, 1);
    return ti;
}

int sceNet_lib_C21E18B2(NetThreadInfo *ti)
{
    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    ti->refcount--;
    return sceKernelUnlockLwMutex(&g_thread_mtx, 1);
}

int sceNet_lib_757085B0(NetThreadInfo *ti, int err)
{
    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    ti->err = err;
    ti->refcount--;
    sceKernelUnlockLwMutex(&g_thread_mtx, 1);
    return (err == 0) ? 0 : -1;
}

int sceNet_lib_435843CB(void)
{
    SceUID thid = sceKernelGetThreadId();
    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    NetThreadInfo *ti = g_thread_head;
    while (ti != NULL && ti->thid != thid)
        ti = ti->list_next;
    sceKernelUnlockLwMutex(&g_thread_mtx, 1);
    if (ti == NULL)
        return SCE_NET_ERROR_ENOMEM;
    return ti->err;
}

int sceNet_lib_D861EF33(int err)
{
    SceUID thid = sceKernelGetThreadId();
    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    NetThreadInfo *ti = g_thread_head;
    while (ti != NULL && ti->thid != thid)
        ti = ti->list_next;
    sceKernelUnlockLwMutex(&g_thread_mtx, 1);
    if (ti == NULL)
        return SCE_NET_ERROR_ENOMEM;
    ti->err = err;
    return 0;
}

int sceNet_lib_45452B7B(void)
{
    int busy = 0;
    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    NetThreadInfo *ti = g_thread_head;
    while (ti != NULL) {
        if (ti->refcount > 0)
            busy++;
        ti = ti->list_next;
    }
    sceKernelUnlockLwMutex(&g_thread_mtx, 1);
    return (busy == 0) ? 0 : SCE_NET_ERROR_EBUSY;
}

static int threadlist_term(void)
{
    int ret = 0;
    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    if (g_threadlist_init >= 0) {
        NetThreadInfo *ti = g_thread_head;
        while (ti != NULL) {
            sceKernelDeleteSema(ti->sema_id);
            ti = ti->list_next;
        }
        ret = sceKernelDeleteLwMutex(&g_thread_mtx);
    }
    g_threadlist_init = -1;
    return ret;
}

int sceNetThreadAbort(SceUID thid)
{
    int err = 0;
    int ret = 0;
    if (_sceKernelCheckThreadStack() < 976)
        return SCE_NET_ERROR_STACK_SMALL;

    NetThreadInfo *self = sceNet_lib_CA3CF5EB(&ret);
    if (self == NULL)
        return ret;

    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    NetThreadInfo *target = g_thread_head;
    while (target != NULL && target->thid != thid)
        target = target->list_next;
    if (target != NULL)
        target->refcount++;
    sceKernelUnlockLwMutex(&g_thread_mtx, 1);

    if (target != NULL) {
        sceNet_lib_C9C97945(target);
        sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
        target->refcount--;
        sceKernelUnlockLwMutex(&g_thread_mtx, 1);
    } else {
        err = 2;
        ret = SCE_NET_ERROR_ENOENT;
    }

    sceKernelLockLwMutex(&g_thread_mtx, 1, NULL);
    self->err = err;
    self->refcount--;
    sceKernelUnlockLwMutex(&g_thread_mtx, 1);
    return ret;
}

/* =========================================================================
 * Callout subsystem
 * ========================================================================= */

static u32 callout_alarm_cb(void *arg)
{
    NetCallout *co = (NetCallout *)arg;
    co->ready_next = NULL;
    co->ready_prev = g_callout_ready_tail;
    *g_callout_ready_tail = co;
    g_callout_ready_tail = &co->ready_next;

    if (co->pend_next != NULL)
        co->pend_next->pend_prev = co->pend_prev;
    else
        g_callout_pend_tail = co->pend_prev;
    *co->pend_prev = co->pend_next;
    co->pend_next = NULL;
    co->pend_prev = NULL;

    sceKernelSignalSema(g_callout_sema, 1);
    return 0;
}

static int callout_thread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    for (;;) {
        sceKernelWaitSema(g_callout_sema, 1, NULL);
        int spl = sceNet_lib_94B44F26();
        for (;;) {
            int intr = sceKernelCpuSuspendIntr();
            NetCallout *co = g_callout_ready_head;
            if (co == NULL) {
                sceKernelCpuResumeIntr(intr);
                break;
            }
            if (co->ready_next != NULL)
                co->ready_next->ready_prev = co->ready_prev;
            else
                g_callout_ready_tail = co->ready_prev;
            *co->ready_prev = co->ready_next;
            void (*fn)(void *) = co->func;
            void *arg = co->arg;
            co->flags &= ~2u;
            co->ready_next = NULL;
            co->ready_prev = NULL;
            co->func = NULL;
            sceKernelCpuResumeIntr(intr);
            fn(arg);
        }
        sceNet_lib_515B2F33(spl);
    }
    return 0;
}

static int callout_init(int prio, SceUID partId, int stackSize)
{
    SceKernelThreadOptParam opt;
    g_callout_ready_tail = &g_callout_ready_head;
    g_callout_pend_tail = &g_callout_pend_head;
    g_callout_ready_head = NULL;
    g_callout_pend_head = NULL;

    g_callout_sema = sceKernelCreateSema("SceNetCalloutQueue", 0, 0, 1, NULL);
    if (g_callout_sema < 0)
        return g_callout_sema;

    opt.size = sizeof(opt);
    opt.stackMpid = partId;
    g_callout_thid = sceKernelCreateThread("SceNetCallout", callout_thread, prio, stackSize, 0, &opt);
    if (g_callout_thid < 0)
        return g_callout_thid;

    int ret = sceKernelStartThread(g_callout_thid, 0, NULL);
    return (ret < 0) ? ret : 0;
}

static int callout_term(void)
{
    int intr = sceKernelCpuSuspendIntr();
    while (g_callout_pend_head != NULL) {
        NetCallout *co = g_callout_pend_head;
        if (co->pend_next != NULL)
            co->pend_next->pend_prev = co->pend_prev;
        else
            g_callout_pend_tail = co->pend_prev;
        *co->pend_prev = co->pend_next;
        co->pend_next = NULL;
        co->pend_prev = NULL;
        sceKernelCancelAlarm(co->alarm_id);
    }
    sceKernelCpuResumeIntr(intr);

    if (g_callout_thid >= 0) {
        sceKernelTerminateThread(g_callout_thid);
        sceKernelDeleteThread(g_callout_thid);
    }
    g_callout_thid = -1;
    if (g_callout_sema >= 0)
        sceKernelDeleteSema(g_callout_sema);
    g_callout_sema = -1;
    return 0;
}

void sceNet_lib_A8B6205A(NetCallout *co, u32 usec, void (*fn)(void *), void *arg)
{
    if (usec == 0)
        usec = 1;
    int intr = sceKernelCpuSuspendIntr();
    if (co->flags & 2)
        sceNet_lib_A93A93E9(co);
    co->flags = 3;
    co->arg = arg;
    co->func = fn;
    co->timeout_us = usec;
    co->ready_next = NULL;
    co->ready_prev = NULL;
    co->alarm_id = sceKernelSetAlarm(usec, callout_alarm_cb, co);
    co->pend_prev = g_callout_pend_tail;
    co->pend_next = NULL;
    NetCallout **prev_tail = g_callout_pend_tail;
    g_callout_pend_tail = &co->pend_next;
    *prev_tail = co;
    sceKernelCpuResumeIntr(intr);
}

int sceNet_lib_A93A93E9(NetCallout *co)
{
    int intr = sceKernelCpuSuspendIntr();
    if ((co->flags & 2) == 0) {
        co->flags &= ~1u;
        sceKernelCpuResumeIntr(intr);
        return 0;
    }

    NetCallout *cur = g_callout_pend_head;
    while (cur != NULL && cur != co)
        cur = cur->pend_next;

    if (cur != NULL) {
        if (sceKernelCancelAlarm(cur->alarm_id) < 0) {
            sceKernelCpuResumeIntr(intr);
            return 0;
        }
        cur->alarm_id = 0;
        if (cur->pend_next != NULL)
            cur->pend_next->pend_prev = cur->pend_prev;
        else
            g_callout_pend_tail = cur->pend_prev;
        *cur->pend_prev = cur->pend_next;
        cur->pend_next = NULL;
        cur->pend_prev = NULL;
        co->flags &= ~3u;
        co->func = NULL;
        co->ready_next = NULL;
        co->ready_prev = NULL;
        sceKernelCpuResumeIntr(intr);
        return 0;
    }

    cur = g_callout_ready_head;
    while (cur != NULL && cur != co)
        cur = cur->ready_next;
    if (cur != NULL) {
        if (cur->ready_next != NULL)
            cur->ready_next->ready_prev = cur->ready_prev;
        else
            g_callout_ready_tail = cur->ready_prev;
        *cur->ready_prev = cur->ready_next;
        co->flags &= ~3u;
        co->func = NULL;
        co->ready_next = NULL;
        co->ready_prev = NULL;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

void sceNet_lib_25CC373A(NetCallout *co)
{
    sceKernelMemset(co, 0, sizeof(NetCallout));
}

void sceNet_lib_DCBC596E(NetCallout *co)
{
    (void)co;
}

void sceNet_lib_7C86FBA4(NetCallout *co, u32 ticks, void (*fn)(void *), void *arg)
{
    if (ticks == 0)
        ticks = 1;
    if (ticks <= 0x68DB8u)
        sceNet_lib_A8B6205A(co, ticks * 10000u, fn, arg);
}

/* =========================================================================
 * Sleep / Wakeup queue & SPL
 * ========================================================================= */

static void slp_timeout_cb(void *arg)
{
    NetThreadInfo *ti = (NetThreadInfo *)arg;
    sceKernelLockLwMutex(&g_slpque_mtx, 1, NULL);
    if (ti->wchan != 0) {
        ti->wchan = 0;
        ti->slp_flags |= 1;
        sceKernelSignalSema(ti->sema_id, 1);
    }
    sceKernelUnlockLwMutex(&g_slpque_mtx, 1);
}

int sceNet_lib_F6DB0A0B(u32 wchan, u32 usec)
{
    int ret = 0;
    SceUID thid = sceKernelGetThreadId();
    NetThreadInfo *ti = sceNet_lib_BB2B3DDB(thid);

    sceKernelLockLwMutex(&g_slpque_mtx, 1, NULL);
    if (g_adhoc_slp_reg > 0)
        g_slp_pre_cb(ti);

    if (ti->slp_flags & 2) {
        ti->slp_flags &= ~2u;
        ret = 4;
    } else {
        SleepBucket *bucket = &g_slpque[(wchan >> 8) & 0x7F];
        ti->slp_next = NULL;
        ti->slp_prev = bucket->tail;
        ti->wchan = wchan;
        *bucket->tail = ti;
        bucket->tail = &ti->slp_next;
        sceKernelUnlockLwMutex(&g_slpque_mtx, 1);

        if (usec != 0)
            sceNet_lib_A8B6205A(&ti->callout, usec, slp_timeout_cb, ti);

        int had_spl = (sceNet_lib_515B2F33(1) >= 0);
        sceKernelWaitSema(ti->sema_id, 1, NULL);
        if (had_spl)
            sceNet_lib_94B44F26();
        if (usec != 0)
            sceNet_lib_A93A93E9(&ti->callout);

        sceKernelLockLwMutex(&g_slpque_mtx, 1, NULL);
        if (ti->slp_next != NULL)
            ti->slp_next->slp_prev = ti->slp_prev;
        else
            bucket->tail = ti->slp_prev;
        *ti->slp_prev = ti->slp_next;
        ti->slp_next = NULL;
        ti->slp_prev = NULL;

        if (ti->slp_flags & 1) {
            ti->slp_flags &= ~1u;
            ret = 11;
        } else if (ti->slp_flags & 2) {
            ti->slp_flags &= ~2u;
            ret = 4;
        }
    }

    sceKernelUnlockLwMutex(&g_slpque_mtx, 1);
    return ret;
}

static int slpque_init(void)
{
    int idx;
    for (idx = 0; idx < 128; idx++) {
        g_slpque[idx].head = NULL;
        g_slpque[idx].tail = &g_slpque[idx].head;
    }
    g_slpque_init = sceKernelCreateLwMutex(&g_slpque_mtx, "SceNetSlpque", 0, 0, NULL);
    return (g_slpque_init < 0) ? g_slpque_init : 0;
}

static int slpque_term(void)
{
    int ret = 0;
    if (g_slpque_init >= 0)
        ret = sceKernelDeleteLwMutex(&g_slpque_mtx);
    g_slpque_init = -1;
    return ret;
}

int sceNet_lib_205E8D17(u32 wchan, u32 ticks)
{
    if (ticks > 0x68DB8u)
        return 22;
    return sceNet_lib_F6DB0A0B(wchan, ticks * 10000u);
}

int sceNet_lib_7574FDA1(u32 wchan)
{
    sceKernelLockLwMutex(&g_slpque_mtx, 1, NULL);
    NetThreadInfo *ti = g_slpque[(wchan >> 8) & 0x7F].head;
    while (ti != NULL) {
        if (ti->wchan == wchan) {
            ti->wchan = 0;
            sceKernelSignalSema(ti->sema_id, 1);
        }
        ti = ti->slp_next;
    }
    return sceKernelUnlockLwMutex(&g_slpque_mtx, 1);
}

int sceNet_lib_B8C4A858(SceUID thid, u32 wchan)
{
    sceKernelLockLwMutex(&g_slpque_mtx, 1, NULL);
    NetThreadInfo *ti = g_slpque[(wchan >> 8) & 0x7F].head;
    while (ti != NULL) {
        if ((thid < 0 || ti->thid == thid) && ti->wchan == wchan) {
            ti->wchan = 0;
            sceKernelSignalSema(ti->sema_id, 1);
        }
        ti = ti->slp_next;
    }
    return sceKernelUnlockLwMutex(&g_slpque_mtx, 1);
}

int sceNet_lib_C9C97945(NetThreadInfo *ti)
{
    sceKernelLockLwMutex(&g_slpque_mtx, 1, NULL);
    ti->slp_flags |= 2;
    if (ti->wchan != 0) {
        ti->wchan = 0;
        sceKernelSignalSema(ti->sema_id, 1);
    }
    return sceKernelUnlockLwMutex(&g_slpque_mtx, 1);
}

int sceNet_lib_6B294EE4(int event_id, int event_arg)
{
    sceKernelLockLwMutex(&g_slpque_mtx, 1, NULL);
    if (g_slp_notify_reg > 0)
        g_slp_notify_cb(event_id, event_arg);
    return sceKernelUnlockLwMutex(&g_slpque_mtx, 1);
}

int sceNet_lib_51C209B2(int event_id)
{
    sceKernelLockLwMutex(&g_slpque_mtx, 1, NULL);
    if (g_adhoc_slp_reg > 0)
        g_slp_post_cb(event_id);
    return sceKernelUnlockLwMutex(&g_slpque_mtx, 1);
}

int sceNet_lib_2E005032(void (*cb)(int, int))
{
    g_slp_notify_cb = cb;
    g_slp_notify_reg = 1;
    return 0;
}

int sceNet_lib_33B230BD(void)
{
    g_slp_notify_reg = 0;
    g_slp_notify_cb = NULL;
    return 0;
}

int sceNet_lib_E155112D(void (*pre_cb)(NetThreadInfo *), void (*post_cb)(int))
{
    g_slp_pre_cb = pre_cb;
    g_slp_post_cb = post_cb;
    g_adhoc_slp_reg = 1;
    return 0;
}

int sceNet_lib_41621EB0(void)
{
    g_adhoc_slp_reg = 0;
    g_slp_pre_cb = NULL;
    g_slp_post_cb = NULL;
    return 0;
}

static int spl_init(void)
{
    g_spl_init = sceKernelCreateLwMutex(&g_spl_mtx, "SceNetSpl", 0x200, 0, NULL);
    return (g_spl_init < 0) ? g_spl_init : 0;
}

static int spl_term(void)
{
    int ret = 0;
    if (g_spl_init >= 0)
        ret = sceKernelDeleteLwMutex(&g_spl_mtx);
    g_spl_init = -1;
    return ret;
}

int sceNet_lib_94B44F26(void)
{
    u32 ret = (u32)sceKernelLockLwMutex(&g_spl_mtx, 1, NULL);
    if (((ret >> 31) & (ret != 0x800201CDu)) != 0)
        return (int)ret;
    return 1;
}

int sceNet_lib_515B2F33(int level)
{
    if (level > 0)
        return sceKernelUnlockLwMutex(&g_spl_mtx, 1);
    return level;
}

u64 sceNet_lib_4B2B3416(NetTimeval *tv)
{
    u64 now = (u64)sceKernelGetSystemTimeWide();
    tv->tv_sec = (u32)(now / 1000000ULL);
    u64 rem = now % 1000000ULL;
    tv->tv_usec = (u32)rem;
    return rem;
}

int sceNet_lib_2B42872F(void *cp, int len, NetUio *uio)
{
    u8 *cursor = (u8 *)cp;
    while (len > 0 && uio->uio_resid != 0) {
        NetIovec *iov = uio->uio_iov;
        u32 cnt = iov->iov_len;
        if (cnt == 0) {
            uio->uio_iov++;
            uio->uio_iovcnt--;
            continue;
        }
        if ((u32)len < cnt)
            cnt = (u32)len;
        if (uio->uio_rw != 0)
            sceNet_lib_2F305274(iov->iov_base, cursor, (int)cnt);
        else
            sceNet_lib_2F305274(cursor, iov->iov_base, (int)cnt);
        iov->iov_base = (u8 *)iov->iov_base + cnt;
        iov->iov_len -= cnt;
        uio->uio_resid -= cnt;
        uio->uio_offset += cnt;
        cursor += cnt;
        len -= (int)cnt;
    }
    return 0;
}

void *sceNet_lib_C4261339(int elements, u32 *hashmask)
{
    int sz = 1;
    while (sz < elements)
        sz <<= 1;
    u32 *tbl = (u32 *)sceNetIfhandle_lib_15CFE3C0((u32)(sz * 4));
    if (tbl == NULL)
        return NULL;
    int idx;
    for (idx = 0; idx < sz; idx++)
        tbl[idx] = 0;
    *hashmask = (u32)(sz - 1);
    return tbl;
}

void sceNet_lib_41FD8B5C(void *tbl)
{
    sceNetIfhandle_lib_76BAD213(tbl);
}

void sceNet_lib_92633D8D(NetThreadInfo *ti, SceUID *sel_thid)
{
    *sel_thid = ti->thid;
}

void sceNet_lib_B9C780C7(SceUID *sel_thid)
{
    SceUID thid = *sel_thid;
    if (thid != 0) {
        sceNet_lib_B8C4A858(thid, (u32)&g_selwait);
        *sel_thid = 0;
    }
}

void *sceNet_lib_B68E1EEA(void)
{
    return &g_selwait;
}

/* =========================================================================
 * Interface management (ifnet)
 * ========================================================================= */

int sceNet_lib_701DDDC3(NetIfnet *ifp)
{
    ifp->if_addrhead = NULL;
    ifp->if_addrtail = &ifp->if_addrhead;

    int idx = 1;
    while (idx < 8) {
        NetIfnet *cur = g_ifnet_list.head;
        int used = 0;
        while (cur != NULL) {
            if (cur->if_index == idx) {
                used = 1;
                break;
            }
            cur = cur->if_next;
        }
        if (!used)
            break;
        idx++;
    }

    ifp->if_index = (u16)idx;
    ifp->if_next = NULL;
    ifp->if_prev = g_ifnet_list.tail;
    *g_ifnet_list.tail = ifp;
    g_ifnet_list.tail = &ifp->if_next;
    g_ifindex2ifnet[idx] = ifp;
    if (idx > g_if_index)
        g_if_index = idx;

    int namelen = sceNet_lib_D8722983(ifp->if_xname);
    u32 socksize = (u32)(namelen + 8 + ifp->if_data.ifi_addrlen);
    if (socksize < 20)
        socksize = 20;
    socksize = ((socksize - 1) | 3) + 1;

    u8 *mem = (u8 *)sceNetIfhandle_lib_15CFE3C0(sizeof(NetIfaddr) + 2 * socksize);
    sceNet_lib_8B615F52(mem, (int)(sizeof(NetIfaddr) + 2 * socksize));

    NetIfaddr *ifa = (NetIfaddr *)mem;
    NetSockaddrDl *sdl = (NetSockaddrDl *)(mem + sizeof(NetIfaddr));
    sdl->sdl_len = (u8)socksize;
    sdl->sdl_family = 18;
    sdl->sdl_index = ifp->if_index;
    sdl->sdl_type = ifp->if_data.ifi_type;
    sdl->sdl_nlen = (u8)namelen;
    sceNet_lib_2F305274(ifp->if_xname, sdl->sdl_data, namelen);

    g_ifnet_addrs[ifp->if_index] = ifa;
    ifa->ifa_refcnt = 2;
    ifa->ifa_rtrequest = sceNet_lib_EB6DE71A;
    ifa->ifa_ifp = ifp;
    ifa->ifa_addr = (NetSockaddr *)sdl;
    ifp->if_sadl = sdl;

    NetSockaddrDl *mask = (NetSockaddrDl *)(mem + sizeof(NetIfaddr) + socksize);
    ifa->ifa_netmask = (NetSockaddr *)mask;
    mask->sdl_len = (u8)(namelen + 8);
    {
        int pos;
        for (pos = 0; pos < namelen; pos++)
            mask->sdl_data[pos] = 0xFF;
    }

    /* Insert at head of if_addrlist */
    NetIfaddr *old_head = ifp->if_addrhead;
    ifa->ifa_next = old_head;
    if (old_head != NULL)
        old_head->ifa_prev = &ifa->ifa_next;
    else
        ifp->if_addrtail = &ifa->ifa_next;
    ifa->ifa_prev = &ifp->if_addrhead;
    ifp->if_addrhead = ifa;

    if (ifp->if_snd.ifq_maxlen == 0)
        ifp->if_snd.ifq_maxlen = (s32)g_ifq_maxlen;
    ifp->if_broadcastaddr = NULL;
    ifp->if_data.ifi_recvquota = 0;

    if (g_if_hooks_reg > 0)
        g_if_attach_cb(ifp);
    return 0;
}

static void ifq_purge(NetIfQueue *ifq)
{
    NetMbuf *mbuf = ifq->ifq_head;
    while (mbuf != NULL) {
        NetMbuf *next = mbuf->m_nextpkt;
        sceNetIfhandle_lib_F56FAC82(mbuf);
        mbuf = next;
    }
    ifq->ifq_head = NULL;
    ifq->ifq_tail = NULL;
    ifq->ifq_len = 0;
}

void sceNet_lib_EDB11CB4(NetIfnet *ifp)
{
    ifp->if_flags &= ~1u;
    if (g_if_hooks_reg > 0)
        g_if_down_pre_cb();
    ifq_purge(&ifp->if_snd);
    if (g_if_hooks_reg > 0)
        g_if_down_post_cb(ifp);
}

int sceNet_lib_D5A03BC0(NetIfnet *ifp)
{
    int spl = sceNet_lib_94B44F26();
    sceNet_lib_EDB11CB4(ifp);

    while (ifp->if_addrhead != NULL) {
        NetIfaddr *ifa = ifp->if_addrhead;
        if (ifa->ifa_addr->sa_family == 18) {
            if (g_if_hooks_reg > 0)
                g_if_detach_addr_cb(ifa);
            NetIfaddr *next = ifa->ifa_next;
            if (next != NULL)
                next->ifa_prev = ifa->ifa_prev;
            else
                ifp->if_addrtail = ifa->ifa_prev;
            *ifa->ifa_prev = next;
            if (--ifa->ifa_refcnt == 0)
                sceNetIfhandle_lib_76BAD213(ifa);
        } else {
            if (g_if_hooks_reg > 0)
                g_if_purge_cb(ifp, ifa->ifa_addr->sa_family);
        }
    }

    if (g_if_hooks_reg > 0)
        g_if_detach_cb(ifp);

    u16 idx = ifp->if_index;
    NetIfaddr *ifa = g_ifnet_addrs[idx];
    if (--ifa->ifa_refcnt == 0)
        sceNetIfhandle_lib_76BAD213(ifa);
    g_ifnet_addrs[idx] = NULL;

    if (g_if_hooks_reg > 0)
        g_if_detach_post_cb(ifp, 0x10000u);

    NetIfnet *next_if = ifp->if_next;
    if (next_if != NULL)
        next_if->if_prev = ifp->if_prev;
    else
        g_ifnet_list.tail = ifp->if_prev;
    *ifp->if_prev = next_if;

    return sceNet_lib_515B2F33(spl);
}

static void if_slowtimo(void *arg)
{
    (void)arg;
    if_slowtimo_body_inline();
}

static void ifinit(void)
{
    g_if_index = 0;
    if (g_if_hooks_reg > 0)
        g_if_init_cb();
    g_ifnet_list.tail = &g_ifnet_list.head;
    g_ifnet_list.head = NULL;
    sceKernelMemset(g_ifindex2ifnet_tbl, 0, sizeof(g_ifindex2ifnet_tbl));
    g_ifindex2ifnet = g_ifindex2ifnet_tbl;
    sceKernelMemset(g_ifnet_addrs_tbl, 0, sizeof(g_ifnet_addrs_tbl));
    g_ifnet_addrs = g_ifnet_addrs_tbl;
    if (g_if_extra_init_reg > 0)
        g_if_extra_init_cb();
    sceNet_lib_25CC373A(&g_if_slowtimo_callout);
    if_slowtimo_body_inline();
}

int sceNet_lib_8C55B410(void)
{
    return 6;
}

void sceNet_lib_EDCC871E(void *ptr)
{
    sceNetIfhandle_lib_76BAD213(ptr);
}

void sceNet_lib_EB6DE71A(int req, int rt, int rtinfo)
{
    if (g_if_hooks_reg > 0)
        g_if_rtrequest_cb(req, rt, rtinfo);
}

void sceNet_lib_FA6DE6A6(NetIfnet *ifp)
{
    ifp->if_flags |= 1;
    if (g_if_hooks_reg > 0)
        g_if_up_cb(ifp);
}

NetIfnet *sceNet_lib_1D10419C(const char *name)
{
    return ifunit_inline(name);
}

int sceNet_lib_522A971B(const char *name)
{
    return (ifunit_inline(name) != NULL) ? 1 : 0;
}

int sceNet_lib_13A8B98A(void *so, u32 cmd, void *req_ptr, int wait)
{
    NetIfnet *ifp = ifunit_inline((const char *)req_ptr);
    int ret = 0;
    if (ifp == NULL)
        return SCE_NET_ERROR_NO_IF;

    if (wait <= 0) {
        if (sceKernelPollSema(ifp->if_ioctl_lock_sema, 1) < 0)
            return SCE_NET_ERROR_IF_BUSY;
    } else {
        sceKernelWaitSema(ifp->if_ioctl_lock_sema, 1, NULL);
    }

    ifp->if_ioctl_result = 0;
    int ierr = ifioctl(so, cmd, req_ptr);
    if (ierr != 0) {
        if (ierr == 125)
            ret = SCE_NET_ERROR_IOCTL_CANCELED;
        else if (ierr == 2)
            ret = SCE_NET_ERROR_IOCTL_ENOENT;
        else
            ret = SCE_NET_ERROR_IOCTL_ERR;
    } else if (wait > 0) {
        while (ifp->if_ioctl_pending > 0) {
            sceKernelWaitSema(ifp->if_ioctl_done_sema, 1, NULL);
            ifp->if_ioctl_pending--;
        }
        ret = ifp->if_ioctl_result;
    }

    if (wait > 0 || ierr != 0)
        sceKernelSignalSema(ifp->if_ioctl_lock_sema, 1);
    return ret;
}

int sceNet_lib_EA42B353(const char *ifname, u32 *out)
{
    if (out == NULL)
        return SCE_NET_ERROR_EINVAL;
    NetIfnet *ifp = ifunit_inline(ifname);
    if (ifp == NULL)
        return SCE_NET_ERROR_NO_IF;

    while (ifp->if_ioctl_pending > 0) {
        if (sceKernelPollSema(ifp->if_ioctl_done_sema, 1) < 0)
            return SCE_NET_ERROR_IF_BUSY;
        ifp->if_ioctl_pending--;
    }
    *out = (u32)ifp->if_ioctl_result;
    sceKernelSignalSema(ifp->if_ioctl_lock_sema, 1);
    return 0;
}

int sceNet_lib_E9D457B6(NetIfnet *ifp, u32 cmd, void *req_ptr)
{
    int (*ioctl_fn)(NetIfnet *, u32, void *) = ifp->if_ioctl;
    if (ioctl_fn == NULL)
        return -1;
    while (ifp->if_ioctl_pending > 0) {
        sceKernelWaitSema(ifp->if_ioctl_done_sema, 1, NULL);
        ifp->if_ioctl_pending--;
    }
    ifp->if_ioctl_result = 0;
    if (ioctl_fn(ifp, cmd, req_ptr) != 0)
        return -1;
    sceKernelWaitSema(ifp->if_ioctl_done_sema, 1, NULL);
    int res = ifp->if_ioctl_result;
    ifp->if_ioctl_pending--;
    return res;
}

int sceNet_lib_710BD467(const char *name)
{
    int idx = -1;
    int spl = sceNet_lib_94B44F26();
    NetIfnet *ifp = g_ifnet_list.head;
    while (ifp != NULL) {
        if (sceNet_lib_94DCA9F0(name, ifp->if_xname, 16) == 0) {
            idx = ifp->if_index;
            break;
        }
        ifp = ifp->if_next;
    }
    sceNet_lib_515B2F33(spl);
    return idx;
}

NetIfnet *sceNet_lib_45945E8D(int idx)
{
    int spl = sceNet_lib_94B44F26();
    NetIfnet *ifp = g_ifnet_list.head;
    while (ifp != NULL) {
        if (ifp->if_index == idx)
            break;
        ifp = ifp->if_next;
    }
    sceNet_lib_515B2F33(spl);
    return ifp;
}

int sceNet_lib_C2EC2EEA(s32 *index_out, NetIfnet ***ifindex_table_out, NetIfaddr ***addrs_out, void *head_out)
{
    if (index_out != NULL)
        *index_out = g_if_index;
    if (ifindex_table_out != NULL)
        *ifindex_table_out = g_ifindex2ifnet;
    if (addrs_out != NULL)
        *addrs_out = g_ifnet_addrs;
    if (head_out != NULL)
        sceKernelMemcpy(head_out, &g_ifnet_list, sizeof(NetIfnetList));
    return 0;
}

int sceNet_lib_976AB1E9(void (*rtreq)(int, int, int), void (*init_cb)(void),
                        void (*attach_cb)(NetIfnet *), void (*detach_addr)(NetIfaddr *),
                        void (*purge_cb)(NetIfnet *, int), void (*detach_cb)(NetIfnet *),
                        void (*detach_post)(NetIfnet *, u32), void (*down_pre)(void),
                        void (*down_post)(NetIfnet *), void (*up_cb)(NetIfnet *),
                        int (*ioctl_pre)(u32, void *, void *, int *),
                        int (*ioctl_so)(void *, int *),
                        int (*ioctl_post)(void *, u32, void *, NetIfnet *))
{
    g_if_rtrequest_cb = rtreq;
    g_if_init_cb = init_cb;
    g_if_attach_cb = attach_cb;
    g_if_detach_addr_cb = detach_addr;
    g_if_purge_cb = purge_cb;
    g_if_detach_cb = detach_cb;
    g_if_detach_post_cb = detach_post;
    g_if_down_pre_cb = down_pre;
    g_if_down_post_cb = down_post;
    g_if_up_cb = up_cb;
    g_if_ioctl_pre_cb = ioctl_pre;
    g_if_ioctl_so_cb = ioctl_so;
    g_if_ioctl_post_cb = ioctl_post;

    NetIfnet *ifp = g_ifnet_list.head;
    while (ifp != NULL) {
        attach_cb(ifp);
        if (ifp->if_flags & 1)
            up_cb(ifp);
        ifp = ifp->if_next;
    }
    g_if_hooks_reg = 1;
    return 0;
}

int sceNet_lib_4C8FD452(void)
{
    NetIfnet *ifp = g_ifnet_list.head;
    g_if_hooks_reg = 0;
    while (ifp != NULL) {
        int spl = sceNet_lib_94B44F26();
        sceNet_lib_EDB11CB4(ifp);
        NetIfaddr *ifa = ifp->if_addrhead;
        while (ifa != NULL) {
            u8 family = ifa->ifa_addr->sa_family;
            if (family == 18) {
                g_if_detach_addr_cb(ifa);
                ifa = ifa->ifa_next;
            } else {
                g_if_purge_cb(ifp, family);
                ifa = ifp->if_addrhead;
            }
        }
        g_if_detach_cb(ifp);
        g_if_detach_post_cb(ifp, 0);
        sceNet_lib_515B2F33(spl);
        ifp = ifp->if_next;
    }

    g_if_rtrequest_cb = NULL;
    g_if_init_cb = NULL;
    g_if_detach_addr_cb = NULL;
    g_if_purge_cb = NULL;
    g_if_detach_cb = NULL;
    g_if_detach_post_cb = NULL;
    g_if_attach_cb = NULL;
    g_if_down_pre_cb = NULL;
    g_if_down_post_cb = NULL;
    g_if_up_cb = NULL;
    g_if_ioctl_pre_cb = NULL;
    g_if_ioctl_so_cb = NULL;
    g_if_ioctl_post_cb = NULL;
    return 0;
}

static int ifioctl(void *so, u32 cmd, void *req_ptr)
{
    NetIfreq *ifr = (NetIfreq *)req_ptr;
    NetIfnet *ifp = ifunit_inline(ifr->ifr_name);
    if (ifp == NULL)
        return 6;

    int spl = sceNet_lib_94B44F26();
    ifp->if_ioctl_pending = 0;
    sceNet_lib_515B2F33(spl);

    if (g_if_hooks_reg > 0) {
        int handled = 0;
        int ret = g_if_ioctl_pre_cb(cmd, req_ptr, req_ptr, &handled);
        if (handled > 0)
            return ret;
    }

    int (*ioctl_fn)(NetIfnet *, u32, void *) = ifp->if_ioctl;

    switch (cmd) {
    case 0xC0206911u: /* SIOCGIFFLAGS */
        ifr->ifr_flags = ifp->if_flags;
        return 0;
    case 0x80206910u: /* SIOCSIFFLAGS */ {
        u16 oldf = ifp->if_flags;
        u16 newf = ifr->ifr_flags;
        if ((oldf & 1) && !(newf & 1)) {
            int inner_spl = sceNet_lib_94B44F26();
            sceNet_lib_EDB11CB4(ifp);
            sceNet_lib_515B2F33(inner_spl);
        }
        if ((newf & 1) && !(ifp->if_flags & 1)) {
            int inner_spl = sceNet_lib_94B44F26();
            ifp->if_flags |= 1;
            if (g_if_hooks_reg > 0)
                g_if_up_cb(ifp);
            sceNet_lib_515B2F33(inner_spl);
        }
        ifp->if_flags = (u16)((ifp->if_flags & 0x8E52) | (newf & 0x71AD));
        if (ioctl_fn != NULL)
            ioctl_fn(ifp, cmd, req_ptr);
        return 0;
    }
    case 0x80206918u: /* SIOCSIFMETRIC */
        ifp->if_data.ifi_metric = (u64)(s64)ifr->ifr_metric;
        return 0;
    case 0xC0206917u: /* SIOCGIFMETRIC */
        ifr->ifr_metric = (s32)ifp->if_data.ifi_metric;
        return 0;
    case 0xC020697Eu: /* SIOCGIFMTU */
        ifr->ifr_mtu = (u32)ifp->if_data.ifi_mtu;
        return 0;
    case 0xC020693Bu: /* SIOCGIFDATA */
        sceKernelMemcpy(ifr->ifr_ptr, &ifp->if_data, sizeof(NetIfData));
        return 0;
    case 0xC020693Du: /* dequeue event */ {
        u32 *out = (u32 *)ifr->ifr_ptr;
        int intr = sceKernelCpuSuspendIntr();
        NetIfEvent *ev = ifp->if_ev_head;
        if (ev == NULL) {
            sceKernelCpuResumeIntr(intr);
            return 2;
        }
        NetIfEvent *next = ev->ev_next;
        if (next != NULL)
            next->ev_prev = ev->ev_prev;
        else
            ifp->if_ev_tail = ev->ev_prev;
        *ev->ev_prev = next;
        sceKernelCpuResumeIntr(intr);
        out[0] = ev->ev_code;
        out[1] = ev->ev_value;
        sceNetIfhandle_lib_76BAD213(ev);
        return 0;
    }
    case 0x8020693Eu: /* set event filter */ {
        u32 *in = (u32 *)ifr->ifr_ptr;
        int intr = sceKernelCpuSuspendIntr();
        ifp->if_ev_mask = in[0];
        ifp->if_ev_arg = in[1];
        if (in[0] == 0 || in[1] == 0)
            free_if_events_inline(ifp);
        sceKernelCpuResumeIntr(intr);
        return 0;
    }
    case 0xC020693Eu: /* get event filter */ {
        u32 *out = (u32 *)ifr->ifr_ptr;
        out[0] = ifp->if_ev_mask;
        out[1] = ifp->if_ev_arg;
        return 0;
    }
    case 0x80206931u:
    case 0x80206932u:
    case 0x80206949u:
    case 0x8020697Fu:
    case 0x80406946u:
    case 0xC0206935u:
    case 0xC0206947u:
    case 0xC0206948u:
    case 0xC0286936u:
        if (ioctl_fn == NULL)
            return 95;
        return ioctl_fn(ifp, cmd, req_ptr);
    default:
        break;
    }

    if (so != NULL && g_if_hooks_reg > 0) {
        int handled = 0;
        int ret = g_if_ioctl_so_cb(so, &handled);
        if (handled > 0)
            return ret;
        return g_if_ioctl_post_cb(so, cmd, req_ptr, ifp);
    }

    if (ioctl_fn == NULL)
        return 95;
    return ioctl_fn(ifp, cmd, req_ptr);
}

/* =========================================================================
 * Ethernet layer
 * ========================================================================= */

static int ether_output(NetIfnet *ifp, NetMbuf *mbuf, void *dst, void *rt)
{
    if (g_ether_hooks_reg > 0)
        return g_ether_output_cb(ifp, mbuf, dst, rt);
    return 0;
}

static void ether_input(NetIfnet *ifp, NetMbuf *mbuf)
{
    NetIfQueue *inq = NULL;
    int isr = 0;

    if ((ifp->if_flags & 1) == 0) {
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return;
    }

    u8 *eh = mbuf->m_data;
    u16 etype = (u16)(((u16)eh[12] << 8) | eh[13]);
    u64 max_len = ifp->if_data.ifi_mtu + 14;
    if (mbuf->m_flags & 0x1000)
        max_len += 4;
    if (etype == 0x8100)
        max_len += 4;
    u32 pkthdr_len = mbuf->m_pkthdr_len;
    if ((u64)pkthdr_len > max_len) {
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return;
    }

    ifp->if_data.ifi_ibytes += pkthdr_len;
    if (eh[0] & 1) {
        ifp->if_data.ifi_imcasts += 1;
    } else if ((ifp->if_flags & 0x100) != 0) {
        NetSockaddrDl *sdl = ifp->if_sadl;
        if (sceNet_lib_E0A81C7C(sdl->sdl_data + sdl->sdl_nlen, eh, 6) != 0) {
            sceNetIfhandle_lib_F56FAC82(mbuf);
            return;
        }
    }

    if (etype == 0x88C8) {
        int spl = sceNet_lib_94B44F26();
        if (g_eapol_hook_reg > 0) {
            if (g_eapol_input_cb(ifp, mbuf, &inq) >= 0) {
                if (inq->ifq_len < inq->ifq_maxlen) {
                    mbuf->m_nextpkt = NULL;
                    if (inq->ifq_tail != NULL)
                        inq->ifq_tail->m_nextpkt = mbuf;
                    else
                        inq->ifq_head = mbuf;
                    inq->ifq_tail = mbuf;
                    inq->ifq_len++;
                } else {
                    inq->ifq_drops++;
                    sceNetIfhandle_lib_F56FAC82(mbuf);
                }
                sceNet_lib_515B2F33(spl);
                sceNet_lib_7C3B86C5(29);
                return;
            }
            sceNet_lib_515B2F33(spl);
            return;
        }
        sceNet_lib_515B2F33(spl);
    }

    if (g_ether_hooks_reg > 0) {
        int handled = 0;
        g_ether_input_pre_cb(ifp, mbuf, etype, &handled);
        if (handled > 0)
            return;
    }

    sceNetIfhandle_lib_BFF3CEA5(mbuf, 14);
    if (mbuf->m_flags & 0x1000)
        sceNetIfhandle_lib_BFF3CEA5(mbuf, -4);

    if (g_ether_hooks_reg > 0) {
        int handled = 0;
        g_ether_input_dispatch_cb(ifp, mbuf, etype, eh, &inq, &isr, &handled);
        if (handled > 0)
            return;
    }

    if (inq == NULL) {
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return;
    }

    int spl = sceNet_lib_94B44F26();
    if (inq->ifq_len < inq->ifq_maxlen) {
        mbuf->m_nextpkt = NULL;
        if (inq->ifq_tail != NULL)
            inq->ifq_tail->m_nextpkt = mbuf;
        else
            inq->ifq_head = mbuf;
        inq->ifq_tail = mbuf;
        inq->ifq_len++;
        sceNet_lib_7C3B86C5(isr);
    } else {
        inq->ifq_drops++;
        sceNetIfhandle_lib_F56FAC82(mbuf);
    }
    sceNet_lib_515B2F33(spl);
}

static void ether_ifattach(NetIfnet *ifp, const void *lla)
{
    ifp->if_data.ifi_type = 6;
    ifp->if_data.ifi_addrlen = 6;
    ifp->if_data.ifi_hdrlen = 14;
    ifp->if_data.ifi_mtu = 1500;
    ifp->if_output = ether_output;
    ifp->if_input = ether_input;
    if (ifp->if_data.ifi_baudrate == 0)
        ifp->if_data.ifi_baudrate = 10000000ULL;
    NetSockaddrDl *sdl = ifp->if_sadl;
    if (sdl != NULL && sdl->sdl_family == 18) {
        sdl->sdl_type = 6;
        sdl->sdl_alen = ifp->if_data.ifi_addrlen;
        sceNet_lib_2F305274(lla, sdl->sdl_data + sdl->sdl_nlen, ifp->if_data.ifi_addrlen);
    }
    ifp->if_broadcastaddr = g_ether_broadcast;
}

static void ether_ifdetach(NetIfnet *ifp)
{
    NetSockaddrDl *sdl = ifp->if_sadl;
    sceKernelMemset(sdl->sdl_data + sdl->sdl_nlen, 0, 6);
    sdl->sdl_type = 0;
    sdl->sdl_alen = 0;
}

static int ether_multiaddr(NetSockaddr *sa, u8 *addrlo, u8 *addrhi)
{
    if (g_ether_hooks_reg > 0) {
        int handled = 0;
        int ret = g_ether_multi_cb(sa, addrlo, addrhi, &handled);
        if (handled > 0)
            return ret;
    }
    if (sa->sa_family != 0)
        return 106;
    sceNet_lib_2F305274(sa->sa_data, addrlo, 6);
    sceNet_lib_2F305274(addrlo, addrhi, 6);
    return 0;
}

NetIfnet *sceNet_lib_D60225A3(const void *mac)
{
    NetIfnet *ifp = g_ifnet_list.head;
    while (ifp != NULL) {
        NetSockaddrDl *sdl = ifp->if_sadl;
        if (sceNet_lib_E0A81C7C(sdl->sdl_data + sdl->sdl_nlen, mac, 6) == 0)
            return ifp;
        ifp = ifp->if_next;
    }
    return NULL;
}

int sceNet_lib_5ED457BE(int (*out_cb)(NetIfnet *, NetMbuf *, void *, void *),
                        void (*in_pre)(NetIfnet *, NetMbuf *, u16, int *),
                        void (*in_disp)(NetIfnet *, NetMbuf *, u16, u8 *, NetIfQueue **, int *, int *),
                        int (*multi_cb)(void *, u8 *, u8 *, int *))
{
    g_ether_output_cb = out_cb;
    g_ether_input_pre_cb = in_pre;
    g_ether_input_dispatch_cb = in_disp;
    g_ether_multi_cb = multi_cb;
    g_ether_hooks_reg = 1;
    return 0;
}

int sceNet_lib_31F3CDA1(void)
{
    g_ether_hooks_reg = 0;
    g_ether_output_cb = NULL;
    g_ether_input_pre_cb = NULL;
    g_ether_input_dispatch_cb = NULL;
    g_ether_multi_cb = NULL;
    return 0;
}

int sceNet_lib_1F94AFD9(int (*cb)(NetIfnet *, NetMbuf *, NetIfQueue **))
{
    g_eapol_input_cb = cb;
    g_eapol_hook_reg = 1;
    return 0;
}

int sceNet_lib_0A5A8751(void)
{
    g_eapol_hook_reg = 0;
    g_eapol_input_cb = NULL;
    return 0;
}

static int ether_ioctl(NetIfnet *ifp, u32 cmd, void *req_ptr)
{
    NetIfreq *ifr = (NetIfreq *)req_ptr;
    int err = 0;
    int drvc = 0;

    if (ifp == NULL)
        return 22;

    int spl = sceNet_lib_94B44F26();

    switch (cmd) {
    case 0x8020690Cu: /* SIOCSIFADDR */ {
        NetIfaddr *ifa = (NetIfaddr *)req_ptr;
        NetSockaddr *sa = ifa->ifa_addr;
        if (sa->sa_family == 2) {
            if (g_arp_hooks_reg > 0)
                g_arp_ifinit_cb(ifp, req_ptr);
        } else if (sa->sa_family == 18) {
            NetSockaddrDl *sa_dl = (NetSockaddrDl *)sa;
            if (sa_dl->sdl_type == 6 && sa_dl->sdl_alen == ifp->if_data.ifi_addrlen) {
                NetSockaddrDl *sdl = ifp->if_sadl;
                sceKernelMemcpy(sdl->sdl_data + sdl->sdl_nlen,
                                sa_dl->sdl_data + sa_dl->sdl_nlen,
                                sa_dl->sdl_alen);
            } else {
                err = 22;
            }
        }
        ifp->if_ioctl_pending++;
        sceNetIfhandle_lib_8FCB05A1(ifp);
        ifp->if_flags |= 0x41;
        break;
    }
    case 0x80206910u: /* SIOCSIFFLAGS */ {
        u16 flags = ifp->if_flags;
        if ((flags & 0x41) == 0x40) {
            ifp->if_ioctl_pending++;
            sceNetIfhandle_lib_EAD3A759(ifp);
            ifp->if_flags &= ~0x40u;
        } else if ((flags & 0x41) == 1) {
            ifp->if_ioctl_pending++;
            sceNetIfhandle_lib_8FCB05A1(ifp);
            ifp->if_flags |= 0x40;
        } else if ((flags & 1) && (flags & 0x100)) {
            ifp->if_ioctl_pending++;
            sceNetIfhandle_lib_0296C7D6(ifp, 5, req_ptr);
        }
        break;
    }
    case 0x80206931u: /* SIOCADDMULTI */
    case 0x80206932u: /* SIOCDELMULTI */ {
        u8 lo[6], hi[6];
        err = ether_multiaddr(&ifr->ifr_addr, lo, hi);
        if (err == 0) {
            if ((lo[0] & 1) == 0 || (hi[0] & 1) == 0 || sceNet_lib_E0A81C7C(lo, hi, 6) != 0) {
                err = 22;
            } else {
                ifp->if_ioctl_pending++;
                sceNetIfhandle_lib_0296C7D6(ifp, (cmd == 0x80206931u) ? 2 : 3, req_ptr);
            }
        }
        break;
    }
    case 0x8020697Fu: /* SIOCSIFMTU */
        if ((s32)ifr->ifr_mtu <= 1500) {
            ifp->if_ioctl_pending++;
            sceNetIfhandle_lib_0296C7D6(ifp, 1, req_ptr);
            ifp->if_data.ifi_mtu = (u64)(s64)(s32)ifr->ifr_mtu;
        } else {
            err = 22;
        }
        break;
    case 0xC0206921u: /* SIOCGIFADDR */ {
        NetSockaddrDl *sdl = ifp->if_sadl;
        sceKernelMemcpy(ifr->ifr_addr.sa_data, sdl->sdl_data + sdl->sdl_nlen, 6);
        break;
    }
    case 0x801669FAu:
    case 0x801669FBu:
        if ((ifr->ifr_word & 1) == 0) {
            err = 22;
        } else {
            drvc = (cmd == 0x801669FAu) ? 6 : 7;
            ifp->if_ioctl_pending++;
            sceNetIfhandle_lib_0296C7D6(ifp, drvc, req_ptr);
        }
        break;
    default:
        switch (cmd) {
        case 0x40006909u: drvc = 59; break;
        case 0x4000690Au: drvc = 61; break;
        case 0x401469D3u: drvc = 51; break;
        case 0x401469DAu: drvc = 58; break;
        case 0x401469DEu: drvc = 65; break;
        case 0x401469E0u: drvc = 67; break;
        case 0x401869D9u: drvc = 57; break;
        case 0x800069C8u: drvc = 59; break;
        case 0x801469D2u: drvc = 50; break;
        case 0x801469DDu: drvc = 64; break;
        case 0x801469DFu: drvc = 66; break;
        case 0x801469E4u: drvc = 71; break;
        case 0x801C69DBu: drvc = 62; break;
        case 0x8020693Cu: drvc = 4;  break;
        case 0x803069E2u: drvc = 69; break;
        case 0x803069E3u: drvc = 70; break;
        case 0xC01469D7u: drvc = 55; break;
        case 0xC01469D8u: drvc = 56; break;
        case 0xC01869D5u: drvc = 53; break;
        case 0xC01869D6u: drvc = 54; break;
        case 0xC01C69DCu: drvc = 63; break;
        case 0xC01C69E1u: drvc = 68; break;
        case 0xC02069D4u: drvc = 52; break;
        default: err = 22; break;
        }
        if (err == 0) {
            ifp->if_ioctl_pending++;
            sceNetIfhandle_lib_0296C7D6(ifp, drvc, req_ptr);
        }
        break;
    }

    sceNet_lib_515B2F33(spl);
    return err;
}

static void ether_start(NetIfnet *ifp)
{
    while (ifp->if_snd.ifq_head != NULL) {
        NetMbuf *mbuf = ifp->if_snd.ifq_head;
        ifp->if_snd.ifq_head = mbuf->m_nextpkt;
        if (ifp->if_snd.ifq_head == NULL)
            ifp->if_snd.ifq_tail = NULL;
        mbuf->m_nextpkt = NULL;
        ifp->if_snd.ifq_len--;
        if (ifp->if_bpf != NULL)
            bpf_mtap(ifp->if_bpf, mbuf);
        sceNetIfhandle_lib_B1F5BB87(ifp, mbuf);
    }
}

int sceNet_lib_991FF86D(void (*cb)(NetIfnet *, void *))
{
    g_arp_ifinit_cb = cb;
    g_arp_hooks_reg = 1;
    return 0;
}

int sceNet_lib_5505D820(void)
{
    g_arp_hooks_reg = 0;
    g_arp_ifinit_cb = NULL;
    return 0;
}

/* =========================================================================
 * PPP layer
 * ========================================================================= */

static int ppp_output(NetIfnet *ifp, NetMbuf *mbuf, void *dst, void *rt)
{
    if (g_ppp_hooks_reg > 0)
        return g_ppp_output_cb(ifp, mbuf, dst, rt);
    return 0;
}

static void ppp_input(NetIfnet *ifp, NetMbuf *mbuf)
{
    if (g_ppp_hooks_reg > 0)
        g_ppp_input_cb(ifp, mbuf);
}

static void ppp_start(NetIfnet *ifp)
{
    if (g_ppp_hooks_reg <= 0)
        return;
    for (;;) {
        NetMbuf *mbuf = g_ppp_dequeue_cb(ifp);
        if (mbuf == NULL)
            break;
        if (ifp->if_bpf != NULL)
            bpf_mtap(ifp->if_bpf, mbuf);
        sceNetIfhandle_lib_B1F5BB87(ifp, mbuf);
    }
}

static int ppp_sync_to_drv(void *ifp_raw)
{
    NetPppSoftc *sc = (NetPppSoftc *)ifp_raw;
    NetPppSyncPkt pkt;
    sc->sc_raccm = sc->sc_raccm_cfg;
    sc->sc_xaccm_lo = sc->sc_xaccm[0];
    sceKernelMemset(&pkt, 0, sizeof(pkt));
    pkt.tx_accm_ctl = sc->sc_tx_accm_ctl;
    pkt.rx_accm_ctl = sc->sc_rx_accm_ctl;
    pkt.drv_cfg[0] = sc->sc_drv_cfg[0];
    pkt.drv_cfg[1] = sc->sc_drv_cfg[1];
    pkt.drv_cfg[2] = sc->sc_drv_cfg[2];
    pkt.drv_cfg[3] = sc->sc_drv_cfg[3];
    sceKernelMemcpy(pkt.xaccm, sc->sc_xaccm, sizeof(pkt.xaccm));
    pkt.raccm = sc->sc_raccm_cfg;
    return sceNetIfhandle_lib_0296C7D6(sc, 100, &pkt);
}

static int ppp_sync_from_drv(void *ifp_raw)
{
    NetPppSoftc *sc = (NetPppSoftc *)ifp_raw;
    NetPppSyncPkt pkt;
    sc->sc_raccm_cfg = sc->sc_raccm;
    sc->sc_xaccm[0] = sc->sc_xaccm_lo;
    sceKernelMemset(&pkt, 0, sizeof(pkt));
    pkt.tx_accm_ctl = sc->sc_tx_accm_ctl;
    pkt.rx_accm_ctl = sc->sc_rx_accm_ctl;
    pkt.drv_cfg[0] = sc->sc_drv_cfg[0];
    pkt.drv_cfg[1] = sc->sc_drv_cfg[1];
    pkt.drv_cfg[2] = sc->sc_drv_cfg[2];
    pkt.drv_cfg[3] = sc->sc_drv_cfg[3];
    sceKernelMemcpy(pkt.xaccm, sc->sc_xaccm, sizeof(pkt.xaccm));
    pkt.raccm = sc->sc_raccm_cfg;
    return sceNetIfhandle_lib_0296C7D6(sc, 100, &pkt);
}

static int ppp_ioctl(NetIfnet *ifp, u32 cmd, void *req_ptr)
{
    NetPppSoftc *sc = (NetPppSoftc *)ifp->if_softc;
    NetIfreq *ifr = (NetIfreq *)req_ptr;

    switch (cmd) {
    case 0x80206986u: {
        void (*fn)(void *) = ifr->ifr_word ? sc->sc_start_link : sc->sc_stop_link;
        fn(sc);
        return 0;
    }
    case 0x80146983u:
        sc->sc_xaccm[0] = ifr->ifr_word;
        return 0;
    case 0x80146985u:
        sc->sc_raccm_cfg = ifr->ifr_word;
        return 0;
    case 0xC0146982u:
        ifr->ifr_word = sc->sc_xaccm[0];
        return 0;
    case 0xC0146984u:
        ifr->ifr_word = sc->sc_raccm_cfg;
        return 0;
    default:
        break;
    }

    int err = 95;
    if (g_ppp_hooks_reg > 0)
        err = g_ppp_ioctl_cb(ifp, cmd, req_ptr);
    if (err == 0) {
        if (cmd == 0x80206931u) {
            ifp->if_ioctl_pending++;
            sceNetIfhandle_lib_0296C7D6(ifp, 2, req_ptr);
        } else if (cmd == 0x80206932u) {
            ifp->if_ioctl_pending++;
            sceNetIfhandle_lib_0296C7D6(ifp, 3, req_ptr);
        }
    }
    return err;
}

static void ppp_attach(NetIfnet *ifp)
{
    NetPppSoftc *sc = (NetPppSoftc *)ifp;
    ifp->if_data.ifi_type = 23;
    ifp->if_data.ifi_hdrlen = 4;
    ifp->if_data.ifi_mtu = 1500;
    ifp->if_flags = 0x8010;
    ifp->if_output = ppp_output;
    ifp->if_input = ppp_input;
    ifp->if_start = ppp_start;
    ifp->if_ioctl = ppp_ioctl;
    sc->sc_flags |= 1;
    sc->sc_sync_to_drv = ppp_sync_to_drv;
    sc->sc_sync_from_drv = ppp_sync_from_drv;
    ifp->if_softc = ifp;
    sceNet_lib_8B615F52(sc->sc_xaccm, sizeof(sc->sc_xaccm));
    sc->sc_xaccm[0] = 0xFFFFFFFFu;
    sc->sc_xaccm[3] = 0x60000000u;
    sc->sc_raccm_cfg = 0;
    if (g_ppp_hooks_reg > 0)
        g_ppp_attach_cb(ifp);
}

static void ppp_detach(NetIfnet *ifp)
{
    if (g_ppp_hooks_reg > 0)
        g_ppp_detach_cb(ifp);
}

int sceNet_lib_B51452D2(void (*att)(NetIfnet *), void (*det)(NetIfnet *),
                        int (*out)(NetIfnet *, NetMbuf *, void *, void *),
                        void (*in)(NetIfnet *, NetMbuf *),
                        int (*ioc)(NetIfnet *, u32, void *),
                        NetMbuf *(*deq)(NetIfnet *))
{
    g_ppp_attach_cb = att;
    g_ppp_detach_cb = det;
    g_ppp_output_cb = out;
    g_ppp_input_cb = in;
    g_ppp_ioctl_cb = ioc;
    g_ppp_dequeue_cb = deq;
    g_ppp_hooks_reg = 1;
    return 0;
}

int sceNet_lib_F6FE6D8E(void)
{
    g_ppp_hooks_reg = 0;
    g_ppp_attach_cb = NULL;
    g_ppp_detach_cb = NULL;
    g_ppp_output_cb = NULL;
    g_ppp_input_cb = NULL;
    g_ppp_ioctl_cb = NULL;
    return 0;
}

/* =========================================================================
 * Ifhandle pool, R521 PRNG, time, HMAC-SHA1, Ether str/ntoa, MallocStat
 * ========================================================================= */

static int net_ifhandle_init(SceUID partId, int poolSize)
{
    int ret = sceNetIfhandle_lib_0C391E9F(partId, poolSize);
    if (ret < 0) {
        sceNetIfhandle_lib_29ED84C5();
        return ret;
    }
    return 0;
}

static int net_ifhandle_term(void)
{
    sceNetIfhandle_lib_29ED84C5();
    return 0;
}

static void net_prng_seed(u32 seed)
{
    int idx, bit;
    u32 word = 0;
    for (idx = 0; idx < 17; idx++) {
        for (bit = 0; bit < 32; bit++) {
            seed = seed * 1566083941u + 1;
            word = (word >> 1) | (seed & 0x80000000u);
        }
        g_prng_state[idx] = word;
    }
    g_prng_state[16] = (g_prng_state[16] << 23) ^ (g_prng_state[0] >> 9) ^ g_prng_state[15];
    for (idx = 17; idx < 521; idx++)
        g_prng_state[idx] = (g_prng_state[idx - 17] << 23) ^ (g_prng_state[idx - 16] >> 9) ^ g_prng_state[idx - 1];
    prng_twist_inline();
    prng_twist_inline();
    prng_twist_inline();
    g_prng_idx = 520;
}

int sceNet_lib_1858883D(void)
{
    if (++g_prng_idx >= 521) {
        prng_twist_inline();
        g_prng_idx = 0;
    }
    return (int)g_prng_state[g_prng_idx];
}

u32 sceNet_lib_75D9985C(void)
{
    u8 sysclock[16];
    u32 sec, usec;
    sceKernelGetSystemTime(sysclock);
    sceKernelSysClock2USec(sysclock, &sec, &usec);
    return sec;
}

int sceNet_lib_8D34E686(u8 *ctx, const u8 *key, int key_len)
{
    u8 k_ipad[65];
    u8 tk[20];
    u8 sha_buf[96];
    int idx;

    if (key_len > 64) {
        sceKernelUtilsSha1BlockInit(sha_buf);
        sceKernelUtilsSha1BlockUpdate(sha_buf, key, (u32)key_len);
        sceKernelUtilsSha1BlockResult(sha_buf, tk);
        key = tk;
        key_len = 20;
    }
    sceKernelMemset(k_ipad, 0, 65);
    sceKernelMemset(ctx + 96, 0, 65);
    sceKernelMemcpy(k_ipad, key, (u32)key_len);
    sceKernelMemcpy(ctx + 96, key, (u32)key_len);
    for (idx = 0; idx < 64; idx++) {
        k_ipad[idx] ^= 0x36;
        ctx[96 + idx] ^= 0x5C;
    }
    sceKernelUtilsSha1BlockInit(ctx);
    return sceKernelUtilsSha1BlockUpdate(ctx, k_ipad, 64);
}

int sceNet_lib_3A0668D5(void *ctx, const void *buf, u32 len)
{
    return sceKernelUtilsSha1BlockUpdate(ctx, buf, len);
}

int sceNet_lib_4A3E75C1(u8 *digest, u8 *ctx)
{
    u8 inner[20];
    sceKernelUtilsSha1BlockResult(ctx, inner);
    sceKernelUtilsSha1BlockInit(ctx);
    sceKernelUtilsSha1BlockUpdate(ctx, ctx + 96, 64);
    sceKernelUtilsSha1BlockUpdate(ctx, inner, 20);
    return sceKernelUtilsSha1BlockResult(ctx, digest);
}

int sceNet_lib_384EFE14(const void *buf, u32 buf_len, const u8 *key, int key_len, u8 *digest)
{
    u8 ctx[96];
    u8 k_opad[65];
    u8 k_ipad[65];
    u8 tk[20];
    u8 sha_buf[96];
    u8 inner[20];
    int idx;

    if (key_len > 64) {
        sceKernelUtilsSha1BlockInit(sha_buf);
        sceKernelUtilsSha1BlockUpdate(sha_buf, key, (u32)key_len);
        sceKernelUtilsSha1BlockResult(sha_buf, tk);
        key = tk;
        key_len = 20;
    }
    sceKernelMemset(k_ipad, 0, 65);
    sceKernelMemset(k_opad, 0, 65);
    sceKernelMemcpy(k_ipad, key, (u32)key_len);
    sceKernelMemcpy(k_opad, key, (u32)key_len);
    for (idx = 0; idx < 64; idx++) {
        k_ipad[idx] ^= 0x36;
        k_opad[idx] ^= 0x5C;
    }
    sceKernelUtilsSha1BlockInit(ctx);
    sceKernelUtilsSha1BlockUpdate(ctx, k_ipad, 64);
    sceKernelUtilsSha1BlockUpdate(ctx, buf, buf_len);
    sceKernelUtilsSha1BlockResult(ctx, inner);
    sceKernelUtilsSha1BlockInit(ctx);
    sceKernelUtilsSha1BlockUpdate(ctx, k_opad, 64);
    sceKernelUtilsSha1BlockUpdate(ctx, inner, 20);
    return sceKernelUtilsSha1BlockResult(ctx, digest);
}

void sceNetEtherNtostr(const u8 *mac, char *str)
{
    if (mac != NULL && str != NULL) {
        sceNet_lib_CF705E46(str, "%02x:%02x:%02x:%02x:%02x:%02x",
                            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

int sceNetEtherStrton(const char *str, u8 *mac)
{
    char pair[3];
    int idx;
    if (str == NULL || mac == NULL)
        return 0;
    for (idx = 0; idx < 6; idx++) {
        sceKernelMemcpy(pair, str, 2);
        pair[2] = '\0';
        mac[idx] = (u8)(hex_val_inline(str[0]) * 16 + hex_val_inline(str[1]));
        str += 3;
    }
    return 0;
}

int sceNetGetMallocStat(void *stat)
{
    if (_sceKernelCheckThreadStack() < 1008)
        return SCE_NET_ERROR_STACK_SMALL;
    return sceNetIfhandle_lib_4C2886CB(stat);
}
