/* The library's mutex, made once (Sony's file: activity_lock_manager.cpp's checks) */
#include <scetypes.h>
#include "activity_debug.h"
#include "../np_activity.h"
#include "activity_lock_manager.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "common\\activity_lock_manager.cpp"

ActivityLockManager *ActivityLockManager::s_instance = NULL;

ActivityLockManager::~ActivityLockManager()
{
	if (m_mutex != NULL) {
#line 49
		ACTIVITY_CHECK("~ActivityLockManager");
		delete m_mutex;
	}
	m_mutex = NULL;
}

inline int ActivityLockManager::CreateAllMutex()
{
	if (m_mutex != NULL) {
#line 133
		ACTIVITY_CHECK("CreateAllMutex");
		delete m_mutex;
		m_mutex = NULL;
	}
	m_mutex = new ActivityMutex("SceNpActivityNetCtxManager");
	if (m_mutex == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return 0;
}

inline void ActivityLockManager::DestroyAllMutex()
{
	delete m_mutex;
	m_mutex = NULL;
}

int ActivityLockManager::Initialize()
{
	s_instance = NULL;
	s_instance = new ActivityLockManager();
	if (s_instance == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return s_instance->CreateAllMutex();
}

ActivityLockManager *ActivityLockManager::GetInstance()
{
	if (s_instance == NULL)
		s_instance = new ActivityLockManager();
	return s_instance;
}

void ActivityLockManager::Finalize()
{
	ActivityLockManager *instance = GetInstance();

	if (instance != NULL) {
		instance->DestroyAllMutex();
		if (s_instance != NULL) {
			delete s_instance;
			s_instance = NULL;
		}
	}
}

ActivityMutex *ActivityLockManager::GetMutex()
{
	return m_mutex;
}
