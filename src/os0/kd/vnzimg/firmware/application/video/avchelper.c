/*
 * The AVC decoder's part of the helper (0x861fe4-0x862660), and the
 * helper's memory: two job contexts, two commands, four slices and four
 * job buffers. A job takes a free command and a free slice, then runs as
 * VThread jobs on the helper's context: one to prepare it, one to load its
 * slice into the bitstream unit, one after the unit has run.
 */
#include "service.h"

#define SHARED_SIZE		128
#define JOB_SIZE		128
#define CONTEXT_SIZE		64
#define STREAM_SIZE		128
#define NAL_SIZE		1216

#define COMMANDS		2
#define COMMAND_SIZE		64
#define SLICES			4

#define FLAG_JOBS_DONE		0x00004000
#define FLAG_FINISH		0x00020000
#define FLAG_UNIT		0x00040000
#define FLAG_FINISHED		0x00400000

#define UNIT_RUN		16

#define SLICE_JOB_SIZE		192
#define PARAMETER_SET_SIZE	1088

#define NAL_IDLE		0x200000	/* nal->flags: nothing for the unit */
#define UNIT_ENDED		129		/* the unit's status: the stream ended */
#define RUN_SLICE_ENDED		3
#define ERROR_SLICES_ENDED	8201

#define ALIGN64(size)		(((size) + 63) & ~63)

/* the VThread jobs, by the slot's index on the context */
#define JOB_INDEX_PREPARE(slot)	((slot) * 8 + 1)
#define JOB_INDEX_SLICE(slot)	((slot) * 8 + 3)

void avcdecJobPrepare(uint32_t shared, uint32_t stream, uint32_t job, uint32_t unused);
void avcdecJobLoad(uint32_t shared, uint32_t stream, uint32_t job, uint32_t unused);
void avcdecJobSlice(uint32_t shared, uint32_t stream, uint32_t job, uint32_t unused);

/* the parser's parts the jobs run (the AVC engine's) */
void avcdecSlicePrepare(const VideoHelperShared *shared, const VideoStream *stream,
	VideoSliceJob *job);					/* 0x827fe4 */
void avcdecSliceSkip(const VideoHelperShared *shared, const VideoStream *stream,
	VideoSliceJob *job);					/* 0x828330 */
void avcdecSliceHeader(const VideoStream *stream, VideoSliceJob *job);	/* 0x82739c */
void avcdecSliceStart(const VideoStream *stream, VideoSliceJob *job);	/* 0x827914 */
void avcdecSliceLoad(const VideoStream *stream, VideoSliceJob *job);	/* 0x827d40 */
void avcdecSliceRun(const VideoStream *stream, VideoSliceJob *job);	/* 0x828378 */
void avcdecPictureOutput(const VideoStream *stream, VideoSliceJob *job,
	uint32_t flush);					/* 0x828a76 */
void avcdecPictureMark(VideoSliceJob *job);			/* 0x829d8c */
void avcdecPictureFinish(VideoSliceJob *job);			/* 0x829dd6 */

/* run a job on the helper's context until the context takes it */
static void runJob(VkVThreadContext *context, uint32_t index, VkJobFunction function,
	uint32_t shared, uint32_t argument, uint32_t argument2, uint32_t flags)
{
	VkVThreadContext *locked;
	uint32_t busy;

	do {
		locked = vkVThreadContextLock(context);
		busy = vkVThreadJobSetNotify(locked, index, 0, function, shared, argument,
			argument2, 0, ((VideoHelperShared *)shared)->thread, flags);
		vkVThreadContextUnlock(locked);
	} while (busy);
}

void avcdecHelperStart(VideoHelper *helper, VideoHelperShared *shared)
{
	helper->needCommand = 1;
}

/* a job buffer back from the engine: free its command and its slice */
void avcdecHelperJob(VideoHelper *helper, uint32_t address)
{
	VideoHelperShared *shared;
	VideoHelperJob *job;
	uint32_t entry;
	int32_t i;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	job = (VideoHelperJob *)vnzPublicOpenPrivate(address, JOB_SIZE);
	if (job->commandUsed) {
		uint32_t command = ((uint32_t *)job->slice)[1];

		job->commandUsed = 0;
		entry = shared->commands;
		for (i = 0; i < COMMANDS; i++) {
			if (entry == command) {
				helper->commandsInUse &= ~(1 << i);
				break;
			}
			entry += COMMAND_SIZE;
		}
	}
	if (job->sliceUsed) {
		job->sliceUsed = 0;
		entry = shared->slices;
		for (i = 0; i < SLICES; i++) {
			if (entry == job->slice) {
				helper->slicesInUse &= ~(1 << i);
				break;
			}
			entry += VIDEO_HELPER_SLICE_SIZE;
		}
	}
	vnzPublicClosePrivate((uint32_t)job, JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
}

/* the next job: a command and a slice for it, then its preparing job */
void avcdecHelperRun(VideoHelper *helper, uint32_t job)
{
	VideoHelperShared *shared;
	uint32_t command, context, slice;
	uint32_t index, argument, argument2;
	int32_t i, n;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	if (helper->needCommand) {
		command = shared->commands;
		context = shared->contexts;
		for (i = 0; i < COMMANDS; i++) {
			if (!(helper->commandsInUse & (1 << i))) {
				helper->commandsInUse |= 1 << i;
				break;
			}
			command += COMMAND_SIZE;
			context += CONTEXT_SIZE;
		}
		if (i < COMMANDS) {
			helper->needCommand = 0;
			shared->command = command;
			shared->context = context;
			/* the jobs held while there was none go again, last first */
			for (n = helper->pending - 1; n >= 0; n--)
				vkMessageSend(helper->jobQueue, videoHelperJob, helper->pendingJobs[n]);
			helper->pending = 0;
		} else {
			helper->pending++;
		}
	}
	if (helper->pending) {
		helper->pendingJobs[helper->pending - 1] = job;
	} else {
		if (helper->needSlice) {
			slice = shared->slices;
			for (i = 0; i < SLICES; i++) {
				if (!(helper->slicesInUse & (1 << i))) {
					helper->slicesInUse |= 1 << i;
					helper->needSlice = 0;
					shared->slice = slice;
					break;
				}
				slice += VIDEO_HELPER_SLICE_SIZE;
			}
		}
		shared->request = job;
		helper->pendingJobs[0] = job;
	}
	if (helper->state == 0 || helper->pending) {
		vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
		return;
	}
	index = JOB_INDEX_PREPARE(shared->slot);
	argument = shared->argument;
	argument2 = shared->argument2;
	runJob(helper->context, index, avcdecJobPrepare,
		vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE),
		argument, argument2, FLAG_FINISH);
}

/* the job is prepared: load its slice into the unit and run it, or skip it */
void avcdecHelperFinish(VideoHelper *helper)
{
	VideoHelperShared *shared;
	const VideoStream *stream;
	VideoNal *nal;
	VkThread *thread;
	uint32_t load = 0;
	uint32_t index, argument, argument2, address;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	stream = (const VideoStream *)vnzPublicOpenProtected(shared->argument, STREAM_SIZE);
	nal = (VideoNal *)vnzPublicOpenPrivate(stream->nal, NAL_SIZE);
	thread = shared->thread;
	if (!nal->skip)
		load = nal->load;
	if (nal->skip || (nal->flags & 0x200000)) {
		vnzThreadSetFlags(thread, FLAG_UNIT);
	} else {
		if (load)
			videoHelperLoad(shared, stream, nal);
		VIDEO_UNIT[VIDEO_UNIT_DESCRIPTORS] = nal->runMacroblocks;
		VIDEO_UNIT[VIDEO_UNIT_COMMAND] = UNIT_RUN;
	}
	vnzPublicClosePrivate((uint32_t)nal, NAL_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, STREAM_SIZE);
	argument = shared->argument;
	argument2 = shared->argument2;
	index = JOB_INDEX_SLICE(shared->slot);
	address = vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
	if (load)
		runJob(helper->context, index, avcdecJobLoad, address,
			argument, argument2, FLAG_FINISHED);
	else
		vnzThreadSetFlags(thread, FLAG_FINISHED);
}

/* the unit has run: the job after it, with the unit's status */
void avcdecHelperStep(VideoHelper *helper)
{
	VideoHelperShared *shared;
	uint32_t index, argument, argument2;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	shared->interruptStatus = *helper->interruptStatus;
	index = JOB_INDEX_SLICE(shared->slot);
	argument2 = shared->argument2;
	argument = shared->argument;
	runJob(helper->context, index, avcdecJobSlice,
		vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE),
		argument, argument2, FLAG_JOBS_DONE);
}

/* a context's memory, the same for both */
#define CONTEXT_MEMORY	(64 + 2048 + 128 + 64 + 4096 + 16384 + 8192 + 1024 + 2048 + 1024)

/* a job buffer's memory, by the command size */
static uint32_t jobMemory(uint32_t commandSize)
{
	return 2 * ALIGN64(16 * commandSize) + ALIGN64(commandSize) +
		ALIGN64(8 * commandSize) + ALIGN64(128 * commandSize) +
		ALIGN64(544 * commandSize) + 2 * ALIGN64(136 * commandSize) +
		ALIGN64(256 * commandSize) + 2 * ALIGN64(64 * commandSize);
}

uint32_t videoHelperQueryMemory(uint32_t commandSize, uint32_t horizontal)
{
	uint32_t rowSize = ((int32_t)horizontal / 16 + 1) * 64;

	return SLICES * jobMemory(commandSize) +
		2 * (CONTEXT_MEMORY + 3 * rowSize) + VIDEO_HELPER_FIXED;
}

/* lay the helper's memory out and give the job buffers to the helper */
void videoHelperInitialize(uint32_t memory, VideoService *service)
{
	VideoHelper *helper = (VideoHelper *)memory;
	VideoHelperShared *shared = (VideoHelperShared *)(memory + VIDEO_HELPER_SHARED);
	VideoHelperContext *context;
	VideoHelperJob *job;
	VkMessageQueue *jobQueue;
	uint32_t rowSize, commandSize, next, area;
	int32_t i;

	helper->shared = shared;
	helper->startQueue = service->helperQueue;
	helper->jobQueue = service->helperQueue2;
	helper->encoderQueue = service->helperQueue3;
	helper->context = service->encoderContext;
	shared->callback[0] = service->callback[0];
	shared->callback[1] = service->callback[1];
	shared->callback[2] = service->callback[2];
	shared->version = service->version;
	shared->contexts = memory + VIDEO_HELPER_CONTEXTS;
	shared->descriptors = (uint32_t)helper->descriptors;
	helper->interruptStatus = &helper->interruptWord;
	shared->tables[2] = memory + VIDEO_HELPER_TABLE1152;
	shared->tables[0] = memory + VIDEO_HELPER_TABLE128;
	shared->tables[4] = memory + VIDEO_HELPER_TABLE1088;
	shared->commands = memory + VIDEO_HELPER_COMMANDS;
	shared->tables[3] = memory + VIDEO_HELPER_TABLE128;
	shared->slices = memory + VIDEO_HELPER_SLICES;

	next = memory + VIDEO_HELPER_FIXED;
	rowSize = (((int16_t)service->horizontal >> 4) + 1) * 64;
	for (i = 0; i < 2; i++) {
		context = (VideoHelperContext *)vnzPublicOpenPrivate(
			memory + VIDEO_HELPER_CONTEXTS + i * CONTEXT_SIZE, CONTEXT_SIZE);
		vnzCacheInvalidateInside((uint32_t)context, CONTEXT_SIZE);
		context->header64 = next;
		context->common = memory + VIDEO_HELPER_COMMON;
		context->context = service->context[i];
		context->table2048 = next + 64;
		context->table128 = context->table2048 + 2048;
		context->lockArea = context->table128 + 128;
		*(volatile uint8_t **)context->lockArea = service->contextLock[i];
		area = context->lockArea + 64;
		context->table4096 = area;
		context->table16384 = area += 4096;
		context->table8192 = area += 16384;
		context->table1024 = area += 8192;
		context->table2048b = area += 1024;
		context->table1024b = area += 2048;
		next = area + 1024;
		context->row0 = next;
		context->row1 = next += rowSize;
		context->row2 = next += rowSize;
		next += rowSize;
		vnzPublicClosePrivate((uint32_t)context, CONTEXT_SIZE);
	}

	jobQueue = service->helperQueue2;
	commandSize = (int16_t)service->commandSize;
	for (i = 0; i < SLICES; i++) {
		job = (VideoHelperJob *)vnzPublicOpenPrivate(
			memory + VIDEO_HELPER_JOBS + i * JOB_SIZE, JOB_SIZE);
		vnzCacheInvalidateInside((uint32_t)job, JOB_SIZE);
		job->commandSize = commandSize;
		job->buffers[0] = next;
		job->codec = 0;
		job->sliceUsed = 0;
		job->commandUsed = 0;
		job->buffers[1] = next += ALIGN64(16 * commandSize);
		job->buffers[2] = next += ALIGN64(16 * commandSize);
		job->buffers[3] = next += ALIGN64(commandSize);
		job->buffers[4] = next += ALIGN64(8 * commandSize);
		job->buffers[5] = next += ALIGN64(128 * commandSize);
		job->buffers[6] = next += ALIGN64(544 * commandSize);
		job->buffers[7] = next += ALIGN64(136 * commandSize);
		job->buffers[8] = next += ALIGN64(136 * commandSize);
		job->buffers[9] = next += ALIGN64(256 * commandSize);
		job->buffers[10] = next += ALIGN64(64 * commandSize);
		next += ALIGN64(64 * commandSize);
		job->slice = memory + VIDEO_HELPER_SLICES + i * VIDEO_HELPER_SLICE_SIZE;
		vkMessageSend(jobQueue, videoHelperJob,
			vnzPublicClosePrivate((uint32_t)job, JOB_SIZE));
	}
}

/* before the unit runs: the slice's command, context and slice, and the parser's part */
void avcdecJobPrepare(uint32_t sharedAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t unused)
{
	const VideoHelperShared *shared;
	const VideoStream *stream;
	VideoSliceJob *job;
	VideoNal *nal;

	shared = (const VideoHelperShared *)vnzPublicOpenProtected(sharedAddress, SHARED_SIZE);
	stream = (const VideoStream *)vnzPublicOpenProtected(streamAddress, STREAM_SIZE);
	job = (VideoSliceJob *)vnzPublicOpenPrivate(jobAddress, SLICE_JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, SLICE_JOB_SIZE);
	nal = (VideoNal *)vnzPublicOpenPrivate(stream->nal, NAL_SIZE);
	job->parser.bits = &nal->bits;
	job->parser.nal = nal;
	if (nal->load) {
		nal->command = shared->command;
		nal->context = shared->context;
		nal->skip = 0;
		nal->slice = shared->slice;
	} else {
		nal->firstMacroblock += nal->runMacroblocks;
	}
	if (!nal->failed && !nal->skip)
		avcdecSlicePrepare(shared, stream, job);
	else
		avcdecSliceSkip(shared, stream, job);
	vnzPublicClosePrivate((uint32_t)nal, NAL_SIZE);
	vnzCachePurge((uint32_t)job, SLICE_JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)job, SLICE_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, STREAM_SIZE);
	vnzPublicCloseProtected((uint32_t)shared, SHARED_SIZE);
}

/* the slice's data for the unit, read with the picture's tables */
void avcdecJobLoad(uint32_t sharedAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t unused)
{
	uint32_t shared, parameterSet, extras, extrasSize;
	const VideoStream *stream;
	VideoAvcPicture *pictures;
	VideoSliceJob *job;
	VideoNal *nal;
	int32_t error;

	shared = vnzPublicOpenProtected(sharedAddress, SHARED_SIZE);
	stream = (const VideoStream *)vnzPublicOpenProtected(streamAddress, STREAM_SIZE);
	job = (VideoSliceJob *)vnzPublicOpenPrivate(jobAddress, SLICE_JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, SLICE_JOB_SIZE);
	nal = (VideoNal *)vnzPublicOpenPrivate(stream->nal, NAL_SIZE);
	pictures = (VideoAvcPicture *)vnzPublicOpenPrivate(stream->pictures,
		nal->pictures * sizeof(VideoAvcPicture));
	extrasSize = ALIGN64(nal->pictures * 16);
	extras = vnzPublicOpenPrivate(stream->pictureExtras, extrasSize);
	parameterSet = vnzPublicOpenProtected(stream->parameterSets +
		nal->parameterSet * PARAMETER_SET_SIZE, PARAMETER_SET_SIZE);
	job->pictures = pictures;
	job->parser.nal = nal;
	job->picture = &pictures[nal->picture];
	job->parameterSet = parameterSet;
	job->pictureExtras = extras;
	error = videoSetJump(job->jump);
	if (error) {
		nal->error = error;
		nal->failed = 1;
	} else {
		avcdecSliceLoad(stream, job);
		avcdecSliceRun(stream, job);
	}
	vnzPublicCloseProtected(parameterSet, PARAMETER_SET_SIZE);
	vnzPublicClosePrivate(extras, extrasSize);
	vnzPublicClosePrivate((uint32_t)pictures, nal->pictures * sizeof(VideoAvcPicture));
	vnzPublicClosePrivate((uint32_t)nal, NAL_SIZE);
	vnzCachePurge((uint32_t)job, SLICE_JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)job, SLICE_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, STREAM_SIZE);
	vnzPublicCloseProtected(shared, SHARED_SIZE);
}

/*
 * After the unit has run: how far it got, the next run (up to the next
 * slice, a row's rest or rowsPerRun rows), the next slice's header when the
 * run reaches it, and the picture's end. The engine gets the run in the job
 * buffer; the helper, the status and whether the picture is done.
 */
void avcdecJobSlice(uint32_t sharedAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t unused)
{
	VideoHelperShared *shared;
	const VideoStream *stream;
	VideoAvcPicture *pictures;
	VideoHelperJob *run;
	VideoSliceJob *job;
	VideoNal *nal;
	uint32_t extras, extrasSize, ended, finished, status;
	int32_t done, count, total, width, first, next, end, nextType;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate(sharedAddress, SHARED_SIZE);
	stream = (const VideoStream *)vnzPublicOpenProtected(streamAddress, STREAM_SIZE);
	job = (VideoSliceJob *)vnzPublicOpenPrivate(jobAddress, SLICE_JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, SLICE_JOB_SIZE);
	nal = (VideoNal *)vnzPublicOpenPrivate(stream->nal, NAL_SIZE);
	pictures = (VideoAvcPicture *)vnzPublicOpenPrivate(stream->pictures,
		nal->pictures * sizeof(VideoAvcPicture));
	extrasSize = ALIGN64(nal->pictures * 16);
	extras = vnzPublicOpenPrivate(stream->pictureExtras, extrasSize);
	run = (VideoHelperJob *)vnzPublicOpenPrivate(shared->request, JOB_SIZE);
	job->parser.nal = nal;
	job->parser.bits = &nal->bits;
	job->pictures = pictures;
	job->picture = &pictures[nal->picture];
	job->pictureExtras = extras;
	job->first = shared->first;

	if (nal->flags & NAL_IDLE) {
		status = nal->doneStatus;
		done = nal->doneMacroblocks;
	} else {
		status = VIDEO_UNIT[VIDEO_UNIT_RUN_STATUS];
		done = VIDEO_UNIT[VIDEO_UNIT_RUN_DONE];
	}
	ended = nal->skip || status == UNIT_ENDED;
	if (nal->failed || ended) {
		done = 0;
		status = RUN_SLICE_ENDED;
	}
	count = nal->runMacroblocks - done;
	total = nal->macroblocks;
	width = nal->widthInMacroblocks;
	first = nal->firstMacroblock;
	next = nal->nextFirstMacroblock;
	end = first + count;
	run->codec = 0;
	run->load = nal->load;
	run->failed = nal->failed;
	run->slice = nal->slice;
	run->nextX = next % width;
	run->nextY = next / width;
	finished = end >= total;
	nal->load = 0;
	nextType = nal->nextType;

	if (end >= total) {
		if (status == 0 || nextType == NAL_SLICE || nextType == NAL_IDR)
			ended = 1;
	} else if (end >= next || status != 0) {
		if (end < next || ((nextType == NAL_SLICE || nextType == NAL_IDR) && status == 0)) {
			/* the run goes on to the next slice */
			ended = 1;
			count -= end - next;
		}
		if (first % width) {
			if (count > width - first % width)
				count = width - first % width;
		} else if (count > width * nal->rowsPerRun)
			count = width * nal->rowsPerRun;
		end = first + count;
		finished = end >= total;
		status = end < next ? 0 : RUN_SLICE_ENDED;
	}

	/* the run reaches the next slice: read its header */
	if (next < total && end >= next) {
		if (videoSetJump(job->jump) == 0) {
			videoNalNext(stream, &job->parser, 1);
			videoBitsBegin(stream, &job->parser);
			avcdecSliceHeader(stream, job);
			avcdecSliceStart(stream, job);
			nal->failed = 0;
		} else {
			nal->nextType = -1;
			nal->nextFirstMacroblock = total;
			nal->firstMacroblock = next;
			nal->failed = 1;
		}
		nal->load = 1;
	}
	if (ended) {
		job->picture->incomplete = 1;
		nal->state.ended = ended;
		nal->error = ERROR_SLICES_ENDED;
		nal->skip = 1;
	}
	if (finished) {
		if (videoSetJump(job->jump) == 0) {
			avcdecPictureFinish(job);
			if (nal->refIdc)
				avcdecPictureMark(job);
		}
		if (nal->refIdc)
			nal->referenceInfo = job->picture->referenceInfo;
		avcdecPictureOutput(stream, job, 0);
		run->pictureInfo[0] = nal->pictureInfo[1];
		run->pictureInfo[1] = nal->pictureInfo[0];
		nal->lastPictureCount = nal->pictureCount;
		nal->pictureCount++;
	}
	nal->runMacroblocks = count;
	run->commandUsed = finished;
	run->firstX = first % width;
	run->firstY = first / width;
	run->endX = end % width;
	run->endY = end / width;
	run->count = count;
	run->first = first;
	run->sliceUsed = end >= total;
	run->ended = ended;
	vnzPublicClosePrivate((uint32_t)run, JOB_SIZE);
	vnzPublicClosePrivate(extras, extrasSize);
	vnzPublicClosePrivate((uint32_t)pictures, nal->pictures * sizeof(VideoAvcPicture));
	vnzPublicClosePrivate((uint32_t)nal, NAL_SIZE);
	shared->finished = finished;
	shared->status = status;
	vnzCachePurge((uint32_t)job, SLICE_JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)job, SLICE_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, STREAM_SIZE);
	vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
}
