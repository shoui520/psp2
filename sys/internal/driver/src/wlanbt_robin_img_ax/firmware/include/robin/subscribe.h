/*
 * Subscribed events (0xef78..0xf5a4): the thresholds the host sets with
 * HostCmd_CMD_802_11_SUBSCRIBE_EVENT (RSSI, SNR, failures, beacon loss, link
 * quality), kept per connection and checked against the link. TLV types and
 * event bits are mlan_fw.h's. See wlan/subscribe.c.
 */
#ifndef ROBIN_SUBSCRIBE_H
#define ROBIN_SUBSCRIBE_H

#include <stdint.h>

struct WlanConnection;
struct WlanTxStatus;

/* HostCmd_DS_802_11_SUBSCRIBE_EVENT actions */
#define HostCmd_ACT_GEN_GET		0
#define HostCmd_ACT_GEN_SET		1
#define HostCmd_ACT_BITWISE_SET		2
#define HostCmd_ACT_BITWISE_CLR		3

/* Event bits */
#define BITMASK_RSSI_LOW		0x0001
#define BITMASK_SNR_LOW			0x0002
#define BITMASK_FAILCOUNT		0x0004
#define BITMASK_BCNMISS			0x0008
#define BITMASK_RSSI_HIGH		0x0010
#define BITMASK_SNR_HIGH		0x0020
#define BITMASK_BCN_RSSI_LOW		0x0040
#define BITMASK_BCN_SNR_LOW		0x0080
#define BITMASK_BCN_RSSI_HIGH		0x0100
#define BITMASK_BCN_SNR_HIGH		0x0200
#define BITMASK_LINK_QUALITY		0x0400
#define BITMASK_PRE_BEACON_LOST		0x0800

/* TLV types */
#define TLV_TYPE_RSSI_LOW		0x0104
#define TLV_TYPE_SNR_LOW		0x0105
#define TLV_TYPE_FAILCOUNT		0x0106
#define TLV_TYPE_BCNMISS		0x0107
#define TLV_TYPE_RSSI_HIGH		0x0116
#define TLV_TYPE_SNR_HIGH		0x0117
#define TLV_TYPE_LINK_QUALITY		0x0124
#define TLV_TYPE_BCN_RSSI_LOW		0x0126
#define TLV_TYPE_BCN_SNR_LOW		0x0127
#define TLV_TYPE_BCN_RSSI_HIGH		0x0128
#define TLV_TYPE_BCN_SNR_HIGH		0x0129
#define TLV_TYPE_PRE_BEACON_LOST	0x0149

/* A threshold: the value and how often the event may repeat. */
struct WlanThreshold {
	uint8_t value;
	uint8_t frequency;
};

/* A threshold TLV, as the host sends it. */
struct __attribute__((packed)) WlanThresholdTlv {
	uint16_t type;
	uint16_t length;
	struct WlanThreshold threshold;
};

/* The link quality TLV: thresholds, each with the periods it must hold for. */
struct __attribute__((packed)) WlanLinkQualityTlv {
	uint16_t type;
	uint16_t length;
	uint16_t linkSnr;
	uint16_t linkSnrFrequency;
	uint16_t linkRate;
	uint16_t linkRateFrequency;
	uint32_t linkTxLatency;		/* microseconds */
	uint32_t linkTxLatencyFrequency;
};

/* The subscription of a connection, as the host's TLVs plus the repeat counters. */
struct __attribute__((packed)) WlanSubscribeEvents {
	uint16_t events;			/* BITMASK_* */
	struct WlanThresholdTlv rssiLow;
	struct WlanThresholdTlv snrLow;
	struct WlanThresholdTlv failCount;
	struct WlanThresholdTlv beaconMissed;
	struct WlanThresholdTlv rssiHigh;
	struct WlanThresholdTlv snrHigh;
	struct WlanThresholdTlv beaconRssiLow;
	struct WlanThresholdTlv beaconSnrLow;
	struct WlanThresholdTlv beaconRssiHigh;
	struct WlanThresholdTlv beaconSnrHigh;
	struct WlanLinkQualityTlv linkQuality;
	struct WlanThresholdTlv preBeaconLost;
	/* Reports left before an event waits for its frequency to come round again. */
	uint8_t rssiLowCount;
	uint8_t rssiHighCount;
	uint8_t snrLowCount;
	uint8_t snrHighCount;
	uint8_t beaconRssiLowCount;
	uint8_t beaconRssiHighCount;
	uint8_t beaconSnrLowCount;
	uint8_t beaconSnrHighCount;
	uint8_t failCountCount;
	uint16_t failures;			/* transmit failures in a row */
};

_Static_assert(sizeof(struct WlanSubscribeEvents) == 99, "subscribed events");
_Static_assert(__builtin_offsetof(struct WlanSubscribeEvents, linkQuality) == 62, "link quality");
_Static_assert(__builtin_offsetof(struct WlanSubscribeEvents, rssiLowCount) == 88, "counters");

#define WLAN_SUBSCRIBE_REPORT_SIZE	88	/* events and TLVs, as the host reads them */
#define WLAN_SUBSCRIBE_SLOTS		1	/* subscriptions per connection */

/* Link quality measured between the checks (0x04000d44). */
struct WlanLinkQualityState {
	uint8_t flags;			/* WLAN_LINK_PRE_BEACON_LOST */
	uint8_t idlePeriods;		/* periods in a row without a frame sent */
	uint16_t snrLowCount;		/* checks in a row with the SNR under its threshold */
	uint16_t rateLowCount;		/* frames in a row under the rate threshold */
	uint16_t latencyHighCount;	/* frames in a row over the latency threshold */
	uint16_t rateLow;		/* 1: the rate stayed under its threshold */
	uint16_t reserved;
	uint32_t frames;		/* frames sent in this period */
};

_Static_assert(sizeof(struct WlanLinkQualityState) == 16, "link quality state");

#define WLAN_LINK_PRE_BEACON_LOST	0x01	/* the beacons missed reached the pre-loss threshold */

extern struct WlanLinkQualityState wlanLinkQuality;
/* wlanPostConnectionEvent() event: subscribed events were met. */
#define WLAN_EVENT_SUBSCRIBED		0x08000000u

extern struct WlanSubscribeEvents wlanSubscribeEventsDefault;

void wlanSubscribeEventsInit(struct WlanSubscribeEvents *events);
void wlanSubscribeEventsSend(struct WlanConnection *connection, unsigned int events);	/* 0x2ceb4 */
int wlanSubscribeEventsPending(const struct WlanConnection *connection);
void wlanSubscribeEventsConfigure(struct WlanConnection *connection, const uint8_t *command,
				  unsigned int length, unsigned int action);
int wlanSubscribeEventsGet(const struct WlanConnection *connection, uint8_t *response);
int wlanSubscribeThresholdMet(struct WlanConnection *connection, int clear,
			      const struct WlanThreshold *threshold, uint8_t *count,
			      unsigned int bit, unsigned int slot);
void wlanSubscribeCheckData(struct WlanConnection *connection);
void wlanSubscribeCheckBeacon(struct WlanConnection *connection);
void wlanSubscribeCheckFailures(struct WlanConnection *connection, const struct WlanTxStatus *status);
void wlanLinkQualityCountFrame(void);
void wlanLinkQualityEndPeriod(void);
void wlanSubscribeCheckLinkSnr(struct WlanConnection *connection);
void wlanSubscribeCheckLinkRate(struct WlanConnection *connection, const struct WlanTxStatus *status);
void wlanSubscribeCheckLinkLatency(struct WlanConnection *connection, uint64_t start);
unsigned int wlanSubscribeEventsTake(struct WlanConnection *connection);
int wlanSubscribeBeaconMissEnabled(const struct WlanConnection *connection);
void wlanSubscribeResetFailures(struct WlanConnection *connection);
void wlanSubscribeEventsClearAll(void);
void wlanSubscribePreBeaconLostDone(void);
void wlanSubscribeCheckPreBeaconLost(struct WlanConnection *connection);
void wlanSubscribeEventsNotify(struct WlanConnection *connection);
void wlanSubscribeApplyThresholds(struct WlanConnection *connection);

#endif /* ROBIN_SUBSCRIBE_H */
