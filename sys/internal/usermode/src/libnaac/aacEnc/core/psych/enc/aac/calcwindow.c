#include "../../../../../naac_internal.h"

static float naacBesselI0(float x)
{
	float sum = 1.0f;
	float term = 1.0f;
	float halfX = x * 0.5f;
	for (int32_t k = 1; k <= 12; k++) {
		term *= (halfX / (float)k) * (halfX / (float)k);
		sum += term;
	}
	return sum;
}

int32_t __f98CalculateLongWindow(float *window, int32_t length, int32_t shape)
{
	if (window == NULL || length <= 0)
		return NAAC_ERR_INVALID_PARAM;

	const float pi = 3.141592653589793f;
	if (shape == NAAC_SHAPE_SINE) {
		for (int32_t i = 0; i < length; i++)
			window[i] = naacSinf((pi / (float)(2 * length)) * ((float)i + 0.5f));
		return NAAC_OK;
	}
	if (shape == NAAC_SHAPE_KBD) {
		const float alphaPi = 4.0f * pi;
		float denom = naacBesselI0(alphaPi);
		float acc = 0.0f;
		float halfLen = (float)length * 0.5f;
		for (int32_t i = 0; i < length; i++) {
			float norm = ((float)i - halfLen) / halfLen;
			float rad = 1.0f - norm * norm;
			if (rad < 0.0f)
				rad = 0.0f;
			acc += naacBesselI0(alphaPi * naacSqrtf(rad)) / denom;
			window[i] = acc;
		}
		if (acc > 0.0f) {
			for (int32_t i = 0; i < length; i++)
				window[i] = naacSqrtf(window[i] / acc);
		}
		return NAAC_OK;
	}
	return NAAC_ERR_INVALID_PARAM;
}

int32_t __f98CalculateShortWindow(float *window, int32_t length, int32_t shape)
{
	return __f98CalculateLongWindow(window, length, shape);
}
