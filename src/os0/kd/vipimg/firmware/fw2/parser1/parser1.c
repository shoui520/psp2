/*
 * FW2's parser1: the parser for H.264 video in an MPEG program stream, fw1's
 * demultiplexer (fw1/pu0/pes.c) redone for FW2. Its start code scanner makes two
 * passes over each piece of the input. The PES pass finds the PES packets of the
 * video stream (stream ids 0xe0-0xef), keeps their timestamps and has the
 * scanner copy their payload to its output; DMA moves that to the elementary
 * stream ring. The NAL pass scans the copied payload for NAL units and writes a
 * record for each to the NAL record ring, with the timestamps of the PES packet
 * the unit starts in. The parser library (fw2/parser) reads the two rings.
 *
 * The ARM side marks the end of a picture's input and of the stream in the input
 * ring's header; the scanner records them too, and ends a picture where the next
 * access unit starts. When the record ring fills, the NAL pass is suspended, its
 * buffer kept in the context's saved data, and a later command resumes it.
 */
#include "../parser/parser.h"
#include "../../fw1/pu0/scanner.h"

/* g_scannerState */
#define SCANNER_IDLE		0
#define SCANNER_RUNNING		1	/* a pass is on its way */
#define SCANNER_STARVED		2	/* a packet header waits for more input */
#define SCANNER_HELD		3	/* the NAL record ring is full: the NAL pass is suspended */
#define SCANNER_ENDED		5	/* the stream's end is recorded */

/* g_pesState: where the PES pass is */
#define PES_SEARCHING		0	/* for a packet */
#define PES_PAYLOAD		1	/* in a payload, copying it, and for the next packet */
#define PES_HEADER_READ		2	/* a packet's header is read: its payload starts */

/* g_nalState: what the NAL pass looks for */
#define NAL_SEARCHING		0	/* a start code and its NAL header */
#define NAL_IN_UNIT		1	/* the unit's end */
#define NAL_HEADER_PENDING	2	/* the byte after a slice's header, in the next piece */

/* the video stream's PES start codes: 00 00 01 Ex */
#define PES_STREAM_ID		0xe0
#define PES_STREAM_ID_IGNORED	0x0f

/* the input the scanner takes at a time, and the output space that needs */
#define PES_PIECE		0x1f0
#define PES_SPACE_MINIMUM	0x200
#define PES_HEADER_MINIMUM	6	/* bytes after the stream id: length, flags, size */

/* where DMA lists put a piece of input for the scanner; the scanner's output */
#define SCAN_INPUT_DMA		0x1a00
#define SCAN_OUTPUT		(VIP_BANK1 + 0x1800)
#define SCAN_OUTPUT_SIZE	0x200

/* the NAL pass goes on while the record ring has room for this */
#define RECORD_SPACE_MINIMUM	(3 * sizeof(VipRecord))

/* the timestamps of the last PES packets, and which one goes with a unit */
#define TIMESTAMP_SLOTS		4
#define NO_SLOT			0xff

/* a PES packet's timestamps, as its NAL units' records take them */
typedef struct VipPesTimestamps {
	uint32_t flags;			/* VIP_RECORD_PTS... */
	uint32_t pts;
	uint32_t dts;
} VipPesTimestamps;

/* the context's scanner state */
static uint32_t g_pesStart VIP_PROGRAM_SAVED;	/* the scanner's offset when the PES search started */
static uint32_t g_savedRead VIP_PROGRAM_SAVED;	/* the NAL pass's range, while suspended */
static uint32_t g_savedEnd VIP_PROGRAM_SAVED;
static uint32_t g_pesConfig VIP_PROGRAM_SAVED;	/* how much of a start code each pass has seen */
static uint32_t g_nalConfig VIP_PROGRAM_SAVED;
static uint32_t g_savedOutput[2] VIP_PROGRAM_SAVED;	/* the output's last partial 8 bytes */
static uint32_t g_nalPass VIP_PROGRAM_SAVED;	/* the scanner is on the NAL pass */
static uint32_t g_pesState VIP_PROGRAM_SAVED;
static uint32_t g_nalState VIP_PROGRAM_SAVED;
uint32_t g_scannerState VIP_PROGRAM_SAVED;
static uint32_t g_units VIP_PROGRAM_SAVED;
static uint32_t g_endRecords VIP_PROGRAM_SAVED;	/* picture and stream ends recorded */
static uint32_t g_payloadStart VIP_PROGRAM_SAVED;	/* the output offset of the payload, or 0 */
static uint32_t g_afterZeros VIP_PROGRAM_SAVED;	/* the last unit ended at zero bytes */
static uint32_t g_inPicture VIP_PROGRAM_SAVED;	/* a slice has come since the last end */
static uint32_t g_zeroCount VIP_PROGRAM_SAVED;	/* of a start code held back; 4: 00 00 01 */
static uint32_t g_pendingHeader VIP_PROGRAM_SAVED;	/* the NAL header of NAL_HEADER_PENDING */
static uint32_t g_timestampSlot VIP_PROGRAM_SAVED;	/* the current packet's */
static uint32_t g_slotPending VIP_PROGRAM_SAVED;	/* the next packet's, until its payload */
static VipPesTimestamps g_timestamps[TIMESTAMP_SLOTS] VIP_PROGRAM_SAVED;
static VipRingHandle g_inputHandle VIP_PROGRAM_SAVED;
static VipRingHandle g_recordHandle VIP_PROGRAM_SAVED;	/* the NAL records */
static VipRingHandle g_streamHandle VIP_PROGRAM_SAVED;	/* the elementary stream */
static VipRingSegment g_readable[2] VIP_PROGRAM_SAVED;	/* of the input */
static uint8_t g_savedBuffer[SCAN_OUTPUT_SIZE] VIP_PROGRAM_SAVED __attribute__((aligned(8)));

/* a command's */
static uint32_t g_pesPosition VIP_PROGRAM_LOCAL;	/* the input position of the piece */
static uint32_t g_streamCursor VIP_PROGRAM_LOCAL;	/* the elementary stream's of the output */
static uint32_t g_readableCount VIP_PROGRAM_LOCAL;
static uint32_t g_writableCount VIP_PROGRAM_LOCAL;
static VipRingSegment g_writable[2] VIP_PROGRAM_LOCAL;	/* of the elementary stream */
static uint32_t g_inputStart VIP_PROGRAM_LOCAL;	/* the input ring's */
static uint32_t g_inputSize VIP_PROGRAM_LOCAL;
static uint32_t g_streamStart VIP_PROGRAM_LOCAL;	/* the elementary stream ring's */
static uint32_t g_streamSize VIP_PROGRAM_LOCAL;
static uint32_t g_pesListed VIP_PROGRAM_LOCAL;	/* bytes of input in the piece */
static VipDmaDescriptor g_lists[2] VIP_PROGRAM_LOCAL __attribute__((aligned(8)));

/* the loader: its image in DRAM and its place in local RAM (fw2/parser.ld) */
extern const uint8_t g_loaderImage[], g_loaderStart[], g_loaderSize[];
#define VIP_PROGRAM_LOADER	__attribute__((section(".loader.program"), noinline))

static void continueNalPass(void);
static void suspendScan(void);
static void gateScannerClock(void);
static VIP_DRAM int startPesScan(void);
static VIP_PROGRAM_OVERLAY uint32_t pesPosition(uint32_t offset);
static VIP_PROGRAM_OVERLAY void releaseInput(void);
static VIP_PROGRAM_OVERLAY uint32_t streamPosition(uint32_t offset);
static VIP_PROGRAM_OVERLAY void addRecord(uint32_t type, uint32_t position,
	const VipPesTimestamps *timestamps);
static VIP_PROGRAM_OVERLAY void scanForUnit(void);
static VIP_PROGRAM_OVERLAY void scanForUnitEnd(void);
VIP_PROGRAM_LOADER void loadNal(void (*callback)(void));

/* ---- in DRAM ---- */

/* the core's reset hook (g_resetHooks): restart the units the parsers left on */
void parser1ResetHardware(void)
{
	if (vipReadControlBus(VIP_CB_CLOCK_REQUEST) & VIP_CLOCK_REQUEST_DECODER) {
		vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 0);
		vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 1);
	}
	if (vipReadControlBus(VIP_CB_CLOCK_ENABLE) & VIP_CLOCK_DEMUX) {
		vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 0);
		vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 1);
	}
}

/* VIP_PARSER_SET_OPTION: option 0 is the option byte, at the top of the value */
static int setOption(const uint32_t *arguments)
{
	if (arguments[0])
		return -1;
	g_parser.parser1.options = arguments[1] >> 24 & VIP_PARSER_OPTION_NEED_INPUT;
	return 0;
}

static int openStreams(VipRing *input, uint32_t cursor, VipRing *stream, VipRing *records);
static void enableScannerInterrupt(void);

/*
 * VIP_PARSER_OPEN: the rings are the input program stream, the parsed stream,
 * its records, the elementary stream and the NAL records. Each must be there,
 * its data 8-byte aligned.
 */
static int openParser(VipRing *const *rings)
{
	uint32_t i;
	int result;

	for (i = 0; i < 5; i++) {
		if (!rings[i])
			return -2;
		if (rings[i]->start && (rings[i]->start & 7))
			return -3;
	}
	result = openStreams(rings[0], 2, rings[3], rings[4]);
	if (result)
		return result;
	initializeDecoder();
	g_loadNal = loadNal;
	g_decoderState = VIP_DECODER_IDLE;
	return openDecoder(rings[3], 2, rings[4], 2, rings[1], rings[2]);
}

/* There is input the scanner or the library has not taken, or the stream ended. */
static int hasInput(void)
{
	const VipRing *input = g_inputHandle.ring, *stream = g_streamHandle.ring;

	if (input->readerState[1])
		return 1;
	if (input->cursors[VIP_RING_WRITER - 1].position != input->cursors[1].position)
		return 1;			/* the scanner's */
	return stream->cursors[VIP_RING_WRITER - 1].position != stream->cursors[1].position;
}

/* Show in the context's state which rings hold the parser. */
static uint32_t updateState(void)
{
	uint32_t set, keep;

	if (g_scannerState == SCANNER_HELD) {
		set = VIP_PARSER_INPUT_HELD;
		keep = ~0u;
	} else {
		set = 0;
		keep = ~VIP_PARSER_INPUT_HELD;
	}
	if (getRingWriterState(&g_outputRing))
		set |= VIP_PARSER_OUTPUT_HELD;
	else
		keep &= ~VIP_PARSER_OUTPUT_HELD;
	if (getRingWriterState(&g_outputRecordRing))
		set |= VIP_PARSER_RECORDS_HELD;
	else
		keep &= ~VIP_PARSER_RECORDS_HELD;
	g_parser.state = (g_parser.state & keep) | set;
	vipSetState(g_parser.state);
	return set;
}

static int scannerStep(void);

/* the library's results for the ARM side */
static inline void copyStatistics(void)
{
	g_parser.pictures = g_pictures;
	g_parser.starved = g_starved;
	g_parser.overflows = g_overflows;
	g_parser.parser1.endRecords = g_endRecords;
	g_parser.parser1.units = g_units;
}

/*
 * Run the scanner and the library until the command's work is done (parser0's
 * runParser).
 */
static void runParser(void)
{
	uint32_t rounds = 0, awake, state;

	do {
		if (g_scannerState != SCANNER_ENDED && g_scannerState != SCANNER_RUNNING &&
		    scannerStep() == VIP_STEP_PICTURE_END)
			vipNotifyEvents(VIP_PARSER_EVENT_PICTURE_END);
		state = g_decoderState;
		if (state == VIP_DECODER_IDLE || state == VIP_DECODER_FAILED ||
		    state == VIP_DECODER_HELD) {
			if (rounds >= VIP_PARSER_IDLE_ROUNDS) {
				vipWaitTimer(VIP_PARSER_IDLE_WAIT);
				rounds = 0;
			}
			decodeRecords();
		}
		copyStatistics();
		if (g_errorCode) {
			uint32_t error = g_errorCode;

			g_errorCode = 0;
			vipTraceEvent(error, g_pictures);
		}
		awake = 1;
		mep_di();
		state = g_decoderState;
		if (g_decoderOpen && state != VIP_DECODER_PICTURE_DONE &&
		    state != VIP_DECODER_IDLE && state != VIP_DECODER_FAILED &&
		    state != VIP_DECODER_HELD &&
		    (g_scannerState == SCANNER_RUNNING || g_scannerState == SCANNER_ENDED) &&
		    !vipReadControlBus(VIP_CB_INTERRUPT_CLEAR)) {
			mep_halt();
			awake = 0;
		}
		mep_ei();
		rounds += awake;
		updateState();
		if (g_decoderState == VIP_DECODER_FAILED)
			vipNotifyEvents(VIP_PARSER_EVENT_FAILED);
	} while (g_decoderState != VIP_DECODER_PICTURE_DONE &&
		 g_decoderState != VIP_DECODER_HELD && g_decoderOpen);
}

/*
 * VIP_PARSER_RUN: scan and parse until a picture's input ends, the stream ends,
 * the input runs dry or the output is full.
 */
static int running(void)
{
	int result;

	if ((g_parser.parser1.options & VIP_PARSER_OPTION_NEED_INPUT) && !hasInput())
		return VIP_PARSER_NO_INPUT;
	if (g_decoderOpen) {
		readDram((uint32_t)g_loaderImage, (uint32_t)g_loaderStart,
			(uint32_t)g_loaderSize);
		enableScannerInterrupt();
		setupInterrupts();
		g_decoderState = VIP_DECODER_IDLE;
		runParser();
		while (g_scannerState == SCANNER_RUNNING)
			;
	}
	if (g_scannerState == SCANNER_ENDED && !g_decoderOpen) {
		g_parser.mode = VIP_PARSER_ENDED;
		vipSetState(VIP_PARSER_ENDED);
		result = VIP_PROGRAM_CONTINUE;
	} else if (g_decoderState == VIP_DECODER_HELD) {
		result = VIP_PARSER_HELD;
	} else {
		result = VIP_RESULT_OK;
	}
	copyStatistics();
	return result;
}

/* the commands a parser knows */
static inline int isParserCommand(uint32_t kind)
{
	return kind == VIP_PARSER_OPEN || kind == VIP_PARSER_SET_OPTION ||
		kind == VIP_PARSER_RUN || kind == VIP_PARSER_CLOSE;
}

/* The program's entry: run the command by the parser's mode. */
int parser1Entry(uint32_t *output)
{
	uint32_t *command, kind;
	int result = VIP_RESULT_OK;

	vipSetState(g_parser.state);
	command = vipGetCommand();
	if (!command)
		return VIP_PARSER_NO_COMMAND;
	kind = command[2];
	switch (g_parser.mode) {
	case VIP_PARSER_OPENED:
		if (kind == VIP_PARSER_RUN)
			result = running();
		else if (isParserCommand(kind))
			result = VIP_PARSER_WRONG_MODE;
		else
			result = VIP_PARSER_NO_COMMAND;
		break;
	case VIP_PARSER_CLOSED:
		if (kind == VIP_PARSER_OPEN) {
			result = openParser((VipRing *const *)(command + 3));
			if (result) {
				*output = result;
				result = VIP_PARSER_FAILED;
				break;
			}
			g_parser.mode = VIP_PARSER_OPENED;
			vipSetState(VIP_PARSER_OPENED);
		} else if (kind == VIP_PARSER_SET_OPTION) {
			if (setOption(command + 3))
				result = VIP_PARSER_FAILED;
		} else if (isParserCommand(kind)) {
			result = VIP_PARSER_WRONG_MODE;
		} else {
			result = VIP_PARSER_NO_COMMAND;
		}
		break;
	case VIP_PARSER_ENDED:
		if (kind == VIP_PARSER_CLOSE) {
			stopDecoder();
			resetScanner();
			g_parser.mode = VIP_PARSER_CLOSED;
			vipSetState(VIP_PARSER_CLOSED);
		} else if (isParserCommand(kind)) {
			result = VIP_PARSER_WRONG_MODE;
		} else {
			result = VIP_PARSER_NO_COMMAND;
		}
		break;
	default:
		result = VIP_PARSER_NO_COMMAND;
		break;
	}
	g_parser.state = (g_parser.state & ~VIP_PARSER_MODE_MASK) | g_parser.mode;
	vipSetState(g_parser.state);
	return result;
}

/*
 * The NAL pass found a unit: its header byte is at header, the scanner at read.
 * Record it (after a picture end, if it starts the next access unit) with its
 * PES packet's timestamps: the last packet's if the unit started before this
 * packet's payload. A slice whose next byte is in the next piece waits for it.
 */
static void recordUnit(const uint8_t *header, uint32_t read, uint32_t end)
{
	uint32_t byte = header[0], next = 0, endsPicture, position, slot;

	if (read < end)
		next = header[1];
	endsPicture = vipNalEndsPicture(byte, next, g_inPicture);
	if (read == end && vipNalIsSlice(byte)) {
		g_pendingHeader = byte;
		g_nalState = NAL_HEADER_PENDING;
		return;
	}
	position = streamPosition(read - 4);
	if (endsPicture) {
		g_inPicture = 0;
		addRecord(VIP_RECORD_PICTURE_END, position, NULL);
		g_endRecords++;
	}
	if (vipNalIsSlice(byte))
		g_inPicture = 1;
	g_units++;
	slot = g_timestampSlot;
	if (read - g_afterZeros - 4 < g_payloadStart)
		slot = (slot - 1) & (TIMESTAMP_SLOTS - 1);
	addRecord(vipNalRecordType(byte, next), position, &g_timestamps[slot]);
	g_pendingHeader = byte;
	g_nalState = NAL_IN_UNIT;
}

/* The packet header at offset is not all in the input yet: wait for more. */
static void stopForInput(uint32_t offset)
{
	vipWriteControlBus(VIP_CB_SCANNER_READ, offset);
	g_pesPosition = pesPosition(offset);
	releaseInput();
	g_scannerState = SCANNER_STARVED;
	gateScannerClock();
}

/* NAL_HEADER_PENDING: the slice's next byte is at the piece's start. */
static void resumeUnit(void)
{
	uint32_t read = vipReadControlBus(VIP_CB_SCANNER_READ);
	uint32_t next = *(const uint8_t *)VIP_SCANNER_ADDRESS(read);
	uint32_t endsPicture, position, slot;

	endsPicture = vipNalEndsPicture(g_pendingHeader, next, g_inPicture);
	position = streamPosition(read - 4);
	if (endsPicture) {
		g_inPicture = 0;
		addRecord(VIP_RECORD_PICTURE_END, position, NULL);
		g_endRecords++;
	}
	if (vipNalIsSlice(g_pendingHeader))
		g_inPicture = 1;
	g_units++;
	slot = g_timestampSlot;
	if (read - g_afterZeros - 4 < g_payloadStart)
		slot = (slot - 1) & (TIMESTAMP_SLOTS - 1);
	addRecord(vipNalRecordType(g_pendingHeader, next), position, &g_timestamps[slot]);
	g_nalState = NAL_IN_UNIT;
	continueNalPass();
}

/* the scanner's clock, on while it works */
static void enableScannerClock(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
		vipReadControlBus(VIP_CB_CLOCK_ENABLE) | VIP_CLOCK_DEMUX);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

static void gateScannerClock(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
		vipReadControlBus(VIP_CB_CLOCK_ENABLE) & ~VIP_CLOCK_DEMUX);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

/*
 * The NAL record ring is full: keep the NAL pass's buffer and range in the saved
 * data, hand over the stream copied so far, and hold.
 */
static void suspendScan(void)
{
	g_lists[0].external = SCAN_OUTPUT;
	g_lists[0].localLength = (uint32_t)g_savedBuffer << 16 | sizeof(g_savedBuffer);
	queueListDma(VIP_DMA_LIST | VIP_DMA_EXTERNAL_LOCAL | VIP_DMA_READ,
		(uint32_t)&g_lists[0] << 16 | sizeof(VipDmaDescriptor), NULL, NULL);
	g_savedRead = vipReadControlBus(VIP_CB_SCANNER_READ);
	g_savedEnd = vipReadControlBus(VIP_CB_SCANNER_END);
	g_scannerState = SCANNER_HELD;
	while (setRingWriter(&g_streamHandle, g_streamCursor))
		;
	setRingWriterState(&g_recordHandle, 1);
	gateScannerClock();
}

/* Go on with a suspended NAL pass, once the record ring has room (-1 if not). */
static int resumeScan(void)
{
	volatile uint32_t done;

	if ((uint32_t)getRingSpace(&g_recordHandle) < RECORD_SPACE_MINIMUM) {
		gateScannerClock();
		return -1;
	}
	g_lists[1].external = (uint32_t)g_savedBuffer;
	g_lists[1].localLength = SCAN_OUTPUT << 16 | sizeof(g_savedBuffer);
	queueListDma(VIP_DMA_LIST | VIP_DMA_EXTERNAL_LOCAL | VIP_DMA_WRITE,
		(uint32_t)&g_lists[1] << 16 | sizeof(VipDmaDescriptor), NULL, &done);
	setRingWriterState(&g_recordHandle, 0);
	g_scannerState = SCANNER_RUNNING;
	vipWriteControlBus(VIP_CB_SCANNER_READ, g_savedRead);
	vipWriteControlBus(VIP_CB_SCANNER_END, g_savedEnd);
	vipWriteControlBus(VIP_CB_SCANNER_CONFIG, g_nalConfig);
	while (done == 1)
		;
	switch (g_nalState) {
	case NAL_HEADER_PENDING:
		resumeUnit();
		break;
	case NAL_IN_UNIT:
		scanForUnitEnd();
		break;
	case NAL_SEARCHING:
		scanForUnit();
		break;
	}
	return 0;
}

/*
 * Start the scanner on the input: a PES pass over the next piece, or the
 * suspended NAL pass. Without input, record the end of the stream or of a
 * picture if the ARM side marked one.
 */
static int scannerStep(void)
{
	VipRing *ring = g_inputHandle.ring;
	uint32_t state = g_scannerState, pictureEnd, streamEnd, lastEnd, end;
	int count;

	if (state == SCANNER_RUNNING)
		return VIP_STEP_BUSY;
	if (state == SCANNER_ENDED)
		return VIP_STEP_ENDED;
	pictureEnd = ring->reserved;
	g_inputStart = ring->start;
	streamEnd = ring->readerState[1];
	g_inputSize = ring->size;
	lastEnd = g_readable[0].start + g_readable[0].size;
	count = getRingReadable(&g_inputHandle, g_readable, 2);
	g_pesPosition = g_readable[0].start;
	g_readableCount = count;
	if ((pictureEnd || streamEnd) && count == 1 && state == SCANNER_STARVED) {
		end = g_readable[0].start + g_readable[0].size;
		if (end == lastEnd) {
			/* no more input will come for the header: drop it */
			if (g_inputStart + g_inputSize == end)
				end = g_inputStart;
			g_readable[0].start = end;
			g_pesPosition = end;
			g_readable[0].size = 0;
			releaseInput();
		}
	}
	g_streamStart = g_streamHandle.ring->start;
	g_streamSize = g_streamHandle.ring->size;
	g_writableCount = getRingWritable(&g_streamHandle, g_writable, 2);
	g_streamCursor = g_writable[0].start;
	if (g_readable[0].size || state == SCANNER_HELD) {
		enableScannerClock();
		if (!g_nalPass)
			return startPesScan();
		return resumeScan();
	}
	if (streamEnd) {
		if (getRingSpace(&g_recordHandle) < (int)sizeof(VipRecord) + 1)
			return VIP_STEP_NOTHING;
		g_endRecords++;
		addRecord(VIP_RECORD_STREAM_END, g_streamCursor, NULL);
		g_scannerState = SCANNER_ENDED;
		setRingReaderState1(&g_streamHandle, 1);
		return VIP_STEP_ENDED;
	}
	if (!pictureEnd || getRingSpace(&g_recordHandle) < (int)sizeof(VipRecord) + 1)
		return VIP_STEP_NOTHING;
	g_inPicture = 0;
	addRecord(VIP_RECORD_PICTURE_END, g_streamCursor, NULL);
	g_endRecords++;
	g_scannerState = SCANNER_IDLE;
	ring->reserved = 0;
	return VIP_STEP_PICTURE_END;
}

/* Route the scanner's interrupt to level 1, and enable it. */
static void enableScannerInterrupt(void)
{
	uint32_t clear;

	vipWriteControlBus(VIP_CB_INTERRUPT_PENDING,
		vipReadControlBus(VIP_CB_INTERRUPT_PENDING) | 1u << VIP_INTERRUPT_SCANNER);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL1,
		(vipReadControlBus(VIP_CB_INTERRUPT_LEVEL1) & ~0xf0000) | 0x10000);
	clear = vipReadControlBus(VIP_CB_INTERRUPT_CLEAR) & ~(1u << VIP_INTERRUPT_SCANNER);
	mep_di();
	vipWriteControlBus(VIP_CB_INTERRUPT_CLEAR, clear);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE,
		vipReadControlBus(VIP_CB_INTERRUPT_ENABLE) | 1u << VIP_INTERRUPT_SCANNER);
	__asm__ volatile ("nop\n\tnop\n\tnop");
	mep_ei();
}

/*
 * Open the scanner's rings: the input, read through the given cursor, and the
 * elementary stream and NAL record rings, which it writes. -3: a ring is not
 * aligned, -4: too small, -1: it does not open.
 */
static int openStreams(VipRing *input, uint32_t cursor, VipRing *stream, VipRing *records)
{
	if (!input || ((uint32_t)input & 3))
		return -3;
	if (input->size <= 0x400 || cursor < VIP_RING_WRITER + 1 || cursor > 4)
		return -4;
	if (!stream || ((uint32_t)stream & 3))
		return -3;
	if (stream->size <= 0x400)
		return -4;
	if (!records || ((uint32_t)records & 3))
		return -3;
	if (records->size <= 0x50)
		return -4;
	g_streamHandle.ring = stream;
	g_inputHandle.cursor = cursor;
	g_inputHandle.ring = input;
	g_streamHandle.cursor = VIP_RING_WRITER;
	g_recordHandle.cursor = VIP_RING_WRITER;
	g_recordHandle.ring = records;
	if (!openRing(&g_inputHandle, VIP_RING_OPEN_READER)) {
		if (!openRing(&g_streamHandle, VIP_RING_OPEN_WRITER)) {
			if (!openRing(&g_recordHandle, VIP_RING_OPEN_WRITER))
				goto opened;
			closeRing(&g_streamHandle);
		}
		closeRing(&g_inputHandle);
	}
	g_inputHandle.ring = NULL;
	g_inputHandle.cursor = 0;
	g_streamHandle.ring = NULL;
	g_streamHandle.cursor = 0;
	g_recordHandle.ring = NULL;
	g_recordHandle.cursor = 0;
	return -1;

opened:
	g_scannerState = SCANNER_IDLE;
	g_pesState = PES_SEARCHING;
	g_nalState = NAL_SEARCHING;
	g_nalPass = 0;
	g_inPicture = 0;
	g_units = 0;
	g_endRecords = 0;
	g_inputSize = 0;
	g_pesConfig = VIP_SCANNER_CONFIG_START;
	g_nalConfig = VIP_SCANNER_CONFIG_START;
	g_zeroCount = 0;
	g_slotPending = NO_SLOT;
	g_payloadStart = 0;
	g_afterZeros = 0;
	enableScannerInterrupt();
	return 0;
}

/* ---- in the program's code overlay ---- */

static VIP_PROGRAM_OVERLAY void pesScanDone(void);
static VIP_PROGRAM_OVERLAY void unitDone(void);
static VIP_PROGRAM_OVERLAY void unitEndDone(void);
static VIP_PROGRAM_OVERLAY void pesPieceLoaded(void);

/* The scanner stopped: go on by the pass and what it looked for. */
VIP_PROGRAM_OVERLAY void scannerInterrupt(void)
{
	vipAcknowledgeInterrupt(VIP_INTERRUPT_SCANNER);
	if (!g_nalPass)
		pesScanDone();
	else if (g_nalState == NAL_IN_UNIT)
		unitEndDone();
	else if (g_nalState == NAL_SEARCHING)
		unitDone();
}

/*
 * Bring the next piece of input to the scanner for the PES pass, once the
 * elementary stream has room for what it may copy. -1 if there is no room or no
 * input: the scanner stops.
 */
static VIP_DRAM int startPesScan(void)
{
	const VipRingSegment *segment = g_writable;
	uint32_t position = g_streamCursor, space = 0, listed = 0, filled = 0, count = 0, i;

	for (i = 0; i < g_writableCount; i++, segment++) {
		uint32_t size = segment->size;

		if (position < segment->start)
			continue;
		if (size >= position - segment->start)
			size -= position - segment->start;
		space += size;
	}
	space &= ~15u;
	if (space >= PES_SPACE_MINIMUM) {
		position = g_pesPosition;
		segment = g_readable;
		for (i = 0; i < g_readableCount; i++, segment++) {
			uint32_t end, aligned, misalignment, length;

			if (position < segment->start)
				continue;
			end = segment->start + segment->size;
			if (end == position)
				continue;
			aligned = position & ~7u;
			misalignment = position & 7;
			length = end - aligned;
			if (length < PES_PIECE - filled) {
				listed += length - misalignment;
				if (length & 7)
					length += 8 - (length & 7);
			} else {
				length = PES_PIECE - filled;
				listed += length - misalignment;
			}
			g_lists[count].external = aligned;
			g_lists[count].localLength = (SCAN_INPUT_DMA + filled) << 16 | length;
			count++;
			filled += length;
			if (filled >= PES_PIECE)
				break;
			position = aligned + length;
			if (position == end && i == 0)
				position = g_readable[1].start;
		}
		g_pesListed = listed;
		if (listed) {
			g_scannerState = SCANNER_RUNNING;
			queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ,
				(uint32_t)g_lists << 16 | count * sizeof(VipDmaDescriptor),
				pesPieceLoaded, NULL);
			return 0;
		}
	}
	releaseInput();
	g_scannerState = SCANNER_IDLE;
	gateScannerClock();
	return -1;
}

/*
 * Copy length bytes of the scanner's output to the elementary stream at its
 * cursor (whole 8-byte units), then go on with the NAL pass over them.
 */
static VIP_DRAM void copyToStream(uint32_t length)
{
	const VipRingSegment *segment = g_writable;
	uint32_t position = g_streamCursor, total, filled = 0, count = 0, i;

	total = ((position + length + 7) & ~7u) - (position & ~7u);
	for (i = 0; i < g_writableCount; i++, segment++) {
		uint32_t end, size;

		if (position < segment->start)
			continue;
		end = segment->start + segment->size;
		if (end == position)
			continue;
		position &= ~7u;
		size = ((end + 7) & ~7u) - position;
		if (size > total - filled)
			size = total - filled;
		g_lists[count].external = position;
		g_lists[count].localLength = (SCAN_OUTPUT + filled) << 16 | size;
		count++;
		filled += size;
		position += size;
		if (position == end && i == 0)
			position = g_writable[1].start;
		if (filled >= total)
			break;
	}
	queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_WRITE,
		(uint32_t)g_lists << 16 | count * sizeof(VipDmaDescriptor), continueNalPass, NULL);
}

/* the input position of a scanner offset in the current piece */
static VIP_PROGRAM_OVERLAY uint32_t pesPosition(uint32_t offset)
{
	int32_t distance = (g_pesPosition & ~7u) + (offset - VIP_SCANNER_INPUT) - g_inputStart;

	if (distance < 0)
		distance += g_inputSize;
	else if ((uint32_t)distance >= g_inputSize)
		distance -= g_inputSize;
	return g_inputStart + distance;
}

/* Give the input up to the piece back to the ring. */
static VIP_PROGRAM_OVERLAY void releaseInput(void)
{
	while (setRingReader(&g_inputHandle, g_pesPosition))
		;
}

/* the elementary stream position of a scanner offset in the output */
static VIP_PROGRAM_OVERLAY uint32_t streamPosition(uint32_t offset)
{
	int32_t distance = (g_streamCursor & ~7u) + (offset - VIP_SCANNER_OUTPUT) - g_streamStart;

	if (distance < 0)
		distance += g_streamSize;
	else if ((uint32_t)distance >= g_streamSize)
		distance -= g_streamSize;
	return g_streamStart + distance;
}

/* Write a record of a NAL unit (or an end) at position, with its timestamps. */
static VIP_PROGRAM_OVERLAY void addRecord(uint32_t type, uint32_t position,
	const VipPesTimestamps *timestamps)
{
	VipRingSegment segments[2];
	VipRecord *record;

	getRingWritable(&g_recordHandle, segments, 2);
	record = (VipRecord *)segments[0].start;
	if (!timestamps) {
		record->header = type << VIP_RECORD_TYPE_SHIFT;
		record->pts = 0;
		record->dts = 0;
	} else {
		record->header = (timestamps->flags & 0xffff) | type << VIP_RECORD_TYPE_SHIFT;
		record->pts = timestamps->pts;
		record->dts = timestamps->dts;
	}
	record->position = position;
	advanceRingWriter(&g_recordHandle, sizeof(VipRecord));
}

/* a PTS or DTS field: bits 0-31 of the timestamp, and bit 32 */
static VIP_PROGRAM_OVERLAY uint32_t readTimestamp(const uint8_t *field, uint32_t *timestamp)
{
	uint32_t middle = (field[1] << 8 | field[2]) >> 1;
	uint32_t low = (field[3] << 8 | field[4]) >> 1;

	*timestamp = (field[0] & 6) << 29 | middle << 15 | low;
	return (field[0] & 8) != 0;
}

/*
 * A PES packet's header from after its stream id, length bytes of it in the
 * piece: its timestamps, and the bytes up to its payload (0 if they are not all
 * there).
 */
static VIP_DRAM uint32_t parsePesHeader(const uint8_t *bytes, uint32_t length,
	VipPesTimestamps *timestamps)
{
	const uint8_t *field = bytes + 5;
	uint8_t header[3];
	uint32_t size, i;

	if (length < PES_HEADER_MINIMUM)
		return 0;
	for (i = 0; i < 3; i++)		/* the flags, and PES_header_data_length */
		header[i] = bytes[2 + i];
	size = header[2] + 5;
	if (length < size)
		return 0;
	timestamps->dts = 0;
	timestamps->pts = 0;
	timestamps->flags = 0;
	if (header[1] >> 6 & 2) {		/* PTS_DTS_flags */
		timestamps->flags = VIP_RECORD_PTS;
		if (readTimestamp(field, &timestamps->pts))
			timestamps->flags |= VIP_RECORD_PTS_HIGH;
		field += 5;
	}
	if (header[1] >> 6 & 1) {
		timestamps->flags |= VIP_RECORD_DTS;
		if (readTimestamp(field, &timestamps->dts))
			timestamps->flags |= VIP_RECORD_DTS_HIGH;
	}
	return size;
}

/*
 * Look for the next PES packet. In a payload the scanner copies it to its output
 * meanwhile, after the 8 bytes the last copy left partial and the start code
 * bytes held back from the last piece.
 */
static VIP_DRAM void startPesSearch(void)
{
	uint32_t state = g_pesState, mode = VIP_SCANNER_MODE_FIND, written, zeros, i;
	uint8_t *output;

	if (state != PES_SEARCHING) {
		if (g_slotPending < TIMESTAMP_SLOTS) {
			g_timestampSlot = g_slotPending;
			g_slotPending = NO_SLOT;
		}
		((uint32_t *)SCAN_OUTPUT)[0] = g_savedOutput[0];
		((uint32_t *)SCAN_OUTPUT)[1] = g_savedOutput[1];
		output = (uint8_t *)SCAN_OUTPUT + (g_streamCursor & 7);
		zeros = g_zeroCount < 4 ? g_zeroCount : 2;
		for (i = 0; i < zeros; i++)
			output[i] = 0;
		if (i < g_zeroCount)		/* 00 00 01 */
			output[i++] = 1;
		written = VIP_SCANNER_OUTPUT + (g_streamCursor & 7) + i;
		vipWriteControlBus(VIP_CB_SCANNER_WRITE, written);
		mode = VIP_SCANNER_MODE_FIND_COPY;
		g_pesState = PES_PAYLOAD;
		if (state == PES_HEADER_READ)
			g_payloadStart = written;
	}
	g_pesStart = vipReadControlBus(VIP_CB_SCANNER_READ);
	vipWriteControlBus(VIP_CB_SCANNER_PATTERN, PES_STREAM_ID);
	vipWriteControlBus(VIP_CB_SCANNER_PATTERN_MASK, PES_STREAM_ID_IGNORED);
	vipWriteControlBus(VIP_CB_SCANNER_MODE, mode);
	vipWriteControlBus(VIP_CB_SCANNER_STOP_AFTER, 1);
	vipWriteControlBus(VIP_CB_SCANNER_START, 1);
}

static VIP_PROGRAM_OVERLAY void nextPesStep(uint32_t read, uint32_t end);

/*
 * The PES pass stopped: at a packet's start code, whose header it reads, or at
 * the piece's end, holding back what may begin a start code. What it copied of a
 * payload goes to the elementary stream and through the NAL pass; then on with
 * the search.
 */
static VIP_PROGRAM_OVERLAY void pesScanDone(void)
{
	uint32_t state = g_pesState, status, read, end, held, written, base, last, config,
		slot, size;
	const uint32_t *saved;

	status = vipReadControlBus(VIP_CB_SCANNER_STATUS);
	read = vipReadControlBus(VIP_CB_SCANNER_READ);
	end = vipReadControlBus(VIP_CB_SCANNER_END);
	if (status < 2) {
		config = vipReadControlBus(VIP_CB_SCANNER_CONFIG);
		g_pesConfig = config;
		if ((config & 0x1f) == 3) {
			g_zeroCount = 4;
			held = 3;
		} else {
			held = config >> 9;
			if (held >= 4)
				held = 3;
			g_zeroCount = held;
		}
	} else {
		slot = (g_timestampSlot + 1) & (TIMESTAMP_SLOTS - 1);
		size = parsePesHeader((const uint8_t *)VIP_SCANNER_ADDRESS(read), end - read,
			&g_timestamps[slot]);
		if (!size) {
			/* the header goes past the piece: from its start code next time */
			g_slotPending = NO_SLOT;
			if (read - g_pesStart < 4) {
				stopForInput(g_pesStart);
				return;
			}
			g_pesState = PES_SEARCHING;
			read -= 4;
			end = read;
			if (state == PES_SEARCHING) {
				stopForInput(read);
				return;
			}
		} else {
			read += size;
			g_slotPending = slot;
			g_pesState = PES_HEADER_READ;
			vipWriteControlBus(VIP_CB_SCANNER_READ, read);
		}
		held = 4;
		g_zeroCount = 0;
		g_pesConfig = VIP_SCANNER_CONFIG_START;
	}
	if (state != PES_SEARCHING) {
		written = vipReadControlBus(VIP_CB_SCANNER_WRITE);
		base = VIP_SCANNER_OUTPUT + (g_streamCursor & 7);
		if (held < written - base) {
			last = written - held;
			vipWriteControlBus(VIP_CB_SCANNER_READ, base);
			vipWriteControlBus(VIP_CB_SCANNER_END, last);
			g_pesPosition = pesPosition(read);
			releaseInput();
			saved = (const uint32_t *)(SCAN_OUTPUT + ((last - VIP_SCANNER_OUTPUT) & ~7u));
			g_savedOutput[0] = saved[0];
			g_savedOutput[1] = saved[1];
			copyToStream(last - base);
			return;
		}
	}
	nextPesStep(read, end);
}

/* The next piece of input at the piece's end, else go on searching this one. */
static VIP_PROGRAM_OVERLAY void nextPesStep(uint32_t read, uint32_t end)
{
	if (read == end) {
		g_pesPosition = pesPosition(read);
		startPesScan();
	} else {
		startPesSearch();
	}
}

/* A piece is in the scan buffer (DMA callback): the PES pass over it. */
static VIP_PROGRAM_OVERLAY void pesPieceLoaded(void)
{
	uint32_t read;

	g_nalPass = 0;
	vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 0);
	vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 1);
	vipWriteControlBus(VIP_CB_SCANNER_CONFIG, g_pesConfig);
	read = VIP_SCANNER_INPUT + (g_pesPosition & 7);
	vipWriteControlBus(VIP_CB_SCANNER_READ, read);
	vipWriteControlBus(VIP_CB_SCANNER_END, read + g_pesListed);
	startPesSearch();
}

/* Look for the next NAL unit: a start code and its header. */
static VIP_PROGRAM_OVERLAY void scanForUnit(void)
{
	vipWriteControlBus(VIP_CB_SCANNER_MODE, VIP_SCANNER_MODE_FIND);
	vipWriteControlBus(VIP_CB_SCANNER_PATTERN_MASK, 0xff);
	vipWriteControlBus(VIP_CB_SCANNER_STOP_AFTER, 1);
	vipWriteControlBus(VIP_CB_SCANNER_START, 1);
}

static VIP_PROGRAM_OVERLAY void nalPieceDone(uint32_t read, uint32_t end);

/* The NAL pass stopped looking for a unit: record the one it found. */
static VIP_PROGRAM_OVERLAY void unitDone(void)
{
	uint32_t status = vipReadControlBus(VIP_CB_SCANNER_STATUS);
	uint32_t read = vipReadControlBus(VIP_CB_SCANNER_READ);
	uint32_t end = vipReadControlBus(VIP_CB_SCANNER_END);

	if (status < 2) {
		g_nalConfig = vipReadControlBus(VIP_CB_SCANNER_CONFIG);
	} else {
		g_nalConfig = VIP_SCANNER_CONFIG_START;
		recordUnit((const uint8_t *)VIP_SCANNER_ADDRESS(read - 1), read, end);
	}
	nalPieceDone(read, end);
}

/* Look for the end of the NAL unit: the next start code, or zero bytes. */
static VIP_PROGRAM_OVERLAY void scanForUnitEnd(void)
{
	vipWriteControlBus(VIP_CB_SCANNER_MODE, VIP_SCANNER_MODE_FIND_END);
	vipWriteControlBus(VIP_CB_SCANNER_STOP_AFTER, 0);
	vipWriteControlBus(VIP_CB_SCANNER_START, 1);
}

/*
 * The NAL pass stopped looking for the unit's end: at zero bytes, which get a
 * record of their own, or at the next start code.
 */
static VIP_PROGRAM_OVERLAY void unitEndDone(void)
{
	uint32_t read = vipReadControlBus(VIP_CB_SCANNER_READ);
	uint32_t status = vipReadControlBus(VIP_CB_SCANNER_STATUS);
	uint32_t afterZeros = 0;

	if (status == (VIP_SCANNER_FOUND_ZEROS | VIP_SCANNER_STOPPED)) {
		addRecord(VIP_RECORD_AVC_ZEROS, streamPosition(read - 3), NULL);
		afterZeros = 1;
		g_nalState = NAL_SEARCHING;
	} else if (status == (VIP_SCANNER_FOUND_PREFIX | VIP_SCANNER_STOPPED)) {
		g_nalState = NAL_SEARCHING;
	}
	g_afterZeros = afterZeros;
	g_nalConfig = vipReadControlBus(VIP_CB_SCANNER_CONFIG);
	nalPieceDone(read, vipReadControlBus(VIP_CB_SCANNER_END));
}

/*
 * The NAL pass is through the copied payload: hand it to the library and go on
 * with the PES pass over the next piece. Else go on with the NAL pass.
 */
static VIP_PROGRAM_OVERLAY void nalPieceDone(uint32_t read, uint32_t end)
{
	if (read != end) {
		continueNalPass();
		return;
	}
	g_nalPass = 0;
	if (g_payloadStart) {
		if (read - g_payloadStart < 6)
			g_payloadStart = VIP_SCANNER_OUTPUT + (read & 7);
		else
			g_payloadStart = 0;
	}
	g_streamCursor = streamPosition(read);
	while (setRingWriter(&g_streamHandle, g_streamCursor))
		;
	startPesScan();
}

/*
 * The NAL pass over the scanner's output (also copyToStream's callback), while
 * the record ring has room for a unit's records; else suspend it.
 */
static VIP_DRAM void continueNalPass(void)
{
	g_nalPass = 1;
	if ((uint32_t)getRingSpace(&g_recordHandle) < RECORD_SPACE_MINIMUM) {
		suspendScan();
		return;
	}
	vipWriteControlBus(VIP_CB_SCANNER_CONFIG, g_nalConfig);
	switch (g_nalState) {
	case NAL_HEADER_PENDING:
		resumeUnit();
		break;
	case NAL_IN_UNIT:
		scanForUnitEnd();
		break;
	case NAL_SEARCHING:
		scanForUnit();
		break;
	}
}

/* ---- in the loader overlay ---- */

/*
 * Load the NAL unit at the read stream's cursor into the read stream buffer
 * (whole 8-byte units, up to both halves), then run callback (g_loadNal).
 */
VIP_PROGRAM_LOADER void loadNal(void (*callback)(void))
{
	VipDmaDescriptor *list = g_dmaSource.readLists[0];
	const VipRingSegment *segment = g_readStream.segments;
	uint32_t cursor = g_readStream.cursor, filled = 0, count = 0, i;

	for (i = 0; i < g_readStream.segmentCount; i++, segment++) {
		uint32_t end, length;

		if (cursor < segment->start)
			continue;
		end = segment->start + segment->size;
		cursor &= ~7u;
		length = end - cursor;
		if (length < 2 * VIP_STREAM_HALF - filled) {
			if (length & 7)
				length += 8 - (length & 7);
		} else {
			length = 2 * VIP_STREAM_HALF - filled;
		}
		list[count].localLength = (VIP_READ_BUFFER + filled) << 16 | length;
		filled += length;
		list[count].external = cursor;
		cursor += length;
		if (cursor == end && i == 0 && g_readStream.segmentCount != 1)
			cursor = g_readStream.segments[1].start;
		count++;
		if (filled >= 2 * VIP_STREAM_HALF)
			break;
	}
	g_readStream.cursor = cursor;
	queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ,
		(uint32_t)list << 16 | count * sizeof(VipDmaDescriptor), callback, NULL);
}

/*
 * The read stream wants a half refilled: list up to a half's worth of the stream
 * and queue it. Past the end of the stream, or once the unit is all read, the
 * interrupt is only acknowledged.
 */
VIP_PROGRAM_LOADER void readStreamInterrupt(void)
{
	const VipRingSegment *segment = g_readStream.segments;
	uint32_t half = g_readStream.half, cursor = g_readStream.cursor;
	uint32_t odd = half & 1, filled = 0, count = 0, i;
	VipDmaDescriptor *list;

	if (odd) {
		vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
		list = g_dmaSource.readLists[1];
	} else {
		vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
		list = g_dmaSource.readLists[0];
	}
	g_readStream.half = half + 1;
	if (!vipReadControlBus(VIP_CB_BITSTREAM_NAL_BITS))
		return;
	for (i = 0; i < g_readStream.segmentCount; i++, segment++) {
		uint32_t offset, remaining, length, local;

		if (cursor < segment->start)
			continue;
		offset = cursor - segment->start;
		if (offset >= segment->size) {
			g_readStream.cursor = cursor;
			return;
		}
		remaining = segment->size - offset;
		if (remaining < VIP_STREAM_HALF - filled) {
			length = remaining;
			if (remaining & 7)
				length += 8 - (remaining & 7);
		} else {
			length = VIP_STREAM_HALF - filled;
		}
		local = VIP_READ_BUFFER + filled;
		if (odd)
			local += VIP_STREAM_HALF;
		filled += length;
		list[count].external = cursor;
		cursor += length;
		list[count].localLength = local << 16 | length;
		if (length >= remaining && i == 0 && g_readStream.segmentCount != 1)
			cursor = g_readStream.segments[1].start;
		count++;
		if (filled >= VIP_STREAM_HALF)
			break;
	}
	g_readStream.cursor = cursor;
	queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ,
		(uint32_t)list << 16 | count * sizeof(VipDmaDescriptor),
		odd ? readStreamHalf1Ready : readStreamHalf0Ready, NULL);
}
