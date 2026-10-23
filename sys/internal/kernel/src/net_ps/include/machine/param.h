/*
 * SCE: the psp2 port's machine parameters: arm's (arm/param.h: mbufs of 256 bytes, clusters of
 * 2048, as both mbinit implementations allocate them) with arm32's 4 KiB pages, without arm32's
 * kernel CPU definitions (arm/arm32/param.h).
 */
#ifndef _PSP2_PARAM_H_
#define _PSP2_PARAM_H_

#define PGSHIFT		12		/* LOG2(NBPG) */
#define NBPG		(1 << PGSHIFT)	/* bytes/page */
#define PGOFSET		(NBPG - 1)	/* byte offset into page */

#include <arm/param.h>

#ifdef _KERNEL
#include <machine/intr.h>	/* SCE: the spl levels, as arm32's param.h brings them */
#endif

#endif /* _PSP2_PARAM_H_ */
