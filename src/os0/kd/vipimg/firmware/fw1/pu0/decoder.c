/*
 * PU0 task 2: the decoder's front end (decoder.h), and the hooks the scheduler and
 * the debugger call for PU0's hardware.
 *
 * decodeRecords takes the next record and its NAL unit; startRecord, once the unit
 * is in the read stream, sends it on by type. A parse ends in a callback that adds
 * the unit's parsed records (addRecord, setRecordSize) and sends them
 * (flushRecords); finishRecord then takes the record from the input and goes on
 * with the next. Most steps run from the DMA-complete or the decoder interrupt,
 * and let higher interrupts in while they run (beginNesting).
 */
#include "decoder.h"
#include "interrupt.h"

/* how long to wait for a bitstream command */
#define BITSTREAM_POLLS		2000

/* the parameter sets' parse results: optional tables after the fixed words */
#define SPS_WORDS		7
#define PPS_WORDS		2
#define SCALING_LIST_WORDS	0x39
#define REF_FRAME_OFFSET_WORDS	0xff

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

/* with less input space than this and no record, the demultiplexer is stuck */
#define INPUT_STUCK		0x280

/* parsed records go out three at a time while an SEI unit is parsed */
#define SEI_RECORDS		3
#define SEI_MAX_BITS		0x1f40
#define SEI_COPY_WORDS		0x80

/* the commands of debug point 1 */
#define COMMAND_NONE		0
#define COMMAND_OPEN		1
#define COMMAND_CLOSE		2

/* their results, and what the command task's state is */
#define RESULT_DONE		1
#define RESULT_BAD_ACTION	3
#define RESULT_BAD_COUNT	4
#define RESULT_BAD_STATE	5
#define COMMAND_OPEN_COUNT	0x18
#define COMMAND_CLOSE_COUNT	8

#define DECODER_CLOSED		0
#define DECODER_OPENING		1
#define DECODER_OPEN		2
#define DECODER_CLOSING		3

/* the debugger's event: PU1 finished a frame */
#define EVENT_PU1_FRAME		0x8000000

/* how long the reset waits for the DMA to stop */
#define RESET_POLLS		0x8000000

/* the local RAM data vipSaveHardware keeps (fw1-pu0.ld checks the section fits) */
#define LOCAL_DATA_WORDS	0x90

#define COPY_LIST_LENGTH	8

/* the interrupt state an interrupt handler changes to let higher ones in */
typedef struct VipNesting {
	uint32_t psw;
	uint32_t enable;
	uint32_t epc;
} VipNesting;

/* what vipSaveHardware keeps for vipRestoreHardware */
typedef struct VipHardwareState {
	uint32_t interruptStatus;
	uint32_t interruptEnable;
	uint32_t interruptPending;
	uint32_t interruptLevel[4];
	uint32_t dmaControl;
	uint32_t pauseFlags;
	uint32_t localData[LOCAL_DATA_WORDS];
	uint8_t bank1[VIP_BANK1_SAVED];
} VipHardwareState;

volatile uint32_t g_decoderState;

/* the bitstream unit's reads and writes, kept in local RAM with the streams */
static uint16_t g_bitCount VIP_LRAM_DATA;	/* bits read since it was cleared */
static uint16_t g_parsedWords VIP_LRAM_DATA;	/* the parse results to write */
static uint8_t g_misalignment VIP_LRAM_DATA;	/* the unit's offset in its first 8 bytes */

/*
 * The DMA lists that copy a NAL unit in and the parse results out. The read list
 * also carries a lone record out, and the clearing of the CABAC state.
 */
static union {
	VipDmaDescriptor list[COPY_LIST_LENGTH];
	VipParsedRecord record;
	struct {
		VipDmaDescriptor descriptor;
		uint32_t zeros[2];
	} clear;
} g_readList VIP_LRAM_DATA;
static VipDmaDescriptor g_writeList[COPY_LIST_LENGTH] VIP_LRAM_DATA;

/* the rings, by channel: input from task 1, output to PU1 */
static VipRingHandle *g_inputRing[VIP_DECODER_CHANNELS];
static VipRingHandle *g_outputRing[VIP_DECODER_CHANNELS];
static VipRingHandle *g_recordRing[VIP_DECODER_CHANNELS];
static VipRingHandle *g_outputRecordRing[VIP_DECODER_CHANNELS];

static uint32_t g_outputRecordCursor;	/* where flushRecords writes */
static uint32_t g_nextRecord;		/* the record ring position of g_records[1] */
static uint32_t g_outputCursor;		/* where the next parsed record's data starts */
static VipSliceInfo g_slice;
static uint8_t g_nalRefIdc;
static uint8_t g_nalUnitType;
static VipSps *g_currentSps;
static VipPps *g_currentPps;
static uint32_t g_nalLength;		/* from 8 bytes before; an SEI payload's size */
static uint32_t g_nextPosition;		/* the next record's input position */
static uint32_t g_cabacWritten;		/* slice data written before the last flush */
static uint32_t g_pts;			/* the unit's timestamps, as its records get them */
static uint32_t g_dts;
static uint32_t g_savedPts[VIP_DECODER_CHANNELS];	/* the last access unit's */
static uint32_t g_nextSavedPts;
static uint32_t g_savedDts[VIP_DECODER_CHANNELS];
static uint32_t g_nextSavedDts;
static uint32_t g_seiRemaining;		/* bytes of the payload copySeiPayload has left */
static uint32_t g_seiOffset;		/* how far the SEI unit's messages have moved */
static uint32_t g_pictureSetup;		/* VIP_CB_BITSTREAM_PICTURE_SETUP of the picture */
static uint16_t g_recordType;
static uint16_t g_recordCount;		/* parsed records not yet sent */
static int16_t g_recordTotal;		/* parsed records of the unit */
static int8_t g_padded;			/* the parse results end in a zero word */
static uint8_t g_lastBytes;		/* bytes used of slice data's last word */
static uint8_t g_lastBits;
/*
 * The timestamp state of a channel: 0 before an access unit delimiter, 1 after
 * one, 2 once one has come with the same timestamps (they are then left out).
 * The "next" values become the channel's when the unit is finished.
 */
static int8_t g_nextTimestampState;
static uint8_t g_timestampState[VIP_DECODER_CHANNELS];
static uint8_t g_recordFlags;		/* VIP_RECORD_PTS... of the unit */
static uint8_t g_savedFlags[VIP_DECODER_CHANNELS];
static int8_t g_nextSavedFlags;
static int8_t g_pictureStarted[VIP_DECODER_CHANNELS];	/* its header records are out */
static int8_t g_sliceHeaderRead;	/* the reference lists have been read */
static uint8_t g_channel;
static int8_t g_channelOpen[VIP_DECODER_CHANNELS];
static uint32_t g_errorCode[VIP_DECODER_CHANNELS];	/* VIP_DECODER_ERROR_* */
static uint32_t g_starved[VIP_DECODER_CHANNELS];	/* frames with no input */
static uint32_t g_overflows[VIP_DECODER_CHANNELS];	/* units held for output space */
static uint32_t g_pictures[VIP_DECODER_CHANNELS];

static VipSps g_sps[VIP_DECODER_CHANNELS][VIP_SPS_COUNT];
static VipPps g_pps[VIP_DECODER_CHANNELS][VIP_PPS_COUNT];

static uint32_t *const g_parseBuffer = (uint32_t *)VIP_PARSED_BUFFER;

static VipDemuxRecord g_records[2];	/* the unit's record and the next */
static VipParsedRecord g_parsed[VIP_PARSED_RECORDS];
static VipRingHandle g_rings[VIP_DECODER_RINGS];	/* debug point 0's */
static VipHardwareState g_savedHardware;

static int decodeRecords(void);
static void finishRecord(void);
static void parseVui(void);
static void parseVuiVclHrd(void);
static void parseVuiEnd(void);
static void vuiWritten(void);
static void parseSliceHeader(void);
static void resumeSliceHeader(void);
static void sliceHeaderWritten(void);
static void sliceRecordsWritten(void);
static void nextSeiMessage(void);
static void parseSei(void);
static void copySeiPayload(void);
static void seiPayloadCopied(void);
static void startSliceData(void);
static void startCabac(void);
static void continueSliceData(void);
static void cavlcWritten(void);
static void cabacWritten(void);
static void startRecord(void);
static void loadNal(void (*callback)(void));
static void resetParameterSets(uint32_t channel);
static void stopDecoder(void);
static void outputRecordWritten(void);
static uint32_t decoderBusy(void);

/* let control register writes take effect */
static inline void settle(void)
{
	__asm__ volatile ("nop\n\tnop\n\tnop");
}

/*
 * In an interrupt handler, let the interrupts above 11 in until endNesting. From a
 * task, with interrupts on, both do nothing.
 */
static inline void beginNesting(VipNesting *nesting)
{
	nesting->psw = vipReadControl(VIP_CR_PSW);
	nesting->enable = 0;
	nesting->epc = 0;
	if (nesting->psw & VIP_PSW_IEC)
		return;
	nesting->enable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);
	nesting->epc = vipReadControl(VIP_CR_EPC);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, nesting->enable & 0xfffff000);
	settle();
	mep_ei();
}

static inline void endNesting(const VipNesting *nesting)
{
	if (nesting->psw & VIP_PSW_IEC)
		return;
	mep_di();
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, nesting->enable);
	vipWriteControl(VIP_CR_PSW, nesting->psw);
	vipWriteControl(VIP_CR_EPC, nesting->epc);
	settle();
}

void vipWakeHardware(void)
{
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
		vipReadControlBus(VIP_CB_CLOCK_ENABLE) | VIP_CLOCK_CORE);
	enableDecoderClocks();
	enableDemuxClocks();
}

/* Stop the clocks; the core's too while the debugger resets, saves or drains it. */
void vipIdleHardware(void)
{
	gateDecoderClocks();
	gateDemuxClocks();
	if (g_status->command == VIP_DEBUG_RESET || g_status->command == VIP_DEBUG_SAVE ||
	    g_status->command == VIP_DEBUG_DRAIN)
		vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
			vipReadControlBus(VIP_CB_CLOCK_ENABLE) & ~VIP_CLOCK_CORE);
}

/* Stop the bitstream and stream units, and drop the interrupts they raised. */
static void stopBitstream(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 0);
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 0);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_DECODER);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_WRITE_STREAM0);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_WRITE_STREAM1);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_8);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_11);
	settle();
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

void resetBitstream(void)
{
	stopBitstream();
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 2);
	vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 1);
	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_NAL);
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY0, 1);
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY1, 1);
}

static void waitBitstream(void)
{
	uint32_t i;

	for (i = 0; i < BITSTREAM_POLLS; i++)
		if (!vipReadControlBus(VIP_CB_BITSTREAM_COMMAND))
			break;
}

static void runBitstream(uint32_t command)
{
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, command);
	waitBitstream();
}

/* the next count bits of the unit (0: 32) */
static uint32_t readBits(uint32_t count)
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
static uint32_t readGolomb(uint32_t isSigned)
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
static void addRecord(uint32_t type, uint32_t value)
{
	VipParsedRecord *record = &g_parsed[g_recordCount];

	record->header = type << VIP_RECORD_TYPE_SHIFT |
		(g_recordFlags & VIP_RECORD_FLAGS_MASK) << VIP_RECORD_FLAGS_SHIFT |
		(value & VIP_RECORD_VALUE_MASK);
	record->pts = g_pts;
	record->dts = g_dts;
}

/* The next record's data: size bytes at the output cursor. */
static void setRecordSize(uint32_t size)
{
	const VipRingSegment *segments = g_writeStream.segments;

	if (g_outputCursor >= segments[0].start + segments[0].size)
		g_outputCursor = g_outputCursor - segments[0].size +
			(segments[1].start - segments[0].start);
	g_parsed[g_recordCount].position = g_outputCursor;
	g_outputCursor += size;
}

static inline void putRecord(uint32_t type, uint32_t value, uint32_t size)
{
	addRecord(type, value);
	setRecordSize(size);
	g_recordCount++;
}

/* Take the unit's record, and its data, from the input. */
static void takeRecord(void)
{
	while (advanceRingReader(g_recordRing[g_channel], sizeof(VipDemuxRecord)))
		;
	while (setRingReader(g_inputRing[g_channel], g_nextPosition))
		;
}

/*
 * The unit's records are out: hand its data and records to PU1, take it from the
 * input, and go on with the next record (CAVLC pictures: in the next frame).
 */
static void finishRecord(void)
{
	VipNesting nesting;
	uint32_t channel;

	beginNesting(&nesting);
	setRingWriterState(g_outputRing[g_channel], 0);
	setRingWriterState(g_outputRecordRing[g_channel], 0);
	while (setRingWriter(g_outputRing[g_channel], g_writeStream.cursor))
		;
	while (advanceRingWriter(g_outputRecordRing[g_channel],
				 (uint16_t)g_recordTotal * sizeof(VipParsedRecord)))
		;
	takeRecord();
	channel = g_channel;
	g_pictureStarted[channel] = 0;
	g_timestampState[channel] = g_nextTimestampState;
	g_savedPts[channel] = g_nextSavedPts;
	g_savedDts[channel] = g_nextSavedDts;
	g_savedFlags[channel] = g_nextSavedFlags;
	if (g_recordType == VIP_RECORD_AVC_START) {
		g_decoderState = VIP_DECODER_IDLE;
		g_channelOpen[g_channel] = 0;
	} else if (g_recordType == VIP_RECORD_AVC_PICTURE) {
		g_pictures[g_channel]++;
		if (g_currentPps->entropyCodingMode)
			decodeRecords();
		else
			g_decoderState = VIP_DECODER_IDLE;
	} else {
		decodeRecords();
	}
	endNesting(&nesting);
}

/*
 * Write the parsed records to the output record ring and run callback; when they
 * do not fit, count an overflow, hold the ring and stop (-1).
 */
static int flushRecords(void (*callback)(void))
{
	VipRingSegment segments[2];
	uint32_t length = g_recordCount * sizeof(VipParsedRecord);
	uint32_t copied = 0, size, i;
	const uint32_t *source;
	uint32_t *destination;
	int count;

	count = getRingWritable(g_outputRecordRing[g_channel], segments, 2);
	for (i = 0; (int)i < count; i++) {
		uint32_t end = segments[i].start + segments[i].size;

		if (g_outputRecordCursor < segments[i].start)
			continue;
		destination = (uint32_t *)g_outputRecordCursor;
		source = (const uint32_t *)((const uint8_t *)g_parsed + copied);
		size = length - copied;
		if (end - g_outputRecordCursor < size)
			size = end - g_outputRecordCursor;
		copied += size;
		g_outputRecordCursor += size;
		if (g_outputRecordCursor == end) {
			if ((int)i + 1 >= count)
				goto overflow;
			g_outputRecordCursor = segments[i + 1].start;
		}
		for (size /= 4; size; size--)
			*destination++ = *source++;
		if (copied >= length)
			break;
	}
	g_recordCount = 0;
	callback();
	return 0;

overflow:
	g_overflows[g_channel]++;
	setRingWriterState(g_outputRecordRing[g_channel], 1);
	resetBitstream();
	g_decoderState = VIP_DECODER_IDLE;
	return -1;
}

/* Count the records, and send them. */
static inline int sendRecords(void (*callback)(void))
{
	g_recordTotal += g_recordCount;
	return flushRecords(callback);
}

/* Drop the unit and go on with the next. */
static void consumeRecord(void)
{
	takeRecord();
	decodeRecords();
}

static void spsWritten(void)
{
	VipNesting nesting;
	uint32_t vui;

	beginNesting(&nesting);
	vui = g_parseBuffer[4] & 1;
	putRecord(VIP_PARSED_SPS, 0, (g_parsedWords - g_padded) * 4);
	if (g_padded)
		putRecord(VIP_PARSED_PADDING, 0, 4);
	sendRecords(vui ? parseVui : finishRecord);
	endNesting(&nesting);
}

static void ppsWritten(void)
{
	VipNesting nesting;

	beginNesting(&nesting);
	putRecord(VIP_PARSED_PPS, 0, (g_parsedWords - g_padded) * 4);
	if (g_padded)
		putRecord(VIP_PARSED_PADDING, 0, 4);
	sendRecords(finishRecord);
	endNesting(&nesting);
}

/* A picture's header records are out: hand them to PU1 before its slice data. */
static void sliceRecordsWritten(void)
{
	setRingWriterState(g_outputRecordRing[g_channel], 0);
	while (advanceRingWriter(g_outputRecordRing[g_channel],
				 (uint16_t)g_recordTotal * sizeof(VipParsedRecord)))
		;
	g_recordTotal = 0;
	startSliceData();
}

/*
 * The slice header is out. A picture's first slice header goes to PU1 at once; the
 * others' records wait for the slice data (three at a time).
 */
static void sliceHeaderWritten(void)
{
	VipNesting nesting;
	uint32_t value;

	beginNesting(&nesting);
	value = g_slice.fieldPic << VIP_PARSED_FIELD_PIC_SHIFT & 0xffff;
	if (g_recordType == VIP_RECORD_AVC_PICTURE) {
		if (!g_pictureStarted[g_channel]) {
			putRecord(VIP_PARSED_PICTURE_HEADER, value, g_parsedWords * 4);
			sendRecords(sliceRecordsWritten);
			g_pictureStarted[g_channel] = 1;
		} else {
			setRecordSize(g_parsedWords * 4);
			startSliceData();
		}
	} else {
		putRecord(VIP_PARSED_SLICE_HEADER, value, g_parsedWords * 4);
		if ((int16_t)g_recordCount != SEI_RECORDS)
			startSliceData();
		else
			sendRecords(startSliceData);
	}
	endNesting(&nesting);
}

/*
 * Copy length bytes of parse results at local to the write stream, then run
 * callback. When the stream has no room, count an overflow and stop.
 */
static void writeParsed(uint32_t local, uint32_t length, void (*callback)(void))
{
	const VipRingSegment *segment = g_writeStream.segments;
	uint32_t control = VIP_DMA_LIST;
	uint32_t written = 0, count = 0, size, i;

	if ((local & 0xffff0000) == VIP_BANK1)
		control |= VIP_DMA_BANK1;
	for (i = 0; i < g_writeStream.segmentCount; i++, segment++) {
		uint32_t end = segment->start + segment->size;

		if (g_writeStream.cursor < segment->start)
			continue;
		size = length - written;
		if (end - g_writeStream.cursor < size)
			size = end - g_writeStream.cursor;
		g_writeList[count].external = g_writeStream.cursor;
		g_writeList[count].localLength = (local + (written & ~3u)) << 16 | size;
		written += size;
		g_writeStream.cursor += size;
		if (g_writeStream.cursor == end) {
			if (i + 1 >= g_writeStream.segmentCount)
				goto overflow;
			g_writeStream.cursor = segment[1].start;
		}
		count = (count + 1) & 0xff;
		if (written >= length)
			break;
	}
	queueListDma(control, (uint32_t)g_writeList << 16 | count * sizeof(VipDmaDescriptor),
		callback, NULL);
	return;

overflow:
	g_overflows[g_channel]++;
	setRingWriterState(g_outputRing[g_channel], 1);
	resetBitstream();
	g_decoderState = VIP_DECODER_IDLE;
}

/* After a failed parse: hand PU1 the unit's records (they have no data). */
static void failedRecordWritten(void)
{
	setRingWriterState(g_outputRing[g_channel], 0);
	setRingWriterState(g_outputRecordRing[g_channel], 0);
	while (setRingWriter(g_outputRing[g_channel], g_outputCursor))
		;
	while (advanceRingWriter(g_outputRecordRing[g_channel],
				 (uint16_t)g_recordTotal * sizeof(VipParsedRecord)))
		;
}

static void delimiterWritten(void)
{
	VipNesting nesting;

	beginNesting(&nesting);
	putRecord(VIP_PARSED_DELIMITER, 0, 8);
	sendRecords(finishRecord);
	endNesting(&nesting);
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
	uint32_t value, lengths, i;

	value = readGolomb(0);
	hrd->cpbCntMinus1 = value;
	buffer[0] = value << 27;
	buffer[0] |= readBits(8) << 19;		/* bit_rate_scale, cpb_size_scale */
	buffer[65] = 0;
	for (i = 0; i <= hrd->cpbCntMinus1; i++) {
		buffer[1 + 2 * i] = readGolomb(0);	/* bit_rate_value_minus1 */
		buffer[2 + 2 * i] = readGolomb(0);	/* cpb_size_value_minus1 */
		buffer[65] |= readBits(1) << (31 - i);	/* cbr_flag */
	}
	lengths = readBits(20);
	buffer[66] = lengths << 12;
	hrd->timeOffsetLength = lengths & 0x1f;
	hrd->initialCpbRemovalDelayLengthMinus1 = lengths >> 15;
	hrd->cpbRemovalDelayLengthMinus1 = lengths >> 10 & 0x1f;
	hrd->dpbOutputDelayLengthMinus1 = lengths >> 5 & 0x1f;
}

/*
 * The unit has parsed an SPS: keep what the slice headers need, and write the
 * results out. A failed parse goes out as an empty record.
 */
static void spsParsed(void)
{
	VipSps *sps;
	uint32_t word, id, scalingLists, refFrameOffsets;

	g_decoderState = VIP_DECODER_BUSY;
	if (vipReadControlBus(VIP_CB_BITSTREAM_STATUS)) {
		g_errorCode[g_channel] = VIP_DECODER_ERROR_SPS;
		putRecord(VIP_PARSED_SPS, 0, 0);
		sendRecords(failedRecordWritten);
		consumeRecord();
		return;
	}
	g_parsedWords = SPS_WORDS;
	id = g_parseBuffer[0] >> 2 & 0xff;
	sps = &g_sps[g_channel][id < VIP_SPS_COUNT ? id : 0];
	g_currentSps = sps;
	sps->valid = 1;
	word = g_parseBuffer[0];
	scalingLists = word >> 31 & 1;
	refFrameOffsets = word >> 30 & 1;
	if ((uint8_t)(word >> 22) == 100)	/* High profile */
		g_currentSps->chroma = (word & 3) != 0;
	else
		g_currentSps->chroma = 1;
	g_currentSps->levelIdc = g_parseBuffer[0] >> 10;
	g_currentSps->log2MaxFrameNumMinus4 = g_parseBuffer[1] >> 28 & 0xf;
	g_currentSps->picOrderCntType = g_parseBuffer[1] >> 26 & 3;
	g_currentSps->log2MaxPicOrderCntLsbMinus4 = g_parseBuffer[1] >> 22 & 0xf;
	g_currentSps->deltaPicOrderAlwaysZero = g_parseBuffer[1] >> 21 & 1;
	g_currentSps->frameMbsOnly = g_parseBuffer[4] >> 4 & 1;
	g_currentSps->mbAdaptiveFrameField = g_parseBuffer[4] >> 3 & 1;
	g_currentSps->picWidthInMbsMinus1 = g_parseBuffer[4] >> 16;
	g_currentSps->direct8x8Inference = g_parseBuffer[4] >> 2 & 1;
	if (scalingLists)
		g_parsedWords += SCALING_LIST_WORDS;
	if (refFrameOffsets)
		g_parsedWords += REF_FRAME_OFFSET_WORDS;
	if (scalingLists + refFrameOffsets != 1) {
		/* an even number of words: whole 8-byte units */
		g_parseBuffer[g_parsedWords] = 0;
		g_parsedWords++;
		g_padded = 1;
	}
	writeParsed(VIP_PARSED_BUFFER, g_parsedWords * 4, spsWritten);
}

/* vui_parameters(), up to the NAL HRD; the SPS record's callback */
static void parseVui(void)
{
	VipNesting nesting;
	uint32_t *buffer = g_parseBuffer;
	uint32_t flag, value, i;

	beginNesting(&nesting);
	for (i = 0; i < 0x128 / 4; i++)
		buffer[i] = 0;
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
	endNesting(&nesting);
}

static void parseVuiVclHrd(void)
{
	VipNesting nesting;
	uint32_t *buffer = g_parseBuffer;
	uint32_t flag;

	beginNesting(&nesting);
	flag = readBits(1);			/* vcl_hrd_parameters_present_flag */
	g_currentSps->vclHrd.present = flag;
	buffer[6] = flag << 31;
	if (flag)
		parseHrd(&g_currentSps->vclHrd);
	writeParsed((uint32_t)(buffer + 6), 0x110, parseVuiEnd);
	endNesting(&nesting);
}

static void parseVuiEnd(void)
{
	VipNesting nesting;
	uint32_t *buffer = g_parseBuffer;
	uint32_t value;

	beginNesting(&nesting);
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
	endNesting(&nesting);
}

static void vuiWritten(void)
{
	VipNesting nesting;

	beginNesting(&nesting);
	putRecord(VIP_PARSED_VUI, 0, 0x238);
	sendRecords(finishRecord);
	endNesting(&nesting);
}

/* The unit has parsed a PPS (see spsParsed). */
static void ppsParsed(void)
{
	VipPps *pps;
	uint32_t scalingLists;

	g_decoderState = VIP_DECODER_BUSY;
	if (vipReadControlBus(VIP_CB_BITSTREAM_STATUS)) {
		g_errorCode[g_channel] = VIP_DECODER_ERROR_PPS;
		putRecord(VIP_PARSED_PPS, 0, 0);
		sendRecords(failedRecordWritten);
		consumeRecord();
		return;
	}
	g_parsedWords = PPS_WORDS;
	pps = &g_pps[g_channel][g_parseBuffer[0] >> 23 & 0xff];
	g_currentPps = pps;
	pps->valid = 1;
	scalingLists = g_parseBuffer[0] >> 31 & 1;
	g_currentPps->spsId = g_parseBuffer[0] >> 15;
	if (g_currentPps->spsId >= VIP_SPS_COUNT)
		g_currentPps->spsId = 0;
	g_currentPps->entropyCodingMode = g_parseBuffer[0] >> 14 & 1;
	g_currentPps->bottomFieldPicOrderInFramePresent = g_parseBuffer[0] >> 13 & 1;
	g_currentPps->numRefIdxL0DefaultActiveMinus1 = g_parseBuffer[0] >> 8 & 0x1f;
	g_currentPps->numRefIdxL1DefaultActiveMinus1 = g_parseBuffer[0] >> 3 & 0x1f;
	g_currentPps->weightedPred = g_parseBuffer[1] >> 31 & 1;
	g_currentPps->weightedBipredIdc = g_parseBuffer[1] >> 29 & 3;
	g_currentPps->picInitQp = (g_parseBuffer[1] >> 23 & 0x3f) + 26;
	g_currentPps->deblockingFilterControlPresent = g_parseBuffer[1] >> 17 & 1;
	g_currentPps->constrainedIntraPred = g_parseBuffer[1] >> 15 & 1;
	if (scalingLists) {
		g_parsedWords += SCALING_LIST_WORDS;
		g_parseBuffer[g_parsedWords] = 0;
		g_parsedWords++;
		g_padded = 1;
	}
	writeParsed(VIP_PARSED_BUFFER, g_parsedWords * 4, ppsWritten);
}

/* the bytes before the unit in its first 8 bytes: 0xff, not a start code */
static void maskMisalignment(void)
{
	uint8_t *nal = (uint8_t *)VIP_NAL_BUFFER;
	uint32_t i;

	for (i = 0; i < g_misalignment; i++)
		nal[i] = 0xff;
}

/* Read past those bytes and the start code; the last value read. */
static uint32_t skipStartCode(void)
{
	uint32_t value;

	if (g_misalignment >= 4)
		readBits(0);
	if (g_misalignment & 3)
		readBits((g_misalignment & 3) * 8);
	value = readBits(0);
	if ((value & 0xffff) == 1)	/* 00 00 00 01 */
		value = readBits(8);
	return value;
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
	g_currentPps = &g_pps[g_channel][ppsId < VIP_PPS_COUNT ? ppsId : 0];
	g_currentSps = &g_sps[g_channel][g_currentPps->spsId];
	if (!g_currentSps->valid || !g_currentPps->valid)
		return -1;
	runBitstream(VIP_BITSTREAM_LOAD);
	vipWriteControlBus(VIP_CB_BITSTREAM_READ_WORDS, 0);
	vipWriteControlBus(VIP_CB_BITSTREAM_NAL_BITS, g_nalLength << 3);
	skipStartCode();
	g_parsedWords = VIP_SLICE_HEADER_WORDS;
	vipWriteControlBus(VIP_CB_BITSTREAM_NAL_HEADER, g_nalRefIdc << 5 | g_nalUnitType);
	if (g_recordType == VIP_RECORD_AVC_PICTURE) {
		sps = g_currentSps;
		pps = g_currentPps;
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
	}
	vipWriteControlBus(VIP_CB_BITSTREAM_PICTURE_SETUP, g_pictureSetup);
	g_decoderState = VIP_DECODER_SLICE_HEADER;
	g_sliceHeaderRead = 0;
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_SLICE_HEADER);
	return 0;
}

/* The reference lists are out: have the unit parse the rest of the header. */
static void resumeSliceHeader(void)
{
	runBitstream(VIP_BITSTREAM_LOAD);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY0, 1);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY1, 1);
	vipWriteControlBus(VIP_CB_BITSTREAM_READ_WORDS, 0);
	vipWriteControlBus(VIP_CB_BITSTREAM_NAL_BITS, g_nalLength << 3);
	maskMisalignment();
	mep_syncm();
	skipStartCode();
	g_decoderState = VIP_DECODER_SLICE_HEADER;
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_SLICE_HEADER);
}

/* one list of ref_pic_list_modification(), at most entries long */
static void readListChanges(uint32_t *list, uint32_t entries)
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
	VipNesting nesting;
	uint32_t *buffer = g_parseBuffer;
	uint32_t sliceType, fieldPic = 0, list1;

	beginNesting(&nesting);
	runBitstream(VIP_BITSTREAM_LOAD);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY0, 1);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY1, 1);
	vipWriteControlBus(VIP_CB_BITSTREAM_READ_WORDS, 0);
	vipWriteControlBus(VIP_CB_BITSTREAM_NAL_BITS, g_nalLength << 3);
	maskMisalignment();
	mep_syncm();
	skipStartCode();
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
	vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
	if (g_readStream.half) {
		g_readStream.cursor = g_records[0].position;
		g_readStream.half = 0;
		loadNal(NULL);
	}
	mep_syncm();
	writeParsed(VIP_PARSED_BUFFER, g_parsedWords * 4, resumeSliceHeader);
	endNesting(&nesting);
}

/*
 * The unit has parsed a slice header: read the lists it left to us first, then
 * keep what the slice data needs and write the results out.
 */
static void sliceHeaderParsed(void)
{
	uint32_t status, word, needsLists, weights, marking;
	uint32_t firstMb, width, row, column;

	g_decoderState = VIP_DECODER_BUSY;
	status = vipReadControlBus(VIP_CB_BITSTREAM_STATUS);
	if (!g_sliceHeaderRead && status) {
		g_errorCode[g_channel] = VIP_DECODER_ERROR_SLICE_HEADER;
		putRecord(g_recordType == VIP_RECORD_AVC_PICTURE ? VIP_PARSED_PICTURE_HEADER :
			VIP_PARSED_SLICE_HEADER, 0, 0);
		putRecord(g_currentPps->entropyCodingMode ? VIP_PARSED_CABAC_DATA :
			VIP_PARSED_CAVLC_DATA, 0, 0);
		sendRecords(failedRecordWritten);
		consumeRecord();
		return;
	}
	word = g_parseBuffer[0];
	needsLists = word >> VIP_SLICE_NEEDS_LISTS & 1;
	if (needsLists && !g_sliceHeaderRead) {
		vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
		vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
		if (!g_readStream.half) {
			parseSliceHeader();
			return;
		}
		g_readStream.half = 0;
		g_readStream.cursor = g_records[0].position;
		loadNal(parseSliceHeader);
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
	g_slice.qp = (g_currentPps->picInitQp + (g_parseBuffer[7] >> 21)) & 0x3f;
	firstMb = word >> 16 & 0x1fff;
	width = g_currentSps->picWidthInMbsMinus1 + 1;
	row = firstMb / width;
	column = firstMb % width;
	if (g_currentSps->mbAdaptiveFrameField && !g_slice.fieldPic)
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
static void advanceSeiStream(void)
{
	const VipRingSegment *segments = g_readStream.segments;
	uint32_t words = vipReadControlBus(VIP_CB_BITSTREAM_READ_WORDS);
	uint32_t taken = (words & 0xfe) * 4;

	g_readStream.cursor = g_seiOffset + g_records[0].position + taken;
	if (g_readStream.cursor > segments[0].start + segments[0].size)
		g_readStream.cursor = g_readStream.cursor - segments[0].size - segments[0].start +
			segments[1].start;
	g_seiOffset += taken;
	vipWriteControlBus(VIP_CB_BITSTREAM_READ_WORDS, words & 1);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY0, 1);
	vipWriteControlBus(VIP_CB_READ_STREAM_READY1, 1);
}

/* Skip what is left of the message; then the next one, or send the records. */
static void nextSeiMessage(void)
{
	VipNesting nesting;
	uint32_t remaining;

	beginNesting(&nesting);
	remaining = g_nalLength * 8 - g_bitCount;
	g_bitCount = 0;
	if (remaining)
		readBits(remaining);
	if (vipReadControlBus(VIP_CB_BITSTREAM_MORE_DATA)) {
		advanceSeiStream();
		loadNal(parseSei);
	} else {
		sendRecords(finishRecord);
	}
	endNesting(&nesting);
}

/* A message's records are in: send them when there are three, and go on. */
static void sendSeiRecords(void)
{
	if (g_recordCount >= SEI_RECORDS)
		sendRecords(nextSeiMessage);
	else
		nextSeiMessage();
}

static void seiMessageWritten(uint32_t type, uint32_t size)
{
	VipNesting nesting;

	beginNesting(&nesting);
	putRecord(type, 0, size);
	sendSeiRecords();
	endNesting(&nesting);
}

static void bufferingPeriodWritten(void)
{
	seiMessageWritten(VIP_PARSED_BUFFERING_PERIOD, 0x208);
}

/* buffering_period() */
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
	uint32_t cancel, i;
	int32_t count;

	for (i = 0; i < 0x40 / 4; i++)
		buffer[i] = 0;
	buffer[0] = readGolomb(0);		/* pan_scan_rect_id */
	cancel = readBits(1);
	buffer[1] = cancel << 31;
	if (!cancel) {
		count = readGolomb(0);		/* pan_scan_cnt_minus1 */
		buffer[1] |= count << 29;
		for (i = 0; (int32_t)i <= count; i++) {
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
	loadNal(NULL);
	writeParsed((uint32_t)g_parseBuffer, SEI_COPY_WORDS * 4, copySeiPayload);
}

static void seiPayloadCopied(void)
{
	VipNesting nesting;

	beginNesting(&nesting);
	setRecordSize(g_nalLength);
	g_recordCount++;
	if (g_nalLength & 7)
		putRecord(VIP_PARSED_PADDING, 0, 8 - (g_nalLength & 7));
	sendSeiRecords();
	endNesting(&nesting);
}

/* An SEI parse that ran away: send what there is and stop. */
static void seiFailed(uint32_t error)
{
	g_decoderState = VIP_DECODER_FAILED;
	g_errorCode[g_channel] = error;
	putRecord(VIP_PARSED_SEI, 0xff, 0);
	sendRecords(finishRecord);
	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_NAL);
}

/* sei_message(): its type and size, and its payload by type */
static void parseSei(void)
{
	VipNesting nesting;
	uint32_t type = 0, byte;

	beginNesting(&nesting);
	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_RAW);
	g_nalLength = 0;
	while ((byte = readBits(8)) == 0xff) {
		if (g_bitCount > SEI_MAX_BITS) {
			seiFailed(VIP_DECODER_ERROR_SEI_TYPE);
			goto out;
		}
		type += 0xff;
	}
	type += byte;
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
		addRecord(VIP_PARSED_USER_DATA, type << 4);
		g_seiRemaining = g_nalLength;
		copySeiPayload();
		break;
	case 5:
		addRecord(VIP_PARSED_USER_DATA_UUID, type << 4);
		g_seiRemaining = g_nalLength;
		copySeiPayload();
		break;
	case 6:
		parseRecoveryPoint();
		break;
	default:
		if (type < 22) {
			addRecord(VIP_PARSED_SEI, type << 4);
			g_seiRemaining = g_nalLength;
			copySeiPayload();
		} else {
			putRecord(VIP_PARSED_SEI, type << 4, 0);
			sendRecords(finishRecord);
		}
		break;
	}
	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, VIP_BITSTREAM_MODE_NAL);
out:
	endNesting(&nesting);
}

/*
 * A slice's CAVLC data is out: its record, and one for the padding of its last
 * word.
 */
static void cavlcWritten(void)
{
	VipNesting nesting;

	beginNesting(&nesting);
	putRecord(VIP_PARSED_CAVLC_DATA, 0, g_writeStream.half * VIP_STREAM_HALF +
		(g_parsedWords - g_padded) * 4 - 4 + g_lastBytes);
	if (g_padded || g_lastBits)
		putRecord(VIP_PARSED_PADDING,
			(g_lastBits & 7) << VIP_PADDING_BITS_SHIFT & 0xffff,
			g_padded * 4 - g_lastBytes + 4);
	sendRecords(finishRecord);
	endNesting(&nesting);
}

/* the write stream's half with the slice data's end; its words, made even */
static uint32_t *takeSliceEnd(void)
{
	uint32_t written = vipReadControlBus(VIP_CB_BITSTREAM_WRITE_WORDS);
	uint32_t *buffer = (uint32_t *)(written & VIP_BITSTREAM_HALF1 ?
		VIP_SLICE_BUFFER + VIP_STREAM_HALF : VIP_SLICE_BUFFER);

	g_parsedWords = written & VIP_BITSTREAM_WORD_MASK;
	return buffer;
}

/* The unit has decoded CAVLC slice data: write out the rest of it. */
static void cavlcStopped(void)
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
	if (sps->levelIdc >= 31) {
		vipWriteControlBus(VIP_CB_BITSTREAM_SMALL_PICTURE, 0);
		if (sps->picWidthInMbsMinus1 < 45)
			vipWriteControlBus(VIP_CB_BITSTREAM_SMALL_PICTURE, 1);
	} else {
		vipWriteControlBus(VIP_CB_BITSTREAM_SMALL_PICTURE, 1);
		if (sps->picWidthInMbsMinus1 >= 76)
			vipWriteControlBus(VIP_CB_BITSTREAM_SMALL_PICTURE, 0);
	}
	/* 0: frame, 1: field, 2: MBAFF frame */
	if (g_currentSps->frameMbsOnly)
		structure = 0;
	else if (g_currentSps->mbAdaptiveFrameField && !g_slice.fieldPic)
		structure = 2;
	else
		structure = g_slice.fieldPic != 0;
	if (g_slice.numRefIdxActiveOverride == 1) {
		list0 = g_slice.numRefIdxL0ActiveMinus1 != 0;
		list1 = g_slice.numRefIdxL1ActiveMinus1 != 0;
	} else {
		list0 = g_currentPps->numRefIdxL0DefaultActiveMinus1 != 0;
		list1 = g_currentPps->numRefIdxL1DefaultActiveMinus1 != 0;
	}
	sps = g_currentSps;
	pps = g_currentPps;
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
	VipNesting nesting;
	uint32_t status, padding;

	beginNesting(&nesting);
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
	endNesting(&nesting);
}

/* The unit stopped in CABAC slice data: write out what it has. */
static void cabacStopped(void)
{
	uint32_t status = vipReadControlBus(VIP_CB_BITSTREAM_STATUS);
	uint32_t *buffer;

	if (status & VIP_BITSTREAM_DATA_ERROR)
		g_errorCode[g_channel] = VIP_DECODER_ERROR_SLICE_DATA;
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

/* Start the unit on the slice data: CABAC once its state is cleared by DMA. */
static void startSliceData(void)
{
	if (!g_currentPps->entropyCodingMode) {
		g_decoderState = VIP_DECODER_CAVLC;
		vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_DECODE);
		return;
	}
	g_decoderState = VIP_DECODER_CABAC;
	g_readList.clear.descriptor.external = (uint32_t)g_readList.clear.zeros;
	g_readList.clear.zeros[0] = 0;
	g_readList.clear.descriptor.localLength = CABAC_STATE << 16 | CABAC_STATE_SIZE;
	g_readList.clear.zeros[1] = 0;
	queueRequest(VIP_DMA_LIST | VIP_DMA_FILL | VIP_DMA_EXTERNAL_LOCAL, 0,
		(uint32_t)&g_readList.clear.descriptor << 16 | sizeof(VipDmaDescriptor),
		sizeof(g_readList.clear.zeros), 0, startCabac);
}

static void addStartRecord(void)
{
	putRecord(VIP_PARSED_START, 0, 0);
}

/* A unit with no data to parse: have the unit finish what it has written. */
static void endStream(void)
{
	g_decoderState = VIP_DECODER_ENDING;
	g_writeStream.ended = 1;
	vipWriteControlBus(VIP_CB_BITSTREAM_COMMAND, VIP_BITSTREAM_DECODE);
}

static void streamEnded(void)
{
	g_writeStream.ended = 0;
	g_decoderState = VIP_DECODER_BUSY;
	setRecordSize(0);
	g_recordCount++;
	sendRecords(finishRecord);
}

static void resetParameterSets(uint32_t channel)
{
	uint32_t i;

	for (i = 0; i < VIP_SPS_COUNT; i++)
		g_sps[channel][i].valid = 0;
	for (i = 0; i < VIP_PPS_COUNT; i++)
		g_pps[channel][i].valid = 0;
}

/*
 * The unit is in the read stream: start the bitstream unit on it, read its NAL
 * header, and go on by its type.
 */
static void startRecord(void)
{
	VipNesting nesting;
	uint32_t sliceStart, header;

	beginNesting(&nesting);
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
	maskMisalignment();
	header = skipStartCode();
	g_nalRefIdc = header >> 5 & 3;
	g_nalUnitType = header & 0x1f;
	g_padded = 0;
	switch (g_recordType) {
	case VIP_RECORD_AVC_PICTURE:
	case VIP_RECORD_AVC_SLICE:
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
		addRecord(VIP_PARSED_END_SEQUENCE, 0);
		endStream();
		break;
	case VIP_RECORD_AVC_END_STREAM:
		addRecord(VIP_PARSED_END_STREAM, 0);
		endStream();
		break;
	case VIP_RECORD_AVC_START:
		addStartRecord();
		if (!sendRecords(finishRecord))
			resetParameterSets(g_channel);
		break;
	default:			/* VIP_RECORD_AVC_ZEROS among them */
		takeRecord();
		g_decoderState = VIP_DECODER_IDLE;
		break;
	}
	endNesting(&nesting);
}

/*
 * Copy the read stream's next 1 KB (from the 8 bytes the cursor is in) into the
 * read stream buffer, then run callback.
 */
static void loadNal(void (*callback)(void))
{
	const VipRingSegment *segment = g_readStream.segments;
	uint32_t loaded = 0, count = 0, start, length, end, i;

	for (i = 0; i < g_readStream.segmentCount; i++, segment++) {
		if (g_readStream.cursor < segment->start)
			continue;
		end = segment->start + segment->size;
		start = g_readStream.cursor & ~7u;
		length = VIP_NAL_BUFFER_SIZE - loaded;
		if (end - start < length) {
			length = end - start;
			if (length & 7)
				length += 8 - (length & 7);
		}
		g_readList.list[count].external = start;
		g_readList.list[count].localLength = (VIP_NAL_BUFFER + loaded) << 16 | length;
		loaded += length;
		g_readStream.cursor = start + length;
		if (g_readStream.cursor == end && i + 1 < g_readStream.segmentCount)
			g_readStream.cursor = segment[1].start;
		count = (count + 1) & 0xff;
		if (loaded >= VIP_NAL_BUFFER_SIZE)
			break;
	}
	queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ,
		(uint32_t)g_readList.list << 16 | count * sizeof(VipDmaDescriptor), callback, NULL);
}

/* Route an interrupt to level 1, and enable it. */
static void setupInterrupt(uint32_t interrupt)
{
	uint32_t bit = 1u << interrupt;
	uint32_t shift = (interrupt & 7) * 4;
	uint32_t mask = ~(0xfu << shift), level = 1u << shift, pending;

	vipWriteControlBus(VIP_CB_INTERRUPT_PENDING,
		vipReadControlBus(VIP_CB_INTERRUPT_PENDING) | bit);
	switch (interrupt >> 3) {
	case 0:
		vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL0,
			(vipReadControlBus(VIP_CB_INTERRUPT_LEVEL0) & mask) | level);
		break;
	case 1:
		vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL1,
			(vipReadControlBus(VIP_CB_INTERRUPT_LEVEL1) & mask) | level);
		break;
	case 2:
		vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL2,
			(vipReadControlBus(VIP_CB_INTERRUPT_LEVEL2) & mask) | level);
		break;
	default:
		vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL3,
			(vipReadControlBus(VIP_CB_INTERRUPT_LEVEL3) & mask) | level);
		break;
	}
	pending = vipReadControlBus(VIP_CB_INTERRUPT_CLEAR) & ~bit;
	mep_di();
	vipWriteControlBus(VIP_CB_INTERRUPT_CLEAR, pending);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE,
		vipReadControlBus(VIP_CB_INTERRUPT_ENABLE) | bit);
	settle();
	mep_ei();
}

static void initializeDecoder(void)
{
	uint32_t interrupt;

	g_decoderState = VIP_DECODER_IDLE;
	g_channel = 0;
	g_channelOpen[0] = 0;
	for (interrupt = VIP_INTERRUPT_DECODER; interrupt <= VIP_INTERRUPT_11; interrupt++)
		setupInterrupt(interrupt);
	g_nextSavedPts = 0;
	g_nextSavedDts = 0;
	g_currentSps = NULL;
	g_currentPps = NULL;
	g_nextSavedFlags = 0;
	g_nextTimestampState = 0;
	g_writeStream.ended = 0;
}

static void stopDecoder(void)
{
	stopBitstream();
	g_decoderState = VIP_DECODER_IDLE;
}

/* Go on to the next open channel after the current one. */
static void selectChannel(void)
{
	uint32_t i, next;

	for (i = g_channel + 1; i < g_channel + VIP_DECODER_CHANNELS; i++) {
		next = i >= VIP_DECODER_CHANNELS ? i - VIP_DECODER_CHANNELS : i;
		if (g_channelOpen[next & 0xff]) {
			g_channel = next;
			return;
		}
	}
}

/* the channel's rings, in handles: input, records, output, output records */
static void setChannelRings(VipRingHandle *handles, uint32_t channel)
{
	g_inputRing[channel] = &handles[0];
	g_recordRing[channel] = &handles[1];
	g_outputRing[channel] = &handles[2];
	g_outputRecordRing[channel] = &handles[3];
}

static int openDecoder(VipRingHandle *handles, uint32_t channel)
{
	int result;

	if (channel >= VIP_DECODER_CHANNELS)
		return -2;
	setChannelRings(handles, channel);
	result = openRing(g_inputRing[channel], VIP_RING_OPEN_READER);
	result |= openRing(g_recordRing[channel], VIP_RING_OPEN_READER);
	result |= openRing(g_outputRing[channel], VIP_RING_OPEN_WRITER);
	result |= openRing(g_outputRecordRing[channel], VIP_RING_OPEN_WRITER);
	if (result & 1)
		return -1;
	g_pictures[channel] = 0;
	g_errorCode[channel] = 0;
	g_starved[channel] = 0;
	g_overflows[channel] = 0;
	g_timestampState[channel] = 0;
	g_savedPts[channel] = 0;
	g_savedDts[channel] = 0;
	g_savedFlags[channel] = 0;
	g_pictureStarted[channel] = 0;
	resetParameterSets(channel);
	g_channelOpen[channel] = 1;
	return 0;
}

/* Stop the channel, once PU1 has closed its output rings (-1 until then). */
static int closeDecoder(VipRingHandle *handles, uint32_t channel)
{
	int result;

	if (channel >= VIP_DECODER_CHANNELS)
		return -2;
	mep_di();
	if (g_channel == channel) {
		enableDecoderClocks();
		stopDecoder();
		gateDecoderClocks();
	}
	mep_ei();
	setChannelRings(handles, channel);
	if (ringReadersClosed(g_outputRing[channel]) ||
	    ringReadersClosed(g_outputRecordRing[channel]))
		return -1;
	result = closeRing(g_inputRing[channel]);
	result |= closeRing(g_recordRing[channel]);
	result |= closeRing(g_outputRing[channel]);
	result |= closeRing(g_outputRecordRing[channel]);
	if (result & 1)
		return -1;
	g_channelOpen[channel] = 0;
	return 0;
}

static void outputRecordWritten(void)
{
	while (advanceRingWriter(g_outputRecordRing[g_channel], sizeof(VipParsedRecord)))
		;
}

/* Forget the stream: its input, its timestamps and its parameter sets. */
static void dropStream(void)
{
	uint32_t channel;

	while (ringSkipToWriter(g_recordRing[g_channel]))
		;
	while (ringSkipToWriter(g_inputRing[g_channel]))
		;
	setRingWriterState(g_outputRing[g_channel], 0);
	setRingWriterState(g_outputRecordRing[g_channel], 0);
	channel = g_channel;
	g_timestampState[channel] = 0;
	g_nextTimestampState = 0;
	g_savedPts[channel] = 0;
	g_nextSavedPts = 0;
	g_savedDts[channel] = 0;
	g_nextSavedDts = 0;
	g_savedFlags[channel] = 0;
	g_nextSavedFlags = 0;
	resetParameterSets(g_channel);
	g_pictureStarted[g_channel] = 0;
}

/*
 * The restart handshake, through the rings' reader state 0: while PU1 has not
 * taken the last restart, wait (0); when the demultiplexer restarted, drop the
 * stream and pass the restart on to PU1 (1).
 */
static int checkRestart(void)
{
	if (getRingReaderState0(g_outputRecordRing[g_channel])) {
		g_decoderState = VIP_DECODER_IDLE;
		return 0;
	}
	if (!getRingReaderState0(g_recordRing[g_channel]))
		return 0;
	dropStream();
	setRingReaderState0(g_outputRecordRing[g_channel], 1);
	setRingReaderState0(g_recordRing[g_channel], 0);
	g_decoderState = VIP_DECODER_IDLE;
	return 1;
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
static void applyTimestamps(void)
{
	uint32_t channel = g_channel;
	int same = g_pts == g_savedPts[channel] && g_dts == g_savedDts[channel] &&
		g_recordFlags == g_savedFlags[channel];

	switch (g_timestampState[channel]) {
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
				g_nextSavedPts = g_pts;
				g_nextSavedDts = g_dts;
				g_nextSavedFlags = g_recordFlags;
			}
		} else if (!same) {
			g_pts = g_savedPts[channel];
			g_dts = g_savedDts[channel];
			g_recordFlags = g_savedFlags[channel];
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

/* An overflow of an output ring: hold its writer until PU1 makes room. */
static void holdOutput(VipRingHandle *ring)
{
	g_overflows[g_channel]++;
	setRingWriterState(ring, 1);
	g_decoderState = VIP_DECODER_IDLE;
}

/*
 * No whole record: if the input is nearly full, the demultiplexer is stuck, so
 * drop its input and give PU1 an empty record; else count a starved frame.
 */
static void waitForRecords(void)
{
	VipRingSegment segments[2];
	int space;

	space = getRingSpace(g_inputRing[g_channel]);
	g_decoderState = VIP_DECODER_IDLE;
	if (space >= 1 && (uint32_t)space < INPUT_STUCK) {
		while (ringSkipToWriter(g_recordRing[g_channel]))
			;
		while (ringSkipToWriter(g_inputRing[g_channel]))
			;
		if ((uint32_t)getRingSpace(g_outputRecordRing[g_channel]) <
		    sizeof(VipParsedRecord) + 1)
			return;
		getRingWritable(g_outputRing[g_channel], segments, 2);
		g_readList.record.header = VIP_PARSED_SPS << VIP_RECORD_TYPE_SHIFT;
		g_readList.record.pts = 0;
		g_readList.record.dts = 0;
		g_readList.record.position = segments[0].start;
		getRingWritable(g_outputRecordRing[g_channel], segments, 2);
		queueRequest(VIP_DMA_WRITE, segments[0].start,
			(uint32_t)&g_readList.record << 16 | sizeof(VipParsedRecord), 0, 0,
			outputRecordWritten);
		return;
	}
	if (g_pictures[g_channel])
		g_starved[g_channel]++;
}

/*
 * Take the next record and its NAL unit (and the next record, where the unit
 * ends), and start on it once the output has room. 2: the channel is closed.
 */
static int decodeRecords(void)
{
	VipRingSegment segments[2];
	VipNesting nesting;
	uint32_t total, copied, length, position, needed, i;
	int count, space, empty;

	selectChannel();
	checkRestart();
	if (!g_channelOpen[g_channel])
		return 2;
	for (;;) {
		if (getRingReaderState0(g_outputRecordRing[g_channel])) {
			g_decoderState = VIP_DECODER_IDLE;
			return 0;
		}
		checkRestart();
		enableDecoderClocks();
		resetBitstream();
		g_decoderState = VIP_DECODER_BUSY;
		count = getRingReadable(g_recordRing[g_channel], segments, 2);
		total = 0;
		for (i = 0; (int)i < count; i++)
			total += segments[i].size;
		if (!total || (total == sizeof(VipDemuxRecord) &&
			       *(const uint32_t *)segments[0].start >> VIP_RECORD_TYPE_SHIFT !=
			       VIP_RECORD_AVC_START)) {
			waitForRecords();
			return 0;
		}

		beginNesting(&nesting);
		copied = 0;
		for (i = 0; (int)i < count; i++) {
			const uint32_t *source = (const uint32_t *)segments[i].start;
			uint32_t *destination = (uint32_t *)((uint8_t *)g_records + copied);
			uint32_t words;

			length = sizeof(g_records) - copied;
			if (segments[i].size < length)
				length = segments[i].size;
			for (words = length / 4; words; words--)
				*destination++ = *source++;
			copied += length;
			if (copied == sizeof(g_records)) {
				g_nextRecord = segments[i].start + length - sizeof(VipDemuxRecord);
				break;
			}
		}
		g_recordCount = 0;
		g_writeStream.segmentCount = getRingWritable(g_outputRing[g_channel],
			g_writeStream.segments, 2);
		g_writeStream.cursor = g_writeStream.segments[0].start;
		g_outputCursor = g_writeStream.segments[0].start;
		getRingWritable(g_outputRecordRing[g_channel], segments, 2);
		g_outputRecordCursor = segments[0].start;
		g_readStream.segmentCount = getRingReadable(g_inputRing[g_channel],
			g_readStream.segments, 2);
		g_recordCount = 0;
		g_recordTotal = 0;
		g_recordType = g_records[0].header >> VIP_RECORD_TYPE_SHIFT;
		g_pts = g_records[0].pts;
		g_dts = g_records[0].dts;
		g_recordFlags = g_records[0].header >> VIP_RECORD_FLAGS_SHIFT & VIP_RECORD_FLAGS_MASK;
		position = g_records[0].position;
		g_readStream.cursor = position;
		applyTimestamps();

		if (g_recordType == VIP_RECORD_AVC_START) {
			g_nextPosition = g_records[0].position;
			startRecord();
			endNesting(&nesting);
			return 0;
		}
		g_nextPosition = g_records[1].position;
		g_nalLength = getRingDistance(g_inputRing[g_channel], position & ~7u,
			g_nextPosition);
		length = getRingDistance(g_inputRing[g_channel], position, g_nextPosition);
		if (getRingAvailable(g_inputRing[g_channel]) < (int)length) {
			endNesting(&nesting);
			g_decoderState = VIP_DECODER_IDLE;
			return 0;
		}
		g_misalignment = position & 7;
		g_writeStream.half = 0;
		g_cabacWritten = 0;
		g_readStream.half = 0;
		if (g_recordType == VIP_RECORD_AVC_ZEROS) {
			startRecord();
			endNesting(&nesting);
			continue;
		}

		if ((uint32_t)getRingSpace(g_outputRecordRing[g_channel]) < SPACE_RECORDS) {
			holdOutput(g_outputRecordRing[g_channel]);
			endNesting(&nesting);
			return 0;
		}
		empty = isRingEmpty(g_outputRing[g_channel]);
		space = getRingSpace(g_outputRing[g_channel]);
		switch (g_recordType) {
		case VIP_RECORD_AVC_SLICE:
		case VIP_RECORD_AVC_PICTURE:
			if ((uint32_t)space < g_nalLength + (g_nalLength >> 1)) {
				if (!empty) {
					holdOutput(g_outputRing[g_channel]);
				} else {
					/* it will never fit */
					dropStream();
					g_decoderState = VIP_DECODER_IDLE;
				}
				endNesting(&nesting);
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
			needed = SPACE_MARKER;
			break;
		default:
			needed = SPACE_OTHER;
			break;
		}
		if ((uint32_t)space < needed) {
			holdOutput(g_outputRing[g_channel]);
			endNesting(&nesting);
			return 0;
		}
		loadNal(startRecord);
		endNesting(&nesting);
		return 0;
	}
}

/* the bitstream unit's interrupt: the step it finished, by the decoder's state */
void decoderInterrupt(uint32_t argument)
{
	VipNesting nesting;

	(void)argument;
	beginNesting(&nesting);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_DECODER);
	if (g_decoderState == VIP_DECODER_SLICE_HEADER)
		sliceHeaderParsed();
	else if (g_decoderState == VIP_DECODER_CABAC)
		cabacStopped();
	else if (g_decoderState == VIP_DECODER_CAVLC)
		cavlcStopped();
	else if (g_decoderState == VIP_DECODER_PPS)
		ppsParsed();
	else if (g_decoderState == VIP_DECODER_SPS)
		spsParsed();
	else
		streamEnded();
	gateDecoderClocks();
	endNesting(&nesting);
}

uint32_t vipHardwareBusy(uint32_t state)
{
	uint32_t busy = 0;

	if (state == VIP_STATE_SAVING && decoderBusy())
		busy = 1;
	busy |= vipReadControlBus(VIP_CB_DMA_BUSY);
	busy |= vipReadControlBus(VIP_CB_BITSTREAM_COMMAND);
	busy |= vipReadControlBus(VIP_CB_SCANNER_START);
	return busy;
}

/* Keep the interrupt controller, the local RAM data and bank 1 for the debugger. */
void vipSaveHardware(void)
{
	const uint32_t *data = (const uint32_t *)__lram_data_start;
	const uint8_t *bank1 = (const uint8_t *)VIP_BANK1;
	uint32_t i;

	g_savedHardware.interruptEnable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, 0);
	g_savedHardware.interruptStatus = vipReadControlBus(VIP_CB_INTERRUPT_STATUS);
	g_savedHardware.interruptPending = vipReadControlBus(VIP_CB_INTERRUPT_PENDING);
	g_savedHardware.interruptLevel[0] = vipReadControlBus(VIP_CB_INTERRUPT_LEVEL0);
	g_savedHardware.interruptLevel[1] = vipReadControlBus(VIP_CB_INTERRUPT_LEVEL1);
	g_savedHardware.interruptLevel[2] = vipReadControlBus(VIP_CB_INTERRUPT_LEVEL2);
	g_savedHardware.interruptLevel[3] = vipReadControlBus(VIP_CB_INTERRUPT_LEVEL3);
	g_savedHardware.dmaControl = vipReadControlBus(VIP_CB_DMA_CONTROL);
	for (i = 0; i < (uint32_t)__lram_data_size / 4; i++)
		g_savedHardware.localData[i] = data[i];
	for (i = 0; i < VIP_BANK1_SAVED; i++)
		g_savedHardware.bank1[i] = bank1[i];
	g_savedHardware.pauseFlags = g_pauseFlags;
}

void vipRestoreHardware(void)
{
	uint32_t *data = (uint32_t *)__lram_data_start;
	uint32_t *bank1Words = (uint32_t *)VIP_BANK1;
	uint8_t *bank1 = (uint8_t *)VIP_BANK1;
	uint32_t i;

	for (i = 0; i < 0x800; i++)
		bank1Words[i] = 0xff;
	for (i = 0; i < VIP_BANK1_SAVED; i++)
		bank1[i] = g_savedHardware.bank1[i];
	for (i = 0; i < (uint32_t)__lram_data_size / 4; i++)
		data[i] = g_savedHardware.localData[i];
	g_pauseFlags = g_savedHardware.pauseFlags;
	vipWriteControlBus(VIP_CB_DMA_CONTROL, g_savedHardware.dmaControl);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, 0);
	vipWriteControlBus(VIP_CB_INTERRUPT_STATUS, g_savedHardware.interruptStatus);
	vipWriteControlBus(VIP_CB_INTERRUPT_PENDING, g_savedHardware.interruptPending);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL0, g_savedHardware.interruptLevel[0]);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL1, g_savedHardware.interruptLevel[1]);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL2, g_savedHardware.interruptLevel[2]);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL3, g_savedHardware.interruptLevel[3]);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, g_savedHardware.interruptEnable);
}

/* Stop every unit; how much of the wait for the DMA was left (0: it did not stop). */
uint32_t vipResetHardware(void)
{
	uint32_t remaining = RESET_POLLS, busy;

	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, 0);
	vipWriteControlBus(VIP_CB_DMA_CONTROL, 0);
	vipWriteControlBus(VIP_CB_DMA_BUSY, 0);
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 0);
	vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 0);
	vipWriteControlBus(VIP_CB_BITSTREAM_MODE, 0);
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY0, 0);
	vipWriteControlBus(VIP_CB_WRITE_STREAM_READY1, 0);
	vipWriteControlBus(VIP_CB_SPARE_CONTROL, 0);
	vipWriteControlBus(VIP_CB_STREAM_SETUP, 0);
	vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 0);
	do {
		busy = vipReadControlBus(VIP_CB_DMA_BUSY);
		remaining--;
	} while (busy && remaining);
	return remaining;
}

/* One frame's decoding, and the counters for the debugger. */
static void decodeFrame(void)
{
	volatile VipDecoderCounters *counters = VIP_DECODER_COUNTERS;

	decodeRecords();
	gateDecoderClocks();
	counters->pictures = g_pictures[0];
	counters->starved = g_starved[0];
	counters->overflows = g_overflows[0];
}

/*
 * Task 2: serve debug point 1's commands (open the decoder on point 0's rings,
 * close it) and, while it is open, decode a frame each time the schedule comes
 * round.
 */
void commandTask(uint32_t point)
{
	volatile VipDecoderCounters *counters = VIP_DECODER_COUNTERS;
	uint32_t action = COMMAND_NONE, argument = 1, count, value, result;
	uint32_t state = DECODER_CLOSED;
	uint32_t pu1Frames = 0, i;

	initializeDecoder();
	for (;;) {
		result = 0;
		if (checkDebugPoint(point, &action, &argument, &count) >= 0) {
			if (action == COMMAND_OPEN) {
				if (argument != 1)
					result = RESULT_BAD_ACTION;
				else if (count != COMMAND_OPEN_COUNT)
					result = RESULT_BAD_COUNT;
				else if (state != DECODER_CLOSED)
					result = RESULT_BAD_STATE;
				else
					state = DECODER_OPENING;
			} else if (action == COMMAND_CLOSE) {
				if (argument != 1)
					result = RESULT_BAD_ACTION;
				else if (count != COMMAND_CLOSE_COUNT)
					result = RESULT_BAD_COUNT;
				else if (state != DECODER_OPEN)
					result = RESULT_BAD_STATE;
				else
					state = DECODER_CLOSING;
			} else if (action != COMMAND_NONE) {
				result = RESULT_BAD_ACTION;
			}
		}
		if (!result) {
			switch (state) {
			case DECODER_OPENING:
				for (i = 0; i < VIP_DECODER_RINGS; i++) {
					getDebugPointValue(point, i, &value);
					setRing(&g_rings[i], (VipRing *)value);
				}
				if (openDecoder(g_rings, 0) < 0)
					break;
				holdSchedule(point);
				result = RESULT_DONE;
				state = DECODER_OPEN;
				break;
			case DECODER_OPEN:
				if (g_errorCode[0]) {
					writeTrace(point, g_errorCode[0], g_pictures[0]);
					if ((g_errorCode[0] & VIP_DECODER_ERROR_CLASS) ==
					    VIP_DECODER_ERROR_SLICE_DATA)
						counters->sliceDataErrors++;
					else
						counters->otherErrors++;
					g_errorCode[0] = 0;
				}
				if (needsRestart() && (g_decoderState == VIP_DECODER_IDLE ||
						       g_decoderState == VIP_DECODER_FAILED))
					decodeFrame();
				break;
			case DECODER_CLOSING:
				if (closeDecoder(g_rings, 0) < 0)
					break;
				releaseSchedule(point);
				state = DECODER_CLOSED;
				result = RESULT_DONE;
				break;
			}
		}
		if (result) {
			writeTrace(point, 0, action << 16 | result);
			setDebugPointResult(point, result);
			disableDebugPoint(point);
		}
		if (pu1Frames != VIP_STATUSES[1].frameCount)
			notifyDebugger(0, EVENT_PU1_FRAME);
		pu1Frames = VIP_STATUSES[1].frameCount;
		setPauseFlag(point, state);
		vipTaskYield();
	}
}

/* the bitstream unit is at work on a unit */
static uint32_t decoderBusy(void)
{
	return g_decoderState != VIP_DECODER_IDLE && g_decoderState != VIP_DECODER_FAILED;
}
