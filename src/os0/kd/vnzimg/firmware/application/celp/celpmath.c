/*
 * The fixed-point math the CELP codecs share (0x86d830-0x86e1cc, 0x875360):
 * logarithms, powers, square roots and the circular functions on Q16
 * values (65536 is 1.0; angles are radians). The results of out-of-range
 * arguments are the error values 0xdeadbeef and 0xdeadbeaf.
 *
 * A "series" is a Taylor or atanh series evaluated with 64-bit products
 * that keep the high word, in the order the original evaluates them.
 */
#include "celp.h"

#define CELP_MATH_ERROR			((int32_t)0xdeadbeef)
#define CELP_MATH_POWER_ERROR		((int32_t)0xdeadbeaf)

#define ONE				0x10000
#define PI				0x3243f
#define HALF_PI				0x1921f
#define TWO_PI_LIMIT			0x6487f
#define TWO_PI_STEP			0x6487e

static const uint8_t invSqrtSeeds[32] = {
	254, 246, 239, 231, 225, 218, 212, 206, 200, 195, 189, 184, 179, 174, 170, 165,
	161, 156, 152, 148, 144, 141, 137, 133, 130, 127, 123, 120, 117, 114, 111, 108,
};

static int32_t absolute(int32_t value)
{
	return value < 0 ? -value : value;
}

/* the high word of a 32-bit value times a sign-extended 32-bit factor */
static uint32_t highProduct(uint32_t a, int32_t b)
{
	return (uint32_t)(((uint64_t)a * (uint64_t)(int64_t)b) >> 32);
}

/* 0x86d830: the base 10 logarithm's atanh series of x (Q16) */
static uint32_t log10Series(uint32_t x)
{
	uint32_t square = x * x;
	uint32_t a = (uint32_t)(((uint64_t)square * 11783) >> 32) + 18969;
	uint32_t b = highProduct(square, (int32_t)a);

	return ((b + 0xde5c) * x) >> 16;
}

/* 0x86d8ae: the natural logarithm's */
static uint32_t lnSeries(uint32_t x)
{
	uint32_t square = x * x;
	uint32_t a = (uint32_t)(((uint64_t)square * 27130) >> 32) + 0xaa9e;
	uint32_t b = highProduct(square, (int32_t)a);

	return ((b + 0x20000) * x) >> 16;
}

/*
 * The mantissa of x (positive): `exponent` is its leading zero bits less 1,
 * and the mantissa lies in [sqrt(.5), sqrt(2)) * 2^30.
 */
static uint32_t normalize(uint32_t x, int32_t *exponent)
{
	int32_t shift = -1;
	uint32_t mantissa;

	do {
		shift++;
		x <<= 1;
	} while (!(x & 0x80000000));
	mantissa = x >> 1;
	if (mantissa >= 0x5a827999) {
		shift--;
		mantissa >>= 1;
	}
	*exponent = shift;
	return mantissa;
}

/* |m - 2^30| / (m + 2^30) in Q32, and the sign of the numerator */
static uint32_t atanhArgument(uint32_t mantissa, int32_t *negative)
{
	int32_t numerator = (int32_t)(mantissa - 0x40000000);
	uint32_t denominator = mantissa + 0x40000000;

	*negative = numerator < 0;
	return (uint32_t)(((uint64_t)absolute(numerator) << 32) / denominator);
}

/* 0x86d930: log10(x) */
int32_t celpLog10(int32_t x)
{
	int32_t exponent, negative;
	uint32_t mantissa, series;
	int32_t scale;

	if (x < 1)
		return x != 0 ? CELP_MATH_ERROR : (int32_t)0x80000000;
	mantissa = normalize(x, &exponent);
	series = atanhArgument(mantissa, &negative) >> 16;
	scale = (int32_t)(((int64_t)(30 - exponent) * 0x4d1054d2) >> 16);
	series = log10Series(series);
	return negative ? scale - series : series + scale;
}

/* 0x86da14: ln(x) */
int32_t celpLn(int32_t x)
{
	int32_t exponent, negative;
	uint32_t mantissa, series;
	int32_t scale;

	if (x < 1)
		return x != 0 ? CELP_MATH_ERROR : (int32_t)0x80000000;
	mantissa = normalize(x, &exponent);
	series = atanhArgument(mantissa, &negative) >> 16;
	scale = (int32_t)(((int64_t)(30 - exponent) * 0x58b90bfc) >> 15);
	series = lnSeries(series);
	return negative ? scale - series : series + scale;
}

/* 0x875360: e^x */
int32_t celpExp(int32_t x)
{
	int32_t magnitude = absolute(x);
	int32_t scaled, fraction, whole, correction;
	uint32_t square, a, b;
	int64_t numerator, divisor, quotient;

	if (magnitude >= 0xa65af)
		return x >= 0 ? 0x7fffffff : 0;
	scaled = (int32_t)(((int64_t)magnitude * 0x17154) >> 16);
	fraction = scaled & 0xffff;
	whole = scaled >> 16;
	if (fraction >= 0x8001) {
		whole++;
		fraction -= ONE;
	}
	if (x < 0) {
		fraction = -fraction;
		whole = -whole;
	}
	square = (uint32_t)(fraction * fraction);
	correction = (fraction * 22713) >> 16;
	divisor = 0x60000 - correction;
	numerator = (int64_t)fraction * 0x10a2b + (int32_t)(uint32_t)(((uint64_t)square * 1967) >> 16);
	quotient = numerator / divisor;
	a = (uint32_t)quotient + ONE;
	b = (uint32_t)(((int64_t)((uint64_t)a * a)) >> 16);
	b = (uint32_t)(((int64_t)((uint64_t)b * b)) >> 1);
	if (whole < 16)
		return b >> (15 - whole);
	return b << (whole - 15);
}

/* 0x86daf8: x^y, both Q16 */
int32_t celpPow(int32_t x, int32_t y)
{
	int32_t result = ONE;
	int32_t magnitude;

	if (y == 0)
		return result;
	result = 0;
	if (x == 0)
		return result;
	magnitude = absolute(x);
	result = magnitude;
	if (magnitude != ONE) {
		int32_t logarithm = celpLn(magnitude) - 0xb1721;

		result = celpExp((int32_t)(((int64_t)y * logarithm) >> 16));
	}
	if (x < 0) {
		if (y & 0xffff)
			return CELP_MATH_POWER_ERROR;
		if (y & ONE)
			result = -result;
	}
	return result;
}

/* 0x86db80: the reciprocal square root of x, two Newton steps from a table */
uint32_t celpInvSqrt(int32_t x)
{
	uint32_t m = x;
	uint32_t shift = 0, seed, y, hi;
	uint64_t product, step;

	if (x < 1)
		return CELP_MATH_ERROR;
	if (!(m & 0xc0000000)) {
		do {
			shift++;
			m <<= 2;
		} while (!(m & 0xc0000000));
	}
	seed = invSqrtSeeds[(m + 0xc0000000) >> 25] + 256;
	y = (uint32_t)((seed << 7) * (0x18000 - (((seed * seed) * (m >> 17)) >> 16)));
	hi = (uint32_t)(((uint64_t)y * y) >> 32);
	product = (uint64_t)m * hi;
	step = 0xc0000000ULL - (product >> 32);
	return (uint32_t)(((uint64_t)y * step) >> 32) >> (15 - shift);
}

/* 0x86dc90: sqrt(x) */
int32_t celpSqrt(int32_t x)
{
	if (x < 1)
		return x != 0 ? CELP_MATH_ERROR : 0;
	return (int32_t)((uint64_t)((int64_t)(int32_t)celpInvSqrt(x) * x) >> 15);
}

/* 0x86dcdc: sin(t) for small t, from its Taylor series */
static int32_t sinSeries(int32_t t)
{
	uint32_t square = (uint32_t)t * (uint32_t)t;
	uint32_t a = highProduct(square, -12);
	uint32_t b = highProduct(square, (int32_t)(a + 546));
	uint32_t c = highProduct(square, (int32_t)(b - 10922));

	return (int32_t)(((c + ONE) * (uint32_t)t) >> 16);
}

/* 0x86dd92: cos(t) for small t */
static int32_t cosSeries(int32_t t)
{
	uint32_t square = (uint32_t)t * (uint32_t)t;
	uint32_t a = highProduct(square, -89);
	uint32_t b = highProduct(square, (int32_t)(a + 2730));
	uint32_t c = highProduct(square, (int32_t)(b - 32767));

	return c + ONE;
}

/* the angle's remainder after whole turns (0x86defe, 0x86dfa8) */
static int32_t reduceAngle(int32_t t)
{
	int32_t magnitude = absolute(t);

	if (magnitude >= TWO_PI_LIMIT) {
		int32_t turns = (int32_t)(((int64_t)magnitude * 10430) >> 32);

		magnitude -= turns * TWO_PI_STEP;
	}
	return magnitude;
}

/* 0x86defe: sin(t) */
int32_t celpSin(int32_t t)
{
	int32_t angle = reduceAngle(t);
	int32_t negative = t < 0;
	int32_t result;

	if (angle >= 0x32440) {
		negative = !negative;
		angle -= PI;
	}
	if (angle >= 0x19220)
		angle = PI - angle;
	if (angle < 21413)
		result = sinSeries(angle);
	else
		result = cosSeries(HALF_PI - angle);
	return negative ? -result : result;
}

/* 0x86dfa8: cos(t) */
int32_t celpCos(int32_t t)
{
	int32_t angle = reduceAngle(t);
	int32_t negative = 0;
	int32_t result;

	if (angle >= 0x32440) {
		negative = 1;
		angle -= PI;
	}
	if (angle >= 0x19220) {
		negative = !negative;
		angle = PI - angle;
	}
	if (angle < 0xa74a)
		result = cosSeries(angle);
	else
		result = sinSeries(HALF_PI - angle);
	return negative ? -result : result;
}
