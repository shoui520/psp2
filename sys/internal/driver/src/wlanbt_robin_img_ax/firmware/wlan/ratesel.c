/*
 * The rate selection (0x3c740 - 0x3d4c6): the order of the rates a connection may use, the decision
 * to move to another one, the host's rate configuration and the A-MSDU size that suits a rate.
 * The file's name and the descriptive names of the variables are inferred; the functions keep the
 * firmware's placeholders. See robin/ratesel.h.
 */
#include <stddef.h>
#include "robin/btrom.h"
#include "robin/conn.h"
#include "robin/calibration.h"
#include "robin/htba.h"
#include "robin/macentry.h"
#include "robin/macrate.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/peerrate.h"
#include "robin/ratectl.h"
#include "robin/ratesel.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/signal.h"
#include "robin/stream.h"
#include "robin/subscribe.h"
#include "robin/txpower.h"
#include "robin/txring.h"

#define PUT16(p, v)	do { (p)[0] = (uint8_t)(v); (p)[1] = (uint8_t)((v) >> 8); } while (0)

/* The rates the legacy part of a rate mask has: CCK 0 to 3, OFDM 5 to 12. */
#define WLAN_RATE_LEGACY_MASK		0x00001fef
#define WLAN_RATE_HT_MASK		0x7fffc000
#define WLAN_RATE_DEFAULT_MASK		0x7fffdfef	/* every rate but index 4 and 13 (the two unused) */
#define WLAN_RATE_HT40_MASK		0x803fffff	/* ... and the lowest HT40 ones, as for HT20 */

#define TLV_RATE_DROP_PATTERN		0x151
#define TLV_RATE_DROP_CONTROL		0x152
#define TLV_RATE_SCOPE			0x153
#define TLV_TX_RATE			0x162

/* The tables, by rate index. */
const uint16_t wlanRateBurstBytes[17] = {
	1222, 2444, 3666, 4888, 7332, 9776, 10998, 12220,
	1128, 2538, 5076, 7614, 10152, 15228, 20304, 22842, 25380,
};

const uint32_t wlanRateKbps[17] = {
	6500, 13000, 19500, 26000, 39000, 52000, 58500, 65000,
	6000, 13500, 27000, 40500, 54000, 81000, 108000, 121500, 135000,
};

const uint8_t wlanRateNext[WLAN_RATE_COUNT] = {
	1, 2, 5, 6, 9, 22, 7, 15, 9, 16, 17, 12, 18, 18, 3, 23,
	10, 24, 25, 26, 21, 27, 14, 8, 11, 19, 20, 28, 29, 30, 30,
};

const uint8_t wlanRatePrevious[WLAN_RATE_COUNT] = {
	0, 0, 1, 14, 9, 2, 3, 6, 23, 8, 16, 24, 11, 12, 22, 7,
	9, 10, 12, 25, 26, 20, 5, 15, 17, 18, 19, 21, 27, 28, 29,
};

const uint32_t wlanRateModeMask[WLAN_RATE_COUNT] = {
	0x7fffdfef, 0x7fffdfee, 0x7fffdfec, 0x7fbf9fc8, 0x7f3f1c00, 0x7fffdfe8, 0x7fbf9fc0, 0x7fbf9f80,
	0x7f3f1f00, 0x7f3f1e00, 0x7f3e1c00, 0x7e3c1800, 0x7e3c1000, 0x78000000, 0x7fbfdfc8, 0x7fbf9f00,
	0x7f3f1c00, 0x7f3e1800, 0x7e3c0000, 0x7c380000, 0x78300000, 0x78200000, 0x7fffdfc8, 0x7fbf1f00,
	0x7f3c1800, 0x7e380000, 0x7c300000, 0x78000000, 0x70000000, 0x60000000, 0x40000000,
};

const uint8_t wlanRateModeFixed[28] = {
	254, 254, 254, 254, 254, 254, 254, 254, 0, 254, 254, 254, 254, 254,
	14, 14, 14, 14, 14, 14, 14, 15, 16, 17, 18, 19, 20, 21,
};

/* What the selection of the rate is set to (the values are the firmware's initial ones). */
struct WlanRateSelection wlanRateSelection __attribute__((section(".dtcm.rw"))) = {	/* 0x4000ff4 */
	.fixedRate = 0,
	.amsduRate = 31,
	.previousRate = 31,
	.amsduBufferSize = 0xffff,
	.amsduLarge = 1860,
	.amsduMedium = 1860,
	.raiseLarge = { 9000, 9000 },
	.raiseMedium = { 4500, 9000 },
	.lowerMedium = { 6000, 3000 },
	.lowerSmall = { 3000, 3000 },
	.burstBytes = {
		1265, 2531, 3797, 5062, 7594, 10125, 11390, 12656,
		1167, 2531, 5063, 7594, 10125, 15188, 20250, 22781, 25313,
	},
};

uint16_t *wlanRateBurstTable __attribute__((section(".dtcm.bss")));			/* 0x4000f7c */
const uint32_t *wlanRateWeightTable __attribute__((section(".dtcm.bss")));		/* 0x4000f80 */

/* The A-MSDU size a rate gets: it only grows while the rate rises, and only shrinks while it falls. */
unsigned int wlanPsNotifyBuffer(unsigned int rate)
{
	struct WlanRateSelection *selection = &wlanRateSelection;
	unsigned int width;
	unsigned int burst;
	unsigned int size;
	unsigned int previous;

	if (rate < 14) {
		size = 0;
	} else {
		width = rate > 21;
		previous = selection->previousRate;
		size = selection->amsduLarge;
		burst = wlanRateBurstBytes[rate - 14];
		if (rate <= previous) {
			if (burst >= selection->lowerSmall[width]) {
				if (burst < selection->lowerMedium[width])
					size = selection->amsduMedium;
			} else {
				size = selection->amsduSmall;
			}
			if (selection->amsduBufferSize < size)
				size = selection->amsduBufferSize;
		} else {
			if (burst <= selection->raiseLarge[width]) {
				if (burst <= selection->raiseMedium[width])
					size = selection->amsduSmall;
				else
					size = selection->amsduMedium;
			}
			if (selection->amsduBufferSize > size)
				size = selection->amsduBufferSize;
		}
	}
	selection->previousRate = rate;
	return size;
}

/* Tells the host the A-MSDU size of the rate, when it changed. */
void wlanPsFormatBuffer(struct WlanConnection *connection, unsigned int rate)
{
	struct WlanRateSelection *selection = &wlanRateSelection;
	unsigned int size;

	if (selection->amsduEnable == 0)
		return;
	if (wlanConnectionIsHt(connection) == 0)
		return;
	if (!(connection->flags2 & WLAN_CONN2_ASSOCIATED))
		return;
	if (connection->mode != 3 && connection->mode != 1)
		return;
	if (selection->amsduRate == rate)
		return;
	size = wlanPsNotifyBuffer(rate);
	if (size != selection->amsduBufferSize) {
		wlanSubscribeEventsNotify(connection);
		selection->amsduRate = rate;
		selection->amsduBufferSize = size;
	}
}

/*
 * Orders the rates the connection may use, slowest first, in rates[]; rateControl.last is the
 * index of the fastest. A connection that has HT rates loses the fastest legacy ones; MCS 0 of
 * HT20 and the 6 Mbit/s rate come first where they are allowed and the order puts them elsewhere.
 */
int wlanPsInspectBuffer(struct WlanConnection *connection)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;
	uint8_t *rates = rc->rates;
	uint32_t mask = rc->rateMask;
	unsigned int last = 0;
	unsigned int rate;
	int key;
	int j;

	if (mask == 0) {
		rates[0] = 0;
	} else {
		if (mask & WLAN_RATE_HT_MASK)
			mask &= ~(wlanPsConvertPacket(wlanPsAdjustPacket()) & WLAN_RATE_LEGACY_MASK);
		if (mask & 0x4008)
			mask &= ~0x40u;
		rates[0] = 30;
		for (rate = 0; rate <= 30; rate = wlanPsClearPacket(rate)) {
			key = (int)romLibBuildEntry(rate);
			if ((1u << rate) & mask) {
				for (;;) {
					if ((int)romLibBuildEntry(rates[last]) < key) {
						last = (uint8_t)(last + 1);
						break;
					}
					if (last == 0)
						break;
					last = (uint8_t)(last - 1);
				}
				rates[last] = rate;
			}
			if (rate >= 30)
				break;
		}
		if (rates[0] == 23) {
			if (!(mask & 0x4000) || last >= 30)
				goto done;
			last = (uint8_t)(last + 1);
			for (j = last; j > 0; j--)
				rates[j] = rates[j - 1];
			rates[0] = 14;
		} else if (rates[0] != 14) {
			goto done;
		}
		if ((mask & 0x20) && last < 30) {
			last = (uint8_t)(last + 1);
			for (j = last; j > 0; j--)
				rates[j] = rates[j - 1];
			rates[0] = 5;
		}
	}
done:
	rc->last = last;
	if (connection->interfaceType != 2 && wlanRateSelection.fixedRate != 0) {
		wlanRateResponseContext = rc->rates[0];
		wlanTxHookIdle();
		wlanRateResponseTableBuild(connection->rateSet, wlanRateResponseContext);
	}
	return 1;
}

/* The connection starts over at its fastest rate. */
int wlanPsTrackBuffer(struct WlanConnection *connection)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;
	unsigned int rate;

	wlanPsInspectBuffer(connection);
	rc->lowest = 0;
	rc->highest = rc->last;
	rc->target = rc->last;
	rc->current = rc->last;
	rc->probing = 0;
	rc->historyCursor = 0;
	rate = wlanRateSelect(rc);
	wlanPsSyncFrame(connection, rate, rc);
	fmCoexAdjustTable(rc);
	wlanRateHistoryClear(rc, 3);
	wlanTxBaArmTimeouts((uintptr_t)connection, wlanTxBaSessions);
	wlanPsFormatBuffer(connection, rate);
	return 1;
}

int wlanPsManageBuffer(struct WlanConnection *connection)
{
	(void)connection;
	return 0;
}

/* A connection takes a peer: its supervision starts. */
int wlanPsAcquireBuffer(struct WlanConnection *connection)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;

	rc->frames = 0;
	rc->timeout = 1000;
	rc->elapsed = 0;
	rc->historyCursor = 0;
	wlanRateHistoryClear(rc, 3);
	if (rc->slot == NULL)
		rc->slot = wlanRateSlotAlloc(wlanRateSlots, 19);
	return wlanPsTrackBuffer(connection);
}

/* The bytes a burst of each HT rate holds, at most what half of the 28-block pool holds. */
unsigned int wlanPsReleaseBuffer(void)
{
	unsigned int limit = 1544 * ((unsigned int)osa_PoolCapacity(&wlanBlockPool28) >> 1);
	unsigned int result = 0;
	unsigned int i;

	for (i = 0; i < 17; i++) {
		result = (1504 * wlanRateKbps[i] / 1000) >> 3;
		if (result > limit)
			result = limit;
		wlanRateBurstTable[i] = result;
	}
	return result;
}

/* Takes the rate at index; the counters of the window start over. */
__attribute__((noinline)) int wlanPsSetupBuffer(struct WlanConnection *connection, unsigned int index)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;

	rc->current = index;
	wlanRateCounterReset(&rc->elapsed);
	wlanPsSyncFrame(connection, rc->rates[index], rc);
	wlanTxBaArmTimeouts((uintptr_t)connection, wlanTxBaSessions);
	return index;
}

/* Moves up: to the next rate, or at the top to the short guard interval. 255: no way up. */
int wlanPsClearBuffer(struct WlanConnection *connection)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;
	unsigned int highest = rc->highest;
	unsigned int target = rc->target;
	unsigned int rate;
	unsigned int index;

	if (target >= highest) {
		rate = rc->rates[target];
		if ((rate != 21 && rate != 30) || rc->probing == 1 || wlanHtShortGiWanted(connection, rate) == 0)
			return 255;
		rc->probing = 1;
		index = rc->target;
	} else {
		index = wlanRateStepUp(target, rc);
	}
	return wlanPsSetupBuffer(connection, index);
}

__attribute__((noinline)) int wlanPsConvertBuffer(struct WlanConnection *connection)
{
	return wlanPsSetupBuffer(connection, wlanRateAdvance(&connection->peerLink.rateControl));
}

__attribute__((noinline)) int wlanPsVerifyBuffer(struct WlanConnection *connection)
{
	return wlanPsSetupBuffer(connection, connection->peerLink.rateControl.target);
}

/* The history says to stay: back off the supervision, or try the rate above. */
int wlanPsSelectBuffer(struct WlanConnection *connection)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;
	uint8_t cursor = rc->historyCursor;

	rc->historyCursor = 2;
	if (wlanPsClearBuffer(connection) == 255) {
		rc->historyCursor = cursor;
		if (rc->timeout == 10)
			rc->timeout = 100;
		else
			rc->timeout = 1000;
	}
	rc->frames = 0;
	rc->elapsed = 0;
	return 0;
}

__attribute__((noinline)) int wlanPsAdjustBuffer(struct WlanConnection *connection)
{
	connection->peerLink.rateControl.historyCursor = 1;
	return wlanPsConvertBuffer(connection);
}

/* The decision the supervision made (0: stay, 1 to 3: the rate the history of that kind gave). */
__attribute__((noinline)) void wlanPsRecordBuffer(struct WlanConnection *connection, unsigned int decision)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;

	if (decision <= 3) {
		if (decision == 0)
			rc->timeout = 1000;
		else if (decision == 2)
			rc->timeout = 10;
		else
			rc->timeout = 100;
		wlanRateHistoryRestore(rc, decision);
		wlanPsVerifyBuffer(connection);
	}
}

/* One look at the window of a connection that is being supervised: 1 when it moved to another rate. */
int wlanPsFilterBuffer(struct WlanConnection *connection)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;
	unsigned int rate = rc->rates[rc->current];
	int noShortGuard = 0;
	int ampdu;
	int status;
	int trend;
	int better;
	int decision;
	int percent;
	uint8_t level;

	if (rc->probing != 0)
		noShortGuard = wlanHtShortGiWanted(connection, rate) == 0;
	if (connection->peerLink.baActive[0] != 0 || connection->peerLink.baActive[1] != 0)
		ampdu = wlanRateAmpduAllowed(rate) != 0;
	else
		ampdu = 0;
	status = wlanPsParseFrame(&rc->window, &percent, &level, ampdu);
	if (status == 2 && !noShortGuard)
		return 0;
	fmCoexAdjustTable(rc);
	wlanRateHistoryPush(rc, &percent);
	if ((level | noShortGuard) != 0) {
		trend = rc->historyCursor;
		if (trend == 2)
			trend = 0;
	} else {
		trend = wlanRateBetter(rc->historyCursor, 0, rc);
	}
	better = wlanPsCheckFrame((uint8_t *)&rc->level);
	if (better != 255 && better != (int)rate) {
		wlanRateHistoryTarget(rc, better, 3);
		trend = wlanRateBetter(3, trend, rc);
	}
	if (trend == 0) {
		if (rc->historyCount < 3) {
			if (rc->historyCursor == 1) {
				if (wlanRateHistoryFull((const uint32_t *)rc))
					wlanPsSelectBuffer(connection);
				if (rc->historyCursor == 2)
					return 0;
			} else if (rc->historyCursor != 2) {
				return 0;
			}
		}
		decision = 0;
	} else if (trend == 1 || trend == 2 || trend == 3) {
		decision = trend;
	} else {
		return 0;
	}
	wlanPsRecordBuffer(connection, decision);
	return 1;
}

/* Every 10 ms: 1 while the supervision has something to do. */
int wlanPsHandlePacket(struct WlanConnection *connection)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;
	unsigned int rate;
	int ampdu;
	int status;
	int percent;
	uint8_t level;

	if (wlanTxRateFlags & 0x40) {
		wlanPsTrackBuffer(connection);
		rc->frames = 0;
		rc->elapsed = 0;
		rc->timeout = 1000;
		return 0;
	}
	rc->elapsed += 10;
	if (rc->historyCursor != 0) {
		if (wlanPsFilterBuffer(connection) == 1) {
			wlanRateHistoryClear(rc, 3);
			wlanPsFormatBuffer(connection, rc->rates[rc->target]);
		}
	} else {
		rate = rc->rates[rc->current];
		if (rc->probing != 0 && wlanHtShortGiWanted(connection, rate) == 0)
			rc->probing = 0;
		if (connection->peerLink.baActive[0] != 0 || connection->peerLink.baActive[1] != 0)
			ampdu = wlanRateAmpduAllowed(rate) != 0;
		else
			ampdu = 0;
		status = wlanPsParseFrame(&rc->window, &percent, &level, ampdu);
		if (status != 2) {
			fmCoexAdjustTable(rc);
			if (status == 0) {
				wlanRateHistoryPush(rc, &percent);
				if (rc->target > rc->lowest) {
					wlanPsAdjustBuffer(connection);
					return rc->historyCursor != 0 || rc->timeout != 1000;
				}
				if (!wlanRateHistoryFull((const uint32_t *)rc))
					return rc->historyCursor != 0 || rc->timeout != 1000;
			} else {
				if (!wlanRateHistoryFull((const uint32_t *)rc))
					return rc->historyCursor != 0 || rc->timeout != 1000;
				wlanRateHistoryPush(rc, &percent);
			}
			wlanPsSelectBuffer(connection);
		}
	}
	return rc->historyCursor != 0 || rc->timeout != 1000;
}

/* The transmit status of a frame is added to the window, unless the connection is protected from low rates. */
int wlanPsProcessPacket(struct WlanRateControl *rateControl, struct WlanTxStatus *status)
{
	if (wlanRadioConfig->primaryDiversity != 0 || !(wlanTxRateFlags & 0x40)) {
		rateControl->frames += fmCoexSelectTable(&rateControl->window, status);
		rateControl->idleTime = 0;
		return 0;
	}
	return wlanTxRateFlags << 25;
}

__attribute__((noinline)) int wlanPsUpdatePacket(void)
{
	return 1;
}

/* The rate configuration the host sent: false when the host's scope allows none of the connection's rates. */
int hostCmd00d6Apply(struct WlanConnection *connection, uint8_t *rateScope, uint8_t *dropPattern,
		     uint8_t *fixedRate)
{
	struct WlanRateSelection *selection = &wlanRateSelection;
	uint32_t previous;

	if (rateScope != NULL) {
		previous = selection->txRateMask;
		selection->txRateMask = wlanRateConfigPack(rateScope);
		if (wlanPsFilterPacket(connection)) {
			selection->txRateMask = previous;
			return 1;
		}
	}
	if (dropPattern != NULL)
		wlanPsCheckPacket(connection, dropPattern);
	if (fixedRate != NULL) {
		unsigned int value = fixedRate[4] | fixedRate[5] << 8;

		if (value != 0)
			connection->groupRate = romRateIndex(fixedRate[4]);
		else
			connection->groupRate = 254;
	}
	wlanPsAcquirePacket(connection);
	macTxNotifyApplyFrame(connection);
	return 0;
}

/* The rate configuration the host reads back: the rate scope, the drop pattern and the fixed rate, as TLVs. */
unsigned int hostCmd00d6Get(struct WlanConnection *connection, uint8_t *data, uint32_t rateMask)
{
	uint8_t *records = data + 24;
	uint8_t *tlv;
	unsigned int count = 0;
	unsigned int rate;
	unsigned int value;

	PUT16(data + 4, rateMask & 0xf);
	PUT16(data + 6, (rateMask << 19) >> 24);
	__aeabi_uwrite4((rateMask << 10) >> 24, data + 8);
	__aeabi_uwrite4((rateMask << 9) >> 31, data + 12);
	__aeabi_uwrite4(0, data + 16);
	__aeabi_uwrite4(0, data + 20);
	PUT16(data, TLV_RATE_SCOPE);
	data[2] = 20;
	data[3] = 0;
	for (rate = 0; rate <= 22; rate++) {
		if (rate != 4 && rate != 13 && (rateMask >> rate) & 1) {
			uint8_t *record = records + 36 * count;

			wlanPsManageFrame(record + 12, wlanRateTable[rate], 8);
			PUT16(record + 8, TLV_RATE_DROP_CONTROL);
			record[10] = 32;
			record[11] = 0;
			count = (uint16_t)(count + 1);
		}
	}
	__aeabi_uwrite4(3, records + 4);
	PUT16(records, TLV_RATE_DROP_PATTERN);
	PUT16(records + 2, (uint16_t)(36 * count + 4));
	tlv = records + 4 + (uint16_t)(36 * count + 4);
	if (connection->groupRate == 254)
		value = 0;
	else
		value = romRateValue(connection->groupRate);
	PUT16(tlv + 4, value);
	PUT16(tlv, TLV_TX_RATE);
	tlv[2] = 2;
	tlv[3] = 0;
	return (uint16_t)(tlv - data + 6);
}

/* The width the connection's rates are chosen for changed. */
void wlanRateChannelWidthSet(struct WlanConnection *connection, int update, int width)
{
	struct WlanRateSelection *selection = &wlanRateSelection;

	if (selection->channelWidth != (uint32_t)width || update != 0) {
		selection->channelWidth = width;
		wlanPsAcquirePacket(connection);
		macTxNotifyApplyFrame(connection);
	}
}

void wlanTxRateAttempts(const struct WlanTxStatus *status, uint8_t *attempts)
{
	__aeabi_memcpy(attempts, status->attempts, sizeof(status->attempts));
}

/* The host's rate configuration starts over. */
__attribute__((noinline)) void wlanPsBuildPacket(void)
{
	struct WlanRateSelection *selection = &wlanRateSelection;

	selection->txRateMask = WLAN_RATE_DEFAULT_MASK;
	selection->fixedRate = 0;
	selection->eventRate = 0;
	selection->allowedRates = WLAN_RATE_DEFAULT_MASK;
}

unsigned int wlanPsComputePacket(void)
{
	wlanPsBuildPacket();
	wlanRateSelection.mode = 1;
	return wlanPsInspectFrame();
}

/* The A-MSDU sizes follow the size of the transmit event buffers. */
void wlanPsQueryPacket(void)
{
	struct WlanRateSelection *selection = &wlanRateSelection;

	selection->amsduSmall = 0;
	if (wlanMgmtFrameSize >= 7000) {
		selection->amsduMedium = 3720;
		selection->amsduLarge = 7440;
	} else if (wlanMgmtFrameSize < 3000) {
		selection->amsduMedium = 1860;
		selection->amsduLarge = 1860;
	} else {
		selection->amsduMedium = 3720;
		selection->amsduLarge = 3720;
	}
	selection->amsduBufferSize = 0xffff;
	selection->previousRate = 31;
	selection->amsduRate = 31;
}

void wlanPsApplyPacket(void)
{
	wlanRateWeightTable = wlanRateWeight;
	wlanRateBurstTable = wlanRateSelection.burstBytes;
}

/* The signal of a received frame; nothing else is learned of it here. */
int wlanPsResolvePacket(struct WlanConnection *connection, struct WlanRxStatus *status)
{
	wlanSignalData(connection, status);
	return wlanPsUpdatePacket();
}

int wlanPsSchedulePacket(struct WlanConnection *connection, struct WlanTxStatus *status)
{
	int result;

	fmCoexConvertTable(status);
	result = wlanTxPowerClassMaximumGet(status->rateOption);
	if (!((*(const uint8_t *const *)(status->data + 96))[6] & 1))
		macTxNotifyResetFrame(connection, status);
	return result;
}

/* The fastest rate the connection may use. */
__attribute__((noinline)) int wlanPsNotifyPacket(struct WlanConnection *connection)
{
	uint32_t mask = connection->peerLink.rateControl.rateMask;
	int rate;

	for (rate = 30; rate != 0; rate = wlanRatePrevious[rate]) {
		if ((1u << rate) & mask)
			break;
	}
	return rate;
}

/* The rate a connection is sent control and management frames at, as the speed in kbit/s. */
int wlanPsFormatPacket(struct WlanConnection *connection)
{
	int rate;

	if (connection->flags2 & WLAN_CONN2_ASSOCIATED)
		rate = wlanPsNotifyPacket(connection);
	else if (macMgmtParseContext() == 3)
		rate = 30;
	else if (macMgmtParseContext() != 0)
		rate = 12;
	else
		rate = 3;
	return 1000 * romLibComputeEntry(rate);
}

/* The single rate a mask allows: 1 when it is one, and *rate is it (0 when the mask holds none of 0 to 22). */
int wlanPsInspectPacket(struct WlanConnection *connection, uint8_t *rate)
{
	uint32_t mask = connection->peerLink.rateControl.rateMask;
	unsigned int index;

	if (mask == 0)
		return 0;
	if ((mask & 0x807fffff) & ((mask & 0x807fffff) - 1))
		return 0;
	*rate = 0;
	for (index = 30; !((1u << index) & mask); ) {
		index = (uint8_t)(index - 1);
		if (index == 0)
			return 1;
	}
	*rate = index;
	return 1;
}

/* The rate selection starts at the lowest rate of a legacy station, MCS 0 of an access point's... (the IBSS's). */
__attribute__((noinline)) int wlanPsTrackPacket(uint8_t *rate)
{
	if (macMgmtParseContext() == 2)
		*rate = 5;
	else
		*rate = 0;
	return 1;
}

/* Walks the rates of the connection's mask from *rate down (direction -1) or up (1); 1 when one was found. */
int wlanPsManagePacket(struct WlanConnection *connection, uint8_t *rate, int direction)
{
	uint32_t mask = connection->peerLink.rateControl.rateMask;
	unsigned int index = *rate;

	if (mask == 0)
		return 0;
	if (direction == -1) {
		while (!((1u << index) & mask)) {
			index = wlanRatePrevious[index];
			mask &= (~wlanRateModeMask[index] & WLAN_RATE_DEFAULT_MASK) | (1u << index);
			if (index == 0 || mask == 0)
				return 0;
		}
	} else if (direction == 1) {
		while (!((1u << index) & mask)) {
			index = wlanRateNext[index];
			mask &= wlanRateModeMask[index];
			if (index >= 30 || mask == 0)
				return 0;
		}
	} else {
		return 0;
	}
	*rate = index;
	return 1;
}

/* The connection's rates: those it and its peer support, the host allows and the width of the channel carries. */
__attribute__((noinline)) int wlanPsAcquirePacket(struct WlanConnection *connection)
{
	struct WlanRateSelection *selection = &wlanRateSelection;
	struct WlanRateControl *rc = &connection->peerLink.rateControl;
	uint32_t widthMask = WLAN_RATE_HT40_MASK;
	uint32_t cckMask = WLAN_RATE_DEFAULT_MASK;
	uint8_t chosen[4];
	int result = 1;

	if (connection->mode == 2) {
		if (wlanHtChannelWidth(connection) != 0)
			cckMask = WLAN_RATE_HT40_MASK;
		rc->rateMask = cckMask & rc->peerMask;
	} else {
		if (wlanHtChannelWidth(connection) == 0)
			widthMask = WLAN_RATE_DEFAULT_MASK;
		if (__aeabi_uread4(wlanRateRestriction) != 0)
			cckMask = 15;
		chosen[0] = selection->fixedRate;
		selection->allowedRates = widthMask & cckMask & rc->peerMask & selection->txRateMask
					  & wlanRateModeMask[selection->fixedRate];
		rc->rateMask = selection->allowedRates;
		if (wlanPsManagePacket(connection, &chosen[0], 1) == 0) {
			wlanPsTrackPacket(&chosen[0]);
			result = 0;
		}
		if (selection->mode == 2)
			selection->eventRate = chosen[0];
		else
			selection->eventRate = 0;
		if (wlanPsInspectPacket(connection, &connection->dataRate) == 0)
			connection->dataRate = 254;
	}
	return result;
}

/* The peer's rates changed. */
int wlanPsReleasePacket(struct WlanConnection *connection, uint32_t rateMask)
{
	connection->peerLink.rateControl.peerMask = rateMask;
	return wlanPsAcquirePacket(connection);
}

int wlanPsSetupPacket(struct WlanPeerLink *link, uint32_t peerMask)
{
	link->rateControl.peerMask = peerMask;
	link->rateControl.rateMask = link->rateControl.peerMask & wlanRateSelection.txRateMask;
	return 1;
}

/* The next rate after index, in the order of the table: the one the mask of that rate says. */
__attribute__((noinline)) int wlanPsClearPacket(unsigned int index)
{
	return wlanRateNext[index];
}

__attribute__((noinline)) uint32_t wlanPsConvertPacket(unsigned int index)
{
	return wlanRateModeMask[index];
}

/* The slowest rate a connection may use, 0 when it has none and no protected rate either. */
__attribute__((noinline)) unsigned int wlanPsVerifyPacket(struct WlanConnection *connection)
{
	uint32_t mask = connection->peerLink.rateControl.rateMask;
	unsigned int rate;

	for (rate = 0; rate < 30; rate = wlanRateNext[rate]) {
		if ((1u << rate) & mask)
			break;
	}
	if (rate == 30 && !(mask & 0x40000000))
		return 0;
	return rate;
}

/* Whether the rate may be carried in an A-MPDU: HT rates but MCS 32, and no HT40 rate below MCS 5... (mask 0x7f3f0000). */
__attribute__((noinline)) int wlanRateAmpduAllowed(unsigned int rate)
{
	return ((1u << (uint8_t)rate) & 0x7f3f0000) != 0;
}

/* The mask a mode's rate is taken from (the fixed rate selects the row). */
__attribute__((noinline)) int wlanPsAdjustPacket(void)
{
	return 16;
}

/* The rate a data frame is sent at: the one in use, or offset rates away from it, within what the connection allows. */
__attribute__((noinline)) unsigned int wlanTxDataRate(struct WlanConnection *connection, int offset)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;
	int index = rc->current;
	unsigned int rate;
	unsigned int highest;
	unsigned int lowest;

	if (offset == 0)
		return rc->rates[index];
	index += offset;
	if (rc->last < index)
		index = rc->last;
	if (index < 0)
		index = 0;
	rate = rc->rates[index];
	lowest = wlanPsVerifyPacket(connection);
	highest = wlanPsNotifyPacket(connection);
	if (rate > highest)
		return highest;
	if (rate < lowest)
		return lowest;
	return rate;
}

/* 1 when the host's mask leaves the connection no rate. */
int wlanPsFilterPacket(struct WlanConnection *connection)
{
	struct WlanRateSelection *selection = &wlanRateSelection;
	uint32_t widthMask = WLAN_RATE_HT40_MASK;
	uint32_t cckMask = WLAN_RATE_DEFAULT_MASK;
	uint32_t mask;
	uint32_t allowed;

	if (connection->mode == 2) {
		if (wlanHtChannelWidth(connection) != 0)
			cckMask = WLAN_RATE_HT40_MASK;
		allowed = connection->peerLink.rateControl.peerMask;
		mask = cckMask;
	} else {
		if (wlanHtChannelWidth(connection) == 0)
			widthMask = WLAN_RATE_DEFAULT_MASK;
		if (__aeabi_uread4(wlanRateRestriction) != 0)
			cckMask = 15;
		mask = widthMask & cckMask & connection->peerLink.rateControl.peerMask & selection->txRateMask;
		allowed = wlanRateModeMask[selection->fixedRate];
	}
	return (mask & allowed) == 0;
}

/* The rate a frame of the class is sent at. */
unsigned int wlanPsHandleFrame(struct WlanConnection *connection, unsigned int mode)
{
	unsigned int rate;

	if (connection->mode == 2 || wlanRateSelection.mode == 0)
		return wlanTxControlRate(connection);
	rate = wlanRateModeFixed[mode];
	if (rate != 254)
		return rate;
	switch (mode) {
	case 0:
	case 2:
	case 4:
	case 11:
	case 13:
		return wlanTxControlRate(connection);
	case 1:
	case 3:
	case 5:
	case 9:
	case 10:
	case 12:
		return wlanTxDataRate(connection, 0);
	default:
		return 0;
	}
}

unsigned int wlanPsProcessFrame(struct WlanConnection *connection, unsigned int mode, int broadcast)
{
	if (broadcast != 0)
		return wlanTxControlRate(connection);
	return wlanPsHandleFrame(connection, mode);
}

/* The HT rate's MCS and whether it is a 40 MHz one; 0 for the legacy rates. */
int wlanPsUpdateFrame(unsigned int rate, uint8_t *wide, uint8_t *index)
{
	if (rate < 14)
		return 0;
	if (rate > 21) {
		if (rate == 22) {
			*wide = 1;
			*index = 32;
		} else if (rate <= 30) {
			*wide = 1;
			*index = rate - 23;
		} else {
			return 0;
		}
	} else {
		*wide = 0;
		*index = rate - 14;
	}
	return 1;
}

/* How the rate in use fares: 0 well, 1 so-so (3 or 4 dropped), 2 badly. */
__attribute__((noinline)) int wlanPsConfigureFrame(const struct WlanRateLevel *level)
{
	unsigned int dropped = level->dropped;

	if (dropped < 5)
		return dropped >= 3;
	return 2;
}

/* The rate the last frames were acknowledged at, once there were three in a row at it; 255 before. */
__attribute__((noinline)) int wlanPsCheckFrame(const uint8_t *level)
{
	unsigned int repeats = level[1];
	int rate = level[0];

	if (repeats < 3)
		return 255;
	return rate;
}

void wlanPsValidateFrame(struct WlanRateWindow *window)
{
	unsigned int age = window->age;

	if (age < 1000)
		window->age = age + 10;
}

/* The transmissions the window counts beyond what the hardware had checked. */
int wlanPsPrepareFrame(const struct WlanRateWindow *window)
{
	unsigned int validated = window->validated;
	unsigned int checked = window->checked;
	int result = window->sent;

	if (validated > checked)
		result += validated - checked;
	return result;
}

void wlanPsDispatchFrame(const struct WlanRateWindow *window, unsigned int *exchanges, unsigned int *excess)
{
	unsigned int checked;
	unsigned int validated;
	unsigned int difference;

	if ((window->rate & 0x7f) >= 14) {
		checked = window->checked;
		validated = window->validated;
		if (validated > checked) {
			difference = validated - checked;
			*exchanges = difference;
			if (difference > window->prepared)
				*excess = difference - window->prepared;
		}
	}
}

/* Reads the window: *percent is the share of the transmissions that were acknowledged (0 to 100). */
int wlanPsParseFrame(struct WlanRateWindow *window, int *percent, uint8_t *level, int ampdu)
{
	int status = wlanPsConfigureFrame((const struct WlanRateLevel *)((uint8_t *)window + 32));

	*level = status;
	if (status != 0) {
		*percent = 100;
		return 0;
	}
	return fmCoexReleaseTable(window, percent, ampdu);
}

/* A rate drop pattern TLV the host sent: mode 0 clears the pattern, 1 resets the retries, 2 and 3 list the rates' drop controls. */
void wlanPsConfigurePacket(const uint8_t *tlv)
{
	uint32_t mode = __aeabi_uread4(tlv + 4);
	const uint8_t *entry = tlv + 8;
	unsigned int count;
	unsigned int i;

	wlanPsInspectFrame();
	wlanPsTrackFrame();
	if (mode == 2) {
		count = (entry[3] << 8 | entry[2]) >> 2;
		for (i = 0; (int)(entry[3] << 8 | entry[2]) >> 2 > (int)i; i++)
			wlanPsNotifyFrame(entry + 4 + 4 * i, count--);
	} else if (mode == 3) {
		while (entry != NULL) {
			wlanPsNotifyFrame(entry + 4, (entry[3] << 8 | entry[2]) >> 2);
			entry = wlanTlvNextOfType(TLV_RATE_DROP_CONTROL, entry);
		}
	}
}

void wlanPsCheckPacket(struct WlanConnection *connection, const uint8_t *tlv)
{
	uint32_t mode;

	if (tlv == NULL)
		return;
	if (wlanPsQueryFrame(__aeabi_uread4(tlv + 4)) == 1)
		return;
	mode = __aeabi_uread4(tlv + 4);
	if (mode == 0) {
		wlanPsInspectFrame();
	} else if (mode == 1) {
		wlanRateTableRetryReset();
	} else if (mode == 2 || mode == 3) {
		wlanPsConfigurePacket(tlv);
	}
	wlanPsScheduleFrame(connection);
}
