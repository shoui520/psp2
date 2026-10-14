/*
 * The decoded picture buffer, both codecs' (PU1's, and FW2's codec). The pictures
 * in it are kept twice, in output order (g_dpbByPoc) and the latest decoded first
 * (g_dpbByDecode). Here are the reference picture marking (H.264 8.2.5), simpler
 * than the standard's in places noted where they are, the output of the decoded
 * pictures to the output ring, and the picture numbers (8.2.4.1). FW2 hands its
 * pictures out through command.c (handOutPicture, bumpPictures, outputAllPictures);
 * it keeps PU1's bumpPictures and outputAllPictures, unreachable, and they are not
 * written for it. Its concealment fills macroblocks as PU1 does.
 */
#ifdef VIP_FW2
#include "../../fw2/codec/codec.h"
#else
#include "../common/debug.h"
#include "codec.h"
#endif

/* the output ring's record of a picture (provisional) */
#define OUTPUT_RECORD_TYPE	0x8003
#define OUTPUT_TIME_STARTED	0x4000	/* VipPictureBuffer.timeStarted */
#define OUTPUT_PTS_HIGH_SHIFT	19	/* bit 32 of the PTS to VIP_RECORD_PTS_HIGH */
#define OUTPUT_TRIES		10000

/*
 * fillMacroblocks: the colour's place in the units' RAM, a macroblock's rows of
 * 16 bytes of Y, 16 of Cb and Cr and 16 of Y; and the DMA that repeats it at the
 * macroblocks (the descriptors' second words and the sizes as the firmware has
 * them, their fields not known)
 */
#define FILL_ROW_WORDS			12
#define FILL_DMA			(VIP_DMA_LIST | VIP_DMA_BANK1 | 0x31100)
#define FILL_FRAME_DESCRIPTOR		0x64000004
#define FILL_FIELD_DESCRIPTOR		0x65800002
#define FILL_PAIR_SECOND_DESCRIPTOR	0x67000004
#define FILL_FRAME_LENGTH		0x60
#define FILL_FRAME_SIZE			0x10000
#define FILL_FIELD_LENGTH		0xc0
#define FILL_FIELD_SIZE			0x20000
#define FILL_FRAME_OFFSET		0x28	/* from VIP_CB_PICTURE_ADDRESS */
#define FILL_FIELD_OFFSET		0x58
#define FILL_PAIR_SECOND_OFFSET		0xb8

#ifdef VIP_FW2
/* FW2's fill lists are in the codec's DMA lists */
#define g_fillPair		g_codecDma.macroblockFill.pair
#define g_fillDescriptor	g_codecDma.macroblockFill.macroblock
#else
static VipOutputRecord g_outputRecord VIP_TASK0_BSS;
static VipDmaDescriptor g_fillPair[2] VIP_TASK0_BSS;	/* an MBAFF macroblock pair */
static VipDmaDescriptor g_fillDescriptor VIP_TASK0_BSS;
#endif

/* A picture buffer begins a picture: nothing known of it. */
static void clearPictureBuffer(VipPictureBuffer *buffer)
{
#ifndef VIP_FW2
	buffer->outputFlags = 0;
#endif
	buffer->idle = 0;
	buffer->bottomPicOrderCnt = 0;
	buffer->topPicOrderCnt = 0;
	buffer->picNum = 0;
	buffer->frameNum = 0;
	buffer->longTermFrameIdx = 0;
	buffer->coding = VIP_CODING_FRAME;
	buffer->bottomReference = VIP_REFERENCE_NONE;
	buffer->topReference = VIP_REFERENCE_NONE;
	buffer->reference = VIP_REFERENCE_NONE;
}

static void forgetFrame(VipPictureBuffer *buffer)
{
	buffer->reference = VIP_REFERENCE_NONE;
	buffer->topReference = VIP_REFERENCE_NONE;
	buffer->bottomReference = VIP_REFERENCE_NONE;
}

/* the field's reference marking, by parity */
static int8_t *fieldMarking(VipPictureBuffer *buffer, int bottom)
{
	return bottom ? &buffer->bottomReference : &buffer->topReference;
}

/*
 * Mark a field of a field pair unused: the one picNum names, its parity from its
 * lowest bit. The frame stops being a reference frame when both fields are unused.
 */
static void forgetField(int8_t reference, uint32_t picNum, int longTerm)
{
	VipPictureBuffer *buffer;
	int bottom = (picNum & 1) == g_sliceHeader.bottomField;
	uint8_t i;

	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (*fieldMarking(buffer, bottom) != reference ||
		    (buffer->picNum & ~1) != (picNum & ~1))
			continue;
		buffer->reference = VIP_REFERENCE_NONE;
		*fieldMarking(buffer, bottom) = VIP_REFERENCE_NONE;
		if (*fieldMarking(buffer, !bottom))
			continue;
		if (longTerm)
			buffer->longTermFrameIdx = 0;
		g_referenceFrameCount--;
	}
}

/* operation 1: a short-term picture unused */
static void forgetShortTerm(uint32_t picNumX)
{
	VipPictureBuffer *buffer;
	uint8_t i;

	if (g_sliceHeader.fieldPic) {
		forgetField(VIP_REFERENCE_SHORT, picNumX, 0);
		return;
	}
	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->reference == VIP_REFERENCE_SHORT && buffer->picNum == (int32_t)picNumX) {
			forgetFrame(buffer);
			g_referenceFrameCount--;
		}
	}
}

/* operation 2: a long-term picture unused */
static void forgetLongTerm(uint32_t longTermPicNum)
{
	VipPictureBuffer *buffer;
	uint8_t i;

	if (g_sliceHeader.fieldPic) {
		forgetField(VIP_REFERENCE_LONG, longTermPicNum, 1);
		return;
	}
	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->reference == VIP_REFERENCE_LONG &&
		    buffer->picNum == (int32_t)longTermPicNum) {
			forgetFrame(buffer);
			buffer->longTermFrameIdx = 0;
			g_referenceFrameCount--;
		}
	}
}

/* the first long-term frame with the LongTermFrameIdx is unused */
static void freeLongTermFrameIdx(uint32_t idx)
{
	VipPictureBuffer *buffer;
	uint32_t i;

	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->reference == VIP_REFERENCE_LONG && buffer->longTermFrameIdx == idx) {
			forgetFrame(buffer);
			buffer->longTermFrameIdx = 0;
			g_referenceFrameCount--;
			return;
		}
	}
}

/* operation 3: a short-term picture becomes long-term */
static void makeLongTerm(uint32_t picNumX, uint32_t idx)
{
	VipPictureBuffer *buffer;
	int bottom;
	uint8_t i;

	freeLongTermFrameIdx(idx);
	if (!g_sliceHeader.fieldPic) {
		for (i = 0; i < g_dpbCount; i++) {
			buffer = g_dpbByPoc[i];
			if (buffer->reference != VIP_REFERENCE_SHORT ||
			    buffer->picNum != (int32_t)picNumX)
				continue;
			buffer->reference = VIP_REFERENCE_LONG;
			buffer->topReference = VIP_REFERENCE_LONG;
			buffer->bottomReference = VIP_REFERENCE_LONG;
			buffer->longTermFrameIdx = idx;
		}
		return;
	}
	bottom = (picNumX & 1) == g_sliceHeader.bottomField;
	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (*fieldMarking(buffer, bottom) != VIP_REFERENCE_SHORT ||
		    (buffer->picNum & ~1) != (int32_t)(picNumX & ~1))
			continue;
		*fieldMarking(buffer, bottom) = VIP_REFERENCE_LONG;
		buffer->longTermFrameIdx = idx;
		if (*fieldMarking(buffer, !bottom) == VIP_REFERENCE_LONG)
			buffer->reference = VIP_REFERENCE_LONG;
	}
}

/* operation 4: the long-term frames past MaxLongTermFrameIdx unused */
static void limitLongTerm(int8_t maxLongTermFrameIdxPlus1)
{
	VipPictureBuffer *buffer;
	uint8_t i;

	g_maxLongTermFrameIdx = maxLongTermFrameIdxPlus1 - 1;
	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->reference == VIP_REFERENCE_LONG &&
		    g_maxLongTermFrameIdx < (int32_t)buffer->longTermFrameIdx) {
			forgetFrame(buffer);
			g_referenceFrameCount--;
		}
	}
}

/*
 * operation 5: every picture unused, and the current picture's numbering starts
 * over (8.2.1: tempPicOrderCnt).
 */
static void forgetAll(void)
{
	VipPictureBuffer *buffer, *current;
	int32_t order;
	uint8_t i;

	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		forgetFrame(buffer);
		buffer->longTermFrameIdx = 0;
	}
	g_maxLongTermFrameIdx = -1;
	g_prevFrameNum = 0;
	g_referenceFrameCount = 0;
	g_prevPicOrderCntMsb = 0;
	g_frameNumOffset = 0;

	current = g_currentBuffer;
	if (g_sliceHeader.fieldPic && !g_sliceHeader.bottomField)
		g_prevPicOrderCntLsb = current->topPicOrderCnt;
	else
		g_prevPicOrderCntLsb = 0;
	if (!g_sliceHeader.fieldPic) {
		order = current->topPicOrderCnt < current->bottomPicOrderCnt ?
			current->topPicOrderCnt : current->bottomPicOrderCnt;
		current->topPicOrderCnt -= order;
		current->bottomPicOrderCnt -= order;
	} else if (g_sliceHeader.bottomField) {
		current->bottomPicOrderCnt = 0;
	} else {
		current->topPicOrderCnt = 0;
	}
	current->frameNum = 0;
	g_sliceHeader.frameNum = 0;
}

/* operation 6: the current picture becomes long-term */
static void makeCurrentLongTerm(uint32_t idx)
{
	VipPictureBuffer *current = g_currentBuffer;
	int bottom = g_sliceHeader.bottomField != 0;

	freeLongTermFrameIdx(idx);
	if (!g_sliceHeader.fieldPic) {
		current->reference = VIP_REFERENCE_LONG;
		current->topReference = VIP_REFERENCE_LONG;
		current->bottomReference = VIP_REFERENCE_LONG;
	} else {
		*fieldMarking(current, bottom) = VIP_REFERENCE_LONG;
		if (g_secondFieldPending &&
		    *fieldMarking(current, !bottom) == VIP_REFERENCE_LONG) {
			/* the pair's first field is counted already */
			current->reference = VIP_REFERENCE_LONG;
			current->longTermFrameIdx = idx;
			return;
		}
	}
	g_referenceFrameCount++;
	current->longTermFrameIdx = idx;
}

/* 8.2.5.4: the slice's memory_management_control_operations, in order */
static void adaptiveMarking(int8_t *currentLongTerm)
{
	const VipSliceHeader *slice = &g_sliceHeader;
	uint32_t currPicNum, picNumX;
	uint8_t i;

	currPicNum = slice->fieldPic ? 2 * slice->frameNum + 1 : slice->frameNum;
	for (i = 0; i < VIP_MARKING_OPERATIONS; i++) {
		picNumX = currPicNum - (slice->differenceOfPicNumsMinus1[i] + 1);
		switch (slice->operation[i]) {
		case VIP_MMCO_END:
			return;
		case VIP_MMCO_FORGET_SHORT:
			forgetShortTerm(picNumX);
			break;
		case VIP_MMCO_FORGET_LONG:
			forgetLongTerm(slice->differenceOfPicNumsMinus1[i]);
			break;
		case VIP_MMCO_SHORT_TO_LONG:
			makeLongTerm(picNumX, slice->longTermFrameIdx[i]);
			break;
		case VIP_MMCO_MAX_LONG_TERM:
			limitLongTerm(slice->maxLongTermFrameIdxPlus1[i]);
			break;
		case VIP_MMCO_FORGET_ALL:
			forgetAll();
			break;
		case VIP_MMCO_CURRENT_TO_LONG:
			*currentLongTerm = 1;
			makeCurrentLongTerm(slice->longTermFrameIdx[i]);
			break;
		}
	}
}

/*
 * 8.2.5.3: with max_num_ref_frames reference frames the oldest short-term one is
 * unused. A second field whose first is a short-term reference only joins it.
 */
static void slidingWindow(void)
{
	VipPictureBuffer *buffer, *current = g_currentBuffer;
	int8_t i;

	if (g_sliceHeader.fieldPic && g_secondFieldPending) {
		if (g_sliceHeader.bottomField) {
			if (current->topReference == VIP_REFERENCE_SHORT) {
				current->bottomReference = VIP_REFERENCE_SHORT;
				return;
			}
		} else if (current->bottomReference == VIP_REFERENCE_SHORT) {
			current->topReference = VIP_REFERENCE_SHORT;
			return;
		}
	}
	if (g_referenceFrameCount != g_currentSps->maxNumRefFrames)
		return;
	for (i = g_dpbCount - 1; i >= 0; i--) {
		buffer = g_dpbByDecode[i];
		if (buffer->topReference == VIP_REFERENCE_SHORT ||
		    buffer->bottomReference == VIP_REFERENCE_SHORT) {
			forgetFrame(buffer);
			g_referenceFrameCount--;
			return;
		}
	}
}

/*
 * Give a decoded picture to the output ring, with its PTS. FW2 keeps no flags for
 * the record, and builds it in g_codecDma.
 */
void outputPicture(VipPictureBuffer *buffer)
{
#ifdef VIP_FW2
	VipOutputRecord *record = &g_codecDma.output;
#else
	VipOutputRecord *record = &g_outputRecord;
	VipFrame *frame;
#endif
	uint32_t header, written = 0, tries;
	int result;

#ifndef VIP_FW2
	clearUnpairedField(buffer->frame);
#endif
	if (buffer->ptsValid) {
		header = OUTPUT_RECORD_TYPE << VIP_RECORD_TYPE_SHIFT | VIP_RECORD_PTS |
			(uint32_t)((buffer->presentationTime & 0x100000000ull) >>
			OUTPUT_PTS_HIGH_SHIFT);
		record->pts = (uint32_t)buffer->presentationTime;
	} else {
		header = OUTPUT_RECORD_TYPE << VIP_RECORD_TYPE_SHIFT;
		record->pts = 0;
#ifndef VIP_FW2
		buffer->outputFlags |= VIP_OUTPUT_NO_PTS;
#endif
	}
	if (buffer->timeStarted)
		header |= OUTPUT_TIME_STARTED;
	record->header = header;
#ifdef VIP_FW2
	record->flags = 0;
	record->frame = getFrameStore(buffer->frame);
#else
	record->flags = buffer->outputFlags;
	getFrameStore(buffer->frame, &frame);
	record->frame = frame;
#endif

	for (tries = 0; tries < OUTPUT_TRIES; tries++) {
		result = writeRing(g_ringHandles[VIP_RING_OUTPUT],
			(uint32_t)((uint32_t *)record + written / 4), sizeof(*record) - written);
		if (result != -1) {
			written += result;
			if (written == sizeof(*record))
				break;
		}
	}
	buffer->idle = 1;
#ifdef VIP_FW2
	g_outputCount++;
#else
	g_lastOutput = buffer;
	g_outputCount++;
	notifyDebugger(0, VIP_EVENT_OUTPUT);
#endif
}

#ifndef VIP_FW2

/* Give every picture not yet shown to the output ring, in output order. */
void outputAllPictures(void)
{
	VipPictureBuffer *buffer;
	int32_t i;

	for (i = 0; i < g_bufferCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer && !buffer->idle)
			outputPicture(buffer);
	}
	g_dpbCount = 0;
}
#endif

/*
 * 8.2.5.1: mark the current picture. -1 when there are more reference frames
 * than max_num_ref_frames: the pictures are flushed.
 */
int markReference(void)
{
	const VipSliceHeader *slice = &g_sliceHeader;
	VipPictureBuffer *current = g_currentBuffer, *buffer;
	int8_t currentLongTerm = 0;
	uint32_t i;

	if (slice->idr) {
		for (i = 0; i < g_dpbCount; i++) {
			buffer = g_dpbByPoc[i];
			forgetFrame(buffer);
			buffer->longTermFrameIdx = 0;
		}
		g_referenceFrameCount = 0;
		g_maxLongTermFrameIdx = -1;
		if (slice->longTermReference) {
			if (!slice->fieldPic) {
				current->reference = VIP_REFERENCE_LONG;
				current->topReference = VIP_REFERENCE_LONG;
				current->bottomReference = VIP_REFERENCE_LONG;
			} else {
				*fieldMarking(current, slice->bottomField) = VIP_REFERENCE_LONG;
			}
			current->longTermFrameIdx = 0;
			g_referenceFrameCount++;
			g_maxLongTermFrameIdx = 0;
		} else {
			current->reference = VIP_REFERENCE_SHORT;
			g_maxLongTermFrameIdx = -1;
			g_referenceFrameCount++;
			if (!slice->fieldPic) {
				current->topReference = VIP_REFERENCE_SHORT;
				current->bottomReference = VIP_REFERENCE_SHORT;
			} else {
				*fieldMarking(current, slice->bottomField) = VIP_REFERENCE_SHORT;
			}
		}
		return 0;
	}

	if (!slice->reference)
		return 0;
	if (slice->adaptiveMarking)
		adaptiveMarking(&currentLongTerm);
	else
		slidingWindow();
	if (!currentLongTerm) {
		current->reference = VIP_REFERENCE_SHORT;
		if (!g_secondFieldPending)
			g_referenceFrameCount++;
		if (!slice->fieldPic) {
			current->topReference = VIP_REFERENCE_SHORT;
			current->bottomReference = VIP_REFERENCE_SHORT;
		} else {
			*fieldMarking(current, slice->bottomField) = VIP_REFERENCE_SHORT;
		}
	}
	if (g_referenceFrameCount > g_currentSps->maxNumRefFrames &&
	    (!g_fieldPic || g_secondFieldPending)) {
		flushPictures();
		return -1;
	}
	return 0;
}

#ifndef VIP_FW2
/*
 * C.4.5.3: with the buffer full, output the pictures up to the first one that is
 * no reference, in output order.
 */
void bumpPictures(void)
{
	VipPictureBuffer *buffer;
	uint32_t i;
	int32_t j;

	if (g_dpbCount < (uint32_t)(g_bufferCount - 1))
		return;
	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->topReference || buffer->bottomReference)
			continue;
		for (j = 0; j < (int32_t)i; j++)
			if (!g_dpbByPoc[j]->idle)
				outputPicture(g_dpbByPoc[j]);
		if (!buffer->idle)
			outputPicture(buffer);
		return;
	}
}
#endif

/*
 * The current picture joins the buffer: in output order by its PicOrderCnt among
 * the pictures since the last reset, and first of the latest decoded. A full
 * buffer loses its last picture in output order. In FW2 a picture of this
 * generation not yet shown with the same PicOrderCnt is first dropped: it will not
 * be shown, it is no reference, and it goes first in output order, out of the
 * generation.
 */
void insertCurrentPicture(int32_t order)
{
	VipPictureBuffer *previous;
	int32_t i, at;
#ifdef VIP_FW2
	VipPictureBuffer *buffer;
	uint32_t generation = g_generation, count = g_dpbCount, j;

	for (j = 0; j < count; j++) {
		buffer = g_dpbByPoc[j];
		if (buffer->generation != generation || buffer->idle ||
		    picOrderCnt(buffer) != order)
			continue;
		if (buffer->reference || buffer->topReference || buffer->bottomReference)
			g_referenceFrameCount--;
		buffer->idle = 1;
		buffer->generation = generation - 1;
		forgetFrame(buffer);
		for (i = j; i >= 1; i--)
			g_dpbByPoc[i] = g_dpbByPoc[i - 1];
		g_dpbByPoc[0] = buffer;
	}
#endif

	if (g_dpbCount < g_bufferCount)
		g_dpbCount++;
	else
		g_dpbCount = g_bufferCount;
	for (i = g_dpbCount - 1; i >= 1; i--) {
		previous = g_dpbByPoc[i - 1];
		if (previous->generation != g_generation || order >= picOrderCnt(previous))
			break;
		g_dpbByPoc[i] = previous;
	}
	g_dpbByPoc[i] = g_currentBuffer;

	at = g_dpbCount - 1;
	for (i = at; i >= 0; i--) {
		if (g_dpbByDecode[i] == g_currentBuffer) {
			at = i;
			break;
		}
	}
	for (i = at; i >= 1; i--)
		g_dpbByDecode[i] = g_dpbByDecode[i - 1];
	g_dpbByDecode[0] = g_currentBuffer;
	clearPictureBuffer(g_currentBuffer);
}

/* 8.2.4.1: PicNum of the short-term pictures, LongTermPicNum of the long-term ones */
void updatePicNums(void)
{
	VipPictureBuffer *buffer;
	int32_t wrap;
	uint32_t i;

	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->topReference == VIP_REFERENCE_SHORT ||
		    buffer->bottomReference == VIP_REFERENCE_SHORT) {
			wrap = frameNumWrap(buffer->frameNum);
			buffer->picNum = g_sliceHeader.fieldPic ? wrap * 2 : wrap;
		} else if (buffer->topReference == VIP_REFERENCE_LONG ||
			   buffer->bottomReference == VIP_REFERENCE_LONG) {
			buffer->picNum = g_sliceHeader.fieldPic ?
				buffer->longTermFrameIdx * 2 : buffer->longTermFrameIdx;
		}
	}
}

/*
 * Fill count macroblocks, those the units name in turn, with one colour:
 * colour is Y << 16 | Cb << 8 | Cr. The DMA repeats a macroblock's worth of
 * the colour, laid out in the units' RAM, at each.
 */
void fillMacroblocks(uint32_t count, uint32_t colour)
{
	volatile uint32_t done = 0;
	uint32_t luma, chroma, *row, *end, length, size, address, i;

	if (!count)
		return;
	chroma = colour & 0xffff;
	luma = colour >> 16 & 0xff;
	luma |= luma << 8;
	luma |= luma << 16;
	chroma |= chroma << 16;
	row = (uint32_t *)(VIP_UNIT_RAM + VIP_UNIT_MACROBLOCK_SLOTS + VIP_UNIT_SLOT_FILL +
		vipReadControlBus(VIP_CB_MACROBLOCK_SLOT) * VIP_UNIT_MACROBLOCK_SLOT_SIZE);
	end = row + VIP_UNIT_SLOT_FILL_SIZE / 4;
	for (; row < end; row += FILL_ROW_WORDS) {
		row[0] = luma;
		row[1] = luma;
		row[2] = luma;
		row[3] = luma;
		for (i = 0; i < 4; i++)
			row[4 + i] = chroma;
		for (i = 0; i < 4; i++)
			row[8 + i] = luma;
	}

	if (!g_mbaffFrame) {
		if (!g_fieldPic) {
			size = FILL_FRAME_SIZE;
			length = FILL_FRAME_LENGTH;
			g_fillDescriptor.localLength = FILL_FRAME_DESCRIPTOR;
		} else {
			length = FILL_FIELD_LENGTH;
			size = FILL_FIELD_SIZE;
			g_fillDescriptor.localLength = FILL_FIELD_DESCRIPTOR;
		}
		for (i = 0; i < count; i++) {
			address = vipReadControlBus(VIP_CB_PICTURE_ADDRESS);
			vipWriteControlBus(VIP_CB_PICTURE_COMMAND, VIP_UNIT_START);
			address += FILL_FIELD_OFFSET;
			if (!g_fieldPic)
				address -= FILL_FIELD_OFFSET - FILL_FRAME_OFFSET;
			while (done == 1)
				;
			g_fillDescriptor.external = address;
			queueListDma(FILL_DMA, (uint32_t)&g_fillDescriptor << 16 |
				sizeof(g_fillDescriptor), length, size, NULL, &done);
		}
	} else {
		g_fillPair[1].localLength = FILL_PAIR_SECOND_DESCRIPTOR;
		g_fillPair[0].localLength = FILL_FRAME_DESCRIPTOR;
		for (i = 0; i < count; i++) {
			address = vipReadControlBus(VIP_CB_PICTURE_ADDRESS);
			vipWriteControlBus(VIP_CB_PICTURE_COMMAND, VIP_UNIT_START);
			while (done == 1)
				;
			g_fillPair[0].external = address + FILL_FIELD_OFFSET;
			g_fillPair[1].external = address + FILL_PAIR_SECOND_OFFSET;
			queueListDma(FILL_DMA, (uint32_t)g_fillPair << 16 | sizeof(g_fillPair),
				FILL_FRAME_LENGTH, FILL_FRAME_SIZE, NULL, &done);
		}
	}
	while (done == 1)
		;
}
