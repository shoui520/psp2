/*
 * kernel/types.h: the kernel's basic types, under Sony's names and with Sony's definitions (SDK
 * Release 03.570.011, target/include/kernel/types.h).
 */
#ifndef _SCE_KERNEL_TYPES_H
#define _SCE_KERNEL_TYPES_H

#include <scetypes.h>

#if !defined(_LANGUAGE_ASSEMBLY)

/* sysmem: a memory block's type */
typedef SceUInt32		SceKernelMemBlockType;

typedef int			SceIoMode;
typedef int			SceMode;	/* Sony keeps it for compatibility */

#ifndef _SCE_KERNEL_IORES_T
#define _SCE_KERNEL_IORES_T
typedef SceInt64		SceIores;
#endif

/* the system time, in microseconds */
typedef union _SceKernelSysClock {
	struct {
		SceUInt32	low;
		SceUInt32	hi;
	} u;
	SceUInt64		quad;
} SceKernelSysClock;

#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* _SCE_KERNEL_TYPES_H */
