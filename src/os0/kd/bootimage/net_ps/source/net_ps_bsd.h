#ifndef SCE_NET_PS_BSD_H
#define SCE_NET_PS_BSD_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

/* SCE: the SDK platform supplies BSD socket and integer types. */
typedef uint64_t u_quad_t;
/* SCE: BOTH mbuf constructors store the 32-bit physical-address sentinel. */
typedef uint32_t paddr_t;
/* SCE: BOTH mbinit implementations allocate mbufs256 and clusters2048. */
#define MSIZE 256
#define MCLBYTES 2048

/*
 * SCE: NetBSD's interface timestamps retain their original 32-bit seconds.
 * The SDK's timeval has a 64-bit time_t and remains a separate platform type.
 * SceNetPsTimeval is the complete original network clock owner.
 */
/* The interface owners belong to NetBSD's extended networking namespace. */
#ifndef _NETBSD_SOURCE
#define _NETBSD_SOURCE 1
#define SCE_NET_PS_UNDEF_NETBSD_SOURCE
#endif
#define timeval SceNetPsTimeval
#define time_t int32_t
#include <net/if.h>
#include "vendor/netbsd-2.1/sys/net/if_ether.h"
#include "vendor/netbsd-2.1/sys/net/if_dl.h"
#undef sa_family_t
#include "vendor/netbsd-2.1/sys/netinet/in_var.h"
#include "vendor/netbsd-3.0/sys/sys/mbuf.h"
#include <sys/socketvar.h>
#undef time_t
#undef timeval
#ifdef SCE_NET_PS_UNDEF_NETBSD_SOURCE
#undef _NETBSD_SOURCE
#undef SCE_NET_PS_UNDEF_NETBSD_SOURCE
#endif

/*
 * SCE: the VM-free pool cache retains these four upstream members only.
 * Both TOOL pool_cache_init implementations initialize the complete 16 bytes.
 */
struct pool_allocator;
struct SceNetPsPoolFreeItem {
	struct SceNetPsPoolFreeItem *next;
};
/* SCE: complete56-byte replacement for NetBSD's page-backed pool owner. */
struct pool {
	unsigned int pr_size;
	unsigned int pr_align;
	unsigned int pr_itemoffset;
	int pr_flags;
	const char *pr_wchan;
	struct pool_allocator *pr_alloc;
	void (*pr_drain_hook)(void *, int);
	void *pr_drain_hook_arg;
	int pr_hardlimit;
	const char *pr_hardlimit_warning;
	int pr_hardlimit_ratecap;
	int pr_minitems;
	struct SceNetPsPoolFreeItem *freeList;
	int freeCount;
};
_Static_assert(sizeof(struct pool) == 56, "SCE pool size");
_Static_assert(offsetof(struct pool, freeList) == 48, "SCE pool free list");

struct mbuf;
struct pool_cache {
	struct pool *pc_pool;
	int (*pc_ctor)(void *, void *, int);
	void (*pc_dtor)(void *, void *);
	void *pc_arg;
};

extern struct pool_cache mb_cache;
void pool_put(struct pool *pool, void *object);
_Static_assert(sizeof(struct mbuf) == 256, "SCE mbuf size");
_Static_assert(offsetof(struct mbuf, m_paddr) == 24, "SCE mbuf physical address");
_Static_assert(offsetof(struct mbuf, m_pkthdr.csum_flags) == 44,
	       "SCE mbuf checksum flags");
_Static_assert(offsetof(struct mbuf, m_ext.ext_buf) == 56,
	       "SCE mbuf external storage");
_Static_assert(offsetof(struct mbuf, m_ext.ext_paddr) == 84,
	       "SCE mbuf external physical address");
_Static_assert(sizeof(struct pool_cache) == 16, "SCE pool cache size");

/* Kernel address-table declarations from pristine in_var.h. */
LIST_HEAD(in_ifaddrhashhead, in_ifaddr);
TAILQ_HEAD(in_ifaddrhead, in_ifaddr);
extern struct in_ifaddrhead in_ifaddrhead;
_Static_assert(sizeof(struct in_ifaddrhead) == 8, "IPv4 interface queue size");
extern struct in_ifaddrhashhead *in_ifaddrhashtbl;
extern struct ifnet loif[];

_Static_assert(sizeof(struct in_ifaddr) == 152, "NetBSD IPv4 interface address size");
_Static_assert(sizeof(struct in6_addrlifetime) == 16,
	       "NetBSD IPv6 address lifetime size");
/* The SDK's complete IPv6 sockaddr includes Sony's sin6_vport extension. */
_Static_assert(sizeof(struct in6_ifaddr) == 232,
	       "SDK IPv6 interface address size");
_Static_assert(offsetof(struct in_ifaddr, ia_addr.sin_addr) == 96,
	       "NetBSD IPv4 interface address offset");
_Static_assert(sizeof(struct ifnet) == 480, "NetBSD ifnet size");
_Static_assert(sizeof(struct sockaddr_dl) == 20, "NetBSD link address size");
_Static_assert(offsetof(struct sockaddr_dl, sdl_nlen) == 5,
	       "NetBSD link address name length");
_Static_assert(offsetof(struct sockaddr_dl, sdl_data) == 8,
	       "NetBSD link address data");
_Static_assert(sizeof(struct ethercom) == 504, "NetBSD ethercom size");
_Static_assert(sizeof(struct ether_multi) == 28, "NetBSD ether_multi size");
_Static_assert(offsetof(struct ifnet, if_data.ifi_mtu) == 64,
	       "NetBSD interface MTU offset");
_Static_assert(offsetof(struct ethercom, ec_multiaddrs) == 480,
	       "NetBSD Ethernet multicast list offset");
_Static_assert(offsetof(struct ether_multi, enm_list) == 20,
	       "NetBSD Ethernet multicast link offset");

#endif
