/*
 * The CELP encoder engine (0x854b82-0x857932), called by celpenc.c: the
 * memory the ARM gives holds a CelpEncoderState 8 bytes in. A bit rate
 * sets up the frame structure; a frame of samples is analysed and written
 * as bits (one frame late), after the stream's ".mp4" header.
 */
#include "celp.h"

/* the bit writer: most significant bit first; ORs into the byte it starts in and writes the next three */
static void putBits(CelpEncoderState *state, uint32_t value, int32_t width)
{
	uint8_t *bytes = state->writeBase + (state->writePosition >> 3);
	uint32_t shifted = value << (32 - (state->writePosition & 7) - width);

	bytes[0] |= shifted >> 24;
	bytes[1] = shifted >> 16;
	bytes[2] = shifted >> 8;
	bytes[3] = shifted;
	state->writePosition += width;
}

/* the same for up to 32 bits, which may reach a fifth byte */
static void putBitsWide(CelpEncoderState *state, uint32_t value, int32_t width)
{
	uint8_t *bytes = state->writeBase + (state->writePosition >> 3);
	int32_t shift = 32 - (state->writePosition & 7) - width;

	if (shift < 0) {
		bytes[4] = value << (shift + 8);
		value >>= -shift;
	} else {
		value <<= shift;
		bytes[4] = 0;
	}
	bytes[0] |= value >> 24;
	bytes[1] = value >> 16;
	bytes[2] = value >> 8;
	bytes[3] = value;
	state->writePosition += width;
}

/* 0x854f3e */
static void writerStart(CelpEncoderState *state)
{
	state->writeBase = state->stream;
	state->writePosition = 0;
	state->stream[0] = 0;
}

/* 0x854f50: the byte the last frame ended in starts the next */
static void writerContinue(CelpEncoderState *state)
{
	state->writeBase[0] = state->writeBase[state->writePosition >> 3];
	state->writePosition &= 7;
}

/* 0x854f6e: the frame's indices, in the order the decoder reads them */
static void packFrame(CelpEncoderState *state)
{
	int32_t subframe;

	putBits(state, state->lspIndex[0], 4);
	putBits(state, state->lspIndex[1], 4);
	putBits(state, state->lspIndex[2], 7);
	putBits(state, state->lspIndex[3], 6);
	putBits(state, state->lspPredicted, 1);
	putBits(state, state->gainMode, 2);
	putBits(state, state->gainIndex, 6);
	for (subframe = 0; subframe < state->subframes; subframe++) {
		putBits(state, state->lagIndex[subframe], 8);
		putBitsWide(state, state->positionIndex[subframe], celpEncPositionBits[state->modeIndex]);
		putBits(state, state->signIndex[subframe], celpEncSignBits[state->modeIndex]);
		putBits(state, state->gainCode[subframe], 6);
	}
	if (state->align)
		state->writePosition = (state->writePosition + 7) & -8;
}

/* 0x855452: the CELP configuration of the stream header (MPEG-4 CELP, 8 kHz MPE) */
static void writeConfiguration(CelpEncoderState *state)
{
	putBits(state, 0, 1);
	putBits(state, 0, 1);
	putBits(state, 0, 1);
	putBits(state, state->modeIndex, 5);
	putBits(state, 0, 2);
	putBits(state, 0, 1);
}

/* 0x857130: the bits of a frame */
static int32_t frameBitsOf(const CelpEncoderState *state)
{
	int32_t bits = 30, subframe;

	for (subframe = 0; subframe < state->subframes; subframe++)
		bits += 8 + celpEncPositionBits[state->modeIndex] + celpEncSignBits[state->modeIndex] + 6;
	if (state->align)
		bits = (bits + 7) & -8;
	return bits;
}

/* 0x855cae: the next frame's input behind the speech it follows */
static void shiftInput(CelpEncoderState *state)
{
	int32_t length = state->frameLength, i;

	for (i = 0; i < length + 80; i++)
		state->speech[i] = state->speech[length + i];
	for (i = 0; i < length; i++)
		state->speech[length + 80 + i] = state->input[i];
}

/* 0x855c74: the analysis window */
static void windowSetup(CelpEncoderState *state)
{
	int32_t i;

	for (i = 0; i < 120; i++)
		state->analysisWindow[i] = celpEncWindowHead[i];
	for (i = 120; i < state->analysisLength; i++)
		state->analysisWindow[i] = celpEncWindowTail[i - 120];
}

/*
 * 0x856f38: how the pulses of a subframe share its positions; the same
 * construction as the decoder's (celpBuildPulses) with a rounded spacing.
 * Returns the bits of the positions, or -1 when the groups cannot merge.
 */
static int32_t buildPulses(CelpEncoderState *state, int32_t length, int32_t pulses, int32_t *widths,
	int32_t (*positions)[CELP_MAX_SUBFRAME])
{
	int32_t slots[CELP_MAX_SUBFRAME / 2 + 1] = { 0 };
	int32_t list[CELP_MAX_SUBFRAME / 2 + 2], next[CELP_MAX_SUBFRAME / 2 + 2];
	int32_t half = length / 2;
	int32_t count = half, passCount, i, j, minimum, size, slotCount, pulse, slot, total;

	for (i = 0; i < half; i++)
		list[i] = 1;
	list[half] = 0;
	while (pulses < count) {
		list[count] = 0;
		passCount = count;
		j = 0;
		i = 0;
		while (count >= 1 && i < passCount) {
			int32_t a = list[i], b = list[i + 1];

			if (b != a) {
				next[j++] = a;
				i++;
				continue;
			}
			i += 2;
			count--;
			next[j++] = a + 1;
			if (!(pulses < count)) {
				while (j < count)
					next[j++] = list[i++];
				break;
			}
		}
		if (passCount == count)
			return -1;
		for (i = 0; i < count; i++)
			list[i] = next[i];
	}
	total = 0;
	for (i = 0; i < pulses; i++) {
		widths[i] = list[i];
		total += list[i];
	}
	minimum = 31;
	for (i = 0; i < pulses; i++)
		minimum = min(minimum, widths[i]);
	size = 1 << minimum;
	slotCount = length / size;
	for (i = 0; i < slotCount; i++)
		slots[i] = -1;
	for (pulse = 0; pulse < pulses; pulse++) {
		int32_t occupy = 1 << (widths[pulse] - minimum);
		int32_t taken = 0;
		int32_t step = (slotCount * 2 / occupy + 1) >> 1;

		slot = 0;
		while (taken < occupy) {
			if (slots[slot] == -1) {
				slots[slot] = pulse;
				slot = (slot + step) % slotCount;
				taken++;
			} else {
				slot++;
			}
		}
	}
	for (pulse = 0; pulse < pulses; pulse++) {
		int32_t filled = 0;

		for (slot = 0; slot < slotCount; slot++) {
			if (slots[slot] != pulse)
				continue;
			for (i = 0; i < size; i++)
				positions[pulse][filled + i] = i * slotCount + slot;
			filled += size;
		}
	}
	return total;
}

/* 0x85716a: the structure of the frames at a bit rate, and the memory cleared */
static void setupFrames(CelpEncoderState *state, int32_t bitRate)
{
	int32_t i;

	memset(state, 0, CELP_ENC_STATE_SIZE);
	state->bitRate = bitRate;
	state->frameCounter = 0;
	writerStart(state);
	for (i = 0; i < 32; i++) {
		if (celpEncModeBitRates[i] == bitRate) {
			state->modeIndex = i;
			break;
		}
	}
	state->frameLength = celpEncFrameLengths[state->modeIndex];
	state->subframes = celpEncSubframeCounts[state->modeIndex];
	state->subframeLength = celpEncSubframeLengths[state->modeIndex];
	state->field8244 = state->frameLength + 120;
	state->field9080 = celpEncPitchBlocks[state->modeIndex];
	state->field2244 = 5;
	state->field8236 = 40;
	state->frameLengthCopy = state->frameLength;
	state->order = CELP_ORDER;
	state->pulseCount = celpEncSignBits[state->modeIndex];
	state->analysisOffset[1] = 80;
	state->analysisOffset[0] = 0;
	state->analysisLength = 200;
	state->field9076 = 6;
	state->analysisOffset[2] = 160;
	state->analysisBlocks = state->field8248 = state->frameLength / state->field9080;
	state->analysisOffset[3] = 240;
	if (state->modeIndex == 27) {
		state->field8248 = 2;
		state->analysisOffset[0] = 80;
		state->analysisLength = 160;
	}
	windowSetup(state);
	memset(state->speech, 0, 760 * sizeof(int16_t));
	for (i = 0; i < state->order; i++) {
		state->angle[i] = 0;
		state->previousLsp[i] = celpEncInitialLsp[i];
	}
	state->field2792 = 0;
	memset(state->history, 0, 193 * 8);
	memset(state->field9088, 0, sizeof(state->field9088));
	for (i = 0; i < state->order; i++) {
		state->perceptualInput[i] = 0;
		state->perceptualOutput[i] = 0;
	}
	memset(state->weightedHistory, 0, 295 * sizeof(int16_t));
	buildPulses(state, state->subframeLength, state->pulseCount, state->pulseWidth,
		state->pulsePosition);
	state->field14800 = 100;
	state->frameBits = frameBitsOf(state);
}

/* 0x85735e: the stream header (header != 0), or a frame: the bits so far, or 0 for the first, which only fills the input */
static int32_t encodeFrame(CelpEncoderState *state, int32_t header, int32_t *info)
{
	state->align = 1;
	state->alignHeader = 1;
	if (header) {
		int32_t bitsPerFrame;

		writerStart(state);
		putBits(state, 0x2e6d7034, 32);
		putBits(state, 0, 8);
		putBits(state, 0x4003, 16);
		putBits(state, 1, 8);
		putBits(state, 8000, 32);
		putBits(state, state->bitRate, 32);
		bitsPerFrame = ((state->frameLengthCopy << 15) / 8000 * state->bitRate + 16384) >> 15;
		putBits(state, bitsPerFrame, 16);
		putBits(state, bitsPerFrame, 16);
		putBits(state, 0, 16);
		putBits(state, 0, 16);
		putBits(state, 2, 8);
		putBits(state, 11, 16);
		writeConfiguration(state);
		if (state->align)
			state->writePosition = (state->writePosition + 7) & -8;
		return state->writePosition;
	}
	if (state->align)
		writerStart(state);
	else
		writerContinue(state);
	info[0] = 0;
	info[1] = 0;
	info[2] = 0;
	info[3] = 0;
	info[4] = 0;
	shiftInput(state);
	if (state->frameCounter++ >= 1) {
		celpEncAnalyseLpc(state);
		celpEncLpcToLsp(state, state->angle);
		celpEncQuantizeLsp(state, state->angle, state->lsp, state->lspIndex);
		celpEncInterpolateLsp(state);
		celpEncSearchPitch(state);
		celpEncSearchExcitation(state);
		packFrame(state);
		info[4] = 0;
		info[0] = state->writePosition >> 3;
		if (!state->align)
			info[0] = state->writePosition >> 3;
		return state->writePosition;
	}
	return 0;
}

/* 0x854b82: the memory an encoder takes and its alignment */
int32_t celpEncoderQuery(uint32_t *size, uint32_t *alignment)
{
	if (size == NULL || alignment == NULL)
		return CELP_ERROR_POINTER;
	*size = 0x1c000;
	*alignment = 0x20000;
	return 0;
}

/* 0x854b9e: an encoder at the lowest bit rate */
int32_t celpEncoderInit(void *engine, uint32_t size)
{
	if (engine == NULL)
		return CELP_ERROR_POINTER;
	if (size < CELP_ENC_STATE_SIZE)
		return CELP_ERROR_SIZE;
	setupFrames(celpEncoderState(engine), 3850);
	return 0;
}

/* 0x854bd8 */
int32_t celpEncoderReset(void *engine)
{
	if (engine == NULL)
		return CELP_ERROR_POINTER;
	setupFrames(celpEncoderState(engine), 3850);
	return 0;
}

/* 0x854bfc: the bytes of a frame's samples and of its bits */
int32_t celpEncoderFrameSizes(void *engine, uint32_t *sizes)
{
	CelpEncoderState *state;

	if (engine == NULL || sizes == NULL)
		return CELP_ERROR_POINTER;
	state = celpEncoderState(engine);
	sizes[0] = state->frameLengthCopy * 2;
	sizes[1] = (state->frameBits + 7) >> 3;
	return 0;
}

/* 0x854b40: whether the encoder has this bit rate */
static int32_t bitRateValid(int32_t bitRate)
{
	int32_t i;

	for (i = 0; i < 10; i++)
		if (celpBitRates[i] == bitRate)
			return 1;
	return 0;
}

/* 0x854b5a: the bit rate of a mode index */
static int32_t modeBitRate(int32_t index, int32_t *bitRate)
{
	int32_t i;

	for (i = 0; i < 10; i++) {
		if (celpBitRateIndices[i] == index) {
			*bitRate = celpBitRates[i];
			return 1;
		}
	}
	return 0;
}

/* 0x854c26: the bit rate, by value (mode -1) or by mode index */
int32_t celpEncoderSetRate(void *engine, CelpEncoderRate *rate)
{
	CelpEncoderState *state;
	int32_t bitRate;

	if (engine == NULL || rate == NULL)
		return CELP_ERROR_POINTER;
	state = celpEncoderState(engine);
	if (rate->mode == -1) {
		if (!bitRateValid(rate->bitRate)) {
			state->invalid = 1;
			return CELP_ERROR_STATE;
		}
		bitRate = rate->bitRate;
	} else {
		if (!modeBitRate(rate->mode, (int32_t *)&rate->bitRate)) {
			state->invalid = 1;
			return CELP_ERROR_STATE;
		}
		bitRate = rate->bitRate;
	}
	setupFrames(state, bitRate);
	return 0;
}

/* 0x854c80: the stream header; `bits` the bits of it */
int32_t celpEncoderHeader(void *engine, uint8_t *header, uint32_t size, int32_t *bits)
{
	CelpEncoderState *state;
	int32_t written, bytes;

	if (engine == NULL || header == NULL || bits == NULL)
		return CELP_ERROR_POINTER;
	state = celpEncoderState(engine);
	if (state->invalid)
		return CELP_ERROR_STATE;
	written = encodeFrame(state, 1, NULL);
	if (written < 0)
		return CELP_ERROR_ENCODE;
	bytes = (written + 7) >> 3;
	if (size < (uint32_t)bytes)
		return CELP_ERROR_SIZE;
	memcpy(header, state->stream, bytes);
	*bits = written;
	return 0;
}

/* 0x854d10: a frame of samples; `esSize` the bytes written (none for the first frame) */
int32_t celpEncode(void *engine, const int16_t *pcm, uint32_t *pcmSize, uint8_t *es, uint32_t *esSize)
{
	CelpEncoderState *state;
	int32_t info[5], written, i;

	if (engine == NULL || pcm == NULL || es == NULL || esSize == NULL)
		return CELP_ERROR_POINTER;
	state = celpEncoderState(engine);
	if (state->invalid)
		return CELP_ERROR_STATE;
	for (i = 0; i < state->frameLengthCopy; i++)
		state->input[i] = pcm[i];
	written = encodeFrame(state, 0, info);
	if (written < 0)
		return CELP_ERROR_ENCODE;
	memcpy(es, state->stream, written >> 3);
	*pcmSize = state->frameLengthCopy * 2;
	*esSize = info[0];
	return 0;
}
