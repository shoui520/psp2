#include "../../../../../naac_internal.h"

int32_t __f98aacSetOption(NaacEncoderInstance *inst, int32_t option)
{
	if (inst == NULL)
		return NAAC_ERR_INVALID_PARAM;
	inst->optionMode = option;
	return NAAC_OK;
}

int32_t aacOpen2(NaacEncoderInstance *inst, const NaacChannelMap *map, int32_t sampleRate,
	int32_t bitRate, int32_t writeAdts)
{
	if (inst == NULL || map == NULL || sampleRate <= 0 || bitRate <= 0)
		return NAAC_ERR_INVALID_PARAM;

	int32_t srIdx = naacSampleRateToIndex(sampleRate);
	if (srIdx < 0)
		return NAAC_ERR_INVALID_PARAM;

	sceClibMemcpy_safe(&inst->channelMap, map, sizeof(NaacChannelMap));
	inst->writeHeader = writeAdts;

	int32_t err = __f98CreatePsychoacoustic(&inst->psycho, map->numChannels, sampleRate);
	if (err != NAAC_OK)
		return err;

	err = __f98CreateLoopKernel(&inst->loopKernel, map->numChannels, sampleRate, bitRate);
	if (err != NAAC_OK)
		return err;

	err = __f98CreateBitstreamEncoder(&inst->bitEnc, map, sampleRate, bitRate, writeAdts);
	if (err != NAAC_OK)
		return err;

	inst->info.sampleRate = sampleRate;
	inst->info.cutoffFreqHz = (float)sampleRate * 0.45f;
	inst->info.numChannels = map->numChannels;
	inst->info.encoderDelay = 1600;
	inst->info.bitRate = bitRate;
	inst->info.audioObjectType = 2; /* AAC-LC */
	inst->info.sampleRateIndex = srIdx;
	inst->info.maxFrameBits = 6144 * map->numChannels;
	inst->isOpen = 1;
	return NAAC_OK;
}

int32_t __f98aacOpen(NaacEncoderInstance *inst, const NaacChannelMap *map, int32_t sampleRate,
	int32_t bitRate, int32_t writeAdts)
{
	return aacOpen2(inst, map, sampleRate, bitRate, writeAdts);
}

int32_t __f98aacGetInfo(const NaacEncoderInstance *inst, NaacCoreInfo *outInfo)
{
	if (inst == NULL || outInfo == NULL || inst->isOpen == 0)
		return NAAC_ERR_INVALID_PARAM;
	*outInfo = inst->info;
	return NAAC_OK;
}

void naacCoreSetHeaderMode(NaacEncoderInstance *inst, int32_t writeHeader)
{
	if (inst == NULL)
		return;
	inst->writeHeader = writeHeader;
	inst->bitEnc.frameInfo.writeAdts = writeHeader;
}

void naacCoreClose(NaacEncoderInstance *inst)
{
	if (inst == NULL)
		return;
	inst->isOpen = 0;
}

int32_t naacCoreEncodeFrame(NaacEncoderInstance *inst, const float *interleavedPcm,
	uint8_t *outBuf, int32_t *outBytes, int32_t maxOutBytes)
{
	if (inst == NULL || inst->isOpen == 0 || interleavedPcm == NULL || outBuf == NULL || outBytes == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t err = naacAdvancePsychoacoustic(&inst->psycho, interleavedPcm, &inst->channelMap, 1, 1);
	if (err != NAAC_OK)
		return err;

	err = __f98AdvanceLoopKernel(&inst->loopKernel, inst->psycho.codChannels, &inst->psycho.psyParam);
	if (err != NAAC_OK)
		return err;

	return __f98AdvanceBitstreamEncoder(&inst->bitEnc, inst->psycho.codChannels,
		inst->psycho.msChannels, &inst->psycho.psyParam, outBuf, maxOutBytes, outBytes);
}
