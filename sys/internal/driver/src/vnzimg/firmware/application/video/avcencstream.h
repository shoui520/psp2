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
	VnzKernelThread *thread;		/* the stream thread's */
	VnzKernelThread *engine;		/* the encoder engine's */
	int32_t id;			/* the codec's */
	VnzKernelVThreadContext *outputContext; /* the client's, for the output's jobs */
	VnzKernelVThreadContext *context;
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
	uint32_t layout;		/* the output's arrays' pitches (AvcencLayout) */
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
	int8_t rateChromaQp;
	int8_t rateDone;
	int8_t cropped;			/* the frame is not whole macroblocks */
	uint8_t cropLeft;
	uint8_t cropTop;
	uint8_t reserved3;
	uint32_t interRecords;		/* the inter decision's, 64 bytes a macroblock */
	uint32_t macroblockRecords;		/* the macroblocks', 64 bytes each */
	uint32_t rowsMotion;
	uint32_t reserved4[2];
	uint32_t searchResults;		/* the coarse search's, 64 bytes a group of rows */
	uint32_t records;		/* the sixteen picture records */
	uint32_t command[6];		/* the job record's */
	uint32_t reserved5[2];
	uint32_t lumaCoded;		/* the rows' coded blocks (AvcencLumaCoded), 64 bytes a row, two sets */
	uint32_t chromaCoded;		/* (AvcencChromaCoded) */
	uint32_t strengths;		/* the rows' bS (AvcencStrengths), 64 bytes a macroblock */
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

/*
 * the rate control's state (576 bytes). The HRD's times are in the time
 * scale's units, shifted left by 8 (the picture's removal times, its bits'
 * arrival), NAL then VCL; the bits are the picture's as the settings answer
 */
typedef struct AvcencRate {
	uint64_t delay[2];		/* initial_cpb_removal_delay */
	uint64_t delayOffset[2];	/* initial_cpb_removal_delay_offset */
	uint64_t savedRemoval[2];	/* the times before the picture, for a skipped one */
	uint64_t savedDelay[2];
	uint64_t savedDelayOffset[2];
	uint64_t initialArrival[2];	/* the next picture's first bit */
	uint64_t finalArrival[2];	/* the picture's last bit */
	uint64_t removal[2];		/* the next picture's removal */
	int32_t groupBitsLeft;		/* of the IDR period's bits, at the picture bit rate */
	int32_t groupMaxBitsLeft;	/* and at the picture maximum bit rate */
	int32_t groupBits;
	int32_t fullness;		/* the bits taken over the average, within half the CPB */
	uint32_t maxCpbLimit;		/* the most bits the next picture may take, by HRD */
	uint32_t cpbLimit;
	uint32_t maxFrameBits;		/* the bit rates per frame */
	uint32_t frameBits;
	uint32_t averageBits;		/* a picture's at the picture bit rate */
	int8_t nalOverflow;		/* the picture overflowed the NAL HRD */
	int8_t vclOverflow;		/* the VCL HRD */
	int8_t oversized;		/* it took more than the picture may */
	int8_t idr;			/* the picture is an IDR */
	uint16_t picturesLeft;		/* in the IDR period */
	int8_t periodicIdr;		/* the IDR period's start */
	uint8_t reserved;
	uint32_t targetBits;		/* the picture's */
	int8_t qp;			/* the picture's */
	int8_t chromaQp;
	int8_t chosenQp;		/* the rate control's */
	int8_t initialQp;		/* an IDR's, from the bits a macroblock gets */
	uint8_t previousQp;		/* the last picture's */
	uint8_t intraQp;		/* the last IDR's */
	uint16_t interPictures;		/* since the last restart */
	uint16_t groupPictures;		/* the IDR period's */
	uint16_t reserved2;
	uint32_t interQpSum;		/* of the inter pictures since the last restart */
	uint32_t intraComplexity;	/* the last IDR's bits, at a step of 1.0 */
	uint32_t interComplexity;	/* the inter pictures', summed */
	uint32_t intraRatio;		/* an IDR's complexity to an inter picture's, in Q15 */
	uint32_t interRatio;		/* and its inverse */
	uint32_t reserved3;
	uint32_t minimumBits;		/* an eighth of the average */
	uint32_t pictureBits;		/* a picture's share of the CPB, rounded up to 2048 bits */
	uint32_t pictureBitsLimit;
	uint32_t sceneSizeLimit;	/* four fifths of the picture size, after a scene change */
	uint32_t sizeLimit;		/* and otherwise */
	int8_t limited;			/* the target is the limit's */
	uint8_t reserved4[3];
	uint32_t cost;			/* the motion job's statistics: the SATDs */
	uint32_t modeBits;		/* the modes' and vectors' bits */
	uint32_t reserved5;
	uint32_t pictureBitRate;	/* the one the IDR period's bits are from */
	uint32_t frameRate;		/* in Q15 */
	uint32_t quantizerStep;		/* the QP's, in Q15 */
	uint32_t weight;		/* the bits a step of cost takes, in Q15: the picture's, */
	uint32_t intraWeight;		/* an intra macroblock's */
	uint32_t interWeight;		/* and an inter one's */
	uint32_t pictures;		/* coded */
	uint32_t numUnitsInTick;	/* compared with the picture's */
	uint32_t timeScale;
	uint8_t reserved6[9];
	int8_t codedQp;
	int8_t sceneChange;		/* the scene job's */
	uint8_t idrPending;
	uint16_t intraMbs;		/* the motion job's statistics */
	uint16_t interMbs;
	uint32_t initialCpbRemovalDelay;
	uint32_t vclInitialCpbRemovalDelay;
	uint32_t initialCpbRemovalDelayOffset;
	uint32_t vclInitialCpbRemovalDelayOffset;
	uint32_t delayTotal[2];		/* the delays and their offsets, as the first picture had them */
	uint32_t sliceBits;		/* the picture's, as the settings answer them */
	uint32_t headerBits;
	uint32_t skipSliceBits;		/* and a skipped one's */
	uint32_t skipHeaderBits;
	int8_t restart;			/* the bit rate changed: the QP is estimated again */
	int8_t maxQp;			/* the picture takes QP 51 */
	int8_t skipNext;		/* the next rate job does nothing */
	int8_t wasIdr;			/* the picture the rate job ran for */
	uint32_t savedLimits[2];	/* the CPB limits before the picture */
	uint32_t savedDelays[4];	/* and the delays */
	int8_t pictureOverflow;		/* it took more than a picture's share of the CPB */
	uint8_t reserved7;
	int16_t qpLimit;
	int16_t interCount;		/* the motion job's statistics */
	uint16_t skipCount;
	uint32_t historyBits[16];	/* the last inter pictures' bits and costs */
	uint32_t historyCost[16];
	uint32_t sinceScene;		/* pictures since the last scene change */
	uint8_t historyIndex;
	uint8_t historyCount;
	int8_t sceneKind;		/* 1 a scene change, 2 a smaller one */
	int8_t previousSceneKind;
	uint8_t reserved8[64];
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
	uint16_t modesMbs;		/* the macroblocks its mode arrays hold */
	uint16_t firstMb;		/* 0 for the picture's first output */
	int8_t flags;			/* to the job record */
	uint8_t firstRow;
	uint8_t reserved1;
	int8_t next;			/* the row after its last (the output after it) */
	uint8_t withInfo;		/* the picture's info comes with it */
	uint8_t reserved2;
	int8_t hasPicture;
	uint8_t reserved3;
	uint32_t info;			/* the picture's VideoAvcPictureInfo */
	uint32_t macroblocks;		/* its VideoAvcMacroblock records */
	uint32_t modes;			/* the 4x4 blocks' modes of each Intra4x4 macroblock */
	uint32_t chromaModes;		/* each intra macroblock's chroma mode */
	uint32_t reserved5;
	uint32_t differences;		/* each coded inter macroblock's vector differences */
	uint32_t levels;		/* the rows' levels */
	uint32_t chromaLevels[2];	/* Cb's, Cr's */
	uint8_t reserved6[76];
} AvcencOutput;

/* the output's arrays: the bytes a row of macroblocks takes in each */
typedef struct AvcencLayout {
	uint16_t macroblocksPitch;
	uint16_t modesPitch;
	uint16_t chromaModesPitch;
	uint16_t reserved2;
	uint16_t differencesPitch;
	uint16_t levelsPitch;
	uint16_t chromaLevelsPitch;
} AvcencLayout;

/* the modes job's per core info (64 bytes of 192 a core, in the output info) */
typedef struct AvcencModesInfo {
	uint8_t reserved[12];
	uint32_t modes;			/* the output's arrays, opened */
	uint32_t chromaModes;
	uint32_t differences;
	uint32_t output;		/* the AvcencOutput, opened */
	uint8_t reserved2[36];
} AvcencModesInfo;

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
	uint32_t searchResults;
	uint32_t interRecords;
	uint32_t macroblockRecords;
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
	VnzKernelMessageQueue *auQueue;
	VnzKernelMessageQueue *engineQueue;
	VnzKernelMessageQueue *wakeQueue;
	VnzKernelMessageQueue *innerQueue;
	VnzKernelMessageQueue *helperQueue;
	VnzKernelVThreadContext *context;
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
	VnzKernelVThreadContext *outputContext;
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
	uint32_t coded;			/* the coded blocks' rows, luma's then chroma's: 2048 bytes each */
	uint32_t strengths;
	uint32_t reserved3;
} AvcencClientMemory;

typedef char clientMemorySizeCheck[sizeof(AvcencClientMemory) == 64 ? 1 : -1];

/* the picture's job record, for the output's jobs */
typedef struct AvcencJobInfo {
	uint32_t output;		/* the AvcencOutput it fills */
	int8_t outputFlags;		/* the output's */
	uint8_t firstRow;
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

/*
 * the reference job's per core info (CORE_INFO_SIZE): a coarse search of each
 * 64x64 block of four rows of macroblocks in the previous motion plane. The
 * SAD maps hold a row for each place across the window (25, two samples
 * apart) and a column for each line down (17): the halfword (line & 3) * 4 +
 * line / 4, the seventeenth at 16
 */
#define SEARCH_PLACES	25
typedef struct AvcencSearchInfo {
	uint16_t top[SEARCH_PLACES][20];	/* the block's top 32 lines' */
	uint16_t bottom[SEARCH_PLACES][20];	/* its bottom 32 lines' */
	uint8_t reserved[24];
	uint8_t columns;		/* of macroblocks in the block, up to four */
	uint8_t rows;
	uint8_t reserved2;		/* cleared */
	uint8_t firstRow;
	uint16_t x;			/* the block's, in pixels */
	uint16_t y;
	uint16_t width;
	uint16_t height;
	uint16_t motionWidth;
	uint16_t lastVector;		/* the last block's, x in the low byte */
	uint16_t *cursor;		/* the next block's vector in the results */
	uint32_t results;		/* the group's 64 bytes of them */
	uint32_t *sumPointer;		/* where the SADs' sum goes, the results' last word */
	uint32_t sum;
	const uint8_t *previous;	/* the previous plane's first sample */
	uint32_t reserved3;
	const uint8_t *current;		/* the group's first line's of the current plane */
	int8_t lengthWeight;		/* of a vector's length, in its cost */
	int8_t changeWeight;		/* of its change from the last block's */
	uint8_t reserved4[42];
} AvcencSearchInfo;

/*
 * a macroblock's inter decision (64 bytes): its vector, each 8x8 block's
 * offset from it and the sub-pel refinement's result
 */
typedef struct AvcencInterRecord {
	int8_t deltas[4][2];		/* each 8x8 block's, in whole pixels, x and y */
	int16_t vectors[4][2];		/* the refined ones, by partition, in quarter pixels */
	int16_t differences[4][2];	/* the vectors' differences from their predictions */
	uint16_t costs[4];		/* the SATDs at the whole pixels, by partition */
	int16_t vector[2];		/* the macroblock's, in pixels */
	uint8_t reserved2[4];
	uint8_t partition;		/* 0 16x16, 1 16x8, 2 8x16, 3 8x8 */
	int8_t skip;			/* the decision takes P_Skip */
	int8_t intra;			/* it takes intra, the vector is not refined */
	uint8_t reserved3[5];
} AvcencInterRecord;

/*
 * a row of groups' motion statistics (64 bytes, sixteen of them), summed
 * into the first at the picture's end for the rate control
 */
typedef struct AvcencMotionStats {
	uint32_t cost;			/* the macroblocks' SATDs, intra ones weighted */
	uint32_t bits;			/* their modes' and vectors' bits */
	uint32_t average;		/* divided by the macroblocks at the end */
	uint16_t intraCount;
	uint16_t intra16x16Count;
	uint16_t interCount;
	uint16_t reserved;
	uint16_t skipCount;
	uint16_t skipDecidedCount;	/* skipped as the inter decision had it */
	uint16_t skipFoundCount;	/* skipped by the check alone */
	uint8_t reserved2[38];
} AvcencMotionStats;

/*
 * the motion job's per core info (1280 bytes of CORE_INFO_SIZE): each
 * macroblock's final mode, its vectors' coding and the P_Skip check, with
 * the records of its neighbours (left, above, above right and above left)
 */
#define MOTION_INFO_SIZE	1280
typedef struct AvcencMotionInfo {
	uint8_t reserved[840];
	uint8_t firstColumn;		/* the row's */
	uint8_t row;
	uint8_t mbX;
	uint8_t mbY;
	uint32_t sliceType;
	uint32_t reserved2;
	uint32_t reference;		/* the reference picture's luma */
	uint32_t referenceChroma;
	uint32_t luma;			/* the macroblock's, in the source */
	uint32_t chroma;
	uint16_t pitch;			/* the reference's width */
	uint16_t chromaPitch;
	uint16_t width;			/* in pixels */
	uint16_t height;
	int16_t skipVector[2];		/* P_Skip's prediction, in quarter pixels */
	uint16_t interCost;		/* the SATD and the weighted vector bits */
	uint16_t interSatd;
	uint8_t interBits;
	uint8_t columns;		/* in the row */
	int8_t sliceStart;		/* the row starts a slice */
	uint8_t interWeight;		/* by QP */
	int8_t qp;
	uint8_t lumaShift2;
	uint8_t lumaShift;
	uint8_t chromaShift;
	const int16_t *lumaThreshold2;
	const int16_t *lumaThreshold;
	const int16_t *chromaThreshold;
	uint32_t macroblock;		/* the macroblock's record, as the decisions left it */
	uint32_t macroblockAbove;
	uint32_t macroblockLeft;
	uint32_t interRecord;		/* the inter decision's records */
	uint32_t interLeft;
	uint32_t interAbove;
	uint32_t interAboveRight;
	uint32_t interAboveLeft;
	uint32_t record;		/* the macroblock records */
	uint32_t recordLeft;
	uint32_t recordAbove;
	uint32_t recordAboveRight;
	uint32_t recordAboveLeft;
	AvcencMotionStats *stats;	/* the row of groups' */
	uint8_t reserved3[304];
	int16_t sourcePitch;		/* the next picture's */
	int16_t sourceChromaPitch;
	uint16_t intraWeight;		/* by QP */
	uint8_t reserved4[10];
} AvcencMotionInfo;

/*
 * the refinement job's per core info (896 bytes of CORE_INFO_SIZE): each
 * macroblock's 8x8 blocks' reference windows and candidates
 */
#define REFINE_INFO_SIZE	896
typedef struct AvcencRefineInfo {
	uint8_t windows[4][192];	/* the left halves' twelve lines, then the right's */
	uint16_t candidates[REFINE_CANDIDATES][4];
	uint32_t luma;			/* the macroblock's, in the source */
	uint32_t reference;		/* the reference picture's luma */
	uint8_t columns;		/* of macroblocks in the group, up to four */
	uint8_t rows;
	uint8_t column;			/* the group's first macroblock */
	uint8_t row;
	uint16_t mbX;			/* the macroblock's */
	uint16_t mbY;
	uint16_t width;			/* in pixels */
	uint16_t height;
	uint16_t pitch;			/* the reference's width */
	uint16_t reserved;
	uint32_t interRecord;		/* the macroblock's */
	uint8_t reserved2[28];
} AvcencRefineInfo;

/*
 * a macroblock's decisions (64 bytes), as the next jobs read them; the
 * motion job writes its type over the modes and, for an inter macroblock,
 * its vectors over the 4x4 blocks' modes
 */
#define MB_KIND			0x300		/* its type: intra, or the inter partitions */
#define MB_INTRA		0x200
#define MB_SKIP_SHIFT		11		/* P_Skip */
#define MB_CHECKED_SHIFT	28		/* the skip check passed: no residual */
#define MB_CHROMA_MODE_SHIFT	14		/* an intra macroblock's chroma prediction */
#define MB_CHROMA_CBP_SHIFT	20		/* coded_block_pattern's chroma */
#define MB_KEEP_RIGHT		(1 << 24)	/* the macroblock right predicts from it */
#define MB_KEEP_BOTTOM		(1 << 25)	/* the one below */
typedef struct AvcencMacroblock {
	union {
		struct {
			uint8_t lumaMode;	/* the 16x16 prediction's, 4 for 4x4 blocks */
			uint8_t chromaMode;
		};
		uint32_t type;		/* the macroblock's type and coding, from the motion job */
	};
	uint8_t reserved[4];
	union {
		uint8_t blockModes[16];	/* the 4x4 blocks', in decoding order */
		int16_t vectors[4][2];	/* each 8x8 block's, in quarter pixels */
	};
	union {
		int8_t modeCodes[16];	/* -128 for the predicted mode, else rem_intra4x4_pred_mode */
		int16_t differences[4][2];	/* the vectors' from their predictions */
		uint64_t modeWords[2];	/* the codes, as the modes job copies them */
		uint32_t differenceWords[4];	/* a difference a word */
	};
	uint8_t reserved2[8];
	uint16_t intraCost;		/* the 4x4 blocks' SATD; all ones when inter is taken */
	uint16_t reserved3;
	uint16_t intraBits;		/* the modes' bits, less one a 4x4 block */
	uint8_t reserved4[10];
} AvcencMacroblock;

/*
 * the decide job's per core info (1344 bytes): each macroblock of a group,
 * a row at a time, the inter decision (around the coarse search's vector)
 * then the intra one and the final choice. The kernels' and the decisions'
 * own state takes the rest
 */
#define DECIDE_INFO_SIZE	1344
typedef struct AvcencDecideInfo {
	AvcencIntraRow intraRows[6];	/* the 4x4 candidates, by row, and the predictions kept */
	/* the intra decision's */
	uint8_t columns;		/* of macroblocks in the group, up to four */
	uint8_t rows;
	uint8_t column;			/* the group's first macroblock */
	uint8_t row;
	uint16_t mbX;			/* the macroblock's */
	uint16_t mbY;
	uint32_t sliceType;
	uint16_t widthInMbs;
	uint16_t heightInMbs;
	int16_t pitch;			/* the source's */
	int16_t chromaPitch;
	uint32_t sourceLuma;
	uint32_t sourceChroma;
	uint32_t luma;			/* the macroblock's, in the source */
	uint32_t chroma;
	uint32_t interRecord;		/* the inter decision's, of the macroblock */
	uint32_t record;		/* the macroblock's, the decisions' result */
	int8_t sliceStart;		/* the row starts a slice */
	uint8_t reserved2;
	uint16_t intraWeight;		/* by QP */
	uint8_t reserved3[984];
	/* the inter decision's */
	uint32_t reference;		/* the reference picture's luma */
	uint32_t referenceChroma;
	uint32_t interLuma;		/* the macroblock's, in the source */
	uint32_t interChroma;
	const int8_t *vector;		/* the coarse search's, of the group's block */
	uint32_t results;		/* the coarse search's, of the group's row */
	uint8_t interColumns;
	uint8_t interRows;
	uint8_t interColumn;
	uint8_t interRow;
	uint16_t interX;
	uint16_t interY;
	uint16_t referencePitch;
	uint16_t referenceChromaPitch;
	uint16_t width;			/* in pixels */
	uint16_t height;
	uint32_t interOutput;		/* where the inter decision's record goes */
	const int16_t *lumaThreshold;	/* the slices', by QP */
	int8_t lumaShift;
	uint8_t interWeight;		/* by QP */
	uint8_t reserved4[2];
	uint32_t prefetchLuma;		/* the reference's next lines to bring in */
	uint32_t prefetchChroma;
	uint16_t prefetchLumaSize;
	uint16_t prefetchChromaSize;
	uint8_t prefetchLine;		/* their first line, its low eight bits */
	int8_t scaled;			/* the slices' */
	uint8_t reserved5[6];
} AvcencDecideInfo;


/*
 * a macroblock's bS (32 bytes, 64 apart in the picture's strengths): by
 * direction (the horizontal edges, then the vertical ones), edge (0 the
 * macroblock's own) and 4-sample group along it. The luma reads an 8x8
 * block's two groups at a time, the chroma an edge's four
 */
typedef union AvcencStrengths {
	uint8_t edges[2][4][4];
	uint16_t pairs[2][4][2];
	uint32_t words[2][4];
} AvcencStrengths;
#define STRENGTHS_HORIZONTAL	0
#define STRENGTHS_VERTICAL	1

/* a macroblock's coded blocks, as the deblocking reads them (8 bytes, a row's 64) */
typedef struct AvcencLumaCoded {
	uint16_t blocks;		/* the 4x4 blocks with coefficients, by bit (z order) */
	uint8_t dc;			/* Intra16x16's DC */
	uint8_t cbp;			/* coded_block_pattern's luma bits */
	uint16_t leftBlocks;		/* the left and top neighbours', as the strengths read them */
	uint16_t topBlocks;
} AvcencLumaCoded;

typedef struct AvcencChromaCoded {
	uint16_t ac;			/* the 4x4 blocks with AC coefficients: Cb's four, Cr's four */
	uint8_t dc;			/* Cb's, Cr's */
	uint8_t cbp;			/* coded_block_pattern's chroma */
	uint32_t reserved;
} AvcencChromaCoded;

/* the deblocking's record of a row (128 bytes, at the job record's command[4]) */
typedef struct AvcencDeblockRow {
	uint8_t lumaColumns[2][4][8];	/* the last macroblock's right four columns, by half, turned */
	uint8_t chromaColumns[2][2][8];	/* its columns 6 and 7, by component */
	uint8_t firstMb;		/* the chunk the next deblocking takes: the one encoded last */
	uint8_t mbs;
	int8_t parity;			/* of its coded blocks' set */
	uint8_t reserved[29];
} AvcencDeblockRow;

/* a row's levels in the output (64 bytes a row & 15, at the job record's command[3]) */
typedef struct AvcencRowLevels {
	uint32_t next;			/* where the next 4x4 block's levels go */
	uint32_t used;			/* the bytes written in the row */
	uint8_t reserved[56];
} AvcencRowLevels;

/* a row's chroma levels in the output (64 bytes a row & 15, at the job record's command[5]) */
typedef struct AvcencChromaRowLevels {
	uint32_t next[2];		/* Cb's, Cr's: where the next block's go */
	uint32_t used[2];		/* the bytes written in the row */
	uint8_t reserved[48];
} AvcencChromaRowLevels;

/*
 * The chunk stages' per core info (192 bytes, in the output info): the
 * deblocking's (the first stage) and the macroblock coding's (the second)
 */
#define CHUNK_INFO_SIZE	192
typedef struct AvcencChunkInfo {
	uint8_t column;			/* the macroblock's */
	uint8_t row;
	uint8_t qp;			/* the picture's */
	uint8_t chromaQp;
	uint32_t type;			/* the macroblock's (AvcencMacroblock) */
	uint16_t leftBlocks;		/* its AvcencLumaCoded's */
	uint16_t topBlocks;
	uint32_t sliceType;
	uint32_t strengths;		/* the macroblock's AvcencStrengths */
	uint32_t lumaCoded;		/* its AvcencLumaCoded */
	uint32_t chromaCoded;
	uint32_t current;		/* the macroblocks' records (AvcencMacroblock) */
	uint32_t left;
	uint32_t above;
	uint32_t macroblock;		/* its VideoAvcMacroblock in the output */
	uint32_t intraRounding;		/* the quantisation's f, 2^qbits / 3 */
	uint32_t interRounding;		/* 2^qbits / 6 */
	uint32_t quantScale;		/* its MF by QP % 6 (avcencQuantScale) */
	uint32_t levelScale;		/* the dequantisation's (avcdecLevelScale) */
	uint8_t quantShift;		/* qbits */
	int8_t levelShift;		/* QP / 6 */
	uint8_t firstMb;		/* the chunk's */
	uint8_t mbs;
	uint32_t reserved2;
	uint32_t neighbours;		/* the macroblock's column's AvcEdges (64 bytes, the chunk's
					   and the one after it at the job record's command[1]) */
	uint32_t chromaNeighbours;	/* its AvcChromaEdges (the job record's command[2]) */
	uint32_t deblockRow;		/* the row's AvcencDeblockRow */
	uint32_t rowLevels;		/* the row's AvcencRowLevels */
	uint32_t chromaRowLevels;	/* its AvcencChromaRowLevels */
	uint8_t reserved4[8];
	uint32_t levels;		/* the output's levels the chunk writes, opened */
	uint32_t chromaLevels[2];	/* Cb's, Cr's */
	uint32_t macroblocks;		/* the output's VideoAvcMacroblock records, the row's */
	uint32_t levelsSize;
	uint32_t chromaLevelsSize[2];
	int8_t hasLeft;			/* the macroblock's neighbours in the slice */
	int8_t hasAbove;
	int8_t hasAboveRight;
	int8_t hasAboveLeft;
	uint32_t coded;			/* the macroblock's coded blocks (VideoAvcMacroblock's) */
	uint32_t luma;			/* the macroblock's samples */
	uint32_t chroma;
	uint32_t lumaAbove;		/* the macroblock's above */
	uint32_t chromaAbove;
	uint16_t width;			/* the picture's, in samples */
	uint16_t chromaWidth;
	uint16_t height;
	uint16_t chromaHeight;
	uint16_t pitch;			/* the luma's */
	uint16_t chromaPitch;
	uint32_t reference;		/* the previous picture's luma */
	uint32_t chromaReference;
	uint32_t source;		/* the client's luma, the macroblock's */
	uint32_t chromaSource;
	int8_t jobsDone;		/* the picture's (the third stage sets it) */
	uint8_t reserved11[15];
} AvcencChunkInfo;

/* the quantisation's MF (avcencstream.c) */
extern const int16_t avcencQuantScale[6][2][4];

/* the macroblocks' bS (avcencmacroblock.c) */
void avcencStrengthsIntra(AvcencChunkInfo *info);
void avcencStrengthsInter(AvcencChunkInfo *info);


typedef char jobInfoSizeCheck[sizeof(AvcencJobInfo) == 64 ? 1 : -1];
typedef char loadSizeCheck[sizeof(AvcencLoad) == 64 ? 1 : -1];
typedef char decideInfoSizeCheck[sizeof(AvcencDecideInfo) == DECIDE_INFO_SIZE ? 1 : -1];
typedef char searchInfoSizeCheck[sizeof(AvcencSearchInfo) == CORE_INFO_SIZE ? 1 : -1];
typedef char buffersSizeCheck[sizeof(AvcencBuffers) == BUFFERS_SIZE ? 1 : -1];
typedef char slicesSizeCheck[sizeof(AvcencSlices) == SLICES_SIZE ? 1 : -1];
typedef char rateSizeCheck[sizeof(AvcencRate) == RATE_STATE_SIZE ? 1 : -1];
typedef char interRecordSizeCheck[sizeof(AvcencInterRecord) == 64 ? 1 : -1];
typedef char motionInfoSizeCheck[sizeof(AvcencMotionInfo) == MOTION_INFO_SIZE ? 1 : -1];
typedef char motionStatsSizeCheck[sizeof(AvcencMotionStats) == 64 ? 1 : -1];
typedef char refineInfoSizeCheck[sizeof(AvcencRefineInfo) == REFINE_INFO_SIZE ? 1 : -1];
typedef char macroblockSizeCheck[sizeof(AvcencMacroblock) == 64 ? 1 : -1];
typedef char recordSizeCheck[sizeof(AvcencRecord) == RECORD_SIZE ? 1 : -1];
typedef char strengthsSizeCheck[sizeof(AvcencStrengths) == 32 ? 1 : -1];
typedef char lumaCodedSizeCheck[sizeof(AvcencLumaCoded) == 8 ? 1 : -1];
typedef char chromaCodedSizeCheck[sizeof(AvcencChromaCoded) == 8 ? 1 : -1];
typedef char deblockRowSizeCheck[sizeof(AvcencDeblockRow) == 128 ? 1 : -1];
typedef char rowLevelsSizeCheck[sizeof(AvcencRowLevels) == 64 ? 1 : -1];
typedef char chunkInfoSizeCheck[sizeof(AvcencChunkInfo) == CHUNK_INFO_SIZE ? 1 : -1];
typedef char streamSizeCheck[sizeof(AvcencStream) == STREAM_HEADER_SIZE ? 1 : -1];

#endif
