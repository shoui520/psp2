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

/* Network packet header structures */

typedef struct __attribute__((packed)) EthHdr {
    u8 dst[6];
    u8 src[6];
    u16 ethertype;
} EthHdr;

typedef struct __attribute__((packed)) PackedU32 {
    u32 val;
} PackedU32;

typedef struct __attribute__((packed)) Ipv4Hdr {
    u8 ihl : 4;
    u8 ver : 4;
    u8 tos;
    u16 tot_len;
    u16 id;
    u16 frag_off;
    u8 ttl;
    u8 proto;
    u16 check;
    u8 saddr[4];
    u8 daddr[4];
} Ipv4Hdr;

typedef struct __attribute__((packed)) UdpHdr {
    u16 sport;
    u16 dport;
    u16 len;
    u16 check;
} UdpHdr;

typedef struct BootpHdr {
    u8 op;
    u8 htype;
    u8 hlen;
    u8 hops;
    u32 xid;
    u16 secs;
    u16 flags;
    u32 ciaddr;
    u32 yiaddr;
    u32 siaddr;
    u32 giaddr;
    u8 chaddr[16];
    u8 sname[64];
    u8 file[128];
    u8 options[312];
} BootpHdr;

typedef struct __attribute__((packed)) ArpEthFrame {
    u8 eth_dst[6];
    u8 eth_src[6];
    u16 eth_type;
    u16 htype;
    u16 ptype;
    u8 hlen;
    u8 plen;
    u16 oper;
    u8 sha[6];
    u8 spa[4];
    u8 tha[6];
    u8 tpa[4];
} ArpEthFrame;

typedef struct UdpPseudoHdr {
    u32 src_ip;
    u32 dst_ip;
    u8 zero;
    u8 proto;
    u16 len;
} UdpPseudoHdr;

typedef struct SockAddrIn {
    u8 sin_len;
    u8 sin_family;
    u16 sin_port;
    u32 sin_addr;
    u8 sin_zero[8];
} SockAddrIn;

typedef struct DhcpLease {
    u8 opt_flags[9];          /* 0 */
    u8 ciaddr[4];             /* 9 */
    u8 yiaddr[4];             /* 13 */
    u8 siaddr[4];             /* 17 */
    u8 giaddr[4];             /* 21 */
    char sname[64];           /* 25 */
    char boot_file[128];      /* 89 */
    char hostname[256];       /* 217 */
    char domain_name[256];    /* 473 */
    char message[256];        /* 729 */
    u8 pad_msg[3];            /* 985 */
    u32 lease_time;           /* 988 */
    u32 renew_time;           /* 992 */
    u32 rebind_time;          /* 996 */
    u32 server_id;            /* 1000 */
    u32 subnet_mask;          /* 1004 */
    u32 broadcast_addr;       /* 1008 */
    u32 router_ip;            /* 1012 */
    u32 dns_a;                /* 1016 */
    u32 dns_b;                /* 1020 */
    u8 msg_type;              /* 1024 */
    u8 pad_end[3];            /* 1025 */
} DhcpLease; /* 1028 bytes */

typedef struct DhcpPacketPtrs {
    EthHdr *eth;              /* +0 */
    Ipv4Hdr *ip;              /* +4 */
    UdpHdr *udp;              /* +8 */
    BootpHdr *bootp;          /* +12 */
    UdpPseudoHdr pseudo;      /* +16 (12 bytes) */
} DhcpPacketPtrs; /* 28 bytes */

typedef struct DhcpClientId {
    u8 htype;                 /* +0 */
    u8 hlen;                  /* +1 */
    u8 pad[2];                /* +2 */
    u8 *addr;                 /* +4 */
} DhcpClientId; /* 8 bytes */

typedef struct DhcpConfig {
    u32 use_static_dns;       /* +0 (5088) */
    char dns_a[16];           /* +4 (5092) */
    char dns_b[16];           /* +20 (5108) */
    char hostname[256];       /* +36 (5124) */
    u32 mcast_probe_flag;     /* +292 (5380) */
    u32 requested_ip;         /* +296 (5384) */
} DhcpConfig; /* 300 bytes */

typedef struct DhcpCtx {
    char ifname[16];                /* 0 */
    int bpf_dhcp;                   /* 16 */
    int bpf_arp;                    /* 20 */
    u8 dhcp_rx_buf[1500];           /* 24 */
    u32 dhcp_rx_buf_size;           /* 1524 */
    u8 arp_rx_buf[1500];            /* 1528 */
    u32 arp_rx_buf_size;            /* 3028 */
    u8 tx_frame_raw[592];           /* 3032 (frame starts at +2 = 3034) */
    u32 tx_frame_len;               /* 3624 */
    DhcpPacketPtrs rx;              /* 3628 */
    DhcpPacketPtrs tx;              /* 3656 */
    u8 hw_type;                     /* 3684 */
    u8 hw_len;                      /* 3685 */
    u8 hw_addr[6];                  /* 3686 */
    u8 pad_hw[4];                   /* 3692 */
    u32 requested_ip;               /* 3696 */
    u32 pad_req_ip;                 /* 3700 */
    u8 opt_req_len;                 /* 3704 */
    u8 opt_req_list[255];           /* 3705 */
    DhcpClientId *client_id;        /* 3960 */
    u32 pad_cid[2];                 /* 3964 */
    DhcpLease *cur_lease;           /* 3972 */
    u32 timeout_sec;                /* 3976 */
    u32 retry_init;                 /* 3980 */
    u32 retry_max;                  /* 3984 */
    u32 start_time;                 /* 3988 */
    u32 lease_start_time;           /* 3992 */
    u32 cur_backoff;                /* 3996 */
    u32 pad_backoff;                /* 4000 */
    SceUID sema;                    /* 4004 */
    SceUID thid;                    /* 4008 */
    int state;                      /* 4012 */
    u32 pad_state;                  /* 4016 */
    u32 bound_flag;                 /* 4020 */
    u32 applied_dns_a;              /* 4024 */
    u32 applied_dns_b;              /* 4028 */
    u32 applied_gw;                 /* 4032 */
    DhcpLease lease_storage;        /* 4036 (1028 bytes) */
    u8 server_mac[6];               /* 5064 */
    u8 server_arp_ip[4];            /* 5070 */
    u8 pad_arp[2];                  /* 5074 */
    DhcpClientId client_id_storage; /* 5076 (8 bytes) */
    void (*cb)(int, int);           /* 5084 */
    DhcpConfig cfg;                 /* 5088 (300 bytes) */
    int unicast_sock;               /* 5388 */
} DhcpCtx; /* 5392 bytes */

static int g_dhcp_running = -1;
static DhcpCtx g_dhcp_ctx;

static u16 ip_checksum(const u16 *buf, int words)
{
    u32 sum = 0;
    u16 res;
    while (words-- > 0)
        sum += *buf++;
    sum = (sum & 0xFFFF) + (sum >> 16);
    sum += (sum >> 16);
    res = (u16)(~sum);
    return (res == 0) ? 0xFFFF : res;
}

static u16 udp_checksum(const u16 *pseudo, const u16 *udp, int len)
{
    u32 sum = 0;
    u16 res;
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
    res = (u16)(~sum);
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

static int dhcp_clear_if(const char *ifname)
{
    int ret = sceNetInet_lib_E6F67D14();
    if (ret >= 0) {
        ret = sceNetInet_lib_6A046357(ifname);
        if (ret > 0)
            ret = 0;
    }
    return ret;
}

static void dhcp_lease_clear(DhcpLease *lease)
{
    if (lease != NULL)
        sceKernelMemset(lease, 0, sizeof(DhcpLease));
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
        u8 len;
        if (code == 0) {
            p++;
            continue;
        }
        if (code == 255)
            return 0;
        if (end - p < 2)
            return -2;
        len = p[1];
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

static const u8 *dhcp_find_opt_sec(const u8 *pkt, int pkt_len, char target, int sec, u32 *overload)
{
    const u8 *p;
    int len;
    int off = 0;
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

    while (off < len) {
        u8 code = p[off];
        if (code == (u8)target)
            return &p[off];
        if (code == 255)
            return NULL;
        if (code == 0) {
            off++;
            continue;
        }
        if (code == 52 && sec == 0 && overload != NULL) {
            u8 v;
            if (off + 2 >= len)
                return NULL;
            v = p[off + 2];
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

static const u8 *dhcp_find_opt(const u8 *pkt, int pkt_len, char target)
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

static int __attribute__((noinline))
dhcp_parse_opt_string(const u8 *opt, DhcpLease *lease)
{
    u8 code = opt[0];
    int n = (opt[1] < 255) ? opt[1] : 255;
    if (code == 15) {
        sceKernelMemset(lease->domain_name, 0, 256);
        sceKernelMemcpy(lease->domain_name, opt + 2, (u32)n);
        lease->domain_name[n] = 0;
    } else if (code == 56) {
        sceKernelMemset(lease->message, 0, 256);
        sceKernelMemcpy(lease->message, opt + 2, (u32)n);
        lease->message[n] = 0;
    } else if (code == 12) {
        sceKernelMemset(lease->hostname, 0, 256);
        sceKernelMemcpy(lease->hostname, opt + 2, (u32)n);
        lease->hostname[n] = 0;
    }
    return 0;
}

static int __attribute__((noinline))
dhcp_parse_opt_ip(const u8 *opt, DhcpLease *lease)
{
    u8 code = opt[0];
    if (code == 28)
        sceKernelMemcpy(&lease->broadcast_addr, opt + 2, 4);
    else if (code == 1)
        sceKernelMemcpy(&lease->subnet_mask, opt + 2, 4);
    else if (code == 54)
        sceKernelMemcpy(&lease->server_id, opt + 2, 4);
    return 0;
}

static int __attribute__((noinline))
dhcp_parse_opt_ip_list(const u8 *opt, DhcpLease *lease)
{
    u8 code = opt[0];
    int count = opt[1] >> 2;
    if (code == 3) {
        sceKernelMemset(&lease->router_ip, 0, 4);
        sceKernelMemcpy(&lease->router_ip, opt + 2, 4);
    } else if (code == 6) {
        sceKernelMemset(&lease->dns_a, 0, 4);
        sceKernelMemset(&lease->dns_b, 0, 4);
        sceKernelMemcpy(&lease->dns_a, opt + 2, 4);
        if (count >= 2)
            sceKernelMemcpy(&lease->dns_b, opt + 6, 4);
    }
    return 0;
}

static int __attribute__((noinline))
dhcp_parse_opt_u32(const u8 *opt, DhcpLease *lease)
{
    u32 val = 0;
    u8 code = opt[0];
    if (opt[1] == 4) {
        val = ((u32)opt[2] << 24) | ((u32)opt[3] << 16) |
              ((u32)opt[4] << 8) | (u32)opt[5];
    }
    if (code == 51)
        lease->lease_time = val;
    else if (code == 58)
        lease->renew_time = val;
    else if (code == 59)
        lease->rebind_time = val;
    return 0;
}

static int dhcp_parse_one_opt(const u8 *opt, DhcpLease *lease)
{
    switch (opt[0]) {
    case 1:
    case 28:
    case 54:
        return dhcp_parse_opt_ip(opt, lease);
    case 3:
    case 6:
        return dhcp_parse_opt_ip_list(opt, lease);
    case 12:
    case 15:
    case 56:
        return dhcp_parse_opt_string(opt, lease);
    case 51:
    case 58:
    case 59:
        return dhcp_parse_opt_u32(opt, lease);
    case 53:
        lease->msg_type = opt[2];
        return 0;
    default:
        return 0;
    }
}

static int dhcp_parse_section(const u8 *pkt, int pkt_len, DhcpLease *lease, int sec, u32 *overload)
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

static int dhcp_parse_lease(const u8 *pkt, int pkt_len, DhcpLease *lease)
{
    u32 overload = 0;
    u32 lease_t, renew_t, rebind_t, span;

    sceKernelMemset(lease, 0, 9);
    dhcp_parse_section(pkt, pkt_len, lease, 0, &overload);

    sceKernelMemset(lease->boot_file, 0, 128);
    if (overload & 1) {
        dhcp_parse_section(pkt, pkt_len, lease, 1, NULL);
    } else {
        int l = sceNet_lib_D8722983((const char *)(pkt + 108));
        sceNet_lib_B5CE388A(lease->boot_file, (const char *)(pkt + 108), l + 1);
    }

    sceKernelMemset(lease->sname, 0, 64);
    if (overload & 2) {
        dhcp_parse_section(pkt, pkt_len, lease, 2, NULL);
    } else {
        int l = sceNet_lib_D8722983((const char *)(pkt + 44));
        sceNet_lib_B5CE388A(lease->sname, (const char *)(pkt + 44), l + 1);
    }

    /* Copy ciaddr, yiaddr, siaddr, giaddr (16 bytes at pkt+12 -> lease->ciaddr) */
    sceKernelMemcpy(lease->ciaddr, pkt + 12, 16);

    lease_t = lease->lease_time;
    if ((lease->opt_flags[7] & 0x20) == 0) {
        lease->opt_flags[7] |= 0x20;
        lease->renew_time = lease_t >> 1;
    }
    renew_t = lease->renew_time;
    span = (renew_t + 3) >> 2;
    if (span == 0)
        span = 1;
    lease->renew_time = ((3 * renew_t + 9) >> 2) + ((u32)sceNet_lib_1858883D() % span);
    if ((lease->opt_flags[7] & 0x10) == 0) {
        lease->opt_flags[7] |= 0x10;
        lease->rebind_time = 7 * (lease_t >> 3);
    }
    rebind_t = lease->rebind_time;
    if (rebind_t < lease->renew_time)
        lease->renew_time = (3 * rebind_t) >> 2;
    return 0;
}

static inline __attribute__((always_inline))
int dhcp_verify_rx_inline(DhcpCtx *ctx)
{
    Ipv4Hdr *ip = ctx->rx.ip;
    UdpHdr *udp = ctx->rx.udp;
    BootpHdr *bootp = ctx->rx.bootp;
    u16 ip_sum = ip->check;
    u16 udp_sum = udp->check;
    UdpPseudoHdr pseudo;
    u16 ip_len;
    u16 udp_len;

    ip->check = 0;
    udp->check = 0;

    pseudo.src_ip = ((const PackedU32 *)ip->saddr)->val;
    pseudo.dst_ip = ((const PackedU32 *)ip->daddr)->val;
    pseudo.zero = 0;
    pseudo.proto = 17;
    pseudo.len = udp->len;

    ip_len = bswap16(ip->tot_len);
    udp_len = bswap16(udp->len);
    if (ip_len < 268 || udp_len < 248)
        return DHCP_ERR_PKT;
    if (ip_sum != ip_checksum((const u16 *)ip, 2 * ip->ihl))
        return DHCP_ERR_PKT;
    if (udp_sum != 0 && udp_sum != udp_checksum((const u16 *)&pseudo, (const u16 *)udp, udp_len))
        return DHCP_ERR_PKT;
    if (bootp->op != 2)
        return DHCP_ERR_PKT;
    if (bootp->xid != ctx->tx.bootp->xid)
        return DHCP_ERR_PKT;
    if (sceNet_lib_E0A81C7C(bootp->options, &g_dhcp_magic, 4) != 0)
        return DHCP_ERR_PKT;
    return 0;
}

static int dhcp_verify_offer(DhcpCtx *ctx)
{
    u16 udp_len;
    const u8 *opt;
    if (dhcp_verify_rx_inline(ctx) < 0)
        return DHCP_ERR_PKT;
    udp_len = bswap16(ctx->rx.udp->len);
    opt = dhcp_find_opt((const u8 *)ctx->rx.bootp, udp_len - 8, 53);
    if (opt == NULL || opt[2] != 2)
        return DHCP_ERR_PKT;
    return 0;
}

static int dhcp_verify_ack(DhcpCtx *ctx)
{
    return dhcp_verify_rx_inline(ctx);
}

static int dhcp_wrap_eth_ip_udp(DhcpCtx *ctx, DhcpPacketPtrs *tx, u32 src_ip, u32 dst_ip)
{
    u16 mac_tmp[8];
    u32 now;
    u32 sum_mac, sum_time;
    u16 csum;
    int i;

    tx->udp->sport = bswap16(68);
    tx->udp->dport = bswap16(67);
    tx->udp->len = bswap16(556);
    tx->pseudo.src_ip = src_ip;
    tx->pseudo.dst_ip = dst_ip;
    tx->pseudo.zero = 0;
    tx->pseudo.proto = 17;
    tx->pseudo.len = tx->udp->len;
    tx->udp->check = 0;
    tx->udp->check = udp_checksum((const u16 *)&tx->pseudo, (const u16 *)tx->udp, bswap16(tx->pseudo.len));

    tx->ip->ver = 4;
    tx->ip->ihl = 5;
    tx->ip->tos = 0;
    tx->ip->tot_len = bswap16(576);
    sceKernelMemcpy(mac_tmp, ctx->hw_addr, 6);
    now = sceNet_lib_75D9985C();
    sum_mac = ip_checksum(mac_tmp, 6);
    sum_time = ip_checksum((const u16 *)&now, 2);
    {
        u32 comb = (sum_mac << 16) + sum_time;
        tx->ip->id = (u16)(~(comb + (comb >> 16)));
    }
    tx->ip->frag_off = bswap16(0x4000);
    tx->ip->ttl = 32;
    tx->ip->proto = 17;
    {
        u8 *dst = tx->ip->saddr;
        dst[3] = (u8)(src_ip >> 24);
        dst[1] = (u8)(src_ip >> 8);
        dst[2] = (u8)(src_ip >> 16);
        dst[0] = (u8)src_ip;
        dst = tx->ip->daddr;
        dst[3] = (u8)(dst_ip >> 24);
        dst[1] = (u8)(dst_ip >> 8);
        dst[2] = (u8)(dst_ip >> 16);
        dst[0] = (u8)dst_ip;
    }
    tx->ip->check = 0;
    {
        Ipv4Hdr *ip = tx->ip;
        csum = ip_checksum((const u16 *)ip, 2 * ip->ihl);
        ip->check = csum;
    }

    for (i = 0; i < 6; i++) {
        tx->eth->dst[i] = 0xFF;
        tx->eth->src[i] = ctx->hw_addr[i];
    }
    tx->eth->ethertype = bswap16(0x0800);
    return csum;
}

static inline __attribute__((always_inline))
u32 dhcp_make_xid(const u8 *mac)
{
    u16 mac_tmp[3];
    u32 now = sceNet_lib_75D9985C();
    sceKernelMemcpy(mac_tmp, mac, 6);
    return ((u32)ip_checksum(mac_tmp, 3) << 16) + ip_checksum((const u16 *)&now, 2);
}

static void dhcp_build_discover(DhcpCtx *ctx)
{
    BootpHdr *bootp = ctx->tx.bootp;
    DhcpClientId *cid;
    int off = 4;

    sceKernelMemset(ctx->tx_frame_raw, 0, ctx->tx_frame_len + 2);
    bootp->op = 1;
    bootp->htype = ctx->hw_type;
    bootp->hlen = ctx->hw_len;
    bootp->xid = dhcp_make_xid(ctx->hw_addr);
    sceKernelMemcpy(bootp->chaddr, ctx->hw_addr, bootp->hlen);
    sceKernelMemcpy(bootp->options, &g_dhcp_magic, 4);

    bootp->options[off++] = 53; bootp->options[off++] = 1; bootp->options[off++] = 1;
    if (ctx->requested_ip != 0) {
        bootp->options[off++] = 50; bootp->options[off++] = 4;
        sceKernelMemcpy(bootp->options + off, &ctx->requested_ip, 4);
        off += 4;
    }
    cid = ctx->client_id;
    if (cid != NULL && off + cid->hlen + 3 < 312) {
        bootp->options[off++] = 61;
        bootp->options[off++] = (u8)(cid->hlen + 1);
        bootp->options[off++] = cid->htype;
        sceKernelMemcpy(bootp->options + off, cid->addr, cid->hlen);
        off += cid->hlen;
    }
    if (off + 4 < 312) {
        u16 maxsz = bswap16(576);
        bootp->options[off++] = 57; bootp->options[off++] = 2;
        sceKernelMemcpy(bootp->options + off, &maxsz, 2);
        off += 2;
    }
    if (ctx->cfg.hostname[0] != 0) {
        int hlen = sceNet_lib_D8722983(ctx->cfg.hostname) + 1;
        if (off + hlen + 2 < 312) {
            bootp->options[off++] = 12;
            bootp->options[off++] = (u8)hlen;
            sceKernelMemcpy(bootp->options + off, ctx->cfg.hostname, (u32)hlen);
            off += hlen;
        }
    }
    if (ctx->opt_req_len != 0 && off + ctx->opt_req_len + 2 < 312) {
        bootp->options[off++] = 55;
        bootp->options[off++] = ctx->opt_req_len;
        sceKernelMemcpy(bootp->options + off, ctx->opt_req_list, ctx->opt_req_len);
        off += ctx->opt_req_len;
    }
    bootp->options[off] = 255;
    dhcp_wrap_eth_ip_udp(ctx, &ctx->tx, 0, 0xFFFFFFFFu);
}

static int dhcp_build_request(DhcpCtx *ctx, int mode)
{
    BootpHdr *bootp = ctx->tx.bootp;
    DhcpLease *lease = ctx->cur_lease;
    DhcpClientId *cid;
    int off = 4;

    sceKernelMemset(ctx->tx_frame_raw, 0, ctx->tx_frame_len + 2);

    bootp->op = 1;
    bootp->htype = ctx->hw_type;
    bootp->hlen = ctx->hw_len;
    bootp->xid = dhcp_make_xid(ctx->hw_addr);
    if (mode == 3)
        bootp->ciaddr = 0;
    else
        sceKernelMemcpy(&bootp->ciaddr, lease->yiaddr, 4);
    sceKernelMemcpy(bootp->chaddr, ctx->hw_addr, bootp->hlen);
    sceKernelMemcpy(bootp->options, &g_dhcp_magic, 4);

    bootp->options[off++] = 53; bootp->options[off++] = 1; bootp->options[off++] = 3;
    cid = ctx->client_id;
    if (cid != NULL && off + cid->hlen + 3 < 312) {
        bootp->options[off++] = 61;
        bootp->options[off++] = (u8)(cid->hlen + 1);
        bootp->options[off++] = cid->htype;
        sceKernelMemcpy(bootp->options + off, cid->addr, cid->hlen);
        off += cid->hlen;
    }
    if (mode == 3) {
        u32 yiaddr;
        sceKernelMemcpy(&yiaddr, lease->yiaddr, 4);
        if (yiaddr == 0 || off + 6 >= 312)
            return DHCP_ERR_BUILD;
        bootp->options[off++] = 50; bootp->options[off++] = 4;
        sceKernelMemcpy(bootp->options + off, lease->yiaddr, 4);
        off += 4;
        if (lease->server_id == 0 || off + 6 >= 312)
            return DHCP_ERR_BUILD;
        bootp->options[off++] = 54; bootp->options[off++] = 4;
        sceKernelMemcpy(bootp->options + off, &lease->server_id, 4);
        off += 4;
    }
    if (off + 4 < 312) {
        u16 maxsz = bswap16(576);
        bootp->options[off++] = 57; bootp->options[off++] = 2;
        sceKernelMemcpy(bootp->options + off, &maxsz, 2);
        off += 2;
    }
    if (ctx->cfg.hostname[0] != 0) {
        int hlen = sceNet_lib_D8722983(ctx->cfg.hostname) + 1;
        if (off + hlen + 2 < 312) {
            bootp->options[off++] = 12;
            bootp->options[off++] = (u8)hlen;
            sceKernelMemcpy(bootp->options + off, ctx->cfg.hostname, (u32)hlen);
            off += hlen;
        }
    }
    if (ctx->opt_req_len != 0 && off + ctx->opt_req_len + 2 < 312) {
        bootp->options[off++] = 55;
        bootp->options[off++] = ctx->opt_req_len;
        sceKernelMemcpy(bootp->options + off, ctx->opt_req_list, ctx->opt_req_len);
        off += ctx->opt_req_len;
    }
    bootp->options[off] = 255;

    if (mode == 3) {
        dhcp_wrap_eth_ip_udp(ctx, &ctx->tx, 0, 0xFFFFFFFFu);
        return 0;
    }
    if (mode == 6) {
        u32 yiaddr;
        sceKernelMemcpy(&yiaddr, lease->yiaddr, 4);
        dhcp_wrap_eth_ip_udp(ctx, &ctx->tx, yiaddr, 0xFFFFFFFFu);
        return 0;
    }
    if (mode == 5)
        return 0;
    return DHCP_ERR_BUILD;
}

static int dhcp_build_decline(DhcpCtx *ctx, int in_use)
{
    BootpHdr *bootp = ctx->tx.bootp;
    DhcpLease *lease = ctx->cur_lease;
    DhcpClientId *cid;
    char msg[256], ipbuf[16];
    u32 yiaddr;
    int pos, off = 4, mlen;

    sceKernelMemset(ctx->tx_frame_raw, 0, ctx->tx_frame_len + 2);
    sceNet_lib_80C9F02A(msg, "Such IP address (");
    pos = sceNet_lib_D8722983(msg);
    if (sceNetInetInetNtop(2, lease->yiaddr, ipbuf, 16) != NULL) {
        sceNet_lib_80C9F02A(msg + pos, ipbuf);
        pos += sceNet_lib_D8722983(ipbuf);
    }
    sceNet_lib_80C9F02A(msg + pos, in_use ? ") is already in use" : ") isn't preferable");

    bootp->op = 1;
    bootp->htype = ctx->hw_type;
    bootp->hlen = ctx->hw_len;
    bootp->xid = dhcp_make_xid(ctx->hw_addr);
    sceKernelMemcpy(bootp->chaddr, ctx->hw_addr, bootp->hlen);
    sceKernelMemcpy(bootp->options, &g_dhcp_magic, 4);

    bootp->options[off++] = 53; bootp->options[off++] = 1; bootp->options[off++] = 4;
    sceKernelMemcpy(&yiaddr, lease->yiaddr, 4);
    if (yiaddr == 0)
        return DHCP_ERR_BUILD;
    bootp->options[off++] = 50; bootp->options[off++] = 4;
    sceKernelMemcpy(bootp->options + off, lease->yiaddr, 4);
    off += 4;
    cid = ctx->client_id;
    if (cid != NULL && off + cid->hlen + 3 < 312) {
        bootp->options[off++] = 61;
        bootp->options[off++] = (u8)(cid->hlen + 1);
        bootp->options[off++] = cid->htype;
        sceKernelMemcpy(bootp->options + off, cid->addr, cid->hlen);
        off += cid->hlen;
    }
    if (lease->server_id == 0 || off + 6 >= 312)
        return DHCP_ERR_BUILD;
    bootp->options[off++] = 54; bootp->options[off++] = 4;
    sceKernelMemcpy(bootp->options + off, &lease->server_id, 4);
    off += 4;
    mlen = sceNet_lib_D8722983(msg);
    if (off + mlen + 2 < 312) {
        bootp->options[off++] = 56;
        bootp->options[off++] = (u8)mlen;
        sceKernelMemcpy(bootp->options + off, msg, (u32)mlen);
        off += mlen;
    }
    bootp->options[off] = 255;
    dhcp_wrap_eth_ip_udp(ctx, &ctx->tx, 0, 0xFFFFFFFFu);
    return 0;
}

static int dhcp_build_release(DhcpCtx *ctx)
{
    BootpHdr *bootp = ctx->tx.bootp;
    DhcpLease *lease = ctx->cur_lease;
    DhcpClientId *cid;
    char msg[256], ipbuf[16];
    int pos, off = 4, mlen;

    sceKernelMemset(ctx->tx_frame_raw, 0, ctx->tx_frame_len + 2);
    sceNet_lib_80C9F02A(msg, "I'm no longer need the IP address (");
    pos = sceNet_lib_D8722983(msg);
    if (sceNetInetInetNtop(2, lease->yiaddr, ipbuf, 16) != NULL) {
        sceNet_lib_80C9F02A(msg + pos, ipbuf);
        pos += sceNet_lib_D8722983(ipbuf);
    }
    sceNet_lib_80C9F02A(msg + pos, ")");

    bootp->op = 1;
    bootp->htype = ctx->hw_type;
    bootp->hlen = ctx->hw_len;
    bootp->xid = dhcp_make_xid(ctx->hw_addr);
    sceKernelMemcpy(&bootp->ciaddr, lease->yiaddr, 4);
    sceKernelMemcpy(bootp->chaddr, ctx->hw_addr, bootp->hlen);
    sceKernelMemcpy(bootp->options, &g_dhcp_magic, 4);

    bootp->options[off++] = 53; bootp->options[off++] = 1; bootp->options[off++] = 7;
    cid = ctx->client_id;
    if (cid != NULL && off + cid->hlen + 3 < 312) {
        bootp->options[off++] = 61;
        bootp->options[off++] = (u8)(cid->hlen + 1);
        bootp->options[off++] = cid->htype;
        sceKernelMemcpy(bootp->options + off, cid->addr, cid->hlen);
        off += cid->hlen;
    }
    if (lease->server_id == 0 || off + 6 >= 312)
        return DHCP_ERR_BUILD;
    bootp->options[off++] = 54; bootp->options[off++] = 4;
    sceKernelMemcpy(bootp->options + off, &lease->server_id, 4);
    off += 4;
    mlen = sceNet_lib_D8722983(msg);
    if (off + mlen + 2 < 312) {
        bootp->options[off++] = 56;
        bootp->options[off++] = (u8)mlen;
        sceKernelMemcpy(bootp->options + off, msg, (u32)mlen);
        off += mlen;
    }
    bootp->options[off] = 255;
    return 0;
}

static int dhcp_open_unicast_sock(int sock)
{
    SockAddrIn sa;
    int sz = 548, reuse = 1;

    if (sock >= 0)
        return sock;
    sock = sceNetInetSocket(2, 2, 0);
    if (sock < 0)
        return DHCP_ERR_SOCKET;
    sceKernelMemset(&sa, 0, sizeof(sa));
    sa.sin_family = 2;
    sa.sin_port = bswap16(68);
    if (sceNetInetBind(sock, &sa, sizeof(sa)) < 0 ||
        sceNetInetSetsockopt(sock, 0xFFFF, 4097, &sz, 4) < 0 ||
        sceNetInetSetsockopt(sock, 0xFFFF, 512, &reuse, 4) < 0) {
        sceNetInetClose(sock);
        return DHCP_ERR_SOCKET;
    }
    return sock;
}

static int dhcp_send_unicast(int sock, const u32 *server_ip, void *bootp)
{
    SockAddrIn sa;
    u32 iov[2];
    u32 msg[7];
    sceKernelMemset(&sa, 0, sizeof(sa));
    sa.sin_family = 2;
    sa.sin_port = bswap16(67);
    sa.sin_addr = *server_ip;
    iov[0] = (u32)bootp;
    iov[1] = 548;
    sceKernelMemset(msg, 0, sizeof(msg));
    msg[0] = (u32)&sa;
    msg[1] = sizeof(sa);
    msg[2] = (u32)iov;
    msg[3] = 1;
    if (sceNetInetSendmsg(sock, msg, 0) < 0) {
        sceNetInetClose(sock);
        return DHCP_ERR_SOCKET;
    }
    return 0;
}

static void dhcp_abort_exit(DhcpCtx *ctx, int state, int err)
{
    if (state >= 4 && state <= 6) {
        if (dhcp_build_release(ctx) >= 0) {
            int s = dhcp_open_unicast_sock(ctx->unicast_sock);
            ctx->unicast_sock = s;
            if (s >= 0) {
                dhcp_send_unicast(s, &ctx->cur_lease->server_id, ctx->tx.bootp);
                sceKernelDelayThread(100000);
                if (ctx->unicast_sock >= 0) {
                    sceNetInetClose(ctx->unicast_sock);
                    ctx->unicast_sock = -1;
                }
            }
        }
    }
    sceNetInet_lib_DC38FEE9(ctx->server_mac, ctx->server_arp_ip);
    dhcp_clear_if(ctx->ifname);
    if (ctx->cb != NULL)
        ctx->cb(err != 0 ? 4 : 1, err);
    sceKernelExitThread(0);
}

static int dhcp_arp_send_req(DhcpCtx *ctx, const void *src_ip, const u32 *target_ip)
{
    ArpEthFrame frame;
    int i;
    sceKernelMemset(&frame, 0, sizeof(frame));
    g_arp_bpf_insns[5].k = bswap32(((const PackedU32 *)target_ip)->val);
    if (sceNet_lib_E1F4696F(ctx->bpf_arp, 0x80084267u, &g_arp_bpf_prog) < 0)
        return 0;
    frame.htype = bswap16(1);
    frame.ptype = bswap16(0x0800);
    frame.hlen = 6;
    frame.plen = 4;
    frame.oper = bswap16(1);
    for (i = 0; i < 6; i++) {
        frame.eth_dst[i] = 0xFF;
        frame.eth_src[i] = ctx->hw_addr[i];
        frame.sha[i] = ctx->hw_addr[i];
        frame.tha[i] = 0;
    }
    sceKernelMemcpy(frame.spa, src_ip, frame.plen);
    sceKernelMemcpy(frame.tpa, target_ip, frame.plen);
    frame.eth_type = bswap16(0x0806);
    return sceNet_lib_C431A214(ctx->bpf_arp, &frame, sizeof(frame));
}

static int dhcp_arp_probe(DhcpCtx *ctx, const void *ip)
{
    u32 zero = 0;
    int n;
    if (dhcp_arp_send_req(ctx, &zero, (const u32 *)ip) < 0)
        return 0;
    n = sceNet_lib_B6FC0A5B(ctx->bpf_arp, ctx->arp_rx_buf, ctx->arp_rx_buf_size);
    return (n > 0) ? -1 : 0;
}

static int dhcp_arp_resolve_gw(DhcpCtx *ctx, const void *gw_ip)
{
    u32 target;
    int tries = 4;
    sceKernelMemcpy(&target, gw_ip, 4);
    if (dhcp_arp_send_req(ctx, ctx->cur_lease->yiaddr, &target) < 0)
        return -1;
    while (tries > 0) {
        int n = sceNet_lib_B6FC0A5B(ctx->bpf_arp, ctx->arp_rx_buf, ctx->arp_rx_buf_size);
        if (n < 0)
            return -1;
        if (sceKernelPollSema(ctx->sema, 1) == 0)
            return 0;
        if (n > 0) {
            const BpfHdr *bh = (const BpfHdr *)ctx->arp_rx_buf;
            sceKernelMemcpy(ctx->server_mac, ctx->arp_rx_buf + bh->hdrlen + 22, 6);
            return 1;
        }
        tries--;
    }
    return -1;
}

static int dhcp_send_gratuitous_arp(DhcpCtx *ctx, const void *ip)
{
    ArpEthFrame frame;
    int i;
    sceKernelMemset(&frame, 0, sizeof(frame));
    frame.htype = bswap16(1);
    frame.ptype = bswap16(0x0800);
    frame.hlen = 6;
    frame.plen = 4;
    frame.oper = bswap16(2);
    for (i = 0; i < 6; i++) {
        frame.eth_dst[i] = 0xFF;
        frame.eth_src[i] = ctx->hw_addr[i];
        frame.sha[i] = ctx->hw_addr[i];
        frame.tha[i] = ctx->hw_addr[i];
    }
    sceKernelMemcpy(frame.spa, ip, frame.plen);
    sceKernelMemcpy(frame.tpa, ip, frame.plen);
    frame.eth_type = bswap16(0x0806);
    sceNet_lib_C431A214(ctx->bpf_arp, &frame, sizeof(frame));
    return 0;
}

static int dhcp_retransmit(DhcpCtx *ctx)
{
    u16 elapsed = (u16)(sceNet_lib_75D9985C() - ctx->start_time);
    ctx->tx.bootp->secs = bswap16(elapsed);
    ctx->tx.udp->check = 0;
    ctx->tx.udp->check = udp_checksum((const u16 *)&ctx->tx.pseudo, (const u16 *)ctx->tx.udp,
                                      bswap16(ctx->tx.pseudo.len));
    sceNet_lib_C431A214(ctx->bpf_dhcp, ctx->tx_frame_raw + 2, ctx->tx_frame_len);
    return 0;
}

static inline __attribute__((always_inline))
int dhcp_send_mcast_probe(DhcpCtx *ctx)
{
    EthHdr frame;
    u32 addr = sceNetInetInetAddr("224.0.0.203");
    int n;
    sceKernelMemset(&frame, 0, sizeof(frame));
    frame.dst[0] = 1;
    frame.dst[1] = 0;
    frame.dst[2] = 94;
    frame.dst[3] = (u8)((addr >> 8) & 0x7F);
    frame.dst[4] = (u8)(addr >> 16);
    frame.dst[5] = (u8)(addr >> 24);
    sceKernelMemcpy(frame.src, ctx->tx.eth->src, 6);
    frame.ethertype = bswap16(0x88C8);
    n = sceNet_lib_C431A214(ctx->bpf_dhcp, &frame, sizeof(frame));
    return (n < (int)sizeof(frame)) ? DHCP_ERR_BPF : 0;
}

static int dhcp_apply_if_ip(DhcpCtx *ctx, const DhcpLease *lease)
{
    u32 cur_ip, cur_mask, cur_brd;
    u32 ip, mask, brd;
    int ret;
    sceKernelMemset(&cur_ip, 0, 4);
    sceKernelMemset(&cur_mask, 0, 4);
    sceKernelMemset(&cur_brd, 0, 4);
    ret = sceNetInet_lib_5155EC8A(ctx->ifname, &cur_ip, &cur_brd, &cur_mask);
    if (ret < 0)
        return ret;
    sceKernelMemset(&ip, 0, 4);
    sceKernelMemset(&mask, 0, 4);
    sceKernelMemset(&brd, 0, 4);
    ip = ((const PackedU32 *)lease->yiaddr)->val;
    mask = lease->subnet_mask;
    if (mask == 0)
        mask = default_netmask(&ip);
    brd = lease->broadcast_addr;
    if (brd == 0)
        brd = ip | ~mask;
    if (cur_ip == ip && cur_mask == mask && cur_brd == brd)
        return 0;
    ret = sceNetInet_lib_4F68DB0E(ctx->ifname, &ip, &brd, &mask);
    sceNetInet_lib_A94A75E7(&ip);
    return ret;
}

static int dhcp_apply_dns_gw(DhcpCtx *ctx)
{
    u32 dns_a, dns_b, gw;
    DhcpLease *lease = ctx->cur_lease;
    int r;

    sceKernelMemset(&dns_a, 0, 4);
    sceKernelMemset(&dns_b, 0, 4);
    sceKernelMemset(&gw, 0, 4);
    if (ctx->cfg.use_static_dns == 0) {
        dns_a = lease->dns_a;
        if (lease->dns_b != 0) {
            if (dns_a != 0) dns_b = lease->dns_b;
            else dns_a = lease->dns_b;
        }
    } else {
        sceNetInetInetAton(ctx->cfg.dns_a, &dns_a);
        if (dns_a != 0) {
            sceNetInetInetAton(ctx->cfg.dns_b, &dns_b);
            if (dns_b == 0 && lease->dns_a != 0)
                dns_b = lease->dns_a;
        }
    }
    gw = lease->router_ip;
    if (ctx->applied_dns_a == dns_a && ctx->applied_dns_b == dns_b && ctx->applied_gw == gw)
        return 0;
    if (gw != 0) {
        r = sceNetInet_lib_7776A492(&gw);
        if (r < 0)
            return r;
    }
    r = sceNetInet_lib_AAF4895A(&dns_a, &dns_b);
    if (r >= 0) {
        ctx->applied_dns_a = dns_a;
        ctx->applied_dns_b = dns_b;
        ctx->applied_gw = gw;
    }
    return r;
}

static inline __attribute__((always_inline))
void dhcp_backoff(DhcpCtx *ctx)
{
    u32 cur = ctx->cur_backoff;
    u32 max_t = ctx->retry_max;
    u32 next = cur + ((u32)sceNet_lib_1858883D() % (2 * cur));
    if (next > max_t)
        next = (max_t >> 1) + ((u32)sceNet_lib_1858883D() % max_t);
    ctx->cur_backoff = next;
}

/* DHCP state functions (0..6) */
static int dhcp_st_init(DhcpCtx *ctx, int prev)
{
    UdpHdr *udp;
    int ret, n;
    (void)prev;

    ret = dhcp_clear_if(ctx->ifname);
    if (ret < 0)
        return ret;
    dhcp_build_discover(ctx);
    udp = ctx->tx.udp;
    ctx->tx.bootp->secs = 0;
    udp->check = 0;
    udp->check = udp_checksum((const u16 *)&ctx->tx.pseudo, (const u16 *)udp, bswap16(ctx->tx.pseudo.len));
    if (ctx->cfg.mcast_probe_flag == 1) {
        ret = dhcp_send_mcast_probe(ctx);
        if (ret < 0)
            return ret;
    }
    n = sceNet_lib_C431A214(ctx->bpf_dhcp, ctx->tx_frame_raw + 2, ctx->tx_frame_len);
    return (n >= (int)ctx->tx_frame_len) ? 1 : DHCP_ERR_BPF;
}

static int dhcp_st_selecting(DhcpCtx *ctx, int prev)
{
    u32 now, lim, wait;

    if (prev != 0) {
        dhcp_backoff(ctx);
    } else {
        if (ctx->cur_lease != NULL) {
            dhcp_lease_clear(ctx->cur_lease);
            ctx->cur_lease = NULL;
        }
        ctx->cur_backoff = ctx->retry_init;
    }

    now = sceNet_lib_75D9985C();
    lim = ctx->timeout_sec;
    if (lim != 0 && now + ctx->cur_backoff >= ctx->start_time + lim) {
        if (now >= ctx->start_time + lim) {
            dhcp_abort_exit(ctx, 1, DHCP_ERR_TIMEOUT);
            return DHCP_ERR_TIMEOUT;
        }
        wait = ctx->start_time + lim - now;
    } else {
        wait = ctx->cur_backoff;
    }

    while (wait > 0) {
        u8 *p;
        int n = sceNet_lib_B6FC0A5B(ctx->bpf_dhcp, ctx->dhcp_rx_buf, ctx->dhcp_rx_buf_size);
        if (n < 0)
            return DHCP_ERR_BPF;
        if (sceKernelPollSema(ctx->sema, 1) == 0)
            dhcp_abort_exit(ctx, 1, 0);
        p = ctx->dhcp_rx_buf;
        while (n > 0) {
            const BpfHdr *bh = (const BpfHdr *)p;
            u16 hlen = bh->hdrlen;
            u32 caplen = bh->caplen;
            u8 *eth = p + hlen;
            u8 *ip = eth + 14;
            u8 *udp = ip + 4 * (ip[0] & 0xF);
            u8 *bootp = udp + 8;
            u16 udp_len;
            u32 step;

            ctx->rx.eth = (EthHdr *)eth;
            ctx->rx.ip = (Ipv4Hdr *)ip;
            ctx->rx.udp = (UdpHdr *)udp;
            ctx->rx.bootp = (BootpHdr *)bootp;
            udp_len = bswap16(ctx->rx.udp->len);
            if (dhcp_verify_offer(ctx) == 0) {
                sceKernelMemset(&ctx->lease_storage, 0, sizeof(DhcpLease));
                if (dhcp_validate_opts(bootp, udp_len - 8) == 0) {
                    int r = dhcp_parse_lease(bootp, udp_len - 8, &ctx->lease_storage);
                    if (r < 0)
                        return r;
                    if (ctx->lease_storage.server_id != 0 && ctx->lease_storage.lease_time != 0) {
                        ctx->cur_lease = &ctx->lease_storage;
                        return 2;
                    }
                    dhcp_lease_clear(&ctx->lease_storage);
                }
            }
            step = (hlen + caplen + 3) & ~3u;
            p += step;
            n -= (int)step;
        }
        wait--;
    }

    now = sceNet_lib_75D9985C();
    if (lim != 0 && now >= ctx->start_time + lim) {
        dhcp_abort_exit(ctx, 1, DHCP_ERR_TIMEOUT);
        return DHCP_ERR_TIMEOUT;
    }
    if (ctx->cfg.mcast_probe_flag == 1) {
        int r = dhcp_send_mcast_probe(ctx);
        if (r < 0)
            return r;
    }
    dhcp_retransmit(ctx);
    return 1;
}

static int dhcp_st_send_req(DhcpCtx *ctx, int prev)
{
    int ret, n;
    (void)prev;
    ret = dhcp_build_request(ctx, 3);
    if (ret < 0)
        return ret;
    n = sceNet_lib_C431A214(ctx->bpf_dhcp, ctx->tx_frame_raw + 2, ctx->tx_frame_len);
    return (n >= (int)ctx->tx_frame_len) ? 3 : DHCP_ERR_BPF;
}

static int dhcp_st_requesting(DhcpCtx *ctx, int prev)
{
    u32 now, lim, wait;

    if (prev == 2)
        ctx->cur_backoff = ctx->retry_init;
    else
        dhcp_backoff(ctx);

    now = sceNet_lib_75D9985C();
    lim = ctx->timeout_sec;
    if (lim != 0 && now + ctx->cur_backoff >= ctx->start_time + lim) {
        if (now >= ctx->start_time + lim) {
            dhcp_abort_exit(ctx, 3, DHCP_ERR_TIMEOUT);
            return DHCP_ERR_TIMEOUT;
        }
        wait = ctx->start_time + lim - now;
    } else {
        wait = ctx->cur_backoff;
    }

    while (wait > 0) {
        u8 *p;
        int n = sceNet_lib_B6FC0A5B(ctx->bpf_dhcp, ctx->dhcp_rx_buf, ctx->dhcp_rx_buf_size);
        if (n < 0)
            return DHCP_ERR_BPF;
        if (sceKernelPollSema(ctx->sema, 1) == 0)
            dhcp_abort_exit(ctx, 3, 0);
        p = ctx->dhcp_rx_buf;
        while (n > 0) {
            const BpfHdr *bh = (const BpfHdr *)p;
            u16 hlen = bh->hdrlen;
            u32 caplen = bh->caplen;
            u8 *eth = p + hlen;
            u8 *ip = eth + 14;
            u8 *udp = ip + 4 * (ip[0] & 0xF);
            u8 *bootp = udp + 8;
            u16 udp_len;
            u32 step;

            ctx->rx.eth = (EthHdr *)eth;
            ctx->rx.ip = (Ipv4Hdr *)ip;
            ctx->rx.udp = (UdpHdr *)udp;
            ctx->rx.bootp = (BootpHdr *)bootp;
            udp_len = bswap16(ctx->rx.udp->len);
            if (dhcp_verify_ack(ctx) == 0) {
                const u8 *mtype = dhcp_find_opt(bootp, udp_len - 8, 53);
                if (mtype != NULL) {
                    if (mtype[2] == 6) {
                        u8 msgbuf[256];
                        const u8 *m;
                        sceKernelMemset(msgbuf, 0, sizeof(msgbuf));
                        m = dhcp_find_opt(bootp, udp_len - 8, 56);
                        if (m != NULL)
                            dhcp_copy_str(m + 2, msgbuf, m[1]);
                        dhcp_lease_clear(ctx->cur_lease);
                        ctx->cur_lease = NULL;
                        return 0;
                    }
                    if (mtype[2] == 5 && dhcp_validate_opts(bootp, udp_len - 8) == 0) {
                        int conflict;
                        int r = dhcp_parse_lease(bootp, udp_len - 8, ctx->cur_lease);
                        if (r < 0)
                            return r;
                        conflict = dhcp_arp_probe(ctx, ctx->cur_lease->yiaddr);
                        if (conflict != 0) {
                            r = dhcp_build_decline(ctx, conflict);
                            if (r < 0)
                                return r;
                            if (sceNet_lib_C431A214(ctx->bpf_dhcp, ctx->tx_frame_raw + 2, ctx->tx_frame_len) < (int)ctx->tx_frame_len)
                                return DHCP_ERR_BPF;
                            dhcp_lease_clear(ctx->cur_lease);
                            ctx->cur_lease = NULL;
                            return 0;
                        }
                        ctx->lease_start_time = sceNet_lib_75D9985C();
                        dhcp_send_gratuitous_arp(ctx, ctx->cur_lease->yiaddr);
                        return 4;
                    }
                }
            }
            step = (hlen + caplen + 3) & ~3u;
            p += step;
            n -= (int)step;
        }
        wait--;
    }

    now = sceNet_lib_75D9985C();
    if (lim != 0 && now >= ctx->start_time + lim) {
        dhcp_abort_exit(ctx, 3, DHCP_ERR_TIMEOUT);
        return DHCP_ERR_TIMEOUT;
    }
    dhcp_retransmit(ctx);
    return 3;
}

static int dhcp_st_bound(DhcpCtx *ctx, int prev)
{
    DhcpLease *lease = ctx->cur_lease;
    u32 yiaddr, mask, now;
    int ret, remain;
    (void)prev;

    ret = dhcp_apply_if_ip(ctx, lease);
    if (ret < 0)
        return ret;
    ret = dhcp_apply_dns_gw(ctx);
    if (ret < 0)
        return ret;

    sceKernelMemcpy(&yiaddr, lease->yiaddr, 4);
    mask = lease->subnet_mask;
    if ((lease->server_id & mask) == (yiaddr & mask)) {
        sceKernelMemcpy(ctx->server_arp_ip, &lease->server_id, 4);
        sceKernelMemcpy(ctx->server_mac, ctx->rx.eth->src, 6);
        sceNetInet_lib_59561561(ctx->server_mac, ctx->server_arp_ip);
    } else {
        int ar;
        sceKernelMemcpy(ctx->server_arp_ip, &lease->router_ip, 4);
        ar = dhcp_arp_resolve_gw(ctx, &lease->router_ip);
        if (ar == 0)
            dhcp_abort_exit(ctx, 4, 0);
        if (ar >= 0)
            sceNetInet_lib_59561561(ctx->server_mac, ctx->server_arp_ip);
    }

    ctx->bound_flag = 1;
    if (ctx->cb != NULL)
        ctx->cb(0, 0);

    remain = (int)lease->renew_time;
    while (remain-- > 0) {
        sceKernelDelayThread(1000000);
        if (sceKernelPollSema(ctx->sema, 1) == 0)
            dhcp_abort_exit(ctx, 4, 0);
    }

    now = sceNet_lib_75D9985C();
    if (now < ctx->lease_start_time + lease->rebind_time) {
        int s;
        ret = dhcp_build_request(ctx, 5);
        if (ret < 0)
            return ret;
        if (lease->server_id == 0)
            return DHCP_ERR_TIMEOUT;
        s = dhcp_open_unicast_sock(ctx->unicast_sock);
        ctx->unicast_sock = s;
        if (s < 0)
            return s;
        ret = dhcp_send_unicast(s, &lease->server_id, ctx->tx.bootp);
        if (ret < 0) {
            sceNetInetClose(ctx->unicast_sock);
            ctx->unicast_sock = -1;
            return ret;
        }
        return 5;
    }
    if (now >= ctx->lease_start_time + lease->lease_time)
        return DHCP_ERR_EXPIRED;
    ret = dhcp_build_request(ctx, 6);
    if (ret < 0)
        return ret;
    if (sceNet_lib_C431A214(ctx->bpf_dhcp, ctx->tx_frame_raw + 2, ctx->tx_frame_len) < (int)ctx->tx_frame_len)
        return DHCP_ERR_BPF;
    return 6;
}

static int dhcp_st_renewing(DhcpCtx *ctx, int prev)
{
    DhcpLease *lease;
    u32 now, rebind_end, wait;
    int s, r;

    if (prev == 4)
        ctx->cur_backoff = ctx->retry_init;
    else
        dhcp_backoff(ctx);

    lease = ctx->cur_lease;
    now = sceNet_lib_75D9985C();
    rebind_end = ctx->lease_start_time + lease->rebind_time;
    if (now >= rebind_end)
        return 6;

    wait = rebind_end - now;
    if (ctx->cur_backoff < wait)
        wait = ctx->cur_backoff;

    while (wait > 0) {
        u8 *p;
        int n = sceNet_lib_B6FC0A5B(ctx->bpf_dhcp, ctx->dhcp_rx_buf, ctx->dhcp_rx_buf_size);
        if (n < 0)
            return DHCP_ERR_BPF;
        if (sceKernelPollSema(ctx->sema, 1) == 0)
            dhcp_abort_exit(ctx, 5, 0);
        p = ctx->dhcp_rx_buf;
        while (n > 0) {
            const BpfHdr *bh = (const BpfHdr *)p;
            u16 hlen = bh->hdrlen;
            u32 caplen = bh->caplen;
            u8 *eth = p + hlen;
            u8 *ip = eth + 14;
            u8 *udp = ip + 4 * (ip[0] & 0xF);
            u8 *bootp = udp + 8;
            u16 udp_len;
            u32 step;

            ctx->rx.eth = (EthHdr *)eth;
            ctx->rx.ip = (Ipv4Hdr *)ip;
            ctx->rx.udp = (UdpHdr *)udp;
            ctx->rx.bootp = (BootpHdr *)bootp;
            udp_len = bswap16(ctx->rx.udp->len);
            if (dhcp_verify_ack(ctx) == 0) {
                const u8 *mtype = dhcp_find_opt(bootp, udp_len - 8, 53);
                if (mtype != NULL) {
                    if (mtype[2] == 6) {
                        u8 msgbuf[256];
                        const u8 *m;
                        sceKernelMemset(msgbuf, 0, sizeof(msgbuf));
                        m = dhcp_find_opt(bootp, udp_len - 8, 56);
                        if (m != NULL)
                            dhcp_copy_str(m + 2, msgbuf, m[1]);
                        dhcp_lease_clear(lease);
                        ctx->cur_lease = NULL;
                        return DHCP_ERR_NAK;
                    }
                    if (mtype[2] == 5 && dhcp_validate_opts(bootp, udp_len - 8) == 0) {
                        r = dhcp_parse_lease(bootp, udp_len - 8, lease);
                        if (r < 0)
                            return r;
                        ctx->lease_start_time = sceNet_lib_75D9985C();
                        dhcp_send_gratuitous_arp(ctx, lease->yiaddr);
                        if (ctx->cb != NULL)
                            ctx->cb(2, 0);
                        if (ctx->unicast_sock >= 0) {
                            sceNetInetClose(ctx->unicast_sock);
                            ctx->unicast_sock = -1;
                        }
                        return 4;
                    }
                }
            }
            step = (hlen + caplen + 3) & ~3u;
            p += step;
            n -= (int)step;
        }
        wait--;
    }

    now = sceNet_lib_75D9985C();
    if (now >= ctx->lease_start_time + lease->rebind_time) {
        r = dhcp_build_request(ctx, 6);
        if (r < 0)
            return r;
        if (sceNet_lib_C431A214(ctx->bpf_dhcp, ctx->tx_frame_raw + 2, ctx->tx_frame_len) < (int)ctx->tx_frame_len)
            return DHCP_ERR_BPF;
        return 6;
    }
    if (lease->server_id == 0)
        return DHCP_ERR_TIMEOUT;
    s = dhcp_open_unicast_sock(ctx->unicast_sock);
    ctx->unicast_sock = s;
    if (s < 0)
        return s;
    r = dhcp_send_unicast(s, &lease->server_id, ctx->tx.bootp);
    if (r < 0) {
        sceNetInetClose(ctx->unicast_sock);
        ctx->unicast_sock = -1;
        return r;
    }
    return 5;
}

static int dhcp_st_rebinding(DhcpCtx *ctx, int prev)
{
    DhcpLease *lease;
    u32 now, end, wait;

    if (ctx->cb != NULL)
        ctx->cb(3, 0);
    if (prev != 6)
        ctx->cur_backoff = ctx->retry_init;

    lease = ctx->cur_lease;
    now = sceNet_lib_75D9985C();
    end = ctx->lease_start_time + lease->lease_time;
    if (now >= end)
        return DHCP_ERR_EXPIRED;

    dhcp_backoff(ctx);
    wait = end - now;
    if (ctx->cur_backoff < wait)
        wait = ctx->cur_backoff;

    while (wait > 0) {
        u8 *p;
        int n = sceNet_lib_B6FC0A5B(ctx->bpf_dhcp, ctx->dhcp_rx_buf, ctx->dhcp_rx_buf_size);
        if (n < 0)
            return DHCP_ERR_BPF;
        if (sceKernelPollSema(ctx->sema, 1) == 0)
            dhcp_abort_exit(ctx, 6, 0);
        p = ctx->dhcp_rx_buf;
        while (n > 0) {
            const BpfHdr *bh = (const BpfHdr *)p;
            u16 hlen = bh->hdrlen;
            u32 caplen = bh->caplen;
            u8 *eth = p + hlen;
            u8 *ip = eth + 14;
            u8 *udp = ip + 4 * (ip[0] & 0xF);
            u8 *bootp = udp + 8;
            u16 udp_len;
            u32 step;

            ctx->rx.eth = (EthHdr *)eth;
            ctx->rx.ip = (Ipv4Hdr *)ip;
            ctx->rx.udp = (UdpHdr *)udp;
            ctx->rx.bootp = (BootpHdr *)bootp;
            udp_len = bswap16(ctx->rx.udp->len);
            if (dhcp_verify_ack(ctx) == 0) {
                const u8 *mtype = dhcp_find_opt(bootp, udp_len - 8, 53);
                if (mtype != NULL) {
                    if (mtype[2] == 6) {
                        u8 msgbuf[256];
                        const u8 *m;
                        sceKernelMemset(msgbuf, 0, sizeof(msgbuf));
                        m = dhcp_find_opt(bootp, udp_len - 8, 56);
                        if (m != NULL)
                            dhcp_copy_str(m + 2, msgbuf, m[1]);
                        dhcp_lease_clear(lease);
                        ctx->cur_lease = NULL;
                        return DHCP_ERR_NAK;
                    }
                    if (mtype[2] == 5 && dhcp_validate_opts(bootp, udp_len - 8) == 0) {
                        int r = dhcp_parse_lease(bootp, udp_len - 8, lease);
                        if (r < 0)
                            return r;
                        ctx->lease_start_time = sceNet_lib_75D9985C();
                        dhcp_send_gratuitous_arp(ctx, lease->yiaddr);
                        return 4;
                    }
                }
            }
            step = (hlen + caplen + 3) & ~3u;
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

typedef int (*DhcpStateFn)(DhcpCtx *, int);
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
    g_dhcp_ctx.state = 0;
    g_dhcp_ctx.start_time = sceNet_lib_75D9985C();

    for (;;) {
        int cur = g_dhcp_ctx.state;
        int next = g_dhcp_states[cur](&g_dhcp_ctx, prev);
        if (next < 0) {
            if (g_dhcp_ctx.cb != NULL)
                g_dhcp_ctx.cb(4, next);
            dhcp_clear_if(g_dhcp_ctx.ifname);
            sceKernelExitThread(0);
            return 0;
        }
        prev = cur;
        g_dhcp_ctx.state = next;
        if (sceKernelPollSema(g_dhcp_ctx.sema, 1) == 0)
            dhcp_abort_exit(&g_dhcp_ctx, next, 0);
    }
}

int dhcp_start(const char *ifname, void (*cb)(int, int), int stack, int prio, int timeout, void *cfg)
{
    int bpf_dhcp = -1, bpf_arp = -1;
    SceUID sema = -1, thid = -1;
    int ret = DHCP_ERR_ALREADY_STARTED;
    const DhcpConfig *dcfg = (const DhcpConfig *)cfg;

    do {
        u8 mac[6];
        u32 one = 1;
        u32 tv[2] = { 1, 0 };
        char ifr[32];

        if (g_dhcp_running > 0)
            break;

        bpf_dhcp = sceNet_lib_3B617AA0();
        ret = DHCP_ERR_BPF;
        if (bpf_dhcp < 0)
            break;
        bpf_arp = sceNet_lib_3B617AA0();
        if (bpf_arp < 0)
            break;

        sceKernelMemset(&g_dhcp_ctx, 0, sizeof(g_dhcp_ctx));
        sceNet_lib_B5CE388A(g_dhcp_ctx.ifname, ifname, 16);
        g_dhcp_ctx.bpf_dhcp = bpf_dhcp;
        g_dhcp_ctx.bpf_arp = bpf_arp;
        g_dhcp_ctx.unicast_sock = -1;
        g_dhcp_ctx.timeout_sec = (u32)timeout;
        g_dhcp_ctx.retry_init = 3;
        g_dhcp_ctx.retry_max = 15;
        g_dhcp_ctx.hw_type = 1;
        g_dhcp_ctx.hw_len = 6;

        ret = sceNet_lib_8D33C11D(g_dhcp_ctx.ifname, mac);
        if (ret < 0)
            break;
        sceKernelMemcpy(g_dhcp_ctx.hw_addr, mac, 6);

        g_dhcp_ctx.client_id_storage.htype = g_dhcp_ctx.hw_type;
        g_dhcp_ctx.client_id_storage.hlen = g_dhcp_ctx.hw_len;
        g_dhcp_ctx.client_id_storage.addr = g_dhcp_ctx.hw_addr;
        g_dhcp_ctx.client_id = &g_dhcp_ctx.client_id_storage;
        g_dhcp_ctx.opt_req_len = 5;
        g_dhcp_ctx.opt_req_list[0] = 1;
        g_dhcp_ctx.opt_req_list[1] = 3;
        g_dhcp_ctx.opt_req_list[2] = 28;
        g_dhcp_ctx.opt_req_list[3] = 6;
        g_dhcp_ctx.opt_req_list[4] = 15;
        if (dcfg != NULL)
            g_dhcp_ctx.requested_ip = dcfg->requested_ip;
        g_dhcp_ctx.cb = cb;

        if (dcfg != NULL) {
            if ((u32)sceNet_lib_D8722983(dcfg->dns_a) >= 256 ||
                (u32)sceNet_lib_D8722983(dcfg->dns_b) >= 256 ||
                (u32)sceNet_lib_D8722983(dcfg->hostname) >= 256) {
                ret = DHCP_ERR_PARAM;
                break;
            }
            sceKernelMemcpy(&g_dhcp_ctx.cfg, dcfg, sizeof(DhcpConfig));
        }

        ret = DHCP_ERR_BPF;
        if (sceNet_lib_E1F4696F(bpf_dhcp, 0x80044270u, &one) < 0 ||
            sceNet_lib_E1F4696F(bpf_arp, 0x80044270u, &one) < 0)
            break;
        g_dhcp_ctx.dhcp_rx_buf_size = 1500;
        if (sceNet_lib_E1F4696F(bpf_dhcp, 0xC0044266u, &g_dhcp_ctx.dhcp_rx_buf_size) < 0)
            break;
        sceKernelMemset(g_dhcp_ctx.dhcp_rx_buf, 0, g_dhcp_ctx.dhcp_rx_buf_size);
        g_dhcp_ctx.arp_rx_buf_size = 1500;
        if (sceNet_lib_E1F4696F(bpf_arp, 0xC0044266u, &g_dhcp_ctx.arp_rx_buf_size) < 0)
            break;
        sceKernelMemset(g_dhcp_ctx.arp_rx_buf, 0, g_dhcp_ctx.arp_rx_buf_size);
        sceKernelMemset(ifr, 0, sizeof(ifr));
        sceNet_lib_B5CE388A(ifr, g_dhcp_ctx.ifname, 16);
        if (sceNet_lib_E1F4696F(bpf_dhcp, 0x8020426Cu, ifr) < 0 ||
            sceNet_lib_E1F4696F(bpf_arp, 0x8020426Cu, ifr) < 0 ||
            sceNet_lib_E1F4696F(bpf_dhcp, 0x80084267u, &g_dhcp_bpf_prog) < 0 ||
            sceNet_lib_E1F4696F(bpf_dhcp, 0x8008426Du, tv) < 0 ||
            sceNet_lib_E1F4696F(bpf_arp, 0x8008426Du, tv) < 0)
            break;

        g_dhcp_ctx.tx_frame_len = 590;
        sceKernelMemset(g_dhcp_ctx.tx_frame_raw, 0, sizeof(g_dhcp_ctx.tx_frame_raw));
        g_dhcp_ctx.tx.eth = (EthHdr *)(g_dhcp_ctx.tx_frame_raw + 2);
        g_dhcp_ctx.tx.ip = (Ipv4Hdr *)(g_dhcp_ctx.tx_frame_raw + 16);
        g_dhcp_ctx.tx.udp = (UdpHdr *)(g_dhcp_ctx.tx_frame_raw + 36);
        g_dhcp_ctx.tx.bootp = (BootpHdr *)(g_dhcp_ctx.tx_frame_raw + 44);

        sema = sceKernelCreateSema("SceNetDhcpClient", 0, 0, 1, NULL);
        ret = sema;
        if (sema < 0)
            break;
        g_dhcp_ctx.sema = sema;

        thid = sceKernelCreateThread("SceNetDhcpClient", dhcp_thread, prio, stack, 0, NULL);
        ret = thid;
        if (thid < 0)
            break;
        ret = sceKernelStartThread(thid, 0, NULL);
        if (ret < 0)
            break;
        g_dhcp_ctx.thid = thid;
        g_dhcp_running = 1;
        return 0;
    } while (0);

    dhcp_clear_if(ifname);
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
    sceKernelSignalSema(g_dhcp_ctx.sema, 1);
    if (g_dhcp_ctx.thid >= 0) {
        sceKernelWaitThreadEnd(g_dhcp_ctx.thid, NULL);
        sceKernelDeleteThread(g_dhcp_ctx.thid);
        sceNetFreeThreadinfo(g_dhcp_ctx.thid);
    }
    if (g_dhcp_ctx.sema >= 0)
        sceKernelDeleteSema(g_dhcp_ctx.sema);
    if (g_dhcp_ctx.bpf_dhcp >= 0)
        sceNet_lib_DB88F458(g_dhcp_ctx.bpf_dhcp);
    if (g_dhcp_ctx.bpf_arp >= 0)
        sceNet_lib_DB88F458(g_dhcp_ctx.bpf_arp);
    if (g_dhcp_ctx.unicast_sock >= 0) {
        sceNetInetClose(g_dhcp_ctx.unicast_sock);
        g_dhcp_ctx.unicast_sock = -1;
    }
    sceKernelMemset(&g_dhcp_ctx, 0, sizeof(g_dhcp_ctx));
    g_dhcp_running = -1;
    return 0;
}
