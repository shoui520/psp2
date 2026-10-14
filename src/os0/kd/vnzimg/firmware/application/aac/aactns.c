/*
 * Temporal noise shaping: the filters' coefficients (0x85111c-0x8512ca).
 * The transmitted reflection coefficients become direct-form LPC
 * coefficients, Q31 halved while any would leave [-1/2, 1/2).
 */
#include "aac.h"

static int32_t mulQ31(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)a * b) >> 31);
}

void aacTnsDecode(AacBitstream *bits, AacTnsData *tns, uint32_t windowSequence)
{
	int32_t parcor[AAC_TNS_MAX_ORDER] = { 0 }, next[AAC_TNS_MAX_ORDER] = { 0 };
	uint8_t windows = windowSequence == AAC_EIGHT_SHORT_SEQUENCE ? 8 : 1;
	int32_t *lpc = tns->lpc, w, f, m, k, value, overflow, coef = 0, filter = 0;
	uint32_t sign, compressedSign, index;
	const int32_t *table;
	uint8_t *order;

	for (w = 0; w < windows; w++) {
		for (f = 0; f < tns->nFilt[w]; f++) {
			sign = 1 << (tns->coefRes[w] + 2);
			compressedSign = sign >> tns->coefCompress[filter];
			table = tns->coefRes[w] ? bits->tables->tnsCoef4 : bits->tables->tnsCoef3;
			order = &tns->order[filter];
			if (*order > AAC_TNS_MAX_ORDER)
				*order = AAC_TNS_MAX_ORDER;
			for (m = 0; m < *order; m++) {
				index = tns->coef[coef++];
				if (index & compressedSign)
					index |= sign;
				parcor[m] = table[index];
			}

			/* the step-up recursion */
			tns->shift[filter] = 1;
			for (m = 0; m < *order; m++) {
				overflow = 0;
				for (k = 0; k < m; k++) {
					value = lpc[k] + mulQ31(lpc[m - k - 1], parcor[m]);
					next[k] = value;
					if (value >= 0x40000000 || value < -0x40000000)
						overflow = 1;
				}
				for (k = 0; k < m; k++)
					lpc[k] = overflow ? next[k] >> 1 : next[k];
				if (overflow)
					tns->shift[filter]++;
				lpc[m] = parcor[m] >> tns->shift[filter];
			}
			filter++;
			lpc += *order;
		}
	}
}
