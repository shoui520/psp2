/*
 * net/inet.h: the address conversions, under Sony's names and with Sony's definitions (SDK Release
 * 03.570.011, target/include/net/inet.h).
 */
#ifndef _SCE_NET_NET_INET_H
#define _SCE_NET_NET_INET_H

#include <scetypes.h>
#include <net/nettypes.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *sceNetInetNtop(int af, const void *src, char *dst,
	SceNetSocklen_t size);
int sceNetInetPton(int af, const char *src, void *dst);

SceUInt64 sceNetHtonll(SceUInt64 host64);
SceUInt32 sceNetHtonl(SceUInt32 host32);
SceUShort16 sceNetHtons(SceUShort16 host16);
SceUInt64 sceNetNtohll(SceUInt64 net64);
SceUInt32 sceNetNtohl(SceUInt32 net32);
SceUShort16 sceNetNtohs(SceUShort16 net16);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NET_NET_INET_H */
