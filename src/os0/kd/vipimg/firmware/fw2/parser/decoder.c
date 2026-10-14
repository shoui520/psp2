/*
 * FW2's parser library (parser.h): fw1's decoder front end (fw1/pu0/decoder.c)
 * for one context per command. The steps are fw1's chain of callbacks, run from
 * the DMA-complete and the unit's interrupts, without fw1's interrupt nesting.
 *
 * The bit readers, the record and DMA list writers and the interrupt handlers
 * run from the program's code overlay (VIP_LIBRARY_OVERLAY), the rest from
 * DRAM. The functions of each part follow the original's order.
 */
#include "parser.h"

/* the parameter sets' parse results: optional tables after the fixed words */
#define SPS_WORDS		7
#define PPS_WORDS		2
#define SCALING_LIST_WORDS	0x39
#define REF_FRAME_OFFSET_WORDS	0xff

/* hrd_parameters(): the CPB specifications kept (cpb_cnt_minus1 is cut to fit) */
#define HRD_CPBS		32

/* pan_scan_rect(): the rectangles kept */
#define PAN_SCAN_RECTANGLES	3

/* the size of the CABAC state the unit keeps, by chroma */
#define CABAC_ROW_SIZE		0x100
#define CABAC_ROW_SIZE_CHROMA	0x180

/* the CABAC state the unit clears before a slice: 0x1c0 bytes at its 0x800 */
#define CABAC_STATE		0x800
#define CABAC_STATE_SIZE	0x1c0

/* the output space a NAL unit needs, by its type */
#define SPACE_SPS		0x800
#define SPACE_PPS		0x100
#define SPACE_MARKER		0x10
#define SPACE_OTHER		0x1000
#define SPACE_RECORDS		0x40

/* with less input space than this and no record, the input is stuck */
#define INPUT_STUCK		0x280

/* parsed records go out three at a time while an SEI unit is parsed */
#define SEI_RECORDS		3
#define SEI_MAX_BITS		0x40	/* of a message's type or size */
#define SEI_COPY_WORDS		0x80

/* SEI payload types parsed here, besides fw1's */
#define SEI_FRAME_PACKING	45
#define SEI_LAST_COPIED		21	/* the types up to this one are copied */

#define WRITE_LIST_LENGTH	2

VipParserGlobals g_parser VIP_GLOBAL;

/* the context's parser state, saved with it */
static uint32_t g_parsedWords VIP_LIBRARY_SAVED;	/* the parse results to write */
static VipSps *g_currentSps VIP_LIBRARY_SAVED;
static VipPps *g_currentPps VIP_LIBRARY_SAVED;
/*
 * The timestamp state: 0 before an access unit delimiter, 1 after one, 2 once
 * one has come with the same timestamps (they are then left out). The "next"
 * values become the current ones when the unit is finished.
 */
static uint32_t g_savedPts VIP_LIBRARY_SAVED;	/* the last access unit's */
static uint32_t g_nextSavedPts VIP_LIBRARY_SAVED;
static uint32_t g_savedDts VIP_LIBRARY_SAVED;
static uint32_t g_nextSavedDts VIP_LIBRARY_SAVED;
static int8_t g_nextTimestampState VIP_LIBRARY_SAVED;
static uint8_t g_timestampState VIP_LIBRARY_SAVED;
static uint8_t g_savedFlags VIP_LIBRARY_SAVED;
static int8_t g_nextSavedFlags VIP_LIBRARY_SAVED;
static int32_t g_pictureStarted VIP_LIBRARY_SAVED;	/* its header records are out */
/*
 * Skip the rest of an SEI message a word at a time, stopping at an error.
 * Nothing in FW2 sets it.
 */
static uint32_t g_seiSkipChecked VIP_LIBRARY_SAVED;
uint8_t g_keepInput VIP_LIBRARY_SAVED;
void (*g_loadNal)(void (*callback)(void)) VIP_LIBRARY_SAVED;
int8_t g_decoderOpen VIP_LIBRARY_SAVED;
static VipRingHandle g_inputRing VIP_LIBRARY_SAVED;
VipRingHandle g_outputRing VIP_LIBRARY_SAVED;
static VipRingHandle g_recordRing VIP_LIBRARY_SAVED;
VipRingHandle g_outputRecordRing VIP_LIBRARY_SAVED;
static uint32_t g_seiRemaining VIP_LIBRARY_SAVED;	/* bytes of the payload to copy */
static uint32_t g_seiOffset VIP_LIBRARY_SAVED;	/* how far the SEI unit's messages have moved */
static uint32_t g_pictureSetup VIP_LIBRARY_SAVED;	/* VIP_CB_BITSTREAM_PICTURE_SETUP */
uint32_t g_errorCode VIP_LIBRARY_SAVED;
uint32_t g_overflows VIP_LIBRARY_SAVED;		/* units held for output space */
uint32_t g_starved VIP_LIBRARY_SAVED;		/* calls that found no record */
uint32_t g_pictures VIP_LIBRARY_SAVED;

/* a command's */
volatile uint32_t g_decoderState VIP_LIBRARY_LOCAL;
static uint8_t g_nalRefIdc VIP_LIBRARY_LOCAL;
static uint8_t g_nalUnitType VIP_LIBRARY_LOCAL;
static uint32_t g_padded VIP_LIBRARY_LOCAL;	/* the parse results end in a zero word */
static VipSliceInfo g_slice VIP_LIBRARY_LOCAL;
static uint32_t g_lastBytes VIP_LIBRARY_LOCAL;	/* bytes used of slice data's last word */
static uint32_t g_lastBits VIP_LIBRARY_LOCAL;
static uint32_t g_sliceHeaderRead VIP_LIBRARY_LOCAL;	/* the reference lists are read */
static VipParserStream g_writeStream VIP_LIBRARY_LOCAL;
VipParserStream g_readStream VIP_LIBRARY_LOCAL;
static uint32_t g_writeEnded VIP_LIBRARY_LOCAL;	/* the write stream's data is not wanted */
static uint32_t g_misalignment VIP_LIBRARY_LOCAL;	/* the unit's offset in its first 8 bytes */
static int32_t g_recordType VIP_LIBRARY_LOCAL;
static uint32_t g_recordCount VIP_LIBRARY_LOCAL;	/* parsed records not yet sent */
static uint32_t g_recordTotal VIP_LIBRARY_LOCAL;	/* parsed records of the unit */
static uint32_t g_outputRecordCursor VIP_LIBRARY_LOCAL;	/* where flushRecords writes */
static uint32_t g_outputCursor VIP_LIBRARY_LOCAL;	/* where the next record's data starts */
static uint32_t g_nalLength VIP_LIBRARY_LOCAL;	/* from 8 bytes before; an SEI payload's size */
static uint32_t g_nextPosition VIP_LIBRARY_LOCAL;	/* the next record's input position */
static uint32_t g_cabacWritten VIP_LIBRARY_LOCAL;	/* slice data written before the last flush */
static uint32_t g_bitCount VIP_LIBRARY_LOCAL;	/* bits read since it was cleared */
static uint32_t g_pts VIP_LIBRARY_LOCAL;	/* the unit's timestamps, as its records get them */
static uint32_t g_dts VIP_LIBRARY_LOCAL;
static uint8_t g_recordFlags VIP_LIBRARY_LOCAL;	/* VIP_RECORD_PTS... */
static uint32_t g_recordPosition VIP_LIBRARY_LOCAL;	/* the unit's input position */

VipParserDmaSource g_dmaSource VIP_LIBRARY_LOCAL __attribute__((aligned(8)));
static VipDmaDescriptor g_writeStreamLists[2][VIP_STREAM_SEGMENTS] VIP_LIBRARY_LOCAL
	__attribute__((aligned(8)));
static VipDmaDescriptor g_writeList[WRITE_LIST_LENGTH] VIP_LIBRARY_LOCAL
	__attribute__((aligned(8)));
static VipParsedRecord g_parsed[VIP_PARSED_RECORDS] VIP_LIBRARY_LOCAL;

static uint32_t *const g_parseBuffer = (uint32_t *)VIP_PARSED_BUFFER;

static void finishRecord(void);
static void outputRecordOverflow(uint32_t cursor);
static void outputOverflow(uint32_t cursor);
static void spsParsed(void);
static void parseVui(void);
static void parseVuiVclHrd(void);
static void parseVuiEnd(void);
static void vuiWritten(void);
static void ppsParsed(void);
static void parseSliceHeader(void);
static void resumeSliceHeader(void);
static void sliceHeaderParsed(void);
static void sliceHeaderWritten(void);
static void nextSeiMessage(void);
static void parseSei(void);
static void copySeiPayload(void);
static void seiPayloadCopied(void);
static void cavlcStopped(void);
static void startCabac(void);
static void continueSliceData(void);
static void cabacWritten(void);
static void cabacStopped(void);
static void startSliceData(void);
static void streamEnded(void);
static void resetParameterSets(void);
static void clearCabacState(void);
static void outputRecordWritten(void);
static void dropStream(void);
static void enableDecoderClocks(void);
static void gateDecoderClocks(void);

/* let control register writes take effect */
static inline void settle(void)
{
	__asm__ volatile ("nop\n\tnop\n\tnop");
}

/* both read stream interrupts at once */
static inline void acknowledgeReadStream(void)
{
	vipWriteControlBus(VIP_CB_INTERRUPT_CLEAR, ~(1u << VIP_INTERRUPT_READ_STREAM0 |
		1u << VIP_INTERRUPT_READ_STREAM1));
}

static inline void waitBitstream(void)
{
	uint32_t i;

	for (i = 0; i < VIP_BITSTREAM_POLLS; i++)
		if (!vipReadControlBus(VIP_CB_BITSTREAM_COMMAND))
			break;
}

static inline void runBitstream(uint32_t command)
{
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, command);
	waitBitstream();
}

/* ---- in the program's code overlay ---- */

/*
 * Queue a VIP_DMA_LIST request (the list is in local RAM, so the request has no
 * DRAM address); -1 if the queue is full.
 */
VIP_LIBRARY_OVERLAY int queueListDma(uint32_t control, uint32_t localLength,
	void (*callback)(void), volatile uint32_t *done)
{
	VipDmaRequest *request;
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	request = &g_dmaQueue.requests[g_dmaQueue.tail];
	if (g_dmaQueue.count < 2) {
		vipWriteControlBus(VIP_CB_DMA_DIRECTION, control);
		vipWriteControlBus(VIP_CB_DMA_LOCAL_LENGTH, localLength);
		if (!g_dmaQueue.count)
			vipWriteControlBus(VIP_CB_DMA_BUSY, 1);
	} else if (g_dmaQueue.count == VIP_DMA_QUEUE_LENGTH) {
		if (psw & VIP_PSW_IEC)
			mep_ei();
		return -1;
	} else {
		request->control = control;
		request->localLength = localLength;
	}
	request->done = done;
	request->callback = callback;
	g_dmaQueue.count++;
	if (done)
		*done = 1;
	g_dmaQueue.tail = (g_dmaQueue.tail + 1) & (VIP_DMA_QUEUE_LENGTH - 1);
	if (psw & VIP_PSW_IEC)
		mep_ei();
	return 0;
}

/* the next count bits of the unit (0: 32) */
VIP_LIBRARY_OVERLAY uint32_t readBits(uint32_t count)
{
	uint32_t value;

	vipWriteControlBus(VIP_CB_BITSTREAM_READ_LENGTH, count);
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_READ);
	if (!count)
		count = 32;
	waitBitstream();
	value = vipReadControlBus(VIP_CB_BITSTREAM_READ_VALUE);
	g_bitCount += count;
	return value;
}

/* an exp-Golomb code: ue(v), or se(v) if isSigned */
static VIP_LIBRARY_OVERLAY uint32_t readGolomb(uint32_t isSigned)
{
	uint32_t bits, value;

	vipWriteControlBus(VIP_CB_BITSTREAM_READ_LENGTH, isSigned);
	runBitstream(VIP_BITSTREAM_READ_GOLOMB);
	bits = vipReadControlBus(VIP_CB_BITSTREAM_GOLOMB_BITS);
	value = vipReadControlBus(VIP_CB_BITSTREAM_READ_VALUE);
	g_bitCount += bits;
	return value;
}

/* Fill in the next parsed record; setRecordSize gives its data. */
static VIP_LIBRARY_OVERLAY void addRecord(uint32_t type, uint32_t value)
{
	VipParsedRecord *record = &g_parsed[g_recordCount];

	record->pts = g_pts;
	record->dts = g_dts;
	record->header = type << VIP_RECORD_TYPE_SHIFT |
		(g_recordFlags & VIP_RECORD_FLAGS_MASK) << VIP_RECORD_FLAGS_SHIFT |
		(value & VIP_RECORD_VALUE_MASK);
}

/* The next record's data: size bytes at the output cursor. */
static VIP_LIBRARY_OVERLAY void setRecordSize(uint32_t size)
{
	const VipRingSegment *segments = g_writeStream.segments;
	uint32_t cursor = g_outputCursor;

	if (cursor >= segments[0].start && cursor - segments[0].start >= segments[0].size)
		cursor += segments[1].start - segments[0].start - segments[0].size;
	g_parsed[g_recordCount].position = cursor;
	g_outputCursor = cursor + size;
}

static inline void putRecord(uint32_t type, uint32_t value, uint32_t size)
{
	addRecord(type, value);
	setRecordSize(size);
	g_recordCount++;
}

/* Read past the bytes before the unit in its first 8 bytes. */
static inline void skipMisalignment(uint32_t misalignment)
{
	if (misalignment >= 4)
		readBits(0);
	if (misalignment & 3)
		readBits((misalignment & 3) * 8);
}

/* Make those bytes 0xff, not a start code, and read past them. */
static VIP_LIBRARY_OVERLAY void maskMisalignment(uint32_t misalignment)
{
	uint8_t *nal = (uint8_t *)VIP_NAL_BUFFER;
	uint32_t i;

	if (!misalignment)
		return;
	for (i = 0; i < misalignment; i++)
		nal[i] = 0xff;
	mep_syncm();
	skipMisalignment(misalignment);
}

/* Read the start code; the last value read. */
static inline uint32_t readStartCode(void)
{
	uint32_t value = readBits(0);

	if ((value & 0xffff) == 1)	/* 00 00 00 01 */
		value = readBits(8);
	return value;
}

/*
 * Write the parsed records to the output record ring and run callback; when they
 * do not fit, hold the ring and stop (-1).
 */
static VIP_LIBRARY_OVERLAY int flushRecords(void (*callback)(void))
{
	VipRingSegment segments[2];
	uint32_t length = g_recordCount * sizeof(VipParsedRecord);
	uint32_t cursor = g_outputRecordCursor, copied = 0, size, i;
	const uint32_t *source;
	uint32_t *destination;
	int count;

	count = getRingWritableBeforeFirstReader(&g_outputRecordRing, segments, 2);
	for (i = 0; (int)i < count; i++) {
		uint32_t end = segments[i].start + segments[i].size;

		if (cursor < segments[i].start)
			continue;
		destination = (uint32_t *)cursor;
		source = (const uint32_t *)((const uint8_t *)g_parsed + (copied & ~3u));
		size = length - copied;
		if (end - cursor < size)
			size = end - cursor;
		copied += size;
		cursor += size;
		if (cursor == end) {
			if ((int)i + 1 >= count) {
				outputRecordOverflow(cursor);
				return -1;
			}
			cursor = segments[i + 1].start;
		}
		for (size /= 4; size; size--)
			*destination++ = *source++;
		if (copied >= length)
			break;
	}
	g_outputRecordCursor = cursor;
	g_recordCount = 0;
	callback();
	return 0;
}

/* Count the records, and send them. */
static inline int sendRecords(void (*callback)(void))
{
	g_recordTotal += g_recordCount;
	return flushRecords(callback);
}

/*
 * Copy length bytes of parse results at local to the write stream, then run
 * callback. When the stream has no room, hold the output and stop.
 */
static VIP_LIBRARY_OVERLAY void writeParsed(uint32_t local, uint32_t length,
	void (*callback)(void))
{
	const VipRingSegment *segment = g_writeStream.segments;
	uint32_t control = VIP_DMA_LIST, cursor = g_writeStream.cursor;
	uint32_t written = 0, count = 0, size, i;

	if ((local & 0xffff0000) == VIP_BANK1)
		control |= VIP_DMA_BANK1;
	for (i = 0; i < g_writeStream.segmentCount; i++, segment++) {
		uint32_t end = segment->start + segment->size;

		if (cursor < segment->start)
			continue;
		size = length - written;
		if (end - cursor < size)
			size = end - cursor;
		g_writeList[count].localLength = (local + (written & ~3u)) << 16 | size;
		g_writeList[count].external = cursor;
		written += size;
		cursor += size;
		if (cursor == end) {
			if (i + 1 >= g_writeStream.segmentCount) {
				outputOverflow(cursor);
				return;
			}
			cursor = segment[1].start;
		}
		count++;
		if (written >= length)
			break;
	}
	g_writeStream.cursor = cursor;
	queueListDma(control, (uint32_t)g_writeList << 16 | count * sizeof(VipDmaDescriptor),
		callback, NULL);
}

/*
 * The unit is in the read stream: start the bitstream unit on it and read its
 * NAL header.
 */
static VIP_LIBRARY_OVERLAY void startNal(void)
{
	uint32_t sliceStart, header;

	sliceStart = vipReadControlBus(VIP_CB_BITSTREAM_SLICE_START);
	vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 0);
	vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 1);
	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_NAL);
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY0, 1);
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY1, 1);
	vipWriteControlBus(VIP_CB_BITSTREAM_SLICE_START, sliceStart);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY0, 1);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY1, 1);
	vipWriteControlBus(VIP_CB_BITSTREAM_READ_WORDS, 0);
	vipWriteControlBus(VIP_CB_BITSTREAM_NAL_BITS, g_nalLength << 3);
	runBitstream(VIP_BITSTREAM_LOAD);
	maskMisalignment(g_misalignment);
	header = readStartCode();
	g_nalRefIdc = (header & 0x60) >> 5;
	g_nalUnitType = header & 0x1f;
	g_padded = 0;
}

/* Route the unit's and the streams' interrupts to level 1, and enable them. */
VIP_DRAM void setupInterrupts(void)
{
	mep_di();
	vipWriteControlBus(VIP_CB_INTERRUPT_PENDING,
		vipReadControlBus(VIP_CB_INTERRUPT_PENDING) | VIP_PARSER_INTERRUPTS);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL0,
		vipReadControlBus(VIP_CB_INTERRUPT_LEVEL0) | 0x11100000);	/* 5 to 7 */
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL1,
		vipReadControlBus(VIP_CB_INTERRUPT_LEVEL1) | 0x1111);	/* 8 to 11 */
	vipWriteControlBus(VIP_CB_INTERRUPT_CLEAR,
		vipReadControlBus(VIP_CB_INTERRUPT_CLEAR) & ~VIP_PARSER_INTERRUPTS);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE,
		vipReadControlBus(VIP_CB_INTERRUPT_ENABLE) | VIP_PARSER_INTERRUPTS);
	settle();
	mep_ei();
}

/* the bitstream unit's interrupt: the step it finished, by the decoder's state */
VIP_LIBRARY_OVERLAY void decoderInterrupt(void)
{
	vipAcknowledgeInterrupt(VIP_INTERRUPT_DECODER);
	switch (g_decoderState) {
	case VIP_DECODER_SPS:
		spsParsed();
		break;
	case VIP_DECODER_PPS:
		ppsParsed();
		break;
	case VIP_DECODER_CAVLC:
		cavlcStopped();
		break;
	case VIP_DECODER_CABAC:
		cabacStopped();
		break;
	case VIP_DECODER_SLICE_HEADER:
		sliceHeaderParsed();
		break;
	default:
		streamEnded();
		break;
	}
}

/* the program's interrupts: the library's units' and the scanner's */
VIP_LIBRARY_OVERLAY void parserInterrupt(void)
{
	switch (vipReadControlBus(VIP_CB_INTERRUPT_STATUS) >> 3 & 0x1f) {
	case VIP_INTERRUPT_WRITE_STREAM1:
	case VIP_INTERRUPT_WRITE_STREAM0:
		writeStreamInterrupt();
		break;
	case VIP_INTERRUPT_READ_STREAM1:
	case VIP_INTERRUPT_READ_STREAM0:
		readStreamInterrupt();
		break;
	case VIP_INTERRUPT_DECODER:
		decoderInterrupt();
		break;
	default:
		scannerInterrupt();
		break;
	}
}

/* the unit's interrupts 8 and 11: only acknowledged */
VIP_LIBRARY_OVERLAY void interrupt8(void)
{
	vipAcknowledgeInterrupt(VIP_INTERRUPT_8);
}

VIP_LIBRARY_OVERLAY void interrupt11(void)
{
	vipAcknowledgeInterrupt(VIP_INTERRUPT_11);
}

/* tell the hardware a buffer half is ready (DMA callbacks) */
VIP_LIBRARY_OVERLAY void readStreamHalf0Ready(void)
{
	vipWriteControlBus(VIP_CB_READ_STREAM_READY0, 1);
}

VIP_LIBRARY_OVERLAY void readStreamHalf1Ready(void)
{
	vipWriteControlBus(VIP_CB_READ_STREAM_READY1, 1);
}

static VIP_LIBRARY_OVERLAY void writeStreamHalf0Ready(void)
{
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY0, 1);
}

static VIP_LIBRARY_OVERLAY void writeStreamHalf1Ready(void)
{
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY1, 1);
}

/*
 * The write stream has filled a half: list it out to the stream's segments and
 * queue it. When the stream reaches the end of its last segment, the output is
 * held instead.
 */
VIP_LIBRARY_OVERLAY void writeStreamInterrupt(void)
{
	VipParserStream *stream = &g_writeStream;
	const VipRingSegment *segment = stream->segments;
	uint32_t half = stream->half & 1, cursor = stream->cursor;
	uint32_t drained = 0, count = 0, length, offset, end, i;
	VipDmaDescriptor *list;

	if (half) {
		vipAcknowledgeInterrupt(VIP_INTERRUPT_WRITE_STREAM1);
		list = g_writeStreamLists[1];
	} else {
		vipAcknowledgeInterrupt(VIP_INTERRUPT_WRITE_STREAM0);
		list = g_writeStreamLists[0];
	}
	if (g_writeEnded) {
		stream->half++;
		vipWriteControlBus(VIP_CB_WRITE_STREAM_READY0, 1);
		vipWriteControlBus(VIP_CB_WRITE_STREAM_READY1, 1);
		return;
	}
	for (i = 0; i < stream->segmentCount; i++, segment++) {
		if (cursor < segment->start)
			continue;
		end = segment->start + segment->size;
		length = VIP_STREAM_HALF - drained;
		if (end - cursor < length)
			length = end - cursor;
		offset = VIP_WRITE_BUFFER + drained;
		if (half)
			offset += VIP_STREAM_HALF;
		drained += length;
		list[count].localLength = offset << 16 | length;
		list[count].external = cursor;
		cursor += length;
		if (cursor == end) {
			if (i + 1 >= stream->segmentCount) {
				stream->cursor = cursor;
				holdWriteStream();
				return;
			}
			cursor = segment[1].start;
		}
		count++;
		if (drained >= VIP_STREAM_HALF)
			break;
	}
	stream->cursor = cursor;
	stream->half++;
	queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_WRITE,
		(uint32_t)list << 16 | count * sizeof(VipDmaDescriptor),
		half ? writeStreamHalf1Ready : writeStreamHalf0Ready, NULL);
}

/* ---- in DRAM ---- */

/* Stop the start code scanner and start it over. */
void resetScanner(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW), enable;

	mep_di();
	enable = vipReadControlBus(VIP_CB_CLOCK_ENABLE);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, enable | VIP_CLOCK_DEMUX);
	vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 0);
	vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 1);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, enable & ~VIP_CLOCK_DEMUX);
	__asm__ volatile ("nop\n\tnop");
	if (psw & VIP_PSW_IEC)
		mep_ei();
	g_scannerState = 0;
}

/* Stop the bitstream unit and the streams, drop their interrupts, and restart. */
void resetBitstream(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 0);
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 0);
	vipWriteControlBus(VIP_CB_INTERRUPT_CLEAR, ~VIP_PARSER_INTERRUPTS);
	settle();
	if (psw & VIP_PSW_IEC)
		mep_ei();
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 2);
	vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 1);
	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_NAL);
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY0, 1);
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY1, 1);
}

/* Take the unit's record, and its data, from the input. */
static inline void takeRecord(void)
{
	while (advanceRingReader(&g_recordRing, sizeof(VipRecord)))
		;
	while (setRingReader(&g_inputRing, g_nextPosition))
		;
}

/*
 * The unit's records are out: hand its data and records to the codec, take it
 * from the input, and go on with the next record. The end of a picture's input
 * ends the command, the end of the stream closes the parser.
 */
static void finishRecord(void)
{
	while (setRingWriter(&g_outputRing, g_writeStream.cursor))
		;
	while (advanceRingWriter(&g_outputRecordRing, g_recordTotal * sizeof(VipParsedRecord)))
		;
	takeRecord();
	g_pictureStarted = 0;
	g_timestampState = g_nextTimestampState;
	g_savedPts = g_nextSavedPts;
	g_savedDts = g_nextSavedDts;
	g_savedFlags = g_nextSavedFlags;
	if (g_recordType == VIP_RECORD_AVC_START || g_recordType == VIP_RECORD_STREAM_END) {
		g_pictures++;
		g_decoderState = VIP_DECODER_IDLE;
		g_decoderOpen = 0;
		gateDecoderClocks();
	} else if (g_recordType == VIP_RECORD_PICTURE_END) {
		g_pictures++;
		g_decoderState = VIP_DECODER_PICTURE_DONE;
		gateDecoderClocks();
	} else {
		decodeRecords();
	}
}

/* The output record ring is full at cursor: hold it until the codec makes room. */
static VIP_DRAM void outputRecordOverflow(uint32_t cursor)
{
	g_outputRecordCursor = cursor;
	g_overflows++;
	setRingWriterState(&g_outputRecordRing, 1);
	resetBitstream();
	g_decoderState = VIP_DECODER_HELD;
	gateDecoderClocks();
}

/* Drop the unit and go on with the next. */
static void consumeRecord(void)
{
	takeRecord();
	decodeRecords();
}

static void spsWritten(void)
{
	uint32_t vui = g_parseBuffer[4] & 1;

	putRecord(VIP_PARSED_SPS, 0, (g_parsedWords - g_padded) * 4);
	if (g_padded)
		putRecord(VIP_PARSED_PADDING, 0, 4);
	sendRecords(vui ? parseVui : finishRecord);
}

static void ppsWritten(void)
{
	putRecord(VIP_PARSED_PPS, 0, (g_parsedWords - g_padded) * 4);
	if (g_padded)
		putRecord(VIP_PARSED_PADDING, 0, 4);
	sendRecords(finishRecord);
}

/* A picture's header records are out: hand them over before its slice data. */
static void sliceRecordsWritten(void)
{
	while (advanceRingWriter(&g_outputRecordRing, g_recordTotal * sizeof(VipParsedRecord)))
		;
	g_recordTotal = 0;
	startSliceData();
}

/*
 * The slice header is out. A picture's first slice header is handed over at
 * once; the others' records wait for the slice data (three at a time).
 */
static void sliceHeaderWritten(void)
{
	uint32_t value = g_slice.fieldPic << VIP_PARSED_FIELD_PIC_SHIFT & 0xffff;

	if (g_recordType == VIP_RECORD_AVC_PICTURE) {
		if (!g_pictureStarted) {
			putRecord(VIP_PARSED_PICTURE_HEADER, value, g_parsedWords * 4);
			sendRecords(sliceRecordsWritten);
			g_pictureStarted = 1;
		} else {
			setRecordSize(g_parsedWords * 4);
			startSliceData();
		}
	} else {
		putRecord(VIP_PARSED_SLICE_HEADER, value, g_parsedWords * 4);
		if (g_recordCount != SEI_RECORDS)
			startSliceData();
		else
			sendRecords(startSliceData);
	}
}

/* The write stream is full at cursor: hold the output until the codec makes room. */
static VIP_DRAM void outputOverflow(uint32_t cursor)
{
	g_writeStream.cursor = cursor;
	g_overflows++;
	setRingWriterState(&g_outputRing, 1);
	resetBitstream();
	g_decoderState = VIP_DECODER_HELD;
	gateDecoderClocks();
}

/* After a failed parse: hand over the unit's records (they have no data). */
static void failedRecordWritten(void)
{
	while (setRingWriter(&g_outputRing, g_outputCursor))
		;
	while (advanceRingWriter(&g_outputRecordRing, g_recordTotal * sizeof(VipParsedRecord)))
		;
}

static void delimiterWritten(void)
{
	putRecord(VIP_PARSED_DELIMITER, 0, 8);
	sendRecords(finishRecord);
}

/* access_unit_delimiter_rbsp(): primary_pic_type */
static void parseDelimiter(void)
{
	uint32_t *buffer = g_parseBuffer;
	uint32_t primaryPicType = readBits(8) >> 5;

	buffer[1] = 0;
	buffer[0] = primaryPicType << 29;
	writeParsed((uint32_t)buffer, 8, delimiterWritten);
}

/* hrd_parameters(), into the parse buffer after the VUI's words */
static void parseHrd(VipHrdInfo *hrd)
{
	uint32_t *buffer = g_parseBuffer + 7;
	uint32_t count, cbr = 0, lengths, i;

	count = readGolomb(0);
	if (count >= HRD_CPBS)
		count = HRD_CPBS - 1;
	hrd->cpbCntMinus1 = count;
	buffer[0] = count << 27 | readBits(8) << 19;	/* bit_rate_scale, cpb_size_scale */
	for (i = 0; i <= hrd->cpbCntMinus1; i++) {
		buffer[1 + 2 * i] = readGolomb(0);	/* bit_rate_value_minus1 */
		buffer[2 + 2 * i] = readGolomb(0);	/* cpb_size_value_minus1 */
		cbr |= readBits(1) << (31 - i);		/* cbr_flag */
	}
	buffer[65] = cbr;
	lengths = readBits(20);
	buffer[66] = lengths << 12;
	hrd->timeOffsetLength = lengths & 0x1f;
	hrd->initialCpbRemovalDelayLengthMinus1 = lengths >> 15;
	hrd->cpbRemovalDelayLengthMinus1 = lengths >> 10 & 0x1f;
	hrd->dpbOutputDelayLengthMinus1 = lengths >> 5 & 0x1f;
}

/* A failed SPS parse goes out as an empty record. */
static void spsFailed(void)
{
	g_errorCode = VIP_DECODER_ERROR_SPS;
	putRecord(VIP_PARSED_SPS, 0, 0);
	sendRecords(failedRecordWritten);
	consumeRecord();
}

/*
 * The unit has parsed an SPS: keep what the slice headers need, and write the
 * results out.
 */
static VIP_DRAM void spsParsed(void)
{
	VipSps *sps;
	uint32_t word, id, scalingLists, refFrameOffsets, words;

	g_decoderState = VIP_DECODER_BUSY;
	if (vipReadControlBus(VIP_CB_BITSTREAM_STATUS)) {
		spsFailed();
		return;
	}
	word = g_parseBuffer[0];
	id = word >> 2 & 0xff;
	sps = &g_parser.sps[id < VIP_SPS_COUNT ? id : 0];
	g_currentSps = sps;
	sps->valid = 1;
	scalingLists = word >> 31 & 1;
	refFrameOffsets = word >> 30 & 1;
	if ((uint8_t)(word >> 22) == 100)	/* High profile */
		sps->chroma = (word & 3) != 0;
	else
		sps->chroma = 1;
	sps->levelIdc = word >> 10;
	sps->log2MaxFrameNumMinus4 = g_parseBuffer[1] >> 28 & 0xf;
	sps->picOrderCntType = g_parseBuffer[1] >> 26 & 3;
	sps->log2MaxPicOrderCntLsbMinus4 = g_parseBuffer[1] >> 22 & 0xf;
	sps->deltaPicOrderAlwaysZero = g_parseBuffer[1] >> 21 & 1;
	sps->frameMbsOnly = g_parseBuffer[4] >> 4 & 1;
	sps->mbAdaptiveFrameField = g_parseBuffer[4] >> 3 & 1;
	sps->picWidthInMbsMinus1 = g_parseBuffer[4] >> 16;
	sps->direct8x8Inference = g_parseBuffer[4] >> 2 & 1;
	words = SPS_WORDS;
	if (scalingLists)
		words += SCALING_LIST_WORDS;
	if (refFrameOffsets)
		words += REF_FRAME_OFFSET_WORDS;
	if (scalingLists + refFrameOffsets != 1) {
		/* an even number of words: whole 8-byte units */
		g_parseBuffer[words++] = 0;
		g_padded = 1;
	}
	g_parsedWords = words;
	writeParsed(VIP_PARSED_BUFFER, words * 4, spsWritten);
}

/* vui_parameters(), up to the NAL HRD; the SPS record's callback */
static void parseVui(void)
{
	uint32_t *buffer = g_parseBuffer;
	uint32_t flag, value;

	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_RAW);
	flag = readBits(1);			/* aspect_ratio_info_present_flag */
	buffer[0] = flag << 31;
	if (flag) {
		value = readBits(8);		/* aspect_ratio_idc */
		buffer[0] |= value << 23;
		if (value == 255)		/* Extended_SAR */
			buffer[1] = readBits(0);
	}
	flag = readBits(1);			/* overscan_info_present_flag */
	buffer[0] |= flag << 22;
	if (flag)
		buffer[0] |= readBits(1) << 21;
	flag = readBits(1);			/* video_signal_type_present_flag */
	buffer[2] = flag << 31;
	if (flag) {
		value = readBits(5);
		buffer[2] |= value << 26;
		if (value & 1)			/* colour_description_present_flag */
			buffer[2] |= readBits(24) << 2;
	}
	flag = readBits(1);			/* chroma_loc_info_present_flag */
	buffer[0] |= flag << 20;
	if (flag) {
		buffer[0] |= readGolomb(0) << 17;
		buffer[0] |= readGolomb(0) << 14;
	}
	flag = readBits(1);			/* timing_info_present_flag */
	buffer[0] |= flag << 13;
	if (flag) {
		buffer[3] = readBits(0);	/* num_units_in_tick */
		buffer[4] = readBits(0);	/* time_scale */
		buffer[0] |= readBits(1) << 12;
	}
	flag = readBits(1);			/* nal_hrd_parameters_present_flag */
	g_currentSps->nalHrd.present = flag;
	buffer[6] = flag << 31;
	if (flag)
		parseHrd(&g_currentSps->nalHrd);
	writeParsed((uint32_t)(buffer + 6), 0x110, parseVuiVclHrd);
}

static void parseVuiVclHrd(void)
{
	uint32_t *buffer = g_parseBuffer;
	uint32_t flag;

	flag = readBits(1);			/* vcl_hrd_parameters_present_flag */
	g_currentSps->vclHrd.present = flag;
	buffer[6] = flag << 31;
	if (flag)
		parseHrd(&g_currentSps->vclHrd);
	writeParsed((uint32_t)(buffer + 6), 0x110, parseVuiEnd);
}

static void parseVuiEnd(void)
{
	uint32_t *buffer = g_parseBuffer;
	uint32_t value;

	if (g_currentSps->nalHrd.present || g_currentSps->vclHrd.present)
		buffer[0] |= readBits(1) << 11;	/* low_delay_hrd_flag */
	value = readBits(2);	/* pic_struct_present_flag, bitstream_restriction_flag */
	buffer[0] |= (value & 2) << 9;
	buffer[5] = value << 31;
	g_currentSps->picStructPresent = value >> 1;
	if (value & 1) {
		buffer[5] |= readBits(1) << 30;
		buffer[5] |= readGolomb(0) << 25;
		buffer[5] |= readGolomb(0) << 20;
		buffer[5] |= readGolomb(0) << 15;
		buffer[5] |= readGolomb(0) << 10;
		buffer[5] |= readGolomb(0) << 5;
		buffer[5] |= readGolomb(0);
	}
	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_NAL);
	writeParsed((uint32_t)buffer, 0x18, vuiWritten);
}

static void vuiWritten(void)
{
	putRecord(VIP_PARSED_VUI, 0, 0x238);
	sendRecords(finishRecord);
}

/* The unit has parsed a PPS (see spsParsed). */
static VIP_DRAM void ppsParsed(void)
{
	VipPps *pps;
	uint32_t word, id, words;

	g_decoderState = VIP_DECODER_BUSY;
	if (vipReadControlBus(VIP_CB_BITSTREAM_STATUS)) {
		g_errorCode = VIP_DECODER_ERROR_PPS;
		putRecord(VIP_PARSED_PPS, 0, 0);
		sendRecords(failedRecordWritten);
		consumeRecord();
		return;
	}
	word = g_parseBuffer[0];
	id = word >> 23 & 0xff;
	pps = &g_parser.pps[id < VIP_PPS_COUNT ? id : 0];
	g_currentPps = pps;
	pps->valid = 1;
	pps->spsId = word >> 15;
	if (pps->spsId >= VIP_SPS_COUNT)
		pps->spsId = 0;
	pps->entropyCodingMode = word >> 14 & 1;
	pps->bottomFieldPicOrderInFramePresent = word >> 13 & 1;
	pps->numRefIdxL0DefaultActiveMinus1 = word >> 8 & 0x1f;
	pps->numRefIdxL1DefaultActiveMinus1 = word >> 3 & 0x1f;
	pps->weightedPred = g_parseBuffer[1] >> 31 & 1;
	pps->weightedBipredIdc = g_parseBuffer[1] >> 29 & 3;
	pps->picInitQp = (g_parseBuffer[1] >> 23 & 0x3f) + 26;
	pps->deblockingFilterControlPresent = g_parseBuffer[1] >> 17 & 1;
	pps->constrainedIntraPred = g_parseBuffer[1] >> 15 & 1;
	words = PPS_WORDS;
	if (word >> 31) {			/* the scaling lists */
		words += SCALING_LIST_WORDS;
		g_parseBuffer[words++] = 0;
		g_padded = 1;
	}
	g_parsedWords = words;
	writeParsed(VIP_PARSED_BUFFER, words * 4, ppsWritten);
}

/*
 * A slice: find its parameter sets (-1 if one is missing), and start the unit on
 * its header.
 */
static int startSlice(void)
{
	VipSps *sps;
	VipPps *pps;
	uint32_t ppsId;

	readGolomb(0);			/* first_mb_in_slice */
	readGolomb(0);			/* slice_type */
	ppsId = readGolomb(0);
	pps = &g_parser.pps[ppsId < VIP_PPS_COUNT ? ppsId : 0];
	g_currentPps = pps;
	sps = &g_parser.sps[pps->spsId];
	g_currentSps = sps;
	if (!sps->valid || !pps->valid)
		return -1;
	runBitstream(VIP_BITSTREAM_LOAD);
	vipWriteControlBus(VIP_CB_BITSTREAM_READ_WORDS, 0);
	vipWriteControlBus(VIP_CB_BITSTREAM_NAL_BITS, g_nalLength << 3);
	skipMisalignment(g_misalignment);
	readStartCode();
	g_parsedWords = VIP_SLICE_HEADER_WORDS;
	vipWriteControlBus(VIP_CB_BITSTREAM_NAL_HEADER, g_nalRefIdc << 5 | g_nalUnitType);
	if (g_recordType == VIP_RECORD_AVC_PICTURE)
		g_pictureSetup = sps->chroma << 27 | sps->log2MaxFrameNumMinus4 << 23 |
			(uint8_t)sps->frameMbsOnly << 22 |
			sps->log2MaxPicOrderCntLsbMinus4 << 18 |
			sps->picOrderCntType << 16 |
			(uint8_t)sps->deltaPicOrderAlwaysZero << 14 |
			(uint8_t)pps->entropyCodingMode << 28 |
			pps->numRefIdxL0DefaultActiveMinus1 << 9 |
			pps->numRefIdxL1DefaultActiveMinus1 << 4 |
			(uint8_t)pps->bottomFieldPicOrderInFramePresent << 15 |
			pps->weightedPred << 3 | pps->weightedBipredIdc << 1 |
			pps->deblockingFilterControlPresent;
	vipWriteControlBus(VIP_CB_BITSTREAM_PICTURE_SETUP, g_pictureSetup);
	g_decoderState = VIP_DECODER_SLICE_HEADER;
	g_sliceHeaderRead = 0;
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_SLICE_HEADER);
	return 0;
}

/* Load the unit again from the read stream's start, past its start code. */
static void reloadNal(void)
{
	runBitstream(VIP_BITSTREAM_LOAD);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY0, 1);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY1, 1);
	vipWriteControlBus(VIP_CB_BITSTREAM_READ_WORDS, 0);
	vipWriteControlBus(VIP_CB_BITSTREAM_NAL_BITS, g_nalLength << 3);
	maskMisalignment(g_misalignment);
	readStartCode();
}

/* The reference lists are out: have the unit parse the rest of the header. */
static void resumeSliceHeader(void)
{
	reloadNal();
	g_decoderState = VIP_DECODER_SLICE_HEADER;
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_SLICE_HEADER);
}

/* one list of ref_pic_list_modification(), at most entries long */
static inline void readListChanges(uint32_t *list, uint32_t entries)
{
	uint32_t idc, i;

	for (i = 0; i < entries; i++) {
		idc = readGolomb(0) & 0xff;	/* modification_of_pic_nums_idc */
		if (idc == 3) {
			if (i != VIP_SLICE_LIST_ENTRIES)
				list[i] = VIP_SLICE_LIST_END;
			return;
		}
		list[i] = (idc << 29) + (readGolomb(0) & 0x1ffff);
	}
}

/*
 * The unit left the reference list changes to us: read the slice header up to
 * them and write them after the unit's results.
 */
static void parseSliceHeader(void)
{
	uint32_t *buffer = g_parseBuffer;
	uint32_t sliceType, fieldPic = 0, list1;

	reloadNal();
	g_parsedWords = VIP_SLICE_HEADER_WORDS;
	readGolomb(0);				/* first_mb_in_slice */
	sliceType = readGolomb(0) & 0xff;
	if (sliceType >= 5)
		sliceType = (sliceType - 5) & 0xff;
	readGolomb(0);				/* pic_parameter_set_id */
	readBits(g_currentSps->log2MaxFrameNumMinus4 + 4);	/* frame_num */
	if (!g_currentSps->frameMbsOnly) {
		fieldPic = readBits(1) & 0xff;
		if (fieldPic)
			readBits(1);		/* bottom_field_flag */
	}
	if (g_nalUnitType == 5)
		readGolomb(0);			/* idr_pic_id */
	if (g_currentSps->picOrderCntType == 0) {
		readBits(g_currentSps->log2MaxPicOrderCntLsbMinus4 + 4);
		if (!fieldPic && g_currentPps->bottomFieldPicOrderInFramePresent)
			readGolomb(1);		/* delta_pic_order_cnt_bottom */
	} else if (g_currentSps->picOrderCntType == 1 &&
		   !g_currentSps->deltaPicOrderAlwaysZero) {
		readGolomb(1);			/* delta_pic_order_cnt[0] */
		if (!fieldPic && g_currentPps->bottomFieldPicOrderInFramePresent)
			readGolomb(1);
	}
	if (sliceType == 0) {			/* P */
		if (readBits(1))		/* num_ref_idx_active_override_flag */
			readGolomb(0);
	} else if (sliceType == 1) {		/* B */
		readBits(1);			/* direct_spatial_mv_pred_flag */
		if (readBits(1)) {
			readGolomb(0);
			readGolomb(0);
		}
	}
	if (sliceType != 2) {
		if (readBits(1))
			readListChanges(buffer + VIP_SLICE_HEADER_WORDS, VIP_SLICE_LIST_ENTRIES + 1);
		if (sliceType == 1 && readBits(1)) {
			list1 = VIP_SLICE_HEADER_WORDS + VIP_SLICE_LIST_ENTRIES;
			if (buffer[0] >> VIP_SLICE_HAS_WEIGHTS & 1)
				list1 += VIP_SLICE_WEIGHT_WORDS;
			if (buffer[0] >> VIP_SLICE_HAS_MARKING & 1)
				list1 += VIP_SLICE_MARKING_WORDS;
			readListChanges(buffer + list1, VIP_SLICE_LIST_ENTRIES);
		}
	}
	buffer[0] |= (g_nalUnitType == 5) << 3 | (g_nalRefIdc != 0) << 2;
	g_parsedWords += VIP_SLICE_LIST_WORDS;
	if (buffer[0] >> VIP_SLICE_HAS_WEIGHTS & 1)
		g_parsedWords += VIP_SLICE_WEIGHT_WORDS;
	if (buffer[0] >> VIP_SLICE_HAS_MARKING & 1)
		g_parsedWords += VIP_SLICE_MARKING_WORDS;
	g_sliceHeaderRead = 1;
	acknowledgeReadStream();
	if (g_readStream.half) {
		g_readStream.half = 0;
		g_readStream.cursor = g_recordPosition;
		g_loadNal(NULL);
	}
	mep_syncm();
	writeParsed(VIP_PARSED_BUFFER, g_parsedWords * 4, resumeSliceHeader);
}

/* the number of leading zero bits of x */
static uint32_t countLeadingZeros(uint32_t x)
{
	uint32_t y, m, n;

	y = -(x >> 16);		/* the left half is 0: n = 16 */
	m = y >> 16 & 16;
	n = 16 - m;
	x >>= m;
	y = x - 0x100;		/* bits 8-15 are 0: add 8, shift left 8 */
	m = y >> 16 & 8;
	n += m;
	x <<= m;
	y = x - 0x1000;
	m = y >> 16 & 4;
	n += m;
	x <<= m;
	y = x - 0x4000;
	m = y >> 16 & 2;
	n += m;
	x <<= m;
	y = x >> 14;		/* 0 to 3 */
	m = y & ~(y >> 1);
	return n + 2 - m;
}

/* dividend / divisor and its remainder, by shifting and subtracting */
static void longDivide(uint32_t dividend, uint32_t divisor, uint32_t *quotient,
	uint32_t *remainder)
{
	uint32_t shift = 0, steps, bit, rest = 0, result = 0, zeros, skip;

	if (dividend < divisor) {
		*remainder = dividend;
		*quotient = 0;
		return;
	}
	if (dividend < 0x10000000)
		shift = countLeadingZeros(dividend);
	dividend <<= shift;
	steps = 31 - shift;
	bit = 1u << steps;
	if (divisor > 16) {
		/* the dividend's first bits are below the divisor: start past them */
		zeros = countLeadingZeros(divisor);
		skip = 31 - zeros;
		rest = dividend >> (zeros + 1);
		bit >>= skip;
		dividend <<= skip;
		steps -= skip;
	}
	do {
		rest = rest << 1 | dividend >> 31;
		if (rest >= divisor) {
			result |= bit;
			rest -= divisor;
		}
		dividend <<= 1;
		bit >>= 1;
	} while (steps--);
	*remainder = rest;
	*quotient = result;
}

static inline void divide(uint32_t dividend, uint32_t divisor, uint32_t *quotient,
	uint32_t *remainder)
{
	if (dividend > divisor) {
		longDivide(dividend, divisor, quotient, remainder);
	} else if (dividend == divisor) {
		*quotient = 1;
		*remainder = 0;
	} else {
		*quotient = 0;
		*remainder = dividend;
	}
}

/*
 * The unit has parsed a slice header: read the lists it left to us first, then
 * keep what the slice data needs and write the results out.
 */
static VIP_DRAM void sliceHeaderParsed(void)
{
	VipSps *sps = g_currentSps;
	VipPps *pps = g_currentPps;
	uint32_t status, word, needsLists, weights, marking, firstMb, row, column;

	g_decoderState = VIP_DECODER_BUSY;
	status = vipReadControlBus(VIP_CB_BITSTREAM_STATUS);
	if (!g_sliceHeaderRead && status) {
		g_errorCode = VIP_DECODER_ERROR_SLICE_HEADER;
		putRecord(g_recordType == VIP_RECORD_AVC_PICTURE ? VIP_PARSED_PICTURE_HEADER :
			VIP_PARSED_SLICE_HEADER, 0, 0);
		putRecord(pps->entropyCodingMode ? VIP_PARSED_CABAC_DATA :
			VIP_PARSED_CAVLC_DATA, 0, 0);
		sendRecords(failedRecordWritten);
		consumeRecord();
		return;
	}
	word = g_parseBuffer[0];
	needsLists = word >> VIP_SLICE_NEEDS_LISTS & 1;
	if (needsLists && !g_sliceHeaderRead) {
		acknowledgeReadStream();
		if (!g_readStream.half) {
			parseSliceHeader();
			return;
		}
		g_readStream.half = 0;
		g_readStream.cursor = g_recordPosition;
		g_loadNal(parseSliceHeader);
		return;
	}
	weights = word >> VIP_SLICE_HAS_WEIGHTS & 1;
	marking = word >> VIP_SLICE_HAS_MARKING & 1;
	g_slice.sliceType = word >> 12 & 0xf;
	g_slice.fieldPic = g_parseBuffer[1] >> 15 & 1;
	g_slice.numRefIdxActiveOverride = g_parseBuffer[6] >> 30 & 1;
	g_slice.numRefIdxL0ActiveMinus1 = g_parseBuffer[6] >> 25 & 0x1f;
	g_slice.numRefIdxL1ActiveMinus1 = g_parseBuffer[6] >> 20 & 0x1f;
	g_slice.cabacInitIdc = g_parseBuffer[7] >> 28 & 3;
	g_slice.qp = (pps->picInitQp + (g_parseBuffer[7] >> 21)) & 0x3f;
	firstMb = word >> 16 & 0x1fff;
	divide(firstMb, sps->picWidthInMbsMinus1 + 1, &row, &column);
	if (sps->mbAdaptiveFrameField && !g_slice.fieldPic)
		column <<= 1;
	vipWriteControlBus(VIP_CB_BITSTREAM_SLICE_START, (row << 8) + column);
	if (g_sliceHeaderRead) {
		sliceHeaderWritten();
		return;
	}
	g_parseBuffer[0] = (g_nalUnitType == 5) << 3 | (g_nalRefIdc != 0) << 2 | word;
	if (needsLists)
		g_parsedWords += VIP_SLICE_LIST_WORDS;
	if (weights)
		g_parsedWords += VIP_SLICE_WEIGHT_WORDS;
	if (marking)
		g_parsedWords += VIP_SLICE_MARKING_WORDS;
	writeParsed(VIP_PARSED_BUFFER, g_parsedWords * 4, sliceHeaderWritten);
}

/* Move the read stream past what the unit has taken of the SEI unit. */
static inline void advanceSeiStream(void)
{
	const VipRingSegment *segments = g_readStream.segments;
	uint32_t words = vipReadControlBus(VIP_CB_BITSTREAM_READ_WORDS);
	uint32_t taken = (words & 0xfe) * 4;
	uint32_t cursor = g_seiOffset + g_recordPosition + taken;

	if (cursor > segments[0].start && cursor - segments[0].start >= segments[0].size)
		cursor = segments[1].start + cursor - segments[0].start - segments[0].size;
	g_readStream.cursor = cursor;
	g_seiOffset += taken;
	vipWriteControlBus(VIP_CB_BITSTREAM_READ_WORDS, words & 1);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY0, 1);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY1, 1);
}

/* Skip what is left of the message; then the next one, or send the records. */
static void nextSeiMessage(void)
{
	uint32_t remaining;

	if (!g_seiSkipChecked) {
		remaining = g_nalLength * 8 - g_bitCount;
		g_bitCount = 0;
		if (remaining)
			readBits(remaining);
	} else {
		while (g_bitCount < g_nalLength * 8) {
			readBits((g_nalLength * 8 - g_bitCount) & 0x1f);
			if (vipReadControlBus(VIP_CB_BITSTREAM_STATUS))
				break;
		}
		g_bitCount = 0;
	}
	if (vipReadControlBus(VIP_CB_BITSTREAM_MORE_DATA)) {
		advanceSeiStream();
		g_loadNal(parseSei);
	} else {
		sendRecords(finishRecord);
	}
}

/* A message's records are in: send them when there are three, and go on. */
static inline void seiMessageWritten(uint32_t type, uint32_t size)
{
	putRecord(type, 0, size);
	if (g_recordCount >= SEI_RECORDS)
		sendRecords(nextSeiMessage);
	else
		nextSeiMessage();
}

/* buffering_period() */
static void parseBufferingPeriod(void);

static void bufferingPeriodWritten(void)
{
	seiMessageWritten(VIP_PARSED_BUFFERING_PERIOD, 0x208);
}

static void parseBufferingPeriod(void)
{
	uint32_t *buffer = g_parseBuffer, *word;
	uint32_t i;

	for (i = 0; i < 0x208 / 4; i++)
		buffer[i] = 0;
	buffer[0] = readGolomb(0) << 24;	/* seq_parameter_set_id */
	word = buffer + 1;
	if (g_currentSps->nalHrd.present) {
		for (i = 0; i <= g_currentSps->nalHrd.cpbCntMinus1; i++) {
			*word++ = readBits(g_currentSps->nalHrd.initialCpbRemovalDelayLengthMinus1 + 1);
			*word++ = readBits(g_currentSps->nalHrd.initialCpbRemovalDelayLengthMinus1 + 1);
		}
	}
	word = buffer + 65;
	if (g_currentSps->vclHrd.present) {
		for (i = 0; i <= g_currentSps->vclHrd.cpbCntMinus1; i++) {
			*word++ = readBits(g_currentSps->vclHrd.initialCpbRemovalDelayLengthMinus1 + 1);
			*word++ = readBits(g_currentSps->vclHrd.initialCpbRemovalDelayLengthMinus1 + 1);
		}
	}
	*word = 0;
	writeParsed((uint32_t)buffer, 0x208, bufferingPeriodWritten);
}

static void pictureTimingWritten(void)
{
	seiMessageWritten(VIP_PARSED_PICTURE_TIMING, 0x30);
}

/* pic_timing() */
static void parsePictureTiming(void)
{
	uint32_t *buffer = g_parseBuffer, *clock;
	uint32_t picStruct, clocks, flag, value, length, i;

	for (i = 0; i < 0x30 / 4; i++)
		buffer[i] = 0;
	if (g_currentSps->nalHrd.present) {
		buffer[0] = readBits(g_currentSps->nalHrd.cpbRemovalDelayLengthMinus1 + 1);
		buffer[1] = readBits(g_currentSps->nalHrd.dpbOutputDelayLengthMinus1 + 1);
	} else if (g_currentSps->vclHrd.present) {
		buffer[0] = readBits(g_currentSps->vclHrd.cpbRemovalDelayLengthMinus1 + 1);
		buffer[1] = readBits(g_currentSps->vclHrd.dpbOutputDelayLengthMinus1 + 1);
	}
	if (g_currentSps->picStructPresent) {
		picStruct = readBits(4);
		buffer[2] = picStruct << 28;
		if (picStruct < 3)
			clocks = 1;
		else if (picStruct < 5 || picStruct == 7)
			clocks = 2;
		else
			clocks = 3;
		for (i = 0; i < clocks; i++) {
			flag = readBits(1);	/* clock_timestamp_flag */
			buffer[2] |= flag << (27 - i);
			if (!flag)
				continue;
			clock = buffer + 3 + 3 * i;
			value = readBits(19);	/* ct_type ... n_frames */
			clock[0] = value << 13;
			if (value >> 10 & 1) {	/* full_timestamp_flag */
				clock[1] = readBits(17) << 15;
			} else {
				value = readBits(1);	/* seconds_flag */
				clock[0] |= value << 12;
				if (value) {
					value = readBits(7);
					clock[1] = (value >> 1) << 26;
					clock[0] |= (value & 1) << 11;
					if (value & 1) {	/* minutes_flag */
						value = readBits(7);
						clock[1] |= (value >> 1) << 20;
						clock[0] |= (value & 1) << 10;
						if (value & 1)	/* hours_flag */
							clock[1] |= readBits(5) << 15;
					}
				}
			}
			length = g_currentSps->nalHrd.timeOffsetLength;
			if (!length)
				length = g_currentSps->vclHrd.timeOffsetLength;
			if (length)
				clock[2] = readBits(length);
		}
	}
	writeParsed((uint32_t)buffer, 0x30, pictureTimingWritten);
}

static void panScanWritten(void)
{
	seiMessageWritten(VIP_PARSED_PAN_SCAN, 0x40);
}

/* pan_scan_rect() */
static void parsePanScan(void)
{
	uint32_t *buffer = g_parseBuffer;
	uint32_t cancel, count, i;

	for (i = 0; i < 0x40 / 4; i++)
		buffer[i] = 0;
	buffer[0] = readGolomb(0);		/* pan_scan_rect_id */
	cancel = readBits(1);
	buffer[1] = cancel << 31;
	if (!cancel) {
		count = readGolomb(0);		/* pan_scan_cnt_minus1 */
		if (count >= PAN_SCAN_RECTANGLES)
			count = PAN_SCAN_RECTANGLES - 1;
		buffer[1] |= count << 29;
		for (i = 0; i <= count; i++) {
			buffer[2 + 4 * i] = readGolomb(1);
			buffer[3 + 4 * i] = readGolomb(1);
			buffer[4 + 4 * i] = readGolomb(1);
			buffer[5 + 4 * i] = readGolomb(1);
		}
		buffer[14] = readGolomb(0);	/* pan_scan_rect_repetition_period */
	}
	buffer[15] = 0;
	writeParsed((uint32_t)buffer, 0x40, panScanWritten);
}

static void recoveryPointWritten(void)
{
	seiMessageWritten(VIP_PARSED_RECOVERY_POINT, 8);
}

/* recovery_point() */
static void parseRecoveryPoint(void)
{
	uint32_t *buffer = g_parseBuffer;

	buffer[0] = 0;
	buffer[1] = 0;
	buffer[0] = readGolomb(0) << 16;	/* recovery_frame_cnt */
	buffer[1] = 0;
	buffer[0] |= readBits(4) << 12;
	writeParsed((uint32_t)buffer, 8, recoveryPointWritten);
}

static void framePackingWritten(void)
{
	seiMessageWritten(VIP_PARSED_FRAME_PACKING, 0x10);
}

/* frame_packing_arrangement() */
static void parseFramePacking(void)
{
	uint32_t *buffer = g_parseBuffer;
	uint32_t cancel, fields, i;

	for (i = 0; i < 0x10 / 4; i++)
		buffer[i] = 0;
	buffer[0] = readGolomb(0);		/* frame_packing_arrangement_id */
	cancel = readBits(1);
	buffer[1] = cancel << 31;
	if (!cancel) {
		/* frame_packing_arrangement_type to frame1_self_contained_flag */
		fields = readBits(20);
		buffer[1] |= fields << 11;
		/* !quincunx_sampling_flag, and not the type of temporal interleaving */
		if (!(fields >> 12 & 1) && fields >> 13 != 5)
			buffer[2] |= readBits(16) << 16;	/* the grid positions */
		buffer[1] |= readBits(8) << 4;		/* frame_packing_arrangement_reserved_byte */
		buffer[2] |= (readGolomb(0) & 0x7fff) << 1;	/* repetition_period */
	}
	buffer[2] |= readBits(1);		/* frame_packing_arrangement_extension_flag */
	writeParsed((uint32_t)buffer, 0x10, framePackingWritten);
}

/*
 * Copy the payload out as it is, 0x200 bytes at a time; its last piece is padded
 * to whole 8-byte units.
 */
static void copySeiPayload(void)
{
	uint32_t *buffer = g_parseBuffer;
	uint32_t remaining, length, pad, i;

	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_RAW);
	for (i = 0; i < SEI_COPY_WORDS; i++) {
		remaining = g_seiRemaining;
		if (remaining < 4) {
			vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_NAL);
			if (remaining)
				*buffer++ = readBits(remaining * 8) << ((4 - remaining) * 8);
			length = g_nalLength & 0x1ff;
			if (g_nalLength & 7) {
				for (pad = 0; pad < (length & 7); pad++)
					*buffer++ = 0;
				length += 8 - (length & 7);
			}
			writeParsed((uint32_t)g_parseBuffer, length, seiPayloadCopied);
			return;
		}
		*buffer++ = readBits(0);
		g_seiRemaining = remaining - 4;
	}
	advanceSeiStream();
	g_loadNal(NULL);
	writeParsed((uint32_t)g_parseBuffer, SEI_COPY_WORDS * 4, copySeiPayload);
}

static void seiPayloadCopied(void)
{
	setRecordSize(g_nalLength);
	g_recordCount++;
	if (g_nalLength & 7)
		putRecord(VIP_PARSED_PADDING, 0, 8 - (g_nalLength & 7));
	if (g_recordCount >= SEI_RECORDS)
		sendRecords(nextSeiMessage);
	else
		nextSeiMessage();
}

/* a payload copied as it is, with a record of the given kind */
static inline void copySei(uint32_t kind, uint32_t type)
{
	addRecord(kind, type << 4);
	g_seiRemaining = g_nalLength;
	copySeiPayload();
}

/* An SEI parse that ran away: send what there is and stop. */
static inline void seiFailed(uint32_t error)
{
	g_decoderState = VIP_DECODER_FAILED;
	g_errorCode = error;
	putRecord(VIP_PARSED_SEI, 0xff, 0);
	sendRecords(finishRecord);
}

/* sei_message(): its type and size, and its payload by type */
static void parseSei(void)
{
	uint32_t type = 0, byte;

	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_RAW);
	g_nalLength = 0;
	g_bitCount = 0;
	while ((byte = readBits(8)) == 0xff) {
		if (g_bitCount > SEI_MAX_BITS) {
			seiFailed(VIP_DECODER_ERROR_SEI_TYPE);
			goto out;
		}
		type += 0xff;
	}
	type += byte;
	g_bitCount = 0;
	while ((byte = readBits(8)) == 0xff) {
		if (g_bitCount > SEI_MAX_BITS) {
			seiFailed(VIP_DECODER_ERROR_SEI_SIZE);
			goto out;
		}
		g_nalLength += 0xff;
	}
	g_nalLength += byte;
	g_bitCount = 0;
	switch (type) {
	case 0:
		parseBufferingPeriod();
		break;
	case 1:
		parsePictureTiming();
		break;
	case 2:
		parsePanScan();
		break;
	case 4:
		copySei(VIP_PARSED_USER_DATA, type);
		break;
	case 5:
		copySei(VIP_PARSED_USER_DATA_UUID, type);
		break;
	case 6:
		parseRecoveryPoint();
		break;
	case SEI_FRAME_PACKING:
		g_seiRemaining = g_nalLength;
		parseFramePacking();
		break;
	default:
		if (type <= SEI_LAST_COPIED) {
			copySei(VIP_PARSED_SEI, type);
		} else {
			putRecord(VIP_PARSED_SEI, type << 4, 0);
			sendRecords(finishRecord);
		}
		break;
	}
out:
	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_NAL);
}

/*
 * A slice's CAVLC data is out: its record, and one for the padding of its last
 * word.
 */
static void cavlcWritten(void)
{
	putRecord(VIP_PARSED_CAVLC_DATA, 0, g_writeStream.half * VIP_STREAM_HALF +
		(g_parsedWords - g_padded) * 4 - 4 + g_lastBytes);
	if (g_padded || g_lastBits)
		putRecord(VIP_PARSED_PADDING,
			(g_lastBits & 7) << VIP_PADDING_BITS_SHIFT & 0xffff,
			g_padded * 4 - g_lastBytes + 4);
	sendRecords(finishRecord);
}

/* the write stream's half with the slice data's end; its words */
static inline uint32_t *takeSliceEnd(void)
{
	uint32_t written = vipReadControlBus(VIP_CB_BITSTREAM_WRITE_WORDS);
	uint32_t *buffer = (uint32_t *)(written & VIP_BITSTREAM_HALF1 ?
		VIP_SLICE_BUFFER + VIP_STREAM_HALF : VIP_SLICE_BUFFER);

	g_parsedWords = written & VIP_BITSTREAM_WORD_MASK;
	return buffer;
}

/* The unit has decoded CAVLC slice data: write out the rest of it. */
static VIP_DRAM void cavlcStopped(void)
{
	uint32_t *buffer;

	g_decoderState = VIP_DECODER_BUSY;
	g_lastBits = vipReadControlBus(VIP_CB_BITSTREAM_STATUS) & VIP_BITSTREAM_LAST_BITS;
	if (!g_lastBits)
		g_lastBytes = 4;
	else
		g_lastBytes = g_lastBits >> 3;
	buffer = takeSliceEnd();
	if (!(g_parsedWords & 1)) {
		buffer[++g_parsedWords] = 0;
		g_padded = 1;
	}
	g_parsedWords++;
	writeParsed((uint32_t)buffer, g_parsedWords * 4, cavlcWritten);
}

/* CABAC: set the unit up for the slice and start it (after the state is cleared). */
static void startCabac(void)
{
	const VipSps *sps;
	const VipPps *pps;
	uint32_t sliceType, structure, list0, list1;

	sliceType = g_slice.sliceType;
	if (sliceType >= 5)
		sliceType -= 5;
	vipWriteControlBus(VIP_CB_BITSTREAM_CABAC_SETUP,
		sliceType << 8 | g_slice.cabacInitIdc << 6 | g_slice.qp);
	runBitstream(VIP_BITSTREAM_CABAC_INIT);
	sps = g_currentSps;
	vipWriteControlBus(VIP_CB_BITSTREAM_PICTURE_WIDTH, sps->picWidthInMbsMinus1);
	if (sps->levelIdc >= 31)
		vipWriteControlBus(VIP_CB_BITSTREAM_SMALL_PICTURE, sps->picWidthInMbsMinus1 < 45);
	else
		vipWriteControlBus(VIP_CB_BITSTREAM_SMALL_PICTURE, sps->picWidthInMbsMinus1 < 76);
	/* 0: frame, 1: field, 2: MBAFF frame */
	if (sps->frameMbsOnly)
		structure = 0;
	else if (sps->mbAdaptiveFrameField && !g_slice.fieldPic)
		structure = 2;
	else
		structure = g_slice.fieldPic != 0;
	pps = g_currentPps;
	if (g_slice.numRefIdxActiveOverride == 1) {
		list0 = g_slice.numRefIdxL0ActiveMinus1 != 0;
		list1 = g_slice.numRefIdxL1ActiveMinus1 != 0;
	} else {
		list0 = pps->numRefIdxL0DefaultActiveMinus1 != 0;
		list1 = pps->numRefIdxL1DefaultActiveMinus1 != 0;
	}
	vipWriteControlBus(VIP_CB_BITSTREAM_SLICE_SETUP, list1 << 2 | list0 << 3 |
		sliceType << 4 | structure << 6 | sps->chroma << 8 |
		sps->direct8x8Inference << 1 | pps->constrainedIntraPred);
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_SLICE_DATA);
}

static void continueSliceData(void)
{
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_CONTINUE);
}

/* Have the unit write its CABAC state for the next part out, then go on. */
static void flushSliceData(void)
{
	uint32_t size;

	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_FLUSH);
	size = g_currentSps->chroma ? CABAC_ROW_SIZE_CHROMA : CABAC_ROW_SIZE;
	waitBitstream();
	writeParsed(VIP_SLICE_BUFFER, size, continueSliceData);
}

/*
 * A part of a slice's CABAC data is out: the unit goes on with the slice (status
 * 1), or the slice is done.
 */
static void cabacWritten(void)
{
	uint32_t status, padding;

	status = vipReadControlBus(VIP_CB_BITSTREAM_STATUS) >> VIP_BITSTREAM_CABAC_SHIFT;
	if (status == 1) {
		g_cabacWritten += g_writeStream.half * VIP_STREAM_HALF + g_parsedWords * 4;
		g_writeStream.half = 0;
		g_cabacWritten += g_currentSps->chroma ? CABAC_ROW_SIZE_CHROMA : CABAC_ROW_SIZE;
		flushSliceData();
	} else if (status == 0 || status == 2 || status == 3) {
		g_decoderState = VIP_DECODER_BUSY;
		putRecord(VIP_PARSED_CABAC_DATA, 0, g_cabacWritten +
			g_writeStream.half * VIP_STREAM_HALF +
			(g_parsedWords - g_padded) * 4 - 4 + g_lastBytes);
		padding = g_padded * 4 - g_lastBytes + 4;
		if (padding)
			putRecord(VIP_PARSED_PADDING, 0, padding);
		sendRecords(finishRecord);
	}
}

/* The unit stopped in CABAC slice data: write out what it has. */
static VIP_DRAM void cabacStopped(void)
{
	uint32_t status = vipReadControlBus(VIP_CB_BITSTREAM_STATUS);
	uint32_t *buffer;

	if (status & VIP_BITSTREAM_DATA_ERROR)
		g_errorCode = VIP_DECODER_ERROR_SLICE_DATA;
	g_lastBits = status & VIP_BITSTREAM_LAST_BITS;
	if (!g_lastBits)
		g_lastBytes = 4;
	else
		g_lastBytes = (g_lastBits >> 3) + ((status & 7) != 0);
	buffer = takeSliceEnd();
	if (g_parsedWords & 1) {
		g_padded = 0;
	} else {
		buffer[++g_parsedWords] = 0;
		g_padded = 1;
	}
	g_parsedWords++;
	writeParsed((uint32_t)buffer, g_parsedWords * 4, cabacWritten);
}

/* Start the unit on the slice data: CABAC once its state is cleared by DMA. */
static void startSliceData(void)
{
	if (g_currentPps->entropyCodingMode) {
		g_decoderState = VIP_DECODER_CABAC;
		clearCabacState();
		return;
	}
	g_decoderState = VIP_DECODER_CAVLC;
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_DECODE);
}

static void addStreamEndRecord(void)
{
	putRecord(VIP_PARSED_STREAM_END, 0, 0);
}

/* A unit with no data to parse: have the unit finish what it has written. */
static void endStream(void)
{
	g_decoderState = VIP_DECODER_ENDING;
	g_writeEnded = 1;
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_DECODE);
}

static VIP_DRAM void streamEnded(void)
{
	g_writeEnded = 0;
	g_decoderState = VIP_DECODER_BUSY;
	setRecordSize(0);
	g_recordCount++;
	sendRecords(finishRecord);
}

static void resetParameterSets(void)
{
	uint32_t i;

	for (i = 0; i < VIP_SPS_COUNT; i++)
		g_parser.sps[i].valid = 0;
	for (i = 0; i < VIP_PPS_COUNT; i++)
		g_parser.pps[i].valid = 0;
}

/* a record of its own, with no data, for the codec */
static inline void passRecord(uint32_t type)
{
	putRecord(type, 0, 0);
	sendRecords(finishRecord);
}

/* The unit is in the read stream and started: go on by its type. */
static void parseRecord(void)
{
	switch (g_recordType) {
	case VIP_RECORD_AVC_SLICE:
	case VIP_RECORD_AVC_PICTURE:
		if (startSlice() < 0)
			consumeRecord();
		break;
	case VIP_RECORD_AVC_PPS:
		vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_PPS);
		g_decoderState = VIP_DECODER_PPS;
		break;
	case VIP_RECORD_AVC_SPS:
		g_decoderState = VIP_DECODER_SPS;
		vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_SPS);
		break;
	case VIP_RECORD_AVC_SEI:
		if (!g_currentSps || !g_currentSps->valid) {
			consumeRecord();
			break;
		}
		g_seiOffset = 0;
		parseSei();
		break;
	case VIP_RECORD_AVC_DELIMITER:
		parseDelimiter();
		break;
	case VIP_RECORD_AVC_OTHER:
		addRecord(VIP_PARSED_OTHER, 0);
		endStream();
		break;
	case VIP_RECORD_AVC_END_SEQUENCE:
		passRecord(VIP_PARSED_END_SEQUENCE);
		break;
	case VIP_RECORD_AVC_END_STREAM:
		passRecord(VIP_PARSED_END_STREAM);
		break;
	case VIP_RECORD_PICTURE_END:
		passRecord(VIP_PARSED_PICTURE_END);
		break;
	case VIP_RECORD_STREAM_END:
		addStreamEndRecord();
		if (!sendRecords(finishRecord))
			resetParameterSets();
		break;
	default:			/* VIP_RECORD_AVC_ZEROS among them */
		takeRecord();
		g_decoderState = VIP_DECODER_IDLE;
		break;
	}
}

/* The unit is in the read stream (g_loadNal's callback). */
void startRecord(void)
{
	startNal();
	parseRecord();
}

void initializeDecoder(void)
{
	g_decoderState = VIP_DECODER_IDLE;
	g_decoderOpen = 0;
	setupInterrupts();
	g_writeEnded = 0;
	g_nextSavedPts = 0;
	g_nextSavedDts = 0;
	g_currentSps = NULL;
	g_currentPps = NULL;
	g_nextSavedFlags = 0;
}

/* Stop the bitstream unit and the streams, and drop their interrupts. */
void stopDecoder(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW), request;

	mep_di();
	request = vipReadControlBus(VIP_CB_CLOCK_REQUEST);
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST, request | VIP_CLOCK_REQUEST_DECODER);
	vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 0);
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 0);
	vipWriteControlBus(VIP_CB_INTERRUPT_CLEAR, ~VIP_PARSER_INTERRUPTS);
	vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 1);
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 1);
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST, request & ~VIP_CLOCK_REQUEST_DECODER);
	settle();
	if (psw & VIP_PSW_IEC)
		mep_ei();
	g_decoderState = VIP_DECODER_IDLE;
}

/*
 * Open the parser on its rings: the input elementary stream and its records,
 * read through the given cursors, and the parsed stream and its records, which
 * it writes. -1 if a ring does not open (the handles are cleared).
 */
int openDecoder(VipRing *input, int32_t inputCursor, VipRing *records,
	int32_t recordsCursor, VipRing *output, VipRing *outputRecords)
{
	int result;

	g_inputRing.ring = input;
	g_inputRing.cursor = inputCursor;
	g_recordRing.ring = records;
	g_recordRing.cursor = recordsCursor;
	g_outputRing.ring = output;
	g_outputRing.cursor = VIP_RING_WRITER;
	g_outputRecordRing.ring = outputRecords;
	g_outputRecordRing.cursor = VIP_RING_WRITER;
	result = openRing(&g_inputRing, VIP_RING_OPEN_READER);
	result |= openRing(&g_recordRing, VIP_RING_OPEN_READER);
	result |= openRing(&g_outputRing, VIP_RING_OPEN_WRITER);
	result |= openRing(&g_outputRecordRing, VIP_RING_OPEN_WRITER);
	if (result & 1) {
		g_inputRing.ring = NULL;
		g_inputRing.cursor = 0;
		g_recordRing.ring = NULL;
		g_recordRing.cursor = 0;
		g_outputRing.ring = NULL;
		g_outputRing.cursor = 0;
		g_outputRecordRing.ring = NULL;
		g_outputRecordRing.cursor = 0;
		return -1;
	}
	g_pictures = 0;
	g_errorCode = 0;
	g_starved = 0;
	g_overflows = 0;
	g_savedPts = 0;
	g_savedDts = 0;
	g_pictureStarted = 0;
	g_savedFlags = 0;
	g_nextTimestampState = 0;
	g_timestampState = 0;
	resetParameterSets();
	g_decoderOpen = 1;
	return 0;
}

/*
 * Queue a request with its parameters and callback, without waiting: it is
 * dropped when the queue is full.
 */
static void queueRequest(uint32_t control, uint32_t external, uint32_t localLength,
	uint32_t parameter0, uint32_t parameter1, void (*callback)(void))
{
	VipDmaRequest *request;
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	request = &g_dmaQueue.requests[g_dmaQueue.tail];
	if (g_dmaQueue.count < 2) {
		vipWriteControlBus(VIP_CB_DMA_DIRECTION, control);
		vipWriteControlBus(VIP_CB_DMA_EXTERNAL, external);
		vipWriteControlBus(VIP_CB_DMA_LOCAL_LENGTH, localLength);
		vipWriteControlBus(VIP_CB_DMA_PARAMETER0, parameter0);
		vipWriteControlBus(VIP_CB_DMA_PARAMETER1, parameter1);
		if (!g_dmaQueue.count)
			vipWriteControlBus(VIP_CB_DMA_BUSY, 1);
	} else if (g_dmaQueue.count == VIP_DMA_QUEUE_LENGTH) {
		if (psw & VIP_PSW_IEC)
			mep_ei();
		return;
	} else {
		request->control = control;
		request->external = external;
		request->localLength = localLength;
		request->parameters[0] = parameter0;
		request->parameters[1] = parameter1;
	}
	request->callback = callback;
	request->done = NULL;
	g_dmaQueue.count++;
	g_dmaQueue.tail = (g_dmaQueue.tail + 1) & (VIP_DMA_QUEUE_LENGTH - 1);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

/* Clear the unit's CABAC state by DMA, then start CABAC slice data. */
static void clearCabacState(void)
{
	g_dmaSource.clear.descriptor.external = (uint32_t)g_dmaSource.clear.zeros;
	g_dmaSource.clear.zeros[0] = 0;
	g_dmaSource.clear.descriptor.localLength = CABAC_STATE << 16 | CABAC_STATE_SIZE;
	g_dmaSource.clear.zeros[1] = 0;
	queueRequest(VIP_DMA_LIST | VIP_DMA_FILL | VIP_DMA_EXTERNAL_LOCAL, 0,
		(uint32_t)&g_dmaSource.clear.descriptor << 16 | sizeof(VipDmaDescriptor),
		sizeof(g_dmaSource.clear.zeros), 0, startCabac);
}

static void outputRecordWritten(void)
{
	while (advanceRingWriter(&g_outputRecordRing, sizeof(VipParsedRecord)))
		;
}

/* Forget the stream: its input, its timestamps and its parameter sets. */
static void dropStream(void)
{
	while (ringSkipToWriter(&g_recordRing))
		;
	while (ringSkipToWriter(&g_inputRing))
		;
	g_savedPts = 0;
	g_nextSavedPts = 0;
	g_savedDts = 0;
	g_nextSavedDts = 0;
	g_timestampState = 0;
	g_nextSavedFlags = 0;
	g_savedFlags = 0;
	g_nextTimestampState = 0;
	resetParameterSets();
	g_pictureStarted = 0;
}

/* the records with no NAL unit to load */
static inline int isMarker(int32_t type)
{
	return type == VIP_RECORD_PICTURE_END || type == VIP_RECORD_STREAM_END ||
		type == VIP_RECORD_AVC_START || type == VIP_RECORD_AVC_END_SEQUENCE ||
		type == VIP_RECORD_AVC_END_STREAM;
}

static inline void saveTimestamps(void)
{
	g_nextSavedPts = g_pts;
	g_nextSavedDts = g_dts;
	g_nextSavedFlags = g_recordFlags;
	g_nextTimestampState = 1;
}

static inline void clearTimestamps(void)
{
	g_pts = 0;
	g_dts = 0;
	g_recordFlags = 0;
}

/*
 * The timestamps an access unit delimiter brings go with the units after it, and
 * only the first time the same ones come.
 */
static inline void applyTimestamps(void)
{
	int same = g_pts == g_savedPts && g_dts == g_savedDts && g_recordFlags == g_savedFlags;

	switch (g_timestampState) {
	case 0:
		if (g_recordType == VIP_RECORD_AVC_DELIMITER)
			saveTimestamps();
		break;
	case 1:
		if (g_recordType == VIP_RECORD_AVC_DELIMITER) {
			if (same) {
				clearTimestamps();
				g_nextTimestampState = 2;
			} else {
				g_nextSavedDts = g_dts;
				g_nextSavedPts = g_pts;
				g_nextSavedFlags = g_recordFlags;
			}
		} else if (!same) {
			g_pts = g_savedPts;
			g_dts = g_savedDts;
			g_recordFlags = g_savedFlags;
		}
		break;
	case 2:
		if (g_recordType == VIP_RECORD_AVC_DELIMITER && !same)
			saveTimestamps();
		else
			clearTimestamps();
		break;
	}
}

/* An output ring is full: hold its writer until the codec makes room. */
static inline void holdOutput(VipRingHandle *ring)
{
	g_overflows++;
	setRingWriterState(ring, 1);
	g_decoderState = VIP_DECODER_HELD;
	gateDecoderClocks();
}

/*
 * No whole record: if the input is nearly full, it is stuck, so drop it and give
 * the codec an empty record; else count a starved call.
 */
static inline void waitForRecords(void)
{
	int space = getRingSpaceBeforeReader(&g_inputRing);

	g_decoderState = VIP_DECODER_IDLE;
	gateDecoderClocks();
	if (!g_keepInput && space >= 1 && space < INPUT_STUCK) {
		while (ringSkipToWriter(&g_recordRing))
			;
		while (ringSkipToWriter(&g_inputRing))
			;
		if (getRingSpaceBeforeFirstReader(&g_outputRecordRing) <
		    (int)sizeof(VipParsedRecord) + 1)
			return;
		g_dmaSource.record.header = VIP_PARSED_SPS << VIP_RECORD_TYPE_SHIFT;
		g_dmaSource.record.position = getRingWriterPosition(&g_outputRing);
		g_dmaSource.record.pts = 0;
		g_dmaSource.record.dts = 0;
		queueRequest(VIP_DMA_WRITE, getRingWriterPosition(&g_outputRecordRing),
			(uint32_t)&g_dmaSource.record << 16 | sizeof(VipParsedRecord), 0, 0,
			outputRecordWritten);
		return;
	}
	if (g_pictures)
		g_starved++;
}

/*
 * Take the next record and its NAL unit (and the next record's position, where
 * the unit ends), and start on it once the output has room. 1: the input
 * restarted, 2: the parser is closed.
 */
int decodeRecords(void)
{
	VipRingSegment segments[2];
	const VipRecord *record, *next;
	uint32_t total, position, length, needed, i;
	int count, space, empty;

	if (!g_decoderOpen)
		return 2;
	for (;;) {
		if (getRingReaderState0(&g_outputRecordRing)) {
			g_decoderState = VIP_DECODER_IDLE;
			return 0;
		}
		/*
		 * The restart handshake, through the rings' reader state 0: when the
		 * input restarted, drop the stream and pass the restart on.
		 */
		if (getRingReaderState0(&g_recordRing)) {
			dropStream();
			setRingReaderState0(&g_outputRecordRing, 1);
			setRingReaderState0(&g_recordRing, 0);
			g_decoderState = VIP_DECODER_IDLE;
			return 1;
		}
		enableDecoderClocks();
		resetBitstream();
		g_decoderState = VIP_DECODER_BUSY;
		count = getRingReadable(&g_recordRing, segments, 2);
		total = 0;
		for (i = 0; (int)i < count; i++)
			total += segments[i].size;
		record = (const VipRecord *)segments[0].start;
		if (!total || (total == sizeof(VipRecord) &&
			       !isMarker((int32_t)record->header >> VIP_RECORD_TYPE_SHIFT))) {
			waitForRecords();
			return 0;
		}

		if (segments[0].size == sizeof(VipRecord) && count == 2)
			next = (const VipRecord *)segments[1].start;
		else
			next = record + 1;
		g_nextPosition = next->position;
		g_writeEnded = 0;
		g_recordCount = 0;
		g_writeStream.segmentCount = getRingWritableBeforeFirstReader(&g_outputRing,
			g_writeStream.segments, 2);
		g_writeStream.cursor = g_writeStream.segments[0].start;
		g_outputCursor = g_writeStream.segments[0].start;
		getRingWritableBeforeFirstReader(&g_outputRecordRing, segments, 2);
		g_outputRecordCursor = segments[0].start;
		g_readStream.segmentCount = getRingReadable(&g_inputRing,
			g_readStream.segments, 2);
		g_recordCount = 0;
		g_recordTotal = 0;
		g_recordFlags = record->header >> VIP_RECORD_FLAGS_SHIFT & VIP_RECORD_FLAGS_MASK;
		g_recordType = (int32_t)record->header >> VIP_RECORD_TYPE_SHIFT;
		g_dts = record->dts;
		g_pts = record->pts;
		position = record->position;
		g_readStream.cursor = position;
		g_recordPosition = position;
		applyTimestamps();

		if (isMarker(g_recordType)) {
			g_nextPosition = position;
			startRecord();
			return 0;
		}
		g_nalLength = getRingDistance(&g_inputRing, position & ~7u, g_nextPosition);
		length = getRingDistance(&g_inputRing, position, g_nextPosition);
		if (getRingAvailable(&g_inputRing) < (int)length) {
			g_decoderState = VIP_DECODER_IDLE;
			gateDecoderClocks();
			return 0;
		}
		g_misalignment = position & 7;
		g_writeStream.half = 0;
		g_readStream.half = 0;
		g_cabacWritten = 0;
		if (g_recordType == VIP_RECORD_AVC_ZEROS) {
			startRecord();
			continue;
		}

		if (getRingSpaceBeforeFirstReader(&g_outputRecordRing) < SPACE_RECORDS) {
			holdOutput(&g_outputRecordRing);
			return 0;
		}
		space = getRingSpaceBeforeFirstReader(&g_outputRing);
		empty = isRingSpaceEmpty(&g_outputRing, space);
		switch (g_recordType) {
		case VIP_RECORD_AVC_SLICE:
		case VIP_RECORD_AVC_PICTURE:
			if ((uint32_t)space < g_nalLength + (g_nalLength >> 1)) {
				if (!empty) {
					holdOutput(&g_outputRing);
				} else {
					/* it will never fit */
					dropStream();
					g_decoderState = VIP_DECODER_IDLE;
					gateDecoderClocks();
				}
				return 0;
			}
			needed = 0;
			break;
		case VIP_RECORD_AVC_PPS:
			needed = SPACE_PPS;
			break;
		case VIP_RECORD_AVC_SPS:
			needed = SPACE_SPS;
			break;
		case VIP_RECORD_AVC_DELIMITER:
		case VIP_RECORD_AVC_OTHER:
		case VIP_RECORD_AVC_END_SEQUENCE:
		case VIP_RECORD_AVC_END_STREAM:
		case VIP_RECORD_AVC_START:
		case VIP_RECORD_STREAM_END:
		case VIP_RECORD_PICTURE_END:
			needed = SPACE_MARKER;
			break;
		default:
			needed = SPACE_OTHER;
			break;
		}
		if ((uint32_t)space < needed) {
			holdOutput(&g_outputRing);
			return 0;
		}
		g_loadNal(startRecord);
		return 0;
	}
}

/* the bitstream unit's clock, which the parser asks for while it works */
static void enableDecoderClocks(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST,
		vipReadControlBus(VIP_CB_CLOCK_REQUEST) | VIP_CLOCK_REQUEST_DECODER);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

static void gateDecoderClocks(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST,
		vipReadControlBus(VIP_CB_CLOCK_REQUEST) & ~VIP_CLOCK_REQUEST_DECODER);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

/* The write stream reached the end of its last segment: hold the output. */
void holdWriteStream(void)
{
	resetBitstream();
	setRingWriterState(&g_outputRing, 1);
	g_decoderState = VIP_DECODER_HELD;
}
