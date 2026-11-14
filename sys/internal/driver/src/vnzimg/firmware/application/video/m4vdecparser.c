/*
 * The MPEG-4 decoder's parser engine thread (0x841730-0x8471ac): its memory,
 * the access units it takes, the VThread jobs it runs on them, and the
 * headers they parse.
 */
#include "m4vdecparser.h"

#define ALIGN64(size)		(((size) + 63) & ~63)
#define ALIGN256(size)		(((size) + 255) & ~255)

void m4vdecParserAu(uint32_t memory, uint32_t request);
void m4vdecParserJobStart(uint32_t info, uint32_t stream, uint32_t job, uint32_t unused);
void m4vdecParserJobFlush(uint32_t info, uint32_t stream, uint32_t job, uint32_t unused);
void m4vdecParserJobOutput(uint32_t info, uint32_t stream, uint32_t job, uint32_t keepError);
void m4vdecDecoderParsed(uint32_t memory, uint32_t parsed);

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
	headerUnit->vop = (VideoM4vVop *)((uint32_t)headerUnit + PARSER_UNIT_SIZE);
	header->info = (M4vdecParserInfo *)(memory + PARSER_HEADER_SIZE);
	header->stream = memory + PARSER_HEADER_SIZE + PARSER_INFO_SIZE;
	header->job = memory + PARSER_HEADER_SIZE + PARSER_INFO_SIZE + PARSER_STREAM_SIZE;
	header->unit = (uint32_t)headerUnit;
	headerUnit->packet = (VideoM4vPacket *)((uint32_t)headerUnit + PARSER_UNIT_SIZE +
		PARSER_VOP_SIZE);
	parser = (M4vdecParser *)vnzPublicOpenPrivate(memory, PARSER_HEADER_SIZE);
	unit = (VideoM4vUnit *)parser->unit;
	info = parser->info;
	info->thread = vnzThreadCurrent();
	unit->lastTable = 0;
	unit->reserved8 = 0;
	info->contextSet = 0;
	unit->error = 0;
	unit->tables = 1;
	unit->vop->info2 = 1;
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
		if (unit->vop->notCoded)
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
		tables[i].reserved2[0] = 1;
		tables[i].fieldCount = 0;
		address += size;
	}
	vnzPublicClosePrivate((uint32_t)tables, PARSER_TABLES * PARSER_TABLE_SIZE);
	unit->reserved5 = address;
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
