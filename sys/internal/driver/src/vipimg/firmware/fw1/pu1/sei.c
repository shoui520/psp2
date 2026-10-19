/*
 * PU1: the VUI and SEI records. What the decoder needs of them it takes (the
 * frame rate, colour, pan-scan rectangles, picture timing, recovery points); the
 * messages themselves go with the picture in its extension (extension.c), and
 * user data goes out to the user data rings for the ARM side.
 *
 * FW2's codec takes the same, but keeps the messages itself (command.c saves
 * each record into g_sei); its user data goes to the context's user data area,
 * and a recovery point counts only where decoding starts.
 */
#ifdef VIP_FW2
#include "../../fw2/codec/codec.h"
#else
#include "codec.h"
#endif

/* the VUI's words (common/parsed.h VIP_VUI_FIRST on), as pu0/decoder.c parseVui writes them */
#define VUI_FLAGS			0
#define VUI_ASPECT_RATIO_INFO		31
#define VUI_ASPECT_RATIO_IDC_SHIFT	23
#define VUI_CHROMA_LOC_INFO		20
#define VUI_CHROMA_LOC_TOP_SHIFT	17	/* chroma_sample_loc_type_top_field, 3 bits */
#define VUI_CHROMA_LOC_BOTTOM_SHIFT	14
#define VUI_TIMING_INFO			13
#define VUI_FIXED_FRAME_RATE		12
#define VUI_PIC_STRUCT_PRESENT		10
#define VUI_SAR				1	/* sar_width << 16 | sar_height */
#define VUI_VIDEO_SIGNAL		2
#define VUI_VIDEO_SIGNAL_TYPE		31
#define VUI_COLOUR_DESCRIPTION		26
#define VUI_COLOUR_PRIMARIES_SHIFT	18
#define VUI_TRANSFER_SHIFT		10
#define VUI_MATRIX_SHIFT		2
#define VUI_NUM_UNITS_IN_TICK		3
#define VUI_TIME_SCALE			4

/* the frame rates of VipFrameInfo.flags, an MPEG frame_rate_code */
#define RATE_23_976		1
#define RATE_24			2
#define RATE_25			3
#define RATE_29_97		4
#define RATE_30			5
#define RATE_50			6
#define RATE_59_94		7
#define RATE_60			8
#define RATE_MASK		(0xfu << VIP_FRAME_RATE_SHIFT)

/* pan_scan_rect()'s words */
#define PAN_SCAN_FLAGS			1
#define PAN_SCAN_CANCEL			31	/* pan_scan_rect_cancel_flag */
#define PAN_SCAN_COUNT_SHIFT		29	/* pan_scan_cnt_minus1 */
#define PAN_SCAN_RECTANGLES		2	/* left, right, top, bottom offsets, each */

/* recovery_point()'s word */
#define RECOVERY_FRAME_COUNT_SHIFT	16
#define RECOVERY_BROKEN_LINK		14

#ifdef VIP_FW2
/* VipSeiMessages.userData, and the messages after it: registered << 30 | bytes */
#define USER_DATA_OVERFLOW		31	/* more messages than VIP_USER_DATA_MESSAGES */
#define USER_DATA_TRUNCATED		30	/* more than the slot holds */
#define USER_DATA_COUNT_SHIFT		26
#define USER_DATA_COUNT_MASK		0xf
#define USER_DATA_REGISTERED_SHIFT	30
#define USER_DATA_BYTES			0x3fffffff
#define USER_DATA_UNIT			8	/* VIP_USER_DATA_SIZE's */
#define USER_DATA_READ_DMA		(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ)
#define USER_DATA_WRITE_DMA		(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_WRITE)

int8_t g_bufferingPeriodTaken VIP_GLOBAL;
int8_t g_recoveryPointTaken VIP_GLOBAL;
int8_t g_pictureTimingTaken VIP_GLOBAL;
uint8_t g_picStruct VIP_GLOBAL;
uint8_t g_lastPicStruct VIP_GLOBAL;
uint32_t g_recoveryFrameCount VIP_GLOBAL;
uint32_t g_cpbRemovalDelay VIP_GLOBAL;
uint32_t g_dpbOutputDelay VIP_GLOBAL;
uint8_t g_ctType VIP_GLOBAL;
#else
/* the user data rings' records: two for each SEI message, its start and end */
#define USER_DATA_REGISTERED		0x4300
#define USER_DATA_UNREGISTERED		0x4301
#define USER_DATA_END			0x4800
#define USER_DATA_RECORDS		2

uint32_t g_cpbRemovalDelay;
uint32_t g_dpbOutputDelay;
VipUserDataRings g_userData;
#endif

#ifdef VIP_FW2
/* The display information is the stream's again: nothing from the SPS or SEI. */
void resetFrameInfo(void)
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
	info->colour = VIP_COLOUR_UNSPECIFIED;
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++) {
		info->offset[i].x = 0;
		info->offset[i].y = 0;
	}
	info->flags = 0;
}
#else

/* The VUI's words go with the SPS, for the pictures' extensions. */
static void keepVuiWords(void)
{
	VipSps *sps = g_currentSps;
	uint32_t i;

	sps->vuiPresent = 1;
	for (i = 0; i < VIP_VUI_WORDS; i++)
		sps->vui[i] = g_parsed[VIP_VUI_FIRST + i];
	for (i = 0; i < VIP_HRD_WORDS; i++) {
		sps->nalHrd[i] = g_parsed[VIP_VUI_NAL_HRD + i];
		sps->vclHrd[i] = g_parsed[VIP_VUI_VCL_HRD + i];
	}
}

#endif

/* frame_rate_code of time_scale / num_units_in_tick / 2, if it is one */
static uint32_t frameRate(uint32_t timeScale, uint32_t numUnitsInTick)
{
	switch (timeScale / numUnitsInTick >> 1) {
	case 23:
		return RATE_23_976;
	case 24:
		return RATE_24;
	case 25:
		return RATE_25;
	case 29:
		return RATE_29_97;
	case 30:
		return RATE_30;
	case 50:
		return RATE_50;
	case 59:
		return RATE_59_94;
	case 60:
		return RATE_60;
	}
	return 0;
}

/* A VIP_PARSED_VUI record: the current SPS's vui_parameters(). */
void takeVui(void)
{
	const uint32_t *vui = &g_parsed[VIP_VUI_FIRST];
	VipSps *sps;
	uint32_t numUnitsInTick;
#ifdef VIP_FW2
	uint32_t rate;
#endif

	readParsed();
	sps = g_currentSps;
	sps->nalHrdPresent = g_parsed[VIP_VUI_NAL_HRD] >> 31 & 1;
	sps->vclHrdPresent = g_parsed[VIP_VUI_VCL_HRD] >> 31 & 1;
	if (vui[VUI_FLAGS] >> VUI_ASPECT_RATIO_INFO) {
		sps->aspectRatioIdc = vui[VUI_FLAGS] >> VUI_ASPECT_RATIO_IDC_SHIFT;
		if (sps->aspectRatioIdc == EXTENDED_SAR) {
			sps->sarWidth = vui[VUI_SAR] >> 16;
			sps->sarHeight = vui[VUI_SAR];
		}
	} else {
		sps->aspectRatioIdc = 0;
	}
	/* the original tests every flag from chroma_loc_info_present_flag up */
	if (vui[VUI_FLAGS] >> VUI_CHROMA_LOC_INFO) {
		sps->chromaSampleLocTypeTopField = vui[VUI_FLAGS] >> VUI_CHROMA_LOC_TOP_SHIFT & 7;
		sps->chromaSampleLocTypeBottomField =
			vui[VUI_FLAGS] >> VUI_CHROMA_LOC_BOTTOM_SHIFT & 7;
	} else {
		sps->chromaSampleLocTypeTopField = 0;
		sps->chromaSampleLocTypeBottomField = 0;
	}
	sps->picStructPresent = vui[VUI_FLAGS] >> VUI_PIC_STRUCT_PRESENT & 1;
	if (vui[VUI_VIDEO_SIGNAL] >> VUI_VIDEO_SIGNAL_TYPE &&
	    vui[VUI_VIDEO_SIGNAL] >> VUI_COLOUR_DESCRIPTION & 1)
		g_frameInfo.colour =
			(vui[VUI_VIDEO_SIGNAL] >> VUI_COLOUR_PRIMARIES_SHIFT & 0xff) <<
				VIP_FRAME_COLOUR_PRIMARIES_SHIFT |
			(vui[VUI_VIDEO_SIGNAL] >> VUI_TRANSFER_SHIFT & 0xff) <<
				VIP_FRAME_TRANSFER_SHIFT |
			(vui[VUI_VIDEO_SIGNAL] >> VUI_MATRIX_SHIFT & 0xff) <<
				VIP_FRAME_MATRIX_SHIFT;

#ifdef VIP_FW2
	/*
	 * FW2 marks a frame rate it does not know with VIP_FRAME_SKIPPED's bit, as
	 * the firmware has it; updateFrameInfo clears it before the picture's slices.
	 */
	if (vui[VUI_FLAGS] >> VUI_TIMING_INFO & 1) {
		numUnitsInTick = vui[VUI_NUM_UNITS_IN_TICK];
		sps->numUnitsInTick = numUnitsInTick;
		sps->timeScale = vui[VUI_TIME_SCALE];
		sps->fixedFrameRate = vui[VUI_FLAGS] >> VUI_FIXED_FRAME_RATE & 1;
		rate = numUnitsInTick ? frameRate(vui[VUI_TIME_SCALE], numUnitsInTick) : 0;
		g_frameInfo.flags &= ~(RATE_MASK | 1u << VIP_FRAME_SKIPPED);
		if (rate)
			g_frameInfo.flags |= rate << VIP_FRAME_RATE_SHIFT;
		else
			g_frameInfo.flags |= 1u << VIP_FRAME_SKIPPED;
	} else {
		g_frameInfo.flags = (g_frameInfo.flags & ~RATE_MASK) | 1u << VIP_FRAME_SKIPPED;
		sps->fixedFrameRate = 0;
		sps->timeScale = 0;
		sps->numUnitsInTick = 0;
	}
#else
	if (vui[VUI_FLAGS] >> VUI_TIMING_INFO & 1) {
		numUnitsInTick = vui[VUI_NUM_UNITS_IN_TICK];
		sps->numUnitsInTick = numUnitsInTick;
		sps->timeScale = vui[VUI_TIME_SCALE];
		sps->fixedFrameRate = vui[VUI_FLAGS] >> VUI_FIXED_FRAME_RATE & 1;
		g_frameInfo.flags &= ~RATE_MASK;
		if (numUnitsInTick)
			g_frameInfo.flags |= frameRate(vui[VUI_TIME_SCALE], numUnitsInTick) <<
				VIP_FRAME_RATE_SHIFT;
	} else {
		g_frameInfo.flags &= ~RATE_MASK;
		sps->numUnitsInTick = 0;
		sps->timeScale = 0;
		sps->fixedFrameRate = 0;
	}
	keepVuiWords();
#endif
}

/* A VIP_PARSED_PICTURE_TIMING record: pic_timing(). */
void takePictureTiming(void)
{
	readParsed();
	g_pictureTimingTaken = 1;
	g_cpbRemovalDelay = g_parsed[VIP_TIMING_CPB_REMOVAL_DELAY];
	g_dpbOutputDelay = g_parsed[VIP_TIMING_DPB_OUTPUT_DELAY];
	g_picStruct = g_parsed[VIP_TIMING_PIC_STRUCT] >> VIP_TIMING_PIC_STRUCT_SHIFT & 0xf;
	if (g_parsed[VIP_TIMING_PIC_STRUCT] >> VIP_TIMING_CLOCK_TIMESTAMP & 1)
		g_ctType = g_parsed[VIP_TIMING_CLOCK] >> VIP_TIMING_CT_TYPE_SHIFT & 3;
	else
		g_ctType = VIP_CT_TYPE_UNKNOWN;
#ifndef VIP_FW2
	keepPictureTiming();
#endif
}

/*
 * One pan-scan rectangle into g_frameInfo: its size in pixels, less the SPS's
 * cropping, and its centre's offset, in 1/16 pixels.
 */
static void takePanScanRectangle(uint32_t i, const uint32_t *offsets, uint32_t cropX,
	uint32_t cropY)
{
	VipSps *sps = g_currentSps;
	uint32_t left = offsets[0], right = offsets[1], top = offsets[2], bottom = offsets[3];

	g_frameInfo.display[i].width = ((right - left) >> 4) - cropX +
		(sps->picWidthInMbsMinus1 * 16 + 16);
	g_frameInfo.display[i].height = ((bottom - top) >> 4) +
		(sps->picHeightInMapUnitsMinus1 * 16 + 16) * (2 - sps->frameMbsOnly) - cropY;
	g_frameInfo.offset[i].x = -((left + right) >> 1);
	g_frameInfo.offset[i].y = -((top + bottom) >> 1);
}

/* A VIP_PARSED_PAN_SCAN record: pan_scan_rect(); a rectangle not given repeats the last. */
void takePanScan(void)
{
	const uint32_t *words = g_parsed;
	VipSps *sps;
	uint32_t count, cropX, cropY, i;

	readParsed();
#ifndef VIP_FW2
	keepPanScan();
#endif
	if (words[PAN_SCAN_FLAGS] >> PAN_SCAN_CANCEL & 1) {
		for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++) {
			g_frameInfo.display[i].width = 0;
			g_frameInfo.display[i].height = 0;
			g_frameInfo.offset[i].x = 0;
			g_frameInfo.offset[i].y = 0;
		}
		return;
	}
	count = words[PAN_SCAN_FLAGS] >> PAN_SCAN_COUNT_SHIFT & 3;
	sps = g_currentSps;
	cropX = sps->frameCropLeft + sps->frameCropRight;
	cropY = sps->frameCropTop + sps->frameCropBottom;
	if (sps->chroma) {
		cropX *= 2;
		cropY *= 2;
	}
	if (!sps->frameMbsOnly)
		cropY *= 2;
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++) {
		if (i > count) {
			g_frameInfo.display[i] = g_frameInfo.display[i - 1];
			g_frameInfo.offset[i] = g_frameInfo.offset[i - 1];
			continue;
		}
		takePanScanRectangle(i, &words[PAN_SCAN_RECTANGLES + 4 * i], cropX, cropY);
	}
}

/* A VIP_PARSED_BUFFERING_PERIOD record: one with an HRD to go by. */
void takeBufferingPeriod(void)
{
	readParsed();
	if (g_currentSps->nalHrdPresent || g_currentSps->vclHrdPresent)
		g_bufferingPeriodTaken = 1;
#ifndef VIP_FW2
	keepBufferingPeriod();
#endif
}

/* A VIP_PARSED_RECOVERY_POINT record: decoding may start here. */
#ifdef VIP_FW2
/*
 * FW2: the recovery frame count is taken only where decoding starts, or where a
 * broken link follows an IDR picture.
 */
void takeRecoveryPoint(uint32_t ignoreBrokenLink)
{
	uint32_t brokenLink = 0;

	readParsed();
	if (!ignoreBrokenLink)
		brokenLink = g_parsed[0] >> RECOVERY_BROKEN_LINK & 1;
	if (g_randomAccess == RANDOM_ACCESS_NONE) {
		g_randomAccess = RANDOM_ACCESS_RECOVERY;
		g_recoveryFrameCount = g_parsed[0] >> RECOVERY_FRAME_COUNT_SHIFT;
	} else if (g_randomAccess == RANDOM_ACCESS_IDR && brokenLink) {
		g_randomAccess = RANDOM_ACCESS_BROKEN_LINK;
		g_recoveryFrameCount = g_parsed[0] >> RECOVERY_FRAME_COUNT_SHIFT;
	}
	g_recoveryPointTaken = 1;
}

/* A VIP_PARSED_FRAME_PACKING record: command.c keeps it. */
void takeFramePacking(void)
{
	readParsed();
}

/*
 * A VIP_PARSED_USER_DATA or VIP_PARSED_USER_DATA_UUID record of bytes, still in
 * the slice ring at g_dataStart: listed in *userData (VipSeiMessages.userData)
 * and copied, through the staging buffer, after what the user data area's slot
 * holds; as much as fits.
 */
void takeUserData(uint32_t *userData, uint32_t registered, uint32_t bytes)
{
	volatile uint32_t done = 0;
	const VipRingSegment *segment;
	VipDmaDescriptor *list;
	uint32_t count = *userData >> USER_DATA_COUNT_SHIFT & USER_DATA_COUNT_MASK;
	uint32_t used, size, to, from, chunk, taken, length, end, i;

	if (count == VIP_USER_DATA_MESSAGES) {
		*userData |= 1u << USER_DATA_OVERFLOW;
		return;
	}
	userData[1 + count] = registered << USER_DATA_REGISTERED_SHIFT | (bytes & USER_DATA_BYTES);
	*userData = (*userData & ~(USER_DATA_COUNT_MASK << USER_DATA_COUNT_SHIFT)) |
		((count + 1) & USER_DATA_COUNT_MASK) << USER_DATA_COUNT_SHIFT;

	used = (*userData & VIP_USER_DATA_SIZE) * USER_DATA_UNIT;
	size = (bytes + USER_DATA_UNIT - 1) & ~(USER_DATA_UNIT - 1);
	to = g_userDataArea + used;
	if (g_userDataSlot < used + size) {
		size = g_userDataSlot - used;
		*userData |= 1u << USER_DATA_TRUNCATED;
		if (!size)
			return;
	}
	*userData = (*userData & ~VIP_USER_DATA_SIZE) |
		((used + size) / USER_DATA_UNIT & VIP_USER_DATA_SIZE);

	from = g_dataStart;
	do {
		chunk = size < VIP_STAGING_SIZE ? size : VIP_STAGING_SIZE;
		list = g_streamList;
		taken = 0;
		for (i = 0; i < g_sliceSegmentCount; i++) {
			segment = &g_sliceSegments[i];
			if (from < segment->start)
				continue;
			end = segment->start + segment->size;
			length = chunk - taken;
			if (end - from < length) {
				length = end - from;
				if (length & (USER_DATA_UNIT - 1))
					length += USER_DATA_UNIT - (length & (USER_DATA_UNIT - 1));
			}
			list->external = from;
			list->localLength = (VIP_STAGING + taken) << 16 | length;
			list++;
			taken += length;
			from += length;
			if (from == end && !i && g_sliceSegmentCount != 1)
				from = g_sliceSegments[1].start;
			if (taken >= chunk)
				break;
		}
		queueListDma(USER_DATA_READ_DMA, (uint32_t)g_streamList << 16 |
			(uint32_t)((char *)list - (char *)g_streamList), 0, 0, NULL, NULL);
		g_codecDma.staging[0].external = to;
		g_codecDma.staging[0].localLength = VIP_STAGING << 16 | taken;
		queueListDma(USER_DATA_WRITE_DMA, VIP_STAGING_LIST(0), 0, 0, NULL, &done);
		size -= taken;
		to += taken;
		while (done == 1)
			;
	} while (size);
	g_dataStart = from;
}
#else
void takeRecoveryPoint(uint32_t ignoreBrokenLink)
{
	uint32_t brokenLink = 0;

	readParsed();
	g_recoveryFrameCount = g_parsed[0] >> RECOVERY_FRAME_COUNT_SHIFT;
	if (!ignoreBrokenLink)
		brokenLink = g_parsed[0] >> RECOVERY_BROKEN_LINK & 1;
	if (g_randomAccess == RANDOM_ACCESS_NONE)
		g_randomAccess = RANDOM_ACCESS_RECOVERY;
	else if (brokenLink)
		g_randomAccess = RANDOM_ACCESS_BROKEN_LINK;
	g_recoveryPointTaken = 1;
	keepRecoveryPoint();
}

/* the space of segments */
static uint32_t segmentSpace(const VipRingSegment *segments, uint32_t count)
{
	uint32_t space = 0, i;

	for (i = 0; i < count; i++)
		space += segments[i].size;
	return space;
}

/* the next word of a ring's two segments */
static uint32_t nextWord(uint32_t address, const VipRingSegment *segments)
{
	address += 4;
	if (address == segments[0].start + segments[0].size)
		address = segments[1].start;
	return address;
}

/*
 * A VIP_PARSED_USER_DATA or VIP_PARSED_USER_DATA_UUID record: copy the message,
 * still in the slice ring at the read stream's cursor, to the user data ring, with
 * a record of where it starts (and the record's PTS) and one of where it ends;
 * -1 if the rings have no room.
 */
int takeUserData(uint32_t type, uint32_t bits, uint32_t flags, uint32_t pts)
{
	VipUserDataRings *rings = &g_userData;
	uint32_t bytes = bits >> 3, size, end, record, from, to, i;

	size = bytes + 8 - (bytes & 7);
	if (!g_ringHandles[VIP_RING_USER_DATA] || !g_ringHandles[VIP_RING_USER_RECORDS])
		return 0;
	if (segmentSpace(rings->data, rings->dataCount) < rings->dataUsed + size)
		return -1;
	if (segmentSpace(rings->records, rings->recordCount) <
	    rings->recordsUsed + USER_DATA_RECORDS * sizeof(VipRecord))
		return -1;

	/* the message's record */
	record = rings->recordCursor;
	if (type == VIP_PARSED_USER_DATA) {
		g_userDataTaken = 1;
		*(uint32_t *)record = USER_DATA_REGISTERED << VIP_RECORD_TYPE_SHIFT;
	} else {
		g_uuidDataTaken = 1;
		*(uint32_t *)record = USER_DATA_UNREGISTERED << VIP_RECORD_TYPE_SHIFT;
	}
	if (flags & VIP_RECORD_PTS >> VIP_RECORD_FLAGS_SHIFT) {
		*(uint32_t *)record |= (flags & (VIP_RECORD_PTS | VIP_RECORD_PTS_HIGH) >>
			VIP_RECORD_FLAGS_SHIFT) << VIP_RECORD_FLAGS_SHIFT;
		((VipRecord *)record)->pts = pts;
	} else {
		((VipRecord *)record)->pts = 0;
	}
	((VipRecord *)record)->dts = 0;
	((VipRecord *)record)->position = rings->dataCursor;
	record = nextWord(record + offsetof(VipRecord, position), rings->records);

	/* and its end's */
	((VipRecord *)record)->header = USER_DATA_END << VIP_RECORD_TYPE_SHIFT;
	((VipRecord *)record)->pts = 0;
	((VipRecord *)record)->dts = 0;
	end = rings->dataCursor + bytes;
	if (end >= rings->data[0].start + rings->data[0].size)
		end = rings->data[1].start + (end - rings->data[0].size - rings->data[0].start);
	((VipRecord *)record)->position = end;
	rings->recordCursor = nextWord(record + offsetof(VipRecord, position), rings->records);

	/* the message, a word at a time */
	from = g_readStream.cursor;
	to = rings->dataCursor;
	for (i = 0; i < size >> 2; i++) {
		*(uint32_t *)to = *(const uint32_t *)from;
		to = nextWord(to, rings->data);
		from = nextWord(from, g_readStream.segments);
	}
	rings->dataCursor = to;
	rings->dataUsed += size;
	rings->recordsUsed += USER_DATA_RECORDS * sizeof(VipRecord);
	return 0;
}
#endif
