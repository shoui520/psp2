/*
 * FW2: the picture start, PU1's (fw1/pu1/picture.c) changed. There is one set of
 * frame stores; the work memory is always laid out by fields (PU1's memory mode
 * 1); a stream of one frame store saves no motion. A picture wider than 1216
 * pixels below level 3.1 decodes in the HD loop unless the codec's options allow
 * it in the SD one (VIP_CODEC_OPTION_PICTURE), and startPicture says when it did
 * not. With no motion data free, an IDR picture or one that forgets all
 * references takes the first.
 */
#include "codec.h"

/* the work memory's layout for the units: by fields */
#define MOTION_LAYOUT		1

/* the motion saved of a macroblock: its size in DRAM, and in DMA units */
#define MOTION_SIZE		32
#define MOTION_LENGTH		2

uint32_t g_currPicNum VIP_GLOBAL;	/* CurrPicNum */
uint32_t g_maxPicNum VIP_GLOBAL;	/* MaxPicNum */

int8_t g_standardPicture VIP_CODEC_LOCAL;
uint8_t g_entropyCommand VIP_CODEC_LOCAL;	/* VIP_CB_UNIT_COMMAND(0) */
int8_t g_mbaffFrame VIP_CODEC_LOCAL;
uint8_t g_widthMbsMinus1 VIP_CODEC_LOCAL;
uint8_t g_mbaffLastX VIP_CODEC_LOCAL;		/* 2 * PicWidthInMbs - 1 */
uint8_t g_lastMbRow VIP_CODEC_LOCAL;		/* an MBAFF frame's: the last pair's top */
int8_t g_heightMapUnitsMinus1 VIP_CODEC_LOCAL;

/* Take the picture buffer the picture decodes into: 0, or -1 if none is free. */
int allocatePictureBuffer(void)
{
	VipPictureBuffer *buffer;
	uint32_t count = g_dpbCount, i, j;

	/* a second field goes into its first field's */
	if (g_sliceHeader.fieldPic && g_secondFieldPending)
		return 0;

	if (count != g_bufferCount) {
		g_currentBuffer = &g_buffers[count];
		return 0;
	}
	for (i = 0; i < g_bufferCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->topReference != VIP_REFERENCE_NONE ||
		    buffer->bottomReference != VIP_REFERENCE_NONE || buffer->idle != 1 ||
		    !getFrameAvailable(buffer->frame))
			continue;
		g_currentBuffer = buffer;
		for (j = i; j < (uint32_t)g_bufferCount - 1; j++)
			g_dpbByPoc[j] = g_dpbByPoc[j + 1];
		g_dpbCount = count - 1;
		return 0;
	}
	return -1;
}

/* whether the slice's memory_management_control_operations forget all references */
static int forgetsAll(void)
{
	uint32_t i;

	for (i = 0; i < VIP_MARKING_OPERATIONS; i++)
		if (g_sliceHeader.operation[i] == VIP_MMCO_FORGET_ALL)
			return 1;
	return 0;
}

/*
 * The picture's motion data: the slot its buffer had, else the first slot that is
 * free or whose picture is no longer a reference, else for a picture that
 * starts over the first. 0, or -1 when there is none.
 */
int takeMotionData(void)
{
	VipPictureBuffer *buffer = g_currentBuffer, *owner;
	uint32_t slots = g_bufferCount - 1, i;

	for (i = 0; i < slots; i++)
		if (g_motionOwners[i] == buffer->index)
			return 0;
	for (i = 0; i < slots; i++) {
		if (g_motionOwners[i] != VIP_MOTION_NO_OWNER) {
			owner = &g_buffers[g_motionOwners[i]];
			if (owner->topReference != VIP_REFERENCE_NONE ||
			    owner->bottomReference != VIP_REFERENCE_NONE)
				continue;
		}
		buffer->colocated = g_motionData[i];
		g_motionOwners[i] = buffer->index;
		return 0;
	}
	if (!slots)
		return 0;
	if (!g_sliceHeader.idr && (!g_sliceHeader.adaptiveMarking || !forgetsAll()))
		return -1;
	buffer->colocated = g_motionData[0];
	g_motionOwners[0] = buffer->index;
	return 0;
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
static const uint8_t *scalingList4x4(const VipSps *sps, const VipPps *pps, uint32_t i)
{
	uint32_t first = i < 3 ? 0 : 3;
	const uint8_t *list;

	if (pps) {
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
static const uint8_t *scalingList8x8(const VipSps *sps, const VipPps *pps, uint32_t i)
{
	if (pps && pps->scalingListPresent[6 + i])
		return pps->scaling.lists8x8[i];
	if (sps->scalingListPresent[6 + i])
		return sps->scaling.lists8x8[i];
	return g_default8x8[i];
}

/*
 * List the scaling lists the units take in g_codecDma.scaling: the PPS's (pps not
 * NULL) or the SPS's. The firmware has the two as two functions.
 */
static uint32_t listScalingLists(const VipSps *sps, const VipPps *pps)
{
	VipCodecDma *dma = &g_codecDma;
	uint32_t kind, local, i, n = 0;

	for (kind = 0; kind < 2; kind++) {
		local = VIP_UNIT_SCALING + kind * SCALING_KIND_SIZE;
		for (i = 0; i < 3; i++) {
			dma->scaling[n].external = (uint32_t)scalingList4x4(sps, pps, kind * 3 + i);
			dma->scaling[n++].localLength = (local + i * SCALING_4X4) << 16 |
				SCALING_4X4;
		}
		dma->scaling[n].external = (uint32_t)scalingList8x8(sps, pps, kind);
		dma->scaling[n++].localLength = (local + 3 * SCALING_4X4) << 16 | SCALING_8X8;
	}
	return (uint32_t)dma->scaling << 16 | sizeof(dma->scaling);
}

/* List a fill of the first count of g_codecDma.fill's targets, size bytes each. */
static uint32_t listFill(uint32_t pattern, const uint32_t *locals, uint32_t count,
	uint32_t size)
{
	VipCodecDma *dma = &g_codecDma;
	uint32_t i;

	dma->fill.pattern[0] = pattern;
	dma->fill.pattern[1] = pattern;
	for (i = 0; i < count; i++) {
		dma->fill.targets[i].external = (uint32_t)dma->fill.pattern;
		dma->fill.targets[i].localLength = locals[i] << 16 |
			size / sizeof(dma->fill.pattern);
	}
	return (uint32_t)dma->fill.targets << 16 | count * sizeof(VipDmaDescriptor);
}

static void runList(uint32_t control, uint32_t list, uint32_t parameter)
{
	volatile uint32_t done = 0;

	queueListDma(control, list, parameter, 0, NULL, &done);
	while (done == 1)
		;
}

/* Load the units' scaling lists: the High profile's, or flat ones. */
static void loadScalingLists(void)
{
	static const uint32_t scaling = VIP_UNIT_SCALING;
	const VipSps *sps = g_currentSps;
	const VipPps *pps = g_currentPps;
	uint32_t list;

	if (sps->profileIdc != VIP_PROFILE_HIGH) {
		list = listFill(NO_SCALING, &scaling, 1, SCALING_SIZE);
		vipWriteControlBus(VIP_CB_SCALING_MATRIX, 0);
		runList(FILL_DMA, list, sizeof(g_codecDma.fill.pattern));
		return;
	}
	if (pps->scalingMatrixPresent) {
		list = listScalingLists(sps, pps);
		vipWriteControlBus(VIP_CB_SCALING_MATRIX, 1);
		runList(SCALING_DMA, list, 0);
	} else if (sps->scalingMatrixPresent) {
		list = listScalingLists(sps, NULL);
		vipWriteControlBus(VIP_CB_SCALING_MATRIX, 1);
		runList(SCALING_DMA, list, 0);
	} else {
		list = listFill(FLAT_SCALING, &scaling, 1, SCALING_SIZE);
		vipWriteControlBus(VIP_CB_SCALING_MATRIX, 1);
		runList(FILL_DMA, list, sizeof(g_codecDma.fill.pattern));
	}
}

/* the picture's PicOrderCnt into the DPB, and its fields' */
static void takePicOrderCnt(void)
{
	VipPictureBuffer *buffer = g_currentBuffer;
	int32_t top, bottom;

	updatePicOrderCnt(&top, &bottom);
	if (!g_sliceHeader.fieldPic) {
		insertCurrentPicture(top);
		buffer->topPicOrderCnt = top;
		buffer->bottomPicOrderCnt = bottom;
	} else if (!g_sliceHeader.bottomField) {
		if (!g_secondFieldPending)
			insertCurrentPicture(top);
		buffer->topPicOrderCnt = top;
	} else {
		if (!g_secondFieldPending)
			insertCurrentPicture(bottom);
		buffer->bottomPicOrderCnt = bottom;
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
 * than 1216 pixels and that is not allowed (*tooWide), else up to 720 pixels wide.
 */
static int8_t standardPicture(uint32_t allowWide, int *tooWide)
{
	const VipSps *sps = g_currentSps;

	if (sps->levelIdc >= LEVEL_3_1)
		return sps->picWidthInMbsMinus1 < SD_MAX_WIDTH_MBS;
	if (sps->picWidthInMbsMinus1 <= LOW_LEVEL_MAX_WIDTH_MBS ||
	    allowWide)
		return 1;
	*tooWide = 1;
	return 0;
}

/* the units the picture's macroblocks go through, and its slices' parameters */
static void setupPictureUnits(uint32_t allowWide, int *tooWide)
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
	g_sliceParameters[VIP_SLICE_TYPE_I] = sliceParameters(0);
	g_sliceParameters[VIP_SLICE_TYPE_B] = sliceParameters(pps->weightedBipredIdc);
	vipWriteControlBus(VIP_CB_TOP_FIELD_ORDER_CNT, g_currentBuffer->topPicOrderCnt);
	vipWriteControlBus(VIP_CB_BOTTOM_FIELD_ORDER_CNT, g_currentBuffer->bottomPicOrderCnt);
	g_standardPicture = standardPicture(allowWide, tooWide);
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
		runList(FILL_DMA, listFill(GREY, slotChroma, 4, VIP_UNIT_SLOT_FILL_SIZE),
			sizeof(g_codecDma.fill.pattern));
}

/* unit 5's channels: the row above's reads, the picture's and its motion's writes */
static void setupChannels(void)
{
	const VipSps *sps = g_currentSps;
	const VipPictureBuffer *buffer = g_currentBuffer;
	uint32_t offset;

	vipWriteControlBus(VIP_CB_CHANNEL_WIDTH, g_widthMbsMinus1);
	vipWriteControlBus(VIP_CB_NEIGHBOUR_MODE, 0);
	vipWriteControlBus(VIP_CB_PICTURE_MODE, 0);
	vipWriteControlBus(VIP_CB_NEIGHBOUR_LAYOUT, CHANNEL_LAYOUT);
	vipWriteControlBus(VIP_CB_PICTURE_LAYOUT, CHANNEL_LAYOUT);
	if (sps->direct8x8Inference) {
		vipWriteControlBus(VIP_CB_MOTION_MODE, 0);
		vipWriteControlBus(VIP_CB_MOTION_LAYOUT, MOTION_LAYOUT);
	} else {
		vipWriteControlBus(VIP_CB_MOTION_MODE, 1);
		vipWriteControlBus(VIP_CB_MOTION_LAYOUT, 0);
	}

	if (!g_sliceHeader.fieldPic) {
		vipWriteControlBus(VIP_CB_MOTION_BASE, buffer->colocated);
		if (!sps->mbAdaptiveFrameField) {
			vipWriteControlBus(VIP_CB_NEIGHBOUR_STRIDE, FRAME_STRIDE);
			vipWriteControlBus(VIP_CB_PICTURE_STRIDE, FRAME_STRIDE);
			vipWriteControlBus(VIP_CB_NEIGHBOUR_BASE, buffer->picture + NEIGHBOUR_FRAME);
			vipWriteControlBus(VIP_CB_PICTURE_BASE, buffer->picture + PICTURE_FRAME);
		} else {
			vipWriteControlBus(VIP_CB_NEIGHBOUR_STRIDE, FIELD_STRIDE);
			vipWriteControlBus(VIP_CB_PICTURE_STRIDE, FIELD_STRIDE);
			vipWriteControlBus(VIP_CB_NEIGHBOUR_BASE, buffer->picture + NEIGHBOUR_FIELD);
			vipWriteControlBus(VIP_CB_PICTURE_BASE, buffer->picture + PICTURE_FIELD);
		}
		vipWriteControlBus(VIP_CB_MOTION_STRIDE,
			sps->direct8x8Inference ? MOTION_SIZE : 4 * VIP_MOTION_MACROBLOCK_WORDS);
		return;
	}

	offset = 0;
	if (!g_sliceHeader.bottomField) {
		vipWriteControlBus(VIP_CB_MOTION_BASE, buffer->colocated);
	} else {
		vipWriteControlBus(VIP_CB_MOTION_BASE, buffer->colocated + MOTION_SIZE);
		offset = VIP_BOTTOM_FIELD_OFFSET;
	}
	vipWriteControlBus(VIP_CB_NEIGHBOUR_BASE, buffer->picture + NEIGHBOUR_FIELD + offset);
	vipWriteControlBus(VIP_CB_PICTURE_BASE, buffer->picture + PICTURE_FIELD + offset);
	vipWriteControlBus(VIP_CB_MOTION_STRIDE, 2 * MOTION_SIZE);
	vipWriteControlBus(VIP_CB_NEIGHBOUR_STRIDE, FIELD_STRIDE);
	vipWriteControlBus(VIP_CB_PICTURE_STRIDE, FIELD_STRIDE);
}

/*
 * What a reference picture keeps of its motion for the direct prediction of
 * later pictures, and the overlay that saves it. A stream of one frame store
 * keeps none.
 */
static void setupMotionSave(void)
{
	uint32_t i;

	if (g_bufferCount == 1 || !g_sliceHeader.reference) {
		g_motionSave = VIP_MOTION_SAVE_NONE;
	} else if (!g_currentSps->direct8x8Inference) {
		g_motionSave = VIP_MOTION_SAVE_ALL;
		readDram((uint32_t)__motion_all_load, (uint32_t)saveMacroblockMotion,
			(uint32_t)__motion_all_size);
	} else {
		g_motionSave = VIP_MOTION_SAVE_INFERRED;
		for (i = 0; i < VIP_MOTION_RECORDS; i++)
			g_motionList[i].localLength =
				(uint32_t)g_motionBuffer.records[i] << 16 | MOTION_LENGTH;
		readDram((uint32_t)__motion_inferred_load, (uint32_t)saveMacroblockMotion,
			(uint32_t)__motion_inferred_size);
	}
	g_motionSize = MOTION_SIZE;
}

/* The macroblock loop for the picture, and the local side of its writes. */
static void loadMacroblockLoop(void)
{
	VipDmaDescriptor *loop = g_codecDma.loop;

	if (g_mbaffFrame) {
		loop[6].localLength = LOOP_LOCAL(0x6400, 4);
		loop[8].localLength = LOOP_LOCAL(0x7300, 4);
		loop[7].localLength = LOOP_LOCAL(0x6700, 4);
		loop[10].localLength = LOOP_LOCAL(0x6880, 2);
		loop[9].localLength = LOOP_LOCAL(0x6580, 2);
		loop[12].localLength = LOOP_LOCAL(0x70c0, 2);
		loop[11].localLength = LOOP_LOCAL(0x6c40, 2);
		loop[4].localLength = LOOP_LOCAL(0x6100, 2);
		loop[5].localLength = LOOP_LOCAL(0x6300, 2);
		readDram((uint32_t)__loop_mbaff_load, (uint32_t)runMacroblocks,
			(uint32_t)__loop_mbaff_size);
		return;
	}

	if (!g_currentSps->direct8x8Inference) {
		vipWriteControlBus(VIP_CB_COLOCATED_MODE, 1);
		vipWriteControlBus(VIP_CB_COLOCATED_LAYOUT, 0);
		vipWriteControlBus(VIP_CB_COLOCATED_STRIDE, 4 * VIP_MOTION_MACROBLOCK_WORDS);
	}
	if (!g_fieldPic) {
		loop[6].localLength = LOOP_LOCAL(0x6a00, 4);
		loop[4].localLength = LOOP_LOCAL(0x6000, 4);
		loop[8].localLength = LOOP_LOCAL(0x6400, 4);
	} else {
		loop[6].localLength = LOOP_LOCAL(0x6c40, 2);
		loop[4].localLength = LOOP_LOCAL(0x6100, 2);
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
 * up for it. allowWide lets a wide picture below level 3.1 decode in the SD loop;
 * 1 when one did not, else 0.
 */
int startPicture(uint32_t allowWide)
{
	const VipSps *sps = g_currentSps;
	VipPictureBuffer *buffer;
	uint32_t maxFrameNum;
	int tooWide = 0;

	clearRepeat();
	takePicOrderCnt();
	buffer = g_currentBuffer;
	buffer->frameNum = g_sliceHeader.frameNum;
	buffer->generation = g_generation;
	maxFrameNum = 1u << (sps->log2MaxFrameNumMinus4 + 4);
	if (!g_secondFieldPending && g_recoveryPointTaken)
		g_recoveryFrameCount = (g_recoveryFrameCount + g_sliceHeader.frameNum) &
			(maxFrameNum - 1);

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
			/* the frame lists too, as the firmware has it; the field lists replace them */
			initFrameLists();
			initFieldLists();
		}
	}

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

	setupPictureUnits(allowWide, &tooWide);
	setupChannels();
	g_directMacroblock = NULL;
	g_colocatedPreloaded = 0;
	setupMotionSave();
	loadMacroblockLoop();

	vipWriteControlBus(VIP_CB_LIST_FIELD_PIC, g_fieldPic);
	vipWriteControlBus(VIP_CB_LIST_FRAME_NUM, g_sliceHeader.frameNum);
	vipWriteControlBus(VIP_CB_LIST_LOG2_MAX_FRAME_NUM, sps->log2MaxFrameNumMinus4);
	vipWriteControlBus(VIP_CB_LIST_TOP_ORDER_CNT, buffer->topPicOrderCnt);
	vipWriteControlBus(VIP_CB_LIST_BOTTOM_ORDER_CNT, buffer->bottomPicOrderCnt);
	vipWriteControlBus(VIP_CB_WEIGHT_CHROMA, sps->chroma);
	return tooWide;
}
