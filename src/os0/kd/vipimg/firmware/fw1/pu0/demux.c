/*
 * PU0 task 1: the demultiplexer task (see demux.h), its rings and its transfers.
 *
 * Each frame the task loads its state from DRAM, takes the debugger's open and
 * close requests (debug point 0), and while it is open copies the rings' shared
 * cursors into local copies, runs the parser until it waits or its time is up,
 * and writes the cursors back.
 */
#include "demux.h"

/* drainInput's state, kept by the task between frames */
#define DRAIN_START		0
#define DRAIN_RUNNING		(-1)
#define DRAIN_FINISH		(-2)

/* storeStreams: which reader states go back to the shared rings */
#define STORE_READER_STATE0	0xf0
#define STORE_READER_STATE1	0x0f

/* the time a frame gives the parser, and what the rest of the frame needs */
#define DEMUX_MARGIN_AVC	0x3db60
#define DEMUX_MARGIN_MVC	0xe978

/* the input and output buffers, the task's data overlay */
static uint8_t g_demuxInputBuffer[VIP_DEMUX_BUFFER_SIZE] VIP_TASK1_DATA;
static uint8_t g_demuxOutputBuffer[VIP_DEMUX_BUFFER_SIZE] VIP_TASK1_DATA;

/* the task's state while it runs, in local RAM */
VipDemux g_demux VIP_TASK1_BSS;

#define DEMUX_BUFFER(buffer, bytes) {					\
	.size = VIP_DEMUX_BUFFER_SIZE,					\
	.start = (uint32_t)(buffer),					\
	.end = (uint32_t)(buffer) + (bytes),				\
	.in = (uint32_t)(buffer),					\
	.out = (uint32_t)(buffer),					\
}

/* where the task's state is kept between frames, and how it starts */
static VipDemux g_demuxHome = {
	.input.handle.cursor = -1,
	.output.handle.cursor = -1,
	.recordStream.handle.cursor = -1,
	.inputListBusy = VIP_DEMUX_LIST_IDLE,
	.stagingListBusy = VIP_DEMUX_LIST_IDLE,
	.outputListBusy = VIP_DEMUX_LIST_IDLE,
	.parser = {
		.mode = 0x100,
		.deadline = 0xa0000,
		.reserved0 = 0x17000,
		.scanner.pattern = VIP_PES_STREAM_ID_AVC,
		.start = pesStart,
		.open = pesOpen,
		.run = pesRun,
		.close = pesClose,
		.drain = pesDrain,
		.flush = pesFlushAvc,
		.readInput = readInputByDma,
		.readStaged = readStagedByDma,
		.writeOutput = writeOutputByDma,
		.writeRecord = writeRecord,
		.input = DEMUX_BUFFER(g_demuxInputBuffer, VIP_DEMUX_BUFFER_SIZE),
		.staging = DEMUX_BUFFER(VIP_DEMUX_STAGING, VIP_DEMUX_BUFFER_SIZE),
		.output = DEMUX_BUFFER(g_demuxOutputBuffer, VIP_DEMUX_BUFFER_SIZE),
		.timestampRecord = VIP_PES_RECORD_TIMESTAMPS,
		.zeroRun = 0xff,
		.records = g_demux.records,
	},
};

/* the argument the task was started with */
uint32_t g_demuxArgument;

/* the time of a frame, by readTime() */
uint32_t g_demuxFrameTime = 0x330000;

/* Set the three rings from debug point 0's values and open them; 1 on a failure. */
static int openStreams(VipDemuxStream *input)
{
	VipDemuxStream *output = &g_demux.output;
	VipDemuxStream *records = &g_demux.recordStream;
	uint32_t ring;
	int result;

	getDebugPointValue(VIP_DEMUX_POINT, VIP_DEMUX_POINT_INPUT, &ring);
	setRing(&input->handle, (VipRing *)ring);
	getDebugPointValue(VIP_DEMUX_POINT, VIP_DEMUX_POINT_OUTPUT, &ring);
	setRing(&output->handle, (VipRing *)ring);
	getDebugPointValue(VIP_DEMUX_POINT, VIP_DEMUX_POINT_RECORDS, &ring);
	setRing(&records->handle, (VipRing *)ring);
	result = openRing(&input->handle, VIP_RING_OPEN_READER);
	result |= openRing(&output->handle, VIP_RING_OPEN_WRITER);
	result |= openRing(&records->handle, VIP_RING_OPEN_WRITER);
	return result & 1;
}

/* Close the three rings; 1 on a failure. */
static int closeStreams(VipDemuxStream *input)
{
	int result;

	result = closeRing(&input->handle);
	result |= closeRing(&g_demux.output.handle);
	result |= closeRing(&g_demux.recordStream.handle);
	return result & 1;
}

/*
 * The input ring's writer has started: read from where it is, a byte at a time
 * until the reader is 8-byte aligned, and let the record ring's reader go.
 */
static int openInput(VipPesParser *parser, VipDemuxStream *input)
{
	VipDemuxStream *records = &g_demux.recordStream;
	uint32_t misalignment;

	ringSkipToWriter(&input->handle);
	g_demux.input.available = getRingAvailable(&input->handle);
	if (input->handle.ring->cursors[VIP_DEMUX_READER].position & 7) {
		parser->readStaged = readStagedByCopy;
		parser->readInput = readInputByCopy;
	}
	parser->start(parser);
	parser->open(parser);
	misalignment = input->handle.ring->cursors[VIP_DEMUX_READER].position & 7;
	parser->input.in = (uint32_t)g_demuxInputBuffer | misalignment;
	resetBuffers(parser);
	setRingReaderState0(&records->handle, 1);
	setRingReaderState0(&input->handle, 0);
	setRingReaderState1(&input->handle, 0);
	return 0;
}

/*
 * The input's writer has finished: parse what is left, a byte at a time near the
 * end, and write out the rest. DRAIN_RUNNING while the parser stopped and needs
 * another frame, else DRAIN_START when the input is drained.
 */
static int drainInput(VipPesParser *parser, VipDemuxStream *input, int state)
{
	VipStreamBuffer *output = &parser->output;
	uint32_t misalignment;

	if (state == DRAIN_FINISH)
		goto finish;
	if (state != DRAIN_RUNNING) {
		misalignment = g_demux.output.handle.ring->cursors[VIP_DEMUX_WRITER].position & 7;
		if (misalignment != (output->out & 7)) {
			output->out |= misalignment;
			output->count -= misalignment;
			g_demux.output.available -= misalignment;
			g_demux.outputWritten += misalignment;
		}
		parser->writeOutput = writeOutputByCopy;
		parser->readStaged = readStagedByCopy;
		parser->readInput = readInputByCopy;
	}
	for (;;) {
		if (!g_demux.input.available && parser->mode == VIP_PES_STOPPED_EMPTY) {
			while (parser->run(parser) == 1)
				;
			if (parser->mode == VIP_PES_STOPPED_BLOCKED ||
			    parser->mode == VIP_PES_STOPPED_TIMEOUT)
				return DRAIN_RUNNING;
			break;
		}
		parser->run(parser);
		if (parser->mode == VIP_PES_STOPPED_BLOCKED ||
		    parser->mode == VIP_PES_STOPPED_TIMEOUT)
			return DRAIN_RUNNING;
	}
finish:
	if (parser->drain(parser) != 1)
		return DRAIN_RUNNING;
	setRingReaderState1(&input->handle, 0);
	parser->writeOutput = writeOutputByDma;
	parser->readStaged = readStagedByDma;
	parser->readInput = readInputByDma;
	return DRAIN_START;
}

/*
 * Empty the buffers, aligning them to 8 bytes for DMA; the output ring's writer
 * moves on to the next multiple of 8.
 */
void resetBuffers(VipPesParser *parser)
{
	VipRingCursor *writer;

	parser->output.count = 0;
	parser->staging.peeked = 0;
	parser->staging.count = 0;
	parser->stagedBytes = 0;
	parser->input.peeked = 0;
	parser->input.count = 0;
	parser->staging.in = parser->staging.out & ~7;
	parser->staging.out = parser->staging.in;
	parser->input.out = parser->input.in;
	writer = &g_demux.output.handle.ring->cursors[VIP_DEMUX_WRITER];
	advanceRingWriter(&g_demux.output.handle, -writer->position & 7);
	parser->output.peeked = 0;
	parser->output.in = parser->output.out & ~7;
	parser->output.out = parser->output.in;
}

/* Copy the shared rings into their local copies. */
static int copyRings(void)
{
	g_demux.input.local = *g_demux.input.handle.ring;
	g_demux.output.local = *g_demux.output.handle.ring;
	g_demux.recordStream.local = *g_demux.recordStream.handle.ring;
	return 0;
}

/* VipPesParser.writeRecord: a record to the record ring. */
int writeRecord(uint32_t local, uint32_t length)
{
	const VipDemuxRecord *record = (const VipDemuxRecord *)local;
	VipRing *ring = g_demux.recordStream.handle.ring;
	VipRingCursor *writer = &ring->cursors[VIP_DEMUX_WRITER];
	VipDemuxRecord *slot = (VipDemuxRecord *)writer->position;

	slot->header = record->header;
	slot->pts = record->pts;
	slot->dts = record->dts;
	slot->position = record->position;
	if (writer->position + sizeof(VipDemuxRecord) < ring->start + ring->size)
		writer->position += sizeof(VipDemuxRecord);
	else
		writer->position = ring->start;
	g_demux.recordStream.available -= length;
	return length;
}

/*
 * VipPesParser.readInput while the input ring's reader is not 8-byte aligned: the
 * bytes up to the next multiple of 8, by the core.
 */
int readInputByCopy(uint32_t local, uint32_t length)
{
	VipRing *ring = g_demux.input.handle.ring;
	VipRingCursor *reader = &ring->cursors[VIP_DEMUX_READER];
	uint32_t count = length & 7, offset, i;
	uint8_t *destination = (uint8_t *)local;

	offset = reader->position - ring->start + 7;
	for (i = 0; i < count; i++, offset--) {
		destination[7 - i] = *(const uint8_t *)(ring->start +
			(offset < ring->size ? offset : offset - ring->size));
	}
	if (advanceRingReader(&g_demux.input.handle, count) < 0)
		return 0;
	g_demux.input.available -= count;
	reportInputLevel();
	if (!(reader->position & 7))
		g_demux.parser.readInput = readInputByDma;
	return count;
}

/* VipPesParser.readStaged, likewise. */
int readStagedByCopy(uint32_t local, uint32_t length)
{
	VipStreamBuffer *staging = &g_demux.parser.staging;
	uint32_t count = length & 7, offset = staging->out - staging->start, i;
	uint8_t *destination = (uint8_t *)local;

	while (g_demux.inputListBusy == 1)
		;
	for (i = 0; i < count; i++, offset++) {
		if (offset >= staging->size)
			offset = 0;
		destination[i] = *(const uint8_t *)(staging->start + offset +
			VIP_DEMUX_STAGING_BUS);
	}
	staging->out = moveInBuffer(staging, 1, staging->out, count);
	staging->count -= count;
	if (!(staging->out & 7))
		g_demux.parser.readStaged = readStagedByDma;
	return count;
}

/* Copy count bytes to the ring at its writer, by the core; -1 on a failure. */
static int writeRingBytes(VipRingHandle *handle, uint32_t local, uint32_t count)
{
	VipRing *ring = handle->ring;
	uint32_t offset = ring->cursors[0].position - ring->start, i;
	const uint8_t *source = (const uint8_t *)local;

	for (i = 0; i < count; i++, offset++) {
		if (offset >= ring->size)
			offset = 0;
		*(uint8_t *)(ring->start + offset) = source[i];
	}
	if (advanceRingWriter(handle, count) < 0)
		return -1;
	return count;
}

/*
 * VipPesParser.writeOutput while the stream drains: the bytes by the core up to a
 * multiple of 32 of the local address, then by writeRing, then the rest by the
 * core.
 */
int writeOutputByCopy(uint32_t local, uint32_t length)
{
	VipRingHandle *handle = &g_demux.output.handle;
	uint32_t written = 0, count;
	int result;

	if ((uint32_t)getRingSpace(handle) >= length) {
		count = -local & 0x1f;
		if (length < count)
			count = length;
		if (count) {
			result = writeRingBytes(handle, local, count);
			if (result < 0)
				goto done;
			written += result;
			local += result;
			length -= result;
		}
		if (length & ~0x1f) {
			result = writeRing(handle, local, length & ~0x1f);
			if (result < 0)
				goto done;
			written += result;
			local += result;
			length -= result;
		}
		if (length & 0x1f) {
			result = writeRingBytes(handle, local, length);
			if (result >= 0)
				written += result;
		}
	}
done:
	g_demux.output.available -= written;
	return written;
}

/* The parser's time for this frame: from now to the frame's end, less a margin. */
static void getRemainingTime(uint32_t streamType, uint32_t *remaining)
{
	uint32_t now;

	if (streamType != VIP_PES_STREAM_AVC && streamType != VIP_PES_STREAM_MVC) {
		*remaining = 0;
		return;
	}
	now = readTime();
	if (streamType == VIP_PES_STREAM_AVC)
		*remaining = g_demuxFrameTime - (now + DEMUX_MARGIN_AVC);
	else
		*remaining = g_demuxFrameTime - (now + DEMUX_MARGIN_MVC);
}

/*
 * Copy length bytes of local RAM from source to destination with the scanner,
 * keeping where it was scanning.
 */
uint32_t copyByScanner(uint32_t source, uint32_t destination, uint32_t length)
{
	uint32_t config, read, end;

	config = vipReadControlBus(VIP_CB_SCANNER_CONFIG);
	read = (vipReadControlBus(VIP_CB_SCANNER_READ) & VIP_SCANNER_MASK) +
		VIP_SCANNER_WINDOW;
	end = (vipReadControlBus(VIP_CB_SCANNER_END) & VIP_SCANNER_MASK) +
		VIP_SCANNER_WINDOW;
	vipWriteControlBus(VIP_CB_SCANNER_WRITE, destination & VIP_SCANNER_MASK);
	setScannerRange(source, source + length);
	vipWriteControlBus(VIP_CB_SCANNER_MODE, VIP_SCANNER_MODE_COPY);
	vipWriteControlBus(VIP_CB_SCANNER_START, 1);
	while (vipReadControlBus(VIP_CB_SCANNER_START))
		;
	setScannerRange(read, end);
	vipWriteControlBus(VIP_CB_SCANNER_CONFIG, config);
	return length;
}

static int handleDebugPoint(VipPesParser *parser, VipDemuxStream *input);
static int loadStreams(void);
static int storeStreams(uint32_t flags);
static int readMode(VipDemux *demux);
static void readParameters(VipDemux *demux);

/*
 * The task: each frame, run the parser while the debugger has the demultiplexer
 * open and the rings are going, and save its state for the next frame.
 */
void demuxTask(uint32_t argument)
{
	VipPesParser *parser = &g_demux.parser;
	VipDemuxStream *input = &g_demux.input;
	uint32_t *statistics = (uint32_t *)VIP_DEMUX_STATISTICS;
	int draining = DRAIN_START, result;

	g_demuxArgument = argument;
	readDram((uint32_t)&g_demuxHome, (uint32_t)&g_demux, sizeof(g_demux));
	parser->start(parser);
	for (;;) {
		g_demux.emptyReported = 0;
		g_demux.fullReported = 0;
		result = handleDebugPoint(parser, input);
		if (result) {
			setDebugPointResult(VIP_DEMUX_POINT, result);
			disableDebugPoint(VIP_DEMUX_POINT);
		}
		if ((parser->mode & VIP_PES_ACTIVE) && g_demux.state == VIP_DEMUX_OPEN &&
		    needsRestart()) {
			loadStreams();
			readParameters(&g_demux);
			reportInputLevel();
			if (input->handle.ring->readerState[0]) {
				openInput(parser, input);
				storeStreams(0x11);
			} else if ((uint32_t)g_demux.input.available < 8 &&
				   (parser->mode == VIP_PES_STOPPED_EMPTY || draining) &&
				   input->handle.ring->readerState[1]) {
				enableDemuxClocks();
				draining = drainInput(parser, input, draining);
				gateDemuxClocks();
				storeStreams(1);
			} else if (!g_demux.recordStream.handle.ring->readerState[0]) {
				getRemainingTime(parser->streamType, &parser->deadline);
				parser->deadline += readTime();
				enableDemuxClocks();
				runParser(parser);
				gateDemuxClocks();
				storeStreams(0);
			} else {
				parser->mode = VIP_PES_STOPPED_BLOCKED;
				storeStreams(0);
			}
		}
		statistics[0] = parser->packets;
		statistics[1] = parser->badHeaders;
		statistics[2] = parser->badTimestamps;
		statistics[3] = parser->errors;
		statistics[4] = parser->reserved1;
		writeDram((uint32_t)&g_demuxHome, (uint32_t)&g_demux, sizeof(g_demux));
		setPauseFlag(argument, parser->mode);
		vipTaskYield();
		readDram((uint32_t)&g_demuxHome, (uint32_t)&g_demux, sizeof(g_demux));
	}
}

/*
 * Take the debugger's request on debug point 0: open (its values are the three
 * rings) or close the demultiplexer, over as many frames as the rings need. 0
 * while there is nothing to report, else the point's result: 1 done, 3 a bad
 * request, 4 the wrong number of values, 5 the wrong state.
 */
static int handleDebugPoint(VipPesParser *parser, VipDemuxStream *input)
{
	uint32_t action, argument, count;

	if (checkDebugPoint(VIP_DEMUX_POINT, &action, &argument, &count) < 0)
		return 0;
	switch (action) {
	case 0:
		if (!(g_demux.state & (VIP_DEMUX_OPENING | VIP_DEMUX_CLOSING)))
			return 0;
		break;
	case VIP_DEMUX_ACTION_OPEN:
		if (count != VIP_DEMUX_OPEN_COUNT)
			return 4;
		if (argument != 1)
			return 3;
		if (parser->mode)
			return 5;
		g_demux.state = VIP_DEMUX_OPENING;
		break;
	case VIP_DEMUX_ACTION_CLOSE:
		if (count != VIP_DEMUX_CLOSE_COUNT)
			return 4;
		if (argument != 1)
			return 3;
		if (!(parser->mode & VIP_PES_ACTIVE))
			return 5;
		g_demux.state = VIP_DEMUX_CLOSING;
		break;
	default:
		return 3;
	}
	if (g_demux.state == VIP_DEMUX_CLOSING) {
		if (closeStreams(input))
			return 0;
		parser->close(parser);
		releaseSchedule(VIP_DEMUX_POINT);
		g_demux.state = VIP_DEMUX_CLOSED;
		return 1;
	}
	if (g_demux.state != VIP_DEMUX_OPENING)
		return 3;
	if (openStreams(input))
		return 0;
	copyRings();
	parser->streamType = readMode(&g_demux);
	parser->input.in = (uint32_t)g_demuxInputBuffer;
	parser->output.out = (uint32_t)g_demuxOutputBuffer;
	resetBuffers(parser);
	parser->packets = 0;
	parser->badHeaders = 0;
	parser->badTimestamps = 0;
	parser->errors = 0;
	parser->open(parser);
	holdSchedule(VIP_DEMUX_POINT);
	g_demux.state = VIP_DEMUX_OPEN;
	return 1;
}

/* Copy the cursors of each ring the task reads into its local copy; use those. */
static int loadStreams(void)
{
	VipDemuxStream *input = &g_demux.input, *output = &g_demux.output;
	VipDemuxStream *records = &g_demux.recordStream;
	const VipRing *ring;

	ring = input->handle.ring;
	input->local.readerState[0] = ring->readerState[0];
	input->local.readerState[1] = ring->readerState[1];
	input->local.cursors[0].open = ring->cursors[0].open;
	input->local.cursors[0].position = ring->cursors[0].position;
	input->local.cursors[2].open = ring->cursors[2].open;
	input->local.cursors[2].position = ring->cursors[2].position;
	ring = output->handle.ring;
	output->local.cursors[1].open = ring->cursors[1].open;
	output->local.cursors[1].position = ring->cursors[1].position;
	output->local.cursors[2].open = ring->cursors[2].open;
	output->local.cursors[2].position = ring->cursors[2].position;
	ring = records->handle.ring;
	records->local.readerState[0] = ring->readerState[0];
	records->local.cursors[1].open = ring->cursors[1].open;
	records->local.cursors[1].position = ring->cursors[1].position;
	records->local.cursors[2].open = ring->cursors[2].open;
	records->local.cursors[2].position = ring->cursors[2].position;

	input->shared = input->handle.ring;
	output->shared = output->handle.ring;
	records->shared = records->handle.ring;
	records->handle.ring = &records->local;
	output->handle.ring = &output->local;
	input->handle.ring = &input->local;
	input->available = getRingAvailable(&input->handle);
	output->available = getRingSpace(&output->handle);
	records->available = getRingSpace(&records->handle);
	return 0;
}

/*
 * When the DMA lists are done, copy the task's own cursors back to the shared
 * rings, and the reader states flags names.
 */
static int storeStreams(uint32_t flags)
{
	VipDemuxStream *input = &g_demux.input, *output = &g_demux.output;
	VipDemuxStream *records = &g_demux.recordStream;

	while (g_demux.outputListBusy == 1 || g_demux.inputListBusy == 1)
		;
	while (g_demux.stagingListBusy == 1)
		;
	input->shared->cursors[VIP_DEMUX_READER].position =
		input->local.cursors[VIP_DEMUX_READER].position;
	output->shared->cursors[VIP_DEMUX_WRITER].position =
		output->local.cursors[VIP_DEMUX_WRITER].position;
	records->shared->cursors[VIP_DEMUX_WRITER].position =
		records->local.cursors[VIP_DEMUX_WRITER].position;
	input->handle.ring = input->shared;
	output->handle.ring = output->shared;
	records->handle.ring = records->shared;
	if (flags & STORE_READER_STATE0) {
		input->handle.ring->readerState[0] = input->local.readerState[0];
		records->handle.ring->readerState[0] = records->local.readerState[0];
	}
	if (flags & STORE_READER_STATE1)
		input->handle.ring->readerState[1] = input->local.readerState[1];
	return 0;
}

/*
 * VipPesParser.readInput: list a DMA transfer of whole 8-byte units from the input
 * ring to the staging buffer, in two parts where the ring wraps.
 */
int readInputByDma(uint32_t local, uint32_t length)
{
	VipRing *ring = g_demux.input.handle.ring;
	VipRingCursor *reader = &ring->cursors[VIP_DEMUX_READER];
	uint32_t count = length & ~7, offset, first, position;
	VipDmaDescriptor *list;

	if (!count)
		return 0;
	while (g_demux.inputListBusy == 1)
		;
	while (g_demux.stagingListBusy == 1)
		;
	list = &g_demux.inputList[g_demux.inputListCount];
	position = reader->position;
	offset = position - ring->start;
	if (ring->size < count + offset) {
		first = ring->size - offset;
		list[0].external = position;
		list[0].localLength = local << 16 | first;
		list[1].external = ring->start;
		list[1].localLength = (local + first) << 16 | (count - first);
		g_demux.inputListCount += 2;
		position = count - first + ring->start;
	} else {
		list[0].external = position;
		list[0].localLength = local << 16 | count;
		g_demux.inputListCount++;
		position = reader->position + count;
		if (ring->start + ring->size == position)
			position = ring->start;
	}
	reader->position = position;
	g_demux.input.available -= count;
	reportInputLevel();
	return count;
}

/* VipPesParser.readStaged: likewise, from the staging buffer to the input buffer. */
int readStagedByDma(uint32_t local, uint32_t length)
{
	VipStreamBuffer *staging = &g_demux.parser.staging;
	uint32_t count = length & ~7, offset, first, out;
	VipDmaDescriptor *list;

	if (!count)
		return 0;
	while (g_demux.stagingListBusy == 1)
		;
	while (g_demux.inputListBusy == 1)
		;
	list = &g_demux.stagingList[g_demux.stagingListCount];
	offset = staging->out - staging->start;
	if (staging->size < count + offset) {
		first = staging->size - offset;
		list[0].external = staging->out + VIP_DEMUX_STAGING_BUS;
		list[0].localLength = local << 16 | first;
		list[1].external = staging->start + VIP_DEMUX_STAGING_BUS;
		list[1].localLength = (local + first) << 16 | (count - first);
		g_demux.stagingListCount += 2;
		out = count - first + staging->start;
	} else {
		list[0].external = staging->out + VIP_DEMUX_STAGING_BUS;
		list[0].localLength = local << 16 | count;
		g_demux.stagingListCount++;
		out = staging->out + count;
		if (staging->start + staging->size == out)
			out = staging->start;
	}
	staging->out = out;
	staging->count -= count;
	return count;
}

/*
 * VipPesParser.writeOutput: list a DMA transfer from the output buffer to the
 * output ring, padded to 8 bytes. The ring's writer moves on by length; the padding
 * is written again with the next transfer, which starts at the writer rounded down.
 */
int writeOutputByDma(uint32_t local, uint32_t length)
{
	VipRing *ring = g_demux.output.handle.ring;
	VipRingCursor *writer = &ring->cursors[VIP_DEMUX_WRITER];
	uint32_t start = ring->start, size = ring->size;
	uint32_t total = length + (-length & 7), count = length & ~7;
	uint32_t offset, first, position;
	VipDmaDescriptor *list;

	writer->position &= ~7;
	while (g_demux.outputListBusy == 1)
		;
	if (g_demux.outputListCount >= VIP_DEMUX_OUTPUT_LIST) {
		flushOutputList();
		while (g_demux.outputListBusy == 1)
			;
	}
	list = &g_demux.outputList[g_demux.outputListCount];
	position = writer->position;
	offset = position - start;
	if (size < offset + total) {
		first = size - offset;
		list[0].external = position;
		list[0].localLength = local << 16 | first;
		list[1].external = start;
		list[1].localLength = (local + first) << 16 | (total - first);
		g_demux.outputListCount += 2;
		position = length - first + start;
	} else {
		list[0].external = position;
		list[0].localLength = local << 16 | total;
		g_demux.outputListCount++;
		position = writer->position + length;
		if (start + size == position)
			position = start;
	}
	writer->position = position;
	g_demux.output.available -= count;
	g_demux.outputWritten += count;
	return count;
}

/* The demultiplexer's mode and input levels; 1 if the stream is MVC. */
static int readMode(VipDemux *demux)
{
	demux->fullSpace = VIP_DEMUX_PARAMETERS->fullSpace;
	demux->emptySpace = VIP_DEMUX_PARAMETERS->emptySpace;
	readDebugRegister(VIP_DEMUX_MODE_REGISTER, &demux->mode);
	return (demux->mode & VIP_DEMUX_MODE_STREAM) == VIP_DEMUX_MODE_MVC;
}

static void readParameters(VipDemux *demux)
{
	demux->holdWhenFull = VIP_DEMUX_PARAMETERS->holdWhenFull;
	demux->notifyFull = VIP_DEMUX_PARAMETERS->notifyFull;
	demux->notifyEmpty = VIP_DEMUX_PARAMETERS->notifyEmpty;
}
