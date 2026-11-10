/* The task list a render collects from its racks and runs (as_task.c). */
#ifndef SCE_SAS_AS_TASK_H
#define SCE_SAS_AS_TASK_H

#include "as.h"

/* how the list runs a task */
#define SCE_SAS_AS_TASK_SERIAL		0	/* run in order, chunk by chunk */
#define SCE_SAS_AS_TASK_PARALLEL	1	/* a run of these is passed over, with the task after it */

#define SCE_SAS_AS_TASK_MAX_BUFFERS	8
#define SCE_SAS_AS_TASK_MAX_CHUNK	256	/* samples a chunk of a mixed list runs at most */

/* a buffer a task works on: its number, where it is and its offset in the allocation */
typedef struct SceSasAsTaskBuffer {
	SceUInt32 index;
	void *pointer;
	SceSize offset;
} SceSasAsTaskBuffer;

typedef struct SceSasAsTask {
	SceUInt32 type;
	int (*run)(const SceSasAsRenderInfo *render, SceSasAsTaskBuffer *buffers,
		   SceUInt32 numBuffers);
	SceUInt32 reserved;
	SceSasAsTaskBuffer buffers[SCE_SAS_AS_TASK_MAX_BUFFERS];
	SceUInt32 numBuffers;
} SceSasAsTask;

typedef struct SceSasAsTaskList {
	SceSasAsTask **tasks;
	SceUInt32 capacity;
	SceUInt32 count;
} SceSasAsTaskList;

int sceSasAsTaskListCreate(SceSasAsAllocator *allocator, SceUInt32 capacity, SceSasAsTaskList **list);
int sceSasAsTaskListDestroy(SceSasAsTaskList *list);
int sceSasAsTaskListReset(SceSasAsTaskList *list);
int sceSasAsTaskListBegin(SceSasAsTaskList *list, int phase);
int sceSasAsTaskListEnd(SceSasAsTaskList *list);
int sceSasAsTaskListAdd(SceSasAsTaskList *list, SceSasAsTask *task);
int sceSasAsTaskListRun(SceSasAsTaskList *list, const SceSasAsRenderInfo *render, SceUInt32 numSamples);

#endif
