/* The library's mutexes (Sony's names: activity_lock_manager.cpp's checks) */
#ifndef NP_ACTIVITY_COMMON_ACTIVITY_LOCK_MANAGER_H
#define NP_ACTIVITY_COMMON_ACTIVITY_LOCK_MANAGER_H

#include "activity_mutex.h"

class ActivityLockManager : public ActivityObject {
public:
	ActivityLockManager() : m_mutex(NULL)
	{
	}
	virtual ~ActivityLockManager();

	static int Initialize();
	static ActivityLockManager *GetInstance();
	static void Finalize();
	ActivityMutex *GetMutex();

private:
	int CreateAllMutex();
	void DestroyAllMutex();

	ActivityMutex *m_mutex;

	static ActivityLockManager *s_instance;
};

#endif
