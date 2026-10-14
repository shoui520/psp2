/*
 * The reverb (SCE_NGS_REVERB_ID): an I3DL2 reverb run by the engine in
 * reverbengine.c. After the voice is keyed off, or its input ends, the
 * module keeps running on silence until the tail has died away.
 */
#include "ngs.h"

#define SCE_NGS_REVERB_PARAMS_STRUCT_ID		SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_REVERB_ID, 1, 1)
#define SCE_NGS_REVERB_PARAMS_STRUCT_ID_V2	SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_REVERB_ID, 1, 2)

/* mB to 8.24 (/ 2000), percentages to 8.24 (/ 100) */
#define MILLIBELS				(16777216.0f / 2000.0f)
#define PERCENT					(16777216.0f / 100.0f)
#define FIXED					16777216.0f
#define MAX_REFLECTIONS_DELAY			0x266666	/* 0.15 s */
#define MIN_DECAY				0x19999a	/* 0.1 */
#define FADE_SAMPLES				64

/* SceNgsReverbParams */
typedef struct NgsReverbUserParams {
	SceNgsParamsDescriptor desc;
	float fRoom;
	float fRoomHF;
	float fDecayTime;
	float fDecayHFRatio;
	float fReflections;
	float fReflectionsDelay;
	float fReverb;
	float fReverbDelay;
	float fDiffusion;
	float fDensity;
	float fHFReference;
	int32_t eEarlyReflectionPattern[SCE_NGS_MAX_SYSTEM_CHANNELS];
	float fEarlyReflectionScalar;
	float fLFReference;
	float fRoomLF;
	float fDryMB;
} NgsReverbUserParams;

typedef struct NgsReverbSettings {
	uint32_t granularity;
	int32_t tail;			/* blocks run since the voice was keyed off */
	int32_t maxTail;		/* blocks for the reverb to die away */
	int32_t bypassed;
	uint32_t engine[];		/* the engine's state */
} NgsReverbSettings;

/* the last 64 samples of each channel faded out (the delay's too) */
void ngsFadeOut(NgsBufferList *inputs)
{
	NgsBuffer *input;
	int32_t channel, step, i;
	int32_t *data;

	for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
		input = inputs->buffers[0];
		data = (int32_t *)input->channelData[channel] + (input->samples - FADE_SAMPLES);
		step = ngsFadeStart / FADE_SAMPLES;
		for (i = 0; i < FADE_SAMPLES; i++)
			data[i] = ngsMultiply(data[i], ngsWindow[ngsFadeStart + i * step]);
	}
}

#define REVERB_FADE_OUT		0x2
#define REVERB_SILENCE		0x4
#define REVERB_BYPASS		0x8

int32_t ngsReverbProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsReverbSettings *settings = (NgsReverbSettings *)params->data[0];
	uint32_t mode = 0;
	int32_t reset = 0, channel;
	NgsBuffer *input, *output;

	if (params->flags & NGS_RUN_BYPASSED) {
		mode = REVERB_BYPASS;
		if (settings->bypassed == 0) {
			settings->bypassed = 1;
			reset = 1;
		}
	} else if (settings->bypassed) {
		settings->bypassed = 0;
	}
	if (inputs->buffers[0]->flags != NGS_BUFFER_VALID || settings->tail >= 1)
		mode |= REVERB_SILENCE;
	else if (params->voiceState & NGS_VOICE_STATE_KEYED_OFF)
		mode |= REVERB_FADE_OUT;

	if (mode & REVERB_SILENCE) {
		for (channel = 0; channel < inputs->buffers[0]->channels; channel++)
			ngsClear(inputs->buffers[0]->channelData[channel],
				inputs->buffers[0]->channelSize);
	} else if (mode & REVERB_FADE_OUT) {
		ngsFadeOut(inputs);
	}

	input = inputs->buffers[0];
	output = outputs->buffers[0];
	if (mode & REVERB_BYPASS)
		ngsReverbBypass(input->channelData, output->channelData, input->channels,
			input->samples, settings->engine, reset);
	else
		ngsReverbRun(input->channelData, output->channelData, input->channels,
			input->samples, settings->engine, params->scratch);

	outputs->buffers[0]->flags = NGS_BUFFER_VALID;
	for (channel = 0; channel < outputs->buffers[0]->channels; channel++)
		outputs->buffers[0]->channelMask |= 1 << channel;

	if (!(params->voiceState & NGS_VOICE_STATE_KEYED_OFF) && settings->tail < 1)
		return 0;
	if (settings->tail < settings->maxTail) {
		settings->tail++;
		params->flags |= NGS_RUN_KEEP_PENDING;
	} else {
		outputs->buffers[0]->flags |= NGS_BUFFER_END;
	}
	return 0;
}

/* RPC 327 gives the ARM this entry */
int32_t ngsReverbAuxiliary(void *state, void *buffers, void *scratch)
{
	ngsReverbEngine(state, buffers, scratch);
	return 0;
}

int32_t ngsReverbInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsReverbUserParams *user = (NgsReverbUserParams *)params->userParams[0];
	NgsReverbSettings *settings = (NgsReverbSettings *)params->data[0];
	int32_t channels = inputs->buffers[0]->channels;
	int32_t reflectionsDelay, blockTime;
	NgsReverbConfig config;

	settings->granularity = inputs->buffers[0]->samples;
	reflectionsDelay = (int32_t)(user->fReflectionsDelay * FIXED);
	reflectionsDelay -= (int32_t)(settings->granularity << 16) / (int32_t)params->sampleRate << 8;
	if (reflectionsDelay < 0)
		reflectionsDelay = 0;
	else
		reflectionsDelay = min(reflectionsDelay, MAX_REFLECTIONS_DELAY);

	config.desc = user->desc;
	config.room = (int32_t)(user->fRoom * MILLIBELS);
	config.roomHF = (int32_t)(user->fRoomHF * MILLIBELS);
	config.decayTime = (int32_t)(user->fDecayTime * FIXED);
	config.decayHFRatio = (int32_t)(user->fDecayHFRatio * FIXED);
	config.reflections = (int32_t)(user->fReflections * MILLIBELS);
	config.reflectionsDelay = reflectionsDelay;
	config.reverb = (int32_t)(user->fReverb * MILLIBELS);
	config.reverbDelay = (int32_t)(user->fReverbDelay * FIXED);
	config.diffusion = (int32_t)(user->fDiffusion * FIXED);
	config.density = (int32_t)(user->fDensity * PERCENT);
	config.hfReference = (int32_t)user->fHFReference;
	config.earlyReflectionPattern[0] = user->eEarlyReflectionPattern[0];
	config.earlyReflectionPattern[1] = user->eEarlyReflectionPattern[1];
	config.earlyReflectionScalar = (int32_t)(user->fEarlyReflectionScalar * PERCENT);
	config.lfReference = (int32_t)user->fLFReference;
	config.roomLF = (int32_t)(user->fRoomLF * MILLIBELS);
	config.dry = (int32_t)(user->fDryMB * MILLIBELS);
	if (config.decayTime == 0)
		config.decayTime = MIN_DECAY;
	if (config.decayHFRatio == 0)
		config.decayHFRatio = MIN_DECAY;

	/* the blocks the reverb takes to die away, and two more */
	blockTime = ngsMultiply(settings->granularity << 16, 0x1000000 / (int32_t)params->sampleRate);
	settings->maxTail = ((config.reflectionsDelay + config.decayTime + config.reverbDelay) >> 8) /
		blockTime + 2;

	if (user->desc.id != SCE_NGS_REVERB_PARAMS_STRUCT_ID &&
		user->desc.id != SCE_NGS_REVERB_PARAMS_STRUCT_ID_V2)
		return SCE_NGS_ERROR_INVALID_PARAM;
	if (params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE) {
		settings->bypassed = 0;
		settings->tail = 0;
		ngsReverbReset(settings->engine, &config,
			inputs->buffers[0]->channels == 1 ? 1 : 2, 0, 1);
	}
	return ngsReverbConfigure(settings->engine, &config, channels, 0);
}
