#include "../../../../../../naac_internal.h"

int32_t __f98CreateLoopKernel(NaacLoopKernel *loop, int32_t numChannels, int32_t sampleRate, int32_t bitRate)
{
	if (loop == NULL || numChannels <= 0 || sampleRate <= 0 || bitRate <= 0)
		return NAAC_ERR_INVALID_PARAM;

	sceClibMemset(loop, 0, sizeof(NaacLoopKernel));
	loop->numChannels = numChannels;
	loop->sampleRate = sampleRate;
	loop->bitRate = bitRate;
	loop->averageBitsPerFrame = (int32_t)(((double)bitRate * (double)NAAC_FRAME_SAMPLES) / (double)sampleRate);
	loop->maxBitReservoir = 6144 * numChannels;
	loop->bitReservoir = loop->maxBitReservoir / 2;
	return NAAC_OK;
}

static int32_t naacQuantizeChannel(NaacCodChannel *chan, const NaacPsyParam *param, int32_t targetBits)
{
	float maxSpec = 0.0f;
	for (int32_t i = 0; i < NAAC_FRAME_SAMPLES; i++) {
		float mag = naacFabsf(chan->spectrum[i]);
		if (mag > maxSpec)
			maxSpec = mag;
	}

	if (maxSpec <= 1e-6f) {
		chan->globalGain = 0;
		sceClibMemset(chan->quantSpectrum, 0, sizeof(chan->quantSpectrum));
		for (int32_t sfb = 0; sfb < chan->maxSfb; sfb++) {
			chan->scaleFactors[sfb] = 0;
			chan->codebook[sfb] = 0;
		}
		return 0;
	}

	/* Compute initial global gain from peak spectral line */
	int32_t gain = 128;
	float quantStep = naacPowf(2.0f, (float)(gain - 100) * 0.25f);
	if (quantStep < 1e-6f)
		quantStep = 1e-6f;

	int32_t estBits = 0;
	for (int32_t iter = 0; iter < 8; iter++) {
		estBits = 0;
		float invStep = 1.0f / quantStep;
		for (int32_t i = 0; i < NAAC_FRAME_SAMPLES; i++) {
			float mag = naacFabsf(chan->spectrum[i]) * invStep;
			int32_t qval = (int32_t)(naacPowf(mag, 0.75f) + 0.4054f);
			if (qval > 8191)
				qval = 8191;
			chan->quantSpectrum[i] = (chan->spectrum[i] < 0.0f) ? -qval : qval;
			if (qval != 0)
				estBits += 4;
		}
		if (estBits <= targetBits)
			break;
		gain += 4;
		quantStep = naacPowf(2.0f, (float)(gain - 100) * 0.25f);
	}

	if (gain > 255)
		gain = 255;
	chan->globalGain = gain;
	for (int32_t sfb = 0; sfb < chan->maxSfb && sfb < param->numSfbLong; sfb++) {
		chan->scaleFactors[sfb] = gain;
		chan->codebook[sfb] = 0;
	}
	return estBits;
}

int32_t __f98AdvanceLoopKernel(NaacLoopKernel *loop, NaacCodChannel *channels, const NaacPsyParam *param)
{
	if (loop == NULL || channels == NULL || param == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t targetPerChan = (int32_t)((float)loop->averageBitsPerFrame / (float)loop->numChannels);
	if (targetPerChan < 64)
		targetPerChan = 64;

	int32_t usedBits = 0;
	for (int32_t ch = 0; ch < loop->numChannels; ch++)
		usedBits += naacQuantizeChannel(&channels[ch], param, targetPerChan);

	loop->bitReservoir += loop->averageBitsPerFrame - usedBits;
	if (loop->bitReservoir > loop->maxBitReservoir)
		loop->bitReservoir = loop->maxBitReservoir;
	if (loop->bitReservoir < 0)
		loop->bitReservoir = 0;

	return NAAC_OK;
}
