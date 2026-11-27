#include "pspnet_inet_internal.h"

s32 g_subnetsarelocal = 1;
s32 g_hostzeroisbroadcast = 1;
s32 g_anonportmin = 49167;
s32 g_anonportmax = 65535;
s32 g_lowportmin = 600;
s32 g_lowportmax = 1023;
u32 g_in_maxmtu;
u32 g_in_ifaddr_hashmask;
u32 g_zeroin_addr;

InetInIfaddr **g_in_ifaddr_hashtbl;
InetInIfaddr *g_in_ifaddr_head;
InetInIfaddr **g_in_ifaddr_tail = &g_in_ifaddr_head;

static InetInIfaddr *g_in_mkopt_head;
static InetInIfaddr **g_in_mkopt_tail = &g_in_mkopt_head;

typedef struct InetIfLaddrReq {
    char iflr_name[16];
    u32 flags;
    u32 prefixlen;
    u8 addr[128];
    u8 dstaddr[128];
} InetIfLaddrReq;

static void inetInSetMaxMtu(void)
{
    InetInIfaddr *ia = g_in_ifaddr_head;
    u32 maxMtu = 0;
    while (ia != NULL) {
        InetIfnet *ifp = ia->ia_ifa.ifa_ifp;
        if (ifp != NULL && (ifp->if_flags & (INET_IFF_UP | INET_IFF_LOOPBACK)) == INET_IFF_UP) {
            u32 mtu = (u32)ifp->if_data.ifi_mtu;
            if (ifp->if_data.ifi_mtu > 0xFFFFFFFFULL || mtu > maxMtu) {
                maxMtu = mtu;
            }
        }
        ia = ia->ia_list_next;
    }
    if (maxMtu != 0) {
        g_in_maxmtu = maxMtu;
    }
}

static InetInIfaddr *inetIfpFirstInAddr(InetIfnet *ifp)
{
    InetIfaddr *ifa = ifp->if_addrhead;
    while (ifa != NULL && ifa->ifa_addr->sa_family != INET_AF_INET) {
        ifa = ifa->ifa_next;
    }
    return (InetInIfaddr *)ifa;
}

static int inetInLifaddrIoctl(InetSocket *so, u32 cmd, void *data, InetIfnet *ifp);

/* [234] 0x0128B8 */
INET_NOINLINE int inetInControl(InetSocket *so, u32 cmd, void *data, InetIfnet *ifp)
{
    InetIfreq *ifr = (InetIfreq *)data;
    InetIfAliasReq *ifra = (InetIfAliasReq *)data;
    InetInIfaddr *ia = NULL;

    if (cmd == 0x8118691CU || cmd == 0x8118691EU || cmd == 0xC118691DU) {
        if (ifp == NULL) {
            return INET_EINVAL;
        }
        return inetInLifaddrIoctl(so, cmd, data, ifp);
    }

    if (ifp != NULL) {
        ia = inetIfpFirstInAddr(ifp);
    }

    switch (cmd) {
    case 0x80206919U: /* SIOCDIFADDR */
    case 0x8040691AU: /* SIOCAIFADDR */
    case 0xC040691BU: /* SIOCGIFALIAS */
        if (ifr->ifr_u.ifru_addr.sa_family == INET_AF_INET) {
            u32 targetIp = ifr->ifr_u.ifru_sin.sin_addr;
            ia = g_in_ifaddr_hashtbl[targetIp % 509U];
            while (ia != NULL) {
                if (ia->ia_ifa.ifa_ifp == ifp && ia->ia_addr.sin_addr == targetIp) {
                    break;
                }
                ia = ia->ia_hash_next;
            }
        }
        if (cmd == 0x80206919U) {
            if (ia == NULL) {
                return INET_EADDRNOTAVAIL;
            }
            if (ifr->ifr_u.ifru_addr.sa_family == INET_AF_UNSPEC) {
                ifr->ifr_u.ifru_addr.sa_family = INET_AF_INET;
            }
        }
        if (ifr->ifr_u.ifru_addr.sa_family != INET_AF_INET) {
            return INET_EAFNOSUPPORT;
        }
        if (cmd != 0xC040691BU && ia == NULL) {
            ia = (InetInIfaddr *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetInIfaddr));
            if (ia == NULL) {
                return INET_ENOBUFS;
            }
            sceNet_lib_8B615F52(ia, sizeof(InetInIfaddr));

            ia->ia_list_next = NULL;
            ia->ia_list_prev = g_in_ifaddr_tail;
            *g_in_ifaddr_tail = ia;
            g_in_ifaddr_tail = &ia->ia_list_next;

            ia->ia_ifa.ifa_next = NULL;
            ia->ia_ifa.ifa_prev = ifp->if_addrtail;
            *ifp->if_addrtail = &ia->ia_ifa;
            ifp->if_addrtail = &ia->ia_ifa.ifa_next;

            ia->ia_ifa.ifa_addr = (InetSockaddr *)&ia->ia_addr;
            ia->ia_ifa.ifa_dstaddr = (InetSockaddr *)&ia->ia_dstaddr;
            ia->ia_ifa.ifa_netmask = (InetSockaddr *)&ia->ia_sockmask;
            ia->ia_sockmask.sin_len = 8;
            if ((ifp->if_flags & INET_IFF_BROADCAST) != 0) {
                ia->ia_dstaddr.sin_len = sizeof(InetSockaddrIn);
                ia->ia_dstaddr.sin_family = INET_AF_INET;
            }
            ia->ia_ifa.ifa_ifp = ifp;
            ia->ia_ifa.ifa_refcnt += 2;
            ia->ia_multiaddrs = NULL;
        }
        break;

    case 0x8020690CU: /* SIOCSIFADDR */
    case 0x8020690EU: /* SIOCSIFDSTADDR */
        if (ifr->ifr_u.ifru_addr.sa_family != INET_AF_INET) {
            return INET_EAFNOSUPPORT;
        }
        /* fall through */
    case 0x80206916U: /* SIOCSIFNETMASK */
        if (ia == NULL) {
            ia = (InetInIfaddr *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetInIfaddr));
            if (ia == NULL) {
                return INET_ENOBUFS;
            }
            sceNet_lib_8B615F52(ia, sizeof(InetInIfaddr));

            ia->ia_list_next = NULL;
            ia->ia_list_prev = g_in_ifaddr_tail;
            *g_in_ifaddr_tail = ia;
            g_in_ifaddr_tail = &ia->ia_list_next;

            ia->ia_ifa.ifa_next = NULL;
            ia->ia_ifa.ifa_prev = ifp->if_addrtail;
            *ifp->if_addrtail = &ia->ia_ifa;
            ifp->if_addrtail = &ia->ia_ifa.ifa_next;

            ia->ia_ifa.ifa_addr = (InetSockaddr *)&ia->ia_addr;
            ia->ia_ifa.ifa_dstaddr = (InetSockaddr *)&ia->ia_dstaddr;
            ia->ia_ifa.ifa_netmask = (InetSockaddr *)&ia->ia_sockmask;
            ia->ia_sockmask.sin_len = 8;
            if ((ifp->if_flags & INET_IFF_BROADCAST) != 0) {
                ia->ia_dstaddr.sin_len = sizeof(InetSockaddrIn);
                ia->ia_dstaddr.sin_family = INET_AF_INET;
            }
            ia->ia_ifa.ifa_ifp = ifp;
            ia->ia_ifa.ifa_refcnt += 2;
            ia->ia_multiaddrs = NULL;
        }
        break;

    case 0x80206913U: /* SIOCSIFBRDADDR */
    case 0xC0206921U: /* SIOCGIFADDR */
    case 0xC0206922U: /* SIOCGIFDSTADDR */
    case 0xC0206923U: /* SIOCGIFBRDADDR */
    case 0xC0206925U: /* SIOCGIFNETMASK */
        if (ia == NULL) {
            return INET_EADDRNOTAVAIL;
        }
        break;

    default:
        break;
    }

    switch (cmd) {
    case 0x8040691AU: { /* SIOCAIFADDR */
        int hostIsNew = 1;
        int maskIsNew = 0;
        int err = 0;

        if (ia->ia_addr.sin_family == INET_AF_INET) {
            if (ifra->ifra_addr.sin_len == 0) {
                ifra->ifra_addr = ia->ia_addr;
                hostIsNew = 0;
            } else if (ifra->ifra_addr.sin_addr == ia->ia_addr.sin_addr) {
                hostIsNew = 0;
            }
        }
        if (ifra->ifra_mask.sin_len != 0) {
            inetInIfScrub(ifp, ia);
            ia->ia_sockmask = ifra->ifra_mask;
            ia->ia_subnetmask = ia->ia_sockmask.sin_addr;
            maskIsNew = 1;
        }
        if ((ifp->if_flags & INET_IFF_POINTOPOINT) != 0 &&
            ifra->ifra_broadaddr.sin_family == INET_AF_INET) {
            inetInIfScrub(ifp, ia);
            ia->ia_dstaddr = ifra->ifra_broadaddr;
            maskIsNew = 1;
        }
        if (ifra->ifra_addr.sin_family == INET_AF_INET && (hostIsNew || maskIsNew)) {
            err = inetInIfInit(ifp, ia, &ifra->ifra_addr, 0);
        }
        if ((ifp->if_flags & INET_IFF_BROADCAST) != 0 &&
            ifra->ifra_broadaddr.sin_family == INET_AF_INET) {
            ia->ia_dstaddr = ifra->ifra_broadaddr;
        }
        return err;
    }

    case 0xC0206921U: /* SIOCGIFADDR */
        ifr->ifr_u.ifru_sin = ia->ia_addr;
        return 0;

    case 0xC0206922U: /* SIOCGIFDSTADDR */
        if ((ifp->if_flags & INET_IFF_POINTOPOINT) == 0) {
            return INET_EINVAL;
        }
        ifr->ifr_u.ifru_sin = ia->ia_dstaddr;
        return 0;

    case 0xC0206923U: /* SIOCGIFBRDADDR */
        if ((ifp->if_flags & INET_IFF_BROADCAST) == 0) {
            return INET_EINVAL;
        }
        ifr->ifr_u.ifru_sin = ia->ia_dstaddr;
        return 0;

    case 0xC0206925U: /* SIOCGIFNETMASK */
        ifr->ifr_u.ifru_sin = ia->ia_sockmask;
        return 0;

    case 0xC040691BU: /* SIOCGIFALIAS */
        ifra->ifra_mask = ia->ia_sockmask;
        if (((ifp->if_flags & INET_IFF_POINTOPOINT) != 0 ||
             (ifp->if_flags & INET_IFF_BROADCAST) != 0) &&
            ia->ia_dstaddr.sin_family == INET_AF_INET) {
            ifra->ifra_broadaddr = ia->ia_dstaddr;
        } else {
            sceNet_lib_8B615F52(&ifra->ifra_broadaddr, sizeof(InetSockaddrIn));
        }
        return 0;

    case 0x8020690CU: /* SIOCSIFADDR */
        return inetInIfInit(ifp, ia, &ifr->ifr_u.ifru_sin, 1);

    case 0x8020690EU: { /* SIOCSIFDSTADDR */
        InetSockaddrIn oldDst;
        if ((ifp->if_flags & INET_IFF_POINTOPOINT) == 0) {
            return INET_EINVAL;
        }
        oldDst = ia->ia_dstaddr;
        ia->ia_dstaddr = ifr->ifr_u.ifru_sin;
        if (ifp->if_ioctl != NULL) {
            int err = ifp->if_ioctl(ifp, 0x8020690EU, ia);
            if (err != 0) {
                ia->ia_dstaddr = oldDst;
                return err;
            }
        }
        if ((ia->ia_ifa.ifa_flags & 1) != 0) {
            ia->ia_ifa.ifa_dstaddr = (InetSockaddr *)&oldDst;
            inetRtInit(&ia->ia_ifa, INET_RTM_DELETE, INET_RTF_HOST);
            ia->ia_ifa.ifa_dstaddr = (InetSockaddr *)&ia->ia_dstaddr;
            inetRtInit(&ia->ia_ifa, INET_RTM_ADD, INET_RTF_HOST | INET_RTF_UP);
        }
        return 0;
    }

    case 0x80206913U: /* SIOCSIFBRDADDR */
        if ((ifp->if_flags & INET_IFF_BROADCAST) == 0) {
            return INET_EINVAL;
        }
        ia->ia_dstaddr = ifr->ifr_u.ifru_sin;
        return 0;

    case 0x80206916U: /* SIOCSIFNETMASK */
        ia->ia_subnetmask = ifr->ifr_u.ifru_sin.sin_addr;
        ia->ia_sockmask.sin_addr = ifr->ifr_u.ifru_sin.sin_addr;
        return 0;

    case 0x80206919U: /* SIOCDIFADDR */
        inetInPurgeAddr(&ia->ia_ifa, ifp);
        return 0;

    default:
        if (ifp == NULL || ifp->if_ioctl == NULL) {
            return INET_EOPNOTSUPP;
        }
        {
            int err = ifp->if_ioctl(ifp, cmd, data);
            inetInSetMaxMtu();
            return err;
        }
    }
}

/* [235] 0x013328 */
INET_NOINLINE void inetInPurgeAddr(InetIfaddr *ifa, InetIfnet *ifp)
{
    InetInIfaddr *ia = (InetInIfaddr *)ifa;

    inetInIfScrub(ifp, ia);
    if (ia->ia_hash_next != NULL) {
        ia->ia_hash_next->ia_hash_prev = ia->ia_hash_prev;
    }
    *ia->ia_hash_prev = ia->ia_hash_next;

    if (ia->ia_ifa.ifa_next != NULL) {
        ia->ia_ifa.ifa_next->ifa_prev = ia->ia_ifa.ifa_prev;
    } else {
        ifp->if_addrtail = ia->ia_ifa.ifa_prev;
    }
    *ia->ia_ifa.ifa_prev = ia->ia_ifa.ifa_next;

    ia->ia_ifa.ifa_refcnt--;
    if (ia->ia_ifa.ifa_refcnt == 0) {
        sceNet_lib_EDCC871E(ia);
    }

    if (ia->ia_list_next != NULL) {
        ia->ia_list_next->ia_list_prev = ia->ia_list_prev;
    } else {
        g_in_ifaddr_tail = ia->ia_list_prev;
    }
    *ia->ia_list_prev = ia->ia_list_next;

    if (ia->ia_allhosts != NULL) {
        inetInDelMulti(ia->ia_allhosts);
    }
    if (ia->ia_multiaddrs != NULL &&
        ifp->if_output != (int (*)(InetIfnet *, InetMbuf *, InetSockaddr *, void *))sceNet_lib_8C55B410) {
        inetInSaveMulti(ia);
    }

    ia->ia_ifa.ifa_refcnt--;
    if (ia->ia_ifa.ifa_refcnt == 0) {
        sceNet_lib_EDCC871E(ia);
    }
    inetInSetMaxMtu();
}

/* [236] 0x0134D0 */
static INET_NOINLINE int inetInLifaddrIoctl(InetSocket *so, u32 cmd, void *data, InetIfnet *ifp)
{
    InetIfLaddrReq *iflr = (InetIfLaddrReq *)data;
    InetSockaddrIn *addrSin = (InetSockaddrIn *)iflr->addr;
    InetSockaddrIn *dstSin = (InetSockaddrIn *)iflr->dstaddr;
    u32 maskBits = 0;
    u32 matchAddr = 0;
    int matchPrefix = 0;
    InetIfaddr *ifa;

    if (cmd == 0x8118691CU || cmd == 0x8118691EU ||
        (cmd == 0xC118691DU && (iflr->flags & 0x8000) != 0)) {
        if (addrSin->sin_family != INET_AF_INET || addrSin->sin_len != 16 ||
            (dstSin->sin_family != 0 && dstSin->sin_family != INET_AF_INET) ||
            (dstSin->sin_len != 0 && dstSin->sin_len != 16)) {
            return INET_EINVAL;
        }
    } else if (cmd != 0xC118691DU) {
        return INET_EOPNOTSUPP;
    }

    if (iflr->prefixlen > 32) {
        return INET_EINVAL;
    }

    if (cmd == 0x8118691CU) { /* SIOCALIFADDR */
        InetIfAliasReq ifra;
        u8 *maskBytes = (u8 *)&ifra.ifra_mask.sin_addr;
        int idx;
        if ((iflr->flags & 0x8000) != 0) {
            return INET_EINVAL;
        }
        sceNet_lib_8B615F52(&ifra, sizeof(ifra));
        sceNet_lib_2F305274(iflr->iflr_name, ifra.ifra_name, 16);
        sceNet_lib_2F305274(addrSin, &ifra.ifra_addr, addrSin->sin_len);
        if (dstSin->sin_family != 0) {
            sceNet_lib_2F305274(dstSin, &ifra.ifra_broadaddr, dstSin->sin_len);
        }
        ifra.ifra_mask.sin_len = 16;
        ifra.ifra_mask.sin_family = INET_AF_INET;
        sceNet_lib_8B615F52(maskBytes, 4);
        for (idx = 0; idx < (int)(iflr->prefixlen / 8); idx++) {
            maskBytes[idx] = 0xFF;
        }
        if ((iflr->prefixlen & 7) != 0) {
            maskBytes[idx] = (u8)(0xFF00U >> (iflr->prefixlen % 8));
        }
        return inetInControl(so, 0x8040691AU, &ifra, ifp);
    }

    if (cmd != 0x8118691EU && cmd != 0xC118691DU) {
        return INET_EOPNOTSUPP;
    }

    if ((iflr->flags & 0x8000) != 0) {
        u8 *mb = (u8 *)&maskBits;
        int idx;
        for (idx = 0; idx < (int)(iflr->prefixlen / 8); idx++) {
            mb[idx] = 0xFF;
        }
        if ((iflr->prefixlen & 7) != 0) {
            mb[idx] = (u8)(0xFF00U >> (iflr->prefixlen % 8));
        }
        matchAddr = addrSin->sin_addr & maskBits;
        if (matchAddr != addrSin->sin_addr) {
            return INET_EINVAL;
        }
        matchPrefix = 1;
    } else if (cmd != 0xC118691DU) {
        maskBits = 0xFFFFFFFFU;
        matchAddr = addrSin->sin_addr;
        matchPrefix = 1;
    }

    for (ifa = ifp->if_addrhead; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family == 24) {
            if (!matchPrefix || ((((InetSockaddrIn *)ifa->ifa_dstaddr)->sin_addr & maskBits) == matchAddr)) {
                break;
            }
        }
    }
    if (ifa == NULL) {
        return INET_EADDRNOTAVAIL;
    }

    {
        InetInIfaddr *ia = (InetInIfaddr *)ifa;
        if (cmd != 0xC118691DU) {
            InetIfAliasReq ifra;
            sceNet_lib_8B615F52(&ifra, sizeof(ifra));
            sceNet_lib_2F305274(iflr->iflr_name, ifra.ifra_name, 16);
            sceNet_lib_2F305274(&ia->ia_addr, &ifra.ifra_addr, ia->ia_addr.sin_len);
            if ((ifp->if_flags & INET_IFF_POINTOPOINT) != 0) {
                sceNet_lib_2F305274(&ia->ia_dstaddr, &ifra.ifra_broadaddr, ia->ia_dstaddr.sin_len);
            }
            sceNet_lib_2F305274(&ia->ia_sockmask, &ifra.ifra_broadaddr, ia->ia_sockmask.sin_len);
            return inetInControl(so, 0x80206919U, &ifra, ifp);
        }

        sceNet_lib_2F305274(&ia->ia_addr, addrSin, ia->ia_addr.sin_len);
        if ((ifp->if_flags & INET_IFF_POINTOPOINT) != 0) {
            sceNet_lib_2F305274(&ia->ia_dstaddr, dstSin, ia->ia_dstaddr.sin_len);
        } else {
            sceNet_lib_8B615F52(dstSin, 128);
        }
        {
            const u8 *mb = (const u8 *)&ia->ia_sockmask.sin_addr;
            int byteIdx = 0;
            int bitIdx = 0;
            while (byteIdx < 4 && mb[byteIdx] == 0xFF) {
                byteIdx++;
            }
            if (byteIdx < 4) {
                while (bitIdx < 8 && (mb[byteIdx] & (0x80 >> bitIdx)) != 0) {
                    bitIdx++;
                }
            }
            iflr->prefixlen = (u32)(8 * byteIdx + bitIdx);
            iflr->flags = 0;
        }
        return 0;
    }
}

/* [237] 0x0139B4 */
INET_NOINLINE void inetInIfScrub(InetIfnet *ifp, InetInIfaddr *ia)
{
    if ((ia->ia_ifa.ifa_flags & 1) != 0) {
        int rtFlags = ((ifp->if_flags & (INET_IFF_LOOPBACK | INET_IFF_POINTOPOINT)) != 0) ?
            INET_RTF_HOST : 0;
        inetRtInit(&ia->ia_ifa, INET_RTM_DELETE, rtFlags);
        ia->ia_ifa.ifa_flags &= ~1U;
    }
}

/* [238] 0x013A1C */
INET_NOINLINE int inetInIfInit(InetIfnet *ifp, InetInIfaddr *ia,
                               const InetSockaddrIn *sin, int scrub)
{
    u32 ipAddr = sin->sin_addr;
    InetSockaddrIn oldAddr;
    int spl = sceNet_lib_94B44F26();
    int flags = INET_RTF_UP;
    int err;

    oldAddr = ia->ia_addr;
    if (ia->ia_addr.sin_family == INET_AF_INET) {
        if (ia->ia_hash_next != NULL) {
            ia->ia_hash_next->ia_hash_prev = ia->ia_hash_prev;
        }
        *ia->ia_hash_prev = ia->ia_hash_next;
    }

    ia->ia_addr = *sin;
    {
        InetInIfaddr **bucket = &g_in_ifaddr_hashtbl[ia->ia_addr.sin_addr % 509U];
        ia->ia_hash_next = *bucket;
        if (*bucket != NULL) {
            (*bucket)->ia_hash_prev = &ia->ia_hash_next;
        }
        ia->ia_hash_prev = bucket;
        *bucket = ia;
    }

    if (ifp->if_ioctl != NULL) {
        err = ifp->if_ioctl(ifp, 0x8020690CU, ia);
        if (err != 0) {
            sceNet_lib_515B2F33(spl);
            if (ia->ia_hash_next != NULL) {
                ia->ia_hash_next->ia_hash_prev = ia->ia_hash_prev;
            }
            *ia->ia_hash_prev = ia->ia_hash_next;
            ia->ia_addr = oldAddr;
            if (ia->ia_addr.sin_family == INET_AF_INET) {
                InetInIfaddr **bucket = &g_in_ifaddr_hashtbl[ia->ia_addr.sin_addr % 509U];
                ia->ia_hash_next = *bucket;
                if (*bucket != NULL) {
                    (*bucket)->ia_hash_prev = &ia->ia_hash_next;
                }
                ia->ia_hash_prev = bucket;
                *bucket = ia;
            }
            return err;
        }
    }
    sceNet_lib_515B2F33(spl);

    if (scrub != 0) {
        ia->ia_ifa.ifa_addr = (InetSockaddr *)&oldAddr;
        inetInIfScrub(ifp, ia);
        ia->ia_ifa.ifa_addr = (InetSockaddr *)&ia->ia_addr;
    }

    if ((ipAddr & inetBswap32(0x80000000U)) == 0) {
        ia->ia_netmask = inetBswap32(0xFF000000U);
    } else if ((ipAddr & inetBswap32(0xC0000000U)) == inetBswap32(0x80000000U)) {
        ia->ia_netmask = inetBswap32(0xFFFF0000U);
    } else {
        ia->ia_netmask = inetBswap32(0xFFFFFF00U);
    }

    if (ia->ia_subnetmask == 0) {
        ia->ia_subnetmask = ia->ia_netmask;
        ia->ia_sockmask.sin_addr = ia->ia_netmask;
    } else {
        ia->ia_netmask &= ia->ia_subnetmask;
    }
    ia->ia_net = ipAddr & ia->ia_netmask;
    ia->ia_subnet = ipAddr & ia->ia_subnetmask;

    {
        const u8 *maskBase = (const u8 *)&ia->ia_sockmask;
        const u8 *scan = (const u8 *)&ia->ia_sockmask.sin_addr + 3;
        ia->ia_sockmask.sin_len = 0;
        while (scan >= (const u8 *)&ia->ia_sockmask.sin_addr) {
            if (*scan != 0) {
                ia->ia_sockmask.sin_len = (u8)(scan - maskBase + 1);
                break;
            }
            scan--;
        }
    }

    inetInSetMaxMtu();
    ia->ia_ifa.ifa_metric = (s32)ifp->if_data.ifi_metric;

    if ((ifp->if_flags & INET_IFF_BROADCAST) != 0) {
        ia->ia_dstaddr.sin_addr = ia->ia_subnet | ~ia->ia_subnetmask;
        ia->ia_netbroadcast = ia->ia_net | ~ia->ia_netmask;
    } else if ((ifp->if_flags & INET_IFF_LOOPBACK) != 0) {
        ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
        flags |= INET_RTF_HOST;
    } else if ((ifp->if_flags & INET_IFF_POINTOPOINT) != 0) {
        if (ia->ia_dstaddr.sin_family != INET_AF_INET) {
            return 0;
        }
        flags |= INET_RTF_HOST;
    }

    err = inetRtInit(&ia->ia_ifa, INET_RTM_ADD, flags);
    if (err == 0) {
        ia->ia_ifa.ifa_flags |= 1;
    }
    if (err == INET_EEXIST) {
        err = 0;
    }

    if ((ifp->if_flags & INET_IFF_MULTICAST) != 0) {
        inetInRestoreMulti(ia, ifp);
        if (ia->ia_allhosts == NULL) {
            u32 allHosts = inetBswap32(0xE0000001U);
            ia->ia_allhosts = inetInAddMulti(&allHosts, ifp);
        }
    }
    return err;
}

/* [239] 0x013EE0 */
INET_NOINLINE void inetInSaveMulti(InetInIfaddr *ia)
{
    InetInIfaddr *target = inetIfpFirstInAddr(ia->ia_ifa.ifa_ifp);
    if (target == NULL) {
        ia->ia_ifa.ifa_refcnt++;
        ia->ia_list_next = NULL;
        ia->ia_list_prev = g_in_mkopt_tail;
        *g_in_mkopt_tail = ia;
        g_in_mkopt_tail = &ia->ia_list_next;
        return;
    }
    {
        InetInMulti *inm = ia->ia_multiaddrs;
        while (inm != NULL) {
            InetInMulti *next = inm->inm_next;
            InetInIfaddr *oldIa = inm->inm_ia;
            oldIa->ia_ifa.ifa_refcnt--;
            if (oldIa->ia_ifa.ifa_refcnt == 0) {
                sceNet_lib_EDCC871E(oldIa);
            }
            inm->inm_ia = target;
            target->ia_ifa.ifa_refcnt++;
            inm->inm_next = target->ia_multiaddrs;
            if (target->ia_multiaddrs != NULL) {
                target->ia_multiaddrs->inm_prev = &inm->inm_next;
            }
            inm->inm_prev = &target->ia_multiaddrs;
            target->ia_multiaddrs = inm;
            inm = next;
        }
    }
}

/* [240] 0x014004 */
INET_NOINLINE void inetInRestoreMulti(InetInIfaddr *ia, InetIfnet *ifp)
{
    InetInIfaddr *orphan = g_in_mkopt_head;
    while (orphan != NULL && orphan->ia_ifa.ifa_ifp != ifp) {
        orphan = orphan->ia_list_next;
    }
    if (orphan == NULL) {
        return;
    }

    {
        InetInMulti *inm = orphan->ia_multiaddrs;
        while (inm != NULL) {
            InetInMulti *next = inm->inm_next;
            InetInIfaddr *oldIa = inm->inm_ia;
            oldIa->ia_ifa.ifa_refcnt--;
            if (oldIa->ia_ifa.ifa_refcnt == 0) {
                sceNet_lib_EDCC871E(oldIa);
            }
            inm->inm_ia = ia;
            ia->ia_ifa.ifa_refcnt++;
            inm->inm_next = ia->ia_multiaddrs;
            if (ia->ia_multiaddrs != NULL) {
                ia->ia_multiaddrs->inm_prev = &inm->inm_next;
            }
            inm->inm_prev = &ia->ia_multiaddrs;
            ia->ia_multiaddrs = inm;
            inm = next;
        }
    }

    if (orphan->ia_list_next != NULL) {
        orphan->ia_list_next->ia_list_prev = orphan->ia_list_prev;
    } else {
        g_in_mkopt_tail = orphan->ia_list_prev;
    }
    *orphan->ia_list_prev = orphan->ia_list_next;

    orphan->ia_ifa.ifa_refcnt--;
    if (orphan->ia_ifa.ifa_refcnt == 0) {
        sceNet_lib_EDCC871E(orphan);
    }
}

/* [241] 0x014128 */
INET_NOINLINE InetInMulti *inetInAddMulti(const u32 *addr, InetIfnet *ifp)
{
    int spl = sceNet_lib_94B44F26();
    InetInIfaddr *ia = inetIfpFirstInAddr(ifp);
    InetInMulti *inm;
    InetIfreq ifr;

    if (ia != NULL) {
        for (inm = ia->ia_multiaddrs; inm != NULL; inm = inm->inm_next) {
            if (inm->inm_addr == *addr) {
                inm->inm_refcount++;
                sceNet_lib_515B2F33(spl);
                return inm;
            }
        }
    }

    inm = (InetInMulti *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetInMulti));
    if (inm == NULL) {
        sceNet_lib_515B2F33(spl);
        return NULL;
    }
    inm->inm_addr = *addr;
    sceNet_lib_B5CE388A(inm->inm_ifname, ifp->if_xname, 16);
    inm->inm_ifname[15] = '\0';
    inm->inm_refcount = 1;

    ia = inetIfpFirstInAddr(ifp);
    if (ia == NULL) {
        sceNetIfhandle_lib_76BAD213(inm);
        sceNet_lib_515B2F33(spl);
        return NULL;
    }
    inm->inm_ia = ia;
    ia->ia_ifa.ifa_refcnt++;
    inm->inm_next = ia->ia_multiaddrs;
    if (ia->ia_multiaddrs != NULL) {
        ia->ia_multiaddrs->inm_prev = &inm->inm_next;
    }
    inm->inm_prev = &ia->ia_multiaddrs;
    ia->ia_multiaddrs = inm;

    ifr.ifr_u.ifru_sin.sin_len = sizeof(InetSockaddrIn);
    ifr.ifr_u.ifru_sin.sin_family = INET_AF_INET;
    ifr.ifr_u.ifru_sin.sin_addr = *addr;
    if (sceNet_lib_E9D457B6(ifp, 0x80206931U, &ifr) < 0) {
        if (inm->inm_next != NULL) {
            inm->inm_next->inm_prev = inm->inm_prev;
        }
        *inm->inm_prev = inm->inm_next;
        sceNetIfhandle_lib_76BAD213(inm);
        sceNet_lib_515B2F33(spl);
        return NULL;
    }

    inetIgmpJoingroup(inm);
    sceNet_lib_515B2F33(spl);
    return inm;
}

/* [242] 0x014344 */
INET_NOINLINE void inetInDelMulti(InetInMulti *inm)
{
    int spl = sceNet_lib_94B44F26();
    InetIfnet *ifp = sceNet_lib_1D10419C(inm->inm_ifname);

    inm->inm_refcount--;
    if (inm->inm_refcount == 0) {
        InetIfreq ifr;
        InetInIfaddr *ia;

        inetIgmpLeavegroup(inm);
        if (inm->inm_next != NULL) {
            inm->inm_next->inm_prev = inm->inm_prev;
        }
        *inm->inm_prev = inm->inm_next;

        ia = inm->inm_ia;
        ia->ia_ifa.ifa_refcnt--;
        if (ia->ia_ifa.ifa_refcnt == 0) {
            sceNet_lib_EDCC871E(ia);
        }

        ifr.ifr_u.ifru_sin.sin_family = INET_AF_INET;
        ifr.ifr_u.ifru_sin.sin_addr = inm->inm_addr;
        if (ifp != NULL) {
            sceNet_lib_E9D457B6(ifp, 0x80206932U, &ifr);
        }
        sceNetIfhandle_lib_76BAD213(inm);
    }
    sceNet_lib_515B2F33(spl);
}

/* [243] 0x014434 */
INET_NOINLINE int inetInLocalAddr(u32 addr)
{
    InetInIfaddr *ia = g_in_ifaddr_head;
    if (g_subnetsarelocal != 0) {
        while (ia != NULL) {
            if ((addr & ia->ia_netmask) == ia->ia_net) {
                return 1;
            }
            ia = ia->ia_list_next;
        }
    } else {
        while (ia != NULL) {
            if ((addr & ia->ia_subnetmask) == ia->ia_subnet) {
                return 1;
            }
            ia = ia->ia_list_next;
        }
    }
    return 0;
}

/* [244] 0x0144C8 */
INET_NOINLINE int inetInCanForward(u32 addr)
{
    u32 classD = inetBswap32(0xE0000000U);
    u32 classMask = inetBswap32(0xF0000000U);
    u32 topNet;

    if ((addr & classMask) == classMask || (addr & classMask) == classD) {
        return 0;
    }
    if ((addr & inetBswap32(0x80000000U)) == 0) {
        topNet = addr & inetBswap32(0xFF000000U);
        if (topNet == 0 || topNet == 127) {
            return 0;
        }
    }
    return 1;
}

/* [245] 0x014544 */
INET_NOINLINE int inetInGetIfaddrInfo(const char *ifname, u32 *addr, u32 *dstAddr, u32 *mask)
{
    InetIfnet *ifp = sceNet_lib_1D10419C(ifname);
    InetInIfaddr *ia;

    if (ifp == NULL) {
        return INET_ENXIO;
    }
    ia = inetIfpFirstInAddr(ifp);
    if (ia == NULL) {
        return INET_EADDRNOTAVAIL;
    }
    if (addr != NULL) {
        *addr = ia->ia_addr.sin_addr;
    }
    if (dstAddr != NULL && (ifp->if_flags & INET_IFF_BROADCAST) != 0) {
        *dstAddr = ia->ia_dstaddr.sin_addr;
    }
    if (mask != NULL) {
        *mask = ia->ia_sockmask.sin_addr;
    }
    return 0;
}

/* [246] 0x01465C */
INET_NOINLINE void inetInPurgeIf(InetIfnet *ifp)
{
    InetIfaddr *ifa = ifp->if_addrhead;
    InetInIfaddr *orphan;

    while (ifa != NULL) {
        InetIfaddr *next = ifa->ifa_next;
        if (ifa->ifa_addr->sa_family == INET_AF_INET) {
            inetInPurgeAddr(ifa, ifp);
        }
        ifa = next;
    }

    orphan = g_in_mkopt_head;
    while (orphan != NULL && orphan->ia_ifa.ifa_ifp != ifp) {
        orphan = orphan->ia_list_next;
    }
    if (orphan != NULL) {
        if (orphan->ia_list_next != NULL) {
            orphan->ia_list_next->ia_list_prev = orphan->ia_list_prev;
        } else {
            g_in_mkopt_tail = orphan->ia_list_prev;
        }
        *orphan->ia_list_prev = orphan->ia_list_next;
        orphan->ia_ifa.ifa_refcnt--;
        if (orphan->ia_ifa.ifa_refcnt == 0) {
            sceNet_lib_EDCC871E(orphan);
        }
    }
}

/* [247] 0x014734 */
INET_NOINLINE int inetInBroadcast(u32 addr, InetIfnet *ifp)
{
    if (addr == 0xFFFFFFFFU || addr == 0) {
        return 1;
    }
    if ((ifp->if_flags & INET_IFF_BROADCAST) == 0) {
        return 0;
    }
    {
        InetIfaddr *ifa = ifp->if_addrhead;
        while (ifa != NULL) {
            if (ifa->ifa_addr->sa_family == INET_AF_INET) {
                InetInIfaddr *ia = (InetInIfaddr *)ifa;
                if (addr != ia->ia_addr.sin_addr) {
                    if (addr == ia->ia_dstaddr.sin_addr || addr == ia->ia_netbroadcast) {
                        return 1;
                    }
                    if (g_hostzeroisbroadcast != 0 &&
                        (addr == ia->ia_subnet || addr == ia->ia_net)) {
                        return 1;
                    }
                }
            }
            ifa = ifa->ifa_next;
        }
    }
    return 0;
}

/* [248] 0x0147F8 */
INET_NOINLINE void inetInInit(void)
{
    g_hostzeroisbroadcast = 1;
    g_subnetsarelocal = 1;
    g_in_maxmtu = 0;
    g_in_mkopt_head = NULL;
    g_in_mkopt_tail = &g_in_mkopt_head;
}

static u16 inetCksumChain(InetMbuf *m, s32 off, s32 len, u32 initSum)
{
    u32 sum = initSum;
    u32 oddByte = 0;
    int pendingByte = 0;
    int byteSwapped = 0;

    while (m != NULL && off > 0 && len > 0) {
        if (off < m->m_len) {
            break;
        }
        off -= m->m_len;
        m = m->m_next;
    }

    while (m != NULL && len > 0) {
        if (m->m_len != 0) {
            const u8 *ptr = m->m_data + off;
            s32 mlen = m->m_len - off;
            off = 0;
            if (pendingByte) {
                oddByte = (oddByte & 0x00FFU) | ((u32)(*ptr++) << 8);
                sum += oddByte;
                mlen--;
                len--;
                pendingByte = 0;
            }
            if (mlen > len) {
                mlen = len;
            }
            len -= mlen;

            if (((uintptr_t)ptr & 1) != 0 && mlen > 0) {
                sum = (sum & 0xFFFFU) + (sum >> 16);
                if (sum > 0xFFFFU) {
                    sum -= 0xFFFFU;
                }
                sum <<= 8;
                oddByte = *ptr++;
                mlen--;
                byteSwapped = 1;
            }

            {
                const u16 *w = (const u16 *)ptr;
                while (mlen >= 32) {
                    sum += w[0] + w[1] + w[2] + w[3] + w[4] + w[5] + w[6] + w[7] +
                           w[8] + w[9] + w[10] + w[11] + w[12] + w[13] + w[14] + w[15];
                    w += 16;
                    mlen -= 32;
                }
                while (mlen >= 8) {
                    sum += w[0] + w[1] + w[2] + w[3];
                    w += 4;
                    mlen -= 8;
                }
                if (mlen != 0 || byteSwapped != 0) {
                    sum = (sum & 0xFFFFU) + (sum >> 16);
                    if (sum > 0xFFFFU) {
                        sum -= 0xFFFFU;
                    }
                    while (mlen >= 2) {
                        sum += *w++;
                        mlen -= 2;
                    }
                    if (byteSwapped) {
                        sum = (sum & 0xFFFFU) + (sum >> 16);
                        if (sum > 0xFFFFU) {
                            sum -= 0xFFFFU;
                        }
                        sum <<= 8;
                        byteSwapped = 0;
                        if (mlen == 1) {
                            oddByte = (oddByte & 0x00FFU) | ((u32)(*(const u8 *)w) << 8);
                            sum += oddByte;
                            pendingByte = 0;
                        } else {
                            pendingByte = 1;
                        }
                    } else if (mlen == 1) {
                        oddByte = *(const u8 *)w;
                        pendingByte = 1;
                    }
                }
            }
        }
        m = m->m_next;
    }

    if (pendingByte) {
        sum += (oddByte & 0x00FFU);
    }
    sum = (sum & 0xFFFFU) + (sum >> 16);
    if (sum > 0xFFFFU) {
        sum++;
    }
    return (u16)(~sum);
}

/* [249] 0x014828 */
INET_NOINLINE u16 inetIn4Cksum(InetMbuf *m, u8 proto, s32 off, s32 len)
{
    u32 initSum = 0;
    if (proto != 0) {
        u16 pseudo[10];
        InetIpHdr *ip = (InetIpHdr *)m->m_data;
        sceNet_lib_8B615F52(pseudo, sizeof(pseudo));
        pseudo[4] = (u16)((u16)proto << 8);
        pseudo[5] = inetBswap16((u16)len);
        *(u32 *)&pseudo[6] = ip->ip_src;
        *(u32 *)&pseudo[8] = ip->ip_dst;
        initSum = pseudo[0] + pseudo[1] + pseudo[2] + pseudo[3] + pseudo[4] +
                  pseudo[5] + pseudo[6] + pseudo[7] + pseudo[8] + pseudo[9];
    }
    return inetCksumChain(m, off, len, initSum);
}

/* [250] 0x014BD4 */
INET_NOINLINE u16 inetInCksum(InetMbuf *m, s32 len)
{
    return inetCksumChain(m, 0, len, 0);
}

/* [251] 0x014E54 */
INET_NOINLINE int inetInPcbBind(InetInpcbReal *inp, InetMbuf *nam)
{
    InetInpcbTable *table = inp->inp_table;
    u16 soOpts = inp->inp_socket->so_options;
    int reusePort = (soOpts & INET_SO_REUSEPORT);
    int wild;

    if (g_in_ifaddr_head == NULL) {
        return INET_EADDRNOTAVAIL;
    }
    if (inp->inp_lport != 0 || inp->inp_laddr != 0) {
        return INET_EINVAL;
    }
    wild = ((soOpts & (INET_SO_REUSEADDR | INET_SO_REUSEPORT)) == 0) ? 1 : 0;

    if (nam != NULL) {
        InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
        u16 port;
        if (nam->m_len != sizeof(InetSockaddrIn)) {
            return INET_EINVAL;
        }
        port = sin->sin_port;
        if ((sin->sin_addr & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U)) {
            if ((soOpts & INET_SO_REUSEADDR) != 0) {
                reusePort = INET_SO_REUSEADDR | INET_SO_REUSEPORT;
            }
        } else if (sin->sin_addr != 0) {
            sin->sin_port = 0;
            if (inetIfaWithAddr((InetSockaddr *)sin) == NULL) {
                return INET_EADDRNOTAVAIL;
            }
        }
        if (port != 0) {
            InetInpcbReal *conflict = inetInPcbLookupPort(table, sin->sin_addr, port, wild);
            if (conflict != NULL && (reusePort & conflict->inp_socket->so_options) == 0) {
                return INET_EADDRINUSE;
            }
        }
        inp->inp_laddr = sin->sin_addr;
        if (port != 0) {
            inp->inp_lport = port;
            inetInPcbState(inp, 1);
            return 0;
        }
    }

    {
        u32 lowPort;
        u32 highPort;
        u16 *lastPortPtr;
        u32 count;
        u16 cand;

        if ((inp->inp_flags & 0x20) != 0) {
            lowPort = (u16)g_lowportmin;
            highPort = (u16)g_lowportmax;
            lastPortPtr = &table->inpt_lastlow;
        } else {
            lowPort = (u16)g_anonportmin;
            highPort = (u16)g_anonportmax;
            lastPortPtr = &table->inpt_lastport;
        }
        if (highPort < lowPort) {
            u32 swapTmp = lowPort;
            lowPort = highPort;
            highPort = swapTmp;
        }

        count = highPort - lowPort + 1;
        cand = (u16)(*lastPortPtr - 1);
        while (count != 0) {
            u16 netPort;
            if (cand < lowPort || cand > highPort) {
                cand = (u16)highPort;
            }
            netPort = inetBswap16(cand);
            count--;
            if (inetInPcbLookupPort(table, inp->inp_laddr, netPort, 1) == NULL) {
                *lastPortPtr = cand;
                inp->inp_flags |= 0x40;
                inp->inp_lport = netPort;
                inetInPcbState(inp, 1);
                return 0;
            }
            cand--;
        }
    }

    if (inp->inp_laddr != 0) {
        inp->inp_laddr = 0;
    }
    return INET_EAGAIN;
}

/* [252] 0x0150A8 */
INET_NOINLINE int inetInPcbConnect(InetInpcbReal *inp, InetMbuf *nam)
{
    InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
    InetSockaddrIn *ifAddr = NULL;
    int err = 0;

    if (nam->m_len != sizeof(InetSockaddrIn)) {
        return INET_EINVAL;
    }
    if (sin->sin_family != INET_AF_INET) {
        return INET_EAFNOSUPPORT;
    }
    if (sin->sin_port == 0) {
        return INET_EADDRNOTAVAIL;
    }

    if (g_in_ifaddr_head != NULL) {
        if (sin->sin_addr == 0) {
            sin->sin_addr = g_in_ifaddr_head->ia_addr.sin_addr;
        } else if (sin->sin_addr == 0xFFFFFFFFU &&
                   (inp->inp_socket->so_options & INET_SO_ONESBCAST) == 0) {
            InetInIfaddr *ia = g_in_ifaddr_head;
            while (ia != NULL && (ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_BROADCAST) == 0) {
                ia = ia->ia_list_next;
            }
            if (ia != NULL) {
                sin->sin_addr = ia->ia_dstaddr.sin_addr;
            }
        }
    }

    if (inp->inp_laddr == 0) {
        ifAddr = inetInSelectSrc(sin, &inp->inp_route, inp->inp_socket->so_options,
                                 inp->inp_moptions, &err);
        if (ifAddr == NULL) {
            return (err != 0) ? err : INET_EADDRNOTAVAIL;
        }
    }

    {
        u32 localIp = (inp->inp_laddr == 0) ? ifAddr->sin_addr : inp->inp_laddr;
        if (inetInPcbLookupConnect(inp->inp_table, sin->sin_addr, sin->sin_port,
                                   localIp, inp->inp_lport) != NULL) {
            return INET_EADDRINUSE;
        }
    }

    if (inp->inp_laddr == 0) {
        if (inp->inp_lport == 0 && inetInPcbBind(inp, NULL) == INET_EAGAIN) {
            return INET_EAGAIN;
        }
        inp->inp_laddr = ifAddr->sin_addr;
    }

    inp->inp_faddr = sin->sin_addr;
    inp->inp_fport = sin->sin_port;
    inetInPcbState(inp, 2);
    return 0;
}

/* [253] 0x0152C8 */
INET_NOINLINE void inetInPcbPurgeIf(InetInpcbTable *table, InetIfnet *ifp)
{
    InetInpcbReal *inp = table->inpt_queue_first;
    while (inp != (InetInpcbReal *)table) {
        InetInpcbReal *next = inp->inp_queue_next;
        if (inp->inp_route.ro_rt != NULL && inp->inp_route.ro_rt->rt_ifp == ifp) {
            inetInRtChange(inp, 0);
        }
        if (inp->inp_moptions != NULL) {
            InetIpMoptions *imo = inp->inp_moptions;
            int idx;
            int removed = 0;
            if (imo->imo_multicast_ifp == ifp) {
                imo->imo_multicast_ifp = NULL;
            }
            for (idx = 0; idx < (int)imo->imo_num_memberships; idx++) {
                if (ifp != NULL &&
                    sceNet_lib_94DCA9F0(imo->imo_membership[idx]->inm_ifname, ifp->if_xname, 16) == 0) {
                    removed++;
                    inetInDelMulti(imo->imo_membership[idx]);
                } else if (removed != 0) {
                    imo->imo_membership[idx - removed] = imo->imo_membership[idx];
                }
            }
            imo->imo_num_memberships = (u16)(imo->imo_num_memberships - removed);
        }
        inp = next;
    }
}

/* [254] 0x0153E8 */
INET_NOINLINE void inetInRtChange(InetInpcbReal *inp, int err)
{
    (void)err;
    if (inp->inp_route.ro_rt != NULL) {
        inetRtFree(inp->inp_route.ro_rt);
        inp->inp_route.ro_rt = NULL;
    }
}

/* [255] 0x015424 */
INET_NOINLINE InetInpcbReal *inetInPcbLookupPort(InetInpcbTable *table, u32 laddr,
                                                 u16 lport, int lookupWild)
{
    InetInpcbReal *inp = table->inpt_queue_first;
    InetInpcbReal *match = NULL;
    int bestWild = 3;

    while (inp != (InetInpcbReal *)table) {
        if (inp->inp_lport == lport) {
            int wild = 0;
            int skip = 0;

            if (inp->inp_faddr != 0) {
                wild++;
            }
            if (inp->inp_laddr != 0) {
                if (laddr == 0) {
                    wild++;
                } else if (inp->inp_laddr != laddr) {
                    skip = 1;
                }
            } else if (laddr != 0) {
                wild++;
            }

            if (!skip && (wild == 0 || lookupWild != 0)) {
                if (wild < bestWild) {
                    match = inp;
                    bestWild = wild;
                    if (wild == 0) {
                        return match;
                    }
                }
            }
        }
        inp = inp->inp_queue_next;
    }
    return match;
}

/* [256] 0x0154DC */
INET_NOINLINE InetInpcbReal *inetInPcbLookupConnect(InetInpcbTable *table, u32 faddr,
                                                    u16 fport, u32 laddr, u16 lport)
{
    u32 hash = (inetBswap32(faddr) + inetBswap16(fport) +
                inetBswap32(laddr) + inetBswap16(lport)) & table->inpt_connecthash;
    InetInpcbReal **head = &table->inpt_connecthashtbl[hash];
    InetInpcbReal *inp = *head;

    while (inp != NULL) {
        if (inp->inp_faddr == faddr && inp->inp_fport == fport &&
            inp->inp_lport == lport && inp->inp_laddr == laddr) {
            if (inp != *head) {
                if (inp->inp_hash_next != NULL) {
                    inp->inp_hash_next->inp_hash_prev = inp->inp_hash_prev;
                }
                *inp->inp_hash_prev = inp->inp_hash_next;

                inp->inp_hash_next = *head;
                if (*head != NULL) {
                    (*head)->inp_hash_prev = &inp->inp_hash_next;
                }
                inp->inp_hash_prev = head;
                *head = inp;
            }
            return inp;
        }
        inp = inp->inp_hash_next;
    }
    return NULL;
}

/* [257] 0x0155C8 */
INET_NOINLINE InetInpcbReal *inetInPcbLookupBind(InetInpcbTable *table, u32 laddr, u16 lport)
{
    u16 hostPort = inetBswap16(lport);
    u32 hash = (inetBswap32(laddr) + hostPort) & table->inpt_bindhash;
    InetInpcbReal **head = &table->inpt_bindhashtbl[hash];
    InetInpcbReal *inp = *head;

    while (inp != NULL) {
        if (inp->inp_lport == lport && inp->inp_laddr == laddr) {
            if (inp != *head) {
                if (inp->inp_hash_next != NULL) {
                    inp->inp_hash_next->inp_hash_prev = inp->inp_hash_prev;
                }
                *inp->inp_hash_prev = inp->inp_hash_next;
                inp->inp_hash_next = *head;
                if (*head != NULL) {
                    (*head)->inp_hash_prev = &inp->inp_hash_next;
                }
                inp->inp_hash_prev = head;
                *head = inp;
            }
            return inp;
        }
        inp = inp->inp_hash_next;
    }

    hash = (inetBswap32(g_zeroin_addr) + hostPort) & table->inpt_bindhash;
    head = &table->inpt_bindhashtbl[hash];
    inp = *head;
    while (inp != NULL) {
        if (inp->inp_lport == lport && inp->inp_laddr == g_zeroin_addr) {
            if (inp != *head) {
                if (inp->inp_hash_next != NULL) {
                    inp->inp_hash_next->inp_hash_prev = inp->inp_hash_prev;
                }
                *inp->inp_hash_prev = inp->inp_hash_next;
                inp->inp_hash_next = *head;
                if (*head != NULL) {
                    (*head)->inp_hash_prev = &inp->inp_hash_next;
                }
                inp->inp_hash_prev = head;
                *head = inp;
            }
            return inp;
        }
        inp = inp->inp_hash_next;
    }
    return NULL;
}

/* [258] 0x0156E0 */
INET_NOINLINE void inetInPcbState(InetInpcbReal *inp, int state)
{
    if (inp->inp_state > 0) {
        if (inp->inp_hash_next != NULL) {
            inp->inp_hash_next->inp_hash_prev = inp->inp_hash_prev;
        }
        *inp->inp_hash_prev = inp->inp_hash_next;
    }

    if (state == 0) {
        inp->inp_state = 0;
        return;
    }
    if (state == 1) {
        InetInpcbTable *table = inp->inp_table;
        u32 hash = (inetBswap32(inp->inp_laddr) + inetBswap16(inp->inp_lport)) & table->inpt_bindhash;
        InetInpcbReal **head = &table->inpt_bindhashtbl[hash];
        inp->inp_hash_next = *head;
        if (*head != NULL) {
            (*head)->inp_hash_prev = &inp->inp_hash_next;
        }
        inp->inp_hash_prev = head;
        *head = inp;
    } else if (state == 2) {
        InetInpcbTable *table = inp->inp_table;
        u32 hash = (inetBswap32(inp->inp_faddr) + inetBswap16(inp->inp_fport) +
                    inetBswap32(inp->inp_laddr) + inetBswap16(inp->inp_lport)) & table->inpt_connecthash;
        InetInpcbReal **head = &table->inpt_connecthashtbl[hash];
        inp->inp_hash_next = *head;
        if (*head != NULL) {
            (*head)->inp_hash_prev = &inp->inp_hash_next;
        }
        inp->inp_hash_prev = head;
        *head = inp;
    }

    inp->inp_state = state;
}

/* [259] 0x0157F8 */
INET_NOINLINE InetSockaddrIn *inetInSelectSrc(InetSockaddrIn *sin, InetRoute *ro,
                                              int soOpts, InetIpMoptions *mopts, int *errp)
{
    InetInIfaddr *ia = NULL;
    int dontRoute = soOpts & INET_SO_DONTROUTE;

    if (ro->ro_rt != NULL) {
        if (((InetSockaddrIn *)&ro->ro_dst)->sin_addr != sin->sin_addr || dontRoute != 0) {
            if (ro->ro_rt->rt_refcnt >= 2) {
                ro->ro_rt->rt_refcnt--;
            } else {
                inetRtFree(ro->ro_rt);
            }
            ro->ro_rt = NULL;
        }
    }

    if (dontRoute == 0 && (ro->ro_rt == NULL || ro->ro_rt->rt_ifp == NULL)) {
        InetSockaddrIn *dst = (InetSockaddrIn *)&ro->ro_dst;
        dst->sin_family = INET_AF_INET;
        dst->sin_len = sizeof(InetSockaddrIn);
        dst->sin_addr = sin->sin_addr;
        inetRtAlloc(ro);
    }

    if (ro->ro_rt != NULL && (ro->ro_rt->rt_ifp->if_flags & INET_IFF_LOOPBACK) == 0) {
        ia = (InetInIfaddr *)ro->ro_rt->rt_ifa;
    }

    if (ia == NULL) {
        u16 savedPort = sin->sin_port;
        sin->sin_port = 0;
        ia = (InetInIfaddr *)inetIfaWithAnyAddr((InetSockaddr *)sin);
        sin->sin_port = savedPort;
        if (ia == NULL) {
            ia = g_in_ifaddr_head;
            while (ia != NULL && (ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_LOOPBACK) != 0) {
                ia = ia->ia_list_next;
            }
            if (ia == NULL) {
                *errp = INET_EADDRNOTAVAIL;
                return NULL;
            }
        }
    }

    if ((sin->sin_addr & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U) &&
        mopts != NULL && mopts->imo_multicast_ifp != NULL) {
        InetInIfaddr *mcastIa = inetIfpFirstInAddr(mopts->imo_multicast_ifp);
        if (mcastIa == NULL) {
            *errp = INET_EADDRNOTAVAIL;
            return NULL;
        }
        return &mcastIa->ia_addr;
    }
    return &ia->ia_addr;
}

/* [260] 0x015A08 */
INET_NOINLINE void inetInPcbInit(InetInpcbTable *table, int bindSize, int connectSize)
{
    u32 range = (u32)(g_anonportmax - g_anonportmin + 1);
    table->inpt_queue_first = (InetInpcbReal *)table;
    table->inpt_queue_last = (InetInpcbReal *)table;
    table->inpt_bindhashtbl = (InetInpcbReal **)sceNet_lib_C4261339(bindSize, &table->inpt_bindhash);
    table->inpt_connecthashtbl = (InetInpcbReal **)sceNet_lib_C4261339(connectSize, &table->inpt_connecthash);
    table->inpt_lastlow = 1023;
    table->inpt_lastport = (u16)(g_anonportmin + (sceNet_lib_1858883D() % range));
}

/* [261] 0x015A9C */
INET_NOINLINE int inetInPcbAlloc(InetSocket *so, InetInpcbTable *table)
{
    InetInpcbReal *inp = (InetInpcbReal *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetInpcbReal));
    int spl;

    if (inp == NULL) {
        return INET_ENOBUFS;
    }
    sceNet_lib_8B615F52(inp, sizeof(InetInpcbReal));
    inp->inp_errormtu = -1;
    so->so_pcb = inp;
    inp->inp_socket = so;
    inp->inp_table = table;

    spl = sceNet_lib_94B44F26();
    inp->inp_queue_prev = (InetInpcbReal **)table;
    inp->inp_queue_next = table->inpt_queue_first;
    if (table->inpt_queue_last == (InetInpcbReal *)table) {
        table->inpt_queue_last = inp;
    } else {
        table->inpt_queue_first->inp_queue_prev = (InetInpcbReal **)inp;
    }
    table->inpt_queue_first = inp;
    inetInPcbState(inp, 0);
    sceNet_lib_515B2F33(spl);
    return 0;
}

/* [262] 0x015B4C */
INET_NOINLINE void inetInPcbDisconnect(InetInpcbReal *inp)
{
    inp->inp_fport = 0;
    inp->inp_faddr = g_zeroin_addr;
    inetInPcbState(inp, 1);
    if ((inp->inp_socket->so_state & INET_SS_NOFDREF) != 0) {
        inetInPcbDetach(inp);
    }
}

/* [263] 0x015C54 */
INET_NOINLINE void inetInPcbDetach(InetInpcbReal *inp)
{
    InetSocket *so = inp->inp_socket;
    InetInpcbTable *table = inp->inp_table;
    InetInpcbReal *next;
    InetInpcbReal *prev;
    int spl;

    so->so_pcb = NULL;
    inetSoFree(so);
    if (inp->inp_options != NULL) {
        sceNetIfhandle_lib_F8825DC4(inp->inp_options);
    }
    if (inp->inp_route.ro_rt != NULL) {
        inetRtFree(inp->inp_route.ro_rt);
    }
    inetIpFreeMoptions(inp->inp_moptions);

    spl = sceNet_lib_94B44F26();
    inetInPcbState(inp, 0);

    next = inp->inp_queue_next;
    prev = (InetInpcbReal *)inp->inp_queue_prev;
    if (next == (InetInpcbReal *)table) {
        table->inpt_queue_last = prev;
    } else {
        next->inp_queue_prev = (InetInpcbReal **)prev;
    }
    if (prev == (InetInpcbReal *)table) {
        table->inpt_queue_first = next;
    } else {
        prev->inp_queue_next = next;
    }
    sceNet_lib_515B2F33(spl);
    sceNetIfhandle_lib_76BAD213(inp);
}

/* [264] 0x015D20 */
INET_NOINLINE void inetInSetSockaddr(InetInpcbReal *inp, InetMbuf *nam)
{
    InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
    nam->m_len = sizeof(InetSockaddrIn);
    sceNet_lib_8B615F52(sin, sizeof(InetSockaddrIn));
    sin->sin_len = sizeof(InetSockaddrIn);
    sin->sin_family = INET_AF_INET;
    sin->sin_port = inp->inp_lport;
    sin->sin_addr = inp->inp_laddr;
}

/* [265] 0x015D8C */
INET_NOINLINE void inetInSetPeeraddr(InetInpcbReal *inp, InetMbuf *nam)
{
    InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
    nam->m_len = sizeof(InetSockaddrIn);
    sceNet_lib_8B615F52(sin, sizeof(InetSockaddrIn));
    sin->sin_len = sizeof(InetSockaddrIn);
    sin->sin_family = INET_AF_INET;
    sin->sin_port = inp->inp_fport;
    sin->sin_addr = inp->inp_faddr;
}

/* [266] 0x015DF8 */
INET_NOINLINE int inetInPcbNotify(InetInpcbTable *table, u32 faddr, u16 fport,
                                  u32 laddr, u16 lport, int err,
                                  void (*notify)(InetInpcbReal *, int))
{
    u32 hash;
    InetInpcbReal *inp;
    int count = 0;

    if (faddr == 0 || notify == NULL) {
        return 0;
    }
    hash = (inetBswap32(faddr) + inetBswap16(fport) +
            inetBswap32(laddr) + inetBswap16(lport)) & table->inpt_connecthash;
    inp = table->inpt_connecthashtbl[hash];
    while (inp != NULL) {
        InetInpcbReal *next = inp->inp_hash_next;
        if (inp->inp_faddr == faddr && inp->inp_fport == fport &&
            inp->inp_lport == lport && inp->inp_laddr == laddr) {
            count++;
            notify(inp, err);
        }
        inp = next;
    }
    return count;
}

/* [267] 0x015F14 */
INET_NOINLINE void inetInPcbNotifyAll(InetInpcbTable *table, u32 faddr, int err,
                                      void (*notify)(InetInpcbReal *, int))
{
    InetInpcbReal *inp;
    if (faddr == 0 || notify == NULL) {
        return;
    }
    inp = table->inpt_queue_first;
    while (inp != (InetInpcbReal *)table) {
        InetInpcbReal *next = inp->inp_queue_next;
        if (inp->inp_faddr == faddr) {
            notify(inp, err);
        }
        inp = next;
    }
}

/* [268] 0x015FB4 */
INET_NOINLINE void inetInLosing(InetInpcbReal *inp)
{
    InetRtentry *rt = inp->inp_route.ro_rt;
    if (rt != NULL) {
        InetRtAddrinfo addrInfo;
        inp->inp_route.ro_rt = NULL;
        sceNet_lib_8B615F52(&addrInfo, 36);
        addrInfo.rti_info[0] = &inp->inp_route.ro_dst;
        addrInfo.rti_info[1] = rt->rt_gateway;
        addrInfo.rti_info[2] = (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_mask;
        inetRtMissMsg(INET_RTM_LOSING, &addrInfo, rt->rt_flags, 0);
        if ((rt->rt_flags & INET_RTF_DYNAMIC) != 0) {
            inetRtRequest(INET_RTM_DELETE,
                          (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key,
                          rt->rt_gateway,
                          (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_mask,
                          (u32)rt->rt_flags,
                          NULL);
        } else {
            inetRtFree(rt);
        }
    }
}

/* [269] 0x016058 */
INET_NOINLINE InetRtentry *inetInPcbRtEntry(InetInpcbReal *inp)
{
    InetRoute *ro = &inp->inp_route;
    if (ro->ro_rt == NULL && inp->inp_faddr != 0) {
        InetSockaddrIn *dst = (InetSockaddrIn *)&ro->ro_dst;
        dst->sin_family = INET_AF_INET;
        dst->sin_len = sizeof(InetSockaddrIn);
        dst->sin_addr = inp->inp_faddr;
        inetRtAlloc(ro);
    }
    return ro->ro_rt;
}
