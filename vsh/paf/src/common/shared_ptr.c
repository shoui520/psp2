#include "shared_ptr.h"
#include "../thread/mutex.h"
#include "../std/stdc++/atexit.h"

static Mutex sharedPtrMutex;

int SharedPtrInitialize(void)
{
	extern void *__dso_handle;

	MutexConstruct(&sharedPtrMutex, "ScePafSharedPtr", MutexOption_None);
	return ScePafStdc_C95732BB(&sharedPtrMutex,
				(void (*)(void *))MutexDestroy, &__dso_handle);
}
