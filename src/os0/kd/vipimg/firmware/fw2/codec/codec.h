/*
 * FW2's codec, core 1's program: fw1's PU1 picture decoder (fw1/pu1) made to
 * serve one context per command. A run command reads the records parser0 or
 * parser1 wrote (VIP_PARSED_*) and the parsed stream they point into, decodes the
 * pictures with the picture units into the frame stores the ARM side gave at
 * open, and hands them over in output order. The SEI messages and VUI that come
 * with a picture go into its frame store's extension; user data SEI goes to a
 * DRAM area the open names.
 *
 * Between commands its state lives in the context: the $gp globals (most of it)
 * and the saved local RAM data (VIP_CODEC_SAVED). The parameter sets are past
 * $gp's reach (VIP_GLOBAL_FAR).
 */
#pragma once

#include "../common/runtime.h"
#include "../common/ring.h"
#include "../common/frame.h"
#include "../../fw1/common/dma.h"
#include "../../fw1/pu1/h264.h"
#include "../../fw1/pu1/units.h"

/* where the codec's code and data go (fw2/codec.ld) */
#define VIP_CODEC_SAVED		__attribute__((section(".saved.program")))
#define VIP_CODEC_LOCAL		__attribute__((section(".local.program")))
#define VIP_CODEC_OVERLAY	__attribute__((section(".overlay.program"), noinline))

/* the codec's commands (VipCommand.kind) besides the parsers' (fw2/parser) */
#define VIP_CODEC_SET_OPTION	4
#define VIP_CODEC_QUERY		5	/* the work memory a size needs */
#define VIP_CODEC_OPEN		0x10000
#define VIP_CODEC_RUN		0x20000
#define VIP_CODEC_CLOSE		0x40000

/* VipCodecGlobals.mode: the commands it takes */
#define VIP_CODEC_CLOSED	0	/* VIP_CODEC_OPEN, _SET_OPTION, _QUERY */
#define VIP_CODEC_OPENED	1	/* VIP_CODEC_RUN */
#define VIP_CODEC_ENDED		2	/* VIP_CODEC_CLOSE: the stream has ended */

/* a command's results besides VIP_RESULT_OK and VIP_PROGRAM_CONTINUE */
#define VIP_CODEC_FAILED	-2	/* the open failed: response[4] says why */
#define VIP_CODEC_WRONG_MODE	-3
#define VIP_CODEC_NO_COMMAND	-5
#define VIP_CODEC_BAD_MODE	-8
#define VIP_CODEC_HELD		-11	/* no frame store came free */
#define VIP_CODEC_NO_INPUT	-12

/* VIP_CODEC_RUN's arguments: the third says which pictures are decoded */
#define VIP_CODEC_RUN_ALL		0
#define VIP_CODEC_RUN_REFERENCES_ONLY	1	/* the others' slices are skipped */

/* the events the codec raises (vipNotifyEvents) */
#define VIP_CODEC_EVENT_PICTURE		0x8000	/* a wide low-level picture was not SD (startPicture) */
#define VIP_CODEC_EVENT_NEW_LAYOUT	0x10000	/* a new size: the old pictures go first */
#define VIP_CODEC_EVENT_RESTART		0x20000	/* the pictures were dropped */
#define VIP_CODEC_EVENT_MARKING		0x40000	/* the reference marking failed */
#define VIP_CODEC_EVENT_NOT_PAIRED	0x80000	/* a field without its pair */
#define VIP_CODEC_EVENT_SLICE_ERROR	0x100000
#define VIP_CODEC_EVENT_HEADER_ERROR	0x200000
#define VIP_CODEC_EVENT_PICTURE_ERROR	0x400000
#define VIP_CODEC_EVENT_PPS_ERROR	0x800000
#define VIP_CODEC_EVENT_SPS_ERROR	0x1000000

/* what the codec traces (vipTraceEvent), with g_pictureCount */
#define VIP_CODEC_TRACE_RESTART		0xf0000000
#define VIP_CODEC_TRACE_BAD_PICTURE	0xf1000000	/* takePictureHeader failed */
#define VIP_CODEC_TRACE_UNKNOWN_RECORD	0xf2000000
#define VIP_CODEC_TRACE_MARKING		0xf3000000
#define VIP_CODEC_TRACE_NO_BUFFER	0xf4000000
#define VIP_CODEC_TRACE_BUFFER_TIMEOUT	0xf5000000
#define VIP_CODEC_TRACE_NOT_PAIRED	0xf6000000
#define VIP_CODEC_TRACE_NO_PICTURE	0xf7000000	/* a picture ends that did not start */
#define VIP_CODEC_TRACE_LAID_OUT	0xf8000000
#define VIP_CODEC_TRACE_NO_DATA		0xffff0000	/* | 0 SPS, 1 PPS, 2 slice header */

/*
 * The context's globals the ARM side reads, at its start: the mode, fw1's
 * counters, and the options (VIP_CODEC_SET_OPTION).
 */
typedef struct VipCodecGlobals {
	uint32_t mode;			/* VIP_CODEC_* */
	VipCodecCounters counters;
	uint8_t options;		/* VIP_CODEC_OPTION_* */
} VipCodecGlobals;

extern VipCodecGlobals g_codec VIP_GLOBAL;

/* VipCodecGlobals.options: the option byte, and what the codec sets */
#define VIP_CODEC_OPTION_NEED_INPUT	0x04	/* refuse a run with nothing to decode */
#define VIP_CODEC_OPTION_ON_TIME	0x08	/* output by the pictures' times */
#define VIP_CODEC_OPTION_PICTURE	0x10	/* startPicture's: wide low-level pictures are SD */
#define VIP_CODEC_OPTION_BROKEN_LINK	0x20	/* takeRecoveryPoint's: ignore broken_link_flag */
#define VIP_CODEC_OPTION_MODE_SHIFT	6	/* 2 bits: takePictureHeader's mode */
#define VIP_CODEC_MODE_RECOVERY_AT_I	0x1	/* an I picture may start the decoding */
#define VIP_CODEC_MODE_BASELINE		0x2	/* Baseline profile streams too */
#define VIP_CODEC_OPTIONS_SET		0xdc	/* the ones VIP_CODEC_SET_OPTION sets */
#define VIP_CODEC_OPTIONS_CLOSED	0xe0	/* at the start, and after a close */

/*
 * The decoder the open sets up: the rings and frame stores the ARM side gives,
 * and its work memory: one block the codec lays the pictures out in, or an area
 * per frame store, where the ARM side put its picture and motion data.
 */
#define VIP_CODEC_RINGS		5	/* fw1's VIP_RING_*; the user data rings are not used */
#define VIP_CODEC_RING_HANDLES	3	/* the rings it opens: VIP_RING_SLICES to _OUTPUT */
#define VIP_CODEC_FRAMES	19
#define VIP_CODEC_AREAS		19

typedef struct VipWorkArea {
	uint32_t picture;		/* DRAM */
	uint32_t motion;		/* its motion data; with one area, the picture's address */
} VipWorkArea;

typedef struct VipCodecDecoder {
	VipRingHandle rings[VIP_CODEC_RINGS];
	VipFrameHandle frames[VIP_CODEC_FRAMES];
	uint32_t workMemory;		/* the block, or VipWorkArea *: the areas */
	uint8_t workKind;		/* VIP_WORK_* */
	uint8_t areaCount;
	uint32_t workSize;		/* the block's, or the area list's */
	VipWorkArea areas[VIP_CODEC_AREAS];
} VipCodecDecoder;

/* VipCodecDecoder.workKind */
#define VIP_WORK_BLOCK		4	/* one block, its size a multiple of 8 KB */
#define VIP_WORK_AREAS		6	/* an area per frame store */

extern VipCodecDecoder g_decoder VIP_GLOBAL;

/* the open's arguments: rings 0-2, the frame stores, the work memory, user data */
typedef struct VipCodecOpen {
	VipRing *input;			/* the parsed stream */
	VipRing *records;		/* its records */
	VipRing *output;		/* written */
	VipFrame *frames;		/* VIP_CODEC_FRAMES of them */
	uint32_t workMemory;
	uint32_t workSize;		/* of the block, or the list */
	uint32_t areaCount;
	uint32_t userData;		/* DRAM: the user data area, or 0 */
	uint32_t userDataSlot;		/* the size of each field's */
} VipCodecOpen;

/*
 * The SEI messages a field had, as PU0 parsed them (each into its frame store's
 * extension), and VipFrameExtension.seiPresent: a bit each, shifted by the field.
 */
#define VIP_USER_DATA_MESSAGES		15

typedef struct VipSeiMessages {
	uint32_t bufferingPeriod[0x208 / 4];
	uint32_t pictureTiming[0x30 / 4];
	uint32_t panScan[0x40 / 4];
	uint32_t recoveryPoint[2];
	uint32_t framePacking[4];
	uint32_t userData;		/* its size in 8 bytes; VIP_USER_DATA_* */
	uint32_t userDataMessages[VIP_USER_DATA_MESSAGES];	/* registered << 30 | bytes */
} VipSeiMessages;

_Static_assert(sizeof(VipSeiMessages) == VIP_EXTENSION_SEI_SIZE, "VipSeiMessages");

#define VIP_SEI_VUI			0x0002
#define VIP_SEI_BUFFERING_PERIOD	0x0004
#define VIP_SEI_PICTURE_TIMING		0x0010
#define VIP_SEI_PAN_SCAN		0x0040
#define VIP_SEI_RECOVERY_POINT		0x0100
#define VIP_SEI_FRAME_PACKING		0x0400
#define VIP_SEI_USER_DATA		0x1000
#define VIP_SEI_FIRST_FIELDS		0x0aa8	/* each kind's bit for the second field */
#define VIP_SEI_WIDE_LOW_LEVEL		0x80000000	/* wider than 76 macroblocks below level 3.1 */
#define VIP_USER_DATA_SIZE		0x3ffffff	/* VipSeiMessages.userData */

extern VipSeiMessages g_sei VIP_GLOBAL;
extern uint32_t g_seiPresent VIP_GLOBAL;
extern uint32_t g_userDataArea VIP_GLOBAL;	/* DRAM; slot 0 collects the picture's */
extern uint32_t g_userDataSlot VIP_GLOBAL;
extern uint8_t g_vui[VIP_SPS_COUNT][VIP_EXTENSION_VUI_SIZE] VIP_GLOBAL;	/* each SPS's */
extern VipFrameExtension g_extensions[VIP_CODEC_FRAMES] VIP_GLOBAL;

/* the parameter sets, past $gp's reach: placeParameterSets points at them */
extern VipSps *VIP_GLOBAL g_sps;
extern VipPps *VIP_GLOBAL g_pps;
extern VipSps *VIP_GLOBAL g_currentSps;
extern VipPps *VIP_GLOBAL g_currentPps;

/* the decoded picture buffer */
extern VipPictureBuffer *g_dpbByPoc[VIP_CODEC_FRAMES] VIP_GLOBAL;
extern VipPictureBuffer *g_dpbByDecode[VIP_CODEC_FRAMES] VIP_GLOBAL;
extern uint32_t g_dpbCount VIP_GLOBAL;
extern uint32_t g_referenceFrameCount VIP_GLOBAL;
extern uint8_t g_bufferCount VIP_GLOBAL;
extern uint32_t g_generation VIP_GLOBAL;
extern VipPictureBuffer *g_currentBuffer VIP_CODEC_SAVED;

/* the stream's state */
extern int8_t g_layoutDone VIP_GLOBAL;		/* the frame stores are laid out */
extern int8_t g_randomAccess VIP_GLOBAL;	/* RANDOM_ACCESS_* */
extern uint8_t g_lastBottomField VIP_GLOBAL;	/* VIP_NO_FIELD after a frame */
extern int8_t g_skipping VIP_GLOBAL;		/* drop records up to a picture's end */
extern uint8_t g_late VIP_GLOBAL;		/* the output is behind the pictures' times */
extern int8_t g_drainState VIP_GLOBAL;		/* VIP_DRAIN_* */
extern int8_t g_secondFieldPending VIP_GLOBAL;
extern int8_t g_bufferingPeriodTaken VIP_GLOBAL;	/* with HRD parameters, since the last picture */
extern int8_t g_recoveryPointTaken VIP_GLOBAL;
extern int8_t g_pictureTimingTaken VIP_GLOBAL;
extern int8_t g_spsTaken VIP_GLOBAL;
extern uint8_t g_picStruct VIP_GLOBAL;		/* pic_struct (Table D-1) */
extern uint8_t g_lastPicStruct VIP_GLOBAL;
extern uint64_t g_outputClock VIP_GLOBAL;	/* 180 kHz: when the next picture is due */
extern uint32_t g_outputClockFraction VIP_GLOBAL;
extern uint64_t g_delimiterTime VIP_GLOBAL;	/* the last delimiter's DTS, or PTS */
extern uint32_t g_untimedDelimiters VIP_GLOBAL;
extern uint8_t g_outputState VIP_GLOBAL;	/* VIP_OUTPUT_* */
extern uint32_t g_outputStartFrame VIP_GLOBAL;	/* the frame_num output starts at */
extern VipFrameInfo g_frameInfo VIP_GLOBAL;
extern uint32_t g_underrunCount VIP_GLOBAL;
extern uint32_t g_pictureCount VIP_GLOBAL;
extern uint32_t g_outputCount VIP_GLOBAL;
extern VipSliceHeader g_sliceHeader VIP_GLOBAL;

/* g_drainState */
#define VIP_DRAIN_NONE		0
#define VIP_DRAIN_RESTART	1	/* the frame stores were laid out again */
#define VIP_DRAIN_LAYOUT	2	/* a new size: until every frame came back */

#define VIP_NO_FIELD		10	/* g_lastBottomField after a frame */

/* g_outputState: where the output starts after a random access */
#define VIP_OUTPUT_WAITING	0	/* for the picture with g_outputStartFrame */
#define VIP_OUTPUT_STARTING	1
#define VIP_OUTPUT_RUNNING	2
#define VIP_OUTPUT_RESUMING	3	/* the next output decides */

/* the pictures' times (timing.c), as fw1 keeps them */
extern uint64_t g_pictureTime VIP_GLOBAL;
extern uint64_t g_periodTime VIP_GLOBAL;	/* the buffering period's start */
extern uint64_t g_pendingPeriodTime VIP_GLOBAL;	/* the next one's, from its SEI */
extern uint64_t g_timeFraction VIP_GLOBAL;
extern uint64_t g_periodTimeFraction VIP_GLOBAL;
extern int8_t g_timeStarted VIP_GLOBAL;	/* a picture's time was worked out since the SPS */

/* the slice ring's readable segments and a run's position in them (local RAM) */
extern VipRingSegment g_sliceSegments[2] VIP_CODEC_SAVED;
extern uint8_t g_sliceSegmentCount VIP_CODEC_LOCAL;
extern uint32_t g_dataStart VIP_CODEC_LOCAL;	/* the record's data in the slice ring */

/* the DMA the slice decoding waits for: the lists' (fw1's slice.c), the colocated motion's */
extern volatile uint32_t g_listsDone VIP_CODEC_LOCAL;
extern volatile uint32_t g_colocatedDone VIP_CODEC_LOCAL;

/* the codec's DMA lists in local RAM (fw1's g_codecDma): the staging copies, or a
   fill and the zeros it repeats */
typedef union VipCodecDma {
	VipDmaDescriptor staging[4];
	struct {
		VipDmaDescriptor weights;
		VipDmaDescriptor lists[2];	/* list 0's changes, list 1's */
	} slice;
	struct {
		VipDmaDescriptor descriptor;
		uint32_t zeros[2];
	} clear;
	VipOutputRecord output;		/* dpb.c outputPicture's */
	VipDmaDescriptor scaling[VIP_SCALING_LISTS];	/* the scaling lists to the units */
	struct {
		uint32_t pattern[2];		/* what a VIP_DMA_FILL repeats */
		VipDmaDescriptor targets[4];
	} fill;
	/* fillMacroblocks' (fw1's g_fillPair, g_fillDescriptor) */
	struct {
		VipDmaDescriptor reserved[6];
		VipDmaDescriptor pair[2];	/* an MBAFF macroblock pair */
		VipDmaDescriptor reserved2[2];
		VipDmaDescriptor macroblock;
	} macroblockFill;
	/* the macroblock loop's writes; startPicture sets their local side */
	VipDmaDescriptor loop[VIP_LOOP_DESCRIPTORS];
} VipCodecDma;

extern VipCodecDma g_codecDma VIP_CODEC_LOCAL;

/* the staging DMA: its lists, and its buffer in local RAM bank 1 */
#define VIP_STAGING_LIST(list)	((uint32_t)&g_codecDma.staging[list] << 16 | sizeof(VipDmaDescriptor))
#define VIP_STAGING		0x400		/* its bank 1 offset */
#define VIP_STAGING_SIZE	0x1e00

/* the records of a run */
#define VIP_RECORD_SIZE		sizeof(VipRecord)

/* command.c */
void enableDecoderClocks(void);
void gateDecoderClocks(void);
void saveParsed(void *destination, uint32_t size);
int handOutPicture(VipPictureBuffer *buffer);
int bumpPictures(void);
int outputAllPictures(void);
void finishPicture(void);

/* layout.c */
extern VipPictureBuffer g_buffers[VIP_CODEC_FRAMES] VIP_GLOBAL;
extern uint32_t g_motionData[VIP_CODEC_FRAMES - 1] VIP_GLOBAL;	/* the frame stores' but one */

uint32_t workMemoryRows(uint32_t widthMbs, uint32_t heightMbs, uint32_t frames, uint32_t limit,
	uint32_t motionRowsPerMbRow);
uint32_t framesFitting(uint32_t widthMbs, uint32_t heightMbs, uint32_t limit,
	uint32_t motionRowsPerMbRow);
int layoutFrames(uint32_t workKind, uint32_t frames);

/* units.c */
extern VipRingHandle *g_ringHandles[VIP_CODEC_RING_HANDLES] VIP_GLOBAL;	/* g_decoder's rings */
extern VipFrameHandle *g_frameHandles[VIP_CODEC_FRAMES] VIP_GLOBAL;	/* and frames */
extern uint32_t g_workBase VIP_GLOBAL;	/* its work memory, as the units address it */
extern uint32_t g_workSize VIP_GLOBAL;
extern uint32_t g_refListCount VIP_GLOBAL;
extern uint32_t g_list0Count VIP_GLOBAL;
extern uint32_t g_list1Count VIP_GLOBAL;
extern int8_t g_maxLongTermFrameIdx VIP_GLOBAL;
extern uint8_t g_motionOwners[VIP_CODEC_FRAMES - 1] VIP_GLOBAL;	/* VIP_MOTION_NO_OWNER */
extern uint8_t g_lastFieldMacroblock VIP_CODEC_LOCAL;

void interrupt5(void) __attribute__((interrupt));
void interrupt8(void) __attribute__((interrupt));
int initializeUnits(void);
void resetDecoding(void);
void flushPictures(void);
void clearByDma(uint32_t control, uint32_t external, uint32_t localLength);
void clearCodecUnitState(void);
void clearUnitState(void);
int openDecoder(VipCodecDecoder *decoder);
int closeDecoder(VipCodecDecoder *decoder);
void resetUnit0(void);
void resetUnits(void);

/* parameters.c (fw1/pu1's, both codecs') */
extern int8_t g_levelAtLeast3 VIP_GLOBAL;

void readParsed(void);
void takeSps(void);
void takePps(void);

/* slice.c: the slice being decoded */
extern uint16_t g_pictureMbs VIP_CODEC_LOCAL;	/* the picture's macroblocks; a field's are half */
extern uint8_t g_sliceType VIP_CODEC_LOCAL;	/* VIP_SLICE_TYPE_* */
extern uint8_t g_fieldPic VIP_CODEC_LOCAL;
extern uint8_t g_bottomField VIP_CODEC_LOCAL;
extern uint32_t g_sliceSetup VIP_CODEC_LOCAL;
extern int8_t g_needsLists VIP_CODEC_LOCAL;	/* the lists' changes went to the units */
extern int8_t g_listModified[2] VIP_CODEC_LOCAL;
extern uint32_t g_layoutWidthMbs VIP_GLOBAL;	/* the picture size the frame stores are for */
extern uint32_t g_layoutHeightMbs VIP_GLOBAL;

int takePictureHeader(uint32_t workKind, uint32_t mode, uint32_t drain, uint32_t areaCount);
int takeSliceHeader(void);

/* macroblocks.c: decoding a slice's macroblocks */
#define VIP_LOOP_ENTROPY_TIMEOUT	(-3)	/* a loop's result: unit 0 did not finish */

extern int8_t g_colocatedPreloaded VIP_CODEC_LOCAL;
extern int8_t g_motionSave VIP_CODEC_LOCAL;	/* VIP_MOTION_SAVE_* */
extern void (*g_directMacroblock)(void) VIP_CODEC_LOCAL;
extern uint16_t g_sliceMbAddress VIP_CODEC_LOCAL;
extern uint8_t g_mbY VIP_CODEC_LOCAL;		/* the next macroblock written out */
extern uint8_t g_mbX VIP_CODEC_LOCAL;
extern uint8_t g_neighbourY VIP_CODEC_LOCAL;	/* the next row above read in */
extern uint8_t g_neighbourX VIP_CODEC_LOCAL;
extern uint32_t g_motionSize VIP_CODEC_LOCAL;
extern VipDmaDescriptor g_streamList[VIP_STREAM_SEGMENTS] VIP_CODEC_LOCAL;
extern VipDmaDescriptor g_motionList[VIP_MOTION_RECORDS] VIP_CODEC_LOCAL;
extern volatile uint32_t g_stageDone[2] VIP_CODEC_LOCAL;
extern uint16_t g_predictionCount VIP_CODEC_LOCAL;
extern uint16_t g_fieldHistory VIP_CODEC_LOCAL;	/* MBAFF: bit n, the macroblock at stage n + 1 */
extern uint16_t g_macroblockCount VIP_CODEC_LOCAL;
extern uint8_t g_pairState VIP_CODEC_LOCAL;
extern uint8_t g_colocatedColumn VIP_CODEC_LOCAL;
extern uint8_t g_colocatedField VIP_CODEC_LOCAL;
extern uint32_t g_colocatedMacroblock[VIP_MOTION_MACROBLOCK_WORDS] VIP_CODEC_LOCAL;
extern VipDmaDescriptor g_colocatedList VIP_CODEC_LOCAL;	/* by VIP_COLOCATED_DMA */
extern VipMotionBuffer g_motionBuffer;
extern uint8_t g_colocatedX VIP_GLOBAL;
extern uint8_t g_colocatedY VIP_GLOBAL;
extern uint32_t g_sliceParameters[3] VIP_GLOBAL;

int decodeSlice(uint32_t data);

/* picture.c: the picture start */
extern uint32_t g_currPicNum VIP_GLOBAL;	/* CurrPicNum */
extern uint32_t g_maxPicNum VIP_GLOBAL;		/* MaxPicNum */
extern int8_t g_standardPicture VIP_CODEC_LOCAL;	/* chooses the SD or HD loop overlay */
extern uint8_t g_entropyCommand VIP_CODEC_LOCAL;
extern int8_t g_mbaffFrame VIP_CODEC_LOCAL;	/* MbaffFrameFlag */
extern uint8_t g_widthMbsMinus1 VIP_CODEC_LOCAL;
extern uint8_t g_mbaffLastX VIP_CODEC_LOCAL;
extern uint8_t g_lastMbRow VIP_CODEC_LOCAL;
extern int8_t g_heightMapUnitsMinus1 VIP_CODEC_LOCAL;

int allocatePictureBuffer(void);
int takeMotionData(void);
int startPicture(uint32_t allowWide);

/*
 * The codec's code overlays, as the linker places them: the macroblock loop's
 * three at runMacroblocks, the motion saving's two at saveMacroblockMotion.
 */
extern char __loop_sd_load[], __loop_sd_size[];
extern char __loop_hd_load[], __loop_hd_size[];
extern char __loop_mbaff_load[], __loop_mbaff_size[];
extern char __motion_inferred_load[], __motion_inferred_size[];
extern char __motion_all_load[], __motion_all_size[];

void saveMacroblockMotion(void);

/*
 * the macroblock loops (fw1/pu1's loop.c): the overlay at runMacroblocks (SD, HD or MBAFF,
 * as startPicture loaded it), and the resident one for small pictures
 */
int runMacroblocks(void);
int runSmallLoop(void);

/* a B slice's direct prediction, by how the colocated picture was coded (fw1/pu1's direct.c) */
uint32_t macroblockSlot(void);
void pushColocatedMotion(void);
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

/* order.c (fw1/pu1's, both codecs') */
extern uint32_t g_prevFrameNum VIP_GLOBAL;
extern uint32_t g_frameNumOffset VIP_GLOBAL;
extern int32_t g_prevPicOrderCntMsb VIP_GLOBAL;
extern uint32_t g_prevPicOrderCntLsb VIP_GLOBAL;

int32_t picOrderCnt(const VipPictureBuffer *buffer);
int32_t oppositePicOrderCnt(const VipPictureBuffer *buffer);
void updatePicOrderCnt(int32_t *top, int32_t *bottom);
int32_t frameNumWrap(uint32_t frameNum);

/* dpb.c (fw1/pu1's, both codecs') */
void outputPicture(VipPictureBuffer *buffer);
int markReference(void);
void insertCurrentPicture(int32_t order);
void fillMacroblocks(uint32_t count, uint32_t colour);
void updatePicNums(void);

/* decode.c */
uint32_t beginDecode(VipCodecDecoder *decoder);
void concealPicture(void);

/* lists.c (fw1/pu1's, both codecs') */
void initFrameRefList(void);
void initFrameLists(void);
void initFieldRefList(void);
void initFieldLists(void);

/* sei.c (fw1/pu1's, both codecs') */
extern uint32_t g_cpbRemovalDelay VIP_GLOBAL;	/* the last pic_timing()'s */
extern uint32_t g_dpbOutputDelay VIP_GLOBAL;
extern uint8_t g_ctType VIP_GLOBAL;		/* VIP_CT_TYPE_* */
extern uint32_t g_recoveryFrameCount VIP_GLOBAL;	/* recovery_frame_cnt */

void resetFrameInfo(void);
void takeVui(void);
void takePictureTiming(void);
void takePanScan(void);
void takeBufferingPeriod(void);
void takeRecoveryPoint(uint32_t ignoreBrokenLink);
void takeFramePacking(void);
void takeUserData(uint32_t *userData, uint32_t registered, uint32_t bytes);

/* info.c */
void updateFrameInfo(void);
void updateSecondFieldInfo(void);

/* stream.c (fw1/pu1's, both codecs'): the read stream, in the code overlay */
extern uint16_t g_streamHalf VIP_CODEC_LOCAL;

void readStreamInterrupt(void);

/* timing.c (fw1/pu1's, both codecs') */
void rescaleClock(uint32_t oldScale, uint32_t newScale);
int takePictureTime(uint32_t noDecodeTimePts, uint32_t option, uint32_t flags,
	uint32_t ptsLow, uint32_t dtsLow, const VipTime *fallback);


