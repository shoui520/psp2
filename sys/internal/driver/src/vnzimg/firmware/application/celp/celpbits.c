/*
 * The CELP decoder's bit reader (0x8c43f8-0x8c4670): big-endian, most
 * significant bit first, over the frame's bytes.
 */
#include "celp.h"

/* 0x8c43f8 */
void celpBitsReset(CelpDecoderState *state)
{
	state->bitBase = state->stream;
	state->bitPosition = 0;
}

static uint32_t loadWord(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
}

/* 0x8c4420: up to 25 bits */
uint32_t celpBitsGet(CelpDecoderState *state, uint32_t count)
{
	const uint8_t *bytes = state->bitBase + (state->bitPosition >> 3);
	uint32_t value = loadWord(bytes);

	value <<= state->bitPosition & 7;
	value >>= 32 - count;
	state->bitPosition += count;
	return value;
}

/* 0x8c44e8: up to 32 bits, reading a fifth byte when the word does not hold them */
uint32_t celpBitsGetWide(CelpDecoderState *state, uint32_t count)
{
	uint32_t offset = state->bitPosition & 7;
	const uint8_t *bytes = state->bitBase + (state->bitPosition >> 3);
	int32_t remaining = 32 - offset - count;
	uint32_t value = loadWord(bytes);

	value <<= offset;
	value >>= 32 - count;
	if (remaining < 0)
		value |= bytes[4] >> (remaining + 8);
	state->bitPosition += count;
	return value;
}

/* 0x8c4638: skips to the next byte when the frame is to end on one */
void celpBitsAlign(CelpDecoderState *state)
{
	if (state->align)
		state->bitPosition = (state->bitPosition + 7) & -8;
}
