/*
 * Transmit statistics (0xed04..0xef78): the 802.11 log the host reads with
 * HostCmd_CMD_802_11_GET_LOG and the per-rate counters. See wlan/txstats.c.
 */
#ifndef ROBIN_TXSTATS_H
#define ROBIN_TXSTATS_H

#include <stdint.h>

#include "robin/defrag.h"

struct WlanConnection;
struct WlanTxStatus;

#define WLAN_TX_RATE_COUNT	31

/* Counters of one rate index. */
struct WlanTxRateStats {
	uint32_t frames;		/* frames first sent at this rate */
	uint32_t acknowledged;		/* frames acknowledged at this rate */
	uint32_t attempts;		/* transmissions at this rate */
	uint32_t failed;		/* frames first sent at this rate and not acknowledged */
	uint32_t blocked;		/* the same, not to be sent again */
};

_Static_assert(sizeof(struct WlanTxRateStats) == 20, "rate counters");

extern HostCmd_DS_802_11_GET_LOG wlanTxStatistics;
extern struct WlanTxRateStats wlanTxRateStatistics[WLAN_TX_RATE_COUNT];

void wlanTxStatsCountRate(struct WlanConnection *connection, const struct WlanTxStatus *status);
void wlanTxStatsTakeRates(struct WlanTxRateStats *rates);
void wlanTxStatsGetLog(HostCmd_DS_802_11_GET_LOG *log);
void wlanTxStatsCount(struct WlanConnection *connection, const struct WlanTxStatus *status);
void wlanTxStatsCountDropped(unsigned int rate, int blocked);
void wlanTxStatsReset(void);

#endif /* ROBIN_TXSTATS_H */
