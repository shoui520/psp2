#include "cache_manager_impl.h"
#include "../std/stdc++/atexit.h"
#include "../std/stdlib.h"

RMutex CacheManagerImplRMutex;

typedef struct PafCacheSharedCounter {
	uint8_t reserved[16];
	uint32_t count;
} PafCacheSharedCounter;

int32_t ScePafMisc_E5B70924(PafCacheSharedCounter *counter)
{
	uint32_t count;

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	count = ++counter->count;
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
	return (int32_t)count;
}

void ScePafMisc_33ADBB0A(CacheManagerImpl *self, uint32_t flags)
{
	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	self->httpFileOpenFlags = flags;
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
}

void ScePafMisc_BF5194CD(CacheManagerImpl *self, uint8_t shutdown)
{
	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	self->shutdown = shutdown;
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
}

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
