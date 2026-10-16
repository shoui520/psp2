/*
 * Distortion (SCE_NGS_DISTORTION_ID): the VLIW kernel on each channel, with
 * the parameters in 8.24.
 */
#include "ngs.h"

#define SCE_NGS_DISTORTION_PARAMS_STRUCT_ID	SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_DISTORTION_ID, 1, 1)

/* SceNgsDistortionParams */
typedef struct NgsDistortionUserParams {
	SceNgsParamsDescriptor desc;
	float fA;
	float fB;
	float fClip;
	float fGate;
	float fWetGain;
	float fDryGain;
} NgsDistortionUserParams;

typedef struct NgsDistortionSettings {
	int32_t a;
	int32_t b;
	int32_t clip;
	int32_t gate;
	int32_t wetGain;
	int32_t dryGain;
} NgsDistortionSettings;

/*
 * 0x879590: y = a x - b x^2 for x > 0, a x + b x^2 below (an odd curve),
 * clipped to +-clip, gated to 0 within +-gate, then mixed with the dry
 * signal in one 64-bit sum.
 */
static int32_t distort(const NgsDistortionSettings *settings, int32_t x)
{
	int64_t curve = (int64_t)settings->a * x;
	int64_t bend = (int64_t)settings->b * ngsMultiply(x, x);
	int32_t y;

	if (!x)
		y = 0;
	else
		y = (int32_t)((x > 0 ? curve - bend : curve + bend) >> 24);
	y = max(min(y, settings->clip), -settings->clip);
	if (y > 0 ? y <= settings->gate : y > -settings->gate)
		y = 0;
	return (int32_t)(((int64_t)x * settings->dryGain + (int64_t)y * settings->wetGain) >> 24);
}

void ngsDistortionApply(const int32_t *input, int32_t *output, void *data, uint32_t count)
{
	const NgsDistortionSettings *settings = (const NgsDistortionSettings *)data;
	uint32_t i;

	for (i = 0; i < count; i++)
		output[i] = distort(settings, input[i]);
}

int32_t ngsDistortionInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsDistortionUserParams *user = (NgsDistortionUserParams *)params->userParams[0];
	NgsDistortionSettings *settings = (NgsDistortionSettings *)params->data[0];

	if (user->desc.id != SCE_NGS_DISTORTION_PARAMS_STRUCT_ID)
		return SCE_NGS_ERROR_INVALID_PARAM;
	settings->a = ngsFixed(user->fA);
	settings->b = ngsFixed(user->fB);
	settings->clip = ngsFixed(user->fClip);
	settings->gate = ngsFixed(user->fGate);
	settings->wetGain = ngsFixed(user->fWetGain);
	settings->dryGain = ngsFixed(user->fDryGain);
	return 0;
}

int32_t ngsDistortionProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsDistortionSettings *settings = (NgsDistortionSettings *)params->data[0];
	NgsBuffer *input = inputs->buffers[0];
	uint32_t flags = params->flags;
	int32_t channel;

	if (!(input->flags & NGS_BUFFER_VALID)) {
		params->flags = flags & ~NGS_RUN_ENABLED;
		goto channels;
	}
	if (flags & NGS_RUN_BYPASSED) {
		if (input->flags & NGS_BUFFER_END) {
			outputs->buffers[0]->flags |= NGS_BUFFER_END;
			params->flags = 0;
		}
		return 0;
	}

	for (channel = 0; channel < inputs->buffers[0]->channels; channel++)
		ngsDistortionApply((int32_t *)inputs->buffers[0]->channelData[channel],
			(int32_t *)outputs->buffers[0]->channelData[channel], settings,
			inputs->buffers[0]->samples);
	outputs->buffers[0]->flags = NGS_BUFFER_VALID;
	if (inputs->buffers[0]->flags & NGS_BUFFER_END) {
		outputs->buffers[0]->flags |= NGS_BUFFER_END;
		params->flags = 0;
	}
channels:
	if (outputs->count)
		outputs->buffers[0]->channelMask = inputs->buffers[0]->channelMask;
	return 0;
}
