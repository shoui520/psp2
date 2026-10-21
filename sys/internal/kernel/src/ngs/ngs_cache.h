/* Data cache maintenance for buffers the codec engine reads. */

#pragma once

#include <stdint.h>
#include <sysmem/cpu.h>

/*
 * Cleans the cache lines that hold [address, address + size). The player
 * buffer preparation inlines it; ngs_atomic.c holds the function the other
 * files call.
 */
inline void ngsCleanDcacheRange(void *address, int size)
{
	uintptr_t start = (uintptr_t)address;

	size += start & 31;
	start &= ~31U;
	size = (size + 31) & ~31U;
	if (size > 0 && start != 0)
		sceKernelDcacheCleanRange((void *)start, size);
}
