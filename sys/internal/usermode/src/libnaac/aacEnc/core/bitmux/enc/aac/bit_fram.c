#include "../../../../../naac_internal.h"

int32_t __f98InitFrameInfo(NaacFrameInfo *info, int32_t sampleRate, int32_t numChannels, int32_t bitRate, int32_t writeAdts)
{
	if (info == NULL || sampleRate <= 0 || numChannels <= 0 || bitRate <= 0)
		return NAAC_ERR_INVALID_PARAM;

	int32_t srIdx = naacSampleRateToIndex(sampleRate);
	if (srIdx < 0)
		return NAAC_ERR_INVALID_PARAM;

	sceClibMemset(info, 0, sizeof(NaacFrameInfo));
	info->sampleRate = sampleRate;
	info->sampleRateIndex = srIdx;
	info->numChannels = numChannels;
	info->bitRate = bitRate;
	info->audioObjectType = 2; /* AAC-LC */
	info->frameBits = (int32_t)(((double)bitRate * (double)NAAC_FRAME_SAMPLES) / (double)sampleRate);
	info->writeAdts = writeAdts;
	info->protectionAbsent = 1;
	info->channelConfig = (numChannels == 8) ? 7 : numChannels;
	return NAAC_OK;
}
