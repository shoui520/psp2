/*
 * The compressor (SCE_NGS_COMPRESSOR_ID): each block of 64 samples is
 * measured (peak or RMS) and compressed by the VLIW kernel; with V1
 * parameters a second input is the side chain, with V2 only while that
 * input holds samples.
 */
#include "ngs.h"

#define SCE_NGS_COMPRESSOR_PARAMS_STRUCT_ID	SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_COMPRESSOR_ID, 1, 1)
#define SCE_NGS_COMPRESSOR_PEAK_MODE		1
#define SCE_NGS_COMPRESSOR_STEREO_LINK_ON	1

/* SceNgsCompressorParams */
typedef struct NgsCompressorUserParams {
	SceNgsParamsDescriptor desc;
	float fRatio;
	float fThreshold;
	float fAttack;
	float fRelease;
	float fMakeupGain;
	int32_t nStereoLink;
	int32_t nPeakMode;
	float fSoftKnee;
} NgsCompressorUserParams;

/* the same in 8.24, the makeup gain as a level, and the running state */
typedef struct NgsCompressorSettings {
	SceNgsParamsDescriptor desc;
	int32_t ratio;
	int32_t threshold;
	int32_t attack;
	int32_t release;
	int32_t makeupGain;
	int32_t stereoLink;
	int32_t peakMode;
	int32_t softKnee;
	int32_t envelope[SCE_NGS_MAX_SYSTEM_CHANNELS];
	int32_t gain[SCE_NGS_MAX_SYSTEM_CHANNELS];
} NgsCompressorSettings;

/* SceNgsCompressorStates */
typedef struct NgsCompressorStates {
	float fInputLevel[SCE_NGS_MAX_SYSTEM_CHANNELS];
	float fOutputLevel[SCE_NGS_MAX_SYSTEM_CHANNELS];
} NgsCompressorStates;

#define ENVELOPE_FLOOR	168	/* the smallest gain the envelope keeps, 8.24 */

/* the gain wanted for a level: 1, or the curve's output over the level */
static int32_t compressorTarget(NgsCompressorSettings *settings, int32_t level)
{
	int32_t decibels = ngsLevelToDecibels(level), knee = settings->softKnee;
	int32_t lower = settings->threshold - knee / 2, over, curve, out;

	if (lower >= decibels)
		return NGS_ONE;
	over = decibels - lower;
	if (knee >= 1 && over < knee) {
		curve = ngsMultiply(ngsMultiply(ngsDivide(over, knee), NGS_ONE / 2),
			settings->ratio - NGS_ONE);
		out = ngsDecibelsToLevel(ngsMultiply(over, curve + NGS_ONE) + lower);
	} else {
		out = ngsDecibelsToLevel(ngsMultiply(decibels - settings->threshold,
			settings->ratio) + settings->threshold);
	}
	if (level < 1)
		return NGS_ONE;
	return ngsDivide(out, level);
}

/*
 * 0x878e98: the envelope moves toward the block's gain at the attack or
 * release rate, and the block is scaled by a gain ramped from the last
 * block's (at once without an attack time).
 */
void ngsCompressorApply(const int32_t *input, int32_t *output, uint32_t count, int32_t level,
	void *data, NgsModuleParams *params, int32_t channel)
{
	NgsCompressorSettings *settings = (NgsCompressorSettings *)data;
	int32_t perSample = NGS_ONE / (int32_t)params->sampleRate;
	int32_t previous = settings->gain[channel], target, difference, change, step, gain;
	int32_t blockTime = ngsMultiply((int32_t)count << 16, perSample) << 8;
	int32_t *envelope = &settings->envelope[channel];
	uint32_t i;

	target = compressorTarget(settings, level);
	difference = target - *envelope;
	if (*envelope < ENVELOPE_FLOOR)
		*envelope = ENVELOPE_FLOOR;
	change = difference;
	if (max(target, ENVELOPE_FLOOR) < *envelope) {
		if (settings->attack >= 1)
			change = max(ngsMultiply(ngsDivide(blockTime, settings->attack),
				difference) << 1, difference);
	} else if (settings->release >= 1) {
		change = min(ngsMultiply(ngsDivide(blockTime, settings->release),
			difference) << 1, difference);
	}
	*envelope += change;

	gain = ngsMultiply(settings->makeupGain, *envelope);
	settings->gain[channel] = gain;
	if (settings->attack) {
		step = (gain - previous) / (int32_t)count;
	} else {
		step = 0;
		previous = gain;
	}
	for (i = 0; i < count; i++)
		output[i] = ngsMultiply(input[i], previous + (int32_t)i * step);
}

int32_t ngsCompressorProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsCompressorStates *states = (NgsCompressorStates *)params->data[1];
	NgsCompressorSettings *settings = (NgsCompressorSettings *)params->data[0];
	NgsBuffer *input = inputs->buffers[0], *output, *sideChain;
	int32_t blocks = input->samples / NGS_BLOCK;
	int32_t inputLevel[SCE_NGS_MAX_SYSTEM_CHANNELS] = { 0, 0 };
	int32_t outputLevel[SCE_NGS_MAX_SYSTEM_CHANNELS];
	int32_t level[SCE_NGS_MAX_SYSTEM_CHANNELS];
	int32_t block, channel, loudest, gain;
	uint32_t offset, useSideChain;
	int32_t *data;

	if (settings->desc.id == SCE_NGS_COMPRESSOR_PARAMS_STRUCT_ID)
		useSideChain = inputs->count >= 2;
	else
		useSideChain = inputs->count >= 2 &&
			(inputs->buffers[1]->flags & NGS_BUFFER_VALID);
	if (useSideChain) {
		outputLevel[0] = settings->makeupGain;
		outputLevel[1] = settings->makeupGain;
	} else {
		outputLevel[1] = 0;
		outputLevel[0] = 0;
	}

	if (!(inputs->buffers[0]->flags & NGS_BUFFER_VALID)) {
		if (useSideChain && (inputs->buffers[1]->flags & NGS_BUFFER_VALID))
			return 0;
		params->flags &= ~NGS_RUN_ENABLED;
		return 0;
	}
	if (params->flags & NGS_RUN_BYPASSED) {
		if (inputs->buffers[0]->flags & NGS_BUFFER_END)
			goto end;
		return 0;
	}

	for (block = 0; block < blocks; block++) {
		offset = block * NGS_BLOCK * NGS_SAMPLE_SIZE;
		loudest = 0;
		if (!useSideChain) {
			for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
				data = (int32_t *)(inputs->buffers[0]->channelData[channel] + offset);
				if (settings->peakMode == SCE_NGS_COMPRESSOR_PEAK_MODE)
					level[channel] = ngsPeakLevel(data, NGS_BLOCK);
				else
					level[channel] = ngsRmsLevel(data, NGS_BLOCK);
				loudest = max(loudest, level[channel]);
			}
		} else {
			sideChain = inputs->buffers[1];
			for (channel = 0; channel < inputs->buffers[1]->channels; channel++) {
				sideChain = inputs->buffers[1];
				data = (int32_t *)(sideChain->channelData[channel] + offset);
				if ((sideChain->flags & NGS_BUFFER_VALID) &&
					(sideChain->channelMask & (1 << channel))) {
					if (settings->peakMode == SCE_NGS_COMPRESSOR_PEAK_MODE)
						level[channel] = ngsPeakLevel(data, NGS_BLOCK);
					else
						level[channel] = ngsRmsLevel(data, NGS_BLOCK);
				} else {
					level[channel] = 0;
				}
				loudest = max(loudest, level[channel]);
			}
		}
		for (channel = 0; channel < inputs->buffers[0]->channels; channel++)
			inputLevel[channel] = max(inputLevel[channel], level[channel]);
		if (settings->stereoLink == SCE_NGS_COMPRESSOR_STEREO_LINK_ON) {
			level[0] = loudest;
			level[1] = loudest;
		}

		for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
			ngsCompressorApply(
				(int32_t *)(inputs->buffers[0]->channelData[channel] + offset),
				(int32_t *)(outputs->buffers[0]->channelData[channel] + offset),
				NGS_BLOCK, level[channel], settings, params, channel);
			if (useSideChain) {
				if (settings->gain[channel] < outputLevel[channel])
					outputLevel[channel] = settings->gain[channel];
			} else {
				gain = ngsMultiply(level[channel], settings->gain[channel]);
				outputLevel[channel] = max(outputLevel[channel], gain);
			}
		}
	}

	for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
		states->fInputLevel[channel] = (float)inputLevel[channel] * NGS_FLOAT_OF_FIXED;
		states->fOutputLevel[channel] = (float)outputLevel[channel] * NGS_FLOAT_OF_FIXED;
	}
	output = outputs->buffers[0];
	output->flags = NGS_BUFFER_VALID;
	output->channelMask = inputs->buffers[0]->channelMask;
	if (!(inputs->buffers[0]->flags & NGS_BUFFER_END))
		return 0;
end:
	outputs->buffers[0]->flags |= NGS_BUFFER_END;
	params->flags = 0;
	return 0;
}

int32_t ngsCompressorInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsCompressorUserParams *user = (NgsCompressorUserParams *)params->userParams[0];
	NgsCompressorSettings *settings = (NgsCompressorSettings *)params->data[0];
	int32_t channel;

	settings->desc.id = user->desc.id;
	settings->desc.size = user->desc.size;
	settings->attack = ngsFixed(user->fAttack);
	settings->makeupGain = ngsFixed(user->fMakeupGain);
	settings->ratio = ngsFixed(user->fRatio);
	settings->release = ngsFixed(user->fRelease);
	settings->softKnee = ngsFixed(user->fSoftKnee);
	settings->threshold = ngsFixed(user->fThreshold);
	settings->peakMode = user->nPeakMode;
	settings->stereoLink = user->nStereoLink;
	settings->makeupGain = ngsDecibelsToLevel(settings->makeupGain);

	if (params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE) {
		for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
			settings->envelope[channel] = NGS_ONE;
			settings->gain[channel] = NGS_ONE;
		}
	}
	return 0;
}
