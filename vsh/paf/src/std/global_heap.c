#include "global_heap.h"
#include "stdlib.h"
#include "new.h"
#include "stdc++/atexit.h"
#include "../module.h"

static HeapAllocator s_heap_storage;
static HeapAllocator *s_heap;

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
