/*
 * The SBR synthesis QMF bank (0x8c1950-0x8c3998): each slot's subbands
 * scaled to a common exponent, modulated by a cosine transform into
 * 2 * numBands samples, pushed into the windowed delay line, and the
 * window summed into numBands output samples. The original runs on the IVC2.
 */
#include "aac.h"
#include "aacsbr.h"

#define SHIFT_MAX	31

/* the synthesis transform's twiddles, Q27 (0x9241c8, 0x9242c8) */
static const int32_t transformTwiddle64[64] = {
	0, 3293868, 6585752, 9873669, 13155638, 16429683, 19693831, 22946116,
	26184580, 29407271, 32612248, 35797580, 38961350, 42101651, 45216591, 48304294,
	51362901, 54390568, 57385473, 60345811, 63269799, 66155675, 69001702, 71806165,
	74567374, 77283667, 79953407, 82574986, 85146825, 87667375, 90135117, 92548565,
	94906266, 97206798, 99448777, 101630851, 103751707, 105810066, 107804690, 109734376,
	111597962, 113394326, 115122385, 116781099, 118369468, 119886536, 121331389, 122703156,
	124001012, 125224174, 126371905, 127443515, 128438358, 129355834, 130195391, 130956523,
	131638772, 132241727, 132765024, 133208348, 133571433, 133854059, 134056057, 134177304,
};

static const int32_t transformTwiddle32[32] = {
	0, 6585752, 13155638, 19693831, 26184580, 32612248, 38961350, 45216591,
	51362901, 57385473, 63269799, 69001702, 74567374, 79953407, 85146825, 90135117,
	94906266, 99448777, 103751707, 107804690, 111597962, 115122385, 118369468, 121331389,
	124001012, 126371905, 128438358, 130195391, 131638772, 132765024, 133571433, 134056057,
};

/* x >> shift, or a saturating x << shift kept 4 bits down */
static int32_t scale(int32_t x, int32_t shift, int32_t up)
{
	return up ? aacShiftSaturate(x, shift) >> 4 : x >> shift;
}

/*
 * Copies count subbands scaled; the kernels take eight at a time when
 * both ends are multiples of eight, and skip the copy outright when that
 * holds for a left shift (so the band keeps the previous slot's values).
 */
static int32_t *scaleBands(int32_t *out, const int32_t *in, int32_t from, int32_t to,
			   int32_t shift, int32_t aligned)
{
	int32_t up = shift >= 1, k = from;

	shift = up ? aacMin(shift + 4, SHIFT_MAX) : aacMin(-shift, SHIFT_MAX);
	if (aligned && up)
		return out;
	if (k >= to)
		return out;
	do {
		*out++ = scale(in[k], shift, up);
		k++;
	} while (k < to);
	return out;
}

/*
 * The real (low-power) slot (0x8c2388): the scaled subbands, transformed,
 * mirrored into 4 * (numBands / 2) samples, and written into the delay
 * line's two newest taps. With the bank created for parametric stereo the
 * samples go back to the matrix instead.
 */
void aacSbrSynthesisSlotReal(int32_t *real, int32_t *imag, int32_t lowShift,
			     int32_t highShift, int32_t outputShift, AacSbrQmf *qmf, int32_t *work,
			     uint32_t downsampled)
{
	int32_t bands = qmf->numBands, half = bands >> 1;
	int32_t *line = qmf->synthesisBuffer + (downsampled ? 620 : 1260);
	int32_t *data = work + half, *out;
	int32_t k, shift;

	scaleBands(data, real, 0, qmf->kx, lowShift, (qmf->kx & 7) == 0);
	out = scaleBands(data + qmf->kx, real, qmf->kx, qmf->stop, highShift,
			 (qmf->kx & 7) == 0 && (qmf->stop & 7) == 0);
	for (k = qmf->stop; k < bands; k++)
		*out++ = 0;

	aacSbrSynthesisTransform(data, bands, work + 128);

	work[3 * half] = 0;
	for (k = 1; k < half; k++)
		work[3 * half + k] = -work[3 * half - k];
	for (k = 0; k < half; k++)
		work[k] = work[2 * half - k];

	if (qmf->created) {
		aacSbrCopy((int8_t *)real, (const int8_t *)work, bands * 4);
		aacSbrCopy((int8_t *)imag, (const int8_t *)(work + bands), bands * 4);
		return;
	}

	shift = outputShift + 3;
	for (k = 0; k < bands; k++) {
		line[10 - 20 * k] = aacShiftSaturate(work[k], shift) >> 4;
		line[-20 * k] = aacShiftSaturate(work[bands + k], shift) >> 4;
	}
}

/* The complex slot's scaling: in place and negated (0x8c2d70) */
static void negateBands(int32_t *real, int32_t *imag, int32_t from, int32_t to, int32_t shift)
{
	int32_t up = shift >= 1, k;

	shift = up ? aacMin(shift + 4, SHIFT_MAX) : aacMin(-shift, SHIFT_MAX);
	for (k = from; k < to; k++) {
		real[k] = -scale(real[k], shift, up);
		imag[k] = -scale(imag[k], shift, up);
	}
}

/*
 * The complex slot (0x8c2d70): the subbands scaled and negated in place,
 * modulated, and the sums and differences of each band's parts written
 * into the delay line's two newest taps, the lower half of the bands from
 * the front and the upper half mirrored.
 */
void aacSbrSynthesisSlot(int32_t *real, int32_t *imag, int32_t lowShift, int32_t highShift,
			 int32_t outputShift, AacSbrQmf *qmf, int32_t *work, uint32_t downsampled)
{
	int32_t bands = qmf->numBands, half = bands >> 1;
	int32_t *buffer = qmf->synthesisBuffer;
	int32_t *line = buffer + (downsampled ? 620 : 1260);
	int32_t shift = outputShift + 2;
	int32_t k;

	negateBands(real, imag, 0, qmf->kx, lowShift);
	negateBands(real, imag, qmf->kx, qmf->stop, highShift);
	for (k = qmf->stop; k < bands; k++) {
		real[k] = 0;
		imag[k] = 0;
	}

	aacSbrModulateReal(real, bands, work);
	aacSbrModulateImag(imag, bands, work);

	for (k = 0; k < half; k++) {
		int32_t mirror = bands - 1 - k;

		line[10 - 20 * k] = aacShiftSaturate(real[k] - imag[k], shift) >> 4;
		buffer[20 * k] = aacShiftSaturate(-(real[k] + imag[k]), shift) >> 4;
		line[-20 * k] = aacShiftSaturate(-(real[mirror] + imag[mirror]), shift) >> 4;
		buffer[10 + 20 * k] = aacShiftSaturate(real[mirror] - imag[mirror], shift) >> 4;
	}
}

static int32_t shift28(int64_t value)
{
	return (int32_t)(value >> 28);
}

/*
 * 0x8e31b8: the evens forward and the odds reversed as length / 2 complex
 * points, a complex FFT, the spectrum untangled against its mirror, and
 * turned back into the real outputs.
 */
static void transform(int32_t *data, int32_t length, const int32_t *table, int32_t *work)
{
	int32_t half = length >> 1;
	int32_t k;

	for (k = 0; k < half; k++) {
		work[k] = data[2 * k];
		work[length - 1 - k] = data[2 * k + 1];
	}

	aacFft(work, half);

	for (k = 1; k < length >> 2; k++) {
		int32_t *a = &work[2 * k], *b = &work[length - 2 * k];
		int64_t c = table[4 * k], s = table[length - 4 * k];
		int64_t diffRe = b[0] - a[0], sumIm = a[1] + b[1];
		int64_t p = c * diffRe + s * sumIm, q = s * diffRe - c * sumIm;
		int64_t sumRe = (int64_t)(a[0] + b[0]) * 0x08000000;
		int64_t diffIm = (int64_t)(b[1] - a[1]) * 0x08000000;

		a[0] = shift28(sumRe + p);
		b[0] = shift28(sumRe - p);
		a[1] = shift28(diffIm - q);
		b[1] = shift28(-diffIm - q);
	}

	for (k = 1; k < half; k++) {
		int64_t u = table[k], v = table[length - k];
		int64_t re = work[2 * k], im = work[2 * k + 1];

		data[length - k] = shift28(v * im + u * re);
		data[k] = shift28(v * re - u * im);
	}

	data[0] = (work[0] + work[1]) >> 1;
	data[half] = shift28((int64_t)table[half] * (work[0] - work[1]));
}

/* 0x86cbe2: the synthesis bank's cosine transform, 64 or 32 points */
void aacSbrSynthesisTransform(int32_t *data, int32_t length, int32_t *work)
{
	if (length == 64)
		transform(data, 64, transformTwiddle64, work);
	else if (length == 32)
		transform(data, 32, transformTwiddle32, work);
}

/*
 * 0x8c1950: each band's row of the delay line holds two ten-tap lines;
 * the odd taps of the first and the even taps of the second meet the
 * window reversed, and both lines move along by one. Rows run from the
 * last band down; the downsampled bank takes every other window row.
 */
void aacSbrSynthesisOutput(int32_t *output, AacSbrQmf *qmf, uint32_t downsampled)
{
	const int32_t *window = &aacSbrQmfWindow[downsampled ? 650 : 640];
	int32_t stride = downsampled ? 20 : 10;
	int32_t *row = qmf->synthesisBuffer;
	int32_t *out = output + qmf->numBands - 1;
	int32_t rows = downsampled ? 32 : 64;
	int32_t r, tap;

	for (r = 0; r < rows; r++, row += 20, window += stride) {
		int64_t sum = 0;

		for (tap = 0; tap < 5; tap++) {
			sum += (int64_t)row[2 * tap + 1] * window[8 - 2 * tap];
			sum += (int64_t)row[10 + 2 * tap] * window[9 - 2 * tap];
		}

		for (tap = 9; tap > 1; tap--) {
			row[tap] = row[tap - 1];
			row[10 + tap] = row[10 + tap - 1];
		}
		row[1] = row[0];
		row[0] = row[2];
		row[11] = row[10];
		row[10] = row[12];

		*out-- = (int32_t)(sum >> 26);
	}
}
