#include "cache_manager_impl.h"
#include "../std/stdc++/atexit.h"
#include "../std/stdlib.h"
#include "../job/job.h"

RMutex CacheManagerImplRMutex;
extern uint32_t sce_paf_misc_counter_c224;

int ScePafMisc_5F18F5F7(const uint8_t *self)
{
	int result;
	const uint8_t *state;

	_ZN3paf6thread6RMutex4LockEv(&CacheManagerImplRMutex);
	state = *(const uint8_t *const *)(self + 4);
	if (state)
		result = state[65];
	else
		result = 1;
	_ZN3paf6thread6RMutex6UnlockEv(&CacheManagerImplRMutex);
	return result;
}

typedef struct PafCacheGate {
	uint8_t reserved[48];
	int (*check)(void *context);
	void *context;
	uint8_t reserved2[9];
	uint8_t blocked;
} PafCacheGate;

typedef struct PafCachePendingJob {
	uint32_t reserved;
	PafCacheGate *gate;
	uint8_t reserved2[20];
	JobItem *item;
} PafCachePendingJob;

_Static_assert(offsetof(PafCacheGate, check) == 48, "cache gate callback offset");
_Static_assert(offsetof(PafCacheGate, blocked) == 65, "cache gate flag offset");
_Static_assert(offsetof(PafCachePendingJob, item) == 28,
	       "cache pending job offset");

__attribute__((noinline)) void ScePafMisc_511CFBA2(CacheManagerImpl *self, void *ptr)
{
	_ZN3paf6thread6RMutex4LockEv(&CacheManagerImplRMutex);
	if (self->longLifeHeap)
		HeapAllocatorFree(self->longLifeHeap, ptr);
	else
		sce_paf_free(ptr);
	_ZN3paf6thread6RMutex6UnlockEv(&CacheManagerImplRMutex);
}

/* Native cache transaction state following the manager's allocator prefix. */
typedef struct PafCacheTransient {
	uint8_t prefix[44];
	void *allocation;
	uint32_t timestampLow;
	uint32_t timestampHigh;
} PafCacheTransient;

_Static_assert(offsetof(PafCacheTransient, allocation) == 44,
	       "cache transient allocation offset");
_Static_assert(offsetof(PafCacheTransient, timestampLow) == 48,
	       "cache transient timestamp offset");

/* Two transaction-state variants use the same guarded allocation cleanup. */
static inline __attribute__((always_inline))
void ClearCacheTransient(CacheManagerImpl *self)
{
	PafCacheTransient *state = (PafCacheTransient *)self;

	_ZN3paf6thread6RMutex4LockEv(&CacheManagerImplRMutex);
	if (state->allocation) {
		if (self->longLifeHeap)
			ScePafMisc_511CFBA2((CacheManagerImpl *)self->longLifeHeap,
					     state->allocation);
		state->allocation = 0;
	}
	state->timestampLow = 0;
	state->timestampHigh = 0;
	_ZN3paf6thread6RMutex6UnlockEv(&CacheManagerImplRMutex);
}

typedef struct PafCacheRefObject PafCacheRefObject;
typedef struct PafCacheRefMethods {
	void *reserved[3];
	void (*onZeroReferences)(PafCacheRefObject *self);
} PafCacheRefMethods;

struct PafCacheRefObject {
	const PafCacheRefMethods *methods;
	uint8_t reserved[12];
	uint32_t references;
};

_Static_assert(offsetof(PafCacheRefObject, references) == 16,
	       "reference count offset");

typedef struct PafCacheSharedCounter {
	uint8_t reserved[16];
	uint32_t count;
} PafCacheSharedCounter;

void ScePafMisc_BF5194CD(CacheManagerImpl *self, uint8_t shutdown)
{
	_ZN3paf6thread6RMutex4LockEv(&CacheManagerImplRMutex);
	self->shutdown = shutdown;
	_ZN3paf6thread6RMutex6UnlockEv(&CacheManagerImplRMutex);
}

void *CacheManagerImplAllocInstantHeap(CacheManagerImpl *self, size_t size)
{
	HeapAllocator *heap;
	void *allocated;

	_ZN3paf6thread6RMutex4LockEv(&CacheManagerImplRMutex);
	heap = self->instantHeap;
	if (heap)
		allocated = HeapAllocatorMemalign(heap, self->instantAlignment, size);
	else
		allocated = sce_paf_memalign(self->instantAlignment, size);
	_ZN3paf6thread6RMutex6UnlockEv(&CacheManagerImplRMutex);
	return allocated;
}

void CacheManagerImplFreeInstantHeap(CacheManagerImpl *self, void *ptr)
{
	HeapAllocator *heap;

	_ZN3paf6thread6RMutex4LockEv(&CacheManagerImplRMutex);
	heap = self->instantHeap;
	if (heap)
		HeapAllocatorFree(heap, ptr);
	else
		sce_paf_free(ptr);
	_ZN3paf6thread6RMutex6UnlockEv(&CacheManagerImplRMutex);
}

int CacheManagerImplInitialize(void)
{
	extern void *__dso_handle;

	_ZN3paf6thread6RMutexC1EPKci(&CacheManagerImplRMutex, "ScePafCacheRmutex",
			    MutexOption_None);
	return ScePafStdc_C95732BB(&CacheManagerImplRMutex,
				(void (*)(void *))_ZN3paf6thread6RMutexD1Ev,
				&__dso_handle);
}
