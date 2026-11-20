/* The mutex held for a scope (Sony's file: activity_simple_lock.cpp's checks) */
#include "activity_debug.h"
#include "activity_simple_lock.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "common\\activity_simple_lock.cpp"

ActivitySimpleLock::ActivitySimpleLock(ActivityMutex *mutex) : m_mutex(mutex)
{
	if (m_mutex == NULL)
#line 34
		ACTIVITY_CHECK("ActivitySimpleLock");
	else
		m_mutex->Lock();
}

ActivitySimpleLock::~ActivitySimpleLock()
{
	if (m_mutex == NULL)
#line 47
		ACTIVITY_CHECK("~ActivitySimpleLock");
	else
		m_mutex->Unlock();
}
