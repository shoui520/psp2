/*
 * net/dns.h: the DNS server information, under Sony's names and with Sony's definitions (SDK
 * Release 03.570.011, target/include/net/dns.h).
 */
#ifndef _SCE_NET_NET_DNS_H
#define _SCE_NET_NET_DNS_H

#include <scetypes.h>
#include <net/in.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_NET_DNS_ADDR_MAX			2

typedef struct SceNetDnsInfo {
	SceNetInAddr dns_addr[SCE_NET_DNS_ADDR_MAX];	/* network byte order */
} SceNetDnsInfo;

int sceNetSetDnsInfo(SceNetDnsInfo *info, int flags);
int sceNetClearDnsCache(int flags);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NET_NET_DNS_H */
