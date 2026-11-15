/*
 * Rate adaptation of a connection: the state it keeps in WlanConnection
 * (rateControl), the rate entries (the MAC's rate words) and the tables the
 * transmit path builds from them (wlan/ratectl.c). The names are inferred
 * from use.
 *
 * A rate has an index from 0 to 30: 0 to 13 the legacy rates (0 to 3 CCK, 5
 * to 12 OFDM), 14 to 21 HT MCS 0 to 7 at 20 MHz, 22 MCS 32, 23 to 30 HT MCS 0
 * to 7 at 40 MHz. A rate word holds a flag for HT (bit 0), one for the
 * entry being enabled (bit 1), one for 40 MHz (bit 2), the index or MCS (bits
 * 3 to 7), the retry count (bits 24 to 27) and, per table, more flags.
 */
#ifndef ROBIN_RATECTL_H
#define ROBIN_RATECTL_H

#include <stddef.h>
#include <stdint.h>

#define WLAN_RATE_COUNT			31
#define WLAN_RATE_ENTRY_HT		0x00000001u
#define WLAN_RATE_ENTRY_ENABLED		0x00000002u
#define WLAN_RATE_ENTRY_40MHZ		0x00000004u
#define WLAN_RATE_ENTRY_LONG_PREAMBLE	0x00008000u	/* of the CCK rates (inferred) */
#define WLAN_RATE_ENTRY_SHORT_GI	0x00800000u	/* of the HT rates (inferred) */
#define WLAN_RATE_ENTRY_RETRY_MASK	0x0f000000u

/* The classes of rate a rate word has (the first byte of a rate item). */
#define WLAN_RATE_CLASS_CCK		3
#define WLAN_RATE_CLASS_OFDM		7
#define WLAN_RATE_CLASS_HT		8

/* What was kept of a rate that was used: its index, whether it was a probe and a statistic (12 bytes). */
struct WlanRateHistory {
	uint32_t index;
	uint8_t probing;
	uint8_t reserved5[3];
	uint32_t value;
};

/* WlanConnection.rateControl (228 bytes). */
struct WlanRateControl {
	uint8_t reserved0[28];
	uint32_t highest;		/* the last index of rates[] in use */
	uint32_t lowest;		/* the first one */
	uint32_t target;		/* the index the next change moves to */
	uint32_t current;		/* the index in use */
	uint8_t historyCount;		/* records pushed since wlanRateHistoryClear() */
	uint8_t probing;		/* the current rate is a probe */
	uint8_t reserved46[50];
	struct WlanRateHistory history[7];
	uint8_t historyCursor;
	uint8_t reserved181[11];
	uint8_t rates[36];		/* rate indexes, slowest first */
};

_Static_assert(sizeof(struct WlanRateControl) == 228, "rate control");
_Static_assert(offsetof(struct WlanRateControl, history) == 96, "rate history");
_Static_assert(offsetof(struct WlanRateControl, rates) == 192, "rate index list");

/* The timeout of a rate index (microseconds, inferred): 0x04000f7c. */
extern uint16_t wlanRateTimeoutTable[];

/* The rates' words (wlanRatePolicy[]) and their weights (wlanRateWeight[]), by index. */
extern const uint32_t wlanRatePolicy[WLAN_RATE_COUNT];		/* 0x53f8c */
extern const uint32_t wlanRateWeight[WLAN_RATE_COUNT];		/* 0x54008 */
/* The words of the rates the MAC uses first: set by the ROM (0x54e8c). */
extern uint32_t **wlanRateTable;

uint32_t wlanRateEntryRetrySet(uint32_t entry, unsigned int retries);		/* 0x2b834 */
unsigned int wlanRateEntryRetryGet(uint32_t entry);				/* 0x2b842 */
uint32_t wlanRateEntryEnableSet(uint32_t entry, unsigned int enable);		/* 0x2b84c */
unsigned int wlanRateEntryIndex(uint32_t entry);				/* 0x2b858 */
unsigned int wlanRateChainBuild(uint32_t *out, const uint32_t *in, unsigned int count,
				uint32_t mask);					/* 0x2b87c */
unsigned int wlanRateEntryClass(uint32_t entry);				/* 0x2b8d2 */
unsigned int wlanRateEntryBase(uint32_t entry);					/* 0x2b8ec */
void wlanRateTableRetryReset(void);						/* 0x2b906 */
unsigned int wlanRateIndexHt40(unsigned int class, unsigned int index);		/* 0x2b92c */
unsigned int wlanRateIndexOf(unsigned int class, unsigned int index);		/* 0x2b948 */
void wlanRateTableLongPreamble(uint8_t *table, int shortPreamble, int mode);	/* 0x2b96c */
void wlanRateWordsLongPreamble(uint32_t *words, int shortPreamble);		/* 0x2b9c4 */
void wlanRateTableShortGi(uint32_t *first, uint32_t *second, int enable);	/* 0x2b9e8 */
void wlanRateTableHtShortGi(uint8_t *table, int enable, int mode);		/* 0x2ba26 */
void wlanRateBlockBuild(const uint8_t *items, int count, uint8_t *blocks,
			uint32_t **table);					/* 0x2ba7e */
/* A slot of a rate table that is free while its first word is zero (12 bytes). */
struct WlanRateSlot {
	uint32_t used;
	uint32_t reserved[2];
};

struct WlanRateSlot *wlanRateSlotAlloc(struct WlanRateSlot *slots, unsigned int count); /* 0x2bc3c */
uint32_t *wlanRateTableLookup(unsigned int index);				/* 0x2bc6c */
void wlanRateHistoryRecord(struct WlanRateControl *rc, const int *value);	/* 0x2bc84 */
void wlanRateHistoryPush(struct WlanRateControl *rc, const int *value);		/* 0x2bca0 */
unsigned int wlanRateSearch(struct WlanRateControl *rc, unsigned int rate);	/* 0x2bcb2 */
void wlanRateHistoryTarget(struct WlanRateControl *rc, unsigned int rate, int divisor); /* 0x2bcdc */
int wlanRateHistoryClear(struct WlanRateControl *rc, int count);		/* 0x2bcf8 */
unsigned int wlanRateStepDown(unsigned int limit, struct WlanRateControl *rc);	/* 0x2bd0e */
unsigned int wlanRateStepUp(unsigned int limit, struct WlanRateControl *rc);	/* 0x2bd2e */
int wlanRateCounterReset(uint32_t *counter);					/* 0x2bd3a */
uint32_t wlanRateTimeoutValue(struct WlanRateControl *rc, uint32_t timeout, unsigned int shift); /* 0x2bd4a */
unsigned int wlanRateAdvance(struct WlanRateControl *rc);			/* 0x2bd6a */
int wlanRateHistoryFull(const uint32_t *window);				/* 0x2bd84 */
void wlanRateHistoryRestore(struct WlanRateControl *rc, int index);		/* 0x2bd9a */
int wlanRateBetter(int first, int second, struct WlanRateControl *rc);		/* 0x2bdb4 */
unsigned int wlanRateSelect(const struct WlanRateControl *rc);			/* 0x2be18 */
int wlanRateProbingAt(const struct WlanRateControl *rc, int index);		/* 0x2be20 */
uint32_t wlanRateConfigPack(const uint8_t *config);				/* 0x2be38 */

#endif /* ROBIN_RATECTL_H */
