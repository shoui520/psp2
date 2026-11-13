/*
 * The AVC decoder's parser engine thread (0x824ca0-0x825dc8): its memory,
 * the access units it takes, and the VThread jobs it runs on them.
 */
#include "avcdecparser.h"

#define ALIGN64(size)		(((size) + 63) & ~63)
#define ALIGN256(size)		(((size) + 255) & ~255)

/* the parameter cache: the lists' heads and two bytes a node, aligned */
#define CACHE_HEADER		6
#define CACHE_MAX_STREAMS	32
#define CACHE_MAX_FRAMES	256

void avcdecParserAu(uint32_t memory, uint32_t request);
void avcdecParserJobStart(uint32_t info, uint32_t stream, uint32_t job, uint32_t unused);
void avcdecParserJobFlush(uint32_t info, uint32_t stream, uint32_t job, uint32_t unused);
void avcdecParserJobOutput(uint32_t info, uint32_t stream, uint32_t job, uint32_t keepError);
void avcdecDecoderParsed(uint32_t memory, uint32_t parsed);
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
	VkMessageQueue *auQueue = parser->auQueue;
	VkMessageQueue *serviceQueue = parser->serviceQueue;
	VkMessageQueue *decoderQueue = parser->decoderQueue;
	VkMessageQueue *wakeQueue = parser->wakeQueue;
	VkMessageQueue *helperQueue = parser->helperQueue;
	uint32_t flags;

	do {
		flags = parserWait(parser);
		if (parser->state == PARSER_IDLE && vkMessageReceive(auQueue, (uint32_t)parser))
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
				vkMessageSend(serviceQueue, videoServiceAuDone, parser->request);
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
		    vkMessageReceive(wakeQueue, (uint32_t)parser)) {
			if (!parser->output)
				vkMessageSend(helperQueue, videoHelperStart, parser->request);
			else if (parser->flush == 1)
				flushRun(parser);
			else
				outputRun(parser);
			parser->state = PARSER_HELPING;
		}
		if (parser->state == PARSER_HELPING && (flags & PARSER_FLAG_JOB_DONE)) {
			requestFinish(parser);
			vkMessageSend(serviceQueue, videoServiceAuDone, parser->request);
			if (flags & PARSER_FLAG_FAILED) {
				unitSkip(parser);
				parser->state = PARSER_PARSING;
				parser->flush = 1;
			} else {
				if (parser->output) {
					parsedSet(parser);
					vkMessageSend(decoderQueue, avcdecDecoderParsed, parser->parsedWord);
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
	nal->lastDts = 0;
	nal->outputFlush = 0;
	nal->outputIndex = -1;
	nal->outputCount = 0;
	nal->outputMode = 2;
	nal->option5 = info->option5;
	nal->option4 = info->option4;
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
	VkVThreadContext *locked;

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
		stream->option5 = timeOf(request->dts);
		stream->option4 = timeOf(request->pts);
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
	request->reserved = info->id;
	request->thread = info->thread;
	request->queue = parser->decoderQueue;
	request->stream = parser->stream;
	request->contextFlags = 0;
	request->job = parser->job;
	stream->option5 = timeOf(request->dts);
	stream->option4 = timeOf(request->pts);
	stream->option = request->option;
	if (info->fieldDecode)
		request->contextFlags |= VIDEO_ENGINE_CONTEXT_FIELD;
	if (info->contextOption)
		request->contextFlags |= VIDEO_ENGINE_CONTEXT_OPTION;
	if (parser->option)
		request->contextFlags |= 0x08;
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
	vnzPublicClosePrivate((uint32_t)stream, PARSER_STREAM_SIZE);
	locked = vkVThreadContextLock(parser->context);
	vkVThreadJobSet(locked, index, 0, avcdecParserJobStart, (uint32_t)info, parser->stream,
		parser->job, 0);
	vkVThreadContextUnlock(locked);
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
	parser->parsed.pictureInfo[0] = nal->pictureInfo[1];
	parser->parsed.pictureInfo[1] = nal->pictureInfo[0];
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
	nal->outputPictures = 0;
	nal->outputReady = 0;
	nal->outputFlag = 0;
	nal->reorder = 0;
	nal->pictureInfo[1] = 0;
	nal->sliceInfo[0] = 0;
	nal->sliceInfo[1] = 0;
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
		pictures[i].state = 0;
		extra[i * PICTURE_EXTRA_SIZE + 8] = 0;
		pictures[i].user = 0;
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
	VkVThreadContext *locked;

	do {
		locked = vkVThreadContextLock(parser->context);
		busy = vkVThreadJobSetNotify(locked, index, 0, avcdecParserJobOutput, (uint32_t)info,
			parser->stream, parser->job, parser->keepError, info->thread,
			PARSER_FLAG_JOB_DONE);
		vkVThreadContextUnlock(locked);
	} while (busy);
}

/* 0x82564c: the flush job, run until the context takes it */
static void flushRun(AvcdecParser *parser)
{
	AvcdecParserInfo *info = parser->info;
	uint32_t index = info->index << 3, busy;
	VkVThreadContext *locked;

	do {
		locked = vkVThreadContextLock(parser->context);
		busy = vkVThreadJobSetNotify(locked, index, 0, avcdecParserJobFlush, (uint32_t)info,
			parser->stream, parser->job, 0, info->thread, PARSER_FLAG_JOB_DONE);
		vkVThreadContextUnlock(locked);
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
		nal->pictureInfo[1] = 0;
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
