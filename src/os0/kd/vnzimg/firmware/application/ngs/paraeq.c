/*
 * The parametric EQ (SCE_NGS_PARAM_EQ_ID): four filters in a row, each
 * run as the filter module on that filter's biquads (one per channel).
 */
#include "ngs.h"

#define SCE_NGS_PARAM_EQ_STRUCT_ID		SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_PARAM_EQ_ID, 1, 1)
#define SCE_NGS_PARAM_EQ_COEFF_STRUCT_ID	SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_PARAM_EQ_ID, 2, 1)
#define SCE_NGS_MAX_EQ_FILTERS			4

/* SceNgsParamEqParams, SceNgsParamEqParamsCoEff: one per channel */
typedef struct NgsParamEqFilter {
	uint32_t eFilterMode;
	float fFrequency;
	float fResonance;
	float fGain;
} NgsParamEqFilter;

typedef struct NgsParamEqCoEff {
	float fB0;
	float fB1;
	float fB2;
	float fA1;
	float fA2;
} NgsParamEqCoEff;

typedef struct NgsParamEqUserParams {
	SceNgsParamsDescriptor desc;
	NgsParamEqFilter filter[SCE_NGS_MAX_EQ_FILTERS];
} NgsParamEqUserParams;

typedef struct NgsParamEqUserCoefficients {
	SceNgsParamsDescriptor desc;
	NgsParamEqCoEff filterCoEff[SCE_NGS_MAX_EQ_FILTERS];
} NgsParamEqUserCoefficients;

/* each channel's biquads */
typedef struct NgsParamEqChannel {
	NgsBiquad filters[SCE_NGS_MAX_EQ_FILTERS];
} NgsParamEqChannel;

int32_t ngsParamEqInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsFilterUserParams design[SCE_NGS_MAX_SYSTEM_CHANNELS];
	NgsFilterUserCoefficients given[SCE_NGS_MAX_SYSTEM_CHANNELS];
	NgsBiquad filters[SCE_NGS_MAX_SYSTEM_CHANNELS];
	NgsParamEqChannel *channels;
	uint8_t *user;
	int32_t result = 0, channel;
	uint32_t filter;

	if (params->userParams[0] == NULL || params->data[0] == NULL)
		return SCE_NGS_ERROR;
	channels = (NgsParamEqChannel *)params->data[0];
	user = params->userParams[0];

	if (((SceNgsParamsDescriptor *)user)->id == SCE_NGS_PARAM_EQ_STRUCT_ID) {
		NgsParamEqUserParams *eq = (NgsParamEqUserParams *)params->userParams[0];

		for (filter = 0; filter < SCE_NGS_MAX_EQ_FILTERS; filter++) {
			for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
				design[channel].desc.id = SCE_NGS_FILTER_PARAMS_STRUCT_ID;
				design[channel].desc.size = sizeof(NgsFilterUserParams);
				design[channel].eFilterMode = eq[channel].filter[filter].eFilterMode;
				design[channel].fFrequency = eq[channel].filter[filter].fFrequency;
				design[channel].fGain = eq[channel].filter[filter].fGain;
				design[channel].fResonance = eq[channel].filter[filter].fResonance;
				filters[channel] = channels[channel].filters[filter];
			}
			params->userParams[0] = (uint8_t *)design;
			params->data[0] = (uint8_t *)filters;
			result = ngsFilterInit(inputs, outputs, params);
			if (result)
				break;
			for (channel = 0; channel < inputs->buffers[0]->channels; channel++)
				channels[channel].filters[filter] = filters[channel];
		}
	} else if (((SceNgsParamsDescriptor *)user)->id == SCE_NGS_PARAM_EQ_COEFF_STRUCT_ID) {
		NgsParamEqUserCoefficients *eq = (NgsParamEqUserCoefficients *)user;

		for (filter = 0; filter < SCE_NGS_MAX_EQ_FILTERS; filter++) {
			/*
			 * The original shares this loop's index with the copy back
			 * below: only channel 0's coefficients are taken, then the
			 * loop ends.
			 */
			for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
				given[channel].desc.id = SCE_NGS_FILTER_PARAMS_COEFF_STRUCT_ID;
				given[channel].desc.size = sizeof(NgsFilterUserCoefficients);
				given[channel].fA1 = eq[channel].filterCoEff[filter].fA1;
				given[channel].fA2 = eq[channel].filterCoEff[filter].fA2;
				given[channel].fB0 = eq[channel].filterCoEff[filter].fB0;
				given[channel].fB1 = eq[channel].filterCoEff[filter].fB1;
				given[channel].fB2 = eq[channel].filterCoEff[filter].fB2;
				filters[channel] = channels[channel].filters[filter];
				params->userParams[0] = (uint8_t *)given;
				params->data[0] = (uint8_t *)filters;
				result = ngsFilterInit(inputs, outputs, params);
				if (result)
					break;
				for (channel = 0; channel < inputs->buffers[0]->channels; channel++)
					channels[channel].filters[filter] = filters[channel];
			}
		}
	} else {
		return SCE_NGS_ERROR;
	}

	params->userParams[0] = user;
	params->data[0] = (uint8_t *)channels;
	return result;
}

/* whether a channel's biquad of a filter does anything */
static uint32_t filterActive(NgsParamEqChannel *channels, uint32_t filter, int32_t count)
{
	int32_t channel;

	for (channel = 0; channel < count; channel++) {
		NgsBiquad *biquad = &channels[channel].filters[filter];

		if (biquad->b0 != NGS_ONE || biquad->b1 || biquad->b2 || biquad->a1 || biquad->a2)
			return 1;
	}
	return 0;
}

/* the active filters in order, each on the last one's output; the last always */
int32_t ngsParamEqProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsParamEqChannel *channels = (NgsParamEqChannel *)params->data[0];
	NgsBiquad filters[SCE_NGS_MAX_SYSTEM_CHANNELS];
	NgsBuffer *source[1];
	NgsBufferList input;
	int32_t result = SCE_NGS_ERROR, ran = 0, channel;
	uint32_t filter;

	input.count = inputs->count;
	input.buffers = source;
	source[0] = inputs->buffers[0];

	for (filter = 0; filter < SCE_NGS_MAX_EQ_FILTERS; filter++) {
		if (!filterActive(channels, filter, inputs->buffers[0]->channels) &&
			(ran || filter != SCE_NGS_MAX_EQ_FILTERS - 1))
			continue;
		ran++;
		for (channel = 0; channel < inputs->buffers[0]->channels; channel++)
			filters[channel] = channels[channel].filters[filter];
		params->data[0] = (uint8_t *)filters;
		result = ngsFilterProcess(&input, outputs, params);
		for (channel = 0; channel < inputs->buffers[0]->channels; channel++)
			channels[channel].filters[filter] = filters[channel];
		if (result)
			break;
		input.count = outputs->count;
		source[0] = outputs->buffers[0];
	}
	params->data[0] = (uint8_t *)channels;
	return result;
}
