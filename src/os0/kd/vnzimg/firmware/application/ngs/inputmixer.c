/*
 * The input mixer (SCE_NGS_INPUT_MIXER_ID): other voices write into its
 * ports' buffers; each run sums the ports into the output and clears them.
 */
#include "ngs.h"

#define MIX_BLOCK		8	/* samples the IVC2 adds at once */

/* zero a buffer the other cores write: purge it, clear it, write it back */
void ngsInputMixerClear(int32_t *buffer, uint32_t size)
{
	cachePurgeRange((uint32_t)buffer, size);
	ngsClear(buffer, size);
	cacheFlushRange((uint32_t)buffer, size);
}

/* 0x87c378 */
int32_t ngsInputMixerBypass(NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsInputMixerSettings *settings = (NgsInputMixerSettings *)params->data[0];
	uint32_t channel, port;

	for (channel = 0; channel < settings->channels; channel++)
		for (port = 0; port < settings->ports; port++) {
			ngsInputMixerClear(settings->buffers[port][channel],
				outputs->buffers[0]->channelSize);
			settings->reset[port] = 0;
		}
	return 0;
}

/* 0x87c528: port 0 is copied, the others added */
int32_t ngsInputMixerRun(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsInputMixerSettings *settings = (NgsInputMixerSettings *)params->data[0];
	NgsBuffer *output = outputs->buffers[0];
	uint32_t channel, port, i;

	output->channelMask = 0;
	for (port = 0; port < settings->ports; port++) {
		if (settings->reset[port] != 1)
			continue;
		for (channel = 0; channel < settings->channels; channel++)
			ngsInputMixerClear(settings->buffers[port][channel], output->channelSize);
		settings->reset[port] = 0;
	}

	for (channel = 0; channel < settings->channels; channel++) {
		int32_t *buffer = settings->buffers[0][channel];
		uint32_t size = output->samples * NGS_SAMPLE_SIZE;

		cachePurgeRange((uint32_t)buffer, size);
		output->channelMask |= 1 << channel;
		ngsCopy(output->channelData[channel], buffer, size);
		ngsClear(buffer, size);
		cacheFlushRange((uint32_t)buffer, size);
	}

	for (port = 1; port < settings->ports; port++)
		for (channel = 0; channel < settings->channels; channel++) {
			int32_t *buffer = settings->buffers[port][channel];
			int32_t *data = (int32_t *)output->channelData[channel];

			output->channelMask |= 1 << channel;
			cachePurgeRange((uint32_t)buffer, output->samples * NGS_SAMPLE_SIZE);
			for (i = 0; i < output->samples / MIX_BLOCK * MIX_BLOCK; i++) {
				data[i] += buffer[i];
				buffer[i] = 0;
			}
			cacheFlushRange((uint32_t)buffer, output->samples * NGS_SAMPLE_SIZE);
		}

	output->flags = NGS_BUFFER_VALID;
	return 0;
}

int32_t ngsInputMixerProcess(NgsBufferList *inputs, NgsBufferList *outputs,
	NgsModuleParams *params)
{
	if (params->flags & NGS_RUN_BYPASSED)
		return ngsInputMixerBypass(outputs, params);
	return ngsInputMixerRun(inputs, outputs, params);
}

int32_t ngsInputMixerInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	return 0;
}
