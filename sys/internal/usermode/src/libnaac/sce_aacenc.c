#include "naac_internal.h"

/* Forward declarations for internal exports called by public wrappers */
int32_t SceAacencInternal_118F588C(void);
int32_t SceAacencInternal_71BDD121(NaacHandle **handleOut, void *workBuffer);
int32_t SceAacencInternal_B3004C1B(NaacHandle *handle);
int32_t SceAacencInternal_C867CE87(NaacHandle *handle, const NaacConfig *config);
int32_t SceAacencInternal_C1876820(NaacHandle *handle);
int32_t SceAacencInternal_9EB15569(NaacHandle *handle, int32_t pcmFormat, NaacBufferSizes *outSizes);
int32_t SceAacencInternal_C58CA5A8(NaacHandle *handle, int32_t *outDelay);
int32_t SceAacencInternal_B8BEEDE6(NaacHandle *handle, const void *pcmIn, uint32_t inputBytes,
	uint8_t *outBuf, int32_t *outBytes, int32_t maxOutBytes);
int32_t SceAacencInternal_88BF7DA5(NaacHandle *handle, uint32_t headerFlag);
int32_t SceAacencInternal_30D75275(NaacHandle *handle, uint8_t *outBuf, int32_t *outBytes);
int32_t SceAacencInternal_7EC218B7(NaacHandle *handle, uint8_t *outBuf, int32_t *outBytes);
int32_t SceAacencInternal_50891A4F(NaacHandle *handle, uint8_t *outBuf, int32_t *outBytes);

static uint32_t naacMapPublicError(int32_t internalErr)
{
	return ((uint32_t)internalErr & NAAC_ERROR_BASE) - (uint32_t)internalErr;
}

/* Self-contained floating-point math helpers (avoids libm dependency) */
float naacFabsf(float x)
{
	return (x < 0.0f) ? -x : x;
}

float naacSqrtf(float x)
{
	if (x <= 0.0f)
		return 0.0f;
	float guess = (x > 1.0f) ? (x * 0.5f) : 1.0f;
	for (int i = 0; i < 16; i++)
		guess = 0.5f * (guess + x / guess);
	return guess;
}

float naacCosf(float x)
{
	const float twoPi = 6.283185307179586f;
	const float pi = 3.141592653589793f;
	if (x < 0.0f)
		x = -x;
	while (x >= twoPi)
		x -= twoPi;
	float sign = 1.0f;
	if (x > pi) {
		x = twoPi - x;
	}
	if (x > pi * 0.5f) {
		x = pi - x;
		sign = -1.0f;
	}
	float sq = x * x;
	float val = 1.0f - sq * (0.5f - sq * (0.041666668f - sq * (0.0013888889f - sq * 0.000024801587f)));
	return sign * val;
}

float naacSinf(float x)
{
	return naacCosf(1.5707963267948966f - x);
}

static float naacLnf(float x)
{
	if (x <= 0.0f)
		return -30.0f;
	int32_t expCount = 0;
	while (x >= 2.0f) {
		x *= 0.5f;
		expCount++;
	}
	while (x < 1.0f) {
		x *= 2.0f;
		expCount--;
	}
	float ratio = (x - 1.0f) / (x + 1.0f);
	float ratioSq = ratio * ratio;
	float term = ratio;
	float sum = term;
	for (int k = 3; k <= 13; k += 2) {
		term *= ratioSq;
		sum += term / (float)k;
	}
	return 2.0f * sum + (float)expCount * 0.6931471805599453f;
}

float naacLog10f(float x)
{
	return naacLnf(x) * 0.4342944819032518f;
}

float naacPowf(float base, float exponent)
{
	if (base <= 0.0f)
		return 0.0f;
	float scaled = exponent * naacLnf(base);
	if (scaled > 80.0f)
		scaled = 80.0f;
	if (scaled < -80.0f)
		scaled = -80.0f;
	int32_t intPart = (int32_t)scaled;
	float frac = scaled - (float)intPart;
	float term = 1.0f;
	float poly = 1.0f;
	for (int i = 1; i <= 10; i++) {
		term = term * frac / (float)i;
		poly += term;
	}
	float factor = 1.0f;
	const float euler = 2.718281828459045f;
	if (intPart >= 0) {
		for (int i = 0; i < intPart; i++)
			factor *= euler;
	} else {
		for (int i = 0; i < -intPart; i++)
			factor /= euler;
	}
	return poly * factor;
}

float naacAtanf(float x)
{
	float sign = (x < 0.0f) ? -1.0f : 1.0f;
	float ax = (x < 0.0f) ? -x : x;
	float result;
	if (ax > 1.0f) {
		float inv = 1.0f / ax;
		float sq = inv * inv;
		result = 1.5707963267948966f - inv * (1.0f - sq * (0.33333334f - sq * (0.2f - sq * 0.14285715f)));
	} else {
		float sq = ax * ax;
		result = ax * (1.0f - sq * (0.33333334f - sq * (0.2f - sq * 0.14285715f)));
	}
	return sign * result;
}

/* PCM sample format converters (normalized to 16-bit float range) */
static void naacConvertPcmInt16(float *dst, const void *src, int32_t sampleCount)
{
	const int16_t *inSamples = (const int16_t *)src;
	for (int32_t i = 0; i < sampleCount; i++)
		dst[i] = (float)inSamples[i];
}

static void naacConvertPcmInt24(float *dst, const void *src, int32_t sampleCount)
{
	const uint8_t *bytes = (const uint8_t *)src;
	for (int32_t i = 0; i < sampleCount; i++) {
		int32_t low = bytes[i * 3];
		int32_t mid = bytes[i * 3 + 1];
		int32_t high = bytes[i * 3 + 2];
		int32_t sample = low | (mid << 8) | (high << 16);
		if ((high & 0x80) != 0)
			sample |= (int32_t)0xFF000000U;
		dst[i] = (float)sample * 0.00390625f;
	}
}

static void naacConvertPcmInt32(float *dst, const void *src, int32_t sampleCount)
{
	const int32_t *inSamples = (const int32_t *)src;
	for (int32_t i = 0; i < sampleCount; i++)
		dst[i] = (float)inSamples[i] * 0.000015258789f;
}

static void naacConvertPcmFloat32(float *dst, const void *src, int32_t sampleCount)
{
	const float *inSamples = (const float *)src;
	for (int32_t i = 0; i < sampleCount; i++)
		dst[i] = inSamples[i] * 32767.0f;
}

/* Simple lowpass pre-filter mirroring naac's input conditioning stage */
static void naacLowpassReset(NaacLowpassFilter *filter)
{
	sceClibMemset(filter, 0, sizeof(NaacLowpassFilter));
}

static int32_t naacLowpassInit(NaacLowpassFilter *filter, int32_t numChannels, float cutoffNorm)
{
	if (numChannels <= 0 || numChannels > NAAC_MAX_CHANNELS)
		return NAAC_ERR_INVALID_PARAM;
	naacLowpassReset(filter);
	filter->numChannels = numChannels;
	filter->cutoffNorm = cutoffNorm;
	filter->tapCount = 1;
	filter->delaySamples = 0;
	return NAAC_OK;
}

static int32_t naacLowpassProcess(NaacLowpassFilter *filter, const float *input, int32_t inputCount,
	float *output, int32_t outputCapacity)
{
	if (filter == NULL || input == NULL || output == NULL || outputCapacity < inputCount)
		return 0;
	float alpha = filter->cutoffNorm;
	if (alpha <= 0.0f || alpha >= 1.0f) {
		for (int32_t i = 0; i < inputCount; i++)
			output[i] = input[i];
		return 1;
	}
	int32_t channels = filter->numChannels;
	int32_t ch = 0;
	for (int32_t i = 0; i < inputCount; i++) {
		float filtered = alpha * input[i] + (1.0f - alpha) * filter->prevSample[ch];
		filter->prevSample[ch] = filtered;
		output[i] = filtered;
		ch++;
		if (ch == channels)
			ch = 0;
	}
	return 1;
}

static int32_t naacFlushInternal(NaacHandle *handle, uint8_t *outBuf, int32_t *outBytes, int32_t maxOutBytes)
{
	if (handle == NULL || handle->magic != NAAC_HANDLE_MAGIC)
		return NAAC_ERR_INVALID_HANDLE;
	if (handle->isConfigured == 0) {
		if (outBytes != NULL)
			*outBytes = 0;
		return NAAC_ERR_INVALID_HANDLE;
	}
	if (handle->flushFramesRemaining == 0) {
		if (outBytes != NULL)
			*outBytes = 0;
		return NAAC_OK;
	}
	if (outBuf == NULL) {
		if (outBytes != NULL)
			*outBytes = 0;
		return NAAC_ERR_INVALID_PARAM;
	}

	int32_t totalSamples = handle->numChannels * NAAC_FRAME_SAMPLES;
	sceClibMemset(handle->pcmWork, 0, (size_t)totalSamples * sizeof(float));
	float filtered[NAAC_FRAME_SAMPLES * NAAC_MAX_CHANNELS];
	naacLowpassProcess(&handle->lowpass, handle->pcmWork, totalSamples, filtered, totalSamples);

	int32_t produced = 0;
	int32_t err = naacCoreEncodeFrame(handle->coreEncoder, filtered, outBuf, &produced, maxOutBytes);
	handle->lastCoreError = err;
	if (err != NAAC_OK) {
		if (outBytes != NULL)
			*outBytes = produced;
		return NAAC_ERR_INTERNAL;
	}

	handle->flushFramesRemaining--;
	if (handle->flushFramesRemaining == 0)
		SceAacencInternal_C1876820(handle);
	if (outBytes != NULL)
		*outBytes = produced;
	return NAAC_OK;
}

/* === SceAacencInternal Exports === */

int32_t SceAacencInternal_118F588C(void)
{
	return NAAC_WORK_BUFFER_SIZE;
}

int32_t SceAacencInternal_B3004C1B(NaacHandle *handle)
{
	if (handle == NULL)
		return NAAC_ERR_INVALID_PARAM;
	if (handle->magic != NAAC_HANDLE_MAGIC)
		return NAAC_ERR_INVALID_HANDLE;

	if (handle->coreEncoder != NULL) {
		naacCoreClose(handle->coreEncoder);
		handle->coreEncoder = NULL;
	}
	naacLowpassReset(&handle->lowpass);
	handle->isConfigured = 0;
	handle->coreInfo = NULL;
	handle->channelMap = NULL;
	handle->ascBytes = 0;
	handle->adifBytes = 0;
	handle->latmBytes = 0;
	handle->magic = 0;
	return NAAC_OK;
}

int32_t SceAacencInternal_71BDD121(NaacHandle **handleOut, void *workBuffer)
{
	if (handleOut == NULL)
		return NAAC_ERR_INVALID_PARAM;
	if (workBuffer == NULL)
		return NAAC_ERR_NO_MEMORY;

	NaacHandle *handle = (NaacHandle *)workBuffer;
	sceClibMemset(handle, 0, sizeof(NaacHandle));
	handle->magic = NAAC_HANDLE_MAGIC;
	handle->workBuffer = workBuffer;
	handle->coreEncoder = &handle->coreStorage;
	handle->coreInfo = &handle->coreStorage.info;
	handle->channelMap = &handle->coreStorage.channelMap;
	handle->pcmFormat = NAAC_PCM_INT16;
	handle->bytesPerSample = 2;
	handle->convertPcm = naacConvertPcmInt16;

	int32_t err = __f98aacSetOption(handle->coreEncoder, 8);
	handle->lastCoreError = err;
	if (err != NAAC_OK) {
		SceAacencInternal_B3004C1B(handle);
		*handleOut = handle;
		return NAAC_ERR_INTERNAL;
	}

	*handleOut = handle;
	return NAAC_OK;
}

int32_t SceAacencInternal_C867CE87(NaacHandle *handle, const NaacConfig *config)
{
	if (handle == NULL || handle->magic != NAAC_HANDLE_MAGIC)
		return NAAC_ERR_INVALID_HANDLE;
	if (config == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t channels = config->numChannels;
	if ((channels < 1 || channels > 6) && channels != 8)
		return NAAC_ERR_INVALID_PARAM;

	double rateHz = (double)config->sampleRate;
	double bitsPerChan = (double)config->bitRate / (double)channels;
	if (bitsPerChan < (double)(int32_t)(rateHz * 0.5) || bitsPerChan > rateHz * 6.0)
		return NAAC_ERR_INVALID_PARAM;

	if (config->streamFormat != 0 && config->streamFormat != 1 && config->streamFormat != 2)
		return NAAC_ERR_INVALID_PARAM;
	if (config->enableTns > 1U || config->enableMs > 1U || config->disablePns > 1U)
		return NAAC_ERR_INVALID_PARAM;
	if (config->enablePce > 1U || config->enableAdts > 1U || config->enableLatm > 1U)
		return NAAC_ERR_INVALID_PARAM;
	if (config->dualMonoMode > 1U || (config->dualMonoMode == 1U && channels != 2))
		return NAAC_ERR_INVALID_PARAM;
	if (config->sbrFlag > 1U || config->sbrRatio > 3U || config->psFlag > 1U)
		return NAAC_ERR_INVALID_PARAM;

	if (config->channelOrder != NULL) {
		for (int32_t ch = 0; ch < channels; ch++) {
			int32_t ord = config->channelOrder[ch];
			if (config->channelStride != NULL) {
				int32_t stride = config->channelStride[ch];
				if (stride < 0) {
					if (ord <= stride || ord > 0)
						return NAAC_ERR_INVALID_PARAM;
				} else {
					if (ord < 0 || ord >= stride)
						return NAAC_ERR_INVALID_PARAM;
				}
			} else {
				if (ord < 0 || ord >= channels)
					return NAAC_ERR_INVALID_PARAM;
			}
		}
	}

	handle->numChannels = channels;
	handle->pnsDisable = (config->disablePns == 0U) ? 1 : 0;

	const char *commentSrc = (config->comment != NULL) ? config->comment : NAAC_DEFAULT_COMMENT;
	int32_t commentIdx = 0;
	while (commentIdx < 254 && commentSrc[commentIdx] != '\0') {
		handle->commentBuffer[commentIdx] = commentSrc[commentIdx];
		commentIdx++;
	}
	handle->commentBuffer[commentIdx] = '\0';

	sceClibMemset(handle->pcmWork, 0, (size_t)(channels * NAAC_FRAME_SAMPLES) * sizeof(float));

	int32_t mapErr = __f98aacInitChannelMappingFromConfiguration(handle->channelMap, channels);
	if (mapErr != NAAC_OK)
		return NAAC_ERR_INVALID_PARAM;

	handle->highBitrateMode = 0;
	if ((channels == 1 && config->bitRate >= 64000) ||
		(channels == 2 && config->bitRate >= 96000) ||
		(channels == 6 && config->bitRate >= 256000)) {
		handle->highBitrateMode = 1;
	}

	for (int32_t ch = 0; ch < channels; ch++) {
		handle->channelOrder[ch] = (config->channelOrder != NULL) ? config->channelOrder[ch] : ch;
		handle->channelStride[ch] = (config->channelStride != NULL) ? config->channelStride[ch] : channels;
	}

	int32_t writeAdts = (config->streamFormat == 0 || config->streamFormat == 2 || config->enableAdts != 0U) ? 1 : 0;
	int32_t openErr = __f98aacOpen(handle->coreEncoder, handle->channelMap, config->sampleRate, config->bitRate, writeAdts);
	handle->lastCoreError = openErr;
	if (openErr != NAAC_OK)
		return NAAC_ERR_INTERNAL;

	NaacCoreInfo infoCopy;
	int32_t infoErr = __f98aacGetInfo(handle->coreEncoder, &infoCopy);
	handle->lastCoreError = infoErr;
	if (infoErr != NAAC_OK)
		return NAAC_ERR_INTERNAL;

	float cutoffRatio = 1.0f;
	if (handle->highBitrateMode == 0 && config->sampleRate > 0) {
		cutoffRatio = (infoCopy.cutoffFreqHz * 2.0f) / (float)config->sampleRate;
		if (cutoffRatio > 1.0f)
			cutoffRatio = 1.0f;
	}
	handle->cutoffGain = 1.0f;
	handle->cutoffNorm = cutoffRatio;
	handle->filterAlpha = 80.0f;

	if (naacLowpassInit(&handle->lowpass, channels, cutoffRatio) != NAAC_OK)
		return NAAC_ERR_INTERNAL;

	handle->encoderDelay = infoCopy.encoderDelay + handle->lowpass.delaySamples;

	/* Generate AudioSpecificConfig */
	NaacBitBuffer ascBitBuf;
	naacBitBufInit(&ascBitBuf, handle->ascBuffer, (int32_t)sizeof(handle->ascBuffer));
	int32_t ascBits = 0;
	if (naacWriteAudioSpecificConfig(&ascBitBuf, &infoCopy, &ascBits) != NAAC_OK)
		return NAAC_ERR_INTERNAL;
	handle->ascBytes = (ascBits + 7) >> 3;

	/* Generate StreamMuxConfig */
	NaacBitBuffer latmBitBuf;
	naacBitBufInit(&latmBitBuf, handle->latmBuffer, (int32_t)sizeof(handle->latmBuffer));
	int32_t latmBits = 0;
	if (naacWriteStreamMuxConfig(&latmBitBuf, &infoCopy, &latmBits) != NAAC_OK)
		return NAAC_ERR_INTERNAL;
	handle->latmBytes = (latmBits + 7) >> 3;

	handle->pcmFormat = NAAC_PCM_INT16;
	handle->bytesPerSample = 2;
	handle->convertPcm = naacConvertPcmInt16;
	handle->isConfigured = 1;
	handle->flushFramesRemaining = 0;
	handle->bufferedSamples = 0;
	return NAAC_OK;
}

int32_t SceAacencInternal_C1876820(NaacHandle *handle)
{
	if (handle == NULL || handle->magic != NAAC_HANDLE_MAGIC || handle->isConfigured == 0)
		return NAAC_ERR_INVALID_HANDLE;

	handle->bufferedSamples = 0;
	sceClibMemset(handle->pcmWork, 0, sizeof(handle->pcmWork));
	int32_t err = naacLowpassInit(&handle->lowpass, handle->numChannels, handle->cutoffNorm);
	if (err != NAAC_OK)
		return NAAC_ERR_INTERNAL;
	handle->flushFramesRemaining = 0;
	return NAAC_OK;
}

int32_t SceAacencInternal_9EB15569(NaacHandle *handle, int32_t pcmFormat, NaacBufferSizes *outSizes)
{
	if (handle == NULL || handle->magic != NAAC_HANDLE_MAGIC || handle->isConfigured == 0)
		return NAAC_ERR_INVALID_HANDLE;
	if (outSizes == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t sampleBytes = 0;
	switch (pcmFormat) {
	case NAAC_PCM_INT16:
		sampleBytes = 2;
		handle->convertPcm = naacConvertPcmInt16;
		break;
	case NAAC_PCM_INT24:
		sampleBytes = 3;
		handle->convertPcm = naacConvertPcmInt24;
		break;
	case NAAC_PCM_INT32:
		sampleBytes = 4;
		handle->convertPcm = naacConvertPcmInt32;
		break;
	case NAAC_PCM_FLOAT32:
		sampleBytes = 4;
		handle->convertPcm = naacConvertPcmFloat32;
		break;
	default:
		return NAAC_ERR_INVALID_PARAM;
	}

	handle->pcmFormat = pcmFormat;
	handle->bytesPerSample = sampleBytes;
	outSizes->inputBytes = sampleBytes * (handle->numChannels * NAAC_FRAME_SAMPLES);
	outSizes->outputBytes = 777 * handle->numChannels + 125;
	return NAAC_OK;
}

int32_t SceAacencInternal_C58CA5A8(NaacHandle *handle, int32_t *outDelay)
{
	if (handle == NULL || handle->magic != NAAC_HANDLE_MAGIC)
		return NAAC_ERR_INVALID_HANDLE;
	if (handle->isConfigured == 0) {
		if (outDelay != NULL)
			*outDelay = 0;
		return NAAC_ERR_INVALID_HANDLE;
	}
	if (outDelay == NULL)
		return NAAC_ERR_INVALID_PARAM;
	*outDelay = handle->encoderDelay;
	return NAAC_OK;
}

int32_t SceAacencInternal_88BF7DA5(NaacHandle *handle, uint32_t headerFlag)
{
	if (handle == NULL || handle->magic != NAAC_HANDLE_MAGIC)
		return NAAC_ERR_INVALID_HANDLE;
	if (headerFlag > 1U)
		return NAAC_ERR_INVALID_PARAM;
	if (handle->isConfigured == 0)
		return NAAC_ERR_INVALID_HANDLE;

	naacCoreSetHeaderMode(handle->coreEncoder, (int32_t)headerFlag);
	return NAAC_OK;
}

int32_t SceAacencInternal_B8BEEDE6(NaacHandle *handle, const void *pcmIn, uint32_t inputBytes,
	uint8_t *outBuf, int32_t *outBytes, int32_t maxOutBytes)
{
	if (handle == NULL || handle->magic != NAAC_HANDLE_MAGIC)
		return NAAC_ERR_INVALID_HANDLE;
	if (handle->isConfigured == 0) {
		if (outBytes != NULL)
			*outBytes = 0;
		return NAAC_ERR_INVALID_HANDLE;
	}
	if (outBytes == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t isFlush = (pcmIn == NULL && inputBytes == 0U) ? 1 : 0;
	if (isFlush != 0) {
		if (handle->flushFramesRemaining == 0) {
			*outBytes = 0;
			return NAAC_OK;
		}
		return naacFlushInternal(handle, outBuf, outBytes, maxOutBytes);
	}

	if (pcmIn == NULL || (int32_t)inputBytes < 0 || outBuf == NULL) {
		*outBytes = 0;
		return NAAC_ERR_INVALID_PARAM;
	}

	int32_t frameSamples = handle->numChannels * NAAC_FRAME_SAMPLES;
	int32_t sampleCount = (handle->bytesPerSample == 2) ? (int32_t)(inputBytes >> 1) :
		((handle->bytesPerSample == 4) ? (int32_t)(inputBytes >> 2) :
		(int32_t)((float)inputBytes / (float)handle->bytesPerSample));
	if (sampleCount > frameSamples) {
		*outBytes = 0;
		return NAAC_ERR_INVALID_PARAM;
	}

	handle->convertPcm(handle->pcmWork, pcmIn, sampleCount);
	if (sampleCount < frameSamples) {
		sceClibMemset(&handle->pcmWork[sampleCount], 0,
			(size_t)(frameSamples - sampleCount) * sizeof(float));
	}
	handle->bufferedSamples = frameSamples;

	float filtered[NAAC_FRAME_SAMPLES * NAAC_MAX_CHANNELS];
	if (naacLowpassProcess(&handle->lowpass, handle->pcmWork, frameSamples, filtered, frameSamples) == 0) {
		*outBytes = 0;
		return NAAC_ERR_INTERNAL;
	}

	int32_t produced = 0;
	int32_t err = naacCoreEncodeFrame(handle->coreEncoder, filtered, outBuf, &produced, maxOutBytes);
	handle->lastCoreError = err;
	if (err != NAAC_OK) {
		*outBytes = produced;
		return NAAC_ERR_INTERNAL;
	}

	handle->flushFramesRemaining = ((handle->encoderDelay + 1023) >> 10) + 1;
	*outBytes = produced;
	return NAAC_OK;
}

int32_t SceAacencInternal_30D75275(NaacHandle *handle, uint8_t *outBuf, int32_t *outBytes)
{
	if (handle == NULL || handle->magic != NAAC_HANDLE_MAGIC)
		return NAAC_ERR_INVALID_HANDLE;
	if (handle->isConfigured == 0) {
		if (outBytes != NULL)
			*outBytes = 0;
		return NAAC_ERR_INVALID_HANDLE;
	}
	if (outBuf == NULL || outBytes == NULL) {
		if (outBytes != NULL)
			*outBytes = 0;
		return NAAC_ERR_INVALID_PARAM;
	}
	if (handle->flushFramesRemaining == 0) {
		*outBytes = 0;
		return NAAC_OK;
	}
	int32_t maxBytes = 777 * handle->numChannels + 125;
	return naacFlushInternal(handle, outBuf, outBytes, maxBytes);
}

int32_t SceAacencInternal_7EC218B7(NaacHandle *handle, uint8_t *outBuf, int32_t *outBytes)
{
	if (handle == NULL || handle->magic != NAAC_HANDLE_MAGIC)
		return NAAC_ERR_INVALID_HANDLE;
	if (handle->isConfigured == 0) {
		if (outBytes != NULL)
			*outBytes = 0;
		return NAAC_ERR_INVALID_HANDLE;
	}
	if (outBuf == NULL && outBytes == NULL)
		return NAAC_ERR_INVALID_PARAM;
	if (outBuf != NULL)
		sceClibMemcpy_safe(outBuf, handle->ascBuffer, (size_t)handle->ascBytes);
	if (outBytes != NULL)
		*outBytes = handle->ascBytes;
	return NAAC_OK;
}

int32_t SceAacencInternal_50891A4F(NaacHandle *handle, uint8_t *outBuf, int32_t *outBytes)
{
	if (handle == NULL || handle->magic != NAAC_HANDLE_MAGIC)
		return NAAC_ERR_INVALID_HANDLE;
	if (handle->isConfigured == 0) {
		if (outBytes != NULL)
			*outBytes = 0;
		return NAAC_ERR_INVALID_HANDLE;
	}
	if (outBuf == NULL && outBytes == NULL)
		return NAAC_ERR_INVALID_PARAM;
	if (outBuf != NULL)
		sceClibMemcpy_safe(outBuf, handle->latmBuffer, (size_t)handle->latmBytes);
	if (outBytes != NULL)
		*outBytes = handle->latmBytes;
	return NAAC_OK;
}

/* === SceAacenc Public Wrapper Exports === */

int32_t SceAacenc_03C4FED0(void)
{
	return SceAacencInternal_118F588C();
}

uint32_t SceAacenc_FCF8B786(NaacHandle **handleOut, void *workBuffer)
{
	return naacMapPublicError(SceAacencInternal_71BDD121(handleOut, workBuffer));
}

uint32_t SceAacenc_5CB16FD6(NaacHandle *handle)
{
	return naacMapPublicError(SceAacencInternal_B3004C1B(handle));
}

uint32_t SceAacenc_F5F58A6B(NaacHandle *handle, const NaacConfig *config)
{
	if (config == NULL)
		return naacMapPublicError(NAAC_ERR_INVALID_PARAM);
	NaacConfig localConfig;
	sceClibMemcpy_safe(&localConfig, config, sizeof(NaacConfig));
	return naacMapPublicError(SceAacencInternal_C867CE87(handle, &localConfig));
}

uint32_t SceAacenc_98A9AF7C(NaacHandle *handle)
{
	return naacMapPublicError(SceAacencInternal_C1876820(handle));
}

uint32_t SceAacenc_93EFB09C(NaacHandle *handle, int32_t pcmFormat, NaacBufferSizes *outSizes)
{
	return naacMapPublicError(SceAacencInternal_9EB15569(handle, pcmFormat, outSizes));
}

uint32_t SceAacenc_0EDE0FBD(NaacHandle *handle, int32_t *outDelay)
{
	return naacMapPublicError(SceAacencInternal_C58CA5A8(handle, outDelay));
}

uint32_t SceAacenc_FCB3AEF4(NaacHandle *handle, const void *pcmIn, uint32_t inputBytes,
	uint8_t *outBuf, int32_t *outBytes, int32_t maxOutBytes)
{
	return naacMapPublicError(SceAacencInternal_B8BEEDE6(handle, pcmIn, inputBytes, outBuf, outBytes, maxOutBytes));
}

uint32_t SceAacenc_EF8BD582(NaacHandle *handle, uint32_t headerFlag)
{
	return naacMapPublicError(SceAacencInternal_88BF7DA5(handle, headerFlag));
}

uint32_t SceAacenc_E539FF41(NaacHandle *handle, uint8_t *outBuf, int32_t *outBytes)
{
	return naacMapPublicError(SceAacencInternal_30D75275(handle, outBuf, outBytes));
}

uint32_t SceAacenc_61BA27BC(NaacHandle *handle, uint8_t *outBuf, int32_t *outBytes)
{
	return naacMapPublicError(SceAacencInternal_7EC218B7(handle, outBuf, outBytes));
}

uint32_t SceAacenc_49ADDF5C(NaacHandle *handle, uint8_t *outBuf, int32_t *outBytes)
{
	return naacMapPublicError(SceAacencInternal_50891A4F(handle, outBuf, outBytes));
}
