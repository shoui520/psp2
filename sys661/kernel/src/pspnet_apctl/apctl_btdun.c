#include "apctl_internal.h"

typedef struct BtdunConfig {
    int device_id;            /* 0x000 */
    char init_cmd[65];        /* 0x004 */
    char dial_num[33];        /* 0x045 */
    char username[128];       /* 0x066 */
    char password[128];       /* 0x0E6 */
    u8 pad_dial[2];           /* 0x166 */
    int use_static_dns;       /* 0x168 */
    char dns_a[16];           /* 0x16C */
    char dns_b[16];           /* 0x17C */
    void (*log_cb)(char *, int, int); /* 0x18C */
} BtdunConfig; /* 400 bytes */

typedef struct BtdunState {
    BtdunConfig cfg;          /* 0x000..0x18F */
    u8 pad_cfg[4];            /* 0x190..0x193 */
    SceUID thid;              /* 0x194 */
    SceUID sema;              /* 0x198 */
    void (*cb)(int, int);     /* 0x19C */
    u8 pad_tail[4];           /* 0x1A0..0x1A3 */
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
    struct {
        char ifname[16];
        int code;
        u8 pad[12];
    } req;
    int ret;

    sceKernelMemset(&req, 0, 20);
    sceNet_lib_B5CE388A(req.ifname, "btdun", 16);
    ret = sceNetInet_lib_AEE60F84(sock, 0xC014693Du, &req);
    if (ret < 0)
        return (int)0x80411885;

    switch (req.code) {
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
    int ret = 0;
    char *write_pos;
    int rlen;
    int finished = 0;

    do {
        if (cmds[0] != NULL) {
            while (cmds[i] != NULL) {
                int len = sceNet_lib_D8722983(cmds[i]);
                ret = sceBtdun_lib_5BED7B20(cmds[i], len, 15000000u);
                if (ret < 0) {
                    finished = 1;
                    break;
                }
                i++;
            }
            if (finished)
                break;
        }

        ret = sceBtdun_lib_5BED7B20("\r\n", sceNet_lib_D8722983("\r\n"), 15000000u);
        if (ret < 0)
            break;

        write_pos = buf;
        while (!finished) {
            int buffered = (int)(write_pos - buf);
            int total;
            char *cur;

            rlen = bufsize - buffered;
            ret = sceBtdun_lib_966A10F5(write_pos, &rlen, 15000000u);
            if (ret < 0)
                break;
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
                        finished = 1;
                        break;
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
    } while (0);

    if (log_cb != NULL)
        log_cb(NULL, 0, ret);
    return ret;
}

static int btdun_ppp_session(void)
{
    int sock;
    int err;
    int notify_err = 0;
    u32 ip_addr;
    u32 dst_addr;
    u32 peer_addr;
    u32 dns_a;
    u32 dns_b;
    struct {
        char ifname[16];
        int flags;
        u32 ip;
        u32 peer;
        u32 dns_a;
        u32 dns_b;
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
        int retry_open;
        int retry_term;
        int flags;
    } lcp_req;
    struct {
        char ifname[16];
        int status;
    } opt_req;
    struct {
        char ifname[16];
        int up;
        u8 pad[12];
    } up_req;

    sock = sceNetInetSocket(2, 2, 0);
    err = (int)0x80411883;

    do {
        int stopping = 0;
        if (sock < 0) {
            notify_err = 1;
            break;
        }

        sceKernelMemset(&lcp_req, 0, sizeof(lcp_req));
        sceNet_lib_B5CE388A(lcp_req.ifname, "btdun", 16);
        lcp_req.timeout = 300;
        lcp_req.mode = 2;
        lcp_req.retry_open = 10;
        lcp_req.retry_term = 10;
        lcp_req.flags = 7;
        if (sceNetInet_lib_AEE60F84(sock, 0x8024692Au, &lcp_req) < 0) {
            err = (int)0x80411884;
            notify_err = 1;
            break;
        }

        sceKernelMemset(&opt_req, 0, sizeof(opt_req));
        sceNet_lib_B5CE388A(opt_req.ifname, "btdun", 16);
        opt_req.status = -1;
        if (sceNetInet_lib_AEE60F84(sock, 0x80146933u, &opt_req) < 0 ||
            sceNetInet_lib_AEE60F84(sock, 0x80146935u, &opt_req) < 0) {
            err = (int)0x80411884;
            notify_err = 1;
            break;
        }

        sceKernelMemset(&auth_req, 0, sizeof(auth_req));
        sceNet_lib_B5CE388A(auth_req.ifname, "btdun", 16);
        sceKernelMemcpy(auth_req.user, g_btdun.cfg.username, 128);
        sceKernelMemcpy(auth_req.pass, g_btdun.cfg.password, 128);
        auth_req.timeout = 300;
        auth_req.retry = 10;
        if (sceNetInet_lib_AEE60F84(sock, 0x81186939u, &auth_req) < 0) {
            err = (int)0x80411884;
            notify_err = 1;
            break;
        }

        ip_addr = bswap32(0u);
        dst_addr = bswap32(1u);
        err = sceNetInet_lib_4F68DB0E("btdun", &ip_addr, &dst_addr, NULL);
        if (err < 0) {
            notify_err = 1;
            break;
        }

        sceKernelMemset(&up_req, 0, sizeof(up_req));
        sceNet_lib_B5CE388A(up_req.ifname, "btdun", 16);
        up_req.up = 1;
        if (sceNetInet_lib_AEE60F84(sock, 0x80206936u, &up_req) < 0) {
            err = (int)0x80411884;
            notify_err = 1;
            break;
        }

        sceKernelMemset(&link_req, 0, sizeof(link_req));
        sceNet_lib_B5CE388A(link_req.ifname, "btdun", 16);
        sceKernelMemset(&ipcp_info, 0, sizeof(ipcp_info));
        sceNet_lib_B5CE388A(ipcp_info.ifname, "btdun", 16);

        for (;;) {
            if (sceNetInet_lib_AEE60F84(sock, 0xC014693Bu, &link_req) < 0) {
                err = (int)0x80411885;
                notify_err = 1;
                break;
            }
            if (link_req.link_up == 0) {
                err = btdun_get_ppp_error(sock);
                notify_err = 1;
                break;
            }
            if (sceNetInet_lib_AEE60F84(sock, 0xC0246938u, &ipcp_info) < 0) {
                err = (int)0x80411885;
                notify_err = 1;
                break;
            }
            err = (int)0x80411886;
            if (sceKernelPollSema(g_btdun.sema, 1) == 0) {
                stopping = 1;
                break;
            }
            if ((ipcp_info.flags & 0x10) != 0)
                break;
            sceKernelDelayThread(1000000);
        }
        if (notify_err || stopping)
            break;

        ip_addr = bswap32(ipcp_info.ip);
        peer_addr = bswap32(ipcp_info.peer);
        if (peer_addr == 0) {
            peer_addr = 0x0A0A0A0Au;
            if (ip_addr == 0x0A0A0A0Au)
                peer_addr = 0x0A0A0A0Bu;
        }

        err = sceNetInet_lib_4F68DB0E("btdun", &ip_addr, &peer_addr, NULL);
        if (err < 0) {
            notify_err = 1;
            break;
        }
        err = sceNetInet_lib_7776A492(&peer_addr);
        if (err < 0) {
            notify_err = 1;
            break;
        }

        dns_a = 0;
        dns_b = 0;
        if (g_btdun.cfg.use_static_dns != 0) {
            sceNetInetInetAton(g_btdun.cfg.dns_a, &dns_a);
            if (dns_a != 0) {
                sceNetInetInetAton(g_btdun.cfg.dns_b, &dns_b);
                if (dns_b == 0 && (ipcp_info.flags & 4) != 0)
                    dns_b = bswap32(ipcp_info.dns_a);
            }
        } else {
            if ((ipcp_info.flags & 4) != 0)
                dns_a = bswap32(ipcp_info.dns_a);
            if ((ipcp_info.flags & 8) != 0) {
                if (dns_a == 0)
                    dns_a = bswap32(ipcp_info.dns_b);
                else
                    dns_b = bswap32(ipcp_info.dns_b);
            }
        }

        err = sceNetInet_lib_AAF4895A(&dns_a, &dns_b);
        if (err < 0) {
            notify_err = 1;
            break;
        }

        sceNetInet_lib_A94A75E7(&ip_addr);
        if (g_btdun.cb != NULL)
            g_btdun.cb(1, 0);

        for (;;) {
            if (sceNetInet_lib_AEE60F84(sock, 0xC014693Bu, &link_req) < 0) {
                err = (int)0x80411885;
                notify_err = 1;
                break;
            }
            err = (int)0x80411886;
            if (link_req.link_up == 0) {
                err = btdun_get_ppp_error(sock);
                notify_err = 1;
                break;
            }
            if (sceKernelPollSema(g_btdun.sema, 1) == 0)
                break;
            sceKernelDelayThread(1000000);
        }
    } while (0);

    if (notify_err && g_btdun.cb != NULL)
        g_btdun.cb(3, err);

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
    void (*log_cb)(char *, int, int) = g_btdun.cfg.log_cb;

    (void)args;
    (void)argp;

    ret = sceBtdun_lib_DDA6D0A5(g_btdun.cfg.device_id);
    if (ret < 0) {
        if (g_btdun.cb != NULL)
            g_btdun.cb(3, ret);
        return 0;
    }

    if (g_btdun.cfg.init_cmd[0] != 0) {
        cmds[0] = g_btdun.cfg.init_cmd;
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
    cmds[1] = g_btdun.cfg.dial_num;
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
    sceKernelMemcpy(&g_btdun.cfg, cfg, sizeof(g_btdun.cfg));
    g_btdun.thid = -1;
    g_btdun.cb = cb;
    g_btdun.sema = -1;

    sema = sceKernelCreateSema("SceNetDunclient", 0, 0, 1, NULL);
    ret = sema;
    if (sema >= 0) {
        g_btdun.sema = sema;
        sceKernelMemset(opt, 0, sizeof(opt));
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
