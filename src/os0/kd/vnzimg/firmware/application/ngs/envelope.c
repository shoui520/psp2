/*
 * The envelope (SCE_NGS_ENVELOPE_ID): its points in samples and 8.24. Each
 * run steps through the points 32 samples at a time, takes the gain at each
 * step (linear or cubic between two points, times the release's scale) and
 * scales the channels by the gains, ramped across each step.
 */
#include "ngs.h"

#define SCE_NGS_ENVELOPE_PARAMS_STRUCT_ID	SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_ENVELOPE_ID, 1, 1)
#define SCE_NGS_ENVELOPE_MAX_POINTS		4
#define SCE_NGS_ENVELOPE_LINEAR			0
#define SCE_NGS_ENVELOPE_CUBIC			1
#define SCE_NGS_ENVELOPE_LOOP_END		(-2)	/* stop at the last point */
#define SCE_NGS_ENVELOPE_LOOP_RELEASE		(-3)	/* release at the last point */

#define ENVELOPE_STEP		32	/* samples between two gains */
#define ENVELOPE_STEPS		64	/* the most in one run, plus one */
#define POSITION_SHIFT		9	/* a position times rate, in 8.24 */
#define QUICK_RELEASE		0x80000	/* 1/32 a step without a release time */

/* SceNgsEnvelopePoint, SceNgsEnvelopeParams */
typedef struct NgsEnvelopeUserPoint {
	uint32_t uMsecsToNextPoint;
	float fAmplitude;
	uint32_t eCurveType;
} NgsEnvelopeUserPoint;

typedef struct NgsEnvelopeUserParams {
	SceNgsParamsDescriptor desc;
	NgsEnvelopeUserPoint envelopePoints[SCE_NGS_ENVELOPE_MAX_POINTS];
	uint32_t uReleaseMsecs;
	uint32_t uNumPoints;
	uint32_t uLoopStart;
	int32_t nLoopEnd;
} NgsEnvelopeUserParams;

typedef struct NgsEnvelopePoint {
	uint32_t samples;		/* to the next point */
	int32_t amplitude;
	uint32_t curveType;
} NgsEnvelopePoint;

typedef struct NgsEnvelopeSettings {
	NgsEnvelopePoint points[SCE_NGS_ENVELOPE_MAX_POINTS];
	uint32_t releaseSamples;	/* in 32nds */
	uint8_t numPoints;
	uint8_t loopStart;
	int8_t loopEnd;
	uint8_t reserved;
	uint32_t position;
	int32_t height;			/* the release's scale */
	int8_t releasing;
	int8_t currentPoint;
	uint8_t reserved2[2];
	int32_t rates[SCE_NGS_ENVELOPE_MAX_POINTS];	/* 0x8000000 / samples, << 12 */
} NgsEnvelopeSettings;

int32_t ngsEnvelopeInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	uint32_t samplesPerMsec = (int32_t)params->sampleRate / 1000;
	NgsEnvelopeSettings *settings = (NgsEnvelopeSettings *)params->data[0];
	NgsEnvelopeUserParams *user = (NgsEnvelopeUserParams *)params->userParams[0];
	NgsEnvelopePoint *point;
	uint32_t i;

	if (user->desc.id != SCE_NGS_ENVELOPE_PARAMS_STRUCT_ID)
		return SCE_NGS_ERROR_INVALID_PARAM;
	if (params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE) {
		settings->currentPoint = 0;
		settings->height = NGS_ONE;
		settings->position = 0;
		settings->releasing = 0;
	}
	settings->numPoints = user->uNumPoints;
	settings->releaseSamples = (samplesPerMsec * user->uReleaseMsecs) >> 5;
	settings->loopStart = user->uLoopStart;
	settings->loopEnd = user->nLoopEnd;

	for (i = 0; i < user->uNumPoints; i++) {
		point = &settings->points[i];
		point->curveType = user->envelopePoints[i].eCurveType;
		point->samples = user->envelopePoints[i].uMsecsToNextPoint * samplesPerMsec;
		point->amplitude = ngsFixed(user->envelopePoints[i].fAmplitude);
		if (point->samples)
			settings->rates[i] = (0x8000000 / (int32_t)point->samples) << 12;
		else
			settings->rates[i] = 0;
	}
	return 0;
}

/* SceNgsEnvelopeStates */
typedef struct NgsEnvelopeStates {
	float fCurrentHeight;
	float fPosition;
	float fReleaseScale;
	int32_t nCurrentPoint;
	int32_t nReleasing;
} NgsEnvelopeStates;

/* the gains of each step: where it is in its segment, between which levels */
typedef struct NgsEnvelopeSteps {
	int32_t position[ENVELOPE_STEPS];
	int32_t from[ENVELOPE_STEPS];
	int32_t to[ENVELOPE_STEPS];
	int32_t length[ENVELOPE_STEPS];
	int32_t rate[ENVELOPE_STEPS];
	int32_t height[ENVELOPE_STEPS];
	int32_t gain[ENVELOPE_STEPS];
} NgsEnvelopeSteps;

/* 0x8799f0: from + (to - from) t */
static void envelopeLinear(int32_t count, NgsEnvelopeSteps *steps)
{
	int32_t i;

	for (i = 0; i < count; i++) {
		int32_t t = ngsMultiply(ngsMultiply(steps->to[i] - steps->from[i],
			steps->position[i] << POSITION_SHIFT), steps->rate[i]);

		steps->gain[i] = ngsMultiply(t + steps->from[i], steps->height[i]);
	}
}

/* 0x879b48: rising, from + (to - from) t^3; falling, to + (to - from) (t - 1)^3 */
static void envelopeCubic(int32_t count, NgsEnvelopeSteps *steps)
{
	int32_t i;

	for (i = 0; i < count; i++) {
		int32_t from = steps->from[i], to = steps->to[i];
		int32_t position = steps->position[i] << POSITION_SHIFT, base, t;

		if (to > from) {
			base = from;
		} else {
			base = to;
			position -= steps->length[i] << POSITION_SHIFT;
		}
		t = ngsMultiply(position, steps->rate[i]);
		t = ngsMultiply(t, ngsMultiply(t, t));
		steps->gain[i] = ngsMultiply(ngsMultiply(t, to - from) + base, steps->height[i]);
	}
}

/* the level the current point moves to: the next point's, or the loop's */
static int32_t envelopeNext(NgsEnvelopeSettings *settings, int32_t point)
{
	if (settings->loopEnd == point)
		return settings->points[settings->loopStart].amplitude;
	if (point == settings->numPoints - 1)
		return settings->points[point].amplitude;
	return settings->points[point + 1].amplitude;
}

/* 0x879f68 */
int32_t ngsEnvelopeRun(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsEnvelopeSettings *settings = (NgsEnvelopeSettings *)params->data[0];
	NgsEnvelopeStates *states = (NgsEnvelopeStates *)params->data[1];
	NgsBuffer *input = inputs->buffers[0], *output;
	int32_t count = (int32_t)input->samples / ENVELOPE_STEP, point, release, j, k;
	int32_t from, to, length, channel;
	NgsEnvelopeSteps steps;

	if (!(input->flags & NGS_BUFFER_VALID)) {
		params->flags &= ~NGS_RUN_ENABLED;
		goto out;
	}

	if (params->voiceState & NGS_VOICE_STATE_KEYED_OFF)
		settings->releasing = 1;
	release = 0;
	if (settings->releasing) {
		if ((int32_t)settings->releaseSamples >= 1) {
			release = NGS_ONE / (int32_t)settings->releaseSamples;
		} else {
			release = QUICK_RELEASE;
			settings->height = 0;
		}
	}

	point = settings->currentPoint;
	from = settings->points[point].amplitude;
	length = settings->points[point].samples;
	to = envelopeNext(settings, point);
	for (j = 0; j <= count; j++) {
		steps.position[j] = settings->position;
		steps.from[j] = from;
		steps.to[j] = to;
		steps.rate[j] = settings->rates[point];
		steps.length[j] = length;
		steps.height[j] = settings->height;
		settings->height = max(settings->height - release, 0);
		if (j == count)
			break;
		settings->position += ENVELOPE_STEP;
		if ((int32_t)settings->position < length)
			continue;
		settings->position = 0;
		if (settings->loopEnd == point) {
			point = settings->loopStart;
		} else if (point + 1 == settings->numPoints) {
			if (settings->loopEnd == SCE_NGS_ENVELOPE_LOOP_END) {
				outputs->buffers[0]->flags |= NGS_BUFFER_END;
				params->flags |= NGS_RUN_STOP;
			} else if (settings->loopEnd == SCE_NGS_ENVELOPE_LOOP_RELEASE) {
				settings->releasing = 1;
				params->voiceState |= NGS_VOICE_STATE_KEYED_OFF;
			}
			continue;
		} else {
			point = (int8_t)(point + 1);
		}
		from = settings->points[point].amplitude;
		length = settings->points[point].samples;
		to = envelopeNext(settings, point);
	}
	settings->currentPoint = point;

	/* other curve types leave the gains as they were */
	if (settings->points[point].curveType == SCE_NGS_ENVELOPE_LINEAR)
		envelopeLinear(count + 1, &steps);
	else if (settings->points[point].curveType == SCE_NGS_ENVELOPE_CUBIC)
		envelopeCubic(count + 1, &steps);

	for (channel = 0; channel < input->channels; channel++) {
		const int32_t *in = (const int32_t *)input->channelData[channel];
		int32_t *out = (int32_t *)outputs->buffers[0]->channelData[channel];

		if (!((input->channelMask >> channel) & 1))
			continue;
		for (j = 0; j < count; j++) {
			int32_t gain = steps.gain[j];
			int32_t step = (steps.gain[j + 1] - gain) / ENVELOPE_STEP;

			for (k = 0; k < ENVELOPE_STEP; k++)
				*out++ = ngsMultiply(*in++, gain + k * step);
		}
	}

	states->fCurrentHeight = (float)steps.gain[count - 1] * NGS_FLOAT_OF_FIXED;
	states->nReleasing = settings->releasing;
	if (settings->points[settings->currentPoint].samples)
		states->fPosition = (float)(int32_t)settings->position /
			(float)(int32_t)settings->points[settings->currentPoint].samples;
	else
		states->fPosition = 0;
	states->fReleaseScale = (float)settings->height * NGS_FLOAT_OF_FIXED;
	states->nCurrentPoint = settings->currentPoint;

	output = outputs->buffers[0];
	output->flags = NGS_BUFFER_VALID;
	if (input->flags & NGS_BUFFER_END) {
		output->flags |= NGS_BUFFER_END;
		params->flags &= ~NGS_RUN_ENABLED;
	}
	if (settings->releasing) {
		if (settings->height >= 1) {
			params->flags |= NGS_RUN_KEEP_PENDING;
		} else if (settings->loopEnd == SCE_NGS_ENVELOPE_LOOP_END) {
			output->flags |= NGS_BUFFER_END;
			params->flags |= NGS_RUN_STOP;
		}
	}
out:
	if (outputs->count)
		outputs->buffers[0]->channelMask = input->channelMask;
	return 0;
}

int32_t ngsEnvelopeProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	return ngsEnvelopeRun(inputs, outputs, params);
}
