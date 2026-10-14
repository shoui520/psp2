/*
 * The mixer (SCE_NGS_MIXER_ID) and the output (SCE_NGS_OUTPUT_ID): mixed
 * by the VLIW code; the mixer's pause fades are here.
 */
#include "ngs.h"

#define SCE_NGS_MIXER_MAX_PORTS		2

/* SceNgsMixerParams */
typedef struct NgsMixerUserParams {
	SceNgsParamsDescriptor desc;
	float fGainIn[SCE_NGS_MIXER_MAX_PORTS];
	uint32_t uPauseLeadIn;
	uint32_t uPauseLeadOut;
} NgsMixerUserParams;

typedef struct NgsMixerSettings {
	int32_t gainIn[SCE_NGS_MIXER_MAX_PORTS];
	uint32_t pauseLeadIn;
	uint32_t pauseLeadOut;
	int8_t paused;
} NgsMixerSettings;

int32_t ngsMixerInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsMixerSettings *settings = (NgsMixerSettings *)params->data[0];
	NgsMixerUserParams *user = (NgsMixerUserParams *)params->userParams[0];
	int32_t *gain;
	float *given = user->fGainIn;

	for (gain = settings->gainIn; gain < settings->gainIn + SCE_NGS_MIXER_MAX_PORTS; gain++)
		*gain = ngsFixed(*given++);
	if (params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE)
		settings->paused = 0;
	settings->pauseLeadIn = user->uPauseLeadIn;
	settings->pauseLeadOut = user->uPauseLeadOut;
	return 0;
}

static int32_t scale(int32_t sample, int32_t gain)
{
	return (int32_t)(((int64_t)sample * gain) >> 24);
}

/* the samples of a fade: at most half the block */
static int32_t fadeLength(uint32_t lead, NgsBuffer *output)
{
	int32_t half = (int32_t)output->samples / 2;

	return half < (int32_t)lead ? half : (int32_t)lead;
}

/*
 * The fades of a pause (called by the VLIW mixer, 0x87d958): in from silence
 * at the start of the first block after a pause, out to silence at the end
 * of the block that pauses. Four samples at a time.
 */
void ngsMixerFade(NgsMixerSettings *settings, NgsBufferList *outputs, uint32_t flags)
{
	NgsBuffer *output;
	int32_t length, step, channel, i, k;
	int32_t gain, *samples;

	if (settings->paused && settings->pauseLeadIn) {
		output = outputs->buffers[0];
		length = fadeLength(settings->pauseLeadIn, output);
		step = NGS_ONE / length;
		for (channel = 0; channel < outputs->buffers[0]->channels; channel++) {
			samples = (int32_t *)outputs->buffers[0]->channelData[channel];
			if (length < 1)
				continue;
			gain = 0;
			i = 0;
			do {
				for (k = 0; k < 4; k++)
					samples[i + k] = scale(samples[i + k], gain + k * step);
				gain += 4 * step;
				i += 4;
			} while (i < length);
		}
		settings->paused = 0;
	}

	if (!(flags & NGS_RUN_STOP))
		return;
	settings->paused = 1;
	if (!settings->pauseLeadOut)
		return;
	output = outputs->buffers[0];
	length = fadeLength(settings->pauseLeadOut, output);
	step = NGS_ONE / length;
	for (channel = 0; channel < outputs->buffers[0]->channels; channel++) {
		int32_t start = (int32_t)outputs->buffers[0]->samples - length;

		samples = (int32_t *)outputs->buffers[0]->channelData[channel] + start / 2 * 2;
		gain = NGS_ONE;
		for (i = start; i < (int32_t)outputs->buffers[0]->samples; i += 4) {
			for (k = 0; k < 4; k++)
				samples[k] = scale(samples[k], gain - k * step);
			samples += 4;
			gain -= 4 * step;
		}
	}
}

/* count the valid inputs and those that ended */
static void countInput(const NgsBuffer *input, int32_t *valid, int32_t *ended)
{
	if (!(input->flags & NGS_BUFFER_VALID))
		return;
	(*valid)++;
	if (input->flags & NGS_BUFFER_END)
		(*ended)++;
}

/* the input's channels that hold samples, copied to the output */
static void copyInput(const NgsBuffer *input, NgsBuffer *output)
{
	int32_t channel;

	for (channel = 0; channel < input->channels; channel++) {
		if (!((input->channelMask >> channel) & 1))
			continue;
		output->channelMask |= 1 << channel;
		ngsCopy(output->channelData[channel], input->channelData[channel],
			output->channelSize);
	}
}

/*
 * 0x87cc30: one input at unity gain is copied, two at unity added; otherwise
 * the output is cleared and each input added times its gain, eight samples
 * at a time.
 */
int32_t ngsMixerRun(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsMixerSettings *settings = (NgsMixerSettings *)params->data[0];
	NgsBuffer *output = outputs->buffers[0], *first, *second;
	int32_t valid = 0, ended = 0, channel, port, i;

	for (channel = 0; channel < output->channels; channel++)
		cacheInvalidateFlush((uint32_t)output->channelData[channel],
			output->samples * NGS_SAMPLE_SIZE);
	output->channelMask = 0;

	if (params->flags & NGS_RUN_BYPASSED) {
		for (port = 0; port < inputs->count; port++)
			countInput(inputs->buffers[port], &valid, &ended);
		if (valid < 1)
			goto stop;
		output->flags = NGS_BUFFER_VALID;
		if (ended == valid) {
			output->flags |= NGS_BUFFER_END;
			params->flags &= ~NGS_RUN_ENABLED;
		}
	}

	first = inputs->buffers[0];
	second = inputs->buffers[1];
	if ((first->flags & NGS_BUFFER_VALID) &&
	    (!(second->flags & NGS_BUFFER_VALID) || !settings->gainIn[1]) &&
	    settings->gainIn[0] == NGS_ONE) {
		countInput(first, &valid, &ended);
		copyInput(first, output);
	} else if ((second->flags & NGS_BUFFER_VALID) &&
	    (!settings->gainIn[0] || !(first->flags & NGS_BUFFER_VALID)) &&
	    settings->gainIn[1] == NGS_ONE) {
		countInput(second, &valid, &ended);
		copyInput(second, output);
	} else if (settings->gainIn[1] == NGS_ONE && settings->gainIn[0] == NGS_ONE &&
	    (first->flags & NGS_BUFFER_VALID) && (second->flags & NGS_BUFFER_VALID)) {
		if (first->flags & NGS_BUFFER_END)
			ended++;
		if (second->flags & NGS_BUFFER_END)
			ended++;
		for (channel = 0; channel < first->channels; channel++) {
			const int32_t *a = (const int32_t *)first->channelData[channel];
			const int32_t *b = (const int32_t *)second->channelData[channel];
			int32_t *out = (int32_t *)output->channelData[channel];

			if (!((first->channelMask >> channel) & 1))
				continue;
			output->channelMask |= 1 << channel;
			for (i = 0; i < (int32_t)first->samples / 8 * 8; i++)
				out[i] = a[i] + b[i];
		}
		valid = 2;
		goto fade;
	} else {
		for (channel = 0; channel < output->channels; channel++)
			ngsClear(output->channelData[channel], output->channelSize);
		for (port = 0; port < inputs->count; port++) {
			NgsBuffer *input = inputs->buffers[port];
			int32_t gain = settings->gainIn[port];

			if (!(input->flags & NGS_BUFFER_VALID))
				continue;
			if (!gain) {
				valid++;
				continue;
			}
			countInput(input, &valid, &ended);
			for (channel = 0; channel < input->channels; channel++) {
				const int32_t *in = (const int32_t *)input->channelData[channel];
				int32_t *out = (int32_t *)output->channelData[channel];

				if (!((input->channelMask >> channel) & 1))
					continue;
				output->channelMask |= 1 << channel;
				for (i = 0; i < (int32_t)input->samples / 8 * 8; i++)
					out[i] += ngsMultiply(in[i], gain);
			}
		}
	}
	if (valid < 1)
		goto stop;
fade:
	ngsMixerFade(settings, outputs, params->voiceState);
	output->flags = NGS_BUFFER_VALID;
	if (ended != valid)
		return 0;
	output->flags |= NGS_BUFFER_END;
stop:
	params->flags &= ~NGS_RUN_ENABLED;
	return 0;
}

int32_t ngsMixerProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	return ngsMixerRun(inputs, outputs, params);
}

int32_t ngsOutputInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	return 0;
}

int32_t ngsOutputProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	ngsOutputRun(inputs, outputs, params);
	return 0;
}
