/*
 * The library's heap: an mspace in the memory block sceNpActivityNetInit is given, or the
 * allocation functions it is given instead. No binary names this file; the name is ours.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "activity_memory.h"

static ActivityMallocFunction s_mallocFunc = NULL;
static ActivityFreeFunction s_freeFunc = NULL;
static void *s_mspace = NULL;
static int s_useFunctions;		/* 1: s_mallocFunc and s_freeFunc, 0: s_mspace */
static int s_allocCount;		/* blocks allocated and not freed */

void activityMemoryInitWithFunctions(ActivityMallocFunction mallocFunc, ActivityFreeFunction freeFunc)
{
	s_mallocFunc = mallocFunc;
	s_freeFunc = freeFunc;
	s_useFunctions = 1;
	s_mspace = NULL;
	s_allocCount = 0;
}

void activityMemoryInit(SceSize size, void *memblock)
{
	s_useFunctions = 0;
	s_mallocFunc = NULL;
	s_freeFunc = NULL;
	s_mspace = NULL;
	s_allocCount = 0;
	s_mspace = sceClibMspaceCreate(memblock, size);
}

void activityMemoryTerm(void)
{
	struct MspaceStats stats;

	if (s_useFunctions) {
		s_mallocFunc = NULL;
		s_freeFunc = NULL;
		s_allocCount = 0;
	} else {
		if (s_mspace != NULL) {
			sceClibMspaceMallocStats(s_mspace, &stats);
			sceClibMspaceDestroy(s_mspace);
			s_allocCount = 0;
		}
		s_mspace = NULL;
	}
}

void *activityMalloc(SceSize size)
{
	struct MspaceStats stats;
	void *ptr;

	if (s_useFunctions) {
		if (s_mallocFunc == NULL)
			return NULL;
		ptr = s_mallocFunc(size);
		if (ptr == NULL)
			return NULL;
	} else {
		if (s_mspace == NULL)
			return NULL;
		ptr = sceClibMspaceMalloc(s_mspace, size);
		if (ptr == NULL) {
			sceClibMspaceMallocStats(s_mspace, &stats);
			return NULL;
		}
	}
	s_allocCount++;
	return ptr;
}

void activityFree(void *ptr)
{
	if (s_useFunctions) {
		if (s_freeFunc == NULL) {
			s_allocCount--;
			return;
		}
		s_allocCount--;
		s_freeFunc(ptr);
	} else {
		if (s_mspace == NULL) {
			s_allocCount--;
			return;
		}
		s_allocCount--;
		sceClibMspaceFree(s_mspace, ptr);
	}
}
