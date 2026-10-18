/*
 * A readers-writer lock on a kernel mutex and two condition variables.
 * Waiting readers are let in before waiting writers.
 */

#include <threadmgr/threadmgr.h>
#include "fios2_internal.h"

void sceFiosKernelRwlockInit(SceFiosKernelRwlock *lock)
{
	lock->mutex = sceKernelCreateMutex("SceFiosKernelRwlockMutex",
					    SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
	lock->readCond = sceKernelCreateCond("SceFiosKernelRwlockReadCond",
					      SCE_KERNEL_ATTR_TH_PRIO, lock->mutex, NULL);
	lock->writeCond = sceKernelCreateCond("SceFiosKernelRwlockWriteCond",
					       SCE_KERNEL_ATTR_TH_PRIO, lock->mutex, NULL);
	lock->readWaiters = 0;
	lock->readers = 0;
	lock->writeWaiters = 0;
	lock->writer = 0;
}

void sceFiosKernelRwlockTerm(SceFiosKernelRwlock *lock)
{
	sceKernelDeleteCond(lock->writeCond);
	sceKernelDeleteCond(lock->readCond);
	sceKernelDeleteMutex(lock->mutex);
}

void sceFiosKernelRwlockReadLock(SceFiosKernelRwlock *lock)
{
	sceKernelLockMutex(lock->mutex, 1, NULL);
	if (unlikely(lock->writer != 0)) {
		lock->readWaiters++;
		do
			sceKernelWaitCond(lock->readCond, NULL);
		while (lock->writer != 0);
		lock->readWaiters--;
	}
	lock->readers++;
	sceKernelUnlockMutex(lock->mutex, 1);
}

void sceFiosKernelRwlockReadUnlock(SceFiosKernelRwlock *lock)
{
	sceKernelLockMutex(lock->mutex, 1, NULL);
	if (--lock->readers == 0 && lock->writeWaiters > 0)
		sceKernelSignalCond(lock->writeCond);
	sceKernelUnlockMutex(lock->mutex, 1);
}

void sceFiosKernelRwlockWriteLock(SceFiosKernelRwlock *lock)
{
	sceKernelLockMutex(lock->mutex, 1, NULL);
	if (unlikely((lock->writer | lock->readers) != 0)) {
		lock->writeWaiters++;
		do
			sceKernelWaitCond(lock->writeCond, NULL);
		while ((lock->writer | lock->readers) != 0);
		lock->writeWaiters--;
	}
	lock->writer = 1;
	sceKernelUnlockMutex(lock->mutex, 1);
}

void sceFiosKernelRwlockWriteUnlock(SceFiosKernelRwlock *lock)
{
	sceKernelLockMutex(lock->mutex, 1, NULL);
	lock->writer = 0;
	if (unlikely(lock->readWaiters != 0))
		sceKernelSignalCondAll(lock->readCond);
	else if (unlikely(lock->writeWaiters != 0))
		sceKernelSignalCond(lock->writeCond);
	sceKernelUnlockMutex(lock->mutex, 1);
}
