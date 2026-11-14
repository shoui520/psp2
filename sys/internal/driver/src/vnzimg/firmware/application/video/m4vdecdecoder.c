/*
 * The MPEG-4 decoder's decoder engine (0x8471ac-0x84b4b0): the engine
 * thread, its runs and rows (the AVC engine's, avcdecdecoder.c, on the
 * records of m4vdecdecoder.h), then the stripes' jobs.
 */
#include "m4vdecdecoder.h"
#include "m4vdecparser.h"	/* m4vdecDcDivide */

static M4vdecDecoder *decoderBegin(uint32_t memory);
static uint32_t decoderWait(M4vdecDecoder *decoder);
static void runStart(M4vdecDecoder *decoder, uint32_t jobAddress);
static uint32_t jobClose(M4vdecDecoder *decoder);
static void pictureDone(M4vdecDecoder *decoder);
static void answerPictures(M4vdecDecoder *decoder);
static void outputStart(M4vdecDecoder *decoder, uint32_t word);
static void answerDrain(M4vdecDecoder *decoder);
static void decoderEnd(M4vdecDecoder *decoder);
static void rowsInit(M4vdecDecoder *decoder, M4vdecDecoderInfo *info);
static void rowsStart(M4vdecDecoder *decoder, M4vdecDecoderInfo *info, int32_t lastRow);
static void runRead(M4vdecDecoder *decoder);
static void runDone(M4vdecDecoder *decoder);
static void stripesDone(M4vdecDecoder *decoder);
static uint32_t stripeDone(M4vdecDecoder *decoder, uint32_t jobIndex, uint32_t row);
static void stripeNext(M4vdecRow *entry, uint32_t jobIndex, uint32_t row, uint32_t count,
	uint32_t mode, uint32_t width);
static void rowsStartSingle(M4vdecDecoder *decoder, M4vdecDecoderInfo *info, int32_t lastRow);
static void runReadSingle(M4vdecDecoder *decoder);
static void stripesDoneSingle(M4vdecDecoder *decoder);
static void jobBuffersOpen(VideoHelperJob *job);
static void jobReferences(uint32_t info, uint32_t pictureStart, uint32_t unused,
	uint32_t unused2);
static void jobPictureDone(uint32_t info, uint32_t word, uint32_t unused, uint32_t unused2);
static void jobOutput(uint32_t info, uint32_t word, uint32_t unused, uint32_t unused2);
static void jobFlush(uint32_t info, uint32_t word, uint32_t unused, uint32_t unused2);
static void jobRun(uint32_t info, uint32_t run, uint32_t unused, uint32_t unused2);
static void runOpen(const M4vdecDecoderInfo *info, const M4vdecRun *run, M4vdecWork *work);
static void runClose(const M4vdecDecoderInfo *info, const M4vdecRun *run, M4vdecWork *work);
static void rowsClear(const M4vdecDecoderInfo *info);

/* a stripe's three jobs: its macroblocks wait for `wanted` signals, its luma and chroma for them */
static inline void stripeSet(const M4vdecDecoderInfo *info, VnzKernelVThreadContext *locked,
	uint32_t index, uint32_t wanted, uint32_t row)
{
	vnzKernelVThreadJobSet(locked, index, wanted, m4vdecStripeMacroblocks, (uint32_t)info,
		row, 0, 0);
	locked = jobNotify(info->context, locked, info->jobsPerKind + index, 1,
		m4vdecStripeLuma, (uint32_t)info, row, 0, info->thread, DECODER_FLAG_STRIPE_DONE);
	jobNotify(info->context, locked, info->jobsPerKind * 2 + index, 1, m4vdecStripeChroma,
		(uint32_t)info, row, 0, info->thread, DECODER_FLAG_STRIPE_DONE);
}

/* a row's entry in the ring */
static inline uint32_t rowEntry(uint32_t rows, uint32_t row)
{
	return rows + (row & (DECODER_ROWS - 1)) * DECODER_ROW_SIZE;
}

/* 0x8471ac: the engine's thread, until it is stopped */
void m4vdecDecoderThread(uint32_t memory, uint32_t codec)
{
	M4vdecDecoder *decoder = decoderBegin(memory);
	M4vdecDecoderInfo *info = decoder->info;
	VnzKernelMessageQueue *parsedQueue = decoder->parsedQueue;
	VnzKernelMessageQueue *helperQueue = decoder->helperQueue;
	VnzKernelMessageQueue *serviceQueue = decoder->serviceQueue;
	VnzKernelMessageQueue *decodeQueue = decoder->decodeQueue;
	uint32_t flags;

	(void)codec;
	do {
		flags = decoderWait(decoder);
		if (decoder->state == DECODER_IDLE &&
		    vnzKernelMessageReceive(decodeQueue, (uint32_t)decoder))
			decoder->state = DECODER_WAITING;
		if (decoder->state == DECODER_WAITING &&
		    (!decoder->receive || vnzKernelMessageReceive(parsedQueue, (uint32_t)decoder)))
			decoder->state = DECODER_RUNNING;
		if (decoder->state != DECODER_RUNNING)
			continue;
		if (decoder->receive)
			vnzKernelMessageReceive(parsedQueue, (uint32_t)decoder);
		if (flags & (DECODER_FLAG_NEXT_RUN | DECODER_FLAG_PICTURE_DONE)) {
			vnzKernelMessageSend(helperQueue, videoHelperJob, jobClose(decoder));
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
		if (flags & (DECODER_FLAG_OUTPUT_DONE | DECODER_FLAG_OUTPUT |
		    DECODER_FLAG_OUTPUT_NONE)) {
			if (flags & (DECODER_FLAG_OUTPUT_DONE | DECODER_FLAG_OUTPUT)) {
				answerPictures(decoder);
				vnzThreadSetFlags(info->thread, DECODER_FLAG_ANSWERED);
			} else {
				answerDrain(decoder);
			}
			vnzKernelMessageSend(serviceQueue, videoServicePicturesDone,
				decoder->request);
			decoder->state = DECODER_IDLE;
		}
	} while (decoder->state != DECODER_STOPPED);
	decoderEnd(decoder);
}

/* 0x8472ec: the memory laid out and opened, the frames cleared */
static M4vdecDecoder *decoderBegin(uint32_t memory)
{
	M4vdecDecoder *header = (M4vdecDecoder *)memory, *decoder;
	M4vdecDecoderInfo *info;
	int32_t i;

	header->info = (M4vdecDecoderInfo *)(memory + DECODER_HEADER_SIZE);
	header->status = (M4vdecDecoderStatus *)(memory + DECODER_HEADER_SIZE +
		DECODER_INFO_SIZE);
	decoder = (M4vdecDecoder *)vnzPublicOpenPrivate(memory, DECODER_HEADER_SIZE);
	info = (M4vdecDecoderInfo *)vnzPublicOpenPrivate((uint32_t)decoder->info,
		DECODER_INFO_SIZE);
	info->thread = vnzThreadCurrent();
	info->status = decoder->status;
	vnzPublicClosePrivate((uint32_t)info, DECODER_INFO_SIZE);
	decoder->info = (M4vdecDecoderInfo *)vnzPublicOpenRaw((uint32_t)decoder->info,
		DECODER_INFO_SIZE);
	decoder->state = DECODER_IDLE;
	decoder->receive = 1;
	decoder->status->count = 0;
	decoder->status->table = -1;
	for (i = 0; i < M4VDEC_DECODER_FRAMES; i++)
		decoder->status->frame[i] = 0;
	return decoder;
}

/* 0x84736a: the thread's flags; an exit stops the engine */
static uint32_t decoderWait(M4vdecDecoder *decoder)
{
	uint32_t flags = vnzThreadWaitFlags(DECODER_FLAGS & ~DECODER_FLAG_FLUSHED);

	if (flags & DECODER_FLAG_EXIT)
		decoder->state = DECODER_STOPPED;
	return flags;
}

/*
 * 0x847390: a message on the parsed queue: the parser's word on a picture
 * (its bit 0 set) or a run the helper parsed.
 */
void m4vdecDecoderParsed(uint32_t memory, uint32_t word)
{
	if (word & 1)
		outputStart((M4vdecDecoder *)memory, word);
	else
		runStart((M4vdecDecoder *)memory, word);
}

/*
 * 0x84739e: a run parsed: for a picture's first run, the picture's command
 * and the helper's context taken, the reference frame noted; then its
 * first job set, which reads it, and the references' job for a new picture.
 */
static void runStart(M4vdecDecoder *decoder, uint32_t jobAddress)
{
	M4vdecDecoderInfo *info = decoder->info;
	const VideoHelperJob *job;
	const VideoHelperContext *context;
	const VideoM4vCommand *command;
	M4vdecDecoderStatus *status;
	uint32_t first, load, runAddress, index;
	VnzKernelVThreadContext *locked;
	M4vdecRun *run;
	int8_t table;

	job = (const VideoHelperJob *)vnzPublicOpenProtected(jobAddress, DECODER_JOB_SIZE);
	first = (uint16_t)job->first;
	load = job->load;
	if (load) {
		context = (const VideoHelperContext *)vnzPublicOpenProtected(job->slice,
			DECODER_CONTEXT_SIZE);
		if (!first) {
			info = (M4vdecDecoderInfo *)vnzPublicOpenPrivate(
				vnzPublicCloseRaw((uint32_t)decoder->info, DECODER_INFO_SIZE),
				DECODER_INFO_SIZE);
			command = (const VideoM4vCommand *)job->command;
			info->command = command;
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
			info->mbSize = 16;
			info->table4096 = context->table4096;
			info->mode = (int8_t)command->mode;
			info->widthInMbs = command->widthInMacroblocks;
			info->heightInMbs = command->heightInMacroblocks;
			info->width = command->width;
			info->pitch = command->pitch;
			info->luma = command->address;
			info->chroma = command->chromaOffset + command->address;
			info->frameSize = command->pictureSize;
			info->chromaOffset = command->chromaOffset;
			info->pictureSize2 = command->pictureSize2;
			info->table = command->table;
			info->frames = command->frames;
			info->lockArea = (AvcdecLockArea *)context->lockArea;
			decoder->pictureStart = 1;
			decoder->rowStripes = (int8_t *)vnzPublicOpenPrivate(context->table128,
				DECODER_ROW_STRIPES_SIZE);
			status = (M4vdecDecoderStatus *)vnzPublicOpenPrivate((uint32_t)info->status,
				M4VDEC_DECODER_STATUS_SIZE);
			table = info->table;
			if (status->table >= 0) {
				info->referenceLuma = status->frame[status->table];
				info->referenceChroma = info->referenceLuma + info->chromaOffset;
			}
			status->table = table;
			vnzPublicClosePrivate((uint32_t)status, M4VDEC_DECODER_STATUS_SIZE);
			rowsInit(decoder, info);
			info = (M4vdecDecoderInfo *)vnzPublicOpenRaw(
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
	run = (M4vdecRun *)vnzPublicOpenPrivate(decoder->run, DECODER_RUN_SIZE);
	run->firstX = job->firstX;
	run->firstY = job->firstY;
	run->count = job->count;
	run->job = vnzPublicCloseProtected((uint32_t)job, DECODER_JOB_SIZE);
	run->common = decoder->common;
	runAddress = vnzPublicClosePrivate((uint32_t)run, DECODER_RUN_SIZE);

	locked = vnzKernelVThreadContextLock(decoder->context);
	index = info->index * 8;
	locked = jobNotify(decoder->context, locked, index + DECODER_JOB_RUN,
		load && first ? 1 : 0, jobRun, (uint32_t)info, runAddress, 0, info->thread,
		DECODER_FLAG_RUN_READ);
	if (load)
		locked = jobNotify(decoder->context, locked, index + DECODER_JOB_REFERENCES, 0,
			jobReferences, (uint32_t)info, decoder->pictureStart, 0, info->thread,
			DECODER_FLAG_RUN_DONE);
	vnzKernelVThreadContextUnlock(locked);
	if (!first)
		rowsStart(decoder, info, info->heightInMbs - 1);
}

/* 0x847676: a decode: its request, answered when its pictures are out */
void m4vdecDecoderDecode(uint32_t memory, uint32_t request)
{
	((M4vdecDecoder *)memory)->request = request;
}

/*
 * 0x84767c: the run of the row given back is closed for the helper: its
 * buffers closed; the parsed queue is read again when the helper has
 * nothing more of the picture.
 */
static uint32_t jobClose(M4vdecDecoder *decoder)
{
	const M4vdecDecoderInfo *info = decoder->info;
	const M4vdecRow *entry = (const M4vdecRow *)(decoder->rows +
		decoder->jobRow % DECODER_ROWS * DECODER_ROW_SIZE);
	VideoHelperJob *job;
	uint32_t size;

	job = (VideoHelperJob *)vnzPublicOpenPrivate(vnzPublicCloseRaw(entry->job,
		DECODER_JOB_SIZE), DECODER_JOB_SIZE);
	size = job->commandSize;
	job->buffers[0] = vnzPublicCloseRaw(job->buffers[0], size * sizeof(VideoM4vMacroblock));
	job->buffers[4] = vnzPublicCloseRaw(job->buffers[4], size * 16);
	job->buffers[5] = vnzPublicCloseRaw(job->buffers[5], size * 512);
	job->buffers[6] = vnzPublicCloseRaw(job->buffers[6], size * 128);
	job->buffers[7] = vnzPublicCloseRaw(job->buffers[7], size * 128);
	if (job->sliceUsed && !job->commandUsed &&
	    (job->endX || !(info->mode < info->widthInMbs)))
		decoder->receive = 1;
	decoder->jobRow = job->endY;
	return vnzPublicClosePrivate((uint32_t)job, DECODER_JOB_SIZE);
}

/* 0x84775a: the picture is decoded: its frame kept with the parser's pictures */
static void pictureDone(M4vdecDecoder *decoder)
{
	M4vdecDecoderInfo *info = decoder->info;
	union {
		AvcdecParsed parsed;
		uint32_t word;
	} done;
	VnzKernelVThreadContext *locked;

	done.parsed.flush = 0;
	done.parsed.valid = 1;
	done.parsed.pictureInfo[0] = decoder->outputCount;
	done.parsed.pictureInfo[1] = decoder->outputStart;
	locked = vnzKernelVThreadContextLock(decoder->context);
	locked = jobNotify(decoder->context, locked, info->index * 8 + DECODER_JOB_OUTPUT, 0,
		jobPictureDone, (uint32_t)info, done.word, 0, info->thread,
		DECODER_FLAG_OUTPUT_DONE);
	vnzKernelVThreadContextUnlock(locked);
	decoder->rowStripes = (int8_t *)vnzPublicClosePrivate((uint32_t)decoder->rowStripes,
		DECODER_ROW_STRIPES_SIZE);
}

/* 0x8477ee: the decode answered with the pictures to give out */
static void answerPictures(M4vdecDecoder *decoder)
{
	const M4vdecDecoderStatus *status = decoder->info->status;
	VideoEngineRequest *request;

	request = (VideoEngineRequest *)vnzPublicOpenPrivate(decoder->request, REQUEST_SIZE);
	request->numOfPictures = status->outputCount;
	request->pictures = status->frames + status->outputStart * sizeof(VideoM4vFrame);
	request->flags = 0;
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
	decoder->receive = 1;
}

/*
 * 0x847842: the parser's word on a unit of no picture to decode: its
 * pictures given out, or a flush (the frames closed when it gives none).
 */
static void outputStart(M4vdecDecoder *decoder, uint32_t word)
{
	M4vdecDecoderInfo *info;
	VnzKernelVThreadContext *locked;
	union {
		AvcdecParsed parsed;
		uint32_t word;
	} parsed;

	decoder->receive = 0;
	info = decoder->info;
	parsed.word = word;
	locked = vnzKernelVThreadContextLock(decoder->context);
	if (parsed.parsed.flush)
		locked = jobNotify(decoder->context, locked, info->index * 8 + DECODER_JOB_FLUSH, 0,
			jobFlush, (uint32_t)info, word, 0, info->thread,
			DECODER_FLAG_OUTPUT_NONE);
	else
		locked = jobNotify(decoder->context, locked, info->index * 8 + DECODER_JOB_OUTPUT,
			0, jobOutput, (uint32_t)info, word, 0, info->thread,
			DECODER_FLAG_OUTPUT_DONE);
	vnzKernelVThreadContextUnlock(locked);
}

/* 0x8478fc: the decode answered as a drain */
static void answerDrain(M4vdecDecoder *decoder)
{
	const M4vdecDecoderStatus *status = decoder->info->status;
	VideoEngineRequest *request;

	request = (VideoEngineRequest *)vnzPublicOpenPrivate(decoder->request, REQUEST_SIZE);
	request->numOfPictures = status->outputCount;
	request->pictures = status->frames + status->outputStart * sizeof(VideoM4vFrame);
	request->flags = VIDEO_ENGINE_DRAIN;
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
	decoder->receive = 1;
}

/* 0x847950 */
static void decoderEnd(M4vdecDecoder *decoder)
{
	vnzPublicCloseRaw((uint32_t)decoder->info, DECODER_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)decoder, DECODER_HEADER_SIZE);
}

/* 0x847976: the engine's memory */
uint32_t m4vdecQueryDecoderMemory(void)
{
	return M4VDEC_DECODER_FIXED_SIZE;
}

/* 0x84797c: the engine's memory laid out for the decoder's slot */
void m4vdecDecoderInitialize(uint32_t memory, VideoService *service, int32_t index)
{
	VideoServiceDecoder *slot = &service->decoders[index];
	M4vdecDecoder *decoder;
	M4vdecDecoderInfo *info;

	decoder = (M4vdecDecoder *)vnzPublicOpenPrivate(memory, M4VDEC_DECODER_FIXED_SIZE);
	vnzCacheInvalidateInside((uint32_t)decoder, M4VDEC_DECODER_FIXED_SIZE);
	info = (M4vdecDecoderInfo *)((uint32_t)decoder + DECODER_HEADER_SIZE);
	decoder->parsedQueue = slot->slot.innerQueue;
	decoder->helperQueue = service->helperQueue2;
	decoder->decodeQueue = slot->slot.decodeQueue;
	decoder->serviceQueue = service->engineQueue2;
	decoder->context = service->encoderContext;
	info->index = index;
	info->id = slot->slot.id;
	info->parserThread = slot->slot.thread;
	vnzPublicClosePrivate((uint32_t)decoder, M4VDEC_DECODER_FIXED_SIZE);
}

/*
 * 0x8479f8: a picture's rows: its stripes counted, each row's flags set
 * by the stripe of the row two above that ends over it.
 */
static void rowsInit(M4vdecDecoder *decoder, M4vdecDecoderInfo *info)
{
	uint32_t mode = info->mode, width = info->widthInMbs, height = info->heightInMbs;
	uint32_t rest = width % mode, stripes, row, column, flags;
	uint8_t columns[8], *end, *last;
	M4vdecRow *rows;

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
	rows = (M4vdecRow *)vnzPublicOpenPrivate(decoder->rows, DECODER_ROWS_SIZE);
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
 * 0x847b98: a picture's first two rows' stripes set: the first row's
 * stripes each wait for the one before, the second's for the two above too.
 */
static void rowsStart(M4vdecDecoder *decoder, M4vdecDecoderInfo *info, int32_t lastRow)
{
	uint32_t mode = info->mode, width = info->widthInMbs, last = decoder->lastStripe;
	uint32_t column, index, rows;
	VnzKernelVThreadContext *locked;

	if (!(mode < width)) {
		rowsStartSingle(decoder, info, lastRow);
		return;
	}
	locked = vnzKernelVThreadContextLock(info->context);
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
	vnzKernelVThreadContextUnlock(locked);
}

/*
 * 0x847fa2: the run's first job read it: its rows' entries written, a
 * stripe each, and the stripes of rows already started signalled.
 */
static void runRead(M4vdecDecoder *decoder)
{
	uint32_t stripes = decoder->rowJobs, rows = decoder->rows, row = decoder->row;
	M4vdecDecoderInfo *info = decoder->info;
	uint32_t width = info->widthInMbs, firstX, endX, endY, index;
	VnzKernelVThreadContext *locked;
	VideoHelperJob *job;
	M4vdecRow *entry;

	if (!(info->mode < width)) {
		runReadSingle(decoder);
		return;
	}
	job = (VideoHelperJob *)vnzPublicOpenPrivate(((const M4vdecRun *)decoder->run)->job,
		DECODER_JOB_SIZE);
	if (job->load) {
		decoder->helperWait = 0;
		decoder->runEnded = 0;
	}
	jobBuffersOpen(job);
	job = (VideoHelperJob *)vnzPublicOpenRaw(vnzPublicClosePrivate((uint32_t)job,
		DECODER_JOB_SIZE), DECODER_JOB_SIZE);
	firstX = job->firstX;
	endY = job->endY;
	endX = job->endX;
	if (firstX) {
		/* the run goes on in the row's stripe that has the macroblocks */
		index = decoder->rowStripes[row] + (row & 3) * stripes;
		entry = (M4vdecRow *)vnzPublicOpenPrivate(rowEntry(rows, row), DECODER_ROW_SIZE);
		entry->x = firstX;
		entry->row = row;
		entry->job = (uint32_t)job;
		if (entry->leftover) {
			entry->count = entry->leftover;
		} else {
			entry->fromStart = 0;
			entry->count = info->mode;
		}
		entry->reserved = 0;
		entry->runEnd = 0;
		entry->leftover = 0;
		entry->jobIndex = index;
		entry->common = decoder->common;
		if (row == endY && !(entry->count + firstX < endX)) {
			entry->runEnd = 1;
			entry->leftover = entry->count - endX + firstX;
			entry->count = endX - firstX;
		}
		if (!(entry->count + firstX < width))
			entry->count = width - entry->x;
		vnzPublicClosePrivate((uint32_t)entry, DECODER_ROW_SIZE);
		locked = vnzKernelVThreadContextLock(info->context);
		vnzKernelVThreadJobSignal(locked, index);
		vnzKernelVThreadContextUnlock(locked);
	} else {
		/* the run's rows from their starts: their first stripes */
		for (; row < endY + (endX > 0); row++) {
			index = (row & 3) * stripes;
			entry = (M4vdecRow *)vnzPublicOpenPrivate(rowEntry(rows, row),
				DECODER_ROW_SIZE);
			entry->row = row;
			entry->x = 0;
			entry->leftover = 0;
			entry->runEnd = 0;
			entry->reserved = 0;
			entry->count = info->mode - row % (info->mode - 1);
			entry->fromStart = 1;
			entry->job = (uint32_t)job;
			entry->jobIndex = index;
			entry->common = decoder->common;
			if (row == endY && !(entry->count < endX)) {
				entry->runEnd = 1;
				entry->leftover = entry->count - endX;
				entry->count = endX;
			}
			vnzPublicClosePrivate((uint32_t)entry, DECODER_ROW_SIZE);
			if (!(decoder->signalRow < row)) {
				locked = vnzKernelVThreadContextLock(info->context);
				vnzKernelVThreadJobSignal(locked, index);
				vnzKernelVThreadContextUnlock(locked);
			}
		}
	}
	decoder->row = endY;
	decoder->lastRow = endX ? endY : endY - 1;
	if (!endX && info->heightInMbs != endY)
		decoder->receive = 1;
}

/*
 * 0x8481d8: the helper's run is decoded: the run's job may go on, or, for
 * a picture's first run, the picture's first stripe.
 */
static void runDone(M4vdecDecoder *decoder)
{
	const M4vdecDecoderInfo *info = decoder->info;
	VnzKernelVThreadContext *locked;

	if (!decoder->pictureStart) {
		locked = vnzKernelVThreadContextLock(decoder->context);
		vnzKernelVThreadJobSignal(locked, info->index * 8 + DECODER_JOB_RUN);
		vnzKernelVThreadContextUnlock(locked);
	} else {
		locked = vnzKernelVThreadContextLock(info->context);
		vnzKernelVThreadJobSignal(locked, 0);
		vnzKernelVThreadContextUnlock(locked);
		decoder->pictureStart = 0;
	}
}

/*
 * 0x848232: stripes are done: the rows done since the last look, up to 15
 * of them, each moved on to its next stripe.
 */
static void stripesDone(M4vdecDecoder *decoder)
{
	M4vdecDecoderInfo *info = decoder->info;
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
 * 0x8482ea: a row's stripe is done: the row's next stripe set up, the
 * stripes it starts (its next, the row below's, the one two below's)
 * signalled, the helper told when the row or the run is done. Returns
 * whether the engine thread was flagged.
 */
static uint32_t stripeDone(M4vdecDecoder *decoder, uint32_t jobIndex, uint32_t row)
{
	uint32_t rows = decoder->rows, x, count, end, flags, width, mode, stripes, belowIndex;
	M4vdecDecoderInfo *info = decoder->info;
	int32_t next, self, limit, started, below;
	uint32_t leftover, fromStart, flagged, entryAddress, belowRow, wanted, bit;
	VnzKernelVThreadContext *locked;
	const VideoHelperJob *job;
	M4vdecRow *entry;

	width = info->widthInMbs;
	entry = (M4vdecRow *)vnzPublicOpenPrivate(rowEntry(rows, row), DECODER_ROW_SIZE);
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
		stripeNext(entry, jobIndex, row, count, mode, width);
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
		locked = vnzKernelVThreadContextLock(info->context);
		stripeSet(info, locked, jobIndex, 1, entryAddress);
		vnzKernelVThreadContextUnlock(locked);
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
		locked = vnzKernelVThreadContextLock(info->context);
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
			vnzKernelVThreadJobSignal(locked, below);
		vnzKernelVThreadContextUnlock(locked);
	}
	locked = vnzKernelVThreadContextLock(info->context);
	if (self >= 0)
		vnzKernelVThreadJobSignal(locked, self);
	if (next >= 0)
		vnzKernelVThreadJobSignal(locked, next);
	vnzKernelVThreadContextUnlock(locked);
	if (!(end < width))
		decoder->rowStripes[row] = stripes;
	else
		decoder->rowStripes[row]++;
	return flagged;
}

/* 0x8488e4: a row's entry moved on to its next stripe */
static void stripeNext(M4vdecRow *entry, uint32_t jobIndex, uint32_t row, uint32_t count,
	uint32_t mode, uint32_t width)
{
	const VideoHelperJob *job = (const VideoHelperJob *)entry->job;
	uint32_t endY = job->endY, endX = job->endX;

	entry->leftover = 0;
	entry->fromStart = 0;
	entry->runEnd = 0;
	entry->jobIndex = jobIndex + 1;
	entry->x += count;
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

/* 0x84895c: rowsStart for a picture of one stripe a row */
static void rowsStartSingle(M4vdecDecoder *decoder, M4vdecDecoderInfo *info, int32_t lastRow)
{
	VnzKernelVThreadContext *locked = vnzKernelVThreadContextLock(info->context);

	stripeSet(info, locked, 0, 2, decoder->rows);
	if (lastRow >= 1)
		stripeSet(info, locked, 1, 2, decoder->rows + DECODER_ROW_SIZE);
	vnzKernelVThreadContextUnlock(locked);
}

/* 0x848abe: runRead for a picture of one stripe a row */
static void runReadSingle(M4vdecDecoder *decoder)
{
	uint32_t rows = decoder->rows, row = decoder->row, index;
	M4vdecDecoderInfo *info = decoder->info;
	uint32_t width = info->widthInMbs, firstX, endX, endY;
	VnzKernelVThreadContext *locked;
	VideoHelperJob *job;
	M4vdecRow *entry;

	job = (VideoHelperJob *)vnzPublicOpenPrivate(((const M4vdecRun *)decoder->run)->job,
		DECODER_JOB_SIZE);
	if (job->load) {
		decoder->helperWait = 0;
		decoder->runEnded = 0;
	}
	jobBuffersOpen(job);
	job = (VideoHelperJob *)vnzPublicOpenRaw(vnzPublicClosePrivate((uint32_t)job,
		DECODER_JOB_SIZE), DECODER_JOB_SIZE);
	firstX = job->firstX;
	endY = job->endY;
	endX = job->endX;
	if (firstX) {
		index = row & 3;
		entry = (M4vdecRow *)vnzPublicOpenPrivate(rowEntry(rows, row), DECODER_ROW_SIZE);
		entry->row = row;
		entry->x = firstX;
		entry->count = width - firstX;
		entry->reserved = 0;
		entry->runEnd = 0;
		entry->job = (uint32_t)job;
		entry->jobIndex = index;
		entry->common = decoder->common;
		if (row == endY && !(entry->count + firstX < endX)) {
			entry->runEnd = 1;
			entry->count = endX - firstX;
		}
		vnzPublicClosePrivate((uint32_t)entry, DECODER_ROW_SIZE);
		if (!(decoder->signalRow < row)) {
			locked = vnzKernelVThreadContextLock(info->context);
			vnzKernelVThreadJobSignal(locked, index);
			vnzKernelVThreadContextUnlock(locked);
		}
	} else {
		for (; row < endY + (endX > 0); row++) {
			index = row & 3;
			entry = (M4vdecRow *)vnzPublicOpenPrivate(rowEntry(rows, row),
				DECODER_ROW_SIZE);
			entry->row = row;
			entry->x = 0;
			entry->reserved = 0;
			entry->runEnd = 0;
			entry->job = (uint32_t)job;
			entry->count = width;
			entry->jobIndex = index;
			entry->common = decoder->common;
			if (row == endY && !(entry->count < endX)) {
				entry->count = endX;
				entry->runEnd = 1;
			}
			vnzPublicClosePrivate((uint32_t)entry, DECODER_ROW_SIZE);
			if (!(decoder->signalRow < row)) {
				locked = vnzKernelVThreadContextLock(info->context);
				vnzKernelVThreadJobSignal(locked, index);
				vnzKernelVThreadContextUnlock(locked);
			}
		}
	}
	decoder->row = endY;
	decoder->lastRow = endX ? endY : endY - 1;
	if (!job->sliceUsed)
		decoder->receive = 1;
}

/*
 * 0x848c6c: a stripe is done in a picture of one stripe a row: the row
 * done once its run's macroblocks are, the rows below started.
 */
static void stripesDoneSingle(M4vdecDecoder *decoder)
{
	uint32_t rows = decoder->rows, row = decoder->doneRow, width, height, bit, index;
	M4vdecDecoderInfo *info = decoder->info;
	AvcdecLockArea *lockArea = info->lockArea;
	uint32_t count, x, endY, endX;
	const M4vdecRow *entry;
	VnzKernelVThreadContext *locked;

	width = info->widthInMbs;
	height = info->heightInMbs;
	if (!(row < height))
		return;
	bit = 1 << (row & 31);
	if (!(lockArea->doneRows & bit))
		return;
	entry = (const M4vdecRow *)rowEntry(rows, row);
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
		locked = vnzKernelVThreadContextLock(info->context);
		stripeSet(info, locked, index, 1, (uint32_t)entry);
		vnzKernelVThreadContextUnlock(locked);
		vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN);
		return;
	}
	decoder->doneRow++;
	decoder->rowStripes[row]++;
	if (row == height - 1) {
		vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN | DECODER_FLAG_PICTURE_DONE);
		return;
	}
	locked = vnzKernelVThreadContextLock(info->context);
	if (row < height - 2) {
		index = (row + 2) & 3;
		stripeSet(info, locked, index, 2, rowEntry(rows, row + 2));
		decoder->signalRow = row + 2;
		if (!(decoder->lastRow < row + 2))
			vnzKernelVThreadJobSignal(locked, index);
	}
	vnzKernelVThreadJobSignal(locked, (row + 1) & 3);
	vnzKernelVThreadContextUnlock(locked);
	if (endY - 1 == row && !endX)
		vnzThreadSetFlags(info->thread, DECODER_FLAG_NEXT_RUN);
}

/* 0x848ec4: a run's buffers opened raw for the stripes' jobs */
static void jobBuffersOpen(VideoHelperJob *job)
{
	uint32_t size = job->commandSize;

	job->buffers[0] = vnzPublicOpenRaw(job->buffers[0], size * sizeof(VideoM4vMacroblock));
	job->buffers[4] = vnzPublicOpenRaw(job->buffers[4], size * 16);
	job->buffers[5] = vnzPublicOpenRaw(job->buffers[5], size * 512);
	job->buffers[6] = vnzPublicOpenRaw(job->buffers[6], size * 128);
	job->buffers[7] = vnzPublicOpenRaw(job->buffers[7], size * 128);
}

/* 0x848f28, a job: a new picture's frame closed first if it was still open */
static void jobReferences(uint32_t infoAddress, uint32_t pictureStart, uint32_t unused,
	uint32_t unused2)
{
	const M4vdecDecoderInfo *info = (const M4vdecDecoderInfo *)infoAddress;
	M4vdecDecoderStatus *status;
	uint32_t frameSize, *previous;

	status = (M4vdecDecoderStatus *)vnzPublicOpenPrivate((uint32_t)info->status,
		M4VDEC_DECODER_STATUS_SIZE);
	frameSize = info->frameSize;
	if (pictureStart) {
		previous = &status->frame[info->table];
		if (*previous) {
			vnzPublicCloseRaw(*previous, frameSize);
			*previous = 0;
		}
	}
	vnzPublicClosePrivate((uint32_t)status, M4VDEC_DECODER_STATUS_SIZE);
}

/*
 * 0x848f70, a job: the picture is decoded: its frame written back and kept
 * open as the next one's reference, the parser's pictures to give out
 * noted; then a read of the frame's last line, after its invalidation.
 */
static void jobPictureDone(uint32_t infoAddress, uint32_t word, uint32_t unused,
	uint32_t unused2)
{
	const M4vdecDecoderInfo *info = (const M4vdecDecoderInfo *)infoAddress;
	uint32_t luma = info->luma, size = info->frameSize;
	M4vdecDecoderStatus *status;
	union {
		AvcdecParsed parsed;
		uint32_t word;
	} parsed;

	status = (M4vdecDecoderStatus *)vnzPublicOpenPrivate((uint32_t)info->status,
		M4VDEC_DECODER_STATUS_SIZE);
	vnzL2Writeback(luma, size);
	vnzL2Invalidate(luma + size - 256, 256);
	status->frame[info->table] = vnzPublicOpenRaw(luma, size);
	parsed.word = word;
	status->frames = info->frames;
	status->outputCount = parsed.parsed.pictureInfo[0];
	status->outputStart = parsed.parsed.pictureInfo[1];
	status->count++;
	vnzPublicClosePrivate((uint32_t)status, M4VDEC_DECODER_STATUS_SIZE);
	(void)*(volatile const uint8_t *)(luma + size - 256);
}

/* 0x849000, a job: the parser's pictures to give out noted */
static void jobOutput(uint32_t infoAddress, uint32_t word, uint32_t unused, uint32_t unused2)
{
	const M4vdecDecoderInfo *info = (const M4vdecDecoderInfo *)infoAddress;
	M4vdecDecoderStatus *status;
	union {
		AvcdecParsed parsed;
		uint32_t word;
	} parsed;

	status = (M4vdecDecoderStatus *)vnzPublicOpenPrivate((uint32_t)info->status,
		M4VDEC_DECODER_STATUS_SIZE);
	parsed.word = word;
	status->outputCount = parsed.parsed.pictureInfo[0];
	status->outputStart = parsed.parsed.pictureInfo[1];
	vnzPublicClosePrivate((uint32_t)status, M4VDEC_DECODER_STATUS_SIZE);
}

/* 0x849036, a job: a flush: the pictures noted, the frames closed when none is left */
static void jobFlush(uint32_t infoAddress, uint32_t word, uint32_t unused, uint32_t unused2)
{
	const M4vdecDecoderInfo *info = (const M4vdecDecoderInfo *)infoAddress;
	M4vdecDecoderStatus *status;
	uint32_t frameSize, *frame;
	int32_t i;
	union {
		AvcdecParsed parsed;
		uint32_t word;
	} parsed;

	status = (M4vdecDecoderStatus *)vnzPublicOpenPrivate((uint32_t)info->status,
		M4VDEC_DECODER_STATUS_SIZE);
	parsed.word = word;
	status->outputCount = parsed.parsed.pictureInfo[0];
	status->outputStart = parsed.parsed.pictureInfo[1];
	if (!parsed.parsed.pictureInfo[0]) {
		frameSize = info->frameSize;
		for (i = 0; i < M4VDEC_DECODER_FRAMES; i++) {
			frame = &status->frame[i];
			if (*frame) {
				vnzPublicCloseRaw(*frame, frameSize);
				*frame = 0;
			}
		}
	}
	vnzPublicClosePrivate((uint32_t)status, M4VDEC_DECODER_STATUS_SIZE);
}

/* this core's work area, by its id */
static inline uint32_t workSlot(void)
{
	return ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 7;
}

/* 0x84909c, a job: the run read, on this core's work area */
static void jobRun(uint32_t infoAddress, uint32_t runAddress, uint32_t unused, uint32_t unused2)
{
	const M4vdecDecoderInfo *info = (const M4vdecDecoderInfo *)infoAddress;
	uint32_t slot = workSlot();
	M4vdecRun *run;
	M4vdecWork *work;

	run = (M4vdecRun *)vnzPublicOpenPrivate(runAddress, DECODER_RUN_SIZE);
	work = (M4vdecWork *)vnzPublicOpenPrivate(run->common + slot * DECODER_WORK_STRIDE,
		DECODER_WORK_SIZE);
	vnzCacheInvalidateInside((uint32_t)work, DECODER_WORK_SIZE);
	runOpen(info, run, work);
	m4vdecRunMacroblocks(run, work);
	runClose(info, run, work);
	vnzPublicClosePrivate((uint32_t)work, DECODER_WORK_SIZE);
	vnzPublicClosePrivate((uint32_t)run, DECODER_RUN_SIZE);
}

/* 0x84910a: the run's job and its macroblocks opened on the work area */
static void runOpen(const M4vdecDecoderInfo *info, const M4vdecRun *run, M4vdecWork *work)
{
	const VideoHelperJob *job;

	job = (const VideoHelperJob *)vnzPublicOpenProtected(run->job, DECODER_JOB_SIZE);
	work->job = job;
	work->macroblocks = (VideoM4vMacroblock *)vnzPublicOpenPrivate(job->buffers[0],
		ALIGN64(job->commandSize * sizeof(VideoM4vMacroblock)));
	work->firstX = run->firstX;
	work->firstY = run->firstY;
	if (!job->first)
		rowsClear(info);
}

/* 0x84916a */
static void runClose(const M4vdecDecoderInfo *info, const M4vdecRun *run, M4vdecWork *work)
{
	const VideoHelperJob *job = work->job;

	vnzPublicClosePrivate((uint32_t)work->macroblocks,
		ALIGN64(job->commandSize * sizeof(VideoM4vMacroblock)));
	vnzPublicCloseProtected((uint32_t)job, DECODER_JOB_SIZE);
}

/* 0x84919c: a picture's first run: the rows above its first row cleared */
static void rowsClear(const M4vdecDecoderInfo *info)
{
	uint32_t width = info->widthInMbs, size = (width + 1) * sizeof(M4vdecAbove);
	M4vdecPredictors *luma, *chroma;
	M4vdecAbove *above;
	int32_t i, j;

	above = (M4vdecAbove *)vnzPublicOpenPrivate(info->row0, size);
	luma = (M4vdecPredictors *)vnzPublicOpenPrivate(info->row1, size);
	chroma = (M4vdecPredictors *)vnzPublicOpenPrivate(info->row2, size);
	for (i = 0; i < (int32_t)width + 1; i++) {
		above[i].quant = 1;
		above[i].vectors[0].word = 0;
		above[i].vectors[1].word = 0;
		above[i].available = 0;
		luma[i].rows[0][0] = DC_RESET;
		luma[i].rows[1][0] = DC_RESET;
		for (j = 1; j < 8; j++) {
			luma[i].rows[0][j] = 0;
			luma[i].rows[1][j] = 0;
		}
		chroma[i].rows[0][0] = DC_RESET;
		chroma[i].rows[1][0] = DC_RESET;
		for (j = 1; j < 8; j++) {
			chroma[i].rows[0][j] = 0;
			chroma[i].rows[1][j] = 0;
		}
	}
	vnzPublicClosePrivate((uint32_t)above, size);
	vnzPublicClosePrivate((uint32_t)luma, size);
	vnzPublicClosePrivate((uint32_t)chroma, size);
}

/* stripe job 1's, after the run pass */
static void vectorsRead(M4vdecStripe *stripe);

static void stripeOpen(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe);
static void mbLoad(M4vdecStripe *stripe);
static void mbStore(M4vdecStripe *stripe);
static void stripeClose(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe);

/* 0x849290, a job: a stripe's macroblocks read with their vectors, then its luma and chroma jobs let go */
void m4vdecStripeMacroblocks(uint32_t infoAddress, uint32_t rowAddress, uint32_t unused,
	uint32_t unused2)
{
	const M4vdecDecoderInfo *info = (const M4vdecDecoderInfo *)infoAddress;
	uint32_t slot = workSlot();
	M4vdecStripe *stripe;
	M4vdecRow *entry;
	int32_t i;

	entry = (M4vdecRow *)vnzPublicOpenProtected(rowAddress, DECODER_ROW_SIZE);
	stripe = (M4vdecStripe *)vnzPublicOpenPrivate(entry->common + slot * M4VDEC_STRIPE_SIZE,
		M4VDEC_STRIPE_SIZE);
	vnzCacheInvalidateInside((uint32_t)stripe, M4VDEC_STRIPE_SIZE);
	stripeOpen(info, entry, stripe);
	for (i = 0; i < entry->count; i++) {
		mbLoad(stripe);
		vectorsRead(stripe);
		mbStore(stripe);
	}
	stripeClose(info, entry, stripe);
	vnzCachePurge((uint32_t)stripe, M4VDEC_STRIPE_SIZE);
	vnzPublicClosePrivate((uint32_t)stripe, M4VDEC_STRIPE_SIZE);
	vnzPublicCloseProtected((uint32_t)entry, DECODER_ROW_SIZE);
}

/*
 * 0x849326: a stripe's records opened: its macroblocks' in the row's ring,
 * the run's buffers where its row starts, the rows above; the rows above
 * cleared where a packet starts inside the row.
 */
static void stripeOpen(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe)
{
	const VideoHelperJob *job = (const VideoHelperJob *)entry->job;
	uint32_t x = entry->x, row = entry->row, count = entry->count, mode = info->mode;
	const VideoM4vCommand *command;
	M4vdecMbRecord *ring;
	M4vdecCursor *cursor;
	M4vdecAbove *above;
	uint32_t limit, i;

	stripe->x = x;
	stripe->row = row;
	command = (const VideoM4vCommand *)vnzPublicOpenProtected((uint32_t)info->command,
		sizeof(VideoM4vCommand));
	stripe->command = command;
	ring = (M4vdecMbRecord *)vnzPublicOpenPrivate(info->table4096 +
		row % info->mbSize * mode * sizeof(M4vdecMbRecord), mode * sizeof(M4vdecMbRecord));
	stripe->mb = ring + (mode - count);
	stripe->cursor = (M4vdecCursor *)(info->table1024 + row % info->mbSize * sizeof(M4vdecCursor));
	stripe->cursor = (M4vdecCursor *)vnzPublicOpenPrivate((uint32_t)stripe->cursor,
		sizeof(M4vdecCursor));
	cursor = stripe->cursor;
	stripe->above = (M4vdecAbove *)(info->row0 + x * sizeof(M4vdecAbove));
	stripe->above = (M4vdecAbove *)vnzPublicOpenPrivate((uint32_t)stripe->above,
		count * sizeof(M4vdecAbove) + sizeof(M4vdecAbove));
	stripe->aboveRight = stripe->above + 1;
	if (count == 1) {
		stripe->left = stripe->mb - 1;
		*stripe->left = *stripe->mb;
	} else {
		stripe->left = stripe->mb + count - 1;
	}
	stripe->shortHeader = command->shortHeader;
	stripe->fcode = job->fcode;
	if (!x || job->firstX == x) {
		cursor->macroblock = (const M4vdecMacroblock *)(job->buffers[0] +
			command->mbRowSize * (row - job->firstY));
		cursor->differences = (const M4vVector *)(job->buffers[4] +
			command->vectorRowSize * (row - job->firstY));
		stripe->left->available = 0;
		stripe->left->vectors[1].word = 0;
		stripe->left->vectors[3].word = 0;
		cursor->aboveLeftAvailable = 0;
	}
	if (job->sliceUsed) {
		if (row == job->endY)
			limit = 0;
		else if (row + 1 == job->endY)
			limit = (int8_t)job->endX;
		else
			limit = info->widthInMbs;
	} else {
		limit = info->widthInMbs;
	}
	stripe->limit = limit;
	if (job->load && job->firstX) {
		above = stripe->above;
		for (i = 0; i < count + 1; i++) {
			above[i].available = 0;
			above[i].vectors[0].word = 0;
			above[i].vectors[1].word = 0;
			above[i].quant = 1;
		}
	}
}

/* 0x8494c2: the next macroblock read into its record, with its neighbours' */
static void mbLoad(M4vdecStripe *stripe)
{
	const M4vdecMacroblock *source = stripe->cursor->macroblock;
	M4vdecMbRecord *mb = stripe->mb;

	mb->kind = source->word;
	mb->quant = source->quant;
	stripe->cursor->macroblock++;
	mb->leftQuant = stripe->left->quant;
	mb->aboveQuant = stripe->above->quant;
	stripe->kind = mb->kind;
	stripe->quant = mb->quant;
	stripe->leftAvailable = stripe->left->available;
	stripe->aboveAvailable = stripe->above->available;
	stripe->aboveRightAvailable = stripe->aboveRight->available;
}

/* 0x84952e: the macroblock kept for the one below and the one after; the next */
static void mbStore(M4vdecStripe *stripe)
{
	M4vdecAbove *above = stripe->above;

	stripe->mb->available = 1;
	stripe->cursor->aboveLeftKind = above->kind;
	stripe->cursor->aboveLeftAvailable = above->available;
	if (stripe->limit < stripe->x || !stripe->limit) {
		above->available = 0;
		above->vectors[0].word = 0;
		above->vectors[1].word = 0;
		above->quant = 1;
	} else {
		above->available = 1;
		above->kind = stripe->kind;
		above->quant = stripe->quant;
	}
	stripe->left = stripe->mb;
	stripe->mb++;
	stripe->above++;
	stripe->aboveRight++;
	stripe->x++;
}

/* 0x8495ba: the stripe's records closed, its luma and chroma jobs let go */
static void stripeClose(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe)
{
	uint32_t mode = info->mode, count = entry->count;
	VnzKernelVThreadContext *locked;

	vnzPublicCloseProtected((uint32_t)stripe->command, sizeof(VideoM4vCommand));
	vnzPublicClosePrivate((uint32_t)(stripe->mb - mode), mode * sizeof(M4vdecMbRecord));
	vnzPublicClosePrivate((uint32_t)(stripe->above - count),
		count * sizeof(M4vdecAbove) + sizeof(M4vdecAbove));
	vnzPublicClosePrivate((uint32_t)stripe->cursor, sizeof(M4vdecCursor));
	locked = vnzKernelVThreadContextLock(info->context);
	vnzKernelVThreadJobSignal(locked, info->jobsPerKind + entry->jobIndex);
	vnzKernelVThreadJobSignal(locked, info->jobsPerKind * 2 + entry->jobIndex);
	vnzKernelVThreadContextUnlock(locked);
}

/* the luma job's, after the run pass */
static void lumaOpen(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe);
static void lumaLoad(M4vdecStripe *stripe);
static void lumaAdvance(M4vdecStripe *stripe);
static void lumaClose(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe);
static void lumaIntra(M4vdecStripe *stripe);
static void lumaInter(M4vdecStripe *stripe);
static void lumaPredict(M4vdecStripe *stripe);
static void referenceFetch(const M4vdecStripe *stripe, const uint8_t *frame, int32_t x, int32_t y,
	M4vdecReference *reference);

/*
 * 0x849644, a job: a stripe's luma decoded, its intra macroblocks from
 * their coefficients, the others predicted from the last picture with
 * their residual; the row's luma noted done for the chroma job's or done.
 */
void m4vdecStripeLuma(uint32_t infoAddress, uint32_t rowAddress, uint32_t unused,
	uint32_t unused2)
{
	const M4vdecDecoderInfo *info = (const M4vdecDecoderInfo *)infoAddress;
	uint32_t slot = workSlot();
	AvcdecLockArea *lockArea;
	M4vdecStripe *stripe;
	M4vdecRow *entry;
	uint32_t bit;
	int32_t i;

	entry = (M4vdecRow *)vnzPublicOpenProtected(rowAddress, DECODER_ROW_SIZE);
	stripe = (M4vdecStripe *)vnzPublicOpenPrivate(entry->common + slot * M4VDEC_STRIPE_SIZE,
		M4VDEC_STRIPE_SIZE);
	vnzCacheInvalidateInside((uint32_t)stripe, M4VDEC_STRIPE_SIZE);
	lumaOpen(info, entry, stripe);
	for (i = 0; i < entry->count; i++) {
		lumaLoad(stripe);
		if (stripe->kind & M4V_KIND_INTRA) {
			lumaIntra(stripe);
		} else {
			lumaPredict(stripe);
			lumaInter(stripe);
		}
		lumaAdvance(stripe);
	}
	lumaClose(info, entry, stripe);
	lockArea = info->lockArea;
	bit = 1 << (stripe->row & 31);
	vnzCachePurge((uint32_t)stripe, M4VDEC_STRIPE_SIZE);
	vnzPublicClosePrivate((uint32_t)stripe, M4VDEC_STRIPE_SIZE);
	vnzPublicCloseProtected((uint32_t)entry, DECODER_ROW_SIZE);
	vnzSpinLock(lockArea->lock);
	if (lockArea->pending & bit) {
		lockArea->pending &= ~bit;
		lockArea->doneRows |= bit;
	} else {
		lockArea->pending |= bit;
	}
	vnzSpinUnlock(lockArea->lock);
}

/*
 * 0x849730: the luma job's records opened: the stripe's luma in the frame,
 * its coefficients' cursor, its macroblocks' in the row's ring, its
 * predictors in the rows above; the predictors reset where a packet starts.
 */
static void lumaOpen(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe)
{
	uint32_t x = entry->x, count = entry->count, row = entry->row, mode = info->mode;
	const VideoHelperJob *job;
	const VideoM4vCommand *command;
	M4vdecPredictors *predictors;
	M4vdecBlockCursor *cursor;
	uint32_t ring, limit, i, k;

	stripe->x = x;
	stripe->row = row;
	command = (const VideoM4vCommand *)vnzPublicOpenProtected((uint32_t)info->command,
		sizeof(VideoM4vCommand));
	stripe->command = command;
	stripe->luma = (uint8_t *)vnzPublicOpenPrivate(info->luma + x * 256 + row * info->width * 16,
		count * 256);
	ring = row % info->mbSize;
	stripe->blockCursor = (M4vdecBlockCursor *)(info->table2048b + ring * sizeof(M4vdecBlockCursor));
	stripe->blockCursor = (M4vdecBlockCursor *)vnzPublicOpenPrivate((uint32_t)stripe->blockCursor,
		sizeof(M4vdecBlockCursor));
	stripe->mb = (M4vdecMbRecord *)vnzPublicOpenProtected(info->table4096 +
		ring * mode * sizeof(M4vdecMbRecord), mode * sizeof(M4vdecMbRecord)) + (mode - count);
	stripe->predictors = (M4vdecPredictors *)(info->row1 + x * sizeof(M4vdecPredictors));
	stripe->predictors = (M4vdecPredictors *)vnzPublicOpenPrivate((uint32_t)stripe->predictors,
		count * sizeof(M4vdecPredictors) + sizeof(M4vdecPredictors));
	stripe->roundingType = command->roundingType;
	stripe->shortHeader = command->shortHeader;
	stripe->width = info->widthInMbs * 16;
	stripe->height = info->heightInMbs * 16;
	stripe->reference = (const uint8_t *)info->referenceLuma;
	stripe->frameWidth = info->width;
	job = (const VideoHelperJob *)entry->job;
	if (!x || job->firstX == x) {
		cursor = stripe->blockCursor;
		cursor->coefficients = (const int16_t *)(job->buffers[5] +
			command->lumaRowSize * (row - job->firstY));
		cursor->aboveLeftDc = DC_RESET;
		predictors = stripe->predictors;
		for (k = 0; k < 2; k++) {
			predictors->columns[k][0] = DC_RESET;
			for (i = 1; i < 8; i++)
				predictors->columns[k][i] = 0;
		}
	}
	if (job->sliceUsed) {
		if (row == job->endY)
			limit = 0;
		else if (row + 1 == job->endY)
			limit = (int8_t)job->endX;
		else
			limit = info->widthInMbs;
	} else {
		limit = info->widthInMbs;
	}
	stripe->limit = limit;
	if (job->load && job->firstX) {
		predictors = stripe->predictors;
		for (i = 0; i < count; i++) {
			for (k = 0; k < 2; k++) {
				predictors[i].rows[k][0] = DC_RESET;
				for (ring = 1; ring < 8; ring++)
					predictors[i].rows[k][ring] = 0;
			}
		}
	}
}

/* 0x8498f4: the next macroblock's kind and quantisers */
static void lumaLoad(M4vdecStripe *stripe)
{
	const M4vdecMbRecord *mb = stripe->mb;

	stripe->kind = mb->kind;
	stripe->quant = mb->quant;
	stripe->leftQuant = mb->leftQuant;
	stripe->aboveQuant = mb->aboveQuant;
}

/* 0x849918: the next macroblock; the predictors below reset past the packet */
static void lumaAdvance(M4vdecStripe *stripe)
{
	M4vdecPredictors *predictors;
	uint32_t k, i;

	if (stripe->x >= stripe->limit) {
		predictors = stripe->predictors;
		for (k = 0; k < 2; k++) {
			predictors->rows[k][0] = DC_RESET;
			for (i = 1; i < 8; i++)
				predictors->rows[k][i] = 0;
		}
	}
	stripe->luma += 256;
	stripe->predictors++;
	stripe->mb++;
	stripe->x++;
}

/* 0x849970: the luma job's records closed */
static void lumaClose(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe)
{
	uint32_t count = entry->count, mode = info->mode;

	vnzPublicCloseProtected((uint32_t)stripe->command, sizeof(VideoM4vCommand));
	vnzPublicCloseProtected((uint32_t)(stripe->mb - mode), mode * sizeof(M4vdecMbRecord));
	vnzPublicClosePrivate((uint32_t)stripe->blockCursor, sizeof(M4vdecBlockCursor));
	vnzPublicClosePrivate((uint32_t)(stripe->predictors - count),
		count * sizeof(M4vdecPredictors) + sizeof(M4vdecPredictors));
	vnzPublicClosePrivate((uint32_t)(stripe->luma - count * 256), count * 256);
}

/* the chroma job's */
static void chromaOpen(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe);
static void chromaLoad(M4vdecStripe *stripe);
static void chromaAdvance(M4vdecStripe *stripe);
static void chromaClose(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe);
static void chromaIntra(M4vdecStripe *stripe);
static void chromaInter(M4vdecStripe *stripe);
static void chromaPredict(M4vdecStripe *stripe);

/* 0x8499cc, a job: a stripe's chroma decoded, as m4vdecStripeLuma its luma */
void m4vdecStripeChroma(uint32_t infoAddress, uint32_t rowAddress, uint32_t unused,
	uint32_t unused2)
{
	const M4vdecDecoderInfo *info = (const M4vdecDecoderInfo *)infoAddress;
	uint32_t slot = workSlot();
	AvcdecLockArea *lockArea;
	M4vdecStripe *stripe;
	M4vdecRow *entry;
	uint32_t bit;
	int32_t i;

	entry = (M4vdecRow *)vnzPublicOpenProtected(rowAddress, DECODER_ROW_SIZE);
	stripe = (M4vdecStripe *)vnzPublicOpenPrivate(entry->common + slot * M4VDEC_STRIPE_SIZE,
		M4VDEC_STRIPE_SIZE);
	vnzCacheInvalidateInside((uint32_t)stripe, M4VDEC_STRIPE_SIZE);
	chromaOpen(info, entry, stripe);
	for (i = 0; i < entry->count; i++) {
		chromaLoad(stripe);
		if (stripe->kind & M4V_KIND_INTRA) {
			chromaIntra(stripe);
		} else {
			chromaPredict(stripe);
			chromaInter(stripe);
		}
		chromaAdvance(stripe);
	}
	chromaClose(info, entry, stripe);
	lockArea = info->lockArea;
	bit = 1 << (stripe->row & 31);
	vnzCachePurge((uint32_t)stripe, M4VDEC_STRIPE_SIZE);
	vnzPublicClosePrivate((uint32_t)stripe, M4VDEC_STRIPE_SIZE);
	vnzPublicCloseProtected((uint32_t)entry, DECODER_ROW_SIZE);
	vnzSpinLock(lockArea->lock);
	if (lockArea->pending & bit) {
		lockArea->pending &= ~bit;
		lockArea->doneRows |= bit;
	} else {
		lockArea->pending |= bit;
	}
	vnzSpinUnlock(lockArea->lock);
}

/* 0x849ab8: the chroma job's records opened, as lumaOpen the luma's */
static void chromaOpen(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe)
{
	uint32_t x = entry->x, count = entry->count, row = entry->row, mode = info->mode;
	const VideoHelperJob *job;
	const VideoM4vCommand *command;
	M4vdecPredictors *predictors;
	M4vdecChromaCursor *cursor;
	uint32_t ring, limit, i, k;

	stripe->x = x;
	stripe->row = row;
	command = (const VideoM4vCommand *)vnzPublicOpenProtected((uint32_t)info->command,
		sizeof(VideoM4vCommand));
	stripe->command = command;
	stripe->chroma = (uint8_t *)vnzPublicOpenPrivate(info->chroma + x * 128 +
		row * info->pitch * 8, count * 128);
	ring = row % info->mbSize;
	stripe->chromaCursor = (M4vdecChromaCursor *)(info->table1024b +
		ring * sizeof(M4vdecChromaCursor));
	stripe->chromaCursor = (M4vdecChromaCursor *)vnzPublicOpenPrivate(
		(uint32_t)stripe->chromaCursor, sizeof(M4vdecChromaCursor));
	stripe->mb = (M4vdecMbRecord *)vnzPublicOpenProtected(info->table4096 +
		ring * mode * sizeof(M4vdecMbRecord), mode * sizeof(M4vdecMbRecord)) + (mode - count);
	stripe->chromaPredictors = (M4vdecPredictors *)(info->row2 + x * sizeof(M4vdecPredictors));
	stripe->chromaPredictors = (M4vdecPredictors *)vnzPublicOpenPrivate(
		(uint32_t)stripe->chromaPredictors,
		count * sizeof(M4vdecPredictors) + sizeof(M4vdecPredictors));
	stripe->roundingType = command->roundingType;
	stripe->shortHeader = command->shortHeader;
	stripe->chromaWidth = info->widthInMbs * 8;
	stripe->chromaHeight = info->heightInMbs * 8;
	stripe->referenceChroma = (const uint8_t *)info->referenceChroma;
	stripe->chromaFrameWidth = info->pitch;
	job = (const VideoHelperJob *)entry->job;
	if (!x || job->firstX == x) {
		cursor = stripe->chromaCursor;
		cursor->coefficients[0] = (const int16_t *)(job->buffers[6] +
			command->chromaRowSize * (row - job->firstY));
		cursor->coefficients[1] = (const int16_t *)(job->buffers[7] +
			command->chromaRowSize * (row - job->firstY));
		cursor->aboveLeftDc[1] = DC_RESET;
		cursor->aboveLeftDc[0] = DC_RESET;
		predictors = stripe->chromaPredictors;
		for (k = 0; k < 2; k++) {
			predictors->columns[k][0] = DC_RESET;
			for (i = 1; i < 8; i++)
				predictors->columns[k][i] = 0;
		}
	}
	if (job->sliceUsed) {
		if (row == job->endY)
			limit = 0;
		else if (row + 1 == job->endY)
			limit = (int8_t)job->endX;
		else
			limit = info->widthInMbs;
	} else {
		limit = info->widthInMbs;
	}
	stripe->limit = limit;
	if (job->load && job->firstX) {
		predictors = stripe->chromaPredictors;
		for (i = 0; i < count; i++) {
			for (k = 0; k < 2; k++) {
				predictors[i].rows[k][0] = DC_RESET;
				for (ring = 1; ring < 8; ring++)
					predictors[i].rows[k][ring] = 0;
			}
		}
	}
}

/* 0x849c86: the next macroblock's kind and quantisers (lumaLoad's) */
static void chromaLoad(M4vdecStripe *stripe)
{
	const M4vdecMbRecord *mb = stripe->mb;

	stripe->kind = mb->kind;
	stripe->quant = mb->quant;
	stripe->leftQuant = mb->leftQuant;
	stripe->aboveQuant = mb->aboveQuant;
}

/* 0x849caa: the next macroblock; the predictors below reset past the packet */
static void chromaAdvance(M4vdecStripe *stripe)
{
	M4vdecPredictors *predictors;
	uint32_t k, i;

	if (stripe->x >= stripe->limit) {
		predictors = stripe->chromaPredictors;
		for (k = 0; k < 2; k++) {
			predictors->rows[k][0] = DC_RESET;
			for (i = 1; i < 8; i++)
				predictors->rows[k][i] = 0;
		}
	}
	stripe->chroma += 128;
	stripe->chromaPredictors++;
	stripe->mb++;
	stripe->x++;
}

/* 0x849d02: the chroma job's records closed */
static void chromaClose(const M4vdecDecoderInfo *info, const M4vdecRow *entry,
	M4vdecStripe *stripe)
{
	uint32_t count = entry->count, mode = info->mode;

	vnzPublicCloseProtected((uint32_t)stripe->command, sizeof(VideoM4vCommand));
	vnzPublicCloseProtected((uint32_t)(stripe->mb - mode), mode * sizeof(M4vdecMbRecord));
	vnzPublicClosePrivate((uint32_t)stripe->chromaCursor, sizeof(M4vdecChromaCursor));
	vnzPublicClosePrivate((uint32_t)(stripe->chromaPredictors - count),
		count * sizeof(M4vdecPredictors) + sizeof(M4vdecPredictors));
	vnzPublicClosePrivate((uint32_t)(stripe->chroma - count * 128), count * 128);
}

/* the kinds by the parser's type (0x8fc174) */
static const uint8_t g_m4vdecKinds[16] = {
	[MB_TYPE_INTER] = 0,
	[MB_TYPE_INTER_Q] = M4V_KIND_QUANT,
	[MB_TYPE_INTER4V] = M4V_KIND_FOUR_VECTORS,
	[MB_TYPE_INTRA] = M4V_KIND_INTRA,
	[MB_TYPE_INTRA_Q] = M4V_KIND_INTRA | M4V_KIND_QUANT,
	[MB_NOT_CODED] = M4V_KIND_NOT_CODED,
};

/* a macroblock's kind word */
static inline uint32_t kindWord(uint32_t type, uint32_t acPrediction, uint32_t blocks,
	uint32_t acCoded, uint32_t dcCoded)
{
	return (g_m4vdecKinds[type] & M4V_KIND_MASK) | (acPrediction & 1) << 7 |
		(blocks & 63) << 8 | (acCoded & 63) << 16 | (dcCoded & 63) << 24;
}

/* the next quantiser: the last one and a dquant, in 1..31 */
static inline int32_t quantNext(int32_t quant, int32_t dquant)
{
	int32_t next = dquant + quant;

	if ((uint32_t)next > 31)
		next = 31;
	if ((uint32_t)next < 1)
		next = 1;
	return next;
}

static uint32_t macroblockKind(VideoM4vMacroblock *mb, uint32_t codingType);
static uint32_t typeInvalid(uint32_t type, uint32_t codingType);
static void runConceal(int32_t x, int32_t y, const VideoHelperJob *job, VideoM4vMacroblock *mb,
	VideoM4vMacroblock *macroblocks);

/*
 * 0x849d5c: the run's macroblocks made kinds with their quantisers; from a
 * macroblock the picture may not have, and for a packet that ended, the
 * rest concealed.
 */
void m4vdecRunMacroblocks(M4vdecRun *run, M4vdecWork *work)
{
	const VideoHelperJob *job = work->job;
	VideoM4vMacroblock *mb = work->macroblocks, *macroblocks = mb;
	const VideoM4vCommand *command = (const VideoM4vCommand *)job->command;
	int32_t quant = job->load ? job->mode : run->quant;
	int32_t y = job->firstY, endY = job->endY, width = command->widthInMacroblocks;
	int32_t endX = job->endX, x = job->firstX, row = 0, dquant;
	uint32_t codingType = command->codingType;

	if (job->ended) {
		runConceal(x, y, job, mb, macroblocks);
		goto done;
	}
	if (x) {
		if (endX && y == endY) {
			for (; x < endX; x++, mb++) {
				dquant = mb->dquant;
				if (macroblockKind(mb, codingType)) {
					runConceal(x, y, job, mb, macroblocks);
					goto done;
				}
				quant = quantNext(quant, dquant);
				((M4vdecMacroblock *)mb)->quant = quant;
			}
			goto done;
		}
		for (; x < width; x++, mb++) {
			dquant = mb->dquant;
			if (macroblockKind(mb, codingType)) {
				runConceal(x, y, job, mb, macroblocks);
				goto done;
			}
			quant = quantNext(quant, dquant);
			((M4vdecMacroblock *)mb)->quant = quant;
		}
		row = 1;
		y++;
	}
	for (; y < endY; y++, row++) {
		mb = (VideoM4vMacroblock *)((uint32_t)macroblocks + command->mbRowSize * row);
		for (x = 0; x < width; x++, mb++) {
			dquant = mb->dquant;
			if (macroblockKind(mb, codingType)) {
				runConceal(x, y, job, mb, macroblocks);
				goto done;
			}
			quant = quantNext(quant, dquant);
			((M4vdecMacroblock *)mb)->quant = quant;
		}
	}
	if (endX) {
		mb = (VideoM4vMacroblock *)((uint32_t)macroblocks + command->mbRowSize * row);
		for (x = 0; x < endX; x++, mb++) {
			dquant = mb->dquant;
			if (macroblockKind(mb, codingType)) {
				runConceal(x, endY, job, mb, macroblocks);
				goto done;
			}
			quant = quantNext(quant, dquant);
			((M4vdecMacroblock *)mb)->quant = quant;
		}
	}
done:
	run->quant = quant;
}

/* 0x849f1e: a macroblock's record made its kind; 1 a type the picture may not have */
static uint32_t macroblockKind(VideoM4vMacroblock *mb, uint32_t codingType)
{
	uint32_t type = mb->type & 15, acCoded = mb->acCoded, blocks = mb->blocks;
	uint32_t acPrediction = mb->acPrediction, dcCoded = mb->dcCoded;

	if (typeInvalid(type, codingType))
		return 1;
	((M4vdecMacroblock *)mb)->word = kindWord(type, acPrediction, blocks, acCoded, dcCoded);
	return 0;
}

/* 0x849f92: a type the picture may not have: an I-VOP's are intra */
static uint32_t typeInvalid(uint32_t type, uint32_t codingType)
{
	if (codingType == VOP_CODING_I)
		return !(type == MB_TYPE_INTRA || type == MB_TYPE_INTRA_Q);
	return !(type < 5 || type == MB_NOT_CODED);
}

/* 0x849fac: a macroblock concealed: intra in an I-VOP, else not coded */
static void macroblockConceal(VideoM4vMacroblock *mb, uint32_t codingType)
{
	uint32_t type, blocks;

	if (codingType == VOP_CODING_I) {
		type = MB_TYPE_INTRA;
		blocks = 63;
	} else {
		type = MB_NOT_CODED;
		blocks = 0;
	}
	((M4vdecMacroblock *)mb)->word = kindWord(type, 0, blocks, 0, 0);
}

/*
 * 0x849fde: the run's macroblocks from (x, y) on concealed, at the packet's
 * quantiser; an I-VOP's coefficients cleared too.
 */
static void runConceal(int32_t x, int32_t y, const VideoHelperJob *job, VideoM4vMacroblock *mb,
	VideoM4vMacroblock *macroblocks)
{
	const VideoM4vCommand *command = (const VideoM4vCommand *)job->command;
	uint32_t quant = job->mode, codingType = command->codingType;
	int32_t width = command->widthInMacroblocks, endX = job->endX, endY = job->endY;
	int32_t row = 0, i;
	uint32_t lumaSize, chromaSize, size;
	uint32_t *luma, *cb, *cr, *l, *b, *r;

	if (x) {
		if (endX && y == endY) {
			for (i = x; i < endX; i++, mb++) {
				macroblockConceal(mb, codingType);
				((M4vdecMacroblock *)mb)->quant = quant;
			}
			return;
		}
		for (i = x; i < width; i++, mb++) {
			macroblockConceal(mb, codingType);
			((M4vdecMacroblock *)mb)->quant = quant;
		}
		row = 1;
		y++;
	}
	for (; y < endY; y++, row++) {
		mb = (VideoM4vMacroblock *)((uint32_t)macroblocks + command->mbRowSize * row);
		for (i = 0; i < width; i++, mb++) {
			macroblockConceal(mb, codingType);
			((M4vdecMacroblock *)mb)->quant = quant;
		}
	}
	if (endX) {
		mb = (VideoM4vMacroblock *)((uint32_t)macroblocks + command->mbRowSize * row);
		for (i = 0; i < endX; i++, mb++) {
			macroblockConceal(mb, codingType);
			((M4vdecMacroblock *)mb)->quant = quant;
		}
	}
	if (codingType != VOP_CODING_I)
		return;
	size = job->commandSize;
	y = job->firstY;
	lumaSize = ALIGN64(size * 512);
	chromaSize = ALIGN64(size * 128);
	luma = (uint32_t *)vnzPublicOpenPrivate(job->buffers[5], lumaSize);
	cb = (uint32_t *)vnzPublicOpenPrivate(job->buffers[6], chromaSize);
	cr = (uint32_t *)vnzPublicOpenPrivate(job->buffers[7], chromaSize);
	for (row = 0; y < endY; y++, row++) {
		l = (uint32_t *)((uint32_t)luma + command->lumaRowSize * row);
		b = (uint32_t *)((uint32_t)cb + command->chromaRowSize * row);
		r = (uint32_t *)((uint32_t)cr + command->chromaRowSize * row);
		for (i = 0; i < width; i++) {
			int32_t k;

			for (k = 0; k < 2; k++) {
				l[0] = 0;
				l[1] = 0;
				l[2] = 0;
				l[3] = 0;
				l += 4;
				*b++ = 0;
				*r++ = 0;
			}
		}
	}
	vnzPublicClosePrivate((uint32_t)luma, lumaSize);
	vnzPublicClosePrivate((uint32_t)cb, chromaSize);
	vnzPublicClosePrivate((uint32_t)cr, chromaSize);
}

/* vop_fcode's range: the vectors wrap at +-low (0x8fc184, 0x8fc194) */
static const uint16_t g_m4vdecVectorLow[8] = { 0, 32, 64, 128, 256, 512, 1024, 2048 };
static const uint16_t g_m4vdecVectorMask[8] = { 0, 63, 127, 255, 511, 1023, 2047, 4095 };

/* the chroma vector's rounding: four vectors' sum / 8, one vector / 2 (0x8fc1a4, 0x8fc1b4) */
static const uint8_t g_m4vdecChromaRound4[16] = {
	0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2
};
static const uint8_t g_m4vdecChromaRound1[4] = { 0, 1, 1, 1 };

static void vectorsIntra(M4vdecStripe *stripe);
static void vectorsZero(M4vdecStripe *stripe);
static void vectorsFour(M4vdecStripe *stripe);
static void vectorsOne(M4vdecStripe *stripe);

/* 0x84a1a4: the macroblock's vectors, by its kind */
static void vectorsRead(M4vdecStripe *stripe)
{
	uint32_t kind = stripe->kind;

	if (kind & M4V_KIND_INTRA)
		vectorsIntra(stripe);
	else if (kind & M4V_KIND_NOT_CODED)
		vectorsZero(stripe);
	else if (kind & M4V_KIND_FOUR_VECTORS)
		vectorsFour(stripe);
	else
		vectorsOne(stripe);
}

/* 0x84a1ca: a vector from its predictor and its difference, wrapped into vop_fcode's range */
static M4vVector vectorAdd(M4vVector predictor, M4vVector difference, uint32_t fcode)
{
	int32_t low = g_m4vdecVectorLow[fcode], mask = g_m4vdecVectorMask[fcode];
	M4vVector vector;
	int16_t x, y;

	x = predictor.x + difference.x;
	x = (int16_t)(x + low) & mask;
	vector.x = x - low;
	y = predictor.y + difference.y;
	y = (int16_t)(y + low) & mask;
	vector.y = y - low;
	return vector;
}

/* 0x84a222: a macroblock not coded: no vectors */
static void vectorsZero(M4vdecStripe *stripe)
{
	M4vdecMbRecord *mb = stripe->mb;
	M4vdecAbove *above = stripe->above;

	mb->chromaVector.word = 0;
	mb->vectors[3].word = 0;
	mb->vectors[2].word = 0;
	mb->vectors[1].word = 0;
	mb->vectors[0].word = 0;
	above->vectors[1].word = 0;
	above->vectors[0].word = 0;
}

/* the median of three */
static inline int32_t median(int32_t a, int32_t b, int32_t c)
{
	int32_t low = min32(a, min32(b, c)), high = max32(a, max32(b, c));

	return a + b + c - low - high;
}

static inline M4vVector vectorMedian(M4vVector a, M4vVector b, M4vVector c)
{
	M4vVector vector;

	vector.x = median(a.x, b.x, c.x);
	vector.y = median(a.y, b.y, c.y);
	return vector;
}

/*
 * a vector's predictor from its three candidates: none available, 0; one,
 * that one (the others are 0); else their median
 */
static inline M4vVector vectorPredict(M4vVector a, M4vVector b, M4vVector c, uint32_t available)
{
	M4vVector vector;

	if (!available) {
		vector.word = 0;
	} else if (available == 1) {
		vector.x = a.x + b.x + c.x;
		vector.y = a.y + b.y + c.y;
	} else {
		vector = vectorMedian(a, b, c);
	}
	return vector;
}

/* the next vector difference */
static inline M4vVector differenceNext(M4vdecStripe *stripe)
{
	return *stripe->cursor->differences++;
}

/* a chroma component from a luma one: halved, rounded toward the quarter samples */
static inline int16_t chromaOne(int16_t v)
{
	int16_t a = abs32(v), c = ((a >> 1) & ~1) + g_m4vdecChromaRound1[a & 3];

	return (v < 0 ? -1 : 1) * c;
}

/* and from four's sum: an eighth */
static inline int16_t chromaFour(int16_t sum)
{
	int16_t a = abs32(sum), c = ((a >> 3) & ~1) + g_m4vdecChromaRound4[a & 15];

	return (sum < 0 ? -1 : 1) * c;
}

/* 0x84a24a: a macroblock's one vector, for its four blocks */
static void vectorsOne(M4vdecStripe *stripe)
{
	M4vdecMbRecord *mb = stripe->mb, *left = stripe->left;
	M4vdecAbove *above = stripe->above, *aboveRight = stripe->aboveRight;
	uint32_t available = left->available + above->available + aboveRight->available;
	M4vVector predictor, vector, chroma;

	predictor = vectorPredict(left->vectors[1], above->vectors[0], aboveRight->vectors[0],
		available);
	vector = vectorAdd(predictor, differenceNext(stripe), stripe->fcode);
	mb->vectors[0] = vector;
	mb->vectors[1] = vector;
	mb->vectors[2] = vector;
	mb->vectors[3] = vector;
	chroma.x = chromaOne(vector.x);
	chroma.y = chromaOne(vector.y);
	mb->chromaVector = chroma;
	above->vectors[0] = vector;
	above->vectors[1] = vector;
}

/* 0x84a402: a macroblock's four vectors, a block each */
static void vectorsFour(M4vdecStripe *stripe)
{
	M4vdecMbRecord *mb = stripe->mb, *left = stripe->left;
	M4vdecAbove *above = stripe->above, *aboveRight = stripe->aboveRight;
	uint32_t fcode = stripe->fcode, aboveAvailable = above->available;
	uint32_t aboveRightAvailable = aboveRight->available;
	M4vVector predictor, vectors[4], chroma;

	/* block 0: the left's block 1, the above's block 2, the above right's block 2 */
	predictor = vectorPredict(left->vectors[1], above->vectors[0], aboveRight->vectors[0],
		left->available + aboveAvailable + aboveRightAvailable);
	vectors[0] = vectorAdd(predictor, differenceNext(stripe), fcode);
	mb->vectors[0] = vectors[0];
	/* block 1: block 0, the above's block 3, the above right's block 2 */
	if (aboveAvailable + aboveRightAvailable + 1 == 1)
		predictor = vectors[0];
	else
		predictor = vectorMedian(vectors[0], above->vectors[1], aboveRight->vectors[0]);
	vectors[1] = vectorAdd(predictor, differenceNext(stripe), fcode);
	mb->vectors[1] = vectors[1];
	/* block 2: the left's block 3, blocks 0 and 1 */
	predictor = vectorMedian(left->vectors[3], vectors[0], vectors[1]);
	vectors[2] = vectorAdd(predictor, differenceNext(stripe), fcode);
	mb->vectors[2] = vectors[2];
	/* block 3: blocks 2, 0 and 1 */
	predictor = vectorMedian(vectors[2], vectors[0], vectors[1]);
	vectors[3] = vectorAdd(predictor, differenceNext(stripe), fcode);
	mb->vectors[3] = vectors[3];
	chroma.x = chromaFour(vectors[0].x + vectors[1].x + vectors[2].x + vectors[3].x);
	chroma.y = chromaFour(vectors[0].y + vectors[1].y + vectors[2].y + vectors[3].y);
	mb->chromaVector = chroma;
	above->vectors[0] = vectors[2];
	above->vectors[1] = vectors[3];
}

/* 0x84a806: an intra macroblock: no vectors for its neighbours */
static void vectorsIntra(M4vdecStripe *stripe)
{
	M4vdecMbRecord *mb = stripe->mb;
	M4vdecAbove *above = stripe->above;

	mb->vectors[3].word = 0;
	mb->vectors[1].word = 0;
	above->vectors[1].word = 0;
	above->vectors[0].word = 0;
}

/* the luma's dc_scaler by quantiser (0x8fc1b8) */
static const uint8_t g_m4vdecDcScalerLuma[32] = {
	0, 8, 8, 8, 8, 10, 12, 14, 16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31, 32, 34, 36, 38, 40, 42, 44, 46,
};

static void lumaIntraShort(M4vdecStripe *stripe);
static void lumaIntraPredicted(M4vdecStripe *stripe);
static void lumaResidual(M4vdecStripe *stripe);
static void lumaInterPredictors(M4vdecStripe *stripe);
static uint32_t dcPredict(M4vdecBlock *block, int32_t left, int32_t aboveLeft, int32_t above,
	int32_t scaler);

/*
 * 0x84a824: an intra macroblock's luma, by the picture's header. (It sets
 * ccr8 to 0xfedcba10, cpperm0's lane 0 from its second operand, for
 * dcDequantize's DC.)
 */
static void lumaIntra(M4vdecStripe *stripe)
{
	if (stripe->shortHeader)
		lumaIntraShort(stripe);
	else
		lumaIntraPredicted(stripe);
}

/* 0x84a83e: an inter macroblock's luma residual, its predictors reset for intra neighbours */
static void lumaInter(M4vdecStripe *stripe)
{
	if (((stripe->kind >> 8) & 63) >> 2)
		lumaResidual(stripe);
	lumaInterPredictors(stripe);
}

/* 0x84a870: the block's IDCT */
static void idct(M4vdecBlock *block)
{
	m4vdecIdctRows(block);
	m4vdecIdctColumns(block);
}

/* 0x84a884: an intra block dequantised, its DC by the dc_scaler, in 12 bits */
static void dcDequantize(M4vdecBlock *block, uint32_t even, uint32_t quant, int32_t scaler)
{
	int32_t dc;

	dc = max32(min32(block->coef[0][0] * scaler, 2047), -2048);
	m4vdecDequantize(block, even, quant);
	block->coef[0][0] = dc;
}

/* 0x84a8b6: a short header's intra luma: no AC/DC prediction, the DCs by 8 */
static void lumaIntraShort(M4vdecStripe *stripe)
{
	const int16_t *coefficients = stripe->blockCursor->coefficients;
	uint32_t quant = (uint8_t)stripe->quant, even = ~quant & 1;
	uint32_t coded = ((stripe->kind >> 16) & 63) >> 2;
	uint8_t *luma = stripe->luma;
	M4vdecBlock block;
	int32_t i;

	for (i = 0; i < 4; i++) {
		if (coded & 8) {
			m4vdecBlockLoad(&block, coefficients);
			coefficients += 64;
		} else {
			m4vdecBlockLoadDc(&block, coefficients);
			coefficients += 4;
		}
		dcDequantize(&block, even, quant, 8);
		idct(&block);
		m4vdecBlockStore(&block, luma + (i & 1) * 8 + (i & 2) * 64);
		coded <<= 1;
	}
	stripe->blockCursor->coefficients = coefficients;
}

/* a block's coefficients, all of them or its DC alone */
static inline const int16_t *blockLoad(M4vdecBlock *block, const int16_t *coefficients,
	uint32_t full)
{
	if (full) {
		m4vdecBlockLoad(block, coefficients);
		return coefficients + 64;
	}
	m4vdecBlockLoadDc(block, coefficients);
	return coefficients + 4;
}

/* the AC prediction's row or column, into the coprocessor */
static inline void predictorLoad(M4vdecBlock *block, const int16_t *predictor)
{
	uint32_t i;

	for (i = 0; i < 8; i++)
		block->predictor[i] = predictor[i];
}

/* the DC alone predicted: added to the DC lanes */
static inline void dcAdd(M4vdecBlock *block)
{
	uint32_t i;

	for (i = 0; i < 4; i++)
		block->coef[0][i] = (int16_t)(block->coef[0][i] + block->dc[i]);
}

/* the AC predicted from the left, rescaled from its quantiser when it is another */
static inline void acPredictLeft(M4vdecBlock *block, const int16_t *predictor, int32_t quant,
	int32_t predictorQuant)
{
	predictorLoad(block, predictor);
	if (quant == predictorQuant)
		m4vdecAcPredictLeft(block);
	else
		m4vdecAcPredictLeftScaled(block, quant, predictorQuant);
}

/* and from above */
static inline void acPredictTop(M4vdecBlock *block, const int16_t *predictor, int32_t quant,
	int32_t predictorQuant)
{
	predictorLoad(block, predictor);
	if (quant == predictorQuant)
		m4vdecAcPredictTop(block);
	else
		m4vdecAcPredictTopScaled(block, quant, predictorQuant);
}

/*
 * 0x84a954: an intra macroblock's luma with AC/DC prediction (7.4.3): each
 * block's DC from the neighbour its gradient picks, with the first column
 * or row of AC when ac_pred_flag is set; its own kept for the blocks after.
 * Blocks 0 and 2 predict from the macroblock to the left, 0 and 1 from the
 * one above, at its quantiser.
 */
static void lumaIntraPredicted(M4vdecStripe *stripe)
{
	const int16_t *coefficients = stripe->blockCursor->coefficients;
	uint32_t quant = (uint8_t)stripe->quant, aboveQuant = (uint8_t)stripe->aboveQuant;
	uint32_t leftQuant = (uint8_t)stripe->leftQuant, even = ~quant & 1;
	uint32_t acPrediction = stripe->kind & M4V_KIND_AC_PREDICTION;
	uint32_t coded = ((stripe->kind >> 16) & 63) >> 2;
	int32_t scaler = g_m4vdecDcScalerLuma[quant];
	M4vdecPredictors *slot = stripe->predictors, *next = slot + 1;
	uint8_t *luma = stripe->luma;
	int32_t aboveLeft, dc0, dc1, dc2, dc3;
	M4vdecBlockCursor *cursor;
	M4vdecBlock block;
	uint32_t left;

	/* block 0: left from the macroblock to the left, above from the one above */
	coefficients = blockLoad(&block, coefficients, coded & 8);
	cursor = stripe->blockCursor;
	aboveLeft = cursor->aboveLeftDc;
	cursor->aboveLeftDc = slot->rows[1][0];
	left = dcPredict(&block, slot->columns[0][0], aboveLeft, slot->rows[0][0], scaler);
	if (!acPrediction)
		dcAdd(&block);
	else if (left)
		acPredictLeft(&block, slot->columns[0], quant, leftQuant);
	else
		acPredictTop(&block, slot->rows[0], quant, aboveQuant);
	aboveLeft = slot->rows[0][0];
	m4vdecPredictorsStore(&block, next->columns[0], slot->rows[0]);
	dcDequantize(&block, even, quant, scaler);
	dc0 = block.coef[0][0];
	idct(&block);
	m4vdecBlockStore(&block, luma);

	/* block 1: left from block 0, above from the macroblock above */
	coefficients = blockLoad(&block, coefficients, coded & 4);
	left = dcPredict(&block, dc0, aboveLeft, slot->rows[1][0], scaler);
	if (!acPrediction) {
		dcAdd(&block);
	} else if (left) {
		predictorLoad(&block, next->columns[0]);
		m4vdecAcPredictLeft(&block);
	} else {
		acPredictTop(&block, slot->rows[1], quant, aboveQuant);
	}
	m4vdecPredictorsStore(&block, next->columns[0], slot->rows[1]);
	dcDequantize(&block, even, quant, scaler);
	dc1 = block.coef[0][0];
	next->columns[0][0] = dc1;
	idct(&block);
	m4vdecBlockStore(&block, luma + 8);

	/* block 2: left from the macroblock to the left, above from block 0 */
	coefficients = blockLoad(&block, coefficients, coded & 2);
	left = dcPredict(&block, slot->columns[1][0], slot->columns[0][0], dc0, scaler);
	if (!acPrediction) {
		dcAdd(&block);
	} else if (left) {
		acPredictLeft(&block, slot->columns[1], quant, leftQuant);
	} else {
		predictorLoad(&block, slot->rows[0]);
		m4vdecAcPredictTop(&block);
	}
	m4vdecPredictorsStore(&block, next->columns[1], slot->rows[0]);
	dcDequantize(&block, even, quant, scaler);
	dc2 = block.coef[0][0];
	slot->rows[0][0] = dc2;
	idct(&block);
	m4vdecBlockStore(&block, luma + 128);

	/* block 3: left from block 2, above from block 1 */
	coefficients = blockLoad(&block, coefficients, coded & 1);
	left = dcPredict(&block, dc2, dc0, dc1, scaler);
	if (!acPrediction) {
		dcAdd(&block);
	} else if (left) {
		predictorLoad(&block, next->columns[1]);
		m4vdecAcPredictLeft(&block);
	} else {
		predictorLoad(&block, slot->rows[1]);
		m4vdecAcPredictTop(&block);
	}
	m4vdecPredictorsStore(&block, next->columns[1], slot->rows[1]);
	dcDequantize(&block, even, quant, scaler);
	dc3 = block.coef[0][0];
	slot->rows[1][0] = dc3;
	next->columns[1][0] = dc3;
	idct(&block);
	m4vdecBlockStore(&block, luma + 136);
	stripe->blockCursor->coefficients = coefficients;
}

/* 0x84abfe: an inter macroblock's luma residual, added to its prediction */
static void lumaResidual(M4vdecStripe *stripe)
{
	const int16_t *coefficients = stripe->blockCursor->coefficients;
	uint32_t quant = (uint8_t)stripe->quant, even = ~quant & 1;
	uint32_t full = ((stripe->kind >> 16) & 63) >> 2;
	uint32_t coded = ((stripe->kind >> 8) & 63) >> 2;
	uint8_t *luma = stripe->luma;
	M4vdecBlock block;
	int32_t i;

	for (i = 0; i < 4; i++) {
		if (coded & 8) {
			coefficients = blockLoad(&block, coefficients, full & 8);
			m4vdecDequantize(&block, even, quant);
			idct(&block);
			m4vdecBlockAdd(&block, luma + (i & 1) * 8 + (i & 2) * 64);
		}
		full <<= 1;
		coded <<= 1;
	}
	stripe->blockCursor->coefficients = coefficients;
}

/* 0x84acba: an inter macroblock's predictors, for intra ones next to it: reset */
static void lumaInterPredictors(M4vdecStripe *stripe)
{
	M4vdecPredictors *slot = stripe->predictors, *next = slot + 1;
	uint32_t k, i;

	stripe->blockCursor->aboveLeftDc = slot->rows[1][0];
	for (k = 0; k < 2; k++) {
		next->columns[k][0] = DC_RESET;
		for (i = 1; i < 8; i++)
			next->columns[k][i] = 0;
	}
	for (k = 0; k < 2; k++) {
		slot->rows[k][0] = DC_RESET;
		for (i = 1; i < 8; i++)
			slot->rows[k][i] = 0;
	}
}

/*
 * 0x84b058: a block's DC predicted (7.4.3.1): from above where the
 * gradient from the block above-left is flatter that way, else from the
 * left; divided by the dc_scaler. Returns 1 from the left.
 */
static uint32_t dcPredict(M4vdecBlock *block, int32_t left, int32_t aboveLeft, int32_t above,
	int32_t scaler)
{
	uint32_t fromLeft;
	int32_t prediction;

	if (abs32(left - aboveLeft) < abs32(aboveLeft - above)) {
		prediction = above;
		fromLeft = 0;
	} else {
		prediction = left;
		fromLeft = 1;
	}
	block->dc[0] = m4vdecDcDivide(prediction, scaler);
	block->dc[1] = 0;
	block->dc[2] = 0;
	block->dc[3] = 0;
	return fromLeft;
}

/*
 * 0x84b09c: an inter macroblock's luma predicted from the last picture, a
 * block by its vector, in half samples (7.6.2)
 */
static void lumaPredict(M4vdecStripe *stripe)
{
	const uint8_t *frame = stripe->reference;
	uint8_t *luma = stripe->luma, *samples;
	int32_t x = stripe->x * 16, y = stripe->row * 16;
	uint32_t rounding = stripe->roundingType;
	const M4vdecMbRecord *mb = stripe->mb;
	M4vdecReference reference;
	int32_t i, vx, vy;

	for (i = 0; i < 4; i++) {
		samples = luma + (i & 1) * 8 + (i & 2) * 64;
		vx = mb->vectors[i].x;
		vy = mb->vectors[i].y;
		referenceFetch(stripe, frame, (vx >> 1) + x + (i & 1) * 8,
			(vy >> 1) + y + (i & 2) * 4, &reference);
		if (!(vx & 1)) {
			if (!(vy & 1))
				m4vdecPredictFull(&reference, samples);
			else if (rounding)
				m4vdecPredictDown(&reference, samples);
			else
				m4vdecPredictDownRounded(&reference, samples);
		} else if (!(vy & 1)) {
			if (rounding)
				m4vdecPredictAcross(&reference, samples);
			else
				m4vdecPredictAcrossRounded(&reference, samples);
		} else {
			m4vdecPredictBoth(&reference, samples, rounding ? 1 : 2);
		}
	}
}

/*
 * 0x84b170: a block's 9 x 9 reference samples at (x, y) in the frame's
 * 16 x 16 tiles, read where they are inside the picture, its edges
 * repeated outside (the unrestricted vectors of 7.6.4)
 */
static void referenceFetch(const M4vdecStripe *stripe, const uint8_t *frame, int32_t x, int32_t y,
	M4vdecReference *reference)
{
	int32_t width = stripe->width, height = stripe->height;
	int32_t frameWidth = (uint16_t)stripe->frameWidth;
	const uint8_t *first, *second;
	int32_t insideX, insideY;

	insideX = min32(max32(x, 0), width - 9);
	insideY = min32(max32(y, 0), height - 9);
	first = frame + (insideY & ~15) * frameWidth + (insideX & ~15) * 16 + (insideY & 15) * 16 +
		(insideX & 8);
	second = insideX & 8 ? first + 248 : first + 8;
	m4vdecReferenceLoad(reference, first, second, frameWidth * 16 - 256, insideY, insideX & 7);
	if (x < 0) {
		if (x >= -7)
			m4vdecReferenceLeftEdge(reference, -x);
		else
			m4vdecReferenceLeftOutside(reference);
	} else if (x > width - 9) {
		if (x < width - 1)
			m4vdecReferenceRightEdge(reference, -(x + 9));
		else
			m4vdecReferenceRightOutside(reference);
	}
	if (y < 0)
		m4vdecReferenceTopEdge(reference, y);
	else if (y > height - 9)
		m4vdecReferenceBottomEdge(reference, y - height + 9);
	m4vdecReferenceNext(reference);
}

/* the chroma's dc_scaler by quantiser (0x8fc1d8) */
static const uint8_t g_m4vdecDcScalerChroma[32] = {
	0, 8, 8, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14,
	14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 20, 21, 22, 23, 24, 25,
};

static void chromaIntraShort(M4vdecStripe *stripe);
static void chromaIntraPredicted(M4vdecStripe *stripe);
static void chromaResidual(M4vdecStripe *stripe);
static void chromaInterPredictors(M4vdecStripe *stripe);

/* 0x84acec: an intra macroblock's chroma, by the picture's header (ccr8 as lumaIntra's) */
static void chromaIntra(M4vdecStripe *stripe)
{
	if (stripe->shortHeader)
		chromaIntraShort(stripe);
	else
		chromaIntraPredicted(stripe);
}

/* 0x84ad06: an inter macroblock's chroma residual, its predictors reset */
static void chromaInter(M4vdecStripe *stripe)
{
	if ((stripe->kind >> 8) & 3)
		chromaResidual(stripe);
	chromaInterPredictors(stripe);
}

/* 0x84ad36: a chroma block's IDCT */
static void chromaIdct(M4vdecBlock *block)
{
	m4vdecChromaIdctRows(block);
	m4vdecChromaIdctColumns(block);
}

/* 0x84ad4a: dcDequantize's, with the chroma's kernel */
static void chromaDcDequantize(M4vdecBlock *block, uint32_t even, uint32_t quant, int32_t scaler)
{
	int32_t dc;

	dc = max32(min32(block->coef[0][0] * scaler, 2047), -2048);
	m4vdecChromaDequantize(block, even, quant);
	block->coef[0][0] = dc;
}

/* 0x84ad7c: the block's DC kept as its row's and its column's first */
static void dcKeep(const M4vdecBlock *block, int16_t *row, int16_t *column)
{
	int32_t dc = block->coef[0][0];

	column[0] = dc;
	row[0] = dc;
}

/* 0x84ad8a: a short header's intra chroma: Cb then Cr, the DCs by 8 */
static void chromaIntraShort(M4vdecStripe *stripe)
{
	uint32_t quant = (uint8_t)stripe->quant, even = ~quant & 1;
	uint32_t coded = (stripe->kind >> 16) & 3;
	const int16_t *coefficients = stripe->chromaCursor->coefficients[0];
	uint8_t *chroma = stripe->chroma;
	M4vdecBlock block;
	int32_t k;

	for (k = 0; k < 2; k++) {
		if (coded & 2) {
			m4vdecChromaBlockLoad(&block, coefficients);
			coefficients += 64;
		} else {
			m4vdecChromaBlockLoadDc(&block, coefficients);
			coefficients += 4;
		}
		chromaDcDequantize(&block, even, quant, 8);
		chromaIdct(&block);
		m4vdecChromaBlockStore(&block, chroma);
		if (!k) {
			chroma += 64;
			stripe->chromaCursor->coefficients[0] = coefficients;
			coded <<= 1;
			coefficients = stripe->chromaCursor->coefficients[1];
		}
	}
	stripe->chromaCursor->coefficients[1] = coefficients;
}

/* a chroma block's coefficients, all of them or its DC alone */
static inline const int16_t *chromaBlockLoad(M4vdecBlock *block, const int16_t *coefficients,
	uint32_t full)
{
	if (full) {
		m4vdecChromaBlockLoad(block, coefficients);
		return coefficients + 64;
	}
	m4vdecChromaBlockLoadDc(block, coefficients);
	return coefficients + 4;
}

/*
 * 0x84ae14: an intra macroblock's chroma with AC/DC prediction: Cb then
 * Cr, each from the macroblocks to the left and above
 */
static void chromaIntraPredicted(M4vdecStripe *stripe)
{
	uint32_t quant = (uint8_t)stripe->quant, aboveQuant = (uint8_t)stripe->aboveQuant;
	uint32_t leftQuant = (uint8_t)stripe->leftQuant, even = ~quant & 1;
	uint32_t acPrediction = stripe->kind & M4V_KIND_AC_PREDICTION;
	uint32_t coded = (stripe->kind >> 16) & 3;
	int32_t scaler = g_m4vdecDcScalerChroma[quant];
	uint8_t *chroma = stripe->chroma;
	const int16_t *coefficients = stripe->chromaCursor->coefficients[0];
	M4vdecPredictors *slot;
	int16_t *column, *row, *left;
	M4vdecBlock block;
	int32_t k, above;
	uint32_t fromLeft;

	for (k = 0; k < 2; k++) {
		slot = stripe->chromaPredictors;
		row = slot->rows[k];
		left = slot->columns[k];
		column = slot[1].columns[k];
		coefficients = chromaBlockLoad(&block, coefficients, coded & 2);
		above = row[0];
		fromLeft = dcPredict(&block, left[0], stripe->chromaCursor->aboveLeftDc[k], above,
			scaler);
		if (!acPrediction) {
			dcAdd(&block);
		} else if (fromLeft) {
			acPredictLeft(&block, left, quant, leftQuant);
		} else {
			acPredictTop(&block, row, quant, aboveQuant);
		}
		m4vdecChromaPredictorsStore(&block, row, column);
		chromaDcDequantize(&block, even, quant, scaler);
		stripe->chromaCursor->aboveLeftDc[k] = above;
		dcKeep(&block, row, column);
		chromaIdct(&block);
		m4vdecChromaBlockStore(&block, chroma);
		coded <<= 1;
		if (!k) {
			chroma += 64;
			stripe->chromaCursor->coefficients[0] = coefficients;
			coefficients = stripe->chromaCursor->coefficients[1];
		}
	}
	stripe->chromaCursor->coefficients[1] = coefficients;
}

/* 0x84af58: an inter macroblock's chroma residual, added to its prediction */
static void chromaResidual(M4vdecStripe *stripe)
{
	uint8_t *chroma = stripe->chroma;
	uint32_t full = (stripe->kind >> 16) & 3, coded = (stripe->kind >> 8) & 3;
	uint32_t quant = (uint8_t)stripe->quant, even = ~quant & 1;
	const int16_t *coefficients;
	M4vdecBlock block;

	if (coded & 2) {
		coefficients = stripe->chromaCursor->coefficients[0];
		coefficients = chromaBlockLoad(&block, coefficients, full & 2);
		m4vdecChromaDequantize(&block, even, quant);
		chromaIdct(&block);
		m4vdecChromaBlockAdd(&block, chroma);
		stripe->chromaCursor->coefficients[0] = coefficients;
	}
	if (coded & 1) {
		coefficients = stripe->chromaCursor->coefficients[1];
		coefficients = chromaBlockLoad(&block, coefficients, full & 1);
		m4vdecChromaDequantize(&block, even, quant);
		chromaIdct(&block);
		m4vdecChromaBlockAdd(&block, chroma + 64);
		stripe->chromaCursor->coefficients[1] = coefficients;
	}
}

/* 0x84b012: an inter macroblock's chroma predictors, for intra ones next to it: reset */
static void chromaInterPredictors(M4vdecStripe *stripe)
{
	M4vdecPredictors *slot;
	uint32_t k, i;

	for (k = 0; k < 2; k++) {
		slot = stripe->chromaPredictors;
		stripe->chromaCursor->aboveLeftDc[k] = slot->rows[k][0];
		slot[1].columns[k][0] = DC_RESET;
		slot->rows[k][0] = DC_RESET;
		for (i = 1; i < 8; i++) {
			slot[1].columns[k][i] = 0;
			slot->rows[k][i] = 0;
		}
	}
}

static void chromaReferenceFetch(const M4vdecStripe *stripe, const uint8_t *frame, int32_t x,
	int32_t y, uint32_t component, M4vdecReference *reference);

/* 0x84b260: an inter macroblock's Cb and Cr predicted by its chroma vector */
static void chromaPredict(M4vdecStripe *stripe)
{
	const uint8_t *frame = stripe->referenceChroma;
	uint8_t *chroma = stripe->chroma;
	uint32_t rounding = stripe->roundingType;
	M4vVector vector = stripe->mb->chromaVector;
	int32_t x = (vector.x >> 1) + stripe->x * 8, y = (vector.y >> 1) + stripe->row * 8;
	M4vdecReference reference;

	chromaReferenceFetch(stripe, frame, x, y, 0, &reference);
	if (!(vector.x & 1)) {
		if (!(vector.y & 1)) {
			m4vdecChromaPredictFull(&reference, chroma);
			chromaReferenceFetch(stripe, frame, x, y, 1, &reference);
			m4vdecChromaPredictFull(&reference, chroma + 64);
		} else if (rounding) {
			m4vdecChromaPredictDown(&reference, chroma);
			chromaReferenceFetch(stripe, frame, x, y, 1, &reference);
			m4vdecChromaPredictDown(&reference, chroma + 64);
		} else {
			m4vdecChromaPredictDownRounded(&reference, chroma);
			chromaReferenceFetch(stripe, frame, x, y, 1, &reference);
			m4vdecChromaPredictDownRounded(&reference, chroma + 64);
		}
	} else if (!(vector.y & 1)) {
		if (rounding) {
			m4vdecChromaPredictAcross(&reference, chroma);
			chromaReferenceFetch(stripe, frame, x, y, 1, &reference);
			m4vdecChromaPredictAcross(&reference, chroma + 64);
		} else {
			m4vdecChromaPredictAcrossRounded(&reference, chroma);
			chromaReferenceFetch(stripe, frame, x, y, 1, &reference);
			m4vdecChromaPredictAcrossRounded(&reference, chroma + 64);
		}
	} else {
		m4vdecChromaPredictBoth(&reference, chroma, rounding ? 1 : 2);
		chromaReferenceFetch(stripe, frame, x, y, 1, &reference);
		m4vdecChromaPredictBoth(&reference, chroma + 64, rounding ? 1 : 2);
	}
}

/*
 * 0x84b3a0: referenceFetch's for a chroma component: the frame's 8 x 8
 * tiles hold a tile's Cb then its Cr; a picture of one tile across or down
 * repeats its tile.
 */
static void chromaReferenceFetch(const M4vdecStripe *stripe, const uint8_t *frame, int32_t x,
	int32_t y, uint32_t component, M4vdecReference *reference)
{
	int32_t width = stripe->chromaWidth, height = stripe->chromaHeight;
	int32_t pitch = (uint16_t)stripe->chromaFrameWidth, stride;
	const uint8_t *first, *second;
	int32_t insideX, insideY;

	insideX = min32(max32(x, 0), width - 9);
	insideY = min32(max32(y, 0), height - 9);
	stride = pitch * 8 - 64;
	if (width < 9)
		insideX = 0;
	if (height < 9) {
		insideY = 0;
		stride = -8;
	}
	first = frame + (insideX & ~7) * 16 + (insideY & ~7) * pitch + (insideY & 7) * 8 +
		component * 64;
	second = width < 9 ? first : first + 128;
	m4vdecChromaReferenceLoad(reference, first, second, stride, insideY, insideX & 7);
	if (x < 0) {
		if (x >= -7)
			m4vdecChromaReferenceLeftEdge(reference, -x);
		else
			m4vdecChromaReferenceLeftOutside(reference);
	} else if (x > width - 9) {
		if (width < 9)
			m4vdecChromaReferenceNarrow(reference);
		if (x < width - 1)
			m4vdecChromaReferenceRightEdge(reference, -(x + 9));
		else
			m4vdecChromaReferenceRightOutside(reference);
	}
	if (y < 0)
		m4vdecChromaReferenceTopEdge(reference, y);
	else if (y > height - 9)
		m4vdecChromaReferenceBottomEdge(reference, height < 9 ? y : y - height + 9);
	m4vdecChromaReferenceNext(reference);
}
