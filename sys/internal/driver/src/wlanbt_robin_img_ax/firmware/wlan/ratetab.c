/*
 * The rate words the MAC sends with (0x3d4ea - 0x3d8a4): the chains of rate words of every rate
 * index, the rows of the hardware rate table slots that the connections use, and the changes of
 * the preamble and the guard interval that rewrite them. The firmware's placeholder names,
 * "wlanPs*Frame", are kept; see robin/ratesel.h.
 */
#include <stddef.h>
#include "robin/conn.h"
#include "robin/htba.h"
#include "robin/mac.h"
#include "robin/protection.h"
#include "robin/ratectl.h"
#include "robin/ratesel.h"
#include "robin/rom.h"
#include "robin/sys.h"
#include "robin/txpower.h"

/* The slots of the rows: the connection's rate control holds one (WlanRateControl.slot). */
#define WLAN_RATE_SLOTS		19
#define WLAN_RATE_CHAIN_WORDS	8

extern uint8_t wlanRateBlocks[0x45c];
extern int (*wlanRateParameterHook)(unsigned int rate);	/* 0x54e90 */

/* What the last wlanPsApplyFrame() and wlanRateGreenfieldSet() set up, and the block mode. */
static uint8_t wlanRateBlockMode __attribute__((section(".dtcm.bss")));		/* 0x4001056 */
static uint8_t wlanRateShortPreamble __attribute__((section(".dtcm.bss")));	/* 0x4001057 */
static uint8_t wlanRateGreenfield __attribute__((section(".dtcm.bss")));	/* 0x4001058 */

/* The window's counting hook, and the age after which a window with few samples is judged. */
void wlanPsBuildFrame(void)
{
	wlanRateWindowState.ageLimit = 1000;
	wlanRateWindowState.count = wlanPsDispatchFrame;
	wlanRateWindowState.reportEnabled = 0;
}

/* Every slot starts with the default rows. */
void wlanPsComputeFrame(void)
{
	unsigned int slot;
	unsigned int word;

	wlanRateChainLength = 32;
	for (slot = 0; slot < WLAN_RATE_SLOTS; slot = (uint8_t)(slot + 1)) {
		for (word = 0; word < WLAN_RATE_CHAIN_WORDS; word = (uint8_t)(word + 1)) {
			wlanRateGroupChains[slot][word] = wlanRateDefaultGroup[word];
			wlanRateUnicastChains[slot][word] = wlanRateChains.defaultUnicast[word];
		}
	}
	for (slot = 0; slot < WLAN_RATE_SLOTS; slot = (uint8_t)(slot + 1)) {
		wlanRateSlots[slot].used = 0;
		wlanRateSlots[slot].groupChain = wlanRateGroupChains[slot];
		wlanRateSlots[slot].unicastChain = wlanRateUnicastChains[slot];
	}
}

/* The mode of the rate blocks, 0 to 3; 1 for anything else. */
unsigned int wlanPsQueryFrame(uint8_t mode)
{
	if (mode >= 4)
		return 1;
	wlanRateBlockMode = mode;
	return 0;
}

/*
 * The chain of a rate. A legacy rate that rateMask replaces by an HT rate (it has one of the HT
 * rates) is looked up as the first HT rate the mask holds from the one wlanRateModeFixed[] names.
 */
uint32_t *wlanPsResetFrame(unsigned int rate, uint32_t rateMask)
{
	uint32_t **table = wlanRateTable;

	if (rate < 14) {
		rate = wlanRateModeFixed[14 + rate];
		if (((rateMask << 1) >> 15) != 0) {
			while (!((1u << rate) & rateMask) && rate < 21)
				rate = (uint8_t)(rate + 1);
		}
	}
	return table[rate];
}

/* Writes the chains of the connection's rate into the hardware slot of its rate control. */
void wlanPsSyncFrame(struct WlanConnection *connection, unsigned int rate, struct WlanRateControl *rc)
{
	UINT posture = sysIrqDisable();
	struct WlanRateSlot *slot = rc->slot;
	uint32_t *unicast = slot->unicastChain;
	uint32_t *group = slot->groupChain;
	int probing = wlanRateProbingAt(&connection->peerLink.rateControl, rate);
	uint32_t *chain = wlanPsResetFrame(rate, rc->rateMask);
	uint32_t *plain;
	uint32_t word;
	unsigned int used = 0;
	unsigned int i;

	if (wlanRateEntryRetryGet(*chain) == 15) {
		word = *chain;
		group[0] = word;
		if (probing)
			group[0] = wlanRateEntryEnableSet(word, 1);
		i = 1;
	} else {
		if (probing) {
			group[0] = *chain;
			word = wlanRateEntryEnableSet(*chain, 1);
			group[0] = word;
			used = 1;
			group[0] = wlanRateEntryRetrySet(word, 1);
		}
		i = used + wlanRateChainBuild(&group[used], chain, (uint8_t)(8 - used), rc->rateMask);
	}
	for (; i < 8; i++)
		group[i] = group[i - 1];

	used = 0;
	plain = wlanRateChains.byRate[rate];
	if (wlanRateEntryRetryGet(*plain) == 15) {
		word = *plain;
		unicast[0] = word;
		if (probing)
			unicast[0] = wlanRateEntryEnableSet(word, 1);
	} else {
		if (probing) {
			unicast[0] = *plain;
			word = wlanRateEntryEnableSet(*plain, 1);
			unicast[0] = word;
			used = 1;
			unicast[0] = wlanRateEntryRetrySet(word, 1);
		}
		i = used + wlanRateChainBuild(&unicast[used], plain, (uint8_t)(3 - used), rc->rateMask);
		word = *wlanRateChains.byRate[rc->rates[0]];
		unicast[i] = word;
		unicast[i] = wlanRateEntryRetrySet(word, 15);
	}
	MAC_194_PENDING_CONTROL |= 1;
	sysIrqRestore(posture);
}

/* Switches the CCK rates and the blocks between the long and the short preamble. */
void wlanPsApplyFrame(struct WlanConnection *connection, int update, unsigned int shortPreamble)
{
	if (shortPreamble != wlanRateShortPreamble || update != 0) {
		wlanRateShortPreamble = shortPreamble;
		wlanRateWordsLongPreamble(wlanRateWordsCck, shortPreamble);
		wlanRateTableLongPreamble(wlanRateBlocks, shortPreamble, wlanRateBlockMode);
		wlanPsSyncFrame(connection, wlanRateSelect(&connection->peerLink.rateControl),
				&connection->peerLink.rateControl);
	}
}

/* The same for the short guard interval of the HT rates. */
void wlanRateGreenfieldSet(struct WlanConnection *connection, int update, int greenfield)
{
	if (greenfield != wlanRateGreenfield || update != 0) {
		wlanRateGreenfield = greenfield;
		wlanRateTableShortGi(wlanRateWordsHt20, wlanRateWordsHt40, greenfield);
		wlanRateTableHtShortGi(wlanRateBlocks, greenfield, wlanRateBlockMode);
		wlanPsSyncFrame(connection, wlanRateSelect(&connection->peerLink.rateControl),
				&connection->peerLink.rateControl);
	}
}

void wlanPsScheduleFrame(struct WlanConnection *connection)
{
	wlanRateGreenfieldSet(connection, 1, wlanHtGreenfieldWanted(connection));
	wlanPsApplyFrame(connection, 1, (wlanProtection.flags >> 5) & 1);
}

void wlanPsNotifyFrame(const uint8_t *items, int count)
{
	wlanRateBlockBuild(items, count, wlanRateBlocks, wlanRateChains.byRate);
}

/* The rows a frame is sent with: the BSS's, or the slot's of the peer. NULL when the connection has none. */
uint32_t *wlanPsFormatFrame(struct WlanConnection *connection, unsigned int multicast)
{
	unsigned int mode = connection->mode;

	if (mode == 3 || mode == 1) {
		if (multicast != 0)
			return wlanRateGroupChains[0];
		return wlanRateUnicastChains[0];
	}
	if (mode == 2 || connection->interfaceType == 1) {
		struct WlanRateSlot *slot = connection->peerLink.rateControl.slot;

		if (multicast != 0)
			return slot->groupChain;
		return slot->unicastChain;
	}
	return NULL;
}

/* Points every rate index at its chain, and gives every word its retry count of 2. */
unsigned int wlanPsInspectFrame(void)
{
	unsigned int rate;
	unsigned int result = 0;

	for (rate = 0; rate <= 3; rate++)
		wlanRateChains.byRate[rate] = &wlanRateWordsCck[3 - rate];
	wlanRateChains.byRate[4] = wlanRateWordsCck;
	for (rate = 5; rate <= 12; rate++)
		wlanRateChains.byRate[rate] = &wlanRateWordsOfdm[12 - rate];
	wlanRateChains.byRate[13] = wlanRateWordsOfdm;
	for (rate = 14; rate <= 21; rate++)
		wlanRateChains.byRate[rate] = &wlanRateWordsHt20[22 - rate];
	for (rate = 23; rate <= 30; rate++)
		wlanRateChains.byRate[rate] = &wlanRateWordsHt40[31 - rate];
	wlanRateChains.byRate[22] = wlanRateWordsMcs32;
	rate = 0;
	do {
		result = wlanRateEntryRetrySet(*wlanRateChains.byRate[rate], 2);
		*wlanRateChains.byRate[rate] = result;
		rate = (uint8_t)(rate + 1);
	} while (rate <= 30);
	return result;
}

void wlanPsTrackFrame(void)
{
	__aeabi_memclr4(wlanRateBlocks, sizeof(wlanRateBlocks));
}

/* Describes length rate words: the class, the base and the retry count of each, 4 bytes apart. */
void wlanPsManageFrame(void *destination, const void *source, unsigned int length)
{
	uint8_t *out = destination;
	unsigned int i;

	for (i = 0; i < length; i++) {
		uint32_t word = __aeabi_uread4((const uint8_t *)source + 4 * i);

		out[4 * i] = wlanRateEntryClass(word);
		out[4 * i + 1] = wlanRateEntryBase(word);
		out[4 * i + 2] = wlanRateEntryRetryGet(word);
	}
}

void wlanPsAcquireFrame(void)
{
	wlanRateTable = wlanRateChains.byRate;
	wlanRateParameterHook = (int (*)(unsigned int))macTxNotifyReleaseContext;
}
