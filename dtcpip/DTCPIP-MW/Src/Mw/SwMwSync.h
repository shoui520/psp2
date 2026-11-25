/*
 * The middleware's synchronisation primitives: a signal (a flag with its condition
 * variable and mutex) and the gate the stream objects serialise their commands on
 * (any number of stream commands share it, the periodic connect owns it alone).
 */

#pragma once

#include <scetypes.h>
#include <kernel/threadmgr.h>

/* one signal: the flag a waiter waits for, with its condition variable and mutex */
struct SwMwSignal {
	SceKernelLwCondWork cond;	/* 0x00 */
	SceKernelLwMutexWork mutex;	/* 0x10 */
	int signalled;			/* 0x30 */
	int reserved;			/* 0x34 */
};

/* the gate: its counters, and one signal for each side's waiters */
struct SwMwGate {
	SceKernelLwMutexWork mutex;		/* 0x00 */
	struct SwMwSignal sharedSignal;		/* 0x20: the shared side's wait */
	struct SwMwSignal exclusiveSignal;	/* 0x58: the exclusive side's wait */
	int sharedCount;			/* 0x90 */
	int sharedWaiters;			/* 0x94 */
	int exclusiveCount;			/* 0x98 */
	int exclusiveWaiters;			/* 0x9C */
};

extern "C" {
int SwMwMutexCreate(SceKernelLwMutexWork *mutex);
int SwMwMutexDelete(SceKernelLwMutexWork *mutex);
int SwMwEventCreate(struct SwMwSignal *signal);
int SwMwEventDelete(struct SwMwSignal *signal);
int SwMwGateInit(struct SwMwGate *gate);
int SwMwGateDone(struct SwMwGate *gate);
int SwMwMutexLock(SceKernelLwMutexWork *mutex);
int SwMwMutexUnlock(SceKernelLwMutexWork *mutex);
int SwMwSignalWait(struct SwMwSignal *signal);
int SwMwSignalSet(struct SwMwSignal *signal);
int SwMwGateSharedLock(struct SwMwGate *gate);
int SwMwGateSharedUnlock(struct SwMwGate *gate);
int SwMwGateExclusiveLock(struct SwMwGate *gate);
int SwMwGateExclusiveUnlock(struct SwMwGate *gate);
}
