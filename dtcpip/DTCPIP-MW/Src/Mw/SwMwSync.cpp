/*
 * The middleware's synchronisation primitives (see SwMwSync.h): the signal's wait and
 * set, and the gate's shared and exclusive sides.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "SwMwSync.h"

/* what a failed signal wait reports (22: the object's own error status) */
#define SWMW_SIGNAL_ERROR	22

extern "C" int SwMwMutexLock(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

extern "C" int SwMwMutexUnlock(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

/* wait for the signal's flag and clear it */
extern "C" int SwMwSignalWait(struct SwMwSignal *signal)
{
	if (sceKernelLockLwMutex(&signal->mutex, 1, NULL) < 0)
		return SWMW_SIGNAL_ERROR;
	while (signal->signalled == 0) {
		if (sceKernelWaitLwCond(&signal->cond, NULL) < 0) {
			sceKernelUnlockLwMutex(&signal->mutex, 1);
			return SWMW_SIGNAL_ERROR;
		}
	}
	signal->signalled = 0;
	if (sceKernelUnlockLwMutex(&signal->mutex, 1) != 0)
		return SWMW_SIGNAL_ERROR;
	return 0;
}

/* set the signal's flag and wake its waiter */
extern "C" int SwMwSignalSet(struct SwMwSignal *signal)
{
	sceKernelLockLwMutex(&signal->mutex, 1, NULL);
	signal->signalled = 1;
	sceKernelSignalLwCond(&signal->cond);
	sceKernelUnlockLwMutex(&signal->mutex, 1);
	return 0;
}

/* take the gate's shared side: it waits while the exclusive side holds the gate */
extern "C" int SwMwGateSharedLock(struct SwMwGate *gate)
{
	SwMwMutexLock(&gate->mutex);
	while (gate->exclusiveCount != 0) {
		gate->sharedWaiters++;
		SwMwMutexUnlock(&gate->mutex);
		SwMwSignalWait(&gate->sharedSignal);
		SwMwMutexLock(&gate->mutex);
		gate->sharedWaiters--;
	}
	gate->sharedCount++;
	return SwMwMutexUnlock(&gate->mutex);
}

/* release the gate's shared side and wake the exclusive side when the last leaves */
extern "C" int SwMwGateSharedUnlock(struct SwMwGate *gate)
{
	int waiter;

	SwMwMutexLock(&gate->mutex);
	gate->sharedCount -= 1;
	if (gate->sharedCount == 0) {
		for (waiter = gate->exclusiveWaiters; waiter != 0; --waiter)
			SwMwSignalSet(&gate->exclusiveSignal);
	}
	return SwMwMutexUnlock(&gate->mutex);
}

/* take the gate's exclusive side: it waits while any shared holder has it */
extern "C" int SwMwGateExclusiveLock(struct SwMwGate *gate)
{
	SwMwMutexLock(&gate->mutex);
	while (gate->sharedCount != 0) {
		gate->exclusiveWaiters++;
		SwMwMutexUnlock(&gate->mutex);
		SwMwSignalWait(&gate->exclusiveSignal);
		SwMwMutexLock(&gate->mutex);
		gate->exclusiveWaiters--;
	}
	gate->exclusiveCount++;
	return SwMwMutexUnlock(&gate->mutex);
}

/* release the gate's exclusive side and wake the shared side when it leaves */
extern "C" int SwMwGateExclusiveUnlock(struct SwMwGate *gate)
{
	int waiter;

	SwMwMutexLock(&gate->mutex);
	gate->exclusiveCount -= 1;
	if (gate->exclusiveCount == 0) {
		for (waiter = gate->sharedWaiters; waiter != 0; --waiter)
			SwMwSignalSet(&gate->sharedSignal);
	}
	return SwMwMutexUnlock(&gate->mutex);
}
