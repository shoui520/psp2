/* A lightweight mutex (Sony's names: activity_mutex.cpp's checks) */
#ifndef NP_ACTIVITY_COMMON_ACTIVITY_MUTEX_H
#define NP_ACTIVITY_COMMON_ACTIVITY_MUTEX_H

#include <kernel/threadmgr.h>
#include "activity_object.h"

class ActivityMutex : public ActivityObject {
public:
	ActivityMutex(const char *name);
	virtual ~ActivityMutex();

	int Lock();
	int Unlock();

private:
	int m_reserved;
	SceKernelLwMutexWork m_work;
};

#endif
