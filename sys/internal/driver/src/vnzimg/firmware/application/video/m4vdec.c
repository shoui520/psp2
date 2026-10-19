/*
 * The MPEG-4 decoder's interface (0x824110-0x824c96), the AVC decoder's
 * (avcdec.c) with the MPEG-4 parameters: the same instance, requests and
 * waits, the same decode and drain order.
 *
 * What differs: the request says codec 1; a sequence carries its size and
 * the per-macroblock data flag; pictures are 64-byte records with the
 * frame rate as a code; and the decoder restarts when a normal unit is
 * followed by any other.
 */
#include "video.h"

#define FRAME_RATE_UNKNOWN		255

#define M4VDEC_VERSION			0x02010a97

#define M4VDEC_ERROR			((int32_t)0x80000000)
#define M4VDEC_ERROR_WORK_ALIGNMENT	((int32_t)0x80000001)
#define M4VDEC_ERROR_WORK_SIZE		((int32_t)0x80000002)
#define M4VDEC_ERROR_CONTEXT_ALIGNMENT	((int32_t)0x80000003)
#define M4VDEC_ERROR_CONTEXT_SIZE	((int32_t)0x80000004)
#define M4VDEC_ERROR_LIBRARY_STATE	((int32_t)0x80000010)
#define M4VDEC_ERROR_IN_USE		((int32_t)0x80000011)
#define M4VDEC_ERROR_NOT_INITIALIZED	((int32_t)0x80000020)
#define M4VDEC_ERROR_POINTER		((int32_t)0x80000030)
#define M4VDEC_ERROR_AU_ALIGNMENT	((int32_t)0x10000001)
#define M4VDEC_ERROR_AU_RANGE		((int32_t)0x10000003)
#define M4VDEC_ERROR_AU_SIZE		((int32_t)0x10000004)

/* the ARM's parameters (the command's param) */
typedef struct M4vdecParam {
	VideoLibrary *library;
	uint16_t horizontal;		/* 16..1920 */
	uint16_t vertical;		/* 16..1088 */
	uint32_t sequenceInfo;
	uint8_t macroblockData;		/* the context keeps per-macroblock data */
	uint8_t option;
	uint8_t reserved[2];
	uint32_t option4[2];
	uint32_t option5[2];
} M4vdecParam;

/* the instance, in the work memory */
typedef struct M4vdecInstance {
	VideoLibrary *library;
	uint32_t frames;		/* the work memory after the instance's own */
	uint32_t serviceState;
	uint32_t flagAccepted;
	uint32_t flagReplied;
	uint32_t request;
	VkMessageQueue *serviceQueue;
	VkMessageQueue *queue;
	VkMessageQueue *serviceQueue2;
	uint32_t replyKind;
	uint32_t context;
	uint32_t pictures;		/* the engine's list, or the sequence's param */
	int32_t status;
	int8_t serviceSlot;
	int8_t numOfPictures;
	uint8_t inFlight;
	uint8_t mode;			/* MODE_* of the next request */
	uint8_t state;
	uint8_t waitToggle;
	uint8_t waitReady;
	uint8_t fieldDecode;
	uint8_t lastMode;		/* the last unit's */
	int8_t released;		/* top of released[] */
	uint8_t reserved[2];
	uint32_t releasedFrame[2];
	uint32_t reserved2[14];
} M4vdecInstance;

/* the request message */
typedef struct M4vdecRequest {
	M4vdecInstance *instance;
	uint32_t reserved;
	uint32_t serviceState;
	uint32_t code;
	uint8_t reserved2[2];
	uint8_t codec;
	uint8_t reserved3;
	uint32_t dts[2];
	uint32_t pts[2];
	uint32_t option5[2];
	uint32_t option4[2];
	uint32_t reserved4;
	union {
		struct {
			uint32_t buffer;
			uint32_t bufferSize;
			uint32_t esOffset;
			uint32_t esSize;
		};
		struct {
			uint32_t reserved5;
			VkThread *thread;
			VkMessageQueue *queue;
			uint32_t frames;
		};
	};
	uint32_t reserved6;
	uint8_t auMode;
	uint8_t sequence;
	uint8_t context;
	uint8_t drain;
	uint8_t reserved7[2];
	uint8_t restart;
	uint8_t reserved8;
	uint8_t option;
	uint8_t macroblockData;
	uint16_t horizontal;
	uint16_t vertical;
	uint16_t reserved9;
	uint32_t contextAddress;
	uint32_t reserved10[8];
} M4vdecRequest;

/* the engine's reply */
typedef struct M4vdecReply {
	uint32_t reserved;
	uint32_t serviceState;
	uint32_t serviceSlot;
	uint32_t reserved2;
	uint32_t kind;
	uint32_t reserved3[2];
	uint32_t frame;			/* a frame released */
	uint32_t status;
	uint32_t pictures;
	uint8_t numOfPictures;
	uint8_t reset;
	uint8_t sequence;		/* the sequence's parameters follow */
	uint8_t reserved4[3];
	uint8_t macroblockData;
	uint8_t reserved5;
	uint16_t horizontal;
	uint16_t vertical;
	uint32_t sequenceInfo;
	uint32_t reserved6[2];
} M4vdecReply;

/* a picture in the engine's list */
typedef struct M4vdecPictureNode {
	uint32_t luma;
	uint32_t chroma;
	uint16_t width;
	uint16_t pitch;
	uint32_t reserved;
	uint16_t height;
	uint16_t chromaHeight;
	uint32_t frame;
	uint8_t type;
	uint8_t info[3];
	uint8_t reserved2[12];
	uint32_t pts[2];
	uint32_t dts[2];
	uint32_t userData;
	uint32_t userDataSize;
	struct M4vdecPictureNode *next;
	uint32_t reserved3;
	uint32_t data;
	uint8_t vopInfo[4];
	int32_t timeIncrementResolution;
	uint8_t fixedVopRate;
	uint8_t reserved4;
	uint16_t fixedVopTimeIncrement;
} M4vdecPictureNode;

uint32_t m4vdecVersion(void)
{
	return M4VDEC_VERSION;
}

static void waitAccepted(M4vdecInstance *instance)
{
	vnzThreadWaitFlags(instance->flagAccepted);
}

static void waitReply(M4vdecInstance *instance, uint32_t bit)
{
	while (!(instance->waitToggle & bit) && !(instance->waitReady & bit))
		while (!vkMessageReceive(instance->queue, (uint32_t)instance))
			vnzThreadWaitFlags(instance->flagReplied);
	if (instance->waitToggle & bit) {
		instance->waitToggle &= ~bit;
	} else {
		instance->waitToggle |= bit;
		instance->waitReady &= ~bit;
	}
}

static void requestBuild(M4vdecInstance *instance, uint32_t type, const VideoAu *au,
	const M4vdecParam *param)
{
	M4vdecRequest *request;

	request = (M4vdecRequest *)vnzPublicOpenPrivate(instance->request, sizeof(*request));
	request->codec = SERVICE_CODEC_M4VDEC;
	request->serviceState = instance->serviceState;
	if (type == REQUEST_DECODE) {
		request->code = SERVICE_DECODE;
	} else if (type == REQUEST_AU) {
		request->code = SERVICE_AU;
		request->drain = instance->mode == MODE_DRAIN;
		request->sequence = instance->mode == MODE_SEQUENCE;
		request->context = instance->mode == MODE_CONTEXT;
		request->restart = instance->mode == MODE_RESTART;
		if (au) {
			request->dts[0] = au->dts[0];
			request->dts[1] = au->dts[1];
			request->pts[0] = au->pts[0];
			request->pts[1] = au->pts[1];
		}
		if (request->restart) {
			request->auMode = AU_RESET;
		} else if (!request->context) {
			request->buffer = au->buffer;
			request->bufferSize = au->bufferSize;
			request->esSize = au->esSize;
			request->esOffset = au->es - au->buffer;
			request->auMode = request->sequence ? AU_RESET : au->advance;
		} else {
			request->contextAddress = instance->context;
			request->horizontal = param->horizontal;
			request->vertical = param->vertical;
			request->macroblockData = param->macroblockData;
		}
	} else if (type == REQUEST_INITIALIZE) {
		request->instance = instance;
		request->code = SERVICE_OPEN;
		request->thread = vnzThreadCurrent();
		request->queue = instance->queue;
		request->frames = instance->frames;
		request->option = param->option;
		request->option5[0] = param->option5[0];
		request->option5[1] = param->option5[1];
		request->option4[0] = param->option4[0];
		request->option4[1] = param->option4[1];
	} else if (type == REQUEST_TERMINATE) {
		request->code = SERVICE_CLOSE;
	}
	vnzPublicClosePrivate((uint32_t)request, sizeof(*request));
}

static void requestSend(M4vdecInstance *instance, uint32_t type, const VideoAu *au,
	const M4vdecParam *param)
{
	requestBuild(instance, type, au, param);
	vkMessageSend(instance->serviceQueue, videoServiceRequest, instance->request);
	waitAccepted(instance);
}

static void replyRead(M4vdecInstance *instance, uint32_t address)
{
	M4vdecParam *param;
	M4vdecReply *reply;
	uint32_t kind, status;

	reply = (M4vdecReply *)vnzPublicOpenProtected(address, REPLY_SIZE);
	kind = reply->kind;
	if (kind == SERVICE_REPLY_PICTURES) {
		instance->numOfPictures = reply->numOfPictures;
		instance->pictures = reply->pictures;
		goto close;
	}
	if (kind == SERVICE_REPLY_AU) {
		if (reply->sequence) {
			param = (M4vdecParam *)instance->pictures;
			param->horizontal = reply->horizontal;
			param->vertical = reply->vertical;
			param->sequenceInfo = reply->sequenceInfo;
			param->macroblockData = reply->macroblockData;
		} else if (reply->reset) {
			instance->releasedFrame[1] = instance->releasedFrame[0];
			instance->released = instance->released >= 0;
			instance->releasedFrame[0] = 0;
		} else if (reply->frame) {
			instance->releasedFrame[1] = instance->releasedFrame[0];
			instance->released = instance->released >= 0;
			instance->releasedFrame[0] = reply->frame;
		}
		status = reply->status;
		if (status & 0xc000)
			status |= 0x10000000;
	} else {
		instance->serviceSlot = reply->serviceSlot;
		instance->serviceState = reply->serviceState;
		status = reply->status ? M4VDEC_ERROR : 0;
	}
	instance->status = status;
close:
	vnzPublicCloseProtected((uint32_t)reply, REPLY_SIZE);
	instance->replyKind = kind;
}

static void replyDone(M4vdecInstance *instance)
{
	uint32_t bit = WAIT_AU;

	if (instance->replyKind != SERVICE_REPLY_PICTURES)
		bit = instance->replyKind == SERVICE_REPLY_AU ? WAIT_DECODE : WAIT_DONE;
	if (instance->waitToggle & bit) {
		instance->waitToggle &= ~bit;
		instance->waitReady |= bit;
	} else {
		instance->waitToggle |= bit;
	}
}

/* the engine's replies, received on the instance's queue */
void m4vdecReceive(uint32_t argument, uint32_t address)
{
	M4vdecInstance *instance = (M4vdecInstance *)argument;

	if (!address) {
		instance->status = M4VDEC_ERROR;
		instance->replyKind = WAIT_DONE;
	} else {
		replyRead(instance, address);
		vkMessageSend(instance->serviceQueue2, videoServiceRelease, instance->serviceSlot);
	}
	replyDone(instance);
}

/* the engine's pictures as the ARM's records */
/* the VOL's fixed rate as the library's frame rate code */
static uint32_t frameRateCode(const M4vdecPictureNode *node)
{
	int32_t increment = node->fixedVopTimeIncrement;
	int32_t resolution = node->timeIncrementResolution;
	int32_t divisor;

	if (!resolution || !node->fixedVopRate || !increment)
		return FRAME_RATE_UNKNOWN;
	if (!(increment % 1001)) {
		divisor = increment / 1001;
		increment /= divisor;
		resolution /= divisor;
	} else if (increment != 1 && !(resolution % increment)) {
		divisor = increment;
		increment /= divisor;
		resolution /= divisor;
	}
	if (resolution == 24000 && increment == 1001)
		return 1;
	if (resolution == 24 && increment == 1)
		return 2;
	if (resolution == 25 && increment == 1)
		return 3;
	if (resolution == 30000 && increment == 1001)
		return 4;
	if (resolution == 30 && increment == 1)
		return 5;
	if (resolution == 50 && increment == 1)
		return 6;
	if (resolution == 60000 && increment == 1001)
		return 7;
	if (resolution == 60 && increment == 1)
		return 8;
	return FRAME_RATE_UNKNOWN;
}

/* the engine's pictures as the ARM's records */
static void getPictures(M4vdecInstance *instance, VideoAu *au, VideoM4vPicture *picture,
	uint32_t numOfPictures)
{
	M4vdecPictureNode *node = (M4vdecPictureNode *)instance->pictures;
	int32_t count = instance->numOfPictures;

	for (; numOfPictures && count >= 1; numOfPictures--, count--, picture++) {
		picture->plane[0] = node->luma;
		picture->plane[1] = 0;
		picture->plane[2] = node->chroma;
		picture->plane[3] = 0;
		picture->width = node->width;
		picture->pitch = node->pitch;
		picture->height = node->height;
		picture->chromaHeight = node->chromaHeight;
		picture->info[0] = node->info[0];
		picture->info[1] = 0;
		picture->info[2] = node->info[1];
		picture->info[3] = node->info[2];
		picture->data = node->data;
		picture->vopInfo[0] = node->vopInfo[0];
		picture->vopInfo[1] = node->vopInfo[1];
		picture->vopInfo[2] = node->vopInfo[2];
		picture->vopInfo[3] = node->vopInfo[3];
		picture->frameRate = frameRateCode(node);
		picture->dts[0] = node->dts[0];
		picture->dts[1] = node->dts[1];
		picture->pts[0] = node->pts[0];
		picture->pts[1] = node->pts[1];
		picture->userDataSize = node->userDataSize;
		picture->userData = node->userData;
		node = node->next;
	}
	instance->pictures = (uint32_t)node;
	instance->numOfPictures = count;
}

uint32_t m4vdecQueryMemory(const uint32_t *param)
{
	uint32_t size;

	size = 256 + videoServiceDecoderSize();
	size += m4vdecQueryParserMemory();
	return size + m4vdecQueryDecoderMemory();
}

int32_t m4vdecInitialize(uint32_t *handle, const uint32_t *param, uint32_t workAddress,
	uint32_t workSize)
{
	const M4vdecParam *p = (const M4vdecParam *)param;
	VideoLibrary *library = p->library;
	uint32_t flag, flag2, serviceQueue, serviceQueue2;
	M4vdecInstance *instance;
	VkMessageQueue *queue;
	volatile uint8_t *lock;
	uint32_t size;

	*handle = 0;
	if (!library)
		return M4VDEC_ERROR_LIBRARY_STATE;
	if (((workAddress + 63) & ~63) != workAddress)
		return M4VDEC_ERROR_WORK_ALIGNMENT;
	size = m4vdecQueryMemory(param);
	if (!size || workSize < size)
		return M4VDEC_ERROR_WORK_SIZE;

	lock = library->lock;
	if (!lock)
		return M4VDEC_ERROR;
	vnzSpinLock(lock);
	if (library->reserved3 >= library->numOfDecoders) {
		vnzSpinUnlock(lock);
		return M4VDEC_ERROR_IN_USE;
	}
	if (!library->active) {
		vnzSpinUnlock(lock);
		return M4VDEC_ERROR_LIBRARY_STATE;
	}
	library->active = 2;
	library->reserved3++;
	vnzSpinUnlock(lock);

	flag = library->coreMask;
	flag2 = library->coreMask2;
	serviceQueue2 = library->serviceInfo[1];
	serviceQueue = library->serviceInfo[0];
	/* Sony's code keeps the library counted in use when this fails */
	queue = vkMessageQueueCreate(vnzThreadCurrent(), flag2, 2);
	if (!queue)
		return M4VDEC_ERROR;

	instance = (M4vdecInstance *)vnzPublicOpenPrivate(workAddress, INSTANCE_SIZE);
	instance->serviceQueue = (VkMessageQueue *)serviceQueue;
	instance->queue = queue;
	instance->flagReplied = flag2;
	instance->lastMode = AU_RESET;
	instance->fieldDecode = 1;
	instance->inFlight = 0;
	instance->waitToggle = 0;
	instance->waitReady = 0;
	instance->numOfPictures = 0;
	instance->pictures = 0;
	instance->frames = workAddress + 256;
	instance->releasedFrame[0] = 0;
	instance->request = workAddress + 128;
	instance->releasedFrame[1] = 0;
	instance->serviceQueue2 = (VkMessageQueue *)serviceQueue2;
	instance->flagAccepted = flag;
	instance->released = -1;
	requestSend(instance, REQUEST_INITIALIZE, 0, p);
	waitReply(instance, WAIT_DONE);
	if (!instance->status) {
		instance->library = library;
		instance->state = STATE_INITIALIZED;
		*handle = (uint32_t)instance;
		return 0;
	}

	if (queue)
		vkMessageQueueDelete(queue);
	vnzPublicClosePrivate((uint32_t)instance, INSTANCE_SIZE);
	lock = library->lock;
	if (lock) {
		vnzSpinLock(lock);
		library->reserved3--;
		if (!library->reserved3 && !library->reserved4)
			library->active = 1;
		vnzSpinUnlock(lock);
	}
	return M4VDEC_ERROR;
}

static int32_t checkAu(const VideoAu *au)
{
	if (((au->buffer + 255) & ~255) != au->buffer)
		return M4VDEC_ERROR_AU_ALIGNMENT;
	if (au->buffer + au->bufferSize <= au->es || au->es < au->buffer)
		return M4VDEC_ERROR_AU_RANGE;
	if (au->bufferSize < au->esSize)
		return M4VDEC_ERROR_AU_SIZE;
	return 0;
}

/* the sequence an access unit starts: its parameters into param */
int32_t m4vdecQuerySequence(uint32_t handle, VideoAu *au, const uint32_t *param,
	uint32_t pictures)
{
	M4vdecInstance *instance = (M4vdecInstance *)handle;
	int32_t result;

	if (!instance)
		return M4VDEC_ERROR_POINTER;
	result = checkAu(au);
	if (result)
		return result;
	if (instance->state != STATE_INITIALIZED && instance->state != STATE_READY)
		return M4VDEC_ERROR;
	instance->mode = MODE_SEQUENCE;
	requestSend(instance, REQUEST_AU, au, 0);
	instance->pictures = (uint32_t)param;
	waitReply(instance, WAIT_DECODE);
	au->numOfOutput = 0;
	return instance->status;
}

uint32_t m4vdecQueryContext(const uint32_t *param)
{
	const M4vdecParam *p = (const M4vdecParam *)param;
	uint32_t horizontal = p->horizontal, vertical = p->vertical;
	uint32_t size = 0;

	if (horizontal < 16 || horizontal > 1920)
		return 0;
	if (vertical < 16 || vertical > 1088)
		return 0;
	if ((horizontal & 15) || (vertical & 15))
		return 0;
	size += m4vdecQueryFrameMemory(horizontal, vertical);
	size += m4vdecQueryMacroblockMemory(horizontal, vertical, p->macroblockData);
	return size;
}

int32_t m4vdecInitializeContext(uint32_t handle, const uint32_t *param, uint32_t address,
	uint32_t size)
{
	M4vdecInstance *instance = (M4vdecInstance *)handle;
	uint32_t needed;

	if (!instance)
		return M4VDEC_ERROR_POINTER;
	if (instance->state != STATE_INITIALIZED && instance->state != STATE_READY)
		return M4VDEC_ERROR;
	if (((address + 255) & ~255) != address)
		return M4VDEC_ERROR_CONTEXT_ALIGNMENT;
	needed = m4vdecQueryContext(param);
	if (!needed || size < needed)
		return M4VDEC_ERROR_CONTEXT_SIZE;
	instance->context = address;
	instance->mode = MODE_CONTEXT;
	requestSend(instance, REQUEST_AU, 0, (const M4vdecParam *)param);
	waitReply(instance, WAIT_DECODE);
	instance->state = STATE_READY;
	return 0;
}

/* the decoder restarted on a reset unit: what it holds is finished first */
static void restart(M4vdecInstance *instance, const VideoAu *au)
{
	instance->mode = MODE_RESTART;
	if (instance->inFlight) {
		waitReply(instance, WAIT_DECODE);
		waitReply(instance, WAIT_AU);
	}
	requestSend(instance, REQUEST_AU, au, 0);
	requestSend(instance, REQUEST_DECODE, au, 0);
	instance->inFlight = 1;
}

/* the frame the engine released last, if any */
static uint32_t popReleased(M4vdecInstance *instance)
{
	uint32_t frame = 0;

	if (instance->released >= 0) {
		frame = instance->releasedFrame[instance->released];
		instance->released--;
	}
	return frame;
}

int32_t m4vdecDecode(uint32_t handle, VideoAu *au, uint32_t pictures, uint32_t numOfPictures,
	uint32_t *option)
{
	M4vdecInstance *instance = (M4vdecInstance *)handle;
	uint32_t released = 0, started;
	int32_t result;

	if (!instance)
		return M4VDEC_ERROR_POINTER;
	result = checkAu(au);
	if (result)
		return result;
	if (instance->state != STATE_READY && instance->state != STATE_DECODING)
		return instance->state == STATE_INITIALIZED ? M4VDEC_ERROR_NOT_INITIALIZED :
			M4VDEC_ERROR;
	if (au->advance != AU_END && au->advance != AU_NORMAL && au->advance != AU_SKIP &&
	    au->advance != AU_RESET)
		return M4VDEC_ERROR;

	if (au->advance == AU_END) {
		/* no unit: the one in flight is finished */
		if (!instance->numOfPictures && instance->inFlight) {
			waitReply(instance, WAIT_DECODE);
			waitReply(instance, WAIT_AU);
			released = popReleased(instance);
			instance->inFlight = 0;
		}
	} else {
		if (instance->lastMode == AU_NORMAL && au->advance != AU_NORMAL)
			restart(instance, au);
		started = instance->inFlight;
		instance->mode = MODE_DECODE;
		instance->lastMode = au->advance;
		requestSend(instance, REQUEST_AU, au, 0);
		requestSend(instance, REQUEST_DECODE, au, 0);
		if (started) {
			waitReply(instance, WAIT_DECODE);
			waitReply(instance, WAIT_AU);
			released = popReleased(instance);
		} else {
			instance->inFlight = 1;
		}
	}
	au->status = released;
	au->numOfOutput = instance->numOfPictures;
	if (instance->numOfPictures >= 1)
		getPictures(instance, au, (VideoM4vPicture *)pictures, numOfPictures);
	instance->state = STATE_DECODING;
	return instance->status;
}

int32_t m4vdecDecodeStop(uint32_t handle, VideoAu *au, uint32_t pictures,
	uint32_t numOfPictures, uint32_t *option)
{
	M4vdecInstance *instance = (M4vdecInstance *)handle;
	uint32_t released;

	if (!instance)
		return M4VDEC_ERROR_POINTER;
	if (instance->state != STATE_READY && instance->state != STATE_DECODING)
		return M4VDEC_ERROR;
	if (instance->inFlight) {
		waitReply(instance, WAIT_DECODE);
		waitReply(instance, WAIT_AU);
		instance->inFlight = 0;
	}
	if (!instance->numOfPictures) {
		instance->mode = MODE_DRAIN;
		requestSend(instance, REQUEST_AU, au, 0);
		requestSend(instance, REQUEST_DECODE, au, 0);
		waitReply(instance, WAIT_DECODE);
		waitReply(instance, WAIT_AU);
	}
	released = popReleased(instance);
	au->status = released;
	au->numOfOutput = instance->numOfPictures;
	if (instance->numOfPictures >= 1)
		getPictures(instance, au, (VideoM4vPicture *)pictures, numOfPictures);
	instance->state = STATE_READY;
	return instance->status;
}

int32_t m4vdecTerminate(uint32_t handle)
{
	M4vdecInstance *instance = (M4vdecInstance *)handle;
	VideoLibrary *library;
	volatile uint8_t *lock;
	VideoAu au;

	if (!instance)
		return M4VDEC_ERROR_POINTER;
	if (instance->state != STATE_INITIALIZED && instance->state != STATE_READY)
		return M4VDEC_ERROR;
	/* the pictures still held are drained through a scratch unit */
	do {
		instance->mode = MODE_DRAIN;
		requestSend(instance, REQUEST_AU, &au, 0);
		requestSend(instance, REQUEST_DECODE, &au, 0);
		waitReply(instance, WAIT_DECODE);
		waitReply(instance, WAIT_AU);
	} while (instance->numOfPictures >= 1);
	requestSend(instance, REQUEST_TERMINATE, 0, 0);
	waitReply(instance, WAIT_DONE);
	if (instance->queue)
		vkMessageQueueDelete(instance->queue);
	library = instance->library;
	instance->state = STATE_NONE;
	vnzPublicClosePrivate((uint32_t)instance, INSTANCE_SIZE);
	lock = library->lock;
	if (lock) {
		vnzSpinLock(lock);
		library->reserved3--;
		if (!library->reserved3 && !library->reserved4)
			library->active = 1;
		vnzSpinUnlock(lock);
	}
	return 0;
}
