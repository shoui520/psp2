/* GPU process-memory contexts. Verified meanings use native offset evidence. */
#include <stdint.h>
#include <stddef.h>
#include "../../gpu_es4/srvkm/gpu-mmu.h"
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysclib.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/memory/memorycontext.c"

extern void *AllocProcessGpuMemory(unsigned size);
extern int DestroyGpuHeap(GpuHeap *heap);
int FreeProcessGpuMemory(void *memory);
int DestroyGpuMmuContext(GpuMmuContext *context);
int sceGpuMmuUnmapMemory(GpuMmuContext *context, unsigned address,
		unsigned size);

GpuMemoryContext *kernelMemoryContext;
GpuMemoryContext *processMemoryContexts;

int sceGpuInitMemoryContexts(void)
{
	GpuMemoryContext *context;
	unsigned pid;

	SCE_DBG_ASSERT_FUNC(kernelMemoryContext == 0, 147, "sceGpuMmuUnmapMemory");
	SCE_DBG_ASSERT_FUNC(processMemoryContexts == 0, 148, "sceGpuMmuUnmapMemory");
	pid = sceKernelGetProcessId();
	context = AllocProcessGpuMemory(0x1028);
	if (!context)
		return (int)0x804c0001;
	memset(context, 0, 0x1028);
	context->pid = pid;
	context->mmu.pageTablePool.uid = -1;
	kernelMemoryContext = context;
	return 0;
}

/* Names unknown for these small native lookup/accessor helpers. */
GpuMemoryContext *FindMemoryContext(unsigned pid)
{
	GpuMemoryContext *context;

	for (context = processMemoryContexts; context; context = context->next) {
		if (context->pid == pid)
			break;
	}
	return context;
}

int AddMemoryContextToList(GpuMemoryContext **output, unsigned pid)
{
	GpuMemoryContext *context, *previous;

	if (!output)
		return (int)0x804c0000;
	if (FindMemoryContext(pid))
		return (int)0x804c0004;
	context = AllocProcessGpuMemory(sizeof(*context));
	if (!context)
		return (int)0x804c0001;
	memset(context, 0, sizeof(*context));
	context->mmu.pageTablePool.uid = -1;
	context->pid = pid;
	SCE_DBG_ASSERT(1, 97);
	previous = processMemoryContexts;
	processMemoryContexts = context;
	context->next = previous;
	*output = context;
	return 0;
}

/* Volatile accesses retain native rereads after potentially aliased stores. */
int AddGpuHeapToMemoryContext(GpuMemoryContext *context, GpuHeap *heap)
{
	volatile GpuMemoryContext *memoryContext = context;
	volatile GpuHeap *newHeap = heap;
	GpuHeap *first;

	if (!context)
		return (int)0x804c0000;
	newHeap->memoryContext = context;
	newHeap->next = memoryContext->heaps;
	newHeap->previous = 0;
	first = memoryContext->heaps;
	if (first)
		((volatile GpuHeap *)first)->previous = heap;
	memoryContext->heaps = heap;
	return 0;
}

int RemoveMemoryContextFromList(GpuMemoryContext *context, unsigned pid)
{
	GpuHeap *heap, *nextHeap;
	GpuHeapSpan *span;
	GpuMemoryContext *current, *previous = NULL;

	if (!context || context != FindMemoryContext(pid))
		return (int)0x804c0000;
	for (heap = context->heaps; heap; heap = nextHeap) {
		for (span = heap->spans; span; span = span->next)
			sceGpuMmuUnmapMemory(&context->mmu, span->base, span->size);
		nextHeap = heap->next;
		DestroyGpuHeap(heap);
	}
	context->heaps = NULL;
	DestroyGpuMmuContext(&context->mmu);
	SCE_DBG_ASSERT(1, 107);
	for (current = processMemoryContexts; current && current != context;
	     current = current->next)
		previous = current;
	SCE_DBG_ASSERT(current != NULL, 117);
	if (previous)
		previous->next = context->next;
	else
		processMemoryContexts = context->next;
	FreeProcessGpuMemory(context);
	return 0;
}

int FindGpuHeapInMemoryContext(GpuMemoryContext *context, unsigned lookupKey,
			     GpuHeap **output)
{
	GpuHeap *heap;

	if (!context || !context->heaps)
		return (int)0x804c0000;
	for (heap = context->heaps; heap; heap = heap->next) {
		if (heap->lookupKey != lookupKey)
			continue;
		*output = heap;
		return 0;
	}
	return (int)0x804c0005;
}

int RemoveGpuHeapFromMemoryContext(GpuMemoryContext *context, GpuHeap *heap)
{
	volatile GpuHeap *removed = heap;
	GpuHeap *previous, *next;

	if (!context || !heap)
		return (int)0x804c0000;
	next = removed->next;
	if (next)
		((volatile GpuHeap *)next)->previous = removed->previous;
	previous = removed->previous;
	next = removed->next;
	if (previous)
		((volatile GpuHeap *)previous)->next = next;
	else
		((volatile GpuMemoryContext *)context)->heaps = next;
	DestroyGpuHeap(heap);
	return 0;
}

int GetMemoryContext(GpuMemoryContext **output, unsigned pid)
{
	GpuMemoryContext *context;

	if (!output)
		return (int)0x804c0000;
	context = FindMemoryContext(pid);
	if (!context)
		return (int)0x804c0005;
	*output = context;
	return 0;
}

int GetKernelMemoryContext(GpuMemoryContext **output)
{
	if (!output)
		return (int)0x804c0000;
	if (!kernelMemoryContext)
		return (int)0x804c0005;
	*output = kernelMemoryContext;
	return 0;
}

/* Native internal names unavailable. The allocated page directory is 4 KiB.
 * The native option setup is retained even though allocation receives NULL.
 */
int AllocateGpuPageDirectory(GpuMmuContext *context)
{
	volatile SceKernelAllocMemBlockKernelOpt options;
	void *base;
	uintptr_t physical;
	int uid, ret;

	if (!context)
		return (int)0x804c0000;
	memset((void *)&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID;
	options.pid = sceKernelGetProcessId();
	uid = sceKernelAllocMemBlockForKernel("SceGpuPageDirectory",
				    SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW,
				    0x1000, 0);
	if (uid < 0)
		return uid;
	ret = sceKernelGetMemBlockBase(uid, &base);
	if (ret >= 0)
		ret = sceKernelVAtoPA(base, &physical);
	if (ret < 0) {
		SceSysmemForDriver_009E1C61(uid);
		return ret;
	}
	memset(base, 0, 0x1000);
	context->pageDirectoryMemblockUid = uid;
	context->pageDirectory = base;
	context->pageDirectoryPhysicalAddress = physical;
	return 0;
}

int FreeGpuPageDirectory(GpuMmuContext *context)
{
	if (!context)
		return (int)0x80020005;
	context->pageDirectory = 0;
	if (context->pageDirectoryMemblockUid != (unsigned)-1) {
		SceSysmemForDriver_009E1C61(context->pageDirectoryMemblockUid);
		context->pageDirectoryMemblockUid = -1;
	}
	context->pageDirectoryPhysicalAddress = 0;
	return 0;
}

int sceGpuMmuShareRange(GpuMmuContext *destination, GpuMmuContext *source,
		unsigned address, unsigned size);

int ShareKernelMappingWithProcesses(unsigned address, unsigned size)
{
	GpuMemoryContext *context;

	for (context = processMemoryContexts; context; context = context->next)
		sceGpuMmuShareRange(&context->mmu, &kernelMemoryContext->mmu,
				address, size);
	return 0;
}

int SceGpuEs4ForDriver_7AA79658(unsigned physical, unsigned *pid)
{
	GpuMemoryContext *context;

	if (!physical || !pid)
		return (int)0x804c0000;
	if (kernelMemoryContext &&
	    kernelMemoryContext->mmu.pageDirectoryPhysicalAddress == physical) {
		*pid = 0x10005;
		return 0;
	}
	for (context = processMemoryContexts; context; context = context->next) {
		if (context->mmu.pageDirectoryPhysicalAddress == physical) {
			*pid = context->pid;
			return 0;
		}
	}
	return (int)0x804c0005;
}
