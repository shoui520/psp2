/*
 * PU1: the picture decoder, task 0. PU0 parses the stream and passes PU1 the slices
 * of each picture; PU1 reconstructs the picture with the decoder's picture units,
 * keeps the reference pictures, and hands the pictures to the output in display
 * order (output.h).
 */
#pragma once

#include "frame.h"
#include "h264.h"
#include "output.h"
#include "stream.h"
#include "units.h"

/*
 * The decoder: the rings and frame stores the ARM side gives it at open, its work
 * memory, and the codec that runs on them.
 */
#define VIP_DECODER_RINGS	5
#define VIP_DECODER_FRAMES	38	/* two sets of VIP_FRAME_SET */
#define VIP_FRAME_SET		19

/* VipDecoder.memoryMode: how much work memory the stream has */
#define VIP_MEMORY_STANDARD	0	/* VIP_STREAM_KIND_STANDARD */
#define VIP_MEMORY_SIZED	1	/* what queryWorkMemory said, or a large kind's minimum */
#define VIP_MEMORY_DOUBLE	2	/* twice that: each reset takes the other set of frames */

/* VipDecoderArea.streamSettings' kinds, and the work memory each needs */
#define VIP_STREAM_KIND_STANDARD	4
#define VIP_STREAM_KIND_LARGE		7
#define VIP_MEMORY_STANDARD_SIZE	0x1680000
#define VIP_MEMORY_LARGE_SIZE		0x19a0000
#define VIP_MEMORY_DOUBLE_SIZE		0x2d00000

/* what the codec's decode returns (provisional names) */
#define VIP_DECODE_CONTINUE	0
#define VIP_DECODE_STOPPED	1
#define VIP_DECODE_LAST_PICTURE	2
#define VIP_DECODE_ERROR	3

/* the area's settings when the decoder opened (decoder.c), which decode reads */
typedef struct VipOpenSettings {
	uint32_t streamSettings;	/* VipDecoderArea.streamSettings */
	uint32_t outputMode;		/* VIP_OUTPUT_* */
	uint32_t userData;		/* VIP_USER_DATA_*, taken again each decode */
	uint32_t concealColour;		/* VipDecoderArea.concealColour */
	uint32_t recoverySettings;	/* VIP_RECOVERY_* */
	uint16_t options;
	uint16_t optionTaken;		/* VIP_OPTION_TAKEN_SHIFT */
} VipOpenSettings;

typedef struct VipDecoder VipDecoder;

typedef int (*VipCodecFunction)(VipDecoder *decoder, uint32_t point, VipOutputState *output);
typedef int (*VipDecodeFunction)(VipDecoder *decoder, uint32_t point, VipOutputState *output,
	VipOpenSettings *settings);

/* the codec's entry points (setupCodec) */
typedef struct VipCodec {
	VipCodecFunction open;
	VipDecodeFunction decode;	/* point -1: run without a debug point */
	VipCodecFunction close;
} VipCodec;

struct VipDecoder {
	VipRingHandle rings[VIP_DECODER_RINGS];
	VipFrameHandle frames[VIP_DECODER_FRAMES];
	uint32_t workMemory;		/* DRAM */
	uint8_t memoryMode;		/* VIP_MEMORY_* */
	VipCodec codec;
};

/* what queryWorkMemory worked out, for the open that follows */
typedef struct VipMemoryQuery {
	uint32_t width;			/* in pixels, a multiple of 16 */
	uint32_t height;
	uint32_t frames;		/* the frame stores: the references, plus 3 */
	uint32_t size;
	uint32_t valid;
} VipMemoryQuery;

extern VipMemoryQuery g_memoryQuery;


extern VipSps g_sps[VIP_SPS_COUNT];
extern VipPps g_pps[VIP_PPS_COUNT];
extern VipSps *g_currentSps;
extern VipPps *g_currentPps;

#define VIP_PICTURE_BUFFERS	VIP_DECODER_FRAMES

extern VipPictureBuffer g_buffers[VIP_PICTURE_BUFFERS];
extern uint32_t g_motionData[VIP_FRAME_SET];
extern uint8_t g_bufferCount;		/* in a set */
extern int8_t g_frameSet;		/* the set the stream decodes into */
extern uint32_t g_workBase;		/* DRAM: the work memory, as the units address it */

/* the decoded picture buffer (provisional names) */
extern VipPictureBuffer *g_dpbByPoc[VIP_PICTURE_BUFFERS];
extern VipPictureBuffer *g_dpbByDecode[VIP_PICTURE_BUFFERS];
extern uint32_t g_dpbCount;
extern uint32_t g_refListCount;
extern uint32_t g_referenceFrameCount;
extern uint32_t g_list0Count;
extern uint32_t g_list1Count;
extern uint32_t g_generation;
extern int8_t g_maxLongTermFrameIdx;
extern uint8_t g_motionOwners[VIP_FRAME_SET - 1];
extern VipPictureBuffer *g_currentBuffer;
extern VipPictureBuffer *g_lastOutput;
extern int8_t g_noFreeBuffer;

/* the stream's state */
extern int8_t g_layoutDone;
extern int8_t g_secondFieldPending;
extern uint32_t g_layoutWidthMbs;
extern uint32_t g_layoutHeightMbs;
extern int8_t g_randomAccess;
extern uint32_t g_recoveryFrameCount;
extern int8_t g_userDataTaken;
extern int8_t g_uuidDataTaken;
extern int8_t g_bufferingPeriodTaken;
extern int8_t g_recoveryPointTaken;
extern int8_t g_pictureTimingTaken;
extern int8_t g_pictureHeld;
extern int8_t g_spsTaken;
extern int8_t g_ppsTaken;
extern int8_t g_levelAtLeast3;
extern uint8_t g_picStruct;
extern uint8_t g_lastPicStruct;
extern uint8_t g_ctType;
extern int8_t g_timeValid;
extern int8_t g_drainState;
extern int8_t g_extensionState;
extern uint8_t g_lastBottomField;
extern int8_t g_retryCount;

/* g_drainState: the pictures are shown out before the decode goes on */
#define VIP_DRAIN_NONE		0
#define VIP_DRAIN_RESTART	1	/* the stream starts over: show the rest, then stop */
#define VIP_DRAIN_LAYOUT	2	/* a new size: until every frame came back */

#define VIP_NO_FIELD		10	/* g_lastBottomField after a frame */
extern uint8_t g_lastFieldMacroblock;	/* VIP_CB_FIELD_MACROBLOCK at the slice's end */


/* what follows the picture, seen ahead in the records (lookahead.c) */
extern int8_t g_nextTimingSeen;		/* the other field's pic_timing() */
extern uint32_t g_nextOutputTime;	/* its cpb_removal_delay + dpb_output_delay */
extern int8_t g_otherFieldRandomAccess;	/* the other field is IDR, or after a recovery point */

int lookAhead(int16_t *last, int16_t *firstFieldLast, uint32_t *end);

/* the picture order count (order.c) */
extern uint32_t g_prevFrameNum;
extern uint32_t g_frameNumOffset;
extern int32_t g_prevPicOrderCntMsb;
extern uint32_t g_prevPicOrderCntLsb;
extern uint16_t g_otherFieldOrderCnt;

int32_t picOrderCnt(const VipPictureBuffer *buffer);
int32_t oppositePicOrderCnt(const VipPictureBuffer *buffer);
void updatePicOrderCnt(int32_t *top, int32_t *bottom);
void peekPicOrderCnt(int32_t *top, int32_t *bottom);
int32_t currentPicOrderCnt(void);
int32_t frameNumWrap(uint32_t frameNum);

/* the reference lists (lists.c) */
void initFrameRefList(void);
void initFrameLists(void);
void initFieldRefList(void);
void initFieldLists(void);

/* the decoded picture buffer (dpb.c) */
#define VIP_OUTPUT_NO_PTS	0x10000	/* VipPictureBuffer.outputFlags */

extern uint32_t g_outputCount;

void outputPicture(VipPictureBuffer *buffer);
void outputAllPictures(void);
int markReference(void);
void bumpPictures(void);
void insertCurrentPicture(int32_t order);
void updatePicNums(void);
void fillMacroblocks(uint32_t count, uint32_t colour);

/* decoding a slice's macroblocks (macroblocks.c) */
extern uint8_t g_widthMbsMinus1;
extern uint16_t g_sliceMbAddress;
extern uint8_t g_neighbourX;		/* the next row above read in */
extern uint8_t g_neighbourY;
extern uint8_t g_mbX;			/* the next macroblock written out */
extern uint8_t g_mbY;
extern uint8_t g_memoryMode;
extern int8_t g_colocatedPreloaded;
extern int8_t g_motionSave;
extern int8_t g_standardPicture;
extern uint32_t g_motionSize;
extern void (*g_directMacroblock)(void);
extern volatile uint32_t g_stageDone[2];
extern volatile uint32_t g_colocatedDone;
extern uint16_t g_predictionCount;
extern uint16_t g_fieldHistory;		/* MBAFF: bit n, the macroblock at stage n + 1 */
extern uint16_t g_macroblockCount;
extern uint8_t g_pairState;
extern uint8_t g_colocatedColumn;
extern uint8_t g_colocatedField;
extern VipMotionBuffer g_motionBuffer;
extern VipDmaDescriptor g_motionList[VIP_MOTION_RECORDS];
extern VipDmaDescriptor g_colocatedList;	/* by VIP_COLOCATED_DMA */
extern uint32_t g_colocatedMacroblock[VIP_MOTION_MACROBLOCK_WORDS];
extern uint8_t g_colocatedX;
extern uint8_t g_colocatedY;
extern uint32_t g_sliceParameters[3];

int decodeSlice(uint32_t data);

/* the macroblock's slot in the units' RAM; the colocated motion to the units (direct.c) */
uint32_t macroblockSlot(void);
void pushColocatedMotion(void);

/* the picture start (picture.c) */
extern uint32_t g_streamSettings;	/* VipOpenSettings.streamSettings, each decode */
extern uint32_t g_currPicNum;		/* CurrPicNum */
extern uint32_t g_maxPicNum;		/* MaxPicNum */
extern uint8_t g_entropyCommand;
extern uint8_t g_lastMbRow;
extern uint8_t g_mbaffLastX;
extern int8_t g_heightMapUnitsMinus1;

/* g_streamSettings' bits the picture start looks at (provisional names) */
#define VIP_STREAM_WIDE_LOW_LEVEL	0x10000	/* wide pictures below level 3.1 decode as SD */
#define VIP_STREAM_REPORT_WIDE		0x20000	/* tell the debugger of one that does not */

/* VipPictureBuffer.outputFlags */
#define VIP_OUTPUT_TOO_WIDE	0x00001	/* wider than its level's SD loop takes */

void startPicture(uint32_t memoryMode);

/*
 * Task 0's other code overlays, as the linker places them (fw1.ld): the macroblock
 * loop's three at runMacroblocks, the motion saving's two at saveMacroblockMotion.
 */
extern char __loop_sd_load[], __loop_sd_size[];
extern char __loop_hd_load[], __loop_hd_size[];
extern char __loop_mbaff_load[], __loop_mbaff_size[];
extern char __motion_inferred_load[], __motion_inferred_size[];
extern char __motion_all_load[], __motion_all_size[];

void saveMacroblockMotion(void);

/* the stream's clock (timing.c) */

int takePictureTime(uint32_t option, uint32_t flags, uint32_t ptsLow, uint32_t dtsLow,
	const VipTime *fallback);
void rescaleClock(uint32_t oldScale, uint32_t newScale);

/* the picture as it starts (picture.c) */
void updateFrameInfo(void);
int allocatePictureBuffer(void);

/*
 * the macroblock loop: task 0's code overlay, by g_standardPicture or MBAFF
 * (loop.c)
 */
int runMacroblocks(void);
int runSdLoop(void);
int runHdLoop(void);
int runMbaffLoop(void);
void saveInferredMotion(void);
void saveAllMotion(void);

/* a B slice's direct prediction, by how the colocated picture was coded (direct.c) */
void startFrameDirect(void);
void frameDirectMacroblock(void);
void startFrameFromFieldsDirect(void);
void frameFromFieldsDirectMacroblock(void);
void startFieldFromFrameDirect(void);
void fieldFromFrameDirectMacroblock(void);
void startFieldDirect(void);
void fieldDirectMacroblock(void);
void startFieldFromMbaffDirect(void);
void fieldFromMbaffDirectMacroblock(void);
void startMbaffFromFrameDirect(void);
void mbaffFromFrameDirectMacroblock(void);
void startMbaffFromFieldsDirect(void);
void mbaffFromFieldsDirectMacroblock(void);

/* the slice being decoded (slice.c) */
extern uint16_t g_pictureMbs;
extern uint8_t g_sliceType;
extern uint8_t g_fieldPic;
extern uint8_t g_bottomField;
extern uint8_t g_implicitWeights;
extern int8_t g_mbaffFrame;		/* MbaffFrameFlag */

extern uint32_t g_sliceSetup;
extern int8_t g_needsLists;
extern int8_t g_listModified[2];
extern volatile uint32_t g_listsDone;

int takePictureHeader(uint32_t mode, uint32_t recoveryAtI, uint32_t mustDrain);
int takeSliceHeader(void);

/* the stream's clock (timing.c; provisional names) */
extern uint64_t g_periodTime;
extern uint64_t g_pendingPeriodTime;
extern uint64_t g_pictureTime;
extern uint64_t g_timeFraction;
extern uint64_t g_periodTimeFraction;

#define VIP_ERROR_LOG		16

extern uint32_t g_errorLog[VIP_ERROR_LOG];
extern uint32_t g_errorCount;
extern uint32_t g_underrunCount;
extern uint32_t g_pictureCount;

#define VIP_CODEC_COUNTERS \
	((volatile VipCodecCounters *)(VIP_STATISTICS_ADDRESS + 0x600))

/* Note an error in the log. */
static inline void logError(uint32_t error)
{
	g_errorLog[g_errorCount++ & (VIP_ERROR_LOG - 1)] = error;
}

/*
 * The codec's DMA lists in task 0's data: a slice's tables to the units, or a fill
 * and the zeros it repeats.
 */
typedef union VipCodecDma {
	struct {
		VipDmaDescriptor weights;
		VipDmaDescriptor lists[2];	/* list 0's changes, list 1's */
	} slice;
	struct {
		VipDmaDescriptor descriptor;
		uint32_t zeros[2];
	} clear;
	VipDmaDescriptor scaling[VIP_SCALING_LISTS];	/* the scaling lists to the units */
	struct {
		uint32_t pattern[2];		/* what a VIP_DMA_FILL repeats */
		VipDmaDescriptor targets[4];
	} fill;
	/* the macroblock loop's writes; startPicture sets their local side */
	VipDmaDescriptor loop[VIP_LOOP_DESCRIPTORS];
} VipCodecDma;

extern VipCodecDma g_codecDma;

extern VipRingHandle *g_ringHandles[VIP_DECODER_RINGS];
extern VipFrameHandle *g_frameHandles[VIP_DECODER_FRAMES];

/* Drop all that PU0 parsed and PU1 has not read: the records, then their data. */
static inline void dropParsedInput(void)
{
	while (ringSkipToWriter(g_ringHandles[VIP_RING_RECORDS]))
		;
	while (ringSkipToWriter(g_ringHandles[VIP_RING_SLICES]))
		;
}

/* the records PU0 parsed, read in a few at a time (decode.c) */
#define VIP_RECORDS_READ	4

extern VipRecord g_recordBuffer[VIP_RECORDS_READ];

int readRecords(uint32_t *offset, uint32_t *segment);
uint32_t beginDecode(VipDecoder *decoder);

/* the display information the next picture gets (setFrameInfo), and its extension */
extern VipFrameInfo g_frameInfo;
extern VipFrameExtension g_frameExtension;
extern int8_t g_timeStarted;		/* a picture's time was worked out since the SPS */

extern VipSliceHeader g_sliceHeader;

void readParsed(void);
void takeSps(void);
void takePps(void);

/*
 * Where the SEI user data goes (sei.c): the user data rings' writable space when
 * the decode began, and how much of it is used since.
 */
typedef struct VipUserDataRings {
	VipRingSegment data[2];		/* VIP_RING_USER_DATA's */
	VipRingSegment records[2];	/* VIP_RING_USER_RECORDS' */
	uint8_t dataCount;
	uint8_t recordCount;
	uint32_t dataCursor;
	uint32_t recordCursor;
	uint32_t dataUsed;
	uint32_t recordsUsed;
} VipUserDataRings;

extern VipUserDataRings g_userData;
extern uint32_t g_cpbRemovalDelay;	/* the last pic_timing()'s */
extern uint32_t g_dpbOutputDelay;

/* the VUI and SEI records (sei.c) */
void takeVui(void);
void takePictureTiming(void);
void takePanScan(void);
void takeBufferingPeriod(void);
void takeRecoveryPoint(uint32_t ignoreBrokenLink);
int takeUserData(uint32_t type, uint32_t bits, uint32_t flags, uint32_t pts);

/* the extensions (extension.c) */
void keepPictureTiming(void);
void nextPictureTiming(void);
void keepPanScan(void);
void nextPanScan(void);
void keepBufferingPeriod(void);
void keepRecoveryPoint(void);
void copyVui(VipFrameExtension *extension, const VipSps *sps);
int setFrameBufferingPeriodCount(VipFrameHandle *handle, uint8_t count);
int setFramePictureTimingCount(VipFrameHandle *handle, uint8_t count);
int setFrameRecoveryPointCount(VipFrameHandle *handle, uint8_t count);
int setFramePanScanCount(VipFrameHandle *handle, uint8_t count);
int setFrameVuiPresent(VipFrameHandle *handle, uint8_t present);
int setFrameBufferingPeriod(VipFrameHandle *handle, const uint32_t *words);
int setFramePictureTiming(VipFrameHandle *handle, const uint32_t *words);
int setFrameRecoveryPoint(VipFrameHandle *handle, const uint32_t *words);
int setFramePanScan(VipFrameHandle *handle, const uint32_t *words);
int setFrameVui(VipFrameHandle *handle, const uint32_t *words);
int setFrameFieldState(VipFrameHandle *handle);
void publishExtension(VipFrameHandle *frame, VipDecoderArea *area,
	VipFrameExtension *extension);
void clearUnpairedField(VipFrameHandle *handle);

/* the codec (codec.c) */
int decode(VipDecoder *decoder, uint32_t point, VipOutputState *output,
	VipOpenSettings *settings);
void setupCodec(VipCodec *codec);
int initializeUnits(void);
void resetDecoding(uint32_t mode);
void flushPictures(void);
void clearUnitState(void);
void resetUnit0(void);
void resetUnits(void);
void clearByDma(uint32_t control, uint32_t external, uint32_t localLength);
void clearExtension(VipFrameExtension *extension);

/* the work memory (layout.c) */
int layoutFrames(uint32_t mode);
int layoutQueried(uint32_t width, uint32_t height);
int queryWorkMemory(int width, int height, uint32_t references, uint32_t *size);
uint32_t memoryQueryValid(void);
void clearMemoryQuery(void);

void enableDecoderClocks(void);
void gateDecoderClocks(void);
void setupInterrupt(uint32_t interrupt);
