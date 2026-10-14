/*
 * The output module's kernel (0x87da30): the input buffer's samples to
 * 16-bit PCM in the voice's output, saturated after the 9-bit shift back.
 * Two channels that both hold samples are interleaved, eight frames at a
 * time; otherwise every channel left in the mask is written, 16 samples at
 * a time, to the start of the same output.
 */
#include "ngs.h"

#define OUTPUT_SHIFT	9
#define STEREO_FRAMES	8
#define MONO_SAMPLES	16

static int16_t toPcm(int32_t sample)
{
	sample >>= OUTPUT_SHIFT;
	if (sample > 32767)
		return 32767;
	if (sample < -32768)
		return -32768;
	return sample;
}

void ngsOutputRun(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsBuffer *input = inputs->buffers[0];
	int16_t *pcm = (int16_t *)params->data[1];
	uint32_t i, block;
	int32_t channel;

	if (input->channelMask == 3 && input->channels == 2) {
		const int32_t *left = (const int32_t *)input->channelData[0];
		const int32_t *right = (const int32_t *)input->channelData[1];

		for (block = 0; block < input->samples / STEREO_FRAMES; block++)
			for (i = 0; i < STEREO_FRAMES; i++) {
				*pcm++ = toPcm(*left++);
				*pcm++ = toPcm(*right++);
			}
	} else {
		for (channel = 0; channel < input->channels; channel++) {
			const int32_t *data = (const int32_t *)input->channelData[channel];
			int16_t *out = pcm;

			if (!(input->channelMask >> channel))
				continue;
			for (block = 0; block < input->samples / MONO_SAMPLES; block++)
				for (i = 0; i < MONO_SAMPLES; i++)
					*out++ = toPcm(*data++);
		}
	}
	if (outputs->count)
		outputs->buffers[0]->channelMask = input->channelMask;
}
