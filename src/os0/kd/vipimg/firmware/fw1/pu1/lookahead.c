/*
 * PU1: the look-ahead. Before a picture is decoded, the records PU0 parsed are
 * looked through, VIP_RECORDS_READ at a time, for where the picture ends: at the
 * next picture's first slice, a delimiter or a restart, or, when the picture is a
 * field, after the other field. On the way it takes what the picture needs of
 * what follows it: the other field's order count and whether it starts over,
 * and the next pic_timing().
 */
#include "codec.h"

#define LOOKAHEAD_READS		256	/* of VIP_RECORDS_READ records */

int8_t g_nextTimingSeen;
uint32_t g_nextOutputTime;
int8_t g_otherFieldRandomAccess;

/* The user data rings' writable space, for the SEI user data of the picture. */
static void takeUserDataSpace(void)
{
	VipUserDataRings *rings = &g_userData;

	if (g_ringHandles[VIP_RING_USER_DATA]) {
		rings->dataCount = getRingWritable(g_ringHandles[VIP_RING_USER_DATA],
			rings->data, VIP_STREAM_SEGMENTS);
		rings->dataCursor = rings->data[0].start;
	} else {
		rings->dataCursor = 0;
		rings->data[0].start = 0;
		rings->dataCount = 0;
	}
	rings->dataUsed = 0;

	if (g_ringHandles[VIP_RING_USER_RECORDS]) {
		rings->recordCount = getRingWritable(g_ringHandles[VIP_RING_USER_RECORDS],
			rings->records, VIP_STREAM_SEGMENTS);
		rings->recordCursor = rings->records[0].start;
	} else {
		rings->recordCursor = 0;
		rings->records[0].start = 0;
		rings->recordCount = 0;
	}
	rings->recordsUsed = 0;
}

/*
 * The records ran out before the picture's end. When PU0 overflowed a ring (its
 * writer state), what is unread is dropped: -1; otherwise 0, to wait for more.
 */
static int dropOverflowedInput(void)
{
	if (g_pictureCount)
		g_underrunCount++;
	if (!getRingWriterState(g_ringHandles[VIP_RING_RECORDS]) &&
	    !getRingWriterState(g_ringHandles[VIP_RING_SLICES]))
		return 0;
	dropParsedInput();
	g_currentBuffer = NULL;
	return -1;
}

/*
 * The word offset bytes into a record's data in the slice ring (readable: its
 * readable segments), stepping 8 bytes at a time, as PU0 pads the data, over the
 * ring's end into the second segment. -1 when it is not written yet.
 */
static int peekParsedWord(const VipRingSegment *readable, uint32_t position,
	uint32_t offset, uint32_t *word)
{
	uint32_t end = readable[0].start + readable[0].size, step;

	if (position >= readable[0].start) {
		for (step = 8; step <= offset; step += 8) {
			if (position + step < end)
				continue;
			if (!readable[1].size)
				return -1;
			position = readable[1].start - step;
			break;
		}
	}
	*word = *(const uint32_t *)(position + offset);
	return 0;
}

/*
 * The other field's pic_order_cnt_lsb, or delta_pic_order_cnt[0], from its slice
 * header record's data; -1 when the data is not written yet.
 */
static int takeOtherFieldOrderCnt(const VipRingSegment *readable, uint32_t position)
{
	const VipSps *sps = g_currentSps;
	uint32_t word;

	if (!sps->picOrderCntType) {
		if (peekParsedWord(readable, position, VIP_SLICE_POC_LSB * 4, &word))
			return -1;
	} else if (sps->deltaPicOrderAlwaysZero) {
		word = 0;
	} else if (peekParsedWord(readable, position, VIP_SLICE_DELTA_POC * 4, &word)) {
		return -1;
	}
	g_otherFieldOrderCnt = word & 0xffff;
	return 0;
}

/*
 * Look for the end of the picture whose records come next. *last is the index of
 * its last slice header record (for a field pair the second field's, and
 * *firstFieldLast the first field's), -1 when the records ran out before the end,
 * -2 when they ran out after the stream's end; *end is the position of the record
 * two after *last, the end of the picture's slice data. 1 when the end was found,
 * and the read stream and picture units are set up for the picture; 0 to wait for
 * PU0; -1 when the unread input was dropped.
 */
int lookAhead(int16_t *last, int16_t *firstFieldLast, uint32_t *end)
{
	VipRingSegment records[VIP_STREAM_SEGMENTS], slices[VIP_STREAM_SEGMENTS];
	VipDmaDescriptor *list = g_readStream.list;
	const VipRecord *record;
	volatile uint32_t done;
	uint32_t segment = 0, offset = 0, usable, length, filled, descriptors, read, i;
	int32_t count, index;
	uint8_t ended = 0, timings = 0, fields = 0;
	uint32_t fieldPic = 0;

	*end = 0;
	g_otherFieldRandomAccess = 0;
	g_nextTimingSeen = 0;
	*last = -1;
	*firstFieldLast = -2;
	takeUserDataSpace();

	getRingReadable(g_ringHandles[VIP_RING_SLICES], slices, VIP_STREAM_SEGMENTS);
	count = getRingReadable(g_ringHandles[VIP_RING_RECORDS], records,
		VIP_STREAM_SEGMENTS);
	for (read = 0; read < LOOKAHEAD_READS; read++) {
		/* list the next records, through the records ring's segments */
		filled = 0;
		descriptors = 0;
		for (; (int32_t)segment < count; segment++, offset = 0) {
			usable = records[segment].size & ~(sizeof(VipRecord) - 1);
			if (offset == usable) {
				if ((int32_t)segment + 1 >= count)
					goto outOfRecords;
				continue;
			}
			length = usable - offset;
			if (length >= sizeof(g_recordBuffer) - filled)
				length = sizeof(g_recordBuffer) - filled;
			list[descriptors].external = records[segment].start + offset;
			list[descriptors].localLength =
				((uint32_t)g_recordBuffer + filled) << 16 | length;
			descriptors++;
			filled += length;
			if (filled >= sizeof(g_recordBuffer)) {
				offset += length;
				break;
			}
		}
		queueListDma(VIP_DMA_LIST | VIP_DMA_READ, (uint32_t)list << 16 |
			descriptors * sizeof(VipDmaDescriptor), 0, 0, NULL, &done);
		while (done == 1)
			;

		for (i = 0; i < VIP_RECORDS_READ; i++) {
			record = &g_recordBuffer[i];
			index = read * VIP_RECORDS_READ + i;
			if (index == *last + 2)
				*end = record->position;
			if (i == filled / sizeof(VipRecord))
				goto outOfRecords;

			switch (record->header >> VIP_RECORD_TYPE_SHIFT) {
			case VIP_PARSED_END_SEQUENCE:
			case VIP_PARSED_END_STREAM:
				ended = 1;
				break;
			case VIP_PARSED_PICTURE_HEADER:
				if (*last == -1) {
					fieldPic = record->header & VIP_PARSED_FIELD_PIC;
					*last = index;
					break;
				}
				/* only a first field goes on, to its other field */
				if (!fieldPic || fields)
					goto found;
				if (takeOtherFieldOrderCnt(slices, record->position))
					return 0;
				*firstFieldLast = *last;
				*last = index;
				fields++;
				if (*(const uint32_t *)record->position >> VIP_SLICE_IDR & 1)
					g_otherFieldRandomAccess = 1;
				break;
			case VIP_PARSED_DELIMITER:
				if (*last == -1)
					break;
				if (!fieldPic || fields)
					goto found;
				break;
			case VIP_PARSED_SLICE_HEADER:
				*last = index;
				break;
			case VIP_PARSED_RECOVERY_POINT:
				if (*last != -1 && fieldPic && !fields)
					g_otherFieldRandomAccess = 1;
				break;
			case VIP_PARSED_PICTURE_TIMING:
				/* the first is the picture's own, the second the other field's */
				if (fieldPic && timings == 1) {
					const uint32_t *words = (const uint32_t *)record->position;

					g_nextTimingSeen = 1;
					g_nextOutputTime = words[VIP_TIMING_DPB_OUTPUT_DELAY] +
						words[VIP_TIMING_CPB_REMOVAL_DELAY];
				}
				timings++;
				break;
			case VIP_PARSED_PICTURE_END:
				if (*last != -1)
					goto found;
				break;
			case VIP_PARSED_START:
				goto found;
			}
		}
	}
	if (*last != -1)
		goto found;
	dropParsedInput();
	return -1;

outOfRecords:
	*last = ended ? -2 : -1;
	return dropOverflowedInput();

found:
	g_readStream.segmentCount = getRingReadable(g_ringHandles[VIP_RING_SLICES],
		g_readStream.segments, VIP_STREAM_SEGMENTS);
	initializeUnits();
	return 1;
}
