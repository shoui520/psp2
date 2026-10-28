#ifndef PAF_CACHE_CACHE_MANAGER_IMPL_H
#define PAF_CACHE_CACHE_MANAGER_IMPL_H

#include "../thread/mutex.h"

#define CacheManagerImplRMutex ScePafMisc_DC169B6B
extern RMutex CacheManagerImplRMutex;

int CacheManagerImplInitialize(void);

#endif
