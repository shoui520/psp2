/*
 * The delay (SCE_NGS_DELAY_ID): four taps on a one-second line per
 * channel, each with feedback, a one-pole filter and a delay modulated by
 * a sine LFO; the VLIW kernel runs them. Each channel has its own
 * parameters. After a key-off the module runs on until the output has been
 * quiet for a second.
 */
#include "ngs.h"

#define SCE_NGS_DELAY_PARAMS_STRUCT_ID		SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_DELAY_ID, 1, 1)
#define SCE_NGS_DELAY_MAX_TAPS			4
#define SCE_NGS_DELAY_FILTER_MODE_OFF		0
#define SCE_NGS_DELAY_FILTER_MODE_LOWPASS_ONEPOLE 1
#define SCE_NGS_DELAY_FILTER_MODE_HIGHPASS_ONEPOLE 2
#define SCE_NGS_DELAY_FILTER_MODE_ALLPASS	3

#define MAX_DELAY_SAMPLES			48000
#define FIXED					16777216.0f
#define TWO_PI					6.2831855f
#define DEGREES					(16777216.0f * 3.14159265f / 180.0f)
#define PI					0x3243f6c	/* 8.24 */
#define FADE_SAMPLES				64
/* output quieter than this (8.24) counts as silence; every 17th sample is looked at */
#define QUIET_LEVEL				16778
#define QUIET_STRIDE				17

/* SceNgsDelayTap */
typedef struct NgsDelayUserTap {
	float fDelayMillisecs;
	float fVolume;
	float fFeedback;
	uint32_t eFilterMode;
	float fCutoff;
	float fPhaseOffsetDeg;
	float fModWidthMillisecs;
} NgsDelayUserTap;

/* SceNgsDelayParams, one for each channel */
typedef struct NgsDelayUserParams {
	SceNgsParamsDescriptor desc;
	float fDryVol;
	float fModRate;
	NgsDelayUserTap taps[SCE_NGS_DELAY_MAX_TAPS];
} NgsDelayUserParams;

typedef struct NgsDelayTap {
	int32_t delay;			/* samples, 20.12 */
	int32_t volume;
	int32_t feedback;
	int32_t width;			/* of the modulation, samples, 20.12 */
	int32_t modulation;		/* cleared here, kept by the kernel */
	int32_t phaseCosine;		/* of the tap's LFO phase offset */
	int32_t phaseSine;
	int32_t reserved;
	int32_t filter[3];		/* one-pole: input, last input, last output */
	int32_t filterState[2];
} NgsDelayTap;

typedef struct NgsDelaySettings {
	int32_t maxDelay;		/* samples */
	int32_t position;
	int32_t dry;
	int32_t lfoSine;		/* the LFO, turned once a block */
	int32_t lfoCosine;
	int32_t stepCosine;		/* its turn in a sample */
	int32_t stepSine;
	int32_t blockCosine;		/* and in a block */
	int32_t blockSine;
	NgsDelayTap taps[SCE_NGS_DELAY_MAX_TAPS];
	int32_t line[MAX_DELAY_SAMPLES];
	int32_t quietSamples;		/* after a key-off */
	int16_t finishing;
	int16_t bypassed;
} NgsDelaySettings;

static int32_t delayFilterDesign(const NgsDelayUserTap *tap, int32_t *filter, uint32_t sampleRate)
{
	int32_t omega, sine, cosine, tangent, k;

	omega = ngsMultiply((int32_t)(tap->fCutoff / (float)(int32_t)sampleRate * FIXED), PI);
	sine = ngsSin(omega);
	cosine = ngsCos(omega);
	if (cosine == 0)
		cosine = 1;
	tangent = ngsDivide(sine, cosine);
	k = ngsDivide(tangent - NGS_ONE, tangent + NGS_ONE);

	switch (tap->eFilterMode) {
	case SCE_NGS_DELAY_FILTER_MODE_OFF:
		filter[0] = NGS_ONE;
		filter[1] = 0;
		filter[2] = 0;
		return 0;
	case SCE_NGS_DELAY_FILTER_MODE_LOWPASS_ONEPOLE:
		filter[0] = k + NGS_ONE;
		filter[1] = 0;
		filter[2] = k;
		return 0;
	case SCE_NGS_DELAY_FILTER_MODE_HIGHPASS_ONEPOLE:
		filter[0] = (NGS_ONE - k) >> 1;
		filter[1] = -filter[0];
		filter[2] = k;
		return 0;
	case SCE_NGS_DELAY_FILTER_MODE_ALLPASS:
		filter[0] = k;
		filter[1] = NGS_ONE;
		filter[2] = k;
		return 0;
	}
	return -1;
}

/* the line and the filters emptied */
static void delayReset(NgsDelaySettings *settings)
{
	int32_t tap;

	memset(settings->line, 0, sizeof(settings->line));
	for (tap = 0; tap < SCE_NGS_DELAY_MAX_TAPS; tap++) {
		settings->taps[tap].filterState[0] = 0;
		settings->taps[tap].filterState[1] = 0;
	}
}

int32_t ngsDelayInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsDelayUserParams *users = (NgsDelayUserParams *)params->userParams[0], *user;
	NgsDelaySettings *settings;
	int32_t result = 0, channel, tapIndex, samplesPerMs, phase, delay, omega;
	NgsDelayUserTap *userTap;
	NgsDelayTap *tap;

	for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
		settings = (NgsDelaySettings *)params->data[0] + channel;
		user = users + channel;
		if (user->desc.id != SCE_NGS_DELAY_PARAMS_STRUCT_ID)
			return SCE_NGS_ERROR_INVALID_PARAM;
		if (params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE) {
			settings->maxDelay = MAX_DELAY_SAMPLES;
			settings->position = 0;
			settings->lfoSine = 0;
			settings->lfoCosine = NGS_ONE;
			delayReset(settings);
			settings->quietSamples = 0;
			settings->finishing = 0;
			settings->bypassed = 0;
		}
		settings->dry = (int32_t)(user->fDryVol * FIXED);

		if (!(user->fModRate > 0.0f)) {
			settings->stepCosine = NGS_ONE;
			settings->stepSine = 0;
			settings->blockCosine = NGS_ONE;
			settings->blockSine = 0;
		} else {
			omega = (int32_t)(TWO_PI / ((float)(int32_t)params->sampleRate / user->fModRate) *
				FIXED);
			settings->stepCosine = ngsCos(omega);
			settings->stepSine = ngsSin(omega);
			omega = (int32_t)(TWO_PI / ((float)(int32_t)params->sampleRate / user->fModRate /
				(float)(int32_t)outputs->buffers[0]->samples) * FIXED);
			settings->blockCosine = ngsCos(omega);
			settings->blockSine = ngsSin(omega);
		}

		for (tapIndex = 0; tapIndex < SCE_NGS_DELAY_MAX_TAPS; tapIndex++) {
			samplesPerMs = (int32_t)params->sampleRate / 1000;
			userTap = &user->taps[tapIndex];
			tap = &settings->taps[tapIndex];
			phase = (int32_t)(userTap->fPhaseOffsetDeg * DEGREES);
			delay = (int32_t)(userTap->fDelayMillisecs * 4096.0f) * samplesPerMs >> 12;
			if (settings->maxDelay < delay) {
				delay = settings->maxDelay;
				result = SCE_NGS_ERROR_INVALID_PARAM;
			} else if (delay < 0) {
				delay = 0;
				result = SCE_NGS_ERROR_INVALID_PARAM;
			}
			delayFilterDesign(userTap, tap->filter, params->sampleRate);
			tap->width = (int32_t)(userTap->fModWidthMillisecs * 4096.0f) * samplesPerMs;
			tap->delay = delay << 12;
			tap->volume = (int32_t)(userTap->fVolume * FIXED);
			tap->modulation = 0;
			tap->feedback = (int32_t)(userTap->fFeedback * FIXED);
			tap->phaseCosine = ngsCos(phase);
			tap->phaseSine = ngsSin(phase);
			/* the modulation stays inside the line */
			if (tap->width + tap->delay >= (MAX_DELAY_SAMPLES << 12) + 1)
				tap->width = (MAX_DELAY_SAMPLES << 12) - tap->delay;
			if (tap->delay - tap->width < 0)
				tap->width = tap->delay;
		}
	}
	return result;
}

#define MIN_DELAY	(3 << 12)	/* the modulated delay's floor, 20.12 */

/* (a p + b q) >> 24 in one 64-bit sum */
static int32_t dot(int32_t a, int32_t p, int32_t b, int32_t q)
{
	return (int32_t)(((int64_t)a * p + (int64_t)b * q) >> 24);
}

static int32_t wrapLine(int32_t index, int32_t size)
{
	return index >= size ? index - size : index;
}

/*
 * 0x87f1d0: the dry part first; then for each sample each tap's LFO turns
 * by a sample, its delay is modulated, the line is read with linear
 * interpolation, filtered (one pole) and summed into the output by volume
 * and into the line by feedback, with the input when active.
 *
 * The original's filter sums in two IVC2 lanes: taps 0 and 1 get their own
 * terms and the state terms of taps 2 and 3, which keep only b0 (cpmadla1
 * and cpmsbla1 where cpmadua1 and cpmsbua1 were meant). Kept as it is.
 */
void ngsDelayRun(void *data, const int32_t *input, int32_t *output, uint32_t samples,
	int32_t active)
{
	NgsDelaySettings *settings = (NgsDelaySettings *)data;
	int32_t sine[SCE_NGS_DELAY_MAX_TAPS], cosine[SCE_NGS_DELAY_MAX_TAPS];
	int32_t tapOut[SCE_NGS_DELAY_MAX_TAPS], filtered[SCE_NGS_DELAY_MAX_TAPS];
	int32_t position = settings->position, size = settings->maxDelay, k;
	uint32_t i;

	for (k = 0; k < SCE_NGS_DELAY_MAX_TAPS; k++) {
		NgsDelayTap *tap = &settings->taps[k];

		sine[k] = ngsMultiply(settings->lfoCosine, tap->phaseSine) +
			ngsMultiply(settings->lfoSine, tap->phaseCosine);
		cosine[k] = ngsMultiply(settings->lfoCosine, tap->phaseCosine) -
			ngsMultiply(settings->lfoSine, tap->phaseSine);
	}

	for (i = 0; i < samples; i++)
		output[i] = ngsMultiply(input[i], settings->dry);

	for (i = 0; i < samples; i++) {
		int32_t wet = 0, feedback = 0;

		for (k = 0; k < SCE_NGS_DELAY_MAX_TAPS; k++) {
			NgsDelayTap *tap = &settings->taps[k];
			int32_t turned = dot(sine[k], settings->stepCosine, cosine[k],
				settings->stepSine);
			int32_t delay, whole, fraction, older, newer;

			cosine[k] = (int32_t)(((int64_t)cosine[k] * settings->stepCosine -
				(int64_t)sine[k] * settings->stepSine) >> 24);
			sine[k] = turned;

			delay = tap->delay + (int32_t)(((int64_t)tap->width * (sine[k] >> 12)) >> 12);
			delay = max(delay, MIN_DELAY);
			whole = delay >> 12;
			fraction = (delay - (whole << 12)) << 12;
			older = wrapLine(position - whole + size - 1, size);
			newer = wrapLine(position - whole + size, size);
			tapOut[k] = dot(settings->line[older], fraction, settings->line[newer],
				NGS_ONE - fraction);
		}

		for (k = 0; k < 2; k++) {
			NgsDelayTap *tap = &settings->taps[k], *other = &settings->taps[k + 2];

			filtered[k] = (int32_t)(((int64_t)tapOut[k] * tap->filter[0] +
				(int64_t)tap->filterState[0] * tap->filter[1] +
				(int64_t)other->filterState[0] * other->filter[1] -
				(int64_t)tap->filterState[1] * tap->filter[2] -
				(int64_t)other->filterState[1] * other->filter[2]) >> 24);
		}
		for (k = 2; k < SCE_NGS_DELAY_MAX_TAPS; k++)
			filtered[k] = (int32_t)(((int64_t)tapOut[k] *
				settings->taps[k].filter[0]) >> 24);

		/* lanes 0 and 2, 1 and 3 summed before the shift */
		for (k = 0; k < 2; k++) {
			NgsDelayTap *tap = &settings->taps[k], *other = &settings->taps[k + 2];

			wet += dot(filtered[k], tap->volume, filtered[k + 2], other->volume);
			feedback += dot(filtered[k], tap->feedback, filtered[k + 2],
				other->feedback);
		}
		for (k = 0; k < SCE_NGS_DELAY_MAX_TAPS; k++) {
			settings->taps[k].filterState[0] = tapOut[k];
			settings->taps[k].filterState[1] = filtered[k];
		}

		output[i] += wet;
		settings->line[position] = feedback + input[i] * active;
		if (++position >= size)
			position -= size;
	}

	settings->position += samples;
	if (settings->position >= settings->maxDelay)
		settings->position -= settings->maxDelay;
}

/* 0x87fd90: the dry part only, four samples at a time */
void ngsDelayBypass(void *data, const int32_t *input, int32_t *output, uint32_t samples)
{
	NgsDelaySettings *settings = (NgsDelaySettings *)data;
	int32_t i;

	for (i = 0; i < (int32_t)samples; i += 4) {
		output[i] = ngsMultiply(input[i], settings->dry);
		output[i + 1] = ngsMultiply(input[i + 1], settings->dry);
		output[i + 2] = ngsMultiply(input[i + 2], settings->dry);
		output[i + 3] = ngsMultiply(input[i + 3], settings->dry);
	}
}

/* the dry input added to the wet output, whose end is faded out */
static void delayBypassStart(NgsDelaySettings *settings, const int32_t *input, int32_t *output,
	int32_t samples)
{
	int32_t step = ngsFadeStart / FADE_SAMPLES, i;
	int32_t *tail = output + (samples - FADE_SAMPLES);

	for (i = 0; i < FADE_SAMPLES; i++)
		tail[i] = ngsMultiply(tail[i], ngsWindow[ngsFadeStart + i * step]);
	for (i = 0; i < samples; i++)
		output[i] += ngsMultiply(input[i], settings->dry);
	delayReset(settings);
}

int32_t ngsDelayProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsDelaySettings *settings;
	int32_t channel, active, bypassStart, samples, sine, cosine, gain, done, i;
	int32_t *input, *output;

	outputs->buffers[0]->channelMask = 0;
	for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
		active = 0;
		settings = (NgsDelaySettings *)params->data[0] + channel;
		if ((inputs->buffers[0]->flags & NGS_BUFFER_VALID) &&
			((inputs->buffers[0]->channelMask >> channel) & 1)) {
			active = 1;
			outputs->buffers[0]->channelMask |= 1 << channel;
		}
		if (params->voiceState & NGS_VOICE_STATE_KEYED_OFF) {
			settings->finishing = 1;
			ngsFadeOut(inputs);
		} else if (settings->finishing) {
			active = 0;
		}

		bypassStart = 0;
		if (params->flags & NGS_RUN_BYPASSED) {
			if (settings->bypassed == 0) {
				bypassStart = 1;
				settings->bypassed = 1;
			}
		} else if (settings->bypassed == 1) {
			settings->bypassed = 0;
		}

		input = (int32_t *)inputs->buffers[0]->channelData[channel];
		samples = inputs->buffers[0]->samples;
		output = (int32_t *)outputs->buffers[0]->channelData[channel];
		if (settings->bypassed && !bypassStart) {
			ngsDelayBypass(settings, input, output, samples);
		} else {
			if (bypassStart)
				active = 0;
			ngsDelayRun(settings, input, output, samples, active);
			if (bypassStart)
				delayBypassStart(settings, input, output, samples);
		}

		/* the LFO turned by a block, its length kept at one */
		sine = ngsMultiply(settings->lfoSine, settings->blockCosine) +
			ngsMultiply(settings->lfoCosine, settings->blockSine);
		cosine = ngsMultiply(settings->lfoCosine, settings->blockCosine) -
			ngsMultiply(settings->lfoSine, settings->blockSine);
		gain = (3 * NGS_ONE - (ngsMultiply(sine, sine) + ngsMultiply(cosine, cosine))) >> 1;
		settings->lfoSine = ngsMultiply(sine, gain);
		settings->lfoCosine = ngsMultiply(cosine, gain);

		if (settings->finishing == 0)
			continue;
		for (i = 0; i < samples; i += QUIET_STRIDE) {
			if (abs(output[i]) >= QUIET_LEVEL) {
				settings->quietSamples = 0;
				break;
			}
		}
		if (i >= samples)
			settings->quietSamples += samples;
	}

	outputs->buffers[0]->flags = NGS_BUFFER_VALID;
	settings = (NgsDelaySettings *)params->data[0];
	if (settings->finishing == 0)
		return 0;
	/* done when every channel has been quiet for a second */
	done = 1;
	for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
		outputs->buffers[0]->channelMask |= 1 << channel;
		if (settings[channel].quietSamples < (int32_t)params->sampleRate)
			done = 0;
	}
	if (done)
		outputs->buffers[0]->flags |= NGS_BUFFER_END;
	else
		params->flags |= NGS_RUN_KEEP_PENDING;
	return 0;
}
