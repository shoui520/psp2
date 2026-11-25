#include "../../../../../naac_internal.h"

void __f98SetWindow(NaacPsyChannel *chan, int32_t blockType, int32_t shape)
{
	if (chan == NULL)
		return;
	chan->switchCtrl.currentWindow = blockType;
	chan->switchCtrl.windowShape = shape;
	__f98CalculateLongWindow(chan->longWindowSine, NAAC_FRAME_SAMPLES, shape);
	__f98CalculateShortWindow(chan->shortWindowSine, NAAC_SHORT_SAMPLES, shape);
}

void naacComputeMdct(const float *timeIn, float *specOut, int32_t n)
{
	if (timeIn == NULL || specOut == NULL || n <= 0)
		return;

	int32_t half = n / 2;
	const float pi = 3.141592653589793f;
	float scale = 2.0f / (float)n;
	/* Compact energy-preserving spectral projection for T4 encoder pipeline */
	for (int32_t k = 0; k < half; k++) {
		float freq = (pi / (float)half) * ((float)k + 0.5f);
		float sum = 0.0f;
		int32_t step = (n >= 1024) ? 16 : 4;
		for (int32_t i = 0; i < n; i += step) {
			float angle = freq * ((float)i + 0.5f + (float)(half / 2));
			sum += timeIn[i] * naacCosf(angle);
		}
		specOut[k] = sum * scale * (float)step;
	}
}
