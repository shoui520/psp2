#include "g729_internal.h"

static const float kPi = 3.14159265358979323846f;
static const float kHalfPi = 1.57079632679489661923f;
static const float kTwoPi = 6.28318530717958647692f;
static const float kInvLn10 = 0.43429448190325182765f;
static const float kLn2 = 0.69314718055994530942f;

float g729Fabsf(float value)
{
	return (value < 0.0f) ? -value : value;
}

float g729Sqrtf(float value)
{
	if (value <= 0.0f)
		return 0.0f;
	float guess = value;
	union { float flt; uint32_t bits; } conv;
	conv.flt = value;
	conv.bits = (conv.bits >> 1) + 0x1FC00000U;
	guess = conv.flt;
	for (int i = 0; i < 4; i++)
		guess = 0.5f * (guess + value / guess);
	return guess;
}

float g729Floorf(float value)
{
	int32_t truncated = (int32_t)value;
	if ((float)truncated > value)
		truncated--;
	return (float)truncated;
}

float g729Cosf(float angle)
{
	float rad = g729Fabsf(angle);
	if (rad > kTwoPi) {
		int32_t turns = (int32_t)(rad / kTwoPi);
		rad -= (float)turns * kTwoPi;
	}
	float sign = 1.0f;
	if (rad > kPi)
		rad = kTwoPi - rad;
	if (rad > kHalfPi) {
		rad = kPi - rad;
		sign = -1.0f;
	}
	float sq = rad * rad;
	float poly = 1.0f + sq * (-0.5f + sq * (0.041666668f + sq * (-0.0013888889f + sq * 0.000024801587f)));
	return sign * poly;
}

float g729Acosf(float value)
{
	if (value >= 1.0f)
		return 0.0f;
	if (value <= -1.0f)
		return kPi;
	float neg = (value < 0.0f) ? 1.0f : 0.0f;
	float absVal = g729Fabsf(value);
	float poly = -0.0187293f;
	poly = poly * absVal + 0.0742610f;
	poly = poly * absVal - 0.2121144f;
	poly = poly * absVal + 1.5707288f;
	float res = poly * g729Sqrtf(1.0f - absVal);
	if (neg > 0.0f)
		res = kPi - res;
	return res;
}

static float g729Lnf(float value)
{
	if (value <= 0.0f)
		return -80.0f;
	union { float flt; uint32_t bits; } conv;
	conv.flt = value;
	int32_t exp = (int32_t)((conv.bits >> 23) & 0xFFU) - 127;
	conv.bits = (conv.bits & 0x007FFFFFU) | 0x3F800000U;
	float mant = conv.flt;
	float ratio = (mant - 1.0f) / (mant + 1.0f);
	float ratioSq = ratio * ratio;
	float series = ratio * (2.0f + ratioSq * (0.66666667f + ratioSq * (0.4f + ratioSq * 0.28571429f)));
	return series + (float)exp * kLn2;
}

float g729Log10f(float value)
{
	return g729Lnf(value) * kInvLn10;
}

static float g729Expf(float value)
{
	if (value > 88.0f)
		value = 88.0f;
	if (value < -88.0f)
		return 0.0f;
	float scaled = value / kLn2;
	int32_t intPart = (int32_t)g729Floorf(scaled);
	float frac = value - (float)intPart * kLn2;
	float poly = 1.0f + frac * (1.0f + frac * (0.5f + frac * (0.16666667f + frac * 0.041666668f)));
	union { float flt; uint32_t bits; } conv;
	conv.bits = (uint32_t)(intPart + 127) << 23;
	return poly * conv.flt;
}

float g729Powf(float base, float exponent)
{
	if (base <= 0.0f)
		return 0.0f;
	return g729Expf(exponent * g729Lnf(base));
}

uint16_t g729PrngNext(uint32_t *seed)
{
	uint32_t next = (*seed) * 31821U + 13849U;
	*seed = next & 0xFFFFU;
	return (uint16_t)(*seed);
}
