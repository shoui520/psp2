#ifndef PAF_CACHE_CACHE_MANAGER_IMPL_H
#define PAF_CACHE_CACHE_MANAGER_IMPL_H

#include "../thread/mutex.h"
#include "../memory/heap_allocator.h"
#include <stddef.h>

#define CacheManagerImplRMutex ScePafMisc_DC169B6B
extern RMutex CacheManagerImplRMutex;

/* Prefix of paf::cache::CacheManagerImpl, including its C++ virtual table.
 * Native cache_manager_impl.h identifies the heap and alignment members. */
typedef struct CacheManagerImpl {
	const void *vtable;
	HeapAllocator *longLifeHeap;
	HeapAllocator *instantHeap;
	HeapAllocator *cacheInstanceHeap;
	size_t longLifeAlignment;
	size_t instantAlignment;
} CacheManagerImpl;

_Static_assert(offsetof(CacheManagerImpl, instantHeap) == 8,
	       "CacheManagerImpl instant heap offset");
_Static_assert(offsetof(CacheManagerImpl, instantAlignment) == 20,
	       "CacheManagerImpl instant alignment offset");

#define CacheManagerImplAllocInstantHeap ScePafMisc_049AEA94
#define CacheManagerImplFreeInstantHeap ScePafMisc_051B5051

void *CacheManagerImplAllocInstantHeap(CacheManagerImpl *self, size_t size);
void CacheManagerImplFreeInstantHeap(CacheManagerImpl *self, void *ptr);

int CacheManagerImplInitialize(void);

#endif
