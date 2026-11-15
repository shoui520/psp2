/*
 * The packet loss concealment's DSP loops (include/robin/sbc.h), ARM code in
 * the original: correlations, energies, scaled copies and sums of
 * magnitudes of 16-bit samples. The sums wrap as the original's
 * multiply-accumulates do.
 */
#include <stdint.h>
#include "robin/sbc.h"

/* The correlations of the pattern with the signal at the lags 0..3, in blocks of 16 samples. */
void btSbcBuildPacket(const int16_t *signal, const int16_t *pattern, int32_t correlations[4], int count)
{
	uint32_t sums[4] = { 0, 0, 0, 0 };
	int i, lag;

	do {
		for (i = 0; i < 16; i++) {
			for (lag = 0; lag < 4; lag++)
				sums[lag] += (uint32_t)(signal[i + lag] * pattern[i]);
		}
		signal += 16;
		pattern += 16;
		count -= 16;
	} while (count > 0);
	for (lag = 0; lag < 4; lag++)
		correlations[lag] = sums[lag];
}

/* Copies samples shifted right by shift, or left by -shift. */
void btSbcComputePacket(const int16_t *input, int16_t *output, int shift, int count)
{
	int i;

	if (shift < 0) {
		for (i = 0; i < count; i++)
			output[i] = input[i] << -shift;
	} else {
		for (i = 0; i < count; i++)
			output[i] = input[i] >> shift;
	}
}

void btSbcQueryPacket(const int16_t *input, int16_t *output, int count)
{
	int i;

	for (i = 0; i < count; i++)
		output[i] = input[i];
}

/* The energy of 64 samples, each a quarter. */
uint32_t btSbcResetPacket(const int16_t *signal)
{
	uint32_t sum = 0;
	int i, sample;

	for (i = 0; i < SBC_PLC_TEMPLATE; i++) {
		sample = signal[i] >> 2;
		sum += (uint32_t)(sample * sample);
	}
	return sum;
}

/* The energy of 64 samples. */
uint32_t btSbcSyncPacket(const int16_t *signal)
{
	uint32_t sum = 0;
	int i;

	for (i = 0; i < SBC_PLC_TEMPLATE; i++)
		sum += (uint32_t)(signal[i] * signal[i]);
	return sum;
}

/* The sums of the magnitudes of count samples, two at a time, going back from first and second. */
void btSbcApplyPacket(const int16_t *first, const int16_t *second, int count, int32_t *firstSum,
		      int32_t *secondSum)
{
	uint32_t sumFirst = 0, sumSecond = 0;
	int pairs, a, b;

	for (pairs = count >> 1; pairs > 0; pairs--) {
		a = first[0];
		b = first[-1];
		sumFirst += a > 0 ? a : -a;
		sumFirst += b > 0 ? b : -b;
		a = second[0];
		b = second[-1];
		sumSecond += a > 0 ? a : -a;
		sumSecond += b > 0 ? b : -b;
		first -= 2;
		second -= 2;
	}
	*firstSum = sumFirst;
	*secondSum = sumSecond;
}
