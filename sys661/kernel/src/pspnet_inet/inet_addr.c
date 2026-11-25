/*
 * IPv4 address string conversion utilities (inet_aton, inet_addr, inet_pton,
 * inet_ntop, inet_ntoa).
 */
#include "pspnet_inet_internal.h"

static int inetParseIpv4(const char *src, u32 *outAddrNet)
{
    u32 parts[4];
    int partCount = 0;
    const char *ptr = src;
    u32 val;

    if (src == NULL || *src == '\0') {
        return 0;
    }

    while (*ptr != '\0' && partCount < 4) {
        if (*ptr < '0' || *ptr > '9') {
            return 0;
        }
        val = 0;
        while (*ptr >= '0' && *ptr <= '9') {
            val = val * 10U + (u32)(*ptr - '0');
            if (val > 255U) {
                return 0;
            }
            ptr++;
        }
        parts[partCount++] = val;
        if (*ptr == '.') {
            ptr++;
            if (*ptr == '\0') {
                return 0;
            }
        } else if (*ptr != '\0') {
            return 0;
        }
    }

    if (partCount != 4 || *ptr != '\0') {
        return 0;
    }

    /* Network byte order stored in little-endian u32 */
    *outAddrNet = (parts[0]) | (parts[1] << 8) | (parts[2] << 16) | (parts[3] << 24);
    return 1;
}

static int inetFormatIpv4(const u8 *octets, char *dst, u32 dstSize)
{
    char tmp[20];
    int pos = 0;
    int idx;

    for (idx = 0; idx < 4; idx++) {
        u32 val = octets[idx];
        char digits[4];
        int dcount = 0;

        if (idx > 0) {
            tmp[pos++] = '.';
        }
        if (val == 0) {
            digits[dcount++] = '0';
        } else {
            while (val > 0) {
                digits[dcount++] = (char)('0' + (val % 10U));
                val /= 10U;
            }
        }
        while (dcount > 0) {
            tmp[pos++] = digits[--dcount];
        }
    }
    tmp[pos] = '\0';

    if ((u32)(pos + 1) > dstSize) {
        return -1;
    }
    sceKernelMemcpy(dst, tmp, (u32)(pos + 1));
    return 0;
}

int sceNetInetInetAton(const char *cp, SceNetInetInAddr *addr)
{
    u32 parsed;

    if (cp == NULL) {
        return 0;
    }
    if (!inetParseIpv4(cp, &parsed)) {
        return 0;
    }
    if (addr != NULL) {
        addr->sAddr = parsed;
    }
    return 1;
}

u32 sceNetInetInetAddr(const char *cp)
{
    SceNetInetInAddr addr;

    if (sceNetInetInetAton(cp, &addr) == 0) {
        return 0xFFFFFFFFU;
    }
    return addr.sAddr;
}

int sceNetInetInetPton(int af, const char *src, void *dst)
{
    u32 parsed;

    if (af != INET_AF_INET) {
        inetSetErrno(INET_EAFNOSUPPORT);
        return -1;
    }
    if (src == NULL || dst == NULL) {
        inetSetErrno(INET_EFAULT);
        return -1;
    }
    if (!inetParseIpv4(src, &parsed)) {
        return 0;
    }
    sceKernelMemcpy(dst, &parsed, sizeof(parsed));
    return 1;
}

const char *sceNetInetInetNtop(int af, const void *src, char *dst, SceSize size)
{
    if (af != INET_AF_INET) {
        inetSetErrno(INET_EAFNOSUPPORT);
        return NULL;
    }
    if (src == NULL || dst == NULL) {
        inetSetErrno(INET_EFAULT);
        return NULL;
    }
    if (inetFormatIpv4((const u8 *)src, dst, size) < 0) {
        inetSetErrno(INET_ENOSPC);
        return NULL;
    }
    return dst;
}

/* inet_ntoa internal export */
char *sceNetInet_lib_2595064C(u32 inAddr)
{
    u8 octets[4];

    octets[0] = (u8)(inAddr & 0xFFU);
    octets[1] = (u8)((inAddr >> 8) & 0xFFU);
    octets[2] = (u8)((inAddr >> 16) & 0xFFU);
    octets[3] = (u8)((inAddr >> 24) & 0xFFU);
    if (inetFormatIpv4(octets, g_inet.ntoaBuf, sizeof(g_inet.ntoaBuf)) < 0) {
        return "[inet_ntoa error]";
    }
    return g_inet.ntoaBuf;
}
