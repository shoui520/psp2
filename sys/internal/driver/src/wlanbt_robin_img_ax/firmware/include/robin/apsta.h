/*
 * An access point's stations: authentication, leaving, association IDs and the
 * commands that start, stop and reset the BSS (wlan/apauth.c).
 */
#ifndef ROBIN_APSTA_H
#define ROBIN_APSTA_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/esatypes.h"
#include "robin/ie.h"
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
			 uint16_t reason);						/* 0x38990 */
int macMgmtTrackBuffer(struct WlanConnection *connection, uint16_t reason);		/* 0x38a18 */
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
int fmCoexConfigureTable(WlanWirelessFrame *frame, const WlanWirelessFrame *request,
			 const WlanIePointers *ies, const Cipher_t *cipher,
			 const uint8_t *htCapability);					/* 0x4bcea */
int fmCoexCheckTable(WlanWirelessFrame *frame, const WlanIePointers *ies, int unparsed,
		     const void *ssid, unsigned int length);				/* 0x4bd26 */
int fmCoexValidateTable(WlanWirelessFrame *frame, const WlanIePointers *ies, const uint8_t *bssRates,
			uint8_t *rates, uint8_t *count, uint8_t *extended);		/* 0x4bd50 */
int fmCoexPrepareTable(const WlanWirelessFrame *request);				/* 0x4bdb2 */
int fmCoexDispatchTable(const WlanWirelessFrame *request);				/* 0x4bdc2 */
void fmCoexParseTable(const WlanIePointers *ies, const uint8_t *htCapability);		/* 0x4bdd2 */
int fmCoexBuildTable(const WlanIePointers *ies);					/* 0x4be00 */
const uint8_t *fmCoexComputeTable(WlanWirelessFrame *frame, const WlanWirelessFrame *request,
				  const uint8_t **elements, int *length);		/* 0x4be30 */
int fmCoexQueryTable(const WlanIePointers *ies);					/* 0x4be7a */
int fmCoexResetTable(const WlanIePointers *ies);					/* 0x4be8a */
int fmCoexSyncTable(uint8_t *destination, const uint8_t *htCapability);		/* 0x4be9a */
int fmCoexApplyTable(uint8_t *destination, const uint8_t *htOperation);		/* 0x4bea6 */
int fmCoexResolveTable(uint8_t *destination, const uint8_t *wmmElement);		/* 0x4beb2 */

/* wlan/aptemplate.c: the beacon and probe response templates. */
struct WlanMacTxRequest;
uint8_t *fmCoexSelectEntry(OsaBuffer *buffer, const uint8_t *source, const uint8_t *bssid,
			   unsigned int subtype, int broadcastSsid, const uint8_t *ssid);	/* 0x4bb3c */
uint8_t *fmCoexAdjustEntry(uint8_t *cursor, const uint8_t *rates, int dsParameter, uint8_t channel); /* 0x4bb84 */
uint8_t *fmCoexRecordEntry(uint8_t *tim, unsigned int stations, uint8_t dtimPeriod, uint8_t *copy); /* 0x4bbaa */
void fmCoexFilterEntry(struct WlanMacTxRequest *request, unsigned int subtype,
		       WlanWirelessFrame *frame, uint8_t *tim);				/* 0x4bbec */
void fmCoexHandleTable(struct WlanMacTxRequest *request, int nonErp, int protection, int barker); /* 0x4bc24 */
void fmCoexProcessTable(struct WlanMacTxRequest *request, const uint8_t *htOperation);	/* 0x4bc5e */
/* The BSS machine (wlan/apbss.c). */
struct WlanApMachine;
struct WlanApEvent;
struct RomHsmState;
int macMgmtHandleFrame(struct WlanConnection *connection);				/* 0x3a5d6 */
int macMgmtProcessFrame(struct WlanConnection *connection);				/* 0x3a636 */
int macMgmtUpdateFrame(int status, OsaBuffer *buffer);		/* 0x3a6a4 */
void macMgmtConfigureFrame(struct WlanConnection *connection);				/* 0x3a712 */
void macMgmtCheckFrame(struct WlanConnection *connection);				/* 0x3a778 */
int macMgmtValidateFrame(struct WlanConnection *connection);				/* 0x3a796 */
void macMgmtPrepareFrame(struct WlanConnection *connection);				/* 0x3a876 */
const int32_t *macMgmtDispatchFrame(struct WlanApMachine *machine, const int32_t *event); /* 0x3a9a8 */
const int32_t *macMgmtParseFrame(struct WlanApMachine *machine,
				 const struct WlanApEvent *event);			/* 0x3a9bc */
const int32_t *macMgmtBuildFrame(struct WlanApMachine *machine,
				 const struct WlanApEvent *event);			/* 0x3aa8c */
const int32_t *macMgmtComputeFrame(struct WlanApMachine *machine,
				   const struct WlanApEvent *event);			/* 0x3ab68 */
const int32_t *macMgmtQueryFrame(struct WlanApMachine *machine, const int32_t *event); /* 0x3abce */
void macMgmtResetFrame(void);								/* 0x3abf6 */
void macMgmtSyncFrame(struct RomHsmState *root, struct RomHsmState *parent,
		      const int32_t *(*handler)(void *machine, const int32_t *event)); /* 0x3abfc */
void macMgmtApplyFrame(struct WlanConnectionInfo *info);				/* 0x3ac34 */
void macMgmtResolveFrame(struct WlanConnectionInfo *info);				/* 0x3ac50 */
void macMgmtValidateTable(int reason);							/* 0x3726c */
void macMgmtProcessContext(struct WlanConnection *connection, uint8_t *channel,
			   unsigned int number);					/* 0x355b0 */
int uapSqramUpdateState(int value);							/* SQRAM 0xc000738e */
int uapSqramConfigureState(int value);							/* SQRAM 0xc0007398 */
void romLibNotifyContext(void);								/* ROM veneer 0x532e4 */
void macMgmtScheduleFrame(struct WlanConnection *connection, unsigned int subtype);	/* 0x3ac6c */
void macMgmtNotifyFrame(struct WlanConnection *connection, struct WlanMacTxRequest *request); /* 0x3af22 */
void macMgmtFormatFrame(struct WlanConnection *connection);				/* 0x3af46 */
void macMgmtInspectFrame(struct WlanConnection *connection);				/* 0x3af64 */
void macMgmtTrackFrame(struct WlanMacTxRequest *request, uint8_t *htOperation);		/* 0x3af74 */
void macMgmtManageFrame(struct WlanConnection *connection, struct WlanMacTxRequest *request); /* 0x3aff8 */
void macMgmtAcquireFrame(struct WlanConnection *connection);				/* 0x3b016 */
void macMgmtReleaseFrame(struct WlanConnection *connection);				/* 0x3b034 */
void macMgmtSetupFrame(uint16_t capability, struct WlanMacTxRequest *beacon,
		       struct WlanMacTxRequest *probe);					/* 0x3b044 */
void macMgmtClearFrame(struct WlanConnection *connection);				/* 0x3b068 */
void macMgmtConvertFrame(struct WlanConnection *connection);				/* 0x3b084 */
void macMgmtVerifyFrame(const uint8_t *ie, struct WlanMacTxRequest *beacon,
			struct WlanMacTxRequest *probe);				/* 0x3b094 */
void macMgmtSelectFrame(struct WlanConnection *connection);				/* 0x3b0d0 */
void fmCoexScheduleTable(uint8_t *frame, uint16_t algorithm, uint16_t sequence,
			 uint16_t status, const void *challenge);			/* 0x4bebe */
void fmCoexNotifyTable(uint8_t *frame, uint16_t capability, uint16_t listenInterval,
		       const uint8_t *currentAp);					/* 0x4bf66 */
int fmCoexFormatTable(const WlanBeaconIePointers *ies, uint8_t *htCapability, uint8_t *htOperation,
		      uint8_t *coexistence, uint8_t *overlapScan);			/* 0x4bfa8 */
unsigned int fmCoexInspectTable(const WlanBeaconIePointers *ies, uint8_t *rates, uint8_t *count,
				const WlanHtCapabilityIe *htCapability);		/* 0x4bff2 */
void fmCoexTrackTable(WlanWirelessFrame *frame, const void *ssid, const void *rates,
		      const void *dsChannel);						/* 0x4c05a */
void fmCoexManageTable(uint8_t *table);							/* 0x4c0cc */

/* wlan/apassoc.c */
int macMgmtHandlePacket(OsaBuffer *buffer);				/* 0x3902c */
int macMgmtUpdatePacket(struct WlanConnection *connection, const uint8_t *wps, const void *rsn,
			const void *wpa, const uint8_t *wapi, uint16_t *status);	/* 0x390a2 */
int macMgmtConfigurePacket(struct WlanConnection *connection, const WlanWirelessFrame *request,
			   unsigned int status);						/* 0x3916c */
int macMgmtSchedulePacket(void);							/* 0x39cb2 */
int macMgmtCheckPacket(OsaBuffer *buffer);						/* 0x39898 */
void macMgmtPreparePacket(struct WlanConnection *connection);				/* 0x398c4 */
void macMgmtDispatchPacket(OsaBuffer *buffer);						/* 0x39954 */
void macMgmtParsePacket(struct WlanConnection *connection, int value, int code);	/* 0x399a8 */
void macMgmtBuildPacket(unsigned int interfaceType, unsigned int bssNumber);		/* 0x399b4 */
void macMgmtComputePacket(struct WlanConnectionInfo *info, uint8_t *element,
			  const uint8_t *htCapability, const struct WlanBandChannel *band,
			  unsigned int beaconPeriod);					/* 0x399bc */
void macMgmtQueryPacket(struct WlanConnection *connection);				/* 0x39a06 */
int macMgmtResetPacket(struct WlanConnection *connection, int flushAll);		/* 0x39b06 */
int macMgmtSyncPacket(struct WlanConnection *connection);				/* 0x39b98 */
void macMgmtApplyPacket(struct WlanConnection *connection);			/* 0x39bca */
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
struct WlanConnection *p2pDirectProcessEntry(unsigned int type);			/* 0x41250 */
int wlanP2pConnection(struct WlanConnection *connection);				/* 0x41228 */
struct WlanPsPeerState *wlanConnectionPsState(struct WlanConnection *connection);	/* 0x1d1f6 */

#endif /* ROBIN_APSTA_H */
