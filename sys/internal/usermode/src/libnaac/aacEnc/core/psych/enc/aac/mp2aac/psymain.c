#include "../../../../../../naac_internal.h"

int32_t __f98CreatePsychoacoustic(NaacPsychoacoustic *psy, int32_t numChannels, int32_t sampleRate)
{
	if (psy == NULL || numChannels <= 0 || numChannels > NAAC_MAX_CHANNELS)
		return NAAC_ERR_INVALID_PARAM;

	sceClibMemset(psy, 0, sizeof(NaacPsychoacoustic));
	psy->numChannels = numChannels;

	int32_t err = __f98CreatePsyParam(&psy->psyParam, sampleRate);
	if (err != NAAC_OK)
		return err;

	for (int32_t ch = 0; ch < numChannels; ch++) {
		err = __f98CreatePsyChannel(&psy->channels[ch], &psy->psyParam);
		if (err != NAAC_OK)
			return err;
		err = __f98CreateCodChannel(&psy->codChannels[ch], &psy->psyParam);
		if (err != NAAC_OK)
			return err;
	}

	for (int32_t elem = 0; elem < NAAC_MAX_ELEMENTS; elem++)
		__f98CreateMSChannel(&psy->msChannels[elem]);

	return NAAC_OK;
}

int32_t naacAdvancePsychoacoustic(NaacPsychoacoustic *psy, const float *interleavedPcm,
	const NaacChannelMap *map, int32_t enableMs, int32_t enableTns)
{
	if (psy == NULL || interleavedPcm == NULL || map == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t channels = map->numChannels;
	float chanBuf[NAAC_FRAME_SAMPLES];

	for (int32_t ch = 0; ch < channels; ch++) {
		for (int32_t i = 0; i < NAAC_FRAME_SAMPLES; i++)
			chanBuf[i] = interleavedPcm[i * channels + ch];

		naacAdvancePsyChannel(&psy->channels[ch], chanBuf, &psy->psyParam);
		if (enableTns != 0)
			naacApplyTns(&psy->channels[ch].tns, psy->channels[ch].mdctSpectrum, NAAC_FRAME_SAMPLES);
		naacFillCodChannel(&psy->codChannels[ch], &psy->channels[ch], &psy->psyParam);
	}

	if (enableMs != 0) {
		int32_t cpeIndex = 0;
		for (int32_t e = 0; e < map->numElements; e++) {
			const NaacChannelElement *elem = &map->elements[e];
			if (elem->elementType == NAAC_ELEM_CPE) {
				__f98AdvanceMSChannel(&psy->msChannels[cpeIndex++],
					&psy->codChannels[elem->firstChannel],
					&psy->codChannels[elem->secondChannel],
					&psy->psyParam);
			}
		}
	}

	return NAAC_OK;
}
