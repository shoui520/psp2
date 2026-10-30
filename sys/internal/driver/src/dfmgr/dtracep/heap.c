/*
 * heap.c: DTrace's own kernel heap. Every allocation DTrace makes comes from
 * it, and a failed allocation panics. The heap's first block range is kept
 * so that probes can tell DTrace's own memory from the memory they read.
 */
#include <dtrace/heap.h>
#include <dtrace/logging.h>
#include <sysmem/debug.h>
#include <sysmem/heap_info.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>

#define DTRACE_HEAP_SIZE	0x1000000
#define DTRACE_HEAP_RANGES	16

static SceUID s_heapUid = -1;
static struct malloc_segment_range s_heapRange;

int dtrace_heap_init(void)
{
	struct malloc_segment_range ranges[DTRACE_HEAP_RANGES];
	SceKernelHeapCreateOpt opt;
	HeapInfo info;

	opt.size = sizeof(opt);
	opt.flags = HEAP_HAS_MEMTYPE;
	opt.memtype = 0x10f0d006;
	s_heapUid = sceKernelCreateHeap("SceDeci4pDtracep", DTRACE_HEAP_SIZE, &opt);
	if (s_heapUid < 0)
		return s_heapUid;

	info.size = sizeof(info);
	info.maxBlocks = DTRACE_HEAP_RANGES;
	info.blockList = ranges;
	if (sceKernelGetHeapInfo(s_heapUid, 5, &info) != SCE_OK)
		SCE_PANIC_AT(45);

	s_heapRange = info.blockList[0];
	return SCE_OK;
}

int dtrace_heap_fini(void)
{
	int res;

	res = sceKernelDeleteHeap(s_heapUid);
	if (res != SCE_OK)
		DTRACE_LOG_ERROR_AT(58, "failed to delete DTrace heap (0x%08x)", res);

	s_heapRange.base = NULL;
	s_heapRange.size = 0;
	s_heapUid = -1;
	return SCE_OK;
}

void *dtrace_heap_alloc(size_t size)
{
	void *ptr;

	ptr = sceKernelAllocHeapMemory(s_heapUid, size);
	if (ptr == NULL)
		SCE_PANIC_AT(74);

	return ptr;
}

void *dtrace_heap_alloc_aligned(size_t size, size_t alignment)
{
	SceAllocOpt opt;
	void *ptr;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = alignment;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	ptr = sceKernelAllocHeapMemoryWithOption(s_heapUid, size, &opt);
	if (ptr == NULL)
		SCE_PANIC_AT(96);

	return ptr;
}

void *dtrace_heap_zalloc(size_t size)
{
	void *ptr;

	ptr = dtrace_heap_alloc(size);
	if (size != 0)
		memset(ptr, 0, size);

	return ptr;
}

void *dtrace_heap_zalloc_nopanic(size_t size)
{
	void *ptr;

	ptr = sceKernelAllocHeapMemory(s_heapUid, size);
	if (ptr != NULL && size != 0)
		memset(ptr, 0, size);

	return ptr;
}

void dtrace_heap_free(void *ptr)
{
	sceKernelFreeHeapMemory(s_heapUid, ptr);
}

/* 0 when [ptr, ptr + size) overlaps the heap's memory, else 1 */
int dtrace_heap_contains(const void *ptr, size_t size)
{
	uintptr_t base = (uintptr_t)s_heapRange.base;

	if ((uintptr_t)ptr - base < s_heapRange.size)
		return 0;

	return base - (uintptr_t)ptr >= size;
}
