/*
 * The SBR and PS kernels the original runs on the IVC2 coprocessor,
 * in C with its arithmetic: saturating shifts, Q27 and Q31 products.
 * The QMF matrix is rows (slots) of 64 real and 64 imaginary values.
 */
#include "aac.h"

/* the bits `count` values could be shifted left by, keeping 4 bits spare (0x8c3e58) */
int32_t aacSbrBufferHeadroom(const int32_t *values, int32_t count)
{
	int32_t maximum = 0, i;

	for (i = 0; i < count; i++)
		maximum = aacMax(maximum, aacAbs(values[i]));
	return aacSignBits32(maximum) - 4;
}

/* values shifted by `shift`: left saturating within 28 bits, or right (0x8c3ee8) */
void aacSbrScale(int32_t *values, int32_t count, int32_t shift)
{
	int32_t i;

	if (shift >= 1) {
		shift = aacMin(shift + 4, 31);
		for (i = 0; i < count; i++)
			values[i] = aacShiftSaturate(values[i], shift) >> 4;
	} else {
		shift = aacMin(-shift, 31);
		for (i = 0; i < count; i++)
			values[i] >>= shift;
	}
}

/* the headroom of bands [low, high) of rows [first, last) (0x8b85c0) */
int32_t aacSbrHeadroom(int32_t **real, int32_t **imag, int32_t low, int32_t high,
	int32_t first, int32_t last)
{
	int32_t maximum = 0, row, band;

	for (row = first; row < last; row++) {
		for (band = low; band < high; band++) {
			maximum = aacMax(maximum, aacAbs(real[row][band]));
			maximum = aacMax(maximum, aacAbs(imag[row][band]));
		}
	}
	return aacSignBits32(maximum) - 4;
}

/* 0x8b8708 */
int32_t aacSbrHeadroomReal(int32_t **real, int32_t low, int32_t high, int32_t first,
	int32_t last)
{
	int32_t maximum = 0, row, band;

	for (row = first; row < last; row++)
		for (band = low; band < high; band++)
			maximum = aacMax(maximum, aacAbs(real[row][band]));
	return aacSignBits32(maximum) - 4;
}

static int32_t shift(int32_t value, int32_t count)
{
	if (count >= 1)
		return aacShiftSaturate(value, count);
	return value >> (aacAbs(count) & 31);
}

/* bands [low, high) of rows [first, last) shifted (0x8b81a0) */
void aacSbrRescale(int32_t **real, int32_t **imag, int32_t low, int32_t high, int32_t first,
	int32_t last, int32_t count)
{
	int32_t row, band;

	if (!count || !(low < high))
		return;
	for (row = first; row < last; row++) {
		for (band = low; band < high; band++) {
			real[row][band] = shift(real[row][band], count);
			imag[row][band] = shift(imag[row][band], count);
		}
	}
}

/* 0x8b8438 */
void aacSbrRescaleReal(int32_t **real, int32_t low, int32_t high, int32_t first,
	int32_t last, int32_t count)
{
	int32_t row, band;

	if (!count)
		return;
	for (row = first; row < last; row++)
		for (band = low; band < high; band++)
			real[row][band] = shift(real[row][band], count);
}

/*
 * 0x8bdb48: x with start * x^count closest to stop, by bisection from 1/2
 * (Q27), halving the step whenever the direction turns, at most 100 steps.
 */
static int32_t root(int32_t stop, int32_t start, int32_t count)
{
	int32_t target = stop << 20, x = 0x04000000, step = 0x02000000;
	int32_t rising = 1, steps = 0, i;

	do {
		int32_t product = start << 20;

		steps++;
		for (i = 0; i < count; i++)
			product = (int32_t)(((int64_t)product * x) >> 27);
		if (product < target) {
			if (!rising)
				step >>= 1;
			x += step;
			rising = 1;
		} else {
			if (rising == 1)
				step >>= 1;
			x -= step;
			rising = 0;
		}
	} while (steps < 101 && step >= 1);
	return x;
}

/*
 * 0x8bdca0: count geometric bands from start to stop, as the differences
 * of the rounded borders (start - next, stored from the last entry down)
 */
void aacSbrCalcBands(uint8_t *diff, int32_t start, int32_t stop, int32_t count)
{
	int32_t ratio = root(stop, start, count);
	int32_t border = start << 20, previous = start, rounded;
	uint8_t *out;

	for (out = diff + count - 1; out >= diff; out--) {
		border = (int32_t)(((int64_t)border * ratio) >> 27);
		rounded = (border + 0x7f000) >> 20;
		*out = previous - rounded;
		previous = rounded;
	}
}

/* 1 / d for d's three bits below the leading one, Q26 (0x924700) */
static const int32_t divideSeed[8] = {
	119304192, 107372544, 97611776, 89477120, 82591744, 76693504, 71581696, 67108864,
};

static int32_t mul26(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)a * b) >> 26);
}

/*
 * 0x8bd9d0: a / b by three Goldschmidt steps from the seed table, both
 * normalised by b's leading bit; negated when a ^ b has any of bits 27-31
 * set (the sign, for operands below 2^27)
 */
int32_t aacSbrQuotient(int32_t a, int32_t b)
{
	int32_t magnitude = aacAbs(b), norm = aacSignBits32(magnitude) - 4;
	int32_t d = (int32_t)((uint32_t)magnitude << (norm & 31));
	int32_t n = (int32_t)((uint32_t)aacAbs(a) << (norm & 31));
	int32_t seed = divideSeed[(d >> 23) & 7];
	int32_t factor, step;

	n = mul26(n, seed);
	d = mul26(d, seed);
	for (step = 0; step < 3; step++) {
		factor = 0x07ffffff - (d >> 1);
		n = mul26(n, factor);
		d = mul26(d, factor);
	}
	return (a ^ b) >> 27 ? -n : n;
}
