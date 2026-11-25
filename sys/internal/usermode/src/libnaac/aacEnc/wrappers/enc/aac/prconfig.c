#include "../../../../naac_internal.h"

int32_t __f98aacMakeDefinedProgramConfig(NaacProgramConfig *pce, const NaacChannelMap *map, int32_t sampleRateIndex)
{
	if (pce == NULL || map == NULL)
		return NAAC_ERR_INVALID_PARAM;
	if (sampleRateIndex < 0 || sampleRateIndex > 12)
		return NAAC_ERR_INVALID_PARAM;

	sceClibMemset(pce, 0, sizeof(NaacProgramConfig));
	pce->elementInstanceTag = 0;
	pce->objectType = 1; /* AAC-LC */
	pce->samplingFreqIndex = sampleRateIndex;
	pce->numFrontChannels = map->numFrontElements;
	pce->numSideChannels = map->numSideElements;
	pce->numBackChannels = map->numBackElements;
	pce->numLfeChannels = map->numLfeElements;
	pce->comment = NAAC_DEFAULT_COMMENT;
	pce->channelConfigIndex = (map->numChannels == 8) ? 7 : map->numChannels;
	return NAAC_OK;
}
