/*
 * The MIXER rack: its task mixes each input channel into each output channel at the
 * matrix's level, ramping from the levels of the last render.
 */

#include <stddef.h>
#include <kernel/threadmgr.h>

#include "as_ssp.h"
#include "as_mixer.h"

static int sceSasAsMixerRackDestroy(SceSasAsHandle *handle)
{
	return 0;
}

static int sceSasAsMixerRackUnlock(SceSasAsHandle *handle, SceUInt32 block)
{
	SceSasAsMixerRack *rack = (SceSasAsMixerRack *)handle;
	SceUInt32 bit;

	switch (block >> 16) {
	case SCE_SAS_AS_RACK_BLOCK_INPUTS >> 16:
		bit = SCE_SAS_AS_MIXER_UPDATED_INPUTS;
		break;
	case SCE_SAS_AS_RACK_BLOCK_OUTPUTS >> 16:
		bit = SCE_SAS_AS_MIXER_UPDATED_OUTPUTS;
		break;
	case SCE_SAS_AS_MIXER_BLOCK_MATRIX:
		bit = SCE_SAS_AS_MIXER_UPDATED_MATRIX;
		break;
	default:
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	}
	sceSasAsAtomicOr(&rack->updated, bit);
	return sceSasAsSpinTryUnlockMask(&rack->lock, bit);
}

static int sceSasAsMixerRackRenderEnd(SceSasAsHandle *handle, const SceSasAsRenderInfo *render)
{
	return 0;
}

static int sceSasAsMixerRackRender(SceSasAsHandle *handle, const SceSasAsRenderInfo *render,
				   SceSasAsTaskList *tasks)
{
	SceSasAsMixerRack *rack = (SceSasAsMixerRack *)handle;

	sceSasAsTaskListAdd(tasks, &rack->task);
	return 0;
}

static int sceSasAsMixerRackRun(const SceSasAsRenderInfo *render, SceSasAsTaskBuffer *buffers,
				SceUInt32 numBuffers);

static int sceSasAsMixerRackCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
				   SceSasAsHandle **handle)
{
	SceUInt32 numChannels;
	SceSasAsMixerRack *rack;
	SceSasAsMixerRackState *state;
	SceSasAsMixerRackIo *taskIo;
	float *matrix;
	SceInt16 *levels;
	SceUInt32 i, j;

	sceSasAsConfigGetUInt(config, "numChannels", 2, &numChannels);
	rack = sceSasAsAllocatorAllocAligned(allocator, sizeof(*rack), 16, 1);
	if (rack != NULL) {
		rack->task.run = sceSasAsMixerRackRun;
		rack->task.type = SCE_SAS_AS_TASK_SERIAL;
		rack->task.numBuffers = 2;
		rack->numChannels = (SceUInt8)numChannels;
		matrix = sceSasAsAllocatorAlloc(allocator, numChannels * numChannels * sizeof(float), 1);
		sceSasAsRackIoParamCreate(allocator, config, &rack->handle,
					  offsetof(SceSasAsMixerRack, io));
		rack->matrixLocked.block = SCE_SAS_AS_MIXER_BLOCK_MATRIX << 16;
		rack->matrix.levels = matrix;
		rack->matrixLocked.handle = &rack->handle;
		rack->matrix.numChannels = numChannels;
		for (i = 0; i < numChannels; i++) {
			for (j = 0; j < numChannels; j++)
				matrix[i * numChannels + j] = j == i ? 1.0f : 0.0f;
		}

		state = sceSasAsAllocatorAlloc(allocator, sizeof(*state), 1);
		levels = sceSasAsAllocatorAlloc(allocator, numChannels * numChannels * sizeof(SceInt16), 1);
		sceSasAsRackIoTableCreate(allocator, config, state,
					  offsetof(SceSasAsMixerRackState, io));
		state->levels = levels;
		state->levelsOffset = SCE_SAS_AS_RELATIVE(state->levelsOffset, levels);
		for (i = 0; i < numChannels; i++) {
			for (j = 0; j < numChannels; j++)
				levels[i * numChannels + j] = j == i ? 0x1000 : 0;
		}
		rack->state = state;
		rack->task.buffers[0].index = 0;
		rack->task.buffers[0].pointer = state;
		rack->task.buffers[0].offset = sceSasAsAllocatorGetOffset(allocator, (SceUIntPtr)state);

		taskIo = sceSasAsAllocatorAlloc(allocator, sizeof(*taskIo), 1);
		levels = sceSasAsAllocatorAlloc(allocator, numChannels * numChannels * sizeof(SceInt16), 1);
		sceSasAsRackIoTaskCreate(allocator, config, taskIo, 0);
		taskIo->levelsOffset = SCE_SAS_AS_RELATIVE(taskIo->levelsOffset, levels);
		for (i = 0; i < numChannels; i++) {
			for (j = 0; j < numChannels; j++)
				levels[i * numChannels + j] = j == i ? 0x1000 : 0;
		}
		rack->taskIo = taskIo;
		rack->task.buffers[1].index = 2;
		rack->task.buffers[1].pointer = taskIo;
		rack->task.buffers[1].offset = sceSasAsAllocatorGetOffset(allocator, (SceUIntPtr)taskIo);
	} else {
		sceSasAsAllocatorAlloc(allocator, numChannels * numChannels * sizeof(float), 1);
		sceSasAsRackIoParamCreate(allocator, config, NULL, offsetof(SceSasAsMixerRack, io));
		state = sceSasAsAllocatorAlloc(allocator, sizeof(*state), 1);
		sceSasAsAllocatorAlloc(allocator, numChannels * numChannels * sizeof(SceInt16), 1);
		sceSasAsRackIoTableCreate(allocator, config, state,
					  offsetof(SceSasAsMixerRackState, io));
		taskIo = sceSasAsAllocatorAlloc(allocator, sizeof(*taskIo), 1);
		sceSasAsAllocatorAlloc(allocator, numChannels * numChannels * sizeof(SceInt16), 1);
		sceSasAsRackIoTaskCreate(allocator, config, taskIo, 0);
	}
	if (handle != NULL)
		*handle = &rack->handle;
	return 0;
}

static int sceSasAsMixerRackUpdate(SceSasAsHandle *handle, const SceSasAsRenderInfo *render)
{
	SceSasAsMixerRack *rack = (SceSasAsMixerRack *)handle;
	SceSasAsMixerRackState *state = rack->state;
	SceUInt32 updated, count;
	SceUInt8 enable;

	if (rack->updated == 0)
		return 0;
	sceSasAsSpinLockMask(&rack->lock, ~0u);
	updated = rack->updated;
	sceSasAsAtomicAnd(&rack->updated, 0);
	if (updated & SCE_SAS_AS_MIXER_UPDATED_INPUTS)
		sceSasAsRackIoTableSetInputs(&rack->io, &state->io);
	if (updated & SCE_SAS_AS_MIXER_UPDATED_OUTPUTS)
		sceSasAsRackIoTableSetOutputs(&rack->io, &state->io);
	if (updated & SCE_SAS_AS_MIXER_UPDATED_MATRIX) {
		enable = rack->matrix.enable != 0;
		if (enable) {
			const float *level = rack->matrix.levels;
			SceInt16 *fixed = state->levels;

			for (count = rack->numChannels * rack->numChannels; count != 0; count--)
				*fixed++ = sceSasAsLevelToFixed(*level++);
		}
		state->enable = enable;
	}
	sceSasAsSpinTryUnlockMask(&rack->lock, ~0u);
	return 0;
}

static int sceSasAsMixerRackLock(SceSasAsHandle *handle, SceUInt32 block, void **pointer)
{
	SceSasAsMixerRack *rack = (SceSasAsMixerRack *)handle;
	SceUInt32 bit;

	switch (block >> 16) {
	case SCE_SAS_AS_RACK_BLOCK_INPUTS >> 16:
		*pointer = &rack->io.inputs;
		bit = SCE_SAS_AS_MIXER_UPDATED_INPUTS;
		break;
	case SCE_SAS_AS_RACK_BLOCK_OUTPUTS >> 16:
		*pointer = &rack->io.outputs;
		bit = SCE_SAS_AS_MIXER_UPDATED_OUTPUTS;
		break;
	case SCE_SAS_AS_MIXER_BLOCK_MATRIX:
		*pointer = &rack->matrix;
		bit = SCE_SAS_AS_MIXER_UPDATED_MATRIX;
		break;
	default:
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	}
	sceSasAsSpinLockMask(&rack->lock, bit);
	return 0;
}

/*
 * The task: the inputs that are not muted into one set of channel buffers (the first
 * replacing, the others adding), through the matrix when it is enabled, then the buffers into
 * each output's buss (replacing it, or adding when the output's flag is set).
 */
static int sceSasAsMixerRackRun(const SceSasAsRenderInfo *render, SceSasAsTaskBuffer *buffers,
				SceUInt32 numBuffers)
{
	SceUInt32 numChannels = render->numChannels;
	SceSasAsMixerRackState *state = buffers[0].pointer;
	SceSasAsMixerRackIo *io = buffers[1].pointer;
	SceInt32 *mix[numChannels];
	SceInt32 *const *busses;
	const SceUInt8 *buss, *mute;
	const SceInt16 *level;
	SceInt16 *lastLevel;
	SceUInt32 mask, port, ch, i, j, used;

	for (ch = 0; ch < numChannels; ch++)
		mix[ch] = __builtin_alloca(render->granularity * sizeof(SceInt32));

	buss = SCE_SAS_AS_AT(state->io.inputBussOffset);
	mute = SCE_SAS_AS_AT(state->io.inputMuteOffset);
	level = SCE_SAS_AS_AT(state->io.inputLevelOffset);
	lastLevel = SCE_SAS_AS_AT(io->io.inputLevelOffset);

	used = 0;
	for (mask = state->io.inputMask, port = 0; mask != 0; mask >>= 1, port++) {
		SceInt32 to = level[port];

		if ((mask & 1) != 0 && buss[port] < render->numBusses && mute[port] == 0) {
			for (ch = 0; ch < numChannels; ch++) {
				SceInt32 from = lastLevel[port];

				busses = (SceInt32 **)render->bussBuffers;
				sceSasAsRackMix(mix[ch], busses[buss[port] * render->numChannels + ch],
						render->granularity, used, from, to,
						(to << 16) - (from << 16));
			}
			used = 1;
		}
		lastLevel[port] = to;
	}
	if (!used) {
		for (ch = 0; ch < numChannels; ch++) {
			for (i = 0; i < render->granularity; i++)
				mix[ch][i] = 0;
		}
	}

	if (state->enable) {
		SceInt32 *input[render->numChannels];

		for (ch = 0; ch < render->numChannels; ch++) {
			input[ch] = __builtin_alloca(render->granularity * sizeof(SceInt32));
			for (i = 0; i < render->granularity; i++)
				input[ch][i] = mix[ch][i];
		}

		level = SCE_SAS_AS_AT(state->levelsOffset);
		lastLevel = SCE_SAS_AS_AT(io->levelsOffset);
		for (i = 0; i < render->numChannels; i++) {
			for (j = 0; j < render->numChannels; j++) {
				SceInt32 from = *lastLevel;
				SceInt32 to = *level++;

				sceSasAsRackMix(mix[i], input[i], render->granularity, i != 0, from, to,
						(to << 16) - (from << 16));
				*lastLevel++ = to;
			}
		}
	}

	numChannels = render->numChannels;
	buss = SCE_SAS_AS_AT(state->io.outputBussOffset);
	mute = SCE_SAS_AS_AT(state->io.outputMuteOffset);
	level = SCE_SAS_AS_AT(state->io.outputLevelOffset);
	lastLevel = SCE_SAS_AS_AT(io->io.outputLevelOffset);
	for (mask = state->io.outputMask, port = 0; mask != 0; mask >>= 1, port++) {
		SceInt32 to = level[port];

		if ((mask & 1) != 0 && buss[port] < render->numBusses && mute[port] == 0) {
			SceUInt32 add = state->io.outputFlags & (1u << port);
			SceInt32 from = lastLevel[port];

			for (ch = 0; ch < numChannels; ch++) {
				busses = (SceInt32 **)render->bussBuffers;
				sceSasAsRackMix(busses[buss[port] * numChannels + ch], mix[ch],
						render->granularity, add, from, to,
						(to << 16) - (from << 16));
			}
		}
		lastLevel[port] = to;
	}
	return 0;
}

const SceSasAsClass sceSasAsMixerRackClass = {
	.id = SCE_SAS_AS_CLASS_RACK,
	.name = "MIXER",
	.version = SCE_SAS_AS_CLASS_VERSION,
	.create = sceSasAsMixerRackCreate,
	.destroy = sceSasAsMixerRackDestroy,
	.ops = {
		.lock = sceSasAsMixerRackLock,
		.unlock = sceSasAsMixerRackUnlock,
		.update = sceSasAsMixerRackUpdate,
		.render = sceSasAsMixerRackRender,
		.renderEnd = sceSasAsMixerRackRenderEnd,
	},
};
