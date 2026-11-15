/*
 * Management side of a connection (0xfdfe..0x10d1c): state machine posting,
 * the radio channel and transmit power limits, management frames built by
 * the firmware, events reported to the host and the host's custom IEs.
 * See wlan/mgmt.c.
 */
#ifndef ROBIN_MGMT_H
#define ROBIN_MGMT_H

#include <stdint.h>
#include "robin/adhoc.h"
#include "robin/buffer.h"
#include "robin/wlan.h"

struct WlanConnection;
struct WlanReplyFrame;

/* A message posted to a connection's state machine (20 bytes). */
struct WlanSmMessage {
	uint32_t event;
	uint32_t reserved4[2];
	uint8_t value[8];
};

extern int32_t wlanTxPowerHostLimit;
extern int32_t wlanTxPowerLinkLimit;
extern uint32_t wlanChannelSwitchHolds;
extern uint16_t wlanCustomIeMask;

int wlanSmPost(struct WlanConnection *connection, const struct WlanSmMessage *message);
void wlanSmPostCommand(OsaBuffer *buffer);
void wlanSetSlotTime(int shortSlot);
int wlanIbssTimingStart(struct WlanConnection *connection, const void *parameters);
void wlanIbssTimingEnd(void);

/* wlan/mgmtutil.c */
void wlanMacAddressSet(const uint8_t *address);			/* 0x2a214 */
void wlanMacAddressApply(void);					/* 0x2a254 */
void wlanIbssAddressGenerate(uint8_t *address, const uint8_t *source);	/* 0x2a25c */
int wlanIbssRecordStart(const WlanAdHocStart *start);		/* 0x2a2e6 */
int wlanIbssRecordJoin(const WlanAdHocBssDesc *join);		/* 0x2a358 */
void wlanIbssTimersControl(int start);				/* 0x2a3ca */
int wlanChannelWidthSelect(const uint8_t *config, uint8_t *width);	/* 0x2a3ec */
int wlanEventMessageBuild(struct WlanSmMessage *message, int code, const void *data); /* 0x2a49a */
uint64_t wlanSignalTimeNow(void);				/* 0x2a4fc */
int wlanMacEntryEnable(unsigned int index);			/* 0x2a504 */
unsigned int wlanTxPowerMinimum(unsigned int channel, unsigned int band);
int wlanTxPowerAllowed(unsigned int channel, unsigned int band, unsigned int rate);
uint8_t wlanCurrentChannel(void);
unsigned int wlanCurrentBand(void);
uint8_t wlanCurrentChannelFlags(uint8_t *flags);
void wlanTxPowerApply(void);
void wlanTxPowerSetHostLimit(int32_t limit, int apply);
void wlanTxPowerSetLinkLimit(int32_t limit, int apply);
uint8_t wlanTxPowerGetLinkLimit(void);
void wlanTxPowerSetConnectionLimit(int32_t *slot, int32_t limit, int apply);
void wlanTxPowerUpdateConnection(struct WlanConnection *connection, unsigned int channel,
				 int apply);
int wlanChannelFlagsDecode(const uint8_t *flags, uint8_t *band, uint8_t *bandwidth);
int wlanSetChannel(uint32_t channel, const uint8_t *flags, int wait);
int wlanSetChannelFlags(const uint8_t *flags);
void wlanMacReleaseHold(void);
uint8_t wlanOperatingChannel(void);
struct WlanReplyFrame *wlanMgmtBuildFrame(OsaBuffer *buffer, const uint8_t *source,
					  const uint8_t *destination, unsigned int subtype,
					  const uint8_t *bssid);
int wlanMgmtSendFrame(OsaBuffer *buffer);
OsaBuffer *wlanMgmtAllocFrame(struct WlanConnection *connection, uint32_t size);
int wlanMgmtSendLeave(struct WlanConnection *connection, const uint8_t *destination,
		      const uint8_t *bssid, uint16_t reason, uint8_t kind, int disassociate);
int wlanMgmtSendDeauth(struct WlanConnection *connection, const uint8_t *destination,
		       const uint8_t *bssid, uint16_t reason, uint8_t kind);
int wlanMgmtSendDisassoc(struct WlanConnection *connection, const uint8_t *destination,
			 const uint8_t *bssid, uint16_t reason, uint8_t kind);
int wlanReportEvent(struct WlanConnection *connection, const void *data, int code);
void wlanCustomIeUpdateMask(void);
int wlanCustomIeConfigure(struct WlanConnection *connection, const uint8_t *list,
			  int16_t remaining);
uint16_t wlanCustomIeGet(struct WlanConnection *connection, uint8_t *out, int single);
void wlanCustomIeRemoveBss(unsigned int interfaceType, unsigned int bssNumber);
int wlanCustomIeReset(void);
void wlanCustomIeApply(struct WlanConnection *connection, int add, WlanWirelessFrame *frame,
		       uint8_t offset);
void wlanCustomIeSet(struct WlanConnection *connection, const uint8_t *list, int16_t length);
int wlanCustomIeCommand(struct WlanConnection *connection, int action, uint8_t *tlv,
			uint16_t *size);
unsigned int wlanFrameClass(unsigned int interfaceType, const WlanWirelessFrame *frame);
const uint8_t *wlanFrameBssid(const WlanWirelessFrame *frame);
int wlanConnectionAuthenticated(struct WlanConnection *connection);
int wlanConnectionBssStarted(struct WlanConnection *connection);
int wlanRxCheckFrameClass(struct WlanConnection *connection, const WlanWirelessFrame *frame);
void wlanPsRefreshBss(struct WlanConnection *connection);
void wlanSecurityRefresh(struct WlanConnection *connection);
struct WlanConnectionInfo;
void wlanApOlbcReset(struct WlanConnectionInfo *info, unsigned int beaconInterval);
void wlanApErpUpdate(struct WlanConnection *connection);
void wlanApOlbcClear(struct WlanConnection *connection);
void wlanApUpdateProtection(struct WlanConnection *connection);
void wlanApNonErpGone(struct WlanConnection *connection);
void wlanConnectionUpdateListenInterval(struct WlanConnection *connection);
void wlanApUpdateSlotTime(struct WlanConnection *connection);
void wlanApLongSlotGone(struct WlanConnection *connection);
void wlanApInstallHooks(void);
unsigned int wlanStartFrequencyBand(unsigned int startFrequency, unsigned int channel);
unsigned int wlanStartFrequency(unsigned int band, unsigned int channel);
void wlanChannelDescriptor(uint8_t *descriptor, unsigned int band, unsigned int channel,
			   unsigned int width);
void wlanTxSetSharedRadio(int which);
uint32_t wlanBssBit(unsigned int interfaceType, unsigned int bssNumber);

/* Not converted yet; provisional names. */
void macMgmtPrepareContext(uint8_t *band, uint32_t *extra);			/* 0x3566a */
int cbProcPrepareTable(int table, int value);					/* 0x1cdc8 */
void cbProcDispatchTable(int table, int value);					/* 0x1ce46 */
int macMgmtUpdateContext(uint8_t channelFlags, unsigned int channel);		/* 0x355d2 */
void macMgmtCheckContext(void);							/* 0x3564a */
void macMgmtConfigureContext(const uint8_t *htOperation);				/* 0x3560c */
void macMgmtValidateContext(void);						/* 0x3565e */
int wlanPsVerifyFrame(int reason);						/* 0x3dac6 */
void macMgmtQueryContext(struct WlanConnection *connection, const uint8_t *rates,
			 unsigned int count, unsigned int bitmap);		/* 0x356f6 */
void cbProcSelectBuffer(int kind, const uint8_t *bssid);			/* 0x1e064 */
extern uint8_t wlanPsHostMode;							/* 0x55130 */

#endif /* ROBIN_MGMT_H */
