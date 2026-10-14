/*
 * The CELP encoder's LPC analysis (0x8c7d98-0x8c8718): every block of the
 * speech is windowed, autocorrelated, lag-windowed and run through the
 * Levinson-Durbin recursion.
 */
#include "celp.h"

/*
 * The coprocessor's restoring division: `remainder` / `divisor` as 31 bits,
 * a bit a step (the divisor is taken at 2^31 times its value).
 */
int32_t celpEncDivide(int64_t remainder, int32_t divisor)
{
	int64_t scaled = (int64_t)divisor << 31;
	int32_t quotient = 0, step;

	for (step = 0; step < 31; step++) {
		int64_t twice = remainder << 1;
		int64_t difference = twice - scaled;

		quotient <<= 1;
		if (difference >= 0) {
			remainder = difference;
			quotient++;
		} else {
			remainder = twice;
		}
	}
	return quotient;
}

/* 0x8c7d98: the LPC of every block of the speech (a >> 4 of the coefficients, one block after another) */
void celpEncAnalyseLpc(CelpEncoderState *state)
{
	int32_t order = state->order;
	int32_t block;

	for (block = 0; block < state->analysisBlocks; block++) {
		int16_t windowed[200];
		int32_t autocorrelation[CELP_MAX_ORDER + 1];
		int32_t reflection[CELP_MAX_ORDER], result[CELP_MAX_ORDER], current[CELP_MAX_ORDER];
		int32_t error, m, i, lag;

		for (i = 0; i < state->analysisLength; i++)
			windowed[i] = (state->speech[state->analysisOffset[block] + i] * state->analysisWindow[i] + 16384) >> 15;
		for (lag = 0; lag <= order; lag++) {
			int64_t sum = 0;

			for (i = 0; i < state->analysisLength - lag; i++)
				sum += windowed[lag + i] * windowed[i];
			autocorrelation[lag] = (int32_t)((sum + 0x40) >> 7);
		}
		for (lag = 0; lag <= order; lag++)
			autocorrelation[lag] = (int32_t)(((int64_t)autocorrelation[lag] * celpEncLagWindow[lag] + 0x400000) >> 23);
		error = autocorrelation[0];
		for (i = 0; i < order; i++) {
			reflection[i] = 0;
			result[i] = 0;
			current[i] = 0;
		}
		if (error >= 2) {
			for (m = 0; m < order; m++) {
				int64_t sum = 0;
				int32_t k, magnitude;

				for (i = 0; i < m; i++)
					sum += (int64_t)current[i] * autocorrelation[m - i];
				if (error >= 1) {
					int64_t numerator = (((int64_t)autocorrelation[m + 1] << 27) - sum) << 1;

					k = 0;
					if (numerator != 0) {
						int32_t negative = numerator < 0;

						if (negative)
							numerator = -numerator;
						k = celpEncDivide(numerator, error);
						if (negative)
							k = -k;
					}
					reflection[m] = k;
				}
				k = reflection[m];
				magnitude = k < 0 ? -k : k;
				if (magnitude >= 0x10000000 || error < 2) {
					for (i = m; i < order; i++) {
						reflection[i] = 0;
						result[i] = 0;
					}
					break;
				}
				result[m] = (k + 1) >> 1;
				for (i = 0; i < m; i++)
					result[i] = (int32_t)((((int64_t)current[i] << 28) - (int64_t)k * current[m - 1 - i]) >> 28);
				error = (int32_t)((int64_t)error * ((0x100000000000000LL - (int64_t)k * k) >> 28) >> 28);
				for (i = 0; i <= m; i++)
					current[i] = result[i];
			}
		}
		for (i = 0; i < order; i++)
			state->analysisLpc[block * order + i] = result[i] >> 4;
	}
}
