/*
 * net/ether.h: the Ethernet address, under Sony's names and with Sony's definitions (SDK Release
 * 03.570.011, target/include/net/ether.h).
 */
#ifndef _SCE_NET_NET_ETHER_H
#define _SCE_NET_NET_ETHER_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_NET_ETHER_ADDR_LEN			6
#define SCE_NET_ETHER_ADDRSTRLEN		18

typedef struct SceNetEtherAddr {
	SceUChar8 data[SCE_NET_ETHER_ADDR_LEN];
} SceNetEtherAddr;

int sceNetEtherStrton(const char *str, SceNetEtherAddr *n);
int sceNetEtherNtostr(const SceNetEtherAddr *n, char *str, SceSize len);
int sceNetGetMacAddress(SceNetEtherAddr *addr, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NET_NET_ETHER_H */
