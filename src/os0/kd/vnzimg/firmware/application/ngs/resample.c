/*
 * Resampling (0x8adec8): each output is a cubic through the four samples
 * x0..x3 around its position, centred between x1 and x2 at u = fraction -
 * 1/2: y = E0 + u E1 + u^2 E2, with E0 and E2 from the pair sums x1 + x2 and
 * x0 + x3, E1 from the differences x2 - x1 and x3 - x0. Positions are
 * 17.15 fixed point; the IVC2 makes two outputs at a time, so an odd count
 * writes and consumes one more. Products are Q31 (cpmulsl, high words).
 */
#include "ngs.h"

#define FRACTION_BITS	15
#define FRACTION_MASK	((1 << FRACTION_BITS) - 1)
#define HALF		0x40000000	/* 1/2, Q31 */
#define OUTPUT_SHIFT	9		/* 16-bit samples to the NGS's */
#define STEREO_BUFFER	2116		/* one channel of the player's input */

/* Q31 */
#define EVEN_INNER	0x36303680	/* E0: x1 + x2 */
#define EVEN_OUTER	0x09d0e380	/* E0: x0 + x3 */
#define ODD_INNER	0x2170fb80	/* E1: x2 - x1 */
#define ODD_OUTER	0x1fb78b00	/* E1: x3 - x0 */
#define CURVE_INNER	((int32_t)0xe4ae0180)	/* E2: x1 + x2 */
#define CURVE_OUTER	0x1b44c280	/* E2: x0 + x3 */

/* a p + b q, Q31 */
static int32_t dot(int32_t a, int32_t p, int32_t b, int32_t q)
{
	return (int32_t)(((int64_t)a * p + (int64_t)b * q) >> 31);
}

static int32_t interpolate(const int16_t *x, uint32_t position)
{
	int32_t u = (int32_t)((position & FRACTION_MASK) << 16) - HALF;
	int32_t inner = x[1] + x[2], outer = x[0] + x[3];
	int32_t even = dot(inner, EVEN_INNER, outer, EVEN_OUTER);
	int32_t odd = dot(x[2] - x[1], ODD_INNER, x[3] - x[0], ODD_OUTER);
	int32_t curve = dot(inner, CURVE_INNER, outer, CURVE_OUTER);
	int32_t square = dot(u, u, 0, 0);

	return dot(curve, square, odd, u) + even;
}

int32_t ngsAdpcmResample(uint32_t *fraction, int32_t *output, const int16_t *input,
	int32_t samples, int32_t rate)
{
	uint32_t position = *fraction;
	int32_t pairs = samples > 1 ? (samples + 1) >> 1 : 1, i;

	for (i = 0; i < 2 * pairs; i++, position += rate)
		output[i] = interpolate(input + (position >> FRACTION_BITS), position) <<
			OUTPUT_SHIFT;
	*fraction = position & FRACTION_MASK;
	return (int32_t)position >> FRACTION_BITS;
}

/* 0x8ae2e0: one channel of mono or interleaved stereo 16-bit input */
int32_t ngsPlayerResample(uint32_t *fraction, int32_t *output, const int16_t *input,
	int32_t inputSamples, int32_t samples, int32_t rate, int32_t channels,
	int32_t channel)
{
	int16_t buffer[STEREO_BUFFER] __attribute__((aligned(8)));
	int32_t i, k;

	if (channels == 1)
		return ngsAdpcmResample(fraction, output, input, samples, rate);
	if (channels != 2)
		return 0;
	/* eight frames at a time */
	for (i = 0; i < inputSamples + 7; i += 8)
		for (k = i; k < i + 8; k++)
			buffer[k] = input[2 * k + (channel != 0)];
	return ngsAdpcmResample(fraction, output, buffer, samples, rate);
}
