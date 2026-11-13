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

/*
 * 0x925774: a 64-bit unsigned quotient (or, with remainder set, the
 * remainder) into result[0] (low word) and result[1]. Long division a hex
 * digit at a time: the dividend is aligned on the divisor's leading digit,
 * then each step takes 8, 4, 2 and 1 times the divisor. A divisor with its
 * top digit set takes one step. Shifts take the shifter's five bits, which
 * decides what a zero divisor gives.
 */
void divideUnsigned64(uint32_t *result, uint32_t dividendHigh, uint32_t dividendLow,
	uint32_t divisorHigh, uint32_t divisorLow, uint32_t remainder)
{
	uint32_t twiceHigh = (divisorHigh << 1) | (divisorLow >> 31), twiceLow = divisorLow << 1;
	uint32_t fourHigh = (divisorHigh << 2) | (divisorLow >> 30), fourLow = divisorLow << 2;
	uint32_t eightHigh = (divisorHigh << 3) | (divisorLow >> 29), eightLow = divisorLow << 3;
	uint32_t remHigh, remLow, nextHigh, nextLow, digit = 0, quotientHigh = 0;
	uint32_t lead, topDividend, topDivisor;
	int32_t zerosDividend, zerosDivisor, count;

	if (divisorHigh & 0xf0000000) {
		remHigh = dividendHigh;
		remLow = dividendLow;
		nextHigh = nextLow = 0;
		count = 60;
		if (divisorHigh & 0x80000000)
			goto once;
		if (divisorHigh & 0x40000000)
			goto twice;
		if (divisorHigh & 0x20000000)
			goto four;
		goto eight;
	}

	lead = dividendHigh ? dividendHigh : dividendLow;
	zerosDividend = 32;
	if (lead) {
		do {
			zerosDividend -= 4;
			lead >>= 4;
		} while (lead);
	}
	if (dividendHigh == 0) {
		topDividend = dividendLow >> ((28 - zerosDividend) & 31);
		zerosDividend += 32;
	} else {
		topDividend = dividendHigh >> ((28 - zerosDividend) & 31);
	}
	lead = divisorHigh ? divisorHigh : divisorLow;
	zerosDivisor = 32;
	if (lead) {
		do {
			zerosDivisor -= 4;
			lead >>= 4;
		} while (lead);
	}
	if (divisorHigh == 0) {
		topDivisor = divisorLow >> ((28 - zerosDivisor) & 31);
		zerosDivisor += 32;
	} else {
		topDivisor = divisorHigh >> ((28 - zerosDivisor) & 31);
	}
	if (topDividend >= topDivisor)
		zerosDivisor += 4;
	count = zerosDividend - zerosDivisor + 64;
	if (count >= 64) {
		remHigh = dividendHigh;
		remLow = dividendLow;
		goto done;
	}

	if (count >= 32) {
		remHigh = 0;
		if (count >= 33)
			remHigh = dividendHigh >> ((64 - count) & 31);
		remLow = dividendHigh << ((count - 32) & 31);
		if (count >= 33)
			remLow |= dividendLow >> ((64 - count) & 31);
		nextHigh = dividendLow << ((count - 32) & 31);
		nextLow = 0;
	} else {
		remLow = 0;
		if (count != 0)
			remLow = dividendHigh >> ((32 - count) & 31);
		nextHigh = dividendHigh << (count & 31);
		if (count != 0)
			nextHigh |= dividendLow >> ((32 - count) & 31);
		remHigh = 0;
		nextLow = dividendLow << (count & 31);
	}
	for (;;) {
		/* the next digit of the dividend into the remainder */
		remHigh = (remHigh << 4) | (remLow >> 28);
		remLow = (remLow << 4) | (nextHigh >> 28);
		nextHigh = (nextHigh << 4) | (nextLow >> 28);
		nextLow <<= 4;
eight:
		if (remHigh > eightHigh || (remHigh == eightHigh && remLow >= eightLow)) {
			remHigh -= eightHigh + (remLow < eightLow);
			remLow -= eightLow;
			digit |= 8;
		}
four:
		if (remHigh > fourHigh || (remHigh == fourHigh && remLow >= fourLow)) {
			remHigh -= fourHigh + (remLow < fourLow);
			remLow -= fourLow;
			digit |= 4;
		}
twice:
		if (remHigh > twiceHigh || (remHigh == twiceHigh && remLow >= twiceLow)) {
			remHigh -= twiceHigh + (remLow < twiceLow);
			remLow -= twiceLow;
			digit |= 2;
		}
once:
		if (remHigh > divisorHigh || (remHigh == divisorHigh && remLow >= divisorLow)) {
			remHigh -= divisorHigh + (remLow < divisorLow);
			remLow -= divisorLow;
			digit |= 1;
		}
		count += 4;
		if (count == 32) {
			quotientHigh = digit;
			digit = 0;
		}
		if (count == 64)
			break;
		digit <<= 4;
	}
done:
	if (remainder) {
		result[0] = remLow;
		result[1] = remHigh;
	} else {
		result[0] = digit;
		result[1] = quotientHigh;
	}
}

/*
 * The 64-bit integer helpers: values by address, as two words, the low word
 * first; results through the first argument, comparisons as truth values.
 */
typedef struct Words64 {
	uint32_t low;
	uint32_t high;
} Words64;

void subtract64(Words64 *result, const Words64 *a, const Words64 *b);

/* 0x926930: -a */
void negate64(Words64 *result, const Words64 *a)
{
	Words64 zero = { 0, 0 };

	subtract64(result, &zero, a);
}

/* 0x926950: a <= b, signed */
int lessEqual64(const Words64 *a, const Words64 *b)
{
	if (a->high == b->high)
		return !(b->low < a->low);
	return (int32_t)a->high < (int32_t)b->high;
}

/* 0x92696a: a < b, signed */
int less64(const Words64 *a, const Words64 *b)
{
	if (a->high == b->high)
		return a->low < b->low;
	return (int32_t)a->high < (int32_t)b->high;
}

/* 0x926982: a >= b, signed */
int greaterEqual64(const Words64 *a, const Words64 *b)
{
	if (a->high == b->high)
		return !(a->low < b->low);
	return !((int32_t)a->high < (int32_t)b->high);
}

/* 0x92699e: a > b, signed */
int greater64(const Words64 *a, const Words64 *b)
{
	if (a->high == b->high)
		return b->low < a->low;
	return !((int32_t)a->high < (int32_t)b->high);
}

/* 0x9269b8: a <= b, unsigned */
int lessEqualUnsigned64(const Words64 *a, const Words64 *b)
{
	if (a->high == b->high)
		return !(b->low < a->low);
	return a->high < b->high;
}

/* 0x9269d2: a < b, unsigned */
int lessUnsigned64(const Words64 *a, const Words64 *b)
{
	if (a->high == b->high)
		return a->low < b->low;
	return a->high < b->high;
}

/* 0x9269ea: a >= b, unsigned */
int greaterEqualUnsigned64(const Words64 *a, const Words64 *b)
{
	if (a->high == b->high)
		return !(a->low < b->low);
	return !(a->high < b->high);
}

/* 0x926a06: a > b, unsigned */
int greaterUnsigned64(const Words64 *a, const Words64 *b)
{
	if (a->high == b->high)
		return b->low < a->low;
	return !(a->high < b->high);
}

/* 0x926a20: a == b */
int equal64(const Words64 *a, const Words64 *b)
{
	if (a->high != b->high)
		return 0;
	return a->low == b->low;
}

/* 0x926a3a: a == b, b an int */
int equalInt64(const Words64 *a, int32_t b)
{
	if (a->high != (uint32_t)(b < 0 ? -1 : 0))
		return 0;
	return a->low == (uint32_t)b;
}

/* 0x926a56: a != b */
int notEqual64(const Words64 *a, const Words64 *b)
{
	if (a->high != b->high)
		return 1;
	return a->low != b->low;
}

/* 0x926a72: a != b, b an int */
int notEqualInt64(const Words64 *a, int32_t b)
{
	if (a->high != (uint32_t)(b < 0 ? -1 : 0))
		return 1;
	return a->low != (uint32_t)b;
}

/* 0x926a92: a <= 0 */
int isNotPositive64(const Words64 *a)
{
	if (a->high == 0)
		return a->low == 0;
	return (int32_t)a->high < 0;
}

/* 0x926aa2: a < 0 */
int isNegative64(const Words64 *a)
{
	return (int32_t)a->high < 0;
}

/* 0x926aaa: a >= 0 */
int isNotNegative64(const Words64 *a)
{
	return !((int32_t)a->high < 0);
}

/* 0x926ab4: a > 0 */
int isPositive64(const Words64 *a)
{
	if (a->high == 0)
		return a->low != 0;
	return !((int32_t)a->high < 0);
}

/* 0x926ac8: a == 0 */
int isZero64(const Words64 *a)
{
	return (a->high | a->low) == 0;
}

/* 0x926ad4: a != 0 */
int isNotZero64(const Words64 *a)
{
	return (a->high | a->low) != 0;
}

/* 0x926ae4: a - b */
void subtract64(Words64 *result, const Words64 *a, const Words64 *b)
{
	result->high = a->high - b->high - (a->low < b->low);
	result->low = a->low - b->low;
}

/* 0x926b04: the most negative value, for the signed division */
const Words64 g_minimum64 = { 0, 0x80000000 };

/* the break instruction: C has no trap */
static inline void trap(void)
{
	__asm__ volatile ("break");
}

static void shiftRightUnsigned64(Words64 *value, uint32_t shift)
{
	if (shift >= 32) {
		value->low = value->high >> (shift - 32);
		value->high = 0;
	} else if (shift != 0) {
		value->low = (value->low >> shift) | (value->high << (32 - shift));
		value->high >>= shift;
	}
}

/*
 * 0x92599c: a / b, signed, toward zero. A zero divisor traps (break). The
 * magnitudes are divided: by a shift when the divisor's is a power of two
 * (its top bit found by shifting it right, arithmetically, until something
 * is left), else by divideUnsigned64.
 */
void divide64(Words64 *result, const Words64 *a, const Words64 *b)
{
	Words64 magnitudeA, magnitudeB, negated, below, shifted, quotient;
	uint32_t negative, bNegative, aNegative, zeros, shift;

	if ((b->low | b->high) == 0)
		trap();
	if ((a->high | a->low) == 0)
		goto zero;
	if ((b->low & b->high) == 0xffffffff) {
		if (a->high == 0x80000000 && a->low == 0)
			*result = g_minimum64;
		else
			negate64(result, a);
		return;
	}

	negate64(&negated, b);
	bNegative = (int32_t)b->high >> 31;
	aNegative = (int32_t)a->high >> 31;
	if (aNegative) {
		negative = !bNegative;
		magnitudeB = bNegative ? negated : *b;
		negate64(&magnitudeA, a);
	} else {
		negative = bNegative != 0;
		magnitudeB = bNegative ? negated : *b;
		magnitudeA = *a;
	}
	if (lessUnsigned64(&magnitudeA, &magnitudeB))
		goto zero;
	if (magnitudeA.high == magnitudeB.high && magnitudeA.low == magnitudeB.low) {
		quotient.low = negative ? 0xffffffff : 1;
		quotient.high = negative ? 0xffffffff : 0;
		*result = quotient;
		return;
	}

	subtract64(&below, &magnitudeB, &(const Words64){ 1, 0 });
	if ((below.low & magnitudeB.low) | (below.high & magnitudeB.high)) {
		divideUnsigned64(&quotient.low, magnitudeA.high, magnitudeA.low, magnitudeB.high,
			magnitudeB.low, 0);
	} else {
		zeros = 0;
		for (shift = 63; ; ) {
			if (shift >= 32) {
				shifted.low = (int32_t)magnitudeB.high >> (shift - 32);
				shifted.high = (int32_t)magnitudeB.high >> 31;
			} else {
				shifted.low = (magnitudeB.low >> shift) | (magnitudeB.high << (32 - shift));
				shifted.high = (int32_t)magnitudeB.high >> shift;
			}
			if ((shifted.low | shifted.high) != 0)
				break;
			zeros++;
			if (--shift == 0)
				break;
		}
		quotient = magnitudeA;
		shiftRightUnsigned64(&quotient, 63 - zeros);
	}
	if (negative)
		negate64(&quotient, &quotient);
	*result = quotient;
	return;

zero:
	result->low = 0;
	result->high = 0;
}

/* 0x925c04: a / b, unsigned; the shortcuts of divide64 */
void divideUnsignedWords64(Words64 *result, const Words64 *a, const Words64 *b)
{
	Words64 below, shifted, quotient;
	uint32_t zeros, shift;

	if ((b->low | b->high) == 0)
		trap();
	if ((a->low | a->high) == 0 || lessUnsigned64(a, b)) {
		result->high = 0;
		result->low = 0;
		return;
	}
	if (a->low == b->low && a->high == b->high) {
		quotient.low = 1;
		quotient.high = 0;
		*result = quotient;
		return;
	}
	subtract64(&below, b, &(const Words64){ 1, 0 });
	if ((below.low & b->low) | (below.high & b->high)) {
		divideUnsigned64(&quotient.low, a->high, a->low, b->high, b->low, 0);
	} else {
		zeros = 0;
		for (shift = 63; ; ) {
			if (shift >= 32) {
				shifted.low = (int32_t)b->high >> (shift - 32);
				shifted.high = (int32_t)b->high >> 31;
			} else {
				shifted.low = (b->low >> shift) | (b->high << (32 - shift));
				shifted.high = (int32_t)b->high >> shift;
			}
			if ((shifted.low | shifted.high) != 0)
				break;
			zeros++;
			if (--shift == 0)
				break;
		}
		quotient = *a;
		shiftRightUnsigned64(&quotient, 63 - zeros);
	}
	*result = quotient;
}

/*
 * 0x925d82: a % b, signed: the sign of a. The magnitudes' remainder, by a
 * mask when the divisor's is a power of two.
 */
void remainder64(Words64 *result, const Words64 *a, const Words64 *b)
{
	Words64 magnitudeA, magnitudeB, mask, value;
	uint32_t bNegative, aNegative;

	if ((b->low | b->high) == 0)
		trap();
	if ((a->low | a->high) == 0)
		goto zero;
	bNegative = b->high >> 31;
	aNegative = a->high >> 31;
	if (bNegative)
		negate64(&magnitudeB, b);
	else
		magnitudeB = *b;
	if (aNegative)
		negate64(&magnitudeA, a);
	else
		magnitudeA = *a;
	if (lessUnsigned64(&magnitudeA, &magnitudeB)) {
		*result = *a;
		return;
	}
	if (magnitudeA.low == magnitudeB.low && magnitudeA.high == magnitudeB.high)
		goto zero;

	subtract64(&mask, &magnitudeB, &(const Words64){ 1, 0 });
	if ((mask.low & magnitudeB.low) | (mask.high & magnitudeB.high)) {
		divideUnsigned64(&value.low, magnitudeA.high, magnitudeA.low, magnitudeB.high,
			magnitudeB.low, 1);
		if (aNegative && (value.low | value.high) != 0)
			negate64(&value, &value);
	} else {
		value.low = magnitudeA.low & mask.low;
		value.high = magnitudeA.high & mask.high;
		if (aNegative && (value.low | value.high) != 0 && !(value.high >> 31))
			negate64(&value, &value);
	}
	*result = value;
	return;

zero:
	result->low = 0;
	result->high = 0;
}

/* 0x925f14: a % b, unsigned */
void remainderUnsigned64(Words64 *result, const Words64 *a, const Words64 *b)
{
	Words64 mask, value;

	if ((b->low | b->high) == 0)
		trap();
	if ((a->low | a->high) == 0)
		goto zero;
	if (lessUnsigned64(a, b)) {
		*result = *a;
		return;
	}
	if (a->low == b->low && a->high == b->high)
		goto zero;
	subtract64(&mask, b, &(const Words64){ 1, 0 });
	if ((mask.low & b->low) | (mask.high & b->high)) {
		divideUnsigned64(&value.low, a->high, a->low, b->high, b->low, 1);
	} else {
		value.low = a->low & mask.low;
		value.high = a->high & mask.high;
	}
	*result = value;
	return;

zero:
	result->low = 0;
	result->high = 0;
}

/*
 * The double-precision routines: values by address as Words64 (sign,
 * 11-bit exponent and the mantissa's top 20 bits in the high word).
 */
#define D_SIGN		0x80000000
#define D_EXPONENT	0x7ff00000
#define D_MANTISSA	0xfffff
#define D_HIDDEN	0x100000

/*
 * 0x926034: a + b. The operand with the larger exponent field first; the
 * mantissas three bits up, the smaller one aligned with a sticky bit, added
 * as two's complement, normalized and rounded to nearest, ties to even. A
 * zero b gives a as it is.
 */
void addDouble(Words64 *result, const Words64 *a, const Words64 *b)
{
	const Words64 *swap;
	uint32_t aHigh, aLow, bHigh, bLow, signA, signB, high, sticky, round, t;
	int32_t exponentA, exponentB, difference, left;
	uint32_t sign;

	if ((a->high & D_EXPONENT) < (b->high & D_EXPONENT)) {
		swap = a;
		a = b;
		b = swap;
	}
	bHigh = b->high & D_MANTISSA;
	bLow = b->low;
	aLow = a->low;
	aHigh = a->high & D_MANTISSA;
	signA = a->high & D_SIGN;
	signB = b->high & D_SIGN;
	exponentA = (a->high & D_EXPONENT) >> 20;
	exponentB = (b->high & D_EXPONENT) >> 20;
	sign = signA != 0;

	if (exponentA == 2047) {
		if (aHigh || aLow)
			goto nan;
		if (exponentB != 2047)
			goto infinity;
		if (bHigh || bLow)
			goto nan;
		if (signA ^ signB)
			goto nan;
		goto infinity;
	}
	if (exponentB == 2047) {
		if (bHigh || bLow)
			goto nan;
		goto infinity;
	}
	if (exponentB == 0 && bHigh == 0 && bLow == 0)
		goto store;

	if (exponentA != 0)
		aHigh |= D_HIDDEN;
	else
		exponentA++;
	if (exponentB != 0)
		bHigh |= D_HIDDEN;
	else
		exponentB++;

	difference = exponentA - exponentB - 3;
	if (difference >= 1) {
		left = 0;
		if (difference < 32) {
			sticky = bLow << ((32 - difference) & 31);
			bLow >>= difference;
			if (difference < 21) {
				bLow |= bHigh << (32 - difference);
				bHigh >>= difference;
			} else {
				bLow |= (bHigh << 11) >> (difference - 21);
				bHigh = 0;
			}
		} else if (difference < 53) {
			sticky = 0;
			if (bLow != 0 || (bHigh << ((64 - difference) & 31)) != 0)
				sticky = 1;
			bLow = (bHigh << 11) >> (difference - 21);
			bHigh = 0;
		} else {
			/* b only rounds: a is the result */
			sticky = 1;
			round = 0;
			goto rounded;
		}
	} else {
		sticky = 0;
		left = -difference;
	}

	aHigh = (aHigh << 3) | (aLow >> 29);
	aLow <<= 3;
	if (left != 0) {
		bHigh = (bHigh << left) | (bLow >> (32 - left));
		bLow <<= left;
	}

	sign = signA && signB;
	if (signA ^ signB) {
		if (signA) {
			t = aHigh;
			aHigh = bHigh;
			bHigh = t;
			t = aLow;
			aLow = bLow;
			bLow = t;
		}
		bHigh = ~bHigh;
		bLow = ~bLow + 1;
		bHigh += bLow < 1;
	}
	aHigh += bHigh;
	aLow += bLow;
	aHigh += aLow < bLow;
	if (aHigh == 0 && aLow == 0) {
		high = 0;
		result->high = high | (sign ? D_SIGN : 0);
		result->low = 0;
		return;
	}
	if (aHigh >> 31) {
		sign = 1;
		aHigh = ~aHigh;
		t = ~aLow;
		aLow = t + 1;
		aHigh += aLow < t;
	}

	if (aHigh >= 0x1000000) {
		if (sticky == 0)
			sticky = aLow & 1;
		aLow = (aHigh << 31) | (aLow >> 1);
		aHigh >>= 1;
		exponentA++;
	} else {
		while (!(aHigh & 0x800000)) {
			if (exponentA < 2)
				break;
			aHigh = (aHigh << 1) | (aLow >> 31);
			aLow <<= 1;
			exponentA--;
		}
	}
	if (sticky == 0)
		sticky = aLow & 3;
	round = (aLow & 4) >> 2;
	aLow = (aLow >> 3) | (aHigh << 29);
	aHigh >>= 3;

rounded:
	if (exponentA == 1 && !(aHigh & D_HIDDEN))
		exponentA = 0;
	if (round && (sticky || (aLow & 1))) {
		aLow++;
		if (aLow == 0) {
			aHigh++;
			if (aHigh == 0x200000) {
				aHigh >>= 1;
				exponentA++;
			} else if (aHigh == D_HIDDEN && exponentA == 0) {
				exponentA++;
			}
		}
	}
	if (exponentA >= 2047)
		goto infinity;

store:
	result->low = aLow;
	high = ((exponentA << 20) & D_EXPONENT) | (aHigh & D_MANTISSA);
	result->high = high;
	if (sign)
		result->high = high | D_SIGN;
	return;

nan:
	result->high = D_EXPONENT | D_MANTISSA | (sign ? D_SIGN : 0);
	result->low = 0xffffffff;
	return;
infinity:
	result->high = D_EXPONENT | (sign ? D_SIGN : 0);
	result->low = 0;
}

/*
 * 0x926298: a * b. Each mantissa as two words, multiplied bit-serially into
 * four 64-bit partial products, summed into 128 bits, normalized (left
 * twelve, then one either way) and rounded to nearest, ties to even. A
 * rounding carry out of a denormal leaves its exponent at zero.
 */
void multiplyDouble(Words64 *result, const Words64 *a, const Words64 *b)
{
	uint32_t aLow = a->low, bLow = b->low;
	uint32_t aHigh = a->high & D_MANTISSA, bHigh = b->high & D_MANTISSA;
	int32_t exponentA = (a->high << 1) >> 21, exponentB = (b->high << 1) >> 21, exponent;
	uint32_t sign = (b->high ^ a->high) >> 31;
	uint64_t low = 0, high = 0, lowByHigh = 0, highByHigh = 0, multiplicandLow, multiplicandHigh;
	uint32_t multiplierLow, multiplierHigh, w0, w1, w2, w3, carry, round, sticky;
	uint32_t top, upper, lower, bottom;
	Words64 value = { 0, 0 };

	if (exponentA == 2047) {
		if (aHigh || aLow)
			goto nan;
		if (exponentB == 2047 && (bHigh || bLow))
			goto nan;
		if (exponentB == 0 && bHigh == 0 && bLow == 0)
			goto nan;
		goto infinity;
	}
	if (exponentA == 0) {
		if (aHigh == 0 && aLow == 0) {
			if (exponentB != 2047)
				goto sign;
			goto nan;
		}
		exponentA++;
	} else {
		aHigh |= D_HIDDEN;
	}
	if (exponentB == 2047) {
		if (bHigh || bLow)
			goto nan;
		goto infinity;
	}
	if (exponentB == 0) {
		if (bHigh == 0 && bLow == 0)
			goto sign;
		exponentB++;
	} else {
		bHigh |= D_HIDDEN;
	}

	exponent = exponentA + exponentB - 1023;
	multiplicandLow = aLow;
	multiplicandHigh = aHigh;
	multiplierLow = bLow;
	multiplierHigh = bHigh;
	while (multiplierLow != 0 || multiplierHigh != 0) {
		if (multiplierLow & 1) {
			low += multiplicandLow;
			high += multiplicandHigh;
		}
		if (multiplierHigh & 1) {
			lowByHigh += multiplicandLow;
			highByHigh += multiplicandHigh;
		}
		multiplierLow >>= 1;
		multiplierHigh >>= 1;
		multiplicandLow <<= 1;
		multiplicandHigh <<= 1;
	}
	/* the partial products summed into four words */
	w0 = (uint32_t)low;
	w1 = (uint32_t)high + (uint32_t)lowByHigh;
	carry = w1 < (uint32_t)lowByHigh;
	w1 += (uint32_t)(low >> 32);
	if (w1 < (uint32_t)(low >> 32))
		carry++;
	w2 = (uint32_t)highByHigh + carry;
	carry = w2 < carry;
	w2 += (uint32_t)(high >> 32);
	if (w2 < (uint32_t)(high >> 32))
		carry++;
	w2 += (uint32_t)(lowByHigh >> 32);
	if (w2 < (uint32_t)(lowByHigh >> 32))
		carry++;
	w3 = (uint32_t)(highByHigh >> 32) + carry;

	top = (w3 << 12) | (w2 >> 20);
	upper = (w2 << 12) | (w1 >> 20);
	lower = (w1 << 12) | (w0 >> 20);
	bottom = w0 << 12;
	if (top >= 0x200000) {
		exponent++;
		bottom = (lower << 31) | (bottom >> 1);
		lower = (upper << 31) | (lower >> 1);
		upper = (top << 31) | (upper >> 1);
		top >>= 1;
	} else {
		while (!(top & D_HIDDEN)) {
			if (exponent < 2)
				break;
			exponent--;
			top = (top << 1) | (upper >> 31);
			upper = (upper << 1) | (lower >> 31);
			lower = (lower << 1) | (bottom >> 31);
			bottom <<= 1;
		}
	}
	if (exponent == 1 && !(top & D_HIDDEN)) {
		exponent = 0;
	} else if (exponent < 1) {
		do {
			exponent++;
			bottom = (lower << 31) | (bottom >> 1);
			lower = (upper << 31) | (lower >> 1);
			upper = (top << 31) | (upper >> 1);
			top >>= 1;
		} while (exponent < 1);
		exponent = 0;
	}

	round = lower >> 31;
	sticky = (lower & 0x7fffffff) != 0 || bottom != 0;
	if (round && (sticky || (upper & 1))) {
		if (upper == 0xffffffff) {
			upper = 0;
			if (top == 0x1fffff) {
				top = D_HIDDEN;
				exponent++;
			} else {
				top++;
			}
		} else {
			upper++;
		}
	}
	if (exponent >= 2047)
		goto infinity;
	if (exponent != 0 && (exponent < 0 || top == 0))
		goto sign;
	value.high |= (exponent << 20) | (top & D_MANTISSA);
	value.low |= upper;
	goto sign;

nan:
	value.low = 0xffffffff;
	value.high = 0x7fffffff;
	goto sign;
infinity:
	value.high = D_EXPONENT;
sign:
	if (sign)
		value.high |= D_SIGN;
	result->high = value.high;
	result->low = value.low;
}

/* a register shift amount: the core takes its low five bits */
static inline uint32_t shiftLeft(uint32_t value, int32_t count)
{
	return value << (count & 31);
}

static inline uint32_t shiftRight(uint32_t value, int32_t count)
{
	return value >> (count & 31);
}

/*
 * 0x926550: a / b. Restoring division four quotient bits at a time against
 * the divisor's multiples by one, two, four and eight: 23 bits into the high
 * word, counted from the first nonzero bit, then 32 into the low word, then
 * the bits the high word took too many shifted back down. Two guard bits
 * round to nearest; a normal result rounds up on any set low word (the
 * remainder is only sticky for a denormal one). The dividend is not
 * normalized, the divisor is.
 */
void divideDouble(Words64 *result, const Words64 *a, const Words64 *b)
{
	uint32_t aHigh = a->high & D_MANTISSA, aLow = a->low;
	uint32_t bHigh = b->high & D_MANTISSA, bLow = b->low;
	uint32_t signA = a->high & D_SIGN, signB = b->high & D_SIGN;
	uint32_t fieldA = a->high & D_EXPONENT, fieldB = b->high & D_EXPONENT;
	int aMaximum = fieldA == D_EXPONENT, bMaximum = fieldB == D_EXPONENT;
	int32_t exponent, exponentB, free, excess = 0, shift, bits, phase;
	uint64_t remainder, divisor;
	uint32_t quotient, word, scan, high = 0, low = 0, sticky, nonzero, round;

	if ((aMaximum && (aHigh || aLow)) || (bMaximum && (bHigh || bLow)))
		goto nan;
	if (aMaximum && bMaximum)
		goto nan;
	if (!fieldA && !fieldB && !aHigh && !aLow && !bHigh && !bLow)
		goto nan;
	if (aMaximum || (!fieldB && !bHigh && !bLow))
		goto infinity;
	if (!fieldA && !aHigh && !aLow)
		goto sign;
	if (bMaximum && !bHigh && !bLow)
		goto sign;

	exponent = fieldA >> 20;
	exponentB = fieldB >> 20;
	if (exponent)
		aHigh |= D_HIDDEN;
	else
		exponent++;
	if (exponentB)
		bHigh |= D_HIDDEN;
	else
		exponentB++;
	divisor = ((uint64_t)bHigh << 32) | bLow;
	while (!((uint32_t)(divisor >> 32) & D_HIDDEN)) {
		divisor <<= 1;
		exponentB--;
	}
	exponent -= exponentB;
	if (exponent < -1076)
		goto sign;

	remainder = (((uint64_t)aHigh << 32) | aLow) << 3;
	phase = 0;
	free = 23;
	quotient = 0;
	for (;;) {
		if (remainder >= divisor << 3) {
			quotient |= 8;
			remainder -= divisor << 3;
		}
		if (remainder >= divisor << 2) {
			quotient |= 4;
			remainder -= divisor << 2;
		}
		if (remainder >= divisor << 1) {
			quotient |= 2;
			remainder -= divisor << 1;
		}
		if (remainder >= divisor) {
			quotient |= 1;
			remainder -= divisor;
		}
		if (phase) {
			free -= 4;
		} else {
			bits = 32;
			if (quotient) {
				bits = 0;
				for (scan = quotient; !(scan & 0x80000000); scan <<= 1)
					bits++;
			}
			bits = 32 - bits;
			if (bits == 0) {
				exponent -= 4;
			} else {
				free -= bits;
				exponent -= 4 - bits;
				phase = 1;
			}
		}
		if (remainder == 0) {
			nonzero = 0;
			word = free < 0 ? shiftRight(quotient, -free) : shiftLeft(quotient, free);
			if (phase == 1) {
				high = word;
				goto done;
			}
			low = word;
			if (excess)
				goto shiftDown;
			goto done;
		}
		if (free < 1) {
			if (phase != 1) {
				nonzero = 1;
				low = quotient;
				if (excess)
					goto shiftDown;
				goto done;
			}
			excess = -free;
			high = quotient;
			quotient = 0;
			free = 32;
			phase = 2;
		}
		remainder <<= 4;
		quotient <<= 4;
	}

shiftDown:
	low = shiftRight(low, excess) | shiftLeft(high, 32 - excess);
	high = shiftRight(high, excess);
done:
	if (!(high | low))
		goto sign;
	sticky = low;
	exponent += 1023;
	if (exponent < 1) {
		shift = 1 - exponent;
		exponent = 0;
		sticky = (low & 1) | nonzero;
		if (shift < 32) {
			sticky |= shiftLeft(low, 32 - shift);
			low = (low >> shift) | shiftLeft(high, 32 - shift);
			high >>= shift;
			sticky |= low & 1;
		} else if (shift < 54) {
			shift -= 32;
			sticky |= shiftLeft(high, 32 - shift);
			low = high >> shift;
			high = 0;
			sticky |= low & 1;
		} else {
			sticky = 1;
			low = 0;
			high = 0;
		}
		if (!(high | low))
			goto sign;
	}
	round = (low & 2) >> 1;
	low = (low >> 2) | (high << 30);
	high >>= 2;
	if (round && (sticky || (low & 1))) {
		low++;
		if (low == 0) {
			high++;
			if (high == 0x200000) {
				high >>= 1;
				exponent++;
			} else if (exponent == 0 && high == D_HIDDEN) {
				exponent++;
			}
		}
	}
	if (exponent >= 2047)
		goto infinity;
	high = (high & (D_SIGN | D_MANTISSA)) | (exponent << 20);
	goto sign;

nan:
	low = 0xffffffff;
	high = 0x7fffffff;
	goto sign;
infinity:
	high = D_EXPONENT;
	low = 0;
sign:
	if (signA ^ signB)
		high |= D_SIGN;
	result->low = low;
	result->high = high;
}

/*
 * 0x926858: a double truncated to an int. Magnitudes from 2^31 up, and
 * infinities and NaNs, give +-0x7fffffff; the low word's bits below the top
 * ten never count.
 */
int32_t doubleToInt(const Words64 *value)
{
	int32_t exponent = ((value->high & D_EXPONENT) >> 20) - 1053;
	int32_t integer;

	if (exponent >= 1)
		integer = 0x7fffffff;
	else if (exponent < -31)
		integer = 0;
	else
		integer = ((((value->high & D_MANTISSA) | D_HIDDEN) << 10) |
			   (value->low >> 22)) >> -exponent;
	if (value->high >> 31)
		integer = ~integer + 1;
	return integer;
}

/* 0x9268a8: an int as a double, exactly */
void intToDouble(Words64 *result, int32_t value)
{
	uint32_t negative = 0, magnitude, mantissa;
	int32_t shift;

	if ((uint32_t)value >> 31) {
		negative = 1;
		value = ~value + 1;
	}
	magnitude = mantissa = value;
	result->low = 0;
	if (mantissa == 0) {
		result->high = 0;
		goto sign;
	}
	shift = 0;
	if (mantissa >= 0x200000) {
		while (mantissa & 0xffe00000) {
			shift++;
			mantissa >>= 1;
		}
	} else {
		while (!(mantissa & D_HIDDEN)) {
			shift--;
			mantissa <<= 1;
		}
	}
	if (shift >= 1)
		result->low = magnitude << (32 - shift);
	mantissa = (mantissa & D_MANTISSA) | ((shift + 1043) << 20);
sign:
	if (negative)
		mantissa |= D_SIGN;
	result->high = mantissa;
}
