/*
 * IBSS (ad-hoc) networks: the beacon and probe response templates the MAC
 * sends, starting and joining an IBSS, and adopting the timing and BSSID of
 * a network that merges with ours. See wlan/ibss.c.
 */
#ifndef ROBIN_IBSS_H
#define ROBIN_IBSS_H

#include <stdint.h>
#include "robin/adhoc.h"
#include "robin/buffer.h"
#include "robin/wlan.h"

struct WlanConnection;
struct WlanMacTxRequest;

/* What the IBSS started or joined runs with (0x56878); byte aligned. */
struct __attribute__((packed)) WlanIbssInfo {
	uint8_t reserved0[4];
	uint8_t bssid[6];
	uint16_t beaconPeriod;		/* TU */
	uint16_t atimWindow;		/* TU */
	uint16_t flags;			/* WLAN_IBSS_* */
};

_Static_assert(sizeof(struct WlanIbssInfo) == 16, "IBSS information");

/* WlanIbssInfo.flags */
#define WLAN_IBSS_PROTECTION		0x0002	/* the BSS uses ERP protection */

extern struct WlanIbssInfo wlanIbssInfo;

void wlanIbssTemplateSetProtection(WlanWirelessFrame *frame, int protection);
void wlanIbssUpdateProtection(void);
void wlanIbssBuildTemplate(struct WlanConnection *connection, const WlanAdHocStart *start,
			   const uint8_t *bssid, unsigned int subtype, OsaBuffer *buffer,
			   struct WlanMacTxRequest *request);
void wlanIbssSetInfo(struct WlanConnection *connection, uint16_t beaconPeriod,
		     uint16_t atimWindow);
void wlanIbssJoin(struct WlanConnection *connection, const WlanAdHocBssDesc *join);
void wlanIbssStart(struct WlanConnection *connection, const WlanAdHocStart *start);
void wlanIbssAdopt(struct WlanConnection *connection, const WlanWirelessFrame *frame,
		   const uint8_t *ibssParameters, const uint8_t *erp, const uint8_t *htInformation);

struct WlanIePointers;
void wlanIbssBeaconReceived(struct WlanConnection *connection, const WlanWirelessFrame *frame,
			    const void *unused, const struct WlanIePointers *ies,
			    unsigned int peerRates);
int wlanIbssSmActive(const struct WlanConnection *connection);
void wlanConnectionTouch(struct WlanConnection *connection);
struct WlanIbssInfo *wlanIbssGetInfo(void);
void wlanIbssRefreshBeacons(struct WlanConnection *connection);
int wlanChannelValidate(void);
void wlanIbssReset(void);
struct WlanConnection *wlanIbssMatchBeacon(const WlanWirelessFrame *frame, const uint8_t *body);
void wlanIbssBeaconMatched(const WlanWirelessFrame *frame, const uint8_t *body);
struct WlanConnection *wlanIbssFindMerge(const WlanWirelessFrame *frame);
void wlanIbssJoinDone(struct WlanConnection *connection, int status);
void wlanIbssStartDone(struct WlanConnection *connection, int status);
void wlanIbssJoinSetup(struct WlanConnection *connection, const WlanAdHocBssDesc *join);
void wlanIbssRefreshBeacon(WlanBeaconDescriptor *const *beacon);

#endif /* ROBIN_IBSS_H */
