/*
 * The controller's per-link sample records (0x43bfc - 0x43ebe). Each link has two rings of four samples
 * (a value, its weight and the ticks it covers); the averages of a ring give the rate the link is
 * running at, which the Bluetooth host is told. The file name and the roles are inferred; the function
 * names are the firmware's placeholders.
 */
#include <stddef.h>
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/btsource.h"

/* A halfword of a record the ROM keeps. */
typedef uint16_t __attribute__((may_alias)) BtHalf;
typedef uint32_t __attribute__((may_alias)) BtWord;

struct BtSourceState btSource __attribute__((section(".itcm.bss")));		/* 0x5a274 */
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
		record->mode = 6;
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
	ring->average = used != 0 ? sampleSum / used : 0;
	return ring->average;
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
unsigned int btLmpLcVerifyEntry(int link, unsigned int clock)
{
	struct BtSourceRecord *record = &btSourceBuffer[link];
	unsigned int elapsed = clock - record->firstClock;

	if (btSource.queue <= elapsed) {
		unsigned int span = 2 * btSource.queue;

		record->firstClock = clock - span;
		if (btLcoRomManageContext(link, clock - span, span) == 0)
			btSource.frame &= ~(1 << link);
	}
	return (uint16_t)elapsed;
}

/* The same for the second ring, without asking the ROM. */
unsigned int btLmpLcSelectEntry(int link, unsigned int clock)
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
 * Takes the two rings' rates for the host's link record (halfword 14: its record index; bytes 22 and 23
 * get the ROM's rate settings of the link, or 1) and stores them in the record.
 */
unsigned int btLmpLcAdjustEntry(uint8_t *link, unsigned int first, unsigned int second)
{
	unsigned int index = *(BtHalf *)(link + 14);
	struct BtSourceRecord *record = &btSourceBuffer[index];
	const uint8_t *table = btLdrRomReleaseState();
	int policy = btLmpLcInspectEntry(btThresholdPolicy, *(BtHalf *)(link + 14));
	unsigned int firstShort = btSource.queue > first;
	unsigned int secondShort = btSource.queue > second;
	uint8_t value = 1;
	unsigned int rate;

	if (policy == 0 || firstShort != 0)
		link[23] = table[116 * *(BtHalf *)(link + 14) + 13];
	else
		link[23] = 1;
	if (secondShort != 0)
		value = table[116 * *(BtHalf *)(link + 14) + 12];
	link[22] = value;
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
