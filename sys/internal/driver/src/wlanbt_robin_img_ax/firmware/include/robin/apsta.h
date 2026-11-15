/*
 * An access point's stations: authentication, leaving, association IDs and the
 * commands that start, stop and reset the BSS (wlan/apauth.c).
 */
#ifndef ROBIN_APSTA_H
#define ROBIN_APSTA_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/wlan.h"
#include "robin/mactx.h"
#include "robin/wlanrx.h"

int wlanEventMaskIndex(int reason);							/* 0x2ab36 */
unsigned int wlanBitmapAllocate(uint8_t *bitmap, unsigned int count);			/* 0x2b3f0 */
void wlanBitmapFree(uint8_t *bitmap, unsigned int count, unsigned int number);		/* 0x2b424 */
void macMgmtDispatchBuffer(struct WlanConnection *connection, const uint8_t *peer,
			   unsigned int eventCode);					/* 0x3854e */
int macMgmtBuildBuffer(struct WlanConnection *connection, unsigned int kind);		/* 0x385de */
int hostCmd00af(struct WlanConnection *connection);					/* 0x38608 */
int hostCmd00b2(struct WlanConnection *connection);					/* 0x3861c */
int macMgmtResetBuffer(struct WlanConnection *connection, int unused, int kind, int reply); /* 0x38648 */
int macMgmtSyncBuffer(struct WlanConnection *connection, const uint8_t *record, int reply); /* 0x386e2 */
int hostCmd00b1(struct WlanConnection *connection);					/* 0x3876e */
int hostCmd00b5(struct WlanConnection *connection, uint8_t *command);			/* 0x38564 */
void macMgmtScheduleBuffer(struct WlanConnection *connection);				/* 0x388a4 */
void macMgmtNotifyBuffer(struct WlanConnection *connection, int keep);			/* 0x38938 */
void macMgmtFormatBuffer(uint32_t timer, uint32_t argument);				/* 0x38974 */
int macMgmtInspectBuffer(struct WlanConnection *connection, const uint8_t *peer,
			 unsigned int reason);						/* 0x38990 */
int macMgmtTrackBuffer(struct WlanConnection *connection, unsigned int reason);		/* 0x38a18 */
int macMgmtManageBuffer(struct WlanConnection *connection, const uint8_t *peer);	/* 0x38a8a */
void macMgmtAcquireBuffer(struct WlanConnection *connection);				/* 0x38ae0 */
int macMgmtReleaseBuffer(OsaBuffer *buffer);						/* 0x38b04 */
int macMgmtSetupBuffer(uint16_t algorithm, uint16_t sequence, uint16_t status,
		       const void *challenge, struct WlanConnection *connection);	/* 0x38b42 */
int macMgmtClearBuffer(const WlanWirelessFrame *frame, unsigned int sequence,
		       unsigned int algorithm, const uint8_t *request);			/* 0x38b8e */
int macMgmtConvertBuffer(OsaBuffer *buffer);						/* 0x38bec */
int macMgmtVerifyBuffer(OsaBuffer *buffer, int status);					/* 0x38c00 */
int macMgmtSelectBuffer(struct WlanConnection *connection, uint8_t **text);		/* 0x38c52 */
void macMgmtAdjustBuffer(OsaBuffer *buffer, uintptr_t argument);		/* 0x38cc4 */
unsigned int macMgmtRecordBuffer(struct WlanConnection *connection);			/* 0x38db8 */
void macMgmtFilterBuffer(struct WlanConnection *connection);				/* 0x38e14 */
void fmCoexUpdateTable(uint8_t *text);							/* 0x4bcb6 */
void fmCoexScheduleTable(uint8_t *frame, uint16_t algorithm, uint16_t sequence,
			 uint16_t status, const void *challenge);			/* 0x4bebe */

/* wlan/apassoc.c */
int macMgmtHandlePacket(OsaBuffer *buffer, uintptr_t argument);				/* 0x3902c */
int macMgmtUpdatePacket(struct WlanConnection *connection, int wps, const void *rsn,
			const void *wpa, const uint8_t *wapi, uint16_t *status);	/* 0x390a2 */
int macMgmtCheckPacket(OsaBuffer *buffer);						/* 0x39898 */
void macMgmtPreparePacket(struct WlanConnection *connection);				/* 0x398c4 */
void macMgmtDispatchPacket(OsaBuffer *buffer);						/* 0x39954 */
void macMgmtParsePacket(struct WlanConnection *connection, int value, int code);	/* 0x399a8 */
void macMgmtBuildPacket(unsigned int interfaceType, unsigned int bssNumber);		/* 0x399b4 */
void macMgmtComputePacket(struct WlanConnectionInfo *info, uint8_t *element,
			  const uint8_t *htCapability, const struct WlanConnectionRole *role,
			  unsigned int beaconPeriod);					/* 0x399bc */
void macMgmtQueryPacket(struct WlanConnection *connection);				/* 0x39a06 */
int macMgmtResetPacket(struct WlanConnection *connection, int flushAll);		/* 0x39b06 */
int macMgmtSyncPacket(struct WlanConnection *connection);				/* 0x39b98 */
int macMgmtApplyPacket(void);								/* 0x39bca */
int macMgmtResolvePacket(struct WlanConnection *connection);				/* 0x39bd2 */

void macMgmtNotifyPacket(struct WlanConnection *connection, uint8_t *out);		/* 0x39d08 */
int macMgmtFormatPacket(struct WlanConnection *connection);				/* 0x39d82 */
int macMgmtInspectPacket(struct WlanConnection *connection, unsigned int a, unsigned int b); /* 0x3a06e */
int macMgmtTrackPacket(OsaBuffer *buffer, uintptr_t argument);				/* 0x3a0a6 */

void macMgmtManagePacket(struct WlanConnection *connection, int want);			/* 0x3a100 */
void macMgmtAcquirePacket(struct WlanConnection *connection, uint8_t *counter,
			  uint8_t *erpInfo);						/* 0x3a204 */
void macMgmtReleasePacket(struct WlanConnection *connection, const uint8_t *body, uint16_t length,
			  unsigned int beaconPeriod, uint8_t *erpInfo, uint8_t *counter,
			  unsigned int seconds);					/* 0x3a230 */
void macMgmtSetupPacket(OsaBuffer *buffer);						/* 0x3a2ba */
void macMgmtClearPacket(struct WlanConnection *connection);				/* 0x3a2fa */
void macMgmtConvertPacket(struct WlanConnection *connection);				/* 0x3a32a */
int macMgmtVerifyPacket(struct WlanConnection *connection, const uint8_t *frame);	/* 0x3a33e */
void macMgmtSelectPacket(struct WlanConnection *connection, OsaBuffer *buffer);		/* 0x3a410 */
int macMgmtAdjustPacket(struct WlanConnection *connection, const uint8_t *peer);	/* 0x3a454 */
void macMgmtRecordPacket(struct WlanConnection *connection, OsaBuffer *buffer,
			 uintptr_t argument);						/* 0x3a4e2 */
void macMgmtFilterPacket(int code, unsigned int interfaceType);				/* 0x3a5aa */

/* The management task's message queue (wlan/tasks.c). */
OsaBuffer *macMgmtHandleState(struct WlanConnection *connection);			/* 0x34778 */
int macMgmtProcessState(OsaBuffer *message);						/* 0x34794 */

/* Not converted yet; provisional names. */
void macTxNotifyConfigureFrame(struct WlanConnection *connection);			/* 0x333ce */
void macTxNotifyParseFrame(struct WlanConnection *connection);				/* 0x33528 */
int macMgmtSchedulePacket(void);							/* 0x39cb2 */
struct WlanConnection *p2pDirectProcessEntry(unsigned int type);			/* 0x41250 */
void p2pDirectSetupContext(struct WlanConnection *connection, uint8_t *context);	/* 0x40f46 */
void uapSqramConfigureBuffer(struct WlanConnection *connection, struct WlanConnection *peer,
			     const uint8_t *address);					/* SQRAM, veneer 0x52dcc */
int wlanP2pConnection(struct WlanConnection *connection);				/* 0x41228 */
struct WlanPsPeerState *wlanConnectionPsState(struct WlanConnection *connection);	/* 0x1d1f6 */
void uapSqramConfigureContext(uint8_t *keyBlock);					/* SQRAM */
void uapSqramResolveContext(uint8_t **keyBlock);					/* SQRAM */

#endif /* ROBIN_APSTA_H */
