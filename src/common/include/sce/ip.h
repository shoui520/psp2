/* IPv4 header names and layout from the PSP2 SDK net/header/ip.h. */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <psp2/types.h>
/* The pinned SDK models ver/hl as two bytes and consequently gives its
 * IPv4 header 24 bytes. Keep those declarations separate from Sony's
 * original one-byte bitfield and 20-byte wire header below.
 */
#define SceNetIpHeaderIpVerHl LegacySdkSceNetIpHeaderIpVerHl
#define SceNetIpHeaderUnion LegacySdkSceNetIpHeaderUnion
#define SceNetIpHeader LegacySdkSceNetIpHeader
#include <psp2common/net.h>
#undef SceNetIpHeader
#undef SceNetIpHeaderUnion
#undef SceNetIpHeaderIpVerHl

typedef struct SceNetIpHeaderIpVerHl {
#if defined(BYTE_ORDER) && defined(BIG_ENDIAN) && BYTE_ORDER == BIG_ENDIAN
	SceUChar8 ver:4;
	SceUChar8 hl:4;
#else
	SceUChar8 hl:4;
	SceUChar8 ver:4;
#endif
} SceNetIpHeaderIpVerHl;

typedef union SceNetIpHeaderUnion {
	SceNetIpHeaderIpVerHl ip_ver_hl;
	SceUChar8 ver_hl;
} SceNetIpHeaderUnion;

#define SCE_NET_IPVERSION 4
#define SCE_NET_IP_RF 0x8000
#define SCE_NET_IP_DF 0x4000
#define SCE_NET_IP_MF 0x2000
#define SCE_NET_IP_OFFMASK 0x1FFF

typedef struct SceNetIpHeader {
	SceNetIpHeaderUnion un;
	SceUChar8 ip_tos;
	SceUShort16 ip_len;
	SceUShort16 ip_id;
	SceUShort16 ip_off;
	SceUChar8 ip_ttl;
	SceUChar8 ip_p;
	SceUShort16 ip_sum;
	SceNetInAddr ip_src;
	SceNetInAddr ip_dst;
} SceNetIpHeader;

_Static_assert(sizeof(SceNetIpHeaderIpVerHl) == 1, "IPv4 version/header length");
_Static_assert(sizeof(SceNetIpHeaderUnion) == 1, "IPv4 first byte");
_Static_assert(sizeof(SceNetIpHeader) == 20, "IPv4 header");
_Static_assert(offsetof(SceNetIpHeader, un) == 0, "IPv4 version/header length");
_Static_assert(offsetof(SceNetIpHeader, ip_tos) == 1, "IPv4 service type");
_Static_assert(offsetof(SceNetIpHeader, ip_len) == 2, "IPv4 length");
_Static_assert(offsetof(SceNetIpHeader, ip_id) == 4, "IPv4 identification");
_Static_assert(offsetof(SceNetIpHeader, ip_off) == 6, "IPv4 fragment offset");
_Static_assert(offsetof(SceNetIpHeader, ip_ttl) == 8, "IPv4 time to live");
_Static_assert(offsetof(SceNetIpHeader, ip_p) == 9, "IPv4 protocol");
_Static_assert(offsetof(SceNetIpHeader, ip_sum) == 10, "IPv4 checksum");
_Static_assert(offsetof(SceNetIpHeader, ip_src) == 12, "IPv4 source");
_Static_assert(offsetof(SceNetIpHeader, ip_dst) == 16, "IPv4 destination");
