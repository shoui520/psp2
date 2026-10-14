/*
 * SceDriverUser: SceMotion single-precision trigonometry, polynomial
 * approximations with the sign carried by the IEEE sign bit.
 */

#include "driver_user.h"
#include "motion.h"

#define PI_F		3.14159274f
#define HALF_PI_F	1.57079637f
#define TWO_PI_F	6.28318548f

typedef union FloatBits {
	float f;
	SceUInt32 u;
} FloatBits;

static inline SceUInt32 Bits(float value)
{
	FloatBits bits = { .f = value };

	return bits.u;
}

static inline float FromBits(SceUInt32 value)
{
	FloatBits bits = { .u = value };

	return bits.f;
}

/* the magnitude of value with the sign bit of sign */
static inline float WithSign(float value, SceUInt32 sign)
{
	return FromBits((sign & 0x80000000) | (Bits(value) & 0x7fffffff));
}

/* angle modulo 2 pi; quotients from 2^25 up are integral already */
static inline float ReduceTwoPi(float angle)
{
	float turns;

	if (angle > TWO_PI_F) {
		turns = angle / TWO_PI_F;
		if (Bits(turns) << 1 < 0x98000000)
			turns = (float)(int)turns;
		angle -= TWO_PI_F * turns;
	}
	return angle;
}

static inline float AtanPolynomial(float t)
{
	float t2 = t * t;
	float p = 0.2f + t2 * -0.14f;

	p = t2 * p - 0.333333313f;
	return t * (1.0f + t2 * p);
}

/* atan of a non-negative value: direct, about pi/8, or about pi/4 */
MOTION_VFP float MotionAtanPositive(float x)
{
	float t;

	if (x <= 0.2f)
		return AtanPolynomial(x);
	if (x < 0.65f) {
		t = (x - 0.414213568f) / (1.0f + x * 0.414213568f);
		return 0.392699093f + AtanPolynomial(t);
	}
	t = (x - 1.0f) / (x + 1.0f);
	return 0.785398185f + AtanPolynomial(t);
}

MOTION_VFP float MotionSin(float angle)
{
	SceUInt32 sign = Bits(angle);
	float a = ReduceTwoPi(__builtin_fabsf(angle));
	float a2, value;

	if (a > PI_F) {
		a -= PI_F;
		sign = -sign;
	}
	if (a > HALF_PI_F)
		a = PI_F - a;
	if (a <= 0.917345047f) {
		a2 = a * a;
		value = 0.00833132491f + a2 * -0.000194188775f;
		value = a2 * value - 0.166666389f;
		value = a * (1.0f + a2 * value);
	} else {
		a = HALF_PI_F - a;
		a2 = a * a;
		value = 0.0416631512f + a2 * -0.00137198542f;
		value = a2 * value - 0.499999791f;
		value = 1.0f + a2 * value;
	}
	if (Bits(value) == 0)
		return 0.0f;
	return WithSign(value, sign);
}

MOTION_VFP float MotionCos(float angle)
{
	float a = ReduceTwoPi(__builtin_fabsf(angle));
	float a2, value;
	int sign;

	if (a > PI_F) {
		a -= PI_F;
		sign = -1;
	} else {
		sign = 1;
	}
	if (a > HALF_PI_F) {
		sign = -sign;
		a = PI_F - a;
	}
	if (a <= 0.653451264f) {
		a2 = a * a;
		value = 0.0416631512f + a2 * -0.00137198542f;
		value = a2 * value - 0.499999791f;
		value = 1.0f + a2 * value;
	} else {
		a = HALF_PI_F - a;
		a2 = a * a;
		value = 0.00833132491f + a2 * -0.000194188775f;
		value = a2 * value - 0.166666389f;
		value = a * (1.0f + a2 * value);
	}
	if (Bits(value) == 0)
		return 0.0f;
	return WithSign(value, sign);
}

MOTION_VFP float MotionAtan(float x)
{
	SceUInt32 sign = Bits(x);
	float a = __builtin_fabsf(x);

	if (Bits(a) <= 0x3fd30000)
		return WithSign(MotionAtanPositive(a), sign);
	return WithSign(HALF_PI_F - MotionAtanPositive(1.0f / a), sign);
}

MOTION_VFP float MotionAtan2(float y, float x)
{
	float ay = __builtin_fabsf(y), ax = __builtin_fabsf(x);
	float angle;

	if (Bits(ax) > Bits(ay))
		angle = MotionAtanPositive(ay / ax);
	else
		angle = HALF_PI_F - MotionAtanPositive(ax / ay);
	if ((int)Bits(x) < 0)
		angle = PI_F - angle;
	return WithSign(angle, Bits(y));
}

MOTION_VFP float MotionAsin(float x)
{
	return MotionAtan2(x, __builtin_sqrtf(1.0f - x * x));
}

MOTION_VFP float MotionAcos(float x)
{
	return MotionAtan2(__builtin_sqrtf(1.0f - x * x), x);
}

MOTION_VFP float MotionTan(float angle)
{
	return MotionSin(angle) / MotionCos(angle);
}
