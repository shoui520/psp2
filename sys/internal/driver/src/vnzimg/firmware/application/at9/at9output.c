/*
 * The frame's output (0x882770-0x883b50): the inverse MDCT leaves each
 * channel's samples in its spectrum array in the order of at9OutputOrder;
 * the PCM sample is the top 16 bits. A stereo or dual mono frame is
 * interleaved. The original writes eight samples at a time, in several passes.
 */
#include "at9.h"

void at9Output(int32_t channelConfig, int16_t *pcm, At9Channel *channels, int32_t skip,
	int32_t count)
{
	int32_t i;

	if (channelConfig == 0) {
		for (i = 0; i < count; i++)
			pcm[i] = channels[0].spectrum[at9OutputOrder[skip + i]] >> 16;
		return;
	}
	for (i = 0; i < count; i++) {
		pcm[i * 2] = channels[0].spectrum[at9OutputOrder[skip + i]] >> 16;
		pcm[i * 2 + 1] = channels[1].spectrum[at9OutputOrder[skip + i]] >> 16;
	}
}
