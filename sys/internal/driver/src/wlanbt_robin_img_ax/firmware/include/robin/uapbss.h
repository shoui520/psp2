/*
 * The micro access point's BSS defaults and the P2P helpers that follow them
 * in SQRAM (sqram/uapbss.c), and the default elements the P2P code gives them
 * (sqram, not converted yet). No Marvell
 * source names them; the names are inferred.
 */
#ifndef ROBIN_UAPBSS_H
#define ROBIN_UAPBSS_H

#include <stdint.h>

struct WlanConnection;

void uapExtCapabilityInit(uint8_t *element);				/* SQRAM 0xc000a6a8 */
void uapObssScanInit(uint8_t *element);					/* SQRAM 0xc000a6c4 */
void uapBssDefaults(struct WlanConnection *connection);			/* SQRAM 0xc000a704 */
void uapBssInit(struct WlanConnection *connection);			/* SQRAM 0xc000a9ba */
void uapBssApply(struct WlanConnection *connection, struct WlanConnection *peer,
		 const uint8_t *address);				/* SQRAM 0xc000a9ec */

void p2pDiscoveryResume(struct WlanConnection *connection);			/* SQRAM 0xc000ab98 */
int p2pEventSend(struct WlanConnection *connection, const uint8_t *frame, uint16_t event, unsigned int type,
		 uint16_t value);						/* SQRAM 0xc000abbc */
int p2pEventSendPeer(struct WlanConnection *connection, const uint8_t *frame, unsigned int type,
		     uint16_t value);						/* SQRAM 0xc000ac0a */
void p2pListenTimerExpired(uint32_t timer, uint32_t argument);		/* SQRAM 0xc000ac18 */
void p2pPendingFrameTimerExpired(uint32_t timer, uint32_t argument);	/* SQRAM 0xc000ac80 */
int p2pPendingFrameSend(struct WlanConnection *connection);		/* SQRAM 0xc000aca4 */
void p2pPendingFrameRetryExpired(uint32_t timer, uint32_t argument);	/* SQRAM 0xc000acc2 */
void p2pBssTimerExpired(uint32_t timer, uint32_t argument);		/* SQRAM 0xc000acc6 */
void p2pDiscoverabilityTimerExpired(uint32_t timer, uint32_t argument);	/* SQRAM 0xc000ace2 */
void p2pGroupSsidUpdate(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000acf6 */
int p2pBssActiveGet(struct WlanConnection *connection);			/* SQRAM 0xc000adee, veneer 0x53254 */
void p2pConnectionStateSet(struct WlanConnection *connection, unsigned int state);	/* SQRAM 0xc000adfe */
struct WlanDirectContext;
unsigned int p2pGroupRoleDecide(const struct WlanDirectContext *peer, unsigned int intent);	/* SQRAM 0xc000ae12 */
void p2pGroupRoleApply(struct WlanConnection *connection, struct WlanDirectContext *peer,
		       unsigned int role);					/* SQRAM 0xc000ae48 */
void p2pDeviceIdAppend(uint8_t *frame, const uint8_t *address);		/* SQRAM 0xc000af78 */
struct OsaBuffer;
int p2pActionFrameAlloc(struct WlanConnection *connection, struct OsaBuffer **frame, const uint8_t *destination,
			unsigned int kind, uint8_t subtype);			/* SQRAM 0xc000afb6 */
int p2pDiscoverabilityRequestSend(struct WlanConnection *connection, const uint8_t *destination);	/* SQRAM 0xc000b0be */
struct WlanPsPeerState;
void p2pDiscoverabilityCheck(struct WlanDirectContext *peer, uint32_t changed,
			     struct WlanPsPeerState *ps);				/* SQRAM 0xc000b124 */
unsigned int p2pWpsConfigAppend(struct WlanConnection *connection, uint8_t *frame);	/* SQRAM 0xc000b1da */
void p2pOperatingChannelSelect(struct WlanConnection *connection, const struct WlanDirectContext *peer);	/* SQRAM 0xc000b24c */
int p2pGoNegotiationBuild(struct WlanConnection *connection, const uint8_t *request, uint8_t *response,
			  unsigned int kind);					/* SQRAM 0xc000b330 */
int p2pGoNegotiationRequestSend(struct WlanConnection *connection);	/* SQRAM 0xc000b71c */
void p2pWpsConfigMethodsAppend(struct WlanConnection *connection, uint8_t *frame,
			       const uint8_t *configMethods);			/* SQRAM 0xc000b7c0 */
void p2pProvisionDiscoveryBuild(struct WlanConnection *connection, uint8_t *frame,
				uint8_t *groupId);				/* SQRAM 0xc000b800 */
int p2pProvisionDiscoveryRequestSend(struct WlanConnection *connection, const uint8_t *destination,
				     uint8_t *groupId);				/* SQRAM 0xc000b864 */
void p2pPeerFound(struct WlanConnection *connection, const uint8_t *frame, unsigned int pending);	/* SQRAM 0xc000b8e0 */
uint8_t *p2pSqramTrackContext(struct WlanConnection *connection, uint32_t *invitation);	/* SQRAM 0xc000da82, not converted yet */
void p2pSqramReleaseContext(struct WlanConnection *connection, uint32_t invitation, uint8_t *address,
			    struct WlanDirectContext *peer);			/* SQRAM 0xc000db0c, not converted yet */
void p2pSqramConvertContext(struct WlanConnection *connection, const uint8_t *groupId,
			    const uint8_t *groupBssid, const uint8_t *ssid, unsigned int mode,
			    const uint8_t *deviceAddress);			/* SQRAM 0xc000dc2e, not converted yet */

void p2pSqramResetEntry(uint8_t *rates);				/* SQRAM 0xc000e5a8: the 12 802.11g rates, 1, 2, 5.5 and 11 basic */
void p2pSqramSyncEntry(uint8_t *capability);				/* SQRAM 0xc000e5da: ESS, short preamble, short slot time */
void p2pSqramApplyEntry(uint8_t *htCapability);				/* SQRAM 0xc000e5e4: the default HT capability element */

#endif /* ROBIN_UAPBSS_H */
