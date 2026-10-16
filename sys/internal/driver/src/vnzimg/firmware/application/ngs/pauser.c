/*
 * The pauser (SCE_NGS_PAUSER_ID): the input is copied through; when the voice
 * resumes, its first lead-in samples fade in, and when it is pausing, its last
 * lead-out samples fade out. A fade lasts at most half the buffer.
 */
#include "ngs.h"

#define COPY_BLOCK	32	/* bytes the IVC2 copies at once */
#define FADE_LANES	4

/* SceNgsPauserParams, kept as given, then whether it is paused */
typedef struct NgsPauserSettings {
	SceNgsParamsDescriptor desc;
	uint32_t uPauseLeadIn;
	uint32_t uPauseLeadOut;
	int8_t paused;
} NgsPauserSettings;

int32_t ngsPauserInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsPauserSettings *settings = (NgsPauserSettings *)params->data[0];
	uint32_t *user = (uint32_t *)params->userParams[0], *copy = (uint32_t *)settings;
	int32_t i;

	if (params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE)
		settings->paused = 0;
	for (i = 0; i < 4; i++)
		copy[i] = user[i];
	return 0;
}

/* the fade's length and its gain step per sample, 8.24 */
static int32_t fadeStep(uint32_t lead, NgsBuffer *buffer, int32_t *length)
{
	int32_t half = (int32_t)buffer->samples / 2;

	*length = half < (int32_t)lead ? half : (int32_t)lead;
	return NGS_ONE / *length;
}

/* `length` samples (whole lanes) scaled by gain, gain + step, ... */
static void fade(int32_t *data, int32_t length, int32_t gain, int32_t step)
{
	int32_t i;

	for (i = 0; i < length; i += FADE_LANES, gain += FADE_LANES * step) {
		data[i] = ngsMultiply(data[i], gain);
		data[i + 1] = ngsMultiply(data[i + 1], gain + step);
		data[i + 2] = ngsMultiply(data[i + 2], gain + 2 * step);
		data[i + 3] = ngsMultiply(data[i + 3], gain + 3 * step);
	}
}

/* 0x87dd68 */
int32_t ngsPauserRun(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsPauserSettings *settings = (NgsPauserSettings *)params->data[0];
	NgsBuffer *input = inputs->buffers[0], *output;
	int32_t channel, length, step, start;
	uint32_t i;

	if (!(input->flags & NGS_BUFFER_VALID)) {
		params->flags &= ~NGS_RUN_ENABLED;
		goto out;
	}
	if (params->flags & NGS_RUN_BYPASSED) {
		if (input->flags & NGS_BUFFER_END) {
			outputs->buffers[0]->flags |= NGS_BUFFER_END;
			params->flags = 0;
		}
		return 0;
	}

	output = outputs->buffers[0];
	for (channel = 0; channel < input->channels; channel++) {
		uint32_t *to = (uint32_t *)output->channelData[channel];
		const uint32_t *from = (const uint32_t *)input->channelData[channel];

		for (i = 0; i < (input->samples * NGS_SAMPLE_SIZE / COPY_BLOCK) * COPY_BLOCK / 4; i++)
			to[i] = from[i];
	}

	if (settings->paused && settings->uPauseLeadIn) {
		step = fadeStep(settings->uPauseLeadIn, input, &length);
		for (channel = 0; channel < input->channels; channel++)
			if ((input->channelMask >> channel) & 1)
				fade((int32_t *)output->channelData[channel], length, 0, step);
		settings->paused = 0;
	}
	if ((params->voiceState & SCE_NGS_VOICE_STATE_UNLOADING) && settings->uPauseLeadOut) {
		step = fadeStep(settings->uPauseLeadOut, input, &length);
		start = ((int32_t)input->samples - length) / 2 * 2;
		for (channel = 0; channel < input->channels; channel++)
			if ((input->channelMask >> channel) & 1)
				fade((int32_t *)output->channelData[channel] + start, length,
					NGS_ONE, -step);
		settings->paused = 1;
	}

	output->flags = NGS_BUFFER_VALID;
	if (input->flags & NGS_BUFFER_END) {
		output->flags |= NGS_BUFFER_END;
		params->flags = 0;
	}
out:
	if (outputs->count)
		outputs->buffers[0]->channelMask = input->channelMask;
	return 0;
}

int32_t ngsPauserProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	return ngsPauserRun(inputs, outputs, params);
}
