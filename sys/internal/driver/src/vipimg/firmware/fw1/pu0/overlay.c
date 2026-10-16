/*
 * PU0 task 1: its code overlay, the functions the parser calls most. The scheduler
 * copies them into local RAM (VIP_TASK_CODE) when it switches the task in.
 */
#include "demux.h"

/* Run the parser for this frame. */
VIP_TASK1_TEXT void runParser(VipPesParser *parser)
{
	g_demux.outputWritten = 0;
	while (parser->run(parser) == 1)
		;
}

/*
 * Tell the debugger, once each, when the input ring runs empty or full, and hold
 * its writer while it is full if the debugger asked for that.
 */
VIP_TASK1_TEXT void reportInputLevel(void)
{
	VipDemuxParameters *parameters = VIP_DEMUX_PARAMETERS;
	uint32_t space;

	if ((g_demux.mode >> VIP_DEMUX_MODE_LEVELS_SHIFT & VIP_DEMUX_MODE_LEVELS_MASK) !=
	    VIP_DEMUX_MODE_LEVELS)
		return;
	space = g_demux.input.handle.ring->size - g_demux.input.available;
	if (space >= g_demux.emptySpace && !g_demux.emptyReported) {
		parameters->empty = 1;
		if (g_demux.notifyEmpty) {
			notifyDebugger(0, VIP_DEMUX_EVENT_EMPTY);
			g_demux.fullReported = 0;
			g_demux.emptyReported = 1;
		}
	}
	if (space <= g_demux.fullSpace && !g_demux.fullReported) {
		parameters->full = 1;
		if (g_demux.notifyFull) {
			notifyDebugger(0, VIP_DEMUX_EVENT_FULL);
			g_demux.emptyReported = 0;
			g_demux.fullReported = 1;
		}
		if (g_demux.holdWhenFull)
			setRingReaderState0(&g_demux.input.handle, 1);
	}
}

/* Queue a DMA list that has transfers in it. */
static inline void flushList(uint32_t control, VipDmaDescriptor *list, uint32_t *count,
	volatile uint32_t *busy)
{
	if (!*count)
		return;
	*busy = VIP_DEMUX_LIST_IDLE;
	queueListDma(control, (uint32_t)list << 16 | *count * sizeof(VipDmaDescriptor),
		NULL, busy);
	*count = 0;
}

/* the input ring to the staging buffer in bank 1 */
VIP_TASK1_TEXT int flushInputList(void)
{
	flushList(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ, g_demux.inputList,
		&g_demux.inputListCount, &g_demux.inputListBusy);
	return 0;
}

/* the staging buffer to the input buffer */
VIP_TASK1_TEXT int flushStagingList(void)
{
	flushList(VIP_DMA_LIST | VIP_DMA_EXTERNAL_LOCAL | VIP_DMA_READ, g_demux.stagingList,
		&g_demux.stagingListCount, &g_demux.stagingListBusy);
	return 0;
}

/* the output buffer to the output ring */
VIP_TASK1_TEXT int flushOutputList(void)
{
	flushList(VIP_DMA_LIST | VIP_DMA_WRITE, g_demux.outputList,
		&g_demux.outputListCount, &g_demux.outputListBusy);
	return 0;
}

/*
 * Give the buffer's data from out up to to to consumer (in two parts where it
 * wraps), and take what it consumed out of the buffer: what the consumer last
 * returned, or -1 if the buffer is empty.
 */
VIP_TASK1_TEXT int drainBuffer(VipPesParser *parser, VipStreamBuffer *buffer, uint32_t to,
	VipPesConsumer consumer)
{
	uint32_t start = buffer->start, end = buffer->end, out = buffer->out;
	uint32_t asked = 0, taken = 0, length;
	int result = 0;

	if (!buffer->count)
		return -1;
	if (out < to) {
		asked = (uint16_t)(to - out);
		result = consumer(parser, out, asked);
		if (result < 0)
			return result;
		taken = (uint16_t)result;
	} else if (to < out || buffer->count == buffer->size) {
		if (out < end) {
			asked = (uint16_t)(end - out);
			result = consumer(parser, out, asked);
			if (result < 0)
				return result;
			taken = (uint16_t)result;
		}
		if ((consumer == scanCallback || asked == (uint32_t)result) && start < to) {
			length = (uint16_t)(to - start);
			asked = (uint16_t)(asked + length);
			result = consumer(parser, start, length);
			if (result >= 1)
				taken = (uint16_t)(taken + result);
		}
	}
	if (taken) {
		if (asked == taken)
			buffer->out = to;
		else
			buffer->out = moveInBuffer(buffer, 1, out, taken);
		buffer->count -= taken;
	}
	return result;
}

/* drainBuffer consumer: drop the bytes. */
VIP_TASK1_TEXT int skipBytes(VipPesParser *parser, uint32_t local, uint32_t length)
{
	g_demux.copyParser = parser;
	g_demux.lastByte = *(const uint8_t *)local;
	return length;
}

static inline uint32_t windowAddress(uint32_t offset)
{
	return (offset & VIP_SCANNER_MASK) + VIP_SCANNER_WINDOW;
}

VIP_TASK1_TEXT void saveScanner(VipScannerState *state)
{
	state->mode = vipReadControlBus(VIP_CB_SCANNER_MODE);
	state->read = windowAddress(vipReadControlBus(VIP_CB_SCANNER_READ));
	state->end = windowAddress(vipReadControlBus(VIP_CB_SCANNER_END));
	state->pattern = vipReadControlBus(VIP_CB_SCANNER_PATTERN);
	state->config = vipReadControlBus(VIP_CB_SCANNER_CONFIG);
	state->write = windowAddress(vipReadControlBus(VIP_CB_SCANNER_WRITE));
	state->status = vipReadControlBus(VIP_CB_SCANNER_STATUS);
}

VIP_TASK1_TEXT void restoreScanner(const VipScannerState *state)
{
	vipWriteControlBus(VIP_CB_SCANNER_MODE, state->mode & VIP_SCANNER_MASK);
	vipWriteControlBus(VIP_CB_SCANNER_READ, state->read & VIP_SCANNER_MASK);
	vipWriteControlBus(VIP_CB_SCANNER_END, state->end & VIP_SCANNER_MASK);
	vipWriteControlBus(VIP_CB_SCANNER_PATTERN, state->pattern);
	vipWriteControlBus(VIP_CB_SCANNER_CONFIG, state->config);
	vipWriteControlBus(VIP_CB_SCANNER_WRITE, state->write & VIP_SCANNER_MASK);
}

/* the scanner's input: from read up to end */
VIP_TASK1_TEXT void setScannerRange(uint32_t read, uint32_t end)
{
	vipWriteControlBus(VIP_CB_SCANNER_READ, read & VIP_SCANNER_MASK);
	vipWriteControlBus(VIP_CB_SCANNER_END, end & VIP_SCANNER_MASK);
}

/*
 * Run the scanner over its range once the DMA lists, which share local RAM with
 * it, are done; in a payload it copies to the output as well. Its status.
 */
VIP_TASK1_TEXT uint32_t runScanner(uint32_t inPayload)
{
	vipWriteControlBus(VIP_CB_SCANNER_MODE,
		inPayload ? VIP_SCANNER_MODE_PAYLOAD : VIP_SCANNER_MODE_SEARCH);
	while (g_demux.outputListBusy == 1 || g_demux.inputListBusy == 1 ||
	       g_demux.stagingListBusy == 1)
		;
	vipWriteControlBus(VIP_CB_SCANNER_START, 1);
	while (vipReadControlBus(VIP_CB_SCANNER_START))
		;
	return vipReadControlBus(VIP_CB_SCANNER_STATUS);
}

/*
 * Queue the zero bytes among count bytes of the output, from distance bytes from
 * position on: each may begin a start code. -1: the output must be flushed (the
 * original's test of the queue's fill always says so).
 */
VIP_TASK1_TEXT int queueStartCodes(VipPesParser *parser, uint32_t position,
	int32_t distance, uint32_t count)
{
	VipStreamBuffer *output = &parser->output;
	uint32_t in = output->in, start = output->start, end = output->end;

	position = moveInBuffer(output, 1, position, distance);
	for (; count; count--, position++) {
		if (position == in)
			break;
		if (position >= end)
			position = start;
		if (!*(const uint8_t *)position) {
			parser->queue[parser->queueHead] = position;
			parser->queueHead = (parser->queueHead + 1) & (VIP_PES_QUEUE_LENGTH - 1);
		}
	}
	return -1;
}
