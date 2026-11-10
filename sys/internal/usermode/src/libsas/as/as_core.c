/*
 * The CORE class: a render updates every rack under the update lock, collects their tasks,
 * runs them into the output buss and the caller's buffer, and ends every rack in reverse.
 */

#include <stddef.h>
#include <kernel/threadmgr.h>

#include "as_ssp.h"
#include "as_buss.h"
#include "as_core.h"

static int sceSasAsCoreDestroy(SceSasAsHandle *handle)
{
	SceSasAsCore *core = (SceSasAsCore *)handle;

	sceSasAsTaskListDestroy(core->tasks);
	sceSasAsMutexDelete(&core->updateMutex);
	sceSasAsMutexDelete(&core->renderMutex);
	return 0;
}

static int sceSasAsCoreUnlock(SceSasAsHandle *handle, SceUInt32 block)
{
	SceSasAsCore *core = (SceSasAsCore *)handle;

	if (block > SCE_SAS_AS_CORE_BLOCK_RACKS)
		return SCE_SAS_AS_ERROR_NOT_FOUND;
	sceSasAsMutexUnlock(&core->renderMutex);
	return 0;
}

static int sceSasAsCoreLock(SceSasAsHandle *handle, SceUInt32 block, void **pointer)
{
	SceSasAsCore *core = (SceSasAsCore *)handle;

	if (block == SCE_SAS_AS_CORE_BLOCK_OUTPUT)
		*pointer = &core->output;
	else if (block == SCE_SAS_AS_CORE_BLOCK_RACKS)
		*pointer = &core->racks;
	else
		return SCE_SAS_AS_ERROR_NOT_FOUND;
	sceSasAsMutexLock(&core->renderMutex);
	return 0;
}

static int sceSasAsCoreCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
			      SceSasAsHandle **handle)
{
	SceSasAsTaskList *tasks;
	SceInt32 numGranularities;
	SceInt32 maxGranularities;
	SceInt32 minGranularities;
	SceInt32 numChannels;
	SceInt32 numProcessors;
	SceInt32 numRacks;
	SceInt32 numJobs;
	SceInt32 outputSampleSize;
	SceSasAsCore *core;
	int ret;

	sceSasAsConfigGetInt(config, "numGranularities", 256, &numGranularities);
	sceSasAsConfigGetInt(config, "numProcessors", 8, &numProcessors);
	sceSasAsConfigGetInt(config, "numChannels", 2, &numChannels);
	sceSasAsConfigGetInt(config, "maxGranularities", numGranularities, &maxGranularities);
	sceSasAsConfigGetInt(config, "minGranularities", numGranularities, &minGranularities);
	sceSasAsConfigGetInt(config, "numRacks", 64, &numRacks);
	sceSasAsConfigGetInt(config, "numJobs", numRacks * numProcessors, &numJobs);
	sceSasAsConfigGetInt(config, "outputSampleSize", 2, &outputSampleSize);
	core = sceSasAsAllocatorAllocAligned(allocator, sizeof(*core), 16, 1);
	if (core != NULL) {
		core->bussSampleSize = sizeof(SceInt32);
		core->granularity = numGranularities;
		core->minGranularity = minGranularities;
		core->maxGranularity = maxGranularities;
		core->numChannels = numChannels;
		core->numProcessors = numProcessors;
		core->outputSampleSize = outputSampleSize;
		core->racks.racks = sceSasAsAllocatorAlloc(allocator, numRacks * sizeof(SceUIntPtr), 1);
		core->outputLocked.block = SCE_SAS_AS_CORE_BLOCK_OUTPUT;
		core->output = NULL;
		core->outputLocked.handle = &core->handle;
		core->racksLocked.handle = &core->handle;
		core->racks.capacity = numRacks;
		core->racks.count = 0;
		core->racksLocked.block = SCE_SAS_AS_CORE_BLOCK_RACKS;
		ret = sceSasAsTaskListCreate(allocator, numJobs, &tasks);
		if (ret < 0)
			return ret;
		core->tasks = tasks;
		sceSasAsMutexCreate(&core->renderMutex, "SceSasAsCoreRender");
		sceSasAsMutexCreate(&core->updateMutex, "SceSasAsCoreUpdate");
	} else {
		sceSasAsAllocatorAlloc(allocator, numRacks * sizeof(SceUIntPtr), 1);
		ret = sceSasAsTaskListCreate(allocator, numJobs, &tasks);
		if (ret < 0)
			return ret;
	}
	if (handle != NULL)
		*handle = &core->handle;
	return 0;
}

static void sceSasAsCoreRemoveChild(SceSasAsHandle *handle, SceSasAsHandle *child)
{
	SceSasAsCore *core = (SceSasAsCore *)handle;
	SceUIntPtr *rack;
	SceUIntPtr *end;
	SceUInt16 count;

	sceSasAsMutexLock(&core->renderMutex);
	count = core->racks.count;
	if (&core->output->handle == child)
		core->output = NULL;
	if (count != 0) {
		end = core->racks.racks + count;
		for (rack = core->racks.racks; rack != end; rack++) {
			if (*rack == (SceUIntPtr)child)
				*rack = 0;
		}
	}
	sceSasAsMutexUnlock(&core->renderMutex);
}

int sceSasAsCoreClassSetGranularity(SceSasAsCore *core, SceUInt32 granularity)
{
	core->granularity = granularity;
	return 0;
}

SceUInt32 sceSasAsCoreClassGetMinGranularity(const SceSasAsCore *core)
{
	return core->minGranularity;
}

SceUInt32 sceSasAsCoreClassGetMaxGranularity(const SceSasAsCore *core)
{
	return core->maxGranularity;
}

SceUInt32 sceSasAsCoreClassGetGranularity(const SceSasAsCore *core)
{
	return core->granularity;
}

SceUInt32 sceSasAsCoreClassGetNumChannels(const SceSasAsCore *core)
{
	return core->numChannels;
}

SceUInt32 sceSasAsCoreClassGetNumProcessors(const SceSasAsCore *core)
{
	return core->numProcessors;
}

static inline SceSasAsHandle *sceSasAsCoreGetRack(const SceSasAsCore *core, SceUInt32 index)
{
	return (SceSasAsHandle *)core->racks.racks[index];
}

int sceSasAsCoreClassRender(SceSasAsCore *core, void *output, SceUInt32 numSamples)
{
	SceSasAsRenderInfo render;
	SceSasAsHandle *rack;
	SceUInt32 numBusses;
	SceUInt32 count;
	SceUInt32 i;
	int ret;

	if (!sceSasAsHandleIsClass(&core->handle, SCE_SAS_AS_CLASS_CORE))
		return SCE_SAS_AS_ERROR_INVALID_HANDLE;
	sceSasAsMutexLock(&core->renderMutex);
	render.bussBuffers = sceSasAsBussGetBuffers(core->output, &numBusses);
	render.output = output;
	render.granularity = core->granularity;
	render.numChannels = core->numChannels;
	render.outputChannels = core->numChannels;
	render.numBusses = numBusses;
	render.numProcessors = core->numProcessors;
	render.outputSampleSize = core->outputSampleSize;
	render.sampleRate = SCE_SAS_AS_SAMPLE_RATE;
	render.processor = 0;
	sceSasAsMutexLock(&core->updateMutex);
	for (i = 0; i < core->racks.count; i++) {
		rack = sceSasAsCoreGetRack(core, i);
		if (rack != NULL)
			rack->ops.update(rack, &render);
	}
	sceSasAsMutexUnlock(&core->updateMutex);
	if (core->output == NULL || core->racks.count == 0) {
		sceSasAsMemZero(output, core->granularity * core->numChannels * core->outputSampleSize);
	} else {
		ret = sceSasAsTaskListReset(core->tasks);
		if (ret < 0)
			return ret;
		ret = sceSasAsTaskListBegin(core->tasks, 1);
		if (ret < 0)
			return ret;
		for (i = 0; i < core->racks.count; i++) {
			rack = sceSasAsCoreGetRack(core, i);
			if (rack != NULL) {
				ret = rack->class->ops.render(rack, &render, core->tasks);
				if (ret < 0)
					return ret;
			}
		}
		ret = sceSasAsTaskListEnd(core->tasks);
		if (ret < 0)
			return ret;
		sceSasAsBussLockBuffers(core->output);
		ret = sceSasAsTaskListRun(core->tasks, &render, numSamples);
		if (ret < 0)
			return ret;
		sceSasAsBussUnlockBuffers(core->output);
		count = core->racks.count;
		while (count-- != 0) {
			rack = sceSasAsCoreGetRack(core, count);
			if (rack != NULL) {
				ret = rack->class->ops.renderEnd(rack, &render);
				if (ret < 0)
					return ret;
			}
		}
	}
	sceSasAsMutexUnlock(&core->renderMutex);
	return 0;
}

int sceSasAsCoreClassLockUpdate(SceSasAsCore *core)
{
	sceSasAsMutexLock(&core->updateMutex);
	core->updateLockCount++;
	return 0;
}

int sceSasAsCoreClassUnlockUpdate(SceSasAsCore *core)
{
	if (core->updateLockCount <= 0)
		return SCE_SAS_AS_ERROR_NOT_LOCKED;
	sceSasAsMutexUnlock(&core->updateMutex);
	return 0;
}

const SceSasAsClass sceSasAsCoreClass = {
	.id = SCE_SAS_AS_CLASS_CORE,
	.name = "CORE",
	.version = SCE_SAS_AS_CLASS_VERSION,
	.create = sceSasAsCoreCreate,
	.destroy = sceSasAsCoreDestroy,
	.removeChild = sceSasAsCoreRemoveChild,
	.ops = {
		.lock = sceSasAsCoreLock,
		.unlock = sceSasAsCoreUnlock,
	},
};
