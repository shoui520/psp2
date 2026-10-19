/*
 * FW2's codec: the picture units and the decoder's state, fw1's (fw1/pu1/codec.c)
 * without the frame sets and the error log. The open takes the decoder's rings
 * and frame stores and starts a stream; each run sets the units up
 * (initializeUnits) and restarts them between slices and pictures (resetUnit0,
 * resetUnits).
 */
#include "codec.h"

VipRingHandle *g_ringHandles[VIP_CODEC_RING_HANDLES] VIP_GLOBAL;
VipFrameHandle *g_frameHandles[VIP_CODEC_FRAMES] VIP_GLOBAL;
uint32_t g_workBase VIP_GLOBAL;
uint32_t g_workSize VIP_GLOBAL;
VipPictureBuffer *g_dpbByPoc[VIP_CODEC_FRAMES] VIP_GLOBAL;
VipPictureBuffer *g_dpbByDecode[VIP_CODEC_FRAMES] VIP_GLOBAL;
uint32_t g_dpbCount VIP_GLOBAL;
uint32_t g_refListCount VIP_GLOBAL;
uint32_t g_list0Count VIP_GLOBAL;
uint32_t g_list1Count VIP_GLOBAL;
uint32_t g_referenceFrameCount VIP_GLOBAL;
uint32_t g_generation VIP_GLOBAL;
int8_t g_maxLongTermFrameIdx VIP_GLOBAL;
uint8_t g_motionOwners[VIP_CODEC_FRAMES - 1] VIP_GLOBAL;
int8_t g_layoutDone VIP_GLOBAL;
int8_t g_secondFieldPending VIP_GLOBAL;
uint8_t g_lastFieldMacroblock VIP_CODEC_LOCAL;

/* the units' interrupts 5 and 8: only acknowledged (vectors.S) */
void interrupt5(void)
{
	vipAcknowledgeInterrupt(VIP_INTERRUPT_DECODER);
}

void interrupt8(void)
{
	vipAcknowledgeInterrupt(VIP_INTERRUPT_8);
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
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(13), 0);
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

/* Start the stream over: no parameter sets, no pictures. */
void resetDecoding(void)
{
	VipPictureBuffer *buffer;
	uint32_t i;

	g_layoutDone = 0;
	g_secondFieldPending = 0;
	g_dpbCount = 0;
	g_currentSps = NULL;
	g_currentPps = NULL;
	vipZeroDram((uint32_t)g_sps, VIP_SPS_COUNT * sizeof(VipSps));
	vipZeroDram((uint32_t)g_pps, VIP_PPS_COUNT * sizeof(VipPps));
	vipZeroDram((uint32_t)&g_sliceHeader, sizeof(g_sliceHeader));
	g_bufferingPeriodTaken = 0;
	g_recoveryPointTaken = 0;
	g_pictureTimingTaken = 0;
	g_spsTaken = 0;
	g_randomAccess = RANDOM_ACCESS_NONE;
	for (i = 0; i < VIP_CODEC_FRAMES; i++) {
		buffer = &g_buffers[i];
		buffer->reference = VIP_REFERENCE_NONE;
		buffer->topReference = VIP_REFERENCE_NONE;
		buffer->bottomReference = VIP_REFERENCE_NONE;
		buffer->longTermFrameIdx = 0;
		buffer->picture = g_workBase;
		buffer->colocated = g_workBase + DEFAULT_COLOCATED;
		buffer->idle = 1;
	}
	for (i = 0; i < sizeof(g_motionOwners); i++)
		g_motionOwners[i] = VIP_MOTION_NO_OWNER;
	g_currentBuffer = NULL;
	g_referenceFrameCount = 0;
	g_generation++;
	g_maxLongTermFrameIdx = -1;
}

/* Drop every picture's reference marking and motion data without showing it. */
void flushPictures(void)
{
	VipPictureBuffer *buffer;
	uint32_t i;

	g_randomAccess = RANDOM_ACCESS_NONE;
	for (i = 0; i < VIP_CODEC_FRAMES; i++) {
		buffer = &g_buffers[i];
		buffer->reference = VIP_REFERENCE_NONE;
		buffer->topReference = VIP_REFERENCE_NONE;
		buffer->bottomReference = VIP_REFERENCE_NONE;
		buffer->longTermFrameIdx = 0;
		buffer->idle = 1;
	}
	for (i = 0; i < sizeof(g_motionOwners); i++)
		g_motionOwners[i] = VIP_MOTION_NO_OWNER;
	g_generation++;
	g_referenceFrameCount = 0;
	g_maxLongTermFrameIdx = -1;
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

/* clear a part of the units' RAM; its length in 8 bytes */
static void clearUnitRam(uint32_t offset, uint32_t length)
{
	clearByDma(VIP_DMA_LIST | VIP_DMA_FILL | VIP_DMA_EXTERNAL_LOCAL,
		(uint32_t)g_codecDma.clear.zeros, offset << 16 | length);
}

void clearCodecUnitState(void)
{
	clearUnitRam(VIP_UNIT_CODEC_STATE, VIP_UNIT_CODEC_STATE_SIZE);
}

/* Clear the units' per-picture state. */
void clearUnitState(void)
{
	clearUnitRam(VIP_UNIT_SLICE_STATE, VIP_UNIT_SLICE_STATE_SIZE);
}

static void clearOpenState(void)
{
	clearUnitRam(VIP_UNIT_OPEN_STATE, VIP_UNIT_OPEN_STATE_SIZE);
}

/* The decoder opened: take its rings and frame stores, and start a stream. */
int openDecoder(VipCodecDecoder *decoder)
{
	VipFrameHandle *frames[VIP_CODEC_FRAMES];
	VipPictureBuffer *buffer;
	uint32_t failed, i;

	for (i = 0; i < VIP_CODEC_RING_HANDLES; i++)
		g_ringHandles[i] = &decoder->rings[i];
	g_workSize = decoder->workSize;
	failed = openRing(g_ringHandles[VIP_RING_SLICES], VIP_RING_OPEN_READER);
	failed |= openRing(g_ringHandles[VIP_RING_RECORDS], VIP_RING_OPEN_READER);
	failed |= openRing(g_ringHandles[VIP_RING_OUTPUT], VIP_RING_OPEN_WRITER);
	for (i = 0; i < VIP_CODEC_FRAMES; i++) {
		frames[i] = &decoder->frames[i];
		g_frameHandles[i] = frames[i];
		failed |= lockFrame(frames[i]);
	}
	if (failed & 1)
		return -1;

	g_dpbCount = 0;
	g_refListCount = 0;
	g_list0Count = 0;
	g_list1Count = 0;
	for (i = 0; i < VIP_CODEC_FRAMES; i++) {
		g_dpbByPoc[i] = NULL;
		g_dpbByDecode[i] = NULL;
		buffer = &g_buffers[i];
		buffer->index = i;
		buffer->idle = 1;
		buffer->frame = frames[i];
		setFrameAvailable(frames[i], 1);
	}
	g_currentBuffer = NULL;
	g_generation = 0;
	g_lastFieldMacroblock = 0;
	g_pictureTime = 0;
	g_periodTime = (uint64_t)-1;
	g_pendingPeriodTime = 0;
	g_timeStarted = 0;
	g_timeFraction = 0;
	g_periodTimeFraction = 0;
	resetFrameInfo();
	g_underrunCount = 0;
	g_pictureCount = 0;
	g_outputCount = 0;
	g_listsDone = 0;
	g_colocatedDone = 0;
	clearOpenState();
	resetDecoding();
	return 0;
}

/* The decoder closed: give its rings and frame stores back. */
int closeDecoder(VipCodecDecoder *decoder)
{
	uint32_t failed, i;

	for (i = 0; i < VIP_CODEC_RING_HANDLES; i++)
		g_ringHandles[i] = &decoder->rings[i];
	g_workSize = decoder->workSize;
	failed = closeRing(g_ringHandles[VIP_RING_SLICES]);
	failed |= closeRing(g_ringHandles[VIP_RING_RECORDS]);
	failed |= closeRing(g_ringHandles[VIP_RING_OUTPUT]);
	for (i = 0; i < VIP_CODEC_FRAMES; i++) {
		g_frameHandles[i] = &decoder->frames[i];
		failed |= unlockFrame(g_frameHandles[i]);
	}
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
	g_spsTaken = 0;
	g_recoveryPointTaken = 0;
	g_pictureTimingTaken = 0;
	g_lastPicStruct = g_picStruct;
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
	vipWriteControlBus(VIP_CB_UNIT_MODE(0), VIP_ENTROPY_UNIT_MODE);
	vipWriteControlBus(VIP_CB_PREDICTION_MODE, PREDICTION_SETUP);
}
