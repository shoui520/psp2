#include "timer.h"
#include "../std/math.h"
#include <arm/fpscr.h>
#include <intrinsics.h>

/* Table suffixes identify the corresponding Timer::Func input curves. */
static const float TimerExpoTable16[33] = {
	0.0f, 0.003378f, 0.007232f, 0.011618f,
	0.016588f, 0.022199f, 0.028518f, 0.035616f,
	0.043573f, 0.052479f, 0.062434f, 0.073549f,
	0.085948f, 0.099769f, 0.115166f, 0.132310f,
	0.151394f, 0.172629f, 0.196254f, 0.222532f,
	0.251759f, 0.284263f, 0.320411f, 0.360610f,
	0.405316f, 0.455034f, 0.510330f, 0.571833f,
	0.640243f, 0.716343f, 0.801004f, 0.895196f,
	1.0f
};

static const float TimerExponentTable[33] = {
	0.0f, 0.002188f, 0.004704f, 0.007592f,
	0.010902f, 0.014686f, 0.019007f, 0.023935f,
	0.029550f, 0.035943f, 0.043218f, 0.051492f,
	0.060902f, 0.071598f, 0.083757f, 0.097577f,
	0.113284f, 0.131136f, 0.151427f, 0.174489f,
	0.200705f, 0.230507f, 0.264389f, 0.302913f,
	0.346719f, 0.396536f, 0.453193f, 0.517637f,
	0.590942f, 0.674336f, 0.769213f, 0.877165f,
	1.0f
};

static const float TimerExpoTable14[33] = {
	0.0f, 0.001299f, 0.002810f, 0.004580f,
	0.006650f, 0.009064f, 0.011878f, 0.015154f,
	0.018965f, 0.023397f, 0.028548f, 0.034535f,
	0.041490f, 0.049571f, 0.058960f, 0.069866f,
	0.082538f, 0.097261f, 0.114368f, 0.134248f,
	0.157351f, 0.184204f, 0.215418f, 0.251703f,
	0.293889f, 0.342938f, 0.399971f, 0.466294f,
	0.543425f, 0.633131f, 0.737468f, 0.858829f,
	1.0f
};

static const float TimerExpoTable13[33] = {
	0.0f, 0.000749f, 0.001639f, 0.002703f,
	0.003973f, 0.005485f, 0.007285f, 0.009425f,
	0.011968f, 0.014989f, 0.018577f, 0.022838f,
	0.027898f, 0.033906f, 0.041040f, 0.049512f,
	0.059575f, 0.071526f, 0.085722f, 0.102586f,
	0.122623f, 0.146430f, 0.174720f, 0.208338f,
	0.248293f, 0.295782f, 0.352227f, 0.419324f,
	0.499085f, 0.593906f, 0.706634f, 0.840656f,
	1.0f
};

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

float TimerDoFuncCustomParabolicIn(float rate, const float *param)
{
	float threshold, value, difference;
	volatile float result;
	union {
		float value;
		uint32_t bits;
	} coefficient;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	threshold = param[0];
	value = param[1];
	if (rate < threshold) {
		difference = (1.0f / threshold) * rate - 1.0f;
		result = value * difference;
		result *= difference;
		return value - result;
	}
	difference = threshold - 1.0f;
	rate -= threshold;
	result = 1.0f - value;
	difference *= difference;
	coefficient.value = result / difference;
	result = coefficient.value * rate;
	result *= rate;
	/* Preserve the coefficient's payload when both factors are NaNs. */
	if ((coefficient.bits & 0x7fffffffU) > 0x7f800000U)
		result = coefficient.value;
	return result + value;
}

float TimerDoFuncCustomParabolicOutIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return (1.0f - TimerDoFuncCustomParabolicIn(1.0f - rate * 2.0f,
							param)) * 0.5f;
	return TimerDoFuncCustomParabolicIn(rate * 2.0f + -1.0f, param) *
		0.5f + 0.5f;
}

float TimerDoFuncCustomParabolicInOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncCustomParabolicIn(rate * 2.0f, param) * 0.5f;
	return (1.0f - TimerDoFuncCustomParabolicIn(2.0f - rate * 2.0f,
						 param)) * 0.5f + 0.5f;
}

float TimerDoFuncCustomParabolicOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncCustomParabolicIn(1.0f - rate, param);
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

static float TimerPolynomialTerm(float coefficient, float factor)
{
	union {
		float value;
		uint32_t bits;
	} number;
	volatile float result = coefficient * factor;

	/* Prior arithmetic has already quieted any NaN in the factor. */
	number.value = coefficient;
	if ((number.bits & 0x7fffffffU) > 0x7f800000U &&
	    !(__builtin_fpscr() & SCE_ARM_FPSCR_DN)) {
		number.bits |= 0x00400000U;
		return number.value;
	}
	return result;
}

float TimerDoFunc72(float rate, const float *param)
{
	float ratio, countValue, duration, span;
	float size = 1.0f, start = 0.0f, direction = -1.0f;
	float nextSize, end, divisor, phase, cosine, amplitude, base;
	int count, convertedCount, limit, step = 1;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	ratio = param[0];
	countValue = param[1];
	duration = param[2];
	count = FloatToInt(countValue);
	convertedCount = FloatToInt(countValue);
	limit = (int)((uint32_t)count + 1U);
	if (ratio < 1.0f) {
		span = duration / (1.0f - ratio);
	} else {
		duration = 0.0f;
		span = 1.0f;
	}
	if (limit > 1) {
		divisor = (float)convertedCount;
		do {
			if (duration == 0.0f) {
				end = 1.0f - (float)step / divisor;
				size = 1.0f - (float)(step - 1) / divisor;
				ratio = end / size;
			}
			start = span * (1.0f - size);
			nextSize = ratio * size;
			end = span * (1.0f - nextSize);
			if (rate >= start && rate < end)
				break;
			step++;
			size = nextSize;
			direction = -direction;
		} while (step < limit);
	}
	if (duration == 0.0f) {
		duration = 1.0f / (float)convertedCount;
		size = 1.0f;
		ratio = 1.0f;
	}
	phase = (rate - start) / (size * duration);
	nextSize = ratio * size;
	phase *= 3.1415927410125732f;
	if (count != step) {
		base = ((direction * 0.5f) * (1.0f - ratio)) * size;
		cosine = sce_paf_cosf(phase);
		amplitude = nextSize + size;
		base += 1.0f;
	} else {
		end = size * direction + 1.0f;
		base = end + 1.0f;
		cosine = sce_paf_cosf(phase);
		amplitude = __builtin_fabsf(1.0f - end);
		base *= 0.5f;
	}
	return base + ((amplitude * direction) * cosine) * 0.5f;
}

float TimerDoFunc71(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFunc72(1.0f - rate, param);
}

float TimerDoFunc73(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFunc71(rate * 2.0f, param) * 0.5f;
	return (1.0f - TimerDoFunc71(2.0f - rate * 2.0f, param)) *
		0.5f + 0.5f;
}

float TimerDoFunc74(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return (1.0f - TimerDoFunc71(1.0f - rate * 2.0f, param)) * 0.5f;
	return TimerDoFunc71(rate * 2.0f + -1.0f, param) * 0.5f + 0.5f;
}

float TimerDoFunc75(float rate, const float *param)
{
	float scale, coefficient;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	scale = (param[0] * 10.0f - 1.0f) * 2.0f;
	coefficient = scale * (0.47f - scale * 0.01f) + 2.70158f;
	return (rate * rate) * (rate * coefficient - (coefficient - 1.0f));
}

float TimerDoFunc76(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFunc75(1.0f - rate, param);
}

float TimerDoFunc77(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFunc75(rate * 2.0f, param) * 0.5f;
	return (1.0f - TimerDoFunc75(2.0f - rate * 2.0f, param)) *
		0.5f + 0.5f;
}

float TimerDoFunc78(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return (1.0f - TimerDoFunc75(1.0f - rate * 2.0f, param)) * 0.5f;
	return TimerDoFunc75(rate * 2.0f + -1.0f, param) * 0.5f + 0.5f;
}

float TimerDoFuncCustomElasticIn(float rate, const float *param)
{
	float frequency, quadratic, quartic, sixth;
	float square, fourth, sine;
	volatile float phase, amplitude;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	square = rate * rate;
	frequency = param[0];
	quadratic = param[1];
	quartic = param[2];
	sixth = param[3];
	fourth = square * square;
	phase = sce_paf_asinf(rate);
	frequency *= 3.1415927410125732f;
	phase += TimerPolynomialTerm(frequency, rate - 1.0f);
	sine = sce_paf_sinf(phase);
	quartic = TimerPolynomialTerm(quartic, fourth);
	sixth = TimerPolynomialTerm(sixth, fourth);
	amplitude = quartic;
	amplitude += TimerPolynomialTerm(quadratic, square);
	amplitude += TimerPolynomialTerm(sixth, square);
	return TimerPolynomialTerm(amplitude, sine);
}

float TimerDoFuncCustomElasticOutIn(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return (1.0f - TimerDoFuncCustomElasticIn(1.0f - rate * 2.0f,
						       param)) * 0.5f;
	return TimerDoFuncCustomElasticIn(rate * 2.0f + -1.0f, param) *
		0.5f + 0.5f;
}

float TimerDoFuncCustomElasticInOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncCustomElasticIn(rate * 2.0f, param) * 0.5f;
	return (1.0f - TimerDoFuncCustomElasticIn(2.0f - rate * 2.0f,
						 param)) * 0.5f + 0.5f;
}

float TimerDoFuncCustomElasticOut(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncCustomElasticIn(1.0f - rate, param);
}

float TimerDoFunc49_52(float rate, const float *param)
{
	const volatile float *coefficients = param;
	float square, fourth, sixth, eighth;
	float quadratic, quartic;
	volatile float result;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	square = rate * rate;
	/* Capture the four input coefficients before evaluating the terms. */
	quartic = coefficients[1];
	quadratic = coefficients[0];
	sixth = coefficients[2];
	eighth = coefficients[3];
	fourth = square * square;
	quartic = TimerPolynomialTerm(quartic, fourth);
	sixth = TimerPolynomialTerm(sixth, fourth);
	eighth = TimerPolynomialTerm(eighth, fourth);
	result = quartic;
	result += TimerPolynomialTerm(quadratic, square);
	result += TimerPolynomialTerm(sixth, square);
	result += TimerPolynomialTerm(eighth, fourth);
	return result;
}

float TimerDoFunc51_54(float rate, const float *param)
{
	float doubled;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	doubled = rate * 2.0f;
	if (rate < 0.5f)
		return TimerDoFunc49_52(doubled, param) * 0.5f;
	return (1.0f - TimerDoFunc49_52(2.0f - doubled, param)) *
		0.5f + 0.5f;
}

float TimerDoFunc50_53(float rate, const float *param)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFunc49_52(1.0f - rate, param);
}

float TimerDoFuncBounceIn(float rate)
{
	float difference;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	difference = 1.0f - rate;
	if (rate > 0.636364f)
		return (difference * -7.5625f) * difference + 1.0f;
	if (rate > 0.272727f) {
		difference -= 0.545454f;
		return (difference * -7.5625f) * difference + 0.25f;
	}
	if (rate > 0.090909f) {
		difference -= 0.818182f;
		return (difference * -7.5625f) * difference + 0.0625f;
	}
	difference -= 0.954545f;
	return (difference * -7.5625f) * difference + 0.015625f;
}

float TimerDoFuncBounceInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncBounceIn(rate * 2.0f) * 0.5f;
	return (1.0f - TimerDoFuncBounceIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
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

float TimerDoFuncElasticIn(float rate)
{
	float amplitude, phase;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f || rate == 0.0f)
		return 0.0f;
	if (rate == 1.0f)
		return 1.0f;
	amplitude = TimerDoFuncExpoIn(rate);
	phase = sce_paf_asinf(rate);
	phase += (rate - 1.0f) * 20.943950653076172f;
	return amplitude * sce_paf_sinf(phase);
}

float TimerDoFuncElasticInOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	if (rate < 0.5f)
		return TimerDoFuncElasticIn(rate * 2.0f) * 0.5f;
	return (1.0f - TimerDoFuncElasticIn(2.0f - rate * 2.0f)) * 0.5f + 0.5f;
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

static float TimerDoFuncExpoInOutTable(float rate, const float *table)
{
	float next, current, fraction, result;
	int integral, index, secondHalf;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	secondHalf = !(rate < 0.5f);
	rate *= 2.0f;
	if (secondHalf)
		rate = 2.0f - rate;
	if (rate >= 1.0f)
		result = 1.0f;
	else if (rate <= 0.0f || rate == 0.0f)
		result = 0.0f;
	else {
		rate *= 32.0f;
		integral = FloatToInt(rate);
		index = FloatToInt(rate);
		next = table[index + 1];
		fraction = rate - (float)integral;
		current = table[index];
		result = next * fraction;
		result += current * (1.0f - fraction);
	}
	if (secondHalf)
		return (1.0f - result) * 0.5f + 0.5f;
	return result * 0.5f;
}

float TimerDoFuncExpoInOut44(float rate)
{
	return TimerDoFuncExpoInOutTable(rate, TimerExpoTable16);
}

float TimerDoFuncExpoInOut43(float rate)
{
	return TimerDoFuncExpoInOutTable(rate, TimerExponentTable);
}

float TimerDoFuncExpoInOut42(float rate)
{
	return TimerDoFuncExpoInOutTable(rate, TimerExpoTable14);
}

float TimerDoFuncExpoInOut41(float rate)
{
	return TimerDoFuncExpoInOutTable(rate, TimerExpoTable13);
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

float TimerDoFuncBounceOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncBounceIn(1.0f - rate);
}

float TimerDoFuncElasticOut(float rate)
{
	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	return 1.0f - TimerDoFuncElasticIn(1.0f - rate);
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

float TimerDoFuncExpoOut30(float rate)
{
	float next, current, fraction, result;
	int integral, index;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	rate = 1.0f - rate;
	if (rate >= 1.0f)
		result = 1.0f;
	else if (rate <= 0.0f || rate == 0.0f)
		result = 0.0f;
	else {
		rate *= 32.0f;
		integral = FloatToInt(rate);
		index = FloatToInt(rate);
		next = TimerExpoTable16[index + 1];
		fraction = rate - (float)integral;
		current = TimerExpoTable16[index];
		result = next * fraction;
		result += current * (1.0f - fraction);
	}
	return 1.0f - result;
}

float TimerDoFuncExponentCurve(float rate)
{
	float next, current, fraction, result;
	int integral, index;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	rate = 1.0f - rate;
	if (rate >= 1.0f)
		result = 1.0f;
	else if (rate <= 0.0f || rate == 0.0f)
		result = 0.0f;
	else {
		rate *= 32.0f;
		integral = FloatToInt(rate);
		index = FloatToInt(rate);
		next = TimerExponentTable[index + 1];
		fraction = rate - (float)integral;
		current = TimerExponentTable[index];
		result = next * fraction;
		result += current * (1.0f - fraction);
	}
	return 1.0f - result;
}

float TimerDoFuncExpoOut28(float rate)
{
	float next, current, fraction, result;
	int integral, index;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	rate = 1.0f - rate;
	if (rate >= 1.0f)
		result = 1.0f;
	else if (rate <= 0.0f || rate == 0.0f)
		result = 0.0f;
	else {
		rate *= 32.0f;
		integral = FloatToInt(rate);
		index = FloatToInt(rate);
		next = TimerExpoTable14[index + 1];
		fraction = rate - (float)integral;
		current = TimerExpoTable14[index];
		result = next * fraction;
		result += current * (1.0f - fraction);
	}
	return 1.0f - result;
}

float TimerDoFuncExpoOut27(float rate)
{
	float next, current, fraction, result;
	int integral, index;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	rate = 1.0f - rate;
	if (rate >= 1.0f)
		result = 1.0f;
	else if (rate <= 0.0f || rate == 0.0f)
		result = 0.0f;
	else {
		rate *= 32.0f;
		integral = FloatToInt(rate);
		index = FloatToInt(rate);
		next = TimerExpoTable13[index + 1];
		fraction = rate - (float)integral;
		current = TimerExpoTable13[index];
		result = next * fraction;
		result += current * (1.0f - fraction);
	}
	return 1.0f - result;
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

float TimerCbrt(float input)
{
	if (input < 0.0f)
		return -sce_paf_powf(-input, 0.3333333432674408f);
	return sce_paf_powf(input, 0.3333333432674408f);
}

float TimerDoFuncBezier3d(float rate, const float *param)
{
	float x1, x2, y1, y2, cubic, quadratic;
	float threeX1, threeX2, threeY1, threeY2;
	float b, c, d, p, cubeB, bc, twiceCubeB;
	float halfQ, halfQSquared, pCubed, thirdB, root;
	float firstRoot, secondRoot, t, cubeT, squareT;
	volatile float result;
	union {
		float value;
		uint32_t bits;
	} negativeHalfQ;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f)
		return 0.0f;
	x1 = param[0];
	x2 = param[2];
	threeX1 = x1 * 3.0f;
	threeX2 = x2 * 3.0f;
	quadratic = x1 * -6.0f + x2 * 3.0f;
	cubic = (threeX1 - threeX2) + 1.0f;
	b = quadratic / cubic;
	d = -rate / cubic;
	c = threeX1 / cubic;
	p = (c - (b * b) / 3.0f) / 3.0f;
	cubeB = sce_paf_powf(b, 3.0f);
	bc = b * c;
	twiceCubeB = cubeB * 2.0f;
	bc /= 3.0f;
	twiceCubeB /= 27.0f;
	d += twiceCubeB;
	d -= bc;
	halfQ = d * 0.5f;
	halfQSquared = sce_paf_powf(halfQ, 2.0f);
	pCubed = sce_paf_powf(p, 3.0f);
	thirdB = b / 3.0f;
	root = __builtin_sqrtf(halfQSquared + pCubed);
	firstRoot = TimerCbrt(root - halfQ);
	/* Negate the product, including its NaN sign. */
	negativeHalfQ.value = d * 0.5f;
	negativeHalfQ.bits ^= 0x80000000U;
	secondRoot = TimerCbrt(negativeHalfQ.value - root);
	y1 = param[1];
	y2 = param[3];
	t = (firstRoot + secondRoot) - thirdB;
	cubeT = sce_paf_powf(t, 3.0f);
	threeY1 = y1 * 3.0f;
	threeY2 = y2 * 3.0f;
	cubic = (threeY1 - threeY2) + 1.0f;
	squareT = sce_paf_powf(t, 2.0f);
	quadratic = y1 * -6.0f + y2 * 3.0f;
	/* Keep the polynomial's rounded accumulation order. */
	result = quadratic * squareT;
	result += cubic * cubeT;
	result += threeY1 * t;
	return result;
}

float TimerDoFuncExponent(float rate, float total, float initval,
			 const float *param)
{
	(void)total;
	(void)initval;
	(void)param;
	return TimerDoFuncExponentCurve(rate);
}

float TimerDoFuncExponent_i(float curr, float total, const float *param)
{
	(void)total;
	return (curr + (1.0f - curr) * param[0]) * 1.0001f;
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

static float TimerDoFuncExpoInTable(float rate, const float *table)
{
	float next, current, fraction;
	volatile float result;
	int integral, index;

	if (rate >= 1.0f)
		return 1.0f;
	if (rate <= 0.0f || rate == 0.0f)
		return 0.0f;
	rate *= 32.0f;
	integral = FloatToInt(rate);
	index = FloatToInt(rate);
	next = table[index + 1];
	fraction = rate - (float)integral;
	current = table[index];
	result = next * fraction;
	result += current * (1.0f - fraction);
	return result;
}

float TimerDoFunc(const Timer *self, float rate, int func, float pre,
		 float *param)
{
	(void)self;
	(void)pre;
	switch (func) {
	case FUNC_SLOWDOWN: {
		volatile float result;

		if (rate >= 1.0f)
			return 1.0f;
		/* The native operation precedes the lower clamp. */
		result = __builtin_sqrtf(rate);
		if (rate <= 0.0f)
			return 0.0f;
		return result;
	}
	case FUNC_FASTUP: {
		volatile float result;

		if (rate >= 1.0f)
			return 1.0f;
		result = rate * rate;
		if (rate <= 0.0f)
			return 0.0f;
		return result;
	}
	case FUNC_ANGULAR:
		return TimerDoFuncAngular(rate);
	case FUNC_EXPONENT:
	case FUNC_EXPO_OUT_UNK_29:
		return TimerDoFuncExponentCurve(rate);
	case FUNC_BEZIER_3D:
		return TimerDoFuncBezier3d(rate, param);
	case FUNC_SWING:
		return TimerDoFuncSwing(rate);
	case FUNC_QUAD_IN:
		return TimerDoFuncQuadIn(rate);
	case FUNC_CUBIC_IN:
		return TimerDoFuncCubicIn(rate);
	case FUNC_QUART_IN:
		return TimerDoFuncQuartIn(rate);
	case FUNC_QUINT_IN:
		return TimerDoFuncQuintIn(rate);
	case FUNC_SINE_IN:
		return TimerDoFuncSineIn(rate);
	case FUNC_EXPO_IN:
		return TimerDoFuncExpoIn(rate);
	case FUNC_EXPO_IN_UNK_13:
		return TimerDoFuncExpoInTable(rate, TimerExpoTable13);
	case FUNC_EXPO_IN_UNK_14:
		return TimerDoFuncExpoInTable(rate, TimerExpoTable14);
	case FUNC_EXPO_IN_UNK_15:
		return TimerDoFuncExpoInTable(rate, TimerExponentTable);
	case FUNC_EXPO_IN_UNK_16:
		return TimerDoFuncExpoInTable(rate, TimerExpoTable16);
	case FUNC_CIRQ_IN:
		return TimerDoFuncCirqIn(rate);
	case FUNC_BACK_IN:
		return TimerDoFuncBackIn(rate);
	case FUNC_ELASTIC_IN:
		return TimerDoFuncElasticIn(rate);
	case FUNC_BOUNCE_IN:
		return TimerDoFuncBounceIn(rate);
	case FUNC_QUAD_OUT:
		return TimerDoFuncQuadOut(rate);
	case FUNC_CUBIC_OUT:
		return TimerDoFuncCubicOut(rate);
	case FUNC_QUART_OUT:
		return TimerDoFuncQuartOut(rate);
	case FUNC_QUINT_OUT:
		return TimerDoFuncQuintOut(rate);
	case FUNC_SINE_OUT:
		return TimerDoFuncSineOut(rate);
	case FUNC_EXPO_OUT:
		return TimerDoFuncExpoOut(rate);
	case FUNC_EXPO_OUT_UNK_27:
		return TimerDoFuncExpoOut27(rate);
	case FUNC_EXPO_OUT_UNK_28:
		return TimerDoFuncExpoOut28(rate);
	case FUNC_EXPO_OUT_UNK_30:
		return TimerDoFuncExpoOut30(rate);
	case FUNC_CIRQ_OUT:
		return TimerDoFuncCirqOut(rate);
	case FUNC_BACK_OUT:
		return TimerDoFuncBackOut(rate);
	case FUNC_ELASTIC_OUT:
		return TimerDoFuncElasticOut(rate);
	case FUNC_BOUNCE_OUT:
		return TimerDoFuncBounceOut(rate);
	case FUNC_QUAD_INOUT:
		return TimerDoFuncQuadInOut(rate);
	case FUNC_CUBIC_INOUT:
		return TimerDoFuncCubicInOut(rate);
	case FUNC_QUART_INOUT:
		return TimerDoFuncQuartInOut(rate);
	case FUNC_QUINT_INOUT:
		return TimerDoFuncQuintInOut(rate);
	case FUNC_SINE_INOUT:
		return TimerDoFuncSineInOut(rate);
	case FUNC_EXPO_INOUT:
		return TimerDoFuncExpoInOut(rate);
	case FUNC_EXPO_INOUT_UNK_41:
		return TimerDoFuncExpoInOut41(rate);
	case FUNC_EXPO_INOUT_UNK_42:
		return TimerDoFuncExpoInOut42(rate);
	case FUNC_EXPO_INOUT_UNK_43:
		return TimerDoFuncExpoInOut43(rate);
	case FUNC_EXPO_INOUT_UNK_44:
		return TimerDoFuncExpoInOut44(rate);
	case FUNC_CIRQ_INOUT:
		return TimerDoFuncCirqInOut(rate);
	case FUNC_BACK_INOUT:
		return TimerDoFuncBackInOut(rate);
	case FUNC_ELASTIC_INOUT:
		return TimerDoFuncElasticInOut(rate);
	case FUNC_BOUNCE_INOUT:
		return TimerDoFuncBounceInOut(rate);
	case FUNC_UNK_49:
	case FUNC_UNK_52:
		return TimerDoFunc49_52(rate, param);
	case FUNC_UNK_50:
	case FUNC_UNK_53:
		return TimerDoFunc50_53(rate, param);
	case FUNC_UNK_51:
	case FUNC_UNK_54:
		return TimerDoFunc51_54(rate, param);
	case FUNC_CUSTOM_EXPO_IN:
		return TimerDoFuncCustomExpoIn(rate, param);
	case FUNC_CUSTOM_EXPO_OUT:
		return TimerDoFuncCustomExpoOut(rate, param);
	case FUNC_CUSTOM_EXPO_INOUT:
		return TimerDoFuncCustomExpoInOut(rate, param);
	case FUNC_CUSTOM_EXPO_OUTIN:
		return TimerDoFuncCustomExpoOutIn(rate, param);
	case FUNC_CUSTOM_PARABOLIC_IN:
		return TimerDoFuncCustomParabolicIn(rate, param);
	case FUNC_CUSTOM_PARABOLIC_OUT:
		return TimerDoFuncCustomParabolicOut(rate, param);
	case FUNC_CUSTOM_PARABOLIC_INOUT:
		return TimerDoFuncCustomParabolicInOut(rate, param);
	case FUNC_CUSTOM_PARABOLIC_OUTIN:
		return TimerDoFuncCustomParabolicOutIn(rate, param);
	case FUNC_CUSTOM_ELASTIC_IN:
		return TimerDoFuncCustomElasticIn(rate, param);
	case FUNC_CUSTOM_ELASTIC_OUT:
		return TimerDoFuncCustomElasticOut(rate, param);
	case FUNC_CUSTOM_ELASTIC_INOUT:
		return TimerDoFuncCustomElasticInOut(rate, param);
	case FUNC_CUSTOM_ELASTIC_OUTIN:
		return TimerDoFuncCustomElasticOutIn(rate, param);
	case FUNC_CUSTOM_UNK_71:
		return TimerDoFunc71(rate, param);
	case FUNC_CUSTOM_UNK_72:
		return TimerDoFunc72(rate, param);
	case FUNC_CUSTOM_UNK_73:
		return TimerDoFunc73(rate, param);
	case FUNC_CUSTOM_UNK_74:
		return TimerDoFunc74(rate, param);
	case FUNC_CUSTOM_UNK_75:
		return TimerDoFunc75(rate, param);
	case FUNC_CUSTOM_UNK_76:
		return TimerDoFunc76(rate, param);
	case FUNC_CUSTOM_UNK_77:
		return TimerDoFunc77(rate, param);
	case FUNC_CUSTOM_UNK_78:
		return TimerDoFunc78(rate, param);
	case FUNC_LINEAR:
	default:
		return TimerDoFuncLinear(rate);
	}
}
