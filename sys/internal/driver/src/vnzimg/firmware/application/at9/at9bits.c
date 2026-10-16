/*
 * The bit reader (0x889848-0x889c80): most significant bit first, over a
 * window of 1024 bytes that ends where the superframe's data ends. Reads
 * past the window give 0 and still move the position.
 */
#include "at9.h"

/* the byte at the position's byte and the next ones, as one number */
static inline uint32_t window16(const At9Reader *reader)
{
	const uint8_t *p = reader->data + (reader->bit >> 3);

	return p[0] << 8 | p[1];
}

static inline uint32_t window24(const At9Reader *reader)
{
	const uint8_t *p = reader->data + (reader->bit >> 3);

	return p[0] << 16 | p[1] << 8 | p[2];
}

/* 0x889890: start at the data given, the superframe's bytes still unread */
void at9ReaderStart(At9DecoderState *decoder, const uint8_t *data)
{
	int32_t left = decoder->superframeBytes - decoder->consumed;

	decoder->reader.data = data - AT9_WINDOW_BYTES + left;
	decoder->reader.bit = (AT9_WINDOW_BYTES - left) * 8;
}

/* 0x889988: up to 9 bits */
uint32_t at9Read(At9Reader *reader, uint32_t count)
{
	uint32_t value = 0;

	if (reader->bit < AT9_WINDOW_BITS) {
		uint32_t shift = 16 - count - (reader->bit & 7);

		value = (window16(reader) >> (shift & 31)) & (0xffffU >> (16 - count));
	}
	reader->bit += count;
	return value;
}

/* 0x8898e0: up to 17 bits */
uint32_t at9ReadWide(At9Reader *reader, uint32_t count)
{
	uint32_t value = 0;

	if (reader->bit < AT9_WINDOW_BITS) {
		uint32_t shift = 24 - count - (reader->bit & 7);

		value = (window24(reader) >> (shift & 31)) & (0xffffffU >> (24 - count));
	}
	reader->bit += count;
	return value;
}

/* 0x889a10 */
uint32_t at9ReadBit(At9Reader *reader)
{
	uint32_t value = 0;

	if (reader->bit < AT9_WINDOW_BITS)
		value = (reader->data[reader->bit >> 3] >> (7 - (reader->bit & 7))) & 1;
	reader->bit++;
	return value;
}

/* 0x889b20: the next bits (up to 9) without taking them */
uint32_t at9Peek(const At9Reader *reader, uint32_t count)
{
	uint32_t value = 0;

	if (reader->bit < AT9_WINDOW_BITS) {
		uint32_t shift = 16 - count - (reader->bit & 7);

		value = (window16(reader) >> (shift & 31)) & (0xffffU >> (16 - count));
	}
	return value;
}

/* 0x889a88: up to 17 bits */
uint32_t at9PeekWide(const At9Reader *reader, uint32_t count)
{
	uint32_t value = 0;

	if (reader->bit < AT9_WINDOW_BITS) {
		uint32_t shift = 24 - count - (reader->bit & 7);

		value = (window24(reader) >> (shift & 31)) & (0xffffffU >> (24 - count));
	}
	return value;
}

/* 0x889848: on to the next byte */
void at9Align(At9Reader *reader)
{
	uint32_t bit = reader->bit;
	int32_t skip = ((bit + 7) >> 3) * 8 - bit;

	if (skip >= 1)
		at9Read(reader, skip);
}

/*
 * 0x889b98: the frame is read. The last frame of a superframe must be
 * followed by 0x01 bytes up to the window's end.
 */
int32_t at9ReaderEnd(At9DecoderState *decoder)
{
	uint32_t i;

	if (decoder->reader.bit > AT9_WINDOW_BITS)
		return AT9_ERROR_FRAME_END;
	if (decoder->frameIndex + 1 != decoder->framesPerSuperframe)
		return 0;
	for (i = decoder->reader.bit >> 3; i < AT9_WINDOW_BYTES; i++)
		if (decoder->reader.data[i] != 1)
			return AT9_ERROR_PADDING;
	decoder->reader.bit = AT9_WINDOW_BITS;
	return 0;
}

/* 0x889c80: the bytes the frame took */
int32_t at9ReaderBytes(const At9DecoderState *decoder)
{
	return (int32_t)(decoder->reader.bit >> 3) - AT9_WINDOW_BYTES + decoder->superframeBytes -
		decoder->consumed;
}
