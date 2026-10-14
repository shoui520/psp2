/*
 * PU0 task 1: the PES parser (see demux.h). Each entry point posts an event; the
 * parser then takes its pending events one at a time, lowest first, and runs the
 * handler the transition table gives for the event in its state. Before taking an
 * event it polls the input (checkInput) and runs the scanner over new input
 * (scanBuffer), which raise events of their own.
 *
 * In a PES payload the scanner copies the input to the output buffer and stops at
 * each start code; the parser queues the position of every start code it sees, so
 * that flushing the output (pesFlushAvc, pesFlushMvc) can write a record for each
 * NAL unit before its data.
 */
#include "demux.h"

#define PES_HEADER_FLAGS_SIZE		3
#define PES_FIXED_HEADER		5	/* length and flags, after the start code */
#define PES_TIMESTAMP_SIZE		5
#define PES_PTS_SIZE			5
#define PES_PTS_DTS_SIZE		10
#define PES_EXTENSION_SIZE		3

#define PES_HANDLERS			20

/* what a classifier makes of the bytes at a start code in the output */
#define NAL_WAIT			(-1)	/* more bytes are needed */
#define NAL_SKIP			0	/* no record */

typedef int (*NalClassifier)(const VipPesParser *parser, const uint8_t *bytes,
	uint32_t count, uint32_t *length);

static inline uint32_t windowAddress(uint32_t offset)
{
	return (offset & VIP_SCANNER_MASK) + VIP_SCANNER_WINDOW;
}

static inline void restartScanner(void)
{
	vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 0);
	vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 1);
}

static inline uint32_t be32(const uint8_t *bytes)
{
	return bytes[0] << 24 | bytes[1] << 16 | bytes[2] << 8 | bytes[3];
}

/* the output ring position where the next byte of output goes */
static inline uint32_t outputPosition(void)
{
	return g_demux.output.handle.ring->cursors[VIP_DEMUX_WRITER].position;
}

static inline uint32_t recordType(const VipDemuxRecord *record)
{
	return record->header >> VIP_RECORD_TYPE_SHIFT;
}

static inline void setRecordType(VipDemuxRecord *record, uint32_t type)
{
	record->header = (record->header & 0xffff) | type << VIP_RECORD_TYPE_SHIFT;
}

/* room for a record in the record ring */
static inline int recordRingFull(uint32_t margin)
{
	return (uint32_t)g_demux.recordStream.available < sizeof(VipDemuxRecord) + margin;
}

/* the producers and consumers of the buffers: the parser's transfers */

static int outputToRing(VipPesParser *parser, uint32_t local, uint32_t length)
{
	return parser->writeOutput(local, length);
}

static int recordToRing(VipPesParser *parser, uint32_t local, uint32_t length)
{
	return parser->writeRecord(local, length);
}

static int inputFromRing(VipPesParser *parser, uint32_t local, uint32_t length)
{
	return parser->readInput(local, length);
}

static int inputFromStaging(VipPesParser *parser, uint32_t local, uint32_t length)
{
	return parser->readStaged(local, length);
}

static inline int readsInput(VipPesConsumer producer)
{
	return producer == inputFromRing || producer == inputFromStaging;
}

/*
 * The position distance bytes from position, moving through the buffer's data
 * (inData) or through its free space: it wraps at the buffer's end and stops at
 * the end of the data or space. A position outside the buffer stays.
 */
uint32_t moveInBuffer(VipStreamBuffer *buffer, uint32_t inData, uint32_t position,
	int32_t distance)
{
	uint32_t target = position + distance, ahead, behind;

	if (inData) {
		ahead = buffer->in;
		behind = buffer->out;
	} else {
		ahead = buffer->out;
		behind = buffer->in;
	}
	if ((inData == 1 && !buffer->count) ||
	    (!inData && buffer->count == buffer->size))
		return position;
	if (position < buffer->start || position > buffer->end || !distance)
		return position;
	if (distance > 0) {
		if (position < ahead && ahead < target)
			return ahead;
		if (target < buffer->end)
			return target;
		target = target - buffer->end + buffer->start;
		return ahead < target ? ahead : target;
	}
	if (behind < position && target < behind)
		return behind;
	if (target >= buffer->start)
		return target;
	target = buffer->end - (buffer->start - target);
	return target < behind ? behind : target;
}

/* Copy up to count bytes of data from the peek position on; the bytes copied. */
static int peekBuffer(VipStreamBuffer *buffer, uint8_t *bytes, uint32_t count)
{
	uint32_t position = buffer->peek;
	uint32_t limit = moveInBuffer(buffer, 1, position, count);
	int copied = 0;

	while (count-- && position != limit) {
		*bytes++ = *(const uint8_t *)position++;
		buffer->peeked++;
		if (position >= buffer->end)
			position = buffer->start;
		copied++;
	}
	buffer->peek = position;
	return copied;
}

/*
 * Fill the buffer from in up to to, through producer (in two parts where it
 * wraps); what the producer last returned, or -1 if the buffer is full.
 */
int fillBuffer(VipPesParser *parser, VipStreamBuffer *buffer, uint32_t to,
	VipPesConsumer producer)
{
	uint32_t in = buffer->in, asked = 0, taken = 0, length;
	int result = 0;

	if (buffer->count == buffer->size)
		return -1;
	if (in < to) {
		asked = (uint16_t)(to - in);
		result = producer(parser, in, asked);
		if (result < 0)
			return result;
		taken = (uint16_t)result;
		buffer->count += taken;
	} else {
		if (in < buffer->end) {
			asked = (uint16_t)(buffer->end - in);
			result = producer(parser, in, asked);
			if (result < 0)
				return result;
			taken = (uint16_t)result;
			buffer->count += taken;
		}
		if (asked == (uint32_t)result && buffer->start < to &&
		    (readsInput(producer) || parser->startCodeLength != (uint32_t)result)) {
			length = (uint16_t)(to - buffer->start);
			asked = (uint16_t)(asked + length);
			result = producer(parser, buffer->start, length);
			if (result >= 1) {
				taken = (uint16_t)(taken + result);
				buffer->count += result;
			}
		}
	}
	if (!taken)
		return result;
	if (asked == taken && (readsInput(producer) || parser->startCodeLength != taken))
		buffer->in = to;
	else
		buffer->in = moveInBuffer(buffer, 0, in, taken);
	return result;
}

/*
 * Poll the input: raise VIP_PES_EVENT_TIMEOUT when the frame's time is up,
 * VIP_PES_EVENT_IDLE when the input stays empty, else move what the input ring
 * has on to the input buffer and raise VIP_PES_EVENT_DATA.
 */
static void checkInput(VipPesParser *parser)
{
	VipStreamBuffer *input = &parser->input, *staging = &parser->staging;
	int32_t available;
	uint32_t before, staged = 0;

	if (readTime() >= parser->deadline) {
		parser->events |= VIP_PES_EVENT_BIT(VIP_PES_EVENT_TIMEOUT);
		return;
	}
	available = g_demux.input.available;
	before = input->count;
	if ((available < 1 && before < 4) || parser->idleScans >= 4) {
		parser->idleScans = 0;
		parser->events |= VIP_PES_EVENT_BIT(VIP_PES_EVENT_IDLE);
		return;
	}
	if (available) {
		fillBuffer(parser, staging,
			moveInBuffer(staging, 0, staging->in, available), inputFromRing);
		flushInputList();
	}
	if (staging->count) {
		fillBuffer(parser, input,
			moveInBuffer(input, 0, input->in, staging->count), inputFromStaging);
		flushStagingList();
		staged = input->count - before;
	}
	if (staged)
		parser->stagedBytes += staged;
	else if (available < 8)
		parser->idleScans++;
	parser->events |= VIP_PES_EVENT_BIT(VIP_PES_EVENT_DATA);
}

/*
 * Look at what the scanner found, after running it over its range unless it
 * stopped at a start code last time: a scanner event's VIP_PES_EVENT_BIT(), or 0.
 * The bytes after a start code prefix tell a PES start code (our stream id) from
 * a NAL unit's (01) or zero bytes (00); in a payload they are copied to the output
 * as well.
 */
static uint32_t scanInput(VipPesParser *parser)
{
	VipStreamBuffer *input = &parser->input, *output = &parser->output, peek;
	uint32_t status, read, end, write, readBefore, writeBefore, want, value;
	uint32_t prefix, pattern, mask, matched = 0, copied, next, i;
	uint32_t event = 0;
	uint16_t written = 0;
	uint8_t bytes[4];

	status = parser->scanner.status;
	if (status & (VIP_SCANNER_FOUND_ZEROS | VIP_SCANNER_FOUND_PREFIX)) {
		read = windowAddress(vipReadControlBus(VIP_CB_SCANNER_READ));
		end = windowAddress(vipReadControlBus(VIP_CB_SCANNER_END));
		write = windowAddress(vipReadControlBus(VIP_CB_SCANNER_WRITE));
	} else {
		parser->startCodeLength = 0;
		writeBefore = windowAddress(vipReadControlBus(VIP_CB_SCANNER_WRITE));
		readBefore = windowAddress(vipReadControlBus(VIP_CB_SCANNER_READ));
		status = runScanner(parser->inPayload);
		parser->scanner.status = status;
		read = windowAddress(vipReadControlBus(VIP_CB_SCANNER_READ));
		end = windowAddress(vipReadControlBus(VIP_CB_SCANNER_END));
		write = windowAddress(vipReadControlBus(VIP_CB_SCANNER_WRITE));
		written = write - writeBefore;
		parser->scannedOutput += written;
		parser->scannedInput += read - readBefore;
		if (parser->lengthKnown && parser->payload.length >= written)
			parser->payload.length -= written;
	}
	if (read == input->end) {
		read = end = input->start;
		setScannerRange(input->start, input->start);
	}
	if (write == output->end)
		write = output->start;
	if (end == read) {
		if (parser->lengthKnown && !parser->payload.length && parser->inPayload == 1)
			return VIP_PES_EVENT_BIT(VIP_PES_EVENT_PAYLOAD_END);
		return VIP_PES_EVENT_BIT(VIP_PES_EVENT_SCAN_END);
	}
	if (!(status & (VIP_SCANNER_FOUND_ZEROS | VIP_SCANNER_FOUND_PREFIX)))
		return 0;

	peek = *input;
	peek.peek = read;
	peek.peeked = 0;
	want = status & VIP_SCANNER_FOUND_PREFIX ? 1 : 4;
	if ((uint32_t)peekBuffer(&peek, bytes, want) != want) {
		parser->startCodeLength = 3;
		return VIP_PES_EVENT_BIT(VIP_PES_EVENT_SCAN_END);
	}

	/* the bytes so far, from the prefix the scanner found: 00 00 00 or 00 00 01 */
	prefix = status & VIP_SCANNER_FOUND_ZEROS ? 0 : 1;
	pattern = (parser->scanner.pattern & 0xff) | 0x100;
	mask = parser->streamType == VIP_PES_STREAM_AVC ? 0xfffffff0 : 0xffffffff;
	value = prefix;
	for (i = 1; i <= want; i++) {
		value = value << 8 | bytes[i - 1];
		if ((value & mask) == pattern) {
			event = VIP_PES_EVENT_BIT(VIP_PES_EVENT_PACKET);
			break;
		}
	}
	if (!event) {
		value = prefix;
		for (i = 1; i <= want; i++) {
			value = value << 8 | bytes[i - 1];
			if (!value) {
				event = VIP_PES_EVENT_BIT(VIP_PES_EVENT_ZERO_CODE);
				break;
			}
			if (value == 1) {
				event = VIP_PES_EVENT_BIT(VIP_PES_EVENT_START_CODE);
				break;
			}
		}
	}
	if (event) {
		matched = i;
		parser->startCodeLength = 4;
	} else {
		event = status & VIP_SCANNER_FOUND_ZEROS ?
			VIP_PES_EVENT_BIT(VIP_PES_EVENT_ZERO_CODE) :
			VIP_PES_EVENT_BIT(VIP_PES_EVENT_START_CODE);
		parser->startCodeLength = 3;
	}

	/* move the scanner past the bytes looked at */
	if (matched && parser->inPayload == 1) {
		copied = 0;
		if (output->count + written + matched < output->size) {
			for (; copied < matched; copied++) {
				*(uint8_t *)write = bytes[copied];
				next = moveInBuffer(output, 0, write, 1);
				if (next == write)
					break;
				write = next;
			}
		}
		if (copied != matched) {
			event = VIP_PES_EVENT_BIT(VIP_PES_EVENT_SCAN_END);
		} else {
			parser->scannedOutput += copied;
			parser->scannedInput += copied;
			if (parser->lengthKnown) {
				if (parser->payload.length >= copied)
					parser->payload.length -= copied;
				if (!parser->payload.length && parser->inPayload == 1)
					event = VIP_PES_EVENT_BIT(VIP_PES_EVENT_PAYLOAD_END);
			}
			vipWriteControlBus(VIP_CB_SCANNER_WRITE, write & VIP_SCANNER_MASK);
			read = moveInBuffer(input, 1, read, matched);
			vipWriteControlBus(VIP_CB_SCANNER_READ, read & VIP_SCANNER_MASK);
		}
	} else if (matched) {
		read = moveInBuffer(input, 1, read, matched);
		vipWriteControlBus(VIP_CB_SCANNER_READ, read & VIP_SCANNER_MASK);
		parser->scannedInput += matched;
	}
	parser->scanner.status = 0;
	return event;
}

/* fillBuffer producer in a payload: the scanner copies input to the output. */
static int scanOutput(VipPesParser *parser, uint32_t local, uint32_t length)
{
	uint32_t read, end, written;

	if (parser->events && parser->events != VIP_PES_EVENT_BIT(VIP_PES_EVENT_SCAN_END))
		return 0;
	vipWriteControlBus(VIP_CB_SCANNER_WRITE, local & VIP_SCANNER_MASK);
	read = windowAddress(vipReadControlBus(VIP_CB_SCANNER_READ));
	end = windowAddress(vipReadControlBus(VIP_CB_SCANNER_END));
	if (length < end - read)
		setScannerRange(read, moveInBuffer(&parser->input, 1, read, length));
	parser->events = scanInput(parser);
	vipWriteControlBus(VIP_CB_SCANNER_END, end & VIP_SCANNER_MASK);
	written = windowAddress(vipReadControlBus(VIP_CB_SCANNER_WRITE));
	if (written >= local)
		return written - local;
	return written - local + parser->output.end - parser->output.start;
}

/*
 * drainBuffer consumer of the input buffer: scan a part of it. It consumes
 * nothing (the handlers drain the input), and returns -1 to stop at an event.
 */
int scanCallback(VipPesParser *parser, uint32_t local, uint32_t length)
{
	VipStreamBuffer *output = &parser->output;

	parser->events &= ~VIP_PES_EVENT_BIT(VIP_PES_EVENT_SCAN_END);
	setScannerRange(local, local + length);
	if (!parser->inPayload) {
		parser->events |= scanInput(parser);
	} else {
		fillBuffer(parser, output, moveInBuffer(output, 0, output->in, length),
			scanOutput);
		if (output->count >= output->size - 4)
			parser->events = VIP_PES_EVENT_BIT(VIP_PES_EVENT_OUTPUT_FULL);
	}
	return parser->events == VIP_PES_EVENT_BIT(VIP_PES_EVENT_SCAN_END) ? 0 : -1;
}

/* Scan the input buffer, when a handler asked for it (VIP_PES_RESCAN). */
static void scanBuffer(VipPesParser *parser)
{
	VipStreamBuffer *input = &parser->input;
	uint32_t to;

	if (!(parser->rescan & VIP_PES_RESCAN))
		return;
	if (parser->resumeEvents) {
		parser->events |= parser->resumeEvents;
		parser->resumeEvents = 1;
	} else {
		parser->scannedInput = 0;
		parser->scannedOutput = 0;
		if (!input->count) {
			parser->events |= VIP_PES_EVENT_BIT(VIP_PES_EVENT_SCAN_END);
		} else {
			if (parser->lengthKnown && parser->payload.length)
				to = moveInBuffer(input, 1, input->out, parser->payload.length);
			else
				to = input->in;
			drainBuffer(parser, input, to, scanCallback);
			saveScanner(&parser->scanner);
			parser->scanner.status = 0;
		}
	}
	parser->rescan &= ~VIP_PES_RESCAN;
}

/* Take the lowest pending event; VIP_PES_EVENT_NONE if there is none. */
static uint32_t takeEvent(VipPesParser *parser)
{
	uint32_t bit = 1, event;

	if ((parser->mode & VIP_PES_ACTIVE) && !parser->rescan &&
	    !(parser->events & VIP_PES_SCANNER_EVENTS))
		checkInput(parser);
	if (!(parser->events & VIP_PES_CONTROL_EVENTS))
		scanBuffer(parser);
	for (event = 0; event < VIP_PES_EVENTS; event++, bit <<= 1) {
		if (parser->events & bit) {
			parser->events &= ~bit;
			return event + 1;
		}
	}
	return VIP_PES_EVENT_NONE;
}

static int (*const g_pesHandlers[PES_HANDLERS])(VipPesParser *parser);
static const uint8_t g_pesTransitions[VIP_PES_EVENTS][VIP_PES_STATES];

static void dispatchEvent(VipPesParser *parser)
{
	uint32_t event = takeEvent(parser);

	g_pesHandlers[g_pesTransitions[event][parser->state]](parser);
}

static int postEvent(VipPesParser *parser, uint32_t event)
{
	parser->events |= VIP_PES_EVENT_BIT(event);
	dispatchEvent(parser);
	return parser->result;
}

static int runResult(const VipPesParser *parser)
{
	switch (parser->result) {
	case VIP_PES_WAITING:
		return 1;
	case VIP_PES_STOPPED:
		return 0;
	default:
		return -1;
	}
}

int pesStart(VipPesParser *parser)
{
	postEvent(parser, VIP_PES_EVENT_START);
	return 0;
}

int pesOpen(VipPesParser *parser)
{
	return postEvent(parser, VIP_PES_EVENT_OPEN) == VIP_PES_FAILED ? -1 : 1;
}

int pesClose(VipPesParser *parser)
{
	postEvent(parser, VIP_PES_EVENT_CLOSE);
	return 0;
}

/* Write out what is left at the end of the stream: 1 when done, 0 to come back. */
int pesDrain(VipPesParser *parser)
{
	postEvent(parser, VIP_PES_EVENT_DRAIN);
	return runResult(parser);
}

/*
 * Run the parser until it waits or stops: 1 when it wants more input, 0 when it
 * stopped, -1 on an error.
 */
int pesRun(VipPesParser *parser)
{
	parser->result = VIP_PES_RUNNING;
	parser->events |= VIP_PES_EVENT_BIT(VIP_PES_EVENT_RUN);
	while (parser->result == VIP_PES_RUNNING)
		dispatchEvent(parser);
	return runResult(parser);
}

/* copyInputToOutput's producer: the scanner copies from the copy source. */
static int copyFromSource(VipPesParser *parser, uint32_t local, uint32_t length)
{
	g_demux.copyParser = parser;
	copyByScanner(g_demux.copySource, local, length);
	g_demux.copySource += length;
	return length;
}

/* drainBuffer consumer: copy input bytes to the output buffer. */
static int copyInputToOutput(VipPesParser *parser, uint32_t local, uint32_t length)
{
	VipStreamBuffer *output = &parser->output;

	g_demux.copySource = local;
	fillBuffer(parser, output, moveInBuffer(output, 0, output->in, length),
		copyFromSource);
	return length;
}

/*
 * The flushes: write the output buffer to the output ring and, before each NAL
 * unit in it, a record of its type. The queue holds the NAL units' positions, and
 * the timestamp records that apply to the NAL units after them.
 */

static inline int isTimestampEntry(uint32_t entry)
{
	return entry >= VIP_PES_RECORD_PACKET && entry < VIP_PES_QUEUE_RECORDS;
}

/* whether position is in [from, to) of a circular buffer; from == to: all of it */
static int inCircularRange(uint32_t position, uint32_t from, uint32_t to)
{
	if (from < to)
		return position >= from && position < to;
	if (to < from)
		return position < to || position >= from;
	return 1;
}

/* a queued position the flush has passed: out of the data, or among bytes skipped */
static int isPassedEntry(const VipStreamBuffer *output, uint32_t entry, uint32_t from,
	uint32_t to)
{
	if (entry < output->start || entry >= output->end)
		return 0;
	if (!inCircularRange(entry, output->out, output->in))
		return 1;
	return inCircularRange(entry, from, to);
}

static uint32_t dequeue(VipPesParser *parser, uint32_t *tail)
{
	parser->queue[*tail] = 0;
	*tail = (*tail + 1) & (VIP_PES_QUEUE_LENGTH - 1);
	return parser->queue[*tail];
}

/* Write the record of a NAL unit, with the timestamps that go with it. */
static void writeNalRecord(VipPesParser *parser, uint32_t type)
{
	VipDemuxRecord *records = parser->records;

	records[0] = records[VIP_PES_RECORD_NEXT];
	setRecordType(&records[0], type);
	records[0].position = outputPosition();
	recordToRing(parser, (uint32_t)records, sizeof(VipDemuxRecord));
}

/*
 * Flush the output up to the first NAL unit that is not complete yet; -1 if the
 * output or record ring has no room. classify reads peekCount bytes at each
 * queued position.
 */
static int flushOutput(VipPesParser *parser, uint32_t peekCount, NalClassifier classify)
{
	VipStreamBuffer *output = &parser->output, peek;
	VipDemuxRecord *records = parser->records;
	uint32_t head, tail = parser->queueTail, entry, from, to, length;
	int wrote = 0, type;
	union {
		uint32_t words[2];
		uint8_t bytes[8];
	} peeked;

	if ((uint32_t)g_demux.output.available < output->count)
		return -1;
	head = parser->queueHead;
	if (head == tail) {
		if (output->count) {
			drainBuffer(parser, output, output->in, outputToRing);
			flushOutputList();
		}
		return 0;
	}
	entry = parser->queue[tail];
	do {
		if (recordRingFull(0)) {
			flushOutputList();
			return -1;
		}
		output->peek = entry;
		output->peeked = 0;
		if (isTimestampEntry(entry)) {
			records[VIP_PES_RECORD_NEXT] = records[entry];
			entry = dequeue(parser, &tail);
		} else if (entry >= output->start && entry < output->end) {
			peek = *output;
			peeked.words[0] = 0;
			peeked.words[1] = 0;
			type = classify(parser, peeked.bytes,
				peekBuffer(&peek, peeked.bytes, peekCount), &length);
			if (type == NAL_WAIT) {
				drainBuffer(parser, output, entry, outputToRing);
				flushOutputList();
				return 0;
			}
			if (type != NAL_SKIP) {
				if (entry != output->out)
					drainBuffer(parser, output, entry, outputToRing);
				writeNalRecord(parser, type);
				wrote = 1;
			}
			if (!length) {
				entry = dequeue(parser, &tail);
			} else {
				from = entry;
				to = moveInBuffer(output, 1, entry, length);
				do {
					entry = dequeue(parser, &tail);
					if (isTimestampEntry(entry))
						records[VIP_PES_RECORD_NEXT] = records[entry];
					else if (!isPassedEntry(output, entry, from, to))
						break;
				} while (tail != head);
			}
		}
		parser->queueTail = tail;
	} while (tail != head);
	if (!wrote && output->count)
		drainBuffer(parser, output, output->in, outputToRing);
	if (g_demux.outputListCount)
		flushOutputList();
	return 0;
}

/*
 * H.264: the record type of the NAL unit at a start code (00 00 01 or 00 00 00
 * 01), with length the bytes up to what identifies it; or zero bytes.
 */
static int classifyAvc(const VipPesParser *parser, const uint8_t *bytes, uint32_t count,
	uint32_t *length)
{
	uint32_t word = be32(bytes), skip, zeros = 0;

	*length = 0;
	if (count < 3)
		return NAL_WAIT;
	if (word == 1 && count >= 4) {
		skip = 4;
	} else if (!word && count >= 4) {
		skip = 1;
		zeros = 1;
	} else if ((word & ~0xffu) == 0x100) {
		skip = 3;
	} else if (word) {
		return NAL_SKIP;
	} else {
		return NAL_WAIT;
	}
	if (skip >= count)
		return NAL_WAIT;
	*length = skip;
	if (zeros) {
		if (recordType(&parser->records[0]) == VIP_RECORD_AVC_ZEROS)
			return NAL_SKIP;
		return VIP_RECORD_AVC_ZEROS;
	}
	*length = skip + 1;
	switch (bytes[skip] & 0x1f) {
	case 1:
	case 5:
		if (skip + 1 >= count)
			return NAL_WAIT;
		*length = skip + 2;
		/* first_mb_in_slice is 0: the slice starts a picture */
		return bytes[skip + 1] & 0x80 ? VIP_RECORD_AVC_PICTURE : VIP_RECORD_AVC_SLICE;
	case 6:
		return VIP_RECORD_AVC_SEI;
	case 7:
		return VIP_RECORD_AVC_SPS;
	case 8:
		return VIP_RECORD_AVC_PPS;
	case 9:
		return VIP_RECORD_AVC_DELIMITER;
	case 10:
		return VIP_RECORD_AVC_END_SEQUENCE;
	case 11:
		return VIP_RECORD_AVC_END_STREAM;
	default:
		return VIP_RECORD_AVC_OTHER;
	}
}

/* the MVC record types of the NAL header bytes 10-15 */
static const uint16_t g_mvcRecordTypes[6] = {
	0x3408, 0x3406, 0x3405, 0x3404, 0x3403, 0x3402,
};

/* MVC: as classifyAvc, by the whole NAL header byte. */
static int classifyMvc(const VipPesParser *parser, const uint8_t *bytes, uint32_t count,
	uint32_t *length)
{
	uint32_t word = be32(bytes), skip, zeros = 0, header;

	*length = 0;
	if (count < 3)
		return NAL_WAIT;
	if (!(word & ~0xffu)) {
		skip = 1;
		zeros = 1;
	} else if ((word & ~0xffu) == 0x100) {
		skip = 3;
	} else {
		return NAL_SKIP;
	}
	if (skip >= count)
		return NAL_WAIT;
	*length = skip;
	if (zeros) {
		if (recordType(&parser->records[0]) == VIP_RECORD_MVC_ZEROS)
			return NAL_SKIP;
		return VIP_RECORD_MVC_ZEROS;
	}
	*length = skip + 1;
	header = bytes[skip];
	if (header >= 0x20)
		return VIP_RECORD_MVC_OTHER;
	if (header >= 0x1b)
		return VIP_RECORD_MVC_RESERVED;
	if (header - 10 < 6)
		return g_mvcRecordTypes[header - 10];
	return VIP_RECORD_MVC_OTHER;
}

int pesFlushAvc(VipPesParser *parser)
{
	return flushOutput(parser, 6, classifyAvc);
}

int pesFlushMvc(VipPesParser *parser)
{
	return flushOutput(parser, 5, classifyMvc);
}

/* the handlers, by their number in the transition table */

static int waitForInput(VipPesParser *parser)
{
	parser->result = VIP_PES_WAITING;
	return 0;
}

static int failEvent(VipPesParser *parser)
{
	parser->result = VIP_PES_FAILED;
	return 0;
}

static int stopParser(VipPesParser *parser)
{
	parser->state = VIP_PES_IDLE;
	parser->result = VIP_PES_WAITING;
	parser->mode = 0;
	return 0;
}

static int resetParser(VipPesParser *parser)
{
	uint32_t i;

	switch (parser->streamType) {
	case VIP_PES_STREAM_AVC:
		parser->scanner.pattern = (parser->scanner.pattern & ~0xffu) |
			VIP_PES_STREAM_ID_AVC;
		parser->flush = pesFlushAvc;
		break;
	case VIP_PES_STREAM_MVC:
		parser->scanner.pattern = (parser->scanner.pattern & ~0xffu) |
			VIP_PES_STREAM_ID_MVC;
		parser->flush = pesFlushMvc;
		break;
	default:
		parser->result = VIP_PES_FAILED;
		return 0;
	}
	parser->scanner.mode = 0;
	parser->scanner.read = 0;
	parser->scanner.end = 0;
	parser->scanner.config = 0;
	parser->scanner.write = 0;
	parser->scanner.status = 0;
	parser->startCodeLength = 0;
	parser->scannedOutput = 0;
	parser->scannedInput = 0;
	parser->inPayload = 0;
	parser->resumeEvents = 0;
	parser->queueHead = 0;
	parser->queueTail = 0;
	for (i = 0; i < VIP_PES_QUEUE_LENGTH; i++)
		parser->queue[i] = 0;
	parser->endPending = 0;
	parser->zeroRun = 0xff;
	parser->timestampRecord = VIP_PES_RECORD_TIMESTAMPS;
	for (i = 0; i < 3; i++)
		parser->records[i].header &= 0xffff;
	parser->mode = VIP_PES_ACTIVE;
	parser->result = VIP_PES_WAITING;
	parser->state = VIP_PES_OPEN;
	parser->events = 0;
	return 0;
}

static int startScanning(VipPesParser *parser)
{
	restartScanner();
	restoreScanner(&parser->scanner);
	parser->state = VIP_PES_SCANNING;
	parser->mode = VIP_PES_ACTIVE;
	parser->idleScans = 0;
	parser->stagedBytes = 0;
	return 0;
}

/*
 * The end of the stream: write out the rest of the input and output, then the
 * record for what ends it (zero bytes, or a start code with nothing after it), and
 * the record that starts the next stream.
 */
static int finishStream(VipPesParser *parser)
{
	VipDemuxRecord *records = parser->records;
	VipStreamBuffer *input = &parser->input, *output = &parser->output, peek;
	uint32_t startType, zerosType, otherType, before, i;
	uint16_t zeros = 0, zeroStart = 0;
	uint8_t byte;

	if (parser->streamType == VIP_PES_STREAM_AVC) {
		otherType = VIP_RECORD_AVC_OTHER;
		zerosType = VIP_RECORD_AVC_ZEROS;
		startType = VIP_RECORD_AVC_START;
	} else {
		otherType = VIP_RECORD_MVC_OTHER;
		zerosType = VIP_RECORD_MVC_ZEROS;
		startType = VIP_RECORD_MVC_START;
	}
	if (input->count) {
		if (parser->inPayload == 1) {
			if (parser->flush(parser) < 0)
				goto stopped;
			before = output->count;
			drainBuffer(parser, input, input->in, copyInputToOutput);
			queueStartCodes(parser, output->in, -(int32_t)(output->count - before),
				output->count - before);
		} else {
			drainBuffer(parser, input, input->in, skipBytes);
		}
	}
	do {
		before = output->count;
		if (parser->flush(parser) < 0)
			goto stopped;
	} while (before != output->count);

	if (output->count) {
		peek = *output;
		for (i = 0; i < output->count; i++) {
			if (peekBuffer(&peek, &byte, 1) != 1)
				continue;
			if (!byte) {
				if (parser->zeroRun) {
					zeroStart = i;
					zeros = 0;
					parser->zeroRun = 0;
				}
				zeros++;
			}
			parser->zeroRun |= byte;
		}
		if (!parser->zeroRun) {
			if (recordType(&records[0]) != zerosType) {
				parser->endPending = 1;
				setRecordType(&records[0], zerosType);
				records[0].position = outputPosition() + zeroStart;
			}
		} else if (zeros >= 2 && parser->zeroRun == 1) {
			parser->endPending = 1;
			setRecordType(&records[0], otherType);
			records[0].position = outputPosition() + zeroStart;
		}
		drainBuffer(parser, output, output->in, outputToRing);
		if (output->count)
			goto stopped;
		parser->queueHead = 0;
		parser->queueTail = 0;
	}
	if (parser->endPending == 1) {
		if (recordRingFull(1))
			goto stopped;
		recordToRing(parser, (uint32_t)records, sizeof(VipDemuxRecord));
		records[0].header &= 0xffff;
		parser->endPending = 0;
	}
	setRecordType(&records[0], startType);
	records[0].position = outputPosition();
	if (recordRingFull(1))
		goto stopped;
	recordToRing(parser, (uint32_t)records, sizeof(VipDemuxRecord));
	records[0].header &= 0xffff;
	parser->endPending = 0;
	parser->zeroRun = 0xff;
	parser->result = VIP_PES_WAITING;
	parser->state = VIP_PES_OPEN;
	return 0;
stopped:
	parser->result = VIP_PES_STOPPED;
	return 0;
}

static int resumeScanning(VipPesParser *parser)
{
	restartScanner();
	restoreScanner(&parser->scanner);
	parser->stagedBytes = 0;
	return 0;
}

static int stopBlocked(VipPesParser *parser)
{
	saveScanner(&parser->scanner);
	parser->result = VIP_PES_STOPPED;
	parser->mode = VIP_PES_STOPPED_BLOCKED;
	parser->idleScans = 0;
	parser->events &= ~VIP_PES_EVENT_BIT(VIP_PES_EVENT_IDLE);
	return 0;
}

static int requestRescan(VipPesParser *parser)
{
	parser->rescan |= VIP_PES_RESCAN;
	return 0;
}

static int stopEmpty(VipPesParser *parser)
{
	saveScanner(&parser->scanner);
	parser->result = VIP_PES_STOPPED;
	parser->mode = VIP_PES_STOPPED_EMPTY;
	return 0;
}

static int stopTimeout(VipPesParser *parser)
{
	saveScanner(&parser->scanner);
	parser->result = VIP_PES_STOPPED;
	parser->mode = VIP_PES_STOPPED_TIMEOUT;
	return 0;
}

/* Restart the scanner where the parser left it, past the start code it found. */
static void resumeScanner(VipPesParser *parser)
{
	vipWriteControlBus(VIP_CB_SCANNER_CONFIG, VIP_SCANNER_CONFIG_RESTART);
	restartScanner();
	parser->scanner.config &= VIP_SCANNER_CONFIG_KEEP;
	restoreScanner(&parser->scanner);
	parser->scanner.status = 0;
}

/* The output or record ring is full: retry the event when it has room. */
static int waitForRoom(VipPesParser *parser, uint32_t event)
{
	parser->resumeEvents = VIP_PES_EVENT_BIT(event);
	parser->events |= VIP_PES_EVENT_BIT(VIP_PES_EVENT_BLOCKED);
	return 0;
}

/* The payload of a PES packet with a length is done. */
static int endPayload(VipPesParser *parser)
{
	VipStreamBuffer *output = &parser->output;
	uint32_t count;

	if (!parser->resumeEvents) {
		drainBuffer(parser, &parser->input,
			windowAddress(vipReadControlBus(VIP_CB_SCANNER_READ)), skipBytes);
		count = parser->scannedOutput;
		parser->inPayload = 0;
		if (count > 4)
			count = 4;
		queueStartCodes(parser, output->in, -(int32_t)count, count);
	}
	parser->resumeEvents = 0;
	if (output->count && parser->flush(parser) < 0)
		return waitForRoom(parser, VIP_PES_EVENT_PAYLOAD_END);
	parser->inPayload = 0;
	parser->scannedInput = 0;
	resumeScanner(parser);
	parser->state = VIP_PES_OPEN;
	parser->result = VIP_PES_WAITING;
	return 0;
}

/* A PES start code with our stream id: read the packet's header next. */
static int startPacket(VipPesParser *parser)
{
	VipStreamBuffer *input = &parser->input;
	uint32_t start;

	start = moveInBuffer(input, 1, parser->scanner.read,
		-(int32_t)parser->startCodeLength);
	if (start != input->out) {
		drainBuffer(parser, input, start, skipBytes);
		parser->scannedInput = parser->startCodeLength;
	}
	parser->idleScans = 0;
	input->peeked = 0;
	input->peek = parser->scanner.read;
	resumeScanner(parser);
	parser->state = VIP_PES_PACKET_LENGTH;
	return 0;
}

/* A NAL unit's start code in the payload: queue where it starts. */
static int startCodeFound(VipPesParser *parser)
{
	VipStreamBuffer *output = &parser->output;
	int32_t distance = -3;
	uint32_t count = 1;
	int flush = 0;

	parser->idleScans = 0;
	if (parser->inPayload == 1) {
		if (parser->streamType == VIP_PES_STREAM_MVC) {
			if (parser->startCodeLength == 4) {
				distance = -4;
				count = 2;
			}
			flush = queueStartCodes(parser, output->in, distance, count);
		} else if (parser->streamType == VIP_PES_STREAM_AVC) {
			if (parser->startCodeLength == 4) {
				distance = -4;
			} else if ((int16_t)parser->scannedInput >= 4) {
				distance = -4;
				count = 2;
			}
			flush = queueStartCodes(parser, output->in, distance, count);
		}
	}
	drainBuffer(parser, &parser->input, parser->scanner.read, skipBytes);
	parser->scannedInput = 0;
	resumeScanner(parser);
	if (flush && parser->flush(parser) < 0)
		parser->result = VIP_PES_STOPPED;
	return 0;
}

/* Zero bytes after a start code prefix: they belong to the stream. */
static int zeroCodeFound(VipPesParser *parser)
{
	VipStreamBuffer *input = &parser->input, *output = &parser->output;
	int32_t scanned = (int16_t)parser->scannedInput;
	uint32_t length = parser->startCodeLength, position;
	int flush;

	parser->idleScans = 0;
	if (parser->inPayload == 1) {
		flush = queueStartCodes(parser, output->in, -(int32_t)length, 1);
		if (scanned >= 3) {
			position = moveInBuffer(input, 1, input->out, 1 - (length - scanned));
			if (position != input->out)
				drainBuffer(parser, input, position, skipBytes);
		}
		length--;
		if (length) {
			output->in = moveInBuffer(output, 1, output->in, -(int32_t)length);
			output->count -= length;
			if (parser->lengthKnown)
				parser->payload.length += length;
		}
		if (flush && parser->flush(parser) < 0)
			parser->result = VIP_PES_STOPPED;
	} else {
		position = moveInBuffer(input, 1, input->out, scanned);
		if (position != input->out)
			drainBuffer(parser, input, position, skipBytes);
	}
	restartScanner();
	restoreScanner(&parser->scanner);
	return 0;
}

/*
 * Drop the input the scanner passed but its last four bytes, which may begin a
 * start code; in a payload take them back out of the output.
 */
static void keepStartCodeTail(VipPesParser *parser, uint32_t scanned)
{
	VipStreamBuffer *input = &parser->input, *output = &parser->output;
	uint32_t position, count;

	if (scanned >= 4) {
		position = moveInBuffer(input, 1, input->out, scanned - 4);
		if (position != input->out)
			drainBuffer(parser, input, position, skipBytes);
	}
	if (parser->inPayload == 1) {
		count = scanned < 4 ? scanned : 4;
		if (count) {
			output->in = moveInBuffer(output, 1, output->in, -(int32_t)count);
			output->count -= count;
			if (parser->lengthKnown)
				parser->payload.length += count;
		}
	}
}

/* The output buffer filled up: flush it, keeping a possible start code. */
static int outputFull(VipPesParser *parser)
{
	uint32_t scanned = parser->scannedInput, count;

	if (!parser->resumeEvents) {
		keepStartCodeTail(parser, scanned);
		if (parser->inPayload == 1) {
			count = parser->scannedOutput < 4 ? parser->scannedOutput : 3;
			queueStartCodes(parser, parser->output.in, -(int32_t)count, count);
		}
	}
	parser->resumeEvents = 0;
	if (parser->flush(parser) < 0)
		return waitForRoom(parser, VIP_PES_EVENT_OUTPUT_FULL);
	parser->scannedInput = scanned >= 4 ? scanned - 4 : scanned;
	resumeScanner(parser);
	return 0;
}

/* The scanner reached the end of the input: rescan the last four bytes later. */
static int scanEnd(VipPesParser *parser)
{
	uint32_t scanned = parser->scannedInput;

	keepStartCodeTail(parser, scanned);
	parser->scannedInput = scanned >= 4 ? scanned - 4 : scanned;
	resumeScanner(parser);
	return 0;
}

/* VIP_PES_PACKET_LENGTH: read PES_packet_length. */
static int readPacketLength(VipPesParser *parser)
{
	VipStreamBuffer *input = &parser->input;

	if (input->count - parser->scannedInput - input->peeked >= 2) {
		peekBuffer(input, (uint8_t *)&parser->packet.length, 2);
		parser->state = VIP_PES_HEADER_FLAGS;
	}
	return 0;
}

/* The header is read: what is left of the packet is its payload. */
static void startBody(VipPesParser *parser)
{
	uint32_t skip = 0;

	parser->idleScans = 0;
	parser->lengthKnown = parser->packet.length > 0;
	if (parser->packet.length)
		skip = parser->packet.headerLength - parser->startCodeLength - 2;
	parser->packet.length -= skip;
	parser->payload = parser->packet;
}

/*
 * A packet started in the middle of a payload whose packet had no length: drop
 * the new start code from the output and flush the payload.
 */
static int endUnboundedPayload(VipPesParser *parser)
{
	VipStreamBuffer *output = &parser->output;
	uint32_t count;

	parser->inPayload = 0;
	output->in = moveInBuffer(output, 1, output->in, -(int32_t)parser->startCodeLength);
	output->count -= parser->startCodeLength;
	count = (uint16_t)(parser->scannedOutput - parser->startCodeLength);
	if (count >= 4)
		count = 3;
	queueStartCodes(parser, output->in, -(int32_t)count, count);
	if (output->count && parser->flush(parser) < 0)
		return waitForRoom(parser, VIP_PES_EVENT_DATA);
	parser->result = VIP_PES_WAITING;
	parser->state = VIP_PES_OPEN;
	return 0;
}

/* A retried endUnboundedPayload: its flush. */
static int retryFlush(VipPesParser *parser)
{
	parser->resumeEvents = 0;
	if (parser->flush(parser) < 0)
		return waitForRoom(parser, VIP_PES_EVENT_DATA);
	parser->result = VIP_PES_WAITING;
	parser->state = VIP_PES_OPEN;
	return 0;
}

/* After the header: skip it and scan the payload, record (its timestamps) first. */
static void startPayload(VipPesParser *parser, uint32_t record)
{
	VipStreamBuffer *input = &parser->input;

	parser->queue[parser->queueHead] = record;
	parser->queueHead = (parser->queueHead + 1) & (VIP_PES_QUEUE_LENGTH - 1);
	drainBuffer(parser, input,
		moveInBuffer(input, 1, input->out, parser->payload.headerLength), skipBytes);
	if (parser->lengthKnown == 1 && !parser->payload.length) {
		vipWriteControlBus(VIP_CB_SCANNER_READ, input->out & VIP_SCANNER_MASK);
		parser->events |= VIP_PES_EVENT_BIT(VIP_PES_EVENT_PAYLOAD_END);
	}
	parser->inPayload = 1;
	parser->state = VIP_PES_SCANNING;
}

/* VIP_PES_HEADER_FLAGS: the flag bytes and the header length. */
static int readHeaderFlags(VipPesParser *parser)
{
	VipStreamBuffer *input = &parser->input;
	VipDemuxRecord *record;
	uint32_t errors = 0;

	if (parser->resumeEvents)
		return retryFlush(parser);
	if (input->count - parser->scannedInput - input->peeked < PES_HEADER_FLAGS_SIZE)
		return 0;
	peekBuffer(input, parser->header, PES_HEADER_FLAGS_SIZE);
	parser->packet.headerSize = 0;
	if ((parser->header[0] & 0xc0) != 0x80)
		errors = VIP_PES_ERROR_MARKER;
	if ((parser->header[0] >> 4 & 3) > 1)
		errors |= VIP_PES_ERROR_SCRAMBLED;
	switch (parser->header[1] >> 6) {
	case 3:
		parser->packet.headerSize = PES_PTS_DTS_SIZE;
		break;
	case 2:
		parser->packet.headerSize = PES_PTS_SIZE;
		break;
	case 1:
		errors |= VIP_PES_ERROR_TIMESTAMP_FLAGS;
		parser->packet.headerSize = PES_PTS_SIZE;
		break;
	}
	if (parser->header[1] & 1)
		parser->packet.headerSize += PES_EXTENSION_SIZE;
	if (errors) {
		parser->badHeaders++;
		parser->errors |= errors;
	}
	parser->packet.headerLength = parser->startCodeLength + PES_FIXED_HEADER +
		parser->header[2];
	if (parser->packet.headerSize) {
		parser->state = VIP_PES_HEADER;
		return 0;
	}
	startBody(parser);
	if (parser->inPayload == 1)
		return endUnboundedPayload(parser);
	if (parser->inPayload)
		return 0;
	parser->packets++;
	record = &parser->records[VIP_PES_RECORD_PACKET];
	record->header = 0;
	record->pts = 0;
	record->dts = 0;
	record->position = 0;
	startPayload(parser, VIP_PES_RECORD_PACKET);
	return 0;
}

/* a PTS or DTS: 33 bits in 5 bytes, with marker bits */
static uint32_t timestampLow(const uint8_t *bytes)
{
	return (uint32_t)(bytes[0] >> 1 & 3) << 30 |
		(uint32_t)((bytes[1] << 8 | bytes[2]) >> 1) << 15 |
		(bytes[3] << 8 | bytes[4]) >> 1;
}

static uint32_t timestampHigh(const uint8_t *bytes)
{
	return bytes[0] >> 3 & 1;
}

/* VIP_PES_HEADER: the timestamps, into the record the payload's NAL units take. */
static int readTimestamps(VipPesParser *parser)
{
	VipStreamBuffer *input = &parser->input, peek;
	VipDemuxRecord *timestamps;
	uint8_t pts[PES_TIMESTAMP_SIZE] = { 0 }, dts[PES_TIMESTAMP_SIZE] = { 0 };
	uint32_t flags = parser->header[1] >> 6, errors = 0, record, header;

	if (parser->resumeEvents)
		return retryFlush(parser);
	if ((uint16_t)(input->count - parser->scannedInput - input->peeked) <
	    parser->packet.headerSize)
		return 0;
	peek = *input;
	if (flags & 2) {
		peekBuffer(&peek, pts, PES_TIMESTAMP_SIZE);
		if ((pts[0] & 0xc1) != 0x01 || !(pts[2] & 1))
			errors = VIP_PES_ERROR_PTS;
		if (!(pts[4] & 1))
			errors |= VIP_PES_ERROR_PTS_MARKER;
		if (flags != (uint32_t)(pts[0] >> 4))
			errors |= VIP_PES_ERROR_PTS_PREFIX;
	}
	if (flags & 1) {
		peekBuffer(&peek, dts, PES_TIMESTAMP_SIZE);
		if ((dts[0] & 0xf1) != 0x11 || !(dts[2] & 1))
			errors |= VIP_PES_ERROR_DTS;
		if (!(dts[4] & 1))
			errors |= VIP_PES_ERROR_DTS_MARKER;
	}
	if ((int32_t)input->count - ((int32_t)parser->scannedInput - parser->startCodeLength) <
	    parser->packet.headerLength)
		return 0;
	*input = peek;
	if (errors) {
		parser->badTimestamps++;
		parser->errors |= errors;
	}
	startBody(parser);
	if (parser->inPayload == 1)
		return endUnboundedPayload(parser);
	if (parser->inPayload)
		return 0;
	parser->packets++;
	record = parser->timestampRecord == VIP_PES_RECORD_PACKET ?
		VIP_PES_RECORD_TIMESTAMPS : VIP_PES_RECORD_PACKET;
	parser->timestampRecord = record;
	timestamps = &parser->records[record];
	header = timestamps->header & 0xffff;
	header = flags & 2 ? header | VIP_RECORD_PTS : header & ~VIP_RECORD_PTS;
	header = flags & 1 ? header | VIP_RECORD_DTS : header & ~VIP_RECORD_DTS;
	header = (header & ~VIP_RECORD_PTS_HIGH) | timestampHigh(pts) << 13;
	header = (header & ~VIP_RECORD_DTS_HIGH) | timestampHigh(dts) << 12;
	timestamps->header = header & 0xfffff000;
	timestamps->pts = timestampLow(pts);
	timestamps->dts = timestampLow(dts);
	timestamps->position = 0;
	startPayload(parser, record);
	return 0;
}

static int (*const g_pesHandlers[PES_HANDLERS])(VipPesParser *parser) = {
	waitForInput,		/* 0 */
	failEvent,
	stopParser,
	resetParser,
	startScanning,
	finishStream,		/* 5 */
	resumeScanning,
	stopBlocked,
	requestRescan,
	stopEmpty,
	stopTimeout,		/* 10 */
	endPayload,
	startPacket,
	startCodeFound,
	zeroCodeFound,
	outputFull,		/* 15 */
	scanEnd,
	readPacketLength,
	readHeaderFlags,
	readTimestamps,
};

/* the handler of each event (rows) in each state (columns) */
static const uint8_t g_pesTransitions[VIP_PES_EVENTS][VIP_PES_STATES] = {
	[VIP_PES_EVENT_NONE]		= { 0, 0, 0, 0, 0, 0, 0 },
	[VIP_PES_EVENT_START]		= { 2, 0, 2, 2, 2, 2, 2 },
	[VIP_PES_EVENT_OPEN]		= { 1, 3, 0, 0, 0, 0, 0 },
	[VIP_PES_EVENT_RUN]		= { 1, 1, 4, 6, 6, 6, 6 },
	[VIP_PES_EVENT_CLOSE]		= { 1, 1, 2, 2, 2, 2, 2 },
	[VIP_PES_EVENT_DRAIN]		= { 1, 1, 5, 5, 5, 5, 5 },
	[VIP_PES_EVENT_BLOCKED]		= { 1, 1, 1, 7, 1, 7, 7 },
	[VIP_PES_EVENT_DATA]		= { 1, 1, 1, 8, 17, 18, 19 },
	[VIP_PES_EVENT_IDLE]		= { 1, 1, 1, 9, 9, 9, 9 },
	[VIP_PES_EVENT_TIMEOUT]		= { 1, 1, 1, 10, 10, 10, 10 },
	[VIP_PES_EVENT_PAYLOAD_END]	= { 1, 1, 1, 11, 1, 1, 1 },
	[VIP_PES_EVENT_PACKET]		= { 1, 1, 1, 12, 1, 1, 1 },
	[VIP_PES_EVENT_START_CODE]	= { 1, 1, 1, 13, 1, 1, 1 },
	[VIP_PES_EVENT_ZERO_CODE]	= { 1, 1, 1, 14, 1, 1, 1 },
	[VIP_PES_EVENT_OUTPUT_FULL]	= { 1, 1, 1, 15, 1, 1, 1 },
	[VIP_PES_EVENT_SCAN_END]	= { 1, 1, 1, 16, 1, 1, 1 },
};
