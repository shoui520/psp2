#include "pspnet_internal.h"

/* =========================================================================
 * Global state
 * ========================================================================= */

/* WLAN scan buffer & active BSS pointer */
static u8 g_wlan_scan_buf[1536];
static void *g_wlan_cur_bss = NULL;
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
static void (*g_adhoc_updown_cb)(void *) = NULL;

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
static int (*g_if_rtrequest_cb)(int, int, int) = NULL;
static void (*g_if_init_cb)(void) = NULL;
static void (*g_if_attach_cb)(void *) = NULL;
static void (*g_if_detach_addr_cb)(void *) = NULL;
static void (*g_if_purge_cb)(void *, int) = NULL;
static void (*g_if_detach_cb)(void *) = NULL;
static void (*g_if_detach_post_cb)(void *, u32) = NULL;
static void (*g_if_down_pre_cb)(void) = NULL;
static void (*g_if_down_post_cb)(void *) = NULL;
static void (*g_if_up_cb)(void *) = NULL;
static int (*g_if_ioctl_pre_cb)(u32, void *, void *, int *) = NULL;
static int (*g_if_ioctl_so_cb)(void *, int *) = NULL;
static int (*g_if_ioctl_post_cb)(void *, u32, void *, void *) = NULL;
static void (*g_if_extra_init_cb)(void) = NULL;
static int g_if_extra_init_reg = 0;

static s32 g_if_index = 0;
static void *g_ifnet_addrs_tbl[8];
static void *g_ifindex2ifnet_tbl[8];
static void **g_ifnet_addrs = g_ifnet_addrs_tbl;
static void **g_ifindex2ifnet = g_ifindex2ifnet_tbl;
static void *g_ifnet_head = NULL;
static void **g_ifnet_tail = &g_ifnet_head;
static NetCallout g_if_slowtimo_callout;
static u32 g_ifq_maxlen = 50;

/* Ethernet hooks */
static int (*g_ether_output_cb)(void *, void *, void *, void *) = NULL;
static void (*g_ether_input_pre_cb)(void *, void *, u16, int *) = NULL;
static void (*g_ether_input_dispatch_cb)(void *, void *, u16, u8 *, void **, int *, int *) = NULL;
static int (*g_ether_multi_cb)(void *, void *, void *, int *) = NULL;
static int (*g_eapol_input_cb)(void *, void *, void **) = NULL;
static void (*g_arp_ifinit_cb)(void *, void *) = NULL;
static const u8 g_ether_broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* PPP hooks */
static int (*g_ppp_attach_cb)(void *) = NULL;
static int (*g_ppp_detach_cb)(void *) = NULL;
static int (*g_ppp_output_cb)(void *, void *, void *, void *) = NULL;
static int (*g_ppp_input_cb)(void *, void *) = NULL;
static int (*g_ppp_ioctl_cb)(void *, u32, void *) = NULL;
static void *(*g_ppp_dequeue_cb)(void *) = NULL;

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
static void ether_ifattach(void *ifp, const void *lla);
static void ether_ifdetach(void *ifp);
static int ether_ioctl(void *ifp, u32 cmd, void *data);
static void ether_start(void *ifp);
static int ppp_attach(void *ifp);
static int ppp_detach(void *ifp);
static int ifioctl(void *so, u32 cmd, void *data);

/* =========================================================================
 * WLAN helper / ioctl wrappers
 * ========================================================================= */

int sceNet_lib_43EE76EC(const char *ifname, const void *ssid, u32 ssid_len,
                        void *wep_cfg, void *auth_cfg, u32 bss_type)
{
    u8 scan_req[76];
    u8 join_req[112];
    u8 ssid_cmp[32];
    struct {
        char ifname[16];
        void *req;
        u32 buf_len;
        void *buf;
    } io_scan;
    struct {
        char ifname[16];
        void *req;
        u32 unused;
    } io_join;
    void *sorted_head = NULL;
    u32 res_len = 1536;
    int i, ret;

    sceKernelMemset(scan_req, 0, sizeof(scan_req));
    if (ssid != NULL)
        sceKernelMemcpy(scan_req + 28, ssid, ssid_len);
    scan_req[24] = (u8)ssid_len;
    *(u32 *)scan_req = bss_type;
    for (i = 0; i < 14; i++)
        scan_req[10 + i] = (u8)(i + 1);
    *(u32 *)(scan_req + 60) = 1;
    *(u32 *)(scan_req + 64) = 0;
    *(u32 *)(scan_req + 68) = 6;
    *(u32 *)(scan_req + 72) = 100;

    sceKernelMemset(g_wlan_scan_buf, 0, sizeof(g_wlan_scan_buf));
    sceKernelMemset(&io_scan, 0, sizeof(io_scan));
    sceNet_lib_B5CE388A(io_scan.ifname, ifname, 16);
    io_scan.req = scan_req;
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
        u32 *cur = (u32 *)g_wlan_scan_buf;
        while (cur != NULL) {
            u32 *next = (u32 *)cur[0];
            if (ssid_len != 0 && sceNet_lib_E0A81C7C(cur + 3, ssid_cmp, 32) != 0) {
                cur = next;
                continue;
            }
            if (bss_type != 0 && cur[11] != bss_type) {
                cur = next;
                continue;
            }
            /* Insert sorted by RSSI descending (s16 at offset 76) */
            u32 **pp = (u32 **)&sorted_head;
            while (*pp != NULL && *(s16 *)((u8 *)(*pp) + 76) >= *(s16 *)((u8 *)cur + 76))
                pp = (u32 **)(*pp);
            cur[0] = (u32)(*pp);
            *pp = cur;
            cur = next;
        }
    }

    ret = SCE_NET_ERROR_JOIN_FAIL;
    while (sorted_head != NULL) {
        u32 *bss = (u32 *)sorted_head;
        ret = sceNet_lib_83FE280A(ifname, wep_cfg);
        if (ret < 0)
            break;
        ret = sceNet_lib_891723D5(ifname, auth_cfg);
        if (ret < 0)
            break;

        sceKernelMemset(join_req, 0, sizeof(join_req));
        sceKernelMemcpy(join_req, (u8 *)bss + 4, 92);
        g_wlan_cur_bss = bss;

        sceKernelMemset(&io_join, 0, sizeof(io_join));
        sceNet_lib_B5CE388A(io_join.ifname, ifname, 16);
        io_join.req = join_req;
        ret = sceNet_lib_13A8B98A(NULL, 0xC01869D6u, &io_join, 1);
        if (ret == 0)
            break;
        g_wlan_cur_bss = NULL;
        sorted_head = (void *)bss[0];
    }
    return ret;
}

int sceNet_lib_4BF9E1DE(const char *ifname, const void *ssid, u32 ssid_len,
                        const void *key, u32 key_len, void *auth_cfg)
{
    struct {
        u32 mode;
        u32 tx_key_idx;
        u32 reserved;
        struct {
            u32 type;
            u8 data[16];
        } keys[4];
        u8 pad[68];
    } wep;
    int i;

    sceKernelMemset(&wep, 0, 160);
    if (key != NULL) {
        if (key_len != 5 && key_len != 13)
            return SCE_NET_ERROR_EINVAL;
        wep.mode = 1;
        wep.tx_key_idx = 1;
        for (i = 0; i < 4; i++) {
            if (key_len == 5)
                wep.keys[i].type = 1;
            else if (key_len == 13)
                wep.keys[i].type = 2;
            sceKernelMemcpy(wep.keys[i].data, key, key_len);
        }
    }
    return sceNet_lib_43EE76EC(ifname, ssid, ssid_len, &wep, auth_cfg, 1);
}

static int bssid_less(const u8 *a, const u8 *b)
{
    u32 hi_a = ((u32)a[0] << 16) | ((u32)a[1] << 8) | a[2];
    u32 hi_b = ((u32)b[0] << 16) | ((u32)b[1] << 8) | b[2];
    if (hi_a != hi_b)
        return hi_a < hi_b;
    u32 lo_a = ((u32)a[3] << 16) | ((u32)a[4] << 8) | a[5];
    u32 lo_b = ((u32)b[3] << 16) | ((u32)b[4] << 8) | b[5];
    return lo_a < lo_b;
}

int sceNet_lib_D5B64E37(const char *ifname, const void *ssid, u32 ssid_len, u32 channel)
{
    u8 scan_req[76];
    u8 bss_req[112];
    u8 cur_bss[96];
    u8 ssid_cmp[32];
    struct {
        char ifname[16];
        void *req;
        u32 buf_len;
        void *buf;
        u32 status;
    } io_scan;
    struct {
        char ifname[16];
        void *req;
        u32 status;
    } io_cmd;
    u32 status = 0;
    u32 res_len = 1536;
    u32 t0, scan_us;
    u8 *best = NULL;
    int ret, attempt;

    if ((channel - 1) >= 14 || ssid == NULL || ssid_len == 0)
        return SCE_NET_ERROR_EINVAL;

    sceKernelMemset(scan_req, 0, sizeof(scan_req));
    sceKernelMemcpy(scan_req + 28, ssid, ssid_len);
    *(u32 *)scan_req = 2;
    scan_req[10] = (u8)channel;
    scan_req[11] = 0;
    scan_req[24] = (u8)ssid_len;
    *(u32 *)(scan_req + 60) = 1;
    *(u32 *)(scan_req + 64) = 0;
    *(u32 *)(scan_req + 68) = 6;
    *(u32 *)(scan_req + 72) = 100;

    sceKernelMemset(g_wlan_scan_buf, 0, sizeof(g_wlan_scan_buf));
    t0 = sceKernelGetSystemTimeLow();
    sceKernelMemset(&io_scan, 0, 32);
    sceNet_lib_B5CE388A(io_scan.ifname, ifname, 16);
    io_scan.req = scan_req;
    io_scan.buf_len = 1536;
    io_scan.buf = g_wlan_scan_buf;
    ret = sceNet_lib_13A8B98A(NULL, 0xC02069D4u, &io_scan, 1);
    if (ret == 0) {
        res_len = io_scan.buf_len;
        status = io_scan.status;
    }
    if (ret < 0)
        return ret;

    scan_us = sceKernelGetSystemTimeLow() - t0;
    if (res_len != 0) {
        sceKernelMemset(ssid_cmp, 0, sizeof(ssid_cmp));
        sceKernelMemcpy(ssid_cmp, ssid, ssid_len);
        u8 *cur = g_wlan_scan_buf;
        while (cur != NULL) {
            if (sceNet_lib_E0A81C7C(cur + 12, ssid_cmp, 32) == 0 && *(u32 *)(cur + 44) == 2) {
                if (best == NULL || bssid_less(cur + 4, best))
                    best = cur + 4;
            }
            cur = *(u8 **)cur;
        }
    }

    if (best != NULL) {
        sceKernelMemset(bss_req, 0, sizeof(bss_req));
        sceKernelMemcpy(bss_req, best, 92);
        status = 0;
        sceKernelMemset(&io_cmd, 0, sizeof(io_cmd));
        sceNet_lib_B5CE388A(io_cmd.ifname, ifname, 16);
        io_cmd.req = bss_req;
        ret = sceNet_lib_13A8B98A(NULL, 0xC01869D6u, &io_cmd, 1);
        if (ret == 0)
            status = io_cmd.status;
    } else {
        sceKernelMemset(bss_req, 0, sizeof(bss_req));
        bss_req[6] = (u8)channel;
        bss_req[7] = (u8)ssid_len;
        sceKernelMemcpy(bss_req + 8, ssid, ssid_len);
        *(u32 *)(bss_req + 40) = 2;
        *(u32 *)(bss_req + 44) = 100;
        *(u16 *)(bss_req + 62) = 34;
        sceKernelMemset(&io_cmd, 0, sizeof(io_cmd));
        sceNet_lib_B5CE388A(io_cmd.ifname, ifname, 16);
        io_cmd.req = bss_req;
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
        ret = sceNet_lib_A55C914F(ifname, cur_bss);
        if (ret < 0)
            goto fail_leave;

        sceKernelMemset(g_wlan_scan_buf, 0, sizeof(g_wlan_scan_buf));
        status = 0;
        sceKernelMemset(&io_scan, 0, 32);
        sceNet_lib_B5CE388A(io_scan.ifname, ifname, 16);
        res_len = 1536;
        io_scan.req = scan_req;
        io_scan.buf_len = 1536;
        io_scan.buf = g_wlan_scan_buf;
        ret = sceNet_lib_13A8B98A(NULL, 0xC02069D4u, &io_scan, 1);
        if (ret == 0) {
            res_len = io_scan.buf_len;
            status = io_scan.status;
        }
        if (ret == SCE_NET_ERROR_ADHOC_CANCEL)
            return 0;
        if (ret < 0)
            goto fail_leave;

        u8 *better = NULL;
        if (res_len != 0) {
            u8 *cur = g_wlan_scan_buf;
            while (cur != NULL) {
                if (sceNet_lib_E0A81C7C(cur + 12, cur_bss + 8, 32) == 0 && *(u32 *)(cur + 44) == 2) {
                    const u8 *ref = (better != NULL) ? better : cur_bss;
                    if (bssid_less(cur + 4, ref))
                        better = cur + 4;
                }
                cur = *(u8 **)cur;
            }
        }

        if (better != NULL) {
            status = 0;
            ret = sceNet_lib_DA02F383(ifname, &status);
            if (ret < 0)
                goto fail_leave;
            sceKernelDelayThread(3000000);
            sceKernelMemset(bss_req, 0, sizeof(bss_req));
            sceKernelMemcpy(bss_req, better, 92);
            status = 0;
            sceKernelMemset(&io_cmd, 0, sizeof(io_cmd));
            sceNet_lib_B5CE388A(io_cmd.ifname, ifname, 16);
            io_cmd.req = bss_req;
            ret = sceNet_lib_13A8B98A(NULL, 0xC01869D6u, &io_cmd, 1);
            if (ret == 0)
                status = io_cmd.status;
            if (ret < 0)
                goto fail_leave;
        } else {
            if (attempt < 2 && scan_us < 1000000u)
                sceKernelDelayThread(1000000u - scan_us - rand_delay);
        }
    }
    return ret;

fail_leave:
    status = 0;
    sceNet_lib_DA02F383(ifname, &status);
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

int sceNet_lib_F355C73B(const char *ifname, int type, void *out)
{
    if (type >= 6) {
        struct { char name[16]; u32 val; } req;
        sceKernelMemset(&req, 0, 20);
        sceNet_lib_B5CE388A(req.name, ifname, 16);
        int ret = sceNet_lib_13A8B98A(NULL, 0x401469DAu, &req, 1);
        if (ret >= 0)
            *(u32 *)out = req.val;
        return ret;
    } else {
        struct { char name[16]; u8 b0; u8 b1; u16 h0; u16 h1; u8 b2; u8 pad; } req;
        sceKernelMemset(&req, 0, 24);
        sceNet_lib_B5CE388A(req.name, ifname, 16);
        int ret = sceNet_lib_13A8B98A(NULL, 0x401869D9u, &req, 1);
        if (ret >= 0) {
            switch (type) {
            case 1: *(u8 *)out = req.b0; break;
            case 2: *(u8 *)out = req.b1; break;
            case 3: *(u16 *)out = req.h0; break;
            case 4: *(u16 *)out = req.h1; break;
            case 5: *(u8 *)out = req.b2; break;
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
    sceKernelMemcpy(out, (u8 *)g_wlan_cur_bss + 4, 92);
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

int sceNet_lib_0D633F53(const char *ifname, u32 a2, u32 a3, u32 *out)
{
    struct { char name[16]; u32 a2; u32 a3; u32 out; } req;
    sceKernelMemset(&req, 0, 28);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.a2 = a2;
    req.a3 = a3;
    int ret = sceNet_lib_13A8B98A(NULL, 0xC01C69E1u, &req, 1);
    if (ret == 0)
        *out = req.out;
    return ret;
}

int sceNet_lib_AB375964(const char *ifname, const void *arg)
{
    struct { char name[16]; u8 data[32]; } req;
    sceKernelMemset(&req, 0, 48);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    sceKernelMemcpy(req.data, arg, 32);
    return sceNet_lib_13A8B98A(NULL, 0x803069E2u, &req, 1);
}

int sceNet_lib_8DA79EB0(const char *ifname, const void *arg)
{
    struct { char name[16]; u8 data[32]; } req;
    sceKernelMemset(&req, 0, 48);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    sceKernelMemcpy(req.data, arg, 32);
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
    void *ifp = sceNet_lib_1D10419C(ifname);
    if (ifp == NULL)
        return SCE_NET_ERROR_NO_IF;
    u8 *sdl = *(u8 **)((u8 *)ifp + 224);
    sceKernelMemcpy(addr, sdl + sdl[5] + 8, 6);
    return 0;
}

int sceNetGetLocalEtherAddr(void *addr)
{
    if (_sceKernelCheckThreadStack() < 1008)
        return SCE_NET_ERROR_STACK_SMALL;
    void *ifp = sceNet_lib_1D10419C("wlan");
    if (ifp == NULL)
        return SCE_NET_ERROR_NO_IF;
    u8 *sdl = *(u8 **)((u8 *)ifp + 224);
    sceKernelMemcpy(addr, sdl + sdl[5] + 8, 6);
    return 0;
}

int sceNet_lib_D1BE2CE9(const char *ifname, u32 *out0, u32 *out1)
{
    struct { char name[16]; void *ptr; u32 pad[3]; } req;
    u32 vals[2];
    sceKernelMemset(&req, 0, 32);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.ptr = vals;
    sceKernelMemset(vals, 0, 8);
    int ret = sceNet_lib_13A8B98A(NULL, 0xC020693Du, &req, 1);
    if (ret == 0) {
        *out0 = vals[0];
        *out1 = vals[1];
    }
    return ret;
}

int sceNet_lib_AB7DD9A5(const char *ifname, u32 v0, u32 v1)
{
    struct { char name[16]; void *ptr; u32 pad[3]; } req;
    u32 vals[2];
    sceKernelMemset(&req, 0, 32);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    vals[0] = v0;
    vals[1] = v1;
    req.ptr = vals;
    return sceNet_lib_13A8B98A(NULL, 0x8020693Eu, &req, 1);
}

int sceNet_lib_80E1933E(const char *ifname, u32 *out0, u32 *out1)
{
    struct { char name[16]; void *ptr; u32 pad[3]; } req;
    u32 vals[2];
    sceKernelMemset(&req, 0, 32);
    sceNet_lib_B5CE388A(req.name, ifname, 16);
    req.ptr = vals;
    sceKernelMemset(vals, 0, 8);
    int ret = sceNet_lib_13A8B98A(NULL, 0xC020693Eu, &req, 1);
    if (ret == 0) {
        *out0 = vals[0];
        *out1 = vals[1];
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
    if (ret < 0)
        goto fail;
    ret = net_ifhandle_init(partId, poolsize);
    if (ret < 0)
        goto fail;
    sceNetIfhandle_lib_0FB8AE0D();
    ret = callout_init(calloutprio, partId, 4096);
    if (ret < 0)
        goto fail;
    ret = spl_init();
    if (ret < 0)
        goto fail;
    ret = slpque_init();
    if (ret < 0)
        goto fail;
    ret = threadlist_init();
    if (ret < 0)
        goto fail;
    ret = netintr_init(netintrprio, partId, 4096);
    if (ret < 0)
        goto fail;

    ifinit();
    bpf_init(8);
    sceWlanDrv_lib_8D5F551B(0x20000);
    return 0;

fail:
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

static void free_if_events(u32 *ifp)
{
    u32 *ev = (u32 *)ifp[68];
    while (ev != NULL) {
        u32 next = ev[0];
        if (next != 0)
            *(u32 *)(next + 4) = ev[1];
        else
            ifp[69] = ev[1];
        *(u32 *)ev[1] = next;
        sceNetIfhandle_lib_76BAD213(ev);
        ev = (u32 *)ifp[68];
    }
}

static int netintr_thread(SceSize args, void *argp)
{
    u32 bits;
    void *ifp_v;
    SceUID sema;
    (void)args;
    (void)argp;

    for (;;) {
        sceKernelWaitEventFlag(g_netintr_evf, 0xFFFFFFFFu, 17, &bits, NULL);
        int s = sceNet_lib_94B44F26();

        if (bits & 1) {
            if (sceNetIfhandle_lib_B8188F96(&ifp_v, &sema) == 0) {
                u32 *ifp = (u32 *)ifp_v;
                ifp[46] = (u32)ether_start;
                ifp[47] = (u32)ether_ioctl;
                *(u16 *)((u8 *)ifp + 48) = 0x8822;
                ifp[69] = (u32)&ifp[68];
                ifp[61] = (u32)sceNetIfhandle_lib_8FCB05A1;
                ifp[62] = (u32)sceNetIfhandle_lib_EAD3A759;
                ifp[63] = (u32)sceNetIfhandle_lib_0296C7D6;
                ifp[68] = 0;
                sceNet_lib_701DDDC3(ifp);
                ether_ifattach(ifp, &ifp[59]);
                bpf_attach((void **)&ifp[10], ifp, 1, 14);
                sceKernelSignalSema(sema, 1);
            }
        }

        if (bits & 2) {
            if (sceNetIfhandle_lib_4FB43BCE(&ifp_v, &sema) == 0) {
                u32 *ifp = (u32 *)ifp_v;
                bpf_detach(ifp);
                ether_ifdetach(ifp);
                sceNet_lib_D5A03BC0(ifp);
                if (g_adhoc_updown_reg > 0)
                    g_adhoc_updown_cb(ifp);
                free_if_events(ifp);
                sceKernelSignalSema(sema, 1);
            }
        }

        if (bits & 4) {
            for (;;) {
                void *m = sceNetIfhandle_lib_E440A7D8();
                if (m == NULL)
                    break;
                u32 *ifp = *(u32 **)((u8 *)m + 20);
                if (ifp == NULL)
                    break;
                if (ifp[10] != 0)
                    bpf_mtap((void *)ifp[10], m);
                void (*if_input)(void *, void *) = (void (*)(void *, void *))ifp[45];
                if_input(ifp, m);
            }
        }

        if ((bits & 8) && g_adhoc_updown_reg > 0) {
            void *ifp = g_ifnet_head;
            while (ifp != NULL) {
                g_adhoc_updown_cb(ifp);
                ifp = *(void **)((u8 *)ifp + 4);
            }
        }

        if (bits & 0x20) {
            if (sceNetIfhandle_lib_B40A882F(&ifp_v, &sema) == 0) {
                u32 *ifp = (u32 *)ifp_v;
                ifp[69] = (u32)&ifp[68];
                ifp[61] = (u32)sceNetIfhandle_lib_8FCB05A1;
                ifp[62] = (u32)sceNetIfhandle_lib_EAD3A759;
                ifp[63] = (u32)sceNetIfhandle_lib_0296C7D6;
                ifp[68] = 0;
                sceNet_lib_701DDDC3(ifp);
                ppp_attach(ifp);
                bpf_attach((void **)&ifp[10], ifp, 0, 0);
                sceKernelSignalSema(sema, 1);
            }
        }

        if (bits & 0x40) {
            if (sceNetIfhandle_lib_9173FD47(&ifp_v, &sema) == 0) {
                u32 *ifp = (u32 *)ifp_v;
                bpf_detach(ifp);
                ppp_detach(ifp);
                sceNet_lib_D5A03BC0(ifp);
                free_if_events(ifp);
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

        sceNet_lib_515B2F33(s);
    }
    return 0;
}

static int netintr_init(int prio, SceUID partId, int stackSize)
{
    SceKernelThreadOptParam opt;
    int ret;

    g_netisr_bits = 0;
    ret = sceKernelCreateEventFlag("SceNetNetintr", 0, 0, NULL);
    if (ret < 0)
        goto fail;
    g_netintr_evf = ret;

    opt.size = sizeof(opt);
    opt.stackMpid = partId;
    ret = sceKernelCreateThread("SceNetNetintr", netintr_thread, prio, stackSize, 0, &opt);
    if (ret < 0)
        goto fail;
    g_netintr_thid = ret;

    ret = sceKernelStartThread(g_netintr_thid, 0, NULL);
    if (ret < 0)
        goto fail;

    sceNetIfhandle_lib_30F69334(g_netintr_evf);
    return 0;

fail:
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

int sceNet_lib_B3A48B7F(void (*intr_fn)(void), void (*updown_cb)(void *))
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
    g_thread_head = NULL;
    g_thread_tail = &g_thread_head;
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
    NetCallout *c = (NetCallout *)arg;
    c->ready_next = NULL;
    c->ready_prev = g_callout_ready_tail;
    *g_callout_ready_tail = c;
    g_callout_ready_tail = &c->ready_next;

    if (c->pend_next != NULL)
        c->pend_next->pend_prev = c->pend_prev;
    else
        g_callout_pend_tail = c->pend_prev;
    *c->pend_prev = c->pend_next;
    c->pend_next = NULL;
    c->pend_prev = NULL;

    sceKernelSignalSema(g_callout_sema, 1);
    return 0;
}

static int callout_thread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    for (;;) {
        sceKernelWaitSema(g_callout_sema, 1, NULL);
        int s = sceNet_lib_94B44F26();
        for (;;) {
            int intr = sceKernelCpuSuspendIntr();
            NetCallout *c = g_callout_ready_head;
            if (c == NULL) {
                sceKernelCpuResumeIntr(intr);
                break;
            }
            if (c->ready_next != NULL)
                c->ready_next->ready_prev = c->ready_prev;
            else
                g_callout_ready_tail = c->ready_prev;
            *c->ready_prev = c->ready_next;
            void (*fn)(void *) = c->func;
            void *arg = c->arg;
            c->flags &= ~2u;
            c->ready_next = NULL;
            c->ready_prev = NULL;
            c->func = NULL;
            sceKernelCpuResumeIntr(intr);
            fn(arg);
        }
        sceNet_lib_515B2F33(s);
    }
    return 0;
}

static int callout_init(int prio, SceUID partId, int stackSize)
{
    SceKernelThreadOptParam opt;
    g_callout_ready_head = NULL;
    g_callout_ready_tail = &g_callout_ready_head;
    g_callout_pend_head = NULL;
    g_callout_pend_tail = &g_callout_pend_head;

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
        NetCallout *c = g_callout_pend_head;
        if (c->pend_next != NULL)
            c->pend_next->pend_prev = c->pend_prev;
        else
            g_callout_pend_tail = c->pend_prev;
        *c->pend_prev = c->pend_next;
        c->pend_next = NULL;
        c->pend_prev = NULL;
        sceKernelCancelAlarm(c->alarm_id);
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

int sceNet_lib_A8B6205A(NetCallout *c, u32 usec, void (*fn)(void *), void *arg)
{
    if (usec == 0)
        usec = 1;
    int intr = sceKernelCpuSuspendIntr();
    if (c->flags & 2)
        sceNet_lib_A93A93E9(c);
    c->flags = 3;
    c->func = fn;
    c->arg = arg;
    c->timeout_us = usec;
    c->ready_next = NULL;
    c->ready_prev = NULL;
    c->alarm_id = sceKernelSetAlarm(usec, callout_alarm_cb, c);
    c->pend_next = NULL;
    c->pend_prev = g_callout_pend_tail;
    *g_callout_pend_tail = c;
    g_callout_pend_tail = &c->pend_next;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceNet_lib_A93A93E9(NetCallout *c)
{
    int intr = sceKernelCpuSuspendIntr();
    if ((c->flags & 2) == 0) {
        c->flags &= ~1u;
        sceKernelCpuResumeIntr(intr);
        return 0;
    }

    NetCallout *cur = g_callout_pend_head;
    while (cur != NULL && cur != c)
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
        c->flags &= ~3u;
        c->func = NULL;
        c->ready_next = NULL;
        c->ready_prev = NULL;
        sceKernelCpuResumeIntr(intr);
        return 0;
    }

    cur = g_callout_ready_head;
    while (cur != NULL && cur != c)
        cur = cur->ready_next;
    if (cur != NULL) {
        if (cur->ready_next != NULL)
            cur->ready_next->ready_prev = cur->ready_prev;
        else
            g_callout_ready_tail = cur->ready_prev;
        *cur->ready_prev = cur->ready_next;
        c->flags &= ~3u;
        c->func = NULL;
        c->ready_next = NULL;
        c->ready_prev = NULL;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

void sceNet_lib_25CC373A(NetCallout *c)
{
    sceKernelMemset(c, 0, sizeof(NetCallout));
}

void sceNet_lib_DCBC596E(NetCallout *c)
{
    (void)c;
}

int sceNet_lib_7C86FBA4(NetCallout *c, u32 ticks, void (*fn)(void *), void *arg)
{
    if (ticks == 0)
        ticks = 1;
    if (ticks <= 0x68DB8u)
        return sceNet_lib_A8B6205A(c, ticks * 10000u, fn, arg);
    return 0;
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
        SleepBucket *b = &g_slpque[(wchan >> 8) & 0x7F];
        ti->slp_next = NULL;
        ti->slp_prev = b->tail;
        ti->wchan = wchan;
        *b->tail = ti;
        b->tail = &ti->slp_next;
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
            b->tail = ti->slp_prev;
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
    int i;
    for (i = 0; i < 128; i++) {
        g_slpque[i].head = NULL;
        g_slpque[i].tail = &g_slpque[i].head;
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

int sceNet_lib_6B294EE4(int a1, int a2)
{
    sceKernelLockLwMutex(&g_slpque_mtx, 1, NULL);
    if (g_slp_notify_reg > 0)
        g_slp_notify_cb(a1, a2);
    return sceKernelUnlockLwMutex(&g_slpque_mtx, 1);
}

int sceNet_lib_51C209B2(int a1)
{
    sceKernelLockLwMutex(&g_slpque_mtx, 1, NULL);
    if (g_adhoc_slp_reg > 0)
        g_slp_post_cb(a1);
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
    if ((s32)ret < 0 && ret != 0x800201CDu)
        return (int)ret;
    return 1;
}

int sceNet_lib_515B2F33(int s)
{
    if (s > 0)
        return sceKernelUnlockLwMutex(&g_spl_mtx, 1);
    return s;
}

u64 sceNet_lib_4B2B3416(u32 *tv)
{
    u64 now = (u64)sceKernelGetSystemTimeWide();
    tv[0] = (u32)(now / 1000000ULL);
    u64 rem = now % 1000000ULL;
    tv[1] = (u32)rem;
    return rem;
}

int sceNet_lib_2B42872F(void *cp, int n, NetUio *uio)
{
    u8 *p = (u8 *)cp;
    while (n > 0 && uio->uio_resid != 0) {
        NetIovec *iov = uio->uio_iov;
        u32 cnt = iov->iov_len;
        if (cnt == 0) {
            uio->uio_iov++;
            uio->uio_iovcnt--;
            continue;
        }
        if ((u32)n < cnt)
            cnt = (u32)n;
        if (uio->uio_rw != 0)
            sceNet_lib_2F305274(iov->iov_base, p, (int)cnt);
        else
            sceNet_lib_2F305274(p, iov->iov_base, (int)cnt);
        iov->iov_base = (u8 *)iov->iov_base + cnt;
        iov->iov_len -= cnt;
        uio->uio_resid -= cnt;
        uio->uio_offset += cnt;
        p += cnt;
        n -= (int)cnt;
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
    int i;
    for (i = 0; i < sz; i++)
        tbl[i] = 0;
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

int sceNet_lib_B9C780C7(SceUID *sel_thid)
{
    SceUID thid = *sel_thid;
    if (thid != 0) {
        int ret = sceNet_lib_B8C4A858(thid, (u32)&g_selwait);
        *sel_thid = 0;
        return ret;
    }
    return 0;
}

void *sceNet_lib_B68E1EEA(void)
{
    return &g_selwait;
}

/* =========================================================================
 * Interface management (ifnet)
 * ========================================================================= */

int sceNet_lib_701DDDC3(void *ifp_v)
{
    u8 *ifp = (u8 *)ifp_v;
    *(u32 *)(ifp + 12) = 0;
    *(u32 *)(ifp + 16) = (u32)(ifp + 12);

    int idx = 1;
    while (idx < 8) {
        u8 *cur = (u8 *)g_ifnet_head;
        int used = 0;
        while (cur != NULL) {
            if (*(u16 *)(cur + 44) == idx) {
                used = 1;
                break;
            }
            cur = *(u8 **)(cur + 4);
        }
        if (!used)
            break;
        idx++;
    }

    *(u16 *)(ifp + 44) = (u16)idx;
    *(u32 *)(ifp + 4) = 0;
    *(u32 *)(ifp + 8) = (u32)g_ifnet_tail;
    *g_ifnet_tail = ifp;
    g_ifnet_tail = (void **)(ifp + 4);
    g_ifindex2ifnet[idx] = ifp;
    if (idx > g_if_index)
        g_if_index = idx;

    int namelen = sceNet_lib_D8722983((const char *)(ifp + 20));
    u32 socksize = (u32)(namelen + 8 + ifp[57]);
    if (socksize < 20)
        socksize = 20;
    socksize = ((socksize - 1) | 3) + 1;

    u8 *ifa = (u8 *)sceNetIfhandle_lib_15CFE3C0(56 + 2 * socksize);
    sceNet_lib_8B615F52(ifa, (int)(56 + 2 * socksize));

    u8 *sdl = ifa + 56;
    sdl[0] = (u8)socksize;
    sdl[1] = 18;
    *(u16 *)(sdl + 2) = *(u16 *)(ifp + 44);
    sdl[4] = ifp[56];
    sdl[5] = (u8)namelen;
    sceNet_lib_2F305274(ifp + 20, sdl + 8, namelen);

    g_ifnet_addrs[*(u16 *)(ifp + 44)] = ifa;
    *(u32 *)(ifa + 48) = 2;
    *(u32 *)(ifa + 40) = (u32)sceNet_lib_EB6DE71A;
    *(u32 *)(ifa + 12) = (u32)ifp;
    *(u32 *)(ifa + 0) = (u32)sdl;
    *(u32 *)(ifp + 224) = (u32)sdl;

    u8 *mask = sdl + socksize;
    *(u32 *)(ifa + 8) = (u32)mask;
    mask[0] = (u8)(namelen + 8);
    {
        int i;
        for (i = 0; i < namelen; i++)
            mask[8 + i] = 0xFF;
    }

    /* Insert at head of if_addrlist */
    u8 *old_head = *(u8 **)(ifp + 12);
    *(u32 *)(ifa + 16) = (u32)old_head;
    if (old_head != NULL)
        *(u32 *)(old_head + 20) = (u32)(ifa + 16);
    else
        *(u32 *)(ifp + 16) = (u32)(ifa + 16);
    *(u32 *)(ifa + 20) = (u32)(ifp + 12);
    *(u32 *)(ifp + 12) = (u32)ifa;

    if (*(u32 *)(ifp + 216) == 0)
        *(u32 *)(ifp + 216) = g_ifq_maxlen;
    *(u32 *)(ifp + 228) = 0;
    *(u32 *)(ifp + 60) = 0;

    if (g_if_hooks_reg > 0)
        g_if_attach_cb(ifp);
    return 0;
}

static void ifq_purge(u32 *ifq)
{
    u32 *m = (u32 *)ifq[0];
    while (m != NULL) {
        u32 *next = (u32 *)m[1];
        sceNetIfhandle_lib_F56FAC82(m);
        m = next;
    }
    ifq[0] = 0;
    ifq[1] = 0;
    ifq[2] = 0;
}

int sceNet_lib_EDB11CB4(void *ifp_v)
{
    u8 *ifp = (u8 *)ifp_v;
    *(u16 *)(ifp + 48) &= ~1u;
    if (g_if_hooks_reg > 0)
        g_if_down_pre_cb();
    ifq_purge((u32 *)(ifp + 204));
    if (g_if_hooks_reg > 0)
        g_if_down_post_cb(ifp);
    return 0;
}

int sceNet_lib_D5A03BC0(void *ifp_v)
{
    u8 *ifp = (u8 *)ifp_v;
    int s = sceNet_lib_94B44F26();
    sceNet_lib_EDB11CB4(ifp);

    while (*(u32 *)(ifp + 12) != 0) {
        u32 *ifa = *(u32 **)(ifp + 12);
        if (*(u8 *)(ifa[0] + 1) == 18) {
            if (g_if_hooks_reg > 0)
                g_if_detach_addr_cb(ifa);
            u32 next = ifa[4];
            if (next != 0)
                *(u32 *)(next + 20) = ifa[5];
            else
                *(u32 *)(ifp + 16) = ifa[5];
            *(u32 *)ifa[5] = next;
            if (--ifa[12] == 0)
                sceNetIfhandle_lib_76BAD213(ifa);
        } else {
            if (g_if_hooks_reg > 0)
                g_if_purge_cb(ifp, *(u8 *)(ifa[0] + 1));
        }
    }

    if (g_if_hooks_reg > 0)
        g_if_detach_cb(ifp);

    u16 idx = *(u16 *)(ifp + 44);
    u32 *ifa = (u32 *)g_ifnet_addrs[idx];
    if (--ifa[12] == 0)
        sceNetIfhandle_lib_76BAD213(ifa);
    g_ifnet_addrs[idx] = NULL;

    if (g_if_hooks_reg > 0)
        g_if_detach_post_cb(ifp, 0x10000u);

    u8 *next_if = *(u8 **)(ifp + 4);
    if (next_if != NULL)
        *(u32 *)(next_if + 8) = *(u32 *)(ifp + 8);
    else
        g_ifnet_tail = *(void ***)(ifp + 8);
    **(void ***)(ifp + 8) = next_if;

    return sceNet_lib_515B2F33(s);
}

static void if_slowtimo(void *arg)
{
    (void)arg;
    int s = sceNet_lib_94B44F26();
    u8 *ifp = (u8 *)g_ifnet_head;
    while (ifp != NULL) {
        u16 timer = *(u16 *)(ifp + 46);
        if (timer != 0) {
            *(u16 *)(ifp + 46) = (u16)(timer - 1);
            if (timer == 1) {
                void (*watchdog)(void *) = *(void (**)(void *))(ifp + 196);
                if (watchdog != NULL)
                    watchdog(ifp);
            }
        }
        ifp = *(u8 **)(ifp + 4);
    }
    sceNet_lib_515B2F33(s);
    sceNet_lib_7C86FBA4(&g_if_slowtimo_callout, 100, if_slowtimo, NULL);
}

static void ifinit(void)
{
    g_if_index = 0;
    if (g_if_hooks_reg > 0)
        g_if_init_cb();
    g_ifnet_head = NULL;
    g_ifnet_tail = &g_ifnet_head;
    sceKernelMemset(g_ifindex2ifnet_tbl, 0, sizeof(g_ifindex2ifnet_tbl));
    g_ifindex2ifnet = g_ifindex2ifnet_tbl;
    sceKernelMemset(g_ifnet_addrs_tbl, 0, sizeof(g_ifnet_addrs_tbl));
    g_ifnet_addrs = g_ifnet_addrs_tbl;
    if (g_if_extra_init_reg > 0)
        g_if_extra_init_cb();
    sceNet_lib_25CC373A(&g_if_slowtimo_callout);
    if_slowtimo(NULL);
}

int sceNet_lib_8C55B410(void)
{
    return 6;
}

void sceNet_lib_EDCC871E(void *ptr)
{
    sceNetIfhandle_lib_76BAD213(ptr);
}

int sceNet_lib_EB6DE71A(int a1, int a2, int a3)
{
    if (g_if_hooks_reg > 0)
        return g_if_rtrequest_cb(a1, a2, a3);
    return 0;
}

int sceNet_lib_FA6DE6A6(void *ifp_v)
{
    u8 *ifp = (u8 *)ifp_v;
    *(u16 *)(ifp + 48) |= 1;
    if (g_if_hooks_reg > 0)
        g_if_up_cb(ifp);
    return 0;
}

void *sceNet_lib_1D10419C(const char *name)
{
    int s = sceNet_lib_94B44F26();
    u8 *ifp = (u8 *)g_ifnet_head;
    while (ifp != NULL) {
        if (*(void **)(ifp + 176) != (void *)sceNet_lib_8C55B410 &&
            sceNet_lib_A0F16ABD((const char *)(ifp + 20), name) == 0) {
            sceNet_lib_515B2F33(s);
            return ifp;
        }
        ifp = *(u8 **)(ifp + 4);
    }
    sceNet_lib_515B2F33(s);
    return NULL;
}

int sceNet_lib_522A971B(const char *name)
{
    return (sceNet_lib_1D10419C(name) != NULL) ? 1 : 0;
}

int sceNet_lib_13A8B98A(void *so, u32 cmd, void *data, int wait)
{
    u32 *ifp = (u32 *)sceNet_lib_1D10419C((const char *)data);
    int ret = 0;
    if (ifp == NULL)
        return SCE_NET_ERROR_NO_IF;

    if (wait <= 0) {
        if (sceKernelPollSema((SceUID)ifp[65], 1) < 0)
            return SCE_NET_ERROR_IF_BUSY;
    } else {
        sceKernelWaitSema((SceUID)ifp[65], 1, NULL);
    }

    ifp[67] = 0;
    int ierr = ifioctl(so, cmd, data);
    if (ierr != 0) {
        if (ierr == 125)
            ret = SCE_NET_ERROR_IOCTL_CANCELED;
        else if (ierr == 2)
            ret = SCE_NET_ERROR_IOCTL_ENOENT;
        else
            ret = SCE_NET_ERROR_IOCTL_ERR;
    } else if (wait > 0) {
        while ((s32)ifp[66] > 0) {
            sceKernelWaitSema((SceUID)ifp[64], 1, NULL);
            ifp[66]--;
        }
        ret = (int)ifp[67];
    }

    if (wait > 0 || ierr != 0)
        sceKernelSignalSema((SceUID)ifp[65], 1);
    return ret;
}

int sceNet_lib_EA42B353(const char *ifname, u32 *out)
{
    if (out == NULL)
        return SCE_NET_ERROR_EINVAL;
    u32 *ifp = (u32 *)sceNet_lib_1D10419C(ifname);
    if (ifp == NULL)
        return SCE_NET_ERROR_NO_IF;

    while ((s32)ifp[66] > 0) {
        if (sceKernelPollSema((SceUID)ifp[64], 1) < 0)
            return SCE_NET_ERROR_IF_BUSY;
        ifp[66]--;
    }
    *out = ifp[67];
    sceKernelSignalSema((SceUID)ifp[65], 1);
    return 0;
}

int sceNet_lib_E9D457B6(void *ifp_v, u32 cmd, void *data)
{
    u32 *ifp = (u32 *)ifp_v;
    int (*ioctl_fn)(void *, u32, void *) = (int (*)(void *, u32, void *))ifp[47];
    if (ioctl_fn == NULL)
        return -1;
    while ((s32)ifp[66] > 0) {
        sceKernelWaitSema((SceUID)ifp[64], 1, NULL);
        ifp[66]--;
    }
    ifp[67] = 0;
    if (ioctl_fn(ifp, cmd, data) != 0)
        return -1;
    sceKernelWaitSema((SceUID)ifp[64], 1, NULL);
    int res = (int)ifp[67];
    ifp[66]--;
    return res;
}

int sceNet_lib_710BD467(const char *name)
{
    int idx = -1;
    int s = sceNet_lib_94B44F26();
    u8 *ifp = (u8 *)g_ifnet_head;
    while (ifp != NULL) {
        if (sceNet_lib_94DCA9F0(name, (const char *)(ifp + 20), 16) == 0) {
            idx = *(u16 *)(ifp + 44);
            break;
        }
        ifp = *(u8 **)(ifp + 4);
    }
    sceNet_lib_515B2F33(s);
    return idx;
}

void *sceNet_lib_45945E8D(int idx)
{
    int s = sceNet_lib_94B44F26();
    u8 *ifp = (u8 *)g_ifnet_head;
    while (ifp != NULL) {
        if (*(u16 *)(ifp + 44) == idx)
            break;
        ifp = *(u8 **)(ifp + 4);
    }
    sceNet_lib_515B2F33(s);
    return ifp;
}

int sceNet_lib_C2EC2EEA(s32 *idx_out, void ***if2if_out, void ***addrs_out, void *head_out)
{
    if (idx_out != NULL)
        *idx_out = g_if_index;
    if (if2if_out != NULL)
        *if2if_out = g_ifindex2ifnet;
    if (addrs_out != NULL)
        *addrs_out = g_ifnet_addrs;
    if (head_out != NULL)
        sceKernelMemcpy(head_out, &g_ifnet_head, 8);
    return 0;
}

int sceNet_lib_976AB1E9(int (*rtreq)(int, int, int), void (*init_cb)(void),
                        void (*attach_cb)(void *), void (*detach_addr)(void *),
                        void (*purge_cb)(void *, int), void (*detach_cb)(void *),
                        void (*detach_post)(void *, u32), void (*down_pre)(void),
                        void (*down_post)(void *), void (*up_cb)(void *),
                        int (*ioctl_pre)(u32, void *, void *, int *),
                        int (*ioctl_so)(void *, int *),
                        int (*ioctl_post)(void *, u32, void *, void *))
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

    u8 *ifp = (u8 *)g_ifnet_head;
    while (ifp != NULL) {
        attach_cb(ifp);
        if (*(u16 *)(ifp + 48) & 1)
            up_cb(ifp);
        ifp = *(u8 **)(ifp + 4);
    }
    g_if_hooks_reg = 1;
    return 0;
}

int sceNet_lib_4C8FD452(void)
{
    u8 *ifp = (u8 *)g_ifnet_head;
    g_if_hooks_reg = 0;
    while (ifp != NULL) {
        int s = sceNet_lib_94B44F26();
        sceNet_lib_EDB11CB4(ifp);
        u32 *ifa = *(u32 **)(ifp + 12);
        while (ifa != NULL) {
            u8 family = *(u8 *)(ifa[0] + 1);
            if (family == 18) {
                g_if_detach_addr_cb(ifa);
                ifa = (u32 *)ifa[4];
            } else {
                g_if_purge_cb(ifp, family);
                ifa = *(u32 **)(ifp + 12);
            }
        }
        g_if_detach_cb(ifp);
        g_if_detach_post_cb(ifp, 0);
        sceNet_lib_515B2F33(s);
        ifp = *(u8 **)(ifp + 4);
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

static int ifioctl(void *so, u32 cmd, void *data)
{
    u8 *ifp = (u8 *)sceNet_lib_1D10419C((const char *)data);
    if (ifp == NULL)
        return 6;

    int s = sceNet_lib_94B44F26();
    *(u32 *)(ifp + 264) = 0;
    sceNet_lib_515B2F33(s);

    if (g_if_hooks_reg > 0) {
        int handled = 0;
        int ret = g_if_ioctl_pre_cb(cmd, data, data, &handled);
        if (handled > 0)
            return ret;
    }

    int (*ioctl_fn)(void *, u32, void *) = *(int (**)(void *, u32, void *))(ifp + 188);

    switch (cmd) {
    case 0xC0206911u: /* SIOCGIFFLAGS */
        *(u16 *)((u8 *)data + 16) = *(u16 *)(ifp + 48);
        return 0;
    case 0x80206910u: /* SIOCSIFFLAGS */ {
        u16 oldf = *(u16 *)(ifp + 48);
        u16 newf = *(u16 *)((u8 *)data + 16);
        if ((oldf & 1) && !(newf & 1)) {
            int s2 = sceNet_lib_94B44F26();
            sceNet_lib_EDB11CB4(ifp);
            sceNet_lib_515B2F33(s2);
        }
        if ((newf & 1) && !(*(u16 *)(ifp + 48) & 1)) {
            int s2 = sceNet_lib_94B44F26();
            *(u16 *)(ifp + 48) |= 1;
            if (g_if_hooks_reg > 0)
                g_if_up_cb(ifp);
            sceNet_lib_515B2F33(s2);
        }
        *(u16 *)(ifp + 48) = (u16)((*(u16 *)(ifp + 48) & 0x8E52) | (newf & 0x71AD));
        if (ioctl_fn != NULL)
            ioctl_fn(ifp, cmd, data);
        return 0;
    }
    case 0x80206918u: /* SIOCSIFMETRIC */
        *(u64 *)(ifp + 72) = *(s32 *)((u8 *)data + 16);
        return 0;
    case 0xC0206917u: /* SIOCGIFMETRIC */
        *(u32 *)((u8 *)data + 16) = *(u32 *)(ifp + 72);
        return 0;
    case 0xC020697Eu: /* SIOCGIFMTU */
        *(u32 *)((u8 *)data + 16) = *(u32 *)(ifp + 64);
        return 0;
    case 0xC020693Bu: /* SIOCGIFDATA */
        sceKernelMemcpy(*(void **)((u8 *)data + 16), ifp + 56, 120);
        return 0;
    case 0xC020693Du: /* dequeue event */ {
        u32 *out = *(u32 **)((u8 *)data + 16);
        int intr = sceKernelCpuSuspendIntr();
        u32 *ev = *(u32 **)(ifp + 272);
        if (ev == NULL) {
            sceKernelCpuResumeIntr(intr);
            return 2;
        }
        u32 next = ev[0];
        if (next != 0)
            *(u32 *)(next + 4) = ev[1];
        else
            *(u32 *)(ifp + 276) = ev[1];
        *(u32 *)ev[1] = next;
        sceKernelCpuResumeIntr(intr);
        out[0] = ev[2];
        out[1] = ev[3];
        sceNetIfhandle_lib_76BAD213(ev);
        return 0;
    }
    case 0x8020693Eu: /* set event filter */ {
        u32 *in = *(u32 **)((u8 *)data + 16);
        int intr = sceKernelCpuSuspendIntr();
        *(u32 *)(ifp + 280) = in[0];
        *(u32 *)(ifp + 284) = in[1];
        if (in[0] == 0 || in[1] == 0)
            free_if_events((u32 *)ifp);
        sceKernelCpuResumeIntr(intr);
        return 0;
    }
    case 0xC020693Eu: /* get event filter */ {
        u32 *out = *(u32 **)((u8 *)data + 16);
        out[0] = *(u32 *)(ifp + 280);
        out[1] = *(u32 *)(ifp + 284);
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
        return ioctl_fn(ifp, cmd, data);
    default:
        break;
    }

    if (so != NULL && g_if_hooks_reg > 0) {
        int handled = 0;
        int ret = g_if_ioctl_so_cb(so, &handled);
        if (handled > 0)
            return ret;
        return g_if_ioctl_post_cb(so, cmd, data, ifp);
    }

    if (ioctl_fn == NULL)
        return 95;
    return ioctl_fn(ifp, cmd, data);
}

/* =========================================================================
 * Ethernet layer
 * ========================================================================= */

static int ether_output(void *ifp, void *m, void *dst, void *rt)
{
    if (g_ether_hooks_reg > 0)
        return g_ether_output_cb(ifp, m, dst, rt);
    return 0;
}

static void ether_input(void *ifp_v, void *m_v)
{
    u8 *ifp = (u8 *)ifp_v;
    u8 *m = (u8 *)m_v;
    void *inq = NULL;
    int isr = 0;

    if ((*(u16 *)(ifp + 48) & 1) == 0) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    u8 *eh = *(u8 **)(m + 8);
    u16 etype = (u16)(((u16)eh[12] << 8) | eh[13]);
    u64 max_len = *(u64 *)(ifp + 64) + 14;
    if (*(u16 *)(m + 18) & 0x1000)
        max_len += 4;
    if (etype == 0x8100)
        max_len += 4;
    u32 pkthdr_len = *(u32 *)(m + 24);
    if ((u64)pkthdr_len > max_len) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    *(u64 *)(ifp + 128) += pkthdr_len;
    if (eh[0] & 1) {
        *(u64 *)(ifp + 144) += 1;
    } else if ((*(u16 *)(ifp + 48) & 0x100) != 0) {
        u8 *sdl = *(u8 **)(ifp + 224);
        if (sceNet_lib_E0A81C7C(sdl + sdl[5] + 8, eh, 6) != 0) {
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }
    }

    if (etype == 0x88C8) {
        int s = sceNet_lib_94B44F26();
        if (g_eapol_hook_reg > 0) {
            if (g_eapol_input_cb(ifp, m, &inq) >= 0) {
                u32 *q = (u32 *)inq;
                if (q[2] < q[3]) {
                    *(u32 *)(m + 4) = 0;
                    if (q[1] != 0)
                        *(u32 *)(q[1] + 4) = (u32)m;
                    else
                        q[0] = (u32)m;
                    q[1] = (u32)m;
                    q[2]++;
                } else {
                    q[4]++;
                    sceNetIfhandle_lib_F56FAC82(m);
                }
                sceNet_lib_515B2F33(s);
                sceNet_lib_7C3B86C5(29);
                return;
            }
            sceNet_lib_515B2F33(s);
            return;
        }
        sceNet_lib_515B2F33(s);
    }

    if (g_ether_hooks_reg > 0) {
        int handled = 0;
        g_ether_input_pre_cb(ifp, m, etype, &handled);
        if (handled > 0)
            return;
    }

    sceNetIfhandle_lib_BFF3CEA5(m, 14);
    if (*(u16 *)(m + 18) & 0x1000)
        sceNetIfhandle_lib_BFF3CEA5(m, -4);

    if (g_ether_hooks_reg > 0) {
        int handled = 0;
        g_ether_input_dispatch_cb(ifp, m, etype, eh, &inq, &isr, &handled);
        if (handled > 0)
            return;
    }

    if (inq == NULL) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    int s = sceNet_lib_94B44F26();
    u32 *q = (u32 *)inq;
    if (q[2] < q[3]) {
        *(u32 *)(m + 4) = 0;
        if (q[1] != 0)
            *(u32 *)(q[1] + 4) = (u32)m;
        else
            q[0] = (u32)m;
        q[1] = (u32)m;
        q[2]++;
        sceNet_lib_7C3B86C5(isr);
    } else {
        q[4]++;
        sceNetIfhandle_lib_F56FAC82(m);
    }
    sceNet_lib_515B2F33(s);
}

static void ether_ifattach(void *ifp_v, const void *lla)
{
    u8 *ifp = (u8 *)ifp_v;
    ifp[56] = 6;
    ifp[57] = 6;
    ifp[58] = 14;
    *(u32 *)(ifp + 64) = 1500;
    *(u32 *)(ifp + 68) = 0;
    *(u32 *)(ifp + 176) = (u32)ether_output;
    *(u32 *)(ifp + 180) = (u32)ether_input;
    if (*(u64 *)(ifp + 80) == 0)
        *(u64 *)(ifp + 80) = 10000000ULL;
    u8 *sdl = *(u8 **)(ifp + 224);
    if (sdl != NULL && sdl[1] == 18) {
        sdl[4] = 6;
        sdl[6] = ifp[57];
        sceNet_lib_2F305274(lla, sdl + sdl[5] + 8, ifp[57]);
    }
    *(const void **)(ifp + 228) = g_ether_broadcast;
}

static void ether_ifdetach(void *ifp_v)
{
    u8 *ifp = (u8 *)ifp_v;
    u8 *sdl = *(u8 **)(ifp + 224);
    sceKernelMemset(sdl + sdl[5] + 8, 0, 6);
    sdl[4] = 0;
    sdl[6] = 0;
}

static int ether_multiaddr(void *sa, u8 *addrlo, u8 *addrhi)
{
    if (g_ether_hooks_reg > 0) {
        int handled = 0;
        int ret = g_ether_multi_cb(sa, addrlo, addrhi, &handled);
        if (handled > 0)
            return ret;
    }
    if (((u8 *)sa)[1] != 0)
        return 106;
    sceNet_lib_2F305274((u8 *)sa + 2, addrlo, 6);
    sceNet_lib_2F305274(addrlo, addrhi, 6);
    return 0;
}

void *sceNet_lib_D60225A3(const void *mac)
{
    u8 *ifp = (u8 *)g_ifnet_head;
    while (ifp != NULL) {
        u8 *sdl = *(u8 **)(ifp + 224);
        if (sceNet_lib_E0A81C7C(sdl + sdl[5] + 8, mac, 6) == 0)
            return ifp;
        ifp = *(u8 **)(ifp + 4);
    }
    return NULL;
}

int sceNet_lib_5ED457BE(int (*out_cb)(void *, void *, void *, void *),
                        void (*in_pre)(void *, void *, u16, int *),
                        void (*in_disp)(void *, void *, u16, u8 *, void **, int *, int *),
                        int (*multi_cb)(void *, void *, void *, int *))
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

int sceNet_lib_1F94AFD9(int (*cb)(void *, void *, void **))
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

static int ether_ioctl(void *ifp_v, u32 cmd, void *data_v)
{
    u8 *ifp = (u8 *)ifp_v;
    u32 *data = (u32 *)data_v;
    int err = 0;
    int drvc = 0;

    if (ifp == NULL)
        return 22;

    int s = sceNet_lib_94B44F26();

    switch (cmd) {
    case 0x8020690Cu: /* SIOCSIFADDR */ {
        u8 *sa = (u8 *)data[0];
        if (sa[1] == 2) {
            if (g_arp_hooks_reg > 0)
                g_arp_ifinit_cb(ifp, data);
        } else if (sa[1] == 18) {
            if (sa[4] == 6 && sa[6] == ifp[57]) {
                u8 *sdl = *(u8 **)(ifp + 224);
                sceKernelMemcpy(sdl + sdl[5] + 8, sa + sa[5] + 8, sa[6]);
            } else {
                err = 22;
            }
        }
        (*(u32 *)(ifp + 264))++;
        sceNetIfhandle_lib_8FCB05A1(ifp);
        *(u16 *)(ifp + 48) |= 0x41;
        break;
    }
    case 0x80206910u: /* SIOCSIFFLAGS */ {
        u16 f = *(u16 *)(ifp + 48);
        if ((f & 0x41) == 0x40) {
            (*(u32 *)(ifp + 264))++;
            sceNetIfhandle_lib_EAD3A759(ifp);
            *(u16 *)(ifp + 48) &= ~0x40u;
        } else if ((f & 0x41) == 1) {
            (*(u32 *)(ifp + 264))++;
            sceNetIfhandle_lib_8FCB05A1(ifp);
            *(u16 *)(ifp + 48) |= 0x40;
        } else if ((f & 1) && (f & 0x100)) {
            (*(u32 *)(ifp + 264))++;
            sceNetIfhandle_lib_0296C7D6(ifp, 5, data);
        }
        break;
    }
    case 0x80206931u: /* SIOCADDMULTI */
    case 0x80206932u: /* SIOCDELMULTI */ {
        u8 lo[6], hi[6];
        err = ether_multiaddr(&data[4], lo, hi);
        if (err == 0) {
            if ((lo[0] & 1) == 0 || (hi[0] & 1) == 0 || sceNet_lib_E0A81C7C(lo, hi, 6) != 0) {
                err = 22;
            } else {
                (*(u32 *)(ifp + 264))++;
                sceNetIfhandle_lib_0296C7D6(ifp, (cmd == 0x80206931u) ? 2 : 3, data);
            }
        }
        break;
    }
    case 0x8020697Fu: /* SIOCSIFMTU */
        if ((s32)data[4] <= 1500) {
            (*(u32 *)(ifp + 264))++;
            sceNetIfhandle_lib_0296C7D6(ifp, 1, data);
            *(u64 *)(ifp + 64) = (s32)data[4];
        } else {
            err = 22;
        }
        break;
    case 0xC0206921u: /* SIOCGIFADDR */ {
        u8 *sdl = *(u8 **)(ifp + 224);
        sceKernelMemcpy((u8 *)data + 18, sdl + sdl[5] + 8, 6);
        break;
    }
    case 0x801669FAu:
    case 0x801669FBu:
        if ((data[4] & 1) == 0) {
            err = 22;
        } else {
            drvc = (cmd == 0x801669FAu) ? 6 : 7;
            (*(u32 *)(ifp + 264))++;
            sceNetIfhandle_lib_0296C7D6(ifp, drvc, data);
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
            (*(u32 *)(ifp + 264))++;
            sceNetIfhandle_lib_0296C7D6(ifp, drvc, data);
        }
        break;
    }

    sceNet_lib_515B2F33(s);
    return err;
}

static void ether_start(void *ifp_v)
{
    u32 *ifp = (u32 *)ifp_v;
    while (ifp[51] != 0) {
        u32 *m = (u32 *)ifp[51];
        ifp[51] = m[1];
        if (ifp[51] == 0)
            ifp[52] = 0;
        m[1] = 0;
        ifp[53]--;
        if (ifp[10] != 0)
            bpf_mtap((void *)ifp[10], m);
        sceNetIfhandle_lib_B1F5BB87(ifp, m);
    }
}

int sceNet_lib_991FF86D(void (*cb)(void *, void *))
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

static int ppp_output(void *ifp, void *m, void *dst, void *rt)
{
    if (g_ppp_hooks_reg > 0)
        return g_ppp_output_cb(ifp, m, dst, rt);
    return 0;
}

static int ppp_input(void *ifp, void *m)
{
    if (g_ppp_hooks_reg > 0)
        return g_ppp_input_cb(ifp, m);
    return 0;
}

static void ppp_start(void *ifp_v)
{
    u8 *ifp = (u8 *)ifp_v;
    if (g_ppp_hooks_reg <= 0)
        return;
    for (;;) {
        void *m = g_ppp_dequeue_cb(ifp);
        if (m == NULL)
            break;
        if (*(void **)(ifp + 40) != NULL)
            bpf_mtap(*(void **)(ifp + 40), m);
        sceNetIfhandle_lib_B1F5BB87(ifp, m);
    }
}

static int ppp_sync_to_drv(void *ifp_v)
{
    u32 *sc = (u32 *)ifp_v;
    u32 pkt[15];
    sc[156] = sc[2284];
    sc[157] = sc[2276];
    sceKernelMemset(pkt, 0, sizeof(pkt));
    pkt[0] = sc[147];
    pkt[1] = sc[148];
    pkt[2] = sc[158];
    pkt[3] = sc[159];
    pkt[4] = sc[160];
    pkt[5] = sc[161];
    sceKernelMemcpy(&pkt[6], &sc[2276], 32);
    pkt[14] = sc[2284];
    return sceNetIfhandle_lib_0296C7D6(sc, 100, pkt);
}

static int ppp_sync_from_drv(void *ifp_v)
{
    u32 *sc = (u32 *)ifp_v;
    u32 pkt[15];
    sc[2284] = sc[156];
    sc[2276] = sc[157];
    sceKernelMemset(pkt, 0, sizeof(pkt));
    pkt[0] = sc[147];
    pkt[1] = sc[148];
    pkt[2] = sc[158];
    pkt[3] = sc[159];
    pkt[4] = sc[160];
    pkt[5] = sc[161];
    sceKernelMemcpy(&pkt[6], &sc[2276], 32);
    pkt[14] = sc[2284];
    return sceNetIfhandle_lib_0296C7D6(sc, 100, pkt);
}

static int ppp_ioctl(void *ifp_v, u32 cmd, void *data_v)
{
    u32 *ifp = (u32 *)ifp_v;
    u32 *sc = (u32 *)ifp[0];
    u8 *data = (u8 *)data_v;

    switch (cmd) {
    case 0x80206986u: {
        void (*fn)(void *) = (void (*)(void *))(*(u32 *)(data + 16) ? sc[2268] : sc[2269]);
        fn(sc);
        return 0;
    }
    case 0x80146983u:
        sc[2276] = *(u32 *)(data + 16);
        return 0;
    case 0x80146985u:
        sc[2284] = *(u32 *)(data + 16);
        return 0;
    case 0xC0146982u:
        *(u32 *)(data + 16) = sc[2276];
        return 0;
    case 0xC0146984u:
        *(u32 *)(data + 16) = sc[2284];
        return 0;
    default:
        break;
    }

    int err = 95;
    if (g_ppp_hooks_reg > 0)
        err = g_ppp_ioctl_cb(ifp, cmd, data);
    if (err == 0) {
        if (cmd == 0x80206931u) {
            ifp[66]++;
            sceNetIfhandle_lib_0296C7D6(ifp, 2, data);
        } else if (cmd == 0x80206932u) {
            ifp[66]++;
            sceNetIfhandle_lib_0296C7D6(ifp, 3, data);
        }
    }
    return err;
}

static int ppp_attach(void *ifp_v)
{
    u8 *ifp = (u8 *)ifp_v;
    ifp[56] = 23;
    ifp[58] = 4;
    *(u32 *)(ifp + 64) = 1500;
    *(u32 *)(ifp + 68) = 0;
    *(u16 *)(ifp + 48) = 0x8010;
    *(u32 *)(ifp + 176) = (u32)ppp_output;
    *(u32 *)(ifp + 180) = (u32)ppp_input;
    *(u32 *)(ifp + 184) = (u32)ppp_start;
    *(u32 *)(ifp + 188) = (u32)ppp_ioctl;
    *(u32 *)(ifp + 316) |= 1;
    *(u32 *)(ifp + 9080) = (u32)ppp_sync_to_drv;
    *(u32 *)(ifp + 9096) = (u32)ppp_sync_from_drv;
    *(u32 *)(ifp + 0) = (u32)ifp;
    sceNet_lib_8B615F52(ifp + 9104, 32);
    *(u32 *)(ifp + 9104) = 0xFFFFFFFFu;
    *(u32 *)(ifp + 9116) = 0x60000000u;
    *(u32 *)(ifp + 9136) = 0;
    if (g_ppp_hooks_reg > 0)
        return g_ppp_attach_cb(ifp);
    return 0x60000000;
}

static int ppp_detach(void *ifp)
{
    if (g_ppp_hooks_reg > 0)
        return g_ppp_detach_cb(ifp);
    return 0;
}

int sceNet_lib_B51452D2(int (*att)(void *), int (*det)(void *),
                        int (*out)(void *, void *, void *, void *),
                        int (*in)(void *, void *),
                        int (*ioc)(void *, u32, void *),
                        void *(*deq)(void *))
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

static void prng_twist(void)
{
    int i;
    for (i = 0; i < 32; i++)
        g_prng_state[i] ^= g_prng_state[i + 489];
    for (i = 32; i < 521; i++)
        g_prng_state[i] ^= g_prng_state[i - 32];
}

static void net_prng_seed(u32 seed)
{
    int i, j;
    u32 x = 0;
    for (i = 0; i < 17; i++) {
        for (j = 0; j < 32; j++) {
            seed = seed * 1566083941u + 1;
            x = (x >> 1) | (seed & 0x80000000u);
        }
        g_prng_state[i] = x;
    }
    g_prng_state[16] = (g_prng_state[16] << 23) ^ (g_prng_state[0] >> 9) ^ g_prng_state[15];
    for (i = 17; i < 521; i++)
        g_prng_state[i] = (g_prng_state[i - 17] << 23) ^ (g_prng_state[i - 16] >> 9) ^ g_prng_state[i - 1];
    prng_twist();
    prng_twist();
    prng_twist();
    g_prng_idx = 520;
}

int sceNet_lib_1858883D(void)
{
    if (++g_prng_idx >= 521) {
        prng_twist();
        g_prng_idx = 0;
    }
    return (int)g_prng_state[g_prng_idx];
}

u32 sceNet_lib_75D9985C(void)
{
    u8 sysclock[16];
    u32 sec = 0, usec = 0;
    sceKernelGetSystemTime(sysclock);
    sceKernelSysClock2USec(sysclock, &sec, &usec);
    return sec;
}

int sceNet_lib_8D34E686(u8 *ctx, const u8 *key, int key_len)
{
    u8 k_ipad[65];
    u8 tk[20];
    u8 sha_tmp[96];
    int i;

    if (key_len > 64) {
        sceKernelUtilsSha1BlockInit(sha_tmp);
        sceKernelUtilsSha1BlockUpdate(sha_tmp, key, (u32)key_len);
        sceKernelUtilsSha1BlockResult(sha_tmp, tk);
        key = tk;
        key_len = 20;
    }
    sceKernelMemset(k_ipad, 0, 65);
    sceKernelMemset(ctx + 96, 0, 65);
    sceKernelMemcpy(k_ipad, key, (u32)key_len);
    sceKernelMemcpy(ctx + 96, key, (u32)key_len);
    for (i = 0; i < 64; i++) {
        k_ipad[i] ^= 0x36;
        ctx[96 + i] ^= 0x5C;
    }
    sceKernelUtilsSha1BlockInit(ctx);
    return sceKernelUtilsSha1BlockUpdate(ctx, k_ipad, 64);
}

int sceNet_lib_3A0668D5(void *ctx, const void *data, u32 len)
{
    return sceKernelUtilsSha1BlockUpdate(ctx, data, len);
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

int sceNet_lib_384EFE14(const void *data, u32 data_len, const u8 *key, int key_len, u8 *digest)
{
    u8 ctx[96];
    u8 k_opad[65];
    u8 k_ipad[65];
    u8 tk[20];
    u8 sha_tmp[96];
    u8 inner[20];
    int i;

    if (key_len > 64) {
        sceKernelUtilsSha1BlockInit(sha_tmp);
        sceKernelUtilsSha1BlockUpdate(sha_tmp, key, (u32)key_len);
        sceKernelUtilsSha1BlockResult(sha_tmp, tk);
        key = tk;
        key_len = 20;
    }
    sceKernelMemset(k_ipad, 0, 65);
    sceKernelMemset(k_opad, 0, 65);
    sceKernelMemcpy(k_ipad, key, (u32)key_len);
    sceKernelMemcpy(k_opad, key, (u32)key_len);
    for (i = 0; i < 64; i++) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5C;
    }
    sceKernelUtilsSha1BlockInit(ctx);
    sceKernelUtilsSha1BlockUpdate(ctx, k_ipad, 64);
    sceKernelUtilsSha1BlockUpdate(ctx, data, data_len);
    sceKernelUtilsSha1BlockResult(ctx, inner);
    sceKernelUtilsSha1BlockInit(ctx);
    sceKernelUtilsSha1BlockUpdate(ctx, k_opad, 64);
    sceKernelUtilsSha1BlockUpdate(ctx, inner, 20);
    return sceKernelUtilsSha1BlockResult(ctx, digest);
}

int sceNetEtherNtostr(const u8 *mac, char *str)
{
    if (mac == NULL || str == NULL)
        return 0;
    return sceNet_lib_CF705E46(str, "%02x:%02x:%02x:%02x:%02x:%02x",
                               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static int hex_val(char c)
{
    if ((u8)(c - '0') < 10)
        return c - '0';
    if ((u8)(c - 'A') < 26)
        return c - 'A' + 10;
    if ((u8)(c - 'a') < 26)
        return c - 'a' + 10;
    return 9999999;
}

int sceNetEtherStrton(const char *str, u8 *mac)
{
    char tmp[3];
    int i;
    if (str == NULL || mac == NULL)
        return 0;
    for (i = 0; i < 6; i++) {
        sceKernelMemcpy(tmp, str, 2);
        tmp[2] = '\0';
        mac[i] = (u8)(hex_val(str[0]) * 16 + hex_val(str[1]));
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
