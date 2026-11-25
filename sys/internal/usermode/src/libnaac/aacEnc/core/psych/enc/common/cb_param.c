#include "../../../../../naac_internal.h"

static float naacFreqToBark(float freqHz)
{
	float khz = freqHz * 0.001f;
	return 13.0f * naacAtanf(0.76f * khz) + 3.5f * naacAtanf((freqHz / 7500.0f) * (freqHz / 7500.0f));
}

void CalculateBarcCbWidthVal(int32_t sampleRate, int32_t numLines, float *outWidths, int32_t *outNumCb)
{
	if (outWidths == NULL || outNumCb == NULL || sampleRate <= 0 || numLines <= 0)
		return;

	float nyquist = (float)sampleRate * 0.5f;
	float maxBark = naacFreqToBark(nyquist);
	int32_t numCb = (int32_t)(maxBark * 2.0f) + 1;
	if (numCb > NAAC_MAX_CB)
		numCb = NAAC_MAX_CB;
	if (numCb < 1)
		numCb = 1;

	float step = maxBark / (float)numCb;
	for (int32_t i = 0; i < numCb; i++)
		outWidths[i] = step;
	*outNumCb = numCb;
}

void CalculateFixedBarcCbWidthVal(int32_t sampleRate, int32_t numLines, float *outWidths, int32_t *outNumCb)
{
	if (outWidths == NULL || outNumCb == NULL || sampleRate <= 0 || numLines <= 0)
		return;

	int32_t numCb = 14;
	if (numCb > NAAC_MAX_CB)
		numCb = NAAC_MAX_CB;
	for (int32_t i = 0; i < numCb; i++)
		outWidths[i] = 1.0f;
	*outNumCb = numCb;
}

void CalculateSpreadingFunction(int32_t numCb, const float *widths, float *outSpread)
{
	if (widths == NULL || outSpread == NULL || numCb <= 0)
		return;

	for (int32_t i = 0; i < numCb && i < NAAC_MAX_CB; i++) {
		float dz = widths[i];
		float spreadDb = -15.0f * dz;
		outSpread[i] = naacPowf(10.0f, spreadDb * 0.1f);
	}
}
