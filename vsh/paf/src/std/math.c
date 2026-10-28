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
