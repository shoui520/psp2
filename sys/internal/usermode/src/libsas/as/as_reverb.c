/*
 * The REVERB rack: its task runs the SAS effect (sceSasSetEffectType's room, studios, hall,
 * space, echo, delay and pipe) over its inputs into its outputs.
 */

#include <stddef.h>
#include <kernel/threadmgr.h>

#include "as_ssp.h"
#include "as_reverb.h"

static int sceSasAsReverbRackDestroy(SceSasAsHandle *handle)
{
	return 0;
}

static int sceSasAsReverbRackUnlock(SceSasAsHandle *handle, SceUInt32 block)
{
	SceSasAsReverbRack *rack = (SceSasAsReverbRack *)handle;
	SceUInt32 bit;

	switch (block >> 16) {
	case SCE_SAS_AS_RACK_BLOCK_INPUTS >> 16:
		bit = SCE_SAS_AS_REVERB_UPDATED_INPUTS;
		break;
	case SCE_SAS_AS_RACK_BLOCK_OUTPUTS >> 16:
		bit = SCE_SAS_AS_REVERB_UPDATED_OUTPUTS;
		break;
	case SCE_SAS_AS_REVERB_BLOCK_EFFECT:
		bit = SCE_SAS_AS_REVERB_UPDATED_EFFECT;
		break;
	default:
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	}
	sceSasAsAtomicOr(&rack->updated, bit);
	return sceSasAsSpinTryUnlockMask(&rack->lock, bit);
}

static int sceSasAsReverbRackRenderEnd(SceSasAsHandle *handle, const SceSasAsRenderInfo *render)
{
	return 0;
}

static int sceSasAsReverbRackRender(SceSasAsHandle *handle, const SceSasAsRenderInfo *render,
				    SceSasAsTaskList *tasks)
{
	SceSasAsReverbRack *rack = (SceSasAsReverbRack *)handle;

	sceSasAsTaskListAdd(tasks, &rack->task);
	return 0;
}

static int sceSasAsReverbRackRun(const SceSasAsRenderInfo *render, SceSasAsTaskBuffer *buffers,
				 SceUInt32 numBuffers);

static int sceSasAsReverbRackCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
				    SceSasAsHandle **handle)
{
	SceSasAsReverbRack *rack;
	SceSasAsReverbRackState *state;
	SceSasAsReverbRackWork *work;

	rack = sceSasAsAllocatorAllocAligned(allocator, sizeof(*rack), 16, 1);
	if (rack != NULL) {
		rack->task.type = SCE_SAS_AS_TASK_SERIAL;
		rack->task.run = sceSasAsReverbRackRun;
		sceSasAsRackIoParamCreate(allocator, config, &rack->handle,
					  offsetof(SceSasAsReverbRack, io));
		rack->effectLocked.block = SCE_SAS_AS_REVERB_BLOCK_EFFECT << 16;
		rack->effectLocked.handle = &rack->handle;
		state = sceSasAsAllocatorAlloc(allocator, sizeof(*state), 1);
		sceSasAsRackIoTableCreate(allocator, config, state,
					  offsetof(SceSasAsReverbRackState, io));
		state->type = SCE_SAS_AS_REVERB_TYPE_OFF;
		rack->state = state;
		rack->task.buffers[0].index = 0;
		rack->task.buffers[0].pointer = state;
		rack->task.buffers[0].offset = sceSasAsAllocatorGetOffset(allocator, (SceUIntPtr)state);
		work = sceSasAsAllocatorAlloc(allocator, sizeof(*work), 1);
		sceSasAsRackIoTaskCreate(allocator, config, work, offsetof(SceSasAsReverbRackWork, io));
		work->type = SCE_SAS_AS_REVERB_TYPE_OFF;
		rack->work = work;
		rack->task.buffers[1].pointer = work;
		rack->task.buffers[1].index = 2;
		rack->task.buffers[1].offset = sceSasAsAllocatorGetOffset(allocator, (SceUIntPtr)work);
	} else {
		sceSasAsRackIoParamCreate(allocator, config, NULL, offsetof(SceSasAsReverbRack, io));
		state = sceSasAsAllocatorAlloc(allocator, sizeof(*state), 1);
		sceSasAsRackIoTableCreate(allocator, config, state,
					  offsetof(SceSasAsReverbRackState, io));
		work = sceSasAsAllocatorAlloc(allocator, sizeof(*work), 1);
		sceSasAsRackIoTaskCreate(allocator, config, work, offsetof(SceSasAsReverbRackWork, io));
	}
	if (handle != NULL)
		*handle = &rack->handle;
	return 0;
}

/* a 0..1 parameter as 0..128 */
static inline SceUInt8 sceSasAsReverbRackParameter(float parameter)
{
	float value = parameter * 128.0f;

	if (value < 0.0f)
		return 0;
	if (value > 128.0f)
		return 128;
	return (SceUInt32)value;
}

static int sceSasAsReverbRackUpdate(SceSasAsHandle *handle, const SceSasAsRenderInfo *render)
{
	SceSasAsReverbRack *rack = (SceSasAsReverbRack *)handle;
	SceSasAsReverbRackState *state = rack->state;
	SceUInt32 updated;

	if (rack->updated == 0)
		return 0;
	sceSasAsSpinLockMask(&rack->lock, ~0u);
	updated = rack->updated;
	sceSasAsAtomicAnd(&rack->updated, 0);
	if (updated & SCE_SAS_AS_REVERB_UPDATED_INPUTS)
		sceSasAsRackIoTableSetInputs(&rack->io, &state->io);
	if (updated & SCE_SAS_AS_REVERB_UPDATED_OUTPUTS)
		sceSasAsRackIoTableSetOutputs(&rack->io, &state->io);
	if (updated & SCE_SAS_AS_REVERB_UPDATED_EFFECT) {
		state->type = rack->effect.type;
		state->feedback = sceSasAsReverbRackParameter(rack->effect.feedback);
		state->delay = sceSasAsReverbRackParameter(rack->effect.delay);
		state->levels[0] = sceSasAsLevelToFixed(rack->effect.levels[0]);
		state->levels[1] = sceSasAsLevelToFixed(rack->effect.levels[1]);
		state->levels[2] = sceSasAsLevelToFixed(rack->effect.levels[2]);
		state->levels[3] = sceSasAsLevelToFixed(rack->effect.levels[3]);
	}
	sceSasAsSpinTryUnlockMask(&rack->lock, ~0u);
	return 0;
}

static int sceSasAsReverbRackLock(SceSasAsHandle *handle, SceUInt32 block, void **pointer)
{
	SceSasAsReverbRack *rack = (SceSasAsReverbRack *)handle;
	SceUInt32 bit;

	switch (block >> 16) {
	case SCE_SAS_AS_RACK_BLOCK_INPUTS >> 16:
		*pointer = &rack->io.inputs;
		bit = SCE_SAS_AS_REVERB_UPDATED_INPUTS;
		break;
	case SCE_SAS_AS_RACK_BLOCK_OUTPUTS >> 16:
		*pointer = &rack->io.outputs;
		bit = SCE_SAS_AS_REVERB_UPDATED_OUTPUTS;
		break;
	case SCE_SAS_AS_REVERB_BLOCK_EFFECT:
		*pointer = &rack->effect;
		bit = SCE_SAS_AS_REVERB_UPDATED_EFFECT;
		break;
	default:
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	}
	sceSasAsSpinLockMask(&rack->lock, bit);
	return 0;
}

/* the task (written next) */

const SceSasAsClass sceSasAsReverbRackClass = {
	.id = SCE_SAS_AS_CLASS_RACK,
	.name = "PVERB",
	.version = SCE_SAS_AS_CLASS_VERSION,
	.create = sceSasAsReverbRackCreate,
	.destroy = sceSasAsReverbRackDestroy,
	.ops = {
		.lock = sceSasAsReverbRackLock,
		.unlock = sceSasAsReverbRackUnlock,
		.update = sceSasAsReverbRackUpdate,
		.render = sceSasAsReverbRackRender,
		.renderEnd = sceSasAsReverbRackRenderEnd,
	},
};
