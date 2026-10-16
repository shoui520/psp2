#include <sce/libkernel_backends.h>
#include "lw_sync.h"
#include "thread_context.h"
#include <sce/error.h>
#include "../../../../../../common/sysclib/atomic.h"

extern int sceKernelTryLockLwMutex(SceKernelLwMutexWork *work, int count);

static int LockLwMutexBusy(SceKernelLwMutexWork *work, int count,
		unsigned int *timeout, unsigned int callbacks)
{
	int result;

	if (currentThreadStorage()->kernel.thread.cannotWait)
		return (int)SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	result = sceKernelTryLockLwMutex(work, count);
	if (result == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN)
		return _sceKernelLockLwMutex(work, count, timeout, callbacks);
	return result;
}

int SceLibKernel_A7819967(SceKernelLwMutexWork *work, int count,
		unsigned int *timeout)
{
	return LockLwMutexBusy(work, count, timeout, 0);
}

int SceLibKernel_6F9C4CC1(SceKernelLwMutexWork *work, int count,
		unsigned int *timeout)
{
	return LockLwMutexBusy(work, count, timeout, 1);
}

int SceLibKernel_9EF798C1(SceKernelLwMutexWork *work, int count)
{
	struct LwMutexState *state = (struct LwMutexState *)work;
	uint32_t owner, threadId, previous, next, failed;

	if (state->uid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;
	if (count <= 0)
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	owner = state->owner;
	threadId = currentThreadStorage()->kernel.thread.threadId;
	if (owner == threadId) {
		if (!(state->attributes & 2))
			return SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
		previous = state->count;
		next = previous + (uint32_t)count;
		if ((~(previous ^ (uint32_t)count) & (previous ^ next)) >> 31)
			return SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF;
		state->count = (int32_t)next;
		return 0;
	}
	if (owner)
		return SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN;
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

int SceLibKernel_499EA781(SceKernelLwMutexWork *work, int count)
{
	struct LwMutexState *state = (struct LwMutexState *)work;
	int previous;
	int64_t remaining;
	uint64_t pair;

	if (state->uid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;
	if (count <= 0 || (!(state->attributes & 2) && count != 1))
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	if (state->owner != (uint32_t)currentThreadStorage()->kernel.thread.threadId)
		return SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED;
	previous = state->count;
	remaining = (int64_t)previous - count;
	if (remaining < 0)
		return SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF;
	state->count = (int32_t)remaining;
	if (remaining > 0)
		return 0;
	AtomicFullBarrier();
	pair = AtomicLoadReserved64(&state->ownerAndWaiters);
	/* Unlike the Thumb API, any failed release goes straight to the syscall. */
	if (!(pair >> 32) && !AtomicStoreReserved64(&state->ownerAndWaiters, 0))
		return 0;
	state->count = previous;
	return _sceKernelUnlockLwMutex(work, count);
}
