/*
 * The VLIW kernels' arithmetic in C: what the IVC2 coprocessor's saturating
 * operations (Toshiba IVC2 manual, MEPUM08H04) do to one lane.
 */
#pragma once

#include <stdint.h>

#define IVC2_MAX	0x7fffffff
#define IVC2_MIN	((int32_t)0x80000000)
#define IVC2_MAX_WIDE	0x7fffffffffffffffLL
#define IVC2_MIN_WIDE	((int64_t)0x8000000000000000ULL)

/* cpsadd3.w */
static inline int32_t ivc2AddSaturate(int32_t a, int32_t b)
{
	int64_t sum = (int64_t)a + b;

	return sum > IVC2_MAX ? IVC2_MAX : sum < IVC2_MIN ? IVC2_MIN : (int32_t)sum;
}

/* cpssub3.w */
static inline int32_t ivc2SubtractSaturate(int32_t a, int32_t b)
{
	int64_t difference = (int64_t)a - b;

	return difference > IVC2_MAX ? IVC2_MAX : difference < IVC2_MIN ? IVC2_MIN :
		(int32_t)difference;
}

/* cpclipi3.w: a signed clip to `bits` bits */
static inline int32_t ivc2Clip(int32_t value, int32_t bits)
{
	int32_t high = (1 << (bits - 1)) - 1;

	return value > high ? high : value < -high - 1 ? -high - 1 : value;
}

/* cpmulsl*.w: the product doubled, saturating only for -1 * -1 */
static inline int64_t ivc2Product(int32_t a, int32_t b)
{
	if (a == IVC2_MIN && b == IVC2_MIN)
		return IVC2_MAX_WIDE;
	return (int64_t)a * b * 2;
}

/* cpsmadsl*.w: a 64-bit sum saturating */
static inline int64_t ivc2AddWide(int64_t a, int64_t b)
{
	int64_t sum = (int64_t)((uint64_t)a + (uint64_t)b);

	if (a >= 0 && b >= 0 && sum < 0)
		return IVC2_MAX_WIDE;
	if (a < 0 && b < 0 && sum >= 0)
		return IVC2_MIN_WIDE;
	return sum;
}

/* cpsmsbsl*.w: a 64-bit difference saturating */
static inline int64_t ivc2SubtractWide(int64_t a, int64_t b)
{
	int64_t difference = (int64_t)((uint64_t)a - (uint64_t)b);

	if (a >= 0 && b < 0 && difference < 0)
		return IVC2_MAX_WIDE;
	if (a < 0 && b >= 0 && difference >= 0)
		return IVC2_MIN_WIDE;
	return difference;
}

/* cpmovh*a1.w: the high word, a Q31 product */
static inline int32_t ivc2High(int64_t value)
{
	return (int32_t)(value >> 32);
}

/* a Q31 product, cpmulsl*.w then cpmovh*a1.w */
static inline int32_t ivc2Multiply(int32_t a, int32_t b)
{
	return ivc2High(ivc2Product(a, b));
}
