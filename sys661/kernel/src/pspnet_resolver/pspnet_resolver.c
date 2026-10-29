#include <common_imp.h>

typedef struct ResolverAsyncCtx {
    int type;           /* 0x00: 0 = NtoA, 1 = AtoN */
    u32 txid;           /* 0x04 */
    int retry_idx;      /* 0x08 */
    int dns_idx;        /* 0x0c */
    int state;          /* 0x10: 0=init, 1=sent, 2=waiting_timeout */
    u32 start_time;     /* 0x14 */
    int timeout;        /* 0x18 */
    int retry;          /* 0x1c */
    char *hostname;     /* 0x20 */
    u32 *addr_ptr;      /* 0x24 (or u8 addr[4] for AtoN) */
    char *out_name;     /* 0x28 */
    int out_len;        /* 0x2c */
} ResolverAsyncCtx;

typedef struct ResolverNode {
    int rid;            /* 0x00 */
    struct ResolverNode *next; /* 0x04 */
    u8 *buf;            /* 0x08 */
    u32 buflen;         /* 0x0c */
    int sock;           /* 0x10 */
    u32 flags;          /* 0x14 */
    ResolverAsyncCtx *async; /* 0x18 */
} ResolverNode;

extern int sceKernelCpuSuspendIntr(void);
extern int sceKernelCpuResumeIntr(int intr);
extern void *sceKernelMemset(void *dst, int c, SceSize n);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize n);
extern int _sceKernelCheckThreadStack(void);
extern SceUID sceKernelGetThreadId(void);
extern int sceKernelGetSystemTime(void *time);
extern u32 sceKernelGetSystemTimeLow(void);

extern void *sceNetIfhandle_lib_15CFE3C0(SceSize size);
extern void sceNetIfhandle_lib_76BAD213(void *ptr);

extern int sceNetInetSocket(int domain, int type, int protocol);
extern int sceNetInetClose(int s);
extern int sceNetInetSetsockopt(int s, int level, int optname, const void *optval, int optlen);
extern int sceNetInetSendto(int s, const void *buf, int len, int flags, const void *to, int tolen);
extern int sceNetInetRecvfrom(int s, void *buf, int len, int flags, void *from, void *fromlen);
extern int sceNetInetGetErrno(void);
extern int sceNetInet_80A21ABD(int s);
extern int sceNetInet_lib_AC9D90A5(u32 *dns1, u32 *dns2);

extern int sceNet_lib_750F705D(int c);
extern char *sceNet_lib_80C9F02A(char *dst, const char *src);
extern void *sceNet_lib_B5CE388A(void *dst, const void *src, SceSize n);
extern char *sceNet_lib_BCBE14CF(const char *s, int c);
extern SceSize sceNet_lib_D8722983(const char *s);

static int g_initialized;
static int g_next_rid;
static ResolverNode *g_resolver_list;
static u16 g_dns_txid;

static int sub_09cc(int rid, int *out, int is_nonblock);
static int sub_0b60(void *buf, SceSize buflen, int *rid);
static int sub_0cb4(int rid);
static int sub_0dc4(void);
static int sub_0e18(void);
static ResolverNode *sub_0e38(int rid, ResolverNode **prev_out);
static int sub_0e74(const char *s1, const char *s2, SceSize n);
static int sub_0f9c(ResolverNode *node, int type, const void *name_or_addr, int out_len, void *out_buf, int timeout, int retry);
static int sub_15f4(void);
static int sub_1638(void);
static int sub_1640(int type, const char *qname, u8 *pkt_buf, u32 *pkt_len_out, u16 *txid_out);
static int sub_1758(ResolverNode *node, u8 *rx_buf, u32 tx_len, const char *qname, u8 *qname_end, void *out_addr, int timeout, const u32 *dns_ip, int out_name, int out_len, int *done_out, u16 txid, int type);
static int sub_1a28(int type, u8 **ptr_io, u8 *end, u16 *txid_out);
static int sub_1b10(u8 **ptr_io, u8 *end, const char *qname);
static int sub_1c14(ResolverNode *node, const void *buf, int len, const u32 *dns_ip, int timeout);
static int sub_1db8(ResolverNode *node, void *buf, int *len_io, int timeout);
static int sub_1f74(u8 **ptr_io, u8 *end, u16 txid, u16 *qdcount_out, u16 *ancount_out, int *done_out);
static int sub_20d0(u8 **ptr_io, u8 *end, u8 *pkt_start, const char *qname, u8 *qname_end, u16 qdcount);
static int sub_21c0(ResolverNode *node, u8 **ptr_io, u8 *end, u8 *pkt_start, const char *qname, u8 *qname_end, void *out_addr, int out_name, int type, int out_len, u16 ancount);
static int sub_2418(u8 *ptr, u8 *end, char *out_name, u8 **out_end_io, u8 *pkt_start, u8 *pkt_limit);
static int sub_25a4(u8 **ptr_io, u8 *end, u8 *pkt_start, u8 *pkt_limit, const char *qname, u8 *qname_end, int *match_out);

int sceNetResolverStartNtoA(int rid, const char *hostname, u32 *addr, unsigned int timeout, int retry)
{
    int intr;
    ResolverNode *node;

    if (_sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    if (hostname == 0 || addr == 0)
        return 0x80410403;
    if (sceNet_lib_D8722983(hostname) == 0 || sceNet_lib_D8722983(hostname) >= 0x100)
        return 0x80410418;

    intr = sceKernelCpuSuspendIntr();
    node = sub_0e38(rid, 0);
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if (node->flags & 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410409;
    }
    node->flags = (node->flags | 2) & ~4U;
    if (node->async != 0) {
        if (node->async->hostname != 0)
            sceNetIfhandle_lib_76BAD213(node->async->hostname);
        sceNetIfhandle_lib_76BAD213(node->async);
        node->async = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return sub_0f9c(node, 0, hostname, 0, addr, timeout, retry);
}

int sceNetResolver_14C17EF9(int rid, const char *hostname, u32 *addr, unsigned int timeout, int retry)
{
    int intr, ret = 0;
    ResolverNode *node;

    if (_sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    if (hostname == 0 || addr == 0)
        return 0x80410403;
    if (sceNet_lib_D8722983(hostname) == 0 || sceNet_lib_D8722983(hostname) >= 0x100)
        return 0x80410418;

    intr = sceKernelCpuSuspendIntr();
    node = sub_0e38(rid, 0);
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if (node->flags & 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410409;
    }
    node->flags |= 6;
    if (node->async != 0) {
        if (node->async->hostname != 0) {
            sceNetIfhandle_lib_76BAD213(node->async->hostname);
            node->async->hostname = 0;
        }
    } else {
        node->async = sceNetIfhandle_lib_15CFE3C0(sizeof(ResolverAsyncCtx));
        if (node->async == 0) {
            sceKernelCpuResumeIntr(intr);
            return 0x80410407;
        }
    }
    sceKernelMemset(node->async, 0, sizeof(ResolverAsyncCtx));
    node->async->addr_ptr = addr;
    node->async->timeout = timeout;
    node->async->retry = retry;
    node->async->type = 0;
    node->async->hostname = sceNetIfhandle_lib_15CFE3C0(sceNet_lib_D8722983(hostname) + 1);
    if (node->async->hostname == 0) {
        sceNetIfhandle_lib_76BAD213(node->async);
        node->async = 0;
        ret = 0x80410407;
    } else {
        sceNet_lib_80C9F02A(node->async->hostname, hostname);
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceNetResolverStartAtoN(int rid, const u32 *addr, char *hostname, SceSize hostname_len, unsigned int timeout, int retry)
{
    int intr;
    ResolverNode *node;

    if (_sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    if (hostname == 0 || addr == 0)
        return 0x80410403;

    intr = sceKernelCpuSuspendIntr();
    node = sub_0e38(rid, 0);
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if (node->flags & 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410409;
    }
    node->flags = (node->flags | 2) & ~4U;
    if (node->async != 0) {
        if (node->async->hostname != 0)
            sceNetIfhandle_lib_76BAD213(node->async->hostname);
        sceNetIfhandle_lib_76BAD213(node->async);
        node->async = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return sub_0f9c(node, 1, hostname, hostname_len, (void *)addr, timeout, retry);
}

int sceNetResolver_AAC09184(int rid, const u32 *addr, char *hostname, SceSize hostname_len, unsigned int timeout, int retry)
{
    int intr, ret = 0;
    ResolverNode *node;

    if (_sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    if (hostname == 0 || addr == 0)
        return 0x80410403;

    intr = sceKernelCpuSuspendIntr();
    node = sub_0e38(rid, 0);
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if (node->flags & 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410409;
    }
    node->flags |= 6;
    if (node->async != 0) {
        if (node->async->hostname != 0) {
            sceNetIfhandle_lib_76BAD213(node->async->hostname);
            node->async->hostname = 0;
        }
    } else {
        node->async = sceNetIfhandle_lib_15CFE3C0(sizeof(ResolverAsyncCtx));
        if (node->async == 0) {
            sceKernelCpuResumeIntr(intr);
            return 0x80410407;
        }
    }
    sceKernelMemset(node->async, 0, sizeof(ResolverAsyncCtx));
    node->async->type = 1;
    node->async->timeout = timeout;
    node->async->retry = retry;
    sceKernelMemcpy(&node->async->addr_ptr, addr, 4);
    node->async->out_name = hostname;
    node->async->out_len = hostname_len;
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceNetResolverStop(int rid)
{
    int intr;
    ResolverNode *node;

    if (_sceKernelCheckThreadStack() < 0x13f0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;

    intr = sceKernelCpuSuspendIntr();
    node = sub_0e38(rid, 0);
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if ((node->flags & 2) == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x8041040a;
    }
    node->flags |= 1;
    sceKernelCpuResumeIntr(intr);
    if (node->sock >= 0)
        sceNetInet_80A21ABD(node->sock);
    return 0;
}

int sceNetResolverInit(void)
{
    int ret;

    if (_sceKernelCheckThreadStack() < 0x13f0)
        return 0x80410005;
    if (g_initialized > 0) {
        sub_0e18();
        return 0x80000020;
    }
    ret = sub_0dc4();
    if (ret < 0) {
        sub_0e18();
        return ret;
    }
    g_initialized = 1;
    return 0;
}

int sceNetResolverTerm(void)
{
    if (_sceKernelCheckThreadStack() < 0x13f0)
        return 0x80410005;
    if (g_initialized != 0) {
        sub_0e18();
        g_initialized = 0;
    }
    return 0;
}

int sceNetResolverCreate(int *rid, void *buf, SceSize buflen)
{
    if (_sceKernelCheckThreadStack() < 0x13f0)
        return 0x80410005;
    if (rid == 0 || buf == 0)
        return 0x80410403;
    if (buflen == 0)
        return 0x80410404;
    return sub_0b60(buf, buflen, rid);
}

int sceNetResolverDelete(int rid)
{
    if (_sceKernelCheckThreadStack() < 0x13f0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    return sub_0cb4(rid);
}

int sceNetResolver_4EE99358(int rid, int *out)
{
    if (_sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    return sub_09cc(rid, out, 0);
}

int sceNetResolver_12748EB9(int rid, int *out)
{
    if (_sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    return sub_09cc(rid, out, 1);
}

static int sub_09cc(int rid, int *out, int is_nonblock)
{
    int intr, ret;
    ResolverNode *node;

    (void)out;
    intr = sceKernelCpuSuspendIntr();
    node = sub_0e38(rid, 0);
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if ((node->flags & 4) == 0 || node->async == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410419;
    }
    if ((node->flags & 2) == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x8041041a;
    }
    sceKernelCpuResumeIntr(intr);

    if (is_nonblock == 1) {
        if (node->sock > 0) {
            int val = 0;
            if (sceNetInetSetsockopt(node->sock, 0xffff, 0x1009, &val, 4) < 0)
                return 0x8041040f;
            node->flags &= ~8U;
        } else {
            node->flags &= ~4U;
        }
    }

    ResolverAsyncCtx *async = node->async;
    if (async->type == 0) {
        ret = sub_0f9c(node, 0, async->hostname, 0, async->addr_ptr, async->timeout, async->retry);
    } else if (async->type == 1) {
        ret = sub_0f9c(node, 1, async->out_name, async->out_len, &async->addr_ptr, async->timeout, async->retry);
    } else {
        ret = 0x80410419;
    }

    if (ret > 0)
        return 0;
    if (ret == (int)0x80410481)
        return 1;
    return ret;
}

static int sub_0b60(void *buf, SceSize buflen, int *rid)
{
    ResolverNode *node;
    int intr, id, start_id;

    node = sceNetIfhandle_lib_15CFE3C0(sizeof(ResolverNode));
    if (node == 0)
        return 0x80410407;

    sceKernelMemset(node, 0, sizeof(ResolverNode));
    intr = sceKernelCpuSuspendIntr();
    start_id = g_next_rid;
    id = (start_id == 0x20) ? 1 : (start_id + 1);

    while (id != start_id) {
        ResolverNode *cur = g_resolver_list;
        while (cur != 0) {
            if (cur->rid == id)
                break;
            cur = cur->next;
        }
        if (cur == 0) {
            g_next_rid = id;
            node->buflen = buflen;
            node->next = g_resolver_list;
            node->rid = id;
            node->buf = buf;
            g_resolver_list = node;
            *rid = id;
            sceKernelCpuResumeIntr(intr);
            return 0;
        }
        id = (id == 0x20) ? 1 : (id + 1);
    }

    sceKernelCpuResumeIntr(intr);
    sceNetIfhandle_lib_76BAD213(node);
    return 0x80410406;
}

static int sub_0cb4(int rid)
{
    int intr;
    ResolverNode *node, *prev = 0;

    intr = sceKernelCpuSuspendIntr();
    node = g_resolver_list;
    while (node != 0) {
        if (node->rid == rid)
            break;
        prev = node;
        node = node->next;
    }
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if ((node->flags & 6) == 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410409;
    }
    if (prev != 0)
        prev->next = node->next;
    else
        g_resolver_list = g_resolver_list->next;

    if (node->async != 0) {
        if (node->async->hostname != 0)
            sceNetIfhandle_lib_76BAD213(node->async->hostname);
        sceNetIfhandle_lib_76BAD213(node->async);
    }
    sceNetIfhandle_lib_76BAD213(node);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static int sub_0dc4(void)
{
    int ret = sub_15f4();
    if (ret < 0) {
        sub_1638();
        return ret;
    }
    g_resolver_list = 0;
    g_next_rid = 0;
    return 0;
}

static int sub_0e18(void)
{
    sub_1638();
    return 0;
}

static ResolverNode *sub_0e38(int rid, ResolverNode **prev_out)
{
    ResolverNode *cur = g_resolver_list;
    ResolverNode *prev = 0;

    while (cur != 0) {
        if (cur->rid == rid)
            break;
        prev = cur;
        cur = cur->next;
    }
    if (prev_out != 0)
        *prev_out = prev;
    return cur;
}

static int sub_0e74(const char *s1, const char *s2, SceSize n)
{
    SceSize i;
    int c1, c2;

    if (n == 0)
        return 0;

    for (i = 0; i < n; i++) {
        c1 = *s1;
        if (sceNet_lib_750F705D(c1) & 1)
            c1 = (s8)(c1 + 0x20);
        c2 = *s2++;
        if (sceNet_lib_750F705D(c2) & 1)
            c2 = (s8)(c2 + 0x20);
        if (c1 != c2) {
            c1 = *s1;
            if (sceNet_lib_750F705D(c1) & 1)
                c1 = (s8)(c1 + 0x20);
            c2 = s2[-1];
            if (sceNet_lib_750F705D(c2) & 1)
                c2 = (s8)(c2 + 0x20);
            return (c1 & 0xff) - (c2 & 0xff);
        }
        if (*(const u8 *)s1 == 0)
            return 0;
        s1++;
    }
    return 0;
}

static int fmt_octet(char *dst, u8 val)
{
    int len = 0;
    u32 d = 100;
    int i;
    for (i = 0; i < 3; i++) {
        u8 q = val / d;
        val = val % d;
        d /= 10;
        if (len != 0 || q != 0)
            dst[len++] = q + '0';
    }
    if (len == 0)
        dst[len++] = '0';
    dst[len] = 0;
    return len;
}

static int sub_0f9c(ResolverNode *node, int type, const void *name_or_addr, int out_len, void *out_buf, int timeout, int retry)
{
    u32 dns_ips[2];
    int done = 0;
    int ret;
    u32 rem_buflen = node->buflen;
    char *qname;
    u8 *pkt_buf;
    int r_idx, d_idx;

    ret = sceNetInet_lib_AC9D90A5(&dns_ips[0], &dns_ips[1]);
    if (ret < 0)
        goto cleanup;

    if (dns_ips[0] == 0 || dns_ips[0] == ~0U) {
        ret = 0x80410402;
        goto cleanup;
    }

    if (type == 0) {
        const char *hname = (const char *)name_or_addr;
        SceSize hlen = sceNet_lib_D8722983(hname);
        if (hname[hlen - 1] == '.') {
            if (hlen >= rem_buflen) {
                ret = 0x8041040c;
                goto cleanup;
            }
            qname = (char *)node->buf;
            sceNet_lib_B5CE388A(qname, hname, hlen - 1);
            qname[hlen - 1] = 0;
            pkt_buf = (u8 *)(qname + hlen);
            rem_buflen -= hlen;
        } else {
            qname = (char *)hname;
            pkt_buf = node->buf;
        }
    } else if (type == 1) {
        if (rem_buflen <= 0x1d) {
            ret = 0x8041040c;
            goto cleanup;
        }
        const u8 *ip = (const u8 *)out_buf;
        char *p = (char *)node->buf;
        int n;
        n = fmt_octet(p, ip[3]); p += n; *p++ = '.';
        n = fmt_octet(p, ip[2]); p += n; *p++ = '.';
        n = fmt_octet(p, ip[1]); p += n; *p++ = '.';
        n = fmt_octet(p, ip[0]); p += n; *p++ = '.';
        SceSize arpa_len = sceNet_lib_D8722983("in-addr.arpa") + 1;
        sceNet_lib_B5CE388A(p, "in-addr.arpa", arpa_len);
        SceSize total_qlen = (SceSize)(p - (char *)node->buf) + arpa_len + 1;
        qname = (char *)node->buf;
        pkt_buf = node->buf + total_qlen;
        rem_buflen -= total_qlen;
    } else {
        ret = 0x80410417;
        goto cleanup;
    }

    r_idx = 0;
    d_idx = 0;
    if ((node->flags & 4) && node->async != 0) {
        d_idx = node->async->dns_idx;
        r_idx = node->async->retry_idx;
    }

    for (; r_idx <= retry; r_idx++) {
        if ((node->flags & 4) && node->async != 0) {
            if (node->async->retry_idx != r_idx) {
                d_idx = 0;
                node->async->state = 0;
                node->async->retry_idx = r_idx;
                node->async->dns_idx = 0;
            }
        } else {
            d_idx = 0;
        }

        while (d_idx < 2 && dns_ips[d_idx] != 0) {
            u32 pkt_len = rem_buflen;
            u16 txid;
            if ((node->flags & 4) && node->async != 0) {
                if (node->async->dns_idx != d_idx) {
                    node->async->state = 0;
                    node->async->dns_idx = d_idx;
                }
            }
            ret = sub_1640(type, qname, pkt_buf, &pkt_len, &txid);
            if (ret < 0)
                goto cleanup;

            int cur_d = d_idx++;
            done = 0;
            ret = sub_1758(node, pkt_buf, pkt_len, qname, pkt_buf, out_buf, timeout, &dns_ips[cur_d], (int)name_or_addr, out_len, &done, txid, type);
            if (done <= 0)
                goto cleanup;
        }
    }

cleanup:
    {
        int intr = sceKernelCpuSuspendIntr();
        if (node->flags & 4) {
            if (node->async != 0 && ret != (int)0x80410481) {
                node->flags &= ~0xbU;
                if (node->async->hostname != 0)
                    sceNetIfhandle_lib_76BAD213(node->async->hostname);
                sceKernelMemset(node->async, 0, sizeof(ResolverAsyncCtx));
            }
        } else {
            node->flags &= ~0xbU;
            if (node->async != 0) {
                if (node->async->hostname != 0)
                    sceNetIfhandle_lib_76BAD213(node->async->hostname);
                sceKernelMemset(node->async, 0, sizeof(ResolverAsyncCtx));
            }
        }
        sceKernelCpuResumeIntr(intr);
    }
    return ret;
}

static int sub_15f4(void)
{
    u16 sys_time[4];
    sceKernelGetSystemTime(sys_time);
    SceUID thid = sceKernelGetThreadId();
    g_dns_txid = (sys_time[0] ^ sys_time[2]) ^ (u16)thid;
    return 0;
}

static int sub_1638(void)
{
    return 0;
}

static int sub_1640(int type, const char *qname, u8 *pkt_buf, u32 *pkt_len_io, u16 *txid_out)
{
    u8 *ptr = pkt_buf;
    u8 *end = pkt_buf + *pkt_len_io;
    int ret;
    u16 qtype, qclass;

    ret = sub_1a28(type, &ptr, end, txid_out);
    if (ret < 0)
        return ret;
    ret = sub_1b10(&ptr, end, qname);
    if (ret < 0)
        return ret;
    if (end < ptr + 4)
        return 0x8041040c;

    if (type == 0)
        qtype = __builtin_bswap16(1);
    else if (type == 1)
        qtype = __builtin_bswap16(12);
    else
        qtype = 0;

    sceKernelMemcpy(ptr, &qtype, 2);
    ptr += 2;
    qclass = __builtin_bswap16(1);
    sceKernelMemcpy(ptr, &qclass, 2);
    ptr += 2;
    *pkt_len_io = (u32)(ptr - pkt_buf);
    return ret;
}

static int sub_1758(ResolverNode *node, u8 *rx_buf, u32 tx_len, const char *qname, u8 *qname_end, void *out_addr, int timeout, const u32 *dns_ip, int out_name, int out_len, int *done_out, u16 txid, int type)
{
    int ret;
    u32 rx_len;
    u8 *ptr, *end;
    u16 qdcount, ancount;

    if ((node->flags & 4) == 0 || node->async == 0 || (u32)(node->async->state - 1) >= 2) {
        ret = sub_1c14(node, rx_buf, tx_len, dns_ip, timeout);
        if (ret < 0)
            goto err_check;
        if ((node->flags & 4) && node->async != 0) {
            node->async->state = 1;
            node->async->txid = txid;
        }
    }

    if ((node->flags & 4) && node->async != 0)
        txid = (u16)node->async->txid;

    rx_len = node->buflen - (u32)(rx_buf - node->buf);
    ret = sub_1db8(node, rx_buf, (int *)&rx_len, timeout);
    if (ret < 0)
        goto err_check;

    ptr = rx_buf;
    end = rx_buf + rx_len;
    ret = sub_1f74(&ptr, end, txid, &qdcount, &ancount, done_out);
    if (ret >= 0) {
        ret = sub_20d0(&ptr, end, rx_buf, qname, qname_end, qdcount);
        if (ret >= 0) {
            ret = sub_21c0(node, &ptr, end, rx_buf, qname, qname_end, out_addr, out_name, type, out_len, ancount);
        }
    }
    if (ret == (int)0x8041040d) {
        if (ptr >= node->buf + node->buflen)
            ret = 0x8041040c;
    }
    goto cleanup;

err_check:
    if (ret != (int)0x8041040e && ret != (int)0x80410481)
        *done_out = 1;

cleanup:
    if ((node->flags & 4) && node->async != 0 && (node->flags & 8) == 0) {
        if (node->async->hostname != 0)
            sceNetIfhandle_lib_76BAD213(node->async->hostname);
        sceNetIfhandle_lib_76BAD213(node->async);
        node->async = 0;
        node->flags &= ~4U;
    }
    if (node->sock >= 0) {
        if ((node->flags & 4) && node->async != 0 && ret == (int)0x80410481)
            return ret;
        sceNetInetClose(node->sock);
        node->sock = 0;
    }
    return ret;
}

static int sub_1a28(int type, u8 **ptr_io, u8 *end, u16 *txid_out)
{
    u8 hdr[12];
    u8 *p = *ptr_io;
    int intr;
    u16 id;

    (void)type;
    if (end < p + 12)
        return 0x8041040c;

    sceKernelMemset(hdr, 0, 12);
    intr = sceKernelCpuSuspendIntr();
    id = g_dns_txid++;
    *txid_out = id;
    sceKernelCpuResumeIntr(intr);

    *(u16 *)&hdr[0] = id;
    hdr[2] = 1;
    hdr[3] = 0;
    *(u16 *)&hdr[4] = __builtin_bswap16(1);
    sceKernelMemcpy(p, hdr, 12);
    *ptr_io = p + 12;
    return 0;
}

static int sub_1b10(u8 **ptr_io, u8 *end, const char *qname)
{
    u8 *p = *ptr_io;
    int last = 0;

    while (!last) {
        const char *dot = sceNet_lib_BCBE14CF(qname, '.');
        u8 len;
        if (dot == 0) {
            dot = sceNet_lib_BCBE14CF(qname, 0);
            last = 1;
        }
        len = (u8)(dot - qname);
        if (end < p + len + 1)
            return 0x8041040c;
        *p++ = len;
        sceKernelMemcpy(p, qname, len);
        p += len;
        qname = dot + 1;
    }
    if (end < p + 1)
        return 0x8041040c;
    *p++ = 0;
    *ptr_io = p;
    return 0;
}

static int sub_1c14(ResolverNode *node, const void *buf, int len, const u32 *dns_ip, int timeout)
{
    int s, ret;
    u8 sin[16];

    if (node->flags & 1) {
        ret = 0x8041040e;
        goto err;
    }
    s = sceNetInetSocket(2, 2, 0);
    if (s < 0) {
        ret = 0x8041040f;
        goto err;
    }
    node->sock = s;
    if ((node->flags & 4) && node->async != 0) {
        int val = 1;
        if (sceNetInetSetsockopt(s, 0xffff, 0x1009, &val, 4) < 0) {
            ret = 0x8041040f;
            goto err;
        }
        node->flags |= 8;
    } else {
        node->flags &= ~8U;
    }

    if (sceNetInetSetsockopt(node->sock, 0xffff, 0x1006, &timeout, 4) < 0) {
        ret = 0x8041040f;
        goto err;
    }

    sceKernelMemset(sin, 0, 16);
    sin[0] = 16;
    sin[1] = 2;
    *(u16 *)&sin[2] = __builtin_bswap16(53);
    sceKernelMemcpy(&sin[4], dns_ip, 4);

    ret = sceNetInetSendto(node->sock, buf, len, 0, sin, 16);
    if (ret >= 0)
        return ret;
    ret = 0x8041040f;

err:
    if (node->sock >= 0) {
        if ((node->flags & 4) && node->async != 0 && ret == (int)0x80410481)
            return ret;
        sceNetInetClose(node->sock);
        node->sock = 0;
    }
    return ret;
}

static int sub_1db8(ResolverNode *node, void *buf, int *len_io, int timeout)
{
    u8 from[20];
    int fromlen;
    int ret;
    u32 elapsed = 0;

    if (node->flags & 1)
        return 0x8041040e;

    if ((node->flags & 4) && node->async != 0 && node->async->state == 2) {
        u32 now = sceKernelGetSystemTimeLow();
        u32 start = node->async->start_time;
        elapsed = (now > start) ? (now - start) : (now - start - 1);
        if ((u32)node->async->timeout < elapsed)
            return 0x80410410;
    }

    if ((node->flags & 4) && node->async != 0 && (node->flags & 8) == 0) {
        int rem = node->async->timeout - elapsed;
        sceNetInetSetsockopt(node->sock, 0xffff, 0x1006, &rem, 4);
    }

    ret = sceNetInetRecvfrom(node->sock, buf, *len_io, 0, from, &fromlen);
    if (ret >= 0) {
        *len_io = ret;
        return ret;
    }

    int err = sceNetInetGetErrno();
    if (err == 4)
        return 0x8041040e;
    if (err != 11)
        return 0x8041040f;

    if ((node->flags & 4) && node->async != 0 && (node->flags & 8)) {
        if (node->async->state == 1) {
            node->async->state = 2;
            node->async->start_time = sceKernelGetSystemTimeLow();
        }
        return 0x80410481;
    }
    return 0x80410410;
}

static int sub_1f74(u8 **ptr_io, u8 *end, u16 txid, u16 *qdcount_out, u16 *ancount_out, int *done_out)
{
    u8 hdr[12];
    u8 *p = *ptr_io;
    int ret = 0;

    if (end < p + 12)
        return 0x8041040d;

    sceKernelMemcpy(hdr, p, 12);
    p += 12;

    if (*(u16 *)&hdr[0] != txid) {
        *done_out = 1;
        *ptr_io = p;
        return 0x80410410;
    }

    u16 flags = *(u16 *)&hdr[2];
    if ((flags & 0xf8) != 0x80) {
        *ptr_io = p;
        return 0x8041040d;
    }

    u16 qd = __builtin_bswap16(*(u16 *)&hdr[4]);
    if (qd != 1) {
        *ptr_io = p;
        return 0x8041040d;
    }

    if (flags & 0x0f00) {
        switch (hdr[3] & 0x0f) {
        case 1: ret = 0x80410412; break;
        case 2: *done_out = 1; ret = 0x80410413; break;
        case 3: ret = 0x80410414; break;
        case 4: ret = 0x80410415; break;
        case 5: ret = 0x80410416; break;
        default: ret = 0x8041040d; break;
        }
    } else {
        *qdcount_out = 1;
        *ancount_out = __builtin_bswap16(*(u16 *)&hdr[6]);
    }
    *ptr_io = p;
    return ret;
}

static int sub_20d0(u8 **ptr_io, u8 *end, u8 *pkt_start, const char *qname, u8 *qname_end, u16 qdcount)
{
    u8 *p = *ptr_io;
    int match = 0;
    int i, ret = 0;

    for (i = 0; i < qdcount; i++) {
        ret = sub_25a4(&p, end, pkt_start, end, qname, qname_end, &match);
        if (ret < 0) {
            *ptr_io = p;
            return ret;
        }
        p += 4;
        if (end < p) {
            *ptr_io = p;
            return 0x8041040d;
        }
    }
    if (qdcount == 0 || match == 0)
        ret = 0x8041040d;
    *ptr_io = p;
    return ret;
}

static int sub_21c0(ResolverNode *node, u8 **ptr_io, u8 *end, u8 *pkt_start, const char *qname, u8 *qname_end, void *out_addr, int out_name, int type, int out_len, u16 ancount)
{
    u8 *p = *ptr_io;
    int i, ret = 0;
    int match = 0;

    (void)type;
    if (ancount == 0) {
        *ptr_io = p;
        return 0x80410411;
    }

    for (i = 0; i < ancount; i++) {
        ret = sub_25a4(&p, end, pkt_start, end, qname, qname_end, &match);
        if (ret < 0) {
            *ptr_io = p;
            return ret;
        }
        if (end < p + 10) {
            *ptr_io = p + 10;
            return 0x8041040d;
        }
        u16 rtype, rclass, rdlen;
        sceKernelMemcpy(&rtype, p, 2);
        rtype = __builtin_bswap16(rtype);
        sceKernelMemcpy(&rclass, p + 2, 2);
        rclass = __builtin_bswap16(rclass);
        sceKernelMemcpy(&rdlen, p + 8, 2);
        rdlen = __builtin_bswap16(rdlen);
        p += 10;
        u8 *rdata = p;
        p += rdlen;
        if (end < p) {
            *ptr_io = p;
            return 0x8041040d;
        }
        if (rclass == 1 && match != 0) {
            if (rtype == 5) {
                u8 *cname_end = node->buf + node->buflen;
                ret = sub_2418(rdata, end, (char *)qname, &cname_end, pkt_start, end);
                if (ret < 0) {
                    *ptr_io = p;
                    return ret;
                }
                qname_end = cname_end;
            } else if (rtype == 1) {
                if (rdlen != 4) {
                    *ptr_io = p;
                    return 0x8041040d;
                }
                sceKernelMemcpy(out_addr, rdata, 4);
                *ptr_io = p;
                return ret;
            } else if (rtype == 12) {
                u8 *name_end = (u8 *)(out_name + out_len);
                ret = sub_2418(rdata, end, (char *)out_name, &name_end, pkt_start, end);
                *ptr_io = p;
                return ret;
            }
        }
    }
    *ptr_io = p;
    return 0x80410411;
}

static int sub_2418(u8 *ptr, u8 *end, char *out_name, u8 **out_end_io, u8 *pkt_start, u8 *pkt_limit)
{
    int total_read = 0;

    for (;;) {
        if (end < ptr + 1)
            return 0x8041040d;
        u8 len;
        sceKernelMemcpy(&len, ptr, 1);
        u8 tag = len & 0xc0;
        if (tag == 0x10 || tag == 0x01)
            return 0x8041040d;
        if (tag == 0xc0) {
            if (end < ptr + 2)
                return 0x8041040d;
            u16 off = ((ptr[0] & 0x3f) << 8) | ptr[1];
            total_read += 2;
            if (total_read >= (int)(pkt_limit - pkt_start))
                return 0x8041040d;
            ptr = pkt_start + off;
            continue;
        }
        if (len == 0) {
            out_name[-1] = 0;
            *out_end_io = (u8 *)out_name;
            return 0;
        }
        ptr++;
        if (end < ptr + len)
            return 0x8041040d;
        total_read += 1 + len;
        if (*out_end_io < (u8 *)(out_name + len + 1))
            return 0x8041040c;
        sceKernelMemcpy(out_name, ptr, len);
        out_name[len] = '.';
        ptr += len;
        out_name += len + 1;
    }
}

static int sub_25a4(u8 **ptr_io, u8 *end, u8 *pkt_start, u8 *pkt_limit, const char *qname, u8 *qname_end, int *match_out)
{
    u8 *ptr = *ptr_io;
    u8 *next_ptr = 0;
    int total_read = 0;

    *match_out = 1;
    for (;;) {
        if (end < ptr + 1)
            goto err;
        u8 len;
        sceKernelMemcpy(&len, ptr, 1);
        u8 tag = len & 0xc0;
        if (tag == 0x10 || tag == 0x01)
            goto err;
        if (tag == 0xc0) {
            if (end < ptr + 2)
                goto err;
            u16 off = ((ptr[0] & 0x3f) << 8) | ptr[1];
            total_read += 2;
            if (total_read >= (int)(pkt_limit - pkt_start))
                goto err;
            if (next_ptr == 0)
                next_ptr = ptr + 2;
            ptr = pkt_start + off;
            continue;
        }
        if (len == 0) {
            if (qname[-1] != 0)
                *match_out = 0;
            *ptr_io = (next_ptr != 0) ? next_ptr : (ptr + 1);
            return 0;
        }
        ptr++;
        if (end < ptr + len)
            goto err;
        total_read += 1 + len;
        if (*match_out == 1) {
            if (qname_end < (u8 *)(qname + len + 1)) {
                *match_out = 0;
            } else if (sub_0e74(qname, (const char *)ptr, len) != 0) {
                *match_out = 0;
            } else if (qname[len] != '.' && qname[len] != 0) {
                *match_out = 0;
            } else {
                qname += len + 1;
            }
        }
        ptr += len;
    }

err:
    *ptr_io = (next_ptr != 0) ? next_ptr : (ptr + 1);
    return 0x8041040d;
}
