#include "math.h"
#include <stdint.h>
#include <intrinsics.h>

float sce_paf_sinf(float x)
{
	union {
		float value;
		uint32_t bits;
	} number;
	const float twoPi = 6.2831854820251465f;
	float pi = twoPi * 0.5f;
	float halfPi = pi * 0.5f;
	float quotient, square, polynomial, result;
	uint32_t sign;

	number.value = x;
	sign = number.bits;
	number.bits &= 0x7fffffffU;
	x = number.value;
	if (x > twoPi) {
		quotient = x / twoPi;
		number.value = quotient;
		/* Large float quotients are already integral; avoid conversion. */
		if ((number.bits << 1) < 0x98000000U)
			quotient = (float)(int32_t)quotient;
		x -= quotient * twoPi;
	}
	if (pi < x) {
		x -= pi;
		sign = 0U - sign;
	}
	if (x > halfPi)
		x = pi - x;
	if (x > 0.9173450469970703f) {
		x = halfPi - x;
		square = x * x;
		polynomial = 0.04166315123438835f +
			     square * -0.001371985417790711f;
		polynomial = -0.4999997913837433f + polynomial * square;
		result = polynomial * square;
		result += 1.0f;
	} else {
		square = x * x;
		polynomial = 0.00833132490515709f +
			     square * -0.0001941887749126181f;
		polynomial = -0.16666638851165771f + polynomial * square;
		result = polynomial * square;
		result += 1.0f;
		result = x * result;
	}
	number.value = result;
	/* Preserve an all-zero result before restoring the quadrant sign. */
	if (number.bits != 0)
		number.bits = (sign & 0x80000000U) |
			      (number.bits & 0x7fffffffU);
	return number.value;
}

float sce_paf_cosf(float x)
{
	union {
		float value;
		uint32_t bits;
	} number;
	const float twoPi = 6.2831854820251465f;
	float pi = twoPi * 0.5f;
	float halfPi = pi * 0.5f;
	float quotient, square, polynomial, result;
	uint32_t sign = 1;

	x = __builtin_fabsf(x);
	if (x > twoPi) {
		quotient = x / twoPi;
		number.value = quotient;
		if ((number.bits << 1) < 0x98000000U)
			quotient = (float)(int32_t)quotient;
		x -= quotient * twoPi;
	}
	if (pi < x) {
		sign = 0xffffffffU;
		x -= pi;
	}
	if (x > halfPi) {
		sign = 0U - sign;
		x = pi - x;
	}
	if (x > 0.6534512639045715f) {
		x = halfPi - x;
		square = x * x;
		polynomial = 0.00833132490515709f +
			     square * -0.0001941887749126181f;
		polynomial = -0.16666638851165771f + polynomial * square;
		result = polynomial * square;
		result += 1.0f;
		result = x * result;
	} else {
		square = x * x;
		polynomial = 0.04166315123438835f +
			     square * -0.001371985417790711f;
		polynomial = -0.4999997913837433f + polynomial * square;
		result = polynomial * square;
		result += 1.0f;
	}
	number.value = result;
	if (number.bits != 0)
		number.bits = (sign & 0x80000000U) |
			      (number.bits & 0x7fffffffU);
	return number.value;
}

float AtanApprox(float x)
{
	float square, polynomial, offset, product, numerator;

	if (x <= 0.20000000298023224f) {
		square = x * x;
		polynomial = square * -0.14000000059604645f;
		polynomial += 0.20000000298023224f;
		polynomial *= square;
		polynomial += -0.3333333134651184f;
		polynomial *= square;
		polynomial += 1.0f;
		return x * polynomial;
	}
	if (x < 0.6499999761581421f) {
		product = x * 0.4142135679721832f;
		numerator = x - 0.4142135679721832f;
		x = numerator / (product + 1.0f);
		offset = 0.39269909262657166f;
	} else {
		numerator = x - 1.0f;
		x = numerator / (x + 1.0f);
		offset = 0.7853981852531433f;
	}
	square = x * x;
	polynomial = square * -0.14000000059604645f;
	polynomial += 0.20000000298023224f;
	polynomial *= square;
	polynomial += -0.3333333134651184f;
	polynomial *= square;
	polynomial += 1.0f;
	return offset + x * polynomial;
}

float sce_paf_atanf(float x)
{
	union {
		float value;
		uint32_t bits;
	} number;
	uint32_t sign;
	float result;

	number.value = x;
	sign = number.bits & 0x80000000U;
	number.bits &= 0x7fffffffU;
	if (number.bits > 0x3fd30000U)
		result = 1.5707963705062866f - AtanApprox(1.0f / number.value);
	else
		result = AtanApprox(number.value);
	number.value = result;
	number.bits = sign | (number.bits & 0x7fffffffU);
	return number.value;
}

float sce_paf_asinf(float x)
{
	union {
		float value;
		uint32_t bits;
	} input, root, result;
	uint32_t rootBits;

	input.value = x;
	root.value = __builtin_sqrtf(1.0f - x * x);
	rootBits = root.bits;
	input.bits &= 0x7fffffffU;
	root.bits &= 0x7fffffffU;
	if (root.bits > input.bits)
		result.value = AtanApprox(input.value / root.value);
	else
		result.value = 1.5707963705062866f -
			       AtanApprox(root.value / input.value);
	if ((int32_t)rootBits < 0)
		result.value = 3.1415927410125732f - result.value;
	input.value = x;
	result.bits = (input.bits & 0x80000000U) |
		      (result.bits & 0x7fffffffU);
	return result.value;
}

float sce_paf_expf(float x)
{
	union {
		float value;
		uint32_t bits;
	} number;
	float fraction, numerator, quadratic, denominator, result;
	int exponent;

	number.value = x;
	number.bits &= 0x7fffffffU;
	if (number.bits >= 0x42b00f34U) {
		if (x != x)
			return x;
		if (x < 0.0f)
			return 0.0f;
		number.bits = 0x7f800000U;
		return number.value;
	}
	fraction = number.value * 1.4426950216293335f;
	exponent = (int)fraction;
	fraction -= (float)(int)fraction;
	if (fraction > 0.5f) {
		exponent++;
		fraction -= 1.0f;
	}
	if (x < 0.0f) {
		fraction = -fraction;
		exponent = -exponent;
	}
	numerator = fraction * 0.6931471824645996f;
	quadratic = fraction * 0.030016301199793816f;
	numerator += 6.0f;
	denominator = fraction * 0.3465735912322998f;
	numerator += quadratic * fraction;
	denominator = 6.0f - denominator;
	number.bits = (uint32_t)(exponent + 127) << 23;
	result = numerator / denominator;
	result *= result;
	result *= result;
	return result * number.value;
}

static const float logCoefficients[] = {
	2.000000476837158f,
	0.6664773225784302f,
	0.41397473216056824f
};

float sce_paf_powf(float base, float exponent)
{
	union {
		float value;
		uint32_t bits;
	} number;
	int integerExponent = FloatToInt(exponent);
	int truncatedExponent = FloatToInt(exponent);
	float result, reduced, square, polynomial, logarithm;
	uint32_t mantissa, correction;
	int numerator, denominator;

	if (exponent < 0.0f) {
		if (base < 0.0f && exponent - (float)truncatedExponent > 0.0f)
			return 0.0f;
		return 1.0f / sce_paf_powf(base, -exponent);
	}
	if (exponent == 0.0f)
		return 1.0f;
	if (__builtin_fabsf(exponent - (float)truncatedExponent) <=
	    9.999999974752427e-7f) {
		result = 1.0f;
		while (integerExponent > 0) {
			integerExponent--;
			result *= base;
		}
		return result;
	}
	if (base == 0.0f)
		return 0.0f;
	number.value = base;
	if ((int32_t)number.bits > 0) {
		mantissa = number.bits & 0x7fffffU;
		numerator = (int)mantissa - 0x800000;
		denominator = (int)mantissa;
		if (mantissa <= 0x3504f3U) {
			numerator = (int)mantissa;
			denominator = (int)mantissa - 0x800000;
			number.bits -= 0x800000U;
		}
		denominator += 0x1800000;
		reduced = (float)numerator / (float)denominator;
		correction = (uint32_t)((int32_t)number.bits >> 23) *
			     11629080U - 1465264080U;
		square = reduced * reduced;
		polynomial = logCoefficients[1] + logCoefficients[2] * square;
		polynomial = logCoefficients[0] + polynomial * square;
		logarithm = reduced * polynomial;
		logarithm += (float)(int32_t)correction * 5.960464477539063e-8f;
	} else {
		if ((number.bits & 0x7fffffffU) == 0)
			number.bits = 0xff800000U;
		else
			number.bits = 0xffc00000U;
		logarithm = number.value;
	}
	return sce_paf_expf(exponent * logarithm);
}
