/*
 * The library's mutex ("SceCompanionUtilMutex"): the exports and the threads take it around the
 * instance's state. UnlockMutex takes what LockMutex returned and unlocks only after a lock.
 */
#include <kernel/threadmgr.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

static bool s_mutexCreated;
static SceKernelLwMutexWork s_mutex;

int CreateMutex(void)
{
	int ret;

	ret = sceKernelCreateLwMutex(&s_mutex, "SceCompanionUtilMutex",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO | SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
				     0, NULL);
	if (ret >= 0) {
		s_mutexCreated = true;
		return 0;
	}
	return ret;
}

int DeleteMutex(void)
{
	if (s_mutexCreated)
		return sceKernelDeleteLwMutex(&s_mutex);
	s_mutexCreated = false;
	return 1;
}

int LockMutex(void)
{
	return sceKernelLockLwMutex(&s_mutex, 1, NULL);
}

int UnlockMutex(int lock)
{
	if (lock == 0)
		return sceKernelUnlockLwMutex(&s_mutex, 1);
	return lock;
}

} /* namespace SceCompanionUtil */
