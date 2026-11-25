#include "apctl_internal.h"

#define DHCP_ERR_ALREADY_STARTED ((int)0x80410580)
#define DHCP_ERR_BPF             ((int)0x80410582)
#define DHCP_ERR_SOCKET          ((int)0x80410583)
#define DHCP_ERR_PARAM           ((int)0x80410584)
#define DHCP_ERR_BUILD           ((int)0x80410586)
#define DHCP_ERR_PKT             ((int)0x80410501)
#define DHCP_ERR_TIMEOUT         ((int)0x80410502)
#define DHCP_ERR_EXPIRED         ((int)0x80410504)
#define DHCP_ERR_NAK             ((int)0x80410505)

static const u32 g_dhcp_magic = 0x63538263u; /* 99.130.83.99 in network byte order */

/* BPF filter for DHCP UDP port 68 */
static BpfInsn g_dhcp_bpf_insns[11] = {
    { 0x28, 0, 0, 12 },
    { 0x15, 0, 8, 0x0800 },
    { 0x28, 0, 0, 20 },
    { 0x45, 6, 0, 0x1FFF },
    { 0x30, 0, 0, 23 },
    { 0x15, 0, 4, 17 },
    { 0xB1, 0, 0, 14 },
    { 0x48, 0, 0, 16 },
    { 0x15, 0, 1, 68 },
    { 0x06, 0, 0, 0xFFFFFFFFu },
    { 0x06, 0, 0, 0 }
};
static BpfProgram g_dhcp_bpf_prog = { 11, g_dhcp_bpf_insns };

/* BPF filter for ARP reply matching target IP */
static BpfInsn g_arp_bpf_insns[8] = {
    { 0x28, 0, 0, 12 },
    { 0x15, 0, 5, 0x0806 },
    { 0x28, 0, 0, 20 },
    { 0x15, 0, 3, 2 },
    { 0x20, 0, 0, 28 },
    { 0x15, 0, 1, 0 },
    { 0x06, 0, 0, 0xFFFFFFFFu },
    { 0x06, 0, 0, 0 }
};
static BpfProgram g_arp_bpf_prog = { 8, g_arp_bpf_insns };

/* DHCP client context (5392 bytes = 0x1510, matching original layout) */
static int g_dhcp_running = -1;
static u8 g_dhcp_ctx[5392];

#define D_U32(p, off) (*(u32 *)((u8 *)(p) + (off)))
#define D_S32(p, off) (*(s32 *)((u8 *)(p) + (off)))
#define D_U16(p, off) (*(u16 *)((u8 *)(p) + (off)))
#define D_U8(p, off)  (*(u8 *)((u8 *)(p) + (off)))

static u16 ip_checksum(const u16 *buf, int words)
{
    u32 sum = 0;
    while (words-- > 0)
        sum += *buf++;
    sum = (sum & 0xFFFF) + (sum >> 16);
    sum += (sum >> 16);
    u16 res = (u16)(~sum);
    return (res == 0) ? 0xFFFF : res;
}

static u16 udp_checksum(const u16 *pseudo, const u16 *udp, int len)
{
    u32 sum = 0;
    int i;
    for (i = 0; i < 6; i++)
        sum += pseudo[i];
    while (len >= 2) {
        sum += *udp++;
        len -= 2;
    }
    if (len == 1)
        sum += *(const u8 *)udp;
    sum = (sum & 0xFFFF) + (sum >> 16);
    sum += (sum >> 16);
    u16 res = (u16)(~sum);
    return (res == 0) ? 0xFFFF : res;
}

static u32 default_netmask(const u32 *ip)
{
    u32 a = *ip;
    if ((a & 0x80) == 0)
        return 0x000000FFu;
    if ((a & 0xC0) == 0x80)
        return 0x0000FFFFu;
    return 0x00FFFFFFu;
}

static int dhcp_clear_if(u8 *ctx)
{
    int ret = sceNetInet_lib_E6F67D14();
    if (ret >= 0) {
        ret = sceNetInet_lib_6A046357((const char *)ctx);
        if (ret > 0)
            ret = 0;
    }
    return ret;
}

static void dhcp_lease_clear(void *lease)
{
    if (lease != NULL)
        sceKernelMemset(lease, 0, 1028);
}

static void dhcp_copy_str(const u8 *src, u8 *dst, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (src[i] != 0)
            *dst++ = src[i];
    }
    *dst = 0;
}

static int dhcp_opt_len_valid(u8 code, u8 len)
{
    switch (code) {
    case 1: case 28: case 51: case 54: case 58: case 59:
        return (len == 4) ? 0 : -1;
    case 3: case 6:
        return ((len & 3) == 0) ? 0 : -1;
    case 12: case 15: case 56:
        return (len != 0) ? 0 : -1;
    case 53:
        return (len == 1) ? 0 : -1;
    default:
        return 0;
    }
}

static int dhcp_validate_section(const u8 *pkt, int pkt_len, int sec, u32 *overload)
{
    const u8 *p, *end;
    if (sec == 0) {
        p = pkt + 240;
        end = pkt + pkt_len;
    } else if (sec == 1) {
        p = pkt + 108;
        end = pkt + 236;
    } else if (sec == 2) {
        p = pkt + 44;
        end = pkt + 108;
    } else {
        return -1;
    }

    while (p < end) {
        u8 code = *p;
        if (code == 0) {
            p++;
            continue;
        }
        if (code == 255)
            return 0;
        if (end - p < 2)
            return -2;
        u8 len = p[1];
        if (end - p < len + 2)
            return -2;
        if (dhcp_opt_len_valid(code, len) < 0)
            return -1;
        if (code == 52 && sec == 0 && overload != NULL) {
            if (p[2] == 1) *overload &= 1;
            else if (p[2] == 2) *overload &= 2;
            else if (p[2] == 3) *overload = 0;
        }
        p += len + 2;
    }
    return 0;
}

static int dhcp_validate_opts(const u8 *pkt, int pkt_len)
{
    u32 overload = 0;
    int r = dhcp_validate_section(pkt, pkt_len, 0, &overload);
    if (r == -1 || r == -2)
        return -1;
    if (overload & 1) {
        r = dhcp_validate_section(pkt, pkt_len, 1, NULL);
        if (r == -1 || r == -2)
            return -1;
    }
    if (overload & 2) {
        r = dhcp_validate_section(pkt, pkt_len, 2, NULL);
        if (r == -1 || r == -2)
            return -1;
    }
    return 0;
}

static const u8 *dhcp_find_opt_sec(const u8 *pkt, int pkt_len, u8 target, int sec, u32 *overload)
{
    const u8 *p;
    int len;
    if (sec == 0) {
        p = pkt + 240;
        len = pkt_len - 240;
    } else if (sec == 1) {
        p = pkt + 108;
        len = 128;
    } else if (sec == 2) {
        p = pkt + 44;
        len = 64;
    } else {
        return NULL;
    }

    int off = 0;
    while (off < len) {
        u8 code = p[off];
        if (code == target)
            return &p[off];
        if (code == 255)
            return NULL;
        if (code == 0) {
            off++;
            continue;
        }
        if (code == 52 && sec == 0 && overload != NULL) {
            if (off + 2 >= len)
                return NULL;
            u8 v = p[off + 2];
            if (v == 1) *overload &= 1;
            else if (v == 2) *overload &= 2;
            else if (v == 3) *overload = 0;
            off += 3;
            continue;
        }
        if (off + 1 >= len)
            return NULL;
        off += p[off + 1] + 2;
    }
    return NULL;
}

static const u8 *dhcp_find_opt(const u8 *pkt, int pkt_len, u8 target)
{
    u32 overload = 0;
    const u8 *opt = dhcp_find_opt_sec(pkt, pkt_len, target, 0, &overload);
    if (opt != NULL)
        return opt;
    if (overload & 1) {
        opt = dhcp_find_opt_sec(pkt, pkt_len, target, 1, NULL);
        if (opt != NULL)
            return opt;
    }
    if (overload & 2)
        return dhcp_find_opt_sec(pkt, pkt_len, target, 2, NULL);
    return NULL;
}

static void dhcp_parse_one_opt(const u8 *opt, u8 *lease)
{
    u8 code = opt[0];
    u8 len = opt[1];
    switch (code) {
    case 1:
        sceKernelMemcpy(lease + 1004, opt + 2, 4);
        break;
    case 28:
        sceKernelMemcpy(lease + 1008, opt + 2, 4);
        break;
    case 54:
        sceKernelMemcpy(lease + 1000, opt + 2, 4);
        break;
    case 3:
        sceKernelMemset(lease + 1012, 0, 4);
        sceKernelMemcpy(lease + 1012, opt + 2, 4);
        break;
    case 6:
        sceKernelMemset(lease + 1016, 0, 4);
        sceKernelMemset(lease + 1020, 0, 4);
        sceKernelMemcpy(lease + 1016, opt + 2, 4);
        if (len >= 8)
            sceKernelMemcpy(lease + 1020, opt + 6, 4);
        break;
    case 12: {
        int n = (len < 255) ? len : 255;
        sceKernelMemset(lease + 217, 0, 256);
        sceKernelMemcpy(lease + 217, opt + 2, (u32)n);
        lease[217 + n] = 0;
        break;
    }
    case 15: {
        int n = (len < 255) ? len : 255;
        sceKernelMemset(lease + 473, 0, 256);
        sceKernelMemcpy(lease + 473, opt + 2, (u32)n);
        lease[473 + n] = 0;
        break;
    }
    case 56: {
        int n = (len < 255) ? len : 255;
        sceKernelMemset(lease + 729, 0, 256);
        sceKernelMemcpy(lease + 729, opt + 2, (u32)n);
        lease[729 + n] = 0;
        break;
    }
    case 51: case 58: case 59: {
        u32 val = 0;
        if (len == 4)
            val = ((u32)opt[2] << 24) | ((u32)opt[3] << 16) | ((u32)opt[4] << 8) | opt[5];
        if (code == 51) *(u32 *)(lease + 988) = val;
        else if (code == 58) *(u32 *)(lease + 992) = val;
        else if (code == 59) *(u32 *)(lease + 996) = val;
        break;
    }
    case 53:
        lease[1024] = opt[2];
        break;
    default:
        break;
    }
}

static int dhcp_parse_section(const u8 *pkt, int pkt_len, u8 *lease, int sec, u32 *overload)
{
    const u8 *p, *end;
    if (sec == 0) {
        p = pkt + 240;
        end = pkt + pkt_len;
    } else if (sec == 1) {
        p = pkt + 108;
        end = pkt + 236;
    } else if (sec == 2) {
        p = pkt + 44;
        end = pkt + 108;
    } else {
        return 0;
    }

    while (p < end) {
        u8 code = *p;
        if (code == 0) {
            p++;
            continue;
        }
        if (code == 255 || end - p < 2)
            return 0;
        if (code == 52 && sec == 0 && overload != NULL) {
            if (end - p < p[1] + 2)
                return 0;
            if (p[2] == 1) *overload &= 1;
            else if (p[2] == 2) *overload &= 2;
            else if (p[2] == 3) *overload = 0;
        } else {
            dhcp_parse_one_opt(p, lease);
        }
        p += p[1] + 2;
    }
    return 0;
}

static int dhcp_parse_lease(const u8 *pkt, int pkt_len, u8 *lease)
{
    u32 overload = 0;
    sceKernelMemset(lease, 0, 9);
    dhcp_parse_section(pkt, pkt_len, lease, 0, &overload);

    sceKernelMemset(lease + 89, 0, 128);
    if (overload & 1) {
        dhcp_parse_section(pkt, pkt_len, lease, 1, NULL);
    } else {
        int l = sceNet_lib_D8722983((const char *)(pkt + 108));
        sceNet_lib_B5CE388A((char *)(lease + 89), (const char *)(pkt + 108), l + 1);
    }

    sceKernelMemset(lease + 25, 0, 64);
    if (overload & 2) {
        dhcp_parse_section(pkt, pkt_len, lease, 2, NULL);
    } else {
        int l = sceNet_lib_D8722983((const char *)(pkt + 44));
        sceNet_lib_B5CE388A((char *)(lease + 25), (const char *)(pkt + 44), l + 1);
    }

    /* Copy ciaddr, yiaddr, siaddr, giaddr (16 bytes at pkt+12 -> lease+9) */
    sceKernelMemcpy(lease + 9, pkt + 12, 16);

    u32 lease_t = *(u32 *)(lease + 988);
    if ((lease[7] & 0x20) == 0) {
        lease[7] |= 0x20;
        *(u32 *)(lease + 992) = lease_t >> 1;
    }
    u32 t1 = *(u32 *)(lease + 992);
    u32 span = (t1 + 3) >> 2;
    if (span == 0)
        span = 1;
    *(u32 *)(lease + 992) = ((3 * t1 + 9) >> 2) + ((u32)sceNet_lib_1858883D() % span);
    if ((lease[7] & 0x10) == 0) {
        lease[7] |= 0x10;
        *(u32 *)(lease + 996) = 7 * (lease_t >> 3);
    }
    u32 t2 = *(u32 *)(lease + 996);
    if (t2 < *(u32 *)(lease + 992))
        *(u32 *)(lease + 992) = (3 * t2) >> 2;
    return 0;
}

static int dhcp_verify_rx_common(u8 *ctx)
{
    u8 *ip = (u8 *)D_U32(ctx, 3632);
    u8 *udp = (u8 *)D_U32(ctx, 3636);
    u8 *bootp = (u8 *)D_U32(ctx, 3640);
    u16 ip_sum = (u16)(((u16)ip[11] << 8) | ip[10]);
    ip[10] = 0; ip[11] = 0;
    u16 udp_sum = (u16)(((u16)udp[7] << 8) | udp[6]);
    udp[6] = 0; udp[7] = 0;

    u8 pseudo[12];
    sceKernelMemcpy(pseudo, ip + 12, 8);
    pseudo[8] = 0;
    pseudo[9] = 17;
    pseudo[10] = udp[4];
    pseudo[11] = udp[5];

    u16 ip_len = (u16)(((u16)ip[2] << 8) | ip[3]);
    u16 udp_len = (u16)(((u16)udp[4] << 8) | udp[5]);
    if (ip_len < 268 || udp_len < 248)
        return DHCP_ERR_PKT;
    if (ip_sum != ip_checksum((const u16 *)ip, 2 * (ip[0] & 0xF)))
        return DHCP_ERR_PKT;
    if (udp_sum != 0 && udp_sum != udp_checksum((const u16 *)pseudo, (const u16 *)udp, udp_len))
        return DHCP_ERR_PKT;
    if (bootp[0] != 2)
        return DHCP_ERR_PKT;
    u8 *tx_bootp = (u8 *)D_U32(ctx, 3668);
    if (*(u32 *)(bootp + 4) != *(u32 *)(tx_bootp + 4))
        return DHCP_ERR_PKT;
    if (sceNet_lib_E0A81C7C(bootp + 236, &g_dhcp_magic, 4) != 0)
        return DHCP_ERR_PKT;
    return 0;
}

static int dhcp_verify_offer(u8 *ctx)
{
    int ret = dhcp_verify_rx_common(ctx);
    if (ret < 0)
        return ret;
    u8 *udp = (u8 *)D_U32(ctx, 3636);
    u8 *bootp = (u8 *)D_U32(ctx, 3640);
    u16 udp_len = (u16)(((u16)udp[4] << 8) | udp[5]);
    const u8 *opt = dhcp_find_opt(bootp, udp_len - 8, 53);
    if (opt == NULL || opt[2] != 2)
        return DHCP_ERR_PKT;
    return 0;
}

static void dhcp_wrap_eth_ip_udp(u8 *ctx, u8 *tx_ptrs, u32 src_ip, u32 dst_ip)
{
    u8 *eth = (u8 *)D_U32(tx_ptrs, 0);
    u8 *ip = (u8 *)D_U32(tx_ptrs, 4);
    u8 *udp = (u8 *)D_U32(tx_ptrs, 8);
    u8 *mac = ctx + 3686;

    *(u16 *)(udp + 0) = bswap16(68);
    *(u16 *)(udp + 2) = bswap16(67);
    *(u16 *)(udp + 4) = bswap16(556);
    D_U32(tx_ptrs, 16) = src_ip;
    D_U32(tx_ptrs, 20) = dst_ip;
    tx_ptrs[24] = 0;
    tx_ptrs[25] = 17;
    tx_ptrs[26] = udp[4];
    tx_ptrs[27] = udp[5];
    udp[6] = 0; udp[7] = 0;
    *(u16 *)(udp + 6) = udp_checksum((const u16 *)(tx_ptrs + 16), (const u16 *)udp, 556);

    ip[0] = 0x45;
    ip[1] = 0;
    *(u16 *)(ip + 2) = bswap16(576);
    {
        u16 mac_tmp[3];
        u32 now = sceNet_lib_75D9985C();
        sceKernelMemcpy(mac_tmp, mac, 6);
        u32 s1 = ip_checksum(mac_tmp, 3);
        u32 s2 = ip_checksum((const u16 *)&now, 2);
        *(u16 *)(ip + 4) = (u16)(~(s2 + (((s1 << 16) + s2) >> 16)));
    }
    *(u16 *)(ip + 6) = bswap16(0x4000);
    ip[8] = 32;
    ip[9] = 17;
    *(u32 *)(ip + 12) = src_ip;
    *(u32 *)(ip + 16) = dst_ip;
    ip[10] = 0; ip[11] = 0;
    u16 csum = ip_checksum((const u16 *)ip, 10);
    ip[10] = (u8)(csum & 0xFF);
    ip[11] = (u8)(csum >> 8);

    sceKernelMemset(eth, 0xFF, 6);
    sceKernelMemcpy(eth + 6, mac, 6);
    *(u16 *)(eth + 12) = bswap16(0x0800);
}

static u32 dhcp_make_xid(const u8 *mac)
{
    u16 mac_tmp[3];
    u32 now = sceNet_lib_75D9985C();
    sceKernelMemcpy(mac_tmp, mac, 6);
    return ((u32)ip_checksum(mac_tmp, 3) << 16) + ip_checksum((const u16 *)&now, 2);
}

static void dhcp_build_discover(u8 *ctx)
{
    u8 *tx_ptrs = ctx + 3656;
    u8 *bootp = (u8 *)D_U32(tx_ptrs, 12);
    sceKernelMemset(ctx + 3032, 0, D_U32(ctx, 3624) + 2);
    bootp[0] = 1;
    bootp[1] = ctx[3684];
    bootp[2] = ctx[3685];
    *(u32 *)(bootp + 4) = dhcp_make_xid(ctx + 3686);
    sceKernelMemcpy(bootp + 28, ctx + 3686, bootp[2]);
    sceKernelMemcpy(bootp + 236, &g_dhcp_magic, 4);

    int off = 4;
    bootp[236 + off++] = 53; bootp[236 + off++] = 1; bootp[236 + off++] = 1;
    if (D_U32(ctx, 3696) != 0) {
        bootp[236 + off++] = 50; bootp[236 + off++] = 4;
        sceKernelMemcpy(bootp + 236 + off, ctx + 3696, 4);
        off += 4;
    }
    u8 *cid = (u8 *)D_U32(ctx, 3960);
    if (cid != NULL && off + cid[1] + 3 < 312) {
        bootp[236 + off++] = 61;
        bootp[236 + off++] = (u8)(cid[1] + 1);
        bootp[236 + off++] = cid[0];
        sceKernelMemcpy(bootp + 236 + off, (void *)D_U32(cid, 4), cid[1]);
        off += cid[1];
    }
    if (off + 4 < 312) {
        u16 maxsz = bswap16(576);
        bootp[236 + off++] = 57; bootp[236 + off++] = 2;
        sceKernelMemcpy(bootp + 236 + off, &maxsz, 2);
        off += 2;
    }
    if (ctx[5124] != 0) {
        int hlen = sceNet_lib_D8722983((const char *)(ctx + 5124)) + 1;
        if (off + hlen + 2 < 312) {
            bootp[236 + off++] = 12;
            bootp[236 + off++] = (u8)hlen;
            sceKernelMemcpy(bootp + 236 + off, ctx + 5124, (u32)hlen);
            off += hlen;
        }
    }
    if (ctx[3704] != 0 && off + ctx[3704] + 2 < 312) {
        bootp[236 + off++] = 55;
        bootp[236 + off++] = ctx[3704];
        sceKernelMemcpy(bootp + 236 + off, ctx + 3705, ctx[3704]);
        off += ctx[3704];
    }
    bootp[236 + off] = 255;
    dhcp_wrap_eth_ip_udp(ctx, tx_ptrs, 0, 0xFFFFFFFFu);
}

static int dhcp_build_request(u8 *ctx, int mode)
{
    u8 *tx_ptrs = ctx + 3656;
    u8 *bootp = (u8 *)D_U32(tx_ptrs, 12);
    u8 *lease = (u8 *)D_U32(ctx, 3972);
    sceKernelMemset(ctx + 3032, 0, D_U32(ctx, 3624) + 2);

    bootp[0] = 1;
    bootp[1] = ctx[3684];
    bootp[2] = ctx[3685];
    *(u32 *)(bootp + 4) = dhcp_make_xid(ctx + 3686);
    if (mode == 3)
        *(u32 *)(bootp + 12) = 0;
    else
        sceKernelMemcpy(bootp + 12, lease + 13, 4);
    sceKernelMemcpy(bootp + 28, ctx + 3686, bootp[2]);
    sceKernelMemcpy(bootp + 236, &g_dhcp_magic, 4);

    int off = 4;
    bootp[236 + off++] = 53; bootp[236 + off++] = 1; bootp[236 + off++] = 3;
    u8 *cid = (u8 *)D_U32(ctx, 3960);
    if (cid != NULL && off + cid[1] + 3 < 312) {
        bootp[236 + off++] = 61;
        bootp[236 + off++] = (u8)(cid[1] + 1);
        bootp[236 + off++] = cid[0];
        sceKernelMemcpy(bootp + 236 + off, (void *)D_U32(cid, 4), cid[1]);
        off += cid[1];
    }
    if (mode == 3) {
        u32 yiaddr;
        sceKernelMemcpy(&yiaddr, lease + 13, 4);
        if (yiaddr == 0 || off + 6 >= 312)
            return DHCP_ERR_BUILD;
        bootp[236 + off++] = 50; bootp[236 + off++] = 4;
        sceKernelMemcpy(bootp + 236 + off, lease + 13, 4);
        off += 4;
        if (D_U32(lease, 1000) == 0 || off + 6 >= 312)
            return DHCP_ERR_BUILD;
        bootp[236 + off++] = 54; bootp[236 + off++] = 4;
        sceKernelMemcpy(bootp + 236 + off, lease + 1000, 4);
        off += 4;
    }
    if (off + 4 < 312) {
        u16 maxsz = bswap16(576);
        bootp[236 + off++] = 57; bootp[236 + off++] = 2;
        sceKernelMemcpy(bootp + 236 + off, &maxsz, 2);
        off += 2;
    }
    if (ctx[5124] != 0) {
        int hlen = sceNet_lib_D8722983((const char *)(ctx + 5124)) + 1;
        if (off + hlen + 2 < 312) {
            bootp[236 + off++] = 12;
            bootp[236 + off++] = (u8)hlen;
            sceKernelMemcpy(bootp + 236 + off, ctx + 5124, (u32)hlen);
            off += hlen;
        }
    }
    if (ctx[3704] != 0 && off + ctx[3704] + 2 < 312) {
        bootp[236 + off++] = 55;
        bootp[236 + off++] = ctx[3704];
        sceKernelMemcpy(bootp + 236 + off, ctx + 3705, ctx[3704]);
        off += ctx[3704];
    }
    bootp[236 + off] = 255;

    if (mode == 3) {
        dhcp_wrap_eth_ip_udp(ctx, tx_ptrs, 0, 0xFFFFFFFFu);
        return 0;
    }
    if (mode == 6) {
        u32 yiaddr;
        sceKernelMemcpy(&yiaddr, lease + 13, 4);
        dhcp_wrap_eth_ip_udp(ctx, tx_ptrs, yiaddr, 0xFFFFFFFFu);
        return 0;
    }
    if (mode == 5)
        return 0;
    return DHCP_ERR_BUILD;
}

static int dhcp_build_decline(u8 *ctx, int in_use)
{
    u8 *tx_ptrs = ctx + 3656;
    u8 *bootp = (u8 *)D_U32(tx_ptrs, 12);
    u8 *lease = (u8 *)D_U32(ctx, 3972);
    char msg[256], ipbuf[16];

    sceKernelMemset(ctx + 3032, 0, D_U32(ctx, 3624) + 2);
    sceNet_lib_80C9F02A(msg, "Such IP address (");
    int pos = sceNet_lib_D8722983(msg);
    if (sceNetInetInetNtop(2, lease + 13, ipbuf, 16) != NULL) {
        sceNet_lib_80C9F02A(msg + pos, ipbuf);
        pos += sceNet_lib_D8722983(ipbuf);
    }
    sceNet_lib_80C9F02A(msg + pos, in_use ? ") is already in use" : ") isn't preferable");

    bootp[0] = 1; bootp[1] = ctx[3684]; bootp[2] = ctx[3685];
    *(u32 *)(bootp + 4) = dhcp_make_xid(ctx + 3686);
    sceKernelMemcpy(bootp + 28, ctx + 3686, bootp[2]);
    sceKernelMemcpy(bootp + 236, &g_dhcp_magic, 4);

    int off = 4;
    bootp[236 + off++] = 53; bootp[236 + off++] = 1; bootp[236 + off++] = 4;
    u32 yiaddr;
    sceKernelMemcpy(&yiaddr, lease + 13, 4);
    if (yiaddr == 0)
        return DHCP_ERR_BUILD;
    bootp[236 + off++] = 50; bootp[236 + off++] = 4;
    sceKernelMemcpy(bootp + 236 + off, lease + 13, 4);
    off += 4;
    u8 *cid = (u8 *)D_U32(ctx, 3960);
    if (cid != NULL && off + cid[1] + 3 < 312) {
        bootp[236 + off++] = 61;
        bootp[236 + off++] = (u8)(cid[1] + 1);
        bootp[236 + off++] = cid[0];
        sceKernelMemcpy(bootp + 236 + off, (void *)D_U32(cid, 4), cid[1]);
        off += cid[1];
    }
    if (D_U32(lease, 1000) == 0 || off + 6 >= 312)
        return DHCP_ERR_BUILD;
    bootp[236 + off++] = 54; bootp[236 + off++] = 4;
    sceKernelMemcpy(bootp + 236 + off, lease + 1000, 4);
    off += 4;
    int mlen = sceNet_lib_D8722983(msg);
    if (off + mlen + 2 < 312) {
        bootp[236 + off++] = 56;
        bootp[236 + off++] = (u8)mlen;
        sceKernelMemcpy(bootp + 236 + off, msg, (u32)mlen);
        off += mlen;
    }
    bootp[236 + off] = 255;
    dhcp_wrap_eth_ip_udp(ctx, tx_ptrs, 0, 0xFFFFFFFFu);
    return 0;
}

static int dhcp_build_release(u8 *ctx)
{
    u8 *tx_ptrs = ctx + 3656;
    u8 *bootp = (u8 *)D_U32(tx_ptrs, 12);
    u8 *lease = (u8 *)D_U32(ctx, 3972);
    char msg[256], ipbuf[16];

    sceKernelMemset(ctx + 3032, 0, D_U32(ctx, 3624) + 2);
    sceNet_lib_80C9F02A(msg, "I'm no longer need the IP address (");
    int pos = sceNet_lib_D8722983(msg);
    if (sceNetInetInetNtop(2, lease + 13, ipbuf, 16) != NULL) {
        sceNet_lib_80C9F02A(msg + pos, ipbuf);
        pos += sceNet_lib_D8722983(ipbuf);
    }
    sceNet_lib_80C9F02A(msg + pos, ")");

    bootp[0] = 1; bootp[1] = ctx[3684]; bootp[2] = ctx[3685];
    *(u32 *)(bootp + 4) = dhcp_make_xid(ctx + 3686);
    sceKernelMemcpy(bootp + 12, lease + 13, 4);
    sceKernelMemcpy(bootp + 28, ctx + 3686, bootp[2]);
    sceKernelMemcpy(bootp + 236, &g_dhcp_magic, 4);

    int off = 4;
    bootp[236 + off++] = 53; bootp[236 + off++] = 1; bootp[236 + off++] = 7;
    u8 *cid = (u8 *)D_U32(ctx, 3960);
    if (cid != NULL && off + cid[1] + 3 < 312) {
        bootp[236 + off++] = 61;
        bootp[236 + off++] = (u8)(cid[1] + 1);
        bootp[236 + off++] = cid[0];
        sceKernelMemcpy(bootp + 236 + off, (void *)D_U32(cid, 4), cid[1]);
        off += cid[1];
    }
    if (D_U32(lease, 1000) == 0 || off + 6 >= 312)
        return DHCP_ERR_BUILD;
    bootp[236 + off++] = 54; bootp[236 + off++] = 4;
    sceKernelMemcpy(bootp + 236 + off, lease + 1000, 4);
    off += 4;
    int mlen = sceNet_lib_D8722983(msg);
    if (off + mlen + 2 < 312) {
        bootp[236 + off++] = 56;
        bootp[236 + off++] = (u8)mlen;
        sceKernelMemcpy(bootp + 236 + off, msg, (u32)mlen);
        off += mlen;
    }
    bootp[236 + off] = 255;
    return 0;
}

static int dhcp_open_unicast_sock(int sock)
{
    if (sock >= 0)
        return sock;
    sock = sceNetInetSocket(2, 2, 0);
    if (sock < 0)
        return DHCP_ERR_SOCKET;
    u8 sa[16];
    sceKernelMemset(sa, 0, sizeof(sa));
    sa[1] = 2;
    *(u16 *)(sa + 2) = bswap16(68);
    int sz = 548, reuse = 1;
    if (sceNetInetBind(sock, sa, 16) < 0 ||
        sceNetInetSetsockopt(sock, 0xFFFF, 4097, &sz, 4) < 0 ||
        sceNetInetSetsockopt(sock, 0xFFFF, 512, &reuse, 4) < 0) {
        sceNetInetClose(sock);
        return DHCP_ERR_SOCKET;
    }
    return sock;
}

static int dhcp_send_unicast(int sock, const u32 *server_ip, void *bootp)
{
    u8 sa[16];
    u32 iov[2];
    u32 msg[7];
    sceKernelMemset(sa, 0, sizeof(sa));
    sa[1] = 2;
    *(u16 *)(sa + 2) = bswap16(67);
    *(u32 *)(sa + 4) = *server_ip;
    iov[0] = (u32)bootp;
    iov[1] = 548;
    sceKernelMemset(msg, 0, sizeof(msg));
    msg[0] = (u32)sa;
    msg[1] = 16;
    msg[2] = (u32)iov;
    msg[3] = 1;
    if (sceNetInetSendmsg(sock, msg, 0) < 0) {
        sceNetInetClose(sock);
        return DHCP_ERR_SOCKET;
    }
    return 0;
}

static void dhcp_abort_exit(u8 *ctx, int state, int err)
{
    if (state >= 4 && state <= 6) {
        if (dhcp_build_release(ctx) >= 0) {
            int s = dhcp_open_unicast_sock(D_S32(ctx, 5388));
            D_S32(ctx, 5388) = s;
            if (s >= 0) {
                dhcp_send_unicast(s, (const u32 *)(D_U32(ctx, 3972) + 1000), (void *)D_U32(ctx, 3668));
                sceKernelDelayThread(100000);
                if (D_S32(ctx, 5388) >= 0) {
                    sceNetInetClose(D_S32(ctx, 5388));
                    D_S32(ctx, 5388) = -1;
                }
            }
        }
    }
    sceNetInet_lib_DC38FEE9(ctx + 5064, ctx + 5070);
    dhcp_clear_if(ctx);
    void (*cb)(int, int) = (void (*)(int, int))D_U32(ctx, 5084);
    if (cb != NULL)
        cb(err != 0 ? 4 : 1, err);
    sceKernelExitThread(0);
}

static int dhcp_arp_send_req(u8 *ctx, const void *src_ip, const u32 *target_ip)
{
    u8 frame[42];
    sceKernelMemset(frame, 0, sizeof(frame));
    g_arp_bpf_insns[5].k = bswap32(*target_ip);
    if (sceNet_lib_E1F4696F(D_S32(ctx, 20), 0x80084267u, &g_arp_bpf_prog) < 0)
        return 0;
    sceKernelMemset(frame, 0xFF, 6);
    sceKernelMemcpy(frame + 6, ctx + 3686, 6);
    *(u16 *)(frame + 12) = bswap16(0x0806);
    *(u16 *)(frame + 14) = bswap16(1);
    *(u16 *)(frame + 16) = bswap16(0x0800);
    frame[18] = 6;
    frame[19] = 4;
    *(u16 *)(frame + 20) = bswap16(1);
    sceKernelMemcpy(frame + 22, ctx + 3686, 6);
    sceKernelMemcpy(frame + 28, src_ip, 4);
    sceKernelMemcpy(frame + 38, target_ip, 4);
    return sceNet_lib_C431A214(D_S32(ctx, 20), frame, 42);
}

static int dhcp_arp_probe(u8 *ctx, const void *ip)
{
    u32 zero = 0;
    u32 target;
    sceKernelMemcpy(&target, ip, 4);
    if (dhcp_arp_send_req(ctx, &zero, &target) < 0)
        return 0;
    int n = sceNet_lib_B6FC0A5B(D_S32(ctx, 20), ctx + 1528, D_U32(ctx, 3028));
    return (n > 0) ? -1 : 0;
}

static int dhcp_arp_resolve_gw(u8 *ctx, const void *gw_ip)
{
    u32 target;
    sceKernelMemcpy(&target, gw_ip, 4);
    if (dhcp_arp_send_req(ctx, (void *)(D_U32(ctx, 3972) + 13), &target) < 0)
        return -1;
    int tries = 4;
    while (tries > 0) {
        int n = sceNet_lib_B6FC0A5B(D_S32(ctx, 20), ctx + 1528, D_U32(ctx, 3028));
        if (n < 0)
            return -1;
        if (sceKernelPollSema(D_S32(ctx, 4004), 1) == 0)
            return 0;
        if (n > 0) {
            u16 hlen = D_U16(ctx, 1544);
            sceKernelMemcpy(ctx + 5064, ctx + 1528 + hlen + 22, 6);
            return 1;
        }
        tries--;
    }
    return -1;
}

static void dhcp_send_gratuitous_arp(u8 *ctx, const void *ip)
{
    u8 frame[42];
    sceKernelMemset(frame, 0, sizeof(frame));
    sceKernelMemset(frame, 0xFF, 6);
    sceKernelMemcpy(frame + 6, ctx + 3686, 6);
    *(u16 *)(frame + 12) = bswap16(0x0806);
    *(u16 *)(frame + 14) = bswap16(1);
    *(u16 *)(frame + 16) = bswap16(0x0800);
    frame[18] = 6;
    frame[19] = 4;
    *(u16 *)(frame + 20) = bswap16(2);
    sceKernelMemcpy(frame + 22, ctx + 3686, 6);
    sceKernelMemcpy(frame + 28, ip, 4);
    sceKernelMemcpy(frame + 32, ctx + 3686, 6);
    sceKernelMemcpy(frame + 38, ip, 4);
    sceNet_lib_C431A214(D_S32(ctx, 20), frame, 42);
}

static int dhcp_retransmit(u8 *ctx)
{
    u8 *udp = (u8 *)D_U32(ctx, 3664);
    u8 *bootp = (u8 *)D_U32(ctx, 3668);
    u16 elapsed = (u16)(sceNet_lib_75D9985C() - D_U32(ctx, 3988));
    *(u16 *)(bootp + 8) = bswap16(elapsed);
    udp[6] = 0; udp[7] = 0;
    *(u16 *)(udp + 6) = udp_checksum((const u16 *)(ctx + 3672), (const u16 *)udp, bswap16(D_U16(ctx, 3682)));
    sceNet_lib_C431A214(D_S32(ctx, 16), ctx + 3034, D_U32(ctx, 3624));
    return 0;
}

static int dhcp_send_mcast_probe(u8 *ctx)
{
    u8 frame[14];
    u32 addr = sceNetInetInetAddr("224.0.0.203");
    sceKernelMemset(frame, 0, sizeof(frame));
    frame[0] = 1;
    frame[1] = 0;
    frame[2] = 94;
    frame[3] = (u8)((addr >> 8) & 0x7F);
    *(u16 *)(frame + 4) = (u16)(addr >> 16);
    sceKernelMemcpy(frame + 6, (void *)(D_U32(ctx, 3656) + 6), 6);
    *(u16 *)(frame + 12) = bswap16(0x88C8);
    int n = sceNet_lib_C431A214(D_S32(ctx, 16), frame, 14);
    return (n < 14) ? DHCP_ERR_BPF : 0;
}

static int dhcp_apply_if_ip(u8 *ctx, const u8 *lease)
{
    u32 cur_ip = 0, cur_mask = 0, cur_brd = 0;
    int ret = sceNetInet_lib_5155EC8A((const char *)ctx, &cur_ip, &cur_brd, &cur_mask);
    if (ret < 0)
        return ret;
    u32 ip = 0, mask = 0, brd = 0;
    sceKernelMemcpy(&ip, lease + 13, 4);
    mask = D_U32(lease, 1004);
    if (mask == 0)
        mask = default_netmask(&ip);
    brd = D_U32(lease, 1008);
    if (brd == 0)
        brd = ip | ~mask;
    if (cur_ip == ip && cur_mask == mask && cur_brd == brd)
        return 0;
    ret = sceNetInet_lib_4F68DB0E((const char *)ctx, &ip, &brd, &mask);
    sceNetInet_lib_A94A75E7(&ip);
    return ret;
}

static int dhcp_apply_dns_gw(u8 *ctx)
{
    u32 dns1 = 0, dns2 = 0, gw = 0;
    u8 *lease = (u8 *)D_U32(ctx, 3972);
    if (D_U32(ctx, 5088) == 0) {
        dns1 = D_U32(lease, 1016);
        if (D_U32(lease, 1020) != 0) {
            if (dns1 != 0) dns2 = D_U32(lease, 1020);
            else dns1 = D_U32(lease, 1020);
        }
    } else {
        sceNetInetInetAton((const char *)(ctx + 5092), &dns1);
        if (dns1 != 0) {
            sceNetInetInetAton((const char *)(ctx + 5108), &dns2);
            if (dns2 == 0 && D_U32(lease, 1016) != 0)
                dns2 = D_U32(lease, 1016);
        }
    }
    gw = D_U32(lease, 1012);
    if (D_U32(ctx, 4024) == dns1 && D_U32(ctx, 4028) == dns2 && D_U32(ctx, 4032) == gw)
        return 0;
    if (gw != 0) {
        int r = sceNetInet_lib_7776A492(&gw);
        if (r < 0)
            return r;
    }
    int r = sceNetInet_lib_AAF4895A(&dns1, &dns2);
    if (r >= 0) {
        D_U32(ctx, 4024) = dns1;
        D_U32(ctx, 4028) = dns2;
        D_U32(ctx, 4032) = gw;
    }
    return r;
}

/* DHCP state functions (0..6) */
static int dhcp_st_init(u8 *ctx, int prev)
{
    (void)prev;
    int ret = dhcp_clear_if(ctx);
    if (ret < 0)
        return ret;
    dhcp_build_discover(ctx);
    u8 *udp = (u8 *)D_U32(ctx, 3664);
    *(u16 *)(D_U32(ctx, 3668) + 8) = 0;
    udp[6] = 0; udp[7] = 0;
    *(u16 *)(udp + 6) = udp_checksum((const u16 *)(ctx + 3672), (const u16 *)udp, bswap16(D_U16(ctx, 3682)));
    if (D_U32(ctx, 5380) == 1) {
        ret = dhcp_send_mcast_probe(ctx);
        if (ret < 0)
            return ret;
    }
    int n = sceNet_lib_C431A214(D_S32(ctx, 16), ctx + 3034, D_U32(ctx, 3624));
    return (n >= D_S32(ctx, 3624)) ? 1 : DHCP_ERR_BPF;
}

static void dhcp_backoff(u8 *ctx)
{
    u32 cur = D_U32(ctx, 3996);
    u32 max_t = D_U32(ctx, 3984);
    u32 next = cur + ((u32)sceNet_lib_1858883D() % (2 * cur));
    if (next > max_t)
        next = (max_t >> 1) + ((u32)sceNet_lib_1858883D() % max_t);
    D_U32(ctx, 3996) = next;
}

static int dhcp_st_selecting(u8 *ctx, int prev)
{
    if (prev != 0) {
        dhcp_backoff(ctx);
    } else {
        if (D_U32(ctx, 3972) != 0) {
            dhcp_lease_clear((void *)D_U32(ctx, 3972));
            D_U32(ctx, 3972) = 0;
        }
        D_U32(ctx, 3996) = D_U32(ctx, 3980);
    }

    u32 now = sceNet_lib_75D9985C();
    u32 lim = D_U32(ctx, 3976);
    u32 wait;
    if (lim != 0 && now + D_U32(ctx, 3996) >= D_U32(ctx, 3988) + lim) {
        if (now >= D_U32(ctx, 3988) + lim) {
            dhcp_abort_exit(ctx, 1, DHCP_ERR_TIMEOUT);
            return DHCP_ERR_TIMEOUT;
        }
        wait = D_U32(ctx, 3988) + lim - now;
    } else {
        wait = D_U32(ctx, 3996);
    }

    while (wait > 0) {
        int n = sceNet_lib_B6FC0A5B(D_S32(ctx, 16), ctx + 24, D_U32(ctx, 1524));
        if (n < 0)
            return DHCP_ERR_BPF;
        if (sceKernelPollSema(D_S32(ctx, 4004), 1) == 0)
            dhcp_abort_exit(ctx, 1, 0);
        u8 *p = ctx + 24;
        while (n > 0) {
            u16 hlen = *(u16 *)(p + 16);
            u32 caplen = *(u32 *)(p + 8);
            u8 *eth = p + hlen;
            u8 *ip = eth + 14;
            u8 *udp = ip + 4 * (ip[0] & 0xF);
            u8 *bootp = udp + 8;
            D_U32(ctx, 3628) = (u32)eth;
            D_U32(ctx, 3632) = (u32)ip;
            D_U32(ctx, 3636) = (u32)udp;
            D_U32(ctx, 3640) = (u32)bootp;
            u16 udp_len = (u16)(((u16)udp[4] << 8) | udp[5]);
            if (dhcp_verify_offer(ctx) == 0) {
                sceKernelMemset(ctx + 4036, 0, 1028);
                if (dhcp_validate_opts(bootp, udp_len - 8) == 0) {
                    int r = dhcp_parse_lease(bootp, udp_len - 8, ctx + 4036);
                    if (r < 0)
                        return r;
                    if (D_U32(ctx, 5036) != 0 && D_U32(ctx, 5024) != 0) {
                        D_U32(ctx, 3972) = (u32)(ctx + 4036);
                        return 2;
                    }
                    dhcp_lease_clear(ctx + 4036);
                }
            }
            u32 step = (hlen + caplen + 3) & ~3u;
            p += step;
            n -= (int)step;
        }
        wait--;
    }

    now = sceNet_lib_75D9985C();
    if (lim != 0 && now >= D_U32(ctx, 3988) + lim) {
        dhcp_abort_exit(ctx, 1, DHCP_ERR_TIMEOUT);
        return DHCP_ERR_TIMEOUT;
    }
    if (D_U32(ctx, 5380) == 1) {
        int r = dhcp_send_mcast_probe(ctx);
        if (r < 0)
            return r;
    }
    dhcp_retransmit(ctx);
    return 1;
}

static int dhcp_st_send_req(u8 *ctx, int prev)
{
    (void)prev;
    int ret = dhcp_build_request(ctx, 3);
    if (ret < 0)
        return ret;
    int n = sceNet_lib_C431A214(D_S32(ctx, 16), ctx + 3034, D_U32(ctx, 3624));
    return (n >= D_S32(ctx, 3624)) ? 3 : DHCP_ERR_BPF;
}

static int dhcp_st_requesting(u8 *ctx, int prev)
{
    if (prev == 2)
        D_U32(ctx, 3996) = D_U32(ctx, 3980);
    else
        dhcp_backoff(ctx);

    u32 now = sceNet_lib_75D9985C();
    u32 lim = D_U32(ctx, 3976);
    u32 wait;
    if (lim != 0 && now + D_U32(ctx, 3996) >= D_U32(ctx, 3988) + lim) {
        if (now >= D_U32(ctx, 3988) + lim) {
            dhcp_abort_exit(ctx, 3, DHCP_ERR_TIMEOUT);
            return DHCP_ERR_TIMEOUT;
        }
        wait = D_U32(ctx, 3988) + lim - now;
    } else {
        wait = D_U32(ctx, 3996);
    }

    while (wait > 0) {
        int n = sceNet_lib_B6FC0A5B(D_S32(ctx, 16), ctx + 24, D_U32(ctx, 1524));
        if (n < 0)
            return DHCP_ERR_BPF;
        if (sceKernelPollSema(D_S32(ctx, 4004), 1) == 0)
            dhcp_abort_exit(ctx, 3, 0);
        u8 *p = ctx + 24;
        while (n > 0) {
            u16 hlen = *(u16 *)(p + 16);
            u32 caplen = *(u32 *)(p + 8);
            u8 *eth = p + hlen;
            u8 *ip = eth + 14;
            u8 *udp = ip + 4 * (ip[0] & 0xF);
            u8 *bootp = udp + 8;
            D_U32(ctx, 3628) = (u32)eth;
            D_U32(ctx, 3632) = (u32)ip;
            D_U32(ctx, 3636) = (u32)udp;
            D_U32(ctx, 3640) = (u32)bootp;
            u16 udp_len = (u16)(((u16)udp[4] << 8) | udp[5]);
            if (dhcp_verify_rx_common(ctx) == 0) {
                const u8 *mtype = dhcp_find_opt(bootp, udp_len - 8, 53);
                if (mtype != NULL) {
                    if (mtype[2] == 6) {
                        u8 msgbuf[256];
                        sceKernelMemset(msgbuf, 0, sizeof(msgbuf));
                        const u8 *m = dhcp_find_opt(bootp, udp_len - 8, 56);
                        if (m != NULL)
                            dhcp_copy_str(m + 2, msgbuf, m[1]);
                        dhcp_lease_clear((void *)D_U32(ctx, 3972));
                        D_U32(ctx, 3972) = 0;
                        return 0;
                    }
                    if (mtype[2] == 5 && dhcp_validate_opts(bootp, udp_len - 8) == 0) {
                        int r = dhcp_parse_lease(bootp, udp_len - 8, (u8 *)D_U32(ctx, 3972));
                        if (r < 0)
                            return r;
                        int conflict = dhcp_arp_probe(ctx, (void *)(D_U32(ctx, 3972) + 13));
                        if (conflict != 0) {
                            r = dhcp_build_decline(ctx, conflict);
                            if (r < 0)
                                return r;
                            if (sceNet_lib_C431A214(D_S32(ctx, 16), ctx + 3034, D_U32(ctx, 3624)) < D_S32(ctx, 3624))
                                return DHCP_ERR_BPF;
                            dhcp_lease_clear((void *)D_U32(ctx, 3972));
                            D_U32(ctx, 3972) = 0;
                            return 0;
                        }
                        D_U32(ctx, 3992) = sceNet_lib_75D9985C();
                        dhcp_send_gratuitous_arp(ctx, (void *)(D_U32(ctx, 3972) + 13));
                        return 4;
                    }
                }
            }
            u32 step = (hlen + caplen + 3) & ~3u;
            p += step;
            n -= (int)step;
        }
        wait--;
    }

    now = sceNet_lib_75D9985C();
    if (lim != 0 && now >= D_U32(ctx, 3988) + lim) {
        dhcp_abort_exit(ctx, 3, DHCP_ERR_TIMEOUT);
        return DHCP_ERR_TIMEOUT;
    }
    dhcp_retransmit(ctx);
    return 3;
}

static int dhcp_st_bound(u8 *ctx, int prev)
{
    (void)prev;
    u8 *lease = (u8 *)D_U32(ctx, 3972);
    int ret = dhcp_apply_if_ip(ctx, lease);
    if (ret < 0)
        return ret;
    ret = dhcp_apply_dns_gw(ctx);
    if (ret < 0)
        return ret;

    u32 yiaddr;
    sceKernelMemcpy(&yiaddr, lease + 13, 4);
    u32 mask = D_U32(lease, 1004);
    if ((D_U32(lease, 1000) & mask) == (yiaddr & mask)) {
        sceKernelMemcpy(ctx + 5070, lease + 1000, 4);
        sceKernelMemcpy(ctx + 5064, (void *)(D_U32(ctx, 3628) + 6), 6);
        sceNetInet_lib_59561561(ctx + 5064, ctx + 5070);
    } else {
        sceKernelMemcpy(ctx + 5070, lease + 1012, 4);
        int ar = dhcp_arp_resolve_gw(ctx, lease + 1012);
        if (ar == 0)
            dhcp_abort_exit(ctx, 4, 0);
        if (ar >= 0)
            sceNetInet_lib_59561561(ctx + 5064, ctx + 5070);
    }

    D_U32(ctx, 4020) = 1;
    void (*cb)(int, int) = (void (*)(int, int))D_U32(ctx, 5084);
    if (cb != NULL)
        cb(0, 0);

    int t1 = D_S32(lease, 992);
    while (t1-- > 0) {
        sceKernelDelayThread(1000000);
        if (sceKernelPollSema(D_S32(ctx, 4004), 1) == 0)
            dhcp_abort_exit(ctx, 4, 0);
    }

    u32 now = sceNet_lib_75D9985C();
    if (now < D_U32(ctx, 3992) + D_U32(lease, 996)) {
        ret = dhcp_build_request(ctx, 5);
        if (ret < 0)
            return ret;
        if (D_U32(lease, 1000) == 0)
            return DHCP_ERR_TIMEOUT;
        int s = dhcp_open_unicast_sock(D_S32(ctx, 5388));
        D_S32(ctx, 5388) = s;
        if (s < 0)
            return s;
        ret = dhcp_send_unicast(s, (const u32 *)(lease + 1000), (void *)D_U32(ctx, 3668));
        if (ret < 0) {
            sceNetInetClose(D_S32(ctx, 5388));
            D_S32(ctx, 5388) = -1;
            return ret;
        }
        return 5;
    }
    if (now >= D_U32(ctx, 3992) + D_U32(lease, 988))
        return DHCP_ERR_EXPIRED;
    ret = dhcp_build_request(ctx, 6);
    if (ret < 0)
        return ret;
    if (sceNet_lib_C431A214(D_S32(ctx, 16), ctx + 3034, D_U32(ctx, 3624)) < D_S32(ctx, 3624))
        return DHCP_ERR_BPF;
    return 6;
}

static int dhcp_st_renewing(u8 *ctx, int prev)
{
    if (prev == 4)
        D_U32(ctx, 3996) = D_U32(ctx, 3980);
    else
        dhcp_backoff(ctx);

    u8 *lease = (u8 *)D_U32(ctx, 3972);
    u32 now = sceNet_lib_75D9985C();
    u32 t2_end = D_U32(ctx, 3992) + D_U32(lease, 996);
    if (now >= t2_end)
        return 6;

    u32 wait = t2_end - now;
    if (D_U32(ctx, 3996) < wait)
        wait = D_U32(ctx, 3996);

    while (wait > 0) {
        int n = sceNet_lib_B6FC0A5B(D_S32(ctx, 16), ctx + 24, D_U32(ctx, 1524));
        if (n < 0)
            return DHCP_ERR_BPF;
        if (sceKernelPollSema(D_S32(ctx, 4004), 1) == 0)
            dhcp_abort_exit(ctx, 5, 0);
        u8 *p = ctx + 24;
        while (n > 0) {
            u16 hlen = *(u16 *)(p + 16);
            u32 caplen = *(u32 *)(p + 8);
            u8 *eth = p + hlen;
            u8 *ip = eth + 14;
            u8 *udp = ip + 4 * (ip[0] & 0xF);
            u8 *bootp = udp + 8;
            D_U32(ctx, 3628) = (u32)eth;
            D_U32(ctx, 3632) = (u32)ip;
            D_U32(ctx, 3636) = (u32)udp;
            D_U32(ctx, 3640) = (u32)bootp;
            u16 udp_len = (u16)(((u16)udp[4] << 8) | udp[5]);
            if (dhcp_verify_rx_common(ctx) == 0) {
                const u8 *mtype = dhcp_find_opt(bootp, udp_len - 8, 53);
                if (mtype != NULL) {
                    if (mtype[2] == 6) {
                        u8 msgbuf[256];
                        sceKernelMemset(msgbuf, 0, sizeof(msgbuf));
                        const u8 *m = dhcp_find_opt(bootp, udp_len - 8, 56);
                        if (m != NULL)
                            dhcp_copy_str(m + 2, msgbuf, m[1]);
                        dhcp_lease_clear(lease);
                        D_U32(ctx, 3972) = 0;
                        return DHCP_ERR_NAK;
                    }
                    if (mtype[2] == 5 && dhcp_validate_opts(bootp, udp_len - 8) == 0) {
                        int r = dhcp_parse_lease(bootp, udp_len - 8, lease);
                        if (r < 0)
                            return r;
                        D_U32(ctx, 3992) = sceNet_lib_75D9985C();
                        dhcp_send_gratuitous_arp(ctx, lease + 13);
                        void (*cb)(int, int) = (void (*)(int, int))D_U32(ctx, 5084);
                        if (cb != NULL)
                            cb(2, 0);
                        if (D_S32(ctx, 5388) >= 0) {
                            sceNetInetClose(D_S32(ctx, 5388));
                            D_S32(ctx, 5388) = -1;
                        }
                        return 4;
                    }
                }
            }
            u32 step = (hlen + caplen + 3) & ~3u;
            p += step;
            n -= (int)step;
        }
        wait--;
    }

    now = sceNet_lib_75D9985C();
    if (now >= D_U32(ctx, 3992) + D_U32(lease, 996)) {
        int r = dhcp_build_request(ctx, 6);
        if (r < 0)
            return r;
        if (sceNet_lib_C431A214(D_S32(ctx, 16), ctx + 3034, D_U32(ctx, 3624)) < D_S32(ctx, 3624))
            return DHCP_ERR_BPF;
        return 6;
    }
    if (D_U32(lease, 1000) == 0)
        return DHCP_ERR_TIMEOUT;
    int s = dhcp_open_unicast_sock(D_S32(ctx, 5388));
    D_S32(ctx, 5388) = s;
    if (s < 0)
        return s;
    int r = dhcp_send_unicast(s, (const u32 *)(lease + 1000), (void *)D_U32(ctx, 3668));
    if (r < 0) {
        sceNetInetClose(D_S32(ctx, 5388));
        D_S32(ctx, 5388) = -1;
        return r;
    }
    return 5;
}

static int dhcp_st_rebinding(u8 *ctx, int prev)
{
    void (*cb)(int, int) = (void (*)(int, int))D_U32(ctx, 5084);
    if (cb != NULL)
        cb(3, 0);
    if (prev != 6)
        D_U32(ctx, 3996) = D_U32(ctx, 3980);

    u8 *lease = (u8 *)D_U32(ctx, 3972);
    u32 now = sceNet_lib_75D9985C();
    u32 end = D_U32(ctx, 3992) + D_U32(lease, 988);
    if (now >= end)
        return DHCP_ERR_EXPIRED;

    dhcp_backoff(ctx);
    u32 wait = end - now;
    if (D_U32(ctx, 3996) < wait)
        wait = D_U32(ctx, 3996);

    while (wait > 0) {
        int n = sceNet_lib_B6FC0A5B(D_S32(ctx, 16), ctx + 24, D_U32(ctx, 1524));
        if (n < 0)
            return DHCP_ERR_BPF;
        if (sceKernelPollSema(D_S32(ctx, 4004), 1) == 0)
            dhcp_abort_exit(ctx, 6, 0);
        u8 *p = ctx + 24;
        while (n > 0) {
            u16 hlen = *(u16 *)(p + 16);
            u32 caplen = *(u32 *)(p + 8);
            u8 *eth = p + hlen;
            u8 *ip = eth + 14;
            u8 *udp = ip + 4 * (ip[0] & 0xF);
            u8 *bootp = udp + 8;
            D_U32(ctx, 3628) = (u32)eth;
            D_U32(ctx, 3632) = (u32)ip;
            D_U32(ctx, 3636) = (u32)udp;
            D_U32(ctx, 3640) = (u32)bootp;
            u16 udp_len = (u16)(((u16)udp[4] << 8) | udp[5]);
            if (dhcp_verify_rx_common(ctx) == 0) {
                const u8 *mtype = dhcp_find_opt(bootp, udp_len - 8, 53);
                if (mtype != NULL) {
                    if (mtype[2] == 6) {
                        u8 msgbuf[256];
                        sceKernelMemset(msgbuf, 0, sizeof(msgbuf));
                        const u8 *m = dhcp_find_opt(bootp, udp_len - 8, 56);
                        if (m != NULL)
                            dhcp_copy_str(m + 2, msgbuf, m[1]);
                        dhcp_lease_clear(lease);
                        D_U32(ctx, 3972) = 0;
                        return DHCP_ERR_NAK;
                    }
                    if (mtype[2] == 5 && dhcp_validate_opts(bootp, udp_len - 8) == 0) {
                        int r = dhcp_parse_lease(bootp, udp_len - 8, lease);
                        if (r < 0)
                            return r;
                        D_U32(ctx, 3992) = sceNet_lib_75D9985C();
                        dhcp_send_gratuitous_arp(ctx, lease + 13);
                        return 4;
                    }
                }
            }
            u32 step = (hlen + caplen + 3) & ~3u;
            p += step;
            n -= (int)step;
        }
        wait--;
    }

    if (sceNet_lib_75D9985C() >= end)
        return DHCP_ERR_EXPIRED;
    dhcp_retransmit(ctx);
    return 6;
}

typedef int (*DhcpStateFn)(u8 *, int);
static const DhcpStateFn g_dhcp_states[7] = {
    dhcp_st_init,
    dhcp_st_selecting,
    dhcp_st_send_req,
    dhcp_st_requesting,
    dhcp_st_bound,
    dhcp_st_renewing,
    dhcp_st_rebinding
};

static int dhcp_thread(SceSize args, void *argp)
{
    int prev = 0;
    (void)args;
    (void)argp;
    D_S32(g_dhcp_ctx, 4012) = 0;
    D_U32(g_dhcp_ctx, 3988) = sceNet_lib_75D9985C();

    for (;;) {
        int cur = D_S32(g_dhcp_ctx, 4012);
        int next = g_dhcp_states[cur](g_dhcp_ctx, prev);
        if (next < 0) {
            void (*cb)(int, int) = (void (*)(int, int))D_U32(g_dhcp_ctx, 5084);
            if (cb != NULL)
                cb(4, next);
            dhcp_clear_if(g_dhcp_ctx);
            sceKernelExitThread(0);
            return 0;
        }
        prev = cur;
        D_S32(g_dhcp_ctx, 4012) = next;
        if (sceKernelPollSema(D_S32(g_dhcp_ctx, 4004), 1) == 0)
            dhcp_abort_exit(g_dhcp_ctx, next, 0);
    }
}

int dhcp_start(const char *ifname, void (*cb)(int, int), int stack, int prio, int timeout, void *cfg)
{
    int bpf_dhcp = -1, bpf_arp = -1;
    SceUID sema = -1, thid = -1;
    int ret = DHCP_ERR_ALREADY_STARTED;

    if (g_dhcp_running > 0)
        goto fail;

    bpf_dhcp = sceNet_lib_3B617AA0();
    ret = DHCP_ERR_BPF;
    if (bpf_dhcp < 0)
        goto fail;
    bpf_arp = sceNet_lib_3B617AA0();
    if (bpf_arp < 0)
        goto fail;

    sceKernelMemset(g_dhcp_ctx, 0, sizeof(g_dhcp_ctx));
    sceNet_lib_B5CE388A((char *)g_dhcp_ctx, ifname, 16);
    D_S32(g_dhcp_ctx, 16) = bpf_dhcp;
    D_S32(g_dhcp_ctx, 20) = bpf_arp;
    D_S32(g_dhcp_ctx, 5388) = -1;
    D_U32(g_dhcp_ctx, 3976) = (u32)timeout;
    D_U32(g_dhcp_ctx, 3980) = 3;
    D_U32(g_dhcp_ctx, 3984) = 15;
    g_dhcp_ctx[3684] = 1;
    g_dhcp_ctx[3685] = 6;

    {
        u8 mac[6];
        ret = sceNet_lib_8D33C11D((const char *)g_dhcp_ctx, mac);
        if (ret < 0)
            goto fail;
        sceKernelMemcpy(g_dhcp_ctx + 3686, mac, 6);
    }
    g_dhcp_ctx[5076] = g_dhcp_ctx[3684];
    g_dhcp_ctx[5077] = g_dhcp_ctx[3685];
    D_U32(g_dhcp_ctx, 5080) = (u32)(g_dhcp_ctx + 3686);
    D_U32(g_dhcp_ctx, 3960) = (u32)(g_dhcp_ctx + 5076);
    g_dhcp_ctx[3704] = 5;
    g_dhcp_ctx[3705] = 1;
    g_dhcp_ctx[3706] = 3;
    g_dhcp_ctx[3707] = 28;
    g_dhcp_ctx[3708] = 6;
    g_dhcp_ctx[3709] = 15;
    if (cfg != NULL)
        D_U32(g_dhcp_ctx, 3696) = *(u32 *)((u8 *)cfg + 296);
    D_U32(g_dhcp_ctx, 5084) = (u32)cb;

    if (cfg != NULL) {
        if ((u32)sceNet_lib_D8722983((const char *)cfg + 4) >= 256 ||
            (u32)sceNet_lib_D8722983((const char *)cfg + 20) >= 256 ||
            (u32)sceNet_lib_D8722983((const char *)cfg + 36) >= 256) {
            ret = DHCP_ERR_PARAM;
            goto fail;
        }
        sceKernelMemcpy(g_dhcp_ctx + 5088, cfg, 300);
    }

    {
        u32 one = 1;
        u32 tv[2] = { 1, 0 };
        char ifr[32];
        ret = DHCP_ERR_BPF;
        if (sceNet_lib_E1F4696F(bpf_dhcp, 0x80044270u, &one) < 0 ||
            sceNet_lib_E1F4696F(bpf_arp, 0x80044270u, &one) < 0)
            goto fail;
        D_U32(g_dhcp_ctx, 1524) = 1500;
        if (sceNet_lib_E1F4696F(bpf_dhcp, 0xC0044266u, g_dhcp_ctx + 1524) < 0)
            goto fail;
        sceKernelMemset(g_dhcp_ctx + 24, 0, D_U32(g_dhcp_ctx, 1524));
        D_U32(g_dhcp_ctx, 3028) = 1500;
        if (sceNet_lib_E1F4696F(bpf_arp, 0xC0044266u, g_dhcp_ctx + 3028) < 0)
            goto fail;
        sceKernelMemset(g_dhcp_ctx + 1528, 0, D_U32(g_dhcp_ctx, 3028));
        sceKernelMemset(ifr, 0, sizeof(ifr));
        sceNet_lib_B5CE388A(ifr, (const char *)g_dhcp_ctx, 16);
        if (sceNet_lib_E1F4696F(bpf_dhcp, 0x8020426Cu, ifr) < 0 ||
            sceNet_lib_E1F4696F(bpf_arp, 0x8020426Cu, ifr) < 0 ||
            sceNet_lib_E1F4696F(bpf_dhcp, 0x80084267u, &g_dhcp_bpf_prog) < 0 ||
            sceNet_lib_E1F4696F(bpf_dhcp, 0x8008426Du, tv) < 0 ||
            sceNet_lib_E1F4696F(bpf_arp, 0x8008426Du, tv) < 0)
            goto fail;
    }

    D_U32(g_dhcp_ctx, 3624) = 590;
    sceKernelMemset(g_dhcp_ctx + 3032, 0, 592);
    D_U32(g_dhcp_ctx, 3656) = (u32)(g_dhcp_ctx + 3034);
    D_U32(g_dhcp_ctx, 3660) = (u32)(g_dhcp_ctx + 3048);
    D_U32(g_dhcp_ctx, 3664) = (u32)(g_dhcp_ctx + 3068);
    D_U32(g_dhcp_ctx, 3668) = (u32)(g_dhcp_ctx + 3076);

    sema = sceKernelCreateSema("SceNetDhcpClient", 0, 0, 1, NULL);
    ret = sema;
    if (sema < 0)
        goto fail;
    D_S32(g_dhcp_ctx, 4004) = sema;

    thid = sceKernelCreateThread("SceNetDhcpClient", dhcp_thread, prio, stack, 0, NULL);
    ret = thid;
    if (thid < 0)
        goto fail;
    ret = sceKernelStartThread(thid, 0, NULL);
    if (ret < 0)
        goto fail;
    D_S32(g_dhcp_ctx, 4008) = thid;
    g_dhcp_running = 1;
    return 0;

fail:
    dhcp_clear_if((u8 *)ifname);
    if (thid >= 0) {
        sceKernelTerminateThread(thid);
        sceKernelDeleteThread(thid);
    }
    if (sema >= 0)
        sceKernelDeleteSema(sema);
    if (bpf_dhcp >= 0)
        sceNet_lib_DB88F458(bpf_dhcp);
    if (bpf_arp >= 0)
        sceNet_lib_DB88F458(bpf_arp);
    return ret;
}

int dhcp_stop(void)
{
    if (g_dhcp_running < 0)
        return 0;
    sceKernelSignalSema(D_S32(g_dhcp_ctx, 4004), 1);
    if (D_S32(g_dhcp_ctx, 4008) >= 0) {
        sceKernelWaitThreadEnd(D_S32(g_dhcp_ctx, 4008), NULL);
        sceKernelDeleteThread(D_S32(g_dhcp_ctx, 4008));
        sceNetFreeThreadinfo(D_S32(g_dhcp_ctx, 4008));
    }
    if (D_S32(g_dhcp_ctx, 4004) >= 0)
        sceKernelDeleteSema(D_S32(g_dhcp_ctx, 4004));
    if (D_S32(g_dhcp_ctx, 16) >= 0)
        sceNet_lib_DB88F458(D_S32(g_dhcp_ctx, 16));
    if (D_S32(g_dhcp_ctx, 20) >= 0)
        sceNet_lib_DB88F458(D_S32(g_dhcp_ctx, 20));
    if (D_S32(g_dhcp_ctx, 5388) >= 0) {
        sceNetInetClose(D_S32(g_dhcp_ctx, 5388));
        D_S32(g_dhcp_ctx, 5388) = -1;
    }
    sceKernelMemset(g_dhcp_ctx, 0, sizeof(g_dhcp_ctx));
    g_dhcp_running = -1;
    return 0;
}
