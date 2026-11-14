/*
 * The frame's output (0x882770-0x883b50): the inverse MDCT leaves each
 * channel's samples in its spectrum array in the order of at9OutputOrder;
 * the PCM sample is the top 16 bits. A stereo or dual mono frame is
 * interleaved. The original writes eight samples at a time, in several passes.
 */
#include "at9.h"

/* 0x8827c8: one channel */
static __attribute__((noinline, noclone)) void outputMono(int16_t *pcm, At9Channel *channels,
	int32_t skip, int32_t count)
{
	int32_t i;

	for (i = 0; i < count; i++)
		pcm[i] = channels[0].spectrum[at9OutputOrder[skip + i]] >> 16;
}

/* 0x882f58: two, interleaved */
static __attribute__((noinline, noclone)) void outputStereo(int16_t *pcm, At9Channel *channels,
	int32_t skip, int32_t count)
{
	int32_t i;

	for (i = 0; i < count; i++) {
		pcm[i * 2] = channels[0].spectrum[at9OutputOrder[skip + i]] >> 16;
		pcm[i * 2 + 1] = channels[1].spectrum[at9OutputOrder[skip + i]] >> 16;
	}
}

/*
 * 0x882770: the kernel for the channel configuration. The original also sets
 * CCR11 (0x76fe32ba), the permutation its kernels' passes use.
 */
void at9Output(int32_t channelConfig, int16_t *pcm, At9Channel *channels, int32_t skip,
	int32_t count)
{
	if (channelConfig == 0)
		outputMono(pcm, channels, skip, count);
	else
		outputStereo(pcm, channels, skip, count);
}

/* 0x883b50: size bytes of value, the at9 code's own fill */
void at9Memset(void *destination, int32_t value, int32_t size)
{
	uint8_t *p = destination;
	int32_t i;

	for (i = 0; i < size; i++)
		p[i] = value;
}
