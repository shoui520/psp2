/*
 * The C++ ABI's static-initialisation guards (ARM EABI: a 32-bit guard word). Bit 0: the object
 * is initialised; GUARD_PENDING: a thread is initialising it; GUARD_WAITING: another thread
 * waits on the guard condition for it.
 */
#include <yvals.h>

#include "libc_internal.h"

#define GUARD_DONE	0x001
#define GUARD_PENDING	0x100
#define GUARD_WAITING	0x200

int __cxa_guard_acquire(int *guard)
{
	int state;

	_Locksyslock(_LOCK_GUARD);
	state = *guard;
	while (!(state & GUARD_DONE)) {
		if (!(state & GUARD_PENDING)) {
			*guard = GUARD_PENDING;
			_Unlocksyslock(_LOCK_GUARD);
			return 1;
		}
		*guard = state | GUARD_WAITING;
		_Guardwait(0);
		state = *guard;
	}
	_Unlocksyslock(_LOCK_GUARD);
	return 0;
}

int __cxa_guard_release(int *guard)
{
	int state;

	_Locksyslock(_LOCK_GUARD);
	state = *guard;
	*guard = GUARD_DONE;
	if (state & GUARD_WAITING)
		_Guardbroadcast(0);
	return _Unlocksyslock(_LOCK_GUARD);
}

int __cxa_guard_abort(int *guard)
{
	int state;

	_Locksyslock(_LOCK_GUARD);
	state = *guard;
	*guard = 0;
	if (state & GUARD_WAITING)
		_Guardbroadcast(0);
	return _Unlocksyslock(_LOCK_GUARD);
}
