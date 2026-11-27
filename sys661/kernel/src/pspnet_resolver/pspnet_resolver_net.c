#include "pspnet_resolver_net.h"

static u16 g_dns_txid;

static int create_query_message(int type, const char *qname, u8 *pkt_buf, u32 *pkt_len_out, u16 *txid_out);
static int do_transaction(ResolverNode *node, u8 *rx_buf, u32 tx_len, const char *qname, u8 *qname_end, void *out_addr, int timeout, const u32 *dns_ip, int out_name, int out_len, int *done_out, u16 txid, int type);
static int create_dns_header(int type, u8 **ptr_io, u8 *end, u16 *txid_out);
static int set_domain_name(u8 **ptr_io, u8 *end, const char *qname);
static int send_query_message(ResolverNode *node, const void *buf, int len, const u32 *dns_ip, int timeout);
static int recv_query_message(ResolverNode *node, void *buf, int *len_io, int timeout);
static int parse_dns_header(u8 **ptr_io, u8 *end, u16 txid, u16 *qdcount_out, u16 *ancount_out, int *done_out);
static int skip_question_section(u8 **ptr_io, u8 *end, u8 *pkt_start, const char *qname, u8 *qname_end, u16 qdcount);
static int parse_answer_section(ResolverNode *node, u8 **ptr_io, u8 *end, u8 *pkt_start, const char *qname, u8 *qname_end, void *out_addr, int out_name, int type, int out_len, u16 ancount);
static int get_domain_name(u8 *ptr, u8 *end, char *out_name, u8 **out_end_io, u8 *pkt_start, u8 *pkt_limit);
static int skip_domain_name(u8 **ptr_io, u8 *end, u8 *pkt_start, u8 *pkt_limit, const char *qname, u8 *qname_end, int *match_out);

static inline int i8toa(char *dst, u8 val)
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

int _sce_pspnet_resolver_do_resolv(ResolverNode *node, int type, const void *name_or_addr, int out_len, void *out_buf, int timeout, int retry)
{
    u32 dns_ips[2];
    int done = 0;
    int ret;
    u32 rem_buflen = node->buflen;
    char *qname;
    u8 *qname_end;
    u8 *pkt_buf;
    int r_idx, d_idx;

    do {
        ret = sceNetInet_lib_AC9D90A5(&dns_ips[0], &dns_ips[1]);
        if (ret < 0)
            break;

        if (dns_ips[0] == 0 || dns_ips[0] == ~0U) {
            ret = 0x80410402;
            break;
        }

        if (type == 0) {
            const char *hname = (const char *)name_or_addr;
            SceSize hlen = sceNet_lib_D8722983(hname);
            if (hname[hlen - 1] == '.') {
                if (hlen >= rem_buflen) {
                    ret = 0x8041040c;
                    break;
                }
                qname = (char *)node->buf;
                sceNet_lib_B5CE388A(qname, hname, hlen - 1);
                qname[hlen - 1] = 0;
                pkt_buf = (u8 *)(qname + hlen);
                qname_end = pkt_buf;
                rem_buflen -= hlen;
            } else {
                qname = (char *)hname;
                qname_end = (u8 *)(hname + hlen + 1);
                pkt_buf = node->buf;
            }
        } else if (type == 1) {
            if (rem_buflen <= 0x1d) {
                ret = 0x8041040c;
                break;
            }
            const u8 *ip = (const u8 *)out_buf;
            char *p = (char *)node->buf;
            int n;
            n = i8toa(p, ip[3]); p += n; *p++ = '.';
            n = i8toa(p, ip[2]); p += n; *p++ = '.';
            n = i8toa(p, ip[1]); p += n; *p++ = '.';
            n = i8toa(p, ip[0]); p += n; *p++ = '.';
            SceSize arpa_len = sceNet_lib_D8722983("in-addr.arpa");
            sceNet_lib_B5CE388A(p, "in-addr.arpa", arpa_len + 1);
            SceSize total_qlen = (SceSize)(p - (char *)node->buf) + arpa_len + 1;
            qname = (char *)node->buf;
            pkt_buf = node->buf + total_qlen;
            qname_end = pkt_buf;
            rem_buflen -= total_qlen;
        } else {
            ret = 0x80410417;
            break;
        }

        r_idx = 0;
        d_idx = 0;
        if ((node->flags & 4) && node->async != 0) {
            d_idx = node->async->dns_idx;
            r_idx = node->async->retry_idx;
        }

        int stop = 0;
        for (; r_idx < retry && !stop; r_idx++) {
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
                ret = create_query_message(type, qname, pkt_buf, &pkt_len, &txid);
                if (ret < 0) {
                    stop = 1;
                    break;
                }

                int cur_d = d_idx++;
                done = 0;
                ret = do_transaction(node, pkt_buf, pkt_len, qname, qname_end, out_buf, timeout, &dns_ips[cur_d], (int)name_or_addr, out_len, &done, txid, type);
                if (done <= 0) {
                    stop = 1;
                    break;
                }
            }
        }
    } while (0);

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

int _sce_pspnet_resolver_net_init(void)
{
    u16 sys_time[4];
    sceKernelGetSystemTime(sys_time);
    SceUID thid = sceKernelGetThreadId();
    g_dns_txid = (sys_time[0] ^ sys_time[2]) ^ (u16)thid;
    return 0;
}

int _sce_pspnet_resolver_net_term(void)
{
    return 0;
}

static int create_query_message(int type, const char *qname, u8 *pkt_buf, u32 *pkt_len_io, u16 *txid_out)
{
    u8 *ptr = pkt_buf;
    u8 *end = pkt_buf + *pkt_len_io;
    int ret;
    u16 qtype, qclass;

    ret = create_dns_header(type, &ptr, end, txid_out);
    if (ret < 0)
        return ret;
    ret = set_domain_name(&ptr, end, qname);
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

static int do_transaction(ResolverNode *node, u8 *rx_buf, u32 tx_len, const char *qname, u8 *qname_end, void *out_addr, int timeout, const u32 *dns_ip, int out_name, int out_len, int *done_out, u16 txid, int type)
{
    int ret;
    u32 rx_len;
    u8 *ptr, *end;
    u16 qdcount, ancount;
    int io_err = 0;

    do {
        if ((node->flags & 4) == 0 || node->async == 0 || (u32)(node->async->state - 1) >= 2) {
            ret = send_query_message(node, rx_buf, tx_len, dns_ip, timeout);
            if (ret < 0) {
                io_err = 1;
                break;
            }
            if ((node->flags & 4) && node->async != 0) {
                node->async->state = 1;
                node->async->txid = txid;
            }
        }

        if ((node->flags & 4) && node->async != 0)
            txid = (u16)node->async->txid;

        rx_len = node->buflen - (u32)(rx_buf - node->buf);
        ret = recv_query_message(node, rx_buf, (int *)&rx_len, timeout);
        if (ret < 0) {
            io_err = 1;
            break;
        }

        ptr = rx_buf;
        end = rx_buf + rx_len;
        ret = parse_dns_header(&ptr, end, txid, &qdcount, &ancount, done_out);
        if (ret >= 0) {
            ret = skip_question_section(&ptr, end, rx_buf, qname, qname_end, qdcount);
            if (ret >= 0) {
                ret = parse_answer_section(node, &ptr, end, rx_buf, qname, qname_end, out_addr, out_name, type, out_len, ancount);
            }
        }
        if (ret == (int)0x8041040d) {
            if (ptr >= node->buf + node->buflen)
                ret = 0x8041040c;
        }
    } while (0);

    if (io_err) {
        if (ret != (int)0x8041040e && ret != (int)0x80410481)
            *done_out = 1;
    }

    if ((node->flags & 4) && node->async != 0 && (node->flags & 8) == 0) {
        node->flags &= ~4U;
        if (node->async->hostname != 0)
            sceNetIfhandle_lib_76BAD213(node->async->hostname);
        sceNetIfhandle_lib_76BAD213(node->async);
        node->async = 0;
    }
    if (node->sock >= 0) {
        if ((node->flags & 4) && node->async != 0 && ret == (int)0x80410481)
            return ret;
        sceNetInetClose(node->sock);
        node->sock = 0;
    }
    return ret;
}

static int create_dns_header(int type, u8 **ptr_io, u8 *end, u16 *txid_out)
{
    SceDnsHeader hdr;
    u8 *p = *ptr_io;
    int intr;
    u16 id;

    (void)type;
    if (end < p + 12)
        return 0x8041040c;

    sceKernelMemset(&hdr, 0, sizeof(hdr));
    intr = sceKernelCpuSuspendIntr();
    id = g_dns_txid++;
    *txid_out = id;
    sceKernelCpuResumeIntr(intr);

    hdr.id = id;
    hdr.flags = 1;
    hdr.qdcount = __builtin_bswap16(1);
    sceKernelMemcpy(p, &hdr, sizeof(hdr));
    *ptr_io = p + sizeof(hdr);
    return 0;
}

static int set_domain_name(u8 **ptr_io, u8 *end, const char *qname)
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

static int send_query_message(ResolverNode *node, const void *buf, int len, const u32 *dns_ip, int timeout)
{
    int s, ret;
    SceNetInetSockaddrIn sin;

    do {
        if (node->flags & 1) {
            ret = 0x8041040e;
            break;
        }
        s = sceNetInetSocket(2, 2, 0);
        if (s < 0) {
            ret = 0x8041040f;
            break;
        }
        node->sock = s;
        if ((node->flags & 4) && node->async != 0) {
            int val = 1;
            if (sceNetInetSetsockopt(s, 0xffff, 0x1009, &val, 4) < 0) {
                ret = 0x8041040f;
                break;
            }
            node->flags |= 8;
        } else {
            node->flags &= ~8U;
        }

        if (sceNetInetSetsockopt(node->sock, 0xffff, 0x1006, &timeout, 4) < 0) {
            ret = 0x8041040f;
            break;
        }

        sceKernelMemset(&sin, 0, sizeof(sin));
        sin.sin_len = sizeof(sin);
        sin.sin_family = 2;
        sin.sin_port = __builtin_bswap16(53);
        sin.sin_addr = *dns_ip;

        ret = sceNetInetSendto(node->sock, buf, len, 0, &sin, sizeof(sin));
        if (ret >= 0)
            return ret;
        ret = 0x8041040f;
    } while (0);

    if (node->sock >= 0) {
        if ((node->flags & 4) && ((node->async == 0) | (ret != (int)0x80410481)) == 0)
            return ret;
        sceNetInetClose(node->sock);
        node->sock = 0;
    }
    return ret;
}

static int recv_query_message(ResolverNode *node, void *buf, int *len_io, int timeout)
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

static int parse_dns_header(u8 **ptr_io, u8 *end, u16 txid, u16 *qdcount_out, u16 *ancount_out, int *done_out)
{
    SceDnsHeader hdr;
    u8 *p = *ptr_io;
    u8 *next = p + sizeof(hdr);
    int ret = 0;

    if (end < next) {
        *ptr_io = next;
        return 0x8041040d;
    }

    sceKernelMemcpy(&hdr, p, sizeof(hdr));
    p = next;

    if (hdr.id != txid) {
        *done_out = 1;
        *ptr_io = p;
        return 0x80410410;
    }

    u16 flags = hdr.flags;
    if ((flags & 0xf8) != 0x80) {
        *ptr_io = p;
        return 0x8041040d;
    }

    u16 qd = __builtin_bswap16(hdr.qdcount);
    if (qd != 1) {
        *ptr_io = p;
        return 0x8041040d;
    }

    if (flags & 0x0f00) {
        switch ((flags >> 8) & 0x0f) {
        case 1: ret = 0x80410412; break;
        case 2: *done_out = 1; ret = 0x80410413; break;
        case 3: ret = 0x80410414; break;
        case 4: ret = 0x80410415; break;
        case 5: ret = 0x80410416; break;
        default: ret = 0x8041040d; break;
        }
    } else {
        *qdcount_out = 1;
        *ancount_out = __builtin_bswap16(hdr.ancount);
    }
    *ptr_io = p;
    return ret;
}

static int skip_question_section(u8 **ptr_io, u8 *end, u8 *pkt_start, const char *qname, u8 *qname_end, u16 qdcount)
{
    u8 *p = *ptr_io;
    int match = 0;
    int i, ret = 0;

    for (i = 0; i < qdcount; i++) {
        ret = skip_domain_name(&p, end, pkt_start, end, qname, qname_end, &match);
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

static int parse_answer_section(ResolverNode *node, u8 **ptr_io, u8 *end, u8 *pkt_start, const char *qname, u8 *qname_end, void *out_addr, int out_name, int type, int out_len, u16 ancount)
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
        ret = skip_domain_name(&p, end, pkt_start, end, qname, qname_end, &match);
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
                ret = get_domain_name(rdata, end, (char *)end, &cname_end, pkt_start, end);
                if (ret < 0) {
                    *ptr_io = p;
                    return ret;
                }
                qname = (const char *)end;
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
                ret = get_domain_name(rdata, end, (char *)out_name, &name_end, pkt_start, end);
                *ptr_io = p;
                return ret;
            }
        }
    }
    *ptr_io = p;
    return 0x80410411;
}

static int get_domain_name(u8 *ptr, u8 *end, char *out_name, u8 **out_end_io, u8 *pkt_start, u8 *pkt_limit)
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

static int skip_domain_name(u8 **ptr_io, u8 *end, u8 *pkt_start, u8 *pkt_limit, const char *qname, u8 *qname_end, int *match_out)
{
    u8 *ptr = *ptr_io;
    u8 *next_ptr = 0;
    int total_read = 0;

    *match_out = 1;
    for (;;) {
        if (end < ptr + 1)
            break;
        u8 len;
        sceKernelMemcpy(&len, ptr, 1);
        u8 tag = len & 0xc0;
        if (tag == 0x10 || tag == 0x01)
            break;
        if (tag == 0xc0) {
            if (end < ptr + 2)
                break;
            u16 off = ((ptr[0] & 0x3f) << 8) | ptr[1];
            total_read += 2;
            if (total_read >= (int)(pkt_limit - pkt_start))
                break;
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
            break;
        total_read += 1 + len;
        if (*match_out == 1) {
            if (qname_end < (u8 *)(qname + len + 1)) {
                *match_out = 0;
            } else if (_sce_pspnet_resolver_strncasecmp(qname, (const char *)ptr, len) != 0) {
                *match_out = 0;
            } else if (qname[len] != '.' && qname[len] != 0) {
                *match_out = 0;
            } else {
                qname += len + 1;
            }
        }
        ptr += len;
    }

    *ptr_io = (next_ptr != 0) ? next_ptr : (ptr + 1);
    return 0x8041040d;
}
