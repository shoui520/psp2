/*
 * The AVC encoder's part of the helper (0x86432c-0x864dcc). A picture is
 * encoded in jobs of rows: each takes a command and a slice, the encoder
 * fills them (the preparing job), the unit or the encoder writes the slice
 * (the encoding job), and the step after it reads how the unit finished.
 */
#include "service.h"

#define SHARED_SIZE		128
#define JOB_SIZE		128
#define INFO_SIZE		64
#define WRITER_SIZE		192
#define ENCODE_JOB_SIZE		256
#define COMMAND_SIZE		64
#define NALS_SIZE		192

#define COMMANDS		2
#define SLICES			4

#define FLAG_JOBS_DONE		0x00004000
#define FLAG_FINISH		0x00020000
#define FLAG_UNIT		0x00040000
#define FLAG_ENCODER_FINISH	0x00800000

#define CODEC_AVCENC		2

/* the VThread jobs, by the slot's index on the context */
#define JOB_INDEX_PREPARE(slot)	((slot) * 8 + 1)
#define JOB_INDEX_ENCODE(slot)	((slot) * 8 + 2)
#define JOB_INDEX_STEP(slot)	((slot) * 8 + 3)

/* the unit's registers for the encoder */
#define UNIT_RESET		0x00
#define UNIT_VERSION		0x03
#define UNIT_MODE		0x04
#define UNIT_RUN		7	/* the command: 16 runs the slice */
#define UNIT_COUNT		8
#define UNIT_RUN_STATUS		10	/* 16 overflow, 129 the stream ended */
#define UNIT_RUN_DONE		11
#define UNIT_WIDTH		0x40
#define UNIT_BUFFER(i)		(0x42 + (i) * 2)
#define UNIT_PARAMETER(i)	(0x43 + (i) * 2)
#define UNIT_ENCODER		0x80
#define UNIT_PICTURE		0x83
#define UNIT_PICTURE2		0x84
#define UNIT_SLICE		0x85
#define UNIT_SLICE2		0x86
#define UNIT_BUFFERS		8

#define UNIT_MODE_ENCODE	0x000f0100
#define UNIT_PICTURE_ENABLE	0x01000000
#define UNIT_COMMAND_RUN	16
#define UNIT_STATUS_OVERFLOW	16
#define UNIT_STATUS_ENDED	129

#define WRITER_OVERFLOW		4096
#define WRITER_ENDED		0x8001

void avcencJobPrepare(uint32_t shared, uint32_t info, uint32_t job, uint32_t unused);
void avcencJobEncode(uint32_t shared, uint32_t info, uint32_t job, uint32_t request);
void avcencJobStep(uint32_t shared, uint32_t info, uint32_t job, uint32_t unused);

/* the AVC encoder's parts the jobs run */
void avcencStreamBegin(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job);	/* 0x8354f2 */
void avcencStreamEnd(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job);	/* 0x835546 */
void avcencCabacLoad(VideoAvcEncodeJob *job, VideoAvcWriter *writer);		/* 0x874448 */
void avcencCabacStart(VideoAvcWriter *writer);					/* 0x8744d2 */
void avcencCabacSave(VideoAvcWriter *writer);					/* 0x874498 */
void avcencRunOpen(const VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoHelperJob *run);							/* 0x8661bc */
void avcencSliceData(VideoAvcEncodeJob *job, const VideoHelperJob *run);	/* 0x865a60 */
void avcencRunClose(const VideoHelperShared *shared, VideoAvcEncodeJob *job,
	VideoHelperJob *run);							/* 0x86623a */
void avcencCabacFlush(VideoAvcEncodeJob *job);					/* 0x866126 */

/* the header writer's (avcencheader.c) */
void avcencWriterBegin(const VideoAvcOptions *options, VideoAvcEncodeJob *job, uint32_t keepBits);
void avcencCabacZeroWords(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job);
void avcencUnitLoad(VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoAvcWriter *writer);
void avcencUnitFinish(VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoAvcWriter *writer);
void avcencUnitNalEnd(VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoAvcWriter *writer);
void avcencWriteSei(const VideoAvcOptions *options, VideoAvcEncodeJob *job);
void avcencWriteSliceHeader(const VideoAvcOptions *options, VideoAvcEncodeJob *job,
	uint32_t firstMb);

/* run a job on the helper's context until the context takes it */
static void runJob(VkVThreadContext *context, uint32_t index, VkJobFunction function,
	uint32_t shared, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t flags)
{
	VkVThreadContext *locked;
	uint32_t busy;

	do {
		locked = vkVThreadContextLock(context);
		busy = vkVThreadJobSetNotify(locked, index, 0, function, shared, argument,
			argument2, argument3, ((VideoHelperShared *)shared)->thread, flags);
		vkVThreadContextUnlock(locked);
	} while (busy);
}

/* the jobs a picture takes, from the encoder's stream */
void avcencHelperStart(VideoHelper *helper, VideoHelperShared *shared)
{
	const VideoAvcStreamInfo *info = (const VideoAvcStreamInfo *)shared->argument;

	helper->needCommand = 1;
	helper->encoderJobs = ((const VideoAvcWriter *)info->writer)->jobs;
}

/* a job buffer back from the engine: free its command and its slice */
void avcencHelperJob(VideoHelper *helper, uint32_t address)
{
	const VideoHelperShared *shared;
	VideoHelperJob *job;
	uint32_t entry, command;
	int32_t i;

	shared = (const VideoHelperShared *)vnzPublicOpenProtected((uint32_t)helper->shared,
		SHARED_SIZE);
	job = (VideoHelperJob *)vnzPublicOpenPrivate(address, JOB_SIZE);
	if (job->commandUsed) {
		command = ((const VideoHelperSlice *)job->slice)->command;
		job->commandUsed = 0;
		entry = shared->commands;
		for (i = 0; i < COMMANDS; i++) {
			if (entry == command) {
				helper->commandsInUse &= ~(1 << i);
				break;
			}
			entry += COMMAND_SIZE;
		}
		entry = shared->slices;
		for (i = 0; i < SLICES; i++) {
			if (entry == job->slice) {
				helper->slicesInUse &= ~(1 << i);
				break;
			}
			entry += VIDEO_HELPER_SLICE_SIZE;
		}
	}
	if (job->sliceUsed)
		job->sliceUsed = 0;
	vnzPublicClosePrivate((uint32_t)job, JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)shared, SHARED_SIZE);
}

/* the next job: a command and a slice for it, then its preparing job */
void avcencHelperRun(VideoHelper *helper, uint32_t job)
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
			context += 64;
		}
		if (i < COMMANDS) {
			helper->needCommand = 0;
			shared->command = command;
			shared->context = context;
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
	shared->hold = helper->hold;
	index = JOB_INDEX_PREPARE(shared->slot);
	argument2 = shared->argument2;
	argument = shared->argument;
	runJob(helper->context, index, avcencJobPrepare,
		vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE),
		argument, argument2, 0, FLAG_FINISH);
}

/* the engine's message: encode the job (on the encoder's queue) */
void avcencHelperEncode(uint32_t helperAddress, uint32_t argument)
{
	VideoHelper *helper = (VideoHelper *)helperAddress;
	const VideoHelperShared *shared;
	uint32_t index, sharedArgument, sharedArgument2;

	helper->encoderArgument = argument;
	shared = (const VideoHelperShared *)vnzPublicOpenProtected((uint32_t)helper->shared,
		SHARED_SIZE);
	index = JOB_INDEX_ENCODE(shared->slot);
	sharedArgument2 = shared->argument2;
	sharedArgument = shared->argument;
	runJob(helper->context, index, avcencJobEncode,
		vnzPublicCloseProtected((uint32_t)shared, SHARED_SIZE),
		sharedArgument, sharedArgument2, argument, FLAG_ENCODER_FINISH);
}

/* the job is encoded: the unit takes the stream and runs, or is not needed */
void avcencHelperFinish(VideoHelper *helper, uint32_t argument)
{
	VideoHelperShared *shared;
	const VideoAvcStreamInfo *info;
	VideoAvcWriter *writer;
	const VideoHelperJob *run;
	VkThread *thread;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(shared->argument, INFO_SIZE);
	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(info->writer, WRITER_SIZE);
	run = (const VideoHelperJob *)vnzPublicOpenProtected(argument, JOB_SIZE);
	thread = shared->thread;
	if (!helper->hold && run->load)
		avcencUnitLoad(shared, info, writer);
	if (writer->noUnit || helper->hold) {
		vnzThreadSetFlags(thread, FLAG_UNIT);
	} else {
		VIDEO_UNIT[UNIT_COUNT] = (uint16_t)run->count;
		VIDEO_UNIT[UNIT_RUN] = UNIT_COMMAND_RUN;
	}
	vnzPublicCloseProtected((uint32_t)run, JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
}

/* the unit has run: the step after it, with the unit's status */
void avcencHelperStep(VideoHelper *helper)
{
	VideoHelperShared *shared;
	uint32_t index, argument, argument2;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	shared->encoderArgument = helper->encoderArgument;
	shared->interruptStatus = *helper->interruptStatus;
	index = JOB_INDEX_STEP(shared->slot);
	argument2 = shared->argument2;
	argument = shared->argument;
	runJob(helper->context, index, avcencJobStep,
		vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE),
		argument, argument2, 0, FLAG_JOBS_DONE);
}

/* the encoder stops: the unit's NAL unit ends */
void avcencHelperStop(VideoHelper *helper)
{
	VideoHelperShared *shared;
	const VideoAvcStreamInfo *info;
	VideoAvcWriter *writer;

	shared = (VideoHelperShared *)vnzPublicOpenProtected((uint32_t)helper->shared, SHARED_SIZE);
	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(shared->argument, INFO_SIZE);
	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(info->writer, WRITER_SIZE);
	if (!writer->noUnit) {
		if (helper->hold)
			avcencUnitNalEnd(shared, info, writer);
		else
			avcencUnitFinish(shared, info, writer);
	}
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
	vnzPublicCloseProtected((uint32_t)shared, SHARED_SIZE);
}

/* the job's rows: a group's next rows, or its rest and the next group */
void avcencJobPrepare(uint32_t sharedAddress, uint32_t infoAddress, uint32_t jobAddress,
	uint32_t unused)
{
	const VideoHelperShared *shared;
	const VideoAvcStreamInfo *info;
	VideoAvcCommand *command;
	VideoHelperSlice *slice;
	VideoAvcWriter *writer;
	VideoHelperJob *run;
	uint32_t work, rows, left, width, group;
	int32_t first;
	int32_t i;

	shared = (const VideoHelperShared *)vnzPublicOpenProtected(sharedAddress, SHARED_SIZE);
	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(infoAddress, INFO_SIZE);
	work = vnzPublicOpenPrivate(jobAddress, ENCODE_JOB_SIZE);
	run = (VideoHelperJob *)vnzPublicOpenPrivate(shared->request, JOB_SIZE);
	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(info->writer, WRITER_SIZE);
	first = writer->firstMacroblock;
	if (first == 0) {
		/* a new picture: its command and its slice */
		command = (VideoAvcCommand *)vnzPublicOpenPrivate(shared->command, COMMAND_SIZE);
		for (i = 0; i < VIDEO_AVC_UNIT_PARAMETERS; i++)
			command->parameters[i] = writer->unitParameters[i];
		command->widthInMacroblocks = writer->widthInMacroblocks;
		command->heightInMacroblocks = writer->heightInMacroblocks;
		command->mode = writer->mode;
		vnzPublicClosePrivate((uint32_t)command, COMMAND_SIZE);
		slice = (VideoHelperSlice *)vnzPublicOpenPrivate(shared->slice, VIDEO_HELPER_SLICE_SIZE);
		slice->context = shared->context;
		slice->command = shared->command;
		vnzPublicClosePrivate((uint32_t)slice, VIDEO_HELPER_SLICE_SIZE);
	}
	run->codec = CODEC_AVCENC;
	run->first = first;
	run->firstX = 0;
	run->firstY = writer->row;
	run->slice = shared->slice;
	run->commandUsed = 0;
	run->load = writer->newGroup;
	rows = writer->rowsPerJob;
	left = writer->rowsLeft;
	width = writer->widthInMacroblocks;
	if (rows < left) {
		run->sliceUsed = 0;
		run->count = width * rows;
		writer->newGroup = 0;
		writer->firstMacroblock += width * rows;
		writer->rowsLeft = left - rows;
		writer->row += rows;
	} else {
		run->sliceUsed = 1;
		run->count = width * left;
		group = writer->group + 1;
		if (group >= info->groups) {
			run->commandUsed = 1;
		} else {
			writer->newGroup = 1;
			writer->firstMacroblock += width * left;
			writer->row += left;
			writer->group = group;
			writer->rowsLeft = info->groupRows[group];
		}
	}
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	vnzPublicClosePrivate((uint32_t)run, JOB_SIZE);
	vnzPublicClosePrivate(work, ENCODE_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
	vnzPublicCloseProtected((uint32_t)shared, SHARED_SIZE);
}

/* the unit set up for a picture's slices, then for the job's buffers */
static void unitSetup(const VideoHelperShared *shared, const VideoAvcWriter *writer,
	const VideoHelperJob *run)
{
	int32_t i;

	if (run->first == 0) {
		VIDEO_UNIT[UNIT_RESET] = 1;
		VIDEO_UNIT[UNIT_VERSION] = shared->version;
		VIDEO_UNIT[UNIT_MODE] = UNIT_MODE_ENCODE;
		VIDEO_UNIT[UNIT_ENCODER] = 1;
		VIDEO_UNIT[UNIT_PICTURE] = (((writer->widthInMacroblocks - 1) & 0x7f) << 8) |
			UNIT_PICTURE_ENABLE;
		VIDEO_UNIT[UNIT_PICTURE2] = 0;
		for (i = 0; i < VIDEO_AVC_UNIT_PARAMETERS; i++)
			VIDEO_UNIT[UNIT_PARAMETER(i)] = (uint16_t)writer->unitParameters[i];
		VIDEO_UNIT[UNIT_PARAMETER(VIDEO_AVC_UNIT_PARAMETERS)] =
			(uint16_t)writer->unitParameters[VIDEO_AVC_UNIT_PARAMETERS - 1];
	}
	VIDEO_UNIT[UNIT_WIDTH] = writer->widthInMacroblocks;
	for (i = 0; i < UNIT_BUFFERS; i++)
		VIDEO_UNIT[UNIT_BUFFER(i)] = vnzToPublic(run->buffers[i]);
	if (run->load) {
		VIDEO_UNIT[UNIT_SLICE] = (writer->sliceType & 3) << 24;
		VIDEO_UNIT[UNIT_SLICE2] = 0;
	}
}

/*
 * The job's slice: its headers written here, then its macroblocks by the
 * unit; or, while the helper holds the unit, all of it by the encoder.
 */
void avcencJobEncode(uint32_t sharedAddress, uint32_t infoAddress, uint32_t jobAddress,
	uint32_t request)
{
	const VideoHelperShared *shared;
	const VideoAvcStreamInfo *info;
	VideoAvcEncodeJob *job;
	VideoAvcWriter *writer;
	VideoHelperJob *run;
	uint32_t slice;
	VideoAvcNalList *nals;
	int32_t error;

	shared = (const VideoHelperShared *)vnzPublicOpenProtected(sharedAddress, SHARED_SIZE);
	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(infoAddress, INFO_SIZE);
	job = (VideoAvcEncodeJob *)vnzPublicOpenPrivate(jobAddress, ENCODE_JOB_SIZE);
	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(info->writer, WRITER_SIZE);
	job->stream = writer;
	if (shared->hold)
		job->cabacState = (uint32_t)writer->cabacState;
	run = (VideoHelperJob *)vnzPublicOpenPrivate(request, JOB_SIZE);
	if (run->load) {
		slice = vnzPublicOpenProtected(run->slice, VIDEO_HELPER_SLICE_SIZE);
		nals = (VideoAvcNalList *)vnzPublicOpenPrivate(info->nals, NALS_SIZE);
		job->nals = nals;
		job->picture = (VideoAvcPictureInfo *)slice;
		avcencStreamBegin(info, job);
		error = videoSetJump(job->jump);
		if (error) {
			writer->noUnit = error;
		} else {
			avcencWriterBegin(info, job, 0);
			if (run->first == 0) {
				job->sei[3] = 0;
				job->sei[2] = 0;
				job->sei[1] = 1;
				job->sei[0] = info->idr != 0;
				if (!info->noVui)
					avcencWriteSei(info, job);
				job->binCount = 0;
			}
			avcencWriteSliceHeader(info, job, (uint16_t)run->first);
		}
		avcencStreamEnd(info, job);
		vnzPublicClosePrivate((uint32_t)nals, NALS_SIZE);
		vnzPublicCloseProtected(slice, VIDEO_HELPER_SLICE_SIZE);
	}
	if (shared->hold) {
		slice = vnzPublicOpenProtected(run->slice, VIDEO_HELPER_SLICE_SIZE);
		job->picture = (VideoAvcPictureInfo *)slice;
		if (run->load)
			avcencCabacLoad(job, writer);
		avcencCabacStart(writer);
		avcencStreamBegin(info, job);
		error = videoSetJump(job->jump);
		if (error) {
			writer->noUnit = error;
		} else {
			avcencWriterBegin(info, job, 1);
			avcencRunOpen(shared, info, run);
			avcencSliceData(job, run);
			avcencRunClose(shared, job, run);
			if (run->commandUsed || run->sliceUsed) {
				avcencCabacFlush(job);
				if (run->commandUsed)
					avcencCabacZeroWords(info, job);
			}
		}
		avcencStreamEnd(info, job);
		avcencCabacSave(writer);
		vnzPublicCloseProtected(slice, VIDEO_HELPER_SLICE_SIZE);
	} else {
		unitSetup(shared, writer, run);
	}
	vnzPublicClosePrivate((uint32_t)run, JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	vnzPublicClosePrivate((uint32_t)job, ENCODE_JOB_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
	vnzPublicCloseProtected((uint32_t)shared, SHARED_SIZE);
}

/* how the unit finished; the picture done with its command's result */
void avcencJobStep(uint32_t sharedAddress, uint32_t infoAddress, uint32_t jobAddress,
	uint32_t unused)
{
	VideoHelperShared *shared;
	const VideoAvcStreamInfo *info;
	VideoAvcWriter *writer;
	const VideoHelperJob *run;
	const VideoHelperSlice *slice;
	uint32_t status, sliceDone;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate(sharedAddress, SHARED_SIZE);
	info = (const VideoAvcStreamInfo *)vnzPublicOpenProtected(infoAddress, INFO_SIZE);
	writer = (VideoAvcWriter *)vnzPublicOpenPrivate(info->writer, WRITER_SIZE);
	run = (const VideoHelperJob *)vnzPublicOpenPrivate(shared->encoderArgument, JOB_SIZE);
	if (!shared->hold) {
		status = VIDEO_UNIT[UNIT_RUN_STATUS];
		(void)VIDEO_UNIT[UNIT_RUN_DONE];
		if (status == UNIT_STATUS_OVERFLOW)
			writer->noUnit = WRITER_OVERFLOW;
		else if (status == UNIT_STATUS_ENDED)
			writer->noUnit = WRITER_ENDED;
	}
	sliceDone = run->sliceUsed != 0;
	if (run->commandUsed || writer->noUnit == WRITER_ENDED) {
		slice = (const VideoHelperSlice *)run->slice;
		writer->commandResult = ((const VideoAvcCommand *)slice->command)->result;
		shared->finished = 1;
	} else {
		shared->finished = 0;
	}
	shared->encoderDone = sliceDone;
	vnzPublicClosePrivate((uint32_t)run, JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)writer, WRITER_SIZE);
	vnzPublicCloseProtected((uint32_t)info, INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
}
