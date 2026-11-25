/*
 * Memory helpers, spectral voice activity detector (VAD), and halfband FIR resamplers.
 */

#include "voice_internal.h"
#include <arm_neon.h>

/* 16-tap symmetric halfband FIR filter coefficients (center tap = 0.50080824f) */
static const float gHalfbandFirCoeffs[16] = {
	-0.0017003968823701143f,
	 0.0029373315628618f,
	-0.006730091292411089f,
	 0.014093887992203236f,
	-0.02678503654897213f,
	 0.04909896105527878f,
	-0.09693833440542221f,
	 0.315619558095932f,
	 0.315619558095932f,
	-0.09693833440542221f,
	 0.04909896105527878f,
	-0.02678503654897213f,
	 0.014093887992203236f,
	-0.006730091292411089f,
	 0.0029373315628618f,
	-0.0017003968823701143f,
};

void *voiceMemset(void *dst, uint8_t value, uint32_t size)
{
	uint8_t *ptr = (uint8_t *)dst;
	if (((uintptr_t)ptr & 0xFu) == 0 && size >= 16u) {
		uint8x16_t fill = vdupq_n_u8(value);
		while (size >= 16u) {
			vst1q_u8(ptr, fill);
			ptr += 16;
			size -= 16;
		}
	}
	while (size != 0) {
		*ptr++ = value;
		size--;
	}
	return ptr;
}

void *voiceMemcpy(void *dst, const void *src, uint32_t size)
{
	uint8_t *out = (uint8_t *)dst;
	const uint8_t *in = (const uint8_t *)src;
	if ((((uintptr_t)out | (uintptr_t)in) & 0xFu) == 0 && size >= 16u) {
		while (size >= 16u) {
			uint64x2_t chunk = vld1q_u64((const uint64_t *)in);
			vst1q_u64((uint64_t *)out, chunk);
			in += 16;
			out += 16;
			size -= 16;
		}
	}
	while (size != 0) {
		*out++ = *in++;
		size--;
	}
	return out;
}

int voiceMemcheck(const uint8_t *buf, uint8_t value, uint32_t size)
{
	if (((uintptr_t)buf & 0xFu) == 0 && size >= 4u) {
		uint32_t pattern = ((uint32_t)value << 24) | ((uint32_t)value << 16) |
		                   ((uint32_t)value << 8) | (uint32_t)value;
		while (size >= 4u) {
			if (*(const uint32_t *)buf != pattern) {
				return -1;
			}
			buf += 4;
			size -= 4;
		}
	}
	while (size != 0) {
		if (*buf++ != value) {
			return -1;
		}
		size--;
	}
	return 0;
}

void voiceVadHangoverConfigInit(VoiceVadHangoverConfig *cfg)
{
	cfg->releaseTime = 0.74f;
	cfg->attackTime = 0.1f;
	cfg->frameStep = 1.0f / 48000.0f;
}

void voiceVadHangoverStateInit(VoiceVadHangoverState *state)
{
	state->active = 0;
	state->elapsed = 0.0f;
}

int voiceVadHangoverUpdate(const VoiceVadHangoverConfig *cfg, VoiceVadHangoverState *state,
                           const int *decisionsBegin, const int *decisionsEnd)
{
	float elapsed = state->elapsed;
	int active = state->active;

	for (const int *cur = decisionsBegin; cur != decisionsEnd; cur++) {
		int speech = (*cur != 0) ? 1 : 0;
		elapsed = (speech == active) ? 0.0f : (elapsed + cfg->frameStep);
		if (speech != 0) {
			if (elapsed > cfg->attackTime) {
				active = 1;
			}
		} else {
			if (elapsed > cfg->releaseTime) {
				active = 0;
			}
		}
	}

	state->active = (uint8_t)active;
	state->elapsed = elapsed;
	return active;
}

int voiceVadFilterInit(VoiceVadFilter *vad, float sampleRate)
{
	float frameRate = sampleRate * (1.0f / 256.0f);
	vad->workPtr = vad->workMemory;
	vad->snrThresholdDb = -6.0f;
	vad->levelThresholdDb = -36.0f;
	vad->alphaSignal = (frameRate * 0.1f) / (frameRate * 0.1f + 1.0f);
	vad->alphaNoise = (frameRate * 10.0f) / (frameRate * 10.0f + 1.0f);

	voiceMemset(vad->workPtr, 0, 384 * sizeof(float));
	for (int i = 384; i < 512; i++) {
		vad->workPtr[i] = 0.0001f;
	}

	vad->hangoverCfg.releaseTime = 0.0f;
	vad->hangoverCfg.attackTime = 0.0f;
	vad->hangoverCfg.frameStep = 1.0f / frameRate;
	return 0;
}

int voiceVadFilterProcess(VoiceVadFilter *vad, const int16_t *samplesBegin, const int16_t *samplesEnd)
{
	int numSamples = (int)(samplesEnd - samplesBegin);
	int numBins = numSamples >> 1;
	float *fftBuf = vad->workPtr;
	float *signalPower = fftBuf + numSamples;
	float *noisePower = signalPower + numBins;
	float alphaSig = vad->alphaSignal;
	float alphaNoise = vad->alphaNoise;
	float snrThresh = vad->snrThresholdDb;
	float levelThresh = vad->levelThresholdDb;

	for (int i = 0; i < 256; i++) {
		fftBuf[i] = (float)samplesBegin[i] * (1.0f / 32768.0f);
	}

	voiceRealFft(fftBuf, numSamples);

	fftBuf[0] = fftBuf[0] * fftBuf[0];
	fftBuf[numBins] = fftBuf[numBins] * fftBuf[numBins];
	for (int i = 1; i < numBins; i++) {
		float re = fftBuf[i];
		float im = fftBuf[numBins + i];
		float mag2 = re * re + im * im;
		fftBuf[i] = mag2;
		fftBuf[numBins + i] = mag2;
	}

	float sumPower = 0.0f;
	float sumSnr = 0.0f;
	for (int i = 0; i < numBins; i++) {
		float cur = fftBuf[i];
		float smoothedSig = cur * (1.0f - alphaSig) + alphaSig * signalPower[i];
		float smoothedNoise = cur * (1.0f - alphaNoise) + alphaNoise * noisePower[i];
		signalPower[i] = smoothedSig;
		sumPower += smoothedSig;
		sumSnr += smoothedSig / (smoothedNoise + 1.0e-10f);
	}

	float meanPower = voiceSqrtf(sumPower / (float)numBins);
	float meanSnr = voiceSqrtf(sumSnr / (float)numBins);
	float snrDb = voiceLog10f(meanSnr) * 20.0f;
	float levelDb = voiceLog10f(meanPower) * 20.0f;

	int frameDecision = (snrDb >= snrThresh && levelDb >= levelThresh) ? 1 : 0;
	int speech = voiceVadHangoverUpdate(&vad->hangoverCfg, &vad->hangoverState,
	                                    &frameDecision, &frameDecision + 1);
	if (speech == 0) {
		for (int i = 0; i < numBins; i++) {
			noisePower[i] = fftBuf[i] * (1.0f - alphaNoise) + alphaNoise * noisePower[i];
		}
	}

	return speech;
}

int voiceVadFilterTerm(VoiceVadFilter *vad)
{
	(void)vad;
	return 0;
}

int voiceFirDownsample2x(float *dst, int dstCount, const float *src, int srcCount)
{
	int usableSrc = srcCount - 40;
	if (srcCount <= 40 || dstCount <= 0) {
		return 0;
	}

	int consumed = 0;
	int produced = 0;
	const float *center = src + 21;
	float *out = dst;

	do {
		float acc0 = 0.0f;
		float acc1 = 0.0f;
		float acc2 = 0.0f;
		float acc3 = 0.0f;
		const float *tap = center - 15;
		for (int k = 0; k < 16; k++) {
			float coeff = gHalfbandFirCoeffs[k];
			acc0 += coeff * tap[-6];
			acc1 += coeff * tap[-4];
			acc2 += coeff * tap[-2];
			acc3 += coeff * tap[0];
			tap += 2;
		}
		out[0] = acc0 + center[-6] * 0.50080824f;
		out[1] = acc1 + center[-4] * 0.50080824f;
		out[2] = acc2 + center[-2] * 0.50080824f;
		out[3] = acc3 + center[0] * 0.50080824f;
		out += 4;
		center += 8;
		produced += 4;
		consumed += 8;
	} while (produced < dstCount && consumed < usableSrc);

	return consumed;
}

int voiceFirUpsample2x(float *dst, int dstCount, const float *src, int srcCount)
{
	int usableSrc = srcCount - 40;
	if (srcCount <= 40 || dstCount <= 0) {
		return 0;
	}

	int consumed = 0;
	const float *center = src + 10;
	float *out = dst;

	do {
		float acc0 = 0.0f;
		float acc1 = 0.0f;
		float acc2 = 0.0f;
		float acc3 = 0.0f;
		const float *tap = center - 7;
		for (int k = 0; k < 16; k++) {
			float coeff = gHalfbandFirCoeffs[k];
			acc0 += coeff * tap[-3];
			acc1 += coeff * tap[-2];
			acc2 += coeff * tap[-1];
			acc3 += coeff * tap[0];
			tap++;
		}
		out[0] = (center[-3] * 0.50080824f) * 2.0f;
		out[1] = acc0 * 2.0f;
		out[2] = (center[-2] * 0.50080824f) * 2.0f;
		out[3] = acc1 * 2.0f;
		out[4] = (center[-1] * 0.50080824f) * 2.0f;
		out[5] = acc2 * 2.0f;
		out[6] = (center[0] * 0.50080824f) * 2.0f;
		out[7] = acc3 * 2.0f;
		out += 8;
		center += 4;
		consumed += 4;
	} while (consumed < usableSrc && (2 * consumed) < dstCount);

	return consumed;
}
