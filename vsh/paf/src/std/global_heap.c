#include "global_heap.h"
#include "stdlib.h"
#include "new.h"
#include "stdc++/atexit.h"
#include "../module.h"
#include "../memory/dlmalloc.h"

static HeapAllocator s_heap_storage;
static HeapAllocator *s_heap;

void *sce_paf_malloc(size_t size)
{
	return scePafMallocWithContext(s_heap, size);
}

void *sce_paf_memalign(size_t align, size_t size)
{
	return scePafMallocAlignWithContext(s_heap, align, size);
}

void *ScePafStdc_D00387E7(size_t size, HeapAllocator *allocator)
{
	return allocator ? scePafMallocWithContext(allocator, size) : NULL;
}

void *ScePafStdc_3CF19248(size_t size, HeapAllocator *allocator)
{
	return allocator ? scePafMallocWithContext(allocator, size) : NULL;
}

void *ScePafStdc_5B828DF7(size_t size, size_t alignment,
			  HeapAllocator *allocator)
{
	return allocator ? scePafMallocAlignWithContext(allocator, alignment, size)
			 : NULL;
}

void *ScePafStdc_6FE032F8(size_t size, size_t alignment,
			  HeapAllocator *allocator)
{
	return allocator ? scePafMallocAlignWithContext(allocator, alignment, size)
			 : NULL;
}

void ScePafStdc_D04CBFC0(void *ptr, HeapAllocator *allocator)
{
	if (allocator && ptr)
		scePafFreeWithContext(allocator, ptr);
}

void ScePafStdc_BE896EAF(void *ptr, HeapAllocator *allocator)
{
	if (allocator && ptr)
		scePafFreeWithContext(allocator, ptr);
}

void ScePafStdc_46A0F0D9(void *ptr, size_t unused1, size_t unused2,
			 HeapAllocator *allocator)
{
	if (allocator && ptr)
		scePafFreeWithContext(allocator, ptr);
}

void ScePafStdc_F22F2F50(void *ptr, size_t unused1, size_t unused2,
			 HeapAllocator *allocator)
{
	if (allocator && ptr)
		scePafFreeWithContext(allocator, ptr);
}

void *sce_paf_realloc(void *ptr, size_t size)
{
	HeapAllocator *heap = ptr ?
		(HeapAllocator *)(uintptr_t)mspace_ptr_get_tag(ptr) : s_heap;

	return scePafReallocWithContext(heap, ptr, size);
}

void *sce_paf_calloc(size_t nitems, size_t size)
{
	return HeapAllocatorCalloc(s_heap, nitems, size);
}

void sce_paf_free(void *ptr)
{
	if (ptr) {
		HeapAllocator *heap = (HeapAllocator *)(uintptr_t)mspace_ptr_get_tag(ptr);

		scePafFreeWithContext(heap, ptr);
	}
}

void sce_paf_mtrim(void)
{
	HeapAllocatorTrim(s_heap);
}

size_t sce_paf_musable_size(const void *ptr)
{
	if (ptr) {
		HeapAllocator *heap = (HeapAllocator *)(uintptr_t)mspace_ptr_get_tag(ptr);

		return HeapAllocatorUsableSize(heap, ptr);
	}
	return 0;
}

void sce_paf_dump_heap_info(int32_t sort_policy)
{
	HeapAllocatorDumpHeapInfo(s_heap, sort_policy);
}

void sce_paf_dump_heap_status(void)
{
	HeapAllocatorDumpHeapStatus(s_heap);
}

int32_t sce_paf_report_memoryblocks(
		int (*callback)(void *, size_t, int, void *), void *userdata)
{
	return HeapAllocatorReportMemoryBlocks(s_heap, callback, userdata);
}

int32_t sce_paf_check_memorybounds(void)
{
	return HeapAllocatorCheckMemoryBoundsAll(s_heap);
}

void *GetGlobalHeapAllocator(void)
{
	return s_heap;
}

int GlobalHeapInitialize(void)
{
	extern void *__dso_handle;

	GlobalHeapConstruct(&s_heap_storage);
	return ScePafStdc_C95732BB(&s_heap_storage,
			(void (*)(void *))GlobalHeapDestruct, &__dso_handle);
}

HeapAllocator *GlobalHeapConstruct(HeapAllocator *self)
{
	AllocationBlockType type = cdlg_mode ? AllocationBlockType_User_Cdlg :
					     AllocationBlockType_User;
	int disable_assert = global_heap_disable_assert_on_alloc_failure;
	int alignment = global_heap_alignment;
	HeapAllocatorOption opt;
	HeapAllocator *heap;

	opt.entry_max = 0;
	opt.warning_size = 0;
	opt.disable_dbgprint = false;
	opt.disable_assert_on_alloc_failure = disable_assert != 0;
	opt.memblk_type = type;
	opt.alignment = alignment;
	heap = PafPlacementNew(sizeof(*self), self);
	if (heap)
		heap = scePafCreateHeap(heap, NULL, global_heap_size,
				       "ScePafGlobalHeap", &opt);
	s_heap = heap;
	return self;
}

HeapAllocator *GlobalHeapDestruct(HeapAllocator *self)
{
	HeapAllocator *heap = s_heap;

	heap->vtable->Destruct(heap);
	return self;
}
