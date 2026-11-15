/*
 * A P2P group owner's connection (interface type 3; wlan/p2pgo.c): the group is started, set up and
 * torn down here, and the information elements its frames carry are built. The names of the
 * firmware's placeholders are kept; the state is WlanPsPeerState (robin/mactx.h).
 */
#ifndef ROBIN_P2PGO_H
#define ROBIN_P2PGO_H

#include <stdint.h>
#include "robin/wlan.h"

struct WlanConnection;
struct WlanConnectionInfo;

/* The bits of WlanPsPeerState.configuredMask that must be set before the group can start. */
#define P2P_GROUP_REQUIRED	0x3abfcu

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
void p2pDirectRecordState(uint8_t *element);						/* 0x40098 */
void p2pDirectFilterState(uint8_t *buffer);						/* 0x400b2 */

#endif /* ROBIN_P2PGO_H */
