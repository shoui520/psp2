#include "cache_manager_impl.h"
#include "../std/stdc++/atexit.h"
#include "../std/stdlib.h"

RMutex CacheManagerImplRMutex;

void *CacheManagerImplAllocInstantHeap(CacheManagerImpl *self, size_t size)
{
	HeapAllocator *heap;
	void *allocated;

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	heap = self->instantHeap;
	if (heap)
		allocated = scePafMallocAlignWithContext(heap, self->instantAlignment, size);
	else
		allocated = sce_paf_memalign(self->instantAlignment, size);
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
	return allocated;
}

void CacheManagerImplFreeInstantHeap(CacheManagerImpl *self, void *ptr)
{
	HeapAllocator *heap;

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	heap = self->instantHeap;
	if (heap)
		scePafFreeWithContext(heap, ptr);
	else
		sce_paf_free(ptr);
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
}

int CacheManagerImplInitialize(void)
{
	extern void *__dso_handle;

	ScePafThread_C4BD9753(&CacheManagerImplRMutex, "ScePafCacheRmutex",
			    MutexOption_None);
	return ScePafStdc_C95732BB(&CacheManagerImplRMutex,
				(void (*)(void *))ScePafThread_8CCECAC9,
				&__dso_handle);
}
