/*
 * Temporary buffers allocated from the module's motion heap.
 */
#include <sce/sysmem.h>
#include <sce/motion_dev.h>

SceUID motionHeap;

void *motionAllocHeap(SceSize size)
{
	SceAllocOpt opt;

	opt.size = sizeof(opt);
	opt.align = 32;
	return ksceKernelAllocHeapMemoryWithOption(motionHeap, size, &opt);
}

int motionFreeHeap(void *ptr)
{
	return ksceKernelFreeHeapMemory(motionHeap, ptr);
}
