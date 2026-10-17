/* SceBbmc control access and heap wrappers. */
#include <sysmem/sysmem.h>
#include "bbmc_memory.h"

SceUID g_bbmcHeap;

/* name unknown: creates the heap used by the manager's allocation wrappers. */
int bbmcCreateHeap(SceSize size)
{
	int ret = SceSysmemForDriver_9328E0E8("SceBbmcMain", size, NULL);

	if (ret >= 0)
		g_bbmcHeap = ret;
	return ret;
}

/* name unknown: the stored heap UID is cleared even if deletion fails. */
int bbmcDeleteHeap(void)
{
	int ret = sceKernelDeleteHeap(g_bbmcHeap);

	g_bbmcHeap = 0;
	return ret;
}

/* name unknown: optional alignment is forwarded to the kernel allocator. */
void *bbmcAlloc(SceSize size, SceSize alignment)
{
	SceAllocOpt opt;
	SceAllocOpt *option = NULL;

	if (alignment != 0) {
		opt.size = sizeof(opt);
		opt.mmapKind = 0;
		opt.align = alignment;
		opt.mmapBase = NULL;
		opt.mmapSize = 0;
		option = &opt;
	}
	return SceSysmemForDriver_49D4DD9B(g_bbmcHeap, size, option);
}

/* name unknown: same heap-release wrapper used in usbd. */
int bbmcFree(void *ptr)
{
	return sceKernelFreeHeapMemory(g_bbmcHeap, ptr);
}
