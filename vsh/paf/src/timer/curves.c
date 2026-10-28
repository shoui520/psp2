#include "timer.h"
#include <intrinsics.h>

float TimerDoFuncLinear(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return rate;
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

float TimerDoFuncSlower(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return __builtin_sqrtf(rate);
}
