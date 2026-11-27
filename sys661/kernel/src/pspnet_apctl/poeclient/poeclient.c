#include "pspnet_apctl_internal.h"

typedef struct PppoeConfig {
    u32 dns_flag;
    char dns_a[16];
    char dns_b[16];
    char user[128];
    char pass[128];
} PppoeConfig;

typedef struct PppoeCtx {
    int sock;
    SceUID sema;
    SceUID thid;
    void (*cb)(int, int);
    PppoeConfig cfg;
} PppoeCtx;

static PppoeCtx *g_pppoe = NULL;
static PppoeCtx g_pppoe_storage;

static int pppoe_get_error(void)
{
    struct { char ifname[16]; u32 status; } req_st, req_err;
    struct { char ifname[16]; u32 code; u8 pad[256]; } req_sub;
    int err = 0;

    sceKernelMemset(&req_st, 0, sizeof(req_st));
    sceNet_lib_B5CE388A(req_st.ifname, "pppoe0", 16);
    if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0x80146972u, &req_st) < 0)
        return (int)0x80410380;

    switch (req_st.status) {
    case 0: err = (int)0x80410301; break;
    case 1: err = (int)0x80410302; break;
    case 2:
    case 4:
        sceKernelMemset(&req_sub, 0, sizeof(req_sub));
        sceNet_lib_B5CE388A(req_sub.ifname, "pppoe0", 16);
        if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0xC1146974u, &req_sub) < 0)
            return (int)0x80410380;
        if (req_sub.code == 513) err = (int)0x80410305;
        else if (req_sub.code == 514) err = (int)0x80410306;
        else if (req_sub.code == 515) err = (int)0x80410307;
        break;
    case 3: err = (int)0x80410303; break;
    case 6: err = (int)0x80410304; break;
    default: break;
    }

    sceKernelMemset(&req_err, 0, sizeof(req_err));
    sceNet_lib_B5CE388A(req_err.ifname, "pppoe0", 16);
    if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0xC014697Du, &req_err) < 0)
        return (int)0x80410380;

    switch (req_err.status) {
    case 0: case 1: return err;
    case 2: return (int)0x8041030A;
    case 3: return (int)0x80410308;
    case 4: return (int)0x80410309;
    default: return 0;
    }
}

static int pppoe_thread(SceSize args, void *argp)
{
    u32 own_ip = 0, peer_ip = bswap32(1u);
    struct { char ifname[16]; u32 up; } req_up;
    struct { char ifname[16]; u32 flags; u32 lip; u32 pip; u32 dns_a; u32 dns_b; } req_info;
    int ret;
    int notify_err = 0;
    (void)args;
    (void)argp;

    do {
        ret = sceNetInet_lib_4F68DB0E("pppoe0", &own_ip, &peer_ip, NULL);
        if (ret < 0) {
            notify_err = 1;
            break;
        }

        sceKernelMemset(&req_up, 0, sizeof(req_up));
        sceNet_lib_B5CE388A(req_up.ifname, "pppoe0", 16);
        sceKernelMemset(&req_info, 0, sizeof(req_info));
        sceNet_lib_B5CE388A(req_info.ifname, "pppoe0", 16);

        {
            int tear_down = 0;
            for (;;) {
                if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0xC014697Bu, &req_up) < 0) {
                    ret = (int)0x80410380;
                    notify_err = 1;
                    break;
                }
                if (req_up.up == 0) {
                    ret = pppoe_get_error();
                    notify_err = 1;
                    break;
                }
                if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0xC0246978u, &req_info) < 0) {
                    ret = (int)0x80410380;
                    notify_err = 1;
                    break;
                }
                ret = sceKernelPollSema(g_pppoe->sema, 1);
                if (ret == 0) {
                    tear_down = 1;
                    break;
                }
                if (ret != (int)0x800201AD) {
                    notify_err = 1;
                    break;
                }
                if (req_info.flags & 0x10)
                    break;
                sceKernelDelayThread(1000000);
            }
            if (notify_err || tear_down)
                break;
        }

        own_ip = bswap32(req_info.lip);
        peer_ip = bswap32(req_info.pip);
        ret = sceNetInet_lib_4F68DB0E("pppoe0", &own_ip, &peer_ip, NULL);
        if (ret < 0) {
            notify_err = 1;
            break;
        }
        ret = sceNetInet_lib_7776A492(&peer_ip);
        if (ret < 0) {
            notify_err = 1;
            break;
        }

        {
            u32 dns_a = 0, dns_b = 0;
            if (g_pppoe->cfg.dns_flag != 0) {
                sceNetInetInetAton(g_pppoe->cfg.dns_a, &dns_a);
                if (dns_a != 0) {
                    sceNetInetInetAton(g_pppoe->cfg.dns_b, &dns_b);
                    if (dns_b == 0 && (req_info.flags & 4))
                        dns_b = bswap32(req_info.dns_a);
                }
            } else {
                if (req_info.flags & 4)
                    dns_a = bswap32(req_info.dns_a);
                if (req_info.flags & 8) {
                    if (dns_a == 0)
                        dns_a = bswap32(req_info.dns_b);
                    else
                        dns_b = bswap32(req_info.dns_b);
                }
            }
            ret = sceNetInet_lib_AAF4895A(&dns_a, &dns_b);
            if (ret < 0) {
                notify_err = 1;
                break;
            }
        }

        sceNetInet_lib_A94A75E7(&own_ip);
        if (g_pppoe->cb != NULL)
            g_pppoe->cb(1, 0);

        for (;;) {
            if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0xC014697Bu, &req_up) < 0) {
                ret = (int)0x80410380;
                notify_err = 1;
                break;
            }
            if (req_up.up == 0) {
                ret = pppoe_get_error();
                notify_err = 1;
                break;
            }
            ret = sceKernelPollSema(g_pppoe->sema, 1);
            if (ret == 0)
                break;
            if (ret != (int)0x800201AD) {
                notify_err = 1;
                break;
            }
            sceKernelDelayThread(1000000);
        }
    } while (0);

    if (notify_err) {
        if (g_pppoe->cb != NULL)
            g_pppoe->cb(3, ret);
        sceKernelWaitSema(g_pppoe->sema, 1, NULL);
    }

    sceNet_lib_D2422E4D("pppoe0");
    sceKernelDelayThread(100000);
    sceNetInet_lib_6A046357("pppoe0");
    if (g_pppoe->cb != NULL)
        g_pppoe->cb(2, ret);
    return 0;
}

int pppoe_start(const char *ifname, void (*cb)(int, int), int stack, int prio, int unused, void *cfg)
{
    struct { char ifname[16]; u32 echo_int; u32 echo_fail; u32 req_retry; u32 req_timeout; u32 pad; } req_lcp;
    struct { char ifname[16]; char user[128]; char pass[128]; u32 timeout; u32 retry; } req_auth;
    const PppoeConfig *pcfg = (const PppoeConfig *)cfg;
    int ret;
    int stage = 0;
    (void)unused;

    if (g_pppoe != NULL)
        return (int)0x80000020;

    g_pppoe = &g_pppoe_storage;
    sceKernelMemset(g_pppoe, 0, sizeof(PppoeCtx));

    do {
        ret = sceNetInet_lib_7CB1D9E3(ifname);
        if (ret < 0)
            break;
        stage = 1;

        g_pppoe->cb = cb;
        sceKernelMemcpy(&g_pppoe->cfg, pcfg, sizeof(PppoeConfig));

        g_pppoe->sock = sceNetInetSocket(2, 2, 0);
        if (g_pppoe->sock < 0) {
            ret = (int)0x80410381;
            break;
        }
        stage = 2;

        sceKernelMemset(&req_lcp, 0, sizeof(req_lcp));
        sceNet_lib_B5CE388A(req_lcp.ifname, "pppoe0", 16);
        req_lcp.echo_int = 100;
        req_lcp.echo_fail = 2;
        req_lcp.req_retry = 10;
        req_lcp.req_timeout = 10;
        if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0x8024697Au, &req_lcp) < 0) {
            ret = (int)0x80410382;
            break;
        }

        sceKernelMemset(&req_auth, 0, sizeof(req_auth));
        sceNet_lib_B5CE388A(req_auth.ifname, "pppoe0", 16);
        sceKernelMemcpy(req_auth.user, pcfg->user, 128);
        sceKernelMemcpy(req_auth.pass, pcfg->pass, 128);
        req_auth.timeout = 300;
        req_auth.retry = 10;
        if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0x81186979u, &req_auth) < 0) {
            ret = (int)0x80410382;
            break;
        }

        g_pppoe->sema = sceKernelCreateSema("SceNetPppoeClient", 0, 0, 1, NULL);
        if (g_pppoe->sema < 0) {
            ret = g_pppoe->sema;
            break;
        }
        stage = 3;

        g_pppoe->thid = sceKernelCreateThread("SceNetPppoeClient", pppoe_thread, prio, stack, 0, NULL);
        if (g_pppoe->thid < 0) {
            ret = g_pppoe->thid;
            break;
        }

        ret = sceKernelStartThread(g_pppoe->thid, 0, NULL);
        if (ret >= 0)
            return 0;

        sceKernelDeleteThread(g_pppoe->thid);
    } while (0);

    if (stage >= 3)
        sceKernelDeleteSema(g_pppoe->sema);
    if (stage >= 2)
        sceNetInetClose(g_pppoe->sock);
    if (stage >= 1)
        sceNetInet_lib_4C13BE10();
    g_pppoe = NULL;
    return ret;
}

int pppoe_stop_signal(void)
{
    if (g_pppoe == NULL)
        return (int)0x8041030B;
    return sceKernelSignalSema(g_pppoe->sema, 1);
}

int pppoe_stop_wait(void)
{
    if (g_pppoe == NULL)
        return (int)0x8041030B;
    sceKernelWaitThreadEnd(g_pppoe->thid, NULL);
    sceNetFreeThreadinfo(g_pppoe->thid);
    sceKernelDeleteThread(g_pppoe->thid);
    sceKernelDeleteSema(g_pppoe->sema);
    sceNetInetClose(g_pppoe->sock);
    sceNetInet_lib_4C13BE10();
    g_pppoe = NULL;
    return 0;
}
