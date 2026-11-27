/*
 * PSP 6.61 SceNetInet_Library (pspnet_inet.prx)
 * Module initialization, address conversion, errno helpers, and AF_INET protosw table
 */

#include "pspnet_inet_internal.h"

static char g_inet_ntoa_buf[18];
static int g_inet_initialized = 0;

/* ICMP error code to BSD errno map (21 entries at 0x32DD4) */
const int g_inetctlerrmap[21] = {
    0, 0, 0, 0, 0,
    INET_EMSGSIZE, INET_EHOSTDOWN, INET_EHOSTUNREACH,
    INET_EHOSTUNREACH, INET_EHOSTUNREACH, INET_ECONNREFUSED,
    INET_ECONNREFUSED, INET_EMSGSIZE, INET_EHOSTUNREACH,
    0, 0, 0, 0, 0, 0,
    INET_ENOPROTOOPT
};

/* AF_INET protocol switch table (9 entries, matching 0x32B8C..0x32D5F) */
InetProtosw g_inetsw[9] = {
    { /* [0] IP */
        0, &g_inetdomain, 0, 0,
        NULL, (void *)inetIpOutput, NULL, NULL,
        NULL, inetIpInit, NULL, inetIpSlowtimo,
        inetIpDrain, (void *)inetIpSysctl
    },
    { /* [1] ICMP */
        INET_SOCK_RAW, &g_inetdomain, INET_IPPROTO_ICMP, 0x0043,
        (void *)inetIcmpInput, (void *)inetRipOutput, NULL, (void *)inetRipCtloutput,
        (void *)inetRipUsrreq, inetIcmpInit, NULL, NULL,
        NULL, (void *)inetIcmpSysctl
    },
    { /* [2] UDP */
        INET_SOCK_DGRAM, &g_inetdomain, INET_IPPROTO_UDP, 0x0003,
        (void *)inetUdpInput, NULL, (void *)inetUdpCtlinput, (void *)inetIpCtloutput,
        (void *)inetUdpUsrreq, inetUdpInit, NULL, NULL,
        NULL, (void *)inetUdpSysctl
    },
    { /* [3] TCP */
        INET_SOCK_STREAM, &g_inetdomain, INET_IPPROTO_TCP, 0x002C,
        (void *)inetTcpInput, NULL, (void *)inetTcpCtlinput, (void *)inetTcpCtloutput,
        (void *)inetTcpUsrreq, inetTcpInit, inetTcpFasttimo, inetTcpSlowtimo,
        inetTcpDrain, (void *)inetTcpSysctl
    },
    { /* [4] RAW 255 */
        INET_SOCK_RAW, &g_inetdomain, 255, 0x0003,
        (void *)inetRipInput, (void *)inetRipOutput, NULL, (void *)inetRipCtloutput,
        (void *)inetRipUsrreq, NULL, NULL, NULL,
        NULL, NULL
    },
    { /* [5] IGMP */
        INET_SOCK_RAW, &g_inetdomain, INET_IPPROTO_IGMP, 0x0043,
        (void *)inetIgmpInput, (void *)inetRipOutput, NULL, (void *)inetRipCtloutput,
        (void *)inetRipUsrreq, inetIgmpInit, inetIgmpFasttimo, inetIgmpSlowtimo,
        NULL, NULL
    },
    { /* [6] UDP-P2P (SOCK_DGRAM_P2P = 6) */
        6, &g_inetdomain, 0, 0x0003,
        (void *)inetUdpInput, NULL, NULL, NULL,
        (void *)inetUdpP2pUsrreq, inetUdpP2pInit, NULL, NULL,
        NULL, NULL
    },
    { /* [7] TCP-P2P (SOCK_STREAM_P2P = 10) */
        10, &g_inetdomain, INET_IPPROTO_TCP, 0x002C,
        (void *)inetTcpInput, NULL, (void *)inetTcpCtlinput, (void *)inetTcpCtloutput,
        (void *)inetTcpUsrreq, inetTcpInit, NULL, inetTcpSlowtimo,
        inetTcpDrain, NULL
    },
    { /* [8] RAW wildcard */
        INET_SOCK_RAW, &g_inetdomain, 0, 0x0003,
        (void *)inetRipInput, (void *)inetRipOutput, NULL, (void *)inetRipCtloutput,
        (void *)inetRipUsrreq, inetRipInit, NULL, NULL,
        NULL, NULL
    }
};

InetDomain g_inetdomain = {
    INET_AF_INET,
    "internet",
    NULL,
    NULL,
    NULL,
    g_inetsw,
    &g_inetsw[9],
    NULL,
    (void *)inetRnInithead,
    32,
    sizeof(InetSockaddrIn)
};

/* [481] 0x03002C */
char *sceNetInet_lib_2595064C(u32 addr)
{
    u32 netAddr = addr;
    sceNet_lib_80C9F02A(g_inet_ntoa_buf, "[inet_ntoa error]");
    sceNetInetInetNtop(INET_AF_INET, &netAddr, g_inet_ntoa_buf, sizeof(g_inet_ntoa_buf));
    return g_inet_ntoa_buf;
}

/* [482] 0x03007C */
const char *sceNetInetInetNtop(int af, const void *src, char *dst, u32 size)
{
    InetThreadInfo *ti;
    char tmpBuf[16];
    const u8 *bytes = (const u8 *)src;
    int stackGuard[4];

    if (src == NULL || dst == NULL) {
        return NULL;
    }

    ti = sceNet_lib_CA3CF5EB(stackGuard);
    if (ti == NULL) {
        return NULL;
    }

    if (af != INET_AF_INET) {
        sceNet_lib_757085B0(ti, INET_EAFNOSUPPORT);
        return NULL;
    }

    {
        u32 len = (u32)sceNet_lib_CF705E46(tmpBuf, "%u.%u.%u.%u",
                                           bytes[0], bytes[1], bytes[2], bytes[3]);
        const char *ret = NULL;
        if (len == 0 || len >= size) {
            sceNet_lib_D861EF33(INET_ENOSPC);
        } else {
            sceNet_lib_80C9F02A(dst, tmpBuf);
            ret = dst;
        }
        sceNet_lib_757085B0(ti, ti->err);
        return ret;
    }
}

/* [483] 0x03017C */
int sceNetInetInetPton(int af, const char *src, void *dst)
{
    int stackGuard[4];
    InetThreadInfo *ti = sceNet_lib_CA3CF5EB(stackGuard);

    if (ti == NULL) {
        return -1;
    }
    if (af != INET_AF_INET) {
        sceNet_lib_757085B0(ti, INET_EAFNOSUPPORT);
        return -1;
    }

    {
        int res = inetAtonInternal(src, (u32 *)dst, 1);
        sceNet_lib_757085B0(ti, ti->err);
        return res;
    }
}

/* [484] 0x030214 */
u32 sceNetInetInetAddr(const char *cp)
{
    u32 addr;
    if (inetAtonInternal(cp, &addr, 0) != 0) {
        return addr;
    }
    return 0xFFFFFFFFU;
}

/* [485] 0x030244 */
int sceNetInetInetAton(const char *cp, void *addr)
{
    return inetAtonInternal(cp, (u32 *)addr, 0);
}

/* [486] 0x030260 */
INET_NOINLINE int inetAtonInternal(const char *cp, u32 *addrOut, int strictDecimal)
{
    u32 parts[4];
    int partCount = 0;
    const u8 *cur = (const u8 *)cp;
    u8 ch;

    if (cp == NULL) {
        return 0;
    }

    ch = *cur;
    for (;;) {
        u32 val = 0;
        u32 base = 10;

        if ((sceNet_lib_750F705D(ch) & 4) == 0) {
            return 0;
        }
        if (ch == '0') {
            ch = *++cur;
            if (ch == 'x' || ch == 'X') {
                ch = *++cur;
                base = 16;
                if (strictDecimal) {
                    return 0;
                }
            } else if ((sceNet_lib_750F705D(ch) & 4) != 0) {
                if (ch != '9') {
                    base = 8;
                }
                if (strictDecimal && base != 10) {
                    return 0;
                }
            }
        }

        for (;;) {
            if ((sceNet_lib_750F705D(ch) & 4) != 0) {
                u32 digit = (u32)(ch - '0');
                if (digit >= base) {
                    break;
                }
                val = val * base + digit;
                ch = *++cur;
            } else if (base == 16 && (sceNet_lib_750F705D(ch) & 0x44) != 0) {
                u32 digit = ((sceNet_lib_750F705D(ch) & 2) != 0) ? (u32)(ch - 'a' + 10) : (u32)(ch - 'A' + 10);
                if (digit >= 16) {
                    break;
                }
                val = (val << 4) | digit;
                ch = *++cur;
            } else {
                break;
            }
        }

        if (ch == '.') {
            if (partCount >= 3) {
                return 0;
            }
            parts[partCount++] = val;
            ch = *++cur;
        } else {
            u32 result;
            if (ch != '\0' && (sceNet_lib_750F705D(ch) & 8) == 0) {
                return 0;
            }
            if (strictDecimal && partCount != 3) {
                return 0;
            }
            switch (partCount) {
            case 0:
                result = val;
                break;
            case 1:
                if (parts[0] > 0xFF || val > 0xFFFFFFU) {
                    return 0;
                }
                result = (parts[0] << 24) | val;
                break;
            case 2:
                if ((parts[0] | parts[1]) > 0xFF || val > 0xFFFFU) {
                    return 0;
                }
                result = (parts[0] << 24) | (parts[1] << 16) | val;
                break;
            case 3:
                if ((parts[0] | parts[1] | parts[2] | val) > 0xFF) {
                    return 0;
                }
                result = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | val;
                break;
            default:
                return 0;
            }
            if (addrOut != NULL) {
                u32 netVal = inetBswap32(result);
                sceKernelMemcpy(addrOut, &netVal, 4);
            }
            return 1;
        }
    }
}

/* [487] 0x030588 */
int sceNetInetInit(void)
{
    int ret;
    u32 loAddr;
    u32 loMask;

    if (sceKernelCheckThreadStack() < 4048) {
        return (int)0x80410005U;
    }
    if (g_inet_initialized == 1) {
        return (int)0x80410201U;
    }

    ret = inetDomainInit();
    if (ret >= 0) {
        inetSoInit();
        ret = inetSoabortInit();
    }
    if (ret >= 0) {
        ret = sceNet_lib_991FF86D((void *)inetArpIfInit);
    }
    if (ret >= 0) {
        ret = inetIfHookInit();
    }
    if (ret >= 0) {
        ret = inetEtherHookInit();
    }
    if (ret >= 0) {
        ret = inetSpppHookInit();
    }
    if (ret >= 0) {
        ret = sceNet_lib_13672F83(inetIpIntr, inetArpIntr);
    }
    if (ret >= 0) {
        inetIfctrlResetSeq();
        inetLoAttach();
        loAddr = inetBswap32(0x7F000001U);
        loMask = inetBswap32(0xFF000000U);
        ret = sceNetInet_lib_4F68DB0E("lo0", &loAddr, NULL, &loMask);
    }

    if (ret < 0) {
        sceNet_lib_5C7C7381();
        inetSpppHookTerm();
        inetEtherHookTerm();
        inetIfHookTerm();
        sceNet_lib_5505D820();
        inetSoabortTerm();
        inetDomainTerm();
        return ret;
    }

    g_inet_initialized = 1;
    return 0;
}

/* [488] 0x030700 */
int sceNetInetTerm(void)
{
    if (sceKernelCheckThreadStack() < 4080) {
        return (int)0x80410005U;
    }
    if (g_inet_initialized == 0) {
        return 0;
    }

    sceNet_lib_5C7C7381();
    inetSpppHookTerm();
    inetEtherHookTerm();
    inetIfHookTerm();
    sceNet_lib_5505D820();
    inetSoabortTerm();
    inetDomainTerm();
    g_inet_initialized = 0;
    return 0;
}

/* [489] 0x030788 */
INET_NOINLINE int inetGetErrnoRaw(void)
{
    return sceNet_lib_435843CB();
}

/* [490] 0x0307A4 */
int sceNetInetGetErrno(void)
{
    if (sceKernelCheckThreadStack() < 4080) {
        return (int)0x80410005U;
    }
    return sceNet_lib_435843CB();
}

/* [491] 0x0307E0 */
u32 sceNetInetGetPspError(void)
{
    int err;
    if (sceKernelCheckThreadStack() < 4080) {
        return (u32)0x80410005U;
    }
    err = sceNet_lib_435843CB();
    if (err != 0) {
        return ((u32)(u16)err | 0x80010000U);
    }
    return 0;
}

/* [492] 0x03082C */
int sceNetInet_CCC18C45(const u8 *macAddr, const u32 *ipAddr)
{
    return sceNetInet_lib_59561561(macAddr, ipAddr);
}

/* [493] 0x030848 */
int sceNetInet_2D5868C0(int unusedArg, const u32 *ipAddr)
{
    return sceNetInet_lib_DC38FEE9(unusedArg, ipAddr);
}

/* [494] 0x030864 */
int sceNetInet_1D023504(s32 bufSize, InetArpEntryInfo *outBuf)
{
    return sceNetInet_lib_824CBC6D(bufSize, outBuf);
}
