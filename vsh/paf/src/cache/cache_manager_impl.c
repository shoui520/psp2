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

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	state = *(const uint8_t *const *)(self + 4);
	if (state)
		result = state[65];
	else
		result = 1;
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
	return result;
}

void ScePafMisc_F4B797C2(CacheManagerImpl *self)
{
	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	self->httpFileOpenFlags = sce_paf_misc_counter_c224;
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
}

int ScePafMisc_3E898511(CacheManagerImpl *self)
{
	int result = 0;

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	if (self->longLifeHeap && ((uint8_t *)self)[81] &&
	    ((int (*)(CacheManagerImpl *))((const void **)self->vtable)[5])(self) >= 0) {
		ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
		self->httpFileOpenFlags = sce_paf_misc_counter_c224;
		ScePafThread_95F9C45F(&CacheManagerImplRMutex);
		result = *(int32_t *)((uint8_t *)self + 44);
	}
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
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

void ScePafMisc_5D7F00C2(void *allocation, const uint8_t *owner)
{
	const uint8_t *heap = *(const uint8_t *const *)(owner + 4);

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	heap = *(const uint8_t *const *)(owner + 4);
	if (heap && !heap[66])
		ScePafMisc_511CFBA2((CacheManagerImpl *)heap, allocation);
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

/* Two transaction-state variants use the same guarded allocation cleanup. */
static inline __attribute__((always_inline))
void ClearCacheTransient(CacheManagerImpl *self)
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

void ScePafMisc_297A9338(CacheManagerImpl *self)
{
	ClearCacheTransient(self);
}

void ScePafMisc_AD1073C1(CacheManagerImpl *self)
{
	ClearCacheTransient(self);
}

int ScePafMisc_E2F15E2D(CacheManagerImpl *self, int32_t status,
			 uint8_t *completed)
{
	uint8_t *bytes = (uint8_t *)self;
	int result;

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	if (self->longLifeHeap) {
		*(int32_t *)(bytes + 12) = status;
		if (status < 0) {
			int32_t *retryCount = (int32_t *)(bytes + 84);

			if (status == -2135994334 || status == -2135994332) {
				*retryCount = 0;
			} else {
				int32_t remaining = *retryCount;

				if (remaining > 0) {
					remaining--;
					*retryCount = remaining;
				}
				if (!*(uint32_t *)(bytes + 88) || remaining == 0)
					*(int32_t *)(bytes + 12) = -2135993294;
			}
			ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
			if (*(void **)(bytes + 44)) {
				if (self->longLifeHeap)
					ScePafMisc_511CFBA2(
						(CacheManagerImpl *)self->longLifeHeap,
						*(void **)(bytes + 44));
				*(void **)(bytes + 44) = 0;
			}
			*(uint32_t *)(bytes + 48) = 0;
			*(uint32_t *)(bytes + 52) = 0;
			ScePafThread_95F9C45F(&CacheManagerImplRMutex);
		}
		*completed = 1;
		ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
		self->httpFileOpenFlags = sce_paf_misc_counter_c224;
		ScePafThread_95F9C45F(&CacheManagerImplRMutex);
		result = 0;
	} else {
		result = -2135993338;
	}
	*(uint32_t *)(bytes + 28) = 0;
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
	return result;
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
