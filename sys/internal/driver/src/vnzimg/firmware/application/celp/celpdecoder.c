/*
 * The CELP decoder engine (0x8546e4-0x854b3c), called by celpdec.c: the
 * memory the ARM gives holds a CelpDecoderState 8 bytes in; a bit rate
 * index (from the stream's bit rate, or its two header bytes) sets up the
 * frame structure; a frame is decoded to 16-bit samples.
 */
#include "celp.h"

/* 0x854b3c */
static int32_t celpDecoderTerminated(void)
{
	return 0;
}

/* 0x8546e4: whether the engine decodes this bit rate index */
static int32_t celpModeIndexValid(int32_t index)
{
	int32_t i;

	for (i = 0; i < 10; i++)
		if (celpModeIndices[i] == index)
			return 1;
	return 0;
}

/* 0x8546fe: the memory a decoder takes and its alignment */
int32_t celpDecoderQuery(uint32_t *size, uint32_t *alignment)
{
	if (size == NULL || alignment == NULL)
		return CELP_ERROR_POINTER;
	*size = 17408;
	*alignment = 0x20000;
	return 0;
}

/* 0x854940: the structure of the frames at the state's bit rate */
static void celpDecoderSetup(CelpDecoderState *state)
{
	int32_t i;

	if (state->align)
		celpBitsReset(state);
	state->frameLengthCopy = celpFrameLengths[state->bitRateIndex];
	state->order = CELP_ORDER;
	state->subframes = celpSubframeCounts[state->bitRateIndex];
	state->subframeLength = celpSubframeLengths[state->bitRateIndex];
	state->frameLength = state->frameLengthCopy;
	state->pulseCount = celpSignBits[state->bitRateIndex];
	for (i = 0; i < state->order; i++) {
		state->previousLsp[i] = (int32_t)(((double)i + 1.0) * 16777216.0 / (double)(state->order + 1));
		state->filterState[i] = 0;
	}
	memset(state->history, 0, 193 * 8);
	state->previousGainLevel = 0;
	celpBuildPulses(state, state->subframeLength, state->pulseCount, state->pulseWidth,
		state->pulsePosition);
	memset(state->lspIndex, 0, 11 * sizeof(int32_t));
	state->frameBits = celpFrameBits(state);
	state->lostFrames = 0;
}

/* 0x85471a */
int32_t celpDecoderInit(void *engine, uint32_t size)
{
	CelpDecoderState *state;

	if (engine == NULL)
		return CELP_ERROR_POINTER;
	if (size < 0x2978)
		return CELP_ERROR_SIZE;
	state = celpDecoderState(engine);
	state->frameCounter = 0;
	state->invalid = 0;
	state->bitRateIndex = 0;
	celpDecoderSetup(state);
	return 0;
}

/* 0x854758 */
int32_t celpDecoderTerminate(void *engine)
{
	if (engine == NULL)
		return CELP_ERROR_POINTER;
	celpDecoderTerminated();
	return 0;
}

/* 0x854774: back to the first bit rate */
int32_t celpDecoderReset(void *engine)
{
	CelpDecoderState *state;

	if (engine == NULL)
		return CELP_ERROR_POINTER;
	state = celpDecoderState(engine);
	state->invalid = 0;
	state->bitRateIndex = 0;
	celpDecoderSetup(state);
	return 0;
}

/* 0x85479c */
int32_t celpDecoderSetBitRate(void *engine, const int32_t *bitRateIndex)
{
	CelpDecoderState *state;

	if (engine == NULL || bitRateIndex == NULL)
		return CELP_ERROR_POINTER;
	state = celpDecoderState(engine);
	state->invalid = 0;
	state->bitRateIndex = *bitRateIndex;
	if (!celpModeIndexValid(*bitRateIndex)) {
		state->invalid = 1;
		return CELP_ERROR_STATE;
	}
	celpDecoderSetup(state);
	return 0;
}

/*
 * 0x8547e6: the bit rate from the stream's two header bytes (the index in
 * the low 5 bits of the first, and zero in the others)
 */
int32_t celpDecoderParseHeader(void *engine, int32_t *bitRateIndex, const uint8_t *header)
{
	CelpDecoderState *state;
	int32_t index;

	if (engine == NULL || bitRateIndex == NULL || header == NULL)
		return CELP_ERROR_POINTER;
	state = celpDecoderState(engine);
	state->invalid = 0;
	index = header[0] & 0x1f;
	if ((header[0] >> 7 & 1) || (header[0] >> 6 & 1) || (header[0] >> 5 & 1)
		|| (header[1] >> 6 & 3) || (header[1] >> 5 & 1) || (header[1] >> 3 & 3)) {
		state->invalid = 1;
		return CELP_ERROR_STATE;
	}
	if (!celpModeIndexValid(index)) {
		state->invalid = 1;
		return CELP_ERROR_STATE;
	}
	state->bitRateIndex = index;
	*bitRateIndex = index;
	celpDecoderSetup(state);
	return 0;
}

/* 0x854a72: the frame's samples, rounded to 16 bits and clamped */
static void celpDecodeFrame(CelpDecoderState *state)
{
	int32_t i;

	state->align = 1;
	celpBitsReset(state);
	celpDecodeHeader(state);
	celpDecodeLsp(state);
	celpDecodeExcitation(state);
	celpSynthesize(state);
	for (i = 0; i < state->frameLength; i++) {
		int32_t sample = (state->output[i] + 16384) >> 15;

		if (sample >= 0x8000)
			sample = 32767;
		else
			sample = max(sample, -32767);
		state->pcm[i] = sample;
	}
}

/* 0x85487a: a frame of `es`; `esUsed` the bytes it took and `pcmSize` the bytes written */
int32_t celpDecode(void *engine, const uint8_t *es, uint32_t esSize, uint32_t *esUsed,
	int16_t *pcm, uint32_t *pcmSize, uint32_t lostCount)
{
	CelpDecoderState *state;

	if (engine == NULL || es == NULL || pcm == NULL)
		return CELP_ERROR_POINTER;
	state = celpDecoderState(engine);
	if (state->invalid)
		return CELP_ERROR_STATE;
	state->pcm = pcm;
	state->stream = es;
	state->lostCount = lostCount;
	celpDecodeFrame(state);
	*esUsed = (state->frameBits + 7) >> 3;
	*pcmSize = state->frameLength * 2;
	state->frameCounter++;
	return 0;
}
