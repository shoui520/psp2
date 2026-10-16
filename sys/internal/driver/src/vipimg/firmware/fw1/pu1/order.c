/*
 * The picture order count (H.264 8.2.1), both codecs' (PU1's, and FW2's codec).
 * The update functions work out the picture being decoded's and keep the state
 * the next picture's needs; PU1's peek functions work it out without keeping
 * anything, and for a field also the other field's, from g_otherFieldOrderCnt.
 * Both reproduce the firmware's departures from the standard, noted where they
 * are. FW2 has no peek functions.
 */
#ifdef VIP_FW2
#include "../../fw2/codec/codec.h"

uint32_t g_prevFrameNum VIP_GLOBAL;
uint32_t g_frameNumOffset VIP_GLOBAL;	/* FrameNumOffset, kept as prevFrameNumOffset */
int32_t g_prevPicOrderCntMsb VIP_GLOBAL;
uint32_t g_prevPicOrderCntLsb VIP_GLOBAL;
#else
#include "codec.h"

uint32_t g_prevFrameNum;
uint32_t g_frameNumOffset;		/* FrameNumOffset, kept as prevFrameNumOffset */
int32_t g_prevPicOrderCntMsb;
uint32_t g_prevPicOrderCntLsb;
/* the other field's pic_order_cnt_lsb (type 0) or delta_pic_order_cnt[0] (type 1) */
uint16_t g_otherFieldOrderCnt;
#endif

/* PicOrderCnt(): the current field's own, a frame's the smaller of its fields' */
int32_t picOrderCnt(const VipPictureBuffer *buffer)
{
	if (g_fieldPic && buffer == g_currentBuffer)
		return g_bottomField ? buffer->bottomPicOrderCnt : buffer->topPicOrderCnt;
	if (buffer->topPicOrderCnt < buffer->bottomPicOrderCnt)
		return buffer->topPicOrderCnt;
	return buffer->bottomPicOrderCnt;
}

/* the current field's opposite parity; a frame's the smaller of its fields' */
int32_t oppositePicOrderCnt(const VipPictureBuffer *buffer)
{
	if (g_fieldPic && buffer == g_currentBuffer)
		return g_bottomField ? buffer->topPicOrderCnt : buffer->bottomPicOrderCnt;
	if (buffer->topPicOrderCnt < buffer->bottomPicOrderCnt)
		return buffer->topPicOrderCnt;
	return buffer->bottomPicOrderCnt;
}

/* PicOrderCntMsb (8-3) */
static int32_t picOrderCntMsb(uint32_t lsb, uint32_t prevLsb, int32_t prevMsb,
	int32_t maxLsb)
{
	int32_t half = maxLsb >> 1;

	if (lsb < prevLsb && prevLsb - lsb >= half)
		return prevMsb + maxLsb;
	if (prevLsb < lsb && lsb - prevLsb > half)
		return prevMsb - maxLsb;
	return prevMsb;
}

/*
 * pic_order_cnt_type 0 (8.2.1.1). prevPicOrderCntMsb and Lsb are the last
 * reference picture's; memory_management_control_operation 5 is not looked at.
 */
static void updatePicOrderCnt0(int32_t *top, int32_t *bottom)
{
	const VipSliceHeader *slice = &g_sliceHeader;
	int32_t maxLsb = 1 << (g_currentSps->log2MaxPicOrderCntLsbMinus4 + 4);
	int32_t msb;

	if (slice->idr) {
		g_prevPicOrderCntMsb = 0;
		g_prevPicOrderCntLsb = 0;
	}
	msb = picOrderCntMsb(slice->picOrderCntLsb, g_prevPicOrderCntLsb,
		g_prevPicOrderCntMsb, maxLsb);
	if (!slice->fieldPic || !slice->bottomField)
		*top = msb + slice->picOrderCntLsb;
	if (!slice->fieldPic)
		*bottom = *top + slice->deltaPicOrderCntBottom;
	else if (slice->bottomField)
		*bottom = msb + slice->picOrderCntLsb;
	if (slice->reference) {
		g_prevPicOrderCntMsb = msb;
		g_prevPicOrderCntLsb = slice->picOrderCntLsb;
	}
	g_prevFrameNum = slice->frameNum;
}

/* FrameNumOffset (8-6, 8-11): kept in g_frameNumOffset */
static uint32_t updateFrameNumOffset(void)
{
	uint32_t maxFrameNum = 1 << (g_currentSps->log2MaxFrameNumMinus4 + 4);

	if (g_sliceHeader.idr)
		g_frameNumOffset = 0;
	else if (g_sliceHeader.frameNum < g_prevFrameNum)
		g_frameNumOffset += maxFrameNum;
	return g_frameNumOffset;
}

#ifndef VIP_FW2
/*
 * FrameNumOffset without keeping it. After frame_num wrapped it is MaxFrameNum,
 * not prevFrameNumOffset + MaxFrameNum.
 */
static uint32_t peekFrameNumOffset(void)
{
	uint32_t maxFrameNum = 1 << (g_currentSps->log2MaxFrameNumMinus4 + 4);

	if (g_sliceHeader.idr)
		return 0;
	if (g_sliceHeader.frameNum < g_prevFrameNum)
		return maxFrameNum;
	return g_frameNumOffset;
}

#endif

/* expectedPicOrderCnt of pic_order_cnt_type 1 (8-7 to 8-10) */
static int32_t expectedPicOrderCnt(uint32_t frameNumOffset)
{
	const VipSps *sps = g_currentSps;
	uint32_t absFrameNum = 0, cycleCnt = 0, frameNumInCycle = 0, i;
	int32_t expectedDeltaPerCycle = 0, expected;

	if (sps->numRefFramesInPicOrderCntCycle)
		absFrameNum = frameNumOffset + g_sliceHeader.frameNum;
	if (!g_sliceHeader.reference && absFrameNum)
		absFrameNum--;
	if (absFrameNum && sps->numRefFramesInPicOrderCntCycle)
		cycleCnt = hardwareDivide(absFrameNum - 1,
			sps->numRefFramesInPicOrderCntCycle, &frameNumInCycle);
	for (i = 0; i < sps->numRefFramesInPicOrderCntCycle; i++)
		expectedDeltaPerCycle += sps->offsetForRefFrame[i];
	expected = 0;
	if (absFrameNum) {
		expected = cycleCnt * expectedDeltaPerCycle;
		for (i = 0; i <= frameNumInCycle; i++)
			expected += sps->offsetForRefFrame[i];
	}
	if (!g_sliceHeader.reference)
		expected += sps->offsetForNonRefPic;
	return expected;
}

/* pic_order_cnt_type 1 (8.2.1.2) */
static void updatePicOrderCnt1(int32_t *top, int32_t *bottom)
{
	const VipSliceHeader *slice = &g_sliceHeader;
	int32_t expected = expectedPicOrderCnt(updateFrameNumOffset());

	if (!slice->fieldPic) {
		*top = expected + slice->deltaPicOrderCnt[0];
		*bottom = *top + g_currentSps->offsetForTopToBottomField +
			slice->deltaPicOrderCnt[1];
	} else if (!slice->bottomField) {
		*top = expected + slice->deltaPicOrderCnt[0];
	} else {
		*bottom = expected + g_currentSps->offsetForTopToBottomField +
			slice->deltaPicOrderCnt[0];
	}
	g_prevFrameNum = slice->frameNum;
}

/* tempPicOrderCnt of pic_order_cnt_type 2 (8-12) */
static int32_t tempPicOrderCnt(uint32_t frameNumOffset)
{
	int32_t temp;

	if (g_sliceHeader.idr)
		return 0;
	temp = 2 * (frameNumOffset + g_sliceHeader.frameNum);
	if (!g_sliceHeader.reference)
		temp--;
	return temp;
}

/* pic_order_cnt_type 2 (8.2.1.3) */
static void updatePicOrderCnt2(int32_t *top, int32_t *bottom)
{
	const VipSliceHeader *slice = &g_sliceHeader;
	int32_t temp = tempPicOrderCnt(updateFrameNumOffset());

	if (!slice->fieldPic) {
		*top = temp;
		*bottom = temp;
	} else if (slice->bottomField) {
		*bottom = temp;
	} else {
		*top = temp;
	}
	g_prevFrameNum = slice->frameNum;
}

#ifndef VIP_FW2
/*
 * pic_order_cnt_type 0, peeked; a field gets the other field's too. Its
 * PicOrderCntMsb takes MaxPicOrderCntLsb off when the current field's lsb, not
 * the other's, is at least half of it past prevPicOrderCntLsb. A
 * memory_management_control_operation 5 clears the field pic_struct names.
 */
static void peekPicOrderCnt0(int32_t *top, int32_t *bottom)
{
	const VipSliceHeader *slice = &g_sliceHeader;
	int32_t maxLsb = 1 << (g_currentSps->log2MaxPicOrderCntLsbMinus4 + 4);
	int32_t half = maxLsb >> 1, prevMsb = 0, msb, order;
	uint32_t prevLsb = 0, i;

	if (!slice->idr) {
		prevMsb = g_prevPicOrderCntMsb;
		prevLsb = g_prevPicOrderCntLsb;
	}
	order = picOrderCntMsb(slice->picOrderCntLsb, prevLsb, prevMsb, maxLsb) +
		slice->picOrderCntLsb;
	if (!slice->fieldPic) {
		*top = order;
		*bottom = order + slice->deltaPicOrderCntBottom;
		return;
	}
	*(slice->bottomField ? bottom : top) = order;

	/* an IDR other field counts from 0, and in the firmware one after a recovery point */
	if (g_otherFieldRandomAccess) {
		prevMsb = 0;
		prevLsb = 0;
	}
	msb = prevMsb;
	if (g_otherFieldOrderCnt < prevLsb) {
		if (prevLsb - g_otherFieldOrderCnt >= half)
			msb = prevMsb + maxLsb;
	} else if (prevLsb < g_otherFieldOrderCnt) {
		if (slice->picOrderCntLsb - prevLsb >= half)
			msb = prevMsb - maxLsb;
	}
	*(slice->bottomField ? top : bottom) = msb + g_otherFieldOrderCnt;

	if (!slice->adaptiveMarking)
		return;
	for (i = 0; i < VIP_MARKING_OPERATIONS && slice->operation[i]; i++) {
		if (slice->operation[i] != VIP_MMCO_FORGET_ALL)
			continue;
		if (g_picStruct == VIP_PIC_STRUCT_TOP_FIELD)
			*top = 0;
		else if (g_picStruct == VIP_PIC_STRUCT_BOTTOM_FIELD)
			*bottom = 0;
	}
}

/* pic_order_cnt_type 1, peeked; a field gets the other field's too */
static void peekPicOrderCnt1(int32_t *top, int32_t *bottom)
{
	const VipSliceHeader *slice = &g_sliceHeader;
	int32_t expected = expectedPicOrderCnt(peekFrameNumOffset());
	int32_t topToBottom = g_currentSps->offsetForTopToBottomField;

	if (!slice->fieldPic) {
		*top = expected + slice->deltaPicOrderCnt[0];
		*bottom = *top + topToBottom + slice->deltaPicOrderCnt[1];
	} else if (!slice->bottomField) {
		*top = expected + slice->deltaPicOrderCnt[0];
		*bottom = *top + topToBottom + g_otherFieldOrderCnt;
	} else {
		*top = expected + g_otherFieldOrderCnt;
		*bottom = expected + slice->deltaPicOrderCnt[0] + topToBottom;
	}
}

/* pic_order_cnt_type 2, peeked: both fields have it */
static void peekPicOrderCnt2(int32_t *top, int32_t *bottom)
{
	int32_t temp = tempPicOrderCnt(peekFrameNumOffset());

	*top = temp;
	*bottom = temp;
}
#endif

/*
 * The picture's TopFieldOrderCnt and BottomFieldOrderCnt (8.2.1). An IDR picture
 * starts a generation, and so does a memory_management_control_operation 5: in
 * g_generation's top bit in PU1, as one more generation in FW2.
 */
void updatePicOrderCnt(int32_t *top, int32_t *bottom)
{
	const VipSliceHeader *slice = &g_sliceHeader;
	uint32_t i;

	if (slice->idr)
		g_generation++;
	switch (g_currentSps->picOrderCntType) {
	case 0:
		updatePicOrderCnt0(top, bottom);
		break;
	case 2:
		updatePicOrderCnt2(top, bottom);
		break;
	default:
		updatePicOrderCnt1(top, bottom);
		break;
	}
	if (!slice->adaptiveMarking)
		return;
	for (i = 0; i < VIP_MARKING_OPERATIONS && slice->operation[i]; i++)
		if (slice->operation[i] == VIP_MMCO_FORGET_ALL)
#ifdef VIP_FW2
			g_generation++;
#else
			g_generation += 0x80000000;
#endif
}

#ifndef VIP_FW2
/* the picture being decoded's TopFieldOrderCnt and BottomFieldOrderCnt, peeked */
void peekPicOrderCnt(int32_t *top, int32_t *bottom)
{
	switch (g_currentSps->picOrderCntType) {
	case 0:
		peekPicOrderCnt0(top, bottom);
		break;
	case 2:
		peekPicOrderCnt2(top, bottom);
		break;
	default:
		peekPicOrderCnt1(top, bottom);
		break;
	}
}

/* the picture being decoded's PicOrderCnt, without keeping anything */
int32_t currentPicOrderCnt(void)
{
	int32_t top, bottom;

	peekPicOrderCnt(&top, &bottom);
	if (g_sliceHeader.fieldPic && g_sliceHeader.bottomField)
		return bottom;
	return top;
}
#endif

/* FrameNumWrap (8-27) of a short-term reference's FrameNum */
int32_t frameNumWrap(uint32_t frameNum)
{
	uint32_t maxFrameNum = 1 << (g_currentSps->log2MaxFrameNumMinus4 + 4);

	if (g_sliceHeader.frameNum < frameNum)
		frameNum -= maxFrameNum;
	return frameNum;
}
