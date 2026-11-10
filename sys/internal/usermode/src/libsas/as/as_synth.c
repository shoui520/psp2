/*
 * The SYNTH rack: the SAS voices. The API sets a voice's parameters block by block (its
 * sources, pitch, ADSR, envelope, distortion, filters, volumes and sends) and gives it key
 * events; an update hands what changed to the voices' task in fixed point, and the task
 * reports each voice's status back after the render.
 */

#include <stddef.h>
#include <kernel/threadmgr.h>

#include "as_ssp.h"
#include "as_synth.h"

/* the status of a voice with nothing to report */
static SceSasAsSynthVoiceStatus sceSasAsSynthNoStatus;

static int sceSasAsSynthRackDestroy(SceSasAsHandle *handle)
{
	return 0;
}

static int sceSasAsSynthRackRender(SceSasAsHandle *handle, const SceSasAsRenderInfo *render,
				   SceSasAsTaskList *tasks)
{
	SceSasAsSynthRack *rack = (SceSasAsSynthRack *)handle;

	rack->info->status = rack->status[rack->statusIndex ^ 1];
	sceSasAsTaskListAdd(tasks, &rack->task);
	return 0;
}

static int sceSasAsSynthRackRun(const SceSasAsRenderInfo *render, SceSasAsTaskBuffer *buffers,
				SceUInt32 numBuffers);

static int sceSasAsSynthRackCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
				   SceSasAsHandle **handle)
{
	SceSasAsSynthRack *rack;
	SceUInt32 numProcessors, numChannels, numVoices, numVoiceSources, numVoiceEnvelopes;
	SceUInt32 numVoiceFilters, numVoiceVolumes, numVoiceSends;
	SceUInt32 numWords, index, i;
	volatile SceUInt32 *updatedVoices;
	SceUInt32 *changedVoices;
	SceSasAsSynthVoice *voices, *voice;
	SceSasAsSynthSource *sources;
	SceSasAsSynthEnvelopePoint *points;
	SceSasAsSynthFilter *filters;
	float *volumes;
	SceSasAsSynthSend *sends;
	SceSasAsSynthVoiceParam *param;
	SceSasAsSynthSource *paramSources;
	SceInt16 *envelopeLevels, *paramVolumes, *filterCoefficients, *sendLevels, *workVolumes;
	SceInt16 *workSendLevels;
	SceUInt32 *envelopeTimes;
	SceUInt8 *filterTypes, *sendBusses, *sourceState, *envelopeState;
	SceSasAsSynthVoiceWork *work;
	SceSasAsSynthVoiceStatus *status0, *status1;
	SceSasAsSynthTaskInfo *info;

	sceSasAsConfigGetUInt(config, "numProcessors", 8, &numProcessors);
	sceSasAsConfigGetUInt(config, "numChannels", 2, &numChannels);
	sceSasAsConfigGetUInt(config, "numVoices", 128, &numVoices);
	sceSasAsConfigGetUInt(config, "numVoiceSources", 8, &numVoiceSources);
	sceSasAsConfigGetUInt(config, "numVoiceEnvelopes", 8, &numVoiceEnvelopes);
	sceSasAsConfigGetUInt(config, "numVoiceFilters", 8, &numVoiceFilters);
	sceSasAsConfigGetUInt(config, "numVoiceVolumes", 1, &numVoiceVolumes);
	sceSasAsConfigGetUInt(config, "numVoiceSends", 8, &numVoiceSends);
	rack = sceSasAsAllocatorAllocAligned(allocator, sizeof(*rack), 16, 1);
	if (rack != NULL) {
		rack->numProcessors = numProcessors;
		rack->numChannels = numChannels;
		rack->task.run = sceSasAsSynthRackRun;
		rack->numVoices = numVoices;
		rack->numSources = numVoiceSources;
		rack->numVolumes = numVoiceVolumes;
		rack->numFilters = numVoiceFilters;
		rack->numSends = numVoiceSends;
		rack->task.type = SCE_SAS_AS_TASK_SERIAL;
		rack->task.numBuffers = 1;
	}

	/* the voices, as the API sets them */
	numWords = (numVoices + 31) >> 5;
	updatedVoices = sceSasAsAllocatorAlloc(allocator, numWords * sizeof(SceUInt32), 1);
	changedVoices = sceSasAsAllocatorAlloc(allocator, numWords * sizeof(SceUInt32), 1);
	voices = sceSasAsAllocatorAlloc(allocator, numVoices * sizeof(*voices), 1);
	sources = sceSasAsAllocatorAlloc(allocator, numVoices * numVoiceSources * sizeof(*sources), 1);
	points = sceSasAsAllocatorAlloc(allocator, numVoices * numVoiceEnvelopes * sizeof(*points), 1);
	filters = sceSasAsAllocatorAlloc(allocator, numVoices * numVoiceFilters * sizeof(*filters), 1);
	volumes = sceSasAsAllocatorAlloc(allocator,
					 numChannels * numVoiceVolumes * sizeof(*volumes) * numVoices, 1);
	sends = sceSasAsAllocatorAlloc(allocator, numVoices * numVoiceSends * sizeof(*sends), 1);
	if (voices != NULL) {
		for (index = 0; index < numVoices; index++) {
			voice = &voices[index];
			voice->sourcesLocked.handle = &rack->handle;
			voice->sourcesLocked.block = index;
			voice->sources.numSources = numVoiceSources;
			voice->sources.sources = sources;
			for (i = 0; i < numVoiceSources; i++) {
				voice->sources.sources[i].type = 0;
				voice->sources.sources[i].next = 0xff;
				voice->sources.sources[i].reserved1 = 0;
				voice->sources.sources[i].data[0] = 0;
			}
			voice->pitchLocked.handle = &rack->handle;
			voice->pitchLocked.block = index | (SCE_SAS_AS_SYNTH_BLOCK_PITCH << 16);
			voice->pitch = 1.0f;
			voice->adsrLocked.handle = &rack->handle;
			voice->adsrLocked.block = index | (SCE_SAS_AS_SYNTH_BLOCK_ADSR << 16);
			voice->envelopeLocked.handle = &rack->handle;
			voice->envelopeLocked.block = index | (SCE_SAS_AS_SYNTH_BLOCK_ENVELOPE << 16);
			voice->distortionLocked.handle = &rack->handle;
			voice->distortionLocked.block = index | (SCE_SAS_AS_SYNTH_BLOCK_DISTORTION << 16);
			voice->distortion.enable = 0;
			voice->distortion.gain = 1.0f;
			voice->distortion.level = 1.0f;
			voice->envelope.numPoints = numVoiceEnvelopes;
			voice->envelope.points = points;
			for (i = 0; i < numVoiceEnvelopes; i++) {
				points[i].time = 0;
				points[i].level = 1.0f;
			}
			voice->filtersLocked.handle = &rack->handle;
			voice->filtersLocked.block = index | (SCE_SAS_AS_SYNTH_BLOCK_FILTERS << 16);
			voice->filters.numFilters = numVoiceFilters;
			voice->filters.filters = filters;
			for (i = 0; i < numVoiceFilters; i++) {
				voice->filters.filters[i].type = 0;
				voice->filters.filters[i].mode = 0;
				voice->filters.filters[i].coefficients[0] = 0.0f;
				voice->filters.filters[i].coefficients[1] = 0.0f;
				voice->filters.filters[i].coefficients[2] = 0.0f;
				voice->filters.filters[i].coefficients[3] = 0.0f;
				voice->filters.filters[i].coefficients[4] = 0.0f;
			}
			/* (set a second time) */
			voice->distortionLocked.handle = &rack->handle;
			voice->distortionLocked.block = index | (SCE_SAS_AS_SYNTH_BLOCK_DISTORTION << 16);
			voice->volumesLocked.handle = &rack->handle;
			voice->volumesLocked.block = index | (SCE_SAS_AS_SYNTH_BLOCK_VOLUMES << 16);
			voice->volumes.numVolumes = numVoiceVolumes;
			voice->volumes.numChannels = numChannels;
			voice->volumes.levels = volumes;
			for (i = 0; i < numVoiceVolumes * numChannels; i++)
				volumes[i] = 1.0f;
			voice->sendsLocked.handle = &rack->handle;
			voice->sendsLocked.block = index | (SCE_SAS_AS_SYNTH_BLOCK_SENDS << 16);
			voice->sends.mask = 1;
			voice->sends.numSends = numVoiceSends;
			voice->sends.sends = sends;
			for (i = 0; i < numVoiceSends; i++) {
				voice->sends.sends[i].buss = 0;
				voice->sends.sends[i].level = 1.0f;
			}
			sources += numVoiceSources;
			points += numVoiceEnvelopes;
			filters += numVoiceFilters;
			volumes += numVoiceVolumes * numChannels;
			sends += numVoiceSends;
		}
	}
	if (rack != NULL) {
		rack->updatedVoices = updatedVoices;
		rack->voices = voices;
		rack->voiceStride = sceSasAsAllocatorGetOffset(allocator, (SceUIntPtr)updatedVoices) /
				    numVoices;
		rack->changedVoices = changedVoices;
		rack->numVoiceWords = numWords;
	}

	/* the voices' parameters, as their task reads them */
	for (index = 0; index < numVoices; index++) {
		param = sceSasAsAllocatorAlloc(allocator, sizeof(*param), 1);
		paramSources = sceSasAsAllocatorAlloc(allocator,
						      numVoiceSources * sizeof(*paramSources), 1);
		envelopeTimes = sceSasAsAllocatorAlloc(allocator,
						       numVoiceEnvelopes * sizeof(*envelopeTimes), 1);
		envelopeLevels = sceSasAsAllocatorAlloc(allocator,
							numVoiceEnvelopes * sizeof(*envelopeLevels), 1);
		paramVolumes = sceSasAsAllocatorAlloc(allocator, numChannels * numVoiceVolumes *
							     sizeof(*paramVolumes), 1);
		filterCoefficients = sceSasAsAllocatorAlloc(allocator, numVoiceFilters * 5 *
								   sizeof(*filterCoefficients), 1);
		sendLevels = sceSasAsAllocatorAlloc(allocator, numVoiceSends * sizeof(*sendLevels), 1);
		filterTypes = sceSasAsAllocatorAlloc(allocator, numVoiceFilters * 2, 1);
		sendBusses = sceSasAsAllocatorAlloc(allocator, numVoiceSends * 2, 1);
		if (param != NULL) {
			param->sources = paramSources;
			param->envelopeLevels = envelopeLevels;
			param->envelopeTimes = envelopeTimes;
			param->filterTypes = filterTypes;
			param->filterCoefficients = filterCoefficients;
			param->volumes = paramVolumes;
			param->sendBusses = sendBusses;
			param->sendLevels = sendLevels;
			param->pitch = 0x4000;
			param->sourcesOffset = (SceUIntPtr)paramSources -
					       (SceUIntPtr)&param->sourcesOffset;
			param->envelopeLevelsOffset = (SceUIntPtr)envelopeLevels -
						      (SceUIntPtr)&param->envelopeLevelsOffset;
			param->envelopeTimesOffset = (SceUIntPtr)envelopeTimes -
						     (SceUIntPtr)&param->envelopeTimesOffset;
			param->filterTypesOffset = (SceUIntPtr)filterTypes -
						   (SceUIntPtr)&param->filterTypesOffset;
			param->filterCoefficientsOffset = (SceUIntPtr)filterCoefficients -
							  (SceUIntPtr)&param->filterCoefficientsOffset;
			param->volumesOffset = (SceUIntPtr)paramVolumes - (SceUIntPtr)&param->volumesOffset;
			param->sendBussesOffset = (SceUIntPtr)sendBusses -
						  (SceUIntPtr)&param->sendBussesOffset;
			param->sendLevelsOffset = (SceUIntPtr)sendLevels -
						  (SceUIntPtr)&param->sendLevelsOffset;
			for (i = 0; i < numVoiceVolumes * numChannels; i++)
				paramVolumes[i] = 0x1000;
			param->sendMask = 1;
			sendLevels[0] = 0x1000;
			param->maxSources = numVoiceSources;
			param->numVolumes = numVoiceVolumes;
			param->numFilters = numVoiceFilters;
			param->numSends = numVoiceSends;
		}
		if (index == 0 && rack != NULL) {
			rack->params = param;
			rack->paramStride = sceSasAsAllocatorGetOffset(allocator, (SceUIntPtr)param);
		}
	}

	/* the voices' work areas */
	for (index = 0; index < numVoices; index++) {
		work = sceSasAsAllocatorAlloc(allocator, sizeof(*work), 1);
		sourceState = sceSasAsAllocatorAlloc(allocator, 10, 1);
		envelopeState = sceSasAsAllocatorAlloc(allocator, 8, 1);
		workVolumes = sceSasAsAllocatorAlloc(allocator, numChannels * numVoiceVolumes *
							    sizeof(*workVolumes), 1);
		workSendLevels = sceSasAsAllocatorAlloc(allocator,
							numVoiceSends * sizeof(*workSendLevels), 1);
		if (work != NULL) {
			work->sourceStateOffset = (SceUIntPtr)sourceState -
						  (SceUIntPtr)&work->sourceStateOffset;
			work->envelopeStateOffset = (SceUIntPtr)envelopeState -
						    (SceUIntPtr)&work->envelopeStateOffset;
			work->volumesOffset = (SceUIntPtr)workVolumes - (SceUIntPtr)&work->volumesOffset;
			work->sendLevelsOffset = (SceUIntPtr)workSendLevels -
						 (SceUIntPtr)&work->sendLevelsOffset;
		}
		if (index == 0 && rack != NULL) {
			rack->works = work;
			rack->workStride = sceSasAsAllocatorGetOffset(allocator, (SceUIntPtr)work);
		}
	}

	/* the voices' status, one for the render and one for the API */
	status0 = sceSasAsAllocatorAlloc(allocator, numVoices * sizeof(*status0), 1);
	status1 = sceSasAsAllocatorAlloc(allocator, numVoices * sizeof(*status1), 1);
	if (rack != NULL) {
		rack->status[0] = status0;
		rack->status[1] = status1;
		rack->statusIndex = 0;
		info = sceSasAsAllocatorAlloc(allocator, sizeof(*info), 1);
		if (info != NULL) {
			info->numVoices = numVoices;
			info->numSends = numVoiceSends;
			info->numFilters = numVoiceFilters;
			info->numVolumes = numVoiceVolumes;
			info->params = rack->params;
			info->paramStride = rack->paramStride;
			info->works = rack->works;
			info->workStride = rack->workStride;
			info->status = NULL;
		}
		rack->info = info;
		rack->task.buffers[0].index = 2;
		rack->task.buffers[0].pointer = info;
		rack->task.buffers[0].offset = sceSasAsAllocatorGetOffset(allocator, (SceUIntPtr)info);
	} else {
		sceSasAsAllocatorAlloc(allocator, sizeof(SceSasAsSynthTaskInfo), 1);
	}
	if (handle != NULL)
		*handle = &rack->handle;
	return 0;
}

static inline SceSasAsSynthVoiceParam *sceSasAsSynthRackParam(const SceSasAsSynthRack *rack,
							      SceUInt32 index)
{
	return (SceSasAsSynthVoiceParam *)((SceUInt8 *)rack->params + index * rack->paramStride);
}

static inline SceInt16 sceSasAsSynthSaturate16(float value)
{
	if (value < -32768.0f)
		return -32768;
	if (value > 32767.0f)
		return 32767;
	return (SceInt16)value;
}

/* a pitch (0..4) as 2.14 */
static inline SceInt32 sceSasAsSynthPitchToFixed(float pitch)
{
	float value;

	if (pitch < 0.0f)
		return 0;
	if (pitch > 4.0f)
		return 0x10000;
	value = pitch * 16384.0f;
	if (value < -2147483648.0f)
		return (SceInt32)0x80000000;
	if (value > 2147483648.0f)
		return 0x7fffffff;
	return (SceInt32)value;
}

/* an envelope level (0..1) as 2.14 */
static inline SceInt16 sceSasAsSynthEnvelopeLevelToFixed(float level)
{
	if (level < 0.0f)
		return 0;
	if (level > 1.0f)
		return 0x4000;
	return sceSasAsSynthSaturate16(level * 16384.0f);
}

static inline void sceSasAsSynthSetEnvelope(SceSasAsSynthVoiceParam *param,
					    const SceSasAsSynthEnvelopePoint *points,
					    SceUInt8 numPoints)
{
	SceInt16 *levels = param->envelopeLevels;
	SceUInt32 *times = param->envelopeTimes;
	SceUInt32 i;

	param->numEnvelopePoints = numPoints;
	for (i = 0; i < numPoints; i++) {
		times[i] = points[i].time;
		levels[i] = sceSasAsSynthEnvelopeLevelToFixed(points[i].level);
	}
}

static int sceSasAsSynthRackUpdate(SceSasAsHandle *handle, const SceSasAsRenderInfo *render)
{
	SceSasAsSynthRack *rack = (SceSasAsSynthRack *)handle;
	SceSasAsSynthVoice *voice;
	SceSasAsSynthVoiceParam *param;
	const SceSasAsSynthSource *source;
	SceSasAsSynthSource *paramSource;
	const SceSasAsSynthFilter *filter;
	const SceSasAsSynthSend *send;
	const float *levels;
	SceUInt8 *types, *busses;
	SceInt16 *coefficients, *fixed;
	SceInt16 c0, c1, c2, c3, c4, level;
	SceInt32 value;
	SceUInt32 word, bits, bit, index, updated, changed, mask, n, i;
	SceUInt16 events, previous;
	SceUInt8 type, mode, buss, volume;

	/* what the last render took is no longer new */
	for (word = 0; word < rack->numVoiceWords; word++) {
		bits = rack->changedVoices[word];
		if (bits == 0)
			continue;
		rack->changedVoices[word] = 0;
		for (index = word * 32; bits != 0; bits >>= 1, index++)
			sceSasAsSynthRackParam(rack, index)->updated = 0;
	}

	for (word = 0; word < rack->numVoiceWords; word++) {
		bits = rack->updatedVoices[word];
		for (bit = 0; bits != 0; bits >>= 1, bit++) {
			if ((bits & 1) == 0)
				continue;
			index = word * 32 + bit;
			voice = &rack->voices[index];
			sceSasAsSpinLockMask(&voice->lock, ~0u);
			sceSasAsAtomicAnd(&rack->updatedVoices[word], ~(1u << bit));
			updated = voice->updated;
			if (updated == 0) {
				sceSasAsSpinTryUnlockMask(&voice->lock, ~0u);
				continue;
			}
			sceSasAsAtomicAnd(&voice->updated, ~SCE_SAS_AS_SYNTH_UPDATED_NOW);
			param = sceSasAsSynthRackParam(rack, index);
			param->updated = updated & SCE_SAS_AS_SYNTH_UPDATED_NOW;

			changed = 0;
			if (updated & SCE_SAS_AS_SYNTH_UPDATED_EVENTS) {
				if (param->keyOnCount != voice->keyOnCount) {
					param->keyOnCount = voice->keyOnCount;
					previous = 0;
				} else {
					previous = param->events;
				}
				events = voice->events;
				voice->events = events & ~SCE_SAS_AS_SYNTH_RELOAD;
				changed = previous ^ events;
				param->events = events & ~SCE_SAS_AS_SYNTH_RELOAD;
				if ((changed & (SCE_SAS_AS_SYNTH_KEY_ON | SCE_SAS_AS_SYNTH_RELOAD)) &&
				    (updated & SCE_SAS_AS_SYNTH_UPDATED_SOURCES)) {
					n = voice->sources.numSources;
					source = voice->sources.sources;
					paramSource = param->sources;
					for (i = 0; i < n; i++)
						paramSource[i] = source[i];
					sceSasAsAtomicAnd(&voice->updated,
							  ~SCE_SAS_AS_SYNTH_UPDATED_SOURCES);
					param->updated |= SCE_SAS_AS_SYNTH_UPDATED_SOURCES;
				}
			}
			if (updated & SCE_SAS_AS_SYNTH_UPDATED_PITCH)
				param->pitch = sceSasAsSynthPitchToFixed(voice->pitch);
			if (updated & SCE_SAS_AS_SYNTH_UPDATED_ADSR) {
				param->adsrEnable = voice->adsr.enable;
				param->adsrModes = voice->adsr.modes;
				param->attackRate = voice->adsr.attackRate;
				param->decayRate = voice->adsr.decayRate;
				param->sustainRate = voice->adsr.sustainRate;
				param->releaseRate = voice->adsr.releaseRate;
				param->sustainLevel = voice->adsr.sustainLevel;
			}
			/* the key-on points at a key on, the key-off points at a key off */
			if (updated & SCE_SAS_AS_SYNTH_UPDATED_ENVELOPE_ON) {
				if (changed & SCE_SAS_AS_SYNTH_KEY_ON) {
					sceSasAsSynthSetEnvelope(param, voice->envelope.points,
								 voice->envelope.numOnPoints);
					sceSasAsAtomicAnd(&voice->updated,
							  ~SCE_SAS_AS_SYNTH_UPDATED_ENVELOPE_ON);
				}
				if (changed & SCE_SAS_AS_SYNTH_KEY_OFF) {
					sceSasAsSynthSetEnvelope(param, voice->envelope.points +
									voice->envelope.numOnPoints,
								 voice->envelope.numOffPoints);
					sceSasAsAtomicAnd(&voice->updated,
							  ~SCE_SAS_AS_SYNTH_UPDATED_ENVELOPE_OFF);
				}
			}
			if (updated & SCE_SAS_AS_SYNTH_UPDATED_DISTORTION) {
				param->distortionEnable = voice->distortion.enable;
				param->distortionGain =
					sceSasAsSynthSaturate16(voice->distortion.gain * 1024.0f);
				param->distortionLevel = sceSasAsLevelToFixed(voice->distortion.level);
			}
			if (updated & SCE_SAS_AS_SYNTH_UPDATED_FILTERS) {
				mask = voice->filters.mask;
				filter = voice->filters.filters;
				coefficients = param->filterCoefficients;
				param->filterMask = mask;
				types = param->filterTypes;
				for (; mask != 0; mask >>= 1, filter++, types += 2, coefficients += 5) {
					if ((mask & 1) == 0)
						continue;
					type = filter->type;
					mode = filter->mode;
					if (type == SCE_SAS_AS_SYNTH_FILTER_COEFFICIENTS) {
						c0 = sceSasAsLevelToFixed(filter->coefficients[0]);
						c1 = sceSasAsLevelToFixed(filter->coefficients[1]);
						c2 = sceSasAsLevelToFixed(filter->coefficients[2]);
						c3 = sceSasAsLevelToFixed(filter->coefficients[3]);
						c4 = sceSasAsLevelToFixed(filter->coefficients[4]);
					} else {
						value = (SceInt32)filter->coefficients[0];
						if (value > 32767)
							value = 32767;
						if (value < -32768)
							value = -32768;
						c0 = value;
						c1 = sceSasAsSynthSaturate16(filter->coefficients[1] *
									     16384.0f);
						c2 = sceSasAsLevelToFixed(filter->coefficients[2]);
						c3 = 0;
						c4 = 0;
					}
					types[0] = type;
					types[1] = mode;
					coefficients[0] = c0;
					coefficients[1] = c1;
					coefficients[2] = c2;
					coefficients[3] = c3;
					coefficients[4] = c4;
				}
			}
			if (updated & SCE_SAS_AS_SYNTH_UPDATED_VOLUMES) {
				n = voice->volumes.numChannels * voice->volumes.numVolumes;
				fixed = param->volumes;
				levels = voice->volumes.levels;
				for (i = 0; i < n; i++)
					fixed[i] = sceSasAsLevelToFixed(levels[i]);
			}
			if (updated & SCE_SAS_AS_SYNTH_UPDATED_SENDS) {
				mask = voice->sends.mask;
				send = voice->sends.sends;
				busses = param->sendBusses;
				param->sendMask = mask;
				fixed = param->sendLevels;
				for (; mask != 0; mask >>= 1, send++, busses += 2, fixed++) {
					if ((mask & 1) == 0)
						continue;
					level = sceSasAsLevelToFixed(send->level);
					buss = send->buss;
					volume = send->volume;
					busses[0] = buss;
					busses[1] = volume;
					*fixed = level;
				}
			}
			rack->changedVoices[word] |= 1u << bit;
			sceSasAsSpinTryUnlockMask(&voice->lock, ~0u);
		}
	}
	return 0;
}

/* a block's lock bit and the bits its unlock sets in the voice's updated */
static int sceSasAsSynthRackUnlock(SceSasAsHandle *handle, SceUInt32 block)
{
	SceSasAsSynthRack *rack = (SceSasAsSynthRack *)handle;
	SceSasAsSynthVoice *voice;
	SceUInt32 index = block & 0xffff;
	SceUInt32 bit, updated;

	if (index >= rack->numVoices)
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	voice = &rack->voices[index];
	switch (block >> 16) {
	case SCE_SAS_AS_SYNTH_BLOCK_SOURCES:
		bit = updated = SCE_SAS_AS_SYNTH_UPDATED_SOURCES;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_PITCH:
		bit = updated = SCE_SAS_AS_SYNTH_UPDATED_PITCH;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_ENVELOPE:
		bit = SCE_SAS_AS_SYNTH_UPDATED_ENVELOPE_ON;
		updated = SCE_SAS_AS_SYNTH_UPDATED_ENVELOPE_ON | SCE_SAS_AS_SYNTH_UPDATED_ENVELOPE_OFF;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_ADSR:
		bit = updated = SCE_SAS_AS_SYNTH_UPDATED_ADSR;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_DISTORTION:
		bit = updated = SCE_SAS_AS_SYNTH_UPDATED_DISTORTION;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_FILTERS:
		bit = updated = SCE_SAS_AS_SYNTH_UPDATED_FILTERS;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_VOLUMES:
		bit = updated = SCE_SAS_AS_SYNTH_UPDATED_VOLUMES;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_SENDS:
		bit = updated = SCE_SAS_AS_SYNTH_UPDATED_SENDS;
		break;
	default:
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	}
	sceSasAsAtomicOr(&voice->updated, updated);
	sceSasAsAtomicOr(&rack->updatedVoices[index >> 5], 1u << (index & 31));
	return sceSasAsSpinTryUnlockMask(&voice->lock, bit);
}

static int sceSasAsSynthRackLock(SceSasAsHandle *handle, SceUInt32 block, void **pointer)
{
	SceSasAsSynthRack *rack = (SceSasAsSynthRack *)handle;
	SceSasAsSynthVoice *voice;
	SceUInt32 index = block & 0xffff;
	SceUInt32 bit;

	if (index >= rack->numVoices)
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	voice = &rack->voices[index];
	switch (block >> 16) {
	case SCE_SAS_AS_SYNTH_BLOCK_SOURCES:
		*pointer = &voice->sources;
		bit = SCE_SAS_AS_SYNTH_UPDATED_SOURCES;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_PITCH:
		*pointer = &voice->pitch;
		bit = SCE_SAS_AS_SYNTH_UPDATED_PITCH;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_ENVELOPE:
		*pointer = &voice->envelope;
		bit = SCE_SAS_AS_SYNTH_UPDATED_ENVELOPE_ON;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_ADSR:
		*pointer = &voice->adsr;
		bit = SCE_SAS_AS_SYNTH_UPDATED_ADSR;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_DISTORTION:
		*pointer = &voice->distortion;
		bit = SCE_SAS_AS_SYNTH_UPDATED_DISTORTION;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_FILTERS:
		*pointer = &voice->filters;
		bit = SCE_SAS_AS_SYNTH_UPDATED_FILTERS;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_VOLUMES:
		*pointer = &voice->volumes;
		bit = SCE_SAS_AS_SYNTH_UPDATED_VOLUMES;
		break;
	case SCE_SAS_AS_SYNTH_BLOCK_SENDS:
		*pointer = &voice->sends;
		bit = SCE_SAS_AS_SYNTH_UPDATED_SENDS;
		break;
	default:
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	}
	sceSasAsSpinLockMask(&voice->lock, bit);
	return 0;
}

static int sceSasAsSynthRackRenderEnd(SceSasAsHandle *handle, const SceSasAsRenderInfo *render)
{
	SceSasAsSynthRack *rack = (SceSasAsSynthRack *)handle;

	sceSasAsSpinLock(&rack->lock);
	rack->statusIndex ^= 1;
	sceSasAsSpinUnlock(&rack->lock);
	return 0;
}

static int sceSasAsSynthRackGetState(SceSasAsHandle *handle, SceUInt32 state, void *value)
{
	SceSasAsSynthRack *rack = (SceSasAsSynthRack *)handle;
	SceSasAsSynthVoiceState *voiceState = value;
	float *envelope = value;
	const SceSasAsSynthVoiceStatus *status;
	SceUInt32 index;
	SceUInt16 voiceStatus;

	if ((state >> 16) < SCE_SAS_AS_SYNTH_STATE_VOICE ||
	    (state >> 16) > SCE_SAS_AS_SYNTH_STATE_ENVELOPE)
		return sceSasAsError(SCE_SAS_AS_ERROR_INVALID_ARGUMENT,
				     "Unknown synth state ID (0x%08X,0x%08X)\n", handle, state);
	index = state & 0xffff;
	if (index >= rack->numVoices)
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	sceSasAsSpinLock(&rack->lock);
	status = &rack->status[rack->statusIndex][index];
	/* a key on the render has not seen yet */
	if (rack->voices[index].keyOnCount != status->keyOnCount) {
		status = &sceSasAsSynthNoStatus;
		voiceStatus = 1;
	} else {
		voiceStatus = status->state;
		if (voiceStatus == 0)
			status = &sceSasAsSynthNoStatus;
	}
	switch (state >> 16) {
	case SCE_SAS_AS_SYNTH_STATE_POSITION:
		voiceState->state = voiceStatus;
		voiceState->events = status->events;
		voiceState->source = status->source;
		voiceState->position = status->position;
		voiceState->time = status->time;
		break;
	case SCE_SAS_AS_SYNTH_STATE_ENVELOPE:
		envelope[0] = status->envelopeLeft * (1.0f / 32768.0f);
		envelope[1] = status->envelopeRight * (1.0f / 32768.0f);
		break;
	default:
		voiceState->state = voiceStatus;
		voiceState->events = status->events;
		break;
	}
	sceSasAsSpinUnlock(&rack->lock);
	return 0;
}

static int sceSasAsSynthRackSetEvent(SceSasAsHandle *handle, SceUInt32 event, SceUInt32 value)
{
	SceSasAsSynthRack *rack = (SceSasAsSynthRack *)handle;
	SceSasAsSynthVoice *voice;
	SceUInt32 index;

	if ((event >> 16) != SCE_SAS_AS_SYNTH_EVENT)
		return sceSasAsError(SCE_SAS_AS_ERROR_INVALID_ARGUMENT,
				     "Unknown synth event ID or value (0x%08X,0x%08X,0x%08X)\n",
				     handle, event, value);
	index = event & 0xffff;
	if (index >= rack->numVoices)
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	voice = &rack->voices[index];
	sceSasAsSpinLock(&voice->lock);
	switch (value) {
	case SCE_SAS_AS_SYNTH_EVENT_KEY_ON:
		voice->events = SCE_SAS_AS_SYNTH_KEY_ON;
		voice->keyOnCount++;
		break;
	case SCE_SAS_AS_SYNTH_EVENT_KEY_OFF:
		voice->events |= SCE_SAS_AS_SYNTH_KEY_OFF;
		break;
	case SCE_SAS_AS_SYNTH_EVENT_STOP:
		voice->events |= SCE_SAS_AS_SYNTH_STOP;
		break;
	case SCE_SAS_AS_SYNTH_EVENT_PAUSE:
		voice->events |= SCE_SAS_AS_SYNTH_PAUSE;
		break;
	case SCE_SAS_AS_SYNTH_EVENT_RESUME:
		voice->events &= ~SCE_SAS_AS_SYNTH_PAUSE;
		break;
	case SCE_SAS_AS_SYNTH_EVENT_RELOAD:
		voice->events |= SCE_SAS_AS_SYNTH_RELOAD;
		break;
	}
	sceSasAsAtomicOr(&voice->updated, SCE_SAS_AS_SYNTH_UPDATED_EVENTS);
	sceSasAsAtomicOr(&rack->updatedVoices[index >> 5], 1u << (index & 31));
	sceSasAsSpinUnlock(&voice->lock);
	return 0;
}

const SceSasAsClass sceSasAsSynthRackClass = {
	.id = SCE_SAS_AS_CLASS_RACK,
	.name = "SYNTH",
	.version = SCE_SAS_AS_CLASS_VERSION,
	.create = sceSasAsSynthRackCreate,
	.destroy = sceSasAsSynthRackDestroy,
	.ops = {
		.lock = sceSasAsSynthRackLock,
		.unlock = sceSasAsSynthRackUnlock,
		.setEvent = sceSasAsSynthRackSetEvent,
		.getState = sceSasAsSynthRackGetState,
		.update = sceSasAsSynthRackUpdate,
		.render = sceSasAsSynthRackRender,
		.renderEnd = sceSasAsSynthRackRenderEnd,
	},
};
