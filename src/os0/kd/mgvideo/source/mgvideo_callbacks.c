/* Descriptive file name: no source-file debug records exist in the original. */
#include "mgvideo_private.h"
#include <psp2kern/kernel/threadmgr/mutex.h>

void notifyContextFinalized(void)
{
}

int lockVideoDriver(void)
{
	return ksceKernelLockMutex(driverMutex, 1, 0);
}

int unlockVideoDriver(void)
{
	return ksceKernelUnlockMutex(driverMutex, 1);
}
