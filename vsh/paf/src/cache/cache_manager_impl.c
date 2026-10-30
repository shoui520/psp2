#include "cache_manager_impl.h"
#include "../std/stdc++/atexit.h"
#include "../std/stdlib.h"
#include "../job/job.h"

RMutex CacheManagerImplRMutex;

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

int ScePafMisc_5A7093DE(PafCachePendingJob *self)
{
	PafCacheGate *gate;
	int result = 0;

	if (!self)
		return 0;
	gate = self->gate;
	if (!gate ||
	    ((!gate->check || !(result = gate->check(gate->context))) &&
	     !(result = gate->blocked))) {
		JobItem *item = self->item;

		if (item)
			return JobItemIsCanceled(item);
	}
	return result;
}

__attribute__((noinline)) void ScePafMisc_511CFBA2(CacheManagerImpl *self, void *ptr)
{
	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	if (self->longLifeHeap)
		scePafFreeWithContext(self->longLifeHeap, ptr);
	else
		sce_paf_free(ptr);
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
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

void ScePafMisc_CB20D5B3(CacheManagerImpl *self)
{
	PafCacheTransient *state = (PafCacheTransient *)self;

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	if (state->allocation) {
		if (self->longLifeHeap)
			ScePafMisc_511CFBA2((CacheManagerImpl *)self->longLifeHeap,
					     state->allocation);
		state->allocation = 0;
	}
	state->timestampLow = 0;
	state->timestampHigh = 0;
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
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

uint32_t ScePafMisc_8A35B3D9(PafCacheRefObject *self)
{
	uint32_t references;

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	references = --self->references;
	if (!references) {
		self->methods->onZeroReferences(self);
		ScePafThread_95F9C45F(&CacheManagerImplRMutex);
		return 0;
	}
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
	return references;
}

typedef struct PafCacheCallbackContext {
	uint8_t callbackState[144];
	int (*callback)(void *callbackState, void *user, void *data);
	void *data;
} PafCacheCallbackContext;

_Static_assert(offsetof(PafCacheCallbackContext, callback) == 144,
	       "cache callback pointer offset");
_Static_assert(offsetof(PafCacheCallbackContext, data) == 148,
	       "cache callback data offset");

int ScePafMisc_F47E07B4(void *user, PafCacheCallbackContext *context)
{
	int result = 0;

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	if (context->callback) {
		int callbackResult = context->callback(context->callbackState + 32,
						 user, context->data);

		if (callbackResult < 0)
			result = callbackResult;
	}
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
	return result;
}

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
