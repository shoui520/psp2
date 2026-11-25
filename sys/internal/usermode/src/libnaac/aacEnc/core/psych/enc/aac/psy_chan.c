#include "../../../../../naac_internal.h"

int32_t __f98CreatePsyChannel(NaacPsyChannel *chan, const NaacPsyParam *param)
{
	if (chan == NULL || param == NULL)
		return NAAC_ERR_INVALID_PARAM;

	sceClibMemset(chan, 0, sizeof(NaacPsyChannel));
	if (__f98CreateFloatFifoBuffer(&chan->inputFifo, chan->timeSignal, NAAC_FRAME_SAMPLES * 2) != NAAC_OK)
		return NAAC_ERR_INTERNAL;
	if (CreateFloatModuloBuffer(&chan->historyBuf, chan->timeSignal, NAAC_FRAME_SAMPLES) != NAAC_OK)
		return NAAC_ERR_INTERNAL;
	if (__f98CreateSwitchControlChannel(&chan->switchCtrl) != NAAC_OK)
		return NAAC_ERR_INTERNAL;
	if (__f98CreateThrChannel(&chan->thrChan) != NAAC_OK)
		return NAAC_ERR_INTERNAL;
	if (__f98CreateTnsData(&chan->tns) != NAAC_OK)
		return NAAC_ERR_INTERNAL;

	__f98SetWindow(chan, NAAC_LONG_WINDOW, NAAC_SHAPE_SINE);
	return NAAC_OK;
}

void naacAdvancePsyChannel(NaacPsyChannel *chan, const float *pcm, const NaacPsyParam *param)
{
	if (chan == NULL || pcm == NULL || param == NULL)
		return;

	/* Shift overlap buffer and append new 1024-sample frame */
	sceClibMemmove(chan->timeSignal, &chan->timeSignal[NAAC_FRAME_SAMPLES],
		NAAC_FRAME_SAMPLES * sizeof(float));
	sceClibMemcpy_safe(&chan->timeSignal[NAAC_FRAME_SAMPLES], pcm,
		NAAC_FRAME_SAMPLES * sizeof(float));

	naacAdvanceSwitchControl(&chan->switchCtrl, pcm, NAAC_FRAME_SAMPLES);

	float windowed[NAAC_FRAME_SAMPLES * 2];
	for (int32_t i = 0; i < NAAC_FRAME_SAMPLES; i++) {
		windowed[i] = chan->timeSignal[i] * chan->longWindowSine[i];
		windowed[NAAC_FRAME_SAMPLES + i] = chan->timeSignal[NAAC_FRAME_SAMPLES + i] *
			chan->longWindowSine[NAAC_FRAME_SAMPLES - 1 - i];
	}

	naacComputeMdct(windowed, chan->mdctSpectrum, NAAC_FRAME_SAMPLES * 2);
	naacComputeThresholds(&chan->thrChan, chan->mdctSpectrum, param);
}
