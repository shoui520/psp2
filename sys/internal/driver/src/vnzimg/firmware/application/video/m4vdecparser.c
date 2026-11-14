/*
 * The MPEG-4 decoder's parser engine thread (0x841730-0x8471ac): its memory,
 * the access units it takes, the VThread jobs it runs on them, and the
 * headers they parse.
 */
#include "m4vdecparser.h"

#define ALIGN256(size)		(((size) + 255) & ~255)

/* the macroblock unit's MPEG-4 registers (VIDEO_UNIT words) */
#define UNIT_MODE_M4V		0x00000001
#define UNIT_M4V_PICTURE	0xc0	/* short header, resync markers disabled, quantiser */
#define UNIT_M4V_VOP		0xc1	/* coding type, intra DC VLC threshold, fcode */
#define UNIT_M4V_RUN		0xc2	/* the row's macroblocks, the run's column and row */
#define UNIT_M4V_CONTROL	0xc3

void m4vdecParserAu(uint32_t memory, uint32_t request);
void m4vdecParserJobStart(uint32_t info, uint32_t stream, uint32_t job, uint32_t unused);
void m4vdecParserJobFlush(uint32_t info, uint32_t stream, uint32_t job, uint32_t unused);
void m4vdecParserJobOutput(uint32_t info, uint32_t stream, uint32_t job, uint32_t keepError);

/* a time stamp from its two words */
static inline VideoTime timeOf(const uint32_t *words)
{
	return (VideoTime)((uint64_t)words[1] << 32 | words[0]);
}

static M4vdecParser *parserBegin(uint32_t memory);
static void parserEnd(M4vdecParser *parser);
static uint32_t parserWait(M4vdecParser *parser);
static void requestFinish(M4vdecParser *parser);
static void requestStatus(M4vdecParser *parser);
static void parsedSet(M4vdecParser *parser);
static void picturesPrepare(M4vdecParser *parser, M4vdecParserInfo *info);
static void outputRun(M4vdecParser *parser);
static void flushRun(M4vdecParser *parser);
static void unitSkip(M4vdecParser *parser);

/* 0x841730: the engine's thread, until it is stopped */
void m4vdecParserThread(uint32_t memory, uint32_t codec)
{
	M4vdecParser *parser = parserBegin(memory);
	M4vdecParserInfo *info = parser->info;
	VnzKernelMessageQueue *auQueue = parser->auQueue;
	VnzKernelMessageQueue *serviceQueue = parser->serviceQueue;
	VnzKernelMessageQueue *decoderQueue = parser->decoderQueue;
	VnzKernelMessageQueue *wakeQueue = parser->wakeQueue;
	VnzKernelMessageQueue *helperQueue = parser->helperQueue;
	uint32_t flags;

	(void)codec;
	do {
		flags = parserWait(parser);
		if (parser->state == PARSER_IDLE && vnzKernelMessageReceive(auQueue,
			(uint32_t)parser))
			parser->state = PARSER_STARTING;
		if (parser->state == PARSER_STARTING) {
			if (flags & PARSER_FLAG_DECODE) {
				if (!parser->prepared)
					picturesPrepare(parser, info);
				parser->state = PARSER_PARSING;
			} else if (flags & PARSER_FLAG_DRAIN) {
				parser->output = 1;
				parser->flush = 1;
				parser->state = PARSER_PARSING;
			} else if (flags & PARSER_FLAG_OUTPUT) {
				parser->output = 1;
				parser->state = PARSER_PARSING;
			} else if (flags & (PARSER_FLAG_SEQUENCE | PARSER_FLAG_CONTEXT)) {
				info = parser->info;
				if (flags & PARSER_FLAG_SEQUENCE) {
					if (flags & PARSER_FLAG_FAILED)
						unitSkip(parser);
					requestStatus(parser);
					requestFinish(parser);
				}
				vnzKernelMessageSend(serviceQueue, videoServiceAuDone,
					parser->request);
				parser->state = PARSER_IDLE;
				vnzThreadSetFlags(info->thread, PARSER_FLAG_AU);
			} else if (flags & PARSER_FLAG_FAILED) {
				unitSkip(parser);
				parser->output = 1;
				parser->keepError = 1;
				parser->state = PARSER_PARSING;
			}
		}
		if (parser->state == PARSER_PARSING &&
		    vnzKernelMessageReceive(wakeQueue, (uint32_t)parser)) {
			if (!parser->output)
				vnzKernelMessageSend(helperQueue, videoHelperStart,
					parser->request);
			else if (parser->flush == 1)
				flushRun(parser);
			else
				outputRun(parser);
			parser->state = PARSER_HELPING;
		}
		if (parser->state == PARSER_HELPING && (flags & PARSER_FLAG_JOB_DONE)) {
			requestFinish(parser);
			vnzKernelMessageSend(serviceQueue, videoServiceAuDone, parser->request);
			if (flags & PARSER_FLAG_FAILED) {
				unitSkip(parser);
				parser->state = PARSER_PARSING;
				parser->flush = 1;
			} else {
				if (parser->output) {
					parsedSet(parser);
					vnzKernelMessageSend(decoderQueue, m4vdecDecoderParsed,
						parser->parsedWord);
					parser->keepError = 0;
					parser->flush = 0;
					parser->output = 0;
				}
				parser->state = PARSER_IDLE;
				vnzThreadSetFlags(info->thread, PARSER_FLAG_AU);
			}
		}
	} while (parser->state != PARSER_STOPPED);
	parserEnd(parser);
}

/* 0x8418de: the memory laid out and opened, the unit cleared */
static M4vdecParser *parserBegin(uint32_t memory)
{
	M4vdecParser *header = (M4vdecParser *)memory, *parser;
	M4vdecParserInfo *info;
	VideoM4vUnit *unit, *headerUnit;

	headerUnit = (VideoM4vUnit *)(memory + PARSER_HEADER_SIZE + PARSER_INFO_SIZE +
		PARSER_STREAM_SIZE + PARSER_JOB_SIZE);
	headerUnit->vol = (VideoM4vVol *)((uint32_t)headerUnit + PARSER_UNIT_SIZE);
	header->info = (M4vdecParserInfo *)(memory + PARSER_HEADER_SIZE);
	header->stream = memory + PARSER_HEADER_SIZE + PARSER_INFO_SIZE;
	header->job = memory + PARSER_HEADER_SIZE + PARSER_INFO_SIZE + PARSER_STREAM_SIZE;
	header->unit = (uint32_t)headerUnit;
	headerUnit->vop = (VideoM4vVop *)((uint32_t)headerUnit + PARSER_UNIT_SIZE +
		PARSER_VOL_SIZE);
	parser = (M4vdecParser *)vnzPublicOpenPrivate(memory, PARSER_HEADER_SIZE);
	unit = (VideoM4vUnit *)parser->unit;
	info = parser->info;
	info->thread = vnzThreadCurrent();
	unit->lastTable = 0;
	unit->restart = 0;
	info->contextSet = 0;
	unit->error = 0;
	unit->tables = 1;
	unit->vol->matrixCoefficients = 1;
	unit->pictureCount = 0;
	unit->lastPts = 0;
	unit->lastTime = 0;
	unit->wrapped = 0;
	unit->timeIncrement = -1;
	unit->tableFields = 0;
	unit->frameFields = 2;
	unit->firstDts = info->option5;
	unit->firstPts = info->option4;
	parser->info = (M4vdecParserInfo *)vnzPublicOpenRaw((uint32_t)info, PARSER_INFO_SIZE);
	parser->state = PARSER_IDLE;
	parser->keepError = 0;
	parser->flush = 0;
	parser->output = 0;
	parser->prepared = 0;
	return parser;
}

/* 0x8419c8 */
static void parserEnd(M4vdecParser *parser)
{
	vnzPublicCloseRaw((uint32_t)parser->info, PARSER_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)parser, PARSER_HEADER_SIZE);
}

/* 0x8419ea: the thread's flags; an exit stops the engine */
static uint32_t parserWait(M4vdecParser *parser)
{
	uint32_t flags = vnzThreadWaitFlags(PARSER_FLAGS);

	if (flags & PARSER_FLAG_EXIT)
		parser->state = PARSER_STOPPED;
	return flags;
}

/*
 * 0x841a10: an access unit: its bytes mapped and its first job started; a
 * drain only noted; a context set only remembered.
 */
void m4vdecParserAu(uint32_t memory, uint32_t requestAddress)
{
	M4vdecParser *parser = (M4vdecParser *)memory;
	M4vdecParserInfo *info = parser->info, *context;
	VideoEngineRequest *request;
	VideoM4vStream *stream;
	uint32_t start, esSize, bufferSize, esOffset, end, line, first, size, index;
	VnzKernelVThreadContext *locked;

	parser->request = requestAddress;
	request = (VideoEngineRequest *)vnzPublicOpenPrivate(requestAddress, REQUEST_SIZE);
	stream = (VideoM4vStream *)vnzPublicOpenPrivate(parser->stream, PARSER_STREAM_SIZE);
	if (request->flags & VIDEO_ENGINE_DRAIN) {
		stream->unit = parser->unit;
		vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
		vnzPublicClosePrivate((uint32_t)stream, PARSER_STREAM_SIZE);
		vnzThreadSetFlags(info->thread, PARSER_FLAG_DRAIN);
		return;
	}
	if (request->flags & VIDEO_ENGINE_CONTEXT) {
		context = (M4vdecParserInfo *)vnzPublicOpenPrivate(
			vnzPublicCloseRaw((uint32_t)info, PARSER_INFO_SIZE), PARSER_INFO_SIZE);
		parser->frames = request->pictures;
		context->contextHorizontal = request->horizontal;
		context->contextVertical = request->vertical;
		context->contextSet = 1;
		context->contextMacroblock =
			(request->contextFlags & VIDEO_ENGINE_CONTEXT_MACROBLOCK) != 0;
		stream->pts = timeOf(request->dts);
		stream->dts = timeOf(request->pts);
		request->status = 0;
		vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
		vnzPublicClosePrivate((uint32_t)stream, PARSER_STREAM_SIZE);
		info = (M4vdecParserInfo *)vnzPublicOpenRaw(
			vnzPublicClosePrivate((uint32_t)context, PARSER_INFO_SIZE), PARSER_INFO_SIZE);
		parser->info = info;
		picturesPrepare(parser, info);
		vnzThreadSetFlags(info->thread, PARSER_FLAG_CONTEXT);
		return;
	}

	index = info->index << 3;
	if (!parser->prepared)
		parser->frames = request->pictures;
	stream->id = info->id;
	if (!(request->flags & VIDEO_ENGINE_RESTART)) {
		start = request->buffer;
		bufferSize = request->bufferSize;
		esOffset = request->esOffset;
		esSize = request->esSize;
		stream->esOffset = esOffset;
	} else {
		/* a restart: the unit the stream has, again */
		start = stream->ringStart;
		bufferSize = stream->bufferSize;
		esOffset = stream->esOffset;
		esSize = stream->esSize;
		request->esOffset = esOffset;
		request->esSize = esSize;
		request->bufferSize = bufferSize;
	}
	end = start + esOffset;
	line = end & ~63;
	if ((int32_t)bufferSize < (int32_t)(esSize + esOffset)) {
		/* the unit wraps: its end at the ring's start */
		first = bufferSize - esOffset;
		stream->ringStart = vnzPublicOpenRaw(start, ALIGN64(esSize - first));
		stream->mapped = vnzPublicOpenRaw(line, ALIGN64(first));
		stream->base = end - line + stream->mapped;
		end = first + stream->base;
	} else {
		size = ALIGN64(esSize + esOffset) - (esOffset & ~63);
		stream->mapped = vnzPublicOpenRaw(line, size);
		stream->ringStart = 0;
		stream->base = end - line + stream->mapped;
		end = stream->mapped + size;
	}
	stream->ringEnd = end - 1;
	stream->esSize = request->esSize;
	stream->bufferSize = request->bufferSize;
	stream->sequence = (request->flags & VIDEO_ENGINE_SEQUENCE) != 0;
	if (request->flags & VIDEO_ENGINE_RESTART)
		stream->auMode = request->auMode | VIDEO_ENGINE_RESTART;
	else
		stream->auMode = request->auMode;
	stream->tables = info->tables;
	stream->unit = parser->unit;
	request->id = info->id;
	request->thread = info->thread;
	request->queue = parser->decoderQueue;
	request->stream = parser->stream;
	request->contextFlags = 0;
	request->job = parser->job;
	stream->pts = timeOf(request->dts);
	stream->dts = timeOf(request->pts);
	if (info->contextMacroblock)
		request->contextFlags |= VIDEO_ENGINE_CONTEXT_MACROBLOCK;
	if (parser->option)
		request->contextFlags |= 0x08;
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
	vnzPublicClosePrivate((uint32_t)stream, PARSER_STREAM_SIZE);
	locked = vnzKernelVThreadContextLock(parser->context);
	vnzKernelVThreadJobSet(locked, index, 0, m4vdecParserJobStart, (uint32_t)info,
		parser->stream,
		parser->job, 0);
	vnzKernelVThreadContextUnlock(locked);
}

/* 0x841cf2: nothing to wake */
void m4vdecParserWake(uint32_t memory, uint32_t request)
{
	(void)memory;
	(void)request;
}

/* 0x841cf4: the request answered with the unit's error, its bytes unmapped */
static void requestFinish(M4vdecParser *parser)
{
	const VideoM4vUnit *unit;
	VideoEngineRequest *request;
	VideoM4vStream *stream;
	uint32_t bufferSize, esSize, esOffset, first;

	unit = (const VideoM4vUnit *)vnzPublicOpenProtected(parser->unit, PARSER_UNIT_SIZE);
	request = (VideoEngineRequest *)vnzPublicOpenPrivate(parser->request, REQUEST_SIZE);
	request->status = unit->error;
	if (!(request->flags & VIDEO_ENGINE_DRAIN)) {
		stream = (VideoM4vStream *)vnzPublicOpenPrivate(parser->stream, PARSER_STREAM_SIZE);
		bufferSize = request->bufferSize;
		esSize = request->esSize;
		esOffset = request->esOffset;
		if ((int32_t)bufferSize < (int32_t)(esSize + esOffset)) {
			first = bufferSize - esOffset;
			vnzPublicCloseRaw(stream->mapped, ALIGN64(first));
			vnzPublicCloseRaw(stream->ringStart, ALIGN64(esSize - first));
		} else {
			vnzPublicCloseRaw(stream->mapped,
				ALIGN64(esSize + esOffset) - (esOffset & ~63));
		}
		stream->ringStart = request->buffer;
		vnzPublicClosePrivate((uint32_t)stream, PARSER_STREAM_SIZE);
	}
	vnzPublicCloseProtected((uint32_t)unit, PARSER_UNIT_SIZE);
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
}

/* 0x841db4: the request answered with the sequence the unit found */
static void requestStatus(M4vdecParser *parser)
{
	const VideoM4vUnit *unit;
	VideoEngineRequest *request;

	unit = (const VideoM4vUnit *)vnzPublicOpenProtected(parser->unit, PARSER_UNIT_SIZE);
	request = (VideoEngineRequest *)vnzPublicOpenPrivate(parser->request, REQUEST_SIZE);
	request->status = unit->error;
	if ((uint32_t)unit->error < VIDEO_ERROR_FATAL) {
		request->horizontal = unit->horizontal;
		request->vertical = unit->vertical;
		request->contextFlags = 0;
		request->sequenceInfo = unit->sequenceInfo;
		if (unit->vol->dataPartitioned)
			request->contextFlags |= VIDEO_ENGINE_CONTEXT_MACROBLOCK;
	} else {
		request->sequenceInfo = 0;
		request->vertical = 0;
		request->horizontal = 0;
		request->contextFlags = 0;
	}
	vnzPublicCloseProtected((uint32_t)unit, PARSER_UNIT_SIZE);
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
}

/* 0x841e50: the message to the decoder engine of the pictures put out */
static void parsedSet(M4vdecParser *parser)
{
	const VideoM4vUnit *unit;

	unit = (const VideoM4vUnit *)vnzPublicOpenProtected(parser->unit, PARSER_UNIT_SIZE);
	parser->parsed.valid = 1;
	parser->parsed.flush = parser->flush;
	parser->parsed.pictureInfo[0] = unit->pictureInfo[1];
	parser->parsed.pictureInfo[1] = unit->pictureInfo[0];
	vnzPublicCloseProtected((uint32_t)unit, PARSER_UNIT_SIZE);
}

/*
 * 0x841e8a: the pictures laid out in the frames' memory, by the context or
 * at the largest size, the tables pointed at them
 */
static void picturesPrepare(M4vdecParser *parser, M4vdecParserInfo *info)
{
	VideoM4vUnit *unit = (VideoM4vUnit *)vnzPublicOpenPrivate(parser->unit, PARSER_UNIT_SIZE);
	uint32_t horizontal, vertical, pitch, luma, chroma, size, address;
	VideoM4vTable *tables;
	int32_t i;

	if (info->contextSet) {
		vertical = info->contextVertical;
		horizontal = info->contextHorizontal;
	} else {
		vertical = PARSER_DEFAULT_HEIGHT;
		horizontal = PARSER_DEFAULT_WIDTH;
	}
	pitch = (horizontal + 31) & ~31;
	luma = ALIGN256(vertical * horizontal);
	chroma = ALIGN256((int32_t)(vertical * pitch) >> 1);
	size = luma + chroma;
	address = parser->frames;
	unit->width = horizontal;
	unit->tables = PARSER_TABLES;
	unit->pitch = pitch;
	unit->chromaOffset = luma;
	unit->pictureSize2 = chroma;
	unit->pictureSize = size;
	unit->pictureInfo[1] = 0;
	unit->table = 0;
	unit->frameCount = 0;
	unit->frames = info->frames;
	tables = (VideoM4vTable *)vnzPublicOpenPrivate(info->tables,
		PARSER_TABLES * PARSER_TABLE_SIZE);
	for (i = 0; i < PARSER_TABLES; i++) {
		tables[i].address = address;
		tables[i].state = 0;
		tables[i].skipped = 1;
		tables[i].fieldCount = 0;
		address += size;
	}
	vnzPublicClosePrivate((uint32_t)tables, PARSER_TABLES * PARSER_TABLE_SIZE);
	unit->partitionData = address;
	vnzPublicClosePrivate((uint32_t)unit, PARSER_UNIT_SIZE);
	parser->prepared = 1;
}

/* 0x841f6e: the output job, run until the context takes it */
static void outputRun(M4vdecParser *parser)
{
	M4vdecParserInfo *info = parser->info;
	uint32_t index = info->index << 3, busy;
	VnzKernelVThreadContext *locked;

	do {
		locked = vnzKernelVThreadContextLock(parser->context);
		busy = vnzKernelVThreadJobSetNotify(locked, index, 0, m4vdecParserJobOutput,
			(uint32_t)info, parser->stream, parser->job, parser->keepError,
			info->thread, PARSER_FLAG_JOB_DONE);
		vnzKernelVThreadContextUnlock(locked);
	} while (busy);
}

/* 0x841fd4: the flush job, run until the context takes it */
static void flushRun(M4vdecParser *parser)
{
	M4vdecParserInfo *info = parser->info;
	uint32_t index = info->index << 3, busy;
	VnzKernelVThreadContext *locked;

	do {
		locked = vnzKernelVThreadContextLock(parser->context);
		busy = vnzKernelVThreadJobSetNotify(locked, index, 0, m4vdecParserJobFlush,
			(uint32_t)info, parser->stream, parser->job, 0, info->thread,
			PARSER_FLAG_JOB_DONE);
		vnzKernelVThreadContextUnlock(locked);
	} while (busy);
}

/* 0x84203a: a unit with nothing to decode needs nothing more here */
static void unitSkip(M4vdecParser *parser)
{
	(void)parser;
}

static uint32_t pictureMemory(uint32_t tables);

/* 0x84203c */
uint32_t m4vdecQueryParserMemory(void)
{
	return PARSER_FIXED_SIZE + pictureMemory(PARSER_TABLES - 1);
}

/* 0x842054: the pictures' records and their tables */
static uint32_t pictureMemory(uint32_t tables)
{
	return PARSER_FRAMES_SIZE + (tables + 1) * PARSER_TABLE_SIZE;
}

/* 0x842062: the macroblocks' data, 16 bytes each, when the decoder keeps it */
uint32_t m4vdecQueryMacroblockMemory(uint32_t horizontal, uint32_t vertical,
	uint32_t macroblockData)
{
	int32_t macroblocks;

	if (!macroblockData)
		return 0;
	macroblocks = (int32_t)(horizontal * vertical) / 256;
	return ALIGN256(macroblocks * 4 + macroblocks * 12);
}

/* 0x84208e: two frames: luma and chroma (half of the pitched luma) */
uint32_t m4vdecQueryFrameMemory(uint32_t horizontal, uint32_t vertical)
{
	uint32_t pitch = (horizontal + 31) & ~31;

	return ALIGN256(horizontal * vertical + (int32_t)(pitch * vertical) / 2) << 1;
}

/* 0x8420ba: the engine's memory laid out for the decoder's slot */
void m4vdecParserInitialize(uint32_t memory, VideoService *service, int32_t index)
{
	VideoServiceDecoder *decoder = &service->decoders[index];
	uint32_t frames = memory + PARSER_FIXED_SIZE;
	uint32_t tables = frames + PARSER_FRAMES_SIZE;
	M4vdecParserInfo *info;
	M4vdecParser *parser;

	parser = (M4vdecParser *)vnzPublicOpenPrivate(memory, PARSER_FIXED_SIZE);
	vnzCacheInvalidateInside((uint32_t)parser, PARSER_FIXED_SIZE);
	info = (M4vdecParserInfo *)((uint32_t)parser + PARSER_HEADER_SIZE);
	parser->auQueue = decoder->slot.auQueue;
	parser->serviceQueue = service->engineQueue;
	parser->decoderQueue = decoder->slot.innerQueue;
	parser->helperQueue = service->helperQueue;
	parser->wakeQueue = decoder->slot.wakeQueue;
	parser->context = service->encoderContext;
	parser->option = decoder->slot.option;
	info->index = index;
	info->id = decoder->slot.id;
	info->decoderThread = decoder->slot.thread2;
	info->frames = frames;
	info->tables = tables;
	info->commandSize = service->commandSize;
	info->horizontal = service->horizontal;
	info->option5 = timeOf(decoder->option5) << 1;
	info->option4 = timeOf(decoder->option4) << 1;
	vnzPublicClosePrivate((uint32_t)parser, PARSER_FIXED_SIZE);
}

/*
 * The start codes in the unit's bytes, by a window of the last bytes read
 * (shifted up a byte each time): 0x000001 (start_code_prefix) is the window
 * 0x100, a short header's 17-bit 0...1 prefix (short_video_start_marker,
 * gob_resync_marker) its top bits 0x8000 of 0xffff8000, the whole 22-bit
 * short_video_start_marker 0x8000 of 0xfffffc00.
 */
#define WINDOW_START			0xffffff00	/* no zeros yet */
#define WINDOW_START_CODE		0x100
#define WINDOW_SHORT_MASK		0xffff8000
#define WINDOW_SHORT_START_MASK		0xfffffc00
#define WINDOW_SHORT			0x8000

/* the byte after one in the ring */
static inline uint32_t ringNext(uint32_t read, uint32_t ringStart, uint32_t ringEnd)
{
	return read != ringEnd ? read + 1 : ringStart;
}

/* the byte `back` before one, which may be at the ring's start */
static inline uint32_t ringBack(uint32_t next, uint32_t back, uint32_t base, uint32_t ringStart,
	uint32_t ringEnd)
{
	if ((next < base || ringEnd < next) && next < ringStart + back)
		return ringEnd + (next - ringStart) - (back - 1);
	return next - back;
}

/*
 * 0x8421a4: the next start code from where the parser looks: its byte
 * (-1 none), the unit from past its prefix to the next one (or to the end
 * of the data less its trailing zeros)
 */
static int32_t startCodeNext(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	uint32_t ringEnd = stream->ringEnd, ringStart = stream->ringStart, base = stream->base;
	uint32_t read = unit->search, window = WINDOW_START, start = 0, size;
	int32_t left = unit->searchLeft, code = 0;

	/* the start code */
	while (left >= 1) {
		window = (window | *(const uint8_t *)read) << 8;
		read = ringStart ? ringNext(read, ringStart, ringEnd) : read + 1;
		left--;
		if (window == WINDOW_START_CODE) {
			code = *(const uint8_t *)read;
			start = read;
			break;
		}
	}

	/* its end: the next one, its prefix and byte not the unit's */
	if (start) {
		while (left >= 1) {
			window = (window | *(const uint8_t *)read) << 8;
			read = ringStart ? ringNext(read, ringStart, ringEnd) : read + 1;
			left--;
			if (window == WINDOW_START_CODE) {
				left += 4;
				if (ringStart)
					read = ringBack(read, 4, base, ringStart, ringEnd);
				else
					read -= 4;
				break;
			}
		}
	}

	if (!start)
		return -1;
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
		size = read - ringStart + (ringEnd - start) + 2;
	else
		size = read - start + 1;
	unit->start = start;
	unit->size = size;
	unit->search = read;
	unit->searchLeft = left;
	return code;
}

/*
 * 0x8422da: the next short header picture from where the parser looks:
 * from its 17-bit prefix to the next short_video_start_marker (or to the
 * end of the data less its trailing zeros). Without a ring the prefix is
 * looked for past the data's end. Returns 1 when found.
 */
static uint32_t shortVideoNext(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	uint32_t ringEnd = stream->ringEnd, ringStart = stream->ringStart, base = stream->base;
	uint32_t read = unit->search, window = WINDOW_START, start = 0, size;
	int32_t left = unit->searchLeft;

	if (ringStart) {
		if (left < 1)
			return 0;
		while (1) {
			window = (window | *(const uint8_t *)read) << 8;
			read = ringNext(read, ringStart, ringEnd);
			left--;
			if ((window & WINDOW_SHORT_MASK) == WINDOW_SHORT) {
				start = ringBack(read, 3, base, ringStart, ringEnd);
				break;
			}
			if (left < 1)
				break;
		}
		if (start) {
			while (left >= 1) {
				window = (window | *(const uint8_t *)read) << 8;
				read = ringNext(read, ringStart, ringEnd);
				left--;
				if ((window & WINDOW_SHORT_START_MASK) == WINDOW_SHORT) {
					left += 3;
					read = ringBack(read, 3, base, ringStart, ringEnd);
					break;
				}
			}
		}
	} else {
		do {
			window = (window | *(const uint8_t *)read) << 8;
			read++;
			left--;
		} while ((window & WINDOW_SHORT_MASK) != WINDOW_SHORT);
		start = read - 3;
		while (left >= 1) {
			window = (window | *(const uint8_t *)read) << 8;
			read++;
			left--;
			if ((window & WINDOW_SHORT_START_MASK) == WINDOW_SHORT) {
				left += 3;
				read -= 3;
				break;
			}
		}
	}

	if (!start)
		return 0;
	if (!left) {
		do {
			if (read == ringStart)
				read = ringEnd;
			else
				read--;
		} while (!*(const int8_t *)read);
	}
	if ((read < base || ringEnd < read) && start >= base && start <= ringEnd)
		size = read - ringStart + (ringEnd - start) + 2;
	else
		size = read - start + 1;
	unit->start = start == ringStart ? ringEnd : start - 1;
	unit->search = read;
	unit->searchLeft = left;
	unit->size = size + 1;
	return 1;
}

/*
 * 0x84244c: whether a short header's 17-bit prefix comes next, with only
 * zero bytes and bytes below 32 (the prefix's own) before it
 */
static uint32_t shortVideoAhead(const VideoM4vStream *stream, VideoM4vJob *job)
{
	const VideoM4vUnit *unit = job->unit;
	uint32_t ringEnd = stream->ringEnd, ringStart = stream->ringStart;
	uint32_t read = unit->search, window = WINDOW_START, byte;
	int32_t left = unit->searchLeft;

	while (left-- >= 1) {
		byte = *(const uint8_t *)read;
		window = (window | byte) << 8;
		if (byte) {
			if ((window & WINDOW_SHORT_MASK) == WINDOW_SHORT)
				return 1;
			if (!((int32_t)byte < 32))
				return 0;
		}
		if (ringStart)
			read = ringNext(read, ringStart, ringEnd);
		else
			read++;
	}
	return 0;
}

/* the unit's search, kept across a look ahead into it */
typedef struct UnitSearch {
	uint32_t search;
	int32_t searchLeft;
	uint32_t start;
	int32_t size;
} UnitSearch;

static inline void searchSave(UnitSearch *saved, const VideoM4vUnit *unit)
{
	saved->searchLeft = unit->searchLeft;
	saved->search = unit->search;
	saved->start = unit->start;
	saved->size = unit->size;
}

/* the unit's search put back, its bits read again from its start */
static inline void searchRestore(const UnitSearch *saved, const VideoM4vStream *stream,
	VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;

	unit->start = saved->start;
	unit->size = saved->size;
	unit->search = saved->search;
	unit->searchLeft = saved->searchLeft;
	m4vdecBitsBegin(stream, job);
}

/*
 * 0x8424e2: a short header's layer, as H.263 has it (30000/1001 pictures a
 * second, 12:11 pixels): its picture's size from the source format, the unit
 * read again from its start
 */
static void shortVideoLayer(const M4vdecParserInfo *info, const VideoM4vStream *stream,
	VideoM4vJob *job)
{
	VideoBitstream *bits = job->bits;
	VideoM4vUnit *unit = job->unit;
	VideoM4vVol *vol = unit->vol;
	VideoM4vVop *vop = unit->vop;
	UnitSearch saved;

	searchSave(&saved, unit);
	vol->fixedVopRate = 1;
	vol->timeIncrementResolution = 30000;
	vol->aspectRatioInfo = 2;
	vol->pictureHeader = 1;
	vol->shortHeader = 1;
	vol->resyncMarkerDisable = 1;
	vol->complexityEstimationDisable = 1;
	vol->matrixCoefficients = 6;
	vol->fixedVopTimeIncrement = 1001;
	vol->vbvBufferSize = 0;
	vol->parHeight = 0;
	vol->parWidth = 0;
	vol->intraH263 = 0;
	vol->reversibleVlc = 0;
	vol->dataPartitioned = 0;
	vol->height = 0;
	vol->width = 0;
	vol->scalability = 0;
	vol->quantPrecision = 0;
	vol->timeIncrementBits = 0;
	vop->codingType = 0;
	vop->width = 0;
	vop->height = 0;
	vop->intraDcVlcThr = 0;
	vop->fcode = 1;
	vop->quant = 0;
	vop->intraH263 = 0;
	vop->reserved3 = 0;
	vop->reversibleVlc = 0;
	vop->timeIncrementResolution = 30000;

	m4vdecBitsRead(bits, 22);	/* short_video_start_marker */
	m4vdecBitsRead(bits, 8);	/* temporal_reference */
	m4vdecBitsRead(bits, 1);	/* marker_bit */
	m4vdecBitsRead(bits, 1);	/* zero_bit */
	m4vdecBitsRead(bits, 1);	/* split_screen_indicator */
	m4vdecBitsRead(bits, 1);	/* document_camera_indicator */
	m4vdecBitsRead(bits, 1);	/* full_picture_freeze_release */
	switch (m4vdecBitsRead(bits, 3)) {	/* source_format */
	case 1:				/* sub-QCIF */
		vop->width = 128;
		vop->height = 96;
		vop->numGobsInVop = 6;
		vop->runEnd = 8;
		break;
	case 2:				/* QCIF */
		vop->width = 176;
		vop->height = 144;
		vop->numGobsInVop = 9;
		vop->runEnd = 11;
		break;
	case 3:				/* CIF */
		vop->width = 352;
		vop->height = 288;
		vop->numGobsInVop = 18;
		vop->runEnd = 22;
		break;
	case 4:				/* 4CIF */
		vop->width = 704;
		vop->height = 576;
		vop->numGobsInVop = 18;
		vop->runEnd = 88;
		break;
	case 5:				/* 16CIF */
		vop->width = 1408;
		vop->height = 1152;
		vop->numGobsInVop = 18;
		vop->runEnd = 352;
		break;
	}
	if (!stream->sequence) {
		if ((int32_t)info->contextHorizontal < vop->width)
			m4vdecParserError(stream, job, 1, ERROR_WIDTH);
		if ((int32_t)info->contextVertical < vop->height)
			m4vdecParserError(stream, job, 1, ERROR_HEIGHT);
	}
	unit->timeIncrement = 6006;
	searchRestore(&saved, stream, job);
}

/* 0x8426ba: define_vop_complexity_estimation_header(), whose counts no picture uses */
static void complexityEstimation(const M4vdecParserInfo *info, const VideoM4vStream *stream,
	VideoM4vJob *job)
{
	VideoBitstream *bits = job->bits;
	VideoM4vVol *vol = job->unit->vol;
	uint32_t method;

	vol->estimationMethod = 0;
	vol->opaque = 0;
	vol->transparent = 0;
	vol->intraCae = 0;
	vol->interCae = 0;
	vol->noUpdate = 0;
	vol->upsampling = 0;
	vol->intraBlocks = 0;
	vol->interBlocks = 0;
	vol->inter4vBlocks = 0;
	vol->notCodedBlocks = 0;
	vol->dctCoefs = 0;
	vol->dctLines = 0;
	vol->vlcSymbols = 0;
	vol->vlcBits = 0;
	vol->apm = 0;
	vol->npm = 0;
	vol->interpolateMcQ = 0;
	vol->forwBackMcQ = 0;
	vol->halfpel2 = 0;
	vol->halfpel4 = 0;
	vol->sadct = 0;
	vol->quarterpel = 0;
	method = m4vdecBitsRead(bits, 2);
	if (method != 0 && method != 1)
		m4vdecParserError(stream, job, 1, ERROR_HEADER);
	vol->estimationMethod = method;
	if (method != 0 && method != 1)
		return;
	if (!m4vdecBitsRead(bits, 1)) {		/* shape_complexity_estimation_disable */
		vol->opaque = m4vdecBitsRead(bits, 1);
		vol->transparent = m4vdecBitsRead(bits, 1);
		vol->intraCae = m4vdecBitsRead(bits, 1);
		vol->interCae = m4vdecBitsRead(bits, 1);
		vol->noUpdate = m4vdecBitsRead(bits, 1);
		vol->upsampling = m4vdecBitsRead(bits, 1);
	}
	if (!m4vdecBitsRead(bits, 1)) {		/* texture_complexity_estimation_set_1_disable */
		vol->intraBlocks = m4vdecBitsRead(bits, 1);
		vol->interBlocks = m4vdecBitsRead(bits, 1);
		vol->inter4vBlocks = m4vdecBitsRead(bits, 1);
		vol->notCodedBlocks = m4vdecBitsRead(bits, 1);
	}
	m4vdecBitsRead(bits, 1);		/* marker_bit */
	if (!m4vdecBitsRead(bits, 1)) {		/* texture_complexity_estimation_set_2_disable */
		vol->dctCoefs = m4vdecBitsRead(bits, 1);
		vol->dctLines = m4vdecBitsRead(bits, 1);
		vol->vlcSymbols = m4vdecBitsRead(bits, 1);
		vol->vlcBits = m4vdecBitsRead(bits, 1);
	}
	if (!m4vdecBitsRead(bits, 1)) {		/* motion_compensation_complexity_disable */
		vol->apm = m4vdecBitsRead(bits, 1);
		vol->npm = m4vdecBitsRead(bits, 1);
		vol->interpolateMcQ = m4vdecBitsRead(bits, 1);
		vol->forwBackMcQ = m4vdecBitsRead(bits, 1);
		vol->halfpel2 = m4vdecBitsRead(bits, 1);
		vol->halfpel4 = m4vdecBitsRead(bits, 1);
	}
	m4vdecBitsRead(bits, 1);		/* marker_bit */
	if (method == 1 && !m4vdecBitsRead(bits, 1)) {	/* version2_complexity_estimation_disable */
		vol->sadct = m4vdecBitsRead(bits, 1);
		vol->quarterpel = m4vdecBitsRead(bits, 1);
	}
}

/* 0x842860: VisualObject(): a video object, version 1; its matrix_coefficients kept */
static void visualObject(const M4vdecParserInfo *info, const VideoM4vStream *stream,
	VideoM4vJob *job)
{
	VideoBitstream *bits = job->bits;
	VideoM4vUnit *unit = job->unit;

	if (m4vdecBitsRead(bits, 1)) {		/* is_visual_object_identifier */
		if (m4vdecBitsRead(bits, 4) != 1)	/* visual_object_verid */
			m4vdecParserError(stream, job, 1, ERROR_HEADER);
		if (!m4vdecBitsRead(bits, 3))	/* visual_object_priority */
			m4vdecParserError(stream, job, 1, ERROR_HEADER);
	}
	if (m4vdecBitsRead(bits, 4) != 1)	/* visual_object_type: video ID */
		m4vdecParserError(stream, job, 1, ERROR_HEADER);
	if (m4vdecBitsRead(bits, 1)) {		/* video_signal_type */
		m4vdecBitsRead(bits, 3);	/* video_format */
		m4vdecBitsRead(bits, 1);	/* video_range */
		if (m4vdecBitsRead(bits, 1)) {	/* colour_description */
			m4vdecBitsRead(bits, 8);	/* colour_primaries */
			m4vdecBitsRead(bits, 8);	/* transfer_characteristics */
			unit->vol->matrixCoefficients = m4vdecBitsRead(bits, 8);
		}
	}
}

/*
 * 0x842920: a VisualObjectSequence()'s profile_and_level_indication checked:
 * the simple profile's levels, and the advanced simple profile's (nothing calls it)
 */
void m4vdecProfileCheck(const M4vdecParserInfo *info, const VideoM4vStream *stream,
	VideoM4vJob *job)
{
	switch (m4vdecBitsRead(job->bits, 8)) {
	case 0x08:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0xf0:
	case 0xf1:
	case 0xf2:
	case 0xf3:
	case 0xf4:
	case 0xf5:
	case 0xf7:
		break;
	default:
		m4vdecParserError(stream, job, 1, ERROR_OBJECT_TYPE);
	}
}

/* a quantiser matrix, up to its first zero: not used */
static inline void quantMatrixSkip(VideoBitstream *bits)
{
	int32_t i;

	for (i = 0; i < 64; i++)
		if (!m4vdecBitsRead(bits, 8))
			break;
}

static inline void markerBit(VideoBitstream *bits, const VideoM4vStream *stream,
	VideoM4vJob *job)
{
	if (m4vdecBitsRead(bits, 1) != 1)
		m4vdecParserError(stream, job, 1, ERROR_HEADER);
}

/*
 * 0x842996: VideoObjectLayer(): a rectangular, progressive layer of a simple or
 * advanced simple profile, its size checked against the decoder's (unless
 * only the sequence is asked), and the unit's macroblocks and runs from it
 */
static void videoObjectLayer(const M4vdecParserInfo *info, const VideoM4vStream *stream,
	VideoM4vJob *job, uint32_t sequence)
{
	VideoBitstream *bits = job->bits;
	VideoM4vUnit *unit = job->unit;
	VideoM4vVol *vol = unit->vol;
	uint32_t type, value, verid, vbvBufferSize;
	int32_t width, height, count, rest, fit, half, run;

	vol->timeIncrementBits = 0;
	vol->complexityEstimationDisable = 0;
	vol->quantPrecision = 0;
	vol->scalability = 0;
	vol->width = 0;
	vol->height = 0;
	vol->resyncMarkerDisable = 0;
	vol->dataPartitioned = 0;
	vol->reversibleVlc = 0;
	vol->intraH263 = 0;
	vol->timeIncrementResolution = 0;
	vol->shortHeader = 0;
	vol->pictureHeader = 1;
	vol->aspectRatioInfo = 0;
	vol->parWidth = 0;
	vol->parHeight = 0;
	vol->fixedVopRate = 0;
	vol->fixedVopTimeIncrement = 0;
	vol->vbvBufferSize = 0;

	m4vdecBitsRead(bits, 1);		/* random_accessible_vol */
	type = m4vdecBitsRead(bits, 8);		/* video_object_type_indication */
	if (type != 0 && type != 1 && type != 3 && type != 17 && type != 12)
		m4vdecParserError(stream, job, 1, ERROR_OBJECT_TYPE);
	verid = 1;
	if (m4vdecBitsRead(bits, 1)) {		/* is_object_layer_identifier */
		verid = m4vdecBitsRead(bits, 4);
		m4vdecBitsRead(bits, 3);	/* video_object_layer_priority */
	}
	vol->aspectRatioInfo = m4vdecBitsRead(bits, 4);
	if (vol->aspectRatioInfo == 15) {	/* extended_PAR */
		value = m4vdecBitsRead(bits, 8);
		if (!value)
			m4vdecParserError(stream, job, 1, ERROR_WIDTH);
		vol->parWidth = value;
		value = m4vdecBitsRead(bits, 8);
		if (!value)
			m4vdecParserError(stream, job, 1, ERROR_HEIGHT);
		vol->parHeight = value;
	}
	if (m4vdecBitsRead(bits, 1)) {		/* vol_control_parameters */
		if (m4vdecBitsRead(bits, 2) != 1)	/* chroma_format: 4:2:0 */
			m4vdecParserError(stream, job, 1, ERROR_HEADER);
		m4vdecBitsRead(bits, 1);	/* low_delay */
		if (m4vdecBitsRead(bits, 1)) {	/* vbv_parameters */
			m4vdecBitsRead(bits, 15);
			markerBit(bits, stream, job);
			m4vdecBitsRead(bits, 15);
			markerBit(bits, stream, job);
			vbvBufferSize = m4vdecBitsRead(bits, 15) << 3;
			markerBit(bits, stream, job);
			vbvBufferSize |= m4vdecBitsRead(bits, 3);
			m4vdecBitsRead(bits, 11);
			markerBit(bits, stream, job);
			m4vdecBitsRead(bits, 15);
			markerBit(bits, stream, job);
			vol->vbvBufferSize = vbvBufferSize << 11;	/* 16384 bits a unit */
		}
	}
	if (m4vdecBitsRead(bits, 2))		/* video_object_layer_shape: rectangular */
		m4vdecParserError(stream, job, 1, ERROR_HEADER);
	markerBit(bits, stream, job);
	value = m4vdecBitsRead(bits, 16);	/* vop_time_increment_resolution */
	if (!value)
		m4vdecParserError(stream, job, 1, ERROR_HEADER);
	vol->timeIncrementResolution = value;
	count = 0;
	if (value)
		for (rest = value - 1; rest; rest >>= 1)
			count++;
	if (count < 1)
		count = 1;
	vol->timeIncrementBits = count;
	markerBit(bits, stream, job);
	vol->fixedVopRate = m4vdecBitsRead(bits, 1);
	if (vol->fixedVopRate) {
		value = m4vdecBitsRead(bits, count);	/* fixed_vop_time_increment */
		if (!value)
			m4vdecParserError(stream, job, 1, ERROR_HEADER);
		vol->fixedVopTimeIncrement = value;
		/* in the 90 kHz clock, doubled */
		unit->timeIncrement = (int32_t)vol->fixedVopTimeIncrement * 180000 /
			(int32_t)vol->timeIncrementResolution;
	}
	markerBit(bits, stream, job);
	width = m4vdecBitsRead(bits, 13);
	if (!(width < 1921))
		m4vdecParserError(stream, job, 1, ERROR_WIDTH);
	vol->layerWidth = width;
	vol->width = (width + 15) & ~15;
	markerBit(bits, stream, job);
	height = m4vdecBitsRead(bits, 13);
	if (!(height < 1089))
		m4vdecParserError(stream, job, 1, ERROR_HEIGHT);
	vol->layerHeight = height;
	vol->height = (height + 15) & ~15;
	markerBit(bits, stream, job);
	if (!sequence) {
		if ((int32_t)info->horizontal < width)
			m4vdecParserError(stream, job, 1, ERROR_WIDTH);
		if ((int32_t)info->contextHorizontal < width)
			m4vdecParserError(stream, job, 1, ERROR_WIDTH);
		if ((int32_t)info->contextVertical < height)
			m4vdecParserError(stream, job, 1, ERROR_HEIGHT);
	}
	if (m4vdecBitsRead(bits, 1))		/* interlaced */
		m4vdecParserError(stream, job, 1, ERROR_INTERLACED);
	m4vdecBitsRead(bits, 1);		/* obmc_disable */
	if (m4vdecBitsRead(bits, verid == 1 ? 1 : 2))	/* sprite_enable */
		m4vdecParserError(stream, job, 1, ERROR_HEADER);
	if (m4vdecBitsRead(bits, 1)) {		/* not_8_bit */
		m4vdecParserError(stream, job, 1, ERROR_HEADER);
		vol->quantPrecision = m4vdecBitsRead(bits, 4);
		m4vdecBitsRead(bits, 4);	/* bits_per_pixel */
	} else {
		vol->quantPrecision = 5;
	}
	if (m4vdecBitsRead(bits, 1)) {		/* quant_type */
		if (m4vdecBitsRead(bits, 1))	/* load_intra_quant_mat */
			quantMatrixSkip(bits);
		if (m4vdecBitsRead(bits, 1))	/* load_nonintra_quant_mat */
			quantMatrixSkip(bits);
	}
	if (verid != 1 && m4vdecBitsRead(bits, 1))	/* quarter_sample */
		m4vdecParserError(stream, job, 1, ERROR_HEADER);
	vol->complexityEstimationDisable = m4vdecBitsRead(bits, 1);
	if (!vol->complexityEstimationDisable)
		complexityEstimation(info, stream, job);
	vol->resyncMarkerDisable = m4vdecBitsRead(bits, 1);
	vol->dataPartitioned = m4vdecBitsRead(bits, 1);
	if (vol->dataPartitioned)
		vol->reversibleVlc = m4vdecBitsRead(bits, 1);
	else
		vol->reversibleVlc = 0;
	if (verid != 1) {
		if (m4vdecBitsRead(bits, 1)) {	/* newpred_enable */
			m4vdecParserError(stream, job, 1, ERROR_HEADER);
			m4vdecBitsRead(bits, 2);	/* requested_upstream_message_type */
			m4vdecBitsRead(bits, 1);	/* newpred_segment_type */
		}
		m4vdecBitsRead(bits, 1);	/* reduced_resolution_vop_enable */
	}
	vol->scalability = m4vdecBitsRead(bits, 1);
	m4vdecBitsRead(bits, 8);
	if (!sequence && info->contextMacroblock < (uint8_t)vol->dataPartitioned)
		m4vdecParserError(stream, job, 1, ERROR_PARTITIONED);

	/* a run: the rows a command holds, up to half the picture's, and 8 rows at most */
	vol->intraH263 = 0;
	width = vol->width >> 4;
	height = vol->height >> 4;
	fit = ALIGN64(info->commandSize << 7) / ALIGN64(width << 7);
	run = width;
	if (height != 1) {
		half = (height + 1) / 2;
		if (fit < half)
			run = width * fit;
		else
			run = half * width;
	}
	if (height >= 33 && run > width * 8)
		run = width * 8;
	unit->runLength = run;
	unit->macroblocks = width * height;
	unit->widthInMacroblocks = width;
	unit->heightInMacroblocks = height;
}

static uint32_t packetType(const VideoM4vStream *stream, VideoM4vJob *job);

/*
 * 0x842f48: a VOP's types: 3 when it is predicted, or when a later video
 * packet of an intra VOP says it is; 1 intra only. The unit is read again.
 */
static uint32_t vopTypes(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	UnitSearch saved;
	uint32_t type, types;

	searchSave(&saved, unit);
	type = m4vdecBitsRead(job->bits, 2);	/* vop_coding_type */
	unit->vop->codingType = type;
	if (type) {
		types = 3;
	} else {
		types = 1;
		while ((int32_t)m4vdecBitsNextStartCode(stream, job) >= 1)
			if (packetType(stream, job)) {
				types = 3;
				break;
			}
	}
	searchRestore(&saved, stream, job);
	return types;
}

/* 0x842fc6: a short header picture's types: 1 intra, 3 inter; the unit read again */
static uint32_t shortVideoTypes(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoBitstream *bits = job->bits;
	UnitSearch saved;
	uint32_t types;

	searchSave(&saved, job->unit);
	m4vdecBitsRead(bits, 22);	/* short_video_start_marker */
	m4vdecBitsRead(bits, 8);	/* temporal_reference */
	m4vdecBitsRead(bits, 1);
	m4vdecBitsRead(bits, 1);
	m4vdecBitsRead(bits, 1);
	m4vdecBitsRead(bits, 1);
	m4vdecBitsRead(bits, 1);
	m4vdecBitsRead(bits, 3);	/* source_format */
	types = m4vdecBitsRead(bits, 1) ? 3 : 1;	/* picture_coding_type */
	searchRestore(&saved, stream, job);
	return types;
}

/* 0x843062: the picture not decoded: its table skipped, the next table taken */
static void pictureSkip(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoM4vTable *table = job->table;
	VideoM4vUnit *unit = job->unit;

	table->skipped2 = 1;
	table->skipped = 1;
	table->state = 0;
	unit->lastTable = unit->table;
	unit->table = (unit->table + 1) % PARSER_TABLES;
}

/*
 * the picture's types against the access unit's mode (the types it decodes):
 * decoded, or put out only; without a reference (the last picture skipped)
 * only an intra picture is decoded
 */
static uint32_t pictureFlags(const VideoM4vStream *stream, VideoM4vJob *job, uint32_t mode,
	uint32_t lastSkipped, uint32_t (*types)(const VideoM4vStream *, VideoM4vJob *))
{
	VideoM4vUnit *unit = job->unit;

	if ((mode | 3) == mode) {
		if (unit->restart) {
			if (vopTypes(stream, job) != 1 && lastSkipped)
				return PARSER_FLAG_OUTPUT;
		} else if (lastSkipped && types(stream, job) != 1) {
			pictureSkip(stream, job);
			return PARSER_FLAG_OUTPUT;
		}
	} else if ((mode | types(stream, job)) != mode) {
		pictureSkip(stream, job);
		return PARSER_FLAG_OUTPUT;
	}
	unit->load = 1;
	return PARSER_FLAG_DECODE;
}

/*
 * 0x843098: the unit's first job: its headers read up to its first picture,
 * which tells what the engine does with it
 */
void m4vdecParserJobStart(uint32_t infoAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t unused)
{
	const M4vdecParserInfo *info = (const M4vdecParserInfo *)infoAddress;
	const VideoM4vStream *stream;
	VideoM4vTable *tables;
	VideoM4vUnit *unit;
	VideoM4vJob *job;
	uint32_t flags, mode, lastSkipped, layer;
	int32_t error, code;

	stream = (const VideoM4vStream *)vnzPublicOpenProtected(streamAddress, PARSER_STREAM_SIZE);
	job = (VideoM4vJob *)vnzPublicOpenPrivate(jobAddress, PARSER_JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, PARSER_JOB_SIZE);
	unit = (VideoM4vUnit *)vnzPublicOpenPrivate(stream->unit, PARSER_UNIT_SIZE);
	tables = (VideoM4vTable *)vnzPublicOpenPrivate(info->tables,
		unit->tables * PARSER_TABLE_SIZE);
	unit->vol = (VideoM4vVol *)vnzPublicOpenPrivate((uint32_t)unit->vol, PARSER_VOL_SIZE);
	unit->vop = (VideoM4vVop *)vnzPublicOpenPrivate((uint32_t)unit->vop, PARSER_VOP_SIZE);
	unit->restart = (stream->auMode & 8) != 0;
	if (unit->restart)
		unit->table = unit->lastTable;
	job->bits = &unit->bits;
	job->unit = unit;
	job->tables = tables;
	job->table = &tables[unit->table];
	unit->search = stream->base;
	unit->error = 0;
	unit->searchLeft = stream->esSize;
	lastSkipped = tables[unit->lastTable].skipped;
	mode = stream->auMode & 3;
	layer = 1;
	error = 0;
	flags = 0;

	if (shortVideoAhead(stream, job)) {
		error = videoSetJump(job->jump);
		if (error) {
			if (error & ERRORS_FATAL)
				flags = stream->sequence ? PARSER_FLAG_SEQUENCE | PARSER_FLAG_FAILED :
					PARSER_FLAG_FAILED;
			goto done;
		}
		shortVideoNext(stream, job);
		m4vdecBitsBegin(stream, job);
		shortVideoLayer(info, stream, job);
		if (stream->sequence) {
			unit->horizontal = unit->vop->width;
			unit->vertical = unit->vop->height;
			unit->sequenceInfo = unit->vol->vbvBufferSize;
			flags = PARSER_FLAG_SEQUENCE;
		} else {
			flags = pictureFlags(stream, job, mode, lastSkipped, shortVideoTypes);
		}
		goto done;
	}

	for (;;) {
		code = startCodeNext(stream, job);
		if (code < 0) {
			flags = PARSER_FLAG_DRAIN;
			break;
		}
		error = videoSetJump(job->jump);
		if (error & ERRORS_FATAL) {
			flags = stream->sequence ? PARSER_FLAG_SEQUENCE | PARSER_FLAG_FAILED :
				PARSER_FLAG_FAILED;
			break;
		}
		m4vdecBitsBegin(stream, job);
		if (code < 0x20) {		/* video_object_start_code */
			layer = 0;
		} else if (code < 0x30) {	/* video_object_layer_start_code */
			videoObjectLayer(info, stream, job, stream->sequence);
			if (stream->sequence) {
				unit->horizontal = unit->vol->width;
				unit->vertical = unit->vol->height;
				flags = PARSER_FLAG_SEQUENCE;
				break;
			}
			layer = 1;
		} else if (code < 0xb0) {
			;
		} else if (code == 0xb0 || code == 0xb1 || code == 0xb2 || code == 0xb3 ||
			   code == 0xb4) {
			;			/* VOS, its end, user data, GOV */
		} else if (code == 0xb5) {	/* visual_object_start_code */
			visualObject(info, stream, job);
		} else if (code == 0xb6) {	/* vop_start_code */
			if (layer != 1)
				m4vdecParserError(stream, job, 1, ERROR_HEADER);
			unit->vop->vopHeader = 1;
			flags = pictureFlags(stream, job, mode, lastSkipped, vopTypes);
			break;
		} else {
			m4vdecParserError(stream, job, 1, ERROR_START_CODE);
		}
	}

done:
	if (error)
		unit->error = error;
	m4vdecBitsSave(&unit->registers);
	unit->vol = (VideoM4vVol *)vnzPublicClosePrivate((uint32_t)unit->vol, PARSER_VOL_SIZE);
	unit->vop = (VideoM4vVop *)vnzPublicClosePrivate((uint32_t)unit->vop, PARSER_VOP_SIZE);
	vnzPublicClosePrivate((uint32_t)job->tables, unit->tables * PARSER_TABLE_SIZE);
	vnzPublicClosePrivate((uint32_t)job->unit, PARSER_UNIT_SIZE);
	vnzPublicClosePrivate((uint32_t)job, PARSER_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, PARSER_STREAM_SIZE);
	vnzThreadSetFlags(info->thread, flags);
}

/* 0x8433fc: the stream flushed: every table free, no reference left */
void m4vdecParserJobFlush(uint32_t infoAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t unused)
{
	const M4vdecParserInfo *info = (const M4vdecParserInfo *)infoAddress;
	const VideoM4vStream *stream;
	VideoM4vTable *tables;
	VideoM4vUnit *unit;
	VideoM4vJob *job;
	int32_t i;

	stream = (const VideoM4vStream *)vnzPublicOpenProtected(streamAddress, PARSER_STREAM_SIZE);
	job = (VideoM4vJob *)vnzPublicOpenPrivate(jobAddress, PARSER_JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, PARSER_JOB_SIZE);
	unit = (VideoM4vUnit *)vnzPublicOpenPrivate(stream->unit, PARSER_UNIT_SIZE);
	tables = (VideoM4vTable *)vnzPublicOpenPrivate(info->tables,
		unit->tables * PARSER_TABLE_SIZE);
	unit->pictureInfo[1] = 0;
	if ((int8_t)unit->tables != 1) {
		job->tables = tables;
		job->unit = unit;
	}
	for (i = 0; i < unit->tables; i++) {
		tables[i].state = 0;
		tables[i].skipped = 1;
	}
	vnzPublicClosePrivate((uint32_t)tables, unit->tables * PARSER_TABLE_SIZE);
	vnzPublicClosePrivate((uint32_t)unit, PARSER_UNIT_SIZE);
	vnzPublicClosePrivate((uint32_t)job, PARSER_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, PARSER_STREAM_SIZE);
}

/* 0x8434bc: a picture put out only: the unit's error cleared unless it is kept */
void m4vdecParserJobOutput(uint32_t infoAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t keepError)
{
	const M4vdecParserInfo *info = (const M4vdecParserInfo *)infoAddress;
	const VideoM4vStream *stream;
	VideoM4vTable *tables;
	VideoM4vUnit *unit;
	VideoM4vJob *job;

	stream = (const VideoM4vStream *)vnzPublicOpenProtected(streamAddress, PARSER_STREAM_SIZE);
	job = (VideoM4vJob *)vnzPublicOpenPrivate(jobAddress, PARSER_JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, PARSER_JOB_SIZE);
	unit = (VideoM4vUnit *)vnzPublicOpenPrivate(stream->unit, PARSER_UNIT_SIZE);
	tables = (VideoM4vTable *)vnzPublicOpenPrivate(info->tables,
		unit->tables * PARSER_TABLE_SIZE);
	job->unit = unit;
	job->tables = tables;
	if (!keepError)
		unit->error = 0;
	unit->pictureInfo[1] = 0;
	vnzPublicClosePrivate((uint32_t)tables, unit->tables * PARSER_TABLE_SIZE);
	vnzPublicClosePrivate((uint32_t)unit, PARSER_UNIT_SIZE);
	vnzPublicClosePrivate((uint32_t)job, PARSER_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, PARSER_STREAM_SIZE);
}

/* a complexity estimate the layer says the VOP has: not used */
static inline void estimateSkip(VideoBitstream *bits, uint8_t present, uint32_t count)
{
	if ((int8_t)present)
		m4vdecBitsRead(bits, count);
}

/*
 * 0x843560: read_vop_complexity_estimation_header(): an I or P VOP's estimates
 * of the counts the layer enabled (Sony reads halfpel4's before halfpel2's)
 */
static void vopComplexityEstimation(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	const VideoM4vVol *vol = unit->vol;
	VideoBitstream *bits = job->bits;

	if (vol->estimationMethod != 0 && vol->estimationMethod != 1)
		return;
	if (!unit->vop->codingType) {
		estimateSkip(bits, vol->opaque, 8);
		estimateSkip(bits, vol->transparent, 8);
		estimateSkip(bits, vol->intraCae, 8);
		estimateSkip(bits, vol->interCae, 8);
		estimateSkip(bits, vol->noUpdate, 8);
		estimateSkip(bits, vol->upsampling, 8);
		estimateSkip(bits, vol->intraBlocks, 8);
		estimateSkip(bits, vol->notCodedBlocks, 8);
		estimateSkip(bits, vol->dctCoefs, 8);
		estimateSkip(bits, vol->dctLines, 8);
		estimateSkip(bits, vol->vlcSymbols, 8);
		estimateSkip(bits, vol->vlcBits, 4);
		estimateSkip(bits, vol->sadct, 8);
	}
	if (unit->vop->codingType != 1)
		return;
	estimateSkip(bits, vol->opaque, 8);
	estimateSkip(bits, vol->transparent, 8);
	estimateSkip(bits, vol->intraCae, 8);
	estimateSkip(bits, vol->interCae, 8);
	estimateSkip(bits, vol->noUpdate, 8);
	estimateSkip(bits, vol->upsampling, 8);
	estimateSkip(bits, vol->intraBlocks, 8);
	estimateSkip(bits, vol->notCodedBlocks, 8);
	estimateSkip(bits, vol->dctCoefs, 8);
	estimateSkip(bits, vol->dctLines, 8);
	estimateSkip(bits, vol->vlcSymbols, 8);
	estimateSkip(bits, vol->vlcBits, 4);
	estimateSkip(bits, vol->interBlocks, 8);
	estimateSkip(bits, vol->inter4vBlocks, 8);
	estimateSkip(bits, vol->apm, 8);
	estimateSkip(bits, vol->npm, 8);
	estimateSkip(bits, vol->forwBackMcQ, 8);
	estimateSkip(bits, vol->halfpel4, 8);
	estimateSkip(bits, vol->halfpel2, 8);
	estimateSkip(bits, vol->sadct, 8);
	estimateSkip(bits, vol->quarterpel, 8);
}

/* a VOP's header cleared, all but whether it is still to be read */
static inline void vopClear(VideoM4vVop *vop)
{
	vop->codingType = 0;
	vop->width = 0;
	vop->height = 0;
	vop->intraDcVlcThr = 0;
	vop->fcode = 0;
	vop->quant = 0;
	vop->runQuant = 0;
	vop->timeIncrementResolution = 0;
	vop->moduloTimeBase = 0;
	vop->timeIncrement = 0;
	vop->intraH263 = 0;
	vop->reserved3 = 0;
	vop->reversibleVlc = 0;
	vop->macroblockNumber = 0;
	vop->reserved4 = 0;
	vop->numGobsInVop = 0;
	vop->runEnd = 0;
	vop->vopCoded = 0;
	vop->pastRun = 0;
}

/* the command's picture: its macroblocks and the decoder's rows for them */
static inline void commandRows(VideoM4vCommand *command, int32_t width, int32_t height)
{
	command->widthInMacroblocks = width;
	command->heightInMacroblocks = height;
	command->mode = width < 40 ? 6 : 8;
	command->mbRowSize = width * 8;
	command->vectorRowSize = ALIGN64(width * 16);
	command->lumaRowSize = ALIGN64(width * 512);
	command->chromaRowSize = ALIGN64(width * 128);
}

/* modulo_time_base: its ones, and the zero after them, as bits */
static inline int32_t moduloTimeBase(VideoBitstream *bits)
{
	int32_t value = m4vdecBitsRead(bits, 1);
	uint32_t bit;

	if (value == 1)
		do {
			bit = m4vdecBitsRead(bits, 1);
			value = value << 1 | bit;
		} while (bit == 1);
	return value;
}

/*
 * 0x843736: vop()'s header up to its macroblocks (an I or P VOP); the command
 * and the unit's macroblocks from the layer's size
 */
void m4vdecVopHeader(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoBitstream *bits = job->bits;
	VideoM4vUnit *unit = job->unit;
	VideoM4vCommand *command = (VideoM4vCommand *)unit->command;
	VideoM4vVop *vop = unit->vop;
	const VideoM4vVol *vol = unit->vol;
	uint32_t type, marker, marker2, coded, quant, fcode;
	int32_t width, height;

	vopClear(vop);
	type = m4vdecBitsRead(bits, 2);		/* vop_coding_type */
	vop->codingType = type;
	command->codingType = type;
	vop->moduloTimeBase = moduloTimeBase(bits);
	marker = m4vdecBitsRead(bits, 1);
	vop->timeIncrement = m4vdecBitsRead(bits, vol->timeIncrementBits);
	marker2 = m4vdecBitsRead(bits, 1);
	coded = m4vdecBitsRead(bits, 1);	/* vop_coded */
	vop->vopCoded = coded;
	fcode = 0;
	if (!coded) {
		quant = 0;
		vop->codingType = 1;
		command->codingType = 1;
	} else {
		if (type == 1)
			command->roundingType = m4vdecBitsRead(bits, 1);
		if (!(int8_t)vol->complexityEstimationDisable)
			vopComplexityEstimation(stream, job);
		vop->intraDcVlcThr = m4vdecBitsRead(bits, 3);
		quant = m4vdecBitsRead(bits, vol->quantPrecision);
		vop->quant = quant;
		if (type) {
			fcode = m4vdecBitsRead(bits, 3);	/* vop_fcode_forward */
			vop->fcode = fcode;
		}
	}
	vop->intraH263 = vol->intraH263;
	vop->reversibleVlc = vol->reversibleVlc;
	vop->width = (uint16_t)vol->width;
	vop->height = (uint16_t)vol->height;
	vop->timeIncrementResolution = vol->timeIncrementResolution;
	unit->horizontal = vop->width;
	unit->vertical = vop->height;
	width = (uint16_t)vol->width >> 4;
	height = (uint16_t)vol->height >> 4;
	commandRows(command, width, height);
	command->shortHeader = 0;
	unit->firstMacroblock = 0;
	unit->pictureNew = 1;
	unit->picturePackets = 0;
	if (type != 0 && type != 1)
		m4vdecParserError(stream, job, 2, ERROR_VOP);
	if (marker != 1)
		m4vdecParserError(stream, job, 2, ERROR_VOP);
	if (marker2 != 1)
		m4vdecParserError(stream, job, 2, ERROR_VOP);
	if (vop->vopCoded) {
		if (!quant)
			m4vdecParserError(stream, job, 2, ERROR_VOP);
		if (type && !fcode)
			m4vdecParserError(stream, job, 2, ERROR_VOP);
	}
}

/*
 * 0x84394e: a short header's picture header (H.263): its size from the source
 * format (sub-QCIF for one not defined), its quantiser; a run is a few GOBs
 */
void m4vdecShortHeader(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoBitstream *bits = job->bits;
	VideoM4vUnit *unit = job->unit;
	VideoM4vCommand *command = (VideoM4vCommand *)unit->command;
	VideoM4vVop *vop = unit->vop;
	uint32_t quant;
	int32_t width, height, run;

	vopClear(vop);
	m4vdecBitsRead(bits, 22);	/* short_video_start_marker */
	m4vdecBitsRead(bits, 8);	/* temporal_reference */
	m4vdecBitsRead(bits, 1);	/* marker_bit */
	m4vdecBitsRead(bits, 1);	/* zero_bit */
	m4vdecBitsRead(bits, 1);	/* split_screen_indicator */
	m4vdecBitsRead(bits, 1);	/* document_camera_indicator */
	m4vdecBitsRead(bits, 1);	/* full_picture_freeze_release */
	switch (m4vdecBitsRead(bits, 3)) {	/* source_format */
	case 2:
		vop->width = 176;
		vop->height = 144;
		vop->numGobsInVop = 9;
		vop->runEnd = 11;
		break;
	case 3:
		vop->width = 352;
		vop->height = 288;
		vop->numGobsInVop = 18;
		vop->runEnd = 22;
		break;
	case 4:
		vop->width = 704;
		vop->height = 576;
		vop->numGobsInVop = 18;
		vop->runEnd = 88;
		break;
	case 5:
		vop->width = 1408;
		vop->height = 1152;
		vop->numGobsInVop = 18;
		vop->runEnd = 352;
		break;
	default:
		vop->width = 128;
		vop->height = 96;
		vop->numGobsInVop = 6;
		vop->runEnd = 8;
		break;
	}
	vop->codingType = m4vdecBitsRead(bits, 1);	/* picture_coding_type */
	command->codingType = vop->codingType;
	m4vdecBitsRead(bits, 4);	/* four_reserved_zero_bits */
	quant = m4vdecBitsRead(bits, 5);	/* vop_quant */
	vop->quant = quant;
	m4vdecBitsRead(bits, 1);	/* zero_bit */
	while (m4vdecBitsRead(bits, 1) == 1)	/* pei */
		m4vdecBitsRead(bits, 8);	/* psupp */
	width = vop->width / 16;
	height = vop->height / 16;
	unit->horizontal = vop->width;
	unit->vertical = vop->height;
	commandRows(command, width, height);
	command->roundingType = 0;
	command->shortHeader = 1;
	vop->fcode = 1;
	vop->vopCoded = 1;
	if (width < 12)
		run = width * 5;
	else if (width < 21)
		run = width * 8;
	else if (width < 41)
		run = width * 6;
	else if (width < 46)
		run = width * 5;
	else if (width < 61)
		run = width * 4;
	else if (width < 81)
		run = width * 3;
	else
		run = width * 2;
	unit->widthInMacroblocks = width;
	unit->heightInMacroblocks = height;
	unit->firstMacroblock = 0;
	unit->pictureNew = 1;
	unit->picturePackets = 0;
	if (run > (height - 1) * width)
		run = (height - 1) * width;
	unit->runLength = run;
	unit->macroblocks = width * height;
	if (!quant)
		m4vdecParserError(stream, job, 2, ERROR_VOP);
}

/* 0x843b90: a GOB's header, when its resync marker is next: 1, its quantiser taken */
uint32_t m4vdecGobHeader(VideoM4vJob *job)
{
	VideoBitstream *bits = job->bits;
	VideoM4vUnit *unit = job->unit;
	uint32_t quant;

	if (m4vdecBitsPeek(bits, 17) != 1)
		return 0;
	m4vdecBitsRead(bits, 17);	/* gob_resync_marker */
	m4vdecBitsRead(bits, 5);	/* gob_number */
	m4vdecBitsRead(bits, 2);	/* gob_frame_id */
	quant = m4vdecBitsRead(bits, 5);	/* quant_scale */
	unit->vop->quant = quant;
	return 1;
}

/* 0x843be4: macroblock_number's bits, for a picture of these macroblocks */
static uint32_t macroblockNumberBits(int32_t width, int32_t height)
{
	int32_t rest = width * height - 1;
	uint32_t count = 0;

	do {
		count++;
		rest >>= 1;
	} while (rest);
	return count;
}

/* 0x843bf8: vop_time_increment's bits, for a vop_time_increment_resolution */
static uint32_t timeIncrementBits(uint32_t resolution)
{
	uint32_t count = 16;

	for (resolution--; !(resolution & 0x8000); resolution = resolution << 1 | 1)
		count--;
	return count;
}

/*
 * 0x843c16: video_packet_header(), when a resync marker is after the byte
 * boundary: 1, its macroblock and quantiser taken; a header extension must
 * repeat the VOP's
 */
uint32_t m4vdecPacketHeader(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	VideoBitstream *bits = job->bits;
	VideoM4vVop *vop = unit->vop;
	uint32_t numberBits, resync, number, quant, extension, type, fcode;
	int32_t width, height, modulo, increment, count;

	width = vop->width / 16;
	height = vop->height / 16;
	numberBits = 0;
	if (width * height >= 1)
		numberBits = macroblockNumberBits(width, height);
	if (!unit->vop->codingType)
		resync = 17;
	else
		resync = unit->vop->fcode + 16;
	if (m4vdecBitsPeekAligned(bits, resync) != 1)
		return 0;
	m4vdecBitsByteAlign(bits);
	m4vdecBitsRead(bits, resync);		/* resync_marker */
	number = m4vdecBitsRead(bits, numberBits);	/* macroblock_number */
	unit->vop->macroblockNumber = number;
	quant = m4vdecBitsRead(bits, unit->vol->quantPrecision);	/* quant_scale */
	if (quant)
		unit->vop->quant = quant;
	extension = m4vdecBitsRead(bits, 1);	/* header_extension_code */
	type = 0;
	modulo = 0;
	fcode = 0;
	increment = 0;
	if (extension) {
		modulo = moduloTimeBase(bits);
		count = timeIncrementBits(unit->vop->timeIncrementResolution);
		if (count < 1)
			count = 1;
		m4vdecBitsRead(bits, 1);	/* marker_bit */
		increment = m4vdecBitsRead(bits, count);	/* vop_time_increment */
		m4vdecBitsRead(bits, 1);	/* marker_bit */
		type = m4vdecBitsRead(bits, 2);	/* vop_coding_type */
		unit->vop->codingType = type;
		unit->vop->intraDcVlcThr = m4vdecBitsRead(bits, 3);
		if (type) {
			fcode = m4vdecBitsRead(bits, 3);	/* vop_fcode_forward */
			unit->vop->fcode = fcode;
		}
	}
	if (!((int32_t)number < 3601))
		m4vdecParserError(stream, job, 3, ERROR_VOP);
	if (extension) {
		if (unit->vop->moduloTimeBase != modulo)
			m4vdecParserError(stream, job, 3, ERROR_VOP);
		if (unit->vop->timeIncrement != increment)
			m4vdecParserError(stream, job, 3, ERROR_VOP);
		if (type != 0 && type != 1)
			m4vdecParserError(stream, job, 3, ERROR_VOP);
		if ((uint8_t)unit->vop->codingType != type)
			m4vdecParserError(stream, job, 3, ERROR_VOP);
		if (type && !fcode)
			m4vdecParserError(stream, job, 3, ERROR_VOP);
	}
	return 1;
}

/* a run's place: its row and column, and its macroblocks (to the row's end, or whole rows) */
static inline void runPlace(VideoM4vUnit *unit, int32_t *row, int32_t *column)
{
	int32_t width = unit->widthInMacroblocks, rows;

	*row = unit->firstMacroblock / width;
	*column = unit->firstMacroblock % width;
	rows = unit->runLength / width;
	if (unit->heightInMacroblocks < *row + rows)
		rows = unit->heightInMacroblocks - *row;
	unit->runMacroblocks = *column ? width - *column : width * rows;
	unit->runRow = *row;
	unit->runOffset = *column;
}

/* 0x843e06: a run on the macroblock unit: its rows' buffers, and the VOP's registers */
void m4vdecRunUnit(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	const VideoM4vCommand *command;
	VideoHelperJob *run;
	uint32_t picture, vop, place;
	int32_t row, column, width;

	runPlace(unit, &row, &column);
	width = unit->widthInMacroblocks;
	run = (VideoHelperJob *)vnzPublicOpenPrivate(shared->request, sizeof(VideoHelperJob));
	command = (const VideoM4vCommand *)unit->command;
	if (!row) {
		VIDEO_UNIT[UNIT_RESET] = 1;
		VIDEO_UNIT[UNIT_VERSION] = shared->version;
		VIDEO_UNIT[UNIT_MODE] = UNIT_MODE_DECODE | UNIT_MODE_M4V;
		VIDEO_UNIT[UNIT_PARAMETER(0)] = command->mbRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(1)] = command->vectorRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(2)] = command->lumaRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(3)] = command->chromaRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(4)] = command->chromaRowSize;
	}
	VIDEO_UNIT[UNIT_WIDTH] = width - column;
	VIDEO_UNIT[UNIT_BUFFER(0)] = vnzToPublic(run->buffers[0]);
	VIDEO_UNIT[UNIT_BUFFER(1)] = vnzToPublic(run->buffers[4]);
	VIDEO_UNIT[UNIT_BUFFER(2)] = vnzToPublic(run->buffers[5]);
	VIDEO_UNIT[UNIT_BUFFER(3)] = vnzToPublic(run->buffers[6]);
	VIDEO_UNIT[UNIT_BUFFER(4)] = vnzToPublic(run->buffers[7]);
	if (unit->load) {
		picture = (command->shortHeader & 1) | (unit->vol->resyncMarkerDisable & 1) << 8 |
			(unit->vop->quant & 0x1f) << 16;
		VIDEO_UNIT[UNIT_M4V_PICTURE] = picture;
	}
	vop = (unit->vop->codingType & 3) | (unit->vop->intraDcVlcThr & 7) << 8 |
		(unit->vop->fcode & 7) << 16;
	VIDEO_UNIT[UNIT_M4V_VOP] = vop;
	if (unit->load) {
		place = (width & 0xff) | (column & 0xff) << 8 | (row & 0x1fff) << 16;
		VIDEO_UNIT[UNIT_M4V_RUN] = place;
	}
	VIDEO_UNIT[UNIT_M4V_CONTROL] = 0;
	vnzPublicClosePrivate((uint32_t)run, sizeof(VideoHelperJob));
}

/*
 * 0x844044: a data partitioned run, decoded here: the DC predictors reset at
 * a packet's start, the quantiser kept across the packet's runs
 */
void m4vdecRunPartitioned(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	VideoHelperJob *run;
	int32_t row, column, done;

	runPlace(unit, &row, &column);
	run = (VideoHelperJob *)vnzPublicOpenPrivate(shared->request, sizeof(VideoHelperJob));
	job->dcRow = (int16_t *)shared->tables[3];
	if (!unit->load) {
		job->quant = *(int32_t *)shared->tables[4];
	} else {
		m4vdecDcReset(job);
		job->quant = unit->vop->quant;
	}
	if (!unit->vop->codingType)
		done = m4vdecRunIntra(job, run);
	else
		done = m4vdecRunInter(job, run);
	unit->doneMacroblocks = done;
	*(int32_t *)shared->tables[4] = job->quant;
	vnzPublicClosePrivate((uint32_t)run, sizeof(VideoHelperJob));
}

/* 0x8440f2: a VOP not coded: its macroblocks copied from the reference here */
void m4vdecRunNotCoded(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	VideoHelperJob *run;
	int32_t row, column;

	runPlace(unit, &row, &column);
	run = (VideoHelperJob *)vnzPublicOpenPrivate(shared->request, sizeof(VideoHelperJob));
	unit->doneMacroblocks = m4vdecVopNotCoded(job, run);
	vnzPublicClosePrivate((uint32_t)run, sizeof(VideoHelperJob));
}

/* 0x84415c: a failed unit's run: its place only */
void m4vdecRunFailed(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job)
{
	int32_t row, column;

	runPlace(job->unit, &row, &column);
}

/*
 * 0x84419a: the coding type a video packet's header extension gives its VOP,
 * 0 without one (the bits after the resync marker's start)
 */
static uint32_t packetType(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	VideoBitstream *bits = job->bits;
	uint32_t numberBits;
	int32_t count;

	numberBits = macroblockNumberBits((uint16_t)unit->vol->width >> 4,
		(uint16_t)unit->vol->height >> 4);
	m4vdecBitsRead(bits, 8);
	m4vdecBitsRead(bits, 17);
	m4vdecBitsRead(bits, numberBits);	/* macroblock_number */
	m4vdecBitsRead(bits, unit->vol->quantPrecision);	/* quant_scale */
	if (!m4vdecBitsRead(bits, 1))		/* header_extension_code */
		return 0;
	while (m4vdecBitsRead(bits, 1) == 1)	/* modulo_time_base */
		;
	count = timeIncrementBits(unit->vol->timeIncrementResolution);
	if (count < 1)
		count = 1;
	m4vdecBitsRead(bits, 1);		/* marker_bit */
	m4vdecBitsRead(bits, count);		/* vop_time_increment */
	m4vdecBitsRead(bits, 1);		/* marker_bit */
	return m4vdecBitsRead(bits, 2);		/* vop_coding_type */
}
