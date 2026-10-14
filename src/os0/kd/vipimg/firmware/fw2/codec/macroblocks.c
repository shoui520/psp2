/*
 * FW2: decoding a slice's macroblocks, PU1's decodeSlice (fw1/pu1/macroblocks.c)
 * changed. The slice data is read in by the read stream's list straight from the
 * slice ring's segments; the work memory is always laid out by fields, so the
 * colocated reads have one layout; there are no implicit weights to fix up. A
 * picture at least 8 macroblocks wide and 2 map units high runs the macroblock
 * loop overlay (runMacroblocks, the SD or HD one by g_standardPicture); a smaller
 * one runs the resident loop (runSmallLoop). The last macroblock's motion is kept
 * even when the loop failed, unless unit 0 timed out.
 */
#include "codec.h"

/* the colocated reads' layout: the work memory's rows are by fields */
#define COLOCATED_LAYOUT	1
#define COLOCATED_STRIDE	0x20	/* a frame macroblock's; a field's twice it */

/* the smallest picture the loop overlays take */
#define OVERLAY_MIN_WIDTH_MBS_MINUS1	7

volatile uint32_t g_colocatedDone VIP_CODEC_LOCAL;
int8_t g_colocatedPreloaded VIP_CODEC_LOCAL;
int8_t g_motionSave VIP_CODEC_LOCAL;		/* VIP_MOTION_SAVE_* */
void (*g_directMacroblock)(void) VIP_CODEC_LOCAL;	/* a direct macroblock's colocated read */

uint16_t g_sliceMbAddress VIP_CODEC_LOCAL;	/* first_mb_in_slice; MBAFF's doubled */
uint8_t g_mbY VIP_CODEC_LOCAL;
uint8_t g_mbX VIP_CODEC_LOCAL;
uint8_t g_neighbourY VIP_CODEC_LOCAL;
uint8_t g_neighbourX VIP_CODEC_LOCAL;
uint32_t g_motionSize VIP_CODEC_LOCAL;		/* a macroblock's motion data in DRAM */

VipDmaDescriptor g_streamList[VIP_STREAM_SEGMENTS] VIP_CODEC_LOCAL __attribute__((aligned(8)));
VipDmaDescriptor g_motionList[VIP_MOTION_RECORDS] VIP_CODEC_LOCAL __attribute__((aligned(8)));

/* the macroblock loop's DMA flags and counters (provisional names) */
volatile uint32_t g_stageDone[2] VIP_CODEC_LOCAL;
uint16_t g_predictionCount VIP_CODEC_LOCAL;
uint16_t g_fieldHistory VIP_CODEC_LOCAL;
uint16_t g_macroblockCount VIP_CODEC_LOCAL;
uint8_t g_pairState VIP_CODEC_LOCAL;
uint8_t g_colocatedColumn VIP_CODEC_LOCAL;	/* from fields: first the place in two rows */
uint8_t g_colocatedField VIP_CODEC_LOCAL;	/* VIP_CB_COLOCATED_FIELD */

uint32_t g_colocatedMacroblock[VIP_MOTION_MACROBLOCK_WORDS] VIP_CODEC_LOCAL
	__attribute__((aligned(8)));
VipDmaDescriptor g_colocatedList VIP_CODEC_LOCAL __attribute__((aligned(8)));
VipMotionBuffer g_motionBuffer VIP_SLICE_BUFFER(motion);

uint8_t g_colocatedX VIP_GLOBAL;	/* the slice's first macroblock in the colocated picture */
uint8_t g_colocatedY VIP_GLOBAL;
uint32_t g_sliceParameters[3] VIP_GLOBAL;	/* VIP_CB_SLICE_PARAMETERS of P, B and I slices */

/* Read the slice data in from g_dataStart: the read stream's whole buffer's worth. */
static void fillReadBuffer(volatile uint32_t *done)
{
	const VipRingSegment *segment = g_sliceSegments;
	uint32_t cursor = g_dataStart, filled = 0, count = 0, length, end, i;

	for (i = 0; i < g_sliceSegmentCount; i++, segment++) {
		if (cursor < segment->start)
			continue;
		end = segment->start + segment->size;
		length = 2 * VIP_STREAM_HALF - filled;
		if (end - cursor < length) {
			length = end - cursor;
			if (length & 7)
				length += 8 - (length & 7);
		}
		g_streamList[count].external = cursor;
		g_streamList[count].localLength = filled << 16 | length;
		filled += length;
		cursor += length;
		if (cursor == end && i + 1 < g_sliceSegmentCount)
			cursor = segment[1].start;
		count++;
		if (filled >= 2 * VIP_STREAM_HALF)
			break;
	}
	g_dataStart = cursor;
	queueListDma(VIP_STREAM_DMA,
		(uint32_t)g_streamList << 16 | count * sizeof(VipDmaDescriptor), 0, 0, NULL, done);
}

static uint32_t waitQuotient(uint32_t *remainder)
{
	while (vipReadControlBus(VIP_CB_DIVIDER_COMMAND))
		;
	*remainder = vipReadControlBus(VIP_CB_REMAINDER);
	return vipReadControlBus(VIP_CB_QUOTIENT);
}

static void startDivide(uint32_t dividend, uint32_t divisor)
{
	vipWriteControlBus(VIP_CB_DIVIDEND, dividend);
	vipWriteControlBus(VIP_CB_DIVISOR, divisor);
	vipWriteControlBus(VIP_CB_DIVIDER_COMMAND, VIP_DIVIDER_DIVIDE);
}

/* the colocated reads, at the address, in rows stride bytes apart */
static void setColocatedReads(uint32_t address, uint32_t stride)
{
	vipWriteControlBus(VIP_CB_COLOCATED_ADDRESS, address);
	vipWriteControlBus(VIP_CB_COLOCATED_LAYOUT, COLOCATED_LAYOUT);
	vipWriteControlBus(VIP_CB_COLOCATED_STRIDE, stride);
}

/*
 * A B slice of a frame or field picture with direct_8x8_inference: start the
 * colocated reads where the slice starts, by how the colocated picture was coded.
 */
static void startDirect(uint32_t widthMbs, uint32_t colocated, uint32_t colocatedCoding,
	uint32_t colocatedBottom)
{
	uint32_t quotient, remainder;

	if (!g_fieldPic) {
		if (colocatedCoding == VIP_CODING_FRAME) {
			vipWriteControlBus(VIP_CB_COLOCATED_MODE, 0);
			vipWriteControlBus(VIP_CB_COLOCATED_X, g_colocatedX);
			vipWriteControlBus(VIP_CB_COLOCATED_Y, g_colocatedY);
			setColocatedReads(colocated, COLOCATED_STRIDE);
			vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
			startFrameDirect();
			g_directMacroblock = frameDirectMacroblock;
			return;
		}
		startDivide(g_sliceMbAddress, 2 * widthMbs);
		vipWriteControlBus(VIP_CB_COLOCATED_X, g_sliceMbAddress % widthMbs);
		vipWriteControlBus(VIP_CB_COLOCATED_MODE, 2);
		setColocatedReads((g_colocatedField << 5) + colocated, 2 * COLOCATED_STRIDE);
		quotient = waitQuotient(&remainder);
		g_colocatedColumn = remainder;
		vipWriteControlBus(VIP_CB_COLOCATED_Y, quotient);
		vipWriteControlBus(VIP_CB_COLOCATED_ROW, remainder);
		vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
		startFrameFromFieldsDirect();
		g_directMacroblock = frameFromFieldsDirectMacroblock;
		return;
	}

	if (colocatedCoding == VIP_CODING_FRAME) {
		startDivide(g_sliceMbAddress, widthMbs);
		vipWriteControlBus(VIP_CB_COLOCATED_MODE, 0);
		setColocatedReads(colocated, 2 * COLOCATED_STRIDE);
		quotient = waitQuotient(&remainder);
		vipWriteControlBus(VIP_CB_COLOCATED_Y, quotient);
		vipWriteControlBus(VIP_CB_COLOCATED_X, remainder);
		vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
		startFieldFromFrameDirect();
		g_directMacroblock = fieldFromFrameDirectMacroblock;
	} else if (colocatedCoding == VIP_CODING_FIELDS) {
		vipWriteControlBus(VIP_CB_COLOCATED_MODE, 0);
		vipWriteControlBus(VIP_CB_COLOCATED_X, g_colocatedX);
		vipWriteControlBus(VIP_CB_COLOCATED_Y, g_colocatedY);
		setColocatedReads((colocatedBottom << 5) + colocated, 2 * COLOCATED_STRIDE);
		vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
		startFieldDirect();
		g_directMacroblock = fieldDirectMacroblock;
	} else {
		startDivide(g_sliceMbAddress * 2, widthMbs);
		vipWriteControlBus(VIP_CB_COLOCATED_MODE, 0);
		setColocatedReads(colocated, COLOCATED_STRIDE);
		quotient = waitQuotient(&remainder);
		vipWriteControlBus(VIP_CB_COLOCATED_X, remainder);
		vipWriteControlBus(VIP_CB_COLOCATED_Y, quotient);
		vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
		startFieldFromMbaffDirect();
		g_directMacroblock = fieldFromMbaffDirectMacroblock;
	}
}

/* An MBAFF B slice: the same, by macroblock pairs. */
static void startMbaffDirect(uint32_t widthMbs, uint32_t colocated, uint32_t colocatedCoding)
{
	uint32_t quotient, remainder;

	if (colocatedCoding != VIP_CODING_FIELDS) {
		vipWriteControlBus(VIP_CB_COLOCATED_MODE, 0);
		vipWriteControlBus(VIP_CB_COLOCATED_X, g_colocatedX);
		vipWriteControlBus(VIP_CB_COLOCATED_Y, g_colocatedY);
		setColocatedReads(colocated, COLOCATED_STRIDE);
		vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
		startMbaffFromFrameDirect();
		g_directMacroblock = mbaffFromFrameDirectMacroblock;
		return;
	}
	startDivide((int16_t)g_sliceMbAddress >> 1, widthMbs);
	vipWriteControlBus(VIP_CB_COLOCATED_MODE, 0);
	setColocatedReads(colocated, 2 * COLOCATED_STRIDE);
	quotient = waitQuotient(&remainder);
	vipWriteControlBus(VIP_CB_COLOCATED_X, remainder);
	vipWriteControlBus(VIP_CB_COLOCATED_Y, quotient);
	vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
	startMbaffFromFieldsDirect();
	g_directMacroblock = mbaffFromFieldsDirectMacroblock;
}

/*
 * A B slice without direct_8x8_inference reads the colocated macroblock where it
 * starts into local RAM before the loop.
 */
static void preloadColocated(uint32_t colocated)
{
	g_colocatedPreloaded = 1;
	vipWriteControlBus(VIP_CB_COLOCATED_X, g_colocatedX);
	vipWriteControlBus(VIP_CB_COLOCATED_Y, g_colocatedY);
	vipWriteControlBus(VIP_CB_COLOCATED_ADDRESS, colocated);
	vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
	while (vipReadControlBus(VIP_CB_COLOCATED_COMMAND))
		;
	g_colocatedList.external = vipReadControlBus(VIP_CB_COLOCATED_DATA);
	vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
	g_colocatedList.localLength = (uint32_t)g_colocatedMacroblock << 16 |
		sizeof(g_colocatedMacroblock);
	queueListDma(VIP_COLOCATED_DMA, (uint32_t)&g_colocatedList << 16 | sizeof(g_colocatedList),
		0, 0, NULL, &g_colocatedDone);
}

/*
 * Keep the motion of the slice's last macroblock with the others of its row, and
 * write the row's records out.
 */
static void saveMotion(void)
{
	uint32_t *record, address, count;

	record = g_motionBuffer.records[(g_macroblockCount - 1) & 0xe];
	address = vipReadControlBus(VIP_CB_MOTION_ADDRESS);
	vipWriteControlBus(VIP_CB_MOTION_COMMAND, VIP_UNIT_START);
	while (g_listsDone == 1)
		;
	if (g_macroblockCount & 1) {
		record[0] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x0);
		record[1] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x1);
		record[2] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x2);
		record[3] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x3);
		record[4] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x4);
		record[6] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x5);
		record[8] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xe);
		record[10] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xf);
		record[5] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x6);
		record[7] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x7);
		record[9] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xc);
		record[11] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xd);
		record[12] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x8);
		record[13] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x9);
		record[14] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xa);
		record[15] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xb);
	} else {
		record += VIP_MOTION_RECORD_WORDS;
		record[0] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x0);
		record[1] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x1);
		record[2] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x2);
		record[3] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x3);
		record[4] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x4);
		record[6] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x5);
		record[8] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xe);
		record[10] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xf);
		record[5] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x6);
		record[7] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x7);
		record[9] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xc);
		record[11] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xd);
		record[12] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x8);
		record[13] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x9);
		record[14] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xa);
		record[15] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xb);
	}
	g_motionList[(g_macroblockCount - 1) & 0xf].external = address;
	count = g_macroblockCount & 0xf;
	if (!count)
		count = VIP_MOTION_RECORDS;
	queueListDma(VIP_MOTION_DMA, (uint32_t)g_motionList << 16 | count * sizeof(VipDmaDescriptor),
		g_motionSize, 0x10000, NULL, &g_listsDone);
}

/* the macroblock loop for the slice: the overlay's, or for a small picture the resident one */
static int runLoop(const VipSps *sps, uint32_t firstMb)
{
	if (!g_mbaffFrame) {
		if (g_widthMbsMinus1 >= OVERLAY_MIN_WIDTH_MBS_MINUS1 && g_heightMapUnitsMinus1)
			return runMacroblocks();	/* the SD or HD overlay, as loaded */
		return runSmallLoop();
	}
	if (sps->picWidthInMbsMinus1 >= OVERLAY_MIN_WIDTH_MBS_MINUS1 &&
	    (int8_t)sps->picHeightInMapUnitsMinus1) {
		g_fieldHistory = 0;
		return runMacroblocks();	/* the MBAFF overlay */
	}
	if (!firstMb)
		g_fieldHistory = 0;
	return runSmallLoop();
}

/*
 * Decode the slice whose header takePictureHeader and takeSliceHeader took, its
 * data at data: 0 when it decoded, negative when not.
 */
int decodeSlice(uint32_t data)
{
	volatile uint32_t done = 0;
	const uint32_t *words = g_parsed;
	VipSps *sps = g_currentSps;
	uint32_t widthMbs = g_widthMbsMinus1 + 1, firstMb, enable, position;
	uint32_t x, y, startX, remainder, colocated, colocatedCoding, list1;
	uint32_t colocatedBottom, index;
	int result;

	firstMb = words[0] >> VIP_SLICE_FIRST_MB_SHIFT & 0x1fff;
	vipWriteControlBus(VIP_CB_WEIGHT_LUMA_DENOM, words[VIP_SLICE_LISTS] >> VIP_SLICE_LUMA_DENOM_SHIFT & 7);
	vipWriteControlBus(VIP_CB_WEIGHT_CHROMA_DENOM, words[VIP_SLICE_LISTS] >> VIP_SLICE_CHROMA_DENOM_SHIFT & 7);
	vipWriteControlBus(VIP_CB_UNIT_COMMAND(7), VIP_UNIT_START);
	startDivide(firstMb, widthMbs);
	enable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, enable | 1 << VIP_INTERRUPT_READ_STREAM0);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, enable | 1 << VIP_INTERRUPT_READ_STREAM1);
	while (g_listsDone == 1)
		;

	vipWriteControlBus(VIP_CB_LIST0_MODIFIED, g_listModified[0]);
	vipWriteControlBus(VIP_CB_LIST1_MODIFIED, g_listModified[1]);
	if (g_sliceType == VIP_SLICE_TYPE_P) {
		vipWriteControlBus(VIP_CB_LIST0_LAST, g_fieldPic ? 0x1f : 0xf);
		vipWriteControlBus(VIP_CB_LIST1_LAST, 0);
	} else if (g_sliceType == VIP_SLICE_TYPE_B) {
		vipWriteControlBus(VIP_CB_LIST0_LAST, g_fieldPic ? 0x1f : 0xf);
		vipWriteControlBus(VIP_CB_LIST1_LAST, g_fieldPic ? 0x1f : 0xf);
	} else {
		vipWriteControlBus(VIP_CB_LIST0_LAST, 0);
		vipWriteControlBus(VIP_CB_LIST1_LAST, 0);
	}
	vipWriteControlBus(VIP_CB_LIST_SLICE_TYPE, g_sliceType);
	vipWriteControlBus(VIP_CB_UNIT_COMMAND(6), VIP_UNIT_START);
	vipWriteControlBus(VIP_CB_SLICE_LIMIT, 0x3fff);
	vipWriteControlBus(VIP_CB_UNIT_COMMAND(0), VIP_UNIT_START);
	g_streamHalf = 0;
	if (g_sliceType == VIP_SLICE_TYPE_P)
		vipWriteControlBus(VIP_CB_SLICE_PARAMETERS, g_sliceParameters[0]);
	else if (g_sliceType == VIP_SLICE_TYPE_B)
		vipWriteControlBus(VIP_CB_SLICE_PARAMETERS, g_sliceParameters[1]);
	else
		vipWriteControlBus(VIP_CB_SLICE_PARAMETERS, g_sliceParameters[2]);

	y = waitQuotient(&x) & 0xff;
	x &= 0xff;
	startX = g_mbaffFrame ? x << 1 : x;
	g_neighbourY = y;
	g_neighbourX = startX;
	g_mbY = y;
	g_mbX = startX;
	position = (y << 8) + startX;
	vipWriteControlBus(VIP_CB_SLICE_POSITION, g_sliceSetup + (position << 17));
	vipWriteControlBus(VIP_CB_SLICE_COUNTER, 0);
	vipWriteControlBus(VIP_CB_SLICE_DATA, data);
	vipWriteControlBus(VIP_CB_SLICE_START, position);
	while (vipReadControlBus(VIP_CB_UNIT_COMMAND(7)))
		;
	while (vipReadControlBus(VIP_CB_UNIT_COMMAND(6)))
		;

	fillReadBuffer(&done);
	vipWriteControlBus(VIP_CB_SLICE_READY0, 1);
	vipWriteControlBus(VIP_CB_SLICE_READY1, 1);
	g_colocatedField = vipReadControlBus(VIP_CB_COLOCATED_FIELD);

	/* the colocated picture: RefPicList1[0] as unit 6 made it */
	list1 = vipUnitList(VIP_UNIT_FINAL_LIST1)->picture;
	colocatedCoding = list1 >> 30 & 1;
	if (!colocatedCoding && sps->mbAdaptiveFrameField)
		colocatedCoding = VIP_CODING_MBAFF;
	index = list1 & VIP_UNIT_PICTURE_INDEX;
	colocatedBottom = list1 >> 29 & 1;
	colocated = g_buffers[index].colocated;
	vipWriteControlBus(VIP_CB_COLOCATED_FIELDS, colocatedCoding == VIP_CODING_FIELDS);

	vipWriteControlBus(VIP_CB_PREDICTION_SLICE, 0);
	vipWriteControlBus(VIP_CB_PREDICTION_SLICE, vipReadControlBus(VIP_CB_SLICE_COUNTER));
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 1);
	vipWriteControlBus(VIP_CB_PREDICTION_START_X, x << 1);
	vipWriteControlBus(VIP_CB_PREDICTION_SLICE_TYPE, g_sliceType);
	if (g_mbaffFrame) {
		/* the divisor is still widthMbs */
		g_sliceMbAddress = firstMb << 1;
		vipWriteControlBus(VIP_CB_DIVIDEND, g_sliceMbAddress);
		vipWriteControlBus(VIP_CB_DIVIDER_COMMAND, VIP_DIVIDER_DIVIDE);
		vipWriteControlBus(VIP_CB_PICTURE_X, x ? x - 1 : 0);
		if (y) {
			vipWriteControlBus(VIP_CB_NEIGHBOUR_X, x);
			vipWriteControlBus(VIP_CB_NEIGHBOUR_Y, y - 1);
		} else {
			vipWriteControlBus(VIP_CB_NEIGHBOUR_X, 0);
			vipWriteControlBus(VIP_CB_NEIGHBOUR_Y, 0);
		}
		vipWriteControlBus(VIP_CB_PICTURE_Y, y);
		g_colocatedY = waitQuotient(&remainder);
		g_colocatedX = remainder;
	} else {
		startDivide(firstMb, widthMbs << 1);
		g_sliceMbAddress = firstMb;
		vipWriteControlBus(VIP_CB_PICTURE_X, x ? x - 1 : 0);
		if (y) {
			vipWriteControlBus(VIP_CB_NEIGHBOUR_X, x);
			vipWriteControlBus(VIP_CB_NEIGHBOUR_Y, y - 1);
		} else {
			vipWriteControlBus(VIP_CB_NEIGHBOUR_X, 0);
			vipWriteControlBus(VIP_CB_NEIGHBOUR_Y, 0);
		}
		vipWriteControlBus(VIP_CB_PICTURE_Y, y);
		if (sps->direct8x8Inference) {
			g_colocatedX = x;
			g_colocatedY = y;
		} else {
			g_colocatedY = waitQuotient(&remainder);
			g_colocatedX = remainder;
		}
	}
	vipWriteControlBus(VIP_CB_MOTION_X, g_colocatedX);
	vipWriteControlBus(VIP_CB_MOTION_Y, g_colocatedY);
	vipWriteControlBus(VIP_CB_MOTION_COMMAND, VIP_UNIT_START);
	vipWriteControlBus(VIP_CB_NEIGHBOUR_COMMAND, VIP_UNIT_START);
	vipWriteControlBus(VIP_CB_PICTURE_COMMAND, VIP_UNIT_START);

	if (g_sliceType > VIP_SLICE_TYPE_I)
		return -1;
	g_stageDone[0] = 0;
	g_stageDone[1] = 0;
	g_predictionCount = 0;
	g_directMacroblock = NULL;
	g_macroblockCount = 0;
	while (done == 1)
		;
	if (!data || firstMb >= g_pictureMbs)
		return -1;

	if (!g_mbaffFrame) {
		if (g_sliceType == VIP_SLICE_TYPE_B) {
			if (!sps->direct8x8Inference)
				preloadColocated(colocated);
			else
				startDirect(widthMbs, colocated, colocatedCoding, colocatedBottom);
		}
	} else {
		g_pairState = 0;
		if (g_sliceType == VIP_SLICE_TYPE_B)
			startMbaffDirect(widthMbs, colocated, colocatedCoding);
	}
	result = runLoop(sps, firstMb);

	mep_di();
	vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
	vipAcknowledgeInterrupt(VIP_INTERRUPT_8);
	mep_ei();
	if (result < 0) {
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
		if (result == VIP_LOOP_ENTROPY_TIMEOUT)
			return result;
	}
	if (g_motionSave == VIP_MOTION_SAVE_INFERRED)
		saveMotion();
	g_lastFieldMacroblock = vipReadControlBus(VIP_CB_FIELD_MACROBLOCK);
	while (g_listsDone == 1)
		;
	return result;
}
