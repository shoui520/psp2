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

/*
 * A slot of the hardware rate tables, one per connection (12 bytes); free while used is zero. The
 * chains are rows of wlanRateGroupChains[] and wlanRateUnicastChains[] (robin/ratesel.h).
 */
struct WlanRateSlot {
	uint32_t used;
	uint32_t *groupChain;		/* 8 rate words: the chain of the frames to a group address */
	uint32_t *unicastChain;		/* 3 rate words, the last at the first rate of the peer */
};

/* What the supervision of a peer's rate counts (32 bytes at WlanRateControl.window). */
struct WlanRateWindow {
	uint8_t rate;			/* the rate counted (fmCoexAdjustTable()); bit 7: it is a probe */
	uint8_t reserved1[3];
	uint32_t exchanges;		/* the frame exchanges the statuses reported (fmCoexVerifyTable()) */
	uint32_t sent;			/* the transmissions at the rate */
	uint32_t delivered;		/* ... that were acknowledged */
	uint32_t age;			/* milliseconds, up to 1000 (wlanPsValidateFrame()) */
	uint32_t validated;		/* the transmit statistics added since: macTxStatsAddValidated() */
	uint32_t prepared;		/* macTxStatsAddPrepared() */
	uint32_t checked;		/* macTxStatsAddChecked(), at most 16 */
};

/*
 * How the rate in use fares (3 bytes), counted by fmCoexClearTable() from the transmit statuses;
 * wlanPsConfigureFrame() maps the count of dropped frames: 0 to 2: level 0, 3 to 4: 1, above: 2.
 */
struct WlanRateLevel {
	uint8_t ackedRate;		/* the rate the last acknowledged frame was sent at */
	uint8_t ackedRepeats;		/* the frames acknowledged in a row at it */
	uint8_t dropped;		/* the frames dropped, with no transmission left, since one was acknowledged */
};

/* WlanConnection.rateControl (228 bytes). */
struct WlanRateControl {
	uint32_t elapsed;		/* milliseconds since the last decision, 10 per supervision tick */
	uint32_t frames;		/* the frames sent since: the prepared counts */
	uint32_t timeout;		/* 1000: nothing decided yet */
	uint32_t peerMask;		/* the rates the peer supports: one bit per rate index */
	uint32_t rateMask;		/* the ones in use: peerMask less what the configuration excludes */
	uint8_t last;			/* the last index of rates[]: the fastest */
	uint8_t reserved21[7];
	uint32_t highest;		/* the last index of rates[] in use */
	uint32_t lowest;		/* the first one */
	uint32_t target;		/* the index the next change moves to */
	uint32_t current;		/* the index in use */
	uint8_t historyCount;		/* records pushed since wlanRateHistoryClear() */
	uint8_t probing;		/* the current rate is a probe */
	uint8_t reserved46[14];
	struct WlanRateWindow window;
	struct WlanRateLevel level;
	uint8_t reserved95;
	struct WlanRateHistory history[7];
	uint8_t historyCursor;
	uint8_t reserved181[3];
	uint32_t idleTime;		/* milliseconds the supervision found nothing to count, up to 10000 */
	struct WlanRateSlot *slot;	/* the rate table's slot of this peer */
	uint8_t rates[32];		/* rate indexes, slowest first */
	uint8_t peerStatus;		/* what antDivRecord() / wlanPsHandlePacket() last said (0 to 2, 6) */
	uint8_t reserved225[3];
};

_Static_assert(sizeof(struct WlanRateControl) == 228, "rate control");
_Static_assert(offsetof(struct WlanRateControl, last) == 20, "rate last");
_Static_assert(offsetof(struct WlanRateControl, highest) == 28, "rate highest");
_Static_assert(offsetof(struct WlanRateControl, window) == 60, "rate window");
_Static_assert(offsetof(struct WlanRateControl, level) == 92, "rate level");
_Static_assert(offsetof(struct WlanRateControl, history) == 96, "rate history");
_Static_assert(offsetof(struct WlanRateControl, historyCursor) == 180, "rate history cursor");
_Static_assert(offsetof(struct WlanRateControl, idleTime) == 184, "rate idle time");
_Static_assert(offsetof(struct WlanRateControl, slot) == 188, "rate slot");
_Static_assert(offsetof(struct WlanRateControl, rates) == 192, "rate index list");
_Static_assert(offsetof(struct WlanRateControl, peerStatus) == 224, "rate peer status");

/* The timeout of a rate index (microseconds, inferred): the table at 0x04001034, set by wlanPsApplyPacket(). */
extern uint16_t *wlanRateTimeoutTable;			/* 0x04000f7c */

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
