/*
 * The MPEG-4 decoder's bit reader (0x867060-0x867564): bitstream.c's, for
 * MPEG-4 part 2. The unit removes no emulation bytes (control 0), the
 * reader saves all of the unit's registers, peeks past a byte boundary and
 * finds start codes; and the unit is loaded with a video packet's data
 * from the bit the parser stopped at.
 */
#include "service.h"

#define BITS(reg)			(VIDEO_BITS_READ + (reg))
#define SET_BITS(reg)			(VIDEO_BITS_WRITE + (reg))

#define STATUS_END			5	/* nothing more to push */

#define UNIT_LOAD			6
#define FLAG_UNIT			0x40000

uint32_t m4vdecBitsRead(VideoBitstream *bits, uint32_t count);

/* the unit set on a unit of the stream: after its first byte, the first word read */
uint32_t m4vdecBitsBegin(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoBitstream *bits = job->bits;
	VideoM4vUnit *unit = job->unit;

	bits->ringStart = stream->ringStart;
	bits->ringEnd = stream->ringEnd;
	if (unit->start == stream->ringEnd)
		bits->read = stream->ringStart;
	else
		bits->read = unit->start + 1;
	bits->remaining = unit->size - 1;
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_CONTROL), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_WORD), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE2), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE3), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_POSITION), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATUS), 1);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE6), 0);
	return m4vdecBitsRead(bits, 32);
}

void m4vdecBitsSave(VideoBitsRegisters *registers)
{
	registers->control = VIDEO_DSP_READ(BITS(VIDEO_BITS_CONTROL));
	registers->word = VIDEO_DSP_READ(BITS(VIDEO_BITS_WORD));
	registers->state2 = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATE2));
	registers->state3 = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATE3));
	registers->position = VIDEO_DSP_READ(BITS(VIDEO_BITS_POSITION));
	registers->status = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATUS));
	registers->state6 = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATE6));
}

void m4vdecBitsRestore(const VideoBitsRegisters *registers)
{
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_CONTROL), registers->control);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_WORD), registers->word);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE2), registers->state2);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE3), registers->state3);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_POSITION), registers->position);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATUS), registers->status);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE6), registers->state6);
}

/*
 * The unit's reader moved to where the unit is in the stream unit: returns
 * the bits read past its first byte.
 */
int32_t m4vdecBitsTell(const VideoM4vStream *stream, VideoM4vUnit *unit)
{
	int32_t bits = VIDEO_DSP_READ(BITS(VIDEO_BITS_POSITION)) - 32;
	int32_t bytes = bits >> 3;
	uint32_t read;

	if (unit->start == stream->ringEnd)
		unit->bits.read = stream->ringStart;
	else
		unit->bits.read = unit->start + 1;
	read = unit->bits.read + bytes;
	unit->bits.read = read;
	unit->bits.remaining = unit->size - 1 - bytes;
	if (stream->ringEnd < read)
		unit->bits.read = unit->bits.ringStart + (read - unit->bits.ringEnd) - 1;
	return bits;
}

/*
 * The stream unit given to the bitstream unit, from the parser's bit on:
 * one descriptor, or two when it wraps around the ring. The unit loads,
 * then skips the bits before it inside the first byte.
 */
void m4vdecUnitLoad(const VideoHelperShared *shared, const VideoM4vStream *stream,
	const VideoM4vUnit *unit)
{
	VideoUnitDescriptor descriptors[2];
	VideoUnitDescriptor *list = (VideoUnitDescriptor *)shared->descriptors;
	int32_t bits = unit->dataBits;
	int32_t bytes = bits >> 3;
	uint32_t start = unit->start + 1 + bytes;
	uint32_t bitOffset, command;
	int32_t size, first;

	if (stream->ringEnd < start)
		start = start - stream->ringEnd + stream->ringStart - 1;
	bitOffset = bits & 7;
	size = unit->size - 1 - bytes;
	if (start + size <= stream->ringEnd) {
		descriptors[0].next = 0;
		descriptors[0].flags = VIDEO_UNIT_DESCRIPTOR_LAST;
		descriptors[0].address = shared->publicStream ? vnzToPublic(start) :
			vnzToUncached(start);
		descriptors[0].size = size;
		list[0] = descriptors[0];
	} else {
		descriptors[0].flags = 0;
		descriptors[0].next = (uint32_t)&list[1];
		descriptors[0].address = shared->publicStream ? vnzToPublic(start) :
			vnzToUncached(start);
		first = stream->ringEnd - start + 1;
		descriptors[0].size = first;
		descriptors[1].next = 0;
		descriptors[1].flags = VIDEO_UNIT_DESCRIPTOR_LAST;
		descriptors[1].address = shared->publicStream ? vnzToPublic(stream->ringStart) :
			vnzToUncached(stream->ringStart);
		descriptors[1].size = size - first;
		list[0] = descriptors[0];
		list[1] = descriptors[1];
	}
	VIDEO_UNIT[VIDEO_UNIT_DESCRIPTORS] = (uint32_t)list;
	VIDEO_UNIT[VIDEO_UNIT_COMMAND] = UNIT_LOAD;
	vnzThreadWaitFlags(FLAG_UNIT);
	if (bitOffset) {
		command = ((bitOffset - 1) & 0xff) << 8;
		VIDEO_UNIT[VIDEO_UNIT_COMMAND] = command;
		vnzThreadWaitFlags(FLAG_UNIT);
	}
	VIDEO_UNIT[VIDEO_UNIT_BITS] = 0;
}

/* the reader set after the bits the unit read (VIDEO_UNIT_BITS) */
void m4vdecBitsSkipUnit(const VideoHelperShared *shared, VideoBitstream *bits)
{
	int32_t position = VIDEO_DSP_READ(BITS(VIDEO_BITS_POSITION)) - 32;
	uint32_t unitBits = VIDEO_UNIT[VIDEO_UNIT_BITS];
	uint32_t bytes = ((position & 7) + unitBits) >> 3;

	bits->read += bytes;
	bits->remaining -= bytes;
	if (bits->ringEnd < bits->read)
		bits->read = bits->ringStart + (bits->read - bits->ringEnd) - 1;
	position += unitBits;
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_WORD), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE2), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE3), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_POSITION), position & ~7);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATUS), 1);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE6), 0);
	m4vdecBitsRead(bits, 32);
	if (position & 7)
		m4vdecBitsRead(bits, position & 7);
}

/* the bits the unit wants pushed from the ring; zeros past the end */
void m4vdecBitsRefill(VideoBitstream *bits, uint32_t wanted)
{
	int32_t remaining = bits->remaining;
	uint32_t read = bits->read, byte;

	if ((int32_t)wanted < 1)
		return;
	do {
		if (remaining < 1) {
			byte = 0;
			VIDEO_DSP(VIDEO_BITS_PUSH, wanted, byte);
			VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATUS), STATUS_END);
		} else {
			remaining--;
			byte = *(const uint8_t *)read;
			VIDEO_DSP(VIDEO_BITS_PUSH, wanted, byte);
			if (read != bits->ringEnd)
				read++;
			else if (bits->ringStart)
				read = bits->ringStart;
			bits->remaining = remaining;
			bits->read = read;
		}
	} while ((int32_t)wanted >= 1);
}

uint32_t m4vdecBitsRead(VideoBitstream *bits, uint32_t count)
{
	uint32_t value;

	value = VIDEO_DSP_GET(VIDEO_BITS_PEEK, count);
	VIDEO_DSP(VIDEO_BITS_SKIP, count, value);
	if ((int32_t)count >= 1)
		m4vdecBitsRefill(bits, count);
	return value;
}

uint32_t m4vdecBitsPeek(VideoBitstream *bits, uint32_t count)
{
	return VIDEO_DSP_GET(VIDEO_BITS_PEEK, count);
}

/*
 * The count bits after the next byte boundary (after 1-8 bits of
 * stuffing), the reader left where it was.
 */
uint32_t m4vdecBitsPeekAligned(VideoBitstream *bits, uint32_t count)
{
	uint32_t word = VIDEO_DSP_READ(BITS(VIDEO_BITS_WORD));
	uint32_t state2 = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATE2));
	uint32_t state3 = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATE3));
	uint32_t position = VIDEO_DSP_READ(BITS(VIDEO_BITS_POSITION));
	uint32_t state6 = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATE6));
	uint32_t status = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATUS));
	uint32_t read = bits->read;
	int32_t remaining = bits->remaining;
	uint32_t value;

	m4vdecBitsRead(bits, 8 - (position & 7));
	bits->remaining = remaining;
	bits->read = read;
	value = VIDEO_DSP_GET(VIDEO_BITS_PEEK, count);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_WORD), word);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE2), state2);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE3), state3);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_POSITION), position);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATUS), status);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE6), state6);
	return value;
}

/* the bits read past the last byte boundary */
uint32_t m4vdecBitsOffset(const VideoBitstream *bits)
{
	return VIDEO_DSP_READ(BITS(VIDEO_BITS_POSITION)) & 7;
}

/* the stuffing to the next byte boundary is a zero and ones */
uint32_t m4vdecBitsValidStuffing(VideoBitstream *bits)
{
	uint32_t offset = VIDEO_DSP_READ(BITS(VIDEO_BITS_POSITION)) & 7;
	uint32_t count = 8 - offset;

	return VIDEO_DSP_GET(VIDEO_BITS_PEEK, count) == (0x7fu >> offset);
}

/* the stuffing read: returns its bits */
uint32_t m4vdecBitsByteAlign(VideoBitstream *bits)
{
	uint32_t count = 8 - (VIDEO_DSP_READ(BITS(VIDEO_BITS_POSITION)) & 7);

	m4vdecBitsRead(bits, count);
	return count;
}

/*
 * Aligned, then on to the next start code prefix (0x000001): bytes pushed
 * one at a time until the unit sees it. Returns 0 at the stream unit's end.
 */
uint32_t m4vdecBitsNextStartCode(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoBitstream *bits = job->bits;
	uint32_t ringEnd = stream->ringEnd;
	uint32_t ringStart = stream->ringStart;
	uint32_t read, wanted, byte;
	int32_t remaining;

	m4vdecBitsByteAlign(bits);
	read = bits->read;
	remaining = bits->remaining;
	if (VIDEO_DSP_GET(VIDEO_BITS_PEEK, 24) == 1)
		return 1;
	while (remaining >= 1) {
		if ((VIDEO_DSP_GET(VIDEO_BITS_PEEK, 32) & 0xffff80) == 0x80) {
			bits->remaining = remaining;
			bits->read = read;
			return 1;
		}
		wanted = 8;
		byte = *(const uint8_t *)read;
		for (;;) {
			VIDEO_DSP(VIDEO_BITS_PUSH, wanted, byte);
			if (read == ringEnd)
				read = ringStart;
			else
				read++;
			remaining--;
			if ((int32_t)wanted < 1)
				break;
			byte = *(const uint8_t *)read;
		}
	}
	return 0;
}
