/*
 * net/net.h: the library's initialisation, under Sony's names and with Sony's definitions (SDK
 * Release 03.570.011, target/include/net/net.h).
 */
#ifndef _SCE_NET_NET_NET_H
#define _SCE_NET_NET_NET_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SceNetInitParam {
	void *memory;
	int size;
	int flags;
} SceNetInitParam;

int sceNetInit(SceNetInitParam *param);
int sceNetTerm(void);

int sceNetShowIfconfig(void);
int sceNetShowRoute(void);
int sceNetShowNetstat(void);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NET_NET_NET_H */
