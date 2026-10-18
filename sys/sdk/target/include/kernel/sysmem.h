/*
 * kernel/sysmem.h: the user memory interface, under Sony's names and layouts (SDK Release
 * 03.570.011, target/include/kernel/sysmem.h).
 */

#pragma once

#include <kernel/sysmem/memblock.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sceKernelGetFreeMemorySize */
typedef struct SceKernelFreeMemorySizeInfo {
	SceSize size;				/* sizeof(SceKernelFreeMemorySizeInfo) */
	SceSize sizeMain;
	SceSize sizeCdram;
	SceSize sizePhycont;
} SceKernelFreeMemorySizeInfo;

int sceKernelGetFreeMemorySize(SceKernelFreeMemorySizeInfo *pInfo);

#ifdef __cplusplus
}
#endif
