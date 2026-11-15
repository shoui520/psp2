/*
 * Transmit statistics (0xed04..0xef78). Each finished frame is counted in
 * the 802.11 log (mlan_fw.h's HostCmd_DS_802_11_GET_LOG, which the host
 * reads with HostCmd_CMD_802_11_GET_LOG) and in the counters of the rates it
 * was sent at. The MAC counts the receive side and RTS exchanges itself;
 * those counters clear on read and are added in when the host asks.
 */
#include <stddef.h>
#include "robin/ratesel.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/signal.h"
#include "robin/soc.h"
#include "robin/txring.h"
#include "robin/txstats.h"
#include "robin/wlan.h"

/* MAC statistics counters, cleared when read. */
#define MAC_COUNT_RTS_FAILURE	REG32(0x8000a830)
#define MAC_COUNT_RTS_SUCCESS	REG32(0x8000a834)
#define MAC_COUNT_FCS_ERROR	REG32(0x8000a840)
#define MAC_COUNT_RX_FRAGMENT	REG32(0x8000a844)
#define MAC_COUNT_MCAST_RX	REG32(0x8000a848)
#define MAC_COUNT_DUPLICATE	REG32(0x8000a84c)

#define WLAN_FC0_TYPE(fc0)	(((fc0) >> 2) & 3)
#define WLAN_FC0_TYPE_DATA	2
#define WLAN_FC1_TO_DS		0x01

/* Not converted yet; provisional names. */

HostCmd_DS_802_11_GET_LOG wlanTxStatistics __attribute__((section(".dtcm.bss")));
struct WlanTxRateStats wlanTxRateStatistics[WLAN_TX_RATE_COUNT] __attribute__((section(".dtcm.bss")));

/* Count a finished frame in the counters of the rates it was sent at. */
void wlanTxStatsCountRate(struct WlanConnection *connection, const struct WlanTxStatus *status)
{
	uint8_t attempts[32];
	unsigned int rate = status->rate;
	unsigned int finalRate, retries;
	uint8_t i;

	if (rate >= WLAN_TX_RATE_COUNT)
		return;
	wlanTxRateStatistics[rate].frames++;
	__aeabi_memclr4(attempts, sizeof(attempts));
	wlanTxRateAttempts(status, attempts);
	finalRate = status->finalRate;
	retries = status->retries;
	wlanLinkTxDone(connection, status->result == 0);
	if (status->result == 0) {
		if (finalRate < WLAN_TX_RATE_COUNT)
			wlanTxRateStatistics[finalRate].acknowledged++;
		if (retries == 0) {
			wlanTxRateStatistics[rate].attempts++;
			return;
		}
	} else if (status->retryBlocked) {
		wlanTxRateStatistics[rate].blocked++;
	} else {
		wlanTxRateStatistics[rate].failed++;
	}
	for (i = 0; i < WLAN_TX_RATE_COUNT; i++)
		wlanTxRateStatistics[i].attempts += attempts[i];
}

/* Hand the rate counters to the host and start them again. */
void wlanTxStatsTakeRates(struct WlanTxRateStats *rates)
{
	__aeabi_memcpy4(rates, wlanTxRateStatistics, sizeof(wlanTxRateStatistics));
	__aeabi_memclr4(wlanTxRateStatistics, sizeof(wlanTxRateStatistics));
}

/* HostCmd_CMD_802_11_GET_LOG: add the MAC's counters in and copy the log out. */
void wlanTxStatsGetLog(HostCmd_DS_802_11_GET_LOG *log)
{
	wlanTxStatistics.frame_dup += MAC_COUNT_DUPLICATE;
	wlanTxStatistics.rts_success += MAC_COUNT_RTS_SUCCESS;
	wlanTxStatistics.rts_failure += MAC_COUNT_RTS_FAILURE;
	wlanTxStatistics.rx_frag += MAC_COUNT_RX_FRAGMENT;
	wlanTxStatistics.mcast_rx_frame += MAC_COUNT_MCAST_RX;
	wlanTxStatistics.fcs_error += MAC_COUNT_FCS_ERROR;
	__aeabi_memcpy(log, &wlanTxStatistics, sizeof(wlanTxStatistics));
}

/* Count a finished frame in the log, then in the rate counters. */
void wlanTxStatsCount(struct WlanConnection *connection, const struct WlanTxStatus *status)
{
	unsigned int retries = status->retries;

	wlanTxStatistics.ack_failure += retries;
	if (status->result != 0) {
		wlanTxStatistics.failed++;
	} else {
		const WlanWirelessFrame *frame = (const WlanWirelessFrame *)TX_DATA_FRAME(status->data);

		if (((frame->frameControl[1] & WLAN_FC1_TO_DS) &&
		     WLAN_FC0_TYPE(frame->frameControl[0]) == WLAN_FC0_TYPE_DATA &&
		     (frame->address3[0] & 1)) ||
		    (frame->address1[0] & 1))
			wlanTxStatistics.mcast_tx_frame++;
		wlanTxStatistics.tx_frame++;
		if (retries != 0) {
			wlanTxStatistics.retry++;
			if (retries > 1)
				wlanTxStatistics.multiretry++;
		}
	}
	wlanTxStatsCountRate(connection, status);
}

/* A frame dropped before it went out. */
void wlanTxStatsCountDropped(unsigned int rate, int blocked)
{
	wlanTxStatistics.failed++;
	wlanTxStatistics.tx_frame++;
	wlanTxRateStatistics[rate].frames++;
	if (blocked)
		wlanTxRateStatistics[rate].blocked++;
}

void wlanTxStatsReset(void)
{
	__aeabi_memclr4(&wlanTxStatistics, sizeof(wlanTxStatistics));
	__aeabi_memclr4(wlanTxRateStatistics, sizeof(wlanTxRateStatistics));
}
