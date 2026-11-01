#include "shared_ptr.h"
#include "../thread/mutex.h"
#include "../std/stdc++/atexit.h"

static Mutex sharedPtrMutex;

int SharedPtrInitialize(void)
{
	extern void *__dso_handle;

	MutexConstruct(&sharedPtrMutex, "ScePafSharedPtr", MutexOption_None);
	return __aeabi_atexit(&sharedPtrMutex,
				(void (*)(void *))MutexDestroy, &__dso_handle);
}
