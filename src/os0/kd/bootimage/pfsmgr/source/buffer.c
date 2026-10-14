/*
 * Fixed-size buffers carved out of one memory block, kept on a free list.
 */

#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include "pfsmgr_internal.h"

#define PFS_BUFFER_MEMBLOCK_TYPE	0x1020d006	/* name unknown */
#define PFS_BUFFER_MEMBLOCK_ALIGN	0x1000

int pfsBufferListInit(ScePfsBufferList *list, const char *name, SceSize elementSize,
		      SceSize align, SceUInt32 count)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceSize stride, total;
	void **base, **element, **prev;
	SceUInt32 i;
	int ret;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.alignment = PFS_BUFFER_MEMBLOCK_ALIGN;
	list->mutex = ksceKernelCreateMutex(name, PFS_MUTEX_ATTR, 0, NULL);
	if (list->mutex < 0)
		return list->mutex;

	stride = elementSize + (align - elementSize % align) % align;
	total = count * stride;
	list->memBlock = ksceKernelAllocMemBlock(name, PFS_BUFFER_MEMBLOCK_TYPE,
						 total + (-total & (PFS_BUFFER_MEMBLOCK_ALIGN - 1)),
						 &opt);
	if (list->memBlock < 0) {
		ret = SCE_PFS_ERROR_NO_MEMORY;
		goto deleteMutex;
	}
	ret = ksceKernelGetMemBlockBase(list->memBlock, (void **)&base);
	if (ret != 0) {
		ksceKernelFreeMemBlock(list->memBlock);
		goto deleteMutex;
	}

	list->freeList = NULL;
	if (count != 0) {
		prev = NULL;
		element = base;
		for (i = 0; i < count; i++) {
			*element = prev;
			prev = element;
			element = (void **)((char *)element + stride);
		}
		list->freeList = (char *)base + (total - stride);
	}
	list->name = name;
	list->count = count;
	list->elementSize = elementSize;
	return 0;

deleteMutex:
	ksceKernelDeleteMutex(list->mutex);
	return ret;
}

void pfsBufferListTerm(ScePfsBufferList *list)
{
	ksceKernelFreeMemBlock(list->memBlock);
	ksceKernelDeleteMutex(list->mutex);
}

void *pfsBufferListAlloc(ScePfsBufferList *list)
{
	void **element;

	ksceKernelLockMutex(list->mutex, 1, NULL);
	element = list->freeList;
	if (element != NULL)
		list->freeList = *element;
	ksceKernelUnlockMutex(list->mutex, 1);
	return element;
}

void pfsBufferListFree(ScePfsBufferList *list, void *element)
{
	ksceKernelLockMutex(list->mutex, 1, NULL);
	*(void **)element = list->freeList;
	list->freeList = element;
	ksceKernelUnlockMutex(list->mutex, 1);
}
