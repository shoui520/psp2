/*
 * PU1: the slice headers. A picture's first slice header chooses the parameter
 * sets, starts the picture and lays the frame stores out when the picture size
 * changes (takePictureHeader); each slice header sets the picture units up for its
 * slice (takeSliceHeader).
 */
#include "codec.h"

/* dec_ref_pic_marking()'s words */
#define MARKING_OPERATION_SHIFT		29
#define MARKING_DIFFERENCE_SHIFT	11	/* difference_of_pic_nums_minus1, 18 bits */
#define MARKING_LONG_TERM_IDX_SHIFT	6
#define MARKING_MAX_LONG_TERM_SHIFT	1

/* the slice's tables go from local RAM to the units' */
#define SLICE_TABLE_DMA		(VIP_DMA_LIST | VIP_DMA_EXTERNAL_LOCAL)

#define WEIGHTED_BIPRED_IMPLICIT	2

/* the most a picture may be in VIP_MEMORY_STANDARD, in macroblocks */
#define STANDARD_MAX_WIDTH_MBS	120
#define STANDARD_MAX_HEIGHT_MBS	68
#define STANDARD_MAX_FRAMES	4

uint16_t g_pictureMbs VIP_TASK0_BSS;	/* the picture's macroblocks; a field's are half */
uint8_t g_sliceType VIP_TASK0_BSS;
uint8_t g_fieldPic VIP_TASK0_BSS;
uint8_t g_bottomField VIP_TASK0_BSS;
uint8_t g_implicitWeights VIP_TASK0_BSS;	/* WEIGHTED_BIPRED_IMPLICIT << 1, or 0 */
int8_t g_mbaffFrame VIP_TASK0_BSS;
uint32_t g_sliceSetup VIP_TASK0_BSS;
int8_t g_needsLists VIP_TASK0_BSS;		/* the lists' changes went to the units */
int8_t g_listModified[2] VIP_TASK0_BSS;
volatile uint32_t g_listsDone VIP_TASK0_BSS;

/* Set the picture units up for the slice whose header is in g_parsed. */
static void setupSlice(void)
{
	const uint32_t *words = g_parsed;
	VipPps *pps = g_currentPps;
	uint32_t l0Active, l1Active, deblocking, table, list1;

	g_sliceHeader.firstMbInSlice = words[0] >> VIP_SLICE_FIRST_MB_SHIFT & 0x1fff;
	g_sliceType = words[0] >> VIP_SLICE_TYPE_SHIFT & 0xf;
	if (g_sliceType >= 5)
		g_sliceType -= 5;
	deblocking = words[VIP_SLICE_CODING] >> VIP_SLICE_DEBLOCKING_SHIFT & 3;
	vipWriteControlBus(VIP_CB_SLICE_QP, (pps->picInitQpMinus26 + 26 +
		(words[VIP_SLICE_CODING] >> VIP_SLICE_QP_DELTA_SHIFT)) & 0x7f);
	if (words[VIP_SLICE_LISTS] >> VIP_SLICE_OVERRIDE & 1) {
		l0Active = words[VIP_SLICE_LISTS] >> VIP_SLICE_L0_ACTIVE_SHIFT & 0x1f;
		l1Active = words[VIP_SLICE_LISTS] >> VIP_SLICE_L1_ACTIVE_SHIFT & 0x1f;
	} else {
		l0Active = pps->numRefIdxL0DefaultActiveMinus1;
		l1Active = pps->numRefIdxL1DefaultActiveMinus1;
	}
	g_sliceSetup = g_sliceType << 15 | g_fieldPic << 14 | g_bottomField << 13 |
		(words[VIP_SLICE_LISTS] >> VIP_SLICE_DIRECT_SPATIAL & 1) << 12 |
		l0Active << 7 | l1Active << 2 | deblocking;
	vipWriteControlBus(VIP_CB_LUMA_LOG2_WEIGHT_DENOM,
		words[VIP_SLICE_LISTS] >> VIP_SLICE_LUMA_DENOM_SHIFT & 7);
	vipWriteControlBus(VIP_CB_CHROMA_LOG2_WEIGHT_DENOM,
		words[VIP_SLICE_LISTS] >> VIP_SLICE_CHROMA_DENOM_SHIFT & 7);
	vipWriteControlBus(VIP_CB_DEBLOCKING_IDC, deblocking);
	vipWriteControlBus(VIP_CB_ALPHA_OFFSET, words[VIP_SLICE_CODING] >> VIP_SLICE_ALPHA_SHIFT & 0xf);
	vipWriteControlBus(VIP_CB_BETA_OFFSET, words[VIP_SLICE_CODING] >> VIP_SLICE_BETA_SHIFT & 0xf);

	/* pred_weight_table() */
	table = VIP_SLICE_HEADER_WORDS;
	if (words[0] >> VIP_SLICE_NEEDS_LISTS & 1)
		table += VIP_SLICE_LIST_ENTRIES;
	if (words[0] >> VIP_SLICE_HAS_WEIGHTS & 1) {
		g_codecDma.slice.weights.external = (uint32_t)&words[table];
		g_codecDma.slice.weights.localLength =
			VIP_UNIT_WEIGHTS << 16 | VIP_UNIT_WEIGHTS_SIZE;
		queueListDma(SLICE_TABLE_DMA, (uint32_t)&g_codecDma.slice.weights << 16 |
			sizeof(VipDmaDescriptor), 0, 0, NULL, NULL);
	}

	/* ref_pic_list_modification() of both lists */
	g_needsLists = words[0] >> VIP_SLICE_NEEDS_LISTS & 1;
	g_listModified[0] = words[VIP_SLICE_LISTS] >> VIP_SLICE_L0_MODIFIED & 1;
	g_listModified[1] = words[VIP_SLICE_LISTS] >> VIP_SLICE_L1_MODIFIED & 1;
	g_listsDone = 0;
	if (!g_needsLists)
		return;
	list1 = VIP_SLICE_HEADER_WORDS + VIP_SLICE_LIST_ENTRIES;
	if (words[0] >> VIP_SLICE_HAS_WEIGHTS & 1)
		list1 += VIP_SLICE_WEIGHT_WORDS;
	if (words[0] >> VIP_SLICE_HAS_MARKING & 1)
		list1 += VIP_SLICE_MARKING_WORDS;
	g_codecDma.slice.lists[0].external = (uint32_t)&words[VIP_SLICE_HEADER_WORDS];
	g_codecDma.slice.lists[0].localLength =
		VIP_UNIT_LIST0_CHANGES << 16 | VIP_UNIT_LIST_CHANGES_SIZE;
	g_codecDma.slice.lists[1].external = (uint32_t)&words[list1];
	g_codecDma.slice.lists[1].localLength =
		VIP_UNIT_LIST1_CHANGES << 16 | VIP_UNIT_LIST_CHANGES_SIZE;
	queueListDma(SLICE_TABLE_DMA, (uint32_t)g_codecDma.slice.lists << 16 |
		sizeof(g_codecDma.slice.lists), 0, 0, NULL, &g_listsDone);
}

/* dec_ref_pic_marking()'s operations, up to the one that ends them */
static void takeMarking(void)
{
	const uint32_t *words = g_parsed;
	uint32_t table, word, i;

	table = VIP_SLICE_HEADER_WORDS;
	if (words[0] >> VIP_SLICE_NEEDS_LISTS & 1)
		table += VIP_SLICE_LIST_ENTRIES;
	if (words[0] >> VIP_SLICE_HAS_WEIGHTS & 1)
		table += VIP_SLICE_WEIGHT_WORDS;
	if (!g_sliceHeader.adaptiveMarking)
		return;
	for (i = 0; i < VIP_MARKING_OPERATIONS; i++) {
		word = words[table + i];
		g_sliceHeader.operation[i] = word >> MARKING_OPERATION_SHIFT & 7;
		if (!g_sliceHeader.operation[i])
			return;
		g_sliceHeader.differenceOfPicNumsMinus1[i] =
			word >> MARKING_DIFFERENCE_SHIFT & 0x3ffff;
		g_sliceHeader.longTermFrameIdx[i] = word >> MARKING_LONG_TERM_IDX_SHIFT & 0x1f;
		g_sliceHeader.maxLongTermFrameIdxPlus1[i] =
			word >> MARKING_MAX_LONG_TERM_SHIFT & 0x1f;
	}
}

/*
 * The picture size changed: start over with the new size, in VIP_MEMORY_DOUBLE in
 * the other set of frame stores.
 */
static int layoutAgain(uint32_t mode)
{
	uint32_t i;

	g_secondFieldPending = 0;
	g_otherFieldRandomAccess = 0;
	if (mode == VIP_MEMORY_DOUBLE)
		g_frameSet = !g_frameSet;
	else
		g_frameSet = 0;
	g_dpbCount = 0;
	for (i = 0; i < VIP_PICTURE_BUFFERS; i++) {
		g_buffers[i].reference = VIP_REFERENCE_NONE;
		g_buffers[i].topReference = VIP_REFERENCE_NONE;
		g_buffers[i].bottomReference = VIP_REFERENCE_NONE;
		g_buffers[i].longTermFrameIdx = 0;
	}
	for (i = 0; i < sizeof(g_motionOwners); i++)
		g_motionOwners[i] = 0xff;
	g_maxLongTermFrameIdx = -1;
	g_referenceFrameCount = 0;
	if (layoutFrames(mode) < 0)
		return VIP_PICTURE_TOO_LARGE;
	g_currentBuffer = NULL;
	return VIP_PICTURE_LAID_OUT;
}

/*
 * A VIP_PARSED_PICTURE_HEADER record: a picture's first slice header. recoveryAtI
 * lets an I picture start the decoding; mustDrain refuses a new picture size until
 * the decode has drained the pictures of the old one (VIP_PICTURE_DRAIN).
 */
int takePictureHeader(uint32_t mode, uint32_t recoveryAtI, uint32_t mustDrain)
{
	const uint32_t *words = g_parsed;
	VipSliceHeader *slice = &g_sliceHeader;
	VipSps *sps;
	VipPps *pps;
	uint32_t widthMbs, heightMbs, i;
	int result;

	readParsed();
	pps = &g_pps[words[0] >> VIP_SLICE_PPS_ID_SHIFT & 0xff];
	g_currentPps = pps;
	sps = &g_sps[pps->spsId];
	g_currentSps = sps;
	if (!sps->valid || !pps->valid)
		return VIP_PICTURE_NO_PARAMETERS;
	if (mode == VIP_MEMORY_STANDARD && sps->maxNumRefFrames > STANDARD_MAX_FRAMES)
		return VIP_PICTURE_TOO_MANY_FRAMES;

	/* the second field of a pair must belong with the first */
	slice->adaptiveMarking = words[0] >> VIP_SLICE_HAS_MARKING & 1;
	slice->idr = words[0] >> VIP_SLICE_IDR & 1;
	if (g_secondFieldPending && slice->reference != (words[0] >> VIP_SLICE_REFERENCE & 1))
		return VIP_PICTURE_NOT_PAIRED;
	slice->reference = words[0] >> VIP_SLICE_REFERENCE & 1;
	if (g_secondFieldPending && slice->frameNum != (words[1] >> VIP_SLICE_FRAME_NUM_SHIFT))
		return VIP_PICTURE_NOT_PAIRED;
	slice->frameNum = words[1] >> VIP_SLICE_FRAME_NUM_SHIFT;
	slice->fieldPic = words[1] >> VIP_SLICE_FIELD_PIC & 1;
	slice->bottomField = words[1] >> VIP_SLICE_BOTTOM_FIELD & 1;
	g_fieldPic = slice->fieldPic;
	g_bottomField = slice->bottomField;
	slice->picOrderCntLsb = words[VIP_SLICE_POC_LSB];
	if (pps->bottomFieldPicOrderInFramePresent && !(words[1] >> VIP_SLICE_FIELD_PIC & 1))
		slice->deltaPicOrderCntBottom = words[VIP_SLICE_DELTA_POC_BOTTOM];
	else
		slice->deltaPicOrderCntBottom = 0;
	if (sps->deltaPicOrderAlwaysZero) {
		slice->deltaPicOrderCnt[0] = 0;
		slice->deltaPicOrderCnt[1] = 0;
	} else {
		slice->deltaPicOrderCnt[0] = words[VIP_SLICE_DELTA_POC];
		slice->deltaPicOrderCnt[1] = words[VIP_SLICE_DELTA_POC + 1];
	}
	slice->longTermReference = words[VIP_SLICE_CODING] >> VIP_SLICE_LONG_TERM_REFERENCE & 1;

	setupSlice();
	g_implicitWeights = (pps->weightedBipredIdc == WEIGHTED_BIPRED_IMPLICIT) << 1;
	vipWriteControlBus(VIP_CB_SLICE_RUNNING, 1);
	vipWriteControlBus(VIP_CB_MACROBLOCK_NUMBER, 0);
	takeMarking();

	/* the frame stores, for the picture's size */
	result = VIP_PICTURE_DONE;
	if (!g_layoutDone) {
		if (layoutFrames(mode) < 0)
			return VIP_PICTURE_TOO_LARGE;
		g_layoutDone = 1;
	} else {
		widthMbs = sps->picWidthInMbsMinus1 + 1;
		heightMbs = (sps->picHeightInMapUnitsMinus1 + 1) * (2 - sps->frameMbsOnly);
		if (mode == VIP_MEMORY_STANDARD) {
			if (widthMbs > STANDARD_MAX_WIDTH_MBS ||
			    heightMbs > STANDARD_MAX_HEIGHT_MBS)
				return VIP_PICTURE_TOO_LARGE;
		} else if (widthMbs != g_layoutWidthMbs || heightMbs != g_layoutHeightMbs) {
			if (g_secondFieldPending)
				return VIP_PICTURE_NOT_PAIRED;
			if (mustDrain)
				return VIP_PICTURE_DRAIN;
			result = layoutAgain(mode);
			if (result < 0)
				return result;
		}
	}
	g_layoutWidthMbs = sps->picWidthInMbsMinus1 + 1;
	g_layoutHeightMbs = (sps->picHeightInMapUnitsMinus1 + 1) * (2 - sps->frameMbsOnly);
	g_pictureMbs = g_layoutWidthMbs * g_layoutHeightMbs;
	if (words[1] >> VIP_SLICE_FIELD_PIC & 1)
		g_pictureMbs >>= 1;

	/* where the decoding may start */
	if (words[0] >> VIP_SLICE_IDR & 1) {
		g_randomAccess = RANDOM_ACCESS_IDR;
		g_recoveryFrameCount = 0;
		if (words[VIP_SLICE_CODING] >> VIP_SLICE_NO_OUTPUT_OF_PRIOR_PICS & 1) {
			for (i = 0; i < VIP_PICTURE_BUFFERS; i++)
				g_buffers[i].idle = 1;
		}
	}
	if (recoveryAtI && g_sliceType == VIP_SLICE_TYPE_I &&
	    g_randomAccess == RANDOM_ACCESS_NONE) {
		g_recoveryFrameCount = 0;
		g_randomAccess = RANDOM_ACCESS_RECOVERY;
		g_recoveryPointTaken = 1;
	}
	clearUnitState();
	return result;
}

/* A VIP_PARSED_SLICE_HEADER record: another slice of the picture. */
int takeSliceHeader(void)
{
	readParsed();
	setupSlice();
	return 0;
}
