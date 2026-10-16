/*
 * Parametric stereo's hybrid filterbank (ISO/IEC 14496-3 8.6.4.3): QMF
 * bands 0-2 split by 13-tap filters into 8 (6 kept), 2 and 2 subbands,
 * and summed back after the mixing (0x86d5f8, 0x8e5800-0x8e5f60).
 */
#include "aac.h"

#define DIAGONAL	0x05a82798	/* cos(pi / 4), Q27 */
#define COS_PI_8	0x07641af0	/* cos(pi / 8) */
#define COS_3PI_8	0x030fbc54	/* cos(3 pi / 8) */

/* subbands per QMF band */
static const int8_t bandSplit[4] = { 8, 2, 2, 0 };

/* the two-band filter's odd taps, Q27; the middle tap is 1/2 */
static const int32_t twoBandFilter[6] = {
	2549449, -9788686, 41066104, 41066104, -9788686, 2549449,
};

/* the eight-band filter, Q27 */
static const int32_t eightBandFilter[13] = {
	-1001376, -3047308, -6102700, -9752413, -13267568, 15829250, 16777216,
	15829250, 13267568, 9752413, 6102700, 3047308, 1001376,
};

static int32_t shift27(int64_t value)
{
	return (int32_t)(value >> 27);
}

/* 0x8e5980: the real two-band filter, low band then high band */
static void twoBand(const int32_t *in, int32_t *out)
{
	int64_t even = (int64_t)in[6] * 0x04000000;
	int64_t odd = 0;
	int32_t k;

	for (k = 0; k < 6; k++)
		odd += (int64_t)twoBandFilter[k] * in[1 + 2 * k];
	out[0] = shift27(even + odd);
	out[1] = shift27(even - odd);
}

/* 0x8e5800: the eight-point inverse FFT of the modulated taps */
static void fft8(const int32_t *work, int32_t *outReal, int32_t *outImag)
{
	int32_t re[8], im[8];
	int32_t aRe, aIm, bRe, bIm, cRe, cIm, dRe, dIm;
	int32_t eRe, eIm, fRe, fIm, iRe, iIm, jRe, jIm;
	int32_t r0, r1, s0, s1;
	int32_t k;

	for (k = 0; k < 8; k++) {
		re[k] = work[2 * k];
		im[k] = work[2 * k + 1];
	}

	/* even: the sums of 0, 4 and 2, 6, then of 1, 5 and 3, 7 */
	aRe = (re[0] + re[4]) + (re[2] + re[6]);
	aIm = (im[0] + im[4]) + (im[2] + im[6]);
	bRe = (re[0] + re[4]) - (re[2] + re[6]);
	bIm = (im[0] + im[4]) - (im[2] + im[6]);
	cRe = (re[1] + re[5]) + (re[3] + re[7]);
	cIm = (im[1] + im[5]) + (im[3] + im[7]);
	dRe = (re[1] + re[5]) - (re[3] + re[7]);
	dIm = (im[1] + im[5]) - (im[3] + im[7]);
	outReal[0] = aRe + cRe;
	outImag[0] = aIm + cIm;
	outReal[4] = aRe - cRe;
	outImag[4] = aIm - cIm;
	outReal[2] = bRe - dIm;
	outImag[2] = bIm + dRe;
	outReal[6] = bRe + dIm;
	outImag[6] = bIm - dRe;

	/* odd: the differences turned by j, the 1, 5 / 3, 7 half by pi / 4 */
	iRe = (re[0] - re[4]) - (im[2] - im[6]);
	iIm = (im[0] - im[4]) - (re[2] - re[6]);
	jRe = (re[0] - re[4]) + (im[2] - im[6]);
	jIm = (im[0] - im[4]) + (re[2] - re[6]);
	eRe = (re[1] - re[5]) + (im[3] - im[7]);
	eIm = (im[1] - im[5]) + (re[3] - re[7]);
	fRe = (re[1] - re[5]) - (im[3] - im[7]);
	fIm = (im[1] - im[5]) - (re[3] - re[7]);
	r0 = shift27((int64_t)(fRe - eIm) * DIAGONAL);
	r1 = shift27((int64_t)(fRe + eIm) * DIAGONAL);
	s0 = shift27((int64_t)(eRe + fIm) * DIAGONAL);
	s1 = shift27((int64_t)(eRe - fIm) * DIAGONAL);
	outReal[1] = iRe + r0;
	outImag[1] = jIm + r1;
	outReal[5] = iRe - r0;
	outImag[5] = jIm - r1;
	outReal[3] = jRe - s0;
	outImag[7] = iIm - s1;
	outReal[7] = jRe + s0;
	outImag[3] = iIm + s1;
}

static int32_t addSaturate(int32_t a, int32_t b)
{
	int64_t sum = (int64_t)a + b;

	if (sum > 0x7fffffff)
		return 0x7fffffff;
	if (sum < -(int64_t)0x80000000)
		return (int32_t)0x80000000;
	return (int32_t)sum;
}

static int32_t subtractSaturate(int32_t a, int32_t b)
{
	int64_t difference = (int64_t)a - b;

	if (difference > 0x7fffffff)
		return 0x7fffffff;
	if (difference < -(int64_t)0x80000000)
		return (int32_t)0x80000000;
	return (int32_t)difference;
}

/* x * a + y * b and x * a - y * b, with the cosines of pi / 8 and 3 pi / 8 */
static int32_t rotate(int32_t x, int32_t a, int32_t y, int32_t b)
{
	return shift27((int64_t)x * a + (int64_t)y * b);
}

static int32_t rotateBack(int32_t x, int32_t a, int32_t y, int32_t b)
{
	return shift27((int64_t)x * a - (int64_t)y * b);
}

/*
 * 0x8e5a50: the complex eight-band filter: the 13 taps folded onto 8 and
 * modulated, then the FFT
 */
static void eightBand(const int32_t *inReal, const int32_t *inImag, int32_t *outReal,
	int32_t *outImag, int32_t *work)
{
	int64_t sumRe[8], sumIm[8];
	int32_t r[8], i[8];
	int32_t n, sum, difference;

	for (n = 0; n < 8; n++) {
		sumRe[n] = (int64_t)eightBandFilter[n] * inReal[n];
		sumIm[n] = (int64_t)eightBandFilter[n] * inImag[n];
		if (n + 8 < 13) {
			sumRe[n] += (int64_t)eightBandFilter[n + 8] * inReal[n + 8];
			sumIm[n] += (int64_t)eightBandFilter[n + 8] * inImag[n + 8];
		}
		r[n] = shift27(sumRe[n]);
		i[n] = shift27(sumIm[n]);
	}

	sum = addSaturate(i[0], r[0]);
	difference = subtractSaturate(i[0], r[0]);
	work[12] = shift27((int64_t)sum * DIAGONAL);
	work[13] = shift27((int64_t)difference * DIAGONAL);
	work[10] = rotate(i[1], COS_PI_8, r[1], COS_3PI_8);
	work[11] = rotateBack(i[1], COS_3PI_8, r[1], COS_PI_8);
	work[8] = i[2];
	work[9] = shift27(-sumRe[2]);
	work[6] = rotateBack(i[3], COS_PI_8, r[3], COS_3PI_8);
	work[7] = shift27(-((int64_t)i[3] * COS_3PI_8 + (int64_t)r[3] * COS_PI_8));
	sum = addSaturate(i[4], r[4]);
	difference = subtractSaturate(i[4], r[4]);
	work[4] = shift27((int64_t)difference * DIAGONAL);
	work[5] = shift27((int64_t)(0 - sum) * DIAGONAL);
	work[2] = rotateBack(r[5], COS_PI_8, i[5], COS_3PI_8);
	work[3] = rotate(r[5], COS_3PI_8, i[5], COS_PI_8);
	work[0] = r[6];
	work[1] = i[6];
	work[14] = rotate(i[7], COS_3PI_8, r[7], COS_PI_8);
	work[15] = rotateBack(i[7], COS_PI_8, r[7], COS_3PI_8);
	fft8(work, outReal, outImag);
}

/*
 * 0x86d5f8: QMF bands 0-2 of one slot, rescaled, into the 10 hybrid
 * subbands; the eight-band filter's 2 + 5 and 3 + 4 are summed
 */
void aacPsHybridAnalysis(const int32_t *real, const int32_t *imag, int32_t *outReal,
	int32_t *outImag, AacPsHybrid *hybrid, int32_t shift, int32_t *work)
{
	int32_t *re = hybrid->buffer[0], *im = hybrid->buffer[1];
	int32_t *fRe = hybrid->filtered[0], *fIm = hybrid->filtered[1];
	int32_t band, n = 0;

	for (band = 0; band < 3; band++) {
		aacSbrCopy12(re, hybrid->delayReal[band]);
		aacSbrCopy12(im, hybrid->delayImag[band]);
		re[hybrid->length] = aacShiftDown(real[band], shift);
		im[hybrid->length] = aacShiftDown(imag[band], shift);
		aacSbrCopy12(hybrid->delayReal[band], re + 1);
		aacSbrCopy12(hybrid->delayImag[band], im + 1);

		if (hybrid->bandSplit[band] == 8) {
			eightBand(re, im, fRe, fIm, work);
			outReal[n] = fRe[0];
			outReal[n + 1] = fRe[1];
			outReal[n + 2] = fRe[2] + fRe[5];
			outReal[n + 3] = fRe[3] + fRe[4];
			outReal[n + 4] = fRe[6];
			outReal[n + 5] = fRe[7];
			outImag[n] = fIm[0];
			outImag[n + 1] = fIm[1];
			outImag[n + 2] = fIm[2] + fIm[5];
			outImag[n + 3] = fIm[3] + fIm[4];
			outImag[n + 4] = fIm[6];
			outImag[n + 5] = fIm[7];
			n += 6;
		} else if (hybrid->bandSplit[band] == 2) {
			twoBand(re, fRe);
			twoBand(im, fIm);
			outReal[n] = fRe[0];
			outReal[n + 1] = fRe[1];
			outImag[n] = fIm[0];
			outImag[n + 1] = fIm[1];
			n += 2;
		}
	}
}

/* 0x8e5dc8: each QMF band back as the saturated sum of its subbands */
void aacPsHybridSynthesis(const int32_t *real, const int32_t *imag, int32_t *outReal,
	int32_t *outImag, AacPsHybrid *hybrid)
{
	int32_t band, k, n = 0;

	for (band = 0; band < 3; band++) {
		int32_t count = aacMin(hybrid->bandSplit[band], 6);
		int32_t sumRe = 0, sumIm = 0;

		for (k = 0; k < count; k++) {
			sumRe = addSaturate(sumRe, real[n + k]);
			sumIm = addSaturate(sumIm, imag[n + k]);
		}
		outReal[band] = sumRe;
		outImag[band] = sumIm;
		n += count;
	}
}

/* 0x8e5f60: the filterbank's buffers, from the 8-byte aligned cursor */
void aacPsHybridInit(AacPsHybrid *hybrid, int32_t **cursor)
{
	uintptr_t address = (uintptr_t)*cursor;
	int32_t *next;
	int32_t i;

	if (address & 7)
		address += 8 - (address & 7);
	next = (int32_t *)address;

	hybrid->bandSplit = bandSplit;
	hybrid->length = 12;
	hybrid->buffer[0] = aacPsTake(&next, hybrid->length + 4, 0);
	hybrid->buffer[1] = aacPsTake(&next, hybrid->length + 4, 0);
	for (i = 0; i < 3; i++) {
		hybrid->delayReal[i] = aacPsTake(&next, hybrid->length, 1);
		hybrid->delayImag[i] = aacPsTake(&next, hybrid->length, 1);
	}
	hybrid->filtered[0] = aacPsTake(&next, 8, 1);
	hybrid->filtered[1] = aacPsTake(&next, 8, 1);
	*cursor = next;
}
