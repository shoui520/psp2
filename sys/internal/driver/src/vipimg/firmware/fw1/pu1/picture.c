/*
 * PU1: a picture as it starts. It gets a picture buffer, a free one or, when the
 * DPB is full, one already output and no longer a reference, and motion data to
 * keep for the direct prediction of later pictures. g_frameInfo gets what the ARM
 * side reports of it: its size and cropping from the SPS, how it is scanned
 * (pic_timing()'s pic_struct, else the slice header), its MPEG picture type and
 * the aspect ratio its sample aspect ratio gives.
 */
#include "codec.h"
#include "../common/debug.h"

/* Table E-1's sample aspect ratios, aspect_ratio_idc 1 on */
static const struct {
	uint8_t width;
	uint8_t height;
} sampleAspectRatios[] = {
	{   1,  1 }, {  12, 11 }, {  10, 11 }, {  16, 11 },
	{  40, 33 }, {  24, 11 }, {  20, 11 }, {  32, 11 },
	{  80, 33 }, {  18, 11 }, {  15, 11 }, {  64, 33 },
	{ 160, 99 }, {   4,  3 }, {   3,  2 }, {   2,  1 },
};

#define SAMPLE_ASPECT_RATIOS	(sizeof(sampleAspectRatios) / sizeof(sampleAspectRatios[0]))

/* whether the next picture's pic_timing() has it output before this one */
static int nextOutputFirst(void)
{
	return g_pictureTimingTaken && g_nextTimingSeen &&
		g_nextOutputTime < g_cpbRemovalDelay + g_dpbOutputDelay;
}

/*
 * A field's order, by its fields' order counts; when they are the same, by the
 * order the next picture, the other field, is output in.
 */
static uint32_t fieldOrder(int bottom)
{
	int32_t top, bottomOrder;

	peekPicOrderCnt(&top, &bottomOrder);
	if (top < bottomOrder)
		return TOP_FIELD_FIRST;
	if (bottomOrder < top)
		return 0;
	if (bottom)
		return nextOutputFirst() ? TOP_FIELD_FIRST : 0;
	return nextOutputFirst() ? 0 : TOP_FIELD_FIRST;
}

/* the scan by pic_struct; -1 for a pic_struct it does not know */
static int32_t timedScan(void)
{
	switch (g_picStruct) {
	case VIP_PIC_STRUCT_FRAME:
		if (g_ctType == VIP_CT_TYPE_INTERLACED)
			return TOP_FIELD_FIRST;
		return PROGRESSIVE_SEQUENCE | PROGRESSIVE_FRAME;
	case VIP_PIC_STRUCT_TOP_FIELD:
		return fieldOrder(0);
	case VIP_PIC_STRUCT_BOTTOM_FIELD:
		return fieldOrder(1);
	case VIP_PIC_STRUCT_TOP_BOTTOM:
		if (g_ctType != VIP_CT_TYPE_PROGRESSIVE)
			return TOP_FIELD_FIRST;
		return PROGRESSIVE_FRAME | TOP_FIELD_FIRST;
	case VIP_PIC_STRUCT_BOTTOM_TOP:
		if (g_ctType != VIP_CT_TYPE_PROGRESSIVE)
			return 0;
		return PROGRESSIVE_FRAME;
	case VIP_PIC_STRUCT_TOP_BOTTOM_TOP:
		return PROGRESSIVE_FRAME | TOP_FIELD_FIRST | REPEAT_FIRST_FIELD;
	case VIP_PIC_STRUCT_BOTTOM_TOP_BOTTOM:
		return PROGRESSIVE_FRAME | REPEAT_FIRST_FIELD;
	case VIP_PIC_STRUCT_FRAME_DOUBLING:
		return PROGRESSIVE_SEQUENCE | REPEAT_FIRST_FIELD;
	case VIP_PIC_STRUCT_FRAME_TRIPLING:
		return PROGRESSIVE_SEQUENCE | TOP_FIELD_FIRST | REPEAT_FIRST_FIELD;
	}
	return -1;
}

/* the scan without pic_struct; -1 for a second field, which keeps the first's */
static int32_t codedScan(void)
{
	if (!g_sliceHeader.fieldPic) {
		if (!g_currentSps->frameMbsOnly)
			return TOP_FIELD_FIRST;
		return PROGRESSIVE_SEQUENCE | PROGRESSIVE_FRAME;
	}
	if (g_secondFieldPending)
		return -1;
	return g_sliceHeader.bottomField ? 0 : TOP_FIELD_FIRST;
}

/* the picture type by slice_type; 0 for SP and SI, which keep the last */
static uint32_t pictureType(void)
{
	switch (g_sliceType) {
	case VIP_SLICE_TYPE_P:
		return VIP_FRAME_TYPE_P;
	case VIP_SLICE_TYPE_B:
		return VIP_FRAME_TYPE_B;
	case VIP_SLICE_TYPE_I:
		return g_sliceHeader.idr ? VIP_FRAME_TYPE_IDR : VIP_FRAME_TYPE_I;
	}
	return 0;
}

/* ten times the display aspect ratio of the picture's display[0], or 0 */
static int32_t displayAspect(const VipSps *sps, uint32_t width, uint32_t height)
{
	uint32_t idc = sps->aspectRatioIdc;

	if (!height)
		return 0;
	if (idc == EXTENDED_SAR) {
		if (!sps->sarHeight)
			return 0;
		return (int32_t)(sps->sarWidth * 10 * width) /
			(int32_t)(sps->sarHeight * height);
	}
	if (idc - 1 >= SAMPLE_ASPECT_RATIOS)
		return 0;
	return (int32_t)(width * sampleAspectRatios[idc - 1].width * 10) /
		(int32_t)(height * sampleAspectRatios[idc - 1].height);
}

/* the MPEG-2 aspect ratio nearest it, or 0 */
static uint32_t nearestAspect(int32_t aspect)
{
	if (aspect >= 9 && aspect <= 11)
		return VIP_FRAME_ASPECT_1_1;
	if (aspect >= 12 && aspect <= 14)
		return VIP_FRAME_ASPECT_4_3;
	if (aspect >= 16 && aspect <= 18)
		return VIP_FRAME_ASPECT_16_9;
	if (aspect >= 21 && aspect <= 23)
		return VIP_FRAME_ASPECT_221_100;
	return 0;
}

/* Set g_frameInfo up for the picture that starts. */
void updateFrameInfo(void)
{
	VipFrameInfo *info = &g_frameInfo;
	const VipSps *sps = g_currentSps;
	uint32_t cropUnitX, cropUnitY, type, i;
	int32_t scan;

	info->width = sps->picWidthInMbsMinus1 * 16 + 16;
	info->height = (2 - sps->frameMbsOnly) * (sps->picHeightInMapUnitsMinus1 * 16 + 16);
	cropUnitX = sps->chroma ? 2 : 1;
	cropUnitY = cropUnitX * (2 - sps->frameMbsOnly);
	info->cropLeft = sps->frameCropLeft * cropUnitX;
	info->cropRight = sps->frameCropRight * cropUnitX;
	info->cropTop = sps->frameCropTop * cropUnitY;
	info->cropBottom = sps->frameCropBottom * cropUnitY;

	/* a pan-scan rectangle the SEI did not give is the cropped picture */
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++) {
		if (!info->display[i].width)
			info->display[i].width = info->width - info->cropLeft -
				info->cropRight;
		if (!info->display[i].height)
			info->display[i].height = info->height - info->cropTop -
				info->cropBottom;
	}

	if (g_pictureTimingTaken && sps->picStructPresent)
		scan = timedScan();
	else
		scan = codedScan();
	if (scan >= 0)
		info->flags = (info->flags & ~SCAN_MASK) | scan;

	if (!g_secondFieldPending) {
		type = pictureType();
		if (type)
			info->flags = (info->flags & ~TYPE_MASK) |
				type << VIP_FRAME_TYPE_SHIFT;
		if (g_sliceHeader.reference)
			info->flags |= 1u << VIP_FRAME_NAL_REFERENCE;
		else
			info->flags &= ~(1u << VIP_FRAME_NAL_REFERENCE);
		if (g_sliceHeader.idr || g_recoveryPointTaken)
			info->flags |= 1u << VIP_FRAME_RANDOM_ACCESS;
		else
			info->flags &= ~(1u << VIP_FRAME_RANDOM_ACCESS);
	}

	info->flags = (info->flags & ~ASPECT_MASK) | nearestAspect(displayAspect(sps,
		info->display[0].width, info->display[0].height)) << VIP_FRAME_ASPECT_SHIFT;
	if (!g_secondFieldPending)
		info->flags &= ~PICTURE_FLAGS;
}

/*
 * The picture's motion data: the slot its buffer had, else the first slot that is
 * free or whose picture is no longer a reference. The owner is looked at in the
 * first set of buffers whichever set decodes.
 */
static void takeMotionData(VipPictureBuffer *buffer)
{
	VipPictureBuffer *owner;
	uint32_t slots = g_bufferCount - 1, i;

	for (i = 0; i < slots; i++)
		if (g_motionOwners[i] == buffer->index)
			return;
	for (i = 0; i < slots; i++) {
		if (g_motionOwners[i] != VIP_MOTION_NO_OWNER) {
			owner = &g_buffers[g_motionOwners[i]];
			if (owner->topReference != VIP_REFERENCE_NONE ||
			    owner->bottomReference != VIP_REFERENCE_NONE)
				continue;
		}
		buffer->colocated = g_motionData[i];
		g_motionOwners[i] = buffer->index;
		return;
	}
}

/* Take the picture buffer the picture decodes into: 0, or -1 if none is free. */
int allocatePictureBuffer(void)
{
	VipPictureBuffer *buffer;
	uint32_t available, i, j;

	/* a second field goes into its first field's */
	if (g_sliceHeader.fieldPic && g_secondFieldPending)
		return 0;

	if (g_dpbCount != g_bufferCount) {
		g_currentBuffer = &g_buffers[g_dpbCount + (g_frameSet ? VIP_FRAME_SET : 0)];
		takeMotionData(g_currentBuffer);
		return 0;
	}

	g_noFreeBuffer = 1;
	for (i = 0; i < g_bufferCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->topReference != VIP_REFERENCE_NONE ||
		    buffer->bottomReference != VIP_REFERENCE_NONE || buffer->idle != 1)
			continue;
		getFrameAvailable(buffer->frame, &available);
		if (!available)
			continue;
		g_currentBuffer = buffer;
		for (j = i; j < g_bufferCount - 1; j++)
			g_dpbByPoc[j] = g_dpbByPoc[j + 1];
		g_dpbCount--;
		takeMotionData(g_currentBuffer);
		return 0;
	}
	return -1;
}

/* the motion saved of a macroblock, in DMA units */
#define MOTION_LENGTH		4
#define MOTION_LENGTH_SIZED	2

uint32_t g_streamSettings;
uint32_t g_currPicNum VIP_TASK0_BSS;
uint32_t g_maxPicNum VIP_TASK0_BSS;
uint8_t g_entropyCommand VIP_TASK0_BSS;	/* VIP_CB_UNIT_COMMAND(0) */
uint8_t g_mbaffLastX VIP_TASK0_BSS;		/* 2 * PicWidthInMbs - 1 */
uint8_t g_lastMbRow VIP_TASK0_BSS;		/* an MBAFF frame's: the last pair's top */
int8_t g_heightMapUnitsMinus1 VIP_TASK0_BSS;

/* Wait for a DMA request queueListDma marks 1 until it completes. */
static void waitDma(volatile uint32_t *done)
{
	while (*done == 1)
		;
}

/*
 * Fill the first count of g_codecDma.fill's targets, size bytes each, with the
 * pattern.
 */
static void fillUnitRam(uint32_t pattern, const uint32_t *locals, uint32_t count,
	uint32_t size)
{
	VipCodecDma *dma = &g_codecDma;
	volatile uint32_t done;
	uint32_t i;

	dma->fill.pattern[0] = pattern;
	dma->fill.pattern[1] = pattern;
	for (i = 0; i < count; i++) {
		dma->fill.targets[i].external = (uint32_t)dma->fill.pattern;
		dma->fill.targets[i].localLength = locals[i] << 16 |
			size / sizeof(dma->fill.pattern);
	}
	queueListDma(FILL_DMA, (uint32_t)dma->fill.targets << 16 |
		count * sizeof(VipDmaDescriptor), sizeof(dma->fill.pattern), 0, NULL, &done);
	waitDma(&done);
}

/*
 * A parameter set's own 4x4 list i, else the one before it of its kind (fall-back
 * rule A); NULL when it has neither.
 */
static const uint8_t *ownList4x4(const VipScalingMatrix *matrix, const uint8_t *present,
	uint32_t i)
{
	uint32_t first = i < 3 ? 0 : 3;

	for (;; i--) {
		if (present[i])
			return matrix->lists4x4[i];
		if (i == first)
			return NULL;
	}
}

/*
 * The 4x4 list i the picture uses. From a PPS with a scaling matrix, a list it
 * does not have falls back to the SPS's first of its kind only.
 */
static const uint8_t *scalingList4x4(uint32_t i)
{
	const VipSps *sps = g_currentSps;
	const VipPps *pps = g_currentPps;
	uint32_t first = i < 3 ? 0 : 3;
	const uint8_t *list;

	if (pps->scalingMatrixPresent) {
		list = ownList4x4(&pps->scaling, pps->scalingListPresent, i);
		if (list)
			return list;
		if (sps->scalingListPresent[first])
			return sps->scaling.lists4x4[first];
	} else {
		list = ownList4x4(&sps->scaling, sps->scalingListPresent, i);
		if (list)
			return list;
	}
	return g_default4x4[first / 3];
}

/* the 8x8 list i, 0 intra and 1 inter */
static const uint8_t *scalingList8x8(uint32_t i)
{
	const VipSps *sps = g_currentSps;
	const VipPps *pps = g_currentPps;

	if (pps->scalingMatrixPresent && pps->scalingListPresent[6 + i])
		return pps->scaling.lists8x8[i];
	if (sps->scalingListPresent[6 + i])
		return sps->scaling.lists8x8[i];
	return g_default8x8[i];
}

/* Load the units' scaling lists: the High profile's, or flat ones. */
static void loadScalingLists(void)
{
	static const uint32_t scaling = VIP_UNIT_SCALING;
	VipCodecDma *dma = &g_codecDma;
	volatile uint32_t done;
	uint32_t kind, local, i, n = 0;

	if (g_currentSps->profileIdc != VIP_PROFILE_HIGH) {
		vipWriteControlBus(VIP_CB_SCALING_MATRIX, 0);
		fillUnitRam(NO_SCALING, &scaling, 1, SCALING_SIZE);
		return;
	}
	vipWriteControlBus(VIP_CB_SCALING_MATRIX, 1);
	if (!g_currentPps->scalingMatrixPresent && !g_currentSps->scalingMatrixPresent) {
		fillUnitRam(FLAT_SCALING, &scaling, 1, SCALING_SIZE);
		return;
	}
	for (kind = 0; kind < 2; kind++) {
		local = VIP_UNIT_SCALING + kind * SCALING_KIND_SIZE;
		for (i = 0; i < 3; i++) {
			dma->scaling[n].external = (uint32_t)scalingList4x4(kind * 3 + i);
			dma->scaling[n++].localLength = (local + i * SCALING_4X4) << 16 |
				SCALING_4X4;
		}
		dma->scaling[n].external = (uint32_t)scalingList8x8(kind);
		dma->scaling[n++].localLength = (local + 3 * SCALING_4X4) << 16 | SCALING_8X8;
	}
	queueListDma(SCALING_DMA, (uint32_t)dma->scaling << 16 | sizeof(dma->scaling),
		0, 0, NULL, &done);
	waitDma(&done);
}

/* the picture's PicOrderCnt into the DPB, and its fields' */
static void takePicOrderCnt(void)
{
	VipPictureBuffer *buffer;
	int32_t top, bottom;

	updatePicOrderCnt(&top, &bottom);
	if (!g_sliceHeader.fieldPic) {
		insertCurrentPicture(top);
		buffer = g_currentBuffer;
		buffer->topPicOrderCnt = top;
		buffer->bottomPicOrderCnt = bottom;
	} else if (!g_sliceHeader.bottomField) {
		if (!g_secondFieldPending)
			insertCurrentPicture(top);
		g_currentBuffer->topPicOrderCnt = top;
	} else {
		if (!g_secondFieldPending)
			insertCurrentPicture(bottom);
		g_currentBuffer->bottomPicOrderCnt = bottom;
	}
}

static uint32_t sliceParameters(uint32_t weighting)
{
	const VipSps *sps = g_currentSps;
	const VipPps *pps = g_currentPps;

	return (uint32_t)sps->chroma << PARAMETER_CHROMA_SHIFT |
		(uint32_t)sps->mbAdaptiveFrameField << PARAMETER_MBAFF_SHIFT |
		(uint32_t)sps->picWidthInMbsMinus1 << PARAMETER_WIDTH_SHIFT |
		(uint32_t)sps->picHeightInMapUnitsMinus1 << PARAMETER_HEIGHT_SHIFT |
		(uint32_t)sps->frameMbsOnly << PARAMETER_FRAME_MBS_SHIFT |
		(uint32_t)sps->direct8x8Inference << PARAMETER_DIRECT_8X8_SHIFT |
		weighting << PARAMETER_WEIGHTING_SHIFT |
		(uint32_t)pps->chromaQpIndexOffset << PARAMETER_CHROMA_QP_SHIFT |
		(uint32_t)pps->transform8x8Mode << PARAMETER_TRANSFORM_8X8_SHIFT |
		(uint32_t)pps->constrainedIntraPred << PARAMETER_CONSTRAINED_SHIFT |
		pps->secondChromaQpIndexOffset;
}

/*
 * Whether the SD loop decodes the picture: below level 3.1 unless it is wider
 * than 1216 pixels (then by g_streamSettings), else up to 720 pixels wide.
 */
static int8_t standardPicture(void)
{
	const VipSps *sps = g_currentSps;

	if (sps->levelIdc >= LEVEL_3_1)
		return sps->picWidthInMbsMinus1 < SD_MAX_WIDTH_MBS;
	if (sps->picWidthInMbsMinus1 <= LOW_LEVEL_MAX_WIDTH_MBS ||
	    (g_streamSettings & VIP_STREAM_WIDE_LOW_LEVEL))
		return 1;
	g_currentBuffer->outputFlags |= VIP_OUTPUT_TOO_WIDE;
	if (g_streamSettings & VIP_STREAM_REPORT_WIDE)
		notifyDebugger(0, VIP_EVENT_WIDE_PICTURE);
	return 0;
}

/* the units the picture's macroblocks go through, and its slices' parameters */
static void setupPictureUnits(void)
{
	static const uint32_t slotChroma[4] = {
		VIP_UNIT_MACROBLOCK_SLOTS + VIP_UNIT_SLOT_FILL,
		VIP_UNIT_MACROBLOCK_SLOTS + VIP_UNIT_MACROBLOCK_SLOT_SIZE + VIP_UNIT_SLOT_FILL,
		VIP_UNIT_MACROBLOCK_SLOTS + 2 * VIP_UNIT_MACROBLOCK_SLOT_SIZE + VIP_UNIT_SLOT_FILL,
		VIP_UNIT_MACROBLOCK_SLOTS + 3 * VIP_UNIT_MACROBLOCK_SLOT_SIZE + VIP_UNIT_SLOT_FILL,
	};
	const VipSps *sps = g_currentSps;
	const VipPps *pps = g_currentPps;

	g_sliceParameters[VIP_SLICE_TYPE_P] = sliceParameters(pps->weightedPred);
	g_sliceParameters[VIP_SLICE_TYPE_B] = sliceParameters(pps->weightedBipredIdc);
	g_sliceParameters[VIP_SLICE_TYPE_I] = sliceParameters(0);
	vipWriteControlBus(VIP_CB_TOP_FIELD_ORDER_CNT, g_currentBuffer->topPicOrderCnt);
	vipWriteControlBus(VIP_CB_BOTTOM_FIELD_ORDER_CNT, g_currentBuffer->bottomPicOrderCnt);
	g_standardPicture = standardPicture();
	vipWriteControlBus(VIP_CB_STANDARD_PICTURE, g_standardPicture);
	g_entropyCommand = pps->entropyCodingMode ? ENTROPY_CABAC : ENTROPY_CAVLC;

	loadScalingLists();

	vipWriteControlBus(VIP_CB_PREDICTION_MONOCHROME, !sps->chroma);
	vipWriteControlBus(VIP_CB_PREDICTION_FIELD_PIC, g_sliceHeader.fieldPic);
	vipWriteControlBus(VIP_CB_PREDICTION_MBAFF, sps->mbAdaptiveFrameField);
	vipWriteControlBus(VIP_CB_PREDICTION_WIDTH, sps->picWidthInMbsMinus1);
	vipWriteControlBus(VIP_CB_TRANSFORM_8X8_MODE, pps->transform8x8Mode);
	vipWriteControlBus(VIP_CB_WEIGHTED_PRED, pps->weightedPred);
	vipWriteControlBus(VIP_CB_WEIGHTED_BIPRED_IDC, pps->weightedBipredIdc);
	vipWriteControlBus(VIP_CB_DEBLOCKING_MBAFF, sps->mbAdaptiveFrameField);
	vipWriteControlBus(VIP_CB_DEBLOCKING_FIELD_PIC, g_sliceHeader.fieldPic);
	vipWriteControlBus(VIP_CB_DEBLOCKING_MONOCHROME, !sps->chroma);
	if (!sps->chroma)
		fillUnitRam(GREY, slotChroma, 4, VIP_UNIT_SLOT_FILL_SIZE);
}

/* unit 5's channels: the row above's reads, the picture's and its motion's writes */
static void setupChannels(void)
{
	const VipSps *sps = g_currentSps;
	const VipPictureBuffer *buffer = g_currentBuffer;
	uint32_t stride, motionStride, offset;

	vipWriteControlBus(VIP_CB_CHANNEL_WIDTH, g_widthMbsMinus1);
	vipWriteControlBus(VIP_CB_NEIGHBOUR_MODE, 0);
	vipWriteControlBus(VIP_CB_PICTURE_MODE, 0);
	vipWriteControlBus(VIP_CB_NEIGHBOUR_LAYOUT, CHANNEL_LAYOUT);
	vipWriteControlBus(VIP_CB_PICTURE_LAYOUT, CHANNEL_LAYOUT);
	if (sps->direct8x8Inference) {
		vipWriteControlBus(VIP_CB_MOTION_MODE, 0);
		vipWriteControlBus(VIP_CB_MOTION_LAYOUT, g_memoryMode ? 1 : 2);
	} else {
		vipWriteControlBus(VIP_CB_MOTION_MODE, 1);
		vipWriteControlBus(VIP_CB_MOTION_LAYOUT, 0);
	}

	if (!g_sliceHeader.fieldPic) {
		vipWriteControlBus(VIP_CB_MOTION_BASE, buffer->colocated);
		if (!sps->mbAdaptiveFrameField) {
			stride = FRAME_STRIDE;
			vipWriteControlBus(VIP_CB_NEIGHBOUR_STRIDE, stride);
			vipWriteControlBus(VIP_CB_PICTURE_STRIDE, stride);
			vipWriteControlBus(VIP_CB_NEIGHBOUR_BASE, buffer->picture + NEIGHBOUR_FRAME);
			vipWriteControlBus(VIP_CB_PICTURE_BASE, buffer->picture + PICTURE_FRAME);
		} else {
			stride = FIELD_STRIDE;
			vipWriteControlBus(VIP_CB_NEIGHBOUR_STRIDE, stride);
			vipWriteControlBus(VIP_CB_PICTURE_STRIDE, stride);
			vipWriteControlBus(VIP_CB_NEIGHBOUR_BASE, buffer->picture + NEIGHBOUR_FIELD);
			vipWriteControlBus(VIP_CB_PICTURE_BASE, buffer->picture + PICTURE_FIELD);
		}
		if (!sps->direct8x8Inference)
			motionStride = 80;
		else
			motionStride = g_memoryMode ? 32 : 16;
		vipWriteControlBus(VIP_CB_MOTION_STRIDE, motionStride);
		return;
	}

	offset = 0;
	if (!g_sliceHeader.bottomField) {
		vipWriteControlBus(VIP_CB_MOTION_BASE, buffer->colocated);
	} else {
		vipWriteControlBus(VIP_CB_MOTION_BASE, buffer->colocated +
			(g_memoryMode ? 32 : 16));
		offset = VIP_BOTTOM_FIELD_OFFSET;
	}
	vipWriteControlBus(VIP_CB_NEIGHBOUR_BASE, buffer->picture + NEIGHBOUR_FIELD + offset);
	vipWriteControlBus(VIP_CB_PICTURE_BASE, buffer->picture + PICTURE_FIELD + offset);
	vipWriteControlBus(VIP_CB_MOTION_STRIDE, g_memoryMode ? 64 : 32);
	vipWriteControlBus(VIP_CB_NEIGHBOUR_STRIDE, FIELD_STRIDE);
	vipWriteControlBus(VIP_CB_PICTURE_STRIDE, FIELD_STRIDE);
}

/*
 * What a reference picture keeps of its motion for the direct prediction of
 * later pictures, and the overlay that saves it.
 */
static void setupMotionSave(void)
{
	uint32_t i;

	if (!g_sliceHeader.reference) {
		g_motionSave = VIP_MOTION_SAVE_NONE;
		return;
	}
	if (!g_currentSps->direct8x8Inference) {
		g_motionSave = VIP_MOTION_SAVE_ALL;
		readDram((uint32_t)__motion_all_load, (uint32_t)saveMacroblockMotion,
			(uint32_t)__motion_all_size);
		return;
	}
	g_motionSave = VIP_MOTION_SAVE_INFERRED;
	for (i = 0; i < VIP_MOTION_RECORDS; i++)
		g_motionList[i].localLength = (uint32_t)g_motionBuffer.records[i] << 16 |
			(g_memoryMode ? MOTION_LENGTH_SIZED : MOTION_LENGTH);
	readDram((uint32_t)__motion_inferred_load, (uint32_t)saveMacroblockMotion,
		(uint32_t)__motion_inferred_size);
}

/* The macroblock loop for the picture, and the local side of its writes. */
static void loadMacroblockLoop(void)
{
	VipDmaDescriptor *loop = g_codecDma.loop;

	if (g_mbaffFrame) {
		loop[4].localLength = LOOP_LOCAL(0x6100, 2);
		loop[5].localLength = LOOP_LOCAL(0x6300, 2);
		loop[6].localLength = LOOP_LOCAL(0x6400, 4);
		loop[7].localLength = LOOP_LOCAL(0x6700, 4);
		loop[8].localLength = LOOP_LOCAL(0x7300, 4);
		loop[9].localLength = LOOP_LOCAL(0x6580, 2);
		loop[10].localLength = LOOP_LOCAL(0x6880, 2);
		loop[11].localLength = LOOP_LOCAL(0x6c40, 2);
		loop[12].localLength = LOOP_LOCAL(0x70c0, 2);
		readDram((uint32_t)__loop_mbaff_load, (uint32_t)runMacroblocks,
			(uint32_t)__loop_mbaff_size);
		return;
	}

	if (!g_currentSps->direct8x8Inference) {
		vipWriteControlBus(VIP_CB_COLOCATED_MODE, 1);
		vipWriteControlBus(VIP_CB_COLOCATED_LAYOUT, 0);
		vipWriteControlBus(VIP_CB_COLOCATED_STRIDE, 80);
	}
	if (!g_fieldPic) {
		loop[4].localLength = LOOP_LOCAL(0x6000, 4);
		loop[6].localLength = LOOP_LOCAL(0x6a00, 4);
		loop[8].localLength = LOOP_LOCAL(0x6400, 4);
	} else {
		loop[4].localLength = LOOP_LOCAL(0x6100, 2);
		loop[6].localLength = LOOP_LOCAL(0x6c40, 2);
		loop[8].localLength = LOOP_LOCAL(0x6580, 2);
	}
	if (g_standardPicture)
		readDram((uint32_t)__loop_sd_load, (uint32_t)runMacroblocks,
			(uint32_t)__loop_sd_size);
	else
		readDram((uint32_t)__loop_hd_load, (uint32_t)runMacroblocks,
			(uint32_t)__loop_hd_size);
}

/*
 * Start decoding the picture whose first slice header takePictureHeader took,
 * into g_currentBuffer: its order count and place in the DPB, its reference
 * lists' first form, and the units, the scaling lists and the code overlays set
 * up for it.
 */
void startPicture(uint32_t memoryMode)
{
	const VipSps *sps = g_currentSps;
	VipPictureBuffer *buffer;
	uint32_t maxFrameNum;

	clearRepeat();
	takePicOrderCnt();
	buffer = g_currentBuffer;
	buffer->frameNum = g_sliceHeader.frameNum;
	buffer->generation = g_generation;
	maxFrameNum = 1u << (sps->log2MaxFrameNumMinus4 + 4);
	if (!g_secondFieldPending && g_recoveryPointTaken)
		g_recoveryFrameCount = (g_recoveryFrameCount + g_sliceHeader.frameNum) %
			maxFrameNum;

	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(0), 2);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(1), 2);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(2), 2);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(3), 2);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(4), 1);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(5), 1);
	vipWriteControlBus(VIP_CB_PREDICTION_STAGE, 2);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(6), 2);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(7), 3);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(8), 0);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(9), 1);

	updatePicNums();
	if (g_referenceFrameCount) {
		if (!g_sliceHeader.fieldPic) {
			g_currPicNum = g_sliceHeader.frameNum;
			g_maxPicNum = maxFrameNum;
			initFrameRefList();
			initFrameLists();
		} else {
			g_currPicNum = 2 * g_sliceHeader.frameNum + 1;
			g_maxPicNum = 2 * maxFrameNum;
			initFieldRefList();
			initFieldLists();
		}
	}

	g_memoryMode = memoryMode;
	g_mbaffFrame = sps->mbAdaptiveFrameField && !g_sliceHeader.fieldPic;
	g_widthMbsMinus1 = sps->picWidthInMbsMinus1;
	g_heightMapUnitsMinus1 = sps->picHeightInMapUnitsMinus1;
	if (g_mbaffFrame) {
		g_mbaffLastX = 2 * sps->picWidthInMbsMinus1 + 1;
		g_lastMbRow = 2 * sps->picHeightInMapUnitsMinus1;
	} else if (!g_sliceHeader.fieldPic && !sps->frameMbsOnly) {
		g_lastMbRow = 2 * sps->picHeightInMapUnitsMinus1 + 1;
	} else {
		g_lastMbRow = sps->picHeightInMapUnitsMinus1;
	}
	if (g_sliceHeader.fieldPic)
		buffer->coding = VIP_CODING_FIELDS;
	else if (g_mbaffFrame)
		buffer->coding = VIP_CODING_MBAFF;

	setupPictureUnits();
	setupChannels();
	g_colocatedPreloaded = 0;
	g_directMacroblock = NULL;
	setupMotionSave();
	g_motionSize = g_memoryMode ? 32 : 16;
	loadMacroblockLoop();

	vipWriteControlBus(VIP_CB_LIST_FIELD_PIC, g_fieldPic);
	vipWriteControlBus(VIP_CB_LIST_FRAME_NUM, g_sliceHeader.frameNum);
	vipWriteControlBus(VIP_CB_LIST_LOG2_MAX_FRAME_NUM, sps->log2MaxFrameNumMinus4);
	vipWriteControlBus(VIP_CB_LIST_TOP_ORDER_CNT, buffer->topPicOrderCnt);
	vipWriteControlBus(VIP_CB_LIST_BOTTOM_ORDER_CNT, buffer->bottomPicOrderCnt);
	vipWriteControlBus(VIP_CB_WEIGHT_CHROMA, sps->chroma);
}
