#include "../../../../../naac_internal.h"

static const int32_t g_naacSampleRates[13] = {
	96000, 88200, 64000, 48000, 44100, 32000,
	24000, 22050, 16000, 12000, 11025, 8000, 7350
};

static const int32_t g_naacSfbWidthLong48k[49] = {
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 8, 8,
	12, 12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 96
};

static const int32_t g_naacSfbWidthShort48k[14] = {
	4, 4, 4, 4, 4, 4, 8, 8, 8, 12, 12, 16, 20, 20
};

int32_t naacSampleRateToIndex(int32_t sampleRate)
{
	for (int32_t i = 0; i < 13; i++) {
		if (g_naacSampleRates[i] == sampleRate)
			return i;
	}
	return -1;
}

int32_t __f98CreatePsyParam(NaacPsyParam *param, int32_t sampleRate)
{
	if (param == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t srIdx = naacSampleRateToIndex(sampleRate);
	if (srIdx < 0)
		return NAAC_ERR_INVALID_PARAM;

	sceClibMemset(param, 0, sizeof(NaacPsyParam));
	param->sampleRate = sampleRate;
	param->sampleRateIndex = srIdx;

	param->numSfbLong = 49;
	int32_t offset = 0;
	for (int32_t i = 0; i < param->numSfbLong; i++) {
		param->sfbOffsetLong[i] = offset;
		offset += g_naacSfbWidthLong48k[i];
	}
	param->sfbOffsetLong[param->numSfbLong] = NAAC_FRAME_SAMPLES;

	param->numSfbShort = 14;
	offset = 0;
	for (int32_t i = 0; i < param->numSfbShort; i++) {
		param->sfbOffsetShort[i] = offset;
		offset += g_naacSfbWidthShort48k[i];
	}
	param->sfbOffsetShort[param->numSfbShort] = NAAC_SHORT_SAMPLES;

	CalculateBarcCbWidthVal(sampleRate, NAAC_FRAME_SAMPLES, param->cbWidthLong, &param->numCbLong);
	CalculateFixedBarcCbWidthVal(sampleRate, NAAC_SHORT_SAMPLES, param->cbWidthShort, &param->numCbShort);
	CalculateSpreadingFunction(param->numCbLong, param->cbWidthLong, param->spreadingLong);
	CalculateSpreadingFunction(param->numCbShort, param->cbWidthShort, param->spreadingShort);
	return NAAC_OK;
}

int32_t GetPsyBlockParam(const NaacPsyParam *param, int32_t blockType, int32_t *outSfbCount)
{
	if (param == NULL || outSfbCount == NULL)
		return NAAC_ERR_INVALID_PARAM;
	*outSfbCount = (blockType == NAAC_SHORT_WINDOW) ? param->numSfbShort : param->numSfbLong;
	return NAAC_OK;
}

int32_t GetAdaptBlockParam(const NaacPsyParam *param, int32_t blockType, int32_t *outCbCount)
{
	if (param == NULL || outCbCount == NULL)
		return NAAC_ERR_INVALID_PARAM;
	*outCbCount = (blockType == NAAC_SHORT_WINDOW) ? param->numCbShort : param->numCbLong;
	return NAAC_OK;
}
