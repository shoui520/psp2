#include "global_heap.h"
#include "stdlib.h"
#include "new.h"
#include "stdc++/atexit.h"
#include "../module.h"
#include "../memory/dlmalloc.h"

static HeapAllocator s_heap_storage;
static HeapAllocator *s_heap;
#ifdef PAF_STATIC_BUILD
extern SceUInt32 sce_paf_global_heap_size;
SceInt32 sce_paf_is_common_dialog_mode __attribute__((weak)) = 0;
#endif

void *sce_paf_malloc(size_t size)
{
	return HeapAllocatorMalloc(s_heap, size);
}

void *sce_paf_memalign(size_t align, size_t size)
{
	return HeapAllocatorMemalign(s_heap, align, size);
}

#ifndef PAF_STATIC_BUILD
void *_ZnwjPN3paf6memory13HeapAllocatorE(size_t size, HeapAllocator *allocator)
{
	return allocator ? HeapAllocatorMalloc(allocator, size) : NULL;
}

void *ScePafStdc_3CF19248(size_t size, HeapAllocator *allocator)
{
	return allocator ? HeapAllocatorMalloc(allocator, size) : NULL;
}

void *ScePafStdc_5B828DF7(size_t size, size_t alignment,
			  HeapAllocator *allocator)
{
	return allocator ? HeapAllocatorMemalign(allocator, alignment, size)
			 : NULL;
}

void *ScePafStdc_6FE032F8(size_t size, size_t alignment,
			  HeapAllocator *allocator)
{
	return allocator ? HeapAllocatorMemalign(allocator, alignment, size)
			 : NULL;
}

void ScePafStdc_D04CBFC0(void *ptr, HeapAllocator *allocator)
{
	if (allocator && ptr)
		HeapAllocatorFree(allocator, ptr);
}

void ScePafStdc_BE896EAF(void *ptr, HeapAllocator *allocator)
{
	if (allocator && ptr)
		HeapAllocatorFree(allocator, ptr);
}

void ScePafStdc_46A0F0D9(void *ptr, size_t unused1, size_t unused2,
			 HeapAllocator *allocator)
{
	if (allocator && ptr)
		HeapAllocatorFree(allocator, ptr);
}

void ScePafStdc_F22F2F50(void *ptr, size_t unused1, size_t unused2,
			 HeapAllocator *allocator)
{
	if (allocator && ptr)
		HeapAllocatorFree(allocator, ptr);
}
#endif

void *sce_paf_realloc(void *ptr, size_t size)
{
#ifdef PAF_STATIC_BUILD
	return HeapAllocatorRealloc(s_heap, ptr, size);
#else
	HeapAllocator *heap = ptr ?
		(HeapAllocator *)(uintptr_t)mspace_ptr_get_tag(ptr) : s_heap;

	return HeapAllocatorRealloc(heap, ptr, size);
#endif
}

void *sce_paf_calloc(size_t nitems, size_t size)
{
	return HeapAllocatorCalloc(s_heap, nitems, size);
}

void sce_paf_free(void *ptr)
{
	if (ptr) {
		HeapAllocator *heap = (HeapAllocator *)(uintptr_t)mspace_ptr_get_tag(ptr);

		HeapAllocatorFree(heap, ptr);
	}
}

void sce_paf_mtrim(void)
{
	HeapAllocatorTrim(s_heap);
}

size_t sce_paf_musable_size(const void *ptr)
{
#ifdef PAF_STATIC_BUILD
	return HeapAllocatorUsableSize(s_heap, ptr);
#else
	if (ptr) {
		HeapAllocator *heap = (HeapAllocator *)(uintptr_t)mspace_ptr_get_tag(ptr);

		return HeapAllocatorUsableSize(heap, ptr);
	}
	return 0;
#endif
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

#ifdef PAF_STATIC_BUILD
	if (sce_paf_is_common_dialog_mode != 0) {
		HeapAllocatorOption option;

		option.entry_max = 0;
		option.warning_size = 0;
		option.disable_dbgprint = false;
		option.disable_assert_on_alloc_failure = false;
		option.memblk_type = AllocationBlockType_User_Cdlg;
		option.alignment = 0;
		s_heap = HeapAllocatorConstruct(&s_heap_storage, NULL,
						sce_paf_global_heap_size,
						"ScePafGlobalHeap", &option);
	} else {
		s_heap = HeapAllocatorConstruct(&s_heap_storage, NULL,
						sce_paf_global_heap_size,
						"ScePafGlobalHeap", NULL);
	}
	return __aeabi_atexit(&s_heap_storage,
			(void (*)(void *))GlobalHeapDestruct, &__dso_handle);
#else
	GlobalHeapConstruct(&s_heap_storage);
	return __aeabi_atexit(&s_heap_storage,
			(void (*)(void *))GlobalHeapDestruct, &__dso_handle);
#endif
}

#ifndef PAF_STATIC_BUILD
HeapAllocator *GlobalHeapConstruct(HeapAllocator *self)
{
	AllocationBlockType type = sce_paf_is_common_dialog_mode ? AllocationBlockType_User_Cdlg :
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
		heap = HeapAllocatorConstruct(heap, NULL, global_heap_size,
				       "ScePafGlobalHeap", &opt);
	s_heap = heap;
	return self;
}
#endif

HeapAllocator *GlobalHeapDestruct(HeapAllocator *self)
{
	HeapAllocator *heap = s_heap;

	heap->vtable->Destruct(heap);
	return self;
}
