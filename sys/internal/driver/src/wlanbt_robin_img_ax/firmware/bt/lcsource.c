/*
 * The controller's per-link sample records (0x43bfc - 0x4414e). Each link has two rings of four samples
 * (a value, its weight and the ticks it covers); the averages of a ring give the rate the link is
 * running at, which the Bluetooth host is told. The file name and the roles are inferred; the function
 * names are the firmware's placeholders.
 */
#include <stddef.h>
#include "robin/btbuffer.h"
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/btsource.h"
#include "robin/bttable.h"

struct BtSourceState btSource __attribute__((section(".itcm.bss")));		/* 0x5a274 */
/* The event filter's count and default (bt/lcbuffer.c); their data is this file's. */
uint8_t btEventFilterCount __attribute__((section(".itcm.bss")));		/* 0x5a050 */
uint8_t btEventFilterAccept __attribute__((section(".itcm.bss")));		/* 0x5a051 */
struct BtSourceRecord btSourceBuffer[BT_SOURCE_LINKS] __attribute__((section(".itcm.bss")));	/* 0x5a054 */

/* The ROM's record of a link is formatted with its timeout opened up. */
int btLmpLcFormatEntry(int link, uint8_t *record)
{
	uint16_t timeout = *(BtHalf *)(record + 54);
	int result;

	*(BtHalf *)(record + 54) = 0xffff;
	result = btLtsRomManageState(link, record);
	*(BtHalf *)(record + 54) = timeout;
	return result;
}

/* Whether the bit of the index is set in the mask. */
__attribute__((noinline)) int btLmpLcInspectEntry(unsigned int mask, int index)
{
	return ((1 << index) & mask) != 0;
}

/* The event clock, in the ROM's units. */
__attribute__((noinline)) int btLmpLcTrackEntry(void)
{
	return btLtsRomScheduleState(*(BtWord *)(btLinkEvent + 8) & 0x3ffffff);
}

/* The limits of the records, and the mode of each. */
unsigned int btLmpLcManageEntry(void)
{
	unsigned int link;

	btSource.packet = 0;
	btSource.descriptor = 4;
	btSource.queue = 80;
	btSource.timer = 40;
	btSource.channel = 3;
	btSource.param = 1;
	link = 0;
	do {
		struct BtSourceRecord *record = &btSourceBuffer[link];

		link = (uint8_t)(link + 1);
		record->rateLimit = 6;
	} while (link < BT_SOURCE_LINKS);
	return link;
}

/* The averages of a ring: the samples over the used slots, the weights over their ticks. */
unsigned int btLmpLcAcquireEntry(struct BtSourceRing *ring)
{
	unsigned int slot = 0;
	unsigned int used = 0;
	unsigned int sampleSum = 0;
	unsigned int weightSum = 0;
	unsigned int tickSum = 0;
	unsigned int average;

	do {
		if (ring->count[slot] != 0) {
			tickSum += ring->count[slot];
			weightSum += ring->weight[slot];
			sampleSum = (uint16_t)(ring->sample[slot] + sampleSum);
			used = (uint8_t)(used + 1);
		}
		slot = (uint8_t)(slot + 1);
	} while (slot < BT_SOURCE_SAMPLES);
	ring->ratio = tickSum != 0 ? weightSum / tickSum : 0;
	average = used != 0 ? sampleSum / used : 0;
	ring->average = average;
	return average;
}

/*
 * The rate of a ring: the average over the ratio. Past the limit (queue ticks) the ring is emptied
 * and the largest rate is returned.
 */
unsigned int btLmpLcReleaseEntry(struct BtSourceRing *ring, unsigned int limit)
{
	unsigned int rate = btSource.timer;

	if (btSource.queue > limit)
		btLmpLcAcquireEntry(ring);
	if (ring->ratio != 0) {
		if (btSource.queue <= limit) {
			unsigned int slot = 0;

			do {
				ring->count[slot] = 0;
				slot = (uint8_t)(slot + 1);
			} while (slot < BT_SOURCE_SAMPLES);
			ring->ratio = 0;
		} else {
			return ring->average / ring->ratio;
		}
	}
	return rate;
}

/* Puts a sample in the next slot of the ring. */
__attribute__((noinline)) void btLmpLcSetupEntry(struct BtSourceRing *ring, uint16_t sample, uint16_t weight, uint8_t count)
{
	unsigned int slot = ring->head & 3;

	ring->head++;
	ring->sample[slot] = sample;
	ring->weight[slot] = weight;
	ring->count[slot] = count != 0 ? count : 1;
}

/* The first ring of the link takes a sample, and the weight is kept for the next. */
struct BtSourceState *btLmpLcClearEntry(int link, int unused, uint16_t value, uint16_t sample)
{
	unsigned int index = btHostWindow[link];
	struct BtSourceRecord *record = &btSourceBuffer[index];
	unsigned int clock = btLmpLcTrackEntry();
	unsigned int previous = record->firstClock;

	record->firstClock = clock;
	btLmpLcSetupEntry(&record->first, sample, record->value, (uint8_t)(clock - previous));
	record->value = value;
	btSource.frame |= 1 << index;
	return &btSource;
}

/* The second ring of the link takes a sample. */
struct BtSourceState *btLmpLcConvertEntry(int link, int unused, uint16_t weight, uint16_t sample)
{
	unsigned int index = btHostWindow[link];
	struct BtSourceRecord *record = &btSourceBuffer[index];
	unsigned int clock = btLmpLcTrackEntry();
	unsigned int previous = record->secondClock;

	record->secondClock = clock;
	btLmpLcSetupEntry(&record->second, sample, weight, (uint8_t)(clock - previous));
	btSource.header |= 1 << index;
	return &btSource;
}

/* The first ring has run past its limit: its clock is moved up and the ROM is asked about the link. */
__attribute__((noinline)) unsigned int btLmpLcVerifyEntry(int link, unsigned int clock)
{
	struct BtSourceRecord *record = &btSourceBuffer[link];
	unsigned int elapsed = clock - record->firstClock;

	if (btSource.queue <= elapsed) {
		unsigned int span = 2 * btSource.queue;

		record->firstClock = clock - span;
		if (btLcoRomManageContext(link) == 0)
			btSource.frame &= ~(1 << link);
	}
	return (uint16_t)elapsed;
}

/* The same for the second ring, without asking the ROM. */
__attribute__((noinline)) unsigned int btLmpLcSelectEntry(int link, unsigned int clock)
{
	struct BtSourceRecord *record = &btSourceBuffer[link];
	unsigned int elapsed = clock - record->secondClock;

	if (btSource.queue <= elapsed) {
		record->secondClock = clock - 2 * btSource.queue;
		btSource.header &= ~(1 << link);
	}
	return (uint16_t)elapsed;
}

/*
 * Takes the two rings' rates for the device's profile (secondSetting and firstSetting get the ROM's rate
 * settings of the link, or 1) and stores them in its record.
 */
unsigned int btLmpLcAdjustEntry(struct BtDeviceProfile *profile, unsigned int first, unsigned int second)
{
	struct BtSourceRecord *record = &btSourceBuffer[profile->index];
	const uint8_t *table = btLdrRomReleaseState();
	int policy = btLmpLcInspectEntry(btTableLinkMask, profile->index);
	unsigned int firstShort = btSource.queue > first;
	unsigned int secondShort = btSource.queue > second;
	uint8_t value = 1;
	unsigned int rate;

	if (policy == 0 || firstShort != 0)
		profile->firstSetting = table[116 * profile->index + 13];
	else
		profile->firstSetting = 1;
	if (secondShort != 0)
		value = table[116 * profile->index + 12];
	profile->secondSetting = value;
	rate = btLmpLcReleaseEntry(&record->first, secondShort);
	if (rate <= btSource.timer)
		rate = btLmpLcReleaseEntry(&record->first, secondShort);
	else
		rate = btSource.timer;
	record->firstRate = rate;
	rate = btLmpLcReleaseEntry(&record->second, firstShort);
	if (rate > btSource.timer)
		rate = btSource.timer;
	else
		rate = btLmpLcReleaseEntry(&record->second, firstShort);
	record->secondRate = rate;
	return rate;
}

/* Whether a link's first ring is late (the rate it gives is below its elapsed ticks). */
static inline int btSourceFirstLate(const struct BtSourceRecord *record, unsigned int elapsed)
{
	unsigned int rate = record->rateLimit;

	if (rate >= record->firstRate)
		rate = record->firstRate;
	return rate < elapsed;
}

/*
 * Each event: the rings of the links 1..7 are aged, and a link whose rate is late is moved to the
 * next slot by the ROM.
 */
void btLmpLcRecordEntry(void)
{
	unsigned int clock = btLmpLcTrackEntry();
	unsigned int link;

	btTargetConfig = btSource.frame == 0 && btSource.header == 0 ? 290 : 72;
	link = 1;
	do {
		struct BtDeviceProfile *profile = &btDeviceProfile[link];
		unsigned int first = btLmpLcVerifyEntry(link, clock);
		unsigned int second = btLmpLcSelectEntry(link, clock);
		struct BtSourceRecord *record;
		int late;
		int lateSecond;
		int due;
		int policy;

		if (btLmpLcInspectEntry(btSource.packet, link) == 0 || profile->connection == 0 ||
		    (profile->state != 5 && profile->state != 9))
			goto next;
		record = &btSourceBuffer[profile->index];
		late = btLcoRomManageContext(profile->index) != 0 && btSourceFirstLate(record, first);
		lateSecond = btSource.queue > second && record->secondRate < second;
		if ((late | lateSecond) == 0)
			goto next;
		due = (int32_t)(profile->due - btDeviceRecord) > 0;
		policy = btLmpLcInspectEntry(btTableLinkMask, link);
		if (due || (profile->active != 0 && policy != 0 && profile->state == 9)) {
			btLtsRomQueryState(profile);
			btLtsRomApplyState(profile, due ? btDeviceRecord : profile->due, policy != 0 ? 5 : profile->state);
		}
next:
		link = (uint8_t)(link + 1);
	} while (link <= 7);
}

/* A link's slot ended at clock: its rates and ticks are taken again and the ROM schedules the next. */
void btLmpLcFilterEntry(struct BtDeviceProfile *profile, unsigned int clock)
{
	unsigned int start = profile->due;
	unsigned int index = profile->index;
	unsigned int second = btLmpLcSelectEntry(index, clock);
	unsigned int first = btLmpLcVerifyEntry(index, clock);
	struct BtSourceRecord *record = &btSourceBuffer[index];
	int policy = btLmpLcInspectEntry(btTableLinkMask, index);
	int fresh;
	int late;
	unsigned int average;
	unsigned int span;
	unsigned int offset;
	unsigned int mode;

	fresh = btLcoRomManageContext(index) != 0 || btSource.queue > second || btSource.queue > first;
	if (btLcoRomManageContext(index) != 0 && btSourceFirstLate(record, first))
		late = 1;
	else
		late = btSource.queue > second && record->secondRate < second;
	average = (profile->firstSetting + profile->secondSetting) >> 1;
	span = average + 1 >= 3 ? average + 1 : 3;
	if (profile->active != 0) {
		if (fresh)
			btLmpLcAdjustEntry(profile, second, first);
		profile->adjust = 0;
		start = clock;
		profile->span = span;
		mode = policy != 0 && late != 0 ? 5 : 9;
		offset = 0;
	} else {
		if (fresh && profile->adjust == 1)
			btLmpLcAdjustEntry(profile, second, first);
		profile->adjust = 0;
		if (fresh) {
			unsigned int rate = record->firstRate;

			if (rate >= record->secondRate)
				rate = record->secondRate;
			if ((policy != 0 ? btSource.param : btSource.channel) * span > rate) {
				offset = 0;
				start = clock;
			} else if (profile->limit != 0) {
				span = profile->limit;
				offset = 0;
				start = clock;
			} else if (policy != 0) {
				offset = btSource.timer;
			} else {
				offset = profile->interval <= rate ? profile->interval : rate;
			}
		} else {
			offset = profile->interval;
		}
		profile->rate = (fresh | late) != 0 ? span : 1;
		mode = policy != 0 ? 5 : 9;
	}
	btLtsRomApplyState(profile, start + offset, mode);
}
