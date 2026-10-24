#ifndef SCE_NET_PS_BSD_H
#define SCE_NET_PS_BSD_H

/* NetBSD's network structures, from its kernel headers (net_ps/include) */
#include <stdint.h>
#include <stddef.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mbuf.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/if_ether.h>
#include <net/if_dl.h>
#include <netinet/in_var.h>
#include <sys/pool.h>
#include <sys/socketvar.h>

_Static_assert(sizeof(struct pool) == 56, "SCE pool size");
_Static_assert(offsetof(struct pool, freeList) == 48, "SCE pool free list");

extern struct pool_cache mb_cache;
_Static_assert(sizeof(struct mbuf) == 256, "SCE mbuf size");
_Static_assert(offsetof(struct mbuf, m_paddr) == 24, "SCE mbuf physical address");
_Static_assert(offsetof(struct mbuf, m_pkthdr.csum_flags) == 44,
	       "SCE mbuf checksum flags");
_Static_assert(offsetof(struct mbuf, m_ext.ext_buf) == 56,
	       "SCE mbuf external storage");
_Static_assert(offsetof(struct mbuf, m_ext.ext_paddr) == 84,
	       "SCE mbuf external physical address");
_Static_assert(sizeof(struct pool_cache) == 16, "SCE pool cache size");

_Static_assert(sizeof(struct in_ifaddrhead) == 8, "IPv4 interface queue size");
/* the loopback interface (if.h leaves it out: an incomplete tentative definition) */
extern struct ifnet loif[];

_Static_assert(sizeof(struct in_ifaddr) == 152, "NetBSD IPv4 interface address size");
_Static_assert(sizeof(struct in6_addrlifetime) == 16,
	       "NetBSD IPv6 address lifetime size");
/* NetBSD's sockaddr_in6, without a virtual port: Sony's struct syn_cache is 0xb0 bytes with sc_dst at 0x64 */
_Static_assert(sizeof(struct sockaddr_in6) == 28, "NetBSD IPv6 socket address size");
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
