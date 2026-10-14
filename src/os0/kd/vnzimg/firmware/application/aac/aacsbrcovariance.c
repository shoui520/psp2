/*
 * The SBR HF generator's LPC covariance (0x8bddf0-0x8bf4a8): each sum
 * normalised by its own sign bits, then all brought to the smallest
 * scale. The kernels' accumulators saturate at 64 bits, which sums of
 * the generator's headroom-scaled samples do not reach.
 *
 * x(0) is the band's first slot; the two before it are the LPC history.
 * The sums run over count slots; past a multiple of four the kernels
 * take two more, as a count of 4n + 2 needs.
 */
#include "aac.h"
#include "aacsbr.h"

/* 0x8c38c0: the 64-bit value's redundant sign bits, less 9 */
int32_t aacSbrNorm64(int64_t value)
{
	int32_t high = (int32_t)(value >> 32), low = (int32_t)value;

	if ((high == 0 && !(low & 0x80000000)) || (high == -1 && (low & 0x80000000)))
		return aacSignBits32(low) + 23;
	return aacSignBits32(high) - 9;
}

/* the sum shifted by its own norm, Q27 */
static int32_t normalise(int64_t sum, int32_t *shift)
{
	*shift = aacSbrNorm64(sum);
	if (*shift >= 1)
		sum = (int64_t)((uint64_t)sum << (*shift & 63));
	else
		sum >>= -*shift & 63;
	return (int32_t)(sum >> 27);
}

static int64_t product(int32_t a, int32_t b)
{
	return (int64_t)a * b;
}

/* the normalised sums at the smallest scale, and the determinant */
static void finish(AacSbrCovariance *out, const int64_t *sum, int32_t count)
{
	int32_t value[8], shift[8];
	int32_t common, i;

	for (i = 0; i < count; i++)
		value[i] = normalise(sum[i], &shift[i]);
	common = shift[0];
	for (i = 1; i < count; i++)
		common = aacMin(common, shift[i]);
	for (i = 0; i < count; i++)
		value[i] >>= (shift[i] - common) & 31;

	out->phi11 = value[0];
	out->phi22 = value[1];
	out->phi01Real = value[2];
	out->phi02Real = value[3];
	out->phi12Real = value[4];
	if (count == 8) {
		out->phi01Imag = value[5];
		out->phi02Imag = value[6];
		out->phi12Imag = value[7];
		out->determinant = (int32_t)((product(out->phi11, out->phi22) -
			(product(out->phi12Imag, out->phi12Imag) +
			product(out->phi12Real, out->phi12Real))) >> 27);
	} else {
		out->determinant = (int32_t)((product(out->phi11, out->phi22) -
			product(out->phi12Real, out->phi12Real)) >> 27);
	}
}

/* x(n) x*(n + lag), from the history's start */
static int64_t lagReal(const int32_t *a, const int32_t *b, int32_t n, int32_t lag)
{
	return product(a[n], a[n + lag]) + product(b[n], b[n + lag]);
}

static int64_t lagImag(const int32_t *a, const int32_t *b, int32_t n, int32_t lag)
{
	return product(a[n], b[n + lag]) - product(a[n + lag], b[n]);
}

/* 0x8bddf0: a complex band */
void aacSbrCovariance(AacSbrCovariance *out, const int32_t *real, const int32_t *imag,
	int32_t count)
{
	const int32_t *a = real - 2, *b = imag - 2;
	int32_t pairs = 2 * aacMax(count >> 1, 1);
	int32_t groups = 4 * aacMax(count >> 2, 1);
	int64_t sum[8] = { 0 };
	int32_t n;

	/* phi22 and the phi12s run from the history's start, phi11 and phi01 a slot later */
	for (n = 0; n < pairs; n++)
		sum[1] += lagReal(a, b, n, 0);
	sum[0] = sum[1] - lagReal(a, b, 0, 0) + lagReal(a, b, pairs, 0);
	for (n = 0; n < groups; n++) {
		sum[4] += lagReal(a, b, n, 1);
		sum[7] += lagImag(a, b, n, 1);
		sum[3] += lagReal(a, b, n, 2);
		sum[6] += lagImag(a, b, n, 2);
	}
	sum[2] = sum[4] - lagReal(a, b, 0, 1) + lagReal(a, b, groups, 1);
	sum[5] = sum[7] - lagImag(a, b, 0, 1) + lagImag(a, b, groups, 1);
	if (count & 3) {
		sum[4] += lagReal(a, b, groups, 1) + lagReal(a, b, groups + 1, 1);
		sum[2] += lagReal(a, b, groups + 1, 1) + lagReal(a, b, groups + 2, 1);
		sum[7] += lagImag(a, b, groups, 1) + lagImag(a, b, groups + 1, 1);
		sum[5] += lagImag(a, b, groups + 1, 1) + lagImag(a, b, groups + 2, 1);
		sum[3] += lagReal(a, b, groups, 2) + lagReal(a, b, groups + 1, 2);
		sum[6] += lagImag(a, b, groups, 2) + lagImag(a, b, groups + 1, 2);
	}
	finish(out, sum, 8);
}

/* 0x8bec00: a real band (the low-power decoder's) */
void aacSbrCovarianceReal(AacSbrCovariance *out, const int32_t *real, int32_t count)
{
	const int32_t *a = real - 2;
	int32_t groups = 4 * (count >> 2);
	int64_t sum[5] = { 0 };
	int32_t n;

	for (n = 0; n < groups; n++) {
		sum[1] += product(a[n], a[n]);
		sum[4] += product(a[n], a[n + 1]);
		sum[3] += product(a[n], a[n + 2]);
	}
	sum[0] = sum[1] - product(a[0], a[0]) + product(a[groups], a[groups]);
	sum[2] = sum[4] - product(a[0], a[1]) + product(a[groups], a[groups + 1]);
	if (count & 3) {
		sum[1] += product(a[groups], a[groups]) + product(a[groups + 1], a[groups + 1]);
		sum[0] += product(a[groups + 1], a[groups + 1]) +
			product(a[groups + 2], a[groups + 2]);
		sum[4] += product(a[groups], a[groups + 1]) +
			product(a[groups + 1], a[groups + 2]);
		sum[2] += product(a[groups + 1], a[groups + 2]) +
			product(a[groups + 2], a[groups + 3]);
		sum[3] += product(a[groups], a[groups + 2]) +
			product(a[groups + 1], a[groups + 3]);
	}
	finish(out, sum, 5);
}
