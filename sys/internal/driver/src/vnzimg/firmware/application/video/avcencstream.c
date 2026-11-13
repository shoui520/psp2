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
	uint16_t frameWidth;
	uint16_t frameHeight;
	uint16_t framePitch;
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
#define MAX_SLICES		24
typedef struct AvcencSlices {
	uint8_t reserved[7];
	uint8_t count;
	uint8_t firstRow[MAX_SLICES];
	uint32_t streamInfo;
	int8_t reset;
	uint8_t reserved2[27];
} AvcencSlices;

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
	uint8_t reserved2[21];
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
