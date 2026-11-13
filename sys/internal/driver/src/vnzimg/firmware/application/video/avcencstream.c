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
#define OUTPUTS_SIZE		2048	/* the client's 32 outputs */
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
#define STREAM_FLAG_CHUNKS_SET	0x00004000	/* the picture's chunk jobs are set */
#define STREAM_FLAG_OUTPUT_DONE	0x00020000	/* the output's last row is encoded */
#define STREAM_FLAG_PICTURE_DONE 0x00040000	/* the picture's */
#define STREAM_FLAG_CHUNK	0x00100000	/* a chunk's last stage is done */
#define STREAM_FLAG_OUTPUT	0x00200000	/* an output's rate job is done: its rows start */
#define STREAM_FLAG_SCENE	0x01000000	/* the jobs' flags as they finish */
#define STREAM_FLAG_RATE	0x02000000
#define STREAM_FLAG_UPDATED	0x04000000
#define STREAM_FLAG_LOADED	0x08000000
#define STREAM_FLAG_ENCODED	0x10000000	/* the encoder engine's */
#define STREAM_FLAG_ANSWERED	0x20000000
#define STREAM_FLAGS		(0x0b360000 | STREAM_FLAG_EXIT | STREAM_FLAG_AU | STREAM_FLAG_LOOP)

/* the picture's state, as the preparing jobs get it (256 bytes) */
typedef struct AvcencPicture {
	VkThread *thread;		/* the stream thread's */
	VkThread *engine;		/* the encoder engine's */
	int32_t id;			/* the codec's */
	VkVThreadContext *outputContext; /* the client's, for the output's jobs */
	VkVThreadContext *context;
	uint16_t chunkSteps;		/* four per chunk of the rows, one more for the row's end */
	int8_t index;			/* the slot's */
	uint8_t jobsDone;
	uint8_t chunkHeight;		/* in lines */
	uint8_t chunkWidth;		/* in macroblocks, 6 or 8 */
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
	int8_t rateQp;			/* the rate control's */
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
	int8_t qp;			/* the picture's, to its info */
	int8_t intra;
	uint8_t reserved4[116];
	int8_t forceIdr;
	uint8_t reserved5[5];
	uint32_t initialCpbRemovalDelay;
	uint32_t vclInitialCpbRemovalDelay;
	uint32_t initialCpbRemovalDelayOffset;
	uint32_t vclInitialCpbRemovalDelayOffset;
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
	uint16_t reserved;
	uint16_t mbs;			/* the macroblocks it holds */
	uint16_t reserved0;
	uint16_t firstMb;		/* 0 for the picture's first output */
	int8_t flags[2];		/* to the job record */
	uint8_t reserved1;
	int8_t next;			/* the row after its last (the output after it) */
	uint8_t withInfo;		/* the picture's info comes with it */
	uint8_t reserved2;
	int8_t hasPicture;
	uint8_t reserved3;
	uint32_t info;			/* the picture's VideoAvcPictureInfo */
	uint8_t reserved4[108];
} AvcencOutput;

/* a row's slot in the client's outputs, 32 in turn */
typedef struct AvcencOutputSlot {
	uint32_t output;		/* the AvcencOutput the row goes to */
	uint8_t firstMb;		/* of the chunk being encoded */
	uint8_t row;
	uint8_t mbs;			/* the chunk's */
	uint8_t reserved[9];
	uint32_t sliceType;
	uint32_t reserved2;
	uint32_t chunk;
	uint32_t outputInfo;
	uint8_t reserved3[5];
	uint8_t parity;			/* flips with each chunk */
	uint8_t reserved4[18];
	uint8_t rowFlags[8];		/* the flags of eight rows, by the row's high bits */
} AvcencOutputSlot;

typedef char outputSlotSizeCheck[sizeof(AvcencOutputSlot) == 64 ? 1 : -1];

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
	int8_t frameNum;
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
	uint8_t chunksDone;
	uint8_t chunksSent;
	int8_t chunks;			/* of a row, one more for the row's end */
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
	uint8_t chunkPending;		/* the last row whose chunks are set */
	uint8_t chunkLast;		/* the output's last row */
	uint8_t reserved4[2];
	uint32_t work;			/* the slot's frame memory */
	AvcencSequence *settings;	/* the sequence's, from the client */
	uint32_t chunkBits;
	uint8_t reserved5[16];
} AvcencStream;

/* the layouts' sizes */
typedef char pictureSizeCheck[sizeof(AvcencPicture) == PICTURE_SIZE ? 1 : -1];
/* the encoder's memory, as the client lays it out (64 bytes) */
typedef struct AvcencClientMemory {
	VkVThreadContext *outputContext;
	uint32_t record;		/* the rows' chunk counts, 128 bytes */
	uint32_t reserved;
	uint32_t outputs;
	uint32_t jobInfo;
	uint32_t outputInfo;
	uint32_t command0;		/* the job record's, in this order */
	uint32_t command3;
	uint32_t command1;
	uint32_t command2;
	uint32_t command4;
	uint32_t command5;
	uint32_t reserved2;
	uint32_t commandBuffer;		/* 2048 bytes */
	uint32_t commandBuffer2;
	uint32_t reserved3;
} AvcencClientMemory;

typedef char clientMemorySizeCheck[sizeof(AvcencClientMemory) == 64 ? 1 : -1];

/* the picture's job record, for the output's jobs */
typedef struct AvcencJobInfo {
	uint32_t output;		/* the AvcencOutput it fills */
	int8_t outputFlags[2];		/* the output's */
	uint8_t mbs;
	uint8_t reserved;
	uint32_t reserved2;
	uint32_t sliceType;
	int8_t skipLevel;
	uint8_t reserved3;
	int8_t chunks;
	int8_t sliceMode;
	uint32_t reserved4;
	uint32_t outputs;
	uint32_t outputInfo;
	uint8_t reserved5[32];
} AvcencJobInfo;

typedef char jobInfoSizeCheck[sizeof(AvcencJobInfo) == 64 ? 1 : -1];
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
#define ENGINE_FLAG_HELPER	0x00800000	/* the helper may take it */
#define ENGINE_FLAG_ENDED	0x00000020	/* the stream ended: encodes are answered empty */

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
	uint32_t reference;
	int8_t index;

	output = (AvcencOutput *)vnzPublicCloseRaw(
		*(uint32_t *)(stream->outputs + (stream->output % 32) * 64), RECORD_SIZE);
	if (output->hasPicture) {
		buffers = stream->buffers;
		index = stream->recordIndex;
		reference = ((VideoAvcPictureInfo *)output->info)->reference;
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
		*(uint32_t *)(reference + 44) = vnzPublicClosePrivate((uint32_t)record,
			RECORD_SIZE);
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
	picture->rateQp = rate->qp;
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
void avcencJobLoad(uint32_t picture, uint32_t slices, uint32_t position, uint32_t unused);

void avcencJobMotion(uint32_t picture, uint32_t slices, uint32_t position, uint32_t unused);
void avcencJobIntra(uint32_t picture, uint32_t slices, uint32_t position, uint32_t unused);
void avcencJobDecide(uint32_t picture, uint32_t slices, uint32_t position, uint32_t unused);
void avcencJobRate(uint32_t picture, uint32_t rate, uint32_t unused, uint32_t unused2);
void avcencJobRateUpdate(uint32_t picture, uint32_t rate, uint32_t unused, uint32_t unused2);
/* a chunk's three stages, by the output's jobs (avcencchunk.c) */
void avcencJobChunkStage1(uint32_t picture, uint32_t outputs, uint32_t unused,
	uint32_t unused2);
void avcencJobChunkStage2(uint32_t picture, uint32_t outputs, uint32_t unused,
	uint32_t unused2);
void avcencJobChunkStage3(uint32_t picture, uint32_t outputs, uint32_t unused,
	uint32_t unused2);
void avcencJobLoadReference(uint32_t picture, uint32_t slices, uint32_t position,
	uint32_t unused);

#define JOB_SCENE(index)	((index) * 8 + 24)
#define JOB_LOAD(index)		((index) * 8 + 8)
#define JOB_FLAG_SCENE		0x01000000
#define JOB_FLAG_LOAD		0x08000000
#define JOB_FLAG_ROWS		0x08000000
#define JOB_FLAG_RATE		0x02000000
#define JOB_FLAG_UPDATE		0x04000000
#define JOB_FLAG_CHUNK		0x00100000

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

/* a job set with the context taken and let go for each attempt, until it is not busy */
static void jobPost(const AvcencPicture *picture, uint32_t index, uint32_t count,
	VkJobFunction function, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t flags)
{
	VkVThreadContext *context;
	int32_t busy;

	do {
		context = vkVThreadContextLock(picture->context);
		busy = vkVThreadJobSetNotify(context, index, count, function, (uint32_t)picture,
			argument, argument2, argument3, picture->thread, flags);
		vkVThreadContextUnlock(context);
	} while (busy);
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
		qp = (uint8_t)picture->rateQp - (previousIdr ? 2 : 1);
		if ((int32_t)rate->initialCpbRemovalDelay >= 1)
			qp = (uint8_t)picture->rateQp;
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

/*
 * 0x836924: the jobs a group's job waits for in the wavefront: the one to
 * its left, the one above, and the one above to the right (from three
 * groups across, by the rows' parity)
 */
static int32_t jobWaits(int32_t column, int32_t row, int32_t groups, int32_t columns)
{
	int32_t odd = row & 1, waits;

	if (groups < 3) {
		waits = odd ? 0 : groups;
		if (row >= 1)
			waits++;
		return waits;
	}
	if (odd) {
		waits = 0;
	} else if (column / 2 == groups - 1 && (groups & 1) != ((row / 2) & 1)) {
		waits = 1;
	} else if (column < 1 && ((row / 2) & 1)) {
		waits = 1;
	} else {
		waits = 2;
	}
	if (column >= 1)
		waits++;
	if (row >= 1 && columns - 1 != column)
		waits++;
	return waits;
}

/*
 * 0x836c90: a picture's jobs. A skipped picture has only the rate control's;
 * the others have the motion jobs, a wavefront over pairs of macroblocks in
 * steps of four (each step's count kept in the frame record), then the
 * intra and decision jobs over groups of four, and the rate control's last
 */
static void picturesRun(AvcencStream *stream, AvcencPicture *picture, uint32_t rateJobs)
{
	uint32_t slices = stream->slices, rate = stream->rate;
	uint8_t *frameRecord = (uint8_t *)picture->frameRecord;
	int32_t index = picture->index, widthInMbs = picture->widthInMbs;
	int32_t heightInMbs = picture->heightInMbs, columns, rows, groups, groupRows;
	int32_t step, steps, row, column, waits, jobs, position, started;
	uint32_t idrNext = stream->buffers->idrNext;
	VkVThreadContext *context;
	VkThread *thread;

	((AvcencSlices *)slices)->idr = idrNext;
	((AvcencSlices *)slices)->sliceType = (0 < idrNext) << 1;
	thread = picture->thread;
	if (picture->predicted) {
		context = vkVThreadContextLock(picture->context);
		context = jobSet(context, picture, index * 8 + picture->jobs * 8 + 9,
			0 < rateJobs, avcencJobRate, rate, 0, 0, JOB_FLAG_RATE);
		vkVThreadContextUnlock(context);
		return;
	}
	for (step = 0; step < 8; step++)
		frameRecord[28 + step] = 0;
	groups = (widthInMbs + 3) >> 2;
	columns = (widthInMbs + 1) >> 1;
	groupRows = (heightInMbs + 3) >> 2;
	rows = (heightInMbs + 1) >> 1;
	context = vkVThreadContextLock(picture->context);

	/* the motion jobs */
	step = 0;
	steps = rows * 5 + columns - 5;
	if (steps > 20)
		steps = 20;
	if (steps >= 1) {
		do {
			row = step < columns ? 0 : (step - columns + 5) / 5;
			started = 0;
			for (; row < rows; row++) {
				column = step - row * 5;
				if (column < -3)
					break;
				if (columns < 5 && column >= 1)
					continue;
				if (column >= columns)
					continue;
				waits = jobWaits(column, row, groups, columns);
				if (!waits)
					continue;
				started++;
				if (column < 0)
					column = 0;
				position = column << 1 | row << 9;
				context = jobSet(context, picture, index * 8 + picture->jobArgument +
					picture->jobGroups * (row & 7) + (((row & 3) + column) >> 2) + 8,
					waits, avcencJobMotion, slices, position, 0, JOB_FLAG_ROWS);
			}
			frameRecord[28 + ((step >> 2) & 7)] = started;
			step += 4;
		} while (step < steps);
	}
	*(uint16_t *)(frameRecord + 26) = step;

	/* the intra and decision jobs */
	if (groups < 3) {
		steps = 4;
	} else {
		steps = groupRows * 5 + groups - 5;
		if (steps > 8)
			steps = 8;
	}
	for (step = 0; step < steps; step++) {
		row = step < groups ? 0 : (step - groups + 5) / 5;
		for (; row < groupRows; row++) {
			column = step - row * 5;
			if (column < 0)
				break;
			if (column >= groups)
				continue;
			jobs = picture->jobs;
			position = row << 10 | column << 2;
			if (!idrNext) {
				context = jobSet(context, picture, index * 8 + jobs * 4 +
					(row & 3) * jobs + column + 8, 1, avcencJobIntra, slices,
					position, 0, JOB_FLAG_ROWS);
				jobs = picture->jobs;
			}
			context = jobSet(context, picture, index * 8 + jobs * (row & 3) + column + 8,
				0, avcencJobDecide, slices, position, 0, JOB_FLAG_ROWS);
		}
	}
	frameRecord[25] = step;
	context = jobSet(context, picture, index * 8 + picture->jobs * 8 + 9,
		(0 < rateJobs) + 1, avcencJobRate, rate, 0, 0, JOB_FLAG_RATE);
	vkVThreadContextUnlock(context);
	(void)thread;
}

/* 0x836ffe: the rate control's update after the picture, its last job */
static void pictureRateUpdate(AvcencStream *stream, AvcencPicture *picture)
{
	jobPost(picture, picture->index * 8 + picture->jobs * 8 + 10, 0, avcencJobRateUpdate,
		stream->rate, 0, 0, JOB_FLAG_UPDATE);
}

/*
 * 0x83706c: the loads that follow the picture's progress: the next source
 * rows once the last load's step is reached, and (in a picture that is not
 * an IDR) each reference frame's next rows once the step passed them
 */
static void pictureLoads(AvcencStream *stream, AvcencPicture *picture)
{
	uint32_t slices = stream->slices;
	AvcencBuffers *buffers = stream->buffers;
	int8_t *frameRecord = (int8_t *)picture->frameRecord;
	int32_t index = picture->index, step = frameRecord[24], reference, i;

	if (buffers->frameNum == step) {
		if (step + 1 < (picture->heightInMbs + 3) >> 2)
			jobPost(picture, index * 8 + ((step + 1) & 7) + 8, 0, avcencJobLoad,
				slices, step * 4 + 4, 0, JOB_FLAG_LOAD);
		buffers->frameNum++;
	}
	if (((AvcencSlices *)slices)->idr)
		return;
	for (i = 0; i < 4; i++) {
		reference = frameRecord[20 + i];
		if (step < reference + 4 || buffers->refFrames[i] != reference)
			continue;
		if (reference + 4 < picture->heightInGroups)
			jobPost(picture, index * 8 + ((reference + 4) & 7) + 16, 0,
				avcencJobLoadReference, slices, reference * 4 + 16, 0, JOB_FLAG_LOAD);
		buffers->refFrames[i] += 4;
	}
}

/*
 * 0x8371b0: the picture's next jobs, once the motion jobs of the step four
 * back are done: the next motion steps (one, or the rest of the picture's
 * steps in a picture under three groups wide), then the intra and decision
 * jobs of the groups the new steps open
 */
static void picturesStep(AvcencStream *stream, AvcencPicture *picture)
{
	int8_t *frameRecord = (int8_t *)picture->frameRecord;
	int32_t index = picture->index, widthInMbs = picture->widthInMbs;
	int32_t heightInMbs = picture->heightInMbs, columns, rows, groups, groupRows;
	int32_t step = *(int16_t *)(frameRecord + 26), end, row, column, waits, jobs;
	int32_t position, started;
	uint32_t slices, idr;
	VkVThreadContext *context;

	if (frameRecord[28 + (((step - 20) >> 2) & 7)])
		return;
	slices = stream->slices;
	idr = (uint8_t)((AvcencSlices *)slices)->idr;
	columns = (widthInMbs + 1) >> 1;
	groups = (widthInMbs + 3) >> 2;
	groupRows = (heightInMbs + 3) >> 2;
	rows = (heightInMbs + 1) >> 1;
	context = vkVThreadContextLock(picture->context);

	/* the motion jobs */
	if (groups >= 3)
		end = step + 4;
	else
		end = step % 5 == 1 ? step + 12 : step + 8;
	while (step < end) {
		row = step < columns ? 0 : (step - columns + 5) / 5;
		started = 0;
		for (; row < rows; row++) {
			column = step - row * 5;
			if (column < -3)
				break;
			if (columns < 5 && column >= 1)
				continue;
			if (column >= columns)
				continue;
			waits = jobWaits(column, row, groups, columns);
			if (!waits)
				continue;
			started++;
			if (column < 0)
				column = 0;
			position = column << 1 | row << 9;
			context = jobSet(context, picture, index * 8 + picture->jobArgument +
				picture->jobGroups * (row & 7) + (((row & 3) + column) >> 2) + 8,
				waits, avcencJobMotion, slices, position, 0, JOB_FLAG_ROWS);
		}
		frameRecord[28 + ((step >> 2) & 7)] = started;
		step += 4;
	}
	*(int16_t *)(frameRecord + 26) = step;

	/* the intra and decision jobs */
	step = frameRecord[25];
	if (groups >= 3)
		end = step + 2;
	else
		end = step % 5 == 4 ? step + 6 : step + 4;
	for (; step < end; step++) {
		row = step < groups ? 0 : (step - groups + 5) / 5;
		for (; row < groupRows; row++) {
			column = step - row * 5;
			if (column < 0)
				break;
			if (column >= groups)
				continue;
			jobs = picture->jobs;
			position = row << 10 | column << 2;
			if (!idr) {
				context = jobSet(context, picture, index * 8 + jobs * 4 +
					(row & 3) * jobs + column + 8, 1, avcencJobIntra, slices,
					position, 0, JOB_FLAG_ROWS);
				jobs = picture->jobs;
			}
			context = jobSet(context, picture, index * 8 + jobs * (row & 3) + column + 8,
				0, avcencJobDecide, slices, position, 0, JOB_FLAG_ROWS);
		}
	}
	frameRecord[25] = step;
	vkVThreadContextUnlock(context);
}

#define ROW_FIRST	0x20		/* the row's flags in the outputs: its first chunk */
#define ROW_LAST	0x40
#define ROW_SPLIT	0xa0		/* the chunk before the split: its own end and the split's */
#define ROW_SPLIT_END	0x80
#define ROW_CHUNK	0x10
#define ROW_MIDDLE	0x08
#define ROW_WRAP	0x01
#define ROW_END		0x02

/*
 * 0x83746c: a sequence's row layout. A row is cut into chunks of six
 * macroblocks (eight from 40 across), the remainder deciding where the
 * last ones split; each row's flags go to the outputs (eight rows to an
 * output, from byte 56), and the picture's progress is cleared
 */
static void streamRowsLayout(AvcencStream *stream, AvcencPicture *picture)
{
	uint32_t heightInMbs = picture->heightInMbs, widthInMbs = picture->widthInMbs;
	uint32_t width = widthInMbs < 40 ? 6 : 8, remainder, chunks, row, chunk;
	uint8_t flags[8], *end, *split, value;
	AvcencOutputSlot *slots;
	uint32_t *progress;

	remainder = widthInMbs % width;
	picture->chunkWidth = width;
	picture->chunkHeight = 16;
	if (widthInMbs <= width) {
		stream->sliceMode = 0;
		stream->chunks = 1;
		picture->chunkSteps = 4;
	} else {
		chunks = (widthInMbs + width - 1) / width;
		stream->sliceMode = (remainder == 2) + 1;
		stream->chunks = chunks + 1;
		picture->chunkSteps = (chunks + 1) * 4;
	}
	stream->output = 0;
	stream->chunksSent = 0;
	stream->chunkPending = 1;
	slots = (AvcencOutputSlot *)vnzPublicOpenPrivate(stream->outputs, OUTPUTS_SIZE);

	for (chunk = 0; chunk < width - 1; chunk++)
		flags[chunk] = ROW_CHUNK;
	end = flags + width;
	switch (remainder) {
	case 0:
		flags[0] |= ROW_FIRST;
		end[-2] |= ROW_LAST;
		end[-3] &= ~ROW_CHUNK;
		break;
	case 1:
	case 2:
		end[-3] |= ROW_LAST;
		break;
	case 3:
		end[-4] |= ROW_SPLIT;
		end[-2] &= ~ROW_CHUNK;
		break;
	default:
		split = flags + (width - remainder);
		split[0] |= ROW_FIRST;
		split[-1] |= ROW_SPLIT;
		end[-2] &= ~ROW_CHUNK;
		end[-3] &= ~ROW_CHUNK;
		break;
	}

	for (row = 0, chunk = 0; row < heightInMbs; row++) {
		((uint8_t *)stream->record)[row] = 0;
		if (row == heightInMbs - 1)
			value = ROW_END;
		else if (row < heightInMbs - 2)
			value = flags[chunk] | ROW_MIDDLE;
		else
			value = 0;
		if (chunk == width - 2) {
			value |= ROW_WRAP;
			chunk = 0;
		} else {
			chunk++;
		}
		slots[row & 31].rowFlags[row >> 5] = value;
	}
	stream->chunksDone = 0;
	stream->chunkBits = 0;
	vnzPublicClosePrivate((uint32_t)slots, OUTPUTS_SIZE);

	progress = (uint32_t *)picture->frameRecord;
	progress[4] = 0;
	progress[1] = 0;
	progress[3] = 0;
	progress[2] = 0;
}

static void chunkRowsSingle(AvcencPicture *picture, uint32_t jobInfo);

/* a chunk job set on the client's context, let go and taken again while it is busy */
static VkVThreadContext *chunkJobSet(VkVThreadContext *context, AvcencPicture *picture,
	uint32_t index, uint32_t count, VkJobFunction function, uint32_t outputs)
{
	while (vkVThreadJobSetNotify(context, index, count, function, (uint32_t)picture, outputs,
	    0, 0, picture->thread, JOB_FLAG_CHUNK)) {
		vkVThreadContextUnlock(context);
		context = vkVThreadContextLock(picture->outputContext);
	}
	return context;
}

/* a chunk's three stages, each a row of chunk steps after the last */
static void chunkJobsSet(VkVThreadContext *context, AvcencPicture *picture, uint32_t index,
	uint32_t count, uint32_t outputs)
{
	uint32_t steps = picture->chunkSteps;

	context = chunkJobSet(context, picture, index, count, avcencJobChunkStage1, outputs);
	context = chunkJobSet(context, picture, index + steps, count, avcencJobChunkStage2,
		outputs);
	chunkJobSet(context, picture, index + steps * 2, count, avcencJobChunkStage3, outputs);
}

/*
 * 0x83761c: the job that sets a picture's chunk jobs (on the client's
 * context): the first row's chunks, each waiting for one job, then the next
 * rows' chunks, waiting for two (the one to the left and the one above), the
 * row's end waiting for one when its remainder is the slice mode's. A row of
 * one chunk goes the other way.
 */
void avcencJobChunks(uint32_t pictureAddress, uint32_t jobInfo, uint32_t unused,
	uint32_t unused2)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	AvcencJobInfo *info = (AvcencJobInfo *)jobInfo;
	uint32_t width = picture->chunkWidth, widthInMbs = picture->widthInMbs;
	uint32_t sliceMode = (uint8_t)info->sliceMode, outputs, index, x;
	VkVThreadContext *context;

	if (width >= widthInMbs) {
		chunkRowsSingle(picture, jobInfo);
		return;
	}
	context = vkVThreadContextLock(picture->outputContext);
	outputs = info->outputs;
	index = 0;
	for (x = 0; x < widthInMbs; x += width)
		chunkJobsSet(context, picture, index++, 1, outputs);

	outputs += 64;
	index = (uint8_t)info->chunks;
	chunkJobsSet(context, picture, index, 2, outputs);
	for (index++, x = width - 1; x < widthInMbs - width; x += width)
		chunkJobsSet(context, picture, index++, 2, outputs);
	chunkJobsSet(context, picture, index, widthInMbs - x == sliceMode ? 1 : 2, outputs);
	vkVThreadContextUnlock(context);
}

/*
 * 0x838350: the chunk jobs of a picture one chunk wide: the first row's
 * (its last stage waiting for two) and the second row's
 */
static void chunkRowsSingle(AvcencPicture *picture, uint32_t jobInfo)
{
	uint32_t outputs = ((AvcencJobInfo *)jobInfo)->outputs, steps;
	VkVThreadContext *context, *current;

	context = vkVThreadContextLock(picture->outputContext);
	steps = picture->chunkSteps;
	current = chunkJobSet(context, picture, 0, 1, avcencJobChunkStage1, outputs);
	chunkJobSet(current, picture, steps, 1, avcencJobChunkStage2, outputs);
	chunkJobSet(context, picture, picture->chunkSteps * 2, 2, avcencJobChunkStage3, outputs);

	outputs += 64;
	steps = picture->chunkSteps;
	current = chunkJobSet(context, picture, 1, 2, avcencJobChunkStage1, outputs);
	current = chunkJobSet(current, picture, steps + 1, 2, avcencJobChunkStage2, outputs);
	chunkJobSet(current, picture, steps * 2 + 1, 2, avcencJobChunkStage3, outputs);
	vkVThreadContextUnlock(context);
}

/*
 * 0x8384d6: the slots of the output's rows not yet started: each row's
 * output, chunk and kind; rows up to the last set have their first jobs
 * signalled
 */
static void streamChunksStart(AvcencStream *stream)
{
	AvcencPicture *picture = stream->picture;
	uint32_t outputs = stream->outputs, row = stream->chunksDone, end, phase, request;
	uint32_t widthInMbs = picture->widthInMbs, second;
	AvcencOutputSlot *slot;
	VkVThreadContext *context;

	request = vnzPublicOpenRaw(((AvcencJobInfo *)stream->jobInfo)->output, RECORD_SIZE);
	end = (uint8_t)((AvcencOutput *)request)->next;
	for (; row < end; row++) {
		phase = row & 3;
		second = picture->chunkSteps + phase;
		slot = (AvcencOutputSlot *)vnzPublicOpenPrivate(outputs + (row & 31) * 64, 64);
		slot->row = row;
		slot->firstMb = 0;
		slot->output = request;
		slot->mbs = widthInMbs;
		slot->sliceType = stream->sliceType;
		slot->parity = 0;
		slot->chunk = phase;
		slot->outputInfo = stream->outputInfo;
		vnzPublicClosePrivate((uint32_t)slot, 64);
		if (row <= stream->chunkPending) {
			context = vkVThreadContextLock(picture->outputContext);
			vkVThreadJobSignal(context, phase);
			vkVThreadJobSignal(context, second);
			vkVThreadContextUnlock(context);
		}
	}
	stream->chunksDone = end;
	stream->waitWake = 1;
	stream->chunkLast = end - 1;
}

/*
 * 0x8385b0: a row's chunks are done (its bit in the frame record's
 * progress): the row is counted, the jobs of the row two below are set (and
 * signalled when it is started) and the row below's first jobs signalled;
 * the engine is told when the output's or the picture's last row is done
 */
static void streamChunkDone(AvcencStream *stream)
{
	AvcencPicture *picture = stream->picture;
	uint32_t row = stream->chunksSent, outputs = stream->outputs, heightInMbs, bit;
	uint32_t *progress = (uint32_t *)picture->frameRecord, end, next, phase, slot;
	VkVThreadContext *context, *current;

	heightInMbs = picture->heightInMbs;
	if (row >= heightInMbs)
		return;
	bit = 1 << (row & 31);
	if (!(bit & progress[4]))
		return;
	end = (uint8_t)((AvcencOutput *)((AvcencOutputSlot *)(outputs + (row & 31) * 64))->output)->next;
	vnzSpinLock(picture->lock);
	progress[4] &= ~bit;
	vnzSpinUnlock(picture->lock);
	stream->chunksSent++;
	((int8_t *)stream->record)[row]++;
	if (row == heightInMbs - 1) {
		vnzThreadSetFlags(picture->thread, STREAM_FLAG_PICTURE_DONE);
		return;
	}
	if (row < heightInMbs - 2) {
		next = row + 2;
		context = vkVThreadContextLock(picture->outputContext);
		phase = next & 3;
		slot = outputs + (next & 31) * 64;
		current = chunkJobSet(context, picture, phase, 2, avcencJobChunkStage1, slot);
		current = chunkJobSet(current, picture, phase + picture->chunkSteps, 2,
			avcencJobChunkStage2, slot);
		chunkJobSet(current, picture, phase + picture->chunkSteps * 2, 2,
			avcencJobChunkStage3, slot);
		stream->chunkPending = next;
		if (next <= stream->chunkLast) {
			vkVThreadJobSignal(context, phase);
			vkVThreadJobSignal(context, picture->chunkSteps + phase);
		}
		vkVThreadContextUnlock(context);
	}
	context = vkVThreadContextLock(picture->outputContext);
	phase = (row + 1) & 3;
	vkVThreadJobSignal(context, phase);
	vkVThreadJobSignal(context, picture->chunkSteps + phase);
	vkVThreadContextUnlock(context);
	if (end - 1 == row)
		vnzThreadSetFlags(picture->thread, STREAM_FLAG_OUTPUT_DONE);
}

/* 0x838312: a slot's next chunk: its first macroblock, width (to the row's end) and parity */
static void slotChunkNext(AvcencOutputSlot *slot, uint32_t chunk, uint32_t mbs, uint32_t width,
	uint32_t widthInMbs)
{
	uint8_t first;

	slot->parity = 1 - slot->parity;
	slot->chunk = chunk + 1;
	first = slot->firstMb + mbs;
	slot->firstMb = first;
	if (first + width >= widthInMbs)
		width = widthInMbs - first;
	slot->mbs = width;
}

/*
 * 0x837c10: a chunk of a row is done. Its slot moves to the row's next
 * chunk (the row's last chunk ends the output or the picture), the next
 * chunk's jobs of this row and the row below are signalled, and the row two
 * below gets its chunk jobs, by the row's flags. Returns whether the output
 * or the picture is done.
 */
static int32_t streamChunkNext(AvcencStream *stream, uint32_t chunk, uint32_t row)
{
	AvcencPicture *picture = stream->picture;
	uint32_t outputs = stream->outputs, widthInMbs = picture->widthInMbs, chunks;
	uint32_t first, end, outputEnd, flags, bit, slot2, steps, count, count2, base;
	int32_t done = 0, signal, signal2, limit, previous;
	int8_t *rowChunks = (int8_t *)stream->record, current;
	AvcencOutputSlot *slot;
	VkVThreadContext *context, *job;

	slot = (AvcencOutputSlot *)vnzPublicOpenPrivate(outputs + (row & 31) * 64, 64);
	first = slot->firstMb;
	outputEnd = (uint8_t)((AvcencOutput *)slot->output)->next;
	flags = slot->rowFlags[row >> 5];
	chunks = (uint8_t)stream->chunks;
	end = first + slot->mbs;
	if (end >= widthInMbs) {
		if (flags & ROW_END) {
			vnzThreadSetFlags(picture->thread, STREAM_FLAG_PICTURE_DONE);
			done = 1;
		} else if (outputEnd - 1 == row) {
			vnzThreadSetFlags(picture->thread, STREAM_FLAG_CHUNK | STREAM_FLAG_OUTPUT_DONE);
			done = 1;
		}
		stream->chunksSent++;
		signal = -1;
	} else {
		slotChunkNext(slot, chunk, slot->mbs, picture->chunkWidth, widthInMbs);
		signal = chunk + 1;
		if (row < 2 || (previous = rowChunks[row - 2]) == (int32_t)chunks)
			limit = chunk + 2;
		else
			limit = previous + (row & 3) * chunks;
		bit = 1 << (row & 31);
		if (signal < limit) {
			stream->chunkBits &= ~bit;
		} else {
			stream->chunkBits |= bit;
			signal = -1;
		}
	}
	vnzPublicClosePrivate((uint32_t)slot, 64);

	steps = picture->chunkSteps;
	current = rowChunks[row];
	if (flags & ROW_END) {
		signal2 = -1;
	} else {
		signal2 = ((row + 1) & 3) * chunks + current;
		if (flags & ROW_WRAP) {
			if (!first)
				signal2 = -1;
			else
				signal2--;
		}
	}

	/* the row two below */
	if (flags & ROW_MIDDLE) {
		slot2 = outputs + ((row + 2) & 31) * 64;
		bit = 1 << ((row + 2) & 31);
		count = (stream->chunkBits & bit) ? 1 : 2;
		stream->chunkBits &= ~bit;
		base = ((row + 2) & 3) * chunks + current;
		context = vkVThreadContextLock(picture->outputContext);
		if (end >= widthInMbs) {
			if (flags & ROW_FIRST) {
				chunkJobSet(context, picture, picture->chunkSteps * 2 + base - 1, count,
					avcencJobChunkStage3, slot2);
				job = chunkJobSet(context, picture, base, count, avcencJobChunkStage1,
					slot2);
				chunkJobSet(job, picture, base + picture->chunkSteps, count,
					avcencJobChunkStage2, slot2);
				count2 = (flags & ROW_SPLIT_END) ? 1 : 2;
				chunkJobSet(context, picture, picture->chunkSteps * 2 + base, 2,
					avcencJobChunkStage3, slot2);
				job = chunkJobSet(context, picture, base + 1, count2,
					avcencJobChunkStage1, slot2);
				chunkJobSet(job, picture, base + 1 + picture->chunkSteps, count2,
					avcencJobChunkStage2, slot2);
				chunkJobSet(context, picture, picture->chunkSteps * 2 + base + 1,
					(flags & ROW_SPLIT_END) ? 1 : 2, avcencJobChunkStage3, slot2);
			} else if (flags & ROW_CHUNK) {
				count2 = (flags & ROW_LAST) ? count - 1 : count;
				chunkJobSet(context, picture, picture->chunkSteps * 2 + base - 1, count,
					avcencJobChunkStage3, slot2);
				job = chunkJobSet(context, picture, base, count2, avcencJobChunkStage1,
					slot2);
				chunkJobSet(job, picture, base + picture->chunkSteps, count2,
					avcencJobChunkStage2, slot2);
				chunkJobSet(context, picture, picture->chunkSteps * 2 + base,
					(flags & ROW_LAST) ? 1 : 2, avcencJobChunkStage3, slot2);
			} else {
				chunkJobSet(context, picture, picture->chunkSteps * 2 + base - 1, 2,
					avcencJobChunkStage3, slot2);
			}
		} else {
			if (first)
				chunkJobSet(context, picture, picture->chunkSteps * 2 + base - 1, count,
					avcencJobChunkStage3, slot2);
			job = chunkJobSet(context, picture, base, count, avcencJobChunkStage1, slot2);
			chunkJobSet(job, picture, base + picture->chunkSteps, count,
				avcencJobChunkStage2, slot2);
		}
		if (!first) {
			stream->chunkPending = row + 2;
			if (row + 2 <= stream->chunkLast) {
				vkVThreadJobSignal(context, base);
				vkVThreadJobSignal(context, steps + base);
			}
		}
		vkVThreadContextUnlock(context);
	}

	/* the next chunk's jobs of this row and of the row below */
	context = vkVThreadContextLock(picture->outputContext);
	if (signal >= 0) {
		vkVThreadJobSignal(context, signal);
		vkVThreadJobSignal(context, steps + signal);
		vkVThreadJobSignal(context, steps * 2 + chunk);
	}
	if (signal2 >= 0) {
		vkVThreadJobSignal(context, signal2);
		vkVThreadJobSignal(context, steps + signal2);
		if (first && first != 2)
			vkVThreadJobSignal(context, steps * 2 + signal2 - 1);
		if (end >= widthInMbs)
			vkVThreadJobSignal(context, steps * 2 + signal2);
	}
	vkVThreadContextUnlock(context);
	if (end < widthInMbs)
		rowChunks[row]++;
	else
		rowChunks[row] = chunks;
	return done;
}

/*
 * 0x837a50: the slots of the output's rows not yet started, for rows of
 * several chunks: each row's first chunk (narrower by the row's place) and
 * its first jobs signalled when the row is set
 */
static void streamOutputsStart(AvcencStream *stream)
{
	AvcencPicture *picture = stream->picture;
	uint32_t chunks = (uint8_t)stream->chunks, outputs = stream->outputs;
	uint32_t row = stream->chunksDone, end, chunk, second, request;
	AvcencOutputSlot *slot;
	VkVThreadContext *context;

	if (picture->chunkWidth >= picture->widthInMbs) {
		streamChunksStart(stream);
		return;
	}
	request = vnzPublicOpenRaw(((AvcencJobInfo *)stream->jobInfo)->output, RECORD_SIZE);
	end = (uint8_t)((AvcencOutput *)request)->next;
	for (; row < end; row++) {
		chunk = (row & 3) * chunks;
		second = picture->chunkSteps + chunk;
		slot = (AvcencOutputSlot *)vnzPublicOpenPrivate(outputs + (row & 31) * 64, 64);
		slot->row = row;
		slot->firstMb = 0;
		slot->output = request;
		slot->mbs = picture->chunkWidth - row % (picture->chunkWidth - 1);
		slot->sliceType = stream->sliceType;
		slot->parity = 0;
		slot->chunk = chunk;
		slot->outputInfo = stream->outputInfo;
		vnzPublicClosePrivate((uint32_t)slot, 64);
		if (row <= stream->chunkPending) {
			context = vkVThreadContextLock(picture->outputContext);
			vkVThreadJobSignal(context, chunk);
			vkVThreadJobSignal(context, second);
			vkVThreadContextUnlock(context);
		}
	}
	stream->waitWake = 1;
	stream->chunksDone = end;
	stream->chunkLast = end - 1;
}

/*
 * 0x837b50: the rows whose chunks are done (their bits in the frame
 * record's progress), up to fifteen past the last row sent, while their
 * counts move; rows of one chunk go the other way
 */
static void streamChunksDone(AvcencStream *stream)
{
	AvcencPicture *picture = stream->picture;
	uint32_t chunks = (uint8_t)stream->chunks, row, end, bit, progress;
	uint32_t *frameProgress = (uint32_t *)picture->frameRecord;
	int32_t last, count;

	if (picture->chunkWidth >= picture->widthInMbs) {
		streamChunkDone(stream);
		return;
	}
	row = stream->chunksSent;
	progress = frameProgress[4];
	end = row + 15;
	if (end > picture->heightInMbs)
		end = picture->heightInMbs;
	for (last = chunks; row < end; row++) {
		count = ((int8_t *)stream->record)[row];
		if (!last && !count)
			return;
		if (last == count)
			continue;
		bit = 1 << (row & 31);
		if (bit & progress) {
			vnzSpinLock(picture->lock);
			frameProgress[4] &= ~bit;
			vnzSpinUnlock(picture->lock);
			if (streamChunkNext(stream, (row & 3) * chunks + count, row))
				return;
		}
		last = count;
	}
}

/*
 * 0x8356b0: the encoder stream's thread. A frame taken while idle starts
 * the picture's jobs (or waits, while the last picture's start is still
 * owed); the engine's flags then carry the picture through the rate
 * control, the loads, the wavefront and the chunks to the outputs, each
 * finished picture answered to the service with its access unit.
 */
void avcencStreamThread(uint32_t memory, uint32_t codec)
{
	AvcencStream *stream = streamBegin(memory);
	VkMessageQueue *innerQueue = stream->innerQueue, *engineQueue = stream->engineQueue;
	VkMessageQueue *auQueue = stream->auQueue;
	AvcencBuffers *buffers = stream->buffers;
	AvcencPicture *picture = stream->picture;
	VkThread *thread = picture->thread;
	int32_t waiting = 0, answer = 0, outputDone = 0, kindDone = 0, idr = 0;
	uint32_t flags;

	vnzThreadSetPriority(thread, vnzThreadPriority(thread) + 1);
	do {
		flags = streamWait(stream);
		if (stream->state == STREAM_IDLE && vkMessageReceive(auQueue, (uint32_t)stream)) {
			picture = stream->picture;
			if (!stream->ended) {
				if (waiting) {
					stream->state = STREAM_ENDING;
				} else {
					picturesStart(stream, picture);
					stream->state = STREAM_STARTED;
				}
			} else if (!waiting) {
				vkMessageSend(engineQueue, videoServiceAuDone, stream->request);
				streamRestart(stream);
			}
		}
		if (stream->state == STREAM_IDLE) {
			if (vkMessageReceive(innerQueue, (uint32_t)stream)) {
				picture = stream->picture;
				stream->state = STREAM_FLUSHING;
			} else if (answer) {
				vkMessageSend(engineQueue, videoServiceAuDone, stream->previousRequest);
				vnzThreadWaitFlags(STREAM_FLAG_ENCODED);
				pictureRateUpdate(stream, picture);
				clientRelease(stream);
				vnzThreadWaitFlags(STREAM_FLAG_UPDATED);
				vnzThreadSetFlags(picture->thread, STREAM_FLAG_AU);
				stream->state = STREAM_IDLE;
				vnzThreadWaitFlags(STREAM_FLAG_ANSWERED);
				answer = 0;
			}
		}
		if (stream->state == STREAM_ENDING &&
		    vkMessageReceive(innerQueue, (uint32_t)stream)) {
			picture = stream->picture;
			picturesStart(stream, picture);
			stream->state = STREAM_ENDED;
		}

		if (stream->state == STREAM_STARTED) {
			if (flags & STREAM_FLAG_LOADED)
				pictureLoads(stream, picture);
			if (flags & STREAM_FLAG_SCENE) {
				pictureKind(stream);
				if (answer) {
					vkMessageSend(engineQueue, videoServiceAuDone,
						stream->previousRequest);
					picturesRun(stream, picture, 1);
					vnzThreadWaitFlags(STREAM_FLAG_ENCODED);
					pictureRateUpdate(stream, picture);
					clientRelease(stream);
					vnzThreadWaitFlags(STREAM_FLAG_UPDATED);
					answer = 0;
					vnzThreadYield();
					vnzThreadWaitFlags(STREAM_FLAG_ANSWERED);
				} else {
					picturesRun(stream, picture, 0);
				}
				stream->state = STREAM_ENCODING;
			}
		}

		if (stream->state == STREAM_ENDED) {
			if (flags & STREAM_FLAG_CHUNK)
				streamChunksDone(stream);
			if (flags & STREAM_FLAG_LOADED)
				pictureLoads(stream, picture);
			if (flags & ~(STREAM_FLAG_LOADED | STREAM_FLAG_CHUNK)) {
				if (stream->waitWake && vkMessageReceive(innerQueue, (uint32_t)stream))
					picture = stream->picture;
				if (flags & STREAM_FLAG_OUTPUT) {
					vnzThreadWaitFlags(STREAM_FLAG_CHUNKS_SET);
					streamOutputsStart(stream);
					vnzThreadSetFlags(picture->thread, STREAM_FLAG_AU);
				}
				if (flags & (STREAM_FLAG_PICTURE_DONE | STREAM_FLAG_OUTPUT_DONE)) {
					vkMessageSend(stream->helperQueue, avcencHelperEncode,
						(uint32_t)outputNext(stream));
					if (flags & STREAM_FLAG_PICTURE_DONE)
						outputDone = 1;
				}
				if (flags & STREAM_FLAG_SCENE) {
					pictureKind(stream);
					kindDone = 1;
				}
				if (outputDone & kindDone) {
					sourceSwap(stream);
					picture = stream->picture;
					vkMessageSend(engineQueue, videoServiceAuDone,
						stream->previousRequest);
					picturesRun(stream, picture, 1);
					vnzThreadWaitFlags(STREAM_FLAG_ENCODED);
					pictureRateUpdate(stream, picture);
					vnzThreadWaitFlags(STREAM_FLAG_UPDATED);
					idr = pictureIdr(stream);
					kindDone = 0;
					outputDone = 0;
					buffers->flushing = 0;
					stream->state = STREAM_ENCODING;
					vnzThreadWaitFlags(STREAM_FLAG_ANSWERED);
				}
			}
		}

		if (stream->state == STREAM_FLUSHING) {
			if (flags & STREAM_FLAG_CHUNK)
				streamChunksDone(stream);
			if (flags & ~(STREAM_FLAG_LOADED | STREAM_FLAG_CHUNK)) {
				if (stream->waitWake && vkMessageReceive(innerQueue, (uint32_t)stream))
					picture = stream->picture;
				if (flags & STREAM_FLAG_OUTPUT) {
					vnzThreadWaitFlags(STREAM_FLAG_CHUNKS_SET);
					streamOutputsStart(stream);
					vnzThreadSetFlags(picture->thread, STREAM_FLAG_AU);
				}
				if (flags & (STREAM_FLAG_PICTURE_DONE | STREAM_FLAG_OUTPUT_DONE))
					vkMessageSend(stream->helperQueue, avcencHelperEncode,
						(uint32_t)outputNext(stream));
				if (flags & STREAM_FLAG_PICTURE_DONE) {
					sourceSwap(stream);
					picture = stream->picture;
					vkMessageSend(engineQueue, videoServiceAuDone,
						stream->previousRequest);
					vnzThreadWaitFlags(STREAM_FLAG_ENCODED);
					pictureRateUpdate(stream, picture);
					vnzThreadWaitFlags(STREAM_FLAG_UPDATED);
					idr = pictureIdr(stream);
					buffers->flushing = 1;
					vnzThreadSetFlags(picture->thread, STREAM_FLAG_AU);
					waiting = 0;
					stream->state = STREAM_IDLE;
					vnzThreadWaitFlags(STREAM_FLAG_ANSWERED);
				}
			}
		}

		if (stream->state == STREAM_ENCODING) {
			if (flags & STREAM_FLAG_LOADED)
				picturesStep(stream, picture);
			if (flags & STREAM_FLAG_RATE) {
				if (!idr) {
					pictureNext(stream);
					picture = stream->picture;
					vnzThreadSetFlags(picture->thread, STREAM_FLAG_AU);
					vnzThreadSetFlags(picture->engine, ENGINE_FLAG_HELPER);
					stream->state = STREAM_IDLE;
					stream->previousRequest = stream->request;
					if (picture->predicted) {
						waiting = 0;
						answer = 1;
					} else {
						waiting = 1;
						answer = 0;
					}
				} else {
					picturesRun(stream, picture, 0);
					idr = 0;
				}
			}
		}
	} while (stream->state != STREAM_STOPPED);
	streamEnd(stream);
}

/* the rate control's start, with the stream's first frame (avcencrate.c) */
void avcencRateInitialize(AvcencPicture *picture, uint32_t rate);

/*
 * 0x835b8c: a frame from the client (or the stream's end, told to the
 * engine). The first frame's size, rounded to whole macroblocks with the
 * crop that centres it, lays out the buffers and the picture; each frame's
 * planes are opened, its settings passed to the engine's sequence.
 */
void avcencStreamAu(uint32_t memory, uint32_t request)
{
	AvcencStream *stream = (AvcencStream *)memory;
	AvcencBuffers *buffers = stream->buffers;
	AvcencPicture *picture = stream->picture;
	VideoEngineRequest *frame;
	AvcencSequence *settings;
	uint32_t width, height;

	stream->request = request;
	frame = (VideoEngineRequest *)vnzPublicOpenPrivate(request, REQUEST_SIZE);
	if (frame->flags & VIDEO_ENGINE_DRAIN) {
		vnzPublicClosePrivate((uint32_t)frame, REQUEST_SIZE);
		stream->ended = 1;
		vnzThreadSetFlags(picture->engine, ENGINE_FLAG_ENDED);
		return;
	}
	buffers->frames++;
	settings = (AvcencSequence *)frame->buffer;	/* the encoder's: the frame's settings */
	stream->settings = settings;
	stream->option = frame->numOfPictures;		/* and its option */
	vnzPublicClosePrivate((uint32_t)frame, REQUEST_SIZE);
	picture = (AvcencPicture *)vnzPublicOpenPrivate(
		vnzPublicCloseRaw((uint32_t)stream->picture, PICTURE_SIZE), PICTURE_SIZE);

	if (!stream->started) {
		width = (settings->horizontal + 15) & ~15;
		height = (settings->vertical + 15) & ~15;
		picture->cropTop = 0;
		picture->cropLeft = 0;
		picture->cropped = 0;
		if (settings->horizontal != width) {
			picture->cropped = 1;
			picture->cropLeft = (16 - (settings->horizontal & 15)) >> 1;
		}
		if (settings->vertical != height) {
			picture->cropped = 1;
			picture->cropTop = (16 - (settings->vertical & 15)) >> 1;
		}
		buffers->width = width;
		buffers->height = height;
		buffersLayout(stream);
		pictureSetup(stream, picture);
		stream->started = 1;
	}
	buffers->clientLuma = vnzPublicOpenRaw(settings->luma, settings->height * settings->pitch);
	buffers->clientChroma = vnzPublicOpenRaw(settings->chroma,
		(settings->height >> 1) * settings->chromaPitch);
	buffers->clientWidth = settings->width;
	buffers->clientHeight = settings->height;
	buffers->clientPitch = settings->pitch;
	buffers->clientChromaPitch = settings->chromaPitch;
	pictureFrame(stream, picture);
	if (!buffers->frames)
		avcencRateInitialize(picture, stream->rate);
	vkMessageSend(stream->wakeQueue, avcencEncoderSequence, (uint32_t)settings);
	stream->picture = (AvcencPicture *)vnzPublicOpenRaw(
		vnzPublicClosePrivate((uint32_t)picture, PICTURE_SIZE), PICTURE_SIZE);
}

/* the rate control's jobs after the stream thread's picture (avcencrate.c) */
void avcencJobRateFirst(uint32_t picture, uint32_t jobInfo, uint32_t rate, uint32_t unused);
void avcencJobRateOutput(uint32_t picture, uint32_t jobInfo, uint32_t rate, uint32_t unused);

#define PICTURE_INFO_SIZE	896

/*
 * 0x835d0c: an output the helper parsed. The picture's first one brings the
 * picture's info (the rate control's QP and delays go into it) and the
 * client's memory: the picture takes its outputs, job record and command
 * buffers, its source frame and macroblocks, and the row layout. Every
 * output sets its job record and the jobs that fill it.
 */
void avcencStreamParsed(uint32_t memory, uint32_t job)
{
	AvcencStream *stream = (AvcencStream *)memory;
	uint32_t rate = stream->rate, jobInfo, index;
	AvcencPicture *picture = stream->picture;
	AvcencBuffers *buffers = stream->buffers;
	AvcencOutput *output;
	AvcencJobInfo *info;
	AvcencClientMemory *client;
	VideoAvcPictureInfo *pictureInfo;
	AvcencRate *rateState;
	uint32_t mbs, firstMb, reference, *sourceChroma, source;
	VkVThreadContext *context;

	output = (AvcencOutput *)vnzPublicOpenPrivate(job, RECORD_SIZE);
	pictureInfo = (VideoAvcPictureInfo *)output->info;
	mbs = output->mbs;
	client = (AvcencClientMemory *)pictureInfo->encoder;
	reference = pictureInfo->reference;
	firstMb = output->firstMb;
	if (output->withInfo) {
		stream->pictureFrame = (uint32_t)pictureInfo;
		if (!firstMb) {
			rateState = (AvcencRate *)vnzPublicOpenProtected(rate, RATE_STATE_SIZE);
			pictureInfo = (VideoAvcPictureInfo *)vnzPublicOpenPrivate(
				(uint32_t)pictureInfo, PICTURE_INFO_SIZE);
			pictureInfo->qp = rateState->qp;
			pictureInfo->initialCpbRemovalDelay = rateState->initialCpbRemovalDelay;
			pictureInfo->vclInitialCpbRemovalDelay = rateState->vclInitialCpbRemovalDelay;
			pictureInfo->initialCpbRemovalDelayOffset =
				rateState->initialCpbRemovalDelayOffset;
			pictureInfo->vclInitialCpbRemovalDelayOffset =
				rateState->vclInitialCpbRemovalDelayOffset;
			pictureInfo->reserved3[0] = 0;
			pictureInfo->alphaOffset = 0;
			pictureInfo->betaOffset = 0;
			vnzPublicClosePrivate((uint32_t)pictureInfo, PICTURE_INFO_SIZE);
			vnzPublicCloseProtected((uint32_t)rateState, RATE_STATE_SIZE);
		}
		client = (AvcencClientMemory *)vnzPublicOpenProtected((uint32_t)client, 64);
		if (firstMb) {
			stream->newSequence = 0;
		} else {
			picture = (AvcencPicture *)vnzPublicOpenPrivate(
				vnzPublicCloseRaw((uint32_t)stream->picture, PICTURE_SIZE),
				PICTURE_SIZE);
			picture->output = reference;
			stream->outputs = client->outputs;
			stream->jobInfo = client->jobInfo;
			stream->outputInfo = client->outputInfo;
			picture->outputContext = client->outputContext;
			picture->command[0] = client->command0;
			picture->command[3] = client->command3;
			picture->command[1] = client->command1;
			picture->command[2] = client->command2;
			picture->command[4] = client->command4;
			picture->command[5] = client->command5;
			picture->commandBuffer = client->commandBuffer;
			picture->commandBufferEnd = client->commandBuffer + 2048;
			picture->commandBuffer2 = client->commandBuffer2;
			stream->record = client->record;
			stream->record = vnzPublicOpenPrivate(client->record, RECORD_SIZE);
			picture->macroblocks = buffers->rowsLeft;
			picture->widthInMbs = buffers->width >> 4;
			picture->heightInMbs = buffers->height >> 4;
			stream->sliceType = buffers->idrNext ? 2 : 0;
			stream->newSequence = 1;
			source = stream->source;
			picture->previousChroma = picture->chroma;
			picture->previousLuma = picture->luma;
			picture->luma = buffers->luma[source];
			sourceChroma = &buffers->chroma[source];
			picture->chroma = *sourceChroma;
			picture->width = buffers->frameWidth;
			picture->chromaWidth = buffers->chromaWidth;
			picture->luma = vnzPublicCloseRaw(picture->luma, buffers->lumaSize);
			buffers->luma[source] = 0;
			picture->chroma = vnzPublicCloseRaw(picture->chroma, buffers->chromaSize);
			*sourceChroma = 0;
			picture->macroblocks = vnzPublicOpenRaw(picture->macroblocks,
				(buffers->width * buffers->height >> 8) << 6);
			streamRowsLayout(stream, picture);
			picture = (AvcencPicture *)vnzPublicOpenRaw(
				vnzPublicClosePrivate((uint32_t)picture, PICTURE_SIZE),
				PICTURE_SIZE);
		}
		vnzPublicCloseProtected((uint32_t)client, 64);
		stream->picture = picture;
	}

	output->next = (firstMb + mbs) / picture->widthInMbs;
	stream->waitWake = 0;
	info = (AvcencJobInfo *)vnzPublicOpenPrivate(stream->jobInfo, 64);
	info->outputFlags[0] = output->flags[0];
	info->outputFlags[1] = output->flags[1];
	info->skipLevel = stream->skipLevel;
	info->sliceType = stream->sliceType;
	info->mbs = output->mbs;
	info->output = vnzPublicClosePrivate((uint32_t)output, RECORD_SIZE);
	info->chunks = stream->chunks;
	info->sliceMode = stream->sliceMode;
	info->outputs = stream->outputs;
	info->outputInfo = stream->outputInfo;
	jobInfo = vnzPublicClosePrivate((uint32_t)info, 64);

	index = picture->index * 8;
	context = vkVThreadContextLock(stream->context);
	if (!firstMb) {
		while (vkVThreadJobSetNotify(context, index + 4, 0, avcencJobChunks,
		    (uint32_t)picture, jobInfo, 0, 0, picture->thread, STREAM_FLAG_CHUNKS_SET)) {
			vkVThreadContextUnlock(context);
			context = vkVThreadContextLock(stream->context);
		}
		while (vkVThreadJobSetNotify(context, index + 7, 0, avcencJobRateFirst,
		    (uint32_t)picture, jobInfo, rate, 0, picture->thread, STREAM_FLAG_OUTPUT)) {
			vkVThreadContextUnlock(context);
			context = vkVThreadContextLock(stream->context);
		}
	} else {
		while (vkVThreadJobSetNotify(context, index + 7, 0, avcencJobRateOutput,
		    (uint32_t)picture, jobInfo, rate, 0, picture->thread,
		    STREAM_FLAG_OUTPUT | STREAM_FLAG_CHUNKS_SET)) {
			vkVThreadContextUnlock(context);
			context = vkVThreadContextLock(stream->context);
		}
	}
	vkVThreadContextUnlock(context);
}
