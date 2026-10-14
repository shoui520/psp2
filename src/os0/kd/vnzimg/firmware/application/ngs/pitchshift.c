/*
 * The pitch shifter (SCE_NGS_PITCHSHIFT_ID): each channel's input goes
 * round a 1024-sample ring, read back by two grains half the ring apart
 * whose delay drifts by 1 - ratio a sample. Each grain is a 4-point cubic
 * (as the resampler's) at its fraction, weighed by a sine window over its
 * cycle, so one fades in as the other wraps.
 */
#include "ngs.h"

#define CENTS_PER_OCTAVE	1200.0f
#define GRAIN_STATE_SIZE	8200

/* SceNgsPitchShiftParams */
typedef struct NgsPitchShiftUserParams {
	SceNgsParamsDescriptor desc;
	float fPitchOffsetInCents;
} NgsPitchShiftUserParams;

typedef struct NgsPitchShiftSettings {
	int32_t ratio;			/* 8.24 */
	int32_t position;
	int32_t phase;			/* 8.24, from 32.0; the kernel keeps it */
	uint8_t grains[GRAIN_STATE_SIZE];
} NgsPitchShiftSettings;

int32_t ngsPitchShiftInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsPitchShiftUserParams *user = (NgsPitchShiftUserParams *)params->userParams[0];
	NgsPitchShiftSettings *settings = (NgsPitchShiftSettings *)params->data[0];

	if (params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE) {
		settings->phase = 32 * NGS_ONE;
		settings->position = 0;
		memset(settings->grains, 0, sizeof(settings->grains));
	}
	/* 2^(cents / 1200) */
	settings->ratio = ngsPower(2 * NGS_ONE,
		(int32_t)(user->fPitchOffsetInCents / CENTS_PER_OCTAVE * 16777216.0f));
	return 0;
}

#define RING_SIZE	1024
#define RING_MASK	(RING_SIZE - 1)
#define RING_BYTES	4104		/* a channel's ring, with room to align it */
#define PHASE_MASK	0x3fffffff	/* a grain's cycle */
#define HALF_CYCLE	0x20000000
#define WINDOW_STEPS	20

/* 8.24 */
#define EVEN_INNER	0x6c606c
#define EVEN_OUTER	0x13a1c7
#define ODD_INNER	0x42e1f7
#define ODD_OUTER	0x3f6f16
#define CURVE_INNER	((int32_t)0xffc95c03)
#define CURVE_OUTER	0x368985

/* sin(pi k / 20) / sqrt 2, Q31 */
static const int32_t pitchWindow[WINDOW_STEPS + 1] = {
	0, 237545472, 469241856, 689383424, 892551168, 1073739776, 1228490752,
	1352990720, 1444177920, 1499801600, 1518497792, 1499801600, 1444177920,
	1352990720, 1228490752, 1073739776, 892551168, 689383424, 469241856,
	237545472, 0,
};

static int32_t dot(int32_t a, int32_t p, int32_t b, int32_t q)
{
	return (int32_t)(((int64_t)a * p + (int64_t)b * q) >> 24);
}

/* the window at a phase, Q31 times 8.24 (cpmulsl), so 8.24 */
static int32_t grainWindow(uint32_t phase)
{
	uint32_t t = (phase >> 10) * WINDOW_STEPS;
	int32_t fraction = (int32_t)((t & 0xfffff) << 4), i = t >> 20;

	return (int32_t)(((int64_t)pitchWindow[i] * (NGS_ONE - fraction) +
		(int64_t)pitchWindow[i + 1] * fraction) >> 31);
}

/* four samples from `index` on, at u = fraction - 1/2 */
static int32_t grainSample(const int32_t *ring, int32_t index, int32_t u)
{
	int32_t x0 = ring[index & RING_MASK], x1 = ring[(index + 1) & RING_MASK];
	int32_t x2 = ring[(index + 2) & RING_MASK], x3 = ring[(index + 3) & RING_MASK];
	int32_t inner = x2 + x1, outer = x3 + x0;
	int32_t even = dot(inner, EVEN_INNER, outer, EVEN_OUTER);
	int32_t odd = dot(x2 - x1, ODD_INNER, x3 - x0, ODD_OUTER);
	int32_t curve = dot(inner, CURVE_INNER, outer, CURVE_OUTER);

	return ngsMultiply(ngsMultiply(curve, u) + odd, u) + even;
}

/* 0x87ff10 */
static void pitchShiftChannel(const int32_t *input, int32_t *output, uint32_t samples,
	NgsPitchShiftSettings *settings, int32_t channel)
{
	uint32_t ring = (uint32_t)settings->grains + channel * RING_BYTES;
	int32_t step = (settings->ratio - NGS_ONE) >> 4, position = settings->position;
	uint32_t i;

	if (ring & 7)
		ring += 4;
	for (i = 0; i < samples; i++) {
		uint32_t phase = (uint32_t)settings->phase;
		uint32_t other = (phase + HALF_CYCLE) & PHASE_MASK;
		int32_t index = (int32_t)(phase >> 20) + position - 1;
		int32_t u = ((int32_t)(phase & 0xfffff) - 0x80000) << 4;
		int32_t a = grainSample((int32_t *)ring, index, u);
		int32_t b = grainSample((int32_t *)ring, index + RING_SIZE / 2, u);

		settings->phase = (int32_t)((phase + step) & PHASE_MASK);
		output[i] = ngsMultiply(a, grainWindow(phase)) + ngsMultiply(b, grainWindow(other));
		((int32_t *)ring)[position] = input[i];
		position = (position + 1) & RING_MASK;
	}
	settings->position = position;
}

/* 0x8805f8: a stereo input runs both channels from the same state */
int32_t ngsPitchShiftRun(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsPitchShiftSettings *settings = (NgsPitchShiftSettings *)params->data[0];
	NgsBuffer *input = inputs->buffers[0];
	int32_t position, phase;

	if (!(input->flags & NGS_BUFFER_VALID)) {
		params->flags &= ~NGS_RUN_ENABLED;
		goto out;
	}
	if (params->flags & NGS_RUN_BYPASSED) {
		if (input->flags & NGS_BUFFER_END) {
			outputs->buffers[0]->flags |= NGS_BUFFER_END;
			params->flags = 0;
		}
		return 0;
	}
	if (input->channels != 1) {
		phase = settings->phase;
		position = settings->position;
		pitchShiftChannel((int32_t *)input->channelData[0],
			(int32_t *)outputs->buffers[0]->channelData[0], input->samples, settings, 0);
		settings->phase = phase;
		settings->position = position;
		pitchShiftChannel((int32_t *)input->channelData[1],
			(int32_t *)outputs->buffers[0]->channelData[1], input->samples, settings, 1);
	} else {
		pitchShiftChannel((int32_t *)input->channelData[0],
			(int32_t *)outputs->buffers[0]->channelData[0], input->samples, settings, 0);
	}
	outputs->buffers[0]->flags = NGS_BUFFER_VALID;
	if (input->flags & NGS_BUFFER_END) {
		outputs->buffers[0]->flags |= NGS_BUFFER_END;
		params->flags = 0;
	}
out:
	if (outputs->count)
		outputs->buffers[0]->channelMask = input->channelMask;
	return 0;
}

int32_t ngsPitchShiftProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	return ngsPitchShiftRun(inputs, outputs, params);
}
