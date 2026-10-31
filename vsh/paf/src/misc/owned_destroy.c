/* Native PAF objects can be allocated with a per-owner heap or globally. */
#include <stdint.h>
#include <stddef.h>
#include "../cache/cache_manager_impl.h"
#include "../memory/heap_allocator.h"
#include "../std/stdlib.h"

typedef struct PafOwnedObject PafOwnedObject;

typedef struct PafOwnedObjectMethods {
	void *reserved;
	void (*destruct)(PafOwnedObject *self);
} PafOwnedObjectMethods;

typedef struct PafObjectOwner {
	uint8_t reserved[12];
	HeapAllocator *allocator;
} PafObjectOwner;

struct PafOwnedObject {
	const PafOwnedObjectMethods *methods;
	PafObjectOwner *owner;
	uint8_t reserved[74];
	uint8_t useOwnerHeap;
};

_Static_assert(offsetof(PafOwnedObject, owner) == 4, "owner pointer offset");
_Static_assert(offsetof(PafOwnedObject, useOwnerHeap) == 82,
	       "allocator flag offset");
_Static_assert(offsetof(PafObjectOwner, allocator) == 12,
	       "owner heap offset");

static void DestroyOwnedObject(PafOwnedObject *self)
{
	if (!self)
		return;

	if (self->useOwnerHeap) {
		PafObjectOwner *owner = self->owner;

		if (owner) {
			HeapAllocator *allocator = owner->allocator;

			self->methods->destruct(self);
			scePafFreeWithContext(allocator, self);
		}
	} else {
		self->methods->destruct(self);
		sce_paf_free(self);
	}
}

void ScePafMisc_87466AA5(PafOwnedObject *self)
{
	DestroyOwnedObject(self);
}

void ScePafMisc_C1A97796(PafOwnedObject *self)
{
	DestroyOwnedObject(self);
}

/* Cache-owned objects are destroyed while the cache global lock is held. */
void ScePafMisc_232CE52A(PafOwnedObject *self)
{
	_ZN3paf6thread6RMutex4LockEv(&CacheManagerImplRMutex);
	DestroyOwnedObject(self);
	_ZN3paf6thread6RMutex6UnlockEv(&CacheManagerImplRMutex);
}

void ScePafMisc_D64D39A6(PafOwnedObject *self)
{
	_ZN3paf6thread6RMutex4LockEv(&CacheManagerImplRMutex);
	DestroyOwnedObject(self);
	_ZN3paf6thread6RMutex6UnlockEv(&CacheManagerImplRMutex);
}
