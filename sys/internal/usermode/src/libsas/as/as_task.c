/*
 * The task list: the racks add their tasks during a render, and the list runs them over the
 * render's samples in chunks of the granularity.
 */

#include <stddef.h>

#include "as_ssp.h"
#include "as_task.h"

int sceSasAsTaskListCreate(SceSasAsAllocator *allocator, SceUInt32 capacity, SceSasAsTaskList **list)
{
	SceSasAsTaskList *created;
	SceSasAsTask **tasks;

	created = sceSasAsAllocatorAlloc(allocator, sizeof(*created), 1);
	tasks = sceSasAsAllocatorAlloc(allocator, capacity * sizeof(*tasks), 1);
	if (created != NULL) {
		created->tasks = tasks;
		created->capacity = capacity;
		created->count = 0;
	}
	if (list != NULL)
		*list = created;
	return 0;
}

int sceSasAsTaskListDestroy(SceSasAsTaskList *list)
{
	if (list == NULL)
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	return 0;
}

int sceSasAsTaskListReset(SceSasAsTaskList *list)
{
	list->count = 0;
	return 0;
}

int sceSasAsTaskListBegin(SceSasAsTaskList *list, int phase)
{
	return 0;
}

int sceSasAsTaskListEnd(SceSasAsTaskList *list)
{
	return 0;
}

int sceSasAsTaskListAdd(SceSasAsTaskList *list, SceSasAsTask *task)
{
	if (list->capacity > list->count) {
		list->tasks[list->count++] = task;
		return 0;
	}
	return sceSasAsError(SCE_SAS_AS_ERROR_FULL, "Overflow task entry count (%u)\n", list->count);
}

int sceSasAsTaskListRun(SceSasAsTaskList *list, const SceSasAsRenderInfo *render, SceUInt32 numSamples)
{
	SceSasAsRenderInfo chunk;
	SceSasAsTask *task;
	SceUInt32 granularity;
	SceUInt32 serial;
	SceUInt32 parallel;
	SceUInt32 length;
	SceUInt32 done;
	SceUInt32 i;
	int hasSerial;
	int hasParallel;

	if (list->count != 0) {
		serial = 0;
		parallel = 0;
		for (i = 0; i < list->count; i++) {
			if (list->tasks[i]->type == SCE_SAS_AS_TASK_SERIAL)
				serial++;
			else if (list->tasks[i]->type == SCE_SAS_AS_TASK_PARALLEL)
				parallel++;
		}
		hasSerial = serial != 0;
		hasParallel = parallel != 0;
		if (hasSerial && hasParallel) {
			chunk = *render;
			for (done = 0; done < numSamples; done += length) {
				granularity = render->granularity;
				if (granularity > SCE_SAS_AS_TASK_MAX_CHUNK)
					granularity = SCE_SAS_AS_TASK_MAX_CHUNK;
				length = numSamples - done;
				if (length > granularity)
					length = granularity;
				chunk.granularity = length;
				chunk.output = (char *)render->output +
					render->outputSampleSize * render->outputChannels * done;
				for (i = 0; i < list->count; i++) {
					task = list->tasks[i];
					if (task == NULL)
						continue;
					if (task->type == SCE_SAS_AS_TASK_SERIAL) {
						task->run(&chunk, task->buffers, task->numBuffers);
					} else if (task->type == SCE_SAS_AS_TASK_PARALLEL) {
						while (i < list->count && list->tasks[i] != NULL &&
						       list->tasks[i]->type == SCE_SAS_AS_TASK_PARALLEL)
							i++;
					}
				}
			}
			return 0;
		}
		if (hasSerial) {
			chunk = *render;
			for (done = 0; done < numSamples; done += length) {
				length = numSamples - done;
				if (length >= render->granularity)
					length = render->granularity;
				chunk.granularity = length;
				chunk.output = (char *)render->output +
					render->outputSampleSize * render->outputChannels * done;
				for (i = 0; i < list->count; i++) {
					task = list->tasks[i];
					if (task != NULL)
						task->run(&chunk, task->buffers, task->numBuffers);
				}
			}
			return 0;
		}
		if (hasParallel)
			return 0;
	}
	sceSasAsMemZero(render->output, render->outputSampleSize * render->outputChannels * numSamples);
	return 0;
}

int sceSasAsTaskListWait(SceSasAsTaskList *list)
{
	return 0;
}
