#include <sce/libkernel_backends.h>
#include "lw_sync.h"
#include "thread_context.h"
#include <sce/error.h>
#include "../../../../../../common/sysclib/atomic.h"

_Static_assert(sizeof(struct LwMutexState) == sizeof(SceKernelLwMutexWork),
	       "lightweight mutex work layout");
_Static_assert(offsetof(struct LwMutexState, count) == 8, "lock count field");
_Static_assert(offsetof(struct LwMutexState, uid) == 16, "mutex uid field");

extern int sceKernelGetLwMutexInfoById(SceUID id, SceKernelLwMutexInfo *info);

static inline __attribute__((always_inline))
int TryLockLwMutex(struct LwMutexState *state, int count, uint32_t threadId)
{
	uint32_t owner;
	uint32_t failed;

	if (count <= 0 || state->uid <= 0)
		return (int)(state->uid <= 0 ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_ILLEGAL_COUNT);
	owner = state->owner;
	if (owner) {
		uint32_t previous, next, overflow;

		if (owner != threadId)
			return SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN;
		if (!(state->attributes & 2))
			return SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
		previous = state->count;
		next = previous + (uint32_t)count;
		overflow = (~(previous ^ (uint32_t)count) & (previous ^ next)) >> 31;
		/* Sony branches on ADDS/BGT, including its overflow flag. */
		if (!next || (next >> 31) != overflow)
			return SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF;
		state->count = (int32_t)next;
		return 0;
	}
	if (!(state->attributes & 2) && count != 1)
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	do {
		owner = AtomicLoadReserved32(&state->owner);
		if (owner)
			return SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN;
		failed = AtomicStoreReserved32(&state->owner, threadId);
	} while (failed);
	AtomicFullBarrier();
	state->count = count;
	return 0;
}

int sceKernelTryLockLwMutex(SceKernelLwMutexWork *work, int count)
{
	struct ThreadLocalStorage *storage = currentThreadStorage();

	return TryLockLwMutex((struct LwMutexState *)work, count,
			     storage->kernel.thread.threadId);
}

int sceKernelLockLwMutex(SceKernelLwMutexWork *work, int count, unsigned int *timeout)
{
	struct ThreadLocalStorage *storage = currentThreadStorage();
	int result;

	if (storage->kernel.thread.cannotWait)
		return (int)SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	result = TryLockLwMutex((struct LwMutexState *)work, count,
			       storage->kernel.thread.threadId);
	if (result < 0)
		return _sceKernelLockLwMutex(work, count, timeout, 0);
	return result;
}

int sceKernelLockLwMutexCB(SceKernelLwMutexWork *work, int count, unsigned int *timeout)
{
	struct ThreadLocalStorage *storage = currentThreadStorage();
	int result;

	if (storage->kernel.thread.cannotWait)
		return (int)SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	result = TryLockLwMutex((struct LwMutexState *)work, count,
			       storage->kernel.thread.threadId);
	if (result < 0)
		return _sceKernelLockLwMutex(work, count, timeout, 1);
	return result;
}

static inline __attribute__((always_inline))
int UnlockLwMutex(SceKernelLwMutexWork *work, int count, int checked)
{
	struct LwMutexState *state = (struct LwMutexState *)work;
	int previous, remaining;
	uint64_t pair;
	uint32_t failed;

	if (count <= 0 || state->uid <= 0) {
		if (!checked)
			return _sceKernelUnlockLwMutex(work, count);
		return (int)(state->uid < 0 ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_ILLEGAL_COUNT);
	}
	if (!(state->attributes & 2) && count != 1)
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	if (state->owner != (uint32_t)currentThreadStorage()->kernel.thread.threadId)
		return SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED;
	previous = state->count;
	if ((int64_t)previous - count < 0)
		return SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF;
	remaining = (uint32_t)previous - (uint32_t)count;
	state->count = remaining;
	if (remaining > 0)
		return 0;
	do {
		pair = AtomicLoadReserved64(&state->ownerAndWaiters);
		if (pair >> 32) {
			state->count = previous;
			return _sceKernelUnlockLwMutex(work, count);
		}
		failed = AtomicStoreReserved64(&state->ownerAndWaiters, 0);
	} while (failed);
	AtomicFullBarrier();
	return 0;
}

int sceKernelUnlockLwMutex(SceKernelLwMutexWork *work, int count)
{
	return UnlockLwMutex(work, count, 0);
}

int sceKernelUnlockLwMutex2(SceKernelLwMutexWork *work, int count)
{
	return UnlockLwMutex(work, count, 1);
}

int sceKernelGetLwMutexInfo(SceKernelLwMutexWork *work, SceKernelLwMutexInfo *info)
{
	return sceKernelGetLwMutexInfoById(((struct LwMutexState *)work)->uid, info);
}
