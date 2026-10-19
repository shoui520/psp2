/*
 * PU1 task 0: the picture decoder's command task. The ARM side opens the decoder
 * through the task's debug point (0x20) with five rings, the work memory and the
 * frame stores; while it is open the task runs the codec's decode each time the
 * schedule comes round, and on close gives everything back. It also answers how
 * much work memory a stream of a given size needs (queryWorkMemory).
 */
#include "../common/debug.h"
#include "codec.h"

/* the debug point's commands besides common/debug.h's */
#define COMMAND_QUERY		3

/* their sizes */
#define COMMAND_OPEN_COUNT	0x34
#define COMMAND_QUERY_COUNT	0x14
#define COMMAND_QUERY_ARGUMENT	2

/* the open command's values */
#define VALUE_WORK_MEMORY	5
#define VALUE_WORK_SIZE		6
#define VALUE_FRAMES		7	/* VIP_DECODER_FRAMES VipFrame */
#define VALUE_FRAMES_SIZE	8
#define VALUE_EXTENSIONS	9	/* VIP_DECODER_FRAMES VipFrameExtension */
#define VALUE_EXTENSIONS_SIZE	10

/* the query command's values: width, height, references in; the size out */
#define VALUE_QUERY_SIZE	3

/* the debugger's events from the task */
#define EVENT_DECODER_ENDED	0x00001000
#define EVENT_DECODER_STOPPED	0x00008000
#define EVENT_DECODER_ROUND	0x00800000	/* the task ran */

/* the debug point's ring values, in VipDecoder.rings order */
static const uint8_t g_ringValues[VIP_DECODER_RINGS] = {
	[VIP_RING_SLICES] = 0,
	[VIP_RING_RECORDS] = 1,
	[VIP_RING_USER_DATA] = 2,
	[VIP_RING_USER_RECORDS] = 3,
	[VIP_RING_OUTPUT] = 4,
};

VipDecoder g_decoder;
VipOutputState g_output;
static VipOpenSettings g_openSettings;

/* Point the decoder at the rings and frame stores of the open command. */
static void takeOpenValues(uint32_t point, uint32_t *frames, uint32_t *workSize)
{
	uint32_t value, i;

	for (i = 0; i < VIP_DECODER_RINGS; i++) {
		getDebugPointValue(point, g_ringValues[i], &value);
		setRing(&g_decoder.rings[i], (VipRing *)value);
	}
	getDebugPointValue(point, VALUE_WORK_MEMORY, &value);
	g_decoder.workMemory = value;
	getDebugPointValue(point, VALUE_WORK_SIZE, workSize);
	getDebugPointValue(point, VALUE_FRAMES, frames);
}

/*
 * Check the open command's sizes and choose the memory mode; 0 if the decoder can
 * open.
 */
static int checkOpenValues(uint32_t point, VipDecoderArea *area, uint32_t frames,
	uint32_t workSize)
{
	uint32_t value, kind, i;

	for (i = 0; i < VIP_DECODER_FRAMES; i++)
		setFrameStore(&g_decoder.frames[i], (VipFrame *)(frames + i * VIP_FRAME_SIZE));
	getDebugPointValue(point, VALUE_FRAMES_SIZE, &value);
	if (value < VIP_DECODER_FRAMES * VIP_FRAME_SIZE)
		return -1;
	getDebugPointValue(point, VALUE_EXTENSIONS, &value);
	for (i = 0; i < VIP_DECODER_FRAMES; i++)
		setFrameExtension(&g_decoder.frames[i],
			(VipFrameExtension *)(value + i * VIP_EXTENSION_SIZE));
	getDebugPointValue(point, VALUE_EXTENSIONS_SIZE, &value);
	if (value < VIP_DECODER_FRAMES * VIP_EXTENSION_SIZE)
		return -1;

	g_openSettings.streamSettings = area->streamSettings;
	kind = g_openSettings.streamSettings >> VIP_STREAM_KIND_SHIFT & VIP_STREAM_KIND_MASK;
	if (g_memoryQuery.valid) {
		if (workSize < g_memoryQuery.size)
			return -1;
		g_decoder.memoryMode = VIP_MEMORY_SIZED;
	} else if (kind == VIP_STREAM_KIND_STANDARD) {
		if (workSize < VIP_MEMORY_STANDARD_SIZE)
			return -1;
		g_decoder.memoryMode = VIP_MEMORY_STANDARD;
	} else if (kind == VIP_STREAM_KIND_LARGE) {
		if (workSize < VIP_MEMORY_LARGE_SIZE)
			return -1;
		if (workSize < VIP_MEMORY_DOUBLE_SIZE)
			g_decoder.memoryMode = VIP_MEMORY_SIZED;
		else
			g_decoder.memoryMode = VIP_MEMORY_DOUBLE;
	}
	return 0;
}

/* The decoder opened: take the area's settings. */
static void takeOpenSettings(VipDecoderArea *area)
{
	area->flags = VIP_AREA_OPEN;
	area->endFlags = 0;
	g_openSettings.outputMode = area->outputMode;
	g_openSettings.concealColour = area->concealColour;
	g_openSettings.options = area->options & VIP_OPTION_MASK;
	g_openSettings.optionTaken = area->options & 1;
	g_openSettings.recoverySettings = area->recoverySettings;
}

/* The query command: the work memory a stream needs; a RESULT_*. */
static uint32_t answerQuery(uint32_t point)
{
	uint32_t width, height, references, size;

	getDebugPointValue(point, 0, &width);
	getDebugPointValue(point, 1, &height);
	getDebugPointValue(point, 2, &references);
	if (queryWorkMemory(width, height, references, &size))
		return RESULT_BAD_COUNT;
	setDebugPointValue(point, VALUE_QUERY_SIZE, size);
	return RESULT_DONE;
}

/* Run a round of the codec's decode; 1 once the stream has ended. */
static uint32_t decodeRound(uint32_t point, VipDecoderArea *area)
{
	int status;

	status = g_decoder.codec.decode(&g_decoder, point, &g_output, &g_openSettings);
	gateDecoderClocks();
	area->options = (area->options & VIP_OPTION_MASK) +
		(g_openSettings.optionTaken << VIP_OPTION_TAKEN_SHIFT);
	switch (status) {
	case VIP_DECODE_STOPPED:
		area->endFlags |= VIP_END_STOPPED;
		notifyDebugger(0, EVENT_DECODER_STOPPED);
		return 1;
	case VIP_DECODE_LAST_PICTURE:
		area->endFlags |= VIP_END_LAST_PICTURE;
		notifyDebugger(0, EVENT_DECODER_ENDED);
		return 1;
	case VIP_DECODE_ERROR:
		area->endFlags |= VIP_END_ERROR;
		notifyDebugger(0, EVENT_DECODER_ENDED);
		return 1;
	}
	return 0;
}

/*
 * Task 0: serve the debug point's commands (open the decoder, close it, query the
 * work memory) and, while it is open, decode each time the schedule comes round.
 * Once the stream has ended the codec still runs, without the debug point, to
 * finish the output.
 */
void decoderTask(uint32_t point)
{
	VipDecoderArea *area;
	uint32_t action = COMMAND_NONE, argument = 1, count, result;
	uint32_t state = DECODER_CLOSED, ended = 0;
	uint32_t frames, workSize;

	getDebugArea(point, (uint32_t *)&area);
	area->version = VIP_DECODER_VERSION;
	setupCodec(&g_decoder.codec);
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
			} else if (action == COMMAND_QUERY) {
				if (state != DECODER_CLOSED)
					result = RESULT_BAD_STATE;
				else if (argument != COMMAND_QUERY_ARGUMENT)
					result = RESULT_BAD_ACTION;
				else if (count != COMMAND_QUERY_COUNT)
					result = RESULT_BAD_COUNT;
				else
					result = answerQuery(point);
			} else if (action != COMMAND_NONE) {
				result = RESULT_BAD_ACTION;
			}
		}
		if (!result) {
			switch (state) {
			case DECODER_OPENING:
				takeOpenValues(point, &frames, &workSize);
				if (checkOpenValues(point, area, frames, workSize)) {
					result = RESULT_BAD_COUNT;
					state = DECODER_CLOSED;
					break;
				}
				enableDecoderClocks();
				if (g_decoder.codec.open(&g_decoder, point, &g_output) < 0) {
					gateDecoderClocks();
					break;
				}
				gateDecoderClocks();
				holdSchedule(point);
				takeOpenSettings(area);
				result = RESULT_DONE;
				state = DECODER_OPEN;
				ended = 0;
				break;
			case DECODER_OPEN:
				if (!needsRestart())
					break;
				reportFrameInterval(&g_output);
				enableDecoderClocks();
				if (!ended) {
					ended = decodeRound(point, area);
				} else {
					g_decoder.codec.decode(&g_decoder, (uint32_t)-1,
						&g_output, &g_openSettings);
					gateDecoderClocks();
				}
				break;
			case DECODER_CLOSING:
				reportFrameInterval(&g_output);
				enableDecoderClocks();
				if (g_decoder.codec.close(&g_decoder, point, &g_output) < 0) {
					gateDecoderClocks();
					break;
				}
				gateDecoderClocks();
				releaseSchedule(point);
				area->flags = 0;
				area->status = 0;
				result = RESULT_DONE;
				state = DECODER_CLOSED;
				break;
			}
		}
		if (result) {
			writeTrace(point, 0, action << 16 | result);
			setDebugPointResult(point, result);
			disableDebugPoint(point);
		}
		notifyDebugger(0, EVENT_DECODER_ROUND);
		setPauseFlag(point, state);
		vipTaskYield();
	}
}
