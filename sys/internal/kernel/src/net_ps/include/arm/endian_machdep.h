/* $NetBSD: endian_machdep.h,v 1.5.10.1 2004/06/14 17:58:29 tron Exp $ */

/* GCC predefines __ARMEB__ when building for big-endian ARM. */
#ifdef __ARMEB__
#define _BYTE_ORDER _BIG_ENDIAN
#else
#define _BYTE_ORDER _LITTLE_ENDIAN
#endif

#ifdef __GNUC__

#include <arm/byte_swap.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN
/* SCE: a halfword is swapped as the word rev16 takes and leaves */
#define	ntohl(x)	((uint32_t)__byte_swap_32((uint32_t)(x)))
#define	ntohs(x)	__byte_swap_16(x)
#define	htonl(x)	((uint32_t)__byte_swap_32((uint32_t)(x)))
#define	htons(x)	__byte_swap_16(x)
#endif

#endif
