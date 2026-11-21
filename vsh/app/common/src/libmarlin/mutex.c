/*
 * The library's locks. This build runs the client in one thread, so a lock is only
 * checked: 0 for a lock that exists, 3 otherwise.
 */
#include "mln_internal.h"

#define MLN_ERROR_NO_MUTEX	3

int MlnMutexCreate(MlnMutex *mutex)
{
	return mutex != NULL ? MLN_OK : MLN_ERROR_NO_MUTEX;
}

int MlnMutexLock(MlnMutex *mutex)
{
	return mutex != NULL ? MLN_OK : MLN_ERROR_NO_MUTEX;
}

int MlnMutexUnlock(MlnMutex *mutex)
{
	return mutex != NULL ? MLN_OK : MLN_ERROR_NO_MUTEX;
}

int MlnMutexDelete(MlnMutex *mutex)
{
	return mutex != NULL ? MLN_OK : MLN_ERROR_NO_MUTEX;
}

int MlnMutexInit(void)
{
	return MLN_OK;
}

int MlnMutexTerm(void)
{
	return MLN_OK;
}
