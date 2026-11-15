/*
 * A P2P group owner's connection (interface type 3; wlan/p2pgo.c): the group is started, set up and
 * torn down here, and the information elements its frames carry are built. The names of the
 * firmware's placeholders are kept; the state is WlanPsPeerState (robin/mactx.h).
 */
#ifndef ROBIN_P2PGO_H
#define ROBIN_P2PGO_H

#include <stdint.h>
#include "robin/wlan.h"
#include "robin/wlantask.h"

struct WlanConnection;
struct WlanConnectionInfo;
struct WlanWmmSlot;

/* The bits of WlanPsPeerState.configuredMask that must be set before the group can start. */
#define P2P_GROUP_REQUIRED	0x3abfcu

/* The P2P attributes a frame carries, or is built with: one pointer per attribute ID of the P2P specification. */
enum {
	P2P_ATTR_STATUS = 0,
	P2P_ATTR_MINOR_REASON = 1,
	P2P_ATTR_CAPABILITY = 2,
	P2P_ATTR_DEVICE_ID = 3,
	P2P_ATTR_GO_INTENT = 4,
	P2P_ATTR_CONFIGURATION_TIMEOUT = 5,
	P2P_ATTR_LISTEN_CHANNEL = 6,
	P2P_ATTR_GROUP_BSSID = 7,
	P2P_ATTR_EXTENDED_LISTEN_TIMING = 8,
	P2P_ATTR_INTENDED_INTERFACE_ADDRESS = 9,
	P2P_ATTR_MANAGEABILITY = 10,
	P2P_ATTR_CHANNEL_LIST = 11,
	P2P_ATTR_NOTICE_OF_ABSENCE = 12,
	P2P_ATTR_DEVICE_INFO = 13,
	P2P_ATTR_GROUP_INFO = 14,
	P2P_ATTR_GROUP_ID = 15,
	P2P_ATTR_INTERFACE = 16,
	P2P_ATTR_OPERATING_CHANNEL = 17,
	P2P_ATTR_INVITATION_FLAGS = 18,
	P2P_ATTR_COUNT = 19,
};

/* A client of the group as its Group Info attribute lists it (WlanPsPeerState.groupInfoAttribute). */
struct WlanP2pClient {
	uint8_t length;			/* the bytes that follow */
	uint8_t deviceAddress[6];
	uint8_t interfaceAddress[6];
	uint8_t capability;		/* the Device Capability bitmap */
	uint8_t information[];		/* the Device Info attribute after its device address */
};

extern struct WlanInitStage p2pDirectInitStage;		/* 0x55244 */

/* The slots of WlanDirectContext.attributes (and of WlanPsPeerState.customIe), which the firmware does not order by attribute ID. */
enum {
	P2P_SLOT_CAPABILITY = 0,
	P2P_SLOT_GO_INTENT = 1,
	P2P_SLOT_STATUS = 2,
	P2P_SLOT_LISTEN_CHANNEL = 3,
	P2P_SLOT_OPERATING_CHANNEL = 4,
	P2P_SLOT_CHANNEL_LIST = 5,
	P2P_SLOT_NOTICE_OF_ABSENCE = 6,
	P2P_SLOT_GROUP_INFO = 7,
	P2P_SLOT_DEVICE_INFO = 8,
	P2P_SLOT_GROUP_ID = 9,
	P2P_SLOT_GROUP_BSSID = 10,
	P2P_SLOT_MANAGEABILITY = 11,
	P2P_SLOT_MINOR_REASON = 12,
	P2P_SLOT_EXTENDED_LISTEN_TIMING = 13,
	P2P_SLOT_DEVICE_ID = 14,
	P2P_SLOT_INTERFACE = 15,
	P2P_SLOT_CONFIGURATION_TIMEOUT = 16,
	P2P_SLOT_INTENDED_INTERFACE_ADDRESS = 17,
	P2P_SLOT_INVITATION_FLAGS = 18,
	P2P_SLOT_COUNT = 19,
};

/* The attributes the host or the peer gave, by slot; the masks of p2pDirectCheckContext() are by attribute ID (bit n: ID n). */
struct WlanDirectContext {
	uint8_t *attributes[P2P_SLOT_COUNT];	/* NULL: none */
};

extern const uint8_t wlanWpsOui[4];			/* 0x53f86: the OUI and type of the WPS element */
extern const uint8_t wlanP2pOui[4];			/* SQRAM 0xc000faa3: the OUI and type of the P2P element */

extern struct WlanConnection *p2pGroupConnection;	/* 0x55260: the group owner's connection once it is set up */
extern const char *p2pDirectSsidPrefix;			/* 0x55240: "DIRECT-" */

unsigned int p2pDirectNotifyState(struct WlanConnection *connection);			/* 0x3f8e4 */
int p2pDirectFormatState(struct WlanConnection *connection, const uint8_t *address);	/* 0x3f94a */
int p2pDirectInspectState(void);							/* 0x3f97c */
unsigned int p2pDirectTrackState(struct WlanConnection *connection, struct WlanConnectionInfo *info); /* 0x3f9b6 */
unsigned int p2pDirectManageState(struct WlanConnection *connection);			/* 0x3fa14 */
unsigned int p2pDirectAcquireState(struct WlanConnection *connection);			/* 0x3faba */
void p2pDirectReleaseState(uint8_t *suffix);						/* 0x3fcac */
unsigned int p2pDirectCheckSsidPrefix(struct WlanConnection *connection);		/* 0x3fd2a */
unsigned int p2pDirectSetupState(struct WlanConnection *connection);			/* 0x3ff14 */
unsigned int p2pDirectClearState(uint8_t *out, const uint8_t *attribute);		/* 0x3ffc4 */
unsigned int p2pDirectConvertState(struct WlanConnection *connection, WlanWirelessFrame *frame, unsigned int mask); /* 0x4000a */
unsigned int p2pDirectVerifyState(struct WlanConnection *connection, WlanWirelessFrame *frame);	/* 0x4008c */
unsigned int p2pDirectSelectState(struct WlanConnection *connection, WlanWirelessFrame *frame);	/* 0x40090 */
unsigned int p2pDirectAdjustState(struct WlanConnection *connection, WlanWirelessFrame *frame);	/* 0x40094 */
uint8_t *p2pDirectRecordState(uint8_t *element);						/* 0x40098 */
uint8_t *p2pDirectFilterState(uint8_t *buffer);						/* 0x400b2 */

unsigned int p2pDirectHandleContext(const uint8_t *list, uint8_t *attribute);		/* 0x400e0 */
unsigned int p2pDirectProcessContext(unsigned int id, const uint8_t *list, uint8_t *attribute);	/* 0x40102 */
void p2pDirectUpdateContext(struct WlanConnection *connection, const uint8_t *first, unsigned int firstCount,
			    const uint8_t *second, unsigned int secondCount);		/* 0x40122 */
unsigned int p2pDirectConfigureContext(uint8_t *element, const uint8_t *attribute);	/* 0x401b2 */
unsigned int p2pDirectCheckContext(struct WlanConnection *connection, WlanWirelessFrame *frame,
				   struct WlanDirectContext *context, unsigned int include,
				   unsigned int exclude);				/* 0x402ea */
unsigned int p2pDirectValidateContext(struct WlanConnection *connection, WlanWirelessFrame *frame);	/* 0x404be */
unsigned int p2pDirectPrepareContext(struct WlanConnection *connection, WlanWirelessFrame *frame);	/* 0x404ec */
uint8_t *p2pDirectDispatchContext(WlanWirelessFrame *frame, int reassociation, const void *oui, unsigned int size); /* 0x4054a */
const uint8_t *p2pDirectParseContext(const uint8_t *attribute, uint16_t *remaining);	/* 0x405be */
unsigned int p2pDirectBuildContext(const uint8_t *attributes, unsigned int length);	/* 0x4060c */
void p2pDirectComputeContext(uint8_t *element);						/* 0x4063a */
uint8_t *p2pDirectQueryContext(uint8_t **next, uint8_t **length, uint16_t *remaining, unsigned int size,
			       uint16_t *total, uint16_t *frameLength, const void *oui);	/* 0x40652 */
uint8_t *p2pDirectResetContext(uint8_t *position);					/* 0x406a6 */
void p2pDirectSyncContext(const uint8_t *source, uint8_t *destination, int limit, uint8_t **mark);	/* 0x406ca */
unsigned int p2pDirectApplyContext(uint8_t *out, const void *oui, unsigned int size, const uint8_t *in,
				   uint16_t limit);					/* 0x407a4 */
void p2pDirectResolveContext(const uint8_t *element, unsigned int *mask, unsigned int *changed, int length,
			     struct WlanDirectContext *context);			/* 0x40992 */
struct WlanDirectContext *p2pDirectScheduleContext(const uint8_t *frame, int body, unsigned int *mask,
						   unsigned int *changed, struct WlanDirectContext *context); /* 0x40b16 */
unsigned int p2pDirectNotifyContext(struct WlanConnection *connection, const uint8_t *frame, uint8_t *event,
				    unsigned int type, uint16_t value);			/* 0x40b88 */
int p2pDirectFormatContext(struct WlanConnection *connection);				/* 0x40da2 */
int p2pDirectInspectContext(WlanWirelessFrame *frame);					/* 0x40dc2 */
int p2pDirectTrackContext(const WlanWirelessFrame *frame);				/* 0x40dd6 */
void p2pDirectManageContext(struct WlanConnection *connection);				/* 0x40de4 */
int p2pDirectAcquireContext(void);							/* 0x40df6 */
struct WlanP2pClient *p2pDirectReleaseContext(struct WlanConnection *connection, const WlanWirelessFrame *request); /* 0x40e20 */
int p2pDirectSetupContext(struct WlanConnection *connection, struct WlanP2pClient *client);	/* 0x40f46 */

/* The direct link and the TSPECs of the group (wlan/p2pdirect.c). */
int wlanDirectLinkPoll(struct WlanConnection *connection, uintptr_t *delay);			/* 0x41050 */
void wlanDirectLinkTrigger(void);								/* 0x41184 */
void p2pDirectVerifyContext(void);								/* 0x411a8 */
void p2pDirectSelectContext(uint32_t timer, uint32_t argument);					/* 0x411be */
void p2pDirectAdjustContext(struct WlanConnection *connection);					/* 0x411ee */
void wlanDirectLinkSend(struct WlanConnection *connection, uintptr_t delay);			/* 0x41202 */
int wlanPsUapsdActive(struct WlanConnection *connection);					/* 0x4123c */
unsigned int p2pDirectUpdateEntry(struct WlanConnection *connection, unsigned int unused);	/* 0x41268 */
unsigned int p2pDirectConfigureEntry(void);							/* 0x412d8 */
unsigned int p2pDirectCheckEntry(void);								/* 0x41304 */
int p2pDirectValidateEntry(uint8_t *toRequest);							/* 0x41338 */
void p2pDirectPrepareEntry(struct WlanWmmSlot *session, int action);				/* 0x4137e */
int p2pDirectDispatchEntry(unsigned int slot);							/* 0x413ac */
void p2pDirectParseEntry(uint32_t timer, uint32_t argument);					/* 0x413e4 */

#endif /* ROBIN_P2PGO_H */
