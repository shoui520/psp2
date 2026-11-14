/*
 * The AVC decoder's parser engine thread (0x824ca0-0x82739c): its memory,
 * the access units it takes, the VThread jobs it runs on them, and the
 * parameter sets and SEI messages they parse.
 */
#include "avcdecparser.h"

#define ALIGN256(size)		(((size) + 255) & ~255)

/* the parameter cache: the lists' heads and two bytes a node, aligned */
#define CACHE_HEADER		6
#define CACHE_MAX_STREAMS	32
#define CACHE_MAX_FRAMES	256

void avcdecParserAu(uint32_t memory, uint32_t request);
void avcdecParserJobStart(uint32_t info, uint32_t stream, uint32_t job, uint32_t unused);
void avcdecParserJobFlush(uint32_t info, uint32_t stream, uint32_t job, uint32_t unused);
void avcdecParserJobOutput(uint32_t info, uint32_t stream, uint32_t job, uint32_t keepError);
void avcdecPictureOutput(const VideoStream *stream, VideoSliceJob *job, uint32_t flush);
void avcdecSliceHeader(const VideoStream *stream, VideoSliceJob *job);
void avcdecSliceStart(const VideoStream *stream, VideoSliceJob *job);
void avcdecPictureSkip(const VideoStream *stream, VideoSliceJob *job);
void avcdecParseSei(const VideoStream *stream, VideoSliceJob *job);
void avcdecParseSps(const AvcdecParserInfo *info, const VideoStream *stream, VideoSliceJob *job);
void avcdecParsePps(const VideoStream *stream, VideoSliceJob *job);
void avcdecParseAud(const VideoStream *stream, VideoSliceJob *job);
void avcdecParseEndOfSequence(const VideoStream *stream, VideoSliceJob *job);
void avcdecParseEndOfStream(const VideoStream *stream, VideoSliceJob *job);
void avcdecParseFiller(const VideoStream *stream, VideoSliceJob *job);
void avcdecParseSpsExtension(const VideoStream *stream, VideoSliceJob *job);

/* a time stamp from its two words */
static inline VideoTime timeOf(const uint32_t *words)
{
	return (VideoTime)((uint64_t)words[1] << 32 | words[0]);
}

static AvcdecParser *parserBegin(uint32_t memory);
static void parserEnd(AvcdecParser *parser);
static uint32_t parserWait(AvcdecParser *parser);
static void requestFinish(AvcdecParser *parser);
static void requestStatus(AvcdecParser *parser);
static void parsedSet(AvcdecParser *parser);
static void picturesPrepare(AvcdecParser *parser, AvcdecParserInfo *info);
static void outputRun(AvcdecParser *parser);
static void flushRun(AvcdecParser *parser);
static void unitSkip(AvcdecParser *parser);

/* 0x824ca0: the engine's thread, until it is stopped */
void avcdecParserThread(uint32_t memory, uint32_t codec)
{
	AvcdecParser *parser = parserBegin(memory);
	AvcdecParserInfo *info = parser->info;
	VnzKernelMessageQueue *auQueue = parser->auQueue;
	VnzKernelMessageQueue *serviceQueue = parser->serviceQueue;
	VnzKernelMessageQueue *decoderQueue = parser->decoderQueue;
	VnzKernelMessageQueue *wakeQueue = parser->wakeQueue;
	VnzKernelMessageQueue *helperQueue = parser->helperQueue;
	uint32_t flags;

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
					vnzKernelMessageSend(decoderQueue, avcdecDecoderParsed,
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

/* 0x824e54: the memory laid out and opened, the unit cleared */
static AvcdecParser *parserBegin(uint32_t memory)
{
	AvcdecParser *header = (AvcdecParser *)memory, *parser;
	AvcdecParserInfo *info;
	VideoNal *nal;

	header->info = (AvcdecParserInfo *)(memory + PARSER_HEADER_SIZE);
	header->stream = memory + PARSER_HEADER_SIZE + PARSER_INFO_SIZE;
	header->job = memory + PARSER_HEADER_SIZE + PARSER_INFO_SIZE + PARSER_STREAM_SIZE;
	header->nal = memory + PARSER_HEADER_SIZE + PARSER_INFO_SIZE + PARSER_STREAM_SIZE +
		PARSER_JOB_SIZE;
	parser = (AvcdecParser *)vnzPublicOpenPrivate(memory, PARSER_HEADER_SIZE);
	nal = (VideoNal *)parser->nal;
	info = parser->info;
	info->thread = vnzThreadCurrent();
	info->contextSet = 0;
	nal->outputTime = (int32_t)0x80000000;
	nal->outputPending = 0;
	nal->vertical = 0;
	nal->horizontal = 0;
	nal->contextOption = 0;
	nal->fieldDecode = 0;
	nal->numOfRefFrames = 0;
	nal->skip = 0;
	nal->error = 0;
	nal->pictures = 1;
	nal->lastPictureCount = 0;
	nal->pictureCount = 0;
	nal->lastPts = 0;
	nal->lastTime = 0;
	nal->timeWrap = 0;
	nal->frameDuration = -1;
	nal->fieldCount = 0;
	nal->frameFields = 2;
	nal->firstPts = info->option5;
	nal->firstDts = info->option4;
	nal->idrPending = 0;
	parser->info = (AvcdecParserInfo *)vnzPublicOpenRaw((uint32_t)info, PARSER_INFO_SIZE);
	parser->state = PARSER_IDLE;
	parser->keepError = 0;
	parser->flush = 0;
	parser->output = 0;
	parser->prepared = 0;
	return parser;
}

/* 0x824f4c */
static void parserEnd(AvcdecParser *parser)
{
	vnzPublicCloseRaw((uint32_t)parser->info, PARSER_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)parser, PARSER_HEADER_SIZE);
}

/* 0x824f70: the thread's flags; an exit stops the engine */
static uint32_t parserWait(AvcdecParser *parser)
{
	uint32_t flags = vnzThreadWaitFlags(PARSER_FLAGS);

	if (flags & PARSER_FLAG_EXIT)
		parser->state = PARSER_STOPPED;
	return flags;
}

/*
 * 0x824f96: an access unit: its bytes mapped and its first job started; a
 * drain only noted; a context set only remembered.
 */
void avcdecParserAu(uint32_t memory, uint32_t requestAddress)
{
	AvcdecParser *parser = (AvcdecParser *)memory;
	AvcdecParserInfo *info = parser->info, *context;
	VideoEngineRequest *request;
	VideoStream *stream;
	uint32_t start, esSize, bufferSize, esOffset, end, line, first, size, index;
	VnzKernelVThreadContext *locked;

	parser->request = requestAddress;
	request = (VideoEngineRequest *)vnzPublicOpenPrivate(requestAddress, REQUEST_SIZE);
	stream = (VideoStream *)vnzPublicOpenPrivate(parser->stream, PARSER_STREAM_SIZE);
	if (request->flags & VIDEO_ENGINE_DRAIN) {
		stream->nal = parser->nal;
		vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
		vnzPublicClosePrivate((uint32_t)stream, PARSER_STREAM_SIZE);
		vnzThreadSetFlags(info->thread, PARSER_FLAG_DRAIN);
		return;
	}
	if (request->flags & VIDEO_ENGINE_CONTEXT) {
		context = (AvcdecParserInfo *)vnzPublicOpenPrivate(
			vnzPublicCloseRaw((uint32_t)info, PARSER_INFO_SIZE), PARSER_INFO_SIZE);
		parser->frames = request->pictures;
		context->contextHorizontal = request->horizontal;
		context->contextVertical = request->vertical;
		context->contextRefFrames = request->numOfRefFrames;
		context->fieldDecode = (request->contextFlags & VIDEO_ENGINE_CONTEXT_FIELD) != 0;
		context->contextSet = 1;
		context->contextOption = (request->contextFlags & VIDEO_ENGINE_CONTEXT_OPTION) != 0;
		stream->pts = timeOf(request->dts);
		stream->dts = timeOf(request->pts);
		stream->option = request->option;
		request->status = 0;
		vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
		vnzPublicClosePrivate((uint32_t)stream, PARSER_STREAM_SIZE);
		info = (AvcdecParserInfo *)vnzPublicOpenRaw(
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
		esSize = request->esSize;
		esOffset = request->esOffset;
		stream->esOffset = esOffset;
		bufferSize = request->bufferSize;
	} else {
		/* a restart: the unit the stream has, again */
		esSize = stream->esSize;
		start = stream->ringStart;
		bufferSize = stream->bufferSize;
		esOffset = stream->esOffset;
		request->bufferSize = bufferSize;
		request->esOffset = esOffset;
		request->esSize = esSize;
	}
	end = start + esOffset;
	line = end & ~63;
	if (bufferSize < esSize + esOffset) {
		/* the unit wraps: its end at the ring's start */
		first = bufferSize - esOffset;
		stream->ringStart = vnzPublicOpenRaw(start, ALIGN64(esSize - first));
		stream->mapped = vnzPublicOpenRaw(line, ALIGN64(first));
		stream->base = end - line + stream->mapped;
		end = first + stream->base;
	} else {
		size = ALIGN64(esSize + esOffset) - (esOffset & ~63);
		stream->mapped = vnzPublicOpenRaw(line, size);
		stream->base = end - line + stream->mapped;
		stream->ringStart = 0;
		end = stream->mapped + size;
	}
	stream->ringEnd = end - 1;
	stream->esSize = esSize;
	stream->bufferSize = bufferSize;
	stream->sequence = (request->flags & VIDEO_ENGINE_SEQUENCE) != 0;
	if (request->flags & VIDEO_ENGINE_RESTART)
		stream->auMode = request->auMode | VIDEO_ENGINE_RESTART;
	else
		stream->auMode = request->auMode;
	stream->cacheSize = info->cacheSize;
	stream->parameterCache = info->parameterCache;
	stream->parameterSets = info->parameterSets;
	stream->parameterSets2 = info->parameterSets2;
	stream->pictureData = info->pictureData;
	stream->pictures = info->pictures;
	stream->pictureExtras = info->pictureExtras;
	stream->nal = parser->nal;
	request->id = info->id;
	request->thread = info->thread;
	request->queue = parser->decoderQueue;
	request->stream = parser->stream;
	request->contextFlags = 0;
	request->job = parser->job;
	stream->pts = timeOf(request->dts);
	stream->dts = timeOf(request->pts);
	stream->option = request->option;
	if (info->fieldDecode)
		request->contextFlags |= VIDEO_ENGINE_CONTEXT_FIELD;
	if (info->contextOption)
		request->contextFlags |= VIDEO_ENGINE_CONTEXT_OPTION;
	if (parser->option)
		request->contextFlags |= 0x08;
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
	vnzPublicClosePrivate((uint32_t)stream, PARSER_STREAM_SIZE);
	locked = vnzKernelVThreadContextLock(parser->context);
	vnzKernelVThreadJobSet(locked, index, 0, avcdecParserJobStart, (uint32_t)info,
		parser->stream,
		parser->job, 0);
	vnzKernelVThreadContextUnlock(locked);
}

/* 0x8252f6: nothing to wake */
void avcdecParserWake(uint32_t memory, uint32_t request)
{
	(void)memory;
	(void)request;
}

/* 0x8252f8: the request answered with the unit's error, its bytes unmapped */
static void requestFinish(AvcdecParser *parser)
{
	const VideoNal *nal = (const VideoNal *)vnzPublicOpenProtected(parser->nal, PARSER_NAL_SIZE);
	VideoEngineRequest *request;
	VideoStream *stream;
	uint32_t bufferSize, esSize, esOffset, first;

	request = (VideoEngineRequest *)vnzPublicOpenPrivate(parser->request, REQUEST_SIZE);
	request->status = nal->error;
	if (!(request->flags & VIDEO_ENGINE_DRAIN)) {
		stream = (VideoStream *)vnzPublicOpenPrivate(parser->stream, PARSER_STREAM_SIZE);
		bufferSize = request->bufferSize;
		esSize = request->esSize;
		esOffset = request->esOffset;
		if (bufferSize < esSize + esOffset) {
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
	vnzPublicCloseProtected((uint32_t)nal, PARSER_NAL_SIZE);
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
}

/* 0x8253bc: the request answered with the sequence the unit found */
static void requestStatus(AvcdecParser *parser)
{
	const VideoNal *nal = (const VideoNal *)vnzPublicOpenProtected(parser->nal, PARSER_NAL_SIZE);
	VideoEngineRequest *request;

	request = (VideoEngineRequest *)vnzPublicOpenPrivate(parser->request, REQUEST_SIZE);
	request->status = nal->error;
	if ((uint32_t)nal->error < VIDEO_ERROR_FATAL) {
		request->numOfRefFrames = nal->numOfRefFrames;
		request->horizontal = nal->horizontal;
		request->vertical = nal->vertical;
		request->contextFlags = 0;
		if (nal->fieldDecode)
			request->contextFlags |= VIDEO_ENGINE_CONTEXT_FIELD;
		if (nal->contextOption)
			request->contextFlags |= VIDEO_ENGINE_CONTEXT_OPTION;
	} else {
		request->vertical = 0;
		request->horizontal = 0;
		request->numOfRefFrames = 0;
		request->contextFlags = 0;
	}
	vnzPublicCloseProtected((uint32_t)nal, PARSER_NAL_SIZE);
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
}

/* 0x825466: the message to the decoder engine of the pictures put out */
static void parsedSet(AvcdecParser *parser)
{
	const VideoNal *nal = (const VideoNal *)vnzPublicOpenProtected(parser->nal, PARSER_NAL_SIZE);

	parser->parsed.valid = 1;
	parser->parsed.flush = parser->flush;
	parser->parsed.pictureInfo[0] = nal->outputCount;
	parser->parsed.pictureInfo[1] = nal->outputStart;
	vnzPublicCloseProtected((uint32_t)nal, PARSER_NAL_SIZE);
}

/* 0x8254a0: the pictures laid out in the frames' memory, by the sequence or the context */
static void picturesPrepare(AvcdecParser *parser, AvcdecParserInfo *info)
{
	VideoNal *nal = (VideoNal *)vnzPublicOpenPrivate(parser->nal, PARSER_NAL_SIZE);
	uint32_t horizontal, vertical, count, pitch, luma, chroma, field, size, address, data;
	VideoAvcPicture *pictures;
	uint8_t *extra;
	int32_t macroblocks, i;

	if (info->contextSet) {
		vertical = info->contextVertical;
		horizontal = info->contextHorizontal;
		count = info->contextRefFrames;
	} else {
		count = nal->numOfRefFrames;
		horizontal = nal->horizontal;
		vertical = nal->vertical;
	}
	count += 2;
	pitch = (horizontal + 31) & ~31;
	macroblocks = (int32_t)(vertical * horizontal) / 256;
	luma = ALIGN256(vertical * horizontal);
	chroma = ALIGN256((int32_t)(vertical * pitch) >> 1);
	field = 0;
	if (info->fieldDecode)
		field = ALIGN256(macroblocks << 6);
	size = luma + chroma + field;
	address = parser->frames;
	nal->lumaSize = luma;
	nal->chromaSize = chroma;
	nal->frameSize = size;
	nal->pitch = pitch;
	nal->width = horizontal;
	nal->pictures = count;
	nal->decoderData = info->decoderData;
	nal->frameCount = 0;
	nal->pocPeriod = 0;
	nal->outputEnd = 0;
	nal->outputDone = 0;
	nal->outputSlot = 0;
	nal->outputCount = 0;
	nal->outputCounts[0] = 0;
	nal->outputCounts[1] = 0;
	pictures = (VideoAvcPicture *)vnzPublicOpenPrivate(info->pictures, count * PICTURE_SIZE);
	extra = (uint8_t *)info->pictureExtras;
	data = info->pictureData;
	for (i = 0; i < (int32_t)count; i++) {
		pictures[i].data = data;
		data += PICTURE_DATA_SIZE;
		pictures[i].address = address;
		address += size;
		pictures[i].referenceInfo = 0;
		pictures[i].free = 1;
		pictures[i].outputState = 0;
		pictures[i].pocPeriod = 0;
		extra[i * PICTURE_EXTRA_SIZE + 8] = 0;
		pictures[i].fieldCount = 0;
	}
	vnzPublicClosePrivate((uint32_t)pictures, count * PICTURE_SIZE);
	vnzPublicClosePrivate((uint32_t)nal, PARSER_NAL_SIZE);
	parser->prepared = 1;
}

/* 0x8255e8: the output job, run until the context takes it */
static void outputRun(AvcdecParser *parser)
{
	AvcdecParserInfo *info = parser->info;
	uint32_t index = info->index << 3, busy;
	VnzKernelVThreadContext *locked;

	do {
		locked = vnzKernelVThreadContextLock(parser->context);
		busy = vnzKernelVThreadJobSetNotify(locked, index, 0, avcdecParserJobOutput,
			(uint32_t)info,
			parser->stream, parser->job, parser->keepError, info->thread,
			PARSER_FLAG_JOB_DONE);
		vnzKernelVThreadContextUnlock(locked);
	} while (busy);
}

/* 0x82564c: the flush job, run until the context takes it */
static void flushRun(AvcdecParser *parser)
{
	AvcdecParserInfo *info = parser->info;
	uint32_t index = info->index << 3, busy;
	VnzKernelVThreadContext *locked;

	do {
		locked = vnzKernelVThreadContextLock(parser->context);
		busy = vnzKernelVThreadJobSetNotify(locked, index, 0, avcdecParserJobFlush,
			(uint32_t)info,
			parser->stream, parser->job, 0, info->thread, PARSER_FLAG_JOB_DONE);
		vnzKernelVThreadContextUnlock(locked);
	} while (busy);
}

/* 0x8256b0: a unit with nothing to decode needs nothing more here */
static void unitSkip(AvcdecParser *parser)
{
	(void)parser;
}

/* 0x8256f8: the parameter cache's size */
static uint32_t cacheMemory(uint32_t maxStreams, uint32_t maxFrames)
{
	if (maxStreams >= CACHE_MAX_STREAMS)
		maxStreams = 0;
	if (maxFrames >= CACHE_MAX_FRAMES)
		maxFrames = 0;
	return ALIGN64((maxStreams + maxFrames) * 2 + CACHE_HEADER);
}

/* 0x825716 */
static uint32_t parameterSetMemory(uint32_t maxStreams)
{
	return ALIGN64(maxStreams * PARAMETER_SET_SIZE);
}

/* 0x825726 */
static uint32_t parameterSet2Memory(uint32_t maxFrames)
{
	return ALIGN64(maxFrames * PARAMETER_SET2_SIZE);
}

/* 0x825732 */
static uint32_t streamDataMemory(uint32_t maxStreams)
{
	return ALIGN256(maxStreams * 256);
}

/* 0x825740 */
static uint32_t pictureDataMemory(uint32_t frames)
{
	return ALIGN256((frames + 2) * PICTURE_DATA_SIZE);
}

/* 0x825750: the decoder's data, the pictures and their extras */
static uint32_t pictureMemory(uint32_t frames)
{
	return ALIGN64(2048 + (frames + 2) * PICTURE_SIZE + (frames + 2) * PICTURE_EXTRA_SIZE);
}

/* 0x8256b2 */
uint32_t avcdecQueryParserMemory(uint32_t maxStreams, uint32_t maxFrames)
{
	uint32_t size = PARSER_FIXED_SIZE + cacheMemory(maxStreams, maxFrames);

	size += parameterSetMemory(maxStreams);
	size += parameterSet2Memory(maxFrames);
	size += streamDataMemory(maxStreams);
	size += pictureDataMemory(MAX_PICTURES);
	size += pictureMemory(MAX_PICTURES);
	return size + 256;
}

/* 0x825768: a field picture's macroblock data, for every frame */
uint32_t avcdecQueryFieldMemory(uint32_t numOfRefFrames, uint32_t horizontal, uint32_t vertical)
{
	int32_t macroblocks = (int32_t)(horizontal * vertical) / 256;

	return ALIGN256(macroblocks << 6) * (numOfRefFrames + 2);
}

/* 0x82578a: the frames: luma and chroma (half of the pitched luma) */
uint32_t avcdecQueryFrameMemory(uint32_t numOfRefFrames, uint32_t horizontal, uint32_t vertical)
{
	uint32_t pitch = (horizontal + 31) & ~31;

	return ALIGN256(horizontal * vertical + (int32_t)(pitch * vertical) / 2) *
		(numOfRefFrames + 2);
}

/* 0x8257b8: the engine's memory laid out for the decoder's slot */
void avcdecParserInitialize(uint32_t memory, VideoService *service, int32_t index)
{
	VideoServiceDecoder *decoder = &service->decoders[index];
	uint32_t maxStreams = decoder->slot.maxStreams, maxFrames = decoder->slot.maxFrames;
	uint32_t streamData, pictureData, cache, cacheSize, sets, sets2, decoderData;
	VideoParameterCache *parameterCache;
	AvcdecParserInfo *info;
	AvcdecParser *parser;
	int32_t i;

	streamData = (memory + PARSER_FIXED_SIZE + 255) & ~255;
	pictureData = streamData + streamDataMemory(maxStreams);
	cache = pictureData + pictureDataMemory(MAX_PICTURES);
	cacheSize = cacheMemory(maxStreams, maxFrames);
	sets = cache + cacheSize;
	sets2 = sets + parameterSetMemory(maxStreams);
	decoderData = sets2 + parameterSet2Memory(maxFrames);

	parser = (AvcdecParser *)vnzPublicOpenPrivate(memory, PARSER_FIXED_SIZE);
	vnzCacheInvalidateInside((uint32_t)parser, PARSER_FIXED_SIZE);
	info = (AvcdecParserInfo *)((uint32_t)parser + PARSER_HEADER_SIZE);
	parser->auQueue = decoder->slot.auQueue;
	parser->serviceQueue = service->engineQueue;
	parser->decoderQueue = decoder->slot.innerQueue;
	parser->helperQueue = service->helperQueue;
	parser->wakeQueue = decoder->slot.wakeQueue;
	parser->context = service->encoderContext;
	parser->option = decoder->slot.option;
	info->id = decoder->slot.id;
	info->index = index;
	info->decoderThread = decoder->slot.thread2;
	info->cacheSize = cacheSize;
	info->parameterCache = cache;
	info->parameterSets = sets;
	info->parameterSets2 = sets2;
	info->pictureData = pictureData;
	info->decoderData = decoderData;
	info->pictures = decoderData + 2048;
	info->streamData = streamData;
	info->pictureExtras = decoderData + 2048 + (MAX_PICTURES + 2) * PICTURE_SIZE;
	info->commandSize = service->commandSize;
	info->horizontal = service->horizontal;
	info->option5 = timeOf(decoder->option5) << 1;
	info->option4 = timeOf(decoder->option4) << 1;

	parameterCache = (VideoParameterCache *)vnzPublicOpenPrivate(cache, 64);
	vnzCacheInvalidateInside((uint32_t)parameterCache, 64);
	if (maxStreams >= CACHE_MAX_STREAMS)
		maxStreams = 0;
	parameterCache->lists[0].capacity = maxStreams;
	parameterCache->lists[1].capacity = maxFrames < CACHE_MAX_FRAMES ? maxFrames : 0;
	parameterCache->lists[0].count = 0;
	parameterCache->lists[1].count = 0;
	parameterCache->lists[0].head = 0;
	parameterCache->lists[1].head = 0;
	/* a list not kept: each set's id is its index */
	if (parameterCache->lists[0].capacity == 0)
		for (i = 0; i < CACHE_MAX_STREAMS; i++)
			((uint8_t *)info->parameterSets)[i * PARAMETER_SET_SIZE] = i + 1;
	if (parameterCache->lists[1].capacity == 0)
		for (i = 0; i < CACHE_MAX_FRAMES; i++)
			((uint8_t *)info->parameterSets2)[i * PARAMETER_SET2_SIZE] = i + 1;
	vnzPublicClosePrivate((uint32_t)parameterCache, 64);
	vnzPublicClosePrivate((uint32_t)parser, PARSER_FIXED_SIZE);
}

/*
 * 0x8259a0: the unit's first job: its NAL units read up to its first slice,
 * which tells what the engine does with the picture.
 */
void avcdecParserJobStart(uint32_t infoAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t unused)
{
	const AvcdecParserInfo *info = (const AvcdecParserInfo *)infoAddress;
	const VideoStream *stream;
	VideoAvcPicture *pictures;
	uint32_t flags, mode, types, saved[4];
	int32_t error, type, picture;
	VideoSliceJob *job;
	VideoNal *nal;

	stream = (const VideoStream *)vnzPublicOpenProtected(streamAddress, PARSER_STREAM_SIZE);
	job = (VideoSliceJob *)vnzPublicOpenPrivate(jobAddress, PARSER_JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, PARSER_JOB_SIZE);
	nal = (VideoNal *)vnzPublicOpenPrivate(stream->nal, PARSER_NAL_SIZE);
	pictures = (VideoAvcPicture *)vnzPublicOpenPrivate(info->pictures,
		nal->pictures * PICTURE_SIZE);
	job->parser.nal = nal;
	job->pictures = pictures;
	picture = 0;
	if (!stream->sequence) {
		/* a picture not waiting for output, or none */
		for (picture = 0; picture < nal->pictures; picture++)
			if (!pictures[picture].outputState)
				break;
		if (picture >= nal->pictures) {
			error = 0x8022;
			flags = PARSER_FLAG_FAILED;
			goto done;
		}
	}
	error = 0;
	job->parser.bits = &nal->bits;
	job->picture = &pictures[picture];
	job->fieldDecode = info->fieldDecode;
	job->contextOption = info->contextOption;
	flags = 0;
	nal->picture = picture;
	nal->search = stream->base;
	nal->refIdc = 0;
	nal->startPending = 1;
	nal->error = 0;
	nal->seiFound = 0;
	nal->searchLeft = stream->esSize;
	do {
		type = videoNalNext(stream, &job->parser, 1);
		if (type < 1)
			break;
		error = videoSetJump(job->jump);
		if (error & (VIDEO_ERROR_FATAL | 0x8000)) {
			flags = PARSER_FLAG_FAILED;
			goto sequence;
		}
		videoBitsBegin(stream, &job->parser);
		switch (type) {
		case NAL_SLICE:
		case NAL_IDR:
			mode = stream->auMode & 7;
			nal->restart = (stream->auMode & 8) != 0;
			if (stream->sequence) {
				flags = PARSER_FLAG_SEQUENCE;
				break;
			}
			if ((mode | 7) == mode) {
				/* every slice type decoded */
				avcdecSliceHeader(stream, job);
				avcdecSliceStart(stream, job);
				nal->load = 1;
				nal->failed = 0;
				nal->scanned = 0;
			} else {
				/* the slice types the unit has, its search kept */
				saved[0] = nal->searchLeft;
				saved[1] = nal->search;
				saved[2] = nal->size;
				saved[3] = nal->start;
				types = videoNalScan(stream, &job->parser);
				nal->start = saved[3];
				nal->size = saved[2];
				nal->search = saved[1];
				nal->searchLeft = saved[0];
				videoBitsBegin(stream, &job->parser);
				avcdecSliceHeader(stream, job);
				avcdecSliceStart(stream, job);
				if ((mode | types) != mode) {
					avcdecPictureSkip(stream, job);
					flags = PARSER_FLAG_OUTPUT;
					break;
				}
				nal->load = 1;
				nal->failed = 0;
				nal->scanned = 1;
			}
			flags = PARSER_FLAG_DECODE;
			nal->startPending = 0;
			break;
		case 2:
		case 3:
		case 4:
			error = 0x2010;		/* data partitions */
			break;
		case NAL_SEI:
			if (!stream->sequence) {
				avcdecParseSei(stream, job);
				nal->seiFound = 1;
			}
			break;
		case NAL_SPS:
			avcdecParseSps(info, stream, job);
			break;
		case NAL_PPS:
			avcdecParsePps(stream, job);
			break;
		case NAL_AUD:
			avcdecParseAud(stream, job);
			break;
		case 10:
			avcdecParseEndOfSequence(stream, job);
			break;
		case 11:
			avcdecParseEndOfStream(stream, job);
			break;
		case NAL_FILLER:
			avcdecParseFiller(stream, job);
			break;
		case 13:
			avcdecParseSpsExtension(stream, job);
			break;
		default:
			error = 0x2001;
			break;
		}
	} while (type != NAL_SLICE && type != NAL_IDR);
	if (!flags) {
		error = 0x2001;
		flags = PARSER_FLAG_FAILED;
	}
sequence:
	if (stream->sequence)
		flags |= PARSER_FLAG_SEQUENCE;
done:
	if (error)
		nal->error = error;
	vnzPublicClosePrivate((uint32_t)job->pictures, nal->pictures * PICTURE_SIZE);
	vnzPublicClosePrivate((uint32_t)job->parser.nal, PARSER_NAL_SIZE);
	vnzCachePurge((uint32_t)job, PARSER_JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)job, PARSER_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, PARSER_STREAM_SIZE);
	vnzThreadSetFlags(info->thread, flags);
	(void)unused;
}

/* 0x825c58: the flush job: every picture put out */
void avcdecParserJobFlush(uint32_t infoAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t unused)
{
	const AvcdecParserInfo *info = (const AvcdecParserInfo *)infoAddress;
	const VideoStream *stream;
	VideoAvcPicture *pictures;
	VideoSliceJob *job;
	VideoNal *nal;

	stream = (const VideoStream *)vnzPublicOpenProtected(streamAddress, PARSER_STREAM_SIZE);
	job = (VideoSliceJob *)vnzPublicOpenPrivate(jobAddress, PARSER_JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, PARSER_JOB_SIZE);
	nal = (VideoNal *)vnzPublicOpenPrivate(stream->nal, PARSER_NAL_SIZE);
	pictures = (VideoAvcPicture *)vnzPublicOpenPrivate(info->pictures,
		nal->pictures * PICTURE_SIZE);
	if (nal->pictures == 1) {
		nal->outputCount = 0;
	} else {
		job->parser.nal = nal;
		job->pictures = pictures;
		avcdecPictureOutput(stream, job, 1);
	}
	vnzPublicClosePrivate((uint32_t)pictures, nal->pictures * PICTURE_SIZE);
	vnzPublicClosePrivate((uint32_t)nal, PARSER_NAL_SIZE);
	vnzCachePurge((uint32_t)job, PARSER_JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)job, PARSER_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, PARSER_STREAM_SIZE);
	(void)unused;
}

/* 0x825d12: the output job: the pictures due put out */
void avcdecParserJobOutput(uint32_t infoAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t keepError)
{
	const AvcdecParserInfo *info = (const AvcdecParserInfo *)infoAddress;
	const VideoStream *stream;
	VideoAvcPicture *pictures;
	VideoSliceJob *job;
	VideoNal *nal;

	stream = (const VideoStream *)vnzPublicOpenProtected(streamAddress, PARSER_STREAM_SIZE);
	job = (VideoSliceJob *)vnzPublicOpenPrivate(jobAddress, PARSER_JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, PARSER_JOB_SIZE);
	nal = (VideoNal *)vnzPublicOpenPrivate(stream->nal, PARSER_NAL_SIZE);
	pictures = (VideoAvcPicture *)vnzPublicOpenPrivate(info->pictures,
		nal->pictures * PICTURE_SIZE);
	job->parser.nal = nal;
	job->pictures = pictures;
	if (!keepError)
		nal->error = 0;
	avcdecPictureOutput(stream, job, 0);
	vnzPublicClosePrivate((uint32_t)pictures, nal->pictures * PICTURE_SIZE);
	vnzPublicClosePrivate((uint32_t)nal, PARSER_NAL_SIZE);
	vnzCachePurge((uint32_t)job, PARSER_JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)job, PARSER_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, PARSER_STREAM_SIZE);
}

/* the parameter sets' and SEI's helpers */
static void seiMessage(const VideoStream *stream, VideoSliceJob *job);
static void trailingBits(const VideoStream *stream, VideoSliceJob *job, uint32_t stage, uint32_t code);
static void spsProfile(const VideoStream *stream, VideoSliceJob *job, VideoAvcSps *sps);
static void spsLevel(const AvcdecParserInfo *info, const VideoStream *stream, VideoSliceJob *job,
	VideoAvcSps *sps);
static void spsVui(const VideoStream *stream, VideoSliceJob *job, VideoAvcSps *sps);
static void vuiHrd(VideoSliceJob *job, VideoAvcVui *vui, VideoAvcHrd *hrd);
static void seiPayload(const VideoStream *stream, VideoSliceJob *job, uint32_t type, int32_t size);
static void seiBufferingPeriod(const VideoStream *stream, VideoSliceJob *job);
static void seiPicTiming(const VideoStream *stream, VideoSliceJob *job);
static void seiPanScan(const VideoStream *stream, VideoSliceJob *job);
static void seiRecoveryPoint(const VideoStream *stream, VideoSliceJob *job);

/* NumClockTS by pic_struct */
static const int8_t numClockTs[9] = { 1, 1, 1, 2, 2, 3, 3, 2, 3 };

/* seq_parameter_set_rbsp(): into the stream's table, kept by id */
void avcdecParseSps(const AvcdecParserInfo *info, const VideoStream *stream, VideoSliceJob *job)
{
	VideoBitstream *bits = job->parser.bits;
	VideoNal *nal = job->parser.nal;
	uint32_t profile, set0, set1, set2, set3, level, id, value, type, cropping;
	uint32_t alwaysZero, cropLeft, cropRight, cropTop, cropBottom;
	int32_t index, nonRef, topToBottom, cycle, i, widthInMbs, heightInMbs;
	int32_t width, height, rows, half;
	VideoAvcSps *sps;

	profile = videoBitsRead(bits, 8);
	set0 = videoBitsRead(bits, 1);
	set1 = videoBitsRead(bits, 1);
	set2 = videoBitsRead(bits, 1);
	set3 = videoBitsRead(bits, 1);
	if (videoBitsRead(bits, 4))
		videoParserError(stream, &job->parser, 0, 0x8006);
	level = videoBitsRead(bits, 8);
	id = videoBitsUe(bits);
	if (id >= AVC_MAX_SPS)
		videoParserError(stream, &job->parser, 0, 0x8006);
	index = videoCacheAdd((VideoParameterCache *)stream->parameterCache, id);
	if (index >= VIDEO_CACHE_EVICTED) {
		index -= VIDEO_CACHE_EVICTED;
		nal->error = 0x200a;
	}
	nal->parameterSet = index;
	sps = (VideoAvcSps *)vnzPublicOpenPrivate(stream->parameterSets +
		index * PARAMETER_SET_SIZE, PARAMETER_SET_SIZE);
	job->parameterSet = (uint32_t)sps;
	sps->profile = profile;
	sps->constraintSet0 = set0;
	sps->level = level;
	sps->constraintSet1 = set1;
	sps->constraintSet2 = set2;
	sps->constraintSet3 = set3;
	spsProfile(stream, job, sps);
	sps->frameMbsOnly = 1;

	value = videoBitsUe(bits);
	if (value >= 13)
		videoParserError(stream, &job->parser, 1, 0x8006);
	sps->log2MaxFrameNum = value + 4;
	sps->maxFrameNum = 1 << (value + 4);
	type = videoBitsUe(bits);
	if (type >= 3)
		videoParserError(stream, &job->parser, 1, 0x8006);
	sps->picOrderCntType = type;
	value = 0;
	alwaysZero = 0;
	nonRef = 0;
	topToBottom = 0;
	cycle = 0;
	if (type == 0) {
		value = videoBitsUe(bits);
		if (value >= 13)
			videoParserError(stream, &job->parser, 1, 0x8006);
	} else if (type == 1) {
		alwaysZero = videoBitsRead(bits, 1);
		nonRef = videoBitsSe(bits);
		topToBottom = videoBitsSe(bits);
		cycle = videoBitsUe(bits);
		if (cycle >= 256)
			videoParserError(stream, &job->parser, 1, 0x8006);
		for (i = 0; i < cycle; i++)
			sps->offsetForRefFrame[i] = videoBitsSe(bits);
	}
	sps->log2MaxPicOrderCntLsb = value + 4;
	sps->maxPicOrderCntLsb = 1 << (value + 4);
	sps->deltaPicOrderAlwaysZero = alwaysZero;
	sps->offsetForNonRefPic = nonRef;
	sps->offsetForTopToBottomField = topToBottom;
	sps->numRefFramesInPicOrderCntCycle = cycle;
	sps->numRefFrames = videoBitsUe(bits);
	sps->gapsInFrameNumAllowed = videoBitsRead(bits, 1);
	value = videoBitsUe(bits);
	if (value >= 120)
		videoParserError(stream, &job->parser, 1, 0x4013);
	widthInMbs = value + 1;
	heightInMbs = videoBitsUe(bits) + 1;
	/* frame_mbs_only_flag: no field coding */
	if (videoBitsRead(bits, 1) != 1)
		videoParserError(stream, &job->parser, 1, 0x4012);
	sps->direct8x8Inference = videoBitsRead(bits, 1);
	cropping = videoBitsRead(bits, 1);
	cropLeft = cropRight = cropTop = cropBottom = 0;
	if (cropping) {
		cropLeft = videoBitsUe(bits);
		cropRight = videoBitsUe(bits);
		cropTop = videoBitsUe(bits);
		cropBottom = videoBitsUe(bits);
	}
	sps->cropLeft = cropLeft;
	sps->frameCropping = cropping;
	sps->cropRight = cropRight;
	sps->cropBottom = cropBottom;
	sps->cropTop = cropTop;

	width = widthInMbs * 16;
	height = heightInMbs * 16;
	if (!stream->sequence) {
		nal->vertical = height;
		nal->horizontal = width;
		if (info->contextSet) {
			if (info->horizontal < nal->horizontal)
				videoParserError(stream, &job->parser, 1, 0x4013);
			if (info->contextHorizontal < nal->horizontal)
				videoParserError(stream, &job->parser, 1, 0x4013);
			if (info->contextVertical < nal->vertical)
				videoParserError(stream, &job->parser, 1, 0x4014);
		}
		/* the rows of 136-byte commands the service's buffer holds, half the picture */
		rows = (int32_t)ALIGN64(info->commandSize * 136) / (int32_t)ALIGN64(widthInMbs * 136);
		half = (heightInMbs + 1) / 2;
		if (rows >= half)
			rows = half;
		if (heightInMbs >= 33 && rows > 8)
			rows = 8;
	} else {
		/* only the sequence: the largest of the stream's */
		if (nal->horizontal < width)
			nal->horizontal = width;
		if (nal->vertical < height)
			nal->vertical = height;
		rows = 0;
	}
	sps->heightInMbs = heightInMbs;
	sps->widthInMbs = widthInMbs;
	sps->width = width;
	sps->height = height;
	sps->rowsPerRun = rows;
	sps->id = id;
	spsLevel(info, stream, job, sps);

	sps->vuiPresent = videoBitsRead(bits, 1);
	sps->frameRateCode = AVC_FRAME_RATE_UNKNOWN;
	if (sps->vuiPresent) {
		sps->vui = info->streamData + index * 256;
		spsVui(stream, job, sps);
		vnzL2Writeback(sps->vui, sizeof(VideoAvcVui));
	} else {
		sps->vui = 0;
		sps->numReorderFrames = sps->maxDpbFrames;
	}
	trailingBits(stream, job, 1, 0x8006);

	if (!stream->sequence) {
		nal->numOfRefFrames = sps->maxDpbFrames;
		nal->numReorderFrames = sps->numReorderFrames;
		if (info->contextSet && info->contextRefFrames < nal->numOfRefFrames)
			videoParserError(stream, &job->parser, 1, 0x2000);
		/* the context's frames are the pictures kept */
		sps->maxDpbFrames = info->contextRefFrames;
		nal->numOfRefFrames = info->contextRefFrames;
	} else if (nal->numOfRefFrames < sps->maxDpbFrames) {
		nal->numOfRefFrames = sps->maxDpbFrames;
	}
	if (sps->maxDpbFrames < sps->numRefFrames)
		videoParserError(stream, &job->parser, 1, 0x2000);
	vnzPublicClosePrivate((uint32_t)sps, PARAMETER_SET_SIZE);
}

/* seq_parameter_set_extension_rbsp(): not used */
void avcdecParseSpsExtension(const VideoStream *stream, VideoSliceJob *job)
{
}

/* pic_parameter_set_rbsp(): the profiles' subset, by id beside its sequence */
void avcdecParsePps(const VideoStream *stream, VideoSliceJob *job)
{
	VideoBitstream *bits = job->parser.bits;
	VideoNal *nal = job->parser.nal;
	VideoParameterCache *cache = (VideoParameterCache *)stream->parameterCache;
	uint32_t id, spsId, entropy, code, l0, l1, transform8x8;
	int32_t spsIndex, index, value;
	const VideoAvcSps *sps;
	VideoAvcPps *pps;
	uint32_t block;

	id = videoBitsUe(bits);
	if (id >= 256)
		videoParserError(stream, &job->parser, 0, 0x8007);
	spsId = videoBitsUe(bits);
	if (spsId >= AVC_MAX_SPS)
		videoParserError(stream, &job->parser, 0, 0x8007);
	nal->spsId = spsId;
	spsIndex = videoCacheFind(cache, spsId);
	if (spsIndex < 0)
		videoParserError(stream, &job->parser, 0, 0x4017);
	index = videoCacheAdd2(cache, id);
	if (index >= VIDEO_CACHE_EVICTED) {
		index -= VIDEO_CACHE_EVICTED;
		nal->error = 0x200b;
	}
	sps = (const VideoAvcSps *)vnzPublicOpenProtected(stream->parameterSets +
		spsIndex * PARAMETER_SET_SIZE, PARAMETER_SET_SIZE);
	/* four sets to a line */
	block = vnzPublicOpenPrivate((stream->parameterSets2 + index * PARAMETER_SET2_SIZE) & ~63,
		PARAMETER_SET2_SIZE);
	job->parameterSet = (uint32_t)sps;
	job->parameterSet2 = block;
	pps = (VideoAvcPps *)(block + (index & 3) * PARAMETER_SET2_SIZE);
	if (sps->id != spsId)
		videoParserError(stream, &job->parser, 3, 0x8007);
	pps->id = id;
	pps->spsId = spsId;

	/* CABAC is not decoded */
	entropy = videoBitsRead(bits, 1);
	if (!stream->sequence) {
		code = job->contextOption ? 0x2015 : 0x4015;
		if (entropy)
			videoParserError(stream, &job->parser, 3, code);
	}
	nal->contextOption = entropy;
	pps->entropyCodingMode = entropy;
	pps->bottomFieldPicOrderInFramePresent = videoBitsRead(bits, 1);
	/* num_slice_groups_minus1: no slice groups */
	if (videoBitsUe(bits))
		videoParserError(stream, &job->parser, 3, 0x8007);
	l0 = videoBitsUe(bits);
	if ((int32_t)l0 >= 16)
		videoParserError(stream, &job->parser, 3, 0x8007);
	l1 = videoBitsUe(bits);
	if ((int32_t)l1 >= 16)
		videoParserError(stream, &job->parser, 3, 0x8007);
	pps->numRefIdxL1Active = l1 + 1;
	pps->numRefIdxL0Active = l0 + 1;
	pps->weightedPred = videoBitsRead(bits, 1);
	value = videoBitsRead(bits, 2);
	if ((uint32_t)value >= 3)
		videoParserError(stream, &job->parser, 3, 0x8007);
	pps->weightedBipredIdc = value;
	value = videoBitsSe(bits);
	if (value < -26 || value >= 26)
		videoParserError(stream, &job->parser, 3, 0x8007);
	pps->picInitQp = value + 26;
	value = videoBitsSe(bits);
	if (value < -26 || value >= 26)
		videoParserError(stream, &job->parser, 3, 0x8007);
	value = videoBitsSe(bits);
	if (value < -12 || value >= 13)
		videoParserError(stream, &job->parser, 3, 0x8007);
	pps->chromaQpIndexOffset = value;
	pps->deblockingFilterControlPresent = videoBitsRead(bits, 1);
	pps->constrainedIntraPred = videoBitsRead(bits, 1);
	/* redundant_pic_cnt_present_flag */
	if (videoBitsRead(bits, 1))
		videoParserError(stream, &job->parser, 3, 0x8007);
	transform8x8 = 0;
	if (videoBitsMoreData(bits)) {
		/* the High profile's: not decoded */
		videoParserError(stream, &job->parser, 3, 0x2010);
		transform8x8 = videoBitsRead(bits, 1);
		if (transform8x8)
			videoParserError(stream, &job->parser, 3, 0x8007);
		/* pic_scaling_matrix_present_flag */
		if (videoBitsRead(bits, 1))
			videoParserError(stream, &job->parser, 3, 0x8007);
		value = videoBitsSe(bits);
		if (value < -12 || value >= 13)
			videoParserError(stream, &job->parser, 3, 0x8007);
	}
	pps->secondChromaQpIndexOffset = value;
	pps->transform8x8Mode = transform8x8;
	trailingBits(stream, job, 3, 0x8007);
	vnzPublicCloseProtected((uint32_t)sps, PARAMETER_SET_SIZE);
	vnzPublicClosePrivate(block, PARAMETER_SET2_SIZE);
}

/* access_unit_delimiter_rbsp(): I and P pictures only */
void avcdecParseAud(const VideoStream *stream, VideoSliceJob *job)
{
	if (videoBitsRead(job->parser.bits, 3) >= 3)
		videoParserError(stream, &job->parser, 0, 0x2002);
	trailingBits(stream, job, 0, 0x2002);
}

/* end_of_seq_rbsp(): nothing */
void avcdecParseEndOfSequence(const VideoStream *stream, VideoSliceJob *job)
{
}

/* end_of_stream_rbsp(): nothing */
void avcdecParseEndOfStream(const VideoStream *stream, VideoSliceJob *job)
{
}

/* filler_data_rbsp() */
void avcdecParseFiller(const VideoStream *stream, VideoSliceJob *job)
{
	VideoBitstream *bits = job->parser.bits;

	while (videoBitsPeek(bits, 8) == 0xff)
		videoBitsRead(bits, 8);
	trailingBits(stream, job, 0, 0x2004);
}

/* sei_rbsp(): into the picture's data */
void avcdecParseSei(const VideoStream *stream, VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	VideoBitstream *bits;
	VideoAvcSei *sei;

	sei = (VideoAvcSei *)vnzPublicOpenPrivate(stream->pictureData +
		nal->picture * PICTURE_DATA_SIZE, PICTURE_DATA_SIZE);
	/* a new picture: its messages start empty */
	if (!nal->pictureCount || nal->lastPictureCount != nal->pictureCount)
		sei->flags = 0;
	job->buffer = (uint32_t)sei;
	bits = job->parser.bits;
	do
		seiMessage(stream, job);
	while (videoBitsMoreData(bits));
	trailingBits(stream, job, 2, 0x2003);
	vnzCacheFlush((uint32_t)sei, PICTURE_DATA_SIZE);
	__asm__ __volatile__("syncm" ::: "memory");
	vnzL2Writeback(vnzPublicClosePrivate((uint32_t)sei, PICTURE_DATA_SIZE), PICTURE_DATA_SIZE);
}

/* sei_message(): its type and size, coded in bytes of 255 */
static void seiMessage(const VideoStream *stream, VideoSliceJob *job)
{
	VideoBitstream *bits = job->parser.bits;
	uint32_t type, size;

	type = 0;
	while (videoBitsPeek(bits, 8) == 0xff) {
		videoBitsRead(bits, 8);
		type += 255;
	}
	type += videoBitsRead(bits, 8);
	size = 0;
	while (videoBitsPeek(bits, 8) == 0xff) {
		videoBitsRead(bits, 8);
		size += 255;
	}
	size += videoBitsRead(bits, 8);
	seiPayload(stream, job, type, size);
}

/* rbsp_trailing_bits(): the stop bit, then zeros to the byte */
static void trailingBits(const VideoStream *stream, VideoSliceJob *job, uint32_t stage, uint32_t code)
{
	VideoBitstream *bits = job->parser.bits;

	if (videoBitsRead(bits, 1) != 1)
		videoParserError(stream, &job->parser, stage, code);
	while (!videoBitsAligned(bits))
		if (videoBitsRead(bits, 1))
			videoParserError(stream, &job->parser, stage, code);
}

/*
 * The profile: Baseline (or constrained to it) is decoded as frames, Main
 * (or constrained to it) needs the field tools; Extended and High are not
 * decoded.
 */
static void spsProfile(const VideoStream *stream, VideoSliceJob *job, VideoAvcSps *sps)
{
	VideoNal *nal = job->parser.nal;
	uint32_t main, unsupported;

	unsupported = 0;
	if (sps->profile == AVC_PROFILE_BASELINE || sps->constraintSet0) {
		main = 0;
	} else if (sps->profile == AVC_PROFILE_MAIN || sps->constraintSet1) {
		main = 1;
	} else {
		main = sps->profile != AVC_PROFILE_EXTENDED && !sps->constraintSet2;
		unsupported = 1;
	}
	if (stream->sequence)
		nal->fieldDecode |= main;
	else
		nal->fieldDecode = main;
	if (unsupported)
		videoParserError(stream, &job->parser, 1, 0x2010);
}

/*
 * The level: its MaxDpbMbs (Table A-1) in KiB of 4:2:0 frames, and the
 * frames that fit in it, at most 16. Levels above 3.1 are not decoded.
 */
static void spsLevel(const AvcdecParserInfo *info, const VideoStream *stream, VideoSliceJob *job,
	VideoAvcSps *sps)
{
	uint32_t level = sps->level, unsupported;
	int32_t limit, frame, frames;

	unsupported = 1;
	if (level > 50)
		limit = 69120;
	else if (level >= 50)
		limit = 41400;
	else if (level >= 42)
		limit = 13056;
	else if (level >= 40)
		limit = 12288;
	else if (level >= 32)
		limit = 7680;
	else {
		unsupported = 0;
		if (level >= 31)
			limit = 6750;
		else if (level >= 22)
			limit = 3038;
		else if (level >= 21)
			limit = 1782;
		else if (level >= 12)
			limit = 891;
		else if (level < 11)
			limit = 149;
		else
			limit = 338;
	}
	sps->maxDpbKbytes = limit;
	if (unsupported)
		videoParserError(stream, &job->parser, 1, 0x2011);
	frame = sps->width * sps->height;
	frame = (frame / 2 + frame) / 1024;
	if (frame < 1)
		frame = 1;
	frames = limit / frame;
	sps->maxDpbFrames = frames < 16 ? frames : 16;
}

/* vui_parameters(): into the stream's data */
static void spsVui(const VideoStream *stream, VideoSliceJob *job, VideoAvcSps *sps)
{
	VideoBitstream *bits = job->parser.bits;
	VideoNal *nal = job->parser.nal;
	uint32_t present, idc, sarWidth, sarHeight, flag, flag2, format, colour, primaries;
	uint32_t transfer, matrix, top, bottom, units, scale, fps, code, nalHrd, vclHrd;
	uint32_t overPic, bytesPerPic, bitsPerMb, mvHorizontal, mvVertical, reorder, buffering;
	VideoAvcVui *vui;

	vui = (VideoAvcVui *)vnzPublicOpenPrivate(sps->vui, sizeof(VideoAvcVui));
	present = videoBitsRead(bits, 1);
	idc = sarWidth = sarHeight = 0;
	if (present) {
		idc = videoBitsRead(bits, 8);
		/* Extended_SAR */
		if (idc == 255) {
			sarWidth = videoBitsRead(bits, 16);
			sarHeight = videoBitsRead(bits, 16);
		}
	}
	vui->aspectRatioIdc = idc;
	vui->sarWidth = sarWidth;
	vui->sarHeight = sarHeight;
	vui->aspectRatioInfoPresent = present;
	flag = videoBitsRead(bits, 1);
	flag2 = 0;
	if (flag)
		flag2 = videoBitsRead(bits, 1);
	vui->overscanInfoPresent = flag;
	vui->overscanAppropriate = flag2;

	present = videoBitsRead(bits, 1);
	format = 5;
	primaries = transfer = matrix = 2;
	flag = colour = 0;
	if (present) {
		format = videoBitsRead(bits, 3);
		flag = videoBitsRead(bits, 1);
		colour = videoBitsRead(bits, 1);
		if (colour) {
			primaries = videoBitsRead(bits, 8);
			transfer = videoBitsRead(bits, 8);
			matrix = videoBitsRead(bits, 8);
		}
	}
	vui->colourDescriptionPresent = colour;
	vui->videoFullRange = flag;
	vui->videoSignalTypePresent = present;
	vui->videoFormat = format;
	vui->colourPrimaries = primaries;
	vui->transferCharacteristics = transfer;
	vui->matrixCoefficients = matrix;

	present = videoBitsRead(bits, 1);
	top = bottom = 0;
	if (present) {
		top = videoBitsUe(bits);
		bottom = videoBitsUe(bits);
	}
	vui->chromaLocInfoPresent = present;
	vui->chromaSampleLocTypeTopField = top;
	vui->chromaSampleLocTypeBottomField = bottom;

	/* the frame duration, in 180 kHz ticks, and its rate's code */
	vui->timingInfoPresent = videoBitsRead(bits, 1);
	if (vui->timingInfoPresent) {
		units = videoBitsRead(bits, 32);
		scale = videoBitsRead(bits, 32);
		vui->numUnitsInTick = units;
		vui->timeScale = scale;
		vui->fixedFrameRate = videoBitsRead(bits, 1);
		if (vui->fixedFrameRate) {
			if (units % 1001 == 0) {
				scale /= units / 1001;
				units = 1001;
			}
			sps->frameRateCode = AVC_FRAME_RATE_UNKNOWN;
			if (units == 1001) {
				nal->frameDuration = (uint64_t)180180000 / (scale >> 1);
				if (scale == 48000)
					code = 1;		/* 23.976 */
				else if (scale == 60000)
					code = 4;		/* 29.97 */
				else if (scale == 120000)
					code = 7;		/* 59.94 */
				else
					goto rated;
			} else {
				fps = scale / units >> 1;
				nal->frameDuration = (uint64_t)units * 360000 / scale;
				if (fps == 24)
					code = 2;
				else if (fps == 25)
					code = 3;
				else if (fps == 30)
					code = 5;
				else if (fps == 50)
					code = 6;
				else if (fps == 60)
					code = 8;
				else
					goto rated;
			}
			sps->frameRateCode = code;
		} else {
			sps->frameRateCode = 0;
			nal->frameDuration = (uint64_t)units * 180000 / (scale >> 1);
		}
	}
rated:
	nalHrd = videoBitsRead(bits, 1);
	vui->nalHrdPresent = nalHrd;
	if (nalHrd)
		vuiHrd(job, vui, &vui->nalHrd);
	vclHrd = videoBitsRead(bits, 1);
	vui->vclHrdPresent = vclHrd;
	if (vclHrd)
		vuiHrd(job, vui, &vui->vclHrd);
	if (nalHrd || vclHrd) {
		flag = videoBitsRead(bits, 1);
	} else {
		flag = 0;
		vui->timeOffsetLength = 24;
		vui->dpbOutputDelayLength = 23;
		vui->cpbRemovalDelayLength = 23;
		vui->initialCpbRemovalDelayLength = 23;
	}
	vui->lowDelayHrd = flag;
	vui->picStructPresent = videoBitsRead(bits, 1);

	present = videoBitsRead(bits, 1);
	if (present) {
		overPic = videoBitsRead(bits, 1);
		bytesPerPic = videoBitsUe(bits);
		bitsPerMb = videoBitsUe(bits);
		mvHorizontal = videoBitsUe(bits);
		mvVertical = videoBitsUe(bits);
		reorder = videoBitsUe(bits);
		buffering = videoBitsUe(bits);
		sps->maxDpbFrames = buffering;
	} else {
		bytesPerPic = 2;
		overPic = bitsPerMb = 1;
		mvHorizontal = mvVertical = 16;
		buffering = sps->maxDpbFrames;
		reorder = buffering;
	}
	vui->log2MaxMvLengthHorizontal = mvHorizontal;
	vui->motionVectorsOverPicBoundaries = overPic;
	vui->bitstreamRestriction = present;
	vui->maxBytesPerPicDenom = bytesPerPic;
	vui->maxBitsPerMbDenom = bitsPerMb;
	vui->log2MaxMvLengthVertical = mvVertical;
	vui->numReorderFrames = reorder;
	vui->maxDecFrameBuffering = buffering;
	sps->nalHrdPresent = nalHrd;
	sps->vclHrdPresent = vclHrd;
	sps->numReorderFrames = reorder;
	vnzCacheFlush((uint32_t)vui, sizeof(VideoAvcVui));
	__asm__ __volatile__("syncm" ::: "memory");
	vnzPublicClosePrivate((uint32_t)vui, sizeof(VideoAvcVui));
}

/* hrd_parameters(): the last CPB's values, and the delays' lengths */
static void vuiHrd(VideoSliceJob *job, VideoAvcVui *vui, VideoAvcHrd *hrd)
{
	VideoBitstream *bits = job->parser.bits;
	uint32_t rate, size, cbr, scale, sizeScale;
	int32_t count, i;

	count = videoBitsUe(bits);
	scale = videoBitsRead(bits, 4);
	sizeScale = videoBitsRead(bits, 4);
	hrd->cpbSizeScale = sizeScale;
	hrd->cpbCnt = count;
	hrd->bitRateScale = scale;
	rate = size = cbr = 0;
	for (i = 0; i <= count; i++) {
		rate = videoBitsUe(bits);
		size = videoBitsUe(bits);
		cbr = videoBitsRead(bits, 1);
	}
	hrd->cbrFlag = cbr;
	hrd->bitRateValue = rate;
	hrd->cpbSizeValue = size;
	vui->initialCpbRemovalDelayLength = videoBitsRead(bits, 5);
	vui->cpbRemovalDelayLength = videoBitsRead(bits, 5);
	vui->dpbOutputDelayLength = videoBitsRead(bits, 5);
	vui->timeOffsetLength = videoBitsRead(bits, 5);
}

/* sei_payload(): the messages kept, the others skipped */
static void seiPayload(const VideoStream *stream, VideoSliceJob *job, uint32_t type, int32_t size)
{
	VideoBitstream *bits = job->parser.bits;
	int32_t i;

	if (type == 0)
		seiBufferingPeriod(stream, job);
	else if (type == 1)
		seiPicTiming(stream, job);
	else if (type == 2)
		seiPanScan(stream, job);
	else if (type == 6)
		seiRecoveryPoint(stream, job);
	else
		for (i = 0; i < size; i++)
			videoBitsRead(bits, 8);
	/* the payload's bit_equal_to_one and zeros to the byte */
	if (videoBitsAligned(bits))
		return;
	if (videoBitsRead(bits, 1) != 1)
		videoParserError(stream, &job->parser, 2, 0x2003);
	while (!videoBitsAligned(bits))
		if (videoBitsRead(bits, 1))
			videoParserError(stream, &job->parser, 2, 0x2003);
}

/* buffering_period(): its sequence's HRDs' initial delays */
static void seiBufferingPeriod(const VideoStream *stream, VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	VideoBitstream *bits = job->parser.bits;
	VideoAvcSei *sei = (VideoAvcSei *)job->buffer;
	VideoParameterCache *cache = (VideoParameterCache *)stream->parameterCache;
	uint32_t id, address, length, delay, offset;
	int32_t index, count, i;
	const VideoAvcSps *sps;
	const VideoAvcVui *vui;

	id = videoBitsUe(bits);
	if (id >= AVC_MAX_SPS)
		videoParserError(stream, &job->parser, 2, 0x2003);
	sei->seqParameterSetId = id;
	index = videoCacheFind(cache, id);
	if (index < 0) {
		videoParserError(stream, &job->parser, 2, 0x2003);
		return;
	}
	nal->spsId = id;
	address = stream->parameterSets + index * PARAMETER_SET_SIZE;
	if (((const VideoAvcSps *)address)->id != id)
		videoParserError(stream, &job->parser, 2, 0x2003);
	if (((const VideoAvcSps *)address)->id != id)
		return;
	if (!((const VideoAvcSps *)address)->vui)
		videoParserError(stream, &job->parser, 2, 0x2003);
	if (!((const VideoAvcSps *)address)->vui)
		return;
	sps = (const VideoAvcSps *)vnzPublicOpenProtected(address, PARAMETER_SET_SIZE);
	vui = (const VideoAvcVui *)vnzPublicOpenProtected(sps->vui, sizeof(VideoAvcVui));
	delay = offset = 0;
	if (sps->nalHrdPresent) {
		length = vui->initialCpbRemovalDelayLength + 1;
		count = vui->nalHrd.cpbCnt + 1;
		for (i = 0; i < count; i++) {
			delay = videoBitsRead(bits, length);
			offset = videoBitsRead(bits, length);
		}
		sei->nalInitialCpbRemovalDelayOffset = offset;
		sei->nalInitialCpbRemovalDelay = delay;
	}
	if (sps->vclHrdPresent) {
		length = vui->initialCpbRemovalDelayLength + 1;
		count = vui->vclHrd.cpbCnt + 1;
		for (i = 0; i < count; i++) {
			delay = videoBitsRead(bits, length);
			offset = videoBitsRead(bits, length);
		}
		sei->vclInitialCpbRemovalDelayOffset = offset;
		sei->vclInitialCpbRemovalDelay = delay;
	}
	sei->flags |= AVC_SEI_BUFFERING_PERIOD;
	vnzPublicCloseProtected((uint32_t)vui, sizeof(VideoAvcVui));
	vnzPublicCloseProtected((uint32_t)sps, PARAMETER_SET_SIZE);
}

/* pic_timing(): the delays and the clock timestamps */
static void seiPicTiming(const VideoStream *stream, VideoSliceJob *job)
{
	VideoBitstream *bits = job->parser.bits;
	VideoNal *nal = job->parser.nal;
	VideoAvcSei *sei = (VideoAvcSei *)job->buffer;
	uint32_t id = nal->spsId, address, cpb, dpb, structure, flag, ctType, nuit, counting;
	uint32_t full, discontinuity, dropped, frames, seconds, minutes, hours;
	uint32_t secondsFlag, minutesFlag, hoursFlag;
	int32_t index, offset, i;
	const VideoAvcSps *sps;
	const VideoAvcVui *vui;
	VideoAvcClockTimestamp *clock;

	index = videoCacheFind((VideoParameterCache *)stream->parameterCache, id);
	if (index < 0) {
		videoParserError(stream, &job->parser, 2, 0x2003);
		return;
	}
	address = stream->parameterSets + index * PARAMETER_SET_SIZE;
	if (((const VideoAvcSps *)address)->id != id)
		videoParserError(stream, &job->parser, 2, 0x2003);
	if (((const VideoAvcSps *)address)->id != id)
		return;
	if (!((const VideoAvcSps *)address)->vui)
		videoParserError(stream, &job->parser, 2, 0x2003);
	if (!((const VideoAvcSps *)address)->vui)
		return;
	sps = (const VideoAvcSps *)vnzPublicOpenProtected(address, PARAMETER_SET_SIZE);
	vui = (const VideoAvcVui *)vnzPublicOpenProtected(sps->vui, sizeof(VideoAvcVui));
	if (sps->nalHrdPresent || sps->vclHrdPresent) {
		cpb = videoBitsRead(bits, vui->cpbRemovalDelayLength + 1);
		dpb = videoBitsRead(bits, vui->dpbOutputDelayLength + 1);
		if (!nal->pictureCount) {
			/* the stream's first picture: no removal delay yet */
			sei->dpbOutputDelay = dpb;
			sei->cpbRemovalDelay = 0;
			nal->idrPending = 1;
		} else {
			sei->dpbOutputDelay = dpb;
			sei->cpbRemovalDelay = cpb;
		}
	}
	if (vui->picStructPresent) {
		structure = videoBitsRead(bits, 4);
		if (structure >= 9) {
			videoParserError(stream, &job->parser, 2, 0x2003);
			goto close;
		}
		ctType = nuit = counting = full = discontinuity = dropped = frames = 0;
		seconds = minutes = hours = secondsFlag = minutesFlag = hoursFlag = 0;
		offset = 0;
		sei->picStruct = structure;
		for (i = 0; i < numClockTs[structure]; i++) {
			flag = videoBitsRead(bits, 1);
			sei->clockTimestampFlag[i] = flag;
			if (!flag)
				continue;
			ctType = videoBitsRead(bits, 2);
			nuit = videoBitsRead(bits, 1);
			counting = videoBitsRead(bits, 5);
			full = videoBitsRead(bits, 1);
			discontinuity = videoBitsRead(bits, 1);
			dropped = videoBitsRead(bits, 1);
			frames = videoBitsRead(bits, 8);
			if (full) {
				seconds = videoBitsRead(bits, 6);
				minutes = videoBitsRead(bits, 6);
				hours = videoBitsRead(bits, 5);
			} else {
				secondsFlag = videoBitsRead(bits, 1);
				if (secondsFlag) {
					seconds = videoBitsRead(bits, 6);
					minutesFlag = videoBitsRead(bits, 1);
					if (minutesFlag) {
						minutes = videoBitsRead(bits, 6);
						hoursFlag = videoBitsRead(bits, 1);
						if (hoursFlag)
							hours = videoBitsRead(bits, 5);
					}
				}
			}
			if (vui->timeOffsetLength >= 1)
				offset = videoBitsReadSigned(bits, vui->timeOffsetLength);
			clock = &sei->clockTimestamp[i];
			clock->ctType = ctType;
			clock->minutesValue = minutes;
			clock->hoursFlag = hoursFlag;
			clock->minutesFlag = minutesFlag;
			clock->nuitFieldBased = nuit;
			clock->countingType = counting;
			clock->discontinuity = discontinuity;
			clock->fullTimestamp = full;
			clock->timeOffset = offset;
			clock->cntDropped = dropped;
			clock->nFrames = frames;
			clock->secondsFlag = secondsFlag;
			clock->hoursValue = hours;
			clock->secondsValue = seconds;
		}
	} else {
		sei->picStruct = 0;
	}
	sei->flags |= AVC_SEI_PIC_TIMING;
close:
	vnzPublicCloseProtected((uint32_t)vui, sizeof(VideoAvcVui));
	vnzPublicCloseProtected((uint32_t)sps, PARAMETER_SET_SIZE);
}

/* pan_scan_rect() */
static void seiPanScan(const VideoStream *stream, VideoSliceJob *job)
{
	VideoBitstream *bits = job->parser.bits;
	VideoAvcSei *sei = (VideoAvcSei *)job->buffer;
	VideoNal *nal = job->parser.nal;
	int32_t count, i, left, right, top, bottom;
	uint32_t id;

	id = videoBitsUe(bits);
	sei->panScanRectId = id;
	sei->panScanRectCancel = videoBitsRead(bits, 1);
	if (!sei->panScanRectCancel) {
		count = videoBitsUe(bits);
		if (count < 0 || count >= 3)
			videoParserError(stream, &job->parser, 2, 0x2003);
		if (nal->error)
			return;
		for (i = 0; i <= count; i++) {
			left = videoBitsSe(bits);
			right = videoBitsSe(bits);
			top = videoBitsSe(bits);
			bottom = videoBitsSe(bits);
			sei->panScanRectTopOffset[i] = top;
			sei->panScanRectBottomOffset[i] = bottom;
			sei->panScanRectLeftOffset[i] = left;
			sei->panScanRectRightOffset[i] = right;
		}
		sei->panScanRectRepetitionPeriod = videoBitsUe(bits);
	}
	sei->flags |= AVC_SEI_PAN_SCAN;
}

/* recovery_point() */
static void seiRecoveryPoint(const VideoStream *stream, VideoSliceJob *job)
{
	VideoBitstream *bits = job->parser.bits;
	VideoAvcSei *sei = (VideoAvcSei *)job->buffer;
	uint32_t frames, exact, broken;

	frames = videoBitsUe(bits);
	exact = videoBitsRead(bits, 1);
	broken = videoBitsRead(bits, 1);
	sei->changingSliceGroupIdc = videoBitsRead(bits, 2);
	sei->brokenLink = broken;
	sei->exactMatch = exact;
	sei->recoveryFrameCnt = frames;
	sei->flags |= AVC_SEI_RECOVERY_POINT;
}

static void refPicListReordering(const VideoStream *stream, VideoSliceJob *job, uint32_t header);
static void decRefPicMarking(const VideoStream *stream, VideoSliceJob *job, uint32_t firstMb);
static void predWeightTable(const VideoStream *stream, VideoSliceJob *job);

/* slice_header(): into the unit's word and the slice's fields */
void avcdecSliceHeader(const VideoStream *stream, VideoSliceJob *job)
{
	static const int32_t noDelta[2] = { 0, 0 };
	VideoParameterCache *cache = (VideoParameterCache *)stream->parameterCache;
	VideoBitstream *bits = job->parser.bits;
	VideoNal *nal = job->parser.nal;
	uint32_t firstMb, type, id, header, frameNum, lsb, bottomField, alwaysZero, pocType;
	uint32_t block, idc, l0, l1, width, height, flags;
	int32_t index, spsIndex, qp, alpha, beta, delta[2];
	const VideoAvcSps *sps;
	const VideoAvcPps *pps;
	const VideoAvcSei *sei;

	delta[0] = noDelta[0];
	delta[1] = noDelta[1];
	firstMb = videoBitsUe(bits);
	if (nal->startPending ? firstMb != 0 : !((int32_t)firstMb < nal->macroblocks))
		videoParserError(stream, &job->parser, 0, 0x8008);
	type = videoBitsUe(bits);
	if (type >= 10)
		videoParserError(stream, &job->parser, 0, 0x8008);
	/* B slices need the Main profile's tools */
	if (!job->fieldDecode && (type == 1 || type == 6))
		videoParserError(stream, &job->parser, 0, 0x4016);
	id = videoBitsUe(bits);
	if (id >= 256)
		videoParserError(stream, &job->parser, 0, 0x8008);
	index = videoCacheFind2(cache, id);
	if (index < 0)
		videoParserError(stream, &job->parser, 0, 0x4018);
	block = vnzPublicOpenProtected((stream->parameterSets2 + index * PARAMETER_SET2_SIZE) & ~63,
		PARAMETER_SET2_SIZE);
	job->parameterSet2 = block;
	pps = (const VideoAvcPps *)(block + (index & 3) * PARAMETER_SET2_SIZE);
	job->pps = (uint32_t)pps;
	if (pps->id != id)
		videoParserError(stream, &job->parser, 4, 0x8008);
	spsIndex = videoCacheFind(cache, pps->spsId);
	if (spsIndex < 0)
		videoParserError(stream, &job->parser, 4, 0x4017);
	sps = (const VideoAvcSps *)vnzPublicOpenProtected(stream->parameterSets +
		spsIndex * PARAMETER_SET_SIZE, PARAMETER_SET_SIZE);
	job->parameterSet = (uint32_t)sps;
	if (sps->id != pps->spsId)
		videoParserError(stream, &job->parser, 5, 0x8008);
	nal->parameterSet = spsIndex;
	nal->parameterSet2 = index;

	header = sps->frameMbsOnly << 24 | sps->direct8x8Inference << 27 |
		pps->entropyCodingMode << 21 | pps->weightedPred << 18 |
		pps->weightedBipredIdc << AVC_SLICE_BIPRED_SHIFT |
		pps->constrainedIntraPred << 19 | pps->transform8x8Mode << 20;
	if (type >= 5) {
		header |= AVC_SLICE_TYPE_ALL;
		type -= 5;
	}
	header |= type | (nal->refIdc > 0) << 11;
	bottomField = pps->bottomFieldPicOrderInFramePresent;
	alwaysZero = sps->deltaPicOrderAlwaysZero;
	lsb = sps->log2MaxPicOrderCntLsb;
	pocType = sps->picOrderCntType;
	frameNum = videoBitsRead(bits, sps->log2MaxFrameNum);
	if (nal->type == NAL_IDR && videoBitsUe(bits) >= 0x10000)
		videoParserError(stream, &job->parser, 5, 0x8008);
	if (pocType == 0) {
		lsb = videoBitsRead(bits, lsb);
		if (bottomField)
			delta[0] = videoBitsSe(bits);
	} else {
		lsb = 0;
		if (pocType == 1 && !alwaysZero) {
			delta[0] = videoBitsSe(bits);
			if (bottomField)
				delta[1] = videoBitsSe(bits);
		}
	}
	nal->picOrderCntLsb = lsb;
	nal->deltaPicOrderCnt[0] = delta[0];
	nal->deltaPicOrderCnt[1] = delta[1];
	if ((header & AVC_SLICE_TYPE) == AVC_SLICE_B)
		header |= videoBitsRead(bits, 1) << 5;

	/* num_ref_idx_active_override_flag */
	type = header & AVC_SLICE_TYPE;
	l1 = 0;
	if (type == AVC_SLICE_P || type == AVC_SLICE_B) {
		if (videoBitsRead(bits, 1)) {
			l0 = videoBitsUe(bits);
			if (l0 >= 16)
				videoParserError(stream, &job->parser, 5, 0x8008);
			l0++;
			if (type == AVC_SLICE_B) {
				l1 = videoBitsUe(bits);
				if (l1 >= 16)
					videoParserError(stream, &job->parser, 5, 0x8008);
				l1++;
			}
		} else {
			l0 = pps->numRefIdxL0Active;
			if (type == AVC_SLICE_B)
				l1 = pps->numRefIdxL1Active;
		}
	} else {
		l0 = 0;
	}
	nal->numRefIdxActive[0] = l0;
	nal->numRefIdxActive[1] = l1;
	refPicListReordering(stream, job, header);
	if (((header & AVC_SLICE_WEIGHTED) && type == AVC_SLICE_P) ||
	    (((int32_t)header >> AVC_SLICE_BIPRED_SHIFT & 3) == 1 && type == AVC_SLICE_B))
		predWeightTable(stream, job);
	nal->mmco5 = 0;
	if (nal->refIdc)
		decRefPicMarking(stream, job, firstMb);
	if ((header & AVC_SLICE_CABAC) && type != AVC_SLICE_I) {
		idc = videoBitsUe(bits);
		if (idc >= 3)
			videoParserError(stream, &job->parser, 6, 0x8008);
		nal->cabacInitIdc = idc;
	}
	delta[0] = videoBitsSe(bits);
	qp = pps->picInitQp + delta[0];
	if (qp < 0 || qp >= 52)
		videoParserError(stream, &job->parser, 6, 0x8008);
	nal->sliceQpDelta = delta[0];
	nal->sliceQp = pps->picInitQp + delta[0];
	idc = 0;
	alpha = beta = 0;
	if (pps->deblockingFilterControlPresent) {
		idc = videoBitsUe(bits);
		if (idc >= 3)
			videoParserError(stream, &job->parser, 6, 0x8008);
		if (idc != 1) {
			alpha = videoBitsSe(bits);
			if (alpha < -6 || alpha >= 7)
				videoParserError(stream, &job->parser, 6, 0x8008);
			beta = videoBitsSe(bits);
			if (beta < -6 || beta >= 7)
				videoParserError(stream, &job->parser, 6, 0x8008);
		}
	}
	nal->sliceDone = 0;
	nal->sliceBetaOffset = beta;
	nal->sliceAlphaC0Offset = alpha;
	nal->sliceHeader = header | idc << AVC_SLICE_DEBLOCKING_SHIFT;
	nal->firstMacroblock = firstMb;
	nal->sliceParsed = 1;

	/* the picture's first slice: the sequence's sizes */
	if (nal->startPending) {
		width = sps->widthInMbs;
		height = sps->heightInMbs;
		nal->chromaModeRowSize = width;
		nal->widthInMacroblocks = width;
		nal->rowsPerRun = sps->rowsPerRun;
		nal->heightInMacroblocks = height;
		nal->macroblocks = width * height;
		nal->mbRowSize = width * 16;
		nal->intraRowSize = width * 16;
		nal->refIdxRowSize = width * 8;
		nal->mvdRowSize = width * 128;
		nal->coefRowSize = ALIGN64(width * 544);
		nal->chromaCoefRowSize = ALIGN64(width * 136);
		nal->pcmRowSize = width * 256;
		nal->pcmChromaRowSize = width * 64;
		nal->numRefFrames = sps->numRefFrames;
		nal->maxFrameNum = sps->maxFrameNum;
		nal->gapsInFrameNumAllowed = sps->gapsInFrameNumAllowed;
		nal->frameNum = frameNum;
		nal->idr = nal->type == NAL_IDR;
		nal->frameCropping = sps->frameCropping;
		if (nal->seiFound) {
			sei = (const VideoAvcSei *)vnzPublicOpenProtected(stream->pictureData +
				nal->picture * PICTURE_DATA_SIZE, PICTURE_DATA_SIZE);
			flags = sei->flags;
			nal->picStruct = (flags & AVC_SEI_PIC_TIMING) ? (int8_t)sei->picStruct : 0;
			/* a recovery point starts the stream as an IDR does */
			if (sei->flags & AVC_SEI_RECOVERY_POINT)
				nal->idr = 1;
			vnzPublicCloseProtected((uint32_t)sei, PICTURE_DATA_SIZE);
		} else {
			nal->picStruct = 0;
		}
	}
	nal->state.bitOffset = videoBitsSeek(stream, nal);
	videoBitsSave(&nal->state);
	vnzPublicCloseProtected((uint32_t)sps, PARAMETER_SET_SIZE);
	vnzPublicCloseProtected(block, PARAMETER_SET2_SIZE);
}

/* the next unit: another slice of the picture, or where the picture ends */
void avcdecSliceStart(const VideoStream *stream, VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	VideoBitstream bits;
	uint32_t words[2], start;
	int32_t size, type, first;

	size = nal->size;
	start = nal->start;
	bits = nal->bits;
	words[0] = nal->reserved[0];
	words[1] = nal->reserved[1];
	type = videoNalNext(stream, &job->parser, 0);
	if (type == NAL_SLICE || type == NAL_IDR) {
		videoBitsBegin(stream, &job->parser);
		first = videoBitsUe(&nal->bits);
		if (!(nal->firstMacroblock < first && first < nal->macroblocks))
			videoParserError(stream, &job->parser, 0, 0x8008);
		nal->nextFirstMacroblock = first;
	} else {
		nal->nextFirstMacroblock = nal->macroblocks;
	}
	nal->nextType = type;
	nal->bits = bits;
	nal->reserved[0] = words[0];
	nal->reserved[1] = words[1];
	nal->start = start;
	nal->size = size;
}

/* ref_pic_list_reordering(): each list's operations, up to 3 */
static void refPicListReordering(const VideoStream *stream, VideoSliceJob *job, uint32_t header)
{
	VideoBitstream *bits = job->parser.bits;
	VideoNal *nal = job->parser.nal;
	uint32_t type = header & AVC_SLICE_TYPE, idc, list;
	int32_t count, i;

	for (list = 0; list < 2; list++) {
		if (list == 0 ? type == AVC_SLICE_I : type != AVC_SLICE_B) {
			nal->refPicListReordering[list] = 0;
			continue;
		}
		nal->refPicListReordering[list] = videoBitsRead(bits, 1);
		if (!nal->refPicListReordering[list])
			continue;
		count = nal->numRefIdxActive[list];
		idc = videoBitsUe(bits);
		if (idc >= 4)
			videoParserError(stream, &job->parser, 6, 0x8008);
		for (i = 0; idc != 3; i++) {
			if (!(i < count))
				videoParserError(stream, &job->parser, 6, 0x8008);
			nal->reorderingIdc[list][i] = idc;
			nal->reorderingValue[list][i] = videoBitsUe(bits);
			idc = videoBitsUe(bits);
		}
		nal->reorderingIdc[list][i] = idc;
	}
}

/* dec_ref_pic_marking() */
static void decRefPicMarking(const VideoStream *stream, VideoSliceJob *job, uint32_t firstMb)
{
	VideoBitstream *bits = job->parser.bits;
	VideoNal *nal = job->parser.nal;
	uint8_t *operation = nal->mmcoOperation;
	uint16_t *argument = nal->mmcoArgument;
	uint32_t op, flag;

	if (nal->type == NAL_IDR) {
		/* no_output_of_prior_pics_flag, long_term_reference_flag */
		videoBitsRead(bits, 1);
		flag = videoBitsRead(bits, 1);
		if (!firstMb)
			nal->longTermReference = flag;
		return;
	}
	nal->adaptiveRefPicMarking = videoBitsRead(bits, 1);
	if (!nal->adaptiveRefPicMarking)
		return;
	do {
		op = videoBitsUe(bits);
		if (op >= 7)
			videoParserError(stream, &job->parser, 6, 0x8008);
		*operation++ = op;
		switch (op) {
		case 3:		/* difference_of_pic_nums_minus1, long_term_frame_idx */
			*argument++ = videoBitsUe(bits);
			/* fall through */
		case 1:		/* difference_of_pic_nums_minus1 */
		case 2:		/* long_term_pic_num */
		case 4:		/* max_long_term_frame_idx_plus1 */
		case 6:		/* long_term_frame_idx */
			*argument++ = videoBitsUe(bits);
			break;
		case 5:
			nal->mmco5 = 1;
			break;
		}
	} while (op);
}

/* pred_weight_table(): unsent weights are the denominators' */
static void predWeightTable(const VideoStream *stream, VideoSliceJob *job)
{
	VideoBitstream *bits = job->parser.bits;
	VideoNal *nal = job->parser.nal;
	int32_t count[2], i, list, lumaWeight, lumaOffset, weight[2], offset[2];
	uint32_t lumaDenom, chromaDenom;
	VideoAvcWeights *weights;

	count[1] = nal->numRefIdxActive[1];
	count[0] = nal->numRefIdxActive[0];
	lumaDenom = videoBitsUe(bits);
	if (lumaDenom >= 8)
		videoParserError(stream, &job->parser, 5, 0x8008);
	chromaDenom = videoBitsUe(bits);
	if (chromaDenom >= 8)
		videoParserError(stream, &job->parser, 5, 0x8008);
	nal->lumaLog2WeightDenom = lumaDenom;
	nal->chromaLog2WeightDenom = chromaDenom;
	for (list = 0; list < 2; list++) {
		weights = &nal->weights[list];
		for (i = 0; i < count[list]; i++) {
			if (videoBitsRead(bits, 1)) {
				lumaWeight = videoBitsSe(bits);
				lumaOffset = videoBitsSe(bits);
			} else {
				lumaOffset = 0;
				lumaWeight = 1 << lumaDenom;
			}
			if (videoBitsRead(bits, 1)) {
				weight[0] = videoBitsSe(bits);
				offset[0] = videoBitsSe(bits);
				weight[1] = videoBitsSe(bits);
				offset[1] = videoBitsSe(bits);
			} else {
				offset[0] = offset[1] = 0;
				weight[0] = weight[1] = 1 << chromaDenom;
			}
			weights->lumaWeight[i] = lumaWeight;
			weights->lumaOffset[i] = lumaOffset;
			weights->chromaWeight[0][i] = weight[0];
			weights->chromaOffset[0][i] = offset[0];
			weights->chromaWeight[1][i] = weight[1];
			weights->chromaOffset[1][i] = offset[1];
		}
	}
}

/* a slice for the unit: the picture's command on its first, the slice's own */
void avcdecSliceLoad(const VideoStream *stream, VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	const VideoAvcSps *sps;
	const VideoAvcPps *pps;
	VideoAvcDecodeCommand *command;
	VideoHelperSlice *slice;
	const VideoAvcWeights *from;
	VideoAvcSliceWeight *to;
	uint32_t block, header, type;
	int32_t l0, l1, list, count, i;

	block = vnzPublicOpenProtected((stream->parameterSets2 +
		nal->parameterSet2 * PARAMETER_SET2_SIZE) & ~63, PARAMETER_SET2_SIZE);
	sps = (const VideoAvcSps *)job->parameterSet;
	pps = (const VideoAvcPps *)(block + (nal->parameterSet2 & 3) * PARAMETER_SET2_SIZE);
	if (!nal->firstMacroblock) {
		command = (VideoAvcDecodeCommand *)vnzPublicOpenPrivate(nal->command,
			sizeof(VideoAvcDecodeCommand));
		command->widthInMacroblocks = sps->widthInMbs;
		command->heightInMacroblocks = sps->heightInMbs;
		command->mode = sps->widthInMbs < 40 ? 6 : 8;
		command->mbRowSize = nal->mbRowSize;
		command->intraRowSize = nal->intraRowSize;
		command->chromaModeRowSize = nal->chromaModeRowSize;
		command->refIdxRowSize = nal->refIdxRowSize;
		command->mvdRowSize = nal->mvdRowSize;
		command->coefRowSize = nal->coefRowSize;
		command->chromaCoefRowSize = nal->chromaCoefRowSize;
		command->pcmRowSize = nal->pcmRowSize;
		command->pcmChromaRowSize = nal->pcmChromaRowSize;
		command->type = nal->type;
		command->refIdc = nal->refIdc;
		vnzPublicClosePrivate((uint32_t)command, sizeof(VideoAvcDecodeCommand));
	}
	slice = (VideoHelperSlice *)vnzPublicOpenPrivate(nal->slice, VIDEO_HELPER_SLICE_SIZE);
	slice->context = nal->context;
	slice->command = nal->command;
	header = nal->sliceHeader;
	slice->sliceHeader = header;
	slice->qp = pps->picInitQp + nal->sliceQpDelta;
	slice->chromaQpIndexOffset = pps->chromaQpIndexOffset;
	slice->alphaOffset = nal->sliceAlphaC0Offset << 1;
	slice->betaOffset = nal->sliceBetaOffset << 1;
	type = header & AVC_SLICE_TYPE;
	l0 = nal->numRefIdxActive[0];
	l1 = nal->numRefIdxActive[1];
	slice->numRefIdxActive[0] = l0;
	slice->numRefIdxActive[1] = l1;
	if (((header & AVC_SLICE_WEIGHTED) && type == AVC_SLICE_P) ||
	    (((int32_t)header >> AVC_SLICE_BIPRED_SHIFT & 3) == 1 && type == AVC_SLICE_B)) {
		slice->lumaLog2WeightDenom = nal->lumaLog2WeightDenom;
		slice->chromaLog2WeightDenom = nal->chromaLog2WeightDenom;
		for (list = 0; list < 2; list++) {
			count = list ? l1 : l0;
			from = &nal->weights[list];
			to = slice->weights[list];
			for (i = 0; i < count; i++, to++) {
				to->lumaWeight = (int16_t)from->lumaWeight[i];
				to->lumaOffset = from->lumaOffset[i];
				to->chromaWeight = (int16_t)from->chromaWeight[0][i];
				to->chromaOffset = from->chromaOffset[0][i];
				to->chromaWeight2 = (int16_t)from->chromaWeight[1][i];
				to->chromaOffset2 = from->chromaOffset[1][i];
			}
		}
	}
	vnzPublicClosePrivate((uint32_t)slice, VIDEO_HELPER_SLICE_SIZE);
	vnzPublicCloseProtected(block, PARAMETER_SET2_SIZE);
}

/*
 * A run of the slice: up to the row's end, or whole rows from a row's start,
 * never past the next slice. The unit is set up for the picture on its
 * first run (or after the slices ended early), then for the run's buffers.
 */
void avcdecSlicePrepare(const VideoHelperShared *shared, const VideoStream *stream,
	VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	int32_t width = nal->widthInMacroblocks, first = nal->firstMacroblock;
	int32_t row = first / width, column = first % width, count, rows, l0, l1;
	uint32_t header, picture, picture2, slice, i;
	VideoHelperJob *run;

	if (column == 0) {
		rows = nal->rowsPerRun;
		if (nal->heightInMacroblocks < rows + row)
			rows = nal->heightInMacroblocks - row;
		count = rows * width;
	} else {
		count = width - column;
	}
	if (nal->nextFirstMacroblock < first + count)
		count = nal->nextFirstMacroblock - first;
	nal->runRow = row;
	nal->runMacroblocks = count;
	header = nal->sliceHeader;
	nal->runColumn = column;
	run = (VideoHelperJob *)vnzPublicOpenPrivate(shared->request, sizeof(VideoHelperJob));
	if (header & AVC_SLICE_CABAC) {
		avcdecCabacStart(shared, job, run);
		if ((header & AVC_SLICE_TYPE) == AVC_SLICE_I) {
			count = avcdecCabacSliceI(job, run);
		} else {
			job->numRefIdxActive[0] = nal->numRefIdxActive[0];
			if ((header & AVC_SLICE_TYPE) == AVC_SLICE_B) {
				job->numRefIdxActive[1] = nal->numRefIdxActive[1];
				count = avcdecCabacSliceB(job, run);
			} else {
				count = avcdecCabacSliceP(job, run);
			}
		}
		nal->doneMacroblocks = count;
		avcdecCabacFinish(shared, job, run);
		goto close;
	}
	if (first == 0 || nal->state.ended) {
		VIDEO_UNIT[UNIT_RESET] = 1;
		VIDEO_UNIT[UNIT_VERSION] = shared->version;
		VIDEO_UNIT[UNIT_MODE] = UNIT_MODE_DECODE;
		VIDEO_UNIT[UNIT_ENABLE] = 1;
		picture = (header & AVC_SLICE_DIRECT_8X8) ? UNIT_PICTURE_DIRECT_8X8 : 0;
		picture = (picture & ~0x7f00) | ((width - 1) & 0x7f) << 8;
		picture = (picture & ~UNIT_PICTURE_FRAME_MBS_ONLY) |
			((int32_t)header >> 24 & 1) << 24;
		VIDEO_UNIT[UNIT_PICTURE] = picture;
		picture2 = (header & AVC_SLICE_TRANSFORM_8X8) ? UNIT_PICTURE2_TRANSFORM_8X8 : 0;
		VIDEO_UNIT[UNIT_PICTURE2] = picture2;
		VIDEO_UNIT[UNIT_PARAMETER(0)] = nal->mbRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(1)] = nal->intraRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(2)] = nal->chromaModeRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(3)] = nal->refIdxRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(4)] = nal->mvdRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(5)] = nal->coefRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(6)] = nal->chromaCoefRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(7)] = nal->chromaCoefRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(8)] = nal->pcmRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(9)] = nal->pcmChromaRowSize;
		VIDEO_UNIT[UNIT_PARAMETER(10)] = nal->pcmChromaRowSize;
		nal->state.ended = 0;
	}
	VIDEO_UNIT[UNIT_WIDTH] = width - column;
	for (i = 0; i < VIDEO_HELPER_JOB_BUFFERS; i++)
		VIDEO_UNIT[UNIT_BUFFER(i)] = vnzToPublic(run->buffers[i]);
	if (nal->load) {
		/* the lists' sizes, at most 3 for the unit */
		l0 = l1 = 0;
		if ((header & AVC_SLICE_TYPE) != AVC_SLICE_I) {
			l0 = nal->numRefIdxActive[0] - 1;
			if ((header & AVC_SLICE_TYPE) == AVC_SLICE_B)
				l1 = nal->numRefIdxActive[1] - 1;
		}
		l0 = l0 < 2 ? l0 : 2;
		l1 = l1 < 2 ? l1 : 2;
		slice = (header & 3) << 24 | (l0 & 3) << 8 | (l1 & 3);
		VIDEO_UNIT[UNIT_SLICE] = slice;
		VIDEO_UNIT[UNIT_SLICE2] = (column & 0x7f) << 16;
	}
close:
	vnzPublicClosePrivate((uint32_t)run, sizeof(VideoHelperJob));
}

/* a slice that is not run: its run's size only */
void avcdecSliceSkip(const VideoHelperShared *shared, const VideoStream *stream,
	VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	int32_t width = nal->widthInMacroblocks, first = nal->firstMacroblock;
	int32_t row = first / width, column = first % width, count, rows;

	if (column == 0) {
		rows = nal->rowsPerRun;
		if (nal->heightInMacroblocks < rows + row)
			rows = nal->heightInMacroblocks - row;
		count = width * rows;
	} else {
		count = width - column;
	}
	if (nal->nextFirstMacroblock < first + count)
		count = nal->nextFirstMacroblock - first;
	nal->runMacroblocks = count;
	nal->runRow = row;
	nal->runColumn = column;
}

static void picOrderCnt(VideoSliceJob *job);
static void picturePrepare(const VideoStream *stream, VideoSliceJob *job);
static void frameNumWrap(VideoSliceJob *job);
static void refLists(VideoSliceJob *job);
static void directTemporal(VideoSliceJob *job);
static void implicitWeights(VideoSliceJob *job);

/* the slice's run: on its picture's first slice, the picture's set up */
void avcdecSliceRun(const VideoStream *stream, VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	VideoAvcPicture *picture, *previous;
	VideoHelperSlice *slice;
	uint32_t header, type, missing;

	nal->pictureMaxFrameNum = nal->maxFrameNum;
	nal->pictureFrameNum = nal->frameNum;
	picture = job->picture;
	if (!nal->firstMacroblock) {
		picOrderCnt(job);
		picturePrepare(stream, job);
		frameNumWrap(job);
		picture = job->picture;
		picture->missingReference = 0;
		picture->free = 0;
		picture->incomplete = 0;
		picture->damaged = 0;
		picture->outputState = AVC_OUTPUT_WAITING;
		picture->picOrderCnt = nal->picOrderCnt;
		if (!nal->restart) {
			nal->restartPicture = nal->picture;
			if (nal->scanned)
				nal->outputTime = nal->picOrderCnt;
			else if (nal->picOrderCnt < nal->outputTime)
				picture->outputState = AVC_OUTPUT_GIVEN;
			else
				nal->outputTime = 0x80000000;
		} else {
			/* the unit restarts: the picture taken over keeps its reference */
			previous = &job->pictures[nal->restartPicture];
			previous->referenceInfo = nal->referenceInfo;
			picture->referenceInfo = 0;
			if (nal->picOrderCnt < nal->outputTime)
				picture->outputState = AVC_OUTPUT_GIVEN;
		}
	}
	slice = (VideoHelperSlice *)vnzPublicOpenPrivate(nal->slice, VIDEO_HELPER_SLICE_SIZE);
	header = nal->sliceHeader;
	type = header & AVC_SLICE_TYPE;
	if (type != AVC_SLICE_I) {
		job->slice = (uint32_t)slice;
		refLists(job);
		missing = picture->missingReference;
		if (missing) {
			if (nal->gapsInFrameNumAllowed != 1) {
				slice->missingReference = 1;
				picture->outputState = AVC_OUTPUT_GIVEN;
				goto close;
			}
			missing = 0;
		}
		slice->missingReference = missing;
		if (type == AVC_SLICE_B) {
			if (!(header & AVC_SLICE_DIRECT_SPATIAL))
				directTemporal(job);
			if (((int32_t)header >> AVC_SLICE_BIPRED_SHIFT & 3) == 2)
				implicitWeights(job);
		}
	} else {
		slice->missingReference = 0;
	}
close:
	vnzPublicClosePrivate((uint32_t)slice, VIDEO_HELPER_SLICE_SIZE);
}

void avcdecPictureFinish(VideoSliceJob *job);
void avcdecPictureMark(VideoSliceJob *job);

/* a picture not decoded: its references kept as if it was */
void avcdecPictureSkip(const VideoStream *stream, VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	VideoAvcPicture *picture;
	uint32_t sps, extras, size;
	int32_t error;

	sps = vnzPublicOpenProtected(stream->parameterSets + nal->parameterSet * PARAMETER_SET_SIZE,
		PARAMETER_SET_SIZE);
	size = ALIGN64(nal->pictures * PICTURE_EXTRA_SIZE);
	extras = vnzPublicOpenPrivate(stream->pictureExtras, size);
	job->parameterSet = sps;
	job->pictureExtras = extras;
	nal->pictureMaxFrameNum = nal->maxFrameNum;
	nal->pictureFrameNum = nal->frameNum;
	picOrderCnt(job);
	frameNumWrap(job);
	picture = job->picture;
	picture->picOrderCnt = nal->picOrderCnt;
	picture->outputState = 0;
	picture->damaged = 0;
	picture->incomplete = 0;
	picture->free = 1;
	picture->missingReference = 1;
	error = videoSetJump(job->jump);
	if (!error) {
		avcdecPictureFinish(job);
		if (nal->refIdc)
			avcdecPictureMark(job);
	} else {
		nal->error = error;
	}
	vnzPublicCloseProtected(sps, PARAMETER_SET_SIZE);
	vnzPublicClosePrivate(extras, size);
}

/*
 * The picture about to be decoded: the unit's command gets its frame, the
 * picture its sizes, order counts and times. The first picture takes the
 * decoder's first times where the unit has none; a later one without a DTS
 * is a frame after the last.
 */
static void picturePrepare(const VideoStream *stream, VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	VideoAvcPicture *picture;
	VideoAvcDecodeCommand *command;
	VideoAvcSei *sei;
	uint32_t seiAddress = stream->pictureData + nal->picture * PICTURE_DATA_SIZE;
	uint32_t chromaSize, lumaSize, pitch, width;
	VideoTime time;
	int32_t fields;

	if (nal->type == 5)
		nal->pocPeriod++;
	chromaSize = nal->chromaSize;
	pitch = nal->pitch;
	lumaSize = nal->lumaSize;
	width = nal->width;
	picture = job->picture;
	command = (VideoAvcDecodeCommand *)vnzPublicOpenPrivate(nal->command,
		sizeof(VideoAvcDecodeCommand));
	sei = (VideoAvcSei *)vnzPublicOpenPrivate(seiAddress, PICTURE_DATA_SIZE);
	command->picture = nal->picture;
	command->width = width;
	command->pitch = pitch;
	command->frameSize = nal->frameSize;
	command->lumaSize = lumaSize;
	command->chromaSize = chromaSize;
	command->address = picture->address;
	command->decoderData = nal->decoderData;
	command->picOrderCnt = nal->picOrderCnt;
	vnzPublicClosePrivate((uint32_t)command, sizeof(VideoAvcDecodeCommand));

	picture->outputState = AVC_OUTPUT_DECODING;
	picture->pocPeriod = nal->pocPeriod;
	picture->pixelWidth = nal->widthInMacroblocks << 4;
	picture->width = width;
	picture->pixelHeight = nal->heightInMacroblocks << 4;
	picture->pitch = pitch;
	picture->frameCropping = nal->frameCropping;
	picture->sps = stream->parameterSets + nal->parameterSet * PARAMETER_SET_SIZE;
	picture->data = stream->pictureData + nal->picture * PICTURE_DATA_SIZE;
	picture->picStruct = nal->picStruct;
	picture->picOrderCnt = nal->picOrderCnt;
	picture->topFieldOffset = nal->topFieldOrderCnt - nal->picOrderCnt;
	picture->bottomFieldOffset = nal->bottomFieldOrderCnt - nal->picOrderCnt;
	picture->frameNum = nal->frameNum;
	picture->picNum = nal->pictureFrameNum;
	picture->idr = nal->idr;
	picture->missingReference = 0;
	picture->free = 0;
	picture->incomplete = 0;
	picture->damaged = 0;
	if (stream->pts < stream->dts)
		videoParserError(0, &job->parser, 7, 0x2026);

	if (nal->picOrderCnt == 0 && nal->pictureCount == 0) {
		if (nal->firstPts < 0 || nal->firstDts < 0) {
			nal->firstPts = 0;
			nal->firstDts = 0;
			videoParserError(0, &job->parser, 7, 0x2025);
		}
		if (stream->dts == VIDEO_TIME_NONE || stream->pts == VIDEO_TIME_NONE) {
			picture->dts = nal->firstDts;
			picture->pts = stream->pts << 1;
			if (nal->frameDuration == -1)
				picture->time = VIDEO_TIME_NONE;
			else
				picture->time = picture->dts;
		} else {
			picture->dts = stream->dts << 1;
			picture->pts = stream->pts << 1;
			picture->time = picture->dts;
		}
		fields = 0;
	} else {
		picture->dts = stream->dts << 1;
		picture->pts = stream->pts << 1;
		if (stream->dts != VIDEO_TIME_NONE) {
			picture->time = picture->dts;
		} else if (nal->frameDuration == -1) {
			picture->time = VIDEO_TIME_NONE;
		} else {
			time = nal->lastTime + nal->frameDuration;
			picture->time = time;
			if (time >= VIDEO_TIME_WRAP) {
				picture->time -= VIDEO_TIME_WRAP;
				nal->timeWrap = 1 - nal->timeWrap;
			}
		}
		fields = nal->fieldCount + VIDEO_FIELDS_FRAME;
	}
	picture->fieldCount = fields;
	if (stream->option) {
		picture->cpbRemovalDelay = sei->cpbRemovalDelay;
		picture->dpbOutputDelay = sei->dpbOutputDelay;
	} else {
		picture->cpbRemovalDelay = -1;
		picture->dpbOutputDelay = -1;
	}
	nal->lastTime = picture->time;
	nal->fieldCount = picture->fieldCount;
	vnzPublicClosePrivate((uint32_t)sei, PICTURE_DATA_SIZE);
}

static void frameOut(const VideoStream *stream, VideoSliceJob *job, VideoNal *nal, int32_t index);

/*
 * The next picture in output order: of those waiting, the smallest
 * PicOrderCnt in the oldest POC period (the period count may have wrapped).
 * Its record is written and it goes into the ring; returns it, or -1.
 */
static int32_t outputNext(const VideoStream *stream, VideoSliceJob *job, VideoNal *nal)
{
	VideoAvcPicture *pictures = job->pictures;
	int32_t count = nal->pictures, index = -1, min, i;
	uint32_t first = 0xffffffff, last = 0;

	for (i = 0; i < count; i++) {
		if (pictures[i].outputState != AVC_OUTPUT_WAITING)
			continue;
		if (pictures[i].pocPeriod < first)
			first = pictures[i].pocPeriod;
		if (last < pictures[i].pocPeriod)
			last = pictures[i].pocPeriod;
	}
	if (first < 18 && !(last < 0xffffffee)) {
		first = 0xffffffff;
		for (i = 0; i < count; i++)
			if (pictures[i].outputState == AVC_OUTPUT_WAITING &&
			    pictures[i].pocPeriod < first && !(pictures[i].pocPeriod < 0xffffffee))
				first = pictures[i].pocPeriod;
	}
	min = 0x7fffffff;
	for (i = 0; i < count; i++)
		if (pictures[i].outputState == AVC_OUTPUT_WAITING && pictures[i].pocPeriod == first &&
		    pictures[i].picOrderCnt < min) {
			min = pictures[i].picOrderCnt;
			index = i;
		}
	if (index == -1)
		return index;
	frameOut(stream, job, nal, index);
	nal->frameCount++;
	nal->outputCount++;
	nal->outputRing[nal->outputEnd] = index;
	nal->outputEnd = (nal->outputEnd + 1) % 16;
	pictures[index].outputState = AVC_OUTPUT_GIVEN;
	return index;
}

static void outputAtOnce(const VideoStream *stream, VideoSliceJob *job, uint32_t flush);
static void outputInOrder(const VideoStream *stream, VideoSliceJob *job, uint32_t flush);

/* the pictures given out after one is decoded (all that wait, to flush) */
void avcdecPictureOutput(const VideoStream *stream, VideoSliceJob *job, uint32_t flush)
{
	if (stream->option == 1 || stream->option == 2)
		outputAtOnce(stream, job, flush);
	else
		outputInOrder(stream, job, flush);
}

static void outputRelease(VideoSliceJob *job);

/* the stream's options 1 and 2: the picture given out as soon as it is decoded */
static void outputAtOnce(const VideoStream *stream, VideoSliceJob *job, uint32_t flush)
{
	VideoNal *nal = job->parser.nal;
	VideoAvcPicture *pictures = job->pictures, *picture;
	int32_t records = 1, index, i;

	nal->outputCount = 0;
	nal->outputStart = nal->outputEnd;
	picture = &pictures[nal->picture];
	nal->outputFrame = nal->decoderData + nal->outputEnd * sizeof(VideoAvcFrame);
	if (flush || nal->scanned) {
		for (i = 0; i < 8; i++) {
			index = outputNext(stream, job, nal);
			if (index < 0)
				break;
			if (flush)
				pictures[index].free = 1;
			records++;
		}
		nal->outputPending = 0;
	} else {
		index = -1;
		if (picture->outputState == AVC_OUTPUT_GIVEN)
			picture->outputState = AVC_OUTPUT_RELEASED;
		if (stream->option == 1 && !(nal->fieldCount < 2))
			index = outputNext(stream, job, nal);
		if (stream->option == 2)
			index = outputNext(stream, job, nal);
	}
	nal->outputCounts[nal->outputSlot] = records;
	nal->outputSlot = (nal->outputSlot + 1) & 1;
	if (picture->missingReference && !nal->gapsInFrameNumAllowed)
		picture->outputState = AVC_OUTPUT_RELEASED;
	if (index >= 0)
		outputRelease(job);
}

/*
 * Output order: a picture goes once num_reorder_frames are waiting, or, for
 * a stream that reorders as far as it keeps references, once the pictures
 * held (references and waiting) pass the frames it keeps.
 */
static void outputInOrder(const VideoStream *stream, VideoSliceJob *job, uint32_t flush)
{
	VideoNal *nal = job->parser.nal;
	VideoAvcPicture *pictures = job->pictures, *picture;
	uint32_t limit = nal->numOfRefFrames, reorder = nal->numReorderFrames;
	int32_t count = nal->pictures, records = 0, held, index, i;

	nal->outputStart = nal->outputEnd;
	nal->outputCount = 0;
	picture = &pictures[nal->picture];
	nal->outputFrame = nal->decoderData + nal->outputEnd * sizeof(VideoAvcFrame);
	if (flush || nal->scanned) {
		for (i = 0; i < 8; i++) {
			index = outputNext(stream, job, nal);
			if (index < 0)
				break;
			if (flush)
				pictures[index].free = 1;
			records++;
		}
		nal->outputPending = 0;
	} else {
		if (picture->outputState == AVC_OUTPUT_GIVEN)
			picture->outputState = AVC_OUTPUT_RELEASED;
		held = 0;
		for (i = 0; i < count; i++)
			if (pictures[i].referenceInfo || pictures[i].outputState == AVC_OUTPUT_WAITING)
				held++;
		if (reorder < limit) {
			if (!(nal->outputPending < reorder)) {
				if (outputNext(stream, job, nal) >= 0)
					records = 1;
			} else if (picture->outputState != AVC_OUTPUT_RELEASED) {
				nal->outputPending++;
			}
		} else {
			if (nal->type == 5)
				limit = nal->pictures < 5 ? 1 : 4;
			while ((int32_t)limit < held) {
				index = outputNext(stream, job, nal);
				if (index < 0)
					break;
				if (++records >= 15)
					break;
				if (!pictures[index].referenceInfo)
					held--;
			}
		}
	}
	nal->outputCounts[nal->outputSlot] = records;
	nal->outputSlot = (nal->outputSlot + 1) & 1;
	if (picture->missingReference && !nal->gapsInFrameNumAllowed)
		picture->outputState = AVC_OUTPUT_RELEASED;
	outputRelease(job);
}

/* the pictures released and no longer references: free again */
static void __attribute__((noinline, noclone)) picturesFree(VideoSliceJob *job, const VideoNal *nal)
{
	VideoAvcPicture *picture = job->pictures;
	int32_t count = nal->pictures, i;

	for (i = 0; i < count; i++, picture++) {
		if (picture->referenceInfo || picture->outputState != AVC_OUTPUT_RELEASED)
			continue;
		picture->pocPeriod = 0;
		picture->outputState = 0;
		picture->free = 1;
	}
}

/* the records of the picture two back are done with: their pictures released */
static void outputRelease(VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	VideoAvcPicture *pictures = job->pictures;
	uint8_t *slot = &nal->outputCounts[nal->outputSlot];
	int32_t records = *slot;

	if (records >= 1)
		*slot = 0;
	while (records-- >= 1) {
		pictures[nal->outputRing[nal->outputDone]].outputState = AVC_OUTPUT_RELEASED;
		nal->outputDone = (nal->outputDone + 1) % 16;
	}
	picturesFree(job, nal);
}

/* the picture's record: its frame, sizes, cropping and times */
static void frameOut(const VideoStream *stream, VideoSliceJob *job, VideoNal *nal, int32_t index)
{
	const VideoAvcPicture *picture;
	const VideoAvcSps *sps;
	uint32_t lumaSize = nal->lumaSize, fields;
	VideoAvcFrame *frame;
	VideoTime pts, base;

	if (index < 0)
		return;
	frame = (VideoAvcFrame *)vnzPublicOpenPrivate(nal->decoderData +
		nal->outputEnd * sizeof(VideoAvcFrame), sizeof(VideoAvcFrame));
	vnzCacheInvalidateInside((uint32_t)frame, sizeof(VideoAvcFrame));
	picture = &job->pictures[index];
	frame->next = nal->decoderData + (nal->outputEnd + 1) % 16 * sizeof(VideoAvcFrame);
	sps = (const VideoAvcSps *)picture->sps;
	frame->frameRateCode = sps->frameRateCode;
	frame->vui = sps->vui;
	frame->sei = picture->data;
	frame->picOrderCnt = picture->picOrderCnt;
	frame->luma = picture->address;
	frame->chroma = picture->address + lumaSize;
	frame->width = picture->pixelWidth;
	frame->height = picture->pixelHeight;
	frame->chromaWidth = frame->width / 2;
	frame->chromaHeight = frame->height / 2;
	frame->bufferWidth = picture->width;
	frame->pitch = picture->pitch;
	frame->number = nal->frameCount;
	if (picture->frameCropping) {
		frame->frameCropping = 1;
		frame->cropUnitY = 2;
		frame->cropUnitX = 2;
		frame->cropLeft = sps->cropLeft;
		frame->cropRight = sps->cropRight;
		frame->cropTop = sps->cropTop;
		frame->cropBottom = sps->cropBottom;
	} else {
		frame->frameCropping = 0;
	}
	frame->errors = 0;
	if (picture->damaged)
		frame->errors |= 1;
	if (picture->incomplete)
		frame->errors |= 2;
	frame->picStruct = picture->picStruct;
	frame->idr = picture->idr;
	frame->topFieldOffset = picture->topFieldOffset;
	frame->bottomFieldOffset = picture->bottomFieldOffset;

	/* the PTS given, or one a frame on from the last (the decoder's first for the first) */
	pts = picture->pts >> 1;
	base = nal->frameCount == 0 ? nal->firstPts : nal->lastPts;
	if (pts != VIDEO_TIME_NONE) {
		frame->pts = picture->pts;
	} else if (nal->frameDuration == -1) {
		frame->pts = VIDEO_TIME_NONE;
	} else {
		frame->pts = base + nal->frameDuration;
		if (frame->pts > VIDEO_TIME_WRAP) {
			frame->pts -= VIDEO_TIME_WRAP;
			nal->timeWrap = 1;
		}
	}
	fields = nal->frameCount == 0 ? VIDEO_FIELDS_FRAME : nal->frameFields + VIDEO_FIELDS_FRAME;
	nal->frameFields = fields;
	frame->fields = fields;
	nal->lastPts = frame->pts;
	frame->pts >>= 1;
	frame->dts = picture->time >> 1;
	frame->fieldCount = picture->fieldCount;
	vnzPublicClosePrivate((uint32_t)frame, sizeof(VideoAvcFrame));
}

static void picOrderCntType0(VideoSliceJob *job);
static void picOrderCntType1(VideoSliceJob *job);
static void picOrderCntType2(VideoSliceJob *job);

/* 8.2.1: the picture's order counts, by the sequence's type */
static void picOrderCnt(VideoSliceJob *job)
{
	const VideoAvcSps *sps = (const VideoAvcSps *)job->parameterSet;

	if (sps->picOrderCntType == 0)
		picOrderCntType0(job);
	else if (sps->picOrderCntType == 1)
		picOrderCntType1(job);
	else
		picOrderCntType2(job);
}

/* 8.2.1.1: pic_order_cnt_type 0, from pic_order_cnt_lsb and the last reference's */
static void __attribute__((noinline, noclone)) picOrderCntType0(VideoSliceJob *job)
{
	const VideoAvcSps *sps = (const VideoAvcSps *)job->parameterSet;
	VideoNal *nal = job->parser.nal;
	int32_t prevMsb, prevLsb, msb, lsb, half, max, top, bottom;

	if (nal->type == NAL_IDR) {
		prevMsb = 0;
		prevLsb = 0;
	} else {
		prevMsb = nal->prevPicOrderCntMsb;
		prevLsb = nal->prevPicOrderCntLsb;
	}
	max = sps->maxPicOrderCntLsb;
	lsb = nal->picOrderCntLsb;
	half = max / 2;
	msb = prevMsb;
	if (lsb < prevLsb && !(prevLsb - lsb < half))
		msb = prevMsb + max;
	else if (prevLsb < lsb && half < lsb - prevLsb)
		msb = prevMsb - max;
	top = msb + lsb;
	nal->topFieldOrderCnt = top;
	bottom = nal->deltaPicOrderCnt[0] + top;
	nal->bottomFieldOrderCnt = bottom;
	nal->picOrderCnt = top < bottom ? top : bottom;
	if (nal->refIdc) {
		if (nal->mmco5) {
			nal->prevPicOrderCntMsb = 0;
			nal->prevPicOrderCntLsb = 0;
		} else {
			nal->prevPicOrderCntLsb = lsb;
			nal->prevPicOrderCntMsb = msb;
		}
	}
	if (nal->type == NAL_IDR)
		nal->frameNumOffset = 0;
}

/* 8.2.1.2: pic_order_cnt_type 1, from the frame number and the sequence's cycle of offsets */
static void __attribute__((noinline, noclone)) picOrderCntType1(VideoSliceJob *job)
{
	const VideoAvcSps *sps = (const VideoAvcSps *)job->parameterSet;
	VideoNal *nal = job->parser.nal;
	int32_t top, bottom, offset, absFrameNum;
	int32_t cycle, count, inCycle, expectedDelta, expected, i;

	cycle = sps->numRefFramesInPicOrderCntCycle;
	if (nal->type == NAL_IDR) {
		offset = 0;
	} else {
		offset = nal->frameNumOffset;
		if (nal->frameNum < nal->prevFrameNum)
			offset = nal->maxFrameNum + offset;
	}
	absFrameNum = cycle ? nal->frameNum + offset : 0;
	if (!nal->refIdc && absFrameNum > 0)
		absFrameNum--;
	expected = 0;
	if (absFrameNum > 0) {
		count = (absFrameNum - 1) / cycle;
		inCycle = (absFrameNum - 1) % cycle;
		expectedDelta = 0;
		for (i = 0; i < cycle; i++)
			expectedDelta += sps->offsetForRefFrame[i];
		expected = count * expectedDelta;
		for (i = 0; i <= inCycle; i++)
			expected += sps->offsetForRefFrame[i];
	}
	if (!nal->refIdc)
		expected = sps->offsetForNonRefPic + expected;
	top = nal->deltaPicOrderCnt[0] + expected;
	nal->topFieldOrderCnt = top;
	bottom = sps->offsetForTopToBottomField + top + nal->deltaPicOrderCnt[1];
	nal->bottomFieldOrderCnt = bottom;
	nal->picOrderCnt = top < bottom ? top : bottom;
	if (nal->mmco5)
		offset = 0;
	nal->frameNumOffset = offset;
	if (nal->type == NAL_IDR) {
		nal->prevPicOrderCntLsb = 0;
		nal->prevPicOrderCntMsb = 0;
	}
}

/* 8.2.1.3: pic_order_cnt_type 2, twice the frame number (less one when not a reference) */
static void __attribute__((noinline, noclone)) picOrderCntType2(VideoSliceJob *job)
{
	const VideoAvcSps *sps = (const VideoAvcSps *)job->parameterSet;
	VideoNal *nal = job->parser.nal;
	int32_t top, offset;

	offset = 0;
	if (nal->type != NAL_IDR) {
		offset = nal->frameNumOffset;
		if (nal->frameNum < nal->prevFrameNum)
			offset = sps->maxFrameNum + offset;
	}
	top = 0;
	if (nal->type != NAL_IDR) {
		top = (nal->frameNum + offset) * 2;
		if (!nal->refIdc)
			top--;
	}
	nal->picOrderCnt = top;
	nal->topFieldOrderCnt = top;
	nal->bottomFieldOrderCnt = top;
	if (nal->mmco5)
		offset = 0;
	nal->frameNumOffset = offset;
	if (nal->type == NAL_IDR) {
		nal->prevPicOrderCntLsb = 0;
		nal->prevPicOrderCntMsb = 0;
	}
}

/* 8.2.4.1: the references' FrameNumWrap and PicNum, LongTermPicNum */
static void frameNumWrap(VideoSliceJob *job)
{
	VideoAvcPicture *picture = job->pictures;
	VideoNal *nal = job->parser.nal;
	VideoAvcPictureExtra *extra = (VideoAvcPictureExtra *)job->pictureExtras;
	uint32_t max = nal->maxFrameNum, current = job->picture->frameNum;
	int32_t count = nal->pictures, i, wrap;

	for (i = 0; i < count; i++, picture++) {
		if (!picture->referenceInfo)
			continue;
		wrap = picture->frameNum;
		if ((int32_t)current < wrap)
			wrap -= max;
		picture->picNum = wrap;
		picture->frameNumWrap = wrap;
		picture->longTermPicNum = picture->longTermFrameIdx;
	}
	for (i = 0; i < count; i++, extra++) {
		if (extra->reference != 1)
			continue;
		wrap = extra->frameNum;
		if ((int32_t)current < wrap)
			wrap -= max;
		extra->frameNumWrap = wrap;
	}
}

static void refListInit(VideoSliceJob *job, const VideoNal *nal);
static int32_t refListInitP(VideoSliceJob *job, const VideoNal *nal);
static int32_t refListInitB(VideoSliceJob *job, const VideoNal *nal);
static void refListReorder(VideoSliceJob *job, VideoNal *nal);

/* the slice's reference lists, and what the picture inherits of them */
static void refLists(VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	VideoAvcPicture *picture = job->picture, *pictures, *reference;
	VideoHelperSlice *slice = (VideoHelperSlice *)job->slice;
	int32_t i, index;

	refListInit(job, nal);
	refListReorder(job, nal);
	pictures = job->pictures;
	slice->refListSize[0] = job->refListSize[0];
	slice->refList[1][0] = -1;
	slice->refList[0][0] = -1;
	slice->refListSize[1] = job->refListSize[1];
	for (i = 0; i < nal->numRefIdxActive[0]; i++) {
		index = slice->refList[0][1 + i];
		if (index < 0)
			continue;
		reference = &pictures[index];
		picture->incomplete |= reference->damaged;
		picture->incomplete |= reference->incomplete;
		picture->missingReference |= reference->free;
		picture->missingReference |= reference->missingReference;
	}
	if ((nal->sliceHeader & AVC_SLICE_TYPE) != AVC_SLICE_B)
		return;
	slice->colocatedShortTerm = pictures[slice->refList[1][1]].referenceInfo == 1;
	for (i = 0; i < nal->numRefIdxActive[1]; i++) {
		index = slice->refList[1][1 + i];
		if (index < 0)
			continue;
		reference = &pictures[index];
		picture->incomplete |= reference->damaged;
		picture->incomplete |= reference->incomplete;
		picture->missingReference |= reference->free;
		picture->missingReference |= reference->missingReference;
	}
}

/* 8.2.4.2: the initial lists, sized to the slice's active references */
static void refListInit(VideoSliceJob *job, const VideoNal *nal)
{
	int32_t count, l0, l1;

	if ((nal->sliceHeader & AVC_SLICE_TYPE) == AVC_SLICE_P)
		count = refListInitP(job, nal);
	else
		count = refListInitB(job, nal);
	if (!count)
		videoParserError(0, &job->parser, 7, nal->pictureCount ? 0x2024 : 0x4024);
	l0 = nal->numRefIdxActive[0];
	if (!nal->refPicListReordering[0])
		job->refListSize[0] = count < l0 ? count : l0;
	else
		job->refListSize[0] = count > l0 ? count : l0;
	l1 = nal->numRefIdxActive[1];
	if (!nal->refPicListReordering[1])
		job->refListSize[1] = count < l1 ? count : l1;
	else
		job->refListSize[1] = count > l1 ? count : l1;
}

/* P: short-term by PicNum descending, then long-term by LongTermPicNum */
static int32_t refListInitP(VideoSliceJob *job, const VideoNal *nal)
{
	VideoHelperSlice *slice = (VideoHelperSlice *)job->slice;
	const VideoAvcPicture *pictures = job->pictures;
	int8_t *list = &slice->refList[0][1];
	int32_t count = nal->pictures, shortTerm, total, i, j, a, b;

	shortTerm = 0;
	for (i = 0; i < count; i++)
		if (pictures[i].referenceInfo == 1)
			list[shortTerm++] = i;
	total = shortTerm;
	for (i = 0; i < count; i++)
		if (pictures[i].referenceInfo == 2)
			list[total++] = i;
	for (i = 0; i < shortTerm - 1; i++)
		for (j = i + 1; j < shortTerm; j++) {
			a = list[i];
			b = list[j];
			if (pictures[a].picNum < pictures[b].picNum) {
				list[i] = b;
				list[j] = a;
			}
		}
	for (i = shortTerm; i < total - 1; i++)
		for (j = i + 1; j < total; j++) {
			a = list[i];
			b = list[j];
			if (pictures[b].longTermPicNum < pictures[a].longTermPicNum) {
				list[i] = b;
				list[j] = a;
			}
		}
	for (i = total; i <= 16; i++)
		list[i] = -1;
	return total;
}

/*
 * B: short-term before the picture by POC descending, then after it
 * ascending (list 1 the other way round), then long-term; list 1 not to
 * equal list 0.
 */
static int32_t refListInitB(VideoSliceJob *job, const VideoNal *nal)
{
	VideoHelperSlice *slice = (VideoHelperSlice *)job->slice;
	const VideoAvcPicture *pictures = job->pictures;
	int8_t *list0 = &slice->refList[0][1], *list1 = &slice->refList[1][1];
	int32_t poc = nal->picOrderCnt, count = nal->pictures, before, after, total, longTerm;
	int32_t i, j, a, b;

	before = after = 0;
	for (i = 0; i < count; i++) {
		if (pictures[i].referenceInfo != 1)
			continue;
		if (pictures[i].picOrderCnt < poc)
			list0[before++] = i;
		else
			list1[after++] = i;
	}
	for (i = 0; i < before - 1; i++)
		for (j = i + 1; j < before; j++) {
			a = list0[i];
			b = list0[j];
			if (pictures[a].picOrderCnt < pictures[b].picOrderCnt) {
				list0[i] = b;
				list0[j] = a;
			}
		}
	for (i = 0; i < after - 1; i++)
		for (j = i + 1; j < after; j++) {
			a = list1[i];
			b = list1[j];
			if (pictures[b].picOrderCnt < pictures[a].picOrderCnt) {
				list1[i] = b;
				list1[j] = a;
			}
		}
	for (i = 0; i < after; i++)
		list0[before + i] = list1[i];
	for (i = 0; i < before; i++)
		list1[after + i] = list0[i];
	total = before + after;
	longTerm = 0;
	for (i = 0; i < count; i++)
		if (pictures[i].referenceInfo == 2)
			list0[total + longTerm++] = i;
	for (i = total; i < total + longTerm - 1; i++)
		for (j = i + 1; j < total + longTerm; j++) {
			a = list0[i];
			b = list0[j];
			if (pictures[b].longTermPicNum < pictures[a].longTermPicNum) {
				list0[i] = b;
				list0[j] = a;
			}
		}
	for (i = 0; i < longTerm; i++)
		list1[total + i] = list0[total + i];
	total += longTerm;
	if (total >= 2) {
		for (i = 0; i < total; i++)
			if (list1[i] != list0[i])
				break;
		if (i >= total) {
			a = list1[0];
			list1[0] = list1[1];
			list1[1] = a;
		}
	}
	for (i = total; i <= 16; i++) {
		list0[i] = -1;
		list1[i] = -1;
	}
	return total;
}

static int32_t reorderShortTerm(VideoSliceJob *job, const VideoNal *nal, int32_t refIdx,
	uint32_t absDiffPicNum, uint32_t idc, int32_t picNumPred, uint32_t list);
static void reorderLongTerm(VideoSliceJob *job, const VideoNal *nal, int32_t refIdx,
	int32_t longTermPicNum, uint32_t list);

/* 8.2.4.3: each list's reordering, on a working copy, then its active part */
static void refListReorder(VideoSliceJob *job, VideoNal *nal)
{
	VideoHelperSlice *slice = (VideoHelperSlice *)job->slice;
	int32_t pred, i, size;
	uint32_t list, idc;

	for (list = 0; list < 2; list++) {
		pred = nal->pictureFrameNum;
		if (nal->refPicListReordering[list]) {
			for (i = 0; i < (uint8_t)job->refListSize[list]; i++)
				job->refListWork[i] = slice->refList[list][1 + i];
			for (i = 0; ; i++) {
				idc = nal->reorderingIdc[list][i];
				if (idc < 2)
					pred = reorderShortTerm(job, nal, i, nal->reorderingValue[list][i],
						idc, pred, list);
				else if (idc == 2)
					reorderLongTerm(job, nal, i, nal->reorderingValue[list][i], list);
				else
					break;
				job->refListSize[list] = (uint8_t)job->refListSize[list] + 1;
			}
			for (i = 0; i < nal->numRefIdxActive[list]; i++)
				slice->refList[list][1 + i] = job->refListWork[i];
		}
		size = (uint8_t)job->refListSize[list];
		job->refListSize[list] = size < nal->numRefIdxActive[list] ?
			size : nal->numRefIdxActive[list];
	}
}

/* a short-term reference's PicNum, MaxPicNum for anything else */
static inline int32_t shortTermPicNum(const VideoSliceJob *job, const VideoNal *nal, int32_t index)
{
	const VideoAvcPicture *picture;

	if (index >= 0 && index < 18) {
		picture = &job->pictures[index];
		if (picture->referenceInfo == 1)
			return picture->picNum;
	}
	return nal->pictureMaxFrameNum;
}

/* a long-term reference's LongTermPicNum, -1 for anything else */
static inline int32_t longTermPicNum(const VideoSliceJob *job, int32_t index)
{
	const VideoAvcPicture *picture;

	if (index >= 0 && index < 18) {
		picture = &job->pictures[index];
		if (picture->referenceInfo == 2)
			return picture->longTermPicNum;
	}
	return -1;
}

/* 8.2.4.3.1: a short-term picture moved to refIdx; returns picNumLXNoWrap */
static int32_t reorderShortTerm(VideoSliceJob *job, const VideoNal *nal, int32_t refIdx,
	uint32_t absDiffPicNum, uint32_t idc, int32_t picNumPred, uint32_t list)
{
	int32_t current = nal->pictureFrameNum, max = nal->pictureMaxFrameNum;
	int32_t noWrap, picNum, size, c, t, n, index;
	int8_t *work = job->refListWork;

	if (idc == 0) {
		noWrap = picNumPred - (int32_t)absDiffPicNum - 1;
		if (noWrap < 0)
			noWrap += max;
	} else {
		noWrap = (int32_t)absDiffPicNum + picNumPred + 1;
		if (!(noWrap < max))
			noWrap -= max;
	}
	picNum = noWrap;
	if (current < noWrap)
		picNum -= max;
	size = (uint8_t)job->refListSize[list != 0];
	for (c = size; refIdx < c; c--)
		work[c] = work[c - 1];
	for (t = 0; t < size + 1; t++)
		if (shortTermPicNum(job, nal, work[t]) == picNum)
			break;
	if (!(t < size + 1))
		videoParserError(0, &job->parser, 7, 0x2024);
	work[refIdx++] = work[t];
	n = refIdx;
	for (c = refIdx; c <= size; c++) {
		index = work[c];
		if (shortTermPicNum(job, nal, index) != picNum)
			work[n++] = index;
	}
	return noWrap;
}

/* 8.2.4.3.2: a long-term picture moved to refIdx */
static void reorderLongTerm(VideoSliceJob *job, const VideoNal *nal, int32_t refIdx,
	int32_t picNum, uint32_t list)
{
	int32_t size, c, t, n, index;
	int8_t *work = job->refListWork;

	size = (uint8_t)job->refListSize[list != 0];
	for (c = size; refIdx < c; c--)
		work[c] = work[c - 1];
	for (t = 0; t < size + 1; t++)
		if (longTermPicNum(job, work[t]) == picNum)
			break;
	if (!(t < size + 1))
		videoParserError(0, &job->parser, 7, 0x2024);
	work[refIdx++] = work[t];
	n = refIdx;
	for (c = refIdx; c <= size; c++) {
		index = work[c];
		if (longTermPicNum(job, index) != picNum)
			work[n++] = index;
	}
}

static inline int32_t clip3(int32_t low, int32_t high, int32_t value)
{
	if (value < high)
		return value < -high ? low : value;
	return high;
}

/* 8.4.1.2.3: temporal direct's DistScaleFactor, and list 0's index of each picture */
static void directTemporal(VideoSliceJob *job)
{
	const VideoAvcPicture *pictures = job->pictures, *ref0, *ref1, *current;
	const VideoNal *nal = job->parser.nal;
	VideoHelperSlice *slice = (VideoHelperSlice *)job->slice;
	int32_t count = nal->numRefIdxActive[0], i, index, tb, td, tx, scale;

	current = &pictures[nal->picture];
	ref1 = &pictures[slice->refList[1][1]];
	for (i = 0; i < count; i++) {
		index = slice->refList[0][1 + i];
		scale = 256;
		if (index >= 0) {
			ref0 = &pictures[index];
			if (ref0->referenceInfo != 2 && ref1->picOrderCnt - ref0->picOrderCnt) {
				tb = clip3(-128, 127, current->picOrderCnt - ref0->picOrderCnt);
				td = clip3(-128, 127, ref1->picOrderCnt - ref0->picOrderCnt);
				tx = (16384 + abs(td / 2)) / td;
				scale = clip3(-1024, 1023, (tb * tx + 32) >> 6);
			}
		}
		slice->distScaleFactor[i] = scale;
	}
	for (i = 0; i < 18; i++)
		slice->refIdxOfPicture[i] = 0;
	for (i = count - 1; i >= 0; i--) {
		index = slice->refList[0][1 + i];
		if (index >= 0)
			slice->refIdxOfPicture[index] = i;
	}
}

static void slidingWindow(VideoSliceJob *job, VideoNal *nal, uint32_t current);
static void adaptiveMarking(VideoSliceJob *job, VideoNal *nal);
static void unmarkReference(VideoSliceJob *job, const VideoNal *nal, int32_t index);

/* 8.2.5.1: the decoded picture marked as a reference */
void avcdecPictureMark(VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	VideoAvcPicture *picture = job->picture;

	if (nal->type == 5) {
		unmarkReference(job, nal, -1);
		picture->referenceInfo = (nal->longTermReference != 0) + 1;
	} else if (!nal->adaptiveRefPicMarking) {
		slidingWindow(job, nal, 1);
	} else {
		adaptiveMarking(job, nal);
	}
}

/* 8.2.5.2: the frames a gap in frame_num skips, as short-term references */
void avcdecPictureFinish(VideoSliceJob *job)
{
	VideoNal *nal = job->parser.nal;
	VideoAvcPictureExtra *extras = (VideoAvcPictureExtra *)job->pictureExtras, *extra;
	int32_t count = nal->pictures, i;
	uint32_t prev = nal->prevFrameNum, frameNum = nal->frameNum, mask, unused;

	nal->prevFrameNum = frameNum;
	if (nal->type == 5 || !nal->refIdc)
		return;
	mask = nal->maxFrameNum - 1;
	unused = (prev + 1) & mask;
	if (!nal->gapsInFrameNumAllowed || frameNum == prev || frameNum == unused)
		return;
	do {
		for (i = 0, extra = extras; i < count; i++, extra++)
			if (!extra->reference)
				break;
		if (i < count) {
			extra->reference = 1;
			extra->frameNum = unused;
		} else {
			videoParserError(0, &job->parser, 0, 0x2021);
		}
		frameNumWrap(job);
		slidingWindow(job, nal, 0);
		unused = (unused + 1) & mask;
	} while (frameNum != unused);
}

/*
 * 8.2.5.3: the sliding window. The short-term reference with the smallest
 * FrameNumWrap goes once the references fill num_ref_frames (one more for a
 * gap's frame, which is not yet counted); the current picture is then marked.
 */
static void slidingWindow(VideoSliceJob *job, VideoNal *nal, uint32_t current)
{
	VideoAvcPicture *pictures = job->pictures;
	VideoAvcPictureExtra *extras = (VideoAvcPictureExtra *)job->pictureExtras;
	int32_t count = nal->pictures, shortTerm = 0, longTerm = 0, limit, min, index, i;
	uint32_t extra = 1;

	for (i = 0; i < count; i++) {
		if (pictures[i].referenceInfo == 1)
			shortTerm++;
		else if (pictures[i].referenceInfo == 2)
			longTerm++;
	}
	for (i = 0; i < count; i++)
		if (extras[i].reference == 1)
			shortTerm++;
	limit = nal->numOfRefFrames < nal->numRefFrames ? nal->numOfRefFrames : nal->numRefFrames;
	if (!current)
		limit++;
	if (limit < 1)
		limit = 1;
	if (shortTerm + longTerm >= limit && shortTerm >= 1) {
		min = 0xffff;
		index = -1;
		for (i = 0; i < count; i++)
			if (extras[i].reference == 1 && extras[i].frameNumWrap < min) {
				index = i;
				min = extras[i].frameNumWrap;
			}
		for (i = 0; i < count; i++)
			if (pictures[i].referenceInfo == 1 && pictures[i].frameNumWrap < min) {
				extra = 0;
				index = i;
				min = pictures[i].frameNumWrap;
			}
		if (index >= 0) {
			if (extra) {
				extras[index].reference = 0;
			} else {
				pictures[index].referenceInfo = 0;
				unmarkReference(job, nal, index);
			}
		}
	}
	if (current)
		job->picture->referenceInfo = 1;
}

static void mmcoShortTermUnused(VideoSliceJob *job, const VideoNal *nal, uint32_t difference);
static void mmcoLongTermUnused(VideoSliceJob *job, const VideoNal *nal, uint32_t longTermPicNum);
static void mmcoLongTermIdxFree(VideoSliceJob *job, const VideoNal *nal, uint32_t longTermFrameIdx);
static void __attribute__((noinline, noclone)) mmcoShortTermToLongTerm(VideoSliceJob *job, const VideoNal *nal, uint32_t difference,
	uint32_t longTermFrameIdx);
static void mmcoMaxLongTermIdx(VideoSliceJob *job, const VideoNal *nal, uint32_t maxPlus1);
static void mmcoAllUnused(VideoSliceJob *job, VideoNal *nal);
static void mmcoCurrentLongTerm(VideoSliceJob *job, const VideoNal *nal, uint32_t longTermFrameIdx);

/* 8.2.5.4: the slice's memory_management_control_operations, in order */
static void adaptiveMarking(VideoSliceJob *job, VideoNal *nal)
{
	const uint8_t *operation = nal->mmcoOperation;
	const uint16_t *argument = nal->mmcoArgument;
	VideoAvcPicture *picture = job->picture;
	uint32_t difference;

	for (;;) {
		switch (*operation++) {
		case 1:
			mmcoShortTermUnused(job, nal, *argument++);
			break;
		case 2:
			mmcoLongTermUnused(job, nal, *argument++);
			break;
		case 3:
			difference = argument[0];
			mmcoLongTermIdxFree(job, nal, argument[1]);
			mmcoShortTermToLongTerm(job, nal, difference, argument[1]);
			argument += 2;
			break;
		case 4:
			mmcoMaxLongTermIdx(job, nal, *argument++);
			break;
		case 5:
			mmcoAllUnused(job, nal);
			break;
		case 6:
			mmcoCurrentLongTerm(job, nal, *argument++);
			break;
		default:
			if (picture->referenceInfo != 2)
				picture->referenceInfo = 1;
			return;
		}
	}
}

/* a picture (all of them for -1) no longer a reference */
static void unmarkReference(VideoSliceJob *job, const VideoNal *nal, int32_t index)
{
	VideoAvcPicture *picture = job->pictures;
	int32_t count, i;

	if (index >= 0) {
		picture[index].referenceInfo = 0;
		picture[index].longTermFrameIdx = 0;
		return;
	}
	count = nal->pictures;
	for (i = 0; i < count; i++, picture++) {
		picture->referenceInfo = 0;
		picture->longTermFrameIdx = 0;
	}
}

/* operation 1: a short-term picture (or a gap's frame) unused */
static void __attribute__((noinline, noclone)) mmcoShortTermUnused(VideoSliceJob *job, const VideoNal *nal, uint32_t difference)
{
	const VideoAvcPicture *picture = job->pictures;
	VideoAvcPictureExtra *extra = (VideoAvcPictureExtra *)job->pictureExtras;
	int32_t picNum = nal->pictureFrameNum - difference - 1, count = nal->pictures, i;

	for (i = 0; i < count; i++, picture++)
		if (picture->picNum == picNum && picture->referenceInfo == 1) {
			unmarkReference(job, nal, i);
			return;
		}
	for (i = 0; i < count; i++, extra++)
		if (extra->frameNumWrap == picNum && extra->reference == 1) {
			extra->reference = 0;
			return;
		}
	videoParserError(0, &job->parser, 0, 0x2021);
}

/* operation 2: a long-term picture unused */
static void __attribute__((noinline, noclone)) mmcoLongTermUnused(VideoSliceJob *job, const VideoNal *nal, uint32_t longTermPicNum)
{
	const VideoAvcPicture *picture = job->pictures;
	int32_t count = nal->pictures, i;

	for (i = 0; i < count; i++, picture++)
		if (picture->longTermPicNum == (int32_t)longTermPicNum && picture->referenceInfo == 2) {
			unmarkReference(job, nal, i);
			return;
		}
	videoParserError(0, &job->parser, 0, 0x2021);
}

/* operation 3's first part: the long-term picture holding the index goes */
static void __attribute__((noinline, noclone)) mmcoLongTermIdxFree(VideoSliceJob *job, const VideoNal *nal, uint32_t longTermFrameIdx)
{
	const VideoAvcPicture *picture = job->pictures;
	int32_t count = nal->pictures, i;

	for (i = 0; i < count; i++, picture++)
		if (picture->longTermFrameIdx == (int32_t)longTermFrameIdx && picture->referenceInfo == 2) {
			unmarkReference(job, nal, i);
			return;
		}
}

/* operation 3: a short-term picture made long-term */
static void __attribute__((noinline, noclone)) mmcoShortTermToLongTerm(VideoSliceJob *job, const VideoNal *nal, uint32_t difference,
	uint32_t longTermFrameIdx)
{
	VideoAvcPicture *picture = job->pictures;
	int32_t picNum = nal->pictureFrameNum - difference - 1, count = nal->pictures, i;

	for (i = 0; i < count; i++, picture++)
		if (picture->picNum == picNum && picture->referenceInfo == 1) {
			picture->longTermFrameIdx = longTermFrameIdx;
			picture->referenceInfo = 2;
			return;
		}
	videoParserError(0, &job->parser, 0, 0x2021);
}

/* operation 4: the long-term pictures above the new maximum index go */
static void __attribute__((noinline, noclone)) mmcoMaxLongTermIdx(VideoSliceJob *job, const VideoNal *nal, uint32_t maxPlus1)
{
	int32_t max = -1, count = nal->pictures, i;

	if (maxPlus1)
		max = maxPlus1 - 1;
	for (i = 0; i < count; i++)
		if (max < job->pictures[i].longTermFrameIdx && job->pictures[i].referenceInfo == 2)
			unmarkReference(job, nal, i);
}

/* operation 5: every reference goes, and the picture's frame_num becomes 0 */
static void __attribute__((noinline, noclone)) mmcoAllUnused(VideoSliceJob *job, VideoNal *nal)
{
	VideoAvcPictureExtra *extra = (VideoAvcPictureExtra *)job->pictureExtras;
	VideoAvcPicture *picture = job->picture;
	int32_t count = nal->pictures, i;

	unmarkReference(job, nal, -1);
	picture->frameNum = 0;
	nal->prevFrameNum = 0;
	for (i = 0; i < count; i++, extra++)
		extra->reference = 0;
	nal->pocPeriod++;
}

/* operation 6: the current picture long-term */
static void __attribute__((noinline, noclone)) mmcoCurrentLongTerm(VideoSliceJob *job, const VideoNal *nal, uint32_t longTermFrameIdx)
{
	VideoAvcPicture *picture = job->picture;

	picture->longTermFrameIdx = longTermFrameIdx;
	picture->referenceInfo = 2;
}

/* 8.4.2.3.1: implicit bi-prediction's weights, from the order count distances */
static void implicitWeights(VideoSliceJob *job)
{
	const VideoAvcPicture *pictures = job->pictures, *ref0, *ref1;
	const VideoNal *nal = job->parser.nal;
	VideoHelperSlice *slice = (VideoHelperSlice *)job->slice;
	int32_t current = nal->picture, l0 = nal->numRefIdxActive[0], l1 = nal->numRefIdxActive[1];
	int32_t i, j, index0, index1, tb, td, tx, scale, weight;

	for (i = 0; i < l0; i++) {
		index0 = slice->refList[0][1 + i];
		for (j = 0; j < l1; j++) {
			index1 = slice->refList[1][1 + j];
			weight = 32;
			if (index0 >= 0 && index1 >= 0) {
				ref0 = &pictures[index0];
				ref1 = &pictures[index1];
				if (ref0->referenceInfo != 2 && ref1->referenceInfo != 2 &&
				    ref1->picOrderCnt - ref0->picOrderCnt) {
					td = clip3(-128, 127, ref1->picOrderCnt - ref0->picOrderCnt);
					tx = (16384 + abs(td / 2)) / td;
					tb = clip3(-128, 127, pictures[current].picOrderCnt - ref0->picOrderCnt);
					scale = clip3(-1024, 1023, (tb * tx + 32) >> 6) >> 2;
					if (!(scale < -64) && scale < 129)
						weight = 64 - scale;
				}
			}
			slice->implicitWeight[i * l1 + j] = weight;
		}
	}
}
