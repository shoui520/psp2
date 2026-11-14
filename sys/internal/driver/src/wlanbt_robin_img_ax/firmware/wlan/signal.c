/*
 * The signal of a connection (0x1760c..0x17bd8): the noise floor, RSSI and
 * SNR of its beacons and data frames, kept as the last values and as running
 * averages for the subscribed events and the host's queries, and the link
 * check, which counts the beacons missed while nothing is heard from the peer,
 * probes it with null frames and reports the loss. wlanSignalMeasure() and
 * wlanSignalFilterGet() sit apart in the image (0x53c74..0x53cd0). The
 * filename is inferred; no source filename survives.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/coex.h"
#include "robin/conn.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/psctl.h"
#include "robin/rom.h"
#include "robin/signal.h"
#include "robin/subscribe.h"
#include "robin/sys.h"
#include "robin/wlanrx.h"

/* WlanRxStatus.legacyRate below this: a DSSS/CCK rate (1, 2, 5.5, 11 Mb/s, 22 Mb/s PBCC). */
#define WLAN_SIGNAL_CCK_RATES		5

#define WLAN_SIGNAL_SHIFT		3	/* the averages over 8 samples */
#define WLAN_SIGNAL_SHIFT_MIN		2	/* wlanSignalAverageShift() */
#define WLAN_SIGNAL_SHIFT_MAX		10

#define WLAN_SIGNAL_FILTER_FRAMES	0	/* the noise floor of the beacons and data frames */
#define WLAN_SIGNAL_FILTER_JOIN		1	/* the noise floor of the join's frames */
#define WLAN_SIGNAL_FILTER_SPARE	2

#define WLAN_LINK_NULL_PROBES		6	/* null frames sent before the loss is reported */
#define WLAN_LINK_NULL_KIND		32	/* macTxSendNullFrame(): a link probe */
#define WLAN_STAT_BEACONS_MISSED	10	/* sysLogStatistic() */
#define WLAN_REPORT_BEACON_LOSS		7	/* wlanReportEvent() */

/* Not converted yet; provisional names. */
int romSignalFilterNoise(struct WlanConnection *connection, const struct WlanRxStatus *status); /* 0xc000a431 */
uint32_t wlanLinkCheckPeriod(void);					/* 0x3dd46 */
uint64_t wlanSignalTimeNow(void);					/* 0x2a4fc */
int wlanStaSmLinked(const struct WlanConnection *connection);		/* 0x35faa */
unsigned int wlanPsDiscountMissed(uint8_t *missed);			/* 0x3dfc4 */
int wlanLinkCheckPost(struct WlanConnection *connection);		/* 0x347be */
unsigned int cbProcNullPriority(void);					/* 0x1e9ba */
extern uint8_t wlanSignalSnapshot[8];					/* 0x0400d288 */

/* Link timers that expired after the check had moved to a new one. */
uint32_t wlanLinkStaleTimers __attribute__((section(".itcm.rw")));	/* 0x54b88 */

static const uint8_t wlanSignalSnapshotEmpty[8];

/* Two samples within 3 dB of each other. */
#define WLAN_SIGNAL_CLOSE(a, b)		((unsigned int)((a) - (b) + 3) <= 6)

int wlanSignalFilterReady(const struct WlanConnection *connection, int filter)
{
	return connection->signalFilters[filter].count >= 3;
}

void wlanSignalFilterAdd(struct WlanConnection *connection, int filter, int sample)
{
	struct WlanSignalFilter *f = &connection->signalFilters[filter];
	int oldest, middle, mean;

	if (f->count < 3)
		f->count++;
	f->samples[0] = f->samples[1];
	middle = f->samples[2];
	f->samples[1] = middle;
	f->samples[2] = sample;
	oldest = f->samples[0];
	mean = (oldest + middle + sample) / 3;
	if (WLAN_SIGNAL_CLOSE(oldest, middle) && WLAN_SIGNAL_CLOSE(middle, sample) &&
	    WLAN_SIGNAL_CLOSE(sample, oldest)) {
		f->value = mean;
		return;
	}
	if (middle < oldest)
		f->value = middle;
	else
		f->value = oldest;
	if (f->samples[2] < f->value)
		f->value = f->samples[2];
}

/* The shift of an average over about the given number of frames. */
uint16_t wlanSignalAverageShift(unsigned int frames)
{
	uint16_t shift;

	for (shift = WLAN_SIGNAL_SHIFT_MIN; shift <= WLAN_SIGNAL_SHIFT_MAX; shift++)
		if (frames < (uint16_t)(3 << (shift - 1)))
			return shift;
	return WLAN_SIGNAL_SHIFT_MAX;
}

void wlanSignalAverageAdd(struct WlanSignalAverage *average, int sample, unsigned int shift)
{
	if (average->count < (1 << shift)) {
		average->sum += sample;
		average->count++;
	} else {
		average->sum = average->sum - (average->sum >> shift) + sample;
	}
}

int wlanSignalAverageGet(const struct WlanSignalAverage *average, unsigned int shift)
{
	if (average->count == 0)
		return WLAN_SIGNAL_UNKNOWN;
	if (average->count < (1 << shift))
		return average->sum / average->count;
	return average->sum >> shift;
}

/* A beacon: its signal into the beacon averages. */
int wlanSignalBeacon(struct WlanConnection *connection, const struct WlanRxStatus *status, int *snr,
		     int *noise, int *rssi)
{
	int measured = wlanSignalMeasure(connection, status, snr, noise, rssi);
	int cck, sample;

	if (measured) {
		cck = status->legacyRate < WLAN_SIGNAL_CCK_RATES;
		sample = *rssi;
		wlanSignalAverageAdd(&connection->noiseFloorAverage, *noise, WLAN_SIGNAL_SHIFT);
		if (cck)
			wlanSignalAverageAdd(&connection->beaconRssiCck, sample, WLAN_SIGNAL_SHIFT);
		else
			wlanSignalAverageAdd(&connection->beaconRssiOfdm, sample, WLAN_SIGNAL_SHIFT);
		wlanSignalAverageAdd(&connection->beaconRssiAverage, sample,
				     connection->beaconRssiShift);
	}
	return measured;
}

/* A data frame: the peer is heard; its signal against the subscribed thresholds. */
void wlanSignalData(struct WlanConnection *connection, const struct WlanRxStatus *status)
{
	int snr, noise, rssi;
	int measured = wlanSignalMeasure(connection, status, &snr, &noise, &rssi);

	connection->link.frameSeen = 1;
	connection->link.nullFrames = 0;
	if (!measured)
		return;
	connection->dataRssi = rssi;
	connection->dataNoiseFloor = noise;
	connection->dataSnr = snr;
	wlanSignalDataAverage(connection);
	if (wlanHostSleepArmed() &&
	    !wlanHostSleepWakeMatch(WLAN_WAKE_EVENT, __aeabi_uread4(wlanHostSleepActive->conditions)))
		return;
	wlanSubscribeCheckData(connection);
	wlanSubscribeCheckLinkSnr(connection);
}

/* A frame of the join (authentication, association response). */
void wlanSignalJoinFrame(struct WlanConnection *connection, const struct WlanRxStatus *status)
{
	int rssi, noise;

	if (connection == NULL)
		return;
	rssi = (int8_t)status->rssi;
	noise = (int8_t)status->noise;
	wlanSignalFilterAdd(connection, WLAN_SIGNAL_FILTER_JOIN, noise);
	wlanSignalAverageAdd(&connection->joinNoiseFloor,
			     wlanSignalFilterGet(connection, WLAN_SIGNAL_FILTER_JOIN), WLAN_SIGNAL_SHIFT);
	if (status->legacyRate < WLAN_SIGNAL_CCK_RATES)
		wlanSignalAverageAdd(&connection->joinRssiCck, rssi, WLAN_SIGNAL_SHIFT);
	else
		wlanSignalAverageAdd(&connection->joinRssiOfdm, rssi, WLAN_SIGNAL_SHIFT);
}

int wlanNoiseFloor(struct WlanConnection *connection)
{
	return wlanSignalAverageGet(&connection->noiseFloorAverage, WLAN_SIGNAL_SHIFT);
}

int wlanBeaconRssi(struct WlanConnection *connection)
{
	return wlanSignalAverageGet(&connection->beaconRssiAverage, connection->beaconRssiShift);
}

int wlanDataRssi(struct WlanConnection *connection)
{
	return wlanSignalAverageGet(&connection->dataRssiAverage, connection->dataRssiShift);
}

int wlanDataSnr(struct WlanConnection *connection)
{
	int rssi = wlanDataRssi(connection);

	return rssi - wlanNoiseFloor(connection);
}

int wlanBeaconSnrCck(struct WlanConnection *connection)
{
	int rssi = wlanSignalAverageGet(&connection->beaconRssiCck, WLAN_SIGNAL_SHIFT);

	return rssi - wlanNoiseFloor(connection);
}

int wlanBeaconSnrOfdm(struct WlanConnection *connection)
{
	int rssi = wlanSignalAverageGet(&connection->beaconRssiOfdm, WLAN_SIGNAL_SHIFT);

	return rssi - wlanNoiseFloor(connection);
}

int wlanBeaconOfdmSeen(const struct WlanConnection *connection)
{
	return connection->beaconRssiOfdm.count >= 1;
}

int wlanBeaconSnr(struct WlanConnection *connection)
{
	int rssi = wlanBeaconRssi(connection);

	return rssi - wlanNoiseFloor(connection);
}

void wlanSignalJoinReset(struct WlanConnection *connection)
{
	__aeabi_memclr4(&connection->signalFilters[WLAN_SIGNAL_FILTER_JOIN],
			sizeof(struct WlanSignalFilter));
	connection->joinNoiseFloor = (struct WlanSignalAverage){ 0, 0 };
	connection->joinRssiCck = (struct WlanSignalAverage){ 0, 0 };
	connection->joinRssiOfdm = (struct WlanSignalAverage){ 0, 0 };
}

void wlanSignalBeaconReset(struct WlanConnection *connection)
{
	connection->snr = 0;
	connection->noise = WLAN_SIGNAL_UNKNOWN;
	connection->beaconRssi = WLAN_SIGNAL_UNKNOWN;
	connection->beaconRssiAverage = (struct WlanSignalAverage){ 0, 0 };
}

void wlanSignalDataReset(struct WlanConnection *connection)
{
	connection->dataSnr = 0;
	connection->dataNoiseFloor = WLAN_SIGNAL_UNKNOWN;
	connection->dataRssi = WLAN_SIGNAL_UNKNOWN;
	__aeabi_memcpy(wlanSignalSnapshot, wlanSignalSnapshotEmpty, sizeof(wlanSignalSnapshot));
	connection->dataRssiAverage = (struct WlanSignalAverage){ 0, 0 };
	wlanSignalDataAverage(connection);
}

void wlanSignalFilterReset(struct WlanConnection *connection)
{
	UINT posture = sysIrqDisable();

	__aeabi_memclr4(&connection->signalFilters[WLAN_SIGNAL_FILTER_FRAMES],
			sizeof(struct WlanSignalFilter));
	wlanSignalBeaconClear(connection);
	sysIrqRestore(posture);
}

void wlanSignalReset(struct WlanConnection *connection)
{
	UINT posture = sysIrqDisable();

	wlanSignalFilterReset(connection);
	wlanSignalDataReset(connection);
	wlanSignalBeaconReset(connection);
	sysIrqRestore(posture);
}

void wlanSignalInit(struct WlanConnection *connection)
{
	connection->dataRssiShift = WLAN_SIGNAL_SHIFT;
	connection->beaconRssiShift = WLAN_SIGNAL_SHIFT;
	__aeabi_memclr4(&connection->signalFilters[WLAN_SIGNAL_FILTER_SPARE],
			sizeof(struct WlanSignalFilter));
	connection->spareAverage = (struct WlanSignalAverage){ 0, 0 };
	wlanSignalJoinReset(connection);
	connection->beaconRssiAverage = (struct WlanSignalAverage){ 0, 0 };
	connection->dataRssiAverage = (struct WlanSignalAverage){ 0, 0 };
	wlanSignalBeaconClear(connection);
}

int wlanBeaconCckSeen(const struct WlanConnection *connection)
{
	return connection->beaconRssiCck.count >= 1;
}

/* The join is done: its measurements become the connection's. */
void wlanSignalJoinDone(struct WlanConnection *connection)
{
	int rssi;

	connection->noiseFloorAverage = connection->joinNoiseFloor;
	connection->beaconRssiCck = connection->joinRssiCck;
	connection->beaconRssiOfdm = connection->joinRssiOfdm;
	if (connection->beaconRssiCck.count == 0)
		rssi = wlanSignalAverageGet(&connection->joinRssiOfdm, WLAN_SIGNAL_SHIFT);
	else
		rssi = wlanSignalAverageGet(&connection->joinRssiCck, WLAN_SIGNAL_SHIFT);
	connection->dataRssi = rssi;
	connection->dataRssiAverage = (struct WlanSignalAverage){ 0, 0 };
	wlanSignalDataAverage(connection);
	connection->dataNoiseFloor = wlanSignalAverageGet(&connection->joinNoiseFloor, WLAN_SIGNAL_SHIFT);
	connection->dataSnr = connection->dataRssi - connection->dataNoiseFloor;
	wlanSignalJoinReset(connection);
}

/* A frame sent to the peer; acknowledged, it shows the peer is there. */
void wlanLinkTxDone(struct WlanConnection *connection, int acknowledged)
{
	if (acknowledged && connection->interfaceType != 0) {
		connection->link.frameSeen = 1;
		connection->link.nullFrames = 0;
	}
}

void wlanLinkFrameReceived(struct WlanConnection *connection, int16_t snr, int16_t rssi)
{
	(void)snr;
	(void)rssi;
	connection->link.frameSeen = 1;
	connection->link.nullFrames = 0;
}

void wlanLinkTimerExpired(uint32_t timer, uint32_t argument)
{
	struct WlanConnection *connection = (struct WlanConnection *)argument;

	if (connection->link.timer == timer) {
		connection->link.timer = 0;
		wlanLinkCheckPost(connection);
	} else {
		wlanLinkStaleTimers++;
	}
}

/* The check once a period: the beacons missed since the peer was last heard. */
void wlanLinkCheck(struct WlanConnection *connection)
{
	uint8_t missed = 0;
	uint32_t period = wlanLinkCheckPeriod();
	struct WlanLinkMonitor *link = &connection->link;
	uint64_t now;

	if (link->frameSeen) {
		link->frameSeen = 0;
		link->silent = 0;
		goto rearm;
	}
	if (!link->silent) {
		link->silent = 1;
		link->silentSince = wlanSignalTimeNow();
	} else if (wlanStaSmLinked(connection)) {
		now = wlanSignalTimeNow();
		missed = (uint32_t)(now - link->silentSince) / period;
		if (missed > 1) {
			wlanPsDiscountMissed(&missed);
			sysLogStatistic(WLAN_STAT_BEACONS_MISSED, 0, missed);
			if (connection->preBeaconLostThreshold != 0 &&
			    connection->preBeaconLostThreshold <= missed) {
				wlanLinkQuality.flags |= WLAN_LINK_PRE_BEACON_LOST;
				wlanSubscribeCheckPreBeaconLost(connection);
			}
			if (connection->beaconMissThreshold <= missed) {
				if (wlanCoexBtNeedsSharing() && link->nullFrames < WLAN_LINK_NULL_PROBES) {
					if (!wlanPsInIeee()) {
						unsigned int priority = cbProcNullPriority();

						macTxSendNullFrame(connection, macTxGetPowerSave(), priority,
								   0, 0, WLAN_LINK_NULL_KIND);
						link->nullFrames++;
					}
				} else {
					link->nullFrames = 0;
					wlanReportEvent(connection, NULL, WLAN_REPORT_BEACON_LOSS);
				}
			}
		}
	} else {
		link->silentSince += period;
	}
	link->silent = 1;
rearm:
	osaCreateTimer(wlanLinkTimerExpired, (uint32_t)connection, period, &link->timer, 0);
}

/* The link check restarts with the connection, except on interface type 1. */
void wlanLinkMonitorStart(struct WlanConnection *connection)
{
	if (connection->interfaceType == 1)
		return;
	osaCancelTimer(connection->link.timer);
	__aeabi_memclr4(&connection->link, sizeof(connection->link));
	wlanLinkCheck(connection);
}

void wlanSignalDataAverage(struct WlanConnection *connection)
{
	wlanSignalAverageAdd(&connection->dataRssiAverage, connection->dataRssi,
			     connection->dataRssiShift);
}

void wlanSignalBeaconClear(struct WlanConnection *connection)
{
	connection->noiseFloorAverage = (struct WlanSignalAverage){ 0, 0 };
	connection->beaconRssiCck = (struct WlanSignalAverage){ 0, 0 };
	connection->beaconRssiOfdm = (struct WlanSignalAverage){ 0, 0 };
	connection->beaconRssiAverage = (struct WlanSignalAverage){ 0, 0 };
}

/* 0x53c74: a frame's noise floor (filtered where the ROM says so), RSSI and SNR. */
int wlanSignalMeasure(struct WlanConnection *connection, const struct WlanRxStatus *status, int *snr,
		      int *noise, int *rssi)
{
	int floor = (int8_t)status->noise;
	int filtered = 0;
	int level;

	if (romSignalFilterNoise(connection, status)) {
		wlanSignalFilterAdd(connection, WLAN_SIGNAL_FILTER_FRAMES, floor);
		floor = wlanSignalFilterGet(connection, WLAN_SIGNAL_FILTER_FRAMES);
		filtered = 1;
	}
	*noise = floor;
	level = (int8_t)status->rssi;
	*rssi = level;
	*snr = level - *noise;
	return filtered;
}

/* 0x53cb8 */
int wlanSignalFilterGet(const struct WlanConnection *connection, int filter)
{
	const struct WlanSignalFilter *f = &connection->signalFilters[filter];

	if (f->count == 0)
		return WLAN_SIGNAL_UNKNOWN;
	return f->value;
}
