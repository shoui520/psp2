/* A lightweight mutex (Sony's file: activity_mutex.cpp's checks) */
#include "activity_debug.h"
#include "activity_mutex.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "common\\activity_mutex.cpp"

ActivityMutex::ActivityMutex(const char *name)
{
	if (sceKernelCreateLwMutex(&m_work, name, SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL) < 0)
#line 44
		ACTIVITY_CHECK("ActivityMutex");
}

ActivityMutex::~ActivityMutex()
{
	if (sceKernelDeleteLwMutex(&m_work) < 0)
#line 60
		ACTIVITY_CHECK("~ActivityMutex");
}

int ActivityMutex::Lock()
{
	return sceKernelLockLwMutex(&m_work, 1, NULL);
}

int ActivityMutex::Unlock()
{
	return sceKernelUnlockLwMutex(&m_work, 1);
}
