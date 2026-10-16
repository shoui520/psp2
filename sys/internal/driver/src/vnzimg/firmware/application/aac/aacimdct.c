/*
 * The inverse MDCT's core, a DCT-IV of each window (0x8b3320): the
 * coefficients pairwise rotated into N/2 complex values, a radix-2 FFT
 * with saturating butterflies and bit-reversed twiddles, and the
 * bit-reversed result rotated back into the window's two halves. The
 * products are Q31 (IVC2 cpmulsl / cpsmadsl / cpsmsbsl, high words).
 * The complex values are kept as real parts at work[0] and imaginary
 * parts at work[512].
 */
#include "aac.h"

/* (x + iy)(c + is), Q31 */
static void rotate(int32_t x, int32_t y, int32_t c, int32_t s, int32_t *re, int32_t *im)
{
	*re = (int32_t)(aacSubtractWide(aacProduct(x, c), aacProduct(y, s)) >> 32);
	*im = (int32_t)(aacAddWide(aacProduct(x, s), aacProduct(y, c)) >> 32);
}

/* one butterfly: a + W b, a - W b */
static void butterfly(int32_t *re, int32_t *im, int32_t a, int32_t b, int32_t c, int32_t s)
{
	int32_t tRe, tIm;

	rotate(re[b], im[b], c, s, &tRe, &tIm);
	re[b] = aacSubtractSaturate(re[a], tRe);
	im[b] = aacSubtractSaturate(im[a], tIm);
	re[a] = aacAddSaturate(re[a], tRe);
	im[a] = aacAddSaturate(im[a], tIm);
}

/* the decimation-in-frequency FFT; the first two stages need no products */
static void fft(int32_t *re, int32_t *im, int32_t length, const int32_t *cos,
	const int32_t *sin)
{
	int32_t half = length >> 1, span, start, block, j;

	/* span length / 2, and the second stage's twiddles 1 and -i */
	for (j = 0; j < half; j++) {
		int32_t aRe = re[j], aIm = im[j], bRe = re[j + half], bIm = im[j + half];

		re[j] = aacAddSaturate(aRe, bRe);
		im[j] = aacAddSaturate(aIm, bIm);
		re[j + half] = aacSubtractSaturate(aRe, bRe);
		im[j + half] = aacSubtractSaturate(aIm, bIm);
	}
	span = half >> 1;
	for (j = 0; j < span; j++) {
		int32_t aRe = re[j], aIm = im[j], bRe = re[j + span], bIm = im[j + span];

		re[j] = aacAddSaturate(aRe, bRe);
		im[j] = aacAddSaturate(aIm, bIm);
		re[j + span] = aacSubtractSaturate(aRe, bRe);
		im[j + span] = aacSubtractSaturate(aIm, bIm);
	}
	for (j = half; j < half + span; j++) {
		int32_t aRe = re[j], aIm = im[j], bRe = re[j + span], bIm = im[j + span];

		re[j] = aacAddSaturate(aRe, bIm);
		im[j] = aacSubtractSaturate(aIm, bRe);
		re[j + span] = aacSubtractSaturate(aRe, bIm);
		im[j + span] = aacAddSaturate(aIm, bRe);
	}

	/* block m's twiddle is the table's m-th, in bit-reversed order */
	for (span >>= 1; span >= 1; span >>= 1)
		for (start = 0, block = 0; start + span < length; start += 2 * span, block++)
			for (j = start; j < start + span; j++)
				butterfly(re, im, j, j + span, cos[block], sin[block]);
}

/* m with its log2(length) bits reversed, from the 6-bit table */
static int32_t reverse(const uint8_t *table, int32_t m, int32_t length)
{
	if (length == 64)
		return table[m];
	return (table[m & 63] << 3) + (table[m >> 6] >> 3);
}

void aacImdct(AacBitstream *bits, AacIcs *ics, int32_t *work)
{
	const AacTables *tables = bits->tables;
	int32_t isShort = ics->info.windowSequence == AAC_EIGHT_SHORT_SEQUENCE;
	int32_t length = isShort ? 128 : 1024, half = length >> 1, quarter = half >> 1;
	int32_t windows = isShort ? 8 : 1, w, k;
	const int32_t *cos = isShort ? tables->imdctCosShort : tables->imdctCosLong;
	const int32_t *sin = isShort ? tables->imdctSinShort : tables->imdctSinLong;
	int32_t *re = work, *im = work + 512;

	for (w = 0; w < windows; w++) {
		int32_t *x = ics->spectrum + w * length;

		for (k = 0; k < quarter; k++) {
			rotate(x[2 * k], x[length - 1 - 2 * k], cos[k], sin[k], &re[k], &im[k]);
			rotate(x[length - 2 - 2 * k], x[2 * k + 1], cos[half - 1 - k],
				sin[half - 1 - k], &re[half - 1 - k], &im[half - 1 - k]);
		}
		fft(re, im, half, tables->fftCos, tables->fftSin);
		for (k = 0; k < half; k++) {
			int32_t i = reverse(tables->bitReverse, k, half);

			rotate(re[i], im[i], cos[k], sin[k], &x[half + k], &x[k]);
		}
	}
}
