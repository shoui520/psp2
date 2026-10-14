/*
 * Inverse quantization and scaling (ISO/IEC 14496-3 4.6.1-4.6.2, 4.6.13,
 * 0x8b4e38): each quantized value becomes sign(x) |x|^(4/3) 2^((sf-100)/4),
 * in fixed point. |x|^(4/3) is a table of the mantissa for 32 <= a < 64,
 * interpolated linearly when |x| had to be shifted down, times 2^(n/3) for
 * the shift n. Noise bands are filled by the noise generator, and intensity
 * bands (the left channel's values, copied by the parser) are scaled by
 * the difference of the two channels' scale factors.
 */
#include "aac.h"

#define MIN_EXPONENT	(-31)

/* sign(x) |x|^(4/3), times scale, shifted by exponent and rounded */
static int32_t dequantize(const AacTables *tables, int32_t x, int32_t scale, int32_t exponent)
{
	int32_t magnitude = x < 0 ? -x : x, sign = x < 0 ? -1 : 1;
	int32_t a = magnitude * 64, n = 0, mantissa, value, shift;
	int64_t shifted;

	while (a >= 64) {
		a >>= 1;
		n++;
	}
	mantissa = tables->pow43Mantissa[a - 32];
	if (n >= 7) {
		int32_t step = tables->pow43Mantissa[a - 31] - mantissa;

		mantissa += (step * (magnitude * 64 - (a << n))) >> n;
	}
	value = (int32_t)(aacProduct(mantissa, tables->pow2Third[n]) >> 32);
	value = (int32_t)(aacProduct(value, scale) >> 32) * sign;

	shift = tables->pow43Exponent[n] + exponent;
	if (shift < MIN_EXPONENT)
		shift = MIN_EXPONENT;
	shift++;
	shifted = shift >= 0 ? (int64_t)value << shift : (int64_t)value >> -shift;
	return (int32_t)((shifted + 1) >> 1);
}

void aacDequantize(AacBitstream *bits, AacIcs *ics, AacChannelPairElement *cpe,
	uint32_t value14, int32_t *work)
{
	const AacTables *tables = bits->tables;
	const AacIcsInfo *info = &ics->info;
	uint32_t isShort = info->windowSequence == AAC_EIGHT_SHORT_SEQUENCE;
	const uint32_t *swbOffset = isShort ? tables->swbOffsetShort[bits->samplingIndex] :
		tables->swbOffsetLong[bits->samplingIndex];
	int32_t numSwb = tables->numSwb[isShort][bits->samplingIndex];
	int32_t count = info->maxSfb < numSwb ? info->maxSfb : numSwb;
	int32_t extraShift = value14 == 32 ? 7 : 0;
	int32_t frameShift = isShort ? -22 : -25;	/* the window's length */
	int32_t noiseShift = isShort ? -23 : -26;
	int32_t window = 0, g, w, sfb, k;

	for (g = 0; g < info->numWindowGroups; g++) {
		for (w = 0; w < info->windowGroupLength[g]; w++, window += 128) {
			int32_t band = g * info->maxSfb;

			for (sfb = 0; sfb < count; sfb++, band++) {
				int32_t *spectrum = ics->spectrum + window;
				uint32_t cb = ics->section.sfbCb[band];
				int32_t t, scale, exponent;

				if (cb == AAC_NOISE_HCB) {
					t = ics->scaleFactors[band] - 90;
					aacNoiseGenerate(&bits->noise, spectrum + swbOffset[sfb],
						swbOffset[sfb + 1] - swbOffset[sfb],
						tables->pow2Quarter[t & 3] >> 8,
						(t >> 2) + 1 + extraShift + noiseShift, work);
					continue;
				}
				if (cb == AAC_INTENSITY_HCB || cb == AAC_INTENSITY_HCB2) {
					int32_t sign = 1;

					if (cpe->msMaskPresent == 1)
						sign = 1 - 2 * cpe->msUsed[band];
					if (cb != AAC_INTENSITY_HCB)
						sign = -sign;
					t = cpe->ics[0]->scaleFactors[band] - ics->scaleFactors[band] - 100;
					scale = sign * tables->pow2Quarter[t & 3];
				} else {
					t = ics->scaleFactors[band] - 100;
					scale = tables->pow2Quarter[t & 3];
				}
				exponent = (t >> 2) + 1 + extraShift + frameShift;

				for (k = swbOffset[sfb]; k < (int32_t)swbOffset[sfb + 1]; k++)
					spectrum[k] = spectrum[k] ?
						dequantize(tables, spectrum[k], scale, exponent) : 0;
			}
		}
	}
}
