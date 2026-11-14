/*
 * Subscribed events (0xef78..0xf5a4): HostCmd_CMD_802_11_SUBSCRIBE_EVENT.
 * A connection's subscription starts from the defaults (beacon loss after
 * 60 beacons) and is changed by the host's events bits and TLVs.
 */
#include <stddef.h>
#include "robin/conn.h"
#include "robin/rom.h"
#include "robin/subscribe.h"
#include "robin/sys.h"
#include "robin/txring.h"

#define LE16(bytes)	((unsigned int)((bytes)[1] << 8 | (bytes)[0]))

#define WLAN_THRESHOLD(tlvType) \
	{ .type = (tlvType), .length = 2 }

/* Not converted yet; provisional names. */
unsigned int wlanTlvParseThreshold(const uint8_t *tlv, uint8_t *threshold, uint8_t *count); /* 0x2c60e */
unsigned int wlanTlvCopyValue(const uint8_t *tlv, void *value);			/* 0x2c662 */
int wlanBeaconRssi(struct WlanConnection *connection);				/* 0x1780c */
int wlanBeaconSnr(struct WlanConnection *connection);				/* 0x1787e */
int wlanDataRssi(struct WlanConnection *connection);				/* 0x17818 */
int wlanDataSnr(struct WlanConnection *connection);				/* 0x17824 */

#define WLAN_EVENT_SUBSCRIBED		0x08000000u	/* wlanPostConnectionEvent: events met */
#define WLAN_SIGNAL_UNKNOWN		(-98)

struct WlanLinkQualityState wlanLinkQuality __attribute__((section(".dtcm.rw")));

/* The subscription of the first connection that needs one. */
struct WlanSubscribeEvents wlanSubscribeEventsDefault __attribute__((section(".dtcm.rw"))) = {
	.events = BITMASK_BCNMISS,
	.rssiLow = WLAN_THRESHOLD(TLV_TYPE_RSSI_LOW),
	.snrLow = WLAN_THRESHOLD(TLV_TYPE_SNR_LOW),
	.failCount = WLAN_THRESHOLD(TLV_TYPE_FAILCOUNT),
	.beaconMissed = { .type = TLV_TYPE_BCNMISS, .length = 2, .value = 60 },
	.rssiHigh = WLAN_THRESHOLD(TLV_TYPE_RSSI_HIGH),
	.snrHigh = WLAN_THRESHOLD(TLV_TYPE_SNR_HIGH),
	.beaconRssiLow = WLAN_THRESHOLD(TLV_TYPE_BCN_RSSI_LOW),
	.beaconSnrLow = WLAN_THRESHOLD(TLV_TYPE_BCN_SNR_LOW),
	.beaconRssiHigh = WLAN_THRESHOLD(TLV_TYPE_BCN_RSSI_HIGH),
	.beaconSnrHigh = WLAN_THRESHOLD(TLV_TYPE_BCN_SNR_HIGH),
	.linkQuality = { .type = TLV_TYPE_LINK_QUALITY, .length = 16 },
	.preBeaconLost = { .type = TLV_TYPE_PRE_BEACON_LOST, .length = 2, .value = 50 },
};

void wlanSubscribeEventsInit(struct WlanSubscribeEvents *events)
{
	if (events == NULL)
		return;
	__aeabi_memclr(events, sizeof(*events));
	events->events = BITMASK_BCNMISS;
	events->rssiLow.type = TLV_TYPE_RSSI_LOW;
	events->rssiLow.length = 2;
	events->snrLow.type = TLV_TYPE_SNR_LOW;
	events->snrLow.length = 2;
	events->failCount.type = TLV_TYPE_FAILCOUNT;
	events->failCount.length = 2;
	events->beaconMissed.type = TLV_TYPE_BCNMISS;
	events->beaconMissed.length = 2;
	events->beaconMissed.value = 60;
	events->rssiHigh.type = TLV_TYPE_RSSI_HIGH;
	events->rssiHigh.length = 2;
	events->snrHigh.type = TLV_TYPE_SNR_HIGH;
	events->snrHigh.length = 2;
	events->beaconRssiLow.type = TLV_TYPE_BCN_RSSI_LOW;
	events->beaconRssiLow.length = 2;
	events->beaconSnrLow.type = TLV_TYPE_BCN_SNR_LOW;
	events->beaconSnrLow.length = 2;
	events->beaconRssiHigh.type = TLV_TYPE_BCN_RSSI_HIGH;
	events->beaconRssiHigh.length = 2;
	events->beaconSnrHigh.type = TLV_TYPE_BCN_SNR_HIGH;
	events->beaconSnrHigh.length = 2;
	events->linkQuality.type = TLV_TYPE_LINK_QUALITY;
	events->linkQuality.length = 16;
	events->preBeaconLost.type = TLV_TYPE_PRE_BEACON_LOST;
	events->preBeaconLost.length = 2;
	events->preBeaconLost.value = 50;
}

/* Events met and not yet reported to the host. */
int wlanSubscribeEventsPending(const struct WlanConnection *connection)
{
	return connection->subscribeTriggered != 0;
}

/*
 * HostCmd_CMD_802_11_SUBSCRIBE_EVENT: the command is the action, the events
 * bits and the TLVs. A TLV is taken when the action sets the events, or adds
 * them and names the TLV's event.
 */
void wlanSubscribeEventsConfigure(struct WlanConnection *connection, const uint8_t *command,
				  unsigned int length, unsigned int action)
{
	unsigned int requested = LE16(command + 2);
	struct WlanSubscribeEvents *events = connection->subscribeEvents[0];
	const uint8_t *tlv;
	unsigned int remaining;

#define WANTED(bit) \
	(action == HostCmd_ACT_GEN_SET || (action == HostCmd_ACT_BITWISE_SET && (requested & (bit)) != 0))

	if (events == NULL) {
		connection->subscribeEvents[0] = &wlanSubscribeEventsDefault;
		events = &wlanSubscribeEventsDefault;
		wlanSubscribeEventsInit(events);
	}
	switch (action) {
	case HostCmd_ACT_GEN_SET:
		events->events = requested;
		break;
	case HostCmd_ACT_BITWISE_SET:
		events->events |= requested;
		break;
	case HostCmd_ACT_BITWISE_CLR:
		events->events &= ~requested;
		break;
	}

	tlv = command + 4;
	remaining = (uint16_t)(length - 4);
	while (remaining >= 4) {
		unsigned int used = (uint16_t)(LE16(tlv + 2) + 4);

		switch (LE16(tlv)) {
		case TLV_TYPE_RSSI_LOW:
			if (WANTED(BITMASK_RSSI_LOW))
				used = wlanTlvParseThreshold(tlv, &events->rssiLow.value,
							     &events->rssiLowCount);
			break;
		case TLV_TYPE_SNR_LOW:
			if (WANTED(BITMASK_SNR_LOW))
				used = wlanTlvParseThreshold(tlv, &events->snrLow.value,
							     &events->snrLowCount);
			break;
		case TLV_TYPE_FAILCOUNT:
			if (WANTED(BITMASK_FAILCOUNT))
				used = wlanTlvParseThreshold(tlv, &events->failCount.value,
							     &events->failCountCount);
			break;
		case TLV_TYPE_BCNMISS:
			if (WANTED(BITMASK_BCNMISS)) {
				used = wlanTlvParseThreshold(tlv, &events->beaconMissed.value, NULL);
				connection->beaconMissThreshold = events->beaconMissed.value;
			}
			break;
		case TLV_TYPE_RSSI_HIGH:
			if (WANTED(BITMASK_RSSI_HIGH))
				used = wlanTlvParseThreshold(tlv, &events->rssiHigh.value,
							     &events->rssiHighCount);
			break;
		case TLV_TYPE_SNR_HIGH:
			if (WANTED(BITMASK_SNR_HIGH))
				used = wlanTlvParseThreshold(tlv, &events->snrHigh.value,
							     &events->snrHighCount);
			break;
		case TLV_TYPE_LINK_QUALITY:
			if (WANTED(BITMASK_LINK_QUALITY))
				used = wlanTlvCopyValue(tlv, &events->linkQuality.linkSnr);
			break;
		case TLV_TYPE_BCN_RSSI_LOW:
			if (WANTED(BITMASK_BCN_RSSI_LOW))
				used = wlanTlvParseThreshold(tlv, &events->beaconRssiLow.value,
							     &events->beaconRssiLowCount);
			break;
		case TLV_TYPE_BCN_SNR_LOW:
			if (WANTED(BITMASK_BCN_SNR_LOW))
				used = wlanTlvParseThreshold(tlv, &events->beaconSnrLow.value,
							     &events->beaconSnrLowCount);
			break;
		case TLV_TYPE_BCN_RSSI_HIGH:
			if (WANTED(BITMASK_BCN_RSSI_HIGH))
				used = wlanTlvParseThreshold(tlv, &events->beaconRssiHigh.value,
							     &events->beaconRssiHighCount);
			break;
		case TLV_TYPE_BCN_SNR_HIGH:
			if (WANTED(BITMASK_BCN_SNR_HIGH))
				used = wlanTlvParseThreshold(tlv, &events->beaconSnrHigh.value,
							     &events->beaconSnrHighCount);
			break;
		case TLV_TYPE_PRE_BEACON_LOST:
			if (WANTED(BITMASK_PRE_BEACON_LOST)) {
				used = wlanTlvParseThreshold(tlv, &events->preBeaconLost.value, NULL);
				connection->preBeaconLostThreshold = events->preBeaconLost.value;
			}
			break;
		default:
			used = remaining;
			break;
		}
		if (remaining < used)
			remaining = 0;
		else
			remaining = (uint16_t)(remaining - used);
		tlv += used;
	}
#undef WANTED
}

/* HostCmd_ACT_GEN_GET: the events bits and TLVs behind the action; the length, or 0 without a subscription. */
int wlanSubscribeEventsGet(const struct WlanConnection *connection, uint8_t *response)
{
	if (connection->subscribeEvents[0] == NULL)
		return 0;
	__aeabi_memcpy(response + 2, connection->subscribeEvents[0], WLAN_SUBSCRIBE_REPORT_SIZE);
	return 86;
}

/*
 * One threshold check. While the link is on the good side (clear), the
 * event's counter is reloaded with its frequency; on the bad side it counts
 * down and the event is met when it reaches 0. An event without a frequency
 * is reported once and unsubscribed.
 */
int wlanSubscribeThresholdMet(struct WlanConnection *connection, int clear,
			      const struct WlanThresholdTlv *threshold, uint8_t *count,
			      unsigned int bit, unsigned int slot)
{
	struct WlanSubscribeEvents *events;

	if (clear) {
		*count = threshold->frequency != 0 ? threshold->frequency : 1;
		return 0;
	}
	if (*count == 0 || --*count != 0)
		return 0;
	if (threshold->frequency != 0) {
		*count = threshold->frequency;
	} else {
		events = connection->subscribeEvents[slot];
		if (events != NULL)
			events->events &= ~bit;
	}
	return 1;
}

static inline __attribute__((always_inline)) void wlanSubscribeReport(struct WlanConnection *connection, unsigned int met)
{
	connection->subscribeTriggered |= met;
	wlanPostConnectionEvent(connection, WLAN_EVENT_SUBSCRIBED);
}

/* The RSSI and SNR of the data frames against their thresholds. */
void wlanSubscribeCheckData(struct WlanConnection *connection)
{
	int snr = (int16_t)wlanDataSnr(connection);
	int rssi = (int16_t)wlanDataRssi(connection);
	uint8_t slot;

	for (slot = 0; slot < WLAN_SUBSCRIBE_SLOTS; slot++) {
		struct WlanSubscribeEvents *events = connection->subscribeEvents[slot];
		unsigned int met = 0;

		if (events == NULL)
			continue;
		if ((events->events & BITMASK_SNR_LOW) &&
		    wlanSubscribeThresholdMet(connection, events->snrLow.value <= snr, &events->snrLow,
					      &events->snrLowCount, BITMASK_SNR_LOW, slot))
			met = BITMASK_SNR_LOW;
		if ((events->events & BITMASK_SNR_HIGH) &&
		    wlanSubscribeThresholdMet(connection, events->snrHigh.value >= snr, &events->snrHigh,
					      &events->snrHighCount, BITMASK_SNR_HIGH, slot))
			met |= BITMASK_SNR_HIGH;
		if ((events->events & BITMASK_RSSI_LOW) &&
		    wlanSubscribeThresholdMet(connection, -events->rssiLow.value <= rssi, &events->rssiLow,
					      &events->rssiLowCount, BITMASK_RSSI_LOW, slot))
			met |= BITMASK_RSSI_LOW;
		if ((events->events & BITMASK_RSSI_HIGH) &&
		    wlanSubscribeThresholdMet(connection, -events->rssiHigh.value >= rssi,
					      &events->rssiHigh, &events->rssiHighCount,
					      BITMASK_RSSI_HIGH, slot))
			met |= BITMASK_RSSI_HIGH;
		if (met != 0)
			wlanSubscribeReport(connection, met);
	}
}

/* The RSSI and SNR of the beacons against their thresholds; each is read once, when needed. */
void wlanSubscribeCheckBeacon(struct WlanConnection *connection)
{
	int snr = WLAN_SIGNAL_UNKNOWN;
	int rssi = WLAN_SIGNAL_UNKNOWN;
	uint8_t slot;

	for (slot = 0; slot < WLAN_SUBSCRIBE_SLOTS; slot++) {
		struct WlanSubscribeEvents *events = connection->subscribeEvents[slot];
		unsigned int wanted, met = 0;

		if (events == NULL)
			continue;
		wanted = events->events & (BITMASK_BCN_RSSI_LOW | BITMASK_BCN_SNR_LOW |
					   BITMASK_BCN_RSSI_HIGH | BITMASK_BCN_SNR_HIGH);
		if (wanted == 0)
			continue;
		if ((wanted & (BITMASK_BCN_SNR_LOW | BITMASK_BCN_SNR_HIGH)) && snr == WLAN_SIGNAL_UNKNOWN)
			snr = (int16_t)wlanBeaconSnr(connection);
		if ((wanted & (BITMASK_BCN_RSSI_LOW | BITMASK_BCN_RSSI_HIGH)) && rssi == WLAN_SIGNAL_UNKNOWN)
			rssi = (int16_t)wlanBeaconRssi(connection);
		if ((wanted & BITMASK_BCN_SNR_LOW) &&
		    wlanSubscribeThresholdMet(connection, events->beaconSnrLow.value <= snr,
					      &events->beaconSnrLow, &events->beaconSnrLowCount,
					      BITMASK_BCN_SNR_LOW, slot))
			met = BITMASK_BCN_SNR_LOW;
		if ((wanted & BITMASK_BCN_SNR_HIGH) &&
		    wlanSubscribeThresholdMet(connection, events->beaconSnrHigh.value >= snr,
					      &events->beaconSnrHigh, &events->beaconSnrHighCount,
					      BITMASK_BCN_SNR_HIGH, slot))
			met |= BITMASK_BCN_SNR_HIGH;
		if ((wanted & BITMASK_BCN_RSSI_LOW) &&
		    wlanSubscribeThresholdMet(connection, -events->beaconRssiLow.value <= rssi,
					      &events->beaconRssiLow, &events->beaconRssiLowCount,
					      BITMASK_BCN_RSSI_LOW, slot))
			met |= BITMASK_BCN_RSSI_LOW;
		if ((wanted & BITMASK_BCN_RSSI_HIGH) &&
		    wlanSubscribeThresholdMet(connection, -events->beaconRssiHigh.value >= rssi,
					      &events->beaconRssiHigh, &events->beaconRssiHighCount,
					      BITMASK_BCN_RSSI_HIGH, slot))
			met |= BITMASK_BCN_RSSI_HIGH;
		if (met != 0)
			wlanSubscribeReport(connection, met);
		break;
	}
}

/* Transmit failures in a row against the failure count threshold. */
void wlanSubscribeCheckFailures(struct WlanConnection *connection, const struct WlanTxStatus *status)
{
	uint8_t slot;

	for (slot = 0; slot < WLAN_SUBSCRIBE_SLOTS; slot++) {
		struct WlanSubscribeEvents *events = connection->subscribeEvents[slot];
		unsigned int failures;

		if (events == NULL || !(events->events & BITMASK_FAILCOUNT))
			continue;
		if (status->result == 0) {
			events->failures = 0;
			events->failCountCount = events->failCount.frequency != 0 ?
						 events->failCount.frequency : 1;
			continue;
		}
		failures = (uint16_t)(events->failures + 1);
		events->failures = failures;
		if (events->failCountCount == 0 || failures < events->failCount.value)
			continue;
		if (--events->failCountCount != 0)
			continue;
		events->failures = 0;
		if (events->failCount.frequency != 0)
			events->failCountCount = events->failCount.frequency;
		else
			events->events &= ~BITMASK_FAILCOUNT;
		wlanSubscribeReport(connection, BITMASK_FAILCOUNT);
	}
}

void wlanLinkQualityCountFrame(void)
{
	wlanLinkQuality.frames++;
}

void wlanLinkQualityEndPeriod(void)
{
	if (wlanLinkQuality.frames != 0)
		wlanLinkQuality.idlePeriods = 0;
	else
		wlanLinkQuality.idlePeriods++;
	wlanLinkQuality.frames = 0;
}

/*
 * The link SNR against its threshold: met when it stayed low for the
 * frequency's checks and the link was idle as long, or its rate was low.
 */
void wlanSubscribeCheckLinkSnr(struct WlanConnection *connection)
{
	struct WlanSubscribeEvents *events = connection->subscribeEvents[0];
	UINT state;

	if (events == NULL || !(events->events & BITMASK_LINK_QUALITY))
		return;
	if (wlanDataSnr(connection) > (int)events->linkQuality.linkSnr) {
		wlanLinkQuality.snrLowCount = 0;
		wlanLinkQuality.rateLow = 0;
		return;
	}
	wlanLinkQuality.snrLowCount++;
	if (events->linkQuality.linkSnrFrequency > wlanLinkQuality.snrLowCount)
		return;
	if (events->linkQuality.linkSnrFrequency > wlanLinkQuality.idlePeriods &&
	    wlanLinkQuality.rateLow != 1)
		return;
	wlanLinkQuality.snrLowCount = 0;
	state = sysIrqDisable();
	wlanLinkQuality.idlePeriods = 0;
	sysIrqRestore(state);
	wlanLinkQuality.rateLow = 0;
	wlanSubscribeReport(connection, BITMASK_LINK_QUALITY);
}

/* The rate a frame finished at against the link rate threshold. */
void wlanSubscribeCheckLinkRate(struct WlanConnection *connection, const struct WlanTxStatus *status)
{
	struct WlanSubscribeEvents *events = connection->subscribeEvents[0];

	if (events == NULL || !(events->events & BITMASK_LINK_QUALITY))
		return;
	if (romRateBitrate(status->finalRate) > events->linkQuality.linkRate) {
		wlanLinkQuality.rateLow = 0;
	} else {
		wlanLinkQuality.rateLowCount++;
		if (events->linkQuality.linkRateFrequency > wlanLinkQuality.rateLowCount)
			return;
		wlanLinkQuality.rateLow = 1;
	}
	wlanLinkQuality.rateLowCount = 0;
}

/* How long a frame took to go out (from start) against the latency threshold. */
void wlanSubscribeCheckLinkLatency(struct WlanConnection *connection, uint64_t start)
{
	struct WlanSubscribeEvents *events = connection->subscribeEvents[0];
	uint64_t now, latency;

	if (events == NULL || !(events->events & BITMASK_LINK_QUALITY))
		return;
	now = romGetTimerTimestamp();
	if (start >= now)
		latency = ~(uint64_t)0 - start + now;
	else
		latency = now - start;
	if (latency < events->linkQuality.linkTxLatency) {
		wlanLinkQuality.latencyHighCount = 0;
		return;
	}
	wlanLinkQuality.latencyHighCount++;
	if (events->linkQuality.linkTxLatencyFrequency <= wlanLinkQuality.latencyHighCount) {
		wlanLinkQuality.latencyHighCount = 0;
		wlanSubscribeReport(connection, BITMASK_LINK_QUALITY);
	}
}

/* The events met since the last call, for the host's event. */
unsigned int wlanSubscribeEventsTake(struct WlanConnection *connection)
{
	UINT state = sysIrqDisable();
	unsigned int met = connection->subscribeTriggered;

	connection->subscribeTriggered = 0;
	sysIrqRestore(state);
	return met;
}

int wlanSubscribeBeaconMissEnabled(const struct WlanConnection *connection)
{
	const struct WlanSubscribeEvents *events = connection->subscribeEvents[0];

	if (events == NULL)
		return 0;
	return (events->events & BITMASK_BCNMISS) != 0;
}

void wlanSubscribeResetFailures(struct WlanConnection *connection)
{
	struct WlanSubscribeEvents *events = connection->subscribeEvents[0];

	if (events != NULL)
		events->failures = 0;
}

static inline __attribute__((always_inline)) void wlanSubscribeEventsDrop(struct WlanConnection *connection)
{
	uint8_t slot;

	for (slot = 0; slot < WLAN_SUBSCRIBE_SLOTS; slot++)
		connection->subscribeEvents[slot] = NULL;
	connection->subscribeTriggered = 0;
}

/* Forget the subscriptions of the first type 0, access point and IBSS connections. */
void wlanSubscribeEventsClearAll(void)
{
	struct WlanConnection *connection;

	connection = wlanConnectionFirst(0);
	if (connection != NULL)
		wlanSubscribeEventsDrop(connection);
	connection = wlanConnectionFirst(WLAN_IF_AP);
	if (connection != NULL)
		wlanSubscribeEventsDrop(connection);
	connection = wlanConnectionFirst(WLAN_IF_IBSS);
	if (connection != NULL)
		wlanSubscribeEventsDrop(connection);
}

/* The pre-beacon-loss event was reported: unsubscribe it until the host asks again. */
void wlanSubscribePreBeaconLostDone(void)
{
	struct WlanSubscribeEvents *events = wlanConnectionFirst(0)->subscribeEvents[0];

	if (events == NULL)
		return;
	events->events &= ~BITMASK_PRE_BEACON_LOST;
	wlanLinkQuality.flags &= ~WLAN_LINK_PRE_BEACON_LOST;
}

void wlanSubscribeCheckPreBeaconLost(struct WlanConnection *connection)
{
	const struct WlanSubscribeEvents *events = connection->subscribeEvents[0];

	if (events != NULL && (events->events & BITMASK_PRE_BEACON_LOST) &&
	    (wlanLinkQuality.flags & WLAN_LINK_PRE_BEACON_LOST))
		wlanSubscribeReport(connection, BITMASK_PRE_BEACON_LOST);
}

/* Tell the host about the events met (the original also rewrites the met bits unchanged). */
void wlanSubscribeEventsNotify(struct WlanConnection *connection)
{
	wlanPostConnectionEvent(connection, WLAN_EVENT_SUBSCRIBED);
}

/* Take the beacon loss thresholds of the subscription, the defaults without one. */
void wlanSubscribeApplyThresholds(struct WlanConnection *connection)
{
	struct WlanSubscribeEvents *events = connection->subscribeEvents[0];

	if (events == NULL) {
		events = &wlanSubscribeEventsDefault;
		connection->subscribeEvents[0] = &wlanSubscribeEventsDefault;
	}
	connection->beaconMissThreshold = events->beaconMissed.value;
	connection->preBeaconLostThreshold = events->preBeaconLost.value;
}
