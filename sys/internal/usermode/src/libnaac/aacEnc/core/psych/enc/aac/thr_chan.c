#include "../../../../../naac_internal.h"

int32_t __f98CreateThrData(NaacThrData *thr)
{
	if (thr == NULL)
		return NAAC_ERR_INVALID_PARAM;
	sceClibMemset(thr, 0, sizeof(NaacThrData));
	for (int32_t i = 0; i < NAAC_MAX_SFB_LONG; i++)
		thr->sfbMinSnr[i] = 0.5f;
	return NAAC_OK;
}

int32_t __f98CreateThrChannel(NaacThrChannel *chan)
{
	if (chan == NULL)
		return NAAC_ERR_INVALID_PARAM;
	sceClibMemset(chan, 0, sizeof(NaacThrChannel));
	return __f98CreateThrData(&chan->thr);
}

void naacComputeThresholds(NaacThrChannel *chan, const float *spectrum, const NaacPsyParam *param)
{
	if (chan == NULL || spectrum == NULL || param == NULL)
		return;

	float peSum = 0.0f;
	for (int32_t sfb = 0; sfb < param->numSfbLong; sfb++) {
		int32_t start = param->sfbOffsetLong[sfb];
		int32_t end = param->sfbOffsetLong[sfb + 1];
		float bandEnergy = 0.0f;
		for (int32_t k = start; k < end; k++)
			bandEnergy += spectrum[k] * spectrum[k];

		chan->thr.energy[sfb] = bandEnergy;
		float maskThr = bandEnergy * 0.05f + 1.0f;
		if (chan->prevEnergy[sfb] > 0.0f && maskThr > chan->prevEnergy[sfb] * 2.0f)
			maskThr = chan->prevEnergy[sfb] * 2.0f;
		chan->thr.threshold[sfb] = maskThr;
		chan->prevEnergy[sfb] = bandEnergy;

		if (bandEnergy > maskThr)
			peSum += (float)(end - start) * naacLog10f(bandEnergy / maskThr);
	}
	chan->thr.perceptualEntropy = peSum;
}
