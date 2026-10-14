/*
 * The CELP encoder's perceptual weighting and open-loop pitch search
 * (0x8c8758-0x8c9ad0): the speech is filtered with A(z/g1) / A(z/g2) of
 * its subframe's LPC; the weighted speech of each block of 80 or 120 samples
 * is correlated with its past, from 17 to 144 samples back, for the lag,
 * and the frame's gains choose the gain mode.
 */
#include "celp.h"

#define PITCH_HISTORY			144
#define PITCH_LAG_MIN			17
#define PITCH_CORRELATION		80

/* 0x8c8758 */
void celpEncSearchPitch(CelpEncoderState *state)
{
	int32_t history[PITCH_HISTORY + CELP_MAX_FRAME + 80];
	int32_t order = state->order, length = state->subframeLength;
	int32_t block, blockCount, i, j, k, subframe, shift, bestLag = PITCH_HISTORY;
	int64_t energySum = 0, gainSum = 0, mean, product;

	if (length == 60)
		length = 120;
	block = max(length, 80);
	blockCount = state->frameLength / block;
	for (i = 0; i < PITCH_HISTORY; i++)
		history[i] = state->weightedHistory[i];
	shift = state->subframes == 4 ? 2 : 1;
	for (subframe = 0; subframe < state->subframes; subframe++) {
		const int32_t *lpc = &state->analysisLpc[((subframe * state->analysisBlocks) >> shift) * order];

		for (k = 0; k < order; k++) {
			int32_t coefficient = -lpc[k];

			state->perceptualNumerator[subframe * order + k] = (int32_t)((int64_t)coefficient * celpEncGamma[k] >> 23);
			state->perceptualDenominator[subframe * order + k] = coefficient >> (8 + k);
		}
	}
	for (subframe = 0; subframe < state->subframes; subframe++) {
		for (i = 0; i < state->subframeLength; i++) {
			int32_t sample = state->speech[80 + subframe * state->subframeLength + i];
			int64_t sum = (int64_t)sample << 16;
			int32_t weighted;

			for (j = 0; j < order; j++)
				sum += (int64_t)state->perceptualNumerator[subframe * order + j] * state->perceptualInput[j];
			for (j = 0; j < order; j++)
				sum -= (int64_t)state->perceptualDenominator[subframe * order + j] * state->perceptualOutput[j];
			weighted = (int16_t)(sum >> 16);
			for (j = order - 1; j >= 1; j--) {
				state->perceptualInput[j] = state->perceptualInput[j - 1];
				state->perceptualOutput[j] = state->perceptualOutput[j - 1];
			}
			state->perceptualInput[0] = sample;
			state->perceptualOutput[0] = weighted;
			history[PITCH_HISTORY + subframe * state->subframeLength + i] = weighted;
		}
	}

	for (i = 0; i < blockCount; i++) {
		int32_t base = i * block, current = base + PITCH_HISTORY, lag;
		int64_t cross = 0, energy = 0, best, score, currentEnergy = 0, residual, difference;
		int32_t correlation, power, quotient;

		for (j = 0; j < block; j++) {
			cross += (int64_t)history[current + j] * history[base + j];
			energy += (int64_t)history[base + j] * history[base + j];
		}
		correlation = (int32_t)(cross >> 5);
		power = (int32_t)(energy >> 5);
		quotient = 0;
		if (power != 0 && correlation >= 1)
			quotient = celpEncDivide((int64_t)correlation * correlation, power);
		score = (int64_t)quotient << 17;
		best = score > 0 ? score : 0;
		bestLag = PITCH_HISTORY;
		for (lag = PITCH_HISTORY - 1; lag >= PITCH_LAG_MIN; lag--) {
			int32_t start = current - lag;

			cross = 0;
			for (j = 0; j < PITCH_CORRELATION; j++)
				cross += (int64_t)history[current + j] * history[start + j];
			energy = energy - (int64_t)history[start - 1] * history[start - 1]
				+ (int64_t)history[block + start - 1] * history[block + start - 1];
			correlation = (int32_t)(cross >> 5);
			power = (int32_t)(energy >> 5);
			quotient = 0;
			if (power != 0 && correlation >= 1)
				quotient = celpEncDivide((int64_t)correlation * correlation, power);
			score = (int64_t)quotient << 17;
			if (bestLag - lag < PITCH_LAG_MIN) {
				if (!(best >= score)) {
					best = score;
					bestLag = lag;
				}
			} else if (!(best * 3 >= score * 4)) {
				best = score;
				bestLag = lag;
			}
		}
		for (j = 0; j < block; j++) {
			currentEnergy += (int64_t)history[current + j] * history[current + j];
			energySum += state->speech[80 + base + j] * state->speech[80 + base + j];
		}
		residual = 0x80000000LL;
		if (currentEnergy != 0)
			residual -= (best << 15) / currentEnergy;
		if (residual < 3)
			residual = 3;
		difference = (int64_t)celpLog10((int32_t)residual) - celpLog10(32768);
		gainSum -= (difference * 10) >> 1;
		for (k = 0; k < state->subframes / blockCount; k++)
			state->lagIndex[i * state->subframes / blockCount + k] = celpEncLagIndex[bestLag];
	}
	gainSum /= blockCount;

	mean = 0;
	if (state->frameLength != 0) {
		int64_t scaled = energySum << 15;

		if (scaled != 0) {
			int32_t negative = scaled < 0;

			if (negative)
				scaled = -scaled;
			mean = celpEncDivide(scaled, state->frameLength);
			if (negative)
				mean = -mean;
		}
	}
	if (mean >= 0x320000 && gainSum >= 49152) {
		if (gainSum < 0x1c000)
			state->gainMode = 1;
		else
			state->gainMode = gainSum < 0x48000 ? 2 : 3;
	} else {
		int32_t high, low, below;

		product = (gainSum * mean) >> 15;
		high = (int32_t)(product >> 32);
		low = (int32_t)product;
		if (high != 0)
			below = high < 0;
		else
			below = !(0xb71b0000u < (uint32_t)low);
		state->gainMode = below ? 0 : 1;
	}
	for (i = 0; i < PITCH_HISTORY; i++)
		state->weightedHistory[i] = history[state->frameLength + i];
}
