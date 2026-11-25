#include "g729_internal.h"

static int32_t g729MapInternalError(uint32_t code)
{
	if (code >= 0x65U && code <= 0x67U)
		return 0;
	if (code >= 257U && code <= 259U)
		return SCE_G729_ERROR_CORRUPT_DATA;
	return SCE_G729_ERROR_INTERNAL;
}

static int32_t g729EncoderInitInternal(G729EncoderContext *ctx)
{
	if (ctx->paramRate > G729_RATE_11800 || ctx->paramVad > 1U)
		return -1;
	ctx->activeRate = ctx->paramRate;
	ctx->activeVad = ctx->paramVad;
	ctx->coder.frameNumber = 0;
	g729PreProcessInit(&ctx->coder.hpf);
	g729InitCoder((int32_t)ctx->activeVad, &ctx->coder);
	if (ctx->activeVad == 1U)
		g729VadInit(&ctx->coder.vad);
	return 0;
}

static void g729EncoderSyncStatus(G729EncoderContext *ctx)
{
	ctx->statusRate = ctx->activeRate;
	ctx->statusVad = ctx->activeVad;
	ctx->statusBits = ctx->encodedBits;
	ctx->statusError = (uint32_t)ctx->coder.internalError;
}

static int32_t g729EncoderProcessFrame(const int16_t *pcmIn, uint8_t *bitstreamOut, G729EncoderContext *ctx)
{
	int32_t prm[G729_MAX_PRM];
	int32_t bitCount = 0;
	sceClibMemset(prm, 0, sizeof(prm));

	ctx->coder.internalError = 0;
	ctx->encodedBits = 0;
	if (ctx->coder.frameNumber == 0x7FFF)
		ctx->coder.frameNumber = 256;
	else
		ctx->coder.frameNumber++;

	float *dst = ctx->coder.newSpeech;
	for (int i = 0; i < G729_FRAME_LEN; i++)
		dst[i] = (float)pcmIn[i];

	g729PreProcess(dst, G729_FRAME_LEN, &ctx->coder.hpf);

	if (g729CoderFrame(prm, ctx->coder.frameNumber, (int32_t)ctx->activeVad, ctx->activeRate, &ctx->coder) != 0)
		return -1;
	if (g729Prm2Bits(prm, bitstreamOut, &bitCount, ctx->activeRate) != 0)
		return -1;

	ctx->encodedBits = bitCount;
	return (ctx->coder.internalError != 0) ? -1 : 0;
}

static int32_t g729DecoderInitInternal(G729DecoderContext *ctx)
{
	ctx->core.lastOrder = G729_LPC_ORDER;
	ctx->core.lastRate = (int32_t)G729_BITS_8000;
	g729InitDecoder(&ctx->core);
	g729PostFilterInit(&ctx->core.postFilter);
	g729PostProcessInit(&ctx->core.postHpf);
	return 0;
}

static void g729DecoderSyncStatus(G729DecoderContext *ctx)
{
	ctx->decodedBitLen = (int32_t)ctx->activeBitLen;
	ctx->statusBfi = ctx->activeBfi;
	ctx->statusError = (uint32_t)ctx->core.internalError;
}

static int32_t g729DecoderProcessFrame(const uint8_t *bitstreamIn, int16_t *pcmOut, G729DecoderContext *ctx)
{
	uint32_t bitLen = ctx->activeBitLen;
	int32_t bfi = (int32_t)ctx->activeBfi;
	float azDec[(G729_LPC_ORDER_BWD + 1) * 2];
	int32_t pitchDelays[2];
	float postSynth[G729_FRAME_LEN];
	int32_t lpcOrder = G729_LPC_ORDER;
	int32_t voicing = 0;

	ctx->core.internalError = 0;
	if (g729Bits2Prm(bitstreamIn, ctx->core.prmBuf, bitLen, &bfi) != 0)
		return -1;

	ctx->activeBfi = (uint32_t)bfi;
	if (bitLen == G729_BITS_8000) {
		ctx->core.prmBuf[7] = g729CheckParityPitch(ctx->core.prmBuf[6], ctx->core.prmBuf[7]);
	} else if (bitLen == G729_BITS_11800) {
		if (ctx->core.prmBuf[2] != 0) {
			int32_t pIdx = ctx->core.prmBuf[3];
			int32_t pBit = ctx->core.prmBuf[4] + (((pIdx & 2) != 0) ? 1 : 0);
			ctx->core.prmBuf[4] = g729CheckParityPitch(pIdx, pBit);
		} else {
			int32_t pIdx = ctx->core.prmBuf[5];
			int32_t pBit = ctx->core.prmBuf[6] + (((pIdx & 2) != 0) ? 1 : 0);
			ctx->core.prmBuf[6] = g729CheckParityPitch(pIdx, pBit);
		}
	}

	ctx->core.prmBuf[0] = bfi;
	if (bfi == 1)
		bitLen = (ctx->core.lastRate <= 63) ? 0U : (uint32_t)ctx->core.lastRate;

	g729DecodeFrame(ctx->core.prmBuf, ctx->core.voicingFlag, ctx->core.synthFrame, azDec, pitchDelays, &lpcOrder, &ctx->core);

	if (ctx->core.lastOrder != lpcOrder)
		g729PostFilterInit(&ctx->core.postFilter);

	int32_t maxOrder = 20;
	if (bitLen == G729_BITS_11800) {
		maxOrder = 32;
		if (ctx->core.prmBuf[2] == 1 && ctx->core.bwd.musicFlag == 1) {
			ctx->core.postFilter.gammaHarm -= 0.0125f;
			ctx->core.postFilter.gammaDen -= 0.035f;
			ctx->core.postFilter.gammaNum -= 0.0325f;
			if (ctx->core.postFilter.gammaHarm < 0.0f)
				ctx->core.postFilter.gammaHarm = 0.0f;
			if (ctx->core.postFilter.gammaDen < 0.0f)
				ctx->core.postFilter.gammaDen = 0.0f;
			if (ctx->core.postFilter.gammaNum < 0.0f)
				ctx->core.postFilter.gammaNum = 0.0f;
		} else {
			ctx->core.postFilter.gammaHarm += 0.0125f;
			ctx->core.postFilter.gammaDen += 0.035f;
			ctx->core.postFilter.gammaNum += 0.0325f;
			if (ctx->core.postFilter.gammaHarm > 0.25f)
				ctx->core.postFilter.gammaHarm = 0.25f;
			if (ctx->core.postFilter.gammaDen > 0.70f)
				ctx->core.postFilter.gammaDen = 0.70f;
			if (ctx->core.postFilter.gammaNum > 0.65f)
				ctx->core.postFilter.gammaNum = 0.65f;
		}
	} else {
		ctx->core.postFilter.gammaDen = 0.70f;
		ctx->core.postFilter.gammaNum = 0.55f;
		ctx->core.postFilter.gammaHarm = 0.50f;
	}

	for (int i = 0; i < G729_FRAME_LEN; i++)
		postSynth[i] = ctx->core.synthFrame[i];

	if (lpcOrder == G729_LPC_ORDER) {
		g729PostFilterShort(ctx->core.synthFrame, azDec, pitchDelays, postSynth, &ctx->core.postFilter);
	} else {
		ctx->core.voicingFlag = 0;
		g729PostFilter(pitchDelays[0], ctx->core.synthFrame, azDec, postSynth, &voicing, maxOrder, lpcOrder, &ctx->core.postFilter);
		if (voicing != 0)
			ctx->core.voicingFlag = voicing;
		g729PostFilter(pitchDelays[1], &ctx->core.synthFrame[G729_SUBFRAME_LEN], &azDec[lpcOrder + 1],
		               &postSynth[G729_SUBFRAME_LEN], &voicing, maxOrder, lpcOrder, &ctx->core.postFilter);
		if (voicing != 0)
			ctx->core.voicingFlag = voicing;
	}

	g729PostProcess(postSynth, G729_FRAME_LEN, &ctx->core.postHpf);

	ctx->core.lastOrder = lpcOrder;
	ctx->core.lastRate = (int32_t)bitLen;

	for (int i = 0; i < G729_FRAME_LEN; i++) {
		float sample = postSynth[i];
		sample = (sample < 0.0f) ? (sample - 0.5f) : (sample + 0.5f);
		if (sample > 32767.0f)
			sample = 32767.0f;
		if (sample < -32768.0f)
			sample = -32768.0f;
		pcmOut[i] = (int16_t)(int32_t)sample;
	}

	return (ctx->core.internalError != 0) ? -1 : 0;
}

int32_t sceG729EncodeGetContextSize(void)
{
	return (int32_t)G729_ENCODER_CONTEXT_SIZE;
}

int32_t sceG729EncodeInit(void *context, uint32_t rate, uint32_t vadEnable)
{
	G729EncoderContext *ctx = (G729EncoderContext *)context;
	if (ctx == NULL || (((uintptr_t)ctx & 3U) != 0) || rate > G729_RATE_11800 || vadEnable > 1U)
		return SCE_G729_ERROR_INVALID_PARAM;

	ctx->paramRate = rate;
	ctx->paramVad = vadEnable;

	if (g729EncoderInitInternal(ctx) != 0) {
		int32_t err = g729MapInternalError(ctx->statusError);
		if (err != 0) {
			ctx->initialized = 0;
			return err;
		}
	} else {
		g729EncoderSyncStatus(ctx);
	}

	ctx->initialized = 1;
	return SCE_G729_OK;
}

int32_t sceG729EncodeReset(void *context)
{
	G729EncoderContext *ctx = (G729EncoderContext *)context;
	if (ctx == NULL || (((uintptr_t)ctx & 3U) != 0))
		return SCE_G729_ERROR_INVALID_PARAM;
	if (ctx->initialized != 1)
		return SCE_G729_ERROR_NOT_INITIALIZED;

	ctx->paramRate = ctx->activeRate;
	ctx->paramVad = ctx->activeVad;

	if (g729EncoderInitInternal(ctx) != 0)
		return g729MapInternalError(ctx->statusError);

	g729EncoderSyncStatus(ctx);
	return SCE_G729_OK;
}

int32_t sceG729EncodeCore(void *context, const int16_t *pcmIn, uint8_t *bitstreamOut)
{
	G729EncoderContext *ctx = (G729EncoderContext *)context;
	if (ctx == NULL || (((uintptr_t)ctx & 3U) != 0))
		return SCE_G729_ERROR_NOT_INITIALIZED;
	if (pcmIn == NULL || bitstreamOut == NULL || (((uintptr_t)pcmIn & 1U) != 0))
		return SCE_G729_ERROR_INVALID_PARAM;
	if (ctx->initialized != 1)
		return SCE_G729_ERROR_NOT_INITIALIZED;

	int32_t ret = g729EncoderProcessFrame(pcmIn, bitstreamOut, ctx);
	g729EncoderSyncStatus(ctx);
	if (ret == 0)
		return ctx->statusBits;
	return g729MapInternalError(ctx->statusError);
}

int32_t sceG729DecodeGetContextSize(void)
{
	return (int32_t)G729_DECODER_CONTEXT_SIZE;
}

int32_t sceG729DecodeReset(void *context)
{
	G729DecoderContext *ctx = (G729DecoderContext *)context;
	if (ctx == NULL)
		return SCE_G729_ERROR_INVALID_PARAM;
	if (ctx->initialized != 1)
		return SCE_G729_ERROR_NOT_INITIALIZED;

	if (g729DecoderInitInternal(ctx) != 0)
		return g729MapInternalError(ctx->statusError);

	g729DecoderSyncStatus(ctx);
	return SCE_G729_OK;
}

int32_t sceG729DecodeInit(void *context)
{
	G729DecoderContext *ctx = (G729DecoderContext *)context;
	if (ctx == NULL || (((uintptr_t)ctx & 3U) != 0))
		return SCE_G729_ERROR_INVALID_PARAM;

	sceClibMemset(ctx, 0, G729_DECODER_CONTEXT_SIZE);
	ctx->initialized = 1;

	if (g729DecoderInitInternal(ctx) != 0) {
		int32_t err = g729MapInternalError(ctx->statusError);
		if (err != 0) {
			ctx->initialized = 0;
			return err;
		}
		return SCE_G729_OK;
	}

	g729DecoderSyncStatus(ctx);
	return SCE_G729_OK;
}

int32_t sceG729DecodeCore(void *context, uint32_t bitLength, uint32_t bfi, const uint8_t *bitstreamIn, int16_t *pcmOut)
{
	G729DecoderContext *ctx = (G729DecoderContext *)context;
	if (ctx == NULL || (((uintptr_t)ctx & 3U) != 0))
		return SCE_G729_ERROR_NOT_INITIALIZED;
	if (bitstreamIn == NULL || pcmOut == NULL || (((uintptr_t)pcmOut & 1U) != 0))
		return SCE_G729_ERROR_INVALID_PARAM;
	if (bitLength != G729_BITS_DTX && bitLength != G729_BITS_SID && bitLength != G729_BITS_SID_ALT &&
	    bitLength != G729_BITS_6400 && bitLength != G729_BITS_8000 && bitLength != G729_BITS_11800) {
		return SCE_G729_ERROR_INVALID_PARAM;
	}
	if (bfi > 1U)
		return SCE_G729_ERROR_INVALID_PARAM;
	if (ctx->initialized != 1)
		return SCE_G729_ERROR_NOT_INITIALIZED;

	ctx->paramBitLen = bitLength;
	ctx->paramBfi = bfi;
	ctx->activeBitLen = bitLength;
	ctx->activeBfi = bfi;

	int32_t ret = g729DecoderProcessFrame(bitstreamIn, pcmOut, ctx);
	g729DecoderSyncStatus(ctx);
	if (ret == 0)
		return ctx->decodedBitLen;
	return g729MapInternalError(ctx->statusError);
}
