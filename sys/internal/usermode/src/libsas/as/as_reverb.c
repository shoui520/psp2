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

/* the effect types' parameters, by SCE_SAS_FX_TYPE_* */
static const SceSasAsReverbPreset sceSasAsReverbPresets[] = {
	{ 0x0200, 0x007d, 0x005b, 0x06d8, { 0x0a97, 0xf7da, 0x0000, 0x0000 }, 0x4580, 0x5800,
	  0x5300, 0x04d6, 0x0333, 0x03f0, 0x0227, 0x0374, 0x01ef, 0x0336, 0x01b7, 0x0335, 0x01b6,
	  0x0334, 0x01b5, 0x0334, 0x01b5, 0x0334, 0x01b5, 0x01b4, 0x0136, 0x00b8 },
	{ 0x0100, 0x0033, 0x0025, 0x070f, { 0x09f5, 0xf79c, 0x0882, 0xf81e }, 0x6400, 0x5280,
	  0x4ec0, 0x03e4, 0x031b, 0x03a4, 0x02af, 0x0372, 0x0266, 0x031c, 0x025d, 0x025c, 0x018e,
	  0x022f, 0x0135, 0x01d2, 0x00b7, 0x018f, 0x00b5, 0x00b4, 0x0080, 0x004c },
	{ 0x0200, 0x00b1, 0x007f, 0x070f, { 0x09f5, 0xf79c, 0x08a2, 0xf7de }, 0x4b40, 0x5280,
	  0x4ec0, 0x0904, 0x076b, 0x0824, 0x065f, 0x07a2, 0x0616, 0x076c, 0x05ed, 0x05ec, 0x042e,
	  0x050f, 0x0305, 0x0462, 0x02b7, 0x042f, 0x0265, 0x0264, 0x01b2, 0x0100 },
	{ 0x0400, 0x00e3, 0x00a9, 0x06f6, { 0x09f5, 0xf79c, 0x08a2, 0xf7de }, 0x5980, 0x5680,
	  0x52c0, 0x0dfb, 0x0b58, 0x0d09, 0x0a3c, 0x0bd9, 0x0973, 0x0b59, 0x08da, 0x08d9, 0x05e9,
	  0x07ec, 0x04b0, 0x06ef, 0x03d2, 0x05ea, 0x031d, 0x031c, 0x0238, 0x0154 },
	{ 0x0400, 0x01a5, 0x0139, 0x0600, { 0x0a00, 0x0980, 0xf700, 0xf780 }, 0x4000, 0x6000,
	  0x5c00, 0x15ba, 0x11bb, 0x14c2, 0x10bd, 0x11bc, 0x0dc1, 0x11c0, 0x0dc3, 0x0dc0, 0x09c1,
	  0x0bc4, 0x07c1, 0x0a00, 0x06cd, 0x09c2, 0x05c1, 0x05c0, 0x041a, 0x0274 },
	{ 0x0800, 0x033d, 0x0231, 0x07e0, { 0x0a00, 0xf680, 0xf600, 0x0980 }, 0x5000, 0x6000,
	  0x5400, 0x1ed6, 0x1a31, 0x1d14, 0x183b, 0x1bc2, 0x16b2, 0x1a32, 0x15ef, 0x15ee, 0x1055,
	  0x1334, 0x0f2d, 0x11f6, 0x0c5d, 0x1056, 0x0ae1, 0x0ae0, 0x07a2, 0x0464 },
	{ 0x1000, 0x0003, 0x0003, 0x0800, { 0x1000, 0x0000, 0x0000, 0x0000 }, 0x7f00, 0x0000,
	  0x0000, 0x1ffd, 0x0ffd, 0x1009, 0x0009, 0x0000, 0x0000, 0x1009, 0x0009, 0x1fff, 0x1fff,
	  0x1ffe, 0x1ffe, 0x1ffe, 0x1ffe, 0x1ffe, 0x1ffe, 0x1008, 0x1004, 0x0008 },
	{ 0x1000, 0x0003, 0x0003, 0x0800, { 0x1000, 0x0000, 0x0000, 0x0000 }, 0x0000, 0x0000,
	  0x0000, 0x1ffd, 0x0ffd, 0x1009, 0x0009, 0x0000, 0x0000, 0x1009, 0x0009, 0x1fff, 0x1fff,
	  0x1ffe, 0x1ffe, 0x1ffe, 0x1ffe, 0x1ffe, 0x1ffe, 0x1008, 0x1004, 0x0008 },
	{ 0x0100, 0x0017, 0x0013, 0x070f, { 0x09f5, 0xf79c, 0x08a2, 0xf7de }, 0x7b00, 0x5f80,
	  0x54c0, 0x0371, 0x02af, 0x02e5, 0x01df, 0x02b0, 0x01d7, 0x0358, 0x026a, 0x01d6, 0x011e,
	  0x012d, 0x00b1, 0x011f, 0x0059, 0x01a0, 0x00e3, 0x0058, 0x0040, 0x0028 },
};

static inline SceInt32 sceSasAsReverbClamp(SceInt32 value)
{
	if (value < -32768)
		value = -32768;
	if (value > 32767)
		value = 32767;
	return value;
}

/* sets the work area up for the effect type, from its preset and the delay and feedback */
static inline void sceSasAsReverbRackSetUp(SceSasAsReverbRackWork *work,
					   const SceSasAsReverbRackState *state)
{
	const SceSasAsReverbPreset *preset = &sceSasAsReverbPresets[(SceInt8)state->type];
	SceInt16 size = preset->size;
	SceUInt16 leftSame = preset->leftSame;
	SceUInt16 rightSame = preset->rightSame;
	SceUInt16 leftComb1 = preset->leftComb1;
	SceUInt16 leftSameDelay = preset->leftSameDelay;
	SceUInt16 apf1LeftDelay = preset->apf1LeftDelay;
	SceUInt16 apf1RightDelay = preset->apf1RightDelay;
	SceUInt16 wallLevel = preset->wallLevel;
	SceUInt16 tap, max;

	if ((SceUInt32)(state->type - SCE_SAS_FX_TYPE_ECHO) <= 1) {
		SceUInt32 delay = (state->delay + 1) * 16;

		leftSame = delay * 2 - preset->echoDelay;
		rightSame = delay - preset->apf2Delay;
		leftComb1 = delay + preset->rightComb1;
		leftSameDelay = preset->rightSameDelay + delay;
		apf1LeftDelay = preset->echoApfDelay + delay;
		apf1RightDelay = delay + (preset->echoApfDelay >> 1);
		wallLevel = state->feedback << 8;
	}

	work->apf1Left = work->ring[size];
	work->apf1Right = work->apf1Left + apf1LeftDelay;
	work->apf2 = work->apf1Right + apf1RightDelay;
	work->apf1LeftLength = apf1LeftDelay;
	work->apf1RightLength = apf1RightDelay;
	work->apf2Length = preset->apf2Delay * 2;
	work->apf1Level = preset->apf1Level;
	work->apf2Level = preset->apf2Level;
	work->iirLevel = preset->iirLevel;
	work->combLevel[0] = preset->combLevel[0];
	work->combLevel[1] = preset->combLevel[1];
	work->combLevel[2] = preset->combLevel[2];
	work->combLevel[3] = preset->combLevel[3];
	work->wallLevel = wallLevel;

	work->combTap[0][0] = leftSame - leftComb1;
	work->combTap[1][0] = leftSame - preset->leftComb2;
	work->reflectionTap[0] = leftSame - leftSameDelay;
	work->combTap[0][1] = preset->leftDiff - preset->leftComb3;
	work->combTap[1][1] = preset->leftDiff - preset->leftComb4;
	work->reflectionTap[1] = preset->leftDiff - preset->leftDiffDelay;
	work->combTap[0][2] = preset->rightDiff - preset->rightComb3;
	work->combTap[1][2] = preset->rightDiff - preset->rightComb4;
	work->reflectionTap[2] = preset->rightDiff - preset->rightDiffDelay;
	work->combTap[0][3] = rightSame - preset->rightComb1;
	work->combTap[1][3] = rightSame - preset->rightComb2;
	work->reflectionTap[3] = rightSame - preset->rightSameDelay;

	/* each reflection's feedback is its farthest tap */
	for (tap = 0; tap < 4; tap++) {
		max = work->reflectionTap[tap];
		if (work->combTap[1][tap] > max)
			max = work->combTap[1][tap];
		if (work->combTap[0][tap] > max)
			max = work->combTap[0][tap];
		work->feedbackTap[tap] = max;
	}

	work->mask = preset->size - 1;
	work->position = 0;
	work->apf2Position = 0;
	work->apf1RightPosition = 0;
	work->apf1LeftPosition = 0;
}

/* one reflection: the input less the wall's, into the ring through the IIR */
static inline SceInt16 sceSasAsReverbReflect(const SceSasAsReverbRackWork *work, SceUInt32 position,
					     SceInt32 input, SceUInt32 slot)
{
	SceUInt32 mask = work->mask;
	SceInt16 wall = work->ring[(position - work->reflectionTap[slot]) & mask][slot];
	SceInt16 last = work->ring[(position - work->feedbackTap[slot]) & mask][slot];
	SceInt32 value = sceSasAsReverbClamp((input - ((wall * work->wallLevel) >> 4)) >> 11);

	return last + (((value - last) * work->iirLevel) >> 11);
}

/* a comb filter's two reflections, through the two all-pass filters */
static inline SceInt32 sceSasAsReverbComb(const SceSasAsReverbRackWork *work, SceUInt32 position,
					  SceUInt32 same, SceUInt32 diff)
{
	SceUInt32 mask = work->mask;

	return work->ring[(position - work->combTap[1][same]) & mask][same] * work->combLevel[1] +
	       work->ring[(position - work->combTap[0][same]) & mask][same] * work->combLevel[0] +
	       work->ring[(position - work->combTap[0][diff]) & mask][diff] * work->combLevel[2] +
	       work->ring[(position - work->combTap[1][diff]) & mask][diff] * work->combLevel[3];
}

static inline SceInt32 sceSasAsReverbAllPass(SceInt16 *line, SceUInt32 position, SceInt32 input,
					     SceInt16 level)
{
	SceInt16 delayed = line[position];
	SceInt32 value = input - ((delayed * level) >> 15);

	line[position] = sceSasAsReverbClamp(value);
	return delayed + ((value * level) >> 15);
}

/* the effect at half rate: each even sample in, two samples out */
static inline void sceSasAsReverbRackProcess(SceSasAsReverbRackWork *work, SceInt32 *left,
					     SceInt32 *right, SceUInt32 granularity)
{
	SceInt32 lastLeft = work->lastLeft;
	SceInt32 lastRight = work->lastRight;
	SceUInt32 count, position;
	SceInt32 inLeft, inRight, value;
	SceInt32 outLeft, outRight;

	for (count = granularity >> 1; count != 0; count--) {
		position = work->position;
		inLeft = left[0] << 11;
		inRight = right[0] << 11;
		work->position = (position + 1) & work->mask;

		work->ring[position][0] = sceSasAsReverbReflect(work, position, inLeft, 0);
		work->ring[position][1] = sceSasAsReverbReflect(work, position, inLeft, 1);
		value = sceSasAsReverbAllPass(work->apf1Left, work->apf1LeftPosition,
					      sceSasAsReverbComb(work, position, 0, 1) >> 12,
					      work->apf1Level);
		if (++work->apf1LeftPosition == work->apf1LeftLength)
			work->apf1LeftPosition = 0;
		outLeft = sceSasAsReverbAllPass(work->apf2, work->apf2Position, value, work->apf2Level);

		work->ring[position][3] = sceSasAsReverbReflect(work, position, inRight, 3);
		work->ring[position][2] = sceSasAsReverbReflect(work, position, inRight, 2);
		value = sceSasAsReverbAllPass(work->apf1Right, work->apf1RightPosition,
					      sceSasAsReverbComb(work, position, 3, 2) >> 12,
					      work->apf1Level);
		if (++work->apf1RightPosition == work->apf1RightLength)
			work->apf1RightPosition = 0;
		outRight = sceSasAsReverbAllPass(work->apf2, work->apf2Position + 1, value,
						 work->apf2Level);
		work->apf2Position += 2;
		if (work->apf2Position == work->apf2Length)
			work->apf2Position = 0;

		left[0] = (lastLeft + outLeft) >> 2;
		right[0] = (lastRight + outRight) >> 2;
		left[1] = outLeft;
		right[1] = outRight;
		lastLeft = outLeft;
		lastRight = outRight;
		left += 2;
		right += 2;
	}
	work->lastLeft = lastLeft;
	work->lastRight = lastRight;
}

static int sceSasAsReverbRackRun(const SceSasAsRenderInfo *render, SceSasAsTaskBuffer *buffers,
				 SceUInt32 numBuffers)
{
	SceSasAsReverbRackState *state = buffers[0].pointer;
	SceSasAsReverbRackWork *work = buffers[1].pointer;
	SceUInt32 granularity = render->granularity;
	SceInt32 *buffer[4];
	SceInt32 *const *busses;
	const SceUInt8 *buss, *mute;
	const SceInt16 *level;
	SceInt16 *lastLevel;
	SceUInt32 mask, port, ch, i, used;
	SceInt16 *memory, *end;

	for (ch = 0; ch < 4; ch++)
		buffer[ch] = __builtin_alloca(granularity * sizeof(SceInt32));

	/* the inputs, into the dry pair */
	buss = SCE_SAS_AS_AT(state->io.inputBussOffset);
	mute = SCE_SAS_AS_AT(state->io.inputMuteOffset);
	level = SCE_SAS_AS_AT(state->io.inputLevelOffset);
	lastLevel = SCE_SAS_AS_AT(work->io.inputLevelOffset);
	used = 0;
	for (mask = state->io.inputMask, port = 0; mask != 0; mask >>= 1, port++) {
		SceInt32 to = level[port];

		if ((mask & 1) != 0 && buss[port] < render->numBusses && mute[port] == 0) {
			for (ch = 0; ch < 2; ch++) {
				SceInt32 from = lastLevel[port];

				busses = (SceInt32 **)render->bussBuffers;
				sceSasAsRackMix(buffer[ch], busses[buss[port] * render->numChannels + ch],
						granularity, used, from, to, (to << 16) - (from << 16));
			}
			used = 1;
		}
		lastLevel[port] = to;
	}
	if (!used) {
		for (i = 0; i < granularity; i++)
			buffer[0][i] = 0;
		for (i = 0; i < granularity; i++)
			buffer[1][i] = 0;
	}

	/* a new effect: set it up and clear its memory */
	if (work->type != state->type || work->feedback != state->feedback ||
	    work->delay != state->delay) {
		if (state->type <= SCE_SAS_FX_TYPE_PIPE)
			sceSasAsReverbRackSetUp(work, state);
		if ((state->type & 0x80) == 0 && (work->type != state->type ||
		    work->feedback != state->feedback || work->delay != state->delay)) {
			end = work->apf2 + work->apf2Length;
			for (memory = work->ring[0]; memory < end; memory++)
				*memory = 0;
			work->lastLeft = 0;
			work->lastRight = 0;
		}
		work->type = state->type;
		work->feedback = state->feedback;
		work->delay = state->delay;
		granularity = render->granularity;
	}

	/* the wet pair: the dry pair at the effect's input levels, through the effect */
	sceSasAsRackMix(buffer[2], buffer[0], granularity, 0, work->levels[2], state->levels[2],
			(state->levels[2] << 16) - (work->levels[2] << 16));
	sceSasAsRackMix(buffer[3], buffer[1], granularity, 0, work->levels[3], state->levels[3],
			(state->levels[3] << 16) - (work->levels[3] << 16));
	if (state->type > SCE_SAS_FX_TYPE_PIPE) {
		for (i = 0; i < granularity; i++)
			buffer[2][i] = 0;
		for (i = 0; i < granularity; i++)
			buffer[3][i] = 0;
	} else {
		sceSasAsReverbRackProcess(work, buffer[2], buffer[3], granularity);
	}

	/* and the dry pair on it, at the dry levels */
	sceSasAsRackMix(buffer[2], buffer[0], granularity, 1, work->levels[0], state->levels[0],
			(state->levels[0] << 16) - (work->levels[0] << 16));
	sceSasAsRackMix(buffer[3], buffer[1], granularity, 1, work->levels[1], state->levels[1],
			(state->levels[1] << 16) - (work->levels[1] << 16));
	work->levels[0] = state->levels[0];
	work->levels[2] = state->levels[2];
	work->levels[1] = state->levels[1];
	work->levels[3] = state->levels[3];

	/* the outputs */
	buss = SCE_SAS_AS_AT(state->io.outputBussOffset);
	mute = SCE_SAS_AS_AT(state->io.outputMuteOffset);
	level = SCE_SAS_AS_AT(state->io.outputLevelOffset);
	lastLevel = SCE_SAS_AS_AT(work->io.outputLevelOffset);
	for (mask = state->io.outputMask, port = 0; mask != 0; mask >>= 1, port++) {
		SceInt32 to = level[port];

		if ((mask & 1) != 0 && buss[port] < render->numBusses && mute[port] == 0) {
			SceUInt32 add = state->io.outputFlags & (1u << port);
			SceInt32 from = lastLevel[port];

			for (ch = 0; ch < 2; ch++) {
				busses = (SceInt32 **)render->bussBuffers;
				sceSasAsRackMix(busses[buss[port] * 2 + ch], buffer[2 + ch],
						render->granularity, add, from, to,
						(to << 16) - (from << 16));
			}
		}
		lastLevel[port] = to;
	}
	return 0;
}

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
