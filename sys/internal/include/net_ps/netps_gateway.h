/* SceNetPs gateway contract shared with the Bluetooth network gateway. */
#ifndef SCE_NETPS_GATEWAY_H
#define SCE_NETPS_GATEWAY_H

#include <psp2/net/net.h>
#include <net_ps/netdrv.h>
#include <stddef.h>

/* The remaining words are copied opaquely and have no operational readers. */
typedef struct SceNetPsDnsConfig {
	SceNetDnsInfo dns;
	unsigned int flags;
	unsigned int reserved[7];
} SceNetPsDnsConfig;

/* Reviewed names use Sony gateway vocabulary; the complete ABI follows originals. */
struct SceNetKernelGateway {
	void *(*allocate)(struct SceNetKernelGateway *interface,
			  unsigned int size, unsigned int alignment);
	int (*free)(struct SceNetKernelGateway *interface, void *pointer);
	int (*send)(struct SceNetKernelGateway *interface, const void *data,
		    unsigned int length, unsigned int destinationAddress);
	unsigned int portBase;
	unsigned int portCount;
	int (*receive)(struct SceNetKernelGateway *interface,
		       struct NetDriver *driver, struct NetDriverPacket *packet);
	int (*notify)(struct SceNetKernelGateway *interface, int event,
		      const SceNetPsDnsConfig *config, int length);
};

_Static_assert(sizeof(SceNetDnsInfo) == 8, "SDK DNS information size");
_Static_assert(sizeof(SceNetPsDnsConfig) == 40, "DNS configuration size");
_Static_assert(offsetof(SceNetPsDnsConfig, dns) == 0, "DNS addresses");
_Static_assert(offsetof(SceNetPsDnsConfig, flags) == 8, "DNS flags");
_Static_assert(offsetof(SceNetPsDnsConfig, reserved) == 12, "DNS reserved tail");
_Static_assert(sizeof(struct SceNetKernelGateway) == 28, "gateway size");
_Static_assert(offsetof(struct SceNetKernelGateway, allocate) == 0,
	       "gateway allocator");
_Static_assert(offsetof(struct SceNetKernelGateway, free) == 4,
	       "gateway deallocator");
_Static_assert(offsetof(struct SceNetKernelGateway, send) == 8, "gateway sender");
_Static_assert(offsetof(struct SceNetKernelGateway, portBase) == 12,
	       "gateway port base");
_Static_assert(offsetof(struct SceNetKernelGateway, portCount) == 16,
	       "gateway port count");
_Static_assert(offsetof(struct SceNetKernelGateway, receive) == 20,
	       "gateway receiver");
_Static_assert(offsetof(struct SceNetKernelGateway, notify) == 24,
	       "gateway notification");

int SceNetKernelGateway(struct SceNetKernelGateway *interface);

#endif
