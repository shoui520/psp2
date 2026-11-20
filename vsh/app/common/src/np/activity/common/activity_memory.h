/*
 * The library's heap: an mspace in the caller's memory block, or the caller's allocation
 * functions. No binary names these functions; the names are ours.
 */
#ifndef NP_ACTIVITY_COMMON_ACTIVITY_MEMORY_H
#define NP_ACTIVITY_COMMON_ACTIVITY_MEMORY_H

#include <scetypes.h>

typedef void *(*ActivityMallocFunction)(SceSize size);
typedef void (*ActivityFreeFunction)(void *ptr);

void activityMemoryInitWithFunctions(ActivityMallocFunction mallocFunc, ActivityFreeFunction freeFunc);
void activityMemoryInit(SceSize size, void *memblock);
void activityMemoryTerm(void);
void *activityMalloc(SceSize size);
void activityFree(void *ptr);

#endif
