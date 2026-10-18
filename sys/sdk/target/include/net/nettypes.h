/*
 * net/nettypes.h: the network library's base types, under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/net/nettypes.h).
 */
#ifndef _SCE_NET_NET_NETTYPES_H
#define _SCE_NET_NET_NETTYPES_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef SceUInt32 SceNetInAddr_t;
typedef SceUShort16 SceNetInPort_t;
typedef SceUChar8 SceNetSaFamily_t;
typedef SceUInt32 SceNetSocklen_t;

typedef int SceNetId;

#define SCE_NET_DEBUG_NAME_LEN_MAX		31	/* non null terminated */

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NET_NET_NETTYPES_H */
