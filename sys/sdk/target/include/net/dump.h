/*
 * net/dump.h: the packet dump functions, under Sony's names and with Sony's definitions (SDK
 * Release 03.570.011, target/include/net/dump.h).
 */
#ifndef _SCE_NET_NET_DUMP_H
#define _SCE_NET_NET_DUMP_H

#include <scetypes.h>
#include <net/nettypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* flags for sceNetDumpRead */
#define SCE_NET_DUMP_PEEK			0x00000010	/* read(set) */
#define SCE_NET_DUMP_DONTWAIT			0x00000020	/* read(set) */
#define SCE_NET_DUMP_OVERFLOW			0x00000040	/* read(get) */

/* flags for sceNetSocketAbort */
#define SCE_NET_DUMP_ABORT_FLAG_PRESERVATION	0x00000001

SceNetId sceNetDumpCreate(const char *name, int len, int flags);
int sceNetDumpRead(SceNetId id, void *buf, int len, int *pflags);
int sceNetDumpDestroy(SceNetId id);
int sceNetDumpAbort(SceNetId id, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NET_NET_DUMP_H */
