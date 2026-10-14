/*
 * The complex FFTs under the SBR and PS transforms (0x86d5c8, kernels
 * 0x8e4460 and 0x8e4b20): forward, in place on {re, im} word pairs, in
 * natural order, each stage halving (the radix-4 stage quartering) so the
 * result is scaled by 1 / length. The original's kernels are unrolled on
 * the IVC2; the twiddle products keep its rounding (Q27, shifted after the
 * sum, a negated imaginary part negated after the shift).
 */
#include "aac.h"
#include "aacsbr.h"

#define DIAGONAL	0x05a8279a	/* cos(pi / 4), Q27 */

typedef struct {
	int32_t re;
	int32_t im;
} Complex;

/* cos(q pi / 16) and sin(q pi / 16), Q27, for q = 1..7 (4 is the diagonal) */
static const int32_t twiddleCos[8] = {
	0x08000000, 0x07d8a5f4, 0x07641af4, 0x06a6d98a, DIAGONAL, 0x0471cece, 0x030fbc55, 0x018f8b84,
};
static const int32_t twiddleSin[8] = {
	0, 0x018f8b84, 0x030fbc55, 0x0471cece, DIAGONAL, 0x06a6d98a, 0x07641af4, 0x07d8a5f4,
};

static int32_t shift27(int64_t value)
{
	return (int32_t)(value >> 27);
}

/*
 * x * e^(-j q pi / 16), 0 <= q < 16. Past a quarter turn the kernels turn
 * by q - 8 and negate the imaginary part after the shift; on the diagonal
 * they add the parts before multiplying.
 */
static Complex twiddle(Complex x, int32_t q)
{
	int64_t a = x.re, b = x.im;
	int32_t late = q > 8;
	Complex out;

	if (q == 0)
		return x;
	if (q == 8) {
		out.re = x.im;
		out.im = -x.re;
		return out;
	}

	if (late)
		q -= 8;
	if (q == 4) {
		out.re = shift27((int64_t)(x.re + x.im) * DIAGONAL);
		out.im = shift27((int64_t)(x.im - x.re) * DIAGONAL);
	} else {
		out.re = shift27(a * twiddleCos[q] + b * twiddleSin[q]);
		out.im = shift27(b * twiddleCos[q] - a * twiddleSin[q]);
	}

	if (late) {
		int32_t re = out.re;

		out.re = out.im;
		out.im = -re;
	}
	return out;
}

/* top, bottom = (top + x * w, top - x * w) / 2 */
static void butterfly(Complex *top, Complex *bottom, Complex x, int32_t q)
{
	Complex t = *top, turned = twiddle(x, q);

	top->re = (t.re + turned.re) >> 1;
	top->im = (t.im + turned.im) >> 1;
	bottom->re = (t.re - turned.re) >> 1;
	bottom->im = (t.im - turned.im) >> 1;
}

/* the four-point DFT of x[0], x[stride], ..., quartered */
static void radix4(const Complex *x, int32_t stride, Complex *out)
{
	Complex a0 = x[0], a1 = x[stride], a2 = x[2 * stride], a3 = x[3 * stride];
	int32_t sumRe = a0.re + a2.re, sumIm = a0.im + a2.im;
	int32_t oddRe = a1.re + a3.re, oddIm = a1.im + a3.im;
	int32_t diffRe = a0.re - a2.re, diffIm = a0.im - a2.im;
	int32_t turnRe = a1.re - a3.re, turnIm = a1.im - a3.im;

	out[0].re = (sumRe + oddRe) >> 2;
	out[0].im = (sumIm + oddIm) >> 2;
	out[1].re = (diffRe + turnIm) >> 2;
	out[1].im = (diffIm - turnRe) >> 2;
	out[2].re = (sumRe - oddRe) >> 2;
	out[2].im = (sumIm - oddIm) >> 2;
	out[3].re = (diffRe - turnIm) >> 2;
	out[3].im = (diffIm + turnRe) >> 2;
}

/* 0x8e4460 */
static void fft16(Complex *x)
{
	static const int8_t order[4] = { 0, 2, 1, 3 };
	Complex t[16];
	int32_t g, k;

	for (g = 0; g < 4; g++)
		radix4(&x[g], 4, &t[4 * order[g]]);

	for (g = 0; g < 16; g += 8) {
		for (k = 0; k < 4; k++) {
			x[g + k] = t[g + k];
			butterfly(&x[g + k], &x[g + k + 4], t[g + k + 4], 4 * k);
		}
	}

	for (k = 0; k < 8; k++)
		butterfly(&x[k], &x[k + 8], x[k + 8], 2 * k);
}

/* 0x8e4b20 */
static void fft32(Complex *x)
{
	Complex t[32];
	int32_t g, k;

	for (g = 0; g < 8; g++)
		radix4(&x[g], 8, &t[4 * (g >> 1) + 16 * (g & 1)]);

	for (g = 0; g < 32; g += 16) {
		for (k = 0; k < 8; k++) {
			int32_t out = g + (k < 4 ? k : k + 4);

			x[out] = t[g + k];
			butterfly(&x[out], &x[out + 4], t[g + k + 8], 4 * (k & 3));
		}
	}

	for (g = 0; g < 32; g += 16)
		for (k = 0; k < 8; k++)
			butterfly(&x[g + k], &x[g + k + 8], x[g + k + 8], 2 * k);

	for (k = 0; k < 16; k++)
		butterfly(&x[k], &x[k + 16], x[k + 16], k);
}

/* 0x86d5c8: the kernel for the length, 32 or 16 points */
void aacFft(int32_t *data, int32_t length)
{
	int32_t bits = 1;

	if (length > 2)
		while ((1 << ++bits) < length)
			;

	if (bits == 5)
		fft32((Complex *)data);
	else
		fft16((Complex *)data);
}
