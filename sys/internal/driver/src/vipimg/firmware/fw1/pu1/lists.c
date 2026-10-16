/*
 * The initial reference picture lists (H.264 8.2.4.2), both codecs' (PU1's, and
 * FW2's codec), written to the units' RAM. A P slice has VIP_UNIT_REF_LIST, a B
 * slice VIP_UNIT_LIST0 and LIST1; the entries after the last repeat the first.
 */
#ifdef VIP_FW2
#include "../../fw2/codec/codec.h"
#else
#include "codec.h"
#endif

/* the picture as the units name it */
static uint32_t unitPicture(const VipPictureBuffer *buffer)
{
	return (buffer->picture & VIP_UNIT_PICTURE_ADDRESS) |
		(uint32_t)(buffer->coding & 1) << 30 |
		(buffer->index & VIP_UNIT_PICTURE_INDEX);
}

/* a frame's two entries; longTerm is 0 or VIP_UNIT_PICTURE_LONG_TERM */
static void putFrame(VipUnitReference *entry, const VipPictureBuffer *buffer,
	uint32_t longTerm)
{
	entry[0].picture = unitPicture(buffer) | longTerm;
	entry[0].picOrderCnt = buffer->topPicOrderCnt;
	entry[0].picNum = (buffer->picNum & VIP_UNIT_PIC_NUM_MASK) | longTerm;
	entry[1].picOrderCnt = buffer->bottomPicOrderCnt;
}

/* the short-term frames, then the long-term ones by LongTermFrameIdx */
static uint32_t putLongTermFrames(VipUnitReference *list, uint32_t count)
{
	VipPictureBuffer *buffer;
	int32_t idx;
	uint32_t i;

	for (idx = 0; idx <= g_maxLongTermFrameIdx; idx++) {
		for (i = 0; i < g_dpbCount; i++) {
			buffer = g_dpbByPoc[i];
			if (buffer->reference != VIP_REFERENCE_LONG ||
			    buffer->longTermFrameIdx != (uint32_t)idx)
				continue;
			putFrame(&list[2 * count++], buffer, VIP_UNIT_PICTURE_LONG_TERM);
		}
	}
	return count;
}

static void fillFrameList(VipUnitReference *list, uint32_t count)
{
	for (; count < VIP_UNIT_LIST_ENTRIES / 2; count++) {
		list[2 * count] = list[0];
		list[2 * count + 1] = list[1];
	}
}

/* 8.2.4.2.1: the short-term frames, the latest decoded first */
void initFrameRefList(void)
{
	VipUnitReference *list = vipUnitList(VIP_UNIT_REF_LIST);
	VipPictureBuffer *buffer;
	uint32_t count = 0, i;

	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByDecode[i];
		if (buffer->reference == VIP_REFERENCE_SHORT)
			putFrame(&list[2 * count++], buffer, 0);
	}
	count = putLongTermFrames(list, count);
	g_refListCount = count;
	fillFrameList(list, count);
}

/* whether RefPicList1 begins as RefPicList0, with more than one entry */
static int frameListsSame(void)
{
	const VipUnitReference *list0 = vipUnitList(VIP_UNIT_LIST0);
	const VipUnitReference *list1 = vipUnitList(VIP_UNIT_LIST1);
	uint32_t i;

	if (g_list1Count < 2)
		return 0;
	for (i = 0; i < g_list1Count; i++)
		if (list1[2 * i].picture != list0[2 * i].picture)
			return 0;
	return 1;
}

/*
 * 8.2.4.2.3: the short-term frames before the current picture in output order,
 * the nearest first, then those after it (RefPicList1 the other way round). A
 * frame with the current picture's PicOrderCnt goes first in both: RefPicList1
 * takes it with those after.
 */
void initFrameLists(void)
{
	VipUnitReference *list0 = vipUnitList(VIP_UNIT_LIST0);
	VipUnitReference *list1 = vipUnitList(VIP_UNIT_LIST1);
	VipUnitReference swap;
	VipPictureBuffer *buffer;
	int32_t current = picOrderCnt(g_currentBuffer), order;
	uint32_t count = 0, i;

	for (i = g_dpbCount; i-- > 0;) {
		buffer = g_dpbByPoc[i];
		if (buffer->reference == VIP_REFERENCE_SHORT && picOrderCnt(buffer) <= current)
			putFrame(&list0[2 * count++], buffer, 0);
	}
	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->reference == VIP_REFERENCE_SHORT && picOrderCnt(buffer) > current)
			putFrame(&list0[2 * count++], buffer, 0);
	}
	g_list0Count = putLongTermFrames(list0, count);

	count = 0;
	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (buffer->reference == VIP_REFERENCE_SHORT && picOrderCnt(buffer) >= current)
			putFrame(&list1[2 * count++], buffer, 0);
	}
	for (i = g_dpbCount; i-- > 0;) {
		buffer = g_dpbByPoc[i];
		if (buffer->reference == VIP_REFERENCE_SHORT && picOrderCnt(buffer) < current)
			putFrame(&list1[2 * count++], buffer, 0);
	}
	g_list1Count = putLongTermFrames(list1, count);

	if (frameListsSame()) {
		swap = list1[0];
		list1[0] = list1[2];
		list1[2] = swap;
		order = list1[1].picOrderCnt;
		list1[1].picOrderCnt = list1[3].picOrderCnt;
		list1[3].picOrderCnt = order;
	}
	fillFrameList(list0, g_list0Count);
	fillFrameList(list1, g_list1Count);
}

static int8_t fieldReference(const VipPictureBuffer *buffer, int bottom)
{
	return bottom ? buffer->bottomReference : buffer->topReference;
}

/* a field's entry; a field of the current one's parity has PicNum one more */
static void putField(VipUnitReference *entry, const VipPictureBuffer *buffer,
	int bottom, uint32_t sameParity, uint32_t longTerm)
{
	entry->picture = unitPicture(buffer) | (bottom ? VIP_UNIT_PICTURE_BOTTOM : 0) |
		longTerm;
	entry->picOrderCnt = bottom ? buffer->bottomPicOrderCnt : buffer->topPicOrderCnt;
	entry->picNum = ((buffer->picNum + sameParity) & VIP_UNIT_PIC_NUM_MASK) | longTerm;
}

/*
 * 8.2.4.2.5: the frames' fields marked as reference, alternately of the current
 * field's parity and the other, until both run out.
 */
static uint32_t alternate(VipUnitReference *list, uint32_t count,
	VipPictureBuffer *const *frames, uint32_t frameCount, int8_t reference,
	uint32_t longTerm)
{
	int bottom = g_bottomField != 0;
	uint32_t same = 0, other = 0;

	while (same < frameCount || other < frameCount) {
		for (; same < frameCount; same++) {
			if (fieldReference(frames[same], bottom) == reference) {
				putField(&list[count++], frames[same++], bottom, 1, longTerm);
				break;
			}
		}
		for (; other < frameCount; other++) {
			if (fieldReference(frames[other], !bottom) == reference) {
				putField(&list[count++], frames[other++], !bottom, 0, longTerm);
				break;
			}
		}
	}
	return count;
}

static void alternateFields(uint32_t shortCount, uint32_t longCount,
	VipPictureBuffer *const *shorts, VipPictureBuffer *const *longs,
	VipUnitReference *list, uint32_t *count)
{
	uint32_t n;

	n = alternate(list, 0, shorts, shortCount, VIP_REFERENCE_SHORT, 0);
	n = alternate(list, n, longs, longCount, VIP_REFERENCE_LONG,
		VIP_UNIT_PICTURE_LONG_TERM);
	*count = n;
}

static int fieldsAre(const VipPictureBuffer *buffer, int8_t reference)
{
	return buffer->topReference == reference || buffer->bottomReference == reference;
}

/* refFrameListLongTerm: by LongTermFrameIdx */
static uint32_t longTermFrames(VipPictureBuffer **frames)
{
	VipPictureBuffer *buffer;
	uint32_t count = 0, i;
	int32_t idx;

	for (idx = 0; idx <= g_maxLongTermFrameIdx; idx++) {
		for (i = 0; i < g_dpbCount; i++) {
			buffer = g_dpbByPoc[i];
			if (fieldsAre(buffer, VIP_REFERENCE_LONG) &&
			    buffer->longTermFrameIdx == (uint32_t)idx)
				frames[count++] = buffer;
		}
	}
	return count;
}

static void fillFieldList(VipUnitReference *list, uint32_t count)
{
	for (; count < VIP_UNIT_LIST_ENTRIES; count++)
		list[count] = list[0];
}

/* 8.2.4.2.2: refFrameList0ShortTerm the latest decoded first */
void initFieldRefList(void)
{
	VipUnitReference *list = vipUnitList(VIP_UNIT_REF_LIST);
	VipPictureBuffer *shorts[VIP_MAX_REFERENCES], *longs[VIP_MAX_REFERENCES];
	VipPictureBuffer *buffer;
	uint32_t shortCount = 0, longCount, i;

	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByDecode[i];
		if (fieldsAre(buffer, VIP_REFERENCE_SHORT))
			shorts[shortCount++] = buffer;
	}
	longCount = longTermFrames(longs);
	alternateFields(shortCount, longCount, shorts, longs, list, &g_refListCount);
	fillFieldList(list, g_refListCount);
}

static int fieldListsSame(void)
{
	const VipUnitReference *list0 = vipUnitList(VIP_UNIT_LIST0);
	const VipUnitReference *list1 = vipUnitList(VIP_UNIT_LIST1);
	uint32_t i;

	if (g_list1Count < 2)
		return 0;
	for (i = 0; i < g_list1Count; i++)
		if (list1[i].picture != list0[i].picture)
			return 0;
	return 1;
}

/*
 * 8.2.4.2.4: refFrameList0ShortTerm and refFrameList1ShortTerm by the order count
 * a frame's fields have for the current field, then alternated.
 */
#ifdef VIP_FW2
/* GCC 4.5's GCSE leaves an insn without a length here when the counts are tiny */
__attribute__((optimize("no-gcse")))
#endif
void initFieldLists(void)
{
	VipUnitReference *list0 = vipUnitList(VIP_UNIT_LIST0);
	VipUnitReference *list1 = vipUnitList(VIP_UNIT_LIST1);
	VipPictureBuffer *shorts[VIP_MAX_REFERENCES], *longs[VIP_MAX_REFERENCES];
	VipPictureBuffer *buffer;
	VipUnitReference swap;
	int32_t current = picOrderCnt(g_currentBuffer);
	uint32_t shortCount = 0, longCount, count, i;

	for (i = g_dpbCount; i-- > 0;) {
		buffer = g_dpbByPoc[i];
		if (fieldsAre(buffer, VIP_REFERENCE_SHORT) &&
		    oppositePicOrderCnt(buffer) <= current)
			shorts[shortCount++] = buffer;
	}
	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (fieldsAre(buffer, VIP_REFERENCE_SHORT) &&
		    oppositePicOrderCnt(buffer) > current)
			shorts[shortCount++] = buffer;
	}
	longCount = longTermFrames(longs);
	alternateFields(shortCount, longCount, shorts, longs, list0, &g_list0Count);

	/* the same frames, so as many */
	count = 0;
	for (i = 0; i < g_dpbCount; i++) {
		buffer = g_dpbByPoc[i];
		if (fieldsAre(buffer, VIP_REFERENCE_SHORT) &&
		    oppositePicOrderCnt(buffer) > current)
			shorts[count++] = buffer;
	}
	for (i = g_dpbCount; i-- > 0;) {
		buffer = g_dpbByPoc[i];
		if (fieldsAre(buffer, VIP_REFERENCE_SHORT) &&
		    oppositePicOrderCnt(buffer) <= current)
			shorts[count++] = buffer;
	}
	alternateFields(shortCount, longCount, shorts, longs, list1, &g_list1Count);

	if (fieldListsSame()) {
		swap = list1[0];
		list1[0] = list1[1];
		list1[1] = swap;
	}
	fillFieldList(list0, g_list0Count);
	fillFieldList(list1, g_list1Count);
}
