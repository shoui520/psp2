/*
 * The canceller's spectra (0x8dfc38-0x8e0130): 1024-sample frames through
 * one complex FFT (0x8df640), two real frames at once as the real and
 * imaginary parts, then separated by the conjugate symmetry; and back,
 * the 512 bins mirrored into a Hermitian spectrum, clamped to 16 bits.
 */
#include "aec.h"

#define POINTS	(2 * AEC_BINS)

/* x / 2, towards zero */
static int32_t half(int32_t value)
{
	return value / 2;
}

/* 0x8dfc38 */
int32_t aecTransform(uint32_t mode, const int16_t *frame, const int16_t *frame2,
	AecComplex *spectrum, AecComplex *spectrum2)
{
	AecComplex z[POINTS];
	int32_t n, k;

	for (n = 0; n < POINTS; n++) {
		z[n].re = frame[n];
		z[n].im = frame2[n];
	}
	aecFft(mode, z, 0);

	spectrum[0].re = z[0].re;
	spectrum[0].im = 0;
	spectrum2[0].re = z[0].im;
	spectrum2[0].im = 0;
	for (k = 1; k < AEC_BINS; k++) {
		const AecComplex *a = &z[k], *b = &z[POINTS - k];

		spectrum[k].re = half(b->re + a->re);
		spectrum[k].im = half(a->im - b->im);
		spectrum2[k].re = half(b->im + a->im);
		spectrum2[k].im = half(-(a->re - b->re));
	}
	return 0;
}

/* 0x8dfe98 (unreferenced): one real frame's first 512 bins */
int32_t aecTransformReal(uint32_t mode, const int16_t *frame, AecComplex *spectrum,
	uint32_t inverse)
{
	AecComplex z[POINTS];
	int32_t n;

	for (n = 0; n < POINTS; n++) {
		z[n].re = frame[n];
		z[n].im = 0;
	}
	aecFft(mode, z, inverse);
	for (n = 0; n < AEC_BINS; n++)
		spectrum[n] = z[n];
	return 0;
}

/* 0x8dffa8 */
int32_t aecInverse(uint32_t mode, const AecComplex *spectrum, int16_t *frame, uint32_t inverse)
{
	AecComplex z[POINTS];
	int32_t n, k;

	z[0] = spectrum[0];
	z[AEC_BINS].re = 0;
	z[AEC_BINS].im = 0;
	for (k = 1; k < AEC_BINS; k++) {
		z[k] = spectrum[k];
		z[POINTS - k].re = spectrum[k].re;
		z[POINTS - k].im = -spectrum[k].im;
	}
	aecFft(mode, z, inverse);
	for (n = 0; n < POINTS; n++)
		frame[n] = z[n].re > 32767 ? 32767 : z[n].re < -32768 ? -32768 : z[n].re;
	return 0;
}
