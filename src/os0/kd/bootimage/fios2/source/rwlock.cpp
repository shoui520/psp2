/*
 * A readers-writer lock on a kernel mutex and two condition variables.
 * Waiting readers are let in before waiting writers.
 */

#include <psp2kern/kernel/threadmgr.h>
#include "fios2_internal.h"

void sceFiosKernelRwlockInit(SceFiosKernelRwlock *lock)
{
	lock->mutex = ksceKernelCreateMutex("SceFiosKernelRwlockMutex",
					    SCE_KERNEL_ATTR_THREAD_PRIO, 0, NULL);
	lock->readCond = ksceKernelCreateCond("SceFiosKernelRwlockReadCond",
					      SCE_KERNEL_ATTR_THREAD_PRIO, lock->mutex, NULL);
	lock->writeCond = ksceKernelCreateCond("SceFiosKernelRwlockWriteCond",
					       SCE_KERNEL_ATTR_THREAD_PRIO, lock->mutex, NULL);
	lock->readWaiters = 0;
	lock->readers = 0;
	lock->writeWaiters = 0;
	lock->writer = 0;
}

void sceFiosKernelRwlockTerm(SceFiosKernelRwlock *lock)
{
	ksceKernelDeleteCond(lock->writeCond);
	ksceKernelDeleteCond(lock->readCond);
	ksceKernelDeleteMutex(lock->mutex);
}

void sceFiosKernelRwlockReadLock(SceFiosKernelRwlock *lock)
{
	ksceKernelLockMutex(lock->mutex, 1, NULL);
	if (unlikely(lock->writer != 0)) {
		lock->readWaiters++;
		do
			ksceKernelWaitCond(lock->readCond, NULL);
		while (lock->writer != 0);
		lock->readWaiters--;
	}
	lock->readers++;
	ksceKernelUnlockMutex(lock->mutex, 1);
}

void sceFiosKernelRwlockReadUnlock(SceFiosKernelRwlock *lock)
{
	ksceKernelLockMutex(lock->mutex, 1, NULL);
	if (--lock->readers == 0 && lock->writeWaiters > 0)
		ksceKernelSignalCond(lock->writeCond);
	ksceKernelUnlockMutex(lock->mutex, 1);
}

void sceFiosKernelRwlockWriteLock(SceFiosKernelRwlock *lock)
{
	ksceKernelLockMutex(lock->mutex, 1, NULL);
	if (unlikely((lock->writer | lock->readers) != 0)) {
		lock->writeWaiters++;
		do
			ksceKernelWaitCond(lock->writeCond, NULL);
		while ((lock->writer | lock->readers) != 0);
		lock->writeWaiters--;
	}
	lock->writer = 1;
	ksceKernelUnlockMutex(lock->mutex, 1);
}

void sceFiosKernelRwlockWriteUnlock(SceFiosKernelRwlock *lock)
{
	ksceKernelLockMutex(lock->mutex, 1, NULL);
	lock->writer = 0;
	if (unlikely(lock->readWaiters != 0))
		ksceKernelSignalCondAll(lock->readCond);
	else if (unlikely(lock->writeWaiters != 0))
		ksceKernelSignalCond(lock->writeCond);
	ksceKernelUnlockMutex(lock->mutex, 1);
}
