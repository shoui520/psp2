/*
 * The AVC encoder's stream thread (0x8356b0-0x838788): the second thread of
 * an encoder's slot. It takes the client's frames as they come, keeps the
 * source and reconstructed frames in turn, runs the rate control and the
 * picture's preparing jobs on the other cores, and hands each picture to the
 * encoder engine's thread (avcencencoder.c) when it is ready.
 *
 * Its memory (avcencQueryRateMemory, 20160 bytes): the header, the picture's
 * state (256 bytes), the frames' buffers (128), the rate control's state
 * (576), the slices (64) and the stream's (64) records, and sixteen picture
 * records of 128 bytes at the end, each opened while it is used.
 */
#include "service.h"

#define STREAM_MEMORY		20160
#define STREAM_OPEN_SIZE	18112	/* everything before the picture records */
#define STREAM_HEADER_SIZE	128
#define PICTURE_SIZE		256
#define PICTURE_OFFSET		128
#define PICTURE_FRAME_OFFSET	384	/* the picture's frame record */
#define BUFFERS_SIZE		128
#define BUFFERS_OFFSET		448
#define RATE_STATE_SIZE		576
#define RATE_OFFSET		576
#define SLICES_SIZE		64
#define SLICES_OFFSET		1152
#define STREAM_INFO_OFFSET	1216
#define RECORD_SIZE		128
#define RECORDS			16

/* the thread's states */
#define STREAM_STOPPED		0
#define STREAM_IDLE		2	/* waiting for a frame */
#define STREAM_ENDING		3	/* the stream's end to answer */
#define STREAM_STARTED		4	/* the first picture's jobs */
#define STREAM_ENDED		5
#define STREAM_ENCODING		6	/* the engine has the picture */
#define STREAM_FLUSHING		7

/* the thread's flags */
#define STREAM_FLAG_LOOP	0x00000001
#define STREAM_FLAG_AU		0x00000004
#define STREAM_FLAG_EXIT	0x00008000
#define STREAM_FLAGS		(0x0b360000 | STREAM_FLAG_EXIT | STREAM_FLAG_AU | STREAM_FLAG_LOOP)

/* the picture's state, as the preparing jobs get it (256 bytes) */
typedef struct AvcencPicture {
	VkThread *thread;		/* the stream thread's */
	VkThread *engine;		/* the encoder engine's */
	int32_t id;			/* the codec's */
	uint32_t pictureCount;
	VkVThreadContext *context;
	uint16_t reserved;
	int8_t index;			/* the slot's */
	uint8_t jobsDone;
	uint8_t reserved2[2];
	uint8_t widthInMbs;
	uint8_t heightInMbs;
	uint8_t widthInGroups;		/* of four macroblocks */
	uint8_t heightInGroups;
	uint8_t jobs;			/* the preparing jobs a row of groups takes */
	uint8_t jobGroups;
	uint16_t jobArgument;
	int16_t qpLimit;
	uint32_t luma;			/* the picture's source frame */
	uint32_t chroma;
	uint32_t motion;
	uint32_t previousLuma;		/* the last picture's */
	uint32_t previousChroma;
	uint32_t previousMotion;
	uint32_t clientLuma;		/* the client's frame */
	uint32_t clientChroma;
	uint32_t nextLuma;		/* the next one */
	uint32_t nextChroma;
	uint32_t reference;
	AvcencSequence *settings;
	uint32_t frameRecord;
	uint32_t output;
	volatile uint8_t *lock;		/* the service's encoder lock */
	int16_t width;
	int16_t chromaWidth;
	int16_t motionWidth;
	int16_t clientWidth;
	uint16_t clientHeight;
	uint16_t clientPitch;
	uint16_t clientChromaPitch;
	int16_t nextWidth;
	int16_t nextHeight;
	int16_t nextPitch;
	int16_t nextChromaPitch;
	uint16_t idrInterval;
	uint32_t maxBitRate;
	uint32_t bitRate;
	uint32_t numUnitsInTick;
	uint32_t timeScale;
	uint32_t pictureMaxBitRate;
	uint32_t pictureBitRate;
	uint32_t pictureInterval;
	uint32_t maxCpbSize;
	uint32_t cpbSize;
	int8_t level;
	int8_t option;
	int8_t intra;			/* the settings', from the second frame */
	int8_t predicted;
	int8_t option2;
	int8_t idr;
	int8_t constantQuantizer;
	int8_t quantizer;
	int8_t quantizer2;
	int8_t rateIdr;			/* the rate control's */
	int8_t rateIntra;
	int8_t rateDone;
	int8_t cropped;			/* the frame is not whole macroblocks */
	uint8_t cropLeft;
	uint8_t cropTop;
	uint8_t reserved3;
	uint32_t rowsAbove;
	uint32_t rowsLeft;
	uint32_t rowsMotion;
	uint32_t reserved4[2];
	uint32_t lines;
	uint32_t records;		/* the sixteen picture records */
	uint32_t command[6];		/* the job record's */
	uint32_t reserved5[2];
	uint32_t commandBuffer;
	uint32_t commandBufferEnd;
	uint32_t commandBuffer2;
	uint32_t macroblocks;		/* 64 bytes per macroblock */
	uint32_t reserved6[2];
} AvcencPicture;

/* the picture's slices: the first row of each */
#define MAX_SLICES		8
typedef struct AvcencSlices {
	uint8_t reserved[7];
	uint8_t count;
	uint8_t firstRow[MAX_SLICES];
	uint32_t sliceType;		/* 2 I, 0 P */
	uint8_t reserved2[12];
	uint32_t streamInfo;
	int8_t idr;			/* the picture is an IDR (the last one's, until it is set) */
	uint8_t reserved3;
	uint8_t lumaShift;		/* 7 - QP / 6, 0 from QP 42 */
	uint8_t lumaShift2;
	uint8_t chromaShift;
	uint8_t reserved4[3];
	const int16_t *lumaThreshold;	/* by QP % 6, or QP - 42 from 42 */
	const int16_t *lumaThreshold2;
	const int16_t *chromaThreshold;	/* by QPc % 6 */
	uint8_t reserved5[3];
	int8_t scaled;			/* QP below 42 */
	uint8_t reserved6[4];
} AvcencSlices;

/* the rate control's state, as the stream thread reads it (576 bytes) */
typedef struct AvcencRate {
	uint8_t reserved[164];
	int8_t sceneChange;
	int8_t idrRequested;
	int8_t intraRequested;
	int8_t idrTaken;
	uint8_t reserved2[2];
	int8_t idrCounted;
	uint8_t reserved3[5];
	int8_t idr;
	int8_t intra;
	uint8_t reserved4[116];
	int8_t forceIdr;
	uint8_t reserved5[5];
	uint32_t vclInitialDelay;
	uint32_t vclInitialDelayOffset;
	uint32_t initialDelay;
	uint32_t initialDelayOffset;
	uint8_t reserved6[52];
	int8_t sceneIdr;
	uint8_t reserved7;
	int16_t qpLimit;
	uint8_t reserved8[204];
} AvcencRate;

/* a picture record, as the helper's jobs and the engine read it (128 bytes) */
typedef struct AvcencRecord {
	uint32_t luma;
	uint32_t chroma;
	uint16_t width;
	uint16_t height;
	uint8_t reserved[4];
	int16_t lumaPitch;
	int16_t chromaPitch;
	uint32_t reserved2;
	int8_t cropped;
	uint8_t reserved3[7];
	int16_t reserved4;
	int16_t cropTop;
	int16_t reserved5;
	int16_t cropLeft;
	uint8_t reserved6[88];
} AvcencRecord;

/* a client's output: the record of its picture */
typedef struct AvcencOutput {
	uint8_t reserved[11];
	int8_t next;			/* the output after it */
	uint8_t reserved2[2];
	int8_t hasPicture;
	uint8_t reserved3;
	uint32_t holder;		/* where the picture record goes, at its +44 */
	uint8_t reserved4[108];
} AvcencOutput;

/* the frames' buffers: the source frames and the reconstructed ones, two of each */
typedef struct AvcencBuffers {
	int32_t frames;			/* taken since the stream's start, -1 before */
	uint32_t released;		/* given back */
	uint16_t width;			/* rounded to whole macroblocks */
	uint16_t height;
	uint32_t luma[2];
	uint32_t chroma[2];
	uint32_t motion[2];		/* a quarter of a byte per pixel */
	uint32_t reference;		/* the reference frame's lines, after the frames */
	uint32_t lines;
	uint32_t rowsAbove;
	uint32_t rowsLeft;
	uint32_t rowsMotion;
	uint8_t reserved[8];
	uint32_t frameSize;		/* the three planes */
	uint32_t lumaSize;
	uint32_t chromaSize;
	uint32_t motionSize;
	uint16_t frameWidth;
	uint16_t chromaWidth;		/* rounded to 32 */
	uint16_t motionWidth;		/* (width + 64) / 8, rounded to 64 */
	uint16_t idrCountdown;		/* pictures to the next IDR */
	uint32_t clientLuma;		/* the frame being taken, opened raw */
	uint32_t clientChroma;
	uint16_t clientWidth;
	uint16_t clientHeight;
	uint16_t clientPitch;
	uint16_t clientChromaPitch;
	int8_t idrNext;			/* the next picture is an IDR */
	int8_t idrPending;
	int8_t flushing;
	uint8_t frameNum;
	int8_t refFrames[4];		/* the reference list's frames, -4 to -1 before any */
	uint8_t reserved2[16];
} AvcencBuffers;

/* the stream thread's header */
typedef struct AvcencStream {
	int32_t state;
	AvcencPicture *picture;		/* opened raw while the thread runs */
	uint32_t outputs;		/* the client's outputs, 64 bytes each, 32 in turn */
	uint32_t outputInfo;
	uint32_t jobInfo;		/* the picture's job record, 64 bytes */
	uint32_t slices;		/* an AvcencSlices */
	uint32_t streamInfo;
	AvcencBuffers *buffers;		/* opened private while the thread runs */
	uint32_t rate;			/* an AvcencRate */
	uint32_t request;		/* the frame being taken */
	uint32_t previousRequest;	/* the one before it, answered with the picture */
	uint32_t record;		/* the picture record being filled, 128 bytes */
	VkMessageQueue *auQueue;
	VkMessageQueue *engineQueue;
	VkMessageQueue *wakeQueue;
	VkMessageQueue *innerQueue;
	VkMessageQueue *helperQueue;
	VkVThreadContext *context;
	int8_t started;			/* the sequence's settings taken */
	int8_t ended;			/* the client's frames ended */
	int8_t waitWake;		/* the engine's wake to take */
	uint8_t output;			/* the client's output being filled */
	uint8_t reserved[2];
	int8_t firstPicture;
	int8_t newSequence;		/* the picture starts a sequence */
	uint8_t recordIndex;		/* of the next picture record, 0 to 15 */
	uint8_t reserved2;
	int8_t skipLevel;
	uint8_t reserved3;
	uint32_t pictureFrame;
	uint32_t sliceType;		/* 2 I, 0 P */
	int8_t sliceMode;
	uint8_t source;			/* the source frame of the next picture, 0 or 1 */
	uint8_t reconstructed;		/* its reconstructed frame */
	int8_t option;			/* the frame's */
	uint8_t reserved4[4];
	uint32_t work;			/* the slot's frame memory */
	AvcencSequence *settings;	/* the sequence's, from the client */
	uint8_t reserved5[20];
} AvcencStream;

/* the layouts' sizes */
typedef char pictureSizeCheck[sizeof(AvcencPicture) == PICTURE_SIZE ? 1 : -1];
typedef char buffersSizeCheck[sizeof(AvcencBuffers) == BUFFERS_SIZE ? 1 : -1];
typedef char slicesSizeCheck[sizeof(AvcencSlices) == SLICES_SIZE ? 1 : -1];
typedef char rateSizeCheck[sizeof(AvcencRate) == RATE_STATE_SIZE ? 1 : -1];
typedef char recordSizeCheck[sizeof(AvcencRecord) == RECORD_SIZE ? 1 : -1];
typedef char streamSizeCheck[sizeof(AvcencStream) == STREAM_HEADER_SIZE ? 1 : -1];

/* 0x8364ba: the stream thread's memory */
uint32_t avcencQueryRateMemory(void)
{
	return STREAM_MEMORY;
}

/* 0x8365c8: the slot's queues and threads, and the service's, kept */
void avcencStreamInitialize(uint32_t memory, VideoService *service, int32_t index)
{
	VideoServiceSlot *slot = &service->encoders[index - service->numOfDecoders];
	AvcencStream *stream;
	AvcencPicture *picture;

	stream = (AvcencStream *)vnzPublicOpenPrivate(memory, STREAM_OPEN_SIZE);
	vnzCacheInvalidateInside((uint32_t)stream, STREAM_OPEN_SIZE);
	picture = (AvcencPicture *)((uint32_t)stream + PICTURE_OFFSET);
	stream->auQueue = slot->auQueue;
	stream->engineQueue = service->engineQueue;
	stream->wakeQueue = slot->wakeQueue;
	stream->innerQueue = slot->innerQueue;
	stream->helperQueue = service->helperQueue3;
	stream->context = service->encoderContext;
	stream->work = slot->work2;
	picture->index = index;
	picture->id = slot->id;
	picture->engine = slot->thread;
	picture->context = service->encoderContext;
	picture->records = memory + STREAM_OPEN_SIZE;
	picture->lock = service->encoderLock;
	vnzPublicClosePrivate((uint32_t)stream, STREAM_OPEN_SIZE);
}

/* 0x835aaa: the memory laid out and opened, the frames cleared */
static AvcencStream *streamBegin(uint32_t memory)
{
	AvcencStream *header = (AvcencStream *)memory, *stream;
	AvcencBuffers *buffers;
	AvcencPicture *picture;
	int32_t i;

	header->picture = (AvcencPicture *)(memory + PICTURE_OFFSET);
	header->buffers = (AvcencBuffers *)(memory + BUFFERS_OFFSET);
	header->rate = memory + RATE_OFFSET;
	header->slices = memory + SLICES_OFFSET;
	header->streamInfo = memory + STREAM_INFO_OFFSET;
	stream = (AvcencStream *)vnzPublicOpenPrivate(memory, STREAM_HEADER_SIZE);
	buffers = (AvcencBuffers *)vnzPublicOpenPrivate((uint32_t)stream->buffers, BUFFERS_SIZE);
	picture = stream->picture;
	picture->frameRecord = memory + PICTURE_FRAME_OFFSET;
	picture->thread = vnzThreadCurrent();
	stream->waitWake = 1;
	stream->recordIndex = 0;
	stream->reconstructed = 0;
	stream->source = 0;
	buffers->flushing = 0;
	buffers->released = 0;
	buffers->frames = -1;
	stream->picture = (AvcencPicture *)vnzPublicOpenRaw((uint32_t)stream->picture,
		PICTURE_SIZE);
	stream->buffers = buffers;
	stream->started = 0;
	stream->ended = 0;
	stream->state = STREAM_IDLE;
	for (i = 0; i < 2; i++) {
		buffers->luma[i] = 0;
		buffers->chroma[i] = 0;
		buffers->motion[i] = 0;
	}
	return stream;
}

/* 0x835b62: the thread's flags; an exit stops it */
static uint32_t streamWait(AvcencStream *stream)
{
	uint32_t flags = vnzThreadWaitFlags(STREAM_FLAGS);

	if (flags & STREAM_FLAG_EXIT)
		stream->state = STREAM_STOPPED;
	return flags;
}

/* 0x836418: the frames counted again from the stream's start */
static void streamRestart(AvcencStream *stream)
{
	AvcencBuffers *buffers = stream->buffers;

	buffers->idrCountdown = 1;
	buffers->released = 0;
	buffers->frames = -1;
	stream->ended = 0;
}

/* 0x836432: the frames and the memory closed */
static void streamEnd(AvcencStream *stream)
{
	AvcencBuffers *buffers = stream->buffers;
	uint32_t motionSize = buffers->motionSize, chromaSize = buffers->chromaSize;
	uint32_t picture = (uint32_t)stream->picture, lumaSize = buffers->lumaSize;
	int32_t i;

	for (i = 0; i < 2; i++) {
		if (buffers->luma[i])
			vnzPublicCloseRaw(buffers->luma[i], lumaSize);
		if (buffers->chroma[i])
			vnzPublicCloseRaw(buffers->chroma[i], chromaSize);
		if (buffers->motion[i])
			vnzPublicCloseRaw(buffers->motion[i], motionSize);
	}
	vnzPublicCloseRaw(picture, PICTURE_SIZE);
	vnzPublicClosePrivate((uint32_t)buffers, BUFFERS_SIZE);
	vnzPublicClosePrivate((uint32_t)stream, STREAM_HEADER_SIZE);
}

/*
 * 0x8364c0: the frames laid out in the slot's frame memory: two of each,
 * every one its luma, chroma and motion planes, then the reference rows
 */
static void buffersLayout(AvcencStream *stream)
{
	AvcencBuffers *buffers = stream->buffers;
	uint32_t width = buffers->width, height = buffers->height;
	uint32_t chromaWidth, motionWidth, lumaSize, chromaSize, motionSize, frameSize;
	uint32_t macroblocks, address;
	int32_t i;

	buffers->frameWidth = width;
	buffers->width = width;
	buffers->height = height;
	chromaWidth = (width + 31) & ~31;
	buffers->chromaWidth = chromaWidth;
	motionWidth = (((width + 64) >> 3) + 63) & ~63;
	buffers->motionWidth = motionWidth;
	lumaSize = width * height;
	chromaSize = (chromaWidth * height) >> 1;
	buffers->chromaSize = chromaSize;
	motionSize = (motionWidth * height) >> 2;
	frameSize = lumaSize + chromaSize + motionSize;
	buffers->lumaSize = lumaSize;
	macroblocks = lumaSize >> 8;
	buffers->motionSize = motionSize;
	buffers->frameSize = frameSize;
	address = stream->work;
	for (i = 0; i < 2; i++) {
		buffers->luma[i] = address;
		buffers->chroma[i] = address + lumaSize;
		buffers->motion[i] = address + lumaSize + chromaSize;
		buffers->luma[i] = vnzPublicOpenRaw(buffers->luma[i], lumaSize);
		buffers->chroma[i] = vnzPublicOpenRaw(buffers->chroma[i], chromaSize);
		buffers->motion[i] = vnzPublicOpenRaw(buffers->motion[i], motionSize);
		address = (address + frameSize + 255) & ~255;
	}
	buffers->reference = address;
	address += motionWidth * 6;
	buffers->rowsAbove = address;
	address += macroblocks * 64;
	buffers->rowsLeft = address;
	address += macroblocks * 64;
	buffers->rowsMotion = address;
	buffers->lines = address + 1024;
}

/* a Main profile level 3.0 stream's slices: a slice of at least 22 macroblocks */
#define PROFILE_MAIN		77
#define LEVEL_30		30
#define MB_PER_SLICE		22

/*
 * 0x836674: the first picture's state from the sequence's settings: its
 * frames, its jobs (a job per four macroblock columns, at least two), and
 * its slices, as even in rows as they divide
 */
static void pictureSetup(AvcencStream *stream, AvcencPicture *picture)
{
	AvcencSequence *settings = stream->settings;
	AvcencBuffers *buffers = stream->buffers;
	uint32_t slicesAddress = stream->slices;
	AvcencSlices *slices;
	int32_t jobs, count, rows, quotient, remainder, i, first;

	picture->jobsDone = 0;
	buffers->idrCountdown = 1;
	picture->widthInMbs = (int32_t)buffers->width >> 4;
	picture->heightInMbs = (int32_t)buffers->height >> 4;
	picture->widthInGroups = picture->widthInMbs >> 2;
	picture->heightInGroups = picture->heightInMbs >> 2;
	picture->motion = buffers->motion[stream->reconstructed];
	picture->motionWidth = buffers->motionWidth;
	picture->reference = buffers->reference;
	picture->luma = buffers->luma[stream->source];
	picture->chroma = buffers->chroma[stream->source];
	picture->motion = vnzPublicCloseRaw(picture->motion, buffers->motionSize);
	picture->lines = buffers->lines;
	picture->rowsAbove = buffers->rowsAbove;
	picture->rowsLeft = buffers->rowsLeft;
	picture->rowsMotion = buffers->rowsMotion;
	picture->maxBitRate = settings->maxBitRate;
	picture->bitRate = settings->bitRate;
	picture->numUnitsInTick = settings->numUnitsInTick;
	picture->timeScale = settings->timeScale;
	picture->maxCpbSize = settings->maxCpbSize;
	picture->cpbSize = settings->cpbSize;
	picture->level = settings->level;
	picture->option = settings->option;
	picture->option2 = settings->option2;
	jobs = picture->widthInMbs;
	if (jobs == 4 || (jobs + 3) >> 2 >= 2)
		jobs = (jobs + 3) >> 2;
	else
		jobs = 2;
	picture->jobs = jobs;
	picture->jobGroups = ((picture->widthInMbs + 7) >> 3) + 1;
	picture->jobArgument = picture->jobs * 8 + 4;

	slices = (AvcencSlices *)vnzPublicOpenPrivate(slicesAddress, SLICES_SIZE);
	slices->streamInfo = stream->streamInfo;
	rows = picture->heightInMbs;
	if (settings->profile == PROFILE_MAIN && settings->level == LEVEL_30) {
		count = picture->widthInMbs * rows / MB_PER_SLICE;
		if (settings->numOfSlices < count)
			count = settings->numOfSlices;
	} else {
		count = settings->numOfSlices;
	}
	quotient = rows / count;
	remainder = rows % count;
	slices->count = count;
	for (i = 0; i < count; i++) {
		if (i < remainder)
			first = (quotient + 1) * i;
		else
			first = remainder + quotient * i;
		slices->firstRow[i] = first;
	}
	vnzPublicClosePrivate((uint32_t)slices, SLICES_SIZE);
}

/*
 * 0x83680e: the client's frame taken for the next picture, with what its
 * settings say of it; a picture that is not predicted from the last one
 * keeps its motion plane, and the next takes the other
 */
static void pictureFrame(AvcencStream *stream, AvcencPicture *picture)
{
	AvcencBuffers *buffers = stream->buffers;
	AvcencSequence *settings = stream->settings;
	uint32_t motionSize, next;
	uint8_t reconstructed;

	picture->nextLuma = buffers->clientLuma;
	picture->nextChroma = buffers->clientChroma;
	picture->nextWidth = buffers->clientWidth;
	picture->nextHeight = buffers->clientHeight;
	picture->nextPitch = buffers->clientPitch;
	picture->nextChromaPitch = buffers->clientChromaPitch;
	picture->pictureBitRate = settings->pictureBitRate;
	picture->pictureMaxBitRate = settings->pictureMaxBitRate;
	picture->pictureInterval = settings->pictureInterval;
	if (settings->numUnitsInTick && settings->timeScale) {
		picture->numUnitsInTick = settings->numUnitsInTick;
		picture->timeScale = settings->timeScale;
	}
	picture->idrInterval = picture->option2 ? 128 : settings->pictureIdrInterval;
	if (buffers->frames == 0) {
		picture->intra = 0;
		picture->predicted = 0;
		picture->idr = 0;
	} else {
		picture->intra = settings->intra;
		picture->predicted = settings->predicted;
		picture->idr = settings->idr;
	}
	picture->constantQuantizer = settings->constantQuantizer;
	picture->quantizer = settings->quantizer;
	picture->quantizer2 = settings->quantizer2;
	if (!picture->predicted) {
		motionSize = buffers->motionSize;
		reconstructed = stream->reconstructed;
		picture->previousMotion = picture->motion;
		picture->previousMotion = vnzPublicOpenRaw(picture->previousMotion, motionSize);
		buffers->motion[reconstructed] = picture->previousMotion;
		next = 1 - reconstructed;
		picture->motion = buffers->motion[next];
		picture->motion = vnzPublicCloseRaw(picture->motion, motionSize);
		buffers->motion[next] = 0;
		stream->reconstructed = next;
	}
}

/* the picture's kind, to the encoder engine */
#define ENGINE_FLAG_IDR		0x00080000
#define ENGINE_FLAG_KIND	0x00100000
#define ENGINE_FLAG_SKIP	0x00200000

/*
 * 0x83608e: the next of the client's outputs; one that has a picture gets
 * the picture's record (its source frame and crop), and the client's frame
 * is given back
 */
static AvcencOutput *outputNext(AvcencStream *stream)
{
	AvcencPicture *picture = stream->picture;
	AvcencBuffers *buffers;
	AvcencOutput *output;
	AvcencRecord *record;
	uint32_t holder;
	int8_t index;

	output = (AvcencOutput *)vnzPublicCloseRaw(
		*(uint32_t *)(stream->outputs + (stream->output % 32) * 64), RECORD_SIZE);
	if (output->hasPicture) {
		buffers = stream->buffers;
		index = stream->recordIndex;
		holder = *(uint32_t *)(output->holder + 4);
		record = (AvcencRecord *)vnzPublicOpenPrivate(picture->records + index * RECORD_SIZE,
			RECORD_SIZE);
		record->width = picture->widthInMbs << 4;
		record->height = picture->heightInMbs << 4;
		record->lumaPitch = picture->width;
		record->chromaPitch = picture->chromaWidth;
		record->luma = picture->luma;
		record->reserved2 = 0;
		record->chroma = picture->chroma;
		record->cropped = picture->cropped;
		record->cropTop = picture->cropTop;
		record->reserved4 = 0;
		record->reserved5 = 0;
		record->cropLeft = picture->cropLeft;
		*(uint32_t *)(holder + 44) = vnzPublicClosePrivate((uint32_t)record, RECORD_SIZE);
		stream->recordIndex = (index + 1) & 15;
		vnzPublicCloseRaw(picture->clientLuma, picture->clientHeight * picture->clientPitch);
		vnzPublicCloseRaw(picture->clientChroma,
			(picture->clientHeight >> 1) * picture->clientChromaPitch);
		stream->record = vnzPublicClosePrivate(stream->record, RECORD_SIZE);
		buffers->released++;
	}
	stream->output = output->next;
	return output;
}

/* 0x8361b2: the client's frame given back */
static void clientRelease(AvcencStream *stream)
{
	AvcencPicture *picture = stream->picture;
	AvcencBuffers *buffers = stream->buffers;

	vnzPublicCloseRaw(picture->clientLuma, picture->clientHeight * picture->clientPitch);
	vnzPublicCloseRaw(picture->clientChroma,
		(picture->clientHeight >> 1) * picture->clientChromaPitch);
	buffers->released++;
}

/*
 * 0x8361f4: the picture's kind told to the engine: skipped, an IDR (the
 * rate control's or the slices' reset), or not
 */
static void pictureKind(AvcencStream *stream)
{
	AvcencSlices *slices = (AvcencSlices *)stream->slices;
	AvcencPicture *picture = stream->picture;
	AvcencBuffers *buffers = stream->buffers;
	AvcencRate *rate = (AvcencRate *)stream->rate;

	if (picture->predicted)
		vnzThreadSetFlags(picture->engine, ENGINE_FLAG_SKIP);
	else if (rate->forceIdr || slices->idr)
		vnzThreadSetFlags(picture->engine, ENGINE_FLAG_IDR);
	else
		vnzThreadSetFlags(picture->engine, ENGINE_FLAG_KIND);
	if (!buffers->idrNext && rate->forceIdr) {
		buffers->idrNext = 1;
		buffers->idrCountdown = picture->idrInterval;
	}
	rate->idrTaken = buffers->idrNext;
	rate->idrCounted = buffers->idrPending;
}

/* 0x836278: the next frame becomes the picture's, with the rate control's decisions */
static void pictureNext(AvcencStream *stream)
{
	AvcencRate *rate = (AvcencRate *)stream->rate;
	AvcencPicture *picture;

	picture = (AvcencPicture *)vnzPublicOpenPrivate(
		vnzPublicCloseRaw((uint32_t)stream->picture, PICTURE_SIZE), PICTURE_SIZE);
	picture->clientLuma = picture->nextLuma;
	picture->clientChroma = picture->nextChroma;
	picture->clientWidth = picture->nextWidth;
	picture->clientChromaPitch = picture->nextChromaPitch;
	picture->clientPitch = picture->nextPitch;
	picture->clientHeight = picture->nextHeight;
	picture->settings = stream->settings;
	picture->rateIdr = rate->idr;
	picture->rateIntra = rate->intra;
	picture->qpLimit = rate->qpLimit;
	stream->picture = (AvcencPicture *)vnzPublicOpenRaw(
		vnzPublicClosePrivate((uint32_t)picture, PICTURE_SIZE), PICTURE_SIZE);
}

/*
 * 0x836308: whether the picture is an IDR: the rate control asks for one,
 * or a scene change (or a periodic intra) does while the stream allows it;
 * an IDR starts the countdown to the next
 */
static int32_t pictureIdr(AvcencStream *stream)
{
	AvcencRate *rate = (AvcencRate *)stream->rate;
	AvcencPicture *picture = stream->picture;
	AvcencBuffers *buffers = stream->buffers;

	if (rate->intraRequested)
		goto idr;
	if ((picture->option || rate->sceneIdr) && (!buffers->idrNext || rate->idrTaken) &&
	    (rate->sceneChange || rate->idrRequested || rate->intraRequested) &&
	    picture->option2 != 1)
		goto idr;
	if (picture->option2 && rate->idrCounted)
		buffers->idrCountdown = picture->idrInterval - 1;
	return 0;
idr:
	buffers->idrNext = 1;
	buffers->idrCountdown = picture->idrInterval;
	return 1;
}

/* 0x836376: the picture's source frame kept, the next picture takes the other */
static void sourceSwap(AvcencStream *stream)
{
	AvcencBuffers *buffers = stream->buffers;
	AvcencPicture *picture;
	uint8_t source = stream->source;

	picture = (AvcencPicture *)vnzPublicOpenPrivate(
		vnzPublicCloseRaw((uint32_t)stream->picture, PICTURE_SIZE), PICTURE_SIZE);
	picture->luma = vnzPublicOpenRaw(picture->luma, buffers->lumaSize);
	buffers->luma[source] = picture->luma;
	picture->chroma = vnzPublicOpenRaw(picture->chroma, buffers->chromaSize);
	buffers->chroma[source] = picture->chroma;
	stream->source = 1 - source;
	picture->macroblocks = vnzPublicCloseRaw(picture->macroblocks,
		((buffers->width * buffers->height) >> 8) << 6);
	stream->picture = (AvcencPicture *)vnzPublicOpenRaw(
		vnzPublicClosePrivate((uint32_t)picture, PICTURE_SIZE), PICTURE_SIZE);
}

/*
 * The quantizer's thresholds (0x91b478, 0x91b518): eight per QP % 6 (and,
 * for luma, per QP - 42 from QP 42), by the position's class in a 4x4 block
 */
const int16_t avcencLumaThreshold[10][8] = {			/* 0x91b478 */
	{ 267, 434, 267, 434, 434, 667, 434, 667 },
	{ 294, 467, 294, 467, 467, 750, 467, 750 },
	{ 347, 534, 347, 534, 534, 834, 534, 834 },
	{ 374, 600, 374, 600, 600, 959, 600, 959 },
	{ 427, 667, 427, 667, 667, 1042, 667, 1042 },
	{ 480, 767, 480, 767, 767, 1208, 767, 1208 },
	{ 534, 867, 534, 867, 867, 1333, 867, 1333 },
	{ 587, 933, 587, 933, 933, 1500, 933, 1500 },
	{ 694, 1067, 694, 1067, 1067, 1667, 1067, 1667 },
	{ 747, 1200, 747, 1200, 1200, 1917, 1200, 1917 },
};

const int16_t avcencChromaThreshold[10][8] = {			/* 0x91b518 */
	{ 587, 954, 587, 954, 954, 1467, 954, 1467 },
	{ 646, 1027, 646, 1027, 1027, 1650, 1027, 1650 },
	{ 763, 1174, 763, 1174, 1174, 1834, 1174, 1834 },
	{ 822, 1320, 822, 1320, 1320, 2109, 1320, 2109 },
	{ 939, 1467, 939, 1467, 1467, 2292, 1467, 2292 },
	{ 1056, 1687, 1056, 1687, 1687, 2658, 1687, 2658 },
	{ 1174, 1907, 1174, 1907, 1907, 2933, 1907, 2933 },
	{ 1291, 2053, 1291, 2053, 2053, 3300, 2053, 3300 },
	{ 1526, 2347, 1526, 2347, 2347, 3667, 2347, 3667 },
	{ 1643, 2640, 1643, 2640, 2640, 4217, 2640, 4217 },
};

/* the preparing jobs, on the other cores (avcencjob.c) */
void avcencJobScene(uint32_t picture, uint32_t slices, uint32_t rate, uint32_t unused);
void avcencJobLoad(uint32_t picture, uint32_t slices, uint32_t unused, uint32_t unused2);

#define JOB_SCENE(index)	((index) * 8 + 24)
#define JOB_LOAD(index)		((index) * 8 + 8)
#define JOB_FLAG_SCENE		0x01000000
#define JOB_FLAG_LOAD		0x08000000

/* a job set on the encoders' context, the context let go and taken again while it is busy */
static VkVThreadContext *jobSet(VkVThreadContext *context, const AvcencPicture *picture,
	uint32_t index, uint32_t count, VkJobFunction function, uint32_t argument,
	uint32_t argument2, uint32_t argument3, uint32_t flags)
{
	while (vkVThreadJobSetNotify(context, index, count, function, (uint32_t)picture, argument,
	    argument2, argument3, picture->thread, flags)) {
		vkVThreadContextUnlock(context);
		context = vkVThreadContextLock(picture->context);
	}
	return context;
}

/*
 * 0x836986: the first picture's jobs: whether it is an IDR (the settings' or
 * the countdown's, or a scene change the stream allows), its quantizer's
 * shifts and thresholds from the rate control's QP, then the scene job and
 * (unless the picture is skipped) the load job
 */
static void picturesStart(AvcencStream *stream, AvcencPicture *picture)
{
	AvcencRate *rate = (AvcencRate *)stream->rate;
	AvcencBuffers *buffers = stream->buffers;
	uint8_t *frameRecord = (uint8_t *)picture->frameRecord;
	int32_t index = picture->index, idr, last, countdown, qp, chroma, quotient, i;
	uint32_t previousIdr, request, scene, load;
	VkVThreadContext *context;
	AvcencSlices *slices;

	countdown = buffers->idrCountdown;
	if (picture->predicted) {
		countdown = countdown == 1 ? 1 : countdown - 1;
		idr = 0;
		last = 0;
	} else {
		last = (countdown ^ 1) < 1;
		request = (rate->idrRequested || rate->sceneChange) ? 1 : 0;
		if (picture->option2) {
			idr = (request & ((0 < (uint8_t)picture->option) & buffers->flushing)) |
				((uint8_t)picture->intra | ((buffers->released < 1) & last));
			if (idr | last)
				countdown = picture->idrInterval;
			else
				countdown--;
		} else {
			idr = (request & ((0 < (uint8_t)picture->option) & buffers->flushing)) |
				((uint8_t)picture->intra | last);
			if (idr)
				countdown = picture->idrInterval;
			else
				countdown--;
			last = 0;
		}
	}
	if (picture->idr || last) {
		buffers->idrPending = 1;
		countdown = picture->idrInterval;
	} else {
		buffers->idrPending = 0;
	}
	buffers->idrNext = idr;
	buffers->idrCountdown = countdown;
	buffers->frameNum = 0;
	frameRecord[24] = -1;
	for (i = 0; i < 4; i++) {
		buffers->refFrames[i] = i - 4;
		frameRecord[20 + i] = i - 4;
	}

	previousIdr = 0;
	slices = (AvcencSlices *)stream->slices;
	scene = JOB_SCENE(index);
	load = JOB_LOAD(index);
	if (!idr)
		previousIdr = (uint8_t)slices->idr;
	slices->idr = idr;
	slices->sliceType = (0 < (uint32_t)idr) << 1;
	if (!idr) {
		qp = (uint8_t)picture->rateIdr - (previousIdr ? 2 : 1);
		if ((int32_t)rate->vclInitialDelay >= 1)
			qp = (uint8_t)picture->rateIdr;
		chroma = avcChromaQp[qp];
		if (qp < 42) {
			for (quotient = 0, i = qp; i >= 6; i -= 6)
				quotient++;
			slices->lumaThreshold = avcencLumaThreshold[i];
			slices->lumaShift = 7 - quotient;
			for (quotient = 0, i = qp; i >= 6; i -= 6)
				quotient++;
			slices->lumaThreshold2 = avcencLumaThreshold[i];
			slices->lumaShift2 = 7 - quotient;
			slices->scaled = 1;
		} else {
			slices->lumaShift2 = 0;
			slices->lumaShift = 0;
			slices->lumaThreshold2 = avcencLumaThreshold[qp - 42];
			slices->lumaThreshold = avcencLumaThreshold[qp - 42];
			slices->scaled = 0;
		}
		for (quotient = 0; chroma >= 6; chroma -= 6)
			quotient++;
		slices->chromaThreshold = avcencChromaThreshold[chroma];
		slices->chromaShift = 7 - quotient;
	}

	context = vkVThreadContextLock(picture->context);
	if (idr) {
		context = jobSet(context, picture, scene, 1, avcencJobScene, (uint32_t)slices,
			(uint32_t)rate, 0, JOB_FLAG_SCENE);
		context = jobSet(context, picture, load, 0, avcencJobLoad, (uint32_t)slices, 0, 0,
			JOB_FLAG_LOAD);
	} else if (picture->predicted) {
		context = jobSet(context, picture, scene, 0, avcencJobScene, (uint32_t)slices,
			(uint32_t)rate, 0, JOB_FLAG_SCENE);
	} else {
		context = jobSet(context, picture, scene, picture->heightInGroups + 1, avcencJobScene,
			(uint32_t)slices, (uint32_t)rate, 0, JOB_FLAG_SCENE);
		context = jobSet(context, picture, load, 0, avcencJobLoad, (uint32_t)slices, 0, 0,
			JOB_FLAG_LOAD);
	}
	vkVThreadContextUnlock(context);
}
