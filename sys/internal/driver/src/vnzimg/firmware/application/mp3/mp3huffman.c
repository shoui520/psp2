/*
 * The MP3 decoder's main data: the bit reservoir's reads (0x86a434-
 * 0x86a514), the CRC (0x874a9c-0x874ae4) and the Huffman decoding of a
 * granule's spectral values (0x874ca4-0x875360).
 */
#include "mp3.h"

#define RESERVOIR_MASK		(MP3_RESERVOIR_WORDS - 1)
#define BIG_VALUES_MAX		288
#define COUNT1_LAST		573	/* the last count a quadruple still fits after */
#define SHORT_REGION_BANDS	9

/* count bits (1-32) from the reservoir */
uint32_t mp3ReservoirRead(Mp3Reservoir *reservoir, uint32_t count)
{
	uint32_t value;
	int32_t next;

	if (!count)
		return 0;
	value = reservoir->words[reservoir->readIndex & RESERVOIR_MASK] << reservoir->bitOffset
		>> (32 - count);
	next = reservoir->bitOffset + count - 32;
	reservoir->bitCount += count;
	if (next < 0) {
		reservoir->bitOffset += count;
		return value;
	}
	reservoir->readIndex++;
	if (next >= 1)
		value |= reservoir->words[reservoir->readIndex & RESERVOIR_MASK] >> (32 - next);
	reservoir->bitOffset = next;
	return value;
}

void mp3ReservoirRewind(Mp3Reservoir *reservoir, int32_t count)
{
	int32_t offset;

	reservoir->bitCount -= count;
	reservoir->readIndex -= count >> 5;
	offset = reservoir->bitOffset - (count & 31);
	if (offset < 0) {
		reservoir->readIndex--;
		offset += 32;
	}
	reservoir->bitOffset = offset;
}

void mp3ReservoirSkip(Mp3Reservoir *reservoir, int32_t count)
{
	int32_t offset;

	reservoir->bitCount += count;
	reservoir->readIndex += count >> 5;
	offset = reservoir->bitOffset + (count & 31);
	if (offset >= 32) {
		offset -= 32;
		reservoir->readIndex++;
	}
	reservoir->bitOffset = offset;
}

/* the 32 bits from the read position, without reading them */
static uint32_t reservoirPeek(const Mp3Reservoir *reservoir)
{
	uint32_t word = reservoir->words[reservoir->readIndex & RESERVOIR_MASK];
	uint32_t next = reservoir->words[(reservoir->readIndex + 1) & RESERVOIR_MASK];

	if (!reservoir->bitOffset)
		return word;
	return word << reservoir->bitOffset | next >> (32 - reservoir->bitOffset);
}

void mp3CrcInit(Mp3Crc *crc)
{
	crc->value = 0xffff;
	crc->polynomial = 0x8005;
}

/* the count low bits of value, high bit first */
void mp3CrcUpdate(Mp3Crc *crc, uint32_t value, uint32_t count)
{
	uint32_t mask = 1 << (count - 1);
	uint32_t carry;

	do {
		carry = !(value & mask) ^ !(crc->value & 0x8000);
		crc->value <<= 1;
		if (carry)
			crc->value ^= crc->polynomial;
		mask >>= 1;
	} while (mask);
}

/* an entry's code against the window; a length of 0 compares all 32 bits */
static int codeMatches(uint32_t entry, uint32_t window)
{
	uint32_t shift = (32 - ((entry >> 8) & 31)) & 31;

	return entry >> shift == window >> shift;
}

/*
 * The entry for the code at the window's top: the first entry of the
 * code's top 4 bits, else a search of their range, else the neighbour the
 * search ended next to.
 */
static uint32_t huffmanLookup(const Mp3HuffmanTable *table, uint32_t window)
{
	uint32_t top = window >> 28, key = (window & ~0x1fff) | 0x1ff;
	const uint32_t *entry = &table->entries[table->index[top]];
	int32_t count = 0, step, backward = 0;

	if (codeMatches(*entry, window))
		return *entry;
	if (top != 15)
		count = table->index[top + 1] - table->index[top];
	while (count >= 2) {
		step = count >> 1;
		if (!backward) {
			entry += step;
			if (key < *entry) {
				count >>= 1;
				backward = 1;
			} else {
				count -= step;
			}
		} else {
			entry -= step;
			if (key < *entry) {
				count -= step;
			} else {
				backward = 0;
				count >>= 1;
			}
		}
	}
	if (codeMatches(*entry, window))
		return *entry;
	return backward ? entry[-1] : entry[1];
}

/* a code from the reservoir; its value is in the low 8 bits */
static uint32_t huffmanRead(Mp3Reservoir *reservoir, const Mp3HuffmanTable *table)
{
	uint32_t entry = huffmanLookup(table, reservoirPeek(reservoir));

	mp3ReservoirSkip(reservoir, (entry >> 8) & 31);
	return entry;
}

static int32_t signRead(Mp3Reservoir *reservoir, int32_t value)
{
	if (value && mp3ReservoirRead(reservoir, 1))
		return -value;
	return value;
}

/* count big values, two a code */
static void huffmanPairs(int32_t *out, Mp3Reservoir *reservoir, const Mp3HuffmanTable *table,
	int32_t count)
{
	int32_t i, x, y;
	uint32_t entry;

	for (i = 0; i < count; i += 2) {
		entry = huffmanRead(reservoir, table);
		x = (entry >> 4) & 15;
		y = entry & 15;
		if (x == 15 && table->linbits >= 1)
			x += mp3ReservoirRead(reservoir, table->linbits);
		x = signRead(reservoir, x);
		if (y == 15 && table->linbits >= 1)
			y += mp3ReservoirRead(reservoir, table->linbits);
		y = signRead(reservoir, y);
		out[0] = x;
		out[1] = y;
		out += 2;
	}
}

/* count1 quadruples until the granule's part 2 and 3 end */
static void huffmanQuads(int32_t *out, Mp3Reservoir *reservoir, const Mp3HuffmanTable *table,
	uint32_t *count, int32_t end)
{
	uint32_t value;

	while (reservoir->bitCount < end && *count < COUNT1_LAST) {
		value = (huffmanRead(reservoir, table) >> 4) & 15;
		out[0] = signRead(reservoir, (value >> 3) & 1);
		out[1] = signRead(reservoir, (value >> 2) & 1);
		out[2] = signRead(reservoir, (value >> 1) & 1);
		out[3] = signRead(reservoir, value & 1);
		out += 4;
		*count += 4;
	}
}

static int tableInvalid(uint8_t table)
{
	return table == 4 || table == 14;
}

/* a granule's spectral values for a channel, after its part2 bits of scale factors */
int32_t mp3Huffman(Mp3Decoder *decoder, uint32_t ch, uint32_t gr, int32_t part2,
	const uint8_t *shortWidths, const uint16_t *longBounds)
{
	const Mp3Granule *granule = &decoder->side.channel[ch].granule[gr];
	int32_t *xr = decoder->synthesisState.xr[ch];
	int32_t end = decoder->granuleStart + granule->part23Length;
	Mp3Reservoir *reservoir = &decoder->reservoir;
	int32_t count, region1, region2, i;
	uint32_t nonzero = 0;

	if (granule->part23Length - part2 < 0)
		return MP3_STATUS_PART2_3;
	if (granule->bigValues > BIG_VALUES_MAX)
		return MP3_STATUS_BIG_VALUES;
	count = granule->bigValues * 2;
	if (count) {
		if (tableInvalid(granule->tableSelect[0]))
			return MP3_STATUS_TABLE;
		if (granule->windowSwitching && granule->blockType == MP3_BLOCK_SHORT) {
			region1 = 0;
			for (i = 0; i < SHORT_REGION_BANDS; i++)
				region1 += shortWidths[i];
			region2 = 0;
			if (region1 < count) {
				if (tableInvalid(granule->tableSelect[1]))
					return MP3_STATUS_TABLE;
			} else {
				region1 = count;
			}
		} else {
			region1 = longBounds[granule->region0Count + 1];
			region2 = longBounds[granule->region0Count + granule->region1Count + 2];
			if (region1 < count) {
				if (region2 < count) {
					if (tableInvalid(granule->tableSelect[2]))
						return MP3_STATUS_TABLE;
				} else {
					if (tableInvalid(granule->tableSelect[1]))
						return MP3_STATUS_TABLE;
					region2 = count;
				}
			} else {
				region2 = count;
				region1 = count;
			}
		}
		huffmanPairs(xr, reservoir, &mp3HuffmanTables[granule->tableSelect[0]], region1);
		if (granule->windowSwitching && granule->blockType == MP3_BLOCK_SHORT) {
			huffmanPairs(xr + region1, reservoir,
				&mp3HuffmanTables[granule->tableSelect[1]], count - region1);
		} else {
			huffmanPairs(xr + region1, reservoir,
				&mp3HuffmanTables[granule->tableSelect[1]], region2 - region1);
			huffmanPairs(xr + region2, reservoir,
				&mp3HuffmanTables[granule->tableSelect[2]], count - region2);
		}
		nonzero = count;
	}

	huffmanQuads(xr + nonzero, reservoir,
		&mp3HuffmanTables[MP3_COUNT1_TABLE + granule->count1TableSelect], &nonzero, end);
	/* the last quadruple read past the end is dropped */
	if ((uint32_t)end < (uint32_t)reservoir->bitCount) {
		mp3ReservoirRewind(reservoir, reservoir->bitCount - end);
		nonzero -= 4;
	} else if ((uint32_t)reservoir->bitCount < (uint32_t)end) {
		mp3ReservoirSkip(reservoir, end - reservoir->bitCount);
	}
	decoder->nonzero[ch] = nonzero;
	for (i = MP3_GRANULE_SAMPLES - 1; i >= (int32_t)nonzero; i--)
		xr[i] = 0;
	return 0;
}
