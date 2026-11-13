/*
 * The AVC decoder's decoder engine thread (0x82cd90-0x82f0b8): the runs the
 * helper parsed taken a row at a time, their rows cut into stripes, and the
 * stripes' three jobs (prediction, residual, filter) set on the helper's
 * VThread context, each waiting for the stripes it depends on.
 */
#include "avcdecdecoder.h"

#define ALIGN64(size)		(((size) + 63) & ~63)

void avcdecDecoderParsed(uint32_t memory, uint32_t word);

static AvcdecDecoder *decoderBegin(uint32_t memory);
static uint32_t decoderWait(AvcdecDecoder *decoder);
static void runStart(AvcdecDecoder *decoder, uint32_t jobAddress);
static uint32_t jobClose(AvcdecDecoder *decoder);
static void pictureDone(AvcdecDecoder *decoder);
static void answerPictures(AvcdecDecoder *decoder);
static void outputStart(AvcdecDecoder *decoder, uint32_t word);
static void answerDrain(AvcdecDecoder *decoder);
static void decoderEnd(AvcdecDecoder *decoder);
static void rowsInit(AvcdecDecoder *decoder, AvcdecDecoderInfo *info);
static void rowsStart(AvcdecDecoder *decoder, AvcdecDecoderInfo *info, int32_t lastRow);
static void runRead(AvcdecDecoder *decoder);
static void runDone(AvcdecDecoder *decoder);
static void stripesDone(AvcdecDecoder *decoder);
static uint32_t stripeDone(AvcdecDecoder *decoder, uint32_t jobIndex, uint32_t row);
static void stripeNext(AvcdecRow *entry, uint32_t jobIndex, uint32_t row, uint32_t mode,
	uint32_t width);
static void rowsStartSingle(AvcdecDecoder *decoder, AvcdecDecoderInfo *info, int32_t lastRow);
static void runReadSingle(AvcdecDecoder *decoder);
static void stripesDoneSingle(AvcdecDecoder *decoder);
static void jobBuffersOpen(VideoHelperJob *job);
static void jobReferences(uint32_t info, uint32_t slice, uint32_t pictureStart, uint32_t unused);
static void jobPictureDone(uint32_t info, uint32_t word, uint32_t unused, uint32_t unused2);
static void jobOutput(uint32_t info, uint32_t word, uint32_t unused, uint32_t unused2);
static void jobFlush(uint32_t info, uint32_t word, uint32_t unused, uint32_t unused2);
static void jobRun(uint32_t info, uint32_t run, uint32_t unused, uint32_t unused2);
static void runOpen(const AvcdecDecoderInfo *info, const AvcdecRun *run, AvcdecWork *work);
static void runMacroblocks(const AvcdecDecoderInfo *info, AvcdecRun *run, AvcdecWork *work);
static void runClose(const AvcdecDecoderInfo *info, const AvcdecRun *run, AvcdecWork *work);
static void rowsClear(const AvcdecDecoderInfo *info);

/* a job set on a locked context, the context relocked until it takes it */
static inline VkVThreadContext *jobNotify(VkVThreadContext *context, VkVThreadContext *locked,
	uint32_t index, uint32_t wanted, VkJobFunction function, uint32_t argument,
	uint32_t argument2, uint32_t argument3, VkThread *thread, uint32_t flags)
{
	while (vkVThreadJobSetNotify(locked, index, wanted, function, argument, argument2,
	    argument3, 0, thread, flags)) {
		vkVThreadContextUnlock(locked);
		locked = vkVThreadContextLock(context);
	}
	return locked;
}

/* a stripe's three jobs: its prediction waits for `wanted` signals, the others for it */
static inline void stripeSet(const AvcdecDecoderInfo *info, VkVThreadContext *locked,
	uint32_t index, uint32_t wanted, uint32_t row)
{
	vkVThreadJobSet(locked, index, wanted, avcdecStripePredict, (uint32_t)info, row, 0, 0);
	locked = jobNotify(info->context, locked, info->jobsPerKind + index, 1,
		avcdecStripeResidual, (uint32_t)info, row, 0, info->thread,
		DECODER_FLAG_STRIPE_DONE);
	jobNotify(info->context, locked, info->jobsPerKind * 2 + index, 1, avcdecStripeFilter,
		(uint32_t)info, row, 0, info->thread, DECODER_FLAG_STRIPE_DONE);
}

/* a row's entry in the ring */
static inline uint32_t rowEntry(uint32_t rows, uint32_t row)
{
	return rows + (row & (DECODER_ROWS - 1)) * DECODER_ROW_SIZE;
}

/* 0x82cd90: the engine's thread, until it is stopped */
void avcdecDecoderThread(uint32_t memory, uint32_t codec)
{
	AvcdecDecoder *decoder = decoderBegin(memory);
	VkMessageQueue *parsedQueue = decoder->parsedQueue;
	AvcdecDecoderInfo *info = decoder->info;
	VkMessageQueue *serviceQueue = decoder->serviceQueue;
	VkMessageQueue *decodeQueue = decoder->decodeQueue;
	VkMessageQueue *helperQueue = decoder->helperQueue;
	AvcdecDecoderStatus *status = info->status;
	uint32_t flags;

	do {
		flags = decoderWait(decoder);
		if (decoder->state == DECODER_IDLE &&
		    vkMessageReceive(decodeQueue, (uint32_t)decoder))
			decoder->state = DECODER_WAITING;
		if (decoder->state == DECODER_WAITING &&
		    (!decoder->receive || vkMessageReceive(parsedQueue, (uint32_t)decoder)))
			decoder->state = DECODER_RUNNING;
		if (decoder->state != DECODER_RUNNING)
			continue;
		if (decoder->receive)
			vkMessageReceive(parsedQueue, (uint32_t)decoder);
		if (flags & (DECODER_FLAG_NEXT_RUN | DECODER_FLAG_PICTURE_DONE)) {
			vkMessageSend(helperQueue, videoHelperJob, jobClose(decoder));
			if (decoder->helperWait) {
				vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN);
				decoder->helperWait = 0;
			}
			vnzThreadSetFlags(info->thread, DECODER_FLAG_LOOP);
		}
		if (flags & DECODER_FLAG_RUN_DONE) {
			info = decoder->info;
			runDone(decoder);
		}
		if (flags & DECODER_FLAG_RUN_READ)
			runRead(decoder);
		if (flags & DECODER_FLAG_STRIPE_DONE)
			stripesDone(decoder);
		if (flags & DECODER_FLAG_PICTURE_DONE)
			pictureDone(decoder);
		if (flags & DECODER_FLAG_FLUSHED)
			vnzThreadSetFlags(info->thread, status->outputCount ? DECODER_FLAG_OUTPUT :
				DECODER_FLAG_OUTPUT_NONE);
		if (flags & (DECODER_FLAG_OUTPUT_DONE | DECODER_FLAG_OUTPUT |
		    DECODER_FLAG_OUTPUT_NONE)) {
			if (flags & (DECODER_FLAG_OUTPUT_DONE | DECODER_FLAG_OUTPUT)) {
				answerPictures(decoder);
				vnzThreadSetFlags(info->thread, DECODER_FLAG_ANSWERED);
			} else {
				answerDrain(decoder);
			}
			vkMessageSend(serviceQueue, videoServicePicturesDone, decoder->request);
			decoder->state = DECODER_IDLE;
		}
	} while (decoder->state != DECODER_STOPPED);
	decoderEnd(decoder);
}

/* 0x82cef0: the memory laid out and opened, the frames cleared */
static AvcdecDecoder *decoderBegin(uint32_t memory)
{
	AvcdecDecoder *header = (AvcdecDecoder *)memory, *decoder;
	AvcdecDecoderInfo *info;
	int32_t i;

	header->info = (AvcdecDecoderInfo *)(memory + DECODER_HEADER_SIZE);
	header->status = (AvcdecDecoderStatus *)(memory + DECODER_HEADER_SIZE +
		DECODER_INFO_SIZE);
	decoder = (AvcdecDecoder *)vnzPublicOpenPrivate(memory, DECODER_HEADER_SIZE);
	info = (AvcdecDecoderInfo *)vnzPublicOpenPrivate((uint32_t)decoder->info,
		DECODER_INFO_SIZE);
	info->thread = vnzThreadCurrent();
	info->status = decoder->status;
	vnzPublicClosePrivate((uint32_t)info, DECODER_INFO_SIZE);
	decoder->info = (AvcdecDecoderInfo *)vnzPublicOpenRaw((uint32_t)decoder->info,
		DECODER_INFO_SIZE);
	decoder->state = DECODER_IDLE;
	decoder->receive = 1;
	decoder->status->count = 0;
	for (i = 0; i < DECODER_FRAMES; i++)
		decoder->status->frames[i] = 0;
	return decoder;
}

/* 0x82cf6e: the thread's flags; an exit stops the engine */
static uint32_t decoderWait(AvcdecDecoder *decoder)
{
	uint32_t flags = vnzThreadWaitFlags(DECODER_FLAGS);

	if (flags & DECODER_FLAG_EXIT)
		decoder->state = DECODER_STOPPED;
	return flags;
}

/*
 * 0x82cf94: a message on the parsed queue: the parser's word on a picture
 * (its bit 0 set) or a run the helper parsed.
 */
void avcdecDecoderParsed(uint32_t memory, uint32_t word)
{
	if (word & 1)
		outputStart((AvcdecDecoder *)memory, word);
	else
		runStart((AvcdecDecoder *)memory, word);
}

/*
 * 0x82cfa2: a run parsed: its slice and, for a picture's first run, the
 * picture's command and the helper's context taken; then its first job
 * set, which reads it, and the references' job for a new slice.
 */
static void runStart(AvcdecDecoder *decoder, uint32_t jobAddress)
{
	AvcdecDecoderInfo *info = decoder->info;
	const VideoHelperJob *job;
	const VideoHelperContext *context;
	const VideoAvcDecodeCommand *command;
	const VideoHelperSlice *slice;
	uint32_t first, load, sliceHeader, contextAddress, commandAddress, runAddress, index;
	uint8_t missingReference;
	VkVThreadContext *locked;
	AvcdecRun *run;
	int8_t qp;

	job = (const VideoHelperJob *)vnzPublicOpenProtected(jobAddress, DECODER_JOB_SIZE);
	first = (uint16_t)job->first;
	load = job->load;
	if (load) {
		slice = (const VideoHelperSlice *)vnzPublicOpenProtected(job->slice,
			VIDEO_HELPER_SLICE_SIZE);
		sliceHeader = slice->sliceHeader;
		missingReference = slice->missingReference;
		contextAddress = slice->context;
		qp = (int8_t)slice->qp;
		commandAddress = slice->command;
		decoder->slice = vnzPublicCloseProtected((uint32_t)slice, VIDEO_HELPER_SLICE_SIZE);
		decoder->sliceHeader = sliceHeader;
		decoder->qp = qp;
		decoder->missingReference = missingReference;
		context = (const VideoHelperContext *)vnzPublicOpenProtected(contextAddress,
			DECODER_CONTEXT_SIZE);
		if (!first) {
			info = (AvcdecDecoderInfo *)vnzPublicOpenPrivate(
				vnzPublicCloseRaw((uint32_t)decoder->info, DECODER_INFO_SIZE),
				DECODER_INFO_SIZE);
			info->command = vnzPublicOpenRaw(commandAddress, DECODER_COMMAND_SIZE);
			command = (const VideoAvcDecodeCommand *)info->command;
			decoder->rows = context->table2048;
			decoder->run = context->header64;
			decoder->common = context->common;
			info->context = context->context;
			info->row0 = context->row0;
			info->table1024 = context->table1024;
			info->row1 = context->row1;
			info->row2 = context->row2;
			info->table2048b = context->table2048b;
			info->table1024b = context->table1024b;
			info->table4096 = context->table4096;
			info->table16384 = context->table16384;
			info->table8192 = context->table8192;
			info->mbSize = 16;
			info->mode = command->mode;
			info->widthInMbs = command->widthInMacroblocks;
			info->heightInMbs = command->heightInMacroblocks;
			info->width = command->width;
			info->pitch = command->pitch;
			info->luma = command->address;
			info->chroma = command->address + command->lumaSize;
			if (command->refIdc &&
			    command->lumaSize + command->chromaSize < command->frameSize)
				info->motion = command->address + command->lumaSize +
					command->chromaSize;
			else
				info->motion = 0;
			info->frameSize = command->frameSize;
			info->lumaSize = command->lumaSize;
			info->chromaSize = command->chromaSize;
			info->picOrderCnt = command->picOrderCnt;
			info->picture = command->picture;
			info->decoderData = command->decoderData;
			info->refIdc = command->refIdc;
			info->lockArea = (AvcdecLockArea *)context->lockArea;
			decoder->pictureStart = 1;
			decoder->rowStripes = (int8_t *)vnzPublicOpenPrivate(context->table128,
				DECODER_ROW_STRIPES_SIZE);
			rowsInit(decoder, info);
			info = (AvcdecDecoderInfo *)vnzPublicOpenRaw(
				vnzPublicClosePrivate((uint32_t)info, DECODER_INFO_SIZE),
				DECODER_INFO_SIZE);
		}
		vnzPublicCloseProtected((uint32_t)context, DECODER_CONTEXT_SIZE);
		decoder->info = info;
	}
	if (job->sliceUsed) {
		decoder->outputCount = job->pictureInfo[0];
		decoder->outputStart = job->pictureInfo[1];
	}
	decoder->receive = 0;
	run = (AvcdecRun *)vnzPublicOpenPrivate(decoder->run, DECODER_RUN_SIZE);
	run->firstX = job->firstX;
	run->firstY = job->firstY;
	run->count = job->count;
	run->job = vnzPublicCloseProtected((uint32_t)job, DECODER_JOB_SIZE);
	run->sliceHeader = decoder->sliceHeader;
	run->qp = decoder->qp;
	run->common = decoder->common;
	runAddress = vnzPublicClosePrivate((uint32_t)run, DECODER_RUN_SIZE);

	locked = vkVThreadContextLock(decoder->context);
	index = info->index * 8;
	locked = jobNotify(decoder->context, locked, index + DECODER_JOB_RUN,
		load && first ? 1 : 0, jobRun, (uint32_t)info, runAddress, 0, info->thread,
		DECODER_FLAG_RUN_READ);
	if (load)
		locked = jobNotify(decoder->context, locked, index + DECODER_JOB_REFERENCES, 0,
			jobReferences, (uint32_t)info, decoder->slice, decoder->pictureStart,
			info->thread, DECODER_FLAG_RUN_DONE);
	vkVThreadContextUnlock(locked);
	if (!first)
		rowsStart(decoder, info, info->heightInMbs - 1);
}

/* 0x82d2e4: a decode: its request, answered when its pictures are out */
void avcdecDecoderDecode(uint32_t memory, uint32_t request)
{
	((AvcdecDecoder *)memory)->request = request;
}

/*
 * 0x82d2ea: the run of the row given back is closed for the helper: its
 * buffers closed, the picture's command with its last; the parsed queue is
 * read again when the helper has nothing more of the picture.
 */
static uint32_t jobClose(AvcdecDecoder *decoder)
{
	const AvcdecDecoderInfo *info = decoder->info;
	const AvcdecRow *entry = (const AvcdecRow *)(decoder->rows +
		decoder->jobRow % DECODER_ROWS * DECODER_ROW_SIZE);
	VideoHelperJob *job;
	uint32_t size;

	job = (VideoHelperJob *)vnzPublicOpenPrivate(vnzPublicCloseRaw(entry->job,
		DECODER_JOB_SIZE), DECODER_JOB_SIZE);
	size = job->commandSize;
	job->buffers[0] = vnzPublicCloseRaw(job->buffers[0], size * 16);
	job->buffers[1] = vnzPublicCloseRaw(job->buffers[1], size * 16);
	job->buffers[3] = vnzPublicCloseRaw(job->buffers[3], size * 8);
	job->buffers[4] = vnzPublicCloseRaw(job->buffers[4], size * 128);
	job->buffers[5] = vnzPublicCloseRaw(job->buffers[5], size * 17 * 32);
	job->buffers[6] = vnzPublicCloseRaw(job->buffers[6], size * 17 * 8);
	job->buffers[7] = vnzPublicCloseRaw(job->buffers[7], size * 17 * 8);
	job->buffers[8] = vnzPublicCloseRaw(job->buffers[8], size * 256);
	job->buffers[9] = vnzPublicCloseRaw(job->buffers[9], size * 64);
	job->buffers[10] = vnzPublicCloseRaw(job->buffers[10], size * 64);
	if (job->sliceUsed) {
		if (job->commandUsed)
			vnzPublicCloseRaw(info->command, DECODER_COMMAND_SIZE);
		else if (job->endX || !(info->mode < info->widthInMbs))
			decoder->receive = 1;
	}
	decoder->jobRow = job->endY;
	return vnzPublicClosePrivate((uint32_t)job, DECODER_JOB_SIZE);
}

/* 0x82d42e: the picture is decoded: its frame noted with the parser's pictures */
static void pictureDone(AvcdecDecoder *decoder)
{
	AvcdecDecoderInfo *info = decoder->info;
	union {
		AvcdecParsed parsed;
		uint32_t word;
	} done;
	VkVThreadContext *locked;

	done.parsed.flush = 0;
	done.parsed.valid = 1;
	done.parsed.pictureInfo[0] = decoder->outputCount;
	done.parsed.pictureInfo[1] = decoder->outputStart;
	locked = vkVThreadContextLock(decoder->context);
	locked = jobNotify(decoder->context, locked, info->index * 8 + DECODER_JOB_OUTPUT, 0,
		jobPictureDone, (uint32_t)info, done.word, 0, info->thread,
		DECODER_FLAG_OUTPUT_DONE);
	vkVThreadContextUnlock(locked);
	decoder->rowStripes = (int8_t *)vnzPublicClosePrivate((uint32_t)decoder->rowStripes,
		DECODER_ROW_STRIPES_SIZE);
}

/* 0x82d4c2: the decode answered with the pictures to give out */
static void answerPictures(AvcdecDecoder *decoder)
{
	const AvcdecDecoderStatus *status = decoder->info->status;
	VideoEngineRequest *request;

	request = (VideoEngineRequest *)vnzPublicOpenPrivate(decoder->request, REQUEST_SIZE);
	request->numOfPictures = status->outputCount;
	request->pictures = status->decoderData + status->outputStart * sizeof(VideoAvcFrame);
	request->flags = 0;
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
	decoder->receive = 1;
}

/*
 * 0x82d516: the parser's word on a picture of no slices to decode: its
 * pictures given out, a flush closing the frames when it gives none.
 */
static void outputStart(AvcdecDecoder *decoder, uint32_t word)
{
	AvcdecDecoderInfo *info;
	VkVThreadContext *locked;
	union {
		AvcdecParsed parsed;
		uint32_t word;
	} parsed;

	decoder->receive = 0;
	info = decoder->info;
	parsed.word = word;
	locked = vkVThreadContextLock(decoder->context);
	if (parsed.parsed.flush)
		locked = jobNotify(decoder->context, locked, info->index * 8 + DECODER_JOB_FLUSH, 0,
			jobFlush, (uint32_t)info, word, 0, info->thread, DECODER_FLAG_FLUSHED);
	else
		locked = jobNotify(decoder->context, locked, info->index * 8 + DECODER_JOB_OUTPUT,
			0, jobOutput, (uint32_t)info, word, 0, info->thread,
			DECODER_FLAG_OUTPUT_DONE);
	vkVThreadContextUnlock(locked);
}

/* 0x82d5d0: the decode answered as a drain */
static void answerDrain(AvcdecDecoder *decoder)
{
	const AvcdecDecoderStatus *status = decoder->info->status;
	VideoEngineRequest *request;

	request = (VideoEngineRequest *)vnzPublicOpenPrivate(decoder->request, REQUEST_SIZE);
	request->numOfPictures = status->outputCount;
	request->pictures = status->decoderData + status->outputStart * sizeof(VideoAvcFrame);
	request->flags = VIDEO_ENGINE_DRAIN;
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
	decoder->receive = 1;
}

/* 0x82d624 */
static void decoderEnd(AvcdecDecoder *decoder)
{
	vnzPublicCloseRaw((uint32_t)decoder->info, DECODER_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)decoder, DECODER_HEADER_SIZE);
}

/* 0x82d64a: the engine's memory */
uint32_t avcdecQueryDecoderMemory(void)
{
	return DECODER_FIXED_SIZE;
}

/* 0x82d650: the engine's memory laid out for the decoder's slot */
void avcdecDecoderInitialize(uint32_t memory, VideoService *service, int32_t index)
{
	VideoServiceDecoder *slot = &service->decoders[index];
	AvcdecDecoder *decoder;
	AvcdecDecoderInfo *info;

	decoder = (AvcdecDecoder *)vnzPublicOpenPrivate(memory, DECODER_FIXED_SIZE);
	vnzCacheInvalidateInside((uint32_t)decoder, DECODER_FIXED_SIZE);
	info = (AvcdecDecoderInfo *)((uint32_t)decoder + DECODER_HEADER_SIZE);
	decoder->parsedQueue = slot->slot.innerQueue;
	decoder->helperQueue = service->helperQueue2;
	decoder->decodeQueue = slot->slot.decodeQueue;
	decoder->serviceQueue = service->engineQueue2;
	decoder->context = service->encoderContext;
	info->index = index;
	info->id = slot->slot.id;
	info->parserThread = slot->slot.thread;
	vnzPublicClosePrivate((uint32_t)decoder, DECODER_FIXED_SIZE);
}

/*
 * 0x82d6cc: a picture's rows: its stripes counted, each row's flags set
 * by the stripe of the row two above that ends over it.
 */
static void rowsInit(AvcdecDecoder *decoder, AvcdecDecoderInfo *info)
{
	uint32_t mode = info->mode, width = info->widthInMbs, height = info->heightInMbs;
	uint32_t rest = width % mode, stripes, row, column, flags;
	uint8_t columns[8], *end, *last;
	AvcdecRow *rows;

	if (!(mode < width)) {
		decoder->lastStripe = 0;
		decoder->rowJobs = 1;
		info->jobsPerKind = 4;
	} else {
		stripes = (width + mode - 1) / mode;
		decoder->lastStripe = (rest == 2) + 1;
		decoder->rowJobs = stripes + 1;
		info->jobsPerKind = (stripes + 1) * 4;
	}
	decoder->jobRow = 0;
	decoder->doneRow = 0;
	decoder->signalRow = 1;
	rows = (AvcdecRow *)vnzPublicOpenPrivate(decoder->rows, DECODER_ROWS_SIZE);
	if (mode - 1 >= 1)
		for (column = 0; column < mode - 1; column++)
			columns[column] = ROW_FLAG_ONE_BELOW;
	end = columns + mode;
	switch (rest) {
	case 3:
		end[-4] |= ROW_FLAG_TWO_BELOW | ROW_FLAG_SECOND_ONE;
		end[-2] &= ~ROW_FLAG_ONE_BELOW;
		break;
	case 2:
	case 1:
		end[-3] |= ROW_FLAG_FEWER_BELOW;
		break;
	case 0:
		columns[0] |= ROW_FLAG_TWO_BELOW;
		end[-3] &= ~ROW_FLAG_ONE_BELOW;
		end[-2] |= ROW_FLAG_FEWER_BELOW;
		break;
	default:
		last = columns + mode - rest;
		last[-1] |= ROW_FLAG_TWO_BELOW | ROW_FLAG_SECOND_ONE;
		last[0] |= ROW_FLAG_TWO_BELOW;
		end[-2] &= ~ROW_FLAG_ONE_BELOW;
		end[-3] &= ~ROW_FLAG_ONE_BELOW;
		break;
	}
	for (row = 0, column = 0; row < height; row++) {
		decoder->rowStripes[row] = 0;
		if (row == height - 1)
			flags = ROW_FLAG_LAST;
		else if (row < height - 2)
			flags = columns[column] | ROW_FLAG_START_BELOW;
		else
			flags = 0;
		if (column == mode - 2) {
			flags |= ROW_FLAG_STRIPE_END;
			column = 0;
		} else {
			column++;
		}
		rows[row & (DECODER_ROWS - 1)].flags[row >> 5] = flags;
	}
	decoder->row = 0;
	decoder->waitingRows = 0;
	vnzPublicClosePrivate((uint32_t)rows, DECODER_ROWS_SIZE);
	info->lockArea->doneRows = 0;
	info->lockArea->pending = 0;
}

/*
 * 0x82d86c: a picture's first two rows' stripes set: the first row's
 * stripes each wait for the one before, the second's for the two above too.
 */
static void rowsStart(AvcdecDecoder *decoder, AvcdecDecoderInfo *info, int32_t lastRow)
{
	uint32_t mode = info->mode, width = info->widthInMbs, last = decoder->lastStripe;
	uint32_t column, index, rows;
	VkVThreadContext *locked;

	if (!(mode < width)) {
		rowsStartSingle(decoder, info, lastRow);
		return;
	}
	locked = vkVThreadContextLock(info->context);
	rows = decoder->rows;
	stripeSet(info, locked, 0, 2, rows);
	for (column = mode, index = 1; column < width; column += mode, index++)
		stripeSet(info, locked, index, 1, rows);
	if (lastRow >= 1) {
		index = decoder->rowJobs;
		rows = decoder->rows + DECODER_ROW_SIZE;
		stripeSet(info, locked, index, 2, rows);
		index++;
		for (column = mode - 1; column < width - mode; column += mode, index++)
			stripeSet(info, locked, index, 2, rows);
		stripeSet(info, locked, index, width - column == last ? 1 : 2, rows);
	}
	vkVThreadContextUnlock(locked);
}

/*
 * 0x82dc76: the run's first job read it: its rows' entries written, a
 * stripe each, and the stripes of rows already started signalled.
 */
static void runRead(AvcdecDecoder *decoder)
{
	uint32_t stripes = decoder->rowJobs, rows = decoder->rows, row = decoder->row;
	AvcdecDecoderInfo *info = decoder->info;
	uint32_t width = info->widthInMbs, firstX, endX, endY, failed, index;
	VkVThreadContext *locked;
	VideoHelperJob *job;
	AvcdecRow *entry;

	if (!(info->mode < width)) {
		runReadSingle(decoder);
		return;
	}
	job = (VideoHelperJob *)vnzPublicOpenPrivate(((const AvcdecRun *)decoder->run)->job,
		DECODER_JOB_SIZE);
	if (job->load) {
		decoder->helperWait = 0;
		decoder->runEnded = 0;
	}
	jobBuffersOpen(job);
	job = (VideoHelperJob *)vnzPublicOpenRaw(vnzPublicClosePrivate((uint32_t)job,
		DECODER_JOB_SIZE), DECODER_JOB_SIZE);
	endX = job->endX;
	firstX = job->firstX;
	endY = job->endY;
	failed = decoder->missingReference | job->failed;
	if (firstX) {
		/* the run goes on in the row's stripe that has the macroblocks */
		index = decoder->rowStripes[row] + (row & 3) * stripes;
		entry = (AvcdecRow *)vnzPublicOpenPrivate(rowEntry(rows, row), DECODER_ROW_SIZE);
		entry->x = firstX;
		entry->row = row;
		entry->job = (uint32_t)job;
		if (entry->leftover) {
			entry->count = entry->leftover;
		} else {
			entry->fromStart = 0;
			entry->count = info->mode;
		}
		entry->leftover = 0;
		entry->runEnd = 0;
		entry->reserved = 0;
		entry->slice = decoder->slice;
		entry->sliceHeader = decoder->sliceHeader;
		entry->jobIndex = index;
		entry->failed = failed;
		entry->common = decoder->common;
		if (row == endY && !(entry->count + firstX < endX)) {
			entry->runEnd = 1;
			entry->leftover = entry->count - endX + firstX;
			entry->count = endX - firstX;
		}
		if (!(entry->count + firstX < width))
			entry->count = width - entry->x;
		vnzPublicClosePrivate((uint32_t)entry, DECODER_ROW_SIZE);
		locked = vkVThreadContextLock(info->context);
		vkVThreadJobSignal(locked, index);
		vkVThreadContextUnlock(locked);
	} else {
		/* the run's rows from their starts: their first stripes */
		for (; row < endY + (endX > 0); row++) {
			index = (row & 3) * stripes;
			entry = (AvcdecRow *)vnzPublicOpenPrivate(rowEntry(rows, row),
				DECODER_ROW_SIZE);
			entry->row = row;
			entry->x = 0;
			entry->leftover = 0;
			entry->runEnd = 0;
			entry->reserved = 0;
			entry->job = (uint32_t)job;
			entry->count = info->mode - row % (info->mode - 1);
			entry->fromStart = 1;
			entry->slice = decoder->slice;
			entry->sliceHeader = decoder->sliceHeader;
			entry->jobIndex = index;
			entry->failed = failed;
			entry->common = decoder->common;
			if (row == endY && !(entry->count < endX)) {
				entry->runEnd = 1;
				entry->leftover = entry->count - endX;
				entry->count = endX;
			}
			vnzPublicClosePrivate((uint32_t)entry, DECODER_ROW_SIZE);
			if (!(decoder->signalRow < row)) {
				locked = vkVThreadContextLock(info->context);
				vkVThreadJobSignal(locked, index);
				vkVThreadContextUnlock(locked);
			}
		}
	}
	decoder->row = endY;
	decoder->lastRow = endX ? endY : endY - 1;
	if (!endX && info->heightInMbs != endY)
		decoder->receive = 1;
}

/*
 * 0x82ded6: the helper's run is decoded: the run's job may go on, or, for
 * a picture's first run, the picture's first stripe.
 */
static void runDone(AvcdecDecoder *decoder)
{
	const AvcdecDecoderInfo *info = decoder->info;
	VkVThreadContext *locked;

	if (!decoder->pictureStart) {
		locked = vkVThreadContextLock(decoder->context);
		vkVThreadJobSignal(locked, info->index * 8 + DECODER_JOB_RUN);
		vkVThreadContextUnlock(locked);
	} else {
		locked = vkVThreadContextLock(info->context);
		vkVThreadJobSignal(locked, 0);
		vkVThreadContextUnlock(locked);
		decoder->pictureStart = 0;
	}
}

/*
 * 0x82df30: stripes are done: the rows done since the last look, up to 15
 * of them, each moved on to its next stripe.
 */
static void stripesDone(AvcdecDecoder *decoder)
{
	AvcdecDecoderInfo *info = decoder->info;
	uint32_t stripes = decoder->rowJobs, height = info->heightInMbs, done, row, limit, bit;
	AvcdecLockArea *lockArea = info->lockArea;
	int32_t started, previous;

	if (!(info->mode < info->widthInMbs)) {
		stripesDoneSingle(decoder);
		return;
	}
	previous = stripes;
	done = lockArea->doneRows;
	row = decoder->doneRow;
	limit = row + 15 < height ? row + 15 : height;
	for (; row < limit; row++) {
		started = decoder->rowStripes[row];
		if (!previous && !started)
			return;
		if (previous != started) {
			bit = 1 << (row & 31);
			if (done & bit) {
				vnzSpinLock(lockArea->lock);
				lockArea->doneRows &= ~bit;
				vnzSpinUnlock(lockArea->lock);
				if (stripeDone(decoder, (row & 3) * stripes + started, row))
					return;
			}
			previous = started;
		}
	}
}

/*
 * 0x82dfe8: a row's stripe is done: the row's next stripe set up, the
 * stripes it starts (its next, the row below's, the one two below's)
 * signalled, the helper told when the row or the run is done. Returns
 * whether the engine thread was flagged.
 */
static uint32_t stripeDone(AvcdecDecoder *decoder, uint32_t jobIndex, uint32_t row)
{
	uint32_t rows = decoder->rows, x, count, end, flags, width, mode, stripes, belowIndex;
	AvcdecDecoderInfo *info = decoder->info;
	int32_t next, self, limit, started, below;
	uint32_t leftover, fromStart, flagged, entryAddress, belowRow, wanted, bit;
	VkVThreadContext *locked;
	const VideoHelperJob *job;
	AvcdecRow *entry;

	width = info->widthInMbs;
	entry = (AvcdecRow *)vnzPublicOpenPrivate(rowEntry(rows, row), DECODER_ROW_SIZE);
	leftover = entry->leftover;
	x = entry->x;
	fromStart = entry->fromStart;
	stripes = decoder->rowJobs;
	count = entry->count;
	flags = entry->flags[row >> 5];
	mode = info->mode;
	started = decoder->rowStripes[row];
	if (flags & ROW_FLAG_LAST) {
		next = -1;
	} else {
		next = started + ((row + 1) & 3) * stripes;
		if (flags & ROW_FLAG_STRIPE_END) {
			if (x + count == 2)
				next = -1;
			else
				next--;
		}
	}
	flagged = 0;
	end = x + count;
	if (!(end < width)) {
		/* the row is done */
		if (flags & ROW_FLAG_LAST) {
			vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN |
				DECODER_FLAG_PICTURE_DONE);
			flagged = 1;
		} else {
			job = (const VideoHelperJob *)entry->job;
			if (decoder->runEnded && decoder->runEndRow - 1 == row) {
				vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN);
				flagged = 1;
				if (!job->endX)
					decoder->helperWait = 1;
			} else if (job->endY - 1 == row && !job->endX) {
				vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN |
					DECODER_FLAG_STRIPE_DONE);
				flagged = 1;
			}
		}
		decoder->doneRow++;
		self = -1;
	} else if (!entry->runEnd) {
		/* the row's next stripe, once the row two above is far enough */
		stripeNext(entry, jobIndex, row, mode, width);
		self = jobIndex + 1;
		if (row < 2 || decoder->rowStripes[row - 2] == stripes)
			limit = self + 1;
		else
			limit = decoder->rowStripes[row - 2] + (row & 3) * stripes;
		bit = 1 << (row & 31);
		if (self < limit) {
			decoder->waitingRows &= ~bit;
		} else {
			decoder->waitingRows |= bit;
			self = -1;
		}
	} else {
		/* the run ends in the stripe: the helper takes the next run */
		if (leftover)
			next = -1;
		decoder->runEnded = 1;
		self = -1;
		decoder->runEndRow = row;
		if (row == 0 || decoder->doneRow == row) {
			vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN);
			flagged = 1;
		}
	}
	entryAddress = vnzPublicClosePrivate((uint32_t)entry, DECODER_ROW_SIZE);
	if (leftover) {
		/* the stripe again, with the macroblocks of the next run */
		locked = vkVThreadContextLock(info->context);
		stripeSet(info, locked, jobIndex, 1, entryAddress);
		vkVThreadContextUnlock(locked);
		decoder->rowStripes[row]--;
		flags &= ~ROW_FLAG_START_BELOW;
	}
	if (flags & ROW_FLAG_START_BELOW) {
		/* the stripes of the row two below that this one starts */
		belowRow = rowEntry(rows, row + 2);
		bit = 1 << ((row + 2) & 31);
		wanted = !(decoder->waitingRows & bit) + 1;
		decoder->waitingRows &= ~bit;
		below = started + ((row + 2) & 3) * stripes;
		locked = vkVThreadContextLock(info->context);
		if (!(end < width)) {
			if (flags & ROW_FLAG_TWO_BELOW) {
				stripeSet(info, locked, below, wanted, belowRow);
				belowIndex = below + 1;
				stripeSet(info, locked, belowIndex,
					flags & ROW_FLAG_SECOND_ONE ? 1 : 2, belowRow);
			} else if (flags & ROW_FLAG_ONE_BELOW) {
				if (flags & ROW_FLAG_FEWER_BELOW)
					wanted--;
				stripeSet(info, locked, below, wanted, belowRow);
			}
		} else {
			stripeSet(info, locked, below, wanted, belowRow);
		}
		if (!(row + 2 < decoder->signalRow))
			decoder->signalRow = row + 2;
		if (fromStart && !(decoder->lastRow < row + 2))
			vkVThreadJobSignal(locked, below);
		vkVThreadContextUnlock(locked);
	}
	locked = vkVThreadContextLock(info->context);
	if (self >= 0)
		vkVThreadJobSignal(locked, self);
	if (next >= 0)
		vkVThreadJobSignal(locked, next);
	vkVThreadContextUnlock(locked);
	if (!(end < width))
		decoder->rowStripes[row] = stripes;
	else
		decoder->rowStripes[row]++;
	return flagged;
}

/* 0x82e5e2: a row's entry moved on to its next stripe */
static void stripeNext(AvcdecRow *entry, uint32_t jobIndex, uint32_t row, uint32_t mode,
	uint32_t width)
{
	const VideoHelperJob *job = (const VideoHelperJob *)entry->job;
	uint32_t endY = job->endY, endX = job->endX, count;

	entry->leftover = 0;
	entry->fromStart = 0;
	entry->runEnd = 0;
	entry->jobIndex = jobIndex + 1;
	entry->x += entry->count;
	count = mode;
	if (!(entry->x + mode < width))
		count = width - entry->x;
	entry->count = count;
	if (endX && row == endY && !(entry->x + entry->count < endX)) {
		entry->runEnd = 1;
		entry->leftover = entry->x + entry->count - endX;
		entry->count = endX - entry->x;
	}
}

/* 0x82e65a: rowsStart for a picture of one stripe a row */
static void rowsStartSingle(AvcdecDecoder *decoder, AvcdecDecoderInfo *info, int32_t lastRow)
{
	VkVThreadContext *locked = vkVThreadContextLock(info->context);

	stripeSet(info, locked, 0, 2, decoder->rows);
	if (lastRow >= 1)
		stripeSet(info, locked, 1, 2, decoder->rows + DECODER_ROW_SIZE);
	vkVThreadContextUnlock(locked);
}

/* 0x82e7bc: runRead for a picture of one stripe a row */
static void runReadSingle(AvcdecDecoder *decoder)
{
	uint32_t rows = decoder->rows, row = decoder->row, index;
	AvcdecDecoderInfo *info = decoder->info;
	uint32_t width = info->widthInMbs, firstX, endX, endY, failed;
	VkVThreadContext *locked;
	VideoHelperJob *job;
	AvcdecRow *entry;

	job = (VideoHelperJob *)vnzPublicOpenPrivate(((const AvcdecRun *)decoder->run)->job,
		DECODER_JOB_SIZE);
	if (job->load) {
		decoder->helperWait = 0;
		decoder->runEnded = 0;
	}
	jobBuffersOpen(job);
	job = (VideoHelperJob *)vnzPublicOpenRaw(vnzPublicClosePrivate((uint32_t)job,
		DECODER_JOB_SIZE), DECODER_JOB_SIZE);
	endX = job->endX;
	firstX = job->firstX;
	endY = job->endY;
	failed = decoder->missingReference | job->failed;
	if (firstX) {
		index = row & 3;
		entry = (AvcdecRow *)vnzPublicOpenPrivate(rowEntry(rows, row), DECODER_ROW_SIZE);
		entry->job = (uint32_t)job;
		entry->row = row;
		entry->x = firstX;
		entry->count = width - firstX;
		entry->runEnd = 0;
		entry->reserved = 0;
		entry->slice = decoder->slice;
		entry->sliceHeader = decoder->sliceHeader;
		entry->jobIndex = index;
		entry->failed = failed;
		entry->common = decoder->common;
		if (row == endY && !(entry->count + firstX < endX)) {
			entry->runEnd = 1;
			entry->count = endX - firstX;
		}
		vnzPublicClosePrivate((uint32_t)entry, DECODER_ROW_SIZE);
		if (!(decoder->signalRow < row)) {
			locked = vkVThreadContextLock(info->context);
			vkVThreadJobSignal(locked, index);
			vkVThreadContextUnlock(locked);
		}
	} else {
		for (; row < endY + (endX > 0); row++) {
			index = row & 3;
			entry = (AvcdecRow *)vnzPublicOpenPrivate(rowEntry(rows, row),
				DECODER_ROW_SIZE);
			entry->runEnd = 0;
			entry->reserved = 0;
			entry->job = (uint32_t)job;
			entry->row = row;
			entry->x = 0;
			entry->count = width;
			entry->slice = decoder->slice;
			entry->sliceHeader = decoder->sliceHeader;
			entry->jobIndex = index;
			entry->failed = failed;
			entry->common = decoder->common;
			if (row == endY && !(entry->count < endX)) {
				entry->count = endX;
				entry->runEnd = 1;
			}
			vnzPublicClosePrivate((uint32_t)entry, DECODER_ROW_SIZE);
			if (!(decoder->signalRow < row)) {
				locked = vkVThreadContextLock(info->context);
				vkVThreadJobSignal(locked, index);
				vkVThreadContextUnlock(locked);
			}
		}
	}
	decoder->row = endY;
	decoder->lastRow = endX ? endY : endY - 1;
	if (!job->sliceUsed)
		decoder->receive = 1;
}

/*
 * 0x82e996: a stripe is done in a picture of one stripe a row: the row
 * done once its run's macroblocks are, the rows below started.
 */
static void stripesDoneSingle(AvcdecDecoder *decoder)
{
	uint32_t rows = decoder->rows, row = decoder->doneRow, width, height, bit, index;
	AvcdecDecoderInfo *info = decoder->info;
	AvcdecLockArea *lockArea = info->lockArea;
	uint32_t count, x, endY, endX;
	const AvcdecRow *entry;
	VkVThreadContext *locked;

	width = info->widthInMbs;
	height = info->heightInMbs;
	if (!(row < height))
		return;
	bit = 1 << (row & 31);
	if (!(lockArea->doneRows & bit))
		return;
	entry = (const AvcdecRow *)rowEntry(rows, row);
	count = entry->count;
	x = entry->x;
	endY = ((const VideoHelperJob *)entry->job)->endY;
	endX = ((const VideoHelperJob *)entry->job)->endX;
	vnzSpinLock(lockArea->lock);
	lockArea->doneRows &= ~bit;
	vnzSpinUnlock(lockArea->lock);
	if (x + count < width) {
		/* the run ended in the row: the row again with the next run */
		index = row & 3;
		locked = vkVThreadContextLock(info->context);
		stripeSet(info, locked, index, 1, (uint32_t)entry);
		vkVThreadContextUnlock(locked);
		vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN);
		return;
	}
	decoder->doneRow++;
	decoder->rowStripes[row]++;
	if (row == height - 1) {
		vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN | DECODER_FLAG_PICTURE_DONE);
		return;
	}
	locked = vkVThreadContextLock(info->context);
	if (row < height - 2) {
		index = (row + 2) & 3;
		stripeSet(info, locked, index, 2, rowEntry(rows, row + 2));
		decoder->signalRow = row + 2;
		if (!(decoder->lastRow < row + 2))
			vkVThreadJobSignal(locked, index);
	}
	vkVThreadJobSignal(locked, (row + 1) & 3);
	vkVThreadContextUnlock(locked);
	if (endY - 1 == row && !endX)
		vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN);
}

/* 0x82ebee: a run's buffers opened raw for the stripes' jobs (its chroma modes aside) */
static void jobBuffersOpen(VideoHelperJob *job)
{
	uint32_t size = job->commandSize;

	job->buffers[0] = vnzPublicOpenRaw(job->buffers[0], size * 16);
	job->buffers[1] = vnzPublicOpenRaw(job->buffers[1], size * 16);
	job->buffers[3] = vnzPublicOpenRaw(job->buffers[3], size * 8);
	job->buffers[4] = vnzPublicOpenRaw(job->buffers[4], size * 128);
	job->buffers[5] = vnzPublicOpenRaw(job->buffers[5], size * 17 * 32);
	job->buffers[6] = vnzPublicOpenRaw(job->buffers[6], size * 17 * 8);
	job->buffers[7] = vnzPublicOpenRaw(job->buffers[7], size * 17 * 8);
	job->buffers[8] = vnzPublicOpenRaw(job->buffers[8], size * 256);
	job->buffers[9] = vnzPublicOpenRaw(job->buffers[9], size * 64);
	job->buffers[10] = vnzPublicOpenRaw(job->buffers[10], size * 64);
}

/*
 * 0x82eca0, a job: a slice's references: the frame of each picture in its
 * lists (a missing one takes the first there is), list 1's first's motion;
 * a new picture's frame closed first if it was still open.
 */
static void jobReferences(uint32_t infoAddress, uint32_t sliceAddress, uint32_t pictureStart,
	uint32_t unused)
{
	const AvcdecDecoderInfo *info = (const AvcdecDecoderInfo *)infoAddress;
	AvcdecDecoderStatus *status;
	uint32_t frameSize, lumaSize, first, frame, *previous;
	VideoHelperSlice *slice;
	int32_t i, count, picture;

	status = (AvcdecDecoderStatus *)vnzPublicOpenPrivate((uint32_t)info->status,
		DECODER_STATUS_SIZE);
	slice = (VideoHelperSlice *)vnzPublicOpenPrivate(sliceAddress, VIDEO_HELPER_SLICE_SIZE);
	frameSize = info->frameSize;
	if (pictureStart) {
		previous = &status->frames[info->picture];
		if (*previous) {
			vnzPublicCloseRaw(*previous, frameSize);
			*previous = 0;
		}
	}
	if ((slice->sliceHeader & AVC_SLICE_TYPE) != AVC_SLICE_I) {
		lumaSize = info->lumaSize;
		count = (uint8_t)slice->numRefIdxActive[0];
		first = 0;
		for (i = 0; i < count; i++) {
			first = status->frames[slice->refList[0][i + 1]];
			if (first)
				break;
		}
		for (i = 0; i < count; i++) {
			picture = slice->refList[0][i + 1];
			if (picture < 0 || !(frame = status->frames[picture]))
				frame = first;
			slice->refLuma[0][i] = frame;
			slice->refChroma[0][i] = frame + lumaSize;
		}
		if ((slice->sliceHeader & AVC_SLICE_TYPE) == AVC_SLICE_B) {
			count = (uint8_t)slice->numRefIdxActive[1];
			if (!first) {
				for (i = 0; i < count; i++) {
					first = status->frames[slice->refList[1][i + 1]];
					if (first)
						break;
				}
			}
			for (i = 0; i < count; i++) {
				picture = slice->refList[1][i + 1];
				if (picture < 0 || !(frame = status->frames[picture]))
					frame = first;
				slice->refLuma[1][i] = frame;
				slice->refChroma[1][i] = frame + lumaSize;
			}
			slice->colocated = slice->refChroma[1][0] + info->chromaSize;
		}
	}
	vnzPublicClosePrivate((uint32_t)slice, VIDEO_HELPER_SLICE_SIZE);
	vnzPublicClosePrivate((uint32_t)status, DECODER_STATUS_SIZE);
}

/*
 * 0x82edd2, a job: the picture is decoded: its frame written back and kept
 * open as a reference, the parser's pictures to give out noted.
 */
static void jobPictureDone(uint32_t infoAddress, uint32_t word, uint32_t unused,
	uint32_t unused2)
{
	const AvcdecDecoderInfo *info = (const AvcdecDecoderInfo *)infoAddress;
	uint32_t luma = info->luma, size = info->chromaSize + info->lumaSize;
	AvcdecDecoderStatus *status;
	union {
		AvcdecParsed parsed;
		uint32_t word;
	} parsed;

	status = (AvcdecDecoderStatus *)vnzPublicOpenPrivate((uint32_t)info->status,
		DECODER_STATUS_SIZE);
	vnzL2Writeback(luma, size);
	vnzL2Invalidate(luma + size - 256, 256);
	status->frames[info->picture] = vnzPublicOpenRaw(luma, info->frameSize);
	parsed.word = word;
	status->decoderData = info->decoderData;
	status->outputCount = parsed.parsed.pictureInfo[0];
	status->outputStart = parsed.parsed.pictureInfo[1];
	status->count++;
	vnzPublicClosePrivate((uint32_t)status, DECODER_STATUS_SIZE);
}

/* 0x82ee5c, a job: the parser's pictures to give out noted */
static void jobOutput(uint32_t infoAddress, uint32_t word, uint32_t unused, uint32_t unused2)
{
	const AvcdecDecoderInfo *info = (const AvcdecDecoderInfo *)infoAddress;
	AvcdecDecoderStatus *status;
	union {
		AvcdecParsed parsed;
		uint32_t word;
	} parsed;

	status = (AvcdecDecoderStatus *)vnzPublicOpenPrivate((uint32_t)info->status,
		DECODER_STATUS_SIZE);
	parsed.word = word;
	status->outputCount = parsed.parsed.pictureInfo[0];
	status->outputStart = parsed.parsed.pictureInfo[1];
	vnzPublicClosePrivate((uint32_t)status, DECODER_STATUS_SIZE);
}

/* 0x82ee96, a job: a flush: the pictures noted, the frames closed when none is left */
static void jobFlush(uint32_t infoAddress, uint32_t word, uint32_t unused, uint32_t unused2)
{
	const AvcdecDecoderInfo *info = (const AvcdecDecoderInfo *)infoAddress;
	AvcdecDecoderStatus *status;
	uint32_t frameSize, *frame;
	int32_t i;
	union {
		AvcdecParsed parsed;
		uint32_t word;
	} parsed;

	status = (AvcdecDecoderStatus *)vnzPublicOpenPrivate((uint32_t)info->status,
		DECODER_STATUS_SIZE);
	parsed.word = word;
	status->outputCount = parsed.parsed.pictureInfo[0];
	status->outputStart = parsed.parsed.pictureInfo[1];
	if (!parsed.parsed.pictureInfo[0]) {
		frameSize = info->frameSize;
		for (i = 0; i < DECODER_FRAMES; i++) {
			frame = &status->frames[i];
			if (*frame) {
				vnzPublicCloseRaw(*frame, frameSize);
				*frame = 0;
			}
		}
	}
	vnzPublicClosePrivate((uint32_t)status, DECODER_STATUS_SIZE);
}

/* this core's work area, by its id */
static inline uint32_t workSlot(void)
{
	return ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 7;
}

/* 0x82ef00, a job: the run read, on this core's work area */
static void jobRun(uint32_t infoAddress, uint32_t runAddress, uint32_t unused, uint32_t unused2)
{
	const AvcdecDecoderInfo *info = (const AvcdecDecoderInfo *)infoAddress;
	uint32_t slot = workSlot();
	AvcdecRun *run;
	AvcdecWork *work;

	run = (AvcdecRun *)vnzPublicOpenPrivate(runAddress, DECODER_RUN_SIZE);
	work = (AvcdecWork *)vnzPublicOpenPrivate(run->common + slot * DECODER_WORK_STRIDE,
		DECODER_WORK_SIZE);
	vnzCacheInvalidateInside((uint32_t)work, DECODER_WORK_SIZE);
	runOpen(info, run, work);
	runMacroblocks(info, run, work);
	runClose(info, run, work);
	vnzCachePurge((uint32_t)work, DECODER_WORK_SIZE);
	vnzPublicClosePrivate((uint32_t)work, DECODER_WORK_SIZE);
	vnzPublicClosePrivate((uint32_t)run, DECODER_RUN_SIZE);
}

/* 0x82ef78: the run's job and its buffers opened on the work area */
static void runOpen(const AvcdecDecoderInfo *info, const AvcdecRun *run, AvcdecWork *work)
{
	const VideoHelperJob *job;
	VideoAvcMbInfo *mbInfo;
	uint32_t size;

	job = (const VideoHelperJob *)vnzPublicOpenProtected(run->job, DECODER_JOB_SIZE);
	size = job->commandSize;
	mbInfo = (VideoAvcMbInfo *)vnzPublicOpenPrivate(job->buffers[0], ALIGN64(size * 16));
	work->chromaModes = (const uint8_t *)vnzPublicOpenProtected(job->buffers[2],
		ALIGN64(size));
	work->job = job;
	work->mbInfo = mbInfo;
	work->firstX = run->firstX;
	work->firstY = run->firstY;
	work->sliceHeader = run->sliceHeader;
	work->ended = job->ended;
	if (job->load) {
		mbInfo->qpPrevious = run->qp;
		if (!job->first)
			rowsClear(info);
	} else {
		mbInfo->qpPrevious = run->lastQp;
	}
}

/* 0x82f012 */
static void runClose(const AvcdecDecoderInfo *info, const AvcdecRun *run, AvcdecWork *work)
{
	const VideoHelperJob *job = work->job;
	uint32_t size = job->commandSize;

	vnzPublicClosePrivate((uint32_t)work->mbInfo, ALIGN64(size * 16));
	vnzPublicCloseProtected((uint32_t)work->chromaModes, ALIGN64(size));
	vnzPublicCloseProtected((uint32_t)job, DECODER_JOB_SIZE);
}

/* 0x82f05e: a picture's first run: the row above its first row cleared */
static void rowsClear(const AvcdecDecoderInfo *info)
{
	uint32_t width = info->widthInMbs, size = ALIGN64((width + 1) * sizeof(AvcdecTop)), i;
	AvcdecTop *top;

	top = (AvcdecTop *)vnzPublicOpenPrivate(info->row0, size);
	for (i = 0; i <= width; i++) {
		top[i].available = 0;
		top[i].refIdx.w[0] = -1;
		top[i].refIdx.w[1] = -1;
	}
	vnzPublicClosePrivate((uint32_t)top, size);
}

/*
 * The macroblocks' kinds by mb_type (AVC_MB_*): the partitions, their
 * lists, intra, Intra16x16PredMode and the luma and chroma CBP of the
 * Intra16x16 types; the sub-macroblocks' by sub_mb_type.
 */
static const uint32_t mbKindsI[26] = {			/* 0x8f70ec */
	0x00000203, 0x00000243, 0x00001243, 0x00002243, 0x00003243, 0x00100243,
	0x00101243, 0x00102243, 0x00103243, 0x00200243, 0x00201243, 0x00202243,
	0x00203243, 0x000f0243, 0x000f1243, 0x000f2243, 0x000f3243, 0x001f0243,
	0x001f1243, 0x001f2243, 0x001f3243, 0x002f0243, 0x002f1243, 0x002f2243,
	0x002f3243, 0x00000283,
};

static const uint32_t mbKindsP[31] = {			/* 0x8f7154 */
	0x00000053, 0x00000056, 0x00000055, 0x00000058, 0x00000458, 0x00000203,
	0x00000243, 0x00001243, 0x00002243, 0x00003243, 0x00100243, 0x00101243,
	0x00102243, 0x00103243, 0x00200243, 0x00201243, 0x00202243, 0x00203243,
	0x000f0243, 0x000f1243, 0x000f2243, 0x000f3243, 0x001f0243, 0x001f1243,
	0x001f2243, 0x001f3243, 0x002f0243, 0x002f1243, 0x002f2243, 0x002f3243,
	0x00000283,
};

static const uint32_t mbKindsB[49] = {			/* 0x8f71d0 */
	0x00000108, 0x00000153, 0x000001a3, 0x000001f3, 0x00000156, 0x00000155,
	0x000001a6, 0x000001a5, 0x00000166, 0x00000165, 0x00000196, 0x00000195,
	0x00000176, 0x00000175, 0x000001b6, 0x000001b5, 0x000001d6, 0x000001d5,
	0x000001e6, 0x000001e5, 0x000001f6, 0x000001f5, 0x000001f8, 0x00000203,
	0x00000243, 0x00001243, 0x00002243, 0x00003243, 0x00100243, 0x00101243,
	0x00102243, 0x00103243, 0x00200243, 0x00201243, 0x00202243, 0x00203243,
	0x000f0243, 0x000f1243, 0x000f2243, 0x000f3243, 0x001f0243, 0x001f1243,
	0x001f2243, 0x001f3243, 0x002f0243, 0x002f1243, 0x002f2243, 0x002f3243,
	0x00000283,
};

static const int8_t subMbKindsP[4] = { 19, 22, 21, 24 };	/* 0x8f7294 */

/* B_Direct_8x8 as four 4x4 blocks, then as one 8x8 block (direct_8x8_inference) */
static const int8_t subMbKindsB[2][13] = {			/* 0x8f7298 */
	{ 72, 83, 99, 115, 86, 85, 102, 101, 118, 117, 88, 104, 120 },
	{ 67, 83, 99, 115, 86, 85, 102, 101, 118, 117, 88, 104, 120 },
};

#define SKIP_KIND_P		0x00000843	/* P_Skip: P_L0_16x16, skipped */
#define SKIP_KIND_B		0x00000908	/* B_Skip: B_Direct_16x16, skipped */
#define SKIP_SUB_KIND_B		72		/* its B_Direct_8x8 */
#define SKIP_SUB_KIND_B_8X8	67
#define FILL_KIND_I		0x03002203	/* concealed: intra DC, kept for its neighbours */

static uint32_t kindI(VideoAvcMbInfo *mb);
static uint32_t kindP(VideoAvcMbInfo *mb);
static uint32_t skipP(VideoAvcMbInfo *mb);
static uint32_t kindB(uint32_t sliceHeader, VideoAvcMbInfo *mb);
static uint32_t skipB(uint32_t sliceHeader, VideoAvcMbInfo *mb);
static VideoAvcMbInfo *runParse(uint32_t width, uint32_t height, AvcdecRun *run,
	AvcdecWork *work);
static VideoAvcMbInfo *runFill(uint32_t width, uint32_t height, AvcdecRun *run,
	AvcdecWork *work, int32_t index);
static void mbQp(VideoAvcMbInfo *mb, AvcdecWork *work, int32_t count, int32_t index);
static void mbFill(VideoAvcMbInfo *mb, uint32_t sliceHeader);
static const uint8_t *mbRead(VideoAvcMbInfo *mb, const uint8_t *chromaModes,
	uint32_t sliceHeader);
static void mbIntraNeighbours(VideoAvcMbInfo *mb, VideoAvcMbInfo *above, int32_t width,
	int32_t x, int32_t y);

/* 0x82f0b8: a macroblock's kind, its sub-macroblocks' converted; AVC_MB_INVALID */
static uint32_t mbKind(uint32_t sliceHeader, VideoAvcMbInfo *mb)
{
	uint32_t type = sliceHeader & AVC_SLICE_TYPE;

	if (type == AVC_SLICE_I)
		return kindI(mb);
	if (type == AVC_SLICE_B)
		return mb->skipped ? skipB(sliceHeader, mb) : kindB(sliceHeader, mb);
	return mb->skipped ? skipP(mb) : kindP(mb);
}

/* 0x82f0e4 */
static uint32_t kindI(VideoAvcMbInfo *mb)
{
	int32_t type = mb->mbType & 0x3f;

	if (type < 26)
		return mbKindsI[type];
	return AVC_MB_INVALID;
}

/* 0x82f100 */
static uint32_t kindP(VideoAvcMbInfo *mb)
{
	int32_t type = mb->mbType & 0x3f, class, i;
	uint32_t kind;

	if (!(type < 31))
		return AVC_MB_INVALID;
	kind = mbKindsP[type];
	class = (kind >> 6) & 0xb;
	if (class != AVC_MB_CLASS_PCM && (class < 8 || !(type & 3)))
		for (i = 0; i < 4; i++)
			mb->subMbType[i] = subMbKindsP[mb->subMbType[i]];
	return kind;
}

/* 0x82f14a */
static uint32_t skipP(VideoAvcMbInfo *mb)
{
	return SKIP_KIND_P;
}

/* 0x82f150 */
static uint32_t kindB(uint32_t sliceHeader, VideoAvcMbInfo *mb)
{
	int32_t type = mb->mbType & 0x3f, class, i, base;
	uint32_t kind;

	if (!(type < 49))
		return AVC_MB_INVALID;
	kind = mbKindsB[type];
	class = (kind >> 6) & 0xb;
	if (!(sliceHeader & AVC_SLICE_DIRECT_8X8) && (1 << ((kind >> 2) & 3)) >= 4)
		base = 0;
	else
		base = 13;
	if (class != AVC_MB_CLASS_PCM && (class < 8 || !(kind & 3)))
		for (i = 0; i < 4; i++)
			mb->subMbType[i] = subMbKindsB[0][mb->subMbType[i] + base];
	return kind;
}

/* 0x82f1ba */
static uint32_t skipB(uint32_t sliceHeader, VideoAvcMbInfo *mb)
{
	uint32_t sub = SKIP_SUB_KIND_B, i;

	if (sliceHeader & AVC_SLICE_DIRECT_8X8)
		sub = SKIP_SUB_KIND_B_8X8;
	for (i = 0; i < 4; i++)
		mb->subMbType[i] = sub;
	return SKIP_KIND_B;
}

/*
 * 0x82f1da: the run's macroblocks from `index` on concealed, up to its
 * count; the position it stopped at kept. Returns the macroblock after the
 * last, 0 at the picture's end.
 */
static VideoAvcMbInfo *runFill(uint32_t width, uint32_t height, AvcdecRun *run,
	AvcdecWork *work, int32_t index)
{
	int32_t x = work->firstX, y = work->firstY, count = run->count;
	VideoAvcMbInfo *above = run->lastRow, *rowStart;
	uint32_t sliceHeader = work->sliceHeader;
	VideoAvcMbInfo *mb = work->mbInfo + index;

	for (; y < (int32_t)height + 1; y++) {
		rowStart = mb;
		for (; x < (int32_t)width; x++) {
			if (!(index < count)) {
				run->lastRow = x ? rowStart : above;
				work->firstY = y;
				work->firstX = x;
				return mb;
			}
			mbFill(mb, sliceHeader);
			mb++;
			index++;
		}
		x = 0;
		above = rowStart;
	}
	return 0;
}

/*
 * 0x82f26e: the run's macroblocks read: each one's QP, kind and flags; an
 * invalid one conceals the rest. Returns the macroblock after the last, 0
 * at the picture's end.
 */
static VideoAvcMbInfo *runParse(uint32_t width, uint32_t height, AvcdecRun *run,
	AvcdecWork *work)
{
	VideoAvcMbInfo *mb = work->mbInfo, *above = run->lastRow, *rowStart;
	int32_t count = run->count, x = 0, y, index;
	const uint8_t *chromaModes = work->chromaModes, *rowModes, *next;
	uint32_t sliceHeader = work->sliceHeader;

	rowModes = chromaModes;
	if (!(count < (int32_t)width))
		x = work->firstX;
	index = 0;
	for (y = work->firstY; y < (int32_t)height + 1; y++) {
		rowStart = mb;
		for (; x < (int32_t)width; x++) {
			if (!(index < count)) {
				run->lastRow = x ? rowStart : above;
				work->firstY = y;
				work->firstX = x;
				return mb;
			}
			mbQp(mb, work, count, index);
			next = mbRead(mb, chromaModes, sliceHeader);
			chromaModes = next;
			if (!next) {
				work->firstX = x;
				work->firstY = y;
				work->ended = 1;
				return runFill(width, height, run, work, index);
			}
			if ((sliceHeader & AVC_SLICE_TYPE) == AVC_SLICE_I)
				mb->kind |= AVC_MB_KEEP_RIGHT | AVC_MB_KEEP_BOTTOM;
			else if (mb->kind & AVC_MB_INTRA)
				mbIntraNeighbours(mb, above, width, x, y);
			mb++;
			index++;
		}
		x = 0;
		chromaModes = rowModes = rowModes + width;
		above = rowStart;
	}
	return 0;
}

/*
 * 0x82f38a: the run's macroblocks read (or concealed after an error); a
 * run that ends above the picture's last row keeps its last row's bottoms
 * for the next.
 */
static void runMacroblocks(const AvcdecDecoderInfo *info, AvcdecRun *run, AvcdecWork *work)
{
	uint32_t width = info->widthInMbs, height = info->heightInMbs;
	VideoAvcMbInfo *last, *row;
	int32_t i;

	if (!work->ended)
		last = runParse(width, height, run, work);
	else
		last = runFill(width, height, run, work, 0);
	if (work->firstY != height) {
		row = run->lastRow;
		for (i = 0; i < (int32_t)width; i++)
			row[i].kind |= AVC_MB_KEEP_BOTTOM;
	}
	run->lastRow = 0;
	run->lastQp = last[-1].qp;
}

/* 0x82f404: a macroblock's QP from the one before and its mb_qp_delta, given on */
static void mbQp(VideoAvcMbInfo *mb, AvcdecWork *work, int32_t count, int32_t index)
{
	int32_t delta = mb->qpDelta, qp = mb->qpPrevious;

	if (delta) {
		qp += delta;
		while (qp >= 52)
			qp -= 52;
		while (qp < 0)
			qp += 52;
	}
	mb->qp = qp;
	if (index + 1 < count)
		mb[1].qpPrevious = qp;
}

/* 0x82f43c: a macroblock concealed: intra DC in an I slice, else skipped */
static void mbFill(VideoAvcMbInfo *mb, uint32_t sliceHeader)
{
	uint32_t type = sliceHeader & AVC_SLICE_TYPE, kind;

	if (type == AVC_SLICE_I)
		kind = FILL_KIND_I;
	else if (type == AVC_SLICE_B)
		kind = SKIP_KIND_B;
	else
		kind = SKIP_KIND_P;
	mb->kind = kind;
	mb->cbf = 0;
	mb->reserved2 = 0;
	mb->qp = 0;
}

/*
 * 0x82f46c: a macroblock's kind written over its type: an intra one's
 * chroma mode taken from the row's modes, its CBP. Returns the modes after
 * it, 0 for an invalid type.
 */
static const uint8_t *mbRead(VideoAvcMbInfo *mb, const uint8_t *chromaModes,
	uint32_t sliceHeader)
{
	uint32_t kind = mbKind(sliceHeader, mb), class;

	if (kind == AVC_MB_INVALID)
		return 0;
	class = AVC_MB_CLASS(kind);
	if (class == AVC_MB_CLASS_INTRA4X4 || class == AVC_MB_CLASS_INTRA16X16)
		kind |= (*chromaModes++ & 3) << AVC_MB_CHROMA_MODE_SHIFT;
	else
		kind |= (mb->skipped & 1) << AVC_MB_SKIP_SHIFT;
	kind |= (mb->cbp & 0x3f) << AVC_MB_CBP_SHIFT;
	mb->kind = kind;
	return chromaModes;
}

/* 0x82f4d4: an intra macroblock: the neighbours it predicts from keep their edges */
static void mbIntraNeighbours(VideoAvcMbInfo *mb, VideoAvcMbInfo *above, int32_t width,
	int32_t x, int32_t y)
{
	if (x)
		mb[-1].kind |= AVC_MB_KEEP_RIGHT;
	if (y && above) {
		above += x;
		if (x - 1 >= 0)
			above[-1].kind |= AVC_MB_KEEP_BOTTOM;
		above[0].kind |= AVC_MB_KEEP_BOTTOM;
		if (!(width < x + 1))
			above[1].kind |= AVC_MB_KEEP_BOTTOM;
	}
}

/*
 * The stripe's prediction job (0x82f524-0x831064): each macroblock's
 * references and vectors predicted and stored for the macroblocks after it,
 * the colocated motion kept for the B pictures, the intra modes and the
 * deblocking's strengths and thresholds worked out.
 */
static void stripeOpen(const AvcdecDecoderInfo *info, const AvcdecRow *entry,
	AvcdecStripe *stripe);
static void stripeClose(const AvcdecDecoderInfo *info, const AvcdecRow *entry,
	AvcdecStripe *stripe);
static void mbLoad(AvcdecStripe *stripe);
static void mbStore(AvcdecStripe *stripe);
static void predictP(AvcdecStripe *stripe, AvcdecMotion *motion);
static void predictB(AvcdecStripe *stripe, AvcdecMotion *motion);
static void refIdxP(AvcdecStripe *stripe, AvcdecMotion *motion);
static void refIdxB(AvcdecStripe *stripe, AvcdecMotion *motion);
static void refPicturesB(AvcdecStripe *stripe, AvcdecMotion *motion);
static void mvdP(AvcdecStripe *stripe, AvcdecMotion *motion);
static void mvdL0(AvcdecStripe *stripe, AvcdecMotion *motion);
static void mvdL1(AvcdecStripe *stripe, AvcdecMotion *motion);
static void mvPredictPartitions(AvcdecStripe *stripe, AvcdecMotion *motion);
static uint32_t directSpatialRefs(AvcdecStripe *stripe, AvcdecMotion *motion,
	uint32_t direct);
static void mvPredictA(AvcdecMotion *motion, uint32_t onlyA);
static void mvPredictB(AvcdecMotion *motion, uint32_t onlyA);
static void mvPredictC(AvcdecMotion *motion, uint32_t onlyA);
static void mvPredictSkip(AvcdecMotion *motion);
static void intraDone(AvcdecStripe *stripe);
static void interDoneP(AvcdecStripe *stripe);
static void interDoneB(AvcdecStripe *stripe);
static void strengthIntra(AvcdecStripe *stripe, uint32_t left, uint32_t top);
static void strengthCoded(AvcdecStripe *stripe, AvcdecLanes strengths[4]);
static void intraPredict(AvcdecStripe *stripe);
static void intraModes(AvcdecStripe *stripe);
static void intraNeighbours(AvcdecStripe *stripe, uint32_t class);

/* 0x82f524, a job: a stripe's macroblocks predicted, then its residual and filter jobs let go */
void avcdecStripePredict(uint32_t infoAddress, uint32_t rowAddress, uint32_t unused,
	uint32_t unused2)
{
	const AvcdecDecoderInfo *info = (const AvcdecDecoderInfo *)infoAddress;
	uint32_t slot = workSlot(), isP, index;
	VkVThreadContext *locked;
	AvcdecStripe *stripe;
	AvcdecMotion motion;
	AvcdecRow *entry;
	int32_t count, i;

	entry = (AvcdecRow *)vnzPublicOpenProtected(rowAddress, DECODER_ROW_SIZE);
	stripe = (AvcdecStripe *)vnzPublicOpenPrivate(entry->common + slot * DECODER_STRIPE_SIZE,
		DECODER_STRIPE_SIZE);
	vnzCacheInvalidateInside((uint32_t)stripe, DECODER_STRIPE_SIZE);
	if (!entry->failed) {
		count = entry->count;
		stripeOpen(info, entry, stripe);
		isP = (stripe->sliceHeader & AVC_SLICE_TYPE) == AVC_SLICE_P;
		for (i = 0; i < count; i++) {
			mbLoad(stripe);
			if (AVC_MB_CLASS(stripe->kind) >= AVC_MB_CLASS_INTRA4X4) {
				intraPredict(stripe);
				intraDone(stripe);
			} else if (isP) {
				predictP(stripe, &motion);
				interDoneP(stripe);
			} else {
				predictB(stripe, &motion);
				interDoneB(stripe);
			}
			mbStore(stripe);
		}
		stripeClose(info, entry, stripe);
	}
	locked = vkVThreadContextLock(info->context);
	index = info->jobsPerKind + entry->jobIndex;
	vkVThreadJobSignal(locked, index);
	vkVThreadJobSignal(locked, info->jobsPerKind * 2 + entry->jobIndex);
	vkVThreadContextUnlock(locked);
	vnzCachePurge((uint32_t)stripe, DECODER_STRIPE_SIZE);
	vnzPublicClosePrivate((uint32_t)stripe, DECODER_STRIPE_SIZE);
	vnzPublicCloseProtected((uint32_t)entry, DECODER_ROW_SIZE);
}

/*
 * 0x82f62a: the stripe's rows opened: its macroblocks at the end of the
 * row's ring entries, the one before them (the last stripe's last) on the
 * left; at the run's start the row's buffers found, and what is outside
 * the run made unavailable.
 */
static void stripeOpen(const AvcdecDecoderInfo *info, const AvcdecRow *entry,
	AvcdecStripe *stripe)
{
	const VideoAvcDecodeCommand *command = (const VideoAvcDecodeCommand *)info->command;
	uint32_t x = entry->x, row = entry->row, mode = info->mode, count = entry->count;
	uint32_t ring = row % info->mbSize, rows, width = info->widthInMbs;
	const VideoHelperJob *job = (const VideoHelperJob *)entry->job;
	AvcdecRowState *rowState;
	AvcdecMbVectors *vectors;
	AvcdecMbFilter *filters;
	AvcdecMbState *states;
	AvcdecTop *top;

	stripe->x = x;
	stripe->row = row;
	rowState = (AvcdecRowState *)(info->table1024 + ring * sizeof(AvcdecRowState));
	top = (AvcdecTop *)(info->row0 + x * sizeof(AvcdecTop));
	stripe->rowState = rowState;
	stripe->top = top;
	stripe->topRight = top + 1;
	stripe->slice = (const VideoHelperSlice *)vnzPublicOpenProtected(entry->slice,
		sizeof(VideoHelperSlice));
	stripe->sliceHeader = stripe->slice->sliceHeader;
	if (info->motion)
		stripe->colocatedOut = (AvcdecColocated *)vnzPublicOpenPrivate(x * 64 +
			(row * width * 64 + info->motion), count * sizeof(AvcdecColocated));
	else
		stripe->colocatedOut = 0;
	ring *= mode;
	states = (AvcdecMbState *)vnzPublicOpenPrivate(info->table4096 +
		ring * sizeof(AvcdecMbState), mode * sizeof(AvcdecMbState));
	vectors = (AvcdecMbVectors *)vnzPublicOpenPrivate(info->table16384 +
		ring * sizeof(AvcdecMbVectors), mode * sizeof(AvcdecMbVectors));
	filters = (AvcdecMbFilter *)vnzPublicOpenPrivate(info->table8192 +
		ring * sizeof(AvcdecMbFilter), mode * sizeof(AvcdecMbFilter));
	stripe->mb = states + (mode - count);
	stripe->vectors = vectors + (mode - count);
	stripe->filter = filters + (mode - count);
	if ((stripe->sliceHeader & AVC_SLICE_TYPE) == AVC_SLICE_B)
		stripe->colocated = (const AvcdecColocated *)stripe->slice->colocated +
			x + row * width;
	stripe->top = (AvcdecTop *)vnzPublicOpenPrivate((uint32_t)stripe->top,
		count * sizeof(AvcdecTop) + sizeof(AvcdecTop));
	stripe->topRight = stripe->top + 1;
	stripe->rowState = (AvcdecRowState *)vnzPublicOpenPrivate((uint32_t)stripe->rowState,
		sizeof(AvcdecRowState));
	if (count != 1) {
		stripe->left = stripe->mb + count - 1;
		stripe->leftVectors = stripe->vectors + count - 1;
		stripe->leftFilter = stripe->filter + count - 1;
	} else {
		/* the stripe's one macroblock takes the last one's place: keep it */
		stripe->left = stripe->mb - 1;
		stripe->leftVectors = stripe->vectors - 1;
		stripe->leftFilter = stripe->filter - 1;
		*stripe->left = *stripe->mb;
		*stripe->leftVectors = *stripe->vectors;
		*stripe->leftFilter = *stripe->filter;
	}
	rowState = stripe->rowState;
	if (!x || (job->firstX == x && job->firstY == row)) {
		rows = row - job->firstY;
		rowState->mbInfo = (VideoAvcMbInfo *)(job->buffers[0] + command->mbRowSize * rows);
		rowState->intra = job->buffers[1] + command->intraRowSize * rows;
		rowState->refIdx = (const AvcdecLanes *)(job->buffers[3] +
			command->refIdxRowSize * rows);
		rowState->mvd = (const uint32_t *)(job->buffers[4] + command->mvdRowSize * rows);
		rowState->topLeftAvailable = 0;
		rowState->topLeftRefIdx.w[0] = -1;
		rowState->topLeftRefIdx.w[1] = -1;
		stripe->left->refIdx.w[0] = -1;
		stripe->left->refIdx.w[1] = -1;
		stripe->left->available = 0;
	}
	if (job->load && job->firstX == x && job->firstY == row) {
		/* a slice starts here: the row above is another's */
		stripe->top->refIdx.w[0] = -1;
		stripe->top->refIdx.w[1] = -1;
		stripe->top->available = 0;
	}
	if (job->nextY == row)
		stripe->belowEnd = 0;
	else if (job->nextY == row + 1)
		stripe->belowEnd = job->nextX;
	else
		stripe->belowEnd = info->widthInMbs;
}

/* 0x82fb94 */
static void stripeClose(const AvcdecDecoderInfo *info, const AvcdecRow *entry,
	AvcdecStripe *stripe)
{
	uint32_t count = entry->count, mode = info->mode;

	vnzPublicCloseProtected((uint32_t)stripe->slice, sizeof(VideoHelperSlice));
	vnzPublicClosePrivate((uint32_t)(stripe->mb - mode), mode * sizeof(AvcdecMbState));
	vnzPublicClosePrivate((uint32_t)(stripe->vectors - mode),
		mode * sizeof(AvcdecMbVectors));
	vnzPublicClosePrivate((uint32_t)(stripe->filter - mode), mode * sizeof(AvcdecMbFilter));
	vnzPublicClosePrivate((uint32_t)(stripe->top - count),
		count * sizeof(AvcdecTop) + sizeof(AvcdecTop));
	vnzPublicClosePrivate((uint32_t)stripe->rowState, sizeof(AvcdecRowState));
	if (stripe->colocatedOut)
		vnzPublicClosePrivate((uint32_t)(stripe->colocatedOut - count),
			count * sizeof(AvcdecColocated));
}

/* 0x82f8e6: the next macroblock read from the run's buffer, its neighbours' availability */
static void mbLoad(AvcdecStripe *stripe)
{
	AvcdecRowState *rowState = stripe->rowState;
	const VideoAvcMbInfo *mbInfo = rowState->mbInfo;
	AvcdecMbState *mb = stripe->mb;
	uint32_t i;

	mb->kind = mbInfo->kind;
	mb->qp = mbInfo->qp;
	mb->qpChroma = mbInfo->reserved2;
	mb->cbf = mbInfo->cbf;
	for (i = 0; i < 4; i++)
		mb->subKinds[i] = mbInfo->subMbType[i];
	stripe->kind = mb->kind;
	for (i = 0; i < 4; i++)
		stripe->subKinds[i] = mb->subKinds[i];
	stripe->cbf = mb->cbf;
	stripe->qp = mb->qp;
	if (AVC_MB_CLASS(stripe->kind) == AVC_MB_CLASS_PCM) {
		stripe->qp = 0;
		mb->qp = 0;
	}
	/* B_Direct_8x8 predicts by 4x4 blocks */
	for (i = 0; i < 4; i++)
		if (stripe->subKinds[i] == SKIP_SUB_KIND_B_8X8)
			stripe->subKinds[i] = SKIP_SUB_KIND_B;
	rowState->mbInfo++;
	stripe->leftAvailable = stripe->left->available;
	stripe->topAvailable = stripe->top->available;
	stripe->topRightAvailable = stripe->topRight->available;
	stripe->topLeftAvailable = rowState->topLeftAvailable;
}

/*
 * 0x82f9a4: the macroblock kept for the ones after it: the row's above-left,
 * the column's bottom (for the row below, if it is in the run), the
 * colocated motion of an intra one; then on to the next.
 */
static void mbStore(AvcdecStripe *stripe)
{
	AvcdecRowState *rowState = stripe->rowState;
	AvcdecMbState *mb = stripe->mb;
	AvcdecTop *top = stripe->top;
	AvcdecColocated *colocated;
	uint32_t kind = stripe->kind;
	uint32_t intra = (kind & (AVC_MB_INTRA | AVC_MB_B)) == AVC_MB_INTRA, i;

	mb->available = 1;
	if (intra) {
		mb->refIdx.w[0] = -1;
		mb->refIdx.w[1] = -1;
		mb->refPicture.w[0] = -1;
		mb->refPicture.w[1] = -1;
	}
	rowState->topLeftKind = top->kind;
	rowState->topLeftRefIdx.w[0] = top->refIdx.w[0];
	rowState->topLeftRefIdx.w[1] = top->refIdx.w[1];
	rowState->topLeftMv[0] = top->mv[3][0];
	rowState->topLeftMv[1] = top->mv[3][1];
	rowState->topLeftAvailable = top->available;
	top->kind = stripe->kind;
	top->cbf = mb->cbf;
	top->qp = mb->qp;
	top->available = 1;
	top->qpChroma = mb->qpChroma;
	if (!intra) {
		top->refPicture.w[0] = mb->refPicture.w[0];
		top->refPicture.w[1] = mb->refPicture.w[1];
		top->mv[0][0] = stripe->vectors->mv[10][0];
		top->mv[0][1] = stripe->vectors->mv[10][1];
		top->mv[1][0] = stripe->vectors->mv[11][0];
		top->mv[1][1] = stripe->vectors->mv[11][1];
		top->mv[2][0] = stripe->vectors->mv[14][0];
		top->mv[2][1] = stripe->vectors->mv[14][1];
		top->mv[3][0] = stripe->vectors->mv[15][0];
		top->mv[3][1] = stripe->vectors->mv[15][1];
	} else {
		top->modes[0] = stripe->filter->modes[10];
		top->modes[1] = stripe->filter->modes[11];
		top->modes[2] = stripe->filter->modes[14];
		top->modes[3] = stripe->filter->modes[15];
		colocated = stripe->colocatedOut;
		if (colocated) {
			if (stripe->sliceHeader & AVC_SLICE_DIRECT_8X8) {
				colocated->corners.refIdxZero = 0;
				for (i = 0; i < 4; i++)
					colocated->corners.pictures[i] = -1;
			} else {
				colocated->blocks[0][3] = 0;
				colocated->blocks[3][2] = -1;
				colocated->blocks[2][2] = -1;
				colocated->blocks[1][2] = -1;
				colocated->blocks[0][2] = -1;
			}
		}
	}
	if (stripe->belowEnd < stripe->x || !stripe->belowEnd) {
		top->available = 0;
		top->refIdx.w[0] = -1;
		top->refIdx.w[1] = -1;
	} else {
		top->available = 1;
		top->refIdx.w[0] = mb->refIdx.w[0];
		top->refIdx.w[1] = mb->refIdx.w[1];
	}
	stripe->x++;
	stripe->leftFilter = stripe->filter;
	stripe->left = stripe->mb;
	stripe->leftVectors = stripe->vectors;
	stripe->mb++;
	stripe->vectors++;
	stripe->filter++;
	stripe->top++;
	stripe->topRight++;
	if (stripe->colocatedOut)
		stripe->colocatedOut++;
	stripe->colocated++;
}

/* an 8x8 block's sub-macroblock partitions predicted: a kernel each */
static inline void mvPredict8x8(AvcdecStripe *stripe, AvcdecMotion *motion, uint32_t block)
{
	if (block == 0)
		avcdecMvPredict8x8Block0(stripe, motion);
	else if (block == 1)
		avcdecMvPredict8x8Block1(stripe, motion);
	else if (block == 2)
		avcdecMvPredict8x8Block2(stripe, motion);
	else
		avcdecMvPredict8x8Block3(stripe, motion);
}

/* 0x82fc0c: a P macroblock's references and vectors */
static void predictP(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	refIdxP(stripe, motion);
	mvdP(stripe, motion);
	if (!(stripe->kind & AVC_MB_8X8)) {
		mvPredictPartitions(stripe, motion);
	} else {
		avcdecMvPredict8x8Block0(stripe, motion);
		avcdecMvPredict8x8Block1(stripe, motion);
		avcdecMvPredict8x8Block2(stripe, motion);
		avcdecMvPredict8x8Block3(stripe, motion);
	}
	avcdecStoreVectors(stripe, motion);
	if (stripe->colocatedOut)
		avcdecStoreColocated(stripe, motion);
}

/*
 * 0x82fc5a: a B macroblock's: its direct 8x8 blocks (all four for
 * B_Skip and B_Direct_16x16) predicted spatially or temporally, the others
 * from their neighbours.
 */
static void predictB(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	AvcdecLanes predictor, current;
	uint32_t direct, i, inferred;

	refIdxB(stripe, motion);
	mvdL0(stripe, motion);
	mvdL1(stripe, motion);
	if (stripe->kind & AVC_MB_8X8) {
		if (!AVC_MB_CLASS(stripe->kind)) {
			/* B_Skip, B_Direct_16x16 */
			for (i = 0; i < 4; i++)
				stripe->subKinds[i] = SKIP_SUB_KIND_B;
			direct = 0xff;
		} else {
			direct = 0;
			for (i = 0; i < 4; i++)
				if (stripe->subKinds[i] == SKIP_SUB_KIND_B)
					direct |= 0x11 << i;
		}
		if (!direct) {
			for (i = 0; i < 4; i++)
				mvPredict8x8(stripe, motion, i);
		} else {
			inferred = 0 < (stripe->sliceHeader & AVC_SLICE_DIRECT_8X8);
			if (!(stripe->sliceHeader & AVC_SLICE_DIRECT_SPATIAL)) {
				for (i = 0; i < 4; i++) {
					if (direct & (1 << i))
						avcdecDirectTemporal(stripe, motion, inferred, i);
					else
						mvPredict8x8(stripe, motion, i);
				}
			} else if (directSpatialRefs(stripe, motion, direct)) {
				/* no neighbour has a reference: zero vectors on ref_idx 0 */
				for (i = 0; i < 4; i++)
					if (!(direct & (1 << i)))
						mvPredict8x8(stripe, motion, i);
			} else {
				predictor = motion->predictor;
				current = motion->current;
				for (i = 0; i < 4; i++) {
					if (direct & (1 << i)) {
						motion->predictor = predictor;
						motion->current = current;
						avcdecDirectSpatial(stripe, motion, inferred, i);
					} else {
						mvPredict8x8(stripe, motion, i);
					}
				}
			}
		}
	} else {
		mvPredictPartitions(stripe, motion);
	}
	refPicturesB(stripe, motion);
	avcdecStoreVectorsB(stripe, motion);
	if (stripe->colocatedOut)
		avcdecStoreColocated(stripe, motion);
}

/*
 * 0x82fdfa: a P macroblock's ref_idx_l0, read when there is more than one
 * reference (clipped to them), by partition; list 1 a copy of list 0.
 */
static void refIdxP(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	const VideoHelperSlice *slice = stripe->slice;
	int32_t count = slice->numRefIdxActive[0];
	AvcdecRowState *rowState;
	uint32_t kind = stripe->kind, i;
	AvcdecLanes refs, limit;
	int8_t pictures[4];

	if (count >= 2 && !(kind & AVC_MB_REF0) && !(kind & AVC_MB_SKIP)) {
		rowState = stripe->rowState;
		for (i = 0; i < 8; i++)
			limit.b[i] = count - 1;
		refs = lanesMinUnsigned(*rowState->refIdx, limit);
		rowState->refIdx++;
		if (!(kind & AVC_MB_8X8)) {
			if (!(kind & 2)) {			/* 8x16: left, right */
				refs.b[2] = refs.b[0];
				refs.b[3] = refs.b[1];
			} else if (!(kind & 1)) {		/* 16x8: top, bottom */
				refs.b[2] = refs.b[1];
				refs.b[3] = refs.b[1];
				refs.b[1] = refs.b[0];
			} else {				/* 16x16 */
				refs.b[1] = refs.b[0];
				refs.b[2] = refs.b[0];
				refs.b[3] = refs.b[0];
			}
		}
	} else {
		refs.all = 0;
	}
	for (i = 0; i < 4; i++)
		pictures[i] = slice->refList[0][refs.b[i] + 1];
	refs.w[1] = refs.w[0];
	motion->refIdx = refs;
	stripe->mb->refIdx.w[0] = refs.w[0];
	stripe->mb->refIdx.w[1] = refs.w[0];
	for (i = 0; i < 4; i++)
		stripe->mb->refPicture.b[i] = pictures[i];
	stripe->mb->refPicture.w[1] = -1;
}

/*
 * 0x82fed2: a B macroblock's ref_idx_l0 and _l1 for the partitions using
 * each list (-1 for the others), read when the list has more than one
 * reference.
 */
static void refIdxB(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	const VideoHelperSlice *slice = stripe->slice;
	int32_t count0 = slice->numRefIdxActive[0], count1 = slice->numRefIdxActive[1];
	AvcdecRowState *rowState = stripe->rowState;
	uint32_t kind = stripe->kind, first = kind >> AVC_MB_LISTS0_SHIFT;
	uint32_t second = kind >> AVC_MB_LISTS1_SHIFT, used = 0, read, i;
	AvcdecLanes refs, read8, limit, none;

	if (!(kind & AVC_MB_8X8)) {
		read8 = *rowState->refIdx;
		if (!(kind & 2)) {			/* 8x16: left, right */
			for (i = 0; i < 2; i++) {
				refs.b[i] = read8.b[i];
				refs.b[2 + i] = read8.b[i];
				refs.b[4 + i] = read8.b[4 + i];
				refs.b[6 + i] = read8.b[4 + i];
			}
			if (first & 1)
				used = 0x05;
			if (second & 1)
				used |= 0x0a;
			if (first & 2)
				used |= 0x50;
			if (second & 2)
				used |= 0xa0;
		} else if (!(kind & 1)) {		/* 16x8: top, bottom */
			for (i = 0; i < 2; i++) {
				refs.b[2 * i] = read8.b[i];
				refs.b[2 * i + 1] = read8.b[i];
				refs.b[4 + 2 * i] = read8.b[4 + i];
				refs.b[4 + 2 * i + 1] = read8.b[4 + i];
			}
			if (first & 1)
				used = 0x03;
			if (second & 1)
				used |= 0x0c;
			if (first & 2)
				used |= 0x30;
			if (second & 2)
				used |= 0xc0;
		} else {				/* 16x16 */
			for (i = 0; i < 4; i++) {
				refs.b[i] = read8.b[0];
				refs.b[4 + i] = read8.b[4];
			}
			if (first & 1)
				used = 0x0f;
			if (first & 2)
				used |= 0xf0;
		}
	} else {
		for (i = 0; i < 4; i++) {
			if (stripe->subKinds[i] & AVC_SUB_L0)
				used |= 1 << i;
			if (stripe->subKinds[i] & AVC_SUB_L1)
				used |= 0x10 << i;
		}
		refs = *rowState->refIdx;
	}
	read = 0;
	if (count0 >= 2)
		read |= used & 0x0f;
	if (count1 >= 2)
		read |= used & 0xf0;
	if (read)
		rowState->refIdx++;
	for (i = 0; i < 4; i++) {
		limit.b[i] = count0 - 1;
		limit.b[4 + i] = count1 - 1;
	}
	none.all = ~0ULL;
	refs = lanesSelect(used, lanesMinUnsigned(refs, limit), none);
	motion->refIdx = refs;
	stripe->mb->refIdx.w[0] = refs.w[0];
	stripe->mb->refIdx.w[1] = refs.w[1];
}

/* 0x830024: a B macroblock's references and the pictures they name */
static void refPicturesB(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	const VideoHelperSlice *slice = stripe->slice;
	AvcdecLanes refs = motion->refIdx, pictures;
	uint32_t i;

	for (i = 0; i < 4; i++)
		pictures.b[i] = slice->refList[0][refs.b[i] + 1];
	for (i = 0; i < 4; i++)
		pictures.b[4 + i] = slice->refList[1][refs.b[4 + i] + 1];
	stripe->mb->refIdx.w[0] = refs.w[0];
	stripe->mb->refIdx.w[1] = refs.w[1];
	stripe->mb->refPicture.w[0] = pictures.w[0];
	stripe->mb->refPicture.w[1] = pictures.w[1];
}

/*
 * A sub-macroblock's mvds by 4x4 block, from its partition (8x8, 8x4, 4x8,
 * 4x4): the words read in order.
 */
static inline const uint32_t *mvdSub(uint32_t sub, const uint32_t *mvd, uint32_t blocks[4])
{
	blocks[0] = *mvd++;
	if (sub & 2) {
		blocks[1] = blocks[0];
		if (sub & 1) {			/* 8x8 */
			blocks[2] = blocks[0];
			blocks[3] = blocks[0];
		} else {			/* 8x4 */
			blocks[2] = *mvd++;
			blocks[3] = blocks[2];
		}
	} else {
		blocks[1] = *mvd++;
		if (sub & 1) {			/* 4x8 */
			blocks[2] = blocks[0];
			blocks[3] = blocks[1];
		} else {			/* 4x4 */
			blocks[2] = *mvd++;
			blocks[3] = *mvd++;
		}
	}
	return mvd;
}

/* 0x8300c6: a P macroblock's mvd_l0, by partition (P_Skip's none) */
static void mvdP(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	const uint32_t *mvd = stripe->rowState->mvd;
	uint32_t kind = stripe->kind, blocks[4], i, j;

	if (kind & AVC_MB_8X8) {
		for (i = 0; i < 4; i++) {
			mvd = mvdSub(stripe->subKinds[i], mvd, blocks);
			for (j = 0; j < 4; j++)
				motion->mv[i][j].all = blocks[j];
		}
	} else if ((kind >> AVC_MB_SKIP_SHIFT) & 1) {
		for (i = 0; i < 4; i++)
			motion->mv[i][0].all = 0;
	} else if (!(kind & 2)) {		/* 8x16 */
		motion->mv[0][0].all = mvd[0];
		motion->mv[1][0].all = mvd[1];
		motion->mv[2][0].all = mvd[0];
		motion->mv[3][0].all = mvd[1];
		mvd += 2;
	} else if (!(kind & 1)) {		/* 16x8 */
		motion->mv[0][0].all = mvd[0];
		motion->mv[1][0].all = mvd[0];
		motion->mv[2][0].all = mvd[1];
		motion->mv[3][0].all = mvd[1];
		mvd += 2;
	} else {
		for (i = 0; i < 4; i++)
			motion->mv[i][0].all = mvd[0];
		mvd++;
	}
	stripe->rowState->mvd = mvd;
}

/* 0x8302b2: a B macroblock's mvd_l0, 0 where list 0 is not used */
static void mvdL0(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	const uint32_t *mvd = stripe->rowState->mvd;
	uint32_t kind = stripe->kind, blocks[4], sub, i, j;

	if (kind & AVC_MB_8X8) {
		for (i = 0; i < 4; i++) {
			sub = stripe->subKinds[i];
			for (j = 0; j < 4; j++)
				motion->mv[i][j].all = 0;
			if ((sub >> AVC_SUB_LISTS_SHIFT) & 1) {
				mvd = mvdSub(sub, mvd, blocks);
				for (j = 0; j < 4; j++)
					motion->mv[i][j].all = blocks[j];
			}
		}
	} else {
		for (i = 0; i < 4; i++)
			motion->mv[i][0].all = 0;
		if (!(kind & 2)) {			/* 8x16 */
			if ((kind >> AVC_MB_LISTS0_SHIFT) & 1)
				motion->mv[0][0].all = *mvd++;
			if ((kind >> AVC_MB_LISTS1_SHIFT) & 1)
				motion->mv[1][0].all = *mvd++;
			motion->mv[2][0] = motion->mv[0][0];
			motion->mv[3][0] = motion->mv[1][0];
		} else if (!(kind & 1)) {		/* 16x8 */
			if ((kind >> AVC_MB_LISTS0_SHIFT) & 1)
				motion->mv[0][0].all = *mvd++;
			if ((kind >> AVC_MB_LISTS1_SHIFT) & 1)
				motion->mv[2][0].all = *mvd++;
			motion->mv[1][0] = motion->mv[0][0];
			motion->mv[3][0] = motion->mv[2][0];
		} else {
			if ((kind >> AVC_MB_LISTS0_SHIFT) & 1)
				motion->mv[0][0].all = *mvd++;
			motion->mv[3][0] = motion->mv[0][0];
			motion->mv[2][0] = motion->mv[0][0];
			motion->mv[1][0] = motion->mv[0][0];
		}
	}
	stripe->rowState->mvd = mvd;
}

/* 0x8304cc: then its mvd_l1, in the vectors' high words */
static void mvdL1(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	const uint32_t *mvd = stripe->rowState->mvd;
	uint32_t kind = stripe->kind, blocks[4], sub, i, j;

	if (kind & AVC_MB_8X8) {
		for (i = 0; i < 4; i++) {
			sub = stripe->subKinds[i];
			if ((sub >> AVC_SUB_LISTS_SHIFT) & 2) {
				mvd = mvdSub(sub, mvd, blocks);
				for (j = 0; j < 4; j++)
					motion->mv[i][j].w[1] = blocks[j];
			}
		}
	} else {
		blocks[0] = blocks[1] = blocks[2] = blocks[3] = 0;
		if (!(kind & 2)) {			/* 8x16 */
			if ((kind >> AVC_MB_LISTS0_SHIFT) & 2)
				blocks[0] = *mvd++;
			if ((kind >> AVC_MB_LISTS1_SHIFT) & 2)
				blocks[1] = *mvd++;
			blocks[2] = blocks[0];
			blocks[3] = blocks[1];
		} else if (!(kind & 1)) {		/* 16x8 */
			if ((kind >> AVC_MB_LISTS0_SHIFT) & 2)
				blocks[0] = *mvd++;
			if ((kind >> AVC_MB_LISTS1_SHIFT) & 2)
				blocks[2] = *mvd++;
			blocks[1] = blocks[0];
			blocks[3] = blocks[2];
		} else {
			if ((kind >> AVC_MB_LISTS0_SHIFT) & 2)
				blocks[0] = *mvd++;
			blocks[3] = blocks[2] = blocks[1] = blocks[0];
		}
		for (i = 0; i < 4; i++)
			motion->mv[i][0].w[1] = blocks[i];
	}
	stripe->rowState->mvd = mvd;
}

static inline AvcdecLanes vectorOf(const uint32_t mv[2])
{
	AvcdecLanes lanes;

	lanes.w[0] = mv[0];
	lanes.w[1] = mv[1];
	return lanes;
}

/* left's vector where its reference is used, 0 where not */
static inline AvcdecLanes vectorA(const AvcdecMotion *motion)
{
	AvcdecLanes zero;

	zero.all = 0;
	return lanesSelect(lanesNegative(motion->refA), zero, motion->mvA);
}

/*
 * 0x8306f2: the vectors of a 16x16, 16x8 or 8x16 macroblock: their mvds
 * plus the predictions from the neighbours (8.4.1.3), the 16x8 and 8x16
 * partitions' directional ones first; P_Skip's (8.4.1.1).
 */
static void mvPredictPartitions(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	const AvcdecMbVectors *leftVectors = stripe->leftVectors;
	const AvcdecRowState *rowState = stripe->rowState;
	const AvcdecMbState *left = stripe->left;
	const AvcdecTop *top = stripe->top;
	AvcdecLanes leftRefs, topRefs, cRefs;
	uint32_t kind = stripe->kind, onlyA, available, i;

	leftRefs = left->refIdx;
	topRefs = top->refIdx;
	motion->refA = lanesOfBlock(leftRefs, 1);
	motion->mvA = vectorOf(leftVectors->mv[5]);
	motion->refB = lanesOfBlock(topRefs, 2);
	motion->mvB = vectorOf(top->mv[0]);
	motion->current = lanesOfBlock(motion->refIdx, 0);
	onlyA = stripe->leftAvailable & !stripe->topAvailable;
	if (!(kind & (3 << AVC_MB_PARTS_SHIFT))) {
		/* 16x16: C above right, else above left */
		if (stripe->topRightAvailable) {
			cRefs = stripe->topRight->refIdx;
			motion->mvC = vectorOf(stripe->topRight->mv[0]);
			motion->refC = lanesOfBlock(cRefs, 2);
			available = stripe->topRightAvailable;
		} else {
			cRefs = rowState->topLeftRefIdx;
			motion->mvC = vectorOf(rowState->topLeftMv);
			motion->refC = lanesOfBlock(cRefs, 3);
			available = stripe->topLeftAvailable;
		}
		onlyA &= !available;
		if ((kind >> AVC_MB_SKIP_SHIFT) & 1) {
			if (!stripe->leftAvailable || !stripe->topAvailable) {
				motion->mv[0][0].all = 0;
			} else {
				mvPredictSkip(motion);
				motion->mv[0][0] = lanesAdd(motion->mv[0][0], motion->predictor);
			}
		} else {
			if (onlyA)
				motion->predictor = vectorA(motion);
			else
				avcdecMvPredictLanes(motion, 0xff);
			motion->mv[0][0] = lanesAdd(motion->mv[0][0], motion->predictor);
		}
		motion->mv[3][0] = motion->mv[0][0];
		motion->mv[2][0] = motion->mv[0][0];
		motion->mv[1][0] = motion->mv[0][0];
	} else if (kind & 2) {
		/* 16x8: the top one from above, the bottom one from the left */
		if (stripe->topRightAvailable) {
			cRefs = stripe->topRight->refIdx;
			motion->mvC = vectorOf(stripe->topRight->mv[0]);
			motion->refC = lanesOfBlock(cRefs, 2);
			available = stripe->topRightAvailable;
		} else {
			cRefs = rowState->topLeftRefIdx;
			motion->mvC = vectorOf(rowState->topLeftMv);
			motion->refC = lanesOfBlock(cRefs, 3);
			available = stripe->topLeftAvailable;
		}
		onlyA &= !available;
		mvPredictB(motion, onlyA);
		motion->refB = motion->current;
		motion->mv[0][0] = lanesAdd(motion->mv[0][0], motion->predictor);
		motion->mvB = motion->mv[0][0];
		motion->refA = lanesOfBlock(leftRefs, 3);
		motion->mvA = vectorOf(leftVectors->mv[13]);
		motion->mvC = vectorOf(leftVectors->mv[7]);
		motion->refC = lanesOfBlock(leftRefs, 1);
		motion->current = lanesOfBlock(motion->refIdx, 2);
		mvPredictA(motion, 0);
		motion->mv[1][0] = motion->mv[0][0];
		motion->mv[2][0] = lanesAdd(motion->mv[2][0], motion->predictor);
		motion->mv[3][0] = motion->mv[2][0];
	} else {
		/* 8x16: the left one from the left, the right one from above right */
		motion->mvC = vectorOf(top->mv[2]);
		motion->refC = lanesOfBlock(topRefs, 3);
		mvPredictA(motion, onlyA);
		motion->refA = motion->current;
		motion->mv[0][0] = lanesAdd(motion->mv[0][0], motion->predictor);
		motion->mvB = vectorOf(top->mv[2]);
		motion->mvA = motion->mv[0][0];
		motion->refB = lanesOfBlock(topRefs, 3);
		if (stripe->topRightAvailable) {
			cRefs = stripe->topRight->refIdx;
			motion->mvC = vectorOf(stripe->topRight->mv[0]);
			onlyA = !stripe->topRightAvailable & !stripe->topAvailable;
		} else {
			cRefs = topRefs;
			motion->mvC = vectorOf(top->mv[1]);
			onlyA = !stripe->topAvailable;
		}
		motion->current = lanesOfBlock(motion->refIdx, 1);
		motion->refC = lanesOfBlock(cRefs, 2);
		mvPredictC(motion, onlyA);
		motion->mv[2][0] = motion->mv[0][0];
		motion->mv[1][0] = lanesAdd(motion->mv[1][0], motion->predictor);
		motion->mv[3][0] = motion->mv[1][0];
	}
	for (i = 0; i < 4; i++)
		motion->mv[i][3] = motion->mv[i][2] = motion->mv[i][1] = motion->mv[i][0];
}

/*
 * 0x830956: spatial direct's references (8.4.1.2.2): by list the smallest
 * of the neighbours', the direct blocks given them. Returns all lanes set
 * when no neighbour has either list (the blocks take ref_idx 0 and no
 * vector), else 0 with the predictor worked out.
 */
static uint32_t directSpatialRefs(AvcdecStripe *stripe, AvcdecMotion *motion,
	uint32_t direct)
{
	const AvcdecMbVectors *leftVectors = stripe->leftVectors;
	const AvcdecRowState *rowState = stripe->rowState;
	const AvcdecTop *top = stripe->top;
	AvcdecLanes leftRefs, topRefs, cRefs, swapped, zero;
	uint32_t none, onlyA;

	leftRefs = stripe->left->refIdx;
	topRefs = top->refIdx;
	if (stripe->topRightAvailable) {
		cRefs = lanesOfBlock(stripe->topRight->refIdx, 2);
	} else {
		cRefs = lanesOfBlock(rowState->topLeftRefIdx, 3);
	}
	zero.all = 0;
	motion->predictor = zero;
	motion->refA = lanesOfBlock(leftRefs, 1);
	motion->refB = lanesOfBlock(topRefs, 2);
	motion->refC = cRefs;
	motion->current = lanesMinUnsigned(lanesMinUnsigned(motion->refB, motion->refC),
		motion->refA);
	swapped.w[0] = motion->current.w[1];
	swapped.w[1] = motion->current.w[0];
	none = lanesNegative(motion->current) & lanesNegative(swapped);
	motion->current = lanesSelect(none, zero, motion->current);
	if (!none) {
		motion->mvA = vectorOf(leftVectors->mv[5]);
		motion->mvB = vectorOf(top->mv[0]);
		onlyA = stripe->leftAvailable & !stripe->topAvailable;
		if (stripe->topRightAvailable) {
			motion->mvC = vectorOf(stripe->topRight->mv[0]);
			onlyA &= !stripe->topRightAvailable;
		} else {
			motion->mvC = vectorOf(rowState->topLeftMv);
			onlyA &= !stripe->topLeftAvailable;
		}
		if (onlyA)
			motion->predictor = vectorA(motion);
		else
			avcdecMvPredictLanes(motion, 0xff);
	}
	motion->refIdx = lanesSelect(direct, motion->current, motion->refIdx);
	stripe->mb->refIdx = motion->refIdx;
	return none;
}

/*
 * 0x830a34: the left's vector where its reference is the partition's,
 * else the median there (only the left's, when it alone is available)
 */
static void mvPredictA(AvcdecMotion *motion, uint32_t onlyA)
{
	uint32_t lanes;

	if (onlyA) {
		motion->predictor = vectorA(motion);
		return;
	}
	lanes = lanesDiffer(motion->current, motion->refA);
	motion->predictor = motion->mvA;
	if (lanes)
		avcdecMvPredictLanes(motion, lanes);
}

/* 0x830a5e: the one above's */
static void mvPredictB(AvcdecMotion *motion, uint32_t onlyA)
{
	uint32_t lanes;

	if (onlyA) {
		motion->predictor = vectorA(motion);
		return;
	}
	lanes = lanesDiffer(motion->current, motion->refB);
	motion->predictor = motion->mvB;
	if (lanes)
		avcdecMvPredictLanes(motion, lanes);
}

/* 0x830a88: the one above right's */
static void mvPredictC(AvcdecMotion *motion, uint32_t onlyA)
{
	uint32_t lanes;

	if (onlyA) {
		motion->predictor = vectorA(motion);
		return;
	}
	lanes = lanesDiffer(motion->current, motion->refC);
	motion->predictor = motion->mvC;
	if (lanes)
		avcdecMvPredictLanes(motion, lanes);
}

/* 0x830ab2: P_Skip's: 0 when the left or the one above has ref_idx 0 and no vector */
static void mvPredictSkip(AvcdecMotion *motion)
{
	uint32_t a, b;

	a = motion->refA.all != 0 || motion->mvA.all != 0;
	b = motion->refB.all != 0 || motion->mvB.all != 0;
	if (!a || !b)
		motion->predictor.all = 0;
	else
		avcdecMvPredictLanes(motion, 0xff);
}

/* the edges the deblocking filters: none at the picture's, nor at the slice's under idc 2 */
static inline uint32_t filterEdges(const AvcdecStripe *stripe, uint32_t *left, uint32_t *top)
{
	uint32_t idc = (stripe->sliceHeader >> AVC_SLICE_DEBLOCKING_SHIFT) & 3;

	*left = 0 < stripe->x;
	*top = 0 < stripe->row;
	if (idc == 2) {
		*top &= stripe->topAvailable;
		*left &= stripe->leftAvailable;
	}
	return idc;
}

/* 0x830aec: an intra macroblock's deblocking: its thresholds, its strengths */
static void intraDone(AvcdecStripe *stripe)
{
	uint32_t left, top;

	avcdecFilterIndexes(stripe);
	if (filterEdges(stripe, &left, &top) != 1)
		strengthIntra(stripe, left, top);
}

/* 0x830b2e: a P macroblock's: the strengths of its coefficients, then of its vectors */
static void interDoneP(AvcdecStripe *stripe)
{
	AvcdecLanes strengths[4];
	uint32_t left, top, idc;

	vnzCacheLine(VNZ_CACHE_INVALIDATE, stripe->filter);
	avcdecFilterIndexes(stripe);
	idc = (stripe->sliceHeader >> AVC_SLICE_DEBLOCKING_SHIFT) & 3;
	if (idc != 1) {
		strengthCoded(stripe, strengths);
		filterEdges(stripe, &left, &top);
		avcdecStrengthsP(stripe, left, top, strengths);
	}
}

/* 0x830b7e: a B macroblock's */
static void interDoneB(AvcdecStripe *stripe)
{
	AvcdecLanes strengths[4];
	uint32_t left, top, idc;

	vnzCacheLine(VNZ_CACHE_INVALIDATE, stripe->filter);
	avcdecFilterIndexes(stripe);
	idc = (stripe->sliceHeader >> AVC_SLICE_DEBLOCKING_SHIFT) & 3;
	if (idc != 1) {
		strengthCoded(stripe, strengths);
		filterEdges(stripe, &left, &top);
		avcdecStrengthsB(stripe, left, top, strengths);
	}
}

/* 0x830bce: an intra macroblock's bS: 4 on the edges filtered, 3 inside */
static void strengthIntra(AvcdecStripe *stripe, uint32_t left, uint32_t top)
{
	AvcdecMbFilter *filter = stripe->filter;
	uint32_t i;

	for (i = 0; i < 4; i++) {
		filter->strength.edges[1][i] = left ? 4 : 0;
		filter->strength.edges[0][i] = top ? 4 : 0;
	}
	for (i = 4; i < 16; i++) {
		filter->strength.edges[1][i] = 3;
		filter->strength.edges[0][i] = 3;
	}
}

/*
 * 0x830c04: an inter macroblock's bS where coefficients are: 2 on the
 * edges of the 4x4 blocks either side of which has some, 4 next to an
 * intra macroblock
 */
static void strengthCoded(AvcdecStripe *stripe, AvcdecLanes strengths[4])
{
	const AvcdecMbState *left = stripe->left;
	const AvcdecTop *top = stripe->top;
	uint32_t cbf = stripe->cbf, vertical, horizontal, leftIntra, topIntra;
	AvcdecMbFilter *filter = stripe->filter;

	leftIntra = left->kind & AVC_MB_INTRA ? 15 : 0;
	topIntra = top->kind & AVC_MB_INTRA ? 15 : 0;
	/* by 4x4 block (z order): it or the one on its left, or above it, has coefficients */
	vertical = (((int32_t)left->cbf >> 5) & 0x505) | ((cbf << 3) & 0x5050) |
		((cbf << 1) & 0xaaaa) | cbf;
	horizontal = (((int32_t)top->cbf >> 10) & 0x33) | ((cbf << 6) & 0x3300) |
		((cbf << 2) & 0xcccc) | cbf;
	avcdecStrengthsCoded(vertical, horizontal, leftIntra, topIntra, strengths);
	filter->strength.lanes[2] = strengths[0];
	filter->strength.lanes[3] = strengths[1];
	filter->strength.lanes[0] = strengths[2];
	filter->strength.lanes[1] = strengths[3];
}

/* 0x830c84: an intra macroblock's modes and the neighbours it may predict from (not I_PCM) */
static void intraPredict(AvcdecStripe *stripe)
{
	uint32_t class = AVC_MB_CLASS(stripe->kind);

	vnzCacheLine(VNZ_CACHE_INVALIDATE, stripe->filter);
	if (class == AVC_MB_CLASS_PCM)
		return;
	if (class == AVC_MB_CLASS_INTRA4X4)
		intraModes(stripe);
	intraNeighbours(stripe, class);
}

/*
 * 0x830fe8: the neighbours an intra macroblock may predict from: the
 * available ones, under constrained_intra_pred only the intra ones; above
 * right and above left for Intra4x4 only (both read from the one above
 * right).
 */
static void intraNeighbours(AvcdecStripe *stripe, uint32_t class)
{
	uint32_t constrained = 0 < (stripe->sliceHeader & AVC_SLICE_CONSTRAINED_INTRA);
	uint32_t neighbours = 0;

	if (stripe->leftAvailable && ((stripe->left->kind & AVC_MB_INTRA) || !constrained))
		neighbours = 1;
	if (stripe->topAvailable && ((stripe->top->kind & AVC_MB_INTRA) || !constrained))
		neighbours |= 2;
	if (class == AVC_MB_CLASS_INTRA4X4) {
		if (stripe->topRightAvailable &&
		    ((stripe->topRight->kind & AVC_MB_INTRA) || !constrained))
			neighbours |= 4;
		if (stripe->topLeftAvailable &&
		    ((stripe->topRight->kind & AVC_MB_INTRA) || !constrained))
			neighbours |= 8;
	}
	stripe->filter->intraNeighbours = neighbours;
}

/* Intra_4x4_DC: what a neighbour gives that is not an Intra4x4 macroblock */
static const int8_t dcModes[16] = {				/* 0x8f72e8 */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
};

/*
 * An Intra4x4PredMode from its prediction and the run's
 * prev_intra4x4_pred_mode_flag / rem_intra4x4_pred_mode (negative for the
 * flag)
 */
static inline int32_t intraMode(int32_t predicted, int32_t rem)
{
	if (rem < 0)
		return predicted;
	if (rem < predicted)
		return rem;
	return rem + 1;
}

static inline int32_t minimum(int32_t a, int32_t b)
{
	return a < b ? a : b;
}

/*
 * 0x830cb8: an Intra4x4 macroblock's modes (8.3.1.1): each predicted from
 * the smaller of the left and the upper block's, DC where either is not
 * available (or is not intra under constrained_intra_pred).
 */
static void intraModes(AvcdecStripe *stripe)
{
	uint32_t constrained = 0 < (stripe->sliceHeader & AVC_SLICE_CONSTRAINED_INTRA);
	const int8_t *rem = (const int8_t *)stripe->rowState->intra;
	uint32_t leftKind = stripe->left->kind, topKind = stripe->top->kind;
	int8_t *modes = stripe->filter->modes;
	const int8_t *left, *top;
	uint32_t dcLeft, dcTop;

	if (!stripe->leftAvailable || (!(leftKind & AVC_MB_INTRA) && constrained))
		dcLeft = 1;
	else
		dcLeft = 0;
	if (!stripe->topAvailable || (!(topKind & AVC_MB_INTRA) && constrained))
		dcTop = 1;
	else
		dcTop = 0;
	left = dcModes;
	if (!dcLeft && AVC_MB_CLASS(leftKind) == AVC_MB_CLASS_INTRA4X4)
		left = stripe->leftFilter->modes;
	top = dcModes;
	if (!dcTop && AVC_MB_CLASS(topKind) == AVC_MB_CLASS_INTRA4X4)
		top = stripe->top->modes;
	modes[0] = intraMode(dcLeft || dcTop ? 2 : minimum(left[5], top[0]), rem[0]);
	modes[2] = intraMode(dcLeft ? 2 : minimum(left[7], modes[0]), rem[2]);
	modes[8] = intraMode(dcLeft ? 2 : minimum(left[13], modes[2]), rem[8]);
	modes[10] = intraMode(dcLeft ? 2 : minimum(left[15], modes[8]), rem[10]);
	modes[1] = intraMode(dcTop ? 2 : minimum(modes[0], top[1]), rem[1]);
	modes[4] = intraMode(dcTop ? 2 : minimum(modes[1], top[2]), rem[4]);
	modes[5] = intraMode(dcTop ? 2 : minimum(modes[4], top[3]), rem[5]);
	modes[3] = intraMode(minimum(modes[2], modes[1]), rem[3]);
	modes[6] = intraMode(minimum(modes[3], modes[4]), rem[6]);
	modes[7] = intraMode(minimum(modes[6], modes[5]), rem[7]);
	modes[9] = intraMode(minimum(modes[8], modes[3]), rem[9]);
	modes[11] = intraMode(minimum(modes[10], modes[9]), rem[11]);
	modes[12] = intraMode(minimum(modes[9], modes[6]), rem[12]);
	modes[13] = intraMode(minimum(modes[12], modes[7]), rem[13]);
	modes[14] = intraMode(minimum(modes[11], modes[12]), rem[14]);
	modes[15] = intraMode(minimum(modes[14], modes[13]), rem[15]);
	stripe->rowState->intra += 16;
}
