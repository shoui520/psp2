/*
 * The codec service (0x85fdd4-0x86120c): the thread the video library
 * opens (videolib.c). It takes the codecs' requests from its queue, gives
 * each decoder and encoder a slot, opens the slot's engine threads, passes
 * the access units and decodes on to them and the engines' answers back to
 * the codec as 64-byte replies. A reply is written only once the codec has
 * released the previous one (a bit per slot).
 */
#include "service.h"

#define SERVICE_SIZE			192
#define DECODER_SIZE			128
#define ENCODER_SIZE			64
#define REQUEST_SIZE			128
#define REPLY_SIZE			64

#define SERVICE_RUNNING			2
#define SERVICE_STACK			512
#define ENGINE_STACK			1024
#define ENCODER_STACK2			2048
#define ENGINE_PRIORITY			3

/* a slot's work memory: the replies, the access units, the decodes, the engine's */
#define SLOT_AU_REQUESTS		128
#define SLOT_DECODE_REQUESTS		384
#define SLOT_ENGINE			640

static void auDone(VideoService *service, uint32_t request);
static void picturesDone(VideoService *service, uint32_t request);

static VideoService *serviceBegin(uint32_t address);
static void serviceEnd(VideoService *service);
static uint32_t serviceWait(VideoService *service);
static void helperDelete(VideoService *service);
static void decoderClose(VideoService *service, int32_t index);
static void encoderClose(VideoService *service, int32_t index);
static void decoderThread(uint32_t memory, uint32_t codec);
static void decoderThread2(uint32_t memory, uint32_t codec);
static void encoderThread(uint32_t memory, uint32_t codec);
static void encoderThread2(uint32_t memory, uint32_t codec);

static VideoServiceSlot *encoderSlot(VideoService *service, int32_t index)
{
	return &service->encoders[index - service->numOfDecoders];
}

static void threadStop(VkThread *thread)
{
	vnzThreadSetFlags(thread, SERVICE_FLAG_EXIT);
	while (vnzThreadIsAlive(thread))
		vnzThreadWaitChildren();
}

/* the service's thread: requests, then the engines' answers, until told to exit */
static void serviceThread(uint32_t address, uint32_t helperMemory)
{
	VideoService *service = serviceBegin(address);
	VkMessageQueue *requestQueue = service->requestQueue;
	VkMessageQueue *engineQueue = service->engineQueue;
	VkMessageQueue *engineQueue2 = service->engineQueue2;
	uint32_t flags;

	do {
		flags = serviceWait(service);
		while (vkMessageReceive(requestQueue, (uint32_t)service))
			;
		while (vkMessageReceive(engineQueue, (uint32_t)service))
			;
		while (vkMessageReceive(engineQueue2, (uint32_t)service))
			;
		if (flags & SERVICE_FLAG_EXIT)
			service->state = 0;
	} while (service->state);
	serviceEnd(service);
}

static VkThread *helperCreate(VideoService *service, uint32_t memory)
{
	VkMessageQueue *queue, *queue2, *queue3;
	VkThread *thread;

	thread = (VkThread *)vnzThreadCreate(videoHelperThread, SERVICE_CORE, ENGINE_PRIORITY,
		ENGINE_STACK);
	queue = vkMessageQueueCreate(thread, 0x10000, service->queueSize);
	queue2 = vkMessageQueueCreate(thread, 2, 4);
	queue3 = vkMessageQueueCreate(thread, 8, 4);
	service->helper = thread;
	service->helperMemory = memory;
	service->helperQueue = queue;
	service->helperQueue2 = queue2;
	service->helperQueue3 = queue3;
	if (thread == VIDEO_NO_THREAD || !queue || !queue2) {
		helperDelete(service);
		return VIDEO_NO_THREAD;
	}
	videoHelperInitialize(memory, service);
	vnzThreadStart(thread, memory, 0);
	return thread;
}

static void helperDelete(VideoService *service)
{
	VkThread *thread = service->helper;

	if (thread != VIDEO_NO_THREAD)
		threadStop(thread);
	if (service->helperQueue)
		vkMessageQueueDelete(service->helperQueue);
	if (service->helperQueue2)
		vkMessageQueueDelete(service->helperQueue2);
	if (service->helperQueue3)
		vkMessageQueueDelete(service->helperQueue3);
	if (thread != VIDEO_NO_THREAD) {
		vnzThreadDelete(thread);
		service->helper = VIDEO_NO_THREAD;
	}
}

/* a decoder's engine: its two threads, their queues and memory */
static void decoderOpen(VideoService *service, uint32_t codec, int32_t index)
{
	VideoServiceSlot *slot = &service->decoders[index].slot;
	VkMessageQueue *auQueue, *wakeQueue, *innerQueue, *decodeQueue;
	VkThread *thread, *thread2;
	uint32_t memory, memory2;

	thread = (VkThread *)vnzThreadCreate(decoderThread, SERVICE_CORE, ENGINE_PRIORITY,
		ENGINE_STACK);
	thread2 = (VkThread *)vnzThreadCreate(decoderThread2, SERVICE_CORE, ENGINE_PRIORITY,
		ENGINE_STACK);
	memory = slot->memory;
	if (codec == SERVICE_CODEC_AVCDEC)
		memory2 = memory + avcdecQueryParserMemory(slot->maxStreams, slot->maxFrames);
	else
		memory2 = memory + m4vdecQueryParserMemory();
	auQueue = vkMessageQueueCreate(thread, 1, 2);
	wakeQueue = vkMessageQueueCreate(thread, 4, 2);
	innerQueue = vkMessageQueueCreate(thread2, 1, 4);
	decodeQueue = vkMessageQueueCreate(thread2, 4, 2);
	slot->thread = thread;
	slot->thread2 = thread2;
	slot->auQueue = auQueue;
	slot->decodeQueue = decodeQueue;
	slot->wakeQueue = wakeQueue;
	slot->innerQueue = innerQueue;
	if (thread == VIDEO_NO_THREAD || thread2 == VIDEO_NO_THREAD || !auQueue || !wakeQueue ||
	    !innerQueue || !decodeQueue) {
		service->status = -1;
		decoderClose(service, index);
		return;
	}
	service->status = 0;
	if (codec == SERVICE_CODEC_AVCDEC) {
		avcdecParserInitialize(memory, service, index);
		avcdecDecoderInitialize(memory2, service, index);
	} else if (codec == SERVICE_CODEC_M4VDEC) {
		m4vdecParserInitialize(memory, service, index);
		m4vdecDecoderInitialize(memory2, service, index);
	}
	vnzThreadStart(thread, memory, codec);
	vnzThreadStart(thread2, memory2, codec);
}

static void slotClose(VideoServiceSlot *slot)
{
	VkThread *thread = slot->thread, *thread2 = slot->thread2;

	if (thread != VIDEO_NO_THREAD)
		threadStop(thread);
	if (thread2 != VIDEO_NO_THREAD)
		threadStop(thread2);
	if (slot->auQueue)
		vkMessageQueueDelete(slot->auQueue);
	if (slot->wakeQueue)
		vkMessageQueueDelete(slot->wakeQueue);
	if (slot->decodeQueue)
		vkMessageQueueDelete(slot->decodeQueue);
	if (slot->innerQueue)
		vkMessageQueueDelete(slot->innerQueue);
	if (thread != VIDEO_NO_THREAD)
		vnzThreadDelete(thread);
	if (thread2 != VIDEO_NO_THREAD)
		vnzThreadDelete(thread2);
}

static void decoderClose(VideoService *service, int32_t index)
{
	slotClose(&service->decoders[index].slot);
}

/* the encoder's engine: the encoding thread, and the stream's */
static void encoderOpen(VideoService *service, uint32_t codec, int32_t index)
{
	VideoServiceSlot *slot = encoderSlot(service, index);
	VkMessageQueue *auQueue, *wakeQueue, *innerQueue, *decodeQueue;
	VkThread *thread, *thread2;
	uint32_t memory, memory2;

	thread = (VkThread *)vnzThreadCreate(encoderThread, SERVICE_CORE, ENGINE_PRIORITY,
		ENGINE_STACK);
	thread2 = (VkThread *)vnzThreadCreate(encoderThread2, SERVICE_CORE, ENGINE_PRIORITY,
		ENCODER_STACK2);
	memory = slot->memory;
	memory2 = memory + avcencQueryContextMemory();
	auQueue = vkMessageQueueCreate(thread2, 1, 2);
	wakeQueue = vkMessageQueueCreate(thread, 1, 2);
	innerQueue = vkMessageQueueCreate(thread2, 4, 4);
	decodeQueue = vkMessageQueueCreate(thread, 4, 2);
	slot->thread = thread;
	slot->thread2 = thread2;
	slot->decodeQueue = decodeQueue;
	slot->innerQueue = innerQueue;
	slot->auQueue = auQueue;
	slot->wakeQueue = wakeQueue;
	if (thread == VIDEO_NO_THREAD || thread2 == VIDEO_NO_THREAD || !decodeQueue ||
	    !wakeQueue || !innerQueue || !auQueue) {
		service->status = -1;
		encoderClose(service, index);
		return;
	}
	service->status = 0;
	avcencEncoderInitialize(memory, service, index);
	avcencStreamInitialize(memory2, service, index);
	vnzThreadStart(thread, memory, codec);
	vnzThreadStart(thread2, memory2, codec);
}

static void encoderClose(VideoService *service, int32_t index)
{
	slotClose(encoderSlot(service, index));
}

/* the service opened for its thread: every slot free */
static VideoService *serviceBegin(uint32_t address)
{
	VideoService *service;
	int32_t numOfEncoders, numOfDecoders, i;

	service = (VideoService *)vnzPublicOpenPrivate(address, SERVICE_SIZE);
	numOfEncoders = service->numOfEncoders;
	numOfDecoders = service->numOfDecoders;
	for (i = 0; i < numOfDecoders; i++)
		service->decoders[i].slot.id = -1;
	service->decodersInUse = 0;
	for (i = 0; i < numOfEncoders; i++)
		service->encoders[i].id = -1;
	service->state = SERVICE_RUNNING;
	service->encodersInUse = 0;
	return service;
}

static void serviceEnd(VideoService *service)
{
	vnzPublicClosePrivate((uint32_t)service, SERVICE_SIZE);
}

static uint32_t serviceWait(VideoService *service)
{
	return vnzThreadWaitFlags(SERVICE_FLAG_EXIT | SERVICE_FLAG_REQUEST | SERVICE_FLAG_ENGINE2 |
		SERVICE_FLAG_ENGINE);
}

/* the engines' messages: an access unit taken, pictures decoded */
void videoServiceAuDone(uint32_t service, uint32_t request)
{
	auDone((VideoService *)service, request);
}

void videoServicePicturesDone(uint32_t service, uint32_t request)
{
	picturesDone((VideoService *)service, request);
}

uint32_t videoServiceQueryMemory(uint32_t numOfDecoders, uint32_t numOfEncoders,
	uint32_t commandSize, uint32_t horizontal)
{
	uint32_t size = SERVICE_SIZE + numOfDecoders * DECODER_SIZE + numOfEncoders * ENCODER_SIZE;

	return size + videoHelperQueryMemory(commandSize, horizontal);
}

/* a codec's work memory for the service: the replies, the requests, the engine's own */
uint32_t videoServiceDecoderSize(void)
{
	return SLOT_ENGINE;
}

uint32_t videoServiceEncoderSize(void)
{
	return SLOT_ENGINE;
}

void videoServiceSetup(uint32_t address, uint32_t resource, uint32_t flagAccepted,
	uint32_t flagReplied, uint32_t numOfDecoders, uint32_t numOfEncoders, uint32_t commandSize,
	uint32_t horizontal, uint32_t (*callback)(void), uint32_t (*callback2)(void),
	uint32_t (*callback3)(void), uint32_t version)
{
	VideoService *service;
	uint32_t queueSize = 0, count;
	int32_t bit;

	/* room for each codec's two messages: the power of two not below twice their number */
	if ((int32_t)numOfDecoders >= 1 || (int32_t)numOfEncoders >= 1) {
		count = (numOfDecoders + numOfEncoders) * 2 - 1;
		for (bit = 31; !(count & (1 << bit)); bit--)
			;
		queueSize = 1 << bit;
	}
	service = (VideoService *)vnzPublicOpenPrivate(address, SERVICE_SIZE);
	service->numOfEncoders = numOfEncoders;
	service->numOfDecoders = numOfDecoders;
	service->flagAccepted = flagAccepted;
	service->resource = (VkVThreadResource *)resource;
	service->replyToggle = 0;
	service->replyReady = -1;
	service->flagReplied = flagReplied;
	service->commandSize = commandSize;
	service->queueSize = queueSize;
	service->horizontal = horizontal;
	service->callback[0] = callback;
	service->callback[1] = callback2;
	service->callback[2] = callback3;
	service->version = version;
	vnzPublicClosePrivate((uint32_t)service, SERVICE_SIZE);
}

/*
 * The service started: its thread, the VThread contexts the engines run their
 * jobs on, its queues and the helper. Returns the thread, or -1.
 */
int32_t videoServiceOpen(uint32_t address, uint32_t *info, uint32_t *info2,
	uint32_t numOfDecoders, uint32_t numOfEncoders, uint32_t horizontal)
{
	VkMessageQueue *engineQueue, *engineQueue2, *requestQueue, *releaseQueue;
	uint32_t decoders, encoders, helperMemory, jobs, count;
	int32_t widthInMbs, groups, rows, columns, queueSize, i;
	VkVThreadContext *context;
	volatile uint8_t *lock;
	VideoService *service;
	VkThread *thread, *helper;
	uint32_t failed;

	thread = (VkThread *)vnzThreadCreate(serviceThread, SERVICE_CORE, 4, SERVICE_STACK);
	decoders = address + SERVICE_SIZE;
	encoders = decoders + numOfDecoders * DECODER_SIZE;
	helperMemory = encoders + numOfEncoders * ENCODER_SIZE;
	service = (VideoService *)vnzPublicOpenPrivate(address, SERVICE_SIZE);

	/* the jobs of a row of macroblocks, and of the encoder's search */
	widthInMbs = (int32_t)(horizontal + 15) >> 4;
	if (widthInMbs < 40)
		groups = (widthInMbs + 5) / 6;
	else
		groups = (widthInMbs + 7) / 8;
	jobs = (groups + 1) * 12;
	columns = (widthInMbs + 3) >> 2;
	if (columns < 2)
		columns = 2;
	rows = ((widthInMbs + 7) >> 3) + 1;
	count = (numOfDecoders + numOfEncoders) * 8 + (columns * 8 + rows * 8 + 4) * numOfEncoders;
	service->encoderContext = vkVThreadContextCreate(count, service->resource);
	failed = !service->encoderContext;
	for (i = 0; i < 2; i++) {
		context = vkVThreadContextCreate(jobs, service->resource);
		lock = vnzLockAllocate();
		failed |= !context;
		failed |= !lock;
		service->context[i] = context;
		service->contextLock[i] = lock;
	}
	lock = 0;
	if (numOfEncoders) {
		lock = vnzLockAllocate();
		failed |= !lock;
	}
	service->encoderLock = lock;

	queueSize = service->queueSize;
	engineQueue = vkMessageQueueCreate(thread, SERVICE_FLAG_ENGINE, queueSize * 2);
	engineQueue2 = vkMessageQueueCreate(thread, SERVICE_FLAG_ENGINE2, queueSize * 2);
	requestQueue = vkMessageQueueCreate(thread, SERVICE_FLAG_REQUEST, queueSize);
	releaseQueue = vkMessageQueueCreate(thread, SERVICE_FLAG_RELEASE, queueSize * 2);
	service->engineQueue = engineQueue;
	service->engineQueue2 = engineQueue2;
	service->requestQueue = requestQueue;
	service->decoders = (VideoServiceDecoder *)decoders;
	service->releaseQueue = releaseQueue;
	service->encoders = (VideoServiceSlot *)encoders;
	service->lastId = 0;
	service->client = vnzThreadCurrent();
	helper = helperCreate(service, helperMemory);
	address = vnzPublicClosePrivate((uint32_t)service, SERVICE_SIZE);
	if (thread == VIDEO_NO_THREAD || helper == VIDEO_NO_THREAD || !engineQueue ||
	    !engineQueue2 || !requestQueue || !releaseQueue || failed) {
		videoServiceClose(address, (int32_t)thread);
		return -1;
	}
	vnzThreadStart(thread, address, helperMemory);
	*info = (uint32_t)requestQueue;
	*info2 = (uint32_t)releaseQueue;
	return (int32_t)thread;
}

void videoServiceClose(uint32_t address, int32_t handle)
{
	VideoService *service = (VideoService *)address;
	VkThread *thread = (VkThread *)handle;
	int32_t i;

	if (thread != VIDEO_NO_THREAD)
		threadStop(thread);
	helperDelete(service);
	if (service->encoderContext)
		vkVThreadContextDelete(service->encoderContext);
	for (i = 0; i < 2; i++) {
		if (service->context[i])
			vkVThreadContextDelete(service->context[i]);
		if (service->contextLock[i])
			vnzLockFree(service->contextLock[i]);
	}
	if (service->encoderLock)
		vnzLockFree(service->encoderLock);
	if (service->engineQueue)
		vkMessageQueueDelete(service->engineQueue);
	if (service->engineQueue2)
		vkMessageQueueDelete(service->engineQueue2);
	if (service->requestQueue)
		vkMessageQueueDelete(service->requestQueue);
	if (service->releaseQueue)
		vkMessageQueueDelete(service->releaseQueue);
	if (thread != VIDEO_NO_THREAD)
		vnzThreadDelete(thread);
}

/* the engines' threads, entered with their memory and the codec (0x8606b4-) */
static void decoderThread(uint32_t memory, uint32_t codec)
{
	if (codec == SERVICE_CODEC_AVCDEC)
		avcdecParserThread(memory, codec);
	else if (codec == SERVICE_CODEC_M4VDEC)
		m4vdecParserThread(memory, codec);
}

static void decoderThread2(uint32_t memory, uint32_t codec)
{
	if (codec == SERVICE_CODEC_AVCDEC)
		avcdecDecoderThread(memory, codec);
	else if (codec == SERVICE_CODEC_M4VDEC)
		m4vdecDecoderThread(memory, codec);
}

static void encoderThread(uint32_t memory, uint32_t codec)
{
	if (codec == SERVICE_CODEC_AVCENC)
		avcencEncoderThread(memory, SERVICE_CODEC_AVCENC);
}

static void encoderThread2(uint32_t memory, uint32_t codec)
{
	if (codec == SERVICE_CODEC_AVCENC)
		avcencStreamThread(memory, SERVICE_CODEC_AVCENC);
}

/* the slots: a decoder's index, an encoder's after the decoders' */
static int32_t decoderAllocate(VideoService *service, int32_t id)
{
	int32_t count = service->numOfDecoders, i;

	if (service->decodersInUse >= count || count < 1)
		return -1;
	for (i = 0; i < count; i++)
		if (service->decoders[i].slot.id == id)
			return -1;
	for (i = 0; i < count; i++) {
		if (service->decoders[i].slot.id < 0) {
			service->decoders[i].slot.id = id;
			service->decodersInUse++;
			return i;
		}
	}
	return -1;
}

static int32_t decoderFind(VideoService *service, int32_t id)
{
	int32_t count = service->numOfDecoders, i;

	for (i = 0; i < count; i++)
		if (service->decoders[i].slot.id == id)
			return i;
	return -1;
}

static int32_t decoderFree(VideoService *service, int32_t id)
{
	int32_t count = service->numOfDecoders, i;

	for (i = 0; i < count; i++) {
		if (service->decoders[i].slot.id == id) {
			service->decoders[i].slot.id = -1;
			service->decodersInUse--;
			return i;
		}
	}
	return -1;
}

static int32_t encoderAllocate(VideoService *service, int32_t id)
{
	int32_t count = service->numOfEncoders, i;

	if (service->encodersInUse >= count || count < 1)
		return -1;
	for (i = 0; i < count; i++)
		if (service->encoders[i].id == id)
			return -1;
	for (i = 0; i < count; i++) {
		if (service->encoders[i].id < 0) {
			service->encoders[i].id = id;
			service->encodersInUse++;
			return service->numOfDecoders + i;
		}
	}
	return -1;
}

static int32_t encoderFind(VideoService *service, int32_t id)
{
	int32_t count = service->numOfEncoders, i;

	for (i = 0; i < count; i++)
		if (service->encoders[i].id == id)
			return service->numOfDecoders + i;
	return -1;
}

static int32_t encoderFree(VideoService *service, int32_t id)
{
	int32_t count = service->numOfEncoders, i;

	for (i = 0; i < count; i++) {
		if (service->encoders[i].id == id) {
			service->encoders[i].id = -1;
			service->encodersInUse--;
			return service->numOfDecoders + i;
		}
	}
	return -1;
}

/* the slot's previous reply released by its codec */
static void replyWait(VideoService *service, int32_t index)
{
	uint32_t bit = 1 << index;

	while (!(service->replyToggle & bit) && !(service->replyReady & bit))
		while (!vkMessageReceive(service->releaseQueue, (uint32_t)service))
			vnzThreadWaitFlags(SERVICE_FLAG_RELEASE);
	if (service->replyToggle & bit) {
		service->replyToggle &= ~bit;
	} else {
		service->replyReady &= ~bit;
		service->replyToggle |= bit;
	}
}

static void replyBuild(VideoService *service, uint32_t code, uint32_t kind, int32_t index,
	const VideoEngineRequest *request)
{
	VideoServiceReply *reply;
	VideoServiceSlot *slot;

	if (index >= service->numOfDecoders)
		slot = encoderSlot(service, index);
	else
		slot = &service->decoders[index].slot;
	reply = (VideoServiceReply *)vnzPublicOpenPrivate(slot->replies +
		slot->replyIndex * REPLY_SIZE, REPLY_SIZE);
	reply->instance = slot->instance;
	reply->code = code;
	reply->kind = kind;
	reply->index = index;
	reply->id = slot->id;
	if (code == 4)
		goto close;
	if (code == SERVICE_OPEN) {
		reply->status = service->status;
		goto close;
	}

	if (index >= service->numOfDecoders) {
		if (code != SERVICE_DECODE)
			goto close;
		/* an encoded picture: the stream written and the frame */
		reply->user = request->user;
		reply->esOffset = request->esOffset;
		reply->frame = request->esSize;
		reply->status = request->status;
		reply->reset = (request->flags & VIDEO_ENGINE_DRAIN) != 0;
		reply->numOfPictures = request->numOfPictures;
		reply->pictures = request->pictures;
		reply->horizontal = request->horizontal;
		reply->vertical = request->vertical;
		goto close;
	}

	if (code == SERVICE_AU) {
		reply->user = request->user;
		reply->esOffset = request->esOffset;
		reply->frame = request->esSize;
		reply->status = request->status;
		if (request->flags & VIDEO_ENGINE_SEQUENCE) {
			reply->sequence = 1;
			reply->numOfRefFrames = request->numOfRefFrames;
			reply->horizontal = request->horizontal;
			reply->vertical = request->vertical;
			reply->sequenceInfo = request->sequenceInfo;
			reply->fieldDecode = (request->contextFlags & VIDEO_ENGINE_CONTEXT_FIELD) != 0;
			reply->option = (request->contextFlags & VIDEO_ENGINE_CONTEXT_OPTION) != 0;
			reply->macroblockData =
				(request->contextFlags & VIDEO_ENGINE_CONTEXT_MACROBLOCK) != 0;
		} else {
			reply->sequence = 0;
			reply->reset = (request->flags & VIDEO_ENGINE_DRAIN) != 0;
		}
	} else if (code == SERVICE_DECODE) {
		reply->user = request->user;
		reply->numOfPictures = request->numOfPictures;
		reply->pictures = request->pictures;
		reply->reset = (request->flags & VIDEO_ENGINE_DRAIN) != 0;
	}
close:
	vnzPublicClosePrivate((uint32_t)reply, REPLY_SIZE);
}

/* a reply to the slot's codec, on its queue to the codec's own message */
static void replySend(VideoService *service, uint32_t code, uint32_t kind, int32_t index,
	const VideoEngineRequest *request)
{
	VideoServiceSlot *slot;
	uint32_t reply;

	replyWait(service, index);
	replyBuild(service, code, kind, index, request);
	if (index >= service->numOfDecoders) {
		slot = encoderSlot(service, index);
		reply = slot->replies + slot->replyIndex * REPLY_SIZE;
		slot->replyIndex = 1 - slot->replyIndex;
		vkMessageSend(slot->clientQueue, avcencReceive, reply);
		return;
	}
	slot = &service->decoders[index].slot;
	reply = slot->replies + slot->replyIndex * REPLY_SIZE;
	slot->replyIndex = 1 - slot->replyIndex;
	if (slot->codec == SERVICE_CODEC_AVCDEC)
		vkMessageSend(slot->clientQueue, avcdecReceive, reply);
	else if (slot->codec == SERVICE_CODEC_M4VDEC)
		vkMessageSend(slot->clientQueue, m4vdecReceive, reply);
}

static void slotOpen(VideoServiceSlot *slot, const VideoServiceRequest *request, int32_t id)
{
	slot->id = id;
	slot->instance = request->instance;
	slot->clientThread = request->thread;
	slot->clientQueue = request->queue;
	slot->decodeIndex = 0;
	slot->auIndex = 0;
	slot->replyIndex = 0;
	slot->replies = request->frames;
	slot->auRequests = request->frames + SLOT_AU_REQUESTS;
	slot->decodeRequests = request->frames + SLOT_DECODE_REQUESTS;
	slot->memory = request->frames + SLOT_ENGINE;
}

static VideoEngineRequest *engineRequest(uint32_t requests, uint8_t *index)
{
	*index = 1 - *index;
	return (VideoEngineRequest *)vnzPublicOpenPrivate(requests + *index * REQUEST_SIZE,
		REQUEST_SIZE);
}

static void engineRequestDone(VideoEngineRequest *request)
{
	vnzPublicClosePrivate((uint32_t)request, REQUEST_SIZE);
}

/* an encoder's request into its slot or engine request */
static void encoderRequest(VideoService *service, uint32_t code, int32_t index, int32_t id,
	const VideoServiceRequest *request)
{
	VideoServiceSlot *slot = encoderSlot(service, index);
	VideoEngineRequest *engine;

	if (code == SERVICE_CLOSE) {
		encoderFree(service, id);
	} else if (code == SERVICE_OPEN) {
		slot = (VideoServiceSlot *)vnzPublicOpenPrivate((uint32_t)slot, ENCODER_SIZE);
		slotOpen(slot, request, id);
		slot->option = request->option;
		slot->codec = request->codec;
		slot->work2 = request->work2;
		service->encoderOption = request->encoderOption;
		vnzPublicClosePrivate((uint32_t)slot, ENCODER_SIZE);
	} else if (code == SERVICE_DECODE) {
		engine = engineRequest(slot->decodeRequests, &slot->decodeIndex);
		engine->index = index;
		engine->codec = slot->codec;
		engine->user = request->user;
		engine->buffer = request->buffer;
		engine->bufferSize = request->bufferSize;
		engineRequestDone(engine);
	} else if (code == SERVICE_AU) {
		engine = engineRequest(slot->auRequests, &slot->auIndex);
		engine->index = index;
		engine->codec = slot->codec;
		engine->user = request->user;
		engine->pictures = request->contextAddress;
		engine->flags = 0;
		engine->auMode = request->auMode;
		if (request->drain)
			engine->flags |= VIDEO_ENGINE_DRAIN;
		engine->buffer = request->buffer;
		engine->bufferSize = request->bufferSize;
		engine->esOffset = request->esOffset;
		engine->esSize = request->esSize;
		engine->numOfPictures = service->decodersInUse;
		engineRequestDone(engine);
	}
}

static void decoderRequest(VideoService *service, uint32_t code, int32_t index, int32_t id,
	const VideoServiceRequest *request)
{
	VideoServiceDecoder *decoder = &service->decoders[index];
	VideoServiceSlot *slot = &decoder->slot;
	VideoEngineRequest *engine;

	if (code == SERVICE_CLOSE) {
		decoderFree(service, id);
	} else if (code == SERVICE_OPEN) {
		decoder = (VideoServiceDecoder *)vnzPublicOpenPrivate((uint32_t)decoder,
			DECODER_SIZE);
		slotOpen(&decoder->slot, request, id);
		decoder->slot.maxStreams = request->maxStreams;
		decoder->slot.maxFrames = request->maxFrames;
		decoder->slot.option = request->option;
		decoder->slot.codec = request->codec;
		decoder->option5[0] = request->option5[0];
		decoder->option5[1] = request->option5[1];
		decoder->option4[0] = request->option4[0];
		decoder->option4[1] = request->option4[1];
		vnzPublicClosePrivate((uint32_t)decoder, DECODER_SIZE);
	} else if (code == SERVICE_DECODE) {
		engine = engineRequest(slot->decodeRequests, &slot->decodeIndex);
		engine->index = index;
		engine->codec = slot->codec;
		engine->user = request->user;
		engine->buffer = request->buffer;
		engineRequestDone(engine);
	} else if (code == SERVICE_AU) {
		engine = engineRequest(slot->auRequests, &slot->auIndex);
		engine->index = index;
		engine->codec = slot->codec;
		engine->user = request->user;
		engine->pictures = request->contextAddress;
		engine->flags = 0;
		engine->auMode = request->auMode;
		engine->dts[0] = request->dts[0];
		engine->dts[1] = request->dts[1];
		engine->pts[0] = request->pts[0];
		engine->pts[1] = request->pts[1];
		engine->option = request->option;
		if (request->sequence)
			engine->flags |= VIDEO_ENGINE_SEQUENCE;
		if (request->drain)
			engine->flags |= VIDEO_ENGINE_DRAIN;
		if (request->context) {
			engine->flags |= VIDEO_ENGINE_CONTEXT;
			engine->numOfRefFrames = request->numOfRefFrames;
			engine->horizontal = request->horizontal;
			engine->contextFlags = 0;
			engine->vertical = request->vertical;
			if (request->fieldDecode)
				engine->contextFlags |= VIDEO_ENGINE_CONTEXT_FIELD;
			if (request->contextOption)
				engine->contextFlags |= VIDEO_ENGINE_CONTEXT_OPTION;
			if (request->macroblockData)
				engine->contextFlags |= VIDEO_ENGINE_CONTEXT_MACROBLOCK;
		} else {
			if (request->restart)
				engine->flags |= VIDEO_ENGINE_RESTART;
			engine->buffer = request->buffer;
			engine->bufferSize = request->bufferSize;
			engine->esOffset = request->esOffset;
			engine->esSize = request->esSize;
		}
		engineRequestDone(engine);
	}
}

/* a codec's request read into its slot; the slot is left in current */
static uint32_t requestTake(VideoService *service, uint32_t address)
{
	const VideoServiceRequest *request;
	int32_t index, id;
	uint32_t code;

	request = (const VideoServiceRequest *)vnzPublicOpenProtected(address, REQUEST_SIZE);
	code = request->code;
	if (request->codec >= SERVICE_CODEC_AVCENC) {
		if (code == SERVICE_OPEN) {
			id = ++service->lastId;
			index = encoderAllocate(service, id);
		} else {
			id = request->id;
			index = encoderFind(service, id);
		}
	} else {
		if (code == SERVICE_OPEN) {
			id = ++service->lastId;
			index = decoderAllocate(service, id);
		} else {
			id = request->id;
			index = decoderFind(service, id);
		}
	}
	service->current = index;
	if (index >= service->numOfDecoders)
		encoderRequest(service, code, index, id, request);
	else if (index >= 0)
		decoderRequest(service, code, index, id, request);
	vnzPublicCloseProtected((uint32_t)request, REQUEST_SIZE);
	return code;
}

/* the request taken passed on: to an engine's thread, or done and answered here */
static void requestDispatch(VideoService *service, uint32_t code)
{
	int32_t index = service->current;
	VideoServiceSlot *slot;
	uint32_t request;

	if (index >= service->numOfDecoders) {
		slot = encoderSlot(service, index);
		if (code == SERVICE_AU) {
			vkMessageSend(slot->auQueue, avcencStreamAu,
				slot->auRequests + slot->auIndex * REQUEST_SIZE);
			return;
		}
		if (code == SERVICE_DECODE) {
			vkMessageSend(slot->decodeQueue, avcencEncoderEncode,
				slot->decodeRequests + slot->decodeIndex * REQUEST_SIZE);
			return;
		}
		if (code == SERVICE_OPEN)
			encoderOpen(service, slot->codec, index);
		else if (code == SERVICE_CLOSE)
			encoderClose(service, index);
		else if (code - 1 >= 8 || code == SERVICE_NONE)
			return;
		replySend(service, code, SERVICE_REPLY_DONE, index, 0);
		return;
	}

	slot = &service->decoders[index].slot;
	if (code == SERVICE_AU) {
		request = slot->auRequests + slot->auIndex * REQUEST_SIZE;
		if (slot->codec == SERVICE_CODEC_AVCDEC)
			vkMessageSend(slot->auQueue, avcdecParserAu, request);
		else if (slot->codec == SERVICE_CODEC_M4VDEC)
			vkMessageSend(slot->auQueue, m4vdecParserAu, request);
		return;
	}
	if (code == SERVICE_DECODE) {
		request = slot->decodeRequests + slot->decodeIndex * REQUEST_SIZE;
		if (slot->codec == SERVICE_CODEC_AVCDEC) {
			vkMessageSend(slot->wakeQueue, avcdecParserWake, request);
			vkMessageSend(slot->decodeQueue, avcdecDecoderDecode, request);
		} else if (slot->codec == SERVICE_CODEC_M4VDEC) {
			vkMessageSend(slot->wakeQueue, m4vdecParserWake, request);
			vkMessageSend(slot->decodeQueue, m4vdecDecoderDecode, request);
		}
		return;
	}
	if (code == SERVICE_OPEN)
		decoderOpen(service, slot->codec, index);
	else if (code == SERVICE_CLOSE)
		decoderClose(service, index);
	else if (code - 1 >= 8 || code == SERVICE_NONE)
		return;
	replySend(service, code, SERVICE_REPLY_DONE, index, 0);
}

/*
 * A codec's request (the message on the request queue): accepted at once,
 * or, for a slot that does not exist, answered with no reply.
 */
void videoServiceRequest(uint32_t address, uint32_t requestAddress)
{
	VideoService *service = (VideoService *)address;
	const VideoServiceRequest *request;
	VideoServiceSlot *slot;
	uint32_t code;
	int32_t index;

	code = requestTake(service, requestAddress);
	index = service->current;
	if (index < service->numOfDecoders) {
		if (index < 0) {
			request = (const VideoServiceRequest *)vnzPublicOpenProtected(requestAddress,
				REQUEST_SIZE);
			vnzThreadSetFlags(request->thread, service->flagAccepted);
			if (request->codec == SERVICE_CODEC_AVCDEC)
				vkMessageSend(request->queue, avcdecReceive, 0);
			else if (request->codec == SERVICE_CODEC_M4VDEC)
				vkMessageSend(request->queue, m4vdecReceive, 0);
			else
				vkMessageSend(request->queue, avcencReceive, 0);
			vnzPublicCloseProtected((uint32_t)request, REQUEST_SIZE);
			return;
		}
		slot = &service->decoders[index].slot;
	} else {
		slot = encoderSlot(service, index);
	}
	vnzThreadSetFlags(slot->clientThread, service->flagAccepted);
	requestDispatch(service, code);
}

/* a codec has read its reply: the slot's next may be written */
void videoServiceRelease(uint32_t address, uint32_t index)
{
	VideoService *service = (VideoService *)address;
	uint32_t bit = 1 << index;

	if (service->replyToggle & bit) {
		service->replyToggle &= ~bit;
		service->replyReady |= bit;
	} else {
		service->replyToggle |= bit;
	}
}

static void picturesDone(VideoService *service, uint32_t address)
{
	const VideoEngineRequest *request;

	request = (const VideoEngineRequest *)vnzPublicOpenProtected(address, REQUEST_SIZE);
	replySend(service, SERVICE_DECODE, SERVICE_REPLY_PICTURES, request->index, request);
	vnzPublicCloseProtected((uint32_t)request, REQUEST_SIZE);
}

static void auDone(VideoService *service, uint32_t address)
{
	const VideoEngineRequest *request;

	request = (const VideoEngineRequest *)vnzPublicOpenProtected(address, REQUEST_SIZE);
	replySend(service, SERVICE_AU, SERVICE_REPLY_AU, request->index, request);
	vnzPublicCloseProtected((uint32_t)request, REQUEST_SIZE);
}
