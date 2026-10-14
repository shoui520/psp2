/*
 * FW2: what the ARM side reports of a picture as it starts (g_frameInfo), PU1's
 * (fw1/pu1/picture.c updateFrameInfo) changed. The scan of a field picture that
 * pic_timing() gives as a single field waits for the second field, which decides
 * it by the fields' order counts, else by their output times. The sample aspect
 * ratio of Table E-1 goes into the SPS, and an I picture after an SPS is a random
 * access point.
 */
#include "codec.h"

#define SCAN_MASK		(0xfu << VIP_FRAME_REPEAT_FIRST_FIELD)
#define PROGRESSIVE_SEQUENCE	(1u << VIP_FRAME_PROGRESSIVE_SEQUENCE)
#define PROGRESSIVE_FRAME	(1u << VIP_FRAME_PROGRESSIVE_FRAME)
#define TOP_FIELD_FIRST		(1u << VIP_FRAME_TOP_FIELD_FIRST)
#define REPEAT_FIRST_FIELD	(1u << VIP_FRAME_REPEAT_FIRST_FIELD)
#define TYPE_MASK		(7u << VIP_FRAME_TYPE_SHIFT)
#define ASPECT_MASK		(0xfu << VIP_FRAME_ASPECT_SHIFT)
#define PICTURE_FLAGS		0xfffu	/* cleared for each picture */

#define EXTENDED_SAR		255

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

VipFrameInfo g_frameInfo VIP_GLOBAL;

/* a first field's pic_timing(), for its second field's scan */
int8_t g_firstFieldTimed VIP_GLOBAL;
uint32_t g_firstFieldOutputTime VIP_GLOBAL;	/* its cpb_removal_delay + dpb_output_delay */

/* the scan by pic_struct; -1 for one that keeps the scan */
static int32_t timedScan(void)
{
	switch (g_picStruct) {
	case VIP_PIC_STRUCT_FRAME:
		if (g_ctType == VIP_CT_TYPE_INTERLACED)
			return TOP_FIELD_FIRST;
		return PROGRESSIVE_SEQUENCE | PROGRESSIVE_FRAME;
	case VIP_PIC_STRUCT_TOP_FIELD:
	case VIP_PIC_STRUCT_BOTTOM_FIELD:
		g_firstFieldTimed = 1;
		g_firstFieldOutputTime = g_cpbRemovalDelay + g_dpbOutputDelay;
		return -1;
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

/* the scan without pic_struct */
static uint32_t codedScan(void)
{
	if (!g_sliceHeader.fieldPic) {
		if (!g_currentSps->frameMbsOnly)
			return TOP_FIELD_FIRST;
		return PROGRESSIVE_SEQUENCE | PROGRESSIVE_FRAME;
	}
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

/*
 * Ten times the display aspect ratio of width by height pixels, or 0; the
 * sample aspect ratio of an aspect_ratio_idc from the table goes into the SPS,
 * and one it does not know is 0:0.
 */
static int32_t displayAspect(VipSps *sps, uint32_t width, uint32_t height)
{
	uint32_t idc = sps->aspectRatioIdc;

	if (idc == EXTENDED_SAR) {
		if (!sps->sarHeight)
			return 0;
		return (int32_t)(width * 10 * sps->sarWidth) /
			(int32_t)(sps->sarHeight * height);
	}
	if (idc - 1 >= SAMPLE_ASPECT_RATIOS) {
		sps->sarWidth = 0;
		sps->sarHeight = 0;
		return 0;
	}
	sps->sarWidth = sampleAspectRatios[idc - 1].width;
	sps->sarHeight = sampleAspectRatios[idc - 1].height;
	return (int32_t)(width * 10 * sampleAspectRatios[idc - 1].width) /
		(int32_t)(height * sampleAspectRatios[idc - 1].height);
}

/* the MPEG-2 aspect ratio nearest it, or 0 */
static uint32_t nearestAspect(int32_t aspect)
{
	if (aspect >= 21 && aspect <= 23)
		return VIP_FRAME_ASPECT_221_100;
	if (aspect >= 16 && aspect <= 18)
		return VIP_FRAME_ASPECT_16_9;
	if (aspect >= 12 && aspect <= 14)
		return VIP_FRAME_ASPECT_4_3;
	if (aspect >= 9 && aspect <= 11)
		return VIP_FRAME_ASPECT_1_1;
	return 0;
}

/* g_frameInfo for a frame or a first field */
void updateFrameInfo(void)
{
	VipFrameInfo *info = &g_frameInfo;
	VipSps *sps = g_currentSps;
	uint32_t width, height, cropLeft, cropRight, cropTop, cropBottom, flags, type, i;
	int32_t scan;

	width = sps->picWidthInMbsMinus1 * 16 + 16;
	height = (sps->picHeightInMapUnitsMinus1 * 16 + 16) << !sps->frameMbsOnly;
	info->width = width;
	info->height = height;
	cropLeft = sps->frameCropLeft;
	cropRight = sps->frameCropRight;
	cropTop = sps->frameCropTop << !sps->frameMbsOnly;
	cropBottom = sps->frameCropBottom << !sps->frameMbsOnly;
	if (sps->chroma) {
		cropBottom <<= 1;
		cropTop <<= 1;
		cropRight <<= 1;
		cropLeft <<= 1;
	}
	info->cropLeft = cropLeft;
	info->cropRight = cropRight;
	info->cropTop = cropTop;
	info->cropBottom = cropBottom;

	/* a pan-scan rectangle the SEI did not give is the cropped picture */
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++) {
		if (!info->display[i].width)
			info->display[i].width = width - cropLeft - cropRight;
		if (!info->display[i].height)
			info->display[i].height = height - cropTop - cropBottom;
	}

	flags = info->flags;
	if (g_pictureTimingTaken && sps->picStructPresent)
		scan = timedScan();
	else
		scan = codedScan();
	if (scan >= 0)
		flags = (flags & ~SCAN_MASK) | scan;
	type = pictureType();
	if (type)
		flags = (flags & ~TYPE_MASK) | type << VIP_FRAME_TYPE_SHIFT;
	if (g_sliceHeader.reference)
		flags |= 1u << VIP_FRAME_NAL_REFERENCE;
	else
		flags &= ~(1u << VIP_FRAME_NAL_REFERENCE);
	if (g_sliceHeader.idr || g_recoveryPointTaken ||
	    (g_spsTaken && g_sliceType == VIP_SLICE_TYPE_I))
		flags |= 1u << VIP_FRAME_RANDOM_ACCESS;
	else
		flags &= ~(1u << VIP_FRAME_RANDOM_ACCESS);
	flags = (flags & ~ASPECT_MASK) | nearestAspect(displayAspect(sps,
		info->display[0].width, info->display[0].height)) << VIP_FRAME_ASPECT_SHIFT;
	info->flags = flags & ~PICTURE_FLAGS;
}

/*
 * A second field's: when pic_timing() gave the pair as single fields, the top
 * field is first when its order count is lower; when the counts are the same,
 * when the field output first is the top one, by this field's output time
 * against the first's (cpb_removal_delay counts only outside a buffering
 * period). Otherwise the first field's scan stands.
 */
void updateSecondFieldInfo(void)
{
	const VipPictureBuffer *buffer;
	uint32_t time;
	int32_t top, bottom;
	int topFirst;

	if (!g_pictureTimingTaken || !g_currentSps->picStructPresent)
		return;
	if (g_picStruct != VIP_PIC_STRUCT_TOP_FIELD && g_picStruct != VIP_PIC_STRUCT_BOTTOM_FIELD)
		return;

	buffer = g_currentBuffer;
	top = buffer->topPicOrderCnt;
	bottom = buffer->bottomPicOrderCnt;
	if (top < bottom) {
		topFirst = 1;
	} else if (top != bottom) {
		topFirst = 0;
	} else if (!g_firstFieldTimed) {
		/* this field's parity is first */
		topFirst = g_picStruct == VIP_PIC_STRUCT_TOP_FIELD;
	} else {
		time = g_dpbOutputDelay;
		if (!g_bufferingPeriodTaken)
			time += g_cpbRemovalDelay;
		/* this field before the first: this one's parity is first */
		if (time < g_firstFieldOutputTime)
			topFirst = g_picStruct == VIP_PIC_STRUCT_TOP_FIELD;
		else
			topFirst = g_picStruct == VIP_PIC_STRUCT_BOTTOM_FIELD;
	}
	if (topFirst)
		g_frameInfo.flags |= TOP_FIELD_FIRST;
}
