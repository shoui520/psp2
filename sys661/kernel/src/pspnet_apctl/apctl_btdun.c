#include "apctl_internal.h"

typedef struct BtdunConfig {
    int device_id;            /* 0x000 */
    char init_cmd[65];        /* 0x004 */
    char dial_num[33];        /* 0x045 */
    char username[128];       /* 0x066 */
    char password[128];       /* 0x0E6 */
    u8 pad166[2];             /* 0x166 */
    int use_static_dns;       /* 0x168 */
    char dns1[16];            /* 0x16C */
    char dns2[16];            /* 0x17C */
    void (*log_cb)(char *, int, int); /* 0x18C */
    int reserved;             /* 0x190 */
} BtdunConfig; /* 400 bytes (0x190) -> wait: 0x18C + 4 = 0x190 */

typedef struct BtdunState {
    u8 cfg_raw[400];          /* 0x000..0x18F */
    u8 pad190[4];             /* 0x190..0x193 */
    SceUID thid;              /* 0x194 (0x12F40 - 0x12DAC) */
    SceUID sema;              /* 0x198 (0x12F44 - 0x12DAC) */
    void (*cb)(int, int);     /* 0x19C (0x12F48 - 0x12DAC) */
    u8 pad1a0[4];
} BtdunState; /* 420 bytes */

typedef struct AtRespEntry {
    const char *text;
    int err;
} AtRespEntry;

static const AtRespEntry g_at_resp_table[7] = {
    { "OK",          0 },
    { "CONNECT",     0 },
    { "NO CARRIER",  (int)0x80411980 },
    { "ERROR",       (int)0x80411981 },
    { "NO DIALTONE", (int)0x80411982 },
    { "BUSY",        (int)0x80411983 },
    { "NO ANSWER",   (int)0x80411984 },
};

static int g_btdun_running = 0;
static BtdunState g_btdun;
static char g_btdun_rxbuf[128];

static int btdun_get_ppp_error(int sock)
{
    u8 req[32];
    int ret;

    sceKernelMemset(req, 0, 20);
    sceNet_lib_B5CE388A((char *)req, "btdun", 16);
    ret = sceNetInet_lib_AEE60F84(sock, 0xC014693Du, req);
    if (ret < 0)
        return (int)0x80411885;

    switch (*(int *)(req + 16)) {
    case 0:
    case 1:
        return 0;
    case 2:
        return (int)0x80411889;
    case 3:
        return (int)0x80411887;
    case 4:
        return (int)0x80411888;
    default:
        return ret;
    }
}

static int btdun_at_chat(const char *const *cmds, char *buf, int bufsize,
                         void (*log_cb)(char *, int, int))
{
    int i = 0;
    int ret;
    char *write_pos;
    int rlen;

    if (cmds[0] != NULL) {
        while (cmds[i] != NULL) {
            int len = sceNet_lib_D8722983(cmds[i]);
            ret = sceBtdun_lib_5BED7B20(cmds[i], len, 15000000u);
            if (ret < 0)
                goto done;
            i++;
        }
    }

    ret = sceBtdun_lib_5BED7B20("\r\n", sceNet_lib_D8722983("\r\n"), 15000000u);
    if (ret < 0)
        goto done;

    write_pos = buf;
    for (;;) {
        int buffered = (int)(write_pos - buf);
        int total;
        char *cur;

        rlen = bufsize - buffered;
        ret = sceBtdun_lib_966A10F5(write_pos, &rlen, 15000000u);
        if (ret < 0)
            goto done;
        if (log_cb != NULL)
            log_cb(write_pos, rlen, 0);

        cur = buf;
        total = buffered + rlen;

        for (;;) {
            int partial = 0;
            u32 idx;
            int rem;

            while (cur - buf < total && (*cur == '\r' || *cur == '\n'))
                cur++;

            rem = total - (int)(cur - buf);
            if (rem == 0) {
                sceNet_lib_4753D878(buf, cur, 0);
                write_pos = buf;
                break;
            }

            for (idx = 0; idx < 7; idx++) {
                u32 tlen = (u32)sceNet_lib_D8722983(g_at_resp_table[idx].text);
                if (tlen >= (u32)rem) {
                    if (sceNet_lib_B9085A96(cur, g_at_resp_table[idx].text, rem) == 0)
                        partial = 1;
                } else {
                    if (sceNet_lib_B9085A96(g_at_resp_table[idx].text, cur, (int)tlen) == 0)
                        break;
                }
            }

            if (idx < 7) {
                char term;
                cur += sceNet_lib_D8722983(g_at_resp_table[idx].text);
                term = *cur;
                if (term == '\r' || term == '\n' || term == ' ') {
                    ret = g_at_resp_table[idx].err;
                    goto done;
                }
            } else if (partial == 1) {
                sceNet_lib_4753D878(buf, cur, rem);
                write_pos = buf + rem;
                break;
            }

            /* Skip rest of current line */
            {
                u32 off = 0;
                u32 left = (u32)(total - (int)(cur - buf));
                while (off < left && cur[off] != '\r' && cur[off] != '\n')
                    off++;
                cur += off + 1;
                if (off == left) {
                    write_pos = buf;
                    break;
                }
            }
        }
    }

done:
    if (log_cb != NULL)
        log_cb(NULL, 0, ret);
    return ret;
}

static int btdun_ppp_session(void)
{
    int sock;
    int err;
    u32 ip_addr;
    u32 dst_addr;
    u32 peer_addr;
    u32 dns1;
    u32 dns2;
    struct {
        char ifname[16];
        int flags;
        u32 ip;
        u32 peer;
        u32 dns1;
        u32 dns2;
    } ipcp_info;
    struct {
        char ifname[16];
        u8 user[128];
        u8 pass[128];
        int timeout;
        int retry;
    } auth_req;
    struct {
        char ifname[16];
        int link_up;
    } link_req;
    struct {
        char ifname[16];
        int timeout;
        int mode;
        int retry1;
        int retry2;
        int flags;
    } lcp_req;
    struct {
        char ifname[16];
        int val;
    } opt_req;
    struct {
        char ifname[16];
        int up;
        u8 pad[12];
    } up_req;

    sock = sceNetInetSocket(2, 2, 0);
    err = (int)0x80411883;
    if (sock < 0)
        goto notify_err;

    sceKernelMemset(&lcp_req, 0, sizeof(lcp_req));
    sceNet_lib_B5CE388A(lcp_req.ifname, "btdun", 16);
    lcp_req.timeout = 300;
    lcp_req.mode = 2;
    lcp_req.retry1 = 10;
    lcp_req.retry2 = 10;
    lcp_req.flags = 7;
    if (sceNetInet_lib_AEE60F84(sock, 0x8024692Au, &lcp_req) < 0)
        goto ioctl_fail;

    sceKernelMemset(&opt_req, 0, sizeof(opt_req));
    sceNet_lib_B5CE388A(opt_req.ifname, "btdun", 16);
    opt_req.val = -1;
    if (sceNetInet_lib_AEE60F84(sock, 0x80146933u, &opt_req) < 0)
        goto ioctl_fail;
    if (sceNetInet_lib_AEE60F84(sock, 0x80146935u, &opt_req) < 0)
        goto ioctl_fail;

    sceKernelMemset(&auth_req, 0, sizeof(auth_req));
    sceNet_lib_B5CE388A(auth_req.ifname, "btdun", 16);
    sceKernelMemcpy(auth_req.user, g_btdun.cfg_raw + 0x66, 128);
    sceKernelMemcpy(auth_req.pass, g_btdun.cfg_raw + 0xE6, 128);
    auth_req.timeout = 300;
    auth_req.retry = 10;
    if (sceNetInet_lib_AEE60F84(sock, 0x81186939u, &auth_req) < 0)
        goto ioctl_fail;

    ip_addr = bswap32(0u);
    dst_addr = bswap32(1u);
    err = sceNetInet_lib_4F68DB0E("btdun", &ip_addr, &dst_addr, NULL);
    if (err < 0)
        goto notify_err;

    sceKernelMemset(&up_req, 0, sizeof(up_req));
    sceNet_lib_B5CE388A(up_req.ifname, "btdun", 16);
    up_req.up = 1;
    if (sceNetInet_lib_AEE60F84(sock, 0x80206936u, &up_req) < 0)
        goto ioctl_fail;

    sceKernelMemset(&link_req, 0, sizeof(link_req));
    sceNet_lib_B5CE388A(link_req.ifname, "btdun", 16);
    sceKernelMemset(&ipcp_info, 0, sizeof(ipcp_info));
    sceNet_lib_B5CE388A(ipcp_info.ifname, "btdun", 16);

    for (;;) {
        if (sceNetInet_lib_AEE60F84(sock, 0xC014693Bu, &link_req) < 0) {
            err = (int)0x80411885;
            goto notify_err;
        }
        if (link_req.link_up == 0) {
            err = btdun_get_ppp_error(sock);
            goto notify_err;
        }
        if (sceNetInet_lib_AEE60F84(sock, 0xC0246938u, &ipcp_info) < 0) {
            err = (int)0x80411885;
            goto notify_err;
        }
        err = (int)0x80411886;
        if (sceKernelPollSema(g_btdun.sema, 1) == 0)
            goto cleanup;
        if ((ipcp_info.flags & 0x10) != 0)
            break;
        sceKernelDelayThread(1000000);
    }

    ip_addr = bswap32(ipcp_info.ip);
    peer_addr = bswap32(ipcp_info.peer);
    if (peer_addr == 0) {
        peer_addr = 0x0A0A0A0Au;
        if (ip_addr == 0x0A0A0A0Au)
            peer_addr = 0x0A0A0A0Bu;
    }

    err = sceNetInet_lib_4F68DB0E("btdun", &ip_addr, &peer_addr, NULL);
    if (err < 0)
        goto notify_err;
    err = sceNetInet_lib_7776A492(&peer_addr);
    if (err < 0)
        goto notify_err;

    dns1 = 0;
    dns2 = 0;
    if (*(int *)(g_btdun.cfg_raw + 0x168) != 0) {
        sceNetInetInetAton((const char *)(g_btdun.cfg_raw + 0x16C), &dns1);
        if (dns1 != 0) {
            sceNetInetInetAton((const char *)(g_btdun.cfg_raw + 0x17C), &dns2);
            if (dns2 == 0 && (ipcp_info.flags & 4) != 0)
                dns2 = bswap32(ipcp_info.dns1);
        }
    } else {
        if ((ipcp_info.flags & 4) != 0)
            dns1 = bswap32(ipcp_info.dns1);
        if ((ipcp_info.flags & 8) != 0) {
            if (dns1 == 0)
                dns1 = bswap32(ipcp_info.dns2);
            else
                dns2 = bswap32(ipcp_info.dns2);
        }
    }

    err = sceNetInet_lib_AAF4895A(&dns1, &dns2);
    if (err < 0)
        goto notify_err;

    sceNetInet_lib_A94A75E7(&ip_addr);
    if (g_btdun.cb != NULL)
        g_btdun.cb(1, 0);

    for (;;) {
        if (sceNetInet_lib_AEE60F84(sock, 0xC014693Bu, &link_req) < 0) {
            err = (int)0x80411885;
            goto notify_err;
        }
        err = (int)0x80411886;
        if (link_req.link_up == 0) {
            err = btdun_get_ppp_error(sock);
            goto notify_err;
        }
        if (sceKernelPollSema(g_btdun.sema, 1) == 0)
            goto cleanup;
        sceKernelDelayThread(1000000);
    }

ioctl_fail:
    err = (int)0x80411884;
notify_err:
    if (g_btdun.cb != NULL)
        g_btdun.cb(3, err);
cleanup:
    sceNet_lib_D2422E4D("btdun");
    sceNetInet_lib_6A046357("btdun");
    if (g_btdun.cb != NULL)
        g_btdun.cb(2, err);
    if (sock >= 0)
        sceNetInetClose(sock);
    return err;
}

static int btdun_thread(SceSize args, void *argp)
{
    int ret;
    const char *cmds[3];
    void (*log_cb)(char *, int, int) = *(void (**)(char *, int, int))(g_btdun.cfg_raw + 0x18C);

    (void)args;
    (void)argp;

    ret = sceBtdun_lib_DDA6D0A5(*(int *)g_btdun.cfg_raw);
    if (ret < 0) {
        if (g_btdun.cb != NULL)
            g_btdun.cb(3, ret);
        return 0;
    }

    if (g_btdun.cfg_raw[4] != 0) {
        cmds[0] = (const char *)(g_btdun.cfg_raw + 4);
        cmds[1] = NULL;
        ret = btdun_at_chat(cmds, g_btdun_rxbuf, 128, log_cb);
        if (ret < 0) {
            if (g_btdun.cb != NULL)
                g_btdun.cb(3, ret);
            return 0;
        }
    }

    if (log_cb != NULL) {
        if (g_btdun.cb != NULL)
            g_btdun.cb(0, 0);
        return 0;
    }

    cmds[0] = "ATDT";
    cmds[1] = (const char *)(g_btdun.cfg_raw + 0x45);
    cmds[2] = NULL;
    ret = btdun_at_chat(cmds, g_btdun_rxbuf, 128, NULL);
    if (ret < 0) {
        if (g_btdun.cb != NULL)
            g_btdun.cb(3, ret);
        return 0;
    }

    sceBtdun_lib_5C91E089();
    if (g_btdun.cb != NULL)
        g_btdun.cb(0, 0);

    ret = btdun_ppp_session();
    if (ret < 0) {
        if (g_btdun.cb != NULL)
            g_btdun.cb(3, ret);
    }
    return 0;
}

int btdun_start(void (*cb)(int, int), SceUID partId, int stack, int prio, void *cfg)
{
    SceUID sema;
    SceUID thid;
    int ret;
    u32 opt[2];

    if (g_btdun_running == 1)
        return (int)0x80411881;

    sceKernelMemset(&g_btdun, 0, sizeof(g_btdun));
    sceKernelMemcpy(g_btdun.cfg_raw, cfg, 400);
    g_btdun.thid = -1;
    g_btdun.cb = cb;
    g_btdun.sema = -1;

    sema = sceKernelCreateSema("SceNetDunclient", 0, 0, 1, NULL);
    ret = sema;
    if (sema >= 0) {
        g_btdun.sema = sema;
        opt[0] = 8;
        opt[1] = (u32)partId;
        thid = sceKernelCreateThread("SceNetDunclient", btdun_thread, prio, stack, 0, opt);
        ret = thid;
        if (thid >= 0) {
            g_btdun.thid = thid;
            ret = sceKernelStartThread(thid, 0, NULL);
            if (ret >= 0) {
                g_btdun_running = 1;
                return 0;
            }
        }
    }

    if (g_btdun.thid >= 0) {
        sceKernelTerminateThread(g_btdun.thid);
        sceKernelDeleteThread(g_btdun.thid);
        sceNetFreeThreadinfo(g_btdun.thid);
        g_btdun.thid = -1;
    }
    if (g_btdun.sema >= 0) {
        sceKernelDeleteSema(g_btdun.sema);
        g_btdun.sema = -1;
    }
    return ret;
}

int btdun_stop(void)
{
    int ret = sceBtdun_lib_BC01BAC1();
    if (g_btdun_running == 0)
        return ret;

    sceKernelSignalSema(g_btdun.sema, 1);
    if (g_btdun.thid >= 0) {
        ret = sceKernelWaitThreadEnd(g_btdun.thid, NULL);
        if (ret >= 0) {
            sceKernelDeleteThread(g_btdun.thid);
            ret = sceNetFreeThreadinfo(g_btdun.thid);
            g_btdun.thid = -1;
        }
    }
    if (g_btdun.sema >= 0) {
        ret = sceKernelDeleteSema(g_btdun.sema);
        g_btdun.sema = -1;
    }
    g_btdun_running = 0;
    return ret;
}
