/*
 * Reading the codecs' streams through the bitstream unit (0x86187c-0x861fe4):
 * bytes are pushed into it from a ring, bits are peeked and consumed, and it
 * tells how many bits it wants pushed again. Exp-Golomb codes, the search
 * for the next NAL unit (the unit spots the start codes), and the NAL
 * header and slice type of H.264.
 */
#include "service.h"

#define BITS(reg)			(VIDEO_BITS_READ + (reg))
#define SET_BITS(reg)			(VIDEO_BITS_WRITE + (reg))

#define STATUS_END			5	/* nothing more to push */
#define START_FOUND			0	/* the status after a start code */
#define START_ZEROS			3	/* the status after zero bytes */

#define ERROR_NAL_HEADER		8193

/* the unit set on a NAL unit: after its header byte, the first word read */
uint32_t videoBitsBegin(const VideoStream *stream, VideoParser *parser)
{
	VideoBitstream *bits = parser->bits;
	VideoNal *nal = parser->nal;

	bits->ringStart = stream->ringStart;
	bits->ringEnd = stream->ringEnd;
	if (nal->start == stream->ringEnd)
		bits->read = stream->ringStart;
	else
		bits->read = nal->start + 1;
	bits->remaining = nal->size - 1;
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_CONTROL), 1);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_WORD), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE2), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE3), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_POSITION), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATUS), 1);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE6), 0);
	return videoBitsRead(bits, 32);
}

void videoBitsSave(VideoBitsState *state)
{
	state->control = VIDEO_DSP_READ(BITS(VIDEO_BITS_CONTROL));
	state->word = VIDEO_DSP_READ(BITS(VIDEO_BITS_WORD));
	state->state2 = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATE2));
	state->state3 = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATE3));
	state->position = VIDEO_DSP_READ(BITS(VIDEO_BITS_POSITION));
	state->status = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATUS));
	state->state6 = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATE6));
}

void videoBitsRestore(const VideoBitsState *state)
{
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_CONTROL), state->control);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_WORD), state->word);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE2), state->state2);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE3), state->state3);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_POSITION), state->position);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATUS), state->status);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE6), state->state6);
}

/*
 * The unit reloaded at its present bit position in the NAL unit: the bytes
 * from there pushed again, and up to three more until the unit's word is
 * the one it had (escaped bytes make the positions differ). Returns the bits
 * read past the unit's first byte.
 */
int32_t videoBitsSeek(const VideoStream *stream, VideoNal *nal)
{
	uint32_t word = VIDEO_DSP_READ(BITS(VIDEO_BITS_WORD));
	int32_t bits = VIDEO_DSP_READ(BITS(VIDEO_BITS_POSITION)) - 32;
	int32_t bytes = bits >> 3, tries;
	uint32_t read = nal->start + 1 + bytes;

	if (stream->ringEnd < read)
		read = read - stream->ringEnd + stream->ringStart - 1;
	nal->bits.read = read;
	nal->bits.remaining = nal->size - 1 - bytes;
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_WORD), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE2), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE3), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_POSITION), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATUS), 1);
	videoBitsRead(&nal->bits, 32);
	if (bits & 7)
		videoBitsRead(&nal->bits, bits & 7);
	for (tries = 3; VIDEO_DSP_READ(BITS(VIDEO_BITS_WORD)) != word && tries >= 1; tries--) {
		if (!nal->escaped)
			nal->bits.remaining++;
		videoBitsRead(&nal->bits, 8);
		bits += 8;
	}
	return bits - 8;
}

/* more_rbsp_data(): bytes left, or more than the stop bit in the word */
uint32_t videoBitsMoreData(const VideoBitstream *bits)
{
	uint32_t word = VIDEO_DSP_GET(VIDEO_BITS_PEEK, 32);

	if (bits->remaining >= 1)
		return 1;
	if (bits->remaining)
		return 0;
	if (word == 0x80000000 || !word)
		return 0;
	return 1;
}

uint32_t videoBitsHasBytes(const VideoBitstream *bits)
{
	return bits->remaining >= 1;
}

uint32_t videoBitsAligned(const VideoBitstream *bits)
{
	int32_t position = VIDEO_DSP_READ(BITS(VIDEO_BITS_POSITION));

	return position % 8 == 0;
}

/* the bits the unit wants pushed from the ring; zeros past the end */
void videoBitsRefill(VideoBitstream *bits, uint32_t wanted)
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

uint32_t videoBitsReadByte(VideoBitstream *bits)
{
	uint32_t count = 8, value;

	value = VIDEO_DSP_GET(VIDEO_BITS_PEEK, count);
	VIDEO_DSP(VIDEO_BITS_SKIP, count, value);
	if (count)
		videoBitsRefill(bits, count);
	return value;
}

uint32_t videoBitsRead(VideoBitstream *bits, uint32_t count)
{
	uint32_t value;

	value = VIDEO_DSP_GET(VIDEO_BITS_PEEK, count);
	VIDEO_DSP(VIDEO_BITS_SKIP, count, value);
	if (count)
		videoBitsRefill(bits, count);
	return value;
}

/*
 * Sony's code sign-extends by the count the unit returns after the skip,
 * not by the count read; kept as it is.
 */
int32_t videoBitsReadSigned(VideoBitstream *bits, uint32_t count)
{
	uint32_t shift = 32;
	int32_t value;

	value = VIDEO_DSP_GET(VIDEO_BITS_PEEK, count);
	VIDEO_DSP(VIDEO_BITS_SKIP, count, value);
	if (count != shift) {
		shift -= count;
		value = (int32_t)((uint32_t)value << shift) >> shift;
	}
	if (count)
		videoBitsRefill(bits, count);
	return value;
}

uint32_t videoBitsPeek(VideoBitstream *bits, uint32_t count)
{
	return VIDEO_DSP_GET(VIDEO_BITS_PEEK, count);
}

/* ue(v) */
uint32_t videoBitsUe(VideoBitstream *bits)
{
	uint32_t word = VIDEO_DSP_GET(VIDEO_BITS_PEEK, 32);
	uint32_t zeros, count, base, value;

	__asm__("ldz %0,%1" : "=r"(zeros) : "r"(word));
	if (!zeros) {
		value = 0;
		count = 1;
		VIDEO_DSP(VIDEO_BITS_SKIP, count, value);
		if (count)
			videoBitsRefill(bits, count);
		return 0;
	}
	base = (1 << zeros) - 1;
	if ((int32_t)zeros >= 15) {
		/* longer than the word: the prefix, then the suffix */
		count = zeros + 1;
		VIDEO_DSP(VIDEO_BITS_SKIP, count, word);
		if ((int32_t)count >= 1)
			videoBitsRefill(bits, count);
		count = zeros;
		value = VIDEO_DSP_GET(VIDEO_BITS_PEEK, count) + base;
		VIDEO_DSP(VIDEO_BITS_SKIP, count, value);
		if (count)
			videoBitsRefill(bits, count);
		return value;
	}
	count = zeros + 1;
	word = (word << count) >> count;
	count += zeros;
	value = base + (word >> (32 - count));
	VIDEO_DSP(VIDEO_BITS_SKIP, count, value);
	if (count)
		videoBitsRefill(bits, count);
	return value;
}

/* te(v) */
uint32_t videoBitsTe(VideoBitstream *bits, int32_t max)
{
	if (max >= 2)
		return videoBitsUe(bits);
	return !videoBitsRead(bits, 1);
}

/* se(v) */
int32_t videoBitsSe(VideoBitstream *bits)
{
	uint32_t code = videoBitsUe(bits);
	int32_t value = (code + 1) >> 1;

	if (!(code & 1))
		value = -value;
	return value;
}

/* the address after p in the ring */
static uint32_t ringNext(const VideoStream *stream, uint32_t p)
{
	return p == stream->ringEnd ? stream->ringStart : p + 1;
}

/*
 * The next NAL unit: its start code found by the unit; with whole, its end
 * too (the next start code, or the end of the data less its trailing zeros).
 * Returns 1 when found, its start and size in the NAL record.
 */
uint32_t videoNalFind(const VideoStream *stream, VideoParser *parser, uint32_t whole)
{
	VideoNal *nal = parser->nal;
	uint32_t ringEnd = stream->ringEnd, ringStart = stream->ringStart;
	uint32_t base = stream->base, read, start = 0, byte, bits, next;
	int32_t left;
	uint32_t status = 0, last;

	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_CONTROL), 3);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_WORD), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE2), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE3), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_POSITION), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATE6), 0);
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATUS), 1);
	read = nal->search;
	left = nal->searchLeft;

	/* the start code */
	while (left >= 1) {
		byte = *(const uint8_t *)read;
		bits = 8;
		VIDEO_DSP(VIDEO_BITS_PUSH, bits, byte);
		next = ringStart ? ringNext(stream, read) : read + 1;
		read = next;
		left--;
		status = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATUS)) & 7;
		if (status == START_FOUND) {
			start = next;
			if (!whole) {
				nal->start = next;
				nal->size = left;
				return 1;
			}
			break;
		}
	}

	/* its end: the next start code, or zero bytes before it */
	VIDEO_DSP_WRITE(SET_BITS(VIDEO_BITS_STATUS), 1);
	while (left >= 1) {
		last = status;
		byte = *(const uint8_t *)read;
		bits = 8;
		VIDEO_DSP(VIDEO_BITS_PUSH, bits, byte);
		next = ringStart ? ringNext(stream, read) : read + 1;
		read = next;
		left--;
		status = VIDEO_DSP_READ(BITS(VIDEO_BITS_STATUS)) & 7;
		if (last == START_ZEROS && (status == START_FOUND || status == START_ZEROS)) {
			left += 4;
			if (ringStart && (next < base || ringEnd < next) && next < ringStart + 4)
				read = ringEnd + (next - ringStart) - 3;
			else
				read -= 4;
			break;
		}
	}

	if (!start)
		return 0;
	if (!left) {
		/* to the end of the data: its trailing zeros are not the unit's */
		do {
			if (read == ringStart)
				read = ringEnd;
			else
				read--;
		} while (!*(const int8_t *)read);
	}
	if ((read < base || ringEnd < read) && start >= base && start <= ringEnd)
		nal->size = read - ringStart + (ringEnd - start) + 2;
	else
		nal->size = read - start + 1;
	nal->search = read;
	nal->start = start;
	nal->searchLeft = left;
	return 1;
}

/* the slice's first_mb_in_slice and slice_type: the pictures it needs (1 I, 3 P, 7 B) */
uint32_t videoNalSliceTypes(const VideoStream *stream, VideoParser *parser)
{
	VideoNal *nal = parser->nal;
	VideoBitstream *bits = parser->bits;
	uint32_t type;

	if (nal->type != NAL_SLICE && nal->type != NAL_IDR)
		return 0;
	videoBitsBegin(stream, parser);
	videoBitsUe(bits);
	type = videoBitsUe(bits);
	switch (type) {
	case 0:
	case 5:
		return 3;
	case 1:
	case 6:
		return 7;
	case 2:
	case 7:
		return 1;
	default:
		return 0;
	}
}

/* the next NAL unit's header: its type, or -1 */
int32_t videoNalNext(const VideoStream *stream, VideoParser *parser, uint32_t whole)
{
	uint32_t header, type, refIdc;
	VideoNal *nal;

	if (!videoNalFind(stream, parser, whole))
		return -1;
	nal = parser->nal;
	header = *(const uint8_t *)nal->start;
	type = header & 31;
	refIdc = (header >> 5) & 3;
	if (type == NAL_IDR) {
		if (!refIdc)
			videoParserError(stream, parser, 0, ERROR_NAL_HEADER);
		refIdc = 3;
	} else if (type == NAL_SEI || (type >= NAL_AUD && type <= NAL_FILLER)) {
		if (refIdc)
			videoParserError(stream, parser, 0, ERROR_NAL_HEADER);
	}
	nal->refIdc = refIdc;
	nal->type = type;
	return type;
}

/* the units up to the access unit's kind of picture: B, or P when no frame is awaited */
uint32_t videoNalScan(const VideoStream *stream, VideoParser *parser)
{
	VideoNal *nal = parser->nal;
	uint32_t types = 0;

	do {
		types |= videoNalSliceTypes(stream, parser);
		if (types == 7)
			break;
		if (types == 3 && !nal->waitFrame)
			break;
	} while (videoNalNext(stream, parser, 1) >= 1);
	return types;
}
