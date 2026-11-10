/*
 * The MASTER rack: its task takes the input busses through a compressor and the master
 * volume into the render's output.
 */

#include <stddef.h>
#include <kernel/threadmgr.h>

#include "as_ssp.h"
#include "as_math.h"
#include "as_master.h"

static int sceSasAsMasterRackDestroy(SceSasAsHandle *handle)
{
	return 0;
}

/* the update bit of a block, 0 when it has none */
static inline SceUInt32 sceSasAsMasterRackBlockBit(SceUInt32 block)
{
	switch (block >> 16) {
	case SCE_SAS_AS_MASTER_BLOCK_COMPRESSOR:
		return SCE_SAS_AS_MASTER_UPDATED_COMPRESSOR;
	case SCE_SAS_AS_MASTER_BLOCK_BUFFERS:
		return SCE_SAS_AS_MASTER_UPDATED_BUFFERS;
	case SCE_SAS_AS_MASTER_BLOCK_VOLUME:
		return SCE_SAS_AS_MASTER_UPDATED_VOLUME;
	case SCE_SAS_AS_RACK_BLOCK_INPUTS >> 16:
		return SCE_SAS_AS_MASTER_UPDATED_INPUTS;
	default:
		return 0;
	}
}

static int sceSasAsMasterRackUnlock(SceSasAsHandle *handle, SceUInt32 block)
{
	SceSasAsMasterRack *rack = (SceSasAsMasterRack *)handle;
	SceUInt32 bit;

	bit = sceSasAsMasterRackBlockBit(block);
	if (bit == 0)
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	sceSasAsAtomicOr(&rack->updated, bit);
	return sceSasAsSpinTryUnlockMask(&rack->lock, bit);
}

static int sceSasAsMasterRackRenderEnd(SceSasAsHandle *handle, const SceSasAsRenderInfo *render)
{
	return 0;
}

static int sceSasAsMasterRackRender(SceSasAsHandle *handle, const SceSasAsRenderInfo *render,
				    SceSasAsTaskList *tasks)
{
	SceSasAsMasterRack *rack = (SceSasAsMasterRack *)handle;

	sceSasAsTaskListAdd(tasks, &rack->task);
	return 0;
}

static int sceSasAsMasterRackRun(const SceSasAsRenderInfo *render, SceSasAsTaskBuffer *buffers,
				 SceUInt32 numBuffers);

static int sceSasAsMasterRackCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
				    SceSasAsHandle **handle)
{
	SceUInt32 numBuffers;
	SceSasAsMasterRack *rack;
	SceSasAsMasterRackState *state;
	SceSasAsMasterRackIo *taskIo;

	sceSasAsConfigGetUInt(config, "numBuffers", 1, &numBuffers);
	rack = sceSasAsAllocatorAllocAligned(allocator, sizeof(*rack), 16, 1);
	if (rack != NULL) {
		rack->task.run = sceSasAsMasterRackRun;
		rack->maxBuffers = numBuffers;
		rack->task.numBuffers = 2;
		rack->task.type = SCE_SAS_AS_TASK_SERIAL;
		sceSasAsRackIoParamCreate(allocator, config, &rack->handle,
					  offsetof(SceSasAsMasterRack, io));
		rack->io.inputs.ports[0].level = 1.0f;
		rack->io.inputs.mask = 1;
		rack->compressor.gain = 0.0f;
		rack->compressor.threshold = 0.0f;
		rack->volume = 1.0f;
		rack->compressor.ratio = 50.0f;
		rack->numBuffers = numBuffers;
		rack->buffersLocked.block = SCE_SAS_AS_MASTER_BLOCK_BUFFERS << 16;
		rack->compressor.attack = 10.0f;
		rack->compressor.release = 100.0f;
		rack->volumeLocked.block = SCE_SAS_AS_MASTER_BLOCK_VOLUME << 16;
		rack->compressorLocked.block = SCE_SAS_AS_MASTER_BLOCK_COMPRESSOR << 16;
		rack->compressor.enable = 0;
		rack->compressorLocked.handle = &rack->handle;
		rack->buffersLocked.handle = &rack->handle;
		rack->volumeLocked.handle = &rack->handle;
		state = sceSasAsAllocatorAlloc(allocator, sizeof(*state), 1);
		sceSasAsRackIoTableCreate(allocator, config, state, 0);
		state->io.inputMask = 1;
		state->io.inputLevel[0] = 0x1000;
		state->maxBuffers = numBuffers;
		state->volume = 0x4000;
		state->numBuffers = 1;
		rack->state = state;
		rack->task.buffers[0].index = 0;
		rack->task.buffers[0].pointer = state;
		rack->task.buffers[0].offset = sceSasAsAllocatorGetOffset(allocator, (SceUIntPtr)state);
		taskIo = sceSasAsAllocatorAlloc(allocator, sizeof(*taskIo), 1);
		sceSasAsRackIoTaskCreate(allocator, config, taskIo, 0);
		taskIo->mode = 0x1000000;
		rack->taskIo = taskIo;
		rack->task.buffers[1].index = 1;
		rack->task.buffers[1].pointer = taskIo;
		rack->task.buffers[1].offset = sceSasAsAllocatorGetOffset(allocator, (SceUIntPtr)taskIo);
	} else {
		sceSasAsRackIoParamCreate(allocator, config, NULL, offsetof(SceSasAsMasterRack, io));
		state = sceSasAsAllocatorAlloc(allocator, sizeof(*state), 1);
		sceSasAsRackIoTableCreate(allocator, config, state, 0);
		taskIo = sceSasAsAllocatorAlloc(allocator, sizeof(*taskIo), 1);
		sceSasAsRackIoTaskCreate(allocator, config, taskIo, 0);
	}
	if (handle != NULL)
		*handle = &rack->handle;
	return 0;
}

/* a gain in dB as 2.14 fixed point, saturated to 16 bits */
static inline SceInt32 sceSasAsMasterRackVolume(float volume)
{
	float value = sceSasAsDbToLinear(volume, SCE_SAS_AS_MASTER_MIN_DB) * 16384.0f;

	if (value < -32768.0f)
		return -32768;
	if (value > 32767.0f)
		return 32767;
	return (SceInt16)value;
}

static int sceSasAsMasterRackUpdate(SceSasAsHandle *handle, const SceSasAsRenderInfo *render)
{
	SceSasAsMasterRack *rack = (SceSasAsMasterRack *)handle;
	SceSasAsMasterRackState *state = rack->state;
	SceUInt32 updated;
	SceInt32 ratio;
	SceInt32 numBuffers;
	float attack;
	float release;

	if (rack->updated == 0)
		return 0;
	sceSasAsSpinLockMask(&rack->lock, ~0u);
	updated = rack->updated;
	sceSasAsAtomicAnd(&rack->updated, 0);
	if (updated & SCE_SAS_AS_MASTER_UPDATED_INPUTS)
		sceSasAsRackIoTableSetInputs(&rack->io, &state->io);
	if (updated & SCE_SAS_AS_MASTER_UPDATED_COMPRESSOR) {
		if (rack->compressor.ratio <= 0.0f)
			ratio = 0x4000;
		else if (rack->compressor.ratio >= 50.0f)
			ratio = 0;
		else
			ratio = (SceInt32)(16384.0f / rack->compressor.ratio);
		attack = (float)render->sampleRate * rack->compressor.attack;
		release = (float)render->sampleRate * rack->compressor.release;
		attack = attack > 1.0f ? attack : 1.0f;
		release = release > 1.0f ? release : 1.0f;
		attack = sceSasAsExpf(9.31322575e-10f) / attack;
		release = sceSasAsExpf(9.31322575e-10f) / release;
		state->enable = rack->compressor.enable;
		state->mode = rack->compressor.mode;
		state->gain = (SceInt32)(sceSasAsDbToLinear(rack->compressor.gain,
							    SCE_SAS_AS_MASTER_MIN_DB) * 16384.0f);
		state->attack = (SceInt32)(attack * 1073741824.0f);
		state->release = (SceInt32)(release * 1073741824.0f);
		state->threshold = (SceInt32)(sceSasAsDbToLinear(rack->compressor.threshold,
								 SCE_SAS_AS_MASTER_MIN_DB) * 32768.0f);
		state->ratio = ratio;
	}
	if (updated & SCE_SAS_AS_MASTER_UPDATED_BUFFERS) {
		numBuffers = rack->numBuffers;
		if (numBuffers <= 0)
			numBuffers = 1;
		else if ((SceInt32)rack->maxBuffers < numBuffers)
			numBuffers = rack->maxBuffers;
		state->numBuffers = numBuffers;
	}
	if (updated & SCE_SAS_AS_MASTER_UPDATED_VOLUME)
		state->volume = sceSasAsMasterRackVolume(rack->volume);
	sceSasAsSpinTryUnlockMask(&rack->lock, ~0u);
	return 0;
}

static int sceSasAsMasterRackLock(SceSasAsHandle *handle, SceUInt32 block, void **pointer)
{
	SceSasAsMasterRack *rack = (SceSasAsMasterRack *)handle;
	SceUInt32 bit;

	switch (block >> 16) {
	case SCE_SAS_AS_MASTER_BLOCK_COMPRESSOR:
		*pointer = &rack->compressor;
		bit = SCE_SAS_AS_MASTER_UPDATED_COMPRESSOR;
		break;
	case SCE_SAS_AS_MASTER_BLOCK_BUFFERS:
		*pointer = &rack->numBuffers;
		bit = SCE_SAS_AS_MASTER_UPDATED_BUFFERS;
		break;
	case SCE_SAS_AS_MASTER_BLOCK_VOLUME:
		*pointer = &rack->volume;
		bit = SCE_SAS_AS_MASTER_UPDATED_VOLUME;
		break;
	case SCE_SAS_AS_RACK_BLOCK_INPUTS >> 16:
		*pointer = &rack->io.inputs;
		bit = SCE_SAS_AS_MASTER_UPDATED_INPUTS;
		break;
	default:
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	}
	sceSasAsSpinLockMask(&rack->lock, bit);
	return 0;
}

/* a sample to 16 bits */
static inline SceInt32 sceSasAsMasterRackSaturate(SceInt32 sample)
{
	if (sample > 32767)
		sample = 32767;
	if (sample < -32768)
		sample = -32768;
	return sample;
}

/*
 * The task: each input into the buffer it names, the compressor over the first buffer in
 * blocks of 32 samples (its level the peak or the RMS of the block), the volume over every
 * buffer, and the buffers in use into the output, interleaved and saturated to 16 bits.
 */
static int sceSasAsMasterRackRun(const SceSasAsRenderInfo *render, SceSasAsTaskBuffer *buffers,
				 SceUInt32 numBuffers)
{
	SceSasAsMasterRackState *state = buffers[0].pointer;
	SceUInt32 numChannels = render->numChannels;
	SceUInt32 maxBuffers = state->maxBuffers;
	SceSasAsMasterRackIo *io = buffers[1].pointer;
	SceUInt32 count = numChannels * maxBuffers;
	SceInt32 *mix[count];
	const SceUInt8 *inputBuss, *inputBuffer;
	const SceInt16 *inputLevel;
	SceInt16 *lastLevel;
	SceUInt32 mask, used, input, ch, i, j;
	SceInt32 volume;

	for (i = 0; i < count; i++)
		mix[i] = __builtin_alloca(render->granularity * sizeof(SceInt32));

	inputBuss = SCE_SAS_AS_AT(state->io.inputBussOffset);
	inputBuffer = SCE_SAS_AS_AT(state->io.inputMuteOffset);
	inputLevel = SCE_SAS_AS_AT(state->io.inputLevelOffset);
	lastLevel = SCE_SAS_AS_AT(io->io.inputLevelOffset);

	used = 0;
	for (mask = state->io.inputMask, input = 0; mask != 0; mask >>= 1, input++) {
		SceInt32 to = inputLevel[input];

		if ((mask & 1) != 0 && inputBuss[input] < render->numBusses &&
		    inputBuffer[input] < maxBuffers) {
			SceUInt32 bit = 1u << inputBuffer[input];
			SceInt32 **buffer = &mix[inputBuffer[input] * numChannels];

			for (ch = 0; ch < numChannels; ch++) {
				SceInt32 from = lastLevel[input];
				SceInt32 *const *busses = (SceInt32 **)render->bussBuffers;

				sceSasAsRackMix(buffer[ch],
						busses[inputBuss[input] * render->numChannels + ch],
						render->granularity, used & bit, from, to,
						(to << 16) - (from << 16));
			}
			used |= bit;
		}
		lastLevel[input] = to;
	}

	for (j = 0; j < maxBuffers; j++) {
		if ((used & (1u << j)) != 0)
			continue;
		for (ch = 0; ch < numChannels; ch++) {
			SceInt32 *buffer = mix[j * numChannels + ch];

			for (i = 0; i < render->granularity; i++)
				buffer[i] = 0;
		}
	}

	if (state->enable) {
		SceInt64 gain = state->gain;
		SceUInt32 done, chunk;

		for (done = 0; done < render->granularity; done += chunk) {
			SceInt32 remaining = render->granularity - done;
			SceInt32 level, target, coefficient, envelope, current;

			chunk = remaining < 32 ? remaining : 32;

			if (state->mode == 0) {
				SceInt32 minimum = 0, maximum = 0;

				for (ch = 0; ch < render->numChannels; ch++) {
					const SceInt32 *sample = mix[ch] + done;

					for (i = 0; i < chunk; i++) {
						if (sample[i] < minimum)
							minimum = sample[i];
						if (sample[i] > maximum)
							maximum = sample[i];
					}
				}
				level = -minimum;
				if (level < maximum)
					level = maximum;
			} else {
				level = 0;
				for (ch = 0; ch < render->numChannels; ch++) {
					const SceInt32 *sample = mix[ch] + done;
					SceInt64 sum = 0;
					SceInt32 rms;

					for (i = 0; i < chunk; i++)
						sum += ((SceInt64)sample[i] * sample[i]) >> 5;
					rms = (SceInt32)sceSasAsSqrtf((float)sum * 32.0f / (float)chunk);
					if (level < rms)
						level = rms;
				}
			}

			current = io->mode;
			if (level > state->threshold)
				target = (SceInt32)(((SceInt64)(state->threshold +
						     (((level - state->threshold) * state->ratio) >> 14))
						     << 24) / level);
			else
				target = 0x1000000;
			coefficient = target < current ? state->attack : state->release;

			envelope = current;
			for (ch = 0; ch < render->numChannels; ch++) {
				SceInt32 *sample = mix[ch] + done;

				envelope = current;
				for (i = 0; i < chunk; i++) {
					envelope += (SceInt32)(((SceInt64)(target - envelope) *
								coefficient) >> 30);
					sample[i] = (SceInt32)(((((SceInt64)envelope * sample[i]) >> 24) *
								gain) >> 14);
				}
			}
			io->mode = envelope;
		}
	}

	volume = state->volume;
	if (volume != 0x4000) {
		count = state->maxBuffers * render->numChannels;
		if (volume > 0x4000) {
			for (j = 0; j < count; j++) {
				SceInt32 *sample = mix[j];

				for (i = 0; i < render->granularity; i++)
					sample[i] = sceSasAsMasterRackSaturate(
						(SceInt32)(((SceInt64)sample[i] * volume) >> 14));
			}
		} else {
			for (j = 0; j < count; j++) {
				SceInt32 *sample = mix[j];

				for (i = 0; i < render->granularity; i++)
					sample[i] = (SceInt16)((sceSasAsMasterRackSaturate(sample[i]) *
								volume) >> 14);
			}
		}
	}

	count = state->numBuffers * render->numChannels;
	for (j = 0; j < count; j++) {
		const SceInt32 *sample = mix[j];
		SceInt16 *output = (SceInt16 *)render->output + j;

		for (i = 0; i < render->granularity; i++)
			output[i * count] = sceSasAsMasterRackSaturate(sample[i]);
	}
	return 0;
}

const SceSasAsClass sceSasAsMasterRackClass = {
	.id = SCE_SAS_AS_CLASS_RACK,
	.name = "MASTER",
	.version = SCE_SAS_AS_CLASS_VERSION,
	.create = sceSasAsMasterRackCreate,
	.destroy = sceSasAsMasterRackDestroy,
	.ops = {
		.lock = sceSasAsMasterRackLock,
		.unlock = sceSasAsMasterRackUnlock,
		.update = sceSasAsMasterRackUpdate,
		.render = sceSasAsMasterRackRender,
		.renderEnd = sceSasAsMasterRackRenderEnd,
	},
};
