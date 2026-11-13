/*
 * The image's floating-point runtime (0x924f90-0x926c00): single precision
 * done in integers, Sony's (the toolchain's) own routines, not libgcc's. The
 * application calls them through the image's physical window (jsr 0x40924f90
 * and so on). The arithmetic ones carry libgcc's names, so that the code GCC
 * builds calls these; the comparisons return a truth value, the toolchain's
 * convention, not libgcc's.
 *
 * A NaN result is 0x7fffffff with the sign of the operation; an infinite one
 * 0x7f800000, likewise.
 */
#include "vnz.h"

#define SIGN		0x80000000
#define EXPONENT	0x7f800000
#define MANTISSA	0x7fffff
#define HIDDEN		0x800000
#define NAN_BITS	0x7fffffff
#define INFINITY_BITS	0x7f800000

typedef union FloatBits {
	float value;
	uint32_t bits;
} FloatBits;

static inline uint32_t bitsOf(float value)
{
	FloatBits f = { .value = value };

	return f.bits;
}

static inline float floatOf(uint32_t bits)
{
	FloatBits f = { .bits = bits };

	return f.value;
}

static inline int isNan(uint32_t bits)
{
	return (bits & EXPONENT) == EXPONENT && (bits & MANTISSA) != 0;
}

/*
 * 0x924f90: a * b. The mantissas, four bits up, are multiplied sixteen bits
 * by sixteen into 64 bits, normalized, and rounded to nearest, ties to even.
 */
float __mulsf3(float a, float b)
{
	uint32_t x = bitsOf(a), y = bitsOf(b);
	uint32_t sign = (x ^ y) >> 31;
	uint32_t mantissaA = x & MANTISSA, mantissaB = y & MANTISSA;
	int32_t exponentA = (x << 1) >> 24, exponentB = (y << 1) >> 24, exponent;
	uint32_t result = 0, low, high, carry, sticky, partial;
	uint32_t aLow, aHigh, bLow, bHigh, lowLow, highLow, lowHigh, highHigh;

	if (exponentA == 255) {
		if (mantissaA != 0)
			goto nan;
		if (exponentB == 255 && mantissaB != 0)
			goto nan;
		if (exponentB == 0 && mantissaB == 0)
			goto nan;
		goto infinity;
	}
	if (exponentA == 0) {
		if (mantissaA == 0) {
			if (exponentB != 255)
				goto done;
			goto nan;
		}
		exponentA++;
	} else {
		mantissaA |= HIDDEN;
	}
	if (exponentB == 255) {
		if (mantissaB == 0)
			goto infinity;
		goto nan;
	}
	if (exponentB == 0) {
		if (mantissaB == 0)
			goto done;
		exponentB++;
	} else {
		mantissaB |= HIDDEN;
	}

	exponent = exponentA + exponentB - 127;
	mantissaB <<= 4;
	mantissaA <<= 4;
	aLow = mantissaA & 0xffff;
	aHigh = mantissaA >> 16;
	bLow = mantissaB & 0xffff;
	bHigh = mantissaB >> 16;
	lowLow = highLow = lowHigh = highHigh = 0;
	while (bLow != 0 || bHigh != 0) {
		if (bLow & 1) {
			highLow += aHigh;
			lowLow += aLow;
		}
		if (bHigh & 1) {
			highHigh += aHigh;
			lowHigh += aLow;
		}
		aHigh <<= 1;
		aLow <<= 1;
		bHigh >>= 1;
		bLow >>= 1;
	}
	low = lowLow + (highLow << 16);
	carry = low < (highLow << 16);
	partial = lowHigh << 16;
	low += partial;
	if (low < partial)
		carry++;
	exponent++;
	high = highHigh + (highLow >> 16) + (lowHigh >> 16) + carry;

	/* normalized, as far as the exponent allows */
	while (!(high & HIDDEN)) {
		if (exponent < 2)
			break;
		high = (high << 1) | (low >> 31);
		low <<= 1;
		exponent--;
	}
	if (!(high & HIDDEN)) {
		while (exponent < 1) {
			exponent++;
			low = (high << 31) | (low >> 1);
			high >>= 1;
		}
		exponent = 0;
	}

	sticky = (low & 0x7fffffff) != 0;
	if (low >> 31) {
		if (sticky || (high & 1)) {
			high++;
			if (high >= 0x1000000) {
				high >>= 1;
				exponent++;
			} else if (high == 0xffffff && exponent == 0) {
				exponent++;
			}
		}
	}
	if (exponent >= 255)
		goto infinity;
	if (exponent >= 0)
		result = (exponent << 23) | (high & MANTISSA);
	goto done;

nan:
	result = NAN_BITS;
	goto done;
infinity:
	result = INFINITY_BITS;
done:
	if (sign)
		result |= SIGN;
	return floatOf(result);
}

/*
 * 0x925124: a / b. Four quotient bits a step by restoring division until 26
 * bits are found (24 and two more), then rounded: ties away from zero.
 */
float __divsf3(float a, float b)
{
	uint32_t x = bitsOf(a), y = bitsOf(b);
	uint32_t mantissaA = x & MANTISSA, mantissaB = y & MANTISSA;
	uint32_t fieldA = x & EXPONENT, fieldB = y & EXPONENT;
	uint32_t signA = x & SIGN, signB = y & SIGN;
	int aMaximum = fieldA == EXPONENT, bMaximum = fieldB == EXPONENT;
	uint32_t result, quotient, twice, four, eight, sticky, bits, shift, round;
	int32_t exponentA, exponentB, exponent, remaining;

	if (aMaximum && mantissaA)
		goto nan;
	if (bMaximum && mantissaB)
		goto nan;
	if (aMaximum && bMaximum)
		goto nan;
	if (fieldA == 0 && fieldB == 0 && mantissaA == 0 && mantissaB == 0)
		goto nan;
	if (aMaximum)
		goto infinity;
	if (fieldB == 0 && mantissaB == 0)
		goto infinity;
	if (fieldA == 0 && mantissaA == 0)
		goto zero;
	if (bMaximum)
		goto zero;

	exponentB = (int32_t)fieldB >> 23;
	exponentA = (int32_t)fieldA >> 23;
	if (exponentA != 0)
		mantissaA |= HIDDEN;
	else
		exponentA++;
	if (exponentB != 0)
		mantissaB |= HIDDEN;
	else
		exponentB++;
	while (!(mantissaB & HIDDEN)) {
		exponentB--;
		mantissaB <<= 1;
	}

	mantissaA <<= 3;
	remaining = 26;
	twice = mantissaB << 1;
	four = mantissaB << 2;
	eight = mantissaB << 3;
	exponent = exponentA - exponentB;
	bits = 0;
	quotient = 0;
	for (;;) {
		if (mantissaA >= eight) {
			quotient |= 8;
			mantissaA -= eight;
		}
		if (mantissaA >= four) {
			quotient |= 4;
			mantissaA -= four;
		}
		if (mantissaA >= twice) {
			quotient |= 2;
			mantissaA -= twice;
		}
		if (mantissaA >= mantissaB) {
			quotient |= 1;
			mantissaA -= mantissaB;
		}
		if (bits != 0) {
			remaining -= 4;
		} else {
			/* the first digits: the exponent takes the leading zeros */
			bits = quotient ? 32 - __builtin_clz(quotient) : 0;
			if (bits == 0) {
				exponent -= 4;
			} else {
				remaining -= bits;
				exponent -= 4 - bits;
			}
		}
		if (mantissaA == 0)
			break;
		if (remaining < 1)
			break;
		mantissaA <<= 4;
		quotient <<= 4;
	}
	if (remaining >= 0) {
		if (mantissaA == 0)
			quotient <<= remaining;
	} else {
		quotient >>= -remaining;
	}
	if (quotient == 0) {
		result = 0;
		goto sign;
	}

	sticky = quotient;
	exponent += 127;
	if (exponent < 1) {
		sticky = (quotient & 1) | mantissaA;
		shift = 1 - exponent;
		exponent = 0;
		if (shift >= 26)
			goto zero;
		sticky |= quotient << (32 - shift);
		quotient >>= shift;
		sticky |= quotient & 1;
		if (quotient == 0) {
			result = 0;
			goto sign;
		}
	}
	round = (quotient & 2) >> 1;
	quotient >>= 2;
	if (round && (sticky || (quotient & 1))) {
		quotient++;
		if (quotient == 0x1000000) {
			quotient >>= 1;
			exponent++;
		} else if (exponent == 0 && quotient == HIDDEN) {
			exponent++;
		}
	}
	if (exponent >= 255)
		goto infinity;
	result = (quotient & 0x807fffff) | (exponent << 23);
	goto sign;

nan:
	result = NAN_BITS;
	goto sign;
infinity:
	result = INFINITY_BITS;
	goto sign;
zero:
	result = 0;
sign:
	if (signA ^ signB)
		result |= SIGN;
	return floatOf(result);
}

/* 0x9252e0: a == b (zeros of either sign equal) */
int floatEqual(float a, float b)
{
	uint32_t x = bitsOf(a), y = bitsOf(b);

	if (isNan(x) || isNan(y))
		return 0;
	if (x == y)
		return 1;
	return ((x | y) ^ SIGN) == 0;
}

/* 0x925324: a != b */
int floatNotEqual(float a, float b)
{
	uint32_t x = bitsOf(a), y = bitsOf(b);

	if (isNan(x) || isNan(y))
		return 1;
	if (x == y)
		return 0;
	return ((x | y) ^ SIGN) != 0;
}

/*
 * The orderings: the signs first, then the exponent fields, then the
 * mantissas, read backwards when both are negative.
 */
static inline int floatOrder(uint32_t x, uint32_t y, int equal)
{
	uint32_t signX = x & SIGN, signY = y & SIGN;
	uint32_t fieldX = x & EXPONENT, fieldY = y & EXPONENT;
	int above, below;

	if (signX & signY) {
		above = 0;
		below = 1;
	} else {
		above = 1;
		below = 0;
	}
	if (fieldY < fieldX)
		return above;
	if (fieldX < fieldY)
		return below;
	if ((y & MANTISSA) < (x & MANTISSA))
		return above;
	if ((x & MANTISSA) < (y & MANTISSA))
		return below;
	return equal;
}

/* 0x92536a: a > b */
int floatGreater(float a, float b)
{
	uint32_t x = bitsOf(a), y = bitsOf(b);

	if (isNan(x) || isNan(y))
		return 0;
	if (x == y || (x | y) == SIGN)
		return 0;
	if ((x & SIGN) < (y & SIGN))
		return 1;
	if ((y & SIGN) < (x & SIGN))
		return 0;
	return floatOrder(x, y, 0);
}

/* 0x9253f0: a >= b */
int floatGreaterEqual(float a, float b)
{
	uint32_t x = bitsOf(a), y = bitsOf(b);

	if (isNan(x) || isNan(y))
		return 0;
	if (x == y || (x | y) == SIGN)
		return 1;
	if ((x & SIGN) < (y & SIGN))
		return 1;
	if ((y & SIGN) < (x & SIGN))
		return 0;
	return floatOrder(x, y, 1);
}

/* 0x925476: a < b */
int floatLess(float a, float b)
{
	uint32_t x = bitsOf(a), y = bitsOf(b);

	if (isNan(x) || isNan(y))
		return 0;
	if (x == y || (x | y) == SIGN)
		return 0;
	if ((x & SIGN) < (y & SIGN))
		return 0;
	if ((y & SIGN) < (x & SIGN))
		return 1;
	return floatOrder(y, x, 0);
}

/* 0x9254fc: a <= b */
int floatLessEqual(float a, float b)
{
	uint32_t x = bitsOf(a), y = bitsOf(b);

	if (isNan(x) || isNan(y))
		return 0;
	if (x == y || (x | y) == SIGN)
		return 1;
	if ((x & SIGN) < (y & SIGN))
		return 0;
	if ((y & SIGN) < (x & SIGN))
		return 1;
	return floatOrder(y, x, 1);
}

/* 0x925584: a == 0 */
int floatIsZero(float a)
{
	uint32_t x = bitsOf(a);

	if (isNan(x))
		return 0;
	return (x & 0x7fffffff) == 0;
}

/* 0x9255aa: a != 0 */
int floatIsNotZero(float a)
{
	uint32_t x = bitsOf(a);

	if (isNan(x))
		return 1;
	return (x & 0x7fffffff) != 0;
}

/* 0x9255d2: a > 0 */
int floatIsPositive(float a)
{
	uint32_t x = bitsOf(a);

	if (isNan(x) || (x & 0x7fffffff) == 0)
		return 0;
	return !(x >> 31);
}

/* 0x9255fe: a >= 0 */
int floatIsNotNegative(float a)
{
	uint32_t x = bitsOf(a);

	if (isNan(x))
		return 0;
	if ((x & 0x7fffffff) == 0)
		return 1;
	return !(x >> 31);
}

/* 0x92562c: a < 0 */
int floatIsNegative(float a)
{
	uint32_t x = bitsOf(a);

	if (isNan(x) || (x & 0x7fffffff) == 0)
		return 0;
	return x >> 31;
}

/* 0x925658: a <= 0 */
int floatIsNotPositive(float a)
{
	uint32_t x = bitsOf(a);

	if (isNan(x))
		return 0;
	if ((x & 0x7fffffff) == 0)
		return 1;
	return x >> 31;
}

/* 0x925688: a to an integer, toward zero; too large (or NaN) is 0x7fffffff */
int32_t __fixsfsi(float a)
{
	uint32_t x = bitsOf(a);
	uint32_t mantissa = (x & MANTISSA) | HIDDEN;
	int32_t shift = (int32_t)((x & EXPONENT) >> 23) - 150;
	uint32_t result;

	if (shift == 0) {
		result = mantissa;
	} else if (shift >= 1) {
		if (shift >= 8)
			result = 0x7fffffff;
		else
			result = (mantissa << shift) & 0x7fffffff;
	} else if (shift < -23) {
		result = 0;
	} else {
		result = (mantissa >> -shift) & 0x7fffffff;
	}
	if (x >> 31)
		result = -result;
	return result;
}

/*
 * 0x9256dc: an integer to a float, rounded to nearest, ties to even (the
 * rounding may carry into a 25th bit, which is shifted out again).
 */
float __floatsisf(int32_t a)
{
	uint32_t value = a, result, negative = 0, sticky, round;
	int32_t exponent;

	if (value >> 31) {
		negative = 1;
		value = -value;
	}
	if (value == 0) {
		result = 0;
		goto sign;
	}
	exponent = 23;
	if (value >= 0x1000000) {
		sticky = 0;
		round = 0;
		for (;;) {
			while (value & 0xff000000) {
				if (round)
					sticky = 1;
				exponent++;
				round = value & 1;
				value >>= 1;
			}
			if (!round || (!sticky && !(value & 1)))
				break;
			value++;
			sticky = 0;
			round = 0;
		}
	} else {
		while (!(value & HIDDEN)) {
			exponent--;
			value <<= 1;
		}
	}
	result = ((exponent + 127) << 23) | (value & MANTISSA);
sign:
	if (negative)
		result |= SIGN;
	return floatOf(result);
}

/* 0x92576c: -a */
float __negsf2(float a)
{
	return floatOf(bitsOf(a) + SIGN);
}
