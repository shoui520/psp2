/*
 * net/resolver.h: the resolver functions, under Sony's names and with Sony's definitions (SDK
 * Release 03.570.011, target/include/net/resolver.h).
 */
#ifndef _SCE_NET_NET_RESOLVER_H
#define _SCE_NET_NET_RESOLVER_H

#include <scetypes.h>
#include <net/in.h>
#include <net/nettypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_NET_RESOLVER_PORT			53
#define SCE_NET_RESOLVER_HOSTNAME_LEN_MAX	255

/* flags for sceNetResolverCreate() */

/* flags for sceNetResolverStartNtoa(), sceNetResolverStartAton() */
#define SCE_NET_RESOLVER_ASYNC			0x00000001
#define SCE_NET_RESOLVER_START_NTOA_DISABLE_IPADDRESS	0x00010000

/* flags for sceNetResolverAbort() */
#define SCE_NET_RESOLVER_ABORT_FLAG_NTOA_PRESERVATION	0x00000001
#define SCE_NET_RESOLVER_ABORT_FLAG_ATON_PRESERVATION	0x00000002

typedef void *(*SceNetResolverFunctionAllocate)(
	SceSize size,
	SceNetId rid,
	const char *name,
	void *user);
typedef void (*SceNetResolverFunctionFree)(
	void *ptr,
	SceNetId rid,
	const char *name,
	void *user);

typedef struct SceNetResolverParam {
	SceNetResolverFunctionAllocate allocate;
	SceNetResolverFunctionFree free;
	void *user;
} SceNetResolverParam;

SceNetId sceNetResolverCreate(const char *name, SceNetResolverParam *param,
	int flags);
int sceNetResolverStartNtoa(SceNetId rid, const char *hostname,
	SceNetInAddr *addr, int timeout, int retry, int flags);
int sceNetResolverStartAton(SceNetId rid, const SceNetInAddr *addr,
	char *hostname, int len, int timeout, int retry, int flags);
int sceNetResolverGetError(SceNetId rid, int *result);
int sceNetResolverDestroy(SceNetId rid);
int sceNetResolverAbort(SceNetId rid, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NET_NET_RESOLVER_H */
