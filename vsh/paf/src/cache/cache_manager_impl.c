#include "cache_manager_impl.h"
#include "../std/stdc++/atexit.h"

RMutex CacheManagerImplRMutex;

int CacheManagerImplInitialize(void)
{
	extern void *__dso_handle;

	ScePafThread_C4BD9753(&CacheManagerImplRMutex, "ScePafCacheRmutex",
			    MutexOption_None);
	return ScePafStdc_C95732BB(&CacheManagerImplRMutex,
				(void (*)(void *))ScePafThread_8CCECAC9,
				&__dso_handle);
}
