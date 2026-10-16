/*
 * The SBR analysis QMF bank (0x8c17c8-0x8c2388): 32 new samples per slot
 * through the windowed delay line, then the modulation as a cosine
 * transform (low power, real) or a complex one. The original runs on the
 * IVC2; the products follow its accumulator arithmetic.
 */
#include "aac.h"
#include "aacsbr.h"

#define ROW		10	/* taps per row of the delay line */
#define WORD_MIN	((int32_t)0x80000000)
#define ACC_MAX		((int64_t)0x7fffffffffffffffLL)
#define ACC_MIN		(-ACC_MAX - 1)

/* cpmulslla1.w / cpsmadslla1.w: a doubled product, saturated to 64 bits */
static int64_t macDouble(int64_t acc, int32_t a, int32_t b)
{
	int64_t product;

	if (a == WORD_MIN && b == WORD_MIN)
		product = ACC_MAX;
	else
		product = (int64_t)a * b * 2;

	if (product > 0 && acc > ACC_MAX - product)
		return ACC_MAX;
	if (product < 0 && acc < ACC_MIN - product)
		return ACC_MIN;
	return acc + product;
}

/*
 * One slot of the window (0x8c17c8): each of the 32 rows sums its even taps
 * into out[31 - row] and its odd ones into out[63 - row], then moves along.
 */
static void analysisWindow(int32_t *out, int32_t *buffer, const int32_t *window)
{
	int32_t row, tap;

	for (row = 0; row < 32; row++) {
		int32_t *line = &buffer[row * ROW];
		const int32_t *coef = &window[row * ROW];
		int64_t even = 0, odd = 0;

		for (tap = 0; tap < ROW; tap += 2) {
			even = macDouble(even, line[tap], coef[tap]);
			odd = macDouble(odd, line[tap + 1], coef[tap + 1]);
		}

		for (tap = ROW - 1; tap > 1; tap--)
			line[tap] = line[tap - 1];
		line[1] = line[0];
		line[0] = line[2];

		out[31 - row] = (int32_t)(even >> 32);
		out[63 - row] = (int32_t)(odd >> 32);
	}
}

/* The low-power modulation (0x8c1be8): fold the 64 sums into 32, transform */
static void analysisReal(const int32_t *sum, int32_t *out, int32_t *work)
{
	int32_t k;

	out[0] = sum[48] >> 1;
	for (k = 1; k < 16; k++)
		out[k] = (sum[48 + k] + sum[48 - k]) >> 1;
	for (k = 0; k < 16; k++)
		out[16 + k] = (sum[32 - k] - sum[k]) >> 1;

	aacSbrCosineTransform(out, 32, aacSbrDctSin32, work);
}

/*
 * The complex modulation (0x8c1da8): the 64 sums as differences (for the
 * real part) and sums (for the imaginary part) of mirrored pairs, both
 * transformed, then the first kx bands turned by the bank's twiddles.
 */
static void analysisComplex(const int32_t *sum, int32_t *real, int32_t *imag, AacSbrQmf *qmf,
			    int32_t *work)
{
	int32_t *diff = work, *both = work + 32;
	int32_t m, k;

	/* each block of eight: sum[start + step * m] against its mirror 63 - that */
	static const int8_t start[8] = { 31, 29, 0, 2, 1, 3, 30, 28 };
	static const int8_t step[8] = { -4, -4, 4, 4, 4, 4, -4, -4 };

	for (m = 0; m < 4; m++) {
		for (k = 0; k < 8; k++) {
			int32_t index = start[k] + step[k] * m;
			int32_t first = sum[index], second = sum[63 - index];

			diff[8 * m + k] = (first - second) >> 1;
			both[8 * m + k] = (first + second) >> 1;
		}
	}

	aacSbrModulateReal(diff, 32, real);
	aacSbrModulateImag(both, 32, imag);

	for (k = 0; k < qmf->kx; k++) {
		int32_t re = real[k], im = imag[k];
		int64_t c = qmf->cos[k], s = qmf->sin[k];

		real[k] = (int32_t)((re * c + im * s) >> 27);
		imag[k] = (int32_t)((im * c - re * s) >> 27);
	}
}

void aacSbrAnalysis(const int32_t *input, int32_t *timing, int32_t **real, int32_t **imag,
		    AacSbrQmf *qmf, int32_t *work, uint8_t created)
{
	int32_t slot, i;

	timing[0] -= 7;
	timing[1] = 0;

	for (slot = 0; slot < qmf->numSlots; slot++) {
		for (i = 0; i < 32; i++)
			qmf->analysisBuffer[i * ROW] = *input++;

		analysisWindow(work, qmf->analysisBuffer, aacSbrQmfAnalysisWindow);

		if (created)
			analysisComplex(work, real[slot], imag[slot], qmf, work + 64);
		else
			analysisReal(work, real[slot], work + 64);
	}
}

/*
 * A real cosine transform of length points through a complex FFT of half
 * the length (0x8e2d40, entered through 0x86cbcc): pair the inputs from both
 * ends under the sine table, untangle the FFT's input, transform, and
 * interleave the halves back.
 */
void aacSbrCosineTransform(int32_t *data, int32_t length, const int32_t *sine, int32_t *work)
{
	int32_t half = length >> 1;
	int32_t quarter = length >> 2;
	int32_t k;

	work[0] = (int32_t)(((int64_t)data[0] * 0x08000000 + (int64_t)data[half] * sine[half]) >> 28);
	work[1] = (int32_t)(((int64_t)data[0] * 0x08000000 - (int64_t)data[half] * sine[half]) >> 28);
	for (k = 1; k < half; k++) {
		int32_t a = data[k], b = data[length - k];
		int32_t s = sine[k], c = sine[length - k];

		work[2 * k] = (int32_t)(((int64_t)c * a + (int64_t)s * b) >> 28);
		work[2 * k + 1] = (int32_t)(((int64_t)c * b - (int64_t)s * a) >> 28);
	}

	for (k = 1; k < quarter; k++) {
		int32_t *x = &work[2 * k], *y = &work[length - 2 * k];
		int32_t sumRe = x[0] + y[0], sumIm = x[1] + y[1];
		int32_t diffRe = y[0] - x[0], diffIm = x[1] - y[1];
		int32_t c = sine[4 * k], s = sine[length - 4 * k];
		int64_t first, second;

		first = (int64_t)sumRe * 0x08000000 - (int64_t)sumIm * s - (int64_t)diffRe * c;
		second = (int64_t)sumRe * 0x08000000 + (int64_t)sumIm * s + (int64_t)diffRe * c;
		x[0] = (int32_t)(first >> 28);
		y[0] = (int32_t)(second >> 28);

		first = (int64_t)diffIm * 0x08000000 - (int64_t)diffRe * s + (int64_t)sumIm * c;
		second = (int64_t)diffIm * 0x08000000 + (int64_t)diffRe * s - (int64_t)sumIm * c;
		x[1] = (int32_t)(first >> 28);
		y[1] = (int32_t)(-second >> 28);
	}

	aacFft(work, half);

	for (k = 0; k < half; k++) {
		data[2 * k] = work[k];
		data[2 * k + 1] = work[length - 1 - k];
	}
}
