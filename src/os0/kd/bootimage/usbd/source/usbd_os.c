/*
 * SceUsbd: the kernel services the stack uses, in one place. All its small
 * allocations come from its own heap; the transfer buffers and the
 * controllers' descriptor memory are physically contiguous memory blocks.
 */

#include <psp2kern/kernel/sysclib.h>
#include <sce/sysmem.h>
#include "usbd_internal.h"

static SceUID s_heapId;

int usbdCreateHeap(void)
{
	SceKernelHeapCreateOpt opt;

	opt.size = sizeof(opt);
	opt.attr = HEAP_AUTO_EXTEND | HEAP_HAS_MAX_SIZE;
	opt.maxSize = 0x80000;
	s_heapId = ksceKernelCreateHeap("SceUsbd", 0x1000, &opt);
	return s_heapId;
}

int usbdDeleteHeap(void)
{
	return ksceKernelDeleteHeap(s_heapId);
}

/* 64-byte aligned, as the controllers' descriptors need */
void *usbdAlloc(SceSize size)
{
	SceAllocOpt opt;

	opt.size = sizeof(opt);
	opt.align = 64;
	return ksceKernelAllocHeapMemoryWithOption(s_heapId, size, &opt);
}

int usbdFree(void *ptr)
{
	return ksceKernelFreeHeapMemory(s_heapId, ptr);
}

SceUID usbdAllocMemBlock(SceSize size, void **base)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceUID blockId;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
	opt.alignment = 64;
	blockId = ksceKernelAllocMemBlock("SceUsbd", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW,
					  size, &opt);
	if (blockId >= 0)
		ksceKernelGetMemBlockBase(blockId, base);
	return blockId;
}

SceUIntPtr usbdVAtoPA(const void *va)
{
	uintptr_t pa;

	ksceKernelVAtoPA(va, &pa);
	return pa;
}

/* a short busy wait, for register settle times */
void usbdDelay(unsigned int count)
{
	volatile unsigned int i;

	for (i = 100 * count; i != 0; i--)
		;
}
