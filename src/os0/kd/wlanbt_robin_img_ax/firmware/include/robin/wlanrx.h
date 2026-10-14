/* Receive-path structures and the frame filters that need nothing else. */
#ifndef ROBIN_WLANRX_H
#define ROBIN_WLANRX_H

#include <stdint.h>
#include "robin/wlan.h"

/*
 * Hardware receive status filled in by the MAC for each frame (68 bytes). Only
 * the fields the receive path reads are named.
 */
struct WlanRxStatus {
	uint8_t reserved0[7];
	uint8_t noise;			/* raw, then calibrated by wlanRxConvertStatus() */
	uint8_t reserved0b[3];
	uint8_t rssi;			/* raw, then calibrated by wlanRxConvertStatus() */
	uint8_t reserved0c[8];
	uint8_t mcs;			/* bit 7: 40 MHz */
	uint8_t reserved1[3];
	uint8_t flags;			/* bit 3: A-MPDU member, bit 7: short guard interval */
	uint8_t reserved2[2];
	uint8_t legacyRate;		/* hardware rate code, then the rate index */
	uint8_t reserved3[24];
	uint8_t phyInfo;		/* bits 3-4: PHY mode, 2 and 3 are HT */
	uint8_t reserved4[3];
	uint8_t qosControl[2];		/* TID in bits 0-2, EOSP bit 4, A-MSDU bit 7 */
	uint8_t reserved5[6];
	uint8_t frame[4];		/* unaligned pointer to the frame header */
};

_Static_assert(sizeof(struct WlanRxStatus) == 68, "receive status");

void wlanBuildPsPoll(OsaBuffer *buffer, const uint8_t *transmitter, const uint8_t *bssid,
		     int powerSave, uint16_t aid);
void wlanSetHeaderAddresses(WlanWirelessFrame *frame, const uint8_t *address1,
			    const uint8_t *address2, const uint8_t *address3);
void wlanInitMgmtHeader(WlanWirelessFrame *frame, unsigned int type, unsigned int subtype,
			const uint8_t *address1, const uint8_t *address2, const uint8_t *address3);
unsigned int wlanDataFrameControl(int qos, int nullData, int moreData);
int wlanRxFilterToDs(int enabled, OsaBuffer *buffer, const WlanWirelessFrame *frame);
int wlanRxFilterIbss(int enabled, OsaBuffer *buffer, const WlanWirelessFrame *frame,
		     const uint8_t *bssid);
int wlanRxFilterStation(int enabled, OsaBuffer *buffer, const WlanWirelessFrame *frame,
			const uint8_t *bssid);
int wlanRxClearProtectedOnNull(WlanWirelessFrame *frame);
void wlanRxDropLoopback(OsaBuffer *buffer, const WlanWirelessFrame *frame, const uint8_t *own);
void wlanRxCheckFragment(OsaBuffer *buffer, const WlanWirelessFrame *frame,
			 uint16_t *lastSequence, uint16_t *lastFragment, unsigned int maxFragments);
int wlanIsNullData(const WlanWirelessFrame *frame);
int wlanIsFragment(const WlanWirelessFrame *frame);
void wlanRxConvertStatus(struct WlanRxStatus *status);
void wlanRxDecodeRate(const struct WlanRxStatus *status, uint8_t *htInfo, uint8_t *rate);

#endif /* ROBIN_WLANRX_H */
