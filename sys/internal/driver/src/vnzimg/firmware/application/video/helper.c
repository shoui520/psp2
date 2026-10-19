/*
 * The service's helper (0x86120c-0x86187c): one thread that parses the
 * codecs' streams with the bitstream unit for the engine thread it is
 * given, a job at a time. An engine sends a start message; the helper then
 * takes the jobs the engine queues, runs the unit and the VThread jobs on
 * them, and answers by flags and messages until the engine is done with it.
 */
#include "service.h"

#define HELPER_SIZE			64

#define HELPER_IDLE			2
#define HELPER_WAITING			3
#define HELPER_RUNNING			4
#define HELPER_WORKING			5

#define HELPER_INTERRUPT		21	/* the bitstream unit's */
#define HELPER_INTERRUPT_LEVEL		15

/* the engine thread's flags the helper sets */
#define ENGINE_FLAG_HELPER		0x00000002
#define ENGINE_FLAG_ENCODER		0x00000008
#define ENGINE_FLAG_HELPER_DONE		0x00010000

/* the codec's parts of the helper */

/* the engines' messages: a parsed job back */
void avcdecDecoderParsed(uint32_t memory, uint32_t job);		/* 0x82cf94 */
void m4vdecDecoderParsed(uint32_t memory, uint32_t job);		/* 0x847390 */
void avcencStreamParsed(uint32_t memory, uint32_t job);		/* 0x835d0c */

static VideoHelper *helperBegin(uint32_t memory);
static void helperEnd(VideoHelper *helper);
static uint32_t helperWait(VideoHelper *helper, uint32_t working);
static void helperFinish(VideoHelper *helper);
static void helperStep(VideoHelper *helper);
static void helperKick(VideoHelper *helper);
static void helperRestart(VideoHelper *helper, uint32_t flags);
static void helperStop(VideoHelper *helper);

/* a decoder's jobs, until its engine lets the helper go */
static uint32_t decoderLoop(VideoHelper *helper, uint32_t flags)
{
	VkMessageQueue *engineQueue = helper->engineQueue;
	VkMessageQueue *jobQueue = helper->jobQueue;
	VkThread *thread = helper->shared->thread;

	for (;;) {
		if (helper->state == HELPER_WAITING &&
		    vkMessageReceive(jobQueue, (uint32_t)helper)) {
			if (helper->pending)
				vnzThreadSetFlags(thread, ENGINE_FLAG_HELPER);
			else
				helper->state = HELPER_WORKING;
		}
		if (helper->state == HELPER_WORKING) {
			if (flags & (HELPER_FLAG_RESTART | HELPER_FLAG_STOP)) {
				if (helper->codec == SERVICE_CODEC_AVCDEC)
					vkMessageSend(engineQueue, avcdecDecoderParsed,
						helper->pendingJobs[0]);
				else if (helper->codec == SERVICE_CODEC_M4VDEC)
					vkMessageSend(engineQueue, m4vdecDecoderParsed,
						helper->pendingJobs[0]);
			}
			if (flags & HELPER_FLAG_FINISH) {
				helperFinish(helper);
				helper->state = HELPER_RUNNING;
			} else if (flags & HELPER_FLAG_RESTART) {
				helperRestart(helper, flags);
				vnzThreadSetFlags(thread, ENGINE_FLAG_HELPER);
				helper->state = HELPER_WAITING;
			} else if (flags & HELPER_FLAG_STOP) {
				helperStop(helper);
				helper->state = HELPER_IDLE;
			} else if (flags & HELPER_FLAG_IDLE) {
				helper->state = HELPER_IDLE;
			}
		}
		if (helper->state == HELPER_RUNNING &&
		    (flags & (HELPER_FLAG_FINISHED | HELPER_FLAG_UNIT))) {
			if (!(flags & HELPER_FLAG_FINISHED))
				vnzThreadWaitFlags(HELPER_FLAG_FINISHED);
			else if (!(flags & HELPER_FLAG_UNIT))
				vnzThreadWaitFlags(HELPER_FLAG_UNIT);
			helperStep(helper);
			helper->state = HELPER_WORKING;
			vnzThreadWaitFlags(HELPER_FLAG_JOBS_DONE);
			helperKick(helper);
		}
		flags = helperWait(helper, 0);
		if (helper->state == HELPER_IDLE)
			break;
	}
	vnzThreadSetFlags(thread, ENGINE_FLAG_HELPER_DONE);
	return flags;
}

/* the encoder's jobs: the stream's, then the encoder's, until it lets go */
static uint32_t encoderLoop(VideoHelper *helper, uint32_t flags)
{
	VkMessageQueue *encoderQueue = helper->encoderQueue;
	VkMessageQueue *jobQueue = helper->jobQueue;
	VkMessageQueue *engineQueue = helper->engineQueue;
	VkThread *thread = helper->shared->thread;
	VkThread *engineThread = helper->shared->engineThread;
	uint32_t first = 1, working = 0;

	for (;;) {
		if (helper->state == HELPER_WAITING) {
			if (flags & HELPER_FLAG_UNIT) {
				helperStep(helper);
				helper->state = HELPER_WORKING;
				vnzThreadWaitFlags(HELPER_FLAG_JOBS_DONE);
				helperKick(helper);
			} else if (helper->encoderJobs >= 1 &&
				   vkMessageReceive(jobQueue, (uint32_t)helper)) {
				if (helper->pending) {
					vnzThreadSetFlags(thread, ENGINE_FLAG_HELPER);
				} else {
					helper->state = HELPER_WORKING;
					helper->encoderJobs--;
				}
			}
		}
		if (helper->state == HELPER_WAITING && first &&
		    vkMessageReceive(encoderQueue, (uint32_t)helper)) {
			first = 0;
			helper->state = HELPER_WORKING;
		}
		if (helper->state == HELPER_WORKING) {
			if (flags & HELPER_FLAG_FINISH) {
				vkMessageSend(engineQueue, avcencStreamParsed, helper->pendingJobs[0]);
				vnzThreadYield();
				vnzThreadSetFlags(thread, ENGINE_FLAG_HELPER);
				helper->state = HELPER_WAITING;
				working = 0;
			} else if (flags & HELPER_FLAG_ENCODER_FINISH) {
				helperFinish(helper);
				helper->state = HELPER_WAITING;
				working = 0;
			} else if (flags & HELPER_FLAG_RESTART) {
				helperRestart(helper, flags);
				vnzThreadSetFlags(thread, ENGINE_FLAG_ENCODER);
				first = 1;
				helper->state = HELPER_WAITING;
				working = 0;
			} else if (flags & HELPER_FLAG_STOP) {
				helperStop(helper);
				vnzThreadSetFlags(engineThread, ENGINE_FLAG_HELPER_DONE);
				helper->state = HELPER_IDLE;
				working = 0;
			} else {
				working = 1;
			}
			if (flags & (HELPER_FLAG_RESTART | HELPER_FLAG_STOP))
				vkMessageSend(jobQueue, videoHelperJob, helper->encoderArgument);
		}
		flags = helperWait(helper, working);
		if (helper->state == HELPER_IDLE)
			break;
	}
	vnzThreadSetFlags(thread, ENGINE_FLAG_HELPER_DONE);
	return flags;
}

/* the helper's thread: a start message, then the codec's loop */
void videoHelperThread(uint32_t memory, uint32_t argument2)
{
	VideoHelper *helper = helperBegin(memory);
	VkMessageQueue *startQueue = helper->startQueue;
	uint32_t flags;

	do {
		flags = helperWait(helper, 0);
		if (helper->state == HELPER_IDLE && vkMessageReceive(startQueue, (uint32_t)helper)) {
			helper->state = HELPER_WAITING;
			if (helper->codec == SERVICE_CODEC_AVCDEC ||
			    helper->codec == SERVICE_CODEC_M4VDEC)
				flags = decoderLoop(helper, flags);
			else if (helper->codec == SERVICE_CODEC_AVCENC)
				flags = encoderLoop(helper, flags);
		}
	} while (!(flags & HELPER_FLAG_EXIT));
	helper->state = 0;
	while (vkMessageReceive(helper->jobQueue, (uint32_t)helper))
		;
	helperEnd(helper);
}

static VideoHelper *helperBegin(uint32_t memory)
{
	VideoHelperShared *shared;
	VideoHelper *helper;

	helper = (VideoHelper *)vnzPublicOpenPrivate(memory, HELPER_SIZE);
	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	shared->thread = vnzThreadCurrent();
	shared->encoderDone = 0;
	shared->finished = 0;
	shared->status = 0;
	shared->ready = 1;
	shared->publicStream = 1;
	shared->request = 0;
	shared->slice = 0;
	shared->context = 0;
	shared->command = 0;
	vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
	helper->reserved = 0;
	helper->state = HELPER_IDLE;
	helper->needCommand = 0;
	helper->needSlice = 0;
	helper->pending = 0;
	helper->commandsInUse = 0;
	helper->slicesInUse = 0;
	helper->hold = 0;
	return helper;
}

static void helperEnd(VideoHelper *helper)
{
	vnzPublicClosePrivate((uint32_t)helper, HELPER_SIZE);
}

/* while working, the unit's interrupt is left for the job that waits for it */
static uint32_t helperWait(VideoHelper *helper, uint32_t working)
{
	if (working)
		return vnzThreadWaitFlags(HELPER_FLAGS & ~HELPER_FLAG_UNIT);
	return vnzThreadWaitFlags(HELPER_FLAGS);
}

/* the start message: an engine thread's job, the unit's interrupt taken */
void videoHelperStart(uint32_t address, uint32_t requestAddress)
{
	VideoHelper *helper = (VideoHelper *)address;
	const VideoHelperRequest *request;
	VideoHelperShared *shared;

	shared = (VideoHelperShared *)vnzPublicOpenPrivate((uint32_t)helper->shared, SHARED_SIZE);
	request = (const VideoHelperRequest *)vnzPublicOpenProtected(requestAddress, REQUEST_SIZE);
	if (shared->callback[0])
		((void (*)(uint32_t))shared->callback[0])((uint32_t)shared->callback[2]);
	helper->hold = (request->flags & VIDEO_HELPER_HOLD) != 0;
	helper->interrupt = vnzInterruptRegister(HELPER_INTERRUPT, HELPER_INTERRUPT_LEVEL, 0,
		videoHelperInterrupt, (uint32_t)helper->interruptStatus, 0);
	helper->engineQueue = request->engineQueue;
	shared->stream = request->stream;
	shared->slot = request->slot;
	shared->first = (request->flags & VIDEO_HELPER_FIRST) != 0;
	shared->publicStream = (request->flags & VIDEO_HELPER_PUBLIC) != 0;
	shared->engineThread = request->engineThread;
	shared->argument = request->argument;
	shared->argument2 = request->argument2;
	helper->needSlice = 1;
	helper->codec = request->codec;
	if (helper->codec == SERVICE_CODEC_AVCDEC)
		avcdecHelperStart(helper, shared);
	else if (helper->codec == SERVICE_CODEC_M4VDEC)
		m4vdecHelperStart(helper, shared);
	else if (helper->codec == SERVICE_CODEC_AVCENC)
		avcencHelperStart(helper, shared);
	vnzPublicCloseProtected((uint32_t)request, REQUEST_SIZE);
	vnzPublicClosePrivate((uint32_t)shared, SHARED_SIZE);
}

/* a job (on the job queue): the job's codec takes it, the helper's codec runs it */
void videoHelperJob(uint32_t address, uint32_t job)
{
	VideoHelper *helper = (VideoHelper *)address;
	uint8_t codec = *(const uint8_t *)job;

	if (codec == SERVICE_CODEC_AVCDEC)
		avcdecHelperJob(helper, job);
	else if (codec == SERVICE_CODEC_M4VDEC)
		m4vdecHelperJob(helper, job);
	else if (codec == SERVICE_CODEC_AVCENC)
		avcencHelperJob(helper, job);
	if (helper->codec == SERVICE_CODEC_AVCDEC)
		avcdecHelperRun(helper, job);
	else if (helper->codec == SERVICE_CODEC_M4VDEC)
		m4vdecHelperRun(helper, job);
	else if (helper->codec == SERVICE_CODEC_AVCENC)
		avcencHelperRun(helper, job);
}

static void helperFinish(VideoHelper *helper)
{
	vnzInterruptEnable(helper->interrupt);
	if (helper->codec == SERVICE_CODEC_AVCDEC)
		avcdecHelperFinish(helper);
	else if (helper->codec == SERVICE_CODEC_M4VDEC)
		m4vdecHelperFinish(helper);
	else if (helper->codec == SERVICE_CODEC_AVCENC)
		avcencHelperFinish(helper, helper->encoderArgument);
}

static void helperStep(VideoHelper *helper)
{
	if (helper->codec == SERVICE_CODEC_AVCDEC)
		avcdecHelperStep(helper);
	else if (helper->codec == SERVICE_CODEC_M4VDEC)
		m4vdecHelperStep(helper);
	else if (helper->codec == SERVICE_CODEC_AVCENC)
		avcencHelperStep(helper);
}

/* the next stage after the jobs: stop, or the restart the shared flags ask */
static void helperKick(VideoHelper *helper)
{
	VideoHelperShared *shared = helper->shared;

	if (helper->codec == SERVICE_CODEC_AVCDEC || helper->codec == SERVICE_CODEC_M4VDEC) {
		if (shared->finished) {
			vnzThreadSetFlags(shared->engineThread, ENGINE_FLAG_HELPER_DONE);
			vnzThreadSetFlags(shared->thread, HELPER_FLAG_STOP);
		} else if (shared->status) {
			vnzThreadSetFlags(shared->thread, HELPER_FLAG_RESTART | HELPER_FLAG_RESTARTED);
		} else {
			vnzThreadSetFlags(shared->thread, HELPER_FLAG_RESTART);
		}
	} else if (helper->codec == SERVICE_CODEC_AVCENC) {
		if (shared->finished)
			vnzThreadSetFlags(shared->thread, HELPER_FLAG_STOP);
		else if (shared->encoderDone)
			vnzThreadSetFlags(shared->thread, HELPER_FLAG_RESTART |
				HELPER_FLAG_ENCODER_STOP);
		else
			vnzThreadSetFlags(shared->thread, HELPER_FLAG_RESTART);
	}
}

static void helperRestart(VideoHelper *helper, uint32_t flags)
{
	if (flags & HELPER_FLAG_RESTARTED)
		helper->needSlice = 1;
	else if ((flags & HELPER_FLAG_ENCODER_STOP) && helper->codec == SERVICE_CODEC_AVCENC)
		avcencHelperStop(helper);
	vnzInterruptDisable(helper->interrupt);
}

static void helperStop(VideoHelper *helper)
{
	VideoHelperShared *shared;

	if (helper->codec == SERVICE_CODEC_AVCENC)
		avcencHelperStop(helper);
	shared = (VideoHelperShared *)vnzPublicOpenProtected((uint32_t)helper->shared,
		SHARED_SIZE);
	vnzInterruptDisable(helper->interrupt);
	vnzInterruptRelease(helper->interrupt);
	if (shared->callback[1])
		((void (*)(uint32_t))shared->callback[1])((uint32_t)shared->callback[2]);
	vnzPublicCloseProtected((uint32_t)shared, SHARED_SIZE);
}

/* the unit's interrupt: its status kept, the helper woken */
uint32_t videoHelperInterrupt(uint32_t status, uint32_t argument2)
{
	*(uint32_t *)status = VIDEO_UNIT[VIDEO_UNIT_STATUS];
	VIDEO_UNIT[VIDEO_UNIT_STATUS] = 0;
	return HELPER_FLAG_UNIT;
}

/* the unit's descriptor: the next, its flags (2 the last), the bytes */
typedef struct HelperDescriptor {
	uint32_t next;
	uint32_t flags;
	uint32_t address;
	uint32_t size;
} HelperDescriptor;

#define DESCRIPTOR_LAST			2

static uint32_t streamAddress(const VideoHelperShared *shared, uint32_t address)
{
	return shared->publicStream ? vnzToPublic(address) : vnzToUncached(address);
}

/*
 * The unit loaded with the rest of a NAL unit from its saved bit offset: one
 * descriptor, or two where the ring wraps; then started at that bit.
 */
void videoHelperLoad(const VideoHelperShared *shared, const VideoStream *stream,
	const VideoNal *nal)
{
	HelperDescriptor descriptor[2], *unit = (HelperDescriptor *)shared->descriptors;
	uint32_t bitOffset = nal->state.bitOffset, start, ringEnd, first;
	int32_t size, i;

	start = nal->start + 1 + (bitOffset >> 3);
	ringEnd = stream->ringEnd;
	if (ringEnd < start)
		start = start - ringEnd + stream->ringStart - 1;
	size = nal->size - 1 - (bitOffset >> 3);
	if (ringEnd < start + size) {
		first = ringEnd - start + 1;
		descriptor[0].next = shared->descriptors + sizeof(HelperDescriptor);
		descriptor[0].flags = 0;
		descriptor[0].address = streamAddress(shared, start);
		descriptor[0].size = first;
		descriptor[1].next = 0;
		descriptor[1].flags = DESCRIPTOR_LAST;
		descriptor[1].address = streamAddress(shared, stream->ringStart);
		descriptor[1].size = size - first;
		for (i = 0; i < 2; i++)
			unit[i] = descriptor[i];
	} else {
		descriptor[0].next = 0;
		descriptor[0].flags = DESCRIPTOR_LAST;
		descriptor[0].address = streamAddress(shared, start);
		descriptor[0].size = size;
		unit[0] = descriptor[0];
	}
	VIDEO_UNIT[VIDEO_UNIT_DESCRIPTORS] = shared->descriptors;
	VIDEO_UNIT[VIDEO_UNIT_COMMAND] = VIDEO_UNIT_LOAD;
	vnzThreadWaitFlags(HELPER_FLAG_UNIT);
	VIDEO_UNIT[VIDEO_UNIT_COMMAND] = ((bitOffset & 7) + 7) << 8 & 0xff00;
	vnzThreadWaitFlags(HELPER_FLAG_UNIT);
	VIDEO_UNIT[VIDEO_UNIT_BITS] = 0;
}
