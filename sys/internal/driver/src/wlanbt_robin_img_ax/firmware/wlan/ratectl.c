/*
 * Rate adaptation helpers: the rate words (the MAC's rate entries), the
 * tables the transmit path is given and the history of the rates tried
 * (the state is WlanConnection.rateControl, see robin/ratectl.h). The
 * filename and the names are inferred from use.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/ratesel.h"
#include "robin/baslot.h"
#include "robin/ratectl.h"
#include "robin/rom.h"
#include "robin/runtime.h"

/* The words of the rates, as the ROM hands the MAC the rate they name. */
const uint32_t wlanRatePolicy[WLAN_RATE_COUNT] = {
	0x800, 0x808, 0x810, 0x818, 0x818, 0x828, 0x830, 0x838,
	0x840, 0x848, 0x850, 0x858, 0x860, 0x860, 0x801, 0x809,
	0x811, 0x819, 0x821, 0x829, 0x831, 0x839, 0x4905, 0x4805,
	0x480d, 0x4815, 0x481d, 0x4825, 0x482d, 0x4835, 0x483d,
};

/* How much each rate weighs when two are compared (about its throughput, in kbit/s). */
const uint32_t wlanRateWeight[WLAN_RATE_COUNT] = {
	0x38e, 0x6d6, 0x1054, 0x1b26, 0x44ba, 0x1482, 0x1d9c, 0x260c,
	0x352a, 0x4254, 0x5834, 0x6a18, 0x7170, 0x7170, 0x189c, 0x30d4,
	0x4844, 0x5e88, 0x8980, 0xb5a4, 0xc9f4, 0xdfd4, 0x1482, 0x32c8,
	0x61a8, 0x9088, 0xbb80, 0x10d88, 0x15ba8, 0x182b8, 0x1a5e0,
};


uint32_t **wlanRateTable __attribute__((section(".itcm.rw")));			/* 0x54e8c */

__attribute__((noinline)) uint32_t wlanRateEntryRetrySet(uint32_t entry, unsigned int retries)
{
	return (retries << 24) | (entry & 0xf0ffffff);
}

unsigned int wlanRateEntryRetryGet(uint32_t entry)
{
	return (entry & WLAN_RATE_ENTRY_RETRY_MASK) >> 24;
}

__attribute__((noinline)) uint32_t wlanRateEntryEnableSet(uint32_t entry, unsigned int enable)
{
	return (2 * enable) | (entry & 0xfffffffd);
}

/* The index (0 to 30) of the rate a word names. */
__attribute__((noinline)) unsigned int wlanRateEntryIndex(uint32_t entry)
{
	unsigned int index = (uint8_t)(entry & 0xf8) >> 3;

	if (entry & WLAN_RATE_ENTRY_HT) {
		if (entry & WLAN_RATE_ENTRY_40MHZ) {
			if (index == 32)
				return 22;
			index = (uint8_t)(index + 23);
		} else {
			index = (uint8_t)(index + 14);
		}
	}
	return index;
}

/*
 * Chains the words of a rate item's fallbacks: out[0] is in[0], then each of
 * in[1..8] whose rate is in mask follows, until the chain repeats a word or
 * count of them are taken. Returns the length of the chain.
 */
unsigned int wlanRateChainBuild(uint32_t *out, const uint32_t *in, unsigned int count, uint32_t mask)
{
	uint32_t previous = in[0];
	unsigned int number = 1;
	unsigned int i = 0;

	*out = in[0];
	do {
		uint32_t entry = in[i + 1];

		if (entry == previous || count == 1)
			break;
		if ((1u << wlanRateEntryIndex(entry)) & mask) {
			out++;
			count = (uint8_t)(count - 1);
			number = (uint8_t)(number + 1);
			*out = entry;
		}
		previous = entry;
		i = (uint8_t)(i + 1);
	} while (i < 8);
	return number;
}

/* The class of rate a word names: HT, CCK or OFDM. */
unsigned int wlanRateEntryClass(uint32_t entry)
{
	if (entry & WLAN_RATE_ENTRY_HT)
		return WLAN_RATE_CLASS_HT;
	if ((uint8_t)(entry & 0xf8) >> 3 < 5)
		return WLAN_RATE_CLASS_CCK;
	return WLAN_RATE_CLASS_OFDM;
}

/* The index of the rate within its class. */
unsigned int wlanRateEntryBase(uint32_t entry)
{
	unsigned int index = (uint8_t)(entry & 0xf8) >> 3;

	if ((entry & WLAN_RATE_ENTRY_HT) || index < 5)
		return index;
	return (uint8_t)(index - 5);
}

/* Gives the first word of each rate the largest retry count. */
void wlanRateTableRetryReset(void)
{
	unsigned int i = 0;

	do {
		wlanRateTable[i][0] = wlanRateEntryRetrySet(wlanRateTable[i][0], 15);
		i = (uint8_t)(i + 1);
	} while (i <= 30);
}

/* The index of an HT rate at 40 MHz (MCS 32 has its own); 255 when the class or the MCS is not HT. */
__attribute__((noinline)) unsigned int wlanRateIndexHt40(unsigned int class, unsigned int index)
{
	if (class == WLAN_RATE_CLASS_HT) {
		if (index == 32)
			return 22;
		if (index <= 7)
			return (uint8_t)(index + 23);
	}
	return 255;
}

/* The index of a rate of a class (HT at 20 MHz); 255 when there is none. */
__attribute__((noinline)) unsigned int wlanRateIndexOf(unsigned int class, unsigned int index)
{
	if (class == WLAN_RATE_CLASS_CCK)
		return index;
	if (class == WLAN_RATE_CLASS_OFDM)
		return (uint8_t)(index + 5);
	if (class == WLAN_RATE_CLASS_HT && index <= 7)
		return (uint8_t)(index + 14);
	return 255;
}

/* Turns the long preamble flag of the CCK rates (not 1 Mb/s) of a rate table off or on. */
void wlanRateTableLongPreamble(uint8_t *table, int shortPreamble, int mode)
{
	unsigned int i;
	unsigned int j;

	if (mode != 2 && mode != 3)
		return;
	for (i = 1; i <= 12; i++) {
		uint8_t *block = table + 36 * i;

		for (j = 1; j < 9; j++) {
			void *address = block + 4 * j;
			uint32_t word = __aeabi_uread4(address);
			unsigned int index = wlanRateEntryIndex(word);

			if (index <= 3 && index != 0) {
				if (shortPreamble != 0)
					word &= ~WLAN_RATE_ENTRY_LONG_PREAMBLE;
				else
					word |= WLAN_RATE_ENTRY_LONG_PREAMBLE;
				__aeabi_uwrite4(word, address);
			}
		}
	}
}

/* The same for three words. */
void wlanRateWordsLongPreamble(uint32_t *words, int shortPreamble)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (shortPreamble != 0)
			words[i] &= ~WLAN_RATE_ENTRY_LONG_PREAMBLE;
		else
			words[i] |= WLAN_RATE_ENTRY_LONG_PREAMBLE;
	}
}

/* Sets or clears the short guard interval flag of the words of two rate lists (15 and 16 words, from 1). */
void wlanRateTableShortGi(uint32_t *first, uint32_t *second, int enable)
{
	int i;

	for (i = 1; i <= 15; i++) {
		if (enable != 0)
			first[i] |= WLAN_RATE_ENTRY_SHORT_GI;
		else
			first[i] &= ~WLAN_RATE_ENTRY_SHORT_GI;
	}
	for (i = 1; i <= 16; i++) {
		if (enable != 0)
			second[i] |= WLAN_RATE_ENTRY_SHORT_GI;
		else
			second[i] &= ~WLAN_RATE_ENTRY_SHORT_GI;
	}
}

/* The same for the HT rates (indexes 14 to 30) of the blocks of a rate table. */
void wlanRateTableHtShortGi(uint8_t *table, int enable, int mode)
{
	int i;
	int j;

	if (mode != 2 && mode != 3)
		return;
	for (i = 14; i <= 30; i++) {
		uint8_t *block = table + 36 * i;

		for (j = 1; j < 9; j++) {
			void *address = block + 4 * j;
			uint32_t word;

			if (enable != 0) {
				word = __aeabi_uread4(address);
				if (!(word & WLAN_RATE_ENTRY_HT))
					continue;
				word |= WLAN_RATE_ENTRY_SHORT_GI;
			} else {
				word = __aeabi_uread4(address) & ~WLAN_RATE_ENTRY_SHORT_GI;
			}
			__aeabi_uwrite4(word, address);
		}
	}
}

/*
 * The word of a rate a block's first entry holds: the rate's word, the
 * parameter the ROM gives it and the retry count of the item.
 */
static uint32_t wlanRateItemWord(unsigned int rate, unsigned int retries)
{
	uint32_t policy = wlanRatePolicy[rate];

	return policy + (wlanRateParameterHook(rate) << 16) + (retries << 24);
}

/*
 * Builds the rate blocks (36 bytes of 9 words each) of the rate an item list
 * names, at 20 and at 40 MHz: the first item has its rate enabled, each next
 * word is that of the item or, when no item is left or its rate does not
 * exist, a copy of the word before it; the last word holds the retry count
 * and the table points at the block.
 */
void wlanRateBlockBuild(const uint8_t *items, int count, uint8_t *blocks, uint32_t **table)
{
	unsigned int class = items[0];
	unsigned int first = items[1];
	unsigned int rate = wlanRateIndexOf(class, first);
	unsigned int rate40 = wlanRateIndexHt40(class, first);
	unsigned int i;

	if (class == WLAN_RATE_CLASS_HT) {
		if (rate != 255) {
			uint32_t word = wlanRatePolicy[rate];

			word = wlanRateEntryEnableSet(word + (wlanRateParameterHook(rate) << 16) + 0x1000000, 1);
			__aeabi_uwrite4(word, blocks + 36 * rate);
		}
		if (rate40 != 255) {
			uint32_t word = wlanRatePolicy[rate40];

			word = wlanRateEntryEnableSet(word + (wlanRateParameterHook(rate40) << 16) + 0x1000000, 1);
			__aeabi_uwrite4(word, blocks + 36 * rate40);
		}
	}
	for (i = 0; i < 8; i = (uint8_t)(i + 1), count--) {
		const uint8_t *item = &items[4 * i];
		unsigned int itemClass = item[0];
		unsigned int itemIndex = item[1];

		if (rate != 255) {
			unsigned int itemRate = wlanRateIndexOf(itemClass, itemIndex);
			uint8_t *entry = blocks + 36 * rate + 4 * i;
			uint32_t word;

			if (count <= 0 || itemRate == 255 || (class == WLAN_RATE_CLASS_HT && itemClass != WLAN_RATE_CLASS_HT))
				word = __aeabi_uread4(entry);
			else
				word = wlanRateItemWord(itemRate, item[2]);
			__aeabi_uwrite4(word, entry + 4);
		}
		if (rate40 != 255) {
			unsigned int itemRate = wlanRateIndexHt40(itemClass, itemIndex);
			uint8_t *entry = blocks + 36 * rate40 + 4 * i;
			uint32_t word;

			if (count <= 0 || itemRate == 255 || (class == WLAN_RATE_CLASS_HT && itemClass != WLAN_RATE_CLASS_HT))
				word = __aeabi_uread4(entry);
			else
				word = wlanRateItemWord(itemRate, item[2]);
			__aeabi_uwrite4(word, entry + 4);
		}
	}
	if (rate != 255) {
		uint8_t *block = blocks + 36 * rate;

		__aeabi_uwrite4(wlanRateEntryRetrySet(__aeabi_uread4(block + 32), 15), block + 32);
		table[rate] = (uint32_t *)(block + 4);
	}
	if (rate40 != 255) {
		uint8_t *block = blocks + 36 * rate40;

		__aeabi_uwrite4(wlanRateEntryRetrySet(__aeabi_uread4(block + 32), 15), block + 32);
		table[rate40] = (uint32_t *)(block + 4);
	}
}

/* Takes a free slot (of count, 12 bytes each); NULL when there is none. */
struct WlanRateSlot *wlanRateSlotAlloc(struct WlanRateSlot *slots, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		if (slots[i].used == 0) {
			slots[i].used = 1;
			break;
		}
	}
	if (i == count)
		return NULL;
	return &slots[i];
}

/* The first word of a rate (index 5 for one out of range). */
uint32_t *wlanRateTableLookup(unsigned int index)
{
	if (index > 30)
		index = 5;
	return wlanRateTable[index];
}

/* Keeps the rate in use, whether it is a probe and a value in the next history record. */
__attribute__((noinline)) void wlanRateHistoryRecord(struct WlanRateControl *rc, const int *value)
{
	struct WlanRateHistory *record = &rc->history[rc->historyCursor];

	record->index = rc->current;
	record->probing = rc->probing;
	record->value = *value;
}

void wlanRateHistoryPush(struct WlanRateControl *rc, const int *value)
{
	wlanRateHistoryRecord(rc, value);
	rc->historyCount++;
}

/* The highest rate in use that is not faster than the rate (compared by value). */
unsigned int wlanRateSearch(struct WlanRateControl *rc, unsigned int rate)
{
	unsigned int metric = romRateValue(rate);
	unsigned int top = rc->highest;

	while (rc->lowest < top && romRateValue(rc->rates[top]) > metric)
		top--;
	return top;
}

/* Keeps the index the rate is found at, with the share (percent) of a divisor's square. */
void wlanRateHistoryTarget(struct WlanRateControl *rc, unsigned int rate, int divisor)
{
	rc->history[3].index = wlanRateSearch(rc, rate);
	rc->history[3].value = 100u / (unsigned int)(divisor * divisor);
}

int wlanRateHistoryClear(struct WlanRateControl *rc, int count)
{
	__aeabi_memclr4(rc->history, 12 * count);
	rc->historyCount = 0;
	return 0;
}

/* Where the rate drops from limit: the rate the ROM names, or the lowest. */
__attribute__((noinline)) unsigned int wlanRateStepDown(unsigned int limit, struct WlanRateControl *rc)
{
	if (rc->lowest < limit)
		return wlanRateSearch(rc, romLibNotifyEntry(rc->rates[limit], rc, limit));
	return rc->lowest;
}

unsigned int wlanRateStepUp(unsigned int limit, struct WlanRateControl *rc)
{
	if (rc->highest > limit)
		return limit + 1;
	return rc->highest;
}

/* Starts the rate's window over and the counts of the next decision afresh. */
int wlanRateCounterReset(struct WlanRateControl *rc)
{
	fmCoexAdjustTable(rc);
	rc->frames = 0;
	rc->elapsed = 0;
	return 0;
}

/* The timeout of the rate in use, at most timeout (0 for a rate below shift). */
uint32_t wlanRateTimeoutValue(struct WlanRateControl *rc, uint32_t timeout, unsigned int shift)
{
	unsigned int rate = rc->rates[rc->current];
	uint32_t result;

	if (rate < shift)
		return 0;
	result = wlanRateBurstTable[rate - shift];
	if (result > timeout)
		return timeout;
	return result;
}

/* The next index: down from the target, or the target itself after a probe. */
unsigned int wlanRateAdvance(struct WlanRateControl *rc)
{
	unsigned int target = rc->target;

	if (rc->probing == 0)
		return wlanRateStepDown(target, rc);
	rc->probing = 0;
	return target;
}

/* A window of statistics {count, enabled, limit}: is it complete? */
int wlanRateHistoryFull(const uint32_t *window)
{
	return window[0] >= window[2] && window[1] != 0;
}

void wlanRateHistoryRestore(struct WlanRateControl *rc, int index)
{
	rc->historyCursor = 0;
	rc->target = rc->history[index].index;
	rc->probing = rc->history[index].probing;
}

/* Of two history records, the one with the higher weighted success (probes weigh a tenth more). */
int wlanRateBetter(int first, int second, struct WlanRateControl *rc)
{
	uint32_t weight1 = wlanRateWeight[rc->rates[rc->history[first].index]];
	uint32_t score1;
	uint32_t weight2;
	uint32_t score2;

	weight1 = rc->history[first].probing ? 11 * weight1 : 10 * weight1;
	score1 = (100 - rc->history[first].value) * weight1;
	weight2 = wlanRateWeight[rc->rates[rc->history[second].index]];
	weight2 = rc->history[second].probing ? 11 * weight2 : 10 * weight2;
	score2 = (100 - rc->history[second].value) * weight2;
	if (score1 >= score2)
		return first;
	return second;
}

/* The index of the rate in use. */
unsigned int wlanRateSelect(const struct WlanRateControl *rc)
{
	return rc->rates[rc->current];
}

/* A probe is under way, and at the last rate of a width. */
int wlanRateProbingAt(const struct WlanRateControl *rc, int index)
{
	return rc->probing != 0 && (index == 21 || index == 30);
}

/* Packs the fields of the host's rate configuration into the word the MAC takes. */
uint32_t wlanRateConfigPack(const uint8_t *config)
{
	uint8_t flags = __aeabi_uread4(config + 8);
	uint32_t word;

	word = (flags << 14) | (config[4] & 0xf) |
	       ((32 * ((config[7] << 8) | config[6])) & 0x1fff) | (flags << 23);
	return ((__aeabi_uread4(config + 12) << 22) & 0x7fffff) | word;
}
