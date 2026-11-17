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
void p2pWpsConfigAppend(struct WlanConnection *connection, uint8_t *frame);	/* SQRAM 0xc000b1da */
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
void p2pProbeRequestAnswer(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000ba1a */
int p2pListenSlotStart(struct WlanConnection *connection, uint32_t slot);	/* SQRAM 0xc000bce4, veneer 0x53284 */
int p2pChannelShared(struct WlanConnection *connection);		/* SQRAM 0xc000bdaa, veneer 0x53294 */
void p2pDiscoverabilityResponseBuild(struct WlanConnection *connection, uint8_t *frame);	/* SQRAM 0xc000bdb6 */
int p2pDiscoverabilityRequestAnswer(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000bdee */
int p2pDiscoverabilityResponseCheck(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000bf6c */
int p2pGoNegotiationRequestAnswer(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000bfa0 */
int p2pGoNegotiationConfirmed(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000c076 */
void p2pDeviceStateSave(struct WlanConnection *connection);		/* SQRAM 0xc000c114 */
void p2pDeviceStateRestore(struct WlanConnection *connection);		/* SQRAM 0xc000c14a */
int p2pGoNegotiationResponseAnswer(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000c1b4 */
int p2pProvisionDiscoveryRequestAnswer(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000c2ae */
int p2pProvisionDiscoveryResponseAnswer(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000c410 */
unsigned int p2pPresenceResponseBuild(struct WlanConnection *connection, uint8_t *frame);	/* SQRAM 0xc000c4b6 */
int p2pVendorActionSend(struct WlanConnection *connection, const uint8_t *destination, unsigned int kind,
			unsigned int dialogToken, unsigned int unused, unsigned int subtype);	/* SQRAM 0xc000c512 */
int p2pPresenceRequestAnswer(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000c5c0 */
int p2pActionFrameReceive(struct WlanConnection *connection, const uint8_t *frame, unsigned int kind);	/* SQRAM 0xc000c622 */
int p2pActionFrameRoute(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000c82a, veneer 0x53244 */
void p2pPublicActionSent(struct WlanConnection *connection, unsigned int subtype);	/* SQRAM 0xc000c848 */
void p2pActionSent(struct WlanConnection *connection, unsigned int subtype);	/* SQRAM 0xc000c8ac */
void p2pConfirmationAcked(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000c8da */
struct WlanConnection *p2pActionFrameConnection(const struct OsaBuffer *buffer, uint32_t *forUs);	/* SQRAM 0xc000c92c */
int p2pServiceDiscoverySend(struct WlanConnection *connection, const uint8_t *request, unsigned int length);	/* SQRAM 0xc000c9c2 */
void p2pFrameSent(struct OsaBuffer *buffer, int acknowledged);		/* SQRAM 0xc000cac0 */
void p2pScanResultFound(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000cb66, veneer 0x53a04 */
int p2pDiscoveryActive(struct WlanConnection *connection);		/* SQRAM 0xc000cbbc, veneer 0x539fc */
void p2pHostAttributesSet(struct WlanConnection *connection, const uint8_t *attributes,
			  unsigned int length);					/* SQRAM 0xc000cc1c, veneer 0x52e0c */
void p2pHostAttributesGet(struct WlanConnection *connection, uint8_t *entry, uint16_t *length);	/* SQRAM 0xc000cd36, veneer 0x52e1c */
int p2pActionCommand(struct WlanConnection *connection, const uint8_t *command);	/* SQRAM 0xc000cf5a */
int p2pPublicActionCommand(struct WlanConnection *connection, const uint8_t *command);	/* SQRAM 0xc000cf7e */
int hostCmd00f4(struct WlanConnection *connection, uint8_t *command);		/* SQRAM 0xc000d06c */
int hostCmd00ea(struct WlanConnection *connection, uint8_t *command);		/* SQRAM 0xc000d086 */
void wpsAttributesParse(const uint8_t *data, unsigned int length, uint8_t **attributes);	/* SQRAM 0xc000ce8c */
void p2pInvitationTimerExpired(uint32_t timer, uint32_t argument);	/* SQRAM 0xc000d80c */
int p2pInvitationInvokesGroup(struct WlanConnection *connection);	/* SQRAM 0xc000d83a */
struct WlanP2pPersistentGroup;
void p2pInvitationRequestBuild(struct WlanConnection *connection, uint8_t *frame,
			       struct WlanP2pPersistentGroup *group, unsigned int mode,
			       struct WlanDirectContext *context);		/* SQRAM 0xc000d8ae */
void p2pInvitationResponseBuild(struct WlanConnection *connection, uint8_t *frame,
				struct WlanP2pPersistentGroup *group, struct WlanDirectContext *peer,
				unsigned int mode);				/* SQRAM 0xc000d986 */
uint8_t *p2pInvitationTargetGet(struct WlanConnection *connection, uint32_t *index);	/* SQRAM 0xc000da82 */
int p2pPersistentGroupClientFind(const struct WlanP2pPersistentGroup *group, const uint8_t *address);	/* SQRAM 0xc000da9c */
int p2pInvitationListFind(struct WlanConnection *connection, const uint8_t *address);	/* SQRAM 0xc000dad6 */
int p2pInvitationRequestSend(struct WlanConnection *connection, uint32_t index, const uint8_t *address,
			     struct WlanDirectContext *peer);			/* SQRAM 0xc000db0c */
int p2pPersistentGroupFind(struct WlanConnection *connection, const uint8_t *groupId);	/* SQRAM 0xc000dbb0 */
int p2pPersistentGroupFreeSlot(struct WlanConnection *connection);	/* SQRAM 0xc000dbf0 */
void p2pPersistentGroupStore(struct WlanConnection *connection, const uint8_t *groupId,
			     const uint8_t *groupBssid, const uint8_t *ssid, unsigned int role,
			     const uint8_t *deviceAddress);			/* SQRAM 0xc000dc2e */
int p2pPersistentGroupClientAdd(struct WlanP2pPersistentGroup *group, const uint8_t *address);	/* SQRAM 0xc000dc90 */
struct WlanP2pPersistentGroup *p2pPersistentGroupLookup(struct WlanConnection *connection, const uint8_t *deviceAddress,
							const uint8_t *ssid, unsigned int ssidLength);	/* SQRAM 0xc000dcce, veneer 0x5334c */
int p2pInvitationRequestAnswer(struct WlanConnection *connection, const uint8_t *frame);	/* SQRAM 0xc000dda6 */
int p2pInvitationResponseAnswer(struct WlanConnection *connection, const uint8_t *frame, uint32_t index);	/* SQRAM 0xc000e05a */
int p2pPersistentGroupPskSave(struct WlanConnection *connection, const uint8_t *deviceAddress,
			      const uint8_t *ssid, unsigned int ssidLength);	/* SQRAM 0xc000dd60, veneer 0x52f5c */
void p2pPersistentGroupsInit(struct WlanPsPeerState *ps);		/* SQRAM 0xc000e1f0 */

/* sqram/uapdefault.c */
void uapDefaultRatesSet(uint8_t *rates);				/* SQRAM 0xc000e5a8 */
void uapDefaultCapabilitySet(uint8_t *capability);			/* SQRAM 0xc000e5da */
void uapDefaultHtCapabilitySet(uint8_t *htCapability);			/* SQRAM 0xc000e5e4 */

#endif /* ROBIN_UAPBSS_H */
