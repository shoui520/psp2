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
	uint8_t reserved2[40];
	int32_t height;
	uint8_t reserved3[4];
	Mutex lock;
} PafLockProtectedDimensions;

_Static_assert(offsetof(PafLockProtectedState, value) == 44,
	       "lock-protected value offset");
_Static_assert(offsetof(PafLockProtectedState, lock) == 56,
	       "lock-protected mutex offset");
_Static_assert(offsetof(PafLockProtectedDimensions, width) == 12,
	       "dimension width offset");
_Static_assert(offsetof(PafLockProtectedDimensions, height) == 56,
	       "dimension height offset");
_Static_assert(offsetof(PafLockProtectedDimensions, lock) == 64,
	       "dimension mutex offset");

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
