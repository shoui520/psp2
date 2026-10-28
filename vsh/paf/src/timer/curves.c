#include "timer.h"
#include "../std/math.h"
#include <intrinsics.h>

static const float TimerExpoTable[33] = {
	0.0f, 0.000243f, 0.000539f, 0.000908f,
	0.001367f, 0.001938f, 0.002646f, 0.003524f,
	0.004614f, 0.005967f, 0.007644f, 0.009724f,
	0.012305f, 0.015506f, 0.019478f, 0.024407f,
	0.030522f, 0.038111f, 0.047531f, 0.059222f,
	0.073734f, 0.091749f, 0.114113f, 0.141879f,
	0.176352f, 0.219153f, 0.272297f, 0.338284f,
	0.420220f, 0.521963f, 0.648302f, 0.805185f,
	1.0f
};

float TimerDoFuncLinear(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return rate;
}

float TimerDoFuncCustomExpoIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - sce_paf_powf(param[0] * (1.0f - rate), rate * param[1]);
}

float TimerDoFuncCustomExpoOutIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return (1.0f - TimerDoFuncCustomExpoIn(1.0f - rate * 2.0f,
						    param)) * 0.5f;
	return TimerDoFuncCustomExpoIn(rate * 2.0f + -1.0f, param) * 0.5f + 0.5f;
}

float TimerDoFuncCustomExpoInOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncCustomExpoIn(rate * 2.0f, param) * 0.5f;
	return (1.0f - TimerDoFuncCustomExpoIn(2.0f - rate * 2.0f, param)) *
	       0.5f + 0.5f;
}

float TimerDoFuncCustomExpoOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncCustomExpoIn(1.0f - rate, param);
}

float TimerDoFuncExpoIn(float rate)
{
	float next, current, fraction, result;
	int integral, index;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f || rate == 0.0f)
		return 0.0f;
	rate *= 32.0f;
	integral = FloatToInt(rate);
	index = FloatToInt(rate);
	next = TimerExpoTable[index + 1];
	fraction = rate - (float)integral;
	current = TimerExpoTable[index];
	result = next * fraction;
	result += current * (1.0f - fraction);
	return result;
}

float TimerDoFuncBackIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return (rate * rate) * (rate * 2.70158f - 1.70158f);
}

float TimerDoFuncBackInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncBackIn(rate * 2.0f) * 0.5f;
	return (1.0f - TimerDoFuncBackIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float TimerDoFuncCirqIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - __builtin_sqrtf(1.0f - rate * rate);
}

float TimerDoFuncCirqInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncCirqIn(rate * 2.0f) * 0.5f;
	return (1.0f - TimerDoFuncCirqIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float TimerDoFuncQuintIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return (((rate * rate) * rate) * rate) * rate;
}

float TimerDoFuncQuintInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncQuintIn(rate * 2.0f) * 0.5f;
	return (1.0f - TimerDoFuncQuintIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float TimerDoFuncQuartIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return ((rate * rate) * rate) * rate;
}

float TimerDoFuncQuartInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncQuartIn(rate * 2.0f) * 0.5f;
	return (1.0f - TimerDoFuncQuartIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float TimerDoFuncCubicIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return (rate * rate) * rate;
}

float TimerDoFuncCubicInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncCubicIn(rate * 2.0f) * 0.5f;
	return (1.0f - TimerDoFuncCubicIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float TimerDoFuncFaster(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return rate * rate;
}

float TimerDoFuncExpoInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncExpoIn(rate * 2.0f) * 0.5f;
	return (1.0f - TimerDoFuncExpoIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float TimerDoFuncSineIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - sce_paf_cosf((rate * 3.1415927410125732f) * 0.5f);
}

float TimerDoFuncSineInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncSineIn(rate * 2.0f) * 0.5f;
	return (1.0f - TimerDoFuncSineIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float TimerDoFuncQuadIn(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return TimerDoFuncFaster(rate);
}

float TimerDoFuncQuadInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncQuadIn(rate * 2.0f) * 0.5f;
	return (1.0f - TimerDoFuncQuadIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
}

float TimerDoFuncBackOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncBackIn(1.0f - rate);
}

float TimerDoFuncCirqOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncCirqIn(1.0f - rate);
}

float TimerDoFuncQuintOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncQuintIn(1.0f - rate);
}

float TimerDoFuncQuartOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncQuartIn(1.0f - rate);
}

float TimerDoFuncCubicOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncCubicIn(1.0f - rate);
}

float TimerDoFuncQuadOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncQuadIn(1.0f - rate);
}

float TimerDoFuncExpoOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncExpoIn(1.0f - rate);
}

float TimerDoFuncSineOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncSineIn(1.0f - rate);
}

float TimerDoFuncSwing(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return sce_paf_cosf(rate * 3.1415927410125732f) * -0.5f + 0.5f;
}

float TimerDoFuncSlower(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return __builtin_sqrtf(rate);
}

float TimerDoFuncAngular(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return (sce_paf_sinf((rate - 0.5f) * 3.1415927410125732f) +
		1.0f) * 0.5f;
}
