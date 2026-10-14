/*
 * Temporal noise shaping's filters (ISO/IEC 14496-3 4.6.9, 0x8b5820): each
 * filter runs all-pole over its bands, upward or downward,
 * y[n] = x[n] - sum a[i] y[n-1-i]. The LPC coefficients are Q31 shifted
 * right by the filter's shift, so the input is scaled up as far before the
 * sum is shifted back. The IVC2 keeps the delay line in register pairs and
 * sums the even and odd taps in two saturating accumulators.
 */
#include "aac.h"

#define LONG_TAPS	12
#define SHORT_TAPS	8

/* 0x8b5d08 (12 taps) and 0x8b6020 (8 taps) */
static void filter(int32_t *x, int32_t size, int32_t step, const int32_t *lpc,
	int32_t shift, int32_t order, int32_t taps)
{
	int32_t coef[LONG_TAPS], history[LONG_TAPS] = { 0 }, n, i;

	for (i = 0; i < order; i++)
		coef[i] = lpc[i];
	for (; i < LONG_TAPS; i++)
		coef[i] = 0;

	for (n = 0; n < size; n++, x += step) {
		int64_t even = 0, odd = (int64_t)*x << (31 - shift);
		int32_t y;

		for (i = taps - 2; i >= 0; i -= 2) {
			even = aacSubtractWide(even, (int64_t)coef[i] * history[i]);
			odd = aacSubtractWide(odd, (int64_t)coef[i + 1] * history[i + 1]);
		}
		y = (int32_t)((even + odd) >> (31 - shift));
		*x = y;
		for (i = taps - 1; i > 0; i--)
			history[i] = history[i - 1];
		history[0] = y;
	}
}

void aacTnsFilter(AacBitstream *bits, AacIcs *ics)
{
	const AacTables *tables = bits->tables;
	AacTnsData *tns = &ics->tns;
	uint32_t isShort = ics->info.windowSequence == AAC_EIGHT_SHORT_SEQUENCE;
	const uint32_t *swbOffset = isShort ? tables->swbOffsetShort[bits->samplingIndex] :
		tables->swbOffsetLong[bits->samplingIndex];
	int32_t maxOrder = tables->tnsMaxOrder[isShort];
	int32_t maxBand = tables->tnsMaxBands[isShort][bits->samplingIndex];
	int32_t windows = isShort ? 8 : 1, numSwb, w, f, filterIndex = 0, coef = 0;

	if (maxBand > ics->info.maxSfb)
		maxBand = ics->info.maxSfb;
	if (!ics->tnsDataPresent)
		return;
	numSwb = tables->numSwb[isShort][bits->samplingIndex];

	for (w = 0; w < windows; w++) {
		int32_t top = numSwb;

		for (f = 0; f < tns->nFilt[w]; f++, filterIndex++) {
			int32_t bottom = top - tns->length[filterIndex], order, start, end;
			int32_t *x;

			if (bottom < 0)
				bottom = 0;
			order = tns->order[filterIndex] < maxOrder ? tns->order[filterIndex] : maxOrder;
			start = swbOffset[bottom < maxBand ? bottom : maxBand];
			end = swbOffset[top < maxBand ? top : maxBand];
			top = bottom;
			if (order == 0 || end - start <= 0)
				continue;

			/* the coefficients advance only past filters that ran */
			x = ics->spectrum + w * 128 + (tns->direction[filterIndex] ? end - 1 : start);
			filter(x, end - start, tns->direction[filterIndex] ? -1 : 1, tns->lpc + coef,
				tns->shift[filterIndex], tns->order[filterIndex],
				windows == 1 ? LONG_TAPS : SHORT_TAPS);
			coef += tns->order[filterIndex];
		}
	}
}
