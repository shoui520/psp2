/*
 * NGS fixed-point helpers: values are signed 8.24 (1.0 = 1 << 24), levels
 * in decibels too.
 */
#include "ngs.h"

#define SILENCE_DECIBELS	(-100 * NGS_ONE)
/* -100 dB, as a level */
#define SILENCE_LEVEL		168
/* the Taylor coefficients 1/n! in 8.24, truncated (cos's by Sony's rounding) */
#define SIN_3			0x2aaaaa
#define SIN_5			0x22222
#define SIN_7			3328
#define SIN_9			46
#define COS_2			0x7fffff
#define COS_4			0xaaaaa
#define COS_6			23300
#define COS_8			415
#define COS_10			4
/* the RMS level's scale, 1.29 */
#define RMS_SCALE		0x14a5296
/* log2 10 in 8.24 */
#define LOG2_10			0x35269e1

const int32_t ngsFadeStart = NGS_WINDOW_SIZE / 2;

/*
 * A window in 8.24, 0.5 - 0.46 cos(2 pi n / 255): it rises from 0.04 to 0.96
 * and falls back; fades take its falling half.
 */
const int32_t ngsWindow[NGS_WINDOW_SIZE] = {
	0x0a3d71, 0x0a4697, 0x0a620a, 0x0a8fc4, 0x0acfbe, 0x0b21f0,
	0x0b864a, 0x0bfcbf, 0x0c853c, 0x0d1fac, 0x0dcbf5, 0x0e89ff,
	0x0f59ab, 0x103ad9, 0x112d65, 0x12312c, 0x134602, 0x146bbf,
	0x15a234, 0x16e931, 0x184083, 0x19a7f5, 0x1b1f4f, 0x1ca656,
	0x1e3ccf, 0x1fe279, 0x219712, 0x235a59, 0x252c05, 0x270bcf,
	0x28f96c, 0x2af490, 0x2cfceb, 0x2f122e, 0x313404, 0x336219,
	0x359c16, 0x37e1a3, 0x3a3266, 0x3c8e01, 0x3ef419, 0x41644c,
	0x43de3a, 0x466180, 0x48edbb, 0x4b8286, 0x4e1f78, 0x50c42b,
	0x537035, 0x56232d, 0x58dca5, 0x5b9c33, 0x5e6169, 0x612bd9,
	0x63fb13, 0x66cea7, 0x69a626, 0x6c811f, 0x6f5f1f, 0x723fb4,
	0x75226d, 0x7806d6, 0x7aec7c, 0x7dd2ed, 0x80b9b4, 0x83a05d,
	0x868677, 0x896b8d, 0x8c4f2c, 0x8f30e2, 0x92103b, 0x94ecc5,
	0x97c60f, 0x9a9ba7, 0x9d6d1d, 0xa039ff, 0xa301e0, 0xa5c44f,
	0xa880e0, 0xab3726, 0xade6b4, 0xb08f20, 0xb33000, 0xb5c8eb,
	0xb8597a, 0xbae148, 0xbd5fee, 0xbfd50b, 0xc2403c, 0xc4a122,
	0xc6f75c, 0xc94290, 0xcb8260, 0xcdb674, 0xcfde75, 0xd1fa0b,
	0xd408e4, 0xd60aae, 0xd7ff18, 0xd9e5d5, 0xdbbe99, 0xdd891c,
	0xdf4514, 0xe0f23f, 0xe29058, 0xe41f1f, 0xe59e57, 0xe70dc4,
	0xe86d2d, 0xe9bc5b, 0xeafb1a, 0xec2939, 0xed4689, 0xee52dc,
	0xef4e0a, 0xf037ec, 0xf1105d, 0xf1d73c, 0xf28c69, 0xf32fc9,
	0xf3c142, 0xf440bd, 0xf4ae27, 0xf5096f, 0xf55286, 0xf58962,
	0xf5adf9, 0xf5c046, 0xf5c046, 0xf5adf9, 0xf58962, 0xf55286,
	0xf5096f, 0xf4ae27, 0xf440bd, 0xf3c142, 0xf32fc9, 0xf28c69,
	0xf1d73c, 0xf1105d, 0xf037ec, 0xef4e0a, 0xee52dc, 0xed4689,
	0xec2939, 0xeafb1a, 0xe9bc5b, 0xe86d2d, 0xe70dc4, 0xe59e57,
	0xe41f1f, 0xe29058, 0xe0f23f, 0xdf4514, 0xdd891c, 0xdbbe99,
	0xd9e5d5, 0xd7ff18, 0xd60aae, 0xd408e4, 0xd1fa0b, 0xcfde75,
	0xcdb674, 0xcb8260, 0xc94290, 0xc6f75c, 0xc4a122, 0xc2403c,
	0xbfd50b, 0xbd5fee, 0xbae148, 0xb8597a, 0xb5c8eb, 0xb33000,
	0xb08f20, 0xade6b4, 0xab3726, 0xa880e0, 0xa5c44f, 0xa301e0,
	0xa039ff, 0x9d6d1d, 0x9a9ba7, 0x97c60f, 0x94ecc5, 0x92103b,
	0x8f30e2, 0x8c4f2c, 0x896b8d, 0x868677, 0x83a05d, 0x80b9b4,
	0x7dd2ed, 0x7aec7c, 0x7806d6, 0x75226d, 0x723fb4, 0x6f5f1f,
	0x6c811f, 0x69a626, 0x66cea7, 0x63fb13, 0x612bd9, 0x5e6169,
	0x5b9c33, 0x58dca5, 0x56232d, 0x537035, 0x50c42b, 0x4e1f78,
	0x4b8286, 0x48edbb, 0x466180, 0x43de3a, 0x41644c, 0x3ef419,
	0x3c8e01, 0x3a3266, 0x37e1a3, 0x359c16, 0x336219, 0x313404,
	0x2f122e, 0x2cfceb, 0x2af490, 0x28f96c, 0x270bcf, 0x252c05,
	0x235a59, 0x219712, 0x1fe279, 0x1e3ccf, 0x1ca656, 0x1b1f4f,
	0x19a7f5, 0x184083, 0x16e931, 0x15a234, 0x146bbf, 0x134602,
	0x12312c, 0x112d65, 0x103ad9, 0x0f59ab, 0x0e89ff, 0x0dcbf5,
	0x0d1fac, 0x0c853c, 0x0bfcbf, 0x0b864a, 0x0b21f0, 0x0acfbe,
	0x0a8fc4, 0x0a620a, 0x0a4697, 0x0a3d71,
};

/*
 * a / b in 8.24: the integer quotient, then the remainder divided by b with
 * 8 and 16 bits dropped, for two more bytes of fraction.
 */
int32_t ngsDivide(int32_t a, int32_t b)
{
	int32_t quotient, fraction, fraction2, remainder;

	quotient = a / b;
	remainder = a - b * quotient;
	b >>= 8;
	if (b == 0)
		return quotient << 24;
	fraction = remainder / b;
	remainder -= b * fraction;
	b >>= 8;
	if (b == 0)
		return (quotient << 24) + (fraction << 16);
	fraction2 = remainder / b;
	return (fraction2 << 8) + (quotient << 24) + (fraction << 16);
}

/* log2(x): the exponent, then a bit of fraction from each squaring */
int32_t ngsLog2(uint32_t x)
{
	uint32_t zeros = __builtin_clz(x);
	int32_t result = 7 - zeros;
	int32_t mantissa, bit, i;

	mantissa = (x << zeros) >> 7;
	for (i = 0; i < 24; i++) {
		mantissa = (int32_t)(((int64_t)mantissa * mantissa) >> 24);
		bit = mantissa >> 25;
		result = (result << 1) + bit;
		mantissa >>= bit;
	}
	return result;
}

/* 2^x: 2^frac by its series in frac * ln 2, then shifted by the integer */
int32_t ngsExp2(int32_t x)
{
	int32_t integer = (x + 0x7fffff) >> 24;
	int32_t fraction = x - (integer << 24);
	int32_t f1, f2, f3, f4, f5, f6, f7;
	uint32_t shift, right;
	int64_t sum;

	if (integer < -3) {
		right = -4 - integer;
		shift = 0;
	} else {
		right = 0;
		shift = integer + 4;
	}

	f1 = fraction << 4;				/* 4.28 */
	f2 = (int32_t)(((int64_t)f1 * f1) >> 32) << 4;	/* 4.28 */
	f4 = (int32_t)(((int64_t)f2 * f2) >> 32);	/* 8.24 */
	f3 = (int32_t)(((int64_t)f1 * f2) >> 32);	/* 8.24 */
	f4 <<= 4;					/* 4.28 */
	f5 = (int32_t)(((int64_t)f1 * f4) >> 32);	/* 8.24 */
	f6 = (int32_t)(((int64_t)f2 * f4) >> 32);	/* 8.24 */
	f7 = (int32_t)(((int64_t)f3 * f4) >> 32);	/* 8.24 */

	/* sum in 12.52: (ln 2)^n / n! in 8.24 or 4.28, against 4.28 or 8.24 */
	sum = (int64_t)1 << 52;
	sum += (int64_t)f1 * 0xb17217;
	sum += (int64_t)f2 * 0x3d7f7b;
	sum += (int64_t)f3 * 0xe35846;
	sum += (int64_t)f4 * 0x27655;
	sum += (int64_t)f5 * 0x5761f;
	sum += (int64_t)f6 * 0xa184;
	sum += (int64_t)f7 * 4094;

	return (int32_t)(((uint64_t)sum << shift) >> 32) >> right;
}

/* sqrt(x), a bit at a time, with 12 bits of fraction more than x */
uint32_t ngsSqrt(uint32_t x)
{
	uint32_t root = 0, remainder = 0, trial;
	int32_t i;

	for (i = 27; i >= 0; i--) {
		remainder = (remainder << 2) | (x >> 30);
		x <<= 2;
		root <<= 1;
		trial = (root << 1) + 1;
		if (remainder >= trial) {
			root++;
			remainder -= trial;
		}
	}
	return root;
}

/* 10^(dB / 20) */
int32_t ngsDecibelsToLevel(int32_t decibels)
{
	if (decibels < SILENCE_DECIBELS + 1)
		return SILENCE_LEVEL;
	return ngsPower(10 * NGS_ONE, decibels / 20);
}

/* 20 log10(level) */
int32_t ngsLevelToDecibels(int32_t level)
{
	if (level < SILENCE_LEVEL + 1)
		return SILENCE_DECIBELS;
	return ngsLog10(level) * 20;
}

/* 0x878778: a b in 8.24 (bytes 3-6 of the 64-bit product, by CPPERM) */
int32_t ngsMultiply(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)a * b) >> 24);
}

/* 0x8787c0: base^exponent, as 2^(exponent log2 base) */
int32_t ngsPower(int32_t base, int32_t exponent)
{
	return ngsExp2((int32_t)(((int64_t)exponent * ngsLog2(base)) >> 24));
}

/* 0x8788f8: log10 x, as log2 x / log2 10; 0 for x <= 0 */
int32_t ngsLog10(int32_t x)
{
	if (x <= 0)
		return 0;
	return ngsDivide(ngsLog2(x), LOG2_10);
}

/* 0x878838: whole 32-byte blocks */
void ngsClear(void *destination, uint32_t size)
{
	uint32_t *p = destination;
	int32_t i;

	for (i = 0; i < ((int32_t)size >> 5) * 8; i++)
		p[i] = 0;
}

/* 0x878890: whole 32-byte blocks */
void ngsCopy(void *destination, const void *source, uint32_t size)
{
	uint32_t *to = destination;
	const uint32_t *from = source;
	int32_t i;

	for (i = 0; i < ((int32_t)size >> 5) * 8; i++)
		to[i] = from[i];
}

/* 0x878970: sin x, Taylor to x^9 in 8.24 */
int32_t ngsSin(int32_t x)
{
	int32_t x2 = ngsMultiply(x, x), p;

	p = ngsMultiply(x2, SIN_9) - SIN_7;
	p = ngsMultiply(x2, p) + SIN_5;
	p = ngsMultiply(x2, p) - SIN_3;
	p = ngsMultiply(x2, p) + NGS_ONE;
	return ngsMultiply(x, p);
}

/* 0x878a68: both series at once (an IVC2 lane each); returns sin x */
int32_t ngsSinCos(int32_t x, int32_t *cosine)
{
	*cosine = ngsCos(x);
	return ngsSin(x);
}

/* 0x878ba0: cos x, Taylor to x^10 in 8.24 */
int32_t ngsCos(int32_t x)
{
	int32_t x2 = ngsMultiply(x, x), p;

	p = ngsMultiply(x2, -COS_10) + COS_8;
	p = ngsMultiply(x2, p) - COS_6;
	p = ngsMultiply(x2, p) + COS_4;
	p = ngsMultiply(x2, p) - COS_2;
	return ngsMultiply(x2, p) + NGS_ONE;
}

/* |x|; the most negative value stays itself (cpabsz) */
static int32_t magnitude(int32_t x)
{
	return x < 0 ? (int32_t)(0u - (uint32_t)x) : x;
}

/* 0x878ca0: the largest magnitude, eight samples at a time */
int32_t ngsPeakLevel(const int32_t *data, uint32_t count)
{
	int32_t peak = 0, i, m;

	for (i = 0; i < (int32_t)count; i++) {
		m = magnitude(data[i]);
		if (m > peak)
			peak = m;
	}
	return peak;
}

/*
 * 0x878d58: the RMS level, sqrt of the mean square (even and odd samples
 * summed apart in 64 bits), scaled by 1.29 (0x14a5296).
 */
int32_t ngsRmsLevel(const int32_t *data, uint32_t count)
{
	int64_t even = 0, odd = 0;
	int32_t i, mean;

	for (i = 0; i < (int32_t)count; i += 2) {
		even += (int64_t)data[i] * data[i];
		odd += (int64_t)data[i + 1] * data[i + 1];
	}
	mean = ngsDivide((int32_t)(even >> 24) + (int32_t)(odd >> 24), count << 24);
	return ngsMultiply(ngsSqrt(mean), RMS_SCALE);
}
