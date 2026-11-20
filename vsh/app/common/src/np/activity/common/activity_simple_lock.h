/* The mutex held for a scope (Sony's names: activity_simple_lock.cpp's checks) */
#ifndef NP_ACTIVITY_COMMON_ACTIVITY_SIMPLE_LOCK_H
#define NP_ACTIVITY_COMMON_ACTIVITY_SIMPLE_LOCK_H

#include "activity_mutex.h"

class ActivitySimpleLock : public ActivityObject {
public:
	ActivitySimpleLock(ActivityMutex *mutex);
	virtual ~ActivitySimpleLock();

private:
	ActivityMutex *m_mutex;
};

#endif
