/* Synchronous state access guarded by PAF's lightweight Mutex methods. */
#include <stdint.h>
#include "../thread/mutex.h"
#include "../std/stdlib.h"

typedef struct PafLockProtectedState {
	uint8_t reserved[44];
	uint32_t value;
	uint8_t reserved2[8];
	Mutex lock;
} PafLockProtectedState;

typedef struct PafLockProtectedDimensions {
	uint8_t reserved[12];
	int32_t width;
	uint8_t reserved2[36];
	int32_t intermediateDimension;
	int32_t height;
	uint8_t reserved3[4];
	Mutex lock;
} PafLockProtectedDimensions;

typedef struct PafLockProtectedStack {
	uint8_t reserved[49];
	uint8_t assertOnEmpty;
	uint8_t reserved2[6];
	int32_t depth;
	void *top;
	Mutex lock;
} PafLockProtectedStack;

typedef struct PafFlagSource PafFlagSource;
typedef struct PafFlagSourceMethods {
	void *reserved;
	uint32_t (*getFlags)(PafFlagSource *self);
} PafFlagSourceMethods;

struct PafFlagSource {
	const PafFlagSourceMethods *methods;
};

typedef struct PafLockedFlagOwner {
	uint32_t reserved;
	PafFlagSource *source;
	uint8_t reserved2[8];
	RMutex lock;
} PafLockedFlagOwner;

_Static_assert(offsetof(PafLockProtectedState, value) == 44,
	       "lock-protected value offset");
_Static_assert(offsetof(PafLockProtectedState, lock) == 56,
	       "lock-protected mutex offset");
_Static_assert(offsetof(PafLockProtectedDimensions, width) == 12,
	       "dimension width offset");
_Static_assert(offsetof(PafLockProtectedDimensions, intermediateDimension) == 52,
	       "intermediate dimension offset");
_Static_assert(offsetof(PafLockProtectedDimensions, height) == 56,
	       "dimension height offset");
_Static_assert(offsetof(PafLockProtectedDimensions, lock) == 64,
	       "dimension mutex offset");
_Static_assert(offsetof(PafLockProtectedStack, assertOnEmpty) == 49,
	       "stack empty assertion flag offset");
_Static_assert(offsetof(PafLockProtectedStack, depth) == 56,
	       "stack depth offset");
_Static_assert(offsetof(PafLockProtectedStack, top) == 60,
	       "stack top offset");
_Static_assert(offsetof(PafLockProtectedStack, lock) == 64,
	       "stack mutex offset");
_Static_assert(offsetof(PafLockedFlagOwner, lock) == 16,
	       "flag mutex offset");

int ScePafMisc_DCE49A56(PafLockProtectedState *self)
{
	Mutex *lock = &self->lock;

	MutexLock(lock);
	MutexUnlock(lock);
	return ScePafStdc_D41FD265();
}

void ScePafMisc_ABA3A343(PafLockProtectedState *self, uint32_t *value)
{
	Mutex *lock = &self->lock;

	MutexLock(lock);
	*value = self->value;
	MutexUnlock(lock);
}

int ScePafMisc_33EDF2E1(Mutex *self)
{
	Mutex *lock = self + 2;

	MutexLock(lock);
	MutexUnlock(lock);
	return ScePafStdc_D41FD265();
}

int32_t ScePafMisc_78209706(PafLockProtectedDimensions *self)
{
	Mutex *lock = &self->lock;
	int32_t area;

	MutexLock(lock);
	area = (int32_t)((uint32_t)self->width * (uint32_t)self->height);
	MutexUnlock(lock);
	return area;
}

void ScePafMisc_07BE1242(PafLockProtectedDimensions *self, int32_t *dimensions)
{
	Mutex *lock = &self->lock;
	int32_t middle, last;

	MutexLock(lock);
	middle = self->intermediateDimension;
	last = self->height;
	dimensions[0] = self->width;
	dimensions[1] = middle;
	dimensions[2] = last;
	MutexUnlock(lock);
}

void ScePafMisc_802D12CD(PafLockProtectedStack *self, void *node)
{
	Mutex *lock = &self->lock;

	MutexLock(lock);
	if (node) {
		*(void **)node = self->top;
		self->depth++;
		self->top = node;
	}
	MutexUnlock(lock);
}

void *ScePafMisc_2713C850(PafLockProtectedStack *self)
{
	Mutex *lock = &self->lock;
	void *node;

	MutexLock(lock);
	node = self->top;
	if (node) {
		void *next = *(void **)node;
		--self->depth;
		self->top = next;
	} else if (!self->assertOnEmpty) {
		ScePafStdc_D41FD265();
	}
	MutexUnlock(lock);
	return node;
}

uint32_t ScePafMisc_4558694C(PafLockedFlagOwner *self)
{
	RMutex *lock = &self->lock;
	uint32_t flags = 63;
	PafFlagSource *source;

	_ZN3paf6thread6RMutex4LockEv(lock);
	_ZN3paf6thread6RMutex4LockEv(lock);
	source = self->source;
	if (source)
		flags = source->methods->getFlags(source) & 63;
	_ZN3paf6thread6RMutex6UnlockEv(lock);
	_ZN3paf6thread6RMutex6UnlockEv(lock);
	return flags & 0x17;
}
