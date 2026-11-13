/*
 * The AVC encoder's stream: its memory, the picture's and the rate control's
 * state, the buffers and the records the stream thread, the rate control and
 * the preparing jobs share.
 */
#ifndef AVCENC_STREAM_H
#define AVCENC_STREAM_H

#include "service.h"
#include "avcenckernel.h"

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
	uint8_t reserved[80];
	uint64_t sums[6];		/* the running sums, cleared at the start */
	uint32_t counts[4];
	uint32_t maxCpbLimit;		/* nine tenths of the CPB sizes */
	uint32_t cpbLimit;
	uint32_t maxFrameBits;		/* the bit rates per frame */
	uint32_t frameBits;
	uint32_t reserved2;
	int8_t sceneChange;
	int8_t idrRequested;
	int8_t intraRequested;
	int8_t idrTaken;
	uint16_t pictures;
	int8_t idrCounted;
	uint8_t reserved3;
	uint32_t intraBits;
	int8_t qp;			/* the picture's, to its info */
	int8_t intra;
	uint8_t reserved4;
	int8_t initialQp;		/* from the bits a macroblock gets */
	uint8_t reserved5[12];
	uint32_t error[2];
	uint32_t gain;			/* 1.5 in Q16 */
	uint32_t gain2;			/* a third in Q15 */
	uint8_t reserved6[8];
	uint32_t pictureBits;		/* a picture's share of the CPB, rounded up to 2048 bits */
	uint32_t pictureBitsLimit;
	uint8_t reserved7[28];
	uint32_t frameRate;		/* in Q15 */
	uint32_t reserved8;
	uint32_t weights[3];		/* 1.0 in Q15 */
	uint32_t intraCount;
	uint8_t reserved9[18];
	int8_t forceIdr;
	uint8_t idrPending;
	uint8_t reserved10[4];
	uint32_t initialCpbRemovalDelay;
	uint32_t vclInitialCpbRemovalDelay;
	uint32_t initialCpbRemovalDelayOffset;
	uint32_t vclInitialCpbRemovalDelayOffset;
	uint8_t reserved11[24];
	uint8_t overflow[3];
	uint8_t reserved12[25];
	int8_t sceneIdr;
	uint8_t reserved13;
	int16_t qpLimit;
	uint8_t reserved14[204];
} AvcencRate;

/* the picture's progress, which its jobs share (at PICTURE_FRAME_OFFSET) */
typedef struct AvcencFrameRecord {
	uint32_t reserved;
	uint32_t progress[3];		/* cleared with the rows' slots */
	uint32_t rowsEncoded;		/* by row & 31: encoded, not yet answered */
	int8_t references[4];		/* the reference loads' group, by reference */
	int8_t loaded;			/* the last group of rows loaded, -1 for none */
	int8_t chunkStep;		/* the chunk stages' next step */
	int16_t step;			/* the motion wavefront's next step */
	int8_t started[8];		/* the jobs started, by step of four */
} AvcencFrameRecord;

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

/* the slices' stream info: a part per core */
#define CORE_INFO_SIZE		2112

/* a core's load of the next source's motion plane, at the start of its part (64 bytes) */
typedef struct AvcencLoad {
	uint8_t *motion;		/* the band's first line of the plane */
	uint32_t source;		/* the rows' first line */
	uint8_t rows;			/* of macroblocks, up to four */
	uint8_t firstRow;
	uint16_t y;			/* the band's first line */
	uint16_t width;			/* in pixels */
	uint16_t height;
	uint16_t motionWidth;		/* the plane's pitch */
	uint16_t pitch;			/* the source's, of a row of tiles over 16 */
	uint8_t *lines;			/* the vertical pass's last three lines, 24 bytes a block */
	uint32_t linesOpened;
	uint32_t motionOpened;
	uint8_t reserved[32];
} AvcencLoad;


typedef char jobInfoSizeCheck[sizeof(AvcencJobInfo) == 64 ? 1 : -1];
typedef char loadSizeCheck[sizeof(AvcencLoad) == 64 ? 1 : -1];
typedef char buffersSizeCheck[sizeof(AvcencBuffers) == BUFFERS_SIZE ? 1 : -1];
typedef char slicesSizeCheck[sizeof(AvcencSlices) == SLICES_SIZE ? 1 : -1];
typedef char rateSizeCheck[sizeof(AvcencRate) == RATE_STATE_SIZE ? 1 : -1];
typedef char recordSizeCheck[sizeof(AvcencRecord) == RECORD_SIZE ? 1 : -1];
typedef char streamSizeCheck[sizeof(AvcencStream) == STREAM_HEADER_SIZE ? 1 : -1];

#endif
