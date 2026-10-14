/*
 * reg_memory.c: the reg library's "SceRegMemBlock" heap, the hooks a port
 * would fill (empty in the kernel) and the name hash. All names unknown.
 */

#include <psp2kern/kernel/sysmem/heap.h>
#include "reg_internal.h"

#define REG_NAME_LEN	0x1a	/* the longest name hashed */
#define REG_HASH_MOD	0x4de1

SceUID g_regHeap = -1;		/* name unknown */

/* create the heap the reg library allocates from */
int regMemInit(int unused, SceSize size)
{
	SceKernelHeapCreateOpt opt;
	SceUID heap;

	opt.size = sizeof(opt);
	opt.uselock = 1;
	g_regHeap = -1;
	heap = ksceKernelCreateHeap("SceRegMemBlock", size, &opt);
	if (heap > 0)
		g_regHeap = heap;

	return heap;
}

void *regMemAlloc(SceSize size)
{
	return ksceKernelAllocHeapMemory(g_regHeap, size);
}

void regMemFree(void *ptr)
{
	ksceKernelFreeHeapMemory(g_regHeap, ptr);
}

/* the hooks a port fills: a lock and pointer checks (empty here) */
int regPlatformLock(void)
{
	return 0;
}

void regPlatformUnlock(int state)
{
}

const void *regPlatformCheckPointer(const void *ptr)
{
	return ptr;
}

const void *regPlatformCheckBuffer(const void *buf, int size)
{
	return buf;
}

/* hash a name into a table of size slots */
int regHashName(const SceChar8 *name, int size, int *hash)
{
	int index = 0;
	int h = 0;
	int len;
	int i;

	len = strnlen((const char *)name, REG_NAME_LEN);
	if (len > 0) {
		for (i = 1; i <= len; i += 2)
			h = (name[0] * 100 + h + name[i]) % REG_HASH_MOD;
		index = h % size;
	}

	if (hash != NULL)
		*hash = h;

	return index;
}
