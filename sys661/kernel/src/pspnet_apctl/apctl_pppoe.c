#include "apctl_internal.h"

typedef struct PppoeCtx {
    int sock;
    SceUID sema;
    SceUID thid;
    void (*cb)(int, int);
    u32 dns_flag;
    char dns1[16];
    char dns2[16];
    char user[128];
    char pass[128];
} PppoeCtx;

static PppoeCtx *g_pppoe = NULL;
static PppoeCtx g_pppoe_storage;

static int pppoe_get_error(void)
{
    struct { char ifname[16]; u32 val; } req_st, req_err;
    struct { char ifname[16]; u32 code; u8 pad[256]; } req_sub;
    int err = 0;

    sceKernelMemset(&req_st, 0, sizeof(req_st));
    sceNet_lib_B5CE388A(req_st.ifname, "pppoe0", 16);
    if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0x80146972u, &req_st) < 0)
        return (int)0x80410380;

    switch (req_st.val) {
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

    switch (req_err.val) {
    case 0: case 1: return err;
    case 2: return (int)0x8041030A;
    case 3: return (int)0x80410308;
    case 4: return (int)0x80410309;
    default: return 0;
    }
}

static int pppoe_thread(SceSize args, void *argp)
{
    u32 local_ip = 0, peer_ip = bswap32(1u);
    struct { char ifname[16]; u32 up; } req_up;
    struct { char ifname[16]; u32 flags; u32 lip; u32 pip; u32 dns1; u32 dns2; } req_info;
    int ret;
    (void)args;
    (void)argp;

    ret = sceNetInet_lib_4F68DB0E("pppoe0", &local_ip, &peer_ip, NULL);
    if (ret < 0)
        goto notify_err;

    sceKernelMemset(&req_up, 0, sizeof(req_up));
    sceNet_lib_B5CE388A(req_up.ifname, "pppoe0", 16);
    sceKernelMemset(&req_info, 0, sizeof(req_info));
    sceNet_lib_B5CE388A(req_info.ifname, "pppoe0", 16);

    for (;;) {
        if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0xC014697Bu, &req_up) < 0) {
            ret = (int)0x80410380;
            goto notify_err;
        }
        if (req_up.up == 0) {
            ret = pppoe_get_error();
            goto notify_err;
        }
        if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0xC0246978u, &req_info) < 0) {
            ret = (int)0x80410380;
            goto notify_err;
        }
        ret = sceKernelPollSema(g_pppoe->sema, 1);
        if (ret == 0)
            goto tear_down;
        if (ret != (int)0x800201AD)
            goto notify_err;
        if (req_info.flags & 0x10)
            break;
        sceKernelDelayThread(1000000);
    }

    local_ip = bswap32(req_info.lip);
    peer_ip = bswap32(req_info.pip);
    ret = sceNetInet_lib_4F68DB0E("pppoe0", &local_ip, &peer_ip, NULL);
    if (ret < 0)
        goto notify_err;
    ret = sceNetInet_lib_7776A492(&peer_ip);
    if (ret < 0)
        goto notify_err;

    {
        u32 d1 = 0, d2 = 0;
        if (g_pppoe->dns_flag != 0) {
            sceNetInetInetAton(g_pppoe->dns1, &d1);
            if (d1 != 0) {
                sceNetInetInetAton(g_pppoe->dns2, &d2);
                if (d2 == 0 && (req_info.flags & 4))
                    d2 = bswap32(req_info.dns1);
            }
        } else {
            if (req_info.flags & 4)
                d1 = bswap32(req_info.dns1);
            if (req_info.flags & 8) {
                if (d1 == 0) d1 = bswap32(req_info.dns2);
                else d2 = bswap32(req_info.dns2);
            }
        }
        ret = sceNetInet_lib_AAF4895A(&d1, &d2);
        if (ret < 0)
            goto notify_err;
    }

    sceNetInet_lib_A94A75E7(&local_ip);
    if (g_pppoe->cb != NULL)
        g_pppoe->cb(1, 0);

    for (;;) {
        if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0xC014697Bu, &req_up) < 0) {
            ret = (int)0x80410380;
            goto notify_err;
        }
        if (req_up.up == 0) {
            ret = pppoe_get_error();
            goto notify_err;
        }
        ret = sceKernelPollSema(g_pppoe->sema, 1);
        if (ret == 0)
            goto tear_down;
        if (ret != (int)0x800201AD)
            goto notify_err;
        sceKernelDelayThread(1000000);
    }

notify_err:
    if (g_pppoe->cb != NULL)
        g_pppoe->cb(3, ret);
    sceKernelWaitSema(g_pppoe->sema, 1, NULL);

tear_down:
    sceNet_lib_D2422E4D("pppoe0");
    sceKernelDelayThread(100000);
    sceNetInet_lib_6A046357("pppoe0");
    if (g_pppoe->cb != NULL)
        g_pppoe->cb(2, ret);
    return 0;
}

int pppoe_start(const char *ifname, void (*cb)(int, int), int stack, int prio, int unused, void *cfg)
{
    struct { char ifname[16]; u32 t1; u32 t2; u32 t3; u32 t4; u32 pad; } req_lcp;
    struct { char ifname[16]; char user[128]; char pass[128]; u32 t1; u32 t2; } req_auth;
    int ret;
    (void)unused;

    if (g_pppoe != NULL)
        return (int)0x80000020;

    g_pppoe = &g_pppoe_storage;
    sceKernelMemset(g_pppoe, 0, sizeof(PppoeCtx));

    ret = sceNetInet_lib_7CB1D9E3(ifname);
    if (ret < 0)
        goto fail_null;

    g_pppoe->cb = cb;
    sceKernelMemcpy(&g_pppoe->dns_flag, cfg, 292);

    g_pppoe->sock = sceNetInetSocket(2, 2, 0);
    if (g_pppoe->sock < 0) {
        ret = (int)0x80410381;
        goto fail_det;
    }

    sceKernelMemset(&req_lcp, 0, sizeof(req_lcp));
    sceNet_lib_B5CE388A(req_lcp.ifname, "pppoe0", 16);
    req_lcp.t1 = 100;
    req_lcp.t2 = 2;
    req_lcp.t3 = 10;
    req_lcp.t4 = 10;
    if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0x8024697Au, &req_lcp) < 0) {
        ret = (int)0x80410382;
        goto fail_sock;
    }

    sceKernelMemset(&req_auth, 0, sizeof(req_auth));
    sceNet_lib_B5CE388A(req_auth.ifname, "pppoe0", 16);
    sceKernelMemcpy(req_auth.user, (u8 *)cfg + 36, 128);
    sceKernelMemcpy(req_auth.pass, (u8 *)cfg + 164, 128);
    req_auth.t1 = 300;
    req_auth.t2 = 10;
    if (sceNetInet_lib_AEE60F84(g_pppoe->sock, 0x81186979u, &req_auth) < 0) {
        ret = (int)0x80410382;
        goto fail_sock;
    }

    g_pppoe->sema = sceKernelCreateSema("SceNetPppoeClient", 0, 0, 1, NULL);
    if (g_pppoe->sema < 0) {
        ret = g_pppoe->sema;
        goto fail_sock;
    }

    g_pppoe->thid = sceKernelCreateThread("SceNetPppoeClient", pppoe_thread, prio, stack, 0, NULL);
    if (g_pppoe->thid < 0) {
        ret = g_pppoe->thid;
        goto fail_sema;
    }

    ret = sceKernelStartThread(g_pppoe->thid, 0, NULL);
    if (ret >= 0)
        return 0;

    sceKernelDeleteThread(g_pppoe->thid);
fail_sema:
    sceKernelDeleteSema(g_pppoe->sema);
fail_sock:
    sceNetInetClose(g_pppoe->sock);
fail_det:
    sceNetInet_lib_4C13BE10();
fail_null:
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
