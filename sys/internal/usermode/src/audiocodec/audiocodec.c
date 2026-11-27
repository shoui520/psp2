/*
 * SceAudiocodec (vs0:sys/external/audiocodec.suprx):
 * Public dispatch interface across ATRAC3plus, ATRAC3, MP3, AAC, and ATRAC9.
 */
#include "audiocodec_internal.h"

int32_t SceAudiocodec_E2AF3A54(uint32_t *work, int32_t codecType)
{
	if (work == NULL)
		return AUDIOCODEC_ERROR_INVALID_ARG;

	switch (codecType) {
	case AUDIOCODEC_TYPE_AT3P:
		return atrac3pCheckNeedMem(work);
	case AUDIOCODEC_TYPE_AT3:
		return atrac3CheckNeedMem(work);
	case AUDIOCODEC_TYPE_MP3:
		return mp3CheckNeedMem(work);
	case AUDIOCODEC_TYPE_AAC:
		return aacCheckNeedMem(work);
	case AUDIOCODEC_TYPE_AT9:
		return atrac9CheckNeedMem(work);
	default:
		return AUDIOCODEC_ERROR_UNSUPPORTED;
	}
}

int32_t SceAudiocodec_1F9A3722(uint32_t *work, int32_t codecType)
{
	if (work == NULL || work[1] == 0)
		return AUDIOCODEC_ERROR_INVALID_ARG;

	switch (codecType) {
	case AUDIOCODEC_TYPE_AT3P:
		return atrac3pInit(work);
	case AUDIOCODEC_TYPE_AT3:
		return atrac3Init(work);
	case AUDIOCODEC_TYPE_MP3:
		return mp3Init(work);
	case AUDIOCODEC_TYPE_AAC:
		return aacInit(work);
	case AUDIOCODEC_TYPE_AT9:
		return atrac9Init(work);
	default:
		return AUDIOCODEC_ERROR_UNSUPPORTED;
	}
}

int32_t SceAudiocodec_E827E04F(uint32_t *work, int32_t codecType)
{
	if (work == NULL || work[1] == 0 || work[4] == 0 || work[6] == 0)
		return AUDIOCODEC_ERROR_INVALID_ARG;

	switch (codecType) {
	case AUDIOCODEC_TYPE_AT3P:
		return atrac3pDecode(work);
	case AUDIOCODEC_TYPE_AT3:
		return atrac3Decode(work);
	case AUDIOCODEC_TYPE_MP3:
		return mp3Decode(work);
	case AUDIOCODEC_TYPE_AAC:
		return aacDecode(work);
	case AUDIOCODEC_TYPE_AT9:
		return atrac9Decode(work);
	default:
		return AUDIOCODEC_ERROR_UNSUPPORTED;
	}
}

__attribute__((noinline)) int32_t SceAudiocodec_3B7D6200(uint32_t *work, int32_t codecType)
{
	if (work == NULL || work[1] == 0 || work[4] == 0 || work[6] == 0)
		return AUDIOCODEC_ERROR_INVALID_ARG;

	if ((uint32_t)(codecType - AUDIOCODEC_TYPE_AT3P) <= 3U)
		return 0;

	return AUDIOCODEC_ERROR_UNSUPPORTED;
}

int32_t SceAudiocodec_2DDF5699(uint32_t *work, int32_t codecType, int32_t *outMaxInput)
{
	int32_t maxInput;

	if (work == NULL || work[1] == 0)
		return AUDIOCODEC_ERROR_INVALID_ARG;

	switch (codecType) {
	case AUDIOCODEC_TYPE_AT3P:
		if (work[10] == 0) {
			maxInput = (int32_t)work[14] + 2;
			if (maxInput < 0)
				return maxInput;
		} else {
			maxInput = 4106;
		}
		break;
	case AUDIOCODEC_TYPE_AT3:
		maxInput = (int32_t)work[10] + 1;
		if (maxInput < 0)
			return maxInput;
		break;
	case AUDIOCODEC_TYPE_MP3:
		maxInput = 1441;
		break;
	case AUDIOCODEC_TYPE_AAC:
		maxInput = 4096;
		break;
	case AUDIOCODEC_TYPE_AT9:
		maxInput = 1024;
		break;
	default:
		return AUDIOCODEC_ERROR_UNSUPPORTED;
	}

	*outMaxInput = maxInput;
	return 0;
}

int32_t SceAudiocodec_4ECFEF31(uint32_t *work, int32_t codecType, int32_t *outMaxOutput)
{
	int32_t maxOutput;

	if (work == NULL || work[1] == 0)
		return AUDIOCODEC_ERROR_INVALID_ARG;

	switch (codecType) {
	case AUDIOCODEC_TYPE_AT3P: {
		uint32_t outChannels = work[12];
		uint32_t chCount = work[16];
		if (outChannels == 1 && chCount != 1) {
			maxOutput = 0x2000;
		} else {
			maxOutput = (int32_t)(chCount << 12);
			if (maxOutput < 0)
				return maxOutput;
		}
		break;
	}
	case AUDIOCODEC_TYPE_AT3:
		if ((uint32_t)(work[8] - 14U) <= 1U)
			maxOutput = 2048;
		else
			maxOutput = 4096;
		break;
	case AUDIOCODEC_TYPE_MP3: {
		uint32_t mpegVer = work[9];
		if (mpegVer == 9999U) {
			int32_t ret = SceAudiocodec_3B7D6200(work, AUDIOCODEC_TYPE_MP3);
			if (ret < 0)
				return ret;
			maxOutput = 2304;
		} else if (mpegVer == 1U) {
			maxOutput = 4608;
		} else {
			maxOutput = 2304;
		}
		break;
	}
	case AUDIOCODEC_TYPE_AAC:
		if (((const uint8_t *)work)[44] != 0)
			maxOutput = 0x2000;
		else
			maxOutput = 4096;
		break;
	default:
		maxOutput = 4096;
		break;
	}

	*outMaxOutput = maxOutput;
	return 0;
}
