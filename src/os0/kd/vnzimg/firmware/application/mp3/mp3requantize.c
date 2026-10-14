/*
 * Requantization (ISO/IEC 11172-3 2.4.3.4.7.1; VLIW, 0x8e8f48): each band
 * gets sign(x) |x|^(4/3) 2^(e/4), with e = global_gain - 210 less the
 * scale factor (with pretab) times 2 or 4, and for short windows also
 * subblock_gain times 8. |x|^(4/3) is the table to 1023, interpolated in
 * steps of 8 above (to 8206).
 */
#include "mp3.h"

#define GAIN_OFFSET	210
#define DIRECT_LIMIT	1024
#define MAX_VALUE	8206

/* 2^(n/4), Q30, truncated */
static const int32_t quarterSteps[4] = {
	1073741824, 1276901416, 1518500249, 1805811301,
};

/* pretab, for preflag */
static const int16_t pretab[MP3_LONG_BANDS] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0,
};

/* 0x8e8d88: `count` values times 2^(exponent / 4) */
static void scaleBand(int32_t *x, int32_t count, int32_t exponent)
{
	int32_t step = quarterSteps[exponent & 3], down = -(exponent >> 2) & 63, i;

	for (i = 0; i < count; i++) {
		int32_t value = x[i], coefficient = value < 0 ? -step : step;
		int32_t magnitude = value < 0 ? -value : value;
		int64_t scaled;

		if (value == 0)
			continue;
		if (magnitude < DIRECT_LIMIT) {
			scaled = ((int64_t)mp3Pow43[magnitude] * coefficient) >> 19;
		} else {
			int32_t low, difference;

			if (magnitude > MAX_VALUE)
				magnitude = MAX_VALUE;
			low = mp3Pow43[magnitude >> 3];
			difference = mp3Pow43[(magnitude >> 3) + 1] - low;
			low += (int32_t)(((int64_t)difference * (magnitude & 7)) >> 3);
			scaled = ((int64_t)low * coefficient) >> 15;
		}
		x[i] = (int32_t)(scaled >> down);
	}
}

/* the exponent of a band: its scale factor shifted by scalefac_scale + 1 */
static int32_t bandExponent(const Mp3Granule *granule, int32_t scalefactor, int32_t extra)
{
	return granule->globalGain - GAIN_OFFSET -
		((int32_t)((uint32_t)scalefactor << ((granule->scalefacScale + 1) & 31)) + extra);
}

static int32_t *longBands(const Mp3Granule *granule, const Mp3Scalefactors *scalefactors,
	const uint16_t *bounds, int32_t *x, uint32_t bands)
{
	uint32_t sfb;

	for (sfb = 0; sfb < bands; sfb++) {
		int32_t scalefactor = scalefactors->longBand[sfb];
		int32_t width = bounds[sfb + 1] - bounds[sfb];

		if (granule->preflag)
			scalefactor += pretab[sfb];
		scaleBand(x, width, bandExponent(granule, scalefactor, 0));
		x += width;
	}
	return x;
}

void mp3Requantize(Mp3Decoder *decoder, uint32_t ch, uint32_t gr)
{
	const Mp3Granule *granule = &decoder->side.channel[ch].granule[gr];
	const Mp3BandBounds *bounds = &mp3BandBounds[decoder->header.samplingIndex];
	const Mp3Scalefactors *scalefactors = &decoder->scalefactors[ch];
	int32_t *x = decoder->synthesis->xr[ch];
	uint32_t sfb = 0, window;

	if (!granule->windowSwitching || granule->blockType != MP3_BLOCK_SHORT) {
		longBands(granule, scalefactors, bounds->longBounds, x, MP3_LONG_BANDS);
		return;
	}
	if (granule->mixedBlock) {
		/* the long bands below the first short band 3: 8 of them for MPEG-1 */
		x = longBands(granule, scalefactors, bounds->longBounds, x,
			decoder->header.samplingIndex < 3 ? 8 : 6);
		sfb = 3;
	}
	for (; sfb < MP3_SHORT_BANDS; sfb++) {
		int32_t width = bounds->shortBounds[sfb + 1] - bounds->shortBounds[sfb];

		for (window = 0; window < 3; window++) {
			scaleBand(x, width, bandExponent(granule,
				scalefactors->shortBand[sfb][window],
				granule->subblockGain[window] << 3));
			x += width;
		}
	}
}
