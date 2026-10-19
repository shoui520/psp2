/*
 * FW2's parser0: the parser for an H.264 elementary stream. Its start code
 * scanner finds the NAL units in the input ring and writes a record for each to
 * the NAL record ring, which the parser library (fw2/parser) reads with the
 * input. The ARM side marks the end of a picture's input and the end of the
 * stream in the input ring's header, and the scanner records those too; it also
 * ends a picture where the next access unit starts.
 *
 * The scanner works on 512-byte pieces of the input that DMA brings to bank 1,
 * from its interrupt, while the library parses. A run command keeps both going
 * (runParser) until the command's work is done. The library's NAL units come into
 * its read stream through the loader, a second overlay.
 */
#include "../parser/parser.h"
#include "../../fw1/pu0/scanner.h"

/* g_scannerState */
#define SCANNER_IDLE		0
#define SCANNER_RUNNING		1	/* a piece is on its way or being scanned */
#define SCANNER_HELD		2	/* the NAL record ring is full */
#define SCANNER_ENDED		4	/* the stream's end is recorded */

/* g_scanMode: what the scanner looks for */
#define SCAN_UNIT		0	/* a start code and the NAL header after it */
#define SCAN_UNIT_END		1	/* the next start code, or zero bytes */

/* a piece of input for the scanner (parser.h SCAN_INPUT_DMA): its size */
#define SCAN_PIECE		512

/*
 * The loader's staging in bank 1: where DMA lists put the NAL unit's data, and
 * where the local-to-local DMA takes it from
 */
#define LOAD_STAGING_DMA	0x1c00
#define LOAD_STAGING		(VIP_BANK1 + 0x1800)

/* a NAL unit longer than the input ring less this is cut */
#define UNIT_LIMIT_MARGIN	512

/* openStreams' limits */
#define INPUT_MINIMUM		0x401	/* bytes of the input ring */
#define RECORDS_MINIMUM		0x51	/* of the NAL record ring */
#define INPUT_CURSOR_LAST	4

/* the context's scanner state */
static uint16_t g_scannerConfig VIP_PROGRAM_SAVED;	/* how much of a start code it has seen */
static uint32_t g_scanEvent VIP_PROGRAM_SAVED;	/* to trace: 1, 2 when a unit was cut */
static uint32_t g_scanMode VIP_PROGRAM_SAVED;
uint32_t g_scannerState VIP_PROGRAM_SAVED;
static uint32_t g_inPicture VIP_PROGRAM_SAVED;	/* a slice has come since the last end */
static uint32_t g_unitBytes VIP_PROGRAM_SAVED;	/* scanned of the current NAL unit */
static uint32_t g_units VIP_PROGRAM_SAVED;
static uint32_t g_endRecords VIP_PROGRAM_SAVED;	/* picture and stream ends recorded */
static uint32_t g_truncatedUnits VIP_PROGRAM_SAVED;
static VipRingHandle g_inputHandle VIP_PROGRAM_SAVED;
static VipRingHandle g_recordHandle VIP_PROGRAM_SAVED;

/* a command's */
static VipDmaDescriptor g_readLists[8] VIP_PROGRAM_LOCAL __attribute__((aligned(8)));
static uint8_t g_loadBuffer[2 * VIP_STREAM_HALF] VIP_PROGRAM_LOCAL __attribute__((aligned(8)));
static uint32_t g_nextList VIP_PROGRAM_LOCAL;	/* the next piece's list request, or 0 */
static uint32_t g_listHalf VIP_PROGRAM_LOCAL;	/* the list of g_scanLists in use */
static uint32_t g_readableCount VIP_PROGRAM_LOCAL;
static uint32_t g_scanPosition VIP_PROGRAM_LOCAL;	/* the input position of the piece */
static VipRingSegment g_readable[2] VIP_PROGRAM_LOCAL;	/* of the input */
static uint32_t g_scanStart VIP_PROGRAM_LOCAL;	/* the scanner's offset when it started */
static uint32_t g_recordCursor VIP_PROGRAM_LOCAL;	/* where the next record goes */
static uint32_t g_recordSpace VIP_PROGRAM_LOCAL;
static uint32_t g_recordStart VIP_PROGRAM_LOCAL;	/* the NAL record ring's */
static uint32_t g_recordSize VIP_PROGRAM_LOCAL;
static uint32_t g_listedBytes VIP_PROGRAM_LOCAL;	/* of the input in the next piece */
static uint32_t g_inputStart VIP_PROGRAM_LOCAL;	/* the input ring's */
static uint32_t g_inputSize VIP_PROGRAM_LOCAL;
static VipDmaDescriptor g_scanLists[2][2] VIP_PROGRAM_LOCAL __attribute__((aligned(8)));

int scannerStep(void);
static void enableScannerInterrupt(void);
VIP_DRAM void runParser(void);
static VIP_DRAM uint32_t prepareList(uint32_t position);
static VIP_PROGRAM_OVERLAY void addScanRecord(uint32_t type, uint32_t position);
static VIP_PROGRAM_OVERLAY void commitRecords(void);
static VIP_DRAM void scanStart(void);
static VIP_DRAM void enableScannerClock(void);
static VIP_DRAM void gateScannerClock(void);
VIP_PROGRAM_LOADER void loadNal(void (*callback)(void));
VIP_PROGRAM_LOADER void readStreamInterrupt(void);

/* ---- in DRAM ---- */

/* VIP_PARSER_SET_OPTION: option 0 is the option byte, at the top of the value */
static int setOption(const uint32_t *arguments)
{
	if (arguments[0])
		return -1;
	g_parser.parser0.options = arguments[1] >> 24 & VIP_PARSER_OPTION_NEED_INPUT;
	return 0;
}

/*
 * Open the scanner's rings: the input, read through the given cursor, and the NAL
 * record ring, which it writes. -3: a ring is not aligned, -4: too small, -1:
 * it does not open.
 */
static int openStreams(VipRing *input, uint32_t cursor, VipRing *records)
{
	if (!input || ((uint32_t)input & 3))
		return -3;
	if (input->size < INPUT_MINIMUM || cursor < VIP_RING_WRITER + 1 ||
	    cursor > INPUT_CURSOR_LAST)
		return -4;
	if (!records || ((uint32_t)records & 3))
		return -3;
	if (records->size < RECORDS_MINIMUM)
		return -4;
	g_inputHandle.cursor = cursor;
	g_inputHandle.ring = input;
	if (openRing(&g_inputHandle, VIP_RING_OPEN_READER)) {
		g_inputHandle.ring = NULL;
		g_inputHandle.cursor = 0;
		return -1;
	}
	g_recordHandle.ring = records;
	g_recordHandle.cursor = VIP_RING_WRITER;
	if (openRing(&g_recordHandle, VIP_RING_OPEN_WRITER)) {
		closeRing(&g_inputHandle);
		g_inputHandle.ring = NULL;
		g_inputHandle.cursor = 0;
		g_recordHandle.ring = NULL;
		g_recordHandle.cursor = 0;
		return -1;
	}
	g_scannerState = SCANNER_IDLE;
	g_scanMode = SCAN_UNIT;
	g_scanEvent = 0;
	g_inPicture = 0;
	g_unitBytes = 0;
	g_units = 0;
	g_endRecords = 0;
	g_inputSize = 0;
	g_truncatedUnits = 0;
	g_scannerConfig = VIP_SCANNER_CONFIG_START;
	enableScannerInterrupt();
	return 0;
}

/*
 * VIP_PARSER_OPEN: the rings are the input elementary stream, the parsed stream,
 * its records and the NAL records. Each must be there, its data 8-byte aligned.
 */
static int openParser(VipRing *const *rings)
{
	uint32_t i;
	int result;

	for (i = 0; i < 4; i++) {
		if (!rings[i])
			return -2;
		if (rings[i]->start && (rings[i]->start & 7))
			return -3;
	}
	result = openStreams(rings[0], 2, rings[3]);
	if (result)
		return result;
	initializeDecoder();
	g_loadNal = loadNal;
	g_keepInput = 1;
	g_decoderState = VIP_DECODER_IDLE;
	return openDecoder(rings[0], 3, rings[3], 2, rings[1], rings[2]);
}

/* There is input the scanner or the library has not taken, or the stream ended. */
static int hasInput(void)
{
	const VipRing *ring = g_inputHandle.ring;
	uint32_t written = ring->cursors[VIP_RING_WRITER - 1].position;

	if (ring->readerState[1])
		return 1;
	if (written != ring->cursors[1].position)	/* the scanner's */
		return 1;
	return ring->cursors[2].position != written;	/* the library's */
}

/* Show in the context's state which rings hold the parser. */
uint32_t updateState(void)
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

/* the library's results for the ARM side */
static inline void copyStatistics(void)
{
	g_parser.pictures = g_pictures;
	g_parser.parser0.truncatedUnits = g_truncatedUnits;
	g_parser.starved = g_starved;
	g_parser.overflows = g_overflows;
}

/*
 * VIP_PARSER_RUN: scan and parse until a picture's input ends, the stream ends,
 * the input runs dry or the output is full.
 */
static int running(uint32_t kind)
{
	int result;

	if (kind != VIP_PARSER_RUN) {
		if (kind == VIP_PARSER_OPEN || kind == VIP_PARSER_SET_OPTION)
			return VIP_PARSER_WRONG_MODE;
		return VIP_PARSER_NO_COMMAND;
	}
	if ((g_parser.parser0.options & VIP_PARSER_OPTION_NEED_INPUT) && !hasInput())
		return VIP_PARSER_NO_INPUT;
	if (g_decoderOpen) {
		readDram((uint32_t)g_loaderImage, (uint32_t)g_loaderStart,
			(uint32_t)g_loaderSize);
		enableScannerInterrupt();
		setupInterrupts();
		runParser();
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

/* the commands of a closed parser */
static int opening(uint32_t *output, uint32_t kind, const uint32_t *arguments)
{
	int result;

	if (kind == VIP_PARSER_OPEN) {
		result = openParser((VipRing *const *)arguments);
		if (result) {
			*output = result;
			return VIP_PARSER_FAILED;
		}
		g_parser.mode = VIP_PARSER_OPENED;
		vipSetState(VIP_PARSER_OPENED);
		return VIP_RESULT_OK;
	}
	if (kind == VIP_PARSER_SET_OPTION) {
		if (setOption(arguments))
			return VIP_PARSER_FAILED;
		return VIP_RESULT_OK;
	}
	if (kind == VIP_PARSER_RUN)
		return VIP_PARSER_WRONG_MODE;
	return VIP_PARSER_NO_COMMAND;
}

/* VIP_PARSER_CLOSE, once the stream has ended */
static int closing(uint32_t kind)
{
	if (kind != VIP_PARSER_CLOSE)
		return VIP_PARSER_WRONG_MODE;
	stopDecoder();
	resetScanner();
	g_parser.mode = VIP_PARSER_CLOSED;
	return VIP_RESULT_OK;
}

/* The program's entry: run the command by the parser's mode. */
int parser0Entry(uint32_t *output)
{
	uint32_t *command;
	int result;

	vipSetState(g_parser.state);
	command = vipGetCommand();
	if (!command)
		return VIP_PARSER_NO_COMMAND;
	switch (g_parser.mode) {
	case VIP_PARSER_CLOSED:
		result = opening(output, command[2], command + 3);
		break;
	case VIP_PARSER_OPENED:
		result = running(command[2]);
		break;
	case VIP_PARSER_ENDED:
		result = closing(command[2]);
		break;
	default:
		result = VIP_PARSER_BAD_MODE;
		break;
	}
	g_parser.state = (g_parser.state & ~VIP_PARSER_MODE_MASK) | g_parser.mode;
	vipSetState(g_parser.state);
	return result;
}

/*
 * Start the scanner on the input it has not scanned: the first piece goes to the
 * scan buffer. Without input, record the end of the stream or of a picture if the
 * ARM side marked one (VIP_STEP_ENDED, VIP_STEP_PICTURE_END).
 */
int scannerStep(void)
{
	VipRingSegment writable[2];
	VipRing *ring;
	uint32_t streamEnd, pictureEnd, position, space, list;

	if (g_scannerState == SCANNER_RUNNING)
		return VIP_STEP_BUSY;
	if (g_scannerState == SCANNER_ENDED)
		return VIP_STEP_ENDED;
	ring = g_inputHandle.ring;
	g_inputStart = ring->start;
	g_inputSize = ring->size;
	streamEnd = ring->readerState[1];
	pictureEnd = ring->reserved;
	g_readableCount = getRingReadable(&g_inputHandle, g_readable, 2);
	position = g_readable[0].start;
	g_scanPosition = position;
	g_recordStart = g_recordHandle.ring->start;
	g_recordSize = g_recordHandle.ring->size;
	writable[1].size = 0;
	getRingWritableBeforeFirstReader(&g_recordHandle, writable, 2);
	g_recordCursor = writable[0].start;
	space = writable[0].size + writable[1].size;
	g_recordSpace = space;
	if (g_readable[0].size) {
		g_listHalf = 0;
		enableScannerClock();
		list = prepareList(position);
		if (!list) {
			gateScannerClock();
			return VIP_STEP_NOTHING;
		}
		g_scannerState = SCANNER_RUNNING;
		queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ, list, scanStart, NULL);
		return VIP_STEP_STARTED;
	}
	if (streamEnd) {
		if (space < sizeof(VipRecord) + 1)
			return VIP_STEP_NOTHING;
		addScanRecord(VIP_RECORD_STREAM_END, position);
		commitRecords();
		g_endRecords++;
		g_scannerState = SCANNER_ENDED;
		return VIP_STEP_ENDED;
	}
	if (!pictureEnd || space < sizeof(VipRecord) + 1)
		return VIP_STEP_NOTHING;
	g_inPicture = 0;
	addScanRecord(VIP_RECORD_PICTURE_END, position);
	commitRecords();
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

/* ---- in the program's code overlay ---- */

static VIP_PROGRAM_OVERLAY void scanUnitDone(void);
static VIP_PROGRAM_OVERLAY void scanForUnitEnd(void);
static VIP_PROGRAM_OVERLAY void scanUnitEndDone(void);
static VIP_PROGRAM_OVERLAY void nextPiece(uint32_t offset);
static VIP_PROGRAM_OVERLAY void stopScanner(uint32_t state);
static VIP_PROGRAM_OVERLAY int cutUnit(uint32_t offset);
static VIP_PROGRAM_OVERLAY int countUnitBytes(uint32_t read, uint32_t end);

/*
 * Run the scanner and the library until the command's work is done: the library
 * holds or has parsed a picture's input, or the parser closed. In between, sleep
 * while both wait for the hardware, and rest a while when the library keeps
 * finding nothing to do.
 */
VIP_DRAM void runParser(void)
{
	uint32_t rounds = 0, awake, state;

	g_decoderState = VIP_DECODER_IDLE;
	do {
		if (scannerStep() == VIP_STEP_PICTURE_END)
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
		if (g_scanEvent) {
			uint32_t event = g_scanEvent;

			g_scanEvent = 0;
			vipTraceEvent(event, g_endRecords);
		}
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
	while (g_scannerState == SCANNER_RUNNING)
		;
}

/* The scanner stopped: go on by what it looked for. */
VIP_PROGRAM_OVERLAY void scannerInterrupt(void)
{
	vipAcknowledgeInterrupt(VIP_INTERRUPT_SCANNER);
	if (g_scanMode == SCAN_UNIT)
		scanUnitDone();
	else
		scanUnitEndDone();
}

/*
 * List the input from position on into the scan buffer: a piece of up to 512
 * bytes, from 8-byte boundaries. The list request, or 0 if there is no input.
 */
static VIP_DRAM uint32_t prepareList(uint32_t position)
{
	const VipRingSegment *segment = g_readable;
	VipDmaDescriptor *list = g_scanLists[g_listHalf ^ 1];
	uint32_t listed = 0, filled = 0, count = 0, i;

	for (i = 0; i < g_readableCount; i++, segment++) {
		uint32_t aligned, misalignment, end, length;

		if (position < segment->start || position - segment->start >= segment->size)
			continue;
		aligned = position & ~7u;
		misalignment = position & 7;
		end = segment->start + segment->size;
		length = end - aligned;
		if (length < SCAN_PIECE - filled) {
			listed += length - misalignment;
			if (length & 7)
				length += 8 - (length & 7);
		} else {
			length = SCAN_PIECE - filled;
			listed += length - misalignment;
		}
		list[count].external = aligned;
		list[count].localLength = (SCAN_INPUT_DMA + filled) << 16 | length;
		count++;
		filled += length;
		if (filled >= SCAN_PIECE)
			break;
		position = aligned + length;
		if (position == end)
			position = g_readable[1].start;
	}
	g_listedBytes = listed;
	if (!listed)
		return 0;
	return (uint32_t)list << 16 | count * sizeof(VipDmaDescriptor);
}

/* the input position of a scanner offset in the current piece */
static VIP_PROGRAM_OVERLAY uint32_t scanPosition(uint32_t offset)
{
	int32_t distance = (g_scanPosition & ~7u) + (offset - VIP_SCANNER_INPUT) - g_inputStart;

	if (distance < 0)
		distance += g_inputSize;
	else if ((uint32_t)distance >= g_inputSize)
		distance -= g_inputSize;
	return g_inputStart + distance;
}

/* Write a record of a NAL unit (or an end) at position; commitRecords hands it over. */
static VIP_PROGRAM_OVERLAY void addScanRecord(uint32_t type, uint32_t position)
{
	VipRecord *record = (VipRecord *)g_recordCursor;

	record->header = type << VIP_RECORD_TYPE_SHIFT;
	record->pts = 0;
	record->dts = 0;
	record->position = position;
	record++;
	g_recordSpace -= sizeof(VipRecord);
	if ((uint32_t)record >= g_recordStart + g_recordSize)
		record = (VipRecord *)((uint32_t)record - g_recordSize);
	g_recordCursor = (uint32_t)record;
}

static VIP_PROGRAM_OVERLAY void commitRecords(void)
{
	VipRing *ring = g_recordHandle.ring;

	ring->writerState = 0;
	ring->cursors[VIP_RING_WRITER - 1].position = g_recordCursor;
}

/* Look for the next NAL unit: a start code and its header. */
static VIP_PROGRAM_OVERLAY void scanForUnit(void)
{
	g_scanMode = SCAN_UNIT;
	g_scanStart = vipReadControlBus(VIP_CB_SCANNER_READ);
	vipWriteControlBus(VIP_CB_SCANNER_MODE, VIP_SCANNER_MODE_FIND);
	vipWriteControlBus(VIP_CB_SCANNER_PATTERN_MASK, 0xff);
	vipWriteControlBus(VIP_CB_SCANNER_STOP_AFTER, 1);
	vipWriteControlBus(VIP_CB_SCANNER_START, 1);
}

/*
 * The scanner stopped looking for a NAL unit: record the unit it found (after a
 * picture end, if it starts the next access unit) and look for its end; or count
 * the piece's bytes if it found none.
 */
static VIP_PROGRAM_OVERLAY void scanUnitDone(void)
{
	uint32_t status = vipReadControlBus(VIP_CB_SCANNER_STATUS);
	uint32_t read = vipReadControlBus(VIP_CB_SCANNER_READ);
	uint32_t end = vipReadControlBus(VIP_CB_SCANNER_END);
	const uint8_t *header;
	uint32_t next = 0, slice, endsPicture, position, start;
	int result;

	if (status < 2) {
		result = countUnitBytes(read, end);
		if (result < -1)
			return;
		if (result < 0)
			g_scanEvent = 1;
		g_scannerConfig = vipReadControlBus(VIP_CB_SCANNER_CONFIG);
	} else {
		header = (const uint8_t *)VIP_SCANNER_ADDRESS(read - 1);
		if (read < end)
			next = header[1];
		g_unitBytes = 0;
		slice = vipNalIsSlice(header[0]);
		endsPicture = vipNalEndsPicture(header[0], next, g_inPicture);
		if (g_recordSpace < sizeof(VipRecord) << endsPicture || (read == end && slice)) {
			/* no room, or the slice's next byte is in the next piece: wait */
			start = g_scanStart;
			if (read - start >= 4) {
				start = read - 4;
				g_scannerConfig = VIP_SCANNER_CONFIG_START;
			}
			g_scanPosition = scanPosition(start);
			g_recordHandle.ring->writerState = 1;
			stopScanner(SCANNER_HELD);
			return;
		}
		position = scanPosition(read - 4);
		g_scannerConfig = VIP_SCANNER_CONFIG_START;
		if (endsPicture) {
			g_inPicture = 0;
			g_endRecords++;
			addScanRecord(VIP_RECORD_PICTURE_END, position);
		}
		if (slice)
			g_inPicture = 1;
		g_units++;
		addScanRecord(vipNalRecordType(header[0], next), position);
		commitRecords();
		g_scanMode = SCAN_UNIT_END;
	}
	if (read == end)
		nextPiece(read);
	else
		scanForUnitEnd();
}

/* Look for the end of the NAL unit: the next start code, or zero bytes. */
static VIP_PROGRAM_OVERLAY void scanForUnitEnd(void)
{
	g_scanStart = vipReadControlBus(VIP_CB_SCANNER_READ);
	g_scanMode = SCAN_UNIT_END;
	vipWriteControlBus(VIP_CB_SCANNER_MODE, VIP_SCANNER_MODE_FIND_END);
	vipWriteControlBus(VIP_CB_SCANNER_STOP_AFTER, 0);
	vipWriteControlBus(VIP_CB_SCANNER_START, 1);
}

/*
 * The scanner stopped looking for the unit's end: at zero bytes, which get a
 * record of their own, or at the next start code; then look for the next unit.
 * Or it went through the piece.
 */
static VIP_PROGRAM_OVERLAY void scanUnitEndDone(void)
{
	uint32_t status = vipReadControlBus(VIP_CB_SCANNER_STATUS);
	uint32_t read = vipReadControlBus(VIP_CB_SCANNER_READ);
	uint32_t end = vipReadControlBus(VIP_CB_SCANNER_END);
	int result;

	if (status == VIP_SCANNER_STOPPED) {
		result = countUnitBytes(read, end);
		if (result == -1)
			g_scanEvent = 2;
		else if (result == -2)
			return;
	} else if (status == (VIP_SCANNER_FOUND_ZEROS | VIP_SCANNER_STOPPED)) {
		g_unitBytes = 0;
		if (cutUnit(read - 3))
			return;
		g_scanMode = SCAN_UNIT;
	} else if (status == (VIP_SCANNER_FOUND_PREFIX | VIP_SCANNER_STOPPED)) {
		g_unitBytes = 0;
		g_scanMode = SCAN_UNIT;
	}
	g_scannerConfig = vipReadControlBus(VIP_CB_SCANNER_CONFIG);
	if (read == end)
		nextPiece(read);
	else
		scanForUnit();
}

/* The piece is scanned: start the next one, or stop until there is input. */
static VIP_PROGRAM_OVERLAY void nextPiece(uint32_t offset)
{
	g_scanPosition = scanPosition(offset);
	if (g_listedBytes)
		queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ, g_nextList,
			scanStart, NULL);
	else
		stopScanner(SCANNER_IDLE);
}

/* Give the scanned input back to the ring, and stop the scanner. */
static VIP_PROGRAM_OVERLAY void stopScanner(uint32_t state)
{
	VipRing *ring = g_inputHandle.ring;

	ring->cursors[g_inputHandle.cursor - 1].position = g_scanPosition;
	g_scannerState = state;
	gateScannerClock();
}

/*
 * End the unit at offset with a record of zero bytes (0). If the record ring is
 * full, stop until it has room (-1).
 */
static VIP_PROGRAM_OVERLAY int cutUnit(uint32_t offset)
{
	uint32_t start;

	if (g_recordSpace < sizeof(VipRecord)) {
		start = g_scanStart;
		if (start < offset)
			g_scannerConfig = VIP_SCANNER_CONFIG_START;
		else
			offset = start;
		g_scanPosition = scanPosition(offset);
		g_recordHandle.ring->writerState = 1;
		stopScanner(SCANNER_HELD);
		return -1;
	}
	addScanRecord(VIP_RECORD_AVC_ZEROS, scanPosition(offset));
	commitRecords();
	return 0;
}

/*
 * The scanner went through the piece without finding the unit's end: count its
 * bytes. A unit that would not fit the input ring is cut where this scan started
 * (-1), unless the record ring is full (-2).
 */
static VIP_PROGRAM_OVERLAY int countUnitBytes(uint32_t read, uint32_t end)
{
	uint32_t bytes = g_unitBytes + (end - read);

	if (bytes < g_inputSize - UNIT_LIMIT_MARGIN) {
		g_unitBytes = bytes;
		return 0;
	}
	if (cutUnit(g_scanStart))
		return -2;
	g_truncatedUnits++;
	g_unitBytes = end - read;
	return -1;
}

/*
 * A piece is in the scan buffer (DMA callback): scan it, and list the next one
 * meanwhile.
 */
static VIP_DRAM void scanStart(void)
{
	uint32_t listed, position, read, next;

	vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 0);
	vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 1);
	vipWriteControlBus(VIP_CB_SCANNER_CONFIG, g_scannerConfig);
	listed = g_listedBytes;
	position = g_scanPosition;
	g_listHalf ^= 1;
	read = VIP_SCANNER_INPUT + (position & 7);
	vipWriteControlBus(VIP_CB_SCANNER_READ, read);
	vipWriteControlBus(VIP_CB_SCANNER_END, read + listed);
	if (g_scanMode == SCAN_UNIT)
		scanForUnit();
	else
		scanForUnitEnd();
	next = position + listed;
	if (next >= g_inputStart + g_inputSize)
		next -= g_inputSize;
	g_nextList = prepareList(next);
}

/* the scanner's clock, on while it works */
static VIP_DRAM void enableScannerClock(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
		vipReadControlBus(VIP_CB_CLOCK_ENABLE) | VIP_CLOCK_DEMUX);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

static VIP_DRAM void gateScannerClock(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
		vipReadControlBus(VIP_CB_CLOCK_ENABLE) & ~VIP_CLOCK_DEMUX);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

/* ---- in the loader overlay ---- */

/*
 * Load the NAL unit at the read stream's cursor into the read stream buffer, a
 * half at a time by way of the staging buffer and g_loadBuffer, then run callback
 * (g_loadNal).
 */
VIP_PROGRAM_LOADER void loadNal(void (*callback)(void))
{
	VipDmaDescriptor *list = g_readLists;
	uint32_t staged = 0, half;

	for (half = 0; half < 2; half++) {
		const VipRingSegment *segment = g_readStream.segments;
		uint32_t cursor = g_readStream.cursor, filled = 0, count = 0, i;

		for (i = 0; i < g_readStream.segmentCount; i++, segment++) {
			uint32_t end, length;

			if (cursor < segment->start)
				continue;
			end = segment->start + segment->size;
			if (end == cursor)
				continue;
			cursor &= ~7u;
			length = end - cursor;
			if (length < VIP_STREAM_HALF - filled) {
				if (length & 7)
					length += 8 - (length & 7);
			} else {
				length = VIP_STREAM_HALF - filled;
			}
			list[count].external = cursor;
			list[count].localLength = (LOAD_STAGING_DMA + filled) << 16 | length;
			filled += length;
			cursor += length;
			if (cursor == end && i == 0 && g_readStream.segmentCount != 1)
				cursor = g_readStream.segments[1].start;
			count++;
			if (filled >= VIP_STREAM_HALF)
				break;
		}
		if (!filled)
			break;
		g_readStream.cursor = cursor;
		queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ,
			(uint32_t)list << 16 | count * sizeof(VipDmaDescriptor), NULL, NULL);
		list += count;
		list->external = LOAD_STAGING;
		list->localLength = (uint32_t)(g_loadBuffer + staged) << 16 | filled;
		queueListDma(VIP_DMA_LIST | VIP_DMA_EXTERNAL_LOCAL | VIP_DMA_READ,
			(uint32_t)list << 16 | sizeof(VipDmaDescriptor), NULL, NULL);
		list++;
		staged += filled;
	}
	list->localLength = VIP_READ_BUFFER << 16 | staged;
	list->external = (uint32_t)g_loadBuffer;
	queueListDma(VIP_DMA_LIST | VIP_DMA_EXTERNAL_LOCAL | VIP_DMA_WRITE,
		(uint32_t)list << 16 | sizeof(VipDmaDescriptor), callback, NULL);
}

/*
 * The read stream wants a half refilled: load the next half's worth of the
 * stream into it, as loadNal does. Past the end of the stream, or once the unit
 * is all read, the interrupt is only acknowledged.
 */
VIP_PROGRAM_LOADER void readStreamInterrupt(void)
{
	const VipRingSegment *segment = g_readStream.segments;
	uint32_t half = g_readStream.half, cursor = g_readStream.cursor;
	uint32_t filled = 0, count = 0, i;
	VipDmaDescriptor *list;
	void (*ready)(void);

	if (half & 1) {
		vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
		list = &g_readLists[4];
	} else {
		vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
		list = &g_readLists[0];
	}
	g_readStream.half = half + 1;
	if (!vipReadControlBus(VIP_CB_BITSTREAM_NAL_BITS))
		return;
	for (i = 0; i < g_readStream.segmentCount; i++, segment++) {
		uint32_t offset, remaining, length;

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
		list[count].external = cursor;
		list[count].localLength = (LOAD_STAGING_DMA + filled) << 16 | length;
		filled += length;
		cursor += length;
		if (length >= remaining && i == 0 && g_readStream.segmentCount != 1)
			cursor = g_readStream.segments[1].start;
		count++;
		if (filled >= VIP_STREAM_HALF)
			break;
	}
	g_readStream.cursor = cursor;
	queueListDma(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ,
		(uint32_t)list << 16 | count * sizeof(VipDmaDescriptor), NULL, NULL);
	list += count;
	list->external = LOAD_STAGING;
	list->localLength = (uint32_t)g_loadBuffer << 16 | filled;
	queueListDma(VIP_DMA_LIST | VIP_DMA_EXTERNAL_LOCAL | VIP_DMA_READ,
		(uint32_t)list << 16 | sizeof(VipDmaDescriptor), NULL, NULL);
	list++;
	if (half & 1) {
		ready = readStreamHalf1Ready;
		list->localLength = (VIP_READ_BUFFER + VIP_STREAM_HALF) << 16 | filled;
	} else {
		ready = readStreamHalf0Ready;
		list->localLength = VIP_READ_BUFFER << 16 | filled;
	}
	list->external = (uint32_t)g_loadBuffer;
	queueListDma(VIP_DMA_LIST | VIP_DMA_EXTERNAL_LOCAL | VIP_DMA_WRITE,
		(uint32_t)list << 16 | sizeof(VipDmaDescriptor), ready, NULL);
}
