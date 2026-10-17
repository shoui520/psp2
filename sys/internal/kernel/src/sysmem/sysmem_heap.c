/*
 * SceSysmem: name copies and the kernel heap's locked entry points. Sony's
 * file name is unknown; it is a file of its own, as SysmemModuleStart calls
 * SysmemHeapModuleStart rather than inlining it.
 */

#include "sysmem_internal.h"

uint32_t SceSysrootForKernel_5C426B19(void);
void *sceKernelSysrootAlloc(SceSize size);
char *strncpy(char *dst, const char *src, SceSize n);

/* name unknown: a 0x20-byte copy of a name, from the sysroot heap while it boots */
char *SysmemAllocName(const char *name)
{
	char *copy;

	if (SceSysrootForKernel_5C426B19() < 3)
		copy = sceKernelSysrootAlloc(0x20);
	else
		copy = SceSysmemForKernel_85571907(0x20);
	strncpy(copy, name, 0x20);
	return copy;
}

/* name unknown: such a copy given back (the sysroot heap's are kept) */
void SysmemFreeName(char *name)
{
	if (SceSysrootForKernel_5C426B19() > 2)
		SceSysmemForKernel_4233C16D(name);
}

/* name unknown: an allocation from a heap, under its lock */
void *HeapAllocLocked(void *heap, SceSize size)
{
	int intr = HeapLock(heap);
	void *ptr = HeapAllocPrologue(heap, size, 0);

	HeapUnlock(heap, intr);
	return ptr;
}

/* name unknown: the same for a free */
int HeapFreeLocked(void *heap, void *ptr)
{
	int intr = HeapLock(heap);
	int ret = HeapFree(heap, ptr);

	HeapUnlock(heap, intr);
	return ret;
}

/* name unknown */
void *UIDManagerGetKernelHeap(void)
{
	return g_uidManager->heapDefault;
}

/* name unknown */
int SysmemHeapModuleStart(const UIDManager *manager)
{
	HeapModuleStart(manager);
	return 0;
}
