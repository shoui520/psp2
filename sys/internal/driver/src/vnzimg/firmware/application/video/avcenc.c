/*
 * The AVC encoder's interface (0x8224c4-0x823588): parameter checks, the
 * instance in the work memory, and the requests to the codec service.
 *
 * The encoder itself runs in the codec service (the shared library) on
 * other cores: a request is a 128-byte message sent to the service's
 * queue; the service accepts it (the instance's flag) and later replies
 * through the instance's own queue. Each reply kind toggles a bit in two
 * masks, so a wait matches one reply to one request.
 */
#include "video.h"

#define SEQUENCE_SIZE			128
#define NAL_TABLE_SIZE			192

/* video.h's states, requests and waits under the encoder's names */
#define STATE_ENCODING			STATE_DECODING

#define REQUEST_PICTURE			REQUEST_AU
#define REQUEST_ENCODE			REQUEST_DECODE

#define SERVICE_PICTURE			SERVICE_AU
#define SERVICE_ENCODE			SERVICE_DECODE

/* the replies, and the bit each toggles */
#define REPLY_PICTURE			8
#define REPLY_TERMINATE			2
#define WAIT_PICTURE			WAIT_AU
#define WAIT_ENCODE			WAIT_DECODE

/* the frame types */
#define FRAME_NONE			0
#define FRAME_PICTURE			1
#define FRAME_IDR			0x20
#define FRAME_I				0x40
#define FRAME_P				0x80

#define AVCENC_ERROR			((int32_t)0x80000000)
#define AVCENC_ERROR_WORK_ALIGNMENT	((int32_t)0x80000001)
#define AVCENC_ERROR_WORK_SIZE		((int32_t)0x80000002)
#define AVCENC_ERROR_WORK2_ALIGNMENT	((int32_t)0x80000003)
#define AVCENC_ERROR_WORK2_SIZE		((int32_t)0x80000004)
#define AVCENC_ERROR_LIBRARY_STATE	((int32_t)0x80000010)
#define AVCENC_ERROR_IN_USE		((int32_t)0x80000011)
#define AVCENC_ERROR_NOT_INITIALIZED	((int32_t)0x80000020)
#define AVCENC_ERROR_POINTER		((int32_t)0x80000030)
#define AVCENC_ERROR_FRAME_ALIGNMENT	((int32_t)0x10000001)
#define AVCENC_ERROR_FRAME_SIZE		((int32_t)0x10000002)
#define AVCENC_ERROR_PROFILE		((int32_t)0x10002010)
#define AVCENC_ERROR_LEVEL		((int32_t)0x10002011)
#define AVCENC_ERROR_PARAM		((int32_t)0x10004000)
#define AVCENC_ERROR_WIDTH		((int32_t)0x10004001)
#define AVCENC_ERROR_HEIGHT		((int32_t)0x10004002)
#define AVCENC_ERROR_PARAM3		((int32_t)0x10004003)
#define AVCENC_ERROR_SLICES		((int32_t)0x10004004)
#define AVCENC_ERROR_PARAM5		((int32_t)0x10004005)
#define AVCENC_ERROR_PARAM6		((int32_t)0x10004006)
#define AVCENC_ERROR_BASELINE		((int32_t)0x10004007)
#define AVCENC_ERROR_BIT_RATE		((int32_t)0x10004008)
#define AVCENC_ERROR_MB_RATE		((int32_t)0x10004009)
#define AVCENC_ERROR_OPTION		((int32_t)0x10004010)
#define AVCENC_ERROR_RATE		((int32_t)0x10008002)
#define AVCENC_ERROR_STATE		((int32_t)0x10008010)
#define AVCENC_ERROR_STREAM_TYPE	((int32_t)0x10008012)
#define AVCENC_ERROR_INTERVAL		((int32_t)0x10008013)
#define AVCENC_ERROR_TIME_SCALE		((int32_t)0x10008014)
#define AVCENC_ERROR_FRAME_RATE		((int32_t)0x10008015)

#define AVCENC_VERSION			0x02080a97

/* the ARM's initialization parameters (the command's param) */
typedef struct AvcencParam {
	VideoLibrary *library;
	uint8_t profile;
	uint8_t level;			/* 10 times the level number */
	uint16_t horizontal;
	uint16_t vertical;
	uint8_t reserved;		/* must be 0 */
	uint8_t numOfSlices;
	uint8_t reserved2;		/* must be 0 */
	uint8_t reserved3;		/* must be 1 */
	uint8_t entropyCoding;		/* 1 is not baseline */
	uint8_t mode;			/* 0..2 */
	uint32_t numUnitsInTick;
	uint32_t timeScale;
	uint32_t bitRate;
	uint32_t maxBitRate;
	uint8_t option;			/* 0 or 16 */
	uint8_t fixed;			/* 1: neither option below */
	uint8_t option2;		/* 0 or 1 */
	uint8_t reserved4;
	const uint8_t *vui;		/* a byte and two halfwords */
	const uint8_t *option3;		/* six bytes */
	uint32_t option4;
} AvcencParam;

/* the encode parameters (AVCENC_SET_PARAM) */
typedef struct AvcencEncodeParam {
	uint32_t bitRate;		/* 0: constant quantizer */
	uint32_t maxBitRate;
	uint32_t interval;
	uint32_t idrInterval;
	uint32_t numUnitsInTick;
	uint32_t timeScale;
} AvcencEncodeParam;

/* a sequence's settings, two of them used in turn */
typedef struct AvcencSequence {
	uint16_t horizontal;
	uint16_t vertical;
	uint8_t profile;
	uint8_t level;
	uint8_t option;
	uint8_t numOfSlices;
	uint32_t bitRate;
	uint32_t maxBitRate;
	uint32_t cpbSize;
	uint32_t maxCpbSize;
	uint8_t reserved;
	uint8_t reserved2;
	uint8_t reserved3;
	uint8_t entropyCoding;
	uint8_t mode;
	uint8_t vuiFlag;
	uint8_t option2;
	uint8_t reserved4;
	uint16_t vui[2];
	uint32_t option4;
	uint8_t fixed;
	uint8_t option3Present;
	uint8_t option3[6];
	uint32_t pictureBitRate;
	uint32_t pictureMaxBitRate;
	uint32_t pictureInterval;
	uint32_t pictureIdrInterval;
	uint32_t numUnitsInTick;
	uint32_t timeScale;
	uint32_t luma;
	uint32_t chroma;
	uint16_t width;
	uint16_t height;
	uint16_t pitch;
	uint16_t chromaPitch;
	uint8_t idr;
	uint8_t intra;
	uint8_t predicted;
	uint8_t constantQuantizer;
	uint8_t quantizer;
	uint8_t quantizer2;
	uint8_t reserved5[2];
	uint32_t nalTable;
	uint32_t streamTable;
	uint32_t work;
	uint32_t reserved6[5];
} AvcencSequence;

/* the NAL units a picture made: in the stream buffer, each with a 4-byte start */
typedef struct AvcencNalTable {
	uint8_t count;
	uint8_t info[3];
	uint32_t info2;
	uint32_t info3;
	uint8_t type[12];
	uint8_t refIdc[12];
	uint32_t start[12];
	uint32_t size[12];
	uint32_t reserved[15];
} AvcencNalTable;

/* the stream's headers */
typedef struct AvcencHeaders {
	uint8_t reserved[13];
	uint8_t spsType;
	uint8_t ppsType;
	uint8_t reserved2[10];
	uint8_t spsRefIdc;
	uint8_t ppsRefIdc;
	uint8_t reserved3[13];
	uint32_t spsStart;
	uint32_t ppsStart;
	uint32_t reserved4[10];
	uint32_t spsSize;
	uint32_t ppsSize;
} AvcencHeaders;

/* the instance, in the work memory (private while it is changed) */
typedef struct AvcencInstance {
	VideoLibrary *library;
	uint32_t seiArea;
	uint32_t serviceState;		/* the service's, from its last reply */
	uint32_t flagAccepted;		/* set when the service took a request */
	uint32_t flagReplied;		/* set with a reply in queue */
	uint32_t request;		/* the request message */
	VkMessageQueue *serviceQueue;
	VkMessageQueue *queue;		/* the replies */
	VkMessageQueue *serviceQueue2;
	uint32_t replyKind;
	uint32_t work2;
	uint32_t pictureInfo;
	AvcencSequence *sequences;
	int32_t status;
	int8_t serviceSlot;
	int8_t pictureCount;
	uint8_t inFlight;		/* a picture went out unanswered */
	uint8_t drain;			/* 3: the pictures held are flushed */
	uint8_t state;
	uint8_t waitToggle;
	uint8_t waitReady;
	uint8_t sequence;		/* which of the two is next */
	AvcencNalTable *nalTables;
	AvcencHeaders *headers;
	uint8_t nalIndex;
	uint8_t seiPosition;
	uint8_t pending;		/* pictures waiting for output */
	uint8_t constantQuantizer;
	uint32_t stream;
	uint32_t frameMemorySize;
	uint32_t streamAddress;
	uint32_t streamSize;
	uint16_t horizontal;
	uint16_t vertical;
	uint32_t defaultInterval;
	uint32_t interval;
	uint32_t bitRate;
	uint32_t maxBitRate;
	uint32_t numUnitsInTick;
	uint32_t timeScale;
	uint16_t idrInterval;
	uint8_t quantizer;
	uint8_t quantizer2;
	uint8_t mode;
	uint8_t reserved[3];
} AvcencInstance;

/* the request message the service reads */
typedef struct AvcencRequest {
	AvcencInstance *instance;
	uint32_t reserved;
	uint32_t serviceState;
	uint32_t code;
	uint8_t reserved2[2];
	uint8_t mode;
	uint8_t reserved3[33];
	uint32_t data;
	uint32_t address;		/* the sequence, or the stream */
	uint32_t value;			/* the thread, or the frame memory size */
	VkMessageQueue *queue;
	uint32_t seiArea;
	uint32_t reserved4;
	union {
		uint32_t work2;
		struct {
			uint8_t reserved5[2];
			uint8_t reset;
			uint8_t drain;
		};
	};
	uint32_t sequences;
	uint8_t initialize;
	uint8_t reserved6[43];
} AvcencRequest;

/* the service's reply */
typedef struct AvcencReply {
	uint32_t reserved;
	uint32_t serviceState;
	uint32_t serviceSlot;
	uint32_t reserved2;
	uint32_t kind;
	uint16_t *size;			/* REPLY_PICTURE: where the picture size goes */
	uint32_t streamOffset;
	uint32_t streamSize;
	uint32_t status;
	uint32_t pictureInfo;
	uint8_t pictureCount;
	uint8_t reserved3[7];
	int16_t width;
	int16_t height;
	uint32_t reserved4[3];
} AvcencReply;

/* the service's messages, in the shared library */
/* the H.264 limits by level (Table A-1): rates in 1000 and 1200 bit/s units */
static const uint16_t maxBitRate[33] = {
	64, 192, 384, 768, 0, 0, 0, 0, 0, 0,
	2000, 4000, 4000, 0, 0, 0, 0, 0, 0, 0,
	10000, 14000, 20000, 0, 0, 0, 0, 0, 0, 0,
	20000, 50000, 50000,
};

static const uint16_t maxCpbSize[33] = {
	175, 500, 1000, 2000, 0, 0, 0, 0, 0, 0,
	2000, 4000, 4000, 0, 0, 0, 0, 0, 0, 0,
	10000, 14000, 20000, 0, 0, 0, 0, 0, 0, 0,
	25000, 62500, 62500,
};

static const uint32_t maxMacroblockRate[33] = {
	1485, 3000, 6000, 11880, 0, 0, 0, 0, 0, 0,
	11880, 19800, 20250, 0, 0, 0, 0, 0, 0, 0,
	40500, 108000, 216000, 0, 0, 0, 0, 0, 0, 0,
	245760, 245760, 522240,
};

uint32_t avcencVersion(void)
{
	return AVCENC_VERSION;
}

static void waitAccepted(AvcencInstance *instance)
{
	vnzThreadWaitFlags(instance->flagAccepted);
}

/* waits for the reply that toggles bit */
static void waitReply(AvcencInstance *instance, uint32_t bit)
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

static void requestBuild(AvcencInstance *instance, uint32_t type, uint32_t data)
{
	AvcencRequest *request;
	AvcencSequence *sequence;

	request = (AvcencRequest *)vnzPublicOpenPrivate(instance->request, sizeof(*request));
	request->serviceState = instance->serviceState;
	request->mode = instance->mode;
	if (type == REQUEST_ENCODE) {
		request->code = SERVICE_ENCODE;
		request->data = data;
		request->drain = instance->drain == 3;
		request->address = instance->stream;
		request->value = instance->frameMemorySize;
	} else if (type == REQUEST_PICTURE) {
		request->code = SERVICE_PICTURE;
		request->data = data;
		request->reset = 0;
		request->drain = instance->drain == 3;
		sequence = &instance->sequences[instance->sequence];
		request->address = (uint32_t)sequence;
		instance->sequence = 1 - instance->sequence;
	} else if (type == REQUEST_INITIALIZE) {
		request->instance = instance;
		request->code = SERVICE_OPEN;
		request->value = (uint32_t)vnzThreadCurrent();
		request->queue = instance->queue;
		request->seiArea = instance->seiArea;
		request->work2 = instance->work2;
		request->initialize = 1;
		request->sequences = (uint32_t)instance->sequences;
	} else if (type == REQUEST_TERMINATE) {
		request->code = SERVICE_CLOSE;
	}
	vnzPublicClosePrivate((uint32_t)request, sizeof(*request));
}

static void requestSend(AvcencInstance *instance, uint32_t type, uint32_t data)
{
	requestBuild(instance, type, data);
	vkMessageSend(instance->serviceQueue, videoServiceRequest, instance->request);
	waitAccepted(instance);
}

static void replyRead(AvcencInstance *instance, uint32_t address)
{
	AvcencReply *reply;
	uint32_t kind, status;

	reply = (AvcencReply *)vnzPublicOpenProtected(address, REPLY_SIZE);
	kind = reply->kind;
	if (kind == REPLY_PICTURE) {
		instance->streamAddress = instance->stream + reply->streamOffset;
		instance->streamSize = reply->streamSize;
		instance->pending = reply->streamSize != 0;
		instance->pictureCount = reply->pictureCount;
		instance->pictureInfo = reply->pictureInfo;
		reply->size[0] = reply->width;
		reply->size[1] = reply->height;
		status = reply->status;
		if (status & 0xc000)
			status |= 0x10000000;
		instance->status = status;
	} else if (kind == REPLY_TERMINATE) {
		instance->nalIndex = 1 - instance->nalIndex;
	} else {
		instance->serviceSlot = reply->serviceSlot;
		instance->serviceState = reply->serviceState;
		instance->status = reply->status ? AVCENC_ERROR : 0;
	}
	vnzPublicCloseProtected((uint32_t)reply, REPLY_SIZE);
	instance->replyKind = kind;
}

static void replyDone(AvcencInstance *instance)
{
	uint32_t bit = WAIT_PICTURE;

	if (instance->replyKind != REPLY_PICTURE)
		bit = instance->replyKind == REPLY_TERMINATE ? WAIT_ENCODE : WAIT_DONE;
	if (instance->waitToggle & bit) {
		instance->waitToggle &= ~bit;
		instance->waitReady |= bit;
	} else {
		instance->waitToggle |= bit;
	}
}

/* the service's replies, received on the instance's queue */
void avcencReceive(uint32_t argument, uint32_t address)
{
	AvcencInstance *instance = (AvcencInstance *)argument;

	if (!address) {
		instance->status = AVCENC_ERROR;
		instance->replyKind = WAIT_DONE;
	} else {
		replyRead(instance, address);
		vkMessageSend(instance->serviceQueue2, videoServiceRelease,
			instance->serviceSlot);
	}
	replyDone(instance);
}

/* the picture's settings into the next sequence */
static void setPicture(AvcencInstance *instance, const VideoEncoderFrame *frame)
{
	AvcencSequence *sequence = &instance->sequences[instance->sequence];

	sequence->luma = frame->luma;
	sequence->chroma = frame->chroma;
	sequence->width = frame->width;
	sequence->height = frame->height;
	sequence->pitch = frame->pitch;
	sequence->chromaPitch = frame->chromaPitch;
	sequence->pictureInterval = instance->interval;
	sequence->pictureBitRate = instance->bitRate;
	sequence->pictureMaxBitRate = instance->maxBitRate;
	sequence->numUnitsInTick = instance->numUnitsInTick;
	sequence->timeScale = instance->timeScale;
	sequence->pictureIdrInterval = instance->idrInterval;
	sequence->constantQuantizer = instance->constantQuantizer;
	sequence->quantizer = instance->quantizer;
	sequence->quantizer2 = instance->quantizer2;
	sequence->intra = frame->type == FRAME_I;
	sequence->predicted = frame->type == FRAME_P;
	sequence->idr = frame->type == FRAME_IDR;
}

/* the picture's output, when one is pending */
static void getOutput(AvcencInstance *instance, VideoEncoderFrame *frame,
	VideoEncoderOutput *output, uint32_t numOfOutputs)
{
	AvcencNalTable *table;
	uint32_t pending = instance->pending;

	if (numOfOutputs && (int32_t)pending >= 1) {
		if (!instance->status) {
			table = &instance->nalTables[instance->nalIndex];
			output->address = instance->streamAddress;
			output->size = instance->streamSize;
			output->numOfSei = table->count;
			output->info[0] = table->info[0];
			output->info[1] = table->info[1];
			output->type = table->info[2];
			output->info2 = table->info2;
			output->info3 = table->info3;
		} else {
			output->type = 128;
			output->info2 = 0;
			output->info[1] = 0;
			output->info[0] = 0;
			output->numOfSei = 0;
			output->size = 0;
			output->address = 0;
			output->info3 = 0;
		}
		pending = 0;
		instance->seiPosition = 0;
		if (output->type == 24) {
			output->type = 8;
			instance->status = AVCENC_ERROR_STREAM_TYPE;
		}
	}
	instance->pending = pending;
}

/* the number of reference frames the level allows here */
static uint32_t numOfRefFrames(uint32_t level)
{
	if (level < 31 || level >= 41)
		return 2;
	return 4;
}

uint32_t avcencQueryMemory(const uint32_t *param)
{
	const AvcencParam *p = (const AvcencParam *)param;
	uint32_t size = 0;

	if ((p->profile == PROFILE_BASELINE || p->profile == PROFILE_MAIN) &&
	    p->level >= 10 && p->level <= 42 && p->numUnitsInTick && p->timeScale &&
	    p->timeScale >= p->numUnitsInTick)
		size = avcencQueryFrameMemory(numOfRefFrames(p->level), p->horizontal,
			p->vertical);
	size += 1088;
	size += videoServiceEncoderSize();
	size += avcencQueryContextMemory();
	return size + avcencQueryRateMemory();
}

int32_t avcencInitialize(uint32_t *handle, const uint32_t *param, uint32_t workAddress,
	uint32_t workSize, uint32_t work2Address, uint32_t work2Size)
{
	const AvcencParam *p = (const AvcencParam *)param;
	VideoLibrary *library = p->library;
	uint32_t mbWidth, mbHeight, maxSlices, refs, level, i;
	uint32_t frameMemory, frames, base, flag, flag2;
	uint32_t bitRateLimit, maxBitRateLimit, cpbLimit, maxCpbLimit;
	uint32_t serviceQueue, serviceQueue2;
	uint64_t available, needed;
	AvcencInstance *instance;
	AvcencSequence *sequence;
	VkMessageQueue *queue;
	volatile uint8_t *lock;
	uint32_t size;

	*handle = 0;
	if (!library)
		return AVCENC_ERROR_LIBRARY_STATE;
	if (((workAddress + 63) & ~63) != workAddress)
		return AVCENC_ERROR_WORK_ALIGNMENT;
	size = avcencQueryMemory(param);
	if (!size || workSize < size)
		return AVCENC_ERROR_WORK_SIZE;
	if (((work2Address + 255) & ~255) != work2Address)
		return AVCENC_ERROR_WORK2_ALIGNMENT;
	size = avcencQueryMemory2(param);
	if (!size || work2Size < size)
		return AVCENC_ERROR_WORK2_SIZE;

	mbWidth = (int32_t)(p->horizontal + 15) >> 4;
	mbHeight = (int32_t)(p->vertical + 15) >> 4;
	maxSlices = mbHeight < 8 ? mbHeight : 8;
	if (p->horizontal < 64 || p->horizontal > 1920 || library->horizontal < p->horizontal)
		return AVCENC_ERROR_WIDTH;
	if (p->vertical < 64 || p->vertical > 1088)
		return AVCENC_ERROR_HEIGHT;
	if (p->reserved)
		return AVCENC_ERROR_PARAM3;
	if (p->numOfSlices < 1 || p->numOfSlices > maxSlices)
		return AVCENC_ERROR_SLICES;
	if (p->reserved2)
		return AVCENC_ERROR_PARAM5;
	if (p->reserved3 != 1)
		return AVCENC_ERROR_PARAM6;
	if (p->profile != PROFILE_BASELINE && p->profile != PROFILE_MAIN)
		return AVCENC_ERROR_PROFILE;
	if (p->profile == PROFILE_BASELINE && p->entropyCoding == 1)
		return AVCENC_ERROR_BASELINE;
	level = p->level;
	if (!((level >= 10 && level < 14) || (level >= 20 && level < 23) ||
	      (level >= 30 && level < 33) || (level >= 40 && level < 43)))
		return AVCENC_ERROR_LEVEL;
	if (!p->timeScale)
		return AVCENC_ERROR_TIME_SCALE;
	if (p->numUnitsInTick && p->timeScale < p->numUnitsInTick * 2)
		return AVCENC_ERROR_FRAME_RATE;
	if (p->mode > 2 || (p->option && p->option != 16) || p->option2 >= 2)
		return AVCENC_ERROR_PARAM;
	if (p->fixed >= 2 || (p->fixed == 1 && (p->vui || p->option3)))
		return AVCENC_ERROR_OPTION;

	bitRateLimit = maxBitRate[level - 10] * 1000;
	maxBitRateLimit = maxBitRate[level - 10] * 1200;
	cpbLimit = maxCpbSize[level - 10] * 1000;
	maxCpbLimit = maxCpbSize[level - 10] * 1200;
	if (bitRateLimit < p->bitRate || maxBitRateLimit < p->maxBitRate)
		return AVCENC_ERROR_BIT_RATE;

	refs = numOfRefFrames(level);
	frameMemory = avcencQueryFrameMemory(refs, p->horizontal, p->vertical);
	frames = avcencQueryFrames(refs, p->horizontal, p->vertical) * 8;
	available = (uint64_t)maxMacroblockRate[level - 10] * (p->numUnitsInTick * 2);
	needed = (uint64_t)(mbWidth * mbHeight) * p->timeScale;
	if (available < needed)
		return AVCENC_ERROR_MB_RATE;

	/* the instance after the frame memory, then its tables */
	base = workAddress + frameMemory;
	for (i = 0; i < 2; i++) {
		sequence = (AvcencSequence *)(base + 256) + i;
		sequence->profile = p->profile;
		sequence->level = p->level;
		sequence->horizontal = p->horizontal;
		sequence->vertical = p->vertical;
		sequence->reserved = p->reserved;
		sequence->numOfSlices = p->numOfSlices;
		sequence->reserved2 = p->reserved2;
		sequence->reserved3 = p->reserved3;
		sequence->entropyCoding = p->entropyCoding;
		sequence->mode = p->mode;
		sequence->option2 = p->option2;
		sequence->numUnitsInTick = p->numUnitsInTick;
		sequence->timeScale = p->timeScale;
		if (!p->bitRate || !p->maxBitRate) {
			sequence->maxBitRate = maxBitRateLimit;
			sequence->bitRate = bitRateLimit;
		} else {
			sequence->bitRate = p->bitRate;
			sequence->maxBitRate = p->maxBitRate;
		}
		sequence->maxCpbSize = maxCpbLimit;
		sequence->cpbSize = cpbLimit;
		sequence->option = p->option;
		sequence->fixed = p->fixed;
		if (p->vui) {
			sequence->vuiFlag = p->vui[0];
			sequence->vui[0] = ((const uint16_t *)p->vui)[2];
			sequence->vui[1] = ((const uint16_t *)p->vui)[3];
		} else {
			sequence->vui[1] = 0;
			sequence->vui[0] = 0;
			sequence->vuiFlag = 0;
		}
		sequence->option4 = p->option4;
		if (p->option3) {
			sequence->option3Present = 1;
			memcpy(sequence->option3, p->option3, 6);
		} else {
			memset(sequence->option3, 0, 6);
			sequence->option3Present = 0;
		}
		sequence->work = workAddress;
		sequence->streamTable = base + 896;
		sequence->nalTable = base + 512 + i * NAL_TABLE_SIZE;
		*(uint8_t *)sequence->nalTable = 0;
	}

	lock = library->lock;
	if (!lock)
		return AVCENC_ERROR;
	vnzSpinLock(lock);
	if (library->reserved4 >= library->numOfEncoders) {
		vnzSpinUnlock(lock);
		return AVCENC_ERROR_IN_USE;
	}
	if (!library->active) {
		vnzSpinUnlock(lock);
		return AVCENC_ERROR_LIBRARY_STATE;
	}
	library->active = 2;
	library->reserved4++;
	vnzSpinUnlock(lock);

	flag2 = library->coreMask2;
	flag = library->coreMask;
	serviceQueue2 = library->serviceInfo[1];
	serviceQueue = library->serviceInfo[0];
	/* Sony's code keeps the library counted in use when this fails */
	queue = vkMessageQueueCreate(vnzThreadCurrent(), flag2, 2);
	if (!queue)
		return AVCENC_ERROR;

	instance = (AvcencInstance *)vnzPublicOpenPrivate(base, INSTANCE_SIZE);
	instance->seiArea = base + 1088;
	instance->queue = queue;
	instance->sequences = (AvcencSequence *)(base + 256);
	instance->request = base + 128;
	instance->nalTables = (AvcencNalTable *)(base + 512);
	instance->serviceQueue = (VkMessageQueue *)serviceQueue;
	instance->serviceQueue2 = (VkMessageQueue *)serviceQueue2;
	instance->headers = (AvcencHeaders *)(base + 896);
	instance->flagAccepted = flag;
	instance->flagReplied = flag2;
	instance->inFlight = 0;
	instance->waitToggle = 0;
	instance->waitReady = 0;
	instance->pictureCount = 0;
	instance->sequence = 0;
	instance->seiPosition = 0;
	instance->pending = 0;
	instance->pictureInfo = 0;
	instance->mode = 2;
	instance->stream = workAddress + 256;
	instance->nalIndex = 1;
	instance->frameMemorySize = frameMemory;
	instance->work2 = work2Address;
	instance->horizontal = p->horizontal;
	instance->vertical = p->vertical;
	instance->defaultInterval = frames;
	requestSend(instance, REQUEST_INITIALIZE, (uint32_t)param);
	waitReply(instance, WAIT_DONE);
	if (!instance->status) {
		instance->library = library;
		instance->state = STATE_INITIALIZED;
		*handle = (uint32_t)instance;
		return 0;
	}

	vkMessageQueueDelete(queue);
	vnzPublicClosePrivate((uint32_t)instance, INSTANCE_SIZE);
	lock = library->lock;
	if (lock) {
		vnzSpinLock(lock);
		library->reserved4--;
		if (!library->reserved3 && !library->reserved4)
			library->active = 1;
		vnzSpinUnlock(lock);
	}
	return AVCENC_ERROR;
}

uint32_t avcencQueryMemory2(const uint32_t *param)
{
	const AvcencParam *p = (const AvcencParam *)param;

	if (p->horizontal < 64 || p->horizontal > 1920)
		return 0;
	if (p->vertical < 64 || p->vertical > 1088)
		return 0;
	return avcencQueryStreamMemory(1);
}

uint32_t avcencQueryMemory3(const uint32_t *param)
{
	const AvcencParam *p = (const AvcencParam *)param;

	if (p->horizontal < 64 || p->horizontal > 1920)
		return 0;
	if (p->vertical < 64 || p->vertical > 1088)
		return 0;
	return avcencQueryFrames(numOfRefFrames(p->level), p->horizontal, p->vertical);
}

/* the SPS and PPS the initialization made, past their start codes */
int32_t avcencGetParameterSets(uint32_t handle, uint32_t sps, uint32_t pps)
{
	AvcencInstance *instance = (AvcencInstance *)handle;
	VideoEncoderBuffer *spsBuffer = (VideoEncoderBuffer *)sps;
	VideoEncoderBuffer *ppsBuffer = (VideoEncoderBuffer *)pps;
	AvcencHeaders *headers;

	if (!instance)
		return AVCENC_ERROR_POINTER;
	if (instance->state != STATE_INITIALIZED && instance->state != STATE_READY &&
	    instance->state != STATE_ENCODING)
		return AVCENC_ERROR_STATE;
	headers = instance->headers;
	spsBuffer->address = headers->spsStart + 4;
	spsBuffer->size = headers->spsSize - 4;
	spsBuffer->type = headers->spsType;
	spsBuffer->refIdc = headers->spsRefIdc;
	ppsBuffer->address = headers->ppsStart + 4;
	ppsBuffer->size = headers->ppsSize - 4;
	ppsBuffer->type = headers->ppsType;
	ppsBuffer->refIdc = headers->ppsRefIdc;
	return 0;
}

int32_t avcencSetParam(uint32_t handle, const uint32_t *param)
{
	AvcencInstance *instance = (AvcencInstance *)handle;
	const AvcencEncodeParam *p = (const AvcencEncodeParam *)param;
	AvcencSequence *sequence, *previous;
	uint32_t defaultInterval;

	if (!instance)
		return AVCENC_ERROR_POINTER;
	defaultInterval = instance->defaultInterval;
	if (instance->state != STATE_INITIALIZED && instance->state != STATE_READY &&
	    instance->state != STATE_ENCODING)
		return AVCENC_ERROR_STATE;
	if (!p->idrInterval)
		return AVCENC_ERROR_INTERVAL;
	sequence = &instance->sequences[instance->sequence];
	if (p->bitRate && (p->maxBitRate < p->bitRate || sequence->bitRate < p->bitRate ||
	    sequence->bitRate < p->maxBitRate))
		return AVCENC_ERROR_RATE;

	instance->bitRate = p->bitRate;
	if (p->bitRate) {
		instance->constantQuantizer = 0;
		instance->maxBitRate = p->maxBitRate;
		instance->interval = p->interval ? p->interval : defaultInterval;
	} else {
		instance->constantQuantizer = 1;
		instance->quantizer = p->maxBitRate;
		instance->interval = defaultInterval;
		instance->quantizer2 = p->interval;
	}
	instance->idrInterval = p->idrInterval;
	if (p->numUnitsInTick && p->timeScale) {
		if (p->timeScale < p->numUnitsInTick * 2)
			return AVCENC_ERROR_FRAME_RATE;
		instance->numUnitsInTick = p->numUnitsInTick;
		instance->timeScale = p->timeScale;
	} else {
		previous = &instance->sequences[1 - instance->sequence];
		instance->numUnitsInTick = previous->numUnitsInTick;
		instance->timeScale = previous->timeScale;
	}
	if (instance->state == STATE_INITIALIZED)
		instance->state = STATE_READY;
	return 0;
}

int32_t avcencEncode(uint32_t handle, VideoEncoderFrame *frame, uint32_t outputs,
	uint32_t numOfOutputs)
{
	AvcencInstance *instance = (AvcencInstance *)handle;
	uint32_t started, done;

	if (!instance)
		return AVCENC_ERROR_POINTER;
	if (instance->state != STATE_READY && instance->state != STATE_ENCODING)
		return instance->state == STATE_INITIALIZED ? AVCENC_ERROR_NOT_INITIALIZED :
			AVCENC_ERROR;
	if (frame->type != FRAME_NONE && frame->type != FRAME_PICTURE &&
	    frame->type != FRAME_IDR && frame->type != FRAME_I && frame->type != FRAME_P)
		return AVCENC_ERROR_PARAM;
	if (!outputs && numOfOutputs)
		return AVCENC_ERROR;

	if (frame->type == FRAME_NONE) {
		/* no picture: the one in flight is finished */
		if (!instance->pending && instance->inFlight) {
			waitReply(instance, WAIT_ENCODE);
			waitReply(instance, WAIT_PICTURE);
			instance->inFlight = 0;
		}
	} else {
		if ((frame->luma & 0xff) || (frame->chroma & 0xff))
			return AVCENC_ERROR_FRAME_ALIGNMENT;
		if (frame->width < 64 || instance->horizontal < frame->width ||
		    frame->height < 64 || instance->vertical < frame->height ||
		    (frame->width & 15) || (frame->height & 15) ||
		    frame->pitch < frame->width || (frame->pitch & 15) ||
		    frame->chromaPitch < frame->width || (frame->chromaPitch & 7))
			return AVCENC_ERROR_FRAME_SIZE;
		setPicture(instance, frame);
		started = instance->inFlight;
		instance->drain = 0;
		requestSend(instance, REQUEST_PICTURE, (uint32_t)frame);
		requestSend(instance, REQUEST_ENCODE, (uint32_t)frame);
		if (started) {
			waitReply(instance, WAIT_ENCODE);
			waitReply(instance, WAIT_PICTURE);
		} else {
			instance->inFlight = 1;
		}
	}
	done = instance->status ? 0 : instance->pending;
	frame->numOfNal = done;
	getOutput(instance, frame, (VideoEncoderOutput *)outputs, numOfOutputs);
	instance->state = STATE_ENCODING;
	return instance->status;
}

int32_t avcencEncodeStop(uint32_t handle, VideoEncoderFrame *frame, uint32_t outputs,
	uint32_t numOfOutputs)
{
	AvcencInstance *instance = (AvcencInstance *)handle;
	uint32_t done;

	if (!instance)
		return AVCENC_ERROR_POINTER;
	if ((instance->state != STATE_READY && instance->state != STATE_ENCODING) ||
	    !outputs || !numOfOutputs)
		return AVCENC_ERROR;
	if (instance->inFlight) {
		waitReply(instance, WAIT_ENCODE);
		waitReply(instance, WAIT_PICTURE);
		instance->inFlight = 0;
	}
	if (!instance->pending) {
		instance->drain = 3;
		requestSend(instance, REQUEST_PICTURE, (uint32_t)frame);
		requestSend(instance, REQUEST_ENCODE, (uint32_t)frame);
		waitReply(instance, WAIT_ENCODE);
		waitReply(instance, WAIT_PICTURE);
	}
	done = instance->status ? 0 : instance->pending;
	frame->numOfNal = done;
	getOutput(instance, frame, (VideoEncoderOutput *)outputs, numOfOutputs);
	instance->state = STATE_READY;
	return instance->status;
}

/* the next SEI unit of the last picture, or an empty one when none is left */
int32_t avcencGetSei(uint32_t handle, uint32_t buffer)
{
	AvcencInstance *instance = (AvcencInstance *)handle;
	VideoEncoderBuffer *sei = (VideoEncoderBuffer *)buffer;
	AvcencNalTable *table;
	uint32_t position;

	if (!instance)
		return AVCENC_ERROR_POINTER;
	if (instance->state != STATE_READY && instance->state != STATE_ENCODING)
		return AVCENC_ERROR;
	position = instance->seiPosition;
	table = &instance->nalTables[instance->nalIndex];
	if (position >= table->count) {
		sei->refIdc = 0;
		sei->type = 0;
		sei->size = 0;
		sei->address = 0;
		instance->seiPosition = 255;
	} else {
		sei->address = table->start[position] + 4;
		sei->size = table->size[position] - 4;
		sei->type = table->type[position];
		sei->refIdc = table->refIdc[position];
		instance->seiPosition = position + 1;
	}
	return 0;
}

int32_t avcencTerminate(uint32_t handle)
{
	AvcencInstance *instance = (AvcencInstance *)handle;
	VideoEncoderFrame frame;
	VideoLibrary *library;
	volatile uint8_t *lock;

	if (!instance)
		return AVCENC_ERROR_POINTER;
	if (instance->state != STATE_INITIALIZED && instance->state != STATE_READY)
		return AVCENC_ERROR;
	/* the pictures still held are drained into a scratch frame */
	do {
		instance->drain = 3;
		requestSend(instance, REQUEST_PICTURE, (uint32_t)&frame);
		requestSend(instance, REQUEST_ENCODE, (uint32_t)&frame);
		waitReply(instance, WAIT_ENCODE);
		waitReply(instance, WAIT_PICTURE);
	} while ((int32_t)instance->pending >= 1);
	requestSend(instance, REQUEST_TERMINATE, 0);
	waitReply(instance, WAIT_DONE);
	if (instance->queue)
		vkMessageQueueDelete(instance->queue);
	library = instance->library;
	instance->state = STATE_NONE;
	vnzPublicClosePrivate((uint32_t)instance, INSTANCE_SIZE);
	lock = library->lock;
	if (lock) {
		vnzSpinLock(lock);
		library->reserved4--;
		if (!library->reserved3 && !library->reserved4)
			library->active = 1;
		vnzSpinUnlock(lock);
	}
	return 0;
}
