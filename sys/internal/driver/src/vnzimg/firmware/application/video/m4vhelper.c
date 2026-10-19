/*
 * The MPEG-4 decoder's part of the helper (0x867564-0x86834c): avchelper.c's
 * for MPEG-4 part 2. A job takes a free command (no slice) and runs as two
 * VThread jobs on the helper's context: one to parse up to the packet's
 * macroblocks and load them into the bitstream unit, one after the unit has
 * run (or after the engine decoded them itself) that tells the engine the
 * run and finds where the next one starts.
 */
#include "service.h"

#define STREAM_SIZE		64
#define UNIT_SIZE		256
#define VOP_SIZE		64
#define PACKET_SIZE		256
#define M4V_JOB_SIZE		1408
#define TABLE_SIZE		64
#define COMMAND_RECORD_SIZE	64
#define FRAME_SIZE		128

#define TABLES			2	/* the tables alternate */
#define TABLE_DECODING		1
#define TABLE_DECODED		4
#define FIELDS_FRAME		2

#define RUN_ENDED		3
#define ERROR_RESYNC		8192	/* the parser lost its place: on to a resync marker */
#define ERROR_PACKET		0x8007
#define ERROR_TIME		8229	/* the first times are negative */
#define ERROR_TIME_ORDER	8230	/* the pts is before the dts */
#define ERRORS_FATAL		0xc000

/* the markers looked for, by their bits */
#define SHORT_VIDEO_START_BITS	22
#define SHORT_VIDEO_START	0x20	/* short_video_start_marker */
#define SHORT_VIDEO_END		0x3f	/* short_video_end_marker */
#define GOB_START_BITS		17
#define START_CODE_PREFIX_BITS	24
#define RESYNC_BITS(packet)	((packet)->predicted ? (packet)->fcode + 16 : 17)

/* the VThread jobs, by the slot's index on the context */

void m4vdecJobPrepare(uint32_t shared, uint32_t stream, uint32_t job, uint32_t unused);
void m4vdecJobSlice(uint32_t shared, uint32_t stream, uint32_t job, uint32_t unused);
void m4vdecJobSliceSoftware(uint32_t shared, uint32_t stream, uint32_t job);
void m4vdecPictureBegin(const VideoM4vStream *stream, VideoM4vJob *job);
void m4vdecPictureEnd(const VideoM4vStream *stream, VideoM4vJob *job);

/* the parser's parts the jobs run (the MPEG-4 engine's) */
void m4vdecVopHeader(const VideoM4vStream *stream, VideoM4vJob *job);	/* 0x843736 */
void m4vdecShortHeader(const VideoM4vStream *stream, VideoM4vJob *job);	/* 0x84394e */
void m4vdecGobHeader(VideoM4vJob *job);					/* 0x843b90 */
void m4vdecPacketHeader(const VideoM4vStream *stream, VideoM4vJob *job);	/* 0x843c16 */
void m4vdecRunUnit(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job);						/* 0x843e06 */
void m4vdecRunNotCoded(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job);						/* 0x844044 */
void m4vdecRunSoftware(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job);						/* 0x8440f2 */
void m4vdecRunFailed(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job);						/* 0x84415c */
uint32_t m4vdecMacroblocksContext(VideoM4vJob *job);			/* 0x846044 */
int32_t m4vdecMacroblocksDecode(VideoM4vUnit *unit, VideoBitstream *bits,
	uint32_t context, int32_t count);				/* 0x8458e6 */
void m4vdecParserError(const VideoM4vStream *stream, VideoM4vJob *job, uint32_t stage,
	int32_t code);							/* 0x847184 */

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

void m4vdecHelperStart(VideoHelper *helper, VideoHelperShared *shared)
{
	helper->needCommand = 1;
}

/* a job buffer back from the engine: free its command */
void m4vdecHelperJob(VideoHelper *helper, uint32_t address)
{
	VideoHelperShared *shared;
	VideoHelperJob *job;
	uint32_t entry;
	int32_t i;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	job = (VideoHelperJob *)vnzPublicOpenPrivate(address, JOB_SIZE);
	if (job->commandUsed) {
		job->commandUsed = 0;
		entry = shared->commands;
		for (i = 0; i < COMMANDS; i++) {
			if (entry == job->command) {
				helper->commandsInUse &= ~(1 << i);
				break;
			}
			entry += COMMAND_SIZE;
		}
	}
	vnzPublicClosePrivate((uint32_t)job, JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
}

/* the next job: a command for it, then its preparing job */
void m4vdecHelperRun(VideoHelper *helper, uint32_t job)
{
	VideoHelperShared *shared;
	uint32_t command, context;
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
			context += COMMAND_SIZE;
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
		shared->request = job;
		helper->pendingJobs[0] = job;
	}
	if (helper->state == 0 || helper->pending) {
		vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
		return;
	}
	index = JOB_INDEX_PREPARE(shared->slot);
	argument2 = shared->argument2;
	argument = shared->argument;
	runJob(helper->context, index, m4vdecJobPrepare,
		vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE),
		argument, argument2, HELPER_FLAG_FINISH);
}

/* the job is prepared: run the unit on its macroblocks, or not */
void m4vdecHelperFinish(VideoHelper *helper)
{
	VideoHelperShared *shared;
	const VideoM4vStream *stream;
	VideoM4vUnit *unit;
	int32_t count;
	uint32_t load;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	stream = (const VideoM4vStream *)vnzPublicOpenProtected(shared->argument, STREAM_SIZE);
	unit = (VideoM4vUnit *)vnzPublicOpenPrivate(stream->unit, UNIT_SIZE);
	if (unit->failed || unit->vop->notCoded || !unit->packet->unitRuns) {
		vnzThreadSetFlags(shared->thread, HELPER_FLAG_UNIT);
	} else {
		load = unit->load;
		if (load)
			m4vdecUnitLoad(shared, stream, unit);
		count = unit->runMacroblocks;
		if (unit->vop->shortHeader && unit->packet->runEnd < count)
			count = unit->packet->runEnd;
		VIDEO_UNIT[VIDEO_UNIT_DESCRIPTORS] = count;
		VIDEO_UNIT[VIDEO_UNIT_COMMAND] = VIDEO_UNIT_RUN;
	}
	vnzThreadSetFlags(shared->thread, HELPER_FLAG_FINISHED);
	vnzPublicClosePrivate((uint32_t)unit, UNIT_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, STREAM_SIZE);
	vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
}

/* the unit has run: the job after it, with the unit's status */
void m4vdecHelperStep(VideoHelper *helper)
{
	VideoHelperShared *shared;
	const VideoM4vStream *stream;
	const VideoM4vUnit *unit;
	uint32_t index, argument, argument2;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	stream = (const VideoM4vStream *)shared->argument;
	unit = (const VideoM4vUnit *)stream->unit;
	argument2 = shared->argument2;
	if (!unit->vop->notCoded)
		shared->interruptStatus = *helper->interruptStatus;
	index = JOB_INDEX_SLICE(shared->slot);
	argument = shared->argument;
	runJob(helper->context, index, m4vdecJobSlice,
		vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE),
		argument, argument2, HELPER_FLAG_JOBS_DONE);
}

/*
 * The first VThread job: the headers up to the packet's macroblocks, then
 * what runs them (the unit, or the engine itself).
 */
void m4vdecJobPrepare(uint32_t sharedAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t unused)
{
	const VideoM4vStream *stream;
	const VideoHelperShared *shared;
	VideoM4vJob *job;
	VideoM4vUnit *unit;
	VideoM4vVop *vop;
	VideoM4vPacket *packet;
	VideoM4vTable *tables;
	uint32_t tablesSize;
	int32_t error, resync;

	stream = (const VideoM4vStream *)vnzPublicOpenProtected(streamAddress, STREAM_SIZE);
	job = (VideoM4vJob *)vnzPublicOpenPrivate(jobAddress, M4V_JOB_SIZE);
	unit = (VideoM4vUnit *)vnzPublicOpenPrivate(stream->unit, UNIT_SIZE);
	unit->vop = (VideoM4vVop *)vnzPublicOpenPrivate((uint32_t)unit->vop, VOP_SIZE);
	unit->packet = (VideoM4vPacket *)vnzPublicOpenPrivate((uint32_t)unit->packet, PACKET_SIZE);
	shared = (const VideoHelperShared *)vnzPublicOpenProtected(sharedAddress, SHARED_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, M4V_JOB_SIZE);
	job->bits = &unit->bits;
	job->unit = unit;
	vop = unit->vop;
	m4vdecBitsRestore(&unit->registers);
	if (unit->load) {
		unit->command = shared->command;
		unit->context = shared->context;
		error = videoSetJump(job->jump);
		if (!error) {
			packet = unit->packet;
			unit->failed = 0;
			resync = RESYNC_BITS(packet);
			if (vop->shortHeader == 1) {
				if (m4vdecBitsPeek(&unit->bits, SHORT_VIDEO_START_BITS) ==
				    SHORT_VIDEO_START && vop->pictureHeader == 1) {
					m4vdecShortHeader(stream, job);
					vop->pictureHeader = 0;
				} else {
					m4vdecGobHeader(job);
					unit->firstMacroblock += unit->runMacroblocks;
				}
			} else if (m4vdecBitsPeekAligned(&unit->bits, resync) == 1) {
				m4vdecPacketHeader(stream, job);
				unit->firstMacroblock += unit->runMacroblocks;
			} else if (packet->vopHeader) {
				m4vdecVopHeader(stream, job);
				unit->packet->vopHeader = 0;
			} else {
				m4vdecParserError(stream, job, 2, ERROR_PACKET);
			}
		} else {
			if (!vop->shortHeader && !unit->packet->vopHeader)
				unit->firstMacroblock += unit->runMacroblocks;
			if (vop->shortHeader == 1 && !vop->pictureHeader)
				unit->firstMacroblock += unit->runMacroblocks;
			if (unit->packet->vopHeader == 1)
				unit->packet->vopHeader = 0;
			if (vop->pictureHeader == 1)
				vop->pictureHeader = 0;
		}
	} else {
		error = 0;
		unit->firstMacroblock += unit->runMacroblocks;
	}
	if (unit->firstMacroblock == 0) {
		/* a new picture: its tables */
		tablesSize = unit->tables * TABLE_SIZE;
		tables = (VideoM4vTable *)vnzPublicOpenPrivate(stream->tables, tablesSize);
		job->tables = tables;
		job->table = &tables[unit->table];
		unit->flags = (uint8_t)unit->packet->predicted;
		m4vdecPictureBegin(stream, job);
		vnzPublicClosePrivate((uint32_t)tables, unit->tables * TABLE_SIZE);
	} else if (unit->load) {
		unit->flags |= (uint8_t)unit->packet->predicted;
	}
	if (error & ERRORS_FATAL)
		m4vdecRunFailed(shared, stream, job);
	else if (unit->failed)
		;
	else if (unit->vop->notCoded)
		m4vdecRunNotCoded(shared, stream, job);
	else if (!unit->packet->unitRuns)
		m4vdecRunSoftware(shared, stream, job);
	else {
		unit->dataBits = m4vdecBitsTell(stream, unit);
		m4vdecRunUnit(shared, stream, job);
	}
	if (error) {
		unit->error = error;
		unit->failed = 1;
	}
	m4vdecBitsSave(&unit->registers);
	vnzPublicCloseProtected((uint32_t)shared, SHARED_SIZE);
	unit->vop = (VideoM4vVop *)vnzPublicClosePrivate((uint32_t)unit->vop, VOP_SIZE);
	unit->packet = (VideoM4vPacket *)vnzPublicClosePrivate((uint32_t)unit->packet,
		PACKET_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, STREAM_SIZE);
	vnzPublicClosePrivate((uint32_t)job, M4V_JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)unit, UNIT_SIZE);
}

/*
 * After a failed run: on through the stream to the next marker that can
 * start a run (a GOB or the picture's end in short headers, a resync
 * marker or the next start code otherwise).
 */
static void findResync(VideoM4vUnit *unit, VideoBitstream *bits, int32_t count, int32_t resync)
{
	if (unit->vop->shortHeader) {
		do {
			m4vdecBitsByteAlign(bits);
			if (unit->firstMacroblock + count < unit->macroblocks) {
				if (m4vdecBitsPeekAligned(bits, GOB_START_BITS) == 1)
					break;
			} else {
				if (m4vdecBitsPeekAligned(bits, SHORT_VIDEO_START_BITS) ==
				    SHORT_VIDEO_START)
					break;
				if (m4vdecBitsPeekAligned(bits, SHORT_VIDEO_START_BITS) ==
				    SHORT_VIDEO_END)
					break;
			}
		} while (bits->remaining >= 1);
	} else {
		do {
			m4vdecBitsByteAlign(bits);
			if (unit->firstMacroblock + count >= unit->macroblocks &&
			    m4vdecBitsPeekAligned(bits, START_CODE_PREFIX_BITS) == 0)
				break;
			if (unit->firstMacroblock + count < unit->macroblocks &&
			    m4vdecBitsPeekAligned(bits, resync) == 1)
				break;
		} while (bits->remaining >= 1);
	}
}

/* the short header's GOB start, past stuffing */
static void gobAlign(VideoBitstream *bits)
{
	if (m4vdecBitsPeek(bits, GOB_START_BITS) != 1 &&
	    m4vdecBitsPeekAligned(bits, GOB_START_BITS) == 1)
		m4vdecBitsByteAlign(bits);
}

/* the run told to the engine */
static void requestFill(VideoHelperJob *request, const VideoHelperShared *shared,
	VideoM4vUnit *unit, int32_t status)
{
	request->codec = SERVICE_CODEC_M4VDEC;
	request->sliceUsed = status != 0;
	request->ended = status == VIDEO_UNIT_ENDED;
	request->load = unit->load;
	request->slice = shared->context;
	request->command = shared->command;
	request->mode = unit->packet->mode;
	request->fcode = unit->packet->fcode;
	unit->load = 0;
}

static void requestRun(VideoHelperJob *request, VideoM4vUnit *unit, int32_t count,
	uint32_t last, uint32_t ended)
{
	int32_t width = unit->widthInMacroblocks;
	int32_t first = unit->firstMacroblock;
	int32_t end = count + first;

	unit->runMacroblocks = count;
	request->count = count;
	request->commandUsed = last;
	request->first = first;
	request->firstX = first % width;
	request->firstY = first / width;
	request->endX = end % width;
	request->endY = end / width;
	request->ended = ended;
}

/*
 * The second VThread job: the run's result (the unit's, or what the engine
 * decoded itself) to the engine, and the next run found.
 */
void m4vdecJobSlice(uint32_t sharedAddress, uint32_t streamAddress, uint32_t jobAddress,
	uint32_t unused)
{
	VideoHelperShared *shared;
	const VideoM4vStream *stream;
	VideoM4vJob *job;
	VideoM4vUnit *unit;
	VideoM4vVop *vop;
	VideoM4vTable *tables;
	VideoHelperJob *request;
	VideoBitstream *bits;
	int32_t done, status, count = 0, resync, n;
	uint32_t ended, last;

	unit = (VideoM4vUnit *)((const VideoM4vStream *)streamAddress)->unit;
	if (!unit->packet->unitRuns) {
		m4vdecJobSliceSoftware(sharedAddress, streamAddress, jobAddress);
		return;
	}
	shared = (VideoHelperShared *)vnzPublicOpenPrivate(sharedAddress, SHARED_SIZE);
	stream = (const VideoM4vStream *)vnzPublicOpenProtected(streamAddress, STREAM_SIZE);
	job = (VideoM4vJob *)vnzPublicOpenPrivate(jobAddress, M4V_JOB_SIZE);
	unit = (VideoM4vUnit *)vnzPublicOpenPrivate(stream->unit, UNIT_SIZE);
	unit->vop = (VideoM4vVop *)vnzPublicOpenPrivate((uint32_t)unit->vop, VOP_SIZE);
	unit->packet = (VideoM4vPacket *)vnzPublicOpenPrivate((uint32_t)unit->packet, PACKET_SIZE);
	tables = (VideoM4vTable *)vnzPublicOpenPrivate(stream->tables, unit->tables * TABLE_SIZE);
	request = (VideoHelperJob *)vnzPublicOpenPrivate(shared->request, JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, M4V_JOB_SIZE);
	job->unit = unit;
	job->bits = &unit->bits;
	job->tables = tables;
	job->table = &tables[unit->table];
	bits = job->bits;
	vop = unit->vop;
	m4vdecBitsRestore(&unit->registers);

	if (unit->failed) {
		done = 0;
		status = RUN_ENDED;
	} else if (vop->notCoded) {
		done = unit->doneMacroblocks;
		status = unit->doneStatus;
	} else {
		status = VIDEO_UNIT[VIDEO_UNIT_RUN_STATUS];
		done = VIDEO_UNIT[VIDEO_UNIT_RUN_DONE];
		if (status == VIDEO_UNIT_ENDED) {
			m4vdecBitsSkipUnit(shared, bits);
			VIDEO_UNIT[0] = 1;
		}
	}
	if (unit->vop->shortHeader) {
		if (status != VIDEO_UNIT_ENDED) {
			count = unit->packet->runEnd - done;
			if (unit->firstMacroblock + count >= unit->macroblocks) {
				status = RUN_ENDED;
			} else {
				/* the rest of the GOB, decoded here */
				m4vdecBitsSkipUnit(shared, bits);
				gobAlign(bits);
				status = m4vdecMacroblocksDecode(unit, bits,
					m4vdecMacroblocksContext(job), count);
				if (!status)
					VIDEO_UNIT[VIDEO_UNIT_BITS] = 0;
			}
		}
	} else {
		count = unit->runMacroblocks - done;
	}
	ended = unit->failed || status == VIDEO_UNIT_ENDED;
	resync = RESYNC_BITS(unit->packet);
	if (ended) {
		if (unit->runOffset)
			n = unit->widthInMacroblocks - unit->runOffset;
		else
			n = unit->runLength;
		if (unit->vop->notCoded)
			unit->packet->skipped = 0;
		status = RUN_ENDED;
		count = n;
		if (unit->macroblocks - unit->firstMacroblock < count)
			count = unit->macroblocks - unit->firstMacroblock;
		if (unit->error == ERROR_RESYNC)
			findResync(unit, bits, count, resync);
	}

	n = unit->firstMacroblock + count - 1;
	if (n < 0)
		n = 0;
	last = n + 1 >= unit->macroblocks;
	requestFill(request, shared, unit, status);
	if (last && !status) {
		status = RUN_ENDED;
		ended = 1;
		count = unit->macroblocks - unit->firstMacroblock;
		request->ended = 1;
		request->sliceUsed = 1;
		last = 1;
	}
	if (!last && status) {
		if (!unit->vop->notCoded && !unit->vop->shortHeader)
			m4vdecBitsSkipUnit(shared, bits);
		unit->load = 1;
	}
	if (ended) {
		job->table->failed = 1;
		unit->failed = 1;
		if (!unit->error)
			unit->error = ERROR_SLICES_ENDED;
	}
	if (last) {
		m4vdecPictureEnd(stream, job);
		request->pictureInfo[0] = unit->pictureInfo[1];
		request->pictureInfo[1] = unit->pictureInfo[0];
		unit->pictureCount++;
	}
	requestRun(request, unit, count, last, ended);
	m4vdecBitsSave(&unit->registers);
	shared->status = status;
	shared->finished = last;
	vnzPublicClosePrivate((uint32_t)tables, unit->tables * TABLE_SIZE);
	unit->vop = (VideoM4vVop *)vnzPublicClosePrivate((uint32_t)unit->vop, VOP_SIZE);
	unit->packet = (VideoM4vPacket *)vnzPublicClosePrivate((uint32_t)unit->packet,
		PACKET_SIZE);
	vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, STREAM_SIZE);
	vnzPublicClosePrivate((uint32_t)job, M4V_JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)unit, UNIT_SIZE);
	vnzPublicClosePrivate((uint32_t)request, JOB_SIZE);
}

/* the second job when the engine decoded the run itself (no unit) */
void m4vdecJobSliceSoftware(uint32_t sharedAddress, uint32_t streamAddress, uint32_t jobAddress)
{
	VideoHelperShared *shared;
	const VideoM4vStream *stream;
	VideoM4vJob *job;
	VideoM4vUnit *unit;
	VideoM4vTable *tables;
	VideoHelperJob *request;
	VideoBitstream *bits;
	const VideoM4vVop *vop;
	const VideoM4vPacket *packet;
	int32_t done, status, count, resync;
	uint32_t ended, last;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate(sharedAddress, SHARED_SIZE);
	stream = (const VideoM4vStream *)vnzPublicOpenProtected(streamAddress, STREAM_SIZE);
	job = (VideoM4vJob *)vnzPublicOpenPrivate(jobAddress, M4V_JOB_SIZE);
	unit = (VideoM4vUnit *)vnzPublicOpenPrivate(stream->unit, UNIT_SIZE);
	tables = (VideoM4vTable *)vnzPublicOpenPrivate(stream->tables, unit->tables * TABLE_SIZE);
	request = (VideoHelperJob *)vnzPublicOpenPrivate(shared->request, JOB_SIZE);
	vnzCacheInvalidateInside((uint32_t)job, M4V_JOB_SIZE);
	job->unit = unit;
	job->bits = &unit->bits;
	job->tables = tables;
	job->table = &tables[unit->table];
	bits = job->bits;
	/* the VOP and packet headers are not opened here: their own addresses */
	vop = unit->vop;
	packet = unit->packet;
	m4vdecBitsRestore(&unit->registers);

	if (unit->failed) {
		status = RUN_ENDED;
		done = 0;
	} else {
		done = unit->doneMacroblocks;
		status = unit->doneStatus;
	}
	if (vop->shortHeader) {
		gobAlign(bits);
		count = packet->runEnd - done;
		status = m4vdecMacroblocksDecode(unit, bits, m4vdecMacroblocksContext(job), count);
	} else {
		count = unit->runMacroblocks - done;
	}
	ended = unit->failed || status == VIDEO_UNIT_ENDED;
	resync = RESYNC_BITS(packet);
	if (ended) {
		status = RUN_ENDED;
		count = unit->runLength - unit->runOffset;
		if (unit->macroblocks - unit->firstMacroblock < count)
			count = unit->macroblocks - unit->firstMacroblock;
		if (unit->error == ERROR_RESYNC) {
			if (vop->shortHeader) {
				do {
					m4vdecBitsByteAlign(bits);
					if (m4vdecBitsPeekAligned(bits, SHORT_VIDEO_START_BITS) ==
					    SHORT_VIDEO_START)
						break;
					if (m4vdecBitsPeekAligned(bits, SHORT_VIDEO_START_BITS) ==
					    SHORT_VIDEO_END)
						break;
				} while (bits->remaining >= 1);
			} else {
				findResync(unit, bits, count, resync);
			}
		}
	}

	last = unit->firstMacroblock + count >= unit->macroblocks;
	requestFill(request, shared, unit, status);
	if (last && !status) {
		status = RUN_ENDED;
		ended = 1;
		count = unit->macroblocks - unit->firstMacroblock;
		request->ended = 1;
		request->sliceUsed = 1;
		last = 1;
	}
	if (!last && status)
		unit->load = 1;
	if (ended) {
		job->table->failed = 1;
		unit->failed = 1;
		if (!unit->error)
			unit->error = ERROR_SLICES_ENDED;
	}
	if (last) {
		m4vdecPictureEnd(stream, job);
		request->pictureInfo[0] = unit->pictureInfo[1];
		request->pictureInfo[1] = unit->pictureInfo[0];
		unit->pictureCount++;
	}
	requestRun(request, unit, count, last, ended);
	m4vdecBitsSave(&unit->registers);
	shared->status = status;
	shared->finished = last;
	vnzPublicClosePrivate((uint32_t)tables, unit->tables * TABLE_SIZE);
	vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
	vnzPublicCloseProtected((uint32_t)stream, STREAM_SIZE);
	vnzPublicClosePrivate((uint32_t)job, M4V_JOB_SIZE);
	vnzPublicClosePrivate((uint32_t)unit, UNIT_SIZE);
	vnzPublicClosePrivate((uint32_t)request, JOB_SIZE);
}

/* a time a frame's increment on from the last, wrapped at 2^34 - 1 */
static VideoTime timeNext(VideoM4vUnit *unit, VideoTime last)
{
	VideoTime time = last + unit->timeIncrement;

	if (time >= VIDEO_TIME_WRAP) {
		time -= VIDEO_TIME_WRAP;
		unit->wrapped = 1 - unit->wrapped;
	}
	return time;
}

/*
 * A new picture: its command, its table, and its times. The first picture
 * takes the stream's first times when the unit has none; later ones without
 * a dts get one a frame on from the last.
 */
void m4vdecPictureBegin(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	VideoM4vTable *table = job->table;
	VideoM4vCommand *command;
	uint32_t pictureSize2 = unit->pictureSize2;
	uint32_t chromaOffset = unit->chromaOffset;
	uint16_t height = unit->height;
	uint16_t width = unit->width;
	uint32_t fields;

	command = (VideoM4vCommand *)vnzPublicOpenPrivate(unit->command, COMMAND_RECORD_SIZE);
	command->table = unit->table;
	command->width = width;
	command->height = height;
	command->pictureSize = unit->pictureSize;
	command->chromaOffset = chromaOffset;
	command->pictureSize2 = pictureSize2;
	command->address = table->address;
	command->frames = unit->frames;
	vnzPublicClosePrivate((uint32_t)command, COMMAND_RECORD_SIZE);

	table->state = TABLE_DECODING;
	table->codedWidth = unit->widthInMacroblocks << 4;
	table->width = width;
	table->codedHeight = unit->heightInMacroblocks << 4;
	table->height = height;
	table->reserved = 0;
	table->info = 0;
	table->reserved2[1] = 0;
	table->reserved2[0] = 0;
	table->concealed = 0;
	table->failed = 0;
	table->info2 = unit->info2;
	if (stream->pts < stream->dts)
		m4vdecParserError(0, job, 3, ERROR_TIME_ORDER);

	if (unit->pictureCount == 0) {
		if (unit->firstPts < 0 || unit->firstDts < 0) {
			unit->firstPts = 0;
			unit->firstDts = 0;
			m4vdecParserError(0, job, 3, ERROR_TIME);
		}
		if (stream->dts == VIDEO_TIME_NONE || stream->pts == VIDEO_TIME_NONE) {
			table->dts = unit->firstDts;
			table->pts = stream->pts << 1;
			if (unit->timeIncrement == -1)
				table->time = VIDEO_TIME_NONE;
			else
				table->time = table->dts;
		} else {
			table->dts = stream->dts << 1;
			table->pts = stream->pts << 1;
			table->time = table->dts;
		}
		fields = 0;
	} else {
		table->dts = stream->dts << 1;
		table->pts = stream->pts << 1;
		if (stream->dts != VIDEO_TIME_NONE)
			table->time = table->dts;
		else if (unit->timeIncrement == -1)
			table->time = VIDEO_TIME_NONE;
		else
			table->time = timeNext(unit, unit->lastTime);
		fields = unit->tableFields + FIELDS_FRAME;
	}
	table->fieldCount = fields;
	unit->lastTime = table->time;
	unit->tableFields = table->fieldCount;
}

/* the picture's record, from its table; returns the table */
int32_t m4vdecFrameOut(const VideoM4vStream *stream, VideoM4vJob *job, VideoM4vUnit *unit,
	int32_t index)
{
	const VideoM4vTable *tables = job->tables;
	const VideoM4vTable *table = &tables[index];
	uint32_t chromaOffset = unit->chromaOffset;
	VideoM4vFrame *frame;
	VideoTime pts, base;
	uint32_t fields;

	frame = (VideoM4vFrame *)vnzPublicOpenPrivate(unit->frames + index * FRAME_SIZE,
		FRAME_SIZE);
	vnzCacheInvalidateInside((uint32_t)frame, FRAME_SIZE);
	frame->luma = table->address;
	frame->chroma = table->address + chromaOffset;
	frame->width = table->codedWidth;
	frame->height = table->codedHeight;
	frame->chromaWidth = frame->width / 2;
	frame->chromaHeight = frame->height / 2;
	frame->displayWidth = table->width;
	frame->displayHeight = table->height;
	frame->number2 = unit->frameCount;
	frame->number = unit->frameCount;
	frame->vopInfo4 = unit->vop->info4;
	frame->vopInfo[0] = unit->vop->info[0];
	frame->vopInfo[1] = unit->vop->info[1];
	frame->vopInfo[2] = unit->vop->info[2];
	frame->vopInfo[3] = unit->vop->info2;
	frame->data = unit->vop->data;
	frame->frameRate = unit->vop->frameRate;
	frame->vopInfo3 = unit->vop->info3;
	frame->reserved = 0;
	frame->reserved3 = 0;
	frame->flags = unit->flags;

	/* the pts given, or one a frame on from the last (the unit's first for the first) */
	pts = table->pts >> 1;
	base = unit->frameCount == 0 ? unit->firstPts : unit->lastPts;
	if (pts != VIDEO_TIME_NONE) {
		frame->pts = table->pts;
	} else if (unit->timeIncrement == -1) {
		frame->pts = VIDEO_TIME_NONE;
	} else {
		frame->pts = base + unit->timeIncrement;
		if (frame->pts > VIDEO_TIME_WRAP) {
			frame->pts -= VIDEO_TIME_WRAP;
			unit->wrapped = 1;
		}
	}
	fields = unit->frameCount == 0 ? FIELDS_FRAME : unit->frameFields + FIELDS_FRAME;
	unit->frameFields = fields;
	frame->fields = fields;
	unit->lastPts = frame->pts;
	frame->pts >>= 1;
	frame->dts = table->time >> 1;
	frame->errors = 0;
	frame->fieldCount = table->fieldCount;
	if (table->failed)
		frame->errors |= 1;
	if (table->concealed)
		frame->errors |= 2;
	frame->info = table->info;
	frame->info2 = table->info2;
	vnzPublicClosePrivate((uint32_t)frame, FRAME_SIZE);
	return index;
}

/* the picture decoded: its record, and the other table next */
void m4vdecPictureEnd(const VideoM4vStream *stream, VideoM4vJob *job)
{
	VideoM4vUnit *unit = job->unit;
	VideoM4vTable *tables = job->tables;
	int32_t index = unit->table;

	unit->frame = (VideoM4vFrame *)(unit->frames + index * FRAME_SIZE);
	m4vdecFrameOut(stream, job, unit, index);
	tables[index].state = TABLE_DECODED;
	unit->pictureInfo[0] = index;
	unit->pictureInfo[1] = 1;
	unit->lastTable = unit->table;
	unit->frameCount++;
	unit->table = (index + 1) % TABLES;
}
