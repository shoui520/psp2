/*
 * PU1: the codec's state, and opening, resetting and closing it. The codec keeps
 * the picture buffers (the frame stores with what it knows of each picture), the
 * display information the next picture gets, and the decoding state between
 * records; the picture units do the decoding itself.
 */
#include "codec.h"

/* the rings the codec opens, by VipDecoder.rings */
#define RING_READ		0
#define RING_WRITE		1

/* where the work memory is: the units' address of it decides */
#define WORK_ADDRESS(address)	(((address) & 0xe0000007) | ((address) >> 16 & 0x1ff8) | \
				 ((address) << 10 & 0x1fffe000))
#define WORK_SPACE_MASK		0xe0000000
#define WORK_SPACE_SHIFT	24

/* the picture units' settings at reset */
#define PREDICTION_SETUP	3	/* VIP_CB_PREDICTION_MODE (provisional) */

/* the motion data's place before the stream is laid out */
#define DEFAULT_COLOCATED	0x4ce0

VipFrameHandle *g_frameHandles[VIP_DECODER_FRAMES];
VipRingHandle *g_ringHandles[VIP_DECODER_RINGS];

VipPictureBuffer g_buffers[VIP_PICTURE_BUFFERS];
VipPictureBuffer *g_dpbByPoc[VIP_PICTURE_BUFFERS];
VipPictureBuffer *g_dpbByDecode[VIP_PICTURE_BUFFERS];
uint32_t g_dpbCount;
uint32_t g_refListCount;
uint32_t g_referenceFrameCount;
uint32_t g_list0Count;
uint32_t g_list1Count;
uint32_t g_generation;		/* counts the resets: a picture of an earlier one is stale */
int8_t g_maxLongTermFrameIdx;	/* -1: no long-term frame indices */
uint8_t g_motionOwners[VIP_FRAME_SET - 1];	/* the picture buffer index, 0xff: free */

int8_t g_layoutDone;		/* the frame stores are laid out for the stream */
int8_t g_frameSet;
uint32_t g_workBase;
int8_t g_secondFieldPending;	/* a field was decoded; the next must be its pair */
uint32_t g_layoutWidthMbs;
uint32_t g_layoutHeightMbs;

VipSliceHeader g_sliceHeader;
VipFrameInfo g_frameInfo;
VipFrameExtension g_frameExtension;

int8_t g_randomAccess;		/* RANDOM_ACCESS_* (provisional) */
uint32_t g_recoveryFrameCount;
int8_t g_userDataTaken;		/* the SEI records seen since the last picture */
int8_t g_uuidDataTaken;
int8_t g_bufferingPeriodTaken;
int8_t g_recoveryPointTaken;
int8_t g_pictureTimingTaken;
int8_t g_pictureHeld;		/* the output held the picture: its records come again, its times not */
uint8_t g_picStruct;		/* pic_timing()'s */
uint8_t g_lastPicStruct;	/* the picture before's */
uint8_t g_ctType;
int8_t g_timeStarted;
VipPictureBuffer *g_lastOutput;

/* the decode's state (decode.c; provisional names) */
int8_t g_timeValid;
int8_t g_drainState;		/* VIP_DRAIN_* */
int8_t g_extensionState;
uint8_t g_lastBottomField;	/* VIP_NO_FIELD before the first field */
int8_t g_retryCount;

/* the decoder's statistics (provisional names) */
uint32_t g_errorLog[VIP_ERROR_LOG];	/* the last errors' VIP_ERROR_*, by g_errorCount */
uint32_t g_errorCount;
uint32_t g_underrunCount;	/* look-aheads short of records, after the first picture */
uint32_t g_pictureCount;	/* the pictures decoded */
uint32_t g_outputCount;		/* and given to the output ring */

VipCodecDma g_codecDma VIP_TASK0_BSS;
VipPictureBuffer *g_currentBuffer VIP_TASK0_BSS;
uint8_t g_lastFieldMacroblock VIP_TASK0_BSS;
int8_t g_noFreeBuffer VIP_TASK0_BSS;

/* The display information is the stream's again: nothing from the SPS or SEI. */
static void resetFrameInfo(void)
{
	VipFrameInfo *info = &g_frameInfo;
	uint32_t i;

	info->width = 0;
	info->height = 0;
	info->cropLeft = 0;
	info->cropRight = 0;
	info->cropTop = 0;
	info->cropBottom = 0;
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++) {
		info->display[i].width = 0;
		info->display[i].height = 0;
	}
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++) {
		info->offset[i].x = 0;
		info->offset[i].y = 0;
	}
	info->extension = &g_frameExtension;
	info->colour = VIP_COLOUR_UNSPECIFIED;
	info->flags = 0;
	clearExtension(&g_frameExtension);
}

/* Reset the picture units and set them up to decode. */
int initializeUnits(void)
{
	vipWriteControlBus(VIP_CB_PREDICTION_COMMAND, 1);
	while (vipReadControlBus(VIP_CB_PREDICTION_COMMAND))
		;
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 0);
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 2);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(1), 0);
	vipWriteControlBus(VIP_CB_PREDICTION_ENABLE, 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(3), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(4), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(5), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(1), 1);
	vipWriteControlBus(VIP_CB_PREDICTION_ENABLE, 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(3), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(4), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(5), 1);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(0), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(1), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(2), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(3), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(4), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(5), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(6), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(7), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(8), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(9), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(10), 0);
	vipWriteControlBus(VIP_CB_DIVIDER_ENABLE, 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(7), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(6), 0);
	vipWriteControlBus(VIP_CB_DIVIDER_ENABLE, 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(7), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(6), 1);
	vipWriteControlBus(VIP_CB_UNIT_MODE(0), VIP_ENTROPY_UNIT_MODE);
	vipWriteControlBus(VIP_CB_PREDICTION_CONTROL, 0);
	vipWriteControlBus(VIP_CB_PREDICTION_MODE, PREDICTION_SETUP);
	return 0;
}

/* forget the pictures' reference marking and which motion data each holds */
static void forgetPictures(void)
{
	uint32_t i;

	g_dpbCount = 0;
	for (i = 0; i < VIP_PICTURE_BUFFERS; i++) {
		g_buffers[i].reference = VIP_REFERENCE_NONE;
		g_buffers[i].topReference = VIP_REFERENCE_NONE;
		g_buffers[i].bottomReference = VIP_REFERENCE_NONE;
		g_buffers[i].longTermFrameIdx = 0;
	}
	for (i = 0; i < sizeof(g_motionOwners); i++)
		g_motionOwners[i] = VIP_MOTION_NO_OWNER;
	g_maxLongTermFrameIdx = -1;
	g_referenceFrameCount = 0;
}

/*
 * Start the stream over: no parameter sets, no pictures, and in VIP_MEMORY_DOUBLE
 * the other set of frame stores.
 */
void resetDecoding(uint32_t mode)
{
	VipPictureBuffer *buffer;
	uint32_t i;

	g_secondFieldPending = 0;
	g_layoutDone = 0;
	if (mode == VIP_MEMORY_DOUBLE)
		g_frameSet = !g_frameSet;
	else
		g_frameSet = 0;
	g_dpbCount = 0;
	g_currentSps = NULL;
	g_currentPps = NULL;
	for (i = 0; i < VIP_SPS_COUNT; i++)
		memset(&g_sps[i], 0, sizeof(g_sps[i]));
	for (i = 0; i < VIP_PPS_COUNT; i++)
		memset(&g_pps[i], 0, sizeof(g_pps[i]));
	memset(&g_sliceHeader, 0, sizeof(g_sliceHeader));
	g_bufferingPeriodTaken = 0;
	g_recoveryPointTaken = 0;
	g_pictureTimingTaken = 0;
	g_spsTaken = 0;
	g_ppsTaken = 0;
	g_userDataTaken = 0;
	g_pictureHeld = 0;
	g_uuidDataTaken = 0;
	g_randomAccess = RANDOM_ACCESS_NONE;
	for (i = 0; i < VIP_PICTURE_BUFFERS; i++) {
		buffer = &g_buffers[i];
		buffer->longTermFrameIdx = 0;
		buffer->bottomReference = VIP_REFERENCE_NONE;
		buffer->topReference = VIP_REFERENCE_NONE;
		buffer->reference = VIP_REFERENCE_NONE;
		buffer->picture = g_workBase;
		buffer->idle = 1;
		buffer->colocated = g_workBase + DEFAULT_COLOCATED;
	}
	for (i = 0; i < sizeof(g_motionOwners); i++)
		g_motionOwners[i] = VIP_MOTION_NO_OWNER;
	g_currentBuffer = NULL;
	g_maxLongTermFrameIdx = -1;
	g_referenceFrameCount = 0;
	g_generation++;
}

/* Drop every picture from the DPB without showing it. */
void flushPictures(void)
{
	g_randomAccess = RANDOM_ACCESS_NONE;
	forgetPictures();
	g_generation++;
}

/* Clear the units' per-picture state. */
void clearUnitState(void)
{
	clearByDma(VIP_DMA_LIST | VIP_DMA_FILL | VIP_DMA_EXTERNAL_LOCAL,
		(uint32_t)g_codecDma.clear.zeros,
		VIP_UNIT_SLICE_STATE << 16 | VIP_UNIT_SLICE_STATE_SIZE);
}

/* the work memory's base for the units, by where it is */
static uint32_t workBase(uint32_t workMemory)
{
	uint32_t address = WORK_ADDRESS(workMemory);

	if (!address)
		return 0;
	if (address >= 0x20000000 && address < 0x30000000)
		return 0x20000000;
	if (address >= 0x40000000 && address < 0xc0000000)
		return 0;
	return workMemory;
}

/*
 * The decoder opened: take its rings and frame stores, set up the picture units'
 * interrupts and start a stream.
 */
static int openCodec(VipDecoder *decoder)
{
	VipPictureBuffer *buffer;
	VipRing *ring;
	uint32_t failed, i;

	for (i = 0; i < VIP_DECODER_RINGS; i++)
		g_ringHandles[i] = &decoder->rings[i];
	g_workBase = workBase(decoder->workMemory);
	vipWriteControlBus(VIP_CB_PREDICTION_SETUP,
		(g_workBase & WORK_SPACE_MASK) >> WORK_SPACE_SHIFT);

	failed = openRing(g_ringHandles[VIP_RING_SLICES], RING_READ);
	failed |= openRing(g_ringHandles[VIP_RING_RECORDS], RING_READ);
	failed |= openRing(g_ringHandles[VIP_RING_OUTPUT], RING_WRITE);
	if (getRing(g_ringHandles[VIP_RING_USER_DATA], &ring) >= 0)
		failed |= openRing(g_ringHandles[VIP_RING_USER_DATA], RING_WRITE);
	if (getRing(g_ringHandles[VIP_RING_USER_RECORDS], &ring) >= 0)
		failed |= openRing(g_ringHandles[VIP_RING_USER_RECORDS], RING_WRITE);
	for (i = 0; i < VIP_DECODER_FRAMES; i++) {
		g_frameHandles[i] = &decoder->frames[i];
		failed |= lockFrame(g_frameHandles[i]);
	}
	if (failed & 1)
		return -1;

	setupInterrupt(VIP_INTERRUPT_READ_STREAM0);
	setupInterrupt(VIP_INTERRUPT_READ_STREAM1);
	setupInterrupt(VIP_INTERRUPT_8);
	setupInterrupt(VIP_INTERRUPT_DECODER);
	g_dpbCount = 0;
	g_refListCount = 0;
	g_list0Count = 0;
	g_list1Count = 0;
	for (i = 0; i < VIP_PICTURE_BUFFERS; i++) {
		g_dpbByPoc[i] = NULL;
		g_dpbByDecode[i] = NULL;
		buffer = &g_buffers[i];
		buffer->index = i < VIP_FRAME_SET ? i : i - VIP_FRAME_SET;
		buffer->idle = 1;
		buffer->frame = g_frameHandles[i];
		setFrameAvailable(g_frameHandles[i], 1);
	}
	g_currentBuffer = NULL;
	g_lastOutput = NULL;
	g_generation = 0;
	g_lastFieldMacroblock = 0;
	g_frameSet = 1;
	g_pictureTime = 0;
	g_periodTime = (uint64_t)-1;
	g_pendingPeriodTime = 0;
	g_timeStarted = 0;
	g_timeFraction = 0;
	g_periodTimeFraction = 0;
	resetFrameInfo();
	g_errorCount = 0;
	g_underrunCount = 0;
	g_pictureCount = 0;
	g_outputCount = 0;
	for (i = 0; i < VIP_ERROR_LOG; i++)
		g_errorLog[i] = 0;
	g_noFreeBuffer = 0;
	clearByDma(VIP_DMA_LIST | VIP_DMA_FILL | VIP_DMA_EXTERNAL_LOCAL,
		(uint32_t)g_codecDma.clear.zeros,
		VIP_UNIT_OPEN_STATE << 16 | VIP_UNIT_OPEN_STATE_SIZE);
	resetDecoding(decoder->memoryMode);
	return 0;
}

/* The decoder closed: give its rings and frame stores back. */
static int closeCodec(VipDecoder *decoder)
{
	VipRing *ring;
	uint32_t failed, i;

	for (i = 0; i < VIP_DECODER_RINGS; i++)
		g_ringHandles[i] = &decoder->rings[i];
	failed = closeRing(g_ringHandles[VIP_RING_SLICES]);
	failed |= closeRing(g_ringHandles[VIP_RING_RECORDS]);
	failed |= closeRing(g_ringHandles[VIP_RING_OUTPUT]);
	if (getRing(g_ringHandles[VIP_RING_USER_DATA], &ring) >= 0)
		failed |= closeRing(g_ringHandles[VIP_RING_USER_DATA]);
	if (getRing(g_ringHandles[VIP_RING_USER_RECORDS], &ring) >= 0)
		failed |= closeRing(g_ringHandles[VIP_RING_USER_RECORDS]);
	for (i = 0; i < VIP_DECODER_FRAMES; i++) {
		g_frameHandles[i] = &decoder->frames[i];
		failed |= unlockFrame(g_frameHandles[i]);
	}
	clearMemoryQuery();
	return failed & 1 ? -1 : 0;
}

/* A buffering period takes effect at the picture that follows it. */
static void startBufferingPeriod(void)
{
	if (g_bufferingPeriodTaken) {
		g_periodTime = g_pendingPeriodTime;
		g_periodTimeFraction = g_timeFraction;
	}
	g_bufferingPeriodTaken = 0;
}

/* Restart unit 0 for the next slice. */
void resetUnit0(void)
{
	startBufferingPeriod();
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 0);
	__asm__ __volatile__("nop\n\tnop\n\tnop");
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 1);
	vipWriteControlBus(VIP_CB_UNIT_MODE(0), VIP_ENTROPY_UNIT_MODE);
}

/* Restart the picture units after a picture, and forget the SEI records seen. */
void resetUnits(void)
{
	startBufferingPeriod();
	g_ppsTaken = 0;
	g_spsTaken = 0;
	g_userDataTaken = 0;
	g_uuidDataTaken = 0;
	g_recoveryPointTaken = 0;
	g_pictureTimingTaken = 0;
	g_lastPicStruct = g_picStruct;
	g_pictureHeld = 0;
	g_picStruct = 0;
	if (g_currentBuffer)
		g_pictureTime = g_currentBuffer->decodeTime;

	vipWriteControlBus(VIP_CB_PREDICTION_COMMAND, 1);
	while (vipReadControlBus(VIP_CB_PREDICTION_COMMAND))
		;
	mep_di();
	vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_8);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_DECODER);
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(1), 0);
	vipWriteControlBus(VIP_CB_PREDICTION_ENABLE, 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(3), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(4), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(5), 0);
	mep_ei();
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 2);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(1), 1);
	vipWriteControlBus(VIP_CB_PREDICTION_ENABLE, 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(3), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(4), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(5), 1);
}

/*
 * Fill with zeros by DMA, from the zeros of g_codecDma, and wait for it: the
 * descriptor's external and local sides as the control says.
 */
void clearByDma(uint32_t control, uint32_t external, uint32_t localLength)
{
	volatile uint32_t done = 0;

	g_codecDma.clear.descriptor.external = external;
	g_codecDma.clear.descriptor.localLength = localLength;
	g_codecDma.clear.zeros[0] = 0;
	g_codecDma.clear.zeros[1] = 0;
	queueListDma(control, (uint32_t)&g_codecDma.clear.descriptor << 16 |
		sizeof(VipDmaDescriptor), sizeof(g_codecDma.clear.zeros), 0, NULL, &done);
	while (done == 1)
		;
}

/* the codec's entry points */
static int codecOpen(VipDecoder *decoder, uint32_t point, VipOutputState *output)
{
	(void)point;
	g_retryCount = 0;
	g_drainState = 0;
	g_timeValid = 1;
	g_lastBottomField = VIP_NO_FIELD;
	g_extensionState = 0;
	resetOutputState(output);
	g_lastPicStruct = 0;
	return openCodec(decoder);
}

static int codecClose(VipDecoder *decoder, uint32_t point, VipOutputState *output)
{
	(void)point;
	closeOutputState(output);
	return closeCodec(decoder);
}

void setupCodec(VipCodec *codec)
{
	codec->open = codecOpen;
	codec->decode = decode;
	codec->close = codecClose;
}
