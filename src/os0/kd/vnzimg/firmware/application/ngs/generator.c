/*
 * The signal generator (SCE_NGS_GENERATOR_ID): sine, triangle, saw and
 * pulse waves (the VLIW kernels), white noise from an LCG and the PSP's
 * noise from a shift register.
 */
#include "ngs.h"

#define SCE_NGS_GENERATOR_SINE		0
#define SCE_NGS_GENERATOR_TRIANGLE	1
#define SCE_NGS_GENERATOR_SAW		2
#define SCE_NGS_GENERATOR_NOISE		3
#define SCE_NGS_GENERATOR_NOISE_PSP	4
#define SCE_NGS_GENERATOR_PULSE		5

#define TWO_PI				0x6487ed8	/* 8.24 */
#define LCG_MULTIPLIER			0x41c64e6d
#define LCG_INCREMENT			12345
#define NOISE_LANES			4
#define WAVE_LANES			4	/* samples the IVC2 makes at once */
#define PULSE_UNIT			4096	/* of a sample, the pulse's phase */
/* PSP noise: the shift register's period, halved per frequency step of 4 */
#define PSP_NOISE_PERIOD		16384
/* bytes of the clock pattern for each frequency & 3, in a nibble each */
#define PSP_NOISE_PATTERNS		0xffffeeaa
/* the feedback taps, indexed by bits 10-12 of the register */
#define PSP_NOISE_TAPS			105

/* SceNgsGeneratorSettings */
typedef struct NgsGeneratorUserParams {
	SceNgsParamsDescriptor desc;
	uint32_t eGeneratorMode;
	int32_t nFrequency;
	float fAmplitude;
	float fPulseWidth;
	uint32_t uSampleOffset;
	uint32_t uPhaseAngle;
} NgsGeneratorUserParams;

typedef struct NgsGeneratorSettings {
	int32_t frequency;
	uint32_t position;		/* samples generated */
	int32_t amplitude;		/* 8.24 */
	uint32_t mode;
	union {
		/* a rotation: sin and cos of the phase, turned by the step's */
		struct {
			int32_t sine;
			int32_t cosine;
			int32_t stepSine;
			int32_t stepCosine;
		} sine;
		/* four LCGs; one while each value lasts more than a sample */
		struct {
			uint32_t seed;
			uint32_t reserved;
			uint32_t seed4;
			uint32_t seed3;
			int32_t phase;
			int32_t value;	/* the held value, or the second LCG */
			int32_t step;
		} noise;
		/* the PSP's: a 15-bit shift register clocked through a pattern */
		struct {
			uint32_t pattern;
			int32_t period;
			int32_t counter;
			uint32_t shift;
		} pspNoise;
		struct {
			int32_t highSamples;
		} pulse;
	};
} NgsGeneratorSettings;

/* sin(x), x in 8.24 radians: its series to x^9 */
static int32_t generatorSine(int32_t x)
{
	int32_t square = ngsMultiply(x, x), sum;

	sum = ngsMultiply(0, square);
	sum = ngsMultiply(sum + 46, square);		/* 1/9! */
	sum = ngsMultiply(sum - 3328, square);		/* 1/7! */
	sum = ngsMultiply(sum + 0x22222, square);	/* 1/5! */
	sum = ngsMultiply(sum - 0x2aaaaa, square);	/* 1/3! */
	return ngsMultiply(sum + NGS_ONE, x);
}

/* cos(x): its series to x^10 */
static int32_t generatorCosine(int32_t x)
{
	int32_t square = ngsMultiply(x, x), sum;

	sum = ngsMultiply(-4, square);			/* 1/10! */
	sum = ngsMultiply(sum + 415, square);		/* 1/8! */
	sum = ngsMultiply(sum - 23300, square);		/* 1/6! */
	sum = ngsMultiply(sum + 0xaaaaa, square);	/* 1/4! */
	sum = ngsMultiply(sum - 0x7fffff, square);	/* 1/2! */
	return sum + NGS_ONE;
}

int32_t ngsGeneratorInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsGeneratorUserParams *user = (NgsGeneratorUserParams *)params->userParams[0];
	NgsGeneratorSettings *settings = (NgsGeneratorSettings *)params->data[0];
	int32_t step, omega;

	if ((params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE) ||
		user->eGeneratorMode != settings->mode) {
		if (user->eGeneratorMode == SCE_NGS_GENERATOR_SINE) {
			settings->sine.sine = 0;
			settings->sine.cosine = NGS_ONE;
		}
		settings->position = user->uSampleOffset;
	}
	settings->mode = user->eGeneratorMode;
	settings->frequency = user->nFrequency;
	settings->amplitude = (int32_t)(user->fAmplitude * 16777216.0f);
	/* cycles per sample, 8.24 */
	step = (user->nFrequency << 13) / (int32_t)params->sampleRate << 11;

	if (user->eGeneratorMode == SCE_NGS_GENERATOR_SINE) {
		omega = ngsMultiply(TWO_PI, step);
		settings->sine.stepSine = generatorSine(omega);
		settings->sine.stepCosine = generatorCosine(omega);
	} else if (user->eGeneratorMode == SCE_NGS_GENERATOR_NOISE_PSP) {
		settings->pspNoise.counter = 0;
		settings->pspNoise.shift = 0;
		settings->pspNoise.pattern = 0xffffffff;
		settings->pspNoise.period = PSP_NOISE_PERIOD >> (user->nFrequency >> 2);
		if (settings->pspNoise.period != 0)
			settings->pspNoise.pattern = ((PSP_NOISE_PATTERNS >>
				((user->nFrequency & 3) * 4)) & 0xff) * 0x01010101;
		else
			settings->pspNoise.period++;
	} else if (user->eGeneratorMode == SCE_NGS_GENERATOR_NOISE) {
		settings->noise.step = step;
		settings->noise.phase = 0;
	} else if (user->eGeneratorMode == SCE_NGS_GENERATOR_PULSE) {
		settings->pulse.highSamples = (int32_t)((float)((int32_t)params->sampleRate /
			user->nFrequency) * user->fPulseWidth);
	}
	return 0;
}

static int32_t lcgNext(uint32_t *seed, int32_t amplitude)
{
	*seed = *seed * LCG_MULTIPLIER + LCG_INCREMENT;
	return ngsMultiply((int32_t)(*seed << 7) >> 7, amplitude);
}

/*
 * White noise. At a sample rate or above, four LCGs give four samples at a
 * time (IVC2 lanes in the original, whose order in memory is our reading of
 * two pack instructions binutils does not know); below it, each value is
 * held.
 */
static void generatorNoise(int32_t *output, NgsGeneratorSettings *settings, int32_t samples,
	NgsModuleParams *params)
{
	int32_t step = min(settings->noise.step, NGS_ONE), i;
	uint32_t value;

	if (step == NGS_ONE) {
		for (i = 0; i < samples; i += NOISE_LANES) {
			*output++ = lcgNext(&settings->noise.seed, settings->amplitude);
			value = (uint32_t)settings->noise.value;
			*output++ = lcgNext(&value, settings->amplitude);
			settings->noise.value = (int32_t)value;
			*output++ = lcgNext(&settings->noise.seed3, settings->amplitude);
			*output++ = lcgNext(&settings->noise.seed4, settings->amplitude);
		}
		return;
	}
	for (i = 0; i < samples; i++) {
		if (settings->noise.phase >= NGS_ONE) {
			settings->noise.phase -= NGS_ONE;
			settings->noise.value = lcgNext(&settings->noise.seed, settings->amplitude);
		}
		output[i] = settings->noise.value;
		settings->noise.phase += step;
	}
}

/* the PSP's noise: the register's low half, stepped when the pattern says */
static void generatorPspNoise(int32_t *output, NgsGeneratorSettings *settings, int32_t samples,
	NgsModuleParams *params)
{
	int32_t counter = settings->pspNoise.counter, i;
	uint32_t shift = settings->pspNoise.shift;
	uint32_t pattern = settings->pspNoise.pattern;

	for (i = 0; i < samples; i++) {
		output[i] = ngsMultiply((int16_t)shift << 9, settings->amplitude);
		if (--counter >= 0)
			continue;
		counter = settings->pspNoise.period;
		pattern = (pattern >> 1) | (pattern << 31);
		if ((int32_t)pattern >= 0)
			continue;
		shift <<= 1;
		shift += ((shift >> 15) ^ (PSP_NOISE_TAPS >> ((shift >> 10) & 7))) & 1;
	}
	settings->pspNoise.counter = counter;
	settings->pspNoise.shift = shift;
	settings->pspNoise.pattern = pattern;
}

/* 0x87e960: the rotation's sine; only the last channel keeps the turn, renormalized */
void ngsGeneratorSine(int32_t *output, void *data, uint32_t samples, int32_t last)
{
	NgsGeneratorSettings *settings = (NgsGeneratorSettings *)data;
	int32_t sine = settings->sine.sine, cosine = settings->sine.cosine, scale;
	uint32_t i;

	for (i = 0; i < samples; i++) {
		int32_t next = ngsMultiply(sine, settings->sine.stepCosine) +
			ngsMultiply(cosine, settings->sine.stepSine);

		output[i] = ngsMultiply(sine, settings->amplitude);
		cosine = ngsMultiply(cosine, settings->sine.stepCosine) -
			ngsMultiply(sine, settings->sine.stepSine);
		sine = next;
	}
	if (!last)
		return;
	/* one Newton step toward length 1: (3 - |v|^2) / 2 */
	scale = (3 * NGS_ONE - (ngsMultiply(cosine, cosine) + ngsMultiply(sine, sine))) / 2;
	settings->sine.sine = ngsMultiply(scale, sine);
	settings->sine.cosine = ngsMultiply(scale, cosine);
}

/* 8.24 of the period per sample, or the period in samples times unit */
static int32_t waveStep(NgsGeneratorSettings *settings, NgsModuleParams *params, float span,
	float unit)
{
	float period = (float)(int32_t)params->sampleRate / (float)settings->frequency;

	if (span)
		return (int32_t)(span / period * unit);
	return (int32_t)(period * unit);
}

/* four phases, a step apart, each wrapped once */
static void waveLanes(int32_t *phase, uint32_t position, int32_t step, int32_t period)
{
	int32_t k;

	for (k = 0; k < WAVE_LANES; k++) {
		phase[k] = position + k * step;
		if (phase[k] >= period)
			phase[k] -= period;
	}
}

/* 0x87e648: +amplitude for the pulse's samples, then -amplitude; phase in 1/4096 */
uint32_t ngsGeneratorPulse(int32_t *output, void *data, uint32_t samples,
	NgsModuleParams *params)
{
	NgsGeneratorSettings *settings = (NgsGeneratorSettings *)data;
	int32_t period = waveStep(settings, params, 0, PULSE_UNIT);
	int32_t high = settings->pulse.highSamples * PULSE_UNIT, phase[WAVE_LANES], k;
	uint32_t i;

	for (k = 0; k < WAVE_LANES; k++) {
		phase[k] = settings->position + k * PULSE_UNIT;
		if (k && period < phase[k])
			phase[k] -= period;
	}
	for (i = 0; i < samples; i += WAVE_LANES)
		for (k = 0; k < WAVE_LANES; k++) {
			*output++ = phase[k] >= high ? -settings->amplitude : settings->amplitude;
			phase[k] += WAVE_LANES * PULSE_UNIT;
			if (phase[k] - period >= 0)
				phase[k] -= period;
		}
	return phase[0];
}

/* 0x87ebd0: phase 0 to 2, rising 2p - 1, then falling 1 - 2(p - 1) */
uint32_t ngsGeneratorTriangle(int32_t *output, void *data, uint32_t samples,
	NgsModuleParams *params)
{
	NgsGeneratorSettings *settings = (NgsGeneratorSettings *)data;
	int32_t step = waveStep(settings, params, 2.0f, NGS_ONE), phase[WAVE_LANES], k;
	uint32_t i;

	waveLanes(phase, settings->position, step, 2 * NGS_ONE);
	for (i = 0; i < samples; i += WAVE_LANES)
		for (k = 0; k < WAVE_LANES; k++) {
			int32_t value = phase[k] >= NGS_ONE ? NGS_ONE - 2 * (phase[k] - NGS_ONE) :
				2 * phase[k] - NGS_ONE;

			*output++ = ngsMultiply(value, settings->amplitude);
			phase[k] += WAVE_LANES * step;
			if (phase[k] >= 2 * NGS_ONE)
				phase[k] -= 2 * NGS_ONE;
		}
	return phase[0];
}

/* 0x87eef8: phase 0 to 1, 2p - 1 */
uint32_t ngsGeneratorSaw(int32_t *output, void *data, uint32_t samples,
	NgsModuleParams *params)
{
	NgsGeneratorSettings *settings = (NgsGeneratorSettings *)data;
	int32_t step = waveStep(settings, params, 1.0f, NGS_ONE), phase[WAVE_LANES], k;
	uint32_t i;

	waveLanes(phase, settings->position, step, NGS_ONE);
	for (i = 0; i < samples; i += WAVE_LANES)
		for (k = 0; k < WAVE_LANES; k++) {
			*output++ = ngsMultiply(2 * phase[k] - NGS_ONE, settings->amplitude);
			phase[k] += WAVE_LANES * step;
			if (phase[k] >= NGS_ONE)
				phase[k] -= NGS_ONE;
		}
	return phase[0];
}

int32_t ngsGeneratorProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsGeneratorSettings *settings = (NgsGeneratorSettings *)params->data[0];
	uint32_t samples = outputs->buffers[0]->samples;
	int32_t channel, last, position;
	int32_t *data;

	if (params->flags & NGS_RUN_BYPASSED) {
		params->flags &= ~NGS_RUN_ENABLED;
		return 0;
	}
	for (channel = 0; channel < outputs->buffers[0]->channels; channel++) {
		outputs->buffers[0]->channelMask |= 1 << channel;
		cacheInvalidateFlush((uint32_t)outputs->buffers[0]->channelData[channel],
			outputs->buffers[0]->samples * NGS_SAMPLE_SIZE);
	}
	if (params->flags & NGS_RUN_BYPASSED)
		return 0;

	/* each channel is the same wave; the last one moves the position */
	for (channel = 0; channel < outputs->buffers[0]->channels; channel++) {
		last = channel == outputs->buffers[0]->channels - 1;
		data = (int32_t *)outputs->buffers[0]->channelData[channel];
		switch (settings->mode) {
		case SCE_NGS_GENERATOR_SINE:
			ngsGeneratorSine(data, settings, samples, last);
			break;
		case SCE_NGS_GENERATOR_PULSE:
			position = ngsGeneratorPulse(data, settings, samples, params);
			if (last)
				settings->position = position;
			continue;
		case SCE_NGS_GENERATOR_SAW:
			position = ngsGeneratorSaw(data, settings, samples, params);
			if (last)
				settings->position = position;
			continue;
		case SCE_NGS_GENERATOR_TRIANGLE:
			position = ngsGeneratorTriangle(data, settings, samples, params);
			if (last)
				settings->position = position;
			continue;
		case SCE_NGS_GENERATOR_NOISE:
			generatorNoise(data, settings, samples, params);
			break;
		case SCE_NGS_GENERATOR_NOISE_PSP:
			generatorPspNoise(data, settings, samples, params);
			break;
		default:
			ngsGeneratorSine(data, settings, samples, last);
			break;
		}
		if (last)
			settings->position += samples;
	}
	outputs->buffers[0]->flags = NGS_BUFFER_VALID;
	return 0;
}
