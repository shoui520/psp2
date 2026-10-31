/* Synchronous state access guarded by PAF's lightweight Mutex methods. */
#include <stdint.h>
#include "../thread/mutex.h"
#include "../std/stdlib.h"

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

_Static_assert(offsetof(PafLockedFlagOwner, lock) == 16,
	       "flag mutex offset");

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
