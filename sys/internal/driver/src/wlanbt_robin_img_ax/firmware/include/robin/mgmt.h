/*
 * Management side of a connection (0xfdfe..0x10d1c): state machine posting,
 * the radio channel and transmit power limits, management frames built by
 * the firmware, events reported to the host and the host's custom IEs.
 * See wlan/mgmt.c.
 */
#ifndef ROBIN_MGMT_H
#define ROBIN_MGMT_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/wlan.h"

struct WlanConnection;
struct WlanReplyFrame;
struct WlanSmMessage;

extern int32_t wlanTxPowerHostLimit;
extern int32_t wlanTxPowerLinkLimit;
extern uint32_t wlanChannelSwitchHolds;
extern uint16_t wlanCustomIeMask;

int wlanSmPost(struct WlanConnection *connection, const struct WlanSmMessage *message);
void wlanSmPostCommand(OsaBuffer *buffer);
void wlanSetSlotTime(int shortSlot);
int wlanCalibrationStart(struct WlanConnection *connection, int value);
void wlanCalibrationEnd(void);
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
void wlanCustomIeCommand(struct WlanConnection *connection, int action, uint8_t *tlv,
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

#endif /* ROBIN_MGMT_H */
