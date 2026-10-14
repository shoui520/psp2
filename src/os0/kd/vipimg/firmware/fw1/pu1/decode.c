/*
 * PU1: the decode. Each time the schedule comes round, the decoder looks ahead
 * for a whole picture in the records PU0 parsed (lookAhead), then reads the
 * records again in order: the parameter sets and SEI messages are taken, the
 * picture header starts the picture once the output's clock wants it
 * (scheduleFrame), and each slice's data is decoded. After the picture's last
 * slice its records and data are given back to PU0, and the picture waits in the
 * DPB to be shown (outputWaitingPicture).
 */
#include "../common/debug.h"
#include "codec.h"

/* writeTrace's channel and codes (provisional names) */
#define TRACE_CHANNEL		0x20		/* VIP_TRACE_CORE1: PU1's ring */
#define TRACE_DECODER		0x60010138	/* a word of VipDecoder.rings */
#define TRACE_RING		0x6101012c	/* a word of a VipRing */
#define TRACE_PICTURE_SKIPPED	0x10000000	/* the output skipped a non-reference one */
#define TRACE_INPUT_DROPPED	0x80000000
#define TRACE_UNPAIRED_FIELD	0x80000002
#define TRACE_NO_PICTURE	0x80000003	/* slice data without a picture */
#define TRACE_SLICE_FAILED	0x80000004
#define TRACE_USER_DATA_FAILED	0xe0000000
#define TRACE_FLUSHED		0xf0000000
#define TRACE_NO_SPS_DATA	0xffff0000
#define TRACE_NO_PPS_DATA	0xffff0001
#define TRACE_NO_HEADER_DATA	0xffff0002

/* notifyDebugger's events (provisional names) */
#define EVENT_IDR_PICTURE	0x00000001	/* or a new size: the pictures drain */
#define EVENT_I_PICTURE		0x00000002
#define EVENT_P_PICTURE		0x00000004
#define EVENT_B_PICTURE		0x00000008
#define EVENT_SPS		0x00000010
#define EVENT_PPS		0x00000040
#define EVENT_ERROR		0x00000100
#define EVENT_NO_DATA		0x00000200
#define EVENT_INPUT_DROPPED	(EVENT_ERROR | EVENT_NO_DATA)
#define EVENT_FLUSHED		0x00004000
#define EVENT_USER_DATA_UUID	0x00010000
#define EVENT_USER_DATA		0x00020000

/* where a debugger finds the decoder, in local RAM (provisional) */
#define DEBUG_DECODER		(*(VipDecoder **)0x3a8)

/* the reads of VIP_RECORDS_READ records before the decode gives up */
#define DECODE_READS		512
/* of each read, the records taken: the last only tells where the one before ends */
#define RECORDS_TAKEN		(VIP_RECORDS_READ - 1)

/* the wait before a picture buffer is asked for again */
#define BUFFER_RETRY_SPINS	4096
#define BUFFER_RETRIES		2

/* lookAhead's firstFieldLast when the picture is not a field pair */
#define NO_FIRST_FIELD		(-2)

VipRecord g_recordBuffer[VIP_RECORDS_READ] VIP_TASK0_BSS;

/* what the decode keeps that nothing reads (provisional names) */
static uint32_t g_outputMode;		/* VipOpenSettings.outputMode */
static uint32_t g_recordFlags;		/* the record's VIP_TIME_* */
static uint32_t g_recordPts;
static uint32_t g_recordDts;
static uint64_t g_delimiterTime;	/* the last access unit delimiter's DTS, else PTS */
static uint32_t g_untimedDelimiters;	/* the delimiters without either since */

/*
 * Read the next VIP_RECORDS_READ records into g_recordBuffer, from *offset bytes
 * into readable segment *segment of the records ring. *offset and *segment move
 * on by all but the last record, which the next read starts with. -1 when there
 * are none.
 */
int readRecords(uint32_t *offset, uint32_t *segment)
{
	VipRingSegment readable[VIP_STREAM_SEGMENTS];
	VipDmaDescriptor *list = g_readStream.list;
	volatile uint32_t done;
	uint32_t filled = 0, descriptors = 0, length, size;
	int32_t count, i;

	count = getRingReadable(g_ringHandles[VIP_RING_RECORDS], readable,
		VIP_STREAM_SEGMENTS);
	if (!readable[0].size)
		return -1;
	for (i = *segment; i < count; i++) {
		size = readable[i].size;
		if (size + filled - *offset < sizeof(g_recordBuffer)) {
			length = size - *offset;
			if (length & 7)
				length += 8 - (length & 7);
		} else {
			length = sizeof(g_recordBuffer) - filled;
		}
		list[descriptors].external = readable[i].start + *offset;
		list[descriptors].localLength =
			((uint32_t)g_recordBuffer + filled) << 16 | length;
		descriptors++;
		filled += length;
		if (filled >= sizeof(g_recordBuffer)) {
			*offset += length - sizeof(VipRecord);
			break;
		}
		(*segment)++;
		*offset = 0;
	}
	queueListDma(VIP_DMA_LIST | VIP_DMA_READ, (uint32_t)list << 16 |
		descriptors * sizeof(VipDmaDescriptor), 0, 0, NULL, &done);
	while (done == 1)
		;
	return 0;
}

/*
 * A decode begins: take the decoder's rings and frame stores. When PU0 passed a
 * restart on (the records ring's reader state 0, its checkRestart), the stream
 * starts over: the pictures go stale, the decoding is reset and what is unread is
 * dropped. Nonzero when it did.
 */
uint32_t beginDecode(VipDecoder *decoder)
{
	uint32_t restarted, i;

	for (i = 0; i < VIP_DECODER_RINGS; i++)
		g_ringHandles[i] = &decoder->rings[i];
	for (i = 0; i < VIP_DECODER_FRAMES; i++)
		g_frameHandles[i] = &decoder->frames[i];
	restarted = getRingReaderState0(g_ringHandles[VIP_RING_RECORDS]);
	if (restarted) {
		g_generation++;
		resetDecoding(decoder->memoryMode);
		dropParsedInput();
		setRingReaderState0(g_ringHandles[VIP_RING_RECORDS], 0);
	}
	return restarted;
}

/*
 * Show the first picture waiting in output order, when the output's clock says
 * so (checkFrame); without times only once the DPB is nearly full. While the
 * decoding recovers (g_randomAccess), the pictures before the recovery point's
 * frame are dropped unshown.
 */
static void showPicture(VipOutputState *state, VipPictureBuffer *buffer,
	VipDecoderArea *area, uint32_t standard, uint32_t format)
{
	area->status = 1;
	publishSecondPictureInfo(state, buffer->frame, area, standard);
	setFrameFormat(buffer->frame, format);
	outputPicture(buffer);
	setFrameAvailable(buffer->frame, 0);
}

static void outputWaitingPicture(uint32_t point, VipOutputState *state,
	uint32_t standard, uint32_t mode)
{
	VipDecoderArea *area;
	VipPictureBuffer *buffer;
	uint64_t pts;
	uint32_t format, i;
	int result;

	getDebugArea(point, (uint32_t *)&area);
	format = state->command >> 4;
	if (format)
		format = (uint8_t)(format + 1);
	if (!g_timeValid && g_dpbCount < (uint32_t)g_bufferCount - 2)
		return;

	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->idle)
			continue;
		pts = buffer->presentationTime;
		result = checkFrame(state, buffer->ptsValid, &pts, point, mode, buffer->frame,
			g_drainState);
		switch (result) {
		case VIP_OUTPUT_SHOW:
			if (g_randomAccess < RANDOM_ACCESS_IDR) {
				if (g_recoveryFrameCount != buffer->frameNum) {
					buffer->idle = 1;
					return;
				}
				g_randomAccess = RANDOM_ACCESS_IDR;
				showPicture(state, buffer, area, standard, format);
				return;
			}
			showPicture(state, buffer, area, standard, format);
			if (g_randomAccess == RANDOM_ACCESS_BROKEN_LINK)
				g_randomAccess = g_recoveryFrameCount == buffer->frameNum ?
					RANDOM_ACCESS_IDR : RANDOM_ACCESS_RECOVERY;
			return;
		case VIP_OUTPUT_HOLD:
			buffer->idle = 1;
			return;
		case VIP_OUTPUT_SKIP:
			/* the output steps: the picture being decoded goes, the others never */
			showPicture(state, g_currentBuffer, area, standard, format);
			for (i = 0; i < g_dpbCount; i++)
				g_dpbByPoc[i]->idle = 1;
			return;
		default:
			return;
		}
	}
}

/* Hand the SEI user data the picture's records gave to the user data rings' reader. */
static void publishUserData(void)
{
	VipUserDataRings *rings = &g_userData;

	if (rings->recordCursor == rings->records[0].start)
		return;
	while (setRingWriter(g_ringHandles[VIP_RING_USER_DATA], rings->dataCursor))
		;
	while (setRingWriter(g_ringHandles[VIP_RING_USER_RECORDS], rings->recordCursor))
		;
	if (g_userDataTaken)
		notifyDebugger(0, EVENT_USER_DATA);
	if (g_uuidDataTaken)
		notifyDebugger(0, EVENT_USER_DATA_UUID);
}

/* Give PU0 back count records, and the slice data up to position. */
static void consumeRecords(uint32_t count, uint32_t position)
{
	while (advanceRingReader(g_ringHandles[VIP_RING_RECORDS], count * sizeof(VipRecord)))
		;
	while (setRingReader(g_ringHandles[VIP_RING_SLICES], position))
		;
}

/* PU0 may write the rings again: their overflow is over. */
static void clearOverflow(void)
{
	setRingWriterState(g_ringHandles[VIP_RING_RECORDS], 0);
	setRingWriterState(g_ringHandles[VIP_RING_SLICES], 0);
}

/*
 * Drop a field pair's first field, whose last slice header record is
 * firstFieldLast: its records, and its data up to the next record's.
 */
static void dropFirstField(int32_t firstFieldLast)
{
	VipRingSegment readable[VIP_STREAM_SEGMENTS];
	uint32_t position;

	while (advanceRingReader(g_ringHandles[VIP_RING_RECORDS],
	    (firstFieldLast + 2) * sizeof(VipRecord)))
		;
	getRingReadable(g_ringHandles[VIP_RING_RECORDS], readable, VIP_STREAM_SEGMENTS);
	position = ((const VipRecord *)readable[0].start)->position;
	while (setRingReader(g_ringHandles[VIP_RING_SLICES], position))
		;
	clearOverflow();
	g_secondFieldPending = 0;
	g_lastBottomField = g_sliceHeader.bottomField;
	resetUnits();
}

/* The picture's display information goes to its frame store. */
static void setPictureInfo(void)
{
	VipFrameInfo info;

	updateFrameInfo();
	info = g_frameInfo;
	setFrameInfo(g_currentBuffer->frame, &info);
}

/* An access unit delimiter's time, its DTS else its PTS. */
static void takeDelimiterTime(uint32_t flags, uint32_t dtsLow, uint32_t ptsLow)
{
	if (flags & VIP_TIME_DTS) {
		g_delimiterTime = recordDts(flags, dtsLow);
	} else if (flags & VIP_TIME_PTS) {
		g_delimiterTime = recordPts(flags, ptsLow);
	} else {
		g_untimedDelimiters++;
		return;
	}
	g_untimedDelimiters = 0;
}

/*
 * No picture buffer comes free though every frame is back: the DPB's pictures go
 * unshown, the frames are given back, and the decoding waits for a random access
 * point again.
 */
static void flushOutput(uint32_t point, VipOutputState *state)
{
	VipPictureBuffer *buffer;
	uint32_t i;

	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		buffer->reference = VIP_REFERENCE_NONE;
		buffer->topReference = VIP_REFERENCE_NONE;
		buffer->bottomReference = VIP_REFERENCE_NONE;
		buffer->idle = 1;
		if (buffer != g_lastOutput)
			setFrameAvailable(buffer->frame, 1);
	}
	g_referenceFrameCount = 0;
	state->started = 0;
	writeTrace(point, TRACE_FLUSHED, g_pictureCount);
	resetUnits();
	g_timeValid = 1;
	notifyDebugger(0, EVENT_FLUSHED);
	g_generation++;
	g_randomAccess = RANDOM_ACCESS_NONE;
}

/* Trace a ring's header, and the decoder's rings with the decoder at DEBUG_DECODER. */
static void traceRing(const VipRing *ring)
{
	const uint32_t *word = (const uint32_t *)ring;
	const uint32_t *end = (const uint32_t *)(ring + 1);

	for (; word < end; word++)
		writeTrace(TRACE_CHANNEL, TRACE_RING, *word);
}

static void traceDecoder(VipDecoder *decoder)
{
	const uint32_t *word = (const uint32_t *)decoder->rings;
	const uint32_t *end = (const uint32_t *)&decoder->rings[VIP_DECODER_RINGS];

	DEBUG_DECODER = decoder;
	for (; word < end; word++)
		writeTrace(TRACE_CHANNEL, TRACE_DECODER, *word);
	traceRing(decoder->rings[VIP_RING_SLICES].ring);
	traceRing(decoder->rings[VIP_RING_RECORDS].ring);
	traceRing(decoder->rings[VIP_RING_OUTPUT].ring);
	traceRing(decoder->rings[VIP_RING_USER_RECORDS].ring);
}

/* Tell the debugger the type of the picture decoded. */
static void notifyPictureType(void)
{
	switch (g_sliceType) {
	case VIP_SLICE_TYPE_P:
		notifyDebugger(0, EVENT_P_PICTURE);
		break;
	case VIP_SLICE_TYPE_B:
		notifyDebugger(0, EVENT_B_PICTURE);
		break;
	case VIP_SLICE_TYPE_I:
		notifyDebugger(0, g_sliceHeader.idr ? EVENT_IDR_PICTURE : EVENT_I_PICTURE);
		break;
	}
}

/*
 * Conceal the macroblocks between where the last slice ended and where this one
 * starts, in colour (VipDecoderArea.concealColour). An MBAFF frame counts its
 * position in macroblocks of pairs; the firmware compares it with
 * first_mb_in_slice as it is, and only then with its double.
 */
static void concealBeforeSlice(uint32_t colour)
{
	uint32_t firstMb = g_sliceHeader.firstMbInSlice, first, current, count = 0;

	if (!g_sliceHeader.fieldPic && g_currentSps->mbAdaptiveFrameField) {
		first = firstMb << 1;
		current = g_mbX + g_mbY * (g_mbaffLastX + 1);
		if (current < firstMb && firstMb < g_pictureMbs)
			count = (firstMb - current) >> 1;
	} else {
		first = firstMb;
		current = g_mbX + g_mbY * (g_widthMbsMinus1 + 1);
		if (current < first && first < g_pictureMbs)
			count = first - current;
	}
	if (first && first != current && !(colour & VIP_CONCEAL_NONE))
		fillMacroblocks(count, colour);
}

/* Conceal the macroblocks after the picture's last slice. */
static void concealAfterPicture(uint32_t colour)
{
	uint32_t end, count;

	if (colour & VIP_CONCEAL_NONE)
		return;
	if (g_currentSps->mbAdaptiveFrameField && !g_sliceHeader.fieldPic) {
		end = (g_mbX & ~1u) + g_mbY * (g_mbaffLastX + 1);
		count = (g_pictureMbs - end) >> 1;
	} else {
		end = g_mbX + g_mbY * (g_widthMbsMinus1 + 1);
		count = g_pictureMbs - end;
	}
	if (!g_noFreeBuffer && end < g_pictureMbs)
		fillMacroblocks(count, colour);
}

/*
 * The codec's decode, each time the schedule comes round: a picture's records,
 * when they are all there. VIP_DECODE_* (VIP_DECODE_STOPPED once a restart's
 * pictures were all shown), or -1 when the picture has to wait.
 */
int decode(VipDecoder *decoder, uint32_t point, VipOutputState *output,
	VipOpenSettings *settings)
{
	volatile VipCodecCounters *counters = VIP_CODEC_COUNTERS;
	VipDecoderArea *area;
	VipPictureBuffer *buffer;
	VipFrameHandle *outputFrame = NULL;
	const VipRecord *record;
	VipTime fallback, decodeTime, presentationTime, pts;
	volatile uint32_t spin;
	uint32_t restarted, available, flags, ptsLow, dtsLow, position, padding;
	uint32_t next, bits, consumed = 0, consumedPosition = 0, offset, segment;
	uint32_t end, read, index, type, timeScale, chroma, mustDrain, i, j;
	int16_t last, firstFieldLast;
	int32_t top, bottom;
	int result, sliceResult, ptsValid;

	debugHook();
	traceDecoder(decoder);
	restarted = beginDecode(decoder);
	traceDecoder(decoder);
	result = getDebugArea(point, (uint32_t *)&area);
	if (result < 0)
		return result;
	result = beginOutput(output, point, settings->outputMode, restarted);
	if (result < 0)
		return result;

	/* a step command ended: what it left in the DPB goes */
	if (output->lastCommand != output->command) {
		switch (output->lastCommand >> 16) {
		case VIP_COMMAND_STEP_I:
		case VIP_COMMAND_STEP_IP:
		case VIP_COMMAND_STEP_I_COUNT:
		case VIP_COMMAND_STEP_IP_COUNT:
			flushPictures();
			break;
		}
	}
	g_streamSettings = settings->streamSettings;
	g_outputMode = settings->outputMode;
	counters->pictures = g_pictureCount;
	counters->outputs = g_outputCount;
	counters->underruns = g_underrunCount;

	if (g_drainState == VIP_DRAIN_RESTART) {
		/* the stream started over: show what is left, then stop */
		result = VIP_DECODE_STOPPED;
		if (!g_timeValid) {
			if (output->command >> 16 != VIP_COMMAND_REPEAT) {
				area->status = 1;
				outputAllPictures();
			}
		} else {
			outputWaitingPicture(point, output, settings->streamSettings,
				settings->outputMode);
			for (i = 0; i < g_dpbCount; i++) {
				if (!g_dpbByPoc[i]->idle) {
					result = VIP_DECODE_CONTINUE;
					break;
				}
			}
		}
		counters->outputs = g_outputCount;
		return result;
	}
	if (g_drainState == VIP_DRAIN_LAYOUT) {
		/* the frame stores are laid out again once every frame is back */
		for (i = 0; i < g_dpbCount; i++) {
			getFrameAvailable(g_dpbByPoc[i]->frame, &available);
			if (!available)
				return VIP_DECODE_CONTINUE;
		}
	}

again:
	result = lookAhead(&last, &firstFieldLast, &end);
	if (result != 1) {
		if (result == -1) {
			writeTrace(point, TRACE_INPUT_DROPPED, (uint32_t)-1);
			notifyDebugger(0, EVENT_INPUT_DROPPED);
		}
		return VIP_DECODE_CONTINUE;
	}
	bits = 0;
	next = 0;
	consumed = 0;
	settings->userData = area->userData;
	area->flags &= VIP_AREA_FLAGS & ~VIP_AREA_WAITING;
	offset = 0;
	segment = 0;

	for (read = 0; read < DECODE_READS; read++) {
		if (readRecords(&offset, &segment) < 0)
			return VIP_DECODE_CONTINUE;
		for (i = 0; i < RECORDS_TAKEN; i++) {
			index = read * RECORDS_TAKEN + i;
			record = &g_recordBuffer[i];
			type = record->header >> VIP_RECORD_TYPE_SHIFT;
			if (type == VIP_PARSED_PADDING)
				continue;
			flags = record->header >> VIP_RECORD_FLAGS_SHIFT & VIP_TIME_FLAGS;
			g_recordFlags = flags;
			ptsLow = record->pts;
			dtsLow = record->dts;
			g_recordDts = dtsLow;
			g_recordPts = ptsLow;
			position = record->position;
			g_readStream.cursor = position;

			/* the record's data, in bits, ends where the next one's begins */
			padding = 0;
			if (record[1].header >> VIP_RECORD_TYPE_SHIFT == VIP_PARSED_PADDING)
				padding = record[1].header >> VIP_PADDING_BITS_SHIFT & 7;
			if (type != VIP_PARSED_START) {
				next = record[1].position;
				if (next >= position)
					bits = (next - position) * 8;
				else
					bits = getRingDistance(g_ringHandles[VIP_RING_SLICES],
						position, next) * 8;
				bits += padding;
			}

			switch (type) {
			case VIP_PARSED_START:
				/* the stream starts over: show the pictures left, then stop */
				consumeRecords(index + 1, position);
				publishUserData();
				g_drainState = VIP_DRAIN_RESTART;
				g_extensionState = 1;
				outputWaitingPicture(point, output, settings->streamSettings,
					settings->outputMode);
				clearOverflow();
				return VIP_DECODE_CONTINUE;

			case VIP_PARSED_SPS:
				if (bits) {
					area->flags |= VIP_AREA_PARAMETERS;
					takeSps();
					area->flags &= VIP_AREA_FLAGS & ~VIP_AREA_PARAMETERS;
				} else {
					counters->spsErrors++;
					writeTrace(point, TRACE_NO_SPS_DATA, g_pictureCount);
					notifyDebugger(0, EVENT_NO_DATA);
				}
				/* a held picture leaves its parameter sets taken */
				consumedPosition = next;
				consumed = index + 1;
				break;

			case VIP_PARSED_PPS:
				if (bits) {
					area->flags |= VIP_AREA_PARAMETERS;
					takePps();
					area->flags &= VIP_AREA_FLAGS & ~VIP_AREA_PARAMETERS;
				} else {
					counters->ppsErrors++;
					writeTrace(point, TRACE_NO_PPS_DATA, g_pictureCount);
					notifyDebugger(0, EVENT_NO_DATA);
				}
				consumedPosition = next;
				consumed = index + 1;
				break;

			case VIP_PARSED_PICTURE_HEADER:
				if (bits) {
					area->flags |= VIP_AREA_PICTURE_HEADER;
					mustDrain = 1;
					if (g_drainState == VIP_DRAIN_LAYOUT) {
						mustDrain = 0;
						g_drainState = VIP_DRAIN_NONE;
					}
					result = takePictureHeader(decoder->memoryMode,
						settings->recoverySettings & VIP_RECOVERY_AT_I ? 1 : 0,
						mustDrain);
					area->flags &= VIP_AREA_FLAGS & ~VIP_AREA_PICTURE_HEADER;
				} else {
					counters->headerErrors++;
					writeTrace(point, TRACE_NO_HEADER_DATA, g_pictureCount);
					notifyDebugger(0, EVENT_ERROR);
					result = -1;
				}

				if (result == VIP_PICTURE_TOO_LARGE) {
					/* the ARM side learns the picture's size, and the decoding ends */
					g_currentBuffer = &g_buffers[0];
					setPictureInfo();
					publishPictureInfo(output, g_currentBuffer->frame, area,
						settings->streamSettings, g_currentSps->profileIdc << 16 |
						g_currentSps->levelIdc);
					return VIP_DECODE_LAST_PICTURE;
				}
				if (result == VIP_PICTURE_TOO_MANY_FRAMES)
					return VIP_DECODE_ERROR;
				if (result == VIP_PICTURE_NOT_PAIRED && firstFieldLast != NO_FIRST_FIELD) {
					dropFirstField(firstFieldLast);
					g_currentBuffer = NULL;
					return -1;
				}
				if (result < 0 && result != VIP_PICTURE_NOT_PAIRED) {
					consumeRecords(last + 2, end);
					clearOverflow();
					resetUnits();
					goto again;
				}
				if (result == VIP_PICTURE_LAID_OUT) {
					output->started = 0;
				} else if (result == VIP_PICTURE_DRAIN) {
					/* every picture of the old size is shown first */
					for (j = 0; j < g_bufferCount; j++) {
						buffer = g_dpbByPoc[j];
						if (buffer && !buffer->idle) {
							outputPicture(buffer);
							setFrameAvailable(buffer->frame, 0);
						}
					}
					g_drainState = VIP_DRAIN_LAYOUT;
					notifyDebugger(0, EVENT_IDR_PICTURE);
					return VIP_DECODE_CONTINUE;
				}

				/* before a random access point the pictures go undecoded, unless stepped */
				switch (output->command >> 16) {
				case VIP_COMMAND_STOP:
				case VIP_COMMAND_STEP_I:
				case VIP_COMMAND_STEP_IP:
				case VIP_COMMAND_STEP_I_COUNT:
				case VIP_COMMAND_STEP_IP_COUNT:
					break;
				default:
					if (g_randomAccess != RANDOM_ACCESS_NONE)
						break;
					if (g_sliceHeader.fieldPic && firstFieldLast != NO_FIRST_FIELD) {
						dropFirstField(firstFieldLast);
					} else {
						consumeRecords(last + 2, end);
						resetUnits();
					}
					clearOverflow();
					goto again;
				}

				if (!g_currentBuffer) {
					while (allocatePictureBuffer() < 0) {
						/* while the display has frames out, one may come back */
						for (j = 0; j < g_dpbCount; j++) {
							getFrameAvailable(g_dpbByPoc[j]->frame, &available);
							if (!available)
								return -1;
						}
						if (g_retryCount == BUFFER_RETRIES) {
							counters->bufferTimeouts++;
							flushOutput(point, output);
							if (settings->options & VIP_OPTION_SWITCH_TIMING)
								settings->optionTaken = !settings->optionTaken;
							return -1;
						}
						if (g_retryCount) {
							g_retryCount++;
							return -1;
						}
						for (spin = 0; spin < BUFFER_RETRY_SPINS; spin++)
							;
						g_retryCount++;
					}
					g_retryCount = 0;
				}

				/* without times, the picture's are the output's next */
				fallback = output->nextPts >> 1;
				if (takePictureTime((uint8_t)settings->optionTaken, flags, ptsLow, dtsLow,
				    &fallback) < 0)
					g_timeValid = 0;

				if (!g_sliceHeader.fieldPic || !g_secondFieldPending) {
					setPictureInfo();

					/*
					 * The frame the output shows next: the first waiting in
					 * order, unless this picture comes before it.
					 */
					outputFrame = NULL;
					for (j = 0; j < g_dpbCount; j++) {
						buffer = g_dpbByPoc[j];
						if (buffer->idle)
							continue;
						if (g_sliceHeader.idr || buffer->generation != g_generation ||
						    currentPicOrderCnt() >= picOrderCnt(buffer))
							outputFrame = buffer->frame;
						else
							outputFrame = g_currentBuffer->frame;
						break;
					}

					pts = recordPts(flags, ptsLow);
					decodeTime = g_currentBuffer->decodeTime;
					applyRateControl(output, g_currentBuffer->frame, area, 1, &decodeTime,
						flags & VIP_TIME_PTS ? 1 : 0, &pts);

					/* the output schedules by the decode time */
					ptsValid = (flags & (VIP_TIME_PTS | VIP_TIME_DTS)) ||
						g_currentSps->timeScale;
					g_pictureHeld = 0;
					decodeTime = g_currentBuffer->decodeTime;
					presentationTime = g_currentBuffer->presentationTime;
					result = scheduleFrame(output, ptsValid, &decodeTime,
						&presentationTime, point, settings->streamSettings,
						settings->outputMode, g_currentBuffer->frame, outputFrame);
					if (output->waitRandomAccess == 2)	/* (never: 0 or 1) */
						flushPictures();
					if (result < 0) {
						/* not yet: it comes again, its parameter sets taken */
						if (g_pictureCount && result == VIP_OUTPUT_DROP)
							counters->droppedPictures++;
						if (consumed) {
							publishUserData();
							consumeRecords(consumed, consumedPosition);
						}
						g_pictureHeld = 1;
						return -1;
					}
					if (result == VIP_OUTPUT_HOLD && !g_sliceHeader.reference) {
						/* nothing refers to the picture: it is skipped */
						updatePicOrderCnt(&top, &bottom);
						writeTrace(point, TRACE_PICTURE_SKIPPED, g_pictureCount);
						consumeRecords(last + 2, end);
						publishUserData();
						advancePts(output, g_currentBuffer->frame, outputFrame);
						counters->skippedPictures++;
						resetUnits();
						clearOverflow();
						goto again;
					}
					if (result == VIP_OUTPUT_SKIP) {
						updatePicOrderCnt(&top, &bottom);
						if (g_sliceHeader.fieldPic && g_otherFieldRandomAccess) {
							/* the second field starts the decoding */
							dropFirstField(firstFieldLast);
						} else {
							consumeRecords(last + 2, end);
							advancePts(output, g_currentBuffer->frame, outputFrame);
							resetUnits();
						}
						publishUserData();
						counters->skippedPictures++;
						clearOverflow();
						goto again;
					}
				}

				/* a second field must be the other parity of the first */
				if (g_secondFieldPending && (!g_sliceHeader.fieldPic ||
				    g_sliceHeader.bottomField == g_lastBottomField)) {
					dropFirstField(firstFieldLast);
					writeTrace(point, TRACE_UNPAIRED_FIELD, g_sliceHeader.fieldPic);
					writeTrace(point, TRACE_UNPAIRED_FIELD, g_sliceHeader.bottomField);
					writeTrace(point, TRACE_UNPAIRED_FIELD, g_lastBottomField);
					notifyDebugger(0, EVENT_INPUT_DROPPED);
					g_currentBuffer = NULL;
					return -1;
				}

				/* (the firmware passes the record's times too, unused) */
				startPicture(decoder->memoryMode);
				publishPictureInfo(output, g_currentBuffer->frame, area,
					settings->streamSettings, g_currentSps->profileIdc << 16 |
					g_currentSps->levelIdc);
				chroma = g_currentSps->chromaSampleLocTypeTopField <<
					VIP_PICTURE_CHROMA_TOP_SHIFT |
					g_currentSps->chromaSampleLocTypeBottomField <<
					VIP_PICTURE_CHROMA_BOTTOM_SHIFT;
				area->picture.progressive |= chroma;
				area->secondPicture.progressive |= chroma;
				publishExtension(g_currentBuffer->frame, area, &g_frameExtension);
				if (g_spsTaken) {
					notifyDebugger(0, EVENT_SPS);
					g_spsTaken = 0;
				}
				if (g_ppsTaken) {
					notifyDebugger(0, EVENT_PPS);
					g_ppsTaken = 0;
				}
				break;

			case VIP_PARSED_SLICE_HEADER:
				if (bits) {
					takeSliceHeader();
					break;
				}
				counters->headerErrors++;
				writeTrace(point, TRACE_NO_HEADER_DATA, g_pictureCount);
				notifyDebugger(0, EVENT_ERROR);
				break;

			case VIP_PARSED_CABAC_DATA:
			case VIP_PARSED_CAVLC_DATA:
				if (!g_currentBuffer) {
					/* a slice without its picture: its records go */
					writeTrace(point, TRACE_NO_PICTURE, g_pictureCount);
					notifyDebugger(0, EVENT_ERROR);
					consumeRecords(index + 1, next);
					publishUserData();
					if (markReference() < 0)
						g_timeValid = 1;
					g_secondFieldPending = 0;
					resetUnits();
					g_currentBuffer = NULL;
					clearOverflow();
					return VIP_DECODE_CONTINUE;
				}
				if (!g_noFreeBuffer)
					concealBeforeSlice(settings->concealColour);
				sliceResult = decodeSlice(bits);
				if (sliceResult < 0) {
					counters->sliceErrors++;
					writeTrace(point, sliceResult, g_pictureCount);
					writeTrace(point, TRACE_SLICE_FAILED, g_pictureCount);
					notifyDebugger(0, EVENT_ERROR);
					setFrameDamaged(g_currentBuffer->frame, 1);
				}

				if ((int32_t)index == last + 1) {
					/* the picture's last slice: the picture is done */
					concealAfterPicture(settings->concealColour);
					consumeRecords(index + 1, next);
					publishUserData();
					if (markReference() < 0)
						g_timeValid = 1;
					if (g_sliceHeader.fieldPic && !g_secondFieldPending)
						g_extensionState = 1;
					outputWaitingPicture(point, output, settings->streamSettings,
						settings->outputMode);
					advancePts(output, g_currentBuffer->frame, outputFrame);
					notifyPictureType();
					g_pictureCount++;
					g_secondFieldPending = 0;
					g_lastBottomField = g_sliceHeader.fieldPic ?
						g_sliceHeader.bottomField : VIP_NO_FIELD;
					resetUnits();
					g_currentBuffer = NULL;
					clearOverflow();
					return VIP_DECODE_CONTINUE;
				}
				if ((int32_t)index == firstFieldLast + 1) {
					/* the first field's last slice: the second field follows */
					if (sliceResult < 0 && !g_noFreeBuffer &&
					    g_sliceMbAddress < g_pictureMbs &&
					    !(settings->concealColour & VIP_CONCEAL_NONE))
						fillMacroblocks(g_pictureMbs - g_sliceMbAddress,
							settings->concealColour);
					if (markReference() < 0)
						g_timeValid = 1;
					notifyPictureType();
					g_secondFieldPending = 1;
					g_lastBottomField = g_sliceHeader.bottomField;
					resetUnit0();
					while (setRingReader(g_ringHandles[VIP_RING_SLICES], next))
						;
				}
				break;

			case VIP_PARSED_VUI:
				timeScale = g_currentSps->timeScale;
				takeVui();
				if (!g_pictureHeld && g_currentSps->timeScale != timeScale)
					rescaleClock(timeScale, g_currentSps->timeScale);
				break;
			case VIP_PARSED_BUFFERING_PERIOD:
				takeBufferingPeriod();
				break;
			case VIP_PARSED_PICTURE_TIMING:
				takePictureTiming();
				break;
			case VIP_PARSED_PAN_SCAN:
				takePanScan();
				break;
			case VIP_PARSED_USER_DATA:
				if ((settings->userData & VIP_USER_DATA_REGISTERED) &&
				    takeUserData(type, bits, flags, ptsLow) < 0)
					writeTrace(point, TRACE_USER_DATA_FAILED, g_pictureCount);
				break;
			case VIP_PARSED_USER_DATA_UUID:
				if ((settings->userData & VIP_USER_DATA_UNREGISTERED) &&
				    takeUserData(type, bits, flags, ptsLow) < 0)
					writeTrace(point, TRACE_USER_DATA_FAILED, g_pictureCount);
				break;
			case VIP_PARSED_RECOVERY_POINT:
				takeRecoveryPoint(settings->recoverySettings &
					VIP_RECOVERY_IGNORE_BROKEN_LINK);
				break;
			case VIP_PARSED_DELIMITER:
				if (!g_pictureHeld)
					takeDelimiterTime(flags, dtsLow, ptsLow);
				break;

			case VIP_PARSED_SEI:
			case VIP_PARSED_SEI_OTHER:
			case VIP_PARSED_RESERVED:
			case VIP_PARSED_END_SEQUENCE:
			case VIP_PARSED_END_STREAM:
			case VIP_PARSED_OTHER:
				break;
			default:
				return -1;
			}
		}
	}
	return -1;
}
