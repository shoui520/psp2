/* The task list a render collects from its racks and runs (as_task.c). */
#ifndef SCE_SAS_AS_TASK_H
#define SCE_SAS_AS_TASK_H

#include "as.h"

typedef struct SceSasAsTaskList {
	void **tasks;
	SceUInt32 capacity;
	SceUInt32 count;
} SceSasAsTaskList;

int sceSasAsTaskListCreate(SceSasAsAllocator *allocator, SceUInt32 capacity, SceSasAsTaskList **list);
int sceSasAsTaskListDestroy(SceSasAsTaskList *list);
int sceSasAsTaskListReset(SceSasAsTaskList *list);
int sceSasAsTaskListBegin(SceSasAsTaskList *list, int phase);
int sceSasAsTaskListEnd(SceSasAsTaskList *list);
int sceSasAsTaskListAdd(SceSasAsTaskList *list, void *task);
int sceSasAsTaskListRun(SceSasAsTaskList *list, const SceSasAsRenderInfo *render, SceUInt32 numSamples);

#endif
