/*
 * The SBR decoders' slots (0x86b868) and the reader the SBR and PS
 * payloads are parsed with (0x86b8b4-0x86baca).
 */
#include "aac.h"

void aacSbrInit(uint32_t channels, AacDecoder *decoder)
{
	int32_t i;

	decoder->sbr = decoder->sbrSlots;
	for (i = 0; i < (int32_t)channels; i++)
		decoder->sbr[i] = NULL;
	decoder->sbrChannels = decoder->sbrChannelSlots;
	for (i = 0; i < (int32_t)channels; i++)
		decoder->sbrChannels[i] = &decoder->sbrChannelData[i];
}

/* moves a pointer around the circular buffer */
static void aacSbrBitsAdvance(AacSbrBits *bits, const uint8_t **pointer, int32_t count)
{
	int32_t i;

	if (count < 0) {
		for (i = 0; i < -count; i++) {
			if (*pointer == bits->start)
				*pointer = bits->last;
			else
				(*pointer)--;
		}
	} else {
		for (i = 0; i < count; i++) {
			if (*pointer == bits->last)
				*pointer = bits->start;
			else
				(*pointer)++;
		}
	}
}

void aacSbrBitsInit(AacSbrBits *bits, const uint8_t *buffer, int32_t size)
{
	bits->remaining = size;
	bits->buffer = buffer;
	bits->cursor = buffer;
	bits->start = buffer;
	bits->cache = 0;
	bits->reserved = 0;
	bits->cacheBits = 0;
	bits->overrun = 0;
	bits->consumed = 0;
	bits->cacheSize = 16;
	bits->last = buffer + ((uint32_t)(size + 7) >> 3) - 1;
}

void aacSbrBitsCopy(const AacSbrBits *from, AacSbrBits *to)
{
	*to = *from;
}

uint32_t aacSbrReadBits(AacSbrBits *bits, uint8_t count)
{
	uint32_t value = 0;
	int32_t cached;

	if (bits->overrun) {
		cached = (uint8_t)bits->cacheBits;
		aacSbrSkipBits(bits, -cached);
		aacSbrSkipBits(bits, cached);
	}
	while (count >= 9) {
		count -= 8;
		value += aacSbrReadBits(bits, 8) << count;
	}
	if ((uint8_t)bits->cacheBits < 9) {
		bits->cache = bits->cache << 8 | *bits->cursor;
		aacSbrBitsAdvance(bits, &bits->cursor, 1);
		bits->cacheBits += 8;
	}
	bits->cacheBits -= count;
	bits->remaining -= count;
	value += (bits->cache >> (uint8_t)bits->cacheBits) & ((1 << count) - 1);
	bits->consumed += count;
	bits->overrun = bits->remaining < (uint8_t)bits->cacheBits;
	return value;
}

/* moves the read position by count bits and reloads the cache from there */
void aacSbrSkipBits(AacSbrBits *bits, int32_t count)
{
	int32_t offset;

	if (count == 0)
		return;
	if (count > 0) {
		bits->remaining -= count;
		bits->consumed += count;
		offset = count - (uint8_t)bits->cacheBits + 16;
		aacSbrBitsAdvance(bits, &bits->cursor, offset / 8 - 2);
		bits->cache = *bits->cursor;
		bits->cacheBits = 8 - offset % 8;
	} else {
		count = -count;
		bits->remaining += count;
		bits->consumed -= count;
		offset = (uint8_t)bits->cacheBits + count + 7;
		aacSbrBitsAdvance(bits, &bits->cursor, -(offset / 8));
		bits->cache = *bits->cursor;
		bits->cacheBits = offset % 8 + 1;
	}
	aacSbrBitsAdvance(bits, &bits->cursor, 1);
}

int32_t aacSbrClamp(int32_t value, int32_t low, int32_t high)
{
	if (value < low)
		return low;
	if (high < value)
		return high;
	return value;
}

/* halves towards zero */
int32_t aacSbrHalve(int32_t value)
{
	int32_t negative = value < 0;

	if (negative)
		value = -value;
	value >>= 1;
	return negative ? -value : value;
}
