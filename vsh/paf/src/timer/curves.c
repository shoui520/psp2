#include "timer.h"
#include "../std/math.h"
#include <intrinsics.h>

float TimerDoFuncLinear(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return rate;
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
