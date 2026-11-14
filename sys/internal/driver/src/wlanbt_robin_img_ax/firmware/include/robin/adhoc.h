/*
 * IBSS (ad-hoc) network parameters: the host's AD_HOC_START and AD_HOC_JOIN
 * commands (Marvell's HostCmd_DS_802_11_AD_HOC_START and AdHoc_BssDesc_t,
 * mlan_fw.h), as the firmware keeps them, with the extra information elements
 * the host appends.
 */
#ifndef ROBIN_ADHOC_H
#define ROBIN_ADHOC_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/wlan.h"

/* HostCmd_DS_802_11_AD_HOC_START, extended by the host's extra elements */
typedef struct __attribute__((packed)) WlanAdHocStart {
	uint8_t ssid[32];
	uint8_t bssMode;
	uint16_t beaconPeriod;
	uint8_t dtimPeriod;
	uint8_t ssParamSet[8];		/* IBSS parameter set element */
	uint8_t phyParamSet[7];		/* DS or FH parameter set element */
	uint16_t reserved1;
	uint16_t capability;
	uint8_t dataRate[14];
	uint8_t reserved2[15];
	const uint8_t *extraIes;
	uint16_t extraIesLength;
} WlanAdHocStart;

/* AdHoc_BssDesc_t, the network the host asks to join */
typedef struct __attribute__((packed)) WlanAdHocBssDesc {
	uint8_t bssid[6];
	uint8_t ssid[32];
	uint8_t bssMode;
	uint16_t beaconPeriod;
	uint8_t dtimPeriod;
	uint8_t timeStamp[8];
	uint8_t localTime[8];
	uint8_t phyParamSet[7];
	uint8_t ssParamSet[8];
	uint16_t capability;
	uint8_t dataRates[14];
	uint8_t reserved[7];
	const uint8_t *extraIes;
	uint16_t extraIesLength;
} WlanAdHocBssDesc;

_Static_assert(sizeof(WlanAdHocStart) == 90, "ad-hoc start parameters");
_Static_assert(__builtin_offsetof(WlanAdHocStart, capability) == 53, "ad-hoc start capability");
_Static_assert(__builtin_offsetof(WlanAdHocStart, extraIes) == 84, "ad-hoc start extra elements");
_Static_assert(__builtin_offsetof(WlanAdHocBssDesc, phyParamSet) == 58, "ad-hoc join phy parameters");
_Static_assert(__builtin_offsetof(WlanAdHocBssDesc, dataRates) == 75, "ad-hoc join rates");
_Static_assert(__builtin_offsetof(WlanAdHocBssDesc, extraIes) == 96, "ad-hoc join extra elements");

/*
 * A beacon or probe response kept for transmission by the MAC (built by the
 * IBSS and AP beacon code at 0x10dbe); its fields are byte aligned.
 */
typedef struct __attribute__((packed)) WlanBeaconDescriptor {
	uint8_t valid;
	uint8_t reserved1[3];
	WlanWirelessFrame *frame;
} WlanBeaconDescriptor;

/* WlanChannelFlags: the band, and the HT secondary channel offset */
enum {
	WLAN_CHANNEL_BAND_MASK = 0x03,		/* 0 2.4 GHz, 1 5 GHz */
	WLAN_CHANNEL_BAND_2GHZ = 0,
	WLAN_CHANNEL_BAND_5GHZ = 1,
	WLAN_CHANNEL_OFFSET_SHIFT = 4,
	WLAN_CHANNEL_OFFSET_MASK = 0x30,
	WLAN_CHANNEL_OFFSET_ABOVE = 0x10,
	WLAN_CHANNEL_OFFSET_BELOW = 0x30,
};

extern const uint8_t wlanBroadcastAddress[6];

/* the 802.11 header builder (0x2a414) */
void *wlanBuildMgmtHeader(WlanWirelessFrame *frame, const uint8_t *source,
			  const uint8_t *destination, const uint8_t *bssid,
			  unsigned int subtype, unsigned int powerManagement);

uint8_t *wlanBuildIbssBeacon(const WlanAdHocStart *start, const uint8_t *source,
			     const uint8_t *bssid, unsigned int subtype,
			     unsigned int powerManagement, OsaBuffer *buffer);
uint8_t *wlanPutPhyParamSet(uint8_t *buffer, const WlanAdHocStart *start);
void wlanAdHocJoinRates(WlanAdHocBssDesc *join, uint8_t *rates, uint8_t *count);
void wlanAdHocSsid(const uint8_t *ssid, uint8_t *bssid, uint8_t *destination,
		   uint8_t *length);
void wlanAdHocJoinToStart(WlanAdHocStart *start, const WlanAdHocBssDesc *join);
uint8_t *wlanSetBeaconPeriod(WlanBeaconDescriptor *const *beacon, uint16_t period);
uint8_t *wlanSetAtimWindow(WlanBeaconDescriptor *const *beacon, uint16_t window);
void wlanSetSecondaryChannel(uint8_t *channelFlags, unsigned int channel,
			     uint8_t *htInformation, int bothWays);
void wlanPutWmmParameterIe(uint8_t **cursor);
struct WlanIePointers;
void wlanSetErpProtection(struct WlanIePointers *ies, const void *bssState,
			  const void *pending, int protection);

#endif /* ROBIN_ADHOC_H */
